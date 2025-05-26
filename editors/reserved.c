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
#include "../merc.h"
#include "../interp.h"
#include "../tables.h"
#include "../olc.h"
#include "../recycle.h"
#include "../scripts.h"
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

extern LLIST *reserved_vnums;
bool reserved_changed = false;

/* Reserved types and their text names */
const struct reserved_type_name {
    const char *name;
    int type;
} reserved_types[] = {
    { "mob",    RESERVED_MOB    },
    { "obj",    RESERVED_OBJ    },
    { "room",   RESERVED_ROOM   },
    { "area",   RESERVED_AREA   },
    { "skill",  RESERVED_SKILL  },
    { "flag",   RESERVED_FLAG   },
    { "command",RESERVED_COMMAND},
    { NULL,     -1              }
};

/*
 * Load reserved items from file
 */
void load_reserved(void)
{
    FILE *fp;
    char *word;
    bool in_block = FALSE;
    RESERVED_DATA *reserved = NULL;
    char buf[MSL];
    
    if ((fp = fopen(RESERVED_FILE, "r")) == NULL) {
        log_string("No reserved items file found. Creating new file at save.");
        return;
    }
    
    log_string("Loading reserved items...");
    
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
                in_block = TRUE;
                reserved = alloc_mem(sizeof(RESERVED_DATA));
                reserved->name = NULL;
                reserved->description = NULL;
                reserved->type = -1;
                reserved->removable = FALSE;
                reserved->id = 0;
            } else if (!str_cmp(word, "#-RESERVED")) {
                if (in_block && reserved) {
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
                    
                    in_block = FALSE;
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
                reserved->id = fread_number(fp);
            } else if (!str_cmp(word, "Description")) {
                reserved->description = fread_string(fp);
            } else {
                sprintf(buf, "Load_reserved: unknown field '%s'", word);
                bug(buf, 0);
                fread_to_eol(fp);
            }
        } else {
            sprintf(buf, "Load_reserved: unexpected data outside block: %s", word);
            bug(buf, 0);
            fread_to_eol(fp);
        }
    }
    
    fclose(fp);
    
    log_string(formatf("%d reserved items loaded.", list_size(reserved_vnums)));
    reserved_changed = FALSE;
}

/*
 * Save all reserved items to file
 */
