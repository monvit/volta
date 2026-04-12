package server

import (
	"fmt"
	"net"

	pb "github.com/monvit/volta/sources/server/pb"

	config "github.com/monvit/volta/sources/server/internal/config"

	"google.golang.org/grpc"
)

func Run(cfg *config.Config) error {
	lis, err := net.Listen("tcp", fmt.Sprintf("localhost:%v", cfg.ServerPort))
	if err != nil {
		return err
	}

	var opts []grpc.ServerOption
	grpcServer := grpc.NewServer(opts...)
	server := &VoltaCollectorServer{
		clients: make(map[string]*Client),
	}
	pb.RegisterVoltaCollectorServer(grpcServer, server)

	if err := grpcServer.Serve(lis); err != nil {
		return err
	}

	return nil
}
