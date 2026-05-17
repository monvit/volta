package server

import (
	"context"
	"time"

	"github.com/google/uuid"
	controlmessage "github.com/monvit/volta/sources/server/internal/controlMessage"
	eventbus "github.com/monvit/volta/sources/server/internal/eventBus"
	log "github.com/monvit/volta/sources/server/internal/logger"
	"github.com/monvit/volta/sources/server/internal/registry"
	"github.com/monvit/volta/sources/server/internal/session"
	"github.com/monvit/volta/sources/server/pb"
	pbt "github.com/monvit/volta/sources/server/pb/types"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

const (
	HANDSHAKE_TIMEOUT = 5 * time.Second
)

type GRPCServer struct {
	pb.UnimplementedVoltaCollectorServer

	registry *registry.AgentRegistry
	bus      *eventbus.EventBus
}

func New(registry *registry.AgentRegistry, bus *eventbus.EventBus) *grpc.Server {
	srv := &GRPCServer{
		registry: registry,
		bus:      bus,
	}

	grpcSrv := grpc.NewServer(
		grpc.ChainUnaryInterceptor( /* np. logging */ ),
		grpc.ChainStreamInterceptor( /* np. logging */ ),
	)
	pb.RegisterVoltaCollectorServer(grpcSrv, srv)
	return grpcSrv
}

/* Connect RPC */
func (s *GRPCServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	id, err := s.handshake(stream)
	if err != nil {
		log.Error("(Connect) Failed to handshake with agent: %v", err)
		return err
	}

	sess := session.New(id, stream)
	s.registry.Add(sess)
	sess.Run()
	return nil
}

func (s *GRPCServer) handshake(stream pb.VoltaCollector_ConnectServer) (string, error) {
	ctx, cancel := context.WithTimeout(stream.Context(), HANDSHAKE_TIMEOUT)
	defer cancel()

	msgCh := make(chan *pbt.ControlMessage, 1)
	errCh := make(chan error, 1)

	go func() {
		msg, err := stream.Recv()
		if err != nil {
			errCh <- err
			return
		}
		msgCh <- msg
	}()

	select {
	case <-ctx.Done():
		return "", status.Error(codes.DeadlineExceeded, "handshake timeout")
	case err := <-errCh:
		return "", err
	case msg := <-msgCh:
		if msg.GetType() != pbt.MessageType_MESSAGE_ID {
			return "", status.Errorf(codes.InvalidArgument,
				"expected MESSAGE_ID, got %v", msg.GetType())
		}

		md, _ := metadata.FromIncomingContext(stream.Context())
		ids := md.Get("agent-id")

		agentID := ""
		if len(ids) > 0 && ids[0] != "" {
			if s.registry.Get(ids[0]) != nil {
				return "", status.Errorf(codes.AlreadyExists,
					"agent with ID %v already exists", ids[0])
			}

			agentID = ids[0]
		} else {
			agentID = uuid.New().String()

			reply := controlmessage.New(pbt.MessageType_MESSAGE_ID, controlmessage.WithPayload(agentID))

			if err := stream.Send(reply); err != nil {
				return "", status.Errorf(codes.Internal,
					"failed to send agent ID: %v", err)
			}
		}

		return agentID, nil
	}
}

/* Connect RPC */

func (s *GRPCServer) StreamData(stream pb.VoltaCollector_StreamDataServer) error {
	return status.Error(codes.Unimplemented, "StreamData is not implemented yet")
}
