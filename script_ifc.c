/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "tables.h"
#include "scripts.h"
#include "wilds.h"
#include <math.h>

extern bool wiznet_script;

//
// All strings will be stored in buffers by the calling routine.
// An ifcheck will not be called unless all of the parameter requirements are met.
// If not, the value will default to the return type default of false or ZERO.
//

#define ARG_TYPE(x,f)	(argv[(x)]->d.f)
#define ISARG_TYPE(x,t,f)	((argv[(x)]->type == (t)) && ARG_TYPE(x,f))

#define ISARG_NUM(x)	(argv[(x)]->type == ENT_NUMBER)
#define ISARG_STR(x)	ISARG_TYPE(x,ENT_STRING,str)
#define ISARG_MOB(x)	ISARG_TYPE(x,ENT_MOBILE,mob)
#define ISARG_OBJ(x)	ISARG_TYPE(x,ENT_OBJECT,obj)
#define ISARG_ROOM(x)	ISARG_TYPE(x,ENT_ROOM,room)
#define ISARG_EXIT(x)	(argv[(x)]->type == ENT_EXIT)
#define ISARG_TOK(x)	ISARG_TYPE(x,ENT_TOKEN,token)
#define ISARG_AREA(x)	ISARG_TYPE(x,ENT_AREA,area)
#define ISARG_SKILL(x)	(argv[(x)]->type == ENT_SKILL)
#define ISARG_SKINFO(x)	(argv[(x)]->type == ENT_SKILLINFO)
#define ISARG_CONN(x)	ISARG_TYPE(x,ENT_CONN,conn)
#define ISARG_WILDS(x)	ISARG_TYPE(x,ENT_WILDS,wilds)
#define ISARG_CHURCH(x)	ISARG_TYPE(x,ENT_CHURCH,church)
#define ISARG_AFF(x)	ISARG_TYPE(x,ENT_AFFECT,aff)
#define ISARG_MUID(x)	(argv[(x)]->type == ENT_MOBILE_ID)
#define ISARG_OUID(x)	(argv[(x)]->type == ENT_OBJECT_ID)
#define ISARG_TUID(x)	(argv[(x)]->type == ENT_TOKEN_ID)
#define ISARG_SKID(x)	(argv[(x)]->type == ENT_SKILLINFO_ID)
#define ISARG_AID(x)	(argv[(x)]->type == ENT_AREA_ID)
#define ISARG_WID(x)	(argv[(x)]->type == ENT_WILDS_ID)
#define ISARG_CHID(x)	(argv[(x)]->type == ENT_CHURCH_ID)
#define ISARG_BLIST(x,t)	((argv[(x)]->type == (t)) && IS_VALID(argv[(x)]->d.blist))
#define ISARG_BLLIST_ROOM(x)	ISARG_BLIST(x,ENT_BLLIST_ROOM)
#define ISARG_BLLIST_MOB(x)	ISARG_BLIST(x,ENT_BLLIST_MOB)
#define ISARG_BLLIST_OBJ(x)	ISARG_BLIST(x,ENT_BLLIST_OBJ)
#define ISARG_BLLIST_TOK(x)	ISARG_BLIST(x,ENT_BLLIST_TOK)
#define ISARG_BLLIST_EXIT(x)	ISARG_BLIST(x,ENT_BLLIST_EXIT)
#define ISARG_BLLIST_SKILL(x)	ISARG_BLIST(x,ENT_BLLIST_SKILL)
#define ISARG_BLLIST_AREA(x)		ISARG_BLIST(x,ENT_BLLIST_AREA)
#define ISARG_BLLIST_WILDS(x)	ISARG_BLIST(x,ENT_BLLIST_WILDS)

#define ISARG_PLLIST_STR(x)	ISARG_BLIST(x,ENT_PLLIST_STR)
#define ISARG_PLLIST_CONN(x)	ISARG_BLIST(x,ENT_PLLIST_CONN)
#define ISARG_PLLIST_ROOM(x)	ISARG_BLIST(x,ENT_PLLIST_ROOM)
#define ISARG_PLLIST_MOB(x)	ISARG_BLIST(x,ENT_PLLIST_MOB)
#define ISARG_PLLIST_OBJ(x)	ISARG_BLIST(x,ENT_PLLIST_OBJ)
#define ISARG_PLLIST_TOK(x)	ISARG_BLIST(x,ENT_PLLIST_TOK)
#define ISARG_PLLIST_CHURCH(x)	ISARG_BLIST(x,ENT_PLLIST_CHURCH)

#define ISARG_DICE(x)	((argv[(x)]->type == ENT_DICE) && argv[(x)]->d.dice)
#define ISARG_SECTION(x)	((argv[(x)]->type == ENT_SECTION) && argv[(x)]->d.section)
#define ISARG_INSTANCE(x)	((argv[(x)]->type == ENT_INSTANCE) && argv[(x)]->d.instance)
#define ISARG_DUNGEON(x)	((argv[(x)]->type == ENT_DUNGEON) && argv[(x)]->d.dungeon)
#define ISARG_SHIP(x)		((argv[(x)]->type == ENT_SHIP) && argv[(x)]->d.ship)
#define ISARG_BV(x)			((argv[(x)]->type == ENT_BITVECTOR) && argv[(x)]->d.bv.table)
#define ISARG_BM(x)			((argv[(x)]->type == ENT_BITMATRIX) && argv[(x)]->d.bv.table)

#define ARG_NUM(x)	ARG_TYPE(x,num)
#define ARG_STR(x)	ARG_TYPE(x,str)
#define ARG_MOB(x)	ARG_TYPE(x,mob)
#define ARG_OBJ(x)	ARG_TYPE(x,obj)
#define ARG_ROOM(x)	ARG_TYPE(x,room)
#define ARG_EXIT(x)	ARG_TYPE(x,door)
#define ARG_TOK(x)	ARG_TYPE(x,token)
#define ARG_AREA(x)	ARG_TYPE(x,area)
#define ARG_SKILL(x)	ARG_TYPE(x,sn)
#define ARG_SKINFO(x)	ARG_TYPE(x,sk)
#define ARG_CONN(x)	ARG_TYPE(x,conn)
#define ARG_WILDS(x)	ARG_TYPE(x,wilds)
#define ARG_CHURCH(x)	ARG_TYPE(x,church)
#define ARG_AFF(x)	ARG_TYPE(x,aff)
#define ARG_BLIST(x)	ARG_TYPE(x,blist)
#define ARG_UID(x)	ARG_TYPE(x,uid)
#define ARG_UID2(x,y)	(argv[(x)]->d.uid[(y)])
#define ARG_AID(x)	ARG_TYPE(x,aid)
#define ARG_WID(x)	ARG_TYPE(x,wid)
#define ARG_CHID(x)	ARG_TYPE(x,chid)
#define ARG_DICE(x) ARG_TYPE(x,dice)
#define ARG_SECTION(x) ARG_TYPE(x,section)
#define ARG_INSTANCE(x) ARG_TYPE(x,instance)
#define ARG_DUNGEON(x) ARG_TYPE(x,dungeon)
#define ARG_SHIP(x)	ARG_TYPE(x,ship)
#define ARG_BV(x) ARG_TYPE(x,bv)
#define ARG_BM(x) ARG_TYPE(x,bm)

#define SHIFT_MOB()	do { if(ISARG_MOB(0)) { mob = ARG_MOB(0); ++argv; --argc; } } while(0)
#define SHIFT_OBJ()	do { if(ISARG_OBJ(0)) { obj = ARG_OBJ(0); ++argv; --argc; } } while(0)
#define SHIFT_ROOM()	do { if(ISARG_ROOM(0)) { room = ARG_ROOM(0); ++argv; --argc; } } while(0)
#define SHIFT_TOK()	do { if(ISARG_TOK(0)) { token = ARG_TOK(0); ++argv; --argc; } } while(0)

#define VALID_STR(x) (ISARG_STR(x) && ARG_STR(x))
#define VALID_NPC(x) (ISARG_MOB(x) && IS_NPC(ARG_MOB(x)))
#define VALID_PLAYER(x) (ISARG_MOB(x) && !IS_NPC(ARG_MOB(x)))

#define ARG_BOOL(x) ( (ISARG_NUM(x) && ARG_NUM(x) != 0) || (ISARG_STR(x) && !str_cmp(ARG_STR(x), "true")) )

long flag_value_ifcheck(const struct flag_type *flag_table, char *argument)
{
	long flag = flag_value(flag_table, argument);

	return (flag != NO_FLAG) ? flag : 0;
}


// Checks if the "act" field on the mob has the given flags
DECL_IFC_FUN(ifc_act)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) &&
		((IS_NPC(ARG_MOB(0)) && IS_SET(ARG_MOB(0)->act[0], flag_value_ifcheck(act_flags,ARG_STR(1)))) ||
		(!IS_NPC(ARG_MOB(0)) && IS_SET(ARG_MOB(0)->act[0], flag_value_ifcheck(plr_flags,ARG_STR(1))))));
	return true;
}

// Checks if the "act2" field on the mob has the given flags
DECL_IFC_FUN(ifc_act2)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) &&
		((IS_NPC(ARG_MOB(0)) && IS_SET(ARG_MOB(0)->act[1], flag_value_ifcheck(act2_flags,ARG_STR(1)))) ||
		(!IS_NPC(ARG_MOB(0)) && IS_SET(ARG_MOB(0)->act[1], flag_value_ifcheck(plr2_flags,ARG_STR(1))))));
	return true;
}

// Checks if the "affected_by" field on the mob has the given flags
DECL_IFC_FUN(ifc_affected)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->affected_by[0], flag_value_ifcheck( affect_flags,ARG_STR(1))));
	return true;
}

// Checks if the "affected_by2" field on the mob has the given flags
DECL_IFC_FUN(ifc_affected2)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->affected_by[1], flag_value_ifcheck( affect2_flags,ARG_STR(1))));
	return true;
}

// Checks if the mobile has an affect with the custom name
DECL_IFC_FUN(ifc_affectedname)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && is_affected_name(ARG_MOB(0), get_affect_cname(ARG_STR(1))));
	return true;
}

// Checks if the mobile is affected by the spell
DECL_IFC_FUN(ifc_affectedspell)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && is_affected(ARG_MOB(0), skill_lookup(ARG_STR(1))));
	return true;
}


// Gets the player's age
DECL_IFC_FUN(ifc_age)
{
	*ret = VALID_PLAYER(0) ? GET_AGE(ARG_MOB(0)) : 0;
	return true;
}

// Gets the mobile's alignment
DECL_IFC_FUN(ifc_align)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->alignment : 0;
	return true;
}

// Gets the player's arena fight count: wins + loss
DECL_IFC_FUN(ifc_arenafights)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->arena_deaths+ARG_MOB(0)->arena_kills) : 0;
	return true;
}

// Gets the player's arena losses
DECL_IFC_FUN(ifc_arenaloss)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->arena_deaths : 0;
	return true;
}

// Gets the player's arena win ratio: 0 = 0.0% to 1000 = 100.0%
DECL_IFC_FUN(ifc_arenaratio)
{
	int val;

	if (VALID_PLAYER(0)) {
		val = (ARG_MOB(0)->arena_deaths+ARG_MOB(0)->arena_kills);
		if(val > 0) val = 1000 * ARG_MOB(0)->arena_kills / val;
		*ret = val;
		return true;
	}

	return false;
}

// Gets the player's arena wins
DECL_IFC_FUN(ifc_arenawins)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->arena_kills : 0;
	return true;
}

DECL_IFC_FUN(ifc_canhunt)
{
	if(argc == 2)
		*ret = (ISARG_MOB(0) && ISARG_MOB(1) && can_hunt(ARG_MOB(0),ARG_MOB(1)));
	else
		*ret = (mob && ISARG_MOB(0) && can_hunt(mob,ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_canpractice)
{
	int sn;

	*ret = (VALID_PLAYER(0) && ARG_STR(1) &&
		((sn = skill_lookup(ARG_STR(1))) > 0) &&
		can_practice(ARG_MOB(0),sn));
	return true;
}

DECL_IFC_FUN(ifc_canscare)
{
	*ret = (ISARG_MOB(0) && can_scare(ARG_MOB(0)));
	return true;
}

// Checks to see if the SELF object is worn by the target mobile
DECL_IFC_FUN(ifc_carriedby)
{
	*ret = (obj && ISARG_MOB(0) && obj->carried_by == ARG_MOB(0) &&
		obj->wear_loc == WEAR_NONE);
	return true;
}

DECL_IFC_FUN(ifc_carries)
{
	if(ISARG_MOB(0)) {
		if (ISARG_NUM(1))
			*ret = (int)has_item(ARG_MOB(0), ARG_NUM(1), -1, false);
		else if(ISARG_STR(1)) {
			if (is_number(ARG_STR(1)))
				*ret = (int)has_item(ARG_MOB(0), atol(ARG_STR(1)), -1, false);
			else
				*ret = (int)(get_obj_carry(ARG_MOB(0), ARG_STR(1), ARG_MOB(0)) && 1);
		} else if(ISARG_OBJ(1))
			*ret = (ARG_OBJ(1)->carried_by == ARG_MOB(0)) ||
				(ARG_OBJ(1)->in_obj && ARG_OBJ(1)->in_obj->carried_by == ARG_MOB(0));
		else
			return false;
		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_carryleft)
{
	if (ISARG_OBJ(0) && (obj = ARG_OBJ(0))) {
		if(obj->item_type == ITEM_CART || obj->item_type == ITEM_CONTAINER)
			*ret = obj->value[3] - get_obj_number_container(obj);
		else if(obj->item_type == ITEM_WEAPON_CONTAINER)
			*ret = obj->value[2] - get_obj_number_container(obj);
		else
			*ret = 0;
	} else if(ISARG_MOB(0) && (mob = ARG_MOB(0)))
		*ret = can_carry_n(mob) - mob->carry_number;
	else
		return false;

	return true;
}

DECL_IFC_FUN(ifc_church)
{
	*ret = false;
	if(VALID_PLAYER(0) && ARG_MOB(0)->church) {
		if(ISARG_STR(1) && !str_cmp(ARG_STR(1), ARG_MOB(0)->church->name))
			*ret = true;
		else if(ISARG_CHURCH(1) && ARG_MOB(0)->church == ARG_CHURCH(1))
			*ret = true;
	}
	return true;
}

DECL_IFC_FUN(ifc_churchonline)
{
	if(VALID_PLAYER(0))
		*ret = get_church_online_count(ARG_MOB(0));
	else if(ISARG_CHURCH(0))
		*ret = list_size(ARG_CHURCH(0)->online_players);
	else
		*ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_churchsize)
{
	if(ISARG_MOB(0)) {
		mob = ARG_MOB(0);
		*ret = (!IS_NPC(mob) && mob->church) ? mob->church->size : 0;
		return true;
	} else if(ISARG_STR(0)) {
		CHURCH_DATA *church = find_church_name(ARG_STR(0));

		*ret = church ? church->size : 0;
	}

	return false;
}

DECL_IFC_FUN(ifc_churchrank)
{
	if(ISARG_MOB(0)) {
		mob = ARG_MOB(0);
		*ret = (!IS_NPC(mob) && mob->church && mob->church_member) ?
			(mob->church_member->rank+1) : 0;
		return true;
	}

	return false;
}

DECL_IFC_FUN(ifc_clan)
{
	return false;
}

DECL_IFC_FUN(ifc_class)
{
	return false;
}

DECL_IFC_FUN(ifc_clones)
{
	*ret = mob ? count_people_room(mob, NULL, NULL, NULL, 3) : 0;
	return true;
}

DECL_IFC_FUN(ifc_container)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_CONTAINER &&
		IS_SET(ARG_OBJ(0)->value[1], flag_value_ifcheck(container_flags,ARG_STR(1)));
	return true;
}

// Gets the player's cpk fight count: wins + loss
DECL_IFC_FUN(ifc_cpkfights)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills) : 0;
	return true;
}

// Gets the player's cpk losses
DECL_IFC_FUN(ifc_cpkloss)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->cpk_deaths : 0;
	return true;
}

// Gets the player's cpk win ratio: 0 = 0.0% to 1000 = 100.0%
DECL_IFC_FUN(ifc_cpkratio)
{
	int val;

	if (VALID_PLAYER(0)) {
		val = (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills);
		if(val > 0) val = 1000 * ARG_MOB(0)->cpk_kills / val;
		*ret = val;
		return true;
	}

	return false;
}

// Gets the player's cpk wins
DECL_IFC_FUN(ifc_cpkwins)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->cpk_kills : 0;
	return true;
}

DECL_IFC_FUN(ifc_curhit)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->hit : 0;
	return true;
}

DECL_IFC_FUN(ifc_curmana)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->mana : 0;
	return true;
}

DECL_IFC_FUN(ifc_curmove)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->move : 0;
	return true;
}

DECL_IFC_FUN(ifc_danger)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->danger_range : 0;
	return true;
}

DECL_IFC_FUN(ifc_damtype)
{
	if(ARG_MOB(0)) *ret = (ARG_MOB(0)->dam_type == attack_lookup(ARG_STR(1)));
	else if(ARG_OBJ(0)) *ret = (ARG_OBJ(0)->item_type == ITEM_WEAPON &&
			ARG_OBJ(0)->value[3] == attack_lookup(ARG_STR(1)));
	else *ret = false;
	return true;
}

// Returns the current game day
DECL_IFC_FUN(ifc_day)
{
	*ret = time_info.day;
	return true;
}

DECL_IFC_FUN(ifc_deathcount)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->deaths : 0;
	return true;
}

DECL_IFC_FUN(ifc_deity)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->deitypoints : 0;
	return true;
}

