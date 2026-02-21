#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../mxp_links.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../scripts.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"

void do_qedit(CHAR_DATA *ch, char *argument);

static bool qedit_show(CHAR_DATA *ch, char *argument);
static void qedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void qedit_show_flow_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void qedit_show_rewards_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void qedit_show_scripting_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void qedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static AREA_DATA *qedit_get_area(void *pEdit);
static bool qedit_exec_session_command(CHAR_DATA *ch, const char *command, char *argument);

static bool qedit_cmd_show(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_name(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_summary(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_description(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_class(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_mode(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_type(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_category(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_scope(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_flags(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_repeat(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_allowance(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_cost(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_entry(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_enabled(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_seedpolicy(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_seed(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_varset(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_varclear(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_addqprog(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_delqprog(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_stage(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_objective(CHAR_DATA *ch, char *argument);
static bool qedit_cmd_reward(CHAR_DATA *ch, char *argument);
static bool qedit_internal_dispatch = false;

static const struct olc_cmd_type qedit_table[] =
{
    { "?",           show_help          },
    { "commands",    show_commands      },
    { "show",        qedit_cmd_show     },
    { "name",        qedit_cmd_name     },
    { "summary",     qedit_cmd_summary  },
    { "description", qedit_cmd_description },
    { "class",       qedit_cmd_class    },
    { "mode",        qedit_cmd_mode     },
    { "type",        qedit_cmd_type     },
    { "category",    qedit_cmd_category },
    { "scope",       qedit_cmd_scope    },
    { "flags",       qedit_cmd_flags    },
    { "repeat",      qedit_cmd_repeat   },
    { "allowance",   qedit_cmd_allowance },
    { "cost",        qedit_cmd_cost     },
    { "entry",       qedit_cmd_entry    },
    { "enabled",     qedit_cmd_enabled  },
    { "seedpolicy",  qedit_cmd_seedpolicy },
    { "seed",        qedit_cmd_seed     },
    { "varset",      qedit_cmd_varset   },
    { "varclear",    qedit_cmd_varclear },
    { "addqprog",    qedit_cmd_addqprog },
    { "delqprog",    qedit_cmd_delqprog },
    { "stage",       qedit_cmd_stage    },
    { "objective",   qedit_cmd_objective },
    { "reward",      qedit_cmd_reward   },
    { NULL,           0                  }
};

static const OLC_EDITOR_DEF qedit_def = {
    .name        = "QEdit",
    .editor_type = ED_QUEST,
    .cmd_table   = qedit_table,
    .show_fn     = qedit_show,
    .tabs        = {
        .count = 5,
        .tabs = {
            { "General",    "Gen", qedit_show_general_tab },
            { "Flow",       "Flw", qedit_show_flow_tab },
            { "Rewards",    "Rwd", qedit_show_rewards_tab },
            { "Scripts",    "Scr", qedit_show_scripting_tab },
            { "Notes",      "Nts", qedit_show_notes_tab },
        }
    },
    .theme       = &olc_theme_entity,
    .perm        = {
        .flags = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_IMPLEMENTOR,
    },
    .change_mode = OLC_CHANGE_AREA_FLAG,
    .get_area_fn = qedit_get_area,
    .audit_changes = true,
};

static const char *qedit_class_name(int quest_class)
{
    switch (quest_class) {
    case QUEST_CLASS_NARRATIVE: return "narrative";
    case QUEST_CLASS_MISSION:   return "mission";
    default:                   return "unknown";
    }
}

static const char *qedit_type_name(int quest_type)
{
    switch (quest_type) {
    case QUEST_TYPE_MAIN_STORY:  return "main";
    case QUEST_TYPE_SIDE_QUEST:  return "side";
    case QUEST_TYPE_UNLOCK:      return "unlock";
    case QUEST_TYPE_CLASS_QUEST: return "class";
    case QUEST_TYPE_EVENT:       return "event";
    case QUEST_TYPE_OTHER:       return "other";
    default:                     return "unknown";
    }
}

static const char *qedit_category_name(int category)
{
    switch (category) {
    case QUEST_LOG_CATEGORY_NONE:     return "none";
    case QUEST_LOG_CATEGORY_REGIONAL: return "regional";
    case QUEST_LOG_CATEGORY_CLASS:    return "class";
    case QUEST_LOG_CATEGORY_STORY:    return "story";
    case QUEST_LOG_CATEGORY_CHURCH:   return "church";
    case QUEST_LOG_CATEGORY_DUNGEON:  return "dungeon";
    case QUEST_LOG_CATEGORY_CRAFTING: return "crafting";
    case QUEST_LOG_CATEGORY_EVENT:    return "event";
    case QUEST_LOG_CATEGORY_OTHER:    return "other";
    default:                          return "unknown";
    }
}

static const char *qedit_scope_name(int scope)
{
    switch (scope) {
    case QUEST_TARGET_SCOPE_CHARACTER: return "character";
    case QUEST_TARGET_SCOPE_GROUP:     return "group";
    case QUEST_TARGET_SCOPE_CHURCH:    return "church";
    default:                           return "unknown";
    }
}

static const char *qedit_repeat_name(int policy)
{
    switch (policy) {
    case QUEST_REPEAT_ONCE:       return "once";
    case QUEST_REPEAT_REPEATABLE: return "repeatable";
    default:                      return "unknown";
    }
}

static const char *qedit_seed_policy_name(int policy)
{
    switch (policy) {
    case QUEST_SEED_POLICY_AUTO:  return "auto";
    case QUEST_SEED_POLICY_FIXED: return "fixed";
    default:                      return "unknown";
    }
}

static bool qedit_parse_index_ref(CHAR_DATA *ch, const char *input, WNUM *wnum)
{
    char ref[MIL];
    char *sep;
    AREA_DATA *area = NULL;

    if (!ch || !wnum || IS_NULLSTR(input))
        return false;

    strncpy(ref, input, sizeof(ref) - 1);
    ref[sizeof(ref) - 1] = '\0';

    sep = strchr(ref, '#');
    if (!sep)
        sep = strchr(ref, ':');

    if (sep) {
        *sep++ = '\0';
        if (!is_number(ref) || !is_number(sep))
            return false;
        area = get_area_index(atol(ref));
        if (!area)
            return false;
        wnum->pArea = area;
        wnum->vnum = atol(sep);
        return wnum->vnum > 0;
    }

    if (!is_number(ref))
        return false;

    if (!ch->in_room || !ch->in_room->area)
        return false;

    wnum->pArea = ch->in_room->area;
    wnum->vnum = atol(ref);
    return wnum->vnum > 0;
}

static const char *qedit_stage_source_name(int source)
{
    switch (source) {
    case QUEST_STAGE_SOURCE_STATIC:    return "static";
    case QUEST_STAGE_SOURCE_GENERATED: return "generated";
    default:                           return "unknown";
    }
}

static const char *qedit_stage_completion_name(int mode)
{
    switch (mode) {
    case QUEST_STAGE_COMPLETE_ALL:    return "all";
    case QUEST_STAGE_COMPLETE_ANY:    return "any";
    case QUEST_STAGE_COMPLETE_CUSTOM: return "custom";
    default:                          return "unknown";
    }
}

static const char *qedit_objective_type_name(int type)
{
    switch (type) {
    case QUEST_OBJECTIVE_KILL:          return "kill";
    case QUEST_OBJECTIVE_COLLECT:       return "collect";
    case QUEST_OBJECTIVE_TALK:          return "talk";
    case QUEST_OBJECTIVE_TRAVEL:        return "travel";
    case QUEST_OBJECTIVE_LOCATE:        return "locate";
    case QUEST_OBJECTIVE_RESCUE:        return "rescue";
    case QUEST_OBJECTIVE_ESCORT:        return "escort";
    case QUEST_OBJECTIVE_CUSTOM_SCRIPT: return "custom";
    default:                            return "unknown";
    }
}

static const char *qedit_target_mode_name(int mode)
{
    switch (mode) {
    case QUEST_OBJECTIVE_TARGET_EXACT: return "exact";
    case QUEST_OBJECTIVE_TARGET_POOL:  return "pool";
    default:                           return "unknown";
    }
}

static const char *qedit_reward_type_name(int type)
{
    switch (type) {
    case QUEST_REWARD_POINTS:     return "points";
    case QUEST_REWARD_CURRENCY:   return "currency";
    case QUEST_REWARD_REPUTATION: return "reputation";
    case QUEST_REWARD_TOKEN:      return "token";
    case QUEST_REWARD_ITEM:       return "item";
    case QUEST_REWARD_SCRIPT:     return "script";
    default:                      return "unknown";
    }
}

static bool qedit_parse_reward_type(const char *text, int *reward_type)
{
    if (!reward_type || IS_NULLSTR(text))
        return false;

    if (!str_prefix(text, "points"))
        *reward_type = QUEST_REWARD_POINTS;
    else if (!str_prefix(text, "currency"))
        *reward_type = QUEST_REWARD_CURRENCY;
    else if (!str_prefix(text, "reputation"))
        *reward_type = QUEST_REWARD_REPUTATION;
    else if (!str_prefix(text, "token"))
        *reward_type = QUEST_REWARD_TOKEN;
    else if (!str_prefix(text, "item"))
        *reward_type = QUEST_REWARD_ITEM;
    else if (!str_prefix(text, "script"))
        *reward_type = QUEST_REWARD_SCRIPT;
    else
        return false;

    return true;
}

static QUEST_REWARD_INDEX_V2_DATA *qedit_get_reward_by_index(
    QUEST_INDEX_V2_DATA *quest_index_v2,
    int reward_index,
    QUEST_REWARD_INDEX_V2_DATA **out_prev)
{
    QUEST_REWARD_INDEX_V2_DATA *prev = NULL;
    QUEST_REWARD_INDEX_V2_DATA *iter;
    int current = 1;

    if (out_prev)
        *out_prev = NULL;

    if (!quest_index_v2 || reward_index < 1)
        return NULL;

    for (iter = quest_index_v2->rewards; iter != NULL; prev = iter, iter = iter->next, current++) {
        if (current == reward_index) {
            if (out_prev)
                *out_prev = prev;
            return iter;
        }
    }

    return NULL;
}

static QUEST_STAGE_INDEX_V2_DATA *qedit_find_stage(QUEST_INDEX_V2_DATA *quest_index_v2, int stage_id)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;

    if (!quest_index_v2 || stage_id < 1)
        return NULL;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
        if (stage->id == stage_id)
            return stage;
    }

    return NULL;
}

static QUEST_OBJECTIVE_INDEX_V2_DATA *qedit_find_objective(QUEST_STAGE_INDEX_V2_DATA *stage, int objective_id)
{
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;

    if (!stage || objective_id < 1)
        return NULL;

    for (objective = stage->objectives; objective != NULL; objective = objective->next) {
        if (objective->id == objective_id)
            return objective;
    }

    return NULL;
}

static QUEST_OBJECTIVE_INDEX_V2_DATA *qedit_find_objective_by_refname(QUEST_INDEX_V2_DATA *quest_index_v2, const char *refname)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;

    if (!quest_index_v2 || IS_NULLSTR(refname))
        return NULL;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
    {
        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            if ((!IS_NULLSTR(objective->target_variable_name) && !str_cmp(objective->target_variable_name, refname))
                || (!IS_NULLSTR(objective->destination_variable_name) && !str_cmp(objective->destination_variable_name, refname))
                || (!IS_NULLSTR(objective->target_token_variable_name) && !str_cmp(objective->target_token_variable_name, refname))
                || (!IS_NULLSTR(objective->destination_token_variable_name) && !str_cmp(objective->destination_token_variable_name, refname)))
                return objective;
        }
    }

    return NULL;
}

static void qedit_append_stage(QUEST_INDEX_V2_DATA *quest_index_v2, QUEST_STAGE_INDEX_V2_DATA *stage)
{
    QUEST_STAGE_INDEX_V2_DATA *tail;

    if (!quest_index_v2 || !stage)
        return;

    stage->next = NULL;
    if (!quest_index_v2->stages) {
        quest_index_v2->stages = stage;
        return;
    }

    for (tail = quest_index_v2->stages; tail->next != NULL; tail = tail->next)
        ;

    tail->next = stage;
}

static bool qedit_remove_stage(QUEST_INDEX_V2_DATA *quest_index_v2, int stage_id)
{
    QUEST_STAGE_INDEX_V2_DATA *prev = NULL;
    QUEST_STAGE_INDEX_V2_DATA *iter;

    if (!quest_index_v2 || stage_id < 1)
        return false;

    for (iter = quest_index_v2->stages; iter != NULL; prev = iter, iter = iter->next) {
        if (iter->id != stage_id)
            continue;

        if (prev)
            prev->next = iter->next;
        else
            quest_index_v2->stages = iter->next;

        iter->next = NULL;
        free_quest_stage_index_v2(iter);
        return true;
    }

    return false;
}

static void qedit_append_objective(QUEST_STAGE_INDEX_V2_DATA *stage, QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    QUEST_OBJECTIVE_INDEX_V2_DATA *tail;

    if (!stage || !objective)
        return;

    objective->next = NULL;
    if (!stage->objectives) {
        stage->objectives = objective;
        return;
    }

    for (tail = stage->objectives; tail->next != NULL; tail = tail->next)
        ;

    tail->next = objective;
}

static bool qedit_remove_objective(QUEST_STAGE_INDEX_V2_DATA *stage, int objective_id)
{
    QUEST_OBJECTIVE_INDEX_V2_DATA *prev = NULL;
    QUEST_OBJECTIVE_INDEX_V2_DATA *iter;

    if (!stage || objective_id < 1)
        return false;

    for (iter = stage->objectives; iter != NULL; prev = iter, iter = iter->next) {
        if (iter->id != objective_id)
            continue;

        if (prev)
            prev->next = iter->next;
        else
            stage->objectives = iter->next;

        iter->next = NULL;
        free_quest_objective_index_v2(iter);
        return true;
    }

    return false;
}

static int qedit_count_pool_entries(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *entry;
    int count = 0;

    if (!objective)
        return 0;

    for (entry = objective->pool_entries; entry != NULL; entry = entry->next)
        count++;

    return count;
}

static bool qedit_objective_has_target_data(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    if (!objective)
        return false;

    if (objective->target_wnum.pArea && objective->target_wnum.vnum > 0)
        return true;

    if (objective->target_ref_stage_id > 0 && objective->target_ref_objective_id > 0)
        return true;

    if (!IS_NULLSTR(objective->target_ref_name))
        return true;

    if (!IS_NULLSTR(objective->target_variable_name))
        return true;

    if (qedit_count_pool_entries(objective) > 0)
        return true;

    return false;
}

static bool qedit_objective_has_destination_data(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    if (!objective)
        return false;

    if (objective->destination_wnum.pArea && objective->destination_wnum.vnum > 0)
        return true;

    if (!IS_NULLSTR(objective->destination_ref_name))
        return true;

    if (!IS_NULLSTR(objective->destination_variable_name))
        return true;

    return false;
}

static bool qedit_objective_has_token_data(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    if (!objective)
        return false;

    if (objective->target_token_wnum.pArea && objective->target_token_wnum.vnum > 0)
        return true;

    if (!IS_NULLSTR(objective->target_token_ref_name))
        return true;

    if (!IS_NULLSTR(objective->target_token_variable_name))
        return true;

    if (objective->destination_token_wnum.pArea && objective->destination_token_wnum.vnum > 0)
        return true;

    if (!IS_NULLSTR(objective->destination_token_ref_name))
        return true;

    if (!IS_NULLSTR(objective->destination_token_variable_name))
        return true;

    return false;
}

static bool qedit_objective_target_relevant(int objective_type)
{
    switch (objective_type) {
    case QUEST_OBJECTIVE_KILL:
    case QUEST_OBJECTIVE_COLLECT:
    case QUEST_OBJECTIVE_TALK:
    case QUEST_OBJECTIVE_LOCATE:
    case QUEST_OBJECTIVE_RESCUE:
    case QUEST_OBJECTIVE_ESCORT:
        return true;

    case QUEST_OBJECTIVE_TRAVEL:
    case QUEST_OBJECTIVE_CUSTOM_SCRIPT:
    default:
        return false;
    }
}

static bool qedit_objective_destination_relevant(int objective_type)
{
    switch (objective_type) {
    case QUEST_OBJECTIVE_TRAVEL:
    case QUEST_OBJECTIVE_RESCUE:
    case QUEST_OBJECTIVE_ESCORT:
        return true;

    default:
        return false;
    }
}

static bool qedit_objective_token_relevant(int objective_type)
{
    switch (objective_type) {
    case QUEST_OBJECTIVE_COLLECT:
        return true;

    default:
        return false;
    }
}

static void qedit_append_pool_entry(QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *entry)
{
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *tail;

    if (!objective || !entry)
        return;

    entry->next = NULL;
    if (!objective->pool_entries) {
        objective->pool_entries = entry;
        return;
    }

    for (tail = objective->pool_entries; tail->next != NULL; tail = tail->next)
        ;

    tail->next = entry;
}

static bool qedit_remove_pool_entry(QUEST_OBJECTIVE_INDEX_V2_DATA *objective, int entry_id)
{
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *prev = NULL;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *iter;

    if (!objective || entry_id < 1)
        return false;

    for (iter = objective->pool_entries; iter != NULL; prev = iter, iter = iter->next) {
        if (iter->id != entry_id)
            continue;

        if (prev)
            prev->next = iter->next;
        else
            objective->pool_entries = iter->next;

        iter->next = NULL;
        free_quest_objective_pool_entry_v2(iter);
        return true;
    }

    return false;
}

static bool qedit_parse_target_wnum(CHAR_DATA *ch, const char *input, WNUM *target)
{
    if (!target)
        return false;

    if (IS_NULLSTR(input) || !str_cmp(input, "none")) {
        *target = wnum_zero;
        return true;
    }

    return qedit_parse_index_ref(ch, input, target);
}

typedef enum {
    QEDIT_WNUM_ANY = 0,
    QEDIT_WNUM_MOB,
    QEDIT_WNUM_OBJ,
    QEDIT_WNUM_ROOM,
    QEDIT_WNUM_TOKEN,
    QEDIT_WNUM_BLUEPRINT,
    QEDIT_WNUM_DUNGEON
} qedit_wnum_kind_t;

static bool qedit_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc)
        return false;
    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

static void qedit_mxp_send(CHAR_DATA *ch, const char *command,
    const char *text, char *dest, size_t dest_size)
{
    const char *result;

    if (!dest || dest_size == 0)
        return;

    if (IS_NULLSTR(text)) {
        dest[0] = '\0';
        return;
    }

    if (!qedit_use_mxp(ch) || IS_NULLSTR(command)) {
        snprintf(dest, dest_size, "%s", text);
        return;
    }

    result = MXPCreateSend(ch->desc, command, text);
    snprintf(dest, dest_size, "%s", result ? result : text);
}

static void qedit_build_entity_links(CHAR_DATA *ch, const char *wnum,
    const char *edit_base, const char *show_base,
    char *wnum_link, size_t wnum_link_size,
    char *show_link, size_t show_link_size,
    char *edit_link, size_t edit_link_size)
{
    char show_cmd[MIL];
    (void)edit_base;

    if (wnum_link && wnum_link_size > 0)
        wnum_link[0] = '\0';
    if (show_link && show_link_size > 0)
        show_link[0] = '\0';
    if (edit_link && edit_link_size > 0)
        edit_link[0] = '\0';

    if (IS_NULLSTR(wnum) || IS_NULLSTR(show_base))
        return;

    snprintf(show_cmd, sizeof(show_cmd), "%s %s", show_base, wnum);

    if (!qedit_use_mxp(ch)) {
        if (wnum_link && wnum_link_size > 0)
            snprintf(wnum_link, wnum_link_size, "%s", wnum);
        return;
    }

    if (wnum_link && wnum_link_size > 0)
        qedit_mxp_send(ch, show_cmd, wnum, wnum_link, wnum_link_size);
}

static bool qedit_resolve_wnum_info(WNUM wnum, qedit_wnum_kind_t preferred,
    const char **out_type, const char **out_name,
    const char **out_show_cmd, const char **out_edit_cmd,
    char *out_wnum, size_t out_wnum_size)
{
    MOB_INDEX_DATA *mob = NULL;
    OBJ_INDEX_DATA *obj = NULL;
    ROOM_INDEX_DATA *room = NULL;
    TOKEN_INDEX_DATA *token = NULL;
    BLUEPRINT *bp = NULL;
    DUNGEON_INDEX_DATA *dng = NULL;

    if (out_type)
        *out_type = "unknown";
    if (out_name)
        *out_name = "(unresolved)";
    if (out_show_cmd)
        *out_show_cmd = NULL;
    if (out_edit_cmd)
        *out_edit_cmd = NULL;
    if (out_wnum && out_wnum_size > 0)
        out_wnum[0] = '\0';

    if (!wnum.pArea || wnum.vnum < 1) {
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "none");
        return false;
    }

    if (preferred == QEDIT_WNUM_MOB || preferred == QEDIT_WNUM_ANY)
        mob = get_mob_index(wnum.pArea, wnum.vnum);
    if (!mob && (preferred == QEDIT_WNUM_OBJ || preferred == QEDIT_WNUM_ANY))
        obj = get_obj_index(wnum.pArea, wnum.vnum);
    if (!mob && !obj && (preferred == QEDIT_WNUM_ROOM || preferred == QEDIT_WNUM_ANY))
        room = get_room_index(wnum.pArea, wnum.vnum);
    if (!mob && !obj && !room && (preferred == QEDIT_WNUM_TOKEN || preferred == QEDIT_WNUM_ANY))
        token = get_token_index(wnum.pArea, wnum.vnum);
    if (!mob && !obj && !room && !token && (preferred == QEDIT_WNUM_BLUEPRINT || preferred == QEDIT_WNUM_ANY))
        bp = get_blueprint_for_area(wnum.pArea, wnum.vnum);
    if (!mob && !obj && !room && !token && !bp && (preferred == QEDIT_WNUM_DUNGEON || preferred == QEDIT_WNUM_ANY))
        dng = get_dungeon_index_for_area(wnum.pArea, wnum.vnum);

    if (mob) {
        if (out_type)
            *out_type = "mob";
        if (out_name)
            *out_name = mob->short_descr ? mob->short_descr : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "mshow";
        if (out_edit_cmd)
            *out_edit_cmd = "medit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_mobile(mob, NULL));
        return true;
    }

    if (obj) {
        if (out_type)
            *out_type = "obj";
        if (out_name)
            *out_name = obj->short_descr ? obj->short_descr : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "oshow";
        if (out_edit_cmd)
            *out_edit_cmd = "oedit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_object(obj, NULL));
        return true;
    }

    if (room) {
        if (out_type)
            *out_type = "room";
        if (out_name)
            *out_name = room->name ? room->name : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "rshow";
        if (out_edit_cmd)
            *out_edit_cmd = "redit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_room(room, NULL));
        return true;
    }

    if (token) {
        if (out_type)
            *out_type = "token";
        if (out_name)
            *out_name = token->name ? token->name : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "tshow";
        if (out_edit_cmd)
            *out_edit_cmd = "tedit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_token(token, NULL));
        return true;
    }

    if (bp) {
        if (out_type)
            *out_type = "blueprint";
        if (out_name)
            *out_name = bp->name ? bp->name : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "bpshow";
        if (out_edit_cmd)
            *out_edit_cmd = "bpedit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_blueprint(bp, NULL));
        return true;
    }

    if (dng) {
        if (out_type)
            *out_type = "dungeon";
        if (out_name)
            *out_name = dng->name ? dng->name : "(unnamed)";
        if (out_show_cmd)
            *out_show_cmd = "dngshow";
        if (out_edit_cmd)
            *out_edit_cmd = "dngedit";
        if (out_wnum && out_wnum_size > 0)
            snprintf(out_wnum, out_wnum_size, "%s", widevnum_string_dungeon(dng, NULL));
        return true;
    }

    if (out_wnum && out_wnum_size > 0)
        snprintf(out_wnum, out_wnum_size, "%ld#%ld", wnum.pArea->uid, wnum.vnum);
    return false;
}

static void qedit_format_wnum_display(CHAR_DATA *ch, WNUM wnum, qedit_wnum_kind_t preferred,
    char *out, size_t out_size)
{
    const char *type_name;
    const char *entity_name;
    const char *show_cmd;
    const char *edit_cmd;
    char wnum_buf[128];
    char wnum_link[256];
    bool resolved;

    if (!out || out_size == 0)
        return;

    out[0] = '\0';

    resolved = qedit_resolve_wnum_info(wnum, preferred,
        &type_name, &entity_name,
        &show_cmd, &edit_cmd,
        wnum_buf, sizeof(wnum_buf));

    if (!resolved) {
        snprintf(out, out_size, "%s (unresolved)", wnum_buf);
        return;
    }

    qedit_build_entity_links(ch, wnum_buf, edit_cmd, show_cmd,
        wnum_link, sizeof(wnum_link),
        NULL, 0,
        NULL, 0);

    snprintf(out, out_size, "%s (%s: %.80s)",
        wnum_link,
        type_name,
        entity_name ? entity_name : "(unnamed)");
}

static const char *qedit_tail_after(const char *input, int consumed_tokens, char *out, size_t out_size)
{
    char local[MSL];
    char token[MIL];
    char *rest;
    int i;

    if (!out || out_size == 0)
        return "";

    out[0] = '\0';

    if (IS_NULLSTR(input))
        return out;

    strncpy(local, input, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';

    rest = local;
    for (i = 0; i < consumed_tokens && !IS_NULLSTR(rest); i++)
        rest = one_argument(rest, token);

    while (*rest && isspace((unsigned char)*rest))
        rest++;

    strncpy(out, rest, out_size - 1);
    out[out_size - 1] = '\0';
    return out;
}

static bool qedit_is_readonly_session_command(const char *argument)
{
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    char local[MSL];
    char *rest;

    if (IS_NULLSTR(argument))
        return true;

    strncpy(local, argument, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';

    rest = local;
    rest = one_argument(rest, arg1);
    rest = one_argument(rest, arg2);
    rest = one_argument(rest, arg3);
    rest = one_argument(rest, arg4);

    if (IS_NULLSTR(arg1))
        return true;

    if (!str_prefix(arg1, "show") || !str_prefix(arg1, "tab")
        || !str_prefix(arg1, "history") || !str_prefix(arg1, "view")
        || !str_prefix(arg1, "commands") || !str_cmp(arg1, "?"))
        return true;

    if (!str_prefix(arg1, "stage"))
    {
        if (!str_prefix(arg2, "list") || !str_prefix(arg2, "show"))
            return true;
        return false;
    }

    if (!str_prefix(arg1, "objective"))
    {
        if (!str_prefix(arg2, "list") || !str_prefix(arg2, "show"))
            return true;

        if (!str_prefix(arg2, "pool") && !str_prefix(arg3, "list"))
            return true;

        return false;
    }

    if (!str_prefix(arg1, "reward"))
    {
        if (!str_prefix(arg2, "list") || !str_prefix(arg2, "show"))
            return true;
        return false;
    }

    return false;
}

static void qedit_mark_area_dirty(QUEST_INDEX_V2_DATA *quest_index_v2)
{
    if (!quest_index_v2 || !quest_index_v2->area)
        return;

    SET_BIT(quest_index_v2->area->area_flags, AREA_CHANGED);
}

static AREA_DATA *qedit_get_area(void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    return quest_index_v2 ? quest_index_v2->area : NULL;
}

static long qedit_next_vnum_in_area(AREA_DATA *area)
{
    QUEST_INDEX_V2_DATA *iter;
    long next_vnum = 1;

    if (!area)
        return 0;

    for (iter = quest_index_v2_list; iter != NULL; iter = iter->next)
    {
        if (iter->area != area)
            continue;

        if (iter->vnum >= next_vnum)
            next_vnum = iter->vnum + 1;
    }

    return next_vnum;
}

static void qedit_enter_editor(CHAR_DATA *ch, QUEST_INDEX_V2_DATA *quest_index_v2)
{
    if (!ch || !ch->desc || !quest_index_v2)
        return;

    olc_editor_enter(ch, &qedit_def, quest_index_v2, true);
}

void qedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &qedit_def);
}

static bool qedit_exec_session_command(CHAR_DATA *ch, const char *command, char *argument)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    char full_command[MSL];
    char local_command[MSL];

    if (!ch || !ch->desc || IS_NULLSTR(command))
        return false;

    quest_index_v2 = (QUEST_INDEX_V2_DATA *)ch->desc->pEdit;
    if (!quest_index_v2 || !quest_index_v2->area || quest_index_v2->vnum < 1) {
        send_to_char("QEdit: no active quest editor context.\n\r", ch);
        return false;
    }

    if (IS_NULLSTR(argument)) {
        snprintf(full_command, sizeof(full_command), "%ld#%ld %s",
            quest_index_v2->area->uid, quest_index_v2->vnum, command);
        snprintf(local_command, sizeof(local_command), "%s", command);
    } else {
        snprintf(full_command, sizeof(full_command), "%ld#%ld %s %.1800s",
            quest_index_v2->area->uid, quest_index_v2->vnum, command, argument);
        snprintf(local_command, sizeof(local_command), "%s %.1800s", command, argument);
    }

    qedit_internal_dispatch = true;
    do_qedit(ch, full_command);
    qedit_internal_dispatch = false;

    if (!qedit_is_readonly_session_command(local_command))
        qedit_mark_area_dirty(quest_index_v2);

    return !qedit_is_readonly_session_command(local_command);
}

static bool qedit_cmd_show(CHAR_DATA *ch, char *argument)
{
    return qedit_show(ch, argument);
}

static bool qedit_cmd_name(CHAR_DATA *ch, char *argument)        { return qedit_exec_session_command(ch, "name", argument); }
static bool qedit_cmd_summary(CHAR_DATA *ch, char *argument)     { return qedit_exec_session_command(ch, "summary", argument); }
static bool qedit_cmd_description(CHAR_DATA *ch, char *argument) { return qedit_exec_session_command(ch, "description", argument); }
static bool qedit_cmd_class(CHAR_DATA *ch, char *argument)       { return qedit_exec_session_command(ch, "class", argument); }
static bool qedit_cmd_mode(CHAR_DATA *ch, char *argument)        { return qedit_exec_session_command(ch, "mode", argument); }
static bool qedit_cmd_type(CHAR_DATA *ch, char *argument)        { return qedit_exec_session_command(ch, "type", argument); }
static bool qedit_cmd_category(CHAR_DATA *ch, char *argument)    { return qedit_exec_session_command(ch, "category", argument); }
static bool qedit_cmd_scope(CHAR_DATA *ch, char *argument)       { return qedit_exec_session_command(ch, "scope", argument); }
static bool qedit_cmd_flags(CHAR_DATA *ch, char *argument)       { return qedit_exec_session_command(ch, "flags", argument); }
static bool qedit_cmd_repeat(CHAR_DATA *ch, char *argument)      { return qedit_exec_session_command(ch, "repeat", argument); }
static bool qedit_cmd_allowance(CHAR_DATA *ch, char *argument)   { return qedit_exec_session_command(ch, "allowance", argument); }
static bool qedit_cmd_cost(CHAR_DATA *ch, char *argument)        { return qedit_exec_session_command(ch, "cost", argument); }
static bool qedit_cmd_entry(CHAR_DATA *ch, char *argument)       { return qedit_exec_session_command(ch, "entry", argument); }
static bool qedit_cmd_enabled(CHAR_DATA *ch, char *argument)     { return qedit_exec_session_command(ch, "enabled", argument); }
static bool qedit_cmd_seedpolicy(CHAR_DATA *ch, char *argument)  { return qedit_exec_session_command(ch, "seedpolicy", argument); }
static bool qedit_cmd_seed(CHAR_DATA *ch, char *argument)        { return qedit_exec_session_command(ch, "seed", argument); }
static bool qedit_cmd_varset(CHAR_DATA *ch, char *argument)      { return qedit_exec_session_command(ch, "varset", argument); }
static bool qedit_cmd_varclear(CHAR_DATA *ch, char *argument)    { return qedit_exec_session_command(ch, "varclear", argument); }
static bool qedit_cmd_addqprog(CHAR_DATA *ch, char *argument)    { return qedit_exec_session_command(ch, "addqprog", argument); }
static bool qedit_cmd_delqprog(CHAR_DATA *ch, char *argument)    { return qedit_exec_session_command(ch, "delqprog", argument); }
static bool qedit_cmd_stage(CHAR_DATA *ch, char *argument)       { return qedit_exec_session_command(ch, "stage", argument); }
static bool qedit_cmd_objective(CHAR_DATA *ch, char *argument)   { return qedit_exec_session_command(ch, "objective", argument); }
static bool qedit_cmd_reward(CHAR_DATA *ch, char *argument)      { return qedit_exec_session_command(ch, "reward", argument); }

static void qedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    int stage_count = 0;
    int objective_count = 0;
    int reward_count = 0;

    (void)ch;

    if (!ctx || !quest_index_v2)
        return;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
        stage_count++;
        for (objective = stage->objectives; objective != NULL; objective = objective->next)
            objective_count++;
    }

    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
        reward_count++;

    olc_render_section(ctx, "General");
    add_buf(ctx->buffer, formatf("{YQuest:{x %ld#%ld (%s)\n\r",
        quest_index_v2->area ? quest_index_v2->area->uid : 0,
        quest_index_v2->vnum,
        quest_index_v2->area ? quest_index_v2->area->name : "no-area"));
    add_buf(ctx->buffer, formatf("{YSummary:{x %s\n\r", quest_index_v2->name));
    add_buf(ctx->buffer, formatf("{YDescription:{x %s\n\r",
        IS_NULLSTR(quest_index_v2->description) ? "" : quest_index_v2->description));
    add_buf(ctx->buffer, formatf("{YClass:{x %s  {YType:{x %s  {YScope:{x %s\n\r",
        qedit_class_name(quest_index_v2->quest_class),
        qedit_type_name(quest_index_v2->quest_type),
        qedit_scope_name(quest_index_v2->target_scope)));
    add_buf(ctx->buffer, formatf("{YCategory:{x %s\n\r",
        qedit_category_name(quest_index_v2->category)));
    add_buf(ctx->buffer, formatf("{YFlags:{x %s\n\r",
        flag_string(quest_v2_flags, quest_index_v2->flags)));
    add_buf(ctx->buffer, formatf("{YRepeat:{x %s  {YCost:{x %d  {YEntry:{x %d\n\r",
        qedit_repeat_name(quest_index_v2->repeat_policy),
        quest_index_v2->allowance_cost,
        quest_index_v2->entry_stage_id));
    add_buf(ctx->buffer, formatf("{YEnabled:{x %s  {YSeed Policy:{x %s  {YSeed:{x %llu\n\r",
        quest_index_v2->enabled ? "on" : "off",
        qedit_seed_policy_name(quest_index_v2->seed_policy),
        quest_index_v2->fixed_seed));
    add_buf(ctx->buffer, formatf("{YCounts:{x stages=%d objectives=%d rewards=%d\n\r",
        stage_count, objective_count, reward_count));
}

static void qedit_show_flow_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    int objective_count = 0;
    int type_counts[8] = {0};

    (void)ch;

    if (!ctx || !quest_index_v2)
        return;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
        for (objective = stage->objectives; objective != NULL; objective = objective->next) {
            objective_count++;
            if (objective->objective_type >= 0 && objective->objective_type < (int)elementsof(type_counts))
                type_counts[objective->objective_type]++;
        }

    olc_render_section(ctx, "Flow (Stages + Objectives)");
    add_buf(ctx->buffer, formatf("{YTotal:{x %d\n\r", objective_count));
    add_buf(ctx->buffer, formatf("  kill:%d collect:%d talk:%d travel:%d locate:%d rescue:%d escort:%d custom:%d\n\r",
        type_counts[QUEST_OBJECTIVE_KILL],
        type_counts[QUEST_OBJECTIVE_COLLECT],
        type_counts[QUEST_OBJECTIVE_TALK],
        type_counts[QUEST_OBJECTIVE_TRAVEL],
        type_counts[QUEST_OBJECTIVE_LOCATE],
        type_counts[QUEST_OBJECTIVE_RESCUE],
        type_counts[QUEST_OBJECTIVE_ESCORT],
        type_counts[QUEST_OBJECTIVE_CUSTOM_SCRIPT]));

    if (objective_count < 1) {
        add_buf(ctx->buffer, "{DNo objectives defined yet.{x\n\r");
        return;
    }

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
        int stage_objective_count = 0;

        for (objective = stage->objectives; objective != NULL; objective = objective->next)
            stage_objective_count++;

        if (stage_objective_count < 1)
            continue;

        add_buf(ctx->buffer, "\n\r");
        add_buf(ctx->buffer, formatf("{WStage %d:{x %s  ({Ysource:{x %s {Ycomplete:{x %s {Ynext:{x %d)\n\r",
            stage->id,
            IS_NULLSTR(stage->name) ? "(unnamed)" : stage->name,
            qedit_stage_source_name(stage->stage_source),
            qedit_stage_completion_name(stage->completion_mode),
            stage->next_stage_id));
        if (!IS_NULLSTR(stage->description))
            add_buf(ctx->buffer, formatf("  {YStage Desc:{x %s\n\r", stage->description));

        for (objective = stage->objectives; objective != NULL; objective = objective->next) {
            char target_buf[MSL];
            char target_ref_buf[MIL];
            char destination_buf[MSL];
            char destination_ref_buf[MIL];
            char token_buf[MSL];
            char token_ref_buf[MIL];
            char destination_token_buf[MSL];
            char destination_token_ref_buf[MIL];

            qedit_format_wnum_display(ch, objective->target_wnum, QEDIT_WNUM_ANY,
                target_buf, sizeof(target_buf));

            if (!IS_NULLSTR(objective->target_ref_name))
                snprintf(target_ref_buf, sizeof(target_ref_buf), "$%s", objective->target_ref_name);
            else if (objective->target_ref_stage_id > 0 && objective->target_ref_objective_id > 0)
                snprintf(target_ref_buf, sizeof(target_ref_buf), "%d:%d", objective->target_ref_stage_id, objective->target_ref_objective_id);
            else
                snprintf(target_ref_buf, sizeof(target_ref_buf), "none");

            qedit_format_wnum_display(ch, objective->destination_wnum, QEDIT_WNUM_ROOM,
                destination_buf, sizeof(destination_buf));

            if (!IS_NULLSTR(objective->destination_ref_name))
                snprintf(destination_ref_buf, sizeof(destination_ref_buf), "$%s", objective->destination_ref_name);
            else
                snprintf(destination_ref_buf, sizeof(destination_ref_buf), "none");

            qedit_format_wnum_display(ch, objective->target_token_wnum, QEDIT_WNUM_TOKEN,
                token_buf, sizeof(token_buf));

            if (!IS_NULLSTR(objective->target_token_ref_name))
                snprintf(token_ref_buf, sizeof(token_ref_buf), "$%s", objective->target_token_ref_name);
            else
                snprintf(token_ref_buf, sizeof(token_ref_buf), "none");

            qedit_format_wnum_display(ch, objective->destination_token_wnum, QEDIT_WNUM_TOKEN,
                destination_token_buf, sizeof(destination_token_buf));

            if (!IS_NULLSTR(objective->destination_token_ref_name))
                snprintf(destination_token_ref_buf, sizeof(destination_token_ref_buf), "$%s", objective->destination_token_ref_name);
            else
                snprintf(destination_token_ref_buf, sizeof(destination_token_ref_buf), "none");

            add_buf(ctx->buffer, formatf("  [{W%d{x] type:%s mode:%s optional:%s required:%d qty:%d\n\r",
                objective->id,
                qedit_objective_type_name(objective->objective_type),
                qedit_target_mode_name(objective->target_mode),
                objective->optional ? "on" : "off",
                objective->required_count,
                objective->quantity));
            add_buf(ctx->buffer, formatf("      strict: %s\n\r",
                objective->strict_target ? "on" : "off"));

            if (qedit_objective_target_relevant(objective->objective_type)
                || qedit_objective_has_target_data(objective)) {
                if (objective->target_mode == QUEST_OBJECTIVE_TARGET_POOL)
                {
                    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
                    add_buf(ctx->buffer, formatf("      target: pool (%d entries)\n\r", qedit_count_pool_entries(objective)));
                    for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next) {
                        char pool_target_buf[MSL];
                        qedit_format_wnum_display(ch, pool_entry->target_wnum, QEDIT_WNUM_ANY,
                            pool_target_buf, sizeof(pool_target_buf));
                        add_buf(ctx->buffer, formatf("          - [%d] %s weight:%d\n\r",
                            pool_entry->id,
                            pool_target_buf,
                            pool_entry->weight));
                    }
                }
                else
                    add_buf(ctx->buffer, formatf("      target: %s  ref:%s  name:%s\n\r",
                        target_buf,
                        target_ref_buf,
                        IS_NULLSTR(objective->target_variable_name) ? "none" : objective->target_variable_name));
            }

            if (qedit_objective_destination_relevant(objective->objective_type)
                || qedit_objective_has_destination_data(objective)) {
                add_buf(ctx->buffer, formatf("      dest: %s  ref:%s  name:%s\n\r",
                    destination_buf,
                    destination_ref_buf,
                    IS_NULLSTR(objective->destination_variable_name) ? "none" : objective->destination_variable_name));
            }

            if (qedit_objective_token_relevant(objective->objective_type)
                || qedit_objective_has_token_data(objective)) {
                add_buf(ctx->buffer, formatf("      token: %s  ref:%s  name:%s\n\r",
                    token_buf,
                    token_ref_buf,
                    IS_NULLSTR(objective->target_token_variable_name) ? "none" : objective->target_token_variable_name));
                add_buf(ctx->buffer, formatf("      desttoken: %s  ref:%s  name:%s\n\r",
                    destination_token_buf,
                    destination_token_ref_buf,
                    IS_NULLSTR(objective->destination_token_variable_name) ? "none" : objective->destination_token_variable_name));
            }

            if (!IS_NULLSTR(objective->target_tag))
                add_buf(ctx->buffer, formatf("      summary: %s\n\r", objective->target_tag));
            if (!IS_NULLSTR(objective->description))
                add_buf(ctx->buffer, formatf("      description: %s\n\r", objective->description));
        }
    }
}

static void qedit_show_notes_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    QUEST_STAGE_INDEX_V2_DATA *entry_stage;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    int stage_count = 0;
    int objective_count = 0;
    int reward_count = 0;

    (void)ch;

    if (!ctx || !quest_index_v2)
        return;

    for (QUEST_STAGE_INDEX_V2_DATA *stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
        stage_count++;
        for (QUEST_OBJECTIVE_INDEX_V2_DATA *objective = stage->objectives; objective != NULL; objective = objective->next)
            objective_count++;
    }

    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
        reward_count++;

    entry_stage = qedit_find_stage(quest_index_v2, quest_index_v2->entry_stage_id);

    olc_render_section(ctx, "Builder Notes / Meta");
    add_buf(ctx->buffer, formatf("{YEntry Stage:{x %d (%s)\n\r",
        quest_index_v2->entry_stage_id,
        entry_stage ? "ok" : "missing"));
    add_buf(ctx->buffer, formatf("{YTopology:{x stages=%d objectives=%d rewards=%d\n\r",
        stage_count,
        objective_count,
        reward_count));
    add_buf(ctx->buffer, "\n\r{YObjective Field Relevance:{x\n\r");
    add_buf(ctx->buffer, "  kill/talk/locate: target\n\r");
    add_buf(ctx->buffer, "  collect: target (object preferred)\n\r");
    add_buf(ctx->buffer, "  travel: destination\n\r");
    add_buf(ctx->buffer, "  rescue/escort: target + destination\n\r");
    add_buf(ctx->buffer, "  custom: script-defined (only populated fields shown)\n\r");
    add_buf(ctx->buffer, "  token: attaches to the resolved objective anchor (target, or destination fallback)\n\r");
    add_buf(ctx->buffer, "\n\r{YNotes:{x Flow tab hides irrelevant fields by type unless data is already set.\n\r");
}

static void qedit_show_scripting_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&qedit_def);

    (void)ch;

    if (!ctx || !quest_index_v2)
        return;

    olc_display_scripts(ctx, theme, quest_index_v2->progs, PRG_QPROG,
        "QuestProg Vnum", "addqprog", "delqprog");

    olc_display_vars(ctx, theme, quest_index_v2->index_vars, "varset", "varclear");
}

static void qedit_show_rewards_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    QUEST_INDEX_V2_DATA *quest_index_v2 = (QUEST_INDEX_V2_DATA *)pEdit;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    int reward_index = 1;

    (void)ch;

    if (!ctx || !quest_index_v2)
        return;

    olc_render_section(ctx, "Rewards");
    if (!quest_index_v2->rewards) {
        add_buf(ctx->buffer, "{DNo rewards defined yet.{x\n\r");
        return;
    }

    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next) {
        char target_buf[MSL];

        qedit_format_wnum_display(ch, reward->target_wnum, QEDIT_WNUM_ANY,
            target_buf, sizeof(target_buf));

        add_buf(ctx->buffer, formatf("[{W%d{x] type:%s amount:%ld target:%s\n\r",
            reward_index++,
            qedit_reward_type_name(reward->reward_type),
            reward->amount,
            target_buf));
        if (!IS_NULLSTR(reward->currency))
            add_buf(ctx->buffer, formatf("      currency:%s\n\r", reward->currency));
        if (!IS_NULLSTR(reward->script))
            add_buf(ctx->buffer, formatf("      script:%s\n\r", reward->script));
    }
}

static bool qedit_show(CHAR_DATA *ch, char *argument)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    OLC_LAYOUT_CTX *ctx;
    int tab;
    bool render_all_tabs;

    if (!ch || !ch->desc)
        return false;

    quest_index_v2 = (QUEST_INDEX_V2_DATA *)ch->desc->pEdit;
    if (!quest_index_v2)
        return false;

    ctx = olc_display_new(ch, olc_get_theme(&qedit_def));
    if (!ctx)
        return false;

    tab = ch->desc ? ch->desc->nEditTab : 0;
    if (tab < 0)
        tab = 0;
    if (tab >= qedit_def.tabs.count)
        tab = qedit_def.tabs.count - 1;
    if (ch->desc)
        ch->desc->nEditTab = tab;

    ctx->current_tab = tab;

    olc_display_header(ctx, "QEdit", quest_index_v2->name,
        formatf("%ld#%ld",
            quest_index_v2->area ? quest_index_v2->area->uid : 0,
            quest_index_v2->vnum), &qedit_def);

    render_all_tabs = (!IS_NULLSTR(argument) && !str_prefix(argument, "all")) || olc_show_all_tabs_mode(ch);
    if (render_all_tabs) {
        for (int i = 0; i < qedit_def.tabs.count; i++) {
            if (qedit_def.tabs.tabs[i].show_fn)
                qedit_def.tabs.tabs[i].show_fn(ch, ctx, quest_index_v2);
        }
    } else if (tab >= 0 && tab < qedit_def.tabs.count && qedit_def.tabs.tabs[tab].show_fn) {
        qedit_def.tabs.tabs[tab].show_fn(ch, ctx, quest_index_v2);
    } else {
        qedit_show_general_tab(ch, ctx, quest_index_v2);
    }

    olc_display_footer(ctx, olc_get_theme(&qedit_def));

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

void do_qedit(CHAR_DATA *ch, char *argument)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    WNUM wnum;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    char arg5[MIL];
    char arg6[MIL];
    char original_input[MSL];
    char text_after_arg2[MSL];
    char text_after_arg4[MSL];
    char text_after_arg5[MSL];
    char text_after_arg6[MSL];
    bool entered_editor = false;

    if (!ch || !ch->pcdata || IS_NPC(ch))
        return;

    if (!IS_IMPLEMENTOR(ch)) {
        send_to_char("QEdit: Insufficient security.\n\r", ch);
        return;
    }

    if (!IS_NULLSTR(argument)) {
        strncpy(original_input, argument, sizeof(original_input) - 1);
        original_input[sizeof(original_input) - 1] = '\0';
    } else {
        original_input[0] = '\0';
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);

    qedit_tail_after(original_input, 2, text_after_arg2, sizeof(text_after_arg2));
    qedit_tail_after(original_input, 4, text_after_arg4, sizeof(text_after_arg4));
    qedit_tail_after(original_input, 5, text_after_arg5, sizeof(text_after_arg5));
    qedit_tail_after(original_input, 6, text_after_arg6, sizeof(text_after_arg6));

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  qedit <auid>#<vnum> create\n\r", ch);
        send_to_char("  qedit <auid>#<vnum>|<vnum> show [all]\n\r", ch);
        send_to_char("  qedit <ref> tab <name|number>\n\r", ch);
        send_to_char("  qedit <ref> summary <text>\n\r", ch);
        send_to_char("  qedit <ref> description [none]  (opens string editor)\n\r", ch);
        send_to_char("  qedit <ref> class <narrative|mission>\n\r", ch);
        send_to_char("  qedit <ref> type <main|side|unlock|class|event|other>\n\r", ch);
        send_to_char("  qedit <ref> category <none|regional|class|story|church|dungeon|crafting|event|other>\n\r", ch);
        send_to_char("  qedit <ref> scope <character|group|church>\n\r", ch);
        send_to_char("  qedit <ref> flags <flag>   (currently: group_snapshot)\n\r", ch);
        send_to_char("  qedit <ref> repeat <once|repeatable>\n\r", ch);
        send_to_char("  qedit <ref> allowance <cost>\n\r", ch);
        send_to_char("  qedit <ref> cost <amount>\n\r", ch);
        send_to_char("  qedit <ref> entry <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> enabled <on|off>\n\r", ch);
        send_to_char("  qedit <ref> seedpolicy <auto|fixed>\n\r", ch);
        send_to_char("  qedit <ref> seed <number|none>\n\r", ch);
        send_to_char("  qedit <ref> varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
        send_to_char("  qedit <ref> varclear <name>\n\r", ch);
        send_to_char("  qedit <ref> addqprog <widevnum> <trigger> <phrase>\n\r", ch);
        send_to_char("  qedit <ref> delqprog <group#> [trigger#]\n\r", ch);
        send_to_char("  qedit <ref> stage list\n\r", ch);
        send_to_char("  qedit <ref> stage add <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage del <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage show <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> source <static|generated>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> autocommence <on|off>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> complete <all|any|custom>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> next <stage_id|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> enter <text|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> exit <text|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> profile <text|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> salt <number|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> summary <text>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> description [none]  (opens string editor)\n\r", ch);
        send_to_char("  qedit <ref> objective list <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> objective add <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective del <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective show <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> type <kill|collect|talk|travel|locate|rescue|escort|custom>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> required <count>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> quantity <count>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> optional <on|off>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> strict <on|off>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target mode <exact|pool>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target wnum <auid>#<vnum|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target ref <stage_id>:<objective_id|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target name <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target pool <list|add|del> ...\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination wnum <auid>#<vnum|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination ref <$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination name <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination token wnum <auid>#<vnum|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination token ref <$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination token name <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> token wnum <auid>#<vnum|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> token ref <$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> token name <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> summary <text|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> description [none]  (opens string editor)\n\r", ch);
        send_to_char("  qedit <ref> reward list\n\r", ch);
        send_to_char("  qedit <ref> reward add [type]\n\r", ch);
        send_to_char("  qedit <ref> reward del <index>\n\r", ch);
        send_to_char("  qedit <ref> reward show <index>\n\r", ch);
        send_to_char("  qedit <ref> reward <index> type <points|currency|reputation|token|item|script>\n\r", ch);
        send_to_char("  qedit <ref> reward <index> amount <number>\n\r", ch);
        send_to_char("  qedit <ref> reward <index> target <auid>#<vnum|none>\n\r", ch);
        send_to_char("  qedit <ref> reward <index> currency <text|none>\n\r", ch);
        send_to_char("  qedit <ref> reward <index> script <text|none>\n\r", ch);
        send_to_char("  (legacy aliases still accepted: targetmode/targetref/refname/destinationref/.../targettokenref...)\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "create")) {
        AREA_DATA *target_area = NULL;
        long target_vnum = 0;

        if (!IS_NULLSTR(arg2)) {
            if (!qedit_parse_index_ref(ch, arg2, &wnum)) {
                send_to_char("QEdit: Invalid quest reference. Use <auid>#<vnum> (or bare <vnum> in-area).\n\r", ch);
                return;
            }

            target_area = wnum.pArea;
            target_vnum = wnum.vnum;
        }
        else
        {
            if (!ch->in_room || !ch->in_room->area) {
                send_to_char("QEdit: You must be in an area to auto-create a quest vnum.\n\r", ch);
                return;
            }

            target_area = ch->in_room->area;
            target_vnum = qedit_next_vnum_in_area(target_area);
            if (target_vnum < 1) {
                send_to_char("QEdit: Unable to allocate a quest vnum in this area.\n\r", ch);
                return;
            }
        }

        wnum.pArea = target_area;
        wnum.vnum = target_vnum;

        quest_index_v2 = get_quest_index_v2_wnum(wnum);
        if (quest_index_v2) {
            send_to_char("QEdit: Quest index already exists.\n\r", ch);
            return;
        }

        quest_index_v2 = new_quest_index_v2();
        quest_index_v2->area = wnum.pArea;
        quest_index_v2->vnum = wnum.vnum;
        if (!quest_index_v2_register(quest_index_v2)) {
            free_quest_index_v2(quest_index_v2);
            send_to_char("QEdit: Failed to register quest index.\n\r", ch);
            return;
        }

        printf_to_char(ch, "QEdit: Created quest index %ld#%ld (%s).\n\r",
            wnum.pArea->uid, wnum.vnum, wnum.pArea->name);

        qedit_mark_area_dirty(quest_index_v2);

        qedit_enter_editor(ch, quest_index_v2);
        return;
    }

    if (!qedit_parse_index_ref(ch, arg1, &wnum)) {
        send_to_char("QEdit: Invalid quest reference. Use <auid>#<vnum> (or bare <vnum> in-area).\n\r", ch);
        return;
    }

    quest_index_v2 = get_quest_index_v2_wnum(wnum);

    if (!IS_NULLSTR(arg2) && !str_prefix(arg2, "create")) {
        if (quest_index_v2) {
            send_to_char("QEdit: Quest index already exists.\n\r", ch);
            return;
        }

        quest_index_v2 = new_quest_index_v2();
        quest_index_v2->area = wnum.pArea;
        quest_index_v2->vnum = wnum.vnum;
        if (!quest_index_v2_register(quest_index_v2)) {
            free_quest_index_v2(quest_index_v2);
            send_to_char("QEdit: Failed to register quest index.\n\r", ch);
            return;
        }

        printf_to_char(ch, "QEdit: Created quest index %ld#%ld (%s).\n\r",
            wnum.pArea->uid, wnum.vnum, wnum.pArea->name);

        qedit_mark_area_dirty(quest_index_v2);

        qedit_enter_editor(ch, quest_index_v2);
        return;
    }

    if (!quest_index_v2) {
        send_to_char("QEdit: Quest index not found. Use CREATE first.\n\r", ch);
        return;
    }

    if ((!ch->desc || ch->desc->editor != ED_QUEST || ch->desc->pEdit != (void *)quest_index_v2)
        && !IS_NULLSTR(arg2)
        && str_prefix(arg2, "show")) {
        send_to_char("QEdit: Open the quest first with 'qedit <ref>', then use subcommands in-session.\n\r", ch);
        return;
    }

    if (!ch->desc || ch->desc->editor != ED_QUEST || ch->desc->pEdit != (void *)quest_index_v2) {
        qedit_enter_editor(ch, quest_index_v2);
        entered_editor = true;
    }

    if (IS_NULLSTR(arg2)) {
        if (!entered_editor)
            qedit_show(ch, "");
        return;
    }

    if (!qedit_internal_dispatch && str_prefix(arg2, "show")) {
        send_to_char("QEdit: Use 'qedit <ref>' or 'qedit create' to enter, then run subcommands in-session.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "commands") || !str_cmp(arg2, "?") || !str_prefix(arg2, "help")) {
        send_to_char("QEdit session tips:\n\r", ch);
        send_to_char("  show | show all\n\r", ch);
        send_to_char("  tab <name|number>\n\r", ch);
        send_to_char("  stage list | stage add <id> | stage <id> ...\n\r", ch);
        send_to_char("  objective list <stage> | objective add <stage> <id> | objective edit <stage> <id> ...\n\r", ch);
        send_to_char("  objective edit <stage> <id> target|destination|token <wnum|ref|name|mode|pool> ...\n\r", ch);
        send_to_char("  reward list | reward add [type] | reward <index> ...\n\r", ch);
        send_to_char("  addqprog <widevnum> <trigger> <phrase> | delqprog <group#> [trigger#]\n\r", ch);
        send_to_char("  done\n\r", ch);
        send_to_char("In [QEdit], use subcommands directly (do not prefix with 'qedit').\n\r", ch);
        return;
    }

    if (IS_NULLSTR(arg2) || !str_prefix(arg2, "show")) {
        qedit_show(ch, arg3);
        return;
    }

    if (!str_prefix(arg2, "name") || !str_prefix(arg2, "summary")) {
        if (IS_NULLSTR(text_after_arg2) || !str_cmp(text_after_arg2, "none")) {
            free_string(quest_index_v2->name);
            quest_index_v2->name = str_dup("unnamed quest");
            send_to_char("QEdit: summary reset to 'unnamed quest'.\n\r", ch);
            return;
        }

        free_string(quest_index_v2->name);
        quest_index_v2->name = str_dup(text_after_arg2);
        printf_to_char(ch, "QEdit: summary set to %s.\n\r", quest_index_v2->name);
        return;
    }

    if (!str_prefix(arg2, "description") || !str_prefix(arg2, "desc")) {
        if (!IS_NULLSTR(arg3) && !str_cmp(arg3, "none")) {
            free_string(quest_index_v2->description);
            quest_index_v2->description = str_dup("");
            send_to_char("QEdit: quest description cleared.\n\r", ch);
            return;
        }

        if (!IS_NULLSTR(arg3)) {
            send_to_char("QEdit: description opens the string editor; use 'description' (or 'description none').\n\r", ch);
            return;
        }

        send_to_char("QEdit: editing quest description.\n\r", ch);
        string_append(ch, &quest_index_v2->description);
        return;
    }

    if (!str_prefix(arg2, "class") || !str_prefix(arg2, "mode")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: class requires narrative or mission.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "narrative"))
            quest_index_v2->quest_class = QUEST_CLASS_NARRATIVE;
        else if (!str_prefix(arg3, "mission"))
            quest_index_v2->quest_class = QUEST_CLASS_MISSION;
        else {
            send_to_char("QEdit: unknown class.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: class set to %s.\n\r", qedit_class_name(quest_index_v2->quest_class));
        return;
    }

    if (!str_prefix(arg2, "type")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: type requires main, side, unlock, class, event, or other.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "main"))
            quest_index_v2->quest_type = QUEST_TYPE_MAIN_STORY;
        else if (!str_prefix(arg3, "side"))
            quest_index_v2->quest_type = QUEST_TYPE_SIDE_QUEST;
        else if (!str_prefix(arg3, "unlock"))
            quest_index_v2->quest_type = QUEST_TYPE_UNLOCK;
        else if (!str_prefix(arg3, "class"))
            quest_index_v2->quest_type = QUEST_TYPE_CLASS_QUEST;
        else if (!str_prefix(arg3, "event"))
            quest_index_v2->quest_type = QUEST_TYPE_EVENT;
        else if (!str_prefix(arg3, "other"))
            quest_index_v2->quest_type = QUEST_TYPE_OTHER;
        else {
            send_to_char("QEdit: unknown type.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: type set to %s.\n\r", qedit_type_name(quest_index_v2->quest_type));
        return;
    }

    if (!str_prefix(arg2, "category")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: category requires none, regional, class, story, church, dungeon, crafting, event, or other.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "none"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_NONE;
        else if (!str_prefix(arg3, "regional"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_REGIONAL;
        else if (!str_prefix(arg3, "class"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_CLASS;
        else if (!str_prefix(arg3, "story") || !str_prefix(arg3, "main"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_STORY;
        else if (!str_prefix(arg3, "church"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_CHURCH;
        else if (!str_prefix(arg3, "dungeon"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_DUNGEON;
        else if (!str_prefix(arg3, "crafting"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_CRAFTING;
        else if (!str_prefix(arg3, "event"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_EVENT;
        else if (!str_prefix(arg3, "other"))
            quest_index_v2->category = QUEST_LOG_CATEGORY_OTHER;
        else {
            send_to_char("QEdit: unknown category.\n\r", ch);
            return;
        }

        printf_to_char(ch, "QEdit: category set to %s.\n\r", qedit_category_name(quest_index_v2->category));
        return;
    }

    if (!str_prefix(arg2, "scope")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: scope requires character, group, or church.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "character"))
            quest_index_v2->target_scope = QUEST_TARGET_SCOPE_CHARACTER;
        else if (!str_prefix(arg3, "group"))
            quest_index_v2->target_scope = QUEST_TARGET_SCOPE_GROUP;
        else if (!str_prefix(arg3, "church"))
            quest_index_v2->target_scope = QUEST_TARGET_SCOPE_CHURCH;
        else {
            send_to_char("QEdit: unknown scope.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: scope set to %s.\n\r", qedit_scope_name(quest_index_v2->target_scope));
        return;
    }

    if (!str_prefix(arg2, "flags")) {
        long value;

        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: flags requires a flag name.\n\r", ch);
            send_to_char("Try '? quest_v2_flags'.\n\r", ch);
            return;
        }

        value = flag_value(quest_v2_flags, arg3);
        if (value == NO_FLAG) {
            send_to_char("QEdit: unknown quest flag. Try '? quest_v2_flags'.\n\r", ch);
            return;
        }

        TOGGLE_BIT(quest_index_v2->flags, value);
        printf_to_char(ch, "QEdit: flags now %s.\n\r",
            flag_string(quest_v2_flags, quest_index_v2->flags));
        return;
    }

    if (!str_prefix(arg2, "repeat")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: repeat requires once or repeatable.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "once"))
            quest_index_v2->repeat_policy = QUEST_REPEAT_ONCE;
        else if (!str_prefix(arg3, "repeatable"))
            quest_index_v2->repeat_policy = QUEST_REPEAT_REPEATABLE;
        else {
            send_to_char("QEdit: unknown repeat policy.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: repeat set to %s.\n\r", qedit_repeat_name(quest_index_v2->repeat_policy));
        return;
    }

    if (!str_prefix(arg2, "allowance") || !str_prefix(arg2, "cost")) {
        long value;

        if (IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("QEdit: cost requires a numeric value >= 0.\n\r", ch);
            return;
        }

        value = atol(arg3);
        if (value < 0) {
            send_to_char("QEdit: cost must be >= 0.\n\r", ch);
            return;
        }
        quest_index_v2->allowance_cost = (int)value;
        printf_to_char(ch, "QEdit: cost set to %d.\n\r", quest_index_v2->allowance_cost);
        return;
    }

    if (!str_prefix(arg2, "entry")) {
        long value;

        if (IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("QEdit: entry requires a numeric stage id >= 1.\n\r", ch);
            return;
        }

        value = atol(arg3);
        if (value < 1) {
            send_to_char("QEdit: entry stage id must be >= 1.\n\r", ch);
            return;
        }
        quest_index_v2->entry_stage_id = (int)value;
        printf_to_char(ch, "QEdit: entry stage set to %d.\n\r", quest_index_v2->entry_stage_id);
        return;
    }

    if (!str_prefix(arg2, "enabled")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: enabled requires on|off.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "on") || !str_prefix(arg3, "yes") || !str_cmp(arg3, "1"))
            quest_index_v2->enabled = true;
        else if (!str_prefix(arg3, "off") || !str_prefix(arg3, "no") || !str_cmp(arg3, "0"))
            quest_index_v2->enabled = false;
        else {
            send_to_char("QEdit: enabled requires on|off.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: enabled set to %s.\n\r", quest_index_v2->enabled ? "on" : "off");
        return;
    }

    if (!str_prefix(arg2, "seedpolicy")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: seedpolicy requires auto|fixed.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "auto"))
            quest_index_v2->seed_policy = QUEST_SEED_POLICY_AUTO;
        else if (!str_prefix(arg3, "fixed"))
            quest_index_v2->seed_policy = QUEST_SEED_POLICY_FIXED;
        else {
            send_to_char("QEdit: unknown seedpolicy.\n\r", ch);
            return;
        }

        printf_to_char(ch, "QEdit: seedpolicy set to %s.\n\r",
            qedit_seed_policy_name(quest_index_v2->seed_policy));
        return;
    }

    if (!str_prefix(arg2, "seed")) {
        if (IS_NULLSTR(arg3) || !str_cmp(arg3, "none") || !str_cmp(arg3, "auto")) {
            quest_index_v2->fixed_seed = 0;
            send_to_char("QEdit: fixed seed cleared.\n\r", ch);
            return;
        }

        if (!is_number(arg3)) {
            send_to_char("QEdit: seed requires a numeric value.\n\r", ch);
            return;
        }

        quest_index_v2->fixed_seed = (unsigned long long)strtoull(arg3, NULL, 10);
        printf_to_char(ch, "QEdit: fixed seed set to %llu.\n\r", quest_index_v2->fixed_seed);
        return;
    }

    if (!str_prefix(arg2, "varset")) {
        if (!olc_varset(&quest_index_v2->index_vars, ch, argument, false))
            send_to_char("Syntax: varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "varclear")) {
        if (!olc_varclear(&quest_index_v2->index_vars, ch, argument, false))
            send_to_char("Syntax: varclear <name>\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "addqprog")) {
        int tindex;
        int slot;
        PROG_LIST *list;
        SCRIPT_DATA *code;
        WNUM script_wnum;
        AREA_DATA *context;

        if (IS_NULLSTR(arg3) || IS_NULLSTR(arg4) || IS_NULLSTR(arg5)) {
            send_to_char("Syntax: addqprog <widevnum> <trigger> <phrase>\n\r", ch);
            return;
        }

        if ((tindex = trigger_index(arg4, PRG_QPROG)) < 0) {
            send_to_char("Valid flags are:\n\r", ch);
            show_help(ch, "qprog");
            return;
        }

        slot = trigger_table[tindex].slot;
        context = olc_relative_widevnum_context(quest_index_v2->area, arg3);
        if (!parse_widevnum(arg3, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return;
        }

        if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_QPROG)) == NULL) {
            send_to_char("No such QUESTProgram.\n\r", ch);
            return;
        }

        if (!quest_index_v2->progs)
            quest_index_v2->progs = new_prog_bank();

        if (edit_trigger_exists(quest_index_v2->progs, code, tindex, arg5)) {
            send_to_char("That trigger/phrase pair is already attached to that script on this quest.\n\r", ch);
            return;
        }

        list = new_trigger();
        list->vnum = script_wnum.vnum;
        list->script_is_widevnum = (script_wnum.pArea != NULL);
        if (list->script_is_widevnum) {
            list->script_load.auid = script_wnum.pArea->uid;
            list->script_load.vnum = script_wnum.vnum;
        }
        list->trig_type = tindex;
        list->trig_phrase = str_dup(arg5);
        if (is_widevnum_format(arg5)) {
            list->numeric = true;
            list->trig_is_widevnum = true;
            parse_widevnum_load(arg5, &list->trig_load);
            list->trig_number = (int)list->trig_load.vnum;
        } else {
            list->trig_number = atoi(list->trig_phrase);
            list->numeric = is_number(list->trig_phrase);
        }

        list->script = code;
        list_appendlink(quest_index_v2->progs[slot], list);

        send_to_char("Qprog Added.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "delqprog")) {
        int group_idx;
        int trig_idx;
        PROG_GROUP groups[MAX_PROG_GROUPS];
        int num_groups;

        if (!quest_index_v2->progs) {
            send_to_char("This quest has no programs attached.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(arg3)) {
            send_to_char("Syntax: delqprog <group#>\n\r", ch);
            send_to_char("        delqprog <group#> <trigger#>\n\r", ch);
            return;
        }

        if (!is_number(arg3)) {
            send_to_char("Please specify a valid group number.\n\r", ch);
            return;
        }

        group_idx = atoi(arg3);
        num_groups = prog_build_groups(quest_index_v2->progs, groups, MAX_PROG_GROUPS, PRG_QPROG);

        if (group_idx < 1 || group_idx > num_groups) {
            send_to_char("Invalid group number.\n\r", ch);
            return;
        }

        PROG_GROUP *group = &groups[group_idx - 1];

        if (IS_NULLSTR(arg4)) {
            if (edit_delscript(quest_index_v2->progs, group->script)) {
                send_to_char("Script group removed.\n\r", ch);
                return;
            }
        } else {
            PROG_GROUP_ENTRY *entry;

            if (!is_number(arg4)) {
                send_to_char("Please specify a valid trigger number within the group.\n\r", ch);
                return;
            }

            trig_idx = atoi(arg4);
            if (trig_idx < 1 || trig_idx > group->trigger_count) {
                send_to_char("Invalid trigger number within that group.\n\r", ch);
                return;
            }

            entry = &group->triggers[trig_idx - 1];
            if (edit_deltrigger_specific(quest_index_v2->progs, group->script,
                entry->entry->trig_type, entry->entry->trig_phrase)) {
                send_to_char("Trigger removed from script group.\n\r", ch);
                return;
            }
        }

        send_to_char("No such program or trigger found.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "stage")) {
        long stage_id;

        if (!str_prefix(arg3, "list")) {
            if (!quest_index_v2->stages) {
                send_to_char("QEdit: no stages on this quest index.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Stages for %ld#%ld:\n\r",
                quest_index_v2->area ? quest_index_v2->area->uid : 0,
                quest_index_v2->vnum);
            for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next) {
                int objective_count = 0;
                for (objective = stage->objectives; objective != NULL; objective = objective->next)
                    objective_count++;
                printf_to_char(ch, "  [%d] %s  source:%s  complete:%s  next:%d  objectives:%d\n\r",
                    stage->id,
                    IS_NULLSTR(stage->name) ? "(unnamed)" : stage->name,
                    qedit_stage_source_name(stage->stage_source),
                    qedit_stage_completion_name(stage->completion_mode),
                    stage->next_stage_id,
                    objective_count);
                printf_to_char(ch, "       autocommence:%s  profile:%s\n\r",
                    stage->auto_commence ? "on" : "off",
                    IS_NULLSTR(stage->generator_profile) ? "" : stage->generator_profile);
            }
            return;
        }

        if (!str_prefix(arg3, "add")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: stage add requires <stage_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            if (stage_id < 1) {
                send_to_char("QEdit: stage id must be >= 1.\n\r", ch);
                return;
            }

            if (qedit_find_stage(quest_index_v2, (int)stage_id)) {
                send_to_char("QEdit: stage id already exists.\n\r", ch);
                return;
            }

            stage = new_quest_stage_index_v2();
            stage->id = (int)stage_id;
            qedit_append_stage(quest_index_v2, stage);
            printf_to_char(ch, "QEdit: added stage %d.\n\r", stage->id);
            return;
        }

        if (!str_prefix(arg3, "del")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: stage del requires <stage_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            if (stage_id < 1) {
                send_to_char("QEdit: stage id must be >= 1.\n\r", ch);
                return;
            }

            if (!qedit_remove_stage(quest_index_v2, (int)stage_id)) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            if (quest_index_v2->entry_stage_id == (int)stage_id)
                quest_index_v2->entry_stage_id = 1;

            printf_to_char(ch, "QEdit: deleted stage %d.\n\r", (int)stage_id);
            return;
        }

        if (!str_prefix(arg3, "show")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: stage show requires <stage_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            stage = qedit_find_stage(quest_index_v2, (int)stage_id);
            if (!stage) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Stage %d\n\r", stage->id);
            printf_to_char(ch, "  summary   : %s\n\r", stage->name);
            printf_to_char(ch, "  description: %s\n\r", stage->description);
            printf_to_char(ch, "  source    : %s\n\r", qedit_stage_source_name(stage->stage_source));
            printf_to_char(ch, "  autocommence: %s\n\r", stage->auto_commence ? "on" : "off");
            printf_to_char(ch, "  complete  : %s\n\r", qedit_stage_completion_name(stage->completion_mode));
            printf_to_char(ch, "  next      : %d\n\r", stage->next_stage_id);
            printf_to_char(ch, "  enter     : %s\n\r", IS_NULLSTR(stage->on_enter_script) ? "" : stage->on_enter_script);
            printf_to_char(ch, "  exit      : %s\n\r", IS_NULLSTR(stage->on_exit_script) ? "" : stage->on_exit_script);
            printf_to_char(ch, "  profile   : %s\n\r", stage->generator_profile);
            printf_to_char(ch, "  salt      : %llu\n\r", stage->generator_salt);
            return;
        }

        if (IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("QEdit: stage command requires stage id or list/add/del/show.\n\r", ch);
            return;
        }

        stage_id = atol(arg3);
        if (stage_id < 1) {
            send_to_char("QEdit: stage id must be >= 1.\n\r", ch);
            return;
        }

        stage = qedit_find_stage(quest_index_v2, (int)stage_id);
        if (!stage) {
            send_to_char("QEdit: stage not found.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(arg4)) {
            send_to_char("QEdit: stage edit requires a field (summary/description/source/autocommence/complete/next/enter/exit/profile/salt).\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "name") || !str_prefix(arg4, "summary")) {
            if (IS_NULLSTR(text_after_arg4)) {
                send_to_char("QEdit: stage summary requires text.\n\r", ch);
                return;
            }
            free_string(stage->name);
            stage->name = str_dup(text_after_arg4);
            send_to_char("QEdit: stage summary updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "desc") || !str_prefix(arg4, "description")) {
            if (!IS_NULLSTR(arg5) && !str_cmp(arg5, "none")) {
                free_string(stage->description);
                stage->description = str_dup("");
                send_to_char("QEdit: stage description cleared.\n\r", ch);
                return;
            }

            if (!IS_NULLSTR(arg5)) {
                send_to_char("QEdit: stage description opens the string editor; use 'description' (or 'description none').\n\r", ch);
                return;
            }

            send_to_char("QEdit: editing stage description.\n\r", ch);
            string_append(ch, &stage->description);
            return;
        }

        if (!str_prefix(arg4, "source")) {
            if (IS_NULLSTR(arg5)) {
                send_to_char("QEdit: source requires static or generated.\n\r", ch);
                return;
            }
            if (!str_prefix(arg5, "static"))
                stage->stage_source = QUEST_STAGE_SOURCE_STATIC;
            else if (!str_prefix(arg5, "generated"))
                stage->stage_source = QUEST_STAGE_SOURCE_GENERATED;
            else {
                send_to_char("QEdit: unknown stage source.\n\r", ch);
                return;
            }
            printf_to_char(ch, "QEdit: stage source set to %s.\n\r", qedit_stage_source_name(stage->stage_source));
            return;
        }

        if (!str_prefix(arg4, "autocommence") || !str_prefix(arg4, "autostart")) {
            if (IS_NULLSTR(arg5)) {
                send_to_char("QEdit: autocommence requires on|off.\n\r", ch);
                return;
            }

            if (!str_prefix(arg5, "on") || !str_prefix(arg5, "yes") || !str_cmp(arg5, "1"))
                stage->auto_commence = true;
            else if (!str_prefix(arg5, "off") || !str_prefix(arg5, "no") || !str_cmp(arg5, "0"))
                stage->auto_commence = false;
            else {
                send_to_char("QEdit: autocommence requires on|off.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: stage autocommence set to %s.\n\r", stage->auto_commence ? "on" : "off");
            return;
        }

        if (!str_prefix(arg4, "complete")) {
            if (IS_NULLSTR(arg5)) {
                send_to_char("QEdit: complete requires all, any, or custom.\n\r", ch);
                return;
            }
            if (!str_prefix(arg5, "all"))
                stage->completion_mode = QUEST_STAGE_COMPLETE_ALL;
            else if (!str_prefix(arg5, "any"))
                stage->completion_mode = QUEST_STAGE_COMPLETE_ANY;
            else if (!str_prefix(arg5, "custom"))
                stage->completion_mode = QUEST_STAGE_COMPLETE_CUSTOM;
            else {
                send_to_char("QEdit: unknown completion mode.\n\r", ch);
                return;
            }
            printf_to_char(ch, "QEdit: stage completion mode set to %s.\n\r", qedit_stage_completion_name(stage->completion_mode));
            return;
        }

        if (!str_prefix(arg4, "next")) {
            long value;

            if (IS_NULLSTR(arg5) || !str_cmp(arg5, "none")) {
                stage->next_stage_id = 0;
                send_to_char("QEdit: stage next cleared.\n\r", ch);
                return;
            }

            if (!is_number(arg5)) {
                send_to_char("QEdit: next requires stage id or none.\n\r", ch);
                return;
            }

            value = atol(arg5);
            if (value < 1) {
                send_to_char("QEdit: next stage id must be >= 1.\n\r", ch);
                return;
            }
            stage->next_stage_id = (int)value;
            printf_to_char(ch, "QEdit: stage next set to %d.\n\r", stage->next_stage_id);
            return;
        }

        if (!str_prefix(arg4, "enter") || !str_prefix(arg4, "onenter") || !str_prefix(arg4, "enterscript")) {
            free_string(stage->on_enter_script);
            stage->on_enter_script = str_dup((IS_NULLSTR(arg5) || !str_cmp(arg5, "none")) ? "" : text_after_arg4);
            send_to_char("QEdit: stage enter script updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "exit") || !str_prefix(arg4, "onexit") || !str_prefix(arg4, "exitscript")) {
            free_string(stage->on_exit_script);
            stage->on_exit_script = str_dup((IS_NULLSTR(arg5) || !str_cmp(arg5, "none")) ? "" : text_after_arg4);
            send_to_char("QEdit: stage exit script updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "profile")) {
            if (IS_NULLSTR(text_after_arg4) || !str_cmp(text_after_arg4, "none")) {
                free_string(stage->generator_profile);
                stage->generator_profile = str_dup("");
                send_to_char("QEdit: stage generator profile cleared.\n\r", ch);
                return;
            }

            free_string(stage->generator_profile);
            stage->generator_profile = str_dup(text_after_arg4);
            send_to_char("QEdit: stage generator profile updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "salt")) {
            if (IS_NULLSTR(arg5) || !str_cmp(arg5, "none")) {
                stage->generator_salt = 0;
                send_to_char("QEdit: stage generator salt cleared.\n\r", ch);
                return;
            }

            if (!is_number(arg5)) {
                send_to_char("QEdit: salt requires numeric value or none.\n\r", ch);
                return;
            }

            stage->generator_salt = (unsigned long long)strtoull(arg5, NULL, 10);
            printf_to_char(ch, "QEdit: stage generator salt set to %llu.\n\r", stage->generator_salt);
            return;
        }

        send_to_char("QEdit: unknown stage field.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "objective")) {
        long stage_id;
        long objective_id;
        long pool_entry_id;
        long pool_weight;
        WNUM target;
        QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;

        if (!str_prefix(arg3, "edit")) {
            char rewritten[MSL];

            if (IS_NULLSTR(arg4) || IS_NULLSTR(arg5) || IS_NULLSTR(arg6)) {
                send_to_char("QEdit: objective edit requires <stage_id> <objective_id> <field> ...\n\r", ch);
                return;
            }

            if (IS_NULLSTR(text_after_arg6))
                snprintf(rewritten, sizeof(rewritten), "%s objective %s %s %s", arg1, arg4, arg5, arg6);
            else
                snprintf(rewritten, sizeof(rewritten), "%s objective %s %s %s %.1800s",
                    arg1, arg4, arg5, arg6, text_after_arg6);

            do_qedit(ch, rewritten);
            return;
        }

        if (!str_prefix(arg3, "pool")) {
            if (!str_prefix(arg4, "list")) {
                if (IS_NULLSTR(arg5) || !is_number(arg5) || IS_NULLSTR(arg6) || !is_number(arg6)) {
                    send_to_char("QEdit: objective pool list requires <stage_id> <objective_id>.\n\r", ch);
                    return;
                }

                stage_id = atol(arg5);
                objective_id = atol(arg6);
                stage = qedit_find_stage(quest_index_v2, (int)stage_id);
                if (!stage) {
                    send_to_char("QEdit: stage not found.\n\r", ch);
                    return;
                }

                objective = qedit_find_objective(stage, (int)objective_id);
                if (!objective) {
                    send_to_char("QEdit: objective not found.\n\r", ch);
                    return;
                }

                if (!objective->pool_entries) {
                    send_to_char("QEdit: no pool entries for that objective.\n\r", ch);
                    return;
                }

                printf_to_char(ch, "Pool entries for objective %d (stage %d):\n\r", objective->id, stage->id);
                for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next) {
                    char target_buf[MSL];
                    qedit_format_wnum_display(ch, pool_entry->target_wnum, QEDIT_WNUM_ANY,
                        target_buf, sizeof(target_buf));
                    printf_to_char(ch, "  [%d] target:%s weight:%d\n\r",
                        pool_entry->id,
                        target_buf,
                        pool_entry->weight);
                }
                return;
            }

            if (!str_prefix(arg4, "add")) {
                char pool_target_arg[MIL];
                char pool_weight_arg[MIL];
                char pool_tail[MSL];
                char *pool_rest;

                if (IS_NULLSTR(arg5) || !is_number(arg5) || IS_NULLSTR(arg6) || !is_number(arg6)) {
                    send_to_char("QEdit: objective pool add requires <stage_id> <objective_id> <target_ref> [weight].\n\r", ch);
                    return;
                }

                stage_id = atol(arg5);
                objective_id = atol(arg6);
                stage = qedit_find_stage(quest_index_v2, (int)stage_id);
                if (!stage) {
                    send_to_char("QEdit: stage not found.\n\r", ch);
                    return;
                }

                objective = qedit_find_objective(stage, (int)objective_id);
                if (!objective) {
                    send_to_char("QEdit: objective not found.\n\r", ch);
                    return;
                }

                strncpy(pool_tail, text_after_arg6, sizeof(pool_tail) - 1);
                pool_tail[sizeof(pool_tail) - 1] = '\0';
                pool_rest = pool_tail;
                pool_rest = one_argument(pool_rest, pool_target_arg);
                pool_rest = one_argument(pool_rest, pool_weight_arg);

                if (!qedit_parse_target_wnum(ch, pool_target_arg, &target) || !target.pArea || target.vnum < 1) {
                    send_to_char("QEdit: objective pool add requires a concrete target ref after ids.\n\r", ch);
                    return;
                }

                pool_weight = 1;
                if (!IS_NULLSTR(pool_weight_arg) && is_number(pool_weight_arg))
                    pool_weight = atol(pool_weight_arg);
                if (pool_weight < 1)
                    pool_weight = 1;

                pool_entry = new_quest_objective_pool_entry_v2();
                pool_entry->id = 1;
                if (objective->pool_entries) {
                    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *tail;
                    for (tail = objective->pool_entries; tail->next != NULL; tail = tail->next)
                        ;
                    pool_entry->id = tail->id + 1;
                }

                pool_entry->weight = (int)pool_weight;
                pool_entry->target_wnum = target;
                pool_entry->target_load.auid = target.pArea->uid;
                pool_entry->target_load.vnum = target.vnum;
                qedit_append_pool_entry(objective, pool_entry);

                objective->target_mode = QUEST_OBJECTIVE_TARGET_POOL;

                printf_to_char(ch, "QEdit: added pool entry [%d] %ld#%ld weight:%d.\n\r",
                    pool_entry->id,
                    target.pArea->uid,
                    target.vnum,
                    pool_entry->weight);
                return;
            }

            if (!str_prefix(arg4, "del")) {
                char pool_entry_arg[MIL];
                char pool_tail[MSL];

                strncpy(pool_tail, text_after_arg6, sizeof(pool_tail) - 1);
                pool_tail[sizeof(pool_tail) - 1] = '\0';
                one_argument(pool_tail, pool_entry_arg);

                if (IS_NULLSTR(arg5) || !is_number(arg5) || IS_NULLSTR(arg6) || !is_number(arg6)
                    || IS_NULLSTR(pool_entry_arg) || !is_number(pool_entry_arg)) {
                    send_to_char("QEdit: objective pool del requires <stage_id> <objective_id> <entry_id>.\n\r", ch);
                    return;
                }

                stage_id = atol(arg5);
                objective_id = atol(arg6);
                pool_entry_id = atol(pool_entry_arg);

                stage = qedit_find_stage(quest_index_v2, (int)stage_id);
                if (!stage) {
                    send_to_char("QEdit: stage not found.\n\r", ch);
                    return;
                }

                objective = qedit_find_objective(stage, (int)objective_id);
                if (!objective) {
                    send_to_char("QEdit: objective not found.\n\r", ch);
                    return;
                }

                if (!qedit_remove_pool_entry(objective, (int)pool_entry_id)) {
                    send_to_char("QEdit: pool entry not found.\n\r", ch);
                    return;
                }

                printf_to_char(ch, "QEdit: objective pool entry %d removed.\n\r", (int)pool_entry_id);
                return;
            }

            send_to_char("QEdit: objective pool supports list/add/del.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "list")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: objective list requires <stage_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            stage = qedit_find_stage(quest_index_v2, (int)stage_id);
            if (!stage) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            if (!stage->objectives) {
                send_to_char("QEdit: no objectives on that stage.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Objectives for stage %d:\n\r", stage->id);
            for (objective = stage->objectives; objective != NULL; objective = objective->next) {
                char ref_buf[MIL];
                char destination_ref_buf[MIL];
                char token_ref_buf[MIL];
                char target_buf[MSL];
                char destination_buf[MSL];
                char token_buf[MSL];
                char destination_token_ref_buf[MIL];
                char destination_token_buf[MSL];
                const char *refname = !IS_NULLSTR(objective->target_variable_name) ? objective->target_variable_name : "(none)";
                const char *destination_refname = !IS_NULLSTR(objective->destination_variable_name) ? objective->destination_variable_name : "(none)";
                const char *token_refname = !IS_NULLSTR(objective->target_token_variable_name) ? objective->target_token_variable_name : "(none)";
                const char *destination_token_refname = !IS_NULLSTR(objective->destination_token_variable_name) ? objective->destination_token_variable_name : "(none)";
                const char *tag = IS_NULLSTR(objective->target_tag) ? "(none)" : objective->target_tag;

                if (!IS_NULLSTR(objective->target_ref_name))
                    snprintf(ref_buf, sizeof(ref_buf), "$%s", objective->target_ref_name);
                else if (objective->target_ref_stage_id > 0 && objective->target_ref_objective_id > 0)
                    snprintf(ref_buf, sizeof(ref_buf), "%d:%d", objective->target_ref_stage_id, objective->target_ref_objective_id);
                else
                    snprintf(ref_buf, sizeof(ref_buf), "none");

                if (!IS_NULLSTR(objective->destination_ref_name))
                    snprintf(destination_ref_buf, sizeof(destination_ref_buf), "$%s", objective->destination_ref_name);
                else
                    snprintf(destination_ref_buf, sizeof(destination_ref_buf), "none");

                if (!IS_NULLSTR(objective->target_token_ref_name))
                    snprintf(token_ref_buf, sizeof(token_ref_buf), "$%s", objective->target_token_ref_name);
                else
                    snprintf(token_ref_buf, sizeof(token_ref_buf), "none");

                if (!IS_NULLSTR(objective->destination_token_ref_name))
                    snprintf(destination_token_ref_buf, sizeof(destination_token_ref_buf), "$%s", objective->destination_token_ref_name);
                else
                    snprintf(destination_token_ref_buf, sizeof(destination_token_ref_buf), "none");

                qedit_format_wnum_display(ch, objective->target_wnum, QEDIT_WNUM_ANY,
                    target_buf, sizeof(target_buf));
                qedit_format_wnum_display(ch, objective->destination_wnum, QEDIT_WNUM_ROOM,
                    destination_buf, sizeof(destination_buf));
                qedit_format_wnum_display(ch, objective->target_token_wnum, QEDIT_WNUM_TOKEN,
                    token_buf, sizeof(token_buf));
                qedit_format_wnum_display(ch, objective->destination_token_wnum, QEDIT_WNUM_TOKEN,
                    destination_token_buf, sizeof(destination_token_buf));

                printf_to_char(ch, "  [{W%d{x] type:%s  mode:%s  optional:%s\n\r",
                    objective->id,
                    qedit_objective_type_name(objective->objective_type),
                    qedit_target_mode_name(objective->target_mode),
                    objective->optional ? "on" : "off");

                printf_to_char(ch, "      required:%d  quantity:%d  strict:%s  pool:%d  tag:%s\n\r",
                    objective->required_count,
                    objective->quantity,
                    objective->strict_target ? "on" : "off",
                    qedit_count_pool_entries(objective),
                    tag);
                if (objective->pool_entries) {
                    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
                    for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next) {
                        char pool_target_buf[MSL];
                        qedit_format_wnum_display(ch, pool_entry->target_wnum, QEDIT_WNUM_ANY,
                            pool_target_buf, sizeof(pool_target_buf));
                        printf_to_char(ch, "          pool[%d]: %s weight:%d\n\r",
                            pool_entry->id,
                            pool_target_buf,
                            pool_entry->weight);
                    }
                }

                printf_to_char(ch, "      target:%s  targetref:%s  refname:%s\n\r",
                    target_buf,
                    ref_buf,
                    refname);

                printf_to_char(ch, "      destination:%s  destinationref:%s  destinationrefname:%s\n\r",
                    destination_buf,
                    destination_ref_buf,
                    destination_refname);

                printf_to_char(ch, "      token:%s  tokenref:%s  tokenrefname:%s\n\r",
                    token_buf,
                    token_ref_buf,
                    token_refname);
                printf_to_char(ch, "      desttoken:%s  desttokenref:%s  desttokenrefname:%s\n\r",
                    destination_token_buf,
                    destination_token_ref_buf,
                    destination_token_refname);
            }
            return;
        }

        if (!str_prefix(arg3, "add")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4) || IS_NULLSTR(arg5) || !is_number(arg5)) {
                send_to_char("QEdit: objective add requires <stage_id> <objective_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            objective_id = atol(arg5);
            if (stage_id < 1 || objective_id < 1) {
                send_to_char("QEdit: ids must be >= 1.\n\r", ch);
                return;
            }

            stage = qedit_find_stage(quest_index_v2, (int)stage_id);
            if (!stage) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            if (qedit_find_objective(stage, (int)objective_id)) {
                send_to_char("QEdit: objective id already exists on that stage.\n\r", ch);
                return;
            }

            objective = new_quest_objective_index_v2();
            objective->id = (int)objective_id;
            qedit_append_objective(stage, objective);
            printf_to_char(ch, "QEdit: objective %d added to stage %d.\n\r", objective->id, stage->id);
            return;
        }

        if (!str_prefix(arg3, "del")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4) || IS_NULLSTR(arg5) || !is_number(arg5)) {
                send_to_char("QEdit: objective del requires <stage_id> <objective_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            objective_id = atol(arg5);
            if (stage_id < 1 || objective_id < 1) {
                send_to_char("QEdit: ids must be >= 1.\n\r", ch);
                return;
            }

            stage = qedit_find_stage(quest_index_v2, (int)stage_id);
            if (!stage) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            if (!qedit_remove_objective(stage, (int)objective_id)) {
                send_to_char("QEdit: objective not found.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: objective %d removed from stage %d.\n\r", (int)objective_id, stage->id);
            return;
        }

        if (!str_prefix(arg3, "show")) {
            char target_buf[MSL];
            char destination_buf[MSL];
            char token_buf[MSL];
            char destination_token_buf[MSL];

            if (IS_NULLSTR(arg4) || !is_number(arg4) || IS_NULLSTR(arg5) || !is_number(arg5)) {
                send_to_char("QEdit: objective show requires <stage_id> <objective_id>.\n\r", ch);
                return;
            }

            stage_id = atol(arg4);
            objective_id = atol(arg5);
            stage = qedit_find_stage(quest_index_v2, (int)stage_id);
            if (!stage) {
                send_to_char("QEdit: stage not found.\n\r", ch);
                return;
            }

            objective = qedit_find_objective(stage, (int)objective_id);
            if (!objective) {
                send_to_char("QEdit: objective not found.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Objective %d (stage %d)\n\r", objective->id, stage->id);
            printf_to_char(ch, "  type     : %s\n\r", qedit_objective_type_name(objective->objective_type));
            printf_to_char(ch, "  required : %d\n\r", objective->required_count);
            printf_to_char(ch, "  quantity : %d\n\r", objective->quantity);
            printf_to_char(ch, "  optional : %s\n\r", objective->optional ? "on" : "off");
            printf_to_char(ch, "  strict   : %s\n\r", objective->strict_target ? "on" : "off");
            printf_to_char(ch, "  mode     : %s\n\r", qedit_target_mode_name(objective->target_mode));
            qedit_format_wnum_display(ch, objective->target_wnum, QEDIT_WNUM_ANY,
                target_buf, sizeof(target_buf));
            qedit_format_wnum_display(ch, objective->destination_wnum, QEDIT_WNUM_ROOM,
                destination_buf, sizeof(destination_buf));
            qedit_format_wnum_display(ch, objective->target_token_wnum, QEDIT_WNUM_TOKEN,
                token_buf, sizeof(token_buf));
            qedit_format_wnum_display(ch, objective->destination_token_wnum, QEDIT_WNUM_TOKEN,
                destination_token_buf, sizeof(destination_token_buf));
            printf_to_char(ch, "  target   : %s\n\r", target_buf);
            if (!IS_NULLSTR(objective->target_ref_name))
                printf_to_char(ch, "  targetref: $%s\n\r", objective->target_ref_name);
            else
                printf_to_char(ch, "  targetref: %d:%d\n\r",
                    objective->target_ref_stage_id,
                    objective->target_ref_objective_id);
            printf_to_char(ch, "  refname  : %s\n\r",
                IS_NULLSTR(objective->target_variable_name) ? "" : objective->target_variable_name);
            printf_to_char(ch, "  destination: %s\n\r", destination_buf);
            if (!IS_NULLSTR(objective->destination_ref_name))
                printf_to_char(ch, "  destinationref: $%s\n\r", objective->destination_ref_name);
            else
                printf_to_char(ch, "  destinationref: none\n\r");
            printf_to_char(ch, "  destinationrefname: %s\n\r",
                IS_NULLSTR(objective->destination_variable_name) ? "" : objective->destination_variable_name);
            printf_to_char(ch, "  token    : %s\n\r", token_buf);
            if (!IS_NULLSTR(objective->target_token_ref_name))
                printf_to_char(ch, "  tokenref : $%s\n\r", objective->target_token_ref_name);
            else
                printf_to_char(ch, "  tokenref : none\n\r");
            printf_to_char(ch, "  tokenrefname: %s\n\r",
                IS_NULLSTR(objective->target_token_variable_name) ? "" : objective->target_token_variable_name);
            printf_to_char(ch, "  desttoken: %s\n\r", destination_token_buf);
            if (!IS_NULLSTR(objective->destination_token_ref_name))
                printf_to_char(ch, "  desttokenref: $%s\n\r", objective->destination_token_ref_name);
            else
                printf_to_char(ch, "  desttokenref: none\n\r");
            printf_to_char(ch, "  desttokenrefname: %s\n\r",
                IS_NULLSTR(objective->destination_token_variable_name) ? "" : objective->destination_token_variable_name);
            printf_to_char(ch, "  pool     : %d entries\n\r", qedit_count_pool_entries(objective));
            if (objective->pool_entries) {
                QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
                for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next) {
                    char pool_target_buf[MSL];
                    qedit_format_wnum_display(ch, pool_entry->target_wnum, QEDIT_WNUM_ANY,
                        pool_target_buf, sizeof(pool_target_buf));
                    printf_to_char(ch, "      [%d] %s weight:%d\n\r",
                        pool_entry->id,
                        pool_target_buf,
                        pool_entry->weight);
                }
            }
            printf_to_char(ch, "  summary  : %s\n\r", objective->target_tag);
            printf_to_char(ch, "  description: %s\n\r", objective->description);
            return;
        }

        if (IS_NULLSTR(arg3) || !is_number(arg3) || IS_NULLSTR(arg4) || !is_number(arg4)) {
            send_to_char("QEdit: objective edit requires <stage_id> <objective_id> <field> ...\n\r", ch);
            return;
        }

        stage_id = atol(arg3);
        objective_id = atol(arg4);
        if (stage_id < 1 || objective_id < 1) {
            send_to_char("QEdit: ids must be >= 1.\n\r", ch);
            return;
        }

        stage = qedit_find_stage(quest_index_v2, (int)stage_id);
        if (!stage) {
            send_to_char("QEdit: stage not found.\n\r", ch);
            return;
        }

        objective = qedit_find_objective(stage, (int)objective_id);
        if (!objective) {
            send_to_char("QEdit: objective not found.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(arg5)) {
            send_to_char("QEdit: objective field required (type/required/quantity/optional/strict/target/destination/token/destinationtoken/summary/description).\n\r", ch);
            send_to_char("       Use target|destination|token subfields: wnum/ref/name (and target mode|pool).\n\r", ch);
            return;
        }

        if (!str_prefix(arg5, "type")) {
            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: type requires kill/collect/talk/travel/locate/rescue/escort/custom.\n\r", ch);
                return;
            }

            if (!str_prefix(arg6, "kill"))
                objective->objective_type = QUEST_OBJECTIVE_KILL;
            else if (!str_prefix(arg6, "collect"))
                objective->objective_type = QUEST_OBJECTIVE_COLLECT;
            else if (!str_prefix(arg6, "talk"))
                objective->objective_type = QUEST_OBJECTIVE_TALK;
            else if (!str_prefix(arg6, "travel"))
                objective->objective_type = QUEST_OBJECTIVE_TRAVEL;
            else if (!str_prefix(arg6, "locate"))
                objective->objective_type = QUEST_OBJECTIVE_LOCATE;
            else if (!str_prefix(arg6, "rescue"))
                objective->objective_type = QUEST_OBJECTIVE_RESCUE;
            else if (!str_prefix(arg6, "escort"))
                objective->objective_type = QUEST_OBJECTIVE_ESCORT;
            else if (!str_prefix(arg6, "custom"))
                objective->objective_type = QUEST_OBJECTIVE_CUSTOM_SCRIPT;
            else {
                send_to_char("QEdit: unknown objective type.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: objective type set to %s.\n\r", qedit_objective_type_name(objective->objective_type));
            return;
        }

        if (!str_prefix(arg5, "required")) {
            long value;

            if (IS_NULLSTR(arg6) || !is_number(arg6)) {
                send_to_char("QEdit: required needs numeric count >= 1.\n\r", ch);
                return;
            }

            value = atol(arg6);
            if (value < 1) {
                send_to_char("QEdit: required must be >= 1.\n\r", ch);
                return;
            }

            objective->required_count = (int)value;
            printf_to_char(ch, "QEdit: objective required count set to %d.\n\r", objective->required_count);
            return;
        }

        if (!str_prefix(arg5, "quantity")) {
            long value;

            if (IS_NULLSTR(arg6) || !is_number(arg6)) {
                send_to_char("QEdit: quantity needs numeric count >= 1.\n\r", ch);
                return;
            }

            value = atol(arg6);
            if (value < 1) {
                send_to_char("QEdit: quantity must be >= 1.\n\r", ch);
                return;
            }

            objective->quantity = (int)value;
            printf_to_char(ch, "QEdit: objective quantity set to %d.\n\r", objective->quantity);
            return;
        }

        if (!str_prefix(arg5, "optional")) {
            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: optional requires on|off.\n\r", ch);
                return;
            }

            if (!str_prefix(arg6, "on") || !str_prefix(arg6, "yes") || !str_cmp(arg6, "1"))
                objective->optional = true;
            else if (!str_prefix(arg6, "off") || !str_prefix(arg6, "no") || !str_cmp(arg6, "0"))
                objective->optional = false;
            else {
                send_to_char("QEdit: optional requires on|off.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: objective optional set to %s.\n\r", objective->optional ? "on" : "off");
            return;
        }

        if (!str_prefix(arg5, "strict")) {
            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: strict requires on|off.\n\r", ch);
                return;
            }

            if (!str_prefix(arg6, "on") || !str_prefix(arg6, "yes") || !str_cmp(arg6, "1"))
                objective->strict_target = true;
            else if (!str_prefix(arg6, "off") || !str_prefix(arg6, "no") || !str_cmp(arg6, "0"))
                objective->strict_target = false;
            else {
                send_to_char("QEdit: strict requires on|off.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: objective strict set to %s.\n\r", objective->strict_target ? "on" : "off");
            return;
        }

        if (!str_cmp(arg5, "targetmode") || !str_cmp(arg5, "mode")) {
            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: targetmode requires exact|pool.\n\r", ch);
                return;
            }

            if (!str_prefix(arg6, "exact"))
                objective->target_mode = QUEST_OBJECTIVE_TARGET_EXACT;
            else if (!str_prefix(arg6, "pool"))
                objective->target_mode = QUEST_OBJECTIVE_TARGET_POOL;
            else {
                send_to_char("QEdit: unknown targetmode.\n\r", ch);
                return;
            }

            printf_to_char(ch, "QEdit: objective target mode set to %s.\n\r",
                qedit_target_mode_name(objective->target_mode));
            return;
        }

        if (!str_cmp(arg5, "target")) {
            if (!str_cmp(arg6, "mode")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: target mode requires exact|pool.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d targetmode %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "ref")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: target ref requires <stage_id>:<objective_id>, $<refname>, or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d targetref %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "name")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: target name requires <refname|none>.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d refname %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "pool")) {
                char op[MIL];
                char rewritten[MSL];
                char pool_tail[MSL];
                char *pool_rest;

                strncpy(pool_tail, text_after_arg6, sizeof(pool_tail) - 1);
                pool_tail[sizeof(pool_tail) - 1] = '\0';
                pool_rest = pool_tail;
                pool_rest = one_argument(pool_rest, op);

                if (IS_NULLSTR(op)) {
                    send_to_char("QEdit: target pool requires list|add|del.\n\r", ch);
                    return;
                }

                if (!str_cmp(op, "list"))
                    snprintf(rewritten, sizeof(rewritten), "%s objective pool list %d %d", arg1, stage->id, objective->id);
                else if (!str_cmp(op, "add"))
                    snprintf(rewritten, sizeof(rewritten), "%s objective pool add %d %d %.1800s", arg1, stage->id, objective->id, pool_rest);
                else if (!str_cmp(op, "del"))
                    snprintf(rewritten, sizeof(rewritten), "%s objective pool del %d %d %.1800s", arg1, stage->id, objective->id, pool_rest);
                else {
                    send_to_char("QEdit: target pool requires list|add|del.\n\r", ch);
                    return;
                }

                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "wnum") || !str_cmp(arg6, "direct")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: target wnum requires <auid>#<vnum> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d target %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!qedit_parse_target_wnum(ch, arg6, &target)) {
                send_to_char("QEdit: target must be <auid>#<vnum>, <vnum> (in-area), or none.\n\r", ch);
                return;
            }

            objective->target_wnum = target;
            if (target.pArea) {
                objective->target_load.auid = target.pArea->uid;
                objective->target_load.vnum = target.vnum;
            } else {
                objective->target_load.auid = 0;
                objective->target_load.vnum = 0;
            }

            printf_to_char(ch, "QEdit: objective target set to %ld#%ld.\n\r",
                target.pArea ? target.pArea->uid : 0,
                target.vnum);
            return;
        }

        if (!str_cmp(arg5, "destinationref") || !str_cmp(arg5, "destref")) {
            const char *refname = arg6;

            if (IS_NULLSTR(refname) || !str_cmp(refname, "none")) {
                free_string(objective->destination_ref_name);
                objective->destination_ref_name = str_dup("");
                send_to_char("QEdit: objective destination reference cleared.\n\r", ch);
                return;
            }

            if (refname[0] == '$')
                refname++;

            if (IS_NULLSTR(refname)) {
                send_to_char("QEdit: destinationref requires $<refname> or none.\n\r", ch);
                return;
            }

            if (!qedit_find_objective_by_refname(quest_index_v2, refname)) {
                send_to_char("QEdit: destinationref $name not found in this quest.\n\r", ch);
                return;
            }

            objective->destination_wnum = wnum_zero;
            objective->destination_load.auid = 0;
            objective->destination_load.vnum = 0;
            free_string(objective->destination_ref_name);
            objective->destination_ref_name = str_dup(refname);
            printf_to_char(ch, "QEdit: objective destination reference set to $%s.\n\r", refname);
            return;
        }

        if (!str_cmp(arg5, "destination") || !str_cmp(arg5, "dest")) {
            if (!str_cmp(arg6, "token")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination token requires wnum|ref|name subcommands.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationtoken %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "ref")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination ref requires $<refname> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationref %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "name")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination name requires <refname|none>.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationrefname %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "wnum") || !str_cmp(arg6, "direct")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination wnum requires <auid>#<vnum> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destination %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            const char *refname = NULL;

            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: destination requires <auid>#<vnum>, <vnum>, $<refname>, or none.\n\r", ch);
                return;
            }

            if (!str_cmp(arg6, "none")) {
                objective->destination_wnum = wnum_zero;
                objective->destination_load.auid = 0;
                objective->destination_load.vnum = 0;
                free_string(objective->destination_ref_name);
                objective->destination_ref_name = str_dup("");
                send_to_char("QEdit: objective destination cleared.\n\r", ch);
                return;
            }

            if (arg6[0] == '$') {
                refname = arg6 + 1;
                if (IS_NULLSTR(refname)) {
                    send_to_char("QEdit: destination $name requires a reference name.\n\r", ch);
                    return;
                }

                if (!qedit_find_objective_by_refname(quest_index_v2, refname)) {
                    send_to_char("QEdit: destination $name not found in this quest.\n\r", ch);
                    return;
                }

                objective->destination_wnum = wnum_zero;
                objective->destination_load.auid = 0;
                objective->destination_load.vnum = 0;
                free_string(objective->destination_ref_name);
                objective->destination_ref_name = str_dup(refname);
                printf_to_char(ch, "QEdit: objective destination reference set to $%s.\n\r", refname);
                return;
            }

            if (!qedit_parse_target_wnum(ch, arg6, &target)) {
                send_to_char("QEdit: destination must be <auid>#<vnum>, <vnum> (in-area), $<refname>, or none.\n\r", ch);
                return;
            }

            objective->destination_wnum = target;
            if (target.pArea) {
                objective->destination_load.auid = target.pArea->uid;
                objective->destination_load.vnum = target.vnum;
            } else {
                objective->destination_load.auid = 0;
                objective->destination_load.vnum = 0;
            }

            free_string(objective->destination_ref_name);
            objective->destination_ref_name = str_dup("");

            printf_to_char(ch, "QEdit: objective destination set to %ld#%ld.\n\r",
                target.pArea ? target.pArea->uid : 0,
                target.vnum);
            return;
        }

        if (!str_cmp(arg5, "destinationtokenref") || !str_cmp(arg5, "desttokenref")) {
            const char *refname = arg6;

            if (IS_NULLSTR(refname) || !str_cmp(refname, "none")) {
                free_string(objective->destination_token_ref_name);
                objective->destination_token_ref_name = str_dup("");
                send_to_char("QEdit: objective destination token reference cleared.\n\r", ch);
                return;
            }

            if (refname[0] == '$')
                refname++;

            if (IS_NULLSTR(refname)) {
                send_to_char("QEdit: destinationtokenref requires $<refname> or none.\n\r", ch);
                return;
            }

            if (!qedit_find_objective_by_refname(quest_index_v2, refname)) {
                send_to_char("QEdit: destinationtokenref $name not found in this quest.\n\r", ch);
                return;
            }

            objective->destination_token_wnum = wnum_zero;
            objective->destination_token_load.auid = 0;
            objective->destination_token_load.vnum = 0;
            free_string(objective->destination_token_ref_name);
            objective->destination_token_ref_name = str_dup(refname);
            printf_to_char(ch, "QEdit: objective destination token reference set to $%s.\n\r", refname);
            return;
        }

        if (!str_prefix(arg5, "destinationtokenrefname") || !str_prefix(arg5, "desttokenname") || !str_prefix(arg5, "desttokenvar")) {
            const char *name = arg6;
            QUEST_OBJECTIVE_INDEX_V2_DATA *existing;

            if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
                free_string(objective->destination_token_variable_name);
                objective->destination_token_variable_name = str_dup("");
                send_to_char("QEdit: objective destination token refname cleared.\n\r", ch);
                return;
            }

            if (name[0] == '$')
                name++;

            if (IS_NULLSTR(name)) {
                send_to_char("QEdit: destination token refname requires a name.\n\r", ch);
                return;
            }

            existing = qedit_find_objective_by_refname(quest_index_v2, name);
            if (existing && existing != objective) {
                send_to_char("QEdit: refname must be unique within the quest.\n\r", ch);
                return;
            }

            free_string(objective->destination_token_variable_name);
            objective->destination_token_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective destination token refname set to %s.\n\r", objective->destination_token_variable_name);
            return;
        }

        if (!str_cmp(arg5, "destinationtoken") || !str_cmp(arg5, "desttoken")) {
            if (!str_cmp(arg6, "ref")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination token ref requires $<refname> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationtokenref %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "name")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination token name requires <refname|none>.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationtokenrefname %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "wnum") || !str_cmp(arg6, "direct")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: destination token wnum requires <auid>#<vnum> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d destinationtoken %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (IS_NULLSTR(arg6) || !str_cmp(arg6, "none")) {
                objective->destination_token_wnum = wnum_zero;
                objective->destination_token_load.auid = 0;
                objective->destination_token_load.vnum = 0;
                free_string(objective->destination_token_ref_name);
                objective->destination_token_ref_name = str_dup("");
                send_to_char("QEdit: objective destination token cleared.\n\r", ch);
                return;
            }

            if (arg6[0] == '$') {
                const char *refname = arg6 + 1;
                if (IS_NULLSTR(refname)) {
                    send_to_char("QEdit: destination token $name requires a reference name.\n\r", ch);
                    return;
                }

                if (!qedit_find_objective_by_refname(quest_index_v2, refname)) {
                    send_to_char("QEdit: destination token $name not found in this quest.\n\r", ch);
                    return;
                }

                objective->destination_token_wnum = wnum_zero;
                objective->destination_token_load.auid = 0;
                objective->destination_token_load.vnum = 0;
                free_string(objective->destination_token_ref_name);
                objective->destination_token_ref_name = str_dup(refname);
                printf_to_char(ch, "QEdit: objective destination token reference set to $%s.\n\r", refname);
                return;
            }

            if (!qedit_parse_target_wnum(ch, arg6, &target)) {
                send_to_char("QEdit: destination token must be <auid>#<vnum>, <vnum> (in-area), $<refname>, or none.\n\r", ch);
                return;
            }

            objective->destination_token_wnum = target;
            if (target.pArea) {
                objective->destination_token_load.auid = target.pArea->uid;
                objective->destination_token_load.vnum = target.vnum;
            } else {
                objective->destination_token_load.auid = 0;
                objective->destination_token_load.vnum = 0;
            }

            printf_to_char(ch, "QEdit: objective destination token set to %ld#%ld.\n\r",
                target.pArea ? target.pArea->uid : 0,
                target.vnum);
            return;
        }

        if (!str_cmp(arg5, "targetref") || !str_cmp(arg5, "ref")) {
            char ref[MIL];
            char *sep;
            long ref_stage_id;
            long ref_objective_id;
            QUEST_STAGE_INDEX_V2_DATA *ref_stage;
            QUEST_OBJECTIVE_INDEX_V2_DATA *ref_objective;
            const char *refname;

            if (IS_NULLSTR(arg6)) {
                send_to_char("QEdit: targetref requires <stage_id>:<objective_id> or none.\n\r", ch);
                return;
            }

            if (!str_cmp(arg6, "none")) {
                objective->target_ref_stage_id = 0;
                objective->target_ref_objective_id = 0;
                free_string(objective->target_ref_name);
                objective->target_ref_name = str_dup("");
                send_to_char("QEdit: objective target reference cleared.\n\r", ch);
                return;
            }

            if (arg6[0] == '$') {
                QUEST_OBJECTIVE_INDEX_V2_DATA *ref_by_name;

                refname = arg6 + 1;
                if (IS_NULLSTR(refname)) {
                    send_to_char("QEdit: targetref $name requires a variable name.\n\r", ch);
                    return;
                }

                ref_by_name = qedit_find_objective_by_refname(quest_index_v2, refname);
                if (!ref_by_name) {
                    send_to_char("QEdit: targetref $name not found in this quest.\n\r", ch);
                    return;
                }

                if (ref_by_name == objective) {
                    send_to_char("QEdit: objective cannot targetref itself.\n\r", ch);
                    return;
                }

                objective->target_ref_stage_id = 0;
                objective->target_ref_objective_id = 0;
                free_string(objective->target_ref_name);
                objective->target_ref_name = str_dup(refname);
                printf_to_char(ch, "QEdit: objective target reference set to $%s.\n\r", refname);
                return;
            }

            strncpy(ref, arg6, sizeof(ref) - 1);
            ref[sizeof(ref) - 1] = '\0';

            sep = strchr(ref, ':');
            if (!sep)
                sep = strchr(ref, '#');
            if (!sep) {
                send_to_char("QEdit: targetref requires <stage_id>:<objective_id>.\n\r", ch);
                return;
            }

            *sep++ = '\0';
            if (!is_number(ref) || !is_number(sep)) {
                send_to_char("QEdit: targetref requires numeric stage/objective ids.\n\r", ch);
                return;
            }

            ref_stage_id = atol(ref);
            ref_objective_id = atol(sep);
            if (ref_stage_id < 1 || ref_objective_id < 1) {
                send_to_char("QEdit: targetref ids must be >= 1.\n\r", ch);
                return;
            }

            ref_stage = qedit_find_stage(quest_index_v2, (int)ref_stage_id);
            if (!ref_stage) {
                send_to_char("QEdit: targetref stage not found.\n\r", ch);
                return;
            }

            ref_objective = qedit_find_objective(ref_stage, (int)ref_objective_id);
            if (!ref_objective) {
                send_to_char("QEdit: targetref objective not found.\n\r", ch);
                return;
            }

            if (ref_stage_id == stage->id && ref_objective_id == objective->id) {
                send_to_char("QEdit: objective cannot targetref itself.\n\r", ch);
                return;
            }

            objective->target_ref_stage_id = (int)ref_stage_id;
            objective->target_ref_objective_id = (int)ref_objective_id;
            free_string(objective->target_ref_name);
            objective->target_ref_name = str_dup("");
            printf_to_char(ch, "QEdit: objective target reference set to %d:%d.\n\r",
                objective->target_ref_stage_id,
                objective->target_ref_objective_id);
            return;
        }

        if (!str_prefix(arg5, "variable") || !str_prefix(arg5, "var") || !str_prefix(arg5, "refname") || !str_prefix(arg5, "targetrefname")) {
            const char *name = arg6;
            QUEST_OBJECTIVE_INDEX_V2_DATA *existing;

            if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
                free_string(objective->target_variable_name);
                objective->target_variable_name = str_dup("");
                send_to_char("QEdit: objective target refname cleared.\n\r", ch);
                return;
            }

            if (name[0] == '$')
                name++;

            if (IS_NULLSTR(name)) {
                send_to_char("QEdit: variable requires a name.\n\r", ch);
                return;
            }

            existing = qedit_find_objective_by_refname(quest_index_v2, name);
            if (existing && existing != objective) {
                send_to_char("QEdit: refname must be unique within the quest.\n\r", ch);
                return;
            }

            free_string(objective->target_variable_name);
            objective->target_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective target refname set to %s.\n\r", objective->target_variable_name);
            return;
        }

        if (!str_prefix(arg5, "destinationrefname") || !str_prefix(arg5, "destrefname") || !str_prefix(arg5, "destvar")) {
            const char *name = arg6;
            QUEST_OBJECTIVE_INDEX_V2_DATA *existing;

            if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
                free_string(objective->destination_variable_name);
                objective->destination_variable_name = str_dup("");
                send_to_char("QEdit: objective destination refname cleared.\n\r", ch);
                return;
            }

            if (name[0] == '$')
                name++;

            if (IS_NULLSTR(name)) {
                send_to_char("QEdit: destinationrefname requires a name.\n\r", ch);
                return;
            }

            existing = qedit_find_objective_by_refname(quest_index_v2, name);
            if (existing && existing != objective) {
                send_to_char("QEdit: refname must be unique within the quest.\n\r", ch);
                return;
            }

            free_string(objective->destination_variable_name);
            objective->destination_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective destination refname set to %s.\n\r", objective->destination_variable_name);
            return;
        }

        if (!str_cmp(arg5, "targettokenref")) {
            const char *refname = arg6;

            if (IS_NULLSTR(refname) || !str_cmp(refname, "none")) {
                free_string(objective->target_token_ref_name);
                objective->target_token_ref_name = str_dup("");
                send_to_char("QEdit: objective token reference cleared.\n\r", ch);
                return;
            }

            if (refname[0] == '$')
                refname++;

            if (IS_NULLSTR(refname)) {
                send_to_char("QEdit: targettokenref requires $<refname> or none.\n\r", ch);
                return;
            }

            if (!qedit_find_objective_by_refname(quest_index_v2, refname)) {
                send_to_char("QEdit: targettokenref $name not found in this quest.\n\r", ch);
                return;
            }

            objective->target_token_wnum = wnum_zero;
            objective->target_token_load.auid = 0;
            objective->target_token_load.vnum = 0;
            free_string(objective->target_token_ref_name);
            objective->target_token_ref_name = str_dup(refname);
            printf_to_char(ch, "QEdit: objective token reference set to $%s.\n\r", refname);
            return;
        }

        if (!str_prefix(arg5, "targettokenrefname") || !str_prefix(arg5, "tokenrefname") || !str_prefix(arg5, "tokenvar")) {
            const char *name = arg6;
            QUEST_OBJECTIVE_INDEX_V2_DATA *existing;

            if (IS_NULLSTR(name) || !str_cmp(name, "none")) {
                free_string(objective->target_token_variable_name);
                objective->target_token_variable_name = str_dup("");
                send_to_char("QEdit: objective token refname cleared.\n\r", ch);
                return;
            }

            if (name[0] == '$')
                name++;

            if (IS_NULLSTR(name)) {
                send_to_char("QEdit: targettokenrefname requires a name.\n\r", ch);
                return;
            }

            existing = qedit_find_objective_by_refname(quest_index_v2, name);
            if (existing && existing != objective) {
                send_to_char("QEdit: refname must be unique within the quest.\n\r", ch);
                return;
            }

            free_string(objective->target_token_variable_name);
            objective->target_token_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective token refname set to %s.\n\r", objective->target_token_variable_name);
            return;
        }

        if (!str_cmp(arg5, "targettoken") || !str_cmp(arg5, "token")) {
            if (!str_cmp(arg6, "ref")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: token ref requires $<refname> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d targettokenref %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "name")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: token name requires <refname|none>.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d targettokenrefname %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!str_cmp(arg6, "wnum") || !str_cmp(arg6, "direct")) {
                char rewritten[MSL];

                if (IS_NULLSTR(text_after_arg6)) {
                    send_to_char("QEdit: token wnum requires <auid>#<vnum> or none.\n\r", ch);
                    return;
                }

                snprintf(rewritten, sizeof(rewritten), "%s objective %d %d targettoken %.1800s",
                    arg1, stage->id, objective->id, text_after_arg6);
                do_qedit(ch, rewritten);
                return;
            }

            if (!qedit_parse_target_wnum(ch, arg6, &target)) {
                send_to_char("QEdit: targettoken must be <auid>#<vnum>, <vnum> (in-area), or none.\n\r", ch);
                return;
            }

            objective->target_token_wnum = target;
            if (target.pArea) {
                objective->target_token_load.auid = target.pArea->uid;
                objective->target_token_load.vnum = target.vnum;
            } else {
                objective->target_token_load.auid = 0;
                objective->target_token_load.vnum = 0;
            }

            printf_to_char(ch, "QEdit: objective target token set to %ld#%ld.\n\r",
                target.pArea ? target.pArea->uid : 0,
                target.vnum);
            return;
        }

        if (!str_prefix(arg5, "tag") || !str_prefix(arg5, "summary")) {
            if (IS_NULLSTR(text_after_arg5) || !str_cmp(text_after_arg5, "none")) {
                free_string(objective->target_tag);
                objective->target_tag = str_dup("");
                send_to_char("QEdit: objective summary cleared.\n\r", ch);
                return;
            }

            free_string(objective->target_tag);
            objective->target_tag = str_dup(text_after_arg5);
            send_to_char("QEdit: objective summary updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg5, "desc") || !str_prefix(arg5, "description")) {
            if (!IS_NULLSTR(arg6) && !str_cmp(arg6, "none")) {
                free_string(objective->description);
                objective->description = str_dup("");
                send_to_char("QEdit: objective description cleared.\n\r", ch);
                return;
            }

            if (!IS_NULLSTR(arg6)) {
                send_to_char("QEdit: objective description opens the string editor; use 'description' (or 'description none').\n\r", ch);
                return;
            }

            send_to_char("QEdit: editing objective description.\n\r", ch);
            string_append(ch, &objective->description);
            return;
        }

        send_to_char("QEdit: unknown objective field.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "reward")) {
        long reward_index;
        long value;
        int reward_type;
        QUEST_REWARD_INDEX_V2_DATA *reward;
        QUEST_REWARD_INDEX_V2_DATA *reward_prev;
        QUEST_REWARD_INDEX_V2_DATA *tail;
        WNUM target;

        if (!str_prefix(arg3, "list")) {
            int index = 1;

            if (!quest_index_v2->rewards) {
                send_to_char("QEdit: no rewards on this quest index.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Rewards for %ld#%ld:\n\r",
                quest_index_v2->area ? quest_index_v2->area->uid : 0,
                quest_index_v2->vnum);

            for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next) {
                char target_buf[MSL];
                qedit_format_wnum_display(ch, reward->target_wnum, QEDIT_WNUM_ANY,
                    target_buf, sizeof(target_buf));
                printf_to_char(ch, "  [%d] type:%s amount:%ld target:%s\n\r",
                    index++,
                    qedit_reward_type_name(reward->reward_type),
                    reward->amount,
                    target_buf);
                if (!IS_NULLSTR(reward->currency))
                    printf_to_char(ch, "      currency:%s\n\r", reward->currency);
                if (!IS_NULLSTR(reward->script))
                    printf_to_char(ch, "      script:%s\n\r", reward->script);
            }
            return;
        }

        if (!str_prefix(arg3, "add")) {
            reward = new_quest_reward_index_v2();
            if (!reward) {
                send_to_char("QEdit: failed to allocate reward.\n\r", ch);
                return;
            }

            if (!IS_NULLSTR(arg4)) {
                if (!qedit_parse_reward_type(arg4, &reward_type)) {
                    free_quest_reward_index_v2(reward);
                    send_to_char("QEdit: reward add type must be points|currency|reputation|token|item|script.\n\r", ch);
                    return;
                }
                reward->reward_type = reward_type;
            }

            reward->next = NULL;
            if (!quest_index_v2->rewards)
                quest_index_v2->rewards = reward;
            else {
                for (tail = quest_index_v2->rewards; tail->next != NULL; tail = tail->next)
                    ;
                tail->next = reward;
            }

            send_to_char("QEdit: reward added.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "del")) {
            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: reward del requires <index>.\n\r", ch);
                return;
            }

            reward_index = atol(arg4);
            reward = qedit_get_reward_by_index(quest_index_v2, (int)reward_index, &reward_prev);
            if (!reward) {
                send_to_char("QEdit: reward index not found.\n\r", ch);
                return;
            }

            if (reward_prev)
                reward_prev->next = reward->next;
            else
                quest_index_v2->rewards = reward->next;

            reward->next = NULL;
            free_quest_reward_index_v2(reward);
            printf_to_char(ch, "QEdit: reward %ld removed.\n\r", reward_index);
            return;
        }

        if (!str_prefix(arg3, "show")) {
            char target_buf[MSL];

            if (IS_NULLSTR(arg4) || !is_number(arg4)) {
                send_to_char("QEdit: reward show requires <index>.\n\r", ch);
                return;
            }

            reward_index = atol(arg4);
            reward = qedit_get_reward_by_index(quest_index_v2, (int)reward_index, NULL);
            if (!reward) {
                send_to_char("QEdit: reward index not found.\n\r", ch);
                return;
            }

            printf_to_char(ch, "Reward %ld\n\r", reward_index);
            printf_to_char(ch, "  type     : %s\n\r", qedit_reward_type_name(reward->reward_type));
            printf_to_char(ch, "  amount   : %ld\n\r", reward->amount);
            qedit_format_wnum_display(ch, reward->target_wnum, QEDIT_WNUM_ANY,
                target_buf, sizeof(target_buf));
            printf_to_char(ch, "  target   : %s\n\r", target_buf);
            printf_to_char(ch, "  currency : %s\n\r", IS_NULLSTR(reward->currency) ? "" : reward->currency);
            printf_to_char(ch, "  script   : %s\n\r", IS_NULLSTR(reward->script) ? "" : reward->script);
            return;
        }

        if (!str_prefix(arg3, "edit")) {
            char rewritten[MSL];

            if (IS_NULLSTR(arg4) || IS_NULLSTR(arg5)) {
                send_to_char("QEdit: reward edit requires <index> <field> ...\n\r", ch);
                return;
            }

            if (IS_NULLSTR(text_after_arg5))
                snprintf(rewritten, sizeof(rewritten), "%s reward %s %s", arg1, arg4, arg5);
            else
                snprintf(rewritten, sizeof(rewritten), "%s reward %s %s %.1800s", arg1, arg4, arg5, text_after_arg5);

            do_qedit(ch, rewritten);
            return;
        }

        if (IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("QEdit: reward command requires list/add/del/show or <index> <field> ...\n\r", ch);
            return;
        }

        reward_index = atol(arg3);
        if (reward_index < 1) {
            send_to_char("QEdit: reward index must be >= 1.\n\r", ch);
            return;
        }

        reward = qedit_get_reward_by_index(quest_index_v2, (int)reward_index, NULL);
        if (!reward) {
            send_to_char("QEdit: reward index not found.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(arg4)) {
            send_to_char("QEdit: reward field required (type/amount/target/currency/script).\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "type")) {
            if (!qedit_parse_reward_type(arg5, &reward_type)) {
                send_to_char("QEdit: reward type must be points|currency|reputation|token|item|script.\n\r", ch);
                return;
            }

            reward->reward_type = reward_type;
            printf_to_char(ch, "QEdit: reward %ld type set to %s.\n\r",
                reward_index, qedit_reward_type_name(reward->reward_type));
            return;
        }

        if (!str_prefix(arg4, "amount")) {
            if (IS_NULLSTR(arg5) || !is_number(arg5)) {
                send_to_char("QEdit: reward amount requires a numeric value.\n\r", ch);
                return;
            }

            value = atol(arg5);
            reward->amount = value;
            printf_to_char(ch, "QEdit: reward %ld amount set to %ld.\n\r", reward_index, reward->amount);
            return;
        }

        if (!str_prefix(arg4, "target")) {
            if (!qedit_parse_target_wnum(ch, arg5, &target)) {
                send_to_char("QEdit: reward target must be <auid>#<vnum>, <vnum> (in-area), or none.\n\r", ch);
                return;
            }

            reward->target_wnum = target;
            if (target.pArea) {
                reward->target_load.auid = target.pArea->uid;
                reward->target_load.vnum = target.vnum;
            } else {
                reward->target_load.auid = 0;
                reward->target_load.vnum = 0;
            }

            printf_to_char(ch, "QEdit: reward %ld target set to %ld#%ld.\n\r",
                reward_index,
                target.pArea ? target.pArea->uid : 0,
                target.vnum);
            return;
        }

        if (!str_prefix(arg4, "currency")) {
            if (IS_NULLSTR(text_after_arg4) || !str_cmp(text_after_arg4, "none")) {
                free_string(reward->currency);
                reward->currency = str_dup("");
                printf_to_char(ch, "QEdit: reward %ld currency cleared.\n\r", reward_index);
                return;
            }

            free_string(reward->currency);
            reward->currency = str_dup(text_after_arg4);
            printf_to_char(ch, "QEdit: reward %ld currency updated.\n\r", reward_index);
            return;
        }

        if (!str_prefix(arg4, "script")) {
            if (IS_NULLSTR(text_after_arg4) || !str_cmp(text_after_arg4, "none")) {
                free_string(reward->script);
                reward->script = str_dup("");
                printf_to_char(ch, "QEdit: reward %ld script cleared.\n\r", reward_index);
                return;
            }

            free_string(reward->script);
            reward->script = str_dup(text_after_arg4);
            printf_to_char(ch, "QEdit: reward %ld script updated.\n\r", reward_index);
            return;
        }

        send_to_char("QEdit: unknown reward field.\n\r", ch);
        return;
    }

    if (!qedit_internal_dispatch && !IS_NULLSTR(original_input)) {
        interpret(ch, original_input);
        return;
    }

    send_to_char("QEdit: Unknown subcommand. In [QEdit], use subcommands like SHOW/STAGE/OBJECTIVE/TAB/COMMANDS (no leading 'qedit').\n\r", ch);
}
