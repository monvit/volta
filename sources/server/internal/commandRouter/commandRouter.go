package commandrouter

import (
	"fmt"

	controlmessage "github.com/monvit/volta/sources/server/internal/controlMessage"
	"github.com/monvit/volta/sources/server/internal/registry"
	pbt "github.com/monvit/volta/sources/server/pb/types"
)

const (
	ErrAgentNotFound = "agent not found"
)

type CommandRouter struct {
	registry *registry.AgentRegistry
}

func New(registry *registry.AgentRegistry) *CommandRouter {
	return &CommandRouter{
		registry: registry,
	}
}

func (r *CommandRouter) StreamData(agentID string) error {
	sess := r.registry.Get(agentID)
	if sess == nil {
		return fmt.Errorf("StreamData: %s", ErrAgentNotFound)
	}

	if err := sess.Send(controlmessage.New(pbt.MessageType_MESSAGE_STREAM_DATA)); err != nil {
		return err
	}

	return nil
}

func (r *CommandRouter) SendData(agentID string) error {
	return fmt.Errorf("SendData not implemented")
}
