#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"

//#define DEBUG_MODULE
#include "debug.h"


//////////////////////////////////////
// A

// ADDAFFECT mobile|object apply-type(string) affect-group(string) skill(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
SCRIPT_CMD(scriptcmd_addaffect)
{
	char *rest;
	int where, group, skill, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;
	SCRIPT_PARAM arg;
	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,&arg))) {
		bug("AddAffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg.d.str, FALSE)))
			obj = script_get_obj_here(info, arg.d.str);
		break;
	case ENT_MOBILE: mob = arg.d.mob; break;
	case ENT_OBJECT: obj = arg.d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("Addaffect - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: where = flag_lookup(arg.d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_WEAPON)
			group = flag_lookup(arg.d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg.d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;


	//
	// Get SKILL number (built-in skill)
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: skill = skill_lookup(arg.d.str); break;
	default: return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: level = arg.d.num; break;
	case ENT_STRING: level = atoi(arg.d.str); break;
	case ENT_MOBILE: level = arg.d.mob->tot_level; break;
	case ENT_OBJECT: level = arg.d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: loc = flag_lookup(arg.d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: mod = arg.d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: hours = arg.d.num; break;
	default: return;
	}


	//
	// Get BITVECTOR
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg.d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;


	//
	// Get BITVECTOR2
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg.d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;


	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,&arg))) {
			bug("Addaffect - Error in parsing.",0);
			return;
		}

		switch(arg.type) {
		case ENT_OBJECT: wear_loc = arg.d.obj ? arg.d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group	= group;
	af.where     = where;
	af.type      = skill;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = NULL;
	af.slot = wear_loc;
	if(mob) affect_join_full(mob, &af);
	else affect_join_full_obj(obj,&af);
}

// ADDAFFECTNAME mobile|object apply-type(string) affect-group(string) name(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
SCRIPT_CMD(scriptcmd_addaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;
	SCRIPT_PARAM arg;
	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg.d.str, FALSE)))
			obj = script_get_obj_here(info, arg.d.str);
		break;
	case ENT_MOBILE: mob = arg.d.mob; break;
	case ENT_OBJECT: obj = arg.d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("AddAffectName - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: where = flag_lookup(arg.d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_WEAPON)
			group = flag_lookup(arg.d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg.d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;


	//
	// Get NAME
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: name = create_affect_cname(arg.d.str); break;
	default: return;
	}

	if(!name) {
		bug("AddAffectName - Error allocating affect name.",0);
		return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: level = arg.d.num; break;
	case ENT_STRING: level = atoi(arg.d.str); break;
	case ENT_MOBILE: level = arg.d.mob->tot_level; break;
	case ENT_OBJECT: level = arg.d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: loc = flag_lookup(arg.d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: mod = arg.d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: hours = arg.d.num; break;
	default: return;
	}


	//
	// Get BITVECTOR
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg.d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;


	//
	// Get BITVECTOR2
	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg.d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;


	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,&arg))) {
			bug("AddAffectName - Error in parsing.",0);
			return;
		}

		switch(arg.type) {
		case ENT_OBJECT: wear_loc = arg.d.obj ? arg.d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group	= group;
	af.where     = where;
	af.type      = -1;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = name;
	af.slot = wear_loc;
	if(mob) affect_join_full(mob, &af);
	else affect_join_full_obj(obj,&af);
}

// APPLYTOXIN mobile string(toxin) int(level) int(duration)
SCRIPT_CMD(scriptcmd_applytoxin)
{
	char *rest;
	CHAR_DATA *victim = NULL;
	int level, duration, toxin;
	SCRIPT_PARAM arg;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,&arg)))
		return;

	switch(arg.type) {
	case ENT_STRING: victim = script_get_char_room(info, arg.d.str, FALSE); break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) return;

	if (!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_STRING ) return;
	if( (toxin = toxin_lookup(arg.d.str)) < 0) return;

	if (!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_NUMBER ) return;
	level = UMAX(arg.d.num, 1);

	if (!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_NUMBER ) return;
	duration = UMAX(5, arg.d.num);

	victim->bitten_type = toxin;
	victim->bitten = UMAX(500/level, 30);
	victim->bitten_level = level;

	if (!IS_SET(victim->affected_by2, AFF2_TOXIN)) {
		AFFECT_DATA af;
		af.where = TO_AFFECTS;
		af.group     = AFFGROUP_BIOLOGICAL;
		af.type  = gsn_toxins;
		af.level = victim->bitten_level;
		af.duration = duration;
		af.location = APPLY_STR;
		af.modifier = -1 * number_range(1,3);
		af.bitvector = 0;
		af.bitvector2 = AFF2_TOXIN;
		af.slot	= WEAR_NONE;
		affect_to_char(victim, &af);
	}

	info->progs->lastreturn = 1;
}

