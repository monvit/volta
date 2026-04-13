package server

import (
	pb "github.com/monvit/volta/sources/server/pb"
	"google.golang.org/protobuf/types/known/timestamppb"
)

type Option func(*pb.ControlMessage)

func CreateControlMessage(msgType pb.MessageType, opts ...Option) *pb.ControlMessage {
	msg := &pb.ControlMessage{
		Type:      msgType,
		Timestamp: timestamppb.Now(),
	}
	for _, opt := range opts {
		opt(msg)
	}
	return msg
}

func WithError(err string) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_Error{
			Error: err,
		}
	}
}

func WithSendDataRequest(req *pb.SendDataRequest) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_SendDataRequest{
			SendDataRequest: req,
		}
	}
}

// TODO: ranges may have to be moved to separate messages
func WithCount(count uint32) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_SendDataRequest{
			SendDataRequest: &pb.SendDataRequest{
				Payload: &pb.SendDataRequest_Count{
					Count: count,
				},
			},
		}
	}
}

func WithRangeDuration(duration *pb.RangeDuration) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_SendDataRequest{
			SendDataRequest: &pb.SendDataRequest{
				Payload: &pb.SendDataRequest_RangeDuration{
					RangeDuration: duration,
				},
			},
		}
	}
}

func WithRangeTime(rangeTime *pb.RangeTime) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_SendDataRequest{
			SendDataRequest: &pb.SendDataRequest{
				Payload: &pb.SendDataRequest_RangeTime{
					RangeTime: rangeTime,
				},
			},
		}
	}
}

func WithRangeCountFrom(rangeCountFrom *pb.RangeCountFrom) Option {
	return func(msg *pb.ControlMessage) {
		msg.Request = &pb.ControlMessage_SendDataRequest{
			SendDataRequest: &pb.SendDataRequest{
				Payload: &pb.SendDataRequest_RangeCountFrom{
					RangeCountFrom: rangeCountFrom,
				},
			},
		}
	}
}
