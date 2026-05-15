package eventbus

import (
	"sync"

	pb "github.com/monvit/volta/sources/server/pb"
)

type Subscriber struct {
	ch     chan *pb.MetricsBatch
	filter string // "" = wszystkie agenty
}

type EventBus struct {
	mu   sync.RWMutex
	subs map[string][]*Subscriber // agentID → subskrybenci
}

func NewEventBus() *EventBus {
	return &EventBus{subs: make(map[string][]*Subscriber)}
}

func (b *EventBus) Subscribe(agentID string, bufSize int) *Subscriber {
	sub := &Subscriber{
		ch:     make(chan *pb.MetricsBatch, bufSize),
		filter: agentID,
	}
	b.mu.Lock()
	b.subs[agentID] = append(b.subs[agentID], sub)
	b.mu.Unlock()
	return sub
}

func (b *EventBus) Unsubscribe(agentID string, sub *Subscriber) {
	b.mu.Lock()
	defer b.mu.Unlock()
	list := b.subs[agentID]
	for i, s := range list {
		if s == sub {
			b.subs[agentID] = append(list[:i], list[i+1:]...)
			return
		}
	}
}

func (b *EventBus) Publish(agentID string, batch *pb.MetricsBatch) {
	b.mu.RLock()
	subs := b.subs[agentID]
	b.mu.RUnlock()
	for _, sub := range subs {
		select {
		case sub.ch <- batch:
		default:
			// konsument nie nadąża — drop (lub log)
		}
	}
}

func (sub *Subscriber) Ch() <-chan *pb.MetricsBatch {
	return sub.ch
}
