/**************************************************************************r
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "wilds.h"

extern LLIST *loaded_instances;

// from act_info.c
void show_char_to_char args((CHAR_DATA * list, CHAR_DATA * ch, CHAR_DATA * victim));
bool check_blind args((CHAR_DATA * ch));


/* returns number of people on an object */
int count_users(OBJ_DATA *obj)
{
    CHAR_DATA *fch;
    int count = 0;

    if (obj->in_room == NULL)
	return 0;

    for (fch = obj->in_room->people; fch != NULL; fch = fch->next_in_room)
	if (fch->on == obj)
	    count++;

    return count;
}


/* return weapon type given its name string */
int weapon_lookup (const char *name)
{

    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
    {
	if (LOWER(name[0]) == LOWER(weapon_table[type].name[0])
	&&  !str_prefix(name,weapon_table[type].name))
	    return type;
    }

    return -1;
}


/* count # of items in a list */
int count_items_list(OBJ_DATA *list)
{
    int i;
    OBJ_DATA *obj;

    i = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    	i++;

    return i;
}


// Include stuff inside containers
int count_items_list_nest(OBJ_DATA *list)
{
    int i;
    OBJ_DATA *obj;

    i = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
	i++;
	if (obj->contains != NULL)
	    i += count_items_list(obj->contains);
    }

    return i;
}


int ranged_weapon_type (const char *name)
{
    int type;

    for (type = 0; ranged_weapon_table[type].name != NULL; type++)
    {
        if (LOWER(name[0]) == LOWER(ranged_weapon_table[type].name[0])
        &&  !str_prefix(name,ranged_weapon_table[type].name))
            return ranged_weapon_table[type].type;
    }

    return RANGED_WEAPON_EXOTIC;
}


int weapon_type (const char *name)
{
    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
    {
        if (LOWER(name[0]) == LOWER(weapon_table[type].name[0])
        &&  !str_prefix(name,weapon_table[type].name))
            return weapon_table[type].type;
    }

    return WEAPON_UNKNOWN;
}


// Return item type string from integer
char *item_name(int item_type)
{
    int type;

    for (type = 0; item_table[type].name != NULL; type++)
	if (item_type == item_table[type].type)
	    return item_table[type].name;

    return "none";
}


// Return weapon type string from integer
char *weapon_name(int weapon_type)
{
    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
        if (weapon_type == weapon_table[type].type)
            return weapon_table[type].name;

    return "unknown";
}


// Return ranged weapon type string from integer
char *ranged_weapon_name(int weapon_type)
{
    int type;

    for (type = 0; ranged_weapon_table[type].name != NULL; type++)
        if (weapon_type == ranged_weapon_table[type].type)
            return ranged_weapon_table[type].name;
    return "exotic";
}


/* find a location from a tag. Very handy! */
ROOM_INDEX_DATA *find_location(CHAR_DATA *ch, char *arg)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AREA_DATA *area;
	ROOM_INDEX_DATA *room = NULL, *rm;
	int vnum;
	char *save;
	char arg1[MIL];
	char arg2[MIL];

	// Goto <area name> ie "goto plith"
	for (area = area_first; area != NULL; area = area->next) {
		if (!is_number(arg) && !str_infix(arg, area->name)) {
			if (!(room = location_to_room(&area->recall))) {
				for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++) {
					if ((rm = get_room_index(vnum)))
						room = rm;
				}
			}

			if (room)
				return room;

			break;
		}
	}

    save = arg;
    arg = one_argument(arg,arg1);
    arg = one_argument(arg,arg2);

	// Done to allow for going to cloned rooms, but only if they exist!
    if (is_number(arg1)) {
	room = get_room_index(atol(arg1));
	if(is_number(arg2) && is_number(arg))
	{
		//log_stringf("get_clone_room: find_location(%ld,%lu,%lu)", room->vnum,atol(arg2),atol(arg));

		return get_clone_room(room,atol(arg2),atol(arg));
	}
	else
		return room;
	}

	arg = save;
    if ((victim = get_char_world(ch, arg)) != NULL)
	return victim->in_room;

    if ((obj = get_obj_world(ch, arg)) != NULL)
		return obj_room(obj);

    return NULL;
}


int attack_lookup(const char *name)
{
    int att;

    for (att = 0; attack_table[att].name != NULL; att++)
    {
	if (LOWER(name[0]) == LOWER(attack_table[att].name[0])
	&&  !str_prefix(name,attack_table[att].name))
	    return att;
    }

    return 0;
}


/* returns a flag for wiznet */
long wiznet_lookup (const char *name)
{
    long flag;

    for (flag = 0; wiznet_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(wiznet_table[flag].name[0])
	&& !str_prefix(name,wiznet_table[flag].name))
	    return flag;
    }

    return -1;
}


/* returns class number */
int class_lookup(const char *name)
{
   int class;

   for (class = 0; class < MAX_CLASS; class++)
   {
        if (LOWER(name[0]) == LOWER(class_table[class].name[0])
        &&  !str_prefix(name,class_table[class].name))
            return class;
   }

   return -1;
}


int sub_class_lookup(CHAR_DATA *ch, const char *name)
{
   int sub_class;

   for (sub_class = 0; sub_class < MAX_SUB_CLASS; sub_class++)
   {
	if (!str_prefix(name, sub_class_table[sub_class].name[ch->sex])
	&&  !sub_class_table[sub_class].remort)
	{
	    if (sub_class_table[sub_class].alignment == ALIGN_GOOD
	    &&  ch->alignment < 0)
		return -1;

	    if (sub_class_table[sub_class].alignment == ALIGN_EVIL
	    &&  ch->alignment > 0)
		return -1;

	    return sub_class;
	}
   }

   return -1;
}

int sub_class_search(const char *name)
{
   int sub_class;

   for (sub_class = 0; sub_class < MAX_SUB_CLASS; sub_class++)
	if (!str_prefix(name, sub_class_table[sub_class].name[SEX_NEUTRAL]) ||
		!str_prefix(name, sub_class_table[sub_class].name[SEX_MALE]) ||
		!str_prefix(name, sub_class_table[sub_class].name[SEX_FEMALE]))
	    return sub_class;
   return -1;
}

/* for immunity, vulnerabiltiy, and resistant
   the 'globals' (magic and weapons) may be overriden
   three other cases -- wood, silver, and iron -- are checked in fight.c */
int check_immune(CHAR_DATA *ch, sh_int dam_type)
{
    int immune, def;
    long bit;

    immune = -1;
    def = IS_NORMAL;

    if (dam_type == DAM_NONE)
	return immune;

    if (dam_type <= 3)
    {
	if (IS_SET(ch->imm_flags,IMM_WEAPON))
	    def = IS_IMMUNE;
	else if (IS_SET(ch->res_flags,RES_WEAPON))
	    def = IS_RESISTANT;
	else if (IS_SET(ch->vuln_flags,VULN_WEAPON))
	    def = IS_VULNERABLE;
    }
    /*
    else  magical attack
    {
	if (IS_SET(ch->imm_flags,IMM_MAGIC))
	    def = IS_IMMUNE;
	else if (IS_SET(ch->res_flags,RES_MAGIC))
	    def = IS_RESISTANT;
	else if (IS_SET(ch->vuln_flags,VULN_MAGIC))
	    def = IS_VULNERABLE;
    }
    */

    switch (dam_type)
    {
	case(DAM_BASH):		bit = IMM_BASH;		break;
	case(DAM_PIERCE):	bit = IMM_PIERCE;	break;
	case(DAM_SLASH):	bit = IMM_SLASH;	break;
	case(DAM_FIRE):		bit = IMM_FIRE;		break;
	case(DAM_COLD):		bit = IMM_COLD;		break;
	case(DAM_LIGHTNING):	bit = IMM_LIGHTNING;	break;
	case(DAM_ACID):		bit = IMM_ACID;		break;
	case(DAM_POISON):	bit = IMM_POISON;	break;
	case(DAM_NEGATIVE):	bit = IMM_NEGATIVE;	break;
	case(DAM_HOLY):		bit = IMM_HOLY;		break;
	case(DAM_MAGIC):	bit = IMM_MAGIC;	break;
	case(DAM_ENERGY):	bit = IMM_ENERGY;       break;
	case(DAM_MENTAL):	bit = IMM_MENTAL;	break;
	case(DAM_DISEASE):	bit = IMM_DISEASE;	break;
	case(DAM_DROWNING):	bit = IMM_DROWNING;	break;
	case(DAM_LIGHT):	bit = IMM_LIGHT;	break;
	case(DAM_CHARM):	bit = IMM_CHARM;	break;
	case(DAM_SOUND):	bit = IMM_SOUND;	break;
	case(DAM_WATER):	bit = IMM_WATER;	break;	// @@@NIB : 20070120
	case(DAM_AIR):		bit = IMM_AIR;		break;	// @@@NIB : 20070125
	case(DAM_EARTH):	bit = IMM_EARTH;	break;	// @@@NIB : 20070125
	case(DAM_PLANT):	bit = IMM_PLANT;	break;	// @@@NIB : 20070125
	default:		return def;
    }

    if (IS_SET(ch->imm_flags,bit))
	immune = IS_IMMUNE;
    else if (IS_SET(ch->res_flags,bit) && immune != IS_IMMUNE)
	immune = IS_RESISTANT;
    else if (IS_SET(ch->vuln_flags,bit))
    {
	if (immune == IS_IMMUNE)
	    immune = IS_RESISTANT;
	else if (immune == IS_RESISTANT)
	    immune = IS_NORMAL;
	else
	    immune = IS_VULNERABLE;
    }

    if (immune == -1)
	return def;
    else
      	return immune;
}



// Get a character's % at a skill.
int get_skill(CHAR_DATA *ch, int sn)
{
    int skill;

    // Racial skills
    if (!IS_NPC(ch))
    {
        int i;

        for (i = 0; pc_race_table[ch->race].skills[i] != NULL; i++)
        {
            if (!str_cmp(skill_table[sn].name,pc_race_table[ch->race].skills[i])) {
		    skill = ch->pcdata->learned[sn];
		    if(skill <= 0) return skill;
		    skill += ch->pcdata->mod_learned[sn];
		    return URANGE(1,skill,100);
	    }
	}
    }

    if (sn == -1) /* shorthand for level based skills */
        skill = ch->level * 5 / 2;
    else if (sn < -1 || sn > MAX_SKILL)
    {
	char buf[MAX_STRING_LENGTH];
	sprintf(buf, "Bad sn %d in get_skill, on char %s.",sn,ch->name);
	skill = 0;
    }
    else if (!IS_NPC(ch))
    {
	int this_class;

	this_class = get_this_class(ch,sn);

	if (had_skill(ch,sn) || ch->level >= skill_table[sn].skill_level[this_class])
	    skill = ch->pcdata->learned[sn];
	else
	    skill = 0;

	if(skill > 0) {
		skill += ch->pcdata->mod_learned[sn];
		skill = URANGE(1,skill,100);
	}
    }
    else /* mobiles */
    {
        /* AO 092916 -- Making it better, but not perfetc, for now. Based on flags.
	 * Changed level calculation to the log functio to scale
	 * well up to lv500  */

	// Account for racial skills.
	if (skill_table[sn].race != -1 && ch->race != skill_table[sn].race)
	    skill = 0;
	if (ch->tot_level < 10)
	    skill = 10;

	/* Handle spells */
	if (skill_table[sn].spell_fun != spell_null) {
		if (ch->max_mana > 0)
			skill = 40+19 * log10(ch->tot_level)/2;
		else
			skill = 0;
	} //thief skills
	else if (IS_SET(ch->act, ACT_THIEF) && (sn == gsn_sneak || sn == gsn_hide))
	    skill = 40+19 * log10(ch->tot_level)/2;

        else if ((sn == gsn_dodge && IS_SET(ch->off_flags,OFF_DODGE))
 	||       (sn == gsn_parry && IS_SET(ch->off_flags,OFF_PARRY)))
	    skill = 40+19 * log10(ch->tot_level)/2;

 	else if (sn == gsn_shield_block)
	    skill = 40+19 * log10(ch->tot_level)/2;

	else if (sn == gsn_second_attack
	&& (IS_SET(ch->act,ACT_WARRIOR) || IS_SET(ch->act,ACT_THIEF)))
	    skill = 40+19 * log10(ch->tot_level)/2;

	else if (sn == gsn_third_attack && IS_SET(ch->act,ACT_WARRIOR))
	    skill = 40 * log10(ch->tot_level);

	else if (sn == gsn_hand_to_hand)
	    skill = 40 + 19 * log10(ch->tot_level)/2;

 	else if (sn == gsn_bash && IS_SET(ch->off_flags,OFF_BASH))
	    skill = 40 + 19 * log10(ch->tot_level)/1.5;

	else if (sn == gsn_disarm
	     &&  (IS_SET(ch->off_flags,OFF_DISARM)
	     ||   IS_SET(ch->act,ACT_WARRIOR)
	     ||	  IS_SET(ch->act,ACT_THIEF)))
	    skill = 40 + 19*log10(ch->tot_level)/1.5;

	else if (sn == gsn_berserk && IS_SET(ch->off_flags,OFF_BERSERK))
	    skill = 19*log10(ch->tot_level)/1.5;

	else if (sn == gsn_kick)
	    skill = 40 + 19*log10(ch->tot_level)/1.5;

	else if (sn == gsn_backstab && IS_SET(ch->act,ACT_THIEF))
	    skill = 40 + 19*log10(ch->tot_level)/2;

  	else if (sn == gsn_rescue)
	    skill = 40 + 19*log10(ch->tot_level)/2;

	else if (sn == gsn_recall)
	    skill = 40 + 19*log10(ch->tot_level)/2;

	else if (sn == gsn_sword
	||  sn == gsn_dagger
	||  sn == gsn_spear
	||  sn == gsn_mace
	||  sn == gsn_axe
	||  sn == gsn_flail
	||  sn == gsn_whip
	||  sn == gsn_stake
	||  sn == gsn_polearm
	||  sn == gsn_quarterstaff)
	    skill = 40 + 19*log10(ch->tot_level);

	else
	   skill = 0;
    }

    if (ch->daze > 0)
    {
	if (skill_table[sn].spell_fun != spell_null)
	    skill /= 2;
	else
	    skill = 2 * skill / 3;
    }

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK]  > 10)
	skill = 9 * skill / 10;

    return URANGE(0,skill,100);
}


/* for returning weapon information */
int get_weapon_sn(CHAR_DATA *ch)
{
    OBJ_DATA *wield;
    int sn;

    wield = get_eq_char(ch, WEAR_WIELD);
    if (wield == NULL || wield->item_type != ITEM_WEAPON)
        sn = gsn_hand_to_hand;

    else switch (wield->value[0])
    {
        default :                  sn = -1; 	       		break;
        case(WEAPON_SWORD):        sn = gsn_sword;         	break;
        case(WEAPON_EXOTIC):       sn = gsn_exotic;         	break;
        case(WEAPON_DAGGER):       sn = gsn_dagger;        	break;
        case(WEAPON_SPEAR):        sn = gsn_spear;         	break;
        case(WEAPON_MACE):         sn = gsn_mace;          	break;
        case(WEAPON_AXE):          sn = gsn_axe;           	break;
        case(WEAPON_FLAIL):        sn = gsn_flail;         	break;
        case(WEAPON_WHIP):         sn = gsn_whip;          	break;
        case(WEAPON_POLEARM):      sn = gsn_polearm;       	break;
        case(WEAPON_STAKE):        sn = gsn_stake;       	break;
	case(WEAPON_QUARTERSTAFF): sn = gsn_quarterstaff; 	break;
   }

   return sn;
}

int get_objweapon_sn(OBJ_DATA *obj)
{
    int sn;

    if (!obj || obj->item_type != ITEM_WEAPON)
        sn = gsn_hand_to_hand;

    else switch (obj->value[0])
    {
        default :                  sn = -1; 	       		break;
        case(WEAPON_SWORD):        sn = gsn_sword;         	break;
        case(WEAPON_EXOTIC):       sn = gsn_exotic;         	break;
        case(WEAPON_DAGGER):       sn = gsn_dagger;        	break;
        case(WEAPON_SPEAR):        sn = gsn_spear;         	break;
        case(WEAPON_MACE):         sn = gsn_mace;          	break;
        case(WEAPON_AXE):          sn = gsn_axe;           	break;
        case(WEAPON_FLAIL):        sn = gsn_flail;         	break;
        case(WEAPON_WHIP):         sn = gsn_whip;          	break;
        case(WEAPON_POLEARM):      sn = gsn_polearm;       	break;
        case(WEAPON_STAKE):        sn = gsn_stake;       	break;
	case(WEAPON_QUARTERSTAFF): sn = gsn_quarterstaff; 	break;
   }

   return sn;
}


int get_weapon_skill(CHAR_DATA *ch, int sn)
{
    int skill;

     /* -1 is default */
    if (IS_NPC(ch))
    {
	if (sn == -1)
	    skill = 3 * ch->level;
	else if (sn == gsn_hand_to_hand)
	    skill = 40 + 2 * ch->level;
	else
	    skill = 40 + 5 * ch->level / 2;
    }

    else
    {
	if (sn == -1)
	    skill = ch->level;
	else
	    skill = ch->pcdata->learned[sn];
    }

    return URANGE(0,skill,100);
}


/* used to de-screw characters */
void reset_char(CHAR_DATA *ch)
{
    int loc;
    int mod;
    int stat;
    OBJ_DATA *obj;
    AFFECT_DATA *af;
    int i;

    if (IS_NPC(ch))
	return;

    if (ch->pcdata->perm_hit == 0
    ||	ch->pcdata->perm_mana == 0
    ||  ch->pcdata->perm_move == 0
    ||	ch->pcdata->last_level == 0)
    {
	/* do a FULL reset */
	for (loc = 0; loc < MAX_WEAR; loc++)
	{
	    obj = get_eq_char(ch,loc);
	    if (obj == NULL)
		continue;
	    for (af = obj->affected; af != NULL; af = af->next)
	    {
		mod = af->modifier;
		switch(af->location)
		{
		    case APPLY_SEX:     ch->sex         -= mod;         break;
		    case APPLY_MANA:    ch->max_mana    -= mod;         break;
		    case APPLY_HIT:     ch->max_hit     -= mod;         break;
		    case APPLY_MOVE:    ch->max_move    -= mod;         break;
		}
	    }
	}

	/* now reset the permanent stats */
	ch->pcdata->perm_hit 	= ch->max_hit;
	ch->pcdata->perm_mana 	= ch->max_mana;
	ch->pcdata->perm_move	= ch->max_move;
	ch->pcdata->last_level	= ch->played/3600;

	if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
	{
	    if (ch->sex > 0 && ch->sex < 3)
		ch->pcdata->true_sex	= ch->sex;
	    else
		ch->pcdata->true_sex 	= 0;
	}

    }

	if(!IS_NPC(ch)) memset(ch->pcdata->mod_learned,0,sizeof(ch->pcdata->mod_learned));

    /* now restore the character to his/her true condition */
    for (stat = 0; stat < MAX_STATS; stat++)
		set_mod_stat(ch, stat, 0);

    if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
	ch->pcdata->true_sex = 0;
    ch->sex		= ch->pcdata->true_sex;
    ch->max_hit 	= ch->pcdata->perm_hit;
    ch->max_mana	= ch->pcdata->perm_mana;
    ch->max_move	= ch->pcdata->perm_move;

    for (i = 0; i < 4; i++)
	ch->armour[i]	= 100;

    ch->hitroll		= 0;
    ch->damroll		= 0;
    ch->saving_throw	= 0;

    /* now start adding back the effects */
    for (loc = 0; loc < MAX_WEAR; loc++)
    {
	obj = get_eq_char(ch,loc);
	if (obj == NULL)
	    continue;
	for (i = 0; i < 4; i++)
	    ch->armour[i] -= apply_ac(obj, loc, i);
	for (af = obj->affected; af != NULL; af = af->next)
	{
	    mod = af->modifier;
	    switch(af->location)
	    {
		case APPLY_STR:         add_mod_stat(ch,STAT_STR,mod); break;
		case APPLY_DEX:         add_mod_stat(ch,STAT_DEX,mod); break;
		case APPLY_INT:         add_mod_stat(ch,STAT_INT,mod); break;
		case APPLY_WIS:         add_mod_stat(ch,STAT_WIS,mod); break;
		case APPLY_CON:         add_mod_stat(ch,STAT_CON,mod); break;

		case APPLY_SEX:         ch->sex                 += mod; break;
		case APPLY_MANA:        ch->max_mana            += mod; break;
		case APPLY_HIT:         ch->max_hit             += mod; break;
		case APPLY_MOVE:        ch->max_move            += mod; break;

		case APPLY_AC:
					for (i = 0; i < 4; i ++)
					    ch->armour[i] += mod;
					break;
		case APPLY_HITROLL:     ch->hitroll             += mod; break;
		case APPLY_DAMROLL:     ch->damroll             += mod; break;
		default:
			if(!IS_NPC(ch) && af->location >= APPLY_SKILL && af->location < APPLY_SKILL_MAX) {
				ch->pcdata->mod_learned[af->location - APPLY_SKILL] += mod;
				break;
			}
			break;
	    }
	}
    }

    /* now add back spell effects */
    for (af = ch->affected; af != NULL; af = af->next)
    {
	mod = af->modifier;
	switch(af->location)
	{
	    case APPLY_STR:         add_mod_stat(ch,STAT_STR,mod); break;
	    case APPLY_DEX:         add_mod_stat(ch,STAT_DEX,mod); break;
	    case APPLY_INT:         add_mod_stat(ch,STAT_INT,mod); break;
	    case APPLY_WIS:         add_mod_stat(ch,STAT_WIS,mod); break;
	    case APPLY_CON:         add_mod_stat(ch,STAT_CON,mod); break;

	    case APPLY_SEX:         ch->sex                 += mod; break;
	    case APPLY_MANA:        ch->max_mana            += mod; break;
	    case APPLY_HIT:         ch->max_hit             += mod; break;
	    case APPLY_MOVE:        ch->max_move            += mod; break;

	    case APPLY_AC:
				    for (i = 0; i < 4; i ++)
					ch->armour[i] += mod;
				    break;
	    case APPLY_HITROLL:     ch->hitroll             += mod; break;
	    case APPLY_DAMROLL:     ch->damroll             += mod; break;
		default:
			if(!IS_NPC(ch) && af->location >= APPLY_SKILL && af->location < APPLY_SKILL_MAX) {
				ch->pcdata->mod_learned[af->location - APPLY_SKILL] += mod;
				break;
			}
			break;


	}
    }

    /* make sure sex is RIGHT!!!! */
    if (ch->sex < 0 || ch->sex > 2)
	ch->sex = ch->pcdata->true_sex;
}


/*
 * Retrieve a character's trusted level for permission checking.
 */
int get_trust(CHAR_DATA *ch)
{
    if (ch == NULL)
	return 0;


    if (IS_NPC(ch))
	return (URANGE(1,ch->tot_level,149));
    else
	return ch->tot_level;
}


int get_age(CHAR_DATA *ch)
{
    return 17 + (ch->played + (int) (current_time - ch->logon)) / (4*72000);
}

void set_mod_stat(CHAR_DATA *ch, int stat, int value)
{
	ch->mod_stat[stat] = value;
	ch->dirty_stat[stat] = TRUE;
}

void add_mod_stat(CHAR_DATA *ch, int stat, int adjust)
{
	ch->mod_stat[stat] += adjust;
	ch->dirty_stat[stat] = TRUE;
}

void set_perm_stat(CHAR_DATA *ch, int stat, int value)
{
	ch->perm_stat[stat] = value;
	ch->dirty_stat[stat] = TRUE;
}

void add_perm_stat(CHAR_DATA *ch, int stat, int adjust)
{
	ch->perm_stat[stat] += adjust;
	ch->dirty_stat[stat] = TRUE;
}

void set_perm_stat_range(CHAR_DATA *ch, int stat, int value, int mn, int mx)
{
	ch->perm_stat[stat] = URANGE(mn, value, mx);
	ch->dirty_stat[stat] = TRUE;
}

/* command for retrieving stats */
int get_curr_stat(CHAR_DATA *ch, int stat)
{
    int max, cur;

    if (ch->dirty_stat[stat]) {

		cur = ch->perm_stat[stat] + ch->mod_stat[stat];
		max = !IS_NPC(ch) ? pc_race_table[ch->race].max_stats[stat] : 25;
		if (cur > max) {
			float t = exp(-0.0075*(cur-max));
			cur = max + (int)((50-max)*(1-t)/(1+t)+0.5);
		}

		ch->cur_stat[stat] = UMAX(3,cur);
		ch->dirty_stat[stat] = FALSE;
	}

	return ch->cur_stat[stat];
}


/* command for returning max training score */
int get_max_train(CHAR_DATA *ch, int stat)
{
    int max;

    if (IS_NPC(ch) || ch->level > LEVEL_IMMORTAL)
	return 25;

    max = pc_race_table[ch->race].max_stats[stat];
/* nrrk! disabling this, too! -- Areo
    if ((stat == STAT_INT && ch->pcdata->class_mage != -1)
    ||  (stat == STAT_WIS && ch->pcdata->class_cleric != -1)
    ||  (stat == STAT_DEX && ch->pcdata->class_thief != -1)
    ||  (stat == STAT_STR && ch->pcdata->class_warrior != -1))
    {*/
	if ((ch->race == race_lookup("human")) || (ch->race == race_lookup("avatar")))
	   max += 1;
/*	else
	   max += 2;
    }*/


    return UMIN(max,25);
}


/*
 * Retrieve a character's carry capacity.
 */
int can_carry_n(CHAR_DATA *ch)
{
    if (!IS_NPC(ch) && ch->level >= LEVEL_IMMORTAL)
	return 1000;

    if (IS_NPC(ch) && IS_SET(ch->act, ACT_PET))
	return 0;

    return MAX_WEAR + get_curr_stat(ch,STAT_DEX) + ch->tot_level/2;
}


/*
 * Retrieve a character's carry capacity.
 */
int can_carry_w(CHAR_DATA *ch)
{
    int weight;

    if (!IS_NPC(ch) && ch->level >= LEVEL_IMMORTAL)
	return 10000000;

    if (IS_NPC(ch) && IS_SET(ch->act, ACT_PET))
	return 0;

    weight = str_app[get_curr_stat(ch,STAT_STR)].carry + ch->tot_level/5;
    if (IS_REMORT(ch))
	weight += weight/4;

    return weight;
}


/*
 * See if a string is one of the names of an object.
 */
bool is_name (char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
    char *list, *string;

    /* fix crash on NULL namelist */
    if (namelist == NULL || namelist[0] == '\0')
    	return FALSE;

    /* fixed to prevent is_name on "" returning TRUE */
    if (str[0] == '\0')
	return FALSE;

    string = str;
    /* we need ALL parts of string to match part of namelist */
    for (; ;)  /* start parsing string */
    {
	str = one_argument(str,part);

	if (part[0] == '\0')
	    return TRUE;

	/* check to see if this is part of namelist */
	list = namelist;
	for (; ;)  /* start parsing namelist */
	{
	    list = one_argument(list,name);
	    if (name[0] == '\0')  /* this name was not found */
		return FALSE;

	    if (!str_prefix(string,name))
		return TRUE; /* full pattern match */

	    if (!str_prefix(part,name))
		break;
	}
    }
}



bool is_exact_name(char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH];

    if (namelist == NULL)
	return FALSE;

    for (; ;)
    {
	namelist = one_argument(namelist, name);
	if (name[0] == '\0')
	    return FALSE;
	if (!str_cmp(str, name))
	    return TRUE;
    }
}

void affect_fix_char(CHAR_DATA *ch)
{
	AFFECT_DATA *paf;
	OBJ_DATA *obj;

	// Reset flags
	ch->affected_by = ch->affected_by_perm;
	ch->affected_by2 = ch->affected_by2_perm;
	ch->imm_flags = ch->imm_flags_perm;
	ch->res_flags = ch->res_flags_perm;
	ch->vuln_flags = ch->vuln_flags_perm;

	ch->deathsight_vision = ( IS_SET(ch->affected_by2_perm, AFF2_DEATHSIGHT) ) ? ch->tot_level : 0;

	// Iterate through affects on character
	for(paf = ch->affected; paf; paf = paf->next)
	{
		switch(paf->where)
		{
		    case TO_AFFECTS:
				SET_BIT(ch->affected_by, paf->bitvector);
				SET_BIT(ch->affected_by2, paf->bitvector2);

				if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
					ch->deathsight_vision = paf->level;

				break;
		    case TO_IMMUNE:
				SET_BIT(ch->imm_flags,paf->bitvector);
				break;
		    case TO_RESIST:
				SET_BIT(ch->res_flags,paf->bitvector);
				break;
		    case TO_VULN:
				SET_BIT(ch->vuln_flags,paf->bitvector);
				break;
		}
	}

	// Iterate through all worn objects
	for(obj = ch->carrying; obj; obj = obj->next_content)
	{
		if( !obj->locker && obj->wear_loc != WEAR_NONE )
		{
			for(paf = obj->affected; paf; paf = paf->next)
			{
				switch (paf->where)
				{
					case TO_AFFECTS:
						SET_BIT(ch->affected_by, paf->bitvector);
						SET_BIT(ch->affected_by2, paf->bitvector2);

						if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
							ch->deathsight_vision = paf->level;

						break;
					case TO_IMMUNE:
						SET_BIT(ch->imm_flags,paf->bitvector);
						break;
					case TO_RESIST:
						SET_BIT(ch->res_flags,paf->bitvector);
						break;
					case TO_VULN:
						SET_BIT(ch->vuln_flags,paf->bitvector);
						break;
				}
			}
		}
	}

}


/*
 * Apply or remove an affect to a character.
 */
