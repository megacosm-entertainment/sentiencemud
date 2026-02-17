/***************************************************************************
 *  hedit.c — OLC Help File & Category Editor                             *
 *                                                                         *
 *  Dual-mode in-game editor for help files and help categories.           *
 *  When pEdit is NULL, operates in category browser mode.                 *
 *  When pEdit is set, operates in help entry editing mode.                *
 *  The "done" command toggles between modes (entry → category → exit).   *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

/***************************************************************************
 * Change Tracking                                                         *
 ***************************************************************************/

/**
 * hedit_mark_changed - Update modification timestamps
 *
 * Sets the modified_by and modified timestamp on either the
 * help entry (if editing one) or the current category.
 *
 * @param ch     Character who made the change
 * @param pEdit  HELP_DATA being edited (may be NULL for category mode)
 */
static void hedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    if (pEdit != NULL) {
        HELP_DATA *help = (HELP_DATA *)pEdit;
        free_string(help->modified_by);
        help->modified_by = str_dup(ch->name);
        help->modified = current_time;
    } else if (ch->desc && ch->desc->hCat) {
        free_string(ch->desc->hCat->modified_by);
        ch->desc->hCat->modified_by = str_dup(ch->name);
        ch->desc->hCat->modified = current_time;
    }
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type hedit_table[] =
{
    { "addcategory",  hedit_addcat      },
    { "addtopic",     hedit_addtopic    },
    { "builder",      hedit_builder     },
    { "commands",     show_commands     },
    { "delete",       hedit_delete      },
    { "description",  hedit_description },
    { "edit",         hedit_edit        },
    { "keyword",      hedit_keywords    },
    { "level",        hedit_level       },
    { "make",         hedit_make        },
    { "move",         hedit_move        },
    { "name",         hedit_name        },
    { "opencategory", hedit_opencat     },
    { "remcategory",  hedit_remcat      },
    { "remtopic",     hedit_remtopic    },
    { "security",     hedit_security    },
    { "shiftcategory",hedit_shiftcat    },
    { "show",         hedit_show        },
    { "text",         hedit_text        },
    { "upcategory",   hedit_upcat       },
    { NULL,           0                 }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF hedit_def = {
    .name           = "HEdit",
    .editor_type    = ED_HELP,
    .cmd_table      = hedit_table,
    .show_fn        = hedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_system,
    .perm           = {
        .flags          = OLC_PERM_NONE
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = hedit_mark_changed,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_hedit - Enter the help file editor
 *
 * Opens in category browser mode at the root help category.
 *
 * Syntax: hedit
 */
void do_hedit(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
        return;

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->editor = ED_HELP;
    ch->desc->pEdit = NULL;
    ch->desc->hCat = topHelpCat;
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * hedit - Command interpreter for the help editor
 *
 * Custom interpreter required because hedit has dual-mode "done"
 * behavior: when editing a help entry, "done" returns to category
 * browser mode; when browsing categories, "done" exits the editor.
 */
void hedit(CHAR_DATA *ch, char *argument)
{
    char command[MIL];
    char arg[MIL];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!IS_IMMORTAL(ch)) {
        send_to_char("HEdit: Insufficient security.\n\r", ch);
        edit_done(ch);
        return;
    }

    /* Dual-mode done: entry editing → category browser → exit */
    if (!str_cmp(command, "done")) {
        if (ch->desc->pEdit == NULL)
            edit_done(ch);
        else {
            ch->desc->pEdit = NULL;
            ch->desc->editor = ED_HELP;
        }
        return;
    }

    if (command[0] == '\0') {
        hedit_show(ch, argument);
        return;
    }

    for (cmd = 0; hedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, hedit_table[cmd].name)) {
            if ((*hedit_table[cmd].olc_fun)(ch, argument)) {
                ch->pcdata->immortal->last_olc_command = current_time;
                olc_mark_changed(ch, &hedit_def);
            }
            return;
        }
    }

    interpret(ch, arg);
}

/***************************************************************************
 * Command Handlers                                                        *
 ***************************************************************************/
/**
 * hedit_show - Display current help entry or category data
 *
 * Dual-mode show function. When pEdit is set, shows the help entry.
 * When pEdit is NULL, shows the current category browser view.
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
HEDIT (hedit_show)
{
    const OLC_EDITOR_THEME *theme = olc_get_theme(&hedit_def);
    OLC_LAYOUT_CTX *ctx;

    ctx = olc_display_new(ch, theme);

    if (ch->desc->pEdit != NULL)
    {
        HELP_DATA *help = (HELP_DATA *)ch->desc->pEdit;
        STRING_DATA *topic;
        int i;

        olc_display_header(ctx, "HEdit", help->keyword,
            formatf("#%d", help->index), &hedit_def);

        olc_display_string(ctx, theme, "Keywords:", "keyword", help->keyword);
        olc_display_number(ctx, theme, "Security:", "security", help->security);
        olc_display_string(ctx, theme, "Builders:", "builder", help->builders);
        olc_display_string(ctx, theme, "Created by:", NULL, help->creator);
        olc_display_string(ctx, theme, "Created on:", NULL,
            help->created == 0 ? "Unknown" : (char *)ctime(&help->created));
        olc_display_string(ctx, theme, "Modified by:", NULL, help->modified_by);
        olc_display_string(ctx, theme, "Modified on:", NULL,
            help->modified == 0 ? "Unknown" : (char *)ctime(&help->modified));
        olc_display_string(ctx, theme, "Category:", NULL,
            help->hCat == topHelpCat ? "Root Category" : help->hCat->name);
        olc_display_number(ctx, theme, "Level:", "level", help->min_level);

        olc_display_text(ctx, theme, NULL, "text", help->text);

        olc_display_section(ctx, theme, "Related Topics");
        if (help->related_topics == NULL) {
            olc_display_infof(ctx, theme, "  None");
        } else {
            i = 0;
            for (topic = help->related_topics; topic != NULL; topic = topic->next, i++) {
                olc_display_infof(ctx, theme, "{b[{B%-2d{b]{x %s", i, topic->string);
            }
        }
    }
    else
    {
        HELP_CATEGORY *hcat;
        HELP_DATA *help;
        int i;

        olc_display_header(ctx, "HEdit",
            ch->desc->hCat == topHelpCat ? "Root Category" : ch->desc->hCat->name,
            NULL, &hedit_def);

        olc_display_string(ctx, theme, "Category:", "name",
            ch->desc->hCat == topHelpCat ? "Root Category" : ch->desc->hCat->name);
        olc_display_number(ctx, theme, "Security:", "security",
            ch->desc->hCat->security);
        olc_display_string(ctx, theme, "Builders:", "builder",
            ch->desc->hCat->builders);
        olc_display_string(ctx, theme, "Created by:", NULL,
            ch->desc->hCat->creator);
        olc_display_string(ctx, theme, "Created on:", NULL,
            ch->desc->hCat->created == 0 ? "Unknown"
            : (char *)ctime(&ch->desc->hCat->created));
        olc_display_string(ctx, theme, "Modified by:", NULL,
            ch->desc->hCat->modified_by);
        olc_display_string(ctx, theme, "Modified on:", NULL,
            ch->desc->hCat->modified == 0 ? "Unknown"
            : (char *)ctime(&ch->desc->hCat->modified));
        olc_display_number(ctx, theme, "Level:", "level",
            ch->desc->hCat->min_level);
        olc_display_text(ctx, theme, "Description:", "description",
            ch->desc->hCat->description);

        olc_display_section(ctx, theme, "Contents");

        i = 0;
        for (hcat = ch->desc->hCat->inside_cats; hcat != NULL; hcat = hcat->next) {
            olc_display_infof(ctx, theme, "{b[{BC{b]{x  {W%.18s{B/{x%s",
                hcat->name, ((i + 1) % 4 == 0) ? "" : " ");
            i++;
        }

        for (help = ch->desc->hCat->inside_helps; help != NULL; help = help->next) {
            olc_display_infof(ctx, theme, "{b%-4d{x %-24.24s%s",
                help->index, help->keyword,
                ((i + 1) % 4 == 0) ? "" : " ");
            i++;
        }
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    return false;
}


HEDIT(hedit_make)
{
    HELP_DATA *pHelp;
    //HELP_DATA *pHelpTmp;
    char buf[MSL];
    int i;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax: hedit make [keyword(s)]\n\r",ch);
    return false;
    }

    if (ch->desc->pEdit != NULL)
    {
        send_to_char("You are already editing a helpfile.\n\r", ch);
    return false;
    }

    if (ch->desc->hCat == topHelpCat) {
    send_to_char("You can only add categories in the root category.\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    pHelp = new_help();
    ch->desc->pEdit = (void *)pHelp;

    /* All keywords are in caps */
    i = 0;
    while (argument[i] != '\0')
    {
    argument[i] = UPPER(argument[i]);
    i++;
    }

    pHelp->keyword  = str_dup(argument);
    pHelp->creator  = str_dup(ch->name);
    pHelp->modified = current_time;
    free_string(pHelp->modified_by);
    pHelp->modified_by = str_dup(ch->name);
    pHelp->created = current_time;
    pHelp->modified = current_time;
    pHelp->hCat     = ch->desc->hCat;
    pHelp->index    = ++top_help_index;

    pHelp->min_level = pHelp->hCat->min_level;
    pHelp->security = pHelp->hCat->security;

    if (ch->desc->hCat->inside_helps == NULL)
    ch->desc->hCat->inside_helps = pHelp;
    else
    insert_help(pHelp, &ch->desc->hCat->inside_helps);

    sprintf(buf, "New help entry %s created inside %s.\n\r", pHelp->keyword, pHelp->hCat->name);
    return true;
}


