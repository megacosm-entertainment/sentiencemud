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

		if( can_edit_blueprints(ch) )
			send_to_char("  asave blueprints - saves blueprints\n\r", ch);

		if( can_edit_dungeons(ch) )
			send_to_char("  asave dungeons   - saves dungeons\n\r", ch);

		if( can_edit_ships(ch) )
			send_to_char("  asave ships      - saves ships\n\r", ch);

		if (ch->tot_level == MAX_LEVEL)
			send_to_char("  asave staff      - saves the immortal staff information\n\r", ch);

		//send_to_char("  asave wilds    - saves wilderness templates (imps only)\n\r", ch);
		return;
	}

	// Save all areas
	if (!str_cmp("world", arg1))
	{
		/* Only for imps since it causes mucho lag */
		if (ch->tot_level < MAX_LEVEL)
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
			projects_changed = FALSE;
			send_to_char("Project list saved.\n\r", ch);
		}

		if (blueprints_changed)
		{
			save_blueprints();
			send_to_char("Blueprints saved.\n\r", ch);
		}

		if (dungeons_changed)
		{
			save_dungeons();
			send_to_char("Dungeons saved.\n\r", ch);
		}

		if (ships_changed)
		{
			save_ships();
			send_to_char("Ships saved.\n\r", ch);
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
						if (ch->tot_level < MAX_LEVEL)
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
		projects_changed = FALSE;
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

    if (!str_cmp(arg1, "blueprints"))
    {
		save_blueprints();
		send_to_char("Blueprints saved.\n\r", ch);
		return;
	}

    if (!str_cmp(arg1, "dungeons"))
    {
		save_dungeons();
		send_to_char("Dungeons saved.\n\r", ch);
		return;
	}

	if (!str_cmp(arg1, "ships"))
	{
		save_ships();
		send_to_char("Ships saved.\n\r", ch);
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


/* save an area to <area>.are */
void save_area_new(AREA_DATA *area)
{
    char buf[2*MAX_STRING_LENGTH];
    FILE *fp;
    char filename[MSL];
    OLC_POINT_BOOST *boost;

/*
	// 20140521 NIB - allowing these to be saved
    if (!str_cmp(area->name, "Netherworld")
	|| !str_cmp(area->name, "Eden")) {
	log_string("save_area_new: not saving wilderness file");
	return;
    }
    */

    // There are some areas which should be saved specially
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
    fprintf(fp, "AreaWho %d\n", 	area->area_who);
    fprintf(fp, "PlaceType %ld\n", 	area->place_flags);
    fprintf(fp, "AreaFlags %ld\n", 	area->area_flags);
    fprintf(fp, "Builders %s~\n",      	fix_string(area->builders));
    fprintf(fp, "VNUMs %ld %ld\n",     	area->min_vnum, area->max_vnum);
	fprintf(fp, "Levels %d %d\n", area->min_level, area->max_level);
    fprintf(fp, "WildsVnum %ld\n",	area->wilds_uid);
    fprintf(fp, "XCoord %d\n", 		area->x);
    fprintf(fp, "YCoord %d\n", 		area->y);
    fprintf(fp, "XLand %d\n",	 	area->land_x);
    fprintf(fp, "YLand %d\n", 		area->land_y);
    fprintf(fp, "Credits %s~\n",	area->credits);
    fprintf(fp, "Security %d\n",       	area->security);
    if(area->recall.wuid)
	fprintf(fp, "RecallW %lu %lu %lu %lu\n", 	area->recall.wuid, area->recall.id[0], area->recall.id[1], area->recall.id[2]);
    else
	fprintf(fp, "Recall %ld\n", 	area->recall.id[0]);
    fprintf(fp, "Open %d\n", 	  	area->open);
    fprintf(fp, "Repop %d\n",		area->repop);
    fprintf(fp, "PostOffice %ld\n",	area->post_office);
    fprintf(fp, "AirshipLand %ld\n", 	area->airship_land_spot);
	fprintf(fp, "Description %s~\n", fix_string(area->description));
	if(area->comments)
		fprintf(fp, "Comments %s~\n", fix_string(area->comments));
	if(area->notes)
		fprintf(fp, "Notes %s~\n", fix_string(area->notes));
    // Save the current versions of everything
    fprintf(fp, "VersArea %d\n",	VERSION_AREA);
    fprintf(fp, "VersMobile %d\n",	VERSION_MOBILE);
    fprintf(fp, "VersObject %d\n",	VERSION_OBJECT);
    fprintf(fp, "VersRoom %d\n",	VERSION_ROOM);
    fprintf(fp, "VersToken %d\n",	VERSION_TOKEN);
    fprintf(fp, "VersScript %d\n",	VERSION_SCRIPT);
    fprintf(fp, "VersWilds %d\n",	VERSION_WILDS);

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
				fprintf(fp, "AreaProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	if(area->index_vars) {
		for(pVARIABLE var = area->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}

    /* Whisp - write this function */
    save_area_trade(fp, area);

    if (!IS_SET(area->area_flags, AREA_NO_ROOMS)/*str_prefix("Maze-Level", area->name) && str_cmp("Geldoff's Maze", area->name)*/)
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
    int i, j, r;

    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
	i = 0; r = MAX_KEY_HASH;
    } else {
	i = area->min_vnum % MAX_KEY_HASH;
	r = area->max_vnum - area->min_vnum + 1;
    }

    for (j = 0; j < r; j++)
    {
	for (room = room_index_hash[i];room;room = room->next)
	    if (room->vnum && room->area == area)// Keep from saving room 0! JIC!!!
		save_room_new(fp, room, ROOMTYPE_NORMAL);

	if (++i == MAX_KEY_HASH)
	    i = 0;
    }
}


/* save all mobiles in an area */
void save_mobiles_new(FILE *fp, AREA_DATA *area)
{
    MOB_INDEX_DATA *mob;
    int i, j, r;

    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
	i = 0; r = MAX_KEY_HASH;
    } else {
	i = area->min_vnum % MAX_KEY_HASH;
	r = area->max_vnum - area->min_vnum + 1;
    }

    for (j = 0; j < r; j++)
    {
	for (mob = mob_index_hash[i];mob;mob = mob->next)
	    if (mob->vnum && mob->area == area)// Keep from saving mob 0! JIC!!!
		save_mobile_new(fp, mob);

	if (++i == MAX_KEY_HASH)
	    i = 0;
    }

}


/* save all objects in an area */
void save_objects_new(FILE *fp, AREA_DATA *area)
{
    OBJ_INDEX_DATA *obj;
    int i, j, r;

    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
	i = 0; r = MAX_KEY_HASH;
    } else {
	i = area->min_vnum % MAX_KEY_HASH;
	r = area->max_vnum - area->min_vnum + 1;
    }

    for (j = 0; j < r; j++)
    {
	for (obj = obj_index_hash[i];obj;obj = obj->next)
	    if (obj->vnum && obj->area == area)// Keep from saving obj 0! JIC!!!
		save_object_new(fp, obj);

	if (++i == MAX_KEY_HASH)
	    i = 0;
    }
}


/* save all tokens in an area */
void save_tokens(FILE *fp, AREA_DATA *area)
{
    TOKEN_INDEX_DATA *token;
    int i, j, r;

    if ((area->max_vnum - area->min_vnum) >= MAX_KEY_HASH) {
	i = 0; r = MAX_KEY_HASH;
    } else {
	i = area->min_vnum % MAX_KEY_HASH;
	r = area->max_vnum - area->min_vnum + 1;
    }

    for (j = 0; j < r; j++)
    {
	for (token = token_index_hash[i];token;token = token->next)
	    if (token->vnum && token->area == area)// Keep from saving token 0! JIC!!!
		save_token(fp, token);

	if (++i == MAX_KEY_HASH)
	    i = 0;
    }
}


