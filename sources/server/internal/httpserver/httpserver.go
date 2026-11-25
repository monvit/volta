package httpserver

import (
	"encoding/json"
	"errors"
	"io"
	"math"
	"net/http"
	"strconv"
	"time"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"

	"github.com/monvit/volta/server/internal/analysis"
	commandrouter "github.com/monvit/volta/server/internal/commandRouter"
	dbstore "github.com/monvit/volta/server/internal/db"
	"github.com/monvit/volta/server/internal/hub"
	"github.com/monvit/volta/server/internal/logger"
	"github.com/monvit/volta/server/internal/registry"
	pbt "github.com/monvit/volta/server/pb/types"
)

// ── HTTP Server ───────────────────────────────────────────────────────────────

type HTTPServer struct {
	registry       *registry.AgentRegistry
	router         *commandrouter.CommandRouter
	hub            *hub.WSHub
	store          *dbstore.Store
	analyzer       *analysis.Client
	allowedOrigins []string
	mux            *chi.Mux
}

func NewHTTPServer(
	registry *registry.AgentRegistry,
	router *commandrouter.CommandRouter,
	hub *hub.WSHub,
	store *dbstore.Store,
	analyzer *analysis.Client,
	allowedOrigins []string,
) *HTTPServer {
	s := &HTTPServer{
		registry:       registry,
		router:         router,
		hub:            hub,
		store:          store,
		analyzer:       analyzer,
		allowedOrigins: allowedOrigins,
	}
	s.mux = s.routes()
	return s
}

func (s *HTTPServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	s.mux.ServeHTTP(w, r)
}

func (s *HTTPServer) routes() *chi.Mux {
	mux := chi.NewRouter()

	mux.Use(middleware.RequestID)
	mux.Use(middleware.RealIP)
	mux.Use(middleware.Recoverer)
	mux.Use(corsMiddleware(s.allowedOrigins))
	mux.Use(middleware.Timeout(30 * time.Second))

	mux.Route("/api", func(r chi.Router) {
		r.Get("/agents", s.HandleListAgents)
		r.Get("/agents/{id}/metrics", s.HandleAgentMetrics)
		r.Post("/agents/{id}/analyze", s.HandleAnalyze)
		r.Post("/agents/{id}/stream", s.HandleStreamData)
		r.Post("/agents/{id}/send", s.HandleSendData)
	})

	mux.Get("/ws", s.hub.HandleWS)

	return mux
}

func corsMiddleware(allowedOrigins []string) func(http.Handler) http.Handler {
	allowed := make(map[string]struct{}, len(allowedOrigins))
	for _, origin := range allowedOrigins {
		allowed[origin] = struct{}{}
	}

	isAllowed := func(origin string) bool {
		if origin == "" {
			return true
		}
		_, ok := allowed[origin]
		return ok
	}

	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			origin := r.Header.Get("Origin")
			if origin == "" {
				next.ServeHTTP(w, r)
				return
			}

			if !isAllowed(origin) {
				if r.Method == http.MethodOptions {
					http.Error(w, "origin not allowed", http.StatusForbidden)
					return
				}
				next.ServeHTTP(w, r)
				return
			}

			w.Header().Set("Access-Control-Allow-Origin", origin)
			w.Header().Set("Vary", "Origin")
			w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
			w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Accept")

			if r.Method == http.MethodOptions {
				w.WriteHeader(http.StatusNoContent)
				return
			}

			next.ServeHTTP(w, r)
		})
	}
}

// ── Handlers ──────────────────────────────────────────────────────────────────

// GET /api/agents
func (s *HTTPServer) HandleListAgents(w http.ResponseWriter, r *http.Request) {
	sessions := s.registry.List()

	agents := make([]*pbt.Agent, 0, len(sessions))
	for _, sess := range sessions {
		agents = append(agents, &pbt.Agent{
			Id:     sess.ID,
			Status: pbt.AgentStatus_AGENT_STATUS_CONNECTED, // TODO: track status
			// ConnectedAt: sess.Info.ConnectedAt,
		})
	}

	writeJSON(w, http.StatusOK, &pbt.ListAgentsResponse{Agents: agents})
}

