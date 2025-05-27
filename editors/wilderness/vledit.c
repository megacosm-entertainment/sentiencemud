/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

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
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"


VLEDIT ( vledit_show )
{
    WILDS_VLINK *pVLink;

    pVLink = (WILDS_VLINK *)ch->desc->pEdit;
    send_to_char("{x[ {WVLedit show{x ]\n\r\n\r", ch);

    printf_to_char(ch, "Uid: %ld\n\r",   pVLink->uid);
    printf_to_char(ch, "Wilds vroom x coor: %ld\n\r",   pVLink->wildsorigin_x);
    printf_to_char(ch, "Wilds vroom y coor: %ld\n\r",   pVLink->wildsorigin_y);
    printf_to_char(ch, "Wilds map tile: %ld\n\r", pVLink->map_tile);
    printf_to_char(ch, "Wilds link direction: %s\n\r",   dir_name[pVLink->door]);
    printf_to_char(ch, "Destination room vnum: %ld\n\r",   pVLink->destvnum);
    printf_to_char(ch, "Default linkage flags: %s\n\r",   vlinkage_bit_name(pVLink->default_linkage));
    printf_to_char(ch, "Current linkage flags: %s\n\r\n\r",   vlinkage_bit_name(pVLink->current_linkage));
    printf_to_char(ch, "Wilds link description: %s\n\r",   pVLink->orig_description);
    printf_to_char(ch, "Wilds link keyword: %s\n\r",   pVLink->orig_keyword);
    printf_to_char(ch, "Wilds link rs_flags: %s\n\r",   flag_string(exit_flags, pVLink->orig_rs_flags));
    printf_to_char(ch, "Wilds key obj vnum: %ld\n\r\n\r",   pVLink->orig_key);
    printf_to_char(ch, "Wilds lock flags: %s\n\r",   flag_string(lock_flags, pVLink->orig_lock));
    printf_to_char(ch, "Wilds link pick chance: %d%%\n\r\n\r",   pVLink->orig_pick);
    printf_to_char(ch, "Reverse link description: %s\n\r",   pVLink->rev_description);
    printf_to_char(ch, "Reverse link keyword: %s\n\r",   pVLink->rev_keyword);
    printf_to_char(ch, "Reverse link rs_flags: %s\n\r",   flag_string (exit_flags, pVLink->rev_rs_flags));
    printf_to_char(ch, "Reverse key obj vnum: %ld\n\r\n\r",   pVLink->rev_key);
    printf_to_char(ch, "Reverse lock flags: %s\n\r",   flag_string(lock_flags, pVLink->rev_lock));
    printf_to_char(ch, "Reverse link pick chance: %d%%\n\r\n\r",   pVLink->rev_pick);

    return (false);
}

