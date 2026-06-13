import { useState } from "react";
import { MagnifyingGlassIcon } from "@phosphor-icons/react";
import { Input } from "@/components/ui/input";
import { StatusDot } from "@/components/status-dot";
import { AgentStatus } from "@/proto/gen/types/agent_pb";
import { useAgentActions, useAgents, useSelectedAgentId } from "@/stores/agent-store";
import { cn } from "@/lib/utils";

function toneForStatus(status: AgentStatus) {
  switch (status) {
    case AgentStatus.CONNECTED:
    case AgentStatus.STREAMING:
      return "online";
    case AgentStatus.DISCONNECTED:
      return "offline";
    default:
      return "idle";
  }
}

export function AgentSelector() {
  const agents = useAgents();
  const selectedId = useSelectedAgentId();
  const { selectAgent } = useAgentActions();
  const [query, setQuery] = useState("");

  const filtered = agents.filter((a) => a.id.toLowerCase().includes(query.trim().toLowerCase()));

  return (
    <div className="flex min-h-0 flex-col gap-2">
      <div className="relative">
        <MagnifyingGlassIcon className="pointer-events-none absolute top-1/2 left-2 size-3.5 -translate-y-1/2 text-muted-foreground" />
        <Input
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Filter agents"
          className="pl-7"
          aria-label="Filter agents"
        />
      </div>
      <div className="-mr-1 flex min-h-0 flex-col gap-px overflow-y-auto pr-1">
        {agents.length === 0 && (
          <p className="px-2 py-3 text-xs text-muted-foreground">No agents connected.</p>
        )}
        {agents.length > 0 && filtered.length === 0 && (
          <p className="px-2 py-3 text-xs text-muted-foreground">No match for “{query}”.</p>
        )}
        {filtered.map((agent) => {
          const active = agent.id === selectedId;
          return (
            <button
              key={agent.id}
              type="button"
              onClick={() => selectAgent(agent.id)}
              className={cn(
                "flex items-center gap-2 px-2 py-1.5 text-left text-xs transition-colors",
                active
                  ? "bg-muted text-foreground"
                  : "text-muted-foreground hover:bg-muted/50 hover:text-foreground",
              )}
            >
              <StatusDot tone={toneForStatus(agent.status)} />
              <span className="truncate font-mono" title={agent.id}>
                {agent.id}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
