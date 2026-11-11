package db

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/monvit/volta/server/internal/config"
	"github.com/monvit/volta/server/internal/logger"
	pbt "github.com/monvit/volta/server/pb/types"
)

const (
	defaultMaxRows       = 5000
	defaultFlushInterval = time.Second
	ingestQueueSize      = 1024
	maxQueryLimit        = 100000
)

type Point struct {
	TS         time.Time `json:"ts"`
	AgentID    string    `json:"agent_id"`
	MetricType int16     `json:"metric_type"`
	DeviceID   *string   `json:"device_id,omitempty"`
	Value      float64   `json:"value"`
}

func (p Point) vals() []any {
	return []any{p.TS, p.AgentID, p.MetricType, p.DeviceID, p.Value}
}

type Store struct {
	pool          *pgxpool.Pool
	retentionDays int

	writeCh chan []Point
	wg      sync.WaitGroup
	ctx     context.Context
	cancel  context.CancelFunc
	done    chan struct{}

	maxRows       int
	flushInterval time.Duration
}

func Open(ctx context.Context, cfg config.DBConfig) (*Store, error) {
	if cfg.DSN == "" {
		return nil, fmt.Errorf("db: dsn is empty")
	}

	poolCfg, err := pgxpool.ParseConfig(cfg.DSN)
	if err != nil {
		return nil, fmt.Errorf("db: parse dsn: %w", err)
	}

	maxConns := int32(cfg.MaxConns)
	if maxConns < 1 {
		maxConns = 8
	}
	minConns := int32(cfg.MaxIdle)
	if minConns < 0 || minConns > maxConns {
		minConns = 0
	}
	poolCfg.MaxConns = maxConns
	poolCfg.MinConns = minConns

	pool, err := pgxpool.NewWithConfig(ctx, poolCfg)
	if err != nil {
		return nil, fmt.Errorf("db: create pool: %w", err)
	}

	if err := pool.Ping(ctx); err != nil {
		pool.Close()
		return nil, fmt.Errorf("db: ping: %w", err)
	}

	storeCtx, cancel := context.WithCancel(context.Background())
	s := &Store{
		pool:          pool,
		retentionDays: cfg.Retention,
		writeCh:       make(chan []Point, ingestQueueSize),
		ctx:           storeCtx,
		cancel:        cancel,
		done:          make(chan struct{}),
		maxRows:       defaultMaxRows,
		flushInterval: defaultFlushInterval,
	}

	if err := s.applyRetention(storeCtx); err != nil {
		cancel()
		pool.Close()
		return nil, fmt.Errorf("db: apply retention policy: %w", err)
	}

	s.wg.Add(1)
	go s.runWriter()

	return s, nil
}

func (s *Store) Close() {
	select {
	case <-s.done:
		return
	default:
	}

	s.cancel()
	s.wg.Wait()
	s.pool.Close()
	close(s.done)
}

func (s *Store) Ingest(agentID string, batch *pbt.MetricBatch) {
	select {
	case <-s.done:
		return
	default:
	}

	points := pointsFromBatch(agentID, batch)
	if len(points) == 0 {
		return
	}

	select {
	case s.writeCh <- points:
	default:
		logger.Warn("db: ingest queue full, dropping batch from agent %s", agentID)
	}
}

func pointsFromBatch(agentID string, batch *pbt.MetricBatch) []Point {
	header := batch.GetHeader()
	if header == nil {
		return nil
	}

	sts := batch.TimestampsNs
	if len(sts) > len(batch.Values) {
		sts = sts[:len(batch.Values)]
	}
	if len(sts) == 0 {
		return nil
	}

	mt := int16(header.GetMetricType())
	deviceID := deviceIDString(header.GetDeviceId())

	points := make([]Point, 0, len(sts))
	for i, ns := range sts {
		points = append(points, Point{
			TS:         time.Unix(0, int64(ns)),
			AgentID:    agentID,
			MetricType: mt,
			DeviceID:   deviceID,
			Value:      batch.Values[i],
		})
	}

	return points
}

