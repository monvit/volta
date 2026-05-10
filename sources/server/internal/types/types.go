package types

type AgentStatus uint8

const (
	AGENT_STATUS_UNKNOWN AgentStatus = iota
	AGENT_STATUS_CONNECTED
	AGENT_STATUS_DISCONNECTED
	AGENT_STATUS_STREAMING
)

func (s AgentStatus) IsValid() bool {
	switch s {
	case AGENT_STATUS_UNKNOWN,
		AGENT_STATUS_CONNECTED,
		AGENT_STATUS_DISCONNECTED,
		AGENT_STATUS_STREAMING:
		return true
	}

	return false
}

type Agent struct {
	Id     string
	Status AgentStatus
	// LastPong time.Time
}
