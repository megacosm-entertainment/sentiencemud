# Script Entity Parity Gaps (src vs src_20_dev)

Date: 2026-02-20

This file tracks entity domains from `src_20_dev` that are **not present at all** in current `src` scripting types, so we can decide what to backport.

## 1) High-confidence missing variable entity types

These names exist in `src_20_dev` `entity_types[]` but not in current `src` `entity_types[]`, and their corresponding `ENT_*` types are also absent from current `scripts.h` `entity_type_enum`.

Completed so far:
- `aregion` (+ `dynlist_aregion`, `list_area`, `list_aregion`)
- `skillgroup`
- Reputation slice: `reputation`, `repindex`, `reprank`
- Blueprint/index slice: `blueprint`, `bpsection`, `dngindex`, `shipindex`
- Liquid/material/spell/lockstate slice: `liquid`, `material`, `spell`, `lockstate`
- Content adjunct slice: `book_page`, `food_buff`, `waypoint`, `stock`

Also addressed while backporting index entities:
- `tokenindex` underlying scripting parity gap (`ENT_TOKEN_INDEX` field/expansion + index varset wiring)

Deferred for now:
- Mission slice: `mission`, `missionpart`
- Compartment slice: `compartment` (and related furniture adjunct plumbing)
- List-only adjuncts: `list_book_page`, `list_food_buff` (to be done with list-entity plumbing)

## 2) Related missing `ENT_*` categories (broader)

The following major domains are absent from current `entity_type_enum` and likely need coordinated backports (enum + entity table + setters/getters + command hooks + tests):

- Area Region domain (remaining: `ENT_AREA_REGION_ID` and list variants)
- Skill Group domain (`ENT_SKILLGROUP`, iterator/list variants)
- Reputation domain (`ENT_REPUTATION`, `ENT_REPUTATION_INDEX`, `ENT_REPUTATION_RANK`)
- Mission domain (`ENT_MISSION`, `ENT_MISSION_PART`, iterators) [deferred]
- Liquid / Material / Spell-data domain (`ENT_LIQUID`, `ENT_MATERIAL`, `ENT_SPELL`)
- Furniture/book/shop adjunct domains (`ENT_COMPARTMENT`, `ENT_BOOK_PAGE`, `ENT_FOOD_BUFF`, `ENT_SHOP_STOCK`) [`ENT_COMPARTMENT` deferred]
- Waypoint domain (`ENT_WAYPOINT`)

## 3) Suggested implementation order

1. **Area/Region list plumbing** (`dynlist_aregion`, `list_area`, `list_aregion`)
2. **Reputation + Mission** (high gameplay/scripting value)
3. **Liquid/Material/Spell-data** (dependency-heavy; verify current data model compatibility first) ✅
4. **Blueprint + index entities** (`blueprint`, `bpsection`, `dngindex`, `shipindex`) ✅
5. **Content adjuncts** (`book_page`, `food_buff`, `compartment`, `stock`, `waypoint`) (`compartment` deferred)

## 4) Regeneration command

Run from `/sentience` to refresh this inventory:

```bash
python3 - <<'PY'
import re
from pathlib import Path

cur_h=Path('src/scripts.h').read_text(errors='ignore')
old_h=Path('src_20_dev/scripts.h').read_text(errors='ignore')
cur_sc=Path('src/script_const.c').read_text(errors='ignore')
old_sc=Path('src_20_dev/script_const.c').read_text(errors='ignore')

m=re.search(r'enum\s+entity_type_enum\s*\{(.*?)\n\};',cur_h,re.S)
cur_ent=set(re.findall(r'\bENT_[A-Z0-9_]+\b', m.group(1))) if m else set()
m=re.search(r'enum\s+entity_type_enum\s*\{(.*?)\n\};',old_h,re.S)
old_ent=set(re.findall(r'\bENT_[A-Z0-9_]+\b', m.group(1))) if m else set()

m=re.search(r'ENT_FIELD\s+entity_types\[\]\s*=\s*\{(.*?)\n\};',cur_sc,re.S)
cur_types=set(re.findall(r'\{"([^"]+)"\s*,', m.group(1))) if m else set()
m=re.search(r'ENT_FIELD\s+entity_types\[\]\s*=\s*\{(.*?)\n\};',old_sc,re.S)
old_types=set(re.findall(r'\{"([^"]+)"\s*,', m.group(1))) if m else set()

print('Missing ENT_*:', *sorted(old_ent-cur_ent), sep='\n')
print('\nMissing type names:', *sorted(old_types-cur_types), sep='\n')
PY
```
