/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,	   *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *									   *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael	   *
 *  Chastain, Michael Quan, and Mitchell Tse.				   *
 *									   *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc	   *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.						   *
 *									   *
 *  Much time and thought has gone into this software and you are	   *
 *  benefitting.  We hope that you share your changes too.  What goes	   *
 *  around, comes around.						   *
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
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "db.h"
#include "olc.h"
#include "scripts.h"

#define MAX_SKILL_LEARNABLE	75
#define MAX_SKILL_TRAINABLE	90

void list_skill_entries(CHAR_DATA *ch, char *argument, bool show_skills, bool show_spells, bool hide_learned);

SPELL_FUN *prespell_func_lookup(char *name)
{
	for (int i = 0; prespell_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, prespell_func_table[i].name))
			return prespell_func_table[i].func;
	}

	return NULL;
}

char *prespell_func_name(SPELL_FUN *func)
{
	for (int i = 0; prespell_func_table[i].name != NULL; i++)
	{
		if (prespell_func_table[i].func == func)
			return prespell_func_table[i].name;
	}

	return NULL;
}

char *prespell_func_display(SPELL_FUN *func)
{
	if (!func || func == spell_null) return NULL;

	for (int i = 0; prespell_func_table[i].name != NULL; i++)
	{
		if (prespell_func_table[i].func == func)
			return prespell_func_table[i].name;
	}

	return "(invalid)";
}

SPELL_FUN *spell_func_lookup(char *name)
{
	for (int i = 0; spell_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, spell_func_table[i].name))
			return spell_func_table[i].func;
	}

	return NULL;
}

char *spell_func_name(SPELL_FUN *func)
{
	for (int i = 0; spell_func_table[i].name != NULL; i++)
	{
		if (spell_func_table[i].func == func)
			return spell_func_table[i].name;
	}

	return NULL;
}

char *spell_func_display(SPELL_FUN *func)
{
	if (!func || func == spell_null) return NULL;

	for (int i = 0; spell_func_table[i].name != NULL; i++)
	{
		if (spell_func_table[i].func == func)
			return spell_func_table[i].name;
	}

	return "(invalid)";
}


PREBREW_FUN *prebrew_func_lookup(char *name)
{
	for (int i = 0; prebrew_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, prebrew_func_table[i].name))
			return prebrew_func_table[i].func;
	}

	return NULL;
}

char *prebrew_func_name(PREBREW_FUN *func)
{
	for (int i = 0; prebrew_func_table[i].name != NULL; i++)
	{
		if (prebrew_func_table[i].func == func)
			return prebrew_func_table[i].name;
	}

	return NULL;
}

char *prebrew_func_display(PREBREW_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; prebrew_func_table[i].name != NULL; i++)
	{
		if (prebrew_func_table[i].func == func)
			return prebrew_func_table[i].name;
	}

	return "(invalid)";
}

BREW_FUN *brew_func_lookup(char *name)
{
	for (int i = 0; brew_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, brew_func_table[i].name))
			return brew_func_table[i].func;
	}

	return NULL;
}



char *brew_func_name(BREW_FUN *func)
{
	for (int i = 0; brew_func_table[i].name != NULL; i++)
	{
		if (brew_func_table[i].func == func)
			return brew_func_table[i].name;
	}

	return NULL;
}

char *brew_func_display(BREW_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; brew_func_table[i].name != NULL; i++)
	{
		if (brew_func_table[i].func == func)
			return brew_func_table[i].name;
	}

	return "(invalid)";
}

QUAFF_FUN *quaff_func_lookup(char *name)
{
	for (int i = 0; quaff_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, quaff_func_table[i].name))
			return quaff_func_table[i].func;
	}

	return NULL;
}

char *quaff_func_name(QUAFF_FUN *func)
{
	for (int i = 0; quaff_func_table[i].name != NULL; i++)
	{
		if (quaff_func_table[i].func == func)
			return quaff_func_table[i].name;
	}

	return NULL;
}

char *quaff_func_display(QUAFF_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; quaff_func_table[i].name != NULL; i++)
	{
		if (quaff_func_table[i].func == func)
			return quaff_func_table[i].name;
	}

	return "(invalid)";
}


PRESCRIBE_FUN *prescribe_func_lookup(char *name)
{
	for (int i = 0; prescribe_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, prescribe_func_table[i].name))
			return prescribe_func_table[i].func;
	}

	return NULL;
}

char *prescribe_func_name(PRESCRIBE_FUN *func)
{
	for (int i = 0; prescribe_func_table[i].name != NULL; i++)
	{
		if (prescribe_func_table[i].func == func)
			return prescribe_func_table[i].name;
	}

	return NULL;
}

char *prescribe_func_display(PRESCRIBE_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; prescribe_func_table[i].name != NULL; i++)
	{
		if (prescribe_func_table[i].func == func)
			return prescribe_func_table[i].name;
	}

	return "(invalid)";
}


SCRIBE_FUN *scribe_func_lookup(char *name)
{
	for (int i = 0; scribe_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, scribe_func_table[i].name))
			return scribe_func_table[i].func;
	}

	return NULL;
}

char *scribe_func_name(SCRIBE_FUN *func)
{
	for (int i = 0; scribe_func_table[i].name != NULL; i++)
	{
		if (scribe_func_table[i].func == func)
			return scribe_func_table[i].name;
	}

	return NULL;
}

char *scribe_func_display(SCRIBE_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; scribe_func_table[i].name != NULL; i++)
	{
		if (scribe_func_table[i].func == func)
			return scribe_func_table[i].name;
	}

	return "(invalid)";
}


RECITE_FUN *recite_func_lookup(char *name)
{
	for (int i = 0; recite_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, recite_func_table[i].name))
			return recite_func_table[i].func;
	}

	return NULL;
}

char *recite_func_name(RECITE_FUN *func)
{
	for (int i = 0; recite_func_table[i].name != NULL; i++)
	{
		if (recite_func_table[i].func == func)
			return recite_func_table[i].name;
	}

	return NULL;
}

char *recite_func_display(RECITE_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; recite_func_table[i].name != NULL; i++)
	{
		if (recite_func_table[i].func == func)
			return recite_func_table[i].name;
	}

	return "(invalid)";
}


PREINK_FUN *preink_func_lookup(char *name)
{
	for (int i = 0; preink_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, preink_func_table[i].name))
			return preink_func_table[i].func;
	}

	return NULL;
}

char *preink_func_name(PREINK_FUN *func)
{
	for (int i = 0; preink_func_table[i].name != NULL; i++)
	{
		if (preink_func_table[i].func == func)
			return preink_func_table[i].name;
	}

	return NULL;
}

char *preink_func_display(PREINK_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; preink_func_table[i].name != NULL; i++)
	{
		if (preink_func_table[i].func == func)
			return preink_func_table[i].name;
	}

	return "(invalid)";
}

INK_FUN *ink_func_lookup(char *name)
{
	for (int i = 0; ink_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, ink_func_table[i].name))
			return ink_func_table[i].func;
	}

	return NULL;
}

char *ink_func_name(INK_FUN *func)
{
	for (int i = 0; ink_func_table[i].name != NULL; i++)
	{
		if (ink_func_table[i].func == func)
			return ink_func_table[i].name;
	}

	return NULL;
}

char *ink_func_display(INK_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; ink_func_table[i].name != NULL; i++)
	{
		if (ink_func_table[i].func == func)
			return ink_func_table[i].name;
	}

	return "(invalid)";
}

TOUCH_FUN *touch_func_lookup(char *name)
{
	for (int i = 0; touch_func_table[i].name != NULL; i++)
	{
		if (!str_cmp(name, touch_func_table[i].name))
			return touch_func_table[i].func;
	}

	return NULL;
}

char *touch_func_name(TOUCH_FUN *func)
{
	for (int i = 0; touch_func_table[i].name != NULL; i++)
	{
		if (touch_func_table[i].func == func)
			return touch_func_table[i].name;
	}

	return NULL;
}

char *touch_func_display(TOUCH_FUN *func)
{
	if (!func) return NULL;

	for (int i = 0; touch_func_table[i].name != NULL; i++)
	{
		if (touch_func_table[i].func == func)
			return touch_func_table[i].name;
	}

	return "(invalid)";
}


sh_int *gsn_from_name(char *name)
{
	for(int i = 0; gsn_table[i].name; i++)
	{
		if(!str_cmp(gsn_table[i].name, name))
		{
			return gsn_table[i].gsn;
		}
	}

	return NULL;
}

SKILL_DATA **gsk_from_name(char *name)
{
	for(int i = 0; gsn_table[i].name; i++)
	{
		if(!str_cmp(gsn_table[i].name, name))
		{
			return gsn_table[i].gsk;
		}
	}

	return NULL;
}

char *gsn_to_name(sh_int *pgsn)
{
	for(int i = 0; gsn_table[i].name; i++)
	{
		if (gsn_table[i].gsn == pgsn)
			return gsn_table[i].name;
	}

	return NULL;
}

void save_skill(FILE *fp, SKILL_DATA *skill)
{
	fprintf(fp, "#%s %s~ %d\n", (skill->isspell?"SPELL":"SKILL"), skill->name, skill->uid);

	if (!IS_NULLSTR(skill->display) && str_cmp(skill->name, skill->display))
		fprintf(fp, "Display %s~\n", skill->display);

	fprintf(fp, "Level");
	for(int j = 0; j < MAX_CLASS; j++) fprintf(fp, " %d", skill->skill_level[j]);
	fprintf(fp, "\n");

	fprintf(fp, "Rating");
	for(int j = 0; j < MAX_CLASS; j++) fprintf(fp, " %d", skill->rating[j]);
	fprintf(fp, "\n");

	if (skill->token)
	{
		// Tokens use triggers instead of the special functions
		fprintf(fp, "Token %ld#%ld\n", skill->token->area->uid, skill->token->vnum);
	}
	else
	{
		if(skill->prespell_fun && skill->prespell_fun != spell_null)
			fprintf(fp, "PreSpellFunc %s~\n", prespell_func_name(skill->prespell_fun));

		if(skill->spell_fun && skill->spell_fun != spell_null)
			fprintf(fp, "SpellFunc %s~\n", spell_func_name(skill->spell_fun));

		if(skill->prebrew_fun)
			fprintf(fp, "PreBrewFunc %s~\n", prebrew_func_name(skill->prebrew_fun));

		if(skill->brew_fun)
			fprintf(fp, "BrewFunc %s~\n", brew_func_name(skill->brew_fun));

		if(skill->quaff_fun)
			fprintf(fp, "QuaffFunc %s~\n", quaff_func_name(skill->quaff_fun));

		if(skill->prescribe_fun)
			fprintf(fp, "PrescribeFunc %s~\n", prescribe_func_name(skill->prescribe_fun));

		if(skill->scribe_fun)
			fprintf(fp, "ScribeFunc %s~\n", scribe_func_name(skill->scribe_fun));

		if(skill->recite_fun)
			fprintf(fp, "ReciteFunc %s~\n", recite_func_name(skill->recite_fun));

		if(skill->preink_fun)
			fprintf(fp, "PreinkFunc %s~\n", preink_func_name(skill->preink_fun));

		if(skill->ink_fun)
			fprintf(fp, "InkFunc %s~\n", ink_func_name(skill->ink_fun));

		if(skill->touch_fun)
			fprintf(fp, "TouchFunc %s~\n", touch_func_name(skill->touch_fun));
	}

	for(int i = 0; i < MAX_SKILL_VALUES; i++)
	{
		if (skill->values[i] != 0)
			fprintf(fp, "Value %d %d\n", i + 1, skill->values[i]);
		if (!IS_NULLSTR(skill->valuenames[i]))
			fprintf(fp, "ValueName %d %s~\n", i + 1, skill->valuenames[i]);
	}

	fprintf(fp, "Target %s~\n", flag_string(spell_target_types, skill->target));
	fprintf(fp, "Position %s~\n", flag_string(position_flags, skill->minimum_position));

	if(skill->pgsn)
		fprintf(fp, "GSN %s~\n", gsn_to_name(skill->pgsn));

	// TODO: After rcedit is added, change accordingly
	if(skill->race >= 0)
		fprintf(fp, "Race %s~\n", race_table[skill->race].name);

	fprintf(fp, "Mana %d\n", skill->cast_mana);
	fprintf(fp, "BrewMana %d\n", skill->brew_mana);
	fprintf(fp, "ScribeMana %d\n", skill->scribe_mana);
	fprintf(fp, "ImbueMana %d\n", skill->imbue_mana);
	fprintf(fp, "Beats %d\n", skill->beats);

	fprintf(fp, "NounDamage %s~\n", skill->noun_damage);
	fprintf(fp, "MsgOff %s~\n", skill->msg_off);
	fprintf(fp, "MsgObj %s~\n", skill->msg_obj);
	fprintf(fp, "MsgDisp %s~\n", skill->msg_disp);

	fprintf(fp, "Inks");
	for(int j = 0; j < 3; j++)
	{
		fprintf(fp, " %s~ %d", flag_string(catalyst_types, skill->inks[j][0]), skill->inks[j][1]);
	}
	fprintf(fp, "\n");


	// The rest are zero / false
	fprintf(fp, "#-%s\n", (skill->isspell?"SPELL":"SKILL"));
}