/* save one token */
void save_token(FILE *fp, TOKEN_INDEX_DATA *token)
{
	ITERATOR it;
    PROG_LIST *trigger;
    EXTRA_DESCR_DATA *ed;
    pVARIABLE var;
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
				fprintf(fp, "TokProg %ld %s~ %s~\n", trigger->vnum, trig_name, trig_phrase);
			}
			iterator_stop(&it);
		}
    }

	if(token->index_vars) {
		for(var = token->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}

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
    pVARIABLE var;

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
	else if(IS_SET(room->room2_flags, ROOM_BLUEPRINT))
	{
		fprintf(fp,"Blueprint %1u %1u %1u\n", (unsigned)room->x, (unsigned)room->y, (unsigned)room->z);
	}

    fprintf(fp, "Room_flags %ld\n", room->room_flags);
    fprintf(fp, "Room2_flags %ld\n", room->room2_flags);
    fprintf(fp, "Sector_type %d\n", room->sector_type);

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

			fprintf(fp, "Key %ld To_room %ld Rs_flags %d Keyword %s~\n",
				ex->door.rs_lock.key_vnum, (ex->u1.to_room ? ex->u1.to_room->vnum : -1), ex->rs_flags, kwd);
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
				fprintf(fp, "RoomProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	if(room->index_vars) {
		for(var = room->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}

    for (reset = room->reset_first; reset != NULL; reset = reset->next) {
	fprintf(fp, "#RESET %c\n", reset->command);
        fprintf(fp, "Arguments %ld %ld %ld %ld\n",
	    reset->arg1, reset->arg2, reset->arg3, reset->arg4);
	fprintf(fp, "#-RESET\n");
    }

    fprintf(fp, "#-ROOM\n\n");
}


/* save one mobile */
void save_mobile_new(FILE *fp, MOB_INDEX_DATA *mob)
{
	ITERATOR it;
    PROG_LIST *trigger;
    pVARIABLE var;
    int race, i;

    race = mob->race;

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
    fprintf(fp, "Race %s~\n", race_table[mob->race].name);
    if (mob->act != 0)
	fprintf(fp, "Act %ld\n", mob->act | race_table[race].act);
    if (mob->act2 != 0)
	fprintf(fp, "Act2 %ld\n", mob->act2 | race_table[race].act2);
    if (mob->affected_by != 0)
	fprintf(fp, "Affected_by %ld\n", mob->affected_by | race_table[race].aff);
    if (mob->affected_by2 != 0)
	fprintf(fp, "Affected_by2 %ld\n", mob->affected_by2);

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
    fprintf(fp, "Material %s~\n", mob->material[0] == '\0' ? "Unknown" : mob->material);
    if (mob->corpse_type)
	fprintf(fp, "CorpseType %ld\n", (long int)mob->corpse_type);
    if (mob->corpse)
		fprintf(fp, "CorpseVnum %ld\n", mob->corpse);
    if (mob->zombie)
		fprintf(fp, "CorpseZombie %ld\n", mob->zombie);
	if(mob->comments)
		fprintf(fp, "Comments %s~\n", fix_string(mob->comments));

	if(mob->boss)
		fprintf(fp, "Boss\n");


	if (mob->pQuestor != NULL)
		save_questor_new(fp, mob->pQuestor);

    /* save the shop */
    if (mob->pShop != NULL)
		save_shop_new(fp, mob->pShop);

	if (IS_VALID(mob->pCrew))
	{
		save_ship_crew_index_new(fp, mob->pCrew);
	}

    if(mob->progs) {
		for(i = 0; i < TRIGSLOT_MAX; i++) if(list_size(mob->progs[i]) > 0) {
			iterator_start(&it, mob->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "MobProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	if(mob->index_vars) {
		for(var = mob->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}

    if (mob->spec_fun != NULL)
	fprintf(fp, "SpecFun %s~\n", spec_name(mob->spec_fun));

    fprintf(fp, "#-MOBILE\n");
}


/* save one object */
void save_object_new(FILE *fp, OBJ_INDEX_DATA *obj)
{
	AFFECT_DATA *af;
	EXTRA_DESCR_DATA *ed;
	ITERATOR it;
	PROG_LIST *trigger;
	pVARIABLE var;
	int i;

	/* hack to not save maps in the abyss as they are generated each reboot */
	if (!str_prefix("Maze-Level", obj->area->name) && obj->vnum == obj->area->max_vnum)
		return;

	fprintf(fp, "#OBJECT %ld\n", obj->vnum);
	fprintf(fp, "Name %s~\n", obj->name);
	fprintf(fp, "ShortDesc %s~\n", obj->short_descr);
	fprintf(fp, "LongDesc %s~\n", obj->description);
	fprintf(fp, "Description %s~\n", fix_string(obj->full_description));
	fprintf(fp, "Material %s~\n", obj->material);
	fprintf(fp, "ImpSig %s~\n", obj->imp_sig);
	if(obj->persist)
		fprintf(fp, "Persist\n");
	fprintf(fp, "CreatorSig %s~\n", obj->creator_sig);
	fprintf(fp, "SKeywds %s~\n", obj->skeywds);
	fprintf(fp, "TimesAllowedFixed %d Fragility %d Points %d Update %d Timer %d\n", obj->times_allowed_fixed, obj->fragility, obj->points, obj->update, obj->timer);
	fprintf(fp, "ItemType %s~\n", item_name(obj->item_type));
	fprintf(fp, "ExtraFlags %ld\n", obj->extra_flags);
	fprintf(fp, "Extra2Flags %ld\n", obj->extra2_flags);
	fprintf(fp, "Extra3Flags %ld\n", obj->extra3_flags);
	fprintf(fp, "Extra4Flags %ld\n", obj->extra4_flags);
	fprintf(fp, "WearFlags %ld\n", obj->wear_flags);

	fprintf(fp, "Values");
	for (i = 0; i < 8; i++) fprintf(fp, " %ld", obj->value[i]);
	fprintf(fp, "\n");

	fprintf(fp, "Level %d\n", obj->level);
	fprintf(fp, "Weight %d\n", obj->weight);
	fprintf(fp, "Cost %ld\n", obj->cost);
	fprintf(fp, "Condition %d\n", obj->condition);
	if(obj->comments)
		fprintf(fp, "Comments %s~\n", fix_string(obj->comments));

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

	// Affects
	for (af = obj->affected; af != NULL; af = af->next) {
		fprintf(fp, "#AFFECT %d\n", af->where);

		fprintf(fp, "Location %d\n", af->location);
		fprintf(fp, "Modifier %d\n", af->modifier);

		fprintf(fp, "Level %d\n", af->level);
		fprintf(fp, "Type %d\n", af->type);
		fprintf(fp, "Duration %d\n", af->duration);

		if (af->bitvector != 0)		fprintf(fp, "BitVector %ld\n", af->bitvector);
		if (af->bitvector2 != 0)	fprintf(fp, "BitVector2 %ld\n", af->bitvector2);

		fprintf(fp, "Random %d\n", af->random);
		fprintf(fp, "#-AFFECT\n");
	}

	// Catalysts
	for (af = obj->catalyst; af != NULL; af = af->next) {
		fprintf(fp, "#CATALYST %s\n", flag_string(catalyst_types,af->type));

		if( af->where == TO_CATALYST_ACTIVE )
			fprintf(fp, "Active 1\n");

		if( !IS_NULLSTR(af->custom_name) )
			fprintf(fp, "Name %s\n", af->custom_name);

		fprintf(fp, "Charges %d\n", af->modifier);

		fprintf(fp, "Strength %d\n", af->level);

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
				fprintf(fp, "ObjProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
				iterator_stop(&it);
			}
		}
	}

	if(obj->index_vars) {
		for(var = obj->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);
		}
	}

	if(obj->lock)
	{
		fprintf(fp, "Lock %ld %d %d\n", obj->lock->key_vnum, obj->lock->flags, obj->lock->pick_chance);
	}

	// Save item spells here.
	if (obj->spells != NULL)
		save_spell(fp, obj->spells);

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

	fprintf(fp, "#-OBJECT\n");
}


void save_spell(FILE *fp, SPELL_DATA *spell)
{
    if (spell->next != NULL)
	save_spell(fp, spell->next);

    fprintf(fp, "SpellNew %s~ %d %d\n",
	skill_table[spell->sn].name, spell->level, spell->repop);
}

void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type)
{
	fprintf(fp, "#%sPROG %ld\n", type, (long int)scr->vnum);
	fprintf(fp, "Name %s~\n", scr->name ? scr->name : "");
	fprintf(fp, "Code %s~\n", scr->code ? scr->src : scr->edit_src);
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
    long vnum;
    SCRIPT_DATA *scr;

    // rooms
    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
	if ((scr = get_script_index(vnum, PRG_RPROG)))
	    save_script_new(fp,area,scr,"ROOM");

    // mobiles
    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
	if ((scr = get_script_index(vnum, PRG_MPROG)))
	    save_script_new(fp,area,scr,"MOB");

    // objects
    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
	if ((scr = get_script_index(vnum, PRG_OPROG)))
	    save_script_new(fp,area,scr,"OBJ");

    // tokens
    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
	if ((scr = get_script_index(vnum, PRG_TPROG)))
	    save_script_new(fp,area,scr,"TOKEN");

	// Areas
    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
	if ((scr = get_script_index(vnum, PRG_APROG)))
	    save_script_new(fp,area,scr,"AREA");
}

void save_questor_new(FILE *fp, QUESTOR_DATA *questor)
{
    fprintf(fp, "#QUESTOR\n");
    fprintf(fp, "Scroll %ld\n", questor->scroll);
    fprintf(fp, "Keywords %s~\n", fix_string(questor->keywords));
    fprintf(fp, "ShortDescr %s~\n", fix_string(questor->short_descr));
    fprintf(fp, "LongDescr %s~\n", fix_string(questor->long_descr));
    fprintf(fp, "Header %s~\n", fix_string(questor->header));
    fprintf(fp, "Footer %s~\n", fix_string(questor->footer));
    fprintf(fp, "Prefix %s~\n", fix_string(questor->prefix));
    fprintf(fp, "Suffix %s~\n", fix_string(questor->suffix));
    fprintf(fp, "LineWidth %d\n", questor->line_width);
    fprintf(fp, "#-QUESTOR\n");
}

void save_shop_stock_new(FILE *fp, SHOP_STOCK_DATA *stock)
{
	if(stock->next)
		save_shop_stock_new(fp, stock->next);

	fprintf(fp, "#STOCK\n");

	fprintf(fp, "Level %d\n", stock->level);
	fprintf(fp, "Discount %d\n", stock->discount);

	// Pricing
	fprintf(fp, "Silver %ld\n", stock->silver);
	fprintf(fp, "QuestPnts %ld\n", stock->qp);
	fprintf(fp, "DeityPnts %ld\n", stock->dp);
	fprintf(fp, "Pneuma %ld\n", stock->pneuma);
	fprintf(fp, "Pricing %s~\n", fix_string(stock->custom_price));

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
		fprintf(fp, "Object %ld\n", stock->vnum);
		break;
	case STOCK_PET:
		fprintf(fp, "Pet %ld\n", stock->vnum);
		break;
	case STOCK_MOUNT:
		fprintf(fp, "Mount %ld\n", stock->vnum);
		break;
	case STOCK_GUARD:
		fprintf(fp, "Guard %ld\n", stock->vnum);
		break;
	case STOCK_CREW:
		fprintf(fp, "Crew %ld\n", stock->vnum);
		break;
	case STOCK_SHIP:
		fprintf(fp, "Ship %ld\n", stock->vnum);
		break;
	case STOCK_CUSTOM:
		fprintf(fp, "Keyword %s~\n", fix_string(stock->custom_keyword));
		break;
	}

	fprintf(fp, "Duration %d\n", stock->duration);

	fprintf(fp, "Description %s~\n", fix_string(stock->custom_descr));

	fprintf(fp, "#-STOCK\n");
}

