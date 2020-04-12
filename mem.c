/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "scripts.h"

//#define DEBUG_MODULE
#include "debug.h"

/* Vizz - External Globals */
extern GLOBAL_DATA gconfig;

// Globals
long last_pc_id;
long last_mob_id;
long last_token_id;
long last_obj_id;

AFFECT_DATA *affect_free;
AFFLICTION_DATA *affliction_free;
AMBUSH_DATA *ambush_free;
AREA_DATA *area_free;
AUCTION_DATA *auction_free;
AUTO_WAR *auto_war_free;
BAN_DATA *ban_free;
BUFFER *buf_free;
CHAR_DATA *char_free;
CHAT_BAN_DATA *chat_ban_free;
CHAT_OP_DATA *chat_op_free;
CHAT_ROOM_DATA *chat_room_free;
CHURCH_DATA *church_free;
CHURCH_PLAYER_DATA *church_player_free;
CONDITIONAL_DESCR_DATA *conditional_descr_free;
DESCRIPTOR_DATA *descriptor_free;
EVENT_DATA *ev_free;
EXIT_DATA *exit_free;
EXTRA_DESCR_DATA *extra_descr_free;
GQ_MOB_DATA *gq_mob_free;
GQ_OBJ_DATA *gq_obj_free;
HELP_DATA *help_free;
HELP_DATA *help_last;
HELP_CATEGORY *help_category_free;
HELP_DATA *help_free;
IGNORE_DATA *ignore_free;
LOG_ENTRY_DATA *log_entry_free;
MAIL_DATA *mail_free;
MOB_INDEX_DATA *mob_index_free;
NOTE_DATA *note_free;
NPC_SHIP_DATA *npc_ship_free;
NPC_SHIP_INDEX_DATA *npc_ship_index_free;
OBJ_DATA *obj_free;
OBJ_INDEX_DATA *obj_index_free;
PC_DATA *pcdata_free;
PROJECT_DATA *project_free;
PROJECT_BUILDER_DATA *project_builder_free;
PROJECT_INQUIRY_DATA *project_inquiry_free;
PROG_CODE *opcode_free;
PROG_CODE *rpcode_free;
PROG_CODE *mpcode_free;
PROG_DATA *prog_data_free;
PROG_LIST *mprog_free;
PROG_LIST *oprog_free;
PROG_LIST *rprog_free;
QUEST_DATA *quest_free;
QUEST_INDEX_DATA *quest_index_free;
QUEST_INDEX_PART_DATA *quest_index_part_free;
QUEST_LIST *quest_list_free;
QUEST_PART_DATA *quest_part_free;
RESET_DATA *reset_free;
ROOM_INDEX_DATA *room_index_free;
SHIP_CREW_DATA *ship_crew_free;
SHOP_DATA *shop_free;
SPELL_DATA *spell_free;
STRING_DATA *string_data_free;
TOKEN_DATA *token_free;
TOKEN_INDEX_DATA *token_index_free;
TRADE_ITEM *trade_item_free;
WAYPOINT_DATA *waypoint_free;
INVASION_QUEST *invasion_quest_free;
STORM_DATA *storm_data_free;
COMMAND_DATA *command_free;
IMMORTAL_DATA *immortal_free;
SKILL_ENTRY *skill_entry_free;
OLC_POINT_BOOST *olc_point_boost_free;

OLC_POINT_BOOST *new_olc_point_boost()
{
	OLC_POINT_BOOST *boost;

	if( olc_point_boost_free == NULL )
		boost = alloc_perm(sizeof(OLC_POINT_BOOST));
	else {
		boost = olc_point_boost_free;
		olc_point_boost_free = olc_point_boost_free->next;
	}

	memset(boost, 0, sizeof(OLC_POINT_BOOST));

	return boost;
}

void free_olc_point_boost(OLC_POINT_BOOST *boost)
{
	boost->next = olc_point_boost_free;
	olc_point_boost_free = boost;
}



SKILL_ENTRY *new_skill_entry()
{
	SKILL_ENTRY *entry;

	if( skill_entry_free == NULL )
		entry = alloc_perm(sizeof(SKILL_ENTRY));
	else {
		entry = skill_entry_free;
		skill_entry_free = skill_entry_free->next;
	}

	entry->source = SKILLSRC_NORMAL;
	entry->isspell = FALSE;
	entry->song = -1;
	entry->practice = TRUE;
	entry->improve = TRUE;

	return entry;
}

void free_skill_entry(SKILL_ENTRY *entry)
{
	entry->next = skill_entry_free;
	skill_entry_free = entry;
}

NOTE_DATA *new_note()
{
    NOTE_DATA *note;

    if (note_free == NULL)
	note = alloc_perm(sizeof(*note));
    else
    {
	note = note_free;
	note_free = note_free->next;
    }
    VALIDATE(note);
    return note;
}


void free_note(NOTE_DATA *note)
{
    if (!IS_VALID(note))
	return;

    free_string( note->text    );
    free_string( note->subject );
    free_string( note->to_list );
    free_string( note->date    );
    free_string( note->sender  );
    INVALIDATE(note);

    note->next = note_free;
    note_free   = note;
}


BAN_DATA *new_ban(void)
{
    static BAN_DATA ban_zero;
    BAN_DATA *ban;

    if (ban_free == NULL)
	ban = alloc_perm(sizeof(*ban));
    else
    {
	ban = ban_free;
	ban_free = ban_free->next;
    }

    *ban = ban_zero;
    VALIDATE(ban);
    ban->name = &str_empty[0];
    return ban;
}


void free_ban(BAN_DATA *ban)
{
    if (!IS_VALID(ban))
	return;

    free_string(ban->name);
    INVALIDATE(ban);

    ban->next = ban_free;
    ban_free = ban;
}


DESCRIPTOR_DATA *new_descriptor(void)
{
    static DESCRIPTOR_DATA d_zero;
    DESCRIPTOR_DATA *d;

    if (descriptor_free == NULL)
	d = alloc_perm(sizeof(*d));
    else
    {
	d = descriptor_free;
	descriptor_free = descriptor_free->next;
    }

    *d = d_zero;
    VALIDATE(d);

    top_descriptor++;

    return d;
}


void free_descriptor(DESCRIPTOR_DATA *d)
{
    if (!IS_VALID(d))
	return;

    free_string( d->host );
    free_mem( d->outbuf, d->outsize );
    if(d->input_var) free_string(d->input_var);
    if(d->input_prompt) free_string(d->input_prompt);
    if(d->inputString) free_string(d->inputString);
    INVALIDATE(d);
    d->next = descriptor_free;
    descriptor_free = d;
}



EXTRA_DESCR_DATA *new_extra_descr(void)
{
    EXTRA_DESCR_DATA *ed;

    if (extra_descr_free == NULL)
	ed = alloc_perm(sizeof(*ed));
    else
    {
	ed = extra_descr_free;
	extra_descr_free = extra_descr_free->next;
    }

    ed->keyword = &str_empty[0];
    ed->description = &str_empty[0];
    VALIDATE(ed);

    top_extra_descr++;
    return ed;
}


void free_extra_descr(EXTRA_DESCR_DATA *ed)
{
    if (!IS_VALID(ed))
	return;

    free_string(ed->keyword);
    free_string(ed->description);
    INVALIDATE(ed);

    ed->next = extra_descr_free;
    extra_descr_free = ed;

    top_extra_descr--;
}


CONDITIONAL_DESCR_DATA *new_conditional_descr(void)
{
    CONDITIONAL_DESCR_DATA *cd;

    if (conditional_descr_free == NULL)
	cd = alloc_perm(sizeof(*cd));
    else
    {
	cd = conditional_descr_free;
	conditional_descr_free = conditional_descr_free->next;
    }

    cd->condition = -1;
    cd->phrase = -1;
    cd->description = &str_empty[0];

    return cd;
}


void free_conditional_descr( CONDITIONAL_DESCR_DATA *cd )
{
    free_string(cd->description);

    cd->next = conditional_descr_free;
    conditional_descr_free = cd;
}


AFFECT_DATA *new_affect(void)
{
    static AFFECT_DATA af_zero;
    AFFECT_DATA *af;

    if (affect_free == NULL)
	af = alloc_perm(sizeof(*af));
    else
    {
	af = affect_free;
	affect_free = affect_free->next;
    }

    *af = af_zero;
    af->custom_name = NULL;
    af->slot = WEAR_NONE;

    top_affect++;

    VALIDATE(af);
    return af;
}


void free_affect(AFFECT_DATA *af)
{
    if (!IS_VALID(af))
	return;

    INVALIDATE(af);
    af->next = affect_free;
    affect_free = af;

    af->custom_name = NULL;

    variable_clearfield(VAR_AFFECT, af);
    script_clear_affect(af);
    script_clear_list(af);

    top_affect--;
}


OBJ_DATA *new_obj(void)
{
    static OBJ_DATA obj_zero;
    OBJ_DATA *obj;

	if (obj_free == NULL)
		obj = alloc_perm(sizeof(*obj));
	else
	{
		obj = obj_free;
		obj_free = obj_free->next;
	}

    *obj = obj_zero;

    SET_MEMTYPE(obj,MEMTYPE_OBJ);
    obj->in_mail = NULL;
	obj->ltokens = list_create(FALSE);
	obj->lcontains = list_create(FALSE);
	obj->lclonerooms = list_create(FALSE);


    VALIDATE(obj);
    return obj;
}


