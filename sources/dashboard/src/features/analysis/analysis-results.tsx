import { ChartLineIcon, WarningCircleIcon } from "@phosphor-icons/react";
import { Badge } from "@/components/ui/badge";
import { Separator } from "@/components/ui/separator";
import { AnalysisChart } from "@/features/analysis/analysis-chart";
import { CorrelationChart } from "@/features/analysis/correlation-chart";
import {
  describeAnalysisKey,
  describeAnalysisSeries,
  describeCorrelation,
  formatAnalysisKeyLabel,
  formatGeneratedAt,
  formatTimestampNs,
  type AnalysisResult,
  type MovingAverageSeries,
} from "@/lib/analysis";
import { formatNumber } from "@/lib/format";
import { CATEGORIES } from "@/lib/metrics";

interface AnalysisResultsProps {
  result: AnalysisResult;
  smaPeriod: number;
  emaPeriod: number;
  sigma: number;
  bucketSeconds: number;
}

const MAX_ANOMALY_ROWS = 200;

function latestMovingAverage(series?: MovingAverageSeries) {
  if (!series?.points.length) return undefined;
  let latest = series.points[0];
  for (const point of series.points) {
    if (point.timestamp_ns >= latest.timestamp_ns) latest = point;
  }
  return latest;
}

export function AnalysisResults({
  result,
  smaPeriod,
  emaPeriod,
  sigma,
  bucketSeconds,
}: AnalysisResultsProps) {
  const rollupsByKey = new Map(
    result.rollups.map((item) => [`${item.metric_type}@${item.device_id}`, item]),
  );
  const averagesByKey = new Map(
    (result.moving_averages ?? []).map((item) => [`${item.metric_type}@${item.device_id}`, item]),
  );
  const regressionsByPair = new Map(
    result.regressions.map((item) => [`${item.x}::${item.y}`, item]),
  );

  const visibleAnomalies = result.anomalies.slice(0, MAX_ANOMALY_ROWS);
  const hiddenAnomalies = result.anomalies.length - visibleAnomalies.length;

  return (
    <div className="flex flex-col gap-5 pt-1 text-xs">
      <Separator />

      <div className="flex flex-wrap items-center justify-between gap-2">
        <div className="flex flex-wrap items-center gap-2">
          <ChartLineIcon className="size-4 text-muted-foreground" />
          <h3 className="font-heading text-sm font-medium">Analysis results</h3>
          <Badge variant="secondary">{result.series.length} series</Badge>
          <Badge variant={result.anomalies.length > 0 ? "destructive" : "outline"}>
            {result.anomalies.length} {result.anomalies.length === 1 ? "anomaly" : "anomalies"}
          </Badge>
          {result.warnings.length > 0 && (
            <Badge
              variant="outline"
              className="border-amber-500/40 bg-amber-500/10 text-amber-600 dark:text-amber-400"
            >
              {result.warnings.length} {result.warnings.length === 1 ? "warning" : "warnings"}
            </Badge>
          )}
        </div>
        <span className="text-[11px] text-muted-foreground tabular-nums">
          {formatGeneratedAt(result.generated_at)} · {bucketSeconds}s buckets · {result.source}
        </span>
      </div>

      {result.warnings.length > 0 && (
        <div className="flex flex-col gap-1.5 border border-amber-500/30 bg-amber-500/5 px-3 py-2.5 text-xs">
          <div className="flex items-center gap-1.5 font-medium text-amber-600 dark:text-amber-400">
            <WarningCircleIcon className="size-4 shrink-0" />
            <span>Warnings</span>
          </div>
          <ul className="flex flex-col gap-1 pl-5 text-muted-foreground">
            {result.warnings.map((warning) => (
              <li key={warning} className="list-disc">
                {warning}
              </li>
            ))}
          </ul>
        </div>
      )}

      {result.series.length > 0 && (
        <div className="flex flex-col gap-2.5">
          <SectionHeading title="Series overview & trends" />
          <div
            className={
              result.series.length === 1
                ? "grid grid-cols-1 gap-4"
                : "grid grid-cols-1 gap-4 xl:grid-cols-2"
            }
          >
            {result.series.map((item) => {
              const key = `${item.metric_type}@${item.device_id}`;
              const display = describeAnalysisSeries(item.metric_type, item.device_id);
              const categoryMeta = CATEGORIES.find((c) => c.id === display.category);
              const Icon = categoryMeta?.icon;
              const rollup = rollupsByKey.get(key);
              const ma = averagesByKey.get(key);
              const latestMa = latestMovingAverage(ma);

              return (
                <div key={key} className="flex flex-col gap-3 border border-border p-3.5">
                  <div className="flex flex-wrap items-center justify-between gap-2">
                    <div className="flex items-center gap-2">
                      {Icon && <Icon className="size-4 text-muted-foreground" />}
                      <span className="font-heading text-sm font-medium">{display.title}</span>
                      <Badge variant="outline">{display.categoryLabel}</Badge>
                      {display.deviceLabel && (
                        <Badge variant="secondary">{display.deviceLabel}</Badge>
                      )}
                    </div>
                    <span className="text-[11px] text-muted-foreground tabular-nums">
                      n = {formatNumber(item.count, 0)} samples
                    </span>
                  </div>

                  <div className="grid grid-cols-2 gap-2 sm:grid-cols-4">
                    <StatCell label="Mean" value={display.formatValue(item.mean)} />
                    <StatCell label="Median" value={display.formatValue(item.median)} />
                    <StatCell label="Latest" value={display.formatValue(item.last)} />
                    <StatCell label="First" value={display.formatValue(item.first)} />
                    <StatCell label="Min" value={display.formatValue(item.min)} />
                    <StatCell label="Max" value={display.formatValue(item.max)} />
                    <StatCell label="Std Dev" value={display.formatValue(item.stddev)} />
                    <StatCell
                      label={`SMA(${smaPeriod}) / EMA(${emaPeriod})`}
                      value={`${latestMa?.sma == null ? "—" : display.formatValue(latestMa.sma)} / ${latestMa ? display.formatValue(latestMa.ema) : "—"}`}
                    />
                  </div>

                  <AnalysisChart
                    rollup={rollup}
                    movingAverage={ma}
                    formatValue={display.formatValue}
                  />
                </div>
              );
            })}
          </div>
        </div>
      )}

      {(result.correlations.length > 0 || result.regressions.length > 0) && (
        <div className="flex flex-col gap-2.5">
          <SectionHeading title="Correlation & regression" />
          <div className="flex flex-col gap-2.5">
            {result.correlations.map((corr) => {
              const reg = regressionsByPair.get(`${corr.x}::${corr.y}`);
              const strength = describeCorrelation(corr.pearson_r);
              const xDisplay = describeAnalysisKey(corr.x);
              const yDisplay = describeAnalysisKey(corr.y);
              const xRollup = rollupsByKey.get(corr.x);
              const yRollup = rollupsByKey.get(corr.y);
              const interceptSign = reg && reg.intercept < 0 ? "−" : "+";

              return (
                <div
                  key={`${corr.x}-${corr.y}`}
                  className="flex flex-col gap-3 border border-border p-3.5"
                >
                  <div className="flex flex-wrap items-center justify-between gap-2">
                    <div className="flex flex-wrap items-center gap-2">
                      <span className="font-heading text-sm font-medium">
                        {formatAnalysisKeyLabel(corr.x)}
                      </span>
                      <span className="text-muted-foreground">vs</span>
                      <span className="font-heading text-sm font-medium">
                        {formatAnalysisKeyLabel(corr.y)}
                      </span>
                    </div>
                    <div className="flex items-center gap-2">
                      <Badge variant={strength.variant}>{strength.label}</Badge>
                      <span className="text-[11px] text-muted-foreground tabular-nums">
                        n = {formatNumber(corr.n, 0)} buckets
                      </span>
                    </div>
                  </div>

                  <div className="grid grid-cols-1 gap-2 sm:grid-cols-3">
                    <StatCell label="Pearson r" value={formatNumber(corr.pearson_r, 3)} />
                    {reg && <StatCell label="R² (fit)" value={formatNumber(reg.r_squared, 3)} />}
                    {reg && (
                      <StatCell
                        label="Linear model"
                        value={`${yDisplay.title} = ${formatNumber(reg.slope, 4)} × ${xDisplay.title} ${interceptSign} ${formatNumber(Math.abs(reg.intercept), 4)}`}
                      />
                    )}
                  </div>
                  {xRollup && yRollup ? (
                    <CorrelationChart
                      xRollup={xRollup}
                      yRollup={yRollup}
                      regression={reg}
                      xLabel={formatAnalysisKeyLabel(corr.x)}
                      yLabel={formatAnalysisKeyLabel(corr.y)}
                      formatX={xDisplay.formatValue}
                      formatY={yDisplay.formatValue}
                    />
                  ) : (
                    <p className="text-muted-foreground">
                      Bucket data is unavailable for this correlation.
                    </p>
                  )}
                </div>
              );
            })}
          </div>
        </div>
      )}

      {result.correlations.length === 0 && (
        <div className="flex flex-col gap-2.5">
          <SectionHeading title="Correlation & regression" />
          <p className="border border-border px-3 py-2.5 text-muted-foreground">
            No correlation available. Select two series and check the warnings for details.
          </p>
        </div>
      )}

      <div className="flex flex-col gap-2.5">
        <div className="flex items-center justify-between gap-2">
          <SectionHeading title={`Anomalies (${result.anomalies.length})`} />
          <span className="text-[11px] text-muted-foreground tabular-nums">
            Threshold: &gt; {formatNumber(sigma, 1)}σ
          </span>
        </div>

        {result.anomalies.length === 0 ? (
          <p className="border border-border px-3 py-2.5 text-muted-foreground">
            No samples exceeded {formatNumber(sigma, 1)}σ in the selected window.
          </p>
        ) : (
          <div className="max-h-72 overflow-auto border border-border">
            <table className="w-full border-collapse text-left text-xs tabular-nums">
              <thead className="sticky top-0 bg-muted/80 text-[10px] tracking-wide text-muted-foreground uppercase backdrop-blur-xs">
                <tr className="border-b border-border">
                  <th className="px-3 py-2 font-medium">Time</th>
                  <th className="px-3 py-2 font-medium">Series</th>
                  <th className="px-3 py-2 font-medium text-right">Value</th>
                  <th className="px-3 py-2 font-medium text-right">Deviation</th>
                </tr>
              </thead>
              <tbody>
                {visibleAnomalies.map((anomaly, index) => {
                  const seriesKey = `${anomaly.metric_type}@${anomaly.device_id}`;
                  const display = describeAnalysisSeries(anomaly.metric_type, anomaly.device_id);
                  return (
                    <tr
                      key={`${seriesKey}-${anomaly.timestamp_ns}-${index}`}
                      className="border-b border-border last:border-b-0"
                    >
                      <td className="px-3 py-2 text-muted-foreground">
                        {formatTimestampNs(anomaly.timestamp_ns)}
                      </td>
                      <td className="px-3 py-2 font-medium">{formatAnalysisKeyLabel(seriesKey)}</td>
                      <td className="px-3 py-2 text-right">{display.formatValue(anomaly.value)}</td>
                      <td className="px-3 py-2 text-right">
                        <Badge variant="destructive">{formatNumber(anomaly.z_score, 2)}σ</Badge>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
            {hiddenAnomalies > 0 && (
              <div className="border-t border-border bg-muted/40 px-3 py-1.5 text-[11px] text-muted-foreground">
                Showing first {MAX_ANOMALY_ROWS} of {result.anomalies.length} anomalies.
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

function SectionHeading({ title }: { title: string }) {
  return (
    <h4 className="text-[11px] font-medium tracking-wide text-muted-foreground uppercase">
      {title}
    </h4>
  );
}

function StatCell({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex flex-col gap-0.5 border border-border bg-muted/20 px-2.5 py-1.5">
      <span className="text-[10px] tracking-wide text-muted-foreground uppercase">{label}</span>
      <span className="font-heading text-xs font-medium tabular-nums break-words">{value}</span>
    </div>
  );
}
