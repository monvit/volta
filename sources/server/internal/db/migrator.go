package db

import (
	"context"
	"database/sql"
	"embed"
	"fmt"
	"io/fs"

	"github.com/pressly/goose/v3"

	_ "github.com/jackc/pgx/v5/stdlib"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

type Migrator struct {
	provider *goose.Provider
	sqlDB    *sql.DB
}

func NewMigrator(dsn string) (*Migrator, error) {
	sqlDB, err := sql.Open("pgx", dsn)
	if err != nil {
		return nil, fmt.Errorf("db: open sql connection: %w", err)
	}

	dir, err := fs.Sub(migrationsFS, "migrations")
	if err != nil {
		sqlDB.Close()
		return nil, fmt.Errorf("db: open embedded migrations: %w", err)
	}

	provider, err := goose.NewProvider(goose.DialectPostgres, sqlDB, dir)
	if err != nil {
		sqlDB.Close()
		return nil, fmt.Errorf("db: create migrator: %w", err)
	}

	return &Migrator{provider: provider, sqlDB: sqlDB}, nil
}

func (m *Migrator) Up(ctx context.Context) error {
	_, err := m.provider.Up(ctx)
	if err != nil {
		return fmt.Errorf("db: migrate up: %w", err)
	}
	return nil
}

func (m *Migrator) Down(ctx context.Context) error {
	_, err := m.provider.Down(ctx)
	if err != nil {
		return fmt.Errorf("db: migrate down: %w", err)
	}
	return nil
}

func (m *Migrator) HasPending(ctx context.Context) (bool, error) {
	return m.provider.HasPending(ctx)
}

func (m *Migrator) Close() error {
	return m.sqlDB.Close()
}
