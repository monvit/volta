package analysis

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

func TestClientAnalyzePassesRequestThrough(t *testing.T) {
	var received Request
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/analyze" {
			t.Errorf("unexpected path: %s", r.URL.Path)
		}
		if err := json.NewDecoder(r.Body).Decode(&received); err != nil {
			t.Fatalf("decode request: %v", err)
		}
		w.Header().Set("Content-Type", "application/json")
		_, _ = w.Write([]byte(`{"source":"database","series":[]}`))
	}))
	defer server.Close()

	client := New(true, server.URL, time.Second, 100)
	result, err := client.Analyze(context.Background(), Request{
		Source: "database",
		Points: []Point{{MetricType: "METRIC_TYPE_CPU_POWER", DeviceID: "system", TimestampNS: 1, Value: 2}},
	})

	if err != nil {
		t.Fatalf("analyze: %v", err)
	}
	if received.Source != "database" || len(received.Points) != 1 {
		t.Fatalf("unexpected forwarded request: %+v", received)
	}
	if string(result) != `{"source":"database","series":[]}` {
		t.Fatalf("unexpected result: %s", result)
	}
}

func TestClientAnalyzeMapsErrors(t *testing.T) {
	tests := []struct {
		name   string
		status int
		want   error
	}{
		{name: "bad request", status: http.StatusBadRequest, want: ErrInvalidRequest},
		{name: "unavailable", status: http.StatusBadGateway, want: ErrUnavailable},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
				http.Error(w, "failure", tt.status)
			}))
			defer server.Close()

			client := New(true, server.URL, time.Second, 100)
			_, err := client.Analyze(context.Background(), Request{Source: "database"})
			if !errors.Is(err, tt.want) {
				t.Fatalf("got %v, want %v", err, tt.want)
			}
		})
	}
}

func TestClientAnalyzeDisabled(t *testing.T) {
	client := New(false, "http://localhost:8000", time.Second, 100)

	if _, err := client.Analyze(context.Background(), Request{}); !errors.Is(err, ErrDisabled) {
		t.Fatalf("got %v, want %v", err, ErrDisabled)
	}
}
