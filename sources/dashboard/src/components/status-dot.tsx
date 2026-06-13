import { cn } from "@/lib/utils";

export type StatusTone = "online" | "idle" | "warn" | "offline";

const TONE_CLASS: Record<StatusTone, string> = {
  online: "bg-emerald-400",
  idle: "bg-muted-foreground",
  warn: "bg-amber-400",
  offline: "bg-destructive",
};

export function StatusDot({
  tone,
  pulse = false,
  className,
}: {
  tone: StatusTone;
  pulse?: boolean;
  className?: string;
}) {
  return (
    <span className={cn("relative inline-flex size-2 shrink-0", className)}>
      {pulse && (
        <span
          className={cn(
            "absolute inline-flex size-full animate-ping rounded-full opacity-60",
            TONE_CLASS[tone],
          )}
        />
      )}
      <span className={cn("relative inline-flex size-2 rounded-full", TONE_CLASS[tone])} />
    </span>
  );
}
