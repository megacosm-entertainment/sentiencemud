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
#include "olc.h"
#include "recycle.h"
#include "scripts.h"

//bool edit_deltrigger(LLIST **list, int index);


REDIT(redit_addcdesc)
{
    int value;
    ROOM_INDEX_DATA *pRoom;
    char type[MSL];
    char phrase[MSL];
    CONDITIONAL_DESCR_DATA *cd;

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, type);
    argument = one_argument(argument, phrase);

    if (type[0] == '\0' || phrase[0] == '\0')
    {
	send_to_char("Syntax: addcdesc [type] [phrase]\n\r", ch);
	return FALSE;
    }

    if ((value = flag_value(room_condition_flags, type)) == NO_FLAG)
    {
	send_to_char("Valid condition types are:\n\r", ch);
	show_help(ch, "condition");
	return FALSE;
    }

    if (cd_phrase_lookup(value, phrase) == -1)
    {
	send_to_char("Invalid phrase.\n\r", ch);
	return FALSE;
    }

    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
	if (cd->condition == value && cd->phrase == cd_phrase_lookup(value, phrase))
	{
	    send_to_char("That would be redundant.\n\r", ch);
	    return FALSE;
	}
    }

    cd = new_conditional_descr();
    cd->condition = value;
    cd->phrase = cd_phrase_lookup(value, phrase);
    cd->next = pRoom->conditional_descr;
    pRoom->conditional_descr = cd;

    string_append(ch, &cd->description);

    return TRUE;
}


REDIT(redit_dislink)
{
    ROOM_INDEX_DATA *pRoom;
    bool changed = FALSE;

    EDIT_ROOM(ch, pRoom);

    if (!str_cmp(argument, "junk")) {
	free_string(pRoom->name);
	pRoom->name = str_dup("NULL");
	changed = TRUE;
    }

    if (dislink_room(pRoom))
    {
	send_to_char("Room dislinked.\n\r", ch);
	changed = TRUE;
    }
    else
	send_to_char("No exits to dislink.\n\r", ch);

    return changed;
}


char *condition_type_to_name (int type)
{
    switch (type)
    {
	case CONDITION_SEASON: return "SEASON";
	case CONDITION_SKY: return "SKY";
	case CONDITION_HOUR: return "HOUR";
	case CONDITION_SCRIPT: return "SCRIPT";
	default: return "UNKNOWN";
    }
}


char *condition_phrase_to_name (int type, int phrase)
{
    switch (type)
    {
	case CONDITION_SEASON:
	    switch (phrase)
	    {
		case SEASON_SPRING: return "SPRING";
		case SEASON_SUMMER: return "SUMMER";
		case SEASON_FALL: return "FALL";
		case SEASON_WINTER: return "WINTER";
		default: return "UNKNOWN";
	    }

	case CONDITION_SKY:
	    switch (phrase)
	    {
		case SKY_CLOUDLESS: return "CLOUDLESS";
		case SKY_CLOUDY: return "CLOUDY";
		case SKY_RAINING: return "RAINY";
		case SKY_LIGHTNING: return "STORMY";
		default: return "UNKNOWN";
	    }

	default: return "UNKNOWN";
    }
}


int cd_phrase_lookup(int condition, char *phrase)
{
    if (condition == CONDITION_SEASON)
    {
	if (!str_cmp(phrase, "winter"))
	    return SEASON_WINTER;
	else
	if (!str_cmp(phrase, "spring"))
	    return SEASON_SPRING;
	else
	if (!str_cmp(phrase, "summer"))
	    return SEASON_SUMMER;
	else
	if (!str_cmp(phrase, "fall"))
	    return SEASON_FALL;
	else
	    return -1;
    }

    if (condition == CONDITION_SKY)
    {
	if (!str_cmp(phrase, "cloudless"))
	    return SKY_CLOUDLESS;
	else
	if (!str_cmp(phrase, "cloudy"))
	    return SKY_CLOUDY;
	else
	if (!str_cmp(phrase, "rainy"))
	    return SKY_RAINING;
	else
	if (!str_cmp(phrase, "stormy"))
	    return SKY_LIGHTNING;
	else
	    return -1;
    }

    if (condition == CONDITION_HOUR)
    {
	int hour;

	hour = atoi(phrase);

	if (hour < 0 || hour > 23)
	    return -1;
	else
	    return hour;
    }

    if (condition == CONDITION_SCRIPT)
    {
	int vnum;

	vnum = atoi(phrase);

	if (!get_script_index(vnum,PRG_RPROG))
	    return -1;
	else
	    return vnum;
    }

    return -1;
}


REDIT(redit_delcdesc)
{
    CONDITIONAL_DESCR_DATA *cd;
    CONDITIONAL_DESCR_DATA *cd_prev;
    int i = 0;
    char cDesc[MSL];
    int value;
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    one_argument(argument, cDesc);
    if (!is_number(cDesc) || cDesc[0] == '\0')
    {
	send_to_char("Syntax: delcdesc [#cdesc]\n\r", ch);
	return FALSE;
    }

    value = atoi(cDesc);
    if (value < 0)
    {
	send_to_char("Invalid value.\n\r", ch);
	return FALSE;
    }

    cd_prev = NULL;
    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
	if (i == value)
	    break;

	cd_prev = cd;
	i++;
    }

    if (cd == NULL)
    {
	send_to_char("Conditional description not found in list.\n\r", ch);
	return FALSE;
    }

    if (cd_prev == NULL) // head of list
    {
	pRoom->conditional_descr = cd->next;
    }
    else
    {
	cd_prev->next = cd->next;
    }

    free_conditional_descr(cd);

    send_to_char("Conditional description removed.\n\r", ch);
    return TRUE;
}


