package server

import (
	pbt "github.com/monvit/volta/sources/server/pb/types"
	"google.golang.org/protobuf/types/known/timestamppb"
)

type Option func(*pbt.ControlMessage)

func CreateControlMessage(msgType pbt.MessageType, opts ...Option) *pbt.ControlMessage {
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
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_Payload{
			Payload: payload,
		}
	}
}

func WithSendDataRequest(req *pbt.SendDataRequest) Option {
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_SendDataRequest{
			SendDataRequest: req,
		}
	}
}

// TODO: ranges may have to be moved to separate messages
func WithCount(count uint32) Option {
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_SendDataRequest{
			SendDataRequest: &pbt.SendDataRequest{
				Payload: &pbt.SendDataRequest_Count{
					Count: count,
				},
			},
		}
	}
}

func WithRangeDuration(duration *pbt.RangeDuration) Option {
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_SendDataRequest{
			SendDataRequest: &pbt.SendDataRequest{
				Payload: &pbt.SendDataRequest_RangeDuration{
					RangeDuration: duration,
				},
			},
		}
	}
}

func WithRangeTime(rangeTime *pbt.RangeTime) Option {
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_SendDataRequest{
			SendDataRequest: &pbt.SendDataRequest{
				Payload: &pbt.SendDataRequest_RangeTime{
					RangeTime: rangeTime,
				},
			},
		}
	}
}

func WithRangeCountFrom(rangeCountFrom *pbt.RangeCountFrom) Option {
	return func(msg *pbt.ControlMessage) {
		msg.Request = &pbt.ControlMessage_SendDataRequest{
			SendDataRequest: &pbt.SendDataRequest{
				Payload: &pbt.SendDataRequest_RangeCountFrom{
					RangeCountFrom: rangeCountFrom,
				},
			},
		}
	}
}
