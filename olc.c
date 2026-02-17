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
#include "merc.h"
#include "tables.h"
#include "interp.h"
#include "olc.h"
#include "recycle.h"
#include "scripts.h"
#include "wilds.h"
#include "editors/common.h"
#include "editors/common/olc_editor.h"
#include "traits.h"
#include "class_data.h"
extern const char *medit_tab_names[];
extern GLOBAL_DATA gconfig;
/*
 *  * Local functions.
 *   */
AREA_DATA *get_area_data args ((long anum));
AREA_DATA *get_area_from_uid args ((long uid));

char *editor_name_table[] = {
    " ",
    "AEdit",
    "REdit",
    "OEdit",
    "MEdit",
    "MpEdit",
    "OpEdit",
    "RpEdit",
    "ShEdit",
    "HEdit",
    "TpEdit",
    "TEdit",
    "PEdit",
    "RSGEdit",
    "WEdit",
    "VLEdit",
    "BSEdit",
    "BPEdit",
    "DNGEdit",
    "ApEdit",
    "IpEdit",
    "DpEdit",
    "CMDEdit",
    "ChgSet",       // 23 ED_CHANGESET
    "AccNote",      // 24 ED_ACCNOTE
    "ChLog",        // 25 ED_CHLOG
    "SocEdit",      // 26 ED_SOCIAL
    "GameEdit",     // 27 ED_GAMESETTING
    "CharNote",     // 28 ED_CHARNOTE
    "RaceEdit",     // 29 ED_RACE
    "TraitEdit",    // 30 ED_TRAIT
    "SkEdit",       // 31 ED_SKILL
    "GrEdit",       // 32 ED_GROUP
    "SoEdit",       // 33 ED_SONG
    "ClsEdit",      // 34 ED_CLASS
    "LiqEdit",      // 35 ED_LIQUID
    "MatEdit",      // 36 ED_MATERIAL
    "CorpsEdit",    // 37 ED_CORPSE
    "SectorEdit",   // 38 ED_SECTOR
    "RepEdit",      // 39 ED_REPUTATION
};

int editor_max_tabs_table[] = {
    0,		// -----
    0,		// AEdit
    0,		// REdit
    0,		// OEdit
    0,		// MEdit
    0,		// MpEdit
    0,		// OpEdit
    0,		// RpEdit
    0,		// ShEdit
    0,		// HEdit
    0,		// TpEdit
    0,		// TEdit
    0,		// PEdit
    0,		// RSGEdit
    0,		// WEdit
    0,		// VLEdit
    0,		// BSEdit
    0,		// BPEdit
    0,		// DNGEdit
    0,		// ApEdit
    0,		// IpEdit
    0,		// DpEdit
    0,		// CMDEdit
    0,		// ChgSet
    0,		// AccNote
    0,		// ChLog
    0,		// SocEdit
    0,		// GameEdit
    0,		// CharNote
    0,		// RaceEdit
    0,		// TraitEdit
    0,		// SkEdit
    0,		// GrEdit
    0,		// SoEdit
    0,		// ClsEdit
    0,		// LiqEdit
    0,		// MatEdit
    0,		// CorpsEdit
    0,		// SectorEdit
    0,		// RepEdit
};

const struct editor_cmd_type editor_table[] =
{
    { "area",		do_aedit	},
    { "room",		do_redit	},
    { "object",		do_oedit	},
    { "mobile",		do_medit	},
    { "mprog",		do_mpedit	},
    { "oprog",		do_opedit	},
    { "rprog",		do_rpedit	},
    { "ship",		do_shedit	},
    { "help",		do_hedit	},
    { "token",		do_tedit	},
    { "tprog",		do_tpedit	},
    { "project",	do_pedit	},
    { "rsg",         do_rsgedit   },
    { "bpsect",		do_bsedit	},
    { "blueprint",	do_bpedit	},
    { "dungeon",	do_dngedit	},
    { "aprog",		do_apedit	},
    { "iprog",		do_ipedit	},
    { "dprog",		do_dpedit	},
    { "wilderness",    do_wedit	},
    { "command",    do_cmdedit  },
    { "race",       do_racedit  },
    { "trait",      do_traitedit },
    { "skill",      do_skedit    },
    { "group",      do_gredit    },
    { "song",       do_soedit    },
    { "class",      do_clsedit   },
    { "liquid",     do_liqedit   },
    { "material",   do_matedit   },
    { "corpse",     do_corpsedit },
    { "sector",     do_sectoredit },
    { "reputation", do_repedit   },
    { NULL,			0,			}
};


/* All editor command tables and interpreter functions have been moved to
 * their respective editor files.  The framework editor registry
 * (olc_editor_interp / olc_find_editor_by_type) dispatches commands
 * without needing centralized switch statements or extern tables here. */


/* Executed from comm.c.  Minimizes compiling when changes are made. */
bool run_olc_editor(DESCRIPTOR_DATA *d)
{
    // No command should have a space, so no need for quoting.
    // No OLC command should start with ' or ".
    if (d->incomm[0] == '\'' || d->incomm[0] == '"') return false;

    /* Try the framework editor registry first.  Editors auto-register
     * via olc_editor_enter() or olc_editor_interp(), so any editor
     * that has been opened at least once will be found here. */
    const OLC_EDITOR_DEF *def = olc_find_editor_by_type(d->editor);
    if (def) {
        olc_editor_interp(d->character, d->incomm, def);
        return true;
    }

    /* Fallback for non-framework editors */
    switch (d->editor)
    {
    case ED_HELP:
        hedit(d->character, d->incomm);
        break;

    default:
        return false;
    }
    return true;
}