void save_reserved(void)
{
    FILE *fp;
    ITERATOR it;
    RESERVED_DATA *reserved;
    
    if (!reserved_changed) {
        log_string("Reserved items unchanged, not saving.");
        return;
    }
    
    if ((fp = fopen(RESERVED_FILE, "w")) == NULL) {
        bug("Save_reserved: couldn't open file for writing", 0);
        return;
    }
    
    /* Write header */
    fprintf(fp, "# Reserved Items File\n");
    fprintf(fp, "# Generated %s\n\n", ctime(&current_time));
    
    /* Write each item */
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            const char *type_name = "unknown";
            
            /* Get type name */
            for (int i = 0; reserved_types[i].name; i++) {
                if (reserved_types[i].type == reserved->type) {
                    type_name = reserved_types[i].name;
                    break;
                }
            }
            
            fprintf(fp, "#RESERVED\n");
            fprintf(fp, "Name %s~\n", reserved->name);
            fprintf(fp, "Type %s\n", type_name);
            fprintf(fp, "Removable %d\n", reserved->removable ? 1 : 0);
            fprintf(fp, "ID %d\n", reserved->id);
            if (reserved->description && reserved->description[0])
                fprintf(fp, "Description %s~\n", reserved->description);
            fprintf(fp, "#-RESERVED\n\n");
        }
        iterator_stop(&it);
    }
    
    /* Write footer */
    fprintf(fp, "#END\n");
    fclose(fp);
    
    log_string(formatf("%d reserved items saved to '%s'.", 
                     list_size(reserved_vnums), RESERVED_FILE));
    reserved_changed = FALSE;
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
        if (reserved->id == id && reserved->type == type) {
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
    char buf[MAX_STRING_LENGTH];
    
    if (reserved)
        return reserved->id;
    sprintf(buf, "get_reserved_vnum: Reserved item '%s' not found", name);
    bug(buf, 0);
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
        send_to_char("  reserved show <name>     - Show details of a reserved item\n\r", ch);
        send_to_char("  reserved add <name> <type> <id> [removable]  - Add a new reserved item\n\r", ch);
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
            return FALSE;
        }
    }
    
    buffer = new_buf();
    
    /* Table header */
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    add_buf(buffer, "{Y| {WName{x                 | {WType{x       | {WID{x     | {WRemove{x  | {WDescription{x           |{x\n\r");
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    
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
            
            /* Format description for display (truncate if needed) */
            char desc_buf[31];
            if (reserved->description && reserved->description[0]) {
                strncpy(desc_buf, reserved->description, 30);
                desc_buf[30] = '\0';
                if (strlen(reserved->description) > 30)
                    strcat(desc_buf, "...");
            } else {
                strcpy(desc_buf, "(none)");
            }
            
            sprintf(buf, "{Y| {C%-22s{x | {B%-10s{x | {Y%-6d{x | {%c%-7s{x | %-23s |{x\n\r",
                reserved->name,
                type_name,
                reserved->id,
                reserved->removable ? 'G' : 'R',
                reserved->removable ? "Yes" : "No",
                desc_buf);
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    }
    
    if (count == 0) {
        sprintf(buf, "{Y| %-74s |{x\n\r", "No reserved items found.");
        add_buf(buffer, buf);
    }
    
    /* Table footer */
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    
    sprintf(buf, "\n\r%d reserved items listed.\n\r", count);
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return TRUE;
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
        return FALSE;
    }
    
    reserved = find_reserved(arg);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return FALSE;
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
    
    sprintf(buf, "{Y| {WID:{x %-74d {Y|{x\n\r", reserved->id);
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
    
    sprintf(buf, "{Y| {xIn code: {W%d = get_reserved_vnum(\"%s\"){x %-32s {Y|{x\n\r", reserved->id, reserved->name, "");
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
    
    return TRUE;
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
    int id;
    bool removable = TRUE;
    RESERVED_DATA *reserved;
    
    argument = one_argument(argument, name);
    argument = one_argument(argument, type_str);
    argument = one_argument(argument, id_str);
    argument = one_argument(argument, removable_str);
    
    if (name[0] == '\0' || type_str[0] == '\0' || id_str[0] == '\0') {
        send_to_char("Syntax: reserved add <name> <type> <id> [removable]\n\r", ch);
        send_to_char("Types: mob, obj, room, area, skill, flag, command\n\r", ch);
        return FALSE;
    }
    
    /* Check for existing item with same name */
    if (find_reserved(name)) {
        send_to_char("A reserved item with that name already exists.\n\r", ch);
        return FALSE;
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
        return FALSE;
    }
    
    /* Validate ID */
    if (!is_number(id_str)) {
        send_to_char("ID must be a number.\n\r", ch);
        return FALSE;
    }
    id = atoi(id_str);
    
    /* Check for removable flag */
    if (removable_str[0] != '\0') {
        if (!str_prefix(removable_str, "permanent") || !str_prefix(removable_str, "no") || 
            !str_prefix(removable_str, "false") || !str_cmp(removable_str, "0")) {
            removable = FALSE;
        }
    }
    
    /* Check for duplicate ID of same type */
    reserved = find_reserved_by_id(id, type);
    if (reserved) {
        send_to_char(formatf("Warning: An item of the same type with ID %d already exists: %s\n\r",
            id, reserved->name), ch);
        send_to_char("Adding anyway...\n\r", ch);
    }
    
    /* Create the new reserved item */
    reserved = alloc_mem(sizeof(RESERVED_DATA));
    reserved->name = str_dup(name);
    reserved->description = str_dup(argument);
    reserved->type = type;
    reserved->id = id;
    reserved->removable = removable;
    
    /* Add to list */
    if (!reserved_vnums)
        reserved_vnums = list_create(FALSE);
    list_appendlink(reserved_vnums, reserved);
    
    reserved_changed = TRUE;
    
    send_to_char(formatf("Added reserved item: %s (type: %s, ID: %d)\n\r", 
        name, reserved_types_get_name(type), id), ch);
    
    return TRUE;
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
        return FALSE;
    }
    
    reserved = find_reserved(arg);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return FALSE;
    }
    
    if (!reserved->removable) {
        send_to_char("This reserved item cannot be removed. It is marked as permanent.\n\r", ch);
        return FALSE;
    }
    
    /* Require confirmation */
    if (str_cmp(confirm, "confirm")) {
        send_to_char(formatf("Are you sure you want to delete reserved item '%s'?\n\r", reserved->name), ch);
        send_to_char("Type 'reserved delete <name> confirm' to confirm.\n\r", ch);
        return FALSE;
    }
    
    /* Remove from list */

    if (list_haslink(reserved_vnums, reserved)) {
        /* Remove from the list */
        list_remlink(reserved_vnums, reserved, true);
        free_string(reserved->name);
        free_string(reserved->description);
        free_mem(reserved, sizeof(RESERVED_DATA));
        reserved_changed = TRUE;
    } else {
        send_to_char("Reserved item not found in the list.\n\r", ch);
        return FALSE;
    }

        
        send_to_char("Reserved item deleted.\n\r", ch);
        
        /* Auto-save after deletion */
        save_reserved();
        
        return TRUE;
    
    
    send_to_char("Error removing reserved item from list.\n\r", ch);
    return FALSE;
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
        send_to_char("Fields: name, type, id, removable, description\n\r", ch);
        return FALSE;
    }
    
    reserved = find_reserved(name);
    
    if (!reserved) {
        send_to_char("No reserved item with that name exists.\n\r", ch);
        return FALSE;
    }
    
    if (!str_cmp(field, "name")) {
        if (argument[0] == '\0') {
            send_to_char("You must specify a new name.\n\r", ch);
            return FALSE;
        }
        
        /* Check if new name already exists */
        if (find_reserved(argument)) {
            send_to_char("A reserved item with that name already exists.\n\r", ch);
            return FALSE;
        }
        
        free_string(reserved->name);
        reserved->name = str_dup(argument);
        send_to_char("Reserved item name changed.\n\r", ch);
        reserved_changed = TRUE;
    }
    else if (!str_cmp(field, "type")) {
        int type = -1;
        
        if (argument[0] == '\0') {
            send_to_char("You must specify a type: mob, obj, room, area, skill, flag, command\n\r", ch);
            return FALSE;
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
            return FALSE;
        }
        
        reserved->type = type;
        send_to_char("Reserved item type changed.\n\r", ch);
        reserved_changed = TRUE;
    }
    else if (!str_cmp(field, "id")) {
        int id;
        
        if (argument[0] == '\0' || !is_number(argument)) {
            send_to_char("You must specify a numeric ID.\n\r", ch);
            return FALSE;
        }
        
        id = atoi(argument);
        
        /* Check for duplicate ID of same type */
        RESERVED_DATA *existing = find_reserved_by_id(id, reserved->type);
        if (existing && existing != reserved) {
            send_to_char(formatf("Warning: An item of the same type with ID %d already exists: %s\n\r",
                id, existing->name), ch);
            send_to_char("Changing anyway...\n\r", ch);
        }
        
        reserved->id = id;
        send_to_char("Reserved item ID changed.\n\r", ch);
        reserved_changed = TRUE;
    }
    else if (!str_cmp(field, "removable")) {
        if (!reserved->removable && get_staff_rank(ch) < STAFF_IMPLEMENTOR) {
            send_to_char("Only implementors can change permanent reserved items to removable.\n\r", ch);
            return FALSE;
        }
        
        if (argument[0] == '\0') {
            reserved->removable = !reserved->removable;
        } else {
            if (!str_prefix(argument, "yes") || !str_prefix(argument, "true") ||
                !str_prefix(argument, "on") || !str_cmp(argument, "1")) {
                reserved->removable = TRUE;
            } else {
                reserved->removable = FALSE;
            }
        }
        
        send_to_char(formatf("Reserved item is now %s.\n\r", reserved->removable ? "removable" : "permanent"), ch);
        reserved_changed = TRUE;
    }
    else if (!str_cmp(field, "description")) {
        if (argument[0] == '\0') {
            /* Start string editor */
            string_append(ch, &reserved->description);
            reserved_changed = TRUE;
            return TRUE;
        } else {
            free_string(reserved->description);
            reserved->description = str_dup(argument);
            send_to_char("Reserved item description changed.\n\r", ch);
            reserved_changed = TRUE;
        }
    }
    else {
        send_to_char("Invalid field. Fields are: name, type, id, removable, description\n\r", ch);
        return FALSE;
    }
    
    return TRUE;
}