void free_obj(OBJ_DATA *obj)
{
    AFFECT_DATA *paf, *paf_next;
    EXTRA_DESCR_DATA *ed, *ed_next;
    EVENT_DATA *ev, *ev_next;

    if (!IS_VALID(obj))
	return;

    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	free_affect(paf);
    }
    obj->affected = NULL;

    for (paf = obj->catalyst; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	free_affect(paf);
    }
    obj->affected = NULL;

    for (ed = obj->extra_descr; ed != NULL; ed = ed_next )
    {
	ed_next = ed->next;
	free_extra_descr(ed);
     }
     obj->extra_descr = NULL;

    free_string( obj->name        );
    free_string( obj->description );
    free_string( obj->short_descr );
    free_string( obj->owner     );
    free_string( obj->material );

    if ( obj->old_name != NULL )
	free_string( obj->old_name );

    if ( obj->old_short_descr != NULL )
	free_string( obj->old_short_descr );

    if ( obj->old_description != NULL )
	free_string( obj->old_description );

    if ( obj->old_full_description != NULL )
	free_string( obj->old_full_description );

    if ( obj->loaded_by != NULL )
	free_string( obj->loaded_by );

    variable_clearfield(VAR_OBJECT, obj);
    script_clear_object(obj);
    script_clear_list(obj);
    wipe_clearinfo_object(obj);
    free_prog_data(obj->progs);

    /* be sure to free any events hooked up to this char so that they arn't called
       on the freed memory space */
    for (ev = obj->events; ev != NULL; ev = ev_next) {
	ev_next = ev->next_event;

	if(ev->delay > 0) extract_event(ev);
    }

    obj->events = NULL;
    obj->id[0] = obj->id[1] = 0;

	if( obj->persist ) persist_removeobject(obj);

	list_destroy(obj->ltokens);
	list_destroy(obj->lcontains);
	list_destroy(obj->lclonerooms);

	if(obj->owner_name != NULL)		free_string(obj->owner_name);
	if(obj->owner_short != NULL)	free_string(obj->owner_short);

    INVALIDATE(obj);

    obj->next   = obj_free;
    obj_free    = obj;
}


CHAR_DATA *new_char( void )
{
    CHAR_DATA *ch;
    static CHAR_DATA char_zero;
    int i;

    if (char_free == NULL)
	ch = alloc_perm(sizeof(*ch));
    else
    {
	ch = char_free;
	char_free = char_free->next;
    }

    *ch = char_zero;

    SET_MEMTYPE(ch,MEMTYPE_MOB);
    VALIDATE(ch);

    ch->morphed = FALSE;
    ch->name                    = &str_empty[0];
    ch->short_descr             = &str_empty[0];
    ch->long_descr              = &str_empty[0];
    ch->description             = &str_empty[0];
    ch->prompt                  = &str_empty[0];
    ch->owner			= &str_empty[0];
    ch->logon                   = current_time;
    ch->lines                   = PAGELEN;
    for (i = 0; i < 4; i++)
        ch->armour[i]            = 100;
    ch->position                = POS_STANDING;
    ch->hit                     = 20;
    ch->max_hit                 = 20;
    ch->mana                    = 100;
    ch->max_mana                = 100;
    ch->move                    = 100;
    ch->max_move                = 100;
    ch->manastore		= 0;
    ch->affected_by 		= 0;
    ch->affected_by2		= 0;
    ch->affected_by_perm		= 0;
    ch->affected_by2_perm		= 0;
    ch->imm_flags     =   0;
    ch->res_flags     =   0;
    ch->vuln_flags    =   0;
    ch->imm_flags_perm     =   0;
    ch->res_flags_perm     =   0;
    ch->vuln_flags_perm    =   0;

    ch->cast_target_name 	= NULL;
    ch->casting_failure_message = NULL;
    ch->projectile_victim	= NULL;
    ch->cast_sn = 0;
    ch->mail = NULL;
    ch->deaths = 0;
    ch->player_kills = 0;
    ch->cpk_kills = 0;
    ch->wars_won = 0;
    ch->monster_kills = 0;
    ch->arena_kills = 0;
    ch->player_deaths = 0;
    ch->cpk_deaths = 0;
    ch->arena_deaths = 0;
    ch->remove_question = NULL;
    ch->cross_zone_question = FALSE;
    ch->heldup = NULL;
    ch->ambush = NULL;
    ch->pursuit_by = NULL;
    ch->has_head = TRUE;
    ch->resurrect = 0;
    ch->cast = 0;
    ch->daze = 0;
    ch->wait = 0;
    ch->fade = 0;
    ch->music = 0;
    ch->brew = 0;
    ch->recite = 0;
    ch->scribe = 0;
    ch->ranged = 0;
    ch->repair = 0;
    ch->paralyzed = 0;
    ch->ship_move = 0;
    ch->ship_attack = 0;
    ch->paroxysm = 0;
    ch->reverie = 0;
    location_clear(&ch->before_social);
    location_clear(&ch->recall);

    ch->boarded_ship = NULL;
    ch->ship_arrival_time = 0;
    ch->ship_depart_time = 0;
    ch->in_war = FALSE;
    ch->ship_crash_time = -1;
    ch->fade_dir = -1;		//@@@NIB : 20071020
    ch->projectile_dir = -1;	//@@@NIB : 20071021

    ch->challenger = NULL;

    for (i = 0; i < MAX_STATS; i ++)
    {
        ch->perm_stat[i] = 13;
        ch->mod_stat[i] = 0;
        ch->dirty_stat[i] = TRUE;
    }

    ch->quest			= NULL;

    ch->hit_damage		= 0;
    ch->hit_type		= TYPE_UNDEFINED;

    ch->sorted_skills		= NULL;
    ch->sorted_songs		= NULL;

    ch->llocker			= list_create(FALSE);
    ch->lcarrying		= list_create(FALSE);
    ch->lworn			= list_create(FALSE);
    ch->ltokens			= list_create(FALSE);
    ch->lclonerooms		= list_create(FALSE);
    ch->lgroup			= list_create(FALSE);

    ch->deathsight_vision = 0;
    ch->in_damage_function = FALSE;

    ch->checkpoint = NULL;
    ch->tempstring = NULL;

    return ch;
}


void free_char( CHAR_DATA *ch )
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;
    TOKEN_DATA *token,*tnext;
    EVENT_DATA *ev, *ev_next;
    SKILL_ENTRY *se, *se_next;

    if (!IS_VALID(ch))
	return;

    if (IS_NPC(ch))
	mobile_count--;

    // Free data structures unique to the character

 	for(se = ch->sorted_skills; se; se = se_next) {
		se_next = se->next;
		free_skill_entry(se);
	}

 	for(se = ch->sorted_songs; se; se = se_next) {
		se_next = se->next;
		free_skill_entry(se);
	}


    // Inventory
    for (obj = ch->carrying; obj != NULL; obj = obj_next)
    {
	obj_next = obj->next_content;
	extract_obj( obj );
    }

    // Locker
    for (obj = ch->locker; obj != NULL; obj = obj_next)
    {
	obj_next = obj->next_content;
	extract_obj( obj );
    }

    // affects
    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
	paf_next = paf->next;
	affect_remove(ch,paf);
    }

    for (token = ch->tokens; token; token = tnext) {
	tnext = token->next;
	free_token(token);
    }

    if (ch->church != NULL)
    {
		list_remlink(ch->church->online_players, ch);
	if (ch->church_member != NULL)
		ch->church_member->ch = NULL;
	ch->church = NULL;
    }

    free_string(ch->name);
    free_string(ch->short_descr);
    free_string(ch->long_descr);
    free_string(ch->description);
    free_string(ch->owner);
    free_string(ch->prompt);
    free_note  (ch->pnote);
    free_pcdata(ch->pcdata);
    free_ambush(ch->ambush);
    free_string(ch->cast_target_name);
    free_string(ch->casting_failure_message);
    free_string(ch->projectile_victim);
    free_string(ch->tempstring);


    list_destroy(ch->llocker);
    list_destroy(ch->lcarrying);
    list_destroy(ch->lworn);
    list_destroy(ch->ltokens);
    list_destroy(ch->lclonerooms);
    list_destroy(ch->lgroup);

	if( !IS_NPC(ch))
	{
#ifdef IMC
		imc_freechardata( ch );
#endif
	}

    variable_clearfield(VAR_MOBILE, ch);
    script_clear_mobile(ch);
    script_clear_list(ch);
    wipe_clearinfo_mobile(ch);
    free_prog_data(ch->progs);


    /* be sure to free any events hooked up to this char so that they arn't called
       on the freed memory space */
    for (ev = ch->events; ev != NULL; ev = ev_next) {
	ev_next = ev->next_event;

	extract_event(ev);
    }

    ch->events = NULL;
    ch->id[0] = ch->id[1] = 0;

    ch->checkpoint = NULL;

	if( ch->persist ) persist_removemobile(ch);

    ch->next = char_free;
    char_free = ch;
}


PC_DATA *new_pcdata(void)
{
    int alias;

    static PC_DATA pcdata_zero;
    PC_DATA *pcdata;

    if (pcdata_free == NULL)
	pcdata = alloc_perm(sizeof(*pcdata));
    else
    {
	pcdata = pcdata_free;
	pcdata_free = pcdata_free->next;
    }

    *pcdata = pcdata_zero;

    for (alias = 0; alias < MAX_ALIAS; alias++)
    {
	pcdata->alias[alias] = NULL;
	pcdata->alias_sub[alias] = NULL;
    }

	pcdata->quit_on_input = FALSE;
    pcdata->email = NULL;
    pcdata->afk_message = NULL;
    //pcdata->imm_title = NULL;
    pcdata->ignoring = NULL;
    pcdata->vis_to_people = NULL;
    pcdata->quiet_people = NULL;
    pcdata->commands = NULL;
    pcdata->quests_completed = 0;
    location_clear(&pcdata->room_before_arena);
    //pcdata->quests = NULL;
    pcdata->buffer = new_buf();
    pcdata->convert_church = -1;
    pcdata->need_change_pw = TRUE;

    pcdata->class_mage = -1;
    pcdata->class_cleric = -1;
    pcdata->class_thief = -1;
    pcdata->class_warrior = -1;

    pcdata->second_class_mage = -1;
    pcdata->second_class_cleric = -1;
    pcdata->second_class_thief = -1;
    pcdata->second_class_warrior = -1;

    pcdata->sub_class_mage = -1;
    pcdata->sub_class_cleric = -1;
    pcdata->sub_class_thief = -1;
    pcdata->sub_class_warrior = -1;

    pcdata->second_sub_class_mage = -1;
    pcdata->second_sub_class_cleric = -1;
    pcdata->second_sub_class_thief = -1;
    pcdata->second_sub_class_warrior = -1;

    VALIDATE(pcdata);
    return pcdata;
}


