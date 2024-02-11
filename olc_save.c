/* OLC Save - Modular + More Efficient Version
   Copyright Anton Ouzilov, 2004-2005 */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "merc.h"
#include "recycle.h"
#include "tables.h"
#include "olc.h"
#include "olc_save.h"
#include "db.h"
#include "scripts.h"
#include "wilds.h"

// VERSION_ROOM_001 special defines
#define VR_001_EX_LOCKED		(C)
#define VR_001_EX_PICKPROOF		(F)
#define VR_001_EX_EASY			(H)
#define VR_001_EX_HARD			(I)
#define VR_001_EX_INFURIATING	(J)

// VERSION_OBJECT_004 special defines
#define VO_004_CONT_PICKPROOF	(B)		// For Containers and Books
#define VO_004_CONT_LOCKED		(D)
#define VO_004_CONT_SNAPKEY		(F)
#define VO_004_EX_LOCKED		(C)		// For Portals
#define VO_004_EX_PICKPROOF		(F)
#define VO_004_EX_EASY			(H)
#define VO_004_EX_HARD			(I)
#define VO_004_EX_INFURIATING	(J)


void obj_index_reset_multitype(OBJ_INDEX_DATA *pObjIndex);
void obj_index_set_primarytype(OBJ_INDEX_DATA *pObjIndex, int item_type);
void save_area_trade( FILE *fp, AREA_DATA *pArea );

/* Vizz - External Globals */
extern GLOBAL_DATA gconfig;

/* for reading */
static bool 	fMatch;
static char 	*word;
static char 	buf[MSL];

void do_asave_new(CHAR_DATA *ch, char *argument)
{
	char arg1[MAX_INPUT_LENGTH];
	AREA_DATA *pArea;
	char log_buf[MAX_STRING_LENGTH];

	smash_tilde(argument);
	strcpy(arg1, argument);

	if (arg1[0] == '\0')
	{
		send_to_char("Syntax:\n\r", ch);
		send_to_char("  asave area       - saves the area you are in\n\r",	ch);
		send_to_char("  asave changed    - saves all changed areas\n\r",	ch);
		send_to_char("  asave world      - saves the world (imps only)\n\r",	ch);
		send_to_char("  asave churches   - saves the churches\n\r", ch);
		send_to_char("  asave help       - saves the help files\n\r", ch);
		send_to_char("  asave mail       - saves the mail\n\r", ch);
		send_to_char("  asave projects   - saves the project database\n\r", ch);
		send_to_char("  asave persist    - saves all persistant entities\n\r", ch);

		if (IS_IMPLEMENTOR(ch))
			send_to_char("  asave staff      - saves the immortal staff information\n\r", ch);

		//send_to_char("  asave wilds    - saves wilderness templates (imps only)\n\r", ch);
		return;
	}

	// Save all areas
	if (!str_cmp("world", arg1))
	{
		/* Only for imps since it causes mucho lag */
		if (!IS_IMPLEMENTOR(ch))
		{
			send_to_char("Insufficient security to save world - action logged.\n\r", ch);
			return;
		}

		save_area_list();

		for (pArea = area_first; pArea; pArea = pArea->next)
		{
			sprintf(log_buf,"olc_save.c, do_asave: saving %s", pArea->name);
			log_string(log_buf);
			sprintf(log_buf, "Saving %s...\n\r", pArea->name);
			send_to_char(log_buf, ch);
			save_area_new(pArea);

			REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
		}

		send_to_char("You saved the world.\n\r", ch);
		return;
	}

    // Save only changed areas
    if (!str_cmp("changed", arg1))
    {
		char buf[MAX_INPUT_LENGTH];

		if (projects_changed)
		{
			save_projects();
			projects_changed = false;
			send_to_char("Project list saved.\n\r", ch);
		}
		log_string("olc_save.c, do_asave: changed, saving area list");
		save_area_list();

		send_to_char("Saved zones:\n\r", ch);

		sprintf(buf, "None.\n\r");

		for (pArea = area_first; pArea; pArea = pArea->next)
		{
			/* Builder must be assigned this area. */
			if (!IS_BUILDER(ch, pArea))
			continue;

			/* Save changed areas. */
			if (IS_SET(pArea->area_flags, AREA_CHANGED))
			{
				if ((!str_cmp(pArea->name, "Eden") || !str_cmp(pArea->name, "Netherworld")) ||
					pArea == (AREA_DATA *)-1 /*get_sailing_boat_area()*/)
						continue;
				else if (IS_SET(pArea->area_flags, AREA_TESTPORT))
				{
					if (!is_test_port)
					{
						if (!IS_IMPLEMENTOR(ch))
						{
							sprintf(buf, "%24s - '%s' NOT SAVED (testport area)\n\r",
							pArea->name, pArea->file_name);
							send_to_char(buf, ch);
							REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
							continue;
						}
						else
						{
							sprintf(buf, "%24s - '%s' saved (warning - this is a testport area)\n\r",
								pArea->name, pArea->file_name);
							send_to_char(buf, ch);
							REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
							save_area_new(pArea);
							continue;
						}
					}
					else
					{
						sprintf(buf, "%24s - '%s' saved and backed up from testport dir.\n\r",
							pArea->name, pArea->file_name);
						send_to_char(buf, ch);
						REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
						save_area_new(pArea);
						continue;
					}
				}
				else
				{
					sprintf(log_buf,"olc_save.c, do_asave: changed, saving %s", pArea->name);
						log_string(log_buf);
					save_area_new(pArea);
				}

				sprintf(buf, "%24s - '%s'\n\r", pArea->name, pArea->file_name);
				send_to_char(buf, ch);
				REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
			}
		}

		if (!str_cmp(buf, "None.\n\r"))
			send_to_char(buf, ch);

		return;
    }

    // Save current area
    if (!str_cmp(arg1, "area"))
    {
		if (!IS_BUILDER(ch, ch->in_room->area)) {
		    send_to_char("Sorry, you're not a builder in this area, so you can't save it.\n\r", ch);
		    return;
		}

		save_area_list();
		save_area_new(ch->in_room->area);
		act("Saved $t.", ch, NULL, NULL, NULL, NULL, ch->in_room->area->name, NULL, TO_CHAR);
		return;
    }

    // Save churches
    if (!str_cmp(arg1, "churches"))
    {
		write_churches_new();
		send_to_char("Churches saved.\n\r", ch);
		return;
    }

    if (!str_cmp(arg1, "projects"))
    {
		save_projects();
		projects_changed = false;
		send_to_char("Projects saved.\n\r", ch);
		return;
    }

    // Save helpfiles
    if (!str_cmp(arg1, "help"))
    {
		save_helpfiles_new();
        send_to_char("Help files saved.\n\r", ch);
        return;
    }

    // Save mail
    if (!str_cmp(arg1, "mail"))
    {
		write_mail();
		send_to_char("Mail saved.\n\r", ch);
		return;
    }

    if (!str_cmp(arg1, "persist"))
    {
	persist_save();
	send_to_char("Persistant entities saved.\n\r", ch);
	return;
    }

    if (!str_cmp(arg1, "staff"))
    {
	save_immstaff();
	send_to_char("Immortal staff list saved.\n\r", ch);
	return;
    }

    // Show syntax
    do_asave_new(ch, "");
}

// save the area.lst file
void save_area_list()
{
    FILE *fp;
    AREA_DATA *pArea;

    log_string("save_area_list: saving area.lst");
    if ((fp = fopen(AREA_LIST, "w")) == NULL)
    {
	bug("Save_area_list: fopen", 0);
	perror(AREA_LIST);
    }
    else
    {
	for (pArea = area_first; pArea; pArea = pArea->next)
	    fprintf(fp, "%s\n", pArea->file_name);

	fprintf(fp, "$\n");
	fclose(fp);
    }

    log_string("save_area_list: finished");
}

void save_area_region(FILE *fp, AREA_DATA *area, AREA_REGION *region)
{
	fprintf(fp, "#REGION %ld\n", region->uid);

	fprintf(fp, "Name %s~\n", fix_string(region->name));
	fprintf(fp, "Description %s~\n", fix_string(region->description));
	fprintf(fp, "Comments %s~\n", fix_string(region->comments));

	fprintf(fp, "Flags %ld\n", region->flags);
	fprintf(fp, "Who %d\n", region->area_who);
	fprintf(fp, "Place %d\n", region->place_flags);
	fprintf(fp, "Savage %d\n", region->savage_level);
	fprintf(fp, "PostOffice %ld\n", region->post_office);

    fprintf(fp, "XCoord %d\n", region->x);
    fprintf(fp, "YCoord %d\n", region->y);
    fprintf(fp, "XLand %d\n", region->land_x);
    fprintf(fp, "YLand %d\n", region->land_y);

    fprintf(fp, "AirshipLand %ld\n", 	region->airship_land_spot);

    if(region->recall.wuid)
		fprintf(fp, "RecallW %lu %lu %lu %lu\n", 	region->recall.wuid, region->recall.id[0], region->recall.id[1], region->recall.id[2]);
    else
		fprintf(fp, "Recall %ld\n", 	region->recall.id[0]);


	fprintf(fp, "#-REGION\n");
}

