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
#include "magic.h"

//#define DEBUG_MODULE
#include "debug.h"


///////////////////////////////////////////
//
// Function: do_apdump
//
// Section: Script/APROG
//
// Purpose: Displays the current edit source code of an APROG.
//
// Syntax: apdump <vnum>
//
// Restrictions: Viewer must have READ access on the script to see it.
//
void do_apdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *aprg;
	long vnum;

	one_argument(argument, buf);
	vnum = atoi(buf);

	if (!(aprg = get_script_index(vnum, PRG_APROG))) {
		send_to_char("No such AREAprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,aprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(aprg->edit_src, ch);
}


///////////////////////////////////////////
//
// Function: do_ipdump
//
// Section: Script/IPROG
//
// Purpose: Displays the current edit source code of an IPROG.
//
// Syntax: ipdump <vnum>
//
// Restrictions: Viewer must be able to edit blueprints
//
void do_ipdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *iprg;
	long vnum;

	one_argument(argument, buf);
	vnum = atoi(buf);

	if (!(iprg = get_script_index(vnum, PRG_IPROG))) {
		send_to_char("No such INSTANCEprogram.\n\r", ch);
		return;
	}

	if (!can_edit_blueprints(ch)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(iprg->edit_src, ch);
}

///////////////////////////////////////////
//
// Function: do_dpdump
//
// Section: Script/DPROG
//
// Purpose: Displays the current edit source code of a DPROG.
//
// Syntax: dpdump <vnum>
//
// Restrictions: Viewer must be able to edit dungeons
//
void do_dpdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *dprg;
	long vnum;

	one_argument(argument, buf);
	vnum = atoi(buf);

	if (!(dprg = get_script_index(vnum, PRG_DPROG))) {
		send_to_char("No such DUNGEONprogram.\n\r", ch);
		return;
	}

	if (!can_edit_dungeons(ch)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(dprg->edit_src, ch);
}




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

	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AddAffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg->d.str, FALSE)))
			obj = script_get_obj_here(info, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("Addaffect - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_WEAPON)
			group = flag_lookup(arg->d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg->d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;


	//
	// Get SKILL number (built-in skill)
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}


	//
	// Get BITVECTOR
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;


	//
	// Get BITVECTOR2
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;


	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("Addaffect - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
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

	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg->d.str, FALSE)))
			obj = script_get_obj_here(info, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("AddAffectName - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_WEAPON)
			group = flag_lookup(arg->d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg->d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;


	//
	// Get NAME
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: name = create_affect_cname(arg->d.str); break;
	default: return;
	}

	if(!name) {
		bug("AddAffectName - Error allocating affect name.",0);
		return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}


	//
	// Get BITVECTOR
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;


	//
	// Get BITVECTOR2
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;


	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("AddAffectName - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
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


	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	if( (toxin = toxin_lookup(arg->d.str)) < 0) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;
	level = UMAX(arg->d.num, 1);

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;
	duration = UMAX(5, arg->d.num);

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


// ATTACH $ENTITY $TARGET $STRING[ $SILENT]
// Attaches the $ENTITY to the given field ($STRING) on $TARGET
// Fields and what they require for ENTITY and TARGET
// * PET: NPC, MOBILE
// * MASTER/FOLLOWER: MOBILE, MOBILE
// * LEADER/GROUP: MOBILE, MOBILE
// * CART/PULL: OBJECT (Pullable), MOBILE
// * ON: FURNITURE, MOBILE/OBJECT
//
// $SILENT is a boolean to indicate whether the action is silent
//
// LASTRETURN will be set to 1 if successful, 0 otherwise
SCRIPT_CMD(scriptcmd_attach)
{
	char *rest;
	char field[MIL];
	CHAR_DATA *entity_mob = NULL;
	CHAR_DATA *target_mob = NULL;
	OBJ_DATA *entity_obj = NULL;
	OBJ_DATA *target_obj = NULL;

	bool show = TRUE;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_MOBILE ) entity_mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) entity_obj = arg->d.obj;
	else
		return;

	if (!entity_mob && !entity_obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE ) target_mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) target_obj = arg->d.obj;
	else
		return;

	if (!target_mob && !target_obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);
	field[MIL] = '\0';

	if(*rest) {
		if (!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			show = !arg->d.boolean;
	}

	if(entity_mob != NULL)
	{
		if(field[0] != '\0')
		{
			if(!str_prefix(field, "pet"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE and doesn't have a pet already
				if(!IS_NPC(entity_mob) || !target_mob || target_mob->pet != NULL)
					return;

				// $ENTITY is following someone else
				if( entity_mob->master != NULL && entity_mob->master != target_mob ) return;

				// Don't allow since their groups is already full
				if( target_mob->num_grouped >= 9 )
					return;

				add_follower(entity_mob, target_mob, show);
				add_grouped(entity_mob, target_mob, show);	// Checks are already done

				target_mob->pet = entity_mob;
				SET_BIT(entity_mob->act, ACT_PET);
				SET_BIT(entity_mob->affected_by, AFF_CHARM);
				entity_mob->comm = COMM_NOTELL|COMM_NOCHANNELS;

			}
			else if(!str_prefix(field, "master") || !str_cmp(field, "follower"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE
				if(!IS_NPC(entity_mob) || !target_mob)
					return;

				// $ENTITY is already following someone
				if( entity_mob->master != NULL ) return;

				add_follower(entity_mob, target_mob, show);
			}
			else if(!str_prefix(field, "leader") || !str_cmp(field, "group"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE and isn't aleady grouped
				if(!IS_NPC(entity_mob) || !target_mob || target_mob->leader != NULL)
					return;

				// $ENTITY is already following someone else
				if( entity_mob->master != NULL && entity_mob->master != target_mob ) return;

				// $ENTITY is already grouped
				if( entity_mob->leader != NULL ) return;

				// Don't allow since their groups is already full
				if( target_mob->num_grouped >= 9 )
					return;

				add_follower(entity_mob, target_mob, show);
				add_grouped(entity_mob, target_mob, show);	// Checks are already done
			}
		}
	}
	else	// entity_obj != NULL
	{
		if(!str_prefix(field,"cart") || !str_prefix(field,"pull") )
		{
			if( target_mob == NULL || target_mob->pulled_cart != NULL ) return;

			// Already being pulled
			if( entity_obj->pulled_by != NULL ) return;

			// Check it is pullable
			if( !is_pullable(entity_obj) ) return;

			target_mob->pulled_cart = entity_obj;
			entity_obj->pulled_by = target_mob;
		}
		else if(!str_prefix(field,"on") )
		{
			// $ENTITY cannot already be on something
			if( entity_obj->on != NULL ) return;

			// $ENTITY is not furniture
			if( entity_obj->item_type != ITEM_FURNITURE ) return;

			if( target_mob != NULL )
			{
				target_mob->on = entity_obj;
			}
			else	// target_obj != NULL
			{
				target_obj->on = entity_obj;
			}
		}

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


	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_CHURCH: church = arg->d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
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

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// B

// BREATHE $VICTIM $TYPE[ $ATTACKER]
// Does the dragon breath of the given type: acid, fire, frost, gas, lightning
SCRIPT_CMD(scriptcmd_breathe)
{
	static char *breath_names[] = { "acid", "fire", "frost", "gas", "lightning", NULL };
	static sh_int *breath_gsn[] = { &gsn_acid_breath, &gsn_fire_breath, &gsn_frost_breath, &gsn_gas_breath, &gsn_lightning_breath };
	static SPELL_FUN *breath_fun[] = { spell_acid_breath, spell_fire_breath, spell_frost_breath, spell_gas_breath, spell_lightning_breath };
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;
	int i;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	for(i=0;breath_names[i] && str_prefix(arg->d.str,breath_names[i]);i++);

	if(!breath_names[i])
		return;

	attacker = info->mob;
	if(*rest) {
		if(!(rest = expand_argument(info,argument,arg)))
			return;

		switch(arg->type) {
		case ENT_STRING: attacker = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: attacker = arg->d.mob; break;
		default: attacker = NULL; break;
		}
	}

	if(!attacker)
		return;

	(*breath_fun[i]) (*breath_gsn[i] , attacker->tot_level, attacker, victim, TARGET_CHAR, WEAR_NONE);

	info->progs->lastreturn = 1;
}

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


	if(!(rest = expand_argument(info,argument,arg))) {
		//bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = script_get_char_room(info, arg->d.str, FALSE);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll)
		return;

	if (fAll && !info->location)
		return;

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = TRUE; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = TRUE; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = TRUE; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = TRUE; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		return;
	}

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(fLevel && !victim)
		return;

	level = victim ? victim->tot_level : 1;

	switch(arg->type) {
	case ENT_NUMBER:
		if(fLevel) level = arg->d.num;
		else high = arg->d.num;
		break;
	case ENT_STRING:
		if(is_number(arg->d.str)) {
			if(fLevel) level = atoi(arg->d.str);
			else high = atoi(arg->d.str);
		} else
			return;
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
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
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_STRING ) return;

		if (!str_cmp(arg->d.str,"kill") || !str_cmp(arg->d.str,"lethal")) fKill = TRUE;
	}

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_STRING ) return;

		dc = damage_class_lookup(arg->d.str);
	} else
		dc = DAM_NONE;

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_MOBILE || !arg->d.mob) return;

		attacker = arg->d.mob;

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


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_CHURCH: church = arg->d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
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


// DETACH $ENTITY $STRING[ $SILENT]
// Detaches the given field ($STRING) from $ENTITY

// Fields and what they require for ENTITY
// * PET: MOBILE
// * MASTER/FOLLOWER: MOBILE
// * LEADER/GROUP: MOBILE
// * CART: MOBILE
// * ON: MOBILE/OBJECT
//
// $SILENT is a boolean to indicate whether the action is silent
//
// LASTRETURN will be set to 1 if successful, 0 otherwise

SCRIPT_CMD(scriptcmd_detach)
{
	char *rest;
	char field[MIL];
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;

	bool show = TRUE;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_MOBILE ) mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) obj = arg->d.obj;
	else
		return;

	if (!mob && !obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);
	field[MIL] = '\0';

	if(*rest) {
		if (!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			show = !arg->d.boolean;
	}

	if( mob )
	{
		if( !str_prefix(field, "pet") )
		{
			if( mob->pet == NULL ) return;

			if( !IS_SET(mob->pet->pIndexData->act, ACT_PET) )
				REMOVE_BIT(mob->pet->act, ACT_PET);

			if( !IS_SET(mob->pet->pIndexData->affected_by, AFF_CHARM) )
				REMOVE_BIT(mob->pet->affected_by, AFF_CHARM);

			// This will not ungroup/unfollow
			mob->pet->comm &= ~(COMM_NOTELL|COMM_NOCHANNELS);
			mob->pet = NULL;
		}
		else if(!str_prefix(field, "master") || !str_cmp(field, "follower"))
		{
			if( mob->master == NULL ) return;

			if( mob->master->pet == mob )
			{
				if( !IS_SET(mob->pet->pIndexData->act, ACT_PET) )
					REMOVE_BIT(mob->pet->act, ACT_PET);

				if( !IS_SET(mob->pet->pIndexData->affected_by, AFF_CHARM) )
					REMOVE_BIT(mob->pet->affected_by, AFF_CHARM);

				// This will not ungroup/unfollow
				mob->pet->comm &= ~(COMM_NOTELL|COMM_NOCHANNELS);
				mob->pet = NULL;

			}

			stop_follower(mob, show);
		}
		else if(!str_prefix(field, "leader") || !str_cmp(field, "group"))
		{
			if( mob->leader == NULL ) return;

			stop_grouped(mob);
		}
		if(!str_prefix(field,"cart") || !str_prefix(field,"pull") )
		{
			if( mob->pulled_cart == NULL ) return;

			mob->pulled_cart->pulled_by = NULL;
			mob->pulled_cart = NULL;
		}
		else if( !str_prefix(field, "on") )
		{
			mob->on = NULL;
		}
	}
	else	// obj != NULL
	{
		if( !str_prefix(field, "on") )
		{
			obj->on = NULL;
		}
	}

	info->progs->lastreturn = 1;
}


//////////////////////////////////////
// E

// ED $OBJECT|$VROOM CLEAR
//    Clears out the entity's extra descriptions
//
// ED $OBJECT|$VROOM SET $NAME $STRING
//
// ED $OBJECT|$VROOM DELETE $NAME
//
// Extra description manipulation for rooms will only work in Wilderness and Clone rooms for now
//
SCRIPT_CMD(scriptcmd_ed)
{
	char *rest;
	EXTRA_DESCR_DATA **ed = NULL;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_OBJECT )
		ed = &arg->d.obj->extra_descr;
	else if( arg->type == ENT_ROOM )
	{
		if( !arg->d.room ) return;
		if( arg->d.room->source || arg->d.room->wilds )
			ed = &arg->d.room->extra_descr;
	}

	if( !ed )
		return;

	if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	if( !str_cmp(arg->d.str, "clear") )
	{
		EXTRA_DESCR_DATA *cur, *next;

		for(cur = *ed; cur; cur = next)
		{
			next = cur->next;
			free_extra_descr(cur);
		}

		*ed = NULL;
	}
	else if( !str_cmp(arg->d.str, "delete") )
	{
		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		EXTRA_DESCR_DATA *prev = NULL, *cur;

		for(cur = *ed; cur; cur = cur->next)
		{
			if( is_name(arg->d.str, cur->keyword) )
				break;
			else
				prev = cur;
		}

		if( !cur ) return;

		if( prev )
			prev->next = cur->next;
		else
			*ed = cur->next;

		free_extra_descr(cur);

	}
	else if( !str_cmp(arg->d.str, "set") )
	{
		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		// Save the keyword in a buffer
		BUFFER *tmp_buffer = new_buf();
		add_buf(tmp_buffer, arg->d.str);

		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		{
			free_buf(tmp_buffer);
			return;
		}

		EXTRA_DESCR_DATA *cur;

		for(cur = *ed; cur; cur = cur->next)
		{
			if( is_name(tmp_buffer->string, cur->keyword) )
				break;
		}

		if( !cur )
		{
			cur = new_extra_descr();
			cur->next = *ed;
			*ed = cur;

			free_string(cur->keyword);
			cur->keyword = str_dup(tmp_buffer->string);
		}

		free_string(cur->description);
		cur->description = str_dup(arg->d.str);

		free_buf(tmp_buffer);
	}

	info->progs->lastreturn = 1;
}

