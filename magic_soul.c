/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "wilds.h"

SPELL_FUNC(spell_soul_essence)
{
    int sn __attribute__((unused)) = skill->uid;
    char buf[MSL];
    OBJ_DATA *obj;
    char *arg = (char *) vo;
    int souls, i;
    int skill_pct, skill2;
    bool found = false, all;
    ITERATOR it;

    if(IS_NPC(ch)) return false;

    if (!arg) return false;

    if (!arg[0] || (!is_number(arg) && str_cmp(arg,"all"))) {
        send_to_char("How much soul essence did you want to absorb?\n\r", ch);
        return false;
    }

    all = !str_cmp(arg,"all");
    souls = atoi(arg);

    // Use the lcarrying LLIST instead of the old carrying linked list
    if (ch->lcarrying) {
        i = 0;
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)) && (all || i < souls)) {
            if (obj->pIndexData == get_reserved_obj_index("obj_pneuma_item")) {
                found = true;
                // Need to remove from list before extracting to prevent invalid list access
                list_remlink(ch->lcarrying, obj, false);
                extract_obj(obj);
                i++;
            }
        }
        iterator_stop(&it);
    }

    if (found) {
        int16_t sn_soul = skill_resolve_gsn("soul essence");
        skill_pct = get_skill(ch,sn_soul); skill_pct = UMAX(0,skill_pct);
        skill2 = get_skill(ch,sn_soul); skill2 = UMAX(0,skill2);

        i = i * skill_pct * skill2 / 10000;

        // Give boost for avatars and wraiths
        if(ch->race && (!str_cmp(ch->race->id, "avatar") || !str_cmp(ch->race->id, "wraith")))
            i = i * ( 240 + ch->tot_level ) / 240;

        if(i > 0) {
            sprintf(buf, "{BYou feel {C%d{B soul%s flowing into you!{x\n\r", i, ((i==1)?"":"s"));
            send_to_char(buf,ch);
            act("{B$n glows briefly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

            if (boost_table[BOOST_PNEUMA].boost != 100)
            {
                send_to_char("{WPNEUMA boost!{x\n\r", ch);
                   ch->pneuma += (i * boost_table[BOOST_PNEUMA].boost)/100;
            }
            else
                ch->pneuma += i;
            
        } else
            send_to_char("You absorb soul essence, but it completely dissipates...\n\r", ch);
    } else
        send_to_char("You lack soul essence to absorb.\n\r", ch);
    return true;
}

