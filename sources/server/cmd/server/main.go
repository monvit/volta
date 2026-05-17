package main

import (
	"context"
	"errors"
	"log/slog"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	commandrouter "github.com/monvit/volta/sources/server/internal/commandRouter"
	"github.com/monvit/volta/sources/server/internal/config"
	eventbus "github.com/monvit/volta/sources/server/internal/eventBus"
	"github.com/monvit/volta/sources/server/internal/httpserver"
	"github.com/monvit/volta/sources/server/internal/hub"
	"github.com/monvit/volta/sources/server/internal/logger"
	"github.com/monvit/volta/sources/server/internal/registry"
	"github.com/monvit/volta/sources/server/internal/server"
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

	// Zależności
	registry := &registry.AgentRegistry{}
	bus := eventbus.New()
	router := commandrouter.New(registry)
	hub := hub.New(bus)
	go hub.Run()

	// gRPC
	grpcSrv := server.New(registry, bus)
	lis, err := net.Listen("tcp", ":5000")
	if err != nil {
		slog.Error("cannot bind gRPC", "err", err)
		os.Exit(1)
	}
	go func() {
		slog.Info("gRPC listening", "addr", ":5000")
		if err := grpcSrv.Serve(lis); err != nil {
			slog.Error("gRPC server error", "err", err)
		}
	}()

	// HTTP
	httpSrv := httpserver.NewHTTPServer(registry, router, hub)
	srv := &http.Server{
		Addr:         ":8080",
		Handler:      httpSrv,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	// Graceful shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		slog.Info("HTTP listening", "addr", ":8080")
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			slog.Error("HTTP server error", "err", err)
			os.Exit(1)
		}
	}()

	<-quit
	slog.Info("shutting down...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	grpcSrv.GracefulStop()
	if err := srv.Shutdown(ctx); err != nil {
		slog.Error("HTTP shutdown error", "err", err)
	}

	slog.Info("bye")
}
