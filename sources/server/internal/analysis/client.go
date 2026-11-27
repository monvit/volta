package analysis

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"mime"
	"net/http"
	"strings"
	"time"
)

const maxResponseBytes = 32 << 20

var (
	ErrDisabled       = errors.New("analysis is disabled")
	ErrInvalidRequest = errors.New("analysis request rejected")
	ErrUnavailable    = errors.New("analysis service unavailable")
)

type Point struct {
	MetricType  string  `json:"metric_type"`
	DeviceID    string  `json:"device_id"`
	TimestampNS int64   `json:"timestamp_ns"`
	Value       float64 `json:"value"`
}

type Options struct {
	BucketSeconds int         `json:"bucket_seconds"`
	Sigma         float64     `json:"sigma"`
	MinPoints     int         `json:"min_points"`
	SMAPeriod     int         `json:"sma_period"`
	EMAPeriod     int         `json:"ema_period"`
	Pairs         [][2]string `json:"pairs"`
}

type Request struct {
	Source  string  `json:"source"`
	Points  []Point `json:"points"`
	Options Options `json:"options"`
}

type Client struct {
	enabled bool
	url     string
	client  *http.Client
	maxRows int
}

func New(enabled bool, url string, timeout time.Duration, maxRows int) *Client {
	return &Client{
		enabled: enabled,
		url:     strings.TrimRight(url, "/"),
		client:  &http.Client{Timeout: timeout},
		maxRows: maxRows,
	}
}

func (c *Client) Enabled() bool {
	return c != nil && c.enabled
}

func (c *Client) MaxRows() int {
	if c == nil || c.maxRows < 1 {
		return 100000
	}
	return c.maxRows
}

func (c *Client) Analyze(ctx context.Context, req Request) (json.RawMessage, error) {
	return c.post(ctx, "/analyze", "application/json", req)
}

func (c *Client) Report(ctx context.Context, req Request) ([]byte, error) {
	return c.post(ctx, "/report", "application/pdf", req)
}

const DefaultPeriod = 10

func (c *Client) post(ctx context.Context, path, accept string, req Request) ([]byte, error) {
	if !c.Enabled() {
		return nil, ErrDisabled
	}

	if req.Options.SMAPeriod == 0 {
		req.Options.SMAPeriod = DefaultPeriod
	}
	if req.Options.EMAPeriod == 0 {
		req.Options.EMAPeriod = DefaultPeriod
	}
	if req.Options.SMAPeriod < 1 || req.Options.EMAPeriod < 1 {
		return nil, ErrInvalidRequest
	}

	body, err := json.Marshal(req)
	if err != nil {
		return nil, fmt.Errorf("analysis: marshal request: %w", err)
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, c.url+path, bytes.NewReader(body))
	if err != nil {
		return nil, fmt.Errorf("analysis: create request: %w", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Accept", accept)

	resp, err := c.client.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("%w: %v", ErrUnavailable, err)
	}
	defer resp.Body.Close()

	data, err := io.ReadAll(io.LimitReader(resp.Body, maxResponseBytes+1))
	if err != nil {
		return nil, fmt.Errorf("%w: read response: %v", ErrUnavailable, err)
	}

	if len(data) > maxResponseBytes {
		return nil, fmt.Errorf("%w: response exceeds size limit", ErrUnavailable)
	}

	switch {
	case resp.StatusCode >= 200 && resp.StatusCode < 300:
		if accept == "application/pdf" {
			mediaType, _, err := mime.ParseMediaType(resp.Header.Get("Content-Type"))
			if err != nil || mediaType != accept || !bytes.HasPrefix(data, []byte("%PDF-")) || !bytes.HasSuffix(bytes.TrimRight(data, "\x00\t\n\f\r "), []byte("%%EOF")) {
				return nil, fmt.Errorf("%w: invalid PDF response", ErrUnavailable)
			}
		} else if !json.Valid(data) {
			return nil, fmt.Errorf("%w: invalid JSON response", ErrUnavailable)
		}
		return data, nil
	case resp.StatusCode >= 400 && resp.StatusCode < 500:
		return nil, fmt.Errorf("%w: %s", ErrInvalidRequest, strings.TrimSpace(string(data)))
	default:
		return nil, fmt.Errorf("%w: status %d: %s", ErrUnavailable, resp.StatusCode, strings.TrimSpace(string(data)))
	}
}
