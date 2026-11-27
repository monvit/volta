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

func TestClientReportPassesOptions(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/report" || r.Header.Get("Accept") != "application/pdf" {
			t.Errorf("unexpected request: %s %s", r.URL.Path, r.Header.Get("Accept"))
		}
		var req Request
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			t.Fatalf("decode request: %v", err)
		}
		if req.Options.SMAPeriod != 5 || req.Options.EMAPeriod != 3 {
			t.Errorf("unexpected options: %+v", req.Options)
		}
		w.Header().Set("Content-Type", "application/pdf")
		_, _ = w.Write([]byte("%PDF-1.4\n%%EOF\n"))
	}))
	defer server.Close()

	client := New(true, server.URL, time.Second, 100)
	result, err := client.Report(context.Background(), Request{Options: Options{SMAPeriod: 5, EMAPeriod: 3}})
	if err != nil || string(result) != "%PDF-1.4\n%%EOF\n" {
		t.Fatalf("report: %q, %v", result, err)
	}
}

func TestClientReportRejectsInvalidResponses(t *testing.T) {
	for _, tc := range []struct {
		name, contentType, body string
	}{
		{"wrong type", "application/json", "%PDF-1.4\n%%EOF\n"},
		{"wrong header", "application/pdf", "not a PDF %%EOF"},
		{"truncated", "application/pdf", "%PDF-1.4\n"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
				w.Header().Set("Content-Type", tc.contentType)
				_, _ = w.Write([]byte(tc.body))
			}))
			defer server.Close()
			client := New(true, server.URL, time.Second, 100)
			if _, err := client.Report(context.Background(), Request{}); !errors.Is(err, ErrUnavailable) {
				t.Fatalf("got %v, want unavailable", err)
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
