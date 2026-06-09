package hub

import (
	"net/http"
	"sync"

	"github.com/gorilla/websocket"
	eventbus "github.com/monvit/volta/server/internal/eventBus"
	"google.golang.org/protobuf/proto"
)

type WSClient struct {
	conn    *websocket.Conn
	agentID string
	sub     *eventbus.Subscriber
	send    chan []byte
}

type WSHub struct {
	bus      *eventbus.EventBus
	upgrader websocket.Upgrader
	register chan *WSClient
	remove   chan *WSClient
	clients  map[*WSClient]struct{}
	shutdown chan struct{}
	mu       sync.Mutex
}

func New(bus *eventbus.EventBus) *WSHub {
	return &WSHub{
		bus: bus,
		upgrader: websocket.Upgrader{
			ReadBufferSize:  1024,
			WriteBufferSize: 1024,
			CheckOrigin: func(r *http.Request) bool {
				return true
			},
		},
		register: make(chan *WSClient, 16),
		remove:   make(chan *WSClient, 16),
		clients:  make(map[*WSClient]struct{}),
		shutdown: make(chan struct{}),
	}
}

func (h *WSHub) Run() {
	for {
		select {
		case c := <-h.register:
			h.mu.Lock()
			h.clients[c] = struct{}{}
			h.mu.Unlock()
			go h.pumpToClient(c)

		case c := <-h.remove:
			h.bus.Unsubscribe(c.sub)
			h.mu.Lock()
			delete(h.clients, c)
			h.mu.Unlock()
		case <-h.shutdown:
			h.mu.Lock()
			for c := range h.clients {
				h.bus.Unsubscribe(c.sub)
				c.conn.Close()
				delete(h.clients, c)
			}
			h.mu.Unlock()
			return
		}
	}
}

func (h *WSHub) Close() {
	close(h.shutdown)
}

func (h *WSHub) pumpToClient(c *WSClient) {
	defer func() { h.remove <- c }()
	for batch := range c.sub.Ch() {
		data, _ := proto.Marshal(batch)
		c.conn.WriteMessage(websocket.BinaryMessage, data)
	}
}

func (h *WSHub) HandleWS(w http.ResponseWriter, r *http.Request) {
	agentID := r.URL.Query().Get("agent_id")
	conn, err := h.upgrader.Upgrade(w, r, nil)
	if err != nil {
		return
	}
	sub := h.bus.Subscribe(agentID)
	client := &WSClient{conn: conn, agentID: agentID, sub: sub}
	h.register <- client
}