void affect_modify(CHAR_DATA *ch, AFFECT_DATA *paf, bool fAdd)
{
    OBJ_DATA *wield;
    int mod;
    int i;

    mod = paf->modifier;

    /* add an affect to a char */
    if (fAdd)
    {
		switch (paf->where)
		{
			case TO_AFFECTS:
				SET_BIT(ch->affected_by, paf->bitvector);
				SET_BIT(ch->affected_by2, paf->bitvector2);

				if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
					ch->deathsight_vision = paf->level;

			break;
			case TO_IMMUNE:
				SET_BIT(ch->imm_flags,paf->bitvector);
			break;
			case TO_RESIST:
				SET_BIT(ch->res_flags,paf->bitvector);
			break;
			case TO_VULN:
				SET_BIT(ch->vuln_flags,paf->bitvector);
			break;
		}
    }
    else /* take an affect from a char */
    {
        switch (paf->where)
	{
	    case TO_AFFECTS:
		MERGE_BIT(ch->affected_by, ch->affected_by_perm, paf->bitvector);
		MERGE_BIT(ch->affected_by2, ch->affected_by2_perm, paf->bitvector2);
		break;
	    case TO_IMMUNE:
			MERGE_BIT(ch->imm_flags,ch->imm_flags_perm,paf->bitvector);
		break;
	    case TO_RESIST:
			MERGE_BIT(ch->res_flags,ch->res_flags_perm,paf->bitvector);
		break;
	    case TO_VULN:
		MERGE_BIT(ch->vuln_flags,ch->vuln_flags_perm,paf->bitvector);
		break;
	}
		mod = -mod; /* reverse modifier */
    }

    /* cancel out affects */
    switch (paf->location)
    {
	case APPLY_NONE:						break;
	case APPLY_STR:           add_mod_stat(ch,STAT_STR,mod);	break;
	case APPLY_DEX:           add_mod_stat(ch,STAT_DEX,mod);	break;
	case APPLY_INT:           add_mod_stat(ch,STAT_INT,mod);	break;
	case APPLY_WIS:           add_mod_stat(ch,STAT_WIS,mod);	break;
	case APPLY_CON:           add_mod_stat(ch,STAT_CON,mod);	break;
	case APPLY_SEX:           ch->sex			+= mod;	break;
	case APPLY_MANA:          ch->max_mana			+= mod;	break;
	case APPLY_HIT:
	    // Make sure it doesn't make them DEAD
	    if( fAdd && ((ch->max_hit + mod) < 1))
			mod = 0;
		else if( !fAdd && ((ch->max_hit - mod) < 1))
			mod = 0;

	    ch->max_hit						+= mod;	break;
	case APPLY_MOVE:          ch->max_move			+= mod;	break;
	case APPLY_GOLD:						break;
	case APPLY_AC:
	    for (i = 0; i < 4; i ++)
		ch->armour[i] += mod;
	    break;
	case APPLY_HITROLL:       ch->hitroll			+= mod;	break;
	case APPLY_DAMROLL:       ch->damroll			+= mod;	break;
	case APPLY_SPELL_AFFECT:  					break;
	default:
		if(!IS_NPC(ch) && paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
			ch->pcdata->mod_learned[paf->location - APPLY_SKILL] += mod;
			break;
		}
	    bug("Affect_modify: unknown location %d.", paf->location);
	    return;
    }

    /*
     * If it takes away strength they drop their weapon.
     */
    if (!IS_NPC(ch)
    && (wield = get_eq_char(ch, WEAR_WIELD)) != NULL
    && !IS_AFFECTED(ch, AFF_DEATH_GRIP)
    && get_obj_weight(wield) > (str_app[get_curr_stat(ch,STAT_STR)].wield*10))
    {
	static int depth;

	if (depth == 0)
	{
	    depth++;
	    act("You drop $p.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR);
	    act("$n drops $p.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_ROOM);
	    obj_from_char(wield);
	    obj_to_room(wield, ch->in_room);
	    depth--;
	}
    }
}


/* find an effect in an affect list */
AFFECT_DATA *affect_find(AFFECT_DATA *paf, int sn)
{
    AFFECT_DATA *paf_find;

    for (paf_find = paf; paf_find != NULL; paf_find = paf_find->next)
    {
        if (paf_find->type == sn)
	return paf_find;
    }

    return NULL;
}


/* fix object affects when removing one */
void affect_check(CHAR_DATA *ch, int where, long vector, long vector2)
{
    AFFECT_DATA *paf;
    OBJ_DATA *obj;

    if (where == TO_OBJECT || where == TO_OBJECT2 || where == TO_OBJECT3 || where == TO_OBJECT4 || where == TO_WEAPON)
	return;

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
		if (paf->where == where && paf->bitvector == vector)
		{
			switch (where)
			{
				case TO_AFFECTS:
				SET_BIT(ch->affected_by,vector);
				break;
				case TO_IMMUNE:
				SET_BIT(ch->imm_flags,vector);
				break;
				case TO_RESIST:
				SET_BIT(ch->res_flags,vector);
				break;
				case TO_VULN:
				SET_BIT(ch->vuln_flags,vector);
				break;
			}
			return;
		}
        else if (paf->where == where && paf->bitvector2 == vector2)
		{
			switch (where)
			{
			case TO_AFFECTS:
				SET_BIT(ch->affected_by2,vector2);
				break;
			}
			return;
		}
	}

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
		if (obj->wear_loc == -1)
			continue;

		for (paf = obj->affected; paf != NULL; paf = paf->next)
		{
			if (paf->where == where && paf->bitvector == vector)
			{
				switch (where)
				{
				case TO_AFFECTS:
					SET_BIT(ch->affected_by,vector);
					break;
				case TO_IMMUNE:
					SET_BIT(ch->imm_flags,vector);
					break;
				case TO_RESIST:
					SET_BIT(ch->res_flags,vector);
					break;
				case TO_VULN:
					SET_BIT(ch->vuln_flags,vector);
				}
				return;
			}
			else if (paf->where == where && paf->bitvector2 == vector2)
			{
				switch (where)
				{
				case TO_AFFECTS:
					SET_BIT(ch->affected_by2,vector2);
					break;
				}
				return;
			}
		}
	}
}


/*
 * Give an affect to a char.
 */
void affect_to_char(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_new;

    paf_new = new_affect();

    *paf_new		= *paf;
    VALIDATE(paf_new);	/* in case we missed it when we set up paf */

    paf_new->next	= ch->affected;
    ch->affected	= paf_new;

    affect_modify(ch, paf_new, TRUE);
}


/* give an affect to an object */
void affect_to_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_new;
    int wear_loc;

    paf_new = new_affect();

    *paf_new		= *paf;

    VALIDATE(paf);	/* in case we missed it when we set up paf */
    paf_new->next	= obj->affected;
    obj->affected	= paf_new;

    if ((wear_loc = obj->wear_loc) != WEAR_NONE && obj->carried_by != NULL)
	affect_modify(obj->carried_by, paf_new, TRUE);

    /* apply any affect vectors to the object's extra_flags */
    if (paf->bitvector)
    {
        switch (paf->where)
        {
	    case TO_OBJECT:
			SET_BIT(obj->extra_flags,paf->bitvector);
			break;
	    case TO_OBJECT2:
			SET_BIT(obj->extra2_flags,paf->bitvector);
			break;
	    case TO_OBJECT3:
			SET_BIT(obj->extra3_flags,paf->bitvector);
			break;
	    case TO_OBJECT4:
			SET_BIT(obj->extra4_flags,paf->bitvector);
			break;
	    case TO_WEAPON:
			if (obj->item_type == ITEM_WEAPON)
			    SET_BIT(obj->value[4],paf->bitvector);
		break;
        }
    }
}

/* give an affect to an object */
void catalyst_to_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_new, *cat;

    for(cat = obj->catalyst; cat; cat = cat->next) {
	    if(cat->type == paf->type && cat->level == paf->level) {
		    if(cat->modifier < 0 || paf->modifier < 0)
			    cat->duration = cat->modifier = -1;
		    else
			    cat->duration += (paf->level * paf->modifier);
		    return;
	    }
    }

    paf_new = new_affect();

    *paf_new		= *paf;

    VALIDATE(paf);	/* in case we missed it when we set up paf */
    paf_new->next	= obj->catalyst;
    obj->catalyst	= paf_new;
    paf_new->duration = (paf_new->modifier > 0) ? (paf_new->level * paf_new->modifier) : -1;
}


/*
 * Remove an affect from a char.
 */
void affect_remove(CHAR_DATA *ch, AFFECT_DATA *paf)
{
	if (ch->affected == NULL)
	{
		bug("Affect_remove: no affect.", 0);
		return;
	}

	affect_modify(ch, paf, FALSE);

	if (paf == ch->affected)
		ch->affected = paf->next;
	else
	{
		AFFECT_DATA *prev;

		for (prev = ch->affected; prev != NULL; prev = prev->next)
		{
			if (prev->next == paf)
			{
				prev->next = paf->next;
				break;
			}
		}

		if (prev == NULL)
		{
			bug("Affect_remove: cannot find paf.", 0);
			return;
		}
	}

	free_affect(paf);
	affect_fix_char(ch);
	return;
}

bool affect_removeall_obj(OBJ_DATA *obj)
{
	AFFECT_DATA *paf, *paf_next;
	bool is_worn = (obj->carried_by != NULL) && (obj->wear_loc != -1);

	for(paf = obj->affected; paf != NULL; paf = paf_next) {
		paf_next = paf->next;

		// If worn, remove this affect from the character, JIC
		if(is_worn) affect_modify(obj->carried_by, paf, FALSE);

		if (paf->bitvector)
			switch(paf->where) {
			case TO_OBJECT:
				MERGE_BIT(obj->extra_flags,obj->extra_flags_perm,paf->bitvector);
				break;
			case TO_OBJECT2:
				MERGE_BIT(obj->extra2_flags,obj->extra2_flags_perm,paf->bitvector);
				break;
			case TO_OBJECT3:
				MERGE_BIT(obj->extra3_flags,obj->extra3_flags_perm,paf->bitvector);
				break;
			case TO_OBJECT4:
				MERGE_BIT(obj->extra4_flags,obj->extra4_flags_perm,paf->bitvector);
				break;
			case TO_WEAPON:
				if (obj->item_type == ITEM_WEAPON)
					MERGE_BIT(obj->value[4],obj->weapon_flags_perm,paf->bitvector);
				break;
			}

		free_affect(paf);
	}

	obj->affected = NULL;

	if(is_worn) affect_fix_char(obj->carried_by);

	return is_worn;
}


bool affect_remove_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
	bool reset_ch = FALSE;

	if (obj->affected == NULL)
	{
		bug("Affect_remove_object: no affects on object.", 0);
		return FALSE;
	}

	if (obj->carried_by != NULL && obj->wear_loc != -1)
	{
		affect_modify(obj->carried_by, paf, FALSE);
		reset_ch = TRUE;
	}

	/* remove flags from the object if needed */
	if (paf->bitvector)
	{
		switch(paf->where)
		{
		case TO_OBJECT:
			MERGE_BIT(obj->extra_flags,obj->extra_flags_perm,paf->bitvector);
			break;
		case TO_OBJECT2:
			MERGE_BIT(obj->extra2_flags,obj->extra2_flags_perm,paf->bitvector);
			break;
		case TO_OBJECT3:
			MERGE_BIT(obj->extra3_flags,obj->extra3_flags_perm,paf->bitvector);
			break;
		case TO_OBJECT4:
			MERGE_BIT(obj->extra4_flags,obj->extra4_flags_perm,paf->bitvector);
			break;
		case TO_WEAPON:
			if (obj->item_type == ITEM_WEAPON)
				MERGE_BIT(obj->value[4],obj->weapon_flags_perm,paf->bitvector);
			break;
		}
	}

	if (paf == obj->affected)
		obj->affected = paf->next;
	else
	{
		AFFECT_DATA *prev;

		for (prev = obj->affected; prev != NULL; prev = prev->next)
		{
			if (prev->next == paf)
			{
				prev->next = paf->next;
				break;
			}
		}

		if (prev == NULL)
		{
			bug("Affect_remove_object: cannot find paf.", 0);
			return reset_ch;
		}
	}

	free_affect(paf);

	if (obj->carried_by != NULL && obj->wear_loc != -1)
	{
		affect_fix_char(obj->carried_by);
	}

	return reset_ch;
}


/*
 * Strip all affects of a given sn.
 */
void affect_strip(CHAR_DATA *ch, int sn)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	if (paf->type == sn)
	    affect_remove(ch, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_name(CHAR_DATA *ch, char *name)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	if (paf->custom_name == name)
	    affect_remove(ch, paf);
    }
}

void affect_stripall_wearloc(CHAR_DATA *ch, int wear_loc)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    if( wear_loc == WEAR_NONE ) return;

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
		paf_next = paf->next;
		if (paf->slot == wear_loc)
	    	affect_remove(ch, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_obj(OBJ_DATA *obj, int sn)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	if (paf->type == sn)
	    affect_remove_obj(obj, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_name_obj(OBJ_DATA *obj, char *name)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	if (paf->custom_name == name)
	    affect_remove_obj(obj, paf);
    }
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected(CHAR_DATA *ch, int sn)
{
    AFFECT_DATA *paf;

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
	if (!paf->custom_name && paf->type == sn)
	    return TRUE;
    }

    return FALSE;
}

/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_name(CHAR_DATA *ch, char *name)
{
    AFFECT_DATA *paf;

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
	if (paf->custom_name && paf->custom_name == name)
	    return TRUE;
    }

    return FALSE;
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_obj(OBJ_DATA *obj, int sn)
{
    AFFECT_DATA *paf;

    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
	if (!paf->custom_name && paf->type == sn)
	    return TRUE;
    }

    return FALSE;
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_name_obj(OBJ_DATA *obj, char *name)
{
    AFFECT_DATA *paf;

    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
	if (paf->custom_name && paf->custom_name == name)
	    return TRUE;
    }

    return FALSE;
}


/*
 * Add or enhance an affect.
 */
void affect_join(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

	if(paf->custom_name) {
		for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (paf_old->custom_name && paf_old->custom_name == paf->custom_name) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove(ch, paf_old);
				break;
			}
		}
	} else {
		for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (!paf_old->custom_name && paf_old->type == paf->type) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove(ch, paf_old);
				break;
			}
		}
	}


    affect_to_char(ch, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_full(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

	if(paf->custom_name) {
		for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (paf_old->custom_name && paf_old->custom_name == paf->custom_name &&
				paf_old->location == paf->location &&
				paf_old->bitvector == paf->bitvector &&
				paf_old->bitvector2 == paf->bitvector2) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove(ch, paf_old);
				break;
			}
		}
	} else {
		for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (!paf_old->custom_name && paf_old->type == paf->type &&
				paf_old->location == paf->location &&
				paf_old->bitvector == paf->bitvector &&
				paf_old->bitvector2 == paf->bitvector2) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove(ch, paf_old);
				break;
			}
		}
	}


    affect_to_char(ch, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

	if(paf->custom_name) {
		for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (paf_old->custom_name && paf_old->custom_name == paf->custom_name) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove_obj(obj, paf_old);
				break;
			}
		}
	} else {
		for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (!paf_old->custom_name && paf_old->type == paf->type) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove_obj(obj, paf_old);
				break;
			}
		}
	}

    affect_to_obj(obj, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_full_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

	if(paf->custom_name) {
		for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (paf_old->custom_name && paf_old->custom_name == paf->custom_name &&
				paf_old->location == paf->location &&
				paf_old->bitvector == paf->bitvector &&
				paf_old->bitvector2 == paf->bitvector2) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove_obj(obj, paf_old);
				break;
			}
		}
	} else {
		for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
			if (!paf_old->custom_name && paf_old->type == paf->type &&
				paf_old->location == paf->location &&
				paf_old->bitvector == paf->bitvector &&
				paf_old->bitvector2 == paf->bitvector2) {
				paf->level = (paf->level + paf_old->level) / 2;
				paf->duration += paf_old->duration;
				paf->modifier += paf_old->modifier;
				affect_remove_obj(obj, paf_old);
				break;
			}
		}
	}

    affect_to_obj(obj, paf);
    return;
}


/*
 * Move a char out of a room.
 */
void char_from_room(CHAR_DATA *ch)
{
    OBJ_DATA *obj, *obj_next;

    if (ch->in_room == NULL)
    {
	bug("Char_from_room: NULL.", 0);
	return;
    }

	if( IS_VALID(ch->in_room->instance_section) && IS_VALID(ch->in_room->instance_section->instance) )
	{
		INSTANCE *instance = ch->in_room->instance_section->instance;
		DUNGEON *dungeon = instance->dungeon;
		if( !IS_NPC(ch) || !IS_SET(ch->act2, ACT2_INSTANCE_MOB) )
		{

			if( IS_VALID(dungeon) )
			{
				if( !IS_NPC(ch) )
					list_remlink(dungeon->players, ch);
				list_remlink(dungeon->mobiles, ch);
			}

			if( !IS_NPC(ch) )
				list_remlink(instance->players, ch);
			list_remlink(instance->mobiles, ch);
		}
		else if( IS_BOSS(ch) )
		{
			if( IS_VALID(dungeon) )
				list_remlink(dungeon->bosses, ch);
			list_remlink(instance->bosses, ch);
		}
	}
	else
	{
	    if (!IS_NPC(ch))
	    {
			--ch->in_room->area->nplayer;
			if (ch->in_wilds)
			{
				--ch->in_wilds->nplayer;
			}
		}

    }


    if (ch->in_room->chat_room != NULL)
	ch->in_room->chat_room->curr_people--;

    if (MOUNTED(ch) && MOUNTED(ch)->in_room == ch->in_room
    && MOUNTED(ch)->in_room != NULL)
        char_from_room(MOUNTED(ch));

    if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
    &&   obj->item_type == ITEM_LIGHT
    &&   obj->value[2] != 0
    &&   ch->in_room->light > 0)
	--ch->in_room->light;

    if (ch == ch->in_room->people)
	ch->in_room->people = ch->next_in_room;
    else
    {
		CHAR_DATA *prev;

		for (prev = ch->in_room->people; prev; prev = prev->next_in_room)
		{
			if (prev->next_in_room == ch)
			{
			prev->next_in_room = ch->next_in_room;
			break;
			}
		}

		if (prev == NULL)
			bug("Char_from_room: ch not found.", 0);
    }

    list_remlink(ch->in_room->lpeople, ch);
    list_remlink(ch->in_room->lentity, ch);

    if (ch->in_wilds)
    {
        if (!ch->in_room->people && !ch->in_room->contents)
            destroy_wilds_vroom(ch->in_room);
    }

    ch->in_room = NULL;
    ch->in_wilds = NULL;        /* Vizz - wilds */
    ch->at_wilds_x = 0;
    ch->at_wilds_y = 0;
    ch->next_in_room = NULL;
    ch->on = NULL;                /* sanity check! */

    // Make sure mail is only done in post offices

    if (ch->mail != NULL)
    {
	for (obj = ch->mail->objects; obj != NULL; obj = obj_next) {
	    obj_next = obj->next_content;

	    obj_from_mail(obj);
	    obj_to_char(obj, ch);
	}

	free_mail(ch->mail);
	ch->mail = NULL;
	send_to_char("Mail order cancelled.\n\r", ch);
    }

    return;
}

/*
 * Move a char into a room.
 */
void char_to_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
    OBJ_DATA *obj;

    if (pRoomIndex == NULL)
    {
	ROOM_INDEX_DATA *room;

	bug("Char_to_room: destination room NULL.", 0);

	if ((room = get_room_index(ROOM_VNUM_TEMPLE)) != NULL)
	    char_to_room(ch,room);

	return;
    }

    if (MOUNTED(ch) && MOUNTED(ch)->in_room == ch->in_room
    && MOUNTED(ch)->in_room == NULL)
	char_to_room(MOUNTED(ch), pRoomIndex);

    ch->in_room		= pRoomIndex;

// VIZZWILDS - Check room's wilds pointer
    if (pRoomIndex->wilds)
        ch->in_wilds = pRoomIndex->wilds;

    ch->next_in_room	= pRoomIndex->people;
    pRoomIndex->people	= ch;

    list_addlink(pRoomIndex->lpeople, ch);
    list_addlink(pRoomIndex->lentity, ch);

    // Prevent catastrophes
    if (ch->next_in_room == ch)
    {
        plogf("[SERIOUS!] char_to_room(): error! char %s (vnum %ld)'s next_in_room is itself!\n",
	     IS_NPC(ch) ? ch->short_descr : ch->name,
	     IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        extract_char(ch, FALSE);
	return;
    }

    if (ch->next == ch)
    {
        plogf("[SERIOUS!] char_to_room(): error! char %s (vnum %ld)'s next is itself!\n",
	     IS_NPC(ch) ? ch->short_descr : ch->name,
	     IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        extract_char(ch, FALSE);
	return;
    }

    if (ch->in_room->chat_room != NULL)
	ch->in_room->chat_room->curr_people++;

    if (!str_cmp(ch->in_room->area->name, "Elysium")
    && !IS_SOCIAL(ch))
	SET_BIT(ch->comm, COMM_SOCIAL);

    if (str_cmp(ch->in_room->area->name, "Elysium")
    && IS_SOCIAL(ch))
	REMOVE_BIT(ch->comm, COMM_SOCIAL);

	if( IS_VALID(pRoomIndex->instance_section) && IS_VALID(pRoomIndex->instance_section->instance) )
	{
		INSTANCE *instance = pRoomIndex->instance_section->instance;
		DUNGEON *dungeon = instance->dungeon;

		if( !IS_NPC(ch) || !IS_SET(ch->act2, ACT2_INSTANCE_MOB) )
		{
			if( IS_VALID(dungeon) )
			{
				if( !IS_NPC(ch) )
					list_appendlink(dungeon->players, ch);
				list_appendlink(dungeon->mobiles, ch);
			}

			if( !IS_NPC(ch) )
				list_appendlink(instance->players, ch);
			list_appendlink(instance->mobiles, ch);
		}
		else if( IS_BOSS(ch) )
		{
			if( IS_VALID(dungeon) )
				list_appendlink(dungeon->bosses, ch);
			list_appendlink(instance->bosses, ch);
		}
	}
	else
	{
	    if (!IS_NPC(ch))
	    {
			if (ch->in_room->area->empty)
			{
				ch->in_room->area->empty = FALSE;
				ch->in_room->area->age = 0;
			}

			++ch->in_room->area->nplayer;
		// VIZZWILDS - Check char's wilds pointer
			if (ch->in_wilds)
			{
				//plogf("handler.c, char_to_room(): %s is entering a wilds area.", ch->name);

				if (ch->in_wilds->empty)
				{
					ch->in_wilds->empty = FALSE;
					ch->in_wilds->age = 0;
				}

				++ch->in_wilds->nplayer;
			}
		}
    }

    if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
    &&   obj->item_type == ITEM_LIGHT
    &&   obj->value[2] != 0)
	++ch->in_room->light;

    // Spread plague
    if (IS_AFFECTED(ch,AFF_PLAGUE))
    {
        AFFECT_DATA *af, plague;
        bool has_plague_af = FALSE;	// @@@NIB : 20070127 : handle special cases
        CHAR_DATA *vch;

        for (af = ch->affected; af != NULL; af = af->next)
        {
            if (af->type == gsn_plague)
                break;
	    // @@@NIB : 20070127 : handle special cases
	    //	So far, only toxic fumes does 'plague' too
            if (af->type == gsn_toxic_fumes)
                has_plague_af = TRUE;
        }

        if (af == NULL)
        {
            if(!has_plague_af) REMOVE_BIT(ch->affected_by,AFF_PLAGUE);
            return;
        }

        if (af->level == 1)
            return;

		plague.where		= TO_AFFECTS;
		plague.group		= AFFGROUP_BIOLOGICAL;
        plague.type 		= gsn_plague;
        plague.level 		= af->level - 1;
        plague.duration 	= number_range(1,2 * plague.level);
        plague.location		= APPLY_STR;
        plague.modifier 	= -5;
        plague.bitvector 	= AFF_PLAGUE;
        plague.bitvector2	= 0;
        plague.slot			= WEAR_NONE;
        plague.custom_name = NULL;


        for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
        {
            if (!saves_spell(plague.level - 2,vch,DAM_DISEASE)
	    &&  !IS_IMMORTAL(vch) &&
            	!IS_AFFECTED(vch,AFF_PLAGUE) && number_bits(6) == 0)
            {
            	send_to_char("You feel hot and feverish.\n\r",vch);
            	act("$n shivers and looks very ill.",vch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
            	affect_join(vch,&plague);
            }
        }
    }

    if (ch->quest != NULL)
	check_quest_travel_room(ch, pRoomIndex, true);

    return;
}


/*
 * Give an obj to a locker.
 */
void obj_to_locker(OBJ_DATA *obj, CHAR_DATA *ch)
{
    obj->next_content    = ch->locker;
    ch->locker           = obj;
    obj->carried_by      = ch;
    obj->in_room         = NULL;
    obj->in_obj          = NULL;
    obj->locker	 	 = TRUE;

    obj->pIndexData->lockered++;

    list_addlink(ch->llocker, obj);

}


/*
 * Give an obj to a char.
 */
void obj_to_char(OBJ_DATA *obj, CHAR_DATA *ch)
{
    obj->next_content	 = ch->carrying;
    ch->carrying	 = obj;
    obj->carried_by	 = ch;
    obj->in_room	 = ch->in_room;
    obj->in_obj		 = NULL;
    ch->carry_number	+= get_obj_number(obj);
    ch->carry_weight	+= get_obj_weight(obj);

    if (IS_SET(obj->extra_flags, ITEM_HIDDEN))
        REMOVE_BIT(obj->extra_flags, ITEM_HIDDEN);

    // convert money obj into gold/silver
    if (obj->item_type == ITEM_MONEY)
    {
	ch->silver += obj->value[0];
	ch->gold += obj->value[1];

	// AUTOSPLIT
	if (IS_SET(ch->act,PLR_AUTOSPLIT))
	{
	    int members;
	    CHAR_DATA *gch;
	    char buffer[MAX_STRING_LENGTH];

	    members = 0;
	    for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	    {
		if (gch->pcdata != NULL && is_same_group(gch, ch))
		    members++;
	    }

	    if (members > 1 && (obj->value[0] > 1 || obj->value[1]))
	    {
		sprintf(buffer,"%d %d",obj->value[0],obj->value[1]);
		do_function(ch, &do_split, buffer);
	    }
	}

	extract_obj(obj);
	return;
    }

    list_addlink(ch->lcarrying, obj);

    if (!IS_NPC(ch))
        check_quest_retrieve_obj(ch, obj, true);

    obj->pIndexData->carried++;

    if (objRepop == TRUE)
    {
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
	objRepop = FALSE;
    }
}


/*
 * Count objs in a characters locker
 */
int count_char_locker(CHAR_DATA *ch)
{
    OBJ_DATA *prev;
    int counter;

    counter = 0;

    if (ch == NULL)
    {
	bug("handler.c, count_char_locker: NULL ch.", 0);
	return -1;
    }

    for (prev = ch->locker; prev != NULL; prev = prev->next_content)
        counter++;

    return counter;
}


/*
 * Take an obj from its characters locker.
 */
void obj_from_locker(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if ((ch = obj->carried_by) == NULL)
    {
	bug("Obj_from_char: null ch.", 0);
	return;
    }

    if (ch->locker == obj)
        ch->locker = obj->next_content;
    else
    {
        OBJ_DATA *prev;

        for (prev = ch->locker; prev != NULL; prev = prev->next_content)
        {
            if (prev->next_content == obj)
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if (prev == NULL)
            bug("locker get: obj not in list.", 0);
    }

    --obj->pIndexData->lockered;

    obj->in_room         = NULL;
    obj->carried_by      = NULL;
    obj->next_content    = NULL;
    obj->locker	 	 = FALSE;

    list_remlink(ch->llocker, obj);
}


/*
 * Take an obj from its character.
 */
void obj_from_char(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if ((ch = obj->carried_by) == NULL)
    {
	bug("Obj_from_char: null ch.", 0);
	return;
    }

    /* Unequip it first */
    if (obj->wear_loc != WEAR_NONE)
	unequip_char(ch, obj, FALSE);

    if (ch->carrying == obj)
	ch->carrying = obj->next_content;
    else
    {
	OBJ_DATA *prev;

	for (prev = ch->carrying; prev != NULL; prev = prev->next_content)
	{
	    if (prev->next_content == obj)
	    {
		prev->next_content = obj->next_content;
		break;
	    }
	}

	if (prev == NULL && !obj->locker)
	    bug("Obj_from_char: obj not in list.", 0);
    }

    --obj->pIndexData->carried;

    REMOVE_BIT(obj->extra_flags, ITEM_INVENTORY);
    obj->carried_by	 = NULL;
    obj->next_content	 = NULL;
    ch->carry_number	-= get_obj_number(obj);
    ch->carry_weight	-= get_obj_weight(obj);
    list_remlink(ch->lcarrying, obj);
}


/*
 * Find the ac value of an obj, including position effect.
 */
int apply_ac(OBJ_DATA *obj, int iWear, int type)
{
    if (obj->item_type != ITEM_ARMOUR)
	return 0;

    switch (iWear)
    {
	case WEAR_BODY:		return 3 * obj->value[type];
	case WEAR_HEAD:		return 3 * obj->value[type];
	case WEAR_LEGS:		return 2 * obj->value[type];
	case WEAR_FEET:		return obj->value[type];
	case WEAR_HANDS: 	return 2 * obj->value[type];
	case WEAR_ARMS:		return 2 * obj->value[type];
	case WEAR_SHIELD: 	return 3 * obj->value[type];
	case WEAR_NECK_1: 	return 3 * obj->value[type];
	case WEAR_NECK_2: 	return 3 * obj->value[type];
	case WEAR_ABOUT: 	return obj->value[type];
	case WEAR_WAIST: 	return obj->value[type];
	case WEAR_FINGER_R: 	return obj->value[type];
	case WEAR_FINGER_L: 	return obj->value[type];
	case WEAR_WRIST_L: 	return obj->value[type];
	case WEAR_WRIST_R: 	return obj->value[type];
	case WEAR_HOLD:		return 2 * obj->value[type];
    }

    return 0;
}


/*
 * Find a piece of eq on a character.
 */
OBJ_DATA *get_eq_char(CHAR_DATA *ch, int iWear)
{
    OBJ_DATA *obj;

    if (ch == NULL)
	return NULL;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->wear_loc == iWear)
	    return obj;
    }

    return NULL;
}


