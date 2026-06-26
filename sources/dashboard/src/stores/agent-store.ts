import { create } from "zustand";
import type { Agent } from "@/api/client";

interface AgentActions {
  /** Replaces the agent list, keeping the current selection if still present, else picking the first. */
  setAgents: (agents: Agent[]) => void;
  selectAgent: (id: string | null) => void;
}

interface AgentState {
  agents: Agent[];
  selectedAgentId: string | null;
  actions: AgentActions;
}

const useAgentStore = create<AgentState>((set, get) => ({
  agents: [],
  selectedAgentId: null,
  actions: {
    setAgents: (agents) => {
      const current = get().selectedAgentId;
      const keep = current !== null && agents.some((a) => a.id === current);
      set({ agents, selectedAgentId: keep ? current : (agents[0]?.id ?? null) });
    },
    selectAgent: (id) => set({ selectedAgentId: id }),
  },
}));

export const useAgents = () => useAgentStore((s) => s.agents);
export const useSelectedAgentId = () => useAgentStore((s) => s.selectedAgentId);
export const useAgentActions = () => useAgentStore((s) => s.actions);

/** Stable action handle for non-React callers (connection flow, stream hook). */
export const agentActions = useAgentStore.getState().actions;
