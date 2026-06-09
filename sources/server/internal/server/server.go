package server

import (
	"context"
	"runtime/debug"
	"time"

	"github.com/google/uuid"
	controlmessage "github.com/monvit/volta/server/internal/controlMessage"
	eventbus "github.com/monvit/volta/server/internal/eventBus"
	"github.com/monvit/volta/server/internal/logger"
	"github.com/monvit/volta/server/internal/registry"
	"github.com/monvit/volta/server/internal/session"
	"github.com/monvit/volta/server/pb"
	"github.com/monvit/volta/server/pb/types"
	pbt "github.com/monvit/volta/server/pb/types"
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
		grpc.StreamInterceptor(recoveryInterceptor),
	)
	pb.RegisterVoltaCollectorServer(grpcSrv, srv)
	return grpcSrv
}

func recoveryInterceptor(
	srv any,
	ss grpc.ServerStream,
	info *grpc.StreamServerInfo,
	handler grpc.StreamHandler,
) error {
	defer func() {
		if r := recover(); r != nil {
			logger.Error("panic recovered in stream %s: %v\n%s",
				info.FullMethod, r, debug.Stack())
		}
	}()

	return handler(srv, ss)
}

/* Connect RPC */
func (s *GRPCServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	id, err := s.handshake(stream)
	if err != nil {
		logger.Error("(Connect) Failed to handshake with agent: %v", err)
		return err
	}

	logger.Debug("(Connect) Agent %v connected", id)

	sess := session.New(id, stream)
	s.registry.Add(sess)
	err = sess.Run()
	if err != nil {
		logger.Error("(Connect) Session error for agent %v: %v", id, err)
		s.registry.Remove(id)
		return err
	}

	s.registry.Remove(id)
	logger.Info("Agent %v disconnected", id)

	return nil
}

func getIdFromContext(ctx context.Context) (string, error) {
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", status.Error(codes.InvalidArgument, "missing metadata")
	}

	ids := md.Get("agent-id")
	if len(ids) == 0 {
		return "", status.Error(codes.InvalidArgument, "missing agent-id in metadata")
	}

	return ids[0], nil
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

		id, err := getIdFromContext(stream.Context())
		if err != nil {
			return "", err
		}

		if id != "" {
			if s.registry.Get(id) != nil {
				return "", status.Errorf(codes.AlreadyExists,
					"agent with ID %v already exists", id)
			}
		} else {
			id = uuid.New().String()

			reply := controlmessage.New(pbt.MessageType_MESSAGE_ID, controlmessage.WithPayload(id))

			if err := stream.Send(reply); err != nil {
				return "", status.Errorf(codes.Internal,
					"failed to send agent ID: %v", err)
			}
		}

		return id, nil
	}
}

/* Connect RPC */

func (s *GRPCServer) StreamMetrics(stream pb.VoltaCollector_StreamMetricsServer) error {
	id, err := getIdFromContext(stream.Context())
	if err != nil {
		logger.Error("(StreamMetrics) Failed to get agent ID from context: %v", err)
		return err
	}

	sess := s.registry.Get(id)
	if sess == nil {
		logger.Error("(StreamMetrics) No session found for agent ID: %v", id)
		return status.Errorf(codes.NotFound, "no session found for agent ID: %v", id)
	}

	// NOTE: consider separating receiving and acknowledging metrics into separate goroutines to not block receiving
	for {
		batch, err := stream.Recv()
		if err != nil {
			logger.Error("(StreamMetrics) Failed to receive message from agent %v: %v", id, err)
			return err
		}

		// TODO: validate batch
		s.bus.Publish(id, batch)

		ack := &types.BatchAck{
			BatchId:         batch.Id,
			SamplesReceived: uint64(len(batch.Values)),
		}

		if err := stream.Send(ack); err != nil {
			logger.Error("(StreamMetrics) Failed to send ack to agent %v: %v", id, err)
			return err
		}
	}
}
