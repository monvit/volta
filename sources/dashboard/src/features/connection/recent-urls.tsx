import { ClockCounterClockwiseIcon, XIcon } from "@phosphor-icons/react";
import { Button } from "@/components/ui/button";
import {
  useConnectionActions,
  useConnectionStatus,
  useRecentConnections,
} from "@/stores/connection-store";
import { cn } from "@/lib/utils";

function formatUrl(url: string) {
  return url || "Same origin";
}

export function RecentUrls() {
  const recent = useRecentConnections();
  const status = useConnectionStatus();
  const { connect, forgetRecent } = useConnectionActions();
  const connecting = status === "connecting";

  if (recent.length === 0) {
    return null;
  }

  return (
    <div className="flex flex-col gap-1.5">
      <span className="flex items-center gap-1.5 text-[10px] font-medium tracking-widest text-muted-foreground uppercase">
        <ClockCounterClockwiseIcon className="size-3" />
        Recent
      </span>
      <ul className="flex flex-col gap-px">
        {recent.map((url) => {
          const label = formatUrl(url);

          return (
            <li key={url} className="group flex items-center">
              <button
                type="button"
                disabled={connecting}
                onClick={() => void connect(url)}
                className={cn(
                  "flex-1 truncate px-2 py-1.5 text-left font-mono text-xs text-muted-foreground transition-colors",
                  "hover:bg-muted/50 hover:text-foreground disabled:pointer-events-none disabled:opacity-50",
                )}
                title={label}
              >
                {label}
              </button>
              <Button
                variant="ghost"
                size="icon-xs"
                aria-label={`Forget ${label}`}
                onClick={() => forgetRecent(url)}
                className="opacity-0 transition-opacity group-hover:opacity-100"
              >
                <XIcon className="size-3" />
              </Button>
            </li>
          );
        })}
      </ul>
    </div>
  );
}