DECL_IFC_FUN(ifc_dice)
{
	if(ISARG_OBJ(0)) {
		if(ARG_OBJ(0)->item_type == ITEM_WEAPON)
			*ret = dice(ARG_OBJ(0)->value[1], ARG_OBJ(0)->value[2]);
		else
			*ret = 0;
		if(ISARG_NUM(1)) *ret += ARG_NUM(1);
	} else if(ISARG_NUM(0) && ISARG_NUM(1)) {
		*ret = dice(ARG_NUM(0), ARG_NUM(1));
		if(ISARG_NUM(2))
			*ret += ARG_NUM(2);
	} else
		return false;

	return true;
}

DECL_IFC_FUN(ifc_drunk)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->condition[COND_DRUNK] : 0;
	return true;
}

DECL_IFC_FUN(ifc_exists)
{
	return false;
}

DECL_IFC_FUN(ifc_exitexists)
{
	int door;
	if(ISARG_STR(0)) {
		if(mob) room = mob->in_room;
		else if(obj) room = obj_room(obj);
		else if(token) room = token_room(token);

		door = get_num_dir(ARG_STR(0));
	} else if(ISARG_STR(1)) {
		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
		else return false;

		door = get_num_dir(ARG_STR(1));
	} else return false;

	*ret = (room && door >= 0 && room->exit[door] && room->exit[door]->u1.to_room);
	return true;
}

DECL_IFC_FUN(ifc_exitflag)
{
	int door;
	EXIT_DATA *ex;

	if(ISARG_EXIT(0)) {
		if(!ISARG_STR(1)) return false;

		if( !ARG_EXIT(0).r ) return false;

		if( !(ex = ARG_EXIT(0).r->exit[ARG_EXIT(0).door]) ) return false;

		*ret = IS_SET(ex->exit_info, flag_value_ifcheck(exit_flags,ARG_STR(1)));
	} else {


		if(!ISARG_STR(1) || !ISARG_STR(2)) return false;

		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
		else return false;

		door = get_num_dir(ARG_STR(1));

		*ret = (room && door >= 0 && room->exit[door] && IS_SET(room->exit[door]->exit_info, flag_value_ifcheck(exit_flags,ARG_STR(2))));
	}
	return true;
}

DECL_IFC_FUN(ifc_fullness)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->condition[COND_FULL] : 0;
	return true;
}

DECL_IFC_FUN(ifc_furniture)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_FURNITURE &&
		IS_SET(ARG_OBJ(0)->value[2], flag_value_ifcheck(furniture_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_gold)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->gold : 0;
	return true;
}

DECL_IFC_FUN(ifc_groundweight)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = get_room_weight(room,true,true,true);
	return true;
}

DECL_IFC_FUN(ifc_groupcon)
{
	*ret = ISARG_MOB(0) ? get_curr_group_stat(ARG_MOB(0),STAT_CON) : 0;
	return true;
}

DECL_IFC_FUN(ifc_groupdex)
{
	*ret = ISARG_MOB(0) ? get_curr_group_stat(ARG_MOB(0),STAT_DEX) : 0;
	return true;
}

DECL_IFC_FUN(ifc_groupint)
{
	*ret = ISARG_MOB(0) ? get_curr_group_stat(ARG_MOB(0),STAT_INT) : 0;
	return true;
}

DECL_IFC_FUN(ifc_groupstr)
{
	*ret = ISARG_MOB(0) ? get_curr_group_stat(ARG_MOB(0),STAT_STR) : 0;
	return true;
}

DECL_IFC_FUN(ifc_groupwis)
{
	*ret = ISARG_MOB(0) ? get_curr_group_stat(ARG_MOB(0),STAT_WIS) : 0;
	return true;
}

DECL_IFC_FUN(ifc_grpsize)
{
	*ret = ISARG_MOB(0) ? count_people_room(ARG_MOB(0), NULL, NULL, NULL, 4) : 0;
	return true;
}

DECL_IFC_FUN(ifc_has)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && has_item(ARG_MOB(0), -1, item_lookup(ARG_STR(1)), false));
	return true;
}

DECL_IFC_FUN(ifc_hasqueue)
{
	if(ISARG_MOB(0)) *ret = (bool)(int)(ARG_MOB(0)->events && 1);
	else if(ISARG_OBJ(0)) *ret = (bool)(int)(ARG_OBJ(0)->events && 1);
	else if(ISARG_ROOM(0)) *ret = (bool)(int)(ARG_ROOM(0)->events && 1);
	else if(ISARG_TOK(0)) *ret = (bool)(int)(ARG_TOK(0)->events && 1);
	else return false;
	return true;
}

DECL_IFC_FUN(ifc_hasship)
{
	return false;
}

DECL_IFC_FUN(ifc_hastarget)
{
	*ret = (ISARG_MOB(0) && ARG_MOB(0)->progs->target &&
		ARG_MOB(0)->in_room == ARG_MOB(0)->progs->target->in_room);
	return true;
}

DECL_IFC_FUN(ifc_hastoken)
{
	long vnum = 0;
	if(ISARG_NUM(1))
		vnum = ARG_NUM(1);
	else if(ISARG_TOK(1))
		vnum = ARG_TOK(1)->pIndexData->vnum;

	if(ISARG_MOB(0)) *ret = true && get_token_char(ARG_MOB(0), vnum, (ISARG_NUM(2) ? ARG_NUM(2) : 1));
	else if(ISARG_OBJ(0)) *ret = true && get_token_obj(ARG_OBJ(0), vnum, (ISARG_NUM(2) ? ARG_NUM(2) : 1));
	else if(ISARG_ROOM(0)) *ret = true && get_token_room(ARG_ROOM(0), vnum, (ISARG_NUM(2) ? ARG_NUM(2) : 1));
	else return false;

	return true;
}

DECL_IFC_FUN(ifc_healregen)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->heal_rate : 0;
	return true;
}

// Returns the current game hour
DECL_IFC_FUN(ifc_hour)
{
	*ret = time_info.hour;
	return true;
}

DECL_IFC_FUN(ifc_hpcnt)
{
	*ret = ISARG_MOB(0) ? (ARG_MOB(0)->hit * 100 / (UMAX(1,ARG_MOB(0)->max_hit))) : 0;
	return true;
}

DECL_IFC_FUN(ifc_hunger)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->condition[COND_HUNGER] : 0;
	return true;
}

DECL_IFC_FUN(ifc_id)
{
	if(ISARG_MOB(0)) *ret = (int)ARG_MOB(0)->id[0];
	else if(ISARG_OBJ(0)) *ret = (int)ARG_OBJ(0)->id[0];
	else if(ISARG_ROOM(0)) *ret = (int)ARG_ROOM(0)->id[0];
	else if(ISARG_TOK(0)) *ret = (int)ARG_TOK(0)->id[0];
	else if(ISARG_MUID(0)) *ret = (int)ARG_UID2(0,0);
	else if(ISARG_OUID(0)) *ret = (int)ARG_UID2(0,0);
	else if(ISARG_TUID(0)) *ret = (int)ARG_UID2(0,0);
	else if(ISARG_AREA(0)) *ret = ARG_AREA(0)->uid;
	else if(ISARG_AID(0)) *ret = (int)ARG_AID(0);
	else if(ISARG_WILDS(0)) *ret = ARG_WILDS(0)->uid;
	else if(ISARG_WID(0)) *ret = (int)ARG_WID(0);
	else if(ISARG_CHURCH(0)) *ret = ARG_CHURCH(0)->uid;
	else if(ISARG_CHID(0)) *ret = (int)ARG_CHID(0);
	else *ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_id2)
{
	if(ISARG_MOB(0)) *ret = (int)ARG_MOB(0)->id[1];
	else if(ISARG_OBJ(0)) *ret = (int)ARG_OBJ(0)->id[1];
	else if(ISARG_ROOM(0)) *ret = (int)ARG_ROOM(0)->id[1];
	else if(ISARG_TOK(0)) *ret = (int)ARG_TOK(0)->id[1];
	else if(ISARG_MUID(0)) *ret = (int)ARG_UID2(0,1);
	else if(ISARG_OUID(0)) *ret = (int)ARG_UID2(0,1);
	else if(ISARG_TUID(0)) *ret = (int)ARG_UID2(0,1);
	else *ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_identical)
{
	if(argc != 2) *ret = false;
	else if(ISARG_NUM(0) && ISARG_NUM(1)) *ret = ARG_NUM(0) == ARG_NUM(1);
	else if(ISARG_STR(0) && ISARG_STR(1)) *ret = !str_cmp(ARG_STR(0),ARG_STR(1));	// Full string comparison, not isname!
	else if(ISARG_MOB(0) && ISARG_MOB(1)) *ret = ARG_MOB(0) == ARG_MOB(1);
	else if(ISARG_OBJ(0) && ISARG_OBJ(1)) *ret = ARG_OBJ(0) == ARG_OBJ(1);
	else if(ISARG_ROOM(0) && ISARG_ROOM(1)) *ret = ARG_ROOM(0) == ARG_ROOM(1);
	else if(ISARG_TOK(0) && ISARG_TOK(1)) *ret = ARG_TOK(0) == ARG_TOK(1);
	else if(ISARG_EXIT(0) && ISARG_EXIT(1)) *ret = (ARG_EXIT(0).r == ARG_EXIT(1).r) && (ARG_EXIT(0).door == ARG_EXIT(1).door);
	else if(ISARG_AREA(0) && ISARG_AREA(1)) *ret = ARG_AREA(0) == ARG_AREA(1);
	else *ret = false;

	return true;
}

