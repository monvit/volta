import { useState, type FormEvent } from "react";
import { ChartLineIcon, SpinnerIcon, WarningCircleIcon } from "@phosphor-icons/react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { RecentUrls } from "@/features/connection/recent-urls";
import {
  DEFAULT_SERVER_URL,
  useBaseUrl,
  useConnectionActions,
  useConnectionError,
  useConnectionStatus,
} from "@/stores/connection-store";

export function ConnectionPage() {
  const status = useConnectionStatus();
  const lastError = useConnectionError();
  const baseUrl = useBaseUrl();
  const { connect } = useConnectionActions();

  const [url, setUrl] = useState(baseUrl || DEFAULT_SERVER_URL);
  const connecting = status === "connecting";

  const onSubmit = (e: FormEvent) => {
    e.preventDefault();
    void connect(url.trim());
  };

  return (
    <div className="flex min-h-dvh items-center justify-center bg-background p-6 text-foreground">
      <div className="flex w-full max-w-sm flex-col gap-5">
        <form onSubmit={onSubmit} className="flex flex-col gap-5">
          <div className="flex items-center gap-2">
            <ChartLineIcon weight="bold" className="size-6 text-primary" />
            <span className="font-heading text-lg font-semibold tracking-tight">VOLTA</span>
          </div>

          <div className="flex flex-col gap-1">
            <h1 className="font-heading text-sm font-medium">Connect to a server</h1>
            <p className="text-xs text-muted-foreground">
              Enter the Volta server URL, or leave blank to use the same origin.
            </p>
          </div>

          <div className="flex flex-col gap-2">
            <Label htmlFor="server-url" className="text-xs">
              Server URL
            </Label>
            <Input
              id="server-url"
              value={url}
              onChange={(e) => setUrl(e.target.value)}
              placeholder="http://localhost:8080"
              autoFocus
              spellCheck={false}
              autoComplete="off"
            />
          </div>

          {status === "error" && lastError && (
            <div className="flex items-start gap-2 bg-destructive/10 p-2 text-xs text-destructive">
              <WarningCircleIcon className="mt-0.5 size-3.5 shrink-0" />
              <span>{lastError}</span>
            </div>
          )}

          <Button type="submit" disabled={connecting}>
            {connecting && <SpinnerIcon className="animate-spin" />}
            {connecting ? "Connecting…" : "Connect"}
          </Button>
        </form>

        <RecentUrls />
      </div>
    </div>
  );
}
