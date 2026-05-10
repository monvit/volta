package broker

import (
	"slices"
	"sync"

	pbt "github.com/monvit/volta/sources/server/pb/types"
)

type Metric pbt.Metric

type Broker struct {
	mu          sync.RWMutex
	subscribers map[string][]chan *pbt.Metric
}

func New() *Broker {
	return &Broker{subscribers: make(map[string][]chan *pbt.Metric)}
}

func (b *Broker) Publish(agentId string, m *pbt.Metric) {
	b.mu.Lock()
	defer b.mu.Unlock()

	for _, ch := range b.subscribers[agentId] {
		// TODO: handle if chan is full
		// for now discard metric
		select {
		case ch <- m:
		default:
		}
	}
}

func (b *Broker) Subscribe(agentId string) chan *pbt.Metric {
	ch := make(chan *pbt.Metric)

	b.mu.Lock()
	defer b.mu.Unlock()

	b.subscribers[agentId] = append(b.subscribers[agentId], ch)

	return ch
}

func (b *Broker) Unsubscribe(agentId string, ch chan *pbt.Metric) {
	b.mu.Lock()
	defer b.mu.Unlock()

	subs := b.subscribers[agentId]
	for i, s := range subs {
		if s == ch {
			subs = slices.Delete(subs, i, i+1)
			break
		}
	}

	close(ch)
}
