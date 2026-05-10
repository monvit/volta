package server

import (
	"fmt"
	"net"

	pb "github.com/monvit/volta/sources/server/pb"

	"google.golang.org/grpc"
)

func Run(addr string, port uint, server *VoltaCollectorServer) error {
	lis, err := net.Listen("tcp", fmt.Sprintf("%s:%v", addr, port))
	if err != nil {
		return err
	}

	var opts []grpc.ServerOption
	grpcServer := grpc.NewServer(opts...)
	// server := New(cfg, broker)
	pb.RegisterVoltaCollectorServer(grpcServer, server)

	if err := grpcServer.Serve(lis); err != nil {
		return err
	}

	return nil
}