func deviceIDString(d *pbt.DeviceID) *string {
	if d == nil {
		return nil
	}

	var v string
	switch id := d.GetDevice().(type) {
	case *pbt.DeviceID_Gpu:
		g := id.Gpu
		v = fmt.Sprintf("gpu:%04x:%02x:%02x.%x", g.PciDomain, g.PciBus, g.PciDevice, g.PciFunction)
	case *pbt.DeviceID_Cpu:
		c := id.Cpu
		if c.CoreIndex > 0 {
			v = fmt.Sprintf("cpu:s%d-c%d", c.SocketIndex, c.CoreIndex)
		} else {
			v = fmt.Sprintf("cpu:s%d", c.SocketIndex)
		}
	case *pbt.DeviceID_Network:
		v = fmt.Sprintf("net:ifindex%d", id.Network.Ifindex)
	case *pbt.DeviceID_Disk:
		v = fmt.Sprintf("disk:%d:%d", id.Disk.Major, id.Disk.Minor)
	default:
		return nil
	}

	return &v
}

func (s *Store) runWriter() {
	defer s.wg.Done()

	ticker := time.NewTicker(s.flushInterval)
	defer ticker.Stop()

	var buf []Point
	flush := func() {
		if len(buf) == 0 {
			return
		}
		if err := copyPoints(s.ctx, s.pool, buf); err != nil {
			logger.Error("db: ingest failed, dropping %d rows: %v", len(buf), err)
		}
		buf = buf[:0]
	}

	for {
		select {
		case <-s.ctx.Done():
			flush()
			return
		case batch, ok := <-s.writeCh:
			if !ok {
				flush()
				return
			}
			buf = append(buf, batch...)
			if len(buf) >= s.maxRows {
				flush()
			}
		case <-ticker.C:
			flush()
		}
	}
}

func copyPoints(ctx context.Context, pool *pgxpool.Pool, points []Point) error {
	_, err := pool.CopyFrom(
		ctx,
		pgx.Identifier{"metrics"},
		[]string{"ts", "agent_id", "metric_type", "device_id", "value"},
		pgx.CopyFromSlice(len(points), func(i int) ([]any, error) {
			return points[i].vals(), nil
		}),
	)
	return err
}

func (s *Store) applyRetention(ctx context.Context) error {
	if s.retentionDays < 1 {
		s.retentionDays = 30
	}

	if _, err := s.pool.Exec(ctx, `SELECT remove_retention_policy('metrics', if_exists => TRUE)`); err != nil {
		return err
	}

	_, err := s.pool.Exec(
		ctx,
		`SELECT add_retention_policy('metrics', make_interval(days => $1), if_not_exists => TRUE)`,
		s.retentionDays,
	)
	return err
}

type QueryOptions struct {
	AgentID    string
	MetricType *int16
	From       time.Time
	To         time.Time
	Limit      int
}

func (s *Store) Query(ctx context.Context, o QueryOptions) ([]Point, error) {
	if o.Limit <= 0 || o.Limit > maxQueryLimit {
		o.Limit = maxQueryLimit
	}

	q := `SELECT ts, agent_id, metric_type, device_id, value
		FROM metrics
		WHERE agent_id = $1 AND ts >= $2 AND ts <= $3`
	args := []any{o.AgentID, o.From, o.To}

	if o.MetricType != nil {
		q += ` AND metric_type = $4`
		args = append(args, *o.MetricType)
	}

	q += fmt.Sprintf(` ORDER BY ts ASC LIMIT %d`, o.Limit)

	rows, err := s.pool.Query(ctx, q, args...)
	if err != nil {
		return nil, fmt.Errorf("db: query metrics: %w", err)
	}
	defer rows.Close()

	var out []Point
	for rows.Next() {
		var p Point
		if err := rows.Scan(&p.TS, &p.AgentID, &p.MetricType, &p.DeviceID, &p.Value); err != nil {
			return nil, fmt.Errorf("db: scan metric: %w", err)
		}
		out = append(out, p)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("db: iterate metrics: %w", err)
	}

	return out, nil
}