REDIT(redit_editcdesc)
{
    CONDITIONAL_DESCR_DATA *cd;
    ROOM_INDEX_DATA *pRoom;
    int i;
    char arg[MSL];
    int num;

    EDIT_ROOM(ch, pRoom);

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax: editcdesc [#cdesc]\n\r", ch);
	return FALSE;
    }

    num = atoi(arg);
    if (num < 0)
    {
	send_to_char("Invalid argument.\n\r", ch);
	return FALSE;
    }

    i = 0;
    for (cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
    {
	if (i == num)
	    break;

	i++;
    }

    if (cd == NULL)
    {
	send_to_char("Conditional description not found in list.\n\r", ch);
	return FALSE;
    }

    string_append(ch, &cd->description);

    return TRUE;
}


OEDIT(oedit_desc)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	string_append(ch, &pObj->full_description);
	return TRUE;
    }

    send_to_char("Syntax:  desc\n\r", ch);
    return FALSE;
}

OEDIT(oedit_comments)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	string_append(ch, &pObj->comments);
	return TRUE;
    }

    send_to_char("Syntax:  comments\n\r", ch);
    return FALSE;
}

OEDIT(oedit_update)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL - 2)
    {
	send_to_char("Insufficient security to toggle update.\n\r", ch);
	return FALSE;
    }

    if (pObj->update == TRUE)
    {
	pObj->update = FALSE;
	send_to_char("Update OFF.\n\r", ch);
    }
    else
    {
	pObj->update = TRUE;
	send_to_char("Update ON.\n\r", ch);
    }

    return TRUE;
}

OEDIT(oedit_timer)
{
    OBJ_INDEX_DATA *pObj;
    char arg[MSL];
    int time;

    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax: timer <#ticks>\n\r", ch);
	return FALSE;
    }

    if (!is_number(arg))
    {
	send_to_char("Argument must be numerical.\n\r", ch);
	return FALSE;
    }

    if ((time = atoi(arg)) < 0 || time > 10000)
    {
	send_to_char("Range is 0 (doesn't decay) to 1000.\n\r", ch);
	return FALSE;
    }

    pObj->timer = time;
    send_to_char("Timer set.\n\r", ch);
    return TRUE;
}


/* Help Editor */
HEDIT (hedit_show)
{
    HELP_CATEGORY *hcat;
    HELP_DATA *help;
    STRING_DATA *topic;
    char buf[2*MSL], buf2[MSL];
    BUFFER *buffer;
    int i;

    buffer = new_buf();

    if (ch->desc->pEdit != NULL)
    {
	help = (HELP_DATA *) ch->desc->pEdit;

	sprintf(buf, "{YKeywords:              {W%s{x\n\r", help->keyword);
	add_buf(buffer, buf);

        sprintf(buf, "{YSecurity:              {x%d\n\r", help->security);
	add_buf(buffer, buf);

	sprintf(buf, "{YBuilders:              {x%s\n\r", help->builders);
	add_buf(buffer, buf);

	sprintf(buf, "{YCreated by:            {x%-20s\n\r", help->creator);
	add_buf(buffer, buf);

	sprintf(buf, "{YCreated on:            {x%s", help->created == 0 ? "Unknown\n\r" : (char *) ctime(&help->created));
	add_buf(buffer, buf);

	sprintf(buf, "{YLast modified by:      {x%-20s\n\r", help->modified_by);
	add_buf(buffer, buf);

	sprintf(buf, "{YLast modified on:      {x%s", help->modified == 0 ? "Unknown\n\r" : (char *) ctime(&help->modified));
	add_buf(buffer, buf);

	sprintf(buf, "{YCategory:              {x%-20s\n\r", help->hCat == topHelpCat ? "Root Category" : help->hCat->name);
	add_buf(buffer, buf);

	sprintf(buf, "{YMinimum level:         {x%-20d\n\r", help->min_level);
	add_buf(buffer, buf);

	add_buf(buffer, "{Y-------------------------------------------------------------------------------------------------------------{x\n\r");

	add_buf(buffer, help->text);
	add_buf(buffer, "{Y-------------------------------------------------------------------------------------------------------------{x\n\r");
	add_buf(buffer, "{YRelated topics:{x\n\r");
	if (help->related_topics == NULL)
	    add_buf(buffer, "None\n\r");
	else
	{
	    i = 0;
	    for (topic = help->related_topics; topic != NULL; topic = topic->next)
	    {
		sprintf(buf, "{b[{B%-2d{b]{x %s\n\r", i, topic->string);
		add_buf(buffer, buf);
		i++;
	    }
	}
    }
    else
    {
	sprintf(buf, "{YCategory name:         {W%s{x\n\r", ch->desc->hCat == topHelpCat ? "Root Category" : ch->desc->hCat->name);
	add_buf(buffer, buf);

	sprintf(buf, "{YSecurity:              {x%d{x\n\r", ch->desc->hCat->security);
	add_buf(buffer, buf);

	sprintf(buf, "{YBuilders:              {x%s\n\r", ch->desc->hCat->builders);
	add_buf(buffer, buf);

	sprintf(buf, "{YCreated by:            {x%s\n\r", ch->desc->hCat->creator);
	add_buf(buffer, buf);

	sprintf(buf, "{YCreated on:            {x%s", ch->desc->hCat->created == 0 ? "Unknown\n\r" : (char *) ctime(&ch->desc->hCat->created));
	add_buf(buffer, buf);

	sprintf(buf, "{YLast modified by:      {x%-20s\n\r", ch->desc->hCat->modified_by);
	add_buf(buffer, buf);

	sprintf(buf, "{YLast modified on:      {x%s", ch->desc->hCat->modified == 0 ? "Unknown\n\r" : (char *) ctime(&ch->desc->hCat->modified));
	add_buf(buffer, buf);

	sprintf(buf, "{YMinimum level:         {x%-20d\n\r", ch->desc->hCat->min_level);
	add_buf(buffer, buf);

	sprintf(buf, "{YDescription:           {x\n\r%s", ch->desc->hCat->description);
	add_buf(buffer, buf);

	add_buf(buffer, "{Y-------------------------------------------------------------------------------------------------------------{x\n\r");

	i = 0;
	for (hcat = ch->desc->hCat->inside_cats; hcat != NULL; hcat = hcat->next)
	{
	    sprintf(buf2, "{W%.18s{B/{x", hcat->name);
	    sprintf(buf, "{b[{BC{b]{x  %-30s", buf2);
	    add_buf(buffer, buf);

	    i++;
	    if (i % 4 == 0)
		add_buf(buffer, "\n\r");
	    else
		add_buf(buffer, " ");
	}

	for (help = ch->desc->hCat->inside_helps; help != NULL; help = help->next)
	{
	    //one_argument(help->keyword, buf2);
	    sprintf(buf2, "%.18s", help->keyword);
	    sprintf(buf, "{b%-4d{x %-24s", /*i + 1*/ help->index, buf2);
	    add_buf(buffer, buf);

	    i++;
	    if (i % 4 == 0)
		add_buf(buffer, "\n\r");
	    else
		add_buf(buffer, " ");
	}

	if (i % 4 != 0)
	    add_buf(buffer, "\n\r");

	add_buf(buffer, "{Y-------------------------------------------------------------------------------------------------------------{x\n\r");

    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);

    return FALSE;
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
	return FALSE;
    }

    if (ch->desc->pEdit != NULL)
    {
    	send_to_char("You are already editing a helpfile.\n\r", ch);
	return FALSE;
    }

    if (ch->desc->hCat == topHelpCat) {
	send_to_char("You can only add categories in the root category.\n\r", ch);
	return FALSE;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
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
    return TRUE;
}


