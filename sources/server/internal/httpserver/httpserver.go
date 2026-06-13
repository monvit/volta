package httpserver

import (
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"time"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"

	commandrouter "github.com/monvit/volta/server/internal/commandRouter"
	"github.com/monvit/volta/server/internal/hub"
	"github.com/monvit/volta/server/internal/logger"
	"github.com/monvit/volta/server/internal/registry"
	pbt "github.com/monvit/volta/server/pb/types"
)

// ── HTTP Server ───────────────────────────────────────────────────────────────

type HTTPServer struct {
	registry *registry.AgentRegistry
	router   *commandrouter.CommandRouter
	hub      *hub.WSHub
	mux      *chi.Mux
}

func NewHTTPServer(registry *registry.AgentRegistry, router *commandrouter.CommandRouter, hub *hub.WSHub) *HTTPServer {
	s := &HTTPServer{
		registry: registry,
		router:   router,
		hub:      hub,
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
	mux.Use(corsMiddleware)
	mux.Use(middleware.Timeout(30 * time.Second))

	mux.Route("/api", func(r chi.Router) {
		r.Get("/agents", s.HandleListAgents)
		r.Post("/agents/{id}/stream", s.HandleStreamData)
		r.Post("/agents/{id}/send", s.HandleSendData)
	})

	mux.Get("/ws", s.hub.HandleWS)

	return mux
}

func corsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Accept")

		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}

		next.ServeHTTP(w, r)
	})
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
