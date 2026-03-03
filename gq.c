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
#include "event_types.h"
#include "io/json/json_gq.h"

static void gq_set_load_from_wnum(const WNUM *wnum, WNUM_LOAD *load)
{
    if (!load) return;

    load->vnum = (wnum ? wnum->vnum : 0);
    load->auid = (wnum && wnum->pArea) ? wnum->pArea->uid : 0;
}

static void gq_resolve_wnum_load(WNUM_LOAD *load, WNUM *wnum)
{
    AREA_DATA *area;

    if (!load || !wnum || load->vnum < 1) {
        if (wnum) *wnum = wnum_zero;
        return;
    }

    if (load->auid > 0) {
        area = get_area_from_uid(load->auid);
    } else {
        WNUM res;
        if (resolve_widevnum(load->vnum, NULL, &res))
            area = res.pArea;
        else
            area = NULL;
    }
    if (!area) {
        area = get_system_area_fallback();
    }

    wnum->pArea = area;
    wnum->vnum = load->vnum;

    if (area && load->auid == 0) {
        load->auid = area->uid;
    }
}


void do_gq(CHAR_DATA *ch, char *argument)
{
    event_legacy_gq_command(ch, argument);
    return;

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
    if (arg[0] == '\0') {
        send_to_char("The legacy 'gq' system is deprecated.\n\r", ch);
        send_to_char("Use: event list | event info gq | event start gq | event stop gq\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "show") || !str_cmp(arg, "list") || !str_cmp(arg, "info")) {
        do_function(ch, &do_event, "info gq");
        return;
    }

    if (!str_cmp(arg, "on") || !str_cmp(arg, "start")) {
        do_function(ch, &do_event, "start gq");
        return;
    }

    if (!str_cmp(arg, "off") || !str_cmp(arg, "stop")) {
        do_function(ch, &do_event, "stop gq");
        return;
    }

    send_to_char("The legacy 'gq' subcommands are deprecated.\n\r", ch);
    send_to_char("Use: event list | event info gq | event start gq | event stop gq\n\r", ch);
    return;

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

        sprintf(buf, "{Y#  %-12s %-34s %-5s", "Vnum", "Name", "Found{x\n\r");
        send_to_char(buf, ch);

        line(ch, 60, NULL, NULL);

        i = 0;
        if (mob)
        {
            for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
            {
                i++;
                if (!gq_mob->vnum_wnum.pArea && gq_mob->vnum_load.vnum > 0)
                    gq_resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum);
                mob_index = get_mob_index(gq_mob->vnum_wnum.pArea, gq_mob->vnum_wnum.vnum);
                sprintf(buf, "{Y%-2d{x %-12s %-30.30s %5d/%-5d\n\r",
                    i,
                    widevnum_string_wnum(gq_mob->vnum_wnum, NULL),
                    mob_index ? mob_index->short_descr : "unknown",
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
                if (!gq_obj->vnum_wnum.pArea && gq_obj->vnum_load.vnum > 0)
                    gq_resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum);
                obj_index = get_obj_index(gq_obj->vnum_wnum.pArea, gq_obj->vnum_wnum.vnum);
                sprintf(buf, "{Y%-2d{x %-12s %-30.30s %5d/%-5d\n\r",
                    i,
                    widevnum_string_wnum(gq_obj->vnum_wnum, NULL),
                    obj_index ? obj_index->short_descr : "unknown",
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

        line(ch, 60, NULL, NULL);
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
    long max;
    WNUM mob_wnum = wnum_zero;
    WNUM obj_wnum = wnum_zero;

    if (arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0' || arg5[0] == '\0')
    {
        send_to_char("Syntax: gq addmob <mob vnum> <obj vnum> <class 0-4> <max to repop> [group]\n\r", ch);
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

    AREA_DATA *context = (ch->in_room ? ch->in_room->area : NULL);
    if (!parse_widevnum(arg2, context, &mob_wnum))
    {
        send_to_char("Mob vnum must be a valid widevnum.\n\r", ch);
        return;
    }

    if (get_mob_index(mob_wnum.pArea, mob_wnum.vnum) == NULL)
    {
        send_to_char("That mob doesn't exist.\n\r", ch);
        return;
    }

    if (strcmp(arg3, "0") != 0)
    {
        if (!parse_widevnum(arg3, context, &obj_wnum))
        {
            send_to_char("Obj vnum must be a valid widevnum or 0.\n\r", ch);
            return;
        }
        if (get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL)
        {
            send_to_char("That object doesn't exist.\n\r", ch);
            return;
        }
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
        if (wnum_match(gq_mob->vnum_wnum, mob_wnum.pArea, mob_wnum.vnum))
        {
        send_to_char("You only need to add a mob once.\n\r", ch);
        return;
        }
    }

    gq_mob = new_gq_mob();
    gq_mob->vnum_wnum = mob_wnum;
    gq_set_load_from_wnum(&mob_wnum, &gq_mob->vnum_load);
    gq_mob->obj_wnum = obj_wnum;
    gq_set_load_from_wnum(&obj_wnum, &gq_mob->obj_load);
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

    sprintf(buf, "Added mob %s (vnum %s), object %s (vnum %s), class %d.\n\r",
        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) ? get_mob_index(mob_wnum.pArea, mob_wnum.vnum)->short_descr : "(invalid)",
        widevnum_string_wnum(mob_wnum, NULL),
        (obj_wnum.vnum > 0 && get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) ? get_obj_index(obj_wnum.pArea, obj_wnum.vnum)->short_descr : "none",
        (obj_wnum.vnum > 0) ? widevnum_string_wnum(obj_wnum, NULL) : "none",
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

    sprintf(buf, "{Y#  %-20s %-12s %-20s %-12s %s %s{x\n\r",
        "Mob Name", "Mob Vnum", "Obj Name", "Obj Vnum", "Class", "Group?");
    send_to_char(buf, ch);
    line(ch, 78, NULL, NULL);

    i = 1;
    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
    {
        if (!gq_mob->vnum_wnum.pArea && gq_mob->vnum_load.vnum > 0)
            gq_resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum);
        if (!gq_mob->obj_wnum.pArea && gq_mob->obj_load.vnum > 0)
            gq_resolve_wnum_load(&gq_mob->obj_load, &gq_mob->obj_wnum);
        mob_index = get_mob_index(gq_mob->vnum_wnum.pArea, gq_mob->vnum_wnum.vnum);
        obj_index = (gq_mob->obj_wnum.vnum > 0)
            ? get_obj_index(gq_mob->obj_wnum.pArea, gq_mob->obj_wnum.vnum)
            : NULL;
        if (mob_index != NULL)
        {
        sprintf(buf,
        "{Y%-2d{x %-20.20s %-12s %-20.20s %-12s %-5d %s\n\r",
            i,
            mob_index->short_descr,
            widevnum_string_wnum(gq_mob->vnum_wnum, NULL),
            obj_index ? obj_index->short_descr : "none",
            obj_index ? widevnum_string_wnum(gq_mob->obj_wnum, NULL) : "none",
            gq_mob->class,
            gq_mob->group == true ? "yes" : "no");
        add_buf(buffer, buf);
        }

        i++;
    }

    page_to_char(buf_string(buffer), ch);
    free_buf (buffer);

    line(ch, 78, NULL, NULL);
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

    if (!gq_mob->vnum_wnum.pArea && gq_mob->vnum_load.vnum > 0)
        gq_resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum);
    sprintf(buf, "Removed %s (vnum %s)\n\r",
        get_mob_index(gq_mob->vnum_wnum.pArea, gq_mob->vnum_wnum.vnum) ? get_mob_index(gq_mob->vnum_wnum.pArea, gq_mob->vnum_wnum.vnum)->short_descr : "(invalid)",
        widevnum_string_wnum(gq_mob->vnum_wnum, NULL));
    send_to_char(buf, ch);

        free_gq_mob(gq_mob);
    return;
    }

    /* set up objects with rewards, etc */
    if (!str_cmp(arg, "addobj"))
    {
    WNUM obj_wnum = wnum_zero;
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
            "Syntax: gq addobj <vnum> <qp> <prac> <exp> <silver> <gold> [repop freq] [max to repop]\n\r", ch);
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
        send_to_char("All arguments must be numerical.\n\r", ch);
        return;
    }

    AREA_DATA *context = (ch->in_room ? ch->in_room->area : NULL);
    if (!parse_widevnum(arg2, context, &obj_wnum))
    {
        send_to_char("Obj vnum must be a valid widevnum.\n\r", ch);
        return;
    }
    if (get_obj_index(obj_wnum.pArea, obj_wnum.vnum) == NULL)
    {
        send_to_char("That object doesn't exist.\n\r", ch);
        return;
    }

    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
    {
        if (wnum_match(gq_obj->vnum_wnum, obj_wnum.pArea, obj_wnum.vnum))
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
    gq_obj->vnum_wnum = obj_wnum;
    gq_set_load_from_wnum(&obj_wnum, &gq_obj->vnum_load);
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

    sprintf(buf, "Added %s (vnum %s), qp %d, prac %d, exp %ld, silver %d, gold %d, repop of %d%%, max amount %d.\n\r",
        get_obj_index(obj_wnum.pArea, obj_wnum.vnum) ? get_obj_index(obj_wnum.pArea, obj_wnum.vnum)->short_descr : "(invalid)",
        widevnum_string_wnum(obj_wnum, NULL),
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

    if (!gq_obj->vnum_wnum.pArea && gq_obj->vnum_load.vnum > 0)
        gq_resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum);
    sprintf(buf, "Removed %s (vnum %s)\n\r",
        get_obj_index(gq_obj->vnum_wnum.pArea, gq_obj->vnum_wnum.vnum) ? get_obj_index(gq_obj->vnum_wnum.pArea, gq_obj->vnum_wnum.vnum)->short_descr : "(invalid)",
        widevnum_string_wnum(gq_obj->vnum_wnum, NULL));
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

    sprintf(buf, "{Y#  %-20s %-12s %-7s %-7s %-10s %-7s %-7s{x\n\r",
        "Obj Name",
        "Obj Vnum",
        "QP",
        "Pracs",
        "Exp",
        "Silver",
        "Gold");
    send_to_char(buf, ch);
    line(ch, 78, NULL, NULL);

    i = 1;
    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
    {
        if (!gq_obj->vnum_wnum.pArea && gq_obj->vnum_load.vnum > 0)
            gq_resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum);
        obj_index = get_obj_index(gq_obj->vnum_wnum.pArea, gq_obj->vnum_wnum.vnum);
        if (obj_index != NULL)
        {
        sprintf(buf,
        "{Y%-2d{x %-20.20s %-12s %-7d %-7d %-10ld %-7d %-7d\n\r",
            i,
            obj_index->short_descr,
            widevnum_string_wnum(gq_obj->vnum_wnum, NULL),
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

    line(ch, 78, NULL, NULL);


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
    if (!save_gq_json()) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "write_gq: failed to save gq.json");
    }
}


void read_gq(void)
{
    GQ_MOB_DATA *gq_mob;
    GQ_OBJ_DATA *gq_obj;

    if (!load_gq_json()) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "read_gq: failed to load gq.json");
        return;
    }

    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next) {
        gq_resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum);
        gq_resolve_wnum_load(&gq_mob->obj_load, &gq_mob->obj_wnum);
    }

    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next) {
        gq_resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum);
    }
}