/*
 * Equip a char with an obj.
 */
void equip_char(CHAR_DATA *ch, OBJ_DATA *obj, int iWear)
{
    AFFECT_DATA *paf;
    SPELL_DATA *spell;
    int i;
    //char buf[MSL];

    if (get_eq_char(ch, iWear) != NULL)
    {
	bug("Equip_char: already equipped (%d).", iWear);
	return;
    }

    // This is generally already handled in wear_obj, but just to be safe, doing the all_remort check here as well.
    /*
    if (!IS_IMMORTAL(ch) && ch->tot_level < obj->level)
	return;
    */

    if (!IS_IMMORTAL(ch) && !IS_NPC(ch)) {
        /* If the object is not a mortal object
        -or- is higher object level and the item is not flagged all_remort or the char is not remort */
        if ((obj->level > LEVEL_HERO) ||
        ((ch->tot_level < obj->level) && !(IS_SET(obj->extra2_flags, ITEM_ALL_REMORT) && IS_REMORT(ch)))) {
            return;
        }
    }


    if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL)    && IS_EVIL(ch)   )
    ||   (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD)    && IS_GOOD(ch)   )
    ||   (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch)))
    {
	act("You are zapped by $p and drop it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n is zapped by $p and drops it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        REMOVE_BIT(obj->extra2_flags, ITEM_KEPT);

	obj_from_char(obj);
	obj_to_room(obj, ch->in_room);
	return;
    }

    obj->wear_loc	 = iWear;
    list_addlink(ch->lworn, obj);

    /* Wear trigger */
    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WEAR, NULL);

	// Concealed items do nothing to the wearer's stats and affects.
	if(wear_params[iWear][WEAR_PARAM_AFFECTS]) {
	    /* apply armour class */
	    for (i = 0; i < 4; i++)
		ch->armour[i] -= apply_ac(obj, iWear, i);


	    /* put obj's affects on the character */
	    for (paf = obj->affected; paf != NULL; paf = paf->next) {
			paf->slot = iWear;
			affect_modify(ch, paf, TRUE);
		}

	    /* set light in room if it's a light */
	    if (obj->item_type == ITEM_LIGHT
	    &&   ch->in_room != NULL)
	    {
		// hack to fix current lights with 0 light remaining
		if (obj->value[2] == 0)
		    obj->value[2] = 10;

		++ch->in_room->light;
	    }

	    if (obj->item_type != ITEM_WAND
	    &&  obj->item_type != ITEM_STAFF
	    &&  obj->item_type != ITEM_SCROLL
	    &&  obj->item_type != ITEM_POTION
	    &&  obj->item_type != ITEM_TATTOO
	    &&  obj->item_type != ITEM_PILL)
	    for (spell = obj->spells; spell != NULL; spell = spell->next)
	    {
			for (paf = ch->affected; paf != NULL; paf = paf->next)
			{
				if (paf->type == spell->sn)
				break;
			}

			if (paf != NULL && paf->level >= spell->level)
				continue;

			affect_strip(ch, spell->sn);
			obj_cast_spell(spell->sn, spell->level + MAGIC_WEAR_SPELL, ch, ch, obj);
	    }
    }

}


/*
 * Unequip a char with an obj.
 */
int unequip_char(CHAR_DATA *ch, OBJ_DATA *obj, bool show)
{
    AFFECT_DATA *paf = NULL;
    AFFECT_DATA *af;
    int i, loc = obj->wear_loc;
    int level = 0;
    int found_loc = WEAR_NONE;
    SPELL_DATA *spell, *spell_tmp;
    OBJ_DATA *obj_tmp;
    bool found;		// @@@NIB : 20070128

    if (obj->wear_loc == WEAR_NONE)
    {
	bug("Unequip_char: already unequipped.", 0);
	return FALSE;	// @@@NIB : 20070128
    }

    obj->wear_loc = WEAR_NONE;
    list_remlink(ch->lworn, obj);

	// If the item was concealed, don't handle any object affects.
	if(wear_params[loc][WEAR_PARAM_AFFECTS]) {
	    for (i = 0; i < 4; i++)
		ch->armour[i] += apply_ac(obj, loc, i);

	    for (paf = obj->affected; paf != NULL; paf = paf->next)
	    {
			affect_modify(ch, paf, FALSE);
//			affect_check(ch, paf->where, paf->bitvector, paf->bitvector2);
	    }

	    if (obj->item_type == ITEM_LIGHT &&
	    	obj->value[2] != 0 && ch->in_room != NULL &&
	    	ch->in_room->light > 0)
			--ch->in_room->light;

	    // Remove spells
	    if (obj->item_type != ITEM_WAND
	    &&  obj->item_type != ITEM_STAFF
	    &&  obj->item_type != ITEM_SCROLL
	    &&  obj->item_type != ITEM_POTION
	    &&  obj->item_type != ITEM_TATTOO
	    &&  obj->item_type != ITEM_PILL)
	    for (spell = obj->spells; spell != NULL; spell = spell->next)
	    {
			int spell_level = spell->level;

			// Find the first affect that matches this spell and is derived from the object
			for (af = ch->affected; af != NULL; af = af->next)
			{
				if (af->type == spell->sn && af->slot == loc)
					break;
			}

			if( !af ) {
				// This spell was not applied by this object
				continue;
			}

			// @@@NIB : 20070128 : this entire block did not account for the
			//	possibility of multiple spells active for the object.
			//	Once it found *one* spell, it returned...
			//	This also bypassed the remove trigger
			found = FALSE;


			// If there's another obj with the same spell put that one on
			for (obj_tmp = ch->carrying; obj_tmp; obj_tmp = obj_tmp->next_content)
			{
				if( obj_tmp->wear_loc != WEAR_NONE && obj != obj_tmp ) {
					for (spell_tmp = obj_tmp->spells; spell_tmp != NULL; spell_tmp = spell_tmp->next) {
						if (spell_tmp->sn == spell->sn && spell_tmp->level > level ) {
							level = spell_tmp->level;	// Keep the maximum
							found_loc = obj_tmp->wear_loc;
							found = TRUE;
						}
					}
				}
			}

			if(!found) {
				// No other worn object had this spell available

				if( show ) {
					if (skill_table[spell->sn].msg_off) {
						send_to_char(skill_table[spell->sn].msg_off, ch);
						send_to_char("\n\r", ch);
					}
				}

				affect_strip(ch, spell->sn);
			} else if( level > spell_level ) {
				level -= spell_level;		// Get the difference

				// Update all affects to the current maximum and its slot
				for(; af; af = af->next ) {
					af->level += level;
					af->slot = found_loc;
				}
			}
			// @@@NIB : 20070128
	    }

	    affect_stripall_wearloc(ch, loc);	// Remove all affects tied to this wear slot

	    affect_fix_char(ch);

    }

    /* Remove trigger */
    return (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REMOVE, NULL));
}


/*
 * Count occurrences of an obj in a list.
 */
int count_obj_list(OBJ_INDEX_DATA *pObjIndex, OBJ_DATA *list)
{
    OBJ_DATA *obj;
    int nMatch;

    nMatch = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
	if (obj->pIndexData == pObjIndex)
	    nMatch++;
    }

    return nMatch;
}


/*
 * Move an obj out of a room.
 */
void obj_from_room(OBJ_DATA *obj)
{
	ROOM_INDEX_DATA *in_room;
	CHAR_DATA *ch;
	char buf[MAX_STRING_LENGTH];

	if ((in_room = obj->in_room) == NULL)
	{
		bug("obj_from_room: NULL.", 0);
		return;
	}

	for (ch = in_room->people; ch != NULL; ch = ch->next_in_room)
		if (ch->on == obj)
			ch->on = NULL;

	list_remlink(in_room->lcontents, obj);
	list_remlink(in_room->lentity, obj);

	if (obj == in_room->contents)
		in_room->contents = obj->next_content;
	else
	{
		OBJ_DATA *prev;

		for (prev = in_room->contents; prev; prev = prev->next_content)
		{
			if (prev->next_content == obj)
			{
				prev->next_content = obj->next_content;
				break;
			}
		}

		if (prev == NULL)
		{
			sprintf(buf, "Obj_from_room: obj not found.");
			bug(buf, 0);
			return;
		}
	}

	if( IS_VALID(obj->in_room->instance_section) && IS_VALID(obj->in_room->instance_section->instance) )
	{
		if( !IS_SET(obj->extra3_flags, ITEM_INSTANCE_OBJ) )
		{
			INSTANCE *instance = obj->in_room->instance_section->instance;
			DUNGEON *dungeon = instance->dungeon;

			if( IS_VALID(dungeon) )
			{
				list_appendlink(dungeon->objects, obj);
			}
		}
	}


	if (obj->in_wilds)
	{
		if (!obj->in_room->people && !obj->in_room->contents)
			destroy_wilds_vroom(obj->in_room);

		obj->in_wilds->loaded_objs--;
	}

	--obj->pIndexData->inrooms;

	obj->in_wilds     = NULL;
	obj->in_room      = NULL;
	obj->next_content = NULL;
}


/*
 * Move an obj into a room.
 */
void obj_to_room(OBJ_DATA *obj, ROOM_INDEX_DATA *pRoomIndex)
{
    obj->next_content		= pRoomIndex->contents;
    pRoomIndex->contents	= obj;
    obj->in_room		= pRoomIndex;
    obj->carried_by		= NULL;
    obj->in_obj			= NULL;

    list_addlink(pRoomIndex->lcontents, obj);
    list_addlink(pRoomIndex->lentity, obj);

		// Vroom
    if( pRoomIndex->wilds )
    {
		pRoomIndex->wilds->loaded_objs++;
		obj->in_wilds = pRoomIndex->wilds;
	}
	else if( IS_VALID(pRoomIndex->instance_section) && IS_VALID(pRoomIndex->instance_section->instance) )
	{
		if( !IS_SET(obj->extra3_flags, ITEM_INSTANCE_OBJ) )
		{
			INSTANCE *instance = pRoomIndex->instance_section->instance;
			DUNGEON *dungeon = instance->dungeon;

			if( IS_VALID(dungeon) )
			{
				list_appendlink(dungeon->objects, obj);
			}
		}
	}

    obj->pIndexData->inrooms++;

    if (objRepop == TRUE)
    {
		p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
		objRepop = FALSE;
    }
}

void obj_to_vroom(OBJ_DATA *obj, WILDS_DATA *pWilds, int x, int y)
{
    ROOM_INDEX_DATA *pWildsRoom = NULL;

/*
    // Find the obj content list at these coors in the obj matrix
    pVObj = pWilds->obj_matrix + (sizeof(OBJ_DATA *) * ((y * pWilds->map_size_x) + x));

    // Add the obj to the obj content list just like if the room existed
    if (pVObj == NULL)
        pVObj = obj;
    else
    {
        obj->next_content = pVObj;
        pVObj = obj;
    }
*/
    // Check if vroom is actually loaded right now
    if ((pWildsRoom = get_wilds_vroom(pWilds, x, y)) != NULL)
    {
        obj->in_wilds               = pWilds;
        obj->in_room                = pWildsRoom;
        obj->carried_by             = NULL;
        obj->in_obj                 = NULL;
        obj->next_content           = pWildsRoom->contents;
        pWildsRoom->contents        = obj;
        list_addlink(pWildsRoom->lcontents, obj);
        list_addlink(pWildsRoom->lentity, obj);
    }
    else
    {
        obj->in_room                = NULL;
        obj->carried_by             = NULL;
        obj->in_obj                 = NULL;
    }

    obj->in_wilds               = pWilds;
    obj->x                      = x;
    obj->y                      = y;
    pWilds->loaded_objs++;

    obj->pIndexData->inrooms++;


    if (objRepop == TRUE)
    {
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
	objRepop = FALSE;
    }
}


/*
 * Move an object into an object.
 */
void obj_to_obj(OBJ_DATA *obj, OBJ_DATA *obj_to)
{
    if (obj_to->carried_by != NULL)
		obj_to->carried_by->carry_weight -= get_obj_weight(obj_to);

    obj->next_content		= obj_to->contains;
    obj_to->contains		= obj;
    obj->in_obj			= obj_to;
    obj->in_room		= obj_to->in_room;
    obj->carried_by		= NULL;

    if (obj_to->carried_by != NULL)
		obj_to->carried_by->carry_weight += get_obj_weight(obj_to);

	if(obj->clone_rooms || obj->nest_clones > 0)
		obj_set_nest_clones(obj_to,true);

    obj->pIndexData->incontainer++;

}


/*
 * Move an object out of an object.
 */
void obj_from_obj(OBJ_DATA *obj)
{
    OBJ_DATA *obj_from;

    if ((obj_from = obj->in_obj) == NULL)
    {
		bug("Obj_from_obj: null obj_from.", 0);
		return;
    }

    if (obj_from->carried_by != NULL)
		obj_from->carried_by->carry_weight -= get_obj_weight(obj_from);

    if (obj == obj_from->contains)
		obj_from->contains = obj->next_content;
    else
    {
		OBJ_DATA *prev;

		for (prev = obj_from->contains; prev; prev = prev->next_content)
		{
		    if (prev->next_content == obj)
		    {
				prev->next_content = obj->next_content;
				break;
		    }
		}

		if (prev == NULL)
		{
		    bug("Obj_from_obj: obj not found.", 0);
		    return;
		}
    }

    --obj->pIndexData->incontainer;

    obj->in_room      = NULL;
    obj->next_content = NULL;
    obj->in_obj       = NULL;

    if (obj_from->carried_by != NULL)
		obj_from->carried_by->carry_weight += get_obj_weight(obj_from);

	if(obj->clone_rooms || obj->nest_clones > 0)
		obj_set_nest_clones(obj_from,false);

    return;
}


/*
 * Extract a chat room from the world.
 */
void extract_chat_room(CHAT_ROOM_DATA *chat)
{
    CHAT_ROOM_DATA *temp_chat;
    CHAT_ROOM_DATA *prev_chat = NULL;

    if (chat == NULL)
    {
	bug("Tried to extract null chat.", 0);
	return;
    }

    for (temp_chat = chat_room_list; temp_chat != NULL; temp_chat = temp_chat->next)
    {
        if (!str_cmp(chat->name, temp_chat->name)) break;

	prev_chat = temp_chat;
    }

    if (temp_chat == NULL)
    {
	bug("Couldn't extract chat as chat was NULL.", 0);
	return;
    }

    if (prev_chat == NULL)
    {
	chat_room_list = chat->next;
    }
    else
    {
	prev_chat->next = chat->next;
    }

    if (temp_chat == NULL)
    {
	bug("Extract_chat: chat not found.", 0);
	return;
    }

    free_chat_room(chat);
    return;
}


/*
 * Extract a church from the world.
 */
void extract_church(CHURCH_DATA *church)
{
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *temp_church;
    CHURCH_PLAYER_DATA *member;

    if (church == NULL)
    {
	bug("Tried to extract null church.", 0);
	return;
    }

    for (temp_church = church_list; temp_church != NULL; temp_church = temp_church->next)
        if (!str_cmp(church->name, temp_church->name))
	    break;

    if (temp_church == NULL)
    {
	bug("Couldn't extract church as church was NULL.", 0);
	return;
    }

    while((member = church->people) != NULL)
        remove_member(member);

    sprintf(buf, "{Y[%s has been disbanded.]{x\n\r", church->name);
    gecho(buf);

    if (church == church_list)
    {
	church_list = church->next;
    }
    else
    {
        for (temp_church = church_list; temp_church != NULL; temp_church = temp_church->next) {
	    if (temp_church->next == church)
	    {
		temp_church->next = church->next;
	        break;
	    }
	}

	if (temp_church == NULL)
	{
	    bug("Extract_church: church not found.", 0);
	    return;
	}
    }

    list_remlink(list_churches, church);

    free_church(church);
    return;
}


/*
 * Extract an obj from the world.
 */
void extract_obj(OBJ_DATA *obj)
{
    //char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj_content;
    OBJ_DATA *obj_next;
    ROOM_INDEX_DATA *clone, *next_clone;
    ITERATOR it;
    INSTANCE *instance;


    if (obj == NULL)
    {
	bug("Tried to extract null object.", 0);
	return;
    }

    if(obj->progs) {
	    SET_BIT(obj->progs->entity_flags,PROG_NODESTRUCT);
	    if(obj->progs->script_ref > 0) {
			obj->progs->extract_when_done = TRUE;
			return;
		}
	    p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    // Deal with all clone rooms here while the object's location still exists
    for(clone = obj->clone_rooms; clone; clone = next_clone) {
	    next_clone = clone->next_clone;
	    p_percent_trigger(NULL, NULL, clone, NULL, NULL, NULL, NULL, obj, NULL, TRIG_CLONE_EXTRACT, NULL);
	    room_from_environment(clone);
    }

    /* if its a scroll make sure that they can't finish reciting the scroll
       unless they still have it. this happens if for example you are fighting
       and it burns up from a flaming weapon. */
    if (obj->item_type == ITEM_SCROLL)
    {
	CHAR_DATA *ch;

	if (obj->carried_by != NULL && obj->carried_by->in_room != NULL)
	{
	    for (ch = obj->carried_by->in_room->people; ch != NULL; ch = ch->next_in_room)
	    {
		if (ch->recite_scroll == obj)
		    ch->recite_scroll = NULL;
	    }
	}
    }

	iterator_start(&it, loaded_instances);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		if( IS_VALID(instance->object) && instance->object == obj)
		{
			extract_instance(instance);
		}
	}

	iterator_stop(&it);

    if (obj->carried_by != NULL)
	obj_from_char(obj);
    else if (obj->in_obj != NULL)
	obj_from_obj(obj);
    else if (obj->in_room != NULL)
	obj_from_room(obj);
    else if (obj->in_mail != NULL)
	obj_from_mail(obj);
    else if (obj->locker == TRUE)
	obj_from_locker(obj);

    /* extract obj's contents */
    for (obj_content = obj->contains; obj_content; obj_content = obj_next)
    {
	obj_next = obj_content->next_content;
	extract_obj(obj_content);
    }

	list_remlink(loaded_objects, obj);

	// Clear the most recent corpse data on the player owner
    if( (obj->item_type == ITEM_CORPSE_PC) && !IS_NULLSTR(obj->owner) )
    {
		CHAR_DATA *victim = get_char_world(NULL, obj->owner);

		if( victim && !IS_NPC(victim) )
			victim->pcdata->corpse = NULL;
	}

	extract_special_key(obj);

    --obj->pIndexData->count;
    free_obj(obj);
}


/*
 * Extract a char from the world.
 */
void extract_char(CHAR_DATA *ch, bool fPull)
{
	ROOM_INDEX_DATA *clone, *next_clone;
    CHAR_DATA *wch;
    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];
    ITERATOR it;

    if (ch->in_room == NULL)
    {
        sprintf(buf, "extract_char: %s had null ch->in_room",
	         IS_NPC(ch) ? ch->short_descr : ch->name);
	return;
    }

    if(IS_NPC(ch) && ch->progs) {
	    SET_BIT(ch->progs->entity_flags,PROG_NODESTRUCT);
		if(ch->progs->script_ref > 0) {
			ch->progs->extract_when_done = TRUE;
			ch->progs->extract_fPull = ch->progs->extract_fPull || fPull;
			return;
		}
	    p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    nuke_pets(ch);
    ch->pet = NULL; /* just in case */

    if (fPull)
	die_follower(ch);

    stop_fighting(ch, TRUE);
    stop_holdup(ch);

    // Deal with all clone rooms here while the object's location still exists
    for(clone = ch->clone_rooms; clone; clone = next_clone) {
	    next_clone = clone->next_clone;
	    p_percent_trigger(NULL, NULL, clone, NULL, NULL, ch, NULL, NULL, NULL, TRIG_CLONE_EXTRACT, NULL);
	    room_from_environment(clone);
    }

    char_from_room(ch);

    if (ch->belongs_to_ship != NULL && ch->belongs_to_ship->owner != NULL)
	ch->belongs_to_ship->owner = NULL;

    if (ch->belongs_to_ship != NULL
    && ch->belongs_to_ship->npc_ship != NULL
    && ch->belongs_to_ship->npc_ship->captain == ch)
	ch->belongs_to_ship->npc_ship->captain = NULL;

    if (IS_NPC(ch) && ch->hunting != NULL)
    	stop_hunt(ch, TRUE);

    if (ch->belongs_to_ship != NULL)
	char_from_crew(ch);

    if (!fPull)
    {
        ROOM_INDEX_DATA *death_room;

        death_room = get_room_index(ROOM_VNUM_DEATH);
        if (IS_DEMON(ch))
        {
            int range;
            range = number_range(0, 7000);
            death_room = get_room_index(200050 + range);
            ch->hit = number_range(1, ch->max_hit);
            ch->mana = number_range(1, ch->max_mana);
            ch->move = number_range(1, ch->max_move);
        }
        else
	if (IS_ANGEL(ch))
        {
            int range;
            range = number_range(0, 7000);
            death_room = get_room_index(300050 + range);
            ch->hit = number_range(1, ch->max_hit);
            ch->mana = number_range(1, ch->max_mana);
            ch->move = number_range(1, ch->max_move);
        }
	char_to_room(ch, death_room);
	return;
    }

	if(ch->mount) {
		ch->mount->rider = NULL;
		ch->mount->riding = FALSE;
	}

	if(ch->rider) {
		ch->rider->mount = NULL;
		ch->rider->riding = FALSE;
	}
	ch->mount = NULL;
	ch->rider = NULL;
	ch->riding = FALSE;

    if (IS_NPC(ch))
    {
	GQ_MOB_DATA *gq_mob;

	if(!IS_SET(ch->act, ACT_ANIMATED))
		--ch->pIndexData->count;

	/* for NPCs and global quests. */
	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
	    if (ch->pIndexData->vnum == gq_mob->vnum)
	    {
		--gq_mob->count;
	    }
	}
    }
    else
    {
	nuke_pets(ch);
	ch->pet = NULL;
    }

    if (ch->desc != NULL && ch->desc->original != NULL)
    {
	do_function(ch, &do_return, "");
	ch->desc = NULL;
    }

    /* modify reply targets and mprog targets */
    iterator_start(&it, loaded_chars);
    while((wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		if (wch->reply == ch)
			wch->reply = NULL;

		if (IS_NPC(wch) && wch->progs->target == ch)
			wch->progs->target = NULL;
    }
    iterator_stop(&it);
    list_remlink(loaded_chars, ch);
    // Temporarily disabled for reconnect crash.
    //list_remlink(loaded_players, ch);


    if (ch->desc != NULL)
		ch->desc->character = NULL;

    // Go through the world and if anyones hunting char, stop them
    // same for challenge to prevent challenge someone/quit crash bug
    for (d = descriptor_list; d != NULL; d = d->next) {
		if (d->character != NULL &&
			d->character->hunting != NULL &&
			d->character->hunting == ch) {
			send_to_char("You sense your target has vanished somewhere.\n\r", d->character);
			d->character->hunting = NULL;
		}

		if (d->character != NULL &&
			d->character->challenged != NULL &&
			d->character->challenged == ch) {
			send_to_char("Your challenger has left the game.\n\r", d->character);
			d->character->challenged = NULL;
		}
    }

    detach_instances_player(ch);
    detach_dungeons_player(ch);
    detach_ships_player(ch);

    free_char(ch);
    return;
}

void extract_token(TOKEN_DATA *token)
{
    if(token->progs) {
	    SET_BIT(token->progs->entity_flags,PROG_NODESTRUCT);
		if(token->progs->script_ref > 0) {
			token->progs->extract_when_done = TRUE;
			return;
		}
	    p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    if(token->player)
    {
    	token_from_char(token);
    }
    else if (token->object)
    {
    	token_from_obj(token);
	}
	else if (token->room)
	{
    	token_from_room(token);
	}

	free_token(token);
	return;
}


/*
 * Is the cart being pulled by anyone in the room?
 */
CHAR_DATA *get_cart_pulled(OBJ_DATA *obj)
{
    if (obj == NULL)
    {
	bug("In get_cart_pulled the obj was null.", 0);
	return NULL;
    }

    return obj->pulled_by;
}


/*
 * Find a char in the room.
 */
CHAR_DATA *get_char_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *rch;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    if (!str_cmp(arg, "self")
    || !str_cmp(arg, "me")
    || (ch != NULL && !str_cmp(arg, ch->name)))
	return ch;

    if (ch && room)
    {
	bug("get_char_room received multiple types (ch/room)", 0);
	//return NULL;
    }

    if (ch)
	rch = ch->in_room->people;
    else
	rch = room->people;

    for (; rch != NULL; rch = rch->next_in_room)
    {
	if ((ch && !can_see(ch, rch)) || !is_name(arg, rch->name))
	    continue;
	if (++count == number)
	    return rch;
    }

    return NULL;
}


/*
 * Find a char in the world, even if they are invisible etc.
 */
CHAR_DATA *find_char_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *wch;
    int number;
    int count;
    ITERATOR it;

    if (ch && (wch = get_char_room(ch, NULL, argument)) != NULL)
	return wch;

    number = number_argument(argument, arg);
    count  = 0;
    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		if (wch->in_room == NULL || !is_name(arg, wch->name))
	    	continue;
		if (++count == number)
			break;
    }
    iterator_stop(&it);

    return wch;
}


/*
 * Find a char in the world.
 */
CHAR_DATA *get_char_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *wch;
    int number;
    int count;
    ITERATOR it;

    if (ch && (wch = get_char_room(ch, NULL, argument)) != NULL)
		return wch;

    number = number_argument(argument, arg);
    count  = 0;
    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		if (wch->in_room == NULL ||
			(ch && !can_see(ch, wch)) ||
			!is_name(arg, wch->name))
	    	continue;
		if (++count == number)
	    	break;
    }
    iterator_stop(&it);

    return wch;
}


/*
 * Find a char in the world by its pIndexData
 */
CHAR_DATA *get_char_world_index(CHAR_DATA *ch, MOB_INDEX_DATA *pMobIndex)
{
	CHAR_DATA *wch;
	ITERATOR it;

	iterator_start(&it, loaded_chars);
	while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (wch->in_room == NULL ||
			(ch && !can_see(ch, wch)) ||
			!IS_NPC(wch) ||
			wch->pIndexData != pMobIndex)
			continue;

		break;
    }
    iterator_stop(&it);

    return wch;
}

/*
 * Find an object in the world by its pIndexData
 */
OBJ_DATA *get_obj_world_index(CHAR_DATA *ch, OBJ_INDEX_DATA *pObjIndex, bool all)
{
	OBJ_DATA *wobj;
	ITERATOR it;

	iterator_start(&it, loaded_objects);
	while(( wobj = (OBJ_DATA *)iterator_nextdata(&it)))
	{
		if ((!all && (wobj->locker || wobj->in_mail)) ||
			(ch && !can_see_obj(ch, wobj)) ||
			wobj->pIndexData != pObjIndex)
			continue;

		break;
    }
    iterator_stop(&it);

    return wobj;
}


/*
 * Find some object with a given index data.
 * Used by area-reset 'P' command.
 */
OBJ_DATA *get_obj_type(OBJ_INDEX_DATA *pObjIndex, ROOM_INDEX_DATA *pRoom)
{
    register OBJ_DATA *obj;
    ITERATOR it;

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
		if (obj->pIndexData == pObjIndex && obj->in_room == pRoom)
		    break;
    }
    iterator_stop(&it);

    return obj;
}


/*
 * Find an obj in a list.
 */
OBJ_DATA *get_obj_list(CHAR_DATA *ch, char *argument, OBJ_DATA *list)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
	if (can_see_obj(ch, obj) && (
			(((obj->ship != NULL) && (!str_cmp(arg, obj->ship->ship_name)))) ||
			is_name(arg, obj->name)))
	{
	    if (++count == number)
		return obj;
	}
    }

    return NULL;
}

/*
 * Find an obj in a list.
 */
OBJ_DATA *get_obj_list_number(CHAR_DATA *ch, char *argument, int *nth, OBJ_DATA *list)
{
    OBJ_DATA *obj;
    int number = *nth;

    for (obj = list; obj != NULL; obj = obj->next_content)
    {
		if (can_see_obj(ch, obj) && (
				(((obj->ship != NULL) && (!str_cmp(argument, obj->ship->ship_name)))) ||
				is_name(argument, obj->name)))
		{
			if (--number < 1)
				return obj;
		}
    }

    // Return last total for chaining together lookups
	*nth = number;
    return NULL;
}


/*
 * Find an obj in player's locker.
 */
OBJ_DATA *get_obj_locker(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    for (obj = ch->locker; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc == WEAR_NONE
        &&   is_name(arg, obj->name))
        {
            if (++count == number)
                            return obj;
        }
    }

    return NULL;
}


/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *get_obj_carry(CHAR_DATA *ch, char *argument, CHAR_DATA *viewer)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc == WEAR_NONE
             && (viewer ? can_see_obj(viewer, obj) : TRUE)
             && is_name(arg, obj->name))
        {
            if (++count == number)
                            return obj;
        }
    }

    return NULL;
}

/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *get_obj_carry_number(CHAR_DATA *ch, char *argument, int *nth, CHAR_DATA *viewer)
{
    OBJ_DATA *obj;
    int number = *nth;

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
		if (obj->wear_loc == WEAR_NONE &&
			(viewer ? can_see_obj(viewer, obj) : TRUE) &&
			is_name(argument, obj->name))
		{
			if (--number < 1) {
				return obj;
			}
		}
	}

    // Return last total for chaining together lookups
	*nth = number;
    return NULL;
}

/*
 * Find an obj in player's inventory by vnum.
 */
