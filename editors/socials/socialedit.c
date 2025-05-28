/***************************************************************************
 *                                                                         *
 *    Social editor - allows in-game editing of social commands            *
 *                                                                         *
 **************************************************************************/

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



/* Entry point for editing social table. */
void do_socialedit(CHAR_DATA *ch, char *argument)
{
    //struct social_type *social;
    char command[MAX_INPUT_LENGTH];

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
            ch->desc->pEdit = (void *)&social_table[i];
            ch->desc->editor = ED_SOCIAL;
            send_to_char("Social found. Beginning edit mode.\n\r", ch);
            socialedit_show(ch, "");
            return;
        }
    }

    send_to_char("That social doesn't exist. Use 'socialedit create' to create a new one.\n\r", ch);
    return;
}

SOCEDIT(socialedit_show)
{
    struct social_type *social;
    char buf[MAX_STRING_LENGTH];

    EDIT_SOCIAL(ch, social);

    sprintf(buf, "Name:               [%s]\n\r", social->name);
    send_to_char(buf, ch);

    sprintf(buf, "Char no argument:   [%s]\n\r", 
            social->char_no_arg ? social->char_no_arg : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Others no argument: [%s]\n\r", 
            social->others_no_arg ? social->others_no_arg : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Char found:         [%s]\n\r", 
            social->char_found ? social->char_found : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Others found:       [%s]\n\r", 
            social->others_found ? social->others_found : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Victim found:       [%s]\n\r", 
            social->vict_found ? social->vict_found : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Char not found:     [%s]\n\r", 
            social->char_not_found ? social->char_not_found : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Char auto:          [%s]\n\r", 
            social->char_auto ? social->char_auto : "NULL");
    send_to_char(buf, ch);

    sprintf(buf, "Others auto:        [%s]\n\r", 
            social->others_auto ? social->others_auto : "NULL");
    send_to_char(buf, ch);

    return true;
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
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: charnoarg <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->char_no_arg)
        free_string(social->char_no_arg);
    
    if (!str_cmp(argument, "$"))
        social->char_no_arg = NULL;
    else
        social->char_no_arg = str_dup(argument);
    
    send_to_char("Character no argument string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_others_no_arg)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: othersnoarg <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->others_no_arg)
        free_string(social->others_no_arg);
    
    if (!str_cmp(argument, "$"))
        social->others_no_arg = NULL;
    else
        social->others_no_arg = str_dup(argument);
    
    send_to_char("Others no argument string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_char_found)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: charfound <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->char_found)
        free_string(social->char_found);
    
    if (!str_cmp(argument, "$"))
        social->char_found = NULL;
    else
        social->char_found = str_dup(argument);
    
    send_to_char("Character found string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_others_found)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: othersfound <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->others_found)
        free_string(social->others_found);
    
    if (!str_cmp(argument, "$"))
        social->others_found = NULL;
    else
        social->others_found = str_dup(argument);
    
    send_to_char("Others found string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_vict_found)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: victfound <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->vict_found)
        free_string(social->vict_found);
    
    if (!str_cmp(argument, "$"))
        social->vict_found = NULL;
    else
        social->vict_found = str_dup(argument);
    
    send_to_char("Victim found string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_char_not_found)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: charnotfound <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->char_not_found)
        free_string(social->char_not_found);
    
    if (!str_cmp(argument, "$"))
        social->char_not_found = NULL;
    else
        social->char_not_found = str_dup(argument);
    
    send_to_char("Character not found string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_char_auto)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: charauto <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->char_auto)
        free_string(social->char_auto);
    
    if (!str_cmp(argument, "$"))
        social->char_auto = NULL;
    else
        social->char_auto = str_dup(argument);
    
    send_to_char("Character auto string changed.\n\r", ch);
    return true;
}

SOCEDIT(socialedit_others_auto)
{
    struct social_type *social;
    
    EDIT_SOCIAL(ch, social);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: othersauto <string>\n\r", ch);
        send_to_char("Use $ to clear the string.\n\r", ch);
        return false;
    }
    
    if (social->others_auto)
        free_string(social->others_auto);
    
    if (!str_cmp(argument, "$"))
        social->others_auto = NULL;
    else
        social->others_auto = str_dup(argument);
    
    send_to_char("Others auto string changed.\n\r", ch);
    return true;
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