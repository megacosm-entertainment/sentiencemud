/***************************************************************************
 *  socialedit.c — OLC Social Command Editor                              *
 *                                                                         *
 *  In-game editor for social commands. Allows immortals to create,       *
 *  modify, and delete social emotes with their various message strings.  *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <math.h>
#include "../../strings.h"
#include "../../merc.h"
#include "../../interp.h"
#include "../../db.h"
#include "../../recycle.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../olc_save.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type socialedit_table[] =
{
    { "?",             show_help                },
    { "charauto",      socialedit_char_auto     },
    { "charfound",     socialedit_char_found    },
    { "charnoarg",     socialedit_char_no_arg   },
    { "charnotfound",  socialedit_char_not_found},
    { "commands",      show_commands            },
    { "create",        socialedit_create        },
    { "delete",        socialedit_delete        },
    { "list",          socialedit_list          },
    { "name",          socialedit_name          },
    { "othersauto",    socialedit_others_auto   },
    { "othersfound",   socialedit_others_found  },
    { "othersnoarg",   socialedit_others_no_arg },
    { "save",          socialedit_save          },
    { "show",          socialedit_show          },
    { "victfound",     socialedit_vict_found    },
    { NULL,            0                        }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF socialedit_def = {
    .name           = "SocialEdit",
    .editor_type    = ED_SOCIAL,
    .cmd_table      = socialedit_table,
    .show_fn        = socialedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_system,
    .perm           = {
        .flags          = OLC_PERM_SECURITY_LEVEL,
        .min_security   = 9
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_socialedit - Enter the social editor
 *
 * Syntax:
 *   socialedit <social name>    - Edit an existing social
 *   socialedit create <name>    - Create a new social
 *   socialedit list             - List all socials
 *   socialedit save             - Save social table to disk
 */
void do_socialedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &socialedit_def, NULL)) {
        send_to_char("SocialEdit: Insufficient security.\n\r", ch);
        return;
    }

    argument = one_argument(argument, command);

    if (command[0] == '\0') {
        send_to_char("Syntax: socialedit <social name>\n\r", ch);
        send_to_char("        socialedit create <social name>\n\r", ch);
        send_to_char("        socialedit list\n\r", ch);
        send_to_char("        socialedit save\n\r", ch);
        return;
    }

    if (!str_cmp(command, "list")) {
        socialedit_list(ch, argument);
        return;
    }

    if (!str_cmp(command, "save")) {
        socialedit_save(ch, argument);
        return;
    }

    if (!str_cmp(command, "create")) {
        if (argument[0] == '\0') {
            send_to_char("Syntax: socialedit create <social name>\n\r", ch);
            return;
        }
        socialedit_create(ch, argument);
        return;
    }

    /* Find the social */
    for (int i = 0; i < social_count; i++) {
        if (!str_prefix(command, social_table[i].name)) {
            olc_editor_enter(ch, &socialedit_def, &social_table[i], true);
            return;
        }
    }

    send_to_char("That social doesn't exist. Use 'socialedit create' to create a new one.\n\r", ch);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * socialedit - Command interpreter for the social editor
 */
void socialedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &socialedit_def);
}

/***************************************************************************
 * Commands                                                                *
 ***************************************************************************/