// ENTERCOMBAT[ $ATTACKER] $VICTIM[ $SILENT]
// Switches target explicitly without triggering any standard combat scripts
//  - used for scripts in combat to change targets
SCRIPT_CMD(scriptcmd_entercombat)
{
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;

	bool fSilent = FALSE;


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		attacker = victim;
		if( arg->type == ENT_BOOLEAN ) {
			// VICTIM SILENT syntax
			fSilent = arg->d.boolean == TRUE;
		} else {
			switch(arg->type) {
			case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
			case ENT_MOBILE: victim = arg->d.mob; break;
			default: victim = NULL; break;
			}

			if (!victim)
				return;

			if(*rest ) {
				if(!expand_argument(info,rest,arg))
					return;

				// ATTACKER VICTIM SILENT syntax
				if( arg->type == ENT_BOOLEAN )
					fSilent = arg->d.boolean == TRUE;
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

// FADE $PLAYER $DIRECTION $RATING[ $INTERRUPT]
// $PLAYER    - player to force into fading
// $DIRECTION - exit to travel in, "random"|"any" to pick a random exit
// $RATING    - skill rating to mimic the fading, ranging from 1 to 100
// $INTERRUPT - flag (default FALSE) on whether to interrupt the player.
//
// Fails if not a player
// Fails if player is rifting
// Fails if player is pulling a cart
// Fails if player is fighting
// Fails if direction does not exist
// Fails if rating is out of range
// Fails if interrupt was false and the player was busy
//
// LASTRETURN will return -1 for failure, otherwise will return the door number
SCRIPT_CMD(scriptcmd_fade)
{
	char *rest;
	CHAR_DATA *target;
	int door = -1;
	int rating;

	if(!info) return;
	info->progs->lastreturn = -1;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	target = info->mob;
	switch(arg->type) {
	case ENT_STRING: target = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: target = arg->d.mob; break;
	default: target = NULL; break;
	}

	if (!target || !IS_VALID(target) || IS_NPC(target)) return;

	// Block rifter, cart pullers and fighters
	if (IS_SOCIAL(target) || PULLING_CART(target) || (target->fighting != NULL)) return;

	if( IS_SET(target->in_room->area->area_flags, AREA_NO_FADING) ) return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	if( !str_cmp(arg->d.str, "random") || !str_cmp(arg->d.str, "any") )
	{
		door = number_range(1, MAX_DIR);
	}
	else
	{
		door = parse_door(arg->d.str);
		if( door < 0 )
			return;
	}

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return;

	if( arg->d.num < 1 || arg->d.num > 100 )
		return;

	rating = arg->d.num;

	bool busy = (is_char_busy(target) || target->desc->pString != NULL || target->desc->input);

	if( *rest )
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		// Check whether to make the busy flag false
		if( arg->type == ENT_BOOLEAN )
		{
			if( arg->d.boolean ) busy = FALSE;
		}
		else if( arg->type == ENT_NUMBER )
		{
			if( arg->d.num != 0 ) busy = FALSE;
		}
		else if( arg->type == ENT_STRING )
		{
			if( !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") )
				busy = FALSE;
		}

	}

	if( busy ) return;

	// No message.. the LASTRETURN can indicate whether to do that
	target->force_fading = URANGE(1, rating, 100);
    target->fade_dir = door;
    FADE_STATE(target, 4);

	info->progs->lastreturn = door;
}

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
	CHAR_DATA *target;
	int door = -1;
	bool conceal = FALSE, pursue = TRUE;
	char fleedata[MIL];
	char *fleearg = str_empty;

	if(!info) return;
	info->progs->lastreturn = -1;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	target = info->mob;
	switch(arg->type) {
	case ENT_STRING: target = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: target = arg->d.mob; break;
	default: target = NULL; break;
	}

	if (!target) return;

	if (!target->fighting || !target->in_room)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,arg)))
				return;

		if(arg->type == ENT_STRING) {
			if (!str_cmp(arg->d.str, "none")) {
				fleearg = str_empty;
			} else if (!str_cmp(arg->d.str, "anyway")) {
				strcpy(fleedata,"anyway");
				fleearg = fleedata;
			} else if (!str_cmp(arg->d.str, "wimpy"))
				fleearg = NULL;
			else {
				strncpy(fleedata,arg->d.str,sizeof(fleedata)-1);
				fleearg = fleedata;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,arg)))
					return;

				if( arg->type == ENT_NUMBER )
					conceal = (arg->d.num != 0);
				else if( arg->type == ENT_STRING )
					conceal = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "all");
				else
					return;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,arg)))
					return;

				if( arg->type == ENT_NUMBER )
					pursue = (arg->d.num != 0);
				else if( arg->type == ENT_STRING )
					pursue = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "all");
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
	char *rest;

	CHAR_DATA *mob;
	TOKEN_DATA *token = NULL;
	TOKEN_INDEX_DATA *token_index = NULL;
	int rating = 1;
	int sn = -1;
	long flags = SKILL_AUTOMATIC;
	char source = SKILLSRC_SCRIPT;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_STRING ) {
		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;
	} else if( arg->type == ENT_NUMBER ) {
		token_index = get_token_index(arg->d.num);

		if( !token_index ) return;
	}
	else
		return;


	if( *rest ) {

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_NUMBER ) return;
		rating = URANGE(1,arg->d.num,100);

		if( *rest ) {
			bool fPerm = FALSE;

			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_BOOLEAN )
				fPerm = arg->d.boolean;
			else if( arg->type == ENT_NUMBER )
				fPerm = (arg->d.num != 0);
			else if( arg->type == ENT_STRING )
				fPerm = !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "perm");
			else
				return;

			source = fPerm ? SKILLSRC_SCRIPT_PERM : SKILLSRC_SCRIPT;

			if( *rest ) {
				BUFFER *buffer = new_buf();
				expand_string(info,rest,buffer);

				if( buffer->state != BUFFER_SAFE || !*buffer->string ) {
					free_buf(buffer);
					return;
				}

				if (!str_cmp(buf_string(buffer), "none"))
					flags = 0;
				else if ((flags = flag_value(skill_flags, buf_string(buffer))) == NO_FLAG)
					flags = SKILL_AUTOMATIC;

				free_buf(buffer);
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
	char *rest;
	int vnum;
	CHAR_DATA *mob = NULL;

	int type;

	info->progs->lastreturn = 0;

	if(info->mob) type = PRG_MPROG;
	else if(info->obj) type = PRG_OPROG;
	else if(info->room) type = PRG_RPROG;
	else if(info->token) type = PRG_TPROG;
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	// Are they already being prompted
	if(mob->pk_question ||
		mob->remove_question ||
		mob->personal_pk_question ||
		mob->cross_zone_question ||
		mob->pcdata->convert_church != -1 ||
		mob->challenged ||
		mob->remort_question)
		return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: return;
	}

	if(vnum < 1 || !get_script_index(vnum, type)) return;
	BUFFER *buffer = new_buf();

	expand_string(info,rest,buffer);


	pVARIABLE var = variable_get(*(info->var),buf_string(buffer));

	mob->desc->input = TRUE;
	if( var && var->type == VAR_STRING && !IS_NULLSTR(var->_.s) )
		mob->desc->inputString = str_dup(var->_.s);
	else
		mob->desc->inputString = &str_empty[0];
	mob->desc->input_var = str_dup(buf_string(buffer));
	mob->desc->input_prompt = NULL;
	mob->desc->input_script = vnum;
	mob->desc->input_mob = info->mob;
	mob->desc->input_obj = info->obj;
	mob->desc->input_room = info->room;
	mob->desc->input_tok = info->token;

	string_append(mob, &mob->desc->inputString);
	free_buf(buffer);

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