/*
 * Save reserved items
 */
RESERVED(reserved_save)
{
    save_reserved();
    send_to_char("Reserved items saved.\n\r", ch);
    return TRUE;
}

/*
 * Search for reserved items
 */
RESERVED(reserved_search)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int count = 0;
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: reserved search <string>\n\r", ch);
        return FALSE;
    }
    
    buffer = new_buf();
    
    /* Table header */
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    add_buf(buffer, "{Y| {WName{x                 | {WType{x       | {WID{x     | {WRemove{x  | {WDescription{x           |{x\n\r");
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    
    if (reserved_vnums && list_size(reserved_vnums) > 0) {
        ITERATOR it;
        RESERVED_DATA *reserved;
        
        iterator_start(&it, reserved_vnums);
        while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
            const char *type_name = "unknown";
            
            /* Check if name or description contains the search string */
            if ((!reserved->name || !strstr(reserved->name, argument)) &&
                (!reserved->description || !strstr(reserved->description, argument))) {
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
            
            /* Format description for display (truncate if needed) */
            char desc_buf[31];
            if (reserved->description && reserved->description[0]) {
                strncpy(desc_buf, reserved->description, 30);
                desc_buf[30] = '\0';
                if (strlen(reserved->description) > 30)
                    strcat(desc_buf, "...");
            } else {
                strcpy(desc_buf, "(none)");
            }
            
            sprintf(buf, "{Y| {C%-22s{x | {B%-10s{x | {Y%-6d{x | {%c%-7s{x | %-23s |{x\n\r",
                reserved->name,
                type_name,
                reserved->id,
                reserved->removable ? 'G' : 'R',
                reserved->removable ? "Yes" : "No",
                desc_buf);
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    }
    
    if (count == 0) {
        sprintf(buf, "{Y| %-74s |{x\n\r", "No matching reserved items found.");
        add_buf(buffer, buf);
    }
    
    /* Table footer */
    add_buf(buffer, "{Y+------------------------+------------+--------+---------+-------------------------+{x\n\r");
    
    sprintf(buf, "\n\r%d reserved items matched your search.\n\r", count);
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return TRUE;
}

