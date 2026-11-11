#!/usr/bin/env bash
set -euo pipefail

DB_HOST="${VOLTA_DB_HOST:-localhost}"
DB_PORT="${VOLTA_DB_PORT:-5432}"
DB_USER_CHECK="${PGUSER:-postgres}"

PENDING=()
FAIL=0

check_cmd() {
	local name="$1"
	if command -v "$name" >/dev/null 2>&1; then
		echo "[ok] $name: $(command -v "$name")"
	else
		echo "[missing] $name"
		PENDING+=("$name")
	fi
}

check_cmd psql
check_cmd pg_isready

if ! pg_isready -q -h "$DB_HOST" -p "$DB_PORT" 2>/dev/null; then
	echo "[missing] running PostgreSQL server at $DB_HOST:$DB_PORT"
	FAIL=1
fi

if command -v psql >/dev/null 2>&1; then
	if ext_result=$(psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER_CHECK" -w -d postgres -Atqc \
		"SELECT default_version || '|' || coalesce(installed_version, '') FROM pg_available_extensions WHERE name = 'timescaledb';" 2>&1); then
		if [ -z "$ext_result" ]; then
			echo "[missing] timescaledb extension not available in PostgreSQL at $DB_HOST:$DB_PORT"
			FAIL=1
		else
			ext_default="${ext_result%%|*}"
			ext_installed="${ext_result#*|}"
			if [ -z "$ext_installed" ]; then
				ext_installed="not installed"
			fi
			echo "[ok] timescaledb available (default $ext_default, $ext_installed)"
		fi
	else
		echo "[missing] cannot connect to PostgreSQL at $DB_HOST:$DB_PORT as role '$DB_USER_CHECK'"
		echo "          $ext_result"
		echo "          hint: set PGUSER to a role that can connect (e.g. PGUSER=postgres)"
		FAIL=1
	fi
fi

if [ "${#PENDING[@]}" -gt 0 ] || [ "$FAIL" -eq 1 ]; then
	cat <<EOF

Missing prerequisites. Install PostgreSQL and the TimescaleDB extension, e.g.:

  Debian/Ubuntu:  https://docs.timescale.com/self-hosted/latest/install/installation-linux/
  Arch:           pacman -S timescaledb postgresql && timescaledb-tune

Then verify with: scripts/db/prereq.sh
EOF
	exit 1
fi

echo "Prerequisites satisfied."