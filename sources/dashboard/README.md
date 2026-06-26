# Volta Dashboard

Web UI for visualising collected agent metrics. React 19 SPA that talks to the Volta server over HTTP + WebSocket.

## Stack

Vite · React 19 (+ React Compiler) · TypeScript · pnpm · Tailwind v4 · shadcn (on Base UI) · Zustand · uPlot · `@bufbuild/protobuf`.

## Develop

```bash
vp install            # or: use pnpm directly
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

There is **no history/persistence** yet — the UI shows a rolling live tail only.

## Layout

- `src/app/` — entry, provider, router, route pages
- `src/components/` — shared + shadcn `ui/` components, charts, layout
- `src/features/` — domain logic (`metrics/`, `agents/`, `connection/`)
- `src/stores/` — Zustand stores (connection, agent, metrics, ui)
- `src/api/client.ts` — HTTP + WS URL helpers and fetchers
- `src/lib/` — formatting, colors, stats, metric metadata
- `src/proto/gen/` — protobuf codegen (do not edit by hand)
