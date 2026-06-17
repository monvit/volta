import { create } from "zustand";
import { persist } from "zustand/middleware";
import { fetchAgents } from "@/api/client";
import { agentActions } from "@/stores/agent-store";

export type ConnectionStatus = "idle" | "connecting" | "connected" | "error";

/**
 * Server URL to offer by default on the connection screen. In dev the page is served
 * by Vite (e.g. :5173), which has no API, so default to the standalone server on :8080;
 * in prod the dashboard is served by that server, so default to same-origin ("").
 */
export const DEFAULT_SERVER_URL: string =
  import.meta.env.VITE_SERVER_TARGET ?? (import.meta.env.DEV ? "http://localhost:8080" : "");

/** How many recently-used server URLs to remember and offer on the connection screen. */
const MAX_RECENT = 5;

interface ConnectionActions {
  /** Probe the server via GET /api/agents; on success persist baseUrl, record history, and load agents. */
  connect: (baseUrl?: string) => Promise<boolean>;
  disconnect: () => void;
  forgetRecent: (baseUrl: string) => void;
}

interface ConnectionState {
  /** A full origin like http://host:8080, or "" for same-origin. */
  baseUrl: string;
  /** Most-recent-first list of successfully-connected server URLs. */
  recentUrls: string[];
  status: ConnectionStatus;
  lastError: string | null;
  actions: ConnectionActions;
}

const useConnectionStore = create<ConnectionState>()(
  persist(
    (set, get) => ({
      baseUrl: "",
      recentUrls: [],
      status: "idle",
      lastError: null,
      actions: {
        connect: async (baseUrl) => {
          const url = (baseUrl ?? get().baseUrl).trim();
          set({ status: "connecting", lastError: null });
          try {
            agentActions.setAgents(await fetchAgents(url));
            const recentUrls = [url, ...get().recentUrls.filter((u) => u !== url)].slice(
              0,
              MAX_RECENT,
            );
            set({ baseUrl: url, status: "connected", lastError: null, recentUrls });
            return true;
          } catch (err) {
            set({ status: "error", lastError: err instanceof Error ? err.message : String(err) });
            return false;
          }
        },
        disconnect: () => set({ status: "idle" }),
        forgetRecent: (baseUrl) =>
          set((s) => ({ recentUrls: s.recentUrls.filter((u) => u !== baseUrl) })),
      },
    }),
    {
      name: "volta.connection",
      partialize: (state) => ({ baseUrl: state.baseUrl, recentUrls: state.recentUrls }),
    },
  ),
);

export const useBaseUrl = () => useConnectionStore((s) => s.baseUrl);
export const useRecentConnections = () => useConnectionStore((s) => s.recentUrls);
export const useConnectionStatus = () => useConnectionStore((s) => s.status);
export const useConnectionError = () => useConnectionStore((s) => s.lastError);
export const useIsConnected = () => useConnectionStore((s) => s.status === "connected");
export const useConnectionActions = () => useConnectionStore((s) => s.actions);