OBJ_DATA *get_obj_vnum_carry(CHAR_DATA *ch, long vnum, CHAR_DATA *viewer)
{
    OBJ_DATA *obj;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc == WEAR_NONE
        &&   (viewer ? can_see_obj(viewer, obj) : TRUE)
        &&   obj->pIndexData->vnum == vnum)
        {
	    return obj;
        }
    }

    return NULL;
}

/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *get_obj_wear(CHAR_DATA *ch, char *argument, bool character)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc != WEAR_NONE
                &&  (character ? can_see_obj(ch, obj) : TRUE)
        &&   is_name(arg, obj->name))
        {
            if (++count == number)
                return obj;
        }
    }

    return NULL;
}


/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *get_obj_wear_number(CHAR_DATA *ch, char *argument, int *nth, bool character)
{
    OBJ_DATA *obj;
    int number = *nth;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc != WEAR_NONE &&
        	(character ? can_see_obj(ch, obj) : TRUE) &&
        	is_name(argument, obj->name))
        {
            if (--number < 1)
                return obj;
        }
    }

    // Return last total for chaining together lookups
    *nth = number;
    return NULL;
}


/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;
    int count;

    if (ch && room)
    {
		bug("get_obj_here received a ch and a room",0);
		return NULL;
    }

    number = number_argument(argument, arg);
    count = 0;

    if (ch)
    {
		obj = get_obj_list_number(ch, arg, &number, ch->in_room->contents);
		if (obj != NULL)
		    return obj;

		if ((obj = get_obj_carry_number(ch, arg, &number, ch)) != NULL)
		    return obj;

		if ((obj = get_obj_wear_number(ch, arg, &number, TRUE)) != NULL)
		    return obj;
    }
    else
    {
		for (obj = room->contents; obj; obj = obj->next_content)
		{
			if (!is_name(arg, obj->name))
				continue;
			if (++count == number)
				return obj;
		}
    }

    return NULL;
}

/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here_number(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument, int *nth)
{
    OBJ_DATA *obj;

    if (ch && room)
    {
		bug("get_obj_here received a ch and a room",0);
		return NULL;
    }

    if (ch)
    {
		obj = get_obj_list_number(ch, argument, nth, ch->in_room->contents);
		if (obj != NULL)
		    return obj;

		if ((obj = get_obj_carry_number(ch, argument, nth, ch)) != NULL)
		    return obj;

		if ((obj = get_obj_wear_number(ch, argument, nth, TRUE)) != NULL)
		    return obj;
    }
    else
    {
	    int number = *nth;

		for (obj = room->contents; obj; obj = obj->next_content)
		{
			if (!is_name(argument, obj->name))
				continue;
			if (--number < 1)
				return obj;
		}

		*nth = number;
    }

    return NULL;
}

/*
 * Same as get_obj_here, except it checks the inventory FIRST.
 *
 * If worn is TRUE, worn items will be checked before carried items.
 */
OBJ_DATA *get_obj_inv(CHAR_DATA *ch, char *argument, bool worn)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;

    if (!ch)
    {
		bug("get_obj_inv received NULL ch",0);
		return NULL;
	}

    number = number_argument(argument, arg);

	if (worn)
	{
		if ((obj = get_obj_wear_number(ch, arg, &number, TRUE)) != NULL)
			return obj;

		if ((obj = get_obj_carry_number(ch, arg, &number, ch)) != NULL)
			return obj;
	}
	else
	{
		if ((obj = get_obj_carry_number(ch, arg, &number, ch)) != NULL)
			return obj;

		if ((obj = get_obj_wear_number(ch, arg, &number, TRUE)) != NULL)
			return obj;
	}

	obj = get_obj_list_number(ch, arg, &number, ch->in_room->contents);
    return obj;
}

OBJ_DATA *get_obj_inv_only(CHAR_DATA *ch, char *argument, bool worn)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;

    if (!ch)
    {
		bug("get_obj_inv received NULL ch",0);
		return NULL;
	}

    number = number_argument(argument, arg);

	if (worn)
	{
		if ((obj = get_obj_wear_number(ch, arg, &number, TRUE)) != NULL)
			return obj;

		if ((obj = get_obj_carry_number(ch, arg, &number, ch)) != NULL)
			return obj;
	}
	else
	{
		if ((obj = get_obj_carry_number(ch, arg, &number, ch)) != NULL)
			return obj;

		if ((obj = get_obj_wear_number(ch, arg, &number, TRUE)) != NULL)
			return obj;
	}

    return NULL;
}


/*
 * Find an obj in the world.
 */
OBJ_DATA *get_obj_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;

    if (ch && (obj = get_obj_here(ch, NULL, argument)) != NULL)
		return obj;

    number = number_argument(argument, arg);
    count  = 0;

    iterator_start(&it, loaded_objects);
    while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
		if ((ch && !can_see_obj(ch, obj)) ||
			!is_name(arg, obj->name))
	    	continue;
		if (++count == number)
		    break;
    }
    iterator_stop(&it);

    return obj;
}


/* deduct cost from a character */
void deduct_cost(CHAR_DATA *ch, int cost)
{
    int silver = 0, gold = 0;

    silver = UMIN(ch->silver,cost);

    if (silver < cost)
    {
	gold = ((cost - silver + 99) / 100);
	silver = cost - 100 * gold;
    }

    ch->gold -= gold;
    ch->silver -= silver;

    if (ch->gold < 0)
    {
	bug("deduct costs: gold %d < 0",ch->gold);
	ch->gold = 0;
    }

    if (ch->silver < 0)
    {
	bug("deduct costs: silver %d < 0",ch->silver);
	ch->silver = 0;
    }
}


/*
 * Create a 'money' obj.
 */
OBJ_DATA *create_money(int gold, int silver)
{
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;

    if (gold < 0 || silver < 0 || (gold == 0 && silver == 0))
    {
	bug("Create_money: zero or negative money.",UMIN(gold,silver));
	gold = UMAX(0,gold);
	silver = UMAX(1,silver);
    }

    if (gold == 0 && silver == 1)
	obj = create_object(get_obj_index(OBJ_VNUM_SILVER_ONE), 0, TRUE);
    else if (gold == 1 && silver == 0)
	obj = create_object(get_obj_index(OBJ_VNUM_GOLD_ONE), 0, TRUE);
    else if (silver == 0)
    {
        obj = create_object(get_obj_index(OBJ_VNUM_GOLD_SOME), 0, TRUE);
        sprintf(buf, obj->short_descr, gold);
        free_string(obj->short_descr);
        obj->short_descr        = str_dup(buf);
        obj->value[1]           = gold;
        obj->cost               = gold;
	obj->weight		= gold/5;
    }
    else if (gold == 0)
    {
        obj = create_object(get_obj_index(OBJ_VNUM_SILVER_SOME), 0, TRUE);
        sprintf(buf, obj->short_descr, silver);
        free_string(obj->short_descr);
        obj->short_descr        = str_dup(buf);
        obj->value[0]           = silver;
        obj->cost               = silver;
	obj->weight		= silver/20;
    }

    else
    {
	obj = create_object(get_obj_index(OBJ_VNUM_COINS), 0, TRUE);
	sprintf(buf, obj->short_descr, silver, gold);
	free_string(obj->short_descr);
	obj->short_descr	= str_dup(buf);
	obj->value[0]		= silver;
	obj->value[1]		= gold;
	obj->cost		= 100 * gold + silver;
	obj->weight		= gold / 5 + silver / 20;
    }

    return obj;
}


/*
 * Return # of objects which an object counts as.
 */
int get_obj_number(OBJ_DATA *obj)
{
    int number;

    if (obj->item_type == ITEM_MONEY)
        number = 0;
    else
        number = 1;

    return number;
}


/* Get number of items in a container */
int get_obj_number_container(OBJ_DATA *obj)
{
    int number = 0;

    for (obj = obj->contains; obj != NULL; obj = obj->next_content)
        number += get_obj_number(obj);

    return number;
}


/*
 * Return weight of an object, including weight of contents.
 * Adjusts content weight for weight-multiplier.
 */
int get_obj_weight(OBJ_DATA *obj)
{
    int weight;

    if (obj->item_type == ITEM_MONEY)
        return get_weight_coins(obj->value[0], obj->value[1]);

    weight = obj->weight;

    // This is for containers. Calculate weight of contents and factor in weight reduction
    weight += (get_obj_weight_container(obj)* WEIGHT_MULT(obj))/100;

    return weight;
}


/* return weight of x silver and y gold */
int get_weight_coins(long silver, long gold)
{
    return silver/50 + gold/30;
}


/*
 * Return weight of a container's objects.
 * This doesn't adjust for weight-multiplier, so note that.
 */
int get_obj_weight_container(OBJ_DATA *obj)
{
    int weight;
    OBJ_DATA *tobj;

    weight = 0;
    for (tobj = obj->contains; tobj != NULL; tobj = tobj->next_content)
	weight += tobj->weight;

    return weight;
}


/*
 * True if room is dark.
 */
bool room_is_dark(ROOM_INDEX_DATA *pRoomIndex)
{
    OBJ_DATA *obj;
    CHAR_DATA *ch;
    EXIT_DATA *exit;
    int i;

    for (obj = pRoomIndex->contents; obj; obj = obj->next_content) {
        if (obj->item_type == ITEM_ROOM_DARKNESS)
            return TRUE;
    }

    for (ch = pRoomIndex->people; ch; ch = ch->next_in_room) {
	for (obj = ch->carrying; obj; obj = obj->next_content) {
	    if (IS_SET(obj->extra2_flags, ITEM_EMITS_LIGHT)
		    && obj->wear_loc != WEAR_NONE) return FALSE;
	}
    }

    for (i = 0; i < MAX_DIR; i++) {
	ROOM_INDEX_DATA *to_room;

	exit = pRoomIndex->exit[i];

	if (!exit || IS_SET(exit->exit_info, EX_CLOSED)) continue;

	to_room = exit->u1.to_room;

	if (!to_room || !to_room->people) continue;

	for (ch = to_room->people; ch; ch = ch->next_in_room) {
	    for (obj = ch->carrying; obj; obj = obj->next_content) {
		if (IS_SET(obj->extra2_flags, ITEM_EMITS_LIGHT)
			&& obj->wear_loc != WEAR_NONE)
		    return FALSE;
	    }
	}
    }

    if (pRoomIndex->light > 0)
	return FALSE;

    if (IS_SET(pRoomIndex->room_flags, ROOM_DARK))
	return TRUE;

    if (pRoomIndex->sector_type == SECT_INSIDE ||
	pRoomIndex->sector_type == SECT_CITY)
	return FALSE;

    if (/*weather_info.sunlight == SUN_SET
    || */  weather_info.sunlight == SUN_DARK)
	return TRUE;

    return FALSE;
}


/* is ch the owner of the room? */
bool is_room_owner(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    if (room->owner == NULL || room->owner[0] == '\0')
	return FALSE;

    return is_name(ch->name,room->owner);
}


/*
 * True if room is private to a char.
 */
bool room_is_private(ROOM_INDEX_DATA *pRoomIndex, CHAR_DATA *looker)
{
    CHAR_DATA *rch;
    int count;
    int max_lev = 0;

    if (looker && !IS_NPC(looker) && looker->tot_level == MAX_LEVEL)
	return FALSE;

    count = 0;
    for (rch = pRoomIndex->people; rch != NULL; rch = rch->next_in_room)
    {
	count++;

	if (!IS_NPC(rch) && rch->tot_level > max_lev)
	    max_lev = rch->tot_level;
    }

    if (looker && !IS_NPC(looker) && IS_IMMORTAL(looker))
    {
	if (looker->tot_level > max_lev)
	    return FALSE;
    }

    if (IS_SET(pRoomIndex->room_flags, ROOM_PRIVATE)  && count >= 2)
	return TRUE;

    if (IS_SET(pRoomIndex->room_flags, ROOM_SOLITARY) && count >= 1)
	return TRUE;

    if (IS_SET(pRoomIndex->room_flags, ROOM_IMP_ONLY))
	return TRUE;

    return FALSE;
}

bool has_light(CHAR_DATA *ch)
{
	OBJ_DATA *obj;

	for(obj = ch->carrying; obj; obj = obj->next_content) {
		if(obj->wear_loc == WEAR_LIGHT || (IS_SET(obj->extra2_flags, ITEM_EMITS_LIGHT) && obj->wear_loc != WEAR_NONE))
			return TRUE;
	}

	return FALSE;
}

bool can_see_imm(CHAR_DATA *ch, CHAR_DATA *victim)
{
	STRING_DATA *string;

	if (victim == NULL || ch == NULL)
		return FALSE;

	/* allow imms to be vis to some people */
	if (!IS_NPC(victim))
	{
		for (string = victim->pcdata->vis_to_people; string != NULL; string = string->next)
		{
			if (!str_cmp(ch->name, string->string))
				return TRUE;
		}
	}

	if (ch->tot_level < victim->invis_level)
		return FALSE;

	if (!IS_IMMORTAL(ch) && IS_NPC(victim) && IS_SET(victim->act2, ACT2_WIZI_MOB) && !IS_SET(ch->act2, ACT2_SEE_WIZI))
		return FALSE;

	if (get_trust(ch) < victim->incog_level && ch->in_room != victim->in_room)
		return FALSE;

	return TRUE;
}

/*
 * True if char can see victim.
 */
bool can_see(CHAR_DATA *ch, CHAR_DATA *victim)
{
	STRING_DATA *string;

	if (victim == NULL || ch == NULL)
		return FALSE;

	/* imms w/ holylight can see everyone except higher level invis imms */
	if (!IS_NPC(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) && victim->invis_level <= get_trust(ch))
		return TRUE;

	// these types of mobs can see everybody.
	if (IS_NPC(ch) && (IS_SET(ch->act2, ACT2_SEE_ALL) || IS_SET(ch->act, ACT_IS_BANKER) || IS_SET(ch->act, ACT_IS_CHANGER) || IS_SET(ch->act, ACT_QUESTOR)))
		return TRUE;

	if (IS_AFFECTED(ch, AFF_BLIND))
		return FALSE;

	if (is_darked(ch->in_room))
		return FALSE;

	if (ch->in_room && IS_SET(ch->in_room->room_flags, ROOM_DARK))
	{
		//(!IS_AFFECTED(ch, AFF_INFRARED) || has_light(ch)) && IS_AFFECTED2(victim,AFF2_DARK_SHROUD)

		if( IS_AFFECTED2(victim,AFF2_DARK_SHROUD) )
			return FALSE;

		if( !IS_AFFECTED(ch, AFF_INFRARED) && !has_light(ch) )
			return FALSE;
	}

	if(!IS_AFFECTED2(victim,AFF2_DARK_SHROUD)) {
		if (IS_SET(victim->affected_by2, AFF2_CLOAK_OF_GUILE) && IS_NPC(ch) && !IS_SET(ch->affected_by2, AFF2_SEE_CLOAK))
			return FALSE;

		if ((IS_AFFECTED(victim, AFF_INVISIBLE) || IS_AFFECTED2(victim, AFF2_IMPROVED_INVIS)) && !IS_AFFECTED(ch, AFF_DETECT_INVIS))
			return FALSE;
	}

	if (IS_AFFECTED(victim, AFF_HIDE) && !IS_AFFECTED(ch, AFF_DETECT_HIDDEN) && !IS_SAGE(ch) && victim->fighting == NULL)
		return FALSE;

	// @@@ Nib 20070715 : Changed so that you need deathsight to see anyone that is dead...
	if (IS_DEAD(victim) && !IS_DEAD(ch) && (!IS_AFFECTED2(ch,AFF2_DEATHSIGHT) || victim->tot_level > ch->deathsight_vision))
		return FALSE;

	if (ch == victim)
		return TRUE;

	/* allow imms to be vis to some people */
	if (!IS_NPC(victim))
	{
		for (string = victim->pcdata->vis_to_people; string != NULL; string = string->next)
		{
			if (!str_cmp(ch->name, string->string))
				return TRUE;
		}
	}

	if (ch->tot_level < victim->invis_level)
		return FALSE;

	if (!IS_IMMORTAL(ch) && IS_NPC(victim) && IS_SET(victim->act2, ACT2_WIZI_MOB) && !IS_SET(ch->act2, ACT2_SEE_WIZI))
		return FALSE;

	if (get_trust(ch) < victim->incog_level && ch->in_room != victim->in_room)
		return FALSE;

	return TRUE;
}


/* visibility on a room -- for entering and exits */
bool can_see_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
    if (IS_SET(pRoomIndex->room_flags, ROOM_IMP_ONLY)
    &&  get_trust(ch) < MAX_LEVEL)
	return FALSE;

    if (IS_SET(pRoomIndex->room_flags, ROOM_GODS_ONLY)
    &&  !IS_IMMORTAL(ch))
	return FALSE;

    if (IS_SET(pRoomIndex->room_flags,ROOM_NEWBIES_ONLY)
    &&  ch->level > 10 && !IS_IMMORTAL(ch))
	return FALSE;

    return TRUE;
}


/*
 * True if char can see obj.
 */
bool can_see_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
    if (obj == NULL)
    {
	bug("can_see_obj, obj was NULL!!!", 0);
        return FALSE;
    }

    // Toggled on dead people's possessions.
    if (IS_SET(obj->extra2_flags, ITEM_UNSEEN)) {
		return FALSE;
	}

    if (IS_SET(obj->extra2_flags, ITEM_BURIED)) {
		return FALSE;
	}

    if (obj->item_type == ITEM_SEED && IS_SET(obj->extra_flags, ITEM_PLANTED)) {
		return FALSE;
	}

    // Item hidden on the ground, must be searched for first
    if (IS_SET(obj->extra_flags, ITEM_HIDDEN) && obj->in_room != NULL) {
		return FALSE;
	}

    if (!IS_NPC(ch) && IS_SET(ch->act, PLR_HOLYLIGHT))
		return TRUE;

    /*
    if (IS_NPC(ch)
    && !IS_DEAD(ch)
    && obj->carried_by == ch)
	return FALSE;
	*/

    if ((obj->carried_by != NULL && IS_DEAD(obj->carried_by))
    && obj->wear_loc == WEAR_NONE)
	return FALSE;

    if (IS_AFFECTED(ch, AFF_BLIND))
	return FALSE;

    if (obj->item_type == ITEM_LIGHT && obj->value[2] != 0)
	return TRUE;

    if (IS_SET(obj->extra_flags, ITEM_INVIS)
    &&   !IS_AFFECTED(ch, AFF_DETECT_INVIS))
        return FALSE;

    if (IS_AFFECTED(ch, AFF_INFRARED) && !is_darked(ch->in_room))
	return TRUE;

    if (IS_OBJ_STAT(obj,ITEM_GLOW))
	return TRUE;

    if (room_is_dark(ch->in_room))
        return FALSE;

    return TRUE;
}



// Assume that if first char is colour code, then UPPER third char, else first.
char *upper_first(char *arg)
{
    if (arg == NULL)
	return NULL;

    if (*arg == '\n')
	return arg;
    else
	if (*arg == '{')
	    *(arg + 2) = UPPER(*(arg + 2));
	else
	    *arg = UPPER(*arg);

    return arg;
}


int get_trade_item(char *arg)
{
    int counter;

    counter = 0;
    while(trade_table[counter].trade_type != TRADE_LAST)
    {
        if (!str_prefix(arg, trade_table[counter].name))
	    break;
        counter++;
    }

    return counter;
}


TRADE_ITEM *find_trade_item args((AREA_DATA* pArea, char *arg)) {
    TRADE_ITEM *item;

    item = pArea->trade_list;
    while(item != NULL)
    {
        if (!str_prefix(trade_table[item->trade_type].name, arg))
	    break;
        item = item->next;
    }

    return item;
}


/*
 * Move a char into a ships crew.
 */
void char_to_crew(CHAR_DATA *ch, SHIP_DATA *ship)
{
#if 0
    ROOM_INDEX_DATA *pRoomIndex;

    if (ship == NULL)
    {
	bug("SHIP WAS NULL with a char_to_crew.", 0);
	return;
    }

    pRoomIndex = ship->ship_rooms[0];
    char_to_room(ch, pRoomIndex);

    ch->belongs_to_ship  = ship;
    ch->next_in_crew    = ship->crew_list;
    ship->crew_list     = ch;
#endif

    return;
}

/*
 * Move a char into an invasion force.
 */
void char_to_invasion(CHAR_DATA *ch, INVASION_QUEST *invasion)
{
    if (invasion == NULL)
    {
	bug("INVASION WAS NULL with a char_to_invasion.", 0);
	return;
    }

    ch->next_in_invasion  = invasion->invasion_mob_list;
    invasion->invasion_mob_list     = ch;

    return;
}

/*
 * Move a char out of invasion force.
 */
void char_from_invasion(CHAR_DATA *ch, INVASION_QUEST *quest)
{
	if (ch == quest->invasion_mob_list)
	{
		quest->invasion_mob_list = ch->next_in_invasion;
	}
	else
	{
		CHAR_DATA *prev;

		for (prev = quest->invasion_mob_list; prev != NULL;
				prev = prev->next_in_invasion)
		{
			if (prev->next_in_invasion == ch)
			{
				prev->next_in_invasion = ch->next_in_invasion;
				break;
			}
		}
	}
	ch->next_in_invasion     = NULL;
	return;
}


/*
 * Move a char out of crew.
 */
void char_from_crew(CHAR_DATA *ch)
{
#if 0
	if (ch->belongs_to_ship == NULL)
	{
		bug("Char_from_crew: belongs_to_ship was null.", 0);
		return;
	}

	if (ch == ch->belongs_to_ship->crew_list)
	{
		ch->belongs_to_ship->crew_list = ch->next_in_crew;
	}
	else
	{
		CHAR_DATA *prev;

		for (prev = ch->belongs_to_ship->crew_list; prev; prev = prev->next_in_crew)
		{
			if (prev->next_in_crew == ch)
			{
				prev->next_in_crew = ch->next_in_crew;
				break;
			}
		}

		if (prev == NULL)
			bug("Char_from_crew: ch not found.", 0);
	}

	ch->belongs_to_ship  = NULL;
	ch->next_in_crew     = NULL;
	ch->on 	         = NULL;  /* sanity check! */
#endif
	return;
}

/*
 * Parses a command and returns which door it is.
 */
int parse_direction(char *arg)
{
    int counter;
    int direction = -1;

    if (arg[0] == '\0')
	return direction;

    /* Vizz - rewrote this to include "n,e,w,s,u,d" directions
     *        (fuck knows why they weren't in here already), and also
     *        made each if an if ... else so that we don't have to do
     *        all of them every time.
     */
    // Nib - changed it to reassigning the char* since all printf's are inefficient really...
    //		especially if this is used alot.
    if (!str_cmp(arg, "n")) arg = "north";
    else if (!str_cmp(arg, "e")) arg = "east";
    else if (!str_cmp(arg, "s")) arg = "south";
    else if (!str_cmp(arg, "w")) arg = "west";
    else if (!str_cmp(arg, "u")) arg = "up";
    else if (!str_cmp(arg, "d")) arg = "down";
    else if (!str_cmp(arg, "nw")) arg = "northwest";
    else if (!str_cmp(arg, "ne")) arg = "northeast";
    else if (!str_cmp(arg, "sw")) arg = "southwest";
    else if (!str_cmp(arg, "se")) arg = "southeast";

/* Vizz - hmm - doesn't the use of str_prefix() here mean that if
 *        the argument string is, for example, "northeast" you'll flee north?!
    if (!str_prefix(arg, "north")) sprintf(arg, "north");
    if (!str_prefix(arg, "south")) sprintf(arg, "south");
    if (!str_prefix(arg, "east")) sprintf(arg, "east");
    if (!str_prefix(arg, "west")) sprintf(arg, "west");
*/
  for (counter = 0; counter < MAX_DIR; counter++)
    {
	if (!str_cmp(arg, dir_name[counter]))
	    direction = counter;
    }

    return direction;
}


/* make NPC ch hunt a victim */
void hunt_char(CHAR_DATA *ch, CHAR_DATA *victim)
{
    bool found;
    char buf[MSL];
    CHAR_DATA *temp;

    if (!IS_NPC(ch)) {
	sprintf(buf, "hunt_char: non-NPC %s", ch->name);
	bug(buf, 0);
	return;
    }

    if (IS_SET(ch->act2, ACT2_NO_CHASE))
	return;

    found = FALSE;
    temp = hunt_last;
    while (temp != NULL)
    {
        if (temp == ch)
	    found = TRUE;
        temp = temp->next_in_hunting;
    }

    if (found)
	return;

    ch->next_in_hunting    = hunt_last;
    hunt_last              = ch;
    ch->hunting = victim;
}


// Stop an NPC hunting
void stop_hunt(CHAR_DATA *ch, bool dead)
{
    if (ch == hunt_last)
	hunt_last = ch->next_in_hunting;
    else
    {
	CHAR_DATA *prev;

	for (prev = hunt_last; prev; prev = prev->next_in_hunting)
	{
	    if (prev->next_in_hunting == ch)
	    {
		prev->next_in_hunting = ch->next_in_hunting;
		break;
	    }
	}

	if (prev == NULL)
	    bug("stop_hunt: ch not found.", 0);
    }

    ch->hunting  = NULL;
    ch->next_in_hunting = NULL;

    if (!dead && ch->home_room != NULL)
    {
    	act("$n wanders off.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	char_from_room(ch);
	char_to_room(ch, ch->home_room);
    }
}


AREA_DATA *find_area(char *name)
{
    AREA_DATA *temp;
    char buf[MSL];

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
	if (!str_cmp(temp->name, name))
	    break;
    }

    if (temp == NULL) {
	sprintf(buf, "find_area: couldn't find area %s", name);
	log_string(buf);
    }

    return temp;
}


AREA_DATA *find_area_kwd(char *keyword)
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
	if (!str_infix(keyword, temp->name))
	    break;
    }

    return temp;
}


bool is_on_ship(CHAR_DATA *ch, SHIP_DATA *ship)
{
    if (ch->in_room != NULL
    && ch->in_room->ship != NULL
    && ch->in_room->ship == ship)
    {
	return TRUE;
    }
    else
    {
	return FALSE;
    }
}


/* Resurrect a dead PC. */
void resurrect_pc(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    ROOM_INDEX_DATA *pRoom = NULL;
    //AREA_DATA *pArea = NULL;
    OBJ_DATA *obj;
    //bool exists = FALSE;

    if (!IS_DEAD(ch))
    {
	sprintf(buf, "resurrect_pc: %s is not dead!", ch->name);
	bug(buf, 0);
	return;
    }

    if (IS_NPC(ch))
    {
	sprintf(buf, "resurrect_pc: %s is an NPC!", ch->short_descr);
	bug(buf, 0);
	return;
    }

    ch->time_left_death = 0;

    char_from_room(ch);

    if ((pRoom = location_to_room(&ch->recall)) == NULL)
	pRoom = get_room_index(ROOM_VNUM_ALTAR);

    char_to_room(ch,pRoom);
    location_clear(&ch->recall);
    // remove and reset affects
    while (ch->affected)
	affect_remove(ch, ch->affected);

    if (IS_SAGE(ch))
		SET_BIT(ch->affected_by, AFF_DETECT_HIDDEN);

    ch->dead = FALSE;

    if (IS_AFFECTED(ch, AFF_CHARM))
    {
	char buf[MAX_STRING_LENGTH];

	if (ch->master != NULL)
	{
	    sprintf(buf, "%s dissipates into the shadows.\n\r", ch->name);
	    send_to_char(buf, ch->master);
	}

	send_to_char("You soul is free once more.\n\r", ch);
    }

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content) {
		if (IS_SET(obj->extra2_flags, ITEM_UNSEEN))
		    REMOVE_BIT(obj->extra2_flags, ITEM_UNSEEN);
    }

    /*FREE DEATH ITEMS HERE IF ANY */

	affect_fix_char(ch);

    /* Reset form and parts - Fixes issue 33 on gitlab repo - Tieryo 07/22/2016 */
	/* Went back to fix properly for issue 126 */
    ch->form = race_table[ch->race].form;
    ch->parts = race_table[ch->race].parts & ~ch->lostparts;
    ch->lostparts	= 0;	// Restore anything lost

    if (IS_SAGE(ch))
		SET_BIT(ch->affected_by, AFF_DETECT_HIDDEN);

    update_pos(ch);

	// Used to handle any post resurrection actions
    p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->pcdata->corpse, NULL, TRIG_RESURRECT, NULL);
    p_percent_trigger(NULL, ch->pcdata->corpse, NULL, NULL, ch, ch, NULL, NULL, NULL, TRIG_RESURRECT, NULL);
}


/* is a mob a global mob? */
bool is_global_mob(CHAR_DATA *mob)
{
    GQ_MOB_DATA *gq_mob;

    if (!IS_NPC(mob))
    {
	bug("is_global_mob: not an npc!", 0);
	return FALSE;
    }

    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
    {
	if (mob->pIndexData->vnum == gq_mob->vnum)
	    return TRUE;
    }

    return FALSE;
}


/* send a yellow line of length 'length' to a character */
void line(CHAR_DATA *ch, int length)
{
    int i;

    send_to_char("{Y", ch);
    for (i = 0; i < length; i++)
    {
	send_to_char("-", ch);
    }
    send_to_char("{x\n\r", ch);
}


/* is a room darked with momentary darkness? */
bool is_darked(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;

    if (room == NULL)
    {
	bug("is_darked: in_room was null.", 0);
	return FALSE;
    }

    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
	if (obj->item_type == ITEM_ROOM_DARKNESS)
	    return TRUE;
    }

    return FALSE;
}


/* returns short "handle" for a character */
char *pers(CHAR_DATA *ch, CHAR_DATA *looker)
{
    if (!can_see(looker, ch))
		return "someone";

    if (IS_NPC(ch) || IS_SWITCHED(ch))
		return ch->short_descr;

    if (IS_MORPHED(ch) || IS_SHIFTED(ch))
    {
		if (can_see_shift(looker, ch))
		    return ch->name;
		else
		    return ch->short_descr;
    }

    return ch->name;
}


