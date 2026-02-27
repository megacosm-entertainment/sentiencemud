/***************************************************************************
 *  olc_mpcode.c — Script Editors (MobProg, ObjProg, RoomProg, etc.)      *
 *                                                                         *
 *  Provides 9 script editors, all sharing a common set of commands:       *
 *    MPEdit (mob progs), OPEdit (obj progs), RPEdit (room progs),         *
 *    TPEdit (token progs), APEdit (area progs), IPEdit (instance progs),  *
 *    DPEdit (dungeon progs), QPEdit (quest progs), EPEdit (event progs)   *
 *                                                                         *
 *  Based on ILAB OLC by Jason Dinkel.                                     *
 *  Mobprogram code by Lordrom for Nevermore Mud.                          *
 *  Scripting engine rebuilt by Michael Kurtz (Nibelung).                  *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework (Phase 6).                *
 ***************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../scripts.h"
#include "../../mxp_links.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

#define SCRIPTEDIT( fun )	bool fun(CHAR_DATA *ch, char *argument)

/***************************************************************************
 * Static Helpers                                                          *
 ***************************************************************************/

static int olc_script_typeifc[] = {
    IFC_M,
    IFC_O,
    IFC_R,
    IFC_T,
    IFC_A,
    IFC_I,
    IFC_D,
    IFC_Q,
    IFC_E,
};

/**
 * script_security_check - Check if character meets script security level
 *
 * Testports have reduced security checks so builders can test scripts
 * without full implementor access.
 *
 * @param ch  Character to check
 * @return    true if character has sufficient security
 */
bool script_security_check(CHAR_DATA *ch)
{
    if (!game_settings.testport)
        return (bool)(!IS_NPC(ch) && ch->tot_level >= (MAX_LEVEL - 1));
    else
        return true;
}

/**
 * script_imp_check - Check if character has implementor-level script access
 *
 * Required for SECURED/SYSTEM flag modifications and security level changes.
 * Testports allow slightly lower access.
 *
 * @param ch  Character to check
 * @return    true if character has implementor-level access
 */
bool script_imp_check(CHAR_DATA *ch)
{
    if (!game_settings.testport)
        return (bool)(!IS_NPC(ch) && ch->tot_level == MAX_LEVEL);
    else
        return (bool)(!IS_NPC(ch) && ch->tot_level >= (MAX_LEVEL - 1));
}

/**
 * script_type_label - Get the display label for a script type
 *
 * @param type  PRG_* constant
 * @return      Short label string (e.g., "MobProg", "ObjProg")
 */
static const char *script_type_label(int type)
{
    switch (type) {
        case PRG_MPROG: return "MobProg";
        case PRG_OPROG: return "ObjProg";
        case PRG_RPROG: return "RoomProg";
        case PRG_TPROG: return "TokenProg";
        case PRG_APROG: return "AreaProg";
        case PRG_IPROG: return "InstanceProg";
        case PRG_DPROG: return "DungeonProg";
        case PRG_QPROG: return "QuestProg";
        case PRG_EPROG: return "EventProg";
        default:        return "Script";
    }
}

#define SCRIPT_EDITOR_LOG_MAX 16384

static const char *scriptedit_format_time(time_t when)
{
    static char buf[64];
    struct tm *tm_info;

    if (when <= 0)
        return "never";

    tm_info = localtime(&when);
    if (!tm_info)
        return "unknown";

    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info) == 0)
        return "unknown";

    return buf;
}

static void scriptedit_set_log_text(char **target, const char *text)
{
    size_t len;
    const char *source;

    if (!target)
        return;

    if (*target) {
        free_string(*target);
        *target = NULL;
    }

    if (IS_NULLSTR(text))
        return;

    len = strlen(text);
    source = text;
    if (len > SCRIPT_EDITOR_LOG_MAX)
        source = text + (len - SCRIPT_EDITOR_LOG_MAX);

    *target = str_dup(source);
}

static bool scriptedit_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc)
        return false;
    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

static void scriptedit_mxp_send(CHAR_DATA *ch, const char *command,
    const char *text, char *dest, size_t dest_size)
{
    const char *result;

    if (!dest || dest_size == 0)
        return;

    if (IS_NULLSTR(text)) {
        dest[0] = '\0';
        return;
    }

    if (!scriptedit_use_mxp(ch) || IS_NULLSTR(command)) {
        snprintf(dest, dest_size, "%s", text);
        return;
    }

    result = MXPCreateSend(ch->desc, command, text);
    snprintf(dest, dest_size, "%s", result ? result : text);
}

static void scriptedit_build_owner_links(CHAR_DATA *ch, const char *wnum,
    const char *edit_base, const char *show_base,
    char *wnum_link, size_t link_size)
{
    BUFFER *tmp;
    char show_cmd[MIL];
    char edit_cmd[MIL];
    mxp_cmd_hint_t items[2];

    if (!wnum_link)
        return;

    wnum_link[0] = '\0';

    if (IS_NULLSTR(wnum) || IS_NULLSTR(show_base))
        return;

    snprintf(show_cmd, sizeof(show_cmd), "%s %s", show_base, wnum);

    if (!scriptedit_use_mxp(ch)) {
        snprintf(wnum_link, link_size, "%s", wnum);
        return;
    }

    if (!IS_NULLSTR(edit_base)) {
        snprintf(edit_cmd, sizeof(edit_cmd), "%s %s", edit_base, wnum);
        items[0].cmd = show_cmd;
        items[0].hint = "Show details";
        items[1].cmd = edit_cmd;
        items[1].hint = "Edit entity";

        tmp = new_buf();
        if (!tmp) {
            snprintf(wnum_link, link_size, "%s", wnum);
            return;
        }

        mxp_link_multi(ch->desc, tmp, wnum, items, 2);
        snprintf(wnum_link, link_size, "%s", buf_string(tmp));
        free_buf(tmp);
    } else {
        scriptedit_mxp_send(ch, show_cmd, wnum, wnum_link, link_size);
    }
}

static bool scriptedit_prog_matches(const PROG_LIST *prog, const SCRIPT_DATA *script,
    int expected_type)
{
    if (!prog || !script)
        return false;

    if (script->type != expected_type)
        return false;

    if (prog->script && prog->script->type == expected_type
        && prog->script->vnum == script->vnum
        && prog->script->area == script->area)
        return true;

    if (prog->script_is_widevnum) {
        if (prog->script_load.vnum == script->vnum
            && script->area
            && prog->script_load.auid == script->area->uid)
            return true;
    }

    return false;
}

