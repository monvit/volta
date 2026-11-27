import { useEffect, useState, type ReactNode } from "react";
import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { AnalysisResults } from "@/features/analysis/analysis-results";
import { formatAnalysisKeyLabel } from "@/lib/analysis";
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

interface SubmittedOptions {
  smaPeriod: number;
  emaPeriod: number;
  sigma: number;
  bucketSeconds: number;
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
  const [smaPeriod, setSmaPeriod] = useState("10");
  const [emaPeriod, setEmaPeriod] = useState("10");
  const [submittedOptions, setSubmittedOptions] = useState<SubmittedOptions | null>(null);

  const baseUrl = useBaseUrl();
  const agentId = useSelectedAgentId();
  const result = useAnalysisResult();
  const loading = useAnalysisLoading();
  const error = useAnalysisError();
  const { run, reset } = useAnalysisActions();

  useEffect(() => {
    setX(selectedAnalysisKey);
  }, [selectedAnalysisKey]);

  useEffect(() => {
    setSubmittedOptions(null);
    return reset;
  }, [agentId, baseUrl, selectedAnalysisKey, reset]);

  const effectiveX = x || selectedAnalysisKey;
  const effectiveY =
    y || options.find((item) => item.analysisKey !== effectiveX)?.analysisKey || "";
  const xSeries = options.find((item) => item.analysisKey === effectiveX);
  const ySeries = options.find((item) => item.analysisKey === effectiveY);
  const metricTypes = [...new Set([xSeries?.type, ySeries?.type].filter((v) => v !== undefined))];

  const periodsValid = [smaPeriod, emaPeriod].every(
    (period) => Number.isSafeInteger(Number(period)) && Number(period) >= 1,
  );
  const canRun = Boolean(
    agentId &&
    xSeries &&
    periodsValid &&
    Number.isSafeInteger(minutes) &&
    minutes >= 1 &&
    minutes <= 1e11 &&
    Number.isSafeInteger(bucketSeconds) &&
    bucketSeconds >= 1 &&
    Number.isFinite(sigma) &&
    sigma >= 0.1,
  );

  const onRun = async (operation: "analysis" | "report") => {
    if (!agentId || !canRun || loading) return;
    const sma = Number(smaPeriod);
    const ema = Number(emaPeriod);
    if (operation === "analysis") {
      setSubmittedOptions({ smaPeriod: sma, emaPeriod: ema, sigma, bucketSeconds });
    }
    const to = new Date();
    const from = new Date(to.getTime() - minutes * 60_000);
    await run(
      baseUrl,
      agentId,
      {
        from: from.toISOString(),
        to: to.toISOString(),
        metric_types: metricTypes,
        bucket_seconds: bucketSeconds,
        sigma,
        min_points: 8,
        sma_period: sma,
        ema_period: ema,
        pairs: ySeries ? [[effectiveX, effectiveY]] : [],
      },
      operation,
    );
  };

  return (
    <Card className="gap-4 px-(--card-spacing)">
      <div className="flex flex-wrap items-end gap-3">
        <Field label="Series X">
          <select
            className={controlClass}
            value={effectiveX}
            onChange={(e) => setX(e.target.value)}
          >
            {options.map((item) => (
              <option key={item.key} value={item.analysisKey}>
                {formatAnalysisKeyLabel(item.analysisKey)}
              </option>
            ))}
          </select>
        </Field>
        <Field label="Series Y">
          <select
            className={controlClass}
            value={effectiveY}
            onChange={(e) => setY(e.target.value)}
          >
            {!effectiveY && <option value="">Not selected</option>}
            {options.map((item) => (
              <option key={item.key} value={item.analysisKey}>
                {formatAnalysisKeyLabel(item.analysisKey)}
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
        <Field label="SMA period">
          <Input
            type="number"
            min={1}
            step={1}
            value={smaPeriod}
            onChange={(e) => setSmaPeriod(e.target.value)}
          />
        </Field>
        <Field label="EMA period">
          <Input
            type="number"
            min={1}
            step={1}
            value={emaPeriod}
            onChange={(e) => setEmaPeriod(e.target.value)}
          />
        </Field>
        <Button size="sm" disabled={Boolean(loading) || !canRun} onClick={() => onRun("analysis")}>
          {loading === "analysis" ? "Analysing…" : "Run analysis"}
        </Button>
        <Button
          size="sm"
          variant="outline"
          disabled={Boolean(loading) || !canRun}
          onClick={() => onRun("report")}
        >
          {loading === "report" ? "Downloading…" : "Download PDF"}
        </Button>
      </div>

      {!periodsValid && (
        <p className="text-xs text-destructive">
          SMA and EMA periods must be positive whole numbers.
        </p>
      )}
      {error && (
        <p role="alert" className="text-xs text-destructive">
          {error}
        </p>
      )}

      {result && (
        <AnalysisResults
          result={result}
          smaPeriod={submittedOptions?.smaPeriod ?? Number(smaPeriod)}
          emaPeriod={submittedOptions?.emaPeriod ?? Number(emaPeriod)}
          sigma={submittedOptions?.sigma ?? sigma}
          bucketSeconds={submittedOptions?.bucketSeconds ?? bucketSeconds}
        />
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
