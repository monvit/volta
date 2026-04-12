package main

import (
	"fmt"
	"os"
	"sync"

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
		log.Error("config: %v", err)
		os.Exit(1)
	}
	for _, w := range result.Warnings {
		log.Warn("config: %v", w)
	}

	cfg := result.Config

	was_err := false
	var wg sync.WaitGroup

	wg.Go(func() {
		if err := server.Run(cfg); err != nil {
			log.Error("server error: %v\n", err)
			was_err = true
		}
	})

	wg.Wait()

	if was_err {
		os.Exit(1)
	}
}
