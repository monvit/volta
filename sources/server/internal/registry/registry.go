package registry

import (
	"sync"

	session "github.com/monvit/volta/server/internal/session"
)

type AgentRegistry struct {
	sessions sync.Map
}

func (r *AgentRegistry) Add(s *session.AgentSession) {
	r.sessions.Store(s.ID, s)
}

func (r *AgentRegistry) Remove(id string) {
	r.sessions.Delete(id)
}

func (r *AgentRegistry) Get(id string) *session.AgentSession {
	v, exists := r.sessions.Load(id)
	if !exists {
		return nil
	}

	return v.(*session.AgentSession)
}

func (r *AgentRegistry) List() []*session.AgentSession {
	var out []*session.AgentSession
	r.sessions.Range(func(_, v any) bool {
		out = append(out, v.(*session.AgentSession))
		return true
	})

	return out
}
