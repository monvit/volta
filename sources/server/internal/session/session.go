package session

import (
	"context"
	"errors"
	"fmt"
	"io"

	controlmessage "github.com/monvit/volta/sources/server/internal/controlMessage"
	log "github.com/monvit/volta/sources/server/internal/logger"
	pb "github.com/monvit/volta/sources/server/pb"
	pbt "github.com/monvit/volta/sources/server/pb/types"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

type AgentSession struct {
	ID     string
	stream pb.VoltaCollector_ConnectServer
	sendCh chan *pbt.ControlMessage
	ctx    context.Context
	cancel context.CancelFunc
}

func New(id string, stream pb.VoltaCollector_ConnectServer) *AgentSession {
	ctx, cancel := context.WithCancel(stream.Context())
	return &AgentSession{
		ID:     id,
		stream: stream,
		sendCh: make(chan *pbt.ControlMessage),
		ctx:    ctx,
		cancel: cancel,
	}
}

func (s *AgentSession) Run() {
	defer s.cancel()

	eg, _ := errgroup.WithContext(s.stream.Context())

	eg.Go(s.recv)
	eg.Go(s.send)

	if err := eg.Wait(); err != nil {
		log.Error("Session[%v]: %v", s.ID, err)
	}
}

func (s *AgentSession) Send(msg *pbt.ControlMessage) error {
	// TODO: aliases for control messages, so that not every message could be sent?
	select {
	case s.sendCh <- msg:
		return nil
	case <-s.ctx.Done():
		return fmt.Errorf("agent %s disconnected", s.ID)
	}
}

func (s *AgentSession) send() error {
	for {
		select {
		case msg := <-s.sendCh:
			if err := s.stream.Send(msg); err != nil {
				return err
			}
		case <-s.ctx.Done():
			return nil
		}
	}
}

func (s *AgentSession) recv() error {
	for {
		msg, err := s.stream.Recv()
		if err != nil {
			switch status.Code(err) {
			// TODO: handle unavailable
			case codes.Canceled, codes.Unavailable:
				log.Info("agent %v disconnected", s.ID)
			default:
				if !errors.Is(err, io.EOF) {
					log.Error("agent %v recv error: %v", s.ID, err)
				}
			}
			return err
		}

		s.handleMessage(msg)
	}
}

func (s *AgentSession) handleMessage(msg *pbt.ControlMessage) error {
	switch msg.Type {
	case pbt.MessageType_MESSAGE_OK:
	case pbt.MessageType_MESSAGE_ERROR:
		log.Error("(Connect) received error from agent %v: %v", s.ID, msg.GetPayload())
	case pbt.MessageType_MESSAGE_PING:
		s.sendCh <- controlmessage.New(pbt.MessageType_MESSAGE_PONG)
	case pbt.MessageType_MESSAGE_PONG:
		// TODO: measure latency? monitor ping responses
	case pbt.MessageType_MESSAGE_SEND_DATA, pbt.MessageType_MESSAGE_STREAM_DATA:
		s.sendCh <- controlmessage.New(pbt.MessageType_MESSAGE_ERROR, controlmessage.WithPayload(fmt.Sprintf("%v is only for agents", msg.Type.String())))
	case pbt.MessageType_MESSAGE_ID:
		// TODO
	case pbt.MessageType_MESSAGE_UNKNOWN:
		log.Warn("(Connect) agent %v sent UNKNOWN message type", s.ID)
	}

	return nil
}