// Return the edit name of character's editor (%o in prompt)
char *olc_ed_name(CHAR_DATA *ch)
{
    if(ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
        return editor_name_table[ch->desc->editor];

    return editor_name_table[0];
}

int olc_ed_tabs(CHAR_DATA *ch)
{
    if(ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
        return editor_max_tabs_table[ch->desc->editor];

    return 0;
}

void olc_set_editor(CHAR_DATA *ch, int editor, void *data)
{
    ch->desc->pEdit = data;
    ch->desc->editor = editor;
    ch->desc->nEditTab = 0;
    ch->desc->nMaxEditTabs = olc_ed_tabs(ch);
}

void olc_show_item(CHAR_DATA *ch, void *data, OLC_FUN *show_fun, char *argument)
{
    int old_tab = ch->desc->nEditTab;
    void *old_data = ch->desc->pEdit;

    ch->desc->nEditTab = 0;
    ch->desc->pEdit = data;

    (*show_fun)(ch, argument);

    ch->desc->nEditTab = old_tab;
    ch->desc->pEdit = old_data;
}

// Return the edit vnum of character's editor (%O in prompt)
char *olc_ed_vnum(CHAR_DATA *ch)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *pRoom;
    OBJ_INDEX_DATA *pObj;
    MOB_INDEX_DATA *pMob;
    SCRIPT_DATA *prog;
    PROJECT_DATA *project;
    HELP_DATA *help;
    SHIP_INDEX_DATA *pShip;
    TOKEN_INDEX_DATA *pTokenIndex;
    WILDS_DATA *pWilds;
    BLUEPRINT_SECTION *bpsect;
    BLUEPRINT *blueprint;
    DUNGEON_INDEX_DATA *dungeon;
    CMD_DATA *command;
    static char buf[64];
    char buf2[MSL];

    buf[0] = '\0';
    switch (ch->desc->editor)
    {
    case ED_AREA:
        pArea = (AREA_DATA *)ch->desc->pEdit;
        sprintf(buf, "%ld", pArea ? pArea->anum : 0);
        break;
    case ED_ROOM:
        pRoom = ch->in_room;
        sprintf(buf, "%s", pRoom ? widevnum_string_room(pRoom, NULL) : "0");
        break;
    case ED_OBJECT:
        pObj = (OBJ_INDEX_DATA *)ch->desc->pEdit;
        sprintf(buf, "%s", pObj ? widevnum_string_object(pObj, NULL) : "0");
        break;
    case ED_MOBILE:
        pMob = (MOB_INDEX_DATA *)ch->desc->pEdit;
        sprintf(buf, "%s", pMob ? widevnum_string_mobile(pMob, NULL) : "0");
        break;
    case ED_MPCODE:
    case ED_OPCODE:
    case ED_RPCODE:
    case ED_TPCODE:
    case ED_APCODE:
    case ED_IPCODE:
    case ED_DPCODE:
        prog = (SCRIPT_DATA *)ch->desc->pEdit;
        sprintf(buf, "%s", prog ? widevnum_string_script(prog, NULL) : "0");
        break;
    case ED_HELP:
        {
        HELP_CATEGORY *hCat;

        help = (HELP_DATA *)ch->desc->pEdit;

        if (help != NULL)
        {
            hCat = help->hCat;
            sprintf(buf, "{x%s", help ? help->keyword : "");

            for (hCat = help->hCat; hCat->up != NULL; hCat = hCat->up)
            {
            sprintf(buf2, "{W%s{B/", hCat->name);
            strcat(buf2, buf);
            strcpy(buf, buf2);
            }

            sprintf(buf2, "{B/{x");
            strcat(buf2, buf);
            strcpy(buf, buf2);
        }
        else
        {
            HELP_CATEGORY *hCatTmp;

            hCat = ch->desc->hCat;

            sprintf(buf, "{W%s{B/{x", ch->desc->hCat->name);

            for (hCatTmp = hCat->up; hCatTmp != NULL; hCatTmp = hCatTmp->up) {
            sprintf(buf2, "{W%s{B/{x", hCatTmp->name);
            strcat(buf2, buf);
            strcpy(buf, buf2);
            }
        }
        }
        break;

    case ED_PROJECT:
        project = (PROJECT_DATA *)ch->desc->pEdit;
        if (project != NULL)
        sprintf(buf, "%s", project->name);
        else
        sprintf(buf, "None");

        break;

    case ED_RSG:
        {
            RANDOM_STRING *rsg = (RANDOM_STRING *)ch->desc->pEdit;
            if (rsg)
                sprintf(buf, "%ld:%s", rsg->uid, rsg->name ? rsg->name : "");
            else
                sprintf(buf, "--");
        }
        break;

    case ED_SHIP:
        pShip = (SHIP_INDEX_DATA *)ch->desc->pEdit;
        sprintf(buf, "%s", pShip ? widevnum_string_ship(pShip, NULL) : "0");
        break;

    case ED_TOKEN:
        pTokenIndex = (TOKEN_INDEX_DATA *) ch->desc->pEdit;
        sprintf(buf, "%s", pTokenIndex ? widevnum_string_token(pTokenIndex, NULL) : "0");
        break;

/* VIZZWILDS */
    case ED_WILDS:
        pWilds = (WILDS_DATA *)ch->desc->pEdit;
        sprintf(buf, "%ld", pWilds ? pWilds->uid : 0);
        break;

    case ED_BPSECT:
        bpsect = (BLUEPRINT_SECTION *)ch->desc->pEdit;
        sprintf(buf, "%s", bpsect ? widevnum_string_blueprint_section(bpsect, NULL) : "0");
        break;

    case ED_BLUEPRINT:
        blueprint = (BLUEPRINT *)ch->desc->pEdit;
        sprintf(buf, "%s", blueprint ? widevnum_string_blueprint(blueprint, NULL) : "0");
        break;

    case ED_DUNGEON:
        dungeon = (DUNGEON_INDEX_DATA*)ch->desc->pEdit;
        sprintf(buf, "%s", dungeon ? widevnum_string_dungeon(dungeon, NULL) : "0");
        break;

    case ED_CMDEDIT:
        command = (CMD_DATA *)ch->desc->pEdit;
        if (command)
            sprintf(buf, "%s", command->name);
        else
            sprintf(buf, "--");
        break;

    case ED_SOCIAL:
        {
            struct social_type *social_ed = (struct social_type *)ch->desc->pEdit;
            if (social_ed && social_ed->name[0] != '\0')
                sprintf(buf, "%s", social_ed->name);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_RACE:
        {
            RACE_DATA *race_ed = (RACE_DATA *)ch->desc->pEdit;
            if (race_ed)
                sprintf(buf, "%s", race_ed->id);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_TRAIT:
        {
            TRAIT_DEF *trait_ed = (TRAIT_DEF *)ch->desc->pEdit;
            if (trait_ed)
                sprintf(buf, "%s", trait_ed->id);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_SKILL:
        {
            SKILL_DATA *sk_ed = (SKILL_DATA *)ch->desc->pEdit;
            if (sk_ed)
                sprintf(buf, "%s", sk_ed->name);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_GROUP:
        {
            SKILL_GROUP *gr_ed = (SKILL_GROUP *)ch->desc->pEdit;
            if (gr_ed)
                sprintf(buf, "%s", gr_ed->name);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_SONG:
        {
            SONG_DATA *so_ed = (SONG_DATA *)ch->desc->pEdit;
            if (so_ed)
                sprintf(buf, "%s", so_ed->name);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_CLASS:
        {
            CLASS_DATA *cls_ed = (CLASS_DATA *)ch->desc->pEdit;
            if (cls_ed)
                sprintf(buf, "%s", cls_ed->name);
            else
                sprintf(buf, "--");
        }
        break;

    case ED_SECTOR:
        {
            int sector_index = (int)((intptr_t)ch->desc->pEdit) - 1;
            if (sector_index >= 0 && sector_index < sector_count())
                snprintf(buf, sizeof(buf), "%d:%s", sector_index, sector_name(sector_index));
            else
                sprintf(buf, "--");
        }
        break;

    case ED_REPUTATION:
        {
            REPUTATION_INDEX_DATA *rep_ed = (REPUTATION_INDEX_DATA *)ch->desc->pEdit;
            if (rep_ed)
                snprintf(buf, sizeof(buf), "%s", rep_ed->name ? rep_ed->name : "(unnamed)");
            else
                sprintf(buf, "--");
        }
        break;

    default:
        sprintf(buf, " ");
        break;
    }

    return buf;
}


/* Format up the commands from given table. */
void show_olc_cmds(CHAR_DATA *ch, const struct olc_cmd_type *olc_table)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH ];
    int  cmd;
    int  col;

    buf1[0] = '\0';
    col = 0;
    for (cmd = 0; olc_table[cmd].name != NULL; cmd++)
    {
    sprintf(buf, "%-15.15s", olc_table[cmd].name);
    strcat(buf1, buf);
    if (++col % 5 == 0)
        strcat(buf1, "\n\r");
    }

    if (col % 5 != 0)
    strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
}


/* Display all OLC commands for your current editor */
bool show_commands(CHAR_DATA *ch, char *argument)
{
    /* Try framework registry — covers all migrated editors */
    const OLC_EDITOR_DEF *def = olc_find_editor_by_type(ch->desc->editor);
    if (def && def->cmd_table) {
        show_olc_cmds(ch, def->cmd_table);
        return false;
    }

    /* Fallback for non-framework editors */
    switch (ch->desc->editor)
    {
    case ED_HELP:
        show_olc_cmds(ch, hedit_table);
        break;
    }

    return false;
}

// Given "anum" of an area, retrieve its area struct
AREA_DATA *get_area_data(long anum)
{
    AREA_DATA *pArea;
    int safety = 0;
    const int MAX_AREAS = 10000; // Safety limit to detect circular references
    AREA_DATA *last_area = NULL;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        if (pArea->anum == anum)
            return pArea;
        
        // Check for circular reference (area points to itself or back to a previous area)
        if (pArea == pArea->next || pArea == area_first)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, 
                "get_area_data: Direct circular reference detected! Area %ld (%s) points to itself or area_first",
                pArea->anum, pArea->name ? pArea->name : "NULL");
            return NULL;
        }
        
        // Safety check for too many iterations
        if (++safety > MAX_AREAS)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, 
                "get_area_data: Infinite loop detected in area list (looking for anum %ld)", anum);
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                "Last area checked: anum=%ld name='%s' next=%p",
                pArea->anum, pArea->name ? pArea->name : "NULL", pArea->next);
            if (last_area) {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                    "Previous area: anum=%ld name='%s' next=%p",
                    last_area->anum, last_area->name ? last_area->name : "NULL", last_area->next);
            }
            return NULL;
        }
        
        last_area = pArea;
    }

    return 0;
}

// Given "uid" of an area, retrieve its area struct
AREA_DATA *get_area_from_uid (long uid)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        if (pArea->uid == uid)
            return pArea;
    }

    return 0;
}


/* Resets builder information on completion. */
bool edit_done(CHAR_DATA *ch)
{
    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit = NULL;
    ch->desc->hCat = NULL;
    ch->desc->editor = 0;
    return false;
}


bool has_access_area(CHAR_DATA *ch, AREA_DATA *area)
{
    if (ch->tot_level == MAX_LEVEL)
    return true;

    if (!IS_BUILDER(ch, area))
    return false;

    return true;
}


// The interpreters are below


// Entry points for all editors are below
void do_olc(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    int  cmd;

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, command);

    if (command[0] == '\0')
    {
        do_help(ch, "olc");
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    /* Search Table and Dispatch Command. */
    for (cmd = 0; editor_table[cmd].name != NULL; cmd++)
    {
    if (!str_prefix(command, editor_table[cmd].name))
    {
        ch->pcdata->immortal->last_olc_command = current_time;
        (*editor_table[cmd].do_fun) (ch, argument);
        return;
    }
    }

    /* Invalid command, send help. */
    do_help(ch, "olc");
}


