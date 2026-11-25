# Volta Dashboard

Web UI for visualising collected agent metrics. React 19 SPA that talks to the Volta server over HTTP + WebSocket.

## Stack

Vite · React 19 (+ React Compiler) · TypeScript · pnpm · Tailwind v4 · shadcn (on Base UI) · Zustand · uPlot · `@bufbuild/protobuf`.

## Develop

```bash
vp install            # or: use pnpm directly
pnpm run proto:gen    # once after clone, or after editing ../proto/**
vp dev                # serves on :5173
vp build              # outputs to dist/
vp preview            # serve the built output for testing
```

## Deploy

The dashboard builds to **standalone static files**. Host the `dist/` output anywhere; the user enters their server URL on the connection screen.

## How it talks to the server (live-only)

1. `GET /api/agents` — probe + list connected agents.
2. Open `WS /ws?agent_id={id}`, then `POST /api/agents/{id}/stream` to start streaming.
3. The socket pushes binary protobuf `MetricBatch` frames, decoded with `@bufbuild/protobuf`.
4. The analysis panel on the detail page calls `POST /api/agents/{id}/analyze` with a time window and a pair of series. The server reads stored metrics and returns the Analysis sidecar's JSON result; the dashboard never calls the sidecar directly. The endpoint returns `503` when the database or analysis sidecar is disabled.

The live views use a rolling in-memory tail only; persisted history and analysis require the remote deployment with TimescaleDB and the Analysis sidecar.

## Layout

- `src/app/` — entry, provider, router, route pages
- `src/components/` — shared + shadcn `ui/` components, charts, layout
- `src/features/` — domain logic (`metrics/`, `analysis/`, `agents/`, `connection/`)
- `src/stores/` — Zustand stores (connection, agent, metrics, analysis, ui)
- `src/api/client.ts` — HTTP + WS URL helpers and fetchers
- `src/lib/` — formatting, colors, stats, metric metadata, analysis result types
- `src/proto/gen/` — protobuf codegen (do not edit by hand)
