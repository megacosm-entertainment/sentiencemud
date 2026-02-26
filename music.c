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
#include "skill_data.h"
#include "song_data.h"

void deduct_mana(CHAR_DATA *ch,int cost);

void do_play(CHAR_DATA *ch, char *argument)
{
    SKILL_ENTRY *entry;
    OBJ_DATA *instrument = NULL;
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

    if ((chance = get_skill(ch, skill_resolve_gsn("music"))) == 0)
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
                    sprintf(buf, "%-30s %10d %13d\n\r", name, level, entry->song->mana);
                add_buf(buffer, buf);
            }
            page_to_char(buf_string(buffer), ch);
            free_buf(buffer);
        }
        else
            send_to_char( "There are no songs you can play at this time.\n\r", ch );

        return;
    }

    if (check_social_status(ch))
        return;

    // Check for a worn instrument using lworn
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->item_type == ITEM_INSTRUMENT) {
                instrument = obj;
                iterator_stop(&it);
                break;
            }
        }
        if (!instrument) {
            iterator_stop(&it);
        }
    }

    if (instrument == NULL)
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
        if( entry->token->pIndexData && entry->token->pIndexData->progs ) {
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
        if(p_percent_trigger(NULL,NULL,NULL,entry->token,ch,NULL,NULL, instrument, NULL, TRIG_PRESPELL, NULL))
            return;

        beats = ch->tempstore[0];

        target = entry->token->pIndexData->value[TOKVAL_SPELL_TARGET];
    }
    else
    {
        mana = entry->song->mana;

        if ((ch->mana + ch->manastore) < mana) {
            send_to_char("You don't have enough mana.\n\r", ch);
            return;
        }

        script = NULL;
        target = entry->song->target;
        beats = entry->song->beats;
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
                if ( is_safe( ch, mob, true ) )
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
            act( "{Y$n begins to play a song on $p{Y softly to $mself...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
            else
            {
            act("{YYou begin to play the song, sweetly exerting its influence on $N...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("{Y$n begins to play a song, exerting its influence on $N...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
            act("{Y$n begins to play a song, exerting its influence on you...{X", ch, mob, NULL, instrument, NULL, NULL, NULL, TO_VICT, NULL, NULL);
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
                if (IS_SET(ch->in_room->room_flag[0], ROOM_SAFE)) {
                    send_to_char("This room is sanctioned by the gods.\n\r", ch);
                    return;
                }

            break;
        }
        act( "{YYou begin to play a song on $p{Y...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act( "{Y$n begins to play a song on $p{Y...{x", ch, NULL, NULL, instrument, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }

    // Setup targets.
    ch->song = entry->song;
    ch->song_token = IS_VALID(entry->token) ? entry->token : NULL;
    ch->song_script = script;
    ch->song_mana = mana;
    ch->song_instrument = instrument;

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

        if( instrument != NULL )
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
            if( INSTRUMENT(instrument)->beats_min > 0 && INSTRUMENT(instrument)->beats_max > 0)
            {
                int scale;

                if( INSTRUMENT(instrument)->beats_min < INSTRUMENT(instrument)->beats_max )
                    scale = number_range(INSTRUMENT(instrument)->beats_min, INSTRUMENT(instrument)->beats_max);
                else
                    scale = number_range(INSTRUMENT(instrument)->beats_max, INSTRUMENT(instrument)->beats_min);

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
    int sn;
    SONG_DATA *pSong = NULL;
    bool offensive = false;
    bool wasdead;

    act( "{YYou finish playing your song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act( "{Y$n finishes $s song.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    if(ch->song_token) {
        token = ch->song_token;
        script = ch->song_script;
        type = token->pIndexData->value[TOKVAL_SPELL_TARGET];
        pSong = NULL;
    } else {
        pSong = ch->song;
        type = pSong->target;
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
            ch->song = NULL;
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
                            offensive = true;
                        if (check_spell_deflection(ch, mob, sn))
                            (*skill_table[sn].spell_fun) (skill_find_uid(sn), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                }

                if( (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) && pSong->spell2 )
                {
                    sn = skill_lookup(pSong->spell2);

                    if( sn > 0 && sn < MAX_SKILL && skill_table[sn].spell_fun != spell_null)
                    {
                        if(skill_table[sn].target == TAR_CHAR_OFFENSIVE || skill_table[sn].target == TAR_OBJ_CHAR_DEF)
                            offensive = true;
                        if (check_spell_deflection(ch, mob, sn))
                            (*skill_table[sn].spell_fun) (skill_find_uid(sn), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                }

                if( (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) && pSong->spell3 )
                {
                    sn = skill_lookup(pSong->spell3);

                    if( sn > 0 && sn < MAX_SKILL && skill_table[sn].spell_fun != spell_null)
                    {
                        if(skill_table[sn].target == TAR_CHAR_OFFENSIVE || skill_table[sn].target == TAR_OBJ_CHAR_DEF)
                            offensive = true;
                        if (check_spell_deflection(ch, mob, sn))
                            (*skill_table[sn].spell_fun) (skill_find_uid(sn), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                }
            }
            else
            {
                if (check_spell_deflection_token(ch, mob, token, script, music_target_name)) {
                    if( execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL, NULL, NULL,music_target_name,NULL,TRIG_NONE,0,0,0,0,0) > 0)
                        offensive = true;
                }
            }

            free_string(music_target_name);

            if (mob != ch && !is_safe(ch, mob, false) && (type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF || offensive))
            {
                multi_hit(ch, mob, TYPE_UNDEFINED);
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
        int sn1 = -1, sn2 = -1, sn3 = -1;

        if(pSong)
        {
            if( pSong->spell1 )
            {
                sn1 = skill_lookup(pSong->spell1);

                if( sn1 < 1 || sn1 >= MAX_SKILL || skill_table[sn1].spell_fun == spell_null)
                    sn1 = -1;
                else if(skill_table[sn1].target == TAR_CHAR_OFFENSIVE || skill_table[sn1].target == TAR_OBJ_CHAR_DEF)
                    offensive = true;

            }

            if( pSong->spell2 )
            {
                sn2 = skill_lookup(pSong->spell2);

                if( sn2 < 1 || sn2 >= MAX_SKILL || skill_table[sn2].spell_fun == spell_null)
                    sn2 = -1;
                else if(skill_table[sn2].target == TAR_CHAR_OFFENSIVE || skill_table[sn2].target == TAR_OBJ_CHAR_DEF)
                    offensive = true;
            }

            if( pSong->spell3 )
            {
                sn3 = skill_lookup(pSong->spell3);

                if( sn3 < 1 || sn3 >= MAX_SKILL || skill_table[sn3].spell_fun == spell_null)
                    sn3 = -1;
                else if(skill_table[sn3].target == TAR_CHAR_OFFENSIVE || skill_table[sn3].target == TAR_OBJ_CHAR_DEF)
                    offensive = true;
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
                            (*skill_table[sn1].spell_fun) (skill_find_uid(sn1), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                    {
                        if (check_spell_deflection(ch, mob, sn2))
                            (*skill_table[sn2].spell_fun) (skill_find_uid(sn2), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                    {
                        if (check_spell_deflection(ch, mob, sn3))
                            (*skill_table[sn3].spell_fun) (skill_find_uid(sn3), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                }
                else
                {
                    if (check_spell_deflection_token(ch, mob, token, script, NULL))
                        if(execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL, NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0) > 0)
                            offensive = true;
                }

                if (mob != ch && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1])) && (mob->dead == wasdead) && !is_safe(ch, mob, false) && offensive)
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
                        (*skill_table[sn1].spell_fun) (skill_find_uid(sn1), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                    {
                        (*skill_table[sn2].spell_fun) (skill_find_uid(sn2), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                    {
                        (*skill_table[sn3].spell_fun) (skill_find_uid(sn3), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                }
                else
                {
                    execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
                }
            }
            break;

        case TAR_CHAR_OFFENSIVE:
        case TAR_OBJ_CHAR_OFF:
            for ( mob = ch->in_room->people; mob != NULL; mob = mob_next)
            {
                mob_next = mob->next_in_room;

                if ( is_safe( ch, mob, false ) )
                    continue;

                id[0] = mob->id[0];
                id[1] = mob->id[1];
                wasdead = mob->dead;

                if( pSong )
                {

                    if( sn1 > 0 && !is_same_group(ch, mob))
                    {
                        if (check_spell_deflection(ch, mob, sn1))
                            (*skill_table[sn1].spell_fun) (skill_find_uid(sn1), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead) && !is_same_group(ch, mob)) )
                    {
                        if (check_spell_deflection(ch, mob, sn2))
                            (*skill_table[sn2].spell_fun) (skill_find_uid(sn2), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }

                    if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead) && !is_same_group(ch, mob)) )
                    {
                        if (check_spell_deflection(ch, mob, sn3))
                            (*skill_table[sn3].spell_fun) (skill_find_uid(sn3), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                    }
                }
                else
                {
                    if (check_spell_deflection_token(ch, mob, token, script, NULL))
                        if(execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL, NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0) > 0)
                            offensive = true;
                }

                if (mob != ch && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1])) && (mob->dead == wasdead) && !is_safe(ch, mob, false) && offensive)
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
                    (*skill_table[sn1].spell_fun) (skill_find_uid(sn1), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                }

                if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                {
                    (*skill_table[sn2].spell_fun) (skill_find_uid(sn2), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                }

                if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                {
                    (*skill_table[sn3].spell_fun) (skill_find_uid(sn3), ch->tot_level, ch, mob, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                }

            }
            else
            {
                execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
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
                    (*skill_table[sn1].spell_fun) (skill_find_uid(sn1), ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE, INVOC_INTERNAL);
                }

                if( sn2 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                {
                    (*skill_table[sn2].spell_fun) (skill_find_uid(sn2), ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE, INVOC_INTERNAL);
                }

                if( sn3 > 0 && (IS_VALID(mob) && (mob->id[0] == id[0] && mob->id[1] == id[1]) && (mob->dead == wasdead)) )
                {
                    (*skill_table[sn3].spell_fun) (skill_find_uid(sn3), ch->tot_level, ch, mob, TARGET_NONE, WEAR_NONE, INVOC_INTERNAL);
                }

            }
            else
            {
                execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, mob, NULL,NULL,NULL,NULL, NULL,TRIG_NONE,0,0,0,0,0);
            }
            break;

        }

        ch->song_token = NULL;
        ch->song_script = NULL;
        ch->song = NULL;
        ch->song_instrument = NULL;
    }

    check_improve(ch, skill_resolve_gsn("music"), true, 2);
}


bool was_bard( CHAR_DATA *ch )
{
    // they are a bard now, so they have to level to get the songs
    if ( ch->pcdata->sub_class_current == CLASS_THIEF_BARD )
    return false;

    // They were a bard sometime in the past so they dont have to level.
    if ( ch->pcdata->sub_class_thief == CLASS_THIEF_BARD )
    return true;

    return false;
}

/**
 * music_lookup - Find a song by exact name match
 *
 * DEPRECATED: Use song_lookup() from song_data.h instead.
 * Kept for backward compatibility during migration.
 *
 * @param name  Exact song name to search for
 * @return      Pointer to SONG_DATA, or NULL if not found
 */
SONG_DATA *music_lookup( char *name)
{
    return song_lookup(name);
}