static SCRIPT_DATA **scriptedit_get_area_script_list_head(AREA_DATA *area, int type)
{
    if (!area)
        return NULL;

    switch (type) {
    case PRG_MPROG: return &area->mprog_list;
    case PRG_OPROG: return &area->oprog_list;
    case PRG_RPROG: return &area->rprog_list;
    case PRG_TPROG: return &area->tprog_list;
    case PRG_APROG: return &area->aprog_list;
    case PRG_IPROG: return &area->iprog_list;
    case PRG_DPROG: return &area->dprog_list;
    case PRG_QPROG: return &area->qprog_list;
    case PRG_EPROG: return &area->eprog_list;
    default: return NULL;
    }
}

static long scriptedit_next_auto_vnum(AREA_DATA *area, int type)
{
    SCRIPT_DATA **list_head;
    SCRIPT_DATA *script;
    long next_vnum = 1;

    list_head = scriptedit_get_area_script_list_head(area, type);
    if (!list_head)
        return 0;

    for (script = *list_head; script; script = script->next) {
        if (script->vnum >= next_vnum)
            next_vnum = script->vnum + 1;
    }

    return next_vnum;
}

static int scriptedit_show_uses_from_bank(OLC_LAYOUT_CTX *ctx, SCRIPT_DATA *script,
    LLIST **progs, const char *owner, bool is_rprog, bool is_tprog,
    int expected_type)
{
    const OLC_EDITOR_THEME *theme = &olc_theme_scripting;
    ITERATOR it;
    PROG_LIST *prog;
    bool header_printed = false;
    int count = 0;

    if (!progs || !owner || !owner[0])
        return 0;

    for (int slot = 0; slot < TRIGSLOT_MAX; slot++) {
        if (!progs[slot])
            continue;

        iterator_start(&it, progs[slot]);
        while ((prog = (PROG_LIST *)iterator_nextdata(&it))) {
            if (!scriptedit_prog_matches(prog, script, expected_type))
                continue;

            if (!header_printed) {
                olc_display_infof(ctx, theme, "{C%s{x", owner);
                header_printed = true;
            }

            olc_display_infof(ctx, theme, "  {g%-12s{x %s",
                trigger_name(prog->trig_type),
                trigger_phrase_olcshow(prog->trig_type, prog->trig_phrase,
                    is_rprog, is_tprog));
            count++;
        }
        iterator_stop(&it);
    }

    return count;
}

/***************************************************************************
 * Framework Callbacks                                                     *
 ***************************************************************************/

/**
 * script_get_area - Get the area associated with a script
 *
 * Used by the framework for OLC_CHANGE_AREA_FLAG change tracking.
 *
 * @param pEdit  SCRIPT_DATA being edited
 * @return       The script's parent area, or NULL
 */
static AREA_DATA *script_get_area(void *pEdit)
{
    SCRIPT_DATA *pCode = (SCRIPT_DATA *)pEdit;
    return pCode ? pCode->area : NULL;
}

/**
 * script_perm_blueprint - Permission check for blueprint scripts
 *
 * @param ch     Character attempting access
 * @param pEdit  SCRIPT_DATA being edited (may be NULL)
 * @return       true if access is allowed
 */
static bool script_perm_blueprint(CHAR_DATA *ch, void *pEdit)
{
    (void)pEdit;
    return can_edit_blueprints(ch);
}

/**
 * script_perm_dungeon - Permission check for dungeon scripts
 *
 * @param ch     Character attempting access
 * @param pEdit  SCRIPT_DATA being edited (may be NULL)
 * @return       true if access is allowed
 */
static bool script_perm_dungeon(CHAR_DATA *ch, void *pEdit)
{
    (void)pEdit;
    return can_edit_dungeons(ch);
}

/***************************************************************************
 * Command Tables                                                          *
 ***************************************************************************/

const struct olc_cmd_type mpedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     mpedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       mpedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type opedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     opedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       opedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type rpedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     rpedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       rpedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type tpedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     tpedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       tpedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type apedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     apedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       apedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type ipedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     ipedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       ipedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type dpedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     dpedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       dpedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type qpedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     qpedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       qpedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

const struct olc_cmd_type epedit_table[] =
{
    { "?",          show_help           },
    { "code",       scriptedit_code     },
    { "commands",   show_commands       },
    { "comments",   scriptedit_comments },
    { "compile",    scriptedit_compile  },
    { "create",     epedit_create       },
    { "depth",      scriptedit_depth    },
    { "flags",      scriptedit_flags    },
    { "list",       epedit_list         },
    { "name",       scriptedit_name     },
    { "security",   scriptedit_security },
    { "show",       scriptedit_show     },
    { NULL,         0                   }
};

/***************************************************************************
 * Tab Show Functions                                                      *
 ***************************************************************************/

/**
 * scriptedit_show_general_tab - Display the main script properties tab
 *
 * Shows name, vnum, type, call depth, security, flags, code source,
 * and builder comments.
 *
 * @param ch     Character viewing the editor
 * @param ctx    Layout context for output
 * @param pEdit  SCRIPT_DATA being viewed
 */
static void scriptedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    SCRIPT_DATA *pCode = (SCRIPT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = &olc_theme_scripting;
    char depth_buf[MIL];
    const char *status;

    /* Properties section */
    olc_display_section(ctx, theme, "Properties");

    olc_display_string(ctx, theme, "Name:", "name",
        pCode->name ? pCode->name : "");

    olc_display_infof(ctx, theme, "Widevnum:     %s%s{x",
        theme->value, widevnum_string_script(pCode, NULL));

    olc_display_infof(ctx, theme, "Type:         %s%s{x",
        theme->value, script_type_label(pCode->type));

    /* Call depth display */
    if (pCode->depth < 0)
        strcpy(depth_buf, "{RInfinite{x");
    else if (!pCode->depth)
        sprintf(depth_buf, "{GDefault{x (%d)", MAX_CALL_LEVEL);
    else
        sprintf(depth_buf, "%s%d{x", theme->value, pCode->depth);

    olc_display_infof(ctx, theme, "Call Depth:   %s", depth_buf);

    olc_display_number(ctx, theme, "Security:", "security", pCode->security);

    olc_display_flags(ctx, theme, "Flags:", "flags", script_flags, pCode->flags);

    /* Status indicator */
    if (IS_SET(pCode->flags, SCRIPT_DISABLED))
        status = "{DDisabled{x";
    else if (pCode->lines > 1 && pCode->src != pCode->edit_src)
        status = "{GModified{x (needs compile)";
    else if (pCode->lines == 1)
        status = "{WBlank{x";
    else if (pCode->code)
        status = "{xCompiled{x";
    else
        status = "{RUncompiled{x";

    olc_display_infof(ctx, theme, "Status:       %s", status);

    /* Code section */
    olc_display_section(ctx, theme, "Code");
    olc_display_text(ctx, theme, NULL, "code", pCode->edit_src);

    /* Comments section (only if present) */
    if (pCode->comments && pCode->comments[0]) {
        olc_display_section(ctx, theme, "Builder Comments");
        olc_display_text(ctx, theme, NULL, "comments", pCode->comments);
    }
}