/*
 * Import defines from header files
 */
RESERVED(reserved_import)
{
    send_to_char("This functionality is not yet implemented.\n\r", ch);
    send_to_char("To import defines, you would need to specify the file to parse.\n\r", ch);
    return FALSE;
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
            sprintf(buf, "#define %-30s %d", reserved->name, reserved->id);
            
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
            
            strcat(buf, "\n");
            add_buf(buffer, buf);
        }
        iterator_stop(&it);
    } else {
        add_buf(buffer, "/* No reserved items found */\n");
    }
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return TRUE;
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
        { "MOB_VNUM_DEATH", RESERVED_MOB, 6502, FALSE, "Death mob used for death handling" },
        { "OBJ_VNUM_SILVER_ONE", RESERVED_OBJ, 1, FALSE, "One silver coin" },
        { "ROOM_VNUM_LIMBO", RESERVED_ROOM, 2, FALSE, "Limbo room for storage" },
        /* Add more defaults here */
        { NULL, 0, 0, FALSE, NULL }
    };
    
    if (!reserved_vnums)
        reserved_vnums = list_create(FALSE);
    
    for (int i = 0; defaults[i].name; i++) {
        RESERVED_DATA *reserved = alloc_mem(sizeof(RESERVED_DATA));
        reserved->name = str_dup(defaults[i].name);
        reserved->type = defaults[i].type;
        reserved->id = defaults[i].id;
        reserved->removable = defaults[i].removable;
        reserved->description = str_dup(defaults[i].description);
        
        list_appendlink(reserved_vnums, reserved);
        reserved_changed = TRUE;
    }
    
    log_string("Default reserved items loaded.");
}