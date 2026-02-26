# MVP Channel Testing Regimen

This is a practical test flow for validating the channel-system MVP before broader testing.

## 1) Preflight (one-time per test cycle)

Run from `/sentience` unless noted.

```bash
cd /sentience/src
./build tests
cd /sentience
```

Gate:
- Build succeeds with no new compile errors.

---

## 2) Automated Regression Ladder (required)

Run these in order:

```bash
# Fast unit subset
./sent -test:unit

# Full integration suite
./sent -test:integration

# Full framework sweep (optional but recommended before external testing)
./sent -test
```

Gates:
- `Failed: 0`
- `Errors: 0`
- Skips are acceptable if expected for environment-sensitive tests.

Notes:
- `libgcov ... different checksum` warnings are profiling-artifact warnings, not functional failures.

---

## 3) Channel Registry/Data Integrity (required)

Validate active channel config JSON:

```bash
python3 -m json.tool /sentience/data/system/channels.json >/dev/null
python3 -m json.tool /sentience/src/bootstrap/bootstrap_data/system/channels.json >/dev/null
```

Optional quick schema sanity checks:

```bash
python3 - <<'PY'
import json
from pathlib import Path
p = Path('/sentience/data/system/channels.json')
obj = json.loads(p.read_text())
channels = obj.get('channels', [])
assert isinstance(channels, list) and channels, 'channels list missing/empty'
ids = [c.get('id') for c in channels if isinstance(c, dict)]
assert all(ids), 'one or more channels missing id'
assert len(ids) == len(set(ids)), 'duplicate channel ids found'
print(f'OK: {len(ids)} channel definitions with unique ids')
PY
```

Gates:
- JSON parses cleanly.
- No missing or duplicate channel IDs.

---

## 4) Runtime Smoke (required)

Start game with fresh binary:

```bash
cd /sentience/src
./build
./install
cd /sentience
```

Then run server and connect 2+ clients.

Minimum in-game checks:

1. `channels` shows expected access and recv state for each tester.
2. Global channels (`gossip`, `ooc`, `quote`) publish/receive correctly.
3. Toggle-only channels (`hints`, `tells`, `announcements`) toggle receive and reject publish text.
4. Tell-family (`tell`, `reply`) honors mute/policy and still updates reply pointer behavior.
5. `history <channel>` renders recent entries without errors.

Gates:
- No runtime errors/crashes.
- No behavior regressions vs expected legacy semantics.

---

## 5) Moderation + Review Flow (required)

Use staff + player accounts:

1. Apply warning and mute:
   - `chanwarn <player> <channel> <reason>`
   - `chanmute <player> <channel> 5m <reason>`
2. Confirm muted sender cannot publish.
3. Clear mute:
   - `chanunmute <player> <channel>`
4. Validate report/review path:
   - Player: `history <channel> report <message-id> <notes>`
   - Staff: `rview list`, `rview read <id>`, `rview ack <id>`

Gates:
- Penalties enforce correctly and are reversible.
- Report appears in review queue and can be acknowledged.

---

## 6) Context Scope Matrix (recommended for larger rollout)

Validate at least one scenario for each scope in use:

- `GLOBAL`
- `AREA`
- `ROOM_WV`
- `GROUP_ID`
- `CHURCH_ID`
- `INSTANCE_ID`
- `DUNGEON_ID`
- `DIRECT_ENTITY`

Checks per scope:
- Publisher in-scope can send.
- In-scope recipient receives.
- Out-of-scope recipient does not receive.

---

## 7) Backend Matrix (recommended if redis/local is in play)

If you are testing multiple transport backends, run the automated ladder + runtime smoke for each backend mode you use:

- `legacy_iterative`
- `local`
- `auto`
- `redis`

Gate:
- No functional channel behavior delta between tested backends.

---

## 8) Exit Criteria for MVP Rollout

Ship when all are true:

- Required sections (2, 3, 4, 5) pass.
- No new failing integration tests.
- No channel publish/delivery regressions in runtime smoke.
- `channels.json` remains valid and complete.

---

## Fast Re-Run Command

For repeat cycles, use the helper script:

```bash
/sentience/src/tests/run_mvp_regimen.sh
```

Add `--full` to include `./sent -test`:

```bash
/sentience/src/tests/run_mvp_regimen.sh --full
```
