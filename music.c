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
#include "tables.h"
#include "recycle.h"
#include "music.h"
#include "olc.h"

#define FUNC_LOOKUPS(f,t,n) \
t * f##_func_lookup(char *name) \
{ \
	for (int i = 0; f##_func_table[i].name != NULL; i++) \
	{ \
		if (!str_cmp(name, f##_func_table[i].name)) \
			return f##_func_table[i].func; \
	} \
 \
	return NULL; \
} \
 \
char *f##_func_name(t *func) \
{ \
	for (int i = 0; f##_func_table[i].name != NULL; i++) \
	{ \
		if (f##_func_table[i].func == func) \
			return f##_func_table[i].name; \
	} \
 \
	return NULL; \
} \
 \
char *f##_func_display(t *func) \
{ \
	if ( n ) return NULL; \
 \
	for (int i = 0; f##_func_table[i].name != NULL; i++) \
	{ \
		if (f##_func_table[i].func == func) \
			return f##_func_table[i].name; \
	} \
 \
	return "(invalid)"; \
} \
 \


FUNC_LOOKUPS(song,SONG_FUN,!func)
FUNC_LOOKUPS(presong,SONG_FUN,!func)

void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);
void __token_remove_trigger(TOKEN_INDEX_DATA *token, int type);
bool __token_add_trigger(TOKEN_INDEX_DATA *token, int type, char *phrase, SCRIPT_DATA *script);

void deduct_mana(CHAR_DATA *ch,int cost);

bool validate_song_target(CHAR_DATA *ch,int type,char *arg, int *targ, CHAR_DATA **vict)
{
	CHAR_DATA *victim = NULL;
	
	switch(type) {
	case TAR_CHAR_FORMATION:
	case TAR_IGNORE:
		*targ = TARGET_NONE;
		*vict = NULL;
		return true;

	case TAR_CHAR_OFFENSIVE:
		if (!arg[0]) victim = ch->fighting;
		else victim = get_char_room(ch, NULL, arg);

		if (!victim) {
			if (!arg[0])
				send_to_char("Play it on whom?\n\r", ch);
			else
				send_to_char("They aren't here.\n\r", ch);
			return false;
		}

		if (is_safe(ch, victim, TRUE) ||
			(victim->fighting && !is_same_group(ch, victim->fighting) &&
			ch != victim && !IS_SET(ch->in_room->room_flag[1], ROOM_MULTIPLAY))) {
			send_to_char("Not on that target.\n\r", ch);
			return false;
		}

		if (ch->fighting && !is_same_group(victim, ch->fighting) && ch != victim && !IS_NPC(victim)) {
			act("You must finish your fight before attacking $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		*targ = TARGET_CHAR;
		*vict = victim;
		return true;

	case TAR_CHAR_DEFENSIVE:
		if (arg[0]) {
			victim = get_char_room(ch, NULL, arg);
			if (!victim) {
				send_to_char("They aren't here.\n\r", ch);
				return false;
			}

			if (victim != ch && victim->fighting && victim->fighting != ch &&
				!is_same_group(ch, victim->fighting) && !IS_NPC(victim) &&
				!IS_NPC(victim->fighting) && !is_pk(ch) && !IS_SET(ch->in_room->room_flag[0], ROOM_ARENA)) {
				send_to_char("You can't interfere in a PK battle if you are not PK.\n\r", ch);
				return false;
			}
		} else
			victim = ch;

		*targ = TARGET_CHAR;
		*vict = victim;
		return true;

	case TAR_CHAR_SELF:
		if (arg[0] && str_cmp(arg, "me") && str_cmp(arg, "self") && str_prefix(arg, ch->name)) {
			send_to_char("You may not cast this spell on another.\n\r", ch);
			return false;
		}

		*vict = ch;
		*targ = TARGET_CHAR;
		break;

	case TAR_OBJ_CHAR_OFF:
		if (!arg[0] && !ch->fighting) {
			send_to_char("Play it on whom?\n\r", ch);
			return FALSE;
		}

		if (ch->fighting && !arg[0])
			victim = ch->fighting;
		else
			victim = get_char_room(ch, NULL, arg);

		if (!victim) {
			send_to_char("They aren't here.\n\r", ch);
			return false;
		}

		if ((is_safe(ch, victim, TRUE) ||
			(victim->fighting && ch != victim && !is_same_group(ch, victim->fighting) &&
			!IS_SET(ch->in_room->room_flag[1], ROOM_MULTIPLAY)))) {
			send_to_char("Not on that target.\n\r", ch);
			return false;
		}
		
		*targ = TARGET_CHAR;
		*vict = victim;
		return true;

	case TAR_OBJ_CHAR_DEF:
		if (!arg[0])
			victim = ch;
		else {
			victim = get_char_room(ch, NULL, arg);
		}

		if (!victim) {
			send_to_char("They aren't here.\n\r", ch);
			return false;
		}

		if (victim != ch && victim->fighting && victim->fighting != ch &&
			!is_same_group(ch, victim->fighting) && !IS_NPC(victim) &&
			!IS_NPC(victim->fighting) && !is_pk(ch) && !IS_SET(ch->in_room->room_flag[0], ROOM_ARENA)) {
			send_to_char("You can't interfere in a PK battle if you are not PK.\n\r", ch);
			return false;
		}

		*targ = TARGET_CHAR;
		*vict = victim;
		return true;

	case TAR_IGNORE_CHAR_DEF:
		if (!arg[0]) {
			send_to_char("Play it on whom?\n\r", ch);
			return false;
		}

		victim = get_char_world(ch, arg);
		if (!victim) {
			send_to_char("They aren't anywhere in Sentience.\n\r", ch);
			return false;
		}

		*targ = TARGET_CHAR;
		*vict = victim;
		return true;

	}

	if (!arg[0]) {
		send_to_char("Play it on whom?\n\r", ch);
		return FALSE;
	}

	send_to_char("They aren't anywhere in Sentience.\n\r", ch);
	return false;
}

void do_play(CHAR_DATA *ch, char *argument)
{
	SKILL_ENTRY *entry;
    OBJ_DATA *instrument;
    CHAR_DATA *mob;
	SCRIPT_DATA *script = NULL;
	ITERATOR it;
	PROG_LIST *prg;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    int chance = 0;
    int target;
    int mana;
    int beats;

    if( IS_NPC(ch) ) return;	// No NPC bards

    argument = one_argument(argument,arg);
    argument = one_argument(argument,arg2);

    if ((chance = get_skill(ch, gsk_music)) == 0)
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
			add_buf(buffer, "{YSong Title                            Level         Mana        Rating{x\n\r");
			add_buf(buffer, "{Y-----------------------------------------------------------------------{x\n\r");

			for(entry = ch->sorted_songs; entry; entry = entry->next)
			{
				// the 18s takes into account the four characters in the color codes.
				sprintf(buf, "%-30s %10d %13d %18s\n\r", entry->song->name, entry->song->level, entry->song->mana,
					((entry->rating < 100) ? formatf("{Y%d%%{x", entry->rating) : "{MMaster{x"));
				add_buf(buffer, buf);
			}
			page_to_char(buf_string(buffer), ch);
			free_buf(buffer);
		}
		else
			send_to_char( "There are no songs you can play at this time.\n\r", ch );

		return;
	}

    entry = skill_entry_findname(ch->sorted_songs, arg);

    if (!entry)
    {
		send_to_char("You don't know that song.\n\r", ch);
		return;
    }

	instrument = NULL;
	if (!IS_SET(entry->song->flags, SONG_VOICE_ONLY))
	{
		// Find equipped instrument
		for (instrument = ch->carrying; instrument != NULL; instrument = instrument->next_content )
		{
			if ( IS_INSTRUMENT(instrument) && instrument->wear_loc != WEAR_NONE)
			{
				break;
			}
		}

		if (IS_SET(entry->song->flags, SONG_INSTRUMENT_ONLY) && instrument == NULL)
		{
			send_to_char("You are not using an instrument.\n\r", ch);
			return;
		}
	}

	// Not using an instrument so make sure they can actually sing.
	if (instrument == NULL && IS_AFFECTED2(ch, AFF2_SILENCE))
	{
		send_to_char("You are unable to command your voice.\n\r", ch);
		return;
	}

	// Check offensive songs for room safety
	if ((entry->song->target == TAR_CHAR_OFFENSIVE || entry->song->target == TAR_OBJ_CHAR_OFF) &&
		IS_SET(ch->in_room->room_flag[0], ROOM_SAFE))
	{
		send_to_char("This room is sanctioned by the gods.\n\r", ch);
		return;
	}

	// Get the song target
	if (!validate_song_target(ch, entry->song->target, arg2, &target, &mob))
		return;

    if( IS_VALID(entry->token) )
    {
		// Check that the token has the right scripts
		// Check thst the token is a valid token spell
		script = NULL;
		if( entry->token->pIndexData->progs ) {
			iterator_start(&it, entry->token->pIndexData->progs[TRIGSLOT_SPELL]);
			while(( prg = (PROG_LIST *)iterator_nextdata(&it))) {
				if(is_trigger_type(prg->trig_type,TRIG_TOKEN_SONG)) {
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

		// Mob is $(victim) (if valid, target == TARGET_CHAR, otherwise TARGET_IGNORE)
		// Instrument is $(obj1) (if not valid, voice is used)
		int ret = p_percent_trigger(NULL,NULL,NULL,entry->token,ch,mob,NULL, instrument, NULL, TRIG_TOKEN_PRESONG, NULL,ch->tot_level,0,0,0,0);
		if(ret != 0)
		{
			if (ret != PRET_SILENT)
			{
				send_to_char("You are unable to play that song.\n\r", ch);
			}

			return;
		}
	}
	else
	{
		script = NULL;
		target = entry->song->target;
		beats = entry->song->beats;

		if (entry->song->presong_fun)
		{
			// Messages must be handled by presong
			if (!(*(entry->song->presong_fun))(entry->song, ch->tot_level, ch, instrument, mob, target))
				return;
		}
	}

	mana = entry->song->mana;
	beats = entry->song->beats;

	// Adjust the mana cost
	if (instrument != NULL)
	{
		if (INSTRUMENT(instrument)->mana_min > 0 && INSTRUMENT(instrument)->mana_max > 0)
		{
			int scale = number_range(INSTRUMENT(instrument)->mana_min, INSTRUMENT(instrument)->mana_max);

			mana = scale * mana / 100;
		}
	}


	if ((ch->mana + ch->manastore) < mana) {
		send_to_char("You don't have enough mana.\n\r", ch);
		return;
	}

	// Should have the target mob and type
	if (target == TARGET_CHAR)
	{
		if (mob == ch)
		{
			if(instrument == NULL)
			{
				// Vocal
				send_to_char("{YYou begin to sing the song softly to yourself...{X\n\r", ch);
				act( "{Y$n begins to sing a song softly to $mself...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				// Instrument
				act( "{YYou begin to play the song on $p{Y softly to yourself...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_CHAR);
				act( "{Y$n begins to play a song on $p{Y softly to $mself...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_ROOM);
			}
		}
		else
		{
			if(instrument == NULL)
			{
				// Vocal
				act("{YYou begin to sing the song, sweetly exerting its influence on $N...{X", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{Y$n begins to sing a song, exerting its influence on $N...{X", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				act("{Y$n begins to sing a song, exerting its influence on you...{X", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			}
			else
			{
				// Instrument
				act("{YYou begin to play the song on $p{Y, sweetly exerting its influence on $N...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_CHAR);
				act("{Y$n begins to play a song on $p{Y, exerting its influence on $N...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_NOTVICT);
				act("{Y$n begins to play a song on $p{Y, exerting its influence on you...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_VICT);
			}

		}

		ch->music_target = str_dup(mob->name);
	}
	else
	{
		if (instrument == NULL)
		{
			act( "{YYou begin to sing a song...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n begins to sing a song...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act( "{YYou begin to play a song on $p{Y...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n begins to play a song on $p{Y...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_ROOM);
		}
	}

	// Setup targets.
	ch->song = entry;
	ch->song_token = IS_VALID(entry->token) ? entry->token : NULL;
	ch->song_script = script;
	ch->song_mana = mana;
	ch->song_instrument = instrument;

	// Block to deal with reductions
	{
		int scale1 = 1;
		int scale2 = 1;

		/* Sage has shorter playing time if Bard before */
		if (IS_SAGE(ch) && ch->pcdata->sub_class_thief == CLASS_THIEF_BARD)
		{
			scale1 *= 2;
			scale2 *= 3;	// Give a 1/3 reduction
		}

		if( instrument != NULL )
		{
			// Only do it if both are set
			if (INSTRUMENT(instrument)->beats_min > 0 && INSTRUMENT(instrument)->beats_max > 0)
			{
				int scale = number_range(INSTRUMENT(instrument)->beats_min, INSTRUMENT(instrument)->beats_max);

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
    CHAR_DATA *mob;
	TOKEN_DATA *token = NULL;
	SONG_DATA *song = NULL;
	int mana;
	int type;
	bool offensive = FALSE;

	int chance = get_skill(ch, gsk_music) * ch->song->rating;

	if (chance < number_range(0, 9999))
	{
		// Failed
		if (ch->song_instrument == NULL)
		{
			act( "{YYou mispronounce a word while singing your song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n mispronounces a word while singing $s song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act( "{YYou play an incorrect note on $p{Y.{x", ch, NULL, NULL, ch->song_instrument, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n plays an incorrect note on $p{Y.{x", ch, NULL, NULL, ch->song_instrument, NULL, NULL, NULL, TO_ROOM);
		}
		
		check_improve_song(ch, ch->song->song, false, 5);
		check_improve(ch, gsk_music, false, 5);

		free_string(ch->music_target);
		ch->music_target = NULL;
		ch->song_token = NULL;
		ch->song_script = NULL;
		ch->song = NULL;
		ch->song_instrument = NULL;
		return;
	}
	else
	{
		if (ch->song_instrument == NULL)
		{
			act( "{YYou finish singing your song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n finishes $s song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act( "{YYou finish playing your song on $p{Y.{x", ch, NULL, NULL, ch->song_instrument, NULL, NULL, NULL, TO_CHAR);
			act( "{Y$n finishes $s song on $p{Y.{x", ch, NULL, NULL, ch->song_instrument, NULL, NULL, NULL, TO_ROOM);
		}
	}

	if(ch->song_token) {
		token = ch->song_token;
	}

	song = ch->song->song;
	type = song->target;
	mana = song->mana;
	offensive = (type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF);

	// TODO: Handle instrument modification of mana costs

	deduct_mana(ch, mana);

    // We are casting it on just one person only
    if (ch->music_target != NULL)
    {
		// TARGET_CHAR
		mob = get_char_room(ch, NULL, ch->music_target);
		if ( mob == NULL )
		{
			send_to_char("They aren't here.\n\r", ch);
			free_string(ch->music_target);
			ch->music_target = NULL;
			ch->song_token = NULL;
			ch->song_script = NULL;
			ch->song = NULL;
			ch->song_instrument = NULL;
			return;
		}
		else
		{
			char *music_target_name = ch->music_target;
			ch->music_target = NULL;

			if (IS_VALID(token))
			{
				if (p_token_index_percent_trigger(song->token, ch, mob, NULL, NULL, NULL, TRIG_TOKEN_SONG, music_target_name, 0,0,0,0,0, ch->tot_level,0,0,0,0) > 0)
					offensive = true;
			}
			else if (song->song_fun)
			{
				if((*(song->song_fun))(song, ch->tot_level, ch, ch->song_instrument, mob, TARGET_CHAR))
					offensive = true;
			}

			free_string(music_target_name);

			if (mob != ch && !is_safe(ch, mob, FALSE) && (type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF || offensive))
			{
				multi_hit(ch, mob, NULL, TYPE_UNDEFINED);
			}

			ch->song_token = NULL;
			ch->song_script = NULL;
			ch->song = NULL;
			ch->song_instrument = NULL;
			return;
		}
    }
    else
    {
		// TARGET_IGNORE
		if (IS_VALID(token))
		{
			if (p_token_index_percent_trigger(song->token, ch, NULL, NULL, NULL, NULL, TRIG_TOKEN_SONG, NULL, 0,0,0,0,0, ch->tot_level,0,0,0,0) > 0)
				offensive = true;
		}
		else if (song->song_fun)
		{
			if((*(song->song_fun))(song, ch->tot_level, ch, ch->song_instrument, NULL, TARGET_NONE))
				offensive = true;
		}

		ch->song_token = NULL;
		ch->song_script = NULL;
		ch->song = NULL;
		ch->song_instrument = NULL;
	}

	check_improve_song(ch, song, true, 2);
	check_improve(ch, gsk_music, true, 2);
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

SONG_DATA *get_song_data( char *name)
{
	ITERATOR it;
	SONG_DATA *song;

	iterator_start(&it, songs_list);
	while((song = (SONG_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(name, song->name))
			break;
	}
	iterator_stop(&it);

	return song;
}

void save_song(FILE *fp, SONG_DATA *song)
{
	fprintf(fp, "#SONG %s~ %d\n", song->name, song->uid);

	fprintf(fp, "Level %d\n", song->level);
	fprintf(fp, "Beats %d\n", song->beats);
	fprintf(fp, "Mana %d\n", song->mana);
	fprintf(fp, "Target %s~\n", flag_string(song_target_types, song->target));
	fprintf(fp, "Flags %s\n", print_flags(song->flags));
	fprintf(fp, "Rating %d\n", song->rating);

	if (song->token)
	{
		// Tokens use triggers instead of the special functions
		fprintf(fp, "Token %ld#%ld\n", song->token->area->uid, song->token->vnum);
	}
	else
	{
		if(song->presong_fun)
			fprintf(fp, "PreSongFunc %s~\n", presong_func_name(song->presong_fun));

		if(song->song_fun)
			fprintf(fp, "SongFunc %s~\n", song_func_name(song->song_fun));
	}

	fprintf(fp, "#-SONG\n");
}

void save_songs(void)
{
	FILE *fp;

	log_string("save_songs: saving " SONGS_FILE);
	if ((fp = fopen(SONGS_FILE, "w")) == NULL)
	{
		bug("save_songs: fopen", 0);
		perror(SONGS_FILE);
	}
	else
	{
		ITERATOR it;
		SONG_DATA *song;
		iterator_start(&it, songs_list);
		while((song = (SONG_DATA *)iterator_nextdata(&it)))
		{
			save_song(fp, song);
		}
		iterator_stop(&it);

		fprintf(fp, "End\n");
		fclose(fp);
	}
}

SONG_DATA *load_song(FILE *fp)
{
	SONG_DATA *song = new_song_data();

	char buf[MSL];
	char *word;
	bool fMatch;

	song->name = fread_string(fp);
	song->uid = fread_number(fp);

    while (str_cmp((word = fread_word(fp)), "#-SONG"))
	{
		switch(word[0])
		{
			case 'B':
				KEY("Beats", song->beats, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", song->flags, fread_flag(fp));
				break;
			
			case 'L':
				KEY("Level", song->level, fread_number(fp));
				break;

			case 'M':
				KEY("Mana", song->mana, fread_number(fp));
				break;

			case 'P':
				if (!str_cmp(word, "PreSongFunc"))
				{
					char *name = fread_string(fp);

					song->presong_fun = presong_func_lookup(name);
					if (!song->presong_fun)
					{
						log_stringf("load_song: Invalid PreSongFunc '%s' for song '%s'", name, song->name);
					}
					fMatch = true;
					break;
				}
				break;

			case 'R':
				KEY("Rating", song->rating, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "SongFunc"))
				{
					char *name = fread_string(fp);

					song->song_fun = song_func_lookup(name);
					if (!song->song_fun)
					{
						log_stringf("load_song: Invalid SongFunc '%s' for song '%s'", name, song->name);
					}
					fMatch = true;
					break;
				}
				break;

			case 'T':
				if (!str_cmp(word, "Target"))
				{
					int value = stat_lookup(fread_string(fp), song_target_types, TAR_IGNORE);

					song->target = value;
					fMatch = true;
					break;
				}
		}

		if (!fMatch)
		{
			sprintf(buf, "load_song: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return song;
}

void insert_song(SONG_DATA *song)
{
	ITERATOR it;
	SONG_DATA *sg;
	iterator_start(&it, songs_list);
	while((sg = (SONG_DATA *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(song->name, sg->name);
		if(cmp < 0)
		{
			iterator_insert_before(&it, song);
			break;
		}
	}
	iterator_stop(&it);

	if (!sg)
	{
		list_appendlink(songs_list, song);
	}
}

static void delete_song_data(void *ptr)
{
	free_song_data((SONG_DATA *)ptr);
}

bool load_songs(void)
{
	SONG_DATA *song;
	FILE *fp;
	char buf[MSL];
	char *word;
	bool fMatch;
	top_song_uid = 0;

	log_string("load_songs: creating songs_list");
	songs_list = list_createx(FALSE, NULL, delete_song_data);
	if (!IS_VALID(songs_list))
	{
		log_string("songs_list was not created.");
		return false;
	}

	log_string("load_songs: loading " SONGS_FILE);
	if ((fp = fopen(SONGS_FILE, "r")) == NULL)
	{
		bug("Songs file does not exist.", 0);
		perror(SONGS_FILE);
		return false;
	}
	
	while(str_cmp((word = fread_word(fp)), "End"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#SONG"))
			{
				song = load_song(fp);
				if (song)
				{
					insert_song(song);

					if (song->uid > top_song_uid)
						top_song_uid = song->uid;
				}
				else
					log_string("Failed to load a song.");

				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch) {
			sprintf(buf, "load_songs: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return true;
}

SONG_FUNC( song_debugging )
{
	// Just spit out the song name to the musician
	send_to_char(formatf("%s!\n\r", song->name), ch);
	return true;
}

SONG_FUNC( song_purple_mist )
{
	// Armour
	// Shield
	return true;
}

SONG_FUNC( song_fireworks )
{
	// Magic Missile
	// Lightning Bolt
	return true;
}

SONG_FUNC( song_dwarven_tale )
{
	// Stone Skin
	// Infravision
	return true;
}

SONG_FUNC( song_fade_to_black )
{
	// Improved Invisibility
	return true;
}

SONG_FUNC( song_pretty_in_pink )
{
	// Faerie Fire
	return true;
}

SONG_FUNC( song_aquatic_polka )
{
	// Underwater Breathing
	return true;
}

SONG_FUNC( song_another_gate )
{
	// Dispel Magic
	return true;
}

SONG_FUNC( song_awareness_jig )
{
	// Detect Invis
	// Detect Hidden
	// Detect Magic
	return true;
}

SONG_FUNC( song_swamp_song )
{
	// Poison
	// Acid Blast
	return true;
}

SONG_FUNC( song_fat_owl_hopping )
{
	// Giant Strength
	// Fly
	return true;
}

SONG_FUNC( song_stormy_weather )
{
	// Call Lightning
	// Call Lightning
	// Call Lightning
	return true;
}

SONG_FUNC( song_rigor )
{
	// Death Grip
	// Frenzy
	return true;
}

SONG_FUNC( song_firefly_tune )
{
	// Heal
	// Refresh
	return true;
}

SONG_FUNC( song_blessed_be )
{
	// Cure Poison
	// Cure Disease
	// Cure Blindness
	return true;
}

SONG_FUNC( song_dark_cloud )
{
	// Blindness
	return true;
}

SONG_FUNC( song_curse_of_the_abyss )
{
	// Fireball
	// Energy Drain
	// Demonfire
	return true;
}



///////////////////////
// SONGEDIT

void songedit_show_trigger(CHAR_DATA *ch, BUFFER *buffer, LLIST **progs, int trigger, char *command, char *label)
{
	char buf[MSL];

	PROG_LIST *pr = find_trigger_data(progs, trigger, 1);

	int width = 20 - strlen_no_colours(label);
	width = UMAX(width, 0);
	if (pr)
	{
		if (IS_NULLSTR(pr->script->name))
			sprintf(buf, formatf("%%s%%%ds%%s\n\r", width),
				MXPCreateSend(ch->desc, command, label), "",
				MXPCreateSend(ch->desc, formatf("tpdump %ld#%ld", pr->script->area->uid, pr->script->vnum),
				formatf("{W%ld{x#{W%ld{x", pr->script->area->uid, pr->script->vnum)));
		else
			sprintf(buf, formatf("%%s%%%ds%%s\n\r", width),
				MXPCreateSend(ch->desc, command, label), "",
				MXPCreateSend(ch->desc, formatf("tpdump %ld#%ld", pr->script->area->uid, pr->script->vnum),
				formatf("{W%s {x({W%ld{x#{W%ld{x)", pr->script->name, pr->script->area->uid, pr->script->vnum)));
	}
	else
		sprintf(buf, formatf("%%s%%%ds%%s\n\r", width),
				MXPCreateSend(ch->desc, command, label), "", "{D(unset){x");
	add_buf(buffer, buf);
}


SONGEDIT( songedit_show )
{
	char buf[MSL];
	BUFFER *buffer;
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	buffer = new_buf();

	sprintf(buf, "Song: {W%s {x({W%d{x)\n\r", song->name, song->uid);
	add_buf(buffer, buf);

	olc_buffer_show_string(ch, buffer, formatf("%d", song->level), "level", "Level:", 20, "xDW");
	olc_buffer_show_string(ch, buffer, formatf("%d", song->beats), "beats", "Beats:", 20, "xDW");
	olc_buffer_show_string(ch, buffer, formatf("%d", song->mana), "mana", "Mana:", 20, "xDW");
	olc_buffer_show_string(ch, buffer, formatf("%d", song->rating), "difficulty", "Difficulty:", 20, "xDW");

	olc_buffer_show_flags_ex(ch, buffer, song_flags, song->flags, "flags", "Flags:", 77, 20, 5, "xxYyCcD");
	olc_buffer_show_flags_ex(ch, buffer, song_target_types, song->target, "target", "Target:", 77, 20, 5, "xxYyCcD");

	if (song->token)
	{
		sprintf(buf, "Token:              {W%s {x({W%ld{x#{W%ld{x)\n\r",
			MXPCreateSend(ch->desc, formatf("tshow %ld#%ld", song->token->area->uid, song->token->vnum), song->token->name),
			song->token->area->uid, song->token->vnum);
		add_buf(buffer, buf);

		songedit_show_trigger(ch, buffer, song->token->progs, TRIG_TOKEN_PRESONG,	"presong",	"  {W+ {xPreSong:");
		songedit_show_trigger(ch, buffer, song->token->progs, TRIG_TOKEN_SONG,	"song",		"  {W+ {xSong:");
	}
	else
	{
		olc_buffer_show_string(ch, buffer, presong_func_display(song->presong_fun),	"presong",	"PreSong:", 20, "XDW");
		olc_buffer_show_string(ch, buffer, song_func_display(song->song_fun),		"song",		"Song:", 20, "XDW");
	}

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		send_to_char(buffer->string, ch);
		//page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
}

SONGEDIT( songedit_list )
{
	char buf[MSL];
	BUFFER *buffer = new_buf();

	ITERATOR it;
	SONG_DATA *song;

	add_buf(buffer, "Songs:\n\r");
	add_buf(buffer, "[Uid] [        Name        ]\n\r");
	add_buf(buffer, "=============================\n\r");

	iterator_start(&it, songs_list);
	while((song = (SONG_DATA *)iterator_nextdata(&it)))
	{
		sprintf(buf, " %s   {%c%s{x\n",
			MXPCreateSend(ch->desc, formatf("songedit %s", song->name), formatf("%3d", song->uid)),
			(song->token ? 'G' : 'Y'),
			MXPCreateSend(ch->desc, formatf("songshow %s", song->name), song->name));
		add_buf(buffer, buf);
	}
	iterator_stop(&it);

	add_buf(buffer, "-----------------------------\n\r");
	sprintf(buf, "Total: %d\n\r", list_size(songs_list));
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


bool song_exists(const char *name)
{
	ITERATOR it;
	SONG_DATA *song;

	iterator_start(&it, songs_list);
	while((song = (SONG_DATA *)iterator_nextdata(&it)))
	{
		if (!str_cmp(name, song->name))
			break;
	}
	iterator_stop(&it);

	return song != NULL;
}


// songedit install source <name>
// songedit install token <widevnum>
SONGEDIT( songedit_install )
{
	char arg[MIL];
	SONG_DATA *song;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  songedit install {Rsource{x <name>\n\r", ch);
		send_to_char("         songedit install {Rtoken{x <widevnum>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "source"))
	{
		smash_tilde(argument);
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  songedit source {R<name>{x\n\r", ch);
			send_to_char("Please specify a name.\n\r", ch);
			return false;
		}

		if (song_exists(argument))
		{
			send_to_char(formatf("The name '{W%s{x' is already in use.\n\r", argument), ch);
			return false;
		}

		song = new_song_data();
		song->uid = ++top_song_uid;
		song->name = str_dup(argument);
		song->token = NULL;

		insert_song(song);

		send_to_char(formatf("Song {W%s{x installed.\n\r", song->name), ch);

		olc_set_editor(ch, ED_SONGEDIT, song);
		return true;
	}

	if (!str_prefix(arg, "token"))
	{
		WNUM wnum;

		if (!parse_widevnum(argument, NULL, &wnum))
		{
			send_to_char("Syntax:  songedit install token {R<widevnum>{x\n\r", ch);
			send_to_char("Please specify a widevnum.\n\r", ch);
			return false;
		}

		TOKEN_INDEX_DATA *token = get_token_index(wnum.pArea, wnum.vnum);
		if (!token)
		{
			send_to_char("No such token by that widevnum.\n\r", ch);
			return false;
		}

		if (token->type != TOKEN_SONG)
		{
			send_to_char("Token must be a SONG token.\n\r", ch);
			return false;
		}

		if (song_exists(token->name))
		{
			send_to_char(formatf("The name '{W%s{x' is already in use.\n\r", token->name), ch);
			return false;
		}

		song = new_song_data();
		song->uid = ++top_song_uid;
		song->name = str_dup(token->name);
		song->token = token;

		insert_song(song);

		send_to_char(formatf("Song {W%s{x installed.\n\r", song->name), ch);

		olc_set_editor(ch, ED_SONGEDIT, song);
		return true;
	}

	songedit_install(ch, "");
	return false;
}

#define SONGEDIT_FUNC(f, p, t, n)		\
SONGEDIT( songedit_##f##func )	\
{ \
	char arg[MIL]; \
	SONG_DATA *song; \
\
	EDIT_SONG(ch, song); \
\
	if (song->token) \
	{ \
		if (argument[0] == '\0') \
		{ \
			send_to_char("Syntax:  songedit " #f " {Rset{x <widevnum>\n\r", ch); \
			send_to_char("         songedit " #f " {Rclear{x\n\r", ch); \
			return false; \
		} \
\
		argument = one_argument(argument, arg); \
\
		if (!str_prefix(arg, "set")) \
		{ \
			/* TOKEN mode */ \
			WNUM wnum; \
\
			/* Allow wnum shortcutting using the token's area */ \
			if (!parse_widevnum(argument, song->token->area, &wnum)) \
			{ \
				send_to_char("Syntax:  songedit " #f " set {R<widevnum>{x\n\r", ch); \
				send_to_char("Please specify a widevnum for the token " #p " trigger.\n\r", ch); \
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
			__token_remove_trigger(song->token, TRIG_##p); \
\
			/* Add trigger to token. */ \
			if (!__token_add_trigger(song->token, TRIG_##p, "100", script)) \
			{ \
				send_to_char("Something went wrong adding " #p " trigger to token.\n\r", ch); \
				return false; \
			} \
\
			/* Mark area as changed. */ \
			SET_BIT(song->token->area->area_flags, AREA_CHANGED); \
			send_to_char(#p " trigger added to spell token.\n\r", ch); \
			return true; \
		} \
\
		if (!str_prefix(arg, "clear")) \
		{ \
			/* Remove any triggers from token. */ \
			__token_remove_trigger(song->token, TRIG_##p); \
\
			/* Mark area as changed. */ \
			SET_BIT(song->token->area->area_flags, AREA_CHANGED); \
			send_to_char(#p " trigger cleared on spell token.\n\r", ch); \
			return true; \
		} \
\
		songedit_##f##func (ch, ""); \
		return false; \
	} \
	else \
	{ \
		if (argument[0] == '\0') \
		{ \
			send_to_char("Syntax:  songedit " #f " {Rset{x <function>\n\r", ch); \
			send_to_char("         songedit " #f " {Rclear{x\n\r", ch); \
			return false; \
		} \
\
		argument = one_argument(argument, arg); \
\
		if (!str_prefix(arg, "set")) \
		{ \
			if (argument[0] == '\0') \
			{ \
				send_to_char("Syntax:  songedit " #f " set {R<function>{x\n\r", ch); \
				send_to_char("Invalid " #f " function.  Use '? " #f "_func' for a list of functions.\n\r", ch); \
				return false; \
			} \
\
			t *func = f##_func_lookup(argument); \
			if(!func) \
			{ \
				send_to_char("Syntax:  songedit " #f " set {R<function>{x\n\r", ch); \
				send_to_char("Invalid " #f " function.  Use '? " #f "_func' for a list of functions.\n\r", ch); \
				return false; \
			} \
\
			song->f##_fun = func; \
			send_to_char("Song " #f " function set.\n\r", ch); \
			return true; \
		} \
\
		if (!str_prefix(arg, "clear")) \
		{ \
			song->f##_fun = n; \
			send_to_char("Song " #f " function cleared.\n\r", ch); \
			return true; \
		} \
\
		songedit_##f##func (ch, ""); \
		return false; \
	} \
}

SONGEDIT_FUNC(presong,TOKEN_PRESONG,SONG_FUN,NULL)
SONGEDIT_FUNC(song,TOKEN_SONG,SONG_FUN,NULL)

SONGEDIT( songedit_flags )
{
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int value;
	if ((value = flag_value(song_flags, argument)) == NO_FLAG)
	{
		send_to_char("Syntax:  songedit flags {R<flags>{x\n\r", ch);
		send_to_char("Invalid song flags.  Use '? song' to see list of valid flags.\n\r", ch);
		show_flag_cmds(ch, song_flags);
		return false;
	}

	// Check the new flags for any problems
	long new_value = song->flags ^ value;

	if (IS_SET(new_value, SONG_INSTRUMENT_ONLY) && IS_SET(new_value, SONG_VOICE_ONLY))
	{
		send_to_char("{Winstrument_only{x and {Wvoice_only{x are mutually exclusive.\n\r", ch);
		return false;
	}

	song->flags = new_value;
	send_to_char("Song target set.\n\r", ch);
	return true;
}

SONGEDIT( songedit_difficulty )
{
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int difficulty;
	if (!is_number(argument) || (difficulty = atoi(argument)) < 1)
	{
		send_to_char("Syntax:  songedit difficulty {R<difficulty>{x\n\r", ch);
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	song->rating = difficulty;
	send_to_char("Song difficulty set.\n\r", ch);
	return true;
}

SONGEDIT( songedit_level )
{
	char buf[MSL];
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int level;
	if (!is_number(argument) || (level = atoi(argument)) < 1 || level > MAX_CLASS_LEVEL)
	{
		sprintf(buf, "Syntax:  songedit level {R<1-%d>{x\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		sprintf(buf, "Please specify a number from 1 to %d.\n\r", MAX_CLASS_LEVEL);
		send_to_char(buf, ch);
		return false;
	}

	song->level = level;
	send_to_char("Song level set.\n\r", ch);
	return true;
}

SONGEDIT( songedit_mana )
{
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int mana;
	if (!is_number(argument) || (mana = atoi(argument)) < 0)
	{
		send_to_char("Syntax:  songedit mana {R<mana>{x\n\r", ch);
		send_to_char("Please specify a non-negative number.\n\r", ch);
		return false;
	}

	song->mana = mana;
	send_to_char("Song mana set.\n\r", ch);
	return true;
}

SONGEDIT( songedit_beats )
{
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int beats;
	if (!is_number(argument) || (beats = atoi(argument)) < 1)
	{
		send_to_char("Syntax:  songedit beats {R<beats>{x\n\r", ch);
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	song->beats = beats;
	send_to_char("Song beats set.\n\r", ch);
	return true;
}

SONGEDIT( songedit_target )
{
	SONG_DATA *song;

	EDIT_SONG(ch, song);

	int value;
	if ((value = stat_lookup(argument, song_target_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  songedit target {R<target>{x\n\r", ch);
		send_to_char("Invalid song target.  Use '? song_targets' to see list of valid target types.\n\r", ch);
		show_flag_cmds(ch, song_target_types);
		return false;
	}

	song->target = value;
	send_to_char("Song target set.\n\r", ch);
	return true;
}

void check_improve_song_show( CHAR_DATA *ch, SONG_DATA *song, bool success, int multiplier, bool show )
{
    int chance;
    char buf[100];
    SKILL_ENTRY *entry;

    if (IS_NPC(ch))
		return;

    if (IS_SOCIAL(ch))
		return;

	entry = skill_entry_findsong(ch->sorted_songs, song);
	if(!entry)
		return;

	if(!IS_SET(entry->flags, SKILL_IMPROVE))
		return;

	if (!IS_IMMORTAL(ch) && ch->pcdata->sub_class_thief != CLASS_THIEF_BARD)
		return;

    if (entry->rating <= 0 || entry->rating == 100)
		return;

	int diff = (song->rating > 0) ? song->rating : 10;

    // check to see if the character has a chance to learn
    chance      = 10 * int_app[get_curr_stat(ch, STAT_INT)].learn;
    multiplier  = UMAX(multiplier,1);
    chance     /= (multiplier * diff * 2);	// Songs should be easier than skills
    chance     += ch->level;

	//if (IS_IMMORTAL(ch))
	//{
	//	send_to_char(formatf("chance = %d, success = %s\n\r", chance, success?"true":"false"), ch);
	//}

    if (number_range(1,1000) > chance)
		return;

    // now that the character has a CHANCE to learn, see if they really have
    if (success)
    {
		chance = URANGE(2, 100 - entry->rating, 25);
		if (number_percent() < chance)
		{
			sprintf(buf,"{WYou have become better at %s!{x\n\r", song->name);
			send_to_char(buf,ch);
			entry->rating++;
			gain_exp(ch, 2 * diff);
		}
    }
    else
    {
		chance = URANGE(5, entry->rating/2, 30);
		if (number_percent() < chance)
		{
			sprintf(buf, "{WYou learn from your mistakes, and your %s skill improves.{x\n\r", song->name);
			send_to_char(buf, ch);
			entry->rating += number_range(1,3);
			entry->rating = UMIN(entry->rating,100);
			gain_exp(ch,diff);
		}
    }
}

void check_improve_song( CHAR_DATA *ch, SONG_DATA *song, bool success, int multiplier )
{
	check_improve_song_show( ch, song, success, multiplier, true );
}
