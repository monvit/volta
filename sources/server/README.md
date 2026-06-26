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
4. Environment variables, mapped to dot notation (`GRPC_PORT` → `grpc.port`)
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
