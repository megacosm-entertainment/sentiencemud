/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../../merc.h"
#include "../../interp.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../scripts.h"
#include "../../json_reserved.h"
#include "reserved.h"

/* Editor functions */
RESERVED(reserved_show);
RESERVED(reserved_add);
RESERVED(reserved_delete);
RESERVED(reserved_listvnums);
RESERVED(reserved_edit);
RESERVED(reserved_save);
RESERVED(reserved_search);
RESERVED(reserved_import);
RESERVED(reserved_export);
RESERVED(reserved_listid);

extern LLIST *reserved_vnums;
bool reserved_changed = false;

static AREA_DATA *reserved_default_area(void)
{
    AREA_DATA *area = NULL;

    if (!IS_NULLSTR(game_settings.system_area)) {
        if (is_number(game_settings.system_area))
            area = get_area_index(atol(game_settings.system_area));
        else
            area = find_area(game_settings.system_area);
    }

    if (!area)
        area = area_first;

    return area;
}

static bool reserved_parse_wnum(const char *input, WNUM *wnum)
{
    AREA_DATA *default_area = reserved_default_area();
    char temp[MSL];

    if (!input || !wnum)
        return false;

    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    if (parse_widevnum(temp, default_area, wnum))
        return true;

    if (is_number(temp) && default_area) {
        wnum->pArea = default_area;
        wnum->vnum = atol(temp);
        return (wnum->vnum > 0);
    }

    return false;
}

static bool reserved_parse_area_uid(const char *input, long *auid)
{
    AREA_DATA *area;

    if (!input || !auid)
        return false;

    if (is_number(input)) {
        *auid = atol(input);
        return (*auid > 0);
    }

    area = find_area((char *)input);
    if (area) {
        *auid = area->uid;
        return true;
    }

    return false;
}

static const char *reserved_wnum_string(const RESERVED_DATA *reserved)
{
    static char out[32];
    AREA_DATA *area;

    if (!reserved)
        return "0#0";

    if (reserved->type == RESERVED_AREA) {
        long auid = reserved->wnum.auid;
        if (auid <= 0) {
            AREA_DATA *fallback = reserved_default_area();
            auid = fallback ? fallback->uid : 0;
        }
        snprintf(out, sizeof(out), "%ld", auid);
        return out;
    }

    area = get_area_index(reserved->wnum.auid);
    if (!area) {
        /* If area_uid is 0 or invalid, search all areas for this vnum */
        ROOM_INDEX_DATA *room;
        for (area = area_first; area; area = area->next) {
            room = get_room_index(area, reserved->wnum.vnum);
            if (room)
                break;
        }
        /* Fallback if not found */
        if (!area)
            area = reserved_default_area();
    }
    return widevnum_string(area, reserved->wnum.vnum, NULL);
}

/* Reserved types and their text names */
const struct reserved_type_name {
    const char *name;
    int type;
} reserved_types[] = {
    { "mob",    RESERVED_MOB    },
    { "obj",    RESERVED_OBJ    },
    { "room",   RESERVED_ROOM   },
    { "area",   RESERVED_AREA   },
    { "token", RESERVED_TOKEN },
    { "mprog", RESERVED_MPROG },
    { "oprog", RESERVED_OPROG },
    { "rprog", RESERVED_RPROG },
    { "tprog", RESERVED_TPROG },
    { "aprog", RESERVED_APROG },
    { NULL,     -1              }
};

/*
 * Load reserved items from file
 * Tries JSON first, falls back to .dat format with auto-migration
 */
