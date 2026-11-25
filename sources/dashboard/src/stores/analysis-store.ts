import { create } from "zustand";
import { runAnalysis } from "@/api/client";
import type { AnalysisRequestPayload, AnalysisResult } from "@/lib/analysis";

interface AnalysisActions {
  run: (
    baseUrl: string,
    agentId: string,
    request: AnalysisRequestPayload,
  ) => Promise<void>;
  reset: () => void;
}

interface AnalysisState {
  result: AnalysisResult | null;
  loading: boolean;
  error: string | null;
  actions: AnalysisActions;
}

const useAnalysisStore = create<AnalysisState>((set) => ({
  result: null,
  loading: false,
  error: null,
  actions: {
    run: async (baseUrl, agentId, request) => {
      set({ loading: true, error: null });
      try {
        set({ result: await runAnalysis(baseUrl, agentId, request), loading: false });
      } catch (err) {
        set({
          error: err instanceof Error ? err.message : String(err),
          loading: false,
        });
      }
    },
    reset: () => set({ result: null, error: null }),
  },
}));

export const useAnalysisResult = () => useAnalysisStore((s) => s.result);
export const useAnalysisLoading = () => useAnalysisStore((s) => s.loading);
export const useAnalysisError = () => useAnalysisStore((s) => s.error);
export const useAnalysisActions = () => useAnalysisStore((s) => s.actions);