// AWARD mobile string(type) number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp, experience/xp
//
// AWARD church string(type) number(amount)
// Types: gold, pneuma, deity/dp
//
SCRIPT_CMD(scriptcmd_award)
{
	char buf[MSL], *rest;
	char field[MIL];
	char *field_name;
	CHAR_DATA *victim = NULL;
	CHURCH_DATA *church = NULL;
	int amount = 0;
	SCRIPT_PARAM arg;


	if (!(rest = expand_argument(info,argument,&arg)))
		return;

	switch(arg.type) {
	case ENT_CHURCH: church = arg.d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg.d.str, TRUE); break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if (!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_STRING ) return;
	strncpy(field,arg.d.str,MIL-1);

	if (!(rest = expand_argument(info,rest,&arg)))
		return;

	switch(arg.type) {
	case ENT_STRING: amount = atoi(arg.d.str); break;
	case ENT_NUMBER: amount = arg.d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	if( church ) {
		if( !str_prefix(field, "gold") ) {
			church->gold += amount;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			church->pneuma += amount;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			church->dp += amount;
			field_name = "deity points";

		} else
			return;


		sprintf(buf, "Award logged: Church %s was awarded %d %s", church->name, amount, field_name);
		log_string(buf);

	} else {

		if( !str_prefix(field, "silver") ) {
			victim->silver += amount;
			field_name = "silver";

		} else if( !str_prefix(field, "gold") ) {
			victim->gold += amount;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			victim->pneuma += amount;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			victim->deitypoints += amount;
			field_name = "deity points";

		} else if( !str_prefix(field, "practice") ) {
			victim->practice += amount;
			field_name = "practices";

		} else if( !str_prefix(field, "train") ) {
			victim->train += amount;
			field_name = "trains";

		} else if( !str_prefix(field, "quest") || !str_cmp(field, "qp") ) {
			victim->questpoints += amount;
			field_name = "quest points";

		} else if( !str_prefix(field, "experience") || !str_cmp(field, "xp") ) {
			gain_exp(victim, amount);
			field_name = "experience";

		} else
			return;


		if(!IS_NPC(victim)) {
			sprintf(buf, "Award logged: %s was awarded %d %s", victim->name, amount, field_name);
			log_string(buf);
		}
	}
}

//////////////////////////////////////
// B

//////////////////////////////////////
// C

//////////////////////////////////////
// D

// DAMAGE mobile|'all' lower upper 'lethal'|'kill'|string damageclass[ attacker]
// DAMAGE mobile|'all' 'level'|'dual'|'remort'|'dualremort' mobile|number 'lethal'|'kill'|string damageclass[ attacker]
SCRIPT_CMD(scriptcmd_damage)
{
	char *rest;
	CHAR_DATA *victim = NULL, *victim_next, *attacker = NULL;
	int low, high, level, value, dc;
	bool fAll = FALSE, fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;
	SCRIPT_PARAM arg;

	if(!(rest = expand_argument(info,argument,&arg))) {
		//bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg.type) {
	case ENT_STRING:
		if(!str_cmp(arg.d.str,"all")) fAll = TRUE;
		else victim = script_get_char_room(info, arg.d.str, FALSE);
		break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll)
		return;

	if (fAll && !info->location)
		return;

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	switch(arg.type) {
	case ENT_NUMBER: low = arg.d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg.d.str,"level")) { fLevel = TRUE; break; }
		if(!str_cmp(arg.d.str,"remort")) { fLevel = fRemort = TRUE; break; }
		if(!str_cmp(arg.d.str,"dual")) { fLevel = fTwo = TRUE; break; }
		if(!str_cmp(arg.d.str,"dualremort")) { fLevel = fTwo = fRemort = TRUE; break; }
		if(is_number(arg.d.str)) { low = atoi(arg.d.str); break; }
	default:
		return;
	}

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	if(fLevel && !victim)
		return;

	level = victim ? victim->tot_level : 1;

	switch(arg.type) {
	case ENT_NUMBER:
		if(fLevel) level = arg.d.num;
		else high = arg.d.num;
		break;
	case ENT_STRING:
		if(is_number(arg.d.str)) {
			if(fLevel) level = atoi(arg.d.str);
			else high = atoi(arg.d.str);
		} else
			return;
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg.d.mob) level = arg.d.mob->tot_level;
			else
				return;
		} else
			return;
		break;
	default:
