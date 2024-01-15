/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"


void do_gq(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];
	char arg3[MAX_STRING_LENGTH];
	char arg4[MAX_STRING_LENGTH];
	char arg5[MAX_STRING_LENGTH];
	char arg6[MAX_STRING_LENGTH];
	char arg7[MAX_STRING_LENGTH];
	char arg8[MAX_STRING_LENGTH];
	char arg9[MAX_STRING_LENGTH];
	char buf[2*MAX_STRING_LENGTH];
	CHAR_DATA *mob;
	MOB_INDEX_DATA *mob_index;
	OBJ_INDEX_DATA *obj_index;
	GQ_MOB_DATA *gq_mob;
	GQ_OBJ_DATA *gq_obj;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);
	argument = one_argument(argument, arg4);
	argument = one_argument(argument, arg5);
	argument = one_argument(argument, arg6);
	argument = one_argument(argument, arg7);
	argument = one_argument(argument, arg8);
	argument = one_argument(argument, arg9);

	if (arg[0] == '\0')
	{
		send_to_char(
			"Syntax:\n\r"
			"    gq on|off|show|save|repop|addmob|delmob\n\r"
			"    moblist|addobj|delobj|objlist\n\r"
			"    gq echo <string>\n\r", ch);
		return;
	}

	if (!str_cmp(arg, "save"))
	{
		write_gq();
		send_to_char("GQ info saved.\n\r", ch);
		return;
	}

	if (!str_cmp(arg, "show"))
	{
		int i;
		bool mob = false;
		bool obj = false;

		if (arg2[0] == '\0' && str_prefix("mob", arg2) && str_prefix("obj", arg2))
		{
			send_to_char("Syntax: gq show <mob|obj>\n\r", ch);
			return;
		}

		if (!str_prefix("mob", arg2))
			mob = true;
		if (!str_prefix("obj", arg2))
			obj = true;

		sprintf(buf, "{Y#  %-8s %-34s %-5s", "Widevnum", "Name", "Found{x\n\r");
		send_to_char(buf, ch);

		line(ch, 60);

		i = 0;
		if (mob)
		{
			for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
			{
				i++;
				mob_index = get_mob_index_auid(gq_mob->wnum_load.auid, gq_mob->wnum_load.vnum);

				sprintf(buf, "{Y%-2d{x %ld#%ld %-30.30s %5d/%-5d\n\r",
					i,
					gq_mob->wnum_load.auid,
					gq_mob->wnum_load.vnum,
					mob_index->short_descr,
					gq_mob->count,
					gq_mob->max);
				send_to_char(buf, ch);
			}
		}
		else if (obj)
		{
			for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
			{
				i++;
				obj_index = get_obj_index_auid(gq_obj->wnum_load.auid, gq_obj->wnum_load.vnum);

				sprintf(buf, "{Y%-2d{x %ld#%ld %-30.30s %5d/%-5d\n\r",
					i,
					gq_obj->wnum_load.auid,
					gq_obj->wnum_load.vnum,
					obj_index->short_descr,
					gq_obj->count,
					gq_obj->max);
				send_to_char(buf, ch);
			}
		}
		else
		{
			send_to_char("Syntax: gq show <mob|obj>\n\r", ch);
			return;
		}

		line(ch, 60);
		return;
	}
	/*
	if (!str_cmp(arg, "xpboost"))
	{
	if (!global)
	{
		send_to_char("Global mode isn't on yet.\n\r", ch);
		return;
	}

	if (!xp_boost)
	{
		sprintf(buf, "{B({WGQ{B)-->{x {BD{CO{BU{CB{BL{CE{B E{CX{BP{CE{BR{CI{BE{CN{BC{CE {Bactivated!{x\n\r");
		gecho(buf);
		xp_boost = true;
	}
	else
	{
		sprintf(buf, "{B({WGQ{B)-->{x {BD{CO{BU{CB{BL{CE{B E{CX{BP{CE{BR{CI{BE{CN{BC{CE {Bhas ended.{x\n\r");
		send_to_char(buf, ch);
		xp_boost = false;
	}
	return;
	}

	if (!str_cmp(arg, "damboost"))
	{
	if (!global)
	{
		send_to_char("Global mode isn't on yet.\n\r", ch);
		return;
	}

	if (!dam_boost)
	{
		sprintf(buf, "{B({WGQ{B)-->{x {RDAMAGE BOOST {ractivated!{x\n\r");
		gecho(buf);
		dam_boost = true;
	}
	else
	{
		sprintf(buf, "{B({WGQ{B)-->{x {RDAMAGE BOOST {rhas ended.{x\n\r");
		send_to_char(buf, ch);
		dam_boost = false;
	}
	return;
	}
	*/

	if (!str_cmp(arg, "purge"))
	{
		ITERATOR it;

		iterator_start(&it, loaded_chars);
		while((mob = (CHAR_DATA *)iterator_nextdata(&it)))
		{
			if (is_global_mob(mob))
				extract_char(mob, true);
		}
		iterator_stop(&it);

		send_to_char("All global mobs purged.\n\r", ch);
		return;
	}

	if (!str_cmp(arg, "on"))
	{
	if (global)
	{
		send_to_char("Global mode is already on!\n\r", ch);
		return;
	}

	send_to_char("Global mode {GON!@#{X\n\r", ch);
	global = true;
	return;
	}

	if (!str_cmp(arg, "off"))
	{
	if (!global)
	{
		send_to_char("Global mode is already disabled.\n\r", ch);
		return;
	}

	send_to_char("Global mode {ROFF!@#{X\n\r", ch);
	global = false;
	return;
	}

	if (!str_cmp(arg, "repop"))
	{
	global_reset();
	send_to_char("All GQ mobs and objects repopped.\n\r", ch);
	return;
	}

	if (!str_cmp(arg, "echo"))
	{
	if (!global)
	{
		send_to_char("Global mode must be turned on first.\n\r", ch);
		return;
	}

	if (arg2[0] == '\0')
	{
		send_to_char("Echo what over the GQ channel?\n\r", ch);
		return;
	}

	sprintf(buf, "{B({WGQ{B)-->{x %s\n\r", arg2);
	gecho(buf);
	return;
	}

	if (!str_cmp(arg, "addmob"))
	{
	WNUM wnum;
	WNUM obj_wnum;
	long max;

	if (arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0' || arg5[0] == '\0')
	{
		send_to_char("Syntax: gq addmob <mob widevnum> <obj widevnum> <class 0-4> <max to repop> [group]\n\r", ch);
		return;
	}

	if (!parse_widevnum(arg2, ch->in_room->area, &wnum))
	{
		send_to_char("Mob must be a widevnum.\n\r", ch);
		return;
	}

	if (!parse_widevnum(arg3, ch->in_room->area, &obj_wnum))
	{
		send_to_char("Obj must be widevnum.\n\r", ch);
		return;
	}

	if (!is_number(arg4))
	{
		send_to_char("Class must be numerical 1-4.\n\r", ch);
		return;
	}

	if (!is_number(arg5))
	{
		send_to_char("Max to repop must be numerical.\n\r", ch);
		return;
	}

	max = atoi(arg5);

	if (get_mob_index(wnum.pArea, wnum.vnum) == NULL)
	{
		send_to_char("That mob doesn't exist.\n\r", ch);
		return;
	}

	if (obj_wnum.pArea && obj_wnum.vnum > 0 && get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL)
	{
		send_to_char("That object doesn't exist.\n\r", ch);
		return;
	}

	if (atoi(arg4) < 0 || atoi(arg4) > 4)
	{
		send_to_char("Class must be between 0-4.\n\r", ch);
		return;
	}

	if (max < 1 || max > 500)
	{
		send_to_char("Range for max is 1-500.\n\r", ch);
		return;
	}

	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
		if (gq_mob->wnum_load.auid == wnum.pArea->uid &&
			gq_mob->wnum_load.vnum == wnum.vnum)
		{
		send_to_char("You only need to add a mob once.\n\r", ch);
		return;
		}
	}

	gq_mob = new_gq_mob();
	gq_mob->wnum_load.auid = wnum.pArea->uid;
	gq_mob->wnum_load.vnum = wnum.vnum;
	gq_mob->obj_load.auid = obj_wnum.pArea->uid;
	gq_mob->obj_load.vnum = obj_wnum.vnum;
	gq_mob->class = atoi(arg4);
	gq_mob->max = max;
	gq_mob->next = NULL;

	if (!str_cmp(arg5, "group"))
		gq_mob->group = true;
	else
		gq_mob->group = false;

	if (global_quest.mobs == NULL)
	{
		global_quest.mobs = gq_mob;
	}
	else
	{
		gq_mob->next = global_quest.mobs;
		global_quest.mobs = gq_mob;
	}

	sprintf(buf, "Added mob %s (wnum %ld#%ld), object %s (wnum %ld#%ld), class %d.\n\r",
		get_mob_index(wnum.pArea, wnum.vnum)->short_descr,
		wnum.pArea ? wnum.pArea->uid : 0, wnum.vnum,
		get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL ? "none" : get_obj_index(obj_wnum.pArea, obj_wnum.vnum)->short_descr,
		obj_wnum.pArea ? obj_wnum.pArea->uid : 0, obj_wnum.vnum,
		atoi(arg4));
	send_to_char(buf, ch);

	return;
	}

	if (!str_cmp(arg, "moblist"))
	{
	BUFFER *buffer;
	int i;

	if (global_quest.mobs == NULL)
	{
		send_to_char("No mobs found.\n\r", ch);
		return;
	}

	buffer = new_buf();

	sprintf(buf, "{Y#  %-20s %-10s %-20s %-10s %s %s{x\n\r",
		"Mob Name", "Mob Wnum", "Obj Name", "Obj Vnum", "Class", "Group?");
	send_to_char(buf, ch);
	line(ch, 78);

	i = 1;
	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
		mob_index = get_mob_index_auid(gq_mob->wnum_load.auid, gq_mob->wnum_load.vnum);
		obj_index = get_obj_index_auid(gq_mob->obj_load.auid, gq_mob->obj_load.vnum);
		if (mob_index != NULL)
		{
		sprintf(buf,
		"{Y%-2d{x %-20.20s %ld#%ld %-20.20s %ld#%ld %-5d %s\n\r",
			i,
			mob_index->short_descr,
			mob_index->area->uid,
			mob_index->vnum,
			obj_index ? obj_index->short_descr : "none",
			obj_index ? obj_index->area->uid : 0,
			obj_index ? obj_index->vnum : 0,
			gq_mob->class,
			gq_mob->group == true ? "yes" : "no");
		add_buf(buffer, buf);
		}

		i++;
	}

	page_to_char(buf_string(buffer), ch);
	free_buf (buffer);

	line(ch, 78);
	return;
	}

	if (!str_cmp(arg, "delmob"))
	{
	GQ_MOB_DATA *prev_gq_mob = NULL;
	int num;
	int i;

	if (arg2[0] == '\0')
	{
		send_to_char("Syntax: gq delmob <#>\n\r", ch);
		return;
	}

	num = atoi(arg2);
	i = 0;
	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
		i++;
		if (i == num)
		break;

		prev_gq_mob = gq_mob;
	}

	if (gq_mob == NULL)
	{
		send_to_char("There's no such mob on the list.\n\r", ch);
		return;
	}

	if (prev_gq_mob == NULL)
		global_quest.mobs = gq_mob->next;
	else
		prev_gq_mob->next = gq_mob->next;

	sprintf(buf, "Removed %s (wnum %ld#%ld)\n\r",
		get_mob_index_auid(gq_mob->wnum_load.auid, gq_mob->wnum_load.vnum)->short_descr,
		gq_mob->wnum_load.auid, gq_mob->wnum_load.vnum);
	send_to_char(buf, ch);

		free_gq_mob(gq_mob);
	return;
	}

	/* set up objects with rewards, etc */
	if (!str_cmp(arg, "addobj"))
	{
	WNUM wnum;
	int qp;
	int prac;
	long exp;
	int silver;
	int gold;
	int repop = -1;
	int max = -1;

	if (arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0'
		|| arg5[0] == '\0' || arg6[0] == '\0' || arg7[0] == '\0')
	{
		send_to_char(
			"Syntax: gq addobj <widevnum> <qp> <prac> <exp> <silver> <gold> [repop freq] [max to repop]\n\r", ch);
		return;
	}

	if (!parse_widevnum(arg2, ch->in_room->area, &wnum))
	{
		send_to_char("Object must be a widevnum.\n\r", ch);
		return;
	}

	if (!is_number(arg3)
	|| !is_number(arg4)
	|| !is_number(arg5)
	|| !is_number(arg6)
	|| !is_number(arg7)
	|| (arg8[0] != '\0' && !is_number(arg8))
	|| (arg9[0] != '\0' && !is_number(arg9)))
	{
		send_to_char("All arguments after the widevnum must be numerical.\n\r", ch);
		return;
	}

	if (get_obj_index(wnum.pArea, wnum.vnum) == NULL)
	{
		send_to_char("That object doesn't exist.\n\r", ch);
		return;
	}

	for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
	{
		if (gq_obj->wnum_load.auid == wnum.pArea->uid &&
			gq_obj->wnum_load.vnum == wnum.vnum)
		{
		send_to_char("You only need to add a obj once.\n\r", ch);
		return;
		}
	}

	qp = atoi(arg3);
	prac = atoi(arg4);
	exp = atol(arg5);
	silver = atoi(arg6);
	gold = atoi(arg7);

	if (arg8[0] != '\0')
		repop = atoi(arg8);

	if (arg9[0] != '\0')
		max = atoi(arg9);

	if (qp < 0 || qp > 100)
	{
		send_to_char("QP range is 1-100.\n\r", ch);
		return;
	}

	if (prac < 0 || prac > 50)
	{
		send_to_char("Practice range is 1-50.\n\r", ch);
		return;
	}

	if (exp < 0 || exp > 1000000)
	{
		send_to_char("Experience range is 1-1000000.\n\r", ch);
		return;
	}

	if (silver < 0 || silver > 30000)
	{
		send_to_char("Silver range is 1-30000.\n\r", ch);
		return;
	}

	if (gold < 0 || gold > 500)
	{
		send_to_char("Gold range is 1-500.\n\r", ch);
		return;
	}

	if (repop != -1 && (repop < 0 || repop > 100))
	{
		send_to_char("Repop frequency must be 0-10, 0 is none, 100 is highest.\n\r", ch);
		return;
	}

	if (max != -1 && (max < 0 || max > 500))
	{
		send_to_char("Max for repop range is 1-500.\n\r", ch);
		return;
	}

	gq_obj = new_gq_obj();
	gq_obj->wnum_load.auid = wnum.pArea->uid;
	gq_obj->wnum_load.vnum = wnum.vnum;
	gq_obj->qp_reward = qp;
	gq_obj->prac_reward = prac;
	gq_obj->exp_reward = exp;
	gq_obj->silver_reward = silver;
	gq_obj->gold_reward = gold;
	gq_obj->repop = repop;
	gq_obj->max = max;
	gq_obj->next = NULL;

	if (global_quest.objects == NULL)
	{
		global_quest.objects = gq_obj;
	}
	else
	{
		gq_obj->next = global_quest.objects;
		global_quest.objects = gq_obj;
	}

	sprintf(buf, "Added %s (wnum %ld#%ld), qp %d, prac %d, exp %ld, silver %d, gold %d, repop of %d%%, max amount %d.\n\r",
		get_obj_index(wnum.pArea, wnum.vnum)->short_descr,
		wnum.pArea->uid,
		wnum.vnum,
		qp,
		prac,
		exp,
		silver,
		gold,
		repop,
		max);
	send_to_char(buf, ch);
	return;
	}

	if (!str_cmp(arg, "delobj"))
	{
	GQ_OBJ_DATA *prev_gq_obj = NULL;
	int num;
	int i;

	if (arg2[0] == '\0')
	{
		send_to_char("Syntax: gq delobj <#>\n\r", ch);
		return;
	}

	num = atoi(arg2);
	i = 0;
	for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
	{
		i++;
		if (i == num)
		break;

		prev_gq_obj = gq_obj;
	}

	if (gq_obj == NULL)
	{
		send_to_char("There's no such obj on the list.\n\r", ch);
		return;
	}

	if (prev_gq_obj == NULL)
		global_quest.objects = gq_obj->next;
	else
		prev_gq_obj->next = gq_obj->next;

	sprintf(buf, "Removed %s (wnum %ld#%ld)\n\r",
		get_obj_index_auid(gq_obj->wnum_load.auid, gq_obj->wnum_load.vnum)->short_descr,
		gq_obj->wnum_load.auid,
		gq_obj->wnum_load.vnum);
	send_to_char(buf, ch);

		free_gq_obj(gq_obj);
	return;

	}

	if (!str_cmp(arg, "objlist"))
	{
	BUFFER *buffer;
	int i;

	if (global_quest.objects == NULL)
	{
		send_to_char("No objects found.\n\r", ch);
		return;
	}

	buffer = new_buf();

	sprintf(buf, "{Y#  %-20s %-10s %-7s %-7s %-10s %-7s %-7s{x\n\r",
		"Obj Name",
		"Obj Wnum",
		"QP",
		"Pracs",
		"Exp",
		"Silver",
		"Gold");
	send_to_char(buf, ch);
	line(ch, 78);

	i = 1;
	for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
	{
		obj_index = get_obj_index_auid(gq_obj->wnum_load.auid, gq_obj->wnum_load.vnum);
		if (obj_index != NULL)
		{
		sprintf(buf,
		"{Y%-2d{x %-20.20s %ld#%ld %-7d %-7d %-10ld %-7d %-7d\n\r",
			i,
			obj_index->short_descr,
			obj_index->area->uid,
			obj_index->vnum,
			gq_obj->qp_reward,
			gq_obj->prac_reward,
			gq_obj->exp_reward,
			gq_obj->silver_reward,
			gq_obj->gold_reward);
		add_buf(buffer, buf);
		}

		i++;
	}

	page_to_char(buf_string(buffer), ch);
	free_buf (buffer);

	line(ch, 78);


		return;
	}

	send_to_char(
	"Syntax:\n\r"
	"    gq on|off|show|save|repop|xpboost|damboost\n\r"
	"    addmob|delmob|moblist|addobj|delobj|objlist\n\r"
	"    gq echo <string>\n\r", ch);
}


