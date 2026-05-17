package eventbus

import (
	"sync"

	"github.com/monvit/volta/sources/server/internal/logger"
	"github.com/monvit/volta/sources/server/pb/types"
)

type EventBus struct {
	mu   sync.RWMutex
	subs map[string][]*Subscriber
}

type Subscriber struct {
	ch      chan *types.Metric
	agentID string
}

func (s *Subscriber) Ch() <-chan *types.Metric {
	return s.ch
}

func (b *EventBus) Publish(agentID string, metric *types.Metric) {
	b.mu.RLock()
	subs := b.subs[agentID]
	b.mu.RUnlock()
	for _, sub := range subs {
		select {
		case sub.ch <- metric:
		default:
			logger.Warn("subscriber lagging, dropping metric from agent %s", agentID)
		}
	}
}

func (b *EventBus) Subscribe(agentID string) *Subscriber {
	sub := &Subscriber{
		ch:      make(chan *types.Metric, 16),
		agentID: agentID,
	}
	b.mu.Lock()
	b.subs[agentID] = append(b.subs[agentID], sub)
	b.mu.Unlock()
	return sub
}

func (b *EventBus) Unsubscribe(sub *Subscriber) {
	b.mu.Lock()
	subs := b.subs[sub.agentID]
	for i, s := range subs {
		if s == sub {
			b.subs[sub.agentID] = append(subs[:i], subs[i+1:]...)
			break
		}
	}
	b.mu.Unlock()
	close(sub.ch)
}

func New() *EventBus {
	return &EventBus{
		subs: make(map[string][]*Subscriber),
	}
}
