/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Strfeldt, Tom Madsen, and Katja Nyboe.    *
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
*\tROM 2.4 is copyright 1993-1998 Russ Taylor\t\t\t   *
*\tROM has been brought to you by the ROM consortium\t\t   *
*\t    Russ Taylor (rtaylor@hypercube.org)\t\t\t\t   *
*\t    Gabrielle Taylor (gtaylor@hypercube.org)\t\t\t   *
*\t    Brian Moore (zump@rom.org)\t\t\t\t\t   *
*\tBy using this code, you have agreed to follow the terms of the\t   *
*\tROM license, in the file Rom24/doc/rom.license\t\t\t   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "merc.h"
#include "interp.h"
#include "tables.h"
#include "scripts.h"
#include "account/penalty.h"
#include "account/unlock.h"
#include "class_data.h"
#include "channel_registry.h"
#include "channel_service.h"

static bool dynamic_channel_is_ooc_command(const char *command)
{
    int i;
    int j;
    const CHANNEL_DEF_DATA *match = NULL;

    if (IS_NULLSTR(command))
        return false;

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        bool matches_id;
        bool matches_command;

        if (!def)
            continue;

        matches_id = !str_cmp(command, def->id);
        matches_command = !IS_NULLSTR(def->command) && !str_cmp(command, def->command);
        if (matches_id || matches_command)
            return IS_SET(def->channel_flags, CHANNEL_FLAG_IS_OOC);

        for (j = 0; j < def->alias_count; j++) {
            if (!IS_NULLSTR(def->aliases[j]) && !str_cmp(command, def->aliases[j]))
                return IS_SET(def->channel_flags, CHANNEL_FLAG_IS_OOC);
        }
    }

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        bool matches_id;
        bool matches_command;
        bool matches_alias = false;

        if (!def)
            continue;

        matches_id = !str_prefix(command, def->id);
        matches_command = !IS_NULLSTR(def->command) && !str_prefix(command, def->command);
        for (j = 0; j < def->alias_count; j++) {
            if (!IS_NULLSTR(def->aliases[j]) && !str_prefix(command, def->aliases[j])) {
                matches_alias = true;
                break;
            }
        }

        if (!matches_id && !matches_command && !matches_alias)
            continue;

        if (match && match != def)
            return false;

        match = def;
    }

    return match && IS_SET(match->channel_flags, CHANNEL_FLAG_IS_OOC);
}

// Log-all switch
bool				logAll		= false;

bool forced_command = false;	// 20070511NIB: Used to prevent forces to do any restricted command

static void __collect_verbs_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    ITERATOR tit, pit;
    ROOM_INDEX_DATA *source;
    TOKEN_DATA *token;
    PROG_LIST *prg;
    int slot = TRIGSLOT_VERB;

    if (room->source) {
        source = room->source;
    } else {
        source = room;
    }

    script_room_addref(room);

    iterator_start(&tit, room->ltokens);
    while ((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
        if (token->pIndexData && token->pIndexData->progs) {
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                    execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
                }
            }
            iterator_stop(&pit);
            script_token_remref(token);
        }
    }
    iterator_stop(&tit);

    if (source->progs && source->progs->progs) {
        script_destructed = false;
        iterator_start(&pit, source->progs->progs[slot]);
        while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
            if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
            }
        }
        iterator_stop(&pit);
    }

    script_room_remref(room);
}

static void __collect_verbs_mob(CHAR_DATA *ch, CHAR_DATA *mob)
{
    ITERATOR tit, pit;
    TOKEN_DATA *token;
    PROG_LIST *prg;
    int slot = TRIGSLOT_VERB;

    if (IS_NPC(mob))
        script_mobile_addref(mob);

    iterator_start(&tit, mob->ltokens);
    while ((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
        if (token->pIndexData && token->pIndexData->progs) {
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                    execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
                }
            }
            iterator_stop(&pit);
            script_token_remref(token);
        }
    }
    iterator_stop(&tit);

    if (ch != mob && IS_NPC(mob)) {
        if (mob->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                    execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
                }
            }
            iterator_stop(&pit);
        }
    }

    if (IS_NPC(mob))
        script_mobile_remref(mob);
}

static void __collect_verbs_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
    ITERATOR tit, pit;
    TOKEN_DATA *token;
    PROG_LIST *prg;
    int slot = TRIGSLOT_VERB;

    script_object_addref(obj);

    iterator_start(&tit, obj->ltokens);
    while ((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
        if (token->pIndexData && token->pIndexData->progs) {
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                    execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
                }
            }
            iterator_stop(&pit);
            script_token_remref(token);
        }
    }
    iterator_stop(&tit);

    if (obj->pIndexData->progs) {
        script_destructed = false;
        iterator_start(&pit, obj->pIndexData->progs[slot]);
        while ((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
            if (is_trigger_type(prg->trig_type, TRIG_SHOWCOMMANDS)) {
                execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_SHOWCOMMANDS, 0, 0, 0, 0, 0);
            }
        }
        iterator_stop(&pit);
    }

    script_object_remref(obj);
}

void collect_verbs(CHAR_DATA *ch)
{
    ITERATOR it;
    OBJ_DATA *obj;
    CHAR_DATA *mob;

    __collect_verbs_mob(ch, ch);
    __collect_verbs_room(ch, ch->in_room);

    iterator_start(&it, ch->in_room->lpeople);
    while ((mob = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (mob != ch)
            __collect_verbs_mob(ch, mob);
    }
    iterator_stop(&it);

    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        __collect_verbs_obj(ch, obj);
    }
    iterator_stop(&it);

    iterator_start(&it, ch->in_room->lcontents);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        __collect_verbs_obj(ch, obj);
    }
    iterator_stop(&it);
}

bool check_verbs(CHAR_DATA *ch, char *command, char *argument)
{
    char buf[MIL], *p;
    ITERATOR tit, pit;
    TOKEN_DATA *token;
    OBJ_DATA *obj;
    CHAR_DATA *mob;
    PROG_LIST *prg;
//	SCRIPT_DATA *script;
//	unsigned long uid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret = PRET_NOSCRIPT; // @@@NIB Default for a trigger loop is NO SCRIPT

//	log_stringf("check_verbs: ch(%s), command(%s), argument(%s)", ch->name, command, argument);
//	printf_to_char(ch, "check_verbs: ch(%s), command(%s), argument(%s)", ch->name, command, argument);

    slot = TRIGSLOT_VERB;

    // Save the UID - TODO: this looks incomplete
//	uid[0] = ch->id[0];
//	uid[1] = ch->id[1];

    // Check for tokens FIRST
    iterator_start(&tit, ch->ltokens);
    while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
        if(token->pIndexData && token->pIndexData->progs) {
//			log_stringf("check_verbs: ch(%s) token(%ld, %s)", ch->name, token->pIndexData->vnum, token->name);
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
//				log_stringf("check_verbs: ch(%s) token(%ld, %s) trigger(%s, %s)", ch->name, token->pIndexData->vnum, token->name, trigger_name(prg->trig_type), prg->trig_phrase);
                if (is_trigger_type(prg->trig_type,TRIG_VERBSELF) && !str_prefix(command, prg->trig_phrase)) {
//					log_stringf("check_verbs: ch(%s) token(%ld, %s) trigger(%s, %s) executing", ch->name, token->pIndexData->vnum, token->name, trigger_name(prg->trig_type), prg->trig_phrase);
                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,argument,prg->trig_phrase,TRIG_VERBSELF,0,0,0,0,0);
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);

                        script_token_remref(token);
                        return ret;
                    }

                }
            }
            iterator_stop(&pit);
            script_token_remref(token);
        }
    }
    iterator_stop(&tit);
    if( ret_val != PRET_NOSCRIPT ) return true;

    p = one_argument(argument,buf);