void write_gq(void)
{
	FILE *fp;
	GQ_MOB_DATA *gq_mob;
	GQ_OBJ_DATA *gq_obj;
	int i;

	fp = fopen(GQ_FILE, "w");
	if (fp == NULL)
	{
	bug("Couldn't load gq.dat", 0);
	exit(1);
	}

	/* write mobs */
	i = 0;
	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
	i++;
	}

	fprintf(fp, "%d\n", i);
	for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
	{
	fprintf(fp, "%ld#%ld\n", gq_mob->wnum_load.auid, gq_mob->wnum_load.vnum);
	fprintf(fp, "%d\n", gq_mob->class);
	fprintf(fp, "%d\n", gq_mob->group);
	fprintf(fp, "%ld#%ld\n", gq_mob->obj_load.auid, gq_mob->obj_load.vnum);
	fprintf(fp, "%d\n", gq_mob->max);
	}

	/* write objects */
	i = 0;
	for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
	{
	i++;
	}

	fprintf(fp, "%d\n", i);
	for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
	{
	fprintf(fp, "%ld#%ld\n", gq_obj->wnum_load.auid, gq_obj->wnum_load.vnum);
	fprintf(fp, "%d\n", gq_obj->qp_reward);
	fprintf(fp, "%d\n", gq_obj->prac_reward);
	fprintf(fp, "%ld\n", gq_obj->exp_reward);
	fprintf(fp, "%d\n", gq_obj->silver_reward);
	fprintf(fp, "%d\n", gq_obj->gold_reward);
	fprintf(fp, "%d\n", gq_obj->repop);
	fprintf(fp, "%d\n", gq_obj->max);
	}

	fclose(fp);
}


