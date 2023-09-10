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
#include <ctype.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "db.h"

//extern long int   __BUILD_DATE;
extern char __BUILD_DATE;
extern long int   __BUILD_NUMBER;

/* MOVED: combat/assess.c */
void do_showdamage(CHAR_DATA *ch, char *argument)
{
#ifndef DEBUG_ALLOW_SHOW_DAMAGE
    if (!IS_IMMORTAL(ch) && !is_test_port) {
	send_to_char("As a player, you may only see the damages of hits on the testport.\n\r", ch);
	return;
    }
#endif

    if (IS_SET(ch->act, PLR_SHOWDAMAGE)) {
		REMOVE_BIT(ch->act, PLR_SHOWDAMAGE);
		send_to_char("You will no longer see the damages of hits.\n\r", ch);
    } else {
		SET_BIT(ch->act, PLR_SHOWDAMAGE);
		send_to_char("You will now see the damages of hits.\n\r", ch);
    }
}

/* MOVED: ship.c */
void do_autosurvey(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
	return;

    if (IS_SET(ch->act2, PLR_AUTOSURVEY))
    {
	REMOVE_BIT(ch->act2, PLR_AUTOSURVEY);
	send_to_char("You will no longer automatically survey on ships.\n\r", ch);
    }
    else
    {
	SET_BIT(ch->act2, PLR_AUTOSURVEY);
	send_to_char("You will now automatically survey on ships.\n\r", ch);
    }
}

void do_showversion(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	buf[0] = '\0';
//	time_t  build_date;
//	build_date = (time_t) &__BUILD_DATE;
//	builddate = &__BUILD_DATE)
//	sprintf(buf,"Build Date: %u\n\r",&build_date);
	sprintf(buf,"Build Number: %s (\t<a href=\"%s\">%s\t</a>)\n\rCommit URL: %s\n\rBuild Date: %s\n\r", BUILD_NUMBER, COMMIT, VERSION, COMMIT, BUILD_DATE);
	send_to_char(buf,ch);

}