static void scriptedit_show_logs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    (void)ch;
    SCRIPT_DATA *pCode = (SCRIPT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = &olc_theme_scripting;

    olc_display_section(ctx, theme, "Compile Logs");
    olc_display_infof(ctx, theme, "Last Attempt: %s%s{x",
        theme->value, scriptedit_format_time(pCode->last_compile_time));
    olc_display_infof(ctx, theme, "Result:       %s%s{x",
        (pCode->last_compile_time <= 0) ? "{D" : (pCode->last_compile_success ? "{G" : "{R"),
        (pCode->last_compile_time <= 0) ? "not run" : (pCode->last_compile_success ? "success" : "failure"));
    if (pCode->last_compile_log && pCode->last_compile_log[0])
        olc_display_text(ctx, theme, NULL, NULL, pCode->last_compile_log);
    else
        olc_display_infof(ctx, theme, "{DNo compile output recorded yet.{x");

    olc_display_section(ctx, theme, "Runtime Logs");
    olc_display_infof(ctx, theme, "Last Runtime Error: %s%s{x",
        theme->value, scriptedit_format_time(pCode->last_runtime_time));
    if (pCode->last_runtime_log && pCode->last_runtime_log[0])
        olc_display_text(ctx, theme, NULL, NULL, pCode->last_runtime_log);
    else
        olc_display_infof(ctx, theme, "{DNo runtime errors captured for this script yet.{x");
}

static void scriptedit_show_uses_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    SCRIPT_DATA *pCode = (SCRIPT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = &olc_theme_scripting;
    AREA_DATA *area;
    int total = 0;

    olc_display_section(ctx, theme, "Script Uses");

    for (area = area_first; area; area = area->next) {
        char owner[MSL];

        for (int hash = 0; hash < MAX_KEY_HASH; hash++) {
            MOB_INDEX_DATA *mob;
            OBJ_INDEX_DATA *obj;
            ROOM_INDEX_DATA *room;
            TOKEN_INDEX_DATA *token;
            BLUEPRINT *bp;
            DUNGEON_INDEX_DATA *dng;
            QUEST_INDEX_V2_DATA *quest_index_v2;
            EVENT_INDEX_DATA *event_index;

            for (mob = area->mob_index_hash[hash]; mob; mob = mob->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_mobile(mob, NULL));
                scriptedit_build_owner_links(ch, wnum, "medit", "mshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Mobile %s (%.120s)",
                    wnum_link,
                    mob->short_descr ? mob->short_descr : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, mob->progs, owner, false, false,
                    PRG_MPROG);
            }

            for (obj = area->obj_index_hash[hash]; obj; obj = obj->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_object(obj, NULL));
                scriptedit_build_owner_links(ch, wnum, "oedit", "oshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Object %s (%.120s)",
                    wnum_link,
                    obj->short_descr ? obj->short_descr : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, obj->progs, owner, false, false,
                    PRG_OPROG);
            }

            for (room = area->room_index_hash[hash]; room; room = room->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_room(room, NULL));
                scriptedit_build_owner_links(ch, wnum, "redit", "rshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Room %s (%.120s)",
                    wnum_link,
                    room->name ? room->name : "(unnamed)");
                if (room->progs)
                    total += scriptedit_show_uses_from_bank(ctx, pCode, room->progs->progs, owner, true, false,
                        PRG_RPROG);
            }

            for (token = area->token_index_hash[hash]; token; token = token->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_token(token, NULL));
                scriptedit_build_owner_links(ch, wnum, "tedit", "tshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Token %s (%.120s)",
                    wnum_link,
                    token->name ? token->name : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, token->progs, owner, false, true,
                    PRG_TPROG);
            }

            for (bp = area->blueprint_hash[hash]; bp; bp = bp->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_blueprint(bp, NULL));
                scriptedit_build_owner_links(ch, wnum, "bpedit", "bpshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Blueprint %s (%.120s)",
                    wnum_link,
                    bp->name ? bp->name : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, bp->progs, owner, false, false,
                    PRG_IPROG);
            }

            for (dng = area->dungeon_index_hash[hash]; dng; dng = dng->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_dungeon(dng, NULL));
                scriptedit_build_owner_links(ch, wnum, "dngedit", "dngshow",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Dungeon %s (%.120s)",
                    wnum_link,
                    dng->name ? dng->name : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, dng->progs, owner, false, false,
                    PRG_DPROG);
            }

            for (quest_index_v2 = area->quest_index_v2_hash[hash]; quest_index_v2;
                quest_index_v2 = quest_index_v2->next) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string(area, quest_index_v2->vnum, NULL));
                scriptedit_build_owner_links(ch, wnum, NULL, "qedit",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Quest %s (%.120s)",
                    wnum_link,
                    quest_index_v2->name ? quest_index_v2->name : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, quest_index_v2->progs, owner,
                    false, false, PRG_QPROG);
            }

            for (event_index = area->event_index_hash[hash]; event_index;
                event_index = event_index->next_hash) {
                char wnum[128], wnum_link[256];
                snprintf(wnum, sizeof(wnum), "%s", widevnum_string_event(event_index, NULL));
                scriptedit_build_owner_links(ch, wnum, "evtedit", "evtedit",
                    wnum_link, sizeof(wnum_link));
                snprintf(owner, sizeof(owner), "Event %s (%.120s)",
                    wnum_link,
                    event_index->name ? event_index->name : "(unnamed)");
                total += scriptedit_show_uses_from_bank(ctx, pCode, event_index->progs, owner,
                    false, false, PRG_EPROG);
            }
        }

        if (area->progs) {
            snprintf(owner, sizeof(owner), "Area %ld (%s)",
                area->uid,
                area->name ? area->name : "(unnamed)");
            total += scriptedit_show_uses_from_bank(ctx, pCode, area->progs->progs, owner, false, false,
                PRG_APROG);
        }
    }

    if (total == 0)
        olc_display_infof(ctx, theme, "{DNo entities currently reference this script.{x");
    else
        olc_display_infof(ctx, theme, "{GTotal uses: %d{x", total);
}

/***************************************************************************
 * Editor Definitions                                                      *
 ***************************************************************************/

