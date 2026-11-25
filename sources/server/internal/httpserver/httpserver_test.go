package httpserver

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/monvit/volta/server/internal/analysis"
	eventbus "github.com/monvit/volta/server/internal/eventBus"
	"github.com/monvit/volta/server/internal/hub"
)

func TestWriteJSONPassesRawMessageThrough(t *testing.T) {
	response := httptest.NewRecorder()

	writeJSON(response, http.StatusOK, json.RawMessage(`{"source":"database","series":[]}`))

	if got := response.Body.String(); got != "{\"source\":\"database\",\"series\":[]}\n" {
		t.Fatalf("unexpected body: %q", got)
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
