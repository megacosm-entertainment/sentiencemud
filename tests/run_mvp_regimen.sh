#!/usr/bin/env bash
set -euo pipefail

RUN_FULL=0
if [[ "${1:-}" == "--full" ]]; then
  RUN_FULL=1
fi

TS="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="/sentience/src/test_runs"
LOG_FILE="${LOG_DIR}/mvp_regimen_${TS}.log"

mkdir -p "$LOG_DIR"

run_step() {
  local title="$1"
  shift
  echo "" | tee -a "$LOG_FILE"
  echo "==== ${title} ====" | tee -a "$LOG_FILE"
  "$@" 2>&1 | tee -a "$LOG_FILE"
}

echo "MVP channel test regimen started: ${TS}" | tee "$LOG_FILE"

run_step "Build tests" bash -lc "cd /sentience/src && ./build tests"

run_step "Run unit tests" bash -lc "cd /sentience && ./sent -test:unit"

run_step "Run integration tests" bash -lc "cd /sentience && ./sent -test:integration"

if [[ "$RUN_FULL" -eq 1 ]]; then
  run_step "Run full test suite" bash -lc "cd /sentience && ./sent -test"
fi

run_step "Validate runtime channels.json" bash -lc "python3 -m json.tool /sentience/data/system/channels.json >/dev/null"

run_step "Validate bootstrap channels.json" bash -lc "python3 -m json.tool /sentience/src/bootstrap/bootstrap_data/system/channels.json >/dev/null"

run_step "Check channel id uniqueness" bash -lc "python3 - <<'PY'
import json
from pathlib import Path
p = Path('/sentience/data/system/channels.json')
obj = json.loads(p.read_text())
channels = obj.get('channels', [])
if not isinstance(channels, list) or not channels:
    raise SystemExit('FAIL: channels list missing or empty')
ids = [c.get('id') for c in channels if isinstance(c, dict)]
if not all(ids):
    raise SystemExit('FAIL: one or more channels missing id')
if len(ids) != len(set(ids)):
    raise SystemExit('FAIL: duplicate channel ids detected')
print(f'PASS: {len(ids)} channel definitions with unique ids')
PY"

echo "" | tee -a "$LOG_FILE"
echo "MVP regimen complete. Log: ${LOG_FILE}" | tee -a "$LOG_FILE"
echo "Next: run manual runtime smoke/moderation/review checks from docs/testing/MVP_CHANNEL_TEST_REGIMEN.md" | tee -a "$LOG_FILE"