/* Tab definitions shared by all 7 script editors */
#define SCRIPT_EDITOR_TABS \
    .tabs = { \
        .count = 3, \
        .tabs = { \
            { "General", "Gen", scriptedit_show_general_tab }, \
            { "Logs",    "Log", scriptedit_show_logs_tab    }, \
            { "Uses",    "Use", scriptedit_show_uses_tab    }, \
        }, \
    }

/* Area-based script editors (mp/op/rp/tp/ap) use IS_BUILDER security */
#define SCRIPT_AREA_EDITOR_DEF(NAME, ED_TYPE) \
    static const OLC_EDITOR_DEF NAME##_def = { \
        .name           = #NAME, \
        .editor_type    = ED_TYPE, \
        .cmd_table      = NAME##_table, \
        .show_fn        = scriptedit_show, \
        SCRIPT_EDITOR_TABS, \
        .theme          = &olc_theme_scripting, \
        .perm           = { \
            .flags          = OLC_PERM_AREA_SECURITY, \
        }, \
        .change_mode    = OLC_CHANGE_AREA_FLAG, \
        .get_area_fn    = script_get_area, \
        .audit_changes  = true, \
    }

/* We can't use a macro for the names since they need specific casing */

static const OLC_EDITOR_DEF mpedit_def = {
    .name           = "MPEdit",
    .editor_type    = ED_MPCODE,
    .cmd_table      = mpedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF opedit_def = {
    .name           = "OPEdit",
    .editor_type    = ED_OPCODE,
    .cmd_table      = opedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF rpedit_def = {
    .name           = "RPEdit",
    .editor_type    = ED_RPCODE,
    .cmd_table      = rpedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF tpedit_def = {
    .name           = "TPEdit",
    .editor_type    = ED_TPCODE,
    .cmd_table      = tpedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF apedit_def = {
    .name           = "APEdit",
    .editor_type    = ED_APCODE,
    .cmd_table      = apedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

/* Blueprint script editor uses custom permission/change tracking */
static const OLC_EDITOR_DEF ipedit_def = {
    .name           = "IPEdit",
    .editor_type    = ED_IPCODE,
    .cmd_table      = ipedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_CUSTOM,
        .check_fn       = script_perm_blueprint,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

/* Dungeon script editor uses custom permission/change tracking */
static const OLC_EDITOR_DEF dpedit_def = {
    .name           = "DPEdit",
    .editor_type    = ED_DPCODE,
    .cmd_table      = dpedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_CUSTOM,
        .check_fn       = script_perm_dungeon,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF qpedit_def = {
    .name           = "QPEdit",
    .editor_type    = ED_QPCODE,
    .cmd_table      = qpedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_IMPLEMENTOR,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

static const OLC_EDITOR_DEF epedit_def = {
    .name           = "EPEdit",
    .editor_type    = ED_EPCODE,
    .cmd_table      = epedit_table,
    .show_fn        = scriptedit_show,
    SCRIPT_EDITOR_TABS,
    .theme          = &olc_theme_scripting,
    .perm           = {
        .flags          = OLC_PERM_AREA_SECURITY,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = script_get_area,
    .audit_changes  = true,
};

/***************************************************************************
 * Show Function                                                           *
 ***************************************************************************/

/**
 * scriptedit_show - Display the script editor
 *
 * Master show function shared by all 7 script editors. Renders the
 * editor header with tab bar, dispatches to the active tab's show
 * function, and outputs the result.
 *
 * @param ch        Character viewing the editor
 * @param argument  Ignored
 * @return          false (display only, no change)
 */
SCRIPTEDIT(scriptedit_show)
{
    SCRIPT_DATA *pCode;
    const OLC_EDITOR_THEME *theme = &olc_theme_scripting;
    const OLC_EDITOR_DEF *def;
    OLC_LAYOUT_CTX *ctx;
    int tab;

    EDIT_SCRIPT(ch, pCode);

    /* Determine which editor definition to use based on script type */
    switch (pCode->type) {
        case PRG_MPROG: def = &mpedit_def; break;
        case PRG_OPROG: def = &opedit_def; break;
        case PRG_RPROG: def = &rpedit_def; break;
        case PRG_TPROG: def = &tpedit_def; break;
        case PRG_APROG: def = &apedit_def; break;
        case PRG_IPROG: def = &ipedit_def; break;
        case PRG_DPROG: def = &dpedit_def; break;
        case PRG_QPROG: def = &qpedit_def; break;
        case PRG_EPROG: def = &epedit_def; break;
        default:        def = &mpedit_def; break;
    }

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, def->name,
        pCode->name ? pCode->name : "(unnamed)",
        formatf("%s", widevnum_string_script(pCode, NULL)),
        def);

    /* Dispatch to active tab */
    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        for (int i = 0; i < def->tabs.count; i++) {
            if (def->tabs.tabs[i].show_fn)
                def->tabs.tabs[i].show_fn(ch, ctx, (void *)pCode);
        }
    } else if (tab >= 0 && tab < def->tabs.count
        && def->tabs.tabs[tab].show_fn) {
        def->tabs.tabs[tab].show_fn(ch, ctx, (void *)pCode);
    } else {
        scriptedit_show_general_tab(ch, ctx, (void *)pCode);
    }

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    return false;
}

/***************************************************************************
 * Interpreter Functions (delegated to framework)                          *
 ***************************************************************************/

void mpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &mpedit_def);
}

void opedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &opedit_def);
}

void rpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &rpedit_def);
}

void tpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &tpedit_def);
}

void apedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &apedit_def);
}

void ipedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &ipedit_def);
}

void dpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &dpedit_def);
}

void qpedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &qpedit_def);
}

void epedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &epedit_def);
}

/***************************************************************************
 * Entry Points                                                            *
 ***************************************************************************/