/* save an area to <area>.are */
void save_area_new(AREA_DATA *area)
{
    char buf[2*MAX_STRING_LENGTH];
    FILE *fp;
    char filename[MSL];
    OLC_POINT_BOOST *boost;

    // There are some areas which should be saved specially
	// TODO: REMOVE THIS CRAP
    if (!str_cmp(area->name, "Geldoff's Maze"))
	sprintf(filename, "../maze/template.geldmaze");
    else if (!str_cmp(area->name, "Maze-Level1"))
	sprintf(filename, "../maze/template.poa1");
    else if (!str_cmp(area->name, "Maze-Level2"))
	sprintf(filename, "../maze/template.poa2");
    else if (!str_cmp(area->name, "Maze-Level3"))
	sprintf(filename, "../maze/template.poa3");
    else if (!str_cmp(area->name, "Maze-Level4"))
	sprintf(filename, "../maze/template.poa4");
    else if (!str_cmp(area->name, "Maze-Level5"))
	sprintf(filename, "../maze/template.poa5");
    else if (IS_SET(area->area_flags, AREA_TESTPORT) && is_test_port)
    {
	sprintf(filename, "../../backups/%s", area->file_name);
	REMOVE_BIT(area->area_flags, AREA_TESTPORT);
	save_area_new(area);
	SET_BIT(area->area_flags, AREA_TESTPORT);
    }
    else
	sprintf(filename, "%s", area->file_name);

    if ((fp = fopen(filename, "w")) == NULL) {
		sprintf(buf, "save_area_new: couldn't open file %s", filename);
		bug(buf, 0);
		return;
    }

    sprintf(buf, "save_area_new: saving area %s to file %s", area->name, area->file_name);
    log_string(buf);

    fprintf(fp, "#AREA %s~\n", 		area->name);
    fprintf(fp, "FileName %s~\n",	area->file_name);
    fprintf(fp, "Uid %ld\n",		area->uid);
    fprintf(fp, "AreaFlags %ld\n", 	area->area_flags);
    fprintf(fp, "Builders %s~\n",      	fix_string(area->builders));
    fprintf(fp, "WildsVnum %ld\n",	area->wilds_uid);
    fprintf(fp, "Credits %s~\n",	area->credits);
    fprintf(fp, "Security %d\n",       	area->security);
    fprintf(fp, "Open %d\n", 	  	area->open);
    fprintf(fp, "Repop %d\n",		area->repop);
	fprintf(fp, "Description %s~\n", fix_string(area->description));
	if(!IS_NULLSTR(area->comments))
		fprintf(fp, "Comments %s~\n", fix_string(area->comments));

	save_area_region(fp, area, &area->region);
	ITERATOR rit;
	AREA_REGION *region;
	iterator_start(&rit, area->regions);
	while((region = (AREA_REGION *)iterator_nextdata(&rit)))
	{
		save_area_region(fp, area, region);
	}
	iterator_stop(&rit);

    // Save the current versions of everything
    fprintf(fp, "VersArea %d\n",			VERSION_AREA);
    fprintf(fp, "VersMobile %d\n",			VERSION_MOBILE);
    fprintf(fp, "VersObject %d\n",			VERSION_OBJECT);
    fprintf(fp, "VersRoom %d\n",			VERSION_ROOM);
    fprintf(fp, "VersToken %d\n",			VERSION_TOKEN);
    fprintf(fp, "VersScript %d\n",			VERSION_SCRIPT);
    fprintf(fp, "VersWilds %d\n",			VERSION_WILDS);
	fprintf(fp, "VersBlueprint %d\n",		VERSION_BLUEPRINT);
	fprintf(fp, "VersShip %d\n",			VERSION_SHIP);
	fprintf(fp, "VersDungeon %d\n",			VERSION_DUNGEON);
	fprintf(fp, "TopRegionUID %ld\n",		area->top_region_uid);

	for(boost = area->points; boost; boost = boost->next)
		fprintf(fp, "OlcPointBoost %d %d %d %d\n",
			boost->category,
			boost->usage,
			boost->imp,
			boost->area);

    if(area->progs->progs) {
		ITERATOR it;
		PROG_LIST *trigger;
		for(int i = 0; i < TRIGSLOT_MAX; i++) if(list_size(area->progs->progs[i]) > 0) {
			iterator_start(&it, area->progs->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "AreaProg %s %s~ %s~\n",
					widevnum_string_wnum(trigger->wnum, area),
					trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	olc_save_index_vars(fp, area->index_vars, area);

	save_reputation_indexes(fp, area);

    /* Whisp - write this function */
    save_area_trade(fp, area);

    if (!IS_SET(area->area_flags, AREA_NO_ROOMS))
		save_rooms_new(fp, area);

// VIZZWILDS
    if (area->wilds)
    {
        save_wilds(fp, area);
    }

    save_mobiles_new(fp, area);
    save_objects_new(fp, area);
    save_scripts_new(fp, area);
    save_tokens(fp, area);
	save_blueprints(fp, area);
	save_ships(fp, area);
	save_dungeons(fp, area);
	save_quests(fp, area);

/*    if (str_prefix("Maze-Level", area->name) && str_cmp("Geldoff's Maze", area->name)
    && str_cmp("Netherworld", area->name)
    &&  str_cmp("Eden", area->name))*/
	fprintf(fp, "#-AREA\n\n");

    fclose(fp);
    log_string("save_area_new: finished");
}


/* save all rooms in an area */
void save_rooms_new(FILE *fp, AREA_DATA *area)
{
    ROOM_INDEX_DATA *room;

	for(int i = 0; i < MAX_KEY_HASH; i++)
	{
		for( room = area->room_index_hash[i]; room; room = room->next)
		{
			if (room->vnum > 0)
				save_room_new(fp, room, ROOMTYPE_NORMAL);
		}
	}

}


/* save all mobiles in an area */
void save_mobiles_new(FILE *fp, AREA_DATA *area)
{
    MOB_INDEX_DATA *mob;
	int i;

    for (i = 0; i < MAX_KEY_HASH; i++)
    {
		for (mob = area->mob_index_hash[i];mob;mob = mob->next)
			save_mobile_new(fp, mob);
    }
}


/* save all objects in an area */
void save_objects_new(FILE *fp, AREA_DATA *area)
{
    OBJ_INDEX_DATA *obj;
	int i;

    for (i = 0; i < MAX_KEY_HASH; i++)
    {
		for (obj = area->obj_index_hash[i];obj;obj = obj->next)
			save_object_new(fp, obj);
    }
}


/* save all tokens in an area */
void save_tokens(FILE *fp, AREA_DATA *area)
{
    TOKEN_INDEX_DATA *token;
    int i;

    for (i = 0; i < MAX_KEY_HASH; i++)
    {
		for (token = area->token_index_hash[i];token;token = token->next)
			save_token(fp, token);

    }
}


/* save one token */
void save_token(FILE *fp, TOKEN_INDEX_DATA *token)
{
	ITERATOR it;
    PROG_LIST *trigger;
    EXTRA_DESCR_DATA *ed;
    int i;

    fprintf(fp, "#TOKEN %ld\n", token->vnum);
    fprintf(fp, "Name %s~\n", token->name);
    fprintf(fp, "Description %s~\n", fix_string(token->description));
    fprintf(fp, "Type %d\n", token->type);
    fprintf(fp, "Flags %ld\n", token->flags);
    fprintf(fp, "Timer %d\n", token->timer);

    for (ed = token->ed; ed != NULL; ed = ed->next) {
		fprintf(fp, "#EXTRA_DESCR %s~\n", ed->keyword);
		if( ed->description )
			fprintf(fp, "Description %s~\n", fix_string(ed->description));
		else
			fprintf(fp, "Environmental\n");
		fprintf(fp, "#-EXTRA_DESCR\n");
    }

    for (i = 0; i < MAX_TOKEN_VALUES; i++)
		fprintf(fp, "Value %d %ld\n", i, token->value[i]);

    for (i = 0; i < MAX_TOKEN_VALUES; i++)
		fprintf(fp, "ValueName %d %s~\n", i, token->value_name[i]);

	if(token->comments)
		fprintf(fp, "Comments %s~\n", fix_string(token->comments));

    if(token->progs) {
		for(i = 0; i < TRIGSLOT_MAX; i++) if(list_size(token->progs[i]) > 0) {
			iterator_start(&it, token->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
			{
				char *trig_name = trigger_name(trigger->trig_type);
				char *trig_phrase = trigger_phrase(trigger->trig_type,trigger->trig_phrase);
				fprintf(fp, "TokProg %s %s~ %s~\n",
					widevnum_string_wnum(trigger->wnum, token->area),
					trig_name, trig_phrase);
			}
			iterator_stop(&it);
		}
    }

	olc_save_index_vars(fp, token->index_vars, token->area);

    fprintf(fp, "#-TOKEN\n");
}


/* save one room */
void save_room_new(FILE *fp, ROOM_INDEX_DATA *room, int recordtype)
{
    EXTRA_DESCR_DATA *ed;
    CONDITIONAL_DESCR_DATA *cd;
    int door, i;
    EXIT_DATA *ex;
    ITERATOR it;
    PROG_LIST *trigger;
    RESET_DATA *reset;

    if (fp == NULL || room == NULL) {
	bug("save_room_new: NULL.", 0);
	return;
    }

    // VIZZWILDS
    if (recordtype == ROOMTYPE_NORMAL)
        fprintf(fp, "#ROOM %ld\n", room->vnum);
    else
        fprintf(fp, "#ROOM\n");
    fprintf(fp, "Name %s~\n", room->name);
    fprintf(fp, "Description %s~\n", fix_string(room->description));
    if(room->persist)
    	fprintf(fp, "Persist\n");

    if (room->home_owner != NULL)
		fprintf(fp, "Home_owner %s~\n", room->home_owner);

	if(room->viewwilds) {
		fprintf(fp,"Wilds %1u %1u %1u %1u\n", (unsigned)room->viewwilds->uid, (unsigned)room->x, (unsigned)room->y, (unsigned)room->z);
	}
	else if(IS_SET(room->room_flag[1], ROOM_BLUEPRINT))
	{
		fprintf(fp,"Blueprint %1u %1u %1u\n", (unsigned)room->x, (unsigned)room->y, (unsigned)room->z);
	}


	if (room->region != NULL)
	{
		fprintf(fp, "Region %ld\n", room->region->uid);
	}

    fprintf(fp, "Room_flags %ld\n", room->room_flag[0]);
    fprintf(fp, "Room2_flags %ld\n", room->room_flag[1]);
    fprintf(fp, "Sector %s\n", room->sector->name);
	fprintf(fp, " - Sector Flags: %s\n", flag_string(sector_flags, room->sector_flags));

    if (room->heal_rate != 100)
	fprintf(fp, "HealRate %d\n", room->heal_rate);

    if (room->mana_rate != 100)
	fprintf(fp, "ManaRate %d\n", room->mana_rate);

    if (room->move_rate != 100)
	fprintf(fp, "MoveRate %d\n", room->move_rate);

    if (room->owner != NULL && room->owner[0] != '\0')
	fprintf(fp, "Owner %s~\n", room->owner);

    for (ed = room->extra_descr; ed != NULL; ed = ed->next) {
		fprintf(fp, "#EXTRA_DESCR %s~\n", ed->keyword);
		if( ed->description )
			fprintf(fp, "Description %s~\n", fix_string(ed->description));
		else
			fprintf(fp, "Environmental\n");
		fprintf(fp, "#-EXTRA_DESCR\n");
    }

    for (cd = room->conditional_descr; cd != NULL; cd = cd->next) {
	fprintf(fp, "#CONDITIONAL_DESCR\n");
	fprintf(fp, "Condition %d\n", cd->condition);
	fprintf(fp, "Phrase %d\n", cd->phrase);
	fprintf(fp, "Description %s~\n", fix_string(cd->description));
        fprintf(fp, "#-CONDITIONAL_DESCR\n");
    }
	if (room->comments)
		fprintf(fp, "Comments %s~\n", fix_string(room->comments));


    for (door = 0; door < MAX_DIR; door++) {
		char kwd[MSL];

		if ((ex = room->exit[door]) != NULL) {
			// Skip vlink exits
			if( IS_SET(ex->rs_flags, EX_VLINK) )
				continue;

			fprintf(fp, "#X '%s'\n", dir_name[ex->orig_door]);	// FIX IF SPACES ARE NEEDED IN NAMES!

				if (ex->keyword[0] == ' ') {
			sprintf(kwd, " ");
			strcat(kwd, ex->keyword + 1);
			}
			else
			sprintf(kwd, ex->keyword);

			WNUM to_room;
			if (ex->u1.to_room)
			{
				to_room.pArea = ex->u1.to_room->area;
				to_room.vnum = ex->u1.to_room->vnum;
			}
			else
			{
				to_room.pArea = NULL;
				to_room.vnum = 0;
			}
			fprintf(fp, "Key %s To_room %s Rs_flags %d Keyword %s~\n",
				widevnum_string_wnum(ex->door.rs_lock.key_wnum, room->area),
				widevnum_string_wnum(to_room, room->area), ex->rs_flags, kwd);
			fprintf(fp, "LockFlags %d\n", ex->door.rs_lock.flags);
			fprintf(fp, "PickChance %d\n", ex->door.rs_lock.pick_chance);
			fprintf(fp, "Description %s~\n", fix_string(ex->short_desc));
			fprintf(fp, "LongDescription %s~\n", fix_string(ex->long_desc));
			fprintf(fp, "#-X\n");
		}
    }

    if(room->progs->progs) {
		for(i = 0; i < TRIGSLOT_MAX; i++) if(list_size(room->progs->progs[i]) > 0) {
			iterator_start(&it, room->progs->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "RoomProg %s %s~ %s~\n", widevnum_string_wnum(trigger->wnum, room->area), trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	olc_save_index_vars(fp, room->index_vars, room->area);

    for (reset = room->reset_first; reset != NULL; reset = reset->next) {
		fprintf(fp, "#RESET %c\n", reset->command);
		fprintf(fp, "Arguments %s %ld %s %ld\n",
				widevnum_string_wnum(reset->arg1.wnum, room->area),
				reset->arg2,
				widevnum_string_wnum(reset->arg3.wnum, room->area),
				reset->arg4);
		fprintf(fp, "#-RESET\n");
    }

    fprintf(fp, "#-ROOM\n\n");
}


/* save one mobile */
void save_mobile_new(FILE *fp, MOB_INDEX_DATA *mob)
{
	ITERATOR it;
    PROG_LIST *trigger;
    int i;

    RACE_DATA *race = mob->race;

    fprintf(fp, "#MOBILE %ld\n", mob->vnum);
    fprintf(fp, "Name %s~\n", mob->player_name);
    fprintf(fp, "ShortDesc %s~\n", mob->short_descr);
    fprintf(fp, "LongDesc %s~\n", mob->long_descr);
    fprintf(fp, "Description %s~\n", fix_string(mob->description));
    fprintf(fp, "Owner %s~\n", mob->owner);
    fprintf(fp, "ImpSig %s~\n", mob->sig);
    fprintf(fp, "CreatorSig %s~\n", mob->creator_sig);
    if(mob->persist)
 	   fprintf(fp, "Persist\n");
    fprintf(fp, "Skeywds %s~\n", mob->skeywds);
    fprintf(fp, "Race %s~\n", mob->race->name);
    if (mob->act[0] != 0)
	fprintf(fp, "Act %ld\n", mob->act[0] | race->act[0]);
    if (mob->act[1] != 0)
	fprintf(fp, "Act2 %ld\n", mob->act[1] | race->act[1]);
    if (mob->affected_by[0] != 0)
	fprintf(fp, "Affected_by %ld\n", mob->affected_by[0] | race->aff[0]);
    if (mob->affected_by[1] != 0)
	fprintf(fp, "Affected_by2 %ld\n", mob->affected_by[1] | race->aff[1]);

    fprintf(fp, "Level %d Alignment %d\n", mob->level, mob->alignment);

    fprintf(fp, "Hitroll %d Hit %d %d %d Mana %d %d %d Damage %d %d %d Movement %ld\n",
        mob->hitroll,
	mob->hit.number, mob->hit.size, mob->hit.bonus,
	mob->mana.number, mob->mana.size, mob->mana.bonus,
	mob->damage.number, mob->damage.size, mob->damage.bonus, mob->move);
    fprintf(fp, "AttackType %d\n", mob->dam_type);
    fprintf(fp, "Attacks %d\n", mob->attacks);
    fprintf(fp, "OffFlags %ld ImmFlags %ld ResFlags %ld VulnFlags %d\n",
        mob->off_flags,
	mob->imm_flags,
	mob->res_flags,
	mob->vuln_flags);
    fprintf(fp, "StartPos %d DefaultPos %d Sex %d Wealth %ld\n",
        mob->start_pos, mob->default_pos, mob->sex, mob->wealth);
    fprintf(fp, "Parts %ld Size %d\n",
	mob->parts, mob->size);
	if (IS_VALID(mob->material))
    	fprintf(fp, "Material %s~\n", mob->material->name);
    if (mob->corpse_type)
	fprintf(fp, "CorpseType %ld\n", (long int)mob->corpse_type);
    if (mob->corpse.auid > 0 && mob->corpse.vnum > 0)
	{
		if (mob->area->uid != mob->corpse.auid)
			fprintf(fp, "CorpseWnum %ld#%ld\n", mob->corpse.auid, mob->corpse.vnum);
		else
			fprintf(fp, "CorpseWnum #%ld\n", mob->corpse.vnum);
	}
    if (mob->zombie.auid > 0 && mob->corpse.vnum > 0)
	{
		if (mob->area->uid != mob->zombie.auid)
			fprintf(fp, "CorpseZombie %ld#%ld\n", mob->zombie.auid, mob->corpse.vnum);
		else
			fprintf(fp, "CorpseZombie #%ld\n", mob->corpse.vnum);
	}
	if(mob->comments)
		fprintf(fp, "Comments %s~\n", fix_string(mob->comments));

	if(mob->boss)
		fprintf(fp, "Boss\n");

	MOB_REPUTATION_DATA *rep;
	for(rep = mob->reputations; rep; rep = rep->next)
	{
		fprintf(fp, "Reputation %s %d %d %ld\n", widevnum_string(rep->reputation->area, rep->reputation->vnum, mob->area), rep->minimum_rank, rep->maximum_rank, rep->points);
	}

	REPUTATION_INDEX_DATA *repIndex;
	iterator_start(&it, mob->factions);
	while((repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Faction %ld#%ld\n", repIndex->area->uid, repIndex->vnum);
	}
	iterator_stop(&it);

	if (mob->pPractice != NULL)
		save_practice_data(fp, mob->pPractice, mob->area);

	if (mob->pMissionary != NULL)
		save_missionary_new(fp, mob->pMissionary, mob->area);

    /* save the shop */
    if (mob->pShop != NULL)
		save_shop_new(fp, mob->pShop, mob->area);

	if (IS_VALID(mob->pCrew))
	{
		save_ship_crew_index_new(fp, mob->pCrew);
	}

    if(mob->progs) {
		for(i = 0; i < TRIGSLOT_MAX; i++) if(list_size(mob->progs[i]) > 0) {
			iterator_start(&it, mob->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "MobProg %s %s~ %s~\n", widevnum_string_wnum(trigger->wnum, mob->area), trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	olc_save_index_vars(fp, mob->index_vars, mob->area);

    if (mob->spec_fun != NULL)
	fprintf(fp, "SpecFun %s~\n", spec_name(mob->spec_fun));

    fprintf(fp, "#-MOBILE\n");
}

void save_object_lockstate(FILE *fp, LOCK_STATE *lock)
{
	fprintf(fp, "#LOCK\n");
	// TOOD: Make it a list
	fprintf(fp, "Key %s\n", widevnum_string_wnum(lock->key_wnum, NULL));
	fprintf(fp, "Flags %s\n", print_flags(lock->flags));
	fprintf(fp, "PickChance %d\n", lock->pick_chance);
	fprintf(fp, "#-LOCK\n");
}

void save_object_multityping(FILE *fp, OBJ_INDEX_DATA *obj)
{
	ITERATOR it;

	if (IS_AMMO(obj))
	{
		fprintf(fp, "#TYPEAMMO\n");

		fprintf(fp, "Type %s~\n", flag_string(ammo_types, AMMO(obj)->type));

		if (!IS_NULLSTR(AMMO(obj)->msg_break))
			fprintf(fp, "MsgBreak %s~\n", fix_string(AMMO(obj)->msg_break));

		fprintf(fp, "Damage %s~ %s %d %d %d\n",
			attack_table[AMMO(obj)->damage_type].noun,
			print_flags(AMMO(obj)->flags),
			AMMO(obj)->damage.number,
			AMMO(obj)->damage.size,
			AMMO(obj)->damage.bonus);

		fprintf(fp, "#-TYPEAMMO\n");
	}

	if (IS_ARMOR(obj))
	{
		ARMOR_DATA *armor = ARMOR(obj);

		fprintf(fp, "#TYPEARMOR\n");

		fprintf(fp, "Type %s~\n", flag_string(armour_types, armor->armor_type));
		fprintf(fp, "Strength %s~\n", flag_string(armour_strength_table, armor->armor_strength));

		for(int i = 0; i < ARMOR_MAX; i++)
		{
			fprintf(fp, "Protection %s~ %d\n", flag_string(armour_protection_types, i), armor->protection[i]);
		}

		fprintf(fp, "MaxAdornments %d\n", armor->max_adornments);
		if (armor->max_adornments > 0 && armor->adornments != NULL)
		{
			for(int i = 0; i < armor->max_adornments; i++)
			{
				ADORNMENT_DATA *adorn = armor->adornments[i];
				if (IS_VALID(adorn))
				{
					fprintf(fp, "#ADORNMENT\n");
					fprintf(fp, "Type %s~\n", flag_string(adornment_types, adorn->type));
					fprintf(fp, "Name %s~\n", fix_string(adorn->name));
					fprintf(fp, "Short %s~\n", fix_string(adorn->short_descr));
					fprintf(fp, "Description %s~\n", fix_string(adorn->description));

					if (adorn->spell != NULL)
						fprintf(fp, "Spell %s~ %d\n", adorn->spell->skill->name, adorn->spell->level);

					fprintf(fp, "#-ADORNMENT\n");
				}
			}
		}

		fprintf(fp, "#-TYPEARMOR\n");
	}

	if (IS_BOOK(obj))
	{
		BOOK_DATA *book = BOOK(obj);
		BOOK_PAGE *page;

		fprintf(fp, "#TYPEBOOK\n");
		fprintf(fp, "Name %s~\n", fix_string(book->name));
		fprintf(fp, "Short %s~\n", fix_string(book->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(book->flags));
		fprintf(fp, "CurrentPage %d\n", book->current_page);
		fprintf(fp, "OpenPage %d\n", book->open_page);

		iterator_start(&it, book->pages);
		while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#PAGE %d\n", page->page_no);
			fprintf(fp, "Title %s~\n", fix_string(page->title));
			fprintf(fp, "Text %s~\n", fix_string(page->text));
			fprintf(fp, "#-PAGE\n");
		}
		iterator_stop(&it);

		if (book->lock)
			save_object_lockstate(fp, book->lock);

		fprintf(fp, "#-TYPEBOOK\n");
	}

	if (IS_CART(obj))
	{
		CART_DATA *cart = CART(obj);
		fprintf(fp, "#TYPECART\n");
		fprintf(fp, "Flags %s\n", print_flags(cart->flags));
		fprintf(fp, "MinStrength %d\n", cart->min_strength);
		fprintf(fp, "MoveDelay %d\n", cart->move_delay);
		fprintf(fp, "#-TYPECART\n");
	}

	if (IS_COMPASS(obj))
	{
		COMPASS_DATA *compass = COMPASS(obj);
		fprintf(fp, "#TYPECOMPASS\n");
		fprintf(fp, "Accuracy %d\n", compass->accuracy);
		fprintf(fp, "Wilds %ld\n", compass->wuid);
		fprintf(fp, "X %ld\n", compass->x);
		fprintf(fp, "Y %ld\n", compass->y);
		fprintf(fp, "#-TYPECOMPASS\n");
	}

	if (IS_CONTAINER(obj))
	{
		CONTAINER_FILTER *filter;

		fprintf(fp, "#TYPECONTAINER\n");
		fprintf(fp, "Name %s~\n", fix_string(CONTAINER(obj)->name));
		fprintf(fp, "Short %s~\n", fix_string(CONTAINER(obj)->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(CONTAINER(obj)->flags));
		fprintf(fp, "MaxWeight %d\n", CONTAINER(obj)->max_weight);
		fprintf(fp, "WeightMultiplier %d\n", CONTAINER(obj)->weight_multiplier);
		fprintf(fp, "MaxVolume %d\n", CONTAINER(obj)->max_volume);

		iterator_start(&it, CONTAINER(obj)->whitelist);
		while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
		{
			char *subtype = "";
			if (filter->item_type == ITEM_WEAPON && filter->sub_type >= 0)
			{
				subtype = flag_string(weapon_class, filter->sub_type);
			}

			if (IS_NULLSTR(subtype))
				fprintf(fp, "Whitelist %s~\n",
					flag_string(type_flags, filter->item_type));
			else
				fprintf(fp, "Whitelist %s~ %s~\n",
					flag_string(type_flags, filter->item_type),
					subtype);
		}
		iterator_stop(&it);

		iterator_start(&it, CONTAINER(obj)->blacklist);
		while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
		{
			char *subtype = "";
			if (filter->item_type == ITEM_WEAPON && filter->sub_type >= 0)
			{
				subtype = flag_string(weapon_class, filter->sub_type);
			}

			if (IS_NULLSTR(subtype))
				fprintf(fp, "Blacklist %s~\n",
					flag_string(type_flags, filter->item_type));
			else
				fprintf(fp, "Blacklist %s~ %s~\n",
					flag_string(type_flags, filter->item_type),
					subtype);
		}
		iterator_stop(&it);

		if (CONTAINER(obj)->lock)
			save_object_lockstate(fp, CONTAINER(obj)->lock);

		fprintf(fp, "#-TYPECONTAINER\n");
	}

	if (IS_FLUID_CON(obj))
	{
		fprintf(fp, "#TYPEFLUIDCONTAINER\n");
		fprintf(fp, "Name %s~\n", fix_string(FLUID_CON(obj)->name));
		fprintf(fp, "Short %s~\n", fix_string(FLUID_CON(obj)->short_descr));
		fprintf(fp, "Flags %s\n", print_flags(FLUID_CON(obj)->flags));

		if (IS_VALID(FLUID_CON(obj)->liquid))
			fprintf(fp, "Liquid %s~\n", FLUID_CON(obj)->liquid->name);
		
		fprintf(fp, "Amount %d\n", FLUID_CON(obj)->amount);
		fprintf(fp, "Capacity %d\n", FLUID_CON(obj)->capacity);
		fprintf(fp, "RefillRate %d\n", FLUID_CON(obj)->refill_rate);
		fprintf(fp, "Poison %d\n", FLUID_CON(obj)->poison);
		fprintf(fp, "PoisonRate %d\n", FLUID_CON(obj)->poison_rate);

		if (FLUID_CON(obj)->lock)
			save_object_lockstate(fp, FLUID_CON(obj)->lock);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, FLUID_CON(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEFLUIDCONTAINER\n");
	}

	if (IS_FOOD(obj))
	{
		FOOD_BUFF_DATA *buff;

		fprintf(fp, "#TYPEFOOD\n");
		fprintf(fp, "Hunger %d\n", FOOD(obj)->hunger);
		fprintf(fp, "Fullness %d\n", FOOD(obj)->full);
		fprintf(fp, "Poison %d\n", FOOD(obj)->poison);

		iterator_start(&it, FOOD(obj)->buffs);
		while((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#FOODBUFF %d\n", buff->where);

			fprintf(fp, "Location %d\n", buff->location);
			fprintf(fp, "Modifier %d\n", buff->modifier);
			fprintf(fp, "Level %d\n", UMAX(0, buff->level));
			fprintf(fp, "Duration %d\n", UMAX(0, buff->duration));

			if (buff->bitvector != 0)		fprintf(fp, "BitVector %s\n", print_flags(buff->bitvector));
			if (buff->bitvector2 != 0)	fprintf(fp, "BitVector2 %s\n", print_flags(buff->bitvector2));

			fprintf(fp, "#-FOODBUFF\n");
		}
		iterator_stop(&it);
		
		fprintf(fp, "#-TYPEFOOD\n");
	}

	if (IS_FURNITURE(obj))
	{
		FURNITURE_COMPARTMENT *compartment;

		fprintf(fp, "#TYPEFURNITURE\n");
		fprintf(fp, "Flags %s\n", print_flags(FURNITURE(obj)->flags));
		fprintf(fp, "MainCompartment %d\n", FURNITURE(obj)->main_compartment);

		iterator_start(&it, FURNITURE(obj)->compartments);
		while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
		{
			fprintf(fp, "#COMPARTMENT\n");

			fprintf(fp, "Name %s~\n", fix_string(compartment->name));
			fprintf(fp, "ShortDesc %s~\n", fix_string(compartment->short_descr));
			fprintf(fp, "Description %s~\n", fix_string(compartment->description));

			fprintf(fp, "Flags %s\n", print_flags(compartment->flags));
			fprintf(fp, "MaxWeight %d\n", compartment->max_weight);
			fprintf(fp, "MaxOccupants %d\n", compartment->max_occupants);

			fprintf(fp, "Standing %s\n", print_flags(compartment->standing));
			fprintf(fp, "Hanging %s\n", print_flags(compartment->hanging));
			fprintf(fp, "Sitting %s\n", print_flags(compartment->sitting));
			fprintf(fp, "Resting %s\n", print_flags(compartment->resting));
			fprintf(fp, "Sleeping %s\n", print_flags(compartment->sleeping));

			fprintf(fp, "HealthRegen %d\n", compartment->health_regen);
			fprintf(fp, "ManaRegen %d\n", compartment->mana_regen);
			fprintf(fp, "MoveRegen %d\n", compartment->move_regen);
			
			if (compartment->lock)
				save_object_lockstate(fp, compartment->lock);

			fprintf(fp, "#-COMPARTMENT\n");
		}
		iterator_stop(&it);

		fprintf(fp, "#-TYPEFURNITURE\n");
	}

	if (IS_INK(obj))
	{
		fprintf(fp, "#TYPEINK\n");
		for(int i = 0; i < MAX_INK_TYPES; i++)
		{
			fprintf(fp, "Type %d %s~ %d\n", i+1,
				flag_string(catalyst_types, INK(obj)->types[i]), INK(obj)->amounts[i]);
		}
		fprintf(fp, "#-TYPEINK\n");
	}

	if (IS_INSTRUMENT(obj))
	{
		fprintf(fp, "#TYPEINSTRUMENT\n");

		fprintf(fp, "Type %s~\n", flag_string(instrument_types, INSTRUMENT(obj)->type));
		fprintf(fp, "Flags %s\n", print_flags(INSTRUMENT(obj)->flags));
		fprintf(fp, "Beats %d %d\n", INSTRUMENT(obj)->beats_min, INSTRUMENT(obj)->beats_max);
		fprintf(fp, "Mana %d %d\n", INSTRUMENT(obj)->mana_min, INSTRUMENT(obj)->mana_max);

		for(int i = 0; i < INSTRUMENT_MAX_CATALYSTS; i++)
		{
			fprintf(fp, "Reservoir %d %s~ %d %d\n", i+1,
				flag_string(catalyst_types, INSTRUMENT(obj)->reservoirs[i].type),
				INSTRUMENT(obj)->reservoirs[i].amount, INSTRUMENT(obj)->reservoirs[i].capacity);
		}
		fprintf(fp, "#-TYPEINSTRUMENT\n");
	}

	if (IS_JEWELRY(obj))
	{
		fprintf(fp, "#TYPEJEWELRY\n");

		fprintf(fp, "MaxMana %d\n", JEWELRY(obj)->max_mana);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, JEWELRY(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEJEWELRY\n");
	}

	if (IS_LIGHT(obj))
	{
		fprintf(fp, "#TYPELIGHT\n");
		fprintf(fp, "Flags %s\n", print_flags(LIGHT(obj)->flags));
		fprintf(fp, "Duration %d\n", LIGHT(obj)->duration);
		fprintf(fp, "#-TYPELIGHT\n");
	}

	if (IS_MAP(obj))
	{
		fprintf(fp, "#TYPEMAP\n");
		fprintf(fp, "Wilds %ld\n", MAP(obj)->wuid);
		fprintf(fp, "X %ld\n", MAP(obj)->x);
		fprintf(fp, "Y %ld\n", MAP(obj)->y);

		ITERATOR wit;
		WAYPOINT_DATA *wp;
		iterator_start(&wit, MAP(obj)->waypoints);
		while((wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)))
		{
			fprintf(fp, "Waypoint %lu %d %d %s~\n", wp->w, wp->x, wp->y, fix_string(wp->name));
		}
		iterator_stop(&wit);
		fprintf(fp, "#-TYPEMAP\n");
	}

	if (IS_MIST(obj))
	{
		fprintf(fp, "#TYPEMIST\n");
		fprintf(fp, "ObscureMobs %d\n", MIST(obj)->obscure_mobs);
		fprintf(fp, "ObscureObjs %d\n", MIST(obj)->obscure_objs);
		fprintf(fp, "ObscureRoom %d\n", MIST(obj)->obscure_room);

		fprintf(fp, "Icy %d\n", MIST(obj)->icy);
		fprintf(fp, "Fiery %d\n", MIST(obj)->fiery);
		fprintf(fp, "Acidic %d\n", MIST(obj)->acidic);
		fprintf(fp, "Stink %d\n", MIST(obj)->stink);
		fprintf(fp, "Wither %d\n", MIST(obj)->wither);
		fprintf(fp, "Toxic %d\n", MIST(obj)->toxic);
		fprintf(fp, "Shock %d\n", MIST(obj)->shock);
		fprintf(fp, "Fog %d\n", MIST(obj)->fog);
		fprintf(fp, "Sleep %d\n", MIST(obj)->sleep);

		fprintf(fp, "#-TYPEMIST\n");
	}

	if (IS_MONEY(obj))
	{
		fprintf(fp, "#TYPEMONEY\n");
		fprintf(fp, "Silver %d\n", MONEY(obj)->silver);
		fprintf(fp, "Gold %d\n", MONEY(obj)->gold);
		fprintf(fp, "#-TYPEMONEY\n");
	}

	if (IS_PAGE(obj))
	{
		fprintf(fp, "#TYPEPAGE %d\n", PAGE(obj)->page_no);
		fprintf(fp, "Title %s~\n", fix_string(PAGE(obj)->title));
		fprintf(fp, "Text %s~\n", fix_string(PAGE(obj)->text));
		if (PAGE(obj)->book.auid > 0 && PAGE(obj)->book.vnum > 0)
		{
			if (PAGE(obj)->book.auid == obj->area->uid)
				fprintf(fp, "Book #%ld\n", PAGE(obj)->book.vnum);
			else
				fprintf(fp, "Book %ld#%ld\n", PAGE(obj)->book.auid, PAGE(obj)->book.vnum);
		}
		fprintf(fp, "#-TYPEPAGE\n");
	}

	if (IS_PORTAL(obj))
	{
		fprintf(fp, "#TYPEPORTAL\n");
		fprintf(fp, "Name %s~\n", fix_string(PORTAL(obj)->name));
		fprintf(fp, "Short %s~\n", fix_string(PORTAL(obj)->short_descr));
		fprintf(fp, "Exit %s\n", print_flags(PORTAL(obj)->exit));
		fprintf(fp, "Flags %s\n", print_flags(PORTAL(obj)->flags));
		fprintf(fp, "Type %s~\n", flag_string(portal_gatetype, PORTAL(obj)->type));

		for(int i = 0; i < MAX_PORTAL_VALUES; i++)
			fprintf(fp, "Param %d %ld\n", i, PORTAL(obj)->params[i]);

		if (PORTAL(obj)->lock)
			save_object_lockstate(fp, PORTAL(obj)->lock);


		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, PORTAL(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "SpellNew %s~ %d %d\n", spell->skill->name, spell->level, spell->repop);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEPORTAL\n");
	}

	if (IS_SCROLL(obj))
	{
		fprintf(fp, "#TYPESCROLL\n");

		fprintf(fp, "MaxMana %d\n", SCROLL(obj)->max_mana);
		fprintf(fp, "Flags %s\n", print_flags(SCROLL(obj)->flags));

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, SCROLL(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPESCROLL\n");
	}

	if (IS_SEXTANT(obj))
	{
		SEXTANT_DATA *sextant = SEXTANT(obj);

		fprintf(fp, "#TYPESEXTANT\n");
		fprintf(fp, "Accuracy %d\n", sextant->accuracy);
		fprintf(fp, "#-TYPESEXTANT\n");
	}

	if (IS_TATTOO(obj))
	{
		fprintf(fp, "#TYPETATTOO\n");

		fprintf(fp, "Touches %d\n", TATTOO(obj)->touches);
		fprintf(fp, "FadeChance %d\n", TATTOO(obj)->fading_chance);
		fprintf(fp, "FadeRate %d\n", TATTOO(obj)->fading_rate);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, TATTOO(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPETATTOO\n");
	}

	if (IS_TELESCOPE(obj))
	{
		TELESCOPE_DATA *telescope = TELESCOPE(obj);
		fprintf(fp, "#TYPETELESCOPE\n");
		fprintf(fp, "Distance %d\n", telescope->distance);
		fprintf(fp, "MinDistance %d\n", telescope->min_distance);
		fprintf(fp, "MaxDistance %d\n", telescope->max_distance);
		fprintf(fp, "BonusView %d\n", telescope->bonus_view);
		if (telescope->heading >= 0)
			fprintf(fp, "Heading %d\n", telescope->heading);
		fprintf(fp, "#-TYPETELESCOPE\n");
	}

	if (IS_WAND(obj))
	{
		fprintf(fp, "#TYPEWAND\n");

		fprintf(fp, "MaxMana %d\n", WAND(obj)->max_mana);
		fprintf(fp, "Charges %d\n", WAND(obj)->charges);
		fprintf(fp, "MaxCharges %d\n", WAND(obj)->max_charges);
		fprintf(fp, "Cooldown %d\n", WAND(obj)->cooldown);
		fprintf(fp, "RechargeTime %d\n", WAND(obj)->recharge_time);

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, WAND(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEWAND\n");
	}

	if (IS_WEAPON(obj))
	{
		fprintf(fp, "#TYPEWEAPON\n");

		fprintf(fp, "WeaponClass %s~\n", flag_string(weapon_class, WEAPON(obj)->weapon_class));
		fprintf(fp, "MaxMana %d\n", WEAPON(obj)->max_mana);
		fprintf(fp, "Charges %d\n", WEAPON(obj)->charges);
		fprintf(fp, "MaxCharges %d\n", WEAPON(obj)->max_charges);
		fprintf(fp, "Cooldown %d\n", WEAPON(obj)->cooldown);
		fprintf(fp, "RechargeTime %d\n", WEAPON(obj)->recharge_time);

		if (WEAPON(obj)->ammo != AMMO_NONE)
			fprintf(fp, "Ammo %s~\n", flag_string(ammo_types, WEAPON(obj)->ammo));
		if (WEAPON(obj)->range > 0)
			fprintf(fp, "Range %d\n", WEAPON(obj)->range);

		for(int i = 0; i < MAX_ATTACK_POINTS; i++)
		{
			if (WEAPON(obj)->attacks[i].type >= 0)
			{
				fprintf(fp, "Attack %d %s~ %s~ %s~ %s %d %d %d\n", i,
					fix_string(WEAPON(obj)->attacks[i].name),
					fix_string(WEAPON(obj)->attacks[i].short_descr),
					attack_table[WEAPON(obj)->attacks[i].type].noun,
					print_flags(WEAPON(obj)->attacks[i].flags),
					WEAPON(obj)->attacks[i].damage.number,
					WEAPON(obj)->attacks[i].damage.size,
					WEAPON(obj)->attacks[i].damage.bonus);
			}
		}

		ITERATOR sit;
		SPELL_DATA *spell;
		iterator_start(&sit, WEAPON(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Spell %s~ %d\n", spell->skill->name, spell->level);
		}
		iterator_stop(&sit);

		fprintf(fp, "#-TYPEWEAPON\n");
	}


}

/* save one object */
void save_object_new(FILE *fp, OBJ_INDEX_DATA *obj)
{
	AFFECT_DATA *af;
	EXTRA_DESCR_DATA *ed;
	ITERATOR it;
	PROG_LIST *trigger;
	int i;

	/* hack to not save maps in the abyss as they are generated each reboot */
	if (!str_prefix("Maze-Level", obj->area->name) && obj->vnum == obj->area->max_vnum)
		return;

	fprintf(fp, "#OBJECT %ld\n", obj->vnum);
	fprintf(fp, "Name %s~\n", obj->name);
	fprintf(fp, "ShortDesc %s~\n", obj->short_descr);
	fprintf(fp, "LongDesc %s~\n", obj->description);
	fprintf(fp, "Description %s~\n", fix_string(obj->full_description));
	if (IS_VALID(obj->material))
		fprintf(fp, "Material %s~\n", obj->material->name);
	fprintf(fp, "ImpSig %s~\n", obj->imp_sig);
	if(obj->persist)
		fprintf(fp, "Persist\n");
	// Only save if the object is not in an immortal area
	if(obj->immortal && !IS_SET(obj->area->area_flags, AREA_IMMORTAL))
		fprintf(fp, "Immortal\n");
	fprintf(fp, "CreatorSig %s~\n", obj->creator_sig);
	fprintf(fp, "SKeywds %s~\n", obj->skeywds);
	fprintf(fp, "TimesAllowedFixed %d Fragility %d Points %d Update %d Timer %d\n", obj->times_allowed_fixed, obj->fragility, obj->points, obj->update, obj->timer);
	fprintf(fp, "ItemType %s~\n", item_name(obj->item_type));
	fprintf(fp, "ExtraFlags %ld\n", obj->extra[0]);
	fprintf(fp, "Extra2Flags %ld\n", obj->extra[1]);
	fprintf(fp, "Extra3Flags %ld\n", obj->extra[2]);
	fprintf(fp, "Extra4Flags %ld\n", obj->extra[3]);
	fprintf(fp, "WearFlags %ld\n", obj->wear_flags);

	if (IS_VALID(obj->clazz))
		fprintf(fp, "Class %s~\n", obj->clazz->name);
	
	if (obj->clazz_type != CLASS_NONE)
		fprintf(fp, "ClassType %s~\n", flag_string(class_types, obj->clazz_type));

	if (list_size(obj->race) > 0)
	{
		ITERATOR rit;
		RACE_DATA *race;
		iterator_start(&rit, obj->race);
		while((race = (RACE_DATA *)iterator_nextdata(&rit)))
		{
			fprintf(fp, "Race %s~\n", race->name);
		}
		iterator_stop(&rit);
	}

	fprintf(fp, "Values");
	for (i = 0; i < MAX_OBJVALUES; i++) fprintf(fp, " %ld", obj->value[i]);
	fprintf(fp, "\n");

	save_object_multityping(fp, obj);	

	fprintf(fp, "Level %d\n", obj->level);
	fprintf(fp, "Weight %d\n", obj->weight);
	fprintf(fp, "Cost %ld\n", obj->cost);
	fprintf(fp, "Condition %d\n", obj->condition);
	if(obj->comments)
		fprintf(fp, "Comments %s~\n", fix_string(obj->comments));

	#if 0
	if( obj->waypoints )
	{
		ITERATOR wit;
		WAYPOINT_DATA *wp;

		iterator_start(&wit, obj->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
		{
			fprintf(fp, "MapWaypoint %lu %d %d %s~\n", wp->w, wp->x, wp->y, fix_string(wp->name));
		}
		iterator_stop(&wit);
	}
	#endif

	// Affects
	for (af = obj->affected; af != NULL; af = af->next) {
		fprintf(fp, "#AFFECT %d\n", af->where);

		fprintf(fp, "Location %d\n", af->location);
		fprintf(fp, "Modifier %d\n", af->modifier);

		fprintf(fp, "Level %d\n", af->level);
		fprintf(fp, "Type %d\n", af->catalyst_type);
		if (IS_VALID(af->skill))
			fprintf(fp, "Skill %s~\n", af->skill->name);
		fprintf(fp, "Duration %d\n", af->duration);

		if (af->bitvector != 0)		fprintf(fp, "BitVector %ld\n", af->bitvector);
		if (af->bitvector2 != 0)	fprintf(fp, "BitVector2 %ld\n", af->bitvector2);

		fprintf(fp, "Random %d\n", af->random);
		fprintf(fp, "#-AFFECT\n");
	}

	// Catalysts
	for (af = obj->catalyst; af != NULL; af = af->next) {
		fprintf(fp, "#CATALYST %s\n", flag_string(catalyst_types,af->catalyst_type));

		if( af->where == TO_CATALYST_ACTIVE )
			fprintf(fp, "Active 1\n");

		if( !IS_NULLSTR(af->custom_name) )
			fprintf(fp, "Name %s\n", af->custom_name);

		fprintf(fp, "Charges %d\n", af->modifier);

		fprintf(fp, "Random %d\n", af->random);
		fprintf(fp, "#-CATALYST\n");
	}

	for (ed = obj->extra_descr; ed != NULL; ed = ed->next) {
		fprintf(fp, "#EXTRA_DESCR %s~\n", ed->keyword);
		if( ed->description )
			fprintf(fp, "Description %s~\n", fix_string(ed->description));
		else
			fprintf(fp, "Environmental\n");
		fprintf(fp, "#-EXTRA_DESCR\n");
	}

	if(obj->progs) {
		for(i = 0; i < TRIGSLOT_MAX; i++) {
			if(list_size(obj->progs[i]) > 0) {
				iterator_start(&it, obj->progs[i]);
				while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "ObjProg %s %s~ %s~\n", widevnum_string_wnum(trigger->wnum, obj->area), trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
				iterator_stop(&it);
			}
		}
	}

	olc_save_index_vars(fp, obj->index_vars, obj->area);

	if(obj->lock)
	{
		fprintf(fp, "Lock %s %d %d\n",
			widevnum_string_wnum(obj->lock->key_wnum, obj->area), obj->lock->flags, obj->lock->pick_chance);
	}

	// Save item spells here.
	if (obj->spells != NULL)
		save_spell(fp, obj->spells);

/*
	// Save objects with old spell format here.
	if (obj->spells == NULL)
		switch (obj->item_type) {
		case ITEM_ARMOUR:
		case ITEM_WEAPON:
		case ITEM_RANGED_WEAPON:
			if (obj->value[5] > 0) {
				if (obj->value[6] > 0 && obj->value[6] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[6]].name, obj->value[5], 100);
				}

				if (obj->value[7] > 0 && obj->value[7] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[7]].name, obj->value[5], 100);
				}

				obj->value[5] = 0;
				obj->value[6] = 0;
				obj->value[7] = 0;
			}
			break;

		case ITEM_LIGHT:
			if (obj->value[3] > 0) {
				if (obj->value[4] > 0 && obj->value[4] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[4]].name, obj->value[3], 100);
				}

				if (obj->value[5] > 0 && obj->value[5] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[5]].name, obj->value[3], 100);
				}

				obj->value[3] = 0;
				obj->value[4] = 0;
				obj->value[5] = 0;
			}
			break;

		case ITEM_ARTIFACT:
			if (obj->value[0] > 0) {
				if (obj->value[1] > 0 && obj->value[1] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[1]].name, obj->value[0], 100);
				}

				if (obj->value[2] > 0 && obj->value[2] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[2]].name, obj->value[0], 100);
				}

				obj->value[0] = 0;
				obj->value[1] = 0;
				obj->value[2] = 0;
			}
			break;

		case ITEM_SCROLL:
		case ITEM_PILL:
		case ITEM_POTION:
			if (obj->value[0] > 0) {
				if (obj->value[1] > 0 && obj->value[1] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[1]].name, obj->value[0], 100);
				}

				if (obj->value[2] > 0 && obj->value[2] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[2]].name, obj->value[0], 100);
				}

				if (obj->value[3] > 0 && obj->value[3] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[3]].name, obj->value[0], 100);
				}

				if (obj->value[4] > 0 && obj->value[4] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[4]].name, obj->value[0], 100);
				}

				obj->value[0] = 0;
				obj->value[1] = 0;
				obj->value[2] = 0;
				obj->value[3] = 0;
				obj->value[4] = 0;
			}

			break;

		case ITEM_WAND:
		case ITEM_STAFF:
			if (obj->value[0] > 0) {
				if (obj->value[3] > 0 && obj->value[3] < MAX_SKILL) {
					fprintf(fp, "SpellNew %s~ %ld %d\n",
					skill_table[obj->value[3]].name, obj->value[0], 100);
				}

				obj->value[0] = 0;
				obj->value[3] = 0;
			}

			break;

		case ITEM_PORTAL:
			break;
	}
*/
	fprintf(fp, "#-OBJECT\n");
}

void save_spell(FILE *fp, SPELL_DATA *spell)
{
    if (spell->next != NULL)
		save_spell(fp, spell->next);

    fprintf(fp, "SpellNew %s~ %d %d\n", spell->skill->name, spell->level, spell->repop);
}

void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type)
{
	fprintf(fp, "#%sPROG %ld\n", type, (long int)scr->vnum);
	fprintf(fp, "Name %s~\n", scr->name ? scr->name : "");
	fprintf(fp, "Code %s~\n", fix_string(scr->code ? scr->src : scr->edit_src));
	fprintf(fp, "Flags %s~\n", flag_string(script_flags, scr->flags));
	fprintf(fp, "Depth %d\n", scr->depth);
	fprintf(fp, "Security %d\n", scr->security);
	if(scr->comments)
		fprintf(fp, "Comments %s~\n", fix_string(scr->comments));
	fprintf(fp, "#-%sPROG\n", type);
}


/* save all scripts of an area */
void save_scripts_new(FILE *fp, AREA_DATA *area)
{
    SCRIPT_DATA *scr;

    // rooms
	for(scr = area->rprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "ROOM");

    // mobiles
	for(scr = area->mprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "MOB");

	// objects
	for(scr = area->oprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "OBJ");

    // tokens
	for(scr = area->tprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "TOKEN");

	// areas
	for(scr = area->aprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "AREA");

	// instances
	for(scr = area->iprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "INSTANCE");

	// dungeons
	for(scr = area->dprog_list; scr; scr = scr->next)
		save_script_new(fp, area, scr, "DUNGEON");
}

void save_missionary_new(FILE *fp, MISSIONARY_DATA *missionary, AREA_DATA *pRefArea)
{
    fprintf(fp, "#MISSIONARY\n");
	if (!pRefArea || missionary->scroll.auid != pRefArea->uid)
	    fprintf(fp, "Scroll %ld#%ld\n", missionary->scroll.auid, missionary->scroll.vnum);
	else
	    fprintf(fp, "Scroll #%ld\n", missionary->scroll.vnum);
	
    fprintf(fp, "Keywords %s~\n", fix_string(missionary->keywords));
    fprintf(fp, "ShortDescr %s~\n", fix_string(missionary->short_descr));
    fprintf(fp, "LongDescr %s~\n", fix_string(missionary->long_descr));
    fprintf(fp, "Header %s~\n", fix_string(missionary->header));
    fprintf(fp, "Footer %s~\n", fix_string(missionary->footer));
    fprintf(fp, "Prefix %s~\n", fix_string(missionary->prefix));
    fprintf(fp, "Suffix %s~\n", fix_string(missionary->suffix));
    fprintf(fp, "LineWidth %d\n", missionary->line_width);
    fprintf(fp, "#-MISSIONARY\n");
}

void save_practice_cost_data(FILE *fp, PRACTICE_COST_DATA *data, AREA_DATA *area)
{
	fprintf(fp, "#COST\n");
	fprintf(fp, "MinRating %d\n", data->min_rating);

	fprintf(fp, "Silver %ld\n", data->silver);
	fprintf(fp, "Practices %ld\n", data->practices);
	fprintf(fp, "Trains %ld\n", data->trains);
	fprintf(fp, "QuestPnts %ld\n", data->qp);
	fprintf(fp, "DeityPnts %ld\n", data->dp);
	fprintf(fp, "Pneuma %ld\n", data->pneuma);
	fprintf(fp, "RepPoints %ld\n", data->rep_points);
	fprintf(fp, "Paragon %ld\n", data->paragon_levels);
	if (!IS_NULLSTR(data->custom_price) && data->check_price)
	{
		fprintf(fp, "CustomPricing %s~ %s\n", fix_string(data->custom_price), widevnum_string(data->check_price->area, data->check_price->vnum, area));	}

	if (data->obj)
		fprintf(fp, "Object %s\n", widevnum_string(data->obj->area, data->obj->vnum, area));

	if (IS_VALID(data->reputation))
	{
		fprintf(fp, "Reputation %s\n", widevnum_string(data->reputation->area, data->reputation->vnum, area));
	}

	fprintf(fp, "#-COST\n");
}

void save_practice_entry_data(FILE *fp, PRACTICE_ENTRY_DATA *data, AREA_DATA *area)
{
	fprintf(fp, "#ENTRY\n");

	if (IS_VALID(data->skill))
		fprintf(fp, "Skill %s~\n", data->skill->name);
	
	if (IS_VALID(data->song))
		fprintf(fp, "Song %s~\n", data->song->name);

	if (IS_VALID(data->reputation))
		fprintf(fp, "Reputation %s %d %d\n", widevnum_string(data->reputation->area, data->reputation->vnum, area), data->min_reputation_rank, data->max_reputation_rank);

	fprintf(fp, "Flags %s\n", print_flags(data->flags));
	fprintf(fp, "MaxRating %d\n", data->max_rating);

	if (data->check_script)
		fprintf(fp, "CheckScript %s\n", widevnum_string(data->check_script->area, data->check_script->vnum, area));

	ITERATOR it;
	PRACTICE_COST_DATA *cost;
	iterator_start(&it, data->costs);
	while((cost = (PRACTICE_COST_DATA *)iterator_nextdata(&it)))
	{
		save_practice_cost_data(fp, cost, area);
	}
	iterator_stop(&it);

	fprintf(fp, "#-ENTRY\n");
}

void save_practice_data(FILE *fp, PRACTICE_DATA *data, AREA_DATA *area)
{
	fprintf(fp, "#PRACTICE\n");
	if (data->standard)
		fprintf(fp, "Standard\n");
	else
	{
		ITERATOR it;
		PRACTICE_ENTRY_DATA *entry;
		iterator_start(&it, data->entries);
		while((entry = (PRACTICE_ENTRY_DATA *)iterator_nextdata(&it)))
		{
			save_practice_entry_data(fp, entry, area);
		}
		iterator_stop(&it);
	}
	fprintf(fp, "#-PRACTICE\n");
}

void save_shop_stock_new(FILE *fp, SHOP_STOCK_DATA *stock, AREA_DATA *pRefArea)
{
	if(stock->next)
		save_shop_stock_new(fp, stock->next, pRefArea);

	fprintf(fp, "#STOCK\n");

	fprintf(fp, "Level %d\n", stock->level);
	fprintf(fp, "Discount %d\n", stock->discount);

	// Pricing
	fprintf(fp, "Silver %ld\n", stock->silver);
	fprintf(fp, "QuestPnts %ld\n", stock->qp);
	fprintf(fp, "DeityPnts %ld\n", stock->dp);
	fprintf(fp, "Pneuma %ld\n", stock->pneuma);
	fprintf(fp, "RepPoints %ld\n", stock->rep_points);
	fprintf(fp, "Paragon %ld\n", stock->paragon_levels);
	if (!IS_NULLSTR(stock->custom_price) && stock->check_price)
		fprintf(fp, "CustomPricing %s~ %s\n", fix_string(stock->custom_price), widevnum_string(stock->check_price->area, stock->check_price->vnum, pRefArea));

	// Quantity
	fprintf(fp, "Quantity %d\n", stock->quantity);
	fprintf(fp, "RestockRate %d\n", stock->restock_rate);
	if(stock->singular)
	{
		fprintf(fp, "Singular\n");
	}

	// Product
	switch(stock->type) {
	case STOCK_OBJECT:
		fprintf(fp, "Object %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_PET:
		fprintf(fp, "Pet %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_MOUNT:
		fprintf(fp, "Mount %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_GUARD:
		fprintf(fp, "Guard %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_CREW:
		fprintf(fp, "Crew %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_SHIP:
		fprintf(fp, "Ship %s\n", widevnum_string_wnum(stock->wnum, pRefArea));
		break;
	case STOCK_CUSTOM:
		fprintf(fp, "Keyword %s~\n", fix_string(stock->custom_keyword));
		break;
	}

	fprintf(fp, "Duration %d\n", stock->duration);

	fprintf(fp, "Description %s~\n", fix_string(stock->custom_descr));

	if (IS_VALID(stock->reputation))
	{
		if (stock->min_show_rank > 0 || stock->max_show_rank > 0)
			fprintf(fp, "ReputationShow %ld#%ld %d %d %d %d\n", stock->reputation->area->uid, stock->reputation->vnum, stock->min_reputation_rank, stock->max_reputation_rank, stock->min_show_rank, stock->max_show_rank);
		else
			fprintf(fp, "Reputation %ld#%ld %d %d\n", stock->reputation->area->uid, stock->reputation->vnum, stock->min_reputation_rank, stock->max_reputation_rank);
	}

	fprintf(fp, "#-STOCK\n");
}

void save_shop_new(FILE *fp, SHOP_DATA *shop, AREA_DATA *pRefArea)
{
    int i;

    fprintf(fp, "#SHOP\n");
    fprintf(fp, "Keeper %ld\n", shop->keeper);
    fprintf(fp, "ProfitBuy %d\n", shop->profit_buy);
    fprintf(fp, "ProfitSell %d\n", shop->profit_sell);
    fprintf(fp, "HourOpen %d\n", shop->open_hour);
    fprintf(fp, "HourClose %d\n", shop->close_hour);
    fprintf(fp, "RestockInterval %d\n", shop->restock_interval);
    if(shop->flags != 0)
    {
	    fprintf(fp, "Flags %d\n", shop->flags);
	}

	fprintf(fp, "Discount %d\n", shop->discount);

    for (i = 0; i < MAX_TRADE; i++) {
		if (shop->buy_type[i] != 0)
		    fprintf(fp, "Trade %d\n", shop->buy_type[i]);
    }

    if( shop->shipyard > 0 )
    {
		fprintf(fp, "Shipyard %ld %d %d %d %d %s~\n",
			shop->shipyard,
			shop->shipyard_region[0][0],
			shop->shipyard_region[0][1],
			shop->shipyard_region[1][0],
			shop->shipyard_region[1][1],
			shop->shipyard_description);
	}

	if (IS_VALID(shop->reputation))
	{
		fprintf(fp, "Reputation %ld#%ld %d\n", shop->reputation->area->uid, shop->reputation->vnum, shop->min_reputation_rank);
	}

	if( shop->stock )
		save_shop_stock_new(fp, shop->stock, pRefArea);


    fprintf(fp, "#-SHOP\n");
}

void save_ship_crew_index_new(FILE *fp, SHIP_CREW_INDEX_DATA *crew)
{
	fprintf(fp, "#CREW\n");
	fprintf(fp, "MinRank %d\n", crew->min_rank);
	fprintf(fp, "Scouting %d\n", crew->scouting);
	fprintf(fp, "Gunning %d\n", crew->gunning);
	fprintf(fp, "Oarring %d\n", crew->oarring);
	fprintf(fp, "Mechanics %d\n", crew->mechanics);
	fprintf(fp, "Navigation %d\n", crew->navigation);
	fprintf(fp, "Leadership %d\n", crew->leadership);
	fprintf(fp, "#-CREW\n");
}

void read_area_region(FILE *fp, AREA_DATA *area)
{
	AREA_REGION *region;
	long uid;
 	char buf[MSL];

	uid = fread_number(fp);

	if (!uid)	// Default region
		region = &area->region;
	else
	{
		region = new_area_region();
		region->uid = uid;
	}

	region->area = area;

	while (str_cmp((word = fread_word(fp)), "#-REGION"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("AirshipLand",		region->airship_land_spot,	fread_number(fp));
				break;

			case 'C':
				KEYS("Comments",		region->comments,			fread_string(fp));
				break;
			
			case 'D':
				KEYS("Description",		region->description,		fread_string(fp));
				break;

			case 'F':
				KEY("Flags",			region->flags,				fread_number(fp));
				break;

			case 'N':
				KEYS("Name",			region->name,				fread_string(fp));
				break;

			case 'P':
				KEY("Place",			region->place_flags,		fread_number(fp));
				KEY("PostOffice",		region->post_office,		fread_number(fp));
				break;

			case 'R':
				if(!str_cmp(word, "Recall"))
				{
					location_clear(&region->recall);

					region->recall.id[0] = fread_number(fp);

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "RecallW"))
				{
					location_clear(&region->recall);

					region->recall.wuid = fread_number(fp);
					region->recall.id[0] = fread_number(fp);
					region->recall.id[1] = fread_number(fp);
					region->recall.id[2] = fread_number(fp);

					fMatch = true;
					break;
				}
				break;
			
			case 'S':
				KEY("Savage",			region->savage_level,		fread_number(fp));
				break;

			case 'W':
				KEY("Who",				region->area_who,			fread_number(fp));
				break;

			case 'X':
				KEY("XCoord",			region->x,					fread_number(fp));
				KEY("XLand",			region->land_x,				fread_number(fp));
				break;
			
			case 'Y':
				KEY("YCoord",			region->y,					fread_number(fp));
				KEY("YLand",			region->land_y,				fread_number(fp));
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "read_area_region: no match for word %s", word);
			bug(buf, 0);
		}
	}

	if (uid > 0)
	{
		list_appendlink(area->regions, region);
		if(uid > area->top_region_uid)
			area->top_region_uid = uid;
	}
}

AREA_DATA *read_area_new(FILE *fp)
{
    AREA_DATA *area = NULL;
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *mob;
    OBJ_INDEX_DATA *obj;
    SCRIPT_DATA *rpr, *mpr, *opr, *tpr, *apr, *dpr, *ipr;
    TOKEN_INDEX_DATA *token;
    char buf[MSL];
    long vnum;
    int iHash;
    int dummy;

    if (fp == NULL)
    {
	bug("read_area_new: fp null", 0);
	return NULL;
    }

    if (str_cmp(word = fread_word(fp) , "#AREA"))
    {
	bug("read_area_new: bad format", 0);
	return NULL;
    }

    area = new_area();
    area->name = fread_string(fp);
    area->version_area =	VERSION_AREA_000;
    area->version_mobile =	VERSION_MOBILE_000;
    area->version_object =	VERSION_OBJECT_000;
    area->version_room =	VERSION_ROOM_000;
    area->version_token =	VERSION_TOKEN_000;
    area->version_script =	VERSION_SCRIPT_000;
    area->version_wilds =	VERSION_WILDS_000;
	area->version_blueprints = VERSION_BLUEPRINT_000;
	area->version_ships = VERSION_SHIP_000;
	area->version_dungeons = VERSION_DUNGEON_000;

    while (str_cmp((word = fread_word(fp)), "#-AREA"))
    {
	fMatch = false;

	//log_string(word);

	switch (word[0])
	{
	    case '#':
		if (!str_cmp(word, "#REPUTATION"))
		{
			REPUTATION_INDEX_DATA *rep = load_reputation_index(fp, area);
			vnum = rep->vnum;
			iHash = vnum % MAX_KEY_HASH;
			rep->next = area->reputation_index_hash[iHash];
			area->reputation_index_hash[iHash] = rep;
			area->bottom_reputation_vnum = UMIN(area->bottom_reputation_vnum, vnum);
			area->top_reputation_vnum = UMAX(area->top_reputation_vnum, vnum);
			fMatch = true;
		}
		else if (!str_cmp(word, "#REGION"))
		{
			read_area_region(fp, area);
			fMatch = true;
		}
		else if (!str_cmp(word, "#ROOM"))
		{
		    room = read_room_new(fp, area, ROOMTYPE_NORMAL);
		    vnum = room->vnum;
		    iHash                   = vnum % MAX_KEY_HASH;
		    room->next        = area->room_index_hash[iHash];
		    room->area = area;
		    list_appendlink(area->room_list, room);	// Add to the area room list
		    area->room_index_hash[iHash]  = room;
		    area->top_room++;
		    area->bottom_vnum_room = UMIN(area->bottom_vnum_room, vnum); /* OLC */
		    area->top_vnum_room = UMAX(area->top_vnum_room, vnum); /* OLC */
			fMatch = true;
		}
		else if (!str_cmp(word, "#MOBILE"))
		{
		    mob = read_mobile_new(fp, area);
		    vnum = mob->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    mob->next = area->mob_index_hash[iHash];
		    area->mob_index_hash[iHash] = mob;
		    mob->area = area;
		    area->top_mob_index++;
			area->bottom_vnum_mob = UMIN(area->bottom_vnum_mob, vnum);
		    area->top_vnum_mob = UMAX(area->top_vnum_mob, vnum);
			fMatch = true;
		}
		else if (!str_cmp( word, "#TRADE"	) ) {
			load_area_trade( area, fp );
		  fMatch = true;
    }
		else if (!str_cmp(word, "#OBJECT"))
		{
		    obj = read_object_new(fp, area);
		    vnum = obj->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    obj->next = area->obj_index_hash[iHash];
		    area->obj_index_hash[iHash] = obj;
		    obj->area = area;
		    area->top_obj_index++;
			area->bottom_vnum_obj = UMIN(area->bottom_vnum_obj, vnum);
		    area->top_vnum_obj = UMAX(area->top_vnum_obj, vnum);
			fMatch = true;
		}
		else if (!str_cmp(word, "#TOKEN"))
		{
		    token = read_token(fp, area);
		    vnum = token->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    token->next = area->token_index_hash[iHash];
		    area->token_index_hash[iHash] = token;
		    token->area = area;
			fMatch = true;
		}
		else if (!str_cmp(word, "#SECTION"))
		{
			BLUEPRINT_SECTION *bs = load_blueprint_section(fp, area);
			int iHash = bs->vnum % MAX_KEY_HASH;

			bs->next = area->blueprint_section_hash[iHash];
			area->blueprint_section_hash[iHash] = bs;

			bs->area = area;
			area->bottom_blueprint_section_vnum = UMIN(area->bottom_blueprint_section_vnum, bs->vnum);
			area->top_blueprint_section_vnum = UMAX(area->top_blueprint_section_vnum, bs->vnum);
			fMatch = true;
		}
		else if( !str_cmp(word, "#BLUEPRINT") )
		{
			BLUEPRINT *bp = load_blueprint(fp, area);
			int iHash = bp->vnum % MAX_KEY_HASH;

			bp->next = area->blueprint_hash[iHash];
			area->blueprint_hash[iHash] = bp;

			bp->area = area;
			area->bottom_blueprint_vnum = UMIN(area->bottom_blueprint_vnum, bp->vnum);
			area->top_blueprint_vnum = UMAX(area->top_blueprint_vnum, bp->vnum);
			fMatch = true;
		}
		else if( !str_cmp(word, "#SHIP") )
		{
			SHIP_INDEX_DATA *ship = read_ship_index(fp, area);
			int iHash = ship->vnum % MAX_KEY_HASH;

			ship->next = area->ship_index_hash[iHash];
			area->ship_index_hash[iHash] = ship;

			ship->area = area;
			area->bottom_ship_vnum = UMIN(area->bottom_ship_vnum, ship->vnum);
			area->top_ship_vnum = UMAX(area->top_ship_vnum, ship->vnum);
			fMatch = true;
		}
		else if( !str_cmp(word, "#DUNGEON") )
		{
			DUNGEON_INDEX_DATA *dng = load_dungeon_index(fp, area);
			int iHash = dng->vnum % MAX_KEY_HASH;

			dng->next = area->dungeon_index_hash[iHash];
			area->dungeon_index_hash[iHash] = dng;

			dng->area = area;
			area->bottom_dungeon_vnum = UMIN(area->bottom_dungeon_vnum, dng->vnum);
			area->top_dungeon_vnum = UMAX(area->top_dungeon_vnum, dng->vnum);
			fMatch = true;
		}
		else if (!str_cmp(word, "#QUEST"))
		{
			QUEST_INDEX_DATA *quest = read_quest(fp, area);
			int iHash = quest->vnum % MAX_KEY_HASH;

			quest->next = area->quest_index_hash[iHash];
			area->quest_index_hash[iHash] = quest;

			quest->area = area;
			area->bottom_quest_vnum = UMIN(area->bottom_quest_vnum, quest->vnum);
			area->top_quest_vnum = UMAX(area->top_quest_vnum, quest->vnum);
			fMatch = true;
		}
		else if (!str_cmp(word, "#ROOMPROG"))
		{
		    rpr = read_script_new(fp, area, IFC_R);
		    if(rpr) {
				rpr->next = area->rprog_list;
				area->rprog_list = rpr;

				area->bottom_rprog_index = UMIN(area->bottom_rprog_index, rpr->vnum);
				area->top_rprog_index = UMAX(area->top_rprog_index, rpr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#MOBPROG"))
		{
		    mpr = read_script_new(fp, area, IFC_M);
		    if(mpr) {
			    mpr->next = area->mprog_list;
			    area->mprog_list = mpr;

				area->bottom_mprog_index = UMIN(area->bottom_mprog_index, mpr->vnum);
				area->top_mprog_index = UMAX(area->top_mprog_index, mpr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#OBJPROG"))
		{
		    opr = read_script_new(fp, area, IFC_O);
		    if(opr) {
				opr->next = area->oprog_list;
				area->oprog_list = opr;

				area->bottom_oprog_index = UMIN(area->bottom_oprog_index, opr->vnum);
				area->top_oprog_index = UMAX(area->top_oprog_index, opr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#TOKENPROG"))
		{
		    tpr = read_script_new(fp, area, IFC_T);
		    if(tpr) {
				tpr->next = area->tprog_list;
				area->tprog_list = tpr;

				area->bottom_tprog_index = UMIN(area->bottom_tprog_index, tpr->vnum);
				area->top_tprog_index = UMAX(area->top_tprog_index, tpr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#AREAPROG"))
		{
		    apr = read_script_new(fp, area, IFC_A);
		    if(apr) {
				apr->next = area->aprog_list;
				area->aprog_list = apr;

				area->bottom_aprog_index = UMIN(area->bottom_aprog_index, apr->vnum);
				area->top_aprog_index = UMAX(area->top_aprog_index, apr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#DUNGEONPROG"))
		{
		    dpr = read_script_new(fp, area, IFC_D);
		    if(dpr) {
				dpr->next = area->dprog_list;
				area->dprog_list = dpr;

				area->bottom_dprog_index = UMIN(area->bottom_dprog_index, dpr->vnum);
				area->top_dprog_index = UMAX(area->top_dprog_index, dpr->vnum);
		    }
			fMatch = true;
		}
		else if (!str_cmp(word, "#INSTANCEPROG"))
		{
		    ipr = read_script_new(fp, area, IFC_I);
		    if(ipr) {
				ipr->next = area->iprog_list;
				area->iprog_list = ipr;

				area->bottom_iprog_index = UMIN(area->bottom_iprog_index, ipr->vnum);
				area->top_iprog_index = UMAX(area->top_iprog_index, ipr->vnum);
		    }
			fMatch = true;
		}
		/* VIZZWILDS */
		else if (!str_cmp(word, "#WILDS"))
		{
		    fMatch = true;
		    load_wilds(fp, area);
		}
		else
		{
		    bug("read_area_new: bad module name", 0);
		    bug(word, 0);
		}

		break;

	    case 'A':
		KEY("AreaFlags",	area->area_flags,	fread_number(fp));

		if (!str_cmp(word, "AreaProg")) {
		    char *p;

			WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
		    p = fread_string(fp);

			struct trigger_type *tt = get_trigger_type(p, PRG_APROG);
		    if(!tt) {
			    sprintf(buf, "read_area_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    PROG_LIST *apr = new_trigger();

			    apr->wnum_load = wnum_load;
			    apr->trig_type = tt->type;
			    apr->trig_phrase = fread_string(fp);
			    if( tt->type == TRIG_SPELLCAST ) {
					char buf[MIL];
					SKILL_DATA *sk = get_skill_data(apr->trig_phrase);

					if( !IS_VALID(sk) ) {
						sprintf(buf, "read_area_new: invalid spell '%s' for TRIG_SPELLCAST", apr->trig_phrase);
						bug(buf, 0);
						free_trigger(apr);
						fMatch = true;
						break;
					}

					free_string(apr->trig_phrase);
					sprintf(buf, "%d", sk->uid);
					apr->trig_phrase = str_dup(buf);
					apr->trig_number = sk->uid;
					apr->numeric = true;

				} else {
			    	apr->trig_number = atoi(apr->trig_phrase);
					apr->numeric = is_number(apr->trig_phrase);
				}

			    if(!area->progs->progs) area->progs->progs = new_prog_bank();

				list_appendlink(area->progs->progs[tt->slot], apr);
				trigger_type_add_use(tt);
		    }
		    fMatch = true;
		}


		KEY("AreaWhoFlags",	dummy,	fread_number(fp));
		KEY("AreaWhoFlags2",	dummy,	fread_number(fp));
		KEY("AreaWho",		area->region.area_who,	fread_number(fp));
		KEY("AirshipLand",	area->region.airship_land_spot, fread_number(fp));
		break;

	    case 'B':
		KEYS("Builders",	area->builders,		fread_string(fp));
		break;

	    case 'C':
		KEYS("Comments", area->comments,	fread_string(fp));
		KEYS("Credits",	area->credits,		fread_string(fp));
		break;

		case 'D':
		KEYS("Description", area->description, fread_string(fp));
		break;

	    case 'F':
	        KEYS("FileName",	area->file_name,	fread_string(fp));
		break;

	    case 'O':
		KEY("Open",		area->open,		fread_number(fp));
		break;

	    case 'P':
	        KEY("PostOffice",	area->region.post_office,	fread_number(fp));
		KEY("PlaceType",	area->region.place_flags,	fread_number(fp));
		break;

	    case 'R':
		if (!str_cmp(word, "Recall")) {
			location_set(&area->region.recall,area,0,fread_number(fp),0,0);
			fMatch = true;
		}
		if (!str_cmp(word, "RecallW")) {
			location_set(&area->region.recall,NULL,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
			fMatch = true;
		}
		KEY("Repop",		area->repop,		fread_number(fp));
		break;

	    case 'S':
		KEY("Security",	area->security,		fread_number(fp));
		break;

		case 'T':
			KEY("TopRegionUID", area->top_region_uid, fread_number(fp));
			break;
/* VIZZWILDS */
            case 'U':
                KEY ("UID", area->uid, fread_number (fp));

	    case 'V':
			if (olc_load_index_vars(fp, word, &area->index_vars, area))
			{
				fMatch = true;
				break;
			}

		KEY("VersArea", area->version_area, fread_number(fp));
		KEY("VersMobile", area->version_mobile, fread_number(fp));
		KEY("VersObject", area->version_object, fread_number(fp));
		KEY("VersRoom", area->version_room, fread_number(fp));
		KEY("VersToken", area->version_token, fread_number(fp));
		KEY("VersScript", area->version_script, fread_number(fp));
		KEY("VersWilds", area->version_wilds, fread_number(fp));
		KEY("VersBlueprint", area->version_blueprints, fread_number(fp));
		KEY("VersShip", area->version_ships, fread_number(fp));
		KEY("VersDungeon", area->version_dungeons, fread_number(fp));
		if (!str_cmp(word, "VNUMs")) {
		    area->min_vnum = fread_number(fp);
		    area->max_vnum = fread_number(fp);
		    fMatch = true;
		}

		break;
		case 'W':
		KEY("WildsVnum",	area->wilds_uid, fread_number(fp));

	    case 'X':
		KEY("XCoord",		area->region.x,		fread_number(fp));
		KEY("XLand",		area->region.land_x,		fread_number(fp));

		break;

	    case 'Y':
		KEY("YCoord",		area->region.y,		fread_number(fp));
		KEY("YLand",		area->region.land_y,		fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_area_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

	variable_copylist(&area->index_vars,&area->progs->vars,false);

    if (IS_SET(area->area_flags, AREA_CHANGED))
		REMOVE_BIT(area->area_flags, AREA_CHANGED);

	if( area->version_area < VERSION_AREA_002 )
	{
		if( !str_cmp(area->name, "Realm of Alendith") )
			SET_BIT(area->area_flags, AREA_NEWBIE);
	}

	if( area->version_area < VERSION_AREA_003 )
	{
		// Handled after all areas are loaded, since there can be cross area handling
	}

	/*
	if (!area->wilds_uid && area->airship_land_spot > 0)
	{
		area->wilds_uid = 6;	// The overworld wilderness
	}*/

    if (area->uid == 0)
    {
        area->uid = gconfig.next_area_uid++;
		gconfig_write();
    }

	(void)dummy;	// Attempt to keep it from triggering -Wunused-but-set-variable

    return area;
}

void assign_area_vnum_new(AREA_DATA *area, long vnum)
{
    if (area->min_vnum == 0 || area->max_vnum == 0)
        area->min_vnum = area->max_vnum = vnum;
    if (vnum != URANGE(area->min_vnum, vnum, area->max_vnum)) {
        if (vnum < area->min_vnum)
            area->min_vnum = vnum;
        else
            area->max_vnum = vnum;
    }
}

/* read one room into an area */
ROOM_INDEX_DATA *read_room_new(FILE *fp, AREA_DATA *area, int recordtype)
{
    ROOM_INDEX_DATA *room = NULL;
    EXTRA_DESCR_DATA *ed;
    CONDITIONAL_DESCR_DATA *cd;
    EXIT_DATA *ex;
    RESET_DATA *reset;
    PROG_LIST *rpr;

    room = new_room_index();
/* VIZZWILDS */
    if (recordtype != ROOMTYPE_TERRAIN)
    {
        room->vnum = fread_number(fp);

		area->bottom_vnum_room = UMIN(area->bottom_vnum_room, room->vnum);
		area->top_vnum_room = UMAX(area->top_vnum_room, room->vnum);
    }
    room->persist = false;

    while (str_cmp((word = fread_word(fp)), "#-ROOM")
           && str_cmp(word, "#-TERRAIN")) {
	fMatch = false;

	switch(word[0]) {
	    case '#':
		if (!str_cmp(word, "#EXTRA_DESCR")) {
		    ed = read_extra_descr_new(fp);
		    ed->next = room->extra_descr;
		    room->extra_descr = ed;
		    fMatch = true;
		}
		else if (!str_cmp(word, "#CONDITIONAL_DESCR")) {
		    cd = read_conditional_descr_new(fp);
		    cd->next = room->conditional_descr;
		    room->conditional_descr = cd;
		    fMatch = true;
		}
		else if (!str_cmp(word, "#X")) { // finish here
		    ex = read_exit_new(fp, area);
		    if( IS_SET(ex->exit_info, EX_VLINK) )
		    {
				// discard VLINK exits
				free_exit(ex);
			}
			else
			{
		    	room->exit[ex->orig_door] = ex;
		    	ex->from_room = room;
			}
		    fMatch = true;
		}
		else if (!str_cmp(word, "#RESET")) {
	   	    reset = read_reset_new(fp, area);
		    new_reset(room, reset);
		    fMatch = true;
		}

		break;
		case 'B':
			if (!str_cmp(word, "Blueprint")) {
				room->w = 0;
				room->x = fread_number(fp);
				room->y = fread_number(fp);
				room->z = fread_number(fp);
				fMatch = true;
			}
			break;

		case 'C':
			KEYS("Comments", room->comments, fread_string(fp));
		break;

	    case 'D':
	        KEYS("Description", room->description, fread_string(fp));
		break;
	    case 'H':
	        KEY("HealRate",	room->heal_rate, 	fread_number(fp));
		KEYS("Home_owner", room->home_owner,	fread_string(fp));
		break;

	    case 'M':
		KEY("ManaRate",	room->mana_rate,	fread_number(fp));
		KEY("MoveRate",	room->move_rate,	fread_number(fp));
		break;

	    case 'N':
	        KEYS("Name",	room->name,	fread_string(fp));
		break;

	    case 'O':
	        KEYS("Owner",	room->owner,	fread_string(fp));
		break;
		case 'P':
			if(!str_cmp(word, "Persist")) {
				room->persist = true;

				fMatch = true;
				break;
			}
			break;

	    case 'R':
			if(!str_cmp(word, "Region"))
			{
				long uid = fread_number(fp);

				if (IS_VALID(room->region))
					list_remlink(room->region->rooms, room, false);

				room->region = get_area_region_by_uid(area, uid);

				if (IS_VALID(room->region))
					list_appendlink(room->region->rooms, room);


				fMatch = true;
				break;
			}
		KEY("Room_flags", 	room->room_flag[0], 	fread_number(fp));
		KEY("Room2_flags", 	room->room_flag[1], 	fread_number(fp));

		if (!str_cmp(word, "RoomProg")) {
		    char *p;

		    WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
		    p = fread_string(fp);

		    struct trigger_type *tt = get_trigger_type(p, PRG_RPROG);
		    if(!tt) {
			    sprintf(buf, "read_room_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    rpr = new_trigger();

			    rpr->wnum_load = wnum_load;
			    rpr->trig_type = tt->type;
			    rpr->trig_phrase = fread_string(fp);
			    if( tt->type == TRIG_SPELLCAST ) {
					char buf[MIL];
					SKILL_DATA *sk = get_skill_data(rpr->trig_phrase);
					
					if( !IS_VALID(sk) ) {
						sprintf(buf, "read_room_new: invalid spell '%s' for TRIG_SPELLCAST", rpr->trig_phrase);
						bug(buf, 0);
						free_trigger(rpr);
						fMatch = true;
						break;
					}

					free_string(rpr->trig_phrase);
					sprintf(buf, "%d", sk->uid);
					rpr->trig_phrase = str_dup(buf);
					rpr->trig_number = sk->uid;
					rpr->numeric = true;

				} else {
			    	rpr->trig_number = atoi(rpr->trig_phrase);
					rpr->numeric = is_number(rpr->trig_phrase);
				}
			    //SET_BIT(room->rprog_flags, rpr->trig_type);

			    if(!room->progs->progs) room->progs->progs = new_prog_bank();

				list_appendlink(room->progs->progs[tt->slot], rpr);
				trigger_type_add_use(tt);
		    }
		    fMatch = true;
		}

		break;

	    case 'S':
			if (!str_cmp(word, "Sector"))
			{
				if(area->version_room < VERSION_ROOM_003)
				{
					int old_type = fread_number(fp);
					char *old_name = flag_string(sector_types, old_type);
					room->sector = get_sector_data(old_name);
				}
				else
				{
					room->sector = get_sector_data(fread_string(fp));
				}
				if (!room->sector) room->sector = gsct_inside;
				room->sector_flags = room->sector->flags;
				fMatch = true;
				break;
			}

			if(area->version_room < VERSION_ROOM_003)
			{
				if (!str_cmp(word, "Sector_type"))
				{
					int old_type = fread_number(fp);
					char *old_name = flag_string(sector_types, old_type);
					room->sector = get_sector_data(old_name);
					if (!room->sector) room->sector = gsct_inside;
					room->sector_flags = room->sector->flags;
					fMatch = true;
					break;
				}
			}
			break;

	    case 'V':
			if (olc_load_index_vars(fp, word, &room->index_vars, area))
			{
				fMatch = true;
				break;
			}
		break;

	    case 'W':
		if (!str_cmp(word, "Wilds")) {
			room->w = fread_number(fp);
			room->x = fread_number(fp);
			room->y = fread_number(fp);
			room->z = fread_number(fp);
		    fMatch = true;
		}
	    	break;

	}

	if (!fMatch) {
	    sprintf(buf, "read_room_new: no match for word %s", word);
	    bug(buf, 0);
		fread_to_eol(fp);
	}
    }

	if (recordtype == ROOMTYPE_NORMAL)
	{
		if (!IS_VALID(room->region))
		{
			// No region defined yet, place in the default region
			room->region = &area->region;
			list_appendlink(area->region.rooms, room);
		}
	}


	if (recordtype != ROOMTYPE_TERRAIN)
		variable_copylist(&room->index_vars,&room->progs->vars,false);

	if( room->persist )
		persist_addroom(room);

	if (recordtype != ROOMTYPE_TERRAIN)
	{
		if( area->version_room < VERSION_ROOM_001 )
		{
			// Correct exits
			for( int e = 0; e < MAX_DIR; e++)
			{
				ex = room->exit[e];

				if( !ex ) continue;

				if( IS_SET(ex->rs_flags, VR_001_EX_LOCKED) )
				{
					SET_BIT(ex->door.rs_lock.flags, LOCK_LOCKED);
				}

				if( IS_SET(ex->rs_flags, VR_001_EX_PICKPROOF) )
				{
					ex->door.rs_lock.pick_chance = 0;
				}
				else if( IS_SET(ex->rs_flags, VR_001_EX_INFURIATING) )
				{
					ex->door.rs_lock.pick_chance = 10;
				}
				else if( IS_SET(ex->rs_flags, VR_001_EX_HARD) )
				{
					ex->door.rs_lock.pick_chance = 40;
				}
				else if( IS_SET(ex->rs_flags, VR_001_EX_EASY) )
				{
					ex->door.rs_lock.pick_chance = 80;
				}
				else
				{
					ex->door.rs_lock.pick_chance = 100;
				}


				REMOVE_BIT(ex->rs_flags, (VR_001_EX_LOCKED|VR_001_EX_PICKPROOF|VR_001_EX_INFURIATING|VR_001_EX_HARD|VR_001_EX_EASY));

//				ex->exit_info = ex->rs_flags;
//				ex->door.lock.flags = ex->door.rs_lock.flags;
//				ex->door.lock.pick_chance = ex->door.rs_lock.pick_chance;
			}
		}
	}


    return room;
}


/* For housekeeping. See below. */
FILE	*fp_temp;

/* read one mobile into an area */
MOB_INDEX_DATA *read_mobile_new(FILE *fp, AREA_DATA *area)
{
    MOB_INDEX_DATA *mob = NULL;
    SHOP_DATA *shop;
    MISSIONARY_DATA *missionary;
    PROG_LIST *mpr;
    char *word;

    mob = new_mob_index();
    mob->vnum = fread_number(fp);

	area->bottom_vnum_mob = UMIN(area->bottom_vnum_mob, mob->vnum);
	area->top_vnum_mob = UMAX(area->top_vnum_mob, mob->vnum);

    mob->persist = false;

    while (str_cmp((word = fread_word(fp)), "#-MOBILE")) {
	fMatch = false;

	switch(word[0]) {
	    case '#':
	        if( !str_cmp(word, "#CREW") )
	        {
				if( mob->pCrew )
					free_ship_crew_index(mob->pCrew);

				mob->pCrew = read_ship_crew_index_new(fp);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#PRACTICE"))
			{
				mob->pPractice = read_practice_data(fp, area);
				fMatch = true;
				break;
			}

	        if (!str_cmp(word, "#SHOP")) {
			    fMatch = true;
			    shop = read_shop_new(fp, area);
			    mob->pShop = shop;
			    break;
			}
	        if (!str_cmp(word, "#MISSIONARY")) {
			    fMatch = true;
			    missionary = read_missionary_new(fp, area);
			    mob->pMissionary = missionary;
			    break;
			}

			break;

		case 'A':
	        KEY("Act",	mob->act[0],	fread_number(fp));
	        KEY("Act2",	mob->act[1],	fread_number(fp));
                KEY("Affected_by", mob->affected_by[0],	fread_number(fp));
		KEY("Affected_by2",  mob->affected_by[1],	fread_number(fp));
		KEY("Alignment",  mob->alignment,	fread_number(fp));
		KEY("AttackType", mob->dam_type,	fread_number(fp));
		KEY("Attacks",	mob->attacks,	fread_number(fp));
		break;

		case 'B':
			KEY("Boss", mob->boss, true);
			break;

	    case 'C':
	        KEYS("CreatorSig", mob->creator_sig,	fread_string(fp));
	        KEY("CorpseType", mob->corpse_type,	fread_number(fp));
	        KEY("CorpseWnum", mob->corpse,	fread_widevnum(fp, area->uid));
	        KEY("CorpseZombie", mob->zombie,	fread_widevnum(fp, area->uid));
			KEY("Comments", mob->comments, fread_string(fp));
	        break;

	    case 'D':
	        KEYS("Description", mob->description, fread_string(fp));
		KEY("DefaultPos",	mob->default_pos,	fread_number(fp));

	        if (!str_cmp(word, "Damage")) {
		    mob->damage.number = fread_number(fp);
		    mob->damage.size = fread_number(fp);
		    mob->damage.bonus = fread_number(fp);
		    fMatch = true;
		}

		break;

	    case 'F':
			if (!str_cmp(word, "Faction"))
			{
				WNUM_LOAD *load = fread_widevnumptr(fp, area->uid);

				if (load)
				{
					list_appendlink(mob->factions_load, load);
				}
				fMatch = true;
				break;
			}
			KEY("Form",		mob->form,	fread_number(fp));
			break;

	    case 'H':
	        KEY("Hitroll",	mob->hitroll,	fread_number(fp));

		if (!str_cmp(word, "Hit")) {
		    mob->hit.number = fread_number(fp);
		    mob->hit.size = fread_number(fp);
		    mob->hit.bonus = fread_number(fp);
		    fMatch = true;
		}

		break;

	    case 'I':
	        KEYS("ImpSig",		mob->sig,	fread_string(fp));
		KEY("ImmFlags", 	mob->imm_flags, fread_number(fp));

	    case 'L':
	        KEYS("LongDesc",	mob->long_descr,	fread_string(fp));
		KEY("Level",		mob->level,		fread_number(fp));
		break;

	    case 'M':
			if (!str_cmp(word, "Material"))
			{
				mob->material = material_lookup(fread_string(fp));
				fMatch = true;
				break;
			}
		KEY("Movement",	mob->move,	fread_number(fp));

		if (!str_cmp(word, "Mana")) {
		    mob->mana.number = fread_number(fp);
		    mob->mana.size = fread_number(fp);
		    mob->mana.bonus = fread_number(fp);
		    fMatch = true;
		}

		if (!str_cmp(word, "MobProg")) {
		    char *p;

		    WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
		    p = fread_string(fp);

		    struct trigger_type *tt = get_trigger_type(p, PRG_MPROG);
		    if(!tt) {
			    sprintf(buf, "read_mob_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    mpr = new_trigger();

			    mpr->wnum_load = wnum_load;
			    mpr->trig_type = tt->type;
			    mpr->trig_phrase = fread_string(fp);
			    if( tt->type == TRIG_SPELLCAST ) {
					char buf[MIL];
					SKILL_DATA *sk = get_skill_data(mpr->trig_phrase);

					if( !IS_VALID(sk) ) {
						sprintf(buf, "read_mob_new: invalid spell '%s' for TRIG_SPELLCAST", mpr->trig_phrase);
						bug(buf, 0);
						free_trigger(mpr);
						fMatch = true;
						break;
					}

					free_string(mpr->trig_phrase);
					sprintf(buf, "%d", sk->uid);
					mpr->trig_phrase = str_dup(buf);
					mpr->trig_number = sk->uid;
					mpr->numeric = true;

				} else {
			    	mpr->trig_number = atoi(mpr->trig_phrase);
					mpr->numeric = is_number(mpr->trig_phrase);
				}
			    //SET_BIT(room->rprog_flags, rpr->trig_type);

			    if(!mob->progs) mob->progs = new_prog_bank();

				list_appendlink(mob->progs[tt->slot], mpr);
				trigger_type_add_use(tt);
		    }
		    fMatch = true;
		}
		break;

	    case 'N':
	        KEYS("Name",	mob->player_name,	fread_string(fp));
		break;

	    case 'O':
	        KEYS("Owner",	mob->owner,	fread_string(fp));
		KEY("OffFlags", mob->off_flags, fread_number(fp));
		break;
	    case 'P':
	        KEY("Parts",	mob->parts,	fread_number(fp));
	        KEY("Persist",	mob->persist, true);
		break;

	    case 'R':
		KEY("ResFlags", 	mob->res_flags, fread_number(fp));

		if (!str_cmp(word, "Race")) {
		    char *race_string = fread_string(fp);

		    mob->race = get_race_data(race_string);

		    free_string(race_string);

		    fMatch = true;
			break;
		}
			if (!str_cmp(word, "Reputation"))
			{
				WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
				int16_t min_rank = fread_number(fp);
				int16_t max_rank = fread_number(fp);
				long points = fread_number(fp);

				MOB_REPUTATION_DATA *new_rep = new_mob_reputation_data();
				new_rep->reputation_load = wnum_load;
				new_rep->minimum_rank = min_rank;
				new_rep->maximum_rank = max_rank;
				new_rep->points = points;
				new_rep->next = NULL;

				MOB_REPUTATION_DATA *rep;

				for(rep = mob->reputations; rep && rep->next; rep = rep->next);

				if (rep)
					rep->next = new_rep;
				else
					mob->reputations = new_rep;

				fMatch = true;
				break;
			}

		break;

	    case 'S':
	        KEYS("ShortDesc",	mob->short_descr,	fread_string(fp));
	        KEY("StartPos",	mob->start_pos,		fread_number(fp));
		KEY("Sex",		mob->sex,		fread_number(fp));
		KEY("Size",	        mob->size,		fread_number(fp));
		KEY("Skeywds",	mob->skeywds,	fread_string(fp));

		if (!str_cmp(word, "SpecFun")) {
		    char *name = fread_string(fp);

		    mob->spec_fun = spec_lookup(name);

		    free_string(name);
		    fMatch = true;
		}

		break;

            case 'V':
				if (olc_load_index_vars(fp, word, &mob->index_vars, area))
				{
					fMatch = true;
					break;
				}

	        KEY("VulnFlags",	mob->vuln_flags,	fread_number(fp));
		break;
	    case 'W':
	        KEY("Wealth",		mob->wealth,		fread_number(fp));
		break;
	}

	if (!fMatch) {
	    //sprintf(buf, "read_mobile_new: no match for word %s", word);
	    //bug(buf, 0);
	}
    }

	// TODO: Temporary
//	{
//		char buf[MSL];
//		sprintf(buf, "Mob %ld#%ld Longdesc: '%s'", area->uid, mob->vnum, mob->long_descr ? mob->long_descr : "(no long description)");
//		bug(buf, 0);
//	}

	// Remove this bit, JIC
	REMOVE_BIT(mob->act[0], ACT_ANIMATED);

    /* Syn - make any fixes or changes to the mob here. Mainly for
       updating formats of area files, moving flags around, and such. */
    if (IS_SET(mob->form, FORM_MAGICAL)) {
	 SET_BIT(mob->off_flags, OFF_MAGIC);
	 REMOVE_BIT(mob->form, FORM_MAGICAL);
	 sprintf(buf, "set off bit magic on for mob %s(%ld)", mob->short_descr, mob->vnum);
	 log_string(buf);
    }

    /* Make sure all mobs are level 1 at least, to prevent pains */
    if (mob->level == 0) {
	sprintf(buf, "read_mob_new: mob %s(%ld) had level 0, set it to 1 and reset vitals",
	    mob->short_descr, mob->vnum);
	log_string(buf);
	mob->level = 1;
	// Be sure to reset the vitals
	set_mob_hitdice(mob);
	set_mob_damdice(mob);
	if (IS_SET(mob->off_flags, OFF_MAGIC))
	    set_mob_manadice(mob);
    }

	if( !has_imp_sig(mob, NULL) ) {
		// Made reseting require there be no impsign

    	/* AO 092516 - Use Mitch's hitdice/damdice fixes */
		set_mob_hitdice(mob);
		set_mob_damdice(mob);
		set_mob_movedice(mob);
	}

    return mob;
}

LOCK_STATE *read_object_lockstate(FILE *fp)
{
	LOCK_STATE *lock = new_lock_state();
	char buf[MSL];
    char *word;

    while (str_cmp((word = fread_word(fp)), "#-LOCK"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", lock->flags, fread_flag(fp));
				break;

			case 'K':
				if (!str_cmp(word, "Key"))
				{
					lock->key_load = fread_widevnum(fp, 0);

					fMatch = true;
					break;
				}
				break;

			case 'P':
				KEY("PickChance", lock->pick_chance, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_lockstate: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return lock;
}

AMMO_DATA *read_object_ammo_data(FILE *fp)
{
	AMMO_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_ammo_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEAMMO"))
	{
		fMatch = false;

		switch(word[0])
		{
		case 'D':
			if (!str_cmp(word, "Damage"))
			{
				data->damage_type = attack_lookup(fread_string(fp));
				data->flags = fread_flag(fp);
				data->damage.number = fread_number(fp);
				data->damage.size = fread_number(fp);
				data->damage.bonus = fread_number(fp);
				data->damage.last_roll = 0;
				fMatch = true;
				break;
			}
			break;

		case 'M':
			KEYS("MsgBreak", data->msg_break, fread_string(fp));
			break;

		case 'T':
			KEY("Type", data->type, stat_lookup(fread_string(fp), ammo_types, AMMO_NONE));
			break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_ammo_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

ADORNMENT_DATA *read_object_adornment_data(FILE *fp)
{
	ADORNMENT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_adornment_data();

    while (str_cmp((word = fread_word(fp)), "#-ADORNMENT"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						if (data->spell != NULL)
							free_spell(data->spell);

						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						data->spell = spell;
					}

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Type", data->type, stat_lookup(fread_string(fp),adornment_types,ADORNMENT_NONE));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_adornment_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

ARMOR_DATA *read_object_armor_data(FILE *fp)
{
	ARMOR_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_armor_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEARMOR"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#ADORNMENT"))
				{
					ADORNMENT_DATA *adornment = read_object_adornment_data(fp);

					bool found = false;
					for(int i = 0; i < data->max_adornments; i++)
					{
						if(data->adornments[i] == NULL)
						{
							data->adornments[i] = adornment;
							found = true;
							break;
						}
					}

					if (!found)
					{
						free_adornment_data(adornment);
						// Complain
					}

					fMatch = true;
					break;
				}
				break;

			case 'M':
				if (!str_cmp(word, "MaxAdornments"))
				{
					int max = fread_number(fp);

					if (data->adornments != NULL)
					{
						// complain about already being defined.
						break;
					}

					if (max >= 1 && max <= MAX_ADORNMENTS)
					{
						data->max_adornments = max;
						data->adornments = alloc_mem(max * sizeof(ADORNMENT_DATA *));
						for (int i = 0; i < max; i++)
							data->adornments[i] = NULL;
					}
					else
						data->max_adornments = 0;
					fMatch = true;
					break;
				}
				break;

			case 'P':
				if (!str_cmp(word,"Protection"))
				{
					int rating = stat_lookup(fread_string(fp),armour_protection_types,NO_FLAG);
					int value = fread_number(fp);

					if (rating != NO_FLAG)
						data->protection[rating] = value;

					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEY("Strength", data->armor_strength, stat_lookup(fread_string(fp), armour_strength_table, OBJ_ARMOUR_NOSTRENGTH));
				break;

			case 'T':
				KEY("Type", data->armor_type, stat_lookup(fread_string(fp), armour_types, ARMOR_TYPE_NONE));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_armor_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}



BOOK_PAGE *read_object_book_page(FILE *fp, char *closer, AREA_DATA *area)
{
	BOOK_PAGE *page = new_book_page();
	char buf[MSL];
	char *word;
	bool fMatch;

	page->page_no = fread_number(fp);

	while(str_cmp((word = fread_word(fp)), closer))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'B':
				KEY("Book", page->book, fread_widevnum(fp, area ? area->uid : 0));
				break;

			case 'T':
				KEYS("Title", page->title, fread_string(fp));
				KEYS("Text", page->text, fread_string(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_book_page: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return page;
}

BOOK_DATA *read_object_book_data(FILE *fp, AREA_DATA *area)
{
	BOOK_DATA *book = new_book_data();
	char buf[MSL];
	char *word;
	bool fMatch;

	while(str_cmp((word = fread_word(fp)), "#-TYPEBOOK"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					book->lock = read_object_lockstate(fp);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#PAGE"))
				{
					BOOK_PAGE *page = read_object_book_page(fp, "#-PAGE", area);

					if (!book_insert_page(book, page))
					{
						sprintf(buf, "read_object_book_data: page with duplicate page number (%d) found!  Discarding.", page->page_no);
						bug(buf, 0);
						free_book_page(page);	
					}
					fMatch = true;
					break;
				}
				break;
			
			case 'C':
				KEY("CurrentPage", book->current_page, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", book->flags, fread_flag(fp));
				break;

			case 'N':
				KEYS("Name", book->name, fread_string(fp));
				break;

			case 'O':
				KEY("OpenPage", book->open_page, fread_number(fp));
				break;
			
			case 'S':
				KEYS("Short", book->short_descr, fread_string(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_book_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return book;
}

CART_DATA *read_object_cart_data(FILE *fp)
{
	CART_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_cart_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECART"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MinStrength", data->min_strength, fread_number(fp));
				KEY("MoveDelay", data->move_delay, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_cart_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

COMPASS_DATA *read_object_compass_data(FILE *fp)
{
	COMPASS_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_compass_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECOMPASS"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Accuracy", data->accuracy, fread_number(fp));
				break;

			case 'W':
				KEY("Wilds", data->wuid, fread_number(fp));
				break;
			
			case 'X':
				KEY("X", data->x, fread_number(fp));
				break;
			
			case 'Y':
				KEY("Y", data->y, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_compass_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

CONTAINER_DATA *read_object_container_data(FILE *fp)
{
	CONTAINER_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_container_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPECONTAINER"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = read_object_lockstate(fp);
					fMatch = true;
					break;
				}
				break;

			case 'B':
				if (!str_cmp(word, "Blacklist"))
				{
					int item_type = stat_lookup(fread_string(fp), type_flags, NO_FLAG);
					int sub_type = -1;

					if (item_type != NO_FLAG)
					{
						if (item_type == ITEM_WEAPON)
						{
							sub_type = stat_lookup(fread_string(fp), weapon_class, -1);
						}

						CONTAINER_FILTER *filter = new_container_filter();
						filter->item_type = item_type;
						filter->sub_type = sub_type;

						list_appendlink(data->blacklist, filter);
					}

					fMatch = true;
					break;
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MaxWeight", data->max_weight, fread_number(fp));
				KEY("MaxVolume", data->max_volume, fread_number(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));
				break;

			case 'W':
				KEY("WeightMultiplier", data->weight_multiplier, fread_number(fp));
				if (!str_cmp(word, "Whitelist"))
				{
					int item_type = stat_lookup(fread_string(fp), type_flags, NO_FLAG);
					int sub_type = -1;

					if (item_type != NO_FLAG)
					{
						if (item_type == ITEM_WEAPON)
						{
							sub_type = stat_lookup(fread_string(fp), weapon_class, -1);
						}

						CONTAINER_FILTER *filter = new_container_filter();
						filter->item_type = item_type;
						filter->sub_type = sub_type;

						list_appendlink(data->whitelist, filter);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_container_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FLUID_CONTAINER_DATA *read_object_fluid_container_data(FILE *fp, AREA_DATA *area)
{
	FLUID_CONTAINER_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_fluid_container_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFLUIDCONTAINER"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = read_object_lockstate(fp);
					fMatch = true;
					break;
				}
				break;

			case 'A':
				KEY("Amount", data->amount, fread_number(fp));
				break;

			case 'C':
				KEY("Capacity", data->capacity, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'L':
				if (!str_cmp(word, "Liquid"))
				{
					data->liquid = liquid_lookup(fread_string(fp));
					fMatch = true;
					break;
				}
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'P':
				KEY("Poison", data->poison, fread_number(fp));
				KEY("PoisonRate", data->poison_rate, fread_number(fp));
				break;

			case 'R':
				KEY("RefillRate", data->refill_rate, fread_number(fp));
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));

				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}

				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_fluid_container_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FOOD_BUFF_DATA *read_food_buff(FILE *fp)
{
	FOOD_BUFF_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_food_buff_data();

	data->where = fread_number(fp);

    while (str_cmp((word = fread_word(fp)), "#-FOODBUFF"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'B':
				KEY("Bitvector", data->bitvector, fread_flag(fp));
				KEY("Bitvector2", data->bitvector2, fread_flag(fp));
				break;

			case 'D':
				KEY("Duration", data->duration, fread_number(fp));
				break;

			case 'L':
				KEY("Level", data->level, fread_number(fp));
				KEY("Location", data->location, fread_number(fp));
				break;

			case 'M':
				KEY("Modifier", data->modifier, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_food_buff: no match for word %s", word);
			bug(buf, 0);
		}
	}

	data->level = UMAX(0, data->level);
	data->duration = UMAX(0, data->duration);

	return data;
}

FOOD_DATA *read_object_food_data(FILE *fp)
{
	FOOD_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_food_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFOOD"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#FOODBUFF"))
				{
					FOOD_BUFF_DATA *buff = read_food_buff(fp);

					list_appendlink(data->buffs, buff);

					fMatch = true;
				}
				break;

			case 'F':
				KEY("Fullness", data->full, fread_number(fp));
				break;

			case 'H':
				KEY("Hunger", data->hunger, fread_number(fp));
				break;

			case 'P':
				KEY("Poison", data->poison, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_food_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


FURNITURE_COMPARTMENT *read_furniture_compartment(FILE *fp)
{
	FURNITURE_COMPARTMENT *data = NULL;

	char buf[MSL];
    char *word;

	data = new_furniture_compartment();

    while (str_cmp((word = fread_word(fp)), "#-COMPARTMENT"))
	{
		bool fMatch = false;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (data->lock) free_lock_state(data->lock);

					data->lock = read_object_lockstate(fp);
					fMatch = true;
					break;
				}
				break;

			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'H':
				KEY("Hanging", data->hanging, fread_flag(fp));
				KEY("HealthRegen", data->health_regen, fread_number(fp));
				break;

			case 'M':
				KEY("ManaRegen", data->mana_regen, fread_number(fp));
				KEY("MaxOccupants", data->max_occupants, fread_number(fp));
				KEY("MaxWeight", data->max_weight, fread_number(fp));
				KEY("MoveRegen", data->move_regen, fread_number(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'R':
				KEY("Resting", data->resting, fread_flag(fp));
				break;
			
			case 'S':
				KEYS("ShortDesc", data->short_descr, fread_string(fp));
				KEY("Sitting", data->sitting, fread_flag(fp));
				KEY("Sleeping", data->sleeping, fread_flag(fp));
				KEY("Standing", data->standing, fread_flag(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_furniture_compartment: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

FURNITURE_DATA *read_object_furniture_data(FILE *fp)
{
	FURNITURE_DATA *data = NULL;

	char buf[MSL];
    char *word;

	data = new_furniture_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEFURNITURE"))
	{
		fMatch = false;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#COMPARTMENT"))
				{
					FURNITURE_COMPARTMENT *compartment = read_furniture_compartment(fp);
					list_appendlink(data->compartments, compartment);

					compartment->ordinal = list_size(data->compartments);

					fMatch = true;
					break;
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MainCompartment", data->main_compartment, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_furniture_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


INK_DATA *read_object_ink_data(FILE *fp)
{
	INK_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_ink_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEINK"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'T':
				if (!str_cmp(word, "Type"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					int amount = fread_number(fp);

					if (index >= 1 && index <= MAX_INK_TYPES)
					{
						int type = stat_lookup(name, catalyst_types, CATALYST_NONE);
						if (type != CATALYST_NONE)
						{
							data->types[index-1] = type;
							data->amounts[index-1] = amount;
						}
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_ink_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


INSTRUMENT_DATA *read_object_instrument_data(FILE *fp)
{
	INSTRUMENT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_instrument_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEINSTRUMENT"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'B':
				if (!str_cmp(word, "Beats"))
				{
					data->beats_min = fread_number(fp);
					data->beats_max = fread_number(fp);

					fMatch = true;
					break;					
				}
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				if (!str_cmp(word, "mana"))
				{
					data->mana_min = fread_number(fp);
					data->mana_max = fread_number(fp);

					fMatch = true;
					break;					
				}
				break;

			case 'R':
				if (!str_cmp(word, "Reservoir"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					int amount = fread_number(fp);
					int capacity = fread_number(fp);

					if (index >= 1 && index <= INSTRUMENT_MAX_CATALYSTS)
					{
						int type = stat_lookup(name, catalyst_types, CATALYST_NONE);
						if (type != CATALYST_NONE)
						{
							data->reservoirs[index - 1].type = type;
							data->reservoirs[index - 1].amount = amount;
							data->reservoirs[index - 1].capacity = capacity;
						}
					}
					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Type", data->type, stat_lookup(fread_string(fp), catalyst_types, CATALYST_NONE));
				break;

		}

		if (!fMatch) {
			sprintf(buf, "read_object_instrument_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

JEWELRY_DATA *read_object_jewelry_data(FILE *fp)
{
	JEWELRY_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_jewelry_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEJEWELRY"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'M':
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}

				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_jewelry_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


LIGHT_DATA *read_object_light_data(FILE *fp)
{
	LIGHT_DATA *data = NULL;

	char buf[MSL];
    char *word;

	data = new_light_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPELIGHT"))
	{
		fMatch = false;
		switch(word[0])
		{
			case 'D':
				KEY("Duration", data->duration, fread_number(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_light_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

MAP_DATA *read_object_map_data(FILE *fp)
{
	MAP_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_map_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMAP"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'W':
				if (!str_cmp(word, "Waypoint"))
				{
					WAYPOINT_DATA *wp = new_waypoint();

					wp->w = fread_number(fp);
					wp->x = fread_number(fp);
					wp->y = fread_number(fp);
					wp->name = fread_string(fp);

					list_appendlink(data->waypoints, wp);
					fMatch = true;
					break;
				}
				KEY("Wilds", data->wuid, fread_number(fp));
				break;
			
			case 'X':
				KEY("X", data->x, fread_number(fp));
				break;
			
			case 'Y':
				KEY("Y", data->y, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_map_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

MIST_DATA *read_object_mist_data(FILE *fp)
{
	MIST_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_mist_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMIST"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Acidic", data->acidic, fread_number(fp));
				break;

			case 'F':
				KEY("Fiery", data->fiery, fread_number(fp));
				KEY("Fog", data->fog, fread_number(fp));
				break;

			case 'I':
				KEY("Icy", data->icy, fread_number(fp));
				break;

			case 'O':
				KEY("ObscureMobs", data->obscure_mobs, fread_number(fp));
				KEY("ObscureObjs", data->obscure_objs, fread_number(fp));
				KEY("ObscureRoom", data->obscure_room, fread_number(fp));
				break;

			case 'S':
				KEY("Shock", data->shock, fread_number(fp));
				KEY("Sleep", data->sleep, fread_number(fp));
				KEY("Stink", data->stink, fread_number(fp));
				break;

			case 'T':
				KEY("Toxic", data->toxic, fread_number(fp));
				break;

			case 'W':
				KEY("Wither", data->wither, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_mist_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

MONEY_DATA *read_object_money_data(FILE *fp)
{
	MONEY_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_money_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEMONEY"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'G':
				KEY("Gold", data->gold, fread_number(fp));
				break;

			case 'S':
				KEY("Silver", data->silver, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_money_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

PORTAL_DATA *read_object_portal_data(FILE *fp)
{
	PORTAL_DATA *data = NULL;
	char buf[MSL];
    char *word;

	data = new_portal_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEPORTAL"))
	{
		fMatch = false;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#LOCK"))
				{
					if (!data->lock) free_lock_state(data->lock);
					data->lock = read_object_lockstate(fp);
					fMatch = true;
					break;
				}
				break;

			case 'E':
				KEY("Exit", data->exit, fread_flag(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'P':
				if (!str_cmp(word, "Param"))
				{
					int p = fread_number(fp);
					long v = fread_number(fp);

					if (p >= 0 && p < MAX_PORTAL_VALUES)
					{
						data->params[p] = v;
					}
					else
					{
						// Complain
					}

					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEYS("Short", data->short_descr, fread_string(fp));

				if (!str_cmp(word, "SpellNew"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);
					int repop = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);

					fMatch = true;
					if (is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = repop;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}
					else
					{
						// Complain
					}
				}
				break;

			case 'T':
				if (!str_cmp(word, "Type"))
				{
					data->type = flag_find(fread_string(fp), portal_gatetype);

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_portal_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


SCROLL_DATA *read_object_scroll_data(FILE *fp)
{
	SCROLL_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_scroll_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPESCROLL"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				break;

			case 'M':
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_scroll_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

SEXTANT_DATA *read_object_sextant_data(FILE *fp)
{
	SEXTANT_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_sextant_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPESEXTANT"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Accuracy", data->accuracy, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_sextant_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

TATTOO_DATA *read_object_tattoo_data(FILE *fp)
{
	TATTOO_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_tattoo_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPETATTOO"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'F':
				KEY("FadeChance", data->fading_chance, fread_number(fp));
				KEY("FadeRate", data->fading_rate, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Touches", data->touches, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_tattoo_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

TELESCOPE_DATA *read_object_telescope_data(FILE *fp)
{
	TELESCOPE_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_telescope_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPETELESCOPE"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'B':
				KEY("BonusView", data->bonus_view, fread_number(fp));
				break;

			case 'D':
				KEY("Distance", data->distance, fread_number(fp));
				break;

			case 'H':
				KEY("Heading", data->heading, fread_number(fp));
				break;

			case 'M':
				KEY("MaxDistance", data->max_distance, fread_number(fp));
				KEY("MinDistance", data->min_distance, fread_number(fp));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_telescope_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

WAND_DATA *read_object_wand_data(FILE *fp)
{
	WAND_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_wand_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEWAND"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'C':
				KEY("Charges", data->charges, fread_number(fp));
				KEY("Cooldown", data->cooldown, fread_number(fp));
				break;

			case 'M':
				KEY("MaxCharges", data->max_charges, fread_number(fp));
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'R':
				KEY("RechargeTime", data->recharge_time, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_wand_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


WEAPON_DATA *read_object_weapon_data(FILE *fp)
{
	WEAPON_DATA *data = NULL;
	char buf[MSL];
    char *word;
	bool fMatch;

	data = new_weapon_data();

    while (str_cmp((word = fread_word(fp)), "#-TYPEWEAPON"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				KEY("Ammo", data->ammo, stat_lookup(fread_string(fp), ammo_types, AMMO_NONE));
				if (!str_cmp(word, "Attack"))
				{
					int index = fread_number(fp);
					char *name = fread_string(fp);
					char *short_descr = fread_string(fp);
					int16_t type = attack_lookup(fread_string(fp));
					long flags = fread_flag(fp);
					int number = fread_number(fp);
					int size = fread_number(fp);
					int bonus = fread_number(fp);

					if (index >= 0 && index < MAX_ATTACK_POINTS)
					{
						data->attacks[index].type = type;
						data->attacks[index].name = name;
						data->attacks[index].short_descr = short_descr;
						data->attacks[index].flags = flags;
						data->attacks[index].damage.number = number;
						data->attacks[index].damage.size = size;
						data->attacks[index].damage.bonus = bonus;
						data->attacks[index].damage.last_roll = 0;
					}
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Charges", data->charges, fread_number(fp));
				KEY("Cooldown", data->cooldown, fread_number(fp));
				break;

			case 'M':
				KEY("MaxCharges", data->max_charges, fread_number(fp));
				KEY("MaxMana", data->max_mana, fread_number(fp));
				break;

			case 'R':
				KEY("Range", data->range, fread_number(fp));
				KEY("RechargeTime", data->recharge_time, fread_number(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Spell"))
				{
					char *name = fread_string(fp);
					int level = fread_number(fp);

					SKILL_DATA *skill = get_skill_data(name);
					if (IS_VALID(skill) && is_skill_spell(skill))
					{
						SPELL_DATA *spell = new_spell();
						spell->skill = skill;
						spell->level = level;
						spell->repop = 100;
						spell->next = NULL;

						list_appendlink(data->spells, spell);
					}

					fMatch = true;
					break;
				}
				break;

			case 'W':
				KEY("WeaponClass", data->weapon_class, stat_lookup(fread_string(fp), weapon_class, WEAPON_EXOTIC));
				break;
		}

		if (!fMatch) {
			sprintf(buf, "read_object_weapon_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}


/* read one object into an area */
OBJ_INDEX_DATA *read_object_new(FILE *fp, AREA_DATA *area)
{
    OBJ_INDEX_DATA *obj = NULL;
    SPELL_DATA *spell;
    PROG_LIST *opr;
    AFFECT_DATA *af;
    EXTRA_DESCR_DATA *ed;
    char *word;

    obj = new_obj_index();
    obj->vnum = fread_number(fp);

	area->bottom_vnum_obj = UMIN(area->bottom_vnum_obj, obj->vnum);
	area->top_vnum_obj = UMAX(area->top_vnum_obj, obj->vnum);

    obj->persist = false;
	long values[MAX_OBJVALUES];

    while (str_cmp((word = fread_word(fp)), "#-OBJECT")) {
	fMatch = false;

	switch(word[0]) {
	    case '#':
	        if (!str_cmp(word, "#AFFECT")) {
				af = read_obj_affect_new(fp);
				af->next = obj->affected;
				obj->affected = af;
				fMatch = true;
			} else if (!str_cmp(word, "#CATALYST")) {
				af = read_obj_catalyst_new(fp);
				af->next = obj->catalyst;
				obj->catalyst = af;
				fMatch = true;
			} else if (!str_cmp(word, "#EXTRA_DESCR")) {
				ed = read_extra_descr_new(fp);
				ed->next = obj->extra_descr;
				obj->extra_descr = ed;
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEAMMO")) {
				if (IS_AMMO(obj)) free_ammo_data(AMMO(obj));
				AMMO(obj) = read_object_ammo_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEARMOR")) {
				if (IS_ARMOR(obj)) free_armor_data(ARMOR(obj));
				ARMOR(obj) = read_object_armor_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEBOOK")) {
				if (IS_BOOK(obj)) free_book_data(BOOK(obj));
				BOOK(obj) = read_object_book_data(fp, area);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPECART")) {
				if (IS_CART(obj)) free_cart_data(CART(obj));
				CART(obj) = read_object_cart_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPECOMPASS")) {
				if (IS_COMPASS(obj)) free_compass_data(COMPASS(obj));
				COMPASS(obj) = read_object_compass_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPECONTAINER")) {
				if (IS_CONTAINER(obj)) free_container_data(CONTAINER(obj));
				CONTAINER(obj) = read_object_container_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEFLUIDCONTAINER")) {
				if (IS_FLUID_CON(obj)) free_fluid_container_data(FLUID_CON(obj));
				FLUID_CON(obj) = read_object_fluid_container_data(fp, area);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEFOOD")) {
				if (IS_FOOD(obj)) free_food_data(FOOD(obj));
				FOOD(obj) = read_object_food_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEFURNITURE")) {
				if (IS_FURNITURE(obj)) free_furniture_data(FURNITURE(obj));
				FURNITURE(obj) = read_object_furniture_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEINK")) {
				if (IS_INK(obj)) free_ink_data(INK(obj));
				INK(obj) = read_object_ink_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEINSTRUMENT")) {
				if (IS_INSTRUMENT(obj)) free_instrument_data(INSTRUMENT(obj));
				INSTRUMENT(obj) = read_object_instrument_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEJEWELRY")) {
				if (IS_JEWELRY(obj)) free_jewelry_data(JEWELRY(obj));
				JEWELRY(obj) = read_object_jewelry_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPELIGHT")) {
				if (IS_LIGHT(obj)) free_light_data(LIGHT(obj));
				LIGHT(obj) = read_object_light_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEMAP")) {
				if (IS_MAP(obj)) free_map_data(MAP(obj));
				MAP(obj) = read_object_map_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEMIST")) {
				if (IS_MIST(obj)) free_mist_data(MIST(obj));
				MIST(obj) = read_object_mist_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEMONEY")) {
				if (IS_MONEY(obj)) free_money_data(MONEY(obj));
				MONEY(obj) = read_object_money_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEPAGE")) {
				if (IS_PAGE(obj)) free_book_page(PAGE(obj));
				PAGE(obj) = read_object_book_page(fp, "#-TYPEPAGE", area);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEPORTAL")) {
				if (IS_PORTAL(obj)) free_portal_data(PORTAL(obj));
				PORTAL(obj) = read_object_portal_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPESCROLL")) {
				if (IS_SCROLL(obj)) free_scroll_data(SCROLL(obj));
				SCROLL(obj) = read_object_scroll_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPESEXTANT")) {
				if (IS_SEXTANT(obj)) free_sextant_data(SEXTANT(obj));
				SEXTANT(obj) = read_object_sextant_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPETATTOO")) {
				if (IS_TATTOO(obj)) free_tattoo_data(TATTOO(obj));
				TATTOO(obj) = read_object_tattoo_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPETELESCOPE")) {
				if (IS_TELESCOPE(obj)) free_telescope_data(TELESCOPE(obj));
				TELESCOPE(obj) = read_object_telescope_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEWAND")) {
				if (IS_WAND(obj)) free_wand_data(WAND(obj));
				WAND(obj) = read_object_wand_data(fp);
				fMatch = true;
			} else if (!str_cmp(word, "#TYPEWEAPON")) {
				if (IS_WEAPON(obj)) free_weapon_data(WEAPON(obj));
				WEAPON(obj) = read_object_weapon_data(fp);
				fMatch = true;
			}
			break;

		case 'A':
			break;

	    case 'C':
			KEY("Class", obj->clazz, get_class_data(fread_string(fp)));
			KEY("ClassType", obj->clazz_type, stat_lookup(fread_string(fp), class_types, CLASS_NONE));
			KEYS("CreatorSig", obj->creator_sig,	fread_string(fp));
			KEY("Cost",	obj->cost,	fread_number(fp));
			KEY("Condition",	obj->condition,	fread_number(fp));
			KEY("Comments", obj->comments, fread_string(fp));
			break;

	    case 'D':
			KEYS("Description", obj->full_description, fread_string(fp));
			break;

	    case 'E':
			KEY("ExtraFlags",	obj->extra[0],	fread_number(fp));
			KEY("Extra2Flags",	obj->extra[1],	fread_number(fp));
			KEY("Extra3Flags",	obj->extra[2],	fread_number(fp));
			KEY("Extra4Flags",	obj->extra[3],	fread_number(fp));

	    case 'F':
			KEY("Fragility", obj->fragility,	fread_number(fp));
			break;

	    case 'I':
			KEY("Immmortal",	obj->immortal,	true);
			KEYS("ImpSig",		obj->imp_sig,	fread_string(fp));

			if (!str_cmp(word, "ItemType")) {
				char *item_type = fread_string(fp);

				obj_index_set_primarytype(obj, item_lookup(item_type));

				free_string(item_type);
				fMatch = true;
			}
			break;

	    case 'L':
			KEY("Level",		obj->level,		fread_number(fp));
			if( !str_cmp(word, "Lock") )
			{
				if( !obj->lock )
				{
					obj->lock = new_lock_state();
				}

				obj->lock->key_load = fread_widevnum(fp, area->uid);
				obj->lock->flags = fread_number(fp);
				obj->lock->pick_chance = fread_number(fp);
				fMatch = true;
				break;
			}

			KEYS("LongDesc",	obj->description,	fread_string(fp));
			break;

	    case 'M':
			/*
			if( !str_cmp(word, "MapWaypoint") )
			{
				WAYPOINT_DATA *wp = new_waypoint();

				wp->w = fread_number(fp);
				wp->x = fread_number(fp);
				wp->y = fread_number(fp);
				wp->name = fread_string(fp);

				if( !obj->waypoints )
				{
					obj->waypoints = new_waypoints_list();
				}

				list_appendlink(obj->waypoints, wp);

				fMatch = true;
				break;
			}
			*/

			if (!str_cmp(word, "Material"))
			{
				obj->material = material_lookup(fread_string(fp));
				fMatch = true;
				break;
			}
			break;

		case 'N':
			KEYS("Name",	obj->name,	fread_string(fp));
			break;

		case 'O':
			if (!str_cmp(word, "ObjProg"))
			{
				char *p;

				WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
				p = fread_string(fp);

				struct trigger_type *tt = get_trigger_type(p, PRG_OPROG);
				if(!tt) {
					sprintf(buf, "read_obj_new: invalid trigger type %s", p);
					bug(buf, 0);
				} else {
					opr = new_trigger();

					opr->wnum_load = wnum_load;
					opr->trig_type = tt->type;
					opr->trig_phrase = fread_string(fp);
					if( tt->type == TRIG_SPELLCAST ) {
						char buf[MIL];
						SKILL_DATA *sk = get_skill_data(opr->trig_phrase);

						if( !IS_VALID(sk) ) {
							sprintf(buf, "read_obj_new: invalid spell '%s' for TRIG_SPELLCAST", opr->trig_phrase);
							bug(buf, 0);
							free_trigger(opr);
							fMatch = true;
							break;
						}

						free_string(opr->trig_phrase);
						sprintf(buf, "%d", sk->uid);
						opr->trig_phrase = str_dup(buf);
						opr->trig_number = sk->uid;
						opr->numeric = true;
					} else {
						opr->trig_number = atoi(opr->trig_phrase);
						opr->numeric = is_number(opr->trig_phrase);
					}
					opr->trig_number = atoi(opr->trig_phrase);
					opr->numeric = is_number(opr->trig_phrase);
					//SET_BIT(room->rprog_flags, rpr->trig_type);

					if(!obj->progs) obj->progs = new_prog_bank();

					list_appendlink(obj->progs[tt->slot], opr);
					trigger_type_add_use(tt);
				}
				fMatch = true;
			}
			break;

	    case 'P':
			if(!str_cmp(word, "Persist")) {
				obj->persist = true;
				fMatch = true;
			}
	        KEY("Points",	obj->points, fread_number(fp));
			break;

	    case 'R':
			if (!str_cmp(word, "Race")) {
				char *name = fread_string(fp);
				RACE_DATA *race = get_race_data(name);

				if (IS_VALID(race))
					list_appendlink(obj->race, race);

				fMatch = true;
				break;
			}
			break;

	    case 'S':
			KEYS("ShortDesc",	obj->short_descr,	fread_string(fp));
			KEYS("Skeywds",		obj->skeywds,			fread_string(fp));

			if (!str_cmp(word, "SpellNew"))
			{
				SKILL_DATA *skill;

				fMatch = true;
				skill = get_skill_data(fread_string(fp));
				if (IS_VALID(skill))
				{
					spell = new_spell();
					spell->skill = skill;
					spell->level = fread_number(fp);
					spell->repop = fread_number(fp);

					spell->next = obj->spells;
					obj->spells = spell;
				}
				else
				{
					sprintf(buf, "Bad spell name for %s (%ld).", obj->short_descr, obj->vnum);
					bug(buf,0);
				}
			}

		break;

            case 'T':
	        KEY("TimesAllowedFixed",	obj->times_allowed_fixed,	fread_number(fp));
		KEY("Timer",			obj->timer,			fread_number(fp));
		break;

	    case 'U':
	        KEY("Update",	obj->update,	fread_number(fp));
	        break;

            case 'V':
		if (!str_cmp(word, "Values")) {
		    int i;

		    for (i = 0; i < MAX_OBJVALUES; i++)
				values[i] = fread_number(fp);

		    fMatch = true;
		}
		if (olc_load_index_vars(fp, word, &obj->index_vars, area))
		{
			fMatch = true;
			break;
		}

		break;

	    case 'W':
	        KEY("WearFlags",	obj->wear_flags,	fread_number(fp));
		KEY("Weight",		obj->weight,		fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_object_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

	/*
	// TODO: Correct
    if (obj->item_type == ITEM_WEAPON && !has_imp_sig(NULL, obj)
    &&  (obj->value[0] == WEAPON_ARROW || obj->value[0] == WEAPON_BOLT))
	set_weapon_dice(obj);
	*/
/*
    if (IS_SET(obj->extra[0], ITEM_ANTI_GOOD)) {
	sprintf(buf, "read_object_new: anti-good flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }

    if (IS_SET(obj->extra[0], ITEM_ANTI_EVIL)) {
	sprintf(buf, "read_object_new: anti-evil flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }

    if (IS_SET(obj->extra[0], ITEM_ANTI_NEUTRAL)) {
	sprintf(buf, "read_object_new: anti-neutral flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }
*/
/*
    if (obj->item_type == ITEM_SCROLL ||
    	obj->item_type == ITEM_POTION ||
    	obj->item_type == ITEM_PILL ||
    	obj->item_type == ITEM_STAFF ||
    	obj->item_type == ITEM_WAND)
    {
		fix_magic_object_index(obj);
	}
	*/

	if (area->version_object < VERSION_OBJECT_005)
	{
		if (obj->item_type == ITEM_MONEY)
		{
			if (!IS_MONEY(obj)) MONEY(obj) = new_money_data();

			MONEY(obj)->silver = values[0];
			MONEY(obj)->gold = values[1];
		}
	}

	if (area->version_object < VERSION_OBJECT_006)
	{
		if (obj->item_type == ITEM_FOOD)
		{
			if (!IS_FOOD(obj)) FOOD(obj) = new_food_data();

			FOOD(obj)->full = values[0];
			FOOD(obj)->hunger = values[1];
			FOOD(obj)->poison = values[3];

			// No food buffs yet
		}
	}

	if (area->version_object < VERSION_OBJECT_007)
	{
		if (obj->item_type == ITEM_CONTAINER)
		{
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = values[0];
			CONTAINER(obj)->flags = values[1];
			CONTAINER(obj)->max_volume = values[3];
			CONTAINER(obj)->weight_multiplier = values[4];

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
#if 0
		else if (obj->item_type == ITEM_WEAPON_CONTAINER)
		{
			obj->item_type = ITEM_CONTAINER;
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = values[0];
			CONTAINER(obj)->flags = 0;
			CONTAINER(obj)->max_volume = values[3];
			CONTAINER(obj)->weight_multiplier = values[4];

			CONTAINER_FILTER *filter = new_container_filter();
			filter->item_type = ITEM_WEAPON;
			filter->sub_type = values[1];
			list_appendlink(CONTAINER(obj)->whitelist, filter);

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
		else if (obj->item_type == ITEM_KEYRING)
		{
			obj->item_type = ITEM_CONTAINER;
			if (!IS_CONTAINER(obj)) CONTAINER(obj) = new_container_data();

			CONTAINER(obj)->max_weight = -1;	// No weight limit
			CONTAINER(obj)->flags = CONT_SINGULAR | CONT_NO_DUPLICATES;
			CONTAINER(obj)->max_volume = 50;
			CONTAINER(obj)->weight_multiplier = 100;

			CONTAINER_FILTER *filter = new_container_filter();
			filter->item_type = ITEM_KEY;
			filter->sub_type = -1;
			list_appendlink(CONTAINER(obj)->whitelist, filter);

			CONTAINER(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
#endif
	}

	if (area->version_object < VERSION_OBJECT_008)
	{
		if (obj->item_type == ITEM_LIGHT)
		{
			if (!IS_LIGHT(obj)) LIGHT(obj) = new_light_data();

			if (values[2] > 0)
			{
				LIGHT(obj)->flags = LIGHT_REMOVE_ON_EXTINGUISH;
				LIGHT(obj)->duration = values[2];
			}
			else if(values[2] < 0)
				LIGHT(obj)->duration = -1;
		}
	}

	if (area->version_object < VERSION_OBJECT_009)
	{
		if (obj->item_type == ITEM_FURNITURE)
		{
			if (!IS_FURNITURE(obj)) FURNITURE(obj) = new_furniture_data();

			FURNITURE_COMPARTMENT *compartment = new_furniture_compartment();

			// Compartment copies the string data of the object itself.
			free_string(compartment->name);
			compartment->name = str_dup(obj->name);
			free_string(compartment->short_descr);
			compartment->short_descr = str_dup(obj->short_descr);
			free_string(compartment->description);
			compartment->description = str_dup(obj->full_description);

			compartment->max_occupants = values[0];
			compartment->max_weight = values[1];

			if (IS_SET(values[2], STAND_AT)) SET_BIT(compartment->standing, FURNITURE_AT);
			if (IS_SET(values[2], STAND_ON)) SET_BIT(compartment->standing, FURNITURE_ON);
			if (IS_SET(values[2], STAND_IN)) SET_BIT(compartment->standing, FURNITURE_IN);
			if (IS_SET(values[2], SIT_AT)) SET_BIT(compartment->sitting, FURNITURE_AT);
			if (IS_SET(values[2], SIT_ON)) SET_BIT(compartment->sitting, FURNITURE_ON);
			if (IS_SET(values[2], SIT_IN)) SET_BIT(compartment->sitting, FURNITURE_IN);
			if (IS_SET(values[2], REST_AT)) SET_BIT(compartment->resting, FURNITURE_AT);
			if (IS_SET(values[2], REST_ON)) SET_BIT(compartment->resting, FURNITURE_ON);
			if (IS_SET(values[2], REST_IN)) SET_BIT(compartment->resting, FURNITURE_IN);
			if (IS_SET(values[2], SLEEP_AT)) SET_BIT(compartment->sleeping, FURNITURE_AT);
			if (IS_SET(values[2], SLEEP_ON)) SET_BIT(compartment->sleeping, FURNITURE_ON);
			if (IS_SET(values[2], SLEEP_IN)) SET_BIT(compartment->sleeping, FURNITURE_IN);

			compartment->health_regen = values[3];
			compartment->mana_regen = values[4];
			compartment->move_regen = values[5];

			list_appendlink(FURNITURE(obj)->compartments, compartment);
			FURNITURE(obj)->main_compartment = 1;

			compartment->lock = obj->lock;
			obj->lock = NULL;
		}
	}


	if (area->version_object < VERSION_OBJECT_010)
	{
		if (obj->item_type == ITEM_PORTAL)
		{
			if (!IS_PORTAL(obj)) PORTAL(obj) = new_portal_data();

			free_string(PORTAL(obj)->name);
			PORTAL(obj)->name = str_dup(obj->name);
			free_string(PORTAL(obj)->short_descr);
			PORTAL(obj)->short_descr = str_dup(obj->short_descr);

			PORTAL(obj)->charges = values[0];
			PORTAL(obj)->exit = values[1];
			PORTAL(obj)->flags = values[2];
			PORTAL(obj)->type = values[3];

			for(int i = 0; i < MAX_PORTAL_VALUES; i++)
				PORTAL(obj)->params[i] = values[5 + i];

			PORTAL(obj)->lock = obj->lock;
			obj->lock = NULL;

			SPELL_DATA *spell, *spell_next;
			for(spell = obj->spells; spell; spell = spell_next)
			{
				spell_next = spell->next;
				spell->next = NULL;
				list_appendlink(PORTAL(obj)->spells, spell);
			}
			obj->spells = NULL;
		}
	}

	if (area->version_object < VERSION_OBJECT_011)
	{
		if (obj->item_type == ITEM_BOOK)
		{
			if(!IS_BOOK(obj)) BOOK(obj) = new_book_data();

			free_string(BOOK(obj)->name);
			BOOK(obj)->name = str_dup(obj->name);
			free_string(BOOK(obj)->short_descr);
			BOOK(obj)->short_descr = str_dup(obj->short_descr);

			BOOK(obj)->flags = values[1] & (BOOK_CLOSEABLE | BOOK_CLOSED | BOOK_CLOSELOCK | BOOK_PUSHOPEN);

			BOOK(obj)->lock = obj->lock;
			obj->lock = NULL;
		}
	}

	if (area->version_object < VERSION_OBJECT_012)
	{
#if 0
		if (obj->item_type == ITEM_BLANK_SCROLL)
		{
			// Convert to ITEM_SCROLL with no spells
			if(!IS_SCROLL(obj)) SCROLL(obj) = new_scroll_data();

			SCROLL(obj)->max_mana = (values[0] > 0) ? values[0] : 200;
			obj->item_type = ITEM_SCROLL;
		}
		else if (obj->item_type == ITEM_EMPTY_VIAL)
		{
			// Convert to ITEM_FLUID_CONTAINER with no spells
			if(!IS_FLUID_CON(obj)) FLUID_CON(obj) = new_fluid_container_data();

			FLUID_CON(obj)->amount = LIQ_SERVING;
			FLUID_CON(obj)->capacity = LIQ_SERVING;
			FLUID_CON(obj)->liquid = liquid_water;

			obj->item_type = ITEM_FLUID_CONTAINER;
		}
		else
#endif
		if (obj->item_type == ITEM_SCROLL)
		{
			if(!IS_SCROLL(obj)) SCROLL(obj) = new_scroll_data();

			SCROLL(obj)->max_mana = 0;

			SPELL_DATA *spell, *spell_next;
			for(spell = obj->spells; spell; spell = spell_next)
			{
				spell_next = spell->next;
				spell->next = NULL;
				list_appendlink(SCROLL(obj)->spells, spell);
			}
			obj->spells = NULL;
		}
		else if (obj->item_type == ITEM_TATTOO)
		{
			if(!IS_TATTOO(obj)) TATTOO(obj) = new_tattoo_data();

			TATTOO(obj)->touches = values[0];
			TATTOO(obj)->fading_chance = values[1];

			SPELL_DATA *spell, *spell_next;
			for(spell = obj->spells; spell; spell = spell_next)
			{
				spell_next = spell->next;
				spell->next = NULL;
				list_appendlink(TATTOO(obj)->spells, spell);
			}
			obj->spells = NULL;
		}
		else if (obj->item_type == ITEM_WAND)
		{
			if(!IS_WAND(obj)) WAND(obj) = new_wand_data();

			WAND(obj)->charges = values[2];
			WAND(obj)->max_charges = values[1];

			SPELL_DATA *spell, *spell_next;
			for(spell = obj->spells; spell; spell = spell_next)
			{
				spell_next = spell->next;
				spell->next = NULL;
				list_appendlink(WAND(obj)->spells, spell);
			}
			obj->spells = NULL;
		}
	}

	if (area->version_object < VERSION_OBJECT_013)
	{
		if (obj->item_type == ITEM_INK)
		{
			// This will only ever have atmost THREE different catalyst types from the old format

			if (IS_INK(obj)) free_ink_data(INK(obj));

			INK(obj) = new_ink_data();
			int16_t amounts[CATALYST_MAX];

			// Tally up how much is on each type
			// Example:
			// v0  air
			// v1  air
			// v2  water
			//
			// Should tally up to 2 air and 1 water
			for(int i = 0; i < CATALYST_MAX; i++) amounts[i] = 0;
			for(int i = 0; i < 3; i++)
				if (values[i] > CATALYST_NONE && values[i] < CATALYST_MAX)
					amounts[values[i]]++;

			// Copy over tallies
			int n = 0;
			for(int i = 0; i < CATALYST_MAX && n < 3; i++)
			{
				if (amounts[i] > 0)
				{
					INK(obj)->types[n] = i;
					INK(obj)->amounts[n] = amounts[i];
				}
			}
		}
		else if (obj->item_type == ITEM_INSTRUMENT)
		{
			if (IS_INSTRUMENT(obj)) free_instrument_data(INSTRUMENT(obj));

			INSTRUMENT(obj) = new_instrument_data();

			INSTRUMENT(obj)->type = values[0];
			INSTRUMENT(obj)->flags = values[1];
			INSTRUMENT(obj)->beats_min = values[2];
			INSTRUMENT(obj)->beats_max = values[3];
		}
	}

	if (area->version_object < VERSION_OBJECT_014)
	{
		// NEVERMIND
	}

	if (area->version_object < VERSION_OBJECT_015)
	{
		if (obj->item_type == ITEM_ICE_STORM)
		{
			obj_index_reset_multitype(obj);
			obj_index_set_primarytype(obj, ITEM_MIST);
			MIST(obj)->icy = 3;
		}
		else if (obj->item_type == ITEM_ROOM_FLAME)
		{
			obj_index_reset_multitype(obj);
			obj_index_set_primarytype(obj, ITEM_MIST);
			MIST(obj)->fiery = 3;
		}
		else if (obj->item_type == ITEM_STINKING_CLOUD)
		{
			obj_index_reset_multitype(obj);
			obj_index_set_primarytype(obj, ITEM_MIST);
			MIST(obj)->stink = 4;
		}
		else if (obj->item_type == ITEM_WITHERING_CLOUD)
		{
			obj_index_reset_multitype(obj);
			obj_index_set_primarytype(obj, ITEM_MIST);
			MIST(obj)->wither = 4;
		}
	}

	if (area->version_object < VERSION_OBJECT_016)
	{
		int new_fragility = obj->fragility;
		switch(obj->fragility)
		{
			case OLD_OBJ_FRAGILE_WEAK: new_fragility = OBJ_FRAGILE_WEAK; break;
			case OLD_OBJ_FRAGILE_NORMAL: new_fragility = OBJ_FRAGILE_NORMAL; break;
			case OLD_OBJ_FRAGILE_STRONG: new_fragility = OBJ_FRAGILE_STRONG; break;
			case OLD_OBJ_FRAGILE_SOLID: new_fragility = OBJ_FRAGILE_SOLID; break;
		}

		obj->fragility = new_fragility;
	}

	// Remove when all item multi-typing is complete
	for(int i = 0; i < MAX_OBJVALUES; i++)
		obj->value[i] = values[i];

	// Any object in an immortal area is automatically flagged as an immortal object
	if (IS_SET(area->area_flags, AREA_IMMORTAL))
		obj->immortal = true;

    return obj;
}


/* read one script into an area */
SCRIPT_DATA *read_script_new(FILE *fp, AREA_DATA *area, int type)
{
	SCRIPT_DATA *scr = NULL;
	char *word;
	char *last_word;

	switch(type)
	{
		case IFC_M:	last_word = "#-MOBPROG";		break;
		case IFC_O:	last_word = "#-OBJPROG";		break;
		case IFC_R:	last_word = "#-ROOMPROG";		break;
		case IFC_T:	last_word = "#-TOKENPROG";		break;
		case IFC_A:	last_word = "#-AREAPROG";		break;
		case IFC_I:	last_word = "#-INSTANCEPROG";	break;
		case IFC_D:	last_word = "#-DUNGEONPROG";	break;
		default:
			return NULL;
	}

	scr = new_script();
	if(!scr) return NULL;
	scr->vnum = fread_number(fp);
	scr->area = area;

	while (str_cmp((word = fread_word(fp)), last_word)) {
		fMatch = false;

		switch (word[0]) {
		case 'C':
			KEYS("Code",		scr->edit_src,	fread_string(fp));
			KEYS("Comments",	scr->comments,	fread_string(fp));
			break;
		case 'D':
			KEY("Depth",		scr->depth,	fread_number(fp));
			break;
		case 'F':
			if (!str_cmp(word, "Flags")) {
			    char *str = fread_string(fp);
			    long value = flag_value(script_flags,str);

			    scr->flags = (value != NO_FLAG) ? value : 0;

			    free_string(str);
			    fMatch = true;
			}
			break;

		case 'N':
			KEYS("Name",		scr->name,	fread_string(fp));
			break;
		case 'S':
			KEY("Security",		scr->security,	fread_number(fp));
			break;
		}

		if (!fMatch) {
			sprintf(buf, "read_script_new: no match for word %s", word);
			bug(buf, 0);
		}
	}

	if(scr) {
		if(!scr->edit_src) {
			free_script(scr);
			return NULL;
		}

		// This will keep the script, so it can be fixed!
		compile_script(NULL,scr,scr->edit_src,type);
	}

	return scr;
}


/* read in an extra descr */
EXTRA_DESCR_DATA *read_extra_descr_new(FILE *fp)
{
	EXTRA_DESCR_DATA *ed;
	char *word;

	ed = new_extra_descr();
	ed->keyword = fread_string(fp);

	while (str_cmp((word = fread_word(fp)), "#-EXTRA_DESCR"))
	{
		fMatch = false;
		switch (word[0])
		{
		case 'D':
			KEYS("Description",	ed->description,	fread_string(fp));
			break;

		case 'E':
			if( !str_cmp(word, "Environmental") )
			{
				if( ed->description )
					free_string(ed->description);
				ed->description = NULL;
				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch) {
			sprintf(buf, "read_extra_descr_new: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return ed;
}


/* read in a conditional descr */
CONDITIONAL_DESCR_DATA *read_conditional_descr_new(FILE *fp)
{
    CONDITIONAL_DESCR_DATA *cd;
    char *word;

    cd = new_conditional_descr();

    while (str_cmp((word = fread_word(fp)), "#-CONDITIONAL_DESCR")) {
	fMatch = false;
	switch (word[0]) {
	    case 'C':
	        KEY("Condition",	cd->condition,	fread_number(fp));
		break;

	    case 'D':
		KEYS("Description",	cd->description,	fread_string(fp));
		break;

	    case 'P':
		KEY("Phrase",		cd->phrase,	fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_conditional_descr_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return cd;
}


/* read in an exit */
EXIT_DATA *read_exit_new(FILE *fp, AREA_DATA *area)
{
    EXIT_DATA *ex;
    char *word;
    char buf[MSL];

    ex = new_exit();
    word = fread_word(fp);
    if( is_number(word) )
    	ex->orig_door = atoi(word);
    else
    	ex->orig_door = parse_direction(word);
    while (str_cmp((word = fread_word(fp)), "#-X")) {
	fMatch = false;

	switch (word[0]) {
	    case 'D':
		KEYS("Description",	ex->short_desc, fread_string(fp));
		break;

	    case 'K':
	        KEY("Key",		ex->door.rs_lock.key_load,	fread_widevnum(fp, area->uid));

		if (!str_cmp(word, "Keyword")) {
		    int i;
		    char letter;

		    fMatch = true;

                    letter = getc(fp);
		    for (i = 0, letter = getc(fp); letter != '~'; i++) {
			buf[i] = letter;
			letter = getc(fp);
		    }

		    buf[i] = '\0';

		    free_string(ex->keyword);
		    ex->keyword = str_dup(buf);
		}
		break;

	    case 'L':
		KEY("LockFlags",		ex->door.rs_lock.flags,	fread_number(fp));
		KEYS("LongDescription",	ex->long_desc, fread_string(fp));
		break;

		case 'P':
		KEY("PickChance",		ex->door.rs_lock.pick_chance,	fread_number(fp));


	    case 'R':
		KEY("Rs_flags",	ex->rs_flags,	fread_number(fp));
		break;

	    case 'T':
	        if (!str_cmp(word, "To_room")) {
			    ex->u1.wnum = fread_widevnum(fp, area->uid);
			    fMatch = true;
			}

			break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_exit_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }



    return ex;
}


/* read in a reset */
RESET_DATA *read_reset_new(FILE *fp, AREA_DATA *area)
{
    RESET_DATA *reset;
    char *word;

    reset = new_reset_data();
    reset->command = fread_letter(fp);

    while (str_cmp((word = fread_word(fp)), "#-RESET")) {
	fMatch = false;
	switch (word[0]) {
	    case 'A':
	    if (!str_cmp(word, "Arguments")) {
                reset->arg1.load = fread_widevnum(fp, area->uid);
                reset->arg2 = fread_number(fp);
                reset->arg3.load = fread_widevnum(fp, area->uid);
                reset->arg4 = fread_number(fp);
		fMatch = true;
	    }

	    break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_reset_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return reset;
}


/* read in an obj affect */
AFFECT_DATA *read_obj_affect_new(FILE *fp)
{
    AFFECT_DATA *af;
    char *word;

    af = new_affect();
    af->where = fread_number(fp);

    while (str_cmp((word = fread_word(fp)), "#-AFFECT")) {
	fMatch = false;
	switch (word[0]) {
	    case 'B':
	        KEY("BitVector",	af->bitvector,	fread_number(fp));
	        KEY("BitVector2",	af->bitvector2,	fread_number(fp));
		break;

	    case 'D':
	        KEY("Duration",	af->duration,	fread_number(fp));
		break;

	    case 'L':
	        KEY("Level",		af->level,	fread_number(fp));
	        KEY("Location",	af->location,	fread_number(fp));
		break;

	    case 'M':
		KEY("Modifier",	af->modifier,	fread_number(fp));
		break;

	    case 'R':
		KEY("Random",		af->random,	fread_number(fp));
	        break;

		case 'S':
			KEY("Skill",	af->skill,	get_skill_data(fread_string(fp)));
			break;

            case 'T':
	        KEY("Type",		af->catalyst_type,	fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_obj_affect_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return af;
}

/* read in an obj affect */
AFFECT_DATA *read_obj_catalyst_new(FILE *fp)
{
    AFFECT_DATA *af;
    char *word;

    af = new_affect();
    af->catalyst_type = flag_value(catalyst_types,fread_string_eol(fp));
    af->where = TO_CATALYST_DORMANT;

    while (str_cmp((word = fread_word(fp)), "#-CATALYST")) {
	fMatch = false;
	switch (word[0]) {
		case 'A':
			if (!str_cmp(word, "Active")) {
				fread_to_eol(fp);
				fMatch = true;
				af->where = TO_CATALYST_ACTIVE;
			}
			break;

	    case 'C':
	        KEY("Charges",	af->modifier,	fread_number(fp));
		break;

		case 'N':
			KEYS("Name",	af->custom_name,	fread_string_eol(fp));

	    case 'R':
		KEY("Random",		af->random,	fread_number(fp));
	        break;

		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_obj_catalyst_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return af;
}


MISSIONARY_DATA *read_missionary_new(FILE *fp, AREA_DATA *area)
{
    MISSIONARY_DATA *missionary;
    char *word;

    missionary = new_missionary_data();

    while (str_cmp((word = fread_word(fp)), "#-MISSIONARY"))
    {
	fMatch = false;
	switch (word[0]) {
		case 'F':
	        KEYS("Footer",		missionary->footer,	fread_string(fp));
	        break;

	    case 'H':
	        KEYS("Header",		missionary->header,	fread_string(fp));
	        break;

	    case 'K':
	        KEYS("Keywords",	missionary->keywords,	fread_string(fp));
	        break;

	    case 'L':
	        KEY("LineWidth",	missionary->line_width,	fread_number(fp));
	        KEYS("LongDescr",	missionary->long_descr,	fread_string(fp));
			break;

	    case 'P':
	        KEYS("Prefix",		missionary->prefix,	fread_string(fp));
	        break;

	    case 'S':
	        KEY("Scroll",		missionary->scroll,	fread_widevnum(fp, area->uid));
	        KEYS("ShortDescr",	missionary->short_descr,	fread_string(fp));
	        KEYS("Suffix",		missionary->suffix,	fread_string(fp));
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_missionary_new: no match for word %s", word);
			bug(buf, 0);
		}
    }

    return missionary;
}

PRACTICE_COST_DATA *read_practice_cost_data(FILE *fp, AREA_DATA *area)
{
	PRACTICE_COST_DATA *data;
	char *word;

	data = new_practice_cost_data();

    while (str_cmp((word = fread_word(fp)), "#-COST"))
    {
		fMatch = false;

		switch (word[0]) {
		case 'C':
			if (!str_cmp(word, "CustomPricing"))
			{
				data->custom_price = fread_string(fp);
				data->check_price_load = fread_widevnum(fp, area->uid);
				fMatch = true;
				break;
			}
			break;

		case 'D':
			KEY("DeityPnts", data->dp, fread_number(fp));
			break;

		case 'M':
			KEY("MinRating", data->min_rating, fread_number(fp));
			break;

		case 'O':
			KEY("Object", data->obj_load, fread_widevnum(fp, area->uid));
			break;

		case 'P':
			KEY("Paragon", data->paragon_levels, fread_number(fp));
			KEY("Pneuma", data->pneuma, fread_number(fp));
			KEY("Practices", data->practices, fread_number(fp));
			break;

		case 'Q':
			KEY("QuestPnts", data->qp, fread_number(fp));
			break;

		case 'R':
			KEY("RepPoints", data->rep_points, fread_number(fp));
			KEY("Reputation", data->reputation_load, fread_widevnum(fp, area->uid));
			break;

		case 'S':
			KEY("Silver", data->silver, fread_number(fp));
			break;

		case 'T':
			KEY("Trains", data->trains, fread_number(fp));
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_practice_cost_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

PRACTICE_ENTRY_DATA *read_practice_entry_data(FILE *fp, AREA_DATA *area)
{
	PRACTICE_ENTRY_DATA *data;
	char *word;

	data = new_practice_entry_data();

    while (str_cmp((word = fread_word(fp)), "#-ENTRY"))
    {
		fMatch = false;
		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#COST"))
			{
				PRACTICE_COST_DATA *cost = read_practice_cost_data(fp, area);

				list_appendlink(data->costs, cost);
				cost->entry = data;

				fMatch = true;
				break;
			}
			break;

		case 'C':
			KEY("CheckScript", data->check_script_load, fread_widevnum(fp, area->uid));
			break;

		case 'F':
			KEY("Flags", data->flags, fread_flag(fp));
			break;

		case 'M':
			KEY("MaxRating", data->max_rating, fread_number(fp));
			break;

		case 'R':
			if (!str_cmp(word, "Reputation"))
			{
				data->reputation_load = fread_widevnum(fp, area->uid);
				data->min_reputation_rank = fread_number(fp);
				data->max_reputation_rank = fread_number(fp);

				fMatch = true;
				break;
			}
			break;

		case 'S':
			if (!str_cmp(word, "Skill"))
			{
				data->skill = get_skill_data(fread_string(fp));
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "Song"))
			{
				data->song = get_song_data(fread_string(fp));
				fMatch = true;
				break;
			}
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_practice_entry_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;

}

PRACTICE_DATA *read_practice_data(FILE *fp, AREA_DATA *area)
{
	PRACTICE_DATA *data;
	char *word;

	data = new_practice_data();

    while (str_cmp((word = fread_word(fp)), "#-PRACTICE"))
    {
		fMatch = false;
		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#ENTRY"))
			{
				PRACTICE_ENTRY_DATA *entry = read_practice_entry_data(fp, area);

				list_appendlink(data->entries, entry);

				fMatch = true;
				break;
			}
			break;

		case 'S':
			KEY("Standard", data->standard, true);

		}

		if (!fMatch) {
			sprintf(buf, "read_practice_data: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;

}

SHOP_STOCK_DATA *read_shop_stock_new(FILE *fp, AREA_DATA *area)
{
	SHOP_STOCK_DATA *stock;
	char *word;

	stock = new_shop_stock();

    while (str_cmp((word = fread_word(fp)), "#-STOCK"))
    {
		fMatch = false;
		switch (word[0]) {
		case 'C':
			if(!str_cmp(word, "Crew"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_CREW;
				break;
			}
			if (!str_cmp(word, "CustomPricing"))
			{
				stock->custom_price = fread_string(fp);
				stock->check_price_load = fread_widevnum(fp, area->uid);
				fMatch = true;
				break;
			}
			break;

		case 'D':
			KEY("DeityPnts", stock->dp, fread_number(fp));
			KEYS("Description", stock->custom_descr, fread_string(fp));
			KEY("Discount", stock->discount, fread_number(fp));
			KEY("Duration", stock->duration, fread_number(fp));
			break;
		case 'G':
			if(!str_cmp(word, "Guard"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_GUARD;
				break;
			}
			break;
		case 'K':
			if(!str_cmp(word, "Keyword"))
			{
				fMatch = true;
				stock->custom_keyword = fread_string(fp);
				stock->wnum_load.auid = 0;
				stock->wnum_load.vnum = 0;
				stock->type = STOCK_CUSTOM;
				break;
			}
			break;
		case 'L':
			KEY("Level", stock->level, fread_number(fp));
			break;
		case 'M':
			if(!str_cmp(word, "Mount"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_MOUNT;
				break;
			}
			break;
		case 'O':
			if(!str_cmp(word, "Object"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_OBJECT;
				break;
			}
			break;
		case 'P':
			KEY("Paragon", stock->paragon_levels, fread_number(fp));
			if(!str_cmp(word, "Pet"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_PET;
				break;
			}
			KEY("Pneuma", stock->pneuma, fread_number(fp));
			KEYS("Pricing", stock->custom_price, fread_string(fp));
			break;
		case 'Q':
			KEY("Quantity", stock->quantity, fread_number(fp));
			KEY("QuestPnts", stock->qp, fread_number(fp));
			break;
		case 'R':
			KEY("RepPoints", stock->rep_points, fread_number(fp));
			if (!str_cmp(word, "Reputation"))
			{
				stock->reputation_load = fread_widevnum(fp, area->uid);
				stock->min_reputation_rank = fread_number(fp);
				stock->max_reputation_rank = fread_number(fp);
				stock->min_show_rank = 0;
				stock->max_show_rank = 0;
				fMatch = true;
				break;
			}
			if (!str_cmp(word, "ReputationShow"))
			{
				stock->reputation_load = fread_widevnum(fp, area->uid);
				stock->min_reputation_rank = fread_number(fp);
				stock->max_reputation_rank = fread_number(fp);
				stock->min_show_rank = fread_number(fp);
				stock->max_show_rank = fread_number(fp);
				fMatch = true;
				break;
			}
			KEY("RestockRate", stock->restock_rate, fread_number(fp));
			break;
		case 'S':
			if(!str_cmp(word, "Ship"))
			{
				fMatch = true;
				stock->wnum_load = fread_widevnum(fp, area->uid);
				stock->type = STOCK_SHIP;
				break;
			}
			KEY("Silver", stock->silver, fread_number(fp));
			KEY("Singular", stock->singular, true);
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_shop_stock_new: no match for word %s", word);
			bug(buf, 0);
		}
	}

	stock->discount = URANGE(0, stock->discount, 100);

	// Clear out the custom price if the script isn't set
	if (!IS_NULLSTR(stock->custom_price) && (stock->check_price_load.auid < 1 || stock->check_price_load.vnum < 1))
	{
		free_string(stock->custom_price);
		stock->custom_price = &str_empty[0];
	}

	return stock;
}

SHIP_CREW_INDEX_DATA *read_ship_crew_index_new(FILE *fp)
{
	SHIP_CREW_INDEX_DATA *crew;
	char *word;

	crew = new_ship_crew_index();
    while (str_cmp((word = fread_word(fp)), "#-CREW"))
    {
		fMatch = false;
		switch (word[0]) {
		case 'G':
			KEY("Gunning", crew->gunning, fread_number(fp));
			break;

		case 'L':
			KEY("Leadership", crew->leadership, fread_number(fp));
			break;

		case 'M':
			KEY("Mechanics", crew->mechanics, fread_number(fp));
			KEY("MinRank", crew->min_rank, fread_number(fp));
			break;

		case 'N':
			KEY("Navigation", crew->navigation, fread_number(fp));
			break;

		case 'O':
			KEY("Oarring", crew->oarring, fread_number(fp));
			break;

		case 'S':
			KEY("Scouting", crew->scouting, fread_number(fp));
			break;
		}

		if (!fMatch) {
			sprintf(buf, "read_ship_crew_index_new: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return crew;
}

SHOP_DATA *read_shop_new(FILE *fp, AREA_DATA *area)
{
    SHOP_DATA *shop;
    char *word;

    shop = new_shop();

    while (str_cmp((word = fread_word(fp)), "#-SHOP"))
    {
	fMatch = false;
	switch (word[0]) {
		case '#':
			if(!str_cmp(word, "#STOCK")) {
				SHOP_STOCK_DATA *stock = read_shop_stock_new(fp, area);

				if(stock) {
					stock->next = shop->stock;
					shop->stock = stock;
				}
				fMatch = true;
			}
			break;
	    case 'D':
	        KEY("Discount",	shop->discount,	fread_number(fp));
	        break;
		case 'F':
			KEY("Flags", shop->flags, fread_number(fp));
			break;
	    case 'H':
	        KEY("HourOpen",	shop->open_hour,	fread_number(fp));
	        KEY("HourClose",	shop->close_hour,	fread_number(fp));
		break;

	    case 'K':
	        KEY("Keeper",	shop->keeper,	fread_number(fp));
		break;

	    case 'P':
			KEY("ProfitBuy",	shop->profit_buy,	fread_number(fp));
			KEY("ProfitSell",	shop->profit_sell,	fread_number(fp));
			break;
		case 'R':
			if (!str_cmp(word, "Reputation"))
			{
				shop->reputation_load = fread_widevnum(fp, area->uid);
				shop->min_reputation_rank = fread_number(fp);
				fMatch = true;
				break;
			}
			KEY("RestockInterval", shop->restock_interval, fread_number(fp));
			break;

		case 'S':
			if( !str_cmp(word, "Shipyard") )
			{
				shop->shipyard = fread_number(fp);
				shop->shipyard_region[0][0] = fread_number(fp);
				shop->shipyard_region[0][1] = fread_number(fp);
				shop->shipyard_region[1][0] = fread_number(fp);
				shop->shipyard_region[1][1] = fread_number(fp);
				shop->shipyard_description = fread_string(fp);
				fMatch = true;
				break;
			}
			break;

	    case 'T':
	        if (!str_cmp(word, "Trade")) {
		    int i;

		    fMatch = true;

                    for (i = 0; i < MAX_TRADE; i++) {
			if (shop->buy_type[i] == 0) {
			    shop->buy_type[i] = fread_number(fp);
			    break;
			}
		    }
		}

		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_shop_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

	shop->discount = URANGE(0, shop->discount, 100);


    return shop;
}


TOKEN_INDEX_DATA *read_token(FILE *fp, AREA_DATA *area)
{
    TOKEN_INDEX_DATA *token = NULL;
    EXTRA_DESCR_DATA *ed;
    PROG_LIST *tpr;

    token = new_token_index();
    token->vnum = fread_number(fp);

	area->bottom_vnum_token = UMIN(area->bottom_vnum_token, token->vnum);
	area->top_vnum_token = UMAX(area->top_vnum_token, token->vnum);

    while (str_cmp((word = fread_word(fp)), "#-TOKEN")) {
	fMatch = false;

	switch(word[0]) {
	    case '#':
		if (!str_cmp(word, "#EXTRA_DESCR")) {
		    ed = read_extra_descr_new(fp);
		    ed->next = token->ed;
		    token->ed = ed;
		}

		case 'C':
			KEYS("Comments", token->comments, fread_string(fp));

	    case 'D':
	        KEYS("Description", token->description, fread_string(fp));
		break;

	    case 'F':
		KEY("Flags",	token->flags,	fread_number(fp));
		break;

	    case 'N':
	        KEYS("Name",	token->name,	fread_string(fp));
		break;

	    case 'T':
		KEY("Timer",	token->timer,	fread_number(fp));
		if (!str_cmp(word, "TokProg")) {
		    char *p;

			WNUM_LOAD wnum_load = fread_widevnum(fp, area->uid);
		    p = fread_string(fp);

		    struct trigger_type *tt = get_trigger_type(p, PRG_TPROG);
		    if(!tt) {
			    sprintf(buf, "read_token: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    tpr = new_trigger();

			    tpr->wnum_load = wnum_load;
			    tpr->trig_type = tt->type;
			    tpr->trig_phrase = fread_string(fp);
			    if( tt->type == TRIG_SPELLCAST ) {
					char buf[MIL];
					SKILL_DATA *sk = get_skill_data(tpr->trig_phrase);

					if( !IS_VALID(sk) ) {
						sprintf(buf, "read_token: invalid spell '%s' for TRIG_SPELLCAST", tpr->trig_phrase);
						bug(buf, 0);
						free_trigger(tpr);
						fMatch = true;
						break;
					}

					free_string(tpr->trig_phrase);
					sprintf(buf, "%d", sk->uid);
					tpr->trig_phrase = str_dup(buf);
					tpr->trig_number = sk->uid;
					tpr->numeric = true;

				} else {
			    	tpr->trig_number = atoi(tpr->trig_phrase);
					tpr->numeric = is_number(tpr->trig_phrase);
				}
			    tpr->trig_number = atoi(tpr->trig_phrase);
				tpr->numeric = is_number(tpr->trig_phrase);

			    if(!token->progs) token->progs = new_prog_bank();

				list_appendlink(token->progs[tt->slot], tpr);
				trigger_type_add_use(tt);
		    }
		    fMatch = true;
		}
		KEY("Type",	token->type,	fread_number(fp));
		break;

            case 'V':
				if (!str_cmp(word, "Value")) {
					int index;
					long value;

					fMatch = true;

					index = fread_number(fp);
					value = fread_number(fp);

					token->value[index] = value;
				}

				if (!str_cmp(word, "ValueName")) {
					int index;

					fMatch = true;

					index = fread_number(fp);
					token->value_name[index] = fread_string(fp);
				}
				if (olc_load_index_vars(fp, word, &token->index_vars, area))
				{
					fMatch = true;
					break;
				}

				break;

	}

	if (!fMatch) {
	    sprintf(buf, "read_token: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return token;
}


void save_area_trade( FILE *fp, AREA_DATA *pArea )
{
    TRADE_ITEM *pTrade;

    fprintf( fp, "#TRADE\n" );

    for ( pTrade = pArea->trade_list; pTrade != NULL; pTrade = pTrade->next )
    {
	fprintf( fp, "#%d\n", pTrade->trade_type );
	fprintf( fp, "%ld\n", pTrade->min_price );
	fprintf( fp, "%ld\n", pTrade->max_price );
	fprintf( fp, "%ld\n", pTrade->max_qty );
	fprintf( fp, "%ld\n", pTrade->replenish_amount );
	fprintf( fp, "%ld\n", pTrade->replenish_time );
	if (pTrade->obj_wnum.auid != pArea->uid)
		fprintf( fp, "%ld#%ld\n", pTrade->obj_wnum.auid, pTrade->obj_wnum.vnum );
	else
		fprintf( fp, "#%ld\n", pTrade->obj_wnum.vnum );
    }

    fprintf( fp, "#0\n\n\n\n" );
    return;
}

