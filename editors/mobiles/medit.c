/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../mxp_links.h"
#include "../../strings.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../common/olc_field_handlers.h"
#include "../common/olc_staged.h"
#include "../common/olc_changeset.h"
#include "../../skill_data.h"

/* Forward declarations for tab show functions */
static void medit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_combat_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_defense_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_economy_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_special_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void medit_show_inheritance_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static AREA_DATA *medit_get_area(void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    return pMob ? pMob->area : NULL;
}

/* Currently unused — will be called from commit-time hooks in Phase 2. */
static void medit_rebuild_auto_tags(MOB_INDEX_DATA *pMob)
{
    if (!pMob)
        return;

    free_string(pMob->auto_tags);
    pMob->auto_tags = short_to_name(pMob->player_name);
}

/*
 * Mobile Editor Command Table
 */
const struct olc_cmd_type medit_table[] =
{
    {   "?",            show_help       },
    {   "act",          medit_act       },
    {   "addmprog",     medit_addmprog  },
    {   "addquest",     medit_addquest   },
    {   "addreputation",medit_addreputation },
    {   "affect",       medit_affect    },
    {   "alignment",    medit_align     },
    {   "armour",       medit_ac        },
    {   "attacks",      medit_attacks   },
    {   "commands",     show_commands   },
    {   "comments",     medit_comments  },
    {   "create",       medit_create    },
    {   "damdice",      medit_damdice   },
    {   "damtype",      medit_damtype   },
    {   "delmprog",     medit_delmprog  },
    {   "delquest",     medit_delquest   },
    {   "delreputation",medit_delreputation },
    {   "description",  medit_desc      },
    {   "hitdice",      medit_hitdice   },
    {   "hitroll",      medit_hitroll   },
    {   "immune",       medit_immune    },
    {   "level",        medit_level     },
    {   "long",         medit_long      },
    {   "manadice",     medit_manadice  },
    {   "material",     medit_material  },
    {   "movedice",     medit_movedice  },
    {   "name",         medit_name      },
    {   "next",         medit_next      },
    {   "off",          medit_off       },
    {   "owner",        medit_owner     },
    {   "parent",       medit_parent    },
    {   "tags",         medit_tags      },
    {   "listname",     medit_listname  },
    {   "listkeywords", medit_listkeywords },
    {   "part",         medit_part      },
    {   "persist",      medit_persist   },
    {   "position",     medit_position  },
    {   "prev",         medit_prev      },
    {   "questor",      medit_questor   },
    {   "trainer",      medit_trainer   },
    {   "crew",         medit_crew      },
    {   "boss",         medit_boss      },
    {   "bodytype",     medit_bodytype  },
    {   "race",         medit_race      },
    {   "res",          medit_res       },
    {   "sex",          medit_sex       },
    {   "shop",         medit_shop      },
    {   "short",        medit_short     },
    {   "show",         medit_show      },
    {   "sign",         medit_sign      },
    {   "size",         medit_size      },
    {   "spec",         medit_spec      },
    {   "pronounss",    medit_pronounss },
    {   "pronounos",    medit_pronounos },
    {   "pronounpas",   medit_pronounpas },
    {   "pronounpps",   medit_pronounpps },
    {   "pronounrs",    medit_pronounrs },
    {   "vuln",         medit_vuln      },
    {   "wealth",       medit_gold      },
    {   "scriptkwd",    medit_skeywds   },
    {   "varset",       medit_varset    },
    {   "varclear",     medit_varclear  },
    {   "corpsetype",   medit_corpsetype },
    {   "corpsevnum",   medit_corpsevnum },
    {   "zombievnum",   medit_zombievnum },
    {   NULL,           0,              }
};

/*
 * Scalar field apply functions — generated via macros.
 */
OLC_FIELD_APPLY_STRING(medit_apply_owner,               MOB_INDEX_DATA, owner)
OLC_FIELD_APPLY_INT16 (medit_apply_alignment,           MOB_INDEX_DATA, alignment)
OLC_FIELD_APPLY_STRING(medit_apply_description,         MOB_INDEX_DATA, description)
OLC_FIELD_APPLY_STRING(medit_apply_comments,            MOB_INDEX_DATA, comments)
OLC_FIELD_APPLY_STRING(medit_apply_list_name,           MOB_INDEX_DATA, list_name)
OLC_FIELD_APPLY_STRING(medit_apply_list_keywords,       MOB_INDEX_DATA, list_keywords)
OLC_FIELD_APPLY_STRING(medit_apply_tags,                MOB_INDEX_DATA, tags)
OLC_FIELD_APPLY_INT   (medit_apply_corpse_type,         MOB_INDEX_DATA, corpse_type)
OLC_FIELD_APPLY_INT16 (medit_apply_sex,                 MOB_INDEX_DATA, sex)
OLC_FIELD_APPLY_STRING(medit_apply_pronoun_sub,         MOB_INDEX_DATA, pronoun_he_she)
OLC_FIELD_APPLY_STRING(medit_apply_pronoun_obj,         MOB_INDEX_DATA, pronoun_him_her)
OLC_FIELD_APPLY_STRING(medit_apply_pronoun_pos_adj,     MOB_INDEX_DATA, pronoun_his_her)
OLC_FIELD_APPLY_STRING(medit_apply_pronoun_pos,         MOB_INDEX_DATA, pronoun_his_hers)
OLC_FIELD_APPLY_STRING(medit_apply_pronoun_ref,         MOB_INDEX_DATA, pronoun_himself_herself)
OLC_FIELD_APPLY_FLAGS (medit_apply_form,                MOB_INDEX_DATA, form)
OLC_FIELD_APPLY_FLAGS (medit_apply_parts,               MOB_INDEX_DATA, parts)
OLC_FIELD_APPLY_FLAGS (medit_apply_imm_flags,           MOB_INDEX_DATA, imm_flags)
OLC_FIELD_APPLY_FLAGS (medit_apply_res_flags,           MOB_INDEX_DATA, res_flags)
OLC_FIELD_APPLY_FLAGS (medit_apply_vuln_flags,          MOB_INDEX_DATA, vuln_flags)
OLC_FIELD_APPLY_STRING(medit_apply_material,            MOB_INDEX_DATA, material)
OLC_FIELD_APPLY_FLAGS (medit_apply_off_flags,           MOB_INDEX_DATA, off_flags)
OLC_FIELD_APPLY_INT16 (medit_apply_size,                MOB_INDEX_DATA, size)
OLC_FIELD_APPLY_INT16 (medit_apply_hitroll,             MOB_INDEX_DATA, hitroll)
OLC_FIELD_APPLY_STRING(medit_apply_player_name,        MOB_INDEX_DATA, player_name)
OLC_FIELD_APPLY_STRING(medit_apply_short_descr,        MOB_INDEX_DATA, short_descr)
OLC_FIELD_APPLY_STRING(medit_apply_long_descr,         MOB_INDEX_DATA, long_descr)
OLC_FIELD_APPLY_STRING(medit_apply_skeywds,            MOB_INDEX_DATA, skeywds)
OLC_FIELD_APPLY_INT16 (medit_apply_dam_type,           MOB_INDEX_DATA, dam_type)
OLC_FIELD_APPLY_INT   (medit_apply_attacks,            MOB_INDEX_DATA, attacks)

/* Custom handler: body type changes also update sex and pronouns */
static bool medit_apply_body_type(void *entity, olc_pending_change_t *change)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!pMob || !change || !change->new_value || !json_is_integer(change->new_value))
        return false;

    int16_t val = (int16_t)json_integer_value(change->new_value);
    if (val < 0 || val >= BODY_TYPE_MAX)
        val = BODY_TYPE_NEUTRAL;

    body_type_t old_body = pMob->body_type;
    pMob->body_type = (body_type_t)val;

    if (pMob->body_type == BODY_TYPE_MALE)       pMob->sex = 1;
    else if (pMob->body_type == BODY_TYPE_FEMALE) pMob->sex = 2;
    else if (pMob->body_type == BODY_TYPE_RANDOM) pMob->sex = 3;
    else                                          pMob->sex = 0;

    if (old_body != pMob->body_type) {
        free_string(pMob->pronoun_he_she);
        pMob->pronoun_he_she = str_dup(body_type_info[pMob->body_type].default_he_she);
        free_string(pMob->pronoun_him_her);
        pMob->pronoun_him_her = str_dup(body_type_info[pMob->body_type].default_him_her);
        free_string(pMob->pronoun_his_her);
        pMob->pronoun_his_her = str_dup(body_type_info[pMob->body_type].default_his_her);
        free_string(pMob->pronoun_his_hers);
        pMob->pronoun_his_hers = str_dup(body_type_info[pMob->body_type].default_his_hers);
        free_string(pMob->pronoun_himself_herself);
        pMob->pronoun_himself_herself = str_dup(body_type_info[pMob->body_type].default_himself_herself);
        pMob->verb_preference = body_type_info[pMob->body_type].verb_preference;
    }

    return true;
}

/* Persist: use_imp_sig when enabling */
static bool medit_apply_persist(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pMob->persist, change)) return false;
    if (pMob->persist) use_imp_sig(pMob, NULL);
    return true;
}

/* Boss: use_imp_sig when enabling */
static bool medit_apply_boss(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_bool(&pMob->boss, change)) return false;
    if (pMob->boss) use_imp_sig(pMob, NULL);
    return true;
}

OLC_FIELD_APPLY_STRING(medit_apply_sign, MOB_INDEX_DATA, sig)

/* Gold: use_imp_sig on any gold change (original calls use_imp_sig unconditionally) */
static bool medit_apply_gold(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_long(&pMob->wealth, change)) return false;
    use_imp_sig(pMob, NULL);
    return true;
}

OLC_FIELD_APPLY_LONG(medit_apply_move, MOB_INDEX_DATA, move)

/* Level: cascade dice recalculation after apply */
static bool medit_apply_level(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (!olc_apply_generic_int16(&pMob->level, change)) return false;
    set_mob_hitdice(pMob);
    set_mob_damdice(pMob);
    if (!IS_SET(pMob->act[0], ACT_MOUNT))
        set_mob_movedice(pMob);
    if (IS_SET(pMob->off_flags, OFF_MAGIC))
        set_mob_manadice(pMob);
    return true;
}

/* Spec: resolve function pointer from name string */
static bool medit_apply_spec(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *name = json_string_value(change->new_value);
    if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
        pMob->spec_fun = NULL;
        return true;
    }
    SPEC_FUN *fn = spec_lookup(name);
    if (!fn) return false;
    pMob->spec_fun = fn;
    return true;
}

OLC_FIELD_APPLY_INT16(medit_apply_start_pos,   MOB_INDEX_DATA, start_pos)
OLC_FIELD_APPLY_INT16(medit_apply_default_pos,  MOB_INDEX_DATA, default_pos)

/* Act flags: force ACT_IS_NPC after applying */
static bool medit_apply_act(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = change->new_value;
    if (!json_is_array(arr)) return false;
    for (size_t i = 0; i < json_array_size(arr) && i < 2; i++) {
        json_t *elem = json_array_get(arr, i);
        if (json_is_integer(elem))
            pMob->act[i] = (long)json_integer_value(elem);
    }
    SET_BIT(pMob->act[0], ACT_IS_NPC);
    return true;
}

static bool medit_apply_affect(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = change->new_value;
    if (!json_is_array(arr)) return false;
    for (size_t i = 0; i < json_array_size(arr) && i < 2; i++) {
        json_t *elem = json_array_get(arr, i);
        if (json_is_integer(elem))
            pMob->affected_by[i] = (long)json_integer_value(elem);
    }
    return true;
}

OLC_FIELD_APPLY_INT16(medit_apply_ac_pierce, MOB_INDEX_DATA, ac[AC_PIERCE])
OLC_FIELD_APPLY_INT16(medit_apply_ac_bash,   MOB_INDEX_DATA, ac[AC_BASH])
OLC_FIELD_APPLY_INT16(medit_apply_ac_slash,  MOB_INDEX_DATA, ac[AC_SLASH])
OLC_FIELD_APPLY_INT16(medit_apply_ac_exotic, MOB_INDEX_DATA, ac[AC_EXOTIC])

OLC_FIELD_APPLY_DICE(medit_apply_hitdice,  MOB_INDEX_DATA, hit)
OLC_FIELD_APPLY_DICE(medit_apply_manadice, MOB_INDEX_DATA, mana)
OLC_FIELD_APPLY_DICE(medit_apply_damdice,  MOB_INDEX_DATA, damage)

/* Parent: resolve WNUM from string */
static bool medit_apply_parent(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "none") || !str_cmp(val, "clear") || !str_cmp(val, "0")) {
        pMob->parent_load.auid = 0;
        pMob->parent_load.vnum = 0;
        pMob->parent_wnum.pArea = NULL;
        pMob->parent_wnum.vnum = 0;
        pMob->parent = NULL;
        pMob->parent_inherited = false;
        return true;
    }
    WNUM wnum;
    char buf[MAX_INPUT_LENGTH];
    strlcpy(buf, val, sizeof(buf));
    if (!parse_widevnum(buf, pMob->area, &wnum)) return false;
    MOB_INDEX_DATA *parent = get_mob_index(wnum.pArea, wnum.vnum);
    if (!parent || parent == pMob) return false;
    pMob->parent_load.auid = wnum.pArea->uid;
    pMob->parent_load.vnum = wnum.vnum;
    pMob->parent_wnum = wnum;
    pMob->parent = parent;
    pMob->parent_inherited = false;
    return true;
}

/* Serialize functions for bitvector pending display */
static json_t *medit_serialize_act(void *entity, const char *field_path) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = json_array();
    for (int i = 0; i < 2; i++)
        json_array_append_new(arr, json_integer(pMob->act[i]));
    return arr;
}

static json_t *medit_serialize_affect(void *entity, const char *field_path) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    json_t *arr = json_array();
    for (int i = 0; i < 2; i++)
        json_array_append_new(arr, json_integer(pMob->affected_by[i]));
    return arr;
}

static bool medit_apply_corpsevnum(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pMob->corpse_load.vnum = 0;
        return true;
    }
    WNUM obj_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pMob->area, val);
    char buf[MAX_INPUT_LENGTH];
    strlcpy(buf, val, sizeof(buf));
    if (!parse_widevnum(buf, context, &obj_wnum)) return false;
    if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) return false;
    pMob->corpse_load.vnum = obj_wnum.vnum;
    return true;
}

static bool medit_apply_zombievnum(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    const char *val = json_string_value(change->new_value);
    if (IS_NULLSTR(val) || !str_cmp(val, "0")) {
        pMob->zombie_load.vnum = 0;
        return true;
    }
    WNUM obj_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pMob->area, val);
    char buf[MAX_INPUT_LENGTH];
    strlcpy(buf, val, sizeof(buf));
    if (!parse_widevnum(buf, context, &obj_wnum)) return false;
    if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) return false;
    pMob->zombie_load.vnum = obj_wnum.vnum;
    return true;
}

static bool medit_apply_var(void *entity, olc_pending_change_t *change) {
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;
    if (json_is_null(change->new_value)) {
        const char *varname = change->field_path + 4; /* skip "var/" */
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, varname, sizeof(buf));
        return olc_varclear(&pMob->index_vars, NULL, buf, true);
    } else {
        const char *arg = json_string_value(change->new_value);
        if (!arg) return false;
        char buf[MAX_INPUT_LENGTH];
        strlcpy(buf, arg, sizeof(buf));
        return olc_varset(&pMob->index_vars, NULL, buf, true);
    }
}

/*
 * List-operation apply handlers — invoked at commit time.
 */

