import { useEffect, useState, type ReactNode } from "react";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { shortSeriesKey } from "@/lib/analysis";
import { formatNumber } from "@/lib/format";
import { useSelectedAgentId } from "@/stores/agent-store";
import {
  useAnalysisActions,
  useAnalysisError,
  useAnalysisLoading,
  useAnalysisResult,
} from "@/stores/analysis-store";
import { useBaseUrl } from "@/stores/connection-store";
import { useSeries } from "@/stores/metrics-store";

const controlClass =
  "h-8 w-full min-w-0 rounded-none border border-input bg-transparent px-2 text-xs outline-none focus-visible:border-ring focus-visible:ring-1 focus-visible:ring-ring/50";

interface AnalysisPanelProps {
  selectedSeriesKey: string;
}

export function AnalysisPanel({ selectedSeriesKey }: AnalysisPanelProps) {
  const series = useSeries();
  const options = Object.values(series);
  const selectedAnalysisKey = series[selectedSeriesKey]?.analysisKey ?? "";

  const [x, setX] = useState(selectedAnalysisKey);
  const [y, setY] = useState("");
  const [minutes, setMinutes] = useState(60);
  const [bucketSeconds, setBucketSeconds] = useState(60);
  const [sigma, setSigma] = useState(3);

  const baseUrl = useBaseUrl();
  const agentId = useSelectedAgentId();
  const result = useAnalysisResult();
  const loading = useAnalysisLoading();
  const error = useAnalysisError();
  const { run, reset } = useAnalysisActions();

  useEffect(() => {
    setX(selectedAnalysisKey);
    reset();
  }, [selectedAnalysisKey, reset]);

  const effectiveX = x || selectedAnalysisKey;
  const effectiveY = y || options.find((item) => item.analysisKey !== effectiveX)?.analysisKey || "";
  const xSeries = options.find((item) => item.analysisKey === effectiveX);
  const ySeries = options.find((item) => item.analysisKey === effectiveY);
  const metricTypes = [...new Set([xSeries?.type, ySeries?.type].filter((v) => v !== undefined))];

  const onRun = async () => {
    if (!agentId || !effectiveX || !effectiveY) return;
    const to = new Date();
    const from = new Date(to.getTime() - minutes * 60_000);
    await run(baseUrl, agentId, {
      from: from.toISOString(),
      to: to.toISOString(),
      metric_types: metricTypes,
      bucket_seconds: bucketSeconds,
      sigma,
      min_points: 8,
      pairs: [[effectiveX, effectiveY]],
    });
  };

  return (
    <Card className="gap-3">
      <div className="flex flex-wrap items-end gap-3">
        <Field label="Series X">
          <select className={controlClass} value={effectiveX} onChange={(e) => setX(e.target.value)}>
            {options.map((item) => (
              <option key={item.key} value={item.analysisKey}>
                {shortSeriesKey(item.analysisKey)}
              </option>
            ))}
          </select>
        </Field>
        <Field label="Series Y">
          <select className={controlClass} value={effectiveY} onChange={(e) => setY(e.target.value)}>
            {options.map((item) => (
              <option key={item.key} value={item.analysisKey}>
                {shortSeriesKey(item.analysisKey)}
              </option>
            ))}
          </select>
        </Field>
        <Field label="Last (min)">
          <Input
            type="number"
            min={1}
            value={minutes}
            onChange={(e) => setMinutes(Number(e.target.value))}
          />
        </Field>
        <Field label="Bucket (s)">
          <Input
            type="number"
            min={1}
            value={bucketSeconds}
            onChange={(e) => setBucketSeconds(Number(e.target.value))}
          />
        </Field>
        <Field label="Sigma">
          <Input
            type="number"
            min={0.1}
            step={0.1}
            value={sigma}
            onChange={(e) => setSigma(Number(e.target.value))}
          />
        </Field>
        <Button size="sm" disabled={loading || !agentId || !effectiveX || !effectiveY} onClick={onRun}>
          {loading ? "Analysing…" : "Run analysis"}
        </Button>
      </div>

      {error && <p className="text-xs text-destructive">{error}</p>}

      {result && (
        <div className="flex flex-col gap-3 text-xs">
          <div className="grid grid-cols-[repeat(auto-fill,minmax(170px,1fr))] gap-2">
            {result.series.map((item) => {
              const key = `${item.metric_type}@${item.device_id}`;
              return (
                <div key={key} className="rounded-none border border-border px-2.5 py-2">
                  <div className="truncate text-muted-foreground">{shortSeriesKey(key)}</div>
                  <div className="font-heading text-sm tabular-nums">
                    mean {formatNumber(item.mean)} · sd {formatNumber(item.stddev)}
                  </div>
                  <div className="text-muted-foreground">
                    n={item.count} min={formatNumber(item.min)} max={formatNumber(item.max)}
                  </div>
                </div>
              );
            })}
          </div>

          {result.correlations.map((item) => (
            <p key={`${item.x}-${item.y}`} className="tabular-nums">
              Pearson r = {formatNumber(item.pearson_r, 3)} ({shortSeriesKey(item.x)} /{" "}
              {shortSeriesKey(item.y)}, n={item.n})
            </p>
          ))}
          {result.regressions.map((item) => (
            <p key={`${item.x}-${item.y}`} className="tabular-nums">
              {shortSeriesKey(item.y)} = {formatNumber(item.slope, 6)} × {shortSeriesKey(item.x)} +{" "}
              {formatNumber(item.intercept, 6)} (R²={formatNumber(item.r_squared, 3)})
            </p>
          ))}
          <p className="text-muted-foreground">
            Anomalies: {result.anomalies.length}
          </p>
          {result.warnings.map((warning) => (
            <p key={warning} className="text-muted-foreground">
              {warning}
            </p>
          ))}
        </div>
      )}
    </Card>
  );
}

function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <label className="flex min-w-28 flex-1 flex-col gap-1 text-[10px] tracking-wide text-muted-foreground uppercase">
      {label}
      {children}
    </label>
  );
}