/**
 * socialedit_show - Display current social data
 *
 * Uses the unified OLC display framework for consistent formatting.
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
SOCEDIT(socialedit_show)
{
    struct social_type *social;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&socialedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_SOCIAL(ch, social);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SocialEdit", social->name, NULL, &socialedit_def);

    olc_display_string(ctx, theme, "Name:", "name", social->name);

    olc_display_section(ctx, theme, "No Target");
    olc_display_string(ctx, theme, "Char:", "charnoarg", social->char_no_arg);
    olc_display_string(ctx, theme, "Others:", "othersnoarg", social->others_no_arg);

    olc_display_section(ctx, theme, "With Target");
    olc_display_string(ctx, theme, "Char:", "charfound", social->char_found);
    olc_display_string(ctx, theme, "Others:", "othersfound", social->others_found);
    olc_display_string(ctx, theme, "Victim:", "victfound", social->vict_found);

    olc_display_section(ctx, theme, "Target Not Found");
    olc_display_string(ctx, theme, "Char:", "charnotfound", social->char_not_found);

    olc_display_section(ctx, theme, "Self Target");
    olc_display_string(ctx, theme, "Char:", "charauto", social->char_auto);
    olc_display_string(ctx, theme, "Others:", "othersauto", social->others_auto);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

SOCEDIT(socialedit_list)
{
    char buf[MAX_STRING_LENGTH];
    int col = 0;
    
    send_to_char("Social Commands:\n\r", ch);
    
    for (int i = 0; i < social_count; i++) {
        sprintf(buf, "%-15.15s", social_table[i].name);
        send_to_char(buf, ch);
        if (++col % 5 == 0)
            send_to_char("\n\r", ch);
    }
    
    if (col % 5 != 0)
        send_to_char("\n\r", ch);
    
    return true;
}

SOCEDIT(socialedit_create)
{
    struct social_type social;
    //struct social_type *temp_table;
    char buf[MAX_STRING_LENGTH];
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: socialedit create <social name>\n\r", ch);
        return false;
    }
    
    /* Check if social already exists */
    for (int i = 0; i < social_count; i++) {
        if (!str_cmp(argument, social_table[i].name)) {
            send_to_char("That social already exists.\n\r", ch);
            return false;
        }
    }
    
    /* Check if we've reached the maximum number of socials */
    if (social_count >= MAX_SOCIALS) {
        sprintf(buf, "Cannot add more socials. Max is %d.\n\r", MAX_SOCIALS);
        send_to_char(buf, ch);
        return false;
    }
    
    /* Create a new social */
    social.char_no_arg = NULL;
    social.others_no_arg = NULL;
    social.char_found = NULL;
    social.others_found = NULL;
    social.vict_found = NULL;
    social.char_not_found = NULL;
    social.char_auto = NULL;
    social.others_auto = NULL;
    
    strncpy(social.name, argument, 20);
    
    /* Add to social table */
    social_table[social_count] = social;
    
    /* Edit the newly created social */
    ch->desc->pEdit = (void *)&social_table[social_count];
    ch->desc->editor = ED_SOCIAL;
    
    social_count++;
    
    send_to_char("Social created.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_name)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: name <social name>\n\r", ch);
        return false;
    }
    
    /* Check if social name already exists */
    for (int i = 0; i < social_count; i++) {
        if (!str_cmp(argument, social_table[i].name) && 
            &social_table[i] != social) {
            send_to_char("That social name already exists.\n\r", ch);
            return false;
        }
    }
    
    strncpy(social->name, argument, 20);
    send_to_char("Social name changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_char_no_arg)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Char No Arg", NULL,
        &social->char_no_arg, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_others_no_arg)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Others No Arg", NULL,
        &social->others_no_arg, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_char_found)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Char Found", NULL,
        &social->char_found, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_others_found)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Others Found", NULL,
        &social->others_found, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_vict_found)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Victim Found", NULL,
        &social->vict_found, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_char_not_found)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Char Not Found", NULL,
        &social->char_not_found, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_char_auto)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Char Auto", NULL,
        &social->char_auto, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_others_auto)
{
    struct social_type *social;
    EDIT_SOCIAL(ch, social);
    return olc_cmd_string(ch, argument, "Others Auto", NULL,
        &social->others_auto, OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, NULL, NULL);
}

SOCEDIT(socialedit_delete)
{
    struct social_type *social;
    int i, pos = -1;
    
    EDIT_SOCIAL(ch, social);
    
    /* Find position of the social in the table */
    for (i = 0; i < social_count; i++) {
        if (!str_cmp(social->name, social_table[i].name)) {
            pos = i;
            break;
        }
    }
    
    if (pos < 0) {
        send_to_char("Error: Couldn't find social in table.\n\r", ch);
        return false;
    }
    
    /* Free memory used by this social */
    if (social->char_no_arg)
        free_string(social->char_no_arg);
    if (social->others_no_arg)
        free_string(social->others_no_arg);
    if (social->char_found)
        free_string(social->char_found);
    if (social->others_found)
        free_string(social->others_found);
    if (social->vict_found)
        free_string(social->vict_found);
    if (social->char_not_found)
        free_string(social->char_not_found);
    if (social->char_auto)
        free_string(social->char_auto);
    if (social->others_auto)
        free_string(social->others_auto);
    
    /* Shift all socials above this one down one position */
    for (i = pos; i < social_count - 1; i++) {
        social_table[i] = social_table[i + 1];
    }
    
    social_count--;
    
    send_to_char("Social deleted.\n\r", ch);
    edit_done(ch);
    return true;
}

SOCEDIT(socialedit_save)
{
    FILE *fp;
    int i;
    
    if ((fp = fopen(SOCIALS_FILE, "w")) == NULL) {
        send_to_char("Error: Could not open file for writing.\n\r", ch);
        return false;
    }
    
    fprintf(fp, "#SOCIALS\n\n");
    
    for (i = 0; i < social_count; i++) {
        fprintf(fp, "%s\n", social_table[i].name);
        
        if (social_table[i].char_no_arg)
            fprintf(fp, "%s\n", social_table[i].char_no_arg);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].others_no_arg)
            fprintf(fp, "%s\n", social_table[i].others_no_arg);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].char_found)
            fprintf(fp, "%s\n", social_table[i].char_found);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].others_found)
            fprintf(fp, "%s\n", social_table[i].others_found);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].vict_found)
            fprintf(fp, "%s\n", social_table[i].vict_found);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].char_not_found)
            fprintf(fp, "%s\n", social_table[i].char_not_found);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].char_auto)
            fprintf(fp, "%s\n", social_table[i].char_auto);
        else
            fprintf(fp, "$\n");
            
        if (social_table[i].others_auto)
            fprintf(fp, "%s\n", social_table[i].others_auto);
        else
            fprintf(fp, "$\n");
            
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "#0\n");
    
    fclose(fp);
    send_to_char("Social table saved.\n\r", ch);
    return true;
}