static bool medit_apply_mprog_ops(void *entity, olc_pending_change_t *change)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        if (!pMob->progs) return false;

        int group_index = (int)json_integer_value(
            json_object_get(change->new_value, "group_index"));
        json_t *jtrig = json_object_get(change->new_value, "trigger_index");
        int trigger_idx = jtrig ? (int)json_integer_value(jtrig) : 0;

        PROG_GROUP groups[MAX_PROG_GROUPS];
        int num_groups = prog_build_groups(pMob->progs, groups, MAX_PROG_GROUPS, PRG_MPROG);

        if (group_index < 1 || group_index > num_groups) return false;
        PROG_GROUP *group = &groups[group_index - 1];

        if (trigger_idx > 0) {
            if (trigger_idx > group->trigger_count) return false;
            PROG_GROUP_ENTRY *entry = &group->triggers[trigger_idx - 1];
            return edit_deltrigger_specific(pMob->progs, group->script,
                entry->entry->trig_type, entry->entry->trig_phrase);
        } else {
            return edit_delscript(pMob->progs, group->script);
        }
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        long auid = (long)json_integer_value(
            json_object_get(change->new_value, "script_auid"));
        long vnum = (long)json_integer_value(
            json_object_get(change->new_value, "script_vnum"));
        int tindex = (int)json_integer_value(
            json_object_get(change->new_value, "trigger_index"));
        const char *phrase = json_string_value(
            json_object_get(change->new_value, "phrase"));
        if (!phrase) return false;

        AREA_DATA *area = get_area_from_uid(auid);
        if (!area) return false;

        SCRIPT_DATA *code = get_script_index(area, vnum, PRG_MPROG);
        if (!code) return false;

        if (!pMob->progs) pMob->progs = new_prog_bank();

        int slot = trigger_table[tindex].slot;

        PROG_LIST *list = new_trigger();
        list->vnum = vnum;
        list->script_is_widevnum = true;
        list->script_load.auid = auid;
        list->script_load.vnum = vnum;
        list->trig_type = tindex;
        list->trig_phrase = str_dup(phrase);
        if (is_widevnum_format(phrase)) {
            list->numeric = true;
            list->trig_is_widevnum = true;
            parse_widevnum_load(phrase, &list->trig_load);
            list->trig_number = (int)list->trig_load.vnum;
        } else {
            list->trig_number = atoi(list->trig_phrase);
            list->numeric = is_number(list->trig_phrase);
        }
        list->script = code;

        list_appendlink(pMob->progs[slot], list);
        return true;
    }

    return false;
}

static bool medit_apply_quest_ops(void *entity, olc_pending_change_t *change)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        int index = (int)json_integer_value(
            json_object_get(change->new_value, "index"));

        QUEST_V2_LIST *prev = NULL;
        int count = 0;
        for (QUEST_V2_LIST *qv2 = pMob->quests_v2; qv2; qv2 = qv2->next) {
            if (count == index) {
                if (prev) prev->next = qv2->next;
                else pMob->quests_v2 = qv2->next;
                free_quest_v2_list(qv2);
                return true;
            }
            prev = qv2;
            count++;
        }
        return false;
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        long auid = (long)json_integer_value(
            json_object_get(change->new_value, "auid"));
        long vnum = (long)json_integer_value(
            json_object_get(change->new_value, "vnum"));

        AREA_DATA *area = get_area_from_uid(auid);
        if (!area) return false;

        WNUM wnum;
        wnum.pArea = area;
        wnum.vnum = vnum;

        QUEST_V2_LIST *qv2 = new_quest_v2_list();
        qv2->load.auid = auid;
        qv2->load.vnum = vnum;
        qv2->wnum = wnum;
        qv2->next = pMob->quests_v2;
        pMob->quests_v2 = qv2;
        return true;
    }

    return false;
}

static bool medit_apply_reputation_ops(void *entity, olc_pending_change_t *change)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)entity;

    if (change->field_type == OLC_FIELD_LIST_REMOVE) {
        int index = (int)json_integer_value(
            json_object_get(change->new_value, "index"));

        MOB_REPUTATION_DATA *prev = NULL;
        int count = 0;
        for (MOB_REPUTATION_DATA *rep = pMob->mob_reputations; rep; rep = rep->next) {
            if (count == index) {
                if (prev) prev->next = rep->next;
                else pMob->mob_reputations = rep->next;
                free_mob_reputation_data(rep);
                return true;
            }
            prev = rep;
            count++;
        }
        return false;
    }

    if (change->field_type == OLC_FIELD_LIST_ADD) {
        long auid = (long)json_integer_value(
            json_object_get(change->new_value, "auid"));
        long vnum = (long)json_integer_value(
            json_object_get(change->new_value, "vnum"));
        int min_rank = (int)json_integer_value(
            json_object_get(change->new_value, "min_rank"));
        int max_rank = (int)json_integer_value(
            json_object_get(change->new_value, "max_rank"));
        long points = (long)json_integer_value(
            json_object_get(change->new_value, "points"));

        AREA_DATA *area = get_area_from_uid(auid);
        if (!area) return false;

        REPUTATION_INDEX_DATA *repIndex = get_reputation_index(area, vnum);
        if (!IS_VALID(repIndex)) return false;

        MOB_REPUTATION_DATA *new_rep = new_mob_reputation_data();
        new_rep->reputation = repIndex;
        new_rep->reputation_load.auid = auid;
        new_rep->reputation_load.vnum = vnum;
        new_rep->minimum_rank = (int16_t)min_rank;
        new_rep->maximum_rank = (int16_t)max_rank;
        new_rep->points = points;

        /* Append to end of list */
        MOB_REPUTATION_DATA *tail;
        for (tail = pMob->mob_reputations; tail && tail->next; tail = tail->next)
            ;

        if (tail)
            tail->next = new_rep;
        else
            pMob->mob_reputations = new_rep;

        return true;
    }

    return false;
}

/*
 * Field Handler Table — maps staged field names to apply functions.
 */
static const olc_field_handler_t medit_field_handlers[] = {
    { "Owner",                          OLC_FIELD_STRING,    NULL, medit_apply_owner,          NULL },
    { "Alignment",                      OLC_FIELD_INT16,     NULL, medit_apply_alignment,      NULL },
    { "Description",                    OLC_FIELD_MULTILINE, NULL, medit_apply_description,    NULL },
    { "Comments",                       OLC_FIELD_MULTILINE, NULL, medit_apply_comments,       NULL },
    { "List Name",                      OLC_FIELD_STRING,    NULL, medit_apply_list_name,      NULL },
    { "List Keywords",                  OLC_FIELD_STRING,    NULL, medit_apply_list_keywords,  NULL },
    { "Tags",                           OLC_FIELD_STRING,    NULL, medit_apply_tags,           NULL },
    { "Corpse Type",                    OLC_FIELD_INT,       NULL, medit_apply_corpse_type,    NULL },
    { "Sex",                            OLC_FIELD_INT16,     NULL, medit_apply_sex,            NULL },
    { "Body Type",                      OLC_FIELD_INT16,     NULL, medit_apply_body_type,      NULL },
    { "Subjective Pronoun",             OLC_FIELD_STRING,    NULL, medit_apply_pronoun_sub,    NULL },
    { "Objective Pronoun",              OLC_FIELD_STRING,    NULL, medit_apply_pronoun_obj,    NULL },
    { "Possessive Adjective Pronoun",   OLC_FIELD_STRING,    NULL, medit_apply_pronoun_pos_adj, NULL },
    { "Possessive Pronoun",             OLC_FIELD_STRING,    NULL, medit_apply_pronoun_pos,    NULL },
    { "Reflexive Pronoun",              OLC_FIELD_STRING,    NULL, medit_apply_pronoun_ref,    NULL },
    { "Form",                           OLC_FIELD_FLAGS,     NULL, medit_apply_form,           NULL },
    { "Parts",                          OLC_FIELD_FLAGS,     NULL, medit_apply_parts,          NULL },
    { "Immunity",                       OLC_FIELD_FLAGS,     NULL, medit_apply_imm_flags,      NULL },
    { "Resistance",                     OLC_FIELD_FLAGS,     NULL, medit_apply_res_flags,      NULL },
    { "Vulnerability",                  OLC_FIELD_FLAGS,     NULL, medit_apply_vuln_flags,     NULL },
    { "Material",                       OLC_FIELD_STRING,    NULL, medit_apply_material,       NULL },
    { "Offensive",                      OLC_FIELD_FLAGS,     NULL, medit_apply_off_flags,      NULL },
    { "Size",                           OLC_FIELD_INT16,     NULL, medit_apply_size,           NULL },
    { "Hitroll",                        OLC_FIELD_INT16,     NULL, medit_apply_hitroll,        NULL },
    { "Name",             OLC_FIELD_STRING,    NULL, medit_apply_player_name,   NULL },
    { "Short",            OLC_FIELD_STRING,    NULL, medit_apply_short_descr,   NULL },
    { "Long",             OLC_FIELD_STRING,    NULL, medit_apply_long_descr,    NULL },
    { "Script Keywords",  OLC_FIELD_STRING,    NULL, medit_apply_skeywds,       NULL },
    { "Dam Type",         OLC_FIELD_INT16,     NULL, medit_apply_dam_type,      NULL },
    { "Attacks",          OLC_FIELD_INT,       NULL, medit_apply_attacks,       NULL },
    { "Persist",          OLC_FIELD_BOOL,    NULL, medit_apply_persist,      NULL },
    { "Boss",             OLC_FIELD_BOOL,    NULL, medit_apply_boss,         NULL },
    { "Signature",        OLC_FIELD_STRING,  NULL, medit_apply_sign,         NULL },
    { "Gold",             OLC_FIELD_LONG,    NULL, medit_apply_gold,         NULL },
    { "Movement",         OLC_FIELD_LONG,    NULL, medit_apply_move,         NULL },
    { "Corpse Vnum",      OLC_FIELD_STRING,  NULL, medit_apply_corpsevnum,   NULL },
    { "Zombie Vnum",      OLC_FIELD_STRING,  NULL, medit_apply_zombievnum,   NULL },
    { "var/*",            OLC_FIELD_STRING,  NULL, medit_apply_var,          NULL },
    { "Level",            OLC_FIELD_INT16,      NULL, medit_apply_level,        NULL },
    { "Spec",             OLC_FIELD_STRING,     NULL, medit_apply_spec,         NULL },
    { "Start Position",   OLC_FIELD_INT16,      NULL, medit_apply_start_pos,    NULL },
    { "Default Position", OLC_FIELD_INT16,      NULL, medit_apply_default_pos,  NULL },
    { "Act",              OLC_FIELD_MULTIFLAGS, medit_serialize_act, medit_apply_act, NULL },
    { "Affected By",      OLC_FIELD_MULTIFLAGS, medit_serialize_affect, medit_apply_affect, NULL },
    { "AC Pierce",        OLC_FIELD_INT16,      NULL, medit_apply_ac_pierce,    NULL },
    { "AC Bash",          OLC_FIELD_INT16,      NULL, medit_apply_ac_bash,      NULL },
    { "AC Slash",         OLC_FIELD_INT16,      NULL, medit_apply_ac_slash,     NULL },
    { "AC Exotic",        OLC_FIELD_INT16,      NULL, medit_apply_ac_exotic,    NULL },
    { "Hit Dice",         OLC_FIELD_EMBEDDED,   NULL, medit_apply_hitdice,      NULL },
    { "Mana Dice",        OLC_FIELD_EMBEDDED,   NULL, medit_apply_manadice,     NULL },
    { "Damage Dice",      OLC_FIELD_EMBEDDED,   NULL, medit_apply_damdice,      NULL },
    { "Parent",           OLC_FIELD_STRING,     NULL, medit_apply_parent,       NULL },
    { "mprogs/**",        OLC_FIELD_LIST_ADD,   NULL, medit_apply_mprog_ops,      NULL },
    { "quests/**",        OLC_FIELD_LIST_ADD,   NULL, medit_apply_quest_ops,      NULL },
    { "reputations/**",   OLC_FIELD_LIST_ADD,   NULL, medit_apply_reputation_ops, NULL },
    { NULL, 0, NULL, NULL, NULL }
};

/*
 * Mobile Editor Definition
 */
static const olc_field_annotation_t medit_annotations[] = {
    { "Alignment", .min = -1000,  .max = 1000 },
    { "Hitroll",   .min = INT_MIN, .max = INT_MAX },
    { NULL }
};

static const OLC_EDITOR_DEF medit_def = {
    .name           = "MEdit",
    .editor_type    = ED_MOBILE,
    .cmd_table      = medit_table,
    .show_fn        = medit_show,
    .tabs           = {
        .count = 7,
        .tabs = {
            { "General",  "Gen", medit_show_general_tab },
            { "Combat",   "Com", medit_show_combat_tab },
            { "Defense",  "Def", medit_show_defense_tab },
            { "Economy",  "Eco", medit_show_economy_tab },
            { "Scripts",  "Scr", medit_show_scripts_tab },
            { "Special",  "Spc", medit_show_special_tab },
            { "Inheritance", "Inh", medit_show_inheritance_tab },
        }
    },
    .theme          = &olc_theme_entity,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_STAGED,
    .get_area_fn    = medit_get_area,
    .audit_changes  = true,
    .field_handlers = medit_field_handlers,
    .annotations    = medit_annotations,
};

/*
 * Mobile Editor Interpreter — delegates to framework.
 */
void medit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &medit_def);
}

extern void medit_register_actions(void);

/*
 * Mobile Editor Entry Point
 */
void do_medit(CHAR_DATA *ch, char *argument)
{
    static bool actions_registered = false;
    if (!actions_registered) {
        medit_register_actions();
        actions_registered = true;
    }

    MOB_INDEX_DATA *pMob;
    AREA_DATA *pArea;
    char arg1[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (arg1[0] != '\0' && str_cmp(arg1, "create"))
    {
        WNUM wnum;
        AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
        if (!parse_widevnum(arg1, context, &wnum)) {
            send_to_char("MEdit: Invalid widevnum format. Use vnum, #vnum or area#vnum.\n\r", ch);
            return;
        }

        if (!(pMob = get_mob_index(wnum.pArea, wnum.vnum)))
        {
            send_to_char("MEdit:  That vnum does not exist.\n\r", ch);
            return;
        }

        if (!has_access_area(ch, pMob->area))
        {
            send_to_char("Insufficient security to edit mob - action logged.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &medit_def, (void *)pMob, true);
    }
    else if (!str_cmp(arg1, "create"))
    {
        if (argument[0] != '\0') {
            WNUM wnum;
            AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
            if (!parse_widevnum(argument, context, &wnum)) {
                send_to_char("MEdit: Invalid widevnum format. Use vnum, #vnum or area#vnum.\n\r", ch);
                return;
            }
            pArea = wnum.pArea;

            if (!pArea)
            {
                send_to_char("MEdit:  That vnum is not assigned an area.\n\r", ch);
                return;
            }

            if (!IS_BUILDER(ch, pArea))
            {
                send_to_char("Insufficient security to edit mob - action logged.\n\r", ch);
                return;
            }
        }

        if (medit_create(ch, argument))
            olc_editor_enter(ch, &medit_def, ch->desc->pEdit, true);
    }
    else
    {
        send_to_char("MEdit:  There is no default mobile to edit.\n\r", ch);
    }
}


/*
 * ========================================================================
 * Tab-specific display functions
 * ========================================================================
 */

static void medit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);

    olc_display_string(ctx, theme, "Name:",       "name",   pMob->player_name);
    olc_display_string(ctx, theme, "Area:",        NULL,    pMob->area ? pMob->area->name : "No Area");
    olc_display_string(ctx, theme, "Vnum:",        NULL,    widevnum_string_mobile(pMob, pMob->area));
    olc_display_number(ctx, theme, "Loaded:",      NULL,    pMob->count);

    olc_display_pair(ctx, theme,
        "Sig:", NULL, pMob->sig,
        "Creator:", NULL, pMob->creator_sig);

    olc_display_pair(ctx, theme,
        "Race:", "race", pMob->race ? pMob->race->name : "unknown",
        "Body Type:", NULL, formatf("%d", pMob->body_type));

    olc_display_number(ctx, theme, "Level:",       "level",     pMob->level);
    olc_display_number(ctx, theme, "Alignment:",   "alignment", pMob->alignment);
    olc_display_type(ctx, theme,   "Sex:",         "sex",       sex_flags, pMob->sex);
    olc_display_type(ctx, theme,   "Body Type:",   "bodytype",  body_types, pMob->body_type);
    olc_display_type(ctx, theme,   "Size:",        "size",      size_flags, pMob->size);
    olc_display_string(ctx, theme, "Material:",    "material",  pMob->material);
    olc_display_string(ctx, theme, "Owner:",       "owner",     pMob->owner);
    olc_display_string(ctx, theme, "Parent:",      "parent",
        pMob->parent_wnum.vnum > 0
            ? widevnum_string(pMob->parent_wnum.pArea, pMob->parent_wnum.vnum, pMob->area)
            : (pMob->parent_load.vnum > 0
                ? formatf("%ld#%ld", pMob->parent_load.auid, pMob->parent_load.vnum)
                : "(none)"));

    olc_display_section(ctx, theme, "Pronouns");
    olc_display_string(ctx, theme, "Subjective:", "pronounss", pMob->pronoun_he_she);
    olc_display_string(ctx, theme, "Objective:",  "pronounos", pMob->pronoun_him_her);
    olc_display_string(ctx, theme, "Poss Adj:",   "pronounpas", pMob->pronoun_his_her);
    olc_display_string(ctx, theme, "Poss Pron:",  "pronounpps", pMob->pronoun_his_hers);
    olc_display_string(ctx, theme, "Reflexive:",  "pronounrs", pMob->pronoun_himself_herself);

    olc_display_type(ctx, theme,   "Start Pos:",   "position start", position_flags, pMob->start_pos);
    olc_display_type(ctx, theme,   "Default Pos:", "position default", position_flags, pMob->default_pos);
        olc_display_section(ctx, theme, "Flags");
        olc_display_flags(ctx, theme,   "Act:",         "act",   act_flags, pMob->act[0]);
        olc_display_flags(ctx, theme,   "Act2:",        "act",   act2_flags, pMob->act[1]);

    olc_display_bool(ctx, theme,   "Boss:",        "boss",    pMob->boss);
    olc_display_bool(ctx, theme,   "Persist:",     "persist", pMob->persist);

    olc_display_infof(ctx, theme,
        "%sAct:%s          %s%s{x",
        theme->label, theme->value, theme->value,
        bitmatrix_string(act_flagbank, pMob->act));

    olc_display_section(ctx, theme, "Descriptions");

    olc_display_string(ctx, theme, "Short Descr:", "short", pMob->short_descr);
    olc_display_string(ctx, theme, "List Name:", "listname", IS_NULLSTR(pMob->list_name) ? "(default: short)" : pMob->list_name);
    olc_display_string(ctx, theme, "List Keywords:", "listkeywords", IS_NULLSTR(pMob->list_keywords) ? "(none)" : pMob->list_keywords);
    olc_display_string(ctx, theme, "Tags:", "tags", IS_NULLSTR(pMob->tags) ? "(none)" : pMob->tags);
    olc_display_string(ctx, theme, "Auto Tags:", NULL, IS_NULLSTR(pMob->auto_tags) ? "(none)" : pMob->auto_tags);
    olc_display_text(ctx, theme,   "Long Descr:",  "long",  pMob->long_descr);
    olc_display_text(ctx, theme,   "Description:", "description", pMob->description);
    olc_display_text(ctx, theme,   "Comments:",    "comments", pMob->comments);
}

static void medit_show_combat_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);
    char buf[MIL];

    olc_display_number(ctx, theme, "Hitroll:",     "hitroll", pMob->hitroll);
    olc_display_string(ctx, theme, "Dam Type:",    "damtype", attack_table[pMob->dam_type].name);
    olc_display_number(ctx, theme, "Movement:",    "movedice", pMob->move);

    if (pMob->attacks < 0)
        olc_display_string(ctx, theme, "Attacks:",  "attacks", "{Yscripted{x");
    else
        olc_display_number(ctx, theme, "Attacks:",  "attacks", pMob->attacks);

    olc_display_section(ctx, theme, "Dice");

    olc_display_dice(ctx, theme, "Hit Dice:",    "hitdice",  &pMob->hit);
    olc_display_dice(ctx, theme, "Damage Dice:", "damdice",  &pMob->damage);
    olc_display_dice(ctx, theme, "Mana Dice:",   "manadice", &pMob->mana);

    olc_display_section(ctx, theme, "Armor Class");

    snprintf(buf, sizeof(buf), "pierce: %d  bash: %d  slash: %d  magic: %d",
        pMob->ac[AC_PIERCE], pMob->ac[AC_BASH],
        pMob->ac[AC_SLASH],  pMob->ac[AC_EXOTIC]);
    olc_display_string(ctx, theme, "Armour:", "armour", buf);

    olc_display_section(ctx, theme, "Offensive");

    olc_display_flags(ctx, theme,  "Off Flags:",   "off",    off_flags, pMob->off_flags);

    olc_display_infof(ctx, theme,
        "%sAffected by:%s  %s%s{x",
        theme->label, theme->value, theme->value,
        bitvector_string(2, pMob->affected_by[0], affect_flags,
                            pMob->affected_by[1], affect2_flags));
}