void read_gq(void)
{
    FILE *fp;
    GQ_MOB_DATA *gq_mob;
    GQ_MOB_DATA *last_gq_mob = NULL;
    GQ_OBJ_DATA *gq_obj;
    GQ_OBJ_DATA *last_gq_obj = NULL;
    int i;
    int count;

    fp = fopen(GQ_FILE, "r");
    if (fp == NULL)
    {
	bug("Couldn't load gq.dat", 0);
	exit(1);
    }

    i = fread_number(fp);
    for (count = 0; count < i; count++)
    {
	gq_mob = new_gq_mob();
	if (last_gq_mob != NULL)
	    last_gq_mob->next = gq_mob;

	gq_mob->wnum_load = fread_widevnum(fp, 0);
	gq_mob->class = fread_number(fp);
	gq_mob->group = fread_number(fp);
	gq_mob->obj_load = fread_widevnum(fp, 0);
	gq_mob->max = fread_number(fp);

	last_gq_mob = gq_mob;
	if (count == 0)
	    global_quest.mobs = gq_mob;
    }

    i = fread_number(fp);
    for (count = 0; count < i; count++)
    {
	gq_obj = new_gq_obj();
	if (last_gq_obj != NULL)
	    last_gq_obj->next = gq_obj;

	gq_obj->wnum_load = fread_widevnum(fp, 0);
	gq_obj->qp_reward = fread_number(fp);
	gq_obj->prac_reward = fread_number(fp);
	gq_obj->exp_reward = fread_number(fp);
	gq_obj->silver_reward = fread_number(fp);
	gq_obj->gold_reward = fread_number(fp);
	gq_obj->repop = fread_number(fp);
	gq_obj->max = fread_number(fp);

	last_gq_obj = gq_obj;
	if (count == 0)
	    global_quest.objects = gq_obj;
    }

    fclose(fp);
}
