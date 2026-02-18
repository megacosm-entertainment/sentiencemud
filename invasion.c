/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "olc.h"

INVASION_QUEST* create_invasion_quest(AREA_DATA *pArea, int max_level, long p_leader_vnum, long p_mob_vnum) {
    (void)pArea;
    (void)max_level;
    (void)p_leader_vnum;
    (void)p_mob_vnum;

    log_message(LOG_LEVEL_INFO, LOG_ERROR, "create_invasion_quest: legacy invasion system is deprecated; use event runtime.");
    return NULL;
}

void extract_invasion_quest(INVASION_QUEST *quest) {
    if (!quest)
        return;

    free_invasion_quest(quest);
}

void check_invasion_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *victim) {
    (void)ch;
    (void)victim;
}

