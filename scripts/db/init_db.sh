#!/usr/bin/env bash
set -euo pipefail

DB_USER="${VOLTA_DB_USER:-volta}"
DB_NAME="${VOLTA_DB_NAME:-volta}"
DB_HOST="${VOLTA_DB_HOST:-localhost}"
DB_PORT="${VOLTA_DB_PORT:-5432}"
DB_PASS="${VOLTA_DB_PASSWORD:-}"

if ! command -v psql >/dev/null 2>&1; then
	echo "psql is required (see scripts/db/prereq.sh)" >&2
	exit 1
fi

run_postgres_psql() {
	if [ "$(id -u)" -eq 0 ]; then
		su postgres -s /bin/bash -c "psql $*"
	else
		sudo -u postgres psql "$@"
	fi
}

if [ -z "$DB_PASS" ]; then
	DB_PASS=$(openssl rand -hex 16)
	echo "no VOLTA_DB_PASSWORD set, generated random password"
fi

role_exists=$(run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -tAc \
	"SELECT 1 FROM pg_roles WHERE rolname = '$DB_USER'" || true)

if [ "$role_exists" != "1" ]; then
	echo "creating role $DB_USER..."
	run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -v ON_ERROR_STOP=1 -c \
		"CREATE ROLE $DB_USER LOGIN SUPERUSER PASSWORD '$DB_PASS'"
else
	echo "role $DB_USER already exists, skipping"
fi

db_exists=$(run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -d postgres -tAc \
	"SELECT 1 FROM pg_database WHERE datname = '$DB_NAME'" || true)

if [ "$db_exists" != "1" ]; then
	echo "creating database $DB_NAME..."
	run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -d postgres -v ON_ERROR_STOP=1 -c \
		"CREATE DATABASE $DB_NAME OWNER $DB_USER"
else
	echo "database $DB_NAME already exists, skipping"
fi

echo "enabling timescaledb extension..."
run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -d "$DB_NAME" -v ON_ERROR_STOP=1 -c \
	"CREATE EXTENSION IF NOT EXISTS timescaledb"

shared_preloads=$(run_postgres_psql -h "$DB_HOST" -p "$DB_PORT" -d "$DB_NAME" -tAc \
	"SHOW shared_preload_libraries")
if [[ "$shared_preloads" != *timescaledb* ]]; then
	if command -v timescaledb-tune >/dev/null 2>&1; then
		echo "configuring timescaledb (timescaledb-tune)..."
		timescaledb-tune --quiet --yes
		echo "WARNING: restart PostgreSQL to apply shared_preload_libraries"
	else
		echo "WARNING: 'timescaledb' is not in shared_preload_libraries ($shared_preloads);"
		echo "         add it to postgresql.conf and restart the server"
	fi
fi

cat <<EOF

Database ready.
  DSN: postgres://$DB_USER:<password>@$DB_HOST:$DB_PORT/$DB_NAME
  password: $DB_PASS

Set DB_DSN to the DSN above and run the migrations:

  make db-migrate
EOF