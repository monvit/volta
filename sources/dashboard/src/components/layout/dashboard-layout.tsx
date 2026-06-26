import {
  ArrowsClockwiseIcon,
  CaretLeftIcon,
  ChartLineIcon,
  PulseIcon,
  SquaresFourIcon,
} from "@phosphor-icons/react";
import type { ReactNode } from "react";
import { ModeToggle } from "@/components/mode-toggle";
import { StatusDot, type StatusTone } from "@/components/status-dot";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Separator } from "@/components/ui/separator";
import { AgentSelector } from "@/features/agents/agent-selector";
import { useMetricStream } from "@/features/metrics/use-metric-stream";
import { cn } from "@/lib/utils";
import { useSelectedAgentId } from "@/stores/agent-store";
import { useBaseUrl, useConnectionActions } from "@/stores/connection-store";
import { useStreamStatus, type StreamStatus } from "@/stores/metrics-store";
import { useSidebarCollapsed, useUiActions, useView, type View } from "@/stores/ui-store";

const VIEW_TITLE: Record<View, string> = {
  dashboard: "Live Dashboard",
  detail: "Metric Detail",
};

const STREAM_LABEL: Record<StreamStatus, string> = {
  idle: "Idle",
  connecting: "Connecting",
  open: "Streaming",
  reconnecting: "Reconnecting",
};

const STREAM_TONE: Record<StreamStatus, StatusTone> = {
  idle: "idle",
  connecting: "warn",
  open: "online",
  reconnecting: "warn",
};

function Sidebar() {
  const view = useView();
  const collapsed = useSidebarCollapsed();
  const { setView, toggleSidebar } = useUiActions();
  const baseUrl = useBaseUrl();
  const { disconnect } = useConnectionActions();
  const streamStatus = useStreamStatus();

  return (
    <aside
      className={cn(
        "flex h-dvh shrink-0 flex-col border-r border-border bg-sidebar text-sidebar-foreground transition-[width]",
        collapsed ? "w-14" : "w-60",
      )}
    >
      <div className="flex h-12 items-center gap-2 px-3">
        <ChartLineIcon weight="bold" className="size-5 text-primary" />
        {!collapsed && (
          <span className="font-heading text-sm font-semibold tracking-tight">VOLTA</span>
        )}
        <Button
          variant="ghost"
          size="icon-xs"
          className="ml-auto"
          onClick={toggleSidebar}
          aria-label={collapsed ? "Expand sidebar" : "Collapse sidebar"}
        >
          <CaretLeftIcon className={cn("transition-transform", collapsed && "rotate-180")} />
        </Button>
      </div>

      <Separator />

      <nav className="flex flex-col gap-px p-2">
        <Button
          variant={view === "dashboard" ? "secondary" : "ghost"}
          size="sm"
          className={cn("justify-start", collapsed && "justify-center px-0")}
          onClick={() => setView("dashboard")}
        >
          <SquaresFourIcon className="size-4" />
          {!collapsed && <span>Dashboard</span>}
        </Button>
      </nav>

      {!collapsed && (
        <>
          <Separator />
          <div className="flex min-h-0 flex-1 flex-col gap-2 p-2">
            <span className="px-1 text-[10px] font-medium tracking-widest text-muted-foreground uppercase">
              Agents
            </span>
            <AgentSelector />
          </div>
        </>
      )}

      <div className="mt-auto" />
      <Separator />
      <div className={cn("flex flex-col gap-2 p-3", collapsed && "items-center")}>
        <div className="flex items-center gap-2 text-xs text-muted-foreground">
          <StatusDot tone={STREAM_TONE[streamStatus]} pulse={streamStatus === "open"} />
          {!collapsed && <span>{STREAM_LABEL[streamStatus]}</span>}
        </div>
        {!collapsed && (
          <>
            <span
              className="truncate text-[10px] text-muted-foreground/70"
              title={baseUrl || "same origin"}
            >
              <PulseIcon className="mr-1 inline size-3" />
              {baseUrl || "same origin"}
            </span>
            <Button variant="outline" size="xs" onClick={disconnect}>
              Disconnect
            </Button>
          </>
        )}
      </div>
    </aside>
  );
}

interface DashboardLayoutProps {
  children: ReactNode;
}

export function DashboardLayout({ children }: DashboardLayoutProps) {
  const view = useView();
  const selectedAgentId = useSelectedAgentId();
  const reconnecting = useStreamStatus() === "reconnecting";

  useMetricStream();

  return (
    <div className="flex h-dvh overflow-hidden bg-background text-foreground">
      <Sidebar />
      <div className="flex min-w-0 flex-1 flex-col">
        <header className="flex h-12 shrink-0 items-center gap-3 border-b border-border px-6">
          <h1 className="font-heading text-sm font-medium">{VIEW_TITLE[view]}</h1>
          {selectedAgentId && (
            <Badge variant="outline" className="font-mono">
              {selectedAgentId}
            </Badge>
          )}
          {reconnecting && (
            <span className="flex items-center gap-1.5 text-xs text-amber-400">
              <ArrowsClockwiseIcon className="size-3.5 animate-spin" />
              Connection lost - retrying…
            </span>
          )}
          <div className="ml-auto">
            <ModeToggle />
          </div>
        </header>
        <main className="flex-1 overflow-y-auto p-6">{children}</main>
      </div>
    </div>
  );
}
