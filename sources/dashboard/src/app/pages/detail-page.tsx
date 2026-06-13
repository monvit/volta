import { useMemo } from "react";
import { ArrowLeftIcon } from "@phosphor-icons/react";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { LiveChart } from "@/features/metrics/live-chart";
import { CATEGORIES, formatMetricValue } from "@/lib/metrics";
import { computeStats } from "@/lib/stats";
import { useSeries } from "@/stores/metrics-store";
import { useDetailSeriesKey, useUiActions } from "@/stores/ui-store";

export function DetailPage() {
  const detailSeriesKey = useDetailSeriesKey();
  const { setView } = useUiActions();
  const series = useSeries()[detailSeriesKey ?? ""];

  const stats = useMemo(() => (series ? computeStats(series.values) : null), [series]);

  if (!series || !stats) {
    return (
      <div className="flex h-full min-h-[60vh] flex-col items-center justify-center gap-3 text-center">
        <p className="text-xs text-muted-foreground">
          This metric is no longer in the live window.
        </p>
        <Button variant="outline" size="sm" onClick={() => setView("dashboard")}>
          <ArrowLeftIcon /> Back to dashboard
        </Button>
      </div>
    );
  }

  const category = CATEGORIES.find((c) => c.id === series.category);
  const Icon = category?.icon;
  const fmt = (v: number) => {
    const { value, unit } = formatMetricValue(series.type, v);
    return unit ? `${value} ${unit}` : value;
  };

  return (
    <div className="flex flex-col gap-4">
      <div className="flex items-center gap-2 text-xs text-muted-foreground">
        <button
          type="button"
          className="hover:text-foreground"
          onClick={() => setView("dashboard")}
        >
          Dashboard
        </button>
        <span>/</span>
        <span className="text-foreground">{category?.label}</span>
        <span>/</span>
        <span className="text-foreground">{series.label}</span>
        {series.deviceLabel && (
          <span className="text-muted-foreground/70">· {series.deviceLabel}</span>
        )}
      </div>

      <div className="flex items-center gap-2">
        {Icon && <Icon className="size-5 text-muted-foreground" />}
        <h1 className="font-heading text-lg font-medium">{series.label}</h1>
      </div>

      <Card className="gap-0 p-0">
        <div className="p-3">
          <LiveChart series={series} height={320} />
        </div>
      </Card>

      <div className="grid grid-cols-[repeat(auto-fill,minmax(130px,1fr))] gap-3">
        <Stat label="Current" value={fmt(stats.last)} />
        <Stat label="Min" value={fmt(stats.min)} />
        <Stat label="Max" value={fmt(stats.max)} />
        <Stat label="Mean" value={fmt(stats.mean)} />
        <Stat label="Std Dev" value={fmt(stats.stddev)} />
        <Stat label="Samples" value={String(stats.count)} />
      </div>
      <p className="text-[10px] text-muted-foreground/70">
        Statistics are computed over the live window buffered since connecting.
      </p>
    </div>
  );
}

function Stat({ label, value }: { label: string; value: string }) {
  return (
    <Card size="sm" className="gap-1">
      <span className="px-(--card-spacing) text-[10px] tracking-wide text-muted-foreground uppercase">
        {label}
      </span>
      <span className="px-(--card-spacing) font-heading text-base tabular-nums">{value}</span>
    </Card>
  );
}
