# Server

Core maintainer: @kox13

The Server is the central aggregation point of Volta. It accepts gRPC connections from agents, fans incoming metric batches out to an in-memory event bus, and exposes that data to the dashboard over a REST API and WebSockets.

## Development Setup

Project uses Go modules for dependency management and Protocol Buffers / gRPC as the wire format for Agent ↔ Server communication.

### 1. Initial Requirements

- **Go** (1.25+)
- **protoc** (Protocol Buffers compiler)
- **protoc-gen-go** and **protoc-gen-go-grpc** (Go plugins for `protoc`)

### 2. Generate Protobuf Code

The server depends on generated code (`sources/server/pb`) produced from the contract defined in `sources/proto`. This package is **not committed** to the repository and must be generated before the project will build.

```bash
# Install the protoc Go plugins (once)
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest

# In sources/server
make proto
```

### 3. Build & Run

```bash
make build
make run
```

On startup the server creates a `logs/` directory (relative to the working directory) and writes a timestamped log file there in addition to stderr.

## Server Configuration

Configuration is layered, with each source overriding the previous one:

1. `/etc/volta/server.conf` — system-wide config (toml)
2. `./server.conf` — local config (TOML, relative to the working directory)
3. `.env` — environment file
4. Environment variables, mapped to dot notation (`GRPC_PORT` → `grpc.port`; only the first underscore is replaced, so `DB_RETENTION_DAYS` → `db.retention_days`)
5. CLI flags, in dot notation (`--grpc.port=5000`)
If no config file is found, the server falls back to built-in defaults.

### Supported keys

| Key | CLI flag | Default | Description |
|---|---|---|---|
| `grpc.addr` | `--grpc.addr` | `0.0.0.0` | gRPC listen address. |
| `grpc.port` | `--grpc.port` | `5000` | gRPC listen port. |
| `rest.addr` | `--rest.addr` | `0.0.0.0` | REST/WebSocket listen address. |
| `rest.port` | `--rest.port` | `8080` | REST/WebSocket listen port. |
| `rest.origins` | `--rest.origins` | `http://localhost:5173`, `http://127.0.0.1:5173`, `https://monvit.github.io` | Allowed CORS/WebSocket origins. |
| `log.level` | `--log.level` | `info` | `debug` / `info` / `warn` / `error`. |
| `db.enabled` | `--db.enabled` | `false` | Enable TimescaleDB metric persistence. |
| `db.dsn` | `--db.dsn` | — | PostgreSQL/TimescaleDB DSN (required when enabled). |
| `db.max_conns` | `--db.max_conns` | `8` | Maximum connections in the pool. |
| `db.max_idle` | `--db.max_idle` | `2` | Minimum idle connections in the pool. |
| `db.retention_days` | `--db.retention_days` | `30` | Metric retention window in days. |

`grpc.port` and `rest.port` must differ — the server fails to start otherwise.

### Example `server.conf`

```toml
[grpc]
addr = "0.0.0.0"
port = 5000
 
[rest]
addr = "0.0.0.0"
port = 8080
origins = ["http://localhost:5173"]
 
[log]
level = "info"
```

## Database (TimescaleDB)

Metric persistence is optional and disabled by default. When enabled, the server writes incoming agent batches to a TimescaleDB hypertable named `metrics` and serves historical data from `GET /api/agents/{id}/metrics`.

The server never runs migrations on startup. It refuses to start if the schema has pending migrations, so apply them explicitly before starting.

### Option A — Docker (recommended)

The repository ships a `docker-compose.yml` with three services: `timescaledb`, a one-shot `migrate`, and `server`. From the repository root:

```bash
docker compose up -d --build
```

Compose starts TimescaleDB, waits for it to become healthy, applies migrations, then starts the server with the database enabled. Ports: `5000` (gRPC) and `8080` (HTTP). Data is kept in the `volta_timescale` volume.

```bash
# status / logs
docker compose ps
docker compose logs -f server

# verify extension and schema
docker compose exec timescaledb psql -U volta -d volta \
  -c "select extname, extversion from pg_extension where extname='timescaledb';"
docker compose exec timescaledb psql -U volta -d volta -c "\d metrics"

# manual migrations
docker compose run --rm migrate volta-migrate status
docker compose run --rm migrate volta-migrate up
docker compose run --rm migrate volta-migrate down

# stop (keep data) / stop and wipe data
docker compose down
docker compose down -v
```