HEDIT(hedit_edit)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcat;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax: hedit edit <keyword(s)>\n\r",ch);
	return FALSE;
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
	act("Couldn't find a helpfile with keyword $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
	return FALSE;
    }

    if (!has_access_help(ch, help) || !has_access_helpcat(ch, help->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    ch->desc->pEdit = (HELP_DATA *) help;
    return FALSE;
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
	return FALSE;
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
	return FALSE;
    }

    if (hCatDest == NULL) {
	send_to_char("Couldn't find the destination category.\n\r", ch);
	return FALSE;
    }

    if (!has_access_helpcat(ch, hCatDest) || !has_access_helpcat(ch, hCatSrc)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    // Do help first since this situation will be more likely, possible both
    // may match
    if (help != NULL) {
	HELP_DATA *helpTmp;

	if (!has_access_help(ch, help) || !has_access_helpcat(ch, help->hCat)) {
	    send_to_char("Insufficient security - access denied.\n\r", ch);
	    return FALSE;
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
	    hCatDest == topHelpCat ? "root category" : hCatDest->name, TO_CHAR);
    }
    else /* moving a category */
    {
	HELP_CATEGORY *hCat_prev = NULL;

	if (!has_access_helpcat(ch, hCat) || !has_access_helpcat(ch, hCatDest)) {
	    send_to_char("Insufficient security - access denied.\n\r", ch);
	    return FALSE;
	}

	if (hCat == hCatDest) {
	    send_to_char("You can't move a category into itself.\n\r", ch);
	    return FALSE;
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
	    hCatDest == topHelpCat ? "root category" : hCatDest->name, TO_CHAR);
    }

    return TRUE;
}


HEDIT(hedit_addcat)
{
    HELP_CATEGORY *hCat, *hCatTmp;
    char buf[MSL];

    if (argument[0] == '\0')
    {
    	send_to_char("Syntax: hedit addcat <name>\n\r", ch);
	return FALSE;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
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

    return TRUE;
}


HEDIT(hedit_opencat)
{
    HELP_CATEGORY *hCat;

    if (argument[0] == '\0')
    {
    	send_to_char("Syntax: opencategory <name>\n\r", ch);
	return FALSE;
    }

    if ((hCat = find_help_category(argument, ch->desc->hCat->inside_cats)) == NULL)
    {
        act("No category by the name of $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
	return FALSE;
    }

    ch->desc->hCat = hCat;
    act("Opened category $t.", ch, NULL, NULL, NULL, NULL, hCat->name, NULL, TO_CHAR);
    return FALSE;
}


HEDIT(hedit_upcat)
{
    if (ch->desc->hCat->up == NULL)
    {
        send_to_char("You're already at the root category.\n\r", ch);
	return FALSE;
    }

    if (ch->desc->pEdit != NULL)
    {
	send_to_char("You must be finished with your current helpfile before switching categories.\n\r", ch);
	return FALSE;
    }

    ch->desc->hCat = ch->desc->hCat->up;

    act("Switched categories to $t.", ch, NULL, NULL, NULL, NULL,
        ch->desc->hCat == topHelpCat ? "root category" : ch->desc->hCat->name, NULL, TO_CHAR);
    return FALSE;
}


HEDIT(hedit_remcat)
{
    HELP_CATEGORY *hCat, *hCatPrev = NULL;

    if (argument[0] == '\0')
    {
    	send_to_char("Syntax: hedit remcat <name>\n\r", ch);
	return FALSE;
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
	return FALSE;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat) || !has_access_helpcat(ch, hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    if (hCatPrev != NULL)
        hCatPrev->next = hCat->next;
    else
	ch->desc->hCat->inside_cats = hCat->next;

    act("Removed category $t.", ch, NULL, NULL, NULL, NULL, hCat->name, NULL, TO_CHAR);
    free_help_category(hCat);
    return TRUE;
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
	return FALSE;
    }

    for (hcat = ch->desc->hCat->inside_cats; hcat != NULL; hcat = hcat->next)
    {
	if (!str_prefix(arg, hcat->name))
	    break;
    }

    if (hcat == NULL) {
	send_to_char("No category found with that name.\n\r", ch);
	return FALSE;
    }

    if (!str_prefix(arg2, "left"))
    {
	if (hcat == ch->desc->hCat->inside_cats) {
	    send_to_char("That category is already at the start of the list.\n\r", ch);
	    return FALSE;
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

	act("Shifted category $t left in the list.", ch, NULL, NULL, NULL, NULL, hcat->name, NULL, TO_CHAR);
    }
    else
    {
	if (hcat->next == NULL) {
	    send_to_char("That category is already at the end of the list.\n\r", ch);
	    return FALSE;
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

	act("Shifted category $t right in the list.", ch, NULL, NULL, NULL, NULL, hcat->name, NULL, TO_CHAR);
    }

    hedit_show(ch, "");

    return TRUE;
}


HEDIT(hedit_text)
{
    HELP_DATA *pHelp;

    if (ch->desc->pEdit == NULL) {
	send_to_char("You aren't editing a help file.\n\r", ch);
	return FALSE;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] =='\0')
    {
       string_append(ch, &pHelp->text);
       return TRUE;
    }

    send_to_char(" Syntax: text\n\r",ch);
    return FALSE;
}


HEDIT(hedit_name)
{
    if (ch->desc->pEdit != NULL) {
	send_to_char("You must finish editing your help file before you rename the category.\n\r", ch);
	return FALSE;
    }

    if (ch->tot_level < MAX_LEVEL && ch->desc->hCat == topHelpCat) {
	send_to_char("You can't rename the root category.\n\r", ch);
	return FALSE;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    if (argument[0] == '\0') {
	send_to_char("Syntax: name [name]\n\r", ch);
	return FALSE;
    }

    free_string(ch->desc->hCat->name);
    ch->desc->hCat->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return TRUE;
}


HEDIT(hedit_description)
{
    if (ch->desc->pEdit != NULL) {
	send_to_char("You must finish editing your help file before you edit the category's description.\n\r", ch);
	return FALSE;
    }

    if (ch->tot_level < MAX_LEVEL && ch->desc->hCat == topHelpCat) {
	send_to_char("You can't change the description of the root category.\n\r", ch);
	return FALSE;
    }

    if (!has_access_helpcat(ch, ch->desc->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    string_append(ch, &ch->desc->hCat->description);
    return TRUE;
}


HEDIT(hedit_keywords)
{
    HELP_DATA *pHelp;
    int i;

    if (ch->desc->pEdit == NULL) {
	send_to_char("You aren't editing a help file.\n\r", ch);
	return FALSE;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0')
    {
        send_to_char(" Syntax: keywords [keywords]\n\r",ch);
        return FALSE;
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
    return TRUE;
}


HEDIT(hedit_level)
{
    HELP_DATA *pHelp;

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  level [number]\n\r", ch);
	return FALSE;
    }

    if (ch->desc->pEdit == NULL) {
        if (ch->desc->hCat == topHelpCat) {
	    send_to_char("You can't change the level of the top-level category.\n\r", ch);
	    return FALSE;
	}
	else {
	    if (ch->pcdata->security <= ch->desc->hCat->security) {
		send_to_char("Insufficient security - access denied.\n\r", ch);
		return FALSE;
	    }

	    ch->desc->hCat->min_level = atoi(argument);
	    act("Current category's level set.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    return TRUE;
	}
    }

    EDIT_HELP(ch, pHelp);

    pHelp->min_level = atoi(argument);

    send_to_char("Level set.\n\r", ch);
    return TRUE;
}


HEDIT(hedit_security)
{
    HELP_DATA *help;
    int arg;

    if (ch->tot_level < MAX_LEVEL) {
	send_to_char("You don't have the clearance to do this.\n\r", ch);
	return FALSE;
    }

    if (argument[0] == '\0' || !is_number(argument)
    ||   (arg = atoi(argument)) > 9 || arg < 1)
    {
	send_to_char("Syntax: security [1-9]\n\r", ch);
	return FALSE;
    }

    if (ch->desc->pEdit == NULL) {
	if (!has_access_helpcat(ch, ch->desc->hCat)) {
	    send_to_char("Insufficient security - access denied.\n\r", ch);
	    return FALSE;
	}

        if (ch->desc->hCat == topHelpCat && ch->tot_level < MAX_LEVEL) {
	    send_to_char("You can't change the security of the root category.\n\r", ch);
	    return FALSE;
	}
	else {
	    ch->desc->hCat->security = arg;
	    act("Current category's security set.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    return TRUE;
	}
    }

    EDIT_HELP(ch, help);

    help->security = arg;

    send_to_char("Security set.\n\r", ch);
    return TRUE;
}


HEDIT(hedit_builder)
{
    HELP_CATEGORY *hcat = NULL;
    HELP_DATA *help = NULL;
    char name[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if (ch->tot_level < MAX_LEVEL - 1) {
	send_to_char("You don't have the clearance to do this.\n\r", ch);
	return FALSE;
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
	return FALSE;
    }

    name[0] = UPPER(name[0]);

    if (hcat != NULL)
    {
	if (!has_access_helpcat(ch, ch->desc->hCat)) {
	    send_to_char("Insufficient security - access denied.\n\r", ch);
	    return FALSE;
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
	    return TRUE;
	}
	else
	{
	    if (!has_access_help(ch, help)) {
		send_to_char("Insufficient security - access denied.\n\r", ch);
		return FALSE;
	    }


	    buf[0] = '\0';

	    if (!player_exists(name) && str_cmp(name, "All"))
	    {
			act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR);
			return FALSE;
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
	    return TRUE;
	}
    }
    else if(help != NULL)
    {

	    if (!has_access_help(ch, help)) {
		send_to_char("Insufficient security - access denied.\n\r", ch);
		return FALSE;
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
	    return TRUE;
	}
	else
	{
	    buf[0] = '\0';

	    if (!player_exists(name) && str_cmp(name, "All"))
	    {
		act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR);
		return FALSE;
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
	    return TRUE;
	}
    }

    return FALSE;
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
	return FALSE;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: addtopic [keywords]\n\r",ch);
        return FALSE;
    }

    if (lookup_help_exact(argument, ch->tot_level, topHelpCat) == NULL)
    {
	act("There is no helpfile with keywords $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
	return FALSE;
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

    act("Related topic $t added.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
    return TRUE;
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
	return FALSE;
    }

    EDIT_HELP(ch, pHelp);

    if (argument[0] == '\0' || (val = atoi(argument)) < 0)
    {
        send_to_char("Syntax: remtopic [#]\n\r",ch);
        return FALSE;
    }

    if (pHelp->related_topics == NULL) {
	send_to_char("There are no related topics on this helpfile.\n\r", ch);
	return FALSE;
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
	return FALSE;
    }

    if (topic_prev != NULL)
	topic_prev->next = topic->next;
    else
	pHelp->related_topics = topic->next;

    free_string_data(topic);
    send_to_char("Related topic removed.\n\r", ch);
    return TRUE;
}


HEDIT(hedit_delete)
{
    HELP_CATEGORY *hCat;
    HELP_DATA *pHelp;
    HELP_DATA *prev_pHelp = NULL;

    if (ch->tot_level < MAX_LEVEL - 4) {
	send_to_char("You don't have the clearance to delete help files.\n\r", ch);
	return FALSE;
    }

    if (argument[0] == '\0') {
	send_to_char("Syntax: delete <keyword>\n\r", ch);
	return FALSE;
    }

    hCat = ch->desc->hCat;
    for (pHelp = hCat->inside_helps; pHelp != NULL; pHelp = pHelp->next) {
	if (!str_prefix(argument, pHelp->keyword))
	    break;

	prev_pHelp = pHelp;
    }

    if (pHelp == NULL) {
	act("Didn't find a file with keyword $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
	return FALSE;
    }

    if (!has_access_help(ch, pHelp) || !has_access_helpcat(ch, pHelp->hCat)) {
	send_to_char("Insufficient security - access denied.\n\r", ch);
	return FALSE;
    }

    if (prev_pHelp != NULL)
	prev_pHelp->next = pHelp->next;
    else
	hCat->inside_helps = pHelp->next;

    act("Help file $t deleted.", ch, NULL, NULL, NULL, NULL, pHelp->keyword, NULL, TO_CHAR);
    free_help(pHelp);
    return TRUE;
}


TEDIT(tedit_create)
{
    TOKEN_INDEX_DATA *token_index;
    AREA_DATA *pArea;
    long value;
    int iHash;

    EDIT_TOKEN(ch, token_index);

    value = atol(argument);
    if (argument[0] == '\0' || value == '\0')
    {
	send_to_char("Syntax: tedit create [vnum]\n\r", ch);
	return FALSE;
    }

    pArea = get_vnum_area(value);
    if (pArea == NULL)
    {
	send_to_char("That vnum is not assigned an area.\n\r", ch);
	return FALSE;
    }

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("You aren't a builder in that area.\n\r", ch);
	return FALSE;
    }

    if (get_token_index(value))
    {
	send_to_char("Token vnum already exists.\n\r", ch);
	return FALSE;
    }

    token_index = new_token_index();
    token_index->vnum = value;
    token_index->area = pArea;

    iHash = value % MAX_KEY_HASH;
    token_index->next = token_index_hash[iHash];
    token_index_hash[iHash] = token_index;

    ch->desc->pEdit = (void *)token_index;

    send_to_char("Token created.\n\r", ch);
    SET_BIT(token_index->area->area_flags, AREA_CHANGED);
    return TRUE;
}


TEDIT(tedit_show)
{
    TOKEN_INDEX_DATA *token_index;
    ITERATOR it;
    PROG_LIST *trigger;
    char buf[MSL];
    int i;

    EDIT_TOKEN(ch, token_index);

    sprintf(buf, "Name:                   {Y[{x%-20s{Y]{x\n\r", token_index->name);
    send_to_char(buf, ch);
    sprintf(buf, "Area:                   {Y[{x%-20s{Y]{x\n\r", token_index->area->name);
    send_to_char(buf, ch);
    sprintf(buf, "Vnum:                   {Y[{x%-20ld{Y]{x\n\r", token_index->vnum);
    send_to_char(buf, ch);
    sprintf(buf, "Type:                   {Y[{x%-20s{Y]{x\n\r", token_table[token_index->type].name);
    send_to_char(buf, ch);
    sprintf(buf, "Flags:                  {Y[{x%-20s{Y]{x\n\r", flag_string(token_flags, token_index->flags));
    send_to_char(buf, ch);
    sprintf(buf, "Timer:                  {Y[{x%-20d{Y]{x ticks\n\r", token_index->timer);
    send_to_char(buf, ch);

    sprintf(buf, "Description:\n\r%s\n\r", token_index->description);
    send_to_char(buf, ch);

    sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", token_index->comments);
    send_to_char(buf,ch);



    buf[0] = '\0';/* not enabled yet
    if (token_index->ed)
    {
	EXTRA_DESCR_DATA *ed;

	strcat(buf,
		"Desc Kwds:    [{x");
	for (ed = token_index->ed; ed; ed = ed->next)
	{
	    strcat(buf, ed->keyword);
	    if (ed->next)
		strcat(buf, " ");
	}
	strcat(buf, "]\n\r");

	send_to_char(buf, ch);
    } */

    send_to_char("{YDefault values:{x\n\r", ch);
    for (i = 0; i < MAX_TOKEN_VALUES; i++) {
    	sprintf(buf,
		"Value {Y[{x%d{Y]:{x %-20s {Y[{x%ld{Y]{x\n\r",
		i, token_index_getvaluename(token_index, i), token_index->value[i]);

	send_to_char(buf, ch);
    }
    if (token_index->progs) {
	int cnt, slot;

	for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
		if(list_size(token_index->progs[slot]) > 0) ++cnt;

	if (cnt > 0) {
		sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "TokProg Vnum", "Trigger", "Phrase");
		send_to_char(buf, ch);

		sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
		send_to_char(buf, ch);

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
			iterator_start(&it, token_index->progs[slot]);
			while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
				sprintf(buf, "{C[{W%4d{C]{x %-20ld %-10s %-6s\n\r", cnt,
					trigger->vnum,trigger_name(trigger->trig_type),
					trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, FALSE, TRUE));
				send_to_char(buf, ch);
				cnt++;
			}
			iterator_stop(&it);
		}
	}
    }

    if (token_index->index_vars) {
	pVARIABLE var;
	int cnt;

	for (cnt = 0, var = token_index->index_vars; var; var = var->next) ++cnt;

	if (cnt > 0) {
		sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
		send_to_char(buf, ch);

		sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
		send_to_char(buf, ch);

		for (var = token_index->index_vars; var; var = var->next) {
			switch(var->type) {
			case VAR_INTEGER:
				sprintf(buf, "{x%-20.20s {GNUMBER     {Y%c   {W%d{x\n\r", var->name,var->save?'Y':'N',var->_.i);
				break;
			case VAR_STRING:
			case VAR_STRING_S:
				sprintf(buf, "{x%-20.20s {GSTRING     {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N',var->_.s?var->_.s:"(empty)");
				break;
			case VAR_ROOM:
				if(var->_.r && var->_.r->vnum > 0)
					sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%d{R){x\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->vnum);
				else
					sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r",var->name,var->save?'Y':'N');
				break;
			default:
				continue;
			}
			send_to_char(buf, ch);
		}
	}
    }

    return FALSE;
}


TEDIT(tedit_name)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  name [string]\n\r", ch);
	return FALSE;
    }

    free_string(token_index->name);
    token_index->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return TRUE;
}


TEDIT(tedit_type)
{
    TOKEN_INDEX_DATA *token_index;
    int i;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  type [general|quest|affect|skill|spell]\n\r", ch);
	return FALSE;
    }

    for (i = 0; token_table[i].name != NULL; i++) {
	if (!str_prefix(argument, token_table[i].name))
	    break;
    }

    if (token_table[i].name == NULL) {
	send_to_char("That token type doesn't exist.\n\r", ch);
	return FALSE;
    }

	if(i == TOKEN_SPELL && ch->tot_level < MAX_LEVEL) {
		send_to_char("Only IMPs can make spell tokens.\n\r", ch);
		return FALSE;
	}

	if(i == TOKEN_SONG && ch->tot_level < MAX_LEVEL) {
		send_to_char("Only IMPs can make song tokens.\n\r", ch);
		return FALSE;
	}

    token_index->type = token_table[i].type;
    act("Set token type to $t.", ch, NULL, NULL, NULL, NULL, token_table[i].name, NULL, TO_CHAR);
    return TRUE;
}


TEDIT(tedit_flags)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    int value;

    if (argument[0] == '\0'
    || ((value = flag_value(token_flags, argument)) == NO_FLAG))
    {
	send_to_char("Syntax:  flags [token flag]\n\rType '? tokenflags' for a list of flags.\n\r", ch);
	return FALSE;
    }

    TOGGLE_BIT(token_index->flags, value);
    send_to_char("Token flag toggled.\n\r", ch);
    return TRUE;
}


TEDIT(tedit_timer)
{
    TOKEN_INDEX_DATA *token_index;
    EDIT_TOKEN(ch, token_index);
    int value;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  timer [number of ticks]\n\r", ch);
	return FALSE;
    }

    if ((value = atoi(argument)) < 0 || value > 65000)
    {
	send_to_char("Invalid value. Must be a number of ticks between 0 and 65,000.\n\r", ch);
	return FALSE;
    }

    token_index->timer = value;
    send_to_char("Timer set.\n\r", ch);
    return TRUE;
}


TEDIT(tedit_ed)
{
    TOKEN_INDEX_DATA *token_index;
    EXTRA_DESCR_DATA *ed;
    char command[MAX_INPUT_LENGTH];
    char keyword[MAX_INPUT_LENGTH];
    char copy_item[MAX_INPUT_LENGTH];

    return FALSE; // ED not implemented yet

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, command);
    argument = one_argument(argument, keyword);
    one_argument(argument, copy_item);

    if (command[0] == '\0' || keyword[0] == '\0')
    {
	send_to_char("Syntax:  ed add [keyword]\n\r", ch);
	send_to_char("         ed edit [keyword]\n\r", ch);
	send_to_char("         ed delete [keyword]\n\r", ch);
	send_to_char("         ed format [keyword]\n\r", ch);
	send_to_char("         ed copy existing_keyword new_keyword\n\r", ch);


	return FALSE;
    }

    if (!str_cmp(command, "copy"))
    {
	EXTRA_DESCR_DATA *ed2;

    	if (keyword[0] == '\0' || copy_item[0] == '\0')
	{
	   send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
	   return FALSE;
        }

	for (ed = token_index->ed; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
	    return FALSE;
	}

	ed2			=   new_extra_descr();
	ed2->keyword		=   str_dup(copy_item);
	ed2->description		=   str_dup(ed->description);
	ed2->next		=   token_index->ed;
	token_index->ed	=   ed2;

	send_to_char("Done.\n\r", ch);

	return TRUE;
    }

    if (!str_cmp(command, "add"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed add [keyword]\n\r", ch);
	    return FALSE;
	}

	ed			=   new_extra_descr();
	ed->keyword		=   str_dup(keyword);
	ed->description		=   str_dup("");
	ed->next		=   token_index->ed;
	token_index->ed	=   ed;

	string_append(ch, &ed->description);

	return TRUE;
    }


    if (!str_cmp(command, "edit"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed edit [keyword]\n\r", ch);
	    return FALSE;
	}

	for (ed = token_index->ed; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
	    return FALSE;
	}

	string_append(ch, &ed->description);

	return TRUE;
    }


    if (!str_cmp(command, "delete"))
    {
	EXTRA_DESCR_DATA *ped = NULL;

	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed delete [keyword]\n\r", ch);
	    return FALSE;
	}

	for (ed = token_index->ed; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	    ped = ed;
	}

	if (!ed)
	{
	    send_to_char("TEdit:  Extra description keyword not found.\n\r", ch);
	    return FALSE;
	}

	if (!ped)
	    token_index->ed = ed->next;
	else
	    ped->next = ed->next;

	free_extra_descr(ed);

	send_to_char("Extra description deleted.\n\r", ch);
	return TRUE;
    }


    if (!str_cmp(command, "format"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed format [keyword]\n\r", ch);
	    return FALSE;
	}

	for (ed = token_index->ed; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("TEDIT:  Extra description keyword not found.\n\r", ch);
	    return FALSE;
	}

	ed->description = format_string(ed->description);

	send_to_char("Extra description formatted.\n\r", ch);
	return TRUE;
    }

    redit_ed(ch, "");
    return TRUE;
}


TEDIT(tedit_description)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] != '\0')
    {
	send_to_char("Syntax:  desc\n\r", ch);
	return FALSE;
    }

    string_append(ch, &token_index->description);
    return TRUE;
}

