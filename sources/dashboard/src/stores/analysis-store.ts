import { create } from "zustand";
import { fetchAnalysisReport, runAnalysis } from "@/api/client";
import type { AnalysisRequestPayload, AnalysisResult } from "@/lib/analysis";

interface AnalysisActions {
  run: (
    baseUrl: string,
    agentId: string,
    request: AnalysisRequestPayload,
    operation: "analysis" | "report",
  ) => Promise<void>;
  reset: () => void;
}

interface AnalysisState {
  result: AnalysisResult | null;
  loading: "analysis" | "report" | null;
  error: string | null;
  actions: AnalysisActions;
}

function downloadReport(blob: Blob) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  try {
    link.href = url;
    link.download = "volta-report.pdf";
    document.body.append(link);
    link.click();
  } finally {
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
}

let activeRequest: AbortController | undefined;

const useAnalysisStore = create<AnalysisState>((set) => ({
  result: null,
  loading: null,
  error: null,
  actions: {
    run: async (baseUrl, agentId, request, operation) => {
      if (activeRequest) return;
      const controller = new AbortController();
      activeRequest = controller;
      const { signal } = controller;
      set({ loading: operation, error: null });
      try {
        if (operation === "report") {
          const blob = await fetchAnalysisReport(baseUrl, agentId, request, signal);
          if (!signal.aborted) downloadReport(blob);
        } else {
          const result = await runAnalysis(baseUrl, agentId, request, signal);
          if (!signal.aborted) set({ result });
        }
      } catch (err) {
        if (!signal.aborted) set({ error: err instanceof Error ? err.message : String(err) });
      } finally {
        if (activeRequest === controller) {
          activeRequest = undefined;
          set({ loading: null });
        }
      }
    },
    reset: () => {
      activeRequest?.abort();
      activeRequest = undefined;
      set({ result: null, error: null, loading: null });
    },
  },
}));

export const useAnalysisResult = () => useAnalysisStore((s) => s.result);
export const useAnalysisLoading = () => useAnalysisStore((s) => s.loading);
export const useAnalysisError = () => useAnalysisStore((s) => s.error);
export const useAnalysisActions = () => useAnalysisStore((s) => s.actions);
