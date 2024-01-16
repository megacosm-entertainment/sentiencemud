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

char *get_affect_name(AFFECT_DATA *paf);

char *get_spell_data_name(SPELL_DATA *spell)
{
	if (!IS_NULLSTR(spell->skill->display))
		return spell->skill->display;

	return spell->skill->name;
}

SPELL_FUNC(spell_armour)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;

	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You are already armoured.\n\r",ch);
		else
			act("$N is already armoured.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = perm ? -1 : 35;
	af.modifier  = 20;
	af.location  = APPLY_AC;
	af.bitvector = 0;
	af.bitvector2 = 0;
	affect_to_char(victim, &af);
	send_to_char("You feel someone protecting you.\n\r", victim);
	if (ch != victim) act("$N is protected by your magic.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	return true;
}

TOUCH_FUNC(touch_armour)
{
	if (is_affected(ch, skill)) {
		send_to_char("You are already armoured.\n\r",ch);
		return false;
	}

	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = 35 * tattoo->condition / 100;
	af.modifier  = 20;
	af.location  = APPLY_AC;
	af.bitvector = 0;
	af.bitvector2 = 0;
	af.slot = tattoo->wear_loc;
	affect_to_char(ch, &af);
	
	send_to_char("You feel someone protecting you.\n\r", ch);
	return true;
}

BRANDISH_FUNC(brandish_armour)
{
	if (is_affected(ch, skill)) {
		send_to_char("You are already armoured.\n\r",ch);
		return false;
	}

	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = 35 * obj->condition / 100;
	af.modifier  = 20;
	af.location  = APPLY_AC;
	af.bitvector = 0;
	af.bitvector2 = 0;
	af.slot = WEAR_NONE;
	affect_to_char(ch, &af);
	
	send_to_char("You feel someone protecting you.\n\r", ch);
	return true;
}

EQUIP_FUNC(equip_armour)
{
	AFFECT_DATA af;

	memset(&af,0,sizeof(af));

	if (is_affected(ch, skill))
		affect_strip(ch, skill);

	af.slot = obj->wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = -1;
	af.modifier  = 20;
	af.location  = APPLY_AC;
	af.bitvector = 0;
	af.bitvector2 = 0;
	affect_to_char(ch, &af);
	send_to_char("You feel someone protecting you.\n\r", ch);
	return true;
}



SPELL_FUNC(spell_cloak_of_guile)
{
	CHAR_DATA *victim;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));
	bool perm = false;

	victim = (CHAR_DATA *) vo;

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (IS_AFFECTED2(victim, AFF2_CLOAK_OF_GUILE) || is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You are already enshrouded.\n\r",ch);
		else
			act("$N is already enshrouded.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = perm ? -1 : (level/9 + 3);
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_CLOAK_OF_GUILE;
	affect_to_char(victim, &af);

	send_to_char("You feel shrouded.\n\r", victim);
	act("$n shimmers with a dark green glow.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return true;
}

SPELL_FUNC(spell_entrap)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;

	if (IS_SET(obj->extra[0], ITEM_HOLY) ||
		obj->item_type == ITEM_ARTIFACT) {
		act("$p is too powerful for you to entrap.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	act("$p vibrates for a second, then stops.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n's $p vibrates for a second, then stops.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	obj->trap_dam = level + dice(get_skill(ch, skill),3);
	SET_BIT(obj->extra[1], ITEM_TRAPPED);
	return true;
}




SPELL_FUNC(spell_faerie_fire)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (IS_AFFECTED(victim, AFF_FAERIE_FIRE))
		return false;

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = level;
	af.location = APPLY_AC;
	af.modifier = -2 * level;
	af.bitvector = AFF_FAERIE_FIRE;
	af.bitvector2 = 0;
	affect_to_char(victim, &af);

	send_to_char("You are surrounded by a pink outline.\n\r", victim);
	act("$n is surrounded by a pink outline.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	return true;
}

void _spell_identify_show_item_data(BUFFER *buffer, CHAR_DATA *ch, OBJ_DATA *obj)
{
	char buf[MSL];
	ITERATOR it;

	if (IS_AMMO(obj))
	{
		// TODO: Finish this
	}

	if (IS_ARMOR(obj))
	{
		ARMOR_DATA *armor = ARMOR(obj);
		sprintf(buf, "{MArmor Type: {W%s{x\n\r", flag_string(armour_types, armor->armor_type));
		add_buf(buffer, buf);
		sprintf(buf, "{MArmor Strength: {W%s{x\n\r", flag_string(armour_strength_table, armor->armor_strength));
		add_buf(buffer, buf);

		// TODO: Indicate what the protections actually do
		for(int i = 0; i < ARMOR_MAX; i++)
		{
			sprintf(buf, "{M{+%s Protection: {W%d{x\n\r", flag_string(armour_protection_types, i), armor->protection[i]);
			add_buf(buffer, buf);
		}

		// Show the adornments
		if (armor->max_adornments && armor->adornments != NULL)
		{
			add_buf(buffer, "{MAdornments:{x\n\r");
			for(int i = 0; i < armor->max_adornments; i++)
			{
				ADORNMENT_DATA *adorn = armor->adornments[i];
				if (IS_VALID(adorn))
				{
					if (adorn->spell != NULL)
						sprintf(buf, "{Y* {MSlot {W%d{M: {+{W%s{M grants level {W%d{M spell {W%s{M when {Wequipped{M.{x\n\r", i+1,
							adorn->short_descr,
							adorn->spell->level,
							adorn->spell->skill->name);
					else
						sprintf(buf, "{Y* {MSlot {W%d{M: {+{W%s{M does nothing.{x\n\r", i+1,
							adorn->short_descr);
				}
				else
					sprintf(buf, "{MSlot {W%d{M: empty\n\r", i+1);
				add_buf(buffer, buf);
			}
		}
	}

	if (IS_BOOK(obj))
	{
		// TODO: Finish this
	}

	if (IS_CONTAINER(obj))
	{
		sprintf(buf,"{MItems: {x%d{M/{x%d{M  Weight: {x%d/%d{M  flags: {x%s{M\n\r",
			get_number_in_container(obj), CONTAINER(obj)->max_volume,
			(get_obj_weight_container(obj) * CONTAINER(obj)->weight_multiplier)/100,
			CONTAINER(obj)->max_weight,
			flag_string(container_flags,CONTAINER(obj)->flags));
		add_buf(buffer,buf);
		if (CONTAINER(obj)->weight_multiplier != 100) {
			sprintf(buf,"{MWeight multiplier: {x%d{M%%\n\r", CONTAINER(obj)->weight_multiplier);
			add_buf(buffer,buf);
		}

		// TODO: Add white/blacklists

		// REMARK: identifying keyrings doesn't need to show the keys.  Can just look at the object itself.
	}

	if (IS_FOOD(obj))
	{
		// Show food buffs
		FOOD_BUFF_DATA *buff;
		iterator_start(&it, FOOD(obj)->buffs);
		while((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
		{
			if (buff->location != APPLY_NONE && buff->modifier)
			{
				sprintf(buf, "{MAffects {x%s {Mby {x%d{M for {x%d {Mhours.{x\n\r", affect_loc_name(buff->location), buff->modifier, (buff->duration > 0 ? buff->duration : FOOD(obj)->full));
				add_buf(buffer,buf);
				if (buff->bitvector || (buff->bitvector2 && buff->where == TO_AFFECTS)) {
					switch(buff->where) {
					case TO_AFFECTS:
						sprintf(buf,"{M  Adds {x%s {Maffect.{x\n", affects_bit_name(buff->bitvector, buff->bitvector2));
						add_buf(buffer,buf);
						break;
					case TO_IMMUNE:
						sprintf(buf,"{M  Adds immunity to {x%s{M.{x\n", imm_bit_name(buff->bitvector));
						add_buf(buffer,buf);
						break;
					case TO_RESIST:
						sprintf(buf,"{M  Adds resistance to {x%s{M.\n\r{x", imm_bit_name(buff->bitvector));
						add_buf(buffer,buf);
						break;
					case TO_VULN:
						sprintf(buf,"{M  Adds vulnerability to {x%s{M.\n\r{x", imm_bit_name(buff->bitvector));
						add_buf(buffer,buf);
						break;
					}
				}
			}
		}
		iterator_stop(&it);
	}

	if (IS_FLUID_CON(obj))
	{
		FLUID_CONTAINER_DATA *fluid = FLUID_CON(obj);
		// Get liquid info
		if (fluid->amount != 0 && IS_VALID(fluid->liquid))
			sprintf(buf, "{MIt holds %s-colored {x%s{M.{x\n\r",
				fluid->liquid->color,
				fluid->liquid->name);
		else
			sprintf(buf,"{MIt is empty.{x\n\r");
		add_buf(buffer,buf);
		

		// Show spells
		if (list_size(FLUID_CON(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wsipped{M, {Wdrank{M or {Wquaffed{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, FLUID_CON(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}

	if (IS_INK(obj))
	{
		// TODO: NYI
		add_buf(buffer, "{MInk: {WNOT YET IMPLEMENTED{x\n\r");
	}

	if (IS_INSTRUMENT(obj))
	{
		// TODO: NYI
		add_buf(buffer, "{MInstrument: {WNOT YET IMPLEMENTED{x\n\r");
	}

	if (IS_JEWELRY(obj))
	{
		// TODO: Need to indicate how much mana the jewelry can hold
		if (list_size(JEWELRY(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wequipped{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, JEWELRY(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}

	if (IS_LIGHT(obj))
	{
		// TODO: NYI
		add_buf(buffer, "{MLight: {WNOT YET IMPLEMENTED{x\n\r");
	}

	if (IS_MIST(obj))
	{
		// TODO: NYI
		add_buf(buffer, "{MMist: {WNOT YET IMPLEMENTED{x\n\r");
	}

	if (IS_SCROLL(obj))
	{
		if (list_size(SCROLL(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wrecited{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, SCROLL(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}

	if (IS_TATTOO(obj))
	{
		if (list_size(TATTOO(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wtouched{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, TATTOO(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}

	if (IS_WAND(obj))
	{
		// TODO: NYI
		add_buf(buffer, "{MWand: {WNOT YET COMPLETED{x\n\r");

		if (list_size(WAND(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wzapped{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, WAND(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}

	if (IS_WEAPON(obj))
	{
		add_buf(buffer, "{MWeapon type is {x");


		// Now... why is this like this?
		switch (obj->value[0]) {
		case(WEAPON_EXOTIC): 		add_buf(buffer, "exotic{M");		break;
		case(WEAPON_SWORD): 		add_buf(buffer, "sword{M");		break;
		case(WEAPON_DAGGER): 		add_buf(buffer, "dagger{M");		break;
		case(WEAPON_SPEAR): 		add_buf(buffer, "spear/staff{M");	break;
		case(WEAPON_MACE): 		add_buf(buffer, "mace/club{M");	break;
		case(WEAPON_AXE): 		add_buf(buffer, "axe{M");		break;
		case(WEAPON_FLAIL): 		add_buf(buffer, "flail{M");		break;
		case(WEAPON_WHIP): 		add_buf(buffer, "whip{M");		break;
		case(WEAPON_POLEARM): 		add_buf(buffer, "polearm{M");		break;
		case(WEAPON_STAKE): 		add_buf(buffer, "stake{M");		break;
		case(WEAPON_QUARTERSTAFF):	add_buf(buffer, "quarterstaff{M");	break;
		case(WEAPON_HARPOON):		add_buf(buffer, "harpoon{M"); 		break;
		default:			add_buf(buffer, "unknown{M");		break;
		}

		sprintf(buf, " with attack type {x%s{M.{x\n\r", attack_table[obj->value[3]].noun);
		add_buf(buffer, buf);

		sprintf(buf,"{MDamage is {x%dd%d {M(average {Y%d{M).\n\r{x",
			obj->value[1],obj->value[2], (1 + obj->value[2]) * obj->value[1] / 2);
		add_buf(buffer, buf);
		if (obj->value[4]) {
			sprintf(buf,"{MWeapons flags: {x%s\n\r",weapon_bit_name(obj->value[4]));
			add_buf(buffer,buf);
		}

		if (list_size(WEAPON(obj)->spells) > 0)
		{
			add_buf(buffer, "{MSpells when {Wbrandished{x:\n\r");
			ITERATOR it;
			SPELL_DATA *spell;
			iterator_start(&it, WEAPON(obj)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, " {W* {MLevel {W%d {Mspell of {W%s{M.{x\n\r",
					spell->level, get_spell_data_name(spell));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);
		}
	}
}

bool __func_identify(SKILL_DATA *skill, int level, CHAR_DATA *ch, OBJ_DATA *obj)
{
	BUFFER *buffer;
	char buf[2*MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH];
	//char extra_flags[MSL];
	AFFECT_DATA *af;
//	OBJ_DATA *key;
//	int i = 0;
//	SPELL_DATA *spell;

	if (IS_SET(obj->extra[1], ITEM_NO_LORE)) {
		act("$p is beyond your power to identify.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	buffer = new_buf();

	sprintf(buf,
		"{MObject '{x%s{M' is type {x%s{M, extra flags {x%s{M.\n\r"
		"Weight is {x%d{M, value is {x%ld{M, level is {x%d{M.\n\r"
		"{MFragility is {x%s{M, condition is {x%d%%{M. It has been repaired {x%d{M/{x%d {Mtimes.{x\n\r" ,
		obj->name,
		item_name(obj->item_type),
		bitmatrix_string(extra_flagbank, obj->extra),
		obj->weight,
		obj->cost,
		obj->level,
		fragile_table[obj->fragility].name,
		obj->condition,
		obj->times_fixed,
		obj->times_allowed_fixed);

	add_buf(buffer, buf);

	sprintf(buf, "{MIt is made out of {x%s{M.\n\r", IS_VALID(obj->material)?obj->material->name:"nothing");
	add_buf(buffer, buf);

	// Sages know where items come from, if from the mortal world
	if (IS_SAGE(ch) || IS_IMMORTAL(ch)) {
		AREA_DATA *pArea;

		pArea = obj->pIndexData->area;
		// TODO: Change how the area is defined so that it can be labeled as being in the mortal world.
		if ((pArea->region.place_flags == PLACE_FIRST_CONTINENT) ||
			(pArea->region.place_flags == PLACE_SECOND_CONTINENT) ||
			(pArea->region.place_flags == PLACE_THIRD_CONTINENT) ||
			(pArea->region.place_flags == PLACE_FOURTH_CONTINENT) ||
			(pArea->region.place_flags == PLACE_ISLAND) ||
			!str_cmp(pArea->name, "Undersea")) {
			sprintf(buf, "{MThis item comes from {x%s{M.{x\n\r", pArea->name);
			add_buf(buffer, buf);
		} else
			add_buf(buffer, "{MThis item is not of the mortal world.{x\n\r");
	}

//	switch (obj->item_type) {
	/*
	case ITEM_RANGED_WEAPON:
		add_buf(buffer, "{MRanged weapon type is {x");

		switch (obj->value[0]) {
		case(RANGED_WEAPON_EXOTIC): 	add_buf(buffer, "exotic{M.{x\n\r");	break;
		case(RANGED_WEAPON_BOW):	add_buf(buffer, "bow{M.{x\n\r");	break;
		case(RANGED_WEAPON_CROSSBOW): 	add_buf(buffer, "crossbow{M.{x\n\r");	break;
		case(RANGED_WEAPON_HARPOON): 	add_buf(buffer, "harpoon{M.{x\n\r");	break;
		case(RANGED_WEAPON_BLOWGUN): 	add_buf(buffer, "blowgun{M.{x\n\r");	break;
		default: 			add_buf(buffer, "unknown{M.{x\n\r");	break;
		}

		sprintf(buf,"{MDamage is {x%dd%d {M(average {Y%d{M).\n\r{x",
			obj->value[1],obj->value[2],
			(1 + obj->value[2]) * obj->value[1] / 2);
		add_buf(buffer, buf);

		sprintf(buf, "{MRange is {x%d{M rooms.\n\r", obj->value[3]);
		add_buf(buffer, buf);
		break;
	*/

	// TODO: update for display the keys when the container whitelists keys
	/*
	case ITEM_KEYRING:
		sprintf(buf, "{MContains the following keys with a total weight of {x%d{M:{x\n\r", get_obj_weight_container(obj));
		add_buf(buffer, buf);
		if (!obj->contains) {
			add_buf(buffer, "{xNo keys.\n\r");
			break;
		}

		for (key = obj->contains; key; key = key->next_content) {
			i++;

			sprintf(buf, "{M[{x%3d{M]:{x %s\n\r", i, key->short_descr);
			add_buf(buffer, buf);
		}
		break;
		*/

	/*
	case ITEM_FLUID_CONTAINER:
		if (obj->value[5] > 0) {
			sprintf(buf, "{MThis potion has {x%d{M remaining charge%s.{x\n\r",
				obj->value[5], obj->value[5] == 1 ? "" : "s");
			add_buf(buffer, buf);
		}
		break;
	*/

	/*
	case ITEM_TATTOO:
		if (obj->value[0] > 0) {
			sprintf(buf, "{MThis tattoo has {x%d{M remaining charge%s.{x\n\r",
				obj->value[0], obj->value[0] == 1 ? "" : "s");
			add_buf(buffer, buf);
		}

		if ((IS_SAGE(ch) || IS_IMMORTAL(ch)) && obj->value[1] > 0) {
			sprintf(buf, "{MThere is a %d%% chance that the tattoo will fade with each touch{x\n\r", obj->value[1]);
			add_buf(buffer, buf);
		}
		break;

	case ITEM_INK:
		if (obj->value[0] > 0) {
			sprintf(buf, "{MThis ink has {x%s{M essence.{x\n\r", catalyst_descs[obj->value[0]]);
			add_buf(buffer, buf);
		}
		if (obj->value[1] > 0) {
			sprintf(buf, "{MThis ink has {x%s{M essence.{x\n\r", catalyst_descs[obj->value[1]]);
			add_buf(buffer, buf);
		}
		if (obj->value[2] > 0) {
			sprintf(buf, "{MThis ink has {x%s{M essence.{x\n\r", catalyst_descs[obj->value[2]]);
			add_buf(buffer, buf);
		}
		break;
	*/

/*
	case ITEM_WAND:
	case ITEM_STAFF:
		sprintf(buf, "{MHas {x%d{M/{x%d {Mcharges.{x\n\r",
			obj->value[2], obj->value[1]);
		add_buf(buffer, buf);
		break;
*/
	/*
	case ITEM_DRINK_CON:
		if (obj->value[0] != 0 && obj->value[1] != 0)
			sprintf(buf,"{MIt holds %s-coloured {x%s{M.\n\r{x",
				liq_table[obj->value[2]].liq_colour,
				liq_table[obj->value[2]].liq_name);
		else
			sprintf(buf,"{MIt is empty.{x\n\r");
		add_buf(buffer,buf);
		break;
	*/

	/*
	case ITEM_WEAPON_CONTAINER:
		sprintf(buf,"{MHolds {x%d{M/{x%d {M%ss and {x%d{M/{x%d{M weight\n\r",
			get_number_in_container(obj), obj->value[3],
			weapon_name(obj->value[1]),
			get_obj_weight_container(obj),
			obj->value[0]);
		add_buf(buffer,buf);
		if (obj->value[3] != 100) {
			sprintf(buf,"{MWeight multiplier: {x%d{M%%\n\r", obj->value[4]);
			add_buf(buffer,buf);
		}
		break;
	*/

	/*
	case ITEM_CONTAINER:
		sprintf(buf,"{MItems: {x%d{M/{x%d{M  Weight: {x%d/%d{M  flags: {x%s{M\n\r",
			get_number_in_container(obj), obj->value[3],
			(get_obj_weight_container(obj) * WEIGHT_MULT(obj))/100,
			obj->value[0], cont_bit_name(obj->value[1]));
		add_buf(buffer,buf);
		if (obj->value[4] != 100) {
			sprintf(buf,"{MWeight multiplier: {x%d{M%%\n\r", obj->value[4]);
			add_buf(buffer,buf);
		}
		break;
	*/

	/*
	case ITEM_WEAPON:
		add_buf(buffer, "{MWeapon type is {x");


		// Now... why is this like this?
		switch (obj->value[0]) {
		case(WEAPON_EXOTIC): 		add_buf(buffer, "exotic{M");		break;
		case(WEAPON_SWORD): 		add_buf(buffer, "sword{M");		break;
		case(WEAPON_DAGGER): 		add_buf(buffer, "dagger{M");		break;
		case(WEAPON_SPEAR): 		add_buf(buffer, "spear/staff{M");	break;
		case(WEAPON_MACE): 		add_buf(buffer, "mace/club{M");	break;
		case(WEAPON_AXE): 		add_buf(buffer, "axe{M");		break;
		case(WEAPON_FLAIL): 		add_buf(buffer, "flail{M");		break;
		case(WEAPON_WHIP): 		add_buf(buffer, "whip{M");		break;
		case(WEAPON_POLEARM): 		add_buf(buffer, "polearm{M");		break;
		case(WEAPON_STAKE): 		add_buf(buffer, "stake{M");		break;
		case(WEAPON_QUARTERSTAFF):	add_buf(buffer, "quarterstaff{M");	break;
		case(WEAPON_HARPOON):		add_buf(buffer, "harpoon{M"); 		break;
		default:			add_buf(buffer, "unknown{M");		break;
		}

		sprintf(buf, " with attack type {x%s{M.{x\n\r", attack_table[obj->value[3]].noun);
		add_buf(buffer, buf);

		sprintf(buf,"{MDamage is {x%dd%d {M(average {Y%d{M).\n\r{x",
			obj->value[1],obj->value[2], (1 + obj->value[2]) * obj->value[1] / 2);
		add_buf(buffer, buf);
		if (obj->value[4]) {
			sprintf(buf,"{MWeapons flags: {x%s\n\r",weapon_bit_name(obj->value[4]));
			add_buf(buffer,buf);
		}
		break;
	*/

	/*
	case ITEM_ARMOUR:
		sprintf(buf, "{MArmour class is {x%d {Mpierce, {x%d {Mbash, {x%d {Mslash, and {x%d {Mvs. magic.\n\r",
			obj->value[0], obj->value[1], obj->value[2], obj->value[3]);
		add_buf(buffer, buf);
		break;
	}
	*/

	_spell_identify_show_item_data(buffer, ch, obj);

	for (af = obj->affected; af; af = af->next) {
		if (af->location != APPLY_NONE && af->modifier && !af->custom_name && !IS_VALID(af->skill)) {
			sprintf(buf, "{MAffects {x%s {Mby {x%d", affect_loc_name(af->location), af->modifier);
			add_buf(buffer, buf);
			if (af->duration > -1)
				sprintf(buf,"{M, {x%d {Mhours.{x\n\r",af->duration);
			else
				sprintf(buf,"{M.\n\r{x");
			add_buf(buffer,buf);
			if (af->bitvector) {
				switch(af->where) {
				case TO_AFFECTS:
					sprintf(buf,"{MAdds {x%s {Maffect.{x\n", affects_bit_name(af->bitvector, af->bitvector2));
					break;
				case TO_OBJECT:
					sprintf(buf,"{MAdds {x%s {Mobject flag.{x\n", extra_bit_name(af->bitvector));
					break;
				case TO_OBJECT2:
					sprintf(buf,"{MAdds {x%s {Mobject flag.{x\n", extra2_bit_name(af->bitvector));
					break;
				case TO_OBJECT3:
					sprintf(buf,"{MAdds {x%s {Mobject flag.{x\n", extra3_bit_name(af->bitvector));
					break;
				case TO_OBJECT4:
					sprintf(buf,"{MAdds {x%s {Mobject flag.{x\n", extra4_bit_name(af->bitvector));
					break;
				// TODO: Fix for the N attack points
				case TO_WEAPON:
					sprintf(buf,"{MAdds {x%s {Mweapon flags.\n{x", weapon_bit_name(af->bitvector));
					break;
				case TO_IMMUNE:
					sprintf(buf,"{MAdds immunity to {x%s{M.{x\n", imm_bit_name(af->bitvector));
					break;
				case TO_RESIST:
					sprintf(buf,"{MAdds resistance to {x%s{M.\n\r{x", imm_bit_name(af->bitvector));
					break;
				case TO_VULN:
					sprintf(buf,"{MAdds vulnerability to {x%s{M.\n\r{x", imm_bit_name(af->bitvector));
					break;
				default:
					sprintf(buf,"{MUnknown bit {x%d{M: {x%ld\n\r", af->where,af->bitvector);
					break;
				}
				add_buf(buffer,buf);
			}
		}
	}

	/*
	for (spell = obj->spells; spell; spell = spell->next) {
		sprintf(buf, "{MLevel {W%d {Mspell of {W%s{M.{x\n\r",
			spell->level, get_spell_data_name(spell));
		add_buf(buffer, buf);
	}
	*/

	for (af = obj->catalyst; af != NULL; af = af->next) {
		sprintf(buf, "%satalyst {x%s {M", ((af->where == TO_CATALYST_ACTIVE) ? "{MC" : "{xDormant{M c" ), flag_string( catalyst_types, af->catalyst_type ));
		add_buf(buffer, buf);
		if (af->duration > -1)
			sprintf(buf,"with {x%d%%{M left.{x\n\r",100 * af->duration / af->modifier );
		else
			sprintf(buf,"with an infinite source.{x\n\r");
		add_buf(buffer,buf);
	}
	
	if (obj->old_name) {
		sprintf(buf, "{MOriginal name: %s{x\n\r", obj->old_name);
		add_buf(buffer, buf);
	}
	if (obj->old_short_descr) {
		sprintf(buf, "{MOriginal short desc: %s{x\n\r", obj->old_short_descr);
		add_buf(buffer, buf);
	}
	if (obj->old_description) {
		sprintf(buf, "{MOriginal long desc: %s{x\n\r", obj->old_description);
		add_buf(buffer, buf);
	}

	// Show spells like bless, etc, which have been casted on obj
	if (obj->affected) {
		for (af = obj->affected; af; af = af->next) {
			if (IS_VALID(af->skill)) {
				buf2[0] = '\0';

				if (af->location != APPLY_NONE && str_cmp(affect_loc_name(af->location), "(unknown)")) {
					sprintf(buf2, "which affects {x%s{M by {x%d{M ",
						affect_loc_name(af->location), af->modifier);
				}

				sprintf(buf, "{MAffected by {x%s{M, level {x%d{M, %sfor {x%d{M hours.{x\n\r",
					af->skill->name, af->level,
					buf2[0] == '\0' ? "" : buf2, af->duration);
				add_buf(buffer, buf);
			}
		}
	}

	// TODO: Make it where these triggers can append to this
	// ch->script_buffer = buffer;
	if(skill == &gsk__auction || skill == &gsk__inspect)
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_INSPECT, NULL,0,0,0,0,0);
	else if(skill == gsk_lore)
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE, NULL,0,0,0,0,0);
	else if(skill == gsk_identify)
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_IDENTIFY, NULL,0,0,0,0,0);

	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
	return true;
}

SPELL_FUNC(spell_identify)
{
	return __func_identify(skill, level, ch, (OBJ_DATA *)vo);
}

RECITE_FUNC(recite_identify)
{
	return __func_identify(skill, level, ch, (OBJ_DATA *)vo);
}

SPELL_FUNC(spell_locate_object)
{
	char buf[MAX_INPUT_LENGTH];
	char *target_name = (char *) vo;
	BUFFER *buffer;
	OBJ_DATA *obj;
	OBJ_DATA *in_obj;
	bool found;
	int number = 0, max_found;
	ITERATOR it;

	if (!target_name)
		return false;

	found = false;
	number = 0;
	max_found = IS_IMMORTAL(ch) ? 200 : 2 * level;

	buffer = new_buf();

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it))) {
		if (!can_see_obj(ch, obj) ||
			!is_name(target_name, obj->name) ||
			IS_SET(obj->extra[0], ITEM_NOLOCATE) ||
			number_percent() > 2 * level ||
			ch->tot_level < obj->level)
			continue;

		found = true;
		number++;

		for (in_obj = obj; in_obj->in_obj; in_obj = in_obj->in_obj);

		if (in_obj->carried_by && can_see(ch,in_obj->carried_by)) {
			sprintf(buf, "one is carried by %s\n\r",
			pers(in_obj->carried_by, ch));
		} else {
			char buf2[MSL];

			if (IS_IMMORTAL(ch) && in_obj->in_room)
				sprintf(buf, "one is in %s [Room %ld] ",
					in_obj->in_room->name, in_obj->in_room->vnum);
			else
				sprintf(buf, "one is in %s ", !in_obj->in_room ? "somewhere" : in_obj->in_room->name);

			if (obj->in_room && !str_cmp(obj->in_room->area->name,"Wilderness")) {
				sprintf(buf2, "[{YX: {x%ld {YY: {x%ld]\n\r", obj->in_room->x, obj->in_room->y);
				strcat(buf, buf2);
			} else {
				strcat(buf, "\n\r");
			}
		}

		buf[0] = UPPER(buf[0]);
		add_buf(buffer,buf);

		if (number >= max_found) break;
	}
	iterator_stop(&it);

	if (!found)
		send_to_char("Nothing like that in heaven or earth.\n\r", ch);
	else
		page_to_char(buf_string(buffer),ch);

	free_buf(buffer);
	return true;
}


SPELL_FUNC(spell_pass_door)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (IS_AFFECTED(victim, AFF_PASS_DOOR)) {
		if (victim == ch)
			send_to_char("You are already out of phase.\n\r",ch);
		else
			act("$N is already shifted out of phase.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : number_fuzzy(level / 4);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = AFF_PASS_DOOR;
	af.bitvector2 = 0;
	affect_to_char(victim, &af);

	act("$n turns translucent.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("You turn translucent.\n\r", victim);
	return true;
}


SPELL_FUNC(spell_room_shield)
{
	OBJ_DATA *roomshield;
	OBJ_DATA *obj;
	int catalyst;
	bool outside = false;

	catalyst = has_catalyst(ch,NULL,CATALYST_LAW,CATALYST_INVENTORY|CATALYST_ACTIVE);

	if(IS_OUTSIDE(ch) || IS_WILDERNESS(ch->in_room)) {
		if( catalyst >= 0 ) {
			if ( catalyst < 10 ) {
				send_to_char("You may not cast this spell outdoors.\n\r", ch);
				return false;
			} else {
				catalyst -= 10;
				outside = true;
			}
		}
	}

	for (obj = ch->in_room->contents; obj; obj = obj->next_content)
		if (obj->item_type == ITEM_ROOM_ROOMSHIELD) {
			send_to_char("A room shield has already been set up here.\n\r", ch);
			return false;
		}

	if( catalyst > 0 )
	{
		int cost;

		if( catalyst > 10 )
			catalyst = 10;

		cost = outside ? (catalyst + 10) : catalyst;	// Being outdoors weakens the use of the catalyst, but is part of the cost

		use_catalyst(ch,NULL,CATALYST_ASTRAL,CATALYST_INVENTORY|CATALYST_ACTIVE,cost,true);
	}
	else if(catalyst < 0)
		catalyst = 10;

	roomshield = create_object(obj_index_roomshield, 0, true);
	roomshield->timer = 3 + ((3 * catalyst * catalyst + 1) / 4);
	roomshield->level = ch->tot_level;
	roomshield->owner = str_dup(ch->name);

	obj_to_room(roomshield, ch->in_room);
	act("{YAn orb of energy forms in $n's hands and $e casts it down.{X",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YThe fizzling energy orb quickly expands to fill the entire room!{X",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YAn orb of energy forms in your hands and you cast it at the ground before you.{X",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{YThe fizzling energy orb quickly expands to fill the entire room!{X",   ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return true;
}




SPELL_FUNC(spell_word_of_recall)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	ROOM_INDEX_DATA *location;

	if (IS_NPC(victim))
		return false;

	if(p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL, NULL,0,0,0,0,0) ||
		p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL, NULL,0,0,0,0,0))
		return false;

	location = get_recall_room(ch);

	if (location == NULL) {
		send_to_char("You are completely lost.\n\r",victim);
		return false;
	}

	//Added area_no_recall check to go with corresponding area flag - Areo 08-10-2006
	if (IS_SET(victim->in_room->room_flag[0],ROOM_NO_RECALL) || IS_AFFECTED(victim,AFF_CURSE) || IS_SET(victim->in_room->area->area_flags, AREA_NO_RECALL)) {
		send_to_char("Your attempt to recall has failed.\n\r",victim);
		return false;
	}

	if (ch->no_recall > 0) {
		send_to_char("You can't summon enough energy.\n\r", ch);
		return false;
	}

	if (victim->fighting) {
		send_to_char("The gods look down and grin with interest. Finish the fight!\n\r", victim);
		return false;
	}

	victim->move /= 2;
	if(victim != ch) ch->move /= 2;
	act("{W$n disappears.{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	char_from_room(victim);
	char_to_room(victim,location);
	act("{W$n appears in the room.{x",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
	do_function(victim, &do_look, "auto");
	return true;
}

RECITE_FUNC( recite_word_of_recall )
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	ROOM_INDEX_DATA *location;

	if (IS_NPC(victim))
		return false;

	if(p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL, NULL,0,0,0,0,0) ||
		p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL, NULL,0,0,0,0,0))
		return false;

	location = get_recall_room(ch);

	if (location == NULL) {
		send_to_char("You are completely lost.\n\r",victim);
		return false;
	}

	//Added area_no_recall check to go with corresponding area flag - Areo 08-10-2006
	if (IS_SET(victim->in_room->room_flag[0],ROOM_NO_RECALL) || IS_AFFECTED(victim,AFF_CURSE) || IS_SET(victim->in_room->area->area_flags, AREA_NO_RECALL)) {
		send_to_char("Your attempt to recall has failed.\n\r",victim);
		return false;
	}

	if (ch->no_recall > 0) {
		send_to_char("You can't summon enough energy.\n\r", ch);
		return false;
	}

	if (victim->fighting) {
		send_to_char("The gods look down and grin with interest. Finish the fight!\n\r", victim);
		return false;
	}

	victim->move /= 2;
	act("{W$n disappears.{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	char_from_room(victim);
	char_to_room(victim,location);
	act("{W$n appears in the room.{x",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
	do_function(victim, &do_look, "auto");
	return true;
}