TEDIT(tedit_comments)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] != '\0')
    {
	send_to_char("Syntax:  comment\n\r", ch);
	return FALSE;
    }

    string_append(ch, &token_index->comments);
    return TRUE;
}

TEDIT(tedit_value)
{
    TOKEN_INDEX_DATA *token_index;
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    int value_num;
    unsigned long value_value;

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
	send_to_char("Syntax:  value <number> <value>\n\r", ch);
	return FALSE;
    }

    if ((value_num = atoi(arg)) < 0 || value_num >= MAX_TOKEN_VALUES) {
	sprintf(buf, "Number must be 0-%d\n\r", MAX_TOKEN_VALUES);
	send_to_char(buf, ch);
	return FALSE;
    }

	if(token_index->type == TOKEN_SPELL) {
		switch(value_num) {
		case TOKVAL_SPELL_RATING:			// Max rating
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Max rating must be within range of 0 (for 100%%) to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Max rating set.\n\r", ch);
			break;
		case TOKVAL_SPELL_DIFFICULTY:			// Difficulty
			value_value = atoi(arg2);
			if(value_value < 1 || value_value > 1000000) {
				send_to_char("Difficulty must be within range of 1 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Difficulty set.\n\r", ch);
			break;
		case TOKVAL_SPELL_TARGET:			// Target Type
			value_value = flag_value(spell_target_types, arg2);
			if(value_value == NO_FLAG) value_value = TAR_IGNORE;

			send_to_char("Target type set.\n\r", ch);
			break;
		case TOKVAL_SPELL_POSITION:			// Minimum Position

			if ((value_value = flag_value(position_flags, arg2)) == NO_FLAG) {
				send_to_char("Invalid position for spell.\n\r", ch);
				return FALSE;
			}

			send_to_char("Minimum position set.\n\r", ch);
			break;
		case TOKVAL_SPELL_MANA:			// Mana cost
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Mana cost must be within range of 0 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Mana cost set.\n\r", ch);
			break;
		case TOKVAL_SPELL_LEARN:			// Learn cost
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Learn cost set.\n\r", ch);
			break;
		default:
			// Need to check for various things.
			value_value = atol(arg2);

			sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
			send_to_char(buf, ch);
			break;
		}

		token_index->value[value_num] = value_value;
	} else if(token_index->type == TOKEN_SKILL) {
		switch(value_num) {
		case TOKVAL_SPELL_RATING:			// Max rating
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Max rating must be within range of 0 (for 100%%) to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Max rating set.\n\r", ch);
			break;
		case TOKVAL_SPELL_DIFFICULTY:			// Difficulty
			value_value = atoi(arg2);
			if(value_value < 1 || value_value > 1000000) {
				send_to_char("Difficulty must be within range of 1 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Difficulty set.\n\r", ch);
			break;
		case TOKVAL_SPELL_LEARN:			// Learn cost
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Learn cost set.\n\r", ch);
			break;
		default:
			// Need to check for various things.
			value_value = atol(arg2);

			sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
			send_to_char(buf, ch);
			break;
		}

		token_index->value[value_num] = value_value;
	} else if(token_index->type == TOKEN_SONG) {
		switch(value_num) {
		case TOKVAL_SPELL_TARGET:			// Target Type
			value_value = flag_value(song_target_types, arg2);
			//Not sure what this one is for?
//			if();

			if(	value_value == NO_FLAG )
			{
				send_to_char("Invalid target for the song.\n\r", ch);
				send_to_char("See '? song_targets' \n\r", ch);
				return FALSE;
			}


			send_to_char("Target type set.\n\r", ch);
			break;
		case TOKVAL_SPELL_MANA:			// Mana cost
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Mana cost must be within range of 0 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Mana cost set.\n\r", ch);
			break;
		case TOKVAL_SPELL_LEARN:			// Learn cost
			value_value = atoi(arg2);
			if(value_value < 0 || value_value > 1000000) {
				send_to_char("Learn cost must be within range of 0 to 1000000.\n\r", ch);
				return FALSE;
			}

			send_to_char("Learn cost set.\n\r", ch);
			break;
		default:
			// Need to check for various things.
			value_value = atol(arg2);

			sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
			send_to_char(buf, ch);
			break;
		}

		token_index->value[value_num] = value_value;
	} else {
		// Need to check for various things.
		value_value = atol(arg2);

		token_index->value[value_num] = value_value;
		sprintf(buf, "Set value %d to %ld.\n\r", value_num, value_value);
		send_to_char(buf, ch);
	}
    return TRUE;
}


TEDIT(tedit_valuename)
{
    TOKEN_INDEX_DATA *token_index;
    char arg[MSL];
    char buf[MSL];
    int value_num;

    EDIT_TOKEN(ch, token_index);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0') {
	send_to_char("Syntax:  valuename <number> <string>\n\r", ch);
	return FALSE;
    }

    if ((value_num = atoi(arg)) < 0 || value_num >= MAX_TOKEN_VALUES) {
	sprintf(buf, "Number must be 0-%d\n\r", MAX_TOKEN_VALUES);
	send_to_char(buf, ch);
	return FALSE;
    }

    if (strlen(argument) <= 2) {
	send_to_char("Value name must have at least 3 characters.\n\r", ch);
	return FALSE;
    }

    free_string(token_index->value_name[value_num]);
    token_index->value_name[value_num] = str_dup(argument);
    sprintf(buf, "Set token %ld's value %d to be named '%s'.\n\r", token_index->vnum,
	    value_num, argument);
    send_to_char(buf, ch);
    return TRUE;
}