void save_shop_new(FILE *fp, SHOP_DATA *shop)
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

	if( shop->stock )
		save_shop_stock_new(fp, shop->stock);


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

AREA_DATA *read_area_new(FILE *fp)
{
    AREA_DATA *area = NULL;
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *mob;
    OBJ_INDEX_DATA *obj;
    SCRIPT_DATA *rpr, *mpr, *opr, *tpr, *apr;
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

    while (str_cmp((word = fread_word(fp)), "#-AREA"))
    {
	fMatch = FALSE;

	switch (word[0])
	{
	    case '#':
		if (!str_cmp(word, "#ROOM"))
		{
		    room = read_room_new(fp, area, ROOMTYPE_NORMAL);
		    vnum = room->vnum;
		    iHash                   = vnum % MAX_KEY_HASH;
		    room->next        = room_index_hash[iHash];
		    room->area = area;
		    list_appendlink(area->room_list, room);	// Add to the area room list
		    room_index_hash[iHash]  = room;
		    top_room++;
		    top_vnum_room = top_vnum_room < vnum ? vnum : top_vnum_room; /* OLC */
		}
		else if (!str_cmp(word, "#MOBILE"))
		{
		    mob = read_mobile_new(fp, area);
		    vnum = mob->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    mob->next = mob_index_hash[iHash];
		    mob_index_hash[iHash] = mob;
		    mob->area = area;
		    top_mob_index++;
		    top_vnum_mob = top_vnum_mob < vnum ? vnum : top_vnum_mob;
		}
		else if (!str_cmp( word, "#TRADE"	) ) {
			load_area_trade( area, fp );
		  fMatch = TRUE;
    }
		else if (!str_cmp(word, "#OBJECT"))
		{
		    obj = read_object_new(fp, area);
		    vnum = obj->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    obj->next = obj_index_hash[iHash];
		    obj_index_hash[iHash] = obj;
		    obj->area = area;
		    top_obj_index++;
		    top_vnum_obj = top_vnum_obj < vnum ? vnum : top_vnum_obj;
		}
		else if (!str_cmp(word, "#TOKEN"))
		{
		    token = read_token(fp);
		    vnum = token->vnum;
		    iHash = vnum % MAX_KEY_HASH;
		    token->next = token_index_hash[iHash];
		    token_index_hash[iHash] = token;
		    token->area = area;
		}
		else if (!str_cmp(word, "#ROOMPROG"))
		{
		    rpr = read_script_new(fp, area, IFC_R);
		    if(rpr) {
			rpr->next = rprog_list;
			rprog_list = rpr;
		    }
		}
		else if (!str_cmp(word, "#MOBPROG"))
		{
		    mpr = read_script_new(fp, area, IFC_M);
		    if(mpr) {
		    mpr->next = mprog_list;
		    mprog_list = mpr;
		    }
		}
		else if (!str_cmp(word, "#OBJPROG"))
		{
		    opr = read_script_new(fp, area, IFC_O);
		    if(opr) {
			opr->next = oprog_list;
			oprog_list = opr;
		    }
		}
		else if (!str_cmp(word, "#TOKENPROG"))
		{
		    tpr = read_script_new(fp, area, IFC_T);
		    if(tpr) {
			tpr->next = tprog_list;
			tprog_list = tpr;
		    }
		}
		else if (!str_cmp(word, "#AREAPROG"))
		{
		    apr = read_script_new(fp, area, IFC_A);
		    if(apr) {
			apr->next = aprog_list;
			aprog_list = apr;
		    }
		}
		/* VIZZWILDS */
		else if (!str_cmp(word, "#WILDS"))
		{
		    fMatch = TRUE;
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
		    int tindex;
		    char *p;

		    long vnum = fread_number(fp);
		    p = fread_string(fp);

		    tindex = trigger_index(p, PRG_APROG);
		    if(tindex < 0) {
			    sprintf(buf, "read_area_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    PROG_LIST *apr = new_trigger();

			    apr->vnum = vnum;
			    apr->trig_type = tindex;
			    apr->trig_phrase = fread_string(fp);
			    if( tindex == TRIG_SPELLCAST ) {
					char buf[MIL];
					int tsn = skill_lookup(apr->trig_phrase);

					if( tsn < 0 ) {
						sprintf(buf, "read_area_new: invalid spell '%s' for TRIG_SPELLCAST", p);
						bug(buf, 0);
						free_trigger(apr);
						fMatch = TRUE;
						break;
					}

					free_string(apr->trig_phrase);
					sprintf(buf, "%d", tsn);
					apr->trig_phrase = str_dup(buf);
					apr->trig_number = tsn;
					apr->numeric = TRUE;

				} else {
			    	apr->trig_number = atoi(apr->trig_phrase);
					apr->numeric = is_number(apr->trig_phrase);
				}

			    if(!area->progs->progs) area->progs->progs = new_prog_bank();

				list_appendlink(area->progs->progs[trigger_table[tindex].slot], apr);
		    }
		    fMatch = TRUE;
		}


		KEY("AreaWhoFlags",	dummy,	fread_number(fp));
		KEY("AreaWhoFlags2",	dummy,	fread_number(fp));
		KEY("AreaWho",		area->area_who,	fread_number(fp));
		KEY("AirshipLand",	area->airship_land_spot, fread_number(fp));
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

		case 'L':
			if (!str_cmp(word, "Levels")) {
		    	area->min_level = fread_number(fp);
		    	area->max_level = fread_number(fp);
		    	fMatch = TRUE;
			}
		break;

		case 'N':
		KEYS("Notes", area->notes, fread_string(fp));
		break;

	    case 'O':
		KEY("Open",		area->open,		fread_number(fp));
		break;

	    case 'P':
	        KEY("PostOffice",	area->post_office,	fread_number(fp));
		KEY("PlaceType",	area->place_flags,	fread_number(fp));
		break;

	    case 'R':
		if (!str_cmp(word, "Recall")) {
			location_set(&area->recall,0,fread_number(fp),0,0);
			fMatch = TRUE;
		}
		if (!str_cmp(word, "RecallW")) {
			location_set(&area->recall,fread_number(fp),fread_number(fp),fread_number(fp),fread_number(fp));
			fMatch = TRUE;
		}
		KEY("Repop",		area->repop,		fread_number(fp));
		break;

	    case 'S':
		KEY("Security",	area->security,		fread_number(fp));
		break;

/* VIZZWILDS */
            case 'U':
                KEY ("UID", area->uid, fread_number (fp));

	    case 'V':
		if (!str_cmp(word, "VarInt")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_integer (&area->index_vars,name,value,saved);
		}

		if (!str_cmp(word, "VarStr")) {
			char *name;
			char *str;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			str = fread_string(fp);

			variables_setindex_string (&area->index_vars,name,str,FALSE,saved);
		}

		if (!str_cmp(word, "VarRoom")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_room (&area->index_vars,name,value,saved);
		}

		KEY("VersArea", area->version_area, fread_number(fp));
		KEY("VersMobile", area->version_mobile, fread_number(fp));
		KEY("VersObject", area->version_object, fread_number(fp));
		KEY("VersRoom", area->version_room, fread_number(fp));
		KEY("VersToken", area->version_token, fread_number(fp));
		KEY("VersScript", area->version_script, fread_number(fp));
		KEY("VersWilds", area->version_wilds, fread_number(fp));
		if (!str_cmp(word, "VNUMs")) {
		    area->min_vnum = fread_number(fp);
		    area->max_vnum = fread_number(fp);
		    fMatch = TRUE;
		}

		break;
		case 'W':
		KEY("WildsVnum",	area->wilds_uid, fread_number(fp));

	    case 'X':
		KEY("XCoord",		area->x,		fread_number(fp));
		KEY("XLand",		area->land_x,		fread_number(fp));

		break;

	    case 'Y':
		KEY("YCoord",		area->y,		fread_number(fp));
		KEY("YLand",		area->land_y,		fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_area_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

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

	if (!area->wilds_uid && area->airship_land_spot > 0)
	{
		area->wilds_uid = 6;	// The overworld wilderness
	}

    if (area->uid == 0)
    {
        area->uid = gconfig.next_area_uid++;
		gconfig_write();
    }

	(void)dummy;	// Attempt to keep it from triggering -Wunused-but-set-variable

    return area;
}

#if 0
// Read in a virtual map
void read_virtual_rooms(FILE *fp, AREA_DATA *area)
{
    char *word;
    bool fMatch;
    int y;
    char *line;
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "Loading virtual area %s...", area->name);
    log_string(buf);

    while (str_cmp((word = fread_word(fp)), "#-VMAP"))
    {
	fMatch = FALSE;

        switch (UPPER(word[0]))
	{
            case 'E':
                /*
		if (!str_cmp(word, "End"))
		{
                    fMatch = TRUE;
                    sprintf(buf, "Area: '%s' (%s), vMap size = %d x %d",
                        area->name, area->file_name, area->map_size_x, area->map_size_y);
                    log_string(buf);

                    if (!str_cmp(area->name, "Wilderness")) {
			area->file_name = str_dup("bigwilds.are");
                        wilderness_area = area;
		    }
                    else if (!str_cmp(area->name, "Netherworld")) {
                        area->file_name = str_dup("nether.are");
			netherworld_area = area;
		    }
                    else if (!str_cmp(area->name, "Eden"))
                        eden_area = area;
                    else {
                        sprintf(buf, "Error in load_virtual_rooms: Unrecognised vArea, %s.",
                            area->name);
                        log_string(buf);
                    }
                    return;
		    }*/
                break;
	    case 'M':
	        if (!str_cmp(word, "Map"))
		{
		    fMatch = TRUE;

		    area->map = alloc_perm(area->map_size_x * area->map_size_y);
		    for (y = 0; y < area->map_size_y; y++) {
			line = fread_string_eol(fp);
			strcat(area->map, line);
		    }
		}

		break;

	    case 'X':
		KEY("XSize",	area->map_size_x,	fread_number(fp));
		break;

	    case 'Y':
		KEY("YSize",	area->map_size_y,	fread_number(fp));
		break;
        }

	if (!fMatch) {
	    sprintf(buf, "read_virtual_rooms: no match for word %s", word);
	    bug(buf, 0);
	}
    }
}


void make_virtual_area(AREA_DATA *area)
{
    long current_vnum = 0, base_vnum = 0, vnum = 0, x, y;
    char j[2];
    char buf[MSL];

    if (!str_cmp(area->name, "Wilderness")) //Big Wilds
    {
	current_vnum = 5000000 + WILDERNESS_VNUM_OFFSET ;
	base_vnum = 5000000;
	area->startx = 0;
	area->starty = 0;
	area->endx = 1212;
	area->endy = 400;
    }
    else
    if (!str_cmp(area->name, "Eden")) {

	current_vnum = 400000 + WILDERNESS_VNUM_OFFSET;
	base_vnum = 400000;
	area_last->startx = 0;
	area_last->starty = 0;
	area_last->endx = 117;
	area_last->endy = 61;
    }
    else
    if (!str_cmp(area->name, "Netherworld")) {
	current_vnum = 2000000 + WILDERNESS_VNUM_OFFSET; //Netherworld
	base_vnum = 2000000;
	area->startx = 0;
	area->starty = 0;
	area->endx = 411;
	area->endy = 246;
    }

    for (y = 0; y < area->map_size_y; y++)
    {
	for (x = 0; x < area->map_size_x; x++)
	{
	    sprintf(j, "%c", area->map[y*area->map_size_x+x]);

	    if (!str_cmp(area->name, "Wilderness")) {
// (7-20-06) Replaced the old mappings with Nib's - Areo
		if (!strcmp(j, " "))
			vnum = 15 + base_vnum;
		if (!strcmp(j, "!"))
			vnum = 36 + base_vnum;
		if (!strcmp(j, "#"))
			vnum = 73 + base_vnum;
		if (!strcmp(j, "$"))
			vnum = 40 + base_vnum;
		if (!strcmp(j, "&"))
			vnum = 58 + base_vnum;
		if (!strcmp(j, "'"))
			vnum = 63 + base_vnum;
		if (!strcmp(j, "("))
			vnum = 24 + base_vnum;
		if (!strcmp(j, ")"))
			vnum = 55 + base_vnum;
		if (!strcmp(j, "*"))
			vnum = 1 + base_vnum;
		if (!strcmp(j, "+"))
			vnum = 54 + base_vnum;
		if (!strcmp(j, ","))
			vnum = 8 + base_vnum;
		if (!strcmp(j, "."))
			vnum = 19 + base_vnum;
		if (!strcmp(j, "/"))
			vnum = 12 + base_vnum;
		if (!strcmp(j, "0"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "1"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "2"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "3"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "4"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "5"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "6"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "7"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "8"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, "9"))
			vnum = 71 + base_vnum;
		if (!strcmp(j, ":"))
			vnum = 21 + base_vnum;
		if (!strcmp(j, ">"))
			vnum = 32 + base_vnum;
		if (!strcmp(j, "@"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "A"))
			vnum = 4 + base_vnum;
		if (!strcmp(j, "B"))
			vnum = 17 + base_vnum;
		if (!strcmp(j, "C"))
			vnum = 57 + base_vnum;
		if (!strcmp(j, "D"))
			vnum = 16 + base_vnum;
		if (!strcmp(j, "E"))
			vnum = 13 + base_vnum;
		if (!strcmp(j, "F"))
			vnum = 2 + base_vnum;
		if (!strcmp(j, "G"))
			vnum = 33 + base_vnum;
		if (!strcmp(j, "H"))
			vnum = 6 + base_vnum;
		if (!strcmp(j, "I"))
			vnum = 60 + base_vnum;
		if (!strcmp(j, "J"))
			vnum = 31 + base_vnum;
		if (!strcmp(j, "K"))
			vnum = 27 + base_vnum;
		if (!strcmp(j, "L"))
			vnum = 14 + base_vnum;
		if (!strcmp(j, "M"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "N"))
			vnum = 67 + base_vnum;
		if (!strcmp(j, "O"))
			vnum = 45 + base_vnum;
		if (!strcmp(j, "P"))
			vnum = 10 + base_vnum;
		if (!strcmp(j, "Q"))
			vnum = 20 + base_vnum;
		if (!strcmp(j, "R"))
			vnum = 38 + base_vnum;
		if (!strcmp(j, "S"))
			vnum = 5 + base_vnum;
		if (!strcmp(j, "T"))
			vnum = 25 + base_vnum;
		if (!strcmp(j, "V"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "W"))
			vnum = 34 + base_vnum;
		if (!strcmp(j, "X"))
			vnum = 11 + base_vnum;
		if (!strcmp(j, "Y"))
			vnum = 23 + base_vnum;
		if (!strcmp(j, "Z"))
			vnum = 29 + base_vnum;
		if (!strcmp(j, "["))
			vnum = 52 + base_vnum;
		if (!strcmp(j, "^"))
			vnum = 9 + base_vnum;
		if (!strcmp(j, "_"))
			vnum = 10 + base_vnum;
		if (!strcmp(j, "a"))
			vnum = 49 + base_vnum;
		if (!strcmp(j, "b"))
			vnum = 47 + base_vnum;
		if (!strcmp(j, "c"))
			vnum = 72 + base_vnum;
		if (!strcmp(j, "d"))
			vnum = 5 + base_vnum;
		if (!strcmp(j, "e"))
			vnum = 51 + base_vnum;
		if (!strcmp(j, "f"))
			vnum = 70 + base_vnum;
		if (!strcmp(j, "g"))
			vnum = 48 + base_vnum;
		if (!strcmp(j, "h"))
			vnum = 66 + base_vnum;
		if (!strcmp(j, "i"))
			vnum = 56 + base_vnum;
		if (!strcmp(j, "j"))
			vnum = 46 + base_vnum;
		if (!strcmp(j, "k"))
			vnum = 69 + base_vnum;
		if (!strcmp(j, "l"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "m"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "n"))
			vnum = 37 + base_vnum;
		if (!strcmp(j, "o"))
			vnum = 43 + base_vnum;
		if (!strcmp(j, "p"))
			vnum = 3 + base_vnum;
		if (!strcmp(j, "q"))
			vnum = 68 + base_vnum;
		if (!strcmp(j, "r"))
			vnum = 59 + base_vnum;
		if (!strcmp(j, "s"))
			vnum = 50 + base_vnum;
		if (!strcmp(j, "t"))
			vnum = 61 + base_vnum;
		if (!strcmp(j, "u"))
			vnum = 0 + base_vnum;
		if (!strcmp(j, "v"))
			vnum = 62 + base_vnum;
		if (!strcmp(j, "w"))
			vnum = 39 + base_vnum;
		if (!strcmp(j, "x"))
			vnum = 44 + base_vnum;
		if (!strcmp(j, "y"))
			vnum = 65 + base_vnum;
		if (!strcmp(j, "z"))
			vnum = 64 + base_vnum;
		if (!strcmp(j, "{"))
			vnum = 18 + base_vnum;
		if (!strcmp(j, "|"))
			vnum = 35 + base_vnum;
		if (!strcmp (j, "`"))
		    vnum = 53 + base_vnum;
	    }
	    else
	    {
		if (!str_cmp(j, "&"))
		    vnum = base_vnum;
		if (!str_cmp(j, "*"))
		    vnum = 1 + base_vnum;
		if (!str_cmp(j, "F"))
		    vnum = 2 + base_vnum;
		if (!str_cmp(j, "p"))
		    vnum = 3 + base_vnum;
		if (!str_cmp(j, "="))
		    vnum = 3 + base_vnum;
		if (!str_cmp(j, "A"))
		    vnum = 4 + base_vnum;
		if (!str_cmp(j, "S"))
		    vnum = 5 + base_vnum;
		if (!str_cmp(j, "H"))
		    vnum = 6 + base_vnum;
		if (!str_cmp(j, "t"))
		    vnum = 7 + base_vnum;
		if (!str_cmp(j, ","))
		    vnum = 8 + base_vnum;
		if (!str_cmp(j, "_"))
		    vnum = 9 + base_vnum;

		if (!str_cmp(j, "^"))
		    vnum = 10 + base_vnum;
    if (!str_cmp(j, "S"))
        vnum = 11 + base_vnum;

		if (!str_cmp(j, "X")) {
			if (x == 68 && y == 164) {
					vnum = 1 + base_vnum;
      }
      else {
					vnum = 11 + base_vnum;
      }


		if (!str_cmp(j, "/"))
		    vnum = 12 + base_vnum;

		if (!str_cmp(j, "E"))
		    vnum = 13 + base_vnum;
		if (!str_cmp(j, "L"))
		    vnum = 14 + base_vnum;
		if (!str_cmp(j, "C"))
		    vnum = 14 + base_vnum;
		if (!str_cmp(j, " "))
		    vnum = 15 + base_vnum;
		if (!str_cmp(j, "."))
		    vnum = 19 + base_vnum;
		if (!str_cmp(j, "D"))
		    vnum = 16 + base_vnum;
		if (!str_cmp(j, "B"))
		    vnum = 17 + base_vnum;

		if (!str_cmp(j, "{"))
		    vnum = 18 + base_vnum;
		if (!str_cmp(j, "+"))
		    vnum = 19 + base_vnum;
		if (!str_cmp(j, "I"))
		    vnum = 20 + base_vnum;
	    }
	}


	    if (vnum == 0) {
		sprintf(buf, "Vnum is about to be 0 for map char %c", j[0]);
		bug(buf, 0);
	    }

	    create_virtual_room_new(area, current_vnum,
		    x + area->startx,
		    y + area->starty,
		    vnum,
		    base_vnum,
		    area->map_size_x,
		    area->map_size_y);
	    current_vnum++;
	}
    }
}


// Set up one virtual room from its coordinates and parent.
void create_virtual_room_new(AREA_DATA *area, long vnum, int x, int y,
	long parent, long base_vnum, int sizex, int sizey)
{
    ROOM_INDEX_DATA *pRoomIndex,pParent;
    int door;
    int iHash;
    bool fLink;

    fBootDb = FALSE;
    if (get_room_index(vnum) != NULL)
    {
        bug("Load_rooms: vnum %ld duplicated.", vnum);
        exit(1);
    }

    fBootDb = TRUE;
    pParent			= get_room_index(parent);
    pRoomIndex			= alloc_perm(sizeof(*pRoomIndex));
    pRoomIndex->owner		= NULL;
    pRoomIndex->people		= NULL;
    pRoomIndex->contents	= NULL;
    pRoomIndex->extra_descr	= NULL;
    pRoomIndex->area		= area;
    pRoomIndex->vnum		= vnum;

    if (room_name_virtual == NULL)
	room_name_virtual = str_dup("VIRTUAL ROOM");

    pRoomIndex->name		= str_dup(pParent->name);
    pRoomIndex->description	= NULL;
    pRoomIndex->room_flags	= pParent->room_flags;
    pRoomIndex->sector_type	= pParent->sector_type;
    pRoomIndex->light		= 0;//get_room_index(parent)->light;
    pRoomIndex->x = x;
    pRoomIndex->y = y;

    if (prog_data_virtual == NULL)
	prog_data_virtual = new_prog_data();

    pRoomIndex->progs		= prog_data_virtual;

    for (door = 0; door < MAX_DIR; door++)
        pRoomIndex->exit[door] = NULL;

    fLink = FALSE;

    /* defaults */
    pRoomIndex->heal_rate = 100;
    pRoomIndex->mana_rate = 100;

    iHash		= vnum % MAX_KEY_HASH;
    pRoomIndex->next	= room_index_hash[iHash];
    room_index_hash[iHash] = pRoomIndex;
    top_room++;
    top_vnum_room = top_vnum_room < vnum ? vnum : top_vnum_room;
    assign_area_vnum_new(area, vnum);

    top_vroom++;
}
#endif

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
    int vnum;

    room = new_room_index();
/* VIZZWILDS */
    if (recordtype != ROOMTYPE_TERRAIN)
    {
        room->vnum = fread_number(fp);
    }
    room->persist = FALSE;

    while (str_cmp((word = fread_word(fp)), "#-ROOM")
           && str_cmp(word, "#-TERRAIN")) {
	fMatch = FALSE;

	switch(word[0]) {
	    case '#':
		if (!str_cmp(word, "#EXTRA_DESCR")) {
		    ed = read_extra_descr_new(fp);
		    ed->next = room->extra_descr;
		    room->extra_descr = ed;
		    fMatch = TRUE;
		}
		else if (!str_cmp(word, "#CONDITIONAL_DESCR")) {
		    cd = read_conditional_descr_new(fp);
		    cd->next = room->conditional_descr;
		    room->conditional_descr = cd;
		    fMatch = TRUE;
		}
		else if (!str_cmp(word, "#X")) { // finish here
		    ex = read_exit_new(fp);
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
		    fMatch = TRUE;
		}
		else if (!str_cmp(word, "#RESET")) {
	   	    reset = read_reset_new(fp);
		    new_reset(room, reset);
		    fMatch = TRUE;
		}

		break;
		case 'B':
			if (!str_cmp(word, "Blueprint")) {
				room->w = 0;
				room->x = fread_number(fp);
				room->y = fread_number(fp);
				room->z = fread_number(fp);
				fMatch = TRUE;
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
				room->persist = TRUE;

				fMatch = TRUE;
			}
			break;

	    case 'R':
		KEY("Room_flags", 	room->room_flags, 	fread_number(fp));
		KEY("Room2_flags", 	room->room2_flags, 	fread_number(fp));

		if (!str_cmp(word, "RoomProg")) {
		    int tindex;
		    char *p;

		    vnum = fread_number(fp);
		    p = fread_string(fp);

		    tindex = trigger_index(p, PRG_RPROG);
		    if(tindex < 0) {
			    sprintf(buf, "read_room_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    rpr = new_trigger();

			    rpr->vnum = vnum;
			    rpr->trig_type = tindex;
			    rpr->trig_phrase = fread_string(fp);
			    if( tindex == TRIG_SPELLCAST ) {
					char buf[MIL];
					int tsn = skill_lookup(rpr->trig_phrase);

					if( tsn < 0 ) {
						sprintf(buf, "read_room_new: invalid spell '%s' for TRIG_SPELLCAST", p);
						bug(buf, 0);
						free_trigger(rpr);
						fMatch = TRUE;
						break;
					}

					free_string(rpr->trig_phrase);
					sprintf(buf, "%d", tsn);
					rpr->trig_phrase = str_dup(buf);
					rpr->trig_number = tsn;
					rpr->numeric = TRUE;

				} else {
			    	rpr->trig_number = atoi(rpr->trig_phrase);
					rpr->numeric = is_number(rpr->trig_phrase);
				}
			    //SET_BIT(room->rprog_flags, rpr->trig_type);

			    if(!room->progs->progs) room->progs->progs = new_prog_bank();

				list_appendlink(room->progs->progs[trigger_table[tindex].slot], rpr);
		    }
		    fMatch = TRUE;
		}

		break;

	    case 'S':
	        KEY("Sector_type",	room->sector_type,	fread_number(fp));
		break;

	    case 'V':
		if (!str_cmp(word, "VarInt")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_integer (&room->index_vars,name,value,saved);
		}

		if (!str_cmp(word, "VarStr")) {
			char *name;
			char *str;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			str = fread_string(fp);

			variables_setindex_string (&room->index_vars,name,str,FALSE,saved);
		}

		if (!str_cmp(word, "VarRoom")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_room (&room->index_vars,name,value,saved);
		}
		break;

	    case 'W':
		if (!str_cmp(word, "Wilds")) {
			room->w = fread_number(fp);
			room->x = fread_number(fp);
			room->y = fread_number(fp);
			room->z = fread_number(fp);
		    fMatch = TRUE;
		}
	    	break;

	}

	if (!fMatch) {
	    sprintf(buf, "read_room_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

	if (recordtype != ROOMTYPE_TERRAIN)
		variable_copylist(&room->index_vars,&room->progs->vars,FALSE);

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
    QUESTOR_DATA *questor;
    PROG_LIST *mpr;
    char *word;
    int vnum;

    mob = new_mob_index();
    mob->vnum = fread_number(fp);
    mob->persist = FALSE;

    while (str_cmp((word = fread_word(fp)), "#-MOBILE")) {
	fMatch = FALSE;

	switch(word[0]) {
	    case '#':
	        if( !str_cmp(word, "#CREW") )
	        {
				if( mob->pCrew )
					free_ship_crew_index(mob->pCrew);

				mob->pCrew = read_ship_crew_index_new(fp);
				fMatch = TRUE;
				break;
			}

	        if (!str_cmp(word, "#SHOP")) {
			    fMatch = TRUE;
			    shop = read_shop_new(fp);
			    mob->pShop = shop;
			    break;
			}
	        if (!str_cmp(word, "#QUESTOR")) {
			    fMatch = TRUE;
			    questor = read_questor_new(fp);
			    mob->pQuestor = questor;
			    break;
			}

			break;

		case 'A':
	        KEY("Act",	mob->act,	fread_number(fp));
	        KEY("Act2",	mob->act2,	fread_number(fp));
                KEY("Affected_by", mob->affected_by,	fread_number(fp));
		KEY("Affected_by2",  mob->affected_by2,	fread_number(fp));
		KEY("Alignment",  mob->alignment,	fread_number(fp));
		KEY("AttackType", mob->dam_type,	fread_number(fp));
		KEY("Attacks",	mob->attacks,	fread_number(fp));
		break;

		case 'B':
			KEY("Boss", mob->boss, TRUE);
			break;

	    case 'C':
	        KEYS("CreatorSig", mob->creator_sig,	fread_string(fp));
	        KEY("CorpseType", mob->corpse_type,	fread_number(fp));
	        KEY("CorpseVnum", mob->corpse,	fread_number(fp));
	        KEY("CorpseZombie", mob->zombie,	fread_number(fp));
			KEY("Comments", mob->comments, fread_string(fp));
	        break;

	    case 'D':
	        KEYS("Description", mob->description, fread_string(fp));
		KEY("DefaultPos",	mob->default_pos,	fread_number(fp));

	        if (!str_cmp(word, "Damage")) {
		    mob->damage.number = fread_number(fp);
		    mob->damage.size = fread_number(fp);
		    mob->damage.bonus = fread_number(fp);
		    fMatch = TRUE;
		}

		break;

	    case 'F':
		KEY("Form",		mob->form,	fread_number(fp));
		break;

	    case 'H':
	        KEY("Hitroll",	mob->hitroll,	fread_number(fp));

		if (!str_cmp(word, "Hit")) {
		    mob->hit.number = fread_number(fp);
		    mob->hit.size = fread_number(fp);
		    mob->hit.bonus = fread_number(fp);
		    fMatch = TRUE;
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
	        KEYS("Material",	mob->material,	fread_string(fp));
		KEY("Movement",	mob->move,	fread_number(fp));

		if (!str_cmp(word, "Mana")) {
		    mob->mana.number = fread_number(fp);
		    mob->mana.size = fread_number(fp);
		    mob->mana.bonus = fread_number(fp);
		    fMatch = TRUE;
		}

		if (!str_cmp(word, "MobProg")) {
		    int tindex;
		    char *p;

		    vnum = fread_number(fp);
		    p = fread_string(fp);

		    tindex = trigger_index(p, PRG_MPROG);
		    if(tindex < 0) {
			    sprintf(buf, "read_mob_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    mpr = new_trigger();

			    mpr->vnum = vnum;
			    mpr->trig_type = tindex;
			    mpr->trig_phrase = fread_string(fp);
			    if( tindex == TRIG_SPELLCAST ) {
					char buf[MIL];
					int tsn = skill_lookup(mpr->trig_phrase);

					if( tsn < 0 ) {
						sprintf(buf, "read_mob_new: invalid spell '%s' for TRIG_SPELLCAST", p);
						bug(buf, 0);
						free_trigger(mpr);
						fMatch = TRUE;
						break;
					}

					free_string(mpr->trig_phrase);
					sprintf(buf, "%d", tsn);
					mpr->trig_phrase = str_dup(buf);
					mpr->trig_number = tsn;
					mpr->numeric = TRUE;

				} else {
			    	mpr->trig_number = atoi(mpr->trig_phrase);
					mpr->numeric = is_number(mpr->trig_phrase);
				}
			    //SET_BIT(room->rprog_flags, rpr->trig_type);

			    if(!mob->progs) mob->progs = new_prog_bank();

				list_appendlink(mob->progs[trigger_table[tindex].slot], mpr);
		    }
		    fMatch = TRUE;
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
	        KEY("Persist",	mob->persist, TRUE);
		break;

	    case 'R':
		KEY("ResFlags", 	mob->res_flags, fread_number(fp));

		if (!str_cmp(word, "Race")) {
		    char *race_string = fread_string(fp);

		    mob->race = race_lookup(race_string);

		    free_string(race_string);

		    fMatch = TRUE;
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
		    fMatch = TRUE;
		}

		break;

            case 'T':
            case 'V':
		if (!str_cmp(word, "VarInt")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_integer (&mob->index_vars,name,value,saved);
		}

		if (!str_cmp(word, "VarStr")) {
			char *name;
			char *str;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			str = fread_string(fp);

			variables_setindex_string (&mob->index_vars,name,str,FALSE,saved);
		}

		if (!str_cmp(word, "VarRoom")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_room (&mob->index_vars,name,value,saved);
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

	// Remove this bit, JIC
	REMOVE_BIT(mob->act, ACT_ANIMATED);

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


/* read one object into an area */
OBJ_INDEX_DATA *read_object_new(FILE *fp, AREA_DATA *area)
{
    OBJ_INDEX_DATA *obj = NULL;
    SPELL_DATA *spell;
    PROG_LIST *opr;
    AFFECT_DATA *af;
    EXTRA_DESCR_DATA *ed;
    char *word;
    int vnum;

    obj = new_obj_index();
    obj->vnum = fread_number(fp);
    obj->persist = FALSE;

    while (str_cmp((word = fread_word(fp)), "#-OBJECT")) {
	fMatch = FALSE;

	switch(word[0]) {
	    case '#':
	        if (!str_cmp(word, "#AFFECT")) {
		    af = read_obj_affect_new(fp);
		    af->next = obj->affected;
		    obj->affected = af;
	        } else if (!str_cmp(word, "#CATALYST")) {
		    af = read_obj_catalyst_new(fp);
		    af->next = obj->catalyst;
		    obj->catalyst = af;
		} else if (!str_cmp(word, "#EXTRA_DESCR")) {
		    ed = read_extra_descr_new(fp);
		    ed->next = obj->extra_descr;
		    obj->extra_descr = ed;
		}

		break;

            case 'A':
		break;

	    case 'C':
	        KEYS("CreatorSig", obj->creator_sig,	fread_string(fp));
	        KEY("Cost",	obj->cost,	fread_number(fp));
		KEY("Condition",	obj->condition,	fread_number(fp));
		KEY("Comments", obj->comments, fread_string(fp));
	        break;

	    case 'D':
	        KEYS("Description", obj->full_description, fread_string(fp));
		break;

	    case 'E':
	        KEY("ExtraFlags",	obj->extra_flags,	fread_number(fp));
	        KEY("Extra2Flags",	obj->extra2_flags,	fread_number(fp));
	        KEY("Extra3Flags",	obj->extra3_flags,	fread_number(fp));
	        KEY("Extra4Flags",	obj->extra4_flags,	fread_number(fp));

	    case 'F':
	        KEY("Fragility", obj->fragility,	fread_number(fp));
		break;

	    case 'I':
	        KEYS("ImpSig",		obj->imp_sig,	fread_string(fp));

	        if (!str_cmp(word, "ItemType")) {
		    char *item_type = fread_string(fp);

		    obj->item_type = item_lookup(item_type);

		    free_string(item_type);
		    fMatch = TRUE;
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

				obj->lock->key_vnum = fread_number(fp);
				obj->lock->flags = fread_number(fp);
				obj->lock->pick_chance = fread_number(fp);
				fMatch = TRUE;
				break;
			}

		KEYS("LongDesc",	obj->description,	fread_string(fp));
		break;

	    case 'M':
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

				fMatch = TRUE;
				break;
			}

	        KEYS("Material",	obj->material,	fread_string(fp));
		break;

	    case 'N':
	        KEYS("Name",	obj->name,	fread_string(fp));
		break;

	    case 'O':
		if (!str_cmp(word, "ObjProg")) {
		    int tindex;
		    char *p;

		    vnum = fread_number(fp);
		    p = fread_string(fp);

		    tindex = trigger_index(p, PRG_OPROG);
		    if(tindex < 0) {
			    sprintf(buf, "read_obj_new: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    opr = new_trigger();

			    opr->vnum = vnum;
			    opr->trig_type = tindex;
			    opr->trig_phrase = fread_string(fp);
			    if( tindex == TRIG_SPELLCAST ) {
					char buf[MIL];
					int tsn = skill_lookup(opr->trig_phrase);

					if( tsn < 0 ) {
						sprintf(buf, "read_obj_new: invalid spell '%s' for TRIG_SPELLCAST", p);
						bug(buf, 0);
						free_trigger(opr);
						fMatch = TRUE;
						break;
					}

					free_string(opr->trig_phrase);
					sprintf(buf, "%d", tsn);
					opr->trig_phrase = str_dup(buf);
					opr->trig_number = tsn;
					opr->numeric = TRUE;

				} else {
			    	opr->trig_number = atoi(opr->trig_phrase);
					opr->numeric = is_number(opr->trig_phrase);
				}
			    opr->trig_number = atoi(opr->trig_phrase);
				opr->numeric = is_number(opr->trig_phrase);
			    //SET_BIT(room->rprog_flags, rpr->trig_type);

			    if(!obj->progs) obj->progs = new_prog_bank();

				list_appendlink(obj->progs[trigger_table[tindex].slot], opr);
		    }
		    fMatch = TRUE;
		}
		break;

	    case 'P':
			if(!str_cmp(word, "Persist")) {
				obj->persist = TRUE;
				fMatch = TRUE;
			}
	        KEY("Points",	obj->points, fread_number(fp));
		break;

	    case 'R':

		break;

	    case 'S':
	        KEYS("ShortDesc",	obj->short_descr,	fread_string(fp));
	        KEYS("Skeywds",		obj->skeywds,			fread_string(fp));

		if (!str_cmp(word, "SpellNew"))
		{
		    int sn;

		    fMatch = TRUE;
		    if ((sn = skill_lookup(fread_string(fp))) > -1)
		    {
			    spell = new_spell();
			    spell->sn = sn;
			    spell->level = fread_number(fp);
			    spell->repop = fread_number(fp);

			    // Syn - clean up bad spells on objs.
			    if (!str_cmp(skill_table[sn].name, "reserved")
			    ||  !str_cmp(skill_table[sn].name, "none")) {
				sprintf(buf, "Obj %s(%ld) had spell none or reserved.",
					obj->short_descr, obj->vnum);
				log_string(buf);
				free_spell(spell);
			    }
			    else
			    {
				spell->next = obj->spells;
				obj->spells = spell;
			    }
		    }
		    else
		    {
			sprintf(buf, "Bad spell name for %s (%ld).", obj->short_descr, obj->vnum);
			bug(buf,0);
		    }
		}

                if (!str_cmp(word, "SpellLevel"))
		{
		    fMatch = TRUE;
		    spell = new_spell();
		    spell->level = fread_number(fp);
		    spell->repop = 100;
		    spell->next = obj->spells;
		    obj->spells = spell;
		}

		if (!str_cmp(word, "Spell1"))
		{
		    fMatch = TRUE;
		    obj->spells->sn = skill_lookup(fread_string(fp));
		}

		if (!str_cmp(word, "Spell2"))
		{
		    fMatch = TRUE;
		    obj->spells->sn = skill_lookup(fread_string(fp));
		}

		if (!str_cmp(word, "Spell3"))
		{
		    fMatch = TRUE;
		    obj->spells->sn = skill_lookup(fread_string(fp));

		}

		if (!str_cmp(word, "Spell4"))
		{
		    fMatch = TRUE;
		    obj->spells->sn = skill_lookup(fread_string(fp));
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

		    for (i = 0; i < 8; i++)
			obj->value[i] = fread_number(fp);

		    fMatch = TRUE;
		}
		if (!str_cmp(word, "VarInt")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_integer (&obj->index_vars,name,value,saved);
		}

		if (!str_cmp(word, "VarStr")) {
			char *name;
			char *str;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			str = fread_string(fp);

			variables_setindex_string (&obj->index_vars,name,str,FALSE,saved);
		}

		if (!str_cmp(word, "VarRoom")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_room (&obj->index_vars,name,value,saved);
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
     * Syn - Fix object indexes here.
     */

    if (IS_SET(obj->extra_flags, ITEM_PERMANENT) && !is_relic(obj)) {
	REMOVE_BIT(obj->extra_flags, ITEM_PERMANENT);
	sprintf(buf, "read_object_new: removed permanent flag for item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }

    if (obj->item_type == ITEM_WEAPON && !has_imp_sig(NULL, obj)
    &&  (obj->value[0] == WEAPON_ARROW || obj->value[0] == WEAPON_BOLT))
	set_weapon_dice(obj);
/*
    if (IS_SET(obj->extra_flags, ITEM_ANTI_GOOD)) {
	sprintf(buf, "read_object_new: anti-good flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }

    if (IS_SET(obj->extra_flags, ITEM_ANTI_EVIL)) {
	sprintf(buf, "read_object_new: anti-evil flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }

    if (IS_SET(obj->extra_flags, ITEM_ANTI_NEUTRAL)) {
	sprintf(buf, "read_object_new: anti-neutral flag on item %s(%ld)",
	    obj->short_descr, obj->vnum);
	log_string(buf);
    }
*/
    if (obj->item_type == ITEM_SCROLL ||
    	obj->item_type == ITEM_POTION ||
    	obj->item_type == ITEM_PILL ||
    	obj->item_type == ITEM_STAFF ||
    	obj->item_type == ITEM_WAND)
    {
		fix_magic_object_index(obj);
	}

	if (area)
	{
		if( area->version_object < VERSION_OBJECT_004 )
		{
			if( !obj->lock )
			{
				switch(obj->item_type)
				{
				case ITEM_CONTAINER:
				case ITEM_BOOK:
					// Value[1] == CONT flags
					// Value[2] == Key

					if( (obj->value[2] > 0) || IS_SET(obj->value[1], VO_004_CONT_LOCKED) )
					{
						obj->lock = new_lock_state();
						obj->lock->key_vnum = obj->value[2];
						obj->lock->flags = 0;
						obj->lock->pick_chance = 100;

						if( IS_SET(obj->value[1], VO_004_CONT_LOCKED) )
						{
							SET_BIT(obj->lock->flags, LOCK_LOCKED);
						}

						if( IS_SET(obj->value[1], VO_004_CONT_PICKPROOF) )
						{
							obj->lock->pick_chance = 0;
						}

						if( IS_SET(obj->value[1], VO_004_CONT_SNAPKEY) )
						{
							SET_BIT(obj->lock->flags, LOCK_SNAPKEY);
						}

						// Remove the old data
						REMOVE_BIT(obj->value[1], (VO_004_CONT_PICKPROOF|VO_004_CONT_LOCKED|VO_004_CONT_SNAPKEY));
						obj->value[2] = 0;
					}
					break;

				case ITEM_PORTAL:
					// Value[1] == EXIT flags
					// Value[4] == Key

					if( (obj->value[4] > 0) || IS_SET(obj->value[1], VO_004_EX_LOCKED) )
					{
						obj->lock = new_lock_state();
						obj->lock->key_vnum = obj->value[4];
						obj->lock->flags = 0;
						obj->lock->pick_chance = 100;

						if( IS_SET(obj->value[1], VO_004_EX_LOCKED) )
						{
							SET_BIT(obj->lock->flags, LOCK_LOCKED);
						}

						if( IS_SET(obj->value[1], VO_004_EX_PICKPROOF) )
						{
							obj->lock->pick_chance = 0;
						}
						else if( IS_SET(obj->value[1], VO_004_EX_INFURIATING) )
						{
							obj->lock->pick_chance = 10;
						}
						else if( IS_SET(obj->value[1], VO_004_EX_HARD) )
						{
							obj->lock->pick_chance = 40;
						}
						else if( IS_SET(obj->value[1], VO_004_EX_EASY) )
						{
							obj->lock->pick_chance = 80;
						}


						REMOVE_BIT(obj->value[1], (VO_004_EX_LOCKED|VO_004_EX_PICKPROOF|VO_004_EX_INFURIATING|VO_004_EX_HARD|VO_004_EX_EASY));
						obj->value[4] = 0;
					}
					break;


//				case ITEM_WEAPON_CONTAINER:
//				case ITEM_DRINKCONTAINER:
//					break;

				}
			}
		}

	}

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
		fMatch = FALSE;

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
			    fMatch = TRUE;
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
		fMatch = FALSE;
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
				fMatch = TRUE;
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
	fMatch = FALSE;
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
EXIT_DATA *read_exit_new(FILE *fp)
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
	fMatch = FALSE;

	switch (word[0]) {
	    case 'D':
		KEYS("Description",	ex->short_desc, fread_string(fp));
		break;

	    case 'K':
	        KEY("Key",		ex->door.rs_lock.key_vnum,	fread_number(fp));

		if (!str_cmp(word, "Keyword")) {
		    int i;
		    char letter;

		    fMatch = TRUE;

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
		    ex->u1.vnum = fread_number(fp);
		    fMatch = TRUE;
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
RESET_DATA *read_reset_new(FILE *fp)
{
    RESET_DATA *reset;
    char *word;

    reset = new_reset_data();
    reset->command = fread_letter(fp);

    while (str_cmp((word = fread_word(fp)), "#-RESET")) {
	fMatch = FALSE;
	switch (word[0]) {
	    case 'A':
	    if (!str_cmp(word, "Arguments")) {
                reset->arg1 = fread_number(fp);
                reset->arg2 = fread_number(fp);
                reset->arg3 = fread_number(fp);
                reset->arg4 = fread_number(fp);
		fMatch = TRUE;
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
	fMatch = FALSE;
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

            case 'T':
	        KEY("Type",		af->type,	fread_number(fp));
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
    af->type = flag_value(catalyst_types,fread_string_eol(fp));
    af->where = TO_CATALYST_DORMANT;

    while (str_cmp((word = fread_word(fp)), "#-CATALYST")) {
	fMatch = FALSE;
	switch (word[0]) {
		case 'A':
			if (!str_cmp(word, "Active")) {
				fread_to_eol(fp);
				fMatch = TRUE;
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

	    case 'S':
	        KEY("Strength",		af->level,	fread_number(fp));
		break;
	}

	if (!fMatch) {
	    sprintf(buf, "read_obj_catalyst_new: no match for word %s", word);
	    bug(buf, 0);
	}
    }

    return af;
}


QUESTOR_DATA *read_questor_new(FILE *fp)
{
    QUESTOR_DATA *questor;
    char *word;

    questor = new_questor_data();

    while (str_cmp((word = fread_word(fp)), "#-QUESTOR"))
    {
	fMatch = FALSE;
	switch (word[0]) {
		case 'F':
	        KEYS("Footer",		questor->footer,	fread_string(fp));
	        break;

	    case 'H':
	        KEYS("Header",		questor->header,	fread_string(fp));
	        break;

	    case 'K':
	        KEYS("Keywords",	questor->keywords,	fread_string(fp));
	        break;

	    case 'L':
	        KEY("LineWidth",	questor->line_width,	fread_number(fp));
	        KEYS("LongDescr",	questor->long_descr,	fread_string(fp));
			break;

	    case 'P':
	        KEYS("Prefix",		questor->prefix,	fread_string(fp));
	        break;

	    case 'S':
	        KEY("Scroll",		questor->scroll,	fread_number(fp));
	        KEYS("ShortDescr",	questor->short_descr,	fread_string(fp));
	        KEYS("Suffix",		questor->suffix,	fread_string(fp));
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_questor_new: no match for word %s", word);
			bug(buf, 0);
		}
    }

    return questor;
}

SHOP_STOCK_DATA *read_shop_stock_new(FILE *fp)
{
	SHOP_STOCK_DATA *stock;
	char *word;

	stock = new_shop_stock();

    while (str_cmp((word = fread_word(fp)), "#-STOCK"))
    {
		fMatch = FALSE;
		switch (word[0]) {
		case 'C':
			if(!str_cmp(word, "Crew"))
			{
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
				stock->type = STOCK_CREW;
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
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
				stock->type = STOCK_GUARD;
				break;
			}
			break;
		case 'K':
			if(!str_cmp(word, "Keyword"))
			{
				fMatch = TRUE;
				stock->custom_keyword = fread_string(fp);
				stock->vnum = 0;
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
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
				stock->type = STOCK_MOUNT;
				break;
			}
			break;
		case 'O':
			if(!str_cmp(word, "Object"))
			{
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
				stock->type = STOCK_OBJECT;
				break;
			}
			break;
		case 'P':
			if(!str_cmp(word, "Pet"))
			{
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
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
			KEY("RestockRate", stock->restock_rate, fread_number(fp));
			break;
		case 'S':
			if(!str_cmp(word, "Ship"))
			{
				fMatch = TRUE;
				stock->vnum = fread_number(fp);
				stock->type = STOCK_SHIP;
				break;
			}
			KEY("Silver", stock->silver, fread_number(fp));
			KEY("Singular", stock->singular, TRUE);
			break;

		}

		if (!fMatch) {
			sprintf(buf, "read_shop_stock_new: no match for word %s", word);
			bug(buf, 0);
		}
	}

	stock->discount = URANGE(0, stock->discount, 100);

	return stock;
}

SHIP_CREW_INDEX_DATA *read_ship_crew_index_new(FILE *fp)
{
	SHIP_CREW_INDEX_DATA *crew;
	char *word;

	crew = new_ship_crew_index();
    while (str_cmp((word = fread_word(fp)), "#-CREW"))
    {
		fMatch = FALSE;
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

SHOP_DATA *read_shop_new(FILE *fp)
{
    SHOP_DATA *shop;
    char *word;

    shop = new_shop();

    while (str_cmp((word = fread_word(fp)), "#-SHOP"))
    {
	fMatch = FALSE;
	switch (word[0]) {
		case '#':
			if(!str_cmp(word, "#STOCK")) {
				SHOP_STOCK_DATA *stock = read_shop_stock_new(fp);

				if(stock) {
					stock->next = shop->stock;
					shop->stock = stock;
				}
				fMatch = TRUE;
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
				fMatch = TRUE;
				break;
			}
			break;

	    case 'T':
	        if (!str_cmp(word, "Trade")) {
		    int i;

		    fMatch = TRUE;

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


TOKEN_INDEX_DATA *read_token(FILE *fp)
{
    TOKEN_INDEX_DATA *token = NULL;
    EXTRA_DESCR_DATA *ed;
    PROG_LIST *tpr;
    int vnum;

    token = new_token_index();
    token->vnum = fread_number(fp);

    while (str_cmp((word = fread_word(fp)), "#-TOKEN")) {
	fMatch = FALSE;

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
		    int tindex;
		    char *p;

		    vnum = fread_number(fp);

		    p = fread_string(fp);

		    tindex = trigger_index(p, PRG_TPROG);
		    if(tindex < 0) {
			    sprintf(buf, "read_token: invalid trigger type %s", p);
			    bug(buf, 0);
		    } else {
			    tpr = new_trigger();

			    tpr->vnum = vnum;
			    tpr->trig_type = tindex;
			    tpr->trig_phrase = fread_string(fp);
			    if( tindex == TRIG_SPELLCAST ) {
					char buf[MIL];
					int tsn = skill_lookup(tpr->trig_phrase);

					if( tsn < 0 ) {
						sprintf(buf, "read_token: invalid spell '%s' for TRIG_SPELLCAST", p);
						bug(buf, 0);
						free_trigger(tpr);
						fMatch = TRUE;
						break;
					}

					free_string(tpr->trig_phrase);
					sprintf(buf, "%d", tsn);
					tpr->trig_phrase = str_dup(buf);
					tpr->trig_number = tsn;
					tpr->numeric = TRUE;

				} else {
			    	tpr->trig_number = atoi(tpr->trig_phrase);
					tpr->numeric = is_number(tpr->trig_phrase);
				}
			    tpr->trig_number = atoi(tpr->trig_phrase);
				tpr->numeric = is_number(tpr->trig_phrase);

			    if(!token->progs) token->progs = new_prog_bank();

				list_appendlink(token->progs[trigger_table[tindex].slot], tpr);
		    }
		    fMatch = TRUE;
		}
		KEY("Type",	token->type,	fread_number(fp));
		break;

            case 'V':
		if (!str_cmp(word, "Value")) {
		    int index;
		    long value;

		    fMatch = TRUE;

		    index = fread_number(fp);
		    value = fread_number(fp);

		    token->value[index] = value;
		}

		if (!str_cmp(word, "ValueName")) {
		    int index;

		    fMatch = TRUE;

		    index = fread_number(fp);
		    token->value_name[index] = fread_string(fp);
		}

		if (!str_cmp(word, "VarInt")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_integer (&token->index_vars,name,value,saved);
		}

		if (!str_cmp(word, "VarStr")) {
			char *name;
			char *str;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			str = fread_string(fp);

			variables_setindex_string (&token->index_vars,name,str,FALSE,saved);
		}

		if (!str_cmp(word, "VarRoom")) {
			char *name;
			int value;
			bool saved;

			fMatch = TRUE;

			name = fread_string(fp);
			saved = fread_number(fp);
			value = fread_number(fp);

			variables_setindex_room (&token->index_vars,name,value,saved);
		}
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
	fprintf( fp, "%ld\n", pTrade->obj_vnum );
    }

    fprintf( fp, "#0\n\n\n\n" );
    return;
}