//		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		if( arg.type != ENT_STRING ) return;

		if (!str_cmp(arg.d.str,"kill") || !str_cmp(arg.d.str,"lethal")) fKill = TRUE;
	}

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		if( arg.type != ENT_STRING ) return;

		dc = damage_class_lookup(arg.d.str);
	} else
		dc = DAM_NONE;

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		if( arg.type != ENT_MOBILE || !arg.d.mob) return;

		attacker = arg.d.mob;

	} else
		attacker = NULL;


	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = info->location->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			if (victim != info->mob && (!attacker || victim != attacker)) {
				value = fLevel ? dice(low,high) : number_range(low,high);
				damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
			}
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
	}
}


// DEDUCT mobile string(type) number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp
// Returns actual amount deducted
//
// DEDUCT church string(type) number(amount)
// Types: gold, pneuma, deity/dp
// Returns actual amount deducted
//
SCRIPT_CMD(scriptcmd_deduct)
{
	char buf[MSL], *rest;
	char field[MIL];
	char *field_name;
	CHAR_DATA *victim = NULL;
	CHURCH_DATA *church = NULL;
	int amount = 0;
	SCRIPT_PARAM arg;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	switch(arg.type) {
	case ENT_CHURCH: church = arg.d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg.d.str, TRUE); break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_STRING ) return;
	strncpy(field,arg.d.str,MIL-1);

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	switch(arg.type) {
	case ENT_STRING: amount = atoi(arg.d.str); break;
	case ENT_NUMBER: amount = arg.d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	if( church ) {
		if( !str_prefix(field, "gold") ) {
			info->progs->lastreturn = UMIN(church->gold, amount);
			church->gold -= info->progs->lastreturn;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			info->progs->lastreturn = UMIN(church->pneuma, amount);
			church->pneuma -= info->progs->lastreturn;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			info->progs->lastreturn = UMIN(church->dp, amount);
			church->dp -= info->progs->lastreturn;
			field_name = "deity points";

		} else
			return;

		sprintf(buf, "Deduct logged: Church %s was deducted %d %s", church->name, amount, field_name);
		log_string(buf);
	} else {
		if( !str_prefix(field, "silver") ) {
			info->progs->lastreturn = UMIN(victim->silver, amount);
			victim->silver -= info->progs->lastreturn;
			field_name = "silver";

		} else if( !str_prefix(field, "gold") ) {
			info->progs->lastreturn = UMIN(victim->gold, amount);
			victim->gold -= info->progs->lastreturn;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			info->progs->lastreturn = UMIN(victim->pneuma, amount);
			victim->pneuma -= info->progs->lastreturn;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			info->progs->lastreturn = UMIN(victim->deitypoints, amount);
			victim->deitypoints -= info->progs->lastreturn;
			field_name = "deity points";

		} else if( !str_prefix(field, "practice") ) {
			info->progs->lastreturn = UMIN(victim->practice, amount);
			victim->practice -= info->progs->lastreturn;
			field_name = "practices";

		} else if( !str_prefix(field, "train") ) {
			info->progs->lastreturn = UMIN(victim->train, amount);
			victim->train -= info->progs->lastreturn;
			field_name = "trains";

		} else if( !str_prefix(field, "quest") || !str_cmp(field, "qp") ) {
			info->progs->lastreturn = UMIN(victim->questpoints, amount);
			victim->questpoints -= info->progs->lastreturn;
			field_name = "quest points";

		} else
			return;


		if(!IS_NPC(victim)) {
			sprintf(buf, "Deduct logged: %s was deducted %d %s", victim->name, amount, field_name);
			log_string(buf);
		}
	}

}

//////////////////////////////////////
// E

