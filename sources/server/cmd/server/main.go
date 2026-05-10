package main

import (
	"fmt"
	"os"

	b "github.com/monvit/volta/sources/server/internal/broker"
	cfg "github.com/monvit/volta/sources/server/internal/config"
	g "github.com/monvit/volta/sources/server/internal/gateway"
	log "github.com/monvit/volta/sources/server/internal/logger"
	s "github.com/monvit/volta/sources/server/internal/server"
)

func main() {
	if err := log.Init(); err != nil {
		fmt.Print(err)
		os.Exit(1)
	}

	result, err := cfg.Load()
	if err != nil {
		log.Errorf("config: %v", err)
		os.Exit(1)
	}

	for _, w := range result.Warnings {
		log.Warnf("config: %v", w)
	}

	config := result.Config
	broker := b.New()
	server := s.New(config, broker)
	gateway := g.New(broker, server, server)

	// TODO: handle errors and restarts
	// temporary solution
	run := func(name string, fn func() error) {
		for {
			if err := fn(); err != nil {
				log.Errorf("%s error: %v, restarting...", name, err)
			}
		}
	}

	go run("gateway", func() error { return gateway.Run(config.RESTAddr, config.RESTPort) })
	go run("grpc", func() error { return s.Run(config.GRPCAddr, config.GRPCPort, server) })

	select {}
}