HEDIT(hedit_edit)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcat;

    if (argument[0] == '\0')
    {
    send_to_char("Syntax: hedit edit <keyword(s)>\n\r",ch);
    return false;
    }

    // Look in current category first
    for (help = ch->desc->hCat->inside_helps; help != NULL; help = help->next) {
    if (!str_prefix(argument, help->keyword))
        break;
    }

    if (help == NULL)
    {
    // Look inside
    for (hcat = ch->desc->hCat->inside_cats; hcat != NULL; hcat = hcat->next) {
        if ((help = find_helpfile(argument, hcat)) != NULL)
        break;
    }

        // Look outside
    if (help == NULL)
        help = find_helpfile(argument, topHelpCat);
    }

    if (help == NULL)
    {
    act("Couldn't find a helpfile with keyword $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    return false;
    }

    if (!has_access_help(ch, help) || !has_access_helpcat(ch, help->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    ch->desc->pEdit = (HELP_DATA *) help;
    return false;
}


HEDIT(hedit_move)
{
    char arg[MSL];
    char arg2[MSL];
    HELP_CATEGORY *hCat = NULL;
    HELP_CATEGORY *hCatSrc;
    HELP_CATEGORY *hCatDest = NULL;
    HELP_DATA *help = NULL;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
    send_to_char("Syntax: move [helpfile|category] [category|up]\n\r", ch);
    return false;
    }

    hCatSrc = ch->desc->hCat;

    // 1st arg can be a category or a helpfile
    hCat = find_help_category(arg, hCatSrc->inside_cats);
    for (help = hCatSrc->inside_helps; help != NULL; help = help->next) {
    if (!str_prefix(arg, help->keyword))
        break;
    }

    // 2nd arg must be a destination category
    if (!str_cmp(arg2, "up"))
    hCatDest = hCatSrc->up;
    else
    hCatDest = find_help_category(arg2, hCatSrc->inside_cats);

    if (hCat == NULL && help == NULL) {
    send_to_char("Couldn't find a help file or category to move.\n\r", ch);
    return false;
    }

    if (hCatDest == NULL) {
    send_to_char("Couldn't find the destination category.\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, hCatDest) || !has_access_helpcat(ch, hCatSrc)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    // Do help first since this situation will be more likely, possible both
    // may match
    if (help != NULL) {
    HELP_DATA *helpTmp;

    if (!has_access_help(ch, help) || !has_access_helpcat(ch, help->hCat)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
    }

    if (help == hCatSrc->inside_helps)
        hCatSrc->inside_helps = help->next;
    else {
        for (helpTmp = hCatSrc->inside_helps; helpTmp != NULL; helpTmp = helpTmp->next) {
        if (helpTmp->next == help) {
            helpTmp->next = help->next;
            break;
        }
        }
    }

    help->next = NULL;
    help->hCat = hCatDest;

    insert_help(help, &hCatDest->inside_helps);

    act("Moved help $t into category $T.", ch, NULL, NULL, NULL, NULL, help->keyword,
        hCatDest == topHelpCat ? "root category" : hCatDest->name, TO_CHAR, NULL, NULL);
    }
    else /* moving a category */
    {
    HELP_CATEGORY *hCat_prev = NULL;

    if (!has_access_helpcat(ch, hCat) || !has_access_helpcat(ch, hCatDest)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
    }

    if (hCat == hCatDest) {
        send_to_char("You can't move a category into itself.\n\r", ch);
        return false;
    }

    if (hCatSrc->inside_cats == hCat)
        hCatSrc->inside_cats = hCatSrc->inside_cats->next;
    else {
        for (hCat_prev = hCatSrc->inside_cats; hCat_prev->next != NULL; hCat_prev = hCat_prev->next) {
        if (hCat_prev->next == hCat) {
            hCat_prev->next = hCat->next;
            break;
        }
        }
    }

    hCat->next = NULL;

    if (hCatDest->inside_cats == NULL)
        hCatDest->inside_cats = hCat;
    else
    {
        for (hCat_prev = hCatDest->inside_cats; hCat_prev != NULL; hCat_prev = hCat_prev->next) {
        if (hCat_prev->next == NULL)
            break;
        }

        hCat_prev->next = hCat;
    }

    hCat->up = hCatDest->up;
    act("Moved category $t into category $T.", ch, NULL, NULL, NULL, NULL, hCat->name,
        hCatDest == topHelpCat ? "root category" : hCatDest->name, TO_CHAR, NULL, NULL);
    }

    return true;
}


HEDIT(hedit_addcat)
{
    HELP_CATEGORY *hCat, *hCatTmp;
    char buf[MSL];

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: hedit addcat <name>\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    hCat = new_help_category();
    hCat->up = ch->desc->hCat;
    hCat->name = str_dup(argument);
    hCat->creator = ch->name;
    hCat->created = current_time;
    hCat->modified_by = ch->name;
    hCat->modified = current_time;
    hCat->description = str_dup("None\n\r");

    hCat->security = hCat->up->security;
    hCat->min_level = hCat->up->min_level;

    if (hCat->up->inside_cats == NULL)
        hCat->up->inside_cats = hCat;
    else
    {
        for (hCatTmp = hCat->up->inside_cats; hCatTmp->next != NULL; hCatTmp = hCatTmp->next)
        ;

    hCatTmp->next = hCat;
    }

    sprintf(buf, "Added category %s inside category %s.\n\r", hCat->name,
        hCat->up == topHelpCat ? "root category" : hCat->up->name);
    send_to_char(buf, ch);

    return true;
}


HEDIT(hedit_opencat)
{
    HELP_CATEGORY *hCat;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: opencategory <name>\n\r", ch);
    return false;
    }

    if ((hCat = find_help_category(argument, ch->desc->hCat->inside_cats)) == NULL)
    {
        act("No category by the name of $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    return false;
    }

    ch->desc->hCat = hCat;
    act("Opened category $t.", ch, NULL, NULL, NULL, NULL, hCat->name, NULL, TO_CHAR, NULL, NULL);
    return false;
}


HEDIT(hedit_upcat)
{
    if (ch->desc->hCat->up == NULL)
    {
        send_to_char("You're already at the root category.\n\r", ch);
    return false;
    }

    if (ch->desc->pEdit != NULL)
    {
    send_to_char("You must be finished with your current helpfile before switching categories.\n\r", ch);
    return false;
    }

    ch->desc->hCat = ch->desc->hCat->up;

    act("Switched categories to $t.", ch, NULL, NULL, NULL, NULL,
        ch->desc->hCat == topHelpCat ? "root category" : ch->desc->hCat->name, NULL, TO_CHAR, NULL, NULL);
    return false;
}


HEDIT(hedit_remcat)
{
    HELP_CATEGORY *hCat, *hCatPrev = NULL;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: hedit remcat <name>\n\r", ch);
    return false;
    }

    for (hCat = ch->desc->hCat->inside_cats; hCat != NULL; hCat = hCat->next)
    {
        if (!str_cmp(hCat->name, argument))
        break;

    hCatPrev = hCat;
    }

    if (hCat == NULL)
    {
        send_to_char("Couldn't find category to remove.\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat) || !has_access_helpcat(ch, hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    if (hCatPrev != NULL)
        hCatPrev->next = hCat->next;
    else
    ch->desc->hCat->inside_cats = hCat->next;

    act("Removed category $t.", ch, NULL, NULL, NULL, NULL, hCat->name, NULL, TO_CHAR, NULL, NULL);
    free_help_category(hCat);
    return true;
}


HEDIT(hedit_shiftcat)
{
    HELP_CATEGORY *hcat;
    HELP_CATEGORY *hcattmp;
    HELP_CATEGORY *hcatprev;
    char arg[MSL];
    char arg2[MSL];

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0'
    ||	(str_prefix(arg2, "left") && str_prefix(arg2, "right")))
    {
    send_to_char("Syntax: shift [category] [left|right]\n\r", ch);
    return false;
    }

    for (hcat = ch->desc->hCat->inside_cats; hcat != NULL; hcat = hcat->next)
    {
    if (!str_prefix(arg, hcat->name))
        break;
    }

    if (hcat == NULL) {
    send_to_char("No category found with that name.\n\r", ch);
    return false;
    }

    if (!str_prefix(arg2, "left"))
    {
    if (hcat == ch->desc->hCat->inside_cats) {
        send_to_char("That category is already at the start of the list.\n\r", ch);
        return false;
    }

    hcatprev = NULL;
    for (hcattmp = ch->desc->hCat->inside_cats; hcattmp != NULL; hcattmp = hcattmp->next)
    {
        if (hcattmp->next == hcat)
        break;

            hcatprev = hcattmp;
    }

        // Swap them
        hcattmp->next = hcat->next;
    hcat->next = hcattmp;

        if (hcatprev != NULL)
        hcatprev->next = hcat;
    else
        ch->desc->hCat->inside_cats = hcat;

    act("Shifted category $t left in the list.", ch, NULL, NULL, NULL, NULL, hcat->name, NULL, TO_CHAR, NULL, NULL);
    }
    else
    {
    if (hcat->next == NULL) {
        send_to_char("That category is already at the end of the list.\n\r", ch);
        return false;
    }

        hcatprev = NULL;
    for (hcattmp = ch->desc->hCat->inside_cats; hcattmp != NULL; hcattmp = hcattmp->next)
    {
        if (hcattmp == hcat)
        break;

        hcatprev = hcattmp;
    }

    hcattmp = hcat->next;

        hcat->next = hcattmp->next;
    hcattmp->next = hcat;
    if (hcatprev != NULL)
        hcatprev->next = hcattmp;
    else
        ch->desc->hCat->inside_cats = hcattmp;

    act("Shifted category $t right in the list.", ch, NULL, NULL, NULL, NULL, hcat->name, NULL, TO_CHAR, NULL, NULL);
    }

    hedit_show(ch, "");

    return true;
}


/**
 * hedit_text - Edit the help entry text (opens string editor)
 */
HEDIT(hedit_text)
{
    HELP_DATA *pHelp;

    if (ch->desc->pEdit == NULL) {
    send_to_char("You aren't editing a help file.\n\r", ch);
    return false;
    }

    EDIT_HELP(ch, pHelp);
    return olc_cmd_string_append(ch, argument, "Text", NULL,
        &pHelp->text, NULL, NULL);
}


HEDIT(hedit_name)
{
    if (ch->desc->pEdit != NULL) {
    send_to_char("You must finish editing your help file before you rename the category.\n\r", ch);
    return false;
    }

    if (ch->tot_level < MAX_LEVEL && ch->desc->hCat == topHelpCat) {
    send_to_char("You can't rename the root category.\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    if (argument[0] == '\0') {
    send_to_char("Syntax: name [name]\n\r", ch);
    return false;
    }

    free_string(ch->desc->hCat->name);
    ch->desc->hCat->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
}


HEDIT(hedit_description)
{
    if (ch->desc->pEdit != NULL) {
    send_to_char("You must finish editing your help file before you edit the category's description.\n\r", ch);
    return false;
    }

    if (ch->tot_level < MAX_LEVEL && ch->desc->hCat == topHelpCat) {
    send_to_char("You can't change the description of the root category.\n\r", ch);
    return false;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    string_append(ch, &ch->desc->hCat->description);
    return true;
}


HEDIT(hedit_keywords)
{
    HELP_DATA *pHelp;
    int i;

    if (ch->desc->pEdit == NULL) {
    send_to_char("You aren't editing a help file.\n\r", ch);
    return false;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0')
    {
        send_to_char(" Syntax: keywords [keywords]\n\r",ch);
        return false;
    }

    i = 0;
    while (argument[i] != '\0')
    {
    argument[i] = UPPER(argument[i]);
    i++;
    }

    free_string(pHelp->keyword);
    pHelp->keyword = str_dup(argument);
    send_to_char("Keyword(s) Set.\n\r", ch);
    return true;
}


HEDIT(hedit_level)
{
    HELP_DATA *pHelp;

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  level [number]\n\r", ch);
    return false;
    }

    if (ch->desc->pEdit == NULL) {
        if (ch->desc->hCat == topHelpCat) {
        send_to_char("You can't change the level of the top-level category.\n\r", ch);
        return false;
    }
    else {
        if (ch->pcdata->security <= ch->desc->hCat->security) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
        }

        ch->desc->hCat->min_level = atoi(argument);
        act("Current category's level set.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return true;
    }
    }

    EDIT_HELP(ch, pHelp);

    pHelp->min_level = atoi(argument);

    send_to_char("Level set.\n\r", ch);
    return true;
}


HEDIT(hedit_security)
{
    HELP_DATA *help;
    int arg;

    if (ch->tot_level < MAX_LEVEL) {
    send_to_char("You don't have the clearance to do this.\n\r", ch);
    return false;
    }

    if (argument[0] == '\0' || !is_number(argument)
    ||   (arg = atoi(argument)) > 9 || arg < 1)
    {
    send_to_char("Syntax: security [1-9]\n\r", ch);
    return false;
    }

    if (ch->desc->pEdit == NULL) {
    if (!has_access_helpcat(ch, ch->desc->hCat)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
    }

        if (ch->desc->hCat == topHelpCat && ch->tot_level < MAX_LEVEL) {
        send_to_char("You can't change the security of the root category.\n\r", ch);
        return false;
    }
    else {
        ch->desc->hCat->security = arg;
        act("Current category's security set.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return true;
    }
    }

    EDIT_HELP(ch, help);

    help->security = arg;

    send_to_char("Security set.\n\r", ch);
    return true;
}


HEDIT(hedit_builder)
{
    HELP_CATEGORY *hcat = NULL;
    HELP_DATA *help = NULL;
    char name[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if (ch->tot_level < MAX_LEVEL - 1) {
    send_to_char("You don't have the clearance to do this.\n\r", ch);
    return false;
    }

    if (ch->desc->pEdit != NULL)
    help = ch->desc->pEdit;
    else
    hcat = ch->desc->hCat;

    one_argument(argument, name);

    if (name[0] == '\0')
    {
    send_to_char("Syntax:  builder [$name]  -toggles builder\n\r", ch);
    send_to_char("Syntax:  builder All      -allows everyone\n\r", ch);
    return false;
    }

    name[0] = UPPER(name[0]);

    if (hcat != NULL)
    {
    if (!has_access_helpcat(ch, ch->desc->hCat)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
    }

    if (strstr(hcat->builders, name) != NULL)
    {
        hcat->builders = string_replace(hcat->builders, name, "\0");
        hcat->builders = string_unpad(hcat->builders);

        if (hcat->builders[0] == '\0')
        {
            free_string(hcat->builders);
            hcat->builders = str_dup("None");
        }
        send_to_char("Builder removed.\n\r", ch);
        return true;
    }
    else
    {
        if (!has_access_help(ch, help)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
        }


        buf[0] = '\0';

        if (!player_exists(name) && str_cmp(name, "All"))
        {
            act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR, NULL, NULL);
            return false;
        }

        if (strstr(hcat->builders, "None") != NULL)
        {
            hcat->builders = string_replace(hcat->builders, "None", "\0");
            hcat->builders = string_unpad(hcat->builders);
        }

        if (hcat->builders[0] != '\0')
        {
            strcat(buf, hcat->builders);
            strcat(buf, " ");
        }
        strcat(buf, name);
        free_string(hcat->builders);
        hcat->builders = string_proper(str_dup(buf));

        send_to_char("Builder added.\n\r", ch);
        send_to_char(hcat->builders,ch);
        send_to_char("\n\r", ch);
        return true;
    }
    }
    else if(help != NULL)
    {

        if (!has_access_help(ch, help)) {
        send_to_char("Insufficient security - access denied.\n\r", ch);
        return false;
        }

    if (strstr(help->builders, name) != NULL)
    {
        help->builders = string_replace(help->builders, name, "\0");
        help->builders = string_unpad(help->builders);

        if (help->builders[0] == '\0')
        {
        free_string(help->builders);
        help->builders = str_dup("None");
        }
        send_to_char("Builder removed.\n\r", ch);
        return true;
    }
    else
    {
        buf[0] = '\0';

        if (!player_exists(name) && str_cmp(name, "All"))
        {
        act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR, NULL, NULL);
        return false;
        }

        if (strstr(help->builders, "None") != NULL)
        {
        help->builders = string_replace(help->builders, "None", "\0");
        help->builders = string_unpad(help->builders);
        }

        if (help->builders[0] != '\0')
        {
        strcat(buf, help->builders);
        strcat(buf, " ");
        }
        strcat(buf, name);
        free_string(help->builders);
        help->builders = string_proper(str_dup(buf));

        send_to_char("Builder added.\n\r", ch);
        send_to_char(help->builders,ch);
        send_to_char("\n\r", ch);
        return true;
    }
    }

    return false;
}


HEDIT(hedit_addtopic)
{
    HELP_DATA *pHelp;
    STRING_DATA *topic;
    STRING_DATA *topic_tmp;
    STRING_DATA *topic_prev;
    int i;

    if (ch->desc->pEdit == NULL) {
    send_to_char("You aren't editing a help file.\n\r", ch);
    return false;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: addtopic [keywords]\n\r",ch);
        return false;
    }

    if (lookup_help_exact(argument, ch->tot_level, topHelpCat) == NULL)
    {
    act("There is no helpfile with keywords $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    return false;
    }

    i = 0;
    while (argument[i] != '\0')
    {
    argument[i] = UPPER(argument[i]);
    i++;
    }

    topic = new_string_data();
    topic->string = str_dup(argument);

    if (pHelp->related_topics == NULL)
    pHelp->related_topics = topic;
    else
    {
    topic_prev = NULL;
    for (topic_tmp = pHelp->related_topics; topic_tmp != NULL; topic_tmp = topic_tmp->next)
    {
        if (strcmp(topic->string, topic_tmp->string) <= 0)
        break;

        topic_prev = topic_tmp;
    }

    topic->next = topic_tmp;
    if (topic_prev != NULL)
        topic_prev->next = topic;
    else
        pHelp->related_topics = topic;
    }

    act("Related topic $t added.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    return true;
}


HEDIT(hedit_remtopic)
{
    HELP_DATA *pHelp;
    STRING_DATA *topic;
    STRING_DATA *topic_prev;
    int i;
    int val;

    if (ch->desc->pEdit == NULL) {
    send_to_char("You aren't editing a help file.\n\r", ch);
    return false;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0' || (val = atoi(argument)) < 0)
    {
        send_to_char("Syntax: remtopic [#]\n\r",ch);
        return false;
    }

    if (pHelp->related_topics == NULL) {
    send_to_char("There are no related topics on this helpfile.\n\r", ch);
    return false;
    }

    i = 0;
    topic_prev = NULL;
    for (topic = pHelp->related_topics; topic != NULL; topic = topic->next)
    {
    if (i == val)
        break;

    i++;
    topic_prev = topic;
    }

    if (topic == NULL)
    {
    send_to_char("Couldn't find that related topic.\n\r", ch);
    return false;
    }

    if (topic_prev != NULL)
    topic_prev->next = topic->next;
    else
    pHelp->related_topics = topic->next;

    free_string_data(topic);
    send_to_char("Related topic removed.\n\r", ch);
    return true;
}


HEDIT(hedit_delete)
{
    HELP_CATEGORY *hCat;
    HELP_DATA *pHelp;
    HELP_DATA *prev_pHelp = NULL;

    if (ch->tot_level < MAX_LEVEL - 4) {
    send_to_char("You don't have the clearance to delete help files.\n\r", ch);
    return false;
    }

    if (argument[0] == '\0') {
    send_to_char("Syntax: delete <keyword>\n\r", ch);
    return false;
    }

    hCat = ch->desc->hCat;
    for (pHelp = hCat->inside_helps; pHelp != NULL; pHelp = pHelp->next) {
    if (!str_prefix(argument, pHelp->keyword))
        break;

    prev_pHelp = pHelp;
    }

    if (pHelp == NULL) {
    act("Didn't find a file with keyword $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    return false;
    }

    if (!has_access_help(ch, pHelp) || !has_access_helpcat(ch, pHelp->hCat)) {
    send_to_char("Insufficient security - access denied.\n\r", ch);
    return false;
    }

    if (prev_pHelp != NULL)
    prev_pHelp->next = pHelp->next;
    else
    hCat->inside_helps = pHelp->next;

    act("Help file $t deleted.", ch, NULL, NULL, NULL, NULL, pHelp->keyword, NULL, TO_CHAR, NULL, NULL);
    free_help(pHelp);
    return true;
}