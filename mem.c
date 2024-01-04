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

extern bool fBootDb;

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
SHOP_STOCK_DATA *shop_stock_free;
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
SHIP_INDEX_DATA *ship_index_free;
SHIP_DATA *ship_free;

WNUM *new_wnum_data()
{
    return alloc_mem(sizeof(WNUM));
}

void *copy_wnum_data(void *ptr)
{
    WNUM *src = (WNUM *)ptr;
    WNUM *data = alloc_mem(sizeof(WNUM));

    data->pArea = src->pArea;
    data->vnum = src->vnum;

    return data;
}

void free_wnum_data(WNUM *data)
{
    free_mem(data, sizeof(WNUM));
}

void delete_wnum_data(void *ptr)
{
    free_wnum_data((WNUM *)ptr);
}

WNUM_LOAD *new_list_wnum_load()
{
    return alloc_mem(sizeof(WNUM_LOAD));
}

void free_list_wnum_load(WNUM_LOAD *wnum)
{
    free_mem(wnum,sizeof(WNUM_LOAD));
}

static void delete_list_wnum_load(void *ptr)
{
    free_list_wnum_load((WNUM_LOAD *)ptr);
}

LLIST_UID_DATA *new_list_uid_data()
{
	return alloc_mem(sizeof(LLIST_UID_DATA));
}

// NOT DOUBLE FREE SAFE
void free_list_uid_data(LLIST_UID_DATA *luid)
{
	free_mem(luid,sizeof(LLIST_UID_DATA));
}

void *copy_list_uid_data(void *ptr)
{
    LLIST_UID_DATA *src = (LLIST_UID_DATA *)ptr;
    LLIST_UID_DATA *data = alloc_mem(sizeof(LLIST_UID_DATA));

    data->ptr = src->ptr;
    data->id[0] = src->id[0];
    data->id[1] = src->id[1];

    return data;
}

void delete_list_uid_data(void *ptr)
{
	free_list_uid_data((LLIST_UID_DATA *)ptr);
}

static void *copy_waypoint(void *ptr)
{
	return clone_waypoint((WAYPOINT_DATA *)ptr);
}

static void delete_waypoint(void *ptr)
{
	free_waypoint((WAYPOINT_DATA *)ptr);
}

LLIST *new_waypoints_list()
{
	return list_createx(FALSE,copy_waypoint,delete_waypoint);
}

static void delete_ship_route(void *ptr)
{
	free_ship_route((SHIP_ROUTE *)ptr);
}


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
    entry->flags = SKILL_AUTOMATIC;
	entry->isspell = FALSE;
	//entry->practice = TRUE;
	//entry->improve = TRUE;

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

    d->last_area = NULL;
    d->last_area_region = NULL;
    d->last_room_sector = SECT_NONE;
    d->last_room_flag[0] = 0;
    d->last_room_flag[0] = 0;

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
    if( ed->description )
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
    obj->lstache = list_create(FALSE);
	obj->lock = NULL;
	obj->waypoints = NULL;

    VALIDATE(obj);
    return obj;
}

