package grpc

import (
	"github.com/google/uuid"
	"github.com/monvit/volta/sources/server/internal/registry"
	"github.com/monvit/volta/sources/server/internal/session"
	"github.com/monvit/volta/sources/server/pb"
)

type GRPCServer struct {
	pb.UnimplementedVoltaCollectorServer

	registry *registry.AgentRegistry
}

func New(registry *registry.AgentRegistry) *GRPCServer {
	return &GRPCServer{
		registry: registry,
	}
}

func (s *GRPCServer) Connect(stream pb.VoltaCollector_ConnectServer) error {
	// id := extractAgentId(stream)
	id := uuid.New().String()
	sess := session.New(id, stream)
	s.registry.Add(sess)
	sess.Run()
	return nil
}