/* can ch see through victim's shift? */
bool can_see_shift(CHAR_DATA *ch, CHAR_DATA *victim)
{
    OBJ_DATA *obj;

    if (ch == NULL || victim == NULL)
    {
	bug("can_see_shift: called with null ch or victim!", 0);
	return FALSE;
    }

    if (IS_IMMORTAL(ch))
        return TRUE;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if(IS_SET(obj->extra2_flags, ITEM_TRUESIGHT)
	&& obj->wear_loc != WEAR_NONE
	&& ch->in_room == victim->in_room)
	    return TRUE;
    }

    return FALSE;
}


// Is a person scary?
bool can_scare(CHAR_DATA *ch)
{
    OBJ_DATA *obj;

    if (ch == NULL) {
	bug("can_scare: NULL ch", 0);
	return FALSE;
    }

    if (IS_SHIFTED_SLAYER(ch))
	return TRUE;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->wear_loc != WEAR_NONE
	&&  IS_SET(obj->extra2_flags, ITEM_SCARE))
	    return TRUE;
    }

    return FALSE;
}


/* does a person have to eat or drink? returns TRUE if not */
bool is_sustained(CHAR_DATA *ch)
{
    OBJ_DATA *obj;

    if (ch->carrying == NULL)
        return FALSE;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (IS_SET(obj->extra2_flags, ITEM_SUSTAIN)
	&& obj->wear_loc != WEAR_NONE)
	    return TRUE;
    }

    return FALSE;
}


/* is victim ignoring ch? */
bool is_ignoring(CHAR_DATA *victim, CHAR_DATA *ch)
{
    IGNORE_DATA *ignore;

    if (IS_IMMORTAL(ch) || IS_NPC(victim))
	return FALSE;

    for (ignore = victim->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
    {
	if (!str_cmp(ignore->name, ch->name))
	    return TRUE;
    }

    return FALSE;
}

/* does a person have both hands full */
bool both_hands_full(CHAR_DATA *ch)
{
	OBJ_DATA *w1,*w2,*sh,*h;

	w1 = get_eq_char(ch, WEAR_WIELD);
	w2 = get_eq_char(ch, WEAR_SECONDARY);
	sh = get_eq_char(ch, WEAR_SHIELD);
	h = get_eq_char(ch, WEAR_HOLD);

	/* wielding 2 weapons */
	if(w1 && w2) return TRUE;

	/* a weapon and a shield/held item */
	if ((sh || h) && (w1 || w2)) return TRUE;

	/* a two-handed weapon, but not for big races */
	if (w1 && ch->size < SIZE_HUGE && IS_WEAPON_STAT(w1,WEAPON_TWO_HANDS)) return TRUE;

	/* a weapon and an item */
	/* a shield and an item */
	if(h && (w1 || w2 || sh)) return TRUE;

	return FALSE;
}


/* does a person have one hand full */
bool one_hand_full(CHAR_DATA *ch)
{
    if (get_eq_char(ch, WEAR_SHIELD) != NULL
    || get_eq_char(ch, WEAR_WIELD) != NULL
    || get_eq_char(ch, WEAR_HOLD) != NULL
    || get_eq_char(ch, WEAR_SECONDARY) != NULL)
	return TRUE;

    return FALSE;
}


/* is an item a relic, any relic */
bool is_relic(OBJ_INDEX_DATA *obj)
{
    if (obj->vnum == OBJ_VNUM_RELIC_EXTRA_DAMAGE
    ||   obj->vnum == OBJ_VNUM_RELIC_EXTRA_XP
    ||   obj->vnum == OBJ_VNUM_RELIC_EXTRA_PNEUMA
    ||   obj->vnum == OBJ_VNUM_RELIC_HP_REGEN
    ||   obj->vnum == OBJ_VNUM_RELIC_MANA_REGEN)
	return TRUE;

    return FALSE;
}


/* is a room completely dislinked (no exits) ? */
bool is_dislinked(ROOM_INDEX_DATA *pRoom)
{
    EXIT_DATA *pExit;
    int i;

    i = 0;
    for (pExit = pRoom->exit[i]; i < MAX_DIR; i++)
    {
	if (pExit != NULL)
	    return FALSE;
    }

    return TRUE;
}


/* is person's church good? */
bool is_good_church(CHAR_DATA *ch)
{
    if (ch->church == NULL)
	return FALSE;

    if (ch->church->alignment == CHURCH_GOOD)
	return TRUE;

    return FALSE;
}


/* is person's church evil? */
bool is_evil_church(CHAR_DATA *ch)
{
    if (ch->church == NULL)
	return FALSE;

    if (ch->church->alignment == CHURCH_EVIL)
	return TRUE;

    return FALSE;
}


/* is person wielding a weapon of a certain type? (spear, sword, whatever) */
bool wields_item_type(CHAR_DATA *ch, int weapon_type)
{
    OBJ_DATA *wield;
    OBJ_DATA *wield2;

    if ((wield = get_eq_char(ch, WEAR_WIELD)) != NULL)
    {
	if (wield->value[0] == weapon_type)
	    return TRUE;
    }

    if ((wield2 = get_eq_char(ch, WEAR_SECONDARY)) != NULL)
    {
        if (wield2->value[0] == weapon_type)
	    return TRUE;
    }

    return FALSE;
}


char *pirate_name_generator(void)
{
	char *first_name[26] = {
		"Long",
		"Short",
		"Big",
		"Small",
		"Snot",
		"Grimy",
		"Lanky",
		"Burly",
		"Angry",
		"Furry",
		"Silly",
		"Dangle",
		"Wobble",
		"Red",
		"Blue",
		"Green",
		"Shiny",
		"Toothy",
		"Fearless",
		"Fat",
		"Baby",
		"Spice",
		"Happy",
		"Toked",
		"Anvil",
		"One-eyed" };

	char *middle_name[15] = {
		"Beard",
		"Hook",
		"Peg",
		"Tooth",
		"Belly",
		"Buckle",
		"Sword",
		"Armed",
		"Rex",
		"Bill",
		"Willy",
		"Monkey",
		"Trouser",
		"Brow",
		"Jim" };

	char *last_name[14] = {
		"Snake",
		"Silver",
		"Hook",
		"Tiger",
		"Lionheart",
		"The bad",
		"Smithers",
		"Pinkleton",
		"Burns",
		"Simpson",
		"Rohnscharch",
		"Diablo",
		"Parker",
		"Adamson"
	};
	int name = number_range(0, 25);
	int middle = number_range(0, 14);
	int last = number_range(0, 13);
	char buf[MAX_STRING_LENGTH];

	sprintf(buf, "%s %s %s", first_name[name], middle_name[middle], last_name[last]);

	return (char *) str_dup(buf);
}

/*
 * This looks at the landing coords.
 * Currently used if airship lands outside in Wilds.
 */
AREA_DATA *find_area_at_land_coords(int x, int y )
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
	if ( temp->land_x == x && temp->land_y == y ) {
	    break;
  }
    }

    if ( temp == NULL )
    {
	bug("Couldn't find area.", 0);
    }

    return temp;
}


/*
 * This looks at city coords.
 * Currently used if airship lands in city.
 */
AREA_DATA *find_area_at_coords(int x, int y )
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
	if ( temp->x == x && temp->y == y )
	    break;
    }

    if ( temp == NULL )
    {
	bug("Couldn't find area.", 0);
    }

    return temp;
}


/* get remort race of a character based on their player race */
// NIB20090323 - simplified this by adding race pointers to the pc_race table that points to the race number for the remort
// If that race doesn't have the pointer, it can't remort. :)
int get_remort_race(CHAR_DATA *ch)
{
	int race, pc_race;

	race = ch->race;
	pc_race = race_table[race].pgprn ? *race_table[race].pgprn : 0;

	return (!pc_race_table[pc_race].remort && pc_race_table[pc_race].prgrn) ? *pc_race_table[pc_race].prgrn : 0;
}


/* is person dead? used for do_functions which check for this. */
bool is_dead(CHAR_DATA *ch)
{
    if (IS_DEAD(ch))
    {
        send_to_char("You can't do that. You are dead.\n\r", ch);
	return TRUE;
    }

    return FALSE;
}


/* deduct movement. makes some hackish adjustments */
void deduct_move(CHAR_DATA *ch, int amount)
{
    if (ch->tot_level < 30)
        amount /= 4;
    else if (ch->tot_level < 60)
        amount /= 3;
    else if (ch->tot_level < 90)
        amount /= 2;

    // athletics reduces movement usage
    if (number_percent() < get_skill(ch, gsn_athletics) / 8)
    {
	if (number_percent() == 1)
	    check_improve(ch, gsn_athletics, TRUE, 8);

	return;
    }

    amount = UMAX(amount, 1);

    ch->move -= amount;
    if (ch->move < 0)
	ch->move = 0;
}


/* is an item wearable on any part of the body? */
bool is_wearable(OBJ_DATA *obj)
{
	if(!(obj->wear_flags & ~ITEM_NONWEAR) && obj->item_type != ITEM_LIGHT)
		return FALSE;

	return TRUE;
}


/* is anyone resting, sleeping, etc on the obj? */
bool is_using_anyone(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if (obj->in_room == NULL)
    {
	bug("Got is_using_obj with obj->in_room NULL!", 0);
	return FALSE;
    }

    for (ch = obj->in_room->people; ch != NULL; ch = ch->next_in_room)
    {
	if (ch->on == obj)
	    return TRUE;
    }

    return FALSE;
}


// Returns from maze spell
void return_from_maze(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA *pRoom = NULL;

    char_from_room(ch);

    do
	pRoom = get_random_room(ch, BOTH_CONTINENTS);
    while (pRoom == NULL);

    char_to_room(ch,pRoom);

    ch->maze_time_left = 0;

    act("{W$n plummets to the ground with a loud THUD!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}


/*
 * Move a char into an autowar team.
 */
void char_to_team(CHAR_DATA *ch)
{
    ch->next_in_auto_war    = auto_war->team_players;
    auto_war->team_players  = ch;
    ch->in_war              = TRUE;
}


/*
 * Move a char from an autowar team.
 */
void char_from_team(CHAR_DATA *ch)
{
    if (auto_war == NULL)
    {
	return;
    }

    if (ch == auto_war->team_players)
    {
	auto_war->team_players = ch->next_in_auto_war;
    }
    else
    {
	CHAR_DATA *prev;

	for (prev = auto_war->team_players; prev != NULL;
	      prev = prev->next_in_auto_war)
	{
	    if (prev->next_in_auto_war == ch)
	    {
		prev->next_in_auto_war = ch->next_in_auto_war;
		break;
	    }
	}
    }
    ch->next_in_auto_war     = NULL;
    ch->in_war 		= FALSE;
}


// return number of objects in container
int get_number_in_container(OBJ_DATA *obj)
{
    int i;
    OBJ_DATA *inside;

    i = 0;
    for (inside = obj->contains; inside != NULL; inside = inside->next_content)
	i++;

    return i;
}


// calculate the dp value of an obj
long get_dp_value(OBJ_DATA *obj)
{
    int deitypoints;
    OBJ_DATA *objnest;

    deitypoints = UMAX(1,obj->level * 3 + UMAX(obj->cost/1000, 1));

    if (obj->item_type != ITEM_CORPSE_NPC
    && obj->item_type != ITEM_CORPSE_PC)
	deitypoints = UMIN(deitypoints,obj->cost);

    if (obj->item_type == ITEM_MONEY)
    {
	deitypoints = obj->value[0] / 100;
	deitypoints += obj->value[1];
    }

    for (objnest = obj->contains; objnest != NULL; objnest = objnest->next_content)
	deitypoints += get_dp_value(objnest);

    return deitypoints;
}


// used for the "qlist" so people may receive some tells while quiet
bool can_tell_while_quiet(CHAR_DATA *ch, CHAR_DATA *victim)
{
    STRING_DATA *string;

    if (ch == NULL || victim == NULL || IS_NPC(ch) || IS_NPC(victim))
	return FALSE;

    for (string = victim->pcdata->quiet_people; string != NULL; string = string->next)
    {
	if (!str_cmp(string->string, ch->name))
	    return TRUE;
    }

    return FALSE;
}


// check if ch can hunt victim
bool can_hunt(CHAR_DATA *ch, CHAR_DATA *victim)
{
    OBJ_DATA *obj;

    if (victim == NULL)
    {
	bug("can_hunt: victim was null!", 0);
	return FALSE;
    }

    for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->wear_loc != WEAR_NONE && IS_SET(obj->extra2_flags, ITEM_NO_HUNT))
	    return FALSE;
    }

    return TRUE;
}

// get region (roughly) in the wilds
int get_region(ROOM_INDEX_DATA *room)
{
	if( !room )
		return -1;

	if( !IS_WILDERNESS(room) )
		return -1;

    // Small Wilds
    if( room->wilds->uid == 6 )
    {
		// Handle smallest regions FIRST
		if( room->x >= 193 && room->x <= 301 &&
			room->y >= 607 && room->y <= 683 )
			return REGION_MORDRAKE_ISLAND;

		if( room->x >= 103 && room->x <= 206 &&
			room->y >= 1043 && room->y <= 1153 )
			return REGION_DRAGON_ISLAND;

		if( room->x >= 1277 && room->x <= 1340 &&
			room->y >= 1132 && room->y <= 1185 )
			return REGION_TEMPLE_ISLAND;

		if( room->x >= 771 && room->x <= 895 &&
			room->y >= 537 && room->y <= 626 )
			return REGION_ARENA_ISLAND;

		if( room->x >= 1277 && room->x <= 1360 &&
			room->y >= 203 && room->y <= 253 )
			return REGION_UNDERSEA;

		// Smaller continents
		if( room->x >= 185 && room->x <= 517 &&
			room->y >= 335 && room->y <= 542 )
			return REGION_FIRST_CONTINENT;

		if( room->x >= 1011 && room->x <= 1488 &&
			room->y >= 746 && room->y <= 993 )
			return REGION_SECOND_CONTINENT;

		if( room->x >= 294 && room->x <= 674 &&
			room->y >= 741 && room->y <= 1078 )
			return REGION_THIRD_CONTINENT;

		// Polar Regions (since parts of the fourth continent overlaps)
		if( room->x >= 313 && room->x <= 454 &&
			room->y <= 97 )
			return REGION_NORTH_POLE;

		if( room->x >= 1419 && room->x <= 1532 &&
			room->y <= 92 )
			return REGION_NORTH_POLE;

		if( room->y <= 55 )
			return REGION_NORTH_POLE;

		if( room->x >= 469 && room->x <= 612 &&
			room->y >= 1125 )
			return REGION_SOUTH_POLE;

		if( room->y >= 1175 )
			return REGION_SOUTH_POLE;

		// Larger continents
		// Fourth's shape needs to be broken up into subregions
		if( room->x >= 717 && room->x <= 1469 &&
			room->y >= 0 && room->y <= 410 )
			return REGION_FOURTH_CONTINENT;

		if( room->x >= 393 && room->x <= 717 &&
			room->y >= 0 && room->y <= 247 )
			return REGION_FOURTH_CONTINENT;

		if( room->x >= 619 && room->x <= 717 &&
			room->y >= 248 && room->y <= 337 )
			return REGION_FOURTH_CONTINENT;

		if( room->x >= 925 && room->x <= 1133 &&
			room->y >= 410 && room->y <= 516 )
			return REGION_FOURTH_CONTINENT;

		// Oceans
		if( room->y <= 363 )
			return REGION_NORTHERN_OCEAN;

		if( room->y >= 937 )
			return REGION_SOUTHERN_OCEAN;

		if( room->x <= 371 )
			return REGION_WESTERN_OCEAN;

		if( room->x >= 1146 )
			return REGION_EASTERN_OCEAN;

		return REGION_CENTRAL_OCEAN;
	}

/*

    if (room->x > 200
    && room->x < 510
    && room->y > 110
    && room->y < 285)
	return REGION_FIRST_CONTINENT;

    if (room->x > 749
    && room->x < 1201
    && room->y > 80
    && room->y < 230)
	return REGION_SECOND_CONTINENT;

    if (room->x > 160
    && room->x < 209
    && room->y > 270
    && room->y < 295)
	return REGION_MORDRAKE_ISLAND;

    if (room->x > 949
    && room->x < 993
    && room->y > 346
    && room->y < 378)
	return REGION_TEMPLE_ISLAND;

    if (room->x > 1129
	&& room->x < 1201
	&& room->y > 14
	&& room->y < 45)
	return REGION_UNDERSEA;

    return REGION_OCEAN;
    */

    return REGION_UNKNOWN;

}


bool is_on_second_continent(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA *room;

    if (ch->in_room == NULL)
	return FALSE;

    if (str_cmp(ch->in_room->area->name, "Wilderness"))
    	return FALSE;

    room = ch->in_room;
    if (room->x > 749
    &&   room->x < 1200
    &&   room->y > 80
    &&   room->y < 295)
	return TRUE;

    return FALSE;
}


bool check_ice_storm(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;

    if (room == NULL)
    {
	bug ("check_ice_storm: room was null", 0);
	return FALSE;
    }

    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
	if (obj->pIndexData->vnum == OBJ_VNUM_ICE_STORM)
	    return TRUE;
    }

    return FALSE;
}


// does a player with said name exist (ie do they have a pfile)
bool player_exists(char *argument)
{
    char player_name[MSL];
    bool found_char = FALSE;
    FILE *fp;

    sprintf(player_name, "%s%c/%s", PLAYER_DIR, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(player_name, "r")) == NULL)
	found_char = FALSE;
    else
    {
	found_char = TRUE;
	fclose (fp);
    }

    return found_char;
}


// Find a skull of a person in ch's inv. Looks in containers.
OBJ_DATA *get_skull(CHAR_DATA *ch, char *owner)
{
    OBJ_DATA *obj;
    OBJ_DATA *objNest;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->contains != NULL)
	    for (objNest = obj->contains; objNest != NULL; objNest = objNest->next_content)
	    {
		if (can_see_obj(ch, objNest)
		&&  (objNest->pIndexData->vnum == OBJ_VNUM_SKULL
		      || objNest->pIndexData->vnum == OBJ_VNUM_GOLD_SKULL)
		&& !str_cmp(objNest->owner, owner))
		    return objNest;
	    }

	if (can_see_obj(ch, obj)
	&&  (obj->pIndexData->vnum == OBJ_VNUM_SKULL
	     || obj->pIndexData->vnum == OBJ_VNUM_GOLD_SKULL)
        && !str_cmp(obj->owner, owner))
	    return obj;
    }

    return NULL;
}


int count_exits(ROOM_INDEX_DATA *room)
{
    int exits = 0;

    for (int n = 0; n < MAX_DIR; n++)
    {
    	EXIT_DATA *exit = room->exit[n];
		if (exit != NULL && exit->u1.to_room != NULL)
	    	exits++;
    }

    return exits;
}

// Is the room PK? (allows for ignoring ARENA)
bool is_room_pk(ROOM_INDEX_DATA *room, bool arena)
{
	if( room != NULL ) {
		if( IS_SET(room->room_flags, ROOM_PK) || IS_SET(room->room_flags, ROOM_CPK) )
			return TRUE;

		// Only count ARENA (LPK) if desired
		if( arena && IS_SET(room->room_flags, ROOM_ARENA) )
			return TRUE;
	}

	return FALSE;
}

// Is a person PK? Covers all possible cases (PK flag, room PK, etc)
bool is_pk(CHAR_DATA *ch)
{
    CHAR_DATA *fch;

    if (ch->church != NULL && ch->church->pk == TRUE)
	return TRUE;

    if (is_room_pk(ch->in_room, FALSE))
		return TRUE;

    if (ch->pk_timer > 0)
    	return TRUE;

    if (IS_SET(ch->act, PLR_PK))
	return TRUE;

    // If you pull a relic you are also PK
    if (ch->pulled_cart != NULL
    &&   is_relic(ch->pulled_cart->pIndexData))
	return TRUE;

   // If you pull a relic your form members are PK as well
   for (fch = ch->in_room->people; fch != NULL; fch = fch->next_in_room)
   {
       if (fch->pulled_cart != NULL
       &&   is_relic(fch->pulled_cart->pIndexData)
       &&   is_same_group(ch, fch))
	   return TRUE;
   }

   return FALSE;
}


// Get direction number from its character-string description.
int get_num_dir(char *arg)
{
    int door;

	 if (!str_cmp(arg, "n") || !str_cmp(arg, "north")) door = 0;
    else if (!str_cmp(arg, "e") || !str_cmp(arg, "east" )) door = 1;
    else if (!str_cmp(arg, "s") || !str_cmp(arg, "south")) door = 2;
    else if (!str_cmp(arg, "w") || !str_cmp(arg, "west" )) door = 3;
    else if (!str_cmp(arg, "u") || !str_cmp(arg, "up"   )) door = 4;
    else if (!str_cmp(arg, "d") || !str_cmp(arg, "down" )) door = 5;
    else if (!str_cmp(arg, "ne") || !str_cmp(arg, "northeast" )) door = 6;
    else if (!str_cmp(arg, "nw") || !str_cmp(arg, "northwest" )) door = 7;
    else if (!str_cmp(arg, "se") || !str_cmp(arg, "southeast" )) door = 8;
    else if (!str_cmp(arg, "sw") || !str_cmp(arg, "southwest" )) door = 9;
    else door = -1;

    return door;
}


// Dislink a room completely. Returns true if anything was changed.
bool dislink_room(ROOM_INDEX_DATA *pRoom)
{
    int i;
    char cmd[MSL];
    char buf[MSL];
    bool changed = FALSE;

    for (i = 0; i < MAX_DIR; i++)
    {
	if (pRoom->exit[i] != NULL
	&&   pRoom->exit[i]->u1.to_room != NULL)
	{
	    sprintf(cmd, "%ld delete", pRoom->vnum);
	    rp_change_exit(pRoom, cmd, i);
	    changed = TRUE;
	    sprintf(buf, "dislink_room: dislinked room %s (%ld)",
	        pRoom->name, pRoom->vnum);
	    log_string(buf);
	}
    }

    return changed;
}


// Used for druids.
bool is_in_nature(CHAR_DATA *ch)
{
    if (ch == NULL)
    {
	bug("is_in_nature: ch null", 0);
	return FALSE;
    }

    if (ch->in_room == NULL)
    {
	bug("is_in_nature: ch->in_room null", 0);
	return FALSE;
    }

    switch(ch->in_room->sector_type)
    {
	case SECT_FIELD:
	case SECT_FOREST:
	case SECT_HILLS:
	case SECT_MOUNTAIN:
	case SECT_WATER_SWIM:
	case SECT_TUNDRA:
	    return TRUE;

	default:
	    return FALSE;
    }
}


// Is there a PK/CPK room within a certain range of this one
int is_pk_safe_range(ROOM_INDEX_DATA *room, int depth, int reverse_dir)
{
    EXIT_DATA *ex;
    ROOM_INDEX_DATA *to_room;
    int dir;

    /*
    if (reverse_dir != -1 && (IS_SET(room->room_flags, ROOM_PK)
    ||   IS_SET(room->room_flags, ROOM_CPK)))
        return rev_dir[reverse_dir];
     */

    if (depth == 0)
    {
	if (IS_SET(room->room_flags, ROOM_PK)
	||   IS_SET(room->room_flags, ROOM_CPK))
	    return 10;

	else
	    return -1;
    }

    for (dir = 0; dir < MAX_DIR; dir++)
    {
    	if (dir != reverse_dir
	&& (ex = room->exit[dir]) != NULL
	&&     (to_room = room->exit[dir]->u1.to_room) != NULL)
	{
	    if (is_pk_safe_range(to_room, depth - 1, rev_dir[dir]) > -1)
	    	return dir;

	    if (IS_SET(to_room->room_flags, ROOM_PK)
	    ||   IS_SET(to_room->room_flags, ROOM_CPK))
		return dir;
	}

    }

    return -1;
}


// is person pulling a relic, any relic
bool is_pulling_relic(CHAR_DATA *ch)
{
    if (ch == NULL)
        return FALSE;

    if (ch->pulled_cart != NULL
    &&   is_relic(ch->pulled_cart->pIndexData))
        return TRUE;

    return FALSE;
}


// Lookup a class, subclass, or second subclass.
int get_profession(CHAR_DATA *ch, int class_type)
{
    if (IS_NPC(ch))
	return CLASS_NPC;

    if (ch->pcdata == NULL)
    {
	bug("get_profession: null pcdata on a pc", 0);
	return CLASS_NPC;
    }

    switch (class_type)
    {
        case CLASS_CURRENT:		return ch->pcdata->class_current;
	case SUBCLASS_CURRENT:		return ch->pcdata->sub_class_current;

	case CLASS_MAGE:		return ch->pcdata->class_mage;
	case CLASS_CLERIC:		return ch->pcdata->class_cleric;
	case CLASS_THIEF:		return ch->pcdata->class_thief;
	case CLASS_WARRIOR:		return ch->pcdata->class_warrior;

	case SECOND_CLASS_MAGE:		return ch->pcdata->second_class_mage;
	case SECOND_CLASS_CLERIC:	return ch->pcdata->second_class_cleric;
	case SECOND_CLASS_THIEF:	return ch->pcdata->second_class_thief;
	case SECOND_CLASS_WARRIOR:	return ch->pcdata->second_class_warrior;

	case SUBCLASS_MAGE:		return ch->pcdata->sub_class_mage;
	case SUBCLASS_CLERIC:		return ch->pcdata->sub_class_cleric;
	case SUBCLASS_THIEF:		return ch->pcdata->sub_class_thief;
	case SUBCLASS_WARRIOR:		return ch->pcdata->sub_class_warrior;

	case SECOND_SUBCLASS_MAGE:	return ch->pcdata->second_sub_class_mage;
	case SECOND_SUBCLASS_CLERIC:	return ch->pcdata->second_sub_class_cleric;
	case SECOND_SUBCLASS_THIEF:	return ch->pcdata->second_sub_class_thief;
	case SECOND_SUBCLASS_WARRIOR:	return ch->pcdata->second_sub_class_warrior;

	default:
	    bug("get_profession: bad class_type %d", class_type);
	    return CLASS_NPC;
    }
}


// Set a class, subclass, etc on someone.
void set_profession(CHAR_DATA *ch, int class_type, int class_value)
{
    if (IS_NPC(ch))
	return;

    if (ch->pcdata == NULL) {
	bug("set_profession: null pcdata on a pc", 0);
	return;
    }

    switch (class_type)
    {
	case CLASS_MAGE:
	    if (class_value == CLASS_MAGE)
		ch->pcdata->class_mage = CLASS_MAGE;
	    else
		bug("set_profession: trying to set a non-mage class for mage class spot", 0);

	    break;

	case CLASS_CLERIC:
	    if (class_value == CLASS_CLERIC)
		ch->pcdata->class_mage = CLASS_CLERIC;
	    else
		bug("set_profession: trying to set a non-cleric class for cleric class spot", 0);

	    break;

	case CLASS_THIEF:
	    if (class_value == CLASS_THIEF)
		ch->pcdata->class_mage = CLASS_THIEF;
	    else
		bug("set_profession: trying to set a non-thief class for thief class spot", 0);

	    break;

	case CLASS_WARRIOR:
	    if (class_value == CLASS_WARRIOR)
		ch->pcdata->class_mage = CLASS_WARRIOR;
	    else
		bug("set_profession: trying to set a non-warrior class for warrior class spot", 0);

	    break;

	case SUBCLASS_MAGE:
	    switch (class_value)
	    {
		case CLASS_MAGE_NECROMANCER:	ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER; 	break;
		case CLASS_MAGE_SORCERER:	ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;	break;
		case CLASS_MAGE_WIZARD:		ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;		break;
		default:
		    bug("set_profession: trying to set a non-mage subclass for mage subclass spot", 0);
		    break;
	    }

	case SUBCLASS_CLERIC:
	    switch (class_value)
	    {
		case CLASS_CLERIC_WITCH:	ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH; 	break;
		case CLASS_CLERIC_DRUID:	ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;	break;
		case CLASS_CLERIC_MONK:		ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;	break;
		default:
		    bug("set_profession: trying to set a non-cleric subclass for cleric subclass spot", 0);
		    break;
	    }

	case SUBCLASS_THIEF:
	    switch (class_value)
	    {
		case CLASS_THIEF_ASSASSIN:	ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN; 	break;
		case CLASS_THIEF_ROGUE:		ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;	break;
		case CLASS_THIEF_BARD:		ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;		break;
		default:
		    bug("set_profession: trying to set a non-thief subclass for thief subclass spot", 0);
		    break;
	    }
	case SUBCLASS_WARRIOR:
	    switch (class_value)
	    {
		case CLASS_WARRIOR_MARAUDER:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER; 	break;
		case CLASS_WARRIOR_GLADIATOR:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;	break;
		case CLASS_WARRIOR_PALADIN:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_PALADIN;		break;
		default:
		    bug("set_profession: trying to set a non-warrior subclass for warrior subclass spot", 0);
		    break;
	    }

	case SECOND_SUBCLASS_MAGE:
	    switch (class_value)
	    {
		case CLASS_MAGE_ARCHMAGE:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_ARCHMAGE; 	break;
		case CLASS_MAGE_GEOMANCER:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_GEOMANCER;	break;
		case CLASS_MAGE_ILLUSIONIST:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_ILLUSIONIST;	break;
		default:
		    bug("set_profession: trying to set a non-mage subclass for mage second subclass spot", 0);
		    break;
	    }

	case SECOND_SUBCLASS_CLERIC:
	    switch (class_value)
	    {
		case CLASS_CLERIC_ALCHEMIST:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ALCHEMIST; 	break;
		case CLASS_CLERIC_RANGER:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_RANGER;	break;
		case CLASS_CLERIC_ADEPT:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ADEPT;	break;
		default:
		    bug("set_profession: trying to set a non-cleric subclass for cleric second subclass spot", 0);
		    break;
	    }

	case SECOND_SUBCLASS_THIEF:
	    switch (class_value)
	    {
		case CLASS_THIEF_HIGHWAYMAN:	ch->pcdata->second_sub_class_thief = CLASS_MAGE_ARCHMAGE; 	break;
		case CLASS_THIEF_NINJA:		ch->pcdata->second_sub_class_thief = CLASS_THIEF_NINJA;		break;
		case CLASS_THIEF_SAGE:		ch->pcdata->second_sub_class_thief = CLASS_THIEF_SAGE;		break;
		default:
		    bug("set_profession: trying to set a non-thief subclass for thief second subclass spot", 0);
		    break;
	    }

	case SECOND_SUBCLASS_WARRIOR:
	    switch (class_value)
	    {
		case CLASS_WARRIOR_WARLORD:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_WARLORD; 	break;
		case CLASS_WARRIOR_DESTROYER:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_DESTROYER;	break;
		case CLASS_WARRIOR_CRUSADER:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_CRUSADER;	break;
		default:
		    bug("set_profession: trying to set a non-warrior subclass for warrior second subclass spot", 0);
		    break;
	    }

	default:
	    bug("set_profession: bad class_type %d", class_type);
	    return;
    }
}