// ENTERCOMBAT[ $ATTACKER] $VICTIM[ $SILENT]
// Switches target explicitly without triggering any standard combat scripts
//  - used for scripts in combat to change targets
SCRIPT_CMD(scriptcmd_entercombat)
{
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;
	SCRIPT_PARAM arg;
	bool fSilent = FALSE;


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg))) {
		bug("MpStartCombat - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg.type) {
	case ENT_STRING: victim = script_get_char_room(info, arg.d.str, FALSE); break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		attacker = victim;
		if( arg.type == ENT_BOOLEAN ) {
			// VICTIM SILENT syntax
			fSilent = arg.d.boolean == TRUE;
		} else {
			switch(arg.type) {
			case ENT_STRING: victim = script_get_char_room(info, arg.d.str, FALSE); break;
			case ENT_MOBILE: victim = arg.d.mob; break;
			default: victim = NULL; break;
			}

			if (!victim)
				return;

			if(*rest ) {
				if(!expand_argument(info,rest,&arg))
					return;

				// ATTACKER VICTIM SILENT syntax
				if( arg.type == ENT_BOOLEAN )
					fSilent = arg.d.boolean == TRUE;
			}
		}
	} else if(!info->mob)
		return;
	else
		attacker = info->mob;

	enter_combat(attacker, victim, fSilent);

	if( attacker->fighting == victim )
		info->progs->lastreturn = 1;
}


//////////////////////////////////////
// F

// FLEE[ mobile[ direction[ conceal[ pursue]]]]
// mobile - target of action (Only optional in mprog)
// direction - direction of flee
//				"none" = random direction (same as doing "flee")
//				"anyway" = random direction (same as used in places like intimidate)
//				"wimpy" = random direction (same as automatic wimpy fleeing)
// conceal - conceal whether the flee is kept hidden from the opponent
// pursue - allows pursuit (only if the flee is successful)
//
// Assigns LASTRETURN with direction of flee
//	if < 0, the flee action FAILED
SCRIPT_CMD(scriptcmd_flee)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *target;
	int door = -1;
	bool conceal = FALSE, pursue = TRUE;
	char fleedata[MIL];
	char *fleearg = str_empty;

	info->progs->lastreturn = -1;
	if(!info) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	target = info->mob;
	switch(arg.type) {
	case ENT_STRING: target = script_get_char_room(info, arg.d.str, TRUE); break;
	case ENT_MOBILE: target = arg.d.mob; break;
	default: target = NULL; break;
	}

	if (!target) return;

	if (!target->fighting || !target->in_room)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,&arg)))
				return;

		if(arg.type == ENT_STRING) {
			if (!str_cmp(arg.d.str, "none")) {
				fleearg = str_empty;
			} else if (!str_cmp(arg.d.str, "anyway")) {
				strcpy(fleedata,"anyway");
				fleearg = fleedata;
			} else if (!str_cmp(arg.d.str, "wimpy"))
				fleearg = NULL;
			else {
				strncpy(fleedata,arg.d.str,sizeof(fleedata)-1);
				fleearg = fleedata;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,&arg)))
					return;

				if( arg.type == ENT_NUMBER )
					conceal = (arg.d.num != 0);
				else if( arg.type == ENT_STRING )
					conceal = !str_cmp(arg.d.str, "yes") || !str_cmp(arg.d.str, "true") || !str_cmp(arg.d.str, "all");
				else
					return;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,&arg)))
					return;

				if( arg.type == ENT_NUMBER )
					pursue = (arg.d.num != 0);
				else if( arg.type == ENT_STRING )
					pursue = !str_cmp(arg.d.str, "yes") || !str_cmp(arg.d.str, "true") || !str_cmp(arg.d.str, "all");
				else
					return;
			}

		}
	}

	door = do_flee_full(target, fleearg, conceal, pursue);
	info->progs->lastreturn = door;
}


//////////////////////////////////////
// G