static void medit_show_defense_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);

    olc_display_flags(ctx, theme, "Immunities:",      "immune", imm_flags,  pMob->imm_flags);
        olc_display_flags(ctx, theme, "Affect:",          "affect", affect_flags, pMob->affected_by[0]);
        olc_display_flags(ctx, theme, "Affect2:",         "affect", affect2_flags, pMob->affected_by[1]);
    olc_display_flags(ctx, theme, "Resistances:",     "res",    res_flags,  pMob->res_flags);
    olc_display_flags(ctx, theme, "Vulnerabilities:", "vuln",   vuln_flags, pMob->vuln_flags);

    olc_display_section(ctx, theme, "Body");

    olc_display_flags(ctx, theme, "Parts:",           "part",   part_flags, pMob->parts);

    olc_display_section(ctx, theme, "Corpse");

    olc_display_type(ctx, theme,  "Corpse Type:",     "corpsetype", corpse_types, pMob->corpse_type);

    if (pMob->corpse_load.vnum) {
        OBJ_INDEX_DATA *obj = get_obj_index(pMob->area, pMob->corpse_load.vnum);
        olc_display_widevnum(ctx, theme, "Corpse Obj:",   "corpsevnum",
            obj ? widevnum_string_object(obj, pMob->area)
                : formatf("%ld", pMob->corpse_load.vnum),
            obj ? obj->short_descr : NULL);
    }

    if (pMob->zombie_load.vnum) {
        OBJ_INDEX_DATA *obj = get_obj_index(pMob->area, pMob->zombie_load.vnum);
        olc_display_widevnum(ctx, theme, "Zombie Obj:",   "zombievnum",
            obj ? widevnum_string_object(obj, pMob->area)
                : formatf("%ld", pMob->zombie_load.vnum),
            obj ? obj->short_descr : NULL);
    }
}

static void medit_show_economy_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);
    char buf[MAX_STRING_LENGTH];

    olc_display_number(ctx, theme, "Wealth:", "wealth", pMob->wealth);

    if (pMob->pShop) {
        SHOP_DATA *pShop = pMob->pShop;
        int iTrade;

        olc_display_section(ctx, theme, "Shop Data");

        olc_display_number(ctx, theme, "Markup (buy):",    "shop profit", pShop->profit_buy);
        olc_display_number(ctx, theme, "Markdown (sell):", "shop profit", pShop->profit_sell);

        snprintf(buf, sizeof(buf), "%d to %d", pShop->open_hour, pShop->close_hour);
        olc_display_string(ctx, theme, "Hours:", "shop hours", buf);

        if (pShop->restock_interval > 0)
            olc_display_number(ctx, theme, "Restock (min):", NULL, pShop->restock_interval);
        else
            olc_display_string(ctx, theme, "Restock:", NULL, "disabled");

        olc_display_percent(ctx, theme, "Discount:", NULL, pShop->discount, 1);
        olc_display_flags(ctx, theme, "Flags:", "shop flag", shop_flags, pShop->flags);

        for (iTrade = 0; iTrade < MAX_TRADE; iTrade++) {
            if (pShop->buy_type[iTrade]) {
                if (!iTrade) {
                    olc_display_section(ctx, theme, "Trade Types");
                }
                snprintf(buf, sizeof(buf), "Trade [%d]:", iTrade);
                olc_display_type(ctx, theme, buf, NULL, type_flags, pShop->buy_type[iTrade]);
            }
        }

        if (pShop->shipyard > 0) {
            WILDS_DATA *wilds = get_wilds_from_uid(NULL, pShop->shipyard);
            olc_display_section(ctx, theme, "Shipyard");
            snprintf(buf, sizeof(buf), "%s (%ld) at (%d,%d) to (%d,%d)",
                wilds ? wilds->name : "(null)", pShop->shipyard,
                pShop->shipyard_region[0][0], pShop->shipyard_region[0][1],
                pShop->shipyard_region[1][0], pShop->shipyard_region[1][1]);
            olc_display_string(ctx, theme, "Location:", NULL, buf);
            olc_display_string(ctx, theme, "Shipyard Desc:", NULL, pShop->shipyard_description);
        }

        if (pShop->stock != NULL) {
            SHOP_STOCK_DATA *pStock;
            int iStock;
            char lvl[MIL], qty[MIL], pricing[MIL], typ[MIL];
            char hours[MIL], item[MIL], disc[MIL];
            int hwidth, lwidth, qwidth, pwidth;

            olc_display_section(ctx, theme, "Stock");

            for (iStock = 1, pStock = pShop->stock; pStock; pStock = pStock->next, iStock++) {
                if (iStock == 1) {
                    add_buf(ctx->buffer, "{G  Stock# Level Quantity Sng Hours    Price(s)    Disc                   Item{x\n\r");
                    add_buf(ctx->buffer, "{G  ------ ----- -------- --- ----- -------------- ---- --------------------------------------{x\n\r");
                }

                if (pStock->level > 0)
                    sprintf(lvl, "{Y%d{x", pStock->level);
                else
                    strcpy(lvl, "{GAuto{x");
                lwidth = get_colour_width(lvl) + 5;

                if (pStock->quantity > 0) {
                    if (pStock->restock_rate > 0)
                        sprintf(qty, "{W%d{x / {W%d{x", pStock->quantity, pStock->restock_rate);
                    else
                        sprintf(qty, "{W%d{x / {D--{x", pStock->quantity);
                } else {
                    strcpy(qty, "   {D--{x   ");
                }
                qwidth = get_colour_width(qty) + 8;

                if (pStock->duration > 0)
                    sprintf(hours, "{G%d{x", pStock->duration);
                else
                    strcpy(hours, " {D---{x ");
                hwidth = get_colour_width(hours) + 5;

                if (!IS_NULLSTR(pStock->custom_price)) {
                    strncpy(pricing, pStock->custom_price, sizeof(pricing) - 3);
                    strcat(pricing, "{x");
                    strcpy(disc, " {D--{x ");
                } else {
                    pricing[0] = '\0';
                    int pj = 0;

                    if (pStock->silver > 0) {
                        long silver = pStock->silver % 100;
                        long gold = pStock->silver / 100;
                        if (gold > 0) {
                            if (silver > 0)
                                pj = sprintf(pricing, "{x%ld{Yg{x%ld{Ws{x", gold, silver);
                            else
                                pj = sprintf(pricing, "{x%ld{Yg{x", gold);
                        } else {
                            pj = sprintf(pricing, "{x%ld{Ws{x", silver);
                        }
                    }
                    if (pStock->qp > 0) {
                        if (pj > 0) { pricing[pj++] = ','; pricing[pj++] = ' '; }
                        pj += sprintf(pricing + pj, "{x%ld{Gqp{x", pStock->qp);
                    }
                    if (pStock->dp > 0) {
                        if (pj > 0) { pricing[pj++] = ','; pricing[pj++] = ' '; }
                        pj += sprintf(pricing + pj, "{x%ld{Mdp{x", pStock->dp);
                    }
                    if (pStock->pneuma > 0) {
                        if (pj > 0) { pricing[pj++] = ','; pricing[pj++] = ' '; }
                        pj += sprintf(pricing + pj, "{x%ld{Cpn{x", pStock->pneuma);
                    }
                    pricing[pj] = '\0';
                    sprintf(disc, "%3d%%", pStock->discount);
                }
                pwidth = get_colour_width(pricing) + 14;

                switch (pStock->type) {
                case STOCK_OBJECT:
                    strcpy(typ, "{GOBJECT{x  ");
                    if (pStock->entity.wnum.vnum > 0) {
                        OBJ_INDEX_DATA *obj = pStock->entity.wnum.pArea ?
                            get_obj_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_obj_index(pMob->area, pStock->entity.wnum.vnum);
                        if (!obj) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", obj->short_descr, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_PET:
                    strcpy(typ, "{GPET{x     ");
                    if (pStock->entity.wnum.vnum > 0) {
                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);
                        if (!mob) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", mob->short_descr, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_MOUNT:
                    strcpy(typ, "{GMOUNT{x   ");
                    if (pStock->entity.wnum.vnum > 0) {
                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);
                        if (!mob) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", mob->short_descr, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_GUARD:
                    strcpy(typ, "{GGUARD{x   ");
                    if (pStock->entity.wnum.vnum > 0) {
                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);
                        if (!mob) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", mob->short_descr, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_CREW:
                    strcpy(typ, "{GCREW{x    ");
                    if (pStock->entity.wnum.vnum > 0) {
                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);
                        if (!mob || !mob->pCrew) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", mob->short_descr, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_SHIP:
                    strcpy(typ, "{GSHIP{x    ");
                    if (pStock->entity.wnum.vnum > 0) {
                        SHIP_INDEX_DATA *ship_index = get_ship_index(pStock->entity.wnum.vnum);
                        if (!ship_index) strcpy(item, "-invalid-");
                        else sprintf(item, "%s (%s)", ship_index->name, widevnum_string_wnum(pStock->entity.wnum, pMob->area));
                    } else strcpy(item, "-invalid-");
                    break;
                case STOCK_CUSTOM:
                    strcpy(typ, "{GCUSTOM{x  ");
                    if (IS_NULLSTR(pStock->custom_keyword))
                        strcpy(item, "-invalid stock item-");
                    else
                        strcpy(item, pStock->custom_keyword);
                    break;
                }

                snprintf(buf, sizeof(buf), "  {G[{x%4d{G]{x %-*s %*s  %s  %*s %-*s %s %s%s\n\r",
                    iStock, lwidth, lvl, qwidth, qty,
                    (pStock->singular ? "{RY{x" : "{GN{x"),
                    hwidth, hours, pwidth, pricing, disc, typ, item);
                add_buf(ctx->buffer, buf);

                if (!IS_NULLSTR(pStock->custom_descr)) {
                    snprintf(buf, sizeof(buf), "                                                              - %s\n\r", pStock->custom_descr);
                    add_buf(ctx->buffer, buf);
                }
            }
        }
    } else {
        olc_display_string(ctx, theme, "Shop:", NULL, "(none)");
    }
}

static void medit_show_scripts_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);

    olc_display_string(ctx, theme, "Script Kwds:", "scriptkwd", pMob->skeywds);

    if (pMob->spec_fun)
        olc_display_string(ctx, theme, "Spec Fun:", "spec", spec_name(pMob->spec_fun));

    olc_display_scripts(ctx, theme, pMob->progs, PRG_MPROG,
        "MobProg Vnum", "addmprog", "delmprog");

    olc_display_vars(ctx, theme, pMob->index_vars, "varset", "varclear");
}

