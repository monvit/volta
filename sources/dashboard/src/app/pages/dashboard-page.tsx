import { useMemo } from "react";
import { Skeleton } from "@/components/ui/skeleton";
import { MetricCard } from "@/features/metrics/metric-card";
import { CATEGORIES } from "@/lib/metrics";
import { useSeries, useStreamStatus, type LiveSeries } from "@/stores/metrics-store";
import { useSelectedAgentId } from "@/stores/agent-store";
import { useUiActions } from "@/stores/ui-store";

export function DashboardPage() {
  const series = useSeries();
  const streamStatus = useStreamStatus();
  const { openDetail } = useUiActions();
  const selectedAgentId = useSelectedAgentId();

  const grouped = useMemo(() => {
    const byCategory = new Map<string, LiveSeries[]>();
    for (const s of Object.values(series)) {
      const list = byCategory.get(s.category) ?? [];
      list.push(s);
      byCategory.set(s.category, list);
    }
    for (const list of byCategory.values()) {
      list.sort(
        (a, b) =>
          a.label.localeCompare(b.label) ||
          (a.deviceLabel ?? "").localeCompare(b.deviceLabel ?? ""),
      );
    }
    return byCategory;
  }, [series]);

  if (!selectedAgentId) {
    return (
      <EmptyState
        title="No agent selected"
        body="Pick an agent from the sidebar to start streaming live metrics."
      />
    );
  }

  if (grouped.size === 0) {
    return streamStatus === "open" ? (
      <SkeletonGrid />
    ) : (
      <EmptyState
        title="Waiting for data"
        body="Connected — waiting for the agent to emit its first metric batch. Live data appears here as it arrives."
      />
    );
  }

  return (
    <div className="flex flex-col gap-8">
      {CATEGORIES.map((category) => {
        const list = grouped.get(category.id);
        if (!list?.length) return null;
        const Icon = category.icon;
        return (
          <section key={category.id} className="flex flex-col gap-3">
            <div className="flex items-center gap-2">
              <Icon className="size-4 text-muted-foreground" />
              <h2 className="font-heading text-sm font-medium">{category.label}</h2>
              <span className="text-xs text-muted-foreground">{list.length}</span>
            </div>
            <div className="grid grid-cols-[repeat(auto-fill,minmax(280px,1fr))] gap-4">
              {list.map((s) => (
                <MetricCard key={s.key} series={s} onOpen={openDetail} />
              ))}
            </div>
          </section>
        );
      })}
    </div>
  );
}

function SkeletonGrid() {
  return (
    <div className="grid grid-cols-[repeat(auto-fill,minmax(280px,1fr))] gap-4">
      {Array.from({ length: 8 }).map((_, i) => (
        <Skeleton key={i} className="h-36 rounded-none" />
      ))}
    </div>
  );
}

function EmptyState({ title, body }: { title: string; body: string }) {
  return (
    <div className="flex h-full min-h-[60vh] flex-col items-center justify-center gap-2 text-center">
      <h2 className="font-heading text-sm font-medium">{title}</h2>
      <p className="max-w-md text-xs text-muted-foreground">{body}</p>
    </div>
  );
}
