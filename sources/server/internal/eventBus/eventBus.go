package eventbus

import (
	"sync"

	"github.com/monvit/volta/server/internal/logger"
	"github.com/monvit/volta/server/pb/types"
)

type EventBus struct {
	mu   sync.RWMutex
	subs map[string][]*Subscriber
}

type Subscriber struct {
	ch      chan *types.MetricBatch
	agentID string
}

func (s *Subscriber) Ch() <-chan *types.MetricBatch {
	return s.ch
}

func (b *EventBus) Publish(agentID string, metric *types.MetricBatch) {
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
		ch:      make(chan *types.MetricBatch, 16),
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
