# Volta

Volta is a lightweight, energy-aware monitoring platform for tracking system and hardware metrics in real time.

It follows a classic **Agent → Server → Dashboard** model:

- One or more **Agents** (C++) run on monitored machines, sample low-level hardware/system metrics and stream them to a single configured server over **gRPC**.
- The **Server** (Go) aggregates metrics from all connected agents and exposes them to clients over **WebSockets**.
- The **Dashboard** (React) visualises live and historical metrics served by the Server.
For the full architecture, deployment models (local vs. remote) and the metric catalogue, see [docs/pl/ARCHITECTURE.md](docs/pl/ARCHITECTURE.md) and [docs/pl/METRICS.md](docs/pl/METRICS.md).

## Repository Structure

| Path | Component | Stack | Description |
|---|---|---|---|
| [`sources/agent`](sources/agent) | Agent | C++20, CMake, vcpkg | Collects CPU/GPU power and standard system metrics; streams them to the server. |
| [`sources/server`](sources/server) | Server | Go | Central aggregation point; gRPC ingest, REST + WebSocket API for the dashboard. |
| [`sources/dashboard`](sources/dashboard) | Dashboard | TypeScript, React, Vite | Web UI for visualising live and historical agent metrics. |
| [`sources/proto`](sources/proto) | Proto | Protocol Buffers | Shared gRPC contract between Agent and Server. |
| [`docs`](docs) | Docs | Markdown | Architecture, metrics and other supporting documentation. |

## Maintainers

- **Agent** — @FW-Nagorko
- **Server** — @kox13
- **Dashboard** - @patryk-przybysz

## Development Setup

### 1. Initial Requirements

- **Git**
- **Make**
- **protoc** (Protocol Buffers compiler) + Go protoc plugins, required to (re)generate the gRPC code shared by Agent and Server
- Component-specific toolchains — see [`sources/agent/README.md`](sources/agent/README.md), [`sources/server/README.md`](sources/server/README.md) and [`sources/dashboard/README.md`](sources/dashboard/README.md)

### 2. Clone the Repository

```bash
git clone https://github.com/monvit/volta.git
cd volta
```

### 3. Generate Protobuf Code

The gRPC/protobuf bindings consumed by the server (`sources/server/pb`) are generated from `sources/proto` and are **not committed** to the repository.

```bash
go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest

cd sources/server
make proto
```

### 4. Run a Component

For the Agent and Dashboard, follow the dedicated setup steps in their respective READMEs.

### 5. Git Hooks (Optional)

```bash
make hooks
```

## Continuous Integration

Each component is linted, built and tested independently and only on relevant changes — see [`.github/workflows`](.github/workflows) (`agent_ci.yml`, `server_ci.yml`, `dashboard_ci.yml`).

## License

Volta is licensed under the [Apache License 2.0](LICENSE).
