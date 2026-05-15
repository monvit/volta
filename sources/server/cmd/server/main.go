package main

import (
	"fmt"
	"net"
	"net/http"
	"os"

	chi "github.com/go-chi/chi/v5"
	cfg "github.com/monvit/volta/sources/server/internal/config"
	gs "github.com/monvit/volta/sources/server/internal/grpc"
	log "github.com/monvit/volta/sources/server/internal/logger"
	r "github.com/monvit/volta/sources/server/internal/registry"
	"github.com/monvit/volta/sources/server/pb"
	"google.golang.org/grpc"
)

func main() {
	// logger
	if err := log.Init(); err != nil {
		log.Error("logger: %v", err)
		os.Exit(1)
	}

	// config
	cfg, err := cfg.Load()
	if err != nil {
		log.Error("config: %v", err)
		os.Exit(1)
	}

	log.SetLevel(cfg.Log.Level)
	log.Info("starting server with config: %+v", cfg)

	registry := &r.AgentRegistry{}

	// gRPC
	grpcSrv := grpc.NewServer()
	pb.RegisterVoltaCollectorServer(grpcSrv, gs.New(registry))

	grpcListener, err := net.Listen("tcp", fmt.Sprintf("%v:%v", cfg.GRPC.Addr, cfg.GRPC.Port))
	if err != nil {
		log.Error("listen error: %v", err)
		os.Exit(1)
	}

	go grpcSrv.Serve(grpcListener)

	// HTTP / REST / WebSocket
	mux := chi.NewRouter()
	// mux.Get("/api/agents", srv.handleListAgents)
	// mux.Post("/api/agents/{id}/stream", srv.handleStreamData)
	// mux.Post("/api/agents/{id}/send", srv.handleSendData)
	// mux.Get("/ws", hub.HandleWS)
	http.ListenAndServe(":8080", mux)
}
