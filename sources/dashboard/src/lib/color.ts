import type { MetricCategory } from "@/lib/metrics";

/** Accent color per category, used for sparklines and chart strokes. */
export const CATEGORY_COLOR: Record<MetricCategory, string> = {
  cpu: "#60a5fa", // blue-400
  gpu: "#a78bfa", // violet-400
  memory: "#34d399", // emerald-400
  disk: "#fbbf24", // amber-400
  network: "#f472b6", // pink-400
};
