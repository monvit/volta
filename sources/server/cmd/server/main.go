package main

import (
	"fmt"
	"os"

	config "github.com/monvit/volta/sources/server/internal/config"
	log "github.com/monvit/volta/sources/server/internal/logger"
	server "github.com/monvit/volta/sources/server/internal/server"
)

func main() {
	// logger
	if err := log.Init(); err != nil {
		fmt.Print(err)
		os.Exit(1)
	}

	// config
	result, err := config.Load()
	if err != nil {
		log.Errorf("config: %v", err)
		os.Exit(1)
	}

	for _, w := range result.Warnings {
		log.Warnf("config: %v", w)
	}

	cfg := result.Config

	errCh := make(chan error, 2)

	go func() { errCh <- server.Run(cfg) }()
	// WS server (not implemented yet)
	// go func() { errCh <- httpServer.Run(cfg) }()

	if err := <-errCh; err != nil {
		log.Errorf("fatal: %v", err)
		os.Exit(1)
	}
}
