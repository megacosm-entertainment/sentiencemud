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

void do_mount(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    CHAR_DATA *mount;

    argument = one_argument(argument, arg);

    if (IS_NPC(ch))
	return;

    if (IS_DEAD(ch)) {
	send_to_char("You can't do that. You are dead.\n\r", ch);
	return;
    }

    if (arg[0] == '\0' && ch->mount && ch->mount->in_room == ch->in_room) {
	mount = ch->mount;
    } else if (!(mount = get_char_room(ch, NULL, arg))) {
	send_to_char("Mount what?\n\r", ch);
	return;
    }

    if (!IS_NPC(mount)) {
	sprintf(buf, "I'm sure %s really wouldn't like you doing that.\n\r", mount->name);
	send_to_char(buf, ch);
	return;
    }

    if (!IS_SET(mount->act, ACT_MOUNT))
    {
	sprintf(buf,"You can't ride that.\n\r");
	send_to_char(buf, ch);
	return;
    }

    if (mount->level - 50 > ch->tot_level)
    {
	send_to_char("That beast is too powerful for you to ride.", ch);
	return;
    }

    if((mount->rider) && (!mount->riding) && (mount->rider != ch))
    {
	sprintf(buf, "%s belongs to %s, not you.\n\r",
	    mount->short_descr, mount->mount->name);
	send_to_char(buf, ch);
	return;
    }

    if (mount->position < POS_STANDING)
    {
	send_to_char("Your mount must be standing.\n\r", ch);
	return;
    }

    if (RIDDEN(mount))
    {
	send_to_char("This beast is already ridden.\n\r", ch);
	return;
    }
    else if (MOUNTED(ch))
    {
	send_to_char("You are already riding.\n\r", ch);
	return;
    }

    if (str_cmp(mount->owner, ch->name)
    && str_cmp(mount->owner, "(no owner)")
    && str_cmp(mount->owner, "(null)"))
    {
	act("{ROUCH! You attempt to mount $N, but $E bucks and kicks you off!{x", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$n attempts to mount $N, but $E bucks and kicks $m off!{x", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	damage(mount, ch, ch->hit/5, gsn_kick, DAM_BASH, FALSE);
	stop_fighting(ch, TRUE);
	ch->position = POS_RESTING;
	ch->bashed = 4;
	return;
    }

    if(p_percent_trigger(mount, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREMOUNT, NULL))
    	return;

    act("You hop on $N's back.", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("$n hops on $N's back.", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
    act("$n hops on your back!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_VICT);

    ch->mount = mount;
    ch->riding = TRUE;
    mount->rider = ch;
    mount->riding = TRUE;

    p_percent_trigger(mount, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MOUNT, NULL);

    affect_strip(ch, gsn_sneak);
    REMOVE_BIT(ch->affected_by, AFF_SNEAK);
    affect_strip(ch, gsn_hide);
    REMOVE_BIT(ch->affected_by, AFF_HIDE);

    if (get_skill(ch, gsn_riding) > 0)
	add_grouped(mount, ch, TRUE);
}


void do_dismount(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mount;

    if(MOUNTED(ch))
    {
	mount = MOUNTED(ch);

	if(p_percent_trigger(mount, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDISMOUNT, NULL))
		return;

	act("You dismount from $N.",  ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n dismounts from $N.",  ch, mount, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("$n dismounts from you.", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	ch->riding = FALSE;
	mount->riding = FALSE;
	mount->rider = NULL;
	ch->mount = NULL;
    }
    else
    {
	send_to_char("You aren't mounted.\n\r", ch);
	return;
    }

    stop_grouped(mount);

    // Kick the mount out of chat so that they can't be used
    // to transfer items in chat. Personal mounts arn't booted because
    // nobody else can mount them except the owner
    if (IS_SOCIAL(mount) && str_cmp(ch->name, mount->owner))
    {
	act("{WA ghostly spirit appears before $n and pulls $m back to the mortal world.{x", mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        do_function(mount, &do_chat, "exit");
    }
}

// Must be loaded
CHAR_DATA *get_personal_mount(CHAR_DATA *ch)
{
    if (IS_NPC(ch)) return NULL;

    if (!ch->pcdata->personal_mount.pArea || ch->pcdata->personal_mount.vnum < 1)
        return NULL;

    MOB_INDEX_DATA *pMobIndex = get_mob_index_wnum(ch->pcdata->personal_mount);
    if (!pMobIndex)
        return NULL;
    
    return get_char_world_index(NULL, pMobIndex);
}

// TODO: Rework.. just store it on the PCDATA?
CHAR_DATA *find_personal_mount(char *name)
{
    MOB_INDEX_DATA *mIndex;
    CHAR_DATA *mount;
    int vnum;

    if (area_housing == NULL)
    {
        bug("find_personal_mount: no housing area", 0);
	return NULL;
    }

    for (vnum = 1; vnum <= area_housing->top_mob_index; vnum++)
    {
    	if ((mIndex = get_mob_index(area_housing, vnum)) != NULL)
	{
	    if (!str_cmp(mIndex->owner, name))
	    {
	    	if ((mount = get_char_world_index(NULL, mIndex)) != NULL)
		    return mount;
	    }
	}
    }

    return NULL;
}


void do_whistle(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    CHAR_DATA *mount;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->pIndexData == obj_index_gold_whistle)
	    break;
    }

    if (obj == NULL || IS_NPC(ch))
    {
	send_to_char("You whistle a little tune to yourself.\n\r", ch);
	act("$n whistles a little tune to $mself.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
    }

    if ((mount = find_personal_mount(ch->name)) == NULL)
    {
    	act("You whistle on $p loudly, but nothing happens.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    act("You whistle on $p loudly and $N appears out of nowhere.", ch, mount, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n whistles on $p loudly and $N appears out of nowhere.", ch, mount, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    char_from_room(mount);
    char_to_room(mount, ch->in_room);
    mount->position = POS_STANDING;
}
