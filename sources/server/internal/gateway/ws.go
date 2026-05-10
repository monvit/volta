package gateway

import (
	"net/http"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool { return true },
}

type MetricMessage struct {
	AgentID   string  `json:"agent_id"`
	Name      string  `json:"name"`
	Value     float64 `json:"value"`
	Timestamp int64   `json:"timestamp"`
}

func (g *Gateway) wsMetrics(w http.ResponseWriter, r *http.Request) {
	agentID := r.PathValue("agent_id")

	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	defer conn.Close()

	// subskrybuj metryki dla tego agenta z brokera
	ch := g.broker.Subscribe(agentID)
	defer g.broker.Unsubscribe(agentID, ch)

	for {
		select {
		case metric, ok := <-ch:
			if !ok {
				return
			}
			if err := conn.WriteJSON(metric); err != nil {
				return
			}
		case <-r.Context().Done():
			return
		}
	}
}
