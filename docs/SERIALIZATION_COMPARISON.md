# JSON vs Old-Format Serialization Comparison

Comprehensive field-by-field comparison of JSON serializers (`json_area.c`, `json_instance.c`, `json_persist.c`) vs old-format serializers (`olc_save.c`, `db.c`).

---

## 1. AREA_DATA Metadata

**JSON writer**: `json_area_serialize_metadata()` — [json_area.c](io/json/json_area.c#L339)
**JSON reader**: `json_area_deserialize_metadata()` — [json_area.c](io/json/json_area.c#L460)
**Old writer**: `save_area_new()` — [olc_save.c](olc_save.c#L358) (old-format section starts ~line 481)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `uid` | `Uid %ld` | `"uid"` | ✅ | ✅ | |
| `name` | `#AREA %s~` | `"name"` | ✅ | ✅ | |
| `file_name` | `FileName %s~` | `"filename"` | ✅ | ✅ | |
| `min_vnum` | `VNUMs %ld %ld` | `"vnums"."min"` | ✅ | ✅ | |
| `max_vnum` | `VNUMs %ld %ld` | `"vnums"."max"` | ✅ | ✅ | |
| `builders` | `Builders %s~` | `"metadata"."builders"` | ✅ | ✅ | |
| `credits` | `Credits %s~` | `"metadata"."credits"` | ✅ | ✅ | |
| `security` | `Security %d` | `"metadata"."security"` | ✅ | ✅ | |
| `area_who` | `AreaWho %d` | `"metadata"."area_who"` | ✅ | ✅ | |
| `low_range` | — | `"metadata"."levels"."min"` | ✅ | ✅ | **See level mismatch below** |
| `high_range` | — | `"metadata"."levels"."max"` | ✅ | ✅ | **See level mismatch below** |
| `min_level` | `Levels %d %d` | — | ❌ MISSING | ❌ MISSING | **Old saves `min_level`/`max_level`; JSON saves `low_range`/`high_range` — different struct fields!** |
| `max_level` | `Levels %d %d` | — | ❌ MISSING | ❌ MISSING | Same as above |
| `area_flags` | `AreaFlags %ld` | `"flags"` (array) | ✅ | ✅ | Old=integer, JSON=flag name array |
| `place_flags` | `PlaceType %ld` | — | ❌ MISSING | ❌ MISSING | **Comment: "place_type not in current structure - skip for now"** |
| `x` | `XCoord %d` | `"coordinates"."x"` | ✅ | ✅ | |
| `y` | `YCoord %d` | `"coordinates"."y"` | ✅ | ✅ | |
| `land_x` | `XLand %d` | `"coordinates"."x_land"` | ✅ | ✅ | |
| `land_y` | `YLand %d` | `"coordinates"."y_land"` | ✅ | ✅ | |
| `recall` (LOCATION) | `RecallW`/`Recall` | `"recall"` (object) | ✅ | ✅ | JSON uses widevnum when available |
| `wilds_uid` | `WildsVnum %ld` | `"wilds_uid"` | ✅ | ✅ | |
| `repop` | `Repop %d` | `"repop"` | ✅ | ✅ | |
| `open` | `Open %d` | — | ❌ MISSING | ❌ MISSING | **Not in JSON at all** |
| `post_office_load.vnum` | `PostOffice %ld` | `"post_office"` | ✅ | ✅ | |
| `airship_land_load.vnum` | `AirshipLand %ld` | `"airship_land"` | ✅ | ✅ | |
| `description` | `Description %s~` | `"description"` | ✅ | ✅ | |
| `comments` | `Comments %s~` | `"comments"` | ✅ | ✅ | |
| `notes` | `Notes %s~` | `"notes"` | ✅ | ✅ | |
| `version_area` | `VersArea %d` | `"versions"."area"` | ✅ | ✅ | |
| `version_mobile` | `VersMobile %d` | `"versions"."mobile"` | ✅ | ✅ | |
| `version_object` | `VersObject %d` | `"versions"."object"` | ✅ | ✅ | |
| `version_room` | `VersRoom %d` | `"versions"."room"` | ✅ | ✅ | |
| `version_token` | `VersToken %d` | `"versions"."token"` | ✅ | ✅ | |
| `version_script` | `VersScript %d` | `"versions"."script"` | ✅ | ✅ | |
| `version_wilds` | `VersWilds %d` | `"versions"."wilds"` | ✅ | ✅ | |
| `points` (OLC_POINT_BOOST) | `OlcPointBoost %d %d %d %d` | — | ❌ MISSING | ❌ MISSING | **Entire OLC point boost chain not serialized in JSON** |
| `progs` | `AreaProg ...` | `"progs"` | ✅ | ✅ | |
| `index_vars` | `olc_save_index_vars()` | `"index_vars"` | ✅ | ✅ | |
| `trade_list` | `save_area_trade()` | `"trade"` | ✅ | ✅ | |
| `wilds` | `save_wilds()` | `"wilderness"` | ✅ | ✅ | |

### Summary: AREA_DATA Gaps
- **`place_flags`** — Old format saves it; JSON has a comment saying "skip for now"
- **`open`** — Old format saves it; completely absent from JSON
- **`min_level` / `max_level`** — Old format saves these fields; JSON saves **different** fields (`low_range`/`high_range`). This is a data mismatch — areas loaded from old format will have `min_level`/`max_level` set, but when re-saved as JSON, they'll get `low_range`/`high_range` instead
- **`OlcPointBoost`** — Entire linked list of OLC point boosts (category/usage/imp/area) is not serialized in JSON

---

## 2. MOB_INDEX_DATA

**JSON writer**: `json_area_serialize_mobile()` — [json_area.c](io/json/json_area.c#L2370)
**JSON reader**: `json_area_deserialize_mobile()` — [json_area.c](io/json/json_area.c#L2580)
**Old writer**: `save_mobile_new()` — [olc_save.c](olc_save.c#L840)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `vnum` | `#MOBILE %ld` | `"vnum"` | ✅ | ✅ | |
| `player_name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `short_descr` | `ShortDesc %s~` | `"short_descr"` | ✅ | ✅ | |
| `long_descr` | `LongDesc %s~` | `"long_descr"` | ✅ | ✅ | |
| `description` | `Description %s~` | `"description"` | ✅ | ✅ | |
| `owner` | `Owner %s~` | `"owner"` | ✅ | ✅ | |
| `sig` | `ImpSig %s~` | `"imp_sig"` | ✅ | ✅ | |
| `creator_sig` | `CreatorSig %s~` | `"creator_sig"` | ✅ | ✅ | |
| `persist` | `Persist` | `"persist"` | ✅ | ✅ | |
| `skeywds` | `Skeywds %s~` | `"script_keywords"` | ✅ | ✅ | |
| `comments` | `Comments %s~` | `"comments"` | ✅ | ✅ | |
| `race` | `Race %s~` | `"race"` | ✅ | ✅ | |
| `act[0]` | `Act %ld` | `"act_flags"` (array) | ✅ | ✅ | Old ORs race act; JSON saves raw |
| `act[1]` | `Act2 %ld` | `"act2_flags"` (array) | ✅ | ✅ | |
| `affected_by[0]` | `Affected_by %ld` | `"affected_by"` (array) | ✅ | ✅ | Old ORs race aff; JSON saves raw |
| `affected_by[1]` | `Affected_by2 %ld` | `"affected_by2"` (array) | ✅ | ✅ | |
| `level` | `Level %d` | `"level"` | ✅ | ✅ | |
| `alignment` | `Alignment %d` | `"alignment"` | ✅ | ✅ | |
| `hitroll` | `Hitroll %d` | `"hitroll"` | ✅ | ✅ | |
| `hit` (dice) | `Hit %d %d %d` | `"hit_dice"` (object) | ✅ | ✅ | |
| `mana` (dice) | `Mana %d %d %d` | `"mana_dice"` (object) | ✅ | ✅ | |
| `damage` (dice) | `Damage %d %d %d` | `"damage_dice"` (object) | ✅ | ✅ | |
| `dam_type` | `AttackType %d` | `"dam_type"` | ✅ | ✅ | |
| `attacks` | `Attacks %d` | `"attacks"` | ✅ | ✅ | |
| `off_flags` | `OffFlags %ld` | `"off_flags"` (array) | ✅ | ✅ | |
| `imm_flags` | `ImmFlags %ld` | `"imm_flags"` (array) | ✅ | ✅ | |
| `res_flags` | `ResFlags %ld` | `"res_flags"` (array) | ✅ | ✅ | |
| `vuln_flags` | `VulnFlags %d` | `"vuln_flags"` (array) | ✅ | ✅ | |
| `start_pos` | `StartPos %d` | `"start_pos"` | ✅ | ✅ | |
| `default_pos` | `DefaultPos %d` | `"default_pos"` | ✅ | ✅ | |
| `wealth` | `Wealth %ld` | `"wealth"` | ✅ | ✅ | |
| `body_type` | `BodyType %d` (×2) | `"body_type"` | ✅ | ✅ | Old saves it twice |
| `parts` | `Parts %ld` | `"parts"` | ✅ | ✅ | |
| `size` | `Size %d` | `"size"` | ✅ | ✅ | |
| `move` | `Movement %ld` | `"movement"` | ✅ | ✅ | |
| `material` | `Material %s~` | `"material"` | ✅ | ✅ | |
| `corpse_type` | `CorpseType %ld` | `"corpse_type"` | ✅ | ✅ | |
| `corpse_load.vnum` | `CorpseVnum %ld` | `"corpse_vnum"` | ✅ | ✅ | |
| `zombie_load.vnum` | `CorpseZombie %ld` | `"zombie_vnum"` | ✅ | ✅ | |
| `boss` | `Boss` | `"boss"` | ✅ | ✅ | |
| `pronoun_he_she` | `PronounSS %s~` | `"pronoun_he_she"` | ✅ | ✅ | |
| `pronoun_him_her` | `PronounOS %s~` | `"pronoun_him_her"` | ✅ | ✅ | |
| `pronoun_his_her` | `PronounPAS %s~` | `"pronoun_his_her"` | ✅ | ✅ | |
| `pronoun_his_hers` | `PronounPPS %s~` | `"pronoun_his_hers"` | ✅ | ✅ | |
| `pronoun_himself_herself` | `PronounRS %s~` | `"pronoun_himself_herself"` | ✅ | ✅ | |
| `verb_preference` | `VerbPref %d` | `"verb_preference"` | ✅ | ✅ | |
| `spec_fun` | `SpecFun %s~` | `"spec_fun"` | ✅ | ✅ | |
| `progs` | `MobProg ...` | `"progs"` | ✅ | ✅ | |
| `pShop` | `save_shop_new()` | `"shop"` | ✅ | ✅ | |
| `index_vars` | `olc_save_index_vars()` | `"index_vars"` | ✅ | ✅ | |
| **`pQuestor`** | `save_questor_new()` | — | ❌ MISSING | ❌ MISSING | **TODO: "Questor, Crew"** |
| **`pCrew`** | `save_ship_crew_index_new()` | — | ❌ MISSING | ❌ MISSING | **TODO: "Questor, Crew"** |

### Summary: MOB_INDEX_DATA Gaps
- **`pQuestor`** — Full questor data (scroll, keywords, short/long descr, header, footer, prefix, suffix, line_width) saved in old format; **completely missing from JSON**. Explicit TODO comment.
- **`pCrew`** — Ship crew index data (min_rank, scouting, gunning, oarring, mechanics, navigation, leadership) saved in old format; **completely missing from JSON**. Explicit TODO comment.
- Old format ORs race flags into `act[0]` and `affected_by[0]` before saving; JSON saves the raw mob flags without race application (JSON behavior is arguably more correct).

---

## 3. OBJ_INDEX_DATA

**JSON writer**: `json_area_serialize_object()` — [json_area.c](io/json/json_area.c#L2684)
**JSON reader**: `json_area_deserialize_object()` — [json_area.c](io/json/json_area.c#L2870)
**Old writer**: `save_object_new()` — [olc_save.c](olc_save.c#L947)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `vnum` | `#OBJECT %ld` | `"vnum"` | ✅ | ✅ | |
| `name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `short_descr` | `ShortDesc %s~` | `"short_descr"` | ✅ | ✅ | |
| `description` | `LongDesc %s~` | `"long_descr"` | ✅ | ✅ | |
| `full_description` | `Description %s~` | `"description"` | ✅ | ✅ | |
| `material` | `Material %s~` | `"material"` | ✅ | ✅ | |
| `imp_sig` | `ImpSig %s~` | `"imp_sig"` | ✅ | ✅ | |
| `persist` | `Persist` | `"persist"` | ✅ | ✅ | |
| `creator_sig` | `CreatorSig %s~` | `"creator_sig"` | ✅ | ✅ | |
| `skeywds` | `SKeywds %s~` | `"script_keywords"` | ✅ | ✅ | |
| `comments` | `Comments %s~` | `"comments"` | ✅ | ✅ | |
| `times_allowed_fixed` | `TimesAllowedFixed %d` | `"times_allowed_fixed"` | ✅ | ✅ | |
| `fragility` | `Fragility %d` | `"fragility"` | ✅ | ✅ | |
| `points` | `Points %d` | `"points"` | ✅ | ✅ | |
| `update` | `Update %d` | `"update"` | ✅ | ✅ | |
| `timer` | `Timer %d` | `"timer"` | ✅ | ✅ | |
| `item_type` | `ItemType %s~` | `"item_type"` (string) | ✅ | ✅ | Both use string name |
| `extra[0]` | `ExtraFlags %ld` | `"extra_flags"` (array) | ✅ | ✅ | Old=integer, JSON=flag array |
| `extra[1]` | `Extra2Flags %ld` | `"extra2_flags"` (array) | ✅ | ✅ | |
| `extra[2]` | `Extra3Flags %ld` | `"extra3_flags"` (array) | ✅ | ✅ | |
| `extra[3]` | `Extra4Flags %ld` | `"extra4_flags"` (array) | ✅ | ✅ | |
| `wear_flags` | `WearFlags %ld` | `"wear_flags"` (array) | ✅ | ✅ | Old=integer, JSON=flag array |
| `value[0..7]` | `Values %ld×8` | `"values"` (array) | ✅ | ✅ | |
| `level` | `Level %d` | `"level"` | ✅ | ✅ | |
| `weight` | `Weight %d` | `"weight"` | ✅ | ✅ | |
| `cost` | `Cost %ld` | `"cost"` | ✅ | ✅ | |
| `condition` | `Condition %d` | `"condition"` | ✅ | ✅ | |
| `waypoints` | `MapWaypoint ...` | `"waypoints"` (array) | ✅ | ✅ | |
| `affected` | `#AFFECT ...` | `"affects"` (array) | ✅ | ✅ | |
| `catalyst` | `#CATALYST ...` | `"catalysts"` (array) | ✅ | ✅ | |
| `extra_descr` | `#EXTRA_DESCR ...` | `"extra_descrs"` (array) | ✅ | ✅ | |
| `lock` | `Lock %ld %d %d` | `"lock"` (object) | ✅ | ✅ | JSON uses widevnum for key |
| `progs` | `ObjProg ...` | `"progs"` | ✅ | ✅ | |
| `index_vars` | `olc_save_index_vars()` | `"index_vars"` | ✅ | ✅ | |
| **`spells`** | `save_spell()` + old value conversion | — | ❌ MISSING | ❌ MISSING | **TODO: "Spells"** |

### Summary: OBJ_INDEX_DATA Gaps
- **`spells`** — The old format saves `SPELL_DATA` via `save_spell()` (writes `SpellNew %s~ %d %d`) AND performs legacy value-to-spell conversion for weapons, armor, lights, artifacts, scrolls, pills, potions, wands, and staves. **The JSON area serializer does not serialize spells at all.** Explicit TODO comment in code.
- Note: `json_persist_object_to_json()` in `json_persist.c` **does** serialize spells for runtime objects — only the OBJ_INDEX_DATA (area/template) serializer is missing it.

---

## 4. SCRIPT_DATA

**JSON writer**: `json_area_serialize_script()` — [json_area.c](io/json/json_area.c#L3006)
**JSON reader**: `json_area_deserialize_script()` — [json_area.c](io/json/json_area.c#L3048)
**Old writer**: `save_script_new()` — [olc_save.c](olc_save.c#L1193)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `vnum` | `#%sPROG %ld` | `"vnum"` | ✅ | ✅ | |
| `name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `edit_src` / `src` | `Code %s~` | `"code"` | ✅ | ✅ | Both prefer `edit_src` |
| `flags` | `Flags %s~` | `"flags"` (string) | ✅ | ✅ | Both use `flag_string()` |
| `depth` | `Depth %d` | `"depth"` | ✅ | ✅ | |
| `security` | `Security %d` | `"security"` | ✅ | ✅ | |
| **`run_security`** | — | `"run_security"` | ✅ | ✅ | **Only in JSON, NOT in old format** |
| `comments` | `Comments %s~` | `"comments"` | ✅ | ✅ | |

### Summary: SCRIPT_DATA Gaps
- **`run_security`** — Present in JSON but **NOT in old format**. This is the reverse gap: scripts loaded from old `.are` files will lose their `run_security` value. However, no data loss occurs on migration since old files never had this field.
- No TODO comments. This serializer is essentially complete.

---

## 5. SHIP_INDEX_DATA

**JSON writer**: `json_area_serialize_ship()` — [json_area.c](io/json/json_area.c#L4687)
**JSON reader**: `json_area_deserialize_ship()` — [json_area.c](io/json/json_area.c#L4740) (approx)
**Old writer**: `save_ship_index()` — only in `src_20_dev/boat.c` (removed from current codebase)

| Struct Field | Old Key (src_20_dev) | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `vnum` | Part of `#SHIP %ld` | `"vnum"` | ✅ | ✅ | |
| `name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `description` | `Description %s~` | `"description"` | ✅ | ✅ | |
| `ship_class` | `ShipClass %d` | `"ship_class"` | ✅ | ✅ | |
| `flags` | `Flags %d` | `"flags"` | ✅ | ✅ | |
| `blueprint_ref` | `Blueprint %ld` | `"blueprint"` (widevnum) | ✅ | ✅ | |
| `ship_object_ref` | `ShipObject %ld` | `"ship_object"` (widevnum) | ✅ | ✅ | |
| `hit` | `Hit %d` | `"hit"` | ✅ | ✅ | |
| `guns` | `Guns %d` | `"guns"` | ✅ | ✅ | |
| `min_crew` | `MinCrew %d` | `"min_crew"` | ✅ | ✅ | |
| `max_crew` | `MaxCrew %d` | `"max_crew"` | ✅ | ✅ | |
| `move_delay` | `MoveDelay %d` | `"move_delay"` | ✅ | ✅ | |
| `move_steps` | `MoveSteps %d` | `"move_steps"` | ✅ | ✅ | |
| `turning` | `Turning %d` | `"turning"` | ✅ | ✅ | |
| `weight` | `Weight %d` | `"weight"` | ✅ | ✅ | |
| `capacity` | `Capacity %d` | `"capacity"` | ✅ | ✅ | |
| `armor` | `Armor %d` | `"armor"` | ✅ | ✅ | |
| `oars` | `Oars %d` | `"oars"` | ✅ | ✅ | |
| `special_keys` (LLIST) | `SpecialKey %ld` | `"special_keys"` (array) | ✅ | ✅ | JSON uses widevnum array |

### Summary: SHIP_INDEX_DATA
- No gaps identified. Old-format save is no longer in the current codebase (ships are JSON-only now).
- The JSON serializer appears complete for all fields in the struct.

---

## 6. BLUEPRINT / BLUEPRINT_SECTION

**JSON writer (section)**: `json_area_serialize_blueprint_section()` — [json_area.c](io/json/json_area.c#L4297)
**JSON writer (blueprint)**: `json_area_serialize_blueprint()` — [json_area.c](io/json/json_area.c#L4350)
**Old writer**: `save_blueprints()` — only in `src_20_dev/blueprint.c` (removed from current codebase)

### BLUEPRINT_SECTION_DATA

| Struct Field | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|
| `vnum` | `"vnum"` | ✅ | ✅ | |
| `name` | `"name"` | ✅ | ✅ | |
| `description` | `"description"` | ✅ | ✅ | |
| `comments` | `"comments"` | ✅ | ✅ | |
| `type` | `"type"` | ✅ | ✅ | |
| `flags` | `"flags"` | ✅ | ✅ | |
| `lower_vnum` | `"lower_vnum"` | ✅ | ✅ | |
| `upper_vnum` | `"upper_vnum"` | ✅ | ✅ | |
| `recall_room` / `recall_ref` | `"recall_room"` (widevnum) | ✅ | ✅ | |
| `links` (LLIST) | `"links"` (array of objects) | ✅ | ✅ | Each: name, room (widevnum), door |

### BLUEPRINT_DATA

| Struct Field | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|
| `vnum` | `"vnum"` | ✅ | ✅ | |
| `name` | `"name"` | ✅ | ✅ | |
| `description` | `"description"` | ✅ | ✅ | |
| `comments` | `"comments"` | ✅ | ✅ | |
| `flags` | `"flags"` | ✅ | ✅ | |
| `mode` | `"mode"` | ✅ | ✅ | |
| `repop` | `"repop"` | ✅ | ✅ | |
| `area_who` | `"area_who"` | ✅ | ✅ | |
| `sections` (LLIST) | `"sections"` (vnum array) | ✅ | ✅ | Saves section vnums |
| `_static.recall` | `"static"."recall"` | ✅ | ✅ | Conditional on mode |
| `_static.entries` | `"static"."entries"` | ✅ | ✅ | |
| `_static.exits` | `"static"."exits"` | ✅ | ✅ | |
| `_static.layout` | `"static"."layout"` | ✅ | ✅ | |
| `special_rooms` | `"special_rooms"` | ✅ | ✅ | |
| `progs` | `"progs"` | ✅ | ✅ | |
| `index_vars` | `"index_vars"` | ✅ | ✅ | |

### Summary: BLUEPRINT Gaps
- No gaps identified. Old-format save is no longer in the current codebase (blueprints are JSON-only now).
- The JSON serializer appears complete for all struct fields.

---

## 7. DUNGEON_INDEX_DATA

**JSON writer**: `json_area_serialize_dungeon()` — [json_area.c](io/json/json_area.c#L4517)
**JSON reader**: `json_area_deserialize_dungeon()` — [json_area.c](io/json/json_area.c#L4590) (approx)
**Old writer**: `save_dungeons()` — only in `src_20_dev/dungeon.c` (removed from current codebase)

| Struct Field | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|
| `vnum` | `"vnum"` | ✅ | ✅ | |
| `name` | `"name"` | ✅ | ✅ | |
| `description` | `"description"` | ✅ | ✅ | |
| `comments` | `"comments"` | ✅ | ✅ | |
| `area_who` | `"area_who"` | ✅ | ✅ | |
| `repop` | `"repop"` | ✅ | ✅ | |
| `flags` | `"flags"` | ✅ | ✅ | |
| zone_out | `"zone_out"` (widevnum) | ✅ | ✅ | |
| portal_out | `"portal_out"` (widevnum) | ✅ | ✅ | |
| mount_out | `"mount_out"` (widevnum) | ✅ | ✅ | |
| entry_room | `"entry_room"` (widevnum) | ✅ | ✅ | |
| exit_room | `"exit_room"` (widevnum) | ✅ | ✅ | |
| floors (LLIST) | `"floors"` (vnum array) | ✅ | ✅ | Blueprint vnum array |
| levels | `"levels"` (array) | ✅ | ✅ | mode, floor, weighted_floors |
| special_rooms | `"special_rooms"` | ✅ | ✅ | |
| **special_exits** | `"special_exits"` | ⚠️ Partial | ⚠️ Partial | **Comment: "simplified here"** — only saves destination widevnum per exit, not full exit data |
| dungeon_progs | `"dungeon_progs"` | ✅ | ✅ | |
| index_vars | `"index_vars"` | ✅ | ✅ | |

### Summary: DUNGEON_INDEX_DATA Gaps
- **`special_exits`** — Serialization is "simplified here" per code comment. Only destination widevnums are saved; full exit data (keywords, descriptions, flags, locks, door data) is not preserved.
- Old-format save is no longer in the current codebase (dungeons are JSON-only now).

---

## 8. Runtime Persistence (Instance/Ship/Dungeon + Objects/Mobiles/Rooms)

### 8a. Runtime Object Persistence (OBJ_DATA)

**JSON writer**: `json_persist_object_to_json()` — [json_persist.c](io/json/json_persist.c#L696)
**Old writer**: `persist_save_object()` — [db.c](../db.c#L6658)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `pIndexData->vnum` | `#OBJECT %ld` | `"vnum"` (widevnum) | ✅ | ✅ | JSON uses widevnum string |
| `id[0]` | `UID %ld` | `"id0"` | ✅ | ✅ | |
| `id[1]` | `UID2 %ld` | `"id1"` | ✅ | ✅ | |
| `persist` | `Persist` | `"persist"` | ✅ | ✅ | |
| `version` | `Version %d` | `"json_version"` + `"version"` | ✅ | ✅ | |
| `name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `short_descr` | `ShortDesc %s~` | `"short_descr"` | ✅ | ✅ | |
| `description` | `LongDesc %s~` | `"description"` | ✅ | ✅ | |
| `full_description` | `FullDesc %s~` | `"full_description"` | ✅ | ✅ | |
| `extra[0..3]` | `Extra`/`Extra2`/`Extra3`/`Extra4` | `"extra"` (array) | ✅ | ✅ | |
| `extra_perm[0..3]` | `PermExtra`/etc. | `"extra_perm"` (array) | ✅ | ✅ | |
| `weapon_flags_perm` | `PermWeapon %ld` | `"weapon_flags_perm"` | ✅ | ✅ | Conditional on ITEM_WEAPON |
| `wear_flags` | `WearFlags %d` | `"wear_flags"` | ✅ | ✅ | |
| `item_type` | `ItemType %d` | `"item_type"` | ✅ | ✅ | |
| `in_room` (location) | `Room`/`Vroom`/`CloneRoom` | `"location"` (object) | ✅ | ✅ | JSON uses typed sub-object |
| `num_enchanted` | `Enchanted %d` | `"num_enchanted"` | ✅ | ✅ | |
| `weight` | `Weight %d` | `"weight"` | ✅ | ✅ | |
| `condition` | `Cond %d` | `"condition"` | ✅ | ✅ | |
| `times_fixed` | `Fixed %d` | `"times_fixed"` | ✅ | ✅ | |
| `owner` | `Owner %s~` | `"owner"` | ✅ | ✅ | |
| `old_name` | `OldName %s~` | `"old_name"` | ✅ | ✅ | |
| `old_short_descr` | `OldShort %s~` | `"old_short_descr"` | ✅ | ✅ | |
| `old_description` | `OldDescr %s~` | `"old_description"` | ✅ | ✅ | |
| `old_full_description` | `OldFullDescr %s~` | `"old_full_description"` | ✅ | ✅ | |
| `loaded_by` | `LoadedBy %s~` | `"loaded_by"` | ✅ | ✅ | |
| `fragility` | `Fragility %d` | `"fragility"` | ✅ | ✅ | |
| `times_allowed_fixed` | `TimesAllowedFixed %d` | `"times_allowed_fixed"` | ✅ | ✅ | |
| `locker` | `Locker` | `"locker"` | ✅ | ✅ | |
| `wear_loc` | `WearLoc %d` | `"wear_loc"` | ✅ | ✅ | |
| `last_wear_loc` | `LastWearLoc %d` | `"last_wear_loc"` | ✅ | ✅ | |
| `level` | `Level %d` | `"level"` | ✅ | ✅ | |
| `timer` | `Timer %d` | `"timer"` | ✅ | ✅ | |
| `cost` | `Cost %ld` | `"cost"` | ✅ | ✅ | |
| `value[0..7]` | `Value %d %ld` | `"values"` (array) | ✅ | ✅ | |
| `lock` | `Lock %ld '%s' %d` | `"lock"` (object) | ✅ | ✅ | |
| `waypoints` | `MapWaypoint ...` | `"waypoints"` (array) | ✅ | ✅ | |
| `spells` | `save_spell()` | `"spells"` (array) | ✅ | ✅ | **Persist serializer DOES include spells** |
| `affected` | `AffObjSk`/`AffObjNm`/`AffMob` | `"affects"` (array) | ✅ | ✅ | |
| `catalyst` | `CataA`/`Cata`/`CataNA`/`CataN` | `"catalysts"` (array) | ✅ | ✅ | |
| `extra_descr` | `ExDe`/`ExDeEnv` | `"extra_descr"` (array) | ✅ | ✅ | |
| `owner_name` | `OwnerName %s~` | `"owner_name"` | ✅ | ✅ | |
| `owner_short` | `OwnerShort %s~` | `"owner_short"` | ✅ | ✅ | |
| `progs` | `persist_save_scriptdata()` | `"variables"` | ✅ | ✅ | |
| `tokens` | `persist_save_token()` | `"tokens"` (array) | ✅ | ✅ | |
| `contains` | `persist_save_object()` (recursive) | `"contains"` (array) | ✅ | ✅ | |

### Summary: Runtime Object Gaps
- No significant gaps found. The JSON persist object serializer is comprehensive and **includes spells** (unlike the area serializer).

---

### 8b. Runtime Mobile Persistence (CHAR_DATA — NPCs)

**JSON writer**: `json_persist_mobile_to_json()` — [json_persist.c](io/json/json_persist.c#L1307)
**Old writer**: `persist_save_mobile()` — [db.c](../db.c#L6914)

| Struct Field | Old Key | JSON Key | In JSON Writer | In JSON Reader | Notes |
|---|---|---|---|---|---|
| `pIndexData->vnum` | `#MOBILE %ld` | `"vnum"` (widevnum) | ✅ | ✅ | |
| `id[0]`/`id[1]` | `UID`/`UID2` | `"id0"`/`"id1"` | ✅ | ✅ | |
| `persist` | `Persist` | `"persist"` | ✅ | ✅ | |
| `version` | `Version %d` | `"version"` | ✅ | ✅ | |
| `dead` / `time_left_death` | `Dead` / `DeathTimeLeft` | `"dead"` / `"death_time_left"` | ✅ | ✅ | |
| `recall` (LOCATION) | `RepopRoom(W/C)` | `"recall"` | ✅ | ✅ | |
| `name` | `Name %s~` | `"name"` | ✅ | ✅ | |
| `owner` | `Owner %s~` | `"owner"` | ✅ | ✅ | |
| `short_descr` | `ShD %s~` | `"short_descr"` | ✅ | ✅ | |
| `long_descr` | `LnD %s~` | `"long_descr"` | ✅ | ✅ | |
| `description` | `Desc %s~` | `"description"` | ✅ | ✅ | |
| `race` | `Race %s~` | `"race"` | ✅ | ✅ | |
| `sex` | `Sex %d` | `"sex"` | ✅ | ✅ | |
| `level` | `Levl %d` | `"level"` | ✅ | ✅ | |
| `tot_level` | `TLevl %d` | `"tot_level"` | ✅ | ✅ | |
| `in_room` (location) | `Room`/`Vroom`/`CloneRoom` | `"location"` | ✅ | ✅ | |
| `toxin[i]` | `Toxn* %d` | `"toxins"` (array) | ✅ | ✅ | |
| `hit`/`max_hit`/etc. | `HMV %ld×6` | six separate keys | ✅ | ✅ | |
| `manastore` | `ManaStore %d` | `"manastore"` | ✅ | ✅ | |
| `gold`/`silver` | `Gold`/`Silv` | `"gold"`/`"silver"` | ✅ | ✅ | |
| `pneuma` | `Pneuma %ld` | `"pneuma"` | ✅ | ✅ | |
| `home` | `Home %ld` | `"home"` | ✅ | ✅ | |
| `questpoints` | `QuestPnts %d` | `"questpoints"` | ✅ | ✅ | |
| `deitypoints` | `DeityPnts %ld` | `"deitypoints"` | ✅ | ✅ | |
| `exp` | `Exp %ld` | `"exp"` | ✅ | ✅ | |
| `act[0]`/`act[1]` | `Act`/`Act2` | `"act0"`/`"act1"` | ✅ | ✅ | |
| `affected_by[0]`/`[1]` | `AfBy`/`AfBy2` | `"affected_by0"`/`"affected_by1"` | ✅ | ✅ | |
| `off_flags` | `OffFlags` | `"off_flags"` | ✅ | ✅ | |
| `imm_flags` | `Immune` | `"imm_flags"` | ✅ | ✅ | |
| `imm_flags_perm` | `ImmunePerm` | `"imm_flags_perm"` | ✅ | ✅ | |
| `res_flags` | `Resist` | `"res_flags"` | ✅ | ✅ | |
| `res_flags_perm` | `ResistPerm` | `"res_flags_perm"` | ✅ | ✅ | |
| `vuln_flags` | `Vuln` | `"vuln_flags"` | ✅ | ✅ | |
| `vuln_flags_perm` | `VulnPerm` | `"vuln_flags_perm"` | ✅ | ✅ | |
| `start_pos`/`default_pos` | `StartPos`/`DefaultPos` | same | ✅ | ✅ | |
| `position` | `Pos %d` | `"position"` | ✅ | ✅ | |
| `parts` | `Parts %ld` | `"parts"` | ✅ | ✅ | |
| `size` | `Size %d` | `"size"` | ✅ | ✅ | |
| `material` | `Material %s~` | `"material"` | ✅ | ✅ | |
| `corpse_type` | `CorpseType` | `"corpse_type"` | ✅ | ✅ | |
| `corpse_load.vnum` | `CorpseVnum` | `"corpse_vnum"` | ✅ | ✅ | |
| `comm` | `Comm` | `"comm"` | ✅ | ✅ | |
| `practice` | `Prac` | `"practice"` | ✅ | ✅ | |
| `train` | `Trai` | `"train"` | ✅ | ✅ | |
| `saving_throw` | `Save` | `"saving_throw"` | ✅ | ✅ | |
| `alignment` | `Alig` | `"alignment"` | ✅ | ✅ | |
| `hitroll` | `Hit` | `"hitroll"` | ✅ | ✅ | |
| `damroll` | `Dam` | `"damroll"` | ✅ | ✅ | |
| `armour[0..3]` | `ACs %d %d %d %d` | `"armour"` (array) | ✅ | ✅ | |
| `wimpy` | `Wimp` | `"wimpy"` | ✅ | ✅ | |
| `perm_stat[0..4]` | `Attr %d×5` | `"perm_stat"` (array) | ✅ | ✅ | |
| `mod_stat[0..4]` | `AMod %d×5` | `"mod_stat"` (array) | ✅ | ✅ | |
| `lostparts` | `LostParts` | `"lostparts"` | ✅ | ✅ | |
| `affected` | `Affcg`/`Affcgn` | `"affects"` (array) | ✅ | ✅ | |
| `shop` | `save_shop_new()` | `"has_shop"` (flag) | ⚠️ | ⚠️ | JSON stores only a boolean flag; relies on mob index to have shop. Old format serializes full shop data. |
| `crew` | `save_ship_crew()` | `"has_crew"` (flag) | ⚠️ | ⚠️ | JSON stores only a boolean flag; relies on mob index. Old format serializes full crew data. |
| `progs` | `persist_save_scriptdata()` | `"variables"` | ✅ | ✅ | |
| `tokens` | `persist_save_token()` | `"tokens"` (array) | ✅ | ✅ | |
| `lcarrying` | `persist_save_object()` | `"carrying"` (array) | ✅ | ✅ | |
| `lworn` | (implicit in old) | `"worn"` (array) | ✅ | ✅ | JSON separates carried/worn explicitly |

### Summary: Runtime Mobile Gaps
- **`shop`** / **`crew`** — JSON stores only a boolean flag (`"has_shop": true`, `"has_crew": true`) and relies on the mob index to still have the data. Old format serializes full shop/crew data inline. This means if the mob index changes, runtime persist data won't match.
- Otherwise comprehensive.

---

### 8c. Instance / Ship / Dungeon Runtime

**JSON instance writer**: `instance_to_json()` — [json_instance.c](io/json/json_instance.c#L325)
**JSON ship writer**: `ship_to_json()` — [json_instance.c](io/json/json_instance.c#L501)
**JSON dungeon writer**: `dungeon_to_json()` — [json_instance.c](io/json/json_instance.c#L720)

These are JSON-only (no old-format equivalent for instances). The old-format persist functions in `db.c` handle individual rooms/objects/mobiles but not the instance/ship/dungeon containers.

**Ship TODO**: Line ~500 area in `json_instance.c` has `/* TODO: Add steering, crew, objects, etc. */` indicating ship runtime state doesn't save:
- Steering state
- Crew members (individual mobs assigned as crew)
- Object cargo

**Dungeon special_exits**: Same issue as the index — simplified serialization.

---

## Overall Gap Summary

### Critical (data loss on save/load cycle)

| Entity | Missing Field | Impact |
|---|---|---|
| **MOB_INDEX_DATA** | `pQuestor` | Quest vendor configuration completely lost when saving to JSON |
| **MOB_INDEX_DATA** | `pCrew` | Ship crew index data lost when saving to JSON |
| **OBJ_INDEX_DATA** | `spells` (area/template) | Object template spells lost; legacy value-to-spell conversion also not performed |
| **AREA_DATA** | `open` | Area open flag lost |
| **AREA_DATA** | `place_flags` | Place type flags lost |
| **AREA_DATA** | `OlcPointBoost` | OLC point boost records lost |
| **AREA_DATA** | `min_level` / `max_level` | Wrong fields stored — `low_range`/`high_range` saved instead |

### Minor (design decision, no immediate data loss)

| Entity | Field | Notes |
|---|---|---|
| **SCRIPT_DATA** | `run_security` | Only in JSON; old format never had it, so no loss on migration |
| **DUNGEON_INDEX_DATA** | `special_exits` | Simplified serialization — only destinations, not full exit data |
| **SHIP_DATA** (runtime) | steering, crew members, cargo | TODO in json_instance.c |
| **CHAR_DATA** (persist) | `shop`/`crew` | JSON stores flag only vs. old format inlining full data |

### Explicit TODO Comments in Code

1. [json_area.c](io/json/json_area.c#L2583) line ~2583: `// TODO: Questor, Crew` (in `json_area_serialize_mobile`)
2. [json_area.c](io/json/json_area.c#L2867) line ~2867: `// TODO: Spells` (in `json_area_serialize_object`)
3. [json_area.c](io/json/json_area.c#L4380) comment in `json_area_serialize_metadata`: `/* place_type not in current structure - skip for now */`
4. [json_instance.c](io/json/json_instance.c#L500) (approx): `/* TODO: Add steering, crew, objects, etc. */` (in `ship_to_json`)
5. [json_area.c](io/json/json_area.c#L4560) (approx): `/* special_exits - simplified here */` (in `json_area_serialize_dungeon`)
