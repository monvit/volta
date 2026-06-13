import { create } from "zustand";
import type { MetricBatch, MetricType } from "@/proto/gen/types/metric_pb";
import { formatDeviceId, getMetricMeta, seriesKey, type MetricCategory } from "@/lib/metrics";
import { nsToMs } from "@/lib/format";

/** Rolling window kept per series. At ~1 Hz this is ~10 minutes of live tail. */
export const MAX_POINTS = 600;

export type StreamStatus = "idle" | "connecting" | "open" | "reconnecting";

export interface LiveSeries {
  key: string;
  type: MetricType;
  category: MetricCategory;
  label: string;
  deviceLabel?: string;
  times: number[];
  values: number[];
  last: number;
}

interface MetricsActions {
  setStreamStatus: (status: StreamStatus) => void;
  ingest: (batch: MetricBatch) => void;
  reset: () => void;
}

interface MetricsState {
  series: Record<string, LiveSeries>;
  streamStatus: StreamStatus;
  actions: MetricsActions;
}

const useMetricsStore = create<MetricsState>((set) => ({
  series: {},
  streamStatus: "idle",
  actions: {
    setStreamStatus: (streamStatus) => set({ streamStatus }),
    reset: () => set({ series: {} }),
    ingest: (batch) => {
      const header = batch.header;
      const count = Math.min(batch.timestampsNs.length, batch.values.length);
      if (!header || count === 0) return;

      const { metricType: type, deviceId: device } = header;
      const key = seriesKey(type, device);

      set((state) => {
        const prev = state.series[key];
        const times = prev ? prev.times.slice() : [];
        const values = prev ? prev.values.slice() : [];
        for (let i = 0; i < count; i++) {
          times.push(nsToMs(batch.timestampsNs[i]));
          values.push(batch.values[i]);
        }
        const overflow = times.length - MAX_POINTS;
        if (overflow > 0) {
          times.splice(0, overflow);
          values.splice(0, overflow);
        }
        const meta = getMetricMeta(type);
        const next: LiveSeries = {
          key,
          type,
          category: meta.category,
          label: meta.label,
          deviceLabel: formatDeviceId(device),
          times,
          values,
          last: values[values.length - 1] ?? Number.NaN,
        };
        return { series: { ...state.series, [key]: next } };
      });
    },
  },
}));

export const useSeries = () => useMetricsStore((s) => s.series);
export const useStreamStatus = () => useMetricsStore((s) => s.streamStatus);
export const useMetricsActions = () => useMetricsStore((s) => s.actions);

/** Stable action handle for non-React callers (stream hook). */
export const metricsActions = useMetricsStore.getState().actions;
