package controlmessage

import (
	pbt "github.com/monvit/volta/server/pb/types"
	"google.golang.org/protobuf/types/known/timestamppb"
)

type Option func(*pbt.ControlMessage) *pbt.ControlMessage

func New(msgType pbt.MessageType, opts ...Option) *pbt.ControlMessage {
	msg := &pbt.ControlMessage{
		Type:      msgType,
		Timestamp: timestamppb.Now(),
	}

	for _, opt := range opts {
		opt(msg)
	}

	return msg
}

func WithPayload(payload string) Option {
	return func(msg *pbt.ControlMessage) *pbt.ControlMessage {
		if msg == nil {
			msg = &pbt.ControlMessage{}
		}

		msg.Request = &pbt.ControlMessage_Payload{Payload: payload}
		return msg
	}
}

// TODO: with SendRequest