void display_resets(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA	*pRoom;
    RESET_DATA		*pReset;
    MOB_INDEX_DATA	*pMob = NULL;
    char 		buf   [ MAX_STRING_LENGTH ];
    char 		final [ MAX_STRING_LENGTH ];
    int 		iReset = 0;

    EDIT_ROOM_VOID(ch, pRoom);
    AREA_DATA *pArea = pRoom->area;
    final[0]  = '\0';

    send_to_char (
    " No.  Loads    Description       Location         Vnum   Mx Mn Description"
    "\n\r"
    "==== ======== ============= =================== ======== ===== ==========="
    "\n\r", ch);

    for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
    {
    OBJ_INDEX_DATA  *pObj;
    MOB_INDEX_DATA  *pMobIndex;
    OBJ_INDEX_DATA  *pObjIndex;
    OBJ_INDEX_DATA  *pObjToIndex;
    ROOM_INDEX_DATA *pRoomIndex;

    final[0] = '\0';
    sprintf(final, "[%2d] ", ++iReset);

    switch (pReset->command)
    {
        default:
        sprintf(buf, "Bad reset command: %c.", pReset->command);
        strcat(final, buf);
        break;

        case 'M':
        {
        // Support both legacy (pArea=NULL, use current area) and new (pArea set, cross-area)
        if (!(pMobIndex = get_mob_index(pReset->arg1.wnum.pArea ? pReset->arg1.wnum.pArea : pRoom->area, pReset->arg1.wnum.vnum)))
        {
            sprintf(buf, "Load Mobile - Bad Mob %s\n\r", 
                widevnum_string_wnum(pReset->arg1.wnum, pArea));
            strcat(final, buf);
            continue;
        }

        if (!(pRoomIndex = get_room_index(pRoom->area, pReset->arg3.value)))
        {
            sprintf(buf, "Load Mobile - Bad Room %ld\n\r", 
                pReset->arg3.value);
            strcat(final, buf);
            continue;
        }

        pMob = pMobIndex;
        sprintf(buf, "M[%s] %-13.13s in room             R[%s] %2ld-%2ld %-15.15s\n\r",
               widevnum_string_mobile(pMobIndex, pArea), pMob->short_descr, 
               widevnum_string_room(pRoomIndex, pArea),
               pReset->arg2, pReset->arg4, pRoomIndex->name);
        strcat(final, buf);

        /*
         * Check for pet shop.
         * -------------------
         */
        {
            ROOM_INDEX_DATA *pRoomIndexPrev;

            pRoomIndexPrev = get_room_index(pRoomIndex->area, pRoomIndex->vnum - 1);
            if (pRoomIndexPrev
            && IS_SET(pRoomIndexPrev->room_flag[0], ROOM_PET_SHOP))
            final[5] = 'P';
        }

        break;
        }

        case 'O':
        {
        // Support both legacy (pArea=NULL, use current area) and new (pArea set, cross-area)
        if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea ? pReset->arg1.wnum.pArea : pRoom->area, pReset->arg1.wnum.vnum)))
        {
            sprintf(buf, "Load Object - Bad Object %s\n\r",
                widevnum_string_wnum(pReset->arg1.wnum, pArea));
            strcat(final, buf);
            continue;
        }

        pObj       = pObjIndex;

        if (!(pRoomIndex = get_room_index(pRoom->area, pReset->arg3.value)))
        {
            sprintf(buf, "Load Object - Bad Room %ld\n\r", 
                pReset->arg3.value);
            strcat(final, buf);
            continue;
        }

        sprintf(buf, "O[%s] %-13.13s in room             "
              "R[%s]       %-15.15s\n\r",
              widevnum_string_object(pObjIndex, pArea), pObj->short_descr,
              widevnum_string_room(pRoomIndex, pArea), pRoomIndex->name);
        strcat(final, buf);
        break;
        }

        case 'P':
        {
        // Support both legacy (pArea=NULL, use current area) and new (pArea set, cross-area)
        if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea ? pReset->arg1.wnum.pArea : pRoom->area, pReset->arg1.wnum.vnum)))
        {
            sprintf(buf, "Put Object - Bad Object %s\n\r",
                widevnum_string_wnum(pReset->arg1.wnum, pArea));
            strcat(final, buf);
            continue;
        }

        pObj       = pObjIndex;

        if (!(pObjToIndex = get_obj_index(pReset->arg3.wnum.pArea ? pReset->arg3.wnum.pArea : pRoom->area, pReset->arg3.wnum.vnum)))
        {
            sprintf(buf, "Put Object - Bad To Object %s\n\r",
                widevnum_string_wnum(pReset->arg3.wnum, pArea));
            strcat(final, buf);
            continue;
        }

        sprintf(buf,
            "O[%s] %-13.13s inside              O[%s] %2ld-%2ld %-15.15s\n\r",
            widevnum_string_object(pObjIndex, pArea),
            pObj->short_descr,
            widevnum_string_object(pObjToIndex, pArea),
            pReset->arg2,
            pReset->arg4,
            pObjToIndex->short_descr);
        strcat(final, buf);

        break;
        }

        case 'G':
        case 'E':
        {
        // Support both legacy (pArea=NULL, use current area) and new (pArea set, cross-area)
        if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea ? pReset->arg1.wnum.pArea : pRoom->area, pReset->arg1.wnum.vnum)))
        {
            sprintf(buf, "Give/Equip Object - Bad Object %s\n\r",
                widevnum_string_wnum(pReset->arg1.wnum, pArea));
            strcat(final, buf);
            continue;
        }
        pObj       = pObjIndex;

        if (!pMob)
        {
            sprintf(buf, "Give/Equip Object - No Previous Mobile\n\r");
            strcat(final, buf);
            break;
        }

        if (pMob->pShop)
        {
        sprintf(buf,
            "O[%s] %-13.13s in the inventory of S[%s]       %-15.15s\n\r",
            widevnum_string_object(pObjIndex, pArea),
            pObj->short_descr,
            widevnum_string_mobile(pMobIndex, pArea),
            pMob->short_descr );
        }
        else
        sprintf(buf,
            "O[%s] %-13.13s %-19.19s M[%ld]       %-15.15s\n\r",
            widevnum_string_object(pObjIndex, pArea),
            pObj->short_descr,
            (pReset->command == 'G') ?
            flag_string(wear_loc_strings, WEAR_NONE)
              : flag_string(wear_loc_strings, pReset->arg3.value),
              pMob->vnum,
              pMob->short_descr);
        strcat(final, buf);

        break;
        }

        /*
         * Doors are set in rs_flags don't need to be displayed.
         * If you want to display them then uncomment the new_reset
         * line in the case 'D' in load_resets in db.c and here.
         */
         /*
        case 'D':
        pRoomIndex = get_room_index(pReset->arg1);
        sprintf(buf, "R[%5ld] %s door of %-19.19s reset to %s\n\r",
            pReset->arg1,
            capitalize(dir_name[ pReset->arg2 ]),
            pRoomIndex->name,
            flag_string(door_resets, pReset->arg3));
        strcat(final, buf);

        break;
        */
        /*
         * End Doors Comment.
         */
        case 'R':
        {
        if (!(pRoomIndex = get_room_index(pRoom->area, pReset->arg1.value)))
        {
            sprintf(buf, "Randomize Exits - Bad Room %ld\n\r",
            pReset->arg1.value);
            strcat(final, buf);
            continue;
        }

        sprintf(buf, "R[%5ld] Exits are randomized in %s\n\r",
            pReset->arg1.value, pRoomIndex->name);
        strcat(final, buf);

        break;
        }
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    send_to_char(final, ch);
    }
}



void add_reset(ROOM_INDEX_DATA *room, RESET_DATA *pReset, int index)
{
    RESET_DATA *reset;
    int iReset = 0;

    if (!room->reset_first)
    {
    room->reset_first	= pReset;
    room->reset_last	= pReset;
    pReset->next		= NULL;
    return;
    }

    index--;

    if (index == 0)	/* First slot (1) selected. */
    {
    pReset->next = room->reset_first;
    room->reset_first = pReset;
    return;
    }

    // If negative slot(<= 0 selected) then this will find the last.
    for (reset = room->reset_first; reset->next; reset = reset->next)
    {
    if (++iReset == index)
        break;
    }

    pReset->next	= reset->next;
    reset->next		= pReset;
    if (!pReset->next)
    room->reset_last = pReset;
}

// Helper to check if argument is a valid vnum format (number or widevnum)
static bool is_valid_vnum_format(const char *arg)
{
    if (!arg || !*arg)
        return false;
    
    // Check for widevnum format (contains '#')
    if (strchr(arg, '#'))
        return true;
    
    // Check for bare number
    return is_number(arg);
}


