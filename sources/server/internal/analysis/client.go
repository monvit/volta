package analysis

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

const maxResponseBytes = 8 << 20

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
	if !c.Enabled() {
		return nil, ErrDisabled
	}

	body, err := json.Marshal(req)
	if err != nil {
		return nil, fmt.Errorf("analysis: marshal request: %w", err)
	}

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, c.url+"/analyze", bytes.NewReader(body))
	if err != nil {
		return nil, fmt.Errorf("analysis: create request: %w", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Accept", "application/json")

	resp, err := c.client.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("%w: %v", ErrUnavailable, err)
	}
	defer resp.Body.Close()

	data, err := io.ReadAll(io.LimitReader(resp.Body, maxResponseBytes))
	if err != nil {
		return nil, fmt.Errorf("%w: read response: %v", ErrUnavailable, err)
	}

	switch {
	case resp.StatusCode >= 200 && resp.StatusCode < 300:
		if !json.Valid(data) {
			return nil, fmt.Errorf("%w: invalid JSON response", ErrUnavailable)
		}
		return json.RawMessage(data), nil
	case resp.StatusCode >= 400 && resp.StatusCode < 500:
		return nil, fmt.Errorf("%w: %s", ErrInvalidRequest, strings.TrimSpace(string(data)))
	default:
		return nil, fmt.Errorf("%w: status %d: %s", ErrUnavailable, resp.StatusCode, strings.TrimSpace(string(data)))
	}
}
