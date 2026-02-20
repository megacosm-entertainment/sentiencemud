# Script Entity Parity Gaps (src vs src_20_dev)

Date: 2026-02-20

This file tracks entity domains from `src_20_dev` that are **not present at all** in current `src` scripting types, so we can decide what to backport.

## 1) High-confidence missing variable entity types

These names exist in `src_20_dev` `entity_types[]` but not in current `src` `entity_types[]`, and their corresponding `ENT_*` types are also absent from current `scripts.h` `entity_type_enum`.

Completed in this session:
- `aregion` (added as first Area Region backport slice with `varset arearegion|aregion`, `room.region`, `area.region`, and `aregion` field table)

- `skillgroup`
- `book_page`
- `food_buff`
- `compartment`
- `waypoint`
- `stock`
- `dynlist_aregion`
- `list_area`
- `list_aregion`
- `list_book_page`
- `list_food_buff`
- `list_compartment`
- `blueprint`
- `bpsection`
- `dngindex`
- `shipindex`
- `lockstate`
- `spell`
- `liquid`
- `material`
- `mission`
- `missionpart`
- `reputation`
- `repindex`
- `reprank`

## 2) Related missing `ENT_*` categories (broader)

The following major domains are absent from current `entity_type_enum` and likely need coordinated backports (enum + entity table + setters/getters + command hooks + tests):

- Area Region domain (remaining: `ENT_AREA_REGION_ID` and list variants)
- Skill Group domain (`ENT_SKILLGROUP`, iterator/list variants)
- Reputation domain (`ENT_REPUTATION`, `ENT_REPUTATION_INDEX`, `ENT_REPUTATION_RANK`)
- Mission domain (`ENT_MISSION`, `ENT_MISSION_PART`, iterators)
- Liquid / Material / Spell-data domain (`ENT_LIQUID`, `ENT_MATERIAL`, `ENT_SPELL`)
- Blueprint domain (`ENT_BLUEPRINT`, `ENT_BLUEPRINT_SECTION`)
- Index domains (`ENT_DUNGEONINDEX`, `ENT_SHIPINDEX`, `ENT_TOKENINDEX`)
- Furniture/book/shop adjunct domains (`ENT_COMPARTMENT`, `ENT_BOOK_PAGE`, `ENT_FOOD_BUFF`, `ENT_SHOP_STOCK`)
- Waypoint domain (`ENT_WAYPOINT`)

## 3) Suggested implementation order

1. **Area/Region list plumbing** (`dynlist_aregion`, `list_area`, `list_aregion`)
2. **Reputation + Mission** (high gameplay/scripting value)
3. **Liquid/Material/Spell-data** (dependency-heavy; verify current data model compatibility first)
4. **Blueprint + index entities** (`blueprint`, `bpsection`, `dngindex`, `shipindex`)
5. **Content adjuncts** (`book_page`, `food_buff`, `compartment`, `stock`, `waypoint`)

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