/**
 * do_mpedit - Enter the mob script editor
 *
 * Syntax: mpedit [widevnum]
 *         mpedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_mpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pMcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pMcode = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG)) == NULL) {
            send_to_char("MPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &mpedit_def, (void *)pMcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (mpedit_create(ch, argument))
            olc_editor_enter(ch, &mpedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: mpedit [widevnum]\n\r", ch);
    send_to_char("        mpedit create [widevnum]\n\r", ch);
}

/**
 * do_opedit - Enter the object script editor
 *
 * Syntax: opedit [widevnum]
 *         opedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_opedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pOcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pOcode = get_script_index(wnum.pArea, wnum.vnum, PRG_OPROG)) == NULL) {
            send_to_char("OPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &opedit_def, (void *)pOcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (opedit_create(ch, argument))
            olc_editor_enter(ch, &opedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: opedit [widevnum]\n\r", ch);
    send_to_char("        opedit create [widevnum]\n\r", ch);
}

/**
 * do_rpedit - Enter the room script editor
 *
 * Syntax: rpedit [widevnum]
 *         rpedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_rpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pRcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pRcode = get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG)) == NULL) {
            send_to_char("RPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &rpedit_def, (void *)pRcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (rpedit_create(ch, argument))
            olc_editor_enter(ch, &rpedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: rpedit [widevnum]\n\r", ch);
    send_to_char("        rpedit create [widevnum]\n\r", ch);
}

/**
 * do_tpedit - Enter the token script editor
 *
 * Syntax: tpedit [widevnum]
 *         tpedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_tpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pTcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pTcode = get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG)) == NULL) {
            send_to_char("TPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &tpedit_def, (void *)pTcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (tpedit_create(ch, argument))
            olc_editor_enter(ch, &tpedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: tpedit [widevnum]\n\r", ch);
    send_to_char("        tpedit create [widevnum]\n\r", ch);
}

/**
 * do_apedit - Enter the area script editor
 *
 * Syntax: apedit [widevnum]
 *         apedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_apedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pAcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pAcode = get_script_index(wnum.pArea, wnum.vnum, PRG_APROG)) == NULL) {
            send_to_char("APEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &apedit_def, (void *)pAcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (apedit_create(ch, argument))
            olc_editor_enter(ch, &apedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: apedit [widevnum]\n\r", ch);
    send_to_char("        apedit create [widevnum]\n\r", ch);
}

/**
 * do_ipedit - Enter the instance/blueprint script editor
 *
 * Syntax: ipedit [widevnum]
 *         ipedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_ipedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pIcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, NULL, &wnum)) {
        if ((pIcode = get_script_index(wnum.pArea, wnum.vnum, PRG_IPROG)) == NULL) {
            send_to_char("IPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &ipedit_def, (void *)pIcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (ipedit_create(ch, argument))
            olc_editor_enter(ch, &ipedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: ipedit [widevnum]\n\r", ch);
    send_to_char("        ipedit create [widevnum]\n\r", ch);
}

/**
 * do_dpedit - Enter the dungeon script editor
 *
 * Syntax: dpedit [widevnum]
 *         dpedit create [widevnum]
 *
 * @param ch        Character entering the editor
 * @param argument  Widevnum or "create" subcommand
 */