DECL_IFC_FUN(ifc_imm)
{
	*ret = (ISARG_MOB(0) && IS_SET(ARG_MOB(0)->imm_flags, flag_value_ifcheck(imm_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_innature)
{
	*ret = (ISARG_MOB(0) && is_in_nature(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isactive)
{
	*ret = (ISARG_MOB(0) && ARG_MOB(0)->position > POS_SLEEPING);
	return true;
}

DECL_IFC_FUN(ifc_isambushing)
{
	*ret = (ISARG_MOB(0) && ARG_MOB(0)->ambush);
	return true;
}

DECL_IFC_FUN(ifc_isangel)
{
	*ret = (ISARG_MOB(0) && IS_ANGEL(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isbrewing)
{
	int sn;

	*ret = ISARG_MOB(0) && ARG_MOB(0)->brew_sn > 0 &&
		(!ISARG_STR(1) || !*ARG_STR(1) ||
			((sn = skill_lookup(ARG_STR(1))) > 0 &&
				ARG_MOB(0)->brew_sn == sn));
	return true;
}

DECL_IFC_FUN(ifc_iscasting)
{
	int sn;

	*ret = ISARG_MOB(0) && ARG_MOB(0)->cast > 0 &&
		((ISARG_NUM(1) && (token = get_token_char(ARG_MOB(0), ARG_NUM(1), (ISARG_NUM(2) ? ARG_NUM(2) : 1))) &&
				token->pIndexData->type == TOKEN_SPELL && ARG_MOB(0)->cast_token == token) ||
			(ISARG_TOK(1) && ARG_TOK(0)->pIndexData->type == TOKEN_SPELL &&
				ARG_TOK(1)->player == ARG_MOB(0) && ARG_MOB(0)->cast_token == ARG_TOK(1)) ||
			!ISARG_STR(1) || !*ARG_STR(1) ||
			((sn = skill_lookup(ARG_STR(1))) > 0 &&
				ARG_MOB(0)->cast_sn == sn));

	return true;
}

DECL_IFC_FUN(ifc_ischarm)
{
	*ret = (ISARG_MOB(0) && IS_AFFECTED(ARG_MOB(0), AFF_CHARM));
	return true;
}

DECL_IFC_FUN(ifc_ischurchexcom)
{
	*ret = (VALID_PLAYER(0) && ARG_MOB(0)->church && ARG_MOB(0)->church_member &&
    		IS_SET(ARG_MOB(0)->church_member->flags,CHURCH_PLAYER_EXCOMMUNICATED));
	return true;
}

DECL_IFC_FUN(ifc_iscpkproof)
{
	*ret = ISARG_OBJ(0) && IS_SET(ARG_OBJ(0)->extra[0],ITEM_NODROP) && IS_SET(ARG_OBJ(0)->extra[0],ITEM_NOUNCURSE);
	return true;
}

DECL_IFC_FUN(ifc_isdead)
{
	*ret = (ISARG_MOB(0) && IS_DEAD(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isdelay)
{
	if(ISARG_MOB(0)) *ret = (ARG_MOB(0)->progs->delay > 0);
	else if(ISARG_OBJ(0)) *ret = (ARG_OBJ(0)->progs->delay > 0);
	else if(ISARG_ROOM(0)) *ret = (ARG_ROOM(0)->progs->delay > 0);
	else if(ISARG_TOK(0)) *ret = (ARG_TOK(0)->progs->delay > 0);
	else return false;

	return true;
}

DECL_IFC_FUN(ifc_isdemon)
{
	*ret = (ISARG_MOB(0) && IS_DEMON(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isevil)
{
	*ret = (ISARG_MOB(0) && IS_EVIL(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isfading)
{
	*ret = ISARG_MOB(0) && ARG_MOB(0)->fade_dir != -1 &&
		(!ISARG_STR(1) || !*ARG_STR(1) ||
			ARG_MOB(0)->fade_dir == get_num_dir(ARG_STR(1)));
	return true;
}

// isfighting <victim>[ <opponent>]
//
// "isfighting <victim>" is <mobile> fighting?
// "isfighting <victim> <mobile>" is <victim> fighting <mobile>?
// "isfighting <victim> <string>" is <victim>'s opponent have the name <string>?
// "isfighting <victim> <number>" is <victim>'s opponent have the vnum <number>?
DECL_IFC_FUN(ifc_isfighting)
{
	*ret = false;
	if(ISARG_MOB(0)) {
		if(ISARG_MOB(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->fighting == ARG_MOB(1));
		else if(ISARG_STR(1)) {
			if(is_number(ARG_STR(1)))
				*ret = (ARG_MOB(0) && ARG_MOB(0)->fighting && IS_NPC(ARG_MOB(0)->fighting) && ARG_MOB(0)->fighting->pIndexData->vnum == atoi(ARG_STR(1)));
			else
				*ret = (ARG_MOB(0) && ARG_MOB(0)->fighting && is_name(ARG_STR(1),ARG_MOB(0)->fighting->name));
		} else if(ISARG_NUM(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->fighting && IS_NPC(ARG_MOB(0)->fighting) && ARG_MOB(0)->fighting->pIndexData->vnum == ARG_NUM(1));
		else if(argc == 1)
			*ret = (ARG_MOB(0) && ARG_MOB(0)->fighting);
	}
	return true;
}

DECL_IFC_FUN(ifc_isflying)
{
	*ret = ISARG_MOB(0) && mobile_is_flying(ARG_MOB(0));

	return true;
}

DECL_IFC_FUN(ifc_isfollow)
{
	*ret = (ISARG_MOB(0) && ARG_MOB(0)->master &&
		ARG_MOB(0)->master->in_room == ARG_MOB(0)->in_room);
	return true;
}

DECL_IFC_FUN(ifc_isgood)
{
	*ret = (ISARG_MOB(0) && IS_GOOD(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_ishunting)
{
	*ret = (ISARG_MOB(0) && ARG_MOB(0)->hunting);
	return true;
}

DECL_IFC_FUN(ifc_isimmort)
{
	*ret = (VALID_PLAYER(0) && IS_IMMORTAL(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_iskey)
{
	int door;
	EXIT_DATA *ex;

	if(ISARG_OBJ(0)) {
		if(ISARG_EXIT(1)) {
			ex = ARG_EXIT(1).r ? ARG_EXIT(1).r->exit[ARG_EXIT(1).door] : NULL;
			*ret = ex && ARG_OBJ(0)->item_type == ITEM_KEY &&
				ex->door.lock.key_vnum == ARG_OBJ(0)->pIndexData->vnum;
		} else if(ISARG_STR(1))
			*ret = ARG_OBJ(0)->item_type == ITEM_KEY && (room = obj_room(ARG_OBJ(0))) &&
				(door = get_num_dir(ARG_STR(1))) != -1 && room->exit[door] &&
				room->exit[door]->door.lock.key_vnum == ARG_OBJ(0)->pIndexData->vnum;
		else *ret = false;

		return true;
	} else if(obj) {
		if(ISARG_EXIT(0)) {
			ex = ARG_EXIT(0).r ? ARG_EXIT(0).r->exit[ARG_EXIT(0).door] : NULL;
			*ret = ex && obj->item_type == ITEM_KEY &&
				ex->door.lock.key_vnum == obj->pIndexData->vnum;
		} else if(ISARG_STR(0))
			*ret = obj->item_type == ITEM_KEY && (room = obj_room(obj)) &&
				(door = get_num_dir(ARG_STR(0))) != -1 && room->exit[door] &&
				room->exit[door]->door.lock.key_vnum == obj->pIndexData->vnum;
		else *ret = false;

		return true;
	}

	return false;
}

DECL_IFC_FUN(ifc_ismorphed)
{
	*ret = (ISARG_MOB(0) && IS_MORPHED(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_ismystic)
{
	*ret = (ISARG_MOB(0) && IS_MYSTIC(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isneutral)
{
	*ret = (ISARG_MOB(0) && IS_NEUTRAL(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isnpc)
{
	*ret = VALID_NPC(0);
	return true;
}

DECL_IFC_FUN(ifc_ison)
{
	if(!ISARG_MOB(0)) return false;

	*ret = false;
	if(ISARG_STR(1)) {
		if(is_number(ARG_STR(1)))
			*ret = (ARG_MOB(0)->on && ARG_MOB(0)->on->pIndexData->vnum == atoi(ARG_STR(1)));
		else
			*ret = (ARG_MOB(0)->on && is_name(ARG_STR(1),ARG_MOB(0)->on->pIndexData->name));
	} else if(ISARG_NUM(1))
		*ret = (ARG_MOB(0)->on && ARG_MOB(0)->on->pIndexData->vnum == ARG_NUM(1));
	else if(ISARG_OBJ(1))
		*ret = (ARG_MOB(0)->on == ARG_OBJ(1));
	else if(argc == 1)
		*ret = (obj && ARG_MOB(0)->on == obj);

	return true;
}

DECL_IFC_FUN(ifc_ispc)
{
	*ret = VALID_PLAYER(0);
	return true;
}

DECL_IFC_FUN(ifc_isprey)
{
	if(ISARG_MOB(0)) {
		if(ISARG_MOB(1))
			*ret = (ARG_MOB(0)->hunting == ARG_MOB(1))?true:false;
		else
			*ret = (mob->hunting == ARG_MOB(0))?true:false;

		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_ispulling)
{
	*ret = false;
	if(ISARG_MOB(0)) {
		if(ISARG_OBJ(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->pulled_cart == ARG_OBJ(1));
		else if(ISARG_STR(1)) {
			if(is_number(ARG_STR(1)))
				*ret = (ARG_MOB(0) && ARG_MOB(0)->pulled_cart && ARG_MOB(0)->pulled_cart->pIndexData->vnum == atoi(ARG_STR(1)));
			else
				*ret = (ARG_MOB(0) && ARG_MOB(0)->pulled_cart && is_name(ARG_STR(1),ARG_MOB(0)->pulled_cart->pIndexData->name));
		} else if(ISARG_NUM(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->pulled_cart && ARG_MOB(0)->pulled_cart->pIndexData->vnum == ARG_NUM(1));
		else if(argc == 1)
			*ret = (ARG_MOB(0) && ARG_MOB(0)->pulled_cart);
	}
	return true;
}

DECL_IFC_FUN(ifc_ispullingrelic)
{
	*ret = (ISARG_MOB(0) && is_pulling_relic(ARG_MOB(0)) &&
	    	(!ARG_STR(1) || !*ARG_STR(1) ||
	    	ARG_MOB(0)->pulled_cart->pIndexData->vnum == flag_value( relic_types,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_isquesting)
{
	*ret = VALID_PLAYER(0) && ON_QUEST(ARG_MOB(0));
	return true;
}

DECL_IFC_FUN(ifc_isrepairable)
{
	*ret = ISARG_OBJ(0) && (ARG_OBJ(0)->times_fixed < ARG_OBJ(0)->times_allowed_fixed);
	return true;
}

DECL_IFC_FUN(ifc_isridden)
{
	*ret = ISARG_MOB(0) && ARG_MOB(0)->rider;
	return true;
}

DECL_IFC_FUN(ifc_isrider)
{
	*ret = false;
	if(ISARG_MOB(0) && ARG_MOB(0)->riding) {
		if(ISARG_MOB(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->rider == ARG_MOB(1));
		else if(ISARG_STR(1)) {
			if(is_number(ARG_STR(1)))
				*ret = (ARG_MOB(0) && ARG_MOB(0)->rider && IS_NPC(ARG_MOB(0)->rider) && ARG_MOB(0)->rider->pIndexData->vnum == atoi(ARG_STR(1)));
			else
				*ret = (ARG_MOB(0) && ARG_MOB(0)->rider && is_name(ARG_STR(1),ARG_MOB(0)->rider->name));
		} else if(ISARG_NUM(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->rider && IS_NPC(ARG_MOB(0)->rider) && ARG_MOB(0)->rider->pIndexData->vnum == ARG_NUM(1));
	}
	return true;
}

DECL_IFC_FUN(ifc_isriding)
{
	*ret = false;
	if(ISARG_MOB(0) && ARG_MOB(0)->riding) {
		if(ISARG_MOB(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->mount == ARG_MOB(1));
		else if(ISARG_STR(1)) {
			if(is_number(ARG_STR(1)))
				*ret = (ARG_MOB(0) && ARG_MOB(0)->mount && IS_NPC(ARG_MOB(0)->mount) && ARG_MOB(0)->mount->pIndexData->vnum == atoi(ARG_STR(1)));
			else
				*ret = (ARG_MOB(0) && ARG_MOB(0)->mount && is_name(ARG_STR(1),ARG_MOB(0)->mount->name));
		} else if(ISARG_NUM(1))
			*ret = (ARG_MOB(0) && ARG_MOB(0)->mount && IS_NPC(ARG_MOB(0)->mount) && ARG_MOB(0)->mount->pIndexData->vnum == ARG_NUM(1));
		else if(argc == 1)
			*ret = true;
	}
	return true;
}

DECL_IFC_FUN(ifc_isroomdark)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room_is_dark(room) : false;
	return true;
}

DECL_IFC_FUN(ifc_isscribing)
{
	int sn;

	*ret = ISARG_MOB(0) && ARG_MOB(0)->scribe_sn > 0 &&
		(!ISARG_STR(1) || !*ARG_STR(1) ||
			((sn = skill_lookup(ARG_STR(1))) > 0 &&
			(	ARG_MOB(0)->scribe_sn == sn ||
				ARG_MOB(0)->scribe_sn2 == sn ||
				ARG_MOB(0)->scribe_sn3 == sn )));
	return true;
}

DECL_IFC_FUN(ifc_isshifted)
{
	*ret = (ISARG_MOB(0) && IS_SHIFTED(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_isshooting)
{
	*ret = ISARG_MOB(0) && ARG_MOB(0)->projectile_dir >= 0 &&
		(!ARG_STR(1) || !*ARG_STR(1) ||
			ARG_MOB(0)->projectile_dir == get_num_dir(ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_isshopkeeper)
{
	*ret = VALID_NPC(0) && ARG_MOB(0)->shop;
	return true;
}

DECL_IFC_FUN(ifc_issustained)
{
	*ret = (ISARG_MOB(0) && is_sustained(ARG_MOB(0)));
	return true;
}

DECL_IFC_FUN(ifc_istarget)
{
	if(ISARG_MOB(0)) {
		if(mob) *ret = (mob->progs->target == ARG_MOB(0));
		else if(obj) *ret = (obj->progs->target == ARG_MOB(0));
		else if(room) *ret = (room->progs->target == ARG_MOB(0));
		else if(token) *ret = (token->progs->target == ARG_MOB(0));
		else *ret = false;
	} else *ret = false;
	return true;
}

DECL_IFC_FUN(ifc_istattooing)
{
	int sn;

	*ret = ISARG_MOB(0) && ARG_MOB(0)->ink_sn > 0 &&
		(!ISARG_STR(1) || !*ARG_STR(1) ||
			((sn = skill_lookup(ARG_STR(1))) > 0 &&
			(	ARG_MOB(0)->ink_sn == sn ||
				ARG_MOB(0)->ink_sn2 == sn ||
				ARG_MOB(0)->ink_sn3 == sn )));
	return true;
}


DECL_IFC_FUN(ifc_isvisible)
{
	if(mob) {
		if(ISARG_MOB(0)) *ret = can_see(mob,ARG_MOB(0));
		else if(ISARG_OBJ(0)) *ret = can_see_obj(mob,ARG_OBJ(0));
		else if(ISARG_ROOM(0)) *ret = can_see_room(mob,ARG_ROOM(0));
		else *ret = false;
	} else *ret = true;

	return true;
}

DECL_IFC_FUN(ifc_isvisibleto)
{
	if(ARG_MOB(0)) {
		if(ISARG_MOB(1)) *ret = can_see(ARG_MOB(0),ARG_MOB(1));
		else if(ISARG_OBJ(1)) *ret = can_see_obj(ARG_MOB(0),ARG_OBJ(1));
		else if(ISARG_ROOM(1)) *ret = can_see_room(ARG_MOB(0),ARG_ROOM(1));
		else if(mob) *ret = can_see(ARG_MOB(0),mob);
		else if(obj) *ret = can_see_obj(ARG_MOB(0),obj);
		else if(room) *ret = can_see_room(ARG_MOB(0),room);
		else *ret = false;
	} else *ret = true;
	return true;
}

DECL_IFC_FUN(ifc_isworn)
{
	*ret = (ISARG_OBJ(0) && ARG_OBJ(0)->wear_loc != WEAR_NONE);
	return true;
}

DECL_IFC_FUN(ifc_lastreturn)
{
	if(mob) *ret = mob->progs->lastreturn;
	else if(obj) *ret = obj->progs->lastreturn;
	else if(room) *ret = room->progs->lastreturn;
	else if(token) *ret = token->progs->lastreturn;
	else return false;
	if(*ret < 0) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_level)
{
	if( ISARG_MOB(0) ) *ret = ARG_MOB(0)->tot_level;
	else if(ISARG_OBJ(0)) *ret = ARG_OBJ(0)->level;
	else
		*ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_liquid)
{
	*ret = ISARG_OBJ(0) &&
		(ARG_OBJ(0)->item_type == ITEM_DRINK_CON || ARG_OBJ(0)->item_type == ITEM_FOUNTAIN) &&
		(ARG_OBJ(0)->value[2] == liq_lookup(ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_manaregen)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->mana_rate : 0;
	return true;
}

DECL_IFC_FUN(ifc_material)
{
	if(ISARG_MOB(0)) *ret = ARG_MOB(0)->material && !str_cmp(ARG_STR(1), ARG_MOB(0)->material);
	else if(ISARG_OBJ(0)) *ret = ARG_OBJ(0)->material && !str_cmp(ARG_STR(1), ARG_OBJ(0)->material);
	else *ret = false;
	return true;
}

DECL_IFC_FUN(ifc_maxcarry)
{
	if (ISARG_OBJ(0)) {
		obj = ARG_OBJ(0);
		if(obj->item_type == ITEM_CART || obj->item_type == ITEM_CONTAINER)
			*ret = obj->value[3];
		else if(obj->item_type == ITEM_WEAPON_CONTAINER)
			*ret = obj->value[2];
		else
			return false;
	} else if(ISARG_MOB(0))
		*ret = can_carry_n(ARG_MOB(0));
	else
		return true;

	*ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_maxhit)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->max_hit : 0;
	return true;
}

DECL_IFC_FUN(ifc_maxmana)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->max_mana : 0;
	return true;
}

DECL_IFC_FUN(ifc_maxmove)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->max_move : 0;
	return true;
}

DECL_IFC_FUN(ifc_maxweight)
{
	if (ISARG_OBJ(0)) {
		obj = ARG_OBJ(0);
		if(obj->item_type == ITEM_CART ||
			obj->item_type == ITEM_CONTAINER ||
			obj->item_type == ITEM_WEAPON_CONTAINER) {
			*ret = obj->value[0];
			return true;
		}
	} else if(ARG_MOB(0)) {
		*ret = can_carry_w(ARG_MOB(0));
		return true;
	}

	return false;
}

DECL_IFC_FUN(ifc_mobexists)
{
	if(ISARG_NUM(0)) {
		MOB_INDEX_DATA *pMobIndex;

		if (!(pMobIndex = get_mob_index(ARG_NUM(0))))
			*ret = false;
		else
			*ret = (bool)(int)(get_char_world_index(NULL, pMobIndex) && 1);
		return true;
	} else if(ISARG_STR(0)) {
		if (is_number(ARG_STR(0))) {
			MOB_INDEX_DATA *pMobIndex;

			if (!(pMobIndex = get_mob_index(atol(ARG_STR(0)))))
				*ret = false;
			else
				*ret = (bool)(int)(get_char_world_index(NULL, pMobIndex) && 1);
		} else
			*ret = ((bool)(int) (get_char_world(NULL, ARG_STR(0)) && 1));
		return true;
	}
	return true;
}

DECL_IFC_FUN(ifc_mobhere)
{
	if(ISARG_NUM(0))
		*ret = ((bool)(int)(get_mob_vnum_room(mob, obj, room, token, ARG_NUM(0)) && 1));
	else if(ISARG_STR(0)) {
		if(is_number(ARG_STR(0)))
			*ret = ((bool)(int)(get_mob_vnum_room(mob, obj, room, token, atol(ARG_STR(0))) && 1));
		else
			*ret = ((bool)(int)(get_char_room(mob, obj ? obj_room(obj) : (token ? token_room(token) : room), ARG_STR(0)) && 1));
	} else if(ISARG_MOB(0)) {
		if(mob) *ret = mob->in_room && ARG_MOB(0)->in_room == mob->in_room;
		else if(obj) *ret = obj_room(obj) && ARG_MOB(0)->in_room == obj_room(obj);
		else if(room) *ret = ARG_MOB(0)->in_room == room;
		else if(token) *ret = token_room(token) && ARG_MOB(0)->in_room == token_room(token);
		else *ret = false;
	}
	return true;
}

DECL_IFC_FUN(ifc_mobs)
{
	*ret = count_people_room(mob, obj, room, token, 2);
	return true;
}

DECL_IFC_FUN(ifc_money)
{
	*ret = ISARG_MOB(0) ? (ARG_MOB(0)->gold*100 + ARG_MOB(0)->silver) : 0;
	return true;
}

DECL_IFC_FUN(ifc_monkills)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->monster_kills : 0;
	return true;
}

// Returns the current game day
DECL_IFC_FUN(ifc_month)
{
	*ret = time_info.month;
	return true;
}

DECL_IFC_FUN(ifc_moveregen)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->move_rate : 0;
	return true;
}

DECL_IFC_FUN(ifc_name)
{
	if(!ISARG_STR(1)) return false;
	if(ISARG_MOB(0)) *ret = is_name(ARG_STR(1), ARG_MOB(0)->name);
	else if(ISARG_OBJ(0)) *ret = is_name(ARG_STR(1), ARG_OBJ(0)->name);
	else if(ISARG_TOK(0)) *ret = is_name(ARG_STR(1), ARG_TOK(0)->name);
	else if(ISARG_AREA(0)) *ret = is_name(ARG_STR(1), ARG_AREA(0)->name);
	else *ret = false;
	return true;
}

DECL_IFC_FUN(ifc_objcond)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->condition : 0;
	return true;
}

DECL_IFC_FUN(ifc_objcost)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->cost : 0;
	return true;
}

DECL_IFC_FUN(ifc_objexists)
{
	if(!ISARG_STR(0)) return false;
	*ret = ((bool)(int) (get_obj_world(NULL, ARG_STR(0)) && 1));
	return true;
}

DECL_IFC_FUN(ifc_objextra)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && IS_SET(ARG_OBJ(0)->extra[0], flag_value_ifcheck(extra_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_objextra2)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && IS_SET(ARG_OBJ(0)->extra[1], flag_value_ifcheck(extra2_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_objextra3)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && IS_SET(ARG_OBJ(0)->extra[2], flag_value_ifcheck(extra3_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_objextra4)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && IS_SET(ARG_OBJ(0)->extra[3], flag_value_ifcheck(extra4_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_objhere)
{
	if(ISARG_NUM(0))
		*ret = ((bool)(int)(get_obj_vnum_room(mob, obj, room, token, ARG_NUM(0)) && 1));
	else if(ISARG_STR(0)) {
		if(is_number(ARG_STR(0)))
			*ret = ((bool)(int)(get_obj_vnum_room(mob, obj, room, token, atol(ARG_STR(0))) && 1));
		else
			*ret = ((bool)(int)(get_obj_here(mob, obj ? obj_room(obj) : (token ? token_room(token) : room), ARG_STR(0)) && 1));
	} else if(ISARG_OBJ(0)) {
		if(mob) *ret = mob->in_room && obj_room(ARG_OBJ(0)) == mob->in_room;
		else if(obj) *ret = obj_room(obj) && obj_room(ARG_OBJ(0)) == obj_room(obj);
		else if(room) *ret = obj_room(ARG_OBJ(0)) == room;
		else if(token) *ret = token_room(token) && obj_room(ARG_OBJ(0)) == token_room(token);
		else *ret = false;
	}
	else
		*ret = false;
	return true;
}

DECL_IFC_FUN(ifc_objmaxweight)
{
	int val;
	if (ISARG_OBJ(0)) {
		obj = ARG_OBJ(0);
		if(obj->item_type == ITEM_CART || obj->item_type == ITEM_CONTAINER)
			val = obj->value[4];
		else if(obj->item_type == ITEM_WEAPON_CONTAINER)
			val = obj->value[3];
		else
			return false;
		if(val < 1) val = 100;
		*ret = obj->value[0] * 100 / val;
		return true;
	}
	return true;
}

DECL_IFC_FUN(ifc_objtimer)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->timer : 0;
	return true;
}

DECL_IFC_FUN(ifc_objtype)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == item_lookup(ARG_STR(1));
	return true;
}

DECL_IFC_FUN(ifc_objval0)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[0] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval1)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[1] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval2)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[2] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval3)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[3] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval4)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[4] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval5)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[5] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval6)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[6] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objval7)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->value[7] : 0;
	return true;
}

DECL_IFC_FUN(ifc_objwear)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && IS_SET(ARG_OBJ(0)->wear_flags, flag_value_ifcheck(wear_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_objwearloc)
{
	*ret = (ISARG_OBJ(0) && ISARG_STR(1) && ARG_OBJ(0)->wear_loc == flag_value(wear_loc_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_objweight)
{
	*ret = ISARG_OBJ(0) ? get_obj_weight(ARG_OBJ(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_objweightleft)
{
	int val;
	if (ISARG_OBJ(0)) {
		obj = ARG_OBJ(0);
		if(obj->item_type == ITEM_CART || obj->item_type == ITEM_CONTAINER)
			val = obj->value[4];
		else if(obj->item_type == ITEM_WEAPON_CONTAINER)
			val = obj->value[3];
		else
			return false;
		if(val < 1) val = 100;
		*ret = (obj->value[0] * 100 / val) - get_obj_weight_container(obj);
		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_off)
{
	*ret = (ISARG_MOB(0) && IS_SET(ARG_MOB(0)->off_flags, flag_value_ifcheck( off_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_order)
{
	*ret = (mob || obj) ? get_order(mob, obj) : 0;
	return true;
}

DECL_IFC_FUN(ifc_parts)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->parts, flag_value_ifcheck(part_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_people)
{
	*ret = count_people_room(mob, obj, room, token, 0);
	return true;
}

DECL_IFC_FUN(ifc_permcon)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->perm_stat[STAT_CON] : 0;
	return true;
}

DECL_IFC_FUN(ifc_permdex)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->perm_stat[STAT_DEX] : 0;
	return true;
}

DECL_IFC_FUN(ifc_permint)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->perm_stat[STAT_INT] : 0;
	return true;
}

DECL_IFC_FUN(ifc_permstr)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->perm_stat[STAT_STR] : 0;
	return true;
}

DECL_IFC_FUN(ifc_permwis)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->perm_stat[STAT_WIS] : 0;
	return true;
}

DECL_IFC_FUN(ifc_pgroupcon)
{
	*ret = ISARG_MOB(0) ? get_perm_group_stat(ARG_MOB(0),STAT_CON) : 0;
	return true;
}

DECL_IFC_FUN(ifc_pgroupdex)
{
	*ret = ISARG_MOB(0) ? get_perm_group_stat(ARG_MOB(0),STAT_DEX) : 0;
	return true;
}

DECL_IFC_FUN(ifc_pgroupint)
{
	*ret = ISARG_MOB(0) ? get_perm_group_stat(ARG_MOB(0),STAT_INT) : 0;
	return true;
}

DECL_IFC_FUN(ifc_pgroupstr)
{
	*ret = ISARG_MOB(0) ? get_perm_group_stat(ARG_MOB(0),STAT_STR) : 0;
	return true;
}

DECL_IFC_FUN(ifc_pgroupwis)
{
	*ret = ISARG_MOB(0) ? get_perm_group_stat(ARG_MOB(0),STAT_WIS) : 0;
	return true;
}

// Gets the player's pk fight count: wins + loss
DECL_IFC_FUN(ifc_pkfights)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills) : 0;
	return true;
}

// Gets the player's pk losses
DECL_IFC_FUN(ifc_pkloss)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->player_deaths : 0;
	return true;
}

// Gets the player's pk win ratio: 0 = 0.0% to 1000 = 100.0%
DECL_IFC_FUN(ifc_pkratio)
{
	int val;

	if (VALID_PLAYER(0)) {
		val = (ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills);
		if(val > 0) val = 1000 * ARG_MOB(0)->player_kills / val;
		*ret = val;
		return true;
	}

	return false;
}

// Gets the player's pk wins
DECL_IFC_FUN(ifc_pkwins)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->player_kills : 0;
	return true;
}

DECL_IFC_FUN(ifc_players)
{
	*ret = count_people_room(mob, obj, room, token, 1);
	return true;
}

DECL_IFC_FUN(ifc_pneuma)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pneuma : 0;
	return true;
}

DECL_IFC_FUN(ifc_portal)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_PORTAL &&
		IS_SET(ARG_OBJ(0)->value[2], flag_value_ifcheck(portal_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_portalexit)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_PORTAL &&
		IS_SET(ARG_OBJ(0)->value[1], flag_value_ifcheck(portal_exit_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_pos)
{
//	char buf[MIL];
//	sprintf(buf,"ifc_pos: %s(%d), %s(%d), %s(%08X)",
//		(mob ? HANDLE(mob) : "(MOB)"),
//		(mob ? VNUM(mob) : -1),
//		(ARG_MOB(0) ? HANDLE(ARG_MOB(0)) : "(MOB)"),
//		(ARG_MOB(0) ? VNUM(ARG_MOB(0)) : -1),
//		(ARG_STR(1) ? ARG_STR(1) : "(null)"),
//		ARG_STR(1));
//	log_string(buf);
	*ret = ISARG_MOB(0) && VALID_STR(1) && ARG_MOB(0)->position == position_lookup(ARG_STR(1));
	return true;
}

DECL_IFC_FUN(ifc_practices)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->practice : 0;
	return true;
}

DECL_IFC_FUN(ifc_quest)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->questpoints : 0;
	return true;
}

DECL_IFC_FUN(ifc_race)
{
	*ret = ISARG_MOB(0) && VALID_STR(1) && ARG_MOB(0)->race == race_lookup(ARG_STR(1));
	return true;
}

// Random number check
// Forms:
//	rand <num>		true: [0:99] < num
//	rand <num1> <num2>	true: [0:num2] < num1
DECL_IFC_FUN(ifc_rand)
{
	if(ISARG_NUM(1))
		*ret = (number_range(0,ARG_NUM(1)-1) < ARG_NUM(0));
	else
		*ret = (number_percent() < ARG_NUM(0));
	return true;
}

DECL_IFC_FUN(ifc_res)
{
	*ret = (ISARG_MOB(0) && VALID_STR(1) && IS_SET(ARG_MOB(0)->res_flags, flag_value_ifcheck(res_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_room)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->vnum : 0;
	return true;
}

DECL_IFC_FUN(ifc_roomflag)
{
	// Wilderness format
	if(ISARG_NUM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		WILDS_DATA *pWilds;
		WILDS_TERRAIN *pTerrain;

		if(!ISARG_STR(3)) return false;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = false;
		else if((room = get_wilds_vroom(pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = (IS_SET(room->room_flag[0], flag_value_ifcheck(room_flags,ARG_STR(3))));
		else if(!(pTerrain = get_terrain_by_coors (pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = false;
		else
			*ret = (IS_SET(pTerrain->template->room_flag[0], flag_value_ifcheck(room_flags,ARG_STR(3))));

	} else {
		if(!ISARG_STR(1)) return false;
		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
		else return false;
		*ret = (room && IS_SET(room->room_flag[0], flag_value_ifcheck(room_flags,ARG_STR(1))));
	}

	return true;
}

DECL_IFC_FUN(ifc_roomflag2)
{
	// Wilderness format
	if(ISARG_NUM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		WILDS_DATA *pWilds;
		WILDS_TERRAIN *pTerrain;

		if(!ISARG_STR(3)) return false;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = false;
		else if((room = get_wilds_vroom(pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = (IS_SET(room->room_flag[1], flag_value_ifcheck(room2_flags,ARG_STR(3))));
		else if(!(pTerrain = get_terrain_by_coors (pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = false;
		else
			*ret = (IS_SET(pTerrain->template->room_flag[1], flag_value_ifcheck(room2_flags,ARG_STR(3))));

	} else {
		if(!ISARG_STR(1)) return false;
		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
		else return false;
		*ret = (room && IS_SET(room->room_flag[1], flag_value_ifcheck(room2_flags,ARG_STR(1))));
	}

	return true;
}

DECL_IFC_FUN(ifc_roomweight)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = get_room_weight(room,true,true,false);
	return true;
}

DECL_IFC_FUN(ifc_sector)
{
	// Wilderness format
	if(ISARG_NUM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		WILDS_DATA *pWilds;
		WILDS_TERRAIN *pTerrain;

		if(!ISARG_STR(3)) return false;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = false;
		else if((room = get_wilds_vroom(pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = (room->sector_type == flag_value_ifcheck( sector_flags,ARG_STR(3)));
		else if(!(pTerrain = get_terrain_by_coors (pWilds, ARG_NUM(1), ARG_NUM(2))))
			*ret = false;
		else
			*ret = (pTerrain->template->sector_type == flag_value( sector_flags,ARG_STR(3)));

	} else {
		if(!ISARG_STR(1)) return false;
		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
		else return false;

		*ret = (room && room->sector_type == flag_value( sector_flags,ARG_STR(1)));
	}
	return true;
}

DECL_IFC_FUN(ifc_sex)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->sex : 0;
	return true;
}

DECL_IFC_FUN(ifc_silver)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->silver : 0;
	return true;
}

DECL_IFC_FUN(ifc_skeyword)
{
	if(!ISARG_STR(1)) return false;
	if(ISARG_MOB(0)) *ret = IS_NPC(ARG_MOB(0)) && is_name(ARG_STR(1), ARG_MOB(0)->pIndexData->skeywds);
	else if(ISARG_OBJ(0)) *ret = is_name(ARG_STR(1), ARG_OBJ(0)->pIndexData->skeywds);
	else return false;
	return true;
}

DECL_IFC_FUN(ifc_skill)
{
	int sn;

	*ret = (VALID_PLAYER(0) && ISARG_STR(1) && ((sn = skill_lookup(ARG_STR(1))) > 0)) ?
			get_skill(ARG_MOB(0), sn) : 0;
	return true;
}

DECL_IFC_FUN(ifc_statcon)
{
	*ret = ISARG_MOB(0) ? get_curr_stat(ARG_MOB(0),STAT_CON) : 0;
	return true;
}

DECL_IFC_FUN(ifc_statdex)
{
	*ret = ISARG_MOB(0) ? get_curr_stat(ARG_MOB(0),STAT_DEX) : 0;
	return true;
}

DECL_IFC_FUN(ifc_statint)
{
	*ret = ISARG_MOB(0) ? get_curr_stat(ARG_MOB(0),STAT_INT) : 0;
	return true;
}

DECL_IFC_FUN(ifc_statstr)
{
	*ret = ISARG_MOB(0) ? get_curr_stat(ARG_MOB(0),STAT_STR) : 0;
	return true;
}

DECL_IFC_FUN(ifc_statwis)
{
	*ret = ISARG_MOB(0) ? get_curr_stat(ARG_MOB(0),STAT_WIS) : 0;
	return true;
}

DECL_IFC_FUN(ifc_testskill)
{
	int sn;
	*ret = (VALID_PLAYER(0) && ISARG_STR(1) && ((sn = skill_lookup(ARG_STR(1))) > 0) &&
			number_percent() < get_skill(ARG_MOB(0), sn));
	return true;
}

DECL_IFC_FUN(ifc_thirst)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->condition[COND_THIRST] : 0;
	return true;
}

DECL_IFC_FUN(ifc_stoned)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->condition[COND_STONED] : 0;
	return true;
}

DECL_IFC_FUN(ifc_tokencount)
{
	TOKEN_INDEX_DATA *ti = NULL;
	TOKEN_DATA *tok;
	int i;

	if((ISARG_NUM(1) && !(ti = get_token_index(ARG_NUM(1)))))
		return false;

	if(ISARG_MOB(0)) tok = ARG_MOB(0)->tokens;
	else if(ISARG_OBJ(0)) tok = ARG_OBJ(0)->tokens;
	else if(ISARG_ROOM(0)) tok = ARG_ROOM(0)->tokens;
	else return false;

	for(i = 0;tok;tok = tok->next) if(!ti || (tok->pIndexData == ti)) ++i;

	*ret = i;
	return true;
}

DECL_IFC_FUN(ifc_tokenexists)
{
	TOKEN_INDEX_DATA *ti;
	*ret = (ISARG_NUM(0) && (ti = get_token_index(ARG_NUM(0))) && ti->loaded > 0);
	return true;
}

DECL_IFC_FUN(ifc_tokentimer)
{
	TOKEN_DATA *tok;

	if(ISARG_MOB(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_char(ARG_MOB(0), ARG_NUM(1), ARG_NUM(2));

	} else if(ISARG_OBJ(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_obj(ARG_OBJ(0), ARG_NUM(1), ARG_NUM(2));

	} else if(ISARG_ROOM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_room(ARG_ROOM(0), ARG_NUM(1), ARG_NUM(2));

	} else if(ISARG_MOB(0) && ISARG_NUM(1)) {
		tok = get_token_char(ARG_MOB(0), ARG_NUM(1), 1);

	} else if(ISARG_OBJ(0) && ISARG_NUM(1)) {
		tok = get_token_obj(ARG_OBJ(0), ARG_NUM(1), 1);

	} else if(ISARG_ROOM(0) && ISARG_NUM(1)) {
		tok = get_token_room(ARG_ROOM(0), ARG_NUM(1), 1);

	} else if(ISARG_TOK(0)) {
		tok = ARG_TOK(0);
	} else
		return false;

	// If the token doesn't exist on the entity, any checks on its values are always false.
	if(!tok) return false;

	*ret = tok->timer;
	return true;
}

DECL_IFC_FUN(ifc_tokentype)
{
	return false;
}

DECL_IFC_FUN(ifc_tokenvalue)
{
	TOKEN_DATA *tok;
	int val;

	if(ISARG_MOB(0) && ISARG_NUM(1) && ISARG_NUM(2) && ISARG_NUM(3)) {
		tok = get_token_char(ARG_MOB(0), ARG_NUM(1), ARG_NUM(2));
		val = ARG_NUM(3);

	} else if(ISARG_OBJ(0) && ISARG_NUM(1) && ISARG_NUM(2) && ISARG_NUM(3)) {
		tok = get_token_obj(ARG_OBJ(0), ARG_NUM(1), ARG_NUM(2));
		val = ARG_NUM(3);

	} else if(ISARG_ROOM(0) && ISARG_NUM(1) && ISARG_NUM(2) && ISARG_NUM(3)) {
		tok = get_token_room(ARG_ROOM(0), ARG_NUM(1), ARG_NUM(2));
		val = ARG_NUM(3);

	} else if(ISARG_MOB(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_char(ARG_MOB(0), ARG_NUM(1), 1);
		val = ARG_NUM(2);

	} else if(ISARG_OBJ(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_obj(ARG_OBJ(0), ARG_NUM(1), 1);
		val = ARG_NUM(2);

	} else if(ISARG_ROOM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		tok = get_token_room(ARG_ROOM(0), ARG_NUM(1), 1);
		val = ARG_NUM(2);

	} else if(ISARG_TOK(0) && ISARG_NUM(1)) {
		tok = ARG_TOK(0);
		val = ARG_NUM(1);
	} else
		return false;

	// If the token doesn't exist on the entity, any checks on its values are always false.
	if(!tok) return false;

	if (val < 0 || val >= MAX_TOKEN_VALUES)
		return false;

	*ret = tok->value[val];
	return true;
}

DECL_IFC_FUN(ifc_totalfights)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills+
			ARG_MOB(0)->arena_deaths+ARG_MOB(0)->arena_kills+
			ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills) : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalloss)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->arena_deaths+ARG_MOB(0)->player_deaths) : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalpkfights)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills+
	    	ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills) : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalpkloss)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->player_deaths) : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalpkratio)
{
	int val;
	if(VALID_PLAYER(0)) {
		val = ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills+
			ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills;
		if(val > 0) val = 1000 * (ARG_MOB(0)->cpk_kills+ARG_MOB(0)->player_kills) / val;
		*ret = val;
		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_totalpkwins)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_kills+ARG_MOB(0)->player_kills) : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalquests)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->quests_completed : 0;
	return true;
}

DECL_IFC_FUN(ifc_totalratio)
{
	int val;
	if(VALID_PLAYER(0)) {
		val = ARG_MOB(0)->cpk_deaths+ARG_MOB(0)->cpk_kills+
			ARG_MOB(0)->arena_deaths+ARG_MOB(0)->arena_kills+
			ARG_MOB(0)->player_deaths+ARG_MOB(0)->player_kills;
		if(val > 0) val = 1000 * (ARG_MOB(0)->cpk_kills+ARG_MOB(0)->arena_kills+ARG_MOB(0)->player_kills) / val;
		*ret = val;
		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_totalwins)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->cpk_kills+ARG_MOB(0)->arena_kills+ARG_MOB(0)->player_kills) : 0;
	return true;
}

DECL_IFC_FUN(ifc_toxin)
{
	int tox;
	if(ISARG_MOB(0) && ISARG_STR(1)) {
		*ret = ((tox = toxin_lookup(ARG_STR(1))) >= 0 && tox < MAX_TOXIN) ?
			ARG_MOB(0)->toxin[tox] : 0;
		return true;
	}

	return false;
}

DECL_IFC_FUN(ifc_trains)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->train : 0;
	return true;
}

DECL_IFC_FUN(ifc_uses)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && has_item(ARG_MOB(0), -1, item_lookup(ARG_STR(1)), true));
	return true;
}

DECL_IFC_FUN(ifc_varexit)
{
	PROG_DATA * progs = NULL;
	pVARIABLE var;
	if(ISARG_MOB(0)) { progs = ARG_MOB(0)->progs; ++argv; }
	else if(ISARG_OBJ(0)) { progs = ARG_OBJ(0)->progs; ++argv; }
	else if(ISARG_ROOM(0)) { progs = ARG_ROOM(0)->progs; ++argv; }
	else if(ISARG_TOK(0)) { progs = ARG_TOK(0)->progs; ++argv; }
	else if(mob) progs = mob->progs;
	else if(obj) progs = obj->progs;
	else if(room) progs = room->progs;
	else if(token) progs = token->progs;

	if(progs && progs->vars && ISARG_STR(0) && ISARG_STR(1)) {
		int door = get_num_dir(ARG_STR(1));

		var = variable_get(progs->vars,ARG_STR(0));

		if(var && var->type == VAR_EXIT) {
			*ret = var->_.door.r && var->_.door.door == door;
			return true;
		}
	}
	return false;
}

DECL_IFC_FUN(ifc_varnumber)
{
	PROG_DATA * progs = NULL;
	pVARIABLE var;
	if(ISARG_MOB(0)) { progs  = ARG_MOB(0)->progs; ++argv; }
	else if(ISARG_OBJ(0)) { progs  = ARG_OBJ(0)->progs; ++argv; }
	else if(ISARG_ROOM(0)) { progs  = ARG_ROOM(0)->progs; ++argv; }
	else if(ISARG_TOK(0)) { progs  = ARG_TOK(0)->progs; ++argv; }
	else if(ISARG_AREA(0)) { progs = ARG_AREA(0)->progs; ++argv; }
	else if(mob) progs  = mob->progs;
	else if(obj) progs  = obj->progs;
	else if(room) progs  = room->progs;
	else if(token) progs  = token->progs;
	else if(area) progs = area->progs;

	if(progs && progs->vars && ISARG_STR(0)) {
		var = variable_get(progs->vars,ARG_STR(0));

		if(var && var->type == VAR_INTEGER) {
			*ret = var->_.i;
			return true;
		}
	}
	return false;
}

DECL_IFC_FUN(ifc_varbool)
{
	PROG_DATA * progs = NULL;
	pVARIABLE var;
	if(ISARG_MOB(0)) { progs  = ARG_MOB(0)->progs; ++argv; }
	else if(ISARG_OBJ(0)) { progs  = ARG_OBJ(0)->progs; ++argv; }
	else if(ISARG_ROOM(0)) { progs  = ARG_ROOM(0)->progs; ++argv; }
	else if(ISARG_TOK(0)) { progs  = ARG_TOK(0)->progs; ++argv; }
	else if(ISARG_AREA(0)) { progs = ARG_AREA(0)->progs; ++argv; }
	else if(mob) progs  = mob->progs;
	else if(obj) progs  = obj->progs;
	else if(room) progs  = room->progs;
	else if(token) progs  = token->progs;
	else if(area) progs = area->progs;

	if(progs && progs->vars && ISARG_STR(0)) {
		var = variable_get(progs->vars,ARG_STR(0));

		if(var && var->type == VAR_BOOLEAN) {
			*ret = var->_.i == true;
			return true;
		}
	}
	return false;
}

DECL_IFC_FUN(ifc_vardefined)
{
	PROG_DATA * progs = NULL;
	pVARIABLE var;
	if(ISARG_MOB(0)) { progs = ARG_MOB(0)->progs; ++argv; }
	else if(ISARG_OBJ(0)) { progs = ARG_OBJ(0)->progs; ++argv; }
	else if(ISARG_ROOM(0)) { progs = ARG_ROOM(0)->progs; ++argv; }
	else if(ISARG_TOK(0)) { progs = ARG_TOK(0)->progs; ++argv; }
	else if(ISARG_AREA(0)) { progs = ARG_AREA(0)->progs; ++argv; }
	else if(mob) progs = mob->progs;
	else if(obj) progs = obj->progs;
	else if(room) progs = room->progs;
	else if(token) progs = token->progs;
	else if(area) progs = area->progs;

//	if(wiznet_script) {
//		sprintf(buf, "vardefined searching for '%s'", ARG_STR(0));
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}

	if(progs && progs->vars && ISARG_STR(0)) {
		var = variable_get(progs->vars,ARG_STR(0));

//		if( var && wiznet_script ) {
//			sprintf(buf, "vardefined found variable: '%s'", var->name);
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}

		*ret = var ? true : false;
		return true;
	}
	return false;
}

DECL_IFC_FUN(ifc_varstring)
{
	PROG_DATA * progs = NULL;
	pVARIABLE var;
	if(ISARG_MOB(0)) { progs = ARG_MOB(0)->progs; ++argv; }
	else if(ISARG_OBJ(0)) { progs = ARG_OBJ(0)->progs; ++argv; }
	else if(ISARG_ROOM(0)) { progs = ARG_ROOM(0)->progs; ++argv; }
	else if(ISARG_TOK(0)) { progs = ARG_TOK(0)->progs; ++argv; }
	else if(ISARG_AREA(0)) { progs = ARG_AREA(0)->progs; ++argv; }
	else if(mob) progs = mob->progs;
	else if(obj) progs = obj->progs;
	else if(room) progs = room->progs;
	else if(token) progs = token->progs;
	else if(area) progs = area->progs;

	if(progs && progs->vars && ISARG_STR(0) && ISARG_STR(1)) {
		var = variable_get(progs->vars,ARG_STR(0));

		if(var && (var->type == VAR_STRING || var->type == VAR_STRING_S)) {
			*ret = is_name(ARG_STR(1),var->_.s);
			return true;
		}
	}
	return false;
}

DECL_IFC_FUN(ifc_vnum)
{
	if(ISARG_MOB(0)) *ret = IS_NPC(ARG_MOB(0)) ? ARG_MOB(0)->pIndexData->vnum : 0;
	else if(ISARG_OBJ(0)) *ret = ARG_OBJ(0)->pIndexData->vnum;
	else if(ISARG_ROOM(0)) *ret = ARG_ROOM(0)->vnum;
	else if(ISARG_TOK(0)) *ret = ARG_TOK(0)->pIndexData->vnum;
	else if(ISARG_DUNGEON(0)) *ret = ARG_DUNGEON(0)->index->vnum;
	else if(ISARG_SHIP(0)) *ret = ARG_SHIP(0)->index->vnum;
	else *ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_vuln)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->vuln_flags, flag_value_ifcheck(vuln_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_weapon)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_WEAPON &&
		IS_SET(ARG_OBJ(0)->value[4], flag_value_ifcheck(weapon_type2,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_weapontype)
{
	*ret = ISARG_OBJ(0) && ARG_OBJ(0)->item_type == ITEM_WEAPON &&
		ARG_OBJ(0)->value[0] == weapon_type(ARG_STR(1));
	return true;
}

DECL_IFC_FUN(ifc_weaponskill)
{
	int sn;

	*ret = (VALID_PLAYER(0) && ISARG_OBJ(1) && ((sn = get_objweapon_sn(ARG_OBJ(1))) > 0)) ?
			get_skill(ARG_MOB(0), sn) : 0;
	return true;
}

DECL_IFC_FUN(ifc_wears)
{
	if(ISARG_MOB(0)) {
		if(ISARG_NUM(1)) {
			*ret = (int)has_item(ARG_MOB(0), ARG_NUM(1), -1, true);
		} else if(ISARG_STR(1)) {
			if (is_number(ARG_STR(1)))
				*ret = (int)has_item(ARG_MOB(0), atol(ARG_STR(1)), -1, true);
			else
				*ret = (int)(get_obj_wear(ARG_MOB(0), ARG_STR(1), true) && 1);
		} else if(ISARG_OBJ(1)) {
			*ret = (ARG_OBJ(1)->carried_by == ARG_MOB(0) && ARG_OBJ(1)->wear_loc != WEAR_NONE) ||
				(ARG_OBJ(1)->in_obj && ARG_OBJ(1)->in_obj->carried_by == ARG_MOB(0) && ARG_OBJ(1)->in_obj->wear_loc != WEAR_NONE);
		} else
			return false;
		return true;
	}
	return false;

}

DECL_IFC_FUN(ifc_wearused)
{
	int wl;

	// invalid flags will return NO_FLAG (-99)
	*ret = ISARG_MOB(0) && ((wl = flag_value(wear_loc_flags,ARG_STR(1))) > WEAR_NONE)
		&& get_eq_char(ARG_MOB(0), wl);
	return true;
}

DECL_IFC_FUN(ifc_weight)
{
	if(ISARG_MOB(0)) *ret = get_carry_weight(ARG_MOB(0));
	else if(ISARG_OBJ(0)) *ret = get_obj_weight(ARG_OBJ(0));
	else return false;

	return true;
}

DECL_IFC_FUN(ifc_weightleft)
{
	if (ISARG_OBJ(0)) {
		obj = ARG_OBJ(0);
		if(obj->item_type == ITEM_CART ||
			obj->item_type == ITEM_CONTAINER ||
			obj->item_type == ITEM_WEAPON_CONTAINER) {
			*ret = obj->value[0] - get_obj_weight_container(obj);
		} else
			*ret = 0;
		return true;
	} else if(ISARG_MOB(0)) {
		*ret = can_carry_w(ARG_MOB(0)) - get_carry_weight(ARG_MOB(0));
		return true;
	}

	return false;
}

DECL_IFC_FUN(ifc_wimpy)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->wimpy : 0;
	return true;
}

// Checks to see if the SELF object is worn by the target mobile
DECL_IFC_FUN(ifc_wornby)
{
	*ret = (obj && ISARG_MOB(0) && obj->carried_by == ARG_MOB(0) &&
		obj->wear_loc != WEAR_NONE);
	return true;
}

// Returns the current game year
DECL_IFC_FUN(ifc_year)
{
	*ret = time_info.year;
	return true;
}

/*
DECL_IFC_FUN(ifc_bit)
{
	*ret = 0;

	while(argc > 0) {
		if(ISARG_NUM(0) && ARG_NUM(0) >= 0 && ARG_NUM(0) < (sizeof(int)*8))
			*ret |= (1<<ARG_NUM(0));
		argc--;
		argv++;
	}

	return true;
}
*/

DECL_IFC_FUN(ifc_flag_act)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(act_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_act2)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(act2_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_affect)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(affect_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_affect2)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(affect2_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_container)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(container_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_exit)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(exit_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_extra)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(extra_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_extra2)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(extra2_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_extra3)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(extra3_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_extra4)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(extra4_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_form)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(form_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_furniture)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(furniture_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_imm)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(imm_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_interrupt)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(interrupt_action_types,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_off)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(off_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_part)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(part_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_portal)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(portal_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_res)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(res_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_room)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(room_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_room2)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(room2_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_vuln)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(vuln_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_weapon)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(weapon_type2,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_flag_wear)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(wear_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}


DECL_IFC_FUN(ifc_value_ac)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(ac_type,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_damage)
{
	*ret = ISARG_STR(0) ? attack_lookup(ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_position)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(position_flags,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_ranged)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(ranged_weapon_class,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_relic)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(relic_types,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_sector)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(sector_flags,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_size)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(size_flags,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_toxin)
{
	*ret = ISARG_STR(0) ? toxin_lookup(ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_type)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(type_flags,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_weapon)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(weapon_class,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_wear)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(wear_loc_flags,ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_value_moon)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(moon_phases,ARG_STR(0)) : -1;
	return true;
}

DECL_IFC_FUN(ifc_value_acstr)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(armour_strength_table,ARG_STR(0)) : -1;
	return true;
}

DECL_IFC_FUN(ifc_timer)
{
	int i;
	*ret = 0;
	if(ISARG_OBJ(0)) *ret = ARG_OBJ(0)->timer;
	else if(ISARG_MOB(0)) {
		if(!ISARG_STR(1)) {
			if(argc == 1) *ret = ARG_MOB(0)->wait;
			else return false;
		} else if(!str_prefix(ARG_STR(1),"banish")) *ret = ARG_MOB(0)->maze_time_left;
		else if(!str_prefix(ARG_STR(1),"bashed")) *ret = ARG_MOB(0)->bashed;
		else if(!str_prefix(ARG_STR(1),"bind")) *ret = ARG_MOB(0)->bind;
		else if(!str_prefix(ARG_STR(1),"bomb")) *ret = ARG_MOB(0)->bomb;
		else if(!str_prefix(ARG_STR(1),"brew")) *ret = ARG_MOB(0)->brew;
		else if(!str_prefix(ARG_STR(1),"cast")) *ret = ARG_MOB(0)->cast;
		else if(!str_prefix(ARG_STR(1),"daze")) *ret = ARG_MOB(0)->daze;
		else if(!str_prefix(ARG_STR(1),"death")) *ret = ARG_MOB(0)->time_left_death;
		else if(!str_prefix(ARG_STR(1),"fade")) *ret = ARG_MOB(0)->fade;
		else if(!str_prefix(ARG_STR(1),"hide")) *ret = ARG_MOB(0)->hide;
		else if(!str_prefix(ARG_STR(1),"music")) *ret = ARG_MOB(0)->music;
		else if(!str_prefix(ARG_STR(1),"next_quest")) *ret = ARG_MOB(0)->nextquest;
		else if(!str_prefix(ARG_STR(1),"nextquest")) *ret = ARG_MOB(0)->nextquest;
		else if(!str_prefix(ARG_STR(1),"norecall")) *ret = ARG_MOB(0)->no_recall;
		else if(!str_prefix(ARG_STR(1),"panic")) *ret = ARG_MOB(0)->panic;
		else if(!str_prefix(ARG_STR(1),"paralyzed")) *ret = ARG_MOB(0)->paralyzed;
		else if(!str_prefix(ARG_STR(1),"paroxysm")) *ret = ARG_MOB(0)->paroxysm;
		else if(!str_prefix(ARG_STR(1),"pk")) *ret = ARG_MOB(0)->pk_timer;
		else if(!str_prefix(ARG_STR(1),"quest")) *ret = ARG_MOB(0)->countdown;
		else if(!str_prefix(ARG_STR(1),"ranged")) *ret = ARG_MOB(0)->ranged;
		else if(!str_prefix(ARG_STR(1),"recite")) *ret = ARG_MOB(0)->recite;
		else if(!str_prefix(ARG_STR(1),"resurrect")) *ret = ARG_MOB(0)->resurrect;
		else if(!str_prefix(ARG_STR(1),"reverie")) *ret = ARG_MOB(0)->reverie;
		else if(!str_prefix(ARG_STR(1),"scribe")) *ret = ARG_MOB(0)->scribe;
		else if(!str_prefix(ARG_STR(1),"script")) *ret = ARG_MOB(0)->script_wait;
		else if(!str_prefix(ARG_STR(1),"timer")) *ret = ARG_MOB(0)->timer;
		else if(!str_prefix(ARG_STR(1),"trance")) *ret = ARG_MOB(0)->trance;
		else if(!str_prefix(ARG_STR(1),"wait")) *ret = ARG_MOB(0)->wait;
		else if(!str_prefix(ARG_STR(1),"hiredto")) *ret = IS_NPC(ARG_MOB(0)) ? ARG_MOB(0)->hired_to : 0;
		else return false;
	} else if(ISARG_STR(0)) {
		if(!str_prefix(ARG_STR(0),"reckoning"))
			*ret = UMAX(0,reckoning_timer);
		else if(!str_prefix(ARG_STR(0),"reckoningcooldown"))
			*ret = UMAX(0,reckoning_cooldown_timer);
		else if(!str_prefix(ARG_STR(0),"prereckoning"))
			*ret = UMAX(0,pre_reckoning);
		else {
			for(i=0;boost_table[i].name;i++)
				if (!str_prefix(ARG_STR(0), boost_table[i].name))
					break;
			if(boost_table[i].name)
				*ret = UMAX(0,boost_table[i].timer);
			else
				return false;
		}
	} else if(ISARG_TOK(0)) {
		*ret = ARG_TOK(0)->timer;
	} else if(ISARG_DUNGEON(0)) {
		*ret = ARG_DUNGEON(0)->idle_timer;
	} else
		return false;

	return true;
}


DECL_IFC_FUN(ifc_isrestrung)
{
	*ret = ISARG_OBJ(0) && (ARG_OBJ(0)->old_short_descr || ARG_OBJ(0)->old_description);
	return true;
}

DECL_IFC_FUN(ifc_timeofday)
{
	int val;

	*ret = false;

	if(ISARG_STR(0)) {

		val = flag_value_ifcheck(time_of_day_flags,ARG_STR(0));

		if(IS_SET(val, TOD_AFTERMIDNIGHT)) {
			if(time_info.hour > 0 && time_info.hour < 5) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_AFTERNOON)) {
			if(time_info.hour > 12 && time_info.hour < 19) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_DAWN)) {
			if(time_info.hour == 5) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_DAY)) {
			if(time_info.hour > 5 && time_info.hour < 20) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_DUSK)) {
			if(time_info.hour == 19) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_EVENING)) {
			if(time_info.hour > 19 && time_info.hour < 24) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_MIDNIGHT)) {
			if(!time_info.hour) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_MORNING)) {
			if(time_info.hour > 5 && time_info.hour < 12) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_NIGHT)) {
			if(time_info.hour < 6 || time_info.hour > 19) { *ret = true; return true; }
		}

		if(IS_SET(val, TOD_NOON)) {
			if(time_info.hour == 12) { *ret = true; return true; }
		}
	}

	return true;
}

DECL_IFC_FUN(ifc_ismoonup)
{
	// Fix to take in location!
	*ret = true;
	return true;
}

DECL_IFC_FUN(ifc_moonphase)
{
	*ret = time_info.moon;
	return true;
}

DECL_IFC_FUN(ifc_reckoning)
{
	*ret = (reckoning_timer > 0) ? ((pre_reckoning > 0) ? 1 : 2) : 0;
	return true;
}

DECL_IFC_FUN(ifc_reckoningchance)
{
	*ret = reckoning_chance;
	return true;
}

DECL_IFC_FUN(ifc_reckoningintensity)
{
	*ret = reckoning_intensity;
	return true;
}

DECL_IFC_FUN(ifc_reckoningduration)
{
	*ret = reckoning_duration;
	return true;
}

DECL_IFC_FUN(ifc_reckoningcooldown)
{
	*ret = reckoning_cooldown;
	return true;
}

DECL_IFC_FUN(ifc_death)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && (ARG_MOB(0)->death_type == flag_value(death_types,ARG_STR(1))));
	return true;
}

// word STRING NUMBER STRING
DECL_IFC_FUN(ifc_word)
{
	int i,c;
	char *p, buf[MIL];

	*ret = false;
	if(ISARG_STR(0) && ISARG_NUM(1) && ISARG_STR(2)) {
		c = ARG_NUM(1);

		if(c > 0) {
			p = ARG_STR(0);
			for(i=0;i<c;i++) p = one_argument(p,buf);
			*ret = !str_cmp(buf,ARG_STR(2));
		} else
			*ret = !str_cmp(ARG_STR(0),ARG_STR(2));
	}

	return true;
}

DECL_IFC_FUN(ifc_inputwait)
{
	*ret = VALID_PLAYER(0) && ARG_MOB(0)->desc && (ARG_MOB(0)->desc->input ||
		ARG_MOB(0)->remove_question ||
		ARG_MOB(0)->pcdata->inquiry_subject ||
		ARG_MOB(0)->pk_question ||
		ARG_MOB(0)->personal_pk_question ||
		ARG_MOB(0)->cross_zone_question ||
		ARG_MOB(0)->pcdata->convert_church != -1 ||
		ARG_MOB(0)->challenged ||
		ARG_MOB(0)->remort_question);
	return true;
}

DECL_IFC_FUN(ifc_cos)
{
	*ret = 0;
	if(ISARG_NUM(0)) {
		double scale = (double)((ISARG_NUM(1) && ARG_NUM(1) > 1) ? ARG_NUM(1) : 1);
		*ret = (int)(10000 * cos(ARG_NUM(0) * 3.1415926 / 180.0 / scale) + 0.5);
	}
	return true;
}

DECL_IFC_FUN(ifc_sin)
{
	*ret = 0;
	if(ISARG_NUM(0)) {
		double scale = (double)((ISARG_NUM(1) && ARG_NUM(1) > 1) ? ARG_NUM(1) : 1);
		*ret = (int)(10000 * sin(ARG_NUM(0) * 3.1415926 / 180.0 / scale) + 0.5);
	}
	return true;
}

DECL_IFC_FUN(ifc_angle)
{
	*ret = -1;
	if(ISARG_NUM(0) && ISARG_NUM(1)) {
		int scale = (ISARG_NUM(2) && ARG_NUM(2) > 1) ? ARG_NUM(2) : 1;
		*ret = (int)(scale * atan2(ARG_NUM(1),ARG_NUM(0)) * 180.0 / 3.1415926);
		if(*ret < 0) *ret += 360 * scale;
	}
	return true;
}

DECL_IFC_FUN(ifc_sign)
{
	*ret = 0;
	if(ISARG_NUM(0))
		*ret = ((ARG_NUM(0) < 0)?-1:((ARG_NUM(0) > 0)?1:0));
	return true;
}

DECL_IFC_FUN(ifc_abs)
{
	*ret = 0;
	if(ISARG_NUM(0))
		*ret = abs(ARG_NUM(0));
	return true;
}

DECL_IFC_FUN(ifc_inwilds)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = (room->wilds != NULL);
	return true;
}


DECL_IFC_FUN(ifc_areaid)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->area->uid : -1;
	return true;
}


DECL_IFC_FUN(ifc_mapid)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = (room && room->wilds)?room->wilds->uid:-1;
	return true;
}


DECL_IFC_FUN(ifc_mapx)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = (room && room->wilds)?room->x:-1;
	return true;
}


DECL_IFC_FUN(ifc_mapy)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = (room && room->wilds)?room->y:-1;
	return true;
}


DECL_IFC_FUN(ifc_areahasland)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room && room->area->land_x >= 0 && room->area->land_y >= 0;
	return true;
}

DECL_IFC_FUN(ifc_areahasxy)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room && room->area->x >= 0 && room->area->y >= 0;
	return true;
}


DECL_IFC_FUN(ifc_arealandx)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->area->land_x : -1;
	return true;
}


DECL_IFC_FUN(ifc_arealandy)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->area->land_y : -1;
	return true;
}

DECL_IFC_FUN(ifc_areax)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->area->x : -1;
	return true;
}


DECL_IFC_FUN(ifc_areay)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	*ret = room ? room->area->y : -1;
	return true;
}


DECL_IFC_FUN(ifc_mapvalid)
{
	// Wilderness format
	if(ISARG_NUM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		WILDS_DATA *pWilds;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = false;
		else if (ARG_NUM(1) > (pWilds->map_size_x - 1) || ARG_NUM(2) > (pWilds->map_size_y - 1))
			*ret = false;
		else
			*ret = check_for_bad_room(pWilds, ARG_NUM(1), ARG_NUM(2));

	}
	return true;
}


DECL_IFC_FUN(ifc_mapwidth)
{
	// Wilderness format
	if(ISARG_NUM(0)) {
		WILDS_DATA *pWilds;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = -1;
		else
			*ret = pWilds->map_size_x;
	} else
		*ret = 0;
	return true;
}


DECL_IFC_FUN(ifc_mapheight)
{
	// Wilderness format
	if(ISARG_NUM(0)) {
		WILDS_DATA *pWilds;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = -1;
		else
			*ret = pWilds->map_size_y;
	} else
		*ret = 0;
	return true;
}


DECL_IFC_FUN(ifc_maparea)
{
	// Wilderness format
	if(ISARG_NUM(0) && ISARG_NUM(1) && ISARG_NUM(2)) {
		WILDS_DATA *pWilds;

		if(!(pWilds = get_wilds_from_uid(NULL,ARG_NUM(0))))
			*ret = -1;
		else
			*ret = pWilds->pArea->uid;
	}
	return true;
}


DECL_IFC_FUN(ifc_hasvlink)
{
	*ret = false;
	return true;
}


// hascatalyst $mobile string string number
DECL_IFC_FUN(ifc_hascatalyst)
{
	if(ISARG_MOB(0)) { mob = ARG_MOB(0); room = NULL; }
	else if(ISARG_ROOM(0)) { mob = NULL; room = ARG_ROOM(0); }
	else return false;
	*ret = ((mob || room) && ISARG_STR(1) && ISARG_STR(2) && ISARG_NUM(3)) ? has_catalyst(mob,room,flag_value_ifcheck(catalyst_types,ARG_STR(1)),flag_value_ifcheck(catalyst_method_types,ARG_STR(2)),ARG_NUM(3),ISARG_NUM(4)?ARG_NUM(4):CATALYST_MAXSTRENGTH) : 0;
	return true;
}

DECL_IFC_FUN(ifc_hitdamage)
{
	*ret = ISARG_MOB(0)?ARG_MOB(0)->hit_damage:0;
	return true;
}

DECL_IFC_FUN(ifc_hitdamtype)
{
	*ret = ISARG_MOB(0)?(ARG_MOB(0)->hit_type == (attack_lookup(ARG_STR(1))+TYPE_HIT)):false;
	return true;
}

DECL_IFC_FUN(ifc_hitskilltype)
{
	*ret = ISARG_MOB(0)?(ARG_MOB(0)->hit_type == skill_lookup(ARG_STR(1))):false;
	return true;
}

DECL_IFC_FUN(ifc_hitdamclass)
{
	*ret = ISARG_MOB(0)?(ARG_MOB(0)->hit_class == damage_class_lookup(ARG_STR(1))):false;
	return true;
}

DECL_IFC_FUN(ifc_systemtime)
{
	*ret = time(NULL) + (ISARG_NUM(0)?ARG_NUM(0):0);
	return true;
}

DECL_IFC_FUN(ifc_hassubclass)
{
	int sub;

	if(ISARG_MOB(0) && ISARG_STR(1) && !IS_NPC(ARG_MOB(0))) {
		sub = sub_class_search(ARG_STR(1));
		if(sub < 0)
			*ret = false;
		else {
			if(sub_class_table[sub].remort)
				*ret = ((get_profession(ARG_MOB(0),sub_class_table[sub].class + SECOND_CLASS_MAGE)) == sub) ? true : false;
			else
				*ret = ((get_profession(ARG_MOB(0),sub_class_table[sub].class + SUBCLASS_MAGE)) == sub) ? true : false;
		}

	} else
		*ret = false;

	return true;
}

DECL_IFC_FUN(ifc_issubclass)
{
	int sub;

	if(ISARG_MOB(0) && ISARG_STR(1) && !IS_NPC(ARG_MOB(0))) {
		sub = sub_class_search(ARG_STR(1));
		if(sub < 0)
			*ret = false;
		else
			*ret = ((get_profession(ARG_MOB(0),SUBCLASS_CURRENT)) == sub) ? true : false;

	} else
		*ret = false;

	return true;
}

DECL_IFC_FUN(ifc_objfrag)
{
	if(ISARG_OBJ(0) && ISARG_STR(1)) {
		if(!str_prefix(ARG_STR(1),"solid")) *ret = (ARG_OBJ(0)->fragility == OBJ_FRAGILE_SOLID) ? true : false;
		else if(!str_prefix(ARG_STR(1),"strong")) *ret = (ARG_OBJ(0)->fragility == OBJ_FRAGILE_STRONG) ? true : false;
		else if(!str_prefix(ARG_STR(1),"normal")) *ret = (ARG_OBJ(0)->fragility == OBJ_FRAGILE_NORMAL) ? true : false;
		else if(!str_prefix(ARG_STR(1),"weak")) *ret = (ARG_OBJ(0)->fragility == OBJ_FRAGILE_WEAK) ? true : false;
		else *ret = false;
	} else
		*ret = false;
	return true;
}

DECL_IFC_FUN(ifc_tempstore1)
{
	     if( ISARG_MOB(0)  )	*ret = ARG_MOB(0)->tempstore[0];
	else if( ISARG_OBJ(0)  )	*ret = ARG_OBJ(0)->tempstore[0];
	else if( ISARG_ROOM(0) )	*ret = ARG_ROOM(0)->tempstore[0];
	else if( ISARG_TOK(0)  )	*ret = ARG_TOK(0)->tempstore[0];
	else						*ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_tempstore2)
{
	     if( ISARG_MOB(0)  )	*ret = ARG_MOB(0)->tempstore[1];
	else if( ISARG_OBJ(0)  )	*ret = ARG_OBJ(0)->tempstore[1];
	else if( ISARG_ROOM(0) )	*ret = ARG_ROOM(0)->tempstore[1];
	else if( ISARG_TOK(0)  )	*ret = ARG_TOK(0)->tempstore[1];
	else						*ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_tempstore3)
{
	     if( ISARG_MOB(0)  )	*ret = ARG_MOB(0)->tempstore[2];
	else if( ISARG_OBJ(0)  )	*ret = ARG_OBJ(0)->tempstore[2];
	else if( ISARG_ROOM(0) )	*ret = ARG_ROOM(0)->tempstore[2];
	else if( ISARG_TOK(0)  )	*ret = ARG_TOK(0)->tempstore[2];
	else						*ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_tempstore4)
{
	     if( ISARG_MOB(0)  )	*ret = ARG_MOB(0)->tempstore[3];
	else if( ISARG_OBJ(0)  )	*ret = ARG_OBJ(0)->tempstore[3];
	else if( ISARG_ROOM(0) )	*ret = ARG_ROOM(0)->tempstore[3];
	else if( ISARG_TOK(0)  )	*ret = ARG_TOK(0)->tempstore[3];
	else						*ret = 0;

	return true;
}

DECL_IFC_FUN(ifc_tempstring)
{
	if(ISARG_MOB(0))
	{
		char *str = ARG_MOB(0)->tempstring;

		if(IS_NULLSTR(str) || !ISARG_STR(1)) return false;

		*ret = !str_cmp(str, ARG_STR(1)) ? true : false;
		return true;
	}
	return false;
}


DECL_IFC_FUN(ifc_strlen)
{
	*ret = ISARG_STR(0) ? strlen(ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_lostparts)
{
	*ret = (ISARG_MOB(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->lostparts, flag_value_ifcheck(part_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_hasprompt)
{
	*ret = (ISARG_MOB(0) && !IS_NPC(ARG_MOB(0)) && ISARG_STR(1) && string_vector_find(ARG_MOB(0)->pcdata->script_prompts,ARG_STR(1))) ? true : false ;
	return true;
}

DECL_IFC_FUN(ifc_numenchants)
{
	*ret = ISARG_OBJ(0) ? ARG_OBJ(0)->num_enchanted : -1;
	return true;
}

DECL_IFC_FUN(ifc_randpoint)
{
#define MAX_RAND_PT	5
	int x[MAX_RAND_PT];
	int y[MAX_RAND_PT];
	int r,n,i;

	if(!(argc&1)) return false;

	n = argc/2;

	if(n < 4 || n > MAX_RAND_PT) return false;

	for(r=0;r < argc; r++) if(!ISARG_NUM(r)) return false;

	for(r=0;r < n; r++) {
		x[r] = ARG_NUM(2*r);
		y[r] = ARG_NUM(2*r+1);
	}

	for(r=0;r < (n-1);r++)
		for(i=r+1;i < n;i++)
			if(x[r] >= x[i]) return false;

	r = number_range(x[0],x[n-1]);
	for(i = n-1; i-- > 0;) {
		if(r > x[i]) {
			*ret = (y[i+1] - y[i]) * (r - x[i]) / (x[i+1] - x[i]) + y[i];
			return true;
		}
	}

	*ret = y[0];
	return true;
}

DECL_IFC_FUN(ifc_issafe)
{
	if(ISARG_MOB(0)) {
		if(ISARG_MOB(1)) {
			*ret = is_safe(ARG_MOB(0),ARG_MOB(1),false) ? true : false;
			return true;
		}

		if(mob) {
			*ret = is_safe(mob,ARG_MOB(0),false) ? true : false;
			return true;
		}
	}
	return false;
}

DECL_IFC_FUN(ifc_roomx)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	if(!room) return false;

	*ret = room->x;
	return true;
}

DECL_IFC_FUN(ifc_roomy)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	if(!room) return false;

	*ret = room->y;
	return true;
}

DECL_IFC_FUN(ifc_roomz)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	if(!room) return false;

	*ret = room->z;
	return true;
}

DECL_IFC_FUN(ifc_roomwilds)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	if(!room) return false;

	*ret = room->wilds ? room->wilds->uid : 0;
	return true;
}

DECL_IFC_FUN(ifc_roomviewwilds)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else return false;

	if(!room) return false;

	*ret = room->viewwilds ? room->viewwilds->uid : 0;
	return true;
}

// isleader			if $i is its leader
// isleader <mobile>		if <mobile> is $i's leader
// isleader <mobile> <leader>	if <leader> is <mobile>'s leader
DECL_IFC_FUN(ifc_isleader)
{
	if(ISARG_MOB(0)) {
		if(ISARG_MOB(1))
			*ret = (ARG_MOB(0)->leader && (ARG_MOB(1) == ARG_MOB(0)->leader)) ? true : false;
		else
			*ret = (mob->leader && (ARG_MOB(0) == mob->leader)) ? true : false;
	} else if(mob) {
		*ret = (mob == mob->leader) ? true : false;
	} else
		return false;


	return true;
}

DECL_IFC_FUN(ifc_samegroup)
{
	if(ISARG_MOB(0)) {
		if(ISARG_MOB(1))
			*ret = is_same_group(ARG_MOB(0),ARG_MOB(1)) ? true : false;
		else
			*ret = is_same_group(ARG_MOB(0),mob) ? true : false;
	} else
		return false;

	return true;
}

DECL_IFC_FUN(ifc_testhardmagic)
{
	int chance;

	if(ISARG_MOB(0)) { mob = ARG_MOB(0); ++argv; --argc; }

	if(!mob || !mob->in_room) return false;

	chance = 0;
	if (IS_SET(mob->in_room->room_flag[1], ROOM_HARD_MAGIC)) chance += 2;
	if (mob->in_room->sector_type == SECT_CURSED_SANCTUM) chance += 2;
	if(!IS_NPC(mob) && chance > 0 && number_range(1,chance) > 1) {
		*ret = true;
	} else
		*ret = false;

	return true;
}

DECL_IFC_FUN(ifc_testslowmagic)
{
	if(ISARG_MOB(0)) { mob = ARG_MOB(0); ++argv; --argc; }

	if(!mob || !mob->in_room) return false;

	*ret = IS_SET(mob->in_room->room_flag[1],ROOM_SLOW_MAGIC) || (mob->in_room->sector_type == SECT_CURSED_SANCTUM);

	return true;
}

// This mimics the built in code sans the messages and skill improves.
//   That will be up to the script to determine that
DECL_IFC_FUN(ifc_testtokenspell)
{
	SHIFT_MOB();

	if(ISARG_TOK(0)) {
		if(!ARG_TOK(0)->pIndexData->value[TOKVAL_SPELL_RATING])
			*ret = (number_percent() < ARG_TOK(0)->value[TOKVAL_SPELL_RATING]);
		else
			*ret = (number_range(0,(ARG_TOK(0)->pIndexData->value[TOKVAL_SPELL_RATING]*100)-1) < ARG_TOK(0)->value[TOKVAL_SPELL_RATING]);
		return true;
	}

	return false;
}


DECL_IFC_FUN(ifc_isspell)
{
	if(ISARG_MOB(0) && ISARG_NUM(1)) {
		token = get_token_char(ARG_MOB(0), ARG_NUM(1), (ISARG_NUM(2) ? ARG_NUM(2) : 1));
		*ret = token ? (token->pIndexData->type == TOKEN_SPELL) : false;
	} else if(ISARG_TOK(0)) {
		*ret = (ARG_TOK(0)->pIndexData->type == TOKEN_SPELL);
	} else {
		int sn;

		if(ISARG_STR(0))
			sn = skill_lookup(ARG_STR(0));
		else if(ISARG_SKILL(0))
			sn = ARG_SKILL(0);
		else if(ISARG_SKINFO(0))
			sn = ARG_SKINFO(0).m ? ARG_SKINFO(0).sn : -1;
		else
			return false;

		*ret = sn >= 0 && sn < MAX_SKILL && skill_table[sn].spell_fun && skill_table[sn].spell_fun != spell_null;
	}

	return true;
}

DECL_IFC_FUN(ifc_hasenvironment)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else if(mob) room = mob->in_room;
	else if(obj) room = obj_room(obj);
	// if room is set because this is from an rprog, duh!
	else if(token) room = token_room(token);

	if(!room) return false;

	*ret = IS_SET(room->room_flag[1],ROOM_VIRTUAL_ROOM) && (room->environ_type != ENVIRON_NONE);
	return true;
}

DECL_IFC_FUN(ifc_iscloneroom)
{
	if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
	else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
	else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
	else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	else if(mob) room = mob->in_room;
	else if(obj) room = obj_room(obj);
	// if room is set because this is from an rprog, duh!
	else if(token) room = token_room(token);

	if(!room) return false;

	*ret = room_is_clone(room);
	return true;
}

DECL_IFC_FUN(ifc_scriptsecurity)
{
	*ret = script_security;
	return true;
}

DECL_IFC_FUN(ifc_bankbalance)
{
	*ret = VALID_PLAYER(0) ? ARG_MOB(0)->pcdata->bankbalance : 0;
	return true;
}

DECL_IFC_FUN(ifc_grouphit)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;	// Since all stats have a minimum, this would be an ERROR

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->hit;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_groupmana)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->mana;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_groupmove)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->move;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_groupmaxhit)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;	// Since all stats have a minimum, this would be an ERROR

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->max_hit;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_groupmaxmana)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->max_mana;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_groupmaxmove)
{
	CHAR_DATA *rch;
	int sum = 0;

	SHIFT_MOB();
	*ret = 0;

	if(!mob || !mob->in_room) return false;

	for(rch = mob->in_room->people;rch; rch = rch->next_in_room)
		if(mob == rch || is_same_group(mob,rch))
			sum += rch->max_move;

	*ret = sum;
	return true;
}

