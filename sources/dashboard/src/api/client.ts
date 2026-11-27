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
  const res = await postAnalysis(baseUrl, agentId, request, "analyze", signal);
  return (await res.json()) as AnalysisResult;
}

export async function fetchAnalysisReport(
  baseUrl: string,
  agentId: string,
  request: AnalysisRequestPayload,
  signal?: AbortSignal,
) {
  const res = await postAnalysis(baseUrl, agentId, request, "report", signal);
  if (res.headers.get("content-type")?.split(";")[0]?.trim().toLowerCase() !== "application/pdf") {
    throw new Error("Server did not return a PDF report");
  }
  return res.blob();
}

async function postAnalysis(
  baseUrl: string,
  agentId: string,
  request: AnalysisRequestPayload,
  endpoint: "analyze" | "report",
  signal?: AbortSignal,
) {
  const url = apiUrl(baseUrl, `/api/agents/${encodeURIComponent(agentId)}/${endpoint}`);
  const res = await fetch(url, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Accept: endpoint === "report" ? "application/pdf" : "application/json",
    },
    body: JSON.stringify(request),
    signal,
  });

  if (!res.ok) {
    const detail = (await res.text()).trim();
    const operation = endpoint === "report" ? "PDF download" : "Analysis";
    throw new Error(detail || `${operation} failed (${res.status})`);
  }

  return res;
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