// PAGEAT $PLAYER $STRING
SCRIPT_CMD(scriptcmd_pageat)
{
	char *rest;

	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	BUFFER *buffer = new_buf();

	if( expand_string(info, rest, buffer) )
	{
		// only do it if they actually HAVE scroll enabled
		if( mob->lines > 0)
			page_to_char(buffer->string, mob);
		else
			send_to_char(buffer->string, mob);

		info->progs->lastreturn = 1;
	}
	free_buf(buffer);
}

//////////////////////////////////////
// Q

// QUESTACCEPT $PLAYER[ $SCROLL]
// Finalizes the $PLAYER's pending SCRIPTED quest
//
// $PLAYER - player who is on a quest
// $SCROLL - scroll object to give to player (optional)
//
// Fails if the player does not have a pending SCRIPTED quest.
// Fails if the scroll is defined but does not have WEAR_TAKE set OR is worn.
//
// LASTRETURN will be the resulting countdown timer
SCRIPT_CMD(scriptcmd_questaccept)
{
	char *rest;
	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Must be on a scripted quest that is still generating
	if( !mob->quest->generating || !mob->quest->scripted ) return;

	// Check if there is a SCROLL, if so.. give it to the target
	if( *rest )
	{
		OBJ_DATA *scroll;

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj) )
			return;

		scroll = arg->d.obj;
		if( !CAN_WEAR(scroll, ITEM_TAKE) || scroll->wear_loc != WEAR_NONE )
			return;

		if( scroll->in_room != NULL )
			obj_from_room(scroll);
		else if( scroll->carried_by != NULL )
			obj_from_char(scroll);
		else if( scroll->in_obj != NULL )
			obj_from_obj(scroll);

		obj_to_char(scroll, mob);
	}

	mob->countdown = 0;
	for (QUEST_PART_DATA *qp = mob->quest->parts; qp != NULL; qp = qp->next)
		mob->countdown += qp->minutes;

	mob->quest->generating = FALSE;
	info->progs->lastreturn = mob->countdown;
}