// GRANTSKILL player name[ int(rating=1)[ bool(permanent=false)[ string(flags)]]]
// GRANTSKILL player vnum[ int(rating=1)[ bool(permanent=false)[ string(flags)]]]
SCRIPT_CMD(scriptcmd_grantskill)
{
	char buf[MSL];
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *mob;
	TOKEN_DATA *token = NULL;
	TOKEN_INDEX_DATA *token_index = NULL;
	int rating = 1;
	int sn = -1;
	long flags = SKILL_AUTOMATIC;
	char source = SKILLSRC_SCRIPT;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	mob = arg.d.mob;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type == ENT_STRING ) {
		sn = skill_lookup(arg.d.str);
		if( sn <= 0 ) return;
	} else if( arg.type == ENT_NUMBER ) {
		token_index = get_token_index(arg.d.num);

		if( !token_index ) return;
	}
	else
		return;


	if( *rest ) {

		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		if( arg.type != ENT_NUMBER ) return;
		rating = URANGE(1,arg.d.num,100);

		if( *rest ) {
			bool fPerm = FALSE;

			if(!(rest = expand_argument(info,rest,&arg)))
				return;

			if( arg.type == ENT_BOOLEAN )
				fPerm = arg.d.boolean;
			else if( arg.type == ENT_NUMBER )
				fPerm = (arg.d.num != 0);
			else if( arg.type == ENT_STRING )
				fPerm = !str_cmp(arg.d.str, "true") || !str_cmp(arg.d.str, "yes") || !str_cmp(arg.d.str, "perm");
			else
				return;

			source = fPerm ? SKILLSRC_SCRIPT_PERM : SKILLSRC_SCRIPT;

			if( *rest ) {
				expand_string(info,rest,buf);

				if(!buf[0]) return;

				if (!str_cmp(buf, "none"))
					flags = 0;
				else if ((flags = flag_value(skill_flags, buf)) == NO_FLAG)
					flags = SKILL_AUTOMATIC;
			}
		}
	}

	if( token_index )
	{
		if( skill_entry_findtokenindex(mob->sorted_skills, token_index) )
			return;


		token = create_token(token_index);

		if(!token) return;


		if( token_index->value[TOKVAL_SPELL_RATING] > 0 )
			token->value[TOKVAL_SPELL_RATING] = token_index->value[TOKVAL_SPELL_RATING] * rating;
		else
			token->value[TOKVAL_SPELL_RATING] = rating;

		token_to_char_ex(token, mob, source, flags);
	}
	else if(sn > 0 && sn < MAX_SKILL )
	{
		if( skill_entry_findsn(mob->sorted_skills, sn) )
			return;

		mob->pcdata->learned[sn] = rating;
		if( skill_table[sn].spell_fun == spell_null ) {
			skill_entry_addskill(mob, sn, NULL, source, flags);
		} else {
			skill_entry_addspell(mob, sn, NULL, source, flags);
		}
	}
	else
		return;

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// H

//////////////////////////////////////
// I

// INPUTSTRING $PLAYER script-vnum variable
//  Invokes the interal string editor, for use in getting multiline strings from players.
//
//  $PLAYER - player entity to get string from
//  script-vnum - script to call after the editor is closed
//  variable - name of variable to use to store the string (as well as supply the initial string)

SCRIPT_CMD(scriptcmd_inputstring)
{
	char buf[MSL];
	char *rest;
	int vnum;
	CHAR_DATA *mob = NULL;
	SCRIPT_PARAM arg;
	int type;

	info->progs->lastreturn = 0;

	if(info->mob) type = PRG_MPROG;
	else if(info->obj) type = PRG_OPROG;
	else if(info->room) type = PRG_RPROG;
	else if(info->token) type = PRG_TPROG;
	else
		return;


	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob) return;

	mob = arg.d.mob;
	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	// Are they already being prompted
	if(mob->pk_question ||
		mob->remove_question ||
		mob->personal_pk_question ||
		mob->cross_zone_question ||
		mob->pcdata->convert_church != -1 ||
		mob->challenged ||
		mob->remort_question)
		return;

	if(!(rest = expand_argument(info,rest,&arg))) {
		bug("MpInput - Error in parsing.",0);
		return;
	}

	switch(arg.type) {
	case ENT_NUMBER: vnum = arg.d.num; break;
	default: return;
	}

	if(vnum < 1 || !get_script_index(vnum, type)) return;

	expand_string(info,rest,buf);
	pVARIABLE var = variable_get(*(info->var),buf);

	mob->desc->input = TRUE;
	if( var && var->type == VAR_STRING && !IS_NULLSTR(var->_.s) )
		mob->desc->inputString = str_dup(var->_.s);
	else
		mob->desc->inputString = &str_empty[0];
	mob->desc->input_var = str_dup(buf);
	mob->desc->input_prompt = NULL;
	mob->desc->input_script = vnum;
	mob->desc->input_mob = info->mob;
	mob->desc->input_obj = info->obj;
	mob->desc->input_room = info->room;
	mob->desc->input_tok = info->token;

	string_append(mob, &mob->desc->inputString);

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// J

//////////////////////////////////////
// K

//////////////////////////////////////
// L

//////////////////////////////////////
// M

//////////////////////////////////////
// N

