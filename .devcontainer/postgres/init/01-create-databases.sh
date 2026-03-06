#!/usr/bin/env bash
# Postgres initialization — runs ONLY when the data volume is first created (empty).
# Creates per-app databases alongside the default POSTGRES_DB.
# Runs as the POSTGRES_USER superuser, so no GRANT statements needed.
set -euo pipefail

create_db() {
    local db="$1"
    if psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" \
        -tAc "SELECT 1 FROM pg_database WHERE datname='$db'" | grep -q 1; then
        echo "Database '$db' already exists, skipping."
    else
        psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" \
            -c "CREATE DATABASE \"$db\";"
        echo "Created database '$db'."
    fi
}

# Payload CMS database (PAYLOAD_DB_NAME from Doppler)
create_db "${PAYLOAD_DB_NAME:-payload}"

# Logto identity provider database (LOGTO_DB_NAME from Doppler)
create_db "${LOGTO_DB_NAME:-logto}"