void do_resets(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    char arg5[MAX_INPUT_LENGTH];
    char arg6[MAX_INPUT_LENGTH];
    char arg7[MAX_INPUT_LENGTH];
    RESET_DATA *pReset = NULL;
    AREA_DATA *area;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);
    argument = one_argument(argument, arg7);

    if (!IS_BUILDER(ch, ch->in_room->area))
    {
    send_to_char("Resets: Invalid security for editing this area.\n\r", ch);
    return;
    }

    area = ch->in_room->area;

    /* show resets */
    if (arg1[0] == '\0')
    {
    if (ch->in_room->reset_first)
    {
        send_to_char(
        "Resets: M = mobile, R = room, O = object, "
        "P = pet, S = shopkeeper\n\r", ch);
        display_resets(ch);
    }
    else
        send_to_char("No resets in this room.\n\r", ch);
    }

    /* take index number and search for commands */
    if (is_number(arg1))
    {
    ROOM_INDEX_DATA *pRoom = ch->in_room;

    /* delete a reset */
    if (!str_cmp(arg2, "delete"))
    {
        long insert_loc = atol(arg1);

        if (!ch->in_room->reset_first)
        {
        send_to_char("No resets in this area.\n\r", ch);
        return;
        }

        if (insert_loc - 1 <= 0)
        {
        pReset = pRoom->reset_first;
        pRoom->reset_first = pRoom->reset_first->next;
        if (!pRoom->reset_first)
            pRoom->reset_last = NULL;
        }
        else
        {
        long iReset = 0;
        RESET_DATA *prev = NULL;

        for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
        {
            if (++iReset == insert_loc)
            break;

            prev = pReset;
        }

        if (!pReset)
        {
            send_to_char("Reset not found.\n\r", ch);
            return;
        }

        if (prev)
            prev->next = prev->next->next;
        else
            pRoom->reset_first = pRoom->reset_first->next;

        for (pRoom->reset_last = pRoom->reset_first;
              pRoom->reset_last->next;
              pRoom->reset_last = pRoom->reset_last->next);
        }

        free_reset_data(pReset);
        send_to_char("Reset deleted.\n\r", ch);
        SET_BIT(area->area_flags, AREA_CHANGED);
    }
    else
    /* add a reset */
    // Accept widevnum formats: #vnum, uid#vnum, AreaName#vnum, or bare vnum
    if ((!str_cmp(arg2, "mob") && is_valid_vnum_format(arg3))
      || (!str_cmp(arg2, "obj") && is_valid_vnum_format(arg3)))
    {
        if (!str_cmp(arg2, "mob"))
        {
        WNUM mob_wnum;
        // Use NULL context for bare vnums (legacy global lookup), current area for widevnum formats
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(arg3, context, &mob_wnum))
        {
            send_to_char("Invalid mob vnum format. Use: vnum, #vnum, uid#vnum, or 'AreaName'#vnum\n\r", ch);
            return;
        }
        
        if (!mob_wnum.pArea)
        {
            send_to_char("Could not find area for that vnum.\n\r", ch);
            return;
        }
        
        if (get_mob_index(mob_wnum.pArea, mob_wnum.vnum) == NULL)
        {
            send_to_char("Mob no existe.\n\r",ch);
            return;
        }
        
        pReset = new_reset_data();
        pReset->command = 'M';
        pReset->arg1.wnum.pArea = mob_wnum.pArea;
        pReset->arg1.wnum.vnum = mob_wnum.vnum;
        pReset->arg2 = is_number(arg4) ? atol(arg4) : 1; /* Max # */
        pReset->arg3.value = ch->in_room->vnum;
        pReset->arg4 = is_number(arg5) ? atol(arg5) : 1; /* Min # */
        }
        else
        if (!str_cmp(arg2, "obj"))
        {
        WNUM obj_wnum;
        // Use NULL context for bare vnums (legacy global lookup), current area for widevnum formats
        AREA_DATA *context = ch->in_room->area;
        if (!parse_widevnum(arg3, context, &obj_wnum))
        {
            send_to_char("Invalid object vnum format. Use: vnum, #vnum, uid#vnum, or 'AreaName'#vnum\n\r", ch);
            return;
        }
        
        if (!obj_wnum.pArea)
        {
            send_to_char("Could not find area for that vnum.\n\r", ch);
            return;
        }
        
        pReset = new_reset_data();
        pReset->arg1.wnum.pArea = obj_wnum.pArea;
        pReset->arg1.wnum.vnum = obj_wnum.vnum;
        
        if (!str_prefix(arg4, "inside"))
        {
            OBJ_INDEX_DATA *temp;
            WNUM container_wnum;
            // Use NULL context for bare vnums (legacy global lookup), current area for widevnum formats
            AREA_DATA *context = ch->in_room->area;
            
            if (!parse_widevnum(arg5, context, &container_wnum))
            {
                send_to_char("Invalid container vnum format.\n\r", ch);
                return;
            }
            
            if (!container_wnum.pArea)
            {
                send_to_char("Could not find area for container vnum.\n\r", ch);
                return;
            }
            
            temp = get_obj_index(container_wnum.pArea, container_wnum.vnum);
            if (temp == NULL) {
                send_to_char("Object not found!\n\r", ch);
                return;
            }

            if ((temp->item_type != ITEM_CONTAINER) &&
                 (temp->item_type != ITEM_CORPSE_NPC))
            {
                send_to_char("Object 2 isn't a container.\n\r", ch);
                return;
            }
            pReset->command = 'P';
            pReset->arg2    = is_number(arg6) ? atol(arg6) : 1;
            pReset->arg3.wnum.pArea = container_wnum.pArea;
            pReset->arg3.wnum.vnum = container_wnum.vnum;
            pReset->arg4    = is_number(arg7) ? atol(arg7) : 1;
        }
        else
        if (!str_cmp(arg4, "room"))
        {
            if (get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL)
            {
                send_to_char("Vnum does not exist.\n\r",ch);
                return;
            }
            pReset->command  = 'O';
            pReset->arg2     = 0;
            pReset->arg3.value = ch->in_room->vnum;  // Room vnum uses .value
            pReset->arg4     = 0;
        }
        else
        {
            if (flag_value(wear_loc_flags, arg4) == NO_FLAG)
            {
                // Hack because WEAR_LIGHT is same value as NO_FLAG
                if (str_cmp(arg4, "light"))
                {
                    send_to_char("Resets: '? wear-loc'\n\r", ch);
                    return;
                }
            }

            if (get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL)
            {
                send_to_char("Vnum does not exist.\n\r",ch);
                return;
            }
            
            // arg3 for wear location uses .value
            pReset->arg3.value =
                (!str_cmp(arg4, "light")) ?
                WEAR_LIGHT :
                flag_value(wear_loc_flags, arg4);

            if (pReset->arg3.value == WEAR_NONE)
                pReset->command = 'G';
            else
                pReset->command = 'E';
        }
        }

        add_reset(ch->in_room, pReset, atol(arg1));
        SET_BIT(ch->in_room->area->area_flags, AREA_CHANGED);
        send_to_char("Reset added.\n\r", ch);
    }
    else
    {
        send_to_char("Syntax: RESET <number> OBJ <vnum> <wear_loc>\n\r", ch);
        send_to_char("        RESET <number> OBJ <vnum> inside <vnum> [limit] [count]\n\r", ch);
        send_to_char("        RESET <number> OBJ <vnum> room\n\r", ch);
        send_to_char("        RESET <number> MOB <vnum> [max #x area] [max #x room]\n\r", ch);
        send_to_char("        RESET <number> DELETE\n\r", ch);
    }
    }
}


void do_asearch(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char result[MAX_STRING_LENGTH*2];
    AREA_DATA *pArea;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("Search through area list for which keyword?\n\r", ch);
    return;
    }

    sprintf(result, "[%3s] [%-27s] (%-5s-%5s) [%-10s] %3s [%-9s]\n\r",
       "Num", "Area Name", "lvnum", "uvnum", "Filename", "Sec", "Builders");


    for (pArea = area_first; pArea; pArea = pArea->next)
    {
    if (!str_infix(arg, pArea->name))
    {
        sprintf(buf,
        "[%3ld] %-27.27s (%-5ld-%5ld) %-12.12s [%d] [%-10.10s]\n\r",
            pArea->anum,
        pArea->name,
        pArea->min_vnum,
        pArea->max_vnum,
        pArea->file_name,
        pArea->security,
        pArea->builders);
        strcat(result, buf);
    }
    }

    send_to_char(result, ch);
}


