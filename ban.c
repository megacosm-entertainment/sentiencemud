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
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
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
#include "merc.h"
#include "recycle.h"
#include "io/json/json_ban.h"

BAN_DATA *ban_list;


/**
 * save_bans - Write permanent bans to disk
 *
 * Saves all bans with the BAN_PERMANENT flag to BAN_FILE.
 * Temporary bans are not persisted. If no permanent bans exist,
 * the ban file is deleted.
 *
 * File format: name level flags (one per line)
 */
void save_bans(void)
{
    char null_file_buf[MAX_INPUT_LENGTH];
    const char *null_file = resolve_game_path(NULL_FILE, null_file_buf, sizeof(null_file_buf));
    fclose(fpReserve);
    json_save_bans(BAN_JSON_FILE);
    fpReserve = fopen(null_file, "r");
}


/**
 * load_bans - Load saved bans from disk at boot time
 *
 * Reads BAN_FILE and populates ban_list with all saved bans.
 * Called during server initialization.
 */
void load_bans(void)
{
    if (!json_load_bans(BAN_JSON_FILE))
        pbugf(LOG_ERROR, "load_bans: failed to load bans from JSON");
}


/**
 * check_ban - Check if a site/host matches any active bans
 *
 * Checks the ban_list for matches against the given site name.
 * Supports wildcard matching:
 * - BAN_PREFIX: ban name matches end of host (*example.com)
 * - BAN_SUFFIX: ban name matches start of host (example.*)
 * - Both: ban name found anywhere in host (*example*)
 *
 * @param site  Hostname or IP to check
 * @param type  Ban type to check for (BAN_ALL, BAN_NEWBIES, BAN_PERMIT)
 * @return      true if site is banned for this type, false otherwise
 */
bool check_ban(char *site,int type)
{
    BAN_DATA *pban;
    char host[MAX_STRING_LENGTH];

    strcpy(host,capitalize(site));
    host[0] = LOWER(host[0]);

    for ( pban = ban_list; pban != NULL; pban = pban->next )
    {
    if(!IS_SET(pban->ban_flags,type))
        continue;

    if (IS_SET(pban->ban_flags,BAN_PREFIX)
    &&  IS_SET(pban->ban_flags,BAN_SUFFIX)
    &&  strstr(pban->name,host) != NULL)
        return true;

    if (IS_SET(pban->ban_flags,BAN_PREFIX)
    &&  !str_suffix(pban->name,host))
        return true;

    if (IS_SET(pban->ban_flags,BAN_SUFFIX)
    &&  !str_prefix(pban->name,host))
        return true;
    }

    return false;
}


/**
 * ban_site - Internal function to add or list bans
 *
 * With no arguments, lists all current bans with their level, type, and status.
 * With arguments, creates a new ban entry.
 *
 * Syntax: ban [site] [type]
 * - site: Hostname/IP to ban. Use * prefix/suffix for wildcards.
 *   Examples: *aol.com, 192.168.*, *bad*
 * - type: all (default), newbies (new chars only), permit (requires permit)
 *
 * Ban precedence: Staff rank determines who can modify/remove a ban.
 * If a ban already exists for the site, it's replaced if the caller
 * has sufficient rank.
 *
 * @param ch     Staff member issuing the ban
 * @param argument  Site and optional type
 * @param fPerm  true for permanent ban (saved to file), false for temp
 */