static void medit_show_special_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);
    char buf[MAX_STRING_LENGTH];
    bool has_special_data = false;

    if (pMob->mob_reputations) {
        MOB_REPUTATION_DATA *rep;
        int index = 0;

        has_special_data = true;
        olc_display_section(ctx, theme, "Reputation Rewards");
        add_buf(ctx->buffer, formatf("  %s#   Reputation                       Min  Max  Points{x\n\r", theme->label));
        add_buf(ctx->buffer, formatf("  %s--- ------------------------------- ---- ---- -------{x\n\r", theme->label));

        for (rep = pMob->mob_reputations; rep; rep = rep->next) {
            const char *name = "(invalid)";
            char wnum[MIL];
            char min_rank_buf[16];
            char max_rank_buf[16];

            if (IS_VALID(rep->reputation)) {
                name = rep->reputation->name ? rep->reputation->name : "(unnamed)";
                strncpy(wnum, widevnum_string(rep->reputation->area, rep->reputation->vnum, pMob->area), sizeof(wnum) - 1);
                wnum[sizeof(wnum) - 1] = '\0';
            } else {
                snprintf(wnum, sizeof(wnum), "%ld#%ld", rep->reputation_load.auid, rep->reputation_load.vnum);
            }

            if (rep->minimum_rank > 0)
                snprintf(min_rank_buf, sizeof(min_rank_buf), "%d", rep->minimum_rank);
            else
                strcpy(min_rank_buf, "-");

            if (rep->maximum_rank > 0)
                snprintf(max_rank_buf, sizeof(max_rank_buf), "%d", rep->maximum_rank);
            else
                strcpy(max_rank_buf, "-");

            add_buf(ctx->buffer, formatf("  %-3d %-31.31s %-4s %-4s %7ld  %s\n\r",
                index,
                name,
                min_rank_buf,
                max_rank_buf,
                rep->points,
                wnum));
            index++;
        }

        olc_display_string(ctx, theme, "Add:", NULL,
            "addreputation <widevnum> <min rank|none> <max rank|none> <points>");
        olc_display_string(ctx, theme, "Delete:", NULL,
            "delreputation <index>");
    }

    if (pMob->pQuestor) {
        QUESTOR_DATA *questor = pMob->pQuestor;
        has_special_data = true;

        olc_display_section(ctx, theme, "Questor Data");

        olc_display_number(ctx, theme, "Scroll Vnum:", "questor scroll", questor->scroll);
        olc_display_string(ctx, theme, "Keywords:", "questor keywords", questor->keywords);
        olc_display_string(ctx, theme, "Short Desc:", "questor short", questor->short_descr);
        olc_display_string(ctx, theme, "Long Desc:", "questor long", questor->long_descr);
        olc_display_text(ctx, theme,   "Header:", "questor header", questor->header);
        olc_display_text(ctx, theme,   "Footer:", "questor footer", questor->footer);
        olc_display_string(ctx, theme, "Prefix:", "questor prefix", questor->prefix);
        olc_display_string(ctx, theme, "Suffix:", "questor suffix", questor->suffix);

        if (questor->line_width > 0)
            olc_display_number(ctx, theme, "Width:", "questor width", questor->line_width);
        else
            olc_display_string(ctx, theme, "Width:", NULL, "disabled");
    }

    if (pMob->pTrainer) {
        TRAINER_DATA *trainer = pMob->pTrainer;
        TRAINER_ENTRY *entry;

        has_special_data = true;

        olc_display_section(ctx, theme, "Trainer Data");

        olc_display_string(ctx, theme, "Greeting:", "trainer greeting", trainer->greeting);

        if (trainer->entries) {
            add_buf(ctx->buffer, formatf("  %s%-25s %-6s %-6s %-8s %-15s{x\n\r",
                theme->label,
                "Skill/Spell/Song", "MaxRat", "Gold", "Trains", "Script"));
            for (entry = trainer->entries; entry; entry = entry->next) {
                if (!IS_VALID(entry)) continue;
                add_buf(ctx->buffer, formatf("  %-25s %-6d %-6d %-8d %-15s\n\r",
                    entry->skill_name ? entry->skill_name : "?",
                    entry->max_rating,
                    entry->cost_gold,
                    entry->cost_trains,
                    entry->check_script ? entry->check_script : "(none)"));
            }
        } else {
            olc_display_string(ctx, theme, "Entries:", NULL, "(none)");
        }
    }

    if (IS_VALID(pMob->pCrew)) {
        has_special_data = true;
        olc_display_section(ctx, theme, "Ship Crew Data");

        olc_display_string(ctx, theme, "Minimum Rank:", NULL, "NYI");
        olc_display_percent(ctx, theme, "Scouting:",    NULL, pMob->pCrew->scouting, 1);
        olc_display_percent(ctx, theme, "Gunning:",     NULL, pMob->pCrew->gunning, 1);
        olc_display_percent(ctx, theme, "Oarring:",     NULL, pMob->pCrew->oarring, 1);
        olc_display_percent(ctx, theme, "Mechanics:",   NULL, pMob->pCrew->mechanics, 1);
        olc_display_percent(ctx, theme, "Navigation:",  NULL, pMob->pCrew->navigation, 1);
        olc_display_percent(ctx, theme, "Leadership:",  NULL, pMob->pCrew->leadership, 1);
    }

    if (pMob->quests_v2) {
        QUEST_V2_LIST *qv2;
        int qidx = 1;
        char wstr[MIL];

        has_special_data = true;
        olc_display_section(ctx, theme, "Available Quests (V2)");
        add_buf(ctx->buffer, formatf("  %s%-3s %-35s  %s{x\n\r",
            theme->label, "#", "Quest Name", "Widevnum"));
        add_buf(ctx->buffer, formatf("  %s--- ----------------------------------- --------{x\n\r",
            theme->label));

        for (qv2 = pMob->quests_v2; qv2; qv2 = qv2->next, qidx++) {
            QUEST_INDEX_V2_DATA *qi = get_quest_index_v2_wnum(qv2->wnum);
            const char *qname = qi ? (qi->name && qi->name[0] ? qi->name : "(unnamed)") : "(invalid)";

            if (qv2->wnum.pArea)
                strncpy(wstr, widevnum_string(qv2->wnum.pArea, qv2->wnum.vnum, pMob->area), sizeof(wstr) - 1);
            else
                snprintf(wstr, sizeof(wstr), "%ld#%ld", qv2->load.auid, qv2->load.vnum);
            wstr[sizeof(wstr) - 1] = '\0';

            add_buf(ctx->buffer, formatf("  %-3d %-35.35s  %s%s\n\r",
                qidx, qname, wstr,
                qi && !qi->enabled ? " {D[disabled]{x" : ""));
        }

        olc_display_string(ctx, theme, "Add:",    NULL, "addquest <widevnum>");
        olc_display_string(ctx, theme, "Delete:", NULL, "delquest <index>");
    }

    if (!has_special_data) {
        snprintf(buf, sizeof(buf), "  %s(No questor, trainer, crew, or v2 quest data){x\n\r", theme->unset);
        add_buf(ctx->buffer, buf);
    }
}

static void medit_show_inheritance_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    MOB_INDEX_DATA *pMob = (MOB_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);
    AREA_DATA *area;
    MOB_INDEX_DATA *mob;
    int iHash;
    int child_count = 0;

    olc_display_section(ctx, theme, "Parent Chain");
    if (pMob->parent) {
        MOB_INDEX_DATA *cur = pMob->parent;
        int depth = 0;

        while (cur && depth < 32) {
            if (depth > 0)
                add_buf(ctx->buffer, " {D->{x ");

            mxp_command_link(ch->desc, ctx->buffer,
                formatf("medit %s", widevnum_string_mobile(cur, NULL)),
                "Edit mobile",
                widevnum_string_mobile(cur, NULL));
            add_buf(ctx->buffer, formatf(" {x%s", cur->short_descr));

            cur = cur->parent;
            depth++;
        }

        if (cur)
            add_buf(ctx->buffer, " {D->{x ...");
        add_buf(ctx->buffer, "\n\r");
    } else if (pMob->parent_load.vnum > 0) {
        olc_display_infof(ctx, theme, "Unresolved parent: %ld#%ld",
            pMob->parent_load.auid, pMob->parent_load.vnum);
    } else {
        olc_display_infof(ctx, theme, "(none)");
    }

    olc_display_section(ctx, theme, "Direct Children");
    for (area = area_first; area != NULL; area = area->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (mob = area->mob_index_hash[iHash]; mob != NULL; mob = mob->next) {
                bool is_child = false;

                if (mob == pMob)
                    continue;

                if (mob->parent == pMob)
                    is_child = true;
                else if (!mob->parent && mob->parent_load.vnum == pMob->vnum
                    && (mob->parent_load.auid == 0 || mob->parent_load.auid == pMob->area->uid))
                    is_child = true;

                if (!is_child)
                    continue;

                mxp_command_link(ch->desc, ctx->buffer,
                    formatf("medit %s", widevnum_string_mobile(mob, mob->area)),
                    "Edit mobile",
                    widevnum_string_mobile(mob, NULL));
                add_buf(ctx->buffer, formatf(" {x%s\n\r", mob->short_descr));
                child_count++;
            }
        }
    }
    if (child_count < 1)
        olc_display_infof(ctx, theme, "(none)");

    olc_display_section(ctx, theme, "Field Source");
    if (!pMob->parent) {
        olc_display_infof(ctx, theme, "No parent set; all values are local.");
        return;
    }

    olc_display_string(ctx, theme, "Act Flags:", NULL,
        (pMob->act[0] == ACT_IS_NPC && pMob->act[1] == 0) ? "Inherited" : "Local");
    olc_display_bool(ctx, theme, "Parent Resolved:", NULL, pMob->parent != NULL);
    olc_display_bool(ctx, theme, "Inheritance Applied:", NULL, pMob->parent_inherited);
}

/*
 * ========================================================================
 * Main show function — dispatches to active tab
 * ========================================================================
 */

MEDIT(medit_show)
{
    MOB_INDEX_DATA *pMob;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&medit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_MOB(ch, pMob);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "MEdit", pMob->list_name,
        formatf("%s", widevnum_string_mobile(pMob, pMob->area)), &medit_def);

    /* Dispatch to active tab's show function */
    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < medit_def.tabs.count; i++) {
            if (medit_def.tabs.tabs[i].show_fn)
                medit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)pMob);
        }
    } else if (tab >= 0 && tab < medit_def.tabs.count && medit_def.tabs.tabs[tab].show_fn) {
        medit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)pMob);
    } else {
        medit_show_general_tab(ch, ctx, (void *)pMob);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

MEDIT(medit_next)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *candidate;
    MOB_INDEX_DATA *nextMob = NULL;
    int hash;

    EDIT_MOB(ch, pMob);

    for (hash = 0; hash < MAX_KEY_HASH; hash++)
    {
        for (candidate = pMob->area->mob_index_hash[hash]; candidate != NULL; candidate = candidate->next)
        {
            if (candidate->vnum > pMob->vnum
            && (!nextMob || candidate->vnum < nextMob->vnum))
                nextMob = candidate;
        }
    }

    if (nextMob == NULL)
    {
    send_to_char("No next mob in area.\n\r", ch);
    }
    else
    {
    olc_editor_enter(ch, &medit_def, (void *)nextMob, false);
    }
    return false;
}

MEDIT(medit_persist)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    /* IMP check only when enabling persistence */
    bool enabling = (!str_cmp(argument, "on") || !str_cmp(argument, "yes")
        || !str_cmp(argument, "true") || IS_NULLSTR(argument));
    if (enabling && !str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }

    return olc_cmd_bool(ch, argument, "Persist",
        "Usage: persist on/off\n\r",
        &pMob->persist, NULL, NULL);
}

MEDIT(medit_boss)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    bool enabling = (!str_cmp(argument, "on") || !str_cmp(argument, "yes")
        || !str_cmp(argument, "true") || IS_NULLSTR(argument));
    if (enabling && !str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
        send_to_char("You can't do this without an IMP's permission.\n\r", ch);
        return false;
    }

    return olc_cmd_bool(ch, argument, "Boss",
        "Usage: boss on/off\n\r",
        &pMob->boss, NULL, NULL);
}

MEDIT(medit_prev)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *candidate;
    MOB_INDEX_DATA *prevMob = NULL;
    int hash;

    EDIT_MOB(ch, pMob);

    for (hash = 0; hash < MAX_KEY_HASH; hash++)
    {
        for (candidate = pMob->area->mob_index_hash[hash]; candidate != NULL; candidate = candidate->next)
        {
            if (candidate->vnum < pMob->vnum
            && (!prevMob || candidate->vnum > prevMob->vnum))
                prevMob = candidate;
        }
    }

    if (prevMob == NULL)
    {
    send_to_char("No previous mob in area.\n\r", ch);
    }
    else
    {
    olc_editor_enter(ch, &medit_def, (void *)prevMob, false);
    }
    return false;
}

MEDIT(medit_attacks)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  attacks [number|scripted]\n\r", ch);
        return false;
    }

    if (!str_prefix(argument, "scripted")) {
        char neg_one[] = "-1";
        return olc_cmd_number(ch, neg_one, "Attacks", NULL,
            &pMob->attacks, -1, 10, NULL, NULL);
    }

    return olc_cmd_number(ch, argument, "Attacks", NULL,
        &pMob->attacks, 0, 10, NULL, NULL);
}

MEDIT(medit_owner)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Owner", NULL, &pMob->owner,
        OLC_STR_CLEARABLE, NULL, NULL);
}

MEDIT(medit_create)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *temp_mob;
    AREA_DATA *pArea;
    long  value;
    int  iHash;
    long auto_vnum = 0;

    // Auto-vnum: if no argument or argument is 0, find next available vnum in current area
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
    pArea = ch->in_room->area;
    auto_vnum = 1;
    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (temp_mob = pArea->mob_index_hash[iHash]; temp_mob; temp_mob = temp_mob->next)
        {
        if (temp_mob->vnum >= auto_vnum)
            auto_vnum = temp_mob->vnum + 1;
        }
    }

    if (auto_vnum <= 0)
    {
        send_to_char("Unable to allocate a new mobile vnum.\n\r", ch);
        return false;
    }
    
    value = auto_vnum;
    }
    else
    {
    // Parse widevnum format
    WNUM wnum;
    AREA_DATA *context = ch->in_room->area;
    if (!parse_widevnum(argument, context, &wnum)) {
        send_to_char("MEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }
    
    value = wnum.vnum;
    pArea = wnum.pArea;
    }

    if (!pArea)
    {
    send_to_char("MEdit:  That vnum is not assigned an area.\n\r", ch);
    return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
    send_to_char("MEdit:  Vnum in an area you cannot build in.\n\r", ch);
    return false;
    }

    if (get_mob_index(pArea, value))
    {
    send_to_char("MEdit:  Mobile vnum already exists.\n\r", ch);
    return false;
    }

    pMob			= new_mob_index();
    pMob->vnum			= value;
    pMob->area			= pArea;

    if (value > top_vnum_mob)
    top_vnum_mob = value;

    pMob->act[0]			= ACT_IS_NPC;
    pMob->act[1]			= 0;
    iHash			= value % MAX_KEY_HASH;
    pMob->next			= pArea->mob_index_hash[iHash];
    pArea->mob_index_hash[iHash]	= pMob;
    ch->desc->pEdit		= (void *)pMob;


    // Make sure to set minimum level to 1.
    pMob->level = 1;
    set_mob_hitdice(pMob);
    set_mob_damdice(pMob);
    if (!IS_SET(pMob->act[0], ACT_MOUNT))
    set_mob_movedice(pMob);

    send_to_char("Mobile Created.\n\r", ch);
    SET_BIT(pMob->area->area_flags, AREA_CHANGED);
    free_string(pMob->creator_sig);
    pMob->creator_sig = str_dup(ch->name);
    return true;
}

MEDIT(medit_spec)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  spec [special function]\n\r", ch);
        return false;
    }

    /* Validate */
    if (str_cmp(argument, "none") && !spec_lookup(argument)) {
        send_to_char("MEdit: No such special function.\n\r", ch);
        return false;
    }

    /* Staged mode: stage spec name as string */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;
            const char *old_name = pMob->spec_fun ? spec_name(pMob->spec_fun) : "none";
            json_t *old_val = json_string(old_name);
            json_t *new_val = json_string(argument);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, "Spec", OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);
            if (result)
                printf_to_char(ch, "{G[STAGED]{x Spec set to %s.\n\r", argument);
            else
                printf_to_char(ch, "Spec reverted to original value.\n\r");
            return result != NULL;
        }
    }

    /* Non-staged fallback */
    if (!str_cmp(argument, "none")) {
        pMob->spec_fun = NULL;
        send_to_char("Spec removed.\n\r", ch);
        return true;
    }

    pMob->spec_fun = spec_lookup(argument);
    send_to_char("Spec set.\n\r", ch);
    return true;
}

MEDIT(medit_damtype)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  damtype [damage message]\n\r"
                     "For a list of damtypes, type '? weapon'.\n\r", ch);
        return false;
    }

    int16_t val = (int16_t)attack_lookup(argument);
    if (val == 0 && str_cmp(argument, attack_table[0].name)) {
        send_to_char("Invalid attack type. Type '? weapon' for a list.\n\r", ch);
        return false;
    }
    char num_buf[16];
    snprintf(num_buf, sizeof(num_buf), "%d", val);
    return olc_cmd_number_i16(ch, num_buf, "Dam Type", NULL,
        &pMob->dam_type, 0, 32767, NULL, NULL);
}

MEDIT(medit_align)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_number_i16(ch, argument, "Alignment", NULL,
        &pMob->alignment, -1000, 1000, NULL, NULL);
}

MEDIT(medit_level)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_number_i16(ch, argument, "Level", NULL,
        &pMob->level, 1, MAX_MOB_SKILL_LEVEL, NULL, NULL);
}

MEDIT(medit_desc)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &pMob->description, NULL, NULL);
}

MEDIT(medit_comments)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &pMob->comments, NULL, NULL);
}


MEDIT(medit_long)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  long [string]\n\r", ch);
        return false;
    }

    char processed[MSL];
    snprintf(processed, sizeof(processed), "%s{x\n\r", argument);
    if (processed[0] != '\0')
        processed[0] = UPPER(processed[0]);

    return olc_cmd_string(ch, processed, "Long", NULL,
        &pMob->long_descr, 0, NULL, NULL);
}


MEDIT(medit_short)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);

    if (!olc_cmd_string(ch, argument, "Short", NULL,
            &pMob->short_descr, 0, NULL, NULL))
        return false;

    if (IS_SET(ch->act[0], PLR_AUTOSETNAME)) {
        char *auto_name = short_to_name(argument);
        if (!IS_NULLSTR(auto_name)) {
            olc_cmd_string(ch, auto_name, "Name", NULL,
                &pMob->player_name, 0, NULL, NULL);
            free_string(auto_name);
        }
    }
    return true;
}


