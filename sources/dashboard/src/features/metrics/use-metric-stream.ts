import { useEffect } from "react";
import { fromBinary } from "@bufbuild/protobuf";
import { MetricBatchSchema } from "@/proto/gen/types/metric_pb";
import { fetchAgents, startStream, wsUrl } from "@/api/client";
import { useBaseUrl, useIsConnected } from "@/stores/connection-store";
import { agentActions, useSelectedAgentId } from "@/stores/agent-store";
import { metricsActions } from "@/stores/metrics-store";

const BACKOFF_MIN_MS = 1000;
const BACKOFF_MAX_MS = 30000;

/**
 * Subscribes to the selected agent's live `MetricBatch` stream: open /ws first,
 * then POST /stream so opening batches aren't missed. Reconnects with exponential
 * backoff and refreshes the agent list on every successful (re)connect. Stream
 * status is published to the metrics store. Mount once near the app root.
 */
export function useMetricStream() {
  const baseUrl = useBaseUrl();
  const connected = useIsConnected();
  const agentId = useSelectedAgentId();

  useEffect(() => {
    if (!connected || !agentId) {
      metricsActions.setStreamStatus("idle");
      return;
    }

    let socket: WebSocket | null = null;
    let reconnectTimer: ReturnType<typeof setTimeout> | undefined;
    let attempt = 0;
    let disposed = false;

    metricsActions.reset();

    const scheduleReconnect = () => {
      if (disposed) return;
      metricsActions.setStreamStatus("reconnecting");
      const delay = Math.min(BACKOFF_MIN_MS * 2 ** attempt, BACKOFF_MAX_MS);
      attempt += 1;
      reconnectTimer = setTimeout(connect, delay);
    };

    function connect() {
      if (disposed) return;
      metricsActions.setStreamStatus(attempt === 0 ? "connecting" : "reconnecting");
      const ws = new WebSocket(wsUrl(baseUrl, agentId!));
      ws.binaryType = "arraybuffer";
      socket = ws;

      ws.onopen = () => {
        if (disposed) return;
        attempt = 0;
        metricsActions.setStreamStatus("open");
        void startStream(baseUrl, agentId!).catch(() => undefined);
        // Refresh the agent list on (re)connect instead of polling continuously.
        void fetchAgents(baseUrl)
          .then(agentActions.setAgents)
          .catch(() => undefined);
      };

      ws.onmessage = (event) => {
        if (!(event.data instanceof ArrayBuffer)) return;
        try {
          metricsActions.ingest(fromBinary(MetricBatchSchema, new Uint8Array(event.data)));
        } catch {
          // Drop undecodable frames rather than tearing down the stream.
        }
      };

      ws.onerror = () => ws.close();

      ws.onclose = () => {
        if (disposed) return;
        socket = null;
        scheduleReconnect();
      };
    }

    connect();

    return () => {
      disposed = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      if (socket) {
        socket.onopen = socket.onmessage = socket.onerror = socket.onclose = null;
        socket.close();
      }
      metricsActions.setStreamStatus("idle");
    };
  }, [baseUrl, connected, agentId]);
}