// GET /api/agents/{id}/metrics
func (s *HTTPServer) HandleAgentMetrics(w http.ResponseWriter, r *http.Request) {
	if s.store == nil {
		http.Error(w, "database is disabled", http.StatusServiceUnavailable)
		return
	}

	agentID := chi.URLParam(r, "id")

	from, to, err := parseTimeRange(r)
	if err != nil {
		http.Error(w, "invalid from/to timestamps", http.StatusBadRequest)
		return
	}

	var metricType *int16
	if raw := r.URL.Query().Get("metric_type"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 || v > math.MaxInt16 {
			http.Error(w, "invalid metric_type", http.StatusBadRequest)
			return
		}
		mt := int16(v)
		metricType = &mt
	}

	limit := 1000
	if raw := r.URL.Query().Get("limit"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v <= 0 {
			http.Error(w, "invalid limit", http.StatusBadRequest)
			return
		}
		limit = v
	}

	points, err := s.store.Query(r.Context(), dbstore.QueryOptions{
		AgentID:    agentID,
		MetricType: metricType,
		From:       from,
		To:         to,
		Limit:      limit,
	})
	if err != nil {
		logger.Error("db query failed: %v", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{"points": points})
}

type analyzeRequest struct {
	From          string      `json:"from"`
	To            string      `json:"to"`
	MetricTypes   []int16     `json:"metric_types"`
	BucketSeconds int         `json:"bucket_seconds"`
	Sigma         float64     `json:"sigma"`
	MinPoints     int         `json:"min_points"`
	Pairs         [][2]string `json:"pairs"`
}

// POST /api/agents/{id}/analyze
func (s *HTTPServer) HandleAnalyze(w http.ResponseWriter, r *http.Request) {
	if s.store == nil {
		http.Error(w, "database is disabled", http.StatusServiceUnavailable)
		return
	}
	if !s.analyzer.Enabled() {
		http.Error(w, "analysis is disabled", http.StatusServiceUnavailable)
		return
	}

	defer r.Body.Close()
	payload := analyzeRequest{BucketSeconds: 60, Sigma: 3, MinPoints: 8}
	if err := json.NewDecoder(io.LimitReader(r.Body, 1<<20)).Decode(&payload); err != nil {
		if !errors.Is(err, io.EOF) {
			http.Error(w, "invalid request body", http.StatusBadRequest)
			return
		}
	}

	fromValues := r.URL.Query()["from"]
	if payload.From != "" {
		fromValues = []string{payload.From}
	}
	toValues := r.URL.Query()["to"]
	if payload.To != "" {
		toValues = []string{payload.To}
	}
	from, to, err := parseTimeRangeValues(fromValues, toValues)
	if err != nil {
		http.Error(w, "invalid from/to timestamps", http.StatusBadRequest)
		return
	}
	if !from.Before(to) {
		http.Error(w, "from must be before to", http.StatusBadRequest)
		return
	}
	if payload.BucketSeconds < 1 || payload.Sigma <= 0 || payload.MinPoints < 2 {
		http.Error(w, "invalid analysis options", http.StatusBadRequest)
		return
	}
	if len(payload.MetricTypes) > 64 || len(payload.Pairs) > 32 {
		http.Error(w, "too many metric types or pairs", http.StatusBadRequest)
		return
	}

	allowed := make(map[int16]struct{}, len(payload.MetricTypes))
	for _, metricType := range payload.MetricTypes {
		name, ok := pbt.MetricType_name[int32(metricType)]
		if !ok || name == "METRIC_TYPE_UNSPECIFIED" {
			http.Error(w, "invalid metric_type", http.StatusBadRequest)
			return
		}
		allowed[metricType] = struct{}{}
	}
	for _, pair := range payload.Pairs {
		if pair[0] == "" || pair[1] == "" {
			http.Error(w, "invalid pair", http.StatusBadRequest)
			return
		}
	}

	points, err := s.store.Query(r.Context(), dbstore.QueryOptions{
		AgentID: chi.URLParam(r, "id"),
		From:    from,
		To:      to,
		Limit:   s.analyzer.MaxRows(),
	})
	if err != nil {
		logger.Error("db query failed: %v", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}

	rows := make([]analysis.Point, 0, len(points))
	for _, point := range points {
		if len(allowed) > 0 {
			if _, ok := allowed[point.MetricType]; !ok {
				continue
			}
		}
		name, ok := pbt.MetricType_name[int32(point.MetricType)]
		if !ok || name == "METRIC_TYPE_UNSPECIFIED" {
			continue
		}
		deviceID := "system"
		if point.DeviceID != nil && *point.DeviceID != "" {
			deviceID = *point.DeviceID
		}
		rows = append(rows, analysis.Point{
			MetricType:  name,
			DeviceID:    deviceID,
			TimestampNS: point.TS.UnixNano(),
			Value:       point.Value,
		})
	}

	result, err := s.analyzer.Analyze(r.Context(), analysis.Request{
		Source: "database",
		Points: rows,
		Options: analysis.Options{
			BucketSeconds: payload.BucketSeconds,
			Sigma:         payload.Sigma,
			MinPoints:     payload.MinPoints,
			Pairs:         payload.Pairs,
		},
	})
	switch {
	case errors.Is(err, analysis.ErrInvalidRequest):
		http.Error(w, err.Error(), http.StatusBadRequest)
	case errors.Is(err, analysis.ErrDisabled):
		http.Error(w, "analysis is disabled", http.StatusServiceUnavailable)
	case errors.Is(err, analysis.ErrUnavailable):
		logger.Error("analysis service unavailable: %v", err)
		http.Error(w, "analysis service unavailable", http.StatusBadGateway)
	case err != nil:
		logger.Error("analysis failed: %v", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
	default:
		writeJSON(w, http.StatusOK, json.RawMessage(result))
	}
}

// POST /api/agents/{id}/stream
func (s *HTTPServer) HandleStreamData(w http.ResponseWriter, r *http.Request) {
	agentID := chi.URLParam(r, "id")

	if err := s.router.StreamData(agentID); err != nil {
		writeError(w, err)
		return
	}

	w.WriteHeader(http.StatusAccepted)
}

// POST /api/agents/{id}/send
func (s *HTTPServer) HandleSendData(w http.ResponseWriter, r *http.Request) {
	agentID := chi.URLParam(r, "id")

	var req pbt.SendDataRequest
	if err := protojson.Unmarshal(readBody(r), &req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	if err := s.router.SendData(agentID); err != nil {
		writeError(w, err)
		return
	}

	w.WriteHeader(http.StatusAccepted)
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)

	if m, ok := v.(proto.Message); ok {
		data, err := protojson.Marshal(m)
		if err != nil {
			logger.Error("protojson marshal failed: %v", err)
			return
		}
		w.Write(data)
		return
	}

	if err := json.NewEncoder(w).Encode(v); err != nil {
		logger.Error("json encode failed: %v", err)
	}
}

var (
	ErrAgentNotFound     = errors.New("agent not found")
	ErrAgentDisconnected = errors.New("agent disconnected")
)

func writeError(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, ErrAgentNotFound):
		http.Error(w, err.Error(), http.StatusNotFound)
	case errors.Is(err, ErrAgentDisconnected):
		http.Error(w, err.Error(), http.StatusServiceUnavailable)
	default:
		logger.Error("internal error: %v", err)
		http.Error(w, "internal server error", http.StatusInternalServerError)
	}
}

func readBody(r *http.Request) []byte {
	defer r.Body.Close()
	data, _ := io.ReadAll(r.Body)
	return data
}

func parseTimeRange(r *http.Request) (time.Time, time.Time, error) {
	return parseTimeRangeValues(r.URL.Query()["from"], r.URL.Query()["to"])
}

func parseTimeRangeValues(fromValues, toValues []string) (time.Time, time.Time, error) {
	to := time.Now()
	from := to.Add(-time.Hour)

	for _, v := range fromValues {
		t, err := time.Parse(time.RFC3339Nano, v)
		if err != nil {
			return time.Time{}, time.Time{}, err
		}
		from = t
	}

	for _, v := range toValues {
		t, err := time.Parse(time.RFC3339Nano, v)
		if err != nil {
			return time.Time{}, time.Time{}, err
		}
		to = t
	}

	return from.UTC(), to.UTC(), nil
}