//////////////////////////////////////
// O

//////////////////////////////////////
// P

//////////////////////////////////////
// Q

// QUESTCOMPLETE $player $partno
SCRIPT_CMD(scriptcmd_questcomplete)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob) || !IS_QUESTING(arg.d.mob)) return;

	mob = arg.d.mob;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type != ENT_NUMBER ) return;

	if(check_quest_custom_task(mob, arg.d.num))
		info->progs->lastreturn = 1;
}

// QUESTPARTCUSTOM $PLAYER $STRING[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartcustom)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *questman;
	CHAR_DATA *ch;
	char desc[MSL];
	int minutes;

	info->progs->lastreturn = 0;

	questman = NULL;
	if(info->mob) questman = info->mob;
	else if(info->token && info->token->player) questman = info->token->player;

	// Only allow mprogs or tprogs where the ultimate mob calling them is a questor mob
	if(!IS_VALID(questman) || !IS_NPC(questman) || !IS_SET(questman->act, ACT_QUESTOR))
		return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	ch = arg.d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(!IS_NULLSTR(arg.d.str))
		return;

	strcpy(desc, arg.d.str);

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,argument,&arg)) || arg.type != ENT_NUMBER)
			return;

		minutes = UMAX(arg.d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	part->custom_task = str_dup(desc);
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTGETITEM $PLAYER $OBJECT[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgetitem)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *questman;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	int minutes;

	info->progs->lastreturn = 0;

	questman = NULL;
	if(info->mob) questman = info->mob;
	else if(info->token && info->token->player) questman = info->token->player;

	// Only allow mprogs or tprogs where the ultimate mob calling them is a questor mob
	if(!IS_VALID(questman) || !IS_NPC(questman) || !IS_SET(questman->act, ACT_QUESTOR))
		return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	ch = arg.d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_OBJECT || !IS_VALID(arg.d.obj)) return;
	obj = arg.d.obj;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,argument,&arg)) || arg.type != ENT_NUMBER)
			return;

		minutes = UMAX(arg.d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	free_string(obj->owner);
	obj->owner = str_dup(ch->name);
	part->obj = obj;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTGOTO $PLAYER first|second|both|$ROOM[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgoto)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *questman;
	CHAR_DATA *ch;
	ROOM_INDEX_DATA *destination;
	int minutes;

	info->progs->lastreturn = 0;

	questman = NULL;
	if(info->mob) questman = info->mob;
	else if(info->token && info->token->player) questman = info->token->player;

	// Only allow mprogs or tprogs where the ultimate mob calling them is a questor mob
	if(!IS_VALID(questman) || !IS_NPC(questman) || !IS_SET(questman->act, ACT_QUESTOR))
		return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	ch = arg.d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	destination = NULL;
	switch(arg.type) {
	case ENT_STRING:
		if(!str_cmp(arg.d.str,"first")) destination = get_random_room(ch, FIRST_CONTINENT);
		else if(!str_cmp(arg.d.str,"second")) destination = get_random_room(ch, SECOND_CONTINENT);
		else if(!str_cmp(arg.d.str,"both")) destination = get_random_room(ch, BOTH_CONTINENTS);
		break;
	case ENT_ROOM:
		destination = arg.d.room;
		break;
	default: return;
	}

	if(!destination)
		return;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,argument,&arg)) || arg.type != ENT_NUMBER)
			return;

		minutes = UMAX(arg.d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	part->room = destination->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTRESCUE $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartrescue)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *questman;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	questman = NULL;
	if(info->mob) questman = info->mob;
	else if(info->token && info->token->player) questman = info->token->player;

	// Only allow mprogs or tprogs where the ultimate mob calling them is a questor mob
	if(!IS_VALID(questman) || !IS_NPC(questman) || !IS_SET(questman->act, ACT_QUESTOR))
		return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	ch = arg.d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !IS_VALID(arg.d.mob) || !IS_NPC(arg.d.mob)) return;
	target = arg.d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,argument,&arg)) || arg.type != ENT_NUMBER)
			return;

		minutes = UMAX(arg.d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	part->mob_rescue = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTSLAY $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartslay)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *questman;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	questman = NULL;
	if(info->mob) questman = info->mob;
	else if(info->token && info->token->player) questman = info->token->player;

	// Only allow mprogs or tprogs where the ultimate mob calling them is a questor mob
	if(!IS_VALID(questman) || !IS_NPC(questman) || !IS_SET(questman->act, ACT_QUESTOR))
		return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	ch = arg.d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !IS_VALID(arg.d.mob) || !IS_NPC(arg.d.mob)) return;
	target = arg.d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,argument,&arg)) || arg.type != ENT_NUMBER)
			return;

		minutes = UMAX(arg.d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	part->mob = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


//////////////////////////////////////
// R

// REVOKESKILL player name
// REVOKESKILL player vnum
SCRIPT_CMD(scriptcmd_revokeskill)
{
	char *rest;
	SKILL_ENTRY *entry;
	SCRIPT_PARAM arg;
	CHAR_DATA *mob;
	TOKEN_INDEX_DATA *token_index = NULL;
	int sn = -1;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob || IS_NPC(arg.d.mob)) return;

	mob = arg.d.mob;

	if(!(rest = expand_argument(info,rest,&arg)))
		return;

	if( arg.type == ENT_STRING ) {
		sn = skill_lookup(arg.d.str);
		if( sn <= 0 ) return;

		entry = skill_entry_findsn(mob->sorted_skills, sn);

	} else if( arg.type == ENT_NUMBER ) {
		token_index = get_token_index(arg.d.num);

		if( !token_index ) return;

		entry = skill_entry_findtokenindex(mob->sorted_skills, token_index);

	}
	else
		return;

	if( !entry ) return;

	skill_entry_removeentry(&mob->sorted_skills, entry);
	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// S

SCRIPT_CMD(scriptcmd_setalign)
{
}

SCRIPT_CMD(scriptcmd_setclass)
{
}

SCRIPT_CMD(scriptcmd_setrace)
{
}

SCRIPT_CMD(scriptcmd_setsubclass)
{
}

// STARTCOMBAT[ $ATTACKER] $VICTIM
SCRIPT_CMD(scriptcmd_startcombat)
{
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;
	SCRIPT_PARAM arg;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg))) {
		bug("MpStartCombat - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}



	switch(arg.type) {
	case ENT_STRING: victim = script_get_char_room(info, arg.d.str, FALSE); break;
	case ENT_MOBILE: victim = arg.d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!expand_argument(info,rest,&arg))
			return;

		attacker = victim;
		switch(arg.type) {
		case ENT_STRING: victim = script_get_char_room(info, arg.d.str, FALSE); break;
		case ENT_MOBILE: victim = arg.d.mob; break;
		default: victim = NULL; break;
		}

		if (!victim)
			return;
	} else if(!info->mob)
		return;
	else
		attacker = info->mob;


	// Attacker is fighting already
	if(attacker->fighting)
		return;

	// The victim is fighting someone else in a singleplay room
	if(!IS_NPC(attacker) && victim->fighting != NULL && victim->fighting != attacker && !IS_SET(attacker->in_room->room2_flags, ROOM_MULTIPLAY))
		return;

	// They are not in the same room
	if(attacker->in_room != victim->in_room)
		return;

	// The victim is safe
	if(is_safe(attacker, victim, FALSE)) return;

	// Set them to fighting!
	if(set_fighting(attacker, victim))
		info->progs->lastreturn = 1;
}


// STOPCOMBAT $MOBILE[ bool(BOTH)]
// Silently stops combat.
// BOTH: causes both sides to stop fighting, defaults to false
SCRIPT_CMD(scriptcmd_stopcombat)
{
	char *rest;
	SCRIPT_PARAM arg;
	CHAR_DATA *mob;
	bool fBoth = FALSE;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,&arg)))
		return;

	if(arg.type != ENT_MOBILE || !arg.d.mob) return;

	mob = arg.d.mob;

	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,&arg)))
			return;

		if( arg.type == ENT_BOOLEAN )
			fBoth = arg.d.boolean;
		else if( arg.type == ENT_NUMBER )
			fBoth = (arg.d.num != 0);
		else if( arg.type == ENT_STRING )
			fBoth = (!str_cmp(arg.d.str,"yes") || !str_cmp(arg.d.str,"true"));
	}

	stop_fighting(mob, fBoth);

	if( mob->fighting == NULL )
		info->progs->lastreturn = 1;
}


//////////////////////////////////////
// T

//////////////////////////////////////
// U

//////////////////////////////////////
// V

//////////////////////////////////////
// W

//////////////////////////////////////
// X

//////////////////////////////////////
// Y

//////////////////////////////////////
// Z



