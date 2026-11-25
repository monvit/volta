import { fromJson } from "@bufbuild/protobuf";
import { ListAgentsResponseSchema, type Agent } from "@/proto/gen/types/agent_pb";
import type { AnalysisRequestPayload, AnalysisResult } from "@/lib/analysis";

export type { Agent };

const trimTrailingSlash = (url: string) => url.replace(/\/+$/, "");

export function apiUrl(baseUrl: string, path: string) {
  return `${trimTrailingSlash(baseUrl)}${path}`;
}

export function wsUrl(baseUrl: string, agentId: string) {
  const origin = trimTrailingSlash(baseUrl) || window.location.origin;

  const url = new URL(origin);
  url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
  url.pathname = "/ws";
  url.search = `agent_id=${encodeURIComponent(agentId)}`;

  return url.toString();
}

/** List connected agents. */
export async function fetchAgents(baseUrl: string, signal?: AbortSignal) {
  const url = apiUrl(baseUrl, "/api/agents");
  const res = await fetch(url, {
    headers: { Accept: "application/json" },
    signal,
  });

  if (!res.ok) {
    throw new Error(`Server returned ${res.status} ${res.statusText}`);
  }

  const json = await res.json();
  const listAgentsResponse = fromJson(ListAgentsResponseSchema, json, {
    ignoreUnknownFields: true,
  });
  return listAgentsResponse.agents;
}

/** Analyse stored metrics for an agent through the server's Analysis sidecar. */
export async function runAnalysis(
  baseUrl: string,
  agentId: string,
  request: AnalysisRequestPayload,
  signal?: AbortSignal,
) {
  const url = apiUrl(baseUrl, `/api/agents/${encodeURIComponent(agentId)}/analyze`);
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify(request),
    signal,
  });

  if (!res.ok) {
    const detail = (await res.text()).trim();
    throw new Error(detail || `Analysis failed (${res.status})`);
  }

  return (await res.json()) as AnalysisResult;
}

/** Ask an agent to begin streaming.
 * Subscribe to /ws first so no opening batches are missed.
 **/
export async function startStream(baseUrl: string, agentId: string) {
  const url = apiUrl(baseUrl, `/api/agents/${encodeURIComponent(agentId)}/stream`);

  const res = await fetch(url, {
    method: "POST",
  });

  if (!res.ok) {
    throw new Error(`Failed to start stream (${res.status})`);
  }
}
