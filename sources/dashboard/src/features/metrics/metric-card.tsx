import { Card } from "@/components/ui/card";
import { Sparkline } from "@/components/charts/sparkline";
import { CATEGORY_COLOR } from "@/lib/color";
import { formatMetricValue } from "@/lib/metrics";
import type { LiveSeries } from "@/stores/metrics-store";

const SPARK_POINTS = 60;

export function MetricCard({
  series,
  onOpen,
}: {
  series: LiveSeries;
  onOpen: (key: string) => void;
}) {
  const color = CATEGORY_COLOR[series.category];
  const { value, unit } = formatMetricValue(series.type, series.last);

  return (
    <button type="button" onClick={() => onOpen(series.key)} className="block text-left">
      <Card
        className="cursor-pointer gap-3 transition-all hover:ring-foreground/25 focus-visible:ring-ring/50"
      >
        <div className="flex items-start justify-between gap-2 px-(--card-spacing)">
          <span className="truncate text-sm text-muted-foreground">{series.label}</span>
          {series.deviceLabel && (
            <span className="shrink-0 text-xs text-muted-foreground/70">
              {series.deviceLabel}
            </span>
          )}
        </div>
        <div className="flex items-baseline gap-1.5 px-(--card-spacing)">
          <span className="font-heading text-2xl tabular-nums">{value}</span>
          {unit && <span className="text-sm text-muted-foreground">{unit}</span>}
        </div>
        <Sparkline
          values={series.values.slice(-SPARK_POINTS)}
          color={color}
          height={48}
          className="px-(--card-spacing)"
        />
      </Card>
    </button>
  );
}
