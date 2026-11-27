package httpserver

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/monvit/volta/server/internal/analysis"
	dbstore "github.com/monvit/volta/server/internal/db"
	eventbus "github.com/monvit/volta/server/internal/eventBus"
	"github.com/monvit/volta/server/internal/hub"
	pbt "github.com/monvit/volta/server/pb/types"
)

func TestWriteJSONPassesRawMessageThrough(t *testing.T) {
	response := httptest.NewRecorder()

	writeJSON(response, http.StatusOK, json.RawMessage(`{"source":"database","series":[]}`))

	if got := response.Body.String(); got != "{\"source\":\"database\",\"series\":[]}\n" {
		t.Fatalf("unexpected body: %q", got)
	}
}

type testMetricStore struct {
	points []dbstore.Point
	query  dbstore.QueryOptions
}

func (s *testMetricStore) Query(_ context.Context, query dbstore.QueryOptions) ([]dbstore.Point, error) {
	s.query = query
	return s.points, nil
}

func TestAnalysisRoutesForwardOptionsAndMetrics(t *testing.T) {
	for _, route := range []string{"analyze", "report"} {
		t.Run(route, func(t *testing.T) {
			var received analysis.Request
			sidecar := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				if r.URL.Path != "/"+route {
					t.Errorf("unexpected sidecar path: %s", r.URL.Path)
				}
				if err := json.NewDecoder(r.Body).Decode(&received); err != nil {
					t.Fatalf("decode: %v", err)
				}
				if route == "report" {
					w.Header().Set("Content-Type", "application/pdf")
					_, _ = w.Write([]byte("%PDF-1.4\n%%EOF\n"))
				} else {
					_, _ = w.Write([]byte(`{"source":"database"}`))
				}
			}))
			defer sidecar.Close()

			store := &testMetricStore{points: []dbstore.Point{
				{TS: time.Unix(1, 0), MetricType: int16(pbt.MetricType_METRIC_TYPE_CPU_UTILIZATION), Value: 42},
				{TS: time.Unix(2, 0), MetricType: int16(pbt.MetricType_METRIC_TYPE_CPU_TEMPERATURE), Value: 55},
			}}
			server := NewHTTPServer(nil, nil, hub.New(eventbus.New(), nil), nil, analysis.New(true, sidecar.URL, time.Second, 100), nil)
			server.store = store
			request := httptest.NewRequest(http.MethodPost, "/api/agents/agent-1/"+route, strings.NewReader(`{"metric_types":[103],"sma_period":3,"ema_period":5}`))
			response := httptest.NewRecorder()
			server.ServeHTTP(response, request)

			if response.Code != http.StatusOK {
				t.Fatalf("status %d: %s", response.Code, response.Body)
			}
			if store.query.AgentID != "agent-1" || store.query.Limit != 100 {
				t.Errorf("unexpected query: %+v", store.query)
			}
			if received.Source != "database" || len(received.Points) != 1 || received.Points[0].MetricType != "METRIC_TYPE_CPU_UTILIZATION" || received.Points[0].DeviceID != "system" {
				t.Errorf("unexpected points: %+v", received)
			}
			if received.Options.SMAPeriod != 3 || received.Options.EMAPeriod != 5 {
				t.Errorf("unexpected options: %+v", received.Options)
			}
			if route == "report" && (response.Header().Get("Content-Type") != "application/pdf" || response.Header().Get("Cache-Control") != "no-store" || response.Body.String() != "%PDF-1.4\n%%EOF\n") {
				t.Errorf("unexpected report response: %v %q", response.Header(), response.Body.String())
			}
		})
	}
}

func TestAnalysisRoutesValidatePeriods(t *testing.T) {
	store := &testMetricStore{}
	server := NewHTTPServer(nil, nil, hub.New(eventbus.New(), nil), nil, analysis.New(true, "http://localhost:8000", time.Second, 100), nil)
	server.store = store
	for _, route := range []string{"analyze", "report"} {
		for _, body := range []string{`{"sma_period":0}`, `{"ema_period":-1}`, `{"sma_period":1.5}`} {
			response := httptest.NewRecorder()
			server.ServeHTTP(response, httptest.NewRequest(http.MethodPost, "/api/agents/1/"+route, strings.NewReader(body)))
			if response.Code != http.StatusBadRequest {
				t.Errorf("%s %s: got %d, want 400", route, body, response.Code)
			}
		}
	}
}

func TestHandleAnalyzeRequiresDatabaseAndAnalysis(t *testing.T) {
	analyzer := analysis.New(false, "http://localhost:8000", time.Second, 100)
	server := NewHTTPServer(nil, nil, hub.New(eventbus.New(), nil), nil, analyzer, nil)

	request := httptest.NewRequest(http.MethodPost, "/api/agents/123/analyze", nil)
	response := httptest.NewRecorder()
	server.ServeHTTP(response, request)

	if response.Code != http.StatusServiceUnavailable {
		t.Fatalf("got status %d, want 503", response.Code)
	}
}
