import { create } from "zustand";
import { persist } from "zustand/middleware";

export type View = "dashboard" | "detail";

interface UiActions {
  setView: (view: View) => void;
  openDetail: (seriesKey: string) => void;
  toggleSidebar: () => void;
}

interface UiState {
  view: View;
  /** Series key (see `lib/metrics.ts#seriesKey`) shown on the detail page. */
  detailSeriesKey: string | null;
  sidebarCollapsed: boolean;
  actions: UiActions;
}

const useUiStore = create<UiState>()(
  persist(
    (set) => ({
      view: "dashboard",
      detailSeriesKey: null,
      sidebarCollapsed: false,
      actions: {
        setView: (view) => set({ view }),
        openDetail: (seriesKey) => set({ view: "detail", detailSeriesKey: seriesKey }),
        toggleSidebar: () => set((s) => ({ sidebarCollapsed: !s.sidebarCollapsed })),
      },
    }),
    {
      name: "volta.ui",
      partialize: (state) => ({ sidebarCollapsed: state.sidebarCollapsed }),
    },
  ),
);

export const useView = () => useUiStore((s) => s.view);
export const useDetailSeriesKey = () => useUiStore((s) => s.detailSeriesKey);
export const useSidebarCollapsed = () => useUiStore((s) => s.sidebarCollapsed);
export const useUiActions = () => useUiStore((s) => s.actions);
