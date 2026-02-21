#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../merc.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../common.h"
#include "../common/olc_editor.h"

static const OLC_EDITOR_TABS qedit_tabs = {
    .tab_count = 5,
    .tabs = {
        { "General", "Gen" },
        { "Stages", "Stg" },
        { "Objectives", "Obj" },
        { "Rewards", "Rwd" },
        { "Commands", "Cmd" },
    }
};

void do_qedit(CHAR_DATA *ch, char *argument);

static const char *qedit_mode_name(int mode)
{
    switch (mode) {
    case QUEST_MODE_NARRATIVE: return "narrative";
    case QUEST_MODE_TEMPLATE:  return "template";
    case QUEST_MODE_HYBRID:    return "hybrid";
    default:                   return "unknown";
    }
}

static const char *qedit_category_name(int category)
{
    switch (category) {
    case QUEST_CATEGORY_FULL:    return "full";
    case QUEST_CATEGORY_MISSION: return "mission";
    default:                     return "unknown";
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
                || (!IS_NULLSTR(objective->target_token_variable_name) && !str_cmp(objective->target_token_variable_name, refname)))
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

    return false;
}

static void qedit_mark_area_dirty(QUEST_INDEX_V2_DATA *quest_index_v2)
{
    if (!quest_index_v2 || !quest_index_v2->area)
        return;

    SET_BIT(quest_index_v2->area->area_flags, AREA_CHANGED);
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

void qedit(CHAR_DATA *ch, char *argument)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    char command[MSL];
    bool readonly_command;

    if (!ch || !ch->desc)
        return;

    if (!str_prefix(argument, "done"))
    {
        edit_done(ch);
        return;
    }

    quest_index_v2 = (QUEST_INDEX_V2_DATA *)ch->desc->pEdit;
    if (!quest_index_v2 || !quest_index_v2->area || quest_index_v2->vnum < 1)
    {
        send_to_char("QEdit: no active quest editor context.\n\r", ch);
        edit_done(ch);
        return;
    }

    if (IS_NULLSTR(argument))
        snprintf(command, sizeof(command), "%ld#%ld show", quest_index_v2->area->uid, quest_index_v2->vnum);
    else
        snprintf(command, sizeof(command), "%ld#%ld %.1900s", quest_index_v2->area->uid, quest_index_v2->vnum, argument);

    readonly_command = qedit_is_readonly_session_command(argument);
    do_qedit(ch, command);

    if (!readonly_command)
        qedit_mark_area_dirty(quest_index_v2);
}

