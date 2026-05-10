package gateway

import (
	"encoding/json"
	"fmt"
	"net/http"

	"github.com/monvit/volta/sources/server/internal/broker"
	t "github.com/monvit/volta/sources/server/internal/types"
)

type AgentStore interface {
	ListAgents() []t.Agent
}

type MetricRequester interface {
	RequestStreamMetrics(agentId string) error
	RequestSendMetrics(agentId string) error
}

type Gateway struct {
	broker    *broker.Broker
	store     AgentStore
	requester MetricRequester
}

func New(broker *broker.Broker, store AgentStore, requester MetricRequester) *Gateway {
	return &Gateway{
		broker:    broker,
		store:     store,
		requester: requester,
	}
}

func (g *Gateway) Run(addr string, port uint) error {
	mux := http.NewServeMux()

	mux.HandleFunc("GET /api/v1/agents", g.listAgents)

	return http.ListenAndServe(fmt.Sprintf("%s:%d", addr, port), mux)
}

func (g *Gateway) listAgents(w http.ResponseWriter, r *http.Request) {
	agents := g.store.ListAgents()
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(agents)
}
