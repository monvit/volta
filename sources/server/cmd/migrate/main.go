package main

import (
	"context"
	"fmt"
	"os"

	"github.com/monvit/volta/server/internal/config"
	dbpkg "github.com/monvit/volta/server/internal/db"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	cfg, err := config.Load()
	if err != nil {
		return fmt.Errorf("config: %w", err)
	}

	command := "up"
	if len(os.Args) > 1 && os.Args[1][0] != '-' {
		command = os.Args[1]
	}

	if cfg.DB.DSN == "" {
		return fmt.Errorf("db.dsn is required (set via config, --db.dsn, or DB_DSN env)")
	}

	m, err := dbpkg.NewMigrator(cfg.DB.DSN)
	if err != nil {
		return err
	}
	defer m.Close()

	ctx := context.Background()

	switch command {
	case "up":
		if err := m.Up(ctx); err != nil {
			return err
		}
		fmt.Println("migrate: up")
	case "down":
		if err := m.Down(ctx); err != nil {
			return err
		}
		fmt.Println("migrate: down")
	case "status":
		pending, err := m.HasPending(ctx)
		if err != nil {
			return err
		}
		if pending {
			fmt.Println("migrate: pending migrations remain")
		} else {
			fmt.Println("migrate: schema up to date")
		}
		return nil
	default:
		return fmt.Errorf("unknown command %q (expected up, down, status)", command)
	}

	return nil
}