void do_dpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pDcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, NULL, &wnum)) {
        if ((pDcode = get_script_index(wnum.pArea, wnum.vnum, PRG_DPROG)) == NULL) {
            send_to_char("DPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &dpedit_def, (void *)pDcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (dpedit_create(ch, argument))
            olc_editor_enter(ch, &dpedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: dpedit [widevnum]\n\r", ch);
    send_to_char("        dpedit create [widevnum]\n\r", ch);
}

void do_qpedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pQcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, NULL, &wnum)) {
        if ((pQcode = get_script_index(wnum.pArea, wnum.vnum, PRG_QPROG)) == NULL) {
            send_to_char("QPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &qpedit_def, (void *)pQcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (qpedit_create(ch, argument))
            olc_editor_enter(ch, &qpedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: qpedit [widevnum]\n\r", ch);
    send_to_char("        qpedit create [widevnum]\n\r", ch);
}

void do_epedit(CHAR_DATA *ch, char *argument)
{
    SCRIPT_DATA *pEcode;
    char command[MAX_INPUT_LENGTH];
    WNUM wnum;

    argument = one_argument(argument, command);

    if (parse_widevnum(command, ch->in_room->area, &wnum)) {
        if ((pEcode = get_script_index(wnum.pArea, wnum.vnum, PRG_EPROG)) == NULL) {
            send_to_char("EPEdit: That widevnum does not exist.\n\r", ch);
            return;
        }

        olc_editor_enter(ch, &epedit_def, (void *)pEcode, true);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (epedit_create(ch, argument))
            olc_editor_enter(ch, &epedit_def, ch->desc->pEdit, true);
        return;
    }

    send_to_char("Syntax: epedit [widevnum]\n\r", ch);
    send_to_char("        epedit create [widevnum]\n\r", ch);
}

/***************************************************************************
 * Command Handlers                                                        *
 ***************************************************************************/

/**
 * scriptedit_name - Set the script's display name
 *
 * Syntax: name <string>
 *
 * @param ch        Character issuing the command
 * @param argument  New name string
 * @return          true if name was changed
 */
SCRIPTEDIT(scriptedit_name)
{
    SCRIPT_DATA *pCode;
    EDIT_SCRIPT(ch, pCode);

    return olc_cmd_string(ch, argument, "name", NULL,
        &pCode->name, 0, NULL, NULL);
}

/**
 * scriptedit_code - Open the script source code editor
 *
 * Opens the multi-line string editor for the script's edit_src field.
 * If the script has the SECURED flag and the user is not an IMP,
 * the flag is removed as a security measure.
 *
 * Syntax: code
 *
 * @param ch        Character issuing the command
 * @param argument  Must be empty
 * @return          true when entering code editor (marks area dirty)
 */
SCRIPTEDIT(scriptedit_code)
{
    SCRIPT_DATA *pCode;
    EDIT_SCRIPT(ch, pCode);

    if (!argument[0]) {
        /* If they edit the code and aren't authorized, remove SECURED */
        if (IS_SET(pCode->flags, SCRIPT_SECURED) && !script_imp_check(ch)) {
            REMOVE_BIT(pCode->flags, SCRIPT_SECURED);
        }

        if (pCode->edit_src == pCode->src)
            pCode->edit_src = str_dup(pCode->src);
        string_append(ch, &pCode->edit_src);

        if (pCode->area)
            SET_BIT(pCode->area->area_flags, AREA_CHANGED);

        return true;
    }

    send_to_char("Syntax: code\n\r", ch);
    return false;
}

/**
 * scriptedit_comments - Open the builder comments editor
 *
 * Opens the multi-line string editor for the script's comments field.
 * Comments are visible to all builders but do not affect script execution.
 *
 * Syntax: comments
 *
 * @param ch        Character issuing the command
 * @param argument  Must be empty
 * @return          true (comments are always considered a change)
 */
SCRIPTEDIT(scriptedit_comments)
{
    SCRIPT_DATA *pCode;

    EDIT_SCRIPT(ch, pCode);

    if (argument[0] != '\0') {
        send_to_char("Syntax: comments\n\r", ch);
        return false;
    }

    string_append(ch, &pCode->comments);
    return true;
}

/**
 * scriptedit_compile - Compile the script source code
 *
 * Compiles the edit_src into executable bytecode. If the source hasn't
 * changed since last compile, reports "up-to-date". Scripts from builders
 * below MAX_LEVEL-1 are automatically flagged for inspection.
 *
 * Syntax: compile
 *
 * @param ch        Character issuing the command
 * @param argument  Must be empty
 * @return          true if compilation occurred
 */
SCRIPTEDIT(scriptedit_compile)
{
    SCRIPT_DATA *pCode;
    BUFFER *buffer;

    EDIT_SCRIPT(ch, pCode);

    if (!argument[0]) {
        bool compiled_ok;

        buffer = new_buf();
        if (!buffer) {
            send_to_char("WTF?! Couldn't create the buffer!\n\r", ch);
            return false;
        }

        if (ch->tot_level < (MAX_LEVEL - 1))
            pCode->flags |= SCRIPT_INSPECT;

        if (pCode->last_compile_success
            && pCode->src && pCode->edit_src
            && ((pCode->src == pCode->edit_src)
                || !str_cmp(pCode->src, pCode->edit_src))) {
            send_to_char("Script is up-to-date.  Nothing to compile.\n\r", ch);
            scriptedit_set_log_text(&pCode->last_compile_log,
                "Script is up-to-date. Nothing to compile.\n\r");
            pCode->last_compile_success = true;
            pCode->last_compile_time = current_time;
            free_buf(buffer);
            return false;
        }

        compiled_ok = compile_script(buffer, pCode, pCode->edit_src,
            olc_script_typeifc[pCode->type]);

        if (compiled_ok && !add_buf(buffer, "Script saved...\n\r")) {
            send_to_char("Compile output exceeded buffer limits.\n\r", ch);
            free_buf(buffer);
            return false;
        }

        scriptedit_set_log_text(&pCode->last_compile_log, buf_string(buffer));
        pCode->last_compile_success = compiled_ok;
        pCode->last_compile_time = current_time;

        page_to_char(buf_string(buffer), ch);
        free_buf(buffer);

        return true;
    }

    send_to_char("Syntax: compile\n\r", ch);
    return false;
}

/**
 * scriptedit_flags - Toggle script flags
 *
 * Requires level 154+ (script security check). SECURED and SYSTEM flags
 * additionally require IMP-level access to set (but not to clear).
 *
 * Syntax: flags <flag>
 *
 * @param ch        Character issuing the command
 * @param argument  Flag name to toggle
 * @return          true if a flag was toggled
 */
SCRIPTEDIT(scriptedit_flags)
{
    SCRIPT_DATA *pCode;
    int value;

    EDIT_SCRIPT(ch, pCode);

    if (argument[0]) {
        if (!script_security_check(ch)) {
            send_to_char("You must be level 154 or higher to toggle these.\n\r", ch);
            return false;
        }

        if ((value = flag_value(script_flags, argument)) != NO_FLAG) {
            if (IS_SET(value, SCRIPT_SECURED)
                && !IS_SET(pCode->flags, SCRIPT_SECURED)
                && !script_imp_check(ch)) {
                send_to_char("Insufficent security to set script as secured.\n\r", ch);
                return false;
            }

            if (IS_SET(value, SCRIPT_SYSTEM)
                && !IS_SET(pCode->flags, SCRIPT_SYSTEM)
                && !script_imp_check(ch)) {
                send_to_char("Insufficent security to set script as system.\n\r", ch);
                return false;
            }

            TOGGLE_BIT(pCode->flags, value);

            send_to_char("Script flag toggled.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: flags [flag]\n\r"
        "Type '? scriptflags' for a list of flags.\n\r", ch);
    return false;
}

/**
 * scriptedit_depth - Set the script's maximum call depth
 *
 * Controls recursion limits. "infinite" allows unlimited recursion,
 * "default" uses the system default (MAX_CALL_LEVEL), or specify
 * a positive number.
 *
 * Requires level 154+ (script security check).
 *
 * Syntax: depth <number|infinite|default>
 *
 * @param ch        Character issuing the command
 * @param argument  Depth value
 * @return          true if depth was changed
 */
SCRIPTEDIT(scriptedit_depth)
{
    SCRIPT_DATA *pCode;
    int value;

    EDIT_SCRIPT(ch, pCode);

    if (!script_security_check(ch)) {
        send_to_char("You must be level 154 or higher to set call depth.\n\r", ch);
        return false;
    }

    if (argument[0]) {
        if (is_number(argument)) {
            value = atoi(argument);
            if (value < 1) {
                send_to_char("Invalid call depth.\n\r", ch);
                send_to_char("Syntax: depth [num>0|infinite|default]\n\r", ch);
                return false;
            }
        } else if (!str_prefix(argument, "infinite"))
            value = -1;
        else if (!str_prefix(argument, "default"))
            value = 0;
        else {
            send_to_char("Invalid call depth.\n\r", ch);
            send_to_char("Syntax: depth [num>0|infinite|default]\n\r", ch);
            return false;
        }

        pCode->depth = value;

        send_to_char("Script call depth set.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: depth [num>0|infinite|default]\n\r", ch);
    return false;
}

/**
 * scriptedit_security - Set the script's security level
 *
 * Only IMPs can set security levels on scripts. Valid range is
 * MIN_SCRIPT_SECURITY (0) to MAX_SCRIPT_SECURITY (9).
 *
 * Syntax: security <0-9>
 *
 * @param ch        Character issuing the command
 * @param argument  Security level number
 * @return          true if security was changed
 */
SCRIPTEDIT(scriptedit_security)
{
    SCRIPT_DATA *pCode;
    int value;

    EDIT_SCRIPT(ch, pCode);

    if (!script_imp_check(ch)) {
        send_to_char("Only an IMP can set security on a script.\n\r", ch);
        return false;
    }

    if (is_number(argument)) {
        value = atoi(argument);
        if (value < MIN_SCRIPT_SECURITY || value > MAX_SCRIPT_SECURITY) {
            char buf[MIL];
            sprintf(buf, "Security may only be from %d to %d.\n\r",
                MIN_SCRIPT_SECURITY, MAX_SCRIPT_SECURITY);
            send_to_char(buf, ch);
            return false;
        }

        pCode->security = value;

        send_to_char("Script security set.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: security <0-9>\n\r", ch);
    return false;
}

/***************************************************************************
 * Create Functions                                                        *
 ***************************************************************************/

/**
 * mpedit_create - Create a new mob script
 *
 * Allocates a new SCRIPT_DATA for mob programs. Supports auto-vnum
 * (empty argument or "0") which finds the next available vnum in the
 * current area, or explicit vnum specification.
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(mpedit_create)
{
    SCRIPT_DATA *pMcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_MPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("MPEdit: Insufficient security to create MobProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_MPROG)) {
        send_to_char("MPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pMcode              = new_script();
    pMcode->vnum        = value;
    pMcode->next        = ad->mprog_list;
    pMcode->type        = PRG_MPROG;
    pMcode->area        = ad;
    ad->mprog_list      = pMcode;
    ch->desc->pEdit     = (void *)pMcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("MobProgram Code Created.\n\r", ch);

    return true;
}

/**
 * opedit_create - Create a new object script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(opedit_create)
{
    SCRIPT_DATA *pOcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_OPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("OPEdit: Insufficient security to create ObjProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_OPROG)) {
        send_to_char("OPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pOcode              = new_script();
    pOcode->vnum        = value;
    pOcode->next        = ad->oprog_list;
    pOcode->area        = ad;
    ad->oprog_list      = pOcode;
    pOcode->type        = PRG_OPROG;
    ch->desc->pEdit     = (void *)pOcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("ObjProgram Code Created.\n\r", ch);

    return true;
}

/**
 * rpedit_create - Create a new room script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(rpedit_create)
{
    SCRIPT_DATA *pRcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_RPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("RPEdit: Insufficient security to create RoomProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_RPROG)) {
        send_to_char("RPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pRcode              = new_script();
    pRcode->vnum        = value;
    pRcode->next        = ad->rprog_list;
    pRcode->area        = ad;
    ad->rprog_list      = pRcode;
    pRcode->type        = PRG_RPROG;
    ch->desc->pEdit     = (void *)pRcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("RoomProgram Code Created.\n\r", ch);

    return true;
}

/**
 * tpedit_create - Create a new token script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(tpedit_create)
{
    SCRIPT_DATA *pTcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_TPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("TPEdit: Insufficient security to create TokenProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_TPROG)) {
        send_to_char("TPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pTcode              = new_script();
    pTcode->vnum        = value;
    pTcode->area        = ad;
    pTcode->next        = ad->tprog_list;
    ad->tprog_list      = pTcode;
    pTcode->type        = PRG_TPROG;
    ch->desc->pEdit     = (void *)pTcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("TokenProgram Code Created.\n\r", ch);

    return true;
}

/**
 * apedit_create - Create a new area script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(apedit_create)
{
    SCRIPT_DATA *pAcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_APROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("APEdit: Insufficient security to create AreaProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_APROG)) {
        send_to_char("APEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pAcode              = new_script();
    pAcode->vnum        = value;
    pAcode->area        = ad;
    pAcode->next        = ad->aprog_list;
    ad->aprog_list      = pAcode;
    pAcode->type        = PRG_APROG;
    ch->desc->pEdit     = (void *)pAcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("AreaProgram Code Created.\n\r", ch);

    return true;
}

/**
 * ipedit_create - Create a new instance/blueprint script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(ipedit_create)
{
    SCRIPT_DATA *pIcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_IPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!can_edit_blueprints(ch)) {
        send_to_char("IPEdit: Insufficient security to create InstanceProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_IPROG)) {
        send_to_char("IPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pIcode              = new_script();
    pIcode->vnum        = value;
    pIcode->area        = ad;
    pIcode->next        = ad->iprog_list;
    ad->iprog_list      = pIcode;
    pIcode->type        = PRG_IPROG;
    ch->desc->pEdit     = (void *)pIcode;

    if (value > top_iprog_index)
        top_iprog_index = value;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("InstanceProgram Code Created.\n\r", ch);

    return true;
}

/**
 * dpedit_create - Create a new dungeon script
 *
 * @param ch        Character creating the script
 * @param argument  Widevnum or empty for auto-assign
 * @return          true if script was created
 */
SCRIPTEDIT(dpedit_create)
{
    SCRIPT_DATA *pDcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_DPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!can_edit_dungeons(ch)) {
        send_to_char("DPEdit: Insufficient security to create DungeonProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_DPROG)) {
        send_to_char("DPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pDcode              = new_script();
    pDcode->vnum        = value;
    pDcode->area        = ad;
    pDcode->next        = ad->dprog_list;
    ad->dprog_list      = pDcode;
    pDcode->type        = PRG_DPROG;
    ch->desc->pEdit     = (void *)pDcode;

    if (value > top_dprog_index)
        top_dprog_index = value;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("DungeonProgram Code Created.\n\r", ch);

    return true;
}

SCRIPTEDIT(qpedit_create)
{
    SCRIPT_DATA *pQcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_QPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_STAFF(ch, STAFF_IMPLEMENTOR)) {
        send_to_char("QPEdit: Insufficient security to create QuestProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_QPROG)) {
        send_to_char("QPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pQcode              = new_script();
    pQcode->vnum        = value;
    pQcode->area        = ad;
    pQcode->next        = ad->qprog_list;
    ad->qprog_list      = pQcode;
    pQcode->type        = PRG_QPROG;
    ch->desc->pEdit     = (void *)pQcode;

    if (value > top_qprog_index)
        top_qprog_index = value;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("QuestProgram Code Created.\n\r", ch);

    return true;
}

SCRIPTEDIT(epedit_create)
{
    SCRIPT_DATA *pEcode;
    AREA_DATA *ad;
    WNUM script_wnum;
    long value;

    if (argument[0] == '\0' || !strcmp(argument, "0")) {
        ad = ch->in_room->area;
        value = scriptedit_next_auto_vnum(ad, PRG_EPROG);
        if (value <= 0) {
            send_to_char("Unable to allocate a new script vnum.\n\r", ch);
            return false;
        }
    } else {
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(argument, context, &script_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }
        ad = script_wnum.pArea;
        value = script_wnum.vnum;
    }

    if (!IS_BUILDER(ch, ad)) {
        send_to_char("EPEdit: Insufficient security to create EventProgs.\n\r", ch);
        return false;
    }

    if (get_script_index(ad, value, PRG_EPROG)) {
        send_to_char("EPEdit: Code widevnum already exists.\n\r", ch);
        return false;
    }

    pEcode              = new_script();
    pEcode->vnum        = value;
    pEcode->area        = ad;
    pEcode->next        = ad->eprog_list;
    ad->eprog_list      = pEcode;
    pEcode->type        = PRG_EPROG;
    ch->desc->pEdit     = (void *)pEcode;

    SET_BIT(ad->area_flags, AREA_CHANGED);
    send_to_char("EventProgram Code Created.\n\r", ch);

    return true;
}

/***************************************************************************
 * List Functions                                                          *
 ***************************************************************************/

/**
 * show_script_list - Display a paginated list of scripts
 *
 * Shows scripts of a given type with their widevnum, line count, depth,
 * compilation status, and name. Supports optional range filtering.
 *
 * @param ch        Character viewing the list
 * @param argument  Optional "min max" widevnum range filter
 * @param type      PRG_* constant for script type
 */
void show_script_list(CHAR_DATA *ch, char *argument, int type)
{
    int count = 1, len;
    SCRIPT_DATA *prg;
    char buf[MSL], *noc;
    BUFFER *buffer;
    bool append_ok = true;
    long min, max;
    AREA_DATA *area, *ad;
    SCRIPT_DATA *list_head = NULL;

    area = ch->in_room->area;

    if (argument[0]) {
        char arg1[MAX_INPUT_LENGTH];
        char arg2[MAX_INPUT_LENGTH];
        WNUM wnum_min, wnum_max;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);

        if (!parse_widevnum(arg1, area, &wnum_min) || !wnum_min.pArea) {
            send_to_char("Invalid minimum widevnum format.\n\r", ch);
            return;
        }

        if (!parse_widevnum(arg2, area, &wnum_max) || !wnum_max.pArea) {
            send_to_char("Invalid maximum widevnum format.\n\r", ch);
            return;
        }

        if (type != PRG_IPROG && type != PRG_DPROG) {
            if (wnum_min.pArea != wnum_max.pArea) {
                send_to_char("Widevnum range must be within the same area for area-scoped progs.\n\r", ch);
                return;
            }
            area = wnum_min.pArea;
        }

        min = wnum_min.vnum;
        max = wnum_max.vnum;

        if (min < 1) return;
        if (max < 1) return;

        if (max < min) {
            long tmp = max;
            max = min;
            min = tmp;
        }
    } else {
        min = -1;
        max = -1;
    }

    switch (type) {
    case PRG_MPROG: list_head = area->mprog_list; break;
    case PRG_OPROG: list_head = area->oprog_list; break;
    case PRG_RPROG: list_head = area->rprog_list; break;
    case PRG_TPROG: list_head = area->tprog_list; break;
    case PRG_APROG: list_head = area->aprog_list; break;
    case PRG_IPROG: list_head = area->iprog_list ? area->iprog_list : iprog_list; break;
    case PRG_DPROG: list_head = area->dprog_list ? area->dprog_list : dprog_list; break;
    case PRG_QPROG: list_head = area->qprog_list ? area->qprog_list : qprog_list; break;
    case PRG_EPROG: list_head = area->eprog_list ? area->eprog_list : eprog_list; break;
    default: return;
    }

    if (!ch->lines)
        send_to_char("{RWARNING:{W Having scrolling off limits how many scripts you can see.{x\n\r", ch);

    buffer = new_buf();

    for (prg = list_head; prg; prg = prg->next) {
        if (min > 0 && max > 0 && (prg->vnum < min || prg->vnum > max))
            continue;

        ad = prg->area;

        len = sprintf(buf, "{B[{W%-4d{B]  ", count);

        len += sprintf(buf + len, "{W%c%c{B {G%-12.12s {W%-5d ",
            ((ad && IS_BUILDER(ch, ad)) ? 'B' : ' '),
            (IS_SET(prg->flags, SCRIPT_WIZNET) ? 'W' : ' '),
            widevnum_string_script(prg, area),
            (prg->lines > 1) ? (prg->lines - 1) : 0);

        if (prg->depth < 0)
            len += sprintf(buf + len, " {RINF ");
        else if (!prg->depth)
            len += sprintf(buf + len, " {GDEF ");
        else
            len += sprintf(buf + len, " {W%-3d ", prg->depth);

        if (IS_SET(prg->flags, SCRIPT_DISABLED))
            len += sprintf(buf + len, "{DDisabled{x   ");
        else if (prg->lines > 1 && prg->src != prg->edit_src)
            len += sprintf(buf + len, "{GModified{x   ");
        else if (prg->lines == 1)
            len += sprintf(buf + len, "{WBlank{x      ");
        else if (prg->code)
            len += sprintf(buf + len, "{xCompiled{x   ");
        else
            len += sprintf(buf + len, "{RUncompiled{x ");

        if (prg->name && *prg->name) {
            noc = nocolour(prg->name);
            len += sprintf(buf + len, "%.40s", noc);
            free_string(noc);
        }

        strcpy(buf + len, "\n\r");
        buf[len + 2] = 0;
        count++;
        if (!add_buf(buffer, buf) ||
            (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH)) {
            append_ok = false;
            break;
        }
    }

    if (count == 1) {
        if (!add_buf(buffer, "No existing scripts in that range.\n\r"))
            append_ok = false;
    } else {
        send_to_char("{BCount  BW   Widevnum      Lines Depth   Status   Name\n\r", ch);
        send_to_char("{b----------------------------------------------------------------------------\n\r", ch);
    }

    if (!append_ok)
        send_to_char("Script list output exceeded buffer limits.\n\r", ch);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

SCRIPTEDIT(mpedit_list)
{
    show_script_list(ch, argument, PRG_MPROG);
    return false;
}

SCRIPTEDIT(opedit_list)
{
    show_script_list(ch, argument, PRG_OPROG);
    return false;
}

SCRIPTEDIT(rpedit_list)
{
    show_script_list(ch, argument, PRG_RPROG);
    return false;
}

SCRIPTEDIT(tpedit_list)
{
    show_script_list(ch, argument, PRG_TPROG);
    return false;
}

SCRIPTEDIT(apedit_list)
{
    show_script_list(ch, argument, PRG_APROG);
    return false;
}

SCRIPTEDIT(ipedit_list)
{
    show_script_list(ch, argument, PRG_IPROG);
    return false;
}

SCRIPTEDIT(dpedit_list)
{
    show_script_list(ch, argument, PRG_DPROG);
    return false;
}

SCRIPTEDIT(qpedit_list)
{
    show_script_list(ch, argument, PRG_QPROG);
    return false;
}

SCRIPTEDIT(epedit_list)
{
    show_script_list(ch, argument, PRG_EPROG);
    return false;
}

/***************************************************************************
 * Standalone List Commands                                                *
 ***************************************************************************/

void do_mplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_MPROG);
}

void do_oplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_OPROG);
}

void do_rplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_RPROG);
}

void do_tplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_TPROG);
}

void do_aplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_APROG);
}

void do_iplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_IPROG);
}

void do_dplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_DPROG);
}

void do_qplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_QPROG);
}

void do_eplist(CHAR_DATA *ch, char *argument)
{
    show_script_list(ch, argument, PRG_EPROG);
}
