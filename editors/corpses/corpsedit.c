#include <stdio.h>
#include <string.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../io/json/json_corpse.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

CORPSEDIT(corpsedit_list);
CORPSEDIT(corpsedit_show);
CORPSEDIT(corpsedit_name);
CORPSEDIT(corpsedit_keywords);
CORPSEDIT(corpsedit_short);
CORPSEDIT(corpsedit_long);
CORPSEDIT(corpsedit_description);
CORPSEDIT(corpsedit_owner_loot);
CORPSEDIT(corpsedit_headless);
CORPSEDIT(corpsedit_animate_headless);
CORPSEDIT(corpsedit_resurrect);
CORPSEDIT(corpsedit_animate);
CORPSEDIT(corpsedit_skull);
CORPSEDIT(corpsedit_decay_type);
CORPSEDIT(corpsedit_decay_rate);
CORPSEDIT(corpsedit_lost);
CORPSEDIT(corpsedit_save);
CORPSEDIT(corpsedit_reload);

static bool corpsedit_booted = false;

static int corpsedit_resolve_type(char *arg);
static const char *corpsedit_type_name(int type);
static struct corpse_info *corpsedit_current(CHAR_DATA *ch, int *out_type);
static bool corpsedit_mark_dirty_and_save(void);

int corpse_type_count(void)
{
    int i;
    int max_type = -1;

    for (i = 0; corpse_types[i].name != NULL; i++) {
        if (corpse_types[i].bit >= RAWKILL_NORMAL && corpse_types[i].bit > max_type)
            max_type = corpse_types[i].bit;
    }

    return max_type + 1;
}

int corpse_type_lookup(const char *name)
{
    if (IS_NULLSTR(name))
        return NO_FLAG;

    return flag_value(corpse_types, (char *)name);
}

const char *corpse_type_name(int corpse_type)
{
    int i;

    for (i = 0; corpse_types[i].name != NULL; i++) {
        if (corpse_types[i].bit == corpse_type)
            return corpse_types[i].name;
    }

    return "unknown";
}

const struct olc_cmd_type corpsedit_table[] = {
    { "?",                show_help },
    { "commands",         show_commands },
    { "list",             corpsedit_list },
    { "show",             corpsedit_show },
    { "name",             corpsedit_name },
    { "keywords",         corpsedit_keywords },
    { "short",            corpsedit_short },
    { "long",             corpsedit_long },
    { "description",      corpsedit_description },
    { "ownerloot",        corpsedit_owner_loot },
    { "headless",         corpsedit_headless },
    { "animateheadless",  corpsedit_animate_headless },
    { "resurrect",        corpsedit_resurrect },
    { "animate",          corpsedit_animate },
    { "skull",            corpsedit_skull },
    { "decaytype",        corpsedit_decay_type },
    { "decayrate",        corpsedit_decay_rate },
    { "lost",             corpsedit_lost },
    { "save",             corpsedit_save },
    { "reload",           corpsedit_reload },
    { NULL,                0 }
};