void do_alist(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    AREA_DATA *pArea;
    BUFFER *buffer;
    int place_type = 0;

    buffer = new_buf();

    sprintf(buf, "[%-7s] [%-7s] [%-26.26s] (%-7s-%7s) [%-10s] %3s [%-10s]\n\r",
       "Anum", "UID", "Area Name", "lvnum", "uvnum", "Filename", "Sec", "Builders");
    add_buf(buffer, buf);

    if (argument[0] != '\0'
    && (place_type = flag_value(place_flags, argument)) == NO_FLAG)
    {
    send_to_char("Syntax: alist\n\r"
                 "        alist <placetype>\n\r", ch);
    return;
    }

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
    if (place_type == 0 || (pArea->place_flags == place_type))
    {
    sprintf(buf, "{D[{x%7ld{D]{x {D[{x%7ld{D]{x %s%-26.26s%s {D({x%-7ld{D-{x%7ld{D){x %-12.12s {D[{x{B%d{x{D]{x {D[{x%-10.10s{D]{x \n\r",
         pArea->anum,
         pArea->uid,
         pArea->open ? "{G" : "{R",
         pArea->name,
         "{x",
         pArea->min_vnum,
         pArea->max_vnum,
         pArea->file_name,
         pArea->security,
         pArea->builders);
    add_buf(buffer, buf);
    }
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/*
 * Copy a room.
 */
void do_rcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    ROOM_INDEX_DATA *old_room;
    ROOM_INDEX_DATA *new_room;
    EXTRA_DESCR_DATA *ed;
    EXTRA_DESCR_DATA *new_ed;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    WNUM wnum_old, wnum_new;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: rcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
        send_to_char("Invalid source vnum format.\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
        send_to_char("Invalid target vnum format.\n\r", ch);
	return;
    }

    if ((old_room = get_room_index(wnum_old.pArea, wnum_old.vnum)) == NULL)
    {
        send_to_char("That room doesn't exist.\n\r", ch);
	return;
    }

    if (get_room_index(wnum_new.pArea, wnum_new.vnum) != NULL)
    {
        send_to_char("That room vnum is already taken.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
        send_to_char("You're not a builder in that area, so you can't "
             "copy from it.\n\r", ch);
	return;
    }

    area = wnum_new.pArea;

    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    old_room = get_room_index(wnum_old.pArea, wnum_old.vnum);
    new_room = new_room_index();

    new_room->area                 = area;
    list_appendlink(area->room_list, new_room);	// Add to the area room list

    new_room->vnum                 = wnum_new.vnum;
    if (wnum_new.vnum > top_vnum_room)
        top_vnum_room = wnum_new.vnum;

    iHash                       = wnum_new.vnum % MAX_KEY_HASH;
    new_room->next              = area->room_index_hash[iHash];
    area->room_index_hash[iHash] = new_room;
    ch->desc->pEdit             = (void *)new_room;
    ch->desc->editor		= ED_ROOM;

    // Copy extra descs
    for (ed = old_room->extra_descr; ed != NULL; ed = ed->next)
    {
    new_ed = new_extra_descr();
    new_ed->keyword = str_dup(ed->keyword);
    if( ed->description )
        new_ed->description = str_dup(ed->description);
    else
        new_ed->description = NULL;
    new_ed->next = new_room->extra_descr;
    new_room->extra_descr = new_ed;
    }

    new_room->name = str_dup(old_room->name);
    new_room->description = str_dup(old_room->description);
    new_room->owner = str_dup(old_room->owner);
    new_room->room_flag[0] = old_room->room_flag[0];
    new_room->room_flag[1] = old_room->room_flag[1];
    room_set_sector_type(new_room, room_sector_type(old_room));
    new_room->heal_rate = old_room->heal_rate;
    new_room->mana_rate = old_room->mana_rate;
    new_room->move_rate = old_room->move_rate;
    new_room->comments = old_room->comments;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("Room copied.\n\r", ch);
}


/*
 * Copy a mob.
 */
void do_mcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    MOB_INDEX_DATA *old_mob;
    MOB_INDEX_DATA *new_mob;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    WNUM wnum_old, wnum_new;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Syntax: mcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
	send_to_char("Invalid source vnum format.\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
	send_to_char("Invalid target vnum format.\n\r", ch);
	return;
    }

    if ((old_mob = get_mob_index(wnum_old.pArea, wnum_old.vnum)) == NULL)
    {
	send_to_char("That mob doesn't exist.\n\r", ch);
	return;
    }

    if (get_mob_index(wnum_new.pArea, wnum_new.vnum) != NULL)
    {
        send_to_char("That mob vnum is already taken.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
	send_to_char("You're not a builder in that area, so you can't "
                     "copy from it.\n\r", ch);
	return;
    }

    area = wnum_new.pArea;

    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_mob       = new_mob_index();
    new_mob->vnum = wnum_new.vnum;
    new_mob->area = area;

    if (wnum_new.vnum > top_vnum_mob)
        top_vnum_mob = wnum_new.vnum;

    iHash			= wnum_new.vnum % MAX_KEY_HASH;
    new_mob->next		= area->mob_index_hash[iHash];
    area->mob_index_hash[iHash]	= new_mob;
    ch->desc->pEdit		= (void *)new_mob;
    ch->desc->editor		= ED_MOBILE;

    new_mob->player_name = str_dup(old_mob->player_name);
    new_mob->short_descr = str_dup(old_mob->short_descr);
    new_mob->long_descr  = str_dup(old_mob->long_descr);
    new_mob->description = str_dup(old_mob->description);
    new_mob->comments   = str_dup(old_mob->comments);

    new_mob->act[0]          = old_mob->act[0];
    new_mob->act[1]         = old_mob->act[1];
    new_mob->affected_by[0]  = old_mob->affected_by[0];
    new_mob->affected_by[1] = old_mob->affected_by[1];

    new_mob->alignment    = old_mob->alignment;
    new_mob->level        = old_mob->level;
    new_mob->hitroll      = old_mob->hitroll;
    new_mob->hit.number       = old_mob->hit.number;
    new_mob->hit.size       = old_mob->hit.size;
    new_mob->hit.bonus       = old_mob->hit.bonus;
    new_mob->mana.number      = old_mob->mana.number;
    new_mob->mana.size      = old_mob->mana.size;
    new_mob->mana.bonus      = old_mob->mana.bonus;
    new_mob->damage.number    = old_mob->damage.number;
    new_mob->damage.size    = old_mob->damage.size;
    new_mob->damage.bonus    = old_mob->damage.bonus;
    new_mob->ac[0]        = old_mob->ac[0];
    new_mob->ac[1]        = old_mob->ac[1];
    new_mob->ac[2]        = old_mob->ac[2];
    new_mob->ac[3]        = old_mob->ac[3];
    new_mob->dam_type     = old_mob->dam_type;

    new_mob->off_flags    = old_mob->off_flags;
    new_mob->imm_flags    = old_mob->imm_flags;
    new_mob->res_flags    = old_mob->res_flags;
    new_mob->vuln_flags   = old_mob->vuln_flags;
    new_mob->start_pos    = old_mob->start_pos;
    new_mob->default_pos  = old_mob->default_pos;

    new_mob->sex	  = old_mob->sex;
    new_mob->race	  = old_mob->race;
    new_mob->wealth	  = old_mob->wealth;
    new_mob->form	  = old_mob->form;
    new_mob->parts	  = old_mob->parts;
    new_mob->size	  = old_mob->size;
    new_mob->material     = str_dup(old_mob->material);
    new_mob->move	  = old_mob->move;
    new_mob->attacks      = old_mob->attacks;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("Mobile copied.\n\r", ch);
}


/*
 * Copy an obj.
 */
void do_ocopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    OBJ_INDEX_DATA *old_obj;
    OBJ_INDEX_DATA *new_obj;
    EXTRA_DESCR_DATA *ed;
    EXTRA_DESCR_DATA *new_ed;
    AFFECT_DATA *af;
    AFFECT_DATA *new_af;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    int i;
    WNUM wnum_old, wnum_new;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: ocopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
        send_to_char("Invalid source vnum format.\n\r", ch);
	return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
        send_to_char("Invalid target vnum format.\n\r", ch);
	return;
    }

    if ((old_obj = get_obj_index(wnum_old.pArea, wnum_old.vnum)) == NULL)
    {
        send_to_char("That obj doesn't exist.\n\r", ch);
	return;
    }

    if (get_obj_index(wnum_new.pArea, wnum_new.vnum) != NULL)
    {
        send_to_char("That obj vnum is already taken.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
        send_to_char("You're not a builder in that area, so you can't "
            "copy from it.\n\r", ch);
	return;
    }

    area = wnum_new.pArea;

    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_obj		= new_obj_index();
    new_obj->vnum	= wnum_new.vnum;
    new_obj->area	= area;

    if (wnum_new.vnum > top_vnum_obj)
        top_vnum_obj = wnum_new.vnum;

    iHash			= wnum_new.vnum % MAX_KEY_HASH;
    new_obj->next		= area->obj_index_hash[iHash];
    area->obj_index_hash[iHash]	= new_obj;
    ch->desc->editor		= ED_OBJECT;
    ch->desc->pEdit		= (void *)new_obj;

    // Copy extra descs
    for (ed = old_obj->extra_descr; ed != NULL; ed = ed->next)
    {
    new_ed = new_extra_descr();
    new_ed->keyword = str_dup(ed->keyword);
    if(ed->description)
        new_ed->description = str_dup(ed->description);
    else
        new_ed->description = NULL;
    new_ed->next = new_obj->extra_descr;
    new_obj->extra_descr = new_ed;
    }

    // Copy affects
    for (af = old_obj->affected; af != NULL; af = af->next)
    {
    new_af = new_affect();
    new_af->location = af->location;
    new_af->modifier = af->modifier;
    new_af->where = af->where;
    new_af->type  = af->type;
    new_af->duration = af->duration;
    new_af->bitvector = af->bitvector;
    new_af->level	 = af->level;
    new_af->random  = af->random;
    new_af->next    = new_obj->affected;
    new_obj->affected = new_af;
    }

    new_obj->name = str_dup(old_obj->name);
    new_obj->short_descr = str_dup(old_obj->short_descr);
    new_obj->description = str_dup(old_obj->description);
    new_obj->full_description = str_dup(old_obj->full_description);
    new_obj->material = str_dup(old_obj->material);
    new_obj->item_type =  old_obj->item_type;
    new_obj->extra[0] = old_obj->extra[0];
    new_obj->extra[1] = old_obj->extra[1];
    new_obj->extra[2] = old_obj->extra[2];
    new_obj->extra[3] = old_obj->extra[3];
    new_obj->wear_flags = old_obj->wear_flags;
    new_obj->level = old_obj->level;
    new_obj->condition = old_obj->condition;
    new_obj->count = old_obj->count;
    new_obj->weight = old_obj->weight;
    new_obj->cost = old_obj->cost;
    new_obj->fragility = old_obj->fragility;
    new_obj->times_allowed_fixed = old_obj->times_allowed_fixed;
    new_obj->comments = old_obj->comments;

    for (i = 0; i <= 8; i++)
    new_obj->value[i] = old_obj->value[i];

    // Only copy impsig if imp (to block cheaters)
    if (get_staff_rank(ch) == STAFF_IMPLEMENTOR)
    new_obj->imp_sig = str_dup(old_obj->imp_sig);
    else
    new_obj->imp_sig = str_dup("none");

    new_obj->points = old_obj->points;

    SET_BIT(area->area_flags, AREA_CHANGED);

    send_to_char("Object copied.\n\r", ch);
    //oedit_show(ch, "");
}


/*
 * Copy an rprog.
 */