static void qedit_render_show(CHAR_DATA *ch, QUEST_INDEX_V2_DATA *quest_index_v2)
{
    OLC_LAYOUT_CTX *ctx;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    int tab = 0;
    bool show_all_tabs = false;
    int stage_count = 0;
    int objective_count = 0;
    int reward_count = 0;
    int type_counts[8] = {0};

    if (!ch || !quest_index_v2)
        return;

    ctx = olc_layout_new(ch);
    if (!ctx)
        return;

    if (ch->desc)
    {
        tab = ch->desc->nEditTab;
        if (tab < 0)
            tab = 0;
        if (tab >= qedit_tabs.tab_count)
            tab = qedit_tabs.tab_count - 1;
        ch->desc->nEditTab = tab;
    }

    show_all_tabs = olc_show_all_tabs_mode(ch);

    if (!show_all_tabs)
        olc_render_tabs(ctx, &qedit_tabs);

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
    {
        stage_count++;
        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            objective_count++;
            if (objective->objective_type >= 0 && objective->objective_type < (int)elementsof(type_counts))
                type_counts[objective->objective_type]++;
        }
    }

    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
        reward_count++;

    if (show_all_tabs || tab == 0)
    {
        olc_render_section(ctx, "General");
        add_buf(ctx->buffer, formatf("{YQuest:{x %ld#%ld (%s)\n\r",
            quest_index_v2->area ? quest_index_v2->area->uid : 0,
            quest_index_v2->vnum,
            quest_index_v2->area ? quest_index_v2->area->name : "no-area"));
        add_buf(ctx->buffer, formatf("{YName:{x %s\n\r", quest_index_v2->name));
        add_buf(ctx->buffer, formatf("{YMode:{x %s  {YCategory:{x %s  {YScope:{x %s\n\r",
            qedit_mode_name(quest_index_v2->quest_mode),
            qedit_category_name(quest_index_v2->category),
            qedit_scope_name(quest_index_v2->target_scope)));
        add_buf(ctx->buffer, formatf("{YRepeat:{x %s  {YAllowance:{x %d  {YEntry:{x %d\n\r",
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

    if (show_all_tabs || tab == 1)
    {
        olc_render_section(ctx, "Stages");
        if (!quest_index_v2->stages)
            add_buf(ctx->buffer, "{DNo stages defined.{x\n\r");
        else
        {
            for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
            {
                int stage_objective_count = 0;
                for (objective = stage->objectives; objective != NULL; objective = objective->next)
                    stage_objective_count++;

                add_buf(ctx->buffer, formatf("[{W%d{x] %s  source:%s complete:%s next:%d objectives:%d\n\r",
                    stage->id,
                    IS_NULLSTR(stage->name) ? "(unnamed)" : stage->name,
                    qedit_stage_source_name(stage->stage_source),
                    qedit_stage_completion_name(stage->completion_mode),
                    stage->next_stage_id,
                    stage_objective_count));
            }
        }
    }

    if (show_all_tabs || tab == 2)
    {
        olc_render_section(ctx, "Objectives");
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
        add_buf(ctx->buffer, "Use {Wobjective list <stage_id>{x and {Wobjective show <stage_id> <objective_id>{x for full detail.\n\r");
    }

    if (show_all_tabs || tab == 3)
    {
        int reward_index = 1;

        olc_render_section(ctx, "Rewards");
        if (!quest_index_v2->rewards)
            add_buf(ctx->buffer, "{DNo rewards defined yet.{x\n\r");
        else
        {
            for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
            {
                add_buf(ctx->buffer, formatf("[{W%d{x] type:%d amount:%ld target:%ld#%ld currency:%s script:%s\n\r",
                    reward_index++,
                    reward->reward_type,
                    reward->amount,
                    reward->target_wnum.pArea ? reward->target_wnum.pArea->uid : 0,
                    reward->target_wnum.vnum,
                    IS_NULLSTR(reward->currency) ? "" : reward->currency,
                    IS_NULLSTR(reward->script) ? "" : reward->script));
            }
        }
    }

    if (show_all_tabs || tab == 4)
    {
        olc_render_section(ctx, "Commands");
        add_buf(ctx->buffer, "{WTab navigation:{x qedit <ref> tab <name|number>\n\r");
        add_buf(ctx->buffer, "{WQuick views:{x qedit <ref> show | stage list | objective list <stage_id>\n\r");
        add_buf(ctx->buffer, "{WObjective refs:{x targetref/refname, destinationref/destinationrefname, targettokenref/targettokenrefname\n\r");
        add_buf(ctx->buffer, "{WTip:{x use plain {Wqedit{x with no args for full syntax.\n\r");
    }

    send_to_char(ctx->buffer->string, ch);
    olc_layout_free(ctx);
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

    if (!ch || !ch->pcdata || IS_NPC(ch))
        return;

    if (!IS_IMPLEMENTOR(ch)) {
        send_to_char("QEdit: Insufficient security.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  qedit <auid>#<vnum> create\n\r", ch);
        send_to_char("  qedit <auid>#<vnum>|<vnum> show [all]\n\r", ch);
        send_to_char("  qedit <ref> tab <name|number>\n\r", ch);
        send_to_char("  qedit <ref> mode <narrative|template|hybrid>\n\r", ch);
        send_to_char("  qedit <ref> category <full|mission>\n\r", ch);
        send_to_char("  qedit <ref> scope <character|group|church>\n\r", ch);
        send_to_char("  qedit <ref> repeat <once|repeatable>\n\r", ch);
        send_to_char("  qedit <ref> allowance <cost>\n\r", ch);
        send_to_char("  qedit <ref> entry <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> enabled <on|off>\n\r", ch);
        send_to_char("  qedit <ref> seedpolicy <auto|fixed>\n\r", ch);
        send_to_char("  qedit <ref> seed <number|none>\n\r", ch);
        send_to_char("  qedit <ref> stage list\n\r", ch);
        send_to_char("  qedit <ref> stage add <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage del <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage show <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> source <static|generated>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> autocommence <on|off>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> complete <all|any|custom>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> next <stage_id|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> profile <text|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> salt <number|none>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> name <text>\n\r", ch);
        send_to_char("  qedit <ref> stage <stage_id> desc <text>\n\r", ch);
        send_to_char("  qedit <ref> objective list <stage_id>\n\r", ch);
        send_to_char("  qedit <ref> objective add <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective del <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective show <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> type <kill|collect|talk|travel|locate|rescue|escort|custom>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> required <count>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> quantity <count>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> optional <on|off>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> targetmode <exact|pool>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> target <auid>#<vnum|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> targetref <stage_id>:<objective_id|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> refname <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destination <auid>#<vnum|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destinationref <$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> destinationrefname <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> targettoken <auid>#<vnum|$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> targettokenref <$refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> targettokenrefname <refname|none>\n\r", ch);
        send_to_char("  qedit <ref> objective pool list <stage_id> <objective_id>\n\r", ch);
        send_to_char("  qedit <ref> objective pool add <stage_id> <objective_id> <target_ref> [weight]\n\r", ch);
        send_to_char("  qedit <ref> objective pool del <stage_id> <objective_id> <entry_id>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> tag <text|none>\n\r", ch);
        send_to_char("  qedit <ref> objective <stage_id> <objective_id> desc <text>\n\r", ch);
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

        olc_set_editor(ch, ED_QUEST, quest_index_v2);
        qedit(ch, "show");
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

        olc_set_editor(ch, ED_QUEST, quest_index_v2);
        qedit(ch, "show");
        return;
    }

    if (!quest_index_v2) {
        send_to_char("QEdit: Quest index not found. Use CREATE first.\n\r", ch);
        return;
    }

    olc_set_editor(ch, ED_QUEST, quest_index_v2);

    if (IS_NULLSTR(arg2)) {
        qedit(ch, "show");
        return;
    }

    if (!str_prefix(arg2, "tab") || (!IS_NULLSTR(arg2) && is_number(arg2) && IS_NULLSTR(arg3))) {
        char tab_arg[MIL];

        if (!str_prefix(arg2, "tab")) {
            if (IS_NULLSTR(arg3)) {
                send_to_char("QEdit: tab requires a tab name or number.\n\r", ch);
                return;
            }
            snprintf(tab_arg, sizeof(tab_arg), "tab %.500s", arg3);
        }
        else
        {
            snprintf(tab_arg, sizeof(tab_arg), "%s", arg2);
        }

        if (olc_tab_switch(ch, tab_arg, &qedit_tabs)) {
            qedit_render_show(ch, quest_index_v2);
            return;
        }

        send_to_char("QEdit: invalid tab selection.\n\r", ch);
        return;
    }

    if (IS_NULLSTR(arg2) || !str_prefix(arg2, "show")) {
        bool old_show_all_tabs = ch->desc ? ch->desc->olc_show_all_tabs : false;

        if (ch->desc && !str_prefix(arg3, "all"))
            ch->desc->olc_show_all_tabs = true;

        qedit_render_show(ch, quest_index_v2);

        if (ch->desc)
            ch->desc->olc_show_all_tabs = old_show_all_tabs;
        return;
    }

    if (!str_prefix(arg2, "mode")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: mode requires narrative, template, or hybrid.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "narrative"))
            quest_index_v2->quest_mode = QUEST_MODE_NARRATIVE;
        else if (!str_prefix(arg3, "template"))
            quest_index_v2->quest_mode = QUEST_MODE_TEMPLATE;
        else if (!str_prefix(arg3, "hybrid"))
            quest_index_v2->quest_mode = QUEST_MODE_HYBRID;
        else {
            send_to_char("QEdit: unknown mode.\n\r", ch);
            return;
        }
        printf_to_char(ch, "QEdit: mode set to %s.\n\r", qedit_mode_name(quest_index_v2->quest_mode));
        return;
    }

    if (!str_prefix(arg2, "category")) {
        if (IS_NULLSTR(arg3)) {
            send_to_char("QEdit: category requires full or mission.\n\r", ch);
            return;
        }

        if (!str_prefix(arg3, "full"))
            quest_index_v2->category = QUEST_CATEGORY_FULL;
        else if (!str_prefix(arg3, "mission"))
            quest_index_v2->category = QUEST_CATEGORY_MISSION;
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

    if (!str_prefix(arg2, "allowance")) {
        long value;

        if (IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("QEdit: allowance requires a numeric cost >= 0.\n\r", ch);
            return;
        }

        value = atol(arg3);
        if (value < 0) {
            send_to_char("QEdit: allowance must be >= 0.\n\r", ch);
            return;
        }
        quest_index_v2->allowance_cost = (int)value;
        printf_to_char(ch, "QEdit: allowance set to %d.\n\r", quest_index_v2->allowance_cost);
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
            printf_to_char(ch, "  name      : %s\n\r", stage->name);
            printf_to_char(ch, "  desc      : %s\n\r", stage->description);
            printf_to_char(ch, "  source    : %s\n\r", qedit_stage_source_name(stage->stage_source));
            printf_to_char(ch, "  autocommence: %s\n\r", stage->auto_commence ? "on" : "off");
            printf_to_char(ch, "  complete  : %s\n\r", qedit_stage_completion_name(stage->completion_mode));
            printf_to_char(ch, "  next      : %d\n\r", stage->next_stage_id);
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
            send_to_char("QEdit: stage edit requires a field (name/desc/source/autocommence/complete/next/profile/salt).\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "name")) {
            if (IS_NULLSTR(argument)) {
                send_to_char("QEdit: stage name requires text.\n\r", ch);
                return;
            }
            free_string(stage->name);
            stage->name = str_dup(argument);
            send_to_char("QEdit: stage name updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg4, "desc") || !str_prefix(arg4, "description")) {
            if (IS_NULLSTR(argument)) {
                send_to_char("QEdit: stage desc requires text.\n\r", ch);
                return;
            }
            free_string(stage->description);
            stage->description = str_dup(argument);
            send_to_char("QEdit: stage description updated.\n\r", ch);
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

        if (!str_prefix(arg4, "profile")) {
            if (IS_NULLSTR(argument) || !str_cmp(argument, "none")) {
                free_string(stage->generator_profile);
                stage->generator_profile = str_dup("");
                send_to_char("QEdit: stage generator profile cleared.\n\r", ch);
                return;
            }

            free_string(stage->generator_profile);
            stage->generator_profile = str_dup(argument);
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
                    printf_to_char(ch, "  [%d] target:%ld#%ld weight:%d\n\r",
                        pool_entry->id,
                        pool_entry->target_wnum.pArea ? pool_entry->target_wnum.pArea->uid : 0,
                        pool_entry->target_wnum.vnum,
                        pool_entry->weight);
                }
                return;
            }

            if (!str_prefix(arg4, "add")) {
                char pool_target_arg[MIL];
                char pool_weight_arg[MIL];

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

                argument = one_argument(argument, pool_target_arg);
                argument = one_argument(argument, pool_weight_arg);

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
                if (IS_NULLSTR(arg5) || !is_number(arg5) || IS_NULLSTR(arg6) || !is_number(arg6) || IS_NULLSTR(argument) || !is_number(argument)) {
                    send_to_char("QEdit: objective pool del requires <stage_id> <objective_id> <entry_id>.\n\r", ch);
                    return;
                }

                stage_id = atol(arg5);
                objective_id = atol(arg6);
                pool_entry_id = atol(argument);

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

                printf_to_char(ch, "  [%d] type:%s required:%d quantity:%d optional:%s mode:%s target:%ld#%ld ref:%s refname:%s destination:%ld#%ld destinationref:%s destinationrefname:%s token:%ld#%ld tokenref:%s tokenrefname:%s pool:%d tag:%s\n\r",
                    objective->id,
                    qedit_objective_type_name(objective->objective_type),
                    objective->required_count,
                    objective->quantity,
                    objective->optional ? "on" : "off",
                    qedit_target_mode_name(objective->target_mode),
                    objective->target_wnum.pArea ? objective->target_wnum.pArea->uid : 0,
                    objective->target_wnum.vnum,
                    ref_buf,
                    !IS_NULLSTR(objective->target_variable_name) ? objective->target_variable_name : "",
                    objective->destination_wnum.pArea ? objective->destination_wnum.pArea->uid : 0,
                    objective->destination_wnum.vnum,
                    destination_ref_buf,
                    !IS_NULLSTR(objective->destination_variable_name) ? objective->destination_variable_name : "",
                    objective->target_token_wnum.pArea ? objective->target_token_wnum.pArea->uid : 0,
                    objective->target_token_wnum.vnum,
                        token_ref_buf,
                        !IS_NULLSTR(objective->target_token_variable_name) ? objective->target_token_variable_name : "",
                    qedit_count_pool_entries(objective),
                    IS_NULLSTR(objective->target_tag) ? "" : objective->target_tag);
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
            printf_to_char(ch, "  mode     : %s\n\r", qedit_target_mode_name(objective->target_mode));
            printf_to_char(ch, "  target   : %ld#%ld\n\r",
                objective->target_wnum.pArea ? objective->target_wnum.pArea->uid : 0,
                objective->target_wnum.vnum);
            if (!IS_NULLSTR(objective->target_ref_name))
                printf_to_char(ch, "  targetref: $%s\n\r", objective->target_ref_name);
            else
                printf_to_char(ch, "  targetref: %d:%d\n\r",
                    objective->target_ref_stage_id,
                    objective->target_ref_objective_id);
            printf_to_char(ch, "  refname  : %s\n\r",
                IS_NULLSTR(objective->target_variable_name) ? "" : objective->target_variable_name);
            printf_to_char(ch, "  destination: %ld#%ld\n\r",
                objective->destination_wnum.pArea ? objective->destination_wnum.pArea->uid : 0,
                objective->destination_wnum.vnum);
            if (!IS_NULLSTR(objective->destination_ref_name))
                printf_to_char(ch, "  destinationref: $%s\n\r", objective->destination_ref_name);
            else
                printf_to_char(ch, "  destinationref: none\n\r");
            printf_to_char(ch, "  destinationrefname: %s\n\r",
                IS_NULLSTR(objective->destination_variable_name) ? "" : objective->destination_variable_name);
            printf_to_char(ch, "  token    : %ld#%ld\n\r",
                objective->target_token_wnum.pArea ? objective->target_token_wnum.pArea->uid : 0,
                objective->target_token_wnum.vnum);
            if (!IS_NULLSTR(objective->target_token_ref_name))
                printf_to_char(ch, "  tokenref : $%s\n\r", objective->target_token_ref_name);
            else
                printf_to_char(ch, "  tokenref : none\n\r");
            printf_to_char(ch, "  tokenrefname: %s\n\r",
                IS_NULLSTR(objective->target_token_variable_name) ? "" : objective->target_token_variable_name);
            printf_to_char(ch, "  pool     : %d entries\n\r", qedit_count_pool_entries(objective));
            printf_to_char(ch, "  tag      : %s\n\r", objective->target_tag);
            printf_to_char(ch, "  desc     : %s\n\r", objective->description);
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
            send_to_char("QEdit: objective field required (type/required/quantity/optional/targetmode/target/targetref/refname/destination/destinationref/destinationrefname/targettoken/targettokenref/targettokenrefname/tag/desc).\n\r", ch);
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

        if (!str_prefix(arg5, "targetmode") || !str_prefix(arg5, "mode")) {
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

        if (!str_prefix(arg5, "destinationref") || !str_prefix(arg5, "destref")) {
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

        if (!str_prefix(arg5, "targetref") || !str_prefix(arg5, "ref")) {
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

            free_string(objective->target_variable_name);
            objective->target_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective target refname set to %s.\n\r", objective->target_variable_name);
            return;
        }

        if (!str_prefix(arg5, "destinationrefname") || !str_prefix(arg5, "destrefname") || !str_prefix(arg5, "destvar")) {
            const char *name = arg6;

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

            free_string(objective->destination_variable_name);
            objective->destination_variable_name = str_dup(name);
            printf_to_char(ch, "QEdit: objective destination refname set to %s.\n\r", objective->destination_variable_name);
            return;
        }

        if (!str_prefix(arg5, "targettoken") || !str_prefix(arg5, "token")) {
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

        if (!str_prefix(arg5, "tag")) {
            if (IS_NULLSTR(argument) || !str_cmp(argument, "none")) {
                free_string(objective->target_tag);
                objective->target_tag = str_dup("");
                send_to_char("QEdit: objective tag cleared.\n\r", ch);
                return;
            }

            free_string(objective->target_tag);
            objective->target_tag = str_dup(argument);
            send_to_char("QEdit: objective tag updated.\n\r", ch);
            return;
        }

        if (!str_prefix(arg5, "desc") || !str_prefix(arg5, "description")) {
            if (IS_NULLSTR(argument)) {
                send_to_char("QEdit: objective desc requires text.\n\r", ch);
                return;
            }

            free_string(objective->description);
            objective->description = str_dup(argument);
            send_to_char("QEdit: objective description updated.\n\r", ch);
            return;
        }

        send_to_char("QEdit: unknown objective field.\n\r", ch);
        return;
    }

    send_to_char("QEdit: Unknown subcommand. Use SHOW/CREATE/MODE/CATEGORY/SCOPE/REPEAT/ALLOWANCE/ENTRY/ENABLED/SEEDPOLICY/SEED/STAGE/OBJECTIVE.\n\r", ch);
}