//	if(!str_cmp(buf,"here")) {
        ROOM_INDEX_DATA *room = ch->in_room;
        ROOM_INDEX_DATA *source;
        //bool isclone;

        if(room->source) {
            source = room->source;
            //isclone = true;
            //uid[0] = room->id[0];
            //uid[1] = room->id[1];
        } else {
            source = room;
            //isclone = false;
        }

        script_room_addref(room);

        // Check for tokens FIRST
        iterator_start(&tit, room->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if ((is_trigger_type(prg->trig_type,TRIG_VERB) || is_trigger_type(prg->trig_type,TRIG_VERBSELF)) && !str_prefix(command, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&tit);
                            iterator_stop(&pit);

                            script_token_remref(token);
                            script_room_remref(room);
                            return ret;
                        }

                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
            script_destructed = false;
            iterator_start(&pit, source->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                } else if (is_trigger_type(prg->trig_type,TRIG_VERBSELF) && !str_prefix(command,prg->trig_phrase)) {
                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL,argument,prg->trig_phrase,TRIG_VERBSELF,0,0,0,0,0);
                }
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);

                        script_room_remref(room);
                        return ret;
                    }


            }
            iterator_stop(&pit);

        }
        script_room_remref(room);

        if( ret_val != PRET_NOSCRIPT ) return true;
//	}

    // Get mobile...
    mob = strcmp(buf,"self") ? get_char_room(ch, NULL, buf) : ch;
    if(mob) {
        script_mobile_addref(mob);

        // Check for tokens FIRST
        iterator_start(&tit, mob->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&tit);
                            iterator_stop(&pit);

                            script_token_remref(token);
                            script_mobile_remref(mob);
                            return ret;
                        }

                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && IS_NPC(mob) && mob->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
                    ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);

                        script_mobile_remref(mob);
                        return ret;
                    }

                }
            }
            iterator_stop(&pit);
        }
        script_mobile_remref(mob);

        if( ret_val != PRET_NOSCRIPT ) return true;
    }

    // Get obj...
    if ((obj = get_obj_here(ch, NULL, buf))) {
        script_object_addref(obj);

        // Check for tokens FIRST
        iterator_start(&tit, obj->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&tit);
                            iterator_stop(&pit);

                            script_token_remref(token);
                            script_object_remref(obj);
                            return ret;
                        }

                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
                    ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);

                        script_object_remref(obj);
                        return ret;
                    }

                }
            }
            iterator_stop(&pit);
        }

        script_object_remref(obj);

        if( ret_val != PRET_NOSCRIPT ) return true;
    }

    return false;
}