void free_obj(OBJ_DATA *obj)
{
    ITERATOR it;
    OBJ_DATA *item;
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
    obj->events_tail = NULL;
    obj->id[0] = obj->id[1] = 0;

	if( obj->persist ) persist_removeobject(obj);

	list_destroy(obj->ltokens);
	list_destroy(obj->lcontains);
	list_destroy(obj->lclonerooms);

    iterator_start(&it, obj->lstache);
    while((item = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        extract_obj(item);
    }
    iterator_stop(&it);
    list_destroy(obj->lstache);

	if(obj->owner_name != NULL)		free_string(obj->owner_name);
	if(obj->owner_short != NULL)	free_string(obj->owner_short);

	if(obj->lock)
	{
		free_lock_state(obj->lock);
		obj->lock = NULL;
	}

	if(obj->waypoints)
	{
		list_destroy(obj->waypoints);
		obj->waypoints = NULL;
	}

    free_ammo_data(AMMO(obj));
    free_book_data(BOOK(obj));
    free_container_data(CONTAINER(obj));
    free_fluid_container_data(FLUID_CON(obj));
    free_food_data(FOOD(obj));
    free_furniture_data(FURNITURE(obj));
    free_ink_data(INK(obj));
    free_instrument_data(INSTRUMENT(obj));
    free_jewelry_data(JEWELRY(obj));
    free_light_data(LIGHT(obj));
    free_money_data(MONEY(obj));
    free_book_page(PAGE(obj));
    free_portal_data(PORTAL(obj));
    free_scroll_data(SCROLL(obj));
    free_tattoo_data(TATTOO(obj));
    free_wand_data(WAND(obj));
    free_weapon_data(WEAPON(obj));

    INVALIDATE(obj);

    obj->next   = obj_free;
    obj_free    = obj;
}


static void delete_aura_data(void *ptr)
{
    free_aura_data((AURA_DATA *)ptr);
}

static void delete_reputation_data(void *ptr)
{
    free_reputation_data((REPUTATION_DATA *)ptr);
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
    ch->affected_by[0] 		= 0;
    ch->affected_by[1]		= 0;
    ch->affected_by_perm[0]		= 0;
    ch->affected_by_perm[1]		= 0;
    ch->imm_flags     =   0;
    ch->res_flags     =   0;
    ch->vuln_flags    =   0;
    ch->imm_flags_perm     =   0;
    ch->res_flags_perm     =   0;
    ch->vuln_flags_perm    =   0;

    ch->cast_target_name 	= NULL;
    ch->casting_failure_message = NULL;
    ch->projectile_victim	= NULL;
    ch->cast_skill = NULL;
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
    ch->seal_book = NULL;
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
    ch->imbuing = 0;
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
    ch->force_fading = 0;
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
    ch->auras           = list_createx(FALSE, NULL, delete_aura_data);
    ch->lstache         = list_create(FALSE);
    ch->reputations     = list_createx(FALSE, NULL, delete_reputation_data);

    ch->deathsight_vision = 0;
    ch->in_damage_function = FALSE;

    ch->checkpoint = NULL;
    ch->tempstring = NULL;
    ch->shop = NULL;
    ch->mob_reputations = NULL;
    ch->factions = list_create(FALSE);

    return ch;
}


void free_char( CHAR_DATA *ch )
{
    ITERATOR it;
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
    list_destroy(ch->auras);
    list_destroy(ch->reputations);

    iterator_start(&it, ch->lstache);
    while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        extract_obj(obj);
    }
    iterator_stop(&it);
    list_destroy(ch->lstache);
    list_destroy(ch->factions);

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
    ch->events_tail = NULL;
    ch->id[0] = ch->id[1] = 0;

    ch->checkpoint = NULL;

    MOB_REPUTATION_DATA *mob_rep, *mob_rep_next;
    for(mob_rep = ch->mob_reputations; mob_rep; mob_rep = mob_rep_next)
    {
        mob_rep_next = mob_rep->next;
        free_mob_reputation_data(mob_rep);
    }
    ch->mob_reputations = NULL;

	if( ch->persist ) persist_removemobile(ch);

	if( ch->shop ) free_shop(ch->shop);

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
    pcdata->title = str_dup("");
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

    pcdata->unlocked_areas = list_create(FALSE);
    pcdata->ships = list_create(FALSE);
    pcdata->spam_block_navigation = false;

    pcdata->extra_commands = NULL;  // This will only ever be not-null during its execution

    pcdata->group_known = list_create(FALSE);

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

    list_destroy(pcdata->unlocked_areas);
    list_destroy(pcdata->ships);
    list_destroy(pcdata->group_known);

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



void get_ship_id(SHIP_DATA *ship)
{
	if(!ship->id[0] && !ship->id[1]) {
		ship->id[0] = gconfig.next_ship_uid[0];
		ship->id[1] = gconfig.next_ship_uid[1];
		if(!++gconfig.next_ship_uid[0])
			++gconfig.next_ship_uid[1];

		if(gconfig.next_ship_uid[0] == gconfig.next_ship_uid[2] &&
			gconfig.next_ship_uid[1] == gconfig.next_ship_uid[3]) {
			gconfig.next_ship_uid[2] += UID_INC;
			if(!gconfig.next_ship_uid[2])
				++gconfig.next_ship_uid[3];
			gconfig_write();
		}
	}
}

void get_instance_id(INSTANCE *inst)
{
	if(!inst->uid[0] && !inst->uid[1]) {
		inst->uid[0] = gconfig.next_instance_uid[0];
		inst->uid[1] = gconfig.next_instance_uid[1];
		if(!++gconfig.next_instance_uid[0])
			++gconfig.next_instance_uid[1];

		if(gconfig.next_instance_uid[0] == gconfig.next_instance_uid[2] &&
			gconfig.next_instance_uid[1] == gconfig.next_instance_uid[3]) {
			gconfig.next_instance_uid[2] += UID_INC;
			if(!gconfig.next_instance_uid[2])
				++gconfig.next_instance_uid[3];
			gconfig_write();
		}
	}
}

void get_dungeon_id(DUNGEON *dng)
{
	if(!dng->uid[0] && !dng->uid[1]) {
		dng->uid[0] = gconfig.next_dungeon_uid[0];
		dng->uid[1] = gconfig.next_dungeon_uid[1];
		if(!++gconfig.next_dungeon_uid[0])
			++gconfig.next_dungeon_uid[1];

		if(gconfig.next_dungeon_uid[0] == gconfig.next_dungeon_uid[2] &&
			gconfig.next_dungeon_uid[1] == gconfig.next_dungeon_uid[3]) {
			gconfig.next_dungeon_uid[2] += UID_INC;
			if(!gconfig.next_dungeon_uid[2])
				++gconfig.next_dungeon_uid[3];
			gconfig_write();
		}
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

bool add_buf_char(BUFFER *buffer, char ch)
{
    char tmp[2];
    tmp[0] = ch;
    tmp[1] = '\0';
    return add_buf(buffer, tmp);
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
	trigger->wnum.pArea = NULL;
    trigger->wnum.vnum = 0;
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

    memset(pReset, 0, sizeof(RESET_DATA));
    pReset->command = 'X';

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

static void delete_area_region(void *ptr)
{
    free_area_region((AREA_REGION *)ptr);
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

    memset(pArea, 0, sizeof(AREA_DATA));

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
    pArea->anum             =   top_area;
    sprintf( buf, "area%ld.are", pArea->anum );
    pArea->file_name        =   str_dup( buf );
    pArea->uid              =   0;  /* Vizz - uid 0 is invalid */
    pArea->open		    =   FALSE;
    pArea->room_list = list_create(FALSE);
    pArea->comments =   &str_empty[0];
    pArea->description  =   &str_empty[0];

    pArea->points		= NULL;

    pArea->progs	    	=   new_prog_data();
    pArea->progs->progs	    =	NULL;
    pArea->progs->vars      =	NULL;
    pArea->index_vars       =	NULL;

    pArea->regions = list_createx(FALSE, NULL, delete_area_region);
    pArea->region.area = pArea;
    pArea->region.uid = 0;  // This is the only one in the area that has this.
    pArea->region.name = str_dup("default region");
    pArea->region.description = str_dup("");
    pArea->region.comments = str_dup("");
    pArea->region.x		    =   -1;
    pArea->region.y		    =   -1;
    pArea->region.land_x	    =   -1;
    pArea->region.land_y	    =   -1;
    pArea->region.rooms = list_create(FALSE);
    pArea->region.players = list_create(FALSE);
    pArea->region.valid = TRUE;
    pArea->top_region_uid = 0;  // Pre-incremented before use

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
    free_string( pArea->comments);
    free_string( pArea->description);
	list_destroy(pArea->room_list);
    list_destroy(pArea->regions);

    free_string(pArea->region.name);
    free_string(pArea->region.description);
    free_string(pArea->region.comments);
    list_destroy(pArea->region.players);
    list_destroy(pArea->region.rooms);
    pArea->region.valid = FALSE;

	for(boost = pArea->points; boost; boost = boost_next) {
		boost_next = boost->next;

		free_olc_point_boost(boost);
	}

    if(pArea->progs && pArea->progs->progs) free_prog_list(pArea->progs->progs);
    free_prog_data(pArea->progs);
    variable_clearfield(VAR_AREA, pArea);
    variable_clearfield(VAR_AREA_REGION, &pArea->region);
    variable_freelist(&pArea->index_vars);


    pArea->next         =   area_free->next;
    area_free           =   pArea;
}

AREA_REGION *area_region_free;

AREA_REGION *new_area_region()
{
    AREA_REGION *region;

    if(area_region_free)
    {
        region = area_region_free;
        area_region_free = area_region_free->next;
    }
    else
        region = alloc_mem(sizeof(AREA_REGION));

    memset(region, 0, sizeof(AREA_REGION));

    region->name = str_dup("");
    region->description = str_dup("");
    region->comments = str_dup("");
    region->players = list_create(FALSE);
    region->rooms = list_create(FALSE);

    VALIDATE(region);
    return region;
}

void free_area_region(AREA_REGION *region)
{
    if (!IS_VALID(region)) return;

    free_string(region->name);
    free_string(region->description);
    free_string(region->comments);
    list_destroy(region->players);
    list_destroy(region->rooms);

    variable_clearfield(VAR_AREA_REGION, region);

    INVALIDATE(region);
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
    pExit->door.lock.flags	= 0;
    pExit->door.lock.pick_chance	= 100;
    pExit->door.rs_lock.flags	= 0;
    pExit->door.rs_lock.pick_chance	= 100;
    pExit->keyword      =   &str_empty[0];
    pExit->short_desc   =   &str_empty[0];
    pExit->long_desc	=	&str_empty[0];
    pExit->rs_flags     =   0;

    VALIDATE(pExit);
    return pExit;
}


void free_exit( EXIT_DATA *pExit )
{
    if (!IS_VALID(pExit)) return;

    free_string( pExit->keyword );
    free_string( pExit->short_desc );
    free_string( pExit->long_desc );

    // The exit had been created by scripting, so will need to destroy the special keys lists
    // Or exit's special keys are a combined list that needs to be freed
    if (IS_SET(pExit->door.lock.flags, LOCK_CREATED) ||
        IS_SET(pExit->door.lock.flags, LOCK_FREE_KEYS))
    {
        list_destroy(pExit->door.rs_lock.special_keys);
    }

    --top_exit;

    pExit->next         =   exit_free;
    exit_free           =   pExit;

    INVALIDATE(pExit);
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

    // Flush it all
    memset(pRoom, 0, sizeof(ROOM_INDEX_DATA));

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
    pRoom->room_flag[0]       =   0;
    pRoom->room_flag[1]      =   0;
    pRoom->light            =   0;
    pRoom->sector_type      =   0;
    pRoom->heal_rate	    =   100;
    pRoom->mana_rate	    =   100;
    pRoom->visited = 0;
    pRoom->id[0] = pRoom->id[1] = 0;	// Explicitly make this 0,0 until set, or left for static rooms
    pRoom->comments         =   &str_empty[0];

    pRoom->reset_first = NULL;
    pRoom->reset_last = NULL;

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
    free_string( pRoom->comments );
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

	RESET_DATA *pNextReset;
    for ( pReset = pRoom->reset_first; pReset; pReset = pNextReset )
    {
		pNextReset = pReset->next;
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

    pRoom->events = NULL;
    pRoom->events_tail = NULL;
    variable_freelist(&pRoom->index_vars);

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

SHOP_STOCK_DATA *new_shop_stock()
{
	SHOP_STOCK_DATA *pStock;

	if( !shop_stock_free )
		pStock = alloc_perm( sizeof(*pStock) );
	else
	{
		pStock = shop_stock_free;
		shop_stock_free = shop_stock_free->next;
	}

	pStock->next = NULL;
	pStock->silver = 0;
	pStock->qp = 0;
	pStock->dp = 0;
	pStock->pneuma = 0;
	pStock->custom_price = &str_empty[0];

	pStock->wnum.pArea = NULL;
    pStock->wnum.vnum = 0;
	pStock->obj = NULL;

	pStock->quantity = 0;
	pStock->max_quantity = 0;
	pStock->restock_rate = 0;

	pStock->duration = -1;
	pStock->singular = FALSE;

	pStock->custom_keyword = &str_empty[0];
	pStock->custom_descr = &str_empty[0];

	return pStock;
}

void free_shop_stock(SHOP_STOCK_DATA *pStock)
{
	if(!pStock) return;

	free_string(pStock->custom_price);
	free_string(pStock->custom_keyword);
	free_string(pStock->custom_descr);

	pStock->next = shop_stock_free;
	shop_stock_free = pStock;
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

    memset(pShop, 0, sizeof(*pShop));

    pShop->next         =   NULL;
    pShop->keeper       =   0;

    for ( buy=0; buy<MAX_TRADE; buy++ )
        pShop->buy_type[buy]    =   0;

    pShop->profit_buy   =   100;
    pShop->profit_sell  =   100;
    pShop->open_hour    =   0;
    pShop->close_hour   =   23;
    pShop->restock_interval = 0;
    pShop->next_restock = 0;
    pShop->discount		= 50;
    pShop->shipyard_description = &str_empty[0];

    pShop->stock = NULL;

    return pShop;
}


void free_shop( SHOP_DATA *pShop )
{
	SHOP_STOCK_DATA *stock, *next_stock;

	for(stock = pShop->stock; stock; stock = next_stock)
	{
		next_stock = stock->next;
		free_shop_stock(stock);
	}

	free_string(pShop->shipyard_description);
	pShop->shipyard_description = NULL;

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
    pObj->extra[0]   =   0;
    pObj->extra[1]  =   0;
    pObj->extra[2]  =   0;
    pObj->extra[3]  =   0;
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
    pObj->comments      = &str_empty[0];
    pObj->lock			= NULL;
    pObj->waypoints		= NULL;

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
    free_string( pObj->comments );

    free_prog_list(pObj->progs);
    variable_freelist(&pObj->index_vars);

    for ( pAf = pObj->affected; pAf; pAf = pAf->next )
        free_affect( pAf );

    for ( pExtra = pObj->extra_descr; pExtra; pExtra = pExtra->next )
        free_extra_descr( pExtra );

    free_lock_state( pObj->lock );

    if( pObj->waypoints )
    {
		list_destroy(pObj->waypoints);
		pObj->waypoints = NULL;
	}

    // Item Multi-typing data
    free_container_data(CONTAINER(pObj));
    free_food_data(FOOD(pObj));
    free_light_data(LIGHT(pObj));
    free_money_data(MONEY(pObj));

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
    pMob->comments      =   &str_empty[0];
    pMob->vnum          =   0;
    pMob->count         =   0;
    pMob->killed        =   0;
    pMob->sex           =   0;
    pMob->level         =   0;
    pMob->act[0]           =   ACT_IS_NPC;
    pMob->act[1]		=   0;
    pMob->affected_by[0]   =   0;
    pMob->affected_by[1]  =   0;
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
    pMob->reputations = NULL;
    pMob->factions = list_create(FALSE);    // REPUTATION_INDEX_DATA *
    pMob->factions_load = list_createx(FALSE, NULL, delete_list_wnum_load);

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
    free_string( pMob->comments );
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
    if(pMob->pShop != NULL) free_shop( pMob->pShop );

    MOB_REPUTATION_DATA *rep, *rep_next;
    for(rep = pMob->reputations; rep; rep = rep_next)
    {
        rep_next = rep->next;
        free_mob_reputation_data(rep);
    }

    list_destroy(pMob->factions);
    list_destroy(pMob->factions_load);

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
    item->obj_wnum.auid = area->uid;
    item->obj_wnum.vnum = obj_vnum;
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
    pQuest->parts = NULL;
    pQuest->msg_complete = FALSE;
    pQuest->generating = FALSE;
    pQuest->scripted = FALSE;

    pQuest->questgiver_type = -1;
    pQuest->questgiver = wnum_zero;
    pQuest->questreceiver_type = -1;
    pQuest->questreceiver = wnum_zero;

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
    pPart->area = NULL;
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

SHIP_INDEX_DATA *new_ship_index()
{
	SHIP_INDEX_DATA *ship;

	if( ship_index_free )
	{
		ship = ship_index_free;
		ship_index_free = ship_index_free->next;
	}
	else
	{
		ship = alloc_perm(sizeof(SHIP_INDEX_DATA));
	}

	memset(ship, 0, sizeof(SHIP_INDEX_DATA));

	ship->name = &str_empty[0];
	ship->description = &str_empty[0];
	ship->hit = 1;
	ship->turning = 1;
	ship->min_crew = -1;
	ship->max_crew = -1;

	ship->special_keys = list_create(FALSE);

	return ship;
}

void free_ship_index(SHIP_INDEX_DATA *ship)
{
	free_string(ship->name);
	free_string(ship->description);

	list_destroy(ship->special_keys);

	ship->next = ship_index_free;
	ship_index_free = ship;
}

SHIP_DATA *new_ship()
{
	SHIP_DATA *ship;

	if( ship_free )
	{
		ship = ship_free;
		ship_free = ship_free->next;
	}
	else
	{
		ship = alloc_mem(sizeof(SHIP_DATA));
	}

	memset(ship, 0, sizeof(SHIP_DATA));

	ship->flag = &str_empty[0];
	ship->ship_name = &str_empty[0];
	ship->ship_name_plain = &str_empty[0];
	ship->steering.heading = -1;
	ship->sextant_x = -1;
	ship->sextant_y = -1;

	ship->crew = list_create(FALSE);
	ship->oarsmen = list_create(FALSE);

	ship->waypoints = new_waypoints_list();
	ship->route_waypoints = list_createx(FALSE, NULL, delete_waypoint);
	ship->routes = list_createx(FALSE, NULL, delete_ship_route);

	VALIDATE(ship);
	return ship;
}

void free_ship(SHIP_DATA *ship)
{
	if( !IS_VALID(ship) ) return;

	free_string(ship->flag);
	free_string(ship->ship_name);

	list_destroy(ship->crew);
	list_destroy(ship->oarsmen);

	list_destroy(ship->waypoints);
	iterator_stop(&ship->route_it);
	list_destroy(ship->route_waypoints);
	list_destroy(ship->routes);

    variable_clearfield(VAR_SHIP, ship);

	INVALIDATE(ship);
	ship->next = ship_free;
	ship_free = ship;
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

    memset(waypoint, 0, sizeof(*waypoint));

	waypoint->name = &str_empty[0];
    waypoint->x = 0;
    waypoint->y = 0;
    waypoint->next = NULL;
    VALIDATE(waypoint);

    top_waypoint++;

    return waypoint;
}


void free_waypoint( WAYPOINT_DATA *waypoint )
{
	if( !IS_VALID(waypoint) ) return;

	free_string(waypoint->name);

	INVALIDATE(waypoint);
    waypoint->next     =   waypoint_free;
    waypoint_free =   waypoint;
    top_waypoint--;
    return;
}

WAYPOINT_DATA *clone_waypoint(WAYPOINT_DATA *waypoint)
{
	if( !IS_VALID(waypoint) ) return NULL;

	WAYPOINT_DATA *wn = new_waypoint();

	free_string(wn->name);
	wn->name = str_dup(waypoint->name);

	wn->w = waypoint->w;
	wn->x = waypoint->x;
	wn->y = waypoint->y;

	return wn;
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

    memset(crew, 0, sizeof(*crew));



	VALIDATE(crew);
    return crew;
}


void free_ship_crew( SHIP_CREW_DATA *crew )
{
	if(!IS_VALID(crew)) return;


	INVALIDATE(crew);
	crew->next = ship_crew_free;
	ship_crew_free = crew;
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
	char_to_room( m_auto_war->team_players, room_index_temple );
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

    gq_mob->wnum_load.auid = 0;
    gq_mob->wnum_load.vnum = 0;
    gq_mob->class = 0;
    gq_mob->group = FALSE;
    gq_mob->obj_load.auid = 0;
    gq_mob->obj_load.vnum = 0;
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

    gq_obj->wnum_load.auid = 0;
    gq_obj->wnum_load.vnum = 0;
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

    quest_list->wnum.pArea = NULL;
    quest_list->wnum.vnum = 0;

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

    memset(spell, 0, sizeof(*spell));

    return spell;
}


void free_spell(SPELL_DATA *spell)
{
    spell->next = spell_free;
    spell_free  = spell;
}

static void *copy_spell(void *ptr)
{
    if (!ptr) return NULL;

    SPELL_DATA *src = (SPELL_DATA *)ptr;
    SPELL_DATA *data = new_spell();

    data->skill = src->skill;
    data->level = src->level;
    data->repop = src->repop;

    return data;
}

static void delete_spell(void *ptr)
{
    free_spell((SPELL_DATA *)ptr);
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

    memset(cmd, 0, sizeof(*cmd));

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

    memset(token_index, 0, sizeof(*token_index));

    token_index->name = str_dup("(no name)");
    token_index->description = &str_empty[0];
    token_index->comments   = &str_empty[0];

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
    free_string(token_index->description);
    free_string(token_index->comments);

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

    memset(token, 0, sizeof(*token));

    token->progs = NULL;
    SET_MEMTYPE(token,MEMTYPE_TOKEN);
    VALIDATE(token);

    return token;
}


void free_token(TOKEN_DATA *token)
{
	TOKEN_DATA *prev, *cur;
	EVENT_DATA *ev, *ev_next;

    free_string(token->name);
    if(token->pIndexData)	// @@@NIB : 20070127 : for "tokenexists" ifcheck
	token->pIndexData->loaded--;

    for (ev = token->events; ev != NULL; ev = ev_next) {
		ev_next = ev->next_event;

		extract_event(ev);
    }

    token->events = NULL;
    token->events_tail = NULL;

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

SCRIPT_SWITCH_CASE *new_script_switch_case(void)
{
    SCRIPT_SWITCH_CASE *data = alloc_mem(sizeof(SCRIPT_SWITCH_CASE));

    memset(data, 0, sizeof(SCRIPT_SWITCH_CASE));

    return data;
}

void free_script_switch_case(SCRIPT_SWITCH_CASE *data)
{
    if (data->next)
        free_script_switch_case(data->next);
    
    free_mem(data, sizeof(*data));
}

SCRIPT_SWITCH *new_script_switch(int nswitch)
{
    SCRIPT_SWITCH *data = alloc_mem(nswitch * sizeof(SCRIPT_SWITCH));

    memset(data, 0, nswitch * sizeof(*data));

    return data;
}

void free_script_switch(SCRIPT_SWITCH *data, int nswitch)
{
    if (data && nswitch > 0)
    {
        for(int i = 0; i < nswitch; i++)
        {
            if (data[i].cases)
                free_script_switch_case(data[i].cases);
        }

        free_mem(data, nswitch * sizeof(*data));
    }
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
    s->comments = &str_empty[0];
	s->area = NULL;
    s->n_switch_table = 0;
    s->switch_table = NULL;

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
    free_string(s->comments);
    free_script_switch(s->switch_table, s->n_switch_table);
    s->switch_table = NULL;

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
	q->keywords = &str_empty[0];
	q->short_descr = &str_empty[0];
	q->long_descr = &str_empty[0];
	q->header = &str_empty[0];
	q->footer = &str_empty[0];
	q->prefix = &str_empty[0];
	q->suffix = &str_empty[0];
	q->line_width = 70;
	q->scroll.auid = obj_wnum_quest_scroll.pArea ? obj_wnum_quest_scroll.pArea->uid : 0;
    q->scroll.vnum = obj_wnum_quest_scroll.vnum;

	return q;
}

void free_questor_data(QUESTOR_DATA *q)
{
	if(!IS_VALID(q)) return;

	free_string(q->keywords);
	free_string(q->short_descr);
	free_string(q->long_descr);
	free_string(q->header);
	free_string(q->footer);
	free_string(q->prefix);
	free_string(q->footer);

	INVALIDATE(q);
	q->next = questor_free;
	questor_free = q;

}


SCRIPT_PARAM *new_script_param()
{
	SCRIPT_PARAM *arg = alloc_mem(sizeof(SCRIPT_PARAM));

	if( arg )
	{
		arg->buffer = new_buf();
	}

	return arg;
}

void free_script_param(SCRIPT_PARAM *arg)
{
	if( arg != NULL )
	{
		free_buf(arg->buffer);
		free_mem(arg, sizeof(SCRIPT_PARAM));
	}
}

BLUEPRINT_LINK *blueprint_link_free;

BLUEPRINT_LINK *new_blueprint_link()
{
	BLUEPRINT_LINK *bl;
	if(blueprint_link_free == NULL)
		bl = alloc_perm(sizeof(BLUEPRINT_LINK));
	else {
		bl = blueprint_link_free;
		blueprint_link_free = blueprint_link_free->next;
	}

	bl->name = &str_empty[0];

	bl->vnum = 0;
	bl->door = -1;

	bl->room = NULL;
	bl->ex = NULL;

	bl->used = FALSE;

	VALIDATE(bl);
	return bl;
}

void free_blueprint_link(BLUEPRINT_LINK *bl)
{
	if(!IS_VALID(bl)) return;

	free_string(bl->name);

	INVALIDATE(bl);
	bl->next = blueprint_link_free;
	blueprint_link_free = bl;
}

MAZE_WEIGHTED_ROOM *maze_weighted_room_free;
MAZE_WEIGHTED_ROOM *new_maze_weighted_room()
{
    MAZE_WEIGHTED_ROOM *room;

    if(maze_weighted_room_free)
    {
        room = maze_weighted_room_free;
        maze_weighted_room_free = maze_weighted_room_free->next;
    }
    else
        room = alloc_mem(sizeof(MAZE_WEIGHTED_ROOM));

    memset(room, 0, sizeof(MAZE_WEIGHTED_ROOM));
    VALIDATE(room);
    return room;
}

void free_maze_weighted_room(MAZE_WEIGHTED_ROOM *room)
{
    if (IS_VALID(room))
    {
        INVALIDATE(room);

        room->next = maze_weighted_room_free;
        maze_weighted_room_free = room;
    }
}

static void delete_maze_weighted_room(void *ptr)
{
    free_maze_weighted_room((MAZE_WEIGHTED_ROOM *)ptr);
}

MAZE_FIXED_ROOM *maze_fixed_room_free;
MAZE_FIXED_ROOM *new_maze_fixed_room()
{
    MAZE_FIXED_ROOM *room;

    if(maze_fixed_room_free)
    {
        room = maze_fixed_room_free;
        maze_fixed_room_free = maze_fixed_room_free->next;
    }
    else
        room = alloc_mem(sizeof(MAZE_FIXED_ROOM));

    memset(room, 0, sizeof(MAZE_FIXED_ROOM));
    VALIDATE(room);
    return room;
}

void free_maze_fixed_room(MAZE_FIXED_ROOM *room)
{
    if (IS_VALID(room))
    {
        INVALIDATE(room);

        room->next = maze_fixed_room_free;
        maze_fixed_room_free = room;
    }
}

static void delete_maze_fixed_room(void *ptr)
{
    free_maze_fixed_room((MAZE_FIXED_ROOM *)ptr);
}

BLUEPRINT_SECTION *blueprint_section_free;

BLUEPRINT_SECTION *new_blueprint_section()
{
	BLUEPRINT_SECTION *bs;
	if(blueprint_section_free == NULL)
		bs = alloc_perm(sizeof(BLUEPRINT_SECTION));
	else {
		bs = blueprint_section_free;
		blueprint_section_free = blueprint_section_free->next;
	}

	bs->vnum = 0;

	bs->name = &str_empty[0];
	bs->description = &str_empty[0];
	bs->comments = &str_empty[0];

	bs->type = BSTYPE_STATIC;
	bs->flags = 0;

	bs->recall = 0;
	bs->lower_vnum = 0;
	bs->upper_vnum = 0;

	bs->links = NULL;

    bs->maze_x = 0;
    bs->maze_y = 0;
    bs->maze_templates = list_createx(FALSE, NULL, delete_maze_weighted_room);
    bs->maze_fixed_rooms = list_createx(FALSE, NULL, delete_maze_fixed_room);

	VALIDATE(bs);
	return bs;
}

void free_blueprint_section(BLUEPRINT_SECTION *bs)
{
	if(!IS_VALID(bs)) return;

	free_string(bs->name);

	BLUEPRINT_LINK *blc, *bln;
	for(blc = bs->links; blc; blc = bln)
	{
		bln = blc->next;
		free_blueprint_link(blc);
	}

	INVALIDATE(bs);
	bs->next = blueprint_section_free;
	blueprint_section_free = bs;
}


STATIC_BLUEPRINT_LINK *static_blueprint_link_free;

STATIC_BLUEPRINT_LINK *new_static_blueprint_link()
{
	STATIC_BLUEPRINT_LINK *bl;
	if(static_blueprint_link_free == NULL)
		bl = alloc_perm(sizeof(STATIC_BLUEPRINT_LINK));
	else {
		bl = static_blueprint_link_free;
		static_blueprint_link_free = static_blueprint_link_free->next;
	}

	bl->blueprint = NULL;

	bl->section1 = -1;
	bl->link1 = -1;

	bl->section2 = -1;
	bl->link2 = -1;

	VALIDATE(bl);
	return bl;
}

void free_static_blueprint_link(STATIC_BLUEPRINT_LINK *bl)
{
	if(!IS_VALID(bl)) return;

	INVALIDATE(bl);
	bl->next = static_blueprint_link_free;
	static_blueprint_link_free = bl;
}


BLUEPRINT_SPECIAL_ROOM *blueprint_special_room_free;

BLUEPRINT_SPECIAL_ROOM *new_blueprint_special_room()
{
	BLUEPRINT_SPECIAL_ROOM *special;

	if( blueprint_special_room_free )
	{
		special = blueprint_special_room_free;
		blueprint_special_room_free = blueprint_special_room_free->next;
	}
	else
	{
		special = alloc_perm(sizeof(BLUEPRINT_SPECIAL_ROOM));
	}
	memset(special, 0, sizeof(BLUEPRINT_SPECIAL_ROOM));

	special->name = &str_empty[0];

	VALIDATE(special);
	return special;
}

void free_blueprint_special_room(BLUEPRINT_SPECIAL_ROOM *special)
{
	if( !IS_VALID(special) ) return;

	free_string(special->name);

	INVALIDATE(special);
	special->next = blueprint_special_room_free;
	blueprint_special_room_free = special;
}

static void delete_blueprint_special_room(void *data) {
	free_blueprint_special_room((BLUEPRINT_SPECIAL_ROOM *)data);
}

BLUEPRINT_EXIT_DATA *new_blueprint_exit_data()
{
    BLUEPRINT_EXIT_DATA *ex = alloc_mem(sizeof(BLUEPRINT_EXIT_DATA));

    ex->name = &str_empty[0];
    ex->section = -1;
    ex->link = -1;

    return ex;
}

void free_blueprint_exit_data(BLUEPRINT_EXIT_DATA *ex)
{
    free_string(ex->name);
    free_mem(ex, sizeof(BLUEPRINT_EXIT_DATA));
}

static void delete_blueprint_exit_data(void *data)
{
    free_blueprint_exit_data((BLUEPRINT_EXIT_DATA *)data);
}

BLUEPRINT_WEIGHTED_SECTION_DATA *new_weighted_random_section()
{
    BLUEPRINT_WEIGHTED_SECTION_DATA *weighted = alloc_mem(sizeof(BLUEPRINT_WEIGHTED_SECTION_DATA));

    if (weighted)
    {
        weighted->weight = 0;
        weighted->section = 0;
    }

    return weighted;
}

void free_weighted_random_section(BLUEPRINT_WEIGHTED_SECTION_DATA *weighted)
{
    free_mem(weighted, sizeof(BLUEPRINT_WEIGHTED_SECTION_DATA));
}

static void delete_weighted_random_section_data(void *data)
{
    free_weighted_random_section((BLUEPRINT_WEIGHTED_SECTION_DATA *)data);
}

BLUEPRINT_WEIGHTED_LINK_DATA *new_weighted_random_link()
{
    BLUEPRINT_WEIGHTED_LINK_DATA *weighted = alloc_mem(sizeof(BLUEPRINT_WEIGHTED_LINK_DATA));

    if (weighted)
    {
        weighted->weight = 0;
        weighted->section = 0;
        weighted->link = 0;
    }

    return weighted;
}

void free_weighted_random_link(BLUEPRINT_WEIGHTED_LINK_DATA *weighted)
{
    free_mem(weighted, sizeof(BLUEPRINT_WEIGHTED_LINK_DATA));
}

static void delete_weighted_random_link_data(void *data)
{
    free_weighted_random_link((BLUEPRINT_WEIGHTED_LINK_DATA *)data);
}


static void delete_blueprint_layout_section_data(void *data)
{
    free_blueprint_layout_section_data((BLUEPRINT_LAYOUT_SECTION_DATA *)data);
}

BLUEPRINT_LAYOUT_SECTION_DATA *blueprint_layout_section_free;
BLUEPRINT_LAYOUT_SECTION_DATA *new_blueprint_layout_section_data()
{
    BLUEPRINT_LAYOUT_SECTION_DATA *data;

    if (blueprint_layout_section_free)
    {
        data = blueprint_layout_section_free;
        blueprint_layout_section_free = blueprint_layout_section_free->next;
    }
    else
    {
        data = alloc_mem(sizeof(BLUEPRINT_LAYOUT_SECTION_DATA));
    }
    memset(data, 0, sizeof(BLUEPRINT_LAYOUT_SECTION_DATA));

    data->weighted_sections = list_createx(FALSE, NULL, delete_weighted_random_section_data);
    data->group = list_createx(FALSE, NULL, delete_blueprint_layout_section_data);

    VALIDATE(data);
    return data;
}

void free_blueprint_layout_section_data(BLUEPRINT_LAYOUT_SECTION_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->weighted_sections);
    list_destroy(data->group);

    INVALIDATE(data);
}



static void delete_blueprint_layout_link_data(void *data)
{
    free_blueprint_layout_link_data((BLUEPRINT_LAYOUT_LINK_DATA *)data);
}

BLUEPRINT_LAYOUT_LINK_DATA *blueprint_layout_link_free;
BLUEPRINT_LAYOUT_LINK_DATA *new_blueprint_layout_link_data()
{
    BLUEPRINT_LAYOUT_LINK_DATA *data;

    if (blueprint_layout_link_free)
    {
        data = blueprint_layout_link_free;
        blueprint_layout_link_free = blueprint_layout_link_free->next;
    }
    else
        data = alloc_mem(sizeof(BLUEPRINT_LAYOUT_LINK_DATA));

    memset(data, 0, sizeof(BLUEPRINT_LAYOUT_LINK_DATA));

    data->from = list_createx(FALSE, NULL, delete_weighted_random_link_data);
    data->to = list_createx(FALSE, NULL, delete_weighted_random_link_data);
    data->group = list_createx(FALSE, NULL, delete_blueprint_layout_link_data);

    VALIDATE(data);
    return data;
}

void free_blueprint_layout_link_data(BLUEPRINT_LAYOUT_LINK_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->from);
    list_destroy(data->to);
    list_destroy(data->group);

    data->next = blueprint_layout_link_free;
    blueprint_layout_link_free = data;
    INVALIDATE(data);
}

BLUEPRINT *blueprint_free;
BLUEPRINT *new_blueprint()
{
	BLUEPRINT *bp;
	if(blueprint_free == NULL)
		bp = alloc_perm(sizeof(BLUEPRINT));
	else {
		bp = blueprint_free;
		blueprint_free = blueprint_free->next;
	}

	bp->vnum = 0;

	bp->name = &str_empty[0];
	bp->description = &str_empty[0];
	bp->comments = &str_empty[0];

	bp->area_who = AREA_INSTANCE;

	bp->sections = list_create(FALSE);
	bp->special_rooms = list_createx(FALSE, NULL, delete_blueprint_special_room);

    bp->layout = list_createx(FALSE, NULL, delete_blueprint_layout_section_data);
    bp->links = list_createx(FALSE, NULL, delete_blueprint_layout_link_data);
    bp->recall = 0;         // 0 = invalid, >0 = positional index, <0 = ordinal index

    bp->entrances = list_createx(FALSE, NULL, delete_blueprint_exit_data);
    bp->exits = list_createx(FALSE, NULL, delete_blueprint_exit_data);

	VALIDATE(bp);
	return bp;
}

void free_blueprint(BLUEPRINT *bp)
{
	if(!IS_VALID(bp)) return;

	free_string(bp->name);
	free_string(bp->description);

	list_destroy(bp->sections);
	list_destroy(bp->special_rooms);

    list_destroy(bp->layout);
    list_destroy(bp->links);
    list_destroy(bp->entrances);
    list_destroy(bp->exits);

    free_prog_list(bp->progs);
    variable_freelist(&bp->index_vars);

	INVALIDATE(bp);
	bp->next = blueprint_free;
	blueprint_free = bp;
}

INSTANCE_SECTION *instance_section_free;

INSTANCE_SECTION *new_instance_section()
{
	INSTANCE_SECTION *section;

	if( instance_section_free )
	{
		section = instance_section_free;
		instance_section_free = instance_section_free->next;
	}
	else
		section = alloc_perm(sizeof(INSTANCE_SECTION));

	section->rooms = list_create(FALSE);

	VALIDATE(section);
	return section;
}

void free_instance_section(INSTANCE_SECTION *section)
{
	if(!IS_VALID(section)) return;

	ITERATOR rit;
	ROOM_INDEX_DATA *room;
	iterator_start(&rit,section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		extract_clone_room(room->source,room->id[0],room->id[1],true);
	}
	iterator_stop(&rit);

	list_destroy(section->rooms);

    variable_clearfield(VAR_SECTION, section);

	INVALIDATE(section);
	section->next = instance_section_free;
	instance_section_free = section;
}

NAMED_SPECIAL_ROOM *named_special_room_free;

NAMED_SPECIAL_ROOM *new_named_special_room()
{
	NAMED_SPECIAL_ROOM *special;

	if( named_special_room_free )
	{
		special = named_special_room_free;
		named_special_room_free = named_special_room_free->next;
	}
	else
	{
		special = alloc_perm(sizeof(NAMED_SPECIAL_ROOM));
	}
	memset(special, 0, sizeof(NAMED_SPECIAL_ROOM));

	special->name = &str_empty[0];

	VALIDATE(special);
	return special;
}

void free_named_special_room(NAMED_SPECIAL_ROOM *special)
{
	if( !IS_VALID(special) ) return;

	free_string(special->name);

	INVALIDATE(special);
	special->next = named_special_room_free;
	named_special_room_free = special;
}

static void delete_named_special_room(void *data) {
	free_named_special_room((NAMED_SPECIAL_ROOM *)data);
}

NAMED_SPECIAL_EXIT *named_special_exit_free;

NAMED_SPECIAL_EXIT *new_named_special_exit()
{
	NAMED_SPECIAL_EXIT *special;

	if( named_special_exit_free )
	{
		special = named_special_exit_free;
		named_special_exit_free = named_special_exit_free->next;
	}
	else
	{
		special = alloc_perm(sizeof(NAMED_SPECIAL_EXIT));
	}
	memset(special, 0, sizeof(NAMED_SPECIAL_EXIT));

	special->name = &str_empty[0];

	VALIDATE(special);
	return special;
}

void free_named_special_exit(NAMED_SPECIAL_EXIT *special)
{
	if( !IS_VALID(special) ) return;

	free_string(special->name);

	INVALIDATE(special);
	special->next = named_special_exit_free;
	named_special_exit_free = special;
}

static void delete_named_special_exit(void *data) {
	free_named_special_exit((NAMED_SPECIAL_EXIT *)data);
}


INSTANCE *instance_free;

INSTANCE *new_instance()
{
	INSTANCE *instance;

	if( instance_free )
	{
		instance = instance_free;
		instance_free = instance_free->next;
	}
	else
		instance = alloc_perm(sizeof(INSTANCE));

	memset(instance, 0, sizeof(INSTANCE));

	instance->blueprint = NULL;

	instance->recall = NULL;

	instance->dungeon = NULL;

	instance->object = NULL;
	instance->object_uid[0] = 0;
	instance->object_uid[1] = 0;

	instance->sections = list_create(FALSE);
	instance->players = list_create(FALSE);
	instance->mobiles = list_create(FALSE);
	instance->objects = list_create(FALSE);
	instance->rooms = list_create(FALSE);
	instance->bosses = list_create(FALSE);
	instance->special_rooms = list_createx(FALSE, NULL, delete_named_special_room);
    instance->special_exits = list_createx(FALSE, NULL, delete_named_special_exit);
	instance->player_owners = list_createx(FALSE, NULL, delete_list_uid_data);

	VALIDATE(instance);
	return instance;
}

void free_instance(INSTANCE *instance)
{
	if(!IS_VALID(instance)) return;

	ITERATOR it;
	INSTANCE_SECTION *section;
	iterator_start(&it, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&it)) )
	{
		free_instance_section(section);
	}
	iterator_stop(&it);

	list_destroy(instance->sections);
	list_destroy(instance->players);
	list_destroy(instance->mobiles);
	list_destroy(instance->objects);
	list_destroy(instance->rooms);
	list_destroy(instance->bosses);
	list_destroy(instance->special_rooms);
    list_destroy(instance->special_exits);
	list_destroy(instance->player_owners);

    variable_clearfield(VAR_INSTANCE, instance);
    script_clear_instance(instance);
    free_prog_data(instance->progs);

	INVALIDATE(instance);
	instance->next = instance_free;
	instance_free = instance;
}

DUNGEON_INDEX_SPECIAL_ROOM *dungeon_index_special_room_free;
DUNGEON_INDEX_SPECIAL_ROOM *new_dungeon_index_special_room()
{
	DUNGEON_INDEX_SPECIAL_ROOM *special;

	if( dungeon_index_special_room_free )
	{
		special = dungeon_index_special_room_free;
		dungeon_index_special_room_free = dungeon_index_special_room_free->next;
	}
	else
	{
		special = alloc_perm(sizeof(DUNGEON_INDEX_SPECIAL_ROOM));
	}

	memset(special, 0, sizeof(DUNGEON_INDEX_SPECIAL_ROOM));
	special->name = &str_empty[0];

	VALIDATE(special);
	return special;
}

void free_dungeon_index_special_room(DUNGEON_INDEX_SPECIAL_ROOM *special)
{
	if( !IS_VALID(special) ) return;

	free_string(special->name);

	INVALIDATE(special);
	special->next = dungeon_index_special_room_free;
	dungeon_index_special_room_free = special;
}


static void delete_weighted_random_exit_data(void *data)
{
    free_weighted_random_exit((DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)data);
}


static void delete_dungeon_index_special_room(void *data) {
	free_dungeon_index_special_room((DUNGEON_INDEX_SPECIAL_ROOM *)data);
}

static void delete_dungeon_index_special_exit(void *data)
{
    free_dungeon_index_special_exit((DUNGEON_INDEX_SPECIAL_EXIT *)data);
}

DUNGEON_INDEX_SPECIAL_EXIT *dungeon_index_special_exit_free;
DUNGEON_INDEX_SPECIAL_EXIT *new_dungeon_index_special_exit()
{
	DUNGEON_INDEX_SPECIAL_EXIT *special;

	if( dungeon_index_special_exit_free )
	{
		special = dungeon_index_special_exit_free;
		dungeon_index_special_exit_free = dungeon_index_special_exit_free->next;
	}
	else
	{
		special = alloc_perm(sizeof(DUNGEON_INDEX_SPECIAL_EXIT));
	}

	memset(special, 0, sizeof(DUNGEON_INDEX_SPECIAL_EXIT));
	special->name = &str_empty[0];
    special->from = list_createx(FALSE, NULL, delete_weighted_random_exit_data);
    special->to = list_createx(FALSE, NULL, delete_weighted_random_exit_data);
    special->group = list_createx(FALSE, NULL, delete_dungeon_index_special_exit);

	VALIDATE(special);
	return special;
}

void free_dungeon_index_special_exit(DUNGEON_INDEX_SPECIAL_EXIT *special)
{
	if( !IS_VALID(special) ) return;

	free_string(special->name);

	INVALIDATE(special);
	special->next = dungeon_index_special_exit_free;
	dungeon_index_special_exit_free = special;
}


DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *new_weighted_random_floor()
{
    DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = alloc_mem(sizeof(DUNGEON_INDEX_WEIGHTED_FLOOR_DATA));

    if (weighted)
    {
        weighted->weight = 0;
        weighted->floor = 0;
    }

    return weighted;
}

void free_weighted_random_floor(DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted)
{
    free_mem(weighted, sizeof(DUNGEON_INDEX_WEIGHTED_FLOOR_DATA));
}

static void delete_weighted_random_floor_data(void *data)
{
    free_weighted_random_floor((DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)data);
}

DUNGEON_INDEX_WEIGHTED_EXIT_DATA *new_weighted_random_exit()
{
    DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted = alloc_mem(sizeof(DUNGEON_INDEX_WEIGHTED_EXIT_DATA));

    if (weighted)
    {
        weighted->weight = 0;
        weighted->door = 0;         // Not to be confused with the exit[door] on ROOM_INDEX_DATA, this is 1-based.
    }

    return weighted;
}

void free_weighted_random_exit(DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted)
{
    free_mem(weighted, sizeof(DUNGEON_INDEX_WEIGHTED_EXIT_DATA));
}


static void delete_dungeon_index_level(void *data) {
    free_dungeon_index_level((DUNGEON_INDEX_LEVEL_DATA *)data);    
}

DUNGEON_INDEX_LEVEL_DATA *dungeon_index_level_free;
DUNGEON_INDEX_LEVEL_DATA *new_dungeon_index_level()
{
    DUNGEON_INDEX_LEVEL_DATA *dungeon_level;

    if (dungeon_index_level_free)
    {
        dungeon_level = dungeon_index_level_free;
        dungeon_index_level_free = dungeon_index_level_free->next;
    }
    else
        dungeon_level = alloc_perm(sizeof(DUNGEON_INDEX_LEVEL_DATA));

    memset(dungeon_level, 0, sizeof(DUNGEON_INDEX_LEVEL_DATA));

    dungeon_level->mode = LEVELMODE_STATIC;
    dungeon_level->floor = 0;
    dungeon_level->weighted_floors = list_createx(FALSE, NULL, delete_weighted_random_floor_data);
    dungeon_level->total_weight = 0;
    dungeon_level->group = list_createx(FALSE, NULL, delete_dungeon_index_level);

    VALIDATE(dungeon_level);

    return dungeon_level;
}

void free_dungeon_index_level(DUNGEON_INDEX_LEVEL_DATA *dungeon_level)
{
    if (!IS_VALID(dungeon_level)) return;

    dungeon_level->next = dungeon_index_level_free;
    dungeon_index_level_free = dungeon_level;

    list_destroy(dungeon_level->weighted_floors);

    INVALIDATE(dungeon_level);
}

DUNGEON_INDEX_DATA *dungeon_index_free;
DUNGEON_INDEX_DATA *new_dungeon_index()
{
	DUNGEON_INDEX_DATA *dungeon_index;

	if( dungeon_index_free )
	{
		dungeon_index = dungeon_index_free;
		dungeon_index_free = dungeon_index_free->next;
	}
	else
		dungeon_index = alloc_perm(sizeof(DUNGEON_INDEX_DATA));

	memset(dungeon_index, 0, sizeof(DUNGEON_INDEX_DATA));

	dungeon_index->vnum = 0;

	dungeon_index->name = &str_empty[0];
	dungeon_index->description = &str_empty[0];
	dungeon_index->comments = &str_empty[0];

	dungeon_index->area_who = AREA_DUNGEON;
    dungeon_index->min_group = 1;
    dungeon_index->max_group = 1;
    dungeon_index->max_players = 1;

    if (fBootDb)
    	dungeon_index->floors = list_createx(FALSE, NULL, delete_list_wnum_load);
    else
    	dungeon_index->floors = list_create(FALSE);

    dungeon_index->levels = list_createx(FALSE, NULL, delete_dungeon_index_level);
	dungeon_index->special_rooms = list_createx(FALSE, NULL, delete_dungeon_index_special_room);
    dungeon_index->special_exits = list_createx(FALSE, NULL, delete_dungeon_index_special_exit);
	dungeon_index->entry_room = 0;
	dungeon_index->exit_room = 0;

	dungeon_index->flags = 0;

	dungeon_index->zone_out = &str_empty[0];
	dungeon_index->zone_out_portal = &str_empty[0];
	dungeon_index->zone_out_mount = &str_empty[0];

    dungeon_index->loaded = list_create(FALSE); // Will contain (DUNGEON *)

	VALIDATE(dungeon_index);
	return dungeon_index;
}

void free_dungeon_index(DUNGEON_INDEX_DATA *dungeon_index)
{
	if( !IS_VALID(dungeon_index) )
		return;

	free_string(dungeon_index->name);
	free_string(dungeon_index->description);
	free_string(dungeon_index->comments);

	list_destroy(dungeon_index->floors);
	list_destroy(dungeon_index->special_rooms);

	free_string(dungeon_index->zone_out);
	free_string(dungeon_index->zone_out_portal);
	free_string(dungeon_index->zone_out_mount);

    list_destroy(dungeon_index->loaded);

    free_prog_list(dungeon_index->progs);
    variable_freelist(&dungeon_index->index_vars);

	INVALIDATE(dungeon_index);
	dungeon_index->next = dungeon_index_free;
	dungeon_index_free = dungeon_index;
}


DUNGEON *dungeon_free;
DUNGEON *new_dungeon()
{
	DUNGEON *dng;

	if( dungeon_free )
	{
		dng = dungeon_free;
		dungeon_free = dungeon_free->next;
	}
	else
		dng = alloc_perm(sizeof(DUNGEON));

	memset(dng, 0, sizeof(DUNGEON));

	dng->empty = FALSE;
	dng->floors = list_create(FALSE);
	dng->players = list_create(FALSE);
	dng->mobiles = list_create(FALSE);
	dng->objects = list_create(FALSE);
	dng->rooms = list_create(FALSE);
	dng->bosses = list_create(FALSE);
	dng->special_rooms = list_createx(FALSE, NULL, delete_named_special_room);
    dng->special_exits = list_createx(FALSE, NULL, delete_named_special_exit);
	dng->player_owners = list_createx(FALSE, NULL, delete_list_uid_data);

	VALIDATE(dng);
	return dng;
}

void free_dungeon(DUNGEON *dng)
{
	if( !IS_VALID(dng) ) return;

	ITERATOR fit;
	INSTANCE *floor;
	iterator_start(&fit, dng->floors);
	while( (floor = (INSTANCE *)iterator_nextdata(&fit)) )
	{
		free_instance(floor);
	}
	iterator_stop(&fit);

	list_destroy(dng->floors);
	list_destroy(dng->players);
	list_destroy(dng->mobiles);
	list_destroy(dng->objects);
	list_destroy(dng->rooms);
	list_destroy(dng->bosses);
	list_destroy(dng->special_rooms);
    list_destroy(dng->special_exits);
	list_destroy(dng->player_owners);

    variable_clearfield(VAR_DUNGEON, dng);
    script_clear_dungeon(dng);
    free_prog_data(dng->progs);

    // Automatically remove it from the list
    if (dng->index)
        list_remlink(dng->index->loaded, dng);

	INVALIDATE(dng);
	dng->next = dungeon_free;
	dungeon_free = dng;
}


SPECIAL_KEY_DATA *special_key_free;
SPECIAL_KEY_DATA *new_special_key()
{
	SPECIAL_KEY_DATA *sk;

	if( special_key_free )
	{
		sk = special_key_free;
		special_key_free = special_key_free->next;
	}
	else
	{
		sk = alloc_mem(sizeof(SPECIAL_KEY_DATA));
	}

	memset(sk, 0, sizeof(SPECIAL_KEY_DATA));

	sk->list = list_createx(FALSE, NULL, delete_list_uid_data);

	VALIDATE(sk);

	list_appendlink(loaded_special_keys, sk);
	return sk;
}

void free_special_key(SPECIAL_KEY_DATA *sk)
{
	if( !IS_VALID(sk) ) return;

	list_remlink(loaded_special_keys, sk);

	list_destroy(sk->list);

	INVALIDATE(sk);
	sk->next = special_key_free;
	special_key_free = sk;
}

LOCK_STATE_KEY *new_lock_state_key()
{
    LOCK_STATE_KEY *data = alloc_mem(sizeof(LOCK_STATE_KEY));

    memset(data, 0, sizeof(*data));

    return data;
}

/*
static void *copy_lock_state_key(void *ptr)
{
    LOCK_STATE_KEY *src = (LOCK_STATE_KEY *)ptr;
    LOCK_STATE_KEY *data = alloc_mem(sizeof(LOCK_STATE_KEY));

    data->load = src->load;
    data->wnum = src->wnum;

    return data;
}
*/

void free_lock_state_key(LOCK_STATE_KEY *data)
{
    free_mem(data, sizeof(LOCK_STATE_KEY));
}

/*
static void delete_lock_state_key(void *ptr)
{
    free_lock_state_key((LOCK_STATE_KEY *)ptr);
}
*/

static void *copy_lockstate_special_key(void *ptr)
{
    return copy_list_uid_data(ptr);
}

static void delete_lockstate_special_key(void *ptr)
{
    free_list_uid_data((LLIST_UID_DATA *)ptr);
}

LOCK_STATE *new_lock_state()
{
	LOCK_STATE *state = alloc_mem(sizeof(LOCK_STATE));

    memset(state, 0, sizeof(LOCK_STATE));

    //state->keys = list_createx(FALSE, copy_lock_state_key, delete_lock_state_key);
	state->pick_chance	= 100;
	state->flags		= 0;
	state->special_keys	= list_createx(FALSE, copy_lockstate_special_key, delete_lockstate_special_key);

	return state;
}

LOCK_STATE *copy_lock_state(LOCK_STATE *src)
{
    if (!src) return NULL;
    
    LOCK_STATE *state = alloc_mem(sizeof(LOCK_STATE));

    //state->keys = list_copy(src->keys);
    state->key_load = src->key_load;
    state->key_wnum = src->key_wnum;
    state->pick_chance = src->pick_chance;
    state->flags = src->flags;
    state->special_keys = list_copy(src->special_keys);

    return state;
}

void free_lock_state(LOCK_STATE *state)
{
	if( state )
	{
        //list_destroy(state->keys);

        if (state->special_keys)
            list_destroy(state->special_keys);

		free_mem(state, sizeof(LOCK_STATE));
	}
}

SHIP_ROUTE *ship_route_free;

SHIP_ROUTE *new_ship_route()
{
	SHIP_ROUTE *route;

	if( ship_route_free )
	{
		route = ship_route_free;
		ship_route_free = ship_route_free->next;
	}
	else
		route = alloc_mem(sizeof(SHIP_ROUTE));

	memset(route, 0, sizeof(SHIP_ROUTE));

	route->name = &str_empty[0];
	route->waypoints = list_create(FALSE);

	VALIDATE(route);
	return route;
}

void free_ship_route(SHIP_ROUTE *route)
{
	if( !IS_VALID(route) ) return;

	free_string(route->name);
	list_destroy(route->waypoints);

	INVALIDATE(route);
	route->next = ship_route_free;
	ship_route_free = route;
}

SHIP_CREW_INDEX_DATA *ship_crew_index_free;

SHIP_CREW_INDEX_DATA *new_ship_crew_index()
{
	SHIP_CREW_INDEX_DATA *crew;

	if( ship_crew_index_free )
	{
		crew = ship_crew_index_free;
		ship_crew_index_free = ship_crew_index_free->next;
	}
	else
		crew = alloc_mem(sizeof(*crew));

	memset(crew, 0, sizeof(*crew));

	VALIDATE(crew);
	return crew;
}

void free_ship_crew_index(SHIP_CREW_INDEX_DATA *crew)
{
	if( !IS_VALID(crew) ) return;

	INVALIDATE(crew);
	crew->next = ship_crew_index_free;
	ship_crew_index_free = crew;

}

AURA_DATA *aura_data_free;

AURA_DATA *new_aura_data()
{
    AURA_DATA *aura;

    if (aura_data_free)
    {
        aura = aura_data_free;
        aura_data_free = aura_data_free->next;
    }
    else
        aura = alloc_mem(sizeof(AURA_DATA));

    aura->next = NULL;
    aura->name = str_dup("");
    aura->long_descr = str_dup("");

    VALIDATE(aura);
    return aura;
}

void free_aura_data(AURA_DATA *aura)
{
    if (!IS_VALID(aura)) return;

    free_string(aura->name);
    free_string(aura->long_descr);

    aura->next = aura_data_free;
    aura_data_free = aura;
    INVALIDATE(aura);
}


// Item Multi-Typing

// ============[ AMMO ]============
AMMO_DATA *ammo_data_free;
AMMO_DATA *new_ammo_data()
{
    AMMO_DATA *data;
    if (ammo_data_free)
    {
        data = ammo_data_free;
        ammo_data_free = ammo_data_free->next;
    }
    else
        data = alloc_mem(sizeof(AMMO_DATA));
    memset(data, 0, sizeof(*data));

    data->type = AMMO_NONE;
    data->msg_break = &str_empty[0];

    VALIDATE(data);
    return data;
}

AMMO_DATA *copy_ammo_data(AMMO_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    AMMO_DATA *data;
    if (ammo_data_free)
    {
        data = ammo_data_free;
        ammo_data_free = ammo_data_free->next;
    }
    else
        data = alloc_mem(sizeof(AMMO_DATA));
    memset(data, 0, sizeof(*data));

    data->type = src->type;
    data->damage_type = src->damage_type;
    data->flags = src->flags;
    data->damage = src->damage;
    data->damage.last_roll = 0;

    data->msg_break = str_dup(src->msg_break);

    VALIDATE(data);
    return data;
}

void free_ammo_data(AMMO_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->msg_break);

    INVALIDATE(data);
    data->next = ammo_data_free;
    ammo_data_free = data;
}

// ============[ BOOK ]============
BOOK_PAGE *book_page_free;
BOOK_PAGE *new_book_page()
{
    BOOK_PAGE *page;

    if (book_page_free)
    {
        page = book_page_free;
        book_page_free = book_page_free->next;
    }
    else
        page = alloc_mem(sizeof(BOOK_PAGE));
    
    memset(page, 0, sizeof(*page));

    page->page_no = 0;
    page->title = str_dup("");
    page->text = str_dup("");

    VALIDATE(page);
    return page;
}

BOOK_PAGE *copy_book_page(BOOK_PAGE *src)
{
    if (!IS_VALID(src)) return NULL;

    BOOK_PAGE *data = alloc_mem(sizeof(BOOK_PAGE));

    data->page_no = src->page_no;
    data->title = str_dup(src->title);
    data->text = str_dup(src->text);

    VALIDATE(data);
    return data;
}

void *_copy_book_page(void *ptr)
{
    return copy_book_page((BOOK_PAGE *)ptr);
}

void free_book_page(BOOK_PAGE *page)
{
    if (!IS_VALID(page)) return;

    free_string(page->title);
    free_string(page->text);

    INVALIDATE(page);
}

static void delete_book_page(void *ptr)
{
    free_book_page((BOOK_PAGE *)ptr);
}

BOOK_DATA *book_data_free;
BOOK_DATA *new_book_data()
{
    BOOK_DATA *data;

    if (book_data_free)
    {
        data = book_data_free;
        book_data_free = book_data_free->next;
    }
    else
        data = alloc_mem(sizeof(BOOK_DATA));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->short_descr = str_dup("");
    data->pages = list_createx(FALSE, _copy_book_page, delete_book_page);

    VALIDATE(data);
    return data;
}

BOOK_DATA *copy_book_data(BOOK_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    BOOK_DATA *data = alloc_mem(sizeof(BOOK_DATA));

    data->name = str_dup(src->name);
    data->short_descr = str_dup(src->short_descr);

    data->flags = src->flags;

    data->current_page = src->current_page;
    data->pages = list_copy(src->pages);

    data->open_page = src->open_page;

    data->lock = copy_lock_state(src->lock);

    VALIDATE(data);
    return data;
}

void free_book_data(BOOK_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->short_descr);

    list_destroy(data->pages);

    free_lock_state(data->lock);

    INVALIDATE(data);
}


// =========[ CONTAINER ]==========
CONTAINER_FILTER *new_container_filter()
{
    CONTAINER_FILTER *filter = alloc_mem(sizeof(CONTAINER_FILTER));

    memset(filter, 0, sizeof(*filter));

    return filter;
}

void free_container_filter(CONTAINER_FILTER *filter)
{
    free_mem(filter, sizeof(*filter));
}

static void *copy_container_filter(void *ptr)
{
    CONTAINER_FILTER *src = (CONTAINER_FILTER *)ptr;
    CONTAINER_FILTER *cpy = new_container_filter();

    if (cpy)
    {
        cpy->item_type = src->item_type;
        cpy->sub_type = src->sub_type;
    }

    return cpy;
}

static void delete_container_filter(void *ptr)
{
    free_container_filter((CONTAINER_FILTER *)ptr);
}

CONTAINER_DATA *container_data_free;

CONTAINER_DATA *new_container_data()
{
    CONTAINER_DATA *data;
    if (container_data_free)
    {
        data = container_data_free;
        container_data_free = container_data_free->next;
    }
    else
        data = alloc_mem(sizeof(CONTAINER_DATA));

    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->short_descr = str_dup("");
    data->whitelist = list_createx(FALSE, copy_container_filter, delete_container_filter);   // Filled with positive INTs
    data->blacklist = list_createx(FALSE, copy_container_filter, delete_container_filter);   // Filled with positive INTs

    VALIDATE(data);
    return data;
}

CONTAINER_DATA *copy_container_data(CONTAINER_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    CONTAINER_DATA *data;
    if (container_data_free)
    {
        data = container_data_free;
        container_data_free = container_data_free->next;
    }
    else
        data = alloc_mem(sizeof(CONTAINER_DATA));

    data->name = str_dup(src->name);
    data->short_descr = str_dup(src->short_descr);
    data->flags = src->flags;
    data->max_weight = src->max_weight;
    data->weight_multiplier = src->weight_multiplier;
    data->max_volume = src->max_volume;

    data->whitelist = list_copy(src->whitelist);
    data->blacklist = list_copy(src->blacklist);

    data->lock = copy_lock_state(src->lock);

    VALIDATE(data);
    return data;
}

void free_container_data(CONTAINER_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->short_descr);
    list_destroy(data->whitelist);
    list_destroy(data->blacklist);
    free_lock_state(data->lock);

    INVALIDATE(data);
    data->next = container_data_free;
    container_data_free = data;
}

// ======[ FLUID CONTAINER ]=======

LIQUID *liquid_free;
LIQUID *new_liquid()
{
    LIQUID *liq;
    if (liquid_free)
    {
        liq = liquid_free;
        liquid_free = liquid_free->next;
    }
    else
        liq = alloc_mem(sizeof(LIQUID));

    memset(liq, 0, sizeof(*liq));

    liq->name = str_dup("");
    liq->color = str_dup("");

    VALIDATE(liq);
    return liq;
}

void free_liquid(LIQUID *liq)
{
    if (!IS_VALID(liq)) return;

    free_string(liq->name);
    free_string(liq->color);

    INVALIDATE(liq);
    liq->next = liquid_free;
    liquid_free = liq;
}

FLUID_CONTAINER_DATA *fluid_container_free;
FLUID_CONTAINER_DATA *new_fluid_container_data()
{
    FLUID_CONTAINER_DATA *data;
    if (fluid_container_free)
    {
        data = fluid_container_free;
        fluid_container_free = fluid_container_free->next;
    }
    else
        data = alloc_mem(sizeof(FLUID_CONTAINER_DATA));

    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->short_descr = str_dup("");
    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

FLUID_CONTAINER_DATA *copy_fluid_container_data(FLUID_CONTAINER_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FLUID_CONTAINER_DATA *data;
    if (fluid_container_free)
    {
        data = fluid_container_free;
        fluid_container_free = fluid_container_free->next;
    }
    else
        data = alloc_mem(sizeof(FLUID_CONTAINER_DATA));

    data->name = str_dup(src->name);
    data->short_descr = str_dup(src->short_descr);
    data->flags = src->flags;

    data->liquid = src->liquid;
    data->capacity = src->capacity;
    data->amount = src->amount;
    data->refill_rate = src->refill_rate;
    data->poison = src->poison;
    data->poison_rate = src->poison_rate;

    data->lock = copy_lock_state(src->lock);
    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_fluid_container_data(FLUID_CONTAINER_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->short_descr);

    free_lock_state(data->lock);
    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = fluid_container_free;
    fluid_container_free = data;
}


// ============[ FOOD ]============
FOOD_BUFF_DATA *food_buff_data_free;

FOOD_BUFF_DATA *new_food_buff_data()
{
    FOOD_BUFF_DATA *data;
    if(food_buff_data_free)
    {
        data = food_buff_data_free;
        food_buff_data_free = food_buff_data_free->next;
    }
    else
        data = alloc_mem(sizeof(FOOD_BUFF_DATA));
    
    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

void free_food_buff_data(FOOD_BUFF_DATA *data)
{
    if(!IS_VALID(data)) return;

    variable_clearfield(VAR_FOOD_BUFF, data);

    INVALIDATE(data);
    data->next = food_buff_data_free;
    food_buff_data_free = data;
}

static void *copy_food_buff_data(void *src)
{
    FOOD_BUFF_DATA *a = (FOOD_BUFF_DATA *)src;
    FOOD_BUFF_DATA *b = new_food_buff_data();

    if (IS_VALID(b))
    {
        b->where = a->where;
        b->level = a->level;
        b->location = a->location;
        b->modifier = a->modifier;
        b->bitvector = a->bitvector;
        b->bitvector2 = a->bitvector2;
        b->duration = a->duration;
    }

    return b;
}

static void delete_food_buff_data(void *ptr)
{
    free_food_buff_data((FOOD_BUFF_DATA *)ptr);
}

FOOD_DATA *food_data_free;

FOOD_DATA *new_food_data()
{
    FOOD_DATA *data;
    if (food_data_free)
    {
        data = food_data_free;
        food_data_free = food_data_free->next;
    }
    else
        data = alloc_mem(sizeof(FOOD_DATA));
    
    memset(data, 0, sizeof(*data));

    data->buffs = list_createx(FALSE, copy_food_buff_data, delete_food_buff_data);

    VALIDATE(data);
    return data;
}

FOOD_DATA *copy_food_data(FOOD_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FOOD_DATA *data;
    if (food_data_free)
    {
        data = food_data_free;
        food_data_free = food_data_free->next;
    }
    else
        data = alloc_mem(sizeof(FOOD_DATA));
    
    data->hunger = src->hunger;
    data->full = src->full;
    data->poison = src->poison;

    data->buffs = list_copy(src->buffs);

    VALIDATE(data);
    return data;
}

void free_food_data(FOOD_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->buffs);

    INVALIDATE(data);
    data->next = food_data_free;
    food_data_free = data;
}

// =========[ FURNITURE ]==========
FURNITURE_COMPARTMENT *furniture_compartment_Free;

FURNITURE_COMPARTMENT *new_furniture_compartment()
{
    FURNITURE_COMPARTMENT *data;
    if (furniture_compartment_Free)
    {
        data = furniture_compartment_Free;
        furniture_compartment_Free = furniture_compartment_Free->next;
    }
    else
        data = alloc_mem(sizeof(FURNITURE_COMPARTMENT));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->short_descr = str_dup("");
    data->description = str_dup("");

    data->max_occupants = -1;
    data->max_weight = -1;

    VALIDATE(data);
    return data;
}

void free_furniture_compartment(FURNITURE_COMPARTMENT *data)
{
    if (!IS_VALID(data)) return;

    variable_clearfield(VAR_COMPARTMENT, data);

    free_string(data->name);
    free_string(data->short_descr);
    free_string(data->description);
    free_lock_state(data->lock);

    INVALIDATE(data);
    data->next = furniture_compartment_Free;
    furniture_compartment_Free = data;
}

static void *copy_furniture_compartment(void *ptr)
{
    FURNITURE_COMPARTMENT *src = (FURNITURE_COMPARTMENT *)ptr;
    FURNITURE_COMPARTMENT *data;
    if (furniture_compartment_Free)
    {
        data = furniture_compartment_Free;
        furniture_compartment_Free = furniture_compartment_Free->next;
    }
    else
        data = alloc_mem(sizeof(FURNITURE_COMPARTMENT));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup(src->name);
    data->short_descr = str_dup(src->short_descr);
    data->description = str_dup(src->description);

    data->flags = src->flags;
    data->max_weight = src->max_weight;
    data->max_occupants = src->max_occupants;

    data->standing = src->standing;
    data->hanging = src->hanging;
    data->sitting = src->sitting;
    data->resting = src->resting;
    data->sleeping = src->sleeping;

    data->health_regen = src->health_regen;
    data->mana_regen = src->mana_regen;
    data->move_regen = src->move_regen;

    data->lock = copy_lock_state(src->lock);

    VALIDATE(data);
    return data;
}

static void delete_furniture_compartment(void *ptr)
{
    free_furniture_compartment((FURNITURE_COMPARTMENT *)ptr);
}

FURNITURE_DATA *furniture_data_free;

FURNITURE_DATA *new_furniture_data()
{
    FURNITURE_DATA *data;
    if (furniture_data_free)
    {
        data = furniture_data_free;
        furniture_data_free = furniture_data_free->next;
    }
    else
        data = alloc_mem(sizeof(FURNITURE_DATA));

    memset(data, 0, sizeof(*data));

    data->compartments = list_createx(FALSE, copy_furniture_compartment, delete_furniture_compartment);
    data->main_compartment = 0;

    VALIDATE(data);
    return data;
}

FURNITURE_DATA *copy_furniture_data(FURNITURE_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    FURNITURE_DATA *data;

    if (furniture_data_free)
    {
        data = furniture_data_free;
        furniture_data_free = furniture_data_free->next;
    }
    else
        data = alloc_mem(sizeof(FURNITURE_DATA));

    // No need to memset here as all data fields are initialized
    data->compartments = list_copy(src->compartments);
    data->main_compartment = src->main_compartment;

    VALIDATE(data);
    return data;
}

void free_furniture_data(FURNITURE_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->compartments);

    INVALIDATE(data);
    data->next = furniture_data_free;
    furniture_data_free = data;
}

// ============[ INK ]=============
INK_DATA *ink_data_free;

INK_DATA *new_ink_data()
{
    INK_DATA *data;
    if (ink_data_free)
    {
        data = ink_data_free;
        ink_data_free = ink_data_free->next;
    }
    else
        data = alloc_mem(sizeof(INK_DATA));
    
    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

INK_DATA *copy_ink_data(INK_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    INK_DATA *data;
    if (ink_data_free)
    {
        data = ink_data_free;
        ink_data_free = ink_data_free->next;
    }
    else
        data = alloc_mem(sizeof(INK_DATA));
    
    memset(data, 0, sizeof(*data));

    for(int i = 0; i < MAX_INK_TYPES; i++)
    {
        data->types[i] = src->types[i];
        data->amounts[i] = src->amounts[i];
    }

    VALIDATE(data);
    return data;
}

void free_ink_data(INK_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = ink_data_free;
    ink_data_free = data;
}

// =========[ INSTRUMENT ]=========
INSTRUMENT_DATA *instrument_data_free;

INSTRUMENT_DATA *new_instrument_data()
{
    INSTRUMENT_DATA *data;
    if (instrument_data_free)
    {
        data = instrument_data_free;
        instrument_data_free = instrument_data_free->next;
    }
    else
        data = alloc_mem(sizeof(INSTRUMENT_DATA));
    
    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

INSTRUMENT_DATA *copy_instrument_data(INSTRUMENT_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    INSTRUMENT_DATA *data;
    if (instrument_data_free)
    {
        data = instrument_data_free;
        instrument_data_free = instrument_data_free->next;
    }
    else
        data = alloc_mem(sizeof(INSTRUMENT_DATA));

    data->next = NULL;
    data->type = src->type;
    data->flags = src->flags;
    data->mana_min = src->mana_min;
    data->mana_max = src->mana_max;
    data->beats_min = src->beats_min;
    data->beats_max = src->beats_max;

    for(int i = 0; i < INSTRUMENT_MAX_CATALYSTS; i++)
    {
        data->reservoirs[i] = src->reservoirs[i];
    }

    VALIDATE(data);
    return data;
}

void free_instrument_data(INSTRUMENT_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = instrument_data_free;
    instrument_data_free = data;
}

// ==========[ JEWELRY ]===========
JEWELRY_DATA *jewelry_data_free;

JEWELRY_DATA *new_jewelry_data()
{
    JEWELRY_DATA *data;
    if(jewelry_data_free)
    {
        data = jewelry_data_free;
        jewelry_data_free = jewelry_data_free->next;
    }
    else
        data = alloc_mem(sizeof(JEWELRY_DATA));
    
    memset(data, 0, sizeof(*data));

    data->spells = list_createx(false, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

JEWELRY_DATA *copy_jewelry_data(JEWELRY_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    JEWELRY_DATA *data;

    if(jewelry_data_free)
    {
        data = jewelry_data_free;
        jewelry_data_free = jewelry_data_free->next;
    }
    else
        data = alloc_mem(sizeof(JEWELRY_DATA));
    
    memset(data, 0, sizeof(*data));

    data->max_mana = src->max_mana;
    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_jewelry_data(JEWELRY_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = jewelry_data_free;
    jewelry_data_free = data;
}

// ===========[ LIGHT ]============
LIGHT_DATA *light_data_free;

LIGHT_DATA *new_light_data()
{
    LIGHT_DATA *data;
    if (light_data_free)
    {
        data = light_data_free;
        light_data_free = light_data_free->next;
    }
    else
        data = alloc_mem(sizeof(LIGHT_DATA));

    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

LIGHT_DATA *copy_light_data(LIGHT_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    LIGHT_DATA *data = new_light_data();

    data->duration = src->duration;

    return data;
}

void free_light_data(LIGHT_DATA *data)
{
    if(!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = light_data_free;
    light_data_free = data;
}

// ===========[ MONEY ]============
MONEY_DATA *money_data_free;

MONEY_DATA *new_money_data()
{
    MONEY_DATA *data;
    if (money_data_free)
    {
        data = money_data_free;
        money_data_free = money_data_free->next;
    }
    else
        data = alloc_mem(sizeof(MONEY_DATA));

    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

MONEY_DATA *copy_money_data(MONEY_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    MONEY_DATA *data = new_money_data();

    data->silver = src->silver;
    data->gold = src->gold;

    return data;
}

void free_money_data(MONEY_DATA *data)
{
    if(!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = money_data_free;
    money_data_free = data;
}




// ===========[ PORTAL ]===========
PORTAL_DATA *portal_data_free;

PORTAL_DATA *new_portal_data()
{
    PORTAL_DATA *data;
    if (portal_data_free)
    {
        data = portal_data_free;
        portal_data_free = portal_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PORTAL_DATA));

    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->short_descr = str_dup("");
    data->charges = -1;
    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

PORTAL_DATA *copy_portal_data(PORTAL_DATA *src, bool repop)
{
    if (!IS_VALID(src)) return NULL;

    PORTAL_DATA *data;
    if (portal_data_free)
    {
        data = portal_data_free;
        portal_data_free = portal_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PORTAL_DATA));

    data->name = str_dup(src->name);
    data->short_descr = str_dup(src->short_descr);
    data->exit = src->exit;
    data->flags = src->flags;
    data->charges = src->charges;
    data->type = src->type;

    for(int i = 0; i < MAX_PORTAL_VALUES; i++)
        data->params[i] = src->params[i];

    data->lock = copy_lock_state(src->lock);
    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_portal_data(PORTAL_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->short_descr);
    free_lock_state(data->lock);
    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = portal_data_free;
    portal_data_free = data;
}





// Scroll
SCROLL_DATA *scroll_data_free;
SCROLL_DATA *new_scroll_data()
{
    SCROLL_DATA *data;
    if (scroll_data_free)
    {
        data = scroll_data_free;
        scroll_data_free = scroll_data_free->next;
    }
    else
        data = alloc_mem(sizeof(SCROLL_DATA));
    
    memset(data, 0, sizeof(*data));

    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

SCROLL_DATA *copy_scroll_data(SCROLL_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    SCROLL_DATA *data;
    if (scroll_data_free)
    {
        data = scroll_data_free;
        scroll_data_free = scroll_data_free->next;
    }
    else
        data = alloc_mem(sizeof(SCROLL_DATA));

    data->max_mana = src->max_mana;
    data->flags = src->flags;

    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_scroll_data(SCROLL_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = scroll_data_free;
    scroll_data_free = data;
}

// Tattoo
TATTOO_DATA *tattoo_data_free;
TATTOO_DATA *new_tattoo_data()
{
    TATTOO_DATA *data;
    if (tattoo_data_free)
    {
        data = tattoo_data_free;
        tattoo_data_free = tattoo_data_free->next;
    }
    else
        data = alloc_mem(sizeof(TATTOO_DATA));
    
    memset(data, 0, sizeof(*data));

    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

TATTOO_DATA *copy_tattoo_data(TATTOO_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    TATTOO_DATA *data;
    if (tattoo_data_free)
    {
        data = tattoo_data_free;
        tattoo_data_free = tattoo_data_free->next;
    }
    else
        data = alloc_mem(sizeof(TATTOO_DATA));

    data->touches = src->touches;
    data->fading_chance = src->fading_chance;
    data->fading_rate = src->fading_rate;

    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_tattoo_data(TATTOO_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = tattoo_data_free;
    tattoo_data_free = data;
}

// Wand
WAND_DATA *wand_data_free;
WAND_DATA *new_wand_data()
{
    WAND_DATA *data;
    if (wand_data_free)
    {
        data = wand_data_free;
        wand_data_free = wand_data_free->next;
    }
    else
        data = alloc_mem(sizeof(WAND_DATA));
    
    memset(data, 0, sizeof(*data));

    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

WAND_DATA *copy_wand_data(WAND_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    WAND_DATA *data;
    if (wand_data_free)
    {
        data = wand_data_free;
        wand_data_free = wand_data_free->next;
    }
    else
        data = alloc_mem(sizeof(WAND_DATA));

    data->charges = src->charges;
    data->max_charges = src->max_charges;
    data->cooldown = src->cooldown;
    data->recharge_time = src->recharge_time;
    data->max_mana = src->max_mana;

    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_wand_data(WAND_DATA *data)
{
    if (!IS_VALID(data)) return;

    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = wand_data_free;
    wand_data_free = data;
}


// Weapon
WEAPON_DATA *weapon_data_free;
WEAPON_DATA *new_weapon_data()
{
    WEAPON_DATA *data;
    if (weapon_data_free)
    {
        data = weapon_data_free;
        weapon_data_free = weapon_data_free->next;
    }
    else
        data = alloc_mem(sizeof(WEAPON_DATA));
    memset(data, 0, sizeof(*data));

    data->weapon_class = WEAPON_UNKNOWN;
    data->ammo = AMMO_NONE;

    for(int i = 0; i < MAX_ATTACK_POINTS; i++)
    {
        data->attacks[i].name = &str_empty[0];
        data->attacks[i].short_descr = &str_empty[0];
        data->attacks[i].type = -1;
    }

    data->spells = list_createx(FALSE, copy_spell, delete_spell);

    VALIDATE(data);
    return data;
}

WEAPON_DATA *copy_weapon_data(WEAPON_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    WEAPON_DATA *data;
    if (weapon_data_free)
    {
        data = weapon_data_free;
        weapon_data_free = weapon_data_free->next;
    }
    else
        data = alloc_mem(sizeof(WEAPON_DATA));
    memset(data, 0, sizeof(*data));

    data->weapon_class = src->weapon_class;
    for(int i = 0; i < MAX_ATTACK_POINTS; i++)
    {
        data->attacks[i] = src->attacks[i];
        data->attacks[i].name = str_dup(src->attacks[i].name);
        data->attacks[i].short_descr = str_dup(src->attacks[i].short_descr);
    }

    data->ammo = src->ammo;
    data->range = src->range;

    data->charges = src->charges;
    data->max_charges = src->max_charges;
    data->cooldown = src->cooldown;
    data->recharge_time = src->recharge_time;
    data->max_mana = src->max_mana;

    data->spells = list_copy(src->spells);

    VALIDATE(data);
    return data;
}

void free_weapon_data(WEAPON_DATA *data)
{
    if (!IS_VALID(data)) return;

    for(int i = 0; i < MAX_ATTACK_POINTS; i++)
    {
        free_string(data->attacks[i].name);
        free_string(data->attacks[i].short_descr);
    }

    list_destroy(data->spells);

    INVALIDATE(data);
    data->next = weapon_data_free;
    weapon_data_free = data;
}




////////////////////////////////////////////////////////////////

SKILL_DATA *skill_data_free;
SKILL_DATA *new_skill_data()
{
    SKILL_DATA *data;
    if (skill_data_free)
    {
        data = skill_data_free;
        skill_data_free = skill_data_free->next;
    }
    else
        data = alloc_mem(sizeof(SKILL_DATA));

    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->display = str_dup("");
    for(int i = 0; i < MAX_CLASS; i++)
    {
        data->skill_level[i] = 41;  // TODO: Replace with constant
        data->rating[i] = 1;
    }

    data->target = TAR_IGNORE;
    data->minimum_position = POS_DEAD;
    data->race = -1;

    data->noun_damage = str_dup("");
    data->msg_off = str_dup("");
    data->msg_obj = str_dup("");
    data->msg_disp = str_dup("");
    data->msg_defl_noaff_char = str_dup("");
    data->msg_defl_noaff_room = str_dup("");
    data->msg_defl_aff_char = str_dup("");
    data->msg_defl_aff_room = str_dup("");
    data->msg_defl_pass_self = str_dup("");
    data->msg_defl_pass_char = str_dup("");
    data->msg_defl_pass_vict = str_dup("");
    data->msg_defl_pass_room = str_dup("");

    data->msg_defl_refl_none_char = str_dup("");
    data->msg_defl_refl_none_room = str_dup("");
    data->msg_defl_refl_char = str_dup("");
    data->msg_defl_refl_vict = str_dup("");
    data->msg_defl_refl_room = str_dup("");

    for(int i = 0; i < MAX_SKILL_VALUES; i++)
        data->valuenames[i] = str_dup("");

    // Rest is NULL/0

    VALIDATE(data);
    return data;
}

void free_skill_data(SKILL_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->display);
    free_string(data->noun_damage);
    free_string(data->msg_disp);
    free_string(data->msg_obj);
    free_string(data->msg_off);

    free_string(data->msg_defl_noaff_char);
    free_string(data->msg_defl_noaff_room);
    free_string(data->msg_defl_aff_char);
    free_string(data->msg_defl_aff_room);
    free_string(data->msg_defl_pass_self);
    free_string(data->msg_defl_pass_char);
    free_string(data->msg_defl_pass_vict);
    free_string(data->msg_defl_pass_room);
    free_string(data->msg_defl_refl_none_char);
    free_string(data->msg_defl_refl_none_room);
    free_string(data->msg_defl_refl_char);
    free_string(data->msg_defl_refl_vict);
    free_string(data->msg_defl_refl_room);

    for(int i = 0; i < MAX_SKILL_VALUES; i++)
        free_string(data->valuenames[i]);

    data->next = skill_data_free;
    skill_data_free = data;
    INVALIDATE(data);
}

static void delete_group_content(void *ptr)
{
    free_string((char *)ptr);
}

SKILL_GROUP *skill_group_data_free;
SKILL_GROUP *new_skill_group_data()
{
    SKILL_GROUP *data;
    if (skill_group_data_free)
    {
        data = skill_group_data_free;
        skill_group_data_free = skill_group_data_free->next;
    }
    else
        data = alloc_mem(sizeof(SKILL_GROUP));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->contents = list_createx(FALSE, NULL, delete_group_content);

    VALIDATE(data);
    return data;
}

void free_skill_group_data(SKILL_GROUP *group)
{
    if (!IS_VALID(group)) return;

    free_string(group->name);
    list_destroy(group->contents);

    INVALIDATE(group);
    group->next = skill_group_data_free;
    skill_group_data_free = group;
}


SONG_DATA *song_data_free;
SONG_DATA *new_song_data()
{
    SONG_DATA *data;
    if (song_data_free)
    {
        data = song_data_free;
        song_data_free = song_data_free->next;
    }
    else
        data = alloc_mem(sizeof(SONG_DATA));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup("");

    VALIDATE(data);
    return data;
}

void free_song_data(SONG_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);

    INVALIDATE(data);
    data->next = song_data_free;
    song_data_free = data;
}


REPUTATION_INDEX_RANK_DATA *reputation_index_rank_free;
REPUTATION_INDEX_RANK_DATA *new_reputation_index_rank_data()
{
    REPUTATION_INDEX_RANK_DATA *data;
    if (reputation_index_rank_free)
    {
        data = reputation_index_rank_free;
        reputation_index_rank_free = reputation_index_rank_free->next;
    }
    else
        data = alloc_mem(sizeof(REPUTATION_INDEX_RANK_DATA));
    
    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->description = str_dup("");
    data->comments = str_dup("");
    data->color = 'Y';      // Yellow
    data->capacity = 1;

    VALIDATE(data);
    return data;
}

void free_reputation_index_rank_data(REPUTATION_INDEX_RANK_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->description);
    free_string(data->comments);

    INVALIDATE(data);
    data->next = reputation_index_rank_free;
    reputation_index_rank_free = data;
}

static void delete_reputation_index_rank_data(void *ptr)
{
    free_reputation_index_rank_data((REPUTATION_INDEX_RANK_DATA *)ptr);
}

REPUTATION_INDEX_DATA *reputation_index_free;
REPUTATION_INDEX_DATA *new_reputation_index_data()
{
    REPUTATION_INDEX_DATA *data;
    if (reputation_index_free)
    {
        data = reputation_index_free;
        reputation_index_free = reputation_index_free->next;
    }
    else
        data = alloc_mem(sizeof(REPUTATION_INDEX_DATA));

    memset(data, 0, sizeof(*data));

    data->name = str_dup("");
    data->description = str_dup("");
    data->comments = str_dup("");
    data->created_by = str_dup("(none)");

    // If initial rank = 0, the reputation has not been configured properly.

    data->ranks = list_createx(FALSE, NULL, delete_reputation_index_rank_data);
    
    VALIDATE(data);
    return data;
}

void free_reputation_index_data(REPUTATION_INDEX_DATA *data)
{
    if (!IS_VALID(data)) return;

    free_string(data->name);
    free_string(data->description);
    free_string(data->comments);
    free_string(data->created_by);

    list_destroy(data->ranks);

    INVALIDATE(data);
    data->next = reputation_index_free;
    reputation_index_free = data;
}

REPUTATION_DATA *reputation_free;
REPUTATION_DATA *new_reputation_data()
{
    REPUTATION_DATA *data;

    if(reputation_free)
    {
        data = reputation_free;
        reputation_free = reputation_free->next;
    }
    else
        data = alloc_mem(sizeof(REPUTATION_DATA));

    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

void free_reputation_data(REPUTATION_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = reputation_free;
    reputation_free = data;
}


MOB_REPUTATION_DATA *mob_reputation_free;
MOB_REPUTATION_DATA *new_mob_reputation_data()
{
    MOB_REPUTATION_DATA *data;
    if (mob_reputation_free)
    {
        data = mob_reputation_free;
        mob_reputation_free = mob_reputation_free->next;
    }
    else
        data = alloc_mem(sizeof(MOB_REPUTATION_DATA));
    memset(data, 0, sizeof(*data));

    VALIDATE(data);
    return data;
}

MOB_REPUTATION_DATA *copy_mob_reputation_data(MOB_REPUTATION_DATA *src)
{
    if (!IS_VALID(src)) return NULL;

    MOB_REPUTATION_DATA *data = new_mob_reputation_data();

    data->reputation = src->reputation;
    data->minimum_rank = src->minimum_rank;
    data->maximum_rank = src->maximum_rank;
    data->points = src->points;

    return data;
}

void free_mob_reputation_data(MOB_REPUTATION_DATA *data)
{
    if (!IS_VALID(data)) return;

    INVALIDATE(data);
    data->next = mob_reputation_free;
    mob_reputation_free = data;
}


PRACTICE_COST_DATA *practice_cost_data_free;
PRACTICE_COST_DATA *new_practice_cost_data()
{
    PRACTICE_COST_DATA *data;
    if (practice_cost_data_free)
    {
        data = practice_cost_data_free;
        practice_cost_data_free = practice_cost_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_COST_DATA));
    memset(data, 0, sizeof(*data));

    // Defaults
    data->custom_price = &str_empty[0];

    return data;
}

PRACTICE_COST_DATA *copy_practice_cost_data(PRACTICE_COST_DATA *src)
{
    if (!src) return NULL;

    PRACTICE_COST_DATA *data;
    if (practice_cost_data_free)
    {
        data = practice_cost_data_free;
        practice_cost_data_free = practice_cost_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_COST_DATA));
    memset(data, 0, sizeof(*data));

    data->min_rating = src->min_rating;

    data->silver = src->silver;
    data->practices = src->practices;
    data->trains = src->trains;
    data->qp = src->qp;
    data->dp = src->dp;
    data->pneuma = src->pneuma;
    data->rep_points = src->rep_points;
    data->paragon_levels = src->paragon_levels;
    data->custom_price = str_dup(src->custom_price);

    data->check_price = src->check_price;
    data->obj = src->obj;
    data->reputation = src->reputation;

    return data;
}
void free_practice_cost_data(PRACTICE_COST_DATA *data)
{
    if (!data) return;

    free_string(data->custom_price);

    data->next = practice_cost_data_free;
    practice_cost_data_free = data;
}

static void *copier_practice_cost_data(void *src)
{
    return copy_practice_cost_data((PRACTICE_COST_DATA *)src);
}

static void delete_practice_cost_data(void *ptr)
{
    free_practice_cost_data((PRACTICE_COST_DATA *)ptr);
}

PRACTICE_ENTRY_DATA *practice_entry_data_free;
PRACTICE_ENTRY_DATA *new_practice_entry_data()
{
    PRACTICE_ENTRY_DATA *data;
    if (practice_entry_data_free)
    {
        data = practice_entry_data_free;
        practice_entry_data_free = practice_entry_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_ENTRY_DATA));
    memset(data, 0, sizeof(*data));

    data->costs = list_createx(false, copier_practice_cost_data, delete_practice_cost_data);
    
    return data;
}

PRACTICE_ENTRY_DATA *copy_practice_entry_data(PRACTICE_ENTRY_DATA *src)
{
    if (!src) return NULL;

    PRACTICE_ENTRY_DATA *data;
    if (practice_entry_data_free)
    {
        data = practice_entry_data_free;
        practice_entry_data_free = practice_entry_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_ENTRY_DATA));
    memset(data, 0, sizeof(*data));

    data->skill = src->skill;
    data->song = src->song;

    data->reputation = src->reputation;
    data->min_reputation_rank = src->min_reputation_rank;
    data->max_reputation_rank = src->max_reputation_rank;
    data->min_show_rank = src->min_show_rank;
    data->max_show_rank = src->max_show_rank;

    data->max_rating = src->max_rating;

    data->costs = list_copy(src->costs);
    ITERATOR it;
    PRACTICE_COST_DATA *cost;
    iterator_start(&it, data->costs);
    while((cost = (PRACTICE_COST_DATA *)iterator_nextdata(&it)))
        cost->entry = data;
    iterator_stop(&it);

    data->check_script = src->check_script;

    return data;
}

void free_practice_entry_data(PRACTICE_ENTRY_DATA *data)
{
    if (!data) return;

    list_destroy(data->costs);

    data->next = practice_entry_data_free;
    practice_entry_data_free = data;
}

static void *copier_practice_entry_data(void *src)
{
    return copy_practice_entry_data((PRACTICE_ENTRY_DATA *)src);
}

static void delete_practice_entry_data(void *ptr)
{
    free_practice_entry_data((PRACTICE_ENTRY_DATA *)ptr);
}

PRACTICE_DATA *practice_data_free;
PRACTICE_DATA *new_practice_data()
{
    PRACTICE_DATA *data;
    if (practice_data_free)
    {
        data = practice_data_free;
        practice_data_free = practice_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_DATA));
    memset(data, 0, sizeof(*data));

    data->entries = list_createx(false, copier_practice_entry_data, delete_practice_entry_data);

    return data;
}

PRACTICE_DATA *copy_practice_data(PRACTICE_DATA *src)
{
    if (!src) return NULL;

    PRACTICE_DATA *data;
    if (practice_data_free)
    {
        data = practice_data_free;
        practice_data_free = practice_data_free->next;
    }
    else
        data = alloc_mem(sizeof(PRACTICE_DATA));
    memset(data, 0, sizeof(*data));

    data->standard = src->standard;
    data->entries = list_copy(src->entries);
    
    return data;
}

void free_practice_data(PRACTICE_DATA *data)
{
    if (data) return;

    list_destroy(data->entries);

    data->next = practice_data_free;
    practice_data_free = data;
}
