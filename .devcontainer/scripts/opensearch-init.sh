#!/bin/sh
# OpenSearch index initialization — runs once after OpenSearch is healthy.
# Creates index templates for Sentience game data.
# All operations use PUT, which is idempotent (safe to re-run).
set -eu

OS="${OPENSEARCH_URL:-http://opensearch:9200}"

echo "OpenSearch init: connecting to $OS"

# ── Character search template ────────────────────────────────────────────────
curl -sf -X PUT "$OS/_index_template/sentience-characters" \
    -H 'Content-Type: application/json' \
    -d '{
  "index_patterns": ["sentience-characters*"],
  "template": {
    "settings": {
      "number_of_shards": 1,
      "number_of_replicas": 0,
      "analysis": {
        "analyzer": {
          "name_analyzer": {
            "type": "custom",
            "tokenizer": "standard",
            "filter": ["lowercase", "asciifolding"]
          }
        }
      }
    },
    "mappings": {
      "properties": {
        "name":        { "type": "text",    "analyzer": "name_analyzer", "fields": { "keyword": { "type": "keyword" } } },
        "account":     { "type": "keyword" },
        "level":       { "type": "integer" },
        "race":        { "type": "keyword" },
        "class":       { "type": "keyword" },
        "last_login":  { "type": "date" },
        "created_at":  { "type": "date" },
        "online":      { "type": "boolean" }
      }
    }
  }
}'
echo "  [OK] sentience-characters template"

# ── Area/Zone search template ────────────────────────────────────────────────
curl -sf -X PUT "$OS/_index_template/sentience-areas" \
    -H 'Content-Type: application/json' \
    -d '{
  "index_patterns": ["sentience-areas*"],
  "template": {
    "settings": {
      "number_of_shards": 1,
      "number_of_replicas": 0
    },
    "mappings": {
      "properties": {
        "name":        { "type": "text",    "fields": { "keyword": { "type": "keyword" } } },
        "description": { "type": "text" },
        "vnum_low":    { "type": "integer" },
        "vnum_high":   { "type": "integer" },
        "builders":    { "type": "keyword" },
        "flags":       { "type": "keyword" }
      }
    }
  }
}'
echo "  [OK] sentience-areas template"

# ── Help file search template ────────────────────────────────────────────────
curl -sf -X PUT "$OS/_index_template/sentience-help" \
    -H 'Content-Type: application/json' \
    -d '{
  "index_patterns": ["sentience-help*"],
  "template": {
    "settings": {
      "number_of_shards": 1,
      "number_of_replicas": 0
    },
    "mappings": {
      "properties": {
        "keywords": { "type": "text",    "fields": { "keyword": { "type": "keyword" } } },
        "text":     { "type": "text" },
        "level":    { "type": "integer" }
      }
    }
  }
}'
echo "  [OK] sentience-help template"

# ── Audit/log event template ──────────────────────────────────────────────────
curl -sf -X PUT "$OS/_index_template/sentience-events" \
    -H 'Content-Type: application/json' \
    -d '{
  "index_patterns": ["sentience-events-*"],
  "template": {
    "settings": {
      "number_of_shards": 1,
      "number_of_replicas": 0
    },
    "mappings": {
      "properties": {
        "timestamp":  { "type": "date" },
        "event_type": { "type": "keyword" },
        "character":  { "type": "keyword" },
        "account":    { "type": "keyword" },
        "ip":         { "type": "ip" },
        "data":       { "type": "object",  "dynamic": true }
      }
    }
  }
}'
echo "  [OK] sentience-events template"

echo "OpenSearch initialization complete."
