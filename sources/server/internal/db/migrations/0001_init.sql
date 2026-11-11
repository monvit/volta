-- +goose Up
-- +goose NO TRANSACTION
CREATE EXTENSION IF NOT EXISTS timescaledb;

CREATE TABLE IF NOT EXISTS metrics (
    ts          TIMESTAMPTZ      NOT NULL,
    agent_id    UUID             NOT NULL,
    metric_type SMALLINT         NOT NULL,
    device_id   TEXT,
    value       DOUBLE PRECISION NOT NULL
);

SELECT create_hypertable('metrics', 'ts', chunk_time_interval => INTERVAL '1 day', if_not_exists => TRUE);

CREATE INDEX IF NOT EXISTS idx_metrics_agent_metric_time
    ON metrics (agent_id, metric_type, device_id, ts DESC);

SELECT add_retention_policy('metrics', INTERVAL '30 days', if_not_exists => TRUE);

-- +goose Down
-- +goose NO TRANSACTION
SELECT remove_retention_policy('metrics', if_exists => TRUE);
DROP TABLE IF EXISTS metrics;