// QUESTCANCEL $PLAYER[ $CLEANUP]
// Cancels the $PLAYER's pending SCRIPTED quest
//
// $PLAYER  - Cancels the pending quest for this player
// $CLEANUP - script (caller space) used to clean up generated quest parts (optional)
//
// Fails if the player does not have a pending scripted quest.
SCRIPT_CMD(scriptcmd_questcancel)
{
	char *rest;
	CHAR_DATA *mob;
	long vnum;
	int type;
	SCRIPT_DATA *script;

	info->progs->lastreturn = 0;

	if(info->mob) type = PRG_MPROG;
	else if(info->obj) type = PRG_OPROG;
	else if(info->room) type = PRG_RPROG;
	else if(info->token) type = PRG_TPROG;
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Must be on a scripted quest that is still generating
	if( !mob->quest->generating || !mob->quest->scripted ) return;

	// Get cleanup script (if there)
	if( *rest )
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		switch(arg->type) {
		case ENT_STRING: vnum = atoi(arg->d.str); break;
		case ENT_NUMBER: vnum = arg->d.num; break;
		default: vnum = 0; break;
		}

		if (vnum < 1 || !(script = get_script_index(vnum, type)))
			return;

		// Don't care about response
		execute_script(script->vnum, script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,0,0,0,0,0);
	}

	free_quest(mob->quest);
	mob->quest = NULL;

	info->progs->lastreturn = 1;
}