void load_reserved(void)
{
    FILE *fp;
    char *word;
    bool in_block = false;
    RESERVED_DATA *reserved = NULL;
    char vnum_str[MAX_INPUT_LENGTH];
    WNUM wnum;
    
    /* Try loading JSON first */
    if (load_reserved_json()) {
        reserved_changed = false;
        return;
    }
    
    /* Fall back to old .dat format */
    if ((fp = fopen(RESERVED_FILE, "r")) == NULL) {
        pwarnf(LOG_INIT, "No reserved items file found. Creating new file at save.");
        return;
    }

    plogf(LOG_INIT, "Loading reserved items from legacy .dat format...");

    /* Clear existing items first */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *old;
        
        iterator_start(&it, reserved_vnums);
        while ((old = (RESERVED_DATA *)iterator_nextdata(&it))) {
            free_string(old->name);
            free_string(old->description);
            free_mem(old, sizeof(RESERVED_DATA));
        }
        iterator_stop(&it);
        list_clear(reserved_vnums);
    }
    
    for (;;) {
        word = feof(fp) ? "#END" : fread_word(fp);
        
        if (word[0] == '#') {
            if (!str_cmp(word, "#END")) {
                break;
            } else if (!str_cmp(word, "#RESERVED")) {
                in_block = true;
                reserved = alloc_mem(sizeof(RESERVED_DATA));
                reserved->name = NULL;
                reserved->description = NULL;
                reserved->type = -1;
                reserved->removable = false;
                reserved->wnum.auid = 0;
                reserved->wnum.vnum = 0;
            } else if (!str_cmp(word, "#-RESERVED")) {
                if (in_block && reserved) {
                    /* If only vnum was provided, use parse_widevnum to find the area */
                    if (reserved->wnum.vnum > 0 && reserved->wnum.auid == 0) {
                        sprintf(vnum_str, "%ld", reserved->wnum.vnum);
                        if (parse_widevnum(vnum_str, NULL, &wnum)) {
                            reserved->wnum.auid = wnum.pArea ? wnum.pArea->uid : 0;
                            plogf(LOG_INFO, "Reserved '%s': Found vnum %ld in area %ld", 
                                  reserved->name, reserved->wnum.vnum, reserved->wnum.auid);
                        } else {
                            plogf(LOG_WARN, "Reserved '%s': Vnum %ld not found in any area", 
                                  reserved->name, reserved->wnum.vnum);
                        }
                    }
                    
                    /* Add to the list if it's valid */
                    if (reserved->name && reserved->name[0] && reserved->type != -1) {
                        list_appendlink(reserved_vnums, reserved);
                    } else {
                        /* Free invalid entry */
                        if (reserved->name)
                            free_string(reserved->name);
                        if (reserved->description)
                            free_string(reserved->description);
                        free_mem(reserved, sizeof(RESERVED_DATA));
                    }
                    
                    in_block = false;
                    reserved = NULL;
                }
            }
        } else if (in_block && reserved) {
            if (!str_cmp(word, "Name")) {
                reserved->name = fread_string(fp);
            } else if (!str_cmp(word, "Type")) {
                word = fread_word(fp);
                for (int i = 0; reserved_types[i].name; i++) {
                    if (!str_cmp(word, reserved_types[i].name)) {
                        reserved->type = reserved_types[i].type;
                        break;
                    }
                }
            } else if (!str_cmp(word, "Removable")) {
                reserved->removable = fread_number(fp) != 0;
            } else if (!str_cmp(word, "ID")) {
                reserved->wnum.auid = 0;
                reserved->wnum.vnum = fread_number(fp);
            } else if (!str_cmp(word, "AreaUID") || !str_cmp(word, "AUID")) {
                reserved->wnum.auid = fread_number(fp);
            } else if (!str_cmp(word, "Vnum") || !str_cmp(word, "VNUM")) {
                reserved->wnum.vnum = fread_number(fp);
            } else if (!str_cmp(word, "WNUM") || !str_cmp(word, "Wnum")) {
                char *wnum_str = fread_string(fp);

                if (reserved_parse_wnum(wnum_str, &wnum)) {
                    reserved->wnum.auid = wnum.pArea ? wnum.pArea->uid : 0;
                    reserved->wnum.vnum = wnum.vnum;
                }
                free_string(wnum_str);
            } else if (!str_cmp(word, "Description")) {
                reserved->description = fread_string(fp);
            } else {
                pwarnf(LOG_INIT, "Unknown field '%s'", word);
                fread_to_eol(fp);
            }
        } else {
            pwarnf(LOG_INIT, "Unexpected data outside block: %s", word);
            fread_to_eol(fp);
        }
    }
    
    fclose(fp);
    
    plogf(LOG_INIT, "%d reserved items loaded from .dat format.", list_size(reserved_vnums));
    
    /* Migrate to JSON format */
    plogf(LOG_INIT, "Migrating reserved items to JSON format...");
    if (save_reserved_json()) {
        char old_file[MAX_INPUT_LENGTH];
        sprintf(old_file, "%s.old", RESERVED_FILE);
        rename(RESERVED_FILE, old_file);
        plogf(LOG_INFO, "Reserved items migrated to JSON, old file saved as %s", old_file);
    }
    
    reserved_changed = false;
}

/*
 * Save all reserved items to file
 */
void save_reserved(void)
{
    if (!reserved_changed) {
        plogf(LOG_INIT, "Reserved items unchanged, not saving.");
        return;
    }
    
    /* Save to JSON format */
    if (save_reserved_json()) {
        plogf(LOG_INFO, "Saved %d reserved items to JSON", 
              list_size(reserved_vnums));
        reserved_changed = false;
    } else {
        pbugf(LOG_ERROR, "Failed to save reserved items to JSON");
    }
}

/*
 * Find a reserved item by name
 */