TEDIT (tedit_addtprog)
{
    int tindex, value, slot;
    TOKEN_INDEX_DATA *token_index;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_TOKEN(ch, token_index);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addtprog [vnum] [trigger] [phrase]\n\r",ch);
	return FALSE;
    }

    if ((tindex = trigger_index(trigger, PRG_TPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "tprog");
	return FALSE;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

	if(value == TRIG_SPELLCAST) {
		int sn = skill_lookup(phrase);
		if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
			send_to_char("Invalid spell for trigger.\n\r",ch);
			return FALSE;
		}
		sprintf(phrase,"%d",sn);
	}
	else if( value == TRIG_EXIT ||
		value == TRIG_EXALL ||
		value == TRIG_KNOCK ||
		value == TRIG_KNOCKING) {
		int door = parse_door(phrase);
		if( door < 0 ) {
			send_to_char("Invalid direction for exit/exall/knock/knocking trigger.\n\r", ch);
			return FALSE;
		}
		sprintf(phrase,"%d",door);
	} else if( value == TRIG_OPEN || value == TRIG_CLOSE) {
		int door = parse_door(phrase);
		if( door >= 0 && door < MAX_DIR ) {
			sprintf(phrase,"%d",door);
		}
	}

    if ((code = get_script_index (atol(num), PRG_TPROG)) == NULL)
    {
	send_to_char("No such TokenProgram.\n\r",ch);
	return FALSE;
    }

    // Make sure this has a list of progs!
    if(!token_index->progs) token_index->progs = new_prog_bank();

    if(!token_index->progs) {
	send_to_char("Could not define token_index->progs!\n\r",ch);
	return FALSE;
    }

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;
    //SET_BIT(token_index->mprog_flags,value);
    list_appendlink(token_index->progs[slot], list);

    send_to_char("Tprog Added.\n\r",ch);
    return TRUE;
}