Development credentials are `volta` / `volta`. Change `POSTGRES_*` and `DB_DSN` in `docker-compose.yml` for anything non-local.

### Option B — Local PostgreSQL + TimescaleDB

Use this when running the server directly (for example with `make run`) against a native PostgreSQL installation.

```bash
# 1. check prerequisites (PostgreSQL reachable + timescaledb available)
make db-prereq

# 2. create the role, database and extension
#    (uses sudo -u postgres; prints a generated password unless VOLTA_DB_PASSWORD is set)
VOLTA_DB_PASSWORD=volta make db-init
```

`db-init` honours `VOLTA_DB_USER`, `VOLTA_DB_NAME`, `VOLTA_DB_HOST`, `VOLTA_DB_PORT` and `VOLTA_DB_PASSWORD` (defaults: `volta`, `volta`, `localhost`, `5432`, random). If `timescaledb` is not in `shared_preload_libraries`, restart PostgreSQL after `db-init`.

Configure the server in `sources/server/server.conf`:

```toml
[db]
enabled = true
dsn = "postgres://volta:volta@localhost:5432/volta?sslmode=disable"
max_conns = 8
max_idle = 2
retention_days = 30
```

Then apply migrations and run:

```bash
make db-migrate
make db-status
make run
```

The same configuration can be supplied through environment variables instead of `server.conf`:

```bash
export DB_ENABLED=true
export DB_DSN="postgres://volta:volta@localhost:5432/volta?sslmode=disable"
export DB_RETENTION_DAYS=30
make run
```

### Migrations

Migrations are embedded in the server binary and managed with [goose](https://github.com/pressly/goose).

```bash
make db-migrate        # apply all pending migrations (go run ./cmd/migrate up)
make db-status         # report whether the schema is up to date
make db-migrate-down   # roll back the latest migration
```

The same commands are available as `go run ./cmd/migrate up|down|status`. The DSN is resolved from the normal configuration chain (`db.dsn`, `DB_DSN`, `--db.dsn`).

### Verifying ingestion

```bash
# insert a test point
psql "postgres://volta:volta@localhost:5432/volta?sslmode=disable" -c \
  "INSERT INTO metrics (ts, agent_id, metric_type, device_id, value)
   VALUES (now(), '11111111-1111-1111-1111-111111111111', 103, NULL, 42.0);"

curl -s "http://localhost:8080/api/agents/11111111-1111-1111-1111-111111111111/metrics"
```

### Schema

- `metrics` — TimescaleDB hypertable partitioned by `ts` into 1-day chunks.
- Columns: `ts` (TIMESTAMPTZ), `agent_id` (UUID), `metric_type` (SMALLINT), `device_id` (TEXT, nullable), `value` (DOUBLE PRECISION).
- Index: `(agent_id, metric_type, device_id, ts DESC)`.
- Retention policy: `db.retention_days` (default 30 days).
- Writes are buffered and flushed every second or every 5000 rows; the buffer is flushed on graceful shutdown.

## API

### gRPC (`sources/proto/volta.proto`)

- `Connect` — bidirectional control-message stream kept open for the lifetime of the connection. Used for the initial handshake (agent ID assignment), ping/pong, and server-initiated commands (e.g. "start streaming").
- `StreamMetrics` — agent → server stream of metric batches, acknowledged per batch.

### REST (`/api`)

| Method & Path | Description |
|---|---|
| `GET /api/agents` | List currently connected agents. |
| `POST /api/agents/{id}/stream` | Instruct an agent to start streaming metrics. |
| `POST /api/agents/{id}/send` | Instruct an agent to send a one-off metric batch. *(not yet implemented)* |

### WebSocket

- `GET /ws?agent_id={id}` — subscribe to a live stream of metric batches for the given agent.
Only origins listed in `rest.origins` are allowed to call the REST API or open a WebSocket connection.

## FAQ

**Q: Build fails with `cannot find package "github.com/monvit/volta/server/pb"`.**
**A:** That package is generated, not committed. Run `make proto-go` from the repository root (with `protoc` and the Go protoc plugins installed) before building.

**Q: The server exits immediately with `gRPC and REST ports cannot be same`.**
**A:** Set distinct `grpc.port` and `rest.port` values via config file, environment variables, or flags.

**Q: I changed `go.mod`, but the build doesn't reflect it.**
**A:** Run `go mod tidy` from `sources/server` to sync `go.sum`.