RESERVED_DATA *find_reserved(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    
    if (!name || !name[0] || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (!str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            return reserved;
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Find a reserved item by ID and type
 */
RESERVED_DATA *find_reserved_by_id(int id, int type)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    
    if (!reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type != type)
            continue;

        if (type == RESERVED_AREA) {
            if (reserved->wnum.auid == id) {
                iterator_stop(&it);
                return reserved;
            }
        } else if (reserved->wnum.vnum == id) {
            iterator_stop(&it);
            return reserved;
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Get the value of a reserved item (used in code to replace #defines)
 */
int get_reserved_vnum(const char *name)
{
    RESERVED_DATA *reserved = find_reserved(name);
    
    if (reserved)
        return reserved->wnum.vnum;
    
    perrf(LOG_ERROR, "Reserved item '%s' not found", name);
    return -1;
}

/*
 * Main editor command function
 */
void do_reserved(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    
    if (IS_NPC(ch)) {
        send_to_char("NPCs cannot use the reserved editor.\n\r", ch);
        return;
    }
    
    if (ch->pcdata->security < MIN_SECURITY_RESERVED) {
        send_to_char("You do not have enough security to edit reserved items.\n\r", ch);
        return;
    }
    
    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (command[0] == '\0') {
        reserved_listvnums(ch, argument);
        return;
    }
    
    /* Command dispatch */
    if (!str_cmp(command, "show"))
        reserved_show(ch, argument);
    else if (!str_cmp(command, "add"))
        reserved_add(ch, argument);
    else if (!str_cmp(command, "delete"))
        reserved_delete(ch, argument);
    else if (!str_cmp(command, "list"))
        reserved_listvnums(ch, argument);
    else if (!str_cmp(command, "listid") || !str_cmp(command, "id"))
        reserved_listid(ch, argument);
    else if (!str_cmp(command, "edit"))
        reserved_edit(ch, argument);
    else if (!str_cmp(command, "save"))
        reserved_save(ch, argument);
    else if (!str_cmp(command, "search"))
        reserved_search(ch, argument);
    else if (!str_cmp(command, "import"))
        reserved_import(ch, argument);
    else if (!str_cmp(command, "export"))
        reserved_export(ch, argument);
    else {
        /* Show help */
        send_to_char("Reserved Item Editor Commands:\n\r", ch);
        send_to_char("  reserved list [type]     - List all reserved items (or by type)\n\r", ch);
        send_to_char("  reserved listid <wnum> [type] - List items with a specific WNUM\n\r", ch);
        send_to_char("  reserved show <name>     - Show details of a reserved item\n\r", ch);
        send_to_char("  reserved add <name> <type> <wnum> [removable] - Add a new reserved item\n\r", ch);
        send_to_char("  reserved edit <name> <field> <value>         - Edit a reserved item\n\r", ch);
        send_to_char("  reserved delete <name>   - Delete a reserved item\n\r", ch);
        send_to_char("  reserved search <string> - Search for reserved items\n\r", ch);
        send_to_char("  reserved save            - Force save reserved items\n\r", ch);
        send_to_char("  reserved import          - Import defines from code\n\r", ch);
        send_to_char("  reserved export          - Export as define statements\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("Reserved Types: mob, obj, room, area, skill, flag, command\n\r", ch);
    }
    
    return;
}
/*
 * List all reserved items or those of a specific type
 */
RESERVED(reserved_listvnums)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int type = -1;
    int count = 0;
    int name_width = 23; // Default width
    int max_name_len = 0;
    int table_width = 80;
    int wnum_width = 18;
    int desc_width = 26; // Default width
    
    /* Check for type filter */
    if (argument[0]) {
        for (int i = 0; reserved_types[i].name; i++) {
            if (!str_prefix(argument, reserved_types[i].name)) {
                type = reserved_types[i].type;
                break;
            }
        }
        
        if (type == -1) {
            send_to_char("Unknown reserved type. Valid types are: mob, obj, room, area, skill, flag, command\n\r", ch);
            return false;
        }
    }
    
    /* First pass: determine longest name for dynamic sizing */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            /* Skip if not matching type filter */
            if (type != -1 && reserved->type != type)
                continue;
            
            int name_len = strlen(reserved->name);
            if (name_len > max_name_len) 
                max_name_len = name_len;
        }
        iterator_stop(&it);
        
        /* Calculate optimal column width (clamped between 15 and 40) */
        name_width = UMAX(15, UMIN(40, max_name_len + 2));
        
        /* Adjust description width to keep table width at 80 chars */
        /* Fixed columns: | (1) name (name_width) | (1) type (10) | (1) ID (6) | (1) remove (7) | (1) desc (desc_width) | (1) */
        desc_width = table_width - (name_width + 10 + wnum_width + 7 + 6);
    }
    
    buffer = new_buf();
    
    /* Generate divider line based on column widths */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    /* Generate header line */
        sprintf(buf, "{Y| {W%-*s{x | {W%-10s{x | {W%-*s{x | {W%-7s{x | {W%-*s{x |{x\n\r",
            name_width, "Name", "Type", wnum_width, "WNUM", "Remove", desc_width, "Description");
    add_buf(buffer, buf);
    
    /* Repeat divider line */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            const char *type_name = "unknown";
            
            /* Skip if not matching type filter */
            if (type != -1 && reserved->type != type)
                continue;
                
            count++;
            
            /* Get type name for display */
            for (int i = 0; reserved_types[i].name; i++) {
                if (reserved_types[i].type == reserved->type) {
                    type_name = reserved_types[i].name;
                    break;
                }
            }
            
            /* Format name and truncate if needed */
            char name_buf[50];
            if (strlen(reserved->name) > name_width) {
                strncpy(name_buf, reserved->name, name_width - 3);
                name_buf[name_width - 3] = '\0';
                strcat(name_buf, "...");
            } else {
                strcpy(name_buf, reserved->name);
            }
            
            /* Format description for display (truncate if needed) */
            char desc_buf[50];
            if (reserved->description && reserved->description[0]) {
                strncpy(desc_buf, reserved->description, desc_width - 3);
                desc_buf[desc_width - 3] = '\0';
                if (strlen(reserved->description) > desc_width - 3)
                    strcat(desc_buf, "...");
            } else {
                strcpy(desc_buf, "(none)");
            }
            
            sprintf(buf, "{Y| {C%-*s{x | {B%-10s{x | {Y%-*s{x | {%c%-7s{x | %-*s |{x\n\r",
                name_width, name_buf,
                type_name,
                wnum_width, reserved_wnum_string(reserved),
                reserved->removable ? 'G' : 'R',
                reserved->removable ? "Yes" : "No",
                desc_width, desc_buf);
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    }
    
    if (count == 0) {
        sprintf(buf, "{Y| %-*s |{x\n\r", table_width - 4, "No reserved items found.");
        add_buf(buffer, buf);
    }
    
    /* Table footer */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    sprintf(buf, "\n\r%d reserved items listed.\n\r", count);
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return true;
}

/*
 * Show details of a specific reserved item
 */
RESERVED(reserved_show)
{
    char arg[MAX_INPUT_LENGTH];
    RESERVED_DATA *reserved;
    char buf[MAX_STRING_LENGTH];
    const char *type_name = "unknown";
    
    argument = one_argument(argument, arg);
    
    if (arg[0] == '\0') {
        send_to_char("Syntax: reserved show <name>\n\r", ch);
        return false;
    }
    
    reserved = find_reserved(arg);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return false;
    }
    
    /* Get type name */
    for (int i = 0; reserved_types[i].name; i++) {
        if (reserved_types[i].type == reserved->type) {
            type_name = reserved_types[i].name;
            break;
        }
    }
    
    /* Display detailed information */
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {WReserved Item: %-60s {Y|{x\n\r", reserved->name);
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {WType:{x %-72s {Y|{x\n\r", type_name);
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {WWNUM:{x %-72s {Y|{x\n\r", reserved_wnum_string(reserved));
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {WRemovable:{x %-68s {Y|{x\n\r", reserved->removable ? "Yes" : "No");
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {WDescription:{x %-65s {Y|{x\n\r", " ");
    send_to_char(buf, ch);
    
    if (reserved->description && reserved->description[0]) {
        /* Break description into lines if needed */
        char desc_copy[MAX_STRING_LENGTH];
        char *p, *q;
        
        strcpy(desc_copy, reserved->description);
        p = desc_copy;
        
        while (*p) {
            q = p;
            while (*q && *q != '\n' && *q != '\r' && (q - p) < 76)
                q++;
                
            *q = '\0';
            
            sprintf(buf, "{Y| {x%-76.76s {Y|{x\n\r", p);
            send_to_char(buf, ch);
            
            p = q + 1;
            if (*p == '\r')
                p++;
            if (*p == '\n')
                p++;
        }
    } else {
        sprintf(buf, "{Y| {x%-76s {Y|{x\n\r", "(No description)");
        send_to_char(buf, ch);
    }
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    send_to_char(buf, ch);
    
    /* Show usage examples */
    sprintf(buf, "{Y| {WUsage Examples:{x %-64s {Y|{x\n\r", " ");
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y| {xIn code: {W%d = get_reserved_vnum(\"%s\"){x %-32s {Y|{x\n\r", reserved->wnum.vnum, reserved->name, "");
    send_to_char(buf, ch);
    
    switch (reserved->type) {
        case RESERVED_MOB:
            sprintf(buf, "{Y| {xIn scripts: {Wload mob $%s{x %-54s {Y|{x\n\r", reserved->name, "");
            break;
        case RESERVED_OBJ:
            sprintf(buf, "{Y| {xIn scripts: {Wload obj $%s{x %-54s {Y|{x\n\r", reserved->name, "");
            break;
        case RESERVED_ROOM:
            sprintf(buf, "{Y| {xIn scripts: {Wtransfer $n $%s{x %-52s {Y|{x\n\r", reserved->name, "");
            break;
        default:
            sprintf(buf, "{Y| {x%-76.76s {Y|{x\n\r", "");
            break;
    }
    send_to_char(buf, ch);
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    send_to_char(buf, ch);
    
    return true;
}

/*
 * Add a new reserved item
 */
RESERVED(reserved_add)
{
    char name[MAX_INPUT_LENGTH];
    char type_str[MAX_INPUT_LENGTH];
    char id_str[MAX_INPUT_LENGTH];
    char removable_str[MAX_INPUT_LENGTH];
    int type = -1;
    WNUM wnum;
    long auid = 0;
    bool removable = true;
    RESERVED_DATA *reserved;
    
    argument = one_argument(argument, name);
    argument = one_argument(argument, type_str);
    argument = one_argument(argument, id_str);
    argument = one_argument(argument, removable_str);
    
    if (name[0] == '\0' || type_str[0] == '\0' || id_str[0] == '\0') {
        send_to_char("Syntax: reserved add <name> <type> <wnum> [removable]\n\r", ch);
        send_to_char("Types: mob, obj, room, area, skill, flag, command\n\r", ch);
        return false;
    }
    
    /* Check for existing item with same name */
    if (find_reserved(name)) {
        send_to_char("A reserved item with that name already exists.\n\r", ch);
        return false;
    }
    
    /* Find the type */
    for (int i = 0; reserved_types[i].name; i++) {
        if (!str_prefix(type_str, reserved_types[i].name)) {
            type = reserved_types[i].type;
            break;
        }
    }
    
    if (type == -1) {
        send_to_char("Invalid type. Valid types are: mob, obj, room, area, skill, flag, command\n\r", ch);
        return false;
    }
    
    if (type == RESERVED_AREA) {
        if (!reserved_parse_area_uid(id_str, &auid)) {
            send_to_char("Area must be a valid UID or area name.\n\r", ch);
            return false;
        }
    } else {
        if (!reserved_parse_wnum(id_str, &wnum)) {
            send_to_char("WNUM must be a valid widevnum (e.g. 5#1234 or #1234).\n\r", ch);
            return false;
        }
    }
    
    /* Check for removable flag */
    if (removable_str[0] != '\0') {
        if (!str_prefix(removable_str, "permanent") || !str_prefix(removable_str, "no") || 
            !str_prefix(removable_str, "false") || !str_cmp(removable_str, "0")) {
            removable = false;
        }
    }
    
    /* Check for duplicate ID of same type */
    reserved = find_reserved_by_id(type == RESERVED_AREA ? (int)auid : (int)wnum.vnum, type);
    if (reserved) {
        send_to_char(formatf("Warning: An item of the same type with ID %ld already exists: %s\n\r",
            type == RESERVED_AREA ? auid : wnum.vnum, reserved->name), ch);
        send_to_char("Adding anyway...\n\r", ch);
    }
    
    /* Create the new reserved item */
    reserved = alloc_mem(sizeof(RESERVED_DATA));
    reserved->name = str_dup(name);
    reserved->description = str_dup(argument);
    reserved->type = type;
    reserved->wnum.auid = (type == RESERVED_AREA) ? auid : (wnum.pArea ? wnum.pArea->uid : 0);
    reserved->wnum.vnum = (type == RESERVED_AREA) ? 0 : wnum.vnum;
    reserved->removable = removable;
    
    /* Add to list */
    if (!reserved_vnums)
        reserved_vnums = list_create(false);
    list_appendlink(reserved_vnums, reserved);
    
    reserved_changed = true;
    
    send_to_char(formatf("Added reserved item: %s (type: %s, WNUM: %s)\n\r", 
        name, reserved_types_get_name(type), reserved_wnum_string(reserved)), ch);
    
    return true;
}

/*
 * Delete a reserved item
 */
RESERVED(reserved_delete)
{
    char arg[MAX_INPUT_LENGTH];
    char confirm[MAX_INPUT_LENGTH];
    RESERVED_DATA *reserved;
    
    argument = one_argument(argument, arg);
    one_argument(argument, confirm);
    
    if (arg[0] == '\0') {
        send_to_char("Syntax: reserved delete <name> [confirm]\n\r", ch);
        return false;
    }
    
    reserved = find_reserved(arg);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return false;
    }
    
    if (!reserved->removable) {
        send_to_char("This reserved item cannot be removed. It is marked as permanent.\n\r", ch);
        return false;
    }
    
    /* Require confirmation */
    if (str_cmp(confirm, "confirm")) {
        send_to_char(formatf("Are you sure you want to delete reserved item '%s'?\n\r", reserved->name), ch);
        send_to_char("Type 'reserved delete <name> confirm' to confirm.\n\r", ch);
        return false;
    }
    
    /* Remove from list */

    if (list_haslink(reserved_vnums, reserved)) {
        /* Remove from the list */
        list_remlink(reserved_vnums, reserved, true);
        free_string(reserved->name);
        free_string(reserved->description);
        free_mem(reserved, sizeof(RESERVED_DATA));
        reserved_changed = true;
    } else {
        send_to_char("Reserved item not found in the list.\n\r", ch);
        return false;
    }

        
        send_to_char("Reserved item deleted.\n\r", ch);
        
        /* Auto-save after deletion */
        save_reserved();
        
        return true;
    
    
    send_to_char("Error removing reserved item from list.\n\r", ch);
    return false;
}

/*
 * Edit a reserved item
 */
RESERVED(reserved_edit)
{
    char name[MAX_INPUT_LENGTH];
    char field[MAX_INPUT_LENGTH];
    RESERVED_DATA *reserved;
    
    argument = one_argument(argument, name);
    argument = one_argument(argument, field);
    
    if (name[0] == '\0' || field[0] == '\0') {
        send_to_char("Syntax: reserved edit <name> <field> <value>\n\r", ch);
        send_to_char("Fields: name, type, wnum, removable, description\n\r", ch);
        return false;
    }
    
    reserved = find_reserved(name);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return false;
    }
    
    if (!str_cmp(field, "name")) {
        if (argument[0] == '\0') {
            send_to_char("You must specify a new name.\n\r", ch);
            return false;
        }
        
        /* Check if new name already exists */
        if (find_reserved(argument)) {
            send_to_char("A reserved item with that name already exists.\n\r", ch);
            return false;
        }
        
        free_string(reserved->name);
        reserved->name = str_dup(argument);
        send_to_char("Reserved item name changed.\n\r", ch);
        reserved_changed = true;
    }
    else if (!str_cmp(field, "type")) {
        int type = -1;
        
        if (argument[0] == '\0') {
            send_to_char("You must specify a type: mob, obj, room, area, skill, flag, command\n\r", ch);
            return false;
        }
        
        /* Find the type */
        for (int i = 0; reserved_types[i].name; i++) {
            if (!str_prefix(argument, reserved_types[i].name)) {
                type = reserved_types[i].type;
                break;
            }
        }
        
        if (type == -1) {
            send_to_char("Invalid type. Valid types are: mob, obj, room, area, skill, flag, command\n\r", ch);
            return false;
        }
        
        reserved->type = type;
        send_to_char("Reserved item type changed.\n\r", ch);
        reserved_changed = true;
    }
    else if (!str_cmp(field, "id") || !str_cmp(field, "wnum")) {
        WNUM wnum;
        long auid = 0;

        if (argument[0] == '\0') {
            send_to_char("You must specify a WNUM or area UID.\n\r", ch);
            return false;
        }

        if (reserved->type == RESERVED_AREA) {
            if (!reserved_parse_area_uid(argument, &auid)) {
                send_to_char("Area must be a valid UID or area name.\n\r", ch);
                return false;
            }
        } else {
            if (!reserved_parse_wnum(argument, &wnum)) {
                send_to_char("WNUM must be a valid widevnum (e.g. 5#1234 or #1234).\n\r", ch);
                return false;
            }
        }

        /* Check for duplicate ID of same type */
        RESERVED_DATA *existing = find_reserved_by_id(reserved->type == RESERVED_AREA ? (int)auid : (int)wnum.vnum, reserved->type);
        if (existing && existing != reserved) {
            send_to_char(formatf("Warning: An item of the same type with ID %ld already exists: %s\n\r",
                reserved->type == RESERVED_AREA ? auid : wnum.vnum, existing->name), ch);
            send_to_char("Changing anyway...\n\r", ch);
        }

        reserved->wnum.auid = (reserved->type == RESERVED_AREA) ? auid : (wnum.pArea ? wnum.pArea->uid : 0);
        reserved->wnum.vnum = (reserved->type == RESERVED_AREA) ? 0 : wnum.vnum;
        send_to_char("Reserved item WNUM changed.\n\r", ch);
        reserved_changed = true;
    }
    else if (!str_cmp(field, "removable")) {
        if (!reserved->removable && get_staff_rank(ch) < STAFF_IMPLEMENTOR) {
            send_to_char("Only implementors can change permanent reserved items to removable.\n\r", ch);
            return false;
        }
        
        if (argument[0] == '\0') {
            reserved->removable = !reserved->removable;
        } else {
            if (!str_prefix(argument, "yes") || !str_prefix(argument, "true") ||
                !str_prefix(argument, "on") || !str_cmp(argument, "1")) {
                reserved->removable = true;
            } else {
                reserved->removable = false;
            }
        }
        
        send_to_char(formatf("Reserved item is now %s.\n\r", reserved->removable ? "removable" : "permanent"), ch);
        reserved_changed = true;
    }
    else if (!str_cmp(field, "description")) {
        if (argument[0] == '\0') {
            /* Start string editor */
            string_append(ch, &reserved->description);
            reserved_changed = true;
            return true;
        } else {
            free_string(reserved->description);
            reserved->description = str_dup(argument);
            send_to_char("Reserved item description changed.\n\r", ch);
            reserved_changed = true;
        }
    }
    else {
        send_to_char("Invalid field. Fields are: name, type, id, removable, description\n\r", ch);
        return false;
    }
    
    return true;
}

/*
 * Save reserved items
 */
RESERVED(reserved_save)
{
    save_reserved();
    send_to_char("Reserved items saved.\n\r", ch);
    return true;
}

/*
 * Search for reserved items
 */
RESERVED(reserved_search)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int count = 0;
    char search_str[MAX_INPUT_LENGTH];
    int name_width = 23; // Default width
    int max_name_len = 0;
    int table_width = 80;
    int wnum_width = 18;
    int desc_width = 26; // Default width
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: reserved search <string>\n\r", ch);
        return false;
    }
    
    /* Convert search string to lowercase for case-insensitive matching */
    strcpy(search_str, argument);
    for (char *p = search_str; *p; p++) {
        *p = LOWER(*p);
    }
    
    /* First pass: determine longest name for dynamic sizing */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        char name_lowercase[MAX_STRING_LENGTH];
        char desc_lowercase[MAX_STRING_LENGTH];
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            /* Convert item data to lowercase for comparison */
            strcpy(name_lowercase, reserved->name ? reserved->name : "");
            for (char *p = name_lowercase; *p; p++) {
                *p = LOWER(*p);
            }
            
            strcpy(desc_lowercase, reserved->description ? reserved->description : "");
            for (char *p = desc_lowercase; *p; p++) {
                *p = LOWER(*p);
            }
            
            /* Check if name or description contains the search string (case-insensitive) */
            if ((!strstr(name_lowercase, search_str)) && 
                (!strstr(desc_lowercase, search_str))) {
                continue;
            }
            
            int name_len = strlen(reserved->name);
            if (name_len > max_name_len) 
                max_name_len = name_len;
        }
        iterator_stop(&it);
        
        /* Calculate optimal column width (clamped between 15 and 40) */
        name_width = UMAX(15, UMIN(40, max_name_len + 2));
        
        /* Adjust description width to keep table width at 80 chars */
        desc_width = table_width - (name_width + 10 + wnum_width + 7 + 6);
    }
    
    buffer = new_buf();
    
    /* Generate divider line based on column widths */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    /* Generate header line */
        sprintf(buf, "{Y| {W%-*s{x | {W%-10s{x | {W%-*s{x | {W%-7s{x | {W%-*s{x |{x\n\r",
            name_width, "Name", "Type", wnum_width, "WNUM", "Remove", desc_width, "Description");
    add_buf(buffer, buf);
    
    /* Repeat divider line */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            const char *type_name = "unknown";
            char name_lowercase[MAX_STRING_LENGTH];
            char desc_lowercase[MAX_STRING_LENGTH];
            
            /* Convert item data to lowercase for comparison */
            strcpy(name_lowercase, reserved->name ? reserved->name : "");
            for (char *p = name_lowercase; *p; p++) {
                *p = LOWER(*p);
            }
            
            strcpy(desc_lowercase, reserved->description ? reserved->description : "");
            for (char *p = desc_lowercase; *p; p++) {
                *p = LOWER(*p);
            }
            
            /* Check if name or description contains the search string (case-insensitive) */
            if ((!strstr(name_lowercase, search_str)) && 
                (!strstr(desc_lowercase, search_str))) {
                continue;
            }
            
            count++;
            
            /* Get type name for display */
            for (int i = 0; reserved_types[i].name; i++) {
                if (reserved_types[i].type == reserved->type) {
                    type_name = reserved_types[i].name;
                    break;
                }
            }
            
            /* Format name and truncate if needed */
            char name_buf[50];
            if (strlen(reserved->name) > name_width) {
                strncpy(name_buf, reserved->name, name_width - 3);
                name_buf[name_width - 3] = '\0';
                strcat(name_buf, "...");
            } else {
                strcpy(name_buf, reserved->name);
            }
            
            /* Format description for display (truncate if needed) */
            char desc_buf[50];
            if (reserved->description && reserved->description[0]) {
                strncpy(desc_buf, reserved->description, desc_width - 3);
                desc_buf[desc_width - 3] = '\0';
                if (strlen(reserved->description) > desc_width - 3)
                    strcat(desc_buf, "...");
            } else {
                strcpy(desc_buf, "(none)");
            }
            
            sprintf(buf, "{Y| {C%-*s{x | {B%-10s{x | {Y%-18s{x | {%c%-7s{x | %-*s |{x\n\r",
                name_width, name_buf,
                type_name,
                reserved_wnum_string(reserved),
                reserved->removable ? 'G' : 'R',
                reserved->removable ? "Yes" : "No",
                desc_width, desc_buf);
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    }
    
    if (count == 0) {
        sprintf(buf, "{Y| %-*s |{x\n\r", table_width - 4, "No matching reserved items found.");
        add_buf(buffer, buf);
    }
    
    /* Table footer */
    sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            6, "--------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    sprintf(buf, "\n\r%d reserved items matched your search.\n\r", count);
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return true;
}

/*
 * Import defines from header files
 */
RESERVED(reserved_import)
{
    send_to_char("This functionality is not yet implemented.\n\r", ch);
    send_to_char("To import defines, you would need to specify the file to parse.\n\r", ch);
    return false;
}

/*
 * Export as define statements
 */
RESERVED(reserved_export)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    
    buffer = new_buf();
    
    add_buf(buffer, "/* Generated Reserved Items as Defines */\n\n");
    
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        int last_type = -1;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            /* Add section comment if type changes */
            if (reserved->type != last_type) {
                const char *type_name = "Unknown";
                
                for (int i = 0; reserved_types[i].name; i++) {
                    if (reserved_types[i].type == reserved->type) {
                        type_name = reserved_types[i].name;
                        break;
                    }
                }
                
                sprintf(buf, "\n/* %s reserved virtual numbers */\n", 
                    type_name ? type_name : "Unknown");
                add_buf(buffer, buf);
                
                last_type = reserved->type;
            }
            
            /* Add the define statement */
            if (reserved->type == RESERVED_AREA)
                sprintf(buf, "#define %-30s %ld", reserved->name, reserved->wnum.auid);
            else
                sprintf(buf, "#define %-30s %ld", reserved->name, reserved->wnum.vnum);
            
            /* Add description as comment if present */
            if (reserved->description && reserved->description[0]) {
                char desc_copy[MAX_STRING_LENGTH];
                char *p;
                
                /* Strip newlines from description */
                strcpy(desc_copy, reserved->description);
                for (p = desc_copy; *p; p++) {
                    if (*p == '\n' || *p == '\r')
                        *p = ' ';
                }
                
                sprintf(buf + strlen(buf), " /* %-76.76s */", desc_copy);
            }

            if (reserved->type != RESERVED_AREA) {
                sprintf(buf + strlen(buf), " /* %s */", reserved_wnum_string(reserved));
            }
            
            strcat(buf, "\n");
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    } else {
        add_buf(buffer, "/* No reserved items found */\n");
    }
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return true;
}

/*
 * Get the name of a reserved type from its value
 */
const char *reserved_types_get_name(int type)
{
    for (int i = 0; reserved_types[i].name; i++) {
        if (reserved_types[i].type == type) {
            return reserved_types[i].name;
        }
    }
    return "unknown";
}

/*
 * Initialize reserved defaults
 */
void init_reserved_defaults(void)
{
    struct {
        const char *name;
        int type;
        int id;
        bool removable;
        const char *description;
    } defaults[] = {
        /* Define your defaults here */
        { "MOB_VNUM_DEATH", RESERVED_MOB, 6502, false, "Death mob used for death handling" },
        { "OBJ_VNUM_SILVER_ONE", RESERVED_OBJ, 1, false, "One silver coin" },
        { "ROOM_VNUM_LIMBO", RESERVED_ROOM, 2, false, "Limbo room for storage" },
        /* Add more defaults here */
        { NULL, 0, 0, false, NULL }
    };
    
    if (!reserved_vnums)
        reserved_vnums = list_create(false);
    
    for (int i = 0; defaults[i].name; i++) {
        RESERVED_DATA *reserved = alloc_mem(sizeof(RESERVED_DATA));
        reserved->name = str_dup(defaults[i].name);
        reserved->type = defaults[i].type;
        reserved->wnum.auid = 0;
        reserved->wnum.vnum = defaults[i].id;
        reserved->removable = defaults[i].removable;
        reserved->description = str_dup(defaults[i].description);
        
        list_appendlink(reserved_vnums, reserved);
        reserved_changed = true;
    }

    plogf(LOG_INIT, "Default reserved items loaded.");
}


/*
 * Search for reserved items by ID
 */
RESERVED(reserved_listid)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH*2];
    int count = 0;
    WNUM wnum;
    long target_auid = 0;
    long target_vnum = 0;
    int type = -1;
    int name_width = 23; // Default width
    int max_name_len = 0;
    int table_width = 80;
    int wnum_width = 18;
    int desc_width = 26; // Default width
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    
    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);
    
    if (arg1[0] == '\0') {
        send_to_char("Syntax: reserved listid <id> [type]\n\r", ch);
        return false;
    }
    
    if (arg1[0] == '\0') {
        send_to_char("Syntax: reserved listid <wnum> [type]\n\r", ch);
        return false;
    }
    
    /* Check for optional type filter */
    if (arg2[0] != '\0') {
        for (int i = 0; reserved_types[i].name; i++) {
            if (!str_prefix(arg2, reserved_types[i].name)) {
                type = reserved_types[i].type;
                break;
            }
        }
        
        if (type == -1) {
            send_to_char("Unknown reserved type. Valid types are: mob, obj, room, area, skill, flag, command\n\r", ch);
            return false;
        }
    }
    
    if (type == RESERVED_AREA) {
        if (!reserved_parse_area_uid(arg1, &target_auid)) {
            send_to_char("Area must be a valid UID or area name.\n\r", ch);
            return false;
        }
    } else {
        if (!reserved_parse_wnum(arg1, &wnum)) {
            send_to_char("WNUM must be a valid widevnum (e.g. 5#1234 or #1234).\n\r", ch);
            return false;
        }
        target_auid = wnum.pArea ? wnum.pArea->uid : 0;
        target_vnum = wnum.vnum;
    }

    /* First pass: determine longest name for dynamic sizing */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            /* Skip if not matching ID or type filter */
            if (type == RESERVED_AREA) {
                long auid = reserved->wnum.auid;
                if (auid <= 0) {
                    AREA_DATA *fallback = reserved_default_area();
                    auid = fallback ? fallback->uid : 0;
                }
                if (auid != target_auid)
                    continue;
            } else {
                long auid = reserved->wnum.auid;
                if (auid <= 0) {
                    AREA_DATA *fallback = reserved_default_area();
                    auid = fallback ? fallback->uid : 0;
                }
                if (auid != target_auid || reserved->wnum.vnum != target_vnum)
                    continue;
            }
                
            if (type != -1 && reserved->type != type)
                continue;
            
            int name_len = strlen(reserved->name);
            if (name_len > max_name_len) 
                max_name_len = name_len;
        }
        iterator_stop(&it);
        
        /* Calculate optimal column width (clamped between 15 and 40) */
        name_width = UMAX(15, UMIN(40, max_name_len + 2));
        
        /* Adjust description width to keep table width at 80 chars */
        desc_width = table_width - (name_width + 10 + wnum_width + 7 + 6);
    }
    
    buffer = new_buf();
    
        {
            char wnum_label[MSL];
            AREA_DATA *label_area = get_area_index(target_auid);
            if (!label_area)
                label_area = reserved_default_area();
            if (type == RESERVED_AREA)
                snprintf(wnum_label, sizeof(wnum_label), "%ld", target_auid);
            else
                snprintf(wnum_label, sizeof(wnum_label), "%s", widevnum_string(label_area, target_vnum, NULL));

            sprintf(buf, "Reserved items with WNUM %s%s%s:\n\r\n\r", 
                wnum_label,
            type != -1 ? " of type " : "",
            type != -1 ? reserved_types_get_name(type) : "");
        }
    add_buf(buffer, buf);
    
    /* Generate divider line based on column widths */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    /* Generate header line */
        sprintf(buf, "{Y| {W%-*s{x | {W%-10s{x | {W%-*s{x | {W%-7s{x | {W%-*s{x |{x\n\r",
            name_width, "Name", "Type", wnum_width, "WNUM", "Remove", desc_width, "Description");
    add_buf(buffer, buf);
    
    /* Repeat divider line */
        sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            wnum_width, "--------------------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            const char *type_name = "unknown";
            
            /* Skip if not matching ID or type filter */
            if (type == RESERVED_AREA) {
                long auid = reserved->wnum.auid;
                if (auid <= 0) {
                    AREA_DATA *fallback = reserved_default_area();
                    auid = fallback ? fallback->uid : 0;
                }
                if (auid != target_auid)
                    continue;
            } else {
                long auid = reserved->wnum.auid;
                if (auid <= 0) {
                    AREA_DATA *fallback = reserved_default_area();
                    auid = fallback ? fallback->uid : 0;
                }
                if (auid != target_auid || reserved->wnum.vnum != target_vnum)
                    continue;
            }
                
            if (type != -1 && reserved->type != type)
                continue;
                
            count++;
            
            /* Get type name for display */
            for (int i = 0; reserved_types[i].name; i++) {
                if (reserved_types[i].type == reserved->type) {
                    type_name = reserved_types[i].name;
                    break;
                }
            }
            
            /* Format name and truncate if needed */
            char name_buf[50];
            if (strlen(reserved->name) > name_width) {
                strncpy(name_buf, reserved->name, name_width - 3);
                name_buf[name_width - 3] = '\0';
                strcat(name_buf, "...");
            } else {
                strcpy(name_buf, reserved->name);
            }
            
            /* Format description for display (truncate if needed) */
            char desc_buf[50];
            if (reserved->description && reserved->description[0]) {
                strncpy(desc_buf, reserved->description, desc_width - 3);
                desc_buf[desc_width - 3] = '\0';
                if (strlen(reserved->description) > desc_width - 3)
                    strcat(desc_buf, "...");
            } else {
                strcpy(desc_buf, "(none)");
            }
            
            sprintf(buf, "{Y| {C%-*s{x | {B%-10s{x | {Y%-*s{x | {%c%-7s{x | %-*s |{x\n\r",
                name_width, name_buf,
                type_name,
                wnum_width, reserved_wnum_string(reserved),
                reserved->removable ? 'G' : 'R',
                reserved->removable ? "Yes" : "No",
                desc_width, desc_buf);
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    }
    
    if (count == 0) {
        sprintf(buf, "{Y| %-*s |{x\n\r", table_width - 4, "No reserved items with that ID found.");
        add_buf(buffer, buf);
    }
    
    /* Table footer */
    sprintf(buf, "{Y+-%.*s-+-%.*s-+-%.*s-+-%.*s-+-%.*s-+{x\n\r",
            name_width, "-------------------------------------------------------------------------",
            10, "------------",
            6, "--------",
            7, "---------",
            desc_width, "-------------------------------------------------------------------");
    add_buf(buffer, buf);
    
    {
        char wnum_label[MSL];
        AREA_DATA *label_area = get_area_index(target_auid);
        if (!label_area)
            label_area = reserved_default_area();
        if (type == RESERVED_AREA)
            snprintf(wnum_label, sizeof(wnum_label), "%ld", target_auid);
        else
            snprintf(wnum_label, sizeof(wnum_label), "%s", widevnum_string(label_area, target_vnum, NULL));

        sprintf(buf, "\n\r%d reserved items found with WNUM %s.\n\r", count, wnum_label);
    }
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return true;
}