void save_skill_group(FILE *fp, SKILL_GROUP *group, char *header)
{
	fprintf(fp, "#%s %s~\n", header, group->name);

	ITERATOR it;
	char *item;
	iterator_start(&it, group->contents);
	while((item = (char *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Item %s~\n", item);
	}
	iterator_stop(&it);

	fprintf(fp, "#-GROUP\n");
}

void save_skills(void)
{
	FILE *fp;

	log_string("save_skills: saving " SKILLS_FILE);
	if ((fp = fopen(SKILLS_FILE, "w")) == NULL)
	{
		bug("save_skills: fopen", 0);
		perror(SKILLS_FILE);
	}
	else
	{
#if 0
		for (int i = 1; i < MAX_SKILL && skill_table[i].name; i++)
		{
			fprintf(fp, "#SKILL %s~ %d\n", skill_table[i].name, i+1);
			fprintf(fp, "Level");
			for(int j = 0; j < MAX_CLASS; j++) fprintf(fp, " %d", skill_table[i].skill_level[j]);
			fprintf(fp, "\n");
			fprintf(fp, "Rating");
			for(int j = 0; j < MAX_CLASS; j++) fprintf(fp, " %d", skill_table[i].rating[j]);
			fprintf(fp, "\n");

			// TODO: Add other artificing hooks
			if(skill_table[i].spell_fun && skill_table[i].spell_fun != spell_null)
				fprintf(fp, "SpellFunc %s~\n", spell_func_name(skill_table[i].spell_fun));

			fprintf(fp, "Target %s~\n", flag_string(spell_target_types, skill_table[i].target));
			fprintf(fp, "Position %s~\n", flag_string(position_flags, skill_table[i].minimum_position));

			if(skill_table[i].pgsn)
				fprintf(fp, "GSN %s~\n", gsn_to_name(skill_table[i].pgsn));

			// TODO: After rcedit is added, change accordingly
			if(skill_table[i].race > 0)
				fprintf(fp, "Race %s~\n", race_table[skill_table[i].race].name);

			fprintf(fp, "Mana %d\n", skill_table[i].min_mana);
			fprintf(fp, "Beats %d\n", skill_table[i].beats);

			fprintf(fp, "NounDamage %s~\n", skill_table[i].noun_damage);
			fprintf(fp, "MsgOff %s~\n", skill_table[i].msg_off);
			fprintf(fp, "MsgObj %s~\n", skill_table[i].msg_obj);
			fprintf(fp, "MsgDisp %s~\n", skill_table[i].msg_disp);

			fprintf(fp, "Inks");
			for(int j = 0; j < 3; j++)
			{
				fprintf(fp, " %s~ %d", flag_string(catalyst_types, skill_table[i].inks[j][0]), skill_table[i].inks[j][1]);
			}
			fprintf(fp, "\n");

			if (IS_VALID(skill_table[i].liquid))
				fprintf(fp, "Liquid %s~\n", skill_table[i].liquid->name);

			// No Token yet

			// The rest are zero / false
			fprintf(fp, "#-SKILL\n");
		}
#else
		ITERATOR it;
		SKILL_DATA *skill;
		iterator_start(&it, skills_list);
		while((skill = (SKILL_DATA *)iterator_nextdata(&it)))
		{
			save_skill(fp, skill);
		}
		iterator_stop(&it);
#endif

#if 0
		for(int i = 0; group_table[i].name; i++)
		{
			fprintf(fp, "#GROUP %s~\n", group_table[i].name);

			for(int j = 0; group_table[i].spells[j]; j++)
				fprintf(fp, "Item %s~\n", group_table[i].spells[j]);

			fprintf(fp, "#-GROUP\n");
		}
#else
		save_skill_group(fp, global_skills, "GLOBALGROUP");

		SKILL_GROUP *group;
		ITERATOR git;
		iterator_start(&git, skill_groups_list);
		while((group = (SKILL_GROUP *)iterator_nextdata(&git)))
		{
			save_skill_group(fp, group, "GROUP");
		}
		iterator_stop(&git);
#endif

		fprintf(fp, "End\n");
		fclose(fp);
	}
}

SKILL_DATA *load_skill(FILE *fp, bool isspell)
{
	SKILL_DATA *skill = new_skill_data();

	char buf[MSL];
	char *word;
	bool fMatch;

	skill->name = fread_string(fp);
	skill->uid = fread_number(fp);
	skill->isspell = isspell;

	const char *end = isspell ? "#-SPELL" : "#-SKILL";

    while (str_cmp((word = fread_word(fp)), end))
	{
		switch(word[0])
		{
			case 'B':
				KEY("Beats", skill->beats, fread_number(fp));
				if (!str_cmp(word, "BrewFunc"))
				{
					char *name = fread_string(fp);

					skill->brew_fun = brew_func_lookup(name);
					if (!skill->brew_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				KEY("BrewMana", skill->brew_mana, fread_number(fp));
				break;

			case 'D':
				KEYS("Display", skill->display, fread_string(fp));
				break;

			case 'G':
				if (!str_cmp(word, "GSN"))
				{
					char *name = fread_string(fp);
					skill->pgsn = gsn_from_name(name);
					fMatch = TRUE;
					break;
				}
				break;
			
			case 'I':
				KEY("ImbueMana", skill->imbue_mana, fread_number(fp));
				if (!str_cmp(word, "InkFunc"))
				{
					char *name = fread_string(fp);

					skill->ink_fun = ink_func_lookup(name);
					if (!skill->ink_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "Inks"))
				{
					for (int i = 0; i < 3; i++)
					{
						skill->inks[i][0] = stat_lookup(fread_string(fp), catalyst_types, CATALYST_NONE);
						skill->inks[i][1] = fread_number(fp);
					}
					fMatch = true;
					break;
				}
				break;
			
			case 'L':
				if (!str_cmp(word, "Level"))
				{
					for(int i = 0; i < MAX_CLASS; i++) skill->skill_level[i] = fread_number(fp);

					fMatch = true;
					break;
				}
				break;

			case 'M':
				KEY("Mana", skill->cast_mana, fread_number(fp));
				KEYS("MsgDisp", skill->msg_disp, fread_string(fp));
				KEYS("MsgObj", skill->msg_obj, fread_string(fp));
				KEYS("MsgOff", skill->msg_off, fread_string(fp));
				break;

			case 'N':
				KEYS("NounDamage", skill->noun_damage, fread_string(fp));
				break;

			case 'P':
				if (!str_cmp(word, "Position"))
				{
					int value = stat_lookup(fread_string(fp), position_flags, POS_DEAD);

					skill->minimum_position = value;
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "PreBrewFunc"))
				{
					char *name = fread_string(fp);

					skill->prebrew_fun = prebrew_func_lookup(name);
					if (!skill->prebrew_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "PreInkFunc"))
				{
					char *name = fread_string(fp);

					skill->preink_fun = preink_func_lookup(name);
					if (!skill->preink_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "PreScribeFunc"))
				{
					char *name = fread_string(fp);

					skill->prescribe_fun = prescribe_func_lookup(name);
					if (!skill->prescribe_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "PreSpellFunc"))
				{
					char *name = fread_string(fp);

					skill->prespell_fun = prespell_func_lookup(name);
					if (!skill->prespell_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				break;

			case 'Q':
				if (!str_cmp(word, "QuaffFunc"))
				{
					char *name = fread_string(fp);

					skill->quaff_fun = quaff_func_lookup(name);
					if (!skill->quaff_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				break;

			case 'R':
				if (!str_cmp(word, "Race"))
				{
					char *name = fread_string(fp);
					skill->race = race_lookup(name);

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "Rating"))
				{
					for(int i = 0; i < MAX_CLASS; i++) skill->rating[i] = fread_number(fp);

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "ReciteFunc"))
				{
					char *name = fread_string(fp);

					skill->recite_fun = recite_func_lookup(name);
					if (!skill->recite_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEY("ScribeMana", skill->scribe_mana, fread_number(fp));
				if (!str_cmp(word, "ScribeFunc"))
				{
					char *name = fread_string(fp);

					skill->scribe_fun = scribe_func_lookup(name);
					if (!skill->scribe_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "SpellFunc"))
				{
					char *name = fread_string(fp);

					skill->spell_fun = spell_func_lookup(name);
					if (!skill->spell_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				break;

			case 'T':
				if (!str_cmp(word, "Target"))
				{
					int value = stat_lookup(fread_string(fp), spell_target_types, TAR_IGNORE);

					skill->target = value;
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "Token"))
				{
					skill->token_load = fread_widevnum(fp, 0);
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word, "TouchFunc"))
				{
					char *name = fread_string(fp);

					skill->touch_fun = touch_func_lookup(name);
					if (!skill->touch_fun)
					{
						// Complain
					}
					fMatch = true;
					break;
				}
				break;

			case 'V':
				if (!str_cmp(word, "Value"))
				{
					int index = fread_number(fp);
					int value = fread_number(fp);

					if (index >= 1 && index <= MAX_SKILL_VALUES)
						skill->values[index - 1] = value;

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "ValueName"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);

					if (index >= 1 && index <= MAX_SKILL_VALUES)
						skill->valuenames[index - 1] = name;

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "load_skill: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return skill;
}

void insert_skill(SKILL_DATA *skill)
{
	ITERATOR it;
	SKILL_DATA *sk;
	iterator_start(&it, skills_list);
	while((sk = (SKILL_DATA *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(skill->name, sk->name);
		if(cmp < 0)
		{
			iterator_insert_before(&it, skill);
			break;
		}
	}
	iterator_stop(&it);

	if (!sk)
	{
		list_appendlink(skills_list, skill);
	}
}

void insert_skill_group_item(SKILL_GROUP *group, char *item)
{
	ITERATOR it;
	char *str;
	iterator_start(&it, group->contents);
	while((str = (char *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(item, str);
		if(cmp < 0)
		{
			iterator_insert_before(&it, item);
			break;
		}
	}
	iterator_stop(&it);

	if (!str)
	{
		list_appendlink(group->contents, item);
	}
}

SKILL_GROUP *load_skill_group(FILE *fp)
{
	SKILL_GROUP *group = new_skill_group_data();

	char buf[MSL];
	char *word;
	bool fMatch;

	group->name = fread_string(fp);

    while (str_cmp((word = fread_word(fp)), "#-GROUP"))
	{
		switch(word[0])
		{
			case 'I':
				if (!str_cmp(word, "Item"))
				{
					insert_skill_group_item(group, fread_string(fp));
					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "load_skill_group: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return group;
}

void insert_skill_group(SKILL_GROUP *group)
{
	ITERATOR it;
	SKILL_GROUP *gr;
	iterator_start(&it, skill_groups_list);
	while((gr = (SKILL_GROUP *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(group->name, gr->name);
		if(cmp < 0)
		{
			iterator_insert_before(&it, group);
			break;
		}
	}
	iterator_stop(&it);

	if (!gr)
	{
		list_appendlink(skill_groups_list, group);
	}
}

static void delete_skill_data(void *ptr)
{
	free_skill_data((SKILL_DATA *)ptr);
}

static void delete_skill_group_data(void *ptr)
{
	free_skill_group_data((SKILL_GROUP *)ptr);	
}

bool load_skills(void)
{
#if 1
	SKILL_DATA *skill;
	FILE *fp;
	char buf[MSL];
	char *word;
	bool fMatch;
	top_skill_uid = 0;

	log_string("load_skills: creating skills_list");
	skills_list = list_createx(FALSE, NULL, delete_skill_data);
	if (!IS_VALID(skills_list))
	{
		log_string("skills_list was not created.");
		return false;
	}

	log_string("load_skills: creating skill_groups_list");
	skill_groups_list = list_createx(FALSE, NULL, delete_skill_group_data);
	if (!IS_VALID(skill_groups_list))
	{
		log_string("skill_groups_list was not created.");
		return false;
	}

	log_string("load_skills: loading " SKILLS_FILE);
	if ((fp = fopen(SKILLS_FILE, "r")) == NULL)
	{
		bug("Skills file does not exist.", 0);
		perror(SKILLS_FILE);
		return false;
	}
	
	while(str_cmp((word = fread_word(fp)), "End"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#GLOBALGROUP"))
			{
				global_skills = load_skill_group(fp);
				if (!IS_VALID(global_skills))
					log_string("Failed to load global skill group.");
				fMatch = true;
				break;
			}
			if (!str_cmp(word, "#GROUP"))
			{
				SKILL_GROUP *group = load_skill_group(fp);
				if (group)
				{
					insert_skill_group(group);
				}
				else
					log_string("Failed to load a skill group.");
				fMatch = true;
				break;
			}
			if (!str_cmp(word, "#SKILL"))
			{
				skill = load_skill(fp, false);
				if (skill)
				{
					insert_skill(skill);

					if (skill->uid > top_skill_uid)
						top_skill_uid = skill->uid;
				}
				else
					log_string("Failed to load a skill.");

				fMatch = true;
				break;
			}
			if (!str_cmp(word, "#SPELL"))
			{
				skill = load_skill(fp, true);
				if (skill)
				{
					insert_skill(skill);

					if (skill->uid > top_skill_uid)
						top_skill_uid = skill->uid;
				}
				else
					log_string("Failed to load a skill.");

				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch) {
			sprintf(buf, "load_skills: no match for word %s", word);
			bug(buf, 0);
		}
	}

	// Post processing
	ITERATOR it;
	iterator_start(&it, skills_list);
	while((skill = (SKILL_DATA *)iterator_nextdata(&it)))
	{
		if (skill->pgsn)
			*(skill->pgsn) = skill->uid;

		for(int i = 0; i < MAX_CLASS; i++)
			if (skill->skill_level[i] > 30)
				skill->skill_level[i] = MAX_CLASS_LEVEL + 1;
	}
	iterator_stop(&it);

	for(int i = 0; gsn_table[i].name; i++)
	{
		if (gsn_table[i].gsn)
			*gsn_table[i].gsk = get_skill_data_uid(*gsn_table[i].gsn);
		else
			*gsn_table[i].gsk = NULL;
	}
	
#endif

	// Built-in hardcoded ones.
	memset(&gsk__auction, 0, sizeof(gsk__auction));
	gsk__auction.name = str_dup("auction");
	gsk__auction.display = str_dup("auction info");
	gsk__auction.uid = gsn__auction = -1;
	
	memset(&gsk__inspect, 0, sizeof(gsk__inspect));
	gsk__inspect.name = str_dup("inspect");
	gsk__inspect.display = str_dup("inspection");
	gsk__inspect.uid = gsn__inspect = -2;

	memset(&gsk__well_fed, 0, sizeof(gsk__well_fed));
	gsk__well_fed.name = str_dup("well fed");
	gsk__well_fed.display = str_dup("well fed");
	gsk__well_fed.uid = gsn__well_fed = -3;
	gsk__well_fed.noun_damage = str_dup("");
	gsk__well_fed.msg_disp = str_dup("You are no longer well fed.");
	gsk__well_fed.msg_obj = str_dup("");
	gsk__well_fed.msg_off = str_dup("You are no longer well fed.");

	return true;
}

// Free up all of the data from the loading
void cleanup_skills(void)
{
	// Do nothing right now
}

SKILL_DATA *get_skill_data(char *name)
{
	ITERATOR it;
	SKILL_DATA *skill;
	iterator_start(&it, skills_list);
	while((skill = (SKILL_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(name, skill->name))
			break;
	}
	iterator_stop(&it);

	return skill;
}

SKILL_DATA *get_skill_data_uid(sh_int uid)
{
	ITERATOR it;
	SKILL_DATA *skill;
	iterator_start(&it, skills_list);
	while((skill = (SKILL_DATA *)iterator_nextdata(&it)))
	{
		if (skill->uid == uid)
			break;
	}
	iterator_stop(&it);

	return skill;
}

void do_multi(CHAR_DATA *ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MSL];
    CHAR_DATA *mob;
    int pneuma_cost;
    int i;

    if (IS_NPC(ch))
	return;

    for (mob = ch->in_room->people; mob; mob = mob->next_in_room)
    {
	if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_TRAIN))
	    break;
    }

    if (mob == NULL)
    {
	send_to_char("You must seek a trainer first.\n\r", ch);
	return;
    }

    if (ch->level < MAX_CLASS_LEVEL)
    {
	send_to_char("You must first complete your current career.\n\r",ch);
	return;
    }

    if (ch->tot_level == LEVEL_HERO || IS_IMMORTAL(ch))
    {
	send_to_char("You have learned all that you can at this time.\n\r",ch);
	return;
    }

    if (argument[0] == '\0')
    {
 	show_multiclass_choices(ch, ch);
	return;
    }

    i = 0;
    if (ch->pcdata->sub_class_mage 		!= -1) i++;
    if (ch->pcdata->sub_class_cleric 		!= -1) i++;
    if (ch->pcdata->sub_class_thief 		!= -1) i++;
    if (ch->pcdata->sub_class_warrior 		!= -1) i++;
    if (ch->pcdata->second_sub_class_mage 	!= -1) i++;
    if (ch->pcdata->second_sub_class_cleric	!= -1) i++;
    if (ch->pcdata->second_sub_class_thief 	!= -1) i++;
    if (ch->pcdata->second_sub_class_warrior 	!= -1) i++;

    switch (i)
    {
	case 1:		pneuma_cost = 0;	break;
	case 2:		pneuma_cost = 500;	break;
	case 3: 	pneuma_cost = 1000;	break;
	case 5: 	pneuma_cost = 2500;	break;
	case 6:		pneuma_cost = 5000;	break;
	case 7: 	pneuma_cost = 10000;	break;
        default:	pneuma_cost = 500;	break;
    }

    for (i = 0; i < MAX_SUB_CLASS; i++)
    {
	if (!str_cmp(argument, sub_class_table[i].name[ch->sex]))
	    break;
    }

    if (i == MAX_SUB_CLASS)
    {
	send_to_char("That's not a subclass.\n\r", ch);
	return;
    }

    if (!can_choose_subclass(ch, i))
    {
	send_to_char("You can't choose that subclass.\n\r", ch);
	return;
    }

    if (ch->pneuma < pneuma_cost)
    {
	sprintf(buf, "You need %d pneuma to multiclass!\n\r", pneuma_cost);
	send_to_char(buf, ch);
	return;
    }

    if (!str_cmp("archmage", argument)
    ||  !str_cmp("geomancer", argument)
    ||  !str_cmp("illusionist", argument))
    {
	ch->pcdata->class_current = CLASS_MAGE;
	ch->pcdata->second_class_mage = CLASS_MAGE;

	if (!str_cmp("archmage", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_ARCHMAGE;
	    ch->pcdata->second_sub_class_mage = CLASS_MAGE_ARCHMAGE;
	}

	if (!str_cmp("geomancer", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_GEOMANCER;
	    ch->pcdata->second_sub_class_mage = CLASS_MAGE_GEOMANCER;
	}

	if (!str_cmp("illusionist", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_ILLUSIONIST;
	    ch->pcdata->second_sub_class_mage = CLASS_MAGE_ILLUSIONIST;
	}
    }

    if (!str_cmp("alchemist", argument)
    ||  !str_cmp("ranger", argument)
    ||  !str_cmp("adept", argument))
    {
	ch->pcdata->class_current = CLASS_CLERIC;
	ch->pcdata->second_class_cleric = CLASS_CLERIC;

	if (!str_cmp("alchemist", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_ALCHEMIST;
	    ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ALCHEMIST;
	}

	if (!str_cmp("ranger", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_RANGER;
	    ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_RANGER;
	}

	if (!str_cmp("adept", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_ADEPT;
	    ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ADEPT;
	}
    }

    if (!str_cmp("highwayman", argument)
    || !str_cmp("ninja", argument)
    || !str_cmp("sage", argument))
    {
	ch->pcdata->class_current = CLASS_THIEF;
	ch->pcdata->second_class_thief = CLASS_THIEF;

	if (!str_cmp("highwayman", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_HIGHWAYMAN;
	    ch->pcdata->second_sub_class_thief = CLASS_THIEF_HIGHWAYMAN;
	}

	if (!str_cmp("ninja", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_NINJA;
	    ch->pcdata->second_sub_class_thief = CLASS_THIEF_NINJA;
	}

	if (!str_cmp("sage", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_SAGE;
	    ch->pcdata->second_sub_class_thief = CLASS_THIEF_SAGE;
	}
    }

    if (!str_cmp("warlord", argument)
    ||  !str_cmp("destroyer", argument)
    ||  !str_cmp("crusader", argument))
    {
	ch->pcdata->class_current = CLASS_WARRIOR;
	ch->pcdata->second_class_warrior = CLASS_WARRIOR;

	if (!str_cmp("warlord", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_WARLORD;
	    ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_WARLORD;
	}

	if (!str_cmp("destroyer", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_DESTROYER;
	    ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_DESTROYER;
	}

	if (!str_cmp("crusader", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_CRUSADER;
	    ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_CRUSADER;
	}
    }

    if (!str_cmp("necromancer", argument)
    || !str_cmp("sorcerer", argument)
    || !str_cmp("wizard", argument))
    {
	ch->pcdata->class_current = CLASS_MAGE;
	ch->pcdata->class_mage = CLASS_MAGE;

	if (!str_cmp("necromancer", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_NECROMANCER;
	    ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER;
	}

	if (!str_cmp("sorcerer", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_SORCERER;
	    ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;
	}

	if (!str_cmp("wizard", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_MAGE_WIZARD;
	    ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;
	}
    }

    if (!str_cmp("witch", argument)
    ||  !str_cmp("warlock", argument)
    ||  !str_cmp("druid", argument)
    ||  !str_cmp("monk", argument))
    {
	ch->pcdata->class_current = CLASS_CLERIC;
	ch->pcdata->class_cleric = CLASS_CLERIC;
	if (!str_cmp("warlock", argument)
	||  !str_cmp("witch", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_WITCH;
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH;
	}

	if (!str_cmp("druid", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_DRUID;
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;
	}

	if (!str_cmp("monk", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_CLERIC_MONK;
	    ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;
	}
    }

    if (!str_cmp("assassin", argument)
    || !str_cmp("rogue", argument)
    || !str_cmp("bard", argument))
    {
	ch->pcdata->class_current = CLASS_THIEF;
	ch->pcdata->class_thief = CLASS_THIEF;

	if (!str_cmp("assassin", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_ASSASSIN;
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN;
	}

	if (!str_cmp("rogue", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_ROGUE;
	    ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;
	}

	if (!str_cmp("bard", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_THIEF_BARD;
	    ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;
	}
    }

    if (!str_cmp("marauder", argument)
    || !str_cmp("gladiator", argument)
    || !str_cmp("paladin", argument))
    {
	ch->pcdata->class_current = CLASS_WARRIOR;
	ch->pcdata->class_warrior = CLASS_WARRIOR;

	if (!str_cmp("marauder", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_MARAUDER;
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER;
	}

	if (!str_cmp("gladiator", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_GLADIATOR;
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;
	}

	if (!str_cmp("paladin", argument))
	{
	    ch->pcdata->sub_class_current = CLASS_WARRIOR_PALADIN;
	    ch->pcdata->sub_class_warrior = CLASS_WARRIOR_PALADIN;
	}
    }

    ch->pneuma -= pneuma_cost;
    ch->level = 1;
    ch->exp = 0;
    ch->tot_level++;
    group_add(ch, class_table[ch->pcdata->class_current].base_group, TRUE);
    group_add(ch, sub_class_table[ch->pcdata->sub_class_current].default_group, TRUE);
    sprintf(buf2, "%s", sub_class_table[ch->pcdata->sub_class_current].name[ch->sex]);
    buf2[0] = UPPER(buf2[0]);
    sprintf(buf, "All congratulate %s, who is now a%s %s!",
        ch->name, (buf2[0] == 'A' || buf2[0] == 'I' || buf2[0] == 'E' || buf2[0] == 'U'
	    || buf2[0] == '0') ? "n" : "", buf2);
    crier_announce(buf);
    double_xp(ch);

	p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MULTICLASS, NULL,0,0,0,0,0);
}


void show_multiclass_choices(CHAR_DATA *ch, CHAR_DATA *looker)
{
    char buf[MSL];
    char buf2[MSL];
    int i;

    if (ch == looker)
	send_to_char("{YYou are skilled in the following subclasses:{x\n\r", looker);
    else
	act("{Y$N is skilled in the following subclasses:{x", looker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    sprintf(buf, "{x");

    for (i = 0; i < MAX_SUB_CLASS; i++)
    {
	switch (sub_class_table[i].class)
	{
	    case CLASS_MAGE:
		if (get_profession(ch, sub_class_table[i].remort ? SECOND_SUBCLASS_MAGE : SUBCLASS_MAGE) == i)
		{
		    sprintf(buf2, "{R%s{x\n\r", sub_class_table[i].name[ch->sex]);
		    buf2[2] = UPPER(buf2[2]);
		    strcat(buf, buf2);
		}

		break;

	    case CLASS_CLERIC:
		if (get_profession(ch, sub_class_table[i].remort ? SECOND_SUBCLASS_CLERIC : SUBCLASS_CLERIC) == i)
		{
		    sprintf(buf2, "{R%s{x\n\r", sub_class_table[i].name[ch->sex]);
		    buf2[2] = UPPER(buf2[2]);
		    strcat(buf, buf2);
		}

		break;

	    case CLASS_THIEF:
		if (get_profession(ch, sub_class_table[i].remort ? SECOND_SUBCLASS_THIEF : SUBCLASS_THIEF) == i)
		{
		    sprintf(buf2, "{R%s{x\n\r", sub_class_table[i].name[ch->sex]);
		    buf2[2] = UPPER(buf2[2]);
		    strcat(buf, buf2);
		}

		break;

	    case CLASS_WARRIOR:
		if (get_profession(ch, sub_class_table[i].remort ? SECOND_SUBCLASS_WARRIOR : SUBCLASS_WARRIOR) == i)
		{
		    sprintf(buf2, "{R%s{x\n\r", sub_class_table[i].name[ch->sex]);
		    buf2[2] = UPPER(buf2[2]);
		    strcat(buf, buf2);
		}

		break;
	}
    }

    send_to_char(buf, looker);

	if( ch->tot_level == LEVEL_HERO ) {
	    if (ch == looker)
			sprintf(buf, "\n\r{YYou may remort to:{x\n\r");
	    else
			sprintf(buf, "\n\r{Y%s may remort to:{x\n\r", ch->name);
	} else {
	    if (ch == looker)
			sprintf(buf, "\n\r{YYou may multiclass to:{x\n\r");
	    else
			sprintf(buf, "\n\r{Y%s may multiclass to:{x\n\r", ch->name);
	}

    for (i = 0; i < MAX_SUB_CLASS; i++)
    {
	if (can_choose_subclass(ch, i))
	{
	    sprintf(buf2, "{x%s\n\r", sub_class_table[i].name[ch->sex]);
	    buf2[2] = UPPER(buf2[2]);
	    strcat(buf, buf2);
	}
    }

    send_to_char(buf, looker);
}


bool can_choose_subclass(CHAR_DATA *ch, int subclass)
{
    char buf[MSL];
    int prof;

    // 1st mort
    if (subclass >= CLASS_WARRIOR_MARAUDER && subclass <= CLASS_THIEF_BARD)
    {
	// Check if they've done it before
	switch (sub_class_table[subclass].class)
	{
	    case CLASS_MAGE:
	        if (get_profession(ch, CLASS_MAGE) != -1)
			    return FALSE;
		    break;

	    case CLASS_CLERIC:
	        if (get_profession(ch, CLASS_CLERIC) != -1)
			    return FALSE;
		    break;

	    case CLASS_THIEF:
	        if (get_profession(ch, CLASS_THIEF) != -1)
			    return FALSE;
		    break;

	    case CLASS_WARRIOR:
	        if (get_profession(ch, CLASS_WARRIOR) != -1)
			    return FALSE;
		    break;
	}

	// Check if they fit align
	switch (sub_class_table[subclass].alignment)
	{
		case ALIGN_EVIL:
			if (IS_GOOD(ch))
				return FALSE;
			break;

		case ALIGN_GOOD:
			if (IS_EVIL(ch))
				return FALSE;
			break;
	}

	return TRUE;
    }
    else // Remort
    {
	if (!IS_REMORT(ch) && ch->tot_level != LEVEL_HERO)
	    return FALSE;

	switch (sub_class_table[subclass].class)
	{
	    case CLASS_MAGE:
			if (get_profession(ch, SECOND_SUBCLASS_MAGE) != -1)
				return FALSE;

			prof = get_profession(ch, SUBCLASS_MAGE);

			if (prof == sub_class_table[subclass].prereq[0] ||
				prof == sub_class_table[subclass].prereq[1])

			return TRUE;
			break;

	    case CLASS_CLERIC:
			if (get_profession(ch, SECOND_SUBCLASS_CLERIC) != -1)
				return FALSE;

			prof = get_profession(ch, SUBCLASS_CLERIC);

			if (prof == sub_class_table[subclass].prereq[0] ||
				prof == sub_class_table[subclass].prereq[1])

			return TRUE;
			break;

	    case CLASS_THIEF:
			if (get_profession(ch, SECOND_SUBCLASS_THIEF) != -1)
				return FALSE;

			prof = get_profession(ch, SUBCLASS_THIEF);

			if (prof == sub_class_table[subclass].prereq[0] ||
				prof == sub_class_table[subclass].prereq[1])

			return TRUE;
			break;

	    case CLASS_WARRIOR:
			if (get_profession(ch, SECOND_SUBCLASS_WARRIOR) != -1)
				return FALSE;

			prof = get_profession(ch, SUBCLASS_WARRIOR);

			if (prof == sub_class_table[subclass].prereq[0] ||
				prof == sub_class_table[subclass].prereq[1])

			return TRUE;
			break;

	    default:
		    return FALSE;
	}

	return FALSE;
    }

    sprintf(buf, "can_choose_subclass: invalid subclass for %s[%d]", ch->name, subclass);
    bug(buf, 0);
    return FALSE;
}


// Train a stat
void do_train(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *mob;
    AFFECT_DATA *af;
    OBJ_DATA *obj;
    sh_int stat = - 1;
    char *pOutput = NULL;
    int cost;
    int max_hit;
    int max_mana;
    int max_move;
    int mod_hit;
    int mod_mana;
    int mod_move;
    int rating;
    char *name;
    SKILL_ENTRY *entry;

    if (IS_NPC(ch))
		return;

	argument = one_argument(argument, arg);

	//Moving this further down to account for anything other than a valid train argument.
/*	if( arg[0] == '\0') {
		sprintf(buf, "You have %d training sessions.\n\r", ch->train);
		send_to_char(buf, ch);
		return;
	} else*/
	 if(!str_cmp(arg, "skill")) {
		for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && (mob->pIndexData->vnum == VNUM_QUESTOR_1
				||  mob->pIndexData->vnum == VNUM_QUESTOR_2))
			break;
		}

		if (mob == NULL)
		{
			send_to_char("There is nobody here to help you do that.\n\r", ch);
			return;
		}

		// TODO: Rework
		if (ch->tot_level < 2 * MAX_CLASS_LEVEL + 1)
		{
			sprintf(buf, "%s, you must be of at least the third class to do this.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		entry = skill_entry_findname(ch->sorted_skills, argument);
		if( !entry )
		{
			sprintf(buf, "You know nothing of that skill, %s!\n\r", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		if( entry->token )
		{
		    if(p_percent_trigger(NULL, NULL, NULL, entry->token, ch, mob, NULL, NULL, NULL, TRIG_PRETRAINTOKEN, NULL,0,0,0,0,0))
		    {
				send_to_char("There is nobody here to help you do that.\n\r", ch);
				return;
			}

			/*
			if( entry->token->value[TOKVAL_SPELL_LEARN] < 1 )
			{
				send_to_char("There is nobody here to help you do that.\n\r", ch);
				return;
			}
			*/
		}

		name = skill_entry_name(entry);
		rating = skill_entry_rating(ch, entry);
		if( rating < MAX_SKILL_LEARNABLE )
		{
			sprintf(buf, "You must come back when you have studied this skill to the utmost through mundane means, %s.\n\r", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

        if (rating >= MAX_SKILL_TRAINABLE)
		{
			sprintf(buf, "Even I can't help your mastery of %s past this point, %s.", name, pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		cost = 3 * rating / 20;
		if (cost <= ch->train)
		{
			ch->train -= cost;
			act("$n trains $t with $N.", ch, mob, NULL, NULL, NULL, name, NULL, TO_ROOM);
			act("You train $t with $N.", ch, mob, NULL, NULL, NULL, name, NULL, TO_CHAR);
			act("{YYou feel your mastery of $t soaring to new heights!{x", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR);

			entry->rating++;
			/*
			if( entry->token ) {
				if( entry->token->pIndexData->value[TOKVAL_SPELL_RATING] > 0 )
					entry->token->value[TOKVAL_SPELL_RATING] += entry->token->pIndexData->value[TOKVAL_SPELL_RATING];
				else
					entry->token->value[TOKVAL_SPELL_RATING]++;
			} else
				ch->pcdata->learned[entry->sn]++;
			*/
		}
		else
		{
			sprintf(buf, "It would take %d trains to train that skill, %s.", cost, pers(ch, mob));
			do_say(mob, buf);
		}
	} else {
		for (mob = ch->in_room->people; mob; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_TRAIN))
				break;
		}

		if (mob == NULL)
		{
			send_to_char("You can't do that here.\n\r", ch);
			return;
		}

	    if(p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRETRAIN, arg,0,0,0,0,0))
			return;

		cost = 1;

		mod_hit = 0;
		mod_mana = 0;
		mod_move = 0;

		max_hit = pc_race_table[ch->race].max_vital_stats[MAX_HIT];
		max_mana = pc_race_table[ch->race].max_vital_stats[MAX_MANA];
		max_move = pc_race_table[ch->race].max_vital_stats[MAX_MOVE];

		for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
		{
			if (obj->wear_loc != WEAR_NONE)
			{
				for (af = obj->affected; af != NULL; af = af->next)
				{
					if (af->location == APPLY_HIT)
						mod_hit += af->modifier;
					if (af->location == APPLY_MANA)
						mod_mana += af->modifier;
					if (af->location == APPLY_MOVE)
						mod_move += af->modifier;
				}
			}
		}

		max_hit += mod_hit;
		max_mana += mod_mana;
		mod_move += mod_move;

		if (!str_cmp(arg, "str"))
		{
			if (class_table[ch->pcdata->class_current].attr_prime == STAT_STR || ch->pcdata->class_mage != -1)
				cost    = 1;
			stat        = STAT_STR;
			pOutput     = "strength";
		}

		else if (!str_cmp(arg, "int"))
		{
			if (class_table[ch->pcdata->class_current].attr_prime == STAT_INT || ch->pcdata->class_mage != -1)
				cost    = 1;
			stat	    = STAT_INT;
			pOutput     = "intelligence";
		}

		else if (!str_cmp(arg, "wis"))
		{
			if (class_table[ch->pcdata->class_current].attr_prime == STAT_WIS || ch->pcdata->class_cleric != -1)
				cost    = 1;
			stat	    = STAT_WIS;
			pOutput     = "wisdom";
		}

		else if (!str_cmp(arg, "dex"))
		{
			if (class_table[ch->pcdata->class_current].attr_prime == STAT_DEX || ch->pcdata->class_thief != -1)
				cost    = 1;
			stat  	    = STAT_DEX;
			pOutput     = "dexterity";
		}

		else if (!str_cmp(arg, "con"))
		{
			if (class_table[ch->pcdata->class_current].attr_prime == STAT_CON)
				cost    = 1;
			stat	    = STAT_CON;
			pOutput     = "constitution";
		}
	    else if (!str_cmp(arg, "hp"))
		    cost = 1;

	    else if (!str_cmp(arg, "mana"))
		    cost = 1;

	    else if (!str_cmp(arg, "move"))
		    cost = 1;

	    else
	    {
			sprintf(buf, "You have %d training sessions. \n\r", ch->train);
			send_to_char(buf,ch);
			strcpy(buf, "You can train:");
			if (ch->perm_stat[STAT_STR] < get_max_train(ch,STAT_STR))	strcat(buf, " str");
			if (ch->perm_stat[STAT_INT] < get_max_train(ch,STAT_INT))	strcat(buf, " int");
			if (ch->perm_stat[STAT_WIS] < get_max_train(ch,STAT_WIS))	strcat(buf, " wis");
			if (ch->perm_stat[STAT_DEX] < get_max_train(ch,STAT_DEX))	strcat(buf, " dex");
			if (ch->perm_stat[STAT_CON] < get_max_train(ch,STAT_CON))	strcat(buf, " con");
			if (ch->max_hit < max_hit)									strcat(buf, " hp");
			if (ch->max_mana < max_mana)								strcat(buf, " mana");
        	if (ch->max_move < max_move)								strcat(buf, " move");

			if (buf[strlen(buf)-1] != ':')
			{
			    strcat(buf, ".\n\r");
			    send_to_char(buf, ch);
			}
			else
			{
			    act("You have nothing left to train.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}

			return;
	    }


		if (!str_cmp(arg, "hp"))
		{
			if (cost > ch->train)
			{
				send_to_char("You don't have enough training sessions.\n\r", ch);
				return;
			}

			if (ch->max_hit >= max_hit) {
				send_to_char("Your body can't get any tougher.\n\r", ch);
				return;
			}

			ch->train -= cost;
			ch->pcdata->perm_hit += 10;
			ch->max_hit += 10;
			ch->hit += 10;
			act("Your health increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
			act("$n's health increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			return;
		}

		if (!str_cmp(arg, "mana"))
		{
			if (cost > ch->train)
			{
				send_to_char("You don't have enough training sessions.\n\r", ch);
				return;
			}

			if (ch->max_mana >= max_mana) {
				send_to_char("Your body can't get any tougher.\n\r", ch);
				return;
			}

			ch->train -= cost;
			ch->pcdata->perm_mana += 10;
			ch->max_mana += 10;
			ch->mana +=10;
			act("Your mana increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
			act("$n's mana increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);

			return;
		}

		if (!str_cmp(arg, "move"))
		{
			if (cost > ch->train)
			{
				send_to_char("You don't have enough training sessions.\n\r", ch);
				return;
			}

			if (ch->max_move >= max_move) {
				send_to_char("Your body can't get any more durable.\n\r", ch);
				return;
			}

			ch->train -= cost;
			ch->pcdata->perm_move += 10;
			ch->max_move += 10;
			ch->move += 10;
			act("Your stamina increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
			act("$n's stamina increases!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			return;
		}

		if (ch->perm_stat[stat]  >= get_max_train(ch,stat))
		{
			act("Your $T is already at maximum.", ch, NULL, NULL, NULL, NULL, NULL, pOutput, TO_CHAR);
			return;
		}

		if (cost > ch->train)
		{
			send_to_char("You don't have enough training sessions.\n\r", ch);
			return;
		}

		ch->train -= cost;
		add_perm_stat(ch, stat, 1);

		act("Your $T increases!", ch, NULL, NULL, NULL, NULL, NULL, pOutput, TO_CHAR);
		act("$n's $T increases!", ch, NULL, NULL, NULL, NULL, NULL, pOutput, TO_ROOM);
	}
}


void do_convert(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *trainer;

    if (ch == NULL)
    {
        bug("do_convert: NULL ch", 0);
        return;
    }

    if (IS_IMMORTAL(ch))
    {
        send_to_char("Why would an immortal need to convert?\n\r", ch);
        return;
    }

    if (IS_NPC(ch))
	return;

    for (trainer = ch->in_room->people; trainer != NULL; trainer = trainer->next_in_room)
    {
	if (IS_NPC(trainer)
	&&  (IS_SET(trainer->act[0], ACT_TRAIN) || IS_SET(trainer->act[0], ACT_PRACTICE)))
	    break;
    }

    if (trainer == NULL || !can_see(ch,trainer))
    {
	send_to_char("You can't do that here.\n\r",ch);
	return;
    }

    one_argument(argument,arg);

    if (arg[0] == '\0')
    {
	do_function(trainer, &do_say, "What is it that you would like to convert?");
	return;
    }

    if (!str_prefix(arg,"practices"))
    {

	if (ch->practice < 20)
	{
	    act("{R$N tells you 'You don't have enough practices. You must have 20 practices!'{x",
		ch,trainer, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	    return;
	}

	act("$N helps you apply your practice to training.",
		ch,trainer, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	ch->practice -= 20;
	ch->train++;
	return;
    }

    if (!str_prefix(arg,"trains"))
    {
	if (ch->train < 1)
	{
	    act("{R$N tells you 'You don't have any trains to convert into pracs!'{x",
		ch,trainer, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	    return;
	}

	act("$N helps you apply your practice to training.",
		ch,trainer, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	ch->practice += 20;
	ch->train--;
	return;
    }

    send_to_char("Syntax:  convert <practices|trains>\n\r", ch);
}

void list_skill_entries(CHAR_DATA *ch, char *argument, bool show_skills, bool show_spells, bool hide_learned)
{
	BUFFER *buffer;

	bool found = FALSE;
	bool favonly = FALSE;
	char buf[MAX_STRING_LENGTH];
	char arg[MSL];
	int i;
	char color, *name;
	int skill, rating, mod, level, mana;
	SKILL_ENTRY *entry;

	if (ch == NULL) {
		bug("do_skills: NULL ch pointer.", 0);
		return;
	}

	if (IS_NPC(ch)) return;

	argument = one_argument( argument, arg );

	if( !hide_learned && *argument == '/' && !str_prefix(argument, "/favourite") )
	{
		entry = skill_entry_findname(ch->sorted_skills, arg);

		if( entry == NULL ) {
			send_to_char("Ability not found.\n\r", ch );
			return;
		}
		name = skill_entry_name(entry);
		rating = skill_entry_rating(ch, entry);
		if( rating < 0)
			sprintf(buf, "'%s' cannot be favourited.\n\r", name);
		else if(IS_SET(entry->flags, SKILL_FAVOURITE)) {
			sprintf(buf, "'%s' removed from favourites.\n\r", name);
			REMOVE_BIT(entry->flags, SKILL_FAVOURITE);
		} else {
			sprintf(buf, "'%s' added to favourites.\n\r", name);
			SET_BIT(entry->flags, SKILL_FAVOURITE);
		}

		send_to_char(buf, ch );
		return;
	}

	buffer = new_buf();
	if( show_spells )
		add_buf(buffer, "\n\r{B [ {w# {B] [ {wMana{B ]  [ {wName{B ]                       [ {w%{B ]{x\n\r");
	else
		add_buf(buffer, "\n\r{B [ {w# {B] [ {wName{B ]                       [ {w%{B ]{x\n\r");

	// Do we only show favourited skills?
	favonly = (IS_SET(ch->act[1], PLR_FAVSKILLS)) && !hide_learned;

	i = 1;

	// Show spells people lost
 	if (!str_cmp(arg, "/negated")) {
		for(entry = ch->sorted_skills; entry; entry = entry->next) {
			if( !show_skills && !IS_SET(entry->flags, SKILL_SPELL) ) continue;
			if( !show_spells && IS_SET(entry->flags, SKILL_SPELL) ) continue;

			rating = skill_entry_rating(ch, entry);

			if(rating < 0) {
				color = ( IS_IMMORTAL(ch) && IS_VALID(entry->token) ) ? 'G' : 'Y';

				if( show_spells )
					sprintf(buf, " %3d     ---      {%c%-26s    {D%d%%{x\n\r", i++, color, skill_entry_name(entry), -rating);
				else
					sprintf(buf, " %3d     {%c%-26s    {D%d%%{x\n\r", i++, color, skill_entry_name(entry), -rating);
				add_buf(buffer,buf);
				found = TRUE;
			}
		}
	} else {
		char min_mana[MIL];
		char eff_name[MIL];
		for(entry = ch->sorted_skills; entry; entry = entry->next) {
			if( favonly && !IS_SET(entry->flags, SKILL_FAVOURITE) ) continue;

			if( !show_skills && !IS_SET(entry->flags, SKILL_SPELL) ) continue;
			if( !show_spells && IS_SET(entry->flags, SKILL_SPELL) ) continue;

			skill = skill_entry_rating(ch, entry);
			mod = skill_entry_mod(ch, entry);
			level = skill_entry_level(ch, entry);	// Negate level implies they do not know it yet.
			name = skill_entry_name(entry);
			mana = skill_entry_mana(ch, entry);

			if( skill < 1 ) continue;

			if( hide_learned && skill >= MAX_SKILL_LEARNABLE) continue;

			if( !arg[0] || !str_prefix(arg, name) ) {

				color = ( IS_IMMORTAL(ch) && IS_VALID(entry->token) ) ? 'G' : 'Y';

				sprintf(eff_name, "{%c%s", color, name);

				// Don't have it yet
				if( show_spells ) {
					if( mana > 0 )
						sprintf(min_mana, "%3d", mana);
					else
						strcpy(min_mana, "---");

					if( level < 0 )
						sprintf(buf, " %3d     %-8s %-26s    {xUnlocks at {W%d", i, min_mana, eff_name, -level);
					else {
						rating = skill + mod;
						rating = URANGE(0,rating,100);

						if( rating >= 100 ) {	// MASTER
							if( mod )
								sprintf(buf, " %3d     %-8s %-26s    {MMaster {W(%+d%%)", i, min_mana, eff_name, mod);
							else
								sprintf(buf, " %3d     %-8s %-26s    {MMaster", i, min_mana, eff_name);
						} else if( mod )
							sprintf(buf, " %3d     %-8s %-26s    {G%d%% {W(%+d%%)", i, min_mana, eff_name, rating, mod);
						else
							sprintf(buf, " %3d     %-8s %-26s    {G%d%%", i, min_mana,  eff_name, rating);
					}
				} else {
					if( level < 0 )
						sprintf(buf, " %3d     %-26s    {xUnlocks at {W%d", i, eff_name, -level);
					else {
						rating = skill + mod;
						rating = URANGE(0,rating,100);

						if( rating >= 100 ) {	// MASTER
							if( mod )
								sprintf(buf, " %3d     %-26s    {MMaster {W(%+d%%)", i, eff_name, mod);
							else
								sprintf(buf, " %3d     %-26s    {MMaster", i, eff_name);
						} else if( mod )
							sprintf(buf, " %3d     %-26s    {G%d%% {W(%+d%%)", i, eff_name, rating, mod);
						else
							sprintf(buf, " %3d     %-26s    {G%d%%", i, eff_name, rating);
					}
				}


				if(!favonly && IS_SET(entry->flags, SKILL_FAVOURITE))
					strcat(buf, " {W[FAVOURITE]");

				strcat(buf, "{x\n\r");


				add_buf(buffer,buf);
				i++;
				found = TRUE;
			}

		}
	}

	if (!found) {
		if( show_skills ) {
			if( show_spells )
				send_to_char("No skills/spells found.\n\r", ch );
			else
				send_to_char("No skills found.\n\r", ch );
		} else if( show_spells )
			send_to_char("No spells found.\n\r", ch );
		else
			send_to_char("No abilities found.\n\r", ch );
	}
	else
		page_to_char(buf_string(buffer),ch);
	free_buf(buffer);
}


void do_spells(CHAR_DATA *ch, char *argument)
{
	list_skill_entries(ch, argument, FALSE, TRUE, FALSE);
}


void do_skills(CHAR_DATA *ch, char *argument)
{
	list_skill_entries(ch, argument, TRUE, FALSE, FALSE);
}


long exp_per_level(CHAR_DATA *ch, long points)
{
    double expl;

    if (IS_NPC(ch))
	return 1000;

    expl = exp_per_level_table[ch->tot_level].exp;

    if (IS_REMORT(ch))
        expl = 3 * expl / 2;

    if (ch->level == MAX_CLASS_LEVEL)
	expl = 0;

    return expl;
}


void check_improve_show( CHAR_DATA *ch, SKILL_DATA *skill, bool success, int multiplier, bool show )
{
    int chance;
    char buf[100];
    int this_class;
    SKILL_ENTRY *entry;

    if (IS_NPC(ch))
		return;

    if (IS_SOCIAL(ch))
		return;

	entry = skill_entry_findskill(ch->sorted_skills, skill);
	if(!entry)
		return;

	if(!IS_SET(entry->flags, SKILL_IMPROVE))
		return;

    this_class = get_this_class(ch, skill);

    if (get_skill(ch, skill) == 0
    ||  skill->rating[this_class] == 0
    ||  entry->rating <= 0
    ||  entry->rating == 100)
	return;

    // check to see if the character has a chance to learn
    chance      = 10 * int_app[get_curr_stat(ch, STAT_INT)].learn;
    multiplier  = UMAX(multiplier,1);
//    multiplier  = UMIN(multiplier + 3, 8);
    chance     /= (multiplier * skill->rating[this_class] * 4);
    chance     += ch->level;

    if (number_range(1,1000) > chance)
	return;

    // now that the character has a CHANCE to learn, see if they really have
    if (success)
    {
	chance = URANGE(2, 100 - entry->rating, 25);
	if (number_percent() < chance)
	{
	    sprintf(buf,"{WYou have become better at %s!{x\n\r", skill->name);
	    send_to_char(buf,ch);
	    entry->rating++;
	    gain_exp(ch, 2 * skill->rating[this_class]);
	}
    }
    else
    {
	chance = URANGE(5, entry->rating/2, 30);
	if (number_percent() < chance)
	{
	    sprintf(buf,
		"{WYou learn from your mistakes, and your %s skill improves.{x\n\r",
		skill->name);
	    send_to_char(buf, ch);
	    entry->rating += number_range(1,3);
	    entry->rating = UMIN(entry->rating,100);
	    gain_exp(ch,2 * skill->rating[ch->pcdata->class_current]);
	}
    }
}

void check_improve( CHAR_DATA *ch, SKILL_DATA *skill, bool success, int multiplier )
{
	check_improve_show( ch, skill, success, multiplier, true );
}

SKILL_GROUP *group_lookup( const char *name )
{
	ITERATOR it;
	SKILL_GROUP *group;

	if (global_skills && LOWER(name[0]) == LOWER(global_skills->name[0]))
	{
		if (!str_prefix(name, global_skills->name))
			return global_skills;
	}

	iterator_start(&it, skill_groups_list);
	while((group = (SKILL_GROUP *)iterator_nextdata(&it)))
	{
		if (LOWER(name[0]) == LOWER(group->name[0]) &&
			!str_prefix(name, group->name))
			break;
	}
	iterator_stop(&it);

	return group;
}

SKILL_GROUP *group_lookup_exact(const char *name)
{
	ITERATOR it;
	SKILL_GROUP *group;

	if (global_skills && LOWER(name[0]) == LOWER(global_skills->name[0]))
	{
		if (!str_cmp(name, global_skills->name))
			return global_skills;
	}

	iterator_start(&it, skill_groups_list);
	while((group = (SKILL_GROUP *)iterator_nextdata(&it)))
	{
		if (LOWER(name[0]) == LOWER(group->name[0]) &&
			!str_cmp(name, group->name))
			break;
	}
	iterator_stop(&it);

	return group;
}


bool group_has_item_exact(SKILL_GROUP *group, const char *name)
{
	ITERATOR it;
	char *str;

	if (!IS_VALID(group)) return false;

	iterator_start(&it, group->contents);
	while((str = (char *)iterator_nextdata(&it)))
	{
		if (!str_cmp(name, str))
			break;
	}
	iterator_stop(&it);

	return str != NULL;
}

void gn_add(CHAR_DATA *ch, SKILL_GROUP *group)
{
	if (!IS_VALID(group)) return;

	list_appendlink(ch->pcdata->group_known, group);

	ITERATOR sit;
	char *sk;
	iterator_start(&sit, group->contents);
	while((sk = (char *)iterator_nextdata(&sit)))
	{
		group_add(ch, sk, false);
	}
	iterator_stop(&sit);
}


void gn_remove( CHAR_DATA *ch, SKILL_GROUP *group)
{

	if (!IS_VALID(group)) return;

	list_remlink(ch->pcdata->group_known, group);

	ITERATOR sit;
	char *sk;
	iterator_start(&sit, group->contents);
	while((sk = (char *)iterator_nextdata(&sit)))
	{
		group_remove(ch, sk);
	}
	iterator_stop(&sit);
}


void group_add( CHAR_DATA *ch, const char *name, bool deduct)
{
	SKILL_DATA *sk;

    if (IS_NPC(ch))
	return;

    sk = get_skill_data((char *)name);

    if (IS_VALID(sk))
    {
		SKILL_ENTRY *entry = skill_entry_findskill(ch->sorted_skills, sk);

		if (!entry)
		{
			TOKEN_DATA *token = NULL;
			if (sk->token)
				token = give_token(sk->token, ch, NULL, NULL);
			
			if (is_skill_spell(sk))
				entry = skill_entry_addspell(ch, sk, token, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
			else
				entry = skill_entry_addskill(ch, sk, token, SKILLSRC_NORMAL, SKILL_AUTOMATIC);

			entry->rating = 1;
		}

		return;
    }

	SKILL_GROUP *group = group_lookup(name);
	if (IS_VALID(group))
	{
		if (!list_hasdata(ch->pcdata->group_known, group))
			gn_add(ch, group);
	}
}


void group_remove(CHAR_DATA *ch, const char *name)
{
    SKILL_DATA *sk;

    sk = get_skill_data((char *)name);

    if (IS_VALID(sk))
    {
		SKILL_ENTRY *entry = skill_entry_findskill(ch->sorted_skills, sk);
		
		skill_entry_removeentry(&ch->sorted_skills, entry);
		return;
    }


	SKILL_GROUP *group = group_lookup(name);
	if (IS_VALID(group) && list_hasdata(ch->pcdata->group_known, group))
	{
		gn_remove(ch, group);
	}
}


void do_practice( CHAR_DATA *ch, char *argument )
{
	char buf[MAX_STRING_LENGTH];
	char arg[MSL];
	SKILL_DATA *skill;
	int learn;
	CHAR_DATA *mob;
	SKILL_ENTRY *entry;

	if (IS_NPC(ch))
		return;

	argument = one_argument( argument, arg );

	if (!arg[0]) {
		list_skill_entries(ch, "", TRUE, TRUE, TRUE);
		send_to_char("\n\r", ch);
		return;
	}

	for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room) {
		if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_PRACTICE))
			break;
	}

	if (!mob) {
		send_to_char("You can't do that here.\n\r", ch);
		return;
	}

	entry = skill_entry_findname(ch->sorted_skills, arg);
	if( !entry )
	{
		send_to_char("You can't practice that.\n\r", ch);
		return;
	}

	learn = skill_entry_learn(ch, entry);
	if( learn < 1 )
	{
		send_to_char("You can't practice that.\n\r", ch);
		return;
	}

	if (ch->practice <= 0) {
		send_to_char("You have no practice sessions left.\n\r", ch);
		return;
	}

#if 0
	if( entry->token )
	{
		int amount;
		// Token ability

		// $VICTIM: allows you to check who is teaching.. are they allowed to teach it?
		if(p_percent_trigger(NULL, NULL, NULL, entry->token, ch, mob, NULL, NULL, NULL, TRIG_PREPRACTICETOKEN, NULL,0,0,0,0,0))
		{
			send_to_char("You can't practice that.\n\r", ch);
			return;
		}

		if(p_percent_token_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, entry->token, TRIG_PREPRACTICE, NULL,0,0,0,0,0))
		{
			send_to_char("You can't practice that.\n\r", ch);
			return;
		}

		amount = token_skill_rating(entry->token);
		if( amount >= MAX_SKILL_LEARNABLE )
		{
			sprintf(buf, "There is nothing more that you can learn about %s here.\n\r", entry->token->name);
			send_to_char(buf, ch);
		}
		else
		{
			--ch->practice;
			ch->tempstore[0] = learn;
			p_percent_trigger(NULL, NULL, NULL, entry->token, ch, mob, NULL, NULL, NULL, TRIG_PRACTICETOKEN, NULL,0,0,0,0,0);
			learn = ch->tempstore[0];
			if( learn < 1 ) learn = 1;	// At this point, it should be a minimum of 1 skill rating

			amount += learn;
			if( amount > MAX_SKILL_LEARNABLE) amount = MAX_SKILL_LEARNABLE;

			if( entry->token->pIndexData->value[TOKVAL_SPELL_RATING] > 0 )
				entry->token->value[TOKVAL_SPELL_RATING] = amount * entry->token->pIndexData->value[TOKVAL_SPELL_RATING];
			else
				entry->token->value[TOKVAL_SPELL_RATING] = amount;


			if (amount < MAX_SKILL_LEARNABLE) {
				act("You practice $T.", ch, NULL, NULL, NULL, NULL, NULL, entry->token->name, TO_CHAR);
				act("$n practices $T.", ch, NULL, NULL, NULL, NULL, NULL, entry->token->name, TO_ROOM);
			} else {
				act("{WYou are now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, entry->token->name, TO_CHAR);
				act("{W$n is now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, entry->token->name, TO_ROOM);
			}
		}
	}
	else
#endif
	{
		// Standard ability
		skill = entry->skill;
		if( !can_practice(ch, skill) )
		{
			send_to_char("You can't practice that.\n\r", ch);
			return;
		}

		if(p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREPRACTICE, skill->name,0,0,0,0,0))
		{
			send_to_char("You can't practice that.\n\r", ch);
			return;
		}

		if (entry->rating >= MAX_SKILL_LEARNABLE) {
			sprintf(buf, "There is nothing more that you can learn about %s here.\n\r", entry->skill->name);
			send_to_char(buf, ch);
		} else {
			--ch->practice;
			ch->tempstore[0] = learn;
			p_percent_trigger(ch, NULL, NULL, NULL, ch, mob, NULL, NULL, NULL, TRIG_PRACTICE, entry->skill->name,0,0,0,0,0);
			learn = ch->tempstore[0];
			if( learn < 1 ) learn = 1;	// At this point, it should be a minimum of 1 skill rating

			entry->rating += learn;

			if (entry->rating < MAX_SKILL_LEARNABLE) {
				act("You practice $T.", ch, NULL, NULL, NULL, NULL, NULL, entry->skill->name, TO_CHAR);
				act("$n practices $T.", ch, NULL, NULL, NULL, NULL, NULL, entry->skill->name, TO_ROOM);
			} else {
				entry->rating = MAX_SKILL_LEARNABLE;
				act("{WYou are now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, entry->skill->name, TO_CHAR);
				act("{W$n is now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, entry->skill->name, TO_ROOM);
			}
		}

	}

#if 0
	sn = find_spell(ch, arg);

	if (sn <= 0 || !can_practice( ch, sn )) {
		if(!p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREPRACTICEOTHER,arg,0,0,0,0,0))
			send_to_char("You can't practice that.\n\r", ch);
		return;
	}

	// If it makes it this far, it is a standard spell

	if(p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREPRACTICE, skill_table[sn].name,0,0,0,0,0))
		return;

	if (ch->practice <= 0) {
		send_to_char("You have no practice sessions left.\n\r", ch);
		return;
	}

	if (ch->pcdata->learned[sn] >= 75) {
		sprintf(buf, "There is nothing more that you can learn about %s here.\n\r", skill_table[sn].name);
		send_to_char(buf, ch);
	} else {
		this_class = get_this_class(ch, sn);
		--ch->practice;
		ch->pcdata->learned[sn] += int_app[get_curr_stat(ch, STAT_INT)].learn /
			(skill_table[sn].rating[this_class] == 0 ? 10 : skill_table[sn].rating[this_class]);

		if (ch->pcdata->learned[sn] < 75) {
			act("You practice $T.", ch, NULL, NULL, NULL, NULL, NULL, skill_table[sn].name, TO_CHAR);
			act("$n practices $T.", ch, NULL, NULL, NULL, NULL, NULL, skill_table[sn].name, TO_ROOM);
		} else {
			ch->pcdata->learned[sn] = 75;
			act("{WYou are now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, skill_table[sn].name, TO_CHAR);
			act("{W$n is now learned at $T.{x", ch, NULL, NULL, NULL, NULL, NULL, skill_table[sn].name, TO_ROOM);
		}
	}
#endif
}

void do_rehearse( CHAR_DATA *ch, char *argument )
{
	char buf[MAX_STRING_LENGTH];
	char arg[MSL];
	int sn;
	CHAR_DATA *mob;
	bool wasbard;
	bool found = FALSE;

	if (IS_NPC(ch))
		return;

	if( ch->pcdata->sub_class_thief != CLASS_THIEF_BARD)
	{
		send_to_char( "You wouldn't even know how to rehearse.\n\r", ch );
		return;
	}

	wasbard = was_bard(ch);

	argument = one_argument( argument, arg );

	if (!arg[0]) {
		// List all the songs the can learn but don't know
		BUFFER *buffer = new_buf();

		add_buf(buffer, "You can learn the following songs: \n\r\n\r");
		add_buf(buffer, "{YSong Title                            Level {x\n\r");
		add_buf(buffer, "{Y--------------------------------------------x\n\r");

		for(sn = 0; (sn < MAX_SONGS) && music_table[sn].name; sn++)
		{
			if( !ch->pcdata->songs_learned[sn] &&
				(music_table[sn].level <= ch->level || wasbard))
			{
				sprintf(buf, "%-30s %10d\n\r",
					music_table[sn].name,
					music_table[sn].level);
				add_buf(buffer, buf);
				found = TRUE;
			}
		}

		if (found)
			page_to_char(buf_string(buffer), ch);
		else
			send_to_char( "There are no songs you can learn at this time.\n\r", ch );

		free_buf(buffer);
		return;
	}

	for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room) {
		if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_PRACTICE))
			break;
	}

	if (!mob) {
		send_to_char("You can't do that here.\n\r", ch);
		return;
	}

	for(sn = 0; (sn < MAX_SONGS) && music_table[sn].name; sn++)
	{
		if( !ch->pcdata->songs_learned[sn] &&
			(music_table[sn].level <= ch->level || wasbard) &&
			!str_prefix(arg, music_table[sn].name))
		{
			found = TRUE;
			break;
		}
	}

	if( !found )
	{
		send_to_char("You can't rehearse that.\n\r", ch);
		return;
	}

	if(p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREHEARSE, music_table[sn].name,0,0,0,0,0))
		return;

	if (ch->practice < 3) {
		send_to_char("You need 3 practices sessions to rehearse a song.\n\r", ch);
		return;
	}

	ch->pcdata->songs_learned[sn] = TRUE;
	skill_entry_addsong(ch, sn, NULL, SKILLSRC_NORMAL);
	ch->practice -= 3;

	act("You rehearse {W$T{x.", ch, NULL, NULL, NULL, NULL, NULL, music_table[sn].name, TO_CHAR);
	act("{+$n rehearses {x$T{x.", ch, NULL, NULL, NULL, NULL, NULL, music_table[sn].name, TO_ROOM);
}


// Is sn a racial skill for a given race?
bool is_racial_skill(int race, SKILL_DATA *skill)
{
    int i;

	if (!IS_VALID(skill)) return false;

    for (i = 0; pc_race_table[race].skills[i] != NULL; i++)
    {
	if (!str_cmp(skill->name, pc_race_table[race].skills[i]))
	    return TRUE;
    }

    return FALSE;
}


// This monster determines if a person can practice a skill
bool can_practice( CHAR_DATA *ch, SKILL_DATA *skill )
{
    int this_class;
	SKILL_ENTRY *entry;

    if (!IS_VALID(skill)) return false;

	entry = skill_entry_findskill(ch->sorted_skills, skill);
	if (!entry) return FALSE;

	if (!IS_SET(entry->flags, SKILL_PRACTICE)) return FALSE;

	if (entry->source != SKILLSRC_NORMAL) return TRUE;

    this_class = get_this_class(ch, skill);

    // Is it a racial skill ?
    if (!IS_NPC(ch))
    {
		if (is_racial_skill(ch->race, skill) &&
			(ch->level >= skill->skill_level[this_class] || had_skill(ch, skill)))
	    return TRUE;
    }

    // Does *everyone* get the skill? (such as hand to hand)
    if (is_global_skill(skill))
		return TRUE;

    // Have they had the skill in a previous subclass or class?
    if (had_skill(ch,skill))
		return TRUE;

    // Is the skill in the person's *current* class and the person
    // is of high enough level for it?
    if (has_class_skill(ch->pcdata->class_current, skill) &&
		ch->level >= skill->skill_level[this_class])
	return TRUE;

    // Ditto for subclass
    if (has_subclass_skill(ch->pcdata->sub_class_current, skill) &&
		ch->level >= skill->skill_level[this_class])
	return TRUE;

    // For old skills which already got practiced
    if (entry->rating > 2)
		return TRUE;

    return FALSE;
}

bool is_skill_spell(SKILL_DATA *skill)
{
	if (!IS_VALID(skill)) return false;

	return skill->isspell;
}

char *skill_level_value(SKILL_DATA *skill, int clazz)
{
	static char buf[4][MIL];
	static int i = 0;

	if (++i == 4) i = 0;
	if (!IS_VALID(skill) || clazz < 0 || clazz >= MAX_CLASS || skill->skill_level[clazz] > MAX_CLASS_LEVEL)
		strcpy(buf[i], "{D--");
	else
		sprintf(buf[i], "{W%d", skill->skill_level[clazz]);

	return buf[i];
}


char *skill_difficulty_value(SKILL_DATA *skill, int clazz)
{
#if 0
	static char *colors[11] = {
		"{[F555]",		// White
		"{[F353]",
		"{[F050]",		// Green
		"{[F350]",
		"{[F550]",		// Yellow
		"{[F540]",
		"{[F530]",		// Orange
		"{[F520]",
		"{[F510]",
		"{[F500]",		// Red
		"{[F300]"		// Dark Red - Excessive
	};
#else
	static char *colors[11] = {
		"`[F505]",		// Magenta
		"`[F353]",
		"`[F050]",		// Green
		"`[F350]",
		"`[F550]",		// Yellow
		"`[F540]",
		"`[F530]",		// Orange
		"`[F520]",
		"`[F510]",
		"`[F500]",		// Red
		"`[F300]"		// Dark Red - Excessive
	};
#endif
	static char buf[4][MIL];
	static int i = 0;

	if (++i == 4) i = 0;
	if (!IS_VALID(skill) || clazz < 0 || clazz >= MAX_CLASS || skill->rating[clazz] <= 0)
		strcpy(buf[i], "{F[222]--");
	else if (skill->rating[clazz] < 10)
		sprintf(buf[i], "%s%d", colors[skill->rating[clazz]], skill->rating[clazz]);
	else
		sprintf(buf[i], "%s%d", colors[10], skill->rating[clazz]);

	return buf[i];
}

// Has a person had a skill in their previous classes/subclasses?
bool had_skill( CHAR_DATA *ch, SKILL_DATA *skill )
{
    if (!IS_VALID(skill)) return false;

    if (IS_IMMORTAL(ch)) return true;

	SKILL_ENTRY *entry = skill_entry_findskill(ch->sorted_skills, skill);
	// Don't have the skill entry at all
	if (!entry) return false;

	if (entry->rating > 1) return true;

    if (!IS_REMORT(ch))
    {
		switch (ch->pcdata->class_current)
		{
		case CLASS_MAGE:
			return (has_subclass_skill( ch->pcdata->sub_class_cleric, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_thief, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_warrior, skill ) ||
					has_class_skill( get_profession(ch, CLASS_CLERIC), skill ) ||
					has_class_skill( get_profession(ch, CLASS_THIEF), skill ) ||
					has_class_skill( get_profession(ch, CLASS_WARRIOR), skill ));

	    case CLASS_CLERIC:
			return (has_subclass_skill( ch->pcdata->sub_class_mage, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_thief, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_warrior, skill ) ||
					has_class_skill( get_profession(ch, CLASS_MAGE), skill ) ||
					has_class_skill( get_profession(ch, CLASS_THIEF), skill ) ||
					has_class_skill( get_profession(ch, CLASS_WARRIOR), skill ));

	    case CLASS_THIEF:
			return (has_subclass_skill( ch->pcdata->sub_class_mage, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_cleric, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_warrior, skill ) ||
					has_class_skill( get_profession(ch, CLASS_MAGE), skill ) ||
					has_class_skill( get_profession(ch, CLASS_CLERIC), skill ) ||
					has_class_skill( get_profession(ch, CLASS_WARRIOR), skill ));

	    case CLASS_WARRIOR:
			return (has_subclass_skill( ch->pcdata->sub_class_mage, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_cleric, skill ) ||
					has_subclass_skill( ch->pcdata->sub_class_thief, skill ) ||
					has_class_skill( get_profession(ch, CLASS_MAGE), skill ) ||
					has_class_skill( get_profession(ch, CLASS_CLERIC), skill ) ||
					has_class_skill( get_profession(ch, CLASS_THIEF), skill ));
		}
    }
    else // for remorts
    {
		if (has_class_skill( CLASS_MAGE, skill ) ||
			has_class_skill( CLASS_CLERIC, skill ) ||
			has_class_skill( CLASS_THIEF, skill ) ||
			has_class_skill( CLASS_WARRIOR, skill ) ||
			has_subclass_skill( ch->pcdata->sub_class_mage, skill ) ||
			has_subclass_skill( ch->pcdata->sub_class_cleric, skill ) ||
			has_subclass_skill( ch->pcdata->sub_class_thief, skill ) ||
			has_subclass_skill( ch->pcdata->sub_class_warrior, skill ))
	    	return true;

		switch( ch->pcdata->sub_class_current )
		{
			case CLASS_MAGE_ARCHMAGE:
			case CLASS_MAGE_ILLUSIONIST:
			case CLASS_MAGE_GEOMANCER:
				return (has_subclass_skill( ch->pcdata->second_sub_class_cleric, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_thief, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_warrior, skill ));

			case CLASS_CLERIC_RANGER:
			case CLASS_CLERIC_ADEPT:
			case CLASS_CLERIC_ALCHEMIST:
				return (has_subclass_skill( ch->pcdata->second_sub_class_mage, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_thief, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_warrior, skill ));

			case CLASS_THIEF_HIGHWAYMAN:
			case CLASS_THIEF_NINJA:
			case CLASS_THIEF_SAGE:
				return (has_subclass_skill( ch->pcdata->second_sub_class_mage, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_cleric, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_warrior, skill ));

			case CLASS_WARRIOR_WARLORD:
			case CLASS_WARRIOR_DESTROYER:
			case CLASS_WARRIOR_CRUSADER:
				return (has_subclass_skill( ch->pcdata->second_sub_class_mage, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_cleric, skill ) ||
						has_subclass_skill( ch->pcdata->second_sub_class_thief, skill ));
		}
    }

	return false;
}


// Does *everyone* get the sn?
bool is_global_skill( SKILL_DATA *skill )
{
    if ( !IS_VALID(skill) || !IS_VALID(global_skills) ) return false;

	ITERATOR it;
	char *str;
	iterator_start(&it, global_skills->contents);
	while((str = (char *)iterator_nextdata(&it)))
	{
		if (!str_cmp(str, skill->name))
			break;
	}
	iterator_stop(&it);

    return str != NULL;
}


// Equalizes skills on a character. Removes ones they arn't supposed to have.
void update_skills( CHAR_DATA *ch )
{
#if 0
	// TODO: Deprecated for now.
    char buf[MSL];
    int sn;
    int reward = 0;

    for (sn = 0; sn < MAX_SKILL && skill_table[sn].name; sn++)
    {
	if (ch->pcdata->learned[sn] > 0 && !should_have_skill(ch, sn)
	&&  str_cmp(skill_table[sn].name, "reserved"))
	{
	    sprintf(buf, "You shouldn't have skill %s (reward of {Y%d{x quest points)\n\r",
	        skill_table[sn].name, 7 * ch->pcdata->learned[sn]);
	    send_to_char(buf, ch);
	    reward += (7 * ch->pcdata->learned[sn]);
	    ch->pcdata->learned[sn] = -ch->pcdata->learned[sn];
	}
    }

    if (reward > 0)
    {
	sprintf(buf, "Total reward is {Y%d{x quest points!\n\r", reward);
	send_to_char(buf, ch);
    }

    ch->questpoints += reward;
#endif
}


// Return true if the skill belongs to the subclass.
bool has_subclass_skill( int subclass, SKILL_DATA *skill )
{
    char *group_name;

    if (!IS_VALID(skill)) return false;

    if (subclass < CLASS_WARRIOR_MARAUDER || subclass > CLASS_THIEF_SAGE) return false;

    group_name = sub_class_table[subclass].default_group;

	SKILL_GROUP *group = group_lookup(group_name);
	if (IS_VALID(group))
	{
		ITERATOR sit;
		char *str;
		iterator_start(&sit, group->contents);
		while((str = (char *)iterator_nextdata(&sit)))
		{
			if (!str_cmp(skill->name, str))
				break;
		}
		iterator_stop(&sit);

		return str != NULL;
	}

    return false;
}


// Returns true if the class has the skill.
bool has_class_skill( int class, SKILL_DATA *skill )
{
    char *group_name;

    if (!IS_VALID(skill)) return false;

    if (class < CLASS_MAGE || class > CLASS_WARRIOR) return false;

    switch (class)
    {
		case CLASS_MAGE: 	group_name = "mage skills"; break;
		case CLASS_CLERIC: 	group_name = "cleric skills"; break;
		case CLASS_THIEF: 	group_name = "thief skills"; break;
		case CLASS_WARRIOR: group_name = "warrior skills"; break;
		default: 			group_name = "global skills"; break;
    }


	SKILL_GROUP *group = group_lookup(group_name);
	if (IS_VALID(group))
	{
		ITERATOR sit;
		char *str;
		iterator_start(&sit, group->contents);
		while((str = (char *)iterator_nextdata(&sit)))
		{
			if (!str_cmp(skill->name, str))
				break;
		}
		iterator_stop(&sit);

		return str != NULL;
	}

    return false;
}


// Should a player have a skill?
// Used for old players having skills they shouldn't.
bool should_have_skill( CHAR_DATA *ch, SKILL_DATA *skill )
{
    if (ch == NULL)
    {
    	bug("should_have_skill: null ch", 0 );
		return FALSE;
    }

    if (!IS_VALID(skill))
    {
    	bug("should_have_skill: bad sn", 0 );
		return FALSE;
    }

    if (is_racial_skill(ch->race, skill))
    	return TRUE;

    if (is_global_skill(skill))
		return TRUE;

    if (has_class_skill( get_profession(ch, CLASS_MAGE), skill )
    ||  has_class_skill( get_profession(ch, CLASS_CLERIC), skill )
    ||  has_class_skill( get_profession(ch, CLASS_THIEF), skill )
    ||  has_class_skill( get_profession(ch, CLASS_WARRIOR), skill ))
		return TRUE;

    if (has_subclass_skill( ch->pcdata->sub_class_mage, skill )
    ||  has_subclass_skill( ch->pcdata->sub_class_cleric, skill )
    ||  has_subclass_skill( ch->pcdata->sub_class_thief, skill )
    ||  has_subclass_skill( ch->pcdata->sub_class_warrior, skill ))
		return TRUE;

    if (has_subclass_skill( ch->pcdata->second_sub_class_mage, skill )
    ||  has_subclass_skill( ch->pcdata->second_sub_class_cleric, skill )
    ||  has_subclass_skill( ch->pcdata->second_sub_class_thief, skill )
    ||  has_subclass_skill( ch->pcdata->second_sub_class_warrior, skill ))
		return TRUE;

    return FALSE;
}

char *skill_entry_name (SKILL_ENTRY *entry)
{
	if( entry ) {
		if ( IS_VALID(entry->token) ) return entry->token->name;

		if ( entry->skill ) return entry->skill->name;
		if ( entry->song >= 0 ) return music_table[entry->song].name;
	}

	return &str_empty[0];
}

int skill_entry_compare (SKILL_ENTRY *a, SKILL_ENTRY *b)
{
	char *an = skill_entry_name(a);
	char *bn = skill_entry_name(b);
	int cmp = str_cmp(an, bn);

	/*
	if( !cmp ) {
		// TODO: Fix comparison
		if( (a->sn > 0 || a->song >= 0) && IS_VALID(b->token)) cmp = -1;
		else if( IS_VALID(a->token) && (b->sn > 0 || b->song >= 0)) cmp = 1;
	}
	*/

	//log_stringf("skill_entry_compare: a(%s) %s b(%s)", an, ((cmp < 0) ? "<" : ((cmp > 0) ? ">" : "==")), bn);

	return cmp;
}

SKILL_ENTRY *skill_entry_insert (SKILL_ENTRY **list, SKILL_DATA *skill, int song, TOKEN_DATA *token, long flags, char source)
{
	SKILL_ENTRY *cur, *prev, *entry;

	entry = new_skill_entry();
	if( !entry ) return NULL;

	entry->skill = skill;
	entry->token = token;
	entry->song = song;
	entry->source = source;
	entry->flags = flags;

	if (IS_SET(entry->flags, SKILL_SPELL)) {
		entry->isspell = true;
	}

	// Link the token to the skill
	if( IS_VALID(token) )
		token->skill = entry;

	cur = *list;
	prev = NULL;

	while( cur ) {
		if( skill_entry_compare(entry, cur) < 0 )
			break;

		prev = cur;
		cur = cur->next;
	}

	if( prev )
		prev->next = entry;
	else
		*list = entry;
	entry->next = cur;

	return entry;
}

void skill_entry_remove (SKILL_ENTRY **list, SKILL_DATA *skill, int song, TOKEN_DATA *token, bool isspell)
{
	SKILL_ENTRY *cur, *prev;

	cur = *list;
	prev = NULL;
	while(cur) {
		if ((( IS_VALID(token) && (cur->token == token) ) ||
			 ( IS_VALID(skill) && (cur->skill == skill) ) ||
			 ( (song >= 0) && (cur->song == song) )) && 
			(!IS_SET(cur->flags, SKILL_SPELL) == !isspell) ) {

			if(prev)
				prev->next = cur->next;
			else
				*list = cur->next;

			// Unlink the token from the skill
			if( IS_VALID(cur->token) )
			{
				cur->token->skill = NULL;

				token_from_char(cur->token);
				free_token(cur->token);
			}

			free_skill_entry(cur);
			return;
		}

		prev = cur;
		cur = cur->next;
	}
}

void skill_entry_removeentry (SKILL_ENTRY **list, SKILL_ENTRY *entry)
{
	SKILL_ENTRY *cur, *prev;

	cur = *list;
	prev = NULL;
	while(cur) {
		if ( cur == entry ) {

			if(prev)
				prev->next = cur->next;
			else
				*list = cur->next;

			// Unlink the token from the skill
			if( IS_VALID(cur->token) )
			{
				cur->token->skill = NULL;

				token_from_char(cur->token);
				free_token(cur->token);
			}

			free_skill_entry(cur);
			return;
		}

		prev = cur;
		cur = cur->next;
	}
}

SKILL_ENTRY *skill_entry_findname( SKILL_ENTRY *list, char *str )
{
	int count;
	char name[MIL];

	count = number_argument(str, name);

	if(count < 1) count = 1;

	while (list) {
		// Name match, until count predecrements to 0
		if( !str_prefix(str, skill_entry_name(list)) && !--count )
			return list;

		list = list->next;
	}

	return NULL;
}

SKILL_ENTRY *skill_entry_findskill( SKILL_ENTRY *list, SKILL_DATA *skill )
{
	if( !IS_VALID(skill) ) return NULL;

	while (list && list->skill != skill)
		list = list->next;

	return list;
}

SKILL_ENTRY *skill_entry_findsong( SKILL_ENTRY *list, int song )
{
	if( song < 0 ) return NULL;

	while (list && list->song != song)
		list = list->next;

	return list;
}

SKILL_ENTRY *skill_entry_findtoken( SKILL_ENTRY *list, TOKEN_DATA *token )
{
	if( !IS_VALID(token) ) return NULL;

	while (list && list->token != token)
		list = list->next;

	return list;
}

SKILL_ENTRY *skill_entry_findtokenindex( SKILL_ENTRY *list, TOKEN_INDEX_DATA *token_index )
{
	while (list && (!list->token || list->token->pIndexData != token_index))
		list = list->next;

	return list;
}

SKILL_ENTRY *skill_entry_addskill (CHAR_DATA *ch, SKILL_DATA *skill, TOKEN_DATA *token, char source, long flags)
{
	if( !ch ) return NULL;

	if (skill->token && skill->token->type != TOKEN_SKILL) return NULL;

	return skill_entry_insert( &ch->sorted_skills, skill, -1, token, (flags & ~SKILL_SPELL), source );
}

SKILL_ENTRY *skill_entry_addspell (CHAR_DATA *ch, SKILL_DATA *skill, TOKEN_DATA *token, char source, long flags)
{
	if( !ch ) return NULL;

	if (skill->token && skill->token->type != TOKEN_SPELL) return NULL;

	return skill_entry_insert( &ch->sorted_skills, skill, -1, token, (SKILL_SPELL | (flags & ~SKILL_SPELL)), source );
}

SKILL_ENTRY *skill_entry_addsong (CHAR_DATA *ch, int song, TOKEN_DATA *token, char source)
{
	if( !ch ) return NULL;

	if( song < 0 && (!token || token->type != TOKEN_SONG)) return NULL;

	return skill_entry_insert( &ch->sorted_songs, 0, song, token, 0, source );
}

void skill_entry_removeskill (CHAR_DATA *ch, SKILL_DATA *skill)
{
	if( !ch ) return;

	if (skill->token && skill->token->type != TOKEN_SKILL) return;

	skill_entry_remove( &ch->sorted_skills, skill, -1, NULL, FALSE );
}

void skill_entry_removespell (CHAR_DATA *ch, SKILL_DATA *skill)
{
	if( !ch ) return;

	if (skill->token && skill->token->type != TOKEN_SPELL) return;

	skill_entry_remove( &ch->sorted_skills, skill, -1, NULL, TRUE );
}

void skill_entry_removesong (CHAR_DATA *ch, int song, TOKEN_DATA *token)
{
	if( !ch ) return;

	if( song < 0 && (!token || token->type != TOKEN_SONG)) return;

	skill_entry_remove( &ch->sorted_songs, NULL, song, token, FALSE );
}

int token_skill_rating( TOKEN_DATA *token)
{
	int percent;

	// Make sure the tokens are skill/spell tokens
	if( ((token->type == TOKEN_SKILL) || (token->type == TOKEN_SPELL)) &&
		(token->type != token->pIndexData->type))
		return 0;

	// Value 0
	// RATING is the UNIT rating (how much is 1%)
	// So, if the rating is 50, you need 5000 to have 100%
	if(token->pIndexData->value[TOKVAL_SPELL_RATING] > 0)
		percent = token->value[TOKVAL_SPELL_RATING] / token->pIndexData->value[TOKVAL_SPELL_RATING];
	else
		percent = token->value[TOKVAL_SPELL_RATING];

	return percent;
}

int token_skill_mana(TOKEN_DATA *token)
{
	// Make sure the tokens are skill/spell tokens
	if( token->type != TOKEN_SPELL || token->pIndexData->type != TOKEN_SPELL)
		return 0;

	return token->value[TOKVAL_SPELL_MANA];
}

int skill_entry_rating (CHAR_DATA *ch, SKILL_ENTRY *entry)
{

	return entry->rating;

/*
	if( IS_VALID(entry->token) ) {
		return token_skill_rating(entry->token);
	} else if( entry->sn > 0) {
		if( IS_NPC(ch) ) {
			if ((skill_table[entry->sn].race != -1 && ch->race != skill_table[entry->sn].race) || ch->tot_level < 10)
				return 0;

			return mob_skill_table[ch->tot_level];
		} else
			return ch->pcdata->learned[entry->sn];
	} else
		return 0;
		*/
}

int skill_entry_mod(CHAR_DATA *ch, SKILL_ENTRY *entry)
{
	return entry->mod_rating;

	/*
	if( IS_VALID(entry->token) ) {
		return 0;	// No mods for TOKEN entries yet!
	} else if( entry->sn > 0) {
		if( IS_NPC(ch) ) return 0;

		return ch->pcdata->mod_learned[entry->sn];
	} else
		return 0;
	*/
}

int skill_entry_level (CHAR_DATA *ch, SKILL_ENTRY *entry)
{
	int this_class;

	if( IS_IMMORTAL(ch) )
		return LEVEL_IMMORTAL;

	if (IS_VALID(entry->skill))
	{
		this_class = get_this_class(ch, entry->skill);

		// Not ready yet
		if( !had_skill( ch, entry->skill ) && (ch->level < entry->skill->skill_level[this_class]) )
			return -entry->skill->skill_level[this_class];

		return entry->skill->skill_level[this_class];
	} else if( entry->song >= 0 ) {
		return music_table[entry->song].level;
	} else
		return 0;
}

int skill_entry_mana (CHAR_DATA *ch, SKILL_ENTRY *entry)
{
	if( IS_VALID(entry->skill)) {
		return entry->skill->cast_mana;
	} else if( entry->song >= 0) {
		return music_table[entry->song].mana;
	} else
		return 0;
}

int skill_entry_learn (CHAR_DATA *ch, SKILL_ENTRY *entry)
{
	int amount = 0;

	if(!IS_SET(entry->flags, SKILL_PRACTICE)) return 0;

	if( IS_VALID(entry->skill)) {
		int this_class = get_this_class(ch, entry->skill);
		amount = (entry->skill->rating[this_class] == 0 ? 10 : entry->skill->rating[this_class]);
	}

	if(amount > 0)
		return int_app[get_curr_stat(ch, STAT_INT)].learn / amount;

	else
		return 0;
}

int skill_entry_target(CHAR_DATA *ch, SKILL_ENTRY *entry)
{
	if (IS_VALID(entry->skill))
		return entry->skill->target;
	else if (entry->song >= 0)
		return music_table[entry->song].target;

	return TARGET_NONE;
}


void remort_player(CHAR_DATA *ch, int remort_class)
{
	const struct sub_class_type *class_info;
	OBJ_DATA *obj;
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MSL];
	int i;

	// Safeguards
	if( IS_NPC(ch) ) return;

	// Must be a player, you twat!
	if( IS_IMMORTAL(ch) ) return;

	// Must not be remort already
	//  - well, you could always be a masochist and want to level again
	if( IS_REMORT(ch) ) return;

	// Must be max level
	if( ch->tot_level != LEVEL_HERO ) return;

	// Only remort classes
	if( remort_class < CLASS_WARRIOR_WARLORD || remort_class >= MAX_SUB_CLASS ) return;

	if( !sub_class_table[remort_class].remort ) return;

	class_info = &sub_class_table[remort_class];

    i = 0;
    ch->race = get_remort_race(ch);
    sprintf(buf2, "%s", pc_race_table[ch->race].name);
    while (buf2[i] != '\0')
    {
		buf2[i] = UPPER(buf2[i]);
		i++;
    }

    if (ch->alignment < 0) {
        sprintf(buf, "{RHoly statues cry tears of blood and the sillhouettes "
		      "of winged horrors appear in the sky.{X\n\r{RA new %s has been born!{x\n\r", buf2);

		ch->alignment = -1000;

		send_to_char("Your mortal essence crumbles as you embrace your fate.\n\r", ch);
		send_to_char("You welcome the dark power as it flows through your divine veins.\n\r", ch);
		send_to_char("A dark influence clouds all that you once knew; your lifeless body\n\r", ch);
		send_to_char("lies slouched in front of you as part of you is torn into the Abyss.\n\r", ch);
		send_to_char("You feel complete, and wielding unfathomable power, you know you can\n\r", ch);
		send_to_char("manipulate it to suit your darkest desires.\n\r", ch);
    } else if (ch->alignment > 0) {
		sprintf(buf, "{WBrilliant white light radiates down from the heavens and thunder rolls through the valleys.\n\r"
		             "{WA new %s has been born!{x\n\r", buf2);

		ch->alignment = 1000;

		send_to_char("Your mortal essence shines brightly, blinding your eyes.\n\r", ch);
		send_to_char("Images flash before you: sadness, grief, terror and hatred.\n\r", ch);
		send_to_char("Your life is played to you, from the beginning to the present.\n\r", ch);
		send_to_char("Your veins flow with the divine influence as you stand before your\n\r", ch);
		send_to_char("lifeless mortal vessel. It becomes clear to you that you have been\n\r", ch);
		send_to_char("reborn a divine power.\n\r", ch);
    } else {
		sprintf(buf, "{CThe cosmic energies of the world shift and the clouds speed overhead.{x\n\r"
					 "{CA new %s has been born!{x\n\r", buf2);

		ch->alignment = 0;
    }

    gecho(buf);

    /* take off equipment*/
    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
		if (obj->wear_loc != WEAR_NONE)
		    unequip_char(ch, obj, FALSE);
    }

    /* take off remaining affects*/
    while (ch->affected)
		affect_remove(ch, ch->affected);

    /* lower their stats significantly*/
    for (i = 0; i < MAX_STATS; i++) {
		int val = ch->perm_stat[i] - number_range(4,6);
		set_perm_stat(ch, i, UMAX(val, 13));
	}

	ch->affected_by_perm[0] = race_table[ch->race].aff;
	ch->affected_by_perm[1] = race_table[ch->race].aff2;
    ch->imm_flags_perm = race_table[ch->race].imm;
    ch->res_flags_perm = race_table[ch->race].res;
    ch->vuln_flags_perm = race_table[ch->race].vuln;

    ch->form        = race_table[ch->race].form;
    ch->parts       = race_table[ch->race].parts;
    ch->lostparts	= 0;	// Restore anything lost

    /* add skills for remort race*/
    for (i = 0; pc_race_table[ch->race].skills[i] != NULL; i++)
		group_add(ch,pc_race_table[ch->race].skills[i],FALSE);

    ch->pcdata->hit_before  = ch->pcdata->perm_hit;
    ch->pcdata->mana_before = ch->pcdata->perm_mana;
    ch->pcdata->move_before = ch->pcdata->perm_move;

    ch->pcdata->perm_hit  = 20;
    ch->pcdata->perm_mana = 20;
    ch->pcdata->perm_move = 20;

    ch->max_hit  = 20;
    ch->max_mana = 20;
    ch->max_move = 20;

    ch->hit  = 20;
    ch->mana = 20;
    ch->move = 20;

    ch->tot_level = 1;
    ch->level = 1;

	// Reset base affects - will reset affected_by, affected_by2, imm_flags, res_flags and vuln_flags
    affect_fix_char(ch);

    char_from_room(ch);
    char_to_room(ch, room_index_school);

	ch->pcdata->class_current = class_info->class;
    ch->pcdata->sub_class_current = remort_class;

	switch(class_info->class) {
	case CLASS_MAGE:
		ch->pcdata->second_class_mage = CLASS_MAGE;
	    ch->pcdata->second_sub_class_mage = remort_class;
		break;

	case CLASS_CLERIC:
		ch->pcdata->second_class_cleric = CLASS_CLERIC;
	    ch->pcdata->second_sub_class_cleric = remort_class;
		break;

	case CLASS_THIEF:
		ch->pcdata->second_class_thief = CLASS_THIEF;
	    ch->pcdata->second_sub_class_thief = remort_class;
		break;

	case CLASS_WARRIOR:
		ch->pcdata->second_class_warrior = CLASS_WARRIOR;
	    ch->pcdata->second_sub_class_warrior = remort_class;
		break;
	}

    group_add(ch, class_table[ch->pcdata->class_current].base_group, TRUE);
    group_add(ch, sub_class_table[ch->pcdata->sub_class_current].default_group, TRUE);
    ch->exp = 0;

    sprintf(buf2, sub_class_table[ch->pcdata->sub_class_current].name[ch->sex]);
    buf2[0] = UPPER(buf2[0]);
    sprintf(buf, "All congratulate %s, who is now a%s %s!",
        ch->name, (buf2[0] == 'A' || buf2[0] == 'I' || buf2[0] == 'E' || buf2[0] == 'U'
	    || buf2[0] == 'O') ? "n" : "", buf2);
    crier_announce(buf);
    double_xp(ch);

	// Reset here since an immortal can still remort a player while they still have this question up
	ch->remort_question = FALSE;

	p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REMORT, NULL,0,0,0,0,0);

    for (obj = ch->carrying; obj != NULL;)
    {
		OBJ_DATA *obj_next = obj->next_content;
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REMORT, NULL,0,0,0,0,0);
		obj = obj_next;
    }

}



// SKILL EDITOR





SKEDIT( skedit_install )
{
	char buf[MSL];
	SKILL_DATA *skill;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit install {Rsource{x skill|spell <name>\n\r", ch);
		send_to_char("         skedit install {Rtoken{x <widevnum>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "source"))
	{
		argument = one_argument(argument, arg);
		bool isspell;

		if (!str_prefix(arg, "spell"))
			isspell = true;
		else if (!str_prefix(arg, "skill"))
			isspell = false;
		else
		{
			send_to_char("Syntax:  skedit source {Rskill|spell{x <name>\n\r", ch);
			send_to_char("Please specify whether this is a {Yskill{x or {Gspell{x.\n\r", ch);
			return false;
		}

		// Install a SOURCE skill/spell
		smash_tilde(argument);
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skedit install source skill|spell {R<name>{x\n\r", ch);
			send_to_char("Please specify a name.\n\r", ch);
			return false;
		}

		// Make sure the name doesn't exist already.
		skill = get_skill_data(argument);
		if (IS_VALID(skill))
		{
			send_to_char("That name is already in use.\n\r", ch);
			return false;
		}

		skill = new_skill_data();
		skill->uid = ++top_skill_uid;
		skill->name = str_dup(argument);
		skill->isspell = isspell;
		skill->token = NULL;
		
		insert_skill(skill);

		sprintf(buf, "Skill {W%s{x installed.\n\r", skill->name);
		send_to_char(buf, ch);

		ch->desc->pEdit = skill;
		ch->desc->editor = ED_SKEDIT;
		return true;
	}

	if (!str_prefix(arg, "token"))
	{
		WNUM wnum;

		if (!parse_widevnum(argument, NULL, &wnum))
		{
			send_to_char("Syntax:  skedit install token {R<widevnum>{x\n\r", ch);
			send_to_char("Please provide a valid widevnum.\n\r", ch);
			return false;
		}

		TOKEN_INDEX_DATA *token = get_token_index_wnum(wnum);
		if (!token)
		{
			send_to_char("That token does not exist.\n\r", ch);
			return false;
		}

		bool isspell;
		if (token->type == TOKEN_SPELL)
			isspell = true;
		else if (token->type == TOKEN_SKILL)
			isspell = false;
		else
		{
			send_to_char("That token is neither a {Yskill{x nor {Gspell{x token.\n\r", ch);
			return false;
		}

		skill = get_skill_data(token->name);
		if (IS_VALID(skill))
		{
			send_to_char("That name is already in use.\n\r", ch);
			return false;
		}

		skill = new_skill_data();
		skill->uid = ++top_skill_uid;
		skill->name = str_dup(token->name);
		skill->token = token;
		skill->isspell = isspell;
		
		insert_skill(skill);

		sprintf(buf, "Skill {W%s{x installed.\n\r", skill->name);
		send_to_char(buf, ch);

		ch->desc->pEdit = skill;
		ch->desc->editor = ED_SKEDIT;
		return true;
	}

	skedit_install(ch, "");
	return false;
}

SKEDIT( skedit_list )
{
	BUFFER *buffer = new_buf();
	char buf[MSL];

	sprintf(buf, "%3s %-20.20s %-20.20s\n\r",
		"###", "Name", "Display");
	add_buf(buffer, buf);
	sprintf(buf, "%3s %-20.20s %-20.20s\n\r",
		"===", "====================", "====================");
	add_buf(buffer, buf);

	int i = 0;
	ITERATOR it;
	SKILL_DATA *skill;
	iterator_start(&it, skills_list);
	while((skill = (SKILL_DATA *)iterator_nextdata(&it)))
	{
		sprintf(buf, "%3d {%c%-20.20s{x %-20.20s\n\r", ++i,
			(skill->token ? 'G' : 'Y'), skill->name, skill->display);
		add_buf(buffer, buf);
	}
	iterator_stop(&it);

	sprintf(buf, "%3s %-20.20s %-20.20s\n\r",
		"---", "--------------------", "--------------------");
	add_buf(buffer, buf);
	sprintf(buf, "Total: %d\n\r", i);
	add_buf(buffer, buf);

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
}


void skedit_show_trigger(BUFFER *buffer, LLIST **progs, int trigger, char *label)
{
	char buf[MSL];

	PROG_LIST *pr = find_trigger_data(progs, trigger, 1);

	if (pr)
		sprintf(buf, "   + %-15s {W%ld{x#{W%ld{x\n\r", label, pr->script->area->uid, pr->script->vnum);
	else
		sprintf(buf, "   + %-15s {D(unset){x\n\r", label);
	add_buf(buffer, buf);
}

void skedit_show_function(BUFFER *buffer, char *name, char *label)
{
	char buf[MSL];

	if (name)
		sprintf(buf, "   + %-15s {W%s{x\n\r", label, name);
	else
		sprintf(buf, "   + %-15s {D(unset){x\n\r", label);
	add_buf(buffer, buf);
}

SKEDIT( skedit_show )
{
	char buf[MSL];
	BUFFER *buffer;
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	buffer = new_buf();

	if (is_skill_spell(skill))
		sprintf(buf, "Spell: {W%s {x({W%d{x)\n\r", skill->name, skill->uid);
	else
		sprintf(buf, "Skill: {W%s {x({W%d{x)\n\r", skill->name, skill->uid);
	add_buf(buffer, buf);

	if (IS_NULLSTR(skill->display))
		sprintf(buf, "Display String:   {D(unset){x\n\r");
	else
		sprintf(buf, "Display String:   {W%s{x\n\r", skill->display);
	add_buf(buffer, buf);

	if (skill->pgsn)
		sprintf(buf, "GSN:              {W%s{x\n\r", gsn_to_name(skill->pgsn));
	else
		sprintf(buf, "GSN:              {D(unset){x\n\r");
	add_buf(buffer, buf);


	if (skill->token)
	{
		sprintf(buf, "Token:            {W%s {x({W%ld{x#{W%ld{x)\n\r", skill->token->name, skill->token->area->uid, skill->token->vnum);
		add_buf(buffer, buf);

		add_buf(buffer, " - Artificing:\n\r");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_BREW, "Brew:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_IMBUE, "Imbue:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_INK, "Ink:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_PREBREW, "Prebrew:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_PREIMBUE, "Preimbue:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_PREINK, "Preink:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_PRESCRIBE, "Prescribe:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_SCRIBE, "Scribe:");

		add_buf(buffer, " - Actions:\n\r");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_BRANDISH, "Brandish:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_PRESPELL, "Prespell:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_QUAFF, "Quaff:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_RECITE, "Recite:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_SPELL, "Spell:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_TOUCH, "Touch:");
		skedit_show_trigger(buffer, skill->token->progs, TRIG_TOKEN_ZAP, "Zap:");
	}
	else
	{
		add_buf(buffer, "Functions:\n\r");
		add_buf(buffer, " - Artificing:\n\r");
		skedit_show_function(buffer, brew_func_display(skill->brew_fun), "Brew:");
		skedit_show_function(buffer, NULL, "Imbue:");
		skedit_show_function(buffer, ink_func_display(skill->ink_fun), "Ink:");
		skedit_show_function(buffer, prebrew_func_display(skill->prebrew_fun), "Prebrew:");
		skedit_show_function(buffer, NULL, "Preimbue:");
		skedit_show_function(buffer, preink_func_display(skill->preink_fun), "Preink:");
		skedit_show_function(buffer, prescribe_func_display(skill->prescribe_fun), "Prescribe:");
		skedit_show_function(buffer, scribe_func_display(skill->scribe_fun), "Scribe:");

		add_buf(buffer, " - Actions:\n\r");
		skedit_show_function(buffer, NULL, "Brandish:");
		skedit_show_function(buffer, prespell_func_display(skill->prespell_fun), "Prespell:");
		skedit_show_function(buffer, quaff_func_display(skill->quaff_fun), "Quaff:");
		skedit_show_function(buffer, recite_func_display(skill->recite_fun), "Recite:");
		skedit_show_function(buffer, spell_func_display(skill->spell_fun), "Spell:");
		skedit_show_function(buffer, touch_func_display(skill->touch_fun), "Touch:");
		skedit_show_function(buffer, NULL, "Zap:");
	}

	add_buf(buffer, "\n\rLevels:\n\r");
	sprintf(buf, " - Cleric:  %-7s{x    - Mage:    %s{x\n\r", skill_level_value(skill,CLASS_CLERIC), skill_level_value(skill,CLASS_MAGE)); add_buf(buffer, buf);
	sprintf(buf, " - Thief:   %-7s{x    - Warrior: %s{x\n\r", skill_level_value(skill,CLASS_THIEF), skill_level_value(skill,CLASS_WARRIOR)); add_buf(buffer, buf);

	add_buf(buffer, "\n\rDifficulties:\n\r");
	sprintf(buf, " - Cleric:  %-12s{x    - Mage:    %s{x\n\r", skill_difficulty_value(skill, CLASS_CLERIC), skill_difficulty_value(skill, CLASS_MAGE)); add_buf(buffer, buf);
	sprintf(buf, " - Thief:   %-12s{x    - Warrior: %s{x\n\r", skill_difficulty_value(skill, CLASS_THIEF), skill_difficulty_value(skill, CLASS_WARRIOR)); add_buf(buffer, buf);

	add_buf(buffer, "\n\rInks:\n\r");
	for(int i = 0; i < 3; i++)
	{
		sprintf(buf, " %d) [%2d] %s\n\r", i + 1, skill->inks[i][1], flag_string(catalyst_types, skill->inks[i][0]));
		add_buf(buffer, buf);
	}

	add_buf(buffer, "\n\rValues:\n\r");
	for(int i = 0; i < MAX_SKILL_VALUES; i++)
	{
		char name[MIL];
		if (IS_NULLSTR(skill->valuenames[i]))
			sprintf(name, "Value %d:", i+1);
		else
			sprintf(name, "%s:", skill->valuenames[i]);
		sprintf(buf, "%-20s %d\n\r", name, skill->values[i]);
		add_buf(buffer, buf);
	}

	sprintf(buf, "\n\rTarget:           %s\n\r", flag_string(spell_target_types, skill->target)); add_buf(buffer, buf);
	sprintf(buf, "Minimum Position: %s\n\r", flag_string(position_flags, skill->minimum_position)); add_buf(buffer, buf);

	if (skill->race > 0)
		sprintf(buf, "Racial Skill:     Yes (%s)\n\r", race_table[skill->race].name);
	else
		sprintf(buf, "Racial Skill:     No\n\r");
	add_buf(buffer, buf);

	sprintf(buf, "Casting Mana:     %d\n\r", skill->cast_mana); add_buf(buffer, buf);
	sprintf(buf, "Brewing Mana:     %d\n\r", skill->brew_mana); add_buf(buffer, buf);
	sprintf(buf, "Scribing Mana:    %d\n\r", skill->scribe_mana); add_buf(buffer, buf);
	sprintf(buf, "Imbuing Mana:     %d\n\r", skill->imbue_mana); add_buf(buffer, buf);
	sprintf(buf, "Beats:            %d\n\r", skill->beats); add_buf(buffer, buf);

	sprintf(buf, "Noun Damage:      %s\n\r", IS_NULLSTR(skill->noun_damage) ? "{D(unset){x" : skill->noun_damage); add_buf(buffer, buf);
	sprintf(buf, "Wear Off Message: %s\n\r", IS_NULLSTR(skill->msg_off) ? "{D(unset){x" : skill->msg_off); add_buf(buffer, buf);
	sprintf(buf, "Object Message:   %s\n\r", IS_NULLSTR(skill->msg_obj) ? "{D(unset){x" : skill->msg_obj); add_buf(buffer, buf);
	sprintf(buf, "Dispel Message:   %s\n\r", IS_NULLSTR(skill->msg_disp) ? "{D(unset){x" : skill->msg_disp); add_buf(buffer, buf);

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
}

SKEDIT( skedit_name )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	smash_tilde(argument);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit name <name>\n\r", ch);
		send_to_char("Please specify a name.\n\r", ch);
		return false;
	}

	free_string(skill->name);
	skill->name = str_dup(argument);

	send_to_char("Skill name set.\n\r", ch);
	return true;
}

SKEDIT( skedit_display )
{
	char arg[MIL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit display {Rset{x <display string>\n\r", ch);
		send_to_char("Syntax:  skedit display {Rclear{x\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "set"))
	{
		smash_tilde(argument);
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skedit display set {R<display string>{x\n\r", ch);
			send_to_char("Please specify a display string.\n\r", ch);
			return false;
		}

		free_string(skill->display);
		skill->display = str_dup(argument);

		send_to_char("Skill display string set.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		free_string(skill->display);
		skill->display = NULL;
		send_to_char("Skill display string cleared.\n\r", ch);
		return true;
	}

	skedit_display(ch, "");
	return false;
}

// TODO: Implement or move into a skgedit
SKEDIT( skedit_group )
{
	/*
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);
	*/
	send_to_char("Not implemented yet.\n\r", ch);

	return false;
}

// Remove all instances of the trigger
void __token_remove_trigger(TOKEN_INDEX_DATA *token, int type)
{
	if (token->progs)
	{
		PROG_LIST *trigger;
		ITERATOR it;
		struct trigger_type *tt = get_trigger_type_bytype(type);

		iterator_start(&it, token->progs[tt->slot]);
		while(( trigger = (PROG_LIST *)iterator_nextdata(&it) ))
		{
			if (trigger->trig_type == type)
			{
				iterator_remcurrent(&it);
				trigger_type_delete_use(tt);
				free_trigger(trigger);
			}
		}
		iterator_stop(&it);
	}
}

bool __token_add_trigger(TOKEN_INDEX_DATA *token, int type, char *phrase, SCRIPT_DATA *script)
{
	// Make sure the token has prog bank
	if (!token->progs) token->progs = new_prog_bank();

	if (!token->progs) return false;	// Something.. bad.. happened.

	struct trigger_type *tt = get_trigger_type_bytype(type);

	PROG_LIST *trigger = new_trigger();
	trigger->wnum.pArea = script->area;
	trigger->wnum.vnum = script->vnum;
	trigger->trig_type = type;
	trigger->trig_phrase = str_dup(phrase);
	trigger->trig_number = atoi(trigger->trig_phrase);
	trigger->numeric = is_number(trigger->trig_phrase);
	trigger->script = script;

	list_appendlink(token->progs[tt->slot], trigger);
	trigger_type_add_use(tt);
	return true;
}

#define SKEDIT_FUNC(f, p, t)		\
SKEDIT( skedit_##f##func )	\
{ \
	SKILL_DATA *skill; \
\
	EDIT_SKILL(ch, skill); \
\
	if (skill->token) \
	{ \
		/* TOKEN mode */ \
		WNUM wnum; \
\
		/* Allow wnum shortcutting using the token's area */ \
		if (!parse_widevnum(argument, skill->token->area, &wnum)) \
		{ \
			send_to_char("Syntax:  skedit " #f " {R<widevnum>{x\n\r", ch); \
			send_to_char("Please specify a widevnum for the token SPELL trigger.\n\r", ch); \
			return false; \
		} \
\
		/* Get the script */ \
		SCRIPT_DATA *script = get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG); \
		if (!script) \
		{ \
			send_to_char("No such token script by that widevnum.\n\r", ch); \
			return false; \
		} \
\
		/* Remove any triggers from token. */ \
		__token_remove_trigger(skill->token, TRIG_##p); \
\
		/* Add trigger to token. */ \
		if (!__token_add_trigger(skill->token, TRIG_##p, "100", script)) \
		{ \
			send_to_char("Something went wrong adding " #p " trigger to token.\n\r", ch); \
			return false; \
		} \
\
		/* Mark area as changed. */ \
		SET_BIT(skill->token->area->area_flags, AREA_CHANGED); \
		send_to_char(#p " trigger added to spell token.\n\r", ch); \
		return true; \
	} \
	else \
	{ \
		if (argument[0] == '\0') \
		{ \
			send_to_char("Syntax:  skedit " #f " {R<function>{x\n\r", ch); \
			send_to_char("Invalid " #f " function.  Use '? " #f "_func' for a list of functions.\n\r", ch); \
			return false; \
		} \
\
		t *func = f##_func_lookup(argument); \
		if(!func) \
		{ \
			send_to_char("Syntax:  skedit " #f " {R<function>{x\n\r", ch); \
			send_to_char("Invalid " #f " function.  Use '? " #f "_func' for a list of functions.\n\r", ch); \
			return false; \
		} \
\
		skill->f##_fun = func; \
		send_to_char("Skill " #f " function set.\n\r", ch); \
		return true; \
	} \
}

SKEDIT_FUNC(prespell,PRESPELL,SPELL_FUN)
SKEDIT_FUNC(spell,SPELL,SPELL_FUN)

SKEDIT_FUNC(prebrew,TOKEN_PREBREW,PREBREW_FUN)
SKEDIT_FUNC(brew,TOKEN_BREW,BREW_FUN)
SKEDIT_FUNC(quaff,TOKEN_QUAFF,QUAFF_FUN)

SKEDIT_FUNC(prescribe,TOKEN_PRESCRIBE,PRESCRIBE_FUN)
SKEDIT_FUNC(scribe,TOKEN_SCRIBE,SCRIBE_FUN)
SKEDIT_FUNC(recite,TOKEN_RECITE,RECITE_FUN)

SKEDIT_FUNC(preink,TOKEN_PREINK,PREINK_FUN)
SKEDIT_FUNC(ink,TOKEN_INK,INK_FUN)
SKEDIT_FUNC(touch,TOKEN_TOUCH,TOUCH_FUN)

// TODO: IMBUE stuff

#if 0
// Morph based upon whether this is a source or scripted ability
SKEDIT( skedit_prespellfunc )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (skill->token)
	{
		// TOKEN mode
		WNUM wnum;

		// Allow wnum shortcutting using the token's area
		if (!parse_widevnum(argument, skill->token->area, &wnum))
		{
			send_to_char("Syntax:  skedit prespell {R<widevnum>{x\n\r", ch);
			send_to_char("Please specify a widevnum for the token SPELL trigger.\n\r", ch);
			return false;
		}

		// Get the script
		SCRIPT_DATA *script = get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG);
		if (!script)
		{
			send_to_char("No such token script by that widevnum.\n\r", ch);
			return false;
		}

		// Remove any TRIG_PRESPELL triggers from token.
		__token_remove_trigger(skill->token, TRIG_PRESPELL);

		// Add trigger to token.
		if (!__token_add_trigger(skill->token, TRIG_PRESPELL, "100", script))
		{
			send_to_char("Something went wrong adding PRESPELL trigger to token.\n\r", ch);
			return false;
		}

		// Mark area as changed.
		SET_BIT(skill->token->area->area_flags, AREA_CHANGED);
		send_to_char("PRESPELL trigger added to spell token.\n\r", ch);
		return true;
	}
	else
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skedit prespell {R<function>{x\n\r", ch);
			send_to_char("Invalid prespell function.  Use '? prespell_func' for a list of functions.\n\r", ch);
			return false;
		}

		SPELL_FUN *func = prespell_func_lookup(argument);
		if(!func)
		{
			send_to_char("Syntax:  skedit prespell {R<function>{x\n\r", ch);
			send_to_char("Invalid prespell function.  Use '? prespell_func' for a list of functions.\n\r", ch);
			return false;
		}

		skill->prespell_fun = func;
		send_to_char("Skill prespell function set.\n\r", ch);
		return true;
	}
}

// Morph based upon whether this is a source or scripted ability
SKEDIT( skedit_spellfunc )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (skill->token)
	{
		// TOKEN mode
		WNUM wnum;

		// Allow wnum shortcutting using the token's area
		if (!parse_widevnum(argument, skill->token->area, &wnum))
		{
			send_to_char("Syntax:  skedit spell {R<widevnum>{x\n\r", ch);
			send_to_char("Please specify a widevnum for the token SPELL trigger.\n\r", ch);
			return false;
		}

		// Get the script
		SCRIPT_DATA *script = get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG);
		if (!script)
		{
			send_to_char("No such token script by that widevnum.\n\r", ch);
			return false;
		}

		// Remove any TRIG_SPELL triggers from token.
		__token_remove_trigger(skill->token, TRIG_SPELL);

		// Add trigger to token.
		if (!__token_add_trigger(skill->token, TRIG_SPELL, "100", script))
		{
			send_to_char("Something went wrong adding SPELL trigger to token.\n\r", ch);
			return false;
		}

		// Mark area as changed.
		SET_BIT(skill->token->area->area_flags, AREA_CHANGED);
		send_to_char("SPELL trigger added to spell token.\n\r", ch);
		return true;
	}
	else
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skedit spell {R<function>{x\n\r", ch);
			send_to_char("Please specify a spell function name.  Use '? spell_func' for a list of functions.\n\r", ch);
			return false;
		}

		SPELL_FUN *func = spell_func_lookup(argument);
		if(!func)
		{
			send_to_char("Syntax:  skedit spell {R<function>{x\n\r", ch);
			send_to_char("Invalid spell function.  Use '? spell_func' for a list of functions.\n\r", ch);
			return false;
		}

		skill->spell_fun = func;
		send_to_char("Skill spell function set.\n\r", ch);
		return true;
	}
}
#endif


SKEDIT( skedit_gsn )
{
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit gsn {Rset{x <gsn>\n\r", ch);
		send_to_char("Syntax:  skedit gsn {Rclear{x\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "set"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skedit gsn set {R<gsn>{x\n\r", ch);
			send_to_char("Please specify a GSN.  Use '? gsn' to see list of available GSNs.\n\r", ch);
			return false;
		}

		sh_int *pgsn = gsn_from_name(argument);
		if (!pgsn)
		{
			send_to_char("Syntax:  skedit gsn set {R<gsn>{x\n\r", ch);
			send_to_char("Invalid GSN.  Use '? gsn' to see list of available GSNs.\n\r", ch);
			return false;
		}

		ITERATOR it;
		SKILL_DATA *sk;
		iterator_start(&it, skills_list);
		while((sk = (SKILL_DATA *)iterator_nextdata(&it)))
		{
			if (sk->pgsn == pgsn && sk != skill)
				break;
		}
		iterator_stop(&it);

		if (sk)
		{
			sprintf(buf, "That GSN is already used by skill {W%s{x.\n\r", sk->name);
			send_to_char(buf, ch);
			return false;
		}

		skill->pgsn = pgsn;
		send_to_char("Skill GSN set.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (!skill->pgsn)
		{
			send_to_char("There is no GSN set on this skill.\n\r", ch);
			return false;
		}

		skill->pgsn = NULL;
		send_to_char("Skill GSN clear.\n\r", ch);
		return true;
	}

	skedit_gsn(ch, "");
	return false;
}

SKEDIT( skedit_level )
{
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (argument[0] == '\0')
	{
		sprintf(buf, "Syntax:  skedit level {R<class>{x 1-%d|none\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		send_to_char("Please select a class.  Use '? classes' to get a list of classes.\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	
	int clazz = class_lookup(arg);
	if (clazz < 0)
	{
		sprintf(buf, "Syntax:  skedit level <class> {R1-%d|none{x\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		send_to_char("Invalid class.  Use '? classes' to get a list of classes.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		sprintf(buf, "Syntax:  skedit level <class> {R1-%d|none{x\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		sprintf(buf, "Please specify a number from 1 to %d or {Wnone{x.\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		return false;
	}

	int value;
	if (is_number(argument))
	{
		value = atoi(argument);
		if (value < 1 || value > MAX_CLASS_LEVEL)
		{
			sprintf(buf, "Syntax:  skedit level <class> {R1-%d{x\n\r", MAX_CLASS_LEVEL);
			send_to_char(buf, ch);
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", MAX_CLASS_LEVEL);
			send_to_char(buf, ch);
			return false;
		}
	}
	else if (!str_prefix(argument, "none"))
	{
		value = -1;
	}
	else
	{
		send_to_char("Syntax:  skedit level <class> {Rnone{x\n\r", ch);
		sprintf(buf, "Please specify a number from 1 to %d or {Wnone{x.\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		return false;
	}

	skill->skill_level[clazz] = value;
	sprintf(buf, "Skill level set for {+%s.\n\r", class_table[clazz].name);
	send_to_char(buf, ch);
	return true;
}

SKEDIT( skedit_difficulty )
{
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit difficulty {R<class>{x 1+|none\n\r", ch);
		send_to_char("Please select a class.  Use '? classes' to get a list of classes.\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	
	int clazz = class_lookup(arg);
	if (clazz < 0)
	{
		send_to_char("Syntax:  skedit difficulty {R<class>{x 1+|none\n\r", ch);
		send_to_char("Invalid class.  Use '? classes' to get a list of classes.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit difficulty <class> {R1+|none{x\n\r", ch);
		send_to_char("Please specify a positive number or {Wnone{x.\n\r", ch);
		return false;
	}

	int value;
	if (is_number(argument))
	{
		value = atoi(argument);
		if (value < 1)
		{
			send_to_char("Syntax:  skedit difficulty <class> {R1+{x\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}
	}
	else if (!str_prefix(argument, "none"))
	{
		value = -1;
	}
	else
	{
		send_to_char("Syntax:  skedit difficulty <class> {Rnone{x\n\r", ch);
		return false;
	}

	skill->rating[clazz] = value;
	sprintf(buf, "Skill difficulty set for {+%s.\n\r", class_table[clazz].name);
	send_to_char(buf, ch);
	return true;
}

SKEDIT( skedit_target )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	int value;
	if ((value = stat_lookup(argument, spell_target_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  skedit target {R<target>{x\n\r", ch);
		send_to_char("Invalid spell target.  Use '? spell_targets' to see list of targets.\n\r", ch);
		return false;
	}

	skill->target = value;
	send_to_char("Skill spell target set.\n\r", ch);
	return true;
}

SKEDIT( skedit_position )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	int value;
	if ((value = stat_lookup(argument, position_flags, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  skedit position {R<position>{x\n\r", ch);
		send_to_char("Invalid position.  Use '? position' to see list of positions.\n\r", ch);
		return false;
	}

	skill->minimum_position = value;
	send_to_char("Skill minimum position set.\n\r", ch);
	return true;
}

SKEDIT( skedit_race )
{
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	int race;
	if (argument[0] != '\0' && (race = race_lookup(argument)) >= 0)
	{
		send_to_char("Syntax:  skedit race {R<race>{x\n\r", ch);
		send_to_char("Please specify a race.  Use '? races' for valid races.\n\r", ch);
		return false;
	}

    if (argument[0] == '?')
    {
		send_to_char("Available races are:", ch);

		for (race = 0; race_table[race].name != NULL; race++)
		{
			if ((race % 3) == 0)
				send_to_char("\n\r", ch);
			sprintf(buf, " %-15s", race_table[race].name);
			send_to_char(buf, ch);
		}

		send_to_char("\n\r", ch);
		return false;
    }

	if (race > 0)
	{
		skill->race = race;
		send_to_char("Skill Race set.\n\r", ch);
	}
	else
	{
		skill->race = -1;
		send_to_char("Skill Race cleared.\n\r", ch);
	}
	return true;
}

SKEDIT( skedit_mana )
{
	char buf[MSL];
	char arg[MIL];
	SKILL_DATA *skill;
	sh_int *mana;

	EDIT_SKILL(ch, skill);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skedit mana {Rcast{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rbrew{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rscribe{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rimbue{x <mana>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "cast"))
	{
		strcpy(arg, "cast");
		mana = &skill->cast_mana;
	}
	else if (!str_prefix(arg, "brew"))
	{
		strcpy(arg, "brew");
		mana = &skill->brew_mana;
	}
	else if (!str_prefix(arg, "scribe"))
	{
		strcpy(arg, "scribe");
		mana = &skill->scribe_mana;
	}
	else if (!str_prefix(arg, "imbue"))
	{
		strcpy(arg, "imbue");
		mana = &skill->imbue_mana;
	}
	else
	{
		send_to_char("Syntax:  skedit mana {Rcast{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rbrew{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rscribe{x <mana>\n\r", ch);
		send_to_char("Syntax:  skedit mana {Rimbue{x <mana>\n\r", ch);
		return false;
	}

	int value;
	if (!is_number(argument) || (value = atoi(argument)) < 0)
	{
		sprintf(buf, "Syntax:  skedit mana %s {R<mana>{x\n\r", arg);
		send_to_char(buf, ch);
		send_to_char("Please specify a non-negative number.\n\r", ch);
		return false;
	}

	*mana = value;
	sprintf(buf, "Skill {+%s Mana Cost set.\n\r", arg);
	send_to_char(buf, ch);
	return true;
}

SKEDIT( skedit_beats )
{
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	int value;
	if (!is_number(argument) || (value = atoi(argument)) < 0)
	{
		send_to_char("Syntax:  skedit beats {R<beats>{x\n\r", ch);
		send_to_char("Please specify a non-negative number.\n\r", ch);
		return false;
	}

	skill->beats = value;
	send_to_char("Skill Beats set.\n\r", ch);
	return true;

	return false;
}

SKEDIT( skedit_message )
{
	char arg[MIL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	argument = one_argument(argument, arg);
	if (arg[0] == '\0')
	{
		send_to_char("Syntax:  skedit message {Rnoun{x set <message>\n\r", ch);
		send_to_char("         skedit message {Rnoun{x clear\n\r", ch);
		send_to_char("         skedit message {Rwearoff{x set <message>\n\r", ch);
		send_to_char("         skedit message {Rwearoff{x clear\n\r", ch);
		send_to_char("         skedit message {Robject{x set <message>\n\r", ch);
		send_to_char("         skedit message {Robject{x clear\n\r", ch);
		send_to_char("         skedit message {Rdispel{x set <message>\n\r", ch);
		send_to_char("         skedit message {Rdispel{x clear\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "noun"))
	{
		argument = one_argument(argument, arg);
		if (arg[0] == '\0')
		{
			send_to_char("Syntax:  skedit message noun {Rset{x <message>\n\r", ch);
			send_to_char("         skedit message noun {Rclear{x\n\r", ch);
			return false;
		}

		if (!str_prefix(arg, "set"))
		{
			smash_tilde(argument);
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  skedit message noun set {R<message>{x\n\r", ch);
				send_to_char("Please specify a noun damage string.\n\r", ch);
				return false;
			}

			free_string(skill->noun_damage);
			skill->noun_damage = str_dup(argument);
			send_to_char("Skill Noun Damage set.\n\r", ch);
			return true;
		}

		if (!str_prefix(arg, "clear"))
		{
			free_string(skill->noun_damage);
			skill->noun_damage = NULL;
			send_to_char("Skill Noun Damage cleared.\n\r", ch);
			return true;
		}

		skedit_message(ch, "noun");
		return false;
	}

	if (!str_prefix(arg, "wearoff"))
	{
		argument = one_argument(argument, arg);
		if (arg[0] == '\0')
		{
			send_to_char("Syntax:  skedit message wearoff {Rset{x <message>\n\r", ch);
			send_to_char("         skedit message wearoff {Rclear{x\n\r", ch);
			return false;
		}

		if (!str_prefix(arg, "set"))
		{
			smash_tilde(argument);
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  skedit message wearoff set {R<message>{x\n\r", ch);
				send_to_char("Please specify a wearoff damage string.\n\r", ch);
				return false;
			}

			free_string(skill->msg_off);
			skill->msg_off = str_dup(argument);
			send_to_char("Skill Wear Off Message set.\n\r", ch);
			return true;
		}

		if (!str_prefix(arg, "clear"))
		{
			free_string(skill->msg_off);
			skill->msg_off = NULL;
			send_to_char("Skill Wear Off Message cleared.\n\r", ch);
			return true;
		}

		skedit_message(ch, "wearoff");
		return false;
	}

	if (!str_prefix(arg, "object"))
	{
		argument = one_argument(argument, arg);
		if (arg[0] == '\0')
		{
			send_to_char("Syntax:  skedit message object {Rset{x <message>\n\r", ch);
			send_to_char("         skedit message object {Rclear{x\n\r", ch);
			return false;
		}

		if (!str_prefix(arg, "set"))
		{
			smash_tilde(argument);
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  skedit message object set {R<message>{x\n\r", ch);
				send_to_char("Please specify a object damage string.\n\r", ch);
				return false;
			}

			free_string(skill->msg_obj);
			skill->msg_obj = str_dup(argument);
			send_to_char("Skill Wear Off (Object) Message set.\n\r", ch);
			return true;
		}

		if (!str_prefix(arg, "clear"))
		{
			free_string(skill->msg_obj);
			skill->msg_obj = NULL;
			send_to_char("Skill Wear Off (Object) Message cleared.\n\r", ch);
			return true;
		}

		skedit_message(ch, "object");
		return false;
	}

	if (!str_prefix(arg, "dispel"))
	{
		argument = one_argument(argument, arg);
		if (arg[0] == '\0')
		{
			send_to_char("Syntax:  skedit message dispel {Rset{x <message>\n\r", ch);
			send_to_char("         skedit message dispel {Rclear{x\n\r", ch);
			return false;
		}

		if (!str_prefix(arg, "set"))
		{
			smash_tilde(argument);
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  skedit message dispel set {R<message>{x\n\r", ch);
				send_to_char("Please specify a dispel damage string.\n\r", ch);
				return false;
			}

			free_string(skill->msg_disp);
			skill->msg_disp = str_dup(argument);
			send_to_char("Skill Dispel Message set.\n\r", ch);
			return true;
		}

		if (!str_prefix(arg, "clear"))
		{
			free_string(skill->msg_disp);
			skill->msg_disp = NULL;
			send_to_char("Skill Dispel Message cleared.\n\r", ch);
			return true;
		}

		skedit_message(ch, "dispel");
		return false;
	}

	skedit_message(ch, "");
	return false;
}

SKEDIT( skedit_inks )
{
	char buf[MSL];
	char arg[MIL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	int index;
	argument = one_argument(argument, arg);
	if (!is_number(arg) || (index = atoi(arg)) < 1 || index > 3)
	{
		send_to_char("Syntax: skedit inks {R<1-3>{x none\n\r", ch);
		send_to_char("        skedit inks {R<1-3>{x <catalyst>[ <amount+>]\n\r", ch);
		send_to_char("Please specify a number from 1 to 3.\n\r", ch);
		return false;
	}

	int catalyst;
	argument = one_argument(argument, arg);
	if ((catalyst = stat_lookup(arg, catalyst_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax: skedit inks {R<1-3>{x none\n\r", ch);
		send_to_char("        skedit inks <1-3> {R<catalyst>{x[ <amount+>]\n\r", ch);
		send_to_char("Invalid catalyst type.  Use '? catalyst' for list of catalyst types.\n\r", ch);
		return false;
	}

	int amount = 0;
	if (catalyst != CATALYST_NONE)
	{
		if (!is_number(argument) || (amount = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  skedit inks <1-3> <catalyst> {R<amount+>{x\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}
	}

	skill->inks[index - 1][0] = catalyst;
	skill->inks[index - 1][1] = amount;
	sprintf(buf, "Skill Ink %d set.\n\r", index);
	send_to_char(buf, ch);
	return true;
}

SKEDIT( skedit_value )
{
	char arg[MIL];
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	argument = one_argument(argument, arg);
	int index;
	if (!is_number(arg) || (index = atoi(arg)) < 1 || index > MAX_SKILL_VALUES)
	{
		sprintf(buf, "Syntax:  skedit value {R1-%d{x <value>\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		sprintf(buf, "Please select an index from 1 to %d.\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		return false;
	}

	if (!is_number(argument))
	{
		sprintf(buf, "Syntax:  skedit value 1-%d {R<value>{x\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		send_to_char("Please provide a number.\n\r", ch);
		return false;
	}

	skill->values[index - 1] = atoi(argument);
	if (IS_NULLSTR(skill->valuenames[index - 1]))
		sprintf(buf, "Skill Value %d set.\n\r", index);
	else
		sprintf(buf, "Skill Value %d (%s) set.\n\r", index, skill->valuenames[index - 1]);
	send_to_char(buf, ch);
	return true;
}

SKEDIT( skedit_valuename )
{
	char arg[MIL];
	char buf[MSL];
	SKILL_DATA *skill;

	EDIT_SKILL(ch, skill);

	argument = one_argument(argument, arg);
	int index;
	if (!is_number(arg) || (index = atoi(arg)) < 1 || index > MAX_SKILL_VALUES)
	{
		sprintf(buf, "Syntax:  skedit valuename {R1-%d{x set <name>\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		sprintf(buf, "         skedit valuename {R1-%d{x clear\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		sprintf(buf, "Please select an index from 1 to %d.\n\r", MAX_SKILL_VALUES);
		send_to_char(buf, ch);
		return false;
	}

	argument = one_argument(argument, arg);
	if(!str_prefix(arg, "set"))
	{
		smash_tilde(argument);
		if (argument[0] == '\0')
		{
			sprintf(buf, "Syntax:  skedit valuename 1-%d set {R<name>{x\n\r", MAX_SKILL_VALUES);
			send_to_char(buf, ch);
			send_to_char("Please provide a name.\n\r", ch);
			return false;
		}

		free_string(skill->valuenames[index - 1]);
		skill->valuenames[index - 1] = str_dup(argument);
		sprintf(buf, "Skill Value Name %d set.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		free_string(skill->valuenames[index - 1]);
		skill->valuenames[index - 1] = NULL;
		sprintf(buf, "Skill Value Name %d cleared.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	sprintf(buf, "Syntax:  skedit valuename 1-%d {Rset{x <name>\n\r", MAX_SKILL_VALUES);
	send_to_char(buf, ch);
	sprintf(buf, "         skedit valuename 1-%d {Rclear{x\n\r", MAX_SKILL_VALUES);
	send_to_char(buf, ch);
	return false;
}


SGEDIT( sgedit_show )
{
	char buf[MSL];
	SKILL_GROUP *group;

	EDIT_SKILL_GROUP(ch, group);

	BUFFER *buffer = new_buf();

	sprintf(buf, "{BGroup: {C%s{x\n\r", group->name);
	add_buf(buffer, buf);

	add_buf(buffer, "{b=================================={x\n\r");

	ITERATOR it;
	SKILL_DATA *sk;
	SKILL_GROUP *gr;
	char *str;
	int cnt = 0;
	iterator_start(&it, group->contents);
	while((str = (char *)iterator_nextdata(&it)))
	{
		sk = get_skill_data(str);

		++cnt;

		if (IS_VALID(sk))
			sprintf(buf, "{C%3d {b- {%c%s{x\n\r", cnt, (sk->isspell?'G':'Y'), sk->name);
		else
		{
			gr = group_lookup(str);
			if (IS_VALID(gr))
				sprintf(buf, "{C%3d {b- {W%s{x\n\r", cnt, gr->name);
			else
				sprintf(buf, "{C%3d {b- {D%s{x\n\r", cnt, str);
		}

		add_buf(buffer, buf);
	}
	iterator_stop(&it);
	
	add_buf(buffer, "{b----------------------------------{x\n\r");
	sprintf(buf, "{BTotal: {C%d{x\n\r", list_size(group->contents));


	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);

	return false;
}

SGEDIT( sgedit_create )
{
	SKILL_GROUP *group;

	smash_tilde(argument);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  sgedit create {R<name>{x\n\r", ch);
		send_to_char("Please provide a unique name.\n\r", ch);
		return false;
	}

	group = group_lookup_exact(argument);
	if (IS_VALID(group))
	{
		send_to_char("Syntax:  sgedit create {R<name>{x\n\r", ch);
		send_to_char("Name is already taken.\n\r", ch);
		return false;
	}

	group = new_skill_group_data();
	group->name = str_dup(argument);
	insert_skill_group(group);

	char buf[MSL];
	sprintf(buf, "Group {W%s{x added.\n\r", argument);
	send_to_char(buf, ch);
	return true;
}

SGEDIT( sgedit_add )
{
	SKILL_GROUP *group;

	EDIT_SKILL_GROUP(ch, group);

	smash_tilde(argument);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  sgedit add {R<name>{x\n\r", ch);
		send_to_char("Please provide the name for a skill, spell or group.\n\r", ch);
		return false;
	}

	if(group_has_item_exact(group, argument))
	{
		send_to_char("That group already contains that item.\n\r", ch);
		return false;
	}

	char buf[MSL];
	SKILL_DATA *sk = get_skill_data(argument);
	if (!IS_VALID(sk))
	{
		SKILL_GROUP *gr = group_lookup(argument);

		if (!IS_VALID(gr))
		{
			send_to_char("There is no skill, spell or group by that name.\n\r", ch);
			return false;
		}

		if (gr == group)
		{
			send_to_char("You cannot add a group to itself.\n\r", ch);
			return false;
		}

		// TODO: Check for circular linkages.

		sprintf(buf, "Added group '%s' to group %s.\n\r", gr->name, group->name);
	}
	else
		sprintf(buf, "Added %s '%s' to group %s.\n\r", (sk->isspell ? "spell" : "skill"), sk->name, group->name);

	insert_skill_group_item(group, str_dup(argument));

	send_to_char(buf, ch);
	return true;
}

SGEDIT( sgedit_remove )
{
	char buf[MSL];
	SKILL_GROUP *group;

	EDIT_SKILL_GROUP(ch, group);

	if (list_size(group->contents) < 1)
	{
		send_to_char("This group is already empty.\n\r", ch);
		return false;
	}

	int index;
	if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(group->contents))
	{
		sprintf(buf, "Syntax:  sgedit remove {R<1-%d>{x\n\r", list_size(group->contents));
		send_to_char(buf, ch);
		sprintf(buf, "Please provide a number from 1 to %d.\n\r", list_size(group->contents));
		send_to_char(buf, ch);
		return false;
	}

	list_remnthlink(group->contents, index);
	sprintf(buf, "Removed #%d item from group.\n\r", index);
	send_to_char(buf, ch);
	return true;
}
