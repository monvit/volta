package db

import (
	"testing"
)

func TestMigrationFileParses(t *testing.T) {
	m, err := NewMigrator("postgres://user:pass@127.0.0.1:5432/db")
	if err != nil {
		t.Fatalf("failed to parse embedded migrations: %v", err)
	}
	defer m.Close()

	sources := m.provider.ListSources()
	if len(sources) != 1 {
		t.Fatalf("expected 1 migration source, got %d", len(sources))
	}
	if sources[0].Version != 1 {
		t.Errorf("expected migration version 1, got %d", sources[0].Version)
	}
}
