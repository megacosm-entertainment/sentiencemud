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
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "scripts.h"

void deduct_mana(CHAR_DATA *ch,int cost);

void do_play(CHAR_DATA *ch, char *argument)
{
	SKILL_ENTRY *entry;
    OBJ_DATA *instrument;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
	SCRIPT_DATA *script = NULL;
	ITERATOR it;
	PROG_LIST *prg;
    char *name;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    int chance = 0;
    int level;
    int target;
    int mana;
    int beats;

    if( IS_NPC(ch) ) return;	// No NPC bards

    argument = one_argument(argument,arg);
    argument = one_argument(argument,arg2);

    if ((chance = get_skill(ch,gsn_music)) == 0)
    {
		send_to_char("You whistle a little tune to yourself.\n\r",ch);
		return;
    }

    if (ch->bashed > 0)
    {
		send_to_char("You must stand up first.\n\r", ch);
		return;
    }

    if ( arg[0] == '\0')
    {
		if( ch->sorted_songs )
		{
			BUFFER *buffer = new_buf();
			add_buf(buffer, "You know the following songs: \n\r\n\r");
			add_buf(buffer, "{YSong Title                            Level         Mana{x\n\r");
			add_buf(buffer, "{Y---------------------------------------------------------{x\n\r");

			for(entry = ch->sorted_songs; entry; entry = entry->next) {
				level = skill_entry_level(ch, entry);
				name = skill_entry_name(entry);
				if( entry->token )
					sprintf(buf, "%-30s %10d\n\r", name, level);
				else
					sprintf(buf, "%-30s %10d %13d\n\r", name, level, music_table[entry->song].mana);
				add_buf(buffer, buf);
			}
			page_to_char(buf_string(buffer), ch);
			free_buf(buffer);
		}
		else
			send_to_char( "There are no songs you can play at this time.\n\r", ch );

		return;
	}

	obj = NULL;
	for (instrument = ch->carrying; instrument != NULL; instrument = instrument->next_content )
	{
		if ( instrument->item_type == ITEM_INSTRUMENT && instrument->wear_loc != WEAR_NONE)
		{
			obj = instrument;
			break;
		}
	}

    if (obj == NULL)
    {
		send_to_char("You are not using an instrument.\n\r", ch);
		return;
    }

    entry = skill_entry_findname(ch->sorted_songs, arg);

    if (!entry)
    {
		send_to_char("You don't know that song.\n\r", ch);
		return;
    }
    else if( IS_VALID(entry->token) )
    {
		// Check that the token has the right scripts
		// Check thst the token is a valid token spell
		script = NULL;
		if( entry->token->pIndexData->progs ) {
			iterator_start(&it, entry->token->pIndexData->progs[TRIGSLOT_SPELL]);
			while(( prg = (PROG_LIST *)iterator_nextdata(&it))) {
				if(is_trigger_type(prg->trig_type,TRIG_SPELL)) {
					script = prg->script;
					break;
				}
			}
			iterator_stop(&it);
		}

		if(!script) {
			// Give some indication that the song token is broken
			send_to_char("You don't recall how to play that song.\n\r", ch);
			return;
		}

		mana = entry->token->value[TOKVAL_SPELL_MANA];
		if ((ch->mana + ch->manastore) < mana) {
			send_to_char("You don't have enough mana.\n\r", ch);
			return;
		}

		// Setup targets.
		ch->tempstore[0] = 0;

		// Precheck for the song token - set the music beats in here!
		if(p_percent_trigger(NULL,NULL,NULL,entry->token,ch,NULL,NULL, obj, NULL, TRIG_PRESPELL, NULL))
			return;

		beats = ch->tempstore[0];

		target = entry->token->pIndexData->value[TOKVAL_SPELL_TARGET];
	}
	else
	{
		mana = music_table[entry->song].mana;

		if ((ch->mana + ch->manastore) < mana) {
			send_to_char("You don't have enough mana.\n\r", ch);
			return;
		}

		script = NULL;
		target = music_table[entry->song].target;
		beats = music_table[entry->song].beats;
	}


    if ( arg2[0] != '\0' )
    {
		if ( ( mob = get_char_room(ch, NULL, arg2) ) == NULL )
		{
			send_to_char("They aren't here.\n\r", ch);
			return;
		}
		else if ( mob != NULL)
		{
			switch (target) {
			case TAR_IGNORE:
				send_to_char("You can't harness the energies of that song onto one target.\n\r", ch );
				return;
				break;

			case TAR_CHAR_SELF:
				if ( mob != ch ) {
					send_to_char("You can't harness the energies of that song onto anyone except yourself.\n\r", ch );
					return;
				}
				break;

			case TAR_CHAR_OFFENSIVE:
			case TAR_OBJ_CHAR_OFF:
				if ( is_safe( ch, mob, TRUE ) )
					return;
				break;

			case TAR_CHAR_DEFENSIVE:
			case TAR_CHAR_FORMATION:
			case TAR_OBJ_CHAR_DEF:
				if ( mob != ch
				&&   mob->fighting != NULL
				&&   ch->fighting != mob
				&&   !IS_NPC(mob)
				&&   !IS_NPC(mob->fighting)
				&&   !is_pk(ch))
				{
				send_to_char("You can't interfere in a PK battle if you are not PK.\n\r", ch );
				return;
				}
				break;
			}

			if ( mob == ch )
			{
			send_to_char("{YYou begin to play the song softly to yourself...{X\n\r", ch);
			act( "{Y$n begins to play a song on $p{Y softly to $mself...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
			act("{YYou begin to play the song, sweetly exerting its influence on $N...{X", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n begins to play a song, exerting its influence on $N...{X", ch, mob, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
			act("{Y$n begins to play a song, exerting its influence on you...{X", ch, mob, NULL, obj, NULL, NULL, NULL, TO_VICT);
			}
		}

		ch->music_target = str_dup(mob->name);
    }
    else
    {
		/* Syn- this fix is here to make sure offensive target songs cannot be played in safe rooms. */

		switch (target) {
			case TAR_CHAR_OFFENSIVE:
			case TAR_OBJ_CHAR_OFF:
				if (IS_SET(ch->in_room->room_flags, ROOM_SAFE)) {
					send_to_char("This room is sanctioned by the gods.\n\r", ch);
					return;
				}

			break;
		}
		act( "{YYou begin to play a song on $p{Y...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act( "{Y$n begins to play a song on $p{Y...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    }

	// Setup targets.
	ch->song_num = entry->song;
	ch->song_token = IS_VALID(entry->token) ? entry->token : NULL;
	ch->song_script = script;
	ch->song_mana = mana;
	ch->song_instrument = obj;

	// Block to deal with reductions
	{
		int scale1 = 100;
		int scale2 = 100;

		/* Sage has shorter playing time if Bard before */
		if (IS_SAGE(ch) && ch->pcdata->sub_class_thief == CLASS_THIEF_BARD)
		{
			scale1 *= 2;
			scale2 *= 3;	// Give a 1/3 reduction
		}

		if( obj != NULL )
		{

			/*
			// Magical instruments reduce the casting time by 25%
			if( IS_OBJ_STAT(obj, ITEM_MAGIC) )
			{
				scale1 *= 3;
				scale2 *= 4;	// Give a 25% reduction
			}
			*/

			// Only do it if both are set
			if( obj->value[2] > 0 && obj->value[3] > 0)
			{
				int scale;

				if( obj->value[2] < obj->value[3] )
					scale = number_range(obj->value[2], obj->value[3]);
				else
					scale = number_range(obj->value[3], obj->value[2]);

				if( scale != 100 )
				{
					scale1 *= scale;
					scale2 *= 100;
				}
			}
		}

		beats = scale1 * beats / scale2;
	}

	if( beats < 1 ) beats = 1;	// Mininum, no matter what the definition tries to pull

	MUSIC_STATE(ch, beats);
}


void music_end( CHAR_DATA *ch )
{
    CHAR_DATA *mob, *mob_next;
	TOKEN_DATA *token = NULL;
	SCRIPT_DATA *script = NULL;
	int mana;
	unsigned long id[2];
	int type;
	int song_num;
	int sn;
	const struct music_type* pSong = NULL;
	bool offensive = FALSE;
	bool wasdead;

    act( "{YYou finish playing your song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act( "{Y$n finishes $s song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	if(ch->song_token) {
		token = ch->song_token;
		script = ch->song_script;
		type = token->pIndexData->value[TOKVAL_SPELL_TARGET];
		pSong = NULL;
		song_num = -1;
	} else {
		song_num = ch->song_num;
		pSong = &music_table[song_num];
		type = music_table[song_num].target;
	}

	mana = ch->song_mana;
	deduct_mana(ch, mana);

    // We are casting it on just one person only
    if (ch->music_target != NULL)
    {
		mob = get_char_room(ch, NULL, ch->music_target);
		if ( mob == NULL )
		{
			send_to_char("They aren't here.\n\r", ch);
			free_string(ch->music_target);
			ch->music_target = NULL;
			ch->song_token = NULL;
			ch->song_script = NULL;
			ch->song_num = -1;
			ch->song_instrument = NULL;
			return;
		}
		else
		{
			char *music_target_name = ch->music_target;
			ch->music_target = NULL;

			if( pSong )
			{
				id[0] = mob->id[0];
				id[1] = mob->id[1];
				wasdead = mob->dead;

				if( (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) && pSong->spell1 )
				{
					sn = skill_lookup(pSong->spell1);

					if( sn > 0 && sn < MAX_SKILL && skill_table[sn].spell_fun != spell_null)
					{
						if(skill_table[sn].target == TAR_CHAR_OFFENSIVE || skill_table[sn].target == TAR_OBJ_CHAR_DEF)
							offensive = TRUE;
						if (check_spell_deflection(ch, mob, sn))
							(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

				}

				if( (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) && pSong->spell2 )
				{
					sn = skill_lookup(pSong->spell2);

					if( sn > 0 && sn < MAX_SKILL && skill_table[sn].spell_fun != spell_null)
					{
						if(skill_table[sn].target == TAR_CHAR_OFFENSIVE || skill_table[sn].target == TAR_OBJ_CHAR_DEF)
							offensive = TRUE;
						if (check_spell_deflection(ch, mob, sn))
							(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

				}

				if( (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) && pSong->spell3 )
				{
					sn = skill_lookup(pSong->spell3);

					if( sn > 0 && sn < MAX_SKILL && skill_table[sn].spell_fun != spell_null)
					{
						if(skill_table[sn].target == TAR_CHAR_OFFENSIVE || skill_table[sn].target == TAR_OBJ_CHAR_DEF)
							offensive = TRUE;
						if (check_spell_deflection(ch, mob, sn))
							(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

				}
			}
			else
			{
				if (check_spell_deflection_token(ch, mob, token, script, music_target_name)) {
					if( execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL, NULL, NULL,music_target_name,NULL,TRIG_NONE,0,0,0,0,0) > 0)
						offensive = TRUE;
				}
			}

			free_string(music_target_name);

			if (mob != ch && !is_safe(ch, mob, FALSE) && (type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF || offensive))
			{
				multi_hit(ch, mob, TYPE_UNDEFINED);
			}

			ch->song_token = NULL;
			ch->song_script = NULL;
			ch->song_num = -1;
			ch->song_instrument = NULL;
			return;
		}
    }
    else
    {
		int sn1 = -1, sn2 = -1, sn3 = -1;

		if(pSong)
		{
			if( pSong->spell1 )
			{
				sn1 = skill_lookup(pSong->spell1);

				if( sn1 < 1 || sn1 >= MAX_SKILL || skill_table[sn1].spell_fun == spell_null)
					sn1 = -1;
				else if(skill_table[sn1].target == TAR_CHAR_OFFENSIVE || skill_table[sn1].target == TAR_OBJ_CHAR_DEF)
					offensive = TRUE;

			}

			if( pSong->spell2 )
			{
				sn2 = skill_lookup(pSong->spell2);

				if( sn2 < 1 || sn2 >= MAX_SKILL || skill_table[sn2].spell_fun == spell_null)
					sn2 = -1;
				else if(skill_table[sn2].target == TAR_CHAR_OFFENSIVE || skill_table[sn2].target == TAR_OBJ_CHAR_DEF)
					offensive = TRUE;
			}

			if( pSong->spell3 )
			{
				sn3 = skill_lookup(pSong->spell3);

				if( sn3 < 1 || sn3 >= MAX_SKILL || skill_table[sn3].spell_fun == spell_null)
					sn3 = -1;
				else if(skill_table[sn3].target == TAR_CHAR_OFFENSIVE || skill_table[sn3].target == TAR_OBJ_CHAR_DEF)
					offensive = TRUE;
			}
		}

		switch(type)
		{
		case TAR_CHAR_FORMATION:
			for(mob = ch->in_room->people; mob != NULL; mob = mob_next)
			{
				mob_next = mob->next_in_room;

				if( !is_same_group(mob, ch) )
					continue;

				if( mob != ch && mob->fighting && !IS_NPC(mob) && !IS_NPC(mob->fighting) && !is_pk(ch))
					continue;

				id[0] = mob->id[0];
				id[1] = mob->id[1];
				wasdead = mob->dead;

				if( pSong )
				{
					if( sn1 > 0 )
					{
						if (check_spell_deflection(ch, mob, sn1))
							(*skill_table[sn1].spell_fun) (sn1, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
					{
						if (check_spell_deflection(ch, mob, sn2))
							(*skill_table[sn2].spell_fun) (sn2, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
					{
						if (check_spell_deflection(ch, mob, sn3))
							(*skill_table[sn3].spell_fun) (sn3, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

				}
				else
				{
					if (check_spell_deflection_token(ch, mob, token, script, NULL))
						if(execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL, NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0) > 0)
							offensive = TRUE;
				}

				if (mob != ch && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1])) && (mob->dead == wasdead) && !is_safe(ch, mob, FALSE) && offensive)
					set_fighting(mob, ch);
			}
			break;

		case TAR_CHAR_DEFENSIVE:
		case TAR_OBJ_CHAR_DEF:
			for(mob = ch->in_room->people; mob != NULL; mob = mob_next)
			{
				mob_next = mob->next_in_room;

				if( mob != ch && mob->fighting && !IS_NPC(mob) && !IS_NPC(mob->fighting) && !is_pk(ch))
					continue;

				id[0] = mob->id[0];
				id[1] = mob->id[1];
				wasdead = mob->dead;

				if( pSong )
				{

					if( sn1 > 0 )
					{
						(*skill_table[sn1].spell_fun) (sn1, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
					{
						(*skill_table[sn2].spell_fun) (sn2, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
					{
						(*skill_table[sn3].spell_fun) (sn3, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

				}
				else
				{
					execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
				}
			}
			break;

		case TAR_CHAR_OFFENSIVE:
		case TAR_OBJ_CHAR_OFF:
			for ( mob = ch->in_room->people; mob != NULL; mob = mob_next)
			{
				mob_next = mob->next_in_room;

				if ( is_safe( ch, mob, FALSE ) )
					continue;

				id[0] = mob->id[0];
				id[1] = mob->id[1];
				wasdead = mob->dead;

				if( pSong )
				{

					if( sn1 > 0 && !is_same_group(ch, mob))
					{
						if (check_spell_deflection(ch, mob, sn1))
							(*skill_table[sn1].spell_fun) (sn1, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead) && !is_same_group(ch, mob)) )
					{
						if (check_spell_deflection(ch, mob, sn2))
							(*skill_table[sn2].spell_fun) (sn2, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}

					if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead) && !is_same_group(ch, mob)) )
					{
						if (check_spell_deflection(ch, mob, sn3))
							(*skill_table[sn3].spell_fun) (sn3, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
					}
				}
				else
				{
					if (check_spell_deflection_token(ch, mob, token, script, NULL))
						if(execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL, NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0) > 0)
							offensive = TRUE;
				}

				if (mob != ch && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1])) && (mob->dead == wasdead) && !is_safe(ch, mob, FALSE) && offensive)
					set_fighting(mob, ch);
			}
			break;

		case TAR_CHAR_SELF:
			mob = ch;
			id[0] = mob->id[0];
			id[1] = mob->id[1];
			wasdead = mob->dead;

			if( pSong )
			{

				if( sn1 > 0 )
				{
					(*skill_table[sn1].spell_fun) (sn1, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
				}

				if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
				{
					(*skill_table[sn2].spell_fun) (sn2, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
				}

				if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
				{
					(*skill_table[sn3].spell_fun) (sn3, ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE);
				}

			}
			else
			{
				execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
			}
			break;
		case TAR_IGNORE:
			mob = ch;
			id[0] = mob->id[0];
			id[1] = mob->id[1];
			wasdead = mob->dead;

			if( pSong )
			{

				if( sn1 > 0 )
				{
					(*skill_table[sn1].spell_fun) (sn1, ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE);
				}

				if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
				{
					(*skill_table[sn2].spell_fun) (sn2, ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE);
				}

				if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
				{
					(*skill_table[sn3].spell_fun) (sn3, ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE);
				}

			}
			else
			{
				execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
			}
			break;

		}

		ch->song_token = NULL;
		ch->song_script = NULL;
		ch->song_num = -1;
		ch->song_instrument = NULL;
	}

/*
    switch( music_table[song_num].target )
    {
	case TAR_CHAR_FORMATION:
	    for ( mob = ch->in_room->people; mob != NULL; mob = mob_next )
	    {
	        mob_next = mob->next_in_room;

		if ( !is_same_group(mob, ch))
		    continue;

		if ( mob != ch
		&&   mob->fighting != NULL
		&&   !IS_NPC(mob)
		&&   !IS_NPC(mob->fighting)
		&&   !is_pk(ch))
		    continue;

		if ( music_table[song_num].spell1 != NULL )
		{
		    int sn = 0;
		    for (sn = 0; sn < MAX_SKILL; sn++ )
			if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell1 ))
			    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell2 != NULL )
		{
		    int sn = 0;
		    for (sn = 0; sn < MAX_SKILL; sn++ )
			if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell2 ))
			    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell3 != NULL )
		{
		    int sn = 0;
		    for (sn = 0; sn < MAX_SKILL; sn++ )
			if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell3 ))
			    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if (mob != ch && !is_safe(ch, mob, FALSE)
		&&  (music_table[song_num].target == TAR_CHAR_OFFENSIVE
		     || music_table[song_num].target == TAR_OBJ_CHAR_OFF))
		    set_fighting(mob, ch);
	    }

	    break;

	case TAR_CHAR_DEFENSIVE:
	case TAR_OBJ_CHAR_DEF:
	    for ( mob = ch->in_room->people; mob != NULL; mob = mob_next )
	    {
	        mob_next = mob->next_in_room;

		if ( mob != ch
		&&   mob->fighting != NULL
		&&   !IS_NPC(mob)
		&&   !IS_NPC(mob->fighting)
		&&   !is_pk(ch))
		    continue;

		if ( music_table[song_num].spell1 != NULL )
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell1 ))
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell2 != NULL )
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell2 ))
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell3 != NULL )
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell3 ))
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}
	    }

	    break;

	case TAR_CHAR_OFFENSIVE:
	case TAR_OBJ_CHAR_OFF:
	    for ( mob = ch->in_room->people; mob != NULL; mob = mob_next)
	    {
	        mob_next = mob->next_in_room;

                if ( is_safe( ch, mob, FALSE ) )
		    continue;

		if ( music_table[song_num].spell1 != NULL
		&& !is_same_group(ch,mob))
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell1 )
		    && mob != ch )
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell2 != NULL
		&& !is_same_group(ch,mob))
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell2 )
		    && mob != ch )
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}

		if ( music_table[song_num].spell3 != NULL
		&& !is_same_group(ch,mob))
		{
		  int sn = 0;
		  for (sn = 0; sn < MAX_SKILL; sn++ )
		    if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell3 )
		    && mob != ch )
			(*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, mob, TARGET_CHAR);
		}
	    }

	    break;

	case TAR_CHAR_SELF:
	    if ( music_table[song_num].spell1 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell1 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_CHAR);
	    }
	    if ( music_table[song_num].spell2 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell2 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_CHAR);
	    }
	    if ( music_table[song_num].spell3 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell3 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_CHAR);
	    }

	    break;

	case TAR_IGNORE:
	    if ( music_table[song_num].spell1 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell1 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_NONE);
	    }

	    if ( music_table[song_num].spell2 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell2 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_NONE);
	    }

	    if ( music_table[song_num].spell3 != NULL )
	    {
	      int sn = 0;
	      for (sn = 0; sn < MAX_SKILL; sn++ )
		if ( !str_cmp(skill_table[sn].name, music_table[song_num].spell3 ))
		    (*skill_table[sn].spell_fun) (sn, ch->tot_level, ch, ch, TARGET_NONE);
	    }
	    break;
    }
    */

    check_improve(ch, gsn_music, TRUE, 2);
}


bool was_bard( CHAR_DATA *ch )
{
    // they are a bard now, so they have to level to get the songs
    if ( ch->pcdata->sub_class_current == CLASS_THIEF_BARD )
	return FALSE;

    // They were a bard sometime in the past so they dont have to level.
    if ( ch->pcdata->sub_class_thief == CLASS_THIEF_BARD )
	return TRUE;

    return FALSE;
}

int music_lookup( char *name)
{
	int i;

	for(i = 0; (i < MAX_SONGS) && music_table[i].name; i++)
		if( !str_cmp(music_table[i].name, name) )
			return i;

	return -1;
}