void do_rpcopy(CHAR_DATA *ch, char *argument)
{
    long old_v;
    long new_v;
    SCRIPT_DATA *old_rpcode;
    SCRIPT_DATA *new_rpc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    WNUM wnum_old, wnum_new;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: rpcopy <old_vnum> <new_vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
        send_to_char("Invalid source vnum format.\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
        send_to_char("Invalid target vnum format.\n\r", ch);
        return;
    }

    old_v = wnum_old.vnum;
    new_v = wnum_new.vnum;

    if ((old_rpcode = get_script_index(wnum_old.pArea, old_v, PRG_RPROG)) == NULL)
    {
        send_to_char("That ROOMprog doesn't exist.\n\r", ch);
        return;
    }

    if (get_script_index(wnum_new.pArea, new_v, PRG_RPROG) != NULL)
    {
        send_to_char("That ROOMprog vnum is already taken.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
        send_to_char("You're not a builder in the source area, so you can't "
            "copy from it.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_new.pArea))
    {
        send_to_char("You can't build in the target area.\n\r", ch);
        return;
    }

    edit_done(ch);
    ch->pcdata->immortal->last_olc_command = current_time;
    new_rpc = new_script();
    new_rpc->vnum = new_v;
    new_rpc->edit_src = str_dup(old_rpcode->src);
    new_rpc->area = wnum_new.pArea;
    compile_script(NULL,new_rpc, new_rpc->edit_src, IFC_R);
    
    new_rpc->next = wnum_new.pArea->rprog_list;
    wnum_new.pArea->rprog_list = new_rpc;

    ch->desc->pEdit             = (void *)new_rpc;
    ch->desc->editor            = ED_RPCODE;

    SET_BIT(wnum_new.pArea->area_flags, AREA_CHANGED);
    send_to_char("RoomProgram code copied.\n\r",ch);
}


/*
 * Copy an mprog.
 */
void do_mpcopy(CHAR_DATA *ch, char *argument)
{
    long old_v;
    long new_v;
    SCRIPT_DATA *old_mpcode;
    SCRIPT_DATA *new_mpc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    WNUM wnum_old, wnum_new;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: mpcopy <old_vnum> <new_vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
        send_to_char("Invalid source vnum format.\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
        send_to_char("Invalid target vnum format.\n\r", ch);
        return;
    }

    old_v = wnum_old.vnum;
    new_v = wnum_new.vnum;

    if ((old_mpcode = get_script_index(wnum_old.pArea, old_v, PRG_MPROG)) == NULL)
    {
        send_to_char("That MOBprog doesn't exist.\n\r", ch);
        return;
    }

    if (get_script_index(wnum_new.pArea, new_v, PRG_MPROG) != NULL)
    {
        send_to_char("That MOBprog vnum is already taken.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
        send_to_char("You're not a builder in the source area, so you can't "
            "copy from it.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_new.pArea))
    {
        send_to_char("You can't build in the target area.\n\r", ch);
        return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_mpc = new_script();
    new_mpc->vnum = new_v;
    new_mpc->area = wnum_new.pArea;
    new_mpc->edit_src = str_dup(old_mpcode->src);
    compile_script(NULL,new_mpc, new_mpc->edit_src, IFC_M);
    
    new_mpc->next = wnum_new.pArea->mprog_list;
    wnum_new.pArea->mprog_list = new_mpc;

    ch->desc->pEdit             = (void *)new_mpc;
    ch->desc->editor            = ED_MPCODE;

    SET_BIT(wnum_new.pArea->area_flags, AREA_CHANGED);
    send_to_char("MobProgram code copied.\n\r",ch);
}


/*
 * Copy an oprog.
 */
void do_opcopy(CHAR_DATA *ch, char *argument)
{
    long old_v;
    long new_v;
    SCRIPT_DATA *old_opcode;
    SCRIPT_DATA *new_opc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    WNUM wnum_old, wnum_new;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: opcopy <old_vnum> <new_vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg, ch->in_room->area, &wnum_old))
    {
        send_to_char("Invalid source vnum format.\n\r", ch);
        return;
    }

    if (!parse_widevnum(arg2, ch->in_room->area, &wnum_new))
    {
        send_to_char("Invalid target vnum format.\n\r", ch);
        return;
    }

    old_v = wnum_old.vnum;
    new_v = wnum_new.vnum;

    if ((old_opcode = get_script_index(wnum_old.pArea, old_v, PRG_OPROG)) == NULL)
    {
        send_to_char("That OBJprog doesn't exist.\n\r", ch);
        return;
    }

    if (get_script_index(wnum_new.pArea, new_v, PRG_OPROG) != NULL)
    {
        send_to_char("That OBJprog vnum is already taken.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_old.pArea))
    {
        send_to_char("You're not a builder in the source area, so you can't "
             "copy from it.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, wnum_new.pArea))
    {
        send_to_char("You can't build in the target area.\n\r", ch);
        return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_opc = new_script();
    new_opc->vnum = new_v;
    new_opc->area = wnum_new.pArea;
    new_opc->edit_src = str_dup(old_opcode->src);
    compile_script(NULL,new_opc, new_opc->edit_src, IFC_O);
    
    new_opc->next = wnum_new.pArea->oprog_list;
    wnum_new.pArea->oprog_list = new_opc;

    ch->desc->pEdit             = (void *)new_opc;
    ch->desc->editor            = ED_OPCODE;

    SET_BIT(wnum_new.pArea->area_flags, AREA_CHANGED);
    send_to_char("ObjProgram code copied.\n\r",ch);
}


void do_rlist(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoomIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    bool use_range = false;
    long vnum_min = 0;
    long vnum_max = 0;
    int col = 0;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    // Parse target area and vnum range
    if (arg[0] == '\0')
    {
        // No args - list all rooms in current area
        pArea = ch->in_room->area;
    }
    else if (arg2[0] == '\0')
    {
        // One arg - could be area name or error
        if ((pArea = find_area(arg)))
        {
            // Area name specified - list all rooms
        }
        else
        {
            send_to_char("Syntax: rlist\n\r"
                         "        rlist <area name>\n\r"
                         "        rlist <min vnum> <max vnum>\n\r"
                         "        rlist <area#min> <area#max>\n\r", ch);
            return;
        }
    }
    else
    {
        // Two args - vnum range (possibly with area prefix)
        WNUM wnum_min, wnum_max;
        AREA_DATA *context = ch->in_room->area;
        
        if (!parse_widevnum(arg, context, &wnum_min) || !wnum_min.pArea)
        {
            send_to_char("Invalid minimum vnum format.\n\r", ch);
            return;
        }
        
        if (!parse_widevnum(arg2, context, &wnum_max) || !wnum_max.pArea)
        {
            send_to_char("Invalid maximum vnum format.\n\r", ch);
            return;
        }
        
        if (wnum_min.pArea != wnum_max.pArea)
        {
            send_to_char("Vnum range must be within the same area.\n\r", ch);
            return;
        }
        
        pArea = wnum_min.pArea;
        vnum_min = wnum_min.vnum;
        vnum_max = wnum_max.vnum;
        
        if (vnum_min > vnum_max)
        {
            long tmp = vnum_min;
            vnum_min = vnum_max;
            vnum_max = tmp;
        }
        
        use_range = true;
    }

    buf1  = new_buf();

    // Iterate through all hash buckets in the area
    for (int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
    for (pRoomIndex = pArea->room_index_hash[iHash]; pRoomIndex != NULL; pRoomIndex = pRoomIndex->next)
    {
    if (!use_range || (pRoomIndex->vnum >= vnum_min && pRoomIndex->vnum <= vnum_max))
    {
        char *noc;
        noc = nocolour(pRoomIndex->name);
        sprintf(buf, "[%s] %-17.16s", widevnum_string_room(pRoomIndex, pArea), noc);
        free_string(noc);
        if (!add_buf(buf1, buf))
        {
        send_to_char("Can't output that much data!\n\r"
            "Use rlist <min vnum> <max vnum>\n\r", ch);
        return;
        }
        if (++col % 3 == 0)
        {
        if (!add_buf(buf1, "\n\r"))
        {
            send_to_char("Can't output that much data!\n\r"
                "Use rlist <min vnum> <max vnum>\n\r", ch);
            return;
        }
        }
    }
    }
    }

    if (col % 3 != 0)
    add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
}


void do_mlist(CHAR_DATA *ch, char *argument)
{
    MOB_INDEX_DATA *pMobIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[ MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool fAll;
    bool found;
    int col = 0;

    argument = one_argument(argument, arg);
    
    if (arg[0] == '\0')
    {
        send_to_char("Syntax:  mlist <all|name>\n\r"
                     "         mlist <area name> <all|name>\n\r", ch);
        return;
    }

    // Check if first argument is an area name
    if ((pArea = find_area(arg)) && argument[0] != '\0')
    {
        // Area specified, get the filter from next arg
        argument = one_argument(argument, arg);
    }
    else
    {
        // No area specified, use current area
        pArea = ch->in_room->area;
    }

    buf1  = new_buf();
    fAll  = !str_cmp(arg, "all");
    found = false;

    // Iterate through all hash buckets to catch widevnum entities
    for (int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
    for (pMobIndex = pArea->mob_index_hash[iHash]; pMobIndex != NULL; pMobIndex = pMobIndex->next)
    {
        if (fAll || is_name(arg, pMobIndex->player_name))
        {
        char *noc;
        found = true;
        noc = nocolour(pMobIndex->short_descr);
        sprintf(buf, "{x[%s] %-17.16s{x", widevnum_string_mobile(pMobIndex, pArea), noc);
        add_buf(buf1, buf);
        free_string(noc);
        if (++col % 3 == 0)
            add_buf(buf1, "\n\r");
        }
    }
    }

    if (!found)
    {
    send_to_char("Mobile(s) not found in this area.\n\r", ch);
    return;
    }

    if (col % 3 != 0)
    add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
    return;
}

int strlen_colours_limit( const char *str, int limit )
{
    int count;
    int i;

    if ( str == NULL )
        return 0;

    count = 0;
    for ( i = 0; count < limit && str[i] != '\0'; i++ )
    {
    if (str[i] == '{' )
    {
        i++;
        continue;
    }

    count++;
    }

    return i - count;
}

void do_olist(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObjIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool fAll, found;
    int col = 0;
    int max;

    argument = one_argument(argument, arg);
    
    if (arg[0] == '\0')
    {
        send_to_char("Syntax:  olist <all|name|item_type>\n\r"
                     "         olist <area name> <all|name|item_type>\n\r", ch);
        return;
    }

    // Check if first argument is an area name
    if ((pArea = find_area(arg)) && argument[0] != '\0')
    {
        // Area specified, get the filter from next arg
        argument = one_argument(argument, arg);
    }
    else
    {
        // No area specified, use current area
        pArea = ch->in_room->area;
    }

    buf1  = new_buf();
    fAll  = !str_cmp(arg, "all");
    found = false;

    // Iterate through all hash buckets to catch widevnum entities
    for (int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
    for (pObjIndex = pArea->obj_index_hash[iHash]; pObjIndex != NULL; pObjIndex = pObjIndex->next)
    {
        if (fAll || is_name(arg, pObjIndex->name)
        || flag_value(type_flags, arg) == pObjIndex->item_type)
        {
        found = true;
        max = strlen_colours_limit(pObjIndex->short_descr,16) + 17;
        sprintf(buf, "{x[%5ld] %-*.*s{x",
            pObjIndex->vnum, max, max - 1, pObjIndex->short_descr);
        add_buf(buf1, buf);
        if (++col % 3 == 0)
            add_buf(buf1, "\n\r");
        }
    }
    }

    if (!found)
    {
    send_to_char("Object(s) not found in this area.\n\r", ch);
    return;
    }

    if (col % 3 != 0)
    add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
    return;
}


void do_mshow(CHAR_DATA *ch, char *argument)
{
    MOB_INDEX_DATA *pMob;
    void *old_edit;
    WNUM wnum;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  mshow <vnum>\n\r", ch);
    return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum))
    {
       send_to_char("Invalid vnum format.\n\r", ch);
       return;
    }

    if (!(pMob = get_mob_index(wnum.pArea, wnum.vnum)))
    {
       send_to_char("That mobile does not exist.\n\r", ch);
       return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pMob;

    medit_show(ch, argument);
    ch->desc->pEdit = old_edit;
    return;
}


void do_oshow(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObj;
    void *old_edit;
    WNUM wnum;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  oshow <vnum>\n\r", ch);
    return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum))
    {
       send_to_char("Invalid vnum format.\n\r", ch);
       return;
    }

    if (!(pObj = get_obj_index(wnum.pArea, wnum.vnum)))
    {
    send_to_char("That object does not exist.\n\r", ch);
    return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pObj;
    oedit_show(ch, argument);
    ch->desc->pEdit = old_edit;
}


void do_rshow(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom, *oldRoom;
    void *old_edit;
    WNUM wnum;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  rshow <vnum>\n\r", ch);
    return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum))
    {
       send_to_char("Invalid vnum format.\n\r", ch);
       return;
    }

    if (!(pRoom = get_room_index(wnum.pArea, wnum.vnum)))
    {
    send_to_char("That room does not exist.\n\r", ch);
    return;
    }

    oldRoom = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, pRoom);

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pRoom;
    redit_show(ch, argument);
    ch->desc->pEdit = old_edit;

    char_from_room(ch);
    char_to_room(ch, oldRoom);
}


int calc_obj_armour(int level, int strength)
{
    switch (strength)
    {
    case OBJ_ARMOUR_LIGHT:		return level/10;
    case OBJ_ARMOUR_MEDIUM:		return level/5;
    case OBJ_ARMOUR_STRONG:		return level * 3/10;
    case OBJ_ARMOUR_HEAVY:		return level * 2/5;
    case OBJ_ARMOUR_NOSTRENGTH:
    default:			return 0;
    }
}


void set_mob_hitdice(MOB_INDEX_DATA *pMob)
{
    int hp_per_level;
    int hitBonus;
    int hitDiceType;
    int hitNumDice;

    if (pMob->level < 10)
    hp_per_level = 10;
    else
        hp_per_level = pMob->level;

    hitBonus = ((hp_per_level) * (pMob->level / 2));
    hitNumDice = pMob->level * 0.8;
    hitDiceType = hp_per_level;
    pMob->hit.number = UMAX(1,hitNumDice);
    pMob->hit.size   = UMAX(1, hitDiceType);
    pMob->hit.bonus  = UMAX(1, hitBonus);
}


void set_mob_damdice(MOB_INDEX_DATA *pMobIndex)
{
    int num;
    int type;

    num = (int) (pMobIndex->level + 8) / 10;
    type = (int) (pMobIndex->level + 8) / 4;

    num = UMAX(1, num);
    type = UMAX(8, type);

    pMobIndex->damage.number = num;
    pMobIndex->damage.size = type;
    pMobIndex->damage.bonus = pMobIndex->level;
}


void set_mob_manadice(MOB_INDEX_DATA *pMobIndex)
{
    int num;
    int type;

    num = (int) (pMobIndex->level + 20) / 20;
    type = (int) (pMobIndex->level + 20) / 8;

    num = UMAX(1, num);
    type = UMAX(8, type);

    pMobIndex->mana.number = num;
    pMobIndex->mana.size = type;
    pMobIndex->mana.bonus = pMobIndex->level/2;
}

void set_mob_movedice(MOB_INDEX_DATA *pMobIndex)
{
    pMobIndex->move = 10 + 13 * pMobIndex->level;
}



/* set some weapon dice automatically on an objIndex. */
void set_weapon_dice(OBJ_INDEX_DATA *objIndex)
{
    int num;
    int type;
    char buf[MAX_STRING_LENGTH];

    if (objIndex->item_type != ITEM_WEAPON || !IS_WEAPON(objIndex))
    {
    sprintf(buf, "set_weapon_dice: tried to set on non-weapon "
        "obj, %s, vnum %ld", objIndex->short_descr,
        objIndex->vnum);
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
    return;
    }

    // This allows certain objects to evade the auto-setting when
    // we want them tooo.
    if (objIndex->imp_sig != NULL
    &&   str_cmp(objIndex->imp_sig, "(null)")
    &&   str_cmp(objIndex->imp_sig, "none"))
    {
    sprintf(buf, "set_weapon_dice: imp sig \"%s\" found, not "
        "auto-setting dice, obj %s, vnum %ld",
        objIndex->imp_sig,
        objIndex->short_descr, objIndex->vnum);
    log_string(buf);
    return;
    }

    num = (objIndex->level + 20) / 10;
    type = (objIndex->level + 20) / 4;

    if (IS_SET(WEAPON(objIndex)->flags, WEAPON_TWO_HANDS))
    type = (type * 7)/5 - 1;

    switch(WEAPON(objIndex)->weapon_class)
    {
    case WEAPON_EXOTIC:		type += 3;	num -= 1; 	break;
    case WEAPON_SWORD:		type += 1;	num += 1; 	break;
    case WEAPON_DAGGER:		type -= 2;	num += 1; 	break;
    case WEAPON_SPEAR: 		type += 1;		  	break;
    case WEAPON_MACE:		type += 2;		  	break;
    case WEAPON_AXE:        	type += 2;		  	break;
    case WEAPON_FLAIL:		type += 2;		 	break;
    case WEAPON_WHIP:		type += 1;	num += 1; 	break;
    case WEAPON_POLEARM:		type += 2;		  	break;
    case WEAPON_STAKE:				  	  	break;
    case WEAPON_QUARTERSTAFF:	type += 1;		  	break;
    case WEAPON_ARROW: 		type = (type*7)/4;	num *= 2; 	break;
    case WEAPON_BOLT:		type *= 2;	num *= 2;	break;
    default: 						  	break;
    }

    if (IS_SET(objIndex->extra[1], ITEM_REMORT_ONLY))
    {
    type += 2;
    num += 2;
    }

    num = UMAX(1, num);
    type = UMAX(8, type);

    WEAPON(objIndex)->damage.number = num;
    WEAPON(objIndex)->damage.size = type;
}


/* set some weapon dice automatically , but for an obj. */
void set_weapon_dice_obj(OBJ_DATA *obj)
{
    int num;
    int type;
    char buf[MAX_STRING_LENGTH];

    if (obj->item_type != ITEM_WEAPON || !IS_WEAPON(obj))
    {
    sprintf(buf, "set_weapon_dice: tried to set on non-weapon "
        "obj, %s, vnum %ld", obj->short_descr,
        obj->pIndexData->vnum);
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
    return;
    }

    // This allows certain objects to evade the auto-setting when
    // we want them tooo.
    if (obj->pIndexData->imp_sig != NULL
    &&   str_cmp(obj->pIndexData->imp_sig, "(null)")
    &&   str_cmp(obj->pIndexData->imp_sig, "none"))
    {
    sprintf(buf, "set_weapon_dice: imp sig \"%s\" found, not "
        "auto-setting dice, obj %s, vnum %ld",
        obj->pIndexData->imp_sig,
        obj->short_descr, obj->pIndexData->vnum);
    log_string(buf);
    return;
    }

    num = (obj->level + 20) / 10;
    type = (obj->level + 20) / 4;

    if (IS_SET(WEAPON(obj)->flags, WEAPON_TWO_HANDS))
    type = type * 7/5 - 1;

    switch(WEAPON(obj)->weapon_class)
    {
    case WEAPON_EXOTIC:		type += 3;	num -= 1; 	break;
    case WEAPON_SWORD:		type += 1;	num += 1; 	break;
    case WEAPON_DAGGER:		type -= 2;	num += 1; 	break;
    case WEAPON_SPEAR: 		type += 1;		  	break;
    case WEAPON_MACE:		type += 2;		  	break;
    case WEAPON_AXE:        	type += 2;		  	break;
    case WEAPON_FLAIL:		type += 2;	num -= 1; 	break;
    case WEAPON_WHIP:		type += 1;	num += 1; 	break;
    case WEAPON_POLEARM:		type += 2;		  	break;
    case WEAPON_STAKE:				  	  	break;
    case WEAPON_QUARTERSTAFF:	type += 1;		  	break;
    case WEAPON_ARROW: 		type = (type*7)/4;	num *= 2; 	break;
    case WEAPON_BOLT:		type *= 2;	num *= 2;	break;
    default: 						  	break;
    }

    if (IS_SET(obj->extra[1], ITEM_REMORT_ONLY))
    {
    type += 2;
    num += 2;
    }

    num = UMAX(1, num);
    type = UMAX(8, type);

    WEAPON(obj)->damage.number = num;
    WEAPON(obj)->damage.size = type;
}


/* set AC for an obj index */
void set_armour(OBJ_INDEX_DATA *objIndex)
{
    int armour;
    int armour_exotic;

    if (!IS_ARMOR(objIndex)) return;

    armour = calc_obj_armour(objIndex->level, ARMOR(objIndex)->armor_strength);
    armour_exotic = armour * 9/10;

    ARMOR(objIndex)->protection[0] = armour;
    ARMOR(objIndex)->protection[1] = armour;
    ARMOR(objIndex)->protection[2] = armour;
    ARMOR(objIndex)->protection[3] = armour_exotic;
}


/* set AC for an obj */
void set_armour_obj(OBJ_DATA *obj)
{
    int armour;
    int armour_exotic;

    if (!IS_ARMOR(obj)) return;

    armour = calc_obj_armour(obj->level, ARMOR(obj)->armor_strength);
    armour_exotic = armour * 9/10;

    ARMOR(obj)->protection[0] = armour;
    ARMOR(obj)->protection[1] = armour;
    ARMOR(obj)->protection[2] = armour;
    ARMOR(obj)->protection[3] = armour_exotic;
}


void do_mpdelete(CHAR_DATA *ch, char *argument)
{
}


void do_opdelete(CHAR_DATA *ch, char *argument)
{
}


void do_rpdelete(CHAR_DATA *ch, char *argument)
{
}


void do_dislink(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[MSL];
    ROOM_INDEX_DATA *room;

    argument = one_argument(argument, arg);
    if (arg[0] == '\0' || !is_number(arg))
    {
    send_to_char("Syntax: dislink <room vnum> [junk]\n\r", ch);
    return;
    }

    long vnum = atol(arg);
    AREA_DATA *area = find_area_by_vnum(vnum, NULL);
    if (!area) area = get_system_area_fallback();
    if ((room = get_room_index(area, vnum)) == NULL)
    {
    send_to_char("There is no such room.\n\r", ch);
    return;
    }

    if (!has_access_area(ch, room->area))
    {
    send_to_char("Insufficient security to edit area - action logged.\n\r", ch);
    sprintf(buf, "do_dislink: %s tried to dislink %s (vnum %ld) in area %s without permissions!",
        ch->name,
        room->name,
        room->vnum,
        room->area->name);
    log_string(buf);
    return;
    }

    if (dislink_room(room))
    SET_BIT(room->area->area_flags, AREA_CHANGED);

    if (!str_cmp(argument, "junk")) {
    free_string(room->name);
    room->name = str_dup("NULL");
    }

    sprintf(buf, "Dislinked room %s (%s)\n\r", room->name, widevnum_string_room(room, NULL));
    send_to_char(buf, ch);
}


bool has_access_helpcat(CHAR_DATA *ch, HELP_CATEGORY *hcat)
{
    if (IS_NPC(ch))
    return false;

    if (ch->pcdata->security >= 9)
        return true;

    if (strstr(hcat->builders, ch->name)
    ||  strstr(hcat->builders, "All"))
    return true;

    if (hcat->security < 9 && ch->pcdata->security > hcat->security)
    return true;

    return false;
}


bool has_access_help(CHAR_DATA *ch, HELP_DATA *help)
{
    if (IS_NPC(ch))
    return false;

    if (ch->pcdata->security >= 9)
        return true;

    if (strstr(help->builders, ch->name)
    ||  strstr(help->builders, "All")
    ||  strstr(help->hCat->builders, ch->name)
    ||  strstr(help->hCat->builders, "All"))
    return true;

    if (help->security < 9 && help->hCat->security < 9
    &&  ch->pcdata->security >= help->security
    &&  ch->pcdata->security >= help->hCat->security)
    return true;

    return false;
}


void do_rjunk(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;
    bool changed = false;

    if (ch->in_room == NULL)
    return;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax: rjunk <room name>\n\r", ch);
    return;
    }

    area = ch->in_room->area;

    if (!has_access_area(ch, area))
    {
    send_to_char("Insufficient security to edit area - action logged.\n\r", ch);
    sprintf(buf, "do_dislink: %s tried to rjunk in area %s without permissions!",
        ch->name,
        area->name);
    log_string(buf);
    return;
    }

    // Iterate through all hash buckets to catch widevnum entities
    for (int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (room = area->room_index_hash[iHash]; room != NULL; room = room->next)
        {
            if (is_name(argument, room->name))
            {
                dislink_room(room);
                free_string(room->name);
                room->name = str_dup("Null");
                changed = true;
            }
        }
    }

    if (changed)
    SET_BIT(area->area_flags, AREA_CHANGED);

    send_to_char("Done.\n\r", ch);
}


// Check a obj or a mob for an imp sig.
bool has_imp_sig(MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj)
{
    if (mob == NULL && obj == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "check_imp_sig: both mob and obj were null.");
    return false;
    }

    if (mob != NULL && obj != NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "check_imp_sig: had both mob and obj.");
    return false;
    }

    if (mob != NULL)
    {
    if (mob->sig == NULL
    ||  !str_cmp(mob->sig, "(none)")
    ||  !str_cmp(mob->sig, "none")
    ||  !str_cmp(mob->sig, "(null)"))
        return false;
    }

    if (obj != NULL)
    {
    if (obj->imp_sig == NULL
    ||  !str_cmp(obj->imp_sig, "(none)")
    ||  !str_cmp(obj->imp_sig, "none")
    ||  !str_cmp(obj->imp_sig, "(null)"))
        return false;
    }

    return true;
}


void use_imp_sig(MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj)
{
    if (mob == NULL && obj == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "use_imp_sig: both mob and obj were null.");
    return;
    }

    if (mob != NULL && obj != NULL)
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "use_imp_sig: had both mob and obj.");

    if (mob != NULL)
    {
    if (mob->sig == NULL)
        return;
    else
    {
        free_string(mob->sig);
        mob->sig = str_dup("none");
    }
    }

    if (obj != NULL)
    {
    if (obj->imp_sig == NULL)
        return;
    else
    {
        free_string(obj->imp_sig);
        obj->imp_sig = str_dup("none");
    }
    }
}


void do_tshow(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index;
    void *old_edit;
    WNUM wnum;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  tshow <vnum>\n\r", ch);
    return;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum))
    {
       send_to_char("Invalid vnum format.\n\r", ch);
       return;
    }

    if (!(token_index = get_token_index(wnum.pArea, wnum.vnum)))
    {
    send_to_char("That token does not exist.\n\r", ch);
    return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) token_index;
    tedit_show(ch, argument);
    ch->desc->pEdit = old_edit;
}