// QUESTCOMPLETE $player $partno
SCRIPT_CMD(scriptcmd_questcomplete)
{
	char *rest;
	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;

	if(check_quest_custom_task(mob, arg->d.num))
		info->progs->lastreturn = 1;
}


// QUESTGENERATE $PLAYER $QUESTRECEIVER $PARTCOUNT $PARTSCRIPT

// QUESTRECEIVER cannot be a wilderness room (for now?)
SCRIPT_CMD(scriptcmd_questgenerate)
{
	char *rest;
	CHAR_DATA *mob;
	int qg_type;
	long qg_vnum;
	CHAR_DATA *qr_mob = NULL;
	OBJ_DATA *qr_obj = NULL;
	ROOM_INDEX_DATA *qr_room = NULL;
	int *tempstores;
	int type, parts;
	long vnum;
	SCRIPT_DATA *script;

	info->progs->lastreturn = 0;

	if(info->mob)
	{
		type = PRG_MPROG;
		tempstores = info->mob->tempstore;

		if( !IS_NPC(info->mob) )
			return;

		qg_type = QUESTOR_MOB;
		qg_vnum = info->mob->pIndexData->vnum;
	}
	else if(info->obj)
	{
		type = PRG_OPROG;
		tempstores = info->obj->tempstore;

		qg_type = QUESTOR_OBJ;
		qg_vnum = info->obj->pIndexData->vnum;
	}
	else if(info->room)
	{
		type = PRG_RPROG;
		tempstores = info->room->tempstore;
		if( info->room->wilds || info->room->source )
			return;

		qg_type = QUESTOR_ROOM;
		qg_vnum = info->room->vnum;
	}
	else if(info->token)
	{
		type = PRG_TPROG;
		tempstores = info->token->tempstore;

		// Select the owner
		if( info->token->player )
		{
			if( !IS_NPC(info->token->player) )
				return;

			qg_type = QUESTOR_MOB;
			qg_vnum = info->token->player->pIndexData->vnum;
		}
		else if( info->token->object )
		{
			qg_type = QUESTOR_OBJ;
			qg_vnum = info->token->object->pIndexData->vnum;
		}
		else if( info->token->room )
		{
			if( info->token->room->wilds || info->token->room->source )
				return;

			qg_type = QUESTOR_ROOM;
			qg_vnum = info->token->room->vnum;
		}
		else
			return;
	}
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Get quest receiver
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE )
	{
		if( !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob) )
			return;

		qr_mob = arg->d.mob;
	}
	else if( arg->type == ENT_OBJECT )
	{
		if( !IS_VALID(arg->d.obj) )
			return;

		qr_obj = arg->d.obj;
	}
	else if( arg->type == ENT_ROOM )
	{
		if( arg->d.room == NULL || (arg->d.room->wilds != NULL) || (arg->d.room->source != NULL) )
			return;

		qr_room = arg->d.room;
	}

	if( !qr_mob && !qr_obj && !qr_room )
		return;

	// Get part count
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: parts = atoi(arg->d.str); break;
	case ENT_NUMBER: parts = arg->d.num; break;
	default: parts = 0; break;
	}

	if( parts < 1 )
		return;

	// Get generator script
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(script = get_script_index(vnum, type)))
		return;

	mob->quest = new_quest();
	mob->quest->generating = TRUE;
	mob->quest->scripted = TRUE;
	mob->quest->questgiver_type = qg_type;
	mob->quest->questgiver = qg_vnum;
	if( qr_mob )
	{
		mob->quest->questreceiver_type = QUESTOR_MOB;
		mob->quest->questreceiver = qr_mob->pIndexData->vnum;
	}
	else if( qr_obj )
	{
		mob->quest->questreceiver_type = QUESTOR_OBJ;
		mob->quest->questreceiver = qr_obj->pIndexData->vnum;
	}
	else if( qr_room )
	{
		mob->quest->questreceiver_type = QUESTOR_ROOM;
		mob->quest->questreceiver = qr_room->vnum;
	}

	bool success = TRUE;
	for(int i = 0; i < parts; i++)
	{
		QUEST_PART_DATA *part = new_quest_part();

		part->next = mob->quest->parts;
		mob->quest->parts = part;
		part->index = parts - i;

		tempstores[0] = part->index;

		if( execute_script(script->vnum, script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,0,0,0,0,0) <= 0 )
		{
			success = FALSE;
			break;
		}
	}

	if( success )
	{
		info->progs->lastreturn = 1;
	}
	else
	{
		free_quest(mob->quest);
		mob->quest = NULL;
	}
}