MEDIT(medit_name)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    char player_dir_buf[MSL];
    const char *player_dir;
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  name <text**>\n\r"
                     "** - <text> must conform to naming restrictions.\n\r", ch);
        return false;
    }

    if (!olc_validate_name(ch, argument))
        return false;

    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(name, sizeof(name), "%s%c/%s", player_dir, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) != NULL) {
        send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
        fclose(fp);
        return false;
    }

    return olc_cmd_string(ch, argument, "Name", NULL,
        &pMob->player_name, 0, NULL, NULL);
}


MEDIT(medit_sign)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 154) {
        send_to_char("Sorry, only immortals of level 154 and above can do that.\n\r", ch);
        return false;
    }

    return olc_cmd_string(ch, ch->name, "Signature", NULL,
        &pMob->sig, OLC_STR_DEFAULT, NULL, NULL);
}

MEDIT(medit_skeywds)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    char player_dir_buf[MSL];
    const char *player_dir;
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax:  skwds [string]\n\r", ch);
        return false;
    }

    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(name, sizeof(name), "%s%c/%s", player_dir, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) != NULL) {
        send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
        fclose(fp);
        return false;
    }

    return olc_cmd_string(ch, argument, "Script Keywords", NULL,
        &pMob->skeywds, 0, NULL, NULL);
}

MEDIT(medit_listname)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "List Name", NULL, &pMob->list_name,
        OLC_STR_CLEARABLE, NULL, NULL);
}

MEDIT(medit_listkeywords)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "List Keywords", NULL, &pMob->list_keywords,
        OLC_STR_CLEARABLE, NULL, NULL);
}

MEDIT(medit_tags)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Tags", NULL, &pMob->tags,
        OLC_STR_CLEARABLE, NULL, NULL);
}

MEDIT(medit_parent)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *parent = NULL;
    WNUM wnum;

    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument))
    {
        send_to_char("Syntax: parent <widevnum|none>\n\r", ch);
        return false;
    }

    /* Determine the value to stage/apply */
    const char *stage_val = argument;

    if (str_cmp(argument, "none") && str_cmp(argument, "clear") && str_cmp(argument, "0"))
    {
        /* Not a clear — validate the widevnum */
        if (!parse_widevnum(argument, pMob->area, &wnum))
        {
            send_to_char("Invalid widevnum. Use vnum, #vnum, or area#vnum.\n\r", ch);
            return false;
        }

        parent = get_mob_index(wnum.pArea, wnum.vnum);
        if (!parent)
        {
            send_to_char("That parent mobile does not exist.\n\r", ch);
            return false;
        }

        if (parent == pMob)
        {
            send_to_char("A mobile cannot inherit from itself.\n\r", ch);
            return false;
        }
    }

    /* Staged mode */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;

                char old_buf[MAX_INPUT_LENGTH];
                if (pMob->parent_load.auid > 0)
                    snprintf(old_buf, sizeof(old_buf), "%ld#%ld",
                        pMob->parent_load.auid, pMob->parent_load.vnum);
                else
                    strlcpy(old_buf, "none", sizeof(old_buf));

                json_t *old_val = json_string(old_buf);
                json_t *new_val = json_string(stage_val);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, "Parent", OLC_FIELD_STRING, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x Parent set to %s.\n\r", stage_val);
                else
                    printf_to_char(ch, "Parent reverted to original value.\n\r");
                return result != NULL;
            }
        }
    }

    /* Non-staged: apply directly */
    if (!str_cmp(argument, "none") || !str_cmp(argument, "clear") || !str_cmp(argument, "0"))
    {
        pMob->parent_load.auid = 0;
        pMob->parent_load.vnum = 0;
        pMob->parent_wnum.pArea = NULL;
        pMob->parent_wnum.vnum = 0;
        pMob->parent = NULL;
        send_to_char("Parent mobile cleared.\n\r", ch);
        return true;
    }

    pMob->parent_load.auid = wnum.pArea->uid;
    pMob->parent_load.vnum = wnum.vnum;
    pMob->parent_wnum = wnum;
    pMob->parent = parent;
    pMob->parent_inherited = false;

    send_to_char("Parent mobile set. Inheritance applied immediately.\n\r", ch);
    return true;
}


MEDIT(medit_varset)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            char varname[MAX_INPUT_LENGTH - 8];
            one_argument(argument, varname);
            if (IS_NULLSTR(varname)) {
                send_to_char("Syntax: varset <name> <type> <value>\n\r", ch);
                return false;
            }
            char field_path[MAX_INPUT_LENGTH];
            snprintf(field_path, sizeof(field_path), "var/%s", varname);

            if (!olc_check_staging_limits(ch, cs)) return false;
            json_t *old_val = json_null();
            json_t *new_val = json_string(argument);
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, field_path, OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x Variable %s staged.\n\r", varname);
            else
                printf_to_char(ch, "Variable %s reverted.\n\r", varname);
            return result != NULL;
        }
    }

    return olc_varset(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_varclear)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            char varname[MAX_INPUT_LENGTH - 8];
            one_argument(argument, varname);
            if (IS_NULLSTR(varname)) {
                send_to_char("Syntax: varclear <name>\n\r", ch);
                return false;
            }
            char field_path[MAX_INPUT_LENGTH];
            snprintf(field_path, sizeof(field_path), "var/%s", varname);

            if (!olc_check_staging_limits(ch, cs)) return false;
            json_t *old_val = json_null();
            json_t *new_val = json_null();
            olc_pending_change_t *result = olc_changeset_add_change(
                cs, field_path, OLC_FIELD_STRING, old_val, new_val);
            json_decref(old_val);
            json_decref(new_val);

            if (result)
                printf_to_char(ch, "{G[STAGED]{x Variable %s clear staged.\n\r", varname);
            else
                printf_to_char(ch, "Variable %s reverted.\n\r", varname);
            return result != NULL;
        }
    }

    return olc_varclear(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_corpsetype)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_type_set(ch, argument, "Corpse Type",
        "Syntax: corpsetype [type]\n\rType '? corpsetypes' for a list of types.\n\r",
        &pMob->corpse_type, corpse_types, NULL, NULL);
}

MEDIT(medit_corpsevnum)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0') {
        send_to_char("Syntax: corpsevnum [widevnum] (0 to clear)\n\r", ch);
        return false;
    }

    /* Validate before staging */
    if (str_cmp(argument, "0")) {
        WNUM obj_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pMob->area, argument);
        if (!parse_widevnum(argument, context, &obj_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
            send_to_char("Object does not exist.\n\r", ch);
            return false;
        }
    }

    /* Staged mode */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                char old_buf[32];
                snprintf(old_buf, sizeof(old_buf), "%ld", pMob->corpse_load.vnum);
                json_t *old_val = json_string(old_buf);
                json_t *new_val = json_string(argument);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, "Corpse Vnum", OLC_FIELD_STRING, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x Corpse vnum set to %s.\n\r", argument);
                else
                    printf_to_char(ch, "Corpse vnum reverted to original value.\n\r");
                return result != NULL;
            }
        }
    }

    /* Non-staged fallback (existing behavior) */
    if (!str_cmp(argument, "0")) {
        send_to_char("Corpse object cleared.\n\r", ch);
        pMob->corpse_load.vnum = 0;
        return true;
    }

    WNUM obj_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pMob->area, argument);
    if (!parse_widevnum(argument, context, &obj_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }
    if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
        send_to_char("Object does not exist.\n\r", ch);
        return false;
    }
    send_to_char("Corpse object vnum set.\n\r", ch);
    pMob->corpse_load.vnum = obj_wnum.vnum;
    return true;
}

MEDIT(medit_zombievnum)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0') {
        send_to_char("Syntax: zombievnum [widevnum] (0 to clear)\n\r", ch);
        return false;
    }

    /* Validate before staging */
    if (str_cmp(argument, "0")) {
        WNUM obj_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pMob->area, argument);
        if (!parse_widevnum(argument, context, &obj_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
            send_to_char("Object does not exist.\n\r", ch);
            return false;
        }
    }

    /* Staged mode */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                char old_buf[32];
                snprintf(old_buf, sizeof(old_buf), "%ld", pMob->zombie_load.vnum);
                json_t *old_val = json_string(old_buf);
                json_t *new_val = json_string(argument);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, "Zombie Vnum", OLC_FIELD_STRING, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x Zombie vnum set to %s.\n\r", argument);
                else
                    printf_to_char(ch, "Zombie vnum reverted to original value.\n\r");
                return result != NULL;
            }
        }
    }

    /* Non-staged fallback (existing behavior) */
    if (!str_cmp(argument, "0")) {
        send_to_char("Zombie corpse object cleared.\n\r", ch);
        pMob->zombie_load.vnum = 0;
        return true;
    }

    WNUM obj_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pMob->area, argument);
    if (!parse_widevnum(argument, context, &obj_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }
    if (!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
        send_to_char("Object does not exist.\n\r", ch);
        return false;
    }
    send_to_char("Zombie corpse object set.\n\r", ch);
    pMob->zombie_load.vnum = obj_wnum.vnum;
    return true;
}

