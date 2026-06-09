package main

import (
	"context"
	"errors"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	commandrouter "github.com/monvit/volta/server/internal/commandRouter"
	"github.com/monvit/volta/server/internal/config"
	eventbus "github.com/monvit/volta/server/internal/eventBus"
	"github.com/monvit/volta/server/internal/httpserver"
	"github.com/monvit/volta/server/internal/hub"
	"github.com/monvit/volta/server/internal/logger"
	"github.com/monvit/volta/server/internal/registry"
	"github.com/monvit/volta/server/internal/server"
)

func main() {
	// logger
	if err := logger.Init(); err != nil {
		logger.Error("logger: %v", err)
		os.Exit(1)
	}

	// config
	cfg, err := config.Load()
	if err != nil {
		logger.Error("config: %v", err)
		os.Exit(1)
	}

	logger.SetLevel(cfg.Log.Level)
	logger.Info("starting server with config: %+v", cfg)

	// dependencies
	registry := &registry.AgentRegistry{}
	bus := eventbus.New()
	router := commandrouter.New(registry)
	hub := hub.New(bus)
	go hub.Run()

	// gRPC
	grpcSrv := server.New(registry, bus)
	lis, err := net.Listen("tcp", net.JoinHostPort(cfg.GRPC.Addr, strconv.Itoa(int(cfg.GRPC.Port))))
	if err != nil {
		logger.Error("cannot bind gRPC: %v", err)
		os.Exit(1)
	}
	go func() {
		logger.Info("gRPC listening at address %v:%v", cfg.GRPC.Addr, cfg.GRPC.Port)
		if err := grpcSrv.Serve(lis); err != nil {
			logger.Error("gRPC server error: %v", err)
		}
	}()

	// HTTP
	httpSrv := httpserver.NewHTTPServer(registry, router, hub)
	srv := &http.Server{
		Addr:         net.JoinHostPort(cfg.REST.Addr, strconv.Itoa(int(cfg.REST.Port))),
		Handler:      httpSrv,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	// shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		logger.Info("HTTP listening at address %v:%v", cfg.REST.Addr, cfg.REST.Port)
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			logger.Error("HTTP server error: %v", err)
			os.Exit(1)
		}
	}()

	// graceful shutdown
	<-quit
	logger.Info("shutting down...")

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	grpcDone := make(chan struct{})
	go func() {
		grpcSrv.GracefulStop()
		close(grpcDone)
	}()

	select {
	case <-grpcDone:
		logger.Info("gRPC stopped cleanly")
	case <-ctx.Done():
		logger.Warn("gRPC shutdown timed out, forcing stop")
		grpcSrv.Stop()
	}

	hub.Close()

	if err := srv.Shutdown(ctx); err != nil {
		logger.Error("HTTP shutdown error: %v", err)
	}

	logger.Info("shutdown complete")
}