// The main entry point for executing commands.
// Can be recursively called from 'at', 'order', 'force'.
void interpret( CHAR_DATA *ch, char *argument )
{
    char command[MAX_INPUT_LENGTH];
    char logline[MAX_INPUT_LENGTH];
//    int cmd;
    int trust;
    (void)trust;
    bool found, allowed;
    char cmd_copy[MAX_INPUT_LENGTH] ;
    char buf[MSL];
//    const struct cmd_type* selected_command = NULL;
    CMD_DATA *selected_command = NULL;
    CMD_DATA *cmd = NULL;

    // Strip leading spaces
    while (ISSPACE(*argument))
    argument++;

    if ( argument[0] == '\0' )
    return;

    // Frozen people can't do anything
    if (!IS_NPC(ch) && (IS_SET(ch->act[0], PLR_FREEZE)
        || (ch->desc && ch->desc->account
            && has_penalty(ch->desc->account, PENALTY_FREEZE, ch->name))))
    {
    send_to_char( "You're totally frozen!\n\r", ch );
    return;
    }

    // Neither can paralyzed people
    if (ch->paralyzed > 0 && !is_allowed( argument ))
    {
    send_to_char("You are paralyzed and can't move a muscle!\n\r", ch );
    return;
    }

    // Deal with scripted input
    if(ch->desc && ch->desc->input && ch->desc->input_script > 0 && ch->desc->inputString == NULL) {

        int ret = PRET_NOSCRIPT;
        SCRIPT_DATA *script = NULL;
        VARIABLE **var = NULL;
        CHAR_DATA *mob = ch->desc->input_mob;
        OBJ_DATA *obj = ch->desc->input_obj;
        ROOM_INDEX_DATA *room = ch->desc->input_room;
        TOKEN_DATA *tok = ch->desc->input_tok;
        char *v = ch->desc->input_var;

        if(ch->desc->input_mob) {
        script = get_script_index_global(ch->desc->input_script,PRG_MPROG);
        var = &ch->desc->input_mob->progs->vars;
    } else if(ch->desc->input_obj) {
        script = get_script_index_global(ch->desc->input_script,PRG_OPROG);
        var = &ch->desc->input_obj->progs->vars;
    } else if(ch->desc->input_room) {
        script = get_script_index_global(ch->desc->input_script,PRG_RPROG);
        var = &ch->desc->input_room->progs->vars;
    } else if(ch->desc->input_tok) {
        script = get_script_index_global(ch->desc->input_script,PRG_TPROG);
        var = &ch->desc->input_tok->progs->vars;
    }

        ch->desc->input_room = NULL;
        ch->desc->input_tok = NULL;
        if(ch->desc->input_prompt) free_string(ch->desc->input_prompt);
        ch->desc->input_prompt = NULL;

        if(script) {
//			send_to_char("Executing script...\n\r",ch);
            if(v) {
//				send_to_char("Var:",ch);
//				send_to_char(v,ch);
//				send_to_char("...\n\r",ch);
                variables_set_string(var,v,argument,false);
            }

            ret = execute_script(script->vnum, script, mob, obj, room, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL,NULL,TRIG_NONE,0,0,0,0,0);
            if(ret > 0 && !IS_NPC(ch) && ch->pcdata->quit_on_input)
                do_function(ch, &do_quit, NULL);
        }

        if(v) free_string(v);

        if(script) return;
    }




    strcpy(cmd_copy, argument);

   /*
    * Grab the command word.
    * Special parsing so ' can be a command,
    *   also no spaces needed after punctuation.
    */
    strcpy( logline, argument );
    if ( !ISALPHA(argument[0]) && !ISDIGIT(argument[0]) )
    {
    command[0] = argument[0];
    command[1] = '\0';
    argument++;
    while ( ISSPACE(*argument) )
        argument++;
    }
    else
    argument = one_argument( argument, command );

    // Questions which people must answer before they can go on with life!

    // Remove yourself from a church?
    // Disabled pneuma/dp loss for now - Tieryo
    if (ch->remove_question)
    {
    if (!str_prefix(command, "yes"))
    {
        if (!str_cmp(ch->name, ch->remove_question->name))
        {
        if (!str_cmp(ch->name, ch->remove_question->church->founder))
        {
            act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            sprintf(buf, "{Y[%s has quit %s]{x\n\r", ch->remove_question->name, ch->church->name);
            gecho( buf );
//		    ch->pneuma = 0;
//		    ch->deitypoints = 0;
            extract_church( ch->church );
            ch->remove_question = NULL;
            sprintf( buf, "%s has quit.", ch->name );
            append_church_log( ch->church, ch->name );
        }
        else
        {
            act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            sprintf(buf, "{Y[%s has quit %s]{x\n\r", ch->remove_question->name, ch->church->name);
            gecho( buf );
            sprintf( buf, "%s has quit.", ch->name );
            append_church_log( ch->church, ch->name );
//		    ch->pneuma = 0;
//		    ch->deitypoints = 0;
            remove_member(ch->remove_question);
            ch->remove_question = NULL;
        }
        }
        return;
    }
    else
        if (!str_prefix(command, "no"))
        {
        ch->remove_question = NULL;
        return;
    }
    else
    {
        send_to_char("Please answer yes or no.\n\r", ch);
        return;
    }
    }

    if (!IS_NPC(ch) && ch->pcdata->inquiry_subject != NULL) {
    if (command[0] != '\0') {
        sprintf(buf, "%s %s", command, argument);
        buf[0] = UPPER(buf[0]);
        ch->pcdata->inquiry_subject->subject = str_dup(buf);
        send_to_char("Inquiry added. Starting editor...\n\r", ch);
        string_append(ch, &ch->pcdata->inquiry_subject->text);
        ch->pcdata->inquiry_subject = NULL;
        projects_changed = true;
    }
    else
        send_to_char("{YEnter inquiry subject:{x ", ch);

    return;
    }

// Toggle church PK?
if (ch->pk_question)
{
    if (!str_prefix(command, "yes"))
    {
        // Call the central function to handle enabling PK
        chtoggle_complete(ch, true);
        ch->pk_question = false;
        return;
    }
    else if (!str_prefix(command, "no"))
    {
        ch->pk_question = false;
        return;
    }
    else
    {
        send_to_char("Please answer yes or no.\n\r", ch);
        return;
    }
}

    // Toggle personal PK?
    if (ch->personal_pk_question)
    {
    char buf[MAX_STRING_LENGTH];

    if (!str_prefix(command, "yes"))
    {
        if (!IS_SET( ch->act[0], PLR_PK ) )
        {
        SET_BIT( ch->act[0], PLR_PK );
        send_to_char("You have toggled PK. Good luck!\n\r", ch );
        sprintf( buf, "%s has toggled PK on!", ch->name );
        crier_announce( buf );
        ch->pneuma -= 5000;
        }
        else
        {
        REMOVE_BIT( ch->act[0], PLR_PK );
        send_to_char("You have toggled PK off.\n\r", ch );
        sprintf( buf, "%s is no longer PK.", ch->name );
        crier_announce( buf );
        ch->pneuma -= 5000;
        }

        ch->personal_pk_question = false;
        return;
    }
    else
        if (!str_prefix(command, "no"))
        {
        ch->personal_pk_question = false;
        return;
    }
    else
    {
        if ( IS_SET( ch->act[0], PLR_PK ) )
        {
        send_to_char("Toggle PK off? (y/n)\n\r", ch );
        }
        else
        {
        send_to_char("Toggle PK on? (y/n)\n\r", ch );
        }
        return;
    }
    }

    // Cross-zone gohall?
    if (ch->cross_zone_question)
    {
    char buf[MAX_STRING_LENGTH];

    if (!str_prefix(command, "yes"))
    {
        long pneuma_cost;
        long dp_cost;

        pneuma_cost = 500;
        dp_cost = 50000;

        if ( ch->church == NULL )
        return;

        ch->church->pneuma -= pneuma_cost;
        ch->church->dp -= dp_cost;
        sprintf( buf, "{Y[%s has recalled cross-zone, draining %ld pneuma and %ld karma!]{x\n\r", ch->name, pneuma_cost, dp_cost );
        msg_church_members( ch->church, buf );
        ch->cross_zone_question = false;

        act("{R$n disappears, leaving a resounding echo of discord.{X", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        char_from_room(ch);
        char_to_room(ch, location_to_room(&ch->church->recall_point));
        act("$n appears in the room.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        do_function(ch, &do_look, "auto");
        return;
    }
    else
        if (!str_prefix(command, "no"))
        {
        send_to_char("Cross-zone recall cancelled.\n\r", ch );
        ch->cross_zone_question = false;
        return;
    }
    else
    {
        send_to_char("Recall cross-zone? (yes/no)\n\r", ch );
        return;
    }
    }

    // Remorting!
    if (ch->remort_question) {
        if (!str_prefix(command, "yes")) {
            remort_player(ch);
        } else if (!str_prefix(command, "no")) {
            send_to_char("You decide not to remort.\n\r", ch);
            ch->remort_question = false;
        } else {
            send_to_char("Are you ready to be reborn? (yes/no)\n\r", ch);
        }
        return;
    }

    // Original race selection (migration for path race characters)
    if (ch->orace_question) {
        RACE_DATA *orace;

        if (!str_cmp(command, "list")) {
            RACE_DATA *r;
            send_to_char("{YAvailable original races:{x\n\r", ch);
            for (r = race_list; r; r = r->next) {
                if (r->playable && !race_is_remort(r))
                    printf_to_char(ch, "  %s\n\r", r->name);
            }
            return;
        }

        orace = race_lookup(command);
        if (!orace || !orace->playable || race_is_remort(orace)) {
            send_to_char("That is not a valid original race. Type 'list' to see choices.\n\r", ch);
            return;
        }

        ch->orace = orace;
        ch->orace_question = false;

        /* If current race is a remort of a path race (e.g., changeling→slayer),
         * downgrade race to the path race prerequisite. We're deprecating the
         * "remort of a path race" pattern in favor of remort OR path. */
        if (ch->race) {
            RACE_DATA *prereq = race_get_prerequisite(ch->race);
            if (prereq && race_is_path(prereq)) {
                printf_to_char(ch, "Your race has been changed from %s to %s.\n\r",
                               ch->race->name, prereq->name);
                ch->race = prereq;
            }
        }

        /* Grant account unlock for their race so they can create new
         * characters with it. */
        if (ch->desc && ch->desc->account && ch->race) {
            if (account_add_race_unlock(ch->desc->account, ch->race->id))
                save_account(ch->desc->account);
        }

        printf_to_char(ch, "Your original race has been set to %s.\n\r", orace->name);
        return;
    }

    // Convert church to a different alignment?
    if (!IS_NPC(ch) && ch->pcdata->convert_church != -1)
    {
        char buf[MAX_STRING_LENGTH];

        if (!str_prefix(command, "yes"))
        {
            long pneuma_cost;
            long dp_cost;

            pneuma_cost = 10000;
            dp_cost = 2500000;

            if ( ch->church == NULL )
            return;

            ch->church->pneuma -= pneuma_cost;
            ch->church->dp -= dp_cost;
            ch->church->alignment = ch->pcdata->convert_church;

            sprintf( buf, "{Y[%s has converted to the faith of %s!]{x\n\r",
                ch->church->name,
                ch->church->alignment == CHURCH_GOOD ? "the Pious" :
                ch->church->alignment == CHURCH_NEUTRAL ? "Neutrality" : "Malice" );
            gecho( buf );
            ch->pcdata->convert_church = -1;
            return;
        }
        else if (!str_prefix(command, "no"))
        {
            send_to_char("Church faith conversion cancelled.\n\r", ch );
            ch->pcdata->convert_church = -1;
            return;
        }
        else
        {
            sprintf( buf, "Are you SURE you want to convert to the faith of %s? (y/n)\n\r"
                    "{R***WARNING***:{x all members who cannot follow that faith will be removed on their next login!!!\n\r",
                    ch->pcdata->convert_church == CHURCH_GOOD ? "the Pious" :
                    ch->pcdata->convert_church == CHURCH_NEUTRAL ? "Neutrality" : "Malice" );
            send_to_char( buf, ch );
            return;
        }
    }

    // Answer a challenge?
    // Display character names, no more "a Slayer/a Werewolf" displayed to everyone -- Areo
    if (ch->challenged != NULL)
    {
        CHAR_DATA *victim = ch->challenged;
        char buf[MAX_STRING_LENGTH];

        if (!str_prefix(command, "yes"))
        {
            sprintf(buf, "%s has accepted %s's challenge! May the battle begin!",
                ch->name, victim->name);
            crier_announce( buf );

            sprintf(buf, "{M%s has accepted your challenge!{x\n\r{RYou are transported to the arena!\n\r{x", pers( ch, victim ) );

            send_to_char(buf, victim);

            sprintf(buf, "{MYou have accepted %s's challenge!{x\n\r{RYou are transported to the arena!\n\r{x", pers( victim, ch ) );

            send_to_char(buf, ch);

            if (ch->fighting != NULL)
                stop_fighting(ch, true);

            if (ch->cast > 0)
                stop_casting(ch, true);

            if (ch->script_wait > 0)
                script_end_failure(ch, true);

            interrupt_script(ch,false);


            if (victim->fighting != NULL)
                stop_fighting(victim, true);

            if (victim->cast > 0)
                stop_casting(victim, true);

            if(victim->script_wait > 0)
                script_end_failure(victim, true);

            interrupt_script(victim,false);

            location_from_room(&ch->pcdata->room_before_arena,ch->in_room);
            location_from_room(&victim->pcdata->room_before_arena,victim->in_room);

            char_from_room(ch);
            char_from_room(victim);
            {
                ROOM_INDEX_DATA *arena = get_reserved_room_index("room_default_arena");
                if (!arena)
                    arena = get_reserved_room_index("room_default");
                if (arena) {
                    char_to_room(ch, arena);
                    char_to_room(victim, arena);
                }
            }

            ch->challenged = NULL;
            return;
        }

        if (!str_prefix(command, "no"))
        {
            sprintf( buf, "%s has declined %s's challenge!",
                ch->name, victim->name);
            crier_announce( buf );

            sprintf(buf, "{M%s has declined your challenge!{x\n\r", ch->name);
            send_to_char(buf, victim);

            sprintf(buf, "{MYou have declined %s's challenge!{x\n\r", victim->name);
            send_to_char(buf, ch);
            ch->challenged = NULL;
            return;
        }

        sprintf(buf, "{M%s has challenged you to a fight to the death in the arena!\n\rDo you accept? (Yes/No)\n\r{x", victim->name);
        send_to_char(buf, ch);
        return;
    }

    // Find command in table.
    found = false;
    if (!IS_SWITCHED(ch))
    {
        trust = get_staff_rank( ch );
    }
    else
    {
        trust = get_staff_rank( ch->desc->original );
    }

    ITERATOR it;
    iterator_start(&it, commands_list);
    while(( cmd = (CMD_DATA *)iterator_nextdata(&it)))
    {	
        if ( command[0] == cmd->name[0] && 
        !str_prefix(command, cmd->name) && 
        (!forced_command || cmd->rank < STAFF_IMMORTAL) && 
        (cmd->rank <= get_staff_rank(ch) || is_granted_command(ch, cmd->name)))
        {
            selected_command = cmd;
            found = true;
            break;
        }
    }
    iterator_stop(&it);

    allowed = is_allowed(command);
    if (!allowed && !found && dynamic_channel_is_ooc_command(command))
        allowed = true;

    if (found && !selected_command->enabled)
    {
        /* Try the channel dispatcher first — a disabled command may have a
         * live channel equivalent that should still work (e.g. 'tell' routed
         * through the channel service after the legacy function is retired). */
        if (dispatch_dynamic_channel_command(ch, command, argument))
            return;

        sprintf(buf,"%s is currently disabled.\n\r", selected_command->name);
        send_to_char(buf,ch);
        if (!IS_NULLSTR(selected_command->reason))
        {
            sprintf(buf,"{RReason: {X%s{x\n\r",selected_command->reason);
            send_to_char(buf,ch);
        }
        return;
    }

    // Check stuff relevant to interpretation.
/*	
    if (IS_AFFECTED(ch, AFF_HIDE) && !(allowed || (selected_command != NULL && selected_command->is_ooc)))
*/
    if (IS_AFFECTED(ch, AFF_HIDE) && !(allowed || (found && IS_SET(selected_command->command_flags,CMD_IS_OOC))))
    {
        affect_strip(ch, skill_resolve_gsn("hide"));
        REMOVE_BIT(ch->affected_by[0], AFF_HIDE);
        act("You step out of the shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
        act("$n steps out of the shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }

    if (is_affected(ch, skill_lookup("paralysis")) && !allowed)
    {
        send_to_char("You can't move a muscle!\n\r", ch );
        return;
    }

    if (ch->paroxysm > 0 && !allowed)
    {
        if (number_percent() < 20)
        {
            send_to_char("{YYou flail your arms about wildly.{x\n\r", ch);
            act("$n flails $s arms about wildly, unable to control $mself.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else if (number_percent() < 20)
        {
            send_to_char("{YYou cartwheel across the floor.{x\n\r", ch);
            act("{Y$n cartwheels across the floor.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else if (number_percent() < 20)
        {
            send_to_char("{YYou babble nonsensically and foam at the mouth.{x\n\r", ch );
            act("{Y$n babbles nonsensically and foams at the mouth.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else if (number_percent() < 20)
        {
            send_to_char("{YYour fall to the floor and begin to convulse.{x\n\r", ch );
            act("{Y$n collapses to the floor and begins to have seizures.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
        }
        else if (number_percent() < 20)
        {
            send_to_char("{YYou begin to spin around in circles.{x\n\r", ch);
            act("$n spins around dizzifyingly.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else
        {
            send_to_char("{YYou stare blankly at your feet.{x\n\r", ch);
            act("$n stares blankly, unable to do anything.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }

        return;
    }

    if (ch->cast > 0 && !allowed)
        stop_casting(ch, true);

    if (ch->script_wait > 0 && !allowed)
        script_end_failure(ch, true);

    if(!allowed) interrupt_script(ch,false);

    if (ch->music > 0 && !allowed)
        stop_music(ch, true);

    if (ch->brew > 0 && !allowed)
        return;

    if (ch->repair > 0 && !allowed)
    {
        act("You stop repairing $p.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n stops repairing $p.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    ch->repair_obj = NULL;
    ch->repair_amt = 0;
    ch->repair = 0;
    }

    if (ch->hide > 0 && !allowed)
    {
        ch->hide = 0;
        send_to_char("You stop looking for a place to hide.\n\r", ch );
    }

    if (ch->bind > 0 && !allowed)
    return;

    if (ch->bomb > 0 && !allowed)
    return;

    if (ch->recite > 0 && !allowed)
    {
    ch->recite = 0;
    free_string( ch->cast_target_name );
    ch->cast_target_name = NULL;
    send_to_char("{WYou stop reciting.{x\n\r", ch );
    }

    if ((ch->reverie > 0 || ch->trance > 0) && !allowed)
    {
        send_to_char("You can't break your meditation.\n\r",ch );
    return;
    }

    if (ch->scribe > 0 && !allowed)
    return;

    // You can move while shooting, but not much else
    if (ch->ranged > 0 && !allowed)
    {
    if ( str_cmp( command, "north" )
    &&   str_cmp( command, "east" )
    &&   str_cmp( command, "south" )
    &&   str_cmp( command, "west" )
    &&   str_cmp( command, "northwest" )
    &&   str_cmp( command, "northeast" )
    &&   str_cmp( command, "southwest" )
    &&   str_cmp( command, "southeast" )
    &&   str_cmp( command, "up" )
    &&   str_cmp( command, "down" ))
        stop_ranged( ch, true );
    }

    if (ch->resurrect > 0 && !allowed)
    {
        send_to_char("You stop resurrecting.\n\r", ch );
        ch->resurrect = 0;
    }

    if (ch->fade > 0 && !allowed)
    {
        send_to_char("You fade back into the real world.\n\r", ch );
        ch->fade = 0;
        ch->fade_dir = -1;		//@@@NIB : 20071020
        ch->force_fading = 0;
    }

    // Stop abuse.
    if (selected_command != NULL && found)
    {
        if (IS_NPC(ch) && selected_command->rank >= STAFF_IMMORTAL)
        {
            sprintf(buf, "interpret: mob %s(%ld) tried immortal command %s",
            ch->short_descr, ch->pIndexData->vnum, selected_command->name);
            log_string(buf);
            return;
        }

        // Log and snoop.
        if ( selected_command->log == LOG_NEVER )
        strcpy( logline, "" );

        if (/*ch->tot_level < MAX_LEVEL    Syn - phasing this out.
        &&*/ ((!IS_NPC(ch) && (IS_SET(ch->act[0], PLR_LOG)
            || (ch->desc && ch->desc->account
                && has_penalty(ch->desc->account, PENALTY_LOG, ch->name))))
            || logAll || selected_command->log == LOG_ALWAYS))
        {
            char s[2 * MAX_INPUT_LENGTH];
            char *ps;
            int i;

            ps = s;
            sprintf( log_buf, "Log %s: %s",
            IS_NPC(ch) ? ch->short_descr : ch->name, logline );

            // Make sure that was is displayed is what is typed
            for ( i = 0; log_buf[i]; i++ )
            {
                *ps++ = log_buf[i];
                if ( log_buf[i] == '$' )
                    *ps++ = '$';
                if ( log_buf[i] == '{' )
                    *ps++ = '{';
            }

            *ps = 0;
            wiznet( s, ch, NULL, WIZ_SECURE, 0, get_staff_rank(ch));
            if ( logline[0] != '\0' )
                log_string( log_buf );
        }
    }

    if ( ch->desc != NULL && ch->desc->snoop_by != NULL )
    {
    write_to_buffer( ch->desc->snoop_by, "% ",    2 );
    write_to_buffer( ch->desc->snoop_by, logline, 0 );
    write_to_buffer( ch->desc->snoop_by, "\n\r",  2 );
    }

    // Command not found... try other places.
    // Modified 2010-08-16 - Changed order. Command -> Custom verbs -> Socials -- Tieryo
    if (!found)
    {
        if (check_verbs(ch,command,argument))
            return;

        if (dispatch_dynamic_channel_command(ch, command, argument))
            return;

        if (check_social(ch, command, argument))
            return;

        send_to_char( "Huh?\n\r", ch);
        if (IS_NPC(ch))
            sprintf(buf, "NPC \t<send href=\"stat mob %ld %ld|mshow %ld|medit %ld\">%s\t</send> (%ld) tried to use the command '%s' but it didn't exist.", ch->id[0], ch->id[1], ch->pIndexData->vnum, ch->pIndexData->vnum, ch->short_descr, ch->pIndexData->vnum, command);
        else
            sprintf(buf, "%s tried to use the command '%s' but it didn't exist.", ch->name, command);
        log_string(buf);
        wiznet(buf, ch, NULL, WIZ_VERBS, 0, 0);
        return;
    }

    // Command found, let's execute it
    if (ch->position == POS_FEIGN)
    {
    do_function( ch, &do_feign, "");
    if ( !str_cmp( selected_command->name, "feign") )
        return;
    }

    if (ch->position == POS_HELDUP)
    {
        send_to_char( "{YYou are too fearful to move a muscle!{x\n\r", ch );
        return;
    }

    if (ch->heldup != NULL)
    {
    act( "You lose your concentration and $N escapes!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act( "$n loses $s concentration, freeing you from the holdup!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    act( "$n loses $s concentration and $N frees $Mself!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
     stop_holdup(ch);
    }

    // Character not in position for command?
    if ( ch->position < selected_command->position )
    {
    switch( ch->position )
    {
    case POS_DEAD:
        send_to_char( "Lie still; you are DEAD.\n\r", ch );
        break;

    case POS_MORTAL:
    case POS_INCAP:
        send_to_char( "You are far too hurt for that.\n\r", ch );
        break;

    case POS_STUNNED:
        send_to_char( "You are too stunned to do that.\n\r", ch );
        break;

    case POS_SLEEPING:
        send_to_char( "In your dreams, or what?\n\r", ch );
        break;

    case POS_RESTING:
        send_to_char( "You are resting at the moment.\n\r", ch);
        break;

    case POS_SITTING:
        send_to_char( "Better stand up first.\n\r",ch);
        break;

    case POS_FIGHTING:
        send_to_char( "No way!  You are still fighting!\n\r", ch);
        break;
    }

    return;
    }

    // Dispatch the command
    (*selected_command->function) ( ch, argument );

    tail_chain();
}


// function to keep argument safe in all commands -- no static strings
void do_function( CHAR_DATA *ch, DO_FUN *do_fun, char *argument )
{
    char *command_string;

    // copy the string
    command_string = argument ? str_dup(argument) : NULL;

    // dispatch the command
    (*do_fun) (ch, command_string);

    // free the string
    if(command_string) free_string(command_string);
}


// Check if a command is a social and execute it if it is.
bool check_social( CHAR_DATA *ch, char *command, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int cmd;
    bool found;

    found  = false;
    for ( cmd = 0; social_table[cmd].name[0] != '\0'; cmd++ )
    {
    if ( command[0] == social_table[cmd].name[0]
    &&   !str_prefix( command, social_table[cmd].name ) )
    {
        found = true;
        break;
    }
    }

    if ( !found )
    return false;

    switch ( ch->position )
    {
    case POS_DEAD:
        send_to_char( "Lie still; you are DEAD.\n\r", ch );
        return true;

    case POS_INCAP:
    case POS_MORTAL:
        send_to_char( "You are hurt far too bad for that.\n\r", ch );
        return true;

    case POS_STUNNED:
        send_to_char( "You are too stunned to do that.\n\r", ch );
        return true;

    case POS_SLEEPING:
        /*
         * I just know this is the path to a 12" 'if' statement.  :(
         * But two players asked for it already!  -- Furey
         */
        if ( !str_cmp( social_table[cmd].name, "snore" ) )
        break;
        send_to_char( "In your dreams, or what?\n\r", ch );
        return true;
    }

    one_argument( argument, arg );
    victim = NULL;
    if ( arg[0] == '\0' ) {
        act( social_table[cmd].others_no_arg, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL    );
        act( social_table[cmd].char_no_arg,   ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL    );
    }
    else if ( ( victim = get_char_room( ch, NULL, arg ) ) == NULL )
    {
        send_to_char( "They aren't here.\n\r", ch );
    }
    else if ( victim == ch )
    {
        act( social_table[cmd].others_auto,   ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL    );
        act( social_table[cmd].char_auto,     ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL    );
    }
    else
    {
        act( social_table[cmd].others_found,  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL );
        act( social_table[cmd].char_found,    ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL    );
        act( social_table[cmd].vict_found,    ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL    );
    }

    // 20140508NIB - Adding EMOTE triggering

    if( victim != NULL )
        p_emoteat_trigger(victim, ch, social_table[cmd].name);
    else
        p_emote_trigger(ch, social_table[cmd].name);

    return true;
}


// Return true if an argument is completely numeric.
bool is_number( const char *arg )
{
    if ( *arg == '\0' )
        return false;

    if ( *arg == '+' || *arg == '-' )
        arg++;

    for ( ; *arg != '\0'; arg++ )
    {
        if ( !ISDIGIT( *arg ) )
            return false;
    }

    return true;
}

bool is_percent( char *arg )
{
    if ( *arg == '\0' )
    return false;

    for ( ; *arg != '%' && *arg != '\0'; arg++ )
    {
        if ( !ISDIGIT( *arg ) )
            return false;
    }

    if( *arg != '%' )
        return false;

    // Skip the %
    ++arg;

    return !*arg;	// Does the string end a null
}


// Given a string like 14.foo, return 14 and 'foo'
int number_argument( char *argument, char *arg )
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
    if ( *pdot == '.' )
    {
        *pdot = '\0';
        number = atoi( argument );
        *pdot = '.';
        strcpy( arg, pdot+1 );
        return number;
    }
    }

    strcpy( arg, argument );
    return 1;
}


// Given a string like 14*foo, return 14 and 'foo'
int mult_argument(char *argument, char *arg)
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
        if ( *pdot == '*' )
        {
            *pdot = '\0';
            number = atoi( argument );
            *pdot = '*';
            strcpy( arg, pdot+1 );
            return number;
        }
    }

    strcpy( arg, argument );
    return 1;
}


// Same as one_argument but doesn't lower case the argument
char *one_argument_norm( char *argument, char *arg_first )
{
    char cEnd;

    while ( ISSPACE(*argument) )
    argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
    cEnd = *argument++;

    while ( *argument != '\0' )
    {
    if ( *argument == cEnd )
    {
        argument++;
        break;
    }
    *arg_first = *argument;
    arg_first++;
    argument++;
    }
    *arg_first = '\0';

    while ( ISSPACE(*argument) )
    argument++;

    return argument;
}


// Pick off one argument from a string and return the rest. Understands quotes.
char *one_argument( char *argument, char *arg_first )
{
    char cEnd;

    while ( ISSPACE(*argument) )
    argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
    cEnd = *argument++;

    while ( *argument != '\0' )
    {
    if ( *argument == cEnd )
    {
        argument++;
        break;
    }
    *arg_first = LOWER(*argument);
    arg_first++;
    argument++;
    }
    *arg_first = '\0';

    while ( ISSPACE(*argument) )
    argument++;

    return argument;
}


/*
 *  * Pick off one argument from a string and return the rest.
 *   * Understands quotes.
 *    */
char *one_caseful_argument (char *argument, char *arg_first)
{
    char cEnd;

    while (ISSPACE (*argument))
        argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"' || *argument == '\'')
        cEnd = *argument++;

    while (*argument != '\0')
    {
        if (*argument == cEnd)
        {
            argument++;
            break;
        }
        *arg_first = *argument;
        arg_first++;
        argument++;
    }
    *arg_first = '\0';

    while (ISSPACE (*argument))
        argument++;

    return argument;
}


static void delete_extra_commands(void *ptr)
{
    free_string((char *)ptr);
}

static bool command_list_has_exact_name(const char *name)
{
    ITERATOR it;
    CMD_DATA *command;

    if (IS_NULLSTR(name))
        return false;

    iterator_start(&it, commands_list);
    while ((command = (CMD_DATA *)iterator_nextdata(&it))) {
        if (!str_cmp(command->name, name)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

static void do_commands_emit_dynamic_channels(CHAR_DATA *ch, int *col)
{
    int i;

    if (!ch || !col)
        return;

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        const char *command_name;
        HELP_DATA *help;
        char mxp_str[1024];
        char hint_buf[256];
        char help_target[64];
        char summary_buf[128];
        char display_name[32];
        bool has_summary;

        if (!def)
            continue;

        command_name = IS_NULLSTR(def->command) ? def->id : def->command;
        if (IS_NULLSTR(command_name))
            continue;

        strlcpy(display_name, command_name, sizeof(display_name));

        if (command_list_has_exact_name(command_name))
            continue;

        if (!channel_service_channel_available_for_sender(ch, def))
            continue;

        help = NULL;
        if (!IS_NULLSTR(def->help_keywords) && str_cmp(def->help_keywords, "(null)"))
            help = lookup_help_exact((char *)def->help_keywords, get_staff_rank(ch), topHelpCat);

        strlcpy(summary_buf, IS_NULLSTR(def->summary) ? "" : def->summary, sizeof(summary_buf));
        has_summary = !IS_NULLSTR(summary_buf);

        if (help)
            snprintf(help_target, sizeof(help_target), "help #%d", help->index);
        else
            help_target[0] = '\0';

        if (help && has_summary)
            snprintf(hint_buf, sizeof(hint_buf), "%s|View '%s' helpfile", summary_buf, command_name);
        else if (help)
            snprintf(hint_buf, sizeof(hint_buf), "Execute %s|View '%s' helpfile", command_name, command_name);
        else if (has_summary)
            snprintf(hint_buf, sizeof(hint_buf), "%s", summary_buf);
        else
            snprintf(hint_buf, sizeof(hint_buf), "Execute %s", command_name);

        if (help)
            snprintf(mxp_str,
                     sizeof(mxp_str),
                     "\t<send href=\"%s|%s\" hint=\"%s\">{X%s\t</send>%s",
                     command_name,
                     help_target,
                     hint_buf,
                     command_name,
                     pad_string(display_name, 13, NULL, NULL));
        else
            snprintf(mxp_str,
                     sizeof(mxp_str),
                     "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s",
                     command_name,
                     hint_buf,
                     command_name,
                     pad_string(display_name, 13, NULL, NULL));

        send_to_char(mxp_str, ch);
        if (++(*col) % 6 == 0)
            send_to_char("\n\r", ch);
    }
}

// Output a table of commands.
void do_commands( CHAR_DATA *ch, char *argument )
{
    char buf[MAX_STRING_LENGTH], mxp_str[1024];
    int col;
    long cmdtype = 0;
    CMD_DATA *command;

    if (IS_NPC(ch) || !ch->pcdata)
        return;

    col = 0;

    if (argument[0] == '\0')
    {
        for (cmdtype = 0; cmdtype < MAX_COMMAND_TYPES; cmdtype++ )
        {
            if (cmdtype == CMDTYPE_ADMIN || cmdtype == CMDTYPE_IMMORTAL || cmdtype == CMDTYPE_OLC || cmdtype == CMDTYPE_NEWBIE)
                continue;

            sprintf(buf, "\n\r{X===== {W{+%s Commands{X ====={x\n\r", command_types[cmdtype].name);
            send_to_char(buf, ch);

            ITERATOR cit;
            iterator_start(&cit, commands_list);
            col = 0;
            while((command = (CMD_DATA *)iterator_nextdata(&cit)))
            {
                if (command->type != cmdtype)
                    continue;

                if (command->rank == STAFF_PLAYER && !IS_SET(command->command_flags, CMD_HIDE_LISTS))
                {
                    if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
                        sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                    else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL ) && IS_NULLSTR(command->summary))
                        sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                    else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
                        sprintf(mxp_str, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
                    else
                        sprintf(mxp_str, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

                    sprintf(buf, "%s", mxp_str);
                    send_to_char(buf, ch);
                    if (++col % 6 == 0)
                        send_to_char("\n\r", ch);
                }
            }
            iterator_stop(&cit);

            if (cmdtype == CMDTYPE_COMM)
                do_commands_emit_dynamic_channels(ch, &col);

            send_to_char("\n\r", ch);
        }

        if (ch->pcdata->extra_commands) {
            list_destroy(ch->pcdata->extra_commands);
            ch->pcdata->extra_commands = NULL;
        }

        ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);
        collect_verbs(ch);

        sprintf(buf, "\n\r{X===== {WExtra Commands{X ====={x\n\r");
        send_to_char(buf, ch);

        if (list_size(ch->pcdata->extra_commands) > 0)
        {
            ITERATOR it;
            char *cmd_str;
            iterator_start(&it, ch->pcdata->extra_commands);
            while((cmd_str = (char *)iterator_nextdata(&it)))
            {
                sprintf(buf, "%s", cmd_str);
                send_to_char(buf, ch);
                if (++col % 6 == 0)
                    send_to_char("\n\r", ch);
            }
            iterator_stop(&it);
        }

        list_destroy(ch->pcdata->extra_commands);
        ch->pcdata->extra_commands = NULL;

        if (col % 6 != 0)
            send_to_char("\n\r", ch);
    }
    else
    {
        if (!str_cmp(argument, "extra"))
        {
            if (ch->pcdata->extra_commands) {
                list_destroy(ch->pcdata->extra_commands);
                ch->pcdata->extra_commands = NULL;
            }

            ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);
            collect_verbs(ch);

            sprintf(buf, "\n\r{X===== {WExtra Commands{X ====={x\n\r");
            send_to_char(buf, ch);

            if (list_size(ch->pcdata->extra_commands) > 0)
            {
                ITERATOR it;
                char *cmd_str;
                iterator_start(&it, ch->pcdata->extra_commands);
                while((cmd_str = (char *)iterator_nextdata(&it)))
                {
                    sprintf(buf, "%s", cmd_str);
                    send_to_char(buf, ch);
                    if (++col % 6 == 0)
                        send_to_char("\n\r", ch);
                }
                iterator_stop(&it);
            }

            list_destroy(ch->pcdata->extra_commands);
            ch->pcdata->extra_commands = NULL;
            return;
        }

        if ((cmdtype = flag_value(command_types, argument)) == NO_FLAG)
        {
            send_to_char("Invalid command type.\n\r", ch);
            return;
        }

        cmdtype = flag_value(command_types, argument);
        if (cmdtype == CMDTYPE_ADMIN || cmdtype == CMDTYPE_IMMORTAL || cmdtype == CMDTYPE_OLC || cmdtype == CMDTYPE_NONE)
        {
            send_to_char("This command list is only for player commands\n\r", ch);
            return;
        }
        sprintf(buf, "\n\r{X===== {W{+%s Commands{X ====={x\n\r", command_types[cmdtype].name);
        send_to_char(buf, ch);

        ITERATOR cit;
        iterator_start(&cit, commands_list);
        col = 0;
        while((command = (CMD_DATA *)iterator_nextdata(&cit)))
        {
            if (command->rank == STAFF_PLAYER && !IS_SET(command->command_flags, CMD_HIDE_LISTS) && IS_SET(command->addl_types, flag_value(command_addl_types, argument)))
            {
                if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
                    sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL ) && IS_NULLSTR(command->summary))
                    sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
                    sprintf(mxp_str, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
                else
                    sprintf(mxp_str, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

                sprintf(buf, "%s", mxp_str);
                send_to_char(buf, ch);
                if (++col % 6 == 0)
                    send_to_char("\n\r", ch);
            }
        }
        iterator_stop(&cit);

        if (cmdtype == CMDTYPE_COMM)
            do_commands_emit_dynamic_channels(ch, &col);

        send_to_char("\n\r", ch);
    }

    if (col % 6 != 0)
        send_to_char("\n\r", ch);
}

// Output a table of imm-only commands.
void do_wizhelp( CHAR_DATA *ch, char *argument )
{
    char buf[MAX_STRING_LENGTH];
//    int cmd;
    int col = 0;
    long cmdtype = -1;
    CMD_DATA *command;
    int rank = 0;

    if (argument[0] != '\0')
    {
        cmdtype = flag_value(command_types, argument);
        sprintf(buf, "{B*{G*{B* {XFiltering for {W{+%s{X commands {B*{G*{B*{X\n\r", command_types[cmdtype].name);
        send_to_char(buf,ch);
    }

        if (cmdtype == NO_FLAG)
        {
            send_to_char("Invalid command type.\n\r", ch);
            return;
        }

    
    for ( rank = STAFF_IMMORTAL; rank <= get_staff_rank(ch); rank++ )
    {
        col = 0;
        sprintf(buf, "\n\r{B*{G*{B* {XCommands for rank {W%s{X {B*{G*{B*{X\n\r", flag_string(staff_ranks, rank));
        send_to_char(buf,ch);

        ITERATOR it;
        iterator_start(&it, commands_list);
        while ((command = (CMD_DATA *)iterator_nextdata(&it)))
        {
            if (cmdtype != -1 && !IS_SET(command->addl_types, flag_value(command_addl_types, argument)))
                continue;

            if (command->rank == rank && !IS_SET(command->command_flags, CMD_HIDE_LISTS))
            {
                if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
                    sprintf(buf, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && IS_NULLSTR(command->summary))
                    sprintf(buf, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
                else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
                    sprintf(buf, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
                else
                    sprintf(buf, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

                send_to_char( buf, ch );
                if ( ++col % 6 == 0 )
                    send_to_char( "\n\r", ch );
                
            
            }
        }
        iterator_stop(&it);
        send_to_char( "\n\r", ch );
    }

    if ( col % 6 != 0 )
    send_to_char( "\n\r", ch );
}


// Certain informational commands are allowed during paralysis, etc. These are listed here.
bool is_allowed( char *command )
{
    if ( !str_cmp( command, "look")
    || !str_cmp( command, "affects")
    || !str_cmp( command, "whois")
    || !str_cmp( command, "equipment")
    || !str_cmp( command, "inventory")
    || !str_cmp( command, "score") 
    || !str_cmp( command, "who" ) 
    || !str_cmp( command, "area" ) 
    || !str_cmp( command, "areas") )
        return true;

    return false;
}

// interrupt a chars spell. safe version (no memory leaks)
void stop_music( CHAR_DATA *ch, bool messages )
{

    // Allow for custom messages as well as handling interrupted songs
    if(ch->song_token) {
        ch->tempstore[0] = messages?1:0;	// Tell the script whether to show messages or not
        if( p_percent_trigger(NULL,NULL,NULL,ch->song_token,ch, NULL, NULL,NULL,NULL,TRIG_SPELLINTER, NULL) )
            messages = false;
    }
    free_string( ch->music_target_name );
    ch->music_target_name = NULL;
    ch->music = 0;
    ch->song = NULL;
    ch->song_token = NULL;
    ch->song_script = NULL;
    ch->song_instrument = NULL;


    if ( messages )
        send_to_char("{YYou stop playing your song.{x\n\r", ch );
}


// interrupt a chars spell. safe version (no memory leaks)
void stop_casting( CHAR_DATA *ch, bool messages )
{

    // Allow for custom messages as well as handling interrupted spells
    if(ch->cast_token) {
        ch->tempstore[0] = messages?1:0;	// Tell the script whether to show messages or not
        if( p_percent_trigger(NULL,NULL,NULL,ch->cast_token,ch, NULL, NULL,NULL,NULL,TRIG_SPELLINTER, NULL) )
            messages = false;
    }
    free_string(ch->casting_failure_message);
    ch->casting_failure_message = NULL;
    free_string( ch->cast_target_name );
    ch->cast_target_name = NULL;
    ch->cast = 0;
    ch->cast_sn = -1;
    ch->cast_token = NULL;
    ch->cast_script = NULL;


    if ( messages )
    {
    send_to_char("{WYou stop your casting.{x\n\r", ch);
    if (number_percent() < 10)
    {
        send_to_char("{YSmall yellow sparks spiral around you then fade away.{x\n\r", ch);
        act("$n's magic fizzles and dies.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    if (number_percent() < 20)
    {
        send_to_char("{YYou hear a loud bang as your magic dissipates.{x\n\r", ch);
        act("{YYou hear a loud bang as $n stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    if (number_percent() < 30)
    {
        send_to_char("{YA puff of smoke billows out of your ears.{x\n\r", ch);
        act("{YA puff of smoke billows out of $n's ears as $e stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    if (number_percent() < 40)
    {
        send_to_char("{YYour skin turns multicoloured then turns back to normal.{x\n\r", ch);
        act("{Y$n's skin turns multicoloured momentarily as $e stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    if (number_percent() < 50)
    {
        send_to_char("{YYour magic fizzles and dies.\n\r{x", ch );
        act("{Y$n's magic fizzles and dies.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    if ( number_percent() < 60 )
    {
        send_to_char("{YEnergy sizzles as you stop your casting.\n\r{x",
            ch );
        act("{YEnergy sizzles as $n stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }
    else
    if (number_percent() < 70 )
    {
        send_to_char("{YSparks fly from your fingers as your magic dissipates.{x\n\r", ch );
        act("{YSparks fly from $n's fingers as $s magic dissipates.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }
    else
    if (number_percent() < 80 )
    {
        send_to_char("{YYou eyes flash with white light as you interrupt your spell.{x\n\r", ch );
        act("{Y$n's eyes flash with white light as $e interrupts $s spell.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }
    else
    if ( number_percent() < 90 )
    {
        send_to_char("{YYour hair stands on end for a moment as you stop your spell.{x\n\r", ch );
        act("{Y$n's hair stands on end for a moment as $e finishes $s spell.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }
    else
    {
        send_to_char("{YYour magic dissipates into the air.{x\n\r", ch );
        act("{Y$n's magic dissipates into the air.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }
    }
}


// Stop ranged attacks safely.
void stop_ranged( CHAR_DATA *ch, bool messages )
{
    if ( ch == NULL )
    {
    pbugf(LOG_ERROR, "stop_ranged: null ch");
    return;
    }

    if ( ch->projectile_weapon != NULL )
    {
    if ( messages )
    {
        act("You put down $p.", ch, NULL, NULL, ch->projectile_weapon, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
        act("$n puts down $p.", ch, NULL, NULL, ch->projectile_weapon, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    }

    ch->ranged = 0;
    ch->projectile_weapon = NULL;
    free_string( ch->projectile_victim );
    ch->projectile_victim = NULL;
    ch->projectile_dir    = -1;
    ch->projectile_range  = 0;
    ch->projectile	      = NULL;
    }
}


bool is_granted_command(CHAR_DATA *ch, char *name)
{
    COMMAND_DATA *cmd;

    if (IS_NPC(ch)) {
    pbugf(LOG_ERROR, "is_granted_command: checking an NPC");
    return false;
    }

    for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next)
    {
    if (!str_cmp(cmd->name, name))
        return true;
    }

    return false;
}

void cmd_under_construction(CHAR_DATA *ch)
{
    send_to_char("{D*{Y*{D*{Y*{D*{Y[{R UNDER CONSTRUCTION {Y]{D*{Y*{D*{Y*{D*{x\n\r\n\r", ch);
    send_to_char("Command is under construction.  Please be patient until it is ready.\n\r\n\r", ch);
    send_to_char("{D*{Y*{D*{Y*{D*{Y[{R UNDER CONSTRUCTION {Y]{D*{Y*{D*{Y*{D*{x\n\r", ch);
}