MEDIT(medit_shop)
{
    MOB_INDEX_DATA *pMob;
    char command[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *flag_start;

    argument = one_argument(argument, command);
    flag_start = argument;
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    EDIT_MOB(ch, pMob);

    if (command[0] == '\0')
    {
        send_to_char("Syntax:  shop assign\n\r", ch);
        send_to_char("         shop remove\n\r\n\r", ch);

        send_to_char("         shop discount [0-100] [reset]\n\r", ch);
        send_to_char("         shop flags [flags]\n\r", ch);
        send_to_char("         shop hours [#xopening] [#xclosing]\n\r", ch);
        send_to_char("         shop profit [#xbuying%] [#xselling%]\n\r", ch);
        send_to_char("         shop restock [minutes]\n\r", ch);
        send_to_char("         shop shipyard clear\n\r", ch);
        send_to_char("         shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
        send_to_char("         shop stock add [type] [value]\n\r", ch);
        send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
        send_to_char("         shop stock [#] description [description]\n\r", ch);
        send_to_char("         shop stock [#] duration [#hours|none]\n\r", ch);
        send_to_char("         shop stock [#] level [level]\n\r", ch);
        send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
        send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
        send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
        send_to_char("         shop stock [#] singular\n\r", ch);
        send_to_char("         shop stock [#] remove\n\r", ch);
        send_to_char("         shop type [#x0-4] [item type]\n\r", ch);
        return false;
    }


    if (!str_prefix(command, "hours"))
    {
        if (arg1[0] == '\0' || !is_number(arg1) ||
            arg2[0] == '\0' || !is_number(arg2))
        {
            send_to_char("Syntax:  shop hours [#xopening] [#xclosing]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        pMob->pShop->open_hour = atoi(arg1);
        pMob->pShop->close_hour = atoi(arg2);

        send_to_char("Shop hours set.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "restock"))
    {
        if (arg1[0] == '\0' || !is_number(arg1))
        {
            send_to_char("Syntax:  shop restock [minutes]\n\r", ch);
            send_to_char("   Specify at least 10 minutes, or 0 to disable restocking.\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        int interval = atoi(arg1);

        if( interval <= 0 )
        {
            send_to_char("Restocking disabled.\n\r", ch);
            pMob->pShop->restock_interval = 0;
            return true;
        }
        else if( interval < 10 )
        {
            send_to_char("Interval too short.\n\rPlease try at least 10 minutes, or 0 to disable restocking.\n\r", ch);
            return false;
        }
        else
        {
            send_to_char("Restocking changed.\n\r", ch);
            pMob->pShop->restock_interval = interval;
            return true;
        }
    }

    if (!str_prefix(command, "shipyard"))
    {
        char arg3[MIL];
        char arg4[MIL];
        char arg5[MIL];
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);
        argument = one_argument(argument, arg5);

        if( !str_cmp(arg1, "clear") )
        {
            pMob->pShop->shipyard = 0;
            pMob->pShop->shipyard_region[0][0] = 0;
            pMob->pShop->shipyard_region[0][1] = 0;
            pMob->pShop->shipyard_region[1][0] = 0;
            pMob->pShop->shipyard_region[1][1] = 0;

            free_string(pMob->pShop->shipyard_description);
            pMob->pShop->shipyard_description = &str_empty[0];

            send_to_char("Shipyard cleared.\n\r", ch);
            return true;
        }
        if( !is_number(arg1) || !is_number(arg2) || !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || IS_NULLSTR(argument) )
        {
            send_to_char("Syntax:  shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
            send_to_char("         shop shipyard clear\n\r", ch);
            return false;
        }
        long wuid = atol(arg1);
        int x1 = atoi(arg2);
        int y1 = atoi(arg3);
        int x2 = atoi(arg4);
        int y2 = atoi(arg5);

        if( !is_shipyard_valid(wuid, x1, y1, x2, y2) )
        {
            send_to_char("Shipyard not valid.  Please verify wilderness and coordinates.\n\r", ch);
            send_to_char("Make sure Shipyard has safe harbor water tiles next to non-water tiles.\n\r", ch);
            return false;
        }

        pMob->pShop->shipyard = wuid;
        pMob->pShop->shipyard_region[0][0] = x1;
        pMob->pShop->shipyard_region[0][1] = y1;
        pMob->pShop->shipyard_region[1][0] = x2;
        pMob->pShop->shipyard_region[1][1] = y2;

        smash_tilde(argument);
        free_string(pMob->pShop->shipyard_description);
        pMob->pShop->shipyard_description = str_dup(argument);


        send_to_char("Shipyard set.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "discount"))
    {
        if (arg1[0] == '\0' || !is_number(arg1))
        {
            send_to_char("Syntax:  shop discount [0-100] [reset]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        int disc = atoi(arg1);

        if( disc < 0 || disc > 100 )
        {
            send_to_char("Discount must be a percentage (0-100).\n\r", ch);
            return false;
        }

        pMob->pShop->discount = disc;

        if( !str_cmp(arg2, "reset") && pMob->pShop->stock != NULL )
        {
            bool updated = false;
            for(SHOP_STOCK_DATA *stock = pMob->pShop->stock; stock; stock = stock->next)
            {
                if( IS_NULLSTR(stock->custom_keyword) )
                {
                    stock->discount = pMob->pShop->discount;
                    updated = true;
                }
            }

            if( updated )
                send_to_char("Discount changed, and stock updated.\n\r", ch);
            else
                send_to_char("Discount changed.\n\r", ch);
        }
        else
            send_to_char("Discount changed.\n\r", ch);
        return true;
    }


    if (!str_prefix(command, "profit"))
    {
        if (arg1[0] == '\0' || !is_number(arg1) ||
            arg2[0] == '\0' || !is_number(arg2))
        {
            send_to_char("Syntax:  shop profit [#xbuying%] [#xselling%]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        pMob->pShop->profit_buy     = atoi(arg1);
        pMob->pShop->profit_sell    = atoi(arg2);

        send_to_char("Shop profit set.\n\r", ch);
        return true;
    }


    if (!str_prefix(command, "type"))
    {
        char buf[MAX_INPUT_LENGTH];
        int value;

        if (arg1[0] == '\0' || !is_number(arg1) || arg2[0] == '\0')
        {
            send_to_char("Syntax:  shop type [#x0-4] [item type]\n\r", ch);
            return false;
        }

        if (atoi(arg1) >= MAX_TRADE)
        {
            sprintf(buf, "MEdit:  May sell %d items max.\n\r", MAX_TRADE);
            send_to_char(buf, ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        if ((value = flag_value(type_flags, arg2)) == NO_FLAG)
        {
            send_to_char("MEdit:  That type of item is not known.\n\r", ch);
            return false;
        }

        pMob->pShop->buy_type[atoi(arg1)] = value;

        send_to_char("Shop type set.\n\r", ch);
        return true;
    }

    /* shop assign && shop delete by Phoenix */

    if (!str_prefix(command, "assign"))
    {
        if (pMob->pShop)
        {
            send_to_char("Mob already has a shop assigned to it.\n\r", ch);
            return false;
        }

        pMob->pShop		= new_shop();
        if (!shop_first)
                shop_first	= pMob->pShop;
        if (shop_last)
            shop_last->next	= pMob->pShop;
        shop_last		= pMob->pShop;

        pMob->pShop->keeper	= pMob->vnum;

        send_to_char("New shop assigned to mobile.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "remove"))
    {
        SHOP_DATA *pShop;

        pShop		= pMob->pShop;
        pMob->pShop	= NULL;

        if (pShop == shop_first)
        {
            if (!pShop->next)
            {
                shop_first = NULL;
                shop_last = NULL;
            }
            else
                shop_first = pShop->next;
        }
        else
        {
            SHOP_DATA *ipShop;

            for (ipShop = shop_first; ipShop; ipShop = ipShop->next)
            {
                if (ipShop->next == pShop)
                {
                    if (!pShop->next)
                    {
                        shop_last = ipShop;
                        shop_last->next = NULL;
                    }
                    else
                        ipShop->next = pShop->next;
                }
            }
        }

        free_shop(pShop);

        send_to_char("Mobile is no longer a shopkeeper.\n\r", ch);
        return true;
    }

    if(!str_prefix(command, "flags"))
    {
        int value;
        if (flag_start[0] != '\0')
        {

            if ((value = flag_value(shop_flags, flag_start)) != NO_FLAG)
            {
                pMob->pShop->flags ^= value;

                send_to_char("Shop flags toggled.\n\r", ch);
                return true;
            }
        }

        send_to_char(	"Syntax: shop flags [flag]\n\r"
                        "Type '? shop' for a list of flags.\n\r", ch);

        return false;
    }

    if(!str_prefix(command, "stock"))
    {
        SHOP_STOCK_DATA *stock;

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        if(arg1[0] == '\0')
        {
            send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
            send_to_char("         shop stock add pet [vnum]\n\r", ch);
            send_to_char("         shop stock add mount [vnum]\n\r", ch);
            send_to_char("         shop stock add guard [vnum]\n\r", ch);
            send_to_char("         shop stock add crew [vnum]\n\r", ch);
            send_to_char("         shop stock add ship [vnum]\n\r", ch);
            send_to_char("         shop stock add custom [keyword]\n\r", ch);
            send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
            send_to_char("         shop stock [#] description [description]\n\r", ch);
            send_to_char("         shop stock [#] duration [#hours|none]\n\r", ch);
            send_to_char("         shop stock [#] level [level]\n\r", ch);
            send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
            send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
            send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
            send_to_char("         shop stock [#] singular\n\r", ch);
            send_to_char("         shop stock [#] remove\n\r", ch);
            return false;
        }

        if(!str_prefix(arg1, "add"))
        {
            if(arg2[0] == '\0' || argument[0] == '\0')
            {
                send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
                send_to_char("         shop stock add pet [vnum]\n\r", ch);
                send_to_char("         shop stock add mount [vnum]\n\r", ch);
                send_to_char("         shop stock add guard [vnum]\n\r", ch);
                send_to_char("         shop stock add crew [vnum]\n\r", ch);
                send_to_char("         shop stock add ship [vnum]\n\r", ch);
                send_to_char("         shop stock add custom [keyword]\n\r", ch);
                return false;
            }

            if(!str_prefix(arg2, "object"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM obj_wnum;
                    AREA_DATA *context = ch->in_room->area;
                    
                    if (!parse_widevnum(argument, context, &obj_wnum)) {
                        send_to_char("Invalid object vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    OBJ_INDEX_DATA *item = obj_wnum.pArea ? 
                        get_obj_index(obj_wnum.pArea, obj_wnum.vnum) :
                        get_obj_index_global(obj_wnum.vnum);
                    
                    if(!item)
                    {
                        send_to_char("Object does not exist.\n\r", ch);
                        return false;
                    }

                    if(item->item_type == ITEM_MONEY)
                    {
                        send_to_char("You cannot sell money.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_OBJECT;
                    stock->entity.wnum = obj_wnum;
                    stock->silver = item->cost;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (OBJECT) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add object [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "pet"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = ch->in_room->area;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_PET;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 10 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (PET) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add pet [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "mount"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = ch->in_room->area;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_MOUNT;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 25 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (MOUNT) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add mount [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "guard"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = ch->in_room->area;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_GUARD;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 50 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (GUARD) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add guard [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "crew"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = ch->in_room->area;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    if(!mob->pCrew)
                    {
                        send_to_char("Mobile has no Crew definition.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_CREW;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 50 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (CREW) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add crew [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "ship"))
            {
                if( !is_shipyard_valid(pMob->pShop->shipyard,
                    pMob->pShop->shipyard_region[0][0],
                    pMob->pShop->shipyard_region[0][1],
                    pMob->pShop->shipyard_region[1][0],
                    pMob->pShop->shipyard_region[1][1]) )
                {
                    send_to_char("Shopkeeper needs to have a valid shipyard defined first before you can add a ship.\n\r", ch);
                    return false;
                }

                if( is_number(argument) )
                {
                    long vnum = atol(argument);
                    SHIP_INDEX_DATA *ship;

                    if( !(ship = get_ship_index(vnum)) )
                    {
                        send_to_char("That ship does not exist.\n\r", ch);
                        return false;
                    }

                    if( !IS_VALID(ship->blueprint) || !ship->ship_object )
                    {
                        send_to_char("Ship is incomplete.  Cannot be sold yet.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_SHIP;
                    stock->entity.wnum.pArea = NULL;
                    stock->entity.wnum.vnum = vnum;
                    stock->silver = 100000;	// Default 1000gold
                    stock->level = 1;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (SHIP) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add ship [vnum]\n\r", ch);
                return false;

            }
            else if(!str_prefix(arg2, "custom"))
            {
                if(!IS_NULLSTR(argument))
                {
                    for(stock = pMob->pShop->stock; stock; stock = stock->next)
                    {
                        if( (stock->type == STOCK_CUSTOM) &&
                            !str_cmp(argument, stock->custom_keyword) )
                        {
                            break;
                        }
                    }

                    if( stock != NULL )
                    {
                        send_to_char("Keyword already used.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_CUSTOM;
                    stock->custom_keyword = str_dup(argument);
                    stock->discount = 0;		// They do not handle discounts.
                                                // If you wish to do discounts, that has to be scripted.

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (CUSTOM) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add custom [keyword]\n\r", ch);
                return false;
            }

            send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
            send_to_char("         shop stock add pet [vnum]\n\r", ch);
            send_to_char("         shop stock add mount [vnum]\n\r", ch);
            send_to_char("         shop stock add guard [vnum]\n\r", ch);
            send_to_char("         shop stock add custom [keyword]\n\r", ch);
            return false;
        }

        if(is_number(arg1))
        {
            int idx = atoi(arg1);
            stock = get_shop_stock_bypos(pMob->pShop, idx);

            if(!stock)
            {
                send_to_char("Invalid stock number.\n\r", ch);
                return false;
            }


            if(!str_prefix(arg2, "duration"))
            {
                int duration;
                if (!str_prefix(argument, "none"))
                    duration = 0;
                else if (!is_number(argument) || (duration = atoi(argument)) < 1)
                {
                    send_to_char("Please provide a positive number or none.\n\r", ch);
                    return false;
                }

                stock->duration = duration;
                send_to_char("Stock duration changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "price"))
            {
                char arg3[MIL];

                argument = one_argument(argument, arg3);

                if(!str_prefix(arg3, "silver"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Silver price must be a number.\n\r", ch);
                        return false;
                    }

                    int silver = atoi(argument);

                    stock->silver = UMAX(silver, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock silver price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "qp"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Quest point price must be a number.\n\r", ch);
                        return false;
                    }

                    int qp = atoi(argument);

                    stock->qp = UMAX(qp, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock quest point price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "dp"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Deity point price must be a number.\n\r", ch);
                        return false;
                    }

                    int dp = atoi(argument);

                    stock->dp = UMAX(dp, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock deity point price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "pneuma"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Pneuma price must be a number.\n\r", ch);
                        return false;
                    }

                    int pneuma = atoi(argument);

                    stock->pneuma = UMAX(pneuma, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock pneuma price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "custom"))
                {
                    if(argument[0] == '\0')
                    {
                        send_to_char("Please specify a custom price string.\n\r", ch);
                        send_to_char("Syntax:  shop stock [#] price custom [value]\n\r\n\r", ch);
                        send_to_char("If you wish to clear the custom pricing, select a different pricing type.\n\r", ch);
                        return false;
                    }

                    stock->silver = 0;
                    stock->qp = 0;
                    stock->dp = 0;
                    stock->pneuma = 0;
                    stock->discount = 0;
                    free_string(stock->custom_price);
                    stock->custom_price = str_dup(argument);
                    send_to_char("Stock custom price changed.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
                return false;
            }

            if(!str_prefix(arg2, "discount"))
            {
                if( !IS_NULLSTR(stock->custom_price) )
                {
                    send_to_char("Stock items with custom pricing do not receive discounts.\n\r", ch);
                    send_to_char("Those need to be handled in the CUSTOM_PRICE trigger.\n\r", ch);
                    return false;
                }

                if(!is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] discount [0-100]\n\r", ch);
                    return false;
                }

                int disc = atoi(argument);

                if(disc < 0 || disc > 100)
                {
                    send_to_char("Discount must be a percentage (0-100).\n\r", ch);
                    return false;
                }

                stock->discount = disc;
                send_to_char("Stock discount changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "level"))
            {
                if(!is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] level [level]\n\r", ch);
                    return false;
                }

                int lvl = atoi(argument);

                if(lvl < 1)
                {
                    stock->level = 0;
                    send_to_char("Stock level set to automatic.\n\r", ch);
                    return true;
                }

                stock->level = lvl;
                send_to_char("Stock level changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "singular"))
            {
                stock->singular = !stock->singular;
                if(stock->singular)
                    send_to_char("Stock is now singular.\n\r", ch);
                else
                    send_to_char("Stock is no longer singular.\n\r", ch);
                return true;
            }


            if(!str_prefix(arg2, "quantity"))
            {
                if(!str_prefix(argument, "unlimited"))
                {
                    stock->quantity = 0;
                    stock->restock_rate = 0;
                    send_to_char("Stock quantity settings changed.\n\r", ch);
                    return true;
                }

                char arg3[MIL];
                argument = one_argument(argument, arg3);
                if(!is_number(arg3) || !is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] quantity [total] [reset rate]\n\r", ch);
                    return false;
                }

                int total = atoi(arg3);
                int rate = atoi(argument);

                if(total < 1)
                {
                    send_to_char("Please specify a positive number for limited quantity.\n\r", ch);
                    return false;
                }

                stock->quantity = total;
                stock->restock_rate = UMAX(rate, 0);		// A rate of zero means it never restock
                send_to_char("Stock quantity settings changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "description"))
            {
                free_string(stock->custom_descr);
                stock->custom_descr = str_dup(argument);

                send_to_char("Stock description changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "remove"))
            {
                if( idx < 1 )
                {
                    send_to_char("Please specify a positive number.\n\r", ch);
                    return false;
                }

                SHOP_STOCK_DATA *prev = NULL;
                for(stock = pMob->pShop->stock;stock;prev = stock, stock = stock->next)
                {
                    if(!--idx)
                        break;
                }

                if( !stock )
                {
                    send_to_char("Invalid stock number.\n\r", ch);
                    return false;
                }

                if( prev != NULL )
                {
                    prev->next = stock->next;
                }
                else
                {
                    pMob->pShop->stock = stock->next;
                }

                free_shop_stock(stock);
                send_to_char("Stock item removed.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  shop stock [#] description [description]\n\r", ch);
            send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
            send_to_char("         shop stock [#] level [level]\n\r", ch);
            send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
            send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
            send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
            send_to_char("         shop stock [#] singular\n\r", ch);
            send_to_char("         shop stock [#] remove\n\r", ch);
            return false;
        }

        medit_shop(ch, "stock");
        return false;
    }

    medit_shop(ch, "");
    return false;
}


MEDIT(medit_sex)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_type_set_i16(ch, argument, "Sex",
        "Syntax: sex [sex]\n\rType '? sex' for a list of flags.\n\r",
        &pMob->sex, sex_flags, NULL, NULL);
}

MEDIT(medit_bodytype)
{
    MOB_INDEX_DATA *pMob;
    int16_t old_body_type;
    int16_t body_type_value;

    EDIT_MOB(ch, pMob);

    old_body_type = (int16_t)pMob->body_type;
    body_type_value = (int16_t)pMob->body_type;
    if (!olc_cmd_type_set_i16(ch, argument, "Body Type",
        "Syntax: bodytype [type]\n\rType '? body_types' for a list of body types.\n\r",
        &body_type_value, body_types, NULL, NULL)) {
        return false;
    }

    pMob->body_type = (body_type_t)body_type_value;

    if (pMob->body_type < 0 || pMob->body_type >= BODY_TYPE_MAX)
        pMob->body_type = BODY_TYPE_NEUTRAL;

    if (pMob->body_type == BODY_TYPE_MALE)
        pMob->sex = 1;
    else if (pMob->body_type == BODY_TYPE_FEMALE)
        pMob->sex = 2;
    else if (pMob->body_type == BODY_TYPE_RANDOM)
        pMob->sex = 3;
    else
        pMob->sex = 0;

    if (old_body_type != pMob->body_type) {
        free_string(pMob->pronoun_he_she);
        pMob->pronoun_he_she = str_dup(body_type_info[pMob->body_type].default_he_she);
        free_string(pMob->pronoun_him_her);
        pMob->pronoun_him_her = str_dup(body_type_info[pMob->body_type].default_him_her);
        free_string(pMob->pronoun_his_her);
        pMob->pronoun_his_her = str_dup(body_type_info[pMob->body_type].default_his_her);
        free_string(pMob->pronoun_his_hers);
        pMob->pronoun_his_hers = str_dup(body_type_info[pMob->body_type].default_his_hers);
        free_string(pMob->pronoun_himself_herself);
        pMob->pronoun_himself_herself = str_dup(body_type_info[pMob->body_type].default_himself_herself);
        pMob->verb_preference = body_type_info[pMob->body_type].verb_preference;
        send_to_char("Pronouns reset to body type defaults.\n\r", ch);
    }

    return true;
}

MEDIT(medit_pronounss)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Subjective Pronoun",
        "Syntax: pronounss <value>\n\r",
        &pMob->pronoun_he_she, OLC_STR_DEFAULT, NULL, NULL);
}

MEDIT(medit_pronounos)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Objective Pronoun",
        "Syntax: pronounos <value>\n\r",
        &pMob->pronoun_him_her, OLC_STR_DEFAULT, NULL, NULL);
}

MEDIT(medit_pronounpas)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Possessive Adjective Pronoun",
        "Syntax: pronounpas <value>\n\r",
        &pMob->pronoun_his_her, OLC_STR_DEFAULT, NULL, NULL);
}

MEDIT(medit_pronounpps)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Possessive Pronoun",
        "Syntax: pronounpps <value>\n\r",
        &pMob->pronoun_his_hers, OLC_STR_DEFAULT, NULL, NULL);
}

MEDIT(medit_pronounrs)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Reflexive Pronoun",
        "Syntax: pronounrs <value>\n\r",
        &pMob->pronoun_himself_herself, OLC_STR_DEFAULT, NULL, NULL);
}


MEDIT(medit_act)
{
    MOB_INDEX_DATA *pMob;

    if (argument[0] != '\0')
    {
        EDIT_MOB(ch, pMob);

        long bits[2] = {0};
        if (!bitvector_lookup(argument, 2, bits, act_flags, act2_flags))
        {
            send_to_char("Syntax: act [flag]\n\rType '? act' for a list of flags.\n\r", ch);
            return false;
        }

        /* Staged mode */
        if (olc_stage_bitvector(ch, "Act", pMob->act, bits, 2))
            return true;

        /* Non-staged fallback */
        TOGGLE_BIT(pMob->act[0], bits[0]);
        TOGGLE_BIT(pMob->act[1], bits[1]);
        SET_BIT(pMob->act[0], ACT_IS_NPC);

        send_to_char("Act flag toggled.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: act [flag]\n\r"
          "Type '? act' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_affect)
{
    MOB_INDEX_DATA *pMob;

    if (argument[0] != '\0')
    {
        EDIT_MOB(ch, pMob);

        long bits[2] = {0};
        if (!bitvector_lookup(argument, 2, bits, affect_flags, affect2_flags))
        {
            send_to_char("Syntax: affect [flag]\n\rType '? affect' for a list of flags.\n\r", ch);
            return false;
        }

        if (olc_stage_bitvector(ch, "Affected By", pMob->affected_by, bits, 2))
            return true;

        TOGGLE_BIT(pMob->affected_by[0], bits[0]);
        TOGGLE_BIT(pMob->affected_by[1], bits[1]);

        send_to_char("Affect flag toggled.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: affect [flag]\n\r"
          "Type '? affect' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_ac)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];
    int16_t values[4];
    bool has_value[4] = {false};

    if (argument[0] == '\0') {
        send_to_char("Syntax:  ac [ac-pierce [ac-bash [ac-slash [ac-exotic]]]]\n\r"
            "help MOB_AC  gives a list of reasonable ac-values.\n\r", ch);
        return false;
    }

    EDIT_MOB(ch, pMob);

    /* Parse first argument (pierce) — required */
    argument = one_argument(argument, arg);
    if (!is_number(arg)) {
        send_to_char("Syntax:  ac [ac-pierce [ac-bash [ac-slash [ac-exotic]]]]\n\r", ch);
        return false;
    }
    values[0] = (int16_t)atoi(arg);
    has_value[0] = true;

    /* Parse optional arguments */
    const char *labels[4] = {"AC Pierce", "AC Bash", "AC Slash", "AC Exotic"};
    int16_t *fields[4] = {&pMob->ac[AC_PIERCE], &pMob->ac[AC_BASH], &pMob->ac[AC_SLASH], &pMob->ac[AC_EXOTIC]};

    for (int i = 1; i < 4; i++) {
        argument = one_argument(argument, arg);
        if (arg[0] == '\0') break;
        if (!is_number(arg)) {
            send_to_char("Syntax:  ac [ac-pierce [ac-bash [ac-slash [ac-exotic]]]]\n\r", ch);
            return false;
        }
        values[i] = (int16_t)atoi(arg);
        has_value[i] = true;
    }

    /* Stage or apply each provided value */
    const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
    if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
        olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
        if (cs) {
            if (!olc_check_staging_limits(ch, cs)) return false;
            for (int i = 0; i < 4; i++) {
                if (!has_value[i]) continue;
                json_t *old_j = json_integer((int)*fields[i]);
                json_t *new_j = json_integer((int)values[i]);
                olc_changeset_add_change(cs, labels[i], OLC_FIELD_INT16, old_j, new_j);
                json_decref(old_j);
                json_decref(new_j);
            }
            send_to_char("{G[STAGED]{x AC values staged.\n\r", ch);
            return true;
        }
    }

    /* Non-staged fallback */
    for (int i = 0; i < 4; i++) {
        if (has_value[i])
            *fields[i] = values[i];
    }
    send_to_char("Ac set.\n\r", ch);
    return true;
}


MEDIT(medit_form)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Form",
        "Syntax: form [flags]\n\rType '? form' for a list of flags.\n\r",
        &pMob->form, form_flags, NULL, NULL);
}


MEDIT(medit_part)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Parts",
        "Syntax: part [flags]\n\rType '? part' for a list of flags.\n\r",
        &pMob->parts, part_flags, NULL, NULL);
}


MEDIT(medit_immune)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Immunity",
        "Syntax: imm [flags]\n\rType '? imm' for a list of flags.\n\r",
        &pMob->imm_flags, imm_flags, NULL, NULL);
}


MEDIT(medit_res)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Resistance",
        "Syntax: res [flags]\n\rType '? res' for a list of flags.\n\r",
        &pMob->res_flags, res_flags, NULL, NULL);
}


MEDIT(medit_vuln)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Vulnerability",
        "Syntax: vuln [flags]\n\rType '? vuln' for a list of flags.\n\r",
        &pMob->vuln_flags, vuln_flags, NULL, NULL);
}


MEDIT(medit_material)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_string(ch, argument, "Material", NULL, &pMob->material,
        OLC_STR_DEFAULT, NULL, NULL);
}


MEDIT(medit_off)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_flag_toggle(ch, argument, "Offensive",
        "Syntax: off [flags]\n\rType '? off' for a list of flags.\n\r",
        &pMob->off_flags, off_flags, NULL, NULL);
}


MEDIT(medit_size)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_type_set_i16(ch, argument, "Size",
        "Syntax: size [size]\n\rType '? size' for a list of sizes.\n\r",
        &pMob->size, size_flags, NULL, NULL);
}


MEDIT(medit_hitdice)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 151)
    {
        send_to_char("You do not have permission to edit hit dice.\n\r", ch);
        return false;
    }

    return olc_cmd_dice(ch, argument, "Hit Dice",
        "Syntax:  hitdice <number> d <type> + <bonus>\n\r",
        &pMob->hit, NULL, NULL);
}


MEDIT(medit_manadice)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_dice(ch, argument, "Mana Dice",
        "Syntax:  manadice <number> d <type> + <bonus>\n\r",
        &pMob->mana, NULL, NULL);
}


MEDIT(medit_damdice)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_dice(ch, argument, "Damage Dice",
        "Syntax:  damdice <number> d <type> + <bonus>\n\r",
        &pMob->damage, NULL, NULL);
}


MEDIT(medit_race)
{
    MOB_INDEX_DATA *pMob;
    RACE_DATA *race;

    if (argument[0] != '\0'
    && (race = race_lookup(argument)) != NULL)
    {
    EDIT_MOB(ch, pMob);

    pMob->race = race;
    pMob->act[0]	  |= race->act[0];
    pMob->act[1]	  |= race->act[1];
    pMob->affected_by[0] |= race->aff[0];
    pMob->off_flags   |= race->off;
    pMob->imm_flags   |= race->imm;
    pMob->res_flags   |= race->res;
    pMob->vuln_flags  |= race->vuln;
    pMob->form        |= race->form;
    pMob->parts       |= race->parts;

    send_to_char("Race set.\n\r", ch);
    return true;
    }

    if (argument[0] == '?')
    {
    char buf[MAX_STRING_LENGTH];
    int count = 0;

    send_to_char("Available races are:", ch);

    for (race = race_list; race != NULL; race = race->next)
    {
        if ((count % 3) == 0)
        send_to_char("\n\r", ch);
        sprintf(buf, " %-15s", race->name);
        send_to_char(buf, ch);
        count++;
    }

    send_to_char("\n\r", ch);
    return false;
    }

    send_to_char("Syntax:  race [race]\n\r"
          "Type 'race ?' for a list of races.\n\r", ch);
    return false;
}


MEDIT(medit_position)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if (!str_prefix(arg, "start")) {
        EDIT_MOB(ch, pMob);
        return olc_cmd_type_set_i16(ch, argument, "Start Position", NULL,
            &pMob->start_pos, position_flags, NULL, NULL);
    }

    if (!str_prefix(arg, "default")) {
        EDIT_MOB(ch, pMob);
        return olc_cmd_type_set_i16(ch, argument, "Default Position", NULL,
            &pMob->default_pos, position_flags, NULL, NULL);
    }

    send_to_char("Syntax:  position [start/default] [position]\n\r"
          "Type '? position' for a list of positions.\n\r", ch);
    return false;
}


MEDIT(medit_movedice)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);
    return olc_cmd_long(ch, argument, "Movement", NULL,
        &pMob->move, 0, LONG_MAX, NULL, NULL);
}


MEDIT(medit_gold)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (!IS_NULLSTR(argument) && is_number(argument)) {
        long value = atol(argument);
        if (value > 1000 && !has_imp_sig(pMob, NULL)) {
            send_to_char("Sorry, that's too much. Have an IMP sign this mob if you want to set that much gold.\n\r", ch);
            return false;
        }
    }

    return olc_cmd_long(ch, argument, "Gold", NULL,
        &pMob->wealth, 0, LONG_MAX, NULL, NULL);
}


MEDIT(medit_hitroll)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    return olc_cmd_number_i16(ch, argument, "Hitroll", NULL,
        &pMob->hitroll, INT_MIN, INT_MAX, NULL, NULL);
}

MEDIT (medit_addmprog)
{
    int tindex, value, slot;
    MOB_INDEX_DATA *pMob;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_MOB(ch, pMob);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
        send_to_char("Syntax:   addmprog [widevnum] [trigger] [phrase]\n\r",ch);
        return false;
    }

    if ((tindex = trigger_index(trigger, PRG_MPROG)) < 0) {
        send_to_char("Valid flags are:\n\r",ch);
        show_help(ch, "mprog");
        return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

    if(value == TRIG_SPELLCAST) {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "0");
        }
        else
        {
            int sn = skill_lookup(phrase);
            SKILL_DATA *skill_ref = skill_find_uid(sn);
            if(sn < 0 || !skill_ref || skill_ref->spell_fun == spell_null) {
                send_to_char("Invalid spell for trigger.\n\r",ch);
                return false;
            }
            sprintf(phrase,"%d",sn);
        }
    }
    else if( value == TRIG_EXIT || value == TRIG_EXALL )
    {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door < 0 ) {
                send_to_char("Invalid direction for exit/exall trigger.\n\r", ch);
                return false;
            }
            sprintf(phrase,"%d",door);
        }
    }

    WNUM script_wnum;
    AREA_DATA *context = olc_relative_widevnum_context(pMob->area, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_MPROG)) == NULL)
    {
        send_to_char("No such MOBProgram.\n\r",ch);
        return false;
    }

    // Make sure this has a list of progs!
    if(!pMob->progs) pMob->progs = new_prog_bank();

    if (edit_trigger_exists(pMob->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this mobile.\n\r", ch);
        return false;
    }

    list                  = new_trigger();
    list->vnum            = script_wnum.vnum;
    list->script_is_widevnum = (script_wnum.pArea != NULL);
    if (list->script_is_widevnum) { list->script_load.auid = script_wnum.pArea->uid; list->script_load.vnum = script_wnum.vnum; }
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
    if (is_widevnum_format(phrase)) {
        list->numeric = true;
        list->trig_is_widevnum = true;
        parse_widevnum_load(phrase, &list->trig_load);
        list->trig_number = (int)list->trig_load.vnum;
    } else {
        list->trig_number = atoi(list->trig_phrase);
        list->numeric = is_number(list->trig_phrase);
    }

    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pMob->progs[slot], list);

    send_to_char("Mprog Added.\n\r",ch);
    return true;
}


MEDIT (medit_delmprog)
{
    MOB_INDEX_DATA *pMob;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    EDIT_MOB(ch, pMob);

    if (!pMob->progs) {
        send_to_char("This mobile has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:  delmprog <group#>\n\r", ch);
        send_to_char("         delmprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(pMob->progs, groups, MAX_PROG_GROUPS, PRG_MPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (arg2[0] == '\0') {
        // Delete entire group (all triggers for this script)
        if (edit_delscript(pMob->progs, group->script)) {
            send_to_char("Script group removed.\n\r", ch);
            return true;
        }
    } else {
        // Delete specific trigger within group
        if (!is_number(arg2)) {
            send_to_char("Please specify a valid trigger number within the group.\n\r", ch);
            return false;
        }

        trig_idx = atoi(arg2);
        if (trig_idx < 1 || trig_idx > group->trigger_count) {
            send_to_char("Invalid trigger number within that group.\n\r", ch);
            return false;
        }

        PROG_GROUP_ENTRY *entry = &group->triggers[trig_idx - 1];
        if (edit_deltrigger_specific(pMob->progs, group->script, entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return true;
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}


MEDIT(medit_addquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_V2_LIST *qv2;
    WNUM wnum;
    AREA_DATA *context;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  addquest <quest widevnum>\n\r", ch);
        return false;
    }

    context = olc_relative_widevnum_context(pMob->area, argument);
    if (!parse_widevnum(argument, context, &wnum) || !wnum.pArea || wnum.vnum < 1)
    {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if (!get_quest_index_v2_wnum(wnum))
    {
        send_to_char("No v2 quest with that widevnum exists.\n\r", ch);
        return false;
    }

    for (qv2 = pMob->quests_v2; qv2 != NULL; qv2 = qv2->next)
    {
        if (qv2->load.auid == wnum.pArea->uid && qv2->load.vnum == wnum.vnum)
        {
            send_to_char("That quest is already in the list.\n\r", ch);
            return false;
        }
    }

    qv2 = new_quest_v2_list();
    qv2->load.auid = wnum.pArea->uid;
    qv2->load.vnum = wnum.vnum;
    qv2->wnum = wnum;
    qv2->next = pMob->quests_v2;
    pMob->quests_v2 = qv2;

    send_to_char("Quest added.\n\r", ch);
    return true;
}


MEDIT(medit_addreputation)
{
    char arg[MIL];
    MOB_INDEX_DATA *pMob;
    WNUM wnum;

    EDIT_MOB(ch, pMob);

    argument = one_argument(argument, arg);
    if (!parse_widevnum(arg, ch->in_room ? ch->in_room->area : NULL, &wnum) || !wnum.pArea || wnum.vnum < 1)
    {
        send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
        send_to_char("Please specify a valid reputation widevnum.\n\r", ch);
        return false;
    }

    REPUTATION_INDEX_DATA *repIndex = get_reputation_index(wnum.pArea, wnum.vnum);
    if (!IS_VALID(repIndex))
    {
        send_to_char("No reputation with that widevnum.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);
    int min_rank;
    if (is_number(arg))
    {
        min_rank = atoi(arg);
        if (min_rank < 1 || min_rank > list_size(repIndex->ranks))
        {
            send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
            send_to_char(formatf("Invalid minimum rank. Use 1..%d or none.\n\r", list_size(repIndex->ranks)), ch);
            return false;
        }
    }
    else if (!str_prefix(arg, "none"))
    {
        min_rank = 0;
    }
    else
    {
        send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);
    int max_rank;
    if (is_number(arg))
    {
        max_rank = atoi(arg);
        if (max_rank < 1 || max_rank > list_size(repIndex->ranks))
        {
            send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
            send_to_char(formatf("Invalid maximum rank. Use 1..%d or none.\n\r", list_size(repIndex->ranks)), ch);
            return false;
        }
    }
    else if (!str_prefix(arg, "none"))
    {
        max_rank = 0;
    }
    else
    {
        send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
        return false;
    }

    if (min_rank && max_rank && min_rank > max_rank)
    {
        send_to_char("Minimum rank cannot be greater than maximum rank.\n\r", ch);
        return false;
    }

    long points;
    if (!is_number(argument) || !(points = atol(argument)))
    {
        send_to_char("Syntax:  addreputation <reputation widevnum> <minimum rank|none> <maximum rank|none> <points>\n\r", ch);
        send_to_char("Please specify a non-zero points value.\n\r", ch);
        return false;
    }

    MOB_REPUTATION_DATA *rep, *new_rep;
    new_rep = new_mob_reputation_data();
    new_rep->reputation = repIndex;
    new_rep->reputation_load.auid = repIndex->area ? repIndex->area->uid : 0;
    new_rep->reputation_load.vnum = repIndex->vnum;
    new_rep->minimum_rank = min_rank;
    new_rep->maximum_rank = max_rank;
    new_rep->points = points;

    for (rep = pMob->mob_reputations; rep && rep->next; rep = rep->next)
    ;

    if (rep)
        rep->next = new_rep;
    else
        pMob->mob_reputations = new_rep;

    send_to_char("Reputation reward added.\n\r", ch);
    return true;
}


MEDIT(medit_delreputation)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_STRING_LENGTH];
    int index;
    MOB_REPUTATION_DATA *prev, *rep;

    EDIT_MOB(ch, pMob);

    one_argument(argument, arg);
    if (!is_number(arg) || arg[0] == '\0')
    {
       send_to_char("Syntax:  delreputation <index>\n\r", ch);
       return false;
    }

    index = atoi(arg);
    if (index < 0)
    {
        send_to_char("Please specify a non-negative index.\n\r", ch);
        return false;
    }

    for (prev = NULL, rep = pMob->mob_reputations; rep && index--; prev = rep, rep = rep->next)
    ;

    if (!rep)
    {
        send_to_char("No such reputation entry.\n\r", ch);
        return false;
    }

    if (prev)
        prev->next = rep->next;
    else
        pMob->mob_reputations = rep->next;

    free_mob_reputation_data(rep);

    send_to_char("Reputation reward removed.\n\r", ch);
    return true;
}


MEDIT(medit_delquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_V2_LIST *qv2, *prev = NULL;
    int i, counter;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
        send_to_char("Syntax:  delquest <#>\n\r", ch);
        return false;
    }

    i = atoi(argument) - 1;  /* list display is 1-based */
    if (i < 0)
    {
        send_to_char("Index must be 1 or greater.\n\r", ch);
        return false;
    }

    counter = 0;
    for (qv2 = pMob->quests_v2; qv2 != NULL; qv2 = qv2->next)
    {
        if (counter == i) break;
        prev = qv2;
        counter++;
    }

    if (qv2 == NULL)
    {
        send_to_char("Index not found.\n\r", ch);
        return false;
    }

    if (prev != NULL)
        prev->next = qv2->next;
    else
        pMob->quests_v2 = qv2->next;

    free_quest_v2_list(qv2);
    send_to_char("Quest removed.\n\r", ch);
    return true;
}

MEDIT(medit_questor)
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];

    EDIT_MOB(ch, pMob);

    if(IS_NULLSTR(argument))
    {
        send_to_char("QUESTOR ADD                 Adds questor data to mob.\n\r", ch);
        send_to_char("        REMOVE              Removes questor data from mob.\n\r", ch);
        send_to_char("        SCROLL [vnum]       Sets the scroll object to the specified vnum.\n\r", ch);
        send_to_char("        KEYWORDS [string]   Sets keywords of scroll.\n\r", ch);
        send_to_char("        SHORT [string]      Sets short description of scroll.\n\r", ch);
        send_to_char("        LONG [string]       Sets long description of scroll.\n\r", ch);
        send_to_char("        HEADER              Edits scroll header.\n\r", ch);
        send_to_char("        FOOTER              Edits scroll footer.\n\r", ch);
        send_to_char("        PREFIX [string]     Edits line prefix.\n\r", ch);
        send_to_char("        SUFFIX [string]     Edits line suffix.\n\r", ch);
        send_to_char("        WIDTH [width]       Sets line width.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_prefix(arg,"add"))
    {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
        {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        if( pMob->pQuestor != NULL )
        {
            send_to_char("There is already questor data.\n\r", ch);
            return false;
        }

        pMob->pQuestor = new_questor_data();
        use_imp_sig(pMob, NULL);
        send_to_char("Questor data added.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"remove")) {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
        {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        if( pMob->pQuestor == NULL )
        {
            send_to_char("There is any questor data.\n\r", ch);
            return false;
        }

        free_questor_data(pMob->pQuestor);
        pMob->pQuestor = NULL;
        send_to_char("Questor data removed.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"scroll")) {
        if(argument[0] == '\0')
        {
            send_to_char("Syntax: questor scroll [widevnum]\n\r", ch);
            return false;
        }

        WNUM obj_wnum;
        AREA_DATA *context = olc_relative_widevnum_context(pMob->area, argument);
        if (!parse_widevnum(argument, context, &obj_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        if( !get_obj_index(obj_wnum.pArea, obj_wnum.vnum) )
        {
            send_to_char("Object does not exist.\n\r", ch);
            return false;
        }

        pMob->pQuestor->scroll = obj_wnum.vnum;
        send_to_char("Questor scroll object changed.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"keywords")) {
        free_string(pMob->pQuestor->keywords);
        pMob->pQuestor->keywords = str_dup(argument);

        send_to_char("Keywords set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"short")) {
        free_string(pMob->pQuestor->short_descr);
        pMob->pQuestor->short_descr = str_dup(argument);

        send_to_char("Short description set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"long")) {
        free_string(pMob->pQuestor->long_descr);
        pMob->pQuestor->long_descr = str_dup(argument);

        send_to_char("Long description set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"header")) {
        send_to_char("Editting the Questor Header:\n\r", ch);
        send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
        send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
        send_to_char("\n\r", ch);

        string_append(ch, &pMob->pQuestor->header);
        return true;

    } else if (!str_prefix(arg,"footer")) {
        send_to_char("Editting the Questor Footer:\n\r", ch);
        send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
        send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
        send_to_char("\n\r", ch);

        string_append(ch, &pMob->pQuestor->footer);
        return true;

    } else if (!str_prefix(arg,"prefix")) {
        free_string(pMob->pQuestor->prefix);
        pMob->pQuestor->prefix = str_dup(argument);

        send_to_char("Prefix set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"suffix")) {
        free_string(pMob->pQuestor->suffix);
        pMob->pQuestor->suffix = str_dup(argument);

        send_to_char("Prefix set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"width")) {
        if(!is_number(argument))
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int width = atoi(argument);
        if( width <= 0 )
        {
            pMob->pQuestor->line_width = 0;
            send_to_char("Line width disabled.\n\r", ch);
            return true;

        }
        else if(width > 160)
        {
            send_to_char("Width is out of range.  Please specify a number from 1 to 160, or 0 to disable width.\n\r", ch);
            return false;
        }

        pMob->pQuestor->line_width = width;
        send_to_char("Line width set.\n\r", ch);
        return true;

    } else {
        medit_questor(ch, "");
        return false;
    }

    return true;

}

/**
 * medit_trainer - Sub-editor for per-mob trainer data
 *
 * Allows adding/removing trainer data and managing the list of
 * skills/spells/songs this mob can train players on.
 *
 * Syntax:
 *   trainer add             - Add trainer data to mob
 *   trainer remove          - Remove trainer data from mob
 *   trainer greeting <text> - Set custom greeting
 *   trainer skill add <name> [maxrating] [gold] [trains] [script]
 *   trainer skill remove <name>
 *   trainer skill list      - List all entries
 */
MEDIT(medit_trainer)
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];

    EDIT_MOB(ch, pMob);

    if (IS_NULLSTR(argument)) {
        send_to_char("TRAINER ADD                          Adds trainer data to mob.\n\r", ch);
        send_to_char("        REMOVE                       Removes trainer data from mob.\n\r", ch);
        send_to_char("        GREETING [text]               Sets custom greeting.\n\r", ch);
        send_to_char("        SKILL ADD <name> [max] [gold] [trains] [script]\n\r", ch);
        send_to_char("        SKILL REMOVE <name>           Removes a trainable skill.\n\r", ch);
        send_to_char("        SKILL REP <name> <rep|none> [min|none] [max|none]\n\r", ch);
        send_to_char("        SKILL LIST                    Lists trainable skills.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_prefix(arg, "add")) {
        if (pMob->pTrainer != NULL) {
            send_to_char("This mob already has trainer data.\n\r", ch);
            return false;
        }

        pMob->pTrainer = new_trainer_data();
        send_to_char("Trainer data added.\n\r", ch);
        return true;

    } else if (!str_prefix(arg, "remove")) {
        if (pMob->pTrainer == NULL) {
            send_to_char("This mob has no trainer data.\n\r", ch);
            return false;
        }

        free_trainer_data(pMob->pTrainer);
        pMob->pTrainer = NULL;
        send_to_char("Trainer data removed.\n\r", ch);
        return true;

    } else if (!str_prefix(arg, "greeting")) {
        if (pMob->pTrainer == NULL) {
            send_to_char("This mob has no trainer data. Use 'trainer add' first.\n\r", ch);
            return false;
        }

        if (IS_NULLSTR(argument)) {
            if (pMob->pTrainer->greeting)
                free_string(pMob->pTrainer->greeting);
            pMob->pTrainer->greeting = NULL;
            send_to_char("Greeting cleared.\n\r", ch);
            return true;
        }

        if (pMob->pTrainer->greeting)
            free_string(pMob->pTrainer->greeting);
        pMob->pTrainer->greeting = str_dup(argument);
        send_to_char("Greeting set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg, "skill")) {
        char sub[MIL];

        if (pMob->pTrainer == NULL) {
            send_to_char("This mob has no trainer data. Use 'trainer add' first.\n\r", ch);
            return false;
        }

        argument = one_argument(argument, sub);

        if (!str_prefix(sub, "add")) {
            char name[MIL];
            TRAINER_ENTRY *entry;
            int max_rating = 0, cost_gold = 0, cost_trains = 0;
            char script_arg[MIL];

            argument = one_argument(argument, name);
            if (name[0] == '\0') {
                send_to_char("Syntax: trainer skill add <name> [max_rating] [gold] [trains] [script]\n\r", ch);
                return false;
            }

            /* Parse optional numeric args */
            char num[MIL];
            argument = one_argument(argument, num);
            if (num[0] && is_number(num)) {
                max_rating = atoi(num);
                argument = one_argument(argument, num);
                if (num[0] && is_number(num)) {
                    cost_gold = atoi(num);
                    argument = one_argument(argument, num);
                    if (num[0] && is_number(num)) {
                        cost_trains = atoi(num);
                        one_argument(argument, script_arg);
                    } else {
                        script_arg[0] = '\0';
                    }
                } else {
                    script_arg[0] = '\0';
                }
            } else {
                script_arg[0] = '\0';
            }

            /* Check for duplicate */
            for (entry = pMob->pTrainer->entries; entry; entry = entry->next) {
                if (IS_VALID(entry) && !str_cmp(entry->skill_name, name)) {
                    entry->max_rating = max_rating;
                    entry->cost_gold = cost_gold;
                    entry->cost_trains = cost_trains;
                    if (entry->check_script)
                        free_string(entry->check_script);
                    entry->check_script = script_arg[0] ? str_dup(script_arg) : NULL;
                    send_to_char(formatf("Updated trainer entry '%s'.\n\r", name), ch);
                    return true;
                }
            }

            /* Add new entry */
            entry = new_trainer_entry();
            free_string(entry->skill_name);
            entry->skill_name = str_dup(name);
            entry->max_rating = max_rating;
            entry->cost_gold = cost_gold;
            entry->cost_trains = cost_trains;
            entry->check_script = script_arg[0] ? str_dup(script_arg) : NULL;

            /* Prepend to list */
            entry->next = pMob->pTrainer->entries;
            pMob->pTrainer->entries = entry;

            send_to_char(formatf("Added trainer entry '%s'.\n\r", name), ch);
            return true;

        } else if (!str_prefix(sub, "remove")) {
            char name[MIL];
            TRAINER_ENTRY *entry, *prev = NULL;

            one_argument(argument, name);
            if (name[0] == '\0') {
                send_to_char("Syntax: trainer skill remove <name>\n\r", ch);
                return false;
            }

            for (entry = pMob->pTrainer->entries; entry; prev = entry, entry = entry->next) {
                if (IS_VALID(entry) && !str_cmp(entry->skill_name, name)) {
                    if (prev)
                        prev->next = entry->next;
                    else
                        pMob->pTrainer->entries = entry->next;
                    free_trainer_entry(entry);
                    send_to_char(formatf("Removed trainer entry '%s'.\n\r", name), ch);
                    return true;
                }
            }

            send_to_char("No trainer entry with that name.\n\r", ch);
            return false;

        } else if (!str_prefix(sub, "rep")) {
            char name[MIL], rep_arg[MIL], min_arg[MIL], max_arg[MIL];
            TRAINER_ENTRY *entry;

            argument = one_argument(argument, name);
            argument = one_argument(argument, rep_arg);
            argument = one_argument(argument, min_arg);
            argument = one_argument(argument, max_arg);

            if (IS_NULLSTR(name) || IS_NULLSTR(rep_arg)) {
                send_to_char("Syntax: trainer skill rep <name> <reputation widevnum|none> [min rank|none] [max rank|none]\n\r", ch);
                return false;
            }

            for (entry = pMob->pTrainer->entries; entry; entry = entry->next) {
                if (IS_VALID(entry) && !str_cmp(entry->skill_name, name))
                    break;
            }

            if (!IS_VALID(entry)) {
                send_to_char("No trainer entry with that name.\n\r", ch);
                return false;
            }

            if (!str_prefix(rep_arg, "none")) {
                entry->reputation = NULL;
                entry->reputation_load.auid = 0;
                entry->reputation_load.vnum = 0;
                entry->min_reputation_rank = 0;
                entry->max_reputation_rank = 0;
                send_to_char("Trainer entry reputation requirement cleared.\n\r", ch);
                return true;
            }

            WNUM wnum;
            REPUTATION_INDEX_DATA *repIndex;
            int min_rank = 0;
            int max_rank = 0;

            if (!parse_widevnum(rep_arg, pMob->area, &wnum) || !wnum.pArea) {
                send_to_char("Please specify a valid reputation widevnum (or 'none').\n\r", ch);
                return false;
            }

            repIndex = get_reputation_index(wnum.pArea, wnum.vnum);
            if (!IS_VALID(repIndex)) {
                send_to_char("No reputation with that widevnum.\n\r", ch);
                return false;
            }

            if (!IS_NULLSTR(min_arg) && str_cmp(min_arg, "none")) {
                if (!is_number(min_arg)) {
                    send_to_char("Minimum rank must be a number or 'none'.\n\r", ch);
                    return false;
                }
                min_rank = atoi(min_arg);
            }

            if (!IS_NULLSTR(max_arg) && str_cmp(max_arg, "none")) {
                if (!is_number(max_arg)) {
                    send_to_char("Maximum rank must be a number or 'none'.\n\r", ch);
                    return false;
                }
                max_rank = atoi(max_arg);
            }

            if (min_rank < 0 || max_rank < 0) {
                send_to_char("Rank bounds cannot be negative.\n\r", ch);
                return false;
            }

            if (min_rank > 0 && min_rank > list_size(repIndex->ranks)) {
                send_to_char(formatf("Minimum rank is out of range (1..%d).\n\r", list_size(repIndex->ranks)), ch);
                return false;
            }

            if (max_rank > 0 && max_rank > list_size(repIndex->ranks)) {
                send_to_char(formatf("Maximum rank is out of range (1..%d).\n\r", list_size(repIndex->ranks)), ch);
                return false;
            }

            if (min_rank > 0 && max_rank > 0 && min_rank > max_rank) {
                send_to_char("Minimum rank cannot be greater than maximum rank.\n\r", ch);
                return false;
            }

            entry->reputation = repIndex;
            entry->reputation_load.auid = repIndex->area ? repIndex->area->uid : 0;
            entry->reputation_load.vnum = repIndex->vnum;
            entry->min_reputation_rank = min_rank;
            entry->max_reputation_rank = max_rank;

            send_to_char("Trainer entry reputation requirement set.\n\r", ch);
            return true;

        } else if (!str_prefix(sub, "list")) {
            TRAINER_ENTRY *entry;
            int count = 0;

            if (!pMob->pTrainer->entries) {
                send_to_char("No trainer entries.\n\r", ch);
                return false;
            }

            send_to_char(formatf("{G%-25s %-6s %-6s %-8s %-15s %-26s{x\n\r",
                "Skill/Spell/Song", "MaxRat", "Gold", "Trains", "Script", "Reputation"), ch);

            for (entry = pMob->pTrainer->entries; entry; entry = entry->next) {
                char rep_buf[MIL];

                if (!IS_VALID(entry)) continue;

                if (IS_VALID(entry->reputation))
                    snprintf(rep_buf, sizeof(rep_buf), "%s [%d,%d]",
                        widevnum_string(entry->reputation->area, entry->reputation->vnum, pMob->area),
                        entry->min_reputation_rank,
                        entry->max_reputation_rank);
                else
                    strcpy(rep_buf, "(none)");

                send_to_char(formatf("%-25s %-6d %-6d %-8d %-15s %-26s\n\r",
                    entry->skill_name ? entry->skill_name : "?",
                    entry->max_rating,
                    entry->cost_gold,
                    entry->cost_trains,
                    entry->check_script ? entry->check_script : "(none)",
                    rep_buf), ch);
                count++;
            }

            send_to_char(formatf("\n\r%d entries.\n\r", count), ch);
            return false;

        } else {
            send_to_char("Syntax: trainer skill add|remove|list\n\r", ch);
            return false;
        }

    } else {
        medit_trainer(ch, "");
        return false;
    }

    return true;
}

MEDIT( medit_crew )
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];

    EDIT_MOB(ch, pMob);

    if(IS_NULLSTR(argument))
    {
        send_to_char("Syntax:  crew assign\n\r", ch);
        send_to_char("         crew remove\n\r", ch);
        send_to_char("         crew minrank <rank>\n\r", ch);
        send_to_char("         crew scouting <rating>\n\r", ch);
        send_to_char("         crew gunning <rating>\n\r", ch);
        send_to_char("         crew oarring <rating>\n\r", ch);
        send_to_char("         crew mechanics <rating>\n\r", ch);
        send_to_char("         crew navigation <rating>\n\r", ch);
        send_to_char("         crew leadership <rating>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "assign") )
    {
        if( IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile already has ship crew data.\n\r", ch);
            return false;
        }

        pMob->pCrew = new_ship_crew_index();
        send_to_char("Ship Crew assigned.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "remove") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile has no ship crew data.\n\r", ch);
            return false;
        }

        free_ship_crew_index(pMob->pCrew);
        pMob->pCrew = NULL;
        send_to_char("Ship Crew removed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "minrank") )
    {
        send_to_char("Not implemented yet.\n\r", ch);
        return false;
    }

    if( !str_prefix(arg, "scouting") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->scouting = value;
        send_to_char("Scouting Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "gunning") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->gunning = value;
        send_to_char("Gunning Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "oarring") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->oarring = value;
        send_to_char("Oarring Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "mechanics") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->mechanics = value;
        send_to_char("Mechanics Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "navigation") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->navigation = value;
        send_to_char("Navigation Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "leadership") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->leadership = value;
        send_to_char("Leadership Rating changed.\n\r", ch);
        return true;
    }


    medit_crew(ch, "");
    return false;
}
