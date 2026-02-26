#!/usr/bin/env bash
set -euo pipefail

# Run test patterns across multiple fresh game process invocations.
# Useful for serialization/deserialization and cache contamination checks.

BIN="/sentience/src/.build/Debug/sent"
CYCLES=2
DATA_ROOT=""
BOOTSTRAP_ROOT=""
DO_BOOTSTRAP=0
FLUSH_REDIS=0

PATTERNS=("profile:serialization_cycle")

usage() {
    cat <<'EOF'
Usage: tests/run_test_cycles.sh [options]

Options:
  --bin <path>                 Sent binary path (default: /sentience/src/.build/Debug/sent)
  --cycles <n>                 Number of restart cycles (default: 2)
  --pattern <value>            Test pattern to run each cycle (repeatable)
                               Example: --pattern profile:bootstrap_ci
  --data-root <path>           Pass --data-root=<path> to test runs
  --bootstrap-root <path>      Bootstrap root and use it as data root
  --bootstrap                  Run bootstrap before cycle 1 (requires --bootstrap-root)
  --flush-redis               Flush Redis before each cycle if redis-cli exists
  --help                      Show this help

Examples:
  tests/run_test_cycles.sh --cycles 3 --pattern profile:serialization_cycle

  tests/run_test_cycles.sh \
    --bootstrap \
    --bootstrap-root /tmp/sent_bootstrap_run \
    --pattern profile:bootstrap_ci \
    --pattern profile:bootstrap_ci_redis
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bin)
            BIN="$2"
            shift 2
            ;;
        --cycles)
            CYCLES="$2"
            shift 2
            ;;
        --pattern)
            if [[ ${#PATTERNS[@]} -eq 1 && "${PATTERNS[0]}" == "profile:serialization_cycle" ]]; then
                PATTERNS=()
            fi
            PATTERNS+=("$2")
            shift 2
            ;;
        --data-root)
            DATA_ROOT="$2"
            shift 2
            ;;
        --bootstrap-root)
            BOOTSTRAP_ROOT="$2"
            DATA_ROOT="$2"
            shift 2
            ;;
        --bootstrap)
            DO_BOOTSTRAP=1
            shift
            ;;
        --flush-redis)
            FLUSH_REDIS=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ ! -x "$BIN" ]]; then
    echo "Error: binary not executable: $BIN"
    echo "Build first with: cd /sentience/src && ./build tests"
    exit 1
fi

if ! [[ "$CYCLES" =~ ^[0-9]+$ ]] || [[ "$CYCLES" -lt 1 ]]; then
    echo "Error: --cycles must be a positive integer"
    exit 1
fi

if [[ "$DO_BOOTSTRAP" -eq 1 && -z "$BOOTSTRAP_ROOT" ]]; then
    echo "Error: --bootstrap requires --bootstrap-root"
    exit 1
fi

if [[ "$DO_BOOTSTRAP" -eq 1 ]]; then
    echo "[multirun] Bootstrapping isolated root: $BOOTSTRAP_ROOT"
    "$BIN" \
        -bootstrap \
        --bootstrap-auto \
        --bootstrap-ci-fixtures \
        "--bootstrap-root=$BOOTSTRAP_ROOT" \
        --bootstrap-username=bootstrapci \
        --bootstrap-email=bootstrapci@example.com \
        --bootstrap-password=bootstrap123
fi

run_test_pattern() {
    local pattern="$1"
    local -a cmd=("$BIN" "-test:$pattern")
    if [[ -n "$DATA_ROOT" ]]; then
        cmd+=("--data-root=$DATA_ROOT")
    fi

    echo "[multirun] Running: ${cmd[*]}"
    "${cmd[@]}"
}

for ((cycle=1; cycle<=CYCLES; cycle++)); do
    echo ""
    echo "[multirun] === Cycle $cycle/$CYCLES ==="

    if [[ "$FLUSH_REDIS" -eq 1 ]]; then
        if command -v redis-cli >/dev/null 2>&1; then
            echo "[multirun] Flushing Redis before cycle $cycle"
            redis-cli FLUSHALL >/dev/null || {
                echo "[multirun] Warning: redis-cli FLUSHALL failed"
            }
        else
            echo "[multirun] Warning: redis-cli not found, skipping Redis flush"
        fi
    fi

    for pattern in "${PATTERNS[@]}"; do
        run_test_pattern "$pattern"
    done
done

echo ""
echo "[multirun] Completed $CYCLES cycles successfully"
