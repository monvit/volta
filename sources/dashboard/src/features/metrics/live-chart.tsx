import { useCallback } from "react";
import { DetailedChart } from "@/components/charts/detailed-chart";
import { CATEGORY_COLOR } from "@/lib/color";
import { getMetricMeta } from "@/lib/metrics";
import type { LiveSeries } from "@/stores/metrics-store";

interface LiveChartProps {
  series: LiveSeries;
  height?: number;
}

/** Live metric detail chart for a single series. */
export function LiveChart({ series, height = 260 }: LiveChartProps) {
  const color = CATEGORY_COLOR[series.category];
  const meta = getMetricMeta(series.type);

  const formatValue = useCallback(
    (v: number) => {
      const { value, unit } = meta.format(v);
      return unit ? `${value} ${unit}` : value;
    },
    [meta],
  );

  return (
    <DetailedChart
      key={series.key}
      times={series.times}
      values={series.values}
      color={color}
      label={series.label}
      formatValue={formatValue}
      height={height}
    />
  );
}