void do_tlist(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool fAll, found;
    int col = 0;

    argument = one_argument(argument, arg);

    // Check if first argument is an area name
    if (arg[0] != '\0' && (pArea = find_area(arg)) && argument[0] != '\0')
    {
        // Area specified, get the filter from next arg
        argument = one_argument(argument, arg);
    }
    else if (arg[0] != '\0' && !find_area(arg))
    {
        // Arg is not an area name, use as filter with current area
        pArea = ch->in_room->area;
    }
    else if (arg[0] != '\0' && find_area(arg) && argument[0] == '\0')
    {
        // Only area name given, no filter - show all
        pArea = find_area(arg);
        arg[0] = '\0';
    }
    else
    {
        // No args at all
        pArea = ch->in_room->area;
    }

    buf1  = new_buf();
    fAll  = arg[0] == '\0';
    found = false;

    // Iterate through token_index_hash
    int iHash;
    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (token_index = pArea->token_index_hash[iHash]; token_index != NULL; token_index = token_index->next)
        {
            if (fAll || is_name(arg, token_index->name))
            {
                found = true;
                sprintf(buf, "{Y[{x%5ld{Y]{x %-17.16s{x",
                    token_index->vnum, token_index->name);
                add_buf(buf1, buf);
                if (++col % 3 == 0)
                    add_buf(buf1, "\n\r");
            }
        }
    }

    if (!found)
    {
    send_to_char("Token(s) not found in this area.\n\r", ch);
    return;
    }

    if (col % 3 != 0)
    add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
}

SHOP_STOCK_DATA *get_shop_stock_bypos(SHOP_DATA *shop, int nth)
{
    if(!shop || !shop->stock || nth < 1 ) return NULL;

    SHOP_STOCK_DATA *stock;

    for(stock = shop->stock; stock; stock = stock->next)
    {
        if(!--nth)
            return stock;
    }

    return NULL;


}