void ban_site(CHAR_DATA *ch, char *argument, bool fPerm)
{
    char buf[2*MAX_STRING_LENGTH],buf2[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    char *name;
    BUFFER *buffer;
    BAN_DATA *pban, *prev;
    bool prefix = false,suffix = false;
    int type;

    argument = one_argument(argument,arg1);
    argument = one_argument(argument,arg2);

    if ( arg1[0] == '\0' )
    {
    if (ban_list == NULL)
    {
        send_to_char("No sites banned at this time.\n\r",ch);
        return;
      }
    buffer = new_buf();

        add_buf(buffer,"Banned sites  level  type     status\n\r");
        for (pban = ban_list;pban != NULL;pban = pban->next)
        {
            sprintf(buf2,"%s%s%s",
                IS_SET(pban->ban_flags,BAN_PREFIX) ? "*" : "",
                pban->name,
                IS_SET(pban->ban_flags,BAN_SUFFIX) ? "*" : "");
                sprintf(buf,"%-12s    %-3d  %-7s  %s\n\r",
                buf2, pban->level,
                IS_SET(pban->ban_flags,BAN_NEWBIES) ? "newbies" :
                IS_SET(pban->ban_flags,BAN_PERMIT)  ? "permit"  :
                IS_SET(pban->ban_flags,BAN_ALL)     ? "all"	: "",
                IS_SET(pban->ban_flags,BAN_PERMANENT) ? "perm" : "temp");
            add_buf(buffer,buf);
        }

        page_to_char( buf_string(buffer), ch );
    free_buf(buffer);
        return;
    }

    /* find out what type of ban */
    if (arg2[0] == '\0' || !str_prefix(arg2,"all"))
    type = BAN_ALL;
    else if (!str_prefix(arg2,"newbies"))
    type = BAN_NEWBIES;
    else if (!str_prefix(arg2,"permit"))
    type = BAN_PERMIT;
    else
    {
    send_to_char("Acceptable ban types are all, newbies, and permit.\n\r",
        ch);
    return;
    }

    name = arg1;

    if (name[0] == '*')
    {
    prefix = true;
    name++;
    }

    if (name[strlen(name) - 1] == '*')
    {
    suffix = true;
    name[strlen(name) - 1] = '\0';
    }

    if (strlen(name) == 0)
    {
    send_to_char("You have to ban SOMETHING.\n\r",ch);
    return;
    }

    prev = NULL;
    for ( pban = ban_list; pban != NULL; prev = pban, pban = pban->next )
    {
        if (!str_cmp(name,pban->name))
        {
        if (pban->rank > get_staff_rank(ch))
        {
                send_to_char( "That ban was set by a higher power.\n\r", ch );
                return;
        }
        else
        {
        if (prev == NULL)
            ban_list = pban->next;
        else
            prev->next = pban->next;
        free_ban(pban);
        }
        }
    }

    pban = new_ban();
    pban->name = str_dup(name);
    pban->rank = get_staff_rank(ch);

    /* set ban type */
    pban->ban_flags = type;

    if (prefix)
    SET_BIT(pban->ban_flags,BAN_PREFIX);
    if (suffix)
    SET_BIT(pban->ban_flags,BAN_SUFFIX);
    if (fPerm)
    SET_BIT(pban->ban_flags,BAN_PERMANENT);

    pban->next  = ban_list;
    ban_list    = pban;
    save_bans();
    sprintf(buf,"%s has been banned.\n\r",pban->name);
    send_to_char( buf, ch );
    return;
}


/**
 * do_ban - Staff command to create a temporary site ban
 *
 * Creates a ban that lasts until server reboot (not saved to file).
 * Wrapper for ban_site() with fPerm=false.
 *
 * @param ch        Staff member issuing the ban
 * @param argument  Site pattern and optional ban type
 */
void do_ban(CHAR_DATA *ch, char *argument)
{
    ban_site(ch,argument,false);
}


/**
 * do_permban - Staff command to create a permanent site ban
 *
 * Creates a ban that persists across reboots (saved to BAN_FILE).
 * Wrapper for ban_site() with fPerm=true.
 *
 * @param ch        Staff member issuing the ban
 * @param argument  Site pattern and optional ban type
 */
void do_permban(CHAR_DATA *ch, char *argument)
{
    ban_site(ch,argument,true);
}


/**
 * do_allow - Staff command to remove a site ban
 *
 * Removes a ban from the ban_list if the caller has sufficient rank
 * (must be >= the rank of the staff member who created the ban).
 * Saves updated ban list to file.
 *
 * @param ch        Staff member removing the ban
 * @param argument  Name of the banned site to unban
 */
void do_allow( CHAR_DATA *ch, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    BAN_DATA *prev;
    BAN_DATA *curr;

    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
        send_to_char( "Remove which site from the ban list?\n\r", ch );
        return;
    }

    prev = NULL;
    for ( curr = ban_list; curr != NULL; prev = curr, curr = curr->next )
    {
        if ( !str_cmp( arg, curr->name ) )
        {
        if (curr->rank > get_staff_rank(ch))
        {
        send_to_char(
           "You are not powerful enough to lift that ban.\n\r",ch);
        return;
        }
            if ( prev == NULL )
                ban_list   = ban_list->next;
            else
                prev->next = curr->next;

            free_ban(curr);
        sprintf(buf,"Ban on %s lifted.\n\r",arg);
            send_to_char( buf, ch );
        save_bans();
            return;
        }
    }

    send_to_char( "Site is not banned.\n\r", ch );
    return;
}