DECL_IFC_FUN(ifc_manastore)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->manastore : 0;
	return true;
}

DECL_IFC_FUN(ifc_arearegion)
{
	*ret = 0;
	return true;
}

DECL_IFC_FUN(ifc_areaflag)
{
	*ret = false;
	return true;
}


DECL_IFC_FUN(ifc_areawho)
{
	*ret = ISARG_AREA(0) && ISARG_STR(1) && (ARG_AREA(0)->area_who == flag_value(area_who_titles,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_areaplace)
{
	*ret = ISARG_AREA(0) && ISARG_STR(1) && (ARG_AREA(0)->area_who == flag_value(area_who_titles,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_isaffectcustom)
{
	*ret = ARG_AFF(0) && ARG_AFF(0)->custom_name != NULL;
	return true;
}

DECL_IFC_FUN(ifc_affectskill)
{
	*ret = ISARG_AFF(0) ? ARG_AFF(0)->type : 0;
	return true;
}

DECL_IFC_FUN(ifc_isaffectskill)
{
	*ret = ISARG_AFF(0) && ISARG_STR(1) && (ARG_AFF(0)->custom_name == NULL) && (ARG_AFF(0)->type == skill_lookup(ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_affectlocation)
{
	*ret = ISARG_AFF(0) ? ARG_AFF(0)->location : 0;
	return true;
}

DECL_IFC_FUN(ifc_affectmodifier)
{
	*ret = ISARG_AFF(0) ? ARG_AFF(0)->modifier : 0;
	return true;
}

DECL_IFC_FUN(ifc_affecttimer)
{
	*ret = ISARG_AFF(0) ? ARG_AFF(0)->duration : 0;
	return true;
}

DECL_IFC_FUN(ifc_affectgroup)
{
	*ret = ISARG_AFF(0) ? ARG_AFF(0)->group : 0;
	return true;
}

DECL_IFC_FUN(ifc_isaffectgroup)
{
	*ret = false;
	if(ISARG_AFF(0) && ISARG_STR(1)) {
		if(ARG_AFF(0)->where == TO_OBJECT || ARG_AFF(0)->where == TO_WEAPON)
			*ret = ARG_AFF(0)->group == flag_lookup(ARG_STR(1), affgroup_object_flags);
		else
			*ret = ARG_AFF(0)->group == flag_lookup(ARG_STR(1), affgroup_mobile_flags);
	}
	return true;
}

DECL_IFC_FUN(ifc_affectbit)
{
	*ret = ISARG_AFF(0) && ISARG_STR(1) && IS_SET(ARG_AFF(0)->bitvector, flag_value_ifcheck(affect_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_affectbit2)
{
	*ret = ISARG_AFF(0) && ISARG_STR(1) && IS_SET(ARG_AFF(0)->bitvector, flag_value_ifcheck(affect2_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_isaffectwhere)
{
	*ret = ISARG_AFF(0) && ISARG_STR(1) && (ARG_AFF(0)->where == flag_value(apply_types, ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_skilllookup)
{
	*ret = ISARG_STR(0) ? skill_lookup(ARG_STR(0)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_xp)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->exp : 0;
	return true;
}

DECL_IFC_FUN(ifc_maxxp)
{
	*ret = ISARG_MOB(0) ? (IS_NPC(ARG_MOB(0)) ? ARG_MOB(0)->maxexp : exp_per_level(ARG_MOB(0),ARG_MOB(0)->pcdata->points)) : 0;
	return true;
}

DECL_IFC_FUN(ifc_sublevel)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->level : 0;
	return true;
}

DECL_IFC_FUN(ifc_mobsize)
{
	*ret = ISARG_MOB(0) ? ARG_MOB(0)->size : 0;
	return true;
}

DECL_IFC_FUN(ifc_handsfull)
{
	*ret = ISARG_MOB(0) ? both_hands_full(ARG_MOB(0)) : false;
	return true;
}

DECL_IFC_FUN(ifc_isremort)
{
	*ret = VALID_PLAYER(0) && IS_REMORT(ARG_MOB(0));
	return true;
}


DECL_IFC_FUN(ifc_findpath)
{
	long start = 0, end = 0;
	int depth = 10, in_zone = 1, thru_doors = 0;

	if( argc < 2 ) {
		*ret = -1;
		return false;
	}

	if(ISARG_NUM(0)) start = ARG_NUM(0);
	else if(ISARG_MOB(0)) start = ARG_MOB(0)->in_room ? ARG_MOB(0)->in_room->vnum : 0;
	else if(ISARG_OBJ(0)) { room = obj_room(ARG_OBJ(0)); start = room ? room->vnum : 0; }
	else if(ISARG_ROOM(0)) start = ARG_ROOM(0)->vnum;
	else if(ISARG_TOK(0)) { room = token_room(ARG_TOK(0)); start = room ? room->vnum : 0; }

	if(ISARG_NUM(1)) end = ARG_NUM(1);
	else if(ISARG_MOB(1)) end = ARG_MOB(1)->in_room ? ARG_MOB(1)->in_room->vnum : 1;
	else if(ISARG_OBJ(1)) { room = obj_room(ARG_OBJ(1)); end = room ? room->vnum : 1; }
	else if(ISARG_ROOM(1)) end = ARG_ROOM(1)->vnum;
	else if(ISARG_TOK(1)) { room = token_room(ARG_TOK(1)); end = room ? room->vnum : 1; }

	if(argc > 2 && ISARG_NUM(2)) depth = ARG_NUM(2);

	if(argc > 3 && ISARG_NUM(3)) in_zone = ARG_NUM(3) && 1;

	if(argc > 4 && ISARG_NUM(4)) thru_doors = ARG_NUM(4) && 1;

	*ret = find_path( start, end, NULL, (thru_doors ? -depth : depth), in_zone);
	return true;
}

// Determines the level of sunlight from 0 to 1000
// Sunlight is 0 inside or in the netherworld
DECL_IFC_FUN(ifc_sunlight)
{
	if(argc > 0) {
		if(ISARG_MOB(0)) room = ARG_MOB(0)->in_room;
		else if(ISARG_OBJ(0)) room = obj_room(ARG_OBJ(0));
		else if(ISARG_ROOM(0)) room = ARG_ROOM(0);
		else if(ISARG_TOK(0)) room = token_room(ARG_TOK(0));
	} else {
		if(mob) room = mob->in_room;
		else if(obj) room = obj_room(obj);
		else if(token) room = token_room(token);
	}

	if (room && (room->wilds || (room->sector_type != SECT_INSIDE && room->sector_type != SECT_NETHERWORLD && !IS_SET(room->room_flag[0], ROOM_INDOORS)))) {
		*ret = (int)(-1000 * cos(3.1415926 * time_info.hour / 12));
		if(*ret < 0) *ret = 0;
	} else
		*ret = 0;
	return true;
}

// if istreasureroom[ $<mobile|church|name>] $<room|vnum>
DECL_IFC_FUN(ifc_istreasureroom)
{
	CHURCH_DATA *church = NULL;
	ROOM_INDEX_DATA *here = NULL;
	ITERATOR it;

	*ret = false;
	if(ISARG_MOB(0)) church = ARG_MOB(0)->church;
	else if(ISARG_CHURCH(0)) church = ARG_CHURCH(0);
	else if(ISARG_STR(0)) church = find_church_name(ARG_STR(0));
	else {
		if(ISARG_ROOM(0)) here = ARG_ROOM(0);
		else if(ISARG_NUM(0)) here = get_room_index(ARG_NUM(0));

		if(here) {
			iterator_start(&it, list_churches);
			while( (church = (CHURCH_DATA *)iterator_nextdata(&it)) ) {
				if( is_treasure_room(church, here) ) {
					*ret = true;
					break;
				}
			}
			iterator_stop(&it);
		}

		return true;
	}

	if(church) {
		if(ISARG_ROOM(1)) here = ARG_ROOM(1);
		else if(ISARG_NUM(1)) here = get_room_index(ARG_NUM(1));

		*ret = here ? is_treasure_room(church, here) : false;
	}

	return true;
}

// if churchhasrelic $<mobile|church|name> $<string=relic>
DECL_IFC_FUN(ifc_churchhasrelic)
{
	CHURCH_DATA *church = NULL;

	*ret = false;
	if(ISARG_MOB(0)) church = ARG_MOB(0)->church;
	else if(ISARG_CHURCH(0)) church = ARG_CHURCH(0);
	else if(ISARG_STR(0)) church = find_church_name(ARG_STR(0));

	if(church) {
		OBJ_DATA *relic = NULL;
		if(ISARG_STR(1)) {
			if(!str_cmp(ARG_STR(1),"xp")) relic = xp_relic;
			else if(!str_cmp(ARG_STR(1),"damage")) relic = damage_relic;
			else if(!str_cmp(ARG_STR(1),"pneuma")) relic = pneuma_relic;
			else if(!str_cmp(ARG_STR(1),"health")) relic = hp_regen_relic;
			else if(!str_cmp(ARG_STR(1),"mana")) relic = mana_regen_relic;
		}

		if( relic && relic->in_room ) {
			ITERATOR it;
			ROOM_INDEX_DATA *troom;

			iterator_start(&it,church->treasure_rooms);
			while( (troom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) ) {
				if( relic->in_room == troom ) {
					*ret = true;
					break;
				}
			}
			iterator_stop(&it);
		}
	}

	return true;
}

// if ischurchpk $<mobile|church|name>
DECL_IFC_FUN(ifc_ischurchpk)
{
	CHURCH_DATA *church = NULL;

	if(ISARG_MOB(0)) church = ARG_MOB(0)->church;
	else if(ISARG_CHURCH(0)) church = ARG_CHURCH(0);
	else if(ISARG_STR(0)) church = find_church_name(ARG_STR(0));

	*ret = church && church->pk;

	return true;
}

// if iscrosszone $<mobile|church|name>
DECL_IFC_FUN(ifc_iscrosszone)
{
	CHURCH_DATA *church = NULL;

	if(ISARG_MOB(0)) church = ARG_MOB(0)->church;
	else if(ISARG_CHURCH(0)) church = ARG_CHURCH(0);
	else if(ISARG_STR(0)) church = find_church_name(ARG_STR(0));

	if(church) *ret = (int)IS_SET(church->settings,CHURCH_ALLOW_CROSSZONES);
	else *ret = false;
	return true;
}

// if inchurch $MOBILE [ $CHURCH|name ]
DECL_IFC_FUN(ifc_inchurch)
{
	*ret = false;

	if (ISARG_MOB(0))
	{
		CHAR_DATA *m = ARG_MOB(0);

		if (ISARG_CHURCH(1))
			*ret = m && m->church == ARG_CHURCH(1);
		else if (ISARG_STR(1))
		{
			CHURCH_DATA *church = find_church_name(ARG_STR(1));
			if (church)
				*ret = m && m->church == church;
		}
		else if (m->church != NULL)
			*ret = true;
	}

	return true;
}

// if ispersist $<mobile|object|room>
DECL_IFC_FUN(ifc_ispersist)
{
	if(ISARG_MOB(0)) *ret = IS_NPC(ARG_MOB(0)) ? ARG_MOB(0)->persist : true;	// Players are persistant by nature
	else if(ISARG_OBJ(0)) *ret = ARG_OBJ(0)->persist;
	else if(ISARG_ROOM(0)) *ret = ARG_ROOM(0)->persist;
	else *ret = false;

	return true;
}

// if listcontains $<list> $<element>
DECL_IFC_FUN(ifc_listcontains)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;
	LLIST_ROOM_DATA *lroom;
	ROOM_INDEX_DATA *proom;
	CHAR_DATA *pmob;
	OBJ_DATA *pobj;
	TOKEN_DATA *ptoken;

	*ret = false;
	if(ISARG_BLLIST_ROOM(0) && ISARG_ROOM(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) ) {
			if( lroom->room == ARG_ROOM(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_PLLIST_ROOM(0) && ISARG_ROOM(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (proom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) ) {
			if( proom == ARG_ROOM(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_BLLIST_MOB(0) && ISARG_MOB(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
			if( (CHAR_DATA*)(luid->ptr) == ARG_MOB(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_PLLIST_MOB(0) && ISARG_MOB(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (pmob = (CHAR_DATA *)iterator_nextdata(&it)) ) {
			if( pmob == ARG_MOB(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_BLLIST_OBJ(0) && ISARG_OBJ(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
			if( (OBJ_DATA*)(luid->ptr) == ARG_OBJ(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_PLLIST_OBJ(0) && ISARG_OBJ(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (pobj = (OBJ_DATA *)iterator_nextdata(&it)) ) {
			if( pobj == ARG_OBJ(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_BLLIST_TOK(0) && ISARG_TOK(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
			if( (TOKEN_DATA*)(luid->ptr) == ARG_TOK(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	} else if(ISARG_PLLIST_TOK(0) && ISARG_TOK(1)) {
		iterator_start(&it,ARG_BLIST(0));
		while( (ptoken = (TOKEN_DATA *)iterator_nextdata(&it)) ) {
			if( ptoken == ARG_TOK(1) ) {
				*ret = true;
				break;
			}
		}
		iterator_stop(&it);
	}

	return true;
}

// if ispk $<player>
// if ispk $<room>[ <boolean=arena>]
// if ispk $<ship>
DECL_IFC_FUN(ifc_ispk)
{
	if(VALID_PLAYER(0)) *ret = is_pk(ARG_MOB(0));
	else if(ISARG_ROOM(0)) *ret = is_room_pk(ARG_ROOM(0),ARG_BOOL(1));
	else if(ISARG_SHIP(0)) *ret = ARG_SHIP(0)->pk;
	else *ret = false;

	return true;
}

// if register <number>
DECL_IFC_FUN(ifc_register)
{

	if(ISARG_NUM(0)) {
		int r = ARG_NUM(0);

		if(r >= 0 && r < 5)
		{
			*ret = info->registers[r];
			return true;
		}
	}

	*ret = 0;
	return false;
}

// if comm $<player> <flags>
DECL_IFC_FUN(ifc_comm)
{
	*ret = VALID_PLAYER(0) && ISARG_STR(1) && IS_SET(ARG_MOB(0)->comm, flag_value_ifcheck(comm_flags, ARG_STR(1)));
	return true;
}

// if flagcomm <flags> == <value>
// Usually used for $[[flagcomm <flags>]]
DECL_IFC_FUN(ifc_flag_comm)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(comm_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

// if min <number> <number>
// returns the minimum of the two numbers
DECL_IFC_FUN(ifc_min)
{
	if(ISARG_NUM(0))
	{
		if( ISARG_NUM(1) && (ARG_NUM(1) < ARG_NUM(0)) )
			*ret = ARG_NUM(1);
		else
			*ret = ARG_NUM(0);
		return true;
	}

	return false;
}

// if max <number> <number>
// returns the maximum of the two numbers
DECL_IFC_FUN(ifc_max)
{
	if(ISARG_NUM(0))
	{
		if( ISARG_NUM(1) && (ARG_NUM(1) > ARG_NUM(0)) )
			*ret = ARG_NUM(1);
		else
			*ret = ARG_NUM(0);
		return true;
	}

	return false;
}

// if candrop $mobile $object[ boolean[ boolean]]
// boolean 1: check keep
DECL_IFC_FUN(ifc_candrop)
{
	OBJ_DATA *o;
	bool keep = false;
	bool silent = false;
	if(!ISARG_MOB(0)) return false;
	if(!ISARG_OBJ(1)) return false;

	if(ISARG_NUM(2) || ISARG_STR(2))
		keep = ARG_BOOL(2);

	if(ISARG_NUM(3) || ISARG_STR(3))
		silent = ARG_BOOL(3);

	o = ARG_OBJ(1);

	*ret = can_drop_obj(ARG_MOB(0), o, silent) && !(keep && IS_SET(o->extra[1], ITEM_KEPT));
	return true;
}

// if canget $mobile $object[ $container] [boolean]
DECL_IFC_FUN(ifc_canget)
{
	CHAR_DATA *ch;
	OBJ_DATA *o, *c = NULL;
	bool silent;

	if(!ISARG_MOB(0)) return false;
	if(!ISARG_OBJ(1)) return false;

	ch = ARG_MOB(0);
	o = ARG_OBJ(1);

	if(ISARG_OBJ(2)) {
		c = ARG_OBJ(2);
		++argv;
		--argc;
	}

	silent = ARG_BOOL(2);

	*ret = can_get_obj(ch, o, c, NULL, silent);
	return true;
}

// if canput $mobile $object $container [boolean]
DECL_IFC_FUN(ifc_canput)
{
	if(!ISARG_MOB(0)) return false;
	if(!ISARG_OBJ(1)) return false;
	if(!ISARG_OBJ(2)) return false;

	*ret = can_put_obj(ARG_MOB(0), ARG_OBJ(1), ARG_OBJ(2), NULL, ARG_BOOL(3));
	return true;
}

// if objcorpse $OBJECT STRING
// where OBJECT is a corpse
DECL_IFC_FUN(ifc_objcorpse)
{
	*ret = ISARG_OBJ(0) && ISARG_STR(1) &&
		(ARG_OBJ(0)->item_type == ITEM_CORPSE_NPC || ARG_OBJ(0)->item_type == ITEM_CORPSE_PC) &&
		IS_SET(CORPSE_FLAGS(ARG_OBJ(0)), flag_value_ifcheck(corpse_object_flags,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_flag_corpse)
{
	*ret = ISARG_STR(0) ? flag_value_ifcheck(corpse_object_flags,ARG_STR(0)) : 0;
	if(*ret == NO_FLAG) *ret = 0;
	return true;
}

// if objweapon $WEAPON TYPE (eg 'sword')
// if objweapon $WEAPONCONTAINER TYPE (eg 'sword')
DECL_IFC_FUN(ifc_objweapon)
{
	*ret = false;
	if(ISARG_OBJ(0) && ISARG_STR(1)) {
		if(ARG_OBJ(0)->item_type == ITEM_WEAPON)
			*ret = ARG_OBJ(0)->value[0] == flag_value(weapon_class,ARG_STR(1));
		else if(ARG_OBJ(0)->item_type == ITEM_WEAPON_CONTAINER)
			*ret = ARG_OBJ(0)->value[1] == flag_value(weapon_class,ARG_STR(1));
	}

	return true;
}

DECL_IFC_FUN(ifc_objweaponstat)
{
	*ret = ISARG_OBJ(0) && ISARG_STR(1) &&
		(ARG_OBJ(0)->item_type == ITEM_WEAPON) &&
		IS_SET(ARG_OBJ(0)->value[4],flag_value_ifcheck(weapon_type2,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_objranged)
{
	*ret = ISARG_OBJ(0) && ISARG_STR(1) &&
		(ARG_OBJ(0)->item_type == ITEM_RANGED_WEAPON) &&
		(ARG_OBJ(0)->value[0] == flag_value(ranged_weapon_class,ARG_STR(1)));
	return true;
}

DECL_IFC_FUN(ifc_isbusy)
{
	*ret = ISARG_MOB(0) && is_char_busy(ARG_MOB(0));
	return true;
}

DECL_IFC_FUN(ifc_iscastsuccess)
{
	*ret = ISARG_MOB(0) && (ARG_MOB(0)->cast_successful == MAGICCAST_SUCCESS);
	return true;
}

DECL_IFC_FUN(ifc_iscastfailure)
{
	*ret = ISARG_MOB(0) && (ARG_MOB(0)->cast_successful == MAGICCAST_FAILURE);
	return true;
}

DECL_IFC_FUN(ifc_iscastroomblocked)
{
	*ret = ISARG_MOB(0) && (ARG_MOB(0)->cast_successful == MAGICCAST_ROOMBLOCK);
	return true;
}

DECL_IFC_FUN(ifc_iscastrecovered)
{
	*ret = ISARG_MOB(0) && ARG_MOB(0)->casting_recovered;
	return true;
}

// hasspell[ OBJECT] STRING
DECL_IFC_FUN(ifc_hasspell)
{
	*ret = false;
	if(ISARG_STR(0)) {
		if(obj)
			*ret = obj_has_spell(obj, ARG_STR(0));
	} else if(ISARG_OBJ(0) && ISARG_STR(1)) {
		*ret = obj_has_spell(ARG_OBJ(0), ARG_STR(1));
	}

	return true;
}

// playerexists STRING
// - checks if the string is a player name
DECL_IFC_FUN(ifc_playerexists)
{
	*ret = ISARG_STR(0) && player_exists(ARG_STR(0));
	return true;
}

// hascheckpoint $PLAYER
// - checks whether the player's checkpoint has been set.
DECL_IFC_FUN(ifc_hascheckpoint)
{
	*ret = VALID_PLAYER(0) ? (ARG_MOB(0)->checkpoint != NULL) : false;
	return true;
}

// ismobile $ENTITY
DECL_IFC_FUN(ifc_ismobile)
{
	*ret = ISARG_MOB(0) && true;
	return true;
}

// isobject $ENTITY
DECL_IFC_FUN(ifc_isobject)
{
	*ret = ISARG_OBJ(0) && true;
	return true;
}

// isroom $ENTITY
DECL_IFC_FUN(ifc_isroom)
{
	*ret = ISARG_ROOM(0) && true;
	return true;
}

// istoken $ENTITY
DECL_IFC_FUN(ifc_istoken)
{
	*ret = ISARG_TOK(0) && true;
	return true;
}

// mobclones[ ROOM] VNUM|MOBILE|MOBIDX == NUMBER
DECL_IFC_FUN(ifc_mobclones)
{
	ROOM_INDEX_DATA *location = NULL;
	MOB_INDEX_DATA *index = NULL;
	CHAR_DATA *m;
	int count;

	if(ISARG_ROOM(0)) {
		location = ARG_ROOM(0);

		if(ISARG_NUM(1)) index = get_mob_index(ARG_NUM(1));
		else if(VALID_NPC(1)) index = ARG_MOB(1)->pIndexData;
		//if(ISARG_MOBIDX(1)) index = ARG_MOBIDX(1);
		else
			return false;


	} else if(ISARG_NUM(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = get_mob_index(ARG_NUM(0));
	} else if(VALID_NPC(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = ARG_MOB(0)->pIndexData;
/*	} else if(ISARG_MOBIDX(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = ISARG_MOBIDX(0);
*/
	} else
		return false;

	if( location == NULL || index == NULL )
		return false;


	count = 0;
	for(m = location->people; m; m = m->next_in_room)
	{
		if(IS_NPC(m) && m->pIndexData == index)
			++count;
	}

	*ret = count;
	return true;
}

// objclones[ ROOM] VNUM|OBJECT|OBJIDX == NUMBER
DECL_IFC_FUN(ifc_objclones)
{
	ROOM_INDEX_DATA *location = NULL;
	OBJ_INDEX_DATA *index = NULL;
	OBJ_DATA *o;
	int count;

	if(ISARG_ROOM(0)) {
		location = ARG_ROOM(0);

		if(ISARG_NUM(1)) index = get_obj_index(ARG_NUM(1));
		else if(ISARG_OBJ(1)) index = ARG_OBJ(1)->pIndexData;
		//if(ISARG_OBJIDX(1)) index = ARG_OBJIDX(1);
		else
			return false;


	} else if(ISARG_NUM(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = get_obj_index(ARG_NUM(0));
	} else if(ISARG_OBJ(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = ARG_OBJ(0)->pIndexData;
/*	} else if(ISARG_OBJIDX(0)) {
		if(mob) location = mob->in_room;
		else if(obj) location = obj_room(obj);
		else if(room) location = room;
		else if(token) location = token_room(token);
		else
			return false;

		index = ISARG_OBJIDX(0);
*/
	} else
		return false;

	if( location == NULL || index == NULL )
		return false;


	count = 0;
	for(o = location->contents; o; o = o->next_content)
	{
		if(o->pIndexData == index)
			++count;
	}

	*ret = count;
	return true;
}

// hitdicebonus MOBILE == NUMBER
DECL_IFC_FUN(ifc_hitdicebonus)
{
	*ret = VALID_NPC(0) ? ARG_MOB(0)->damage.bonus : 0;
	return true;
}

// hitdicenumber MOBILE == NUMBER
DECL_IFC_FUN(ifc_hitdicenumber)
{
	*ret = VALID_NPC(0) ? ARG_MOB(0)->damage.number : 0;
	return true;
}

// hitdicetype MOBILE == NUMBER
DECL_IFC_FUN(ifc_hitdicetype)
{
	*ret = VALID_NPC(0) ? ARG_MOB(0)->damage.size : 0;
	return true;
}

// strprefix STRING1 STRING2
// if STRING2 prefixes STRING1
DECL_IFC_FUN(ifc_strprefix)
{
	*ret = ISARG_STR(0) && ISARG_STR(1) && !str_prefix(ARG_STR(1), ARG_STR(0));
	return true;
}

// roll DICE == NUMBER
DECL_IFC_FUN(ifc_roll)
{
	if(!ISARG_DICE(0)) return false;

	*ret = dice_roll(ARG_DICE(0));
	return true;
}

// boost TYPE == NUMBER
DECL_IFC_FUN(ifc_boost)
{
	if(!ISARG_STR(0)) return false;

	if(!str_cmp(ARG_STR(0),"xp")) *ret = boost_table[BOOST_EXPERIENCE].boost;
	else if(!str_cmp(ARG_STR(0),"damage")) *ret = boost_table[BOOST_DAMAGE].boost;
	else if(!str_cmp(ARG_STR(0),"qp")) *ret = boost_table[BOOST_QP].boost;
	else if(!str_cmp(ARG_STR(0),"pneuma")) *ret = boost_table[BOOST_PNEUMA].boost;
	else if(!str_cmp(ARG_STR(0),"reckoning")) *ret = boost_table[BOOST_RECKONING].boost;
	else
		return false;

	return true;
}

// boosttimer TYPE == NUMBER
DECL_IFC_FUN(ifc_boosttimer)
{
	if(!ISARG_STR(0)) return false;

	time_t timer;

	if(!str_cmp(ARG_STR(0),"xp")) timer = boost_table[BOOST_EXPERIENCE].boost;
	else if(!str_cmp(ARG_STR(0),"damage")) timer = boost_table[BOOST_DAMAGE].boost;
	else if(!str_cmp(ARG_STR(0),"qp")) timer = boost_table[BOOST_QP].boost;
	else if(!str_cmp(ARG_STR(0),"pneuma")) timer = boost_table[BOOST_PNEUMA].boost;
	else if(!str_cmp(ARG_STR(0),"reckoning")) timer = boost_table[BOOST_RECKONING].boost;
	else
		return false;

	*ret = (timer - current_time) / 60;
	if( *ret < 0 ) *ret = 0;

	return true;
}

// HIRED $MOBILE -> true/false
// Shortcut for "IF ACT2 $MOBILE hired"
DECL_IFC_FUN(ifc_ishired)
{
	if( VALID_NPC(0) )
	{
		*ret = IS_SET(ARG_MOB(0)->act[1], ACT2_HIRED) && true;
		return true;
	}

	*ret = false;
	return false;
}

// HIRED $MOBILE == NUMBER
// Gets the number of minutes the mob's hired_to has left
//  Value is rounded up to the nearest minute to leave 0 to mean expired/nonexistent
//  Is similar to "IF TIMER $MOBILE hiredto == $[[systemtime]]"
//  "IF TIMER $MOBILE hiredto" gives the raw timestamp (in seconds)
//  Is the timer version if you want the full resolution to this timer
DECL_IFC_FUN(ifc_hired)
{
	if(VALID_NPC(0))
	{
		if( ARG_MOB(0)->hired_to > current_time )
			*ret = (int)(ARG_MOB(0)->hired_to - current_time + 59) / 60;
		else
			*ret = 0;

		return true;
	}

	*ret = 0;
	return false;
}

// TODO: expand to check whether a mob can pull the object
// CANPULL $OBJECT - check whether the object is something that can be pulled around
//
DECL_IFC_FUN(ifc_canpull)
{
	*ret = false;
	if( ISARG_OBJ(0) )
	{
		*ret = is_pullable(ARG_OBJ(0));
	}

	return true;
}

// LOADED $MOBILE[ $INSTANCE|$DUNGEON] == NUMBER
// LOADED $OBJECT == NUMBER
// LOADED "MOBILE" NUMBER[ $INSTANCE|$DUNGEON] == NUMBER
// LOADED "OBJECT" NUMBER == NUMBER
DECL_IFC_FUN(ifc_loaded)
{
	*ret = 0;
	if( ISARG_STR(0) )
	{
		if( !IS_NULLSTR(ARG_STR(0)) )
		{
			if(!str_prefix(ARG_STR(0), "mobile"))
			{
				if( ISARG_NUM(1) )
				{
					MOB_INDEX_DATA *mobindex = get_mob_index(ARG_NUM(1));

					if( mobindex )
					{
						if( ISARG_INSTANCE(2) )
							*ret = instance_count_mob(ARG_INSTANCE(2), mobindex);
						else if( ISARG_DUNGEON(2) )
							*ret = dungeon_count_mob(ARG_DUNGEON(2), mobindex);
						else
							*ret = mobindex->count;

					}
					else
						*ret = 0;
				}
			}
			else if(!str_prefix(ARG_STR(0), "object"))
			{
				if( ISARG_NUM(1) )
				{
					OBJ_INDEX_DATA *objindex = get_obj_index(ARG_NUM(1));

					*ret = objindex ? objindex->count : 0;
				}
			}
		}
	}
	else if(VALID_NPC(0))
	{
		MOB_INDEX_DATA *mobindex = ARG_MOB(0)->pIndexData;

		if( ISARG_INSTANCE(1) )
			*ret = instance_count_mob(ARG_INSTANCE(1), mobindex);
		else if( ISARG_DUNGEON(1) )
			*ret = dungeon_count_mob(ARG_DUNGEON(1), mobindex);
		else
			*ret = mobindex->count;
	}
	else if(ISARG_OBJ(0))
	{
		*ret = ARG_OBJ(0)->pIndexData->count;
	}

	return true;
}

// PROTOCOL $PLAYER field
// FIELD:
//  * mxp
//  * msp
//  * more to come?
DECL_IFC_FUN(ifc_protocol)
{
	*ret = false;
	if(VALID_PLAYER(0) && ISARG_STR(1) && !IS_NULLSTR(ARG_STR(1)))
	{
		DESCRIPTOR_DATA *d = ARG_MOB(0)->desc;

		if( d != NULL )
		{
			protocol_t *p = d->pProtocol;


			if(!str_cmp(ARG_STR(1), "mxp"))
			{
				*ret = (p && p->bMXP);
			}
			else if(!str_cmp(ARG_STR(1), "msp"))
			{
				*ret = (p && p->bMSP);
			}
		}
	}

	return true;
}

DECL_IFC_FUN(ifc_isboss)
{
	*ret = VALID_NPC(0) ? IS_BOSS(ARG_MOB(0)) : false;
	return true;
}

DECL_IFC_FUN(ifc_dungeonflag)
{
	*ret = (ISARG_DUNGEON(0) && ISARG_STR(1) && IS_SET(ARG_DUNGEON(0)->flags, flag_value_ifcheck( dungeon_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_instanceflag)
{
	*ret = (ISARG_INSTANCE(0) && ISARG_STR(1) && IS_SET(ARG_INSTANCE(0)->flags, flag_value_ifcheck( instance_flags,ARG_STR(1))));
	return true;
}

DECL_IFC_FUN(ifc_sectionflag)
{
	*ret = (ISARG_SECTION(0) && ISARG_STR(1) && IS_SET(ARG_SECTION(0)->section->flags, flag_value_ifcheck( blueprint_section_flags,ARG_STR(1))));
	return true;
}

// ISAREAUNLOCKED $PLAYER $AREA|$ROOM|$AREA_ID
DECL_IFC_FUN(ifc_isareaunlocked)
{
	*ret = false;
	if( VALID_PLAYER(0) )
	{
		if( ISARG_AREA(1) )		*ret = is_area_unlocked(ARG_MOB(0), ARG_AREA(1));
		else if(ISARG_ROOM(1))	*ret = is_room_unlocked(ARG_MOB(0), ARG_ROOM(1));
		else if(ISARG_NUM(1))
		{
			AREA_DATA *area = get_area_from_uid(ARG_NUM(1));
			*ret = (area && is_area_unlocked(ARG_MOB(0), area));
		}
	}
	return true;
}

DECL_IFC_FUN(ifc_isowner)
{
	*ret = false;
	if( ISARG_SHIP(0) )
	{
		if( ISARG_MOB(1) ) *ret = ship_isowner_player(ARG_SHIP(0), ARG_MOB(1));
	}

	return true;
}

DECL_IFC_FUN(ifc_shiptype)
{
	*ret = false;
	if( ISARG_SHIP(0) && ISARG_STR(1) )
	{
		*ret = ARG_SHIP(0)->ship_type == flag_value_ifcheck(ship_class_types, ARG_STR(1));
	}
	return true;
}

// ISPROG $MOBILE|OBJECT|ROOM $STRING
//
// Checks whether the entity has the specified program flags
DECL_IFC_FUN(ifc_isprog)
{
	long flag;
	*ret = false;
	if (ISARG_STR(0))
	{
		flag = script_flag_value(prog_entity_flags, ARG_STR(0));

		if (flag && flag != NO_FLAG)
		{
			if (info->mob) *ret = PROG_FLAG(info->mob, flag) && true;
			else if (info->obj) *ret = PROG_FLAG(info->obj, flag) && true;
			else if (info->room) *ret = PROG_FLAG(info->room, flag) && true;
		}
	}
	else if (ISARG_MOB(0))
	{
		if (ISARG_STR(1))
		{
			flag = script_flag_value(prog_entity_flags, ARG_STR(1));

			*ret = (flag && flag != NO_FLAG) && PROG_FLAG(ARG_MOB(0), flag) && true;
		}
	}
	else if (ISARG_OBJ(0))
	{
		if (ISARG_STR(1))
		{
			flag = script_flag_value(prog_entity_flags, ARG_STR(1));

			*ret = (flag && flag != NO_FLAG) && PROG_FLAG(ARG_OBJ(0), flag) && true;
		}
	}
	else if (ISARG_ROOM(0))
	{
		if (ISARG_STR(1))
		{
			flag = script_flag_value(prog_entity_flags, ARG_STR(1));

			*ret = (flag && flag != NO_FLAG) && PROG_FLAG(ARG_ROOM(0), flag) && true;
		}
	}

	return true;
}

// BIT $BITVECTOR $STRING
// BIT $BITMATRIX $STRING
DECL_IFC_FUN(ifc_bit)
{
	if (ISARG_BV(0) && ISARG_STR(1))
	{
		long flag = script_flag_value(ARG_BV(0).table,ARG_STR(1));

		*ret = (flag && flag != NO_FLAG) && IS_SET(ARG_BV(0).value, flag) && true;

		return true;
	}
	else if(ISARG_BM(0) && ISARG_STR(1))
	{
		*ret = bitmatrix_isset(ARG_STR(1), ARG_BM(0).bank, ARG_BM(0).values);

		return true;
	}
	return false;
}