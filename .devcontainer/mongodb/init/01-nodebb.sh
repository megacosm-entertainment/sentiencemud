#!/usr/bin/env bash
# MongoDB initialization — runs ONLY when the data directory is first created (empty).
# Ensures the NodeBB database exists and the root user (MONGO_USER) has explicit
# readWrite + dbAdmin access on it.
#
# The root user is created automatically from MONGO_INITDB_ROOT_USERNAME/PASSWORD.
# NodeBB connects using those same credentials (MONGO_USER / MONGO_PASSWORD).
# This script just makes that access explicit and creates the database up-front.
set -euo pipefail

NODEBB_DB="${MONGO_DB_NAME:-nodebb}"

mongosh --quiet \
    --username "$MONGO_INITDB_ROOT_USERNAME" \
    --password "$MONGO_INITDB_ROOT_PASSWORD" \
    --authenticationDatabase admin \
    --eval "
        db = db.getSiblingDB('${NODEBB_DB}');
        var existing = db.getUser('${MONGO_INITDB_ROOT_USERNAME}');
        if (!existing) {
            db.createUser({
                user: '${MONGO_INITDB_ROOT_USERNAME}',
                pwd: '${MONGO_INITDB_ROOT_PASSWORD}',
                roles: [
                    { role: 'readWrite', db: '${NODEBB_DB}' },
                    { role: 'dbAdmin',   db: '${NODEBB_DB}' }
                ]
            });
            print('Granted ${MONGO_INITDB_ROOT_USERNAME} access to ${NODEBB_DB}.');
        } else {
            print('User ${MONGO_INITDB_ROOT_USERNAME} already has access, skipping.');
        }
        // Touch a marker collection so the db is created in the volume
        db.createCollection('_init', { capped: true, size: 1024, max: 1 });
        print('MongoDB NodeBB database initialized.');
    "