void free_pcdata(PC_DATA *pcdata)
{
    int alias;
    IGNORE_DATA *ignore;
    IGNORE_DATA *ignore_next;
    STRING_DATA *string;
    STRING_DATA *string_next;

    if (!IS_VALID(pcdata))
	return;

    free_string(pcdata->email);
    free_string(pcdata->pwd);

    free_string(pcdata->afk_message);
    //free_string(pcdata->imm_title);
    //free_string(pcdata->bamfin);
    //free_string(pcdata->bamfout);
    free_string(pcdata->title);
    free_buf(pcdata->buffer);

    for (alias = 0; alias < MAX_ALIAS; alias++)
    {
	free_string(pcdata->alias[alias]);
	free_string(pcdata->alias_sub[alias]);
    }

    for ( ignore = pcdata->ignoring; ignore != NULL; ignore = ignore_next )
    {
	ignore_next = ignore->next;
	free_ignore( ignore );
    }

    for ( string = pcdata->vis_to_people; string != NULL; string = string_next )
    {
	string_next = string->next;
	free_string_data( string );
    }

    for ( string = pcdata->quiet_people; string != NULL; string = string_next )
    {
	string_next = string->next;
	free_string_data( string );
    }

    string_vector_freeall(pcdata->script_prompts);
    pcdata->script_prompts = NULL;

    INVALIDATE(pcdata);
    pcdata->next = pcdata_free;
    pcdata_free = pcdata;

    return;
}


long get_pc_id(void)
{
    long val;

    val = (current_time <= last_pc_id) ? last_pc_id + 1 : current_time;
    last_pc_id = val;
    return val;
}


void get_mob_id(CHAR_DATA *ch)
{
	if(!ch->id[0] && !ch->id[1]) {
		ch->id[0] = gconfig.next_mob_uid[0];
		ch->id[1] = gconfig.next_mob_uid[1];
		if(!++gconfig.next_mob_uid[0])
			++gconfig.next_mob_uid[1];

		if(gconfig.next_mob_uid[0] == gconfig.next_mob_uid[2] &&
			gconfig.next_mob_uid[1] == gconfig.next_mob_uid[3]) {
			gconfig.next_mob_uid[2] += UID_INC;
			if(!gconfig.next_mob_uid[2])
				++gconfig.next_mob_uid[3];
			gconfig_write();
		}
	}
}

void get_token_id(TOKEN_DATA *token)
{
        if(!token->id[0] && !token->id[1]) {
                token->id[0] = gconfig.next_token_uid[0];
                token->id[1] = gconfig.next_token_uid[1];
                if(!++gconfig.next_token_uid[0])
                        ++gconfig.next_token_uid[1];

                if(gconfig.next_token_uid[0] == gconfig.next_token_uid[2] &&
                        gconfig.next_token_uid[1] == gconfig.next_token_uid[3]) {
                        gconfig.next_token_uid[2] += UID_INC;
                        if(!gconfig.next_token_uid[2])
                                ++gconfig.next_token_uid[3];
                        gconfig_write();
                }
        }
}

void get_vroom_id(ROOM_INDEX_DATA *vroom)
{
        if(vroom && vroom->source && !vroom->id[0] && !vroom->id[1]) {
                vroom->id[0] = gconfig.next_vroom_uid[0];
                vroom->id[1] = gconfig.next_vroom_uid[1];
                if(!++gconfig.next_vroom_uid[0])
                        ++gconfig.next_vroom_uid[1];

                if(gconfig.next_vroom_uid[0] == gconfig.next_vroom_uid[2] &&
                        gconfig.next_vroom_uid[1] == gconfig.next_vroom_uid[3]) {
                        gconfig.next_vroom_uid[2] += UID_INC;
                        if(!gconfig.next_vroom_uid[2])
                                ++gconfig.next_vroom_uid[3];
                        gconfig_write();
                }
        }
}

void get_obj_id(OBJ_DATA *obj)
{
	if(!obj->id[0] && !obj->id[1]) {
		obj->id[0] = gconfig.next_obj_uid[0];
		obj->id[1] = gconfig.next_obj_uid[1];
		if(!++gconfig.next_obj_uid[0])
			++gconfig.next_obj_uid[1];

		if(gconfig.next_obj_uid[0] == gconfig.next_obj_uid[2] &&
			gconfig.next_obj_uid[1] == gconfig.next_obj_uid[3]) {
			gconfig.next_obj_uid[2] += UID_INC;
			if(!gconfig.next_obj_uid[2])
				++gconfig.next_obj_uid[3];
			gconfig_write();
		}
	}
}


void get_church_id(CHURCH_DATA *church)
{
	if(!church->uid) {
		church->uid = gconfig.next_church_uid++;
		gconfig_write();
	}
}

/* buffer sizes */
const int buf_size[MAX_BUF_LIST] =
{
    16,32,64,128,256,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576
};


/* local procedure for finding the next acceptable size */
/* -1 indicates out-of-boundary error */
int get_size( int val , int target)
{
    int i;

    for (i = 0; i < MAX_BUF_LIST; i++)
	if (buf_size[i] >= val)
	{
	    return buf_size[i];
	}

    return -1;
}


BUFFER *new_buf()
{
    BUFFER *buffer;

    if (buf_free == NULL)
	buffer = alloc_perm(sizeof(*buffer));
    else
    {
	buffer = buf_free;
	buf_free = buf_free->next;
    }

    buffer->next	= NULL;
    buffer->state	= BUFFER_SAFE;
    buffer->size	= get_size(BASE_BUF,0);

    buffer->string	= malloc(buffer->size);
    buffer->string[0]	= '\0';
    VALIDATE(buffer);

    return buffer;
}


BUFFER *new_buf_size(int size)
{
    BUFFER *buffer;

    if (buf_free == NULL)
        buffer = alloc_perm(sizeof(*buffer));
    else
    {
        buffer = buf_free;
        buf_free = buf_free->next;
    }

    buffer->next        = NULL;
    buffer->state       = BUFFER_SAFE;
    buffer->size        = get_size(size,0);
    if (buffer->size == -1)
    {
        bug("new_buf: buffer size %d too large.",size);
        exit(1);
    }
    buffer->string      = malloc(buffer->size);
    buffer->string[0]   = '\0';
    VALIDATE(buffer);

    return buffer;
}


void free_buf(BUFFER *buffer)
{
    if (!IS_VALID(buffer))
	return;

    free(buffer->string);
    buffer->string = NULL;
    buffer->size   = 0;
    buffer->state  = BUFFER_FREED;
    INVALIDATE(buffer);

    buffer->next  = buf_free;
    buf_free      = buffer;
}


bool add_buf(BUFFER *buffer, char *string)
{
    int len;
    char *oldstr;
    int oldsize;

    oldstr = buffer->string;
    oldsize = buffer->size;

    if (buffer->state == BUFFER_OVERFLOW) /* don't waste time on bad strings! */
	return FALSE;

    len = strlen(buffer->string) + strlen(string) + 1;

    while (len >= buffer->size) /* increase the buffer size */
    {
	buffer->size 	= get_size(buffer->size + 1, len);
	{
	    if (buffer->size == -1) /* overflow */
	    {
		buffer->size = oldsize;
		buffer->state = BUFFER_OVERFLOW;
		bug("buffer overflow past size %d",buffer->size);
		return FALSE;
	    }
  	}
    }

    if (buffer->size != oldsize)
    {
	buffer->string	= malloc(buffer->size);

	strcpy(buffer->string,oldstr);
	free(oldstr);
    }

    strcat(buffer->string,string);
    return TRUE;
}


void clear_buf(BUFFER *buffer)
{
    buffer->string[0] = '\0';
    buffer->state     = BUFFER_SAFE;
}


char *buf_string(BUFFER *buffer)
{
    return buffer->string;
}

PROG_DATA *new_prog_data(void)
{
    PROG_DATA *pr_dat;

    if (prog_data_free == NULL)
	pr_dat = alloc_perm(sizeof(*pr_dat));
    else
    {
	pr_dat = prog_data_free;
	prog_data_free = prog_data_free->next;
    }

    memset(pr_dat,0,sizeof(*pr_dat));
    pr_dat->progs = NULL;
    pr_dat->target = NULL;
    pr_dat->delay = 0;
    pr_dat->lastreturn = 0;
    pr_dat->entity_flags = 0;

    return pr_dat;
}

LLIST **new_prog_bank(void)
{
	int i;
	LLIST **data = alloc_mem(TRIGSLOT_MAX *sizeof(LLIST *));
	for(i = 0; i < TRIGSLOT_MAX; i++)
		data[i] = list_create(FALSE);

	return data;
}

void free_prog_data(PROG_DATA *pr_dat)
{
	if(!pr_dat) return;

	variable_freelist(&pr_dat->vars);

	pr_dat->next = prog_data_free;
	prog_data_free = pr_dat;
}

PROG_LIST *trigger_free = NULL;

void free_trigger(PROG_LIST *trigger)
{
	if (!IS_VALID(trigger)) return;
	free_string(trigger->trig_phrase);
	INVALIDATE(trigger);
	trigger->next = trigger_free;
	trigger_free = trigger;
}

