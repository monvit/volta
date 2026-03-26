package main

import (
	"fmt"
	"os"
	"sync"
	config "volta/server/internal/config"
	server "volta/server/internal/server"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)

		if cfg == nil {
			os.Exit(1)
		}
	}

	var wg sync.WaitGroup

	wg.Go(func() {
		if err := server.Run(cfg); err != nil {
			fmt.Fprintf(os.Stderr, "server error: %v\n", err)
			os.Exit(1) // TODO: exit in main
		}
	})

	wg.Wait()
}