static const OLC_EDITOR_DEF corpsedit_def = {
    .name           = "CorpsEdit",
    .editor_type    = ED_CORPSE,
    .cmd_table      = corpsedit_table,
    .show_fn        = corpsedit_show,
    .tabs           = {
        .count      = 1,
        .tabs       = {
            { "Basic", "Bas", NULL },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_NONE,
    .audit_changes  = false,
};

void do_corpsedit(CHAR_DATA *ch, char *argument)
{
    int type;

    if (IS_NPC(ch))
        return;

    load_corpse_data();

    if (!olc_editor_check_perm(ch, &corpsedit_def, NULL)) {
        send_to_char("You don't have permission to edit corpse definitions.\n\r", ch);
        return;
    }

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: corpsedit list\n\r", ch);
        send_to_char("        corpsedit <type|index>\n\r", ch);
        return;
    }

    if (!str_cmp(argument, "list")) {
        corpsedit_list(ch, "");
        return;
    }

    type = corpsedit_resolve_type(argument);
    if (type < 0 || type >= corpse_type_count()) {
        send_to_char("No corpse type found by that name or index.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &corpsedit_def, &corpse_info_table[type], true);
}

void corpsedit(CHAR_DATA *ch, char *argument)
{
    load_corpse_data();
    olc_editor_interp(ch, argument, &corpsedit_def);
}

static bool corpsedit_mark_dirty_and_save(void)
{
    return save_corpse_data();
}

int corpse_type_sanitize(int corpse_type)
{
    if (corpse_type < 0 || corpse_type >= corpse_type_count())
        return RAWKILL_NORMAL;

    return corpse_type;
}

const struct corpse_info *corpse_info_by_type(int corpse_type)
{
    load_corpse_data();
    return &corpse_info_table[corpse_type_sanitize(corpse_type)];
}

bool save_corpse_data(void)
{
    int saved = 0;

    if (!corpsedit_booted)
        return true;

    if (!json_corpse_save_types(&saved))
        return false;

    log_stringf("save_corpse_data: Saved %d corpse types.", saved);
    return true;
}

void load_corpse_data(void)
{
    int loaded = 0;

    if (corpsedit_booted)
        return;

    corpsedit_booted = true;

    if (!json_corpse_load_types(&loaded)) {
        if (!save_corpse_data())
            log_string("load_corpse_data: Failed to seed corpses.json from defaults.");
        return;
    }

    log_stringf("load_corpse_data: Loaded %d corpse types.", loaded);
}

static int corpsedit_resolve_type(char *arg)
{
    int value;

    if (IS_NULLSTR(arg))
        return -1;

    if (is_number(arg))
        return atoi(arg);

    value = corpse_type_lookup(arg);
    if (value == NO_FLAG)
        return -1;

    return value;
}

static const char *corpsedit_type_name(int type)
{
    return corpse_type_name(type);
}

static struct corpse_info *corpsedit_current(CHAR_DATA *ch, int *out_type)
{
    struct corpse_info *corpse;
    int i;

    if (!ch || !ch->desc || !ch->desc->pEdit)
        return NULL;

    corpse = (struct corpse_info *)ch->desc->pEdit;

    for (i = 0; i < corpse_type_count(); i++) {
        if (&corpse_info_table[i] == corpse) {
            if (out_type)
                *out_type = i;
            return corpse;
        }
    }

    return NULL;
}

CORPSEDIT(corpsedit_list)
{
    int i;

    send_to_char("{WIdx  Type          Name{X\n\r", ch);
    send_to_char("{D---- ------------- --------------------------------{X\n\r", ch);

    for (i = 0; i < corpse_type_count(); i++) {
        printf_to_char(ch, "{W%-4d %-13s {x%s{X\n\r",
            i,
            corpsedit_type_name(i),
            corpse_info_table[i].name ? corpse_info_table[i].name : "(null)");
    }

    return false;
}

CORPSEDIT(corpsedit_show)
{
    struct corpse_info *corpse;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&corpsedit_def);
    OLC_LAYOUT_CTX *ctx;
    int type = -1;

    corpse = corpsedit_current(ch, &type);
    if (!corpse)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "CorpsEdit",
        corpsedit_type_name(type),
        formatf("Type %d", type),
        &corpsedit_def);

    olc_display_section(ctx, theme, "Identity");
    olc_display_string(ctx, theme, "Name:", "name", corpse->name);
    olc_display_string(ctx, theme, "Keywords:", "keywords", corpse->short_descr);
    olc_display_string(ctx, theme, "Short:", "short", corpse->short_descr);
    olc_display_string(ctx, theme, "Long:", "long", corpse->long_descr);
    olc_display_text(ctx, theme, "Description:", "description", corpse->full_descr);

    olc_display_blank(ctx);
    olc_display_section(ctx, theme, "Behavior");
    olc_display_bool(ctx, theme, "Owner Loot:", "ownerloot", corpse->owner_loot);
    olc_display_bool(ctx, theme, "Headless:", "headless", corpse->headless);
    olc_display_bool(ctx, theme, "Animate Headless:", "animateheadless", corpse->animate_headless);
    olc_display_number(ctx, theme, "Resurrect Chance:", "resurrect", corpse->resurrect_chance);
    olc_display_number(ctx, theme, "Animate Chance:", "animate", corpse->animation_chance);
    olc_display_number(ctx, theme, "Skull Chance:", "skull", corpse->skulling_chance);
    olc_display_string(ctx, theme, "Decay Type:", "decaytype",
        corpsedit_type_name(corpse->decay_type));
    olc_display_number(ctx, theme, "Decay Rate:", "decayrate", corpse->decay_rate);
    olc_display_text(ctx, theme, "Lost Parts:", "lost", part_bit_name(corpse->lost_bodyparts));

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

CORPSEDIT(corpsedit_name)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_string(ch, argument, "name", "name <format>",
        &corpse->name, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Name updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_keywords)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_string(ch, argument, "keywords", "keywords <text>",
        &corpse->short_descr, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Keywords updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_short)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_string(ch, argument, "short", "short <text>",
        &corpse->short_descr, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Short updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_long)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_string(ch, argument, "long", "long <text>",
        &corpse->long_descr, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Long updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_description)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_string(ch, argument, "description", "description <text>",
        &corpse->full_descr, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Description updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_owner_loot)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_bool(ch, argument, "ownerloot", NULL,
        &corpse->owner_loot, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Owner loot updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_headless)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_bool(ch, argument, "headless", NULL,
        &corpse->headless, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Headless updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_animate_headless)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_bool(ch, argument, "animateheadless", NULL,
        &corpse->animate_headless, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Animate headless updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_resurrect)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_number(ch, argument, "resurrect",
        "Syntax: resurrect <0-100>\n\r",
        &corpse->resurrect_chance, 0, 100, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Resurrect chance updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_animate)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_number(ch, argument, "animate",
        "Syntax: animate <0-100>\n\r",
        &corpse->animation_chance, 0, 100, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Animate chance updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_skull)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_number(ch, argument, "skull",
        "Syntax: skull <-1..100>\n\r",
        &corpse->skulling_chance, -1, 100, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Skull chance updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_decay_type)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    int value;

    if (!corpse)
        return false;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: decaytype <corpse type>\n\r", ch);
        return false;
    }

    value = flag_value(corpse_types, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid corpse type. Use corpsedit list for valid types.\n\r", ch);
        return false;
    }

    corpse->decay_type = value;
    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Decay type updated, but failed to save corpses.json.\n\r", ch);
    else
        send_to_char("Decay type set.\n\r", ch);
    return true;
}

