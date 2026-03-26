package server

import (
	"fmt"
	"net"
	pb "volta/server/generated/volta"
	config "volta/server/internal/config"

	"google.golang.org/grpc"
)

func Run(cfg *config.Config) error {
	lis, err := net.Listen("tcp", fmt.Sprintf("localhost:%v", cfg.ServerPort))
	if err != nil {
		return err
	}

	var opts []grpc.ServerOption
	grpcServer := grpc.NewServer(opts...)
	server := &VoltaCollectorServer{}
	pb.RegisterVoltaCollectorServer(grpcServer, server)

	if err := grpcServer.Serve(lis); err != nil {
		return err
	}

	return nil
}