void free_prog_list(LLIST **pr_list)
{
	PROG_LIST *trigger;
	ITERATOR it;
	int i;

	if(pr_list) {
		for(i=0;i<TRIGSLOT_MAX;i++) if( pr_list[i] ) {
			iterator_start(&it, pr_list[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				free_trigger(trigger);
			iterator_stop(&it);
			list_destroy(pr_list[i]);
		}
		free_mem(pr_list,TRIGSLOT_MAX *sizeof(LLIST *));
	}
}

PROG_LIST *new_trigger(void)
{
	static PROG_LIST trig_zero;
	PROG_LIST *trigger;

	if (!trigger_free)
		trigger = alloc_perm(sizeof(PROG_LIST));
	else {
		trigger = trigger_free;
		trigger_free=trigger_free->next;
	}

	*trigger = trig_zero;
	trigger->vnum = 0;
	trigger->trig_type = -1;
	trigger->script = NULL;
	VALIDATE(trigger);
	return trigger;
}


RESET_DATA *new_reset_data( void )
{
    RESET_DATA *pReset;

    if ( !reset_free )
    {
        pReset          =   alloc_perm( sizeof(*pReset) );
        top_reset++;
    }
    else
    {
        pReset          =   reset_free;
        reset_free      =   reset_free->next;
    }

    pReset->next        =   NULL;
    pReset->command     =   'X';
    pReset->arg1        =   0;
    pReset->arg2        =   0;
    pReset->arg3        =   0;
    pReset->arg4	=   0;

    return pReset;
}


void free_reset_data( RESET_DATA *pReset )
{
    pReset->next            = reset_free;
    reset_free              = pReset;
    return;
}

static void delete_church_treasure_room(void *data) { free_mem(data, sizeof(CHURCH_TREASURE_ROOM)); }

CHURCH_DATA *new_church( void )
{
    CHURCH_DATA *pChurch;

    if ( !church_free )
    {
	pChurch = alloc_perm( sizeof(*pChurch) );
    }
    else
    {
	pChurch = church_free;
	church_free = church_free->next;
    }

	pChurch->uid = 0;
    pChurch->next = NULL;
    pChurch->name = NULL;
    pChurch->flag = NULL;
    pChurch->founder = NULL;
    pChurch->pneuma = 0;
    pChurch->dp	    = 0;
    pChurch->gold   = 0;
    pChurch->max_positions = 0;
    pChurch->size = 0;
    pChurch->alignment = 0;
    location_clear(&pChurch->recall_point);
    pChurch->rules = NULL;
    pChurch->motd = NULL;
    pChurch->log = NULL;
    pChurch->founder_last_login = 0;
    pChurch->pk = 0;
    pChurch->settings = 0;
    pChurch->treasure_rooms = list_createx(FALSE, NULL, delete_church_treasure_room);
    pChurch->key = 0;

    pChurch->pk_wins = 0;
    pChurch->pk_losses = 0;
    pChurch->cpk_wins = 0;
    pChurch->cpk_losses = 0;
    pChurch->wars_won = 0;
    pChurch->created = 0;
    pChurch->colour1 = 'C';
    pChurch->colour2 = 'B';
    pChurch->online_players = list_create(FALSE);
    pChurch->roster = list_create(FALSE);

    top_church++;

    return pChurch;
}


void free_church( CHURCH_DATA *pChurch )
{
    CHURCH_PLAYER_DATA *people;
    CHURCH_PLAYER_DATA *next_person;

    free_string( pChurch->name );
    free_string( pChurch->flag );
    free_string( pChurch->rules );
    free_string( pChurch->motd );
    free_string( pChurch->log );
    free_string( pChurch->founder );

    people = pChurch->people;

    while( people != NULL) {
	next_person = people->next;
        free_church_player( people );
	people = next_person;
    }

	variable_clearfield(VAR_CHURCH, pChurch);

    list_destroy(pChurch->online_players);
    list_destroy(pChurch->roster);

    pChurch->next         =   church_free;
    church_free             =   pChurch;
    return;
}


CHURCH_PLAYER_DATA *new_church_player( void )
{
    CHURCH_PLAYER_DATA *pMember;

    if ( !church_player_free )
    {
	pMember = alloc_perm( sizeof(*pMember) );
    }
    else
    {
	pMember = church_player_free;
	church_player_free = church_player_free->next;
    }

    pMember->next = NULL;
    pMember->name = NULL;
    pMember->rank = 0;
    pMember->sex = 0;
    pMember->dep_pneuma = 0;
    pMember->dep_dp = 0;
    pMember->dep_gold = 0;
    pMember->ch = NULL;
    pMember->church = NULL;
    pMember->commands = NULL;

    pMember->pk_wins = 0;
    pMember->pk_losses = 0;
    pMember->cpk_wins = 0;
    pMember->cpk_losses = 0;
    pMember->wars_won = 0;
    pMember->alignment = 0;

    top_church_player++;

    return pMember;
}


void free_church_player( CHURCH_PLAYER_DATA *pMember )
{
    STRING_DATA *string;
    STRING_DATA *string_next;

    free_string( pMember->name );
    for ( string = pMember->commands; string != NULL; string = string_next )
    {
	string_next = string->next;
	free_string_data( string );
    }

    pMember->name = NULL;
    pMember->rank = 0;
    pMember->sex = 0;
    pMember->ch = NULL;
    pMember->church = NULL;

    pMember->next         =   church_player_free;
    church_player_free    =   pMember;
    return;
}


AREA_DATA *new_area( void )
{
    AREA_DATA *pArea;
    char buf[MAX_INPUT_LENGTH];

    if ( !area_free )
    {
        pArea   =   alloc_perm( sizeof(*pArea) );
        top_area++;
    }
    else
    {
        pArea       =   area_free;
        area_free   =   area_free->next;
    }

    SET_MEMTYPE(pArea,MEMTYPE_AREA);
    pArea->next             =   NULL;
    pArea->name             =   str_dup( "New area" );
    pArea->area_flags       =   AREA_ADDED;
    pArea->security         =   1;
    pArea->builders         =   str_dup( "None" );
    pArea->min_vnum         =   0;
    pArea->max_vnum         =   0;
    pArea->age              =   0;
    pArea->repop	    =   0;
    pArea->nplayer          =   0;
    //pArea->flags	    =   0;
    pArea->empty            =   TRUE;              /* ROM patch */
    sprintf( buf, "area%ld.are", pArea->anum );
    pArea->file_name        =   str_dup( buf );
    pArea->anum             =   top_area-1;
    pArea->uid              =   0;  /* Vizz - uid 0 is invalid */
    pArea->open		    =   FALSE;
    pArea->x		    =   -1;
    pArea->y		    =   -1;
    pArea->land_x	    =   -1;
    pArea->land_y	    =   -1;
    pArea->room_list = list_create(FALSE);

    pArea->points		= NULL;

    return pArea;
}


void free_area( AREA_DATA *pArea )
{
	OLC_POINT_BOOST *boost, *boost_next;

    free_string( pArea->name );
    free_string( pArea->file_name );
    free_string( pArea->builders );
    free_string( pArea->credits );
    free_string( pArea->map);
	list_destroy(pArea->room_list);

	for(boost = pArea->points; boost; boost = boost_next) {
		boost_next = boost->next;

		free_olc_point_boost(boost);
	}

    pArea->next         =   area_free->next;
    area_free           =   pArea;
}


EXIT_DATA *new_exit( void )
{
    EXIT_DATA *pExit;

    if ( !exit_free )
    {
        pExit           =   alloc_perm( sizeof(*pExit) );
        top_exit++;
    }
    else
    {
        pExit           =   exit_free;
        exit_free       =   exit_free->next;
    }

    memset(pExit,0,sizeof(*pExit));
    pExit->u1.to_room   =   NULL;
    pExit->next         =   NULL;
    pExit->exit_info    =   0;
    pExit->door.key_vnum=   0;
    pExit->keyword      =   &str_empty[0];
    pExit->short_desc   =   &str_empty[0];
    pExit->rs_flags     =   0;

    return pExit;
}


void free_exit( EXIT_DATA *pExit )
{
    free_string( pExit->keyword );
    free_string( pExit->short_desc );

    --top_exit;

    pExit->next         =   exit_free;
    exit_free           =   pExit;
    return;
}


ROOM_INDEX_DATA *new_room_index( void )
{
    ROOM_INDEX_DATA *pRoom;
    int door;

    if ( !room_index_free )
    {
        pRoom           =   alloc_perm( sizeof(*pRoom) );
        top_room++;
    }
    else
    {
        pRoom           =   room_index_free;
        room_index_free =   room_index_free->next;
    }

    SET_MEMTYPE(pRoom,MEMTYPE_ROOM);
    pRoom->next             =   NULL;
    pRoom->people           =   NULL;
    pRoom->contents         =   NULL;
    pRoom->extra_descr      =   NULL;
    pRoom->area             =   NULL;
    pRoom->source           =   NULL;

	pRoom->environ_type	= ENVIRON_NONE;
	memset(&pRoom->environ,0,sizeof(pRoom->environ));

    // VIZZWILDS
    pRoom->wilds            =   NULL;

    pRoom->progs	    =   new_prog_data();
    pRoom->progs->progs	    =	NULL;
    pRoom->progs->vars      =	NULL;
    pRoom->index_vars       =	NULL;

    for ( door=0; door < MAX_DIR; door++ )
        pRoom->exit[door]   =   NULL;

    pRoom->name             =   &str_empty[0];
    pRoom->description      =   &str_empty[0];
    pRoom->owner	    =	&str_empty[0];
    pRoom->home_owner	    =   NULL;
    pRoom->vnum             =   0;
    pRoom->room_flags       =   0;
    pRoom->room2_flags      =   0;
    pRoom->light            =   0;
    pRoom->sector_type      =   0;
    pRoom->heal_rate	    =   100;
    pRoom->mana_rate	    =   100;
    pRoom->visited = 0;
    pRoom->id[0] = pRoom->id[1] = 0;	// Explicitly make this 0,0 until set, or left for static rooms

    pRoom->lentity = list_create(FALSE);
    pRoom->lpeople = list_create(FALSE);
    pRoom->lcontents = list_create(FALSE);
    pRoom->levents = list_create(FALSE);
    pRoom->ltokens = list_create(FALSE);
    pRoom->lclonerooms = list_create(FALSE);

    return pRoom;
}


void free_room_index( ROOM_INDEX_DATA *pRoom )
{
    int door;
    EXTRA_DESCR_DATA *pExtra;
    CONDITIONAL_DESCR_DATA *pCd;
    CONDITIONAL_DESCR_DATA *pCd_next;
    RESET_DATA *pReset;
    EVENT_DATA *ev, *ev_next;
    ROOM_INDEX_DATA *clone, *clone_next;

    free_string( pRoom->name );
    free_string( pRoom->description );
    free_string( pRoom->owner );
    free_string( pRoom->home_owner );
    if(pRoom->progs && pRoom->progs->progs) free_prog_list(pRoom->progs->progs);
    free_prog_data(pRoom->progs);

    for ( door = 0; door < MAX_DIR; door++ )
    {
        if ( pRoom->exit[door] )
            free_exit( pRoom->exit[door] );
    }

    for ( pExtra = pRoom->extra_descr; pExtra; pExtra = pExtra->next )
    {
        free_extra_descr( pExtra );
    }

    for ( pCd = pRoom->conditional_descr; pCd != NULL; pCd = pCd_next )
    {
	pCd_next = pCd->next;

	free_conditional_descr( pCd );
    }

    for ( pReset = pRoom->reset_first; pReset; pReset = pReset->next )
    {
        free_reset_data( pReset );
    }

    variable_clearfield(VAR_ROOM, pRoom);
    script_clear_room(pRoom);
    script_clear_list(pRoom);
    wipe_clearinfo_room(pRoom);

    /* be sure to free any events hooked up to this room so that they arn't called
       on the freed memory space */
    for (ev = pRoom->events; ev != NULL; ev = ev_next) {
	ev_next = ev->next_event;

	extract_event(ev);
    }

    variable_freelist(&pRoom->index_vars);
    pRoom->events = NULL;

    // Handle any outstanding cloned rooms
    // Do not worry about anything in the rooms, since everything else had to have been freed already
	for(clone = pRoom->clones; clone; clone = clone_next) {
		clone_next = clone->next;
		free_room_index(clone);
	}

    list_destroy(pRoom->lentity);
    list_destroy(pRoom->lpeople);
    list_destroy(pRoom->lcontents);
    list_destroy(pRoom->levents);
    list_destroy(pRoom->ltokens);
    list_destroy(pRoom->lclonerooms);

	if( pRoom->persist ) persist_removeroom(pRoom);

    pRoom->next     =   room_index_free;
    room_index_free =   pRoom;
    return;
}


SHOP_DATA *new_shop( void )
{
    SHOP_DATA *pShop;
    int buy;

    if ( !shop_free )
    {
        pShop           =   alloc_perm( sizeof(*pShop) );
        top_shop++;
    }
    else
    {
        pShop           =   shop_free;
        shop_free       =   shop_free->next;
    }

    pShop->next         =   NULL;
    pShop->keeper       =   0;

    for ( buy=0; buy<MAX_TRADE; buy++ )
        pShop->buy_type[buy]    =   0;

    pShop->profit_buy   =   100;
    pShop->profit_sell  =   100;
    pShop->open_hour    =   0;
    pShop->close_hour   =   23;

    return pShop;
}


void free_shop( SHOP_DATA *pShop )
{
    pShop->next = shop_free;
    shop_free   = pShop;
    return;
}


OBJ_INDEX_DATA *new_obj_index( void )
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if ( !obj_index_free )
    {
        pObj           =   alloc_perm( sizeof(*pObj) );
    }
    else
    {
        pObj            =   obj_index_free;
        obj_index_free  =   obj_index_free->next;
    }

    pObj->next          =   NULL;
    pObj->extra_descr   =   NULL;
    pObj->affected      =   NULL;
    pObj->area          =   NULL;
    pObj->name          =   str_dup( "no name" );
    pObj->short_descr   =   str_dup( "(no short description)" );
    pObj->description   =   str_dup( "(no description)" );
    pObj->full_description = &str_empty[0];
    pObj->imp_sig	=   str_dup( "none" );
    pObj->creator_sig   =   str_dup( "none" );
    pObj->vnum          =   0;
    pObj->item_type     =   ITEM_TRASH;
    pObj->extra_flags   =   0;
    pObj->extra2_flags  =   0;
    pObj->extra3_flags  =   0;
    pObj->extra4_flags  =   0;
    pObj->wear_flags    =   0;
    pObj->count         =   0;
    pObj->weight        =   0;
    pObj->cost          =   0;
    pObj->points	=   0;
    pObj->material      =   str_dup( "unknown" );      /* ROM */
    pObj->condition     =   100;                        /* ROM */
    pObj->timer		=   0;
    pObj->skeywds		=	str_dup( "none" );
    for ( value = 0; value < 8; value++ )               /* 5 - ROM */
        pObj->value[value]  =   0;

    return pObj;
}


void free_obj_index( OBJ_INDEX_DATA *pObj )
{
    EXTRA_DESCR_DATA *pExtra;
    AFFECT_DATA *pAf;

    free_string( pObj->name );
    free_string( pObj->short_descr );
    free_string( pObj->description );
    free_string( pObj->imp_sig );
    free_string( pObj->creator_sig );
    free_string( pObj->skeywds );

    free_prog_list(pObj->progs);
    variable_freelist(&pObj->index_vars);

    for ( pAf = pObj->affected; pAf; pAf = pAf->next )
        free_affect( pAf );

    for ( pExtra = pObj->extra_descr; pExtra; pExtra = pExtra->next )
        free_extra_descr( pExtra );

    pObj->next              = obj_index_free;
    obj_index_free          = pObj;
    return;
}


MOB_INDEX_DATA *new_mob_index( void )
{
    MOB_INDEX_DATA *pMob;

    if ( !mob_index_free )
    {
        pMob           =   alloc_perm( sizeof(*pMob) );
    }
    else
    {
        pMob            =   mob_index_free;
        mob_index_free  =   mob_index_free->next;
    }

    pMob->next          =   NULL;
    pMob->spec_fun      =   NULL;
    pMob->pShop         =   NULL;
    pMob->area          =   NULL;
    pMob->player_name   =   str_dup( "no name" );
    pMob->short_descr   =   str_dup( "(no short description)" );
    pMob->long_descr    =   str_dup( "(no long description)\n\r" );
    pMob->owner	        =   str_dup( "(no owner)");
    pMob->sig		=   str_dup( "(none)");
    pMob->creator_sig   =   str_dup( "(none)");
    pMob->description   =   &str_empty[0];
    pMob->vnum          =   0;
    pMob->count         =   0;
    pMob->killed        =   0;
    pMob->sex           =   0;
    pMob->level         =   0;
    pMob->act           =   ACT_IS_NPC;
    pMob->act2		=   0;
    pMob->affected_by   =   0;
    pMob->affected_by2  =   0;
    pMob->alignment     =   0;
    pMob->hitroll	=   0;
    pMob->race          =   race_lookup( "human" );
    pMob->form          =   0;
    pMob->parts         =   0;
    pMob->imm_flags     =   0;
    pMob->res_flags     =   0;
    pMob->vuln_flags    =   0;
    pMob->material      =   str_dup("unknown");
    pMob->off_flags     =   0;
    pMob->size          =   SIZE_MEDIUM;
    pMob->ac[AC_PIERCE]	=   0;
    pMob->ac[AC_BASH]	=   0;
    pMob->ac[AC_SLASH]	=   0;
    pMob->ac[AC_EXOTIC]	=   0;
    pMob->hit.number	=   0;
    pMob->hit.size	=   0;
    pMob->hit.bonus	=   0;
    pMob->mana.number	=   0;
    pMob->mana.size	=   0;
    pMob->mana.bonus	=   0;
    pMob->damage.number	=   0;
    pMob->damage.size	=   0;
    pMob->damage.bonus	=   0;
    pMob->start_pos             =   POS_STANDING;
    pMob->default_pos           =   POS_STANDING;
    pMob->wealth                =   0;
    pMob->skeywds		=	str_dup( "none" );
    pMob->attacks	=   0;
    pMob->quests =  NULL;

    return pMob;
}


void free_mob_index( MOB_INDEX_DATA *pMob )
{
    QUEST_LIST *quest_list;
    QUEST_LIST *quest_list_next;

    free_string( pMob->player_name );
    free_string( pMob->short_descr );
    free_string( pMob->long_descr );
    free_string( pMob->description );
    free_string( pMob->material );
    free_string( pMob->sig );
    free_string( pMob->skeywds );

    free_prog_list(pMob->progs);
    variable_freelist(&pMob->index_vars);

    for ( quest_list = pMob->quests; quest_list != NULL; quest_list = quest_list_next )
    {
	quest_list_next = quest_list->next;

	free_quest_list( quest_list );
    }

	free_questor_data( pMob->pQuestor );
    free_shop( pMob->pShop );

    pMob->next              = mob_index_free;
    mob_index_free          = pMob;
    return;
}


void free_trade_item(TRADE_ITEM *item)
{
    item->trade_type = 0;
    item->next = trade_item_free;
    trade_item_free = item;
    return;
}

void new_trade_item( AREA_DATA *area, sh_int type, long replenish_time, long replenish_amount,
    long max_qty, long min_price, long max_price, long obj_vnum )
{
    TRADE_ITEM *item;

    if (!trade_item_free)
    {
  item = alloc_perm(sizeof(*item) );
    }
    else
    {
  item     = trade_item_free;
  trade_item_free = trade_item_free->next;
    }

    item->trade_type    = type;
    item->replenish_time = replenish_time;
    item->replenish_amount = replenish_amount;
    item->max_qty = max_qty;
    item->min_price = min_price;
    item->max_price = max_price;
    item->obj_vnum = obj_vnum;
    item->area   = area->anum;

    item->next = area->trade_list;
    item->next_trade_produce = trade_produce_list;

    area->trade_list = item;
    trade_produce_list = item;
}


HELP_DATA *new_help(void)
{
     HELP_DATA *NewHelp;

     NewHelp = alloc_perm(sizeof(*NewHelp) );

     NewHelp->creator = NULL;
     NewHelp->min_level   = 0;
     NewHelp->keyword = str_dup("");
     NewHelp->text    = str_dup("");
     NewHelp->next    = NULL;
     NewHelp->builders = str_dup("None");
     NewHelp->related_topics = NULL;

     return NewHelp;
}


void free_help( HELP_DATA *pHelp )
{
    STRING_DATA *topic, *topic_next;

    for (topic = pHelp->related_topics; topic != NULL; topic = topic_next)
    {
	topic_next = topic->next;

	free_string_data(topic);
    }

    free_string(pHelp->creator);
    free_string(pHelp->keyword);
    free_string(pHelp->text );
    free_string(pHelp->modified_by);
    free_string(pHelp->builders);

    pHelp->next = help_free;
    help_free   = pHelp;
}


QUEST_DATA *new_quest( void )
{
    QUEST_DATA *pQuest;

    if ( !quest_free )
    {
	pQuest = alloc_perm( sizeof(*pQuest) );
    }
    else
    {
	pQuest = quest_free;
	quest_free = quest_free->next;
    }

    pQuest->next = NULL;
    pQuest->questgiver = (long)NULL;
    pQuest->parts = NULL;
    pQuest->msg_complete = FALSE;

    top_quest++;

    return pQuest;
}


void free_quest( QUEST_DATA *pQuest )
{
    QUEST_PART_DATA *part;
    QUEST_PART_DATA *next_part;

    part = pQuest->parts;

    while( part != NULL) {
	next_part = part->next;
        free_quest_part( part );
	part = next_part;
    }

    pQuest->next         =   quest_free;
    quest_free             =   pQuest;
    return;
}


QUEST_PART_DATA *new_quest_part( void )
{
    QUEST_PART_DATA *pPart;

    if ( !quest_part_free )
    {
	pPart = alloc_perm( sizeof(*pPart) );
    }
    else
    {
	pPart = quest_part_free;
	quest_part_free = quest_part_free->next;
    }

    pPart->pObj = NULL;
    pPart->next = NULL;
    pPart->obj = -1;
    pPart->mob = -1;
    pPart->obj_sac = -1;
    pPart->mob_rescue = -1;
    pPart->room = -1;
    pPart->description = &str_empty[0];
    pPart->custom_task = FALSE;
    pPart->complete = FALSE;

    top_quest_part++;

    return pPart;
}


void free_quest_part( QUEST_PART_DATA *pPart )
{
    pPart->pObj = NULL;

    pPart->next         =   quest_part_free;
    quest_part_free     =   pPart;

    if(pPart->description) free_string(pPart->description);
    return;
}


NPC_SHIP_INDEX_DATA *new_npc_ship_index( void )
{
    NPC_SHIP_INDEX_DATA *npc_ship;

    if ( !npc_ship_index_free )
    {
        npc_ship           =   alloc_perm( sizeof(*npc_ship) );
        top_npc_ship++;
    }
    else
    {
        npc_ship           =   npc_ship_index_free;
        npc_ship_index_free =  npc_ship_index_free->next;
    }

    npc_ship->next             =   NULL;
    npc_ship->name             =   (char *)&str_empty[0];
    npc_ship->captain          =   NULL;
    npc_ship->flag             =   (char *)&str_empty[0];
    npc_ship->ship_type        =   0;
    npc_ship->area             =   str_dup("");
    npc_ship->npc_type         =   1;
    npc_ship->npc_sub_type     =   0;
    npc_ship->chance_repop     =   0;
    npc_ship->initial_ships_destroyed = 0;
    npc_ship->ships_destroyed  = 0;
    return npc_ship;
}


void free_npc_ship_index( NPC_SHIP_INDEX_DATA *npc_ship )
{
    free_string( npc_ship->name );
    free_string( npc_ship->flag );

    npc_ship->next     =   npc_ship_index_free;
    npc_ship_index_free =   npc_ship;
    return;
}


WAYPOINT_DATA *new_waypoint( void )
{
    WAYPOINT_DATA *waypoint;

    if ( !waypoint_free )
    {
        waypoint           =   alloc_perm( sizeof(*waypoint) );
    }
    else
    {
        waypoint           =   waypoint_free;
        waypoint_free      =  waypoint_free->next;
    }

    waypoint->x = 0;
    waypoint->y = 0;
    waypoint->next = NULL;

    top_waypoint++;

    return waypoint;
}


void free_waypoint( WAYPOINT_DATA *waypoint )
{
    waypoint->next     =   waypoint_free;
    waypoint_free =   waypoint;
    top_waypoint--;
    return;
}


SHIP_CREW_DATA *new_ship_crew( void )
{
    SHIP_CREW_DATA *crew;

    if ( !ship_crew_free )
    {
        crew           =   alloc_perm( sizeof(*crew) );
    }
    else
    {
        crew           =   ship_crew_free;
        ship_crew_free      =  ship_crew_free->next;
    }

    crew->vnum = 0;

    top_ship_crew++;

    return crew;
}


void free_ship_crew( SHIP_CREW_DATA *crew )
{
    crew->next     =  ship_crew_free;
    ship_crew_free =   crew;
    top_ship_crew--;
    return;
}


CHAT_ROOM_DATA *new_chat_room( void )
{
    CHAT_ROOM_DATA *pChat;

    if ( !chat_room_free )
    {
	pChat = alloc_perm( sizeof(*pChat) );
    }
    else
    {
	pChat = chat_room_free;
	chat_room_free = chat_room_free->next;
    }

    pChat->next	= NULL;
    pChat->name = NULL;
    pChat->ops 	= NULL;
    pChat->bans = NULL;
    pChat->password = NULL;
    pChat->topic = NULL;
    pChat->max_people = 0;
    pChat->curr_people = 0;
    pChat->created_by = NULL;

    top_chatroom++;

    return pChat;
}


void free_chat_room( CHAT_ROOM_DATA *pChat )
{
    CHAT_OP_DATA *op;
    CHAT_OP_DATA *op_next;

    free_string( pChat->name );
    free_string( pChat->password );
    free_string( pChat->created_by );
    free_string( pChat->topic );

    for ( op = pChat->ops; op != NULL; op = op_next )
    {
	op_next = op->next;

	free_chat_op( op );
    }

    pChat->name = NULL;
    pChat->topic = NULL;
    pChat->password = NULL;
    pChat->created_by = NULL;
    pChat->max_people = 0;
    pChat->curr_people = 0;

    pChat->next       =   chat_room_free;
    chat_room_free    =   pChat;

    top_chatroom--;
    return;
}


CHAT_OP_DATA *new_chat_op( void )
{
    CHAT_OP_DATA *pOp;

    if ( !chat_op_free )
    {
	pOp = alloc_perm( sizeof(*pOp) );
    }
    else
    {
	pOp = chat_op_free;
	chat_op_free = chat_op_free->next;
    }

    pOp->chat_room = NULL;
    pOp->name = NULL;

    top_chat_op++;

    return pOp;
}


void free_chat_op( CHAT_OP_DATA *pOp )
{
    free_string( pOp->name );

    pOp->next       =   chat_op_free;
    chat_op_free    =   pOp;

    top_chat_op--;
    return;
}


CHAT_BAN_DATA *new_chat_ban( void )
{
    CHAT_BAN_DATA *pBan;

    if ( !chat_ban_free )
    {
	pBan = alloc_perm( sizeof(*pBan ) );
    }
    else
    {
	pBan = chat_ban_free;
	chat_ban_free = chat_ban_free->next;
    }

    pBan->chat_room = NULL;
    pBan->name = NULL;
    pBan->banned_by = NULL;

    top_chat_ban++;

    return pBan;
}


void free_chat_ban( CHAT_BAN_DATA *pBan )
{
    pBan->chat_room = NULL;
    free_string( pBan->name );
    free_string( pBan->banned_by );

    pBan->next	= chat_ban_free;
    chat_ban_free	= pBan;

    top_chat_ban--;
}


IGNORE_DATA *new_ignore( void )
{
    IGNORE_DATA *ignore;

    if ( !ignore_free )
    {
	ignore = alloc_perm( sizeof(*ignore ) );
    }
    else
    {
	ignore = ignore_free;
	ignore_free = ignore_free->next;
    }

    ignore->name = NULL;
    ignore->reason = NULL;

    return ignore;
}


void free_ignore( IGNORE_DATA *ignore )
{
    free_string( ignore->name );
    free_string( ignore->reason );
    ignore->next = ignore_free;
    ignore_free = ignore;
}


STRING_DATA *new_string_data( void )
{
    STRING_DATA *string;
    if ( !string_data_free )
    {
	string = alloc_perm( sizeof( *string ) );
    }
    else
    {
	string = string_data_free;
	string_data_free = string_data_free->next;
    }

    string->string = NULL;
    string->next = NULL;

    return string;
}


void free_string_data( STRING_DATA *string )
{
    free_string( string->string );
    string->next = string_data_free;
    string_data_free  = string;
}


AUCTION_DATA *new_auction( void )
{
    AUCTION_DATA *auction;

    if ( !auction_free )
    {
	auction = alloc_perm( sizeof(*auction ) );
    }
    else
    {
	auction = auction_free;
	auction_free = auction_free->next;
    }

    auction->item = NULL;
    auction->owner = NULL;
    auction->high_bidder = NULL;
    auction->status = 0;
    auction->current_bid = 0;
    auction->minimum_bid = 0;
    auction->silver_held = 0;
    auction->gold_held = 0;

    return auction;
}


void free_auction( AUCTION_DATA *auction )
{
    auction->next = auction_free;
    auction_free = auction->next;
}


AUTO_WAR *new_auto_war( int war_type, int min_players, int min_level, int max_level )
{
    AUTO_WAR *auto_war;

    if (!auto_war_free)
    {
	auto_war = alloc_perm(sizeof(*auto_war) );
    }
    else
    {
	auto_war     = auto_war_free;
	auto_war_free = auto_war_free->next;
    }

    auto_war->war_type = war_type;
    auto_war->min_players = min_players;
    auto_war->min = min_level;
    auto_war->max = max_level;
    auto_war->next = NULL;

    return auto_war;
}


void free_auto_war( AUTO_WAR *m_auto_war )
{
    while( m_auto_war->team_players != NULL )
    {
	stop_fighting( m_auto_war->team_players, FALSE);
	act( "{D$n disappears in puff of smoke.{x", m_auto_war->team_players, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act( "You have been transported to Plith.", m_auto_war->team_players, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
	char_from_room( m_auto_war->team_players );
	char_to_room( m_auto_war->team_players, get_room_index( ROOM_VNUM_TEMPLE ) );
	do_function( m_auto_war->team_players, &do_look, "auto");
	char_from_team( m_auto_war->team_players );
    }

    m_auto_war->next = auto_war_free;
    auto_war_free  = m_auto_war;
    auto_war = NULL;

    auto_war_battle_timer = 0;
    auto_war_timer = 0;
    return;
}


GQ_MOB_DATA *new_gq_mob( void )
{
    GQ_MOB_DATA *gq_mob;

    if (!gq_mob_free )
    {
	gq_mob = alloc_perm(sizeof(*gq_mob));
    }
    else
    {
	gq_mob = gq_mob_free;
	gq_mob_free = gq_mob_free->next;
    }

    gq_mob->vnum = 0;
    gq_mob->class = 0;
    gq_mob->group = FALSE;
    gq_mob->obj = 0;
    gq_mob->count = 0;

    return gq_mob;
}


void free_gq_mob( GQ_MOB_DATA *gq_mob )
{
    gq_mob->next = gq_mob_free;
    gq_mob_free  = gq_mob;
}


GQ_OBJ_DATA *new_gq_obj( void )
{
    GQ_OBJ_DATA *gq_obj;

    if (!gq_obj_free )
    {
	gq_obj = alloc_perm(sizeof(*gq_obj));
    }
    else
    {
	gq_obj = gq_obj_free;
	gq_obj_free = gq_obj_free->next;
    }

    gq_obj->qp_reward = 0;
    gq_obj->prac_reward = 0;
    gq_obj->exp_reward = 0;
    gq_obj->silver_reward = 0;
    gq_obj->gold_reward = 0;

    return gq_obj;
}


void free_gq_obj( GQ_OBJ_DATA *gq_obj )
{
    gq_obj->next = gq_obj_free;
    gq_obj_free  = gq_obj;
}


AMBUSH_DATA *new_ambush( void )
{
    AMBUSH_DATA *ambush;

    if (!ambush_free)
    {
	ambush = alloc_perm(sizeof(*ambush) );
    }
    else
    {
	ambush     = ambush_free;
	ambush_free = ambush_free->next;
    }

    ambush->type = 0;
    ambush->min_level = 0;
    ambush->max_level = 0;
    ambush->command = NULL;

    return ambush;
};


void free_ambush( AMBUSH_DATA *ambush )
{
    if ( ambush == NULL )
	return;

    free_string( ambush->command );

    ambush->next = ambush_free;
    ambush_free  = ambush;
}


QUEST_INDEX_DATA *new_quest_index( void )
{
    QUEST_INDEX_DATA *quest_index;

    if (!quest_index_free)
    {
	quest_index = alloc_perm(sizeof(*quest_index) );
    }
    else
    {
	quest_index      = quest_index_free;
	quest_index_free = quest_index_free->next;
    }

    quest_index->vnum = 0;
    quest_index->area = NULL;
    quest_index->name = str_dup("no name");

    return quest_index;
}


void free_quest_index( QUEST_INDEX_DATA *quest_index )
{
    free_string( quest_index->name );

    quest_index->next = quest_index_free;
    quest_index_free = quest_index;
}


QUEST_LIST *new_quest_list( void )
{
    QUEST_LIST *quest_list;

    if (!quest_list_free)
    {
	quest_list = alloc_perm(sizeof(*quest_list) );
    }
    else
    {
	quest_list      = quest_list_free;
	quest_list_free = quest_list_free->next;
    }

    quest_list->vnum = 0;

    return quest_list;
}


void free_quest_list( QUEST_LIST *quest_list )
{
    quest_list->next = quest_list_free;
    quest_list_free = quest_list;
}


MAIL_DATA *new_mail( void )
{
    MAIL_DATA *mail;

    if ( !mail_free )
    {
	mail = alloc_perm( sizeof( *mail ) );
    }
    else
    {
	mail = mail_free;
	mail_free = mail_free->next;
    }

    mail->objects = NULL;
    mail->sender = NULL;
    mail->recipient = NULL;
    mail->message = NULL;
    mail->status = 0;
    mail->picked_up = FALSE;

    return mail;
}


void free_mail( MAIL_DATA *mail )
{
    free_string( mail->sender );
    free_string( mail->recipient );
    free_string( mail->message );

    mail->next = mail_free;
    mail_free  = mail;
}


HELP_CATEGORY *new_help_category()
{
    HELP_CATEGORY *hcat;

    if (!help_category_free)
    {
    	hcat = alloc_perm(sizeof(*hcat));
    }
    else
    {
	hcat = help_category_free;
    	help_category_free = help_category_free->next;
    }

    hcat->next = NULL;
    hcat->name = NULL;
    hcat->description = NULL;
    hcat->creator = str_dup("Unknown");
    hcat->modified_by = str_dup("Unknown");
    hcat->inside_cats = NULL;
    hcat->inside_helps = NULL;
    hcat->builders = str_dup("None");

    return hcat;
}


void free_help_category( HELP_CATEGORY *hcat )
{
    HELP_CATEGORY *hCatNest, *hCatNestNext;
    HELP_DATA *help, *helpNext;

    // Free up nested cats
    for (hCatNest = hcat->inside_cats; hCatNest != NULL; hCatNest = hCatNestNext ) {
	hCatNestNext = hCatNest->next;

	free_help_category(hCatNest);
    }

    // Free up helps
    for (help = hcat->inside_helps; help != NULL; help = helpNext) {
	helpNext = help->next;

    	free_help( help );
    }

    free_string(hcat->name);
    free_string(hcat->description);
    free_string(hcat->creator);
    free_string(hcat->modified_by);
    free_string(hcat->builders);

    hcat->next = help_category_free;
    help_category_free = hcat;
}

INVASION_QUEST *new_invasion_quest(void)
{
    INVASION_QUEST *invasion_quest;

    if (!invasion_quest_free)
    {
	invasion_quest = alloc_perm(sizeof(*invasion_quest) );
    }
    else
    {
	invasion_quest      = invasion_quest_free;
	invasion_quest_free = invasion_quest_free->next;
    }
    return invasion_quest;
}


void free_invasion_quest(INVASION_QUEST *invasion_quest)
{
    invasion_quest->next   = invasion_quest_free;
    invasion_quest_free    = invasion_quest;
}


STORM_DATA *new_storm_data(void)
{
    STORM_DATA *storm_data;

    if (!storm_data_free)
    {
	storm_data = alloc_perm(sizeof(*storm_data) );
    }
    else
    {
	storm_data      = storm_data_free;
	storm_data_free = storm_data_free->next;
    }
    return storm_data;
}


void free_storm_data(STORM_DATA *storm_data)
{
    storm_data->next   = storm_data_free;
    storm_data_free    = storm_data;
}


SPELL_DATA *new_spell(void)
{
    SPELL_DATA *spell;
    if (!spell_free)
	spell = alloc_perm(sizeof(*spell));
    else {
	spell = spell_free;
	spell_free = spell_free->next;
    }

    return spell;
}


void free_spell(SPELL_DATA *spell)
{
    spell->next = spell_free;
    spell_free  = spell;
}


COMMAND_DATA *new_command()
{
    COMMAND_DATA *cmd;

    if (!command_free)
        cmd = alloc_perm(sizeof(*cmd));
    else {
	cmd = command_free;
	command_free = command_free->next;
    }

    return cmd;
}


void free_command(COMMAND_DATA *cmd)
{
    free_string(cmd->name);

    cmd->next = command_free;
    command_free = cmd;
}


TOKEN_INDEX_DATA *new_token_index()
{
    TOKEN_INDEX_DATA *token_index;
    int i;

    if (!token_index_free)
	token_index = alloc_perm(sizeof(*token_index));
    else
    {
	token_index = token_index_free;
	token_index_free = token_index_free->next;
    }

    token_index->name = str_dup("(no name)");
    token_index->description = &str_empty[0];

    for (i = 0; i < MAX_TOKEN_VALUES; i++)
	token_index->value_name[i] = str_dup("Unused");

    token_index->progs = NULL;
    token_index->index_vars = NULL;

    return token_index;
}


void free_token_index(TOKEN_INDEX_DATA *token_index)
{
    int i;

    free_string(token_index->name);

    for (i = 0; i < MAX_TOKEN_VALUES; i++) {
	if (token_index->value_name[i] != NULL)
	    free_string(token_index->value_name[i]);
    }

    free_prog_list(token_index->progs);
    variable_freelist(&token_index->index_vars);

    token_index->next = token_index_free;
    token_index_free = token_index;
}


TOKEN_DATA *new_token()
{
    TOKEN_DATA *token;

    if (!token_free)
	token = alloc_perm(sizeof(*token));
    else
    {
	token = token_free;
	token_free = token_free->next;
    }

    token->progs = NULL;
    SET_MEMTYPE(token,MEMTYPE_TOKEN);
    VALIDATE(token);

    return token;
}


void free_token(TOKEN_DATA *token)
{
	TOKEN_DATA *prev, *cur;

    free_string(token->name);
    if(token->pIndexData)	// @@@NIB : 20070127 : for "tokenexists" ifcheck
	token->pIndexData->loaded--;

    variable_clearfield(VAR_TOKEN, token);
    script_clear_token(token);
    script_clear_list(token);
    wipe_clearinfo_token(token);

    token->id[0] = token->id[1] = 0;

	for( prev = NULL, cur = global_tokens; cur; prev = cur, cur = cur->global_next)
		if( cur == token )
			break;

	if( cur ) {
		if( prev )
			prev->global_next = cur->global_next;
		else
			global_tokens = cur->global_next;
		cur->global_next = NULL;
	}

    INVALIDATE(token);
    token->next = token_free;
    token_free = token;

}

EVENT_DATA *new_event(void)
{
    static EVENT_DATA ev_zero;
    EVENT_DATA *ev;

    if (ev_free == NULL)
	ev = alloc_perm(sizeof(*ev));
    else
    {
	ev = ev_free;
	ev_free = ev_free->next;
    }

    *ev = ev_zero;
    VALIDATE(ev);
    return ev;
}


void free_event(EVENT_DATA *ev)
{
    if (!IS_VALID(ev))
	return;

    free_string(ev->args);
    if(ev->info) free(ev->info);
    INVALIDATE(ev);

    ev->next = ev_free;
    ev_free = ev;
}


PROJECT_DATA *new_project(void)
{
    static PROJECT_DATA proj_zero;
    PROJECT_DATA *proj;

    if (!project_free)
	proj = alloc_perm(sizeof(*proj));
    else
    {
	proj = project_free;
	project_free = project_free->next;
    }

    *proj = proj_zero;
    VALIDATE(proj);
    proj->builders = NULL;
    proj->areas = NULL;
    proj->name = str_dup("New Project");
    proj->leader = str_dup("Nobody");
    proj->summary = str_dup("No summary");
    proj->description = str_dup("");

    return proj;
}


void free_project(PROJECT_DATA *proj)
{
    PROJECT_INQUIRY_DATA *pinq, *pinq_next;
    PROJECT_BUILDER_DATA *pb, *pb_next;
    STRING_DATA *string, *string_next;


    free_string(proj->name);
    free_string(proj->summary);
    free_string(proj->description);
    free_string(proj->leader);

    for (string = proj->areas; string != NULL; string = string_next) {
	string_next = string->next;

	free_string_data(string);
    }

    for (pinq = proj->inquiries; pinq != NULL; pinq = pinq_next) {
	pinq_next = pinq->next;

	free_project_inquiry(pinq);
    }

    for (pb = proj->builders; pb != NULL; pb = pb_next) {
	pb_next = pb->next;

	free_project_builder(pb);
    }

    INVALIDATE(proj);
    proj->next = project_free;
    project_free = proj;
}


PROJECT_BUILDER_DATA *new_project_builder(void)
{
    PROJECT_BUILDER_DATA *pb;

    if (!project_builder_free)
        pb = alloc_perm(sizeof(*pb));
    else
    {
	pb = project_builder_free;
	project_builder_free = project_builder_free->next;
    }

    pb->next = NULL;

    return pb;
}


void free_project_builder(PROJECT_BUILDER_DATA *pb)
{
    STRING_DATA *string, *string_next;

    for (string = pb->commands; string != NULL; string = string_next) {
	string_next = string->next;

	free_string_data(string);
    }

    free_string(pb->name);

    pb->next = project_builder_free;
    project_builder_free = pb;
}


PROJECT_INQUIRY_DATA *new_project_inquiry(void)
{
    PROJECT_INQUIRY_DATA *pinq;

    if (!project_inquiry_free)
	pinq = alloc_perm(sizeof(*pinq));
    else
    {
	pinq = project_inquiry_free;
	project_inquiry_free = project_inquiry_free->next;
    }

    pinq->next = NULL;
    pinq->replies = NULL;
    pinq->subject = str_dup("No subject.");
    pinq->date = current_time;
    pinq->text = str_dup("");
    pinq->closed = 0;
    pinq->closed_by = NULL;

    return pinq;
}


void free_project_inquiry(PROJECT_INQUIRY_DATA *pinq)
{
    free_string(pinq->sender);
    free_string(pinq->subject);
    free_string(pinq->text);
    free_string(pinq->closed_by);

    pinq->next = project_inquiry_free;
    project_inquiry_free = pinq;
}


IMMORTAL_DATA *new_immortal(void)
{
    IMMORTAL_DATA *immortal;

    if (!immortal_free)
	immortal = alloc_perm(sizeof(*immortal));
    else
    {
	immortal = immortal_free;
	immortal_free = immortal_free->next;
    }

    immortal->next = NULL;
    immortal->name = str_dup("No name");
    immortal->duties = 0;
    immortal->build_project = NULL;
    immortal->imm_flag = str_dup("None");
    immortal->last_olc_command = 0;
    immortal->bamfin = str_dup("");
    immortal->bamfout = str_dup("");
    immortal->leader = NULL;	// Added this... should this be str_dup("")?

    return immortal;
}


void free_immortal(IMMORTAL_DATA *immortal)
{
    free_string(immortal->name);
    free_string(immortal->imm_flag);
    free_string(immortal->bamfin);
    free_string(immortal->bamfout);
    free_string(immortal->leader);

    immortal->next = immortal_free;
    immortal_free = immortal;
}


LOG_ENTRY_DATA *new_log_entry(void)
{
    LOG_ENTRY_DATA *log;

    if (!log_entry_free)
	log = alloc_perm(sizeof(*log));
    else
    {
	log = log_entry_free;
	log_entry_free = log_entry_free->next;
    }

    log->next = NULL;
    log->date = current_time;

    return log;
}


void free_log_entry(LOG_ENTRY_DATA *log)
{
    free_string(log->text);
}

// @@@NIB : 20070123 ----------
SCRIPT_DATA *script_freechain = NULL;

SCRIPT_DATA *new_script(void)
{
	static SCRIPT_DATA s_zero;
	SCRIPT_DATA *s;

	if (!script_freechain)
		s = alloc_perm(sizeof(*s));
	else {
		s = script_freechain;
		script_freechain = script_freechain->next;
	}

	*s = s_zero;
	s->edit_src = s->src = str_dup("");
	s->name = str_dup("");
	s->flags = 0;
	s->area = NULL;

	return s;
}

void free_script_code(SCRIPT_CODE *code, int lines)
{
	int i;
	DBG2ENTRY2(PTR,code,NUM,lines);
	if(!code) return;

	for(i=0;i<lines;i++) {
		if(code[i].rest) {
			if(code[i].opcode == OP_IF || code[i].opcode == OP_WHILE || code[i].opcode == OP_ELSEIF)
				free_boolexp((BOOLEXP *)code[i].rest);
			else
				free_string(code[i].rest);
		}
	}

	free_mem(code,i *sizeof(SCRIPT_CODE));
}

void free_script(SCRIPT_DATA *s)
{
	if (!s) return;

	if(s->code) {
		free_script_code(s->code, s->lines);
		s->code = NULL;
	}
	if(s->src != s->edit_src) free_string(s->edit_src);
	free_string(s->src);
	free_string(s->name);

	s->next = script_freechain;
	script_freechain = s;
}
// @@@NIB : 20070123 ----------


struct affect_name_type {
	struct affect_name_type *next;
	char *name;
};

struct affect_name_type *affect_names = NULL;

char *create_affect_cname(char *name)
{
	struct affect_name_type *cur;

	// Search the list for existing names...
	for(cur = affect_names;cur;cur = cur->next)
		if(!str_cmp(cur->name,name))
			return cur->name;

	cur = malloc(sizeof(struct affect_name_type));
	if(!cur) return NULL;

	cur->name = str_dup(name);
	cur->next = affect_names;
	affect_names = cur;

	return cur->name;
}

char *get_affect_cname(char *name)
{
	struct affect_name_type *cur;

	// Search the list for existing names...
	for(cur = affect_names;cur;cur = cur->next)
		if(!str_cmp(cur->name,name))
			return cur->name;

	return NULL;
}


AFFLICTION_DATA *new_affliction(void)
{
	static AFFLICTION_DATA aff_zero;
	AFFLICTION_DATA *aff;

	if (affliction_free == NULL)
		aff = alloc_perm(sizeof(*aff));
	else {
		aff = affliction_free;
		affliction_free = affliction_free->next;
	}

	*aff = aff_zero;

	top_affliction++;

	VALIDATE(aff);
	return aff;
}


void free_affliction(AFFLICTION_DATA *aff)
{
	if (!IS_VALID(aff))
		return;

	INVALIDATE(aff);
	aff->next = affliction_free;
	affliction_free = aff;

	top_affliction--;
}

BOOLEXP *boolexp_free = NULL;
BOOLEXP *new_boolexp()
{
	BOOLEXP *boolexp;
	if(boolexp_free == NULL)
		boolexp = alloc_perm(sizeof(*boolexp));
	else {
		boolexp = boolexp_free;
		boolexp_free = boolexp_free->left;
	}

	boolexp->type = BOOLEXP_TRUE;
	boolexp->left = NULL;
	boolexp->right = NULL;
	boolexp->parent = NULL;
	boolexp->rest = NULL;

	return boolexp;
}

void free_boolexp(BOOLEXP *boolexp)
{

	if( boolexp->left )
		free_boolexp(boolexp->left);

	if( boolexp->right )
		free_boolexp(boolexp->right);

	if( boolexp->rest )
		free_string(boolexp->rest);

	boolexp->left = boolexp_free;
	boolexp_free = boolexp;
}

QUESTOR_DATA *questor_free = NULL;

QUESTOR_DATA *new_questor_data()
{
	QUESTOR_DATA *q;
	if(!questor_free)
		q = alloc_perm(sizeof(QUESTOR_DATA));
	else
	{
		q = questor_free;
		questor_free = questor_free->next;
	}

	VALIDATE(q);
	q->header = &str_empty[0];
	q->footer = &str_empty[0];
	q->prefix = &str_empty[0];
	q->footer = &str_empty[0];
	q->line_width = 70;

	return q;
}

void free_questor_data(QUESTOR_DATA *q)
{
	if(!IS_VALID(q)) return;

	free_string(q->header);
	free_string(q->footer);
	free_string(q->prefix);
	free_string(q->footer);

	INVALIDATE(q);
	q->next = questor_free;
	questor_free = q;

}