CORPSEDIT(corpsedit_decay_rate)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    bool changed;

    if (!corpse)
        return false;

    changed = olc_cmd_number(ch, argument, "decayrate",
        "Syntax: decayrate <0-10000>\n\r",
        &corpse->decay_rate, 0, 10000, NULL, NULL);
    if (!changed)
        return false;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Decay rate updated, but failed to save corpses.json.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_lost)
{
    struct corpse_info *corpse = corpsedit_current(ch, NULL);
    long value;

    if (!corpse)
        return false;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: lost <part>\n\r", ch);
        return false;
    }

    value = flag_value(part_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid part flag.\n\r", ch);
        return false;
    }

    corpse->lost_bodyparts ^= value;

    if (!corpsedit_mark_dirty_and_save())
        send_to_char("Lost body parts updated, but failed to save corpses.json.\n\r", ch);
    else
        send_to_char("Lost body parts toggled.\n\r", ch);

    return true;
}

CORPSEDIT(corpsedit_save)
{
    load_corpse_data();

    if (!save_corpse_data()) {
        send_to_char("Failed to save corpses.json.\n\r", ch);
        return false;
    }

    send_to_char("Corpse data saved to corpses.json.\n\r", ch);
    return false;
}

CORPSEDIT(corpsedit_reload)
{
    int loaded = 0;

    load_corpse_data();

    if (!json_corpse_load_types(&loaded)) {
        send_to_char("Failed to reload corpses.json.\n\r", ch);
        return false;
    }

    send_to_char("Corpse data reloaded from corpses.json.\n\r", ch);
    return true;
}