TEDIT (tedit_deltprog)
{
    TOKEN_INDEX_DATA *token_index;
    char tprog[MAX_STRING_LENGTH];
    int value;

    EDIT_TOKEN(ch, token_index);

    one_argument(argument, tprog);
    if (!is_number(tprog) || tprog[0] == '\0')
    {
       send_to_char("Syntax:  delmprog [#mprog]\n\r",ch);
       return FALSE;
    }

    value = atol (tprog);

    if (value < 0)
    {
        send_to_char("Only non-negative tprog-numbers allowed.\n\r",ch);
        return FALSE;
    }

    if(!edit_deltrigger(token_index->progs,value)) {
	send_to_char("No such mprog.\n\r",ch);
	return FALSE;
    }

    send_to_char("Tprog removed.\n\r", ch);
    return TRUE;
}

TEDIT(tedit_varset)
{
    TOKEN_INDEX_DATA *token_index;
    char name[MIL];
    char type[MIL];
    char yesno[MIL];
    bool saved;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
	return FALSE;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, type);
    argument = one_argument(argument, yesno);

    if(!variable_validname(name)) {
	send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
	return FALSE;
    }

    saved = !str_cmp(yesno,"yes");

    if(!argument[0]) {
	send_to_char("Set what on the variable?\n\r", ch);
	return FALSE;
    }

    if(!str_cmp(type,"room")) {
	if(!is_number(argument)) {
	    send_to_char("Specify a room vnum.\n\r", ch);
	    return FALSE;
	}

	variables_setindex_room(&token_index->index_vars,name,atoi(argument),saved);
    } else if(!str_cmp(type,"string"))
	variables_setindex_string(&token_index->index_vars,name,argument,FALSE,saved);
    else if(!str_cmp(type,"number")) {
	if(!is_number(argument)) {
	    send_to_char("Specify an integer.\n\r", ch);
	    return FALSE;
	}

	variables_setindex_integer(&token_index->index_vars,name,atoi(argument),saved);
    } else {
	send_to_char("Invalid type of variable.\n\r", ch);
	return FALSE;
    }
    send_to_char("Variable set.\n\r", ch);
    return TRUE;
}