// Find out what room an obj is 'in'.
ROOM_INDEX_DATA *obj_room(OBJ_DATA *obj)
{
    OBJ_DATA *container;
    CHAR_DATA *ch;

    if (!obj) return NULL;

    // In-locker and in-mail objects don't technically have a room.
    if (obj->locker || obj->in_mail)
	return NULL;

    // It's inside a container
    if ((container = obj->in_obj) != NULL)
	return obj_room(container);

    if ((ch = obj->carried_by) != NULL)
	return ch->in_room;

    if (obj->in_room != NULL)
	return obj->in_room;

    return NULL;
}

// Find out what room a token is 'in'.
ROOM_INDEX_DATA *token_room(TOKEN_DATA *token)
{
	if(token->player)
		return token->player->in_room;

/*	// Not yet!!
	if(token->obj)
		return obj_room(token->obj);

	if(token->room)
		return token->room;
*/
	return NULL;
}



void exit_name(ROOM_INDEX_DATA *room, int door, char *kwd)
{
    EXIT_DATA *ex;
    char buf[MSL];
    char article[MSL];

    if (room == NULL) {
	bug("exit_name: room was null", 0);
	return;
    }

    if ((ex = room->exit[door]) == NULL) {
	bug("exit_name: null exit", 0);
	return;
    }

    switch (ex->keyword[0]) {
	case '\0':
	    switch (door) {
		case 4:
		case 5:		sprintf(kwd, "the %swards entrance", dir_name[door]); break;
		default:	sprintf(kwd, "the %s entrance", dir_name[door]); break;
	    }
            break;

	case ' ':
	    sprintf(kwd, "%s", ex->keyword + 1);
	    break;

	default:
 	    one_argument(ex->keyword, buf);
	    switch (UPPER(buf[0]))
	    {
		case 'A':
		case 'E':
		case 'I':
		case 'O':
		case 'U':
		    sprintf(article, "an");
		    break;

		default:
		    sprintf(article, "a");
	    }

	    sprintf(kwd, "%s %s", article, buf);
	    break;
    }
}