// QUESTPARTCUSTOM $PLAYER $STRING[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartcustom)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(!IS_NULLSTR(arg->d.str))
		return;

	QUEST_PART_DATA *part = ch->quest->parts;
	sprintf(buf, "{xTask {Y%d{x: %s{x.", part->index, arg->d.str);


	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}


	part->description = str_dup(buf);
	part->custom_task = TRUE;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTGETITEM $PLAYER $OBJECT[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgetitem)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj)) return;
	obj = arg->d.obj;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Retrieve {Y%s{x from {Y%s{x in {Y%s{x.",
		part->index,
		obj->short_descr,
		obj->in_room->name,
		obj->in_room->area->name);

	part->description = str_dup(buf);
	free_string(obj->owner);
	obj->owner = str_dup(ch->name);
	part->pObj = obj;
	part->obj = obj->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTGOTO $PLAYER first|second|both|$ROOM[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgoto)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	ROOM_INDEX_DATA *destination;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	destination = NULL;
	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"first")) destination = get_random_room(ch, FIRST_CONTINENT);
		else if(!str_cmp(arg->d.str,"second")) destination = get_random_room(ch, SECOND_CONTINENT);
		else if(!str_cmp(arg->d.str,"both")) destination = get_random_room(ch, BOTH_CONTINENTS);
		break;
	case ENT_ROOM:
		destination = arg->d.room;
		break;
	default: return;
	}

	if(!destination)
		return;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Travel to {Y%s{x in {Y%s{x.",
		part->index,
		destination->name,
		destination->area->name);

	part->description = str_dup(buf);
	part->room = destination->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTRESCUE $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartrescue)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob)) return;
	target = arg->d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Rescue {Y%s{x from {Y%s{x in {Y%s{x.",
		part->index,
		target->short_descr,
		target->in_room->name,
		target->in_room->area->name);

	part->description = str_dup(buf);
	part->mob_rescue = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTSLAY $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartslay)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob)) return;
	target = arg->d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Slay {Y%s{x.  %s was last seen in {Y%s{x.",
		part->index,
		target->short_descr,
		target->sex == SEX_MALE ? "He" :
		target->sex == SEX_FEMALE ? "She" : "It",
		target->in_room->area->name);

	part->description = str_dup(buf);
	part->mob = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

char *__get_questscroll_args(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg,
	char **header, char **footer, int *width, char **prefix, char **suffix)
{
	char *rest;

	*header = NULL;
	*footer = NULL;
	*width = 0;
	*prefix = NULL;
	*suffix = NULL;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING)
		return NULL;

	*header = str_dup(arg->d.str);

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return NULL;

	*footer = str_dup(arg->d.str);

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return NULL;

	if(arg->d.num < 0)
		return NULL;

	*width = arg->d.num;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return NULL;

	*prefix = str_dup(arg->d.str);

	if( *width > 0 )
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return NULL;

		*suffix = str_dup(arg->d.str);
	}

	return rest;
}

// QUESTSCROLL $PLAYER $QUESTGIVER $VNUM $HEADER $FOOTER $WIDTH $PREFIX[ $SUFFIX] $VARIABLENAME
SCRIPT_CMD(scriptcmd_questscroll)
{
	char *header, *footer, *prefix, *suffix;
	int width;
	char questgiver[MSL];
	char *rest;
	CHAR_DATA *ch;
	long vnum;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating || !ch->quest->scripted ) return;

	// Get questreceiver description
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE )
	{
		if( !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob) )
			return;

		strncpy(questgiver, arg->d.mob->short_descr, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_OBJECT )
	{
		if( !IS_VALID(arg->d.obj) )
			return;

		strncpy(questgiver, arg->d.obj->short_descr, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_ROOM )
	{
		if( arg->d.room == NULL || arg->d.room->wilds || arg->d.room->source )
			return;

		strncpy(questgiver, arg->d.room->name, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_STRING )
	{
		if( IS_NULLSTR(arg->d.str) )
			return;

		strncpy(questgiver, arg->d.str, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else
		return;

	// Get scroll vnum
	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return;

	if( arg->d.num < 1 || !get_obj_index(arg->d.num))
		return;

	vnum = arg->d.num;

	rest = __get_questscroll_args(info, rest, arg, &header, &footer, &width, &prefix, &suffix);
	if( rest && *rest )
	{
		rest = expand_argument(info,rest,arg);

		if( rest && arg->type == ENT_STRING )
		{
			OBJ_DATA *scroll = generate_quest_scroll(ch,questgiver,vnum,header,footer,prefix,suffix,width);
			if( scroll != NULL )
			{
				variables_set_object(info->var, arg->d.str, scroll);
				info->progs->lastreturn = 1;
			}
		}
	}

	if( header ) free_string(header);
	if( footer ) free_string(footer);
	if( prefix ) free_string(prefix);
	if( suffix ) free_string(suffix);
}


//////////////////////////////////////
// R

// REVOKESKILL player name
// REVOKESKILL player vnum
SCRIPT_CMD(scriptcmd_revokeskill)
{
	char *rest;
	SKILL_ENTRY *entry;

	CHAR_DATA *mob;
	TOKEN_INDEX_DATA *token_index = NULL;
	int sn = -1;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_STRING ) {
		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;

		entry = skill_entry_findsn(mob->sorted_skills, sn);

	} else if( arg->type == ENT_NUMBER ) {
		token_index = get_token_index(arg->d.num);

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


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStartCombat - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}



	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!expand_argument(info,rest,arg))
			return;

		attacker = victim;
		switch(arg->type) {
		case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: victim = arg->d.mob; break;
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

	CHAR_DATA *mob;
	bool fBoth = FALSE;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;

	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			fBoth = arg->d.boolean;
		else if( arg->type == ENT_NUMBER )
			fBoth = (arg->d.num != 0);
		else if( arg->type == ENT_STRING )
			fBoth = (!str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"true"));
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