TEDIT(tedit_varclear)
{
    TOKEN_INDEX_DATA *token_index;

    EDIT_TOKEN(ch, token_index);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  varclear <name>\n\r", ch);
	return FALSE;
    }

    if(!variable_validname(argument)) {
	send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
	return FALSE;
    }

    if(!variable_remove(&token_index->index_vars,argument)) {
	send_to_char("No such variable defined.\n\r", ch);
	return FALSE;
    }

    send_to_char("Variable cleared.\n\r", ch);
    return TRUE;
}

char *token_index_getvaluename(TOKEN_INDEX_DATA *token, int v)
{
	if(token->type == TOKEN_SPELL )
	{
		if( v == TOKVAL_SPELL_RATING ) return "Rating";
		else if( v == TOKVAL_SPELL_DIFFICULTY ) return "Difficulty";
		else if( v == TOKVAL_SPELL_TARGET ) return "Spell Target";
		else if( v == TOKVAL_SPELL_POSITION ) return "Min Position";
		else if( v == TOKVAL_SPELL_MANA ) return "Mana Cost";
		else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
	}
	else if( token->type == TOKEN_SKILL )
	{
		if( v == TOKVAL_SPELL_RATING ) return "Rating";
		else if( v == TOKVAL_SPELL_DIFFICULTY ) return "Difficulty";
		else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
	}
	else if( token->type == TOKEN_SONG )
	{
		if( v == TOKVAL_SPELL_TARGET ) return "Song Target";
		else if( v == TOKVAL_SPELL_MANA ) return "Mana Cost";
		else if( v == TOKVAL_SPELL_LEARN ) return "Learn Cost";
	}

	return token->value_name[v];
}