bool can_give_obj(CHAR_DATA *ch, OBJ_DATA *obj, CHAR_DATA *victim, bool silent)
{
    if (!ch)
    {
	bug("can_give_obj: ch NULL.", 0);
	return FALSE;
    }

    if (!obj)
    {
	bug("can_give_obj: obj NULL.", 0);
	return FALSE;
    }

    if (!victim)
    {
	bug("can_give_obj: victim NULL.", 0);
	return FALSE;
    }

    if (!can_see_obj(ch, obj))
	return FALSE;

    if (obj->wear_loc != WEAR_NONE)
    {
	if (!silent)
	    act("You'll have to remove $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (IS_SET(obj->extra2_flags, ITEM_SINGULAR)
    &&  get_obj_vnum_carry(victim, obj->pIndexData->vnum, victim) != NULL)
    {
	if (!silent)
	    act("A mysterious force prevents you from giving $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (IS_NPC(victim) && victim->shop != NULL)
    {
	if (!silent)
	    act("{R$N tells you 'Sorry, you'll have to sell that.{x'", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (!can_drop_obj(ch, obj, TRUE) || IS_SET(obj->extra2_flags, ITEM_KEPT))
    {
	if (!silent)
	    send_to_char("You can't let go of it.\n\r", ch);

	return FALSE;
    }

    /*
    if (victim->carry_number + get_obj_number(obj) > can_carry_n(victim))
    {
	if (!silent)
	    act("$N has $S hands full.", ch, NULL, victim, TO_CHAR);

	return FALSE;
    }

    if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim))
	MSG(act("$N can't carry that much weight.", ch, NULL, victim, TO_CHAR))
     */

    return TRUE;
}


bool can_drop_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent)
{
    if (!ch)
    {
	bug("can_drop_obj: ch NULL.", 0);
	return FALSE;
    }

    if (!obj)
    {
	bug("can_drop_obj: obj NULL.", 0);
	return FALSE;
    }

    if (!can_see_obj(ch, obj))
	return FALSE;

/*  Syn - this shouldn't be here, since it essentially allows people to select
    which items they don't want stolen (and I'm sure there's other abuses of it too).
    Kept should be assigned specifically in functions which are called by the user
    of that item only such as give, drop, donate, etc etc
    if (IS_SET(obj->extra2_flags, ITEM_KEPT))
    {
	if (!silent)
	    act("$p has been marked for keeping. Type \"keep <item>\" to unmark it.", ch, obj, NULL, TO_CHAR);

	return FALSE;
    }
*/
    if (obj->wear_loc != WEAR_NONE)
    {
	if (!silent)
	    act("You must remove $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (!IS_NPC(ch) && ch->tot_level >= LEVEL_IMMORTAL)
	return TRUE;

    if (IS_SOCIAL(ch))
    {
	if (!silent)
	    send_to_char("You can't drop items here.\n\r", ch);

	return FALSE;
    }

    if (IS_SET(obj->extra_flags, ITEM_NODROP))
    {
	if (!silent)
	    act("You can't let go of $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    return TRUE;
}


bool can_get_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent)
{
    CHAR_DATA *gch;

    if (!ch)
    {
	bug("can_get_obj: ch NULL.", 0);
	return FALSE;
    }

    if (!obj)
    {
	bug("can_get_obj: obj NULL.", 0);
	return FALSE;
    }

    if (mail && container)
    {
	bug("can_get_obj: received mail and container", 0);
	return FALSE;
    }

    if (container && obj->in_obj != container)
    {
	bug("can_get_obj: obj not in container", 0);
	return FALSE;
    }

    if (!can_see_obj(ch, obj))
	return FALSE;

    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
	MSG(act("$p: you can't carry that many items.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR))

    if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
	MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR))

    if (IS_SET(obj->extra2_flags, ITEM_SINGULAR)
    &&  get_obj_vnum_carry(ch, obj->pIndexData->vnum, ch) != NULL)
    {
	if (!silent)
	    act("A mysterious force prevents you from picking up $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (container)
    {
	switch (container->item_type)
	{
	    default:
	        if (!silent)
		    send_to_char("That's not a container.\n\r", ch);

		return FALSE;

	    case ITEM_CART:
	    case ITEM_CONTAINER:
	    case ITEM_WEAPON_CONTAINER:
	    case ITEM_CORPSE_NPC:
	    case ITEM_CORPSE_PC:
	    case ITEM_KEYRING:
		break;
	}

	if (container->item_type == ITEM_CART && get_cart_pulled(container) != ch)
	{
	    if (!silent)
	    {
		act("You can't take items from $N's cart.", ch, get_cart_pulled(container), NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    }

	    return FALSE;
	}

	if (container->item_type == ITEM_CONTAINER
	&&  IS_SET(container->value[1], CONT_CLOSED))
	{
	    if (!silent)
		act("The $d is closed.", ch, NULL, NULL, NULL, NULL, NULL, container->name, TO_CHAR);

	    return FALSE;
	}

	if(p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,obj,NULL,TRIG_PREGET,silent?"silent":NULL))
		return FALSE;

    }

    if (mail)
    {
	if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
	    MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR))
    }

    // Get an item from the ground or from a container on the ground
    if (!container || container->carried_by != ch)
    {
	if (!IS_SET(obj->wear_flags, ITEM_TAKE) || obj->item_type == ITEM_CORPSE_PC)
	{
	    if (!silent)
		send_to_char("You can't take that.\n\r", ch);

	    return FALSE;
	}

	if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
	    MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR))

	if (obj_room(obj))
	{
	    for (gch = obj_room(obj)->people; gch != NULL; gch = gch->next_in_room)
	    {
		if (gch->on == obj)
		{
		    if (!silent)
			act("$N appears to be using $p.", ch, gch, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		    return FALSE;
		}
	    }
	}

	if (obj->item_type == ITEM_CART)
	{
	    if (!silent)
		act("$p is far too heavy.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	    return FALSE;
	}
    }

	return !p_percent_trigger(NULL,obj,NULL,NULL,ch, NULL, NULL,container,NULL,TRIG_PREGET,silent?"silent":NULL);
}


bool can_put_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent)
{
    char buf[MAX_STRING_LENGTH];
    int weight;

    if (!ch)
    {
	bug("can_put_obj: ch NULL.", 0);
	return FALSE;
    }

    if (!obj)
    {
	bug("can_put_obj: obj NULL.", 0);
	return FALSE;
    }

    if (!container && !mail)
    {
	bug("can_put_obj: container AND mail NULL.", 0);
	return FALSE;
    }

    if (container && mail)
    {
	bug("can_put_obj: received container and mail", 0);
	return FALSE;
    }

    if (obj->wear_loc != WEAR_NONE)
    {
	if (!silent)
	    send_to_char("You must remove it first.\n\r", ch);

	return FALSE;
    }

    if (container)
    {
	// To put it in a container on the ground you must be able to drop it
	if (container->carried_by != ch && (!can_drop_obj(ch, obj, silent) || IS_SET(obj->extra2_flags, ITEM_KEPT))) {
	    if (!silent)
		send_to_char("You can't let go of it.\n\r", ch);
	    return FALSE;
	}

	if (container->item_type != ITEM_CONTAINER
	&&  container->item_type != ITEM_WEAPON_CONTAINER
	&&  container->item_type != ITEM_CART
	&&  container->item_type != ITEM_KEYRING
	&&  container->pIndexData->vnum != OBJ_VNUM_CURSED_ORB)
	{
	    if (!silent)
		send_to_char("That's not a container or cart.\n\r", ch);

	    return FALSE;
	}

	if (container->item_type != ITEM_CART
	&&  container->item_type != ITEM_WEAPON_CONTAINER
	&&  IS_SET(container->value[1], CONT_CLOSED))
	{
	    if (!silent)
		act("$p is closed.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
	    return FALSE;
	}

	if (obj == container)
	{
	    if (!silent)
		send_to_char("You can't fold it into itself.\n\r", ch);

	    return FALSE;
	}

	if (obj->item_type == ITEM_CONTAINER
	||  obj->item_type == ITEM_WEAPON_CONTAINER)
	{
	    if (!silent)
		send_to_char("You can't put containers inside another container.\n\r", ch);

	    return FALSE;
	}

	if (IS_SET(obj->extra2_flags, ITEM_NO_CONTAINER) ||
	    (IS_SET(obj->extra_flags, ITEM_NOUNCURSE) && IS_SET(obj->extra_flags, ITEM_NODROP)))
	{
	    if (!silent)
		act("You can't put $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);

	    return FALSE;
	}

	if (WEIGHT_MULT(obj) != 100)
	    return FALSE;

        /*
	if (get_obj_weight(obj) + (get_obj_weight_container(container) * WEIGHT_MULT(container))/100
		> (container->value[0])
		||  (get_obj_number_container(container) >= container->value[3]))
	{
	    act("$p won't fit in $P.", ch, obj, container, TO_CHAR);
	    return FALSE;
	}
	*/

	if (container->item_type == ITEM_WEAPON_CONTAINER
	&&  container->value[1] != obj->value[0])
	{
	    if (!silent)
	    {
		sprintf(buf, "This container will only take %ss.\n\r",
			weapon_name(container->value[1]));
		send_to_char(buf, ch);
	    }

	    return FALSE;
	}
    }
    else
    {
	if (obj->timer > 0)
	{
	    if (!silent)
		act("You can't send $p through the mail.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	    return FALSE;
	}

	if ((obj->pIndexData->vnum == OBJ_VNUM_SKULL || obj->pIndexData->vnum == OBJ_VNUM_GOLD_SKULL)
	&&   obj->affected != NULL)
	{
	    if (!silent)
		send_to_char("You can't send that enchanted item through the mail.\n\r", ch);

	    return FALSE;
	}

	if (IS_SET(obj->extra_flags, ITEM_NOUNCURSE) && IS_SET(obj->extra_flags, ITEM_NODROP))
	{
	    if (!silent)
		act("You can't let go of $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	    return FALSE;
	}

	weight = get_obj_weight(obj);
	if (weight > MAX_POSTAL_WEIGHT)
	{
	    if (!silent)
		send_to_char("Sorry, that item is far too heavy to send through the mail.\n\r", ch);

	    return FALSE;
	}

	if (count_weight_mail(mail) + weight > MAX_POSTAL_WEIGHT)
	    MSG(send_to_char("Postal weight limit has been reached.\n\r", ch))

	if (count_items_list_nest(mail->objects)
	    + count_items_list_nest(obj->contains) + 1 > MAX_POSTAL_ITEMS)
	{
	    sprintf(buf, "Postal limit of %d items per package has been reached.\n\r", MAX_POSTAL_ITEMS);
	    MSG(send_to_char(buf, ch))
	}

    }

    return !p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,obj,NULL,TRIG_PREPUT,silent?"silent":NULL);
}


bool can_sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent)
{
    CHAR_DATA *gch;

    if (!can_see_obj(ch, obj))
	return FALSE;

    if ((!IS_SET(obj->wear_flags, ITEM_TAKE) && obj->item_type != ITEM_CORPSE_NPC && obj->item_type != ITEM_CORPSE_PC)
    ||  IS_SET(obj->wear_flags, ITEM_NO_SAC)
    ||  (obj->item_type == ITEM_CORPSE_PC && obj->contains))
    {
	if (!silent)
	    act("$p is not an acceptable sacrifice.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if ((obj->item_type == ITEM_CORPSE_NPC || obj->item_type == ITEM_CORPSE_PC)
    && obj->contains && !IS_SET(ch->act2, PLR_SACRIFICE_ALL))
    {
	if (!silent)
	    act("You must rid $p of its belongings before sacrificing it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	return FALSE;
    }

    if (obj_room(obj))
    {
	for (gch = obj_room(obj)->people; gch != NULL; gch = gch->next_in_room)
	{
	    if (gch->on == obj)
	    {
		if (!silent)
		    act("$N appears to be using $p.", ch, gch, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		return FALSE;
	    }

	}
    }

    return TRUE;
}

CHAR_DATA* get_player(char *name)
{
	ITERATOR it;
    CHAR_DATA *ch = NULL;

	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (!IS_NPC(ch) && !str_cmp(ch->name, name))
			break;
	}
	iterator_stop(&it);
    return ch;
}

// get reputation for player
sh_int  get_player_reputation args( ( int reputation_points ) )
{
	int i = 0;
	int reputation_type = 0;
	while (rating_table[i].name != NULL) {
		if (reputation_points >= rating_table[i].points) {
			reputation_type = rating_table[i].type;
		}
		i++;
	}
	return reputation_type;
}

int get_coord_distance( int x1, int y1, int x2, int y2 ) {
  int distance = 0;
		 distance = (int) sqrt( 					\
							( x1 - x2 ) *	\
							( x1 - x2 ) +	\
							( y1 - y2 ) *	\
							( y1 - y2 ) );

  return distance;
}


// Find the closest saferoom around a player. Used when repopping people into churches.
ROOM_INDEX_DATA *find_safe_room(ROOM_INDEX_DATA *from_room, int depth, bool crossarea)
{
    int door;
    ROOM_INDEX_DATA *room = NULL;

    if (depth == 0)
	return NULL;

    if (IS_SET(from_room->room_flags, ROOM_SAFE))
	return from_room;

    for (door = 0; door < MAX_DIR; door++)
    {
	if (from_room->exit[door] != NULL
	&&  from_room->exit[door]->u1.to_room != NULL)
	{
	    if (!crossarea && from_room->exit[door]->u1.to_room->area != from_room->area)
		;
	    else
		room = find_safe_room(from_room->exit[door]->u1.to_room, depth - 1, crossarea);
	}

	if (room != NULL)
	    return room;
    }

    return NULL;
}


// Checks if there is a PC or wilds wandering NPC in a room. Used in managing wilderness exits.
bool can_clear_exit(ROOM_INDEX_DATA *room)
{
    CHAR_DATA *rch;

    for (rch = room->people; rch != NULL; rch = rch->next_in_room) {
	if (!IS_NPC(rch) || IS_SET(rch->act2, ACT2_WILDS_WANDERER))
	    return FALSE;
    }

    return TRUE;
}

TOKEN_DATA *create_token(TOKEN_INDEX_DATA *token_index)
{
	TOKEN_DATA *token;
	int i;

	token = new_token();
	token->pIndexData = token_index;
	token->type = token_index->type;
	token->name = str_dup(token_index->name);
	token->description = str_dup(token_index->description);
	token->flags = token_index->flags;
	token->timer = token_index->timer;
	token->progs = new_prog_data();
	token->progs->progs = token_index->progs;
	token_index->loaded++;	// @@@NIB : 20070127 : for "tokenexists" ifcheck
	token->id[0] = token->id[1] = 0;
	token->global_next = global_tokens;
	global_tokens = token;

	get_token_id(token);

	variable_copylist(&token_index->index_vars,&token->progs->vars,FALSE);

	for (i = 0; i < MAX_TOKEN_VALUES; i++)
		token->value[i] = token_index->value[i];

	return token;
}


/* set up a token and give it to a char */
TOKEN_DATA *give_token(TOKEN_INDEX_DATA *token_index, CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room)
{
	TOKEN_DATA *token;

	if(!ch && !obj && !room) return NULL;

	if( (ch && obj) || (ch && room) || (obj && room) ) return NULL;

	token = create_token(token_index);

	if(ch)
		token_to_char(token, ch);
	else if(obj)
		token_to_obj(token, obj);
	else if(room)
		token_to_room(token, room);

	return token;
}


void token_from_char(TOKEN_DATA *token)
{
	TOKEN_DATA *token_tmp, *token_prev;
	char buf[MSL];

	if (token->player == NULL) {
		bug("token_from_char: called on token with no player", 0);
		return;
	}

	token_prev = NULL;
	for (token_tmp = token->player->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
		if (token_tmp == token)
			break;

		token_prev = token_tmp;
	}

	if(token->player->cast_token == token)
		stop_casting(token->player,TRUE);

	if(token->player->script_wait_token == token)
		script_end_failure(token->player, TRUE);

	if(token->type == TOKEN_SKILL) skill_entry_removeskill(token->player, 0, token);
	else if(token->type == TOKEN_SPELL) skill_entry_removespell(token->player, 0, token);
	else if(token->type == TOKEN_SONG) skill_entry_removesong(token->player, -1, token);

	sprintf(buf, "token_from_char: removed token %s(%ld) from char %s(%ld)",
		token->name, token->pIndexData->vnum,
		HANDLE(token->player), IS_NPC(token->player) ? token->player->pIndexData->vnum : 0);
	log_string(buf);

	list_remlink(token->player->ltokens, token);

	if (token_prev == NULL)
		token_tmp->player->tokens = token_tmp->next;
	else
		token_prev->next = token->next;

	token->player = NULL;
}

/* transfers a token to a char */
void token_to_char_ex(TOKEN_DATA *token, CHAR_DATA *ch, char source, long flags)
{
	char buf[MSL];

	if (token == NULL || ch == NULL) {
		bug("token_to_char: NULL", 0);
		return;
	}

	token->player = ch;
	token->object = NULL;
	token->room = NULL;
	token->next = ch->tokens;
	ch->tokens = token;

	list_addlink(ch->ltokens, token);

	// Do sorted lists
	if(token->type == TOKEN_SKILL) skill_entry_addskill(token->player, 0, token, source, flags);
	else if(token->type == TOKEN_SPELL) skill_entry_addspell(token->player, 0, token, source, flags);
	else if(token->type == TOKEN_SONG) skill_entry_addsong(token->player,-1,token, source);

	sprintf(buf, "token_to_char: gave token %s(%ld) to char %s(%ld)",
		token->name, token->pIndexData->vnum,
		HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
	log_string(buf);
}

void token_to_char(TOKEN_DATA *token, CHAR_DATA *ch)
{
	token_to_char_ex(token, ch, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);
}

TOKEN_DATA *get_token_list(LLIST *tokens, long vnum, int count)
{
	TOKEN_DATA *token;
	ITERATOR it;

	count = UMAX(1,count);

	iterator_start(&it, tokens);
	while( (token = (TOKEN_DATA*)iterator_nextdata(&it)) ) {
		if( IS_VALID(token) && token->pIndexData && token->pIndexData->vnum == vnum && !--count )
			break;
	}
	iterator_stop(&it);

	return token;
}

/* finds a token on an object given the vnum */
TOKEN_DATA *get_token_char(CHAR_DATA *ch, long vnum, int count)
{
	return get_token_list(ch->ltokens, vnum, count);
}

void token_from_obj(TOKEN_DATA *token)
{
	TOKEN_DATA *token_tmp, *token_prev;
	char buf[MSL];

	if (token->object == NULL) {
		bug("token_from_obj: called on token with no object", 0);
		return;
	}

	token_prev = NULL;
	for (token_tmp = token->object->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
		if (token_tmp == token)
			break;

		token_prev = token_tmp;
	}

	sprintf(buf, "token_from_obj: removed token %s(%ld) from object %s(%ld)",
		token->name, token->pIndexData->vnum, token->object->short_descr, VNUM(token->object));
	log_string(buf);

	list_remlink(token->object->ltokens, token);

	if (token_prev == NULL)
		token_tmp->object->tokens = token_tmp->next;
	else
		token_prev->next = token->next;

	token->object = NULL;
}


/* transfers a token to an object */
void token_to_obj(TOKEN_DATA *token, OBJ_DATA *obj)
{
	char buf[MSL];

	if (token == NULL || obj == NULL) {
		bug("token_to_obj: NULL", 0);
		return;
	}

	token->player = NULL;
	token->object = obj;
	token->room = NULL;
	token->next = obj->tokens;
	obj->tokens = token;

	list_addlink(obj->ltokens, token);

	sprintf(buf, "token_to_obj: gave token %s(%ld) to object %s(%ld)",
		token->name, token->pIndexData->vnum, obj->short_descr, VNUM(obj));
	log_string(buf);
}


/* finds a token on a char given the vnum */
TOKEN_DATA *get_token_obj(OBJ_DATA *obj, long vnum, int count)
{
	return get_token_list(obj->ltokens, vnum, count);
}

void token_from_room(TOKEN_DATA *token)
{
	TOKEN_DATA *token_tmp, *token_prev;
	char buf[MSL];

	if (token->room == NULL) {
		bug("token_from_room: called on token with no room", 0);
		return;
	}

	token_prev = NULL;
	for (token_tmp = token->room->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
		if (token_tmp == token)
			break;

		token_prev = token_tmp;
	}

	if( token->room->wilds )
		sprintf(buf, "token_from_room: removed token %s(%ld) from vroom <%ld, %ld, %ld>",
			token->name, token->pIndexData->vnum, token->room->wilds->uid, token->room->x, token->room->y);
	else if( token->room->source )
		sprintf(buf, "token_from_room: removed token %s(%ld) from croom %s(%ld %08lX:%08lX)",
			token->name, token->pIndexData->vnum, token->room->name, token->room->source->vnum, token->room->id[0], token->room->id[1]);
	else
		sprintf(buf, "token_from_room: removed token %s(%ld) from room %s(%ld)",
			token->name, token->pIndexData->vnum, token->room->name, token->room->vnum);

	sprintf(buf, "token_from_obj: removed token %s(%ld) from room %s(%ld)",
		token->name, token->pIndexData->vnum, token->room->name, token->room->vnum);
	log_string(buf);

	list_remlink(token->object->ltokens, token);

	if (token_prev == NULL)
		token_tmp->object->tokens = token_tmp->next;
	else
		token_prev->next = token->next;

	token->room = NULL;
}


/* transfers a token to a room*/
void token_to_room(TOKEN_DATA *token, ROOM_INDEX_DATA *room)
{
	char buf[MSL];

	if (token == NULL || room == NULL) {
		bug("token_to_room: NULL", 0);
		return;
	}

	token->player = NULL;
	token->object = NULL;
	token->room = room;
	token->next = room->tokens;
	room->tokens = token;

	list_addlink(room->ltokens, token);

	if( room->wilds )
		sprintf(buf, "token_to_room: gave token %s(%ld) to vroom <%ld, %ld, %ld>",
			token->name, token->pIndexData->vnum, room->wilds->uid, room->x, room->y);
	else if( room->source )
		sprintf(buf, "token_to_room: gave token %s(%ld) to croom %s(%ld %08lX:%08lX)",
			token->name, token->pIndexData->vnum, room->name, room->source->vnum, room->id[0], room->id[1]);
	else
		sprintf(buf, "token_to_room: gave token %s(%ld) to room %s(%ld)",
			token->name, token->pIndexData->vnum, room->name, room->vnum);
	log_string(buf);
}


/* finds a token on a char given the vnum */
TOKEN_DATA *get_token_room(ROOM_INDEX_DATA *room, long vnum, int count)
{
	return get_token_list(room->ltokens, vnum, count);
}



/* Syn - this function fixes the problems with staves, potions, scrolls, and wands.
   It takes the spell numbers (previously stored in the v0-v9 values) and changes them
   into spell data structs on the object. */
void fix_magic_object_index(OBJ_INDEX_DATA *obj)
{
    int val;
    SPELL_DATA *spell, *spell_tmp;
    bool already_has_spell;
    char buf[MSL];

    /* scrolls, potions, and pills had level in v0, and spells in v1 onwards */
    if (obj->item_type == ITEM_SCROLL
    ||  obj->item_type == ITEM_POTION
    ||  obj->item_type == ITEM_PILL) {
	for (val = 1; val < 8; val++) {
	    if (obj->value[val] > 0) {
		 /* Don't put the same spell on twice. */
		 already_has_spell = FALSE;
		 for (spell = obj->spells; spell != NULL; spell = spell->next) {
		     if (spell->sn == obj->value[val])
			 already_has_spell = TRUE;
		 }

		 if (already_has_spell)
		     continue;

		 spell 		= new_spell();
		 spell->sn	= obj->value[val];
		 spell->level	= obj->value[0];
		 spell->repop	= 100; // Assuming 100 on objects made before rand was implemented
		 spell->next     = NULL;
		 if (!str_cmp(skill_table[spell->sn].name, "none"))
		     free_spell(spell);
		 else {
		     if (obj->spells == NULL)
			 obj->spells = spell;
		     else
		     {
			 for (spell_tmp = obj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
			     ;

			 spell_tmp->next = spell;
		     }

		     sprintf(buf, "Obj %s (%ld): Added spell %s, level %d, random %d.\n\r",
			     obj->short_descr, obj->vnum,
			     skill_table[obj->value[val]].name, spell->level, spell->repop);
		     log_string(buf);
		 }

		 obj->value[val] = 0; // Reset val to 0 since it won't any longer be needed
	    }
	}
    }

    /* staves and wands had level in v0, total charges in v1, initial charges in v2, spells in v3 onwards */
    if (obj->item_type == ITEM_WAND
    ||  obj->item_type == ITEM_STAFF) {
         for (val = 3; val < 8; val++) {
	     if (obj->value[val] > 0) {
		 /* Don't put the same spell on twice. */
		 already_has_spell = FALSE;
		 for (spell = obj->spells; spell != NULL; spell = spell->next) {
		     if (spell->sn == obj->value[val])
			 already_has_spell = TRUE;
		 }

		 if (already_has_spell)
		     continue;

		 spell 		= new_spell();
		 spell->sn	= obj->value[val];
		 spell->level	= obj->value[0];
		 spell->repop	= 100; // Assuming 100 on objects made before rand was implemented
		 spell->next     = NULL;

		 if (!str_cmp(skill_table[spell->sn].name, "none"))
		     free_spell(spell);
		 else {
		     if (obj->spells == NULL)
			 obj->spells = spell;
		     else
		     {
			 for (spell_tmp = obj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
			     ;

			 spell_tmp->next = spell;
		     }

		     sprintf(buf, "Obj %s (%ld): Added spell %s, level %d, random %d.\n\r",
			     obj->short_descr, obj->vnum,
			     skill_table[obj->value[val]].name, spell->level, spell->repop);
		     log_string(buf);
		 }
		 obj->value[val] = 0;
	     }
	 }
    }
}


/* Extracts an event, freeing it from all linked lists. Made it a function since it's
   used in more than one place. */
void extract_event(EVENT_DATA *ev)
{
	EVENT_DATA *ev_last, *ev_temp; /* For removing from global list */
	EVENT_DATA *ev_entity_last, *ev_entity_temp; /* For removing from entity list */
	EVENT_DATA **ev_head, **ev_tail;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;

	/* Remove from global list */

	/* Find it in the list first
	"*_next = *->next saving is not required here since we're not modifying structures within the for-loop */
	ev_last = NULL;
	for (ev_temp = events; ev_temp != NULL; ev_temp = ev_temp->next) {
		if (ev_temp == ev)
			break;

		ev_last = ev_temp;
	}

	if (ev_last != NULL)
		ev_last->next = ev_temp->next;
	else
		events = ev_temp->next;

	if( events_tail == ev_temp )
	{
		if( ev_last )
			events_tail = ev_last;
		else
			events_tail = NULL;
	}

	/* Remove from entity list */

	/* Figure out which type of entity we are using */
	switch (ev->event_type) {
	case EVENT_MOBQUEUE:
		ch = (CHAR_DATA *) ev->entity;
		ev_head = &ch->events;
		ev_tail = &ch->events_tail;
		break;

	case EVENT_OBJQUEUE:
		obj = (OBJ_DATA *) ev->entity;
		ev_head = &obj->events;
		ev_tail = &obj->events_tail;
		break;

	case EVENT_ROOMQUEUE:
	case EVENT_ECHO:
		room = (ROOM_INDEX_DATA *) ev->entity;
		ev_head = &room->events;
		ev_tail = &room->events_tail;
		break;

	case EVENT_TOKENQUEUE:
		token = (TOKEN_DATA *) ev->entity;
		ev_head = &token->events;
		ev_tail = &token->events_tail;
		break;
	default:
		ev_head = NULL;
		ev_tail = NULL;
		break;
	}

	ev_entity_last = NULL;
	/* Locate it in the list and re-link the list */
	if (ev_head && ev_tail) {
		for (ev_entity_temp = *ev_head; ev_entity_temp; ev_entity_temp = ev_entity_temp->next_event) {
			if (ev_entity_temp == ev)
				break;

			ev_entity_last = ev_entity_temp;
		}

		if (ev_entity_last)
			ev_entity_last->next_event = ev_entity_temp->next_event;
		else
			*ev_head = ev_entity_temp->next_event;

		if( *ev_tail == ev_entity_temp )
			*ev_tail = ev_entity_last;
		else
			*ev_tail = NULL;

	}

	free_event(ev);
}



void extract_project_inquiry(PROJECT_INQUIRY_DATA *pinq)
{
     PROJECT_DATA *project;
     PROJECT_INQUIRY_DATA *pinq_tmp, *pinq_tmp_last;

     project = pinq->project;

     /* Remove from project */
     pinq_tmp_last = NULL;
     for (pinq_tmp = project->inquiries; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next) {
	 if (pinq_tmp == pinq)
	     break;

	 pinq_tmp_last = pinq_tmp;
     }

     if (!pinq_tmp_last)
	 project->inquiries = pinq_tmp->next;
     else
	 pinq_tmp_last->next = pinq_tmp->next;


     /* Remove from global */
     pinq_tmp_last = NULL;
     for (pinq_tmp = project_inquiry_list; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next_global) {
	 if (pinq_tmp == pinq)
	     break;

	 pinq_tmp_last = pinq_tmp;
     }

     if (!pinq_tmp_last)
	 project_inquiry_list = pinq_tmp->next_global;
     else
	 pinq_tmp_last->next_global = pinq_tmp->next_global;

     free_project_inquiry(pinq);
}


/* Syn - This function uses the simple "log_entry" data structure to add a string
   to a list of logs. I wrote it to store info in the project system that's available
   from within the game.

   You can feel free to adapt it to anything else you want logged with a linked
   list.

   Instead of using some sort
   of buffer to store log entries, this implementation uses a linked list. If the number
   of log entries in the list is the max (MAX_LOG_ENTRIES), the oldest entry is taken
   off and saved to a file, and the new entry is added to the head of the list.
   This allows immortals to view log information in-game without having to tie up
   tons of memory by storing EVERYTHING that has happened since last boot.

   Note: this is not the most efficient system to use for logs where a MASSIVE
   amount of text (i.e. several new log entries added per second) since this means
   either a lot of performance draining saving or the risk of losing log information.
   But for this purpose, where log entries are added relatively sporadically, it
   works perfectly fine.
 */
void log_string_to_list(char *argument, LOG_ENTRY_DATA *list)
{
}


/* This function saves all logs to their files periodically. If you want to use my
   log entry list, be sure to add it in here. */
void save_logs()
{
}



// @@@NIB : 20070120 : Totals up the current value particular stat for all
//			people in the current room that are in the same
//			group as the target.
int get_curr_group_stat(CHAR_DATA *ch, int stat)
{
	CHAR_DATA *rch;
	int sum = 0;

	if(!ch->in_room) return 0;	// Since all stats have a minimum, this would be an ERROR

	for(rch = ch->in_room->people;rch; rch = rch->next_in_room)
		if(ch == rch || is_same_group(ch,rch))
			sum += get_curr_stat(rch,stat);

	return sum;
}

// @@@NIB : 20070120 : Totals up the base value particular stat for all
//			people in the current room that are in the same
//			group as the target.
int get_perm_group_stat(CHAR_DATA *ch, int stat)
{
	CHAR_DATA *rch;
	int sum = 0;

	if(!ch->in_room) return 0;	// Since all stats should have a minimum, this would be an ERROR

	for(rch = ch->in_room->people;rch; rch = rch->next_in_room)
		if(ch == rch || is_same_group(ch,rch))
			sum += rch->perm_stat[stat];

	return sum;
}

// @@@NIB : 20070120 : Counts the number of members online that belong
//			in the same church as the target.  If the target
//			is a mobile or churchless, the count will be zero.
int get_church_online_count(CHAR_DATA *ch)
{
//	DESCRIPTOR_DATA *d;

	if(ch && !IS_NPC(ch) && ch->church)
		return list_size(ch->church->online_players);


	return 0;
}

// @@@NIB : 20070120 : Totals up the weight of either the people or
//			objects, or both, in the specified room.
// @@@NIB : 20070121 : Added a 'ground' flag to exclude flying/floating entities
// @@@ASH : 20111231 : Changed the flying/float check to use the flying check function
int get_room_weight(ROOM_INDEX_DATA *room, bool mobs, bool objs, bool ground)
{
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	int weight = 0;

	if(!room) return 0;

	if(mobs) for(ch = room->people; ch; ch = ch->next_in_room) {
		if(ground && mobile_is_flying(ch))
	    		continue;

		weight += get_carry_weight(ch);
	}

	if(objs) for(obj = room->contents; obj; obj = obj->next_content)
		weight += get_obj_weight(obj);

	return weight;
}

// @@@NIB : 20070121 : Common function for checking if the mobile has
//			a "float_user" object worn
bool is_float_user(CHAR_DATA *ch)
{
	OBJ_DATA *obj;

	if(!ch) return FALSE;

	for (obj = ch->carrying; obj; obj = obj->next_content)
	    if (IS_SET(obj->extra2_flags, ITEM_FLOAT_USER) && obj->wear_loc != WEAR_NONE)
		return TRUE;

	return FALSE;
}


// 20070521 : NIB : Function to see if CH has the desired catalyst or not
int has_catalyst(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int method,int min_strength, int max_strength)
{
	int total;
	OBJ_DATA *obj, *next;
	OBJ_DATA *objNest, *nextNest;
	AFFECT_DATA *aff;

	// For now, it just checks to see if it has WARP_STONES...  CHECK: fixed to check any catalyst type
	// Fix to allow multicharged catalysts...  CHECK: utilizes multiple charges
	// Allow for multityped catalysts, using catalyst affects
	// Add for "CATALYST_HERE" for doing room level catalysts

	if(!ch && !room) return 0;

	if(!ch && !IS_SET(method,CATALYST_ROOM)) return 0;

	if(!room) room = ch->in_room;

	total = 0;

	if(ch) {
		for (obj = ch->carrying; obj; obj = next) {
			next = obj->next_content;
			if((IS_SET(method,CATALYST_HOLD) && obj->wear_loc == WEAR_HOLD) ||
				(IS_SET(method,CATALYST_WORN) && obj->wear_loc != WEAR_NONE) ||
				IS_SET(method,(CATALYST_CARRY))) {
					for(aff = obj->catalyst; aff; aff = aff->next) if(aff->level >= min_strength && aff->level <= max_strength && aff->type == type) {
						if( IS_SET(method, CATALYST_ACTIVE) && (aff->where != TO_CATALYST_ACTIVE))
							continue;

						if(aff->duration < 0) return -1;	// Negative is treated as a "source"

						total += aff->duration;

					}
			} else if(obj->contains && IS_SET(method,CATALYST_CONTAINERS)) {	/* look in bags too */
				for (objNest = obj->contains; objNest; objNest = nextNest) {
					nextNest = objNest->next_content;
					for(aff = objNest->catalyst; aff; aff = aff->next) if(aff->level >= min_strength && aff->level <= max_strength && aff->type == type) {
						if( IS_SET(method, CATALYST_ACTIVE) && (aff->where != TO_CATALYST_ACTIVE))
							continue;
						if(aff->duration < 0) return -1;	// Negative is treated as a "source"

						total += aff->duration;

					}
				}
			}
		}
	}

	if(IS_SET(method,CATALYST_ROOM)) {
		for(obj = room->contents; obj; obj = obj->next_content) {
			for(aff = obj->catalyst; aff; aff = aff->next) if(aff->level >= min_strength && aff->level <= max_strength && aff->type == type) {
					if( IS_SET(method, CATALYST_ACTIVE) && (aff->where != TO_CATALYST_ACTIVE))
						continue;
				if(aff->duration < 0) return -1;	// Negative is treated as a "source"
				total += aff->duration;
			}
			for (objNest = obj->contains; objNest; objNest = nextNest) {
				nextNest = objNest->next_content;
				for(aff = objNest->catalyst; aff; aff = aff->next) if(aff->level >= min_strength && aff->level <= max_strength && aff->type == type) {
					if( IS_SET(method, CATALYST_ACTIVE) && (aff->where != TO_CATALYST_ACTIVE))
						continue;
					if(aff->duration < 0) return -1;	// Negative is treated as a "source"
						total += aff->duration;
				}
			}
		}
	}

	return total;
}

int use_catalyst_obj(CHAR_DATA *ch,ROOM_INDEX_DATA *room,OBJ_DATA *obj,int type,int left,int min_strength, int max_strength, bool active, bool show)
{
	bool used;
	int total = 0;
	AFFECT_DATA *aff, *prev, *next;

	if(!obj) return 0;

	if(!ch && !room) room = obj_room(obj);

	if(!room) return 0;

	used = FALSE;
	for(prev = NULL, aff = obj->catalyst; aff && total < left; aff = next) {
		next = aff->next;
		if(aff->level >= min_strength && aff->level <= max_strength && aff->type == type) {
			if( active && (aff->where != TO_CATALYST_ACTIVE) ) continue;

			if(aff->duration < 0) {
				if(show && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST_SOURCE, NULL))
					act("$p pulsates brightly.",room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
				return -1;
			}

			if(total + aff->duration <= left) {
				total += aff->duration;
				if(prev) prev->next = next;
				else obj->catalyst = next;

				free_affect(aff);

				if(!obj->catalyst) {	// All catalyst affects have been exhausted
					if(show && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST_FULL, NULL) && ch)
						act("$p flares brightly and vanishes!",room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
					extract_obj(obj);
					return total;
				}
			} else {
				aff->duration -= left - total;
				total = left;
			}

			used = TRUE;
		}
	}

	if(show && used && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST, NULL)) {
		act("$p shimmers brightly, but only dims back to normal.",room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
	}
	return total;
}

int use_catalyst_here(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int amount,int min_strength, int max_strength, bool active, bool show)
{
	int total = 0, total2;
	OBJ_DATA *obj;
	OBJ_DATA *objNest, *nextNest;

	if(!ch && !room) return 0;

	if(!room) room = ch->in_room;

	for(obj = room->contents; obj && total < amount; obj = obj->next_content) {
		total2 = use_catalyst_obj(ch,room,obj,type,amount - total,min_strength,max_strength,active,show);
		if(total2 < 0) return -1;

		total += total2;
		for (objNest = obj->contains; objNest && total < amount; objNest = nextNest) {
			nextNest = objNest->next_content;
			total2 = use_catalyst_obj(ch,room,objNest,type,amount - total,min_strength,max_strength,active,show);
			if(total2 < 0) return -1;

			total += total2;
		}
	}

	return total;
}

int use_catalyst(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int method,int amount,int min_strength, int max_strength, bool show)
{
	int total, total2;
	OBJ_DATA *obj, *next;
	OBJ_DATA *objNest, *nextNest;
	bool active;

	if(!ch && !room) return 0;

 	if(!room) room = ch->in_room;

	if(!room) return 0;

	total = 0;
	active = IS_SET(method, CATALYST_ACTIVE);

	for (obj = ch->carrying; obj && total < amount; obj = next) {
		next = obj->next_content;
		if((IS_SET(method,CATALYST_HOLD) && obj->wear_loc == WEAR_HOLD) ||
			(IS_SET(method,CATALYST_WORN) && obj->wear_loc != WEAR_NONE) ||
			IS_SET(method,(CATALYST_CARRY))) {
				total2 = use_catalyst_obj(ch,room,obj,type,amount - total,min_strength,max_strength,active,show);
				if(total2 < 0) return amount;

				total += total2;
		} else if(obj->contains && IS_SET(method,CATALYST_CONTAINERS)) {	/* look in bags too */
			for (objNest = obj->contains; objNest; objNest = nextNest) {
				nextNest = objNest->next_content;
				total2 = use_catalyst_obj(ch,room,objNest,type,amount - total,min_strength,max_strength,active,show);
				if(total2 < 0) return amount;

				total += total2;
			}
		}
	}

	if(IS_SET(method,CATALYST_ROOM) && total < amount) {
		int htotal = use_catalyst_here(ch,room,type,amount - total,min_strength,max_strength,active,show);
		if(htotal < 0) return amount;
		total += htotal;
	}

	return total;
}

void move_cart(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool delay)
{
	if (PULLING_CART(ch))
	{
		obj_from_room(PULLING_CART(ch));

		if(room->wilds)
			obj_to_vroom(PULLING_CART(ch), room->wilds, room->x, room->y);
		else
			obj_to_room(PULLING_CART(ch), room);

		if(delay) {
			int wait_amount;

			wait_amount = PULLING_CART(ch)->value[1];

			if (MOUNTED(ch))
				WAIT_STATE(ch, wait_amount/2);
			else
				WAIT_STATE(ch, wait_amount);
		}
	}
}

unsigned long last_visited_room = 0;

void visit_room_recurse(LLIST *visited, ROOM_INDEX_DATA *room, VISIT_FUNC *func, int depth, void *argv[], int argc, bool closed, int door)
{
	EXIT_DATA *ex;
	int i;

	if(!list_hasdata(visited, room)) {
		list_appendlink(visited, room);
		if((*func)(room,argv,argc,depth,door) && depth > 0) {
			for(i = 0; i < MAX_DIR; i++) if((ex = room->exit[i])) {
				if(ex->u1.to_room && (!closed || !IS_SET(ex->exit_info, EX_CLOSED)))
					visit_room_recurse(visited, ex->u1.to_room, func, depth - 1, argv, argc, closed, i);
			}
		}
	}
}

void visit_rooms(ROOM_INDEX_DATA *room, VISIT_FUNC *func, int depth, void *argv[], int argc, bool closed)
{
	LLIST *visited = list_create(FALSE);

	visit_room_recurse(visited, room,func,depth,argv,argc,closed,MAX_DIR);

	list_destroy(visited);
}

bool char_exists(CHAR_DATA *ch)
{
	CHAR_DATA *cur;
	ITERATOR it;

	iterator_start(&it, loaded_chars);
	while(( cur = (CHAR_DATA *)iterator_nextdata(&it)))
		if( cur == ch )
			break;
	iterator_stop(&it);
	return (cur != NULL);
}


CHAR_DATA *idfind_mobile(register unsigned long id1, register unsigned long id2)
{
	register CHAR_DATA *ch;
	ITERATOR it;

	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
		if(ch->id[0] == id1 && ch->id[1] == id2)
			break;
	iterator_stop(&it);
	return ch;
}

CHAR_DATA *idfind_player(register unsigned long id1, register unsigned long id2)
{
	register CHAR_DATA *ch;
	ITERATOR it;

	iterator_start(&it, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
		if(!IS_NPC(ch) && ch->id[0] == id1 && ch->id[1] == id2)
			break;
	iterator_stop(&it);
	return ch;
}

OBJ_DATA *idfind_object(register unsigned long id1, register unsigned long id2)
{
	register OBJ_DATA *obj;
	ITERATOR it;

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
		if(obj->id[0] == id1 && obj->id[1] == id2)
			break;
	iterator_stop(&it);

	return obj;
}

TOKEN_DATA *idfind_token(register unsigned long id1, register unsigned long id2)
{
	register TOKEN_DATA *token;
	for (token = global_tokens; token; token = token->global_next)
		if(token->id[0] == id1 && token->id[1] == id2)
			return token;
	return NULL;
}

TOKEN_DATA *idfind_token_char(CHAR_DATA *ch, register unsigned long id1, register unsigned long id2)
{
	register TOKEN_DATA *token;

	if( !ch ) return FALSE;

	for (token = ch->tokens; token; token = token->next)
		if(token->id[0] == id1 && token->id[1] == id2)
			return token;
	return NULL;
}

TOKEN_DATA *idfind_token_object(OBJ_DATA *obj, register unsigned long id1, register unsigned long id2)
{
	register TOKEN_DATA *token;

	if( !obj ) return FALSE;

	for (token = obj->tokens; token; token = token->next)
		if(token->id[0] == id1 && token->id[1] == id2)
			return token;
	return NULL;
}

TOKEN_DATA *idfind_token_room(ROOM_INDEX_DATA *room, register unsigned long id1, register unsigned long id2)
{
	register TOKEN_DATA *token;

	if( !room ) return FALSE;

	for (token = room->tokens; token; token = token->next)
		if(token->id[0] == id1 && token->id[1] == id2)
			return token;
	return NULL;
}

bool string_vector_add(STRING_VECTOR **head, char *key, char *string)
{
	STRING_VECTOR *v;

	v = malloc(sizeof(STRING_VECTOR));
	if(v) {
		v->key = str_dup(key);
		v->string = str_dup(string);
		v->next = *head;
		*head = v;
		return TRUE;
	}
	return FALSE;
}

void string_vector_free(STRING_VECTOR *v)
{
	free_string(v->key);
	free_string(v->string);
	free(v);
}

void string_vector_remove(STRING_VECTOR **head, char *key)
{
	STRING_VECTOR *v, *p;

	if(!*head) return;

	if(!str_cmp((*head)->key,key)) {
		v = *head;
		*head = (*head)->next;

		string_vector_free(v);
	} else {
		for(p = *head; p->next; p = p->next) {
			if(!str_cmp(p->next->key,key)) {
				v = p->next;
				p->next = p->next->next;
				string_vector_free(v);
				break;
			}
		}
	}
}

void string_vector_freeall(STRING_VECTOR *head)
{
	STRING_VECTOR *v, *n;

	for(v = head; v; v = n) {
		n = v->next;
		string_vector_free(v);
	}
}

STRING_VECTOR *string_vector_find(register STRING_VECTOR *head, char *key)
{
	while(head) {
		if(!str_cmp(head->key,key)) return head;
		head = head->next;
	}

	return NULL;
}

void string_vector_set(register STRING_VECTOR **head, char *key, char *string)
{
	STRING_VECTOR *v = string_vector_find(*head,key);

	if(v) {
		free_string(v->string);
		v->string = str_dup(string);
	} else
		string_vector_add(head,key,string);
}


void get_random_room_target(ROOM_INDEX_DATA *room, OBJ_DATA **obj, CHAR_DATA **ch, long max)
{
	register OBJ_DATA *o;
	register CHAR_DATA *c;
	register long ocnt;
	register long ccnt;
	register long r;

	for(o = room->contents, ocnt = 0;o;o = o->next_content, ++ocnt);

	for(c = room->people, ccnt = 0;c;c = c->next_in_room, ++ccnt);

	// If the entity count is more than the meter, set the meter to the total count.
	if((ocnt + ccnt) > max) max= ocnt + ccnt;

	// Now that we have the counts, let's pick a random number
	r = number_range(0,max-1);

	if(r < ocnt) {
		for(o = room->contents, ocnt = 0;o && ocnt < r;o = o->next_content,++ocnt);
		*obj = o;
		*ch = NULL;
	} else if(r < (ocnt + ccnt)) {
		for(c = room->people, ccnt = 0, r -= ocnt;c && ccnt < r;c = c->next_in_room, ++ccnt);
		*obj = NULL;
		*ch = c;
	} else {
		*obj = NULL;
		*ch = NULL;
	}

	return;
}


// @@@REMOVEME: This function is invalid (id1 is used as a vnum and an id part)
ROOM_INDEX_DATA *idfind_vroom(register unsigned long id1, register unsigned long id2)
{
	ROOM_INDEX_DATA *room;
	room = get_room_index((long)id1);

	if(!room) return NULL;

	for(room = room->clones; room; room = room->next)
		if(room->id[0] == id1 && room->id[1] == id2)
			return room;

	return NULL;
}

ROOM_INDEX_DATA *get_environment(ROOM_INDEX_DATA *room)
{
	if(!room) return NULL;

	// static or floating virtual rooms have no environment
	if(!IS_SET(room->room2_flags,ROOM_VIRTUAL_ROOM)) return NULL;

	switch(room->environ_type) {
	case ENVIRON_ROOM:	return room->environ.room;
	case ENVIRON_MOBILE:	return room->environ.mob ? room->environ.mob->in_room : NULL;
	case ENVIRON_OBJECT:	return room->environ.obj ? obj_room(room->environ.obj) : NULL;
	case ENVIRON_TOKEN:
		if(room->environ.token) {
			if( room->environ.token->player ) return room->environ.token->player->in_room;
			if( room->environ.token->object ) return obj_room(room->environ.token->object);
			if( room->environ.token->room ) return room->environ.token->room;
		}
		break;
	}

	return NULL;
}



bool mobile_is_flying(CHAR_DATA *mob)
{
	return mob && (IS_SET(mob->affected_by, AFF_FLYING) || is_float_user(mob) ||
		(mob->riding && mob->mount && mobile_is_flying(mob->mount)));
}

bool check_vision(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool blind, bool dark)
{
	if(!room) room = ch->in_room;

	if (!check_blind(ch)) {
		if(blind) send_to_char("{DYou can't see a thing!\n\r{x", ch);
		return false;
	}

	if ((!IS_NPC(ch) || IS_SWITCHED(ch)) // we are a player
		&& !IS_SET(ch->act, PLR_HOLYLIGHT)
		&& room_is_dark(room)
		&& !IS_AFFECTED(ch, AFF_INFRARED)) {
		if(dark) {
			send_to_char("{DIt is pitch black ... \n\r{x", ch);
			show_char_to_char(room->people, ch, NULL);
		}
		return false;
	}

	return true;
}


void room_from_environment(ROOM_INDEX_DATA *room)
{
	ROOM_INDEX_DATA **prev = NULL;
	LLIST *lclones = NULL;

	if(!room || !room_is_clone(room)) return;

	switch(room->environ_type) {
	case ENVIRON_ROOM: prev = &room->environ.room->clone_rooms; lclones = room->environ.room->lclonerooms; break;
	case ENVIRON_MOBILE: prev = &room->environ.mob->clone_rooms; lclones = room->environ.mob->lclonerooms; break;
	case ENVIRON_OBJECT: prev = &room->environ.obj->clone_rooms; lclones = room->environ.obj->lclonerooms; break;
	case ENVIRON_TOKEN: prev = &room->environ.token->clone_rooms; lclones = room->environ.token->lclonerooms; break;
	default: return;
	}

	while(*prev) {
		if(*prev == room) {
			*prev = room->next_clone;
			break;
		}
		prev = &(*prev)->next_clone;
	}

	list_remlink(lclones, room);

	// The object now has no clones
	if(room->environ_type == ENVIRON_OBJECT && !room->environ.obj->clone_rooms)
		obj_update_nest_clones(room->environ.obj);


	room->environ_type = ENVIRON_NONE;
	room->next_clone = NULL;
}

bool room_to_environment(ROOM_INDEX_DATA *clone,CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token)
{
	if(!clone || !room_is_clone(clone) || clone->environ_type != ENVIRON_NONE) return false;

	if(mob) {
		clone->next_clone = mob->clone_rooms;
		mob->clone_rooms = clone;
		if( !list_appendlink(mob->lclonerooms, clone) ) {
			bug("Failed to add clone room to environment due to memory issues with 'list_appendlink',", 0);
			abort();
		}
		clone->environ.mob = mob;
		clone->environ_type = ENVIRON_MOBILE;
		return true;
	}


	if(obj) {
		clone->next_clone = obj->clone_rooms;
		obj->clone_rooms = clone;
		if( !list_appendlink(obj->lclonerooms, clone) ) {
			bug("Failed to add clone room to environment due to memory issues with 'list_appendlink',", 0);
			abort();
		}

		// This is the only clone.. hence, the object used to have no clones anchored
		if(!clone->next_clone) obj_update_nest_clones(obj);
		clone->environ.obj = obj;
		clone->environ_type = ENVIRON_OBJECT;
		return true;
	}


	if(room && room != clone && room->environ_type != ENVIRON_ROOM) {
		clone->next_clone = room->clone_rooms;
		room->clone_rooms = clone;
		if( !list_appendlink(room->lclonerooms, clone) ) {
			bug("Failed to add clone room to environment due to memory issues with 'list_appendlink',", 0);
			abort();
		}
		clone->environ.room = room;
		clone->environ_type = ENVIRON_ROOM;
		return true;
	}

	if(token) {
		clone->next_clone = token->clone_rooms;
		token->clone_rooms = clone;
		if( !list_appendlink(token->lclonerooms, clone) ) {
			bug("Failed to add clone room to environment due to memory issues with 'list_appendlink',", 0);
			abort();
		}

		// This is the only clone.. hence, the object used to have no clones anchored
		clone->environ.token = token;
		clone->environ_type = ENVIRON_TOKEN;
		return true;
	}


	return false;
}

// number of nest_clones + an adjustment based upon number of clone_rooms assigned
int obj_nest_clones(OBJ_DATA *obj)
{
	return obj ? ((obj->clone_rooms ? (obj->clone_rooms->next_clone ? 2 : 1) : 0) + obj->nest_clones) : 0;
}

void obj_set_nest_clones(OBJ_DATA *obj, bool add)
{
	if(!obj) return;

	if(add) {
		obj->nest_clones++;

		// There were already extra clones on here, no need to update
		if(obj_nest_clones(obj) > 1) return;

		// Update container...
		obj_set_nest_clones(obj->in_obj,true);
	} else {
		// This already had no clones, no need to progress deeper
		if(!obj_nest_clones(obj)) return;

		obj->nest_clones--;

		// Update container...
		obj_set_nest_clones(obj->in_obj,false);
	}
}

void obj_update_nest_clones(OBJ_DATA *obj)
{
	// If this is already flagged as having nested clones from contained objects, abort
	if(!obj->in_obj || obj_nest_clones(obj) > 1) return;

	obj_set_nest_clones(obj->in_obj,(obj->clone_rooms ? true : false));
}


LLIST *list_create(bool purge)
{
	LLIST *lp = alloc_mem(sizeof(LLIST));

	if(lp) {
		lp->next = NULL;
		lp->head = NULL;
		lp->ref = 0;
		lp->size = 0;
		lp->valid = TRUE;
		lp->purge = purge;
		lp->deleter = NULL;
	}

	return lp;
}

LLIST *list_createx(bool purge, LISTCOPY_FUNC copier, LISTDESTROY_FUNC deleter)
{
	LLIST *lp = list_create(purge);

	if( lp ) {
		lp->copier = copier;
		lp->deleter = deleter;
	}

	return lp;
}


LLIST *list_copy(LLIST *src)
{
	if( src == NULL || !src->valid || src->purge ) return NULL;

	LLIST *cpy = list_create(src->purge);

	if( cpy ) {
		register LLIST_LINK *cur, *next;
		bool valid = TRUE;

		cpy->copier = src->copier;
		cpy->deleter = src->deleter;

		for( cur = src->head; cur; cur = next ) {
			next = cur->next;
			if( cur->data ) {
				void *data = cur->data;

				if( cpy->copier )
					data = (*cpy->copier)(data);

				if( !data || !list_appendlink(cpy, data) ) {
					if( data && cpy->copier && cpy->deleter )
						(*cpy->deleter)(data);

					valid = FALSE;
					break;
				}
			}
		}

		if( !valid ) {
			list_destroy(cpy);
			cpy = NULL;
		}
	}

	return cpy;
}

void list_purge(LLIST *lp)
{
	register LLIST_LINK *cur, *next;
	if( lp && !lp->valid && lp->ref < 1) {
		for( cur = lp->head; cur; cur = next ) {
			next = cur->next;

			if( lp->deleter && cur->data )
				(*lp->deleter)(cur->data);

			free_mem(cur, sizeof(LLIST_LINK));
		}
		lp->head = NULL;
		lp->tail = NULL;
	}
}

void list_destroy(LLIST *lp)
{
	if(lp && lp->valid ) {
		lp->valid = FALSE;
		if( lp->ref < 1 ) {
			// This point is only ever reached if the list has not references at the time this list is destroyed
			// If the list is in-use, the purging/freeing is handled when the references are cleared.
			list_purge(lp);
			free(lp);
		}
	}
}

void list_cull(LLIST *lp)
{
	register LLIST_LINK **prev, *cur;

	if(lp && lp->ref < 1) {
		// Cull any null data nodes
		for(prev = &lp->head, cur = lp->head; cur;) {
			if(!cur->data) {
				do {
					if( lp->tail == cur )
						lp->tail = NULL;
					*prev = cur->next;
					free_mem(cur,sizeof(LLIST_LINK));
					cur = *prev;
				} while(cur && !cur->data);
			} else {
				prev = &cur->next;
				cur = *prev;
			}
		}

		if(!lp->tail && lp->head ) {
			if( lp->head->next ) {
				for(cur = lp->head; cur->next; cur = cur->next );
				lp->tail = cur;
			} else
				lp->tail = lp->head;
		}
	}
}

void list_addref(LLIST *lp)
{
	if(lp) lp->ref++;
}

void list_remref(LLIST *lp)
{
	if(lp) {
		--lp->ref;
		list_cull(lp);

		if(lp->ref < 1 && !lp->valid) {
			list_purge(lp);
			free(lp);
		} else if(lp->ref < 1 && lp->purge) {
			list_destroy(lp);
		}
	}
}

void list_remdata(LLIST *lp, LLIST_LINK *link)
{
	if( link ) {
		if( lp->deleter )
			(*lp->deleter)(link->data);

		link->data = NULL;
		lp->size--;
	}
}


bool list_addlink(LLIST *lp, void *data)
{
	LLIST_LINK *link;

	if(lp && lp->valid && (link = alloc_mem(sizeof(LLIST_LINK)))) {

		// No need to worry about the linkage and reference
		link->next = lp->head;
		if( !lp->head )
			lp->tail = link;
		lp->head = link;

		link->data = data;
		lp->size++;
		return true;
	}
	return false;
}

bool list_appendlink(LLIST *lp, void *data)
{
	LLIST_LINK *link;

	if(lp && lp->valid && (link = alloc_mem(sizeof(LLIST_LINK)))) {
//		log_stringf("list_appendlink: Adding data %016X to list %016X.", lp, data);
		// First one?
		if( !lp->head )
			lp->head = link;
		else
			lp->tail->next = link;
		lp->tail = link;

		link->data = data;
		lp->size++;
		return true;
	}
	return false;
}

// DOES NOT DEEP COPY ELEMENTS
bool list_appendlist(LLIST *lp, LLIST *src)
{
	if( IS_VALID(lp) && IS_VALID(src) )
	{
		ITERATOR it;
		void *data;
		bool good = true;


		iterator_start(&it, src);
		while( (data = iterator_nextdata(&it)) )
		{
			if( !list_appendlink(lp, data) )
			{
				good = false;
				break;
			}
		}
		iterator_stop(&it);

		return good;
	}

	return false;
}

bool list_movelink(LLIST *lp, int from, int to)
{
	LLIST_LINK *old, *link, *prev;

	if( from < 0 ) from = lp->size + from;
	if( to < 0 ) to = lp->size + to;

	if( !from || !to ) return false;

	if( from == to ) return true;	// "Moved" it! :D :D :D :D

	//if( to > from ) --to;

	if( lp )
	{
		old = NULL;
		for(link = lp->head; link && from > 0; link = link->next)
			if(link->data)
			{
				--from;
				if( !from )
				{
					old = link;
					break;
				}
			}

		if( !old )
			return false;

		for(prev = NULL, link = lp->head; link && to > 0; prev = link, link = link->next )
		{
			if(link != old)
			{
				if( (to == 1) && (!link->data) )
				{
					// This is an empty link, reuse it
					link->data = old->data;
					old->data = NULL;
					return true;
				}

				if( link->data )
				{
					if( !--to )
					{
						link = alloc_mem(sizeof(LLIST_LINK));
						if( !link )
							return false;

						link->data = old->data;
						old->data = NULL;

						if( prev )
						{
							link->next = prev->next;
							prev->next = link;
						}
						else
						{
							link->next = lp->head;
							lp->head = link;
						}

						return true;
					}
				}
			}
		}

		if( to > 0 )
		{
			// Needs to append if it's at the end
			link = alloc_mem(sizeof(LLIST_LINK));
			if( !link )
				return false;

			if( !lp->head )
				lp->head = link;
			else
				lp->tail->next = link;
			lp->tail = link;

			link->data = old->data;
			old->data = NULL;
		}
	}

	return false;
}

bool list_insertlink(LLIST *lp, void *data, int to)
{
	LLIST_LINK *link, *prev;

	if( to < 0 ) to = lp->size + to;

	if( !to ) return false;

	if( lp )
	{
		for(prev = NULL, link = lp->head; link && to > 0; prev = link, link = link->next )
		{
			if( (to == 1) && (!link->data) )
			{
				// This is an empty link, reuse it
				link->data = data;
				lp->size++;
				return true;
			}

			if( link->data )
			{
				if( !--to )
				{
					link = alloc_mem(sizeof(LLIST_LINK));
					if( !link )
						return false;

					link->data = data;

					if( prev )
					{
						link->next = prev->next;
						prev->next = link;
					}
					else
					{
						link->next = lp->head;
						lp->head = link;
					}

					lp->size++;
					return true;
				}
			}
		}


		if( to > 0 )
		{
			// Needs to append if it's at the end
			link = alloc_mem(sizeof(LLIST_LINK));
			if( !link )
				return false;

			if( !lp->head )
				lp->head = link;
			else
				lp->tail->next = link;
			lp->tail = link;
			link->data = data;
			lp->size++;
		}

	}

	return false;
}


// Nulls out any data pointer that matches the supplied pointer
// It will NOT cull the list
void list_remlink(LLIST *lp, void *data)
{
	LLIST_LINK *link;

	if(lp && data) {
		for(link = lp->head; link; link = link->next)
			if(link->data == data) {
				list_remdata(lp, link);
			}
	}
}

// Clears out the entire list
void list_clear(LLIST *lp)
{
	LLIST_LINK *link;
	if(lp) {
		for(link = lp->head; link; link = link->next) {
			list_remdata(lp, link);
		}
	}
}

void *list_nthdata(LLIST *lp, register int nth)
{
	register LLIST_LINK *link = NULL;

	if(lp && lp->valid) {
		if( nth < 0 ) nth = lp->size + nth;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	return (link && !nth) ? link->data : NULL;
}

void list_remnthlink(LLIST *lp, register int nth)
{
	register LLIST_LINK *link = NULL;

	if(lp && lp->valid) {
		if( nth < 0 ) nth = lp->size + nth;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	if( link && !nth ) {
		list_remdata(lp, link);
	}
}

bool list_hasdata(LLIST *lp, register void *ptr)
{
	ITERATOR it;
	void *data;

	if(!lp || !lp->valid || !ptr) return FALSE;

	iterator_start(&it, lp);
	while((data = iterator_nextdata(&it)) && (data != ptr));

	iterator_stop(&it);

	return data && TRUE;
}

int list_size(LLIST *lp)
{
	ITERATOR it;
	int size;

	if(!lp || !lp->valid) return 0;

	size = 0;
	iterator_start(&it, lp);
	while((iterator_nextdata(&it))) ++size;

	iterator_stop(&it);

	return size;
}

bool list_isvalid(LLIST *lp)
{
	return lp && lp->valid;
}

// ITERATORs can just be straight variables.  No allocation is needed.
void iterator_start(ITERATOR *it, LLIST *lp)
{
	if(it) {
		if(lp && lp->valid) {
//			log_stringf("iterator_start: list =  %016lX.", lp);
			it->list = lp;
			it->current = lp->head;
			it->moved = FALSE;

			list_addref(lp);
		} else {
			it->list = NULL;
			it->current = NULL;
			it->moved = FALSE;
		}
	}
}

void iterator_start_nth(ITERATOR *it, LLIST *lp, int nth)
{
	register LLIST_LINK *link;

	if(it) {
		if(lp && lp->valid) {
			it->list = lp;

			if( nth < 0 ) nth = lp->size + nth;

			for(link = lp->head; link && nth > 0; link = link->next)
				if(link->data)
					--nth;

			it->current = link;
			it->moved = FALSE;

			list_addref(lp);
		} else {
			it->list = NULL;
			it->current = NULL;
		}
	}
}

LLIST_LINK *iterator_next(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->next; link && !link->data; link = link->next);
		} else {
			for(link = it->current; link && !link->data; link = link->next);

			it->moved = TRUE;
		}
		it->current = link;

	}

	return link;
}

void *iterator_nextdata(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	//register LLIST_LINK *next = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->next; link && !link->data; link = link->next);
		} else {
			for(link = it->current; link && !link->data; link = link->next);

			it->moved = TRUE;
		}
		it->current = link;
	}

	return link ? link->data : NULL;
}

void iterator_remcurrent(ITERATOR *it)
{
	if(it && it->list && it->current && it->current->data)
	{
		// Delete the data using specific deleter if it is defined
		if( it->list->deleter )
			(*it->list->deleter)(it->current->data);

		it->current->data = NULL;
	}
}

void iterator_reset(ITERATOR *it)
{
	if(it) {
		if(it->list && it->list->valid) {
			it->current = it->list->head;
		} else {
			it->list = NULL;
			it->current = NULL;
		}
	}
}

void iterator_stop(ITERATOR *it)
{
	if(it) {
		if(it->list) list_remref(it->list);

		it->list = NULL;
		it->current = NULL;
	}
}


///////////////////////////////////////////
//
// Function: area_has_read_access
//
// Section: Building/Security
//
// Purpose: Determines if the builder has READ access for items in the specified area.
//
// Returns: TRUE if the builder has read access
//
bool area_has_read_access(CHAR_DATA *ch, AREA_DATA *area)
{
	if(IS_NPC(ch)) return false;

//	if(!IS_IMMORTAL(ch)) return false;

	if(IS_BUILDER(ch, area)) return true;

	// Max rank can read anything
	if(ch->tot_level >= MAX_LEVEL) return true;

	return false;
}

///////////////////////////////////////////
//
// Function: area_has_write_access
//
// Section: Building/Security
//
// Purpose: Determines if the builder has WRITE access for items in the specified area.
//
// Returns: TRUE if the builder has write access
//
bool area_has_write_access(CHAR_DATA *ch, AREA_DATA *area)
{
	if(IS_NPC(ch)) return false;

	if(!IS_IMMORTAL(ch)) return false;

	if(IS_BUILDER(ch, area)) return true;

	// Only a max rank, fully secured imm can write to ANYTHING
	if(ch->tot_level < MAX_LEVEL || ch->pcdata->security < 9) return false;

	return true;
}

CHAR_DATA *obj_carrier(OBJ_DATA *obj)
{
	if(obj->carried_by) return obj->carried_by;

	if(obj->in_obj) return obj_carrier(obj->in_obj);

	return NULL;
}


// Converts the location to a room
ROOM_INDEX_DATA *location_to_room(LOCATION *loc)
{
	ROOM_INDEX_DATA *room = NULL;
	if(loc->wuid) {
		WILDS_DATA *wilds = get_wilds_from_uid(NULL,loc->wuid);
		if(wilds && !(room = get_wilds_vroom(wilds,loc->id[0],loc->id[1])))
			room = create_wilds_vroom(wilds,loc->id[0],loc->id[1]);
	} else if(loc->id[0]) {
		room = get_room_index(loc->id[0]);
		if(room && (loc->id[1] || loc->id[2]))
			room = get_clone_room(room,loc->id[1],loc->id[2]);
	}

	return room;
}

// Converts the room into a location
void location_from_room(LOCATION *loc, ROOM_INDEX_DATA *room)
{
	if(!loc) return;

	if(!room)
		location_clear(loc);
	else if(room->wilds)
		location_set(loc,room->wilds->uid,room->x,room->y,room->z);
	else
		location_set(loc,0,room->vnum,room->id[0],room->id[1]);
}


ROOM_INDEX_DATA *get_recall_room(CHAR_DATA *ch)
{
	ROOM_INDEX_DATA *loc;

	// Priority

	// 1) RECALL triggers

	// Do not reset the recall point here, as it may have been set by other means.
	// Simply call the recall triggers to see if they MODIFY it.

	if(!p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECALL, NULL))
		p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECALL, NULL);

	loc = location_to_room(&ch->recall);
	memset(&ch->recall,0,sizeof(LOCATION));

	// 2) Room assigned recalls
	if(!loc) loc = location_to_room(&ch->in_room->recall);

	// 3) Area assigned recalls
	if(!loc) loc = location_to_room(&ch->in_room->area->recall);

	return loc;
}

void location_clear(LOCATION *loc)
{
	loc->wuid = 0;
	loc->id[0] = 0;
	loc->id[1] = 0;
	loc->id[2] = 0;
}

void location_set(LOCATION *loc, unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
	loc->wuid = a;
	loc->id[0] = b;
	loc->id[1] = c;
	loc->id[2] = d;

	// if a != 0, then <b,c,d> is the xyz location on wilderness 'a'
	// if a == 0 and b != 0 and c:d == 0, then is the static room 'b'
	// if a == 0 and b != 0 and c:d != 0, then is the clone of room 'b' with id c:d
	// if a == 0 and b == 0, then it is nowhere
}

bool location_isset(LOCATION *loc)
{
	return loc && (loc->wuid || loc->id[0]);
}

float diminishing_returns(float val, float scale)
{
	float mult, trinum;
	if(val < 0)
		return -diminishing_returns(-val, scale);
	mult = val / scale;
	trinum = (sqrt(8.0 * mult + 1.0) - 1.0) / 2.0;
	return trinum * scale;
}

bool is_char_busy(CHAR_DATA *ch)
{
	if(ch == NULL) return FALSE;

	if( ch->cast > 0 ) return TRUE;
	if( ch->bind > 0 ) return TRUE;
	if( ch->bomb > 0 ) return TRUE;
	if( ch->bashed > 0 ) return TRUE;
	if( ch->resurrect > 0 ) return TRUE;
	if( ch->brew > 0 ) return TRUE;
	if( ch->recite > 0 ) return TRUE;
	if( ch->paroxysm > 0 ) return TRUE;
	if( ch->panic > 0 ) return TRUE;
	if( ch->repair > 0 ) return TRUE;
	if( ch->hide > 0 ) return TRUE;
	if( ch->fade > 0 ) return TRUE;
	if( ch->reverie > 0 ) return TRUE;
	if( ch->trance > 0 ) return TRUE;
	if( ch->scribe > 0 ) return TRUE;
	if( ch->inking > 0 ) return TRUE;
	if( ch->music > 0 ) return TRUE;
	if( ch->ranged > 0 ) return TRUE;
	if( ch->script_wait > 0 ) return TRUE;


	return FALSE;
}

bool obj_has_spell(OBJ_DATA *obj, char *name)
{
	SPELL_DATA *spell;
	int sn = skill_lookup(name);

	if( !obj || sn <= 0 ) return FALSE;

	for(spell = obj->spells; spell; spell = spell->next)
		if( spell->sn == sn )
			return TRUE;

	return FALSE;
}

void restore_char(CHAR_DATA *ch, CHAR_DATA *whom, int percent)
{
	int restored;
	affect_strip(ch,gsn_plague);
	affect_strip(ch,gsn_poison);
	affect_strip(ch,gsn_blindness);
	affect_strip(ch,gsn_sleep);
	affect_strip(ch,gsn_curse);
	affect_strip(ch,gsn_toxic_fumes);	/* @@@NIB : 20070127*/
	ch->hit 	= ch->max_hit;
	ch->mana	= ch->max_mana;
	ch->move	= ch->max_move;

	percent = URANGE(1, percent, 100);	// Clamp to usable values

	restored = percent * ch->max_hit / 100;
	ch->hit 	= UMAX(ch->hit, restored);

	restored = percent * ch->max_mana / 100;
	ch->mana	= UMAX(ch->mana, restored);

	restored = percent * ch->max_move / 100;
	ch->move	= UMAX(ch->move, restored);

	if (IS_DEAD(ch))
		resurrect_pc(ch);

	if (ch->maze_time_left > 0)
		return_from_maze(ch);

	affect_fix_char(ch);

	// Will only be set when used by the command "restore"
	//  - scripted restores will pass NULL
	if(whom)
		act("$n has restored you.",whom, ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);

	p_percent_trigger( ch, NULL, NULL, NULL, ch, whom, NULL,NULL, NULL, TRIG_RESTORE, NULL);

}


void visit_room_direction(CHAR_DATA *ch, ROOM_INDEX_DATA *start_room, int max_depth, int door, void *data, pVISIT_ROOM_LINE_FUNC func, pVISIT_ROOM_END_FUNC end_func)
{
	int depth;
	int to_x, to_y;
	DESTINATION_DATA dest;
	DESTINATION_DATA nextdest;
	EXIT_DATA *pExit;
	WILDS_VLINK *pVLink = NULL;
	bool canceled = FALSE;

	if( max_depth < 1)
		return;

	// Initialize current destination data to the start room
	dest.room =		nextdest.room =		start_room;
	dest.wilds =	nextdest.wilds =	NULL;
	dest.wx =		nextdest.wx =		0;
	dest.wy =		nextdest.wy =		0;

	for( depth = 1; !canceled && depth <= max_depth; depth++) {

		if( dest.room ) {
			// We have an actual room (static, clone or existing wilds)
			if ((pExit = dest.room->exit[door])) {
				// Hidden exits that haven't been found
				if(IS_SET(pExit->exit_info,EX_HIDDEN) && !IS_SET(pExit->exit_info,EX_FOUND))
					break;

				// Closed exits
				if(IS_SET(pExit->exit_info, EX_CLOSED))
					break;

				if(!exit_destination_data(pExit, &nextdest))
					break;

			} else
				break;

		} else if(dest.wilds) {
			// We have a wilds location, the room has not been loaded

			pVLink = vroom_get_to_vlink(dest.wilds, dest.wx, dest.wy, door);
			if( pVLink != NULL ) {
				if( !pVLink->pDestRoom )
					nextdest.room = get_room_index(pVLink->destvnum);
				else
					nextdest.room = pVLink->pDestRoom;

				if( nextdest.room &&
					(IS_SET(nextdest.room->room2_flags, ROOM_BLUEPRINT) ||
					IS_SET(nextdest.room->area->area_flags, AREA_BLUEPRINT)) )
				{
					nextdest.room = NULL;
				}

			} else {
				to_x = get_wilds_vroom_x_by_dir(dest.wilds, dest.wx, dest.wy, door);
				to_y = get_wilds_vroom_y_by_dir(dest.wilds, dest.wx, dest.wy, door);

				// Nothing here to reach, so stop
				if( !check_for_bad_room(dest.wilds, to_x, to_y) )
					break;

				nextdest.room = get_wilds_vroom(dest.wilds, to_x, to_y);
				nextdest.wx = to_x;
				nextdest.wy = to_y;


			}
		} else
			break;

		// We have an actual room
		if(nextdest.room) {
			// Depth 0 == initial room, that should already be taken care of prior to using this function
			if(func)
				canceled = (*func)(nextdest.room, ch, depth, door, data);

/*
			else {
				if( nextdest.room->wilds )
					printf_to_char(ch, "visit: depth = %d, door = %d, Wilds = <%ld, %d, %d>\n\r", depth, door, nextdest.room->wilds->uid, nextdest.room->x, nextdest.room->y);
				else
					printf_to_char(ch, "visit: depth = %d, door = %d, Room = <%ld>\n\r", depth, door, nextdest.room->vnum);
			}*/
		} else {
//			printf_to_char(ch, "visit: depth = %d, door = %d, Unloaded Wilds = <%d, %d>\n\r", depth, door, nextdest.wx, nextdest.wy);
		}

		dest.room =		nextdest.room;
		dest.wilds =	nextdest.wilds;
		dest.wx =		nextdest.wx;
		dest.wy =		nextdest.wy;
	}

	if( end_func && dest.room )
		(*end_func)(dest.room, ch, depth - 1, door, data, canceled);


/*

	for( depth = 1; depth < max_depth; depth++) {
		last_room = dest.room;

		if( pVLink != NULL ) {
			// TODO: VLINKS need FROM-WILD side exit flags

			// Hidden exits that haven't been found
			// Closed exits

			// No room there actually!
			if( !pVLink->pDestRoom )
				dest.room = get_room_index(pVLink->destvnum);
			else
				dest.room = pVLink->pDestRoom;

			if(!dest.room) {

				break;
			}

			(*func)(dest.room, ch, depth, door, data);


			pVLink = NULL;

		} else if( dest.room ) {
			// We have an actual room (static, clone or existing wilds)
			if ((pExit = dest.room->exit[door])) {
				// Hidden exits that haven't been found
				if(IS_SET(pExit->exit_info,EX_HIDDEN) && !IS_SET(pExit->exit_info,EX_FOUND))
					break;

				// Closed exits
				if(IS_SET(pExit->exit_info, EX_CLOSED))
					break;

				if(!exit_destination_data(pExit, &dest))
					break;

				// We have an actual room
				if(dest.room)
					(*func)(dest.room, ch, depth, door, data);
			} else
				break;

		} else if(dest.wilds) {
			// We have a wilds location, the room has not been loaded

			pVLink = vroom_get_to_vlink(dest.wilds, dest.wx, dest.wy, door);
			if( pVLink != NULL ) {
				continue;

			} else {
				to_x = get_wilds_vroom_x_by_dir(dest.wilds, dest.wx, dest.wy, door);
				to_y = get_wilds_vroom_y_by_dir(dest.wilds, dest.wx, dest.wy, door);

				// Nothing here to reach, so stop
				if( !check_for_bad_room(dest.wilds, to_x, to_y) )
					break;

				dest.room = get_wilds_vroom(dest.wilds, to_x, to_y);
				dest.wx = to_x;
				dest.wy = to_y;

				if(dest.room) {
					(*func)(dest.room, ch, depth, door, data);
				}

			}
		} else
			break;

	}

	if( end_func && last_room )
		(*end_func)(last_room, ch, depth, door, data);
*/
}


long dice_roll(DICE_DATA *d)
{
	d->last_roll = dice(d->number, d->size) + d->bonus;

	return d->last_roll;
}

void dice_copy(DICE_DATA *a, DICE_DATA *b)
{
	b->number = a->number;
	b->size = a->size;
	b->bonus = a->bonus;
	b->last_roll = -1;
}

char *get_shop_stock_price(SHOP_STOCK_DATA *stock)
{
	static char buf[4][MSL];
	static int count = 0;

	count = (count + 1) & 3;

	char *pricing = buf[count];

	if( IS_NULLSTR(stock->custom_price) )
	{
		pricing[0] = '\0';
		int pj = 0;

		if( stock->silver > 0)
		{
			long silver = stock->silver % 100;
			long gold = stock->silver / 100;

			if( gold > 0 )
			{
				if( silver > 0 )
				{
					pj = sprintf(pricing, "{x%ld{Yg{x%ld{Ws{x", gold, silver);
				}
				else
				{
					pj = sprintf(pricing, "{x%ld{Yg{x", gold);
				}
			}
			else
			{
				pj = sprintf(pricing, "{x%ld{Ws{x", silver);
			}
		}

		if( stock->qp > 0 )
		{
			if( pj > 0 )
			{
				pricing[pj++] = ',';
				pricing[pj++] = ' ';
			}

			pj += sprintf(pricing+pj, "{x%ld{Gqp{x", stock->qp);
		}

		if( stock->dp > 0 )
		{
			if( pj > 0 )
			{
				pricing[pj++] = ',';
				pricing[pj++] = ' ';
			}

			pj += sprintf(pricing+pj, "{x%ld{Mdp{x", stock->dp);
		}

		if( stock->pneuma > 0 )
		{
			if( pj > 0 )
			{
				pricing[pj++] = ',';
				pricing[pj++] = ' ';
			}

			pj += sprintf(pricing+pj, "{x%ld{Cpn{x", stock->pneuma);
		}
		pricing[pj] = '\0';

	}
	else
	{
		strncpy(pricing, stock->custom_price, MSL-3);
		strcat(pricing, "{x");
	}

	return pricing;
}

char *get_shop_purchase_price(long silver, long qp, long dp, long pneuma)
{
	static char buf[4][MSL];
	static int count = 0;

	count = (count + 1) & 3;

	char *pricing = buf[count];
	int pj = 0;

	bool added = FALSE;
	if( silver > 0 )
	{
		long gold = silver / 100;
		silver = silver % 100;

		if( gold > 0 )
		{
			if( silver > 0 )
				pj = sprintf(pricing, " %ld gold, %ld silver", gold, silver);
			else
				pj = sprintf(pricing, " %ld gold", gold);
		}
		else
		{
			pj = sprintf(pricing, " %ld silver", silver);
		}

		added = TRUE;
	}
	if( qp > 0 )
	{
		pj += sprintf(pricing + pj, "%s%ld quest points", (added?", ":" "), qp);
		added = TRUE;
	}
	if( dp > 0 )
	{
		pj += sprintf(pricing + pj, "%s%ld deity points", (added?", ":" "), dp);
		added = TRUE;
	}
	if( pneuma > 0 )
	{
		pj += sprintf(pricing + pj, "%s%ld pneuma", (added?", ":" "), pneuma);
	}

	pricing[pj] = '\0';

	return pricing;
}

bool is_pullable(OBJ_DATA *obj)
{
	if(!IS_VALID(obj))
		return FALSE;

	if( obj->item_type == ITEM_CART ) return TRUE;
	// if( obj->item_type == ITEM_CORPSE_NPC ) return TRUE;
	// if( obj->item_type == ITEM_CORPSE_PC ) return TRUE;


	return FALSE;
}


bool can_room_update(ROOM_INDEX_DATA *room)
{
	if( !room ) return FALSE;

	if( IS_SET(room->room2_flags, ROOM_ALWAYS_UPDATE) ) return TRUE;

	if( IS_VALID(room->instance_section) )
	{
		INSTANCE *instance = room->instance_section->instance;

		if( IS_VALID(instance) )
		{
			if( IS_VALID(instance->dungeon) )
			{
				// If the dungeon is flagged for unloading, stop normal updates
				if( IS_SET(instance->dungeon->flags, DUNGEON_DESTROY) )
					return FALSE;

				return list_size(instance->dungeon->players) > 0;
			}
			else
				return list_size(instance->players) > 0;
		}

		return TRUE;
	}

	return !room->area->empty;
}

bool is_area_unlocked(CHAR_DATA *ch, AREA_DATA *area)
{
	// Check player
	if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || (IS_IMMORTAL(ch) && IS_SET(ch->act2, PLR_HOLYWARP))) return true;

	// Check area
	if( !area || !IS_SET(area->area_flags, AREA_LOCKED) || IS_SET(area->area_flags, AREA_BLUEPRINT) ) return true;

	// Check if the area is in their list
	bool ret = false;

	ITERATOR it;
	AREA_DATA *unlocked;
	iterator_start(&it, ch->pcdata->unlocked_areas);
	while( (unlocked = (AREA_DATA *)iterator_nextdata(&it)) )
	{
		if( unlocked == area )
		{
			ret = true;
			break;
		}
	}
	iterator_stop(&it);

	return ret;
}

bool is_room_unlocked(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
	if( !room ||											// Phantom room
		room_is_clone(room) ||								// General clone
		IS_SET(room->room2_flags,ROOM_VIRTUAL_ROOM) ||		// Wilderness room
		IS_VALID(room->instance_section) )					// Instance room (should count as clone, though)
		return true;

	return is_area_unlocked(ch, room->area);
}

void player_unlock_area(CHAR_DATA *ch, AREA_DATA *area)
{
	if( is_area_unlocked(ch, area) ) return;

	list_appendlink(ch->pcdata->unlocked_areas, area);
}

bool lockstate_functional(LOCK_STATE *lock)
{
	if( !lock ) return false;

	if( list_size(lock->keys) > 0 )
	{
		return true;
	}

	if( lock->key_vnum > 0 )
	{
		return true;
	}

	return false;
}

OBJ_DATA *lockstate_getkey(CHAR_DATA *ch, LOCK_STATE *lock)
{
	if( !lock ) return NULL;

	if( IS_VALID(lock->keys) )
	{
		LLIST_UID_DATA *luid;
		OBJ_DATA *obj = NULL;

		ITERATOR it;
		iterator_start(&it, lock->keys);
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
		{
			if( luid->ptr )
			{
				obj = (OBJ_DATA *)luid->ptr;

				// In primary inventory?
				if( obj->carried_by == ch )
					break;

				// Inside a keyring that is in the primary inventory?
				if( obj->in_obj != NULL && obj->in_obj->item_type == ITEM_KEYRING )
				{
					if( obj->in_obj->carried_by == ch )
						break;
				}

			}
		}
		iterator_stop(&it);

		return obj;
	}

	if( lock->key_vnum > 0 )
	{
		OBJ_DATA *obj;
		OBJ_DATA *key;

		for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
		{
			if (obj->pIndexData->vnum == lock->key_vnum)
				return obj;
		}

		/* Keyring */
		for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
		{
			if (obj->pIndexData->item_type == ITEM_KEYRING)
			{
				for (key = obj->contains; key != NULL; key = key->next_content)
				{
					if (key->pIndexData->vnum == lock->key_vnum)
						return key;
				}
			}
		}

		return NULL;
	}

	return NULL;
}

SPECIAL_KEY_DATA *get_special_key(LLIST *list, long vnum)
{
	ITERATOR it;
	SPECIAL_KEY_DATA *sk;

	if( !IS_VALID(list) ) return NULL;

	iterator_start(&it, list);
	while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&it)) )
	{
		if( sk->key_vnum == vnum)
			break;
	}
	iterator_stop(&it);

	return sk;
}

void extract_special_key(OBJ_DATA *obj)
{
	ITERATOR skit, kit;
	SPECIAL_KEY_DATA *sk;
	LLIST_UID_DATA *luid;

	iterator_start(&skit, loaded_special_keys);
	while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&skit)) )
	{
		if( sk->key_vnum == obj->pIndexData->vnum)
		{
			iterator_start(&kit, sk->list);
			while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&kit)) )
			{
				if( luid->id[0] == obj->id[0] &&
					luid->id[1] == obj->id[1] )
				{
					luid->ptr = NULL;
				}
			}
			iterator_stop(&kit);

		}
	}
	iterator_stop(&skit);
}

void resolve_special_key(OBJ_DATA *obj)
{
	ITERATOR skit, kit;
	SPECIAL_KEY_DATA *sk;
	LLIST_UID_DATA *luid;

	iterator_start(&skit, loaded_special_keys);
	while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&skit)) )
	{
		if( sk->key_vnum == obj->pIndexData->vnum)
		{
			iterator_start(&kit, sk->list);
			while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&kit)) )
			{
				if( luid->id[0] == obj->id[0] &&
					luid->id[1] == obj->id[1] )
				{
					luid->ptr = obj;
				}
			}
			iterator_stop(&kit);

		}
	}
	iterator_stop(&skit);
}

char *get_article(char *text, bool upper)
{
	switch(UPPER(text[0]))
	{
	case 'A':
	case 'E':
	case 'I':
	case 'O':
	case 'U':
		return upper ? "An" : "an";
	}

	return upper ? "A" : "a";

}