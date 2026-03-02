/**************************************************************************r
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
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
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <libpng/png.h>
#include <qrencode.h>
#include <sys/stat.h>  /* For chmod() */
#include <openssl/rand.h>  /* For RAND_bytes() */
#include <openssl/evp.h>
#include <sodium.h>
#include "merc.h"
#include "event_types.h"
#include "account/auth_sodium.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "io/cache/redis_cache.h"
#include "wilds.h"
#include "traits.h"
#include "class_data.h"
#include "skill_data.h"


unsigned char crypto_key[AES_KEY_SIZE]; // Server-side key
bool key_initialized;

extern LLIST *loaded_instances;
bool is_llist(const void *ptr);

/***************************************************************************
 * Loaded Object ID Hash Table                                             *
 *                                                                         *
 * O(1) lookup of objects by (id[0], id[1]) for deduplication during load.  *
 * Maintained alongside the loaded_objects LLIST.                           *
 ***************************************************************************/

static inline unsigned int loaded_obj_hash_func(unsigned long id0, unsigned long id1)
{
    /* Mix both ID components. id0 is the primary counter, id1 is the high word. */
    return (unsigned int)((id0 ^ (id1 * 2654435761UL)) % LOADED_OBJ_HASH_SIZE);
}

void loaded_obj_hash_init(void)
{
    memset(loaded_obj_hash, 0, sizeof(loaded_obj_hash));
}

void loaded_obj_hash_add(OBJ_DATA *obj)
{
    unsigned int idx;
    LOADED_OBJ_HASH_ENTRY *entry;

    if (!obj || (!obj->id[0] && !obj->id[1]))
        return;

    idx = loaded_obj_hash_func(obj->id[0], obj->id[1]);

    /* Allocate a new hash entry */
    entry = (LOADED_OBJ_HASH_ENTRY *)alloc_mem(sizeof(LOADED_OBJ_HASH_ENTRY));
    entry->id[0] = obj->id[0];
    entry->id[1] = obj->id[1];
    entry->obj = obj;
    entry->next = loaded_obj_hash[idx];
    loaded_obj_hash[idx] = entry;
}

void loaded_obj_hash_remove(OBJ_DATA *obj)
{
    unsigned int idx;
    LOADED_OBJ_HASH_ENTRY *entry, *prev;

    if (!obj || (!obj->id[0] && !obj->id[1]))
        return;

    idx = loaded_obj_hash_func(obj->id[0], obj->id[1]);
    prev = NULL;

    for (entry = loaded_obj_hash[idx]; entry; entry = entry->next) {
        if (entry->obj == obj) {
            if (prev)
                prev->next = entry->next;
            else
                loaded_obj_hash[idx] = entry->next;
            free_mem(entry, sizeof(LOADED_OBJ_HASH_ENTRY));
            return;
        }
        prev = entry;
    }
}

OBJ_DATA *loaded_obj_hash_find(unsigned long id0, unsigned long id1)
{
    unsigned int idx;
    LOADED_OBJ_HASH_ENTRY *entry;

    if (!id0 && !id1)
        return NULL;

    idx = loaded_obj_hash_func(id0, id1);

    for (entry = loaded_obj_hash[idx]; entry; entry = entry->next) {
        if (entry->id[0] == id0 && entry->id[1] == id1)
            return entry->obj;
    }

    return NULL;
}

// from act_info.c
void show_char_to_char args((CHAR_DATA * list, CHAR_DATA * ch, CHAR_DATA * victim));
bool check_blind args((CHAR_DATA * ch));


/* returns number of people on an object */
int count_users(OBJ_DATA *obj)
{
    CHAR_DATA *fch;
    int count = 0;

    if (obj->in_room == NULL)
    return 0;

    for (fch = obj->in_room->people; fch != NULL; fch = fch->next_in_room)
    if (fch->on == obj)
        count++;

    return count;
}


/* return weapon type given its name string */
int weapon_lookup (const char *name)
{

    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
    {
    if (LOWER(name[0]) == LOWER(weapon_table[type].name[0])
    &&  !str_prefix(name,weapon_table[type].name))
        return type;
    }

    return -1;
}


/* count # of items in a list */
int count_items_list(OBJ_DATA *list)
{
    int i;
    OBJ_DATA *obj;

    i = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
        i++;

    return i;
}


// Include stuff inside containers
int count_items_list_nest(OBJ_DATA *list)
{
    int i;
    OBJ_DATA *obj;

    i = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
    i++;
    if (obj->contains != NULL)
        i += count_items_list(obj->contains);
    }

    return i;
}


int ranged_weapon_type (const char *name)
{
    int type;

    for (type = 0; ranged_weapon_table[type].name != NULL; type++)
    {
        if (LOWER(name[0]) == LOWER(ranged_weapon_table[type].name[0])
        &&  !str_prefix(name,ranged_weapon_table[type].name))
            return ranged_weapon_table[type].type;
    }

    return RANGED_WEAPON_EXOTIC;
}


int weapon_type (const char *name)
{
    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
    {
        if (LOWER(name[0]) == LOWER(weapon_table[type].name[0])
        &&  !str_prefix(name,weapon_table[type].name))
            return weapon_table[type].type;
    }

    return WEAPON_UNKNOWN;
}


// Return item type string from integer
char *item_name(int item_type)
{
    int type;

    for (type = 0; item_table[type].name != NULL; type++)
    if (item_type == item_table[type].type)
        return item_table[type].name;

    return "none";
}


// Return weapon type string from integer
char *weapon_name(int weapon_type)
{
    int type;

    for (type = 0; weapon_table[type].name != NULL; type++)
        if (weapon_type == weapon_table[type].type)
            return weapon_table[type].name;

    return "unknown";
}


// Return ranged weapon type string from integer
char *ranged_weapon_name(int weapon_type)
{
    int type;

    for (type = 0; ranged_weapon_table[type].name != NULL; type++)
        if (weapon_type == ranged_weapon_table[type].type)
            return ranged_weapon_table[type].name;
    return "exotic";
}


/* find a location from a tag. Very handy! */
ROOM_INDEX_DATA *find_location(CHAR_DATA *ch, char *arg)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    AREA_DATA *area;
    ROOM_INDEX_DATA *room = NULL;
    ITERATOR it;
    char *save;
    char arg1[MIL];
    char arg2[MIL];

    // Goto <area name> ie "goto plith"
    for (area = area_first; area != NULL; area = area->next) {
        if (!is_number(arg) && !str_infix(arg, area->name)) {
            room = get_area_recall_room(area);

            /* Legacy recall values can be stored as plain vnum with no area uid.
             * When matching by area name, force that lookup to stay in the matched area.
             */
            if (!room && area->recall.id[0] > 0)
                room = get_room_index(area, area->recall.id[0]);

            if (!room && area->room_list) {
                iterator_start(&it, area->room_list);
                room = (ROOM_INDEX_DATA *)iterator_nextdata(&it);
                iterator_stop(&it);
            }

            if (room)
                return room;

            break;
        }
    }

    save = arg;
    arg = one_argument(arg,arg1);
    arg = one_argument(arg,arg2);

    // Support widevnum format for room lookup (uid#vnum, #vnum, or bare vnum)
    // Also handles cloned rooms with additional coordinates
    if (is_number(arg1) || strchr(arg1, '#')) {
        WNUM room_wnum;
        AREA_DATA *context = ch ? ch->in_room->area : NULL;
        
        if (parse_widevnum(arg1, context, &room_wnum) && room_wnum.pArea) {
            room = get_room_index(room_wnum.pArea, room_wnum.vnum);
                
            if (room && is_number(arg2) && is_number(arg))
            {
                // Clone room with coordinates
                return get_clone_room(room, atol(arg2), atol(arg));
            }
            else if (room)
            {
                return room;
            }
        }
    }

    arg = save;
    if ((victim = get_char_world(ch, arg)) != NULL)
    return victim->in_room;

    if ((obj = get_obj_world(ch, arg)) != NULL)
        return obj_room(obj);

    return NULL;
}


int attack_lookup(const char *name)
{
    int att;

    for (att = 0; attack_table[att].name != NULL; att++)
    {
    if (LOWER(name[0]) == LOWER(attack_table[att].name[0])
    &&  !str_prefix(name,attack_table[att].name))
        return att;
    }

    return 0;
}


/* returns a flag for wiznet */
long wiznet_lookup (const char *name)
{
    long flag;

    for (flag = 0; wiznet_table[flag].name != NULL; flag++)
    {
    if (LOWER(name[0]) == LOWER(wiznet_table[flag].name[0])
    && !str_prefix(name,wiznet_table[flag].name))
        return flag;
    }

    return -1;
}


/* returns class number */
int class_lookup(const char *name)
{
   int class;

   for (class = 0; class < MAX_CLASS; class++)
   {
        const char *legacy_name = class_name_from_legacy(class);
        if (!legacy_name)
            continue;

        if (LOWER(name[0]) == LOWER(legacy_name[0])
        &&  !str_prefix(name, legacy_name))
            return class;
   }

   {
       CLASS_DATA *clazz = class_find(name);
       class = class_legacy_index(clazz);
       if (class >= 0)
           return class;
   }

   return -1;
}


int sub_class_lookup(CHAR_DATA *ch, const char *name)
{
   int sub_class;
   CLASS_DATA *clazz = class_find(name);

   if (clazz)
   {
       sub_class = sub_class_legacy_index(clazz);
       if (sub_class < 0)
           return -1;
       if (sub_class_legacy_is_remort(sub_class))
           return -1;
       if (sub_class_legacy_alignment(sub_class) == ALIGN_GOOD && ch->alignment < 0)
           return -1;
       if (sub_class_legacy_alignment(sub_class) == ALIGN_EVIL && ch->alignment > 0)
           return -1;
       return sub_class;
   }

   for (sub_class = 0; sub_class < MAX_SUB_CLASS; sub_class++)
   {
    CLASS_DATA *legacy_sub = class_from_legacy(0, sub_class);
    if (legacy_sub
    &&  !str_prefix(name, class_name(legacy_sub))
    &&  !sub_class_legacy_is_remort(sub_class))
    {
        if (sub_class_legacy_alignment(sub_class) == ALIGN_GOOD
        &&  ch->alignment < 0)
        return -1;

        if (sub_class_legacy_alignment(sub_class) == ALIGN_EVIL
        &&  ch->alignment > 0)
        return -1;

        return sub_class;
    }
   }

   return -1;
}

int sub_class_search(const char *name)
{
   int sub_class;
   CLASS_DATA *clazz = class_find(name);

   if (clazz)
       return sub_class_legacy_index(clazz);

   for (sub_class = 0; sub_class < MAX_SUB_CLASS; sub_class++)
   {
       CLASS_DATA *legacy_sub = class_from_legacy(0, sub_class);
       if (legacy_sub && !str_prefix(name, class_name(legacy_sub)))
           return sub_class;
   }

   return -1;
}

/* for immunity, vulnerabiltiy, and resistant
   the 'globals' (magic and weapons) may be overriden
   three other cases -- wood, silver, and iron -- are checked in fight.c */
int check_immune(CHAR_DATA *ch, int16_t dam_type)
{
    int immune, def;
    long bit;

    immune = -1;
    def = IS_NORMAL;

    if (dam_type == DAM_NONE)
    return immune;

    if (dam_type <= 3)
    {
    if (IS_SET(ch->imm_flags,IMM_WEAPON))
        def = IS_IMMUNE;
    else if (IS_SET(ch->res_flags,RES_WEAPON))
        def = IS_RESISTANT;
    else if (IS_SET(ch->vuln_flags,VULN_WEAPON))
        def = IS_VULNERABLE;
    }
    /*
    else  magical attack
    {
    if (IS_SET(ch->imm_flags,IMM_MAGIC))
        def = IS_IMMUNE;
    else if (IS_SET(ch->res_flags,RES_MAGIC))
        def = IS_RESISTANT;
    else if (IS_SET(ch->vuln_flags,VULN_MAGIC))
        def = IS_VULNERABLE;
    }
    */

    switch (dam_type)
    {
    case(DAM_BASH):		bit = IMM_BASH;		break;
    case(DAM_PIERCE):	bit = IMM_PIERCE;	break;
    case(DAM_SLASH):	bit = IMM_SLASH;	break;
    case(DAM_FIRE):		bit = IMM_FIRE;		break;
    case(DAM_COLD):		bit = IMM_COLD;		break;
    case(DAM_LIGHTNING):	bit = IMM_LIGHTNING;	break;
    case(DAM_ACID):		bit = IMM_ACID;		break;
    case(DAM_POISON):	bit = IMM_POISON;	break;
    case(DAM_NEGATIVE):	bit = IMM_NEGATIVE;	break;
    case(DAM_HOLY):		bit = IMM_HOLY;		break;
    case(DAM_MAGIC):	bit = IMM_MAGIC;	break;
    case(DAM_ENERGY):	bit = IMM_ENERGY;       break;
    case(DAM_MENTAL):	bit = IMM_MENTAL;	break;
    case(DAM_DISEASE):	bit = IMM_DISEASE;	break;
    case(DAM_DROWNING):	bit = IMM_DROWNING;	break;
    case(DAM_LIGHT):	bit = IMM_LIGHT;	break;
    case(DAM_CHARM):	bit = IMM_CHARM;	break;
    case(DAM_SOUND):	bit = IMM_SOUND;	break;
    case(DAM_WATER):	bit = IMM_WATER;	break;	// @@@NIB : 20070120
    case(DAM_AIR):		bit = IMM_AIR;		break;	// @@@NIB : 20070125
    case(DAM_EARTH):	bit = IMM_EARTH;	break;	// @@@NIB : 20070125
    case(DAM_PLANT):	bit = IMM_PLANT;	break;	// @@@NIB : 20070125
    default:		return def;
    }

    if (IS_SET(ch->imm_flags,bit))
    immune = IS_IMMUNE;
    else if (IS_SET(ch->res_flags,bit) && immune != IS_IMMUNE)
    immune = IS_RESISTANT;
    else if (IS_SET(ch->vuln_flags,bit))
    {
    if (immune == IS_IMMUNE)
        immune = IS_RESISTANT;
    else if (immune == IS_RESISTANT)
        immune = IS_NORMAL;
    else
        immune = IS_VULNERABLE;
    }

    if (immune == -1)
    return def;
    else
          return immune;
}



// Get a character's % at a skill.
int get_skill(CHAR_DATA *ch, int sn)
{
    int skill;
    SKILL_ENTRY *entry = NULL;

    if (!IS_NPC(ch) && sn >= 0 && sn < MAX_SKILL)
        entry = skill_entry_findsn(ch->sorted_skills, sn);

    // Racial skills
    if (!IS_NPC(ch) && ch->race && sn >= 0 && sn < MAX_SKILL)
    {
        if (race_has_skill(ch->race, skill_table[sn].name)) {
            if (!entry)
                return 0;

            skill = skill_entry_rating(ch, entry);

            if (skill <= 0) return skill;

            skill += skill_entry_mod(ch, entry);

            return URANGE(1, skill, 100);
        }
    }

    if (sn == -1) /* shorthand for level based skills */
        skill = ch->level * 5 / 2;
    else if (sn < -1 || sn > MAX_SKILL)
    {
    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Bad sn %d in get_skill, on char %s.",sn,ch->name);
    skill = 0;
    }
    else if (!IS_NPC(ch))
    {
    int this_class;

    this_class = get_this_class(ch,sn);

    if (had_skill(ch,sn) || ch->level >= skill_table[sn].skill_level[this_class]) {
        if (entry)
            skill = skill_entry_rating(ch, entry);
        else
            skill = 0;
    }
    else
        skill = 0;

    /* Cross-class scope check: verify class-granted skills are currently
     * available with the character's active class setup. */
    if (skill > 0) {
        if (entry && !skill_entry_is_usable_now(ch, entry))
            skill = 0;
    }

    if(skill > 0) {
        if (entry)
            skill += skill_entry_mod(ch, entry);
        skill = URANGE(1,skill,100);
    }
    }
    else /* mobiles */
    {
        /* AO 092916 -- Making it better, but not perfetc, for now. Based on flags.
     * Changed level calculation to the log functio to scale
     * well up to lv500  */

    // Account for racial skills.
    if (skill_table[sn].race != -1 && (!ch->race || ch->race->uid != skill_table[sn].race))
        skill = 0;
    if (ch->tot_level < 10)
        skill = 10;

    /* Handle spells */
    if (skill_table[sn].spell_fun != spell_null) {
        if (ch->max_mana > 0)
            skill = 40+19 * log10(ch->tot_level)/2;
        else
            skill = 0;
    } //thief skills
    else if (IS_SET(ch->act[0], ACT_THIEF) && (sn == skill_resolve_gsn("sneak") || sn == skill_resolve_gsn("hide")))
        skill = 40+19 * log10(ch->tot_level)/2;

        else if ((sn == skill_resolve_gsn("dodge") && IS_SET(ch->off_flags,OFF_DODGE))
     ||       (sn == skill_resolve_gsn("parry") && IS_SET(ch->off_flags,OFF_PARRY)))
        skill = 40+19 * log10(ch->tot_level)/2;

     else if (sn == skill_resolve_gsn("shield block"))
        skill = 40+19 * log10(ch->tot_level)/2;

    else if (sn == skill_resolve_gsn("second attack")
    && (IS_SET(ch->act[0],ACT_WARRIOR) || IS_SET(ch->act[0],ACT_THIEF)))
        skill = 40+19 * log10(ch->tot_level)/2;

    else if (sn == skill_resolve_gsn("third attack") && IS_SET(ch->act[0],ACT_WARRIOR))
        skill = 40 * log10(ch->tot_level);

    else if (sn == skill_resolve_gsn("hand to hand"))
        skill = 40 + 19 * log10(ch->tot_level)/2;

     else if (sn == skill_resolve_gsn("bash") && IS_SET(ch->off_flags,OFF_BASH))
        skill = 40 + 19 * log10(ch->tot_level)/1.5;

    else if (sn == skill_resolve_gsn("disarm")
         &&  (IS_SET(ch->off_flags,OFF_DISARM)
         ||   IS_SET(ch->act[0],ACT_WARRIOR)
         ||	  IS_SET(ch->act[0],ACT_THIEF)))
        skill = 40 + 19*log10(ch->tot_level)/1.5;

    else if (sn == skill_resolve_gsn("berserk") && IS_SET(ch->off_flags,OFF_BERSERK))
        skill = 19*log10(ch->tot_level)/1.5;

    else if (sn == skill_resolve_gsn("kick"))
        skill = 40 + 19*log10(ch->tot_level)/1.5;

    else if (sn == skill_resolve_gsn("backstab") && IS_SET(ch->act[0],ACT_THIEF))
        skill = 40 + 19*log10(ch->tot_level)/2;

      else if (sn == skill_resolve_gsn("rescue"))
        skill = 40 + 19*log10(ch->tot_level)/2;

    else if (sn == skill_resolve_gsn("recall"))
        skill = 40 + 19*log10(ch->tot_level)/2;

    else if (sn == skill_resolve_gsn("sword")
    ||  sn == skill_resolve_gsn("dagger")
    ||  sn == skill_resolve_gsn("spear")
    ||  sn == skill_resolve_gsn("mace")
    ||  sn == skill_resolve_gsn("axe")
    ||  sn == skill_resolve_gsn("flail")
    ||  sn == skill_resolve_gsn("whip")
    ||  sn == skill_resolve_gsn("stake")
    ||  sn == skill_resolve_gsn("polearm")
    ||  sn == skill_resolve_gsn("quarterstaff"))
        skill = 40 + 19*log10(ch->tot_level);

    else
       skill = 0;
    }

    if (ch->daze > 0)
    {
    if (skill_table[sn].spell_fun != spell_null)
        skill /= 2;
    else
        skill = 2 * skill / 3;
    }

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK]  > 10)
    skill = 9 * skill / 10;

    return URANGE(0,skill,100);
}


/* for returning weapon information */
int get_weapon_sn(CHAR_DATA *ch)
{
    OBJ_DATA *wield;
    int sn;

    wield = get_eq_char(ch, WEAR_WIELD);
    if (wield == NULL || wield->item_type != ITEM_WEAPON)
        sn = skill_resolve_gsn("hand to hand");

    else switch (WEAPON(wield)->weapon_class)
    {
        default :                  sn = -1; 	       		break;
        case(WEAPON_SWORD):        sn = skill_resolve_gsn("sword");         	break;
        case(WEAPON_EXOTIC):       sn = skill_resolve_gsn("exotic");         	break;
        case(WEAPON_DAGGER):       sn = skill_resolve_gsn("dagger");        	break;
        case(WEAPON_SPEAR):        sn = skill_resolve_gsn("spear");         	break;
        case(WEAPON_MACE):         sn = skill_resolve_gsn("mace");          	break;
        case(WEAPON_AXE):          sn = skill_resolve_gsn("axe");           	break;
        case(WEAPON_FLAIL):        sn = skill_resolve_gsn("flail");         	break;
        case(WEAPON_WHIP):         sn = skill_resolve_gsn("whip");          	break;
        case(WEAPON_POLEARM):      sn = skill_resolve_gsn("polearm");       	break;
        case(WEAPON_STAKE):        sn = skill_resolve_gsn("stake");       	break;
    case(WEAPON_QUARTERSTAFF): sn = skill_resolve_gsn("quarterstaff"); 	break;
   }

   return sn;
}

int get_objweapon_sn(OBJ_DATA *obj)
{
    int sn;

    if (!obj || obj->item_type != ITEM_WEAPON)
        sn = skill_resolve_gsn("hand to hand");

    else switch (WEAPON(obj)->weapon_class)
    {
        default :                  sn = -1; 	       		break;
        case(WEAPON_SWORD):        sn = skill_resolve_gsn("sword");         	break;
        case(WEAPON_EXOTIC):       sn = skill_resolve_gsn("exotic");         	break;
        case(WEAPON_DAGGER):       sn = skill_resolve_gsn("dagger");        	break;
        case(WEAPON_SPEAR):        sn = skill_resolve_gsn("spear");         	break;
        case(WEAPON_MACE):         sn = skill_resolve_gsn("mace");          	break;
        case(WEAPON_AXE):          sn = skill_resolve_gsn("axe");           	break;
        case(WEAPON_FLAIL):        sn = skill_resolve_gsn("flail");         	break;
        case(WEAPON_WHIP):         sn = skill_resolve_gsn("whip");          	break;
        case(WEAPON_POLEARM):      sn = skill_resolve_gsn("polearm");       	break;
        case(WEAPON_STAKE):        sn = skill_resolve_gsn("stake");       	break;
    case(WEAPON_QUARTERSTAFF): sn = skill_resolve_gsn("quarterstaff"); 	break;
   }

   return sn;
}


int get_weapon_skill(CHAR_DATA *ch, int sn)
{
    int skill;

     /* -1 is default */
    if (IS_NPC(ch))
    {
    if (sn == -1)
        skill = 3 * ch->level;
    else if (sn == skill_resolve_gsn("hand to hand"))
        skill = 40 + 2 * ch->level;
    else
        skill = 40 + 5 * ch->level / 2;
    }

    else
    {
    if (sn == -1)
        skill = ch->level;
    else
        skill = get_skill(ch, sn);
    }

    return URANGE(0,skill,100);
}


/* used to de-screw characters */
void reset_char(CHAR_DATA *ch)
{
    int loc;
    int mod;
    int stat;
    OBJ_DATA *obj;
    AFFECT_DATA *af;
    int i;

    if (IS_NPC(ch))
    return;

    if (ch->pcdata->perm_hit == 0
    ||	ch->pcdata->perm_mana == 0
    ||  ch->pcdata->perm_move == 0
    ||	ch->pcdata->last_level == 0)
    {
    /* do a FULL reset */
    for (loc = 0; loc < MAX_WEAR; loc++)
    {
        obj = get_eq_char(ch,loc);
        if (obj == NULL)
        continue;
        for (af = obj->affected; af != NULL; af = af->next)
        {
        mod = af->modifier;
        switch(af->location)
        {
            case APPLY_SEX:     ch->sex         -= mod;         break;
            case APPLY_MANA:    ch->max_mana    -= mod;         break;
            case APPLY_HIT:     ch->max_hit     -= mod;         break;
            case APPLY_MOVE:    ch->max_move    -= mod;         break;
        }
        }
    }

    /* now reset the permanent stats */
    ch->pcdata->perm_hit 	= ch->max_hit;
    ch->pcdata->perm_mana 	= ch->max_mana;
    ch->pcdata->perm_move	= ch->max_move;
    ch->pcdata->last_level	= ch->played/3600;

    if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
    {
        if (ch->sex > 0 && ch->sex < 3)
        ch->pcdata->true_sex	= ch->sex;
        else
        ch->pcdata->true_sex 	= 0;
    }

    }

    if(!IS_NPC(ch)) memset(ch->pcdata->mod_learned,0,sizeof(ch->pcdata->mod_learned));

    /* now restore the character to his/her true condition */
    for (stat = 0; stat < MAX_STATS; stat++)
        set_mod_stat(ch, stat, 0);

    if (ch->pcdata->true_sex < 0 || ch->pcdata->true_sex > 2)
    ch->pcdata->true_sex = 0;
    ch->sex		= ch->pcdata->true_sex;
    ch->max_hit 	= ch->pcdata->perm_hit;
    ch->max_mana	= ch->pcdata->perm_mana;
    ch->max_move	= ch->pcdata->perm_move;

    for (i = 0; i < 4; i++)
    ch->armour[i]	= 100;

    ch->hitroll		= 0;
    ch->damroll		= 0;
    ch->saving_throw	= 0;

    /* now start adding back the effects */
    for (loc = 0; loc < MAX_WEAR; loc++)
    {
    obj = get_eq_char(ch,loc);
    if (obj == NULL)
        continue;
    for (i = 0; i < 4; i++)
        ch->armour[i] -= apply_ac(obj, loc, i);
    for (af = obj->affected; af != NULL; af = af->next)
    {
        mod = af->modifier;
        switch(af->location)
        {
        case APPLY_STR:         add_mod_stat(ch,STAT_STR,mod); break;
        case APPLY_DEX:         add_mod_stat(ch,STAT_DEX,mod); break;
        case APPLY_INT:         add_mod_stat(ch,STAT_INT,mod); break;
        case APPLY_WIS:         add_mod_stat(ch,STAT_WIS,mod); break;
        case APPLY_CON:         add_mod_stat(ch,STAT_CON,mod); break;

        case APPLY_SEX:         ch->sex                 += mod; break;
        case APPLY_MANA:        ch->max_mana            += mod; break;
        case APPLY_HIT:         ch->max_hit             += mod; break;
        case APPLY_MOVE:        ch->max_move            += mod; break;

        case APPLY_AC:
                    for (i = 0; i < 4; i ++)
                        ch->armour[i] += mod;
                    break;
        case APPLY_HITROLL:     ch->hitroll             += mod; break;
        case APPLY_DAMROLL:     ch->damroll             += mod; break;
        default:
            if(!IS_NPC(ch) && af->location >= APPLY_SKILL && af->location < APPLY_SKILL_MAX) {
                ch->pcdata->mod_learned[af->location - APPLY_SKILL] += mod;
                break;
            }
            break;
        }
    }
    }

    /* now add back spell effects */
    for (af = ch->affected; af != NULL; af = af->next)
    {
    mod = af->modifier;
    switch(af->location)
    {
        case APPLY_STR:         add_mod_stat(ch,STAT_STR,mod); break;
        case APPLY_DEX:         add_mod_stat(ch,STAT_DEX,mod); break;
        case APPLY_INT:         add_mod_stat(ch,STAT_INT,mod); break;
        case APPLY_WIS:         add_mod_stat(ch,STAT_WIS,mod); break;
        case APPLY_CON:         add_mod_stat(ch,STAT_CON,mod); break;

        case APPLY_SEX:         ch->sex                 += mod; break;
        case APPLY_MANA:        ch->max_mana            += mod; break;
        case APPLY_HIT:         ch->max_hit             += mod; break;
        case APPLY_MOVE:        ch->max_move            += mod; break;

        case APPLY_AC:
                    for (i = 0; i < 4; i ++)
                    ch->armour[i] += mod;
                    break;
        case APPLY_HITROLL:     ch->hitroll             += mod; break;
        case APPLY_DAMROLL:     ch->damroll             += mod; break;
        default:
            if(!IS_NPC(ch) && af->location >= APPLY_SKILL && af->location < APPLY_SKILL_MAX) {
                ch->pcdata->mod_learned[af->location - APPLY_SKILL] += mod;
                break;
            }
            break;


    }
    }

    /* make sure sex is RIGHT!!!! */
    if (ch->sex < 0 || ch->sex > 2)
    ch->sex = ch->pcdata->true_sex;
}


/**
 * get_mob_level - Get a mobile's effective level
 *
 * For NPCs, returns tot_level clamped to 1..149.
 * For PCs, returns tot_level (used in scripting for object creation level).
 */
int get_mob_level(CHAR_DATA *ch)
{
    if (ch == NULL)
    return 0;


    if (IS_NPC(ch))
    return (URANGE(1,ch->tot_level,149));
    else
    return ch->tot_level;
}


int get_age(CHAR_DATA *ch)
{
    return 17 + (ch->played + (int) (current_time - ch->logon)) / (4*72000);
}

void set_mod_stat(CHAR_DATA *ch, int stat, int value)
{
    ch->mod_stat[stat] = value;
    ch->dirty_stat[stat] = true;
}

void add_mod_stat(CHAR_DATA *ch, int stat, int adjust)
{
    ch->mod_stat[stat] += adjust;
    ch->dirty_stat[stat] = true;
}

void set_perm_stat(CHAR_DATA *ch, int stat, int value)
{
    ch->perm_stat[stat] = value;
    ch->dirty_stat[stat] = true;
}

void add_perm_stat(CHAR_DATA *ch, int stat, int adjust)
{
    ch->perm_stat[stat] += adjust;
    ch->dirty_stat[stat] = true;
}

void set_perm_stat_range(CHAR_DATA *ch, int stat, int value, int mn, int mx)
{
    ch->perm_stat[stat] = URANGE(mn, value, mx);
    ch->dirty_stat[stat] = true;
}

/* command for retrieving stats */
int get_curr_stat(CHAR_DATA *ch, int stat)
{
    int max, cur;

    if (ch->dirty_stat[stat]) {

        cur = ch->perm_stat[stat] + ch->mod_stat[stat];
        max = (!IS_NPC(ch) && ch->race) ? ch->race->max_stats[stat] : 25;
        if (cur > max) {
            float t = exp(-0.0075*(cur-max));
            cur = max + (int)((50-max)*(1-t)/(1+t)+0.5);
        }

        ch->cur_stat[stat] = UMAX(3,cur);
        ch->dirty_stat[stat] = false;
    }

    return ch->cur_stat[stat];
}


/* command for returning max training score */
int get_max_train(CHAR_DATA *ch, int stat)
{
    int max;

    if (IS_NPC(ch) || ch->level > LEVEL_IMMORTAL)
    return 25;

    max = ch->race ? ch->race->max_stats[stat] : 20;
/* nrrk! disabling this, too! -- Areo
    if ((stat == STAT_INT && ch->pcdata->class_mage != -1)
    ||  (stat == STAT_WIS && ch->pcdata->class_cleric != -1)
    ||  (stat == STAT_DEX && ch->pcdata->class_thief != -1)
    ||  (stat == STAT_STR && ch->pcdata->class_warrior != -1))
    {*/
    if (ch->race && (!str_cmp(ch->race->id, "human") || !str_cmp(ch->race->id, "avatar")))
       max += 1;
/*	else
       max += 2;
    }*/


    return UMIN(max,25);
}


/*
 * Retrieve a character's carry capacity.
 */
int can_carry_n(CHAR_DATA *ch)
{
    if (IS_IMMORTAL(ch))
    return 1000;

    if (IS_NPC(ch) && IS_SET(ch->act[0], ACT_PET))
    return 0;

    return MAX_WEAR + get_curr_stat(ch,STAT_DEX) + ch->tot_level/2;
}


/*
 * Retrieve a character's carry capacity.
 */
int can_carry_w(CHAR_DATA *ch)
{
    int weight;

    if (IS_IMMORTAL(ch))
    return 10000000;

    if (IS_NPC(ch) && IS_SET(ch->act[0], ACT_PET))
    return 0;

    weight = str_app[get_curr_stat(ch,STAT_STR)].carry + ch->tot_level/5;
    if (IS_REMORT(ch))
    weight += weight/4;

    return weight;
}


/*
 * See if a string is one of the names of an object.
 */
bool is_name (char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH], part[MAX_INPUT_LENGTH];
    char *list, *string;

    /* fix crash on NULL namelist */
    if (namelist == NULL || namelist[0] == '\0')
        return false;

    /* fixed to prevent is_name on "" returning true */
    if (str[0] == '\0')
    return false;

    string = str;
    /* we need ALL parts of string to match part of namelist */
    for (; ;)  /* start parsing string */
    {
    str = one_argument(str,part);

    if (part[0] == '\0')
        return true;

    /* check to see if this is part of namelist */
    list = namelist;
    for (; ;)  /* start parsing namelist */
    {
        list = one_argument(list,name);
        if (name[0] == '\0')  /* this name was not found */
        return false;

        if (!str_prefix(string,name))
        return true; /* full pattern match */

        if (!str_prefix(part,name))
        break;
    }
    }
}



bool is_exact_name(char *str, char *namelist)
{
    char name[MAX_INPUT_LENGTH];

    if (namelist == NULL)
    return false;

    for (; ;)
    {
    namelist = one_argument(namelist, name);
    if (name[0] == '\0')
        return false;
    if (!str_cmp(str, name))
        return true;
    }
}

void affect_fix_char(CHAR_DATA *ch)
{
    AFFECT_DATA *paf;
    OBJ_DATA *obj;
    ITERATOR it;

    // Reset flags
    ch->affected_by[0] = ch->affected_by_perm[0];
    ch->affected_by[1] = ch->affected_by_perm[1];
    ch->imm_flags = ch->imm_flags_perm;
    ch->res_flags = ch->res_flags_perm;
    ch->vuln_flags = ch->vuln_flags_perm;

    ch->deathsight_vision = ( IS_SET(ch->affected_by_perm[1], AFF2_DEATHSIGHT) ) ? ch->tot_level : 0;

    // Iterate through affects on character
    for(paf = ch->affected; paf; paf = paf->next)
    {
        switch(paf->where)
        {
            case TO_AFFECTS:
                SET_BIT(ch->affected_by[0], paf->bitvector);
                SET_BIT(ch->affected_by[1], paf->bitvector2);

                if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
                    ch->deathsight_vision = paf->level;

                break;
            case TO_IMMUNE:
                SET_BIT(ch->imm_flags,paf->bitvector);
                break;
            case TO_RESIST:
                SET_BIT(ch->res_flags,paf->bitvector);
                break;
            case TO_VULN:
                SET_BIT(ch->vuln_flags,paf->bitvector);
                break;
        }
    }

    // Iterate through all worn objects using lworn list
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            for(paf = obj->affected; paf; paf = paf->next)
            {
                switch (paf->where)
                {
                    case TO_AFFECTS:
                        SET_BIT(ch->affected_by[0], paf->bitvector);
                        SET_BIT(ch->affected_by[1], paf->bitvector2);

                        if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
                            ch->deathsight_vision = paf->level;

                        break;
                    case TO_IMMUNE:
                        SET_BIT(ch->imm_flags, paf->bitvector);
                        break;
                    case TO_RESIST:
                        SET_BIT(ch->res_flags, paf->bitvector);
                        break;
                    case TO_VULN:
                        SET_BIT(ch->vuln_flags, paf->bitvector);
                        break;
                }
            }
        }
        iterator_stop(&it);
    }
}


/*
 * Apply or remove an affect to a character.
 */
void affect_modify(CHAR_DATA *ch, AFFECT_DATA *paf, bool fAdd)
{
    OBJ_DATA *wield;
    int mod;
    int i;

    mod = paf->modifier;

    /* add an affect to a char */
    if (fAdd)
    {
        switch (paf->where)
        {
            case TO_AFFECTS:
                SET_BIT(ch->affected_by[0], paf->bitvector);
                SET_BIT(ch->affected_by[1], paf->bitvector2);

                if( IS_SET(paf->bitvector2, AFF2_DEATHSIGHT) && (paf->level > ch->deathsight_vision) )
                    ch->deathsight_vision = paf->level;

            break;
            case TO_IMMUNE:
                SET_BIT(ch->imm_flags,paf->bitvector);
            break;
            case TO_RESIST:
                SET_BIT(ch->res_flags,paf->bitvector);
            break;
            case TO_VULN:
                SET_BIT(ch->vuln_flags,paf->bitvector);
            break;
        }
    }
    else /* take an affect from a char */
    {
        switch (paf->where)
    {
        case TO_AFFECTS:
        MERGE_BIT(ch->affected_by[0], ch->affected_by_perm[0], paf->bitvector);
        MERGE_BIT(ch->affected_by[1], ch->affected_by_perm[1], paf->bitvector2);
        break;
        case TO_IMMUNE:
            MERGE_BIT(ch->imm_flags,ch->imm_flags_perm,paf->bitvector);
        break;
        case TO_RESIST:
            MERGE_BIT(ch->res_flags,ch->res_flags_perm,paf->bitvector);
        break;
        case TO_VULN:
        MERGE_BIT(ch->vuln_flags,ch->vuln_flags_perm,paf->bitvector);
        break;
    }
        mod = -mod; /* reverse modifier */
    }

    /* cancel out affects */
    switch (paf->location)
    {
    case APPLY_NONE:						break;
    case APPLY_STR:           add_mod_stat(ch,STAT_STR,mod);	break;
    case APPLY_DEX:           add_mod_stat(ch,STAT_DEX,mod);	break;
    case APPLY_INT:           add_mod_stat(ch,STAT_INT,mod);	break;
    case APPLY_WIS:           add_mod_stat(ch,STAT_WIS,mod);	break;
    case APPLY_CON:           add_mod_stat(ch,STAT_CON,mod);	break;
    case APPLY_SEX:           ch->sex			+= mod;	break;
    case APPLY_MANA:          ch->max_mana			+= mod;	break;
    case APPLY_HIT:
        // Make sure it doesn't make them DEAD
        if( fAdd && ((ch->max_hit + mod) < 1))
            mod = 0;
        else if( !fAdd && ((ch->max_hit - mod) < 1))
            mod = 0;

        ch->max_hit						+= mod;	break;
    case APPLY_MOVE:          ch->max_move			+= mod;	break;
    case APPLY_GOLD:						break;
    case APPLY_AC:
        for (i = 0; i < 4; i ++)
        ch->armour[i] += mod;
        break;
    case APPLY_HITROLL:       ch->hitroll			+= mod;	break;
    case APPLY_DAMROLL:       ch->damroll			+= mod;	break;
    case APPLY_SPELL_AFFECT:  					break;
    default:
        if(!IS_NPC(ch) && paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
            ch->pcdata->mod_learned[paf->location - APPLY_SKILL] += mod;
            break;
        }
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Affect_modify: unknown location %d.", paf->location);
        return;
    }

    /*
     * If it takes away strength they drop their weapon.
     */
    if (!IS_NPC(ch)
    && (wield = get_eq_char(ch, WEAR_WIELD)) != NULL
    && !IS_AFFECTED(ch, AFF_DEATH_GRIP)
    && get_obj_weight(wield) > (str_app[get_curr_stat(ch,STAT_STR)].wield*10))
    {
    static int depth;

    if (depth == 0)
    {
        depth++;
        act("You drop $p.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n drops $p.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        obj_from_char(wield);
        obj_to_room(wield, ch->in_room);
        depth--;
    }
    }
}


/* find an effect in an affect list */
static bool affect_matches_skill_sn(const AFFECT_DATA *paf, int sn, SKILL_DATA *skill)
{
    if (!paf)
        return false;

    if (skill && paf->skill == skill)
        return true;

    return paf->type == sn;
}

AFFECT_DATA *affect_find(AFFECT_DATA *paf, int sn)
{
    AFFECT_DATA *paf_find;
    SKILL_DATA *skill = skill_find_uid(sn);

    for (paf_find = paf; paf_find != NULL; paf_find = paf_find->next)
    {
        if (affect_matches_skill_sn(paf_find, sn, skill))
    return paf_find;
    }

    return NULL;
}


/* fix object affects when removing one */
void affect_check(CHAR_DATA *ch, int where, long vector, long vector2)
{
    AFFECT_DATA *paf;
    OBJ_DATA *obj;
    ITERATOR it;

    if (where == TO_OBJECT || where == TO_OBJECT2 || where == TO_OBJECT3 || where == TO_OBJECT4 || where == TO_WEAPON)
    return;

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
        if (paf->where == where && paf->bitvector == vector)
        {
            switch (where)
            {
                case TO_AFFECTS:
                SET_BIT(ch->affected_by[0],vector);
                break;
                case TO_IMMUNE:
                SET_BIT(ch->imm_flags,vector);
                break;
                case TO_RESIST:
                SET_BIT(ch->res_flags,vector);
                break;
                case TO_VULN:
                SET_BIT(ch->vuln_flags,vector);
                break;
            }
            return;
        }
        else if (paf->where == where && paf->bitvector2 == vector2)
        {
            switch (where)
            {
            case TO_AFFECTS:
                SET_BIT(ch->affected_by[1],vector2);
                break;
            }
            return;
        }
    }

    // Use the lworn list to check worn objects
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            for (paf = obj->affected; paf != NULL; paf = paf->next)
            {
                if (paf->where == where && paf->bitvector == vector)
                {
                    switch (where)
                    {
                    case TO_AFFECTS:
                        SET_BIT(ch->affected_by[0],vector);
                        break;
                    case TO_IMMUNE:
                        SET_BIT(ch->imm_flags,vector);
                        break;
                    case TO_RESIST:
                        SET_BIT(ch->res_flags,vector);
                        break;
                    case TO_VULN:
                        SET_BIT(ch->vuln_flags,vector);
                    }
                    iterator_stop(&it);
                    return;
                }
                else if (paf->where == where && paf->bitvector2 == vector2)
                {
                    switch (where)
                    {
                    case TO_AFFECTS:
                        SET_BIT(ch->affected_by[1],vector2);
                        break;
                    }
                    iterator_stop(&it);
                    return;
                }
            }
        }
        iterator_stop(&it);
    }
}


/*
 * Give an affect to a char.
 */
void affect_to_char(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_new;

    paf_new = new_affect();

    *paf_new		= *paf;
    VALIDATE(paf_new);	/* in case we missed it when we set up paf */

    /* Link affect to source token if present and valid */
    if (paf_new->token != NULL && IS_VALID(paf_new->token))
    list_appendlink(paf_new->token->affects, paf_new);
    else
    paf_new->token = NULL;  /* Clear uninitialized/invalid token pointer */

    paf_new->next	= ch->affected;
    ch->affected	= paf_new;

    affect_modify(ch, paf_new, true);
}


/* give an affect to an object */
void affect_to_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_new;
    int wear_loc;

    paf_new = new_affect();

    *paf_new		= *paf;

    VALIDATE(paf);	/* in case we missed it when we set up paf */

    /* Link affect to source token if present and valid */
    if (paf_new->token != NULL && IS_VALID(paf_new->token))
    list_appendlink(paf_new->token->affects, paf_new);
    else
    paf_new->token = NULL;  /* Clear uninitialized/invalid token pointer */

    paf_new->next	= obj->affected;
    obj->affected	= paf_new;

    if ((wear_loc = obj->wear_loc) != WEAR_NONE && obj->carried_by != NULL)
    affect_modify(obj->carried_by, paf_new, true);

    /* apply any affect vectors to the object's extra_flags */
    if (paf->bitvector)
    {
        switch (paf->where)
        {
        case TO_OBJECT:
            SET_BIT(obj->extra[0],paf->bitvector);
            break;
        case TO_OBJECT2:
            SET_BIT(obj->extra[1],paf->bitvector);
            break;
        case TO_OBJECT3:
            SET_BIT(obj->extra[2],paf->bitvector);
            break;
        case TO_OBJECT4:
            SET_BIT(obj->extra[3],paf->bitvector);
            break;
        case TO_WEAPON:
            if (obj->item_type == ITEM_WEAPON)
                SET_BIT(WEAPON(obj)->flags,paf->bitvector);
        break;
        }
    }
}

/* give an affect to an object */
void catalyst_to_obj(OBJ_DATA *obj, CATALYST_DATA *cat)
{
    CATALYST_DATA *cat_new, *existing;

    for(existing = obj->catalyst; existing; existing = existing->next) {
        if(existing->type == cat->type && existing->level == cat->level) {
            if(existing->modifier < 0 || cat->modifier < 0)
                existing->duration = existing->modifier = -1;
            else
                existing->duration += (cat->level * cat->modifier);
            return;
        }
    }

    cat_new = new_catalyst();

    *cat_new		= *cat;

    VALIDATE(cat);	/* in case we missed it when we set up cat */
    cat_new->next	= obj->catalyst;
    obj->catalyst	= cat_new;
    cat_new->duration = (cat_new->modifier > 0) ? (cat_new->level * cat_new->modifier) : -1;
}


/*
 * Remove an affect from a char.
 */
void affect_remove(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    if (ch->affected == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Affect_remove: no affect.");
        return;
    }

    affect_modify(ch, paf, false);

    if (paf == ch->affected)
        ch->affected = paf->next;
    else
    {
        AFFECT_DATA *prev;

        for (prev = ch->affected; prev != NULL; prev = prev->next)
        {
            if (prev->next == paf)
            {
                prev->next = paf->next;
                break;
            }
        }

        if (prev == NULL)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Affect_remove: cannot find paf.");
            return;
        }
    }

    /* Unlink from source token if present */
    if (IS_VALID(paf->token))
        list_remlink(paf->token->affects, paf, false);

    free_affect(paf);
    affect_fix_char(ch);
    return;
}

bool affect_removeall_obj(OBJ_DATA *obj)
{
    AFFECT_DATA *paf, *paf_next;
    bool is_worn = (obj->carried_by != NULL) && (obj->wear_loc != -1);

    for(paf = obj->affected; paf != NULL; paf = paf_next) {
        paf_next = paf->next;

        // If worn, remove this affect from the character, JIC
        if(is_worn) affect_modify(obj->carried_by, paf, false);

        if (paf->bitvector)
            switch(paf->where) {
            case TO_OBJECT:
                MERGE_BIT(obj->extra[0],obj->extra_perm[0],paf->bitvector);
                break;
            case TO_OBJECT2:
                MERGE_BIT(obj->extra[1],obj->extra_perm[1],paf->bitvector);
                break;
            case TO_OBJECT3:
                MERGE_BIT(obj->extra[2],obj->extra_perm[2],paf->bitvector);
                break;
            case TO_OBJECT4:
                MERGE_BIT(obj->extra[3],obj->extra_perm[3],paf->bitvector);
                break;
            case TO_WEAPON:
                if (obj->item_type == ITEM_WEAPON)
                    MERGE_BIT(WEAPON(obj)->flags,obj->weapon_flags_perm,paf->bitvector);
                break;
            }

        /* Unlink from source token if present */
        if (IS_VALID(paf->token))
            list_remlink(paf->token->affects, paf, false);

        free_affect(paf);
    }

    obj->affected = NULL;

    if(is_worn) affect_fix_char(obj->carried_by);

    return is_worn;
}


bool affect_remove_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    bool reset_ch = false;

    if (obj->affected == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Affect_remove_object: no affects on object.");
        return false;
    }

    if (obj->carried_by != NULL && obj->wear_loc != -1)
    {
        affect_modify(obj->carried_by, paf, false);
        reset_ch = true;
    }

    /* remove flags from the object if needed */
    if (paf->bitvector)
    {
        switch(paf->where)
        {
        case TO_OBJECT:
            MERGE_BIT(obj->extra[0],obj->extra_perm[0],paf->bitvector);
            break;
        case TO_OBJECT2:
            MERGE_BIT(obj->extra[1],obj->extra_perm[1],paf->bitvector);
            break;
        case TO_OBJECT3:
            MERGE_BIT(obj->extra[2],obj->extra_perm[2],paf->bitvector);
            break;
        case TO_OBJECT4:
            MERGE_BIT(obj->extra[3],obj->extra_perm[3],paf->bitvector);
            break;
        case TO_WEAPON:
            if (obj->item_type == ITEM_WEAPON)
                MERGE_BIT(WEAPON(obj)->flags,obj->weapon_flags_perm,paf->bitvector);
            break;
        }
    }

    if (paf == obj->affected)
        obj->affected = paf->next;
    else
    {
        AFFECT_DATA *prev;

        for (prev = obj->affected; prev != NULL; prev = prev->next)
        {
            if (prev->next == paf)
            {
                prev->next = paf->next;
                break;
            }
        }

        if (prev == NULL)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Affect_remove_object: cannot find paf.");
            return reset_ch;
        }
    }

    /* Unlink from source token if present */
    if (IS_VALID(paf->token))
        list_remlink(paf->token->affects, paf, false);

    free_affect(paf);

    if (obj->carried_by != NULL && obj->wear_loc != -1)
    {
        affect_fix_char(obj->carried_by);
    }

    return reset_ch;
}


/*
 * Strip all affects of a given sn.
 */
void affect_strip(CHAR_DATA *ch, int sn)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;
    SKILL_DATA *skill = skill_find_uid(sn);

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
    paf_next = paf->next;
    if (affect_matches_skill_sn(paf, sn, skill))
        affect_remove(ch, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_name(CHAR_DATA *ch, char *name)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
    paf_next = paf->next;
    if (paf->custom_name == name)
        affect_remove(ch, paf);
    }
}

void affect_stripall_wearloc(CHAR_DATA *ch, int wear_loc)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    if( wear_loc == WEAR_NONE ) return;

    for (paf = ch->affected; paf != NULL; paf = paf_next)
    {
        paf_next = paf->next;
        if (paf->slot == wear_loc)
            affect_remove(ch, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_obj(OBJ_DATA *obj, int sn)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;
    SKILL_DATA *skill = skill_find_uid(sn);

    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
    paf_next = paf->next;
    if (affect_matches_skill_sn(paf, sn, skill))
        affect_remove_obj(obj, paf);
    }
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip_name_obj(OBJ_DATA *obj, char *name)
{
    AFFECT_DATA *paf;
    AFFECT_DATA *paf_next;

    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
    paf_next = paf->next;
    if (paf->custom_name == name)
        affect_remove_obj(obj, paf);
    }
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected(CHAR_DATA *ch, int sn)
{
    AFFECT_DATA *paf;
    SKILL_DATA *skill = skill_find_uid(sn);

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
    if (!paf->custom_name && affect_matches_skill_sn(paf, sn, skill))
        return true;
    }

    return false;
}

/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_name(CHAR_DATA *ch, char *name)
{
    AFFECT_DATA *paf;

    for (paf = ch->affected; paf != NULL; paf = paf->next)
    {
    if (paf->custom_name && paf->custom_name == name)
        return true;
    }

    return false;
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_obj(OBJ_DATA *obj, int sn)
{
    AFFECT_DATA *paf;

    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
    if (!paf->custom_name && paf->type == sn)
        return true;
    }

    return false;
}


/*
 * Return true if a char is affected by a spell.
 */
bool is_affected_name_obj(OBJ_DATA *obj, char *name)
{
    AFFECT_DATA *paf;

    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
    if (paf->custom_name && paf->custom_name == name)
        return true;
    }

    return false;
}


/*
 * Add or enhance an affect.
 */
void affect_join(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

    if(paf->custom_name) {
        for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (paf_old->custom_name && paf_old->custom_name == paf->custom_name) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove(ch, paf_old);
                break;
            }
        }
    } else {
        for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (!paf_old->custom_name && paf_old->type == paf->type) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove(ch, paf_old);
                break;
            }
        }
    }


    affect_to_char(ch, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_full(CHAR_DATA *ch, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

    if(paf->custom_name) {
        for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (paf_old->custom_name && paf_old->custom_name == paf->custom_name &&
                paf_old->location == paf->location &&
                paf_old->bitvector == paf->bitvector &&
                paf_old->bitvector2 == paf->bitvector2) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove(ch, paf_old);
                break;
            }
        }
    } else {
        for (paf_old = ch->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (!paf_old->custom_name && paf_old->type == paf->type &&
                paf_old->location == paf->location &&
                paf_old->bitvector == paf->bitvector &&
                paf_old->bitvector2 == paf->bitvector2) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove(ch, paf_old);
                break;
            }
        }
    }


    affect_to_char(ch, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

    if(paf->custom_name) {
        for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (paf_old->custom_name && paf_old->custom_name == paf->custom_name) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove_obj(obj, paf_old);
                break;
            }
        }
    } else {
        for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (!paf_old->custom_name && paf_old->type == paf->type) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove_obj(obj, paf_old);
                break;
            }
        }
    }

    affect_to_obj(obj, paf);
    return;
}

/*
 * Add or enhance an affect.
 */
void affect_join_full_obj(OBJ_DATA *obj, AFFECT_DATA *paf)
{
    AFFECT_DATA *paf_old;

    if(paf->custom_name) {
        for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (paf_old->custom_name && paf_old->custom_name == paf->custom_name &&
                paf_old->location == paf->location &&
                paf_old->bitvector == paf->bitvector &&
                paf_old->bitvector2 == paf->bitvector2) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove_obj(obj, paf_old);
                break;
            }
        }
    } else {
        for (paf_old = obj->affected; paf_old != NULL; paf_old = paf_old->next) {
            if (!paf_old->custom_name && paf_old->type == paf->type &&
                paf_old->location == paf->location &&
                paf_old->bitvector == paf->bitvector &&
                paf_old->bitvector2 == paf->bitvector2) {
                paf->level = (paf->level + paf_old->level) / 2;
                paf->duration += paf_old->duration;
                paf->modifier += paf_old->modifier;
                affect_remove_obj(obj, paf_old);
                break;
            }
        }
    }

    affect_to_obj(obj, paf);
    return;
}


/*
 * Move a char out of a room.
 */
void char_from_room(CHAR_DATA *ch)
{
    OBJ_DATA *obj, *obj_next;

    if (ch->in_room == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Char_from_room: NULL.");
    return;
    }

    if( IS_VALID(ch->in_room->instance_section) && IS_VALID(ch->in_room->instance_section->instance) )
    {
        INSTANCE *instance = ch->in_room->instance_section->instance;
        DUNGEON *dungeon = instance->dungeon;
        if( !IS_NPC(ch) || !IS_SET(ch->act[1], ACT2_INSTANCE_MOB) )
        {

            if( IS_VALID(dungeon) )
            {
                if( !IS_NPC(ch) )
                    list_remlink(dungeon->players, ch, false);
                list_remlink(dungeon->mobiles, ch, false);
            }

            if( !IS_NPC(ch) )
                list_remlink(instance->players, ch, false);
            list_remlink(instance->mobiles, ch, false);
        }
        else if( IS_BOSS(ch) )
        {
            if( IS_VALID(dungeon) )
                list_remlink(dungeon->bosses, ch, false);
            list_remlink(instance->bosses, ch, false);
        }
    }
    else
    {
        if (!IS_NPC(ch))
        {
            --ch->in_room->area->nplayer;
            if (ch->in_wilds)
            {
                --ch->in_wilds->nplayer;
            }
        }

    }


    if (ch->in_room->chat_room != NULL)
    ch->in_room->chat_room->curr_people--;

    if (MOUNTED(ch) && MOUNTED(ch)->in_room == ch->in_room
    && MOUNTED(ch)->in_room != NULL)
        char_from_room(MOUNTED(ch));

    if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
    &&   obj->item_type == ITEM_LIGHT
    &&   LIGHT(obj)->duration != 0
    &&   ch->in_room->light > 0)
    --ch->in_room->light;

    if (ch == ch->in_room->people)
    ch->in_room->people = ch->next_in_room;
    else
    {
        CHAR_DATA *prev;

        for (prev = ch->in_room->people; prev; prev = prev->next_in_room)
        {
            if (prev->next_in_room == ch)
            {
            prev->next_in_room = ch->next_in_room;
            break;
            }
        }

        if (prev == NULL)
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                "Char_from_room: %s not found in room %ld people list! "
                "Character may have been in multiple rooms or removed twice.",
                IS_NPC(ch) ? ch->short_descr : ch->name,
                ch->in_room ? ch->in_room->vnum : 0);
    }

    list_remlink(ch->in_room->lpeople, ch, false);
    list_remlink(ch->in_room->lentity, ch, false);

    if (ch->in_wilds)
    {
        if (!ch->in_room->people && !ch->in_room->contents)
            destroy_wilds_vroom(ch->in_room);
    }

    ch->in_room = NULL;
    ch->in_wilds = NULL;        /* Vizz - wilds */
    ch->at_wilds_x = 0;
    ch->at_wilds_y = 0;
    ch->next_in_room = NULL;
    ch->on = NULL;                /* sanity check! */

    // Make sure mail is only done in post offices

    if (ch->mail != NULL)
    {
    for (obj = ch->mail->objects; obj != NULL; obj = obj_next) {
        obj_next = obj->next_content;

        obj_from_mail(obj);
        obj_to_char(obj, ch);
    }

    free_mail(ch->mail);
    ch->mail = NULL;
    send_to_char("Mail order cancelled.\n\r", ch);
    }

    return;
}

/*
 * Move a char into a room.
 */
void char_to_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
    OBJ_DATA *obj;

    if (pRoomIndex == NULL)
    {
    ROOM_INDEX_DATA *room;

    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Char_to_room: destination room NULL.");

    if ((room = get_reserved_room_index("room_default_recall")) != NULL)
        char_to_room(ch,room);

    return;
    }

    /* Safety check: if the character is already on a room's people list,
     * remove them first. This prevents dual-room corruption where a
     * character ends up on two rooms' people lists simultaneously.
     * This can happen when char_to_room is called without char_from_room
     * (e.g., during login when in_room is pre-set from save data, or
     * via char_to_vroom which bypasses char_from_room). */
    if (ch->in_room != NULL)
    {
        /* Check if ch is actually linked into the old room's people list */
        CHAR_DATA *scan;
        bool on_list = false;
        for (scan = ch->in_room->people; scan; scan = scan->next_in_room)
        {
            if (scan == ch) { on_list = true; break; }
        }
        if (on_list)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                "Char_to_room: %s already on people list of room %ld, removing first.",
                IS_NPC(ch) ? ch->short_descr : ch->name,
                ch->in_room->vnum);
            char_from_room(ch);
        }
    }

    if (MOUNTED(ch) && MOUNTED(ch)->in_room == ch->in_room
    && MOUNTED(ch)->in_room == NULL)
    char_to_room(MOUNTED(ch), pRoomIndex);

    ch->in_room		= pRoomIndex;

// VIZZWILDS - Check room's wilds pointer
    if (pRoomIndex->wilds)
        ch->in_wilds = pRoomIndex->wilds;

    ch->next_in_room	= pRoomIndex->people;
    pRoomIndex->people	= ch;

    list_addlink(pRoomIndex->lpeople, ch);
    list_addlink(pRoomIndex->lentity, ch);

    // Prevent catastrophes
    if (ch->next_in_room == ch)
    {
        pbugf(LOG_INFO, "[SERIOUS!] error! char %s (vnum %ld)'s next_in_room is itself!\n",
         IS_NPC(ch) ? ch->short_descr : ch->name,
         IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        extract_char(ch, false);
    return;
    }

    if (ch->next == ch)
    {
        pbugf(LOG_INFO, "[SERIOUS!] error! char %s (vnum %ld)'s next is itself!\n",
         IS_NPC(ch) ? ch->short_descr : ch->name,
         IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        extract_char(ch, false);
    return;
    }

    if (ch->in_room->chat_room != NULL)
    ch->in_room->chat_room->curr_people++;

/* Phased out in favour of an AREA_SOCIAL flag on social zones. 
    if (!str_cmp(ch->in_room->area->name, "Elysium")
    && !IS_SOCIAL(ch))
        SET_BIT(ch->comm, COMM_SOCIAL);

    if (str_cmp(ch->in_room->area->name, "Elysium")
    && IS_SOCIAL(ch))
        REMOVE_BIT(ch->comm, COMM_SOCIAL);
*/

    if( IS_VALID(pRoomIndex->instance_section) && IS_VALID(pRoomIndex->instance_section->instance) )
    {
        INSTANCE *instance = pRoomIndex->instance_section->instance;
        DUNGEON *dungeon = instance->dungeon;

        if( !IS_NPC(ch) || !IS_SET(ch->act[1], ACT2_INSTANCE_MOB) )
        {
            if( IS_VALID(dungeon) )
            {
                if( !IS_NPC(ch) )
                    list_appendlink(dungeon->players, ch);
                list_appendlink(dungeon->mobiles, ch);
            }

            if( !IS_NPC(ch) )
                list_appendlink(instance->players, ch);
            list_appendlink(instance->mobiles, ch);
        }
        else if( IS_BOSS(ch) )
        {
            if( IS_VALID(dungeon) )
                list_appendlink(dungeon->bosses, ch);
            list_appendlink(instance->bosses, ch);
        }
    }
    else
    {
        if (!IS_NPC(ch))
        {
            if (ch->in_room->area->empty)
            {
                ch->in_room->area->empty = false;
                ch->in_room->area->age = 0;
            }

            ++ch->in_room->area->nplayer;
        // VIZZWILDS - Check char's wilds pointer
            if (ch->in_wilds)
            {
                //plogf(LOG_INFO, "handler.c, char_to_room(): %s is entering a wilds area.", ch->name);

                if (ch->in_wilds->empty)
                {
                    ch->in_wilds->empty = false;
                    ch->in_wilds->age = 0;
                }

                ++ch->in_wilds->nplayer;
            }
        }
    }

    if ((obj = get_eq_char(ch, WEAR_LIGHT)) != NULL
    &&   obj->item_type == ITEM_LIGHT
    &&   LIGHT(obj)->duration != 0)
    ++ch->in_room->light;

    // Spread plague
    if (IS_AFFECTED(ch,AFF_PLAGUE))
    {
        AFFECT_DATA *af, plague;
        bool has_plague_af = false;	// @@@NIB : 20070127 : handle special cases
        CHAR_DATA *vch;

        for (af = ch->affected; af != NULL; af = af->next)
        {
            if (af->type == skill_resolve_gsn("plague"))
                break;
        // @@@NIB : 20070127 : handle special cases
        //	So far, only toxic fumes does 'plague' too
            if (af->type == skill_resolve_gsn("toxic fumes"))
                has_plague_af = true;
        }

        if (af == NULL)
        {
            if(!has_plague_af) REMOVE_BIT(ch->affected_by[0],AFF_PLAGUE);
            return;
        }

        if (af->level == 1)
            return;

        plague.where		= TO_AFFECTS;
        plague.group		= AFFGROUP_BIOLOGICAL;
        plague.type 		= skill_resolve_gsn("plague");
        plague.skill		= skill_find_uid(plague.type);
        plague.level 		= af->level - 1;
        plague.duration 	= number_range(1,2 * plague.level);
        plague.location		= APPLY_STR;
        plague.modifier 	= -5;
        plague.bitvector 	= AFF_PLAGUE;
        plague.bitvector2	= 0;
        plague.slot			= WEAR_NONE;
        plague.custom_name = NULL;


        for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
        {
            if (!saves_spell(plague.level - 2,vch,DAM_DISEASE)
        &&  !IS_IMMORTAL(vch) &&
                !IS_AFFECTED(vch,AFF_PLAGUE) && number_bits(6) == 0)
            {
                send_to_char("You feel hot and feverish.\n\r",vch);
                act("$n shivers and looks very ill.",vch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
                affect_join(vch,&plague);
            }
        }
    }

    if (ch->quest != NULL)
    check_quest_travel_room(ch, pRoomIndex, true);

    return;
}


/*
 * Give an obj to a locker.
 */
void obj_to_locker(OBJ_DATA *obj, CHAR_DATA *ch)
{
    // MIGRATION: Moving from old linked-list to new LLIST system
    // Keep old system for backward compatibility during transition
    obj->next_content    = ch->locker;
    ch->locker           = obj;

    obj->carried_by      = ch;
    obj->in_room         = NULL;
    obj->in_obj          = NULL;
    obj->locker	 	 = true;

    obj->pIndexData->lockered++;

    // New LLIST system - this is the primary storage now
    list_addlink(ch->llocker, obj);

}


/*
 * Give an obj to a char.
 */
void obj_to_char(OBJ_DATA *obj, CHAR_DATA *ch)
{
    if (obj == NULL || ch == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "obj_to_char: null obj or ch");
        return;
    }

    obj->carried_by     = ch;
    obj->in_room        = ch->in_room;
    obj->in_obj         = NULL;
    obj->locker         = false;

    // Remove hidden flag when picking up objects
    if (IS_SET(obj->extra[0], ITEM_HIDDEN))
        REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);

    if (!IS_NPC(ch) && IS_SET(obj->extra[1], ITEM_KEY_ITEM))
        obj->stached = true;

    // Convert money obj into gold/silver
    if (obj->item_type == ITEM_MONEY)
    {
        ch->silver += MONEY(obj)->silver;
        ch->gold += MONEY(obj)->gold;

        // AUTOSPLIT
        if (IS_SET(ch->act[0], PLR_AUTOSPLIT))
        {
            int members;
            CHAR_DATA *gch;
            char buffer[MAX_STRING_LENGTH];

            members = 0;
            for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
            {
                if (gch->pcdata != NULL && is_same_group(gch, ch))
                    members++;
            }

            if (members > 1 && (MONEY(obj)->silver > 1 || MONEY(obj)->gold))
            {
                sprintf(buffer, "%d %d", MONEY(obj)->silver, MONEY(obj)->gold);
                do_function(ch, &do_split, buffer);
            }
        }

        extract_obj(obj);
        return;
    }

    if (obj->stached)
    {
        if (!list_haslink(ch->lstache, obj))
            list_addlink(ch->lstache, obj);
        return;
    }

    // Update character stats (non-stached inventory only)
    ch->carry_number    += get_obj_number(obj);
    ch->carry_weight    += get_obj_weight(obj);

    // Add to the LLIST only if it's not equipped (wear_loc == WEAR_NONE)
    // This ensures lcarrying only contains objects in the active inventory
    if (obj->wear_loc == WEAR_NONE && !list_haslink(ch->lcarrying, obj)) {
        list_addlink(ch->lcarrying, obj);
    }

    if (!IS_NPC(ch))
        check_quest_retrieve_obj(ch, obj, true);

    obj->pIndexData->carried++;

    if (objRepop == true)
    {
        p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
        objRepop = false;
    }
}

/*
 * Count objs in a characters locker
 */
int count_char_locker(CHAR_DATA *ch)
{
    OBJ_DATA *prev;
    int counter;

    counter = 0;

    if (ch == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "handler.c, count_char_locker: NULL ch.");
    return -1;
    }

    for (prev = ch->locker; prev != NULL; prev = prev->next_content)
        counter++;

    return counter;
}


/*
 * Take an obj from its characters locker.
 */
void obj_from_locker(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if ((ch = obj->carried_by) == NULL)
    {
log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Obj_from_char: null ch.");
    return;
    }

    if (ch->locker == obj)
        ch->locker = obj->next_content;
    else
    {
        OBJ_DATA *prev;

        for (prev = ch->locker; prev != NULL; prev = prev->next_content)
        {
            if (prev->next_content == obj)
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if (prev == NULL)
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "locker get: obj not in list.");
    }

    --obj->pIndexData->lockered;

    obj->in_room         = NULL;
    obj->carried_by      = NULL;
    obj->next_content    = NULL;
    obj->locker	 	 = false;

    list_remlink(ch->llocker, obj, false);
}


/*
 * Take an obj from its character.
 */
void obj_from_char(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if ((ch = obj->carried_by) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Obj_from_char: null ch.");
        return;
    }

    /* Unequip it first */
    if (obj->wear_loc != WEAR_NONE)
        unequip_char(ch, obj, false);

    REMOVE_BIT(obj->extra[0], ITEM_INVENTORY);
    obj->carried_by = NULL;
    obj->next_content = NULL;

    if (obj->stached)
    {
        list_remlink(ch->lstache, obj, false);
        return;
    }

    --obj->pIndexData->carried;
    ch->carry_number -= get_obj_number(obj);
    ch->carry_weight -= get_obj_weight(obj);

    /* Remove from the LLIST */
    list_remlink(ch->lcarrying, obj, false);
}


/*
 * Find the ac value of an obj, including position effect.
 */
int apply_ac(OBJ_DATA *obj, int iWear, int type)
{
    if (obj->item_type != ITEM_ARMOUR)
    return 0;

    switch (iWear)
    {
    case WEAR_BODY:		return 3 * ARMOR(obj)->protection[type];
    case WEAR_HEAD:		return 3 * ARMOR(obj)->protection[type];
    case WEAR_LEGS:		return 2 * ARMOR(obj)->protection[type];
    case WEAR_FEET:		return ARMOR(obj)->protection[type];
    case WEAR_HANDS: 	return 2 * ARMOR(obj)->protection[type];
    case WEAR_ARMS:		return 2 * ARMOR(obj)->protection[type];
    case WEAR_SHIELD: 	return 3 * ARMOR(obj)->protection[type];
    case WEAR_NECK_1: 	return 3 * ARMOR(obj)->protection[type];
    case WEAR_NECK_2: 	return 3 * ARMOR(obj)->protection[type];
    case WEAR_ABOUT: 	return ARMOR(obj)->protection[type];
    case WEAR_WAIST: 	return ARMOR(obj)->protection[type];
    case WEAR_FINGER_R: 	return ARMOR(obj)->protection[type];
    case WEAR_FINGER_L: 	return ARMOR(obj)->protection[type];
    case WEAR_WRIST_L: 	return ARMOR(obj)->protection[type];
    case WEAR_WRIST_R: 	return ARMOR(obj)->protection[type];
    case WEAR_HOLD:		return 2 * ARMOR(obj)->protection[type];
    }

    return 0;
}


/*
 * Find a piece of eq on a character.
 */
OBJ_DATA *get_eq_char(CHAR_DATA *ch, int iWear)
{
    OBJ_DATA *obj;
    ITERATOR it;
    
    if (ch == NULL)
        return NULL;
    
    // Use the lworn LLIST for better performance
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->wear_loc == iWear) {
                iterator_stop(&it);
                return obj;
            }
        }
        iterator_stop(&it);
        return NULL;
    }
    
    return NULL;
}


/*
 * Equip a char with an obj.
 */
void equip_char(CHAR_DATA *ch, OBJ_DATA *obj, int iWear)
{
    AFFECT_DATA *paf;
    SPELL_DATA *spell;
    int i;

    if (get_eq_char(ch, iWear) != NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Equip_char: already equipped (%d).", iWear);
    return;
    }

    // This is generally already handled in wear_obj, but just to be safe, doing the all_remort check here as well.
    if (!IS_IMMORTAL(ch) && !IS_NPC(ch)) {
        /* If the object is not a mortal object
        -or- is higher object level and the item is not flagged all_remort or the char is not remort */
        if ((obj->level > LEVEL_HERO) ||
        ((ch->tot_level < obj->level) && !(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)))) {
            return;
        }
    }

    if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL)    && IS_EVIL(ch)   )
    ||   (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD)    && IS_GOOD(ch)   )
    ||   (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch)))
    {
    act("You are zapped by $p and drop it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n is zapped by $p and drops it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        REMOVE_BIT(obj->extra[1], ITEM_KEPT);

    obj_from_char(obj);
    obj_to_room(obj, ch->in_room);
    return;
    }

    obj->wear_loc = iWear;
    
    // Add to lworn list
    list_addlink(ch->lworn, obj);
    
    // Remove from lcarrying list since it's now worn
    list_remlink(ch->lcarrying, obj, false);

    /* Wear trigger */
    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WEAR, NULL);

    // Concealed items do nothing to the wearer's stats and affects.
    if(wear_params[iWear][WEAR_PARAM_AFFECTS]) {
        /* apply armour class */
        for (i = 0; i < 4; i++)
        ch->armour[i] -= apply_ac(obj, iWear, i);

        /* put obj's affects on the character */
        for (paf = obj->affected; paf != NULL; paf = paf->next) {
            paf->slot = iWear;
            affect_modify(ch, paf, true);
        }

        /* set light in room if it's a light */
        if (obj->item_type == ITEM_LIGHT
        &&   ch->in_room != NULL)
        {
        // hack to fix current lights with 0 light remaining
        if (LIGHT(obj)->duration == 0)
            LIGHT(obj)->duration = 10;

        ++ch->in_room->light;
        }

        if (obj->item_type != ITEM_WAND
        &&  obj->item_type != ITEM_STAFF
        &&  obj->item_type != ITEM_SCROLL
        &&  obj->item_type != ITEM_POTION
        &&  obj->item_type != ITEM_TATTOO
        &&  obj->item_type != ITEM_PILL)
        for (spell = obj->spells; spell != NULL; spell = spell->next)
        {
            for (paf = ch->affected; paf != NULL; paf = paf->next)
            {
                if (paf->type == spell->sn)
                break;
            }

            if (paf != NULL && paf->level >= spell->level)
                continue;

            affect_strip(ch, spell->sn);
            obj_cast_spell(spell->sn, spell->level + MAGIC_WEAR_SPELL, ch, ch, obj);
        }
    }
}


/*
 * Unequip a char with an obj.
 */
int unequip_char(CHAR_DATA *ch, OBJ_DATA *obj, bool show)
{
    AFFECT_DATA *paf = NULL;
    int i, loc = obj->wear_loc;

    if (obj->wear_loc == WEAR_NONE)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Unequip_char: already unequipped.");
        return false;    // @@@NIB : 20070128
    }

    obj->wear_loc = WEAR_NONE;
    list_remlink(ch->lworn, obj, false);
    
    // Add back to lcarrying since it's no longer worn
    list_addlink(ch->lcarrying, obj);

    // If the item was concealed, don't handle any object affects.
    if(wear_params[loc][WEAR_PARAM_AFFECTS]) {
        for (i = 0; i < 4; i++)
            ch->armour[i] += apply_ac(obj, loc, i);

        for (paf = obj->affected; paf != NULL; paf = paf->next)
        {
            affect_modify(ch, paf, false);
        }

        if (obj->item_type == ITEM_LIGHT &&
            LIGHT(obj)->duration != 0 && ch->in_room != NULL &&
            ch->in_room->light > 0)
            --ch->in_room->light;

        // Remove spells
        if (obj->item_type != ITEM_WAND
        &&  obj->item_type != ITEM_STAFF
        &&  obj->item_type != ITEM_SCROLL
        &&  obj->item_type != ITEM_POTION
        &&  obj->item_type != ITEM_TATTOO
        &&  obj->item_type != ITEM_PILL)
        for (SPELL_DATA *spell = obj->spells; spell != NULL; spell = spell->next)
        {
            int spell_level = spell->level;
            AFFECT_DATA *af;

            // Find the first affect that matches this spell and is derived from the object
            for (af = ch->affected; af != NULL; af = af->next)
            {
                if (af->type == spell->sn && af->slot == loc)
                    break;
            }

            if(!af) {
                // This spell was not applied by this object
                continue;
            }

            // @@@NIB : 20070128 : this entire block did not account for the
            //    possibility of multiple spells active for the object.
            //    Once it found *one* spell, it returned...
            //    This also bypassed the remove trigger
            bool found = false;
            int level = 0;
            int found_loc = WEAR_NONE;

            // If there's another obj with the same spell put that one on
            for (OBJ_DATA *obj_tmp = NULL; obj_tmp != NULL; obj_tmp = obj_tmp->next_content)
            {
                if (obj_tmp->wear_loc != WEAR_NONE && obj != obj_tmp) {
                    for (SPELL_DATA *spell_tmp = obj_tmp->spells; spell_tmp != NULL; spell_tmp = spell_tmp->next) {
                        if (spell_tmp->sn == spell->sn && spell_tmp->level > level ) {
                            level = spell_tmp->level;    // Keep the maximum
                            found_loc = obj_tmp->wear_loc;
                            found = true;
                        }
                    }
                }
            }

            if(!found) {
                // No other worn object had this spell available
                if (show) {
                    if (skill_table[spell->sn].msg_off) {
                        send_to_char(skill_table[spell->sn].msg_off, ch);
                        send_to_char("\n\r", ch);
                    }
                }

                affect_strip(ch, spell->sn);
            } else if (level > spell_level) {
                level -= spell_level;        // Get the difference

                // Update all affects to the current maximum and its slot
                for(; af; af = af->next) {
                    af->level += level;
                    af->slot = found_loc;
                }
            }
            // @@@NIB : 20070128
        }

        affect_stripall_wearloc(ch, loc);    // Remove all affects tied to this wear slot

        affect_fix_char(ch);
    }

    /* Remove trigger */
    return (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REMOVE, NULL));
}


/*
 * Count occurrences of an obj in a list.
 */
int count_obj_list(OBJ_INDEX_DATA *pObjIndex, OBJ_DATA *list)
{
    OBJ_DATA *obj;
    int nMatch;

    nMatch = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == pObjIndex)
        nMatch++;
    }

    return nMatch;
}


/*
 * Move an obj out of a room.
 */
void obj_from_room(OBJ_DATA *obj)
{
    ROOM_INDEX_DATA *in_room;
    CHAR_DATA *ch;
    char buf[MAX_STRING_LENGTH];

    if ((in_room = obj->in_room) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "obj_from_room: NULL.");
        return;
    }

    for (ch = in_room->people; ch != NULL; ch = ch->next_in_room)
        if (ch->on == obj)
            ch->on = NULL;

    list_remlink(in_room->lcontents, obj, false);
    list_remlink(in_room->lentity, obj, false);

    if (obj == in_room->contents)
        in_room->contents = obj->next_content;
    else
    {
        OBJ_DATA *prev;

        for (prev = in_room->contents; prev; prev = prev->next_content)
        {
            if (prev->next_content == obj)
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if (prev == NULL)
        {
            sprintf(buf, "Obj_from_room: obj not found.");
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
            return;
        }
    }

    if( IS_VALID(obj->in_room->instance_section) && IS_VALID(obj->in_room->instance_section->instance) )
    {
        if( !IS_SET(obj->extra[2], ITEM_INSTANCE_OBJ) )
        {
            INSTANCE *instance = obj->in_room->instance_section->instance;
            DUNGEON *dungeon = instance->dungeon;

            if( IS_VALID(dungeon) )
            {
                list_appendlink(dungeon->objects, obj);
            }
        }
    }


    if (obj->in_wilds)
    {
        if (!obj->in_room->people && !obj->in_room->contents)
            destroy_wilds_vroom(obj->in_room);

        obj->in_wilds->loaded_objs--;
    }

    --obj->pIndexData->inrooms;

    obj->in_wilds     = NULL;
    obj->in_room      = NULL;
    obj->next_content = NULL;
}


/*
 * Move an obj into a room.
 */
void obj_to_room(OBJ_DATA *obj, ROOM_INDEX_DATA *pRoomIndex)
{
    /* Safety check: if the object is already in a room's contents list,
     * remove it first. This prevents dual-room corruption where an
     * object ends up on two rooms' contents lists simultaneously. */
    if (obj->in_room != NULL)
    {
        OBJ_DATA *scan;
        bool on_list = false;
        for (scan = obj->in_room->contents; scan; scan = scan->next_content)
        {
            if (scan == obj) { on_list = true; break; }
        }
        if (on_list)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                "obj_to_room: obj '%s' (vnum %ld) already on contents list of room %ld, removing first.",
                obj->short_descr, obj->pIndexData->vnum, obj->in_room->vnum);
            obj_from_room(obj);
        }
    }

    obj->next_content		= pRoomIndex->contents;
    pRoomIndex->contents	= obj;
    obj->in_room		= pRoomIndex;
    obj->carried_by		= NULL;
    obj->in_obj			= NULL;

    list_addlink(pRoomIndex->lcontents, obj);
    list_addlink(pRoomIndex->lentity, obj);

        // Vroom
    if( pRoomIndex->wilds )
    {
        pRoomIndex->wilds->loaded_objs++;
        obj->in_wilds = pRoomIndex->wilds;
    }
    else if( IS_VALID(pRoomIndex->instance_section) && IS_VALID(pRoomIndex->instance_section->instance) )
    {
        if( !IS_SET(obj->extra[2], ITEM_INSTANCE_OBJ) )
        {
            INSTANCE *instance = pRoomIndex->instance_section->instance;
            DUNGEON *dungeon = instance->dungeon;

            if( IS_VALID(dungeon) )
            {
                list_appendlink(dungeon->objects, obj);
            }
        }
    }

    obj->pIndexData->inrooms++;

    if (objRepop == true)
    {
        p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
        objRepop = false;
    }
}

void obj_to_vroom(OBJ_DATA *obj, WILDS_DATA *pWilds, int x, int y)
{
    ROOM_INDEX_DATA *pWildsRoom = get_wilds_vroom(pWilds, x, y);
    if (!pWildsRoom)
        pWildsRoom = create_wilds_vroom(pWilds, x, y);
    if (pWildsRoom) {
        /* Safety check: if the object is already in a room's contents list,
         * remove it first to prevent dual-room corruption. */
        if (obj->in_room != NULL)
        {
            OBJ_DATA *scan;
            bool on_list = false;
            for (scan = obj->in_room->contents; scan; scan = scan->next_content)
            {
                if (scan == obj) { on_list = true; break; }
            }
            if (on_list)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                    "obj_to_vroom: obj '%s' (vnum %ld) already on contents list of room %ld, removing first.",
                    obj->short_descr, obj->pIndexData->vnum, obj->in_room->vnum);
                obj_from_room(obj);
            }
        }

        obj->in_wilds = pWilds;
        obj->in_room = pWildsRoom;
        obj->carried_by = NULL;
        obj->in_obj = NULL;
        obj->next_content = pWildsRoom->contents;
        pWildsRoom->contents = obj;
        list_addlink(pWildsRoom->lcontents, obj);
        list_addlink(pWildsRoom->lentity, obj);
        pWilds->loaded_objs++;
        obj->x = x;
        obj->y = y;
        obj->pIndexData->inrooms++;
        if (objRepop == true) {
            p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
            objRepop = false;
        }
    }
}


/*
 * Move an object into an object.
 */
void obj_to_obj(OBJ_DATA *obj, OBJ_DATA *obj_to)
{
    if (obj_to->carried_by != NULL)
        obj_to->carried_by->carry_weight -= get_obj_weight(obj_to);

    obj->next_content		= obj_to->contains;
    obj_to->contains		= obj;
    obj->in_obj			= obj_to;
    obj->in_room		= obj_to->in_room;
    obj->carried_by		= NULL;

    if (obj_to->carried_by != NULL)
        obj_to->carried_by->carry_weight += get_obj_weight(obj_to);

    if(obj->clone_rooms || obj->nest_clones > 0)
        obj_set_nest_clones(obj_to,true);

    obj->pIndexData->incontainer++;

}


/*
 * Move an object out of an object.
 */
void obj_from_obj(OBJ_DATA *obj)
{
    OBJ_DATA *obj_from;

    if ((obj_from = obj->in_obj) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Obj_from_obj: null obj_from.");
        return;
    }

    if (obj_from->carried_by != NULL)
        obj_from->carried_by->carry_weight -= get_obj_weight(obj_from);

    if (obj == obj_from->contains)
        obj_from->contains = obj->next_content;
    else
    {
        OBJ_DATA *prev;

        for (prev = obj_from->contains; prev; prev = prev->next_content)
        {
            if (prev->next_content == obj)
            {
                prev->next_content = obj->next_content;
                break;
            }
        }

        if (prev == NULL)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Obj_from_obj: obj not found.");
            return;
        }
    }

    --obj->pIndexData->incontainer;

    obj->in_room      = NULL;
    obj->next_content = NULL;
    obj->in_obj       = NULL;

    if (obj_from->carried_by != NULL)
        obj_from->carried_by->carry_weight += get_obj_weight(obj_from);

    if(obj->clone_rooms || obj->nest_clones > 0)
        obj_set_nest_clones(obj_from,false);

    return;
}


/*
 * Extract a chat room from the world.
 */
void extract_chat_room(CHAT_ROOM_DATA *chat)
{
    CHAT_ROOM_DATA *temp_chat;
    CHAT_ROOM_DATA *prev_chat = NULL;

    if (chat == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Tried to extract null chat.");
    return;
    }

    for (temp_chat = chat_room_list; temp_chat != NULL; temp_chat = temp_chat->next)
    {
        if (!str_cmp(chat->name, temp_chat->name)) break;

    prev_chat = temp_chat;
    }

    if (temp_chat == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Couldn't extract chat as chat was NULL.");
    return;
    }

    if (prev_chat == NULL)
    {
    chat_room_list = chat->next;
    }
    else
    {
    prev_chat->next = chat->next;
    }

    if (temp_chat == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Extract_chat: chat not found.");
    return;
    }

    free_chat_room(chat);
    return;
}


/*
 * Extract a church from the world.
 */
void extract_church(CHURCH_DATA *church)
{
    char buf[MAX_STRING_LENGTH];
    CHURCH_PLAYER_DATA *member;
    CHURCH_PLAYER_DATA *iter;

    if (church == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Tried to extract null church.");
        return;
    }

    for (iter = church->people; iter != NULL; iter = iter->next)
    {
        if (iter->ch != NULL)
            quest_runtime_remove_church_runs(iter->ch, church->uid);
    }

    // Remove all members
    while((member = church->people) != NULL)
        remove_member(member);

    sprintf(buf, "{Y[%s has been disbanded.]{x\n\r", church->name);
    gecho(buf);

    // Remove from LLIST
    list_remlink(list_churches, church, false);

    // Optionally, if you still maintain church_list as a view, rebuild it here:
    // rebuild_church_list_from_llist();

    free_church(church);
    return;
}

/*
 * Extract an obj from the world.
 */
void extract_obj(OBJ_DATA *obj)
{
    //char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj_content;
    OBJ_DATA *obj_next;
    ROOM_INDEX_DATA *clone, *next_clone;
    ITERATOR it;
    INSTANCE *instance;


    if (obj == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Tried to extract null object.");
    return;
    }

    if (obj->gc || list_hasdata(gc_objects, obj))
        return;

    if(obj->progs) {
        SET_BIT(obj->progs->entity_flags,PROG_NODESTRUCT);
        if(obj->progs->script_ref > 0) {
            obj->progs->extract_when_done = true;
            return;
        }
        p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    // Deal with all clone rooms here while the object's location still exists
    for(clone = obj->clone_rooms; clone; clone = next_clone) {
        next_clone = clone->next_clone;
        p_percent_trigger(NULL, NULL, clone, NULL, NULL, NULL, NULL, obj, NULL, TRIG_CLONE_EXTRACT, NULL);
        room_from_environment(clone);
    }

    /* if its a scroll make sure that they can't finish reciting the scroll
       unless they still have it. this happens if for example you are fighting
       and it burns up from a flaming weapon. */
    if (obj->item_type == ITEM_SCROLL)
    {
    CHAR_DATA *ch;

    if (obj->carried_by != NULL && obj->carried_by->in_room != NULL)
    {
        for (ch = obj->carried_by->in_room->people; ch != NULL; ch = ch->next_in_room)
        {
        if (ch->recite_scroll == obj)
            ch->recite_scroll = NULL;
        }
    }
    }

    iterator_start(&it, loaded_instances);
    while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
    {
        if( IS_VALID(instance->object) && instance->object == obj)
        {
            extract_instance(instance);
        }
    }

    iterator_stop(&it);

    if (obj->carried_by != NULL)
    obj_from_char(obj);
    else if (obj->in_obj != NULL)
    obj_from_obj(obj);
    else if (obj->in_room != NULL)
    obj_from_room(obj);
    else if (obj->in_mail != NULL)
    obj_from_mail(obj);
    else if (obj->locker == true)
    obj_from_locker(obj);

    /* extract obj's contents */
    for (obj_content = obj->contains; obj_content; obj_content = obj_next)
    {
    obj_next = obj_content->next_content;
    extract_obj(obj_content);
    }

    list_remlink(loaded_objects, obj, false);
    loaded_obj_hash_remove(obj);

    // Clear the most recent corpse data on the player owner
    if( (obj->item_type == ITEM_CORPSE_PC) && !IS_NULLSTR(obj->owner) )
    {
        CHAR_DATA *victim = get_char_world(NULL, obj->owner);

        if( victim && !IS_NPC(victim) )
            victim->pcdata->corpse = NULL;
    }

    extract_special_key(obj);

    --obj->pIndexData->count;
    list_appendlink(gc_objects, obj);
    obj->gc = true;
}


/*
 * Extract a char from the world.
 */
void extract_char(CHAR_DATA *ch, bool fPull)
{
    ROOM_INDEX_DATA *clone, *next_clone;
    CHAR_DATA *wch;
    DESCRIPTOR_DATA *d;
    ITERATOR it;

    if (ch->gc || list_hasdata(gc_mobiles, ch))
        return;

    if (ch->in_room == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
            "extract_char: %s had null ch->in_room (NPC=%d, valid=%d). "
            "Character may have already been extracted or was never placed in a room.",
            IS_NPC(ch) ? ch->short_descr : ch->name,
            IS_NPC(ch) ? 1 : 0,
            IS_VALID(ch) ? 1 : 0);
    return;
    }

    if(IS_NPC(ch) && ch->progs) {
        SET_BIT(ch->progs->entity_flags,PROG_NODESTRUCT);
        if(ch->progs->script_ref > 0) {
            ch->progs->extract_when_done = true;
            ch->progs->extract_fPull = ch->progs->extract_fPull || fPull;
            return;
        }
        p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    nuke_pets(ch);
    ch->pet = NULL; /* just in case */

    if (fPull)
    die_follower(ch);

    stop_fighting(ch, true);
    stop_holdup(ch);

    // Deal with all clone rooms here while the object's location still exists
    for(clone = ch->clone_rooms; clone; clone = next_clone) {
        next_clone = clone->next_clone;
        p_percent_trigger(NULL, NULL, clone, NULL, NULL, ch, NULL, NULL, NULL, TRIG_CLONE_EXTRACT, NULL);
        room_from_environment(clone);
    }

    char_from_room(ch);

    if( IS_VALID(ch->belongs_to_ship) )
    {
        // NPC SHIP: Is this the ship's captain?

        // Remove from ship's crew
        if( list_hasdata(ch->belongs_to_ship->crew, ch) )
            list_remlink(ch->belongs_to_ship->crew, ch, false);

        if( list_hasdata(ch->belongs_to_ship->oarsmen, ch) )
            list_remlink(ch->belongs_to_ship->oarsmen, ch, false);

        if( ch->belongs_to_ship->first_mate == ch )
            ch->belongs_to_ship->first_mate = NULL;

        if( ch->belongs_to_ship->navigator == ch )
            ch->belongs_to_ship->navigator = NULL;

        if( ch->belongs_to_ship->scout == ch )
            ch->belongs_to_ship->scout = NULL;

        ch->belongs_to_ship = NULL;
    }

    /*
    if (ch->belongs_to_ship != NULL
    && ch->belongs_to_ship->npc_ship != NULL
    && ch->belongs_to_ship->npc_ship->captain == ch)
    ch->belongs_to_ship->npc_ship->captain = NULL;
    */

    if (IS_NPC(ch) && ch->hunting != NULL)
        stop_hunt(ch, true);

    if (ch->belongs_to_ship != NULL)
    char_from_crew(ch);

    if (!fPull)
    {
        ROOM_INDEX_DATA *death_room;

        death_room = get_reserved_room_index("room_death");
        {
            int dp_min = race_get_trait_int(ch->race, "death_plane_vnum_min");
            int dp_max = race_get_trait_int(ch->race, "death_plane_vnum_max");
            if (dp_min > 0 && dp_max > dp_min)
            {
                int range;
                range = number_range(0, dp_max - dp_min);
                long plane_vnum = dp_min + range;
                WNUM plane_wnum;
                if (resolve_widevnum(plane_vnum, NULL, &plane_wnum))
                    death_room = get_room_index(plane_wnum.pArea, plane_wnum.vnum);
                ch->hit = number_range(1, ch->max_hit);
                ch->mana = number_range(1, ch->max_mana);
                ch->move = number_range(1, ch->max_move);
            }
        }
    char_to_room(ch, death_room);
    return;
    }

    if(ch->mount) {
        ch->mount->rider = NULL;
        ch->mount->riding = false;
    }

    if(ch->rider) {
        ch->rider->mount = NULL;
        ch->rider->riding = false;
    }
    ch->mount = NULL;
    ch->rider = NULL;
    ch->riding = false;

    if (IS_NPC(ch))
    {
    GQ_MOB_DATA *gq_mob;

    if(!IS_SET(ch->act[0], ACT_ANIMATED))
        --ch->pIndexData->count;

    /* for NPCs and global quests. */
    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
    {
        if (wnum_match_mob(gq_mob->vnum_wnum, ch))
        {
        --gq_mob->count;
        }
    }
    }
    else
    {
    nuke_pets(ch);
    ch->pet = NULL;
    }

    if (ch->desc != NULL && ch->desc->original != NULL)
    {
    do_function(ch, &do_return, "");
    ch->desc = NULL;
    }

    /* modify reply targets and mprog targets */
    iterator_start(&it, loaded_chars);
    while((wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (wch->reply == ch)
            wch->reply = NULL;

        if (IS_NPC(wch) && wch->progs->target == ch)
            wch->progs->target = NULL;
    }
    iterator_stop(&it);
    list_remlink(loaded_chars, ch, false);
    // Temporarily disabled for reconnect crash.
    //list_remlink(loaded_players, ch, false);


    if (ch->desc != NULL)
        ch->desc->character = NULL;

    // Go through the world and if anyones hunting char, stop them
    // same for challenge to prevent challenge someone/quit crash bug
    for (d = descriptor_list; d != NULL; d = d->next) {
        if (d->character != NULL &&
            d->character->hunting != NULL &&
            d->character->hunting == ch) {
            send_to_char("You sense your target has vanished somewhere.\n\r", d->character);
            d->character->hunting = NULL;
        }

        if (d->character != NULL &&
            d->character->challenged != NULL &&
            d->character->challenged == ch) {
            send_to_char("Your challenger has left the game.\n\r", d->character);
            d->character->challenged = NULL;
        }
    }

    detach_instances_player(ch);
    detach_dungeons_player(ch);
    detach_ships_player(ch);

    list_appendlink(gc_mobiles, ch);
    ch->gc = true;
    return;
}

void extract_token(TOKEN_DATA *token)
{
    ITERATOR it;
    AFFECT_DATA *paf;

    if (token->gc || list_hasdata(gc_tokens, token))
        return;

    if(token->progs) {
        SET_BIT(token->progs->entity_flags,PROG_NODESTRUCT);
        if(token->progs->script_ref > 0) {
            token->progs->extract_when_done = true;
            return;
        }
        p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    /* Remove all affects created by this token from their owners */
    if (token->affects)
    {
        iterator_start(&it, token->affects);
        while ((paf = (AFFECT_DATA *)iterator_nextdata(&it)))
        {
            if (paf->valid)
            {
                /* Break the back-link first to prevent affect_remove from
                 * trying to modify this list while we're iterating it */
                paf->token = NULL;

                /* Remove the affect from its owning character or object */
                if (token->player)
                    affect_remove(token->player, paf);
                else if (token->object)
                    affect_remove_obj(token->object, paf);
            }
        }
        iterator_stop(&it);
        list_clear(token->affects);
    }

    if(token->player)
    {
        token_from_char(token);
    }
    else if (token->object)
    {
        token_from_obj(token);
    }
    else if (token->room)
    {
        token_from_room(token);
    }

    list_appendlink(gc_tokens, token);
    token->gc = true;
    return;
}


/*
 * Is the cart being pulled by anyone in the room?
 */
CHAR_DATA *get_cart_pulled(OBJ_DATA *obj)
{
    if (obj == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "In get_cart_pulled the obj was null.");
    return NULL;
    }

    return obj->pulled_by;
}


/*
 * Find a char in the room.
 */
CHAR_DATA *get_char_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *rch;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    if (!str_cmp(arg, "self")
    || !str_cmp(arg, "me")
    || (ch != NULL && !str_cmp(arg, ch->name)))
    return ch;

    if (ch && room)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_char_room received multiple types (ch/room)");
    //return NULL;
    }

    if (ch)
    rch = ch->in_room->people;
    else
    rch = room->people;

    for (; rch != NULL; rch = rch->next_in_room)
    {
    if ((ch && !can_see(ch, rch)) || !is_name(arg, rch->name))
        continue;
    if (++count == number)
        return rch;
    }

    return NULL;
}


/*
 * Find a char in the world, even if they are invisible etc.
 */
CHAR_DATA *find_char_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *wch;
    int number;
    int count;
    ITERATOR it;

    if (ch && (wch = get_char_room(ch, NULL, argument)) != NULL)
    return wch;

    number = number_argument(argument, arg);
    count  = 0;
    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (wch->in_room == NULL || !is_name(arg, wch->name))
            continue;
        if (++count == number)
            break;
    }
    iterator_stop(&it);

    return wch;
}


/*
 * Find a char in the world.
 */
CHAR_DATA *get_char_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *wch;
    int number;
    int count;
    ITERATOR it;

    if (ch && (wch = get_char_room(ch, NULL, argument)) != NULL)
        return wch;

    number = number_argument(argument, arg);
    count  = 0;
    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (wch->in_room == NULL ||
            (ch && !can_see(ch, wch)) ||
            !is_name(arg, wch->name))
            continue;
        if (++count == number)
            break;
    }
    iterator_stop(&it);

    return wch;
}


/*
 * Find a char in the world by its pIndexData
 */
CHAR_DATA *get_char_world_index(CHAR_DATA *ch, MOB_INDEX_DATA *pMobIndex)
{
    CHAR_DATA *wch;
    ITERATOR it;

    iterator_start(&it, loaded_chars);
    while(( wch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (wch->in_room == NULL ||
            (ch && !can_see(ch, wch)) ||
            !IS_NPC(wch) ||
            wch->pIndexData != pMobIndex)
            continue;

        break;
    }
    iterator_stop(&it);

    return wch;
}

/*
 * Find an object in the world by its pIndexData
 */
OBJ_DATA *get_obj_world_index(CHAR_DATA *ch, OBJ_INDEX_DATA *pObjIndex, bool all)
{
    OBJ_DATA *wobj;
    ITERATOR it;

    iterator_start(&it, loaded_objects);
    while(( wobj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if ((!all && (wobj->locker || wobj->in_mail)) ||
            (ch && !can_see_obj(ch, wobj)) ||
            wobj->pIndexData != pObjIndex)
            continue;

        break;
    }
    iterator_stop(&it);

    return wobj;
}


/*
 * Find some object with a given index data.
 * Used by area-reset 'P' command.
 */
OBJ_DATA *get_obj_type(OBJ_INDEX_DATA *pObjIndex, ROOM_INDEX_DATA *pRoom)
{
    register OBJ_DATA *obj;
    ITERATOR it;

    iterator_start(&it, loaded_objects);
    while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (obj->pIndexData == pObjIndex && obj->in_room == pRoom)
            break;
    }
    iterator_stop(&it);

    return obj;
}


/*
 * Safe version of get_obj_list that properly distinguishes between
 * traditional linked lists and LLIST structures
 */
OBJ_DATA *get_obj_list(CHAR_DATA *ch, char *argument, void *list)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;
    
    if (list == NULL)
        return NULL;
        
    number = number_argument(argument, arg);
    count = 0;

    // Check if we're dealing with an LLIST properly
    if (is_llist(list)) {
        LLIST *llist = (LLIST *)list;
        
        iterator_start(&it, llist);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (can_see_obj(ch, obj) && is_name(arg, obj->name)) {
                if (++count == number) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
        return NULL;
    }
    
    // Traditional linked list case
    for (obj = (OBJ_DATA *)list; obj != NULL; obj = obj->next_content) {
        if (can_see_obj(ch, obj) && is_name(arg, obj->name)) {
            if (++count == number)
                return obj;
        }
    }
    
    return NULL;
}

/*
 * Safe version of get_obj_list_number that properly distinguishes between
 * traditional linked lists and LLIST structures
 */
OBJ_DATA *get_obj_list_number(CHAR_DATA *ch, char *argument, int *nth, void *list)
{
    OBJ_DATA *obj;
    ITERATOR it;
    int number = *nth;
    
    if (list == NULL) {
        *nth = number;
        return NULL;
    }
    
    // Handle LLIST case safely
    if (is_llist(list)) {
        LLIST *llist = (LLIST *)list;
        
        iterator_start(&it, llist);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (can_see_obj(ch, obj) && is_name(argument, obj->name)) {
                if (--number < 1) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
        
        *nth = number;
        return NULL;
    }
    
    // Traditional linked list case
    for (obj = (OBJ_DATA *)list; obj != NULL; obj = obj->next_content) {
        if (can_see_obj(ch, obj) && is_name(argument, obj->name)) {
            if (--number < 1)
                return obj;
        }
    }
    
    *nth = number;
    return NULL;
}


/*
 * Find an obj in player's locker.
 */
OBJ_DATA *get_obj_locker(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;
    
    if (!ch || !ch->llocker) 
        return NULL;
    
    number = number_argument(argument, arg);
    count = 0;
    
    // Use the llocker LLIST
    iterator_start(&it, ch->llocker);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (is_name(arg, obj->name)) {
            if (++count == number) {
                iterator_stop(&it);
                return obj;
            }
        }
    }
    iterator_stop(&it);
    
    // Fallback to traditional list (for backward compatibility)
    if (count == 0 && ch->locker) {
        for (obj = ch->locker; obj != NULL; obj = obj->next_content) {
            if (is_name(arg, obj->name)) {
                if (++count == number)
                    return obj;
            }
        }
    }
    
    return NULL;
}


/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *get_obj_carry(CHAR_DATA *ch, char *argument, CHAR_DATA *viewer)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;
    
    if (!ch || (!ch->lcarrying && !ch->lstache))
        return NULL;
    
    number = number_argument(argument, arg);
    count = 0;
    
    // Use the lcarrying LLIST
    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (obj->wear_loc == WEAR_NONE && 
            (viewer ? can_see_obj(viewer, obj) : true) &&
            is_name(arg, obj->name)) {
            if (++count == number) {
                iterator_stop(&it);
                return obj;
            }
        }
    }
    iterator_stop(&it);

    iterator_start(&it, ch->lstache);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if ((viewer ? can_see_obj(viewer, obj) : true) &&
            is_name(arg, obj->name)) {
            if (++count == number) {
                iterator_stop(&it);
                return obj;
            }
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Find an obj in player's inventory with continuation support.
 */
OBJ_DATA *get_obj_carry_number(CHAR_DATA *ch, char *argument, int *nth, CHAR_DATA *viewer)
{
    OBJ_DATA *obj;
    int number = *nth;
    ITERATOR it;

    if (!ch || (!ch->lcarrying && !ch->carrying && !ch->lstache)) {
        *nth = number;
        return NULL;
    }

    // Use the lcarrying LLIST
    if (ch->lcarrying) {
        int item_num = 0;
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            item_num++;
            bool wear_ok = (obj->wear_loc == WEAR_NONE);
            bool see_ok = (viewer ? can_see_obj(viewer, obj) : true);
            bool name_ok = is_name(argument, obj->name);

            // Log each item checked
            log_stringf("  [%d] vnum=%ld name='%s' short='%s' wear_loc=%d wear_ok=%d see_ok=%d name_ok=%d",
                       item_num,
                       obj->pIndexData ? obj->pIndexData->vnum : 0,
                       obj->name ? obj->name : "(null)",
                       obj->short_descr ? obj->short_descr : "(null)",
                       obj->wear_loc,
                       wear_ok, see_ok, name_ok);

            if (wear_ok && see_ok && name_ok) {
                if (--number < 1) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
    }

    if (ch->lstache) {
        iterator_start(&it, ch->lstache);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            bool see_ok = (viewer ? can_see_obj(viewer, obj) : true);
            bool name_ok = is_name(argument, obj->name);

            if (see_ok && name_ok) {
                if (--number < 1) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
    }

    *nth = number;
    return NULL;
}

/*
 * Find an obj in player's inventory by vnum.
 */
OBJ_DATA *get_obj_vnum_carry(CHAR_DATA *ch, long vnum, CHAR_DATA *viewer)
{
    OBJ_DATA *obj;
    ITERATOR it;
    
    if (!ch || (!ch->lcarrying && !ch->carrying && !ch->lstache))
        return NULL;
    
    // Use the lcarrying LLIST
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->wear_loc == WEAR_NONE &&
                (viewer ? can_see_obj(viewer, obj) : true) &&
                obj->pIndexData->vnum == vnum) {
                iterator_stop(&it);
                return obj;
            }
        }
        iterator_stop(&it);
    }

    if (ch->lstache) {
        iterator_start(&it, ch->lstache);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if ((viewer ? can_see_obj(viewer, obj) : true) &&
                obj->pIndexData->vnum == vnum) {
                iterator_stop(&it);
                return obj;
            }
        }
        iterator_stop(&it);
    }
    
    return NULL;
}

/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *get_obj_wear(CHAR_DATA *ch, char *argument, bool character)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;
    
    if (!ch)
        return NULL;
    
    number = number_argument(argument, arg);
    count = 0;
    
    // Use the lworn LLIST
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if ((character ? can_see_obj(ch, obj) : true) &&
                is_name(arg, obj->name)) {
                if (++count == number) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
    }

    return NULL;
}


/*
 * Find an obj in player's equipment with continuation support.
 */
OBJ_DATA *get_obj_wear_number(CHAR_DATA *ch, char *argument, int *nth, bool character)
{
    OBJ_DATA *obj;
    int number = *nth;
    ITERATOR it;
    
    if (!ch) {
        *nth = number;
        return NULL;
    }
    
    // Use the lworn LLIST
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if ((character ? can_see_obj(ch, obj) : true) &&
                is_name(argument, obj->name)) {
                if (--number < 1) {
                    iterator_stop(&it);
                    return obj;
                }
            }
        }
        iterator_stop(&it);
    }
    
    *nth = number;
    return NULL;
}


/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;

    if (ch && room) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_obj_here received both a ch and a room");
        return NULL;
    }

    if (!ch && !room) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_obj_here received neither a ch nor a room");
        return NULL;
    }

    number = number_argument(argument, arg);

    if (ch) {
        // First check room contents using either lcontents (preferred) or contents (legacy)
        if (ch->in_room) {
            if (ch->in_room->lcontents) {
                obj = get_obj_list_number(ch, arg, &number, ch->in_room->lcontents);
                if (obj)
                    return obj;
            } else if (ch->in_room->contents) {
                obj = get_obj_list_number(ch, arg, &number, ch->in_room->contents);
                if (obj)
                    return obj;
            }
        }

        // Then check carried items
        obj = get_obj_carry_number(ch, arg, &number, ch);
        if (obj)
            return obj;

        // Finally check worn items
        obj = get_obj_wear_number(ch, arg, &number, true);
        return obj;
    }
    else { // room only
        // Check room contents using either lcontents (preferred) or contents (legacy)
        if (room->lcontents) {
            obj = get_obj_list_number(NULL, arg, &number, room->lcontents);
            if (obj)
                return obj;
        } 
        
        return get_obj_list_number(NULL, arg, &number, room->contents);
    }
}

/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here_number(CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument, int *nth)
{
    OBJ_DATA *obj;

    if (ch && room)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_obj_here received a ch and a room");
        return NULL;
    }

    if (ch)
    {
        obj = get_obj_list_number(ch, argument, nth, ch->in_room->contents);
        if (obj != NULL)
            return obj;

        if ((obj = get_obj_carry_number(ch, argument, nth, ch)) != NULL)
            return obj;

        if ((obj = get_obj_wear_number(ch, argument, nth, true)) != NULL)
            return obj;
    }
    else
    {
        int number = *nth;

        for (obj = room->contents; obj; obj = obj->next_content)
        {
            if (!is_name(argument, obj->name))
                continue;
            if (--number < 1)
                return obj;
        }

        *nth = number;
    }

    return NULL;
}

/*
 * Same as get_obj_here, except it checks the inventory FIRST.
 *
 * If worn is true, worn items will be checked before carried items.
 */
OBJ_DATA *get_obj_inv(CHAR_DATA *ch, char *argument, bool worn)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;

    if (!ch) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_obj_inv received NULL ch");
        return NULL;
    }

    number = number_argument(argument, arg);

    if (worn) {
        // Check worn items first
        obj = get_obj_wear_number(ch, arg, &number, true);
        if (obj)
            return obj;

        // Then check carried items
        obj = get_obj_carry_number(ch, arg, &number, ch);
        if (obj)
            return obj;
    } else {
        // Check carried items first
        obj = get_obj_carry_number(ch, arg, &number, ch);
        if (obj)
            return obj;

        // Then check worn items
        obj = get_obj_wear_number(ch, arg, &number, true);
        if (obj)
            return obj;
    }

    // Finally check room contents - using lcontents if available, otherwise contents
    if (ch->in_room) {
        if (ch->in_room->lcontents)
            return get_obj_list_number(ch, arg, &number, ch->in_room->lcontents);
        else
            return get_obj_list_number(ch, arg, &number, ch->in_room->contents);
    }
    
    return NULL;
}

/*
 * Same as get_obj_inv but only checks inventory and worn items, not the room.
 */
OBJ_DATA *get_obj_inv_only(CHAR_DATA *ch, char *argument, bool worn)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    int number;

    if (!ch) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_obj_inv_only received NULL ch");
        return NULL;
    }

    number = number_argument(argument, arg);

    if (worn) {
        // Check worn items first
        obj = get_obj_wear_number(ch, arg, &number, true);
        if (obj)
            return obj;

        // Then check carried items
        obj = get_obj_carry_number(ch, arg, &number, ch);
        if (obj)
            return obj;
    } else {
        // Check carried items first
        obj = get_obj_carry_number(ch, arg, &number, ch);
        if (obj)
            return obj;

        // Then check worn items
        obj = get_obj_wear_number(ch, arg, &number, true);
        if (obj)
            return obj;
    }

    return NULL;
}


/*
 * Find an obj in the world.
 */
OBJ_DATA *get_obj_world(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;
    ITERATOR it;

    if (ch && (obj = get_obj_here(ch, NULL, argument)) != NULL)
        return obj;

    number = number_argument(argument, arg);
    count  = 0;

    iterator_start(&it, loaded_objects);
    while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if ((ch && !can_see_obj(ch, obj)) ||
            !is_name(arg, obj->name))
            continue;
        if (++count == number)
            break;
    }
    iterator_stop(&it);

    return obj;
}


/* deduct cost from a character */
void deduct_cost(CHAR_DATA *ch, int cost)
{
    int silver = 0, gold = 0;

    silver = UMIN(ch->silver,cost);

    if (silver < cost)
    {
    gold = ((cost - silver + 99) / 100);
    silver = cost - 100 * gold;
    }

    ch->gold -= gold;
    ch->silver -= silver;

    if (ch->gold < 0)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "deduct costs: gold %d < 0",ch->gold);
    ch->gold = 0;
    }

    if (ch->silver < 0)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "deduct costs: silver %d < 0",ch->silver);
    ch->silver = 0;
    }
}


/*
 * Create a 'money' obj.
 */
OBJ_DATA *create_money(int gold, int silver)
{
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;

    if (gold < 0 || silver < 0 || (gold == 0 && silver == 0))
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Create_money: zero or negative money.",UMIN(gold,silver));
    gold = UMAX(0,gold);
    silver = UMAX(1,silver);
    }

    if (gold == 0 && silver == 1)
    obj = create_object(get_reserved_obj_index("obj_coin_silver_single"), 0, true);
    else if (gold == 1 && silver == 0)
    obj = create_object(get_reserved_obj_index("obj_coin_gold_single"), 0, true);
    else if (silver == 0)
    {
        obj = create_object(get_reserved_obj_index("obj_coin_gold_multiple"), 0, true);
        sprintf(buf, obj->short_descr, gold);
        free_string(obj->short_descr);
        obj->short_descr        = str_dup(buf);
        MONEY(obj)->gold        = gold;
        obj->cost               = gold;
    obj->weight		= get_weight_coins(silver, gold);
    }
    else if (gold == 0)
    {
        obj = create_object(get_reserved_obj_index("obj_coin_silver_multiple"), 0, true);
        sprintf(buf, obj->short_descr, silver);
        free_string(obj->short_descr);
        obj->short_descr        = str_dup(buf);
        MONEY(obj)->silver      = silver;
        obj->cost               = silver;
    obj->weight		= get_weight_coins(silver, gold);
    }

    else
    {
    obj = create_object(get_reserved_obj_index("obj_coin_mixed"), 0, true);
    sprintf(buf, obj->short_descr, silver, gold);
    free_string(obj->short_descr);
    obj->short_descr	= str_dup(buf);
    MONEY(obj)->silver	= silver;
    MONEY(obj)->gold	= gold;
    obj->cost		= 100 * gold + silver;
    obj->weight		= get_weight_coins(silver, gold);
    }

    return obj;
}


/*
 * Return # of objects which an object counts as.
 */
int get_obj_number(OBJ_DATA *obj)
{
    int number;

    if (obj->item_type == ITEM_MONEY)
        number = 0;
    else
        number = 1;

    return number;
}


/* Get number of items in a container */
int get_obj_number_container(OBJ_DATA *obj)
{
    int number = 0;

    for (obj = obj->contains; obj != NULL; obj = obj->next_content)
        number += get_obj_number(obj);

    return number;
}


/*
 * Return weight of an object, including weight of contents.
 * Adjusts content weight for weight-multiplier.
 */
int get_obj_weight(OBJ_DATA *obj)
{
    int weight;

    if (obj->item_type == ITEM_MONEY)
        return get_weight_coins(MONEY(obj)->silver, MONEY(obj)->gold);

    weight = obj->weight;

    // This is for containers. Calculate weight of contents and factor in weight reduction
    weight += (get_obj_weight_container(obj)* WEIGHT_MULT(obj))/100;

    return weight;
}


/* return weight of x silver and y gold */
int get_weight_coins(long silver, long gold)
{
    return silver/800 + gold/300;
}


/*
 * Return weight of a container's objects.
 * This doesn't adjust for weight-multiplier, so note that.
 */
int get_obj_weight_container(OBJ_DATA *obj)
{
    int weight;
    OBJ_DATA *tobj;

    weight = 0;
    for (tobj = obj->contains; tobj != NULL; tobj = tobj->next_content)
    weight += tobj->weight;

    return weight;
}


/*
 * True if room is dark.
 */
bool room_is_dark(ROOM_INDEX_DATA *pRoomIndex)
{
    OBJ_DATA *obj;
    CHAR_DATA *ch;
    ITERATOR it;

    // Check for room darkness objects
    iterator_start(&it, pRoomIndex->lcontents);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (obj->item_type == ITEM_ROOM_DARKNESS) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    // Check for light-emitting equipment on characters
    iterator_start(&it, pRoomIndex->lpeople);
    while ((ch = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (ch->lworn) {
            ITERATOR it_obj;
            iterator_start(&it_obj, ch->lworn);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it_obj))) {
                if (IS_SET(obj->extra[1], ITEM_EMITS_LIGHT)) {
                    iterator_stop(&it_obj);
                    iterator_stop(&it);
                    return false;
                }
            }
            iterator_stop(&it_obj);
        }
    }
    iterator_stop(&it);

    // Check for light from adjacent rooms
    for (int i = 0; i < MAX_DIR; i++) {
        ROOM_INDEX_DATA *to_room;
        EXIT_DATA *exit = pRoomIndex->exit[i];

        if (!exit || IS_SET(exit->exit_info, EX_CLOSED))
            continue;

        to_room = exit->u1.to_room;
        if (!to_room || !to_room->people)
            continue;

        iterator_start(&it, to_room->lpeople);
        while ((ch = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (ch->lworn) {
                ITERATOR it_obj;
                iterator_start(&it_obj, ch->lworn);
                while ((obj = (OBJ_DATA *)iterator_nextdata(&it_obj))) {
                    if (IS_SET(obj->extra[1], ITEM_EMITS_LIGHT)) {
                        iterator_stop(&it_obj);
                        iterator_stop(&it);
                        return false;
                    }
                }
                iterator_stop(&it_obj);
            }
        }
        iterator_stop(&it);
    }

    // Standard light checks
    if (pRoomIndex->light > 0)
        return false;

    if (IS_SET(pRoomIndex->room_flag[0], ROOM_DARK))
        return true;

    if (room_in_sector(pRoomIndex, SECT_INSIDE) ||
        room_in_sector(pRoomIndex, SECT_CITY))
        return false;

    if (weather_info.sunlight == SUN_DARK)
        return true;

    return false;
}


/* is ch the owner of the room? */
bool is_room_owner(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    if (room->owner == NULL || room->owner[0] == '\0')
    return false;

    return is_name(ch->name,room->owner);
}


/*
 * True if room is private to a char.
 */
bool room_is_private(ROOM_INDEX_DATA *pRoomIndex, CHAR_DATA *looker)
{
    CHAR_DATA *rch;
    int count;
    int max_lev = 0;

    if (looker && !IS_NPC(looker) && looker->tot_level == MAX_LEVEL)
    return false;

    count = 0;
    for (rch = pRoomIndex->people; rch != NULL; rch = rch->next_in_room)
    {
    count++;

    if (!IS_NPC(rch) && rch->tot_level > max_lev)
        max_lev = rch->tot_level;
    }

    if (looker && !IS_NPC(looker) && IS_IMMORTAL(looker))
    {
    if (looker->tot_level > max_lev)
        return false;
    }

    if (IS_SET(pRoomIndex->room_flag[0], ROOM_PRIVATE)  && count >= 2)
    return true;

    if (IS_SET(pRoomIndex->room_flag[0], ROOM_SOLITARY) && count >= 1)
    return true;

    if (IS_SET(pRoomIndex->room_flag[0], ROOM_IMP_ONLY))
    return true;

    return false;
}

bool has_light(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (!ch) return false;

    // Use the lworn LLIST
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->wear_loc == WEAR_LIGHT || IS_SET(obj->extra[1], ITEM_EMITS_LIGHT)) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }

    return false;
}

bool can_see_imm(CHAR_DATA *ch, CHAR_DATA *victim)
{
    STRING_DATA *string;

    if (victim == NULL || ch == NULL)
        return false;

    /* allow imms to be vis to some people */
    if (!IS_NPC(victim))
    {
        for (string = victim->pcdata->vis_to_people; string != NULL; string = string->next)
        {
            if (!str_cmp(ch->name, string->string))
                return true;
        }
    }

    if (ch->tot_level < victim->invis_level)
        return false;

    if (!IS_IMMORTAL(ch) && IS_NPC(victim) && IS_SET(victim->act[1], ACT2_WIZI_MOB) && !IS_SET(ch->act[1], ACT2_SEE_WIZI))
        return false;

    if (get_staff_rank(ch) < victim->incog_level && ch->in_room != victim->in_room)
        return false;

    return true;
}

/*
 * True if char can see victim.
 */
bool can_see(CHAR_DATA *ch, CHAR_DATA *victim)
{
    STRING_DATA *string;

    if (victim == NULL || ch == NULL)
        return false;

    /* imms w/ holylight can see everyone except higher level invis imms */
    if (!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) && victim->invis_level <= get_staff_rank(ch))
        return true;

    // these types of mobs can see everybody.
    if (IS_NPC(ch) && (IS_SET(ch->act[1], ACT2_SEE_ALL) || IS_SET(ch->act[0], ACT_IS_BANKER) || IS_SET(ch->act[0], ACT_IS_CHANGER) || ch->pIndexData->pQuestor != NULL))
        return true;

    if (IS_AFFECTED(ch, AFF_BLIND))
        return false;

    if (is_darked(ch->in_room))
        return false;

    if (ch->in_room && IS_SET(ch->in_room->room_flag[0], ROOM_DARK))
    {
        //(!IS_AFFECTED(ch, AFF_INFRARED) || has_light(ch)) && IS_AFFECTED2(victim,AFF2_DARK_SHROUD)

        if( IS_AFFECTED2(victim,AFF2_DARK_SHROUD) )
            return false;

        if( !IS_AFFECTED(ch, AFF_INFRARED) && !has_light(ch) )
            return false;
    }

    if(!IS_AFFECTED2(victim,AFF2_DARK_SHROUD)) {
        if (IS_SET(victim->affected_by[1], AFF2_CLOAK_OF_GUILE) && IS_NPC(ch) && !IS_SET(ch->affected_by[1], AFF2_SEE_CLOAK))
            return false;

        if ((IS_AFFECTED(victim, AFF_INVISIBLE) || IS_AFFECTED2(victim, AFF2_IMPROVED_INVIS)) && !IS_AFFECTED(ch, AFF_DETECT_INVIS))
            return false;
    }

    if (IS_AFFECTED(victim, AFF_HIDE) && !IS_AFFECTED(ch, AFF_DETECT_HIDDEN) && !IS_SAGE(ch) && victim->fighting == NULL)
        return false;

    // @@@ Nib 20070715 : Changed so that you need deathsight to see anyone that is dead...
    if (IS_DEAD(victim) && !IS_DEAD(ch) && (!IS_AFFECTED2(ch,AFF2_DEATHSIGHT) || victim->tot_level > ch->deathsight_vision))
        return false;

    if (ch == victim)
        return true;

    /* allow imms to be vis to some people */
    if (!IS_NPC(victim))
    {
        for (string = victim->pcdata->vis_to_people; string != NULL; string = string->next)
        {
            if (!str_cmp(ch->name, string->string))
                return true;
        }
    }

    if (ch->tot_level < victim->invis_level)
        return false;

    if (!IS_IMMORTAL(ch) && IS_NPC(victim) && IS_SET(victim->act[1], ACT2_WIZI_MOB) && !IS_SET(ch->act[1], ACT2_SEE_WIZI))
        return false;

    if (get_staff_rank(ch) < victim->incog_level && ch->in_room != victim->in_room)
        return false;

    return true;
}


/* visibility on a room -- for entering and exits */
bool can_see_room(CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex)
{
    if (IS_SET(pRoomIndex->room_flag[0], ROOM_IMP_ONLY)
    &&  get_staff_rank(ch) < MAX_LEVEL)
    return false;

    if (IS_SET(pRoomIndex->room_flag[0], ROOM_GODS_ONLY)
    &&  !IS_IMMORTAL(ch))
    return false;

    if (IS_SET(pRoomIndex->room_flag[0],ROOM_NEWBIES_ONLY)
    &&  ch->level > 10 && !IS_IMMORTAL(ch))
    return false;

    return true;
}


/*
 * True if char can see obj.
 */
bool can_see_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
    if (obj == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_see_obj, obj was NULL!!!");
        return false;
    }

    if (ch == NULL) {
        // If no character is provided, we can always "see" the object
        // This prevents crashes in script triggers and other scenarios
        return true;
    }

    // Toggled on dead people's possessions.
    if (IS_SET(obj->extra[1], ITEM_UNSEEN)) {
        return false;
    }

    if (IS_SET(obj->extra[1], ITEM_BURIED)) {
        return false;
    }

    if (obj->item_type == ITEM_SEED && IS_SET(obj->extra[0], ITEM_PLANTED)) {
        return false;
    }

    // Item hidden on the ground, must be searched for first
    if (IS_SET(obj->extra[0], ITEM_HIDDEN) && obj->in_room != NULL) {
        return false;
    }

    if (!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
        return true;

    /*
    if (IS_NPC(ch)
    && !IS_DEAD(ch)
    && obj->carried_by == ch)
    return false;
    */

    if ((obj->carried_by != NULL && IS_DEAD(obj->carried_by))
    && obj->wear_loc == WEAR_NONE)
    return false;

    if (IS_AFFECTED(ch, AFF_BLIND))
    return false;

    if (obj->item_type == ITEM_LIGHT && LIGHT(obj)->duration != 0)
    return true;

    if (IS_SET(obj->extra[0], ITEM_INVIS)
    &&   !IS_AFFECTED(ch, AFF_DETECT_INVIS))
        return false;

    if (IS_AFFECTED(ch, AFF_INFRARED) && !is_darked(ch->in_room))
    return true;

    if (IS_OBJ_STAT(obj,ITEM_GLOW))
    return true;

    if (room_is_dark(ch->in_room))
        return false;

    return true;
}



// Assume that if first char is colour code, then UPPER third char, else first.
char *upper_first(char *arg)
{
    if (arg == NULL)
    return NULL;

    if (*arg == '\n')
    return arg;
    else
    if (*arg == COLOUR_CHAR && *(arg + 1) == '[')
        *(arg + 7) = UPPER(*(arg + 7));
    else
    if (*arg == COLOUR_CHAR)
        *(arg + 2) = UPPER(*(arg + 2));
    else
        *arg = UPPER(*arg);

    return arg;
}


int get_trade_item(char *arg)
{
    int counter;

    counter = 0;
    while(trade_table[counter].trade_type != TRADE_LAST)
    {
        if (!str_prefix(arg, trade_table[counter].name))
        break;
        counter++;
    }

    return counter;
}


TRADE_ITEM *find_trade_item args((AREA_DATA* pArea, char *arg)) {
    TRADE_ITEM *item;

    item = pArea->trade_list;
    while(item != NULL)
    {
        if (!str_prefix(trade_table[item->trade_type].name, arg))
        break;
        item = item->next;
    }

    return item;
}


/*
 * Move a char into a ships crew.
 */
void char_to_crew(CHAR_DATA *ch, SHIP_DATA *ship)
{
#if 0
    ROOM_INDEX_DATA *pRoomIndex;

    if (ship == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "SHIP WAS NULL with a char_to_crew.");
    return;
    }

    pRoomIndex = ship->ship_rooms[0];
    char_to_room(ch, pRoomIndex);

    ch->belongs_to_ship  = ship;
    ch->next_in_crew    = ship->crew_list;
    ship->crew_list     = ch;
#endif

    return;
}

/*
 * Move a char into an invasion force.
 */
void char_to_invasion(CHAR_DATA *ch, INVASION_QUEST *invasion)
{
    if (invasion == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "INVASION WAS NULL with a char_to_invasion.");
    return;
    }

    ch->next_in_invasion  = invasion->invasion_mob_list;
    invasion->invasion_mob_list     = ch;

    return;
}

/*
 * Move a char out of invasion force.
 */
void char_from_invasion(CHAR_DATA *ch, INVASION_QUEST *quest)
{
    if (ch == quest->invasion_mob_list)
    {
        quest->invasion_mob_list = ch->next_in_invasion;
    }
    else
    {
        CHAR_DATA *prev;

        for (prev = quest->invasion_mob_list; prev != NULL;
                prev = prev->next_in_invasion)
        {
            if (prev->next_in_invasion == ch)
            {
                prev->next_in_invasion = ch->next_in_invasion;
                break;
            }
        }
    }
    ch->next_in_invasion     = NULL;
    return;
}


/*
 * Move a char out of crew.
 */
void char_from_crew(CHAR_DATA *ch)
{
#if 0
    if (ch->belongs_to_ship == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Char_from_crew: belongs_to_ship was null.");
        return;
    }

    if (ch == ch->belongs_to_ship->crew_list)
    {
        ch->belongs_to_ship->crew_list = ch->next_in_crew;
    }
    else
    {
        CHAR_DATA *prev;

        for (prev = ch->belongs_to_ship->crew_list; prev; prev = prev->next_in_crew)
        {
            if (prev->next_in_crew == ch)
            {
                prev->next_in_crew = ch->next_in_crew;
                break;
            }
        }

        if (prev == NULL)
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Char_from_crew: ch not found.");
    }

    ch->belongs_to_ship  = NULL;
    ch->next_in_crew     = NULL;
    ch->on 	         = NULL;  /* sanity check! */
#endif
    return;
}

/*
 * Parses a command and returns which door it is.
 */
int parse_direction(char *arg)
{
    int counter;
    int direction = -1;

    if (arg[0] == '\0')
    return direction;

    /* Vizz - rewrote this to include "n,e,w,s,u,d" directions
     *        (fuck knows why they weren't in here already), and also
     *        made each if an if ... else so that we don't have to do
     *        all of them every time.
     */
    // Nib - changed it to reassigning the char* since all printf's are inefficient really...
    //		especially if this is used alot.
    if (!str_cmp(arg, "n")) arg = "north";
    else if (!str_cmp(arg, "e")) arg = "east";
    else if (!str_cmp(arg, "s")) arg = "south";
    else if (!str_cmp(arg, "w")) arg = "west";
    else if (!str_cmp(arg, "u")) arg = "up";
    else if (!str_cmp(arg, "d")) arg = "down";
    else if (!str_cmp(arg, "nw")) arg = "northwest";
    else if (!str_cmp(arg, "ne")) arg = "northeast";
    else if (!str_cmp(arg, "sw")) arg = "southwest";
    else if (!str_cmp(arg, "se")) arg = "southeast";

/* Vizz - hmm - doesn't the use of str_prefix() here mean that if
 *        the argument string is, for example, "northeast" you'll flee north?!
    if (!str_prefix(arg, "north")) sprintf(arg, "north");
    if (!str_prefix(arg, "south")) sprintf(arg, "south");
    if (!str_prefix(arg, "east")) sprintf(arg, "east");
    if (!str_prefix(arg, "west")) sprintf(arg, "west");
*/
  for (counter = 0; counter < MAX_DIR; counter++)
    {
    if (!str_cmp(arg, dir_name[counter]))
        direction = counter;
    }

    return direction;
}


/* make NPC ch hunt a victim */
void hunt_char(CHAR_DATA *ch, CHAR_DATA *victim)
{
    bool found;
    char buf[MSL];
    CHAR_DATA *temp;

    if (!IS_NPC(ch)) {
    sprintf(buf, "hunt_char: non-NPC %s", ch->name);
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
    return;
    }

    if (IS_SET(ch->act[1], ACT2_NO_CHASE))
    return;

    found = false;
    temp = hunt_last;
    while (temp != NULL)
    {
        if (temp == ch)
        found = true;
        temp = temp->next_in_hunting;
    }

    if (found)
    return;

    ch->next_in_hunting    = hunt_last;
    hunt_last              = ch;
    ch->hunting = victim;
}


// Stop an NPC hunting
void stop_hunt(CHAR_DATA *ch, bool dead)
{
    if (ch == hunt_last)
    hunt_last = ch->next_in_hunting;
    else
    {
    CHAR_DATA *prev;

    for (prev = hunt_last; prev; prev = prev->next_in_hunting)
    {
        if (prev->next_in_hunting == ch)
        {
        prev->next_in_hunting = ch->next_in_hunting;
        break;
        }
    }

    if (prev == NULL)
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "stop_hunt: ch not found.");
    }

    ch->hunting  = NULL;
    ch->next_in_hunting = NULL;

    if (!dead && ch->home_room != NULL)
    {
        act("$n wanders off.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(ch);
    char_to_room(ch, ch->home_room);
    }
}


AREA_DATA *find_area(char *name)
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
    if (!str_cmp(temp->name, name))
        break;
    }

    if (temp == NULL) {
    log_message_f(LOG_LEVEL_WARN, LOG_WARN, "find_area: couldn't find area %s", name);
    }

    return temp;
}


AREA_DATA *find_area_kwd(char *keyword)
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
    if (!str_infix(keyword, temp->name))
        break;
    }

    return temp;
}


bool is_on_ship(CHAR_DATA *ch, SHIP_DATA *ship)
{
    if (ch->in_room != NULL
    && ch->in_room->ship != NULL
    && ch->in_room->ship == ship)
    {
    return true;
    }
    else
    {
    return false;
    }
}


/* Resurrect a dead PC. */
void resurrect_pc(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    ROOM_INDEX_DATA *pRoom = NULL;
    OBJ_DATA *obj;
    ITERATOR it;

    if (!IS_DEAD(ch))
    {
        sprintf(buf, "resurrect_pc: %s is not dead!", ch->name);
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
        return;
    }

    if (IS_NPC(ch))
    {
        sprintf(buf, "resurrect_pc: %s is an NPC!", ch->short_descr);
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "%s", buf);
        return;
    }

    ch->time_left_death = 0;

    if (ch->pcdata) {
        ch->pcdata->pending_resurrect_offer_expires = 0;
        ch->pcdata->pending_resurrect_offer_from_id[0] = 0;
        ch->pcdata->pending_resurrect_offer_from_id[1] = 0;
        ch->pcdata->pending_summon_offer_expires = 0;
        ch->pcdata->pending_summon_offer_from_id[0] = 0;
        ch->pcdata->pending_summon_offer_from_id[1] = 0;
    }

    char_from_room(ch);

    if ((pRoom = location_to_room(&ch->recall)) == NULL)
        pRoom = get_reserved_room_index("room_default_altar");

    char_to_room(ch, pRoom);
    location_clear(&ch->recall);
    
    // remove and reset affects
    while (ch->affected)
        affect_remove(ch, ch->affected);

    if (IS_SAGE(ch))
        SET_BIT(ch->affected_by[0], AFF_DETECT_HIDDEN);

    ch->dead = false;

    if (IS_AFFECTED(ch, AFF_CHARM))
    {
        char buf[MAX_STRING_LENGTH];

        if (ch->master != NULL)
        {
            sprintf(buf, "%s dissipates into the shadows.\n\r", ch->name);
            send_to_char(buf, ch->master);
        }

        send_to_char("You soul is free once more.\n\r", ch);
    }

    // Use iterator to traverse the lcarrying list
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_UNSEEN))
                REMOVE_BIT(obj->extra[1], ITEM_UNSEEN);
        }
        iterator_stop(&it);
    }
    
    // Also check worn items
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_UNSEEN))
                REMOVE_BIT(obj->extra[1], ITEM_UNSEEN);
        }
        iterator_stop(&it);
    }

    /*FREE DEATH ITEMS HERE IF ANY */

    affect_fix_char(ch);

    /* Reset form and parts - Fixes issue 33 on gitlab repo - Tieryo 07/22/2016 */
    /* Went back to fix properly for issue 126 */
    ch->form = ch->race ? ch->race->form : 0;
    ch->parts = ch->race ? (ch->race->parts & ~ch->lostparts) : 0;
    ch->lostparts = 0;   // Restore anything lost

    if (IS_SAGE(ch))
        SET_BIT(ch->affected_by[0], AFF_DETECT_HIDDEN);

    update_pos(ch);

    // Used to handle any post resurrection actions
    p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->pcdata->corpse, NULL, TRIG_RESURRECT, NULL);
    p_percent_trigger(NULL, ch->pcdata->corpse, NULL, NULL, ch, ch, NULL, NULL, NULL, TRIG_RESURRECT, NULL);
}

/* is a mob a global mob? */
bool is_global_mob(CHAR_DATA *mob)
{
    GQ_MOB_DATA *gq_mob;

    if (!IS_NPC(mob))
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "is_global_mob: not an npc!");
    return false;
    }

    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
    {
    if (wnum_match_mob(gq_mob->vnum_wnum, mob))
        return true;
    }

    return false;
}

// Create a pad function that accepts a string, a length, a colour, and a character to pad with. It should return just the padding for the given string, up to the length supplied.
char *pad_string(char *string, int length, char *colour, char *character)
{
    int i, pad_length;
    char buf[MAX_STRING_LENGTH];

    if (colour == NULL)
        colour = "{X";

    if (character == NULL)
        character = " ";

    pad_length = length - strlen_no_colours(string);

    for (i = 0; i < pad_length; i++)
    {
    buf[i] = character[0];
    }
    buf[i] = '\0';

    return str_dup(buf);
}

/* send a line of length 'length' to a character, allow custom colour and character */
void line(CHAR_DATA *ch, int length, char *colour, char *character)
{
    int i;

    if (colour == NULL)
        colour = "{Y";

    if (character == NULL)
        character = "-";

    send_to_char(colour, ch);
    for (i = 0; i < length; i++)
    {
    send_to_char(character, ch);
    }
    send_to_char("{x\n\r", ch);
}


/* is a room darked with momentary darkness? */
bool is_darked(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;

    if (room == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "is_darked: in_room was null.");
    return false;
    }

    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->item_type == ITEM_ROOM_DARKNESS)
        return true;
    }

    return false;
}


/* returns short "handle" for a character */
char *pers(CHAR_DATA *ch, CHAR_DATA *looker)
{
    if (!can_see(looker, ch))
        return "someone";

    if (IS_NPC(ch) || IS_SWITCHED(ch))
        return ch->short_descr;

    if (IS_MORPHED(ch) || IS_SHIFTED(ch))
    {
        if (can_see_shift(looker, ch))
            return ch->name;
        else
            return ch->short_descr;
    }

    return ch->name;
}


/* can ch see through victim's shift? */
bool can_see_shift(CHAR_DATA *ch, CHAR_DATA *victim)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (ch == NULL || victim == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_see_shift: called with null ch or victim!");
        return false;
    }

    if (IS_IMMORTAL(ch))
        return true;

    // Use the lworn LLIST
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_TRUESIGHT) &&
                ch->in_room == victim->in_room) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }

    return false;
}


// Is a person scary?
bool can_scare(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (ch == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_scare: NULL ch");
        return false;
    }

    if (IS_SHIFTED_SLAYER(ch))
        return true;

    // Use the lworn LLIST to check worn items
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_SCARE)) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }

    return false;
}


/* does a person have to eat or drink? returns true if not */
bool is_sustained(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (!ch || !ch->lworn)
        return false;

    // Use the lworn LLIST to check worn items
    iterator_start(&it, ch->lworn);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (IS_SET(obj->extra[1], ITEM_SUSTAIN)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}


/* is victim ignoring ch? */
bool is_ignoring(CHAR_DATA *victim, CHAR_DATA *ch)
{
    IGNORE_DATA *ignore;

    if (IS_IMMORTAL(ch) || IS_NPC(victim))
    return false;

    for (ignore = victim->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
    {
    if (!str_cmp(ignore->name, ch->name))
        return true;
    }

    return false;
}

/* does a person have both hands full */
bool both_hands_full(CHAR_DATA *ch)
{
    OBJ_DATA *w1,*w2,*sh,*h;

    w1 = get_eq_char(ch, WEAR_WIELD);
    w2 = get_eq_char(ch, WEAR_SECONDARY);
    sh = get_eq_char(ch, WEAR_SHIELD);
    h = get_eq_char(ch, WEAR_HOLD);

    /* wielding 2 weapons */
    if(w1 && w2) return true;

    /* a weapon and a shield/held item */
    if ((sh || h) && (w1 || w2)) return true;

    /* a two-handed weapon, but not for big races */
    if (w1 && ch->size < SIZE_HUGE && IS_WEAPON_STAT(w1,WEAPON_TWO_HANDS)) return true;

    /* a weapon and an item */
    /* a shield and an item */
    if(h && (w1 || w2 || sh)) return true;

    return false;
}


/* does a person have one hand full */
bool one_hand_full(CHAR_DATA *ch)
{
    if (get_eq_char(ch, WEAR_SHIELD) != NULL
    || get_eq_char(ch, WEAR_WIELD) != NULL
    || get_eq_char(ch, WEAR_HOLD) != NULL
    || get_eq_char(ch, WEAR_SECONDARY) != NULL)
    return true;

    return false;
}


/* is an item a relic, any relic */
bool is_relic(OBJ_INDEX_DATA *obj)
{
    if (!obj) return false;

    return (obj == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_DAMAGE")
        ||  obj == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_XP")
        ||  obj == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_PNEUMA")
        ||  obj == get_reserved_obj_index("OBJ_VNUM_RELIC_HP_REGEN")
        ||  obj == get_reserved_obj_index("OBJ_VNUM_RELIC_MANA_REGEN"));
}


/* is a room completely dislinked (no exits) ? */
bool is_dislinked(ROOM_INDEX_DATA *pRoom)
{
    EXIT_DATA *pExit;
    int i;

    i = 0;
    for (pExit = pRoom->exit[i]; i < MAX_DIR; i++)
    {
    if (pExit != NULL)
        return false;
    }

    return true;
}


/* is person's church good? */
bool is_good_church(CHAR_DATA *ch)
{
    if (ch->church == NULL)
    return false;

    if (ch->church->alignment == CHURCH_GOOD)
    return true;

    return false;
}


/* is person's church evil? */
bool is_evil_church(CHAR_DATA *ch)
{
    if (ch->church == NULL)
    return false;

    if (ch->church->alignment == CHURCH_EVIL)
    return true;

    return false;
}


/* is person wielding a weapon of a certain type? (spear, sword, whatever) */
bool wields_item_type(CHAR_DATA *ch, int weapon_type)
{
    OBJ_DATA *wield;
    OBJ_DATA *wield2;

    if ((wield = get_eq_char(ch, WEAR_WIELD)) != NULL)
    {
    if (WEAPON(wield)->weapon_class == weapon_type)
        return true;
    }

    if ((wield2 = get_eq_char(ch, WEAR_SECONDARY)) != NULL)
    {
        if (WEAPON(wield2)->weapon_class == weapon_type)
        return true;
    }

    return false;
}


char *pirate_name_generator(void)
{
    char *first_name[26] = {
        "Long",
        "Short",
        "Big",
        "Small",
        "Snot",
        "Grimy",
        "Lanky",
        "Burly",
        "Angry",
        "Furry",
        "Silly",
        "Dangle",
        "Wobble",
        "Red",
        "Blue",
        "Green",
        "Shiny",
        "Toothy",
        "Fearless",
        "Fat",
        "Baby",
        "Spice",
        "Happy",
        "Toked",
        "Anvil",
        "One-eyed" };

    char *middle_name[15] = {
        "Beard",
        "Hook",
        "Peg",
        "Tooth",
        "Belly",
        "Buckle",
        "Sword",
        "Armed",
        "Rex",
        "Bill",
        "Willy",
        "Monkey",
        "Trouser",
        "Brow",
        "Jim" };

    char *last_name[14] = {
        "Snake",
        "Silver",
        "Hook",
        "Tiger",
        "Lionheart",
        "The bad",
        "Smithers",
        "Pinkleton",
        "Burns",
        "Simpson",
        "Rohnscharch",
        "Diablo",
        "Parker",
        "Adamson"
    };
    int name = number_range(0, 25);
    int middle = number_range(0, 14);
    int last = number_range(0, 13);
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "%s %s %s", first_name[name], middle_name[middle], last_name[last]);

    return (char *) str_dup(buf);
}

/*
 * This looks at the landing coords.
 * Currently used if airship lands outside in Wilds.
 */
AREA_DATA *find_area_at_land_coords(int x, int y )
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
    if ( temp->land_x == x && temp->land_y == y )
        break;
    }

    if ( temp == NULL )
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Couldn't find area.");

    return temp;
}


/*
 * This looks at city coords.
 * Currently used if airship lands in city.
 */
AREA_DATA *find_area_at_coords(int x, int y )
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
    if ( temp->x == x && temp->y == y )
        break;
    }

    if ( temp == NULL )
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Couldn't find area.");

    return temp;
}


/* get remort race of a character based on their player race */
// Returns the RACE_DATA pointer for the remort destination race, or NULL if no remort available
RACE_DATA *get_remort_race(CHAR_DATA *ch)
{
    if (!ch || !ch->race)
        return NULL;

    /* If already a remort race, can't remort again */
    if (race_is_remort(ch->race))
        return NULL;

    return race_get_remort_into(ch->race);
}


/* is person dead? used for do_functions which check for this. */
bool is_dead(CHAR_DATA *ch)
{
    if (IS_DEAD(ch))
    {
        send_to_char("You can't do that. You are dead.\n\r", ch);
    return true;
    }

    return false;
}


/* deduct movement. makes some hackish adjustments */
void deduct_move(CHAR_DATA *ch, int amount)
{
    if (ch->tot_level < 30)
        amount /= 4;
    else if (ch->tot_level < 60)
        amount /= 3;
    else if (ch->tot_level < 90)
        amount /= 2;

    // athletics reduces movement usage
    if (number_percent() < get_skill(ch, skill_resolve_gsn("athletics")) / 8)
    {
    if (number_percent() == 1)
        check_improve(ch, skill_resolve_gsn("athletics"), true, 8);

    return;
    }

    amount = UMAX(amount, 1);

    ch->move -= amount;
    if (ch->move < 0)
    ch->move = 0;
}


/* is an item wearable on any part of the body? */
bool is_wearable(OBJ_DATA *obj)
{
    if(!(obj->wear_flags & ~ITEM_NONWEAR) && obj->item_type != ITEM_LIGHT)
        return false;

    return true;
}


/* is anyone resting, sleeping, etc on the obj? */
bool is_using_anyone(OBJ_DATA *obj)
{
    CHAR_DATA *ch;

    if (obj->in_room == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Got is_using_obj with obj->in_room NULL!");
    return false;
    }

    for (ch = obj->in_room->people; ch != NULL; ch = ch->next_in_room)
    {
    if (ch->on == obj)
        return true;
    }

    return false;
}


// Returns from maze spell
void return_from_maze(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA *pRoom = NULL;

    char_from_room(ch);

    do
    pRoom = get_random_room(ch, ANY_CONTINENT);
    while (pRoom == NULL);

    char_to_room(ch,pRoom);

    ch->maze_time_left = 0;

    act("{W$n plummets to the ground with a loud THUD!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}


/*
 * Move a char into an autowar team.
 */
void char_to_team(CHAR_DATA *ch)
{
    ch->next_in_auto_war    = auto_war->team_players;
    auto_war->team_players  = ch;
    ch->in_war              = true;
}


/*
 * Move a char from an autowar team.
 */
void char_from_team(CHAR_DATA *ch)
{
    if (auto_war == NULL)
    {
    return;
    }

    if (ch == auto_war->team_players)
    {
    auto_war->team_players = ch->next_in_auto_war;
    }
    else
    {
    CHAR_DATA *prev;

    for (prev = auto_war->team_players; prev != NULL;
          prev = prev->next_in_auto_war)
    {
        if (prev->next_in_auto_war == ch)
        {
        prev->next_in_auto_war = ch->next_in_auto_war;
        break;
        }
    }
    }
    ch->next_in_auto_war     = NULL;
    ch->in_war 		= false;
}


// return number of objects in container
int get_number_in_container(OBJ_DATA *obj)
{
    int i;
    OBJ_DATA *inside;

    i = 0;
    for (inside = obj->contains; inside != NULL; inside = inside->next_content)
    i++;

    return i;
}


// calculate the dp value of an obj
long get_dp_value(OBJ_DATA *obj)
{
    int deitypoints;
    OBJ_DATA *objnest;

    deitypoints = UMAX(1,obj->level * 3 + UMAX(obj->cost/1000, 1));

    if (obj->item_type != ITEM_CORPSE_NPC
    && obj->item_type != ITEM_CORPSE_PC)
    deitypoints = UMIN(deitypoints,obj->cost);

    if (obj->item_type == ITEM_MONEY)
    {
    deitypoints = MONEY(obj)->silver / 100;
    deitypoints += MONEY(obj)->gold;
    }

    for (objnest = obj->contains; objnest != NULL; objnest = objnest->next_content)
    deitypoints += get_dp_value(objnest);

    return deitypoints;
}


// used for the "qlist" so people may receive some tells while quiet
bool can_tell_while_quiet(CHAR_DATA *ch, CHAR_DATA *victim)
{
    STRING_DATA *string;

    if (ch == NULL || victim == NULL || IS_NPC(ch) || IS_NPC(victim))
    return false;

    for (string = victim->pcdata->quiet_people; string != NULL; string = string->next)
    {
    if (!str_cmp(string->string, ch->name))
        return true;
    }

    return false;
}


// check if ch can hunt victim
bool can_hunt(CHAR_DATA *ch, CHAR_DATA *victim)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (victim == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_hunt: victim was null!");
        return false;
    }

    // Use the lworn LLIST to check worn items
    if (victim->lworn) {
        iterator_start(&it, victim->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_NO_HUNT)) {
                iterator_stop(&it);
                return false;
            }
        }
        iterator_stop(&it);
    }

    return true;
}

int get_region_wyx(long wuid, int x, int y)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    WILDS_REGION *pRegion;

    if( wuid < 1 )
        return -1;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
        {
            if (pWilds->uid != wuid)
                continue;

            pRegion = get_region_by_coors(pWilds, x, y);
            if (pRegion != NULL)
                return pRegion->region;

            if (pWilds->defaultRegion != REGION_UNKNOWN)
                return pWilds->defaultRegion;

            switch (pWilds->defaultPlaceFlags)
            {
                case PLACE_FIRST_CONTINENT: return REGION_FIRST_CONTINENT;
                case PLACE_SECOND_CONTINENT: return REGION_SECOND_CONTINENT;
                case PLACE_THIRD_CONTINENT: return REGION_THIRD_CONTINENT;
                case PLACE_FOURTH_CONTINENT: return REGION_FOURTH_CONTINENT;
                default: break;
            }

            break;
        }
    }

    // Small Wilds
    if( wuid == 6 )
    {
        // Handle smallest regions FIRST
        if( x >= 193 && x <= 301 &&
            y >= 607 && y <= 683 )
            return REGION_MORDRAKE_ISLAND;

        if( x >= 103 && x <= 206 &&
            y >= 1043 && y <= 1153 )
            return REGION_DRAGON_ISLAND;

        if( x >= 1277 && x <= 1340 &&
            y >= 1132 && y <= 1185 )
            return REGION_TEMPLE_ISLAND;

        if( x >= 771 && x <= 895 &&
            y >= 537 && y <= 626 )
            return REGION_ARENA_ISLAND;

        if( x >= 1277 && x <= 1360 &&
            y >= 203 && y <= 253 )
            return REGION_UNDERSEA;

        // Smaller continents
        if( x >= 185 && x <= 517 &&
            y >= 335 && y <= 542 )
            return REGION_FIRST_CONTINENT;

        if( x >= 1011 && x <= 1488 &&
            y >= 746 && y <= 993 )
            return REGION_SECOND_CONTINENT;

        if( x >= 294 && x <= 674 &&
            y >= 741 && y <= 1078 )
            return REGION_THIRD_CONTINENT;

        // Polar Regions (since parts of the fourth continent overlaps)
        if( x >= 313 && x <= 454 &&
            y <= 97 )
            return REGION_NORTH_POLE;

        if( x >= 1419 && x <= 1532 &&
            y <= 92 )
            return REGION_NORTH_POLE;

        if( y <= 55 )
            return REGION_NORTH_POLE;

        if( x >= 469 && x <= 612 &&
            y >= 1125 )
            return REGION_SOUTH_POLE;

        if( y >= 1175 )
            return REGION_SOUTH_POLE;

        // Larger continents
        // Fourth's shape needs to be broken up into subregions
        if( x >= 717 && x <= 1469 &&
            y >= 0 && y <= 410 )
            return REGION_FOURTH_CONTINENT;

        if( x >= 393 && x <= 717 &&
            y >= 0 && y <= 247 )
            return REGION_FOURTH_CONTINENT;

        if( x >= 619 && x <= 717 &&
            y >= 248 && y <= 337 )
            return REGION_FOURTH_CONTINENT;

        if( x >= 925 && x <= 1133 &&
            y >= 410 && y <= 516 )
            return REGION_FOURTH_CONTINENT;

        // Oceans
        if( y <= 363 )
            return REGION_NORTHERN_OCEAN;

        if( y >= 937 )
            return REGION_SOUTHERN_OCEAN;

        if( x <= 371 )
            return REGION_WESTERN_OCEAN;

        if( x >= 1146 )
            return REGION_EASTERN_OCEAN;

        return REGION_CENTRAL_OCEAN;
    }

    return REGION_UNKNOWN;
}

int get_region_area(AREA_DATA *area)
{
    int region = REGION_UNKNOWN;
    if( area->wilds_uid > 0 )
        region = get_region_wyx(area->wilds_uid, area->x, area->y);


    if( region == REGION_UNKNOWN )
    {
        switch(area->place_flags)
        {
        case PLACE_FIRST_CONTINENT:		region = REGION_FIRST_CONTINENT; break;
        case PLACE_SECOND_CONTINENT:	region = REGION_SECOND_CONTINENT; break;
        case PLACE_THIRD_CONTINENT:		region = REGION_THIRD_CONTINENT; break;
        case PLACE_FOURTH_CONTINENT:	region = REGION_FOURTH_CONTINENT; break;
        }
    }

    return region;
}

// get region (roughly) in the wilds
int get_region(ROOM_INDEX_DATA *room)
{
    int rel_x;
    int rel_y;
    int region;

    if( !room )
        return REGION_UNKNOWN;

    if( !IS_WILDERNESS(room) )
        return get_region_area(room->area);

    if (!room->wilds)
        return REGION_UNKNOWN;

    rel_x = room->x - room->wilds->startx;
    rel_y = room->y - room->wilds->starty;
    region = get_wilds_effective_region(room->wilds, rel_x, rel_y);

    if (region != REGION_UNKNOWN)
        return region;

    return get_region_wyx(room->wilds->uid, room->x, room->y);
}

bool is_same_place_area(AREA_DATA *from, AREA_DATA *to)
{
    int region_from = get_region_area(from);

    if( region_from == REGION_UNKNOWN ) return false;

    int region_to = get_region_area(to);

    return region_from == region_to;
}

bool is_same_place(ROOM_INDEX_DATA *from, ROOM_INDEX_DATA *to)
{
    if( IS_WILDERNESS(from) )
    {
        int region = get_region(from);

        if( region == REGION_FIRST_CONTINENT )
            return to->area->place_flags == PLACE_FIRST_CONTINENT;

        if( region == REGION_SECOND_CONTINENT )
            return to->area->place_flags == PLACE_SECOND_CONTINENT;

        if( region == REGION_THIRD_CONTINENT )
            return to->area->place_flags == PLACE_THIRD_CONTINENT;

        if( region == REGION_FOURTH_CONTINENT )
            return to->area->place_flags == PLACE_FOURTH_CONTINENT;

    }
    else if( from->area->place_flags != PLACE_NOWHERE )
    {
        return from->area->place_flags == to->area->place_flags;
    }

    return false;
}


int get_continent( const char *name )
{
    if( IS_NULLSTR(name) ) return -1;

    if( !str_prefix(name, "any") ) return ANY_CONTINENT;
    if( !str_prefix(name, "athemia") ) return SECOND_CONTINENT;
    if( !str_prefix(name, "east") ) return EAST_CONTINENTS;
    if( !str_prefix(name, "first") ) return FIRST_CONTINENT;
    if( !str_prefix(name, "fourth") ) return FOURTH_CONTINENT;
    if( !str_prefix(name, "heletane") ) return FOURTH_CONTINENT;
    if( !str_prefix(name, "naranda") ) return THIRD_CONTINENT;
    if( !str_prefix(name, "north") ) return NORTH_CONTINENTS;
    if( !str_prefix(name, "second") ) return SECOND_CONTINENT;
    if( !str_prefix(name, "seralia") ) return FIRST_CONTINENT;
    if( !str_prefix(name, "south") ) return SOUTH_CONTINENTS;
    if( !str_prefix(name, "third") ) return THIRD_CONTINENT;
    if( !str_prefix(name, "west") ) return WEST_CONTINENTS;

    return ANY_CONTINENT;
}

/* NONUSED
bool is_on_second_continent(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA *room;

    if (ch->in_room == NULL)
    return false;

    if (str_cmp(ch->in_room->area->name, "Wilderness"))
        return false;

    room = ch->in_room;
    if (room->x > 749
    &&   room->x < 1200
    &&   room->y > 80
    &&   room->y < 295)
    return true;

    return false;
}
*/

bool check_ice_storm(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;

    if (room == NULL)
    {
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "check_ice_storm: room was null");
    return false;
    }

    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == get_reserved_obj_index("obj_spell_icestorm"))
        return true;
    }

    return false;
}


// does a player with said name exist (ie do they have a pfile)
bool player_exists(char *argument)
{
    char player_name[MSL];
    char player_dir_buf[MSL];
    const char *player_dir;
    bool found_char = false;
    FILE *fp;

    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(player_name, sizeof(player_name), "%s%c/%s", player_dir, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(player_name, "r")) == NULL)
    found_char = false;
    else
    {
    found_char = true;
    fclose (fp);
    }

    return found_char;
}

// checks if an account with the given name exists
bool account_exists(char *argument)
{
    char account_name[MSL];
    char account_dir_buf[MSL];
    const char *account_dir;
    bool found_account = false;
    FILE *fp;

    account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));
    snprintf(account_name, sizeof(account_name), "%s%c/%s", account_dir, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(account_name, "r")) == NULL)
        found_account = false;
    else
    {
        found_account = true;
        fclose(fp);
    }

    return found_account;
}

/*
 * Find a skull of a person in ch's inv. Looks in containers.
 */
OBJ_DATA *get_skull(CHAR_DATA *ch, char *owner)
{
    OBJ_DATA *obj;
    OBJ_DATA *objNest;
    ITERATOR it;
    
    if (!ch || !owner || !*owner)
        return NULL;
        
    // First check in character's carried items
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Check containers first
            if (obj->contains) {
                // Look inside containers
                for (objNest = obj->contains; objNest != NULL; objNest = objNest->next_content) {
                    if (can_see_obj(ch, objNest)
                    && (objNest->pIndexData == get_reserved_obj_index("obj_skull_normal")
                        || objNest->pIndexData == get_reserved_obj_index("obj_skull_golden"))
                    && !str_cmp(objNest->owner, owner)) {
                        iterator_stop(&it);
                        return objNest;
                    }
                }
            }

            // Check if this is a skull with matching owner
            if (can_see_obj(ch, obj)
            && (obj->pIndexData == get_reserved_obj_index("obj_skull_normal")
                || obj->pIndexData == get_reserved_obj_index("obj_skull_golden"))
            && !str_cmp(obj->owner, owner)) {
                iterator_stop(&it);
                return obj;
            }
        }
        iterator_stop(&it);
    }
    
    // Also check worn items that might be containers (like pouches, bags)
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (obj->contains) {
                for (objNest = obj->contains; objNest != NULL; objNest = objNest->next_content) {
                    if (can_see_obj(ch, objNest)
                    && (objNest->pIndexData == get_reserved_obj_index("obj_skull_normal")
                        || objNest->pIndexData == get_reserved_obj_index("obj_skull_golden"))
                    && !str_cmp(objNest->owner, owner)) {
                        iterator_stop(&it);
                        return objNest;
                    }
                }
            }
        }
        iterator_stop(&it);
    }

    return NULL;
}


int count_exits(ROOM_INDEX_DATA *room)
{
    int exits = 0;

    for (int n = 0; n < MAX_DIR; n++)
    {
        EXIT_DATA *exit = room->exit[n];
        if (exit != NULL && exit->u1.to_room != NULL)
            exits++;
    }

    return exits;
}

// Is the room PK? (allows for ignoring ARENA)
bool is_room_pk(ROOM_INDEX_DATA *room, bool arena)
{
    if( room != NULL ) {
        if( IS_SET(room->room_flag[0], ROOM_PK) )
            return true;

        // Only count ARENA (LPK) if desired
        if( arena && IS_SET(room->room_flag[0], ROOM_ARENA) )
            return true;
    }

    return false;
}

// Is the room full CPK? (both player_killing and chaotic)
bool is_room_full_cpk(ROOM_INDEX_DATA *room)
{
    return room != NULL
        && IS_SET(room->room_flag[0], ROOM_PK)
        && IS_SET(room->room_flag[0], ROOM_CHAOTIC);
}

// Is a person PK? Covers all possible cases (PK flag, room PK, etc)
bool is_pk(CHAR_DATA *ch)
{
    CHAR_DATA *fch;

    if (ch->church != NULL && ch->church->pk == true)
    return true;

    if (is_room_pk(ch->in_room, false))
        return true;

    if (ch->pk_timer > 0)
        return true;

    if (IS_SET(ch->act[0], PLR_PK))
    return true;

    // If you pull a relic you are also PK
    if (ch->pulled_cart != NULL
    &&   is_relic(ch->pulled_cart->pIndexData))
    return true;

   // If you pull a relic your form members are PK as well
   for (fch = ch->in_room->people; fch != NULL; fch = fch->next_in_room)
   {
       if (fch->pulled_cart != NULL
       &&   is_relic(fch->pulled_cart->pIndexData)
       &&   is_same_group(ch, fch))
       return true;
   }

   return false;
}


// Get direction number from its character-string description.
int get_num_dir(char *arg)
{
    int door;

     if (!str_cmp(arg, "n") || !str_cmp(arg, "north")) door = 0;
    else if (!str_cmp(arg, "e") || !str_cmp(arg, "east" )) door = 1;
    else if (!str_cmp(arg, "s") || !str_cmp(arg, "south")) door = 2;
    else if (!str_cmp(arg, "w") || !str_cmp(arg, "west" )) door = 3;
    else if (!str_cmp(arg, "u") || !str_cmp(arg, "up"   )) door = 4;
    else if (!str_cmp(arg, "d") || !str_cmp(arg, "down" )) door = 5;
    else if (!str_cmp(arg, "ne") || !str_cmp(arg, "northeast" )) door = 6;
    else if (!str_cmp(arg, "nw") || !str_cmp(arg, "northwest" )) door = 7;
    else if (!str_cmp(arg, "se") || !str_cmp(arg, "southeast" )) door = 8;
    else if (!str_cmp(arg, "sw") || !str_cmp(arg, "southwest" )) door = 9;
    else door = -1;

    return door;
}


// Dislink a room completely. Returns true if anything was changed.
bool dislink_room(ROOM_INDEX_DATA *pRoom)
{
    int i;
    char cmd[MSL];
    bool changed = false;

    for (i = 0; i < MAX_DIR; i++)
    {
    if (pRoom->exit[i] != NULL
    &&   pRoom->exit[i]->u1.to_room != NULL)
    {
        sprintf(cmd, "%ld delete", pRoom->vnum);
        rp_change_exit(pRoom, cmd, i);
        changed = true;
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "dislink_room: dislinked room %s (%ld)",
            pRoom->name, pRoom->vnum);
    }
    }

    return changed;
}


// Used for druids.
bool is_in_nature(CHAR_DATA *ch)
{
    if (ch == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "is_in_nature: ch null");
    return false;
    }

    if (ch->in_room == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "is_in_nature: ch->in_room null");
    return false;
    }

    switch(room_sector_type(ch->in_room))
    {
    case SECT_FIELD:
    case SECT_FOREST:
    case SECT_HILLS:
    case SECT_MOUNTAIN:
    case SECT_WATER_SWIM:
    case SECT_TUNDRA:
        return true;

    default:
        return false;
    }
}


// Is there a PK room within a certain range of this one
int is_pk_safe_range(ROOM_INDEX_DATA *room, int depth, int reverse_dir)
{
    EXIT_DATA *ex;
    ROOM_INDEX_DATA *to_room;
    int dir;

    /*
    if (reverse_dir != -1 && (IS_SET(room->room_flag[0], ROOM_PK)
    ||   IS_SET(room->room_flag[0], ROOM_CHAOTIC)))
        return rev_dir[reverse_dir];
     */

    if (depth == 0)
    {
    if (IS_SET(room->room_flag[0], ROOM_PK)
    )
        return 10;

    else
        return -1;
    }

    for (dir = 0; dir < MAX_DIR; dir++)
    {
        if (dir != reverse_dir
    && (ex = room->exit[dir]) != NULL
    &&     (to_room = room->exit[dir]->u1.to_room) != NULL)
    {
        if (is_pk_safe_range(to_room, depth - 1, rev_dir[dir]) > -1)
            return dir;

        if (IS_SET(to_room->room_flag[0], ROOM_PK)
        )
        return dir;
    }

    }

    return -1;
}


// is person pulling a relic, any relic
bool is_pulling_relic(CHAR_DATA *ch)
{
    if (ch == NULL)
        return false;

    if (ch->pulled_cart != NULL
    &&   is_relic(ch->pulled_cart->pIndexData))
        return true;

    return false;
}


// Lookup a class, subclass, or second subclass.
int get_profession(CHAR_DATA *ch, int class_type)
{
    if (IS_NPC(ch))
    return CLASS_NPC;

    if (ch->pcdata == NULL)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_profession: null pcdata on a pc");
    return CLASS_NPC;
    }

    switch (class_type)
    {
        case CLASS_CURRENT:		return ch->pcdata->class_current;
    case SUBCLASS_CURRENT:		return ch->pcdata->sub_class_current;

    case CLASS_MAGE:		return ch->pcdata->class_mage;
    case CLASS_CLERIC:		return ch->pcdata->class_cleric;
    case CLASS_THIEF:		return ch->pcdata->class_thief;
    case CLASS_WARRIOR:		return ch->pcdata->class_warrior;

    case SECOND_CLASS_MAGE:		return ch->pcdata->second_class_mage;
    case SECOND_CLASS_CLERIC:	return ch->pcdata->second_class_cleric;
    case SECOND_CLASS_THIEF:	return ch->pcdata->second_class_thief;
    case SECOND_CLASS_WARRIOR:	return ch->pcdata->second_class_warrior;

    case SUBCLASS_MAGE:		return ch->pcdata->sub_class_mage;
    case SUBCLASS_CLERIC:		return ch->pcdata->sub_class_cleric;
    case SUBCLASS_THIEF:		return ch->pcdata->sub_class_thief;
    case SUBCLASS_WARRIOR:		return ch->pcdata->sub_class_warrior;

    case SECOND_SUBCLASS_MAGE:	return ch->pcdata->second_sub_class_mage;
    case SECOND_SUBCLASS_CLERIC:	return ch->pcdata->second_sub_class_cleric;
    case SECOND_SUBCLASS_THIEF:	return ch->pcdata->second_sub_class_thief;
    case SECOND_SUBCLASS_WARRIOR:	return ch->pcdata->second_sub_class_warrior;

    default:
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "get_profession: bad class_type %d", class_type);
        return CLASS_NPC;
    }
}


// Set a class, subclass, etc on someone.
void set_profession(CHAR_DATA *ch, int class_type, int class_value)
{
    if (IS_NPC(ch))
    return;

    if (ch->pcdata == NULL) {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: null pcdata on a pc");
    return;
    }

    switch (class_type)
    {
    case CLASS_MAGE:
        if (class_value == CLASS_MAGE)
        ch->pcdata->class_mage = CLASS_MAGE;
        else
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-mage class for mage class spot");

        break;

    case CLASS_CLERIC:
        if (class_value == CLASS_CLERIC)
        ch->pcdata->class_mage = CLASS_CLERIC;
        else
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-cleric class for cleric class spot");

        break;

    case CLASS_THIEF:
        if (class_value == CLASS_THIEF)
        ch->pcdata->class_mage = CLASS_THIEF;
        else
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-thief class for thief class spot");

        break;

    case CLASS_WARRIOR:
        if (class_value == CLASS_WARRIOR)
        ch->pcdata->class_mage = CLASS_WARRIOR;
        else
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-warrior class for warrior class spot");

        break;

    case SUBCLASS_MAGE:
        switch (class_value)
        {
        case CLASS_MAGE_NECROMANCER:	ch->pcdata->sub_class_mage = CLASS_MAGE_NECROMANCER; 	break;
        case CLASS_MAGE_SORCERER:	ch->pcdata->sub_class_mage = CLASS_MAGE_SORCERER;	break;
        case CLASS_MAGE_WIZARD:		ch->pcdata->sub_class_mage = CLASS_MAGE_WIZARD;		break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-mage subclass for mage subclass spot");
            break;
        }

    case SUBCLASS_CLERIC:
        switch (class_value)
        {
        case CLASS_CLERIC_WITCH:	ch->pcdata->sub_class_cleric = CLASS_CLERIC_WITCH; 	break;
        case CLASS_CLERIC_DRUID:	ch->pcdata->sub_class_cleric = CLASS_CLERIC_DRUID;	break;
        case CLASS_CLERIC_MONK:		ch->pcdata->sub_class_cleric = CLASS_CLERIC_MONK;	break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-cleric subclass for cleric subclass spot");
            break;
        }

    case SUBCLASS_THIEF:
        switch (class_value)
        {
        case CLASS_THIEF_ASSASSIN:	ch->pcdata->sub_class_thief = CLASS_THIEF_ASSASSIN; 	break;
        case CLASS_THIEF_ROGUE:		ch->pcdata->sub_class_thief = CLASS_THIEF_ROGUE;	break;
        case CLASS_THIEF_BARD:		ch->pcdata->sub_class_thief = CLASS_THIEF_BARD;		break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-thief subclass for thief subclass spot");
            break;
        }
    case SUBCLASS_WARRIOR:
        switch (class_value)
        {
        case CLASS_WARRIOR_MARAUDER:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_MARAUDER; 	break;
        case CLASS_WARRIOR_GLADIATOR:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_GLADIATOR;	break;
        case CLASS_WARRIOR_PALADIN:	ch->pcdata->sub_class_warrior = CLASS_WARRIOR_PALADIN;		break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-warrior subclass for warrior subclass spot");
            break;
        }

    case SECOND_SUBCLASS_MAGE:
        switch (class_value)
        {
        case CLASS_MAGE_ARCHMAGE:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_ARCHMAGE; 	break;
        case CLASS_MAGE_GEOMANCER:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_GEOMANCER;	break;
        case CLASS_MAGE_ILLUSIONIST:	ch->pcdata->second_sub_class_mage = CLASS_MAGE_ILLUSIONIST;	break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-mage subclass for mage second subclass spot");
            break;
        }

    case SECOND_SUBCLASS_CLERIC:
        switch (class_value)
        {
        case CLASS_CLERIC_ALCHEMIST:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ALCHEMIST; 	break;
        case CLASS_CLERIC_RANGER:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_RANGER;	break;
        case CLASS_CLERIC_ADEPT:	ch->pcdata->second_sub_class_cleric = CLASS_CLERIC_ADEPT;	break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-cleric subclass for cleric second subclass spot");
            break;
        }

    case SECOND_SUBCLASS_THIEF:
        switch (class_value)
        {
        case CLASS_THIEF_HIGHWAYMAN:	ch->pcdata->second_sub_class_thief = CLASS_MAGE_ARCHMAGE; 	break;
        case CLASS_THIEF_NINJA:		ch->pcdata->second_sub_class_thief = CLASS_THIEF_NINJA;		break;
        case CLASS_THIEF_SAGE:		ch->pcdata->second_sub_class_thief = CLASS_THIEF_SAGE;		break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-thief subclass for thief second subclass spot");
            break;
        }

    case SECOND_SUBCLASS_WARRIOR:
        switch (class_value)
        {
        case CLASS_WARRIOR_WARLORD:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_WARLORD; 	break;
        case CLASS_WARRIOR_DESTROYER:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_DESTROYER;	break;
        case CLASS_WARRIOR_CRUSADER:	ch->pcdata->second_sub_class_warrior = CLASS_WARRIOR_CRUSADER;	break;
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: trying to set a non-warrior subclass for warrior second subclass spot");
            break;
        }

    default:
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_profession: bad class_type %d", class_type);
        return;
    }
}


// Find out what room an obj is 'in'.
ROOM_INDEX_DATA *obj_room(OBJ_DATA *obj)
{
    OBJ_DATA *container;
    CHAR_DATA *ch;

    if (!obj) return NULL;

    // In-locker and in-mail objects don't technically have a room.
    if (obj->locker || obj->in_mail)
    return NULL;

    // It's inside a container
    if ((container = obj->in_obj) != NULL)
    return obj_room(container);

    if ((ch = obj->carried_by) != NULL)
    return ch->in_room;

    if (obj->in_room != NULL)
    return obj->in_room;

    return NULL;
}

// Find out what room a token is 'in'.
ROOM_INDEX_DATA *token_room(TOKEN_DATA *token)
{
    if(token->player)
        return token->player->in_room;

    if(token->object)
        return obj_room(token->object);

    if(token->room)
        return token->room;

    return NULL;
}



void exit_name(ROOM_INDEX_DATA *room, int door, char *kwd)
{
    EXIT_DATA *ex;
    char buf[MSL];
    char article[MSL];

    if (room == NULL) {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "exit_name: room was null");
    return;
    }

    if ((ex = room->exit[door]) == NULL) {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "exit_name: null exit");
    return;
    }

    switch (ex->keyword[0]) {
    case '\0':
        switch (door) {
        case 4:
        case 5:		sprintf(kwd, "the %swards entrance", dir_name[door]); break;
        default:	sprintf(kwd, "the %s entrance", dir_name[door]); break;
        }
            break;

    case ' ':
        sprintf(kwd, "%s", ex->keyword + 1);
        break;

    default:
         one_argument(ex->keyword, buf);
        switch (UPPER(buf[0]))
        {
        case 'A':
        case 'E':
        case 'I':
        case 'O':
        case 'U':
            sprintf(article, "an");
            break;

        default:
            sprintf(article, "a");
        }

        sprintf(kwd, "%s %s", article, buf);
        break;
    }
}


bool can_give_obj(CHAR_DATA *ch, OBJ_DATA *obj, CHAR_DATA *victim, bool silent)
{
    if (!ch)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_give_obj: ch NULL.");
    return false;
    }

    if (!obj)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_give_obj: obj NULL.");
    return false;
    }

    if (!victim)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_give_obj: victim NULL.");
    return false;
    }

    if (!can_see_obj(ch, obj))
    return false;

    if (obj->wear_loc != WEAR_NONE)
    {
    if (!silent)
        act("You'll have to remove $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (IS_SET(obj->extra[1], ITEM_SINGULAR)
    &&  get_obj_vnum_carry(victim, obj->pIndexData->vnum, victim) != NULL)
    {
    if (!silent)
        act("A mysterious force prevents you from giving $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (IS_NPC(victim) && victim->shop != NULL)
    {
    if (!silent)
        act("{R$N tells you 'Sorry, you'll have to sell that.{x'", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
    if (!silent)
        send_to_char("You can't let go of it.\n\r", ch);

    return false;
    }

    /*
    if (victim->carry_number + get_obj_number(obj) > can_carry_n(victim))
    {
    if (!silent)
        act("$N has $S hands full.", ch, NULL, victim, TO_CHAR);

    return false;
    }

    if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim))
    MSG(act("$N can't carry that much weight.", ch, NULL, victim, TO_CHAR))
     */

    return true;
}


bool can_drop_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent)
{
    if (!ch && !silent)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_drop_obj: ch NULL while not silent.");
    return false;
    }

    if (!obj)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_drop_obj: obj NULL.");
    return false;
    }

    if (!can_see_obj(ch, obj))
    return false;

/*  Syn - this shouldn't be here, since it essentially allows people to select
    which items they don't want stolen (and I'm sure there's other abuses of it too).
    Kept should be assigned specifically in functions which are called by the user
    of that item only such as give, drop, donate, etc etc
    if (IS_SET(obj->extra[1], ITEM_KEPT))
    {
    if (!silent)
        act("$p has been marked for keeping. Type \"keep <item>\" to unmark it.", ch, obj, NULL, TO_CHAR);

    return false;
    }
*/
    if (obj->wear_loc != WEAR_NONE)
    {
    if (!silent)
        act("You must remove $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (IS_IMMORTAL(ch))
    return true;

    if (IS_SOCIAL(ch))
    {
    if (!silent)
        send_to_char("You can't drop items here.\n\r", ch);

    return false;
    }

    if (IS_SET(obj->extra[0], ITEM_NODROP))
    {
    if (!silent)
        act("You can't let go of $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    return true;
}


bool can_get_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent)
{
    CHAR_DATA *gch;
    long event_uid = 0;
    uint32_t instance_id = 0;

    if (!ch)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_get_obj: ch NULL.");
    return false;
    }

    if (!obj)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_get_obj: obj NULL.");
    return false;
    }

    if (mail && container)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_get_obj: received mail and container");
    return false;
    }

    if (container && obj->in_obj != container)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_get_obj: obj not in container");
    return false;
    }

    if (!can_see_obj(ch, obj))
    return false;

    if (!IS_NPC(ch)
    && event_get_object_spawn_source(obj, &event_uid, &instance_id)
    && event_uid > 0
    && !event_runtime_ensure_participation_for_action(ch, event_uid, instance_id, "collection", !silent))
        return false;

    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
    MSG(act("$p: you can't carry that many items.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL))

    if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
    MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL))

    if (IS_SET(obj->extra[1], ITEM_SINGULAR)
    &&  get_obj_vnum_carry(ch, obj->pIndexData->vnum, ch) != NULL)
    {
    if (!silent)
        act("A mysterious force prevents you from picking up $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (container)
    {
    switch (container->item_type)
    {
        default:
            if (!silent)
            send_to_char("That's not a container.\n\r", ch);

        return false;

        case ITEM_CART:
        case ITEM_CONTAINER:
        case ITEM_WEAPON_CONTAINER:
        case ITEM_CORPSE_NPC:
        case ITEM_CORPSE_PC:
        case ITEM_KEYRING:
        break;
    }

    if (container->item_type == ITEM_CART && get_cart_pulled(container) != ch)
    {
        if (!silent)
        {
        act("You can't take items from $N's cart.", ch, get_cart_pulled(container), NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }

        return false;
    }

    if (container->item_type == ITEM_CONTAINER
    &&  IS_SET(CONTAINER(container)->flags, CONT_CLOSED))
    {
        if (!silent)
        act("The $d is closed.", ch, NULL, NULL, NULL, NULL, NULL, container->name, TO_CHAR, NULL, NULL);

        return false;
    }

    if(p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,obj,NULL,TRIG_PREGET,silent?"silent":NULL))
        return false;

    }

    if (mail)
    {
    if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
        MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL))
    }

    // Get an item from the ground or from a container on the ground
    if (!container || container->carried_by != ch)
    {
    if (!IS_SET(obj->wear_flags, ITEM_TAKE) || obj->item_type == ITEM_CORPSE_PC)
    {
        if (!silent)
        send_to_char("You can't take that.\n\r", ch);

        return false;
    }

    if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
        MSG(act("$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL))

    if (obj_room(obj))
    {
        for (gch = obj_room(obj)->people; gch != NULL; gch = gch->next_in_room)
        {
        if (gch->on == obj)
        {
            if (!silent)
            act("$N appears to be using $p.", ch, gch, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

            return false;
        }
        }
    }

    if (obj->item_type == ITEM_CART)
    {
        if (!silent)
        act("$p is far too heavy.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        return false;
    }
    }

    return !p_percent_trigger(NULL,obj,NULL,NULL,ch, NULL, NULL,container,NULL,TRIG_PREGET,silent?"silent":NULL);
}


bool can_put_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent)
{
    char buf[MAX_STRING_LENGTH];
    int weight;

    if (!ch && !silent)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_put_obj: ch NULL when not silent.");
    return false;
    }

    if (!obj)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_put_obj: obj NULL.");
    return false;
    }

    if (!container && !mail)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_put_obj: container AND mail NULL.");
    return false;
    }

    if (container && mail)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "can_put_obj: received container and mail");
    return false;
    }

    if (obj->wear_loc != WEAR_NONE)
    {
    if (!silent)
        send_to_char("You must remove it first.\n\r", ch);

    return false;
    }

    if (container)
    {
    // To put it in a container on the ground you must be able to drop it
    if (container->carried_by != ch && (!can_drop_obj(ch, obj, silent) || IS_SET(obj->extra[1], ITEM_KEPT))) {
        if (!silent)
        send_to_char("You can't let go of it.\n\r", ch);
        return false;
    }

    if (container->item_type != ITEM_CONTAINER
    &&  container->item_type != ITEM_WEAPON_CONTAINER
    &&  container->item_type != ITEM_CART
    &&  container->item_type != ITEM_KEYRING
    &&  container->pIndexData != get_reserved_obj_index("OBJ_VNUM_CURSED_ORB"))
    {
        if (!silent)
        send_to_char("That's not a container or cart.\n\r", ch);

        return false;
    }

    if (container->item_type != ITEM_CART
    &&  container->item_type != ITEM_WEAPON_CONTAINER
    &&  IS_SET(CONTAINER(container)->flags, CONT_CLOSED))
    {
        if (!silent)
        act("$p is closed.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    if (obj == container)
    {
        if (!silent)
        send_to_char("You can't fold it into itself.\n\r", ch);

        return false;
    }

    if (obj->item_type == ITEM_CONTAINER
    ||  obj->item_type == ITEM_WEAPON_CONTAINER)
    {
        if (!silent)
        send_to_char("You can't put containers inside another container.\n\r", ch);

        return false;
    }

    if (IS_SET(obj->extra[1], ITEM_NO_CONTAINER) ||
        (IS_SET(obj->extra[0], ITEM_NOUNCURSE) && IS_SET(obj->extra[0], ITEM_NODROP)))
    {
        if (!silent)
        act("You can't put $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);

        return false;
    }

    if (WEIGHT_MULT(obj) != 100)
        return false;

        /*
    Legacy slot-based container capacity check retained here for reference.
    Runtime checks now use typed container data paths.
    */

    if (container->item_type == ITEM_WEAPON_CONTAINER
    &&  WEAPON_CON(container)->weapon_type != WEAPON(obj)->weapon_class)
    {
        if (!silent)
        {
        sprintf(buf, "This container will only take %ss.\n\r",
            weapon_name(WEAPON_CON(container)->weapon_type));
        send_to_char(buf, ch);
        }

        return false;
    }
    }
    else
    {
    if (obj->timer > 0)
    {
        if (!silent)
        act("You can't send $p through the mail.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        return false;
    }

    if ((obj->pIndexData == get_reserved_obj_index("obj_skull_normal") || obj->pIndexData == get_reserved_obj_index("obj_skull_golden"))
    &&   obj->affected != NULL)
    {
        if (!silent)
        send_to_char("You can't send that enchanted item through the mail.\n\r", ch);

        return false;
    }

    if (IS_SET(obj->extra[0], ITEM_NOUNCURSE) && IS_SET(obj->extra[0], ITEM_NODROP))
    {
        if (!silent)
        act("You can't let go of $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        return false;
    }

    weight = get_obj_weight(obj);
    if (weight > MAX_POSTAL_WEIGHT)
    {
        if (!silent)
        send_to_char("Sorry, that item is far too heavy to send through the mail.\n\r", ch);

        return false;
    }

    if (count_weight_mail(mail) + weight > MAX_POSTAL_WEIGHT)
        MSG(send_to_char("Postal weight limit has been reached.\n\r", ch))

    if (count_items_list_nest(mail->objects)
        + count_items_list_nest(obj->contains) + 1 > MAX_POSTAL_ITEMS)
    {
        sprintf(buf, "Postal limit of %d items per package has been reached.\n\r", MAX_POSTAL_ITEMS);
        MSG(send_to_char(buf, ch))
    }

    }

    return !p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,obj,NULL,TRIG_PREPUT,silent?"silent":NULL);
}


bool can_sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent)
{
    CHAR_DATA *gch;

    if (!can_see_obj(ch, obj))
    return false;

    if ((!IS_SET(obj->wear_flags, ITEM_TAKE) && obj->item_type != ITEM_CORPSE_NPC && obj->item_type != ITEM_CORPSE_PC)
    ||  IS_SET(obj->wear_flags, ITEM_NO_SAC)
    ||  (obj->item_type == ITEM_CORPSE_PC && obj->contains))
    {
    if (!silent)
        act("$p is not an acceptable sacrifice.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if ((obj->item_type == ITEM_CORPSE_NPC || obj->item_type == ITEM_CORPSE_PC)
    && obj->contains && !IS_SET(ch->act[1], PLR_SACRIFICE_ALL))
    {
    if (!silent)
        act("You must rid $p of its belongings before sacrificing it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return false;
    }

    if (obj_room(obj))
    {
    for (gch = obj_room(obj)->people; gch != NULL; gch = gch->next_in_room)
    {
        if (gch->on == obj)
        {
        if (!silent)
            act("$N appears to be using $p.", ch, gch, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        return false;
        }

    }
    }

    return true;
}

CHAR_DATA* get_player(char *name)
{
    ITERATOR it;
    CHAR_DATA *ch = NULL;

    iterator_start(&it, loaded_chars);
    while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (!IS_NPC(ch) && !str_cmp(ch->name, name))
            break;
    }
    iterator_stop(&it);
    return ch;
}

// get reputation for player
int16_t  get_player_reputation args( ( int reputation_points ) )
{
    int i = 0;
    int reputation_type = 0;
    while (rating_table[i].name != NULL) {
        if (reputation_points >= rating_table[i].points) {
            reputation_type = rating_table[i].type;
        }
        i++;
    }
    return reputation_type;
}

int get_coord_distance( int x1, int y1, int x2, int y2 ) {
  int distance = 0;
         distance = (int) sqrt( 					\
                            ( x1 - x2 ) *	\
                            ( x1 - x2 ) +	\
                            ( y1 - y2 ) *	\
                            ( y1 - y2 ) );

  return distance;
}


// Find the closest saferoom around a player. Used when repopping people into churches.
ROOM_INDEX_DATA *find_safe_room(ROOM_INDEX_DATA *from_room, int depth, bool crossarea)
{
    int door;
    ROOM_INDEX_DATA *room = NULL;

    if (depth == 0)
    return NULL;

    if (IS_SET(from_room->room_flag[0], ROOM_SAFE))
    return from_room;

    for (door = 0; door < MAX_DIR; door++)
    {
    if (from_room->exit[door] != NULL
    &&  from_room->exit[door]->u1.to_room != NULL)
    {
        if (!crossarea && from_room->exit[door]->u1.to_room->area != from_room->area)
        ;
        else
        room = find_safe_room(from_room->exit[door]->u1.to_room, depth - 1, crossarea);
    }

    if (room != NULL)
        return room;
    }

    return NULL;
}


// Checks if there is a PC or wilds wandering NPC in a room. Used in managing wilderness exits.
bool can_clear_exit(ROOM_INDEX_DATA *room)
{
    CHAR_DATA *rch;

    for (rch = room->people; rch != NULL; rch = rch->next_in_room) {
    if (!IS_NPC(rch) || IS_SET(rch->act[1], ACT2_WILDS_WANDERER))
        return false;
    }

    return true;
}

TOKEN_DATA *create_token(TOKEN_INDEX_DATA *token_index)
{
    TOKEN_DATA *token;
    int i;

    token = new_token();
    token->pIndexData = token_index;
    token->type = token_index->type;
    token->name = str_dup(token_index->name);
    token->description = str_dup(token_index->description);
    token->flags = token_index->flags;
    token->timer = token_index->timer;
    token->progs = new_prog_data();
    token->progs->progs = token_index->progs;
    token_index->loaded++;	// @@@NIB : 20070127 : for "tokenexists" ifcheck
    token->id[0] = token->id[1] = 0;
    token->global_next = global_tokens;
    global_tokens = token;

    get_token_id(token);

    variable_copylist(&token_index->index_vars,&token->progs->vars,false);
    variables_resolve_rsg_bindings(&token->progs->vars);

    free_string(token->name);
    token->name = variables_expand_text_dup(token->progs->vars, token_index->name);
    free_string(token->description);
    token->description = variables_expand_text_dup(token->progs->vars, token_index->description);

    for (i = 0; i < MAX_TOKEN_VALUES; i++)
        token->value[i] = token_index->value[i];

    return token;
}


/* set up a token and give it to a char */
TOKEN_DATA *give_token(TOKEN_INDEX_DATA *token_index, CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room)
{
    TOKEN_DATA *token;

    if(!ch && !obj && !room) return NULL;

    if( (ch && obj) || (ch && room) || (obj && room) ) return NULL;

    token = create_token(token_index);

    if(ch)
        token_to_char(token, ch);
    else if(obj)
        token_to_obj(token, obj);
    else if(room)
        token_to_room(token, room);

    return token;
}


void token_from_char(TOKEN_DATA *token)
{
    TOKEN_DATA *token_tmp, *token_prev;

    if (token->player == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_char: called on token with no player");
        return;
    }

    token_prev = NULL;
    for (token_tmp = token->player->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
        if (token_tmp == token)
            break;

        token_prev = token_tmp;
    }

    if(token->player->cast_token == token)
        stop_casting(token->player,true);

    if(token->player->script_wait_token == token)
        script_end_failure(token->player, true);

    if(token->type == TOKEN_SKILL) skill_entry_removeskill(token->player, 0, token);
    else if(token->type == TOKEN_SPELL) skill_entry_removespell(token->player, 0, token);
    else if(token->type == TOKEN_SONG) skill_entry_removesong(token->player, NULL, token);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_from_char: removed token %s(%ld) from char %s(%ld)",
        token->name, token->pIndexData->vnum,
        HANDLE(token->player), IS_NPC(token->player) ? token->player->pIndexData->vnum : 0);

    list_remlink(token->player->ltokens, token, false);

    if (token_tmp == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_char: token not found in char's token list");
        token->player = NULL;
        return;
    }

    if (token_prev == NULL)
        token_tmp->player->tokens = token_tmp->next;
    else
        token_prev->next = token->next;

    token->player = NULL;
}

/* transfers a token to a char */
void token_to_char_ex(TOKEN_DATA *token, CHAR_DATA *ch, char source, long flags)
{
    if (token == NULL || ch == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_char: NULL");
        return;
    }

    if (token->pIndexData == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_char: token with NULL pIndexData being added to char '%s', token name='%s'",
            HANDLE(ch), token->name ? token->name : "(null)");
        return;
    }

    token->player = ch;
    token->object = NULL;
    token->room = NULL;
    token->next = ch->tokens;
    ch->tokens = token;

    list_addlink(ch->ltokens, token);

    // Do sorted lists
    if(token->type == TOKEN_SKILL) skill_entry_addskill(token->player, 0, token, source, flags);
    else if(token->type == TOKEN_SPELL) skill_entry_addspell(token->player, 0, token, source, flags);
    else if(token->type == TOKEN_SONG) skill_entry_addsong(token->player, NULL, token, source);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_to_char: gave token %s(%ld) to char %s(%ld)",
        token->name, token->pIndexData->vnum,
        HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
}

void token_to_char(TOKEN_DATA *token, CHAR_DATA *ch)
{
    token_to_char_ex(token, ch, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);
}

/**
 * get_token_list - Find a token in a list by vnum with optional area constraint
 *
 * When area is NULL, matches any token with the given vnum (legacy behavior).
 * When area is non-NULL, only matches tokens from that specific area.
 *
 * @param tokens  Token list to search
 * @param vnum    Token vnum to match
 * @param area    Area constraint (NULL = match any area)
 * @param count   Which occurrence to return (1 = first match)
 * @return        Matching token or NULL
 */
TOKEN_DATA *get_token_list(LLIST *tokens, long vnum, AREA_DATA *area, int count)
{
    TOKEN_DATA *token;
    ITERATOR it;

    count = UMAX(1,count);

    iterator_start(&it, tokens);
    while( (token = (TOKEN_DATA*)iterator_nextdata(&it)) ) {
        if( IS_VALID(token) && token->pIndexData
            && token->pIndexData->vnum == vnum
            && (!area || token->pIndexData->area == area)
            && !--count )
            break;
    }
    iterator_stop(&it);

    return token;
}

/* finds a token on a character given the vnum, optionally constrained to area */
TOKEN_DATA *get_token_char(CHAR_DATA *ch, long vnum, AREA_DATA *area, int count)
{
    return get_token_list(ch->ltokens, vnum, area, count);
}

void token_from_obj(TOKEN_DATA *token)
{
    TOKEN_DATA *token_tmp, *token_prev;

    if (token->object == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_obj: called on token with no object");
        return;
    }

    token_prev = NULL;
    for (token_tmp = token->object->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
        if (token_tmp == token)
            break;

        token_prev = token_tmp;
    }

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_from_obj: removed token %s(%ld) from object %s(%ld)",
        token->name, token->pIndexData->vnum, token->object->short_descr, VNUM(token->object));

    list_remlink(token->object->ltokens, token, false);

    if (token_tmp == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_obj: token not found in object's token list");
        token->object = NULL;
        return;
    }

    if (token_prev == NULL)
        token_tmp->object->tokens = token_tmp->next;
    else
        token_prev->next = token->next;

    token->object = NULL;
}


/* transfers a token to an object */
void token_to_obj(TOKEN_DATA *token, OBJ_DATA *obj)
{
    if (token == NULL || obj == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_obj: NULL");
        return;
    }

    if (token->pIndexData == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_obj: token with NULL pIndexData being added to object '%s' (vnum %ld), token name='%s'",
            obj->short_descr, VNUM(obj), token->name ? token->name : "(null)");
        return;
    }

    token->player = NULL;
    token->object = obj;
    token->room = NULL;
    token->next = obj->tokens;
    obj->tokens = token;

    list_addlink(obj->ltokens, token);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_to_obj: gave token %s(%ld) to object %s(%ld)",
        token->name, token->pIndexData->vnum, obj->short_descr, VNUM(obj));
}


/* finds a token on an object given the vnum, optionally constrained to area */
TOKEN_DATA *get_token_obj(OBJ_DATA *obj, long vnum, AREA_DATA *area, int count)
{
    return get_token_list(obj->ltokens, vnum, area, count);
}

void token_from_room(TOKEN_DATA *token)
{
    TOKEN_DATA *token_tmp, *token_prev;
    char buf[MSL];

    if (token->room == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_room: called on token with no room");
        return;
    }

    token_prev = NULL;
    for (token_tmp = token->room->tokens; token_tmp != NULL; token_tmp = token_tmp->next) {
        if (token_tmp == token)
            break;

        token_prev = token_tmp;
    }

    if( token->room->wilds )
        sprintf(buf, "token_from_room: removed token %s(%ld) from vroom <%ld, %ld, %ld>",
            token->name, token->pIndexData->vnum, token->room->wilds->uid, token->room->x, token->room->y);
    else if( token->room->source )
        sprintf(buf, "token_from_room: removed token %s(%ld) from croom %s(%ld %08lX:%08lX)",
            token->name, token->pIndexData->vnum, token->room->name, token->room->source->vnum, token->room->id[0], token->room->id[1]);
    else
        sprintf(buf, "token_from_room: removed token %s(%ld) from room %s(%ld)",
            token->name, token->pIndexData->vnum, token->room->name, token->room->vnum);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_from_room: removed token %s(%ld) from room %s(%ld)",
        token->name, token->pIndexData->vnum, token->room->name, token->room->vnum);

    list_remlink(token->room->ltokens, token, false);

    if (token_tmp == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_from_room: token not found in room's token list");
        token->room = NULL;
        return;
    }

    if (token_prev == NULL)
        token_tmp->room->tokens = token_tmp->next;
    else
        token_prev->next = token->next;

    token->room = NULL;
}


/* transfers a token to a room*/
void token_to_room(TOKEN_DATA *token, ROOM_INDEX_DATA *room)
{
    if (token == NULL || room == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_room: NULL");
        return;
    }

    if (token->pIndexData == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "token_to_room: token with NULL pIndexData being added to room '%s' (vnum %ld), token name='%s'",
            room->name, room->vnum, token->name ? token->name : "(null)");
        return;
    }

    token->player = NULL;
    token->object = NULL;
    token->room = room;
    token->next = room->tokens;
    room->tokens = token;

    list_addlink(room->ltokens, token);

    if( room->wilds )
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_to_room: gave token %s(%ld) to vroom <%ld, %ld, %ld>",
            token->name, token->pIndexData->vnum, room->wilds->uid, room->x, room->y);
    else if( room->source )
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_to_room: gave token %s(%ld) to croom %s(%ld %08lX:%08lX)",
            token->name, token->pIndexData->vnum, room->name, room->source->vnum, room->id[0], room->id[1]);
    else
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "token_to_room: gave token %s(%ld) to room %s(%ld)",
            token->name, token->pIndexData->vnum, room->name, room->vnum);
}


/* finds a token on a room given the vnum */
TOKEN_DATA *get_token_room(ROOM_INDEX_DATA *room, long vnum, AREA_DATA *area, int count)
{
    return get_token_list(room->ltokens, vnum, area, count);
}


static inline int legacy_obj_index_value_get(const OBJ_INDEX_DATA *pObj, int slot)
{
    if (!pObj || slot < 0 || slot > 7)
        return 0;
    return pObj->value[slot];
}



/* Syn - this function fixes the problems with staves, potions, scrolls, and wands.
   It takes the spell numbers (previously stored in the v0-v9 values) and changes them
   into spell data structs on the object. */
void fix_magic_object_index(OBJ_INDEX_DATA *obj)
{
    int val;
    SPELL_DATA *spell, *spell_tmp;
    bool already_has_spell;

    /* scrolls, potions, and pills had level in v0, and spells in v1 onwards */
    if (obj->item_type == ITEM_SCROLL
    ||  obj->item_type == ITEM_POTION
    ||  obj->item_type == ITEM_PILL) {
    for (val = 1; val < 8; val++) {
        if (legacy_obj_index_value_get(obj, val) > 0) {
         /* Don't put the same spell on twice. */
         already_has_spell = false;
         for (spell = obj->spells; spell != NULL; spell = spell->next) {
             if (spell->sn == legacy_obj_index_value_get(obj, val))
             already_has_spell = true;
         }

         if (already_has_spell)
             continue;

         spell 		= new_spell();
         spell->sn	= legacy_obj_index_value_get(obj, val);
         spell->level	= legacy_obj_index_value_get(obj, 0);
         spell->repop	= 100; // Assuming 100 on objects made before rand was implemented
         spell->next     = NULL;
         if (!str_cmp(skill_table[spell->sn].name, "none"))
             free_spell(spell);
         else {
             if (obj->spells == NULL)
             obj->spells = spell;
             else
             {
             for (spell_tmp = obj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
                 ;

             spell_tmp->next = spell;
             }

             log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "Obj %s (%ld): Added spell %s, level %d, random %d.",
                 obj->short_descr, obj->vnum,
                 skill_table[legacy_obj_index_value_get(obj, val)].name, spell->level, spell->repop);
         }

         obj->value[val] = 0; // Reset legacy slot after migration
        }
    }
    }

    /* staves and wands had level in v0, total charges in v1, initial charges in v2, spells in v3 onwards */
    if (obj->item_type == ITEM_WAND
    ||  obj->item_type == ITEM_STAFF) {
         for (val = 3; val < 8; val++) {
         if (legacy_obj_index_value_get(obj, val) > 0) {
         /* Don't put the same spell on twice. */
         already_has_spell = false;
         for (spell = obj->spells; spell != NULL; spell = spell->next) {
             if (spell->sn == legacy_obj_index_value_get(obj, val))
             already_has_spell = true;
         }

         if (already_has_spell)
             continue;

         spell 		= new_spell();
         spell->sn	= legacy_obj_index_value_get(obj, val);
         spell->level	= legacy_obj_index_value_get(obj, 0);
         spell->repop	= 100; // Assuming 100 on objects made before rand was implemented
         spell->next     = NULL;

         if (!str_cmp(skill_table[spell->sn].name, "none"))
             free_spell(spell);
         else {
             if (obj->spells == NULL)
             obj->spells = spell;
             else
             {
             for (spell_tmp = obj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
                 ;

             spell_tmp->next = spell;
             }

             log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "Obj %s (%ld): Added spell %s, level %d, random %d.",
                 obj->short_descr, obj->vnum,
                 skill_table[legacy_obj_index_value_get(obj, val)].name, spell->level, spell->repop);
         }
         obj->value[val] = 0;
         }
     }
    }
}


/* Extracts an event, freeing it from all linked lists. Made it a function since it's
   used in more than one place. */
void extract_event(EVENT_DATA *ev)
{
    EVENT_DATA *ev_last, *ev_temp; /* For removing from global list */
    EVENT_DATA *ev_entity_last, *ev_entity_temp; /* For removing from entity list */
    EVENT_DATA **ev_head, **ev_tail;
    CHAR_DATA *ch = NULL;
    OBJ_DATA *obj = NULL;
    ROOM_INDEX_DATA *room = NULL;
    TOKEN_DATA *token = NULL;

    /* Remove from global list */

if (!IS_VALID(ev))
  return;

    /* Find it in the list first
    "*_next = *->next saving is not required here since we're not modifying structures within the for-loop */
    ev_last = NULL;
    for (ev_temp = events; ev_temp != NULL; ev_temp = ev_temp->next) {
        if (ev_temp == ev)
            break;

        ev_last = ev_temp;
    }

    if (ev_temp == NULL) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "extract_event: event not found in global list");
        return;
    }

    if (ev_last != NULL)
        ev_last->next = ev_temp->next;
    else
        events = ev_temp->next;

    if( events_tail == ev_temp )
    {
        if( ev_last )
            events_tail = ev_last;
        else
            events_tail = NULL;
    }

    /* Remove from entity list */

    /* Figure out which type of entity we are using */
    switch (ev->event_type) {
    case EVENT_MOBQUEUE:
        ch = (CHAR_DATA *) ev->entity;
        ev_head = &ch->events;
        ev_tail = &ch->events_tail;
        break;

    case EVENT_OBJQUEUE:
        obj = (OBJ_DATA *) ev->entity;
        ev_head = &obj->events;
        ev_tail = &obj->events_tail;
        break;

    case EVENT_ROOMQUEUE:
    case EVENT_ECHO:
        room = (ROOM_INDEX_DATA *) ev->entity;
        ev_head = &room->events;
        ev_tail = &room->events_tail;
        break;

    case EVENT_TOKENQUEUE:
        token = (TOKEN_DATA *) ev->entity;
        ev_head = &token->events;
        ev_tail = &token->events_tail;
        break;
    default:
        ev_head = NULL;
        ev_tail = NULL;
        break;
    }

    ev_entity_last = NULL;
    /* Locate it in the list and re-link the list */
    if (ev_head && ev_tail) {
        for (ev_entity_temp = *ev_head; ev_entity_temp; ev_entity_temp = ev_entity_temp->next_event) {
            if (ev_entity_temp == ev)
                break;

            ev_entity_last = ev_entity_temp;
        }
        if (ev_entity_temp)
        {
        if (ev_entity_last)
            ev_entity_last->next_event = ev_entity_temp->next_event;
        else
            *ev_head = ev_entity_temp->next_event;

        if( *ev_tail == ev_entity_temp )
            *ev_tail = ev_entity_last;
        else
            *ev_tail = NULL;
        }
    }

    free_event(ev);
}



void extract_project_inquiry(PROJECT_INQUIRY_DATA *pinq)
{
     PROJECT_DATA *project;
     PROJECT_INQUIRY_DATA *pinq_tmp, *pinq_tmp_last;

     project = pinq->project;

     /* Remove from project */
     pinq_tmp_last = NULL;
     for (pinq_tmp = project->inquiries; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next) {
     if (pinq_tmp == pinq)
         break;

     pinq_tmp_last = pinq_tmp;
     }

     if (pinq_tmp == NULL) {
     log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "extract_project_inquiry: inquiry not found in project list");
     return;
     }

     if (!pinq_tmp_last)
     project->inquiries = pinq_tmp->next;
     else
     pinq_tmp_last->next = pinq_tmp->next;


     /* Remove from global */
     pinq_tmp_last = NULL;
     for (pinq_tmp = project_inquiry_list; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next_global) {
     if (pinq_tmp == pinq)
         break;

     pinq_tmp_last = pinq_tmp;
     }

     if (pinq_tmp == NULL) {
     log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "extract_project_inquiry: inquiry not found in global list");
     return;
     }

     if (!pinq_tmp_last)
     project_inquiry_list = pinq_tmp->next_global;
     else
     pinq_tmp_last->next_global = pinq_tmp->next_global;

     free_project_inquiry(pinq);
}


/* Syn - This function uses the simple "log_entry" data structure to add a string
   to a list of logs. I wrote it to store info in the project system that's available
   from within the game.

   You can feel free to adapt it to anything else you want logged with a linked
   list.

   Instead of using some sort
   of buffer to store log entries, this implementation uses a linked list. If the number
   of log entries in the list is the max (MAX_LOG_ENTRIES), the oldest entry is taken
   off and saved to a file, and the new entry is added to the head of the list.
   This allows immortals to view log information in-game without having to tie up
   tons of memory by storing EVERYTHING that has happened since last boot.

   Note: this is not the most efficient system to use for logs where a MASSIVE
   amount of text (i.e. several new log entries added per second) since this means
   either a lot of performance draining saving or the risk of losing log information.
   But for this purpose, where log entries are added relatively sporadically, it
   works perfectly fine.
 */
void log_string_to_list(char *argument, LOG_ENTRY_DATA *list)
{
}


/* This function saves all logs to their files periodically. If you want to use my
   log entry list, be sure to add it in here. */
void save_logs()
{
}



// @@@NIB : 20070120 : Totals up the current value particular stat for all
//			people in the current room that are in the same
//			group as the target.
int get_curr_group_stat(CHAR_DATA *ch, int stat)
{
    CHAR_DATA *rch;
    int sum = 0;

    if(!ch->in_room) return 0;	// Since all stats have a minimum, this would be an ERROR

    for(rch = ch->in_room->people;rch; rch = rch->next_in_room)
        if(ch == rch || is_same_group(ch,rch))
            sum += get_curr_stat(rch,stat);

    return sum;
}

// @@@NIB : 20070120 : Totals up the base value particular stat for all
//			people in the current room that are in the same
//			group as the target.
int get_perm_group_stat(CHAR_DATA *ch, int stat)
{
    CHAR_DATA *rch;
    int sum = 0;

    if(!ch->in_room) return 0;	// Since all stats should have a minimum, this would be an ERROR

    for(rch = ch->in_room->people;rch; rch = rch->next_in_room)
        if(ch == rch || is_same_group(ch,rch))
            sum += rch->perm_stat[stat];

    return sum;
}

// @@@NIB : 20070120 : Counts the number of members online that belong
//			in the same church as the target.  If the target
//			is a mobile or churchless, the count will be zero.
int get_church_online_count(CHAR_DATA *ch)
{
//	DESCRIPTOR_DATA *d;

    if(ch && !IS_NPC(ch) && ch->church)
        return list_size(ch->church->online_players);


    return 0;
}

// @@@NIB : 20070120 : Totals up the weight of either the people or
//			objects, or both, in the specified room.
// @@@NIB : 20070121 : Added a 'ground' flag to exclude flying/floating entities
// @@@ASH : 20111231 : Changed the flying/float check to use the flying check function
int get_room_weight(ROOM_INDEX_DATA *room, bool mobs, bool objs, bool ground)
{
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    int weight = 0;

    if(!room) return 0;

    if(mobs) for(ch = room->people; ch; ch = ch->next_in_room) {
        if(ground && mobile_is_flying(ch))
                continue;

        weight += get_carry_weight(ch);
    }

    if(objs) for(obj = room->contents; obj; obj = obj->next_content)
        weight += get_obj_weight(obj);

    return weight;
}

// @@@NIB : 20070121 : Common function for checking if the mobile has
//			a "float_user" object worn
bool is_float_user(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (!ch) return false;

    // Use lworn LLIST instead of checking all carrying objects
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if (IS_SET(obj->extra[1], ITEM_FLOAT_USER)) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }

    return false;
}


// 20070521 : NIB : Function to see if CH has the desired catalyst or not
int has_catalyst(CHAR_DATA *ch, ROOM_INDEX_DATA *room, int type, int method, int min_strength, int max_strength)
{
    int total;
    OBJ_DATA *obj;
    OBJ_DATA *objNest;
    CATALYST_DATA *cat;
    ITERATOR it;

    // For now, it just checks to see if it has WARP_STONES...  CHECK: fixed to check any catalyst type
    // Fix to allow multicharged catalysts...  CHECK: utilizes multiple charges
    // Allow for multityped catalysts, using catalyst affects
    // Add for "CATALYST_HERE" for doing room level catalysts

    if(!ch && !room) return 0;

    if(!ch && !IS_SET(method,CATALYST_ROOM)) return 0;

    if(!room) room = ch->in_room;

    total = 0;

    if(ch) {
        // Use the lcarrying LLIST instead of the old carrying list
        if (ch->lcarrying) {
            iterator_start(&it, ch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if((IS_SET(method,CATALYST_HOLD) && obj->wear_loc == WEAR_HOLD) ||
                    (IS_SET(method,CATALYST_WORN) && obj->wear_loc != WEAR_NONE) ||
                    IS_SET(method,(CATALYST_CARRY))) {
                        for(cat = obj->catalyst; cat; cat = cat->next) 
                            if(cat->level >= min_strength && cat->level <= max_strength && cat->type == type) {
                                if(IS_SET(method, CATALYST_ACTIVE) && (cat->where != TO_CATALYST_ACTIVE) && !IS_SET(ch->act[1], PLR_AUTOCAT))
                                    continue;

                                if(cat->duration < 0) {
                                    iterator_stop(&it);
                                    return -1;    // Negative is treated as a "source"
                                }

                                total += cat->duration;
                            }
                } else if(obj->contains && IS_SET(method,CATALYST_CONTAINERS)) {    /* look in bags too */
                    // Navigate through container contents
                    for (objNest = obj->contains; objNest; objNest = objNest->next_content) {
                        for(cat = objNest->catalyst; cat; cat = cat->next) 
                            if(cat->level >= min_strength && cat->level <= max_strength && cat->type == type) {
                                if(IS_SET(method, CATALYST_ACTIVE) && (cat->where != TO_CATALYST_ACTIVE) && !IS_SET(ch->act[1], PLR_AUTOCAT))
                                    continue;
                                if(cat->duration < 0) {
                                    iterator_stop(&it);
                                    return -1;    // Negative is treated as a "source"
                                }

                                total += cat->duration;
                            }
                    }
                }
            }
            iterator_stop(&it);
        }
    }

    if(IS_SET(method,CATALYST_ROOM)) {
        // Use the lcontents LLIST instead of the old contents list
        if (room->lcontents) {
            iterator_start(&it, room->lcontents);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                for(cat = obj->catalyst; cat; cat = cat->next) 
                    if(cat->level >= min_strength && cat->level <= max_strength && cat->type == type) {
                        if(IS_SET(method, CATALYST_ACTIVE) && (cat->where != TO_CATALYST_ACTIVE) && !IS_SET(ch->act[1], PLR_AUTOCAT))
                            continue;
                        if(cat->duration < 0) {
                            iterator_stop(&it);
                            return -1;    // Negative is treated as a "source"
                        }
                        total += cat->duration;
                    }
                
                // Check container contents
                if (obj->contains) {
                    for (objNest = obj->contains; objNest; objNest = objNest->next_content) {
                        for(cat = objNest->catalyst; cat; cat = cat->next) 
                            if(cat->level >= min_strength && cat->level <= max_strength && cat->type == type) {
                                if(IS_SET(method, CATALYST_ACTIVE) && (cat->where != TO_CATALYST_ACTIVE) && !IS_SET(ch->act[1], PLR_AUTOCAT))
                                    continue;
                                if(cat->duration < 0) {
                                    iterator_stop(&it);
                                    return -1;    // Negative is treated as a "source"
                                }
                                total += cat->duration;
                            }
                    }
                }
            }
            iterator_stop(&it);
        }
    }

    return total;
}

int use_catalyst_obj(CHAR_DATA *ch, ROOM_INDEX_DATA *room, OBJ_DATA *obj, int type, int left, int min_strength, int max_strength, bool active, bool show)
{
    bool used;
    int total = 0;
    CATALYST_DATA *cat, *prev, *next;

    if(!obj) return 0;

    if(!ch && !room) room = obj_room(obj);

    if(!room) return 0;

    used = false;
    for(prev = NULL, cat = obj->catalyst; cat && total < left; cat = next) {
        next = cat->next;
        if(cat->level >= min_strength && cat->level <= max_strength && cat->type == type) {
            if(active && (cat->where != TO_CATALYST_ACTIVE) && !IS_SET(ch->act[1], PLR_AUTOCAT)) continue;

            if(cat->duration < 0) {
                if(show && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST_SOURCE, NULL))
                    act("$p pulsates brightly.", room->people, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL, NULL, NULL);
                return -1;
            }

            if(total + cat->duration <= left) {
                total += cat->duration;
                if(prev) prev->next = next;
                else obj->catalyst = next;

                free_catalyst(cat);

                if(!obj->catalyst) {    // All catalyst affects have been exhausted
                    if(show && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST_FULL, NULL) && ch)
                        act("$p flares brightly!", room->people, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL, NULL, NULL);
//                    extract_obj(obj);
                    return total;
                }
            } else {
                cat->duration -= left - total;
                total = left;
            }

            used = true;
        } else {
            prev = cat;
        }
    }

    if(show && used && !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CATALYST, NULL)) {
        act("$p shimmers brightly, but only dims back to normal.", room->people, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL, NULL, NULL);
    }
    return total;
}

int use_catalyst_here(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int amount,int min_strength, int max_strength, bool active, bool show)
{
    int total = 0, total2;
    OBJ_DATA *obj;
    OBJ_DATA *objNest, *nextNest;

    if(!ch && !room) return 0;

    if(!room) room = ch->in_room;

    OBJ_DATA *obj_next;
    for(obj = room->contents; obj && total < amount; obj = obj_next) {
        obj_next = obj->next_content;
        total2 = use_catalyst_obj(ch,room,obj,type,amount - total,min_strength,max_strength,active,show);
        if(total2 < 0) return -1;

        total += total2;
        for (objNest = obj->contains; objNest && total < amount; objNest = nextNest) {
            nextNest = objNest->next_content;
            total2 = use_catalyst_obj(ch,room,objNest,type,amount - total,min_strength,max_strength,active,show);
            if(total2 < 0) return -1;

            total += total2;
        }
    }

    return total;
}

int use_catalyst(CHAR_DATA *ch, ROOM_INDEX_DATA *room, int type, int method, int amount, int min_strength, int max_strength, bool show)
{
    int total, total2;
    OBJ_DATA *obj;
    OBJ_DATA *objNest, *nextNest;
    bool active;
    ITERATOR it;

    if(!ch && !room) return 0;

    if(!room) room = ch->in_room;

    if(!room) return 0;

    total = 0;
    active = IS_SET(method, CATALYST_ACTIVE);

    // Use the lcarrying LLIST instead of the old carrying list
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)) && total < amount) {
            if((IS_SET(method, CATALYST_HOLD) && obj->wear_loc == WEAR_HOLD) ||
                (IS_SET(method, CATALYST_WORN) && obj->wear_loc != WEAR_NONE) ||
                IS_SET(method, (CATALYST_CARRY))) {
                    total2 = use_catalyst_obj(ch, room, obj, type, amount - total, min_strength, max_strength, active, show);
                    if(total2 < 0) {
                        iterator_stop(&it);
                        return amount;
                    }

                    total += total2;
            } else if(obj->contains && IS_SET(method, CATALYST_CONTAINERS)) {    /* look in bags too */
                for (objNest = obj->contains; objNest; objNest = nextNest) {
                    nextNest = objNest->next_content;
                    total2 = use_catalyst_obj(ch, room, objNest, type, amount - total, min_strength, max_strength, active, show);
                    if(total2 < 0) {
                        iterator_stop(&it);
                        return amount;
                    }

                    total += total2;
                }
            }
        }
        iterator_stop(&it);
    }

    if(IS_SET(method, CATALYST_ROOM) && total < amount) {
        int htotal = use_catalyst_here(ch, room, type, amount - total, min_strength, max_strength, active, show);
        if(htotal < 0) return amount;
        total += htotal;
    }

    return total;
}

void move_cart(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool delay)
{
    if (PULLING_CART(ch))
    {
        obj_from_room(PULLING_CART(ch));

        if(room->wilds)
            obj_to_vroom(PULLING_CART(ch), room->wilds, room->x, room->y);
        else
            obj_to_room(PULLING_CART(ch), room);

        if(delay) {
            int wait_amount;

            wait_amount = CART(PULLING_CART(ch))->move_delay;

            if (MOUNTED(ch))
                WAIT_STATE(ch, wait_amount/2);
            else
                WAIT_STATE(ch, wait_amount);
        }
    }
}

unsigned long last_visited_room = 0;

void visit_room_recurse(LLIST *visited, ROOM_INDEX_DATA *room, VISIT_FUNC *func, int depth, void *argv[], int argc, bool closed, int door)
{
    EXIT_DATA *ex;
    int i;

    if(!list_hasdata(visited, room)) {
        list_appendlink(visited, room);
        if((*func)(room,argv,argc,depth,door) && depth > 0) {
            for(i = 0; i < MAX_DIR; i++) if((ex = room->exit[i])) {
                if(ex->u1.to_room && (!closed || !IS_SET(ex->exit_info, EX_CLOSED)))
                    visit_room_recurse(visited, ex->u1.to_room, func, depth - 1, argv, argc, closed, i);
            }
        }
    }
}

void visit_rooms(ROOM_INDEX_DATA *room, VISIT_FUNC *func, int depth, void *argv[], int argc, bool closed)
{
    LLIST *visited = list_create(false);

    visit_room_recurse(visited, room,func,depth,argv,argc,closed,MAX_DIR);

    list_destroy(visited);
}

bool char_exists(CHAR_DATA *ch)
{
    CHAR_DATA *cur;
    ITERATOR it;

    iterator_start(&it, loaded_chars);
    while(( cur = (CHAR_DATA *)iterator_nextdata(&it)))
        if( cur == ch )
            break;
    iterator_stop(&it);
    return (cur != NULL);
}


CHAR_DATA *idfind_mobile(register unsigned long id1, register unsigned long id2)
{
    register CHAR_DATA *ch;
    ITERATOR it;

    iterator_start(&it, loaded_chars);
    while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
        if(ch->id[0] == id1 && ch->id[1] == id2)
            break;
    iterator_stop(&it);
    return ch;
}

CHAR_DATA *idfind_player(register unsigned long id1, register unsigned long id2)
{
    register CHAR_DATA *ch;
    ITERATOR it;

    iterator_start(&it, loaded_chars);
    while(( ch = (CHAR_DATA *)iterator_nextdata(&it)))
        if(!IS_NPC(ch) && ch->id[0] == id1 && ch->id[1] == id2)
            break;
    iterator_stop(&it);
    return ch;
}

OBJ_DATA *idfind_object(register unsigned long id1, register unsigned long id2)
{
    register OBJ_DATA *obj;
    ITERATOR it;

    iterator_start(&it, loaded_objects);
    while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
        if(obj->id[0] == id1 && obj->id[1] == id2)
            break;
    iterator_stop(&it);

    return obj;
}

TOKEN_DATA *idfind_token(register unsigned long id1, register unsigned long id2)
{
    register TOKEN_DATA *token;
    for (token = global_tokens; token; token = token->global_next)
        if(token->id[0] == id1 && token->id[1] == id2)
            return token;
    return NULL;
}

TOKEN_DATA *idfind_token_char(CHAR_DATA *ch, register unsigned long id1, register unsigned long id2)
{
    register TOKEN_DATA *token;

    if( !ch ) return NULL;

    for (token = ch->tokens; token; token = token->next)
        if(token->id[0] == id1 && token->id[1] == id2)
            return token;
    return NULL;
}

TOKEN_DATA *idfind_token_object(OBJ_DATA *obj, register unsigned long id1, register unsigned long id2)
{
    register TOKEN_DATA *token;

    if( !obj ) return NULL;

    for (token = obj->tokens; token; token = token->next)
        if(token->id[0] == id1 && token->id[1] == id2)
            return token;
    return NULL;
}

TOKEN_DATA *idfind_token_room(ROOM_INDEX_DATA *room, register unsigned long id1, register unsigned long id2)
{
    register TOKEN_DATA *token;

    if( !room ) return NULL;

    for (token = room->tokens; token; token = token->next)
        if(token->id[0] == id1 && token->id[1] == id2)
            return token;
    return NULL;
}

bool string_vector_add(STRING_VECTOR **head, char *key, char *string)
{
    STRING_VECTOR *v;

    v = malloc(sizeof(STRING_VECTOR));
    if(v) {
        v->key = str_dup(key);
        v->string = str_dup(string);
        v->next = *head;
        *head = v;
        return true;
    }
    return false;
}

void string_vector_free(STRING_VECTOR *v)
{
    free_string(v->key);
    free_string(v->string);
    free(v);
}

void string_vector_remove(STRING_VECTOR **head, char *key)
{
    STRING_VECTOR *v, *p;

    if(!*head) return;

    if(!str_cmp((*head)->key,key)) {
        v = *head;
        *head = (*head)->next;

        string_vector_free(v);
    } else {
        for(p = *head; p->next; p = p->next) {
            if(!str_cmp(p->next->key,key)) {
                v = p->next;
                p->next = p->next->next;
                string_vector_free(v);
                break;
            }
        }
    }
}

void string_vector_freeall(STRING_VECTOR *head)
{
    STRING_VECTOR *v, *n;

    for(v = head; v; v = n) {
        n = v->next;
        string_vector_free(v);
    }
}

STRING_VECTOR *string_vector_find(register STRING_VECTOR *head, char *key)
{
    while(head) {
        if(!str_cmp(head->key,key)) return head;
        head = head->next;
    }

    return NULL;
}

void string_vector_set(register STRING_VECTOR **head, char *key, char *string)
{
    STRING_VECTOR *v = string_vector_find(*head,key);

    if(v) {
        free_string(v->string);
        v->string = str_dup(string);
    } else
        string_vector_add(head,key,string);
}


void get_random_room_target(ROOM_INDEX_DATA *room, OBJ_DATA **obj, CHAR_DATA **ch, long max)
{
    register OBJ_DATA *o;
    register CHAR_DATA *c;
    register long ocnt;
    register long ccnt;
    register long r;

    for(o = room->contents, ocnt = 0;o;o = o->next_content, ++ocnt);

    for(c = room->people, ccnt = 0;c;c = c->next_in_room, ++ccnt);

    // If the entity count is more than the meter, set the meter to the total count.
    if((ocnt + ccnt) > max) max= ocnt + ccnt;

    // Now that we have the counts, let's pick a random number
    r = number_range(0,max-1);

    if(r < ocnt) {
        for(o = room->contents, ocnt = 0;o && ocnt < r;o = o->next_content,++ocnt);
        *obj = o;
        *ch = NULL;
    } else if(r < (ocnt + ccnt)) {
        for(c = room->people, ccnt = 0, r -= ocnt;c && ccnt < r;c = c->next_in_room, ++ccnt);
        *obj = NULL;
        *ch = c;
    } else {
        *obj = NULL;
        *ch = NULL;
    }

    return;
}


// @@@REMOVEME: This function is invalid (id1 is used as a vnum and an id part)
ROOM_INDEX_DATA *idfind_vroom(register unsigned long id1, register unsigned long id2)
{
    ROOM_INDEX_DATA *room = NULL;
    WNUM wnum;
    if (resolve_widevnum((long)id1, NULL, &wnum))
        room = get_room_index(wnum.pArea, wnum.vnum);

    if(!room) return NULL;

    for(room = room->clones; room; room = room->next)
        if(room->id[0] == id1 && room->id[1] == id2)
            return room;

    return NULL;
}

ROOM_INDEX_DATA *get_environment(ROOM_INDEX_DATA *room)
{
    if(!room) return NULL;

    SHIP_DATA *ship = get_room_ship(room);

    if( IS_VALID(ship) )
    {
        return ship->ship ? obj_room(ship->ship) : NULL;
    }

    // static or floating virtual rooms have no environment
    if(!IS_SET(room->room_flag[1],ROOM_VIRTUAL_ROOM)) return NULL;

    switch(room->environ_type) {
    case ENVIRON_ROOM:	return room->environ.room;
    case ENVIRON_MOBILE:	return room->environ.mob ? room->environ.mob->in_room : NULL;
    case ENVIRON_OBJECT:	return room->environ.obj ? obj_room(room->environ.obj) : NULL;
    case ENVIRON_TOKEN:
        if(room->environ.token) {
            if( room->environ.token->player ) return room->environ.token->player->in_room;
            if( room->environ.token->object ) return obj_room(room->environ.token->object);
            if( room->environ.token->room ) return room->environ.token->room;
        }
        break;
    }

    return NULL;
}



bool mobile_is_flying(CHAR_DATA *mob)
{
    return mob && (IS_SET(mob->affected_by[0], AFF_FLYING) || is_float_user(mob) ||
        (mob->riding && mob->mount && mobile_is_flying(mob->mount)));
}

bool check_vision(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool blind, bool dark)
{
    if(!room) room = ch->in_room;

    if (!check_blind(ch)) {
        if(blind) send_to_char("{DYou can't see a thing!\n\r{x", ch);
        return false;
    }

    if ((!IS_NPC(ch) || IS_SWITCHED(ch)) // we are a player
        && (IS_NPC(ch) || !IS_SET(ch->act[0], PLR_HOLYLIGHT))
        && room_is_dark(room)
        && !IS_AFFECTED(ch, AFF_INFRARED)) {
        if(dark) {
            send_to_char("{DIt is pitch black ... \n\r{x", ch);
            show_char_to_char(room->people, ch, NULL);
        }
        return false;
    }

    return true;
}


void room_from_environment(ROOM_INDEX_DATA *room)
{
    ROOM_INDEX_DATA **prev = NULL;
    LLIST *lclones = NULL;

    if(!room || !room_is_clone(room)) return;

    switch(room->environ_type) {
    case ENVIRON_ROOM: prev = &room->environ.room->clone_rooms; lclones = room->environ.room->lclonerooms; break;
    case ENVIRON_MOBILE: prev = &room->environ.mob->clone_rooms; lclones = room->environ.mob->lclonerooms; break;
    case ENVIRON_OBJECT: prev = &room->environ.obj->clone_rooms; lclones = room->environ.obj->lclonerooms; break;
    case ENVIRON_TOKEN: prev = &room->environ.token->clone_rooms; lclones = room->environ.token->lclonerooms; break;
    default: return;
    }

    while(*prev) {
        if(*prev == room) {
            *prev = room->next_clone;
            break;
        }
        prev = &(*prev)->next_clone;
    }

    list_remlink(lclones, room, false);

    // The object now has no clones
    if(room->environ_type == ENVIRON_OBJECT && !room->environ.obj->clone_rooms)
        obj_update_nest_clones(room->environ.obj);


    room->environ_type = ENVIRON_NONE;
    room->next_clone = NULL;
}

bool room_to_environment(ROOM_INDEX_DATA *clone,CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token)
{
    if(!clone || !room_is_clone(clone) || clone->environ_type != ENVIRON_NONE) return false;

    if(mob) {
        clone->next_clone = mob->clone_rooms;
        mob->clone_rooms = clone;
        if( !list_appendlink(mob->lclonerooms, clone) ) {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Failed to add clone room to environment due to memory issues with 'list_appendlink',");
            abort();
        }
        clone->environ.mob = mob;
        clone->environ_type = ENVIRON_MOBILE;
        return true;
    }


    if(obj) {
        clone->next_clone = obj->clone_rooms;
        obj->clone_rooms = clone;
        if( !list_appendlink(obj->lclonerooms, clone) ) {
            log_message(LOG_LEVEL_CRITICAL, LOG_CRITICAL, "Failed to add clone room to environment due to memory issues with 'list_appendlink'");
            abort();
        }

        // This is the only clone.. hence, the object used to have no clones anchored
        if(!clone->next_clone) obj_update_nest_clones(obj);
        clone->environ.obj = obj;
        clone->environ_type = ENVIRON_OBJECT;
        return true;
    }


    if(room && room != clone && room->environ_type != ENVIRON_ROOM) {
        clone->next_clone = room->clone_rooms;
        room->clone_rooms = clone;
        if( !list_appendlink(room->lclonerooms, clone) ) {
            log_message(LOG_LEVEL_CRITICAL, LOG_CRITICAL, "Failed to add clone room to environment due to memory issues with 'list_appendlink'");
            abort();
        }
        clone->environ.room = room;
        clone->environ_type = ENVIRON_ROOM;
        return true;
    }

    if(token) {
        clone->next_clone = token->clone_rooms;
        token->clone_rooms = clone;
        if( !list_appendlink(token->lclonerooms, clone) ) {
            log_message(LOG_LEVEL_CRITICAL, LOG_CRITICAL, "Failed to add clone room to environment due to memory issues with 'list_appendlink'");
            abort();
        }

        // This is the only clone.. hence, the object used to have no clones anchored
        clone->environ.token = token;
        clone->environ_type = ENVIRON_TOKEN;
        return true;
    }


    return false;
}

// number of nest_clones + an adjustment based upon number of clone_rooms assigned
int obj_nest_clones(OBJ_DATA *obj)
{
    return obj ? ((obj->clone_rooms ? (obj->clone_rooms->next_clone ? 2 : 1) : 0) + obj->nest_clones) : 0;
}

void obj_set_nest_clones(OBJ_DATA *obj, bool add)
{
    if(!obj) return;

    if(add) {
        obj->nest_clones++;

        // There were already extra clones on here, no need to update
        if(obj_nest_clones(obj) > 1) return;

        // Update container...
        obj_set_nest_clones(obj->in_obj,true);
    } else {
        // This already had no clones, no need to progress deeper
        if(!obj_nest_clones(obj)) return;

        obj->nest_clones--;

        // Update container...
        obj_set_nest_clones(obj->in_obj,false);
    }
}

void obj_update_nest_clones(OBJ_DATA *obj)
{
    // If this is already flagged as having nested clones from contained objects, abort
    if(!obj->in_obj || obj_nest_clones(obj) > 1) return;

    obj_set_nest_clones(obj->in_obj,(obj->clone_rooms ? true : false));
}


LLIST *list_create(bool purge)
{
    LLIST *lp = alloc_mem(sizeof(LLIST));

    if(lp) {
        lp->next = NULL;
        lp->head = NULL;
        lp->ref = 0;
        lp->size = 0;
        lp->valid = true;
        lp->purge = purge;
        lp->copier = NULL;
        lp->deleter = NULL;
        lp->identifier = LLIST_IDENT;
    }

    return lp;
}

LLIST *list_createx(bool purge, LISTCOPY_FUNC copier, LISTDESTROY_FUNC deleter)
{
    LLIST *lp = list_create(purge);

    if( lp ) {
        lp->copier = copier;
        lp->deleter = deleter;
    }

    return lp;
}


LLIST *list_copy(LLIST *src)
{
    if( src == NULL || !src->valid || src->purge ) return NULL;

    LLIST *cpy = list_create(src->purge);

    if( cpy ) {
        register LLIST_LINK *cur, *next;
        bool valid = true;

        cpy->copier = src->copier;
        cpy->deleter = src->deleter;

        for( cur = src->head; cur; cur = next ) {
            next = cur->next;
            if( cur->data ) {
                void *data = cur->data;

                if( cpy->copier )
                    data = (*cpy->copier)(data);

                if( !data || !list_appendlink(cpy, data) ) {
                    if( data && cpy->copier && cpy->deleter )
                        (*cpy->deleter)(data);

                    valid = false;
                    break;
                }
            }
        }

        if( !valid ) {
            list_destroy(cpy);
            cpy = NULL;
        }
    }

    return cpy;
}

void list_purge(LLIST *lp)
{
    register LLIST_LINK *cur, *next;
    if( lp && !lp->valid && lp->ref < 1) {
        for( cur = lp->head; cur; cur = next ) {
            next = cur->next;

            if( lp->deleter && cur->data )
                (*lp->deleter)(cur->data);

            free_mem(cur, sizeof(LLIST_LINK));
        }
        lp->head = NULL;
        lp->tail = NULL;
    }
}

void list_destroy(LLIST *lp)
{
    if(lp && lp->valid ) {
        lp->valid = false;
        if( lp->ref < 1 ) {
            // This point is only ever reached if the list has not references at the time this list is destroyed
            // If the list is in-use, the purging/freeing is handled when the references are cleared.
            list_purge(lp);
            free(lp);
        }
    }
}

void list_cull(LLIST *lp)
{
    register LLIST_LINK /***prev,*/ *cur, *next;

    if(lp && lp->ref < 1) {
        // Cull any null data nodes
        for(cur = lp->head;cur;)
        {
            if (!cur->data)
            {
                next = cur->next;
                if (cur->prev)
                    cur->prev->next = next;
                else
                    lp->head = next;

                if (cur->next)
                    cur->next->prev = cur->prev;
                else
                    lp->tail = cur->prev;

                free_mem(cur, sizeof(LLIST_LINK));
                cur = next;
            }
            else
            {
                cur = cur->next;
            }
        }

/*
        for(prev = &lp->head, cur = lp->head; cur;) {
            if(!cur->data) {
                do {
                    if( lp->tail == cur )
                        lp->tail = lp->tail->prev;
                    *prev = cur->next;


                    free_mem(cur,sizeof(LLIST_LINK));
                    cur = *prev;
                } while(cur && !cur->data);
            } else {
                prev = &cur->next;
                cur = *prev;
            }
        }
*/

        if(!lp->tail && lp->head ) {
            if( lp->head->next ) {
                for(cur = lp->head; cur->next;cur = cur->next )
                {
                    cur->next->prev = cur;	// Reset the double linkage
                }
                lp->tail = cur;
            } else
                lp->tail = lp->head;
            lp->head->prev = NULL;
            lp->tail->next = NULL;
        }
    }
}

void list_addref(LLIST *lp)
{
    if(lp) lp->ref++;
}

void list_remref(LLIST *lp)
{
    if(lp) {
        --lp->ref;
        list_cull(lp);

        if(lp->ref < 1 && !lp->valid) {
            list_purge(lp);
            free(lp);
        } else if(lp->ref < 1 && lp->purge) {
            list_destroy(lp);
        }
    }
}

void list_remdata(LLIST *lp, LLIST_LINK *link, bool del)
{
    if( link ) {
        if( del && lp->deleter )
            (*lp->deleter)(link->data);

        link->data = NULL;
        lp->size--;
    }
}


bool list_addlink(LLIST *lp, void *data)
{
    LLIST_LINK *link;

    if(lp && lp->valid && (link = alloc_mem(sizeof(LLIST_LINK)))) {

        // No need to worry about the linkage and reference
        link->next = lp->head;
        if( !lp->head )
            lp->tail = link;
        else
            lp->head->prev = link;
        lp->head = link;
        link->prev = NULL;

        link->data = data;
        lp->size++;
        return true;
    }
    return false;
}

bool list_appendlink(LLIST *lp, void *data)
{
    LLIST_LINK *link;

    if(lp && lp->valid && (link = alloc_mem(sizeof(LLIST_LINK)))) {
        //log_stringf("list_appendlink: Adding data %016X to list %016X.", lp, data);
        // First one?
        if( !lp->head )
            lp->head = link;
        else
            lp->tail->next = link;
        link->prev = lp->tail;
        lp->tail = link;

        link->data = data;
        lp->size++;
        return true;
    }
    return false;
}

// DOES NOT DEEP COPY ELEMENTS
bool list_appendlist(LLIST *lp, LLIST *src)
{
    if( IS_VALID(lp) && IS_VALID(src) )
    {
        ITERATOR it;
        void *data;
        bool good = true;


        iterator_start(&it, src);
        while( (data = iterator_nextdata(&it)) )
        {
            if( !list_appendlink(lp, data) )
            {
                good = false;
                break;
            }
        }
        iterator_stop(&it);

        return good;
    }

    return false;
}

bool list_movelink(LLIST *lp, int from, int to)
{
    LLIST_LINK *old, *link, *new_link;

    // Adjust negative indices
    if (from < 0) from = lp->size + from + 1;
    if (to < 0) to = lp->size + to + 1;

    // Check for invalid positions
    if (from <= 0 || to <= 0 || from > lp->size || to > lp->size) return false;

    // If from and to are the same, no need to move
    if (from == to) return true;

    if (lp)
    {
        old = NULL;
        // Locate the 'from' node
        for (link = lp->head; link && from > 0; link = link->next)
            if (link->data)
            {
                --from;
                if (!from)
                {
                    old = link;
                    break;
                }
            }

        if (!old) return false;

        // Locate the 'to' position
        for (link = lp->head; link && to > 0; link = link->next)
        {
            if (link != old)
            {
                if ((to == 1) && (!link->data))
                {
                    // This is an empty link, reuse it
                    link->data = old->data;
                    old->data = NULL;
                    return true;
                }

                if (link->data)
                {
                    if (!--to)
                    {
                        new_link = alloc_mem(sizeof(LLIST_LINK));
                        if (!new_link) return false;

                        new_link->data = old->data;
                        old->data = NULL;

                        if (link->prev)
                        {
                            new_link->next = link;
                            new_link->prev = link->prev;
                            link->prev->next = new_link;
                            link->prev = new_link;
                        }
                        else
                        {
                            new_link->next = lp->head;
                            lp->head->prev = new_link;
                            lp->head = new_link;
                            new_link->prev = NULL;
                        }

                        // Update list size
                        lp->size++;
                        return true;
                    }
                }
            }
        }

        if (to > 0)
        {
            // Needs to append if it's at the end
            link = alloc_mem(sizeof(LLIST_LINK));
            if (!link) return false;

            if (!lp->head)
                lp->head = link;
            else
                lp->tail->next = link;
            link->prev = lp->tail;
            lp->tail = link;

            link->data = old->data;
            old->data = NULL;

            // Update list size
            lp->size++;

            // Update head and tail if necessary
            if (old == lp->head) lp->head = old->next;
            if (old == lp->tail) lp->tail = old->prev;

            // Remove old link from its current position
            if (old->prev) old->prev->next = old->next;
            if (old->next) old->next->prev = old->prev;

            free_mem(old, sizeof(LLIST_LINK));

            // Update list size
            lp->size--;

            return true;
        }
    }

    return false;
}

bool list_insertlink(LLIST *lp, void *data, int to)
{
    LLIST_LINK *link, *new_link;

    if( to < 0 ) to = lp->size + to + 1;

    if( !to ) return false;

    if( lp )
    {
        for(link = lp->head; link && to > 0; link = link->next )
        {
            if( (to == 1) && (!link->data) )
            {
                // This is an empty link, reuse it
                link->data = data;
                lp->size++;
                return true;
            }

            if( link->data )
            {
                if( !--to )
                {
                    new_link = alloc_mem(sizeof(LLIST_LINK));
                    if( !new_link )
                        return false;

                    new_link->data = data;

                    if( link->prev )
                    {
                        new_link->prev = link->prev;
                        new_link->next = link;
                        link->prev->next = new_link;
                        link->prev = new_link;
                    }
                    else
                    {
                        new_link->next = link;
                        link->prev = new_link;
                        lp->head = new_link;
                    }

                    lp->size++;
                    return true;
                }
            }
        }


        if( to > 0 )
        {
            // Needs to append if it's at the end
            link = alloc_mem(sizeof(LLIST_LINK));
            if( !link )
                return false;

            if( !lp->head )
                lp->head = link;
            else
                lp->tail->next = link;
            link->prev = lp->tail;
            lp->tail = link;
            link->data = data;
            lp->size++;
            return true;
        }

    }

    return false;
}


// Nulls out any data pointer that matches the supplied pointer
// It will NOT cull the list
void list_remlink(LLIST *lp, void *data, bool del)
{
    LLIST_LINK *link, *link_next;

    if(lp && data) {
        for(link = lp->head; link; link = link_next)
        {
            link_next = link->next;
            if(link->data == data) {
                list_remdata(lp, link, del);
            }
        }
    }
}

bool list_haslink(LLIST *list, void *data) {
    LLIST_LINK *link;
    if (!list || !data)
        return false;
    for (link = list->head; link != NULL; link = link->next) {
        if (link->data == data)
            return true;
    }
    return false;
}

// Clears out the entire list
void list_clear(LLIST *lp)
{
    LLIST_LINK *link, *link_next;
    if(lp && lp->valid) {
        for(link = lp->head; link; link = link_next) {
            link_next = link->next;
            list_remdata(lp, link, true);
        }

        lp->size = 0;
        list_cull(lp);
    }
}

// Get the last entry of a list.
void *list_last(LLIST *list)
{
    if (!list || list->size == 0)
        return NULL;
        
    void *data = NULL;
    ITERATOR it;
    
    iterator_start(&it, list);
    while (iterator_hasdata(&it)) {
        data = iterator_nextdata(&it);
    }
    iterator_stop(&it);
    
    return data;
}

void *iterator_peek_nextdata(ITERATOR *it)
{
    LLIST_LINK *link;

    if (!it || !it->list || !it->list->valid || !it->current)
        return NULL;

    // If we haven't moved yet, peek at current if valid, else next
    if (!it->moved) {
        link = it->current;
        if (link && link->data)
            link = link->next;
    } else {
        link = it->current ? it->current->next : NULL;
    }

    // Find the next link with data
    while (link && !link->data)
        link = link->next;

    return link ? link->data : NULL;
}

bool iterator_hasdata(ITERATOR *it)
{
    if(it && it->list && it->list->valid && it->current) {
        return (it->current->data != NULL);
    }
    return false;
}

void *list_randomdata(LLIST *lp)
{
    register LLIST_LINK *link = NULL;
    register int nth = 0;

    if(lp && lp->valid) {
        nth = number_range(1, lp->size);
        if( nth < 0 ) nth = lp->size + nth + 1;
        for(link = lp->head; link && nth > 0; link = link->next)
            if(link->data)
            {
                --nth;
                if( !nth ) break;
            }
    }

    return (link && !nth) ? link->data : NULL;
}


void *list_nthdata(LLIST *lp, register int nth)
{
    register LLIST_LINK *link = NULL;

    if(lp && lp->valid) {
        if( nth < 0 ) nth = lp->size + nth + 1;
        for(link = lp->head; link && nth > 0; link = link->next)
            if(link->data)
            {
                --nth;
                if( !nth ) break;
            }
    }

    return (link && !nth) ? link->data : NULL;
}

void list_remnthlink(LLIST *lp, register int nth, bool del)
{
    register LLIST_LINK *link = NULL;

    if(lp && lp->valid) {
        if( nth < 0 ) nth = lp->size + nth + 1;
        for(link = lp->head; link && nth > 0; link = link->next)
            if(link->data)
            {
                --nth;
                if( !nth ) break;
            }
    }

    if( link && !nth ) {
        list_remdata(lp, link, del);
    }
}

bool list_contains(LLIST *lp, register void *ptr, int (*cmp)(void *a, void *b))
{
    ITERATOR it;
    void *data;

    if(!lp || !lp->valid || !ptr) return false;

    iterator_start(&it, lp);
    if (cmp != NULL)
    {
        while((data = iterator_nextdata(&it)))
        {
            if (!cmp(data, ptr))
                break;
        }
    }
    else
    {
        while((data = iterator_nextdata(&it)) && (data != ptr));
    }

    iterator_stop(&it);

    return data && true;
}

bool list_hasdata(LLIST *lp, register void *ptr)
{
    ITERATOR it;
    void *data;

    if(!lp || !lp->valid || !ptr) return false;

    iterator_start(&it, lp);
    while((data = iterator_nextdata(&it)) && (data != ptr));

    iterator_stop(&it);

    return data && true;
}

int list_size(LLIST *lp)
{

    if(!lp || !lp->valid) return 0;

#if 0
    ITERATOR it;
    int size = 0;

    iterator_start(&it, lp);
    while((iterator_nextdata(&it))) ++size;

    iterator_stop(&it);

    return size;
#else
    return lp->size;
#endif
}

int list_getindex(LLIST *lp, void *ptr)
{
    ITERATOR it;
    void *data;
    int index = 0;

    iterator_start(&it, lp);
    while( (data = iterator_nextdata(&it)) )
    {
        ++index;

        if( data == ptr )
            break;
    }
    iterator_stop(&it);

    return data ? index : 0;
}

bool list_isvalid(LLIST *lp)
{
    return lp && lp->valid;
}

// ITERATORs can just be straight variables.  No allocation is needed.
void iterator_start(ITERATOR *it, LLIST *lp)
{
    if(it) {
        if(lp && lp->valid) {
//			log_stringf("iterator_start: list =  %016lX.", lp);
            it->list = lp;
            it->current = lp->head;
            it->moved = false;

            list_addref(lp);
        } else {
            it->list = NULL;
            it->current = NULL;
            it->moved = false;
        }
    }
}

void iterator_start_nth(ITERATOR *it, LLIST *lp, int nth)
{
    register LLIST_LINK *link;

    if(it) {
        if(lp && lp->valid) {
            it->list = lp;

            if( nth < 0 ) nth = lp->size + nth + 1;

            for(link = lp->head; link && nth > 0; link = link->next)
                if(link->data)
                    --nth;

            it->current = link;
            it->moved = false;

            list_addref(lp);
        } else {
            it->list = NULL;
            it->current = NULL;
        }
    }
}

LLIST_LINK *iterator_next(ITERATOR *it)
{
    register LLIST_LINK *link = NULL;
    if(it && it->list && it->list->valid && it->current) {
        if( it->moved ) {
            for(link = it->current->next; link && !link->data; link = link->next);
        } else {
            for(link = it->current; link && !link->data; link = link->next);

            it->moved = true;
        }
        it->current = link;

    }

    return link;
}

void *iterator_prevdata(ITERATOR *it)
{
    register LLIST_LINK *link = NULL;
    //register LLIST_LINK *next = NULL;
    if(it && it->list && it->list->valid && it->current) {
        if( it->moved ) {
            for(link = it->current->prev; link && !link->data; link = link->prev);
        } else {
            for(link = it->current; link && !link->data; link = link->prev);

            it->moved = true;
        }
        it->current = link;
    }

    return link ? link->data : NULL;

}


void *iterator_currentdata(ITERATOR *it)
{
    if(it && it->list && it->list->valid && it->current) {
        return it->current->data;
    }
    return NULL;
}
//extern bool it_debug;
void *iterator_nextdata(ITERATOR *it)
{
    extern bool it_debug;
    register LLIST_LINK *link = NULL;
    //register LLIST_LINK *next = NULL;
    if(it && it->list && it->list->valid && it->current) {
        if( it->moved ) {
            link = it->current->next;
            if (it_debug)
            log_stringf("iterator_nextdata[moved]: current=%p current->next=%p link=%p link->data=%p",
                (void*)it->current, (void*)it->current->next, (void*)link, link ? link->data : NULL);
            for(; link && !link->data; link = link->next);
            if (it_debug)
            log_stringf("iterator_nextdata[moved]: after loop link=%p", (void*)link);		} else {
            for(link = it->current; link && !link->data; link = link->next);

            it->moved = true;
        }
        it->current = link;
    } else {
        if (it_debug)
        log_stringf("iterator_nextdata: condition FAILED - it=%p list=%p valid=%d current=%p",
            (void*)it, it ? (void*)it->list : NULL,
            (it && it->list) ? it->list->valid : -1,
            it ? (void*)it->current : NULL);
    }

    return link ? link->data : NULL;
}

void iterator_setcurrent(ITERATOR *it, void *data)
{
    if (it && it->list && it->list->valid && it->current)
    {
        // Delete the current data using specific deleter if it is defined
        if( it->list->deleter )
            (*it->list->deleter)(it->current->data);
        
        if (!it->current->data)
            it->list->size++;

        it->current->data = data;
    }
}

void iterator_remcurrent(ITERATOR *it)
{
    if(it && it->list && it->current && it->current->data)
    {
        // Delete the data using specific deleter if it is defined
        if( it->list->deleter )
            (*it->list->deleter)(it->current->data);

        it->current->data = NULL;
        it->list->size--;

    }
}

void iterator_reset(ITERATOR *it)
{
    if(it) {
        if(it->list && it->list->valid) {
            it->current = it->list->head;
            it->moved = false;
        } else {
            it->list = NULL;
            it->current = NULL;
            it->moved = false;
        }
    }
}

void iterator_stop(ITERATOR *it)
{
    if(it) {
        if(it->list) list_remref(it->list);

        it->list = NULL;
        it->current = NULL;
    }
}

bool iterator_insert_before(ITERATOR *it, void *data)
{
    if (it && it->list && it->current)
    {
        // This spot is already blank
        if(!it->current->data)
            it->current->data = data;

        // Previous spot is already blank
        else if (it->current->prev && !it->current->prev->data)
            it->current->prev->data = data;
        else
        {
            LLIST_LINK *link = alloc_mem(sizeof(LLIST_LINK));
            if(!link) return false;

            link->prev = it->current->prev;

            if (it->current->prev)
                it->current->prev->next = link;
            else
                it->list->head = link;

            link->next = it->current;
            it->current->prev = link;

            link->data = data;
        }

        it->list->size++;
        return true;
    }

    return false;
}

bool iterator_insert_after(ITERATOR *it, void *data)
{
    if (it && it->list && it->current)
    {
        // This spot is already blank
        if(!it->current->data)
            it->current->data = data;

        // Next spot is already blank
        else if (it->current->next && !it->current->next->data)
            it->current->next->data = data;
        else
        {
            LLIST_LINK *link = alloc_mem(sizeof(LLIST_LINK));
            if(!link) return false;

            link->next = it->current->next;
            if (it->current->next)
                it->current->next->prev = link;
            else
                it->list->tail = link;

            link->prev = it->current;
            it->current->next = link;

            link->data = data;
        }

        it->list->size++;
        return true;
    }

    return false;
}

static int llist_link_cmp_user_adapter(const void *a, const void *b) {
    extern int (*llist_link_cmp_user)(void *, void *);
    LLIST_LINK *la = *(LLIST_LINK **)a;
    LLIST_LINK *lb = *(LLIST_LINK **)b;
    return llist_link_cmp_user(la->data, lb->data);
}
int (*llist_link_cmp_user)(void *, void *) = NULL;

bool list_quicksort(LLIST *lp, int (*cmp)(void *a, void *b))
{
    if (IS_VALID(lp) && cmp)
    {
        LLIST_LINK *cur;
        int count;

        // Count elements
        for(count = 0, cur = lp->head; cur; cur = cur->next)
            count++;

        if (count < 1) return false;

        LLIST_LINK **arr = alloc_mem(sizeof(LLIST_LINK *) * count);
        int i = 0;
        for(cur = lp->head; cur; cur = cur->next)
            arr[i++] = cur;

        // Use qsort to sort the array of pointers
        llist_link_cmp_user = cmp;
        qsort(arr, count, sizeof(LLIST_LINK *), llist_link_cmp_user_adapter);
        llist_link_cmp_user = NULL;

        // Relink everything
        for(int i = 0; i < count; i++)
        {
            if (i > 0) arr[i]->prev = arr[i-1];
            else arr[i]->prev = NULL; // Ensure head's prev is NULL

            if (i < (count - 1)) arr[i]->next = arr[i+1];
            else arr[i]->next = NULL; // Ensure tail's next is NULL
        }
        lp->head = arr[0];
        lp->tail = arr[count - 1];

        free_mem(arr, sizeof(LLIST_LINK *) * count);
        return true;
    }

    return false;
}


///////////////////////////////////////////
//
// Function: area_has_read_access
//
// Section: Building/Security
//
// Purpose: Determines if the builder has READ access for items in the specified area.
//
// Returns: true if the builder has read access
//
bool area_has_read_access(CHAR_DATA *ch, AREA_DATA *area)
{
    if(IS_NPC(ch)) return false;

//	if(!IS_IMMORTAL(ch)) return false;

    if(IS_BUILDER(ch, area)) return true;

    // Max rank can read anything
    if(ch->tot_level >= MAX_LEVEL) return true;

    return false;
}

///////////////////////////////////////////
//
// Function: area_has_write_access
//
// Section: Building/Security
//
// Purpose: Determines if the builder has WRITE access for items in the specified area.
//
// Returns: true if the builder has write access
//
bool area_has_write_access(CHAR_DATA *ch, AREA_DATA *area)
{
    if(IS_NPC(ch)) return false;

    if(!IS_IMMORTAL(ch)) return false;

    if(IS_BUILDER(ch, area)) return true;

    // Only a max rank, fully secured imm can write to ANYTHING
    if(ch->tot_level < MAX_LEVEL || ch->pcdata->security < 9) return false;

    return true;
}

CHAR_DATA *obj_carrier(OBJ_DATA *obj)
{
    if(obj->carried_by) return obj->carried_by;

    if(obj->in_obj) return obj_carrier(obj->in_obj);

    return NULL;
}


// Converts the location to a room
ROOM_INDEX_DATA *location_to_room(LOCATION *loc)
{
    ROOM_INDEX_DATA *room = NULL;
    if(loc->wuid) {
        WILDS_DATA *wilds = get_wilds_from_uid(NULL,loc->wuid);
        if(wilds && !(room = get_wilds_vroom(wilds,loc->id[0],loc->id[1])))
            room = create_wilds_vroom(wilds,loc->id[0],loc->id[1]);
    } else if(loc->id[0]) {
        WNUM wnum;
        if (resolve_widevnum(loc->id[0], NULL, &wnum))
            room = get_room_index(wnum.pArea, wnum.vnum);
        if(room && (loc->id[1] || loc->id[2]))
            room = get_clone_room(room,loc->id[1],loc->id[2]);
    }

    return room;
}

ROOM_INDEX_DATA *get_area_recall_room(AREA_DATA *area)
{
    ROOM_INDEX_DATA *room;

    if (!area)
        return NULL;

    if (area->recall.wuid > 0)
        return location_to_room(&area->recall);

    if (area->recall.id[0] > 0) {
        room = get_room_index(area, area->recall.id[0]);
        if (room && (area->recall.id[1] || area->recall.id[2]))
            room = get_clone_room(room, area->recall.id[1], area->recall.id[2]);

        if (room)
            return room;
    }

    return location_to_room(&area->recall);
}

// Converts the room into a location
void location_from_room(LOCATION *loc, ROOM_INDEX_DATA *room)
{
    if(!loc) return;

    if(!room)
        location_clear(loc);
    else if(room->wilds)
        location_set(loc,room->wilds->uid,room->x,room->y,room->z);
    else
        location_set(loc,0,room->vnum,room->id[0],room->id[1]);
}


ROOM_INDEX_DATA *get_recall_room(CHAR_DATA *ch, bool death)
{
    ROOM_INDEX_DATA *loc;

    // Priority

    // 1) RECALL triggers

    // Do not reset the recall point here, as it may have been set by other means.
    // Simply call the recall triggers to see if they MODIFY it.
    if (!death)
    {
    if(!p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECALL, NULL))
        p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECALL, NULL);
    }
    loc = location_to_room(&ch->recall);
    memset(&ch->recall,0,sizeof(LOCATION));

    // 2) Instance/dungeon recall - if the player is in an instanced room
    if (!loc && ch->in_room && IS_VALID(ch->in_room->instance_section)
        && IS_VALID(ch->in_room->instance_section->instance))
    {
        INSTANCE *inst = ch->in_room->instance_section->instance;
        if (inst->recall)
            loc = inst->recall;
    }

    // 3) Room assigned recalls
    if(!loc) loc = location_to_room(&ch->in_room->recall);

    // 4) Area assigned recalls
    if(!loc) loc = get_area_recall_room(ch->in_room->area);

    return loc;
}

void location_clear(LOCATION *loc)
{
    loc->wuid = 0;
    loc->id[0] = 0;
    loc->id[1] = 0;
    loc->id[2] = 0;
}

void location_set(LOCATION *loc, unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
    loc->wuid = a;
    loc->id[0] = b;
    loc->id[1] = c;
    loc->id[2] = d;

    // if a != 0, then <b,c,d> is the xyz location on wilderness 'a'
    // if a == 0 and b != 0 and c:d == 0, then is the static room 'b'
    // if a == 0 and b != 0 and c:d != 0, then is the clone of room 'b' with id c:d
    // if a == 0 and b == 0, then it is nowhere
}

void rs_location_clear(RS_LOCATION *loc)
{
    loc->wuid = 0;
    loc->id[0] = 0;
    loc->id[1] = 0;
    loc->id[2] = 0;
}

void rs_location_set(RS_LOCATION *loc, unsigned long a, unsigned long b, unsigned long c, unsigned long d)
{
    loc->wuid = a;
    loc->id[0] = b;
    loc->id[1] = c;
    loc->id[2] = d;

    // if a != 0, then <b,c,d> is the xyz location on wilderness 'a'
    // if a == 0 and b != 0 and c:d == 0, then is the static room 'b'
    // if a == 0 and b != 0 and c:d != 0, then is the clone of room 'b' with id c:d
    // if a == 0 and b == 0, then it is nowhere
}

bool rs_location_isset(RS_LOCATION *loc)
{
    return loc && (loc->wuid || loc->id[0]);
}

bool location_isset(LOCATION *loc)
{
    return loc && (loc->wuid || loc->id[0]);
}

float diminishing_returns(float val, float scale)
{
    float mult, trinum;
    if(val < 0)
        return -diminishing_returns(-val, scale);
    mult = val / scale;
    trinum = (sqrt(8.0 * mult + 1.0) - 1.0) / 2.0;
    return trinum * scale;
}

bool is_char_busy(CHAR_DATA *ch)
{
    if(ch == NULL) return false;

    if( ch->cast > 0 ) return true;
    if( ch->bind > 0 ) return true;
    if( ch->bomb > 0 ) return true;
    if( ch->bashed > 0 ) return true;
    if( ch->resurrect > 0 ) return true;
    if( ch->brew > 0 ) return true;
    if( ch->recite > 0 ) return true;
    if( ch->paroxysm > 0 ) return true;
    if( ch->panic > 0 ) return true;
    if( ch->repair > 0 ) return true;
    if( ch->hide > 0 ) return true;
    if( ch->fade > 0 ) return true;
    if( ch->reverie > 0 ) return true;
    if( ch->trance > 0 ) return true;
    if( ch->scribe > 0 ) return true;
    if( ch->inking > 0 ) return true;
    if( ch->music > 0 ) return true;
    if( ch->ranged > 0 ) return true;
    if( ch->script_wait > 0 ) return true;


    return false;
}

bool obj_has_spell(OBJ_DATA *obj, char *name)
{
    SPELL_DATA *spell;
    int sn = skill_lookup(name);

    if( !obj || sn <= 0 ) return false;

    for(spell = obj->spells; spell; spell = spell->next)
        if( spell->sn == sn )
            return true;

    return false;
}

void restore_char(CHAR_DATA *ch, CHAR_DATA *whom, int percent)
{
    int restored;
    affect_strip(ch,skill_resolve_gsn("plague"));
    affect_strip(ch,skill_resolve_gsn("poison"));
    affect_strip(ch,skill_resolve_gsn("blindness"));
    affect_strip(ch,skill_resolve_gsn("sleep"));
    affect_strip(ch,skill_resolve_gsn("curse"));
    affect_strip(ch,skill_resolve_gsn("toxic fumes"));	/* @@@NIB : 20070127*/
    ch->hit 	= ch->max_hit;
    ch->mana	= ch->max_mana;
    ch->move	= ch->max_move;

    percent = URANGE(1, percent, 100);	// Clamp to usable values

    restored = percent * ch->max_hit / 100;
    ch->hit 	= UMAX(ch->hit, restored);

    restored = percent * ch->max_mana / 100;
    ch->mana	= UMAX(ch->mana, restored);

    restored = percent * ch->max_move / 100;
    ch->move	= UMAX(ch->move, restored);

    if (IS_DEAD(ch))
        resurrect_pc(ch);

    if (ch->maze_time_left > 0)
        return_from_maze(ch);

    affect_fix_char(ch);

    // Will only be set when used by the command "restore"
    //  - scripted restores will pass NULL
    if(whom)
        act("$n has restored you.",whom, ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);

    p_percent_trigger( ch, NULL, NULL, NULL, ch, whom, NULL,NULL, NULL, TRIG_RESTORE, NULL);

}


void visit_room_direction(CHAR_DATA *ch, ROOM_INDEX_DATA *start_room, int max_depth, int door, void *data, pVISIT_ROOM_LINE_FUNC func, pVISIT_ROOM_END_FUNC end_func)
{
    int depth;
    int to_x, to_y;
    DESTINATION_DATA dest;
    DESTINATION_DATA nextdest;
    EXIT_DATA *pExit;
    WILDS_VLINK *pVLink = NULL;
    bool canceled = false;

    if( max_depth < 1)
        return;

    // Initialize current destination data to the start room
    dest.room =		nextdest.room =		start_room;
    dest.wilds =	nextdest.wilds =	NULL;
    dest.wx =		nextdest.wx =		0;
    dest.wy =		nextdest.wy =		0;

    for( depth = 1; !canceled && depth <= max_depth; depth++) {

        if( dest.room ) {
            // We have an actual room (static, clone or existing wilds)
            if ((pExit = dest.room->exit[door])) {
                // Hidden exits that haven't been found
                if(IS_SET(pExit->exit_info,EX_HIDDEN) && !IS_SET(pExit->exit_info,EX_FOUND))
                    break;

                // Closed exits
                if(IS_SET(pExit->exit_info, EX_CLOSED))
                    break;

                if(!exit_destination_data(pExit, &nextdest))
                    break;

            } else
                break;

        } else if(dest.wilds) {
            // We have a wilds location, the room has not been loaded

            pVLink = vroom_get_to_vlink(dest.wilds, dest.wx, dest.wy, door);
            if( pVLink != NULL ) {
                if( !pVLink->pDestRoom ) {
                    WNUM dest_wnum;
                    if (resolve_widevnum(pVLink->destvnum, NULL, &dest_wnum))
                        nextdest.room = get_room_index(dest_wnum.pArea, dest_wnum.vnum);
                }
                else
                    nextdest.room = pVLink->pDestRoom;

                if( nextdest.room &&
                    (IS_SET(nextdest.room->room_flag[1], ROOM_BLUEPRINT) ||
                    IS_SET(nextdest.room->area->area_flags, AREA_BLUEPRINT)) )
                {
                    nextdest.room = NULL;
                }

            } else {
                to_x = get_wilds_vroom_x_by_dir(dest.wilds, dest.wx, dest.wy, door);
                to_y = get_wilds_vroom_y_by_dir(dest.wilds, dest.wx, dest.wy, door);

                // Nothing here to reach, so stop
                if( !check_for_bad_room(dest.wilds, to_x, to_y) )
                    break;

                nextdest.room = get_wilds_vroom(dest.wilds, to_x, to_y);
                nextdest.wx = to_x;
                nextdest.wy = to_y;


            }
        } else
            break;

        // We have an actual room
        if(nextdest.room) {
            // Depth 0 == initial room, that should already be taken care of prior to using this function
            if(func)
                canceled = (*func)(nextdest.room, ch, depth, door, data);

/*
            else {
                if( nextdest.room->wilds )
                    printf_to_char(ch, "visit: depth = %d, door = %d, Wilds = <%ld, %d, %d>\n\r", depth, door, nextdest.room->wilds->uid, nextdest.room->x, nextdest.room->y);
                else
                    printf_to_char(ch, "visit: depth = %d, door = %d, Room = <%s>\n\r", depth, door, widevnum_string_room(nextdest.room, ch->in_room->area));
            }*/
        } else {
//			printf_to_char(ch, "visit: depth = %d, door = %d, Unloaded Wilds = <%d, %d>\n\r", depth, door, nextdest.wx, nextdest.wy);
        }

        dest.room =		nextdest.room;
        dest.wilds =	nextdest.wilds;
        dest.wx =		nextdest.wx;
        dest.wy =		nextdest.wy;
    }

    if( end_func && dest.room )
        (*end_func)(dest.room, ch, depth - 1, door, data, canceled);


/*

    for( depth = 1; depth < max_depth; depth++) {
        last_room = dest.room;

        if( pVLink != NULL ) {
            // TODO: VLINKS need FROM-WILD side exit flags

            // Hidden exits that haven't been found
            // Closed exits

            // No room there actually!
            if( !pVLink->pDestRoom )
                dest.room = get_room_index(pVLink->destvnum);
            else
                dest.room = pVLink->pDestRoom;

            if(!dest.room) {

                break;
            }

            (*func)(dest.room, ch, depth, door, data);


            pVLink = NULL;

        } else if( dest.room ) {
            // We have an actual room (static, clone or existing wilds)
            if ((pExit = dest.room->exit[door])) {
                // Hidden exits that haven't been found
                if(IS_SET(pExit->exit_info,EX_HIDDEN) && !IS_SET(pExit->exit_info,EX_FOUND))
                    break;

                // Closed exits
                if(IS_SET(pExit->exit_info, EX_CLOSED))
                    break;

                if(!exit_destination_data(pExit, &dest))
                    break;

                // We have an actual room
                if(dest.room)
                    (*func)(dest.room, ch, depth, door, data);
            } else
                break;

        } else if(dest.wilds) {
            // We have a wilds location, the room has not been loaded

            pVLink = vroom_get_to_vlink(dest.wilds, dest.wx, dest.wy, door);
            if( pVLink != NULL ) {
                continue;

            } else {
                to_x = get_wilds_vroom_x_by_dir(dest.wilds, dest.wx, dest.wy, door);
                to_y = get_wilds_vroom_y_by_dir(dest.wilds, dest.wx, dest.wy, door);

                // Nothing here to reach, so stop
                if( !check_for_bad_room(dest.wilds, to_x, to_y) )
                    break;

                dest.room = get_wilds_vroom(dest.wilds, to_x, to_y);
                dest.wx = to_x;
                dest.wy = to_y;

                if(dest.room) {
                    (*func)(dest.room, ch, depth, door, data);
                }

            }
        } else
            break;

    }

    if( end_func && last_room )
        (*end_func)(last_room, ch, depth, door, data);
*/
}


long dice_roll(DICE_DATA *d)
{
    d->last_roll = dice(d->number, d->size) + d->bonus;

    return d->last_roll;
}

void dice_copy(DICE_DATA *a, DICE_DATA *b)
{
    b->number = a->number;
    b->size = a->size;
    b->bonus = a->bonus;
    b->last_roll = -1;
}

char *get_shop_stock_price(SHOP_STOCK_DATA *stock)
{
    static char buf[4][MSL];
    static int count = 0;

    count = (count + 1) & 3;

    char *pricing = buf[count];

    if( IS_NULLSTR(stock->custom_price) )
    {
        pricing[0] = '\0';
        int pj = 0;

        if( stock->silver > 0)
        {
            long silver = stock->silver % 100;
            long gold = stock->silver / 100;

            if( gold > 0 )
            {
                if( silver > 0 )
                {
                    pj = sprintf(pricing, "{x%ld{Yg{x%ld{Ws{x", gold, silver);
                }
                else
                {
                    pj = sprintf(pricing, "{x%ld{Yg{x", gold);
                }
            }
            else
            {
                pj = sprintf(pricing, "{x%ld{Ws{x", silver);
            }
        }

        if( stock->qp > 0 )
        {
            if( pj > 0 )
            {
                pricing[pj++] = ',';
                pricing[pj++] = ' ';
            }

            pj += sprintf(pricing+pj, "{x%ld{Gqp{x", stock->qp);
        }

        if( stock->dp > 0 )
        {
            if( pj > 0 )
            {
                pricing[pj++] = ',';
                pricing[pj++] = ' ';
            }

            pj += sprintf(pricing+pj, "{x%ld{Mdp{x", stock->dp);
        }

        if( stock->pneuma > 0 )
        {
            if( pj > 0 )
            {
                pricing[pj++] = ',';
                pricing[pj++] = ' ';
            }

            pj += sprintf(pricing+pj, "{x%ld{Cpn{x", stock->pneuma);
        }
        pricing[pj] = '\0';

    }
    else
    {
        strncpy(pricing, stock->custom_price, MSL-3);
        strcat(pricing, "{x");
    }

    return pricing;
}

char *get_shop_purchase_price(long silver, long qp, long dp, long pneuma)
{
    static char buf[4][MSL];
    static int count = 0;

    count = (count + 1) & 3;

    char *pricing = buf[count];
    int pj = 0;

    bool added = false;
    if( silver > 0 )
    {
        long gold = silver / 100;
        silver = silver % 100;

        if( gold > 0 )
        {
            if( silver > 0 )
                pj = sprintf(pricing, " %ld gold, %ld silver", gold, silver);
            else
                pj = sprintf(pricing, " %ld gold", gold);
        }
        else
        {
            pj = sprintf(pricing, " %ld silver", silver);
        }

        added = true;
    }
    if( qp > 0 )
    {
        pj += sprintf(pricing + pj, "%s%ld quest points", (added?", ":" "), qp);
        added = true;
    }
    if( dp > 0 )
    {
        pj += sprintf(pricing + pj, "%s%ld deity points", (added?", ":" "), dp);
        added = true;
    }
    if( pneuma > 0 )
    {
        pj += sprintf(pricing + pj, "%s%ld pneuma", (added?", ":" "), pneuma);
    }

    pricing[pj] = '\0';

    return pricing;
}

bool is_pullable(OBJ_DATA *obj)
{
    if(!IS_VALID(obj))
        return false;

    if( obj->item_type == ITEM_CART ) return true;
    // if( obj->item_type == ITEM_CORPSE_NPC ) return true;
    // if( obj->item_type == ITEM_CORPSE_PC ) return true;


    return false;
}


bool can_room_update(ROOM_INDEX_DATA *room)
{
    if( !room ) return false;

    if( IS_SET(room->room_flag[1], ROOM_ALWAYS_UPDATE) ) return true;

    if( IS_VALID(room->instance_section) )
    {
        INSTANCE *instance = room->instance_section->instance;

        if( IS_VALID(instance) )
        {
            if( IS_VALID(instance->dungeon) )
            {
                // If the dungeon is flagged for unloading, stop normal updates
                if( IS_SET(instance->dungeon->flags, DUNGEON_DESTROY) )
                    return false;

                return list_size(instance->dungeon->players) > 0;
            }
            else
                return list_size(instance->players) > 0;
        }

        return true;
    }

    return !room->area->empty;
}

bool is_area_unlocked(CHAR_DATA *ch, AREA_DATA *area)
{
    // Check player
    if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || (IS_IMMORTAL(ch) && IS_SET(ch->act[1], PLR_HOLYWARP))) return true;

    // Check area
    if( !area || !IS_SET(area->area_flags, AREA_LOCKED) || IS_SET(area->area_flags, AREA_BLUEPRINT) ) return true;

    // Check if the area is in their list
    bool ret = false;

    ITERATOR it;
    AREA_DATA *unlocked;
    iterator_start(&it, ch->pcdata->unlocked_areas);
    while( (unlocked = (AREA_DATA *)iterator_nextdata(&it)) )
    {
        if( unlocked == area )
        {
            ret = true;
            break;
        }
    }
    iterator_stop(&it);

    return ret;
}

bool is_room_unlocked(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    if( !room ||											// Phantom room
        room_is_clone(room) ||								// General clone
        IS_SET(room->room_flag[1],ROOM_VIRTUAL_ROOM) )		// Wilderness room
        return true;

    if( IS_VALID(room->instance_section) )
    {
        DUNGEON *dungeon = get_room_dungeon(room);

        if( IS_VALID(dungeon) )
            return is_dungeon_unlocked(ch, dungeon->index);

        return true;
    }

    return is_area_unlocked(ch, room->area);
}

bool is_dungeon_unlocked(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dungeon_index)
{
    if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || (IS_IMMORTAL(ch) && IS_SET(ch->act[1], PLR_HOLYWARP)) )
        return true;

    if( !dungeon_index || !IS_SET(dungeon_index->flags, DUNGEON_LOCKED) )
        return true;

    ITERATOR it;
    DUNGEON_INDEX_DATA *unlocked;

    iterator_start(&it, ch->pcdata->unlocked_dungeons);
    while( (unlocked = (DUNGEON_INDEX_DATA *)iterator_nextdata(&it)) )
    {
        if( unlocked == dungeon_index )
        {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

void player_unlock_area(CHAR_DATA *ch, AREA_DATA *area)
{
    if( is_area_unlocked(ch, area) ) return;

    list_appendlink(ch->pcdata->unlocked_areas, area);
}

void player_relock_area(CHAR_DATA *ch, AREA_DATA *area)
{
    if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || !area )
        return;

    list_remlink(ch->pcdata->unlocked_areas, area, false);
}

void player_unlock_dungeon(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dungeon_index)
{
    if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || !IS_VALID(dungeon_index) )
        return;

    if( is_dungeon_unlocked(ch, dungeon_index) )
        return;

    list_appendlink(ch->pcdata->unlocked_dungeons, dungeon_index);
}

void player_relock_dungeon(CHAR_DATA *ch, DUNGEON_INDEX_DATA *dungeon_index)
{
    if( !IS_VALID(ch) || IS_NPC(ch) || !IS_VALID(ch->pcdata) || !IS_VALID(dungeon_index) )
        return;

    list_remlink(ch->pcdata->unlocked_dungeons, dungeon_index, false);
}

bool lockstate_functional(LOCK_STATE *lock)
{
    if( !lock ) return false;

    if( list_size(lock->special_keys) > 0 )
    {
        return true;
    }

    if( lock->key_wnum.pArea && lock->key_wnum.vnum > 0 )
    {
        return true;
    }

    return false;
}

OBJ_DATA *lockstate_getkey(CHAR_DATA *ch, LOCK_STATE *lock)
{
    if (!lock) return NULL;

    if (IS_VALID(lock->special_keys))
    {
        LLIST_UID_DATA *luid;
        OBJ_DATA *obj = NULL;
        ITERATOR it;

        iterator_start(&it, lock->special_keys);
        while ((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
        {
            if (luid->ptr)
            {
                obj = (OBJ_DATA *)luid->ptr;

                // In primary inventory?
                if (obj->carried_by == ch && obj->wear_loc == WEAR_NONE)
                    break;

                // Worn directly?
                if (obj->carried_by == ch && obj->wear_loc != WEAR_NONE)
                    break;

                // Inside a keyring that is in primary inventory or worn?
                if (obj->in_obj != NULL && obj->in_obj->item_type == ITEM_KEYRING && obj->in_obj->carried_by == ch)
                    break;
            }
        }
        iterator_stop(&it);

        if (obj || !IS_SET(lock->flags, LOCK_CHECK_BOTH))
            return obj;
    }

    if (lock->key_wnum.pArea && lock->key_wnum.vnum > 0)
    {
        OBJ_DATA *obj;
        OBJ_DATA *key;
        ITERATOR it;
        
        // Check inventory
        if (ch->lcarrying) {
            iterator_start(&it, ch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if (wnum_match_obj(lock->key_wnum, obj)) {
                    iterator_stop(&it);
                    return obj;
                }
                
                // Check keyrings in inventory
                if (obj->item_type == ITEM_KEYRING && obj->contains) {
                    for (key = obj->contains; key != NULL; key = key->next_content) {
                        if (wnum_match_obj(lock->key_wnum, key)) {
                            iterator_stop(&it);
                            return key;
                        }
                    }
                }
            }
            iterator_stop(&it);
        }
        
        // Check worn equipment
        if (ch->lworn) {
            iterator_start(&it, ch->lworn);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if (wnum_match_obj(lock->key_wnum, obj)) {
                    iterator_stop(&it);
                    return obj;
                }
                
                // Check keyrings being worn
                if (obj->item_type == ITEM_KEYRING && obj->contains) {
                    for (key = obj->contains; key != NULL; key = key->next_content) {
                        if (wnum_match_obj(lock->key_wnum, key)) {
                            iterator_stop(&it);
                            return key;
                        }
                    }
                }
            }
            iterator_stop(&it);
        }

        return NULL;
    }

    return NULL;
}

bool lockstate_iskey(LOCK_STATE *lock, OBJ_DATA *key)
{
    if (!lock || !IS_VALID(key)) return false;

    if (IS_VALID(lock->special_keys))
    {
        LLIST_UID_DATA *luid;
        ITERATOR it;

        iterator_start(&it, lock->special_keys);
        while ((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
        {
            if (luid->ptr == key)
            {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);

        if (!IS_SET(lock->flags, LOCK_CHECK_BOTH))
            return false;
    }

    return wnum_match_obj(lock->key_wnum, key);
}

SPECIAL_KEY_DATA *get_special_key(LLIST *list, WNUM wnum)
{
    ITERATOR it;
    SPECIAL_KEY_DATA *sk;

    if( !IS_VALID(list) ) return NULL;

    iterator_start(&it, list);
    while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&it)) )
    {
        if( sk->key_wnum.pArea == wnum.pArea && sk->key_wnum.vnum == wnum.vnum )
            break;
    }
    iterator_stop(&it);

    return sk;
}

void extract_special_key(OBJ_DATA *obj)
{
    ITERATOR skit, kit;
    SPECIAL_KEY_DATA *sk;
    LLIST_UID_DATA *luid;

    iterator_start(&skit, loaded_special_keys);
    while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&skit)) )
    {
        if( sk->key_wnum.pArea == obj->pIndexData->area && sk->key_wnum.vnum == obj->pIndexData->vnum )
        {
            iterator_start(&kit, sk->list);
            while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&kit)) )
            {
                if( luid->id[0] == obj->id[0] &&
                    luid->id[1] == obj->id[1] )
                {
                    luid->ptr = NULL;
                }
            }
            iterator_stop(&kit);

        }
    }
    iterator_stop(&skit);
}

void resolve_special_key(OBJ_DATA *obj)
{
    ITERATOR skit, kit;
    SPECIAL_KEY_DATA *sk;
    LLIST_UID_DATA *luid;

    iterator_start(&skit, loaded_special_keys);
    while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&skit)) )
    {
        if( sk->key_wnum.pArea == obj->pIndexData->area && sk->key_wnum.vnum == obj->pIndexData->vnum )
        {
            iterator_start(&kit, sk->list);
            while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&kit)) )
            {
                if( luid->id[0] == obj->id[0] &&
                    luid->id[1] == obj->id[1] )
                {
                    luid->ptr = obj;
                }
            }
            iterator_stop(&kit);

        }
    }
    iterator_stop(&skit);
}

char *get_article(char *text, bool upper)
{
    switch(UPPER(text[0]))
    {
    case 'A':
    case 'E':
    case 'I':
    case 'O':
    case 'U':
        return upper ? "An" : "an";
    }

    return upper ? "A" : "a";

}

AURA_DATA *find_aura_char(CHAR_DATA *ch, char *name)
{
    ITERATOR it;
    AURA_DATA *aura;

    iterator_start(&it, ch->auras);
    while ((aura = (AURA_DATA *)iterator_nextdata(&it)))
    {
        if (!str_cmp(aura->name, name))
            break;
    }
    iterator_stop(&it);

    return aura;
}

void add_aura_to_char(CHAR_DATA *ch, char *name, char *long_descr)
{
    AURA_DATA *aura;

    if (!IS_VALID(ch) || IS_NULLSTR(name) || IS_NULLSTR(long_descr))
        return;

    aura = find_aura_char(ch, name);

    if (!IS_VALID(aura))
    {
        aura = new_aura_data();
        list_appendlink(ch->auras, aura);

        free_string(aura->name);
        aura->name = str_dup(name);
    }

    free_string(aura->long_descr);
    aura->long_descr = str_dup(long_descr);
}

void remove_aura_from_char(CHAR_DATA *ch, char *name)
{
    ITERATOR it;
    AURA_DATA *aura;

    if (!IS_VALID(ch) || IS_NULLSTR(name))
        return;

    iterator_start(&it, ch->auras);
    while ((aura = (AURA_DATA *)iterator_nextdata(&it)))
    {
        if (!str_cmp(aura->name, name))
        {
            iterator_remcurrent(&it);
            break;
        }
    }
    iterator_stop(&it);
}

char *formatf(const char *fmt, ...)
{
    static int i = 0;
    static char buf[10][MSL*3];

    i = (i + 1) % 10;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf[i], MSL*3, fmt, args);
    va_end(args);

    return buf[i];
}

/*
 * Safe version of formatf that writes to a caller-provided buffer.
 * Use this when you need to preserve the result across multiple formatf/MXP calls.
 */
void formatf_to(char *dest, size_t dest_size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dest, dest_size, fmt, args);
    va_end(args);
}

bool check_social_status(CHAR_DATA *ch)
{
    if (IS_SOCIAL(ch))
    {
        send_to_char("You can't do that while socializing.\n\r", ch);
        return true;
    }

    return false;
}

void send_email_ex(CHAR_DATA *ch, ACCOUNT_DATA *acct, char *email, char *subject, char *message, char *attachment_filename, char *attachment_mime_type)
{
    char subj_buf[256];
    char body_buf[MSL*2];
    char body_buf_html[MSL*5];
    char *recipient_name = NULL;

    extern GAME_SETTINGS_DATA game_settings;

    quickmail_initialize();

    if (subject[0] != '\0')
        sprintf(subj_buf, "%s", subject);
    else
        sprintf(subj_buf, "Email from SentienceMUD");

    quickmail mailobj = quickmail_create(game_settings.email_from_name, game_settings.email_from_addr, subj_buf);

    quickmail_add_to(mailobj, email);

    quickmail_add_header(mailobj, "Importance: Low");
    quickmail_add_header(mailobj, "X-Priority: 5");
    quickmail_add_header(mailobj, "X-MSMail-Priority: Low");

    // Get the appropriate name to address the email
    if (ch)
        recipient_name = ch->name;
    else if (acct)
        recipient_name = acct->username;
    else
        recipient_name = "Adventurer";

        // Create the plain text version of the email
    sprintf(body_buf, "Hello %s,\n\n%s\n\nSincerely,\n\nThe SentienceMUD Staff", recipient_name, message);
    
    // Create the HTML version by converting all newlines to <br/> tags
    char *src = body_buf;
    char *dst = body_buf_html;
    
    // Convert newlines to <br/> tags
    while (*src) {
        if (*src == '\n') {
            strcpy(dst, "<br/>");
            dst += 5;  // Length of "<br/>"
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    quickmail_set_body(mailobj, body_buf);
    quickmail_add_body_memory(mailobj, "text/html", body_buf_html, strlen(body_buf_html), 0);

    if (attachment_filename && attachment_mime_type) {
        quickmail_add_attachment_file(mailobj, attachment_filename, attachment_mime_type);
    }

    const char* errmsg;

    if ((errmsg = quickmail_send(mailobj, game_settings.email_host, game_settings.email_port, game_settings.email_username, game_settings.email_password)) != NULL)
        fprintf(stderr, "Error sending e-mail: %s\n", errmsg);
      quickmail_destroy(mailobj);
      quickmail_cleanup();
}


// Legacy wrapper to maintain backward compatibility
void send_email(CHAR_DATA *ch, char *email, char *subject, char *message, char *attachment_filename, char *attachment_mime_type)
{
    send_email_ex(ch, NULL, email, subject, message, attachment_filename, attachment_mime_type);
}


// Define a structure to hold email-related data
struct EmailData {
    CHAR_DATA *ch;
    ACCOUNT_DATA *acct;
    char *email;
    char *subject;
    char *message;
    char *attachment_filename;
    char *attachment_mime_type;
};

// Function executed by the email thread
void *send_email_thread(void *arg) {
    struct EmailData *emailData = (struct EmailData *)arg;

    send_email_ex(emailData->ch, emailData->acct, emailData->email, emailData->subject, emailData->message, 
                  emailData->attachment_filename, emailData->attachment_mime_type);

    // Clean up and exit the thread
    free(emailData->subject);
    free(emailData->message);
    if (emailData->attachment_filename)
        free(emailData->attachment_filename);
    if (emailData->attachment_mime_type)
        free(emailData->attachment_mime_type);
    free(emailData);
    pthread_exit(NULL);
}

// Function to send an email asynchronously with extended parameters
void send_email_async_ex(CHAR_DATA *ch, ACCOUNT_DATA *acct, char *email, char *subject, char *message, 
                         char *attachment_filename, char *attachment_mime_type) {
    // Allocate memory for the email data
    struct EmailData *emailData = (struct EmailData *)malloc(sizeof(struct EmailData));
    if (!emailData) {
        fprintf(stderr, "Error allocating memory for email data\n");
        return;
    }
    
    emailData->ch = ch;
    emailData->acct = acct;
    emailData->email = email;
    emailData->subject = strdup(subject); // Duplicate the subject string
    if (!emailData->subject) {
        free(emailData);
        return;
    }
    
    emailData->message = strdup(message); // Duplicate the message string
    if (!emailData->message) {
        free(emailData->subject);
        free(emailData);
        return;
    }
    
    emailData->attachment_filename = attachment_filename ? strdup(attachment_filename) : NULL;
    emailData->attachment_mime_type = attachment_mime_type ? strdup(attachment_mime_type) : NULL;

    // Create a new thread for email dispatching
    pthread_t emailThread;
    if (pthread_create(&emailThread, NULL, send_email_thread, emailData) != 0) {
        fprintf(stderr, "Error creating email thread\n");
        // Clean up on error
        free(emailData->subject);
        free(emailData->message);
        if (emailData->attachment_filename)
            free(emailData->attachment_filename);
        if (emailData->attachment_mime_type)
            free(emailData->attachment_mime_type);
        free(emailData);
    }
}

// Legacy wrapper for backward compatibility
void send_email_async(CHAR_DATA *ch, char *email, char *subject, char *message, 
                      char *attachment_filename, char *attachment_mime_type) {
    send_email_async_ex(ch, NULL, email, subject, message, attachment_filename, attachment_mime_type);
}


// Function to return a random character from a given index
char random_char(int index) {
    char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    return charset[index];
}

// Function to generate a random string of a specified length
void generate_reset_code(char* str, int str_len) {
    srand(time(NULL)); // Seed the random number generator

    // Pick the first character in the range 1..15
    *str = random_char(number_range(1,26));
    str++; // Move to the next character

    // Generate the remaining characters
    for (int i = 1; i < str_len; i++) {
        *str = random_char(number_range(1,52)); // Following characters in the range 0..15
        str++;
    }
    str--; // Move back to the last character
    *str = '\0'; // Add the null character at the end
}

char *sha256_crypt(const char *pwd) {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    static char output[65];
    unsigned char sha256sum[32];
    unsigned int j;

    if (context == NULL) {
        return NULL; // Handle error
    }

    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(context);
        return NULL; // Handle error
    }

    if (EVP_DigestUpdate(context, pwd, strlen(pwd)) != 1) {
        EVP_MD_CTX_free(context);
        return NULL; // Handle error
    }

    if (EVP_DigestFinal_ex(context, sha256sum, NULL) != 1) {
        EVP_MD_CTX_free(context);
        return NULL; // Handle error
    }

    for (j = 0; j < 32; ++j) {
        snprintf(output + j * 2, 3, "%02x", sha256sum[j]);
    }

    EVP_MD_CTX_free(context);
    return output;
}

char *tmp_sprintf(const char *fmt, ...)
{
    static char buf[MAX_STRING_LENGTH];
    va_list args;
    
    buf[0] = '\0';
    
    va_start(args, fmt);
    vsnprintf(buf, MAX_STRING_LENGTH, fmt, args);
    va_end(args);
    
    return buf;
}

// Converts total minutes to a formatted string "X week(s) Y day(s) Z hour(s) W minute(s)"
void format_duration(int total_minutes, char *outbuf, size_t outbuf_len) {
    int weeks = total_minutes / (60 * 24 * 7);
    int days = (total_minutes / (60 * 24)) % 7;
    int hours = (total_minutes / 60) % 24;
    int minutes = total_minutes % 60;
    char temp[128];
    temp[0] = '\0';

    if (weeks > 0) {
        snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
                 "%d week%s", weeks, weeks == 1 ? "" : "s");
    }
    if (days > 0) {
        if (temp[0] != '\0') strcat(temp, " ");
        snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
                 "%d day%s", days, days == 1 ? "" : "s");
    }
    if (hours > 0) {
        if (temp[0] != '\0') strcat(temp, " ");
        snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
                 "%d hour%s", hours, hours == 1 ? "" : "s");
    }
    if (minutes > 0 || temp[0] == '\0') {
        if (temp[0] != '\0') strcat(temp, " ");
        snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
                 "%d minute%s", minutes, minutes == 1 ? "" : "s");
    }

    snprintf(outbuf, outbuf_len, "%s", temp);
}

int account_count_nonstaff_characters(ACCOUNT_DATA *acct) {
    int count = 0;
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;

    if (!acct || !acct->characters)
        return 0;

    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!(ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL))
            count++;
    }
    iterator_stop(&it);
    return count;
}

int account_count_staff_characters(ACCOUNT_DATA *acct) {
    int count = 0;
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;

    if (!acct || !acct->characters)
        return 0;

    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL)
            count++;
    }
    iterator_stop(&it);
    return count;
}

char *generate_random_code(int len) {
    static char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static char buf[16];
    for (int i = 0; i < len; ++i)
        buf[i] = charset[rand() % (sizeof(charset) - 1)];
    buf[len] = '\0';
    return buf;
}

void generate_recovery_codes(char **codes, bool *used, int count) {
    for (int i = 0; i < count; ++i) {
        if (codes[i]) free_string(codes[i]);
        codes[i] = str_dup(generate_random_code(10));
        used[i] = false;
    }
}

/**
 * hash_recovery_codes_in_place - Hash plaintext recovery codes in place
 *
 * After recovery codes are displayed to the user, this function hashes them
 * in place so they're never stored in plaintext again.
 *
 * @param codes  Array of recovery code strings to hash
 * @param count  Number of codes in array
 */
void hash_recovery_codes_in_place(char **codes, int count)
{
    for (int i = 0; i < count; ++i) {
        if (IS_NULLSTR(codes[i]))
            continue;

        // Skip if already hashed
        if (is_hashed_recovery_code(codes[i]))
            continue;

        // Hash the plaintext code
        char *hashed = hash_recovery_code(codes[i]);
        if (hashed) {
            free_string(codes[i]);
            codes[i] = str_dup(hashed);
            free_string(hashed);
        }
    }
}

/**
 * hash_recovery_code - Hash a recovery code using Argon2id
 *
 * Uses fast parameters since recovery codes are random and high-entropy.
 * Interactive level is sufficient (64 MiB, ~100ms).
 *
 * @param code  Plain text recovery code to hash
 * @return      Argon2id hash string, or NULL on error
 */
char *hash_recovery_code(const char *code)
{
    if (IS_NULLSTR(code))
        return str_dup("");

    // Use interactive level - recovery codes are random, don't need higher security
    return hash_password_v3(code, PWD_SECURITY_INTERACTIVE);
}

/**
 * is_hashed_recovery_code - Check if recovery code is already hashed
 *
 * @param code  Recovery code string to check
 * @return      true if hashed (Argon2id format), false if plaintext
 */
bool is_hashed_recovery_code(const char *code)
{
    if (IS_NULLSTR(code))
        return false;

    return is_argon2id_hash(code);
}

/**
 * verify_recovery_code_hash - Verify recovery code against hash
 *
 * Handles both hashed (Argon2id) and plaintext codes for migration.
 *
 * @param stored_code  Stored recovery code (hashed or plaintext)
 * @param input_code   User-provided code to verify
 * @return             true if codes match, false otherwise
 */
bool verify_recovery_code_hash(const char *stored_code, const char *input_code)
{
    if (IS_NULLSTR(stored_code) || IS_NULLSTR(input_code))
        return false;

    // If stored code is hashed, verify with Argon2id
    if (is_hashed_recovery_code(stored_code)) {
        return verify_password_v3(stored_code, input_code);
    }

    // Legacy plaintext comparison (for migration)
    return !str_cmp(stored_code, input_code);
}

/**
 * migrate_recovery_codes - Migrate plaintext recovery codes to hashed
 *
 * Note: Recovery codes are one-time use and random. Once displayed to user
 * they cannot be rehashed (we don't have the plaintext). Migration only
 * happens when new codes are generated.
 *
 * This function is a no-op but kept for API consistency.
 *
 * @param codes  Array of recovery code strings
 * @param count  Number of codes in array
 * @return       Number of codes migrated (always 0 - see note)
 */
int migrate_recovery_codes(char **codes, int count)
{
    // Cannot migrate existing codes - they're one-time use random codes
    // that we don't have the plaintext for after they're shown to user.
    // New codes generated will be hashed automatically.
    return 0;
}

bool check_recovery_code(CHAR_DATA *ch, const char *code)
{
    ACCOUNT_DATA *acct = NULL;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = false;
    
    if (!ch || !code || !*code)
        return false;
        
    // Get account character data
    if (ch->desc && ch->desc->account) {
        acct = ch->desc->account;
        has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    }
    
    if (!has_auth_data || !acct_char) {
        // Fall back to character pcdata as legacy support
        for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
            if (!ch->pcdata->recovery_used[i] && verify_recovery_code_hash(ch->pcdata->recovery_codes[i], code)) {
                ch->pcdata->recovery_used[i] = true;
                save_char_obj(ch);
                return true;
            }
        }
        return false;
    }

    // Check against account character recovery codes
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (!acct_char->recovery_used[i] && verify_recovery_code_hash(acct_char->recovery_codes[i], code)) {
            acct_char->recovery_used[i] = true;
            save_account(acct);
            return true;
        }
    }
    
    return false;
}

bool check_account_recovery_code(ACCOUNT_DATA *acct, const char *code) {
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (!acct->recovery_used[i] && verify_recovery_code_hash(acct->recovery_codes[i], code)) {
            acct->recovery_used[i] = true;
            save_account(acct);
            return true;
        }
    }
    return false;
}

// Display recovery codes to the user using account character data
void display_recovery_codes(DESCRIPTOR_DATA *d, ACCOUNT_CHARACTER *acct_char)
{
    bool has_plaintext = false;

    if (!d || !acct_char)
        return;

    write_to_buffer(d, "\n\r{YYour recovery codes (each can be used once):{x\n\r", 0);
    write_to_buffer(d, "{RWARNING: Write these down now! They will not be shown again.{x\n\r\n\r", 0);

    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        char buf[256];

        // Skip empty codes
        if (IS_NULLSTR(acct_char->recovery_codes[i]))
            continue;

        // Check if code is hashed (can't be displayed)
        if (is_hashed_recovery_code(acct_char->recovery_codes[i])) {
            if (acct_char->recovery_used[i])
                sprintf(buf, "{D[code %d] {X(used){x\n\r", i + 1);
            else
                sprintf(buf, "{D[code %d] {Y(secured - cannot display){x\n\r", i + 1);
        } else {
            // Plaintext code - display it
            has_plaintext = true;
            if (acct_char->recovery_used[i])
                sprintf(buf, "{R%s {X(used){x\n\r", acct_char->recovery_codes[i]);
            else
                sprintf(buf, "{G%s{x\n\r", acct_char->recovery_codes[i]);
        }

        write_to_buffer(d, buf, 0);
    }

    // Hash any plaintext codes after displaying them
    if (has_plaintext) {
        hash_recovery_codes_in_place(acct_char->recovery_codes, MFA_RECOVERY_CODES);
        // Note: Caller should save account after this
        if (d->account) {
            save_account(d->account);
        }
    }
}

void display_account_recovery_codes(DESCRIPTOR_DATA *d, ACCOUNT_DATA *acct) {
    bool has_plaintext = false;

    write_to_buffer(d, "\n\r{YYour recovery codes (each can be used once):{x\n\r", 0);
    write_to_buffer(d, "{RWARNING: Write these down now! They will not be shown again.{x\n\r\n\r", 0);

    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        char buf[256];

        // Skip empty codes
        if (IS_NULLSTR(acct->recovery_codes[i]))
            continue;

        // Check if code is hashed (can't be displayed)
        if (is_hashed_recovery_code(acct->recovery_codes[i])) {
            if (acct->recovery_used[i])
                sprintf(buf, "{D[code %d] {X(used){x\n\r", i + 1);
            else
                sprintf(buf, "{D[code %d] {Y(secured - cannot display){x\n\r", i + 1);
        } else {
            // Plaintext code - display it
            has_plaintext = true;
            if (acct->recovery_used[i])
                sprintf(buf, "{R%s {X(used){x\n\r", acct->recovery_codes[i]);
            else
                sprintf(buf, "{G%s{x\n\r", acct->recovery_codes[i]);
        }

        write_to_buffer(d, buf, 0);
    }

    // Hash any plaintext codes after displaying them
    if (has_plaintext) {
        hash_recovery_codes_in_place(acct->recovery_codes, MFA_RECOVERY_CODES);
        save_account(acct);
    }
}

void *delayed_unlink_thread(void *arg) {
    char *filename = (char *)arg;
    sleep(10);
    unlink(filename);
    free(filename);
    return NULL;
}

void delayed_unlink(const char *filename) {
    pthread_t tid;
    char *fname = strdup(filename);
    if (fname) {
        pthread_create(&tid, NULL, delayed_unlink_thread, fname);
        pthread_detach(tid);
    }
}

bool has_recovery_codes(const char **codes, int count) {
    for (int i = 0; i < count; ++i)
        if (!IS_NULLSTR(codes[i]))
            return true;
    return false;
}

ACCOUNT_DATA *get_account_online_or_offline(char *name, bool *was_loaded) {
    ACCOUNT_DATA *account = get_account_by_name(name); // online/in-memory
    if (account) {
        if (was_loaded) *was_loaded = false;
        return account;
    }

    // Not online, try to load from disk
    DESCRIPTOR_DATA d;
    memset(&d, 0, sizeof(d));
    if (!load_account(&d, name)) {
        if (was_loaded) *was_loaded = false;
        return NULL;
    }

    // Add newly loaded account to the global list for tracking
    if (d.account) {
        if (!list_haslink(loaded_accounts, d.account)) {
            list_appendlink(loaded_accounts, d.account);
        }
    }

    if (was_loaded) *was_loaded = true;
    return d.account;
}

ACCOUNT_DATA *get_account_by_name(const char *name) {
    ITERATOR it;
    ACCOUNT_DATA *acct;

    if (IS_NULLSTR(name))
        return NULL;

    iterator_start(&it, loaded_accounts);
    while ((acct = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
        if (!str_cmp(acct->username, name))
            break;
    }
    iterator_stop(&it);

    return acct;
}

bool is_staff_duty_in_list(CHAR_DATA *ch, const char *duty_list)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata || !ch->pcdata->immortal)
        return false;

    long duties = ch->pcdata->immortal->duties;

    // Tokenize the duty_list (space-separated or quoted)
    char duty_name[MAX_INPUT_LENGTH];
    char pbuf [1024];
    strncpy(pbuf, duty_list, sizeof(pbuf));
    pbuf[sizeof(pbuf)-1] = '\0';
    char *p = pbuf;
    while (*p != '\0') {
        p = one_argument(p, duty_name);
        if (duty_name[0] == '\0')
            break;

        int flag = flag_value(immortal_flags, duty_name);
        if (flag != NO_FLAG && IS_SET(duties, flag))
            return true;
    }
    return false;
}

void show_staff_duties(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    buf[0] = '\0';

    for (int i = 0, first = 1; immortal_flags[i].name != NULL; i++) {
        if (!immortal_flags[i].settable)
            continue;
        if (!first)
            strcat(buf, ", ");
        strcat(buf, immortal_flags[i].name);
        first = 0;
    }
    strcat(buf, "\n\r");
    send_to_char("Available staff duties:\n\r", ch);
    send_to_char(buf, ch);
}

bool is_staff_rank_in_list(CHAR_DATA *ch, const char *rank_list)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return false;

    int rank = get_staff_rank(ch);

    char rank_name[MAX_INPUT_LENGTH];
    char pbuf [1024];
    strncpy(pbuf, rank_list, sizeof(pbuf));
    pbuf[sizeof(pbuf)-1] = '\0';
    char *p = pbuf;
    while (*p != '\0') {
        p = one_argument(p, rank_name);
        if (rank_name[0] == '\0')
            break;

        int flag = flag_value(staff_ranks, rank_name);
        if (flag != NO_FLAG && rank >= flag)
            return true;
    }
    return false;
}

void show_staff_ranks(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    buf[0] = '\0';

    for (int i = 0, first = 1; staff_ranks[i].name != NULL; i++) {
        if (!staff_ranks[i].settable)
            continue;
        if (!first)
            strcat(buf, ", ");
        strcat(buf, staff_ranks[i].name);
        first = 0;
    }
    strcat(buf, "\n\r");
    send_to_char("Available staff ranks:\n\r", ch);
    send_to_char(buf, ch);
}

CHURCH_DATA *get_church_by_name(const char *name)
{
    CHURCH_DATA *church;
    ITERATOR it;

    if (IS_NULLSTR(name))
        return NULL;

    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
    {
        if (!str_cmp(church->name, name))
            break;
    }
    iterator_stop(&it);
    return church;
}

bool validate_account_recipient(const char *account_name) {
    bool was_loaded = false;
    ACCOUNT_DATA *acct = get_account_online_or_offline((char *)account_name, &was_loaded);
    if (!acct)
        return false;
    // If we loaded it just for validation, free it now
    if (was_loaded) {
        free_account(acct);
    }
    return true;
}
int colour_trunc_len(const char *str, int limit)
{
    int vis = 0, i = 0;
    int code_len;
    if (!str) return 0;

    while (str[i] && vis < limit) {
        // Handle MUD newline marker
        if (str[i] == '{' && str[i+1] == '|') {
            i += 2;
            vis++; // treat as one visible char (space)
        }
        // Handle color codes (single-char and extended forms)
        else if ((code_len = get_colour_code_length_at_start(str + i)) > 0) {
            i += code_len;
        }
        // Literal newline/CR
        else if (str[i] == '\n' || str[i] == '\r') {
            i++;
            vis++; // treat as one visible char (space)
        }
        else {
            i++;
            vis++;
        }
    }
    return i;
}
ACCOUNT_DATA *get_account_by_identifier(const char *identifier, bool *loaded)
{
    ACCOUNT_DATA *account = NULL;
    
    if (loaded) *loaded = false;
    
    // Try using fixed get_account_online_or_offline 
    if (!strncmp(identifier, "player:", 7))
    {
        // Extract the player name
        const char *player_name = identifier + 7;
        
        // Find the character first
        CHAR_DATA *ch = get_char_world(NULL, (char*)player_name);
        
        if (!ch) {
            // Try loading character
            DESCRIPTOR_DATA temp_d;
            memset(&temp_d, 0, sizeof(temp_d));
            
            if (load_char_obj(&temp_d, (char*)player_name)) {
                ch = temp_d.character;
                
                // Get account info
                if (ch && ch->pcdata && ch->pcdata->account_name[0]) {
                    account = get_account_online_or_offline(ch->pcdata->account_name, loaded);
                }
                
                // Clean up
                free_char(ch);
            }
        }
        else if (ch && ch->pcdata && ch->pcdata->account_name[0]) {
            // Character is online, get account
            account = get_account_online_or_offline(ch->pcdata->account_name, loaded);
        }
    }
    else
    {
        // Direct account lookup
        account = get_account_online_or_offline((char*)identifier, loaded);
    }
    
    // Log for debugging
    if (!account) {
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "Account lookup for '%s' failed - no account found", identifier);
    }
    else if (*loaded) {
        // Ensure loaded accounts get added to the global list
        if (loaded_accounts) {
            list_remlink(loaded_accounts, account, NULL); // Remove if exists
            list_appendlink(loaded_accounts, account);    // Then add back
        }
    }
    
    return account;
}


// Add a utility function to create a normalized filename
char *normalize_filename(const char *name)
{
    static char buf[256];
    char *dest = buf;
    const char *src = name;
    int i;
    
    // Convert to lowercase and replace spaces/symbols with underscores
    for (i = 0; *src && i < 250; src++) {
        if (isalnum(*src))
            *dest++ = tolower(*src);
        else if (*src == ' ' || !isprint(*src) || *src == '/' || *src == '\\' || *src == '.')
            *dest++ = '_';
    }
    *dest = '\0';
    
    return buf;
}

bool is_duplicate_object(OBJ_DATA *obj) {
    if (!obj || (!obj->id[0] && !obj->id[1])) return false;
    OBJ_DATA *existing = loaded_obj_hash_find(obj->id[0], obj->id[1]);
    return (existing != NULL && existing != obj &&
            existing->pIndexData == obj->pIndexData);
}

int get_staff_rank(CHAR_DATA *ch)
{
    if (!IS_VALID(ch) || IS_NPC(ch)) return STAFF_PLAYER;	// Treat NPCs as players in this situation

    return URANGE(STAFF_PLAYER,ch->pcdata->staff_rank,STAFF_IMPLEMENTOR);
}


/**
 * delete_character_by_name - Permanently delete a character pfile by name
 *
 * Moves the character's pfile from the active player directory to the
 * old player archive directory with a timestamp suffix. Also removes the
 * .json summary file and invalidates the Redis cache entry.
 *
 * This variant takes a name string directly, avoiding the need to load
 * an entire CHAR_DATA just to get the name (e.g., during account purge).
 *
 * @param name  The character name to delete
 * @return      true if the pfile was successfully moved, false on error
 */
bool delete_character_by_name(const char *name)
{
    char old_path[MAX_INPUT_LENGTH];
    char new_path[MAX_INPUT_LENGTH];
    char json_path[MAX_INPUT_LENGTH];
    char player_dir_buf[MAX_INPUT_LENGTH];
    char old_player_dir_buf[MAX_INPUT_LENGTH];
    const char *player_dir;
    const char *old_player_dir;
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (IS_NULLSTR(name))
        return false;

    // Format timestamp: DAY_MONTH_YEAR_HOURMINSEC
    strftime(timestamp, sizeof(timestamp), "%d_%m_%Y_%H%M%S", tm_info);

    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    old_player_dir = resolve_game_path(OLD_PLAYER_DIR, old_player_dir_buf, sizeof(old_player_dir_buf));

    // Build source and destination paths using letter subdirectory
    snprintf(old_path, sizeof(old_path), "%s%c/%s",
             player_dir, tolower(name[0]), capitalize(name));
    snprintf(new_path, sizeof(new_path), "%s%s_%s",
             old_player_dir, capitalize(name), timestamp);

    // Invalidate Redis cache
    redis_invalidate_char(name);

    // Try to move the pfile
    if (rename(old_path, new_path) != 0) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
            "delete_character: Failed to move %s to %s: %s",
            old_path, new_path, strerror(errno));
        return false;
    }

    // Remove the .json summary/index file if it exists
    snprintf(json_path, sizeof(json_path), "%s%c/%s.json",
             player_dir, tolower(name[0]), capitalize(name));
    remove(json_path);  // Ignore errors — file may not exist

    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "delete_character: Moved %s to %s", old_path, new_path);

    return true;
}

/**
 * delete_character - Permanently delete a character pfile
 *
 * Convenience wrapper around delete_character_by_name() that extracts
 * the name from a loaded CHAR_DATA.
 *
 * @param ch  The character whose pfile to delete
 * @return    true if the pfile was successfully moved, false on error
 */
bool delete_character(CHAR_DATA *ch)
{
    return delete_character_by_name(ch->name);
}

bool should_purge_deleted_character(const ACCOUNT_CHARACTER *ch_entry) {
    if (!ch_entry->deleted)
        return false;
    if (ch_entry->delete_time == 0)
        return false;
    long delay = game_settings.character_delete_delay_days;
    if (delay <= 0) delay = 30; // Default to 30 days if not set
    return (current_time - ch_entry->delete_time) >= (delay * 86400);
}

void generate_discord_who() {
    char buf[2 * MAX_STRING_LENGTH];
    char level[50];
    DESCRIPTOR_DATA *d;
    int nMatch = 0;
    int nMatch2 = 0;
    CHAR_DATA *wch;
    char classstr[100];
    char racestr[100];
    char *area_type;
    char nocol[2 * MAX_STRING_LENGTH];

    FILE *file;
    char player_list_path_buf[MAX_INPUT_LENGTH];
    const char *player_list_path = resolve_game_path(PLAYER_LIST, player_list_path_buf, sizeof(player_list_path_buf));

    // Open the file for writing
    file = fopen(player_list_path, "w");
    if (!file) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Unable to open player list file for writing.");
        return;
    }

    fprintf(file, "Players in Sentience:\n\n```\n");

    // Count total visible players
    for (d = descriptor_list; d != NULL; d = d->next) {
        if (d->connected != CON_PLAYING)
            continue;

        wch = (d->original != NULL) ? d->original : d->character;

        if (wch) {
            if (IS_IMMORTAL(wch) && (wch->invis_level > 0 || wch->incog_level > 0))
                continue;
            else
                nMatch2++;
        }
    }

    // Generate the who list
    for (d = descriptor_list; d != NULL; d = d->next) {
        wch = (d->original != NULL) ? d->original : d->character;

        if (d->connected != CON_PLAYING || 
            (IS_IMMORTAL(wch) && (wch->invis_level > 0 || wch->incog_level > 0)) || 
            wch->invis_level > 0 || 
            wch->incog_level > 0) {
            continue;
        }

        if (IS_IMMORTAL(wch))
            strcpy(classstr, wch->pcdata->immortal->imm_flag);
        else {
            CLASS_DATA *wch_class = get_current_class(wch);
            if (wch_class)
                strcpy(classstr, class_who_ch(wch_class, wch));
            else
                strcpy(classstr, "Adventurer");
        }

        if (!wch->race || !wch->race->who_name || !wch->race->who_name[0])
            strcpy(racestr, "       ");
        else
            strcpy(racestr, wch->race->who_name);

        nMatch++;

        area_type = get_char_where(wch);

        if (IS_IMMORTAL(wch))
            sprintf(level, "IMM");
        else
            sprintf(level, "%-3d", wch->tot_level);

        // Fix the church name formatting
        char church_buf[100];
        if (wch->church)
            snprintf(church_buf, sizeof(church_buf), "[%s] ", wch->church->flag);
        else
            church_buf[0] = '\0';

        // Use snprintf to prevent buffer overflow
        snprintf(buf, sizeof(buf),
                "[%s] [%-7s %-12s %-6s] %s %s%s%s%s%s%s%s",
                level,
                racestr,
                classstr,
                area_type,
                wch->name,
                church_buf,
                IS_SET(wch->act[0], PLR_BOTTER) ? "[BOTTER] " : "",
                IS_SET(wch->act[0], PLR_HELPER) ? "[HELPER] " : "",
                IS_SET(wch->comm, COMM_AFK) ? "[AFK] " : "",
                IS_SET(wch->comm, COMM_QUIET) ? "[Q] " : "",
                IS_SET(wch->act[0], PLR_PK) ? "[PK] " : "",
                IS_SET(wch->act[0], PLR_BUILDING) ? "[Building] " : ""
        );
        
        // Clear the buffer before stripping colors
        memset(nocol, 0, sizeof(nocol));
        
        // Strip color codes
        STRIP_COLOUR(buf, nocol);

        // Free dynamically allocated memory returned by get_char_where
        free_string(area_type);

        // Write to file
        fprintf(file, "%s\n", nocol);
    }

    if (nMatch != nMatch2) {
        fprintf(file, "\nPlayers found: %d\n", nMatch);
    }
    fprintf(file, "```\nPlayers online: %d\n", nMatch2);
    fprintf(file, "Generated at <t:%ld:F>\n", (long)current_time);

    // Close the file
    fclose(file);
}


// Salt characters for crypt(): ./0-9A-Za-z
static const char crypt_salt_chars[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
#define CRYPT_SALT_METHOD_PREFIX "$6$" // For SHA512-crypt. Use "$5$" for SHA256-crypt. Check your system's crypt(3) man page.
#define CRYPT_SALT_LENGTH 16           // Recommended salt length for SHA512/SHA256 crypt

// Generates a salt string for use with crypt()
bool generate_crypt_salt(char *salt_buffer, size_t salt_buffer_size) {
    if (salt_buffer_size < strlen(CRYPT_SALT_METHOD_PREFIX) + CRYPT_SALT_LENGTH + 1) {

        return false;
    }
    strcpy(salt_buffer, CRYPT_SALT_METHOD_PREFIX);
    char *p = salt_buffer + strlen(CRYPT_SALT_METHOD_PREFIX);
    for (int i = 0; i < CRYPT_SALT_LENGTH; ++i) {
        *p++ = crypt_salt_chars[rand() % (sizeof(crypt_salt_chars) - 1)];
    }
    *p = '\0';
    return true;
}

// Sets/updates a password using the system's crypt()
// Modifies target_password_field (e.g., acct->passwd) and target_version_field.
// Returns true on success, false on failure.
bool set_encrypted_password(char **target_password_field, int *target_version_field, const char *plaintext_password) {
    if (!plaintext_password || !target_password_field || !target_version_field) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "set_encrypted_password: NULL argument.");
        return false;
    }
    if (strlen(plaintext_password) == 0) { // Do not set empty passwords
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "set_encrypted_password: Attempt to set empty password.");
        return false;
    }


    char salt[128];
    if (!generate_crypt_salt(salt, sizeof(salt))) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "set_encrypted_password: Failed to generate salt.");
        // CRITICAL: If salt generation fails, you might fall back to a less secure method or abort.
        // For now, we abort. Ensure rand() is seeded and salt generation is robust.
        return false;
    }

    char *hashed_password = crypt(plaintext_password, salt);
    if (!hashed_password) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "set_encrypted_password: crypt() failed. Salt: %s. Error: %s. Check crypt support for %s.", salt, strerror(errno), CRYPT_SALT_METHOD_PREFIX);
        // This indicates a system-level issue or unsupported crypt method.
        return false;
    }

    free_string(*target_password_field); 
    *target_password_field = str_dup(hashed_password);
    *target_version_field = PWD_VER_CRYPT_SYSTEM;

    return true;
}

// Checks a plaintext password against a stored hash using tiered methods.
password_check_status check_encrypted_password(const char *plaintext_password, const char *stored_hash, int stored_version) {
    if (!plaintext_password || !stored_hash || stored_hash[0] == '\0') {
        return PWD_CHECK_FAIL; // Cannot check against empty stored hash
    }
     if (strlen(plaintext_password) == 0) { // Do not check empty passwords
        return PWD_CHECK_FAIL;
    }


    // 1. Try system crypt() if stored_hash looks like it (starts with '$') or version is PWD_VER_CRYPT_SYSTEM
    if (stored_hash[0] == '$' || stored_version == PWD_VER_CRYPT_SYSTEM) {
        char *crypted_input = crypt(plaintext_password, stored_hash);
        if (crypted_input && strcmp(crypted_input, stored_hash) == 0) {
            return PWD_CHECK_SUCCESS_CRYPT_SYSTEM;
        }
    }

    // 2. Fallback to custom sha256_crypt()
    // This assumes your sha256_crypt() is deterministic and returns a hex string.
    // Be wary of the static buffer in your sha256_crypt if this function were called in more complex ways.
    if (stored_version == PWD_VER_SHA256_CUSTOM || (stored_hash[0] != '$' && strlen(stored_hash) == 64)) { // Heuristic for sha256 hex
        char *custom_hashed = sha256_crypt(plaintext_password); // Uses your existing function
        if (custom_hashed && strcmp(custom_hashed, stored_hash) == 0) {
            return PWD_CHECK_SUCCESS_SHA256_CUSTOM;
        }
    }
    
    // 3. Fallback to plaintext comparison
    // Only attempt plaintext if it's explicitly marked as such or doesn't look like a crypt hash.
    if (stored_version == PWD_VER_PLAINTEXT && stored_hash[0] != '$') { 
        if (strcmp(plaintext_password, stored_hash) == 0) {
            return PWD_CHECK_SUCCESS_PLAINTEXT;
        }
    }

    return PWD_CHECK_FAIL;
}

bool is_valid_colour_code(const char *code) {
    const char *p = code;
    while (*p) {
        if (*p != '{')
            return false;
        p++;
        // Single-char code: e.g. {Y
        if (*p && strchr("xXrRbBwWcCmMgGyYD01234567aAjJlLoOpPtTvV", *p)) {
            p++;
            continue;
        }
        // Extended code: {[F###] or {[B###]
        if (*p == '[') {
            p++;
            if (*p == 'F' || *p == 'B') {
                p++;
                // Must be 3 digits
                for (int i = 0; i < 3; i++) {
                    if (!isdigit(*p) || *p < '0' || *p > '5')
                        return false;
                    p++;
                }
                if (*p == ']') {
                    p++;
                    continue;
                }
            }
            return false;
        }
        // Special codes: {i, {f
        if (*p == 'i' || *p == 'f') {
            p++;
            continue;
        }
        return false;
    }
    return true;
}

int get_colour_code_length_at_start(const char *p) {
    if (*p != COLOUR_CHAR) {
        return 0; // Not starting with a color code opener
    }

    const char *start = p;
    p++; // Move past '{'

    if (!*p) return 0; // Unterminated '{'

    // Single-char code: e.g. {Y
    if (strchr("xXrRbBwWcCmMgGyYD01234567aAjJlLoOpPtTvV", *p)) {
        return (p + 1) - start; // Should be 2
    }

    // Extended code: {[F###] or {[B###]
    if (*p == '[') {
        const char *ext_start = p;
        (void)ext_start;
        p++; // Move past '['
        if (*p == 'F' || *p == 'B') {
            p++; // Move past F or B
            // Must be 3 digits
            for (int i = 0; i < 3; i++) {
                if (!*p || !isdigit(*p) || *p < '0' || *p > '5') { // Added !*p check
                    return 0; // Invalid extended code format
                }
                p++;
            }
            if (*p == ']') {
                return (p + 1) - start; // Should be 7
            }
        }
        return 0; // Invalid extended code format (e.g., missing F/B or closing ']')
    }

    // Special codes: {i, {f
    if (*p == 'i' || *p == 'f') {
        return (p + 1) - start; // Should be 2
    }

    return 0; // Not a recognized color code pattern after '{'
}

/*
 * Safely check if a void pointer is actually an LLIST structure
 * Returns true if the pointer is a valid LLIST, false otherwise
 */
bool is_llist(const void *ptr)
{
    // First ensure the pointer isn't NULL and has reasonable alignment
    if (!ptr || ((unsigned long)ptr & 3))
        return false;

    // Try to safely check if this has an LLIST structure format
    // We need to be extremely careful about dereferencing here
    const LLIST *potential_list = (const LLIST *)ptr;
    
    // First check if the pointer is valid memory
    // Using minimal checks: check valid and identifier fields
    // without dereferencing other pointers within the structure
    if (potential_list->valid && 
        potential_list->identifier == LLIST_IDENT)
        return true;

    return false;
}


/**
 * load_or_generate_salt - Load or generate salt for key derivation
 *
 * Loads salt from file for a specific key version, or generates new salt if
 * file doesn't exist. Salt files are named: <base_path>.v<version>
 *
 * @param base_path  Base path for salt file (without version)
 * @param version    Key version number
 * @param salt_out   Buffer to store 32-byte salt (must be allocated)
 * @return           true on success, false on error
 */
static bool load_or_generate_salt(const char *base_path, int version, unsigned char *salt_out)
{
    char salt_file_path[256];
    char resolved_base_path[256];
    const char *base_path_resolved;
    FILE *salt_file;

    if (!base_path || !*base_path) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "load_or_generate_salt: Empty base path");
        return false;
    }

    base_path_resolved = resolve_game_path(base_path, resolved_base_path, sizeof(resolved_base_path));

    // Construct versioned salt file path: base_path.v1, base_path.v2, etc.
    snprintf(salt_file_path, sizeof(salt_file_path), "%s.v%d", base_path_resolved, version);

    // Try to load existing salt
    salt_file = fopen(salt_file_path, "rb");
    if (salt_file) {
        if (fread(salt_out, 1, AES_KEY_SIZE, salt_file) != AES_KEY_SIZE) {
            log_message_f(LOG_LEVEL_WARN, LOG_WARN, "Failed to read salt file %s, generating new salt", salt_file_path);
            randombytes_buf(salt_out, AES_KEY_SIZE);
            fclose(salt_file);

            // Save the newly generated salt
            salt_file = fopen(salt_file_path, "wb");
            if (salt_file) {
                fwrite(salt_out, 1, AES_KEY_SIZE, salt_file);
                fclose(salt_file);
                chmod(salt_file_path, 0600);
            }
        } else {
            fclose(salt_file);
            log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Loaded salt from %s", salt_file_path);
        }
        return true;
    }

    // Generate new salt
    randombytes_buf(salt_out, AES_KEY_SIZE);

    // Save salt to file
    salt_file = fopen(salt_file_path, "wb");
    if (salt_file) {
        fwrite(salt_out, 1, AES_KEY_SIZE, salt_file);
        fclose(salt_file);
        chmod(salt_file_path, 0600);
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Generated new salt file: %s", salt_file_path);
        return true;
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to create salt file: %s", salt_file_path);
        return false;
    }
}

/**
 * derive_key_from_passphrase - Derive key from specific passphrase and version
 *
 * Helper function to derive a key from a given passphrase and version.
 * Used for both current and previous keys during rotation.
 *
 * @param passphrase   Passphrase to derive from
 * @param version      Key version (determines salt file)
 * @param salt_base    Base path for salt file
 * @param key_out      Output buffer for derived key (must be AES_KEY_SIZE)
 * @return             true on success, false on error
 */
bool derive_key_from_passphrase(const char *passphrase, int version,
                                const char *salt_base, unsigned char *key_out)
{
    unsigned char salt[AES_KEY_SIZE];

    if (IS_NULLSTR(passphrase)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "derive_key_from_passphrase: NULL passphrase");
        return false;
    }

    // Load or generate salt for this version
    if (!load_or_generate_salt(salt_base, version, salt)) {
        return false;
    }

    // Derive key using Argon2id
    if (crypto_pwhash(key_out, AES_KEY_SIZE,
                     passphrase, strlen(passphrase),
                     salt,
                     crypto_pwhash_OPSLIMIT_MODERATE,
                     crypto_pwhash_MEMLIMIT_MODERATE,
                     crypto_pwhash_ALG_ARGON2ID13) != 0) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "derive_key_from_passphrase: Key derivation failed");
        return false;
    }

    return true;
}

/**
 * crypto_init_from_passphrase - Derive crypto key from passphrase
 *
 * Uses Argon2id to derive a 32-byte encryption key from a passphrase and salt.
 * The salt is version-specific to enable key rotation.
 *
 * Supports rotation mode: When crypto_key_passphrase_previous is set, the system
 * is in rotation mode and can decrypt with old key, encrypt with new key.
 *
 * @return  true on success, false on error
 */
static bool crypto_init_from_passphrase(void)
{
    const char *passphrase;
    const char *passphrase_previous;
    const char *salt_file_base;
    int key_version;
    bool in_rotation_mode;

    // Get passphrase from game settings (supports env vars via existing logic)
    passphrase = game_settings.crypto_key_passphrase;
    if (IS_NULLSTR(passphrase)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "crypto_init: Passphrase mode enabled but no passphrase provided");
        return false;
    }

    // Check if we're in rotation mode
    passphrase_previous = game_settings.crypto_key_passphrase_previous;
    in_rotation_mode = !IS_NULLSTR(passphrase_previous);

    // Get salt file path (default if not specified)
    salt_file_base = game_settings.crypto_salt_file;
    if (IS_NULLSTR(salt_file_base)) {
        salt_file_base = SYSTEM_DIR "crypto_salt";
    }

    // Get key version
    key_version = game_settings.crypto_key_version;
    if (key_version < 1) {
        key_version = 1;
    }

    // Derive current key (always use current passphrase for encryption)
    if (!derive_key_from_passphrase(passphrase, key_version, salt_file_base, crypto_key)) {
        return false;
    }

    if (in_rotation_mode) {
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
            "Crypto key derived from passphrase (version %d) - ROTATION MODE ACTIVE", key_version);
        log_message(LOG_LEVEL_WARN, LOG_WARN,
            "Key rotation mode detected. Use 'cryptorotate' command to re-encrypt data.");
    } else {
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
            "Crypto key derived from passphrase (version %d)", key_version);
    }

    return true;
}

/**
 * crypto_init_from_file - Initialize crypto key from file (legacy method)
 *
 * Loads or generates a random 32-byte key stored in plaintext file.
 * This is the original method and is retained for backward compatibility.
 *
 * @return  true on success, false on error
 */
static bool crypto_init_from_file(void)
{
    FILE *key_file;
    char key_path_buf[MAX_INPUT_LENGTH];
    const char *key_path = resolve_game_path(MFA_ENC_KEY, key_path_buf, sizeof(key_path_buf));

    key_file = fopen(key_path, "rb");
    if (key_file) {
        // Read existing key
        if (fread(crypto_key, 1, AES_KEY_SIZE, key_file) != AES_KEY_SIZE) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Failed to read crypto key file, generating new one");
            RAND_bytes(crypto_key, AES_KEY_SIZE);
        } else {
            log_message(LOG_LEVEL_INFO, LOG_INIT, "Loaded crypto key from file");
        }
        fclose(key_file);
    } else {
        // Generate and save a new key
        RAND_bytes(crypto_key, AES_KEY_SIZE);
        key_file = fopen(key_path, "wb");
        if (key_file) {
            fwrite(crypto_key, 1, AES_KEY_SIZE, key_file);
            fclose(key_file);
            chmod(key_path, 0600);
            log_message(LOG_LEVEL_INFO, LOG_INIT, "Generated new crypto key file");
        } else {
            log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to create crypto key file");
            return false;
        }
    }

    return true;
}

/**
 * crypto_init - Initialize the server-side crypto key
 *
 * Initializes the global crypto_key used for encrypting sensitive data (OTP keys,
 * recovery codes, etc.). Checks game_settings.crypto_use_passphrase to determine
 * whether to use passphrase-based key derivation or file-based key storage.
 *
 * Safe to call multiple times (subsequent calls are no-ops).
 */
void crypto_init(void)
{
    bool success;

    if (key_initialized)
        return;

    // Initialize libsodium (safe to call multiple times)
    if (!init_sodium()) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "crypto_init: Failed to initialize libsodium");
        return;
    }

    // Choose initialization method based on game settings
    if (game_settings.crypto_use_passphrase) {
        success = crypto_init_from_passphrase();
    } else {
        success = crypto_init_from_file();
    }

    if (success) {
        key_initialized = true;
    } else {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "crypto_init: Failed to initialize crypto key");
    }
}

/*
 * Check if a string appears to be an encrypted key
 * This version also recognizes strings with linebreaks as encrypted
 */
bool is_encrypted_key(const char *str)
{
    if (IS_NULLSTR(str))
        return false;
    
    // Base64 encoded data will contain characters from this set: A-Za-z0-9+/=
    // Plus potential newlines/carriage returns
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    int valid_chars = 0;
    int len = strlen(str);
    
    for (int i = 0; i < len; i++) {
        if (strchr(base64_chars, str[i]) != NULL || str[i] == '\r' || str[i] == '\n')
            valid_chars++;
    }
    
    // Check that at least 90% of characters are valid base64 chars or newlines
    // and that the length is reasonable for an encrypted key (>16)
    return (valid_chars > len * 0.9 && len > 16);
}

/*
 * Normalize an encrypted string by removing linebreaks
 * This ensures consistent handling regardless of how it was stored
 */
char *normalize_encrypted_key(const char *encrypted)
{
    if (IS_NULLSTR(encrypted))
        return str_dup("");
        
    char *result = malloc(strlen(encrypted) + 1);
    if (!result)
        return str_dup("");
        
    int j = 0;
    for (int i = 0; encrypted[i]; i++) {
        if (encrypted[i] != '\r' && encrypted[i] != '\n')
            result[j++] = encrypted[i];
    }
    result[j] = '\0';
    
    return result;
}

/*
 * Encrypt a string using AES-256-CBC
 * Creates a string safe for file storage (no linebreaks)
 */
char* encrypt_string(const char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    unsigned char *ciphertext, *output;
    unsigned char iv[AES_IV_SIZE];
    unsigned char salt[CRYPTO_SALT_SIZE];
    int len, ciphertext_len = 0;
    size_t output_len;
    
    if (!key_initialized)
        crypto_init();
    
    if (!plaintext || !*plaintext)
        return str_dup("");
    
    // Generate a random IV and salt for each encryption
    RAND_bytes(iv, AES_IV_SIZE);
    RAND_bytes(salt, CRYPTO_SALT_SIZE);
    
    // Allocate memory for the ciphertext
    ciphertext = malloc(strlen(plaintext) + AES_IV_SIZE + CRYPTO_SALT_SIZE + EVP_MAX_BLOCK_LENGTH);
    if (!ciphertext)
        return str_dup("");
    
    // Create and initialize the context
    ctx = EVP_CIPHER_CTX_new();
    
    // Initialize encryption
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, crypto_key, iv);
    
    // Encrypt: First copy the IV and salt directly to the output buffer
    memcpy(ciphertext, iv, AES_IV_SIZE);
    memcpy(ciphertext + AES_IV_SIZE, salt, CRYPTO_SALT_SIZE);
    ciphertext_len = AES_IV_SIZE + CRYPTO_SALT_SIZE;
    
    // Perform encryption
    EVP_EncryptUpdate(ctx, ciphertext + ciphertext_len, &len, 
                     (unsigned char*)plaintext, strlen(plaintext));
    ciphertext_len += len;
    
    // Finalize encryption
    EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len);
    ciphertext_len += len;
    
    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    
    // Base64 encode the result (IV + salt + ciphertext)
    output = (unsigned char*)base64_encode(ciphertext, ciphertext_len, &output_len);
    free(ciphertext);
    
    // Clean up any linebreaks in the output to ensure consistent storage
    char *normalized = normalize_encrypted_key((char*)output);
    free(output);
    
    return normalized;
}

/*
 * Decrypt a string using AES-256-CBC
 * Handles potential linebreaks in input
 */
char* decrypt_string(const char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;
    unsigned char *ciphertext_binary, *plaintext;
    unsigned char iv[AES_IV_SIZE];
    int len, plaintext_len = 0;
    size_t ciphertext_len;
    char *result;
    
    if (!key_initialized)
        crypto_init();
    
    if (!ciphertext || !*ciphertext)
        return str_dup("");
    
    // Normalize the input to remove any linebreaks
    char *normalized_input = normalize_encrypted_key(ciphertext);
    
    // Base64 decode
    ciphertext_binary = base64_decode(normalized_input, strlen(normalized_input), &ciphertext_len);
    free(normalized_input);
    
    if (!ciphertext_binary || ciphertext_len <= AES_IV_SIZE + CRYPTO_SALT_SIZE) {
        if (ciphertext_binary) free(ciphertext_binary);
        return str_dup("");
    }
    
    // Extract the IV (first AES_IV_SIZE bytes)
    memcpy(iv, ciphertext_binary, AES_IV_SIZE);
    
    // Allocate memory for the plaintext
    plaintext = malloc(ciphertext_len);
    if (!plaintext) {
        free(ciphertext_binary);
        return str_dup("");
    }
    
    // Create and initialize the context
    ctx = EVP_CIPHER_CTX_new();
    
    // Initialize decryption
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, crypto_key, iv);
    
    // Decrypt (skip the IV and salt in the input)
    EVP_DecryptUpdate(ctx, plaintext, &len, 
                     ciphertext_binary + AES_IV_SIZE + CRYPTO_SALT_SIZE, 
                     ciphertext_len - AES_IV_SIZE - CRYPTO_SALT_SIZE);
    plaintext_len = len;
    
    // Finalize decryption
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len;
    
    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    free(ciphertext_binary);
    
    // Null-terminate the plaintext
    plaintext[plaintext_len] = '\0';
    result = str_dup((char*)plaintext);
    free(plaintext);
    
    return result;
}

/**
 * detect_encryption_version - Detect encryption format version
 *
 * Checks if encrypted string uses v2 authenticated encryption (XSalsa20-Poly1305)
 * or v1 legacy AES-CBC encryption.
 *
 * @param ciphertext  Encrypted string to analyze
 * @return            2 for v2 (has "v2:" prefix), 1 for v1 (legacy)
 */
int detect_encryption_version(const char *ciphertext)
{
    if (IS_NULLSTR(ciphertext))
        return 1;

    // v2 format starts with "v2:" prefix
    if (strncmp(ciphertext, "v2:", 3) == 0)
        return 2;

    // Default to v1 (legacy AES-CBC)
    return 1;
}

/**
 * encrypt_string_versioned - Encrypt string using current encryption version
 *
 * Uses v2 (XSalsa20-Poly1305 authenticated encryption) for new encryptions.
 * Output format: "v2:<base64_encrypted_data>"
 *
 * @param plaintext  Plain text string to encrypt
 * @return           Encrypted string with version prefix, or NULL on error
 */
char *encrypt_string_versioned(const char *plaintext)
{
    char *encrypted;
    char *result;
    size_t result_len;

    if (!key_initialized)
        crypto_init();

    if (IS_NULLSTR(plaintext))
        return str_dup("");

    // Use v2 authenticated encryption (libsodium)
    encrypted = encrypt_string_v2(plaintext, crypto_key);
    if (!encrypted)
        return str_dup("");

    // Add "v2:" prefix
    result_len = strlen(encrypted) + 4; // "v2:" + encrypted + null
    result = malloc(result_len);
    if (!result) {
        free_string(encrypted);
        return str_dup("");
    }

    snprintf(result, result_len, "v2:%s", encrypted);
    free_string(encrypted);

    return result;
}

/**
 * decrypt_string_versioned - Decrypt string with automatic version detection
 *
 * Detects encryption version and calls appropriate decryption function:
 * - v2: XSalsa20-Poly1305 authenticated encryption (with tamper detection)
 * - v1: Legacy AES-256-CBC (no authentication)
 *
 * @param ciphertext  Encrypted string (with or without version prefix)
 * @return            Decrypted plain text, or empty string on error
 */
char *decrypt_string_versioned(const char *ciphertext)
{
    int version;
    const char *encrypted_data;

    if (!key_initialized)
        crypto_init();

    if (IS_NULLSTR(ciphertext))
        return str_dup("");

    version = detect_encryption_version(ciphertext);

    if (version == 2) {
        // v2: Skip "v2:" prefix and decrypt with libsodium
        encrypted_data = ciphertext + 3;
        return decrypt_string_v2(encrypted_data, crypto_key);
    } else {
        // v1: Legacy AES-CBC decryption
        return decrypt_string(ciphertext);
    }
}

/**
 * needs_encryption_upgrade - Check if encrypted data needs upgrade to v2
 *
 * @param ciphertext  Encrypted string to check
 * @return            true if upgrade needed (v1 format), false if current (v2)
 */
bool needs_encryption_upgrade(const char *ciphertext)
{
    if (IS_NULLSTR(ciphertext))
        return false;

    return detect_encryption_version(ciphertext) < 2;
}

/**
 * Base64 encode using OpenSSL
 */
unsigned char *base64_encode(const unsigned char *src, size_t len, size_t *out_len)
{
    EVP_ENCODE_CTX *ctx;
    unsigned char *out;
    int outlen, tlen;
    
    if (!src || len == 0)
        return NULL;
    
    // Allocate enough space for the encoded data (4/3 ratio plus padding)
    *out_len = ((len + 2) / 3) * 4 + 1;  // +1 for null terminator
    out = malloc(*out_len);
    if (!out)
        return NULL;
    
    ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        free(out);
        return NULL;
    }
    
    EVP_EncodeInit(ctx);
    EVP_EncodeUpdate(ctx, out, &outlen, src, len);
    EVP_EncodeFinal(ctx, out + outlen, &tlen);
    EVP_ENCODE_CTX_free(ctx);
    
    *out_len = outlen + tlen;
    out[*out_len] = '\0';  // Null terminate for string usage
    
    return out;
}

/**
 * Base64 decode using OpenSSL
 */
unsigned char *base64_decode(const char *src, size_t len, size_t *out_len)
{
    EVP_ENCODE_CTX *ctx;
    unsigned char *out;
    int outlen, tlen;
    
    if (!src || len == 0)
        return NULL;
    
    // Allocate enough space for the decoded data (3/4 ratio)
    *out_len = ((len + 3) / 4) * 3 + 1;  // +1 for null terminator
    out = malloc(*out_len);
    if (!out)
        return NULL;
    
    ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        free(out);
        return NULL;
    }
    
    EVP_DecodeInit(ctx);
    if (EVP_DecodeUpdate(ctx, out, &outlen, (unsigned char*)src, len) < 0) {
        EVP_ENCODE_CTX_free(ctx);
        free(out);
        return NULL;
    }
    
    if (EVP_DecodeFinal(ctx, out + outlen, &tlen) < 0) {
        EVP_ENCODE_CTX_free(ctx);
        free(out);
        return NULL;
    }
    
    EVP_ENCODE_CTX_free(ctx);
    
    *out_len = outlen + tlen;
    out[*out_len] = '\0';  // Null terminate for string usage
    
    return out;
}


/**
 * Checks if a password matches any staff character password in an account
 * @param acct The account to check
 * @param plaintext_password The plaintext password to check against
 * @param exclude_char Character to exclude from the check (useful when changing a specific character's password)
 * @return true if the password matches any staff character's password, false otherwise
 */
bool password_matches_staff_character(ACCOUNT_DATA *acct, const char *plaintext_password, const char *exclude_name) {
    ITERATOR it;
    ACCOUNT_CHARACTER *acct_char;
    
    if (!acct || IS_NULLSTR(plaintext_password))
        return false;
        
    iterator_start(&it, acct->characters);
    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        // Skip characters that aren't staff
        if (!acct_char->staff || acct_char->staff_rank < STAFF_IMMORTAL)
            continue;
            
        // Skip the excluded character if provided
        if (!IS_NULLSTR(exclude_name) && !str_cmp(exclude_name, acct_char->name))
            continue;
            
        // Skip if the character has no password
        if (IS_NULLSTR(acct_char->pwd))
            continue;
            
        // Check if the plaintext password matches this character's password
        if (check_encrypted_password(plaintext_password, acct_char->pwd, acct_char->pwd_vers) != PWD_CHECK_FAIL) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);
    
    return false;
}

/**
 * Checks if a password matches the account's password
 * @param acct The account to check
 * @param plaintext_password The plaintext password to check against
 * @return true if the password matches the account password, false otherwise
 */
bool password_matches_account(ACCOUNT_DATA *acct, const char *plaintext_password) {
    if (!acct || IS_NULLSTR(plaintext_password) || IS_NULLSTR(acct->passwd))
        return false;
        
    return (check_encrypted_password(plaintext_password, acct->passwd, acct->passwd_version) != PWD_CHECK_FAIL);
}

/**
 * Comprehensive password check that enforces staff password uniqueness rules
 * @param acct The account to check
 * @param plaintext_password The password to validate
 * @param is_for_character Whether this is for a character password
 * @param character_name If for a character, which character (can be NULL)
 * @param is_staff Whether the character is a staff member
 * @return true if the password is valid according to uniqueness rules, false otherwise
 */
bool validate_password_uniqueness(ACCOUNT_DATA *acct, const char *plaintext_password, 
                                bool is_for_character, const char *character_name, bool is_staff) {
    // Don't enforce rules if feature is disabled
    if (!game_settings.require_uniq_pass_staff)
        return true;
        
    // Case 1: Updating account password
    if (!is_for_character) {
        // Check against any staff character passwords
        if (password_matches_staff_character(acct, plaintext_password, NULL)) {
            return false;  // Account password matches a staff character password
        }
    }
    
    // Case 2: Setting/updating a staff character password
    else if (is_for_character && is_staff) {
        // Check against the account password
        if (password_matches_account(acct, plaintext_password)) {
            return false;  // Character password matches account password
        }
        
        // Check against ALL other character passwords (staff or not)
        ITERATOR it;
        ACCOUNT_CHARACTER *acct_char;
        
        iterator_start(&it, acct->characters);
        while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
            // Skip the current character
            if (!IS_NULLSTR(character_name) && !str_cmp(character_name, acct_char->name))
                continue;
                
            // Skip if the character has no password
            if (IS_NULLSTR(acct_char->pwd))
                continue;
                
            // Check if the plaintext password matches this character's password
            if (check_encrypted_password(plaintext_password, acct_char->pwd, acct_char->pwd_vers) != PWD_CHECK_FAIL) {
                iterator_stop(&it);
                return false;  // Character password matches another character's password
            }
        }
        iterator_stop(&it);
    }
    
    // Case 3: Setting/updating a non-staff character password
    else if (is_for_character && !is_staff) {
        // Check against any staff character passwords
        if (password_matches_staff_character(acct, plaintext_password, character_name)) {
            return false;  // Non-staff character password matches a staff character password
        }
    }
    
    // Password passes all uniqueness checks
    return true;
}






// For game setting lookup
char *get_game_setting_value(char *setting_name, bool *sensitive)
{
    static char value_buffer[MAX_STRING_LENGTH];
    const struct game_setting_type *setting = NULL;
    
    // Find setting in game_settings_table
    for (int i = 0; game_settings_table[i].name; i++) {
        if (!str_cmp(setting_name, game_settings_table[i].name)) {
            setting = &game_settings_table[i];
            break;
        }
    }
    
    if (!setting) {
        *sensitive = false;
        strcpy(value_buffer, "");
        return value_buffer;
    }
    
    *sensitive = setting->sensitive;
    if (sensitive && script_security < 9) {
        strcpy(value_buffer, "*****");
        return value_buffer;
    }
    
    // Format the setting value based on type
    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            sprintf(value_buffer, "%s", *(bool*)setting->ptr ? "true" : "false");
            break;
        case SETTING_TYPE_INT:
            sprintf(value_buffer, "%d", *(int*)setting->ptr);
            break;
        case SETTING_TYPE_STRING:
        case SETTING_TYPE_EXTSTR:
            sprintf(value_buffer, "%s", (char*)setting->ptr);
            break;
        case SETTING_TYPE_FLOAT:
            sprintf(value_buffer, "%.2f", *(float*)setting->ptr);
            break;
        default:
            strcpy(value_buffer, "");
            break;
    }
    
    return value_buffer;
}

AREA_DATA *get_system_area_fallback(void)
{
    AREA_DATA *area = NULL;

    if (!IS_NULLSTR(game_settings.system_area)) {
        if (is_number(game_settings.system_area))
            area = get_area_index(atol(game_settings.system_area));
        else
            area = find_area(game_settings.system_area);
    }

    if (!area)
        area = area_first;

    return area;
}

/*
 * Helper function to get area data by reserved name
 */
AREA_DATA *get_reserved_area_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_AREA && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            if (reserved->wnum.auid > 0)
                return get_area_index(reserved->wnum.auid);

            return get_system_area_fallback();
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get token index data by reserved name
 */
TOKEN_INDEX_DATA *get_reserved_token_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_TOKEN && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_token_index(wnum.pArea, wnum.vnum);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get room prog index by reserved name
 */
SCRIPT_DATA *get_reserved_rprog_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_RPROG && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get object prog index by reserved name
 */
SCRIPT_DATA *get_reserved_oprog_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_OPROG && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_script_index(wnum.pArea, wnum.vnum, PRG_OPROG);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get mobile prog index by reserved name
 */
SCRIPT_DATA *get_reserved_mprog_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_MPROG && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get token prog index by reserved name
 */
SCRIPT_DATA *get_reserved_tprog_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_TPROG && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_script_index(wnum.pArea, wnum.vnum, PRG_TPROG);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get area prog index by reserved name
 */
SCRIPT_DATA *get_reserved_aprog_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_APROG && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea) {
                wnum.pArea = get_system_area_fallback();
            }
            wnum.vnum = reserved->wnum.vnum;
            return get_script_index(wnum.pArea, wnum.vnum, PRG_APROG);
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

const struct game_setting_type *get_game_setting(const char *name)
{
    extern const struct game_setting_type game_settings_table[];
    
    for (int i = 0; game_settings_table[i].name != NULL; i++) {
        if (!str_cmp(game_settings_table[i].name, name)) {
            return &game_settings_table[i];
        }
    }
    
    return NULL;
}

/*
 * Helper function to get mobile index by reserved name
 */
MOB_INDEX_DATA *get_reserved_mob_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;
    AREA_DATA *area;
    MOB_INDEX_DATA *mob;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;
        
    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_MOB && 
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            wnum.vnum = reserved->wnum.vnum;

            if (wnum.pArea) {
                mob = get_mob_index(wnum.pArea, wnum.vnum);
                if (mob)
                    return mob;
            }

            for (area = area_first; area; area = area->next) {
                mob = get_mob_index(area, wnum.vnum);
                if (mob)
                    return mob;
            }

            wnum.pArea = get_system_area_fallback();
            if (wnum.pArea)
                return get_mob_index(wnum.pArea, wnum.vnum);
            return NULL;
        }
    }
    iterator_stop(&it);
    
    return NULL;
}

/*
 * Helper function to get object index by reserved name
 */
OBJ_INDEX_DATA *get_reserved_obj_index(const char *name)
{
    RESERVED_DATA *reserved;
    AREA_DATA *reserved_area;
    OBJ_INDEX_DATA *obj;
    WNUM resolved;
    char vnum_str[32];
    
    if (!name || !*name || !reserved_vnums)
        return NULL;

    reserved = find_reserved(name);
    if (!reserved || reserved->type != RESERVED_OBJ)
        return NULL;

    if (reserved->wnum.auid <= 0 && reserved->wnum.vnum > 0) {
        snprintf(vnum_str, sizeof(vnum_str), "%ld", reserved->wnum.vnum);
        if (parse_widevnum(vnum_str, NULL, &resolved) && resolved.pArea)
            reserved->wnum.auid = resolved.pArea->uid;
    }

    reserved_area = get_area_index(reserved->wnum.auid);
    if (reserved_area) {
        obj = get_obj_index(reserved_area, reserved->wnum.vnum);
        if (obj)
            return obj;
    }

    return get_obj_index_global(reserved->wnum.vnum);
    
    return NULL;
}

/*
 * Helper function to get room index by reserved name
 */
ROOM_INDEX_DATA *get_reserved_room_index(const char *name)
{
    RESERVED_DATA *reserved;
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;
    long vnum;
    
    if (!name || !*name || !reserved_vnums)
        return NULL;

    reserved = find_reserved(name);
    if (!reserved || reserved->type != RESERVED_ROOM)
        return NULL;

    vnum = reserved->wnum.vnum;
    if (vnum <= 0)
        return NULL;

    area = get_area_index(reserved->wnum.auid);
    if (area) {
        for (room = area->room_index_hash[vnum % MAX_KEY_HASH]; room != NULL; room = room->next) {
            if (room->vnum == vnum)
                return room;
        }

        reserved->wnum.auid = 0;
    }

    for (area = area_first; area; area = area->next) {
        for (room = area->room_index_hash[vnum % MAX_KEY_HASH]; room != NULL; room = room->next) {
            if (room->vnum == vnum) {
                reserved->wnum.auid = area->uid;
                return room;
            }
        }
    }

    return NULL;
}

BLUEPRINT *get_reserved_blueprint(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;

    if (!name || !*name || !reserved_vnums)
        return NULL;

    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_BLUEPRINT &&
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea)
                wnum.pArea = get_system_area_fallback();
            wnum.vnum = reserved->wnum.vnum;
            return get_blueprint_for_area(wnum.pArea, wnum.vnum);
        }
    }
    iterator_stop(&it);

    return NULL;
}

DUNGEON_INDEX_DATA *get_reserved_dungeon_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;

    if (!name || !*name || !reserved_vnums)
        return NULL;

    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_DUNGEON &&
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea)
                wnum.pArea = get_system_area_fallback();
            wnum.vnum = reserved->wnum.vnum;
            return get_dungeon_index_for_area(wnum.pArea, wnum.vnum);
        }
    }
    iterator_stop(&it);

    return NULL;
}

SHIP_INDEX_DATA *get_reserved_ship_index(const char *name)
{
    ITERATOR it;
    RESERVED_DATA *reserved;
    WNUM wnum;

    if (!name || !*name || !reserved_vnums)
        return NULL;

    iterator_start(&it, reserved_vnums);
    while ((reserved = (RESERVED_DATA *)iterator_nextdata(&it))) {
        if (reserved->type == RESERVED_SHIP &&
            !str_cmp(name, reserved->name)) {
            iterator_stop(&it);
            wnum.pArea = get_area_index(reserved->wnum.auid);
            if (!wnum.pArea)
                wnum.pArea = get_system_area_fallback();
            wnum.vnum = reserved->wnum.vnum;
            return get_ship_index_for_area(wnum.pArea, wnum.vnum);
        }
    }
    iterator_stop(&it);

    return NULL;
}

/*
 * Helper function to get area index by id
 */
AREA_DATA *get_area_index(long uid)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        if (pArea->uid == uid)
            return pArea;
    }

    return NULL;
}

/**
 * find_area_by_vnum - Locate which area contains a specific vnum
 *
 * Resolves the area owning a bare room vnum. Prefers direct room lookup,
 * then falls back to legacy min/max range checks for compatibility.
 *
 * This function enables backward compatibility: when a user enters
 * a bare vnum like "3001" without an area prefix, the system can
 * still determine which area owns that vnum and create the proper
 * WNUM structure.
 *
 * Older areas relied on min_vnum/max_vnum boundaries, but wide-only areas
 * may leave those unset. Direct room resolution keeps both models working.
 *
 * @param vnum         The vnum to search for
 * @param current_area Reserved for future use (currently unused)
 * @return             Area containing the vnum, or NULL if not found
 */
AREA_DATA *find_area_by_vnum(long vnum, AREA_DATA *current_area)
{
    AREA_DATA *pArea;

    if (vnum <= 0)
        return NULL;

    if (current_area && get_room_index(current_area, vnum))
        return current_area;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        if (get_room_index(pArea, vnum))
            return pArea;
    }
    
    // Legacy fallback: scan all areas checking vnum ranges
    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        if (vnum >= pArea->min_vnum && vnum <= pArea->max_vnum) {
            return pArea;
        }
    }
    
    return NULL;
}

/**
 * parse_reserved_wnum_reference - Parse reserved syntax into a WNUM
 *
 * Supported forms:
 * - $name
 *
 * @param argument  Input token
 * @param wnum      Output WNUM to populate
 * @return          true if successfully resolved, false otherwise
 */
static bool parse_reserved_wnum_reference(const char *argument, WNUM *wnum)
{
    const char *name_start = NULL;
    const char *trim_start;
    const char *trim_end;
    size_t argument_len;
    size_t name_len = 0;
    char reserved_name[MSL];
    RESERVED_DATA *reserved;
    AREA_DATA *area;

    if (!argument || !wnum || argument[0] != '$')
        return false;

    argument_len = strlen(argument);
    if (argument_len < 2)
        return false;

    name_start = argument + 1;
    name_len = argument_len - 1;

    if (name_len == 0 || name_len >= sizeof(reserved_name))
        return false;

    strncpy(reserved_name, name_start, name_len);
    reserved_name[name_len] = '\0';

    trim_start = reserved_name;
    while (*trim_start && isspace((unsigned char)*trim_start))
        trim_start++;

    if (!*trim_start)
        return false;

    trim_end = trim_start + strlen(trim_start);
    while (trim_end > trim_start && isspace((unsigned char)*(trim_end - 1)))
        trim_end--;

    name_len = (size_t)(trim_end - trim_start);
    if (name_len == 0 || name_len >= sizeof(reserved_name))
        return false;

    memmove(reserved_name, trim_start, name_len);
    reserved_name[name_len] = '\0';

    reserved = find_reserved(reserved_name);
    if (!reserved || reserved->wnum.vnum < 1)
        return false;

    area = get_area_index(reserved->wnum.auid);
    if (!area)
        area = find_area_by_vnum(reserved->wnum.vnum, NULL);

    if (!area)
        return false;

    wnum->pArea = area;
    wnum->vnum = reserved->wnum.vnum;
    return true;
}

/**
 * parse_widevnum - Parse a widevnum string into a WNUM structure
 *
 * Converts user input into a WNUM structure that identifies an entity
 * by both area and vnum. Supports multiple input formats for flexibility:
 *
 * Supported formats:
 * - "#1234"           : Relative to current_area (local area context)
 * - "5#1234"          : Absolute (area UID 5, vnum 1234)
 * - "Plith#1234"      : Area name (finds area by name)
 * - "'Multi Word'#42" : Quoted area name for names with spaces
 * - "$name"           : Reserved entity name
 * - "1234"            : Bare vnum - looks up which area contains it
 *
 * Legacy support (bare vnums):
 * When no area prefix is provided, the function searches all loaded
 * areas to find which one contains the vnum. This maintains backward
 * compatibility with commands like "redit north 3001" while still
 * creating proper cross-area references.
 *
 * Error conditions:
 * - Invalid format returns false
 * - Vnum <= 0 returns false
 * - Area not found returns false
 * - Bare vnum not in any area returns false
 *
 * @param argument     String to parse (modified during parsing)
 * @param current_area Default area context (can be NULL)
 * @param wnum         Output WNUM structure to populate
 * @return             true if parsed successfully, false otherwise
 */
bool parse_widevnum(char *argument, AREA_DATA *current_area, WNUM *wnum)
{
    char *hash_pos;
    long auid = 0;
    long vnum = 0;
    
    if (!argument || !wnum) {
        return false;
    }
    
    // Clear output
    wnum->pArea = NULL;
    wnum->vnum = 0;

    // Reserved entity reference support: $name
    if (parse_reserved_wnum_reference(argument, wnum)) {
        return true;
    }
    
    // Look for hash separator
    hash_pos = strchr(argument, '#');
    
    if (hash_pos != NULL) {
        // Has hash - parse both parts
        if (hash_pos == argument) {
            // "#vnum" format - relative to current area being worked on
            if (!current_area) {
                return false;
            }
            if (!is_number(hash_pos + 1)) {
                return false;
            }
            vnum = atol(hash_pos + 1);
            wnum->pArea = current_area;
            wnum->vnum = vnum;
            return (vnum > 0);
        } else {
            // Extract left side (before #)
            size_t left_len = hash_pos - argument;
            char left_part[MSL];
            strncpy(left_part, argument, left_len);
            left_part[left_len] = '\0';
            
            // Parse right side (vnum)
            if (!is_number(hash_pos + 1)) {
                return false;
            }
            vnum = atol(hash_pos + 1);
            if (vnum <= 0) {
                return false;
            }
            
            // Check if left part is quoted area name
            if (left_part[0] == '\'' && left_part[left_len-1] == '\'') {
                // Remove quotes
                left_part[left_len-1] = '\0';
                wnum->pArea = find_area(left_part + 1);
            } else if (is_number(left_part)) {
                // Numeric area UID
                auid = atol(left_part);
                wnum->pArea = get_area_index(auid);
            } else {
                // Area name without quotes
                wnum->pArea = find_area(left_part);
            }
            
            wnum->vnum = vnum;
            return (wnum->pArea != NULL && vnum > 0);
        }
    } else {
        AREA_DATA *found_area;

        // No hash - global vnum lookup by legacy area ranges
        if (!is_number(argument)) {
            return false;
        }

        vnum = atol(argument);
        if (vnum <= 0) {
            return false;
        }

        found_area = find_area_by_vnum(vnum, NULL);
        if (!found_area) {
            return false;
        }

        wnum->pArea = found_area;
        wnum->vnum = vnum;
        return true;
    }
}

/**
 * parse_widevnum_load - Parse a widevnum string into a WNUM_LOAD structure
 *
 * Lightweight parser that extracts raw area UID and vnum without requiring
 * the area to exist in the global area list. Use this during deserialization
 * when areas may not yet be fully loaded (e.g., an area's own exits reference
 * itself by UID, but the area isn't in the global list yet).
 *
 * Supported formats:
 * - "UID#vnum"  : Extracts area UID and vnum
 * - "vnum"      : Bare vnum (area UID set to 0)
 *
 * @param str   String to parse (not modified)
 * @param wload Output WNUM_LOAD structure to populate
 * @return      true if a valid vnum was extracted, false otherwise
 */
bool parse_widevnum_load(const char *str, WNUM_LOAD *wload)
{
    const char *hash;

    if (!str || !wload) {
        return false;
    }

    wload->auid = 0;
    wload->vnum = 0;

    hash = strchr(str, '#');
    if (hash) {
        wload->auid = atol(str);
        wload->vnum = atol(hash + 1);
    } else {
        wload->vnum = atol(str);
    }

    return (wload->vnum > 0);
}

/**
 * is_widevnum_format - Check if a string is in widevnum format (auid#vnum)
 *
 * Returns true for strings like "923#1234" (digits, hash, digits).
 * Returns false for bare numbers, names, or empty strings.
 *
 * @param str  String to check
 * @return     true if string matches the widevnum format
 */
bool is_widevnum_format(const char *str)
{
    const char *p;

    if (!str || !*str)
        return false;

    p = str;

    // First part: digits (area UID)
    if (!isdigit((unsigned char)*p))
        return false;
    while (isdigit((unsigned char)*p)) p++;

    // Hash separator
    if (*p != '#')
        return false;
    p++;

    // Second part: digits (vnum)
    if (!isdigit((unsigned char)*p))
        return false;
    while (isdigit((unsigned char)*p)) p++;

    // Must be end of string
    return (*p == '\0');
}

/**
 * wnum_match - Compare a WNUM against an area/vnum pair
 *
 * Checks whether the given WNUM matches the specified area and vnum.
 * Both area pointer and vnum must match for a positive result.
 *
 * @param wnum  The WNUM to compare against
 * @param area  The area to match
 * @param vnum  The vnum to match
 * @return      true if both area and vnum match
 */
bool wnum_match(WNUM wnum, AREA_DATA *area, long vnum)
{
    if (!area || vnum < 1) return false;

    return wnum.pArea == area && wnum.vnum == vnum;
}

/**
 * wnum_match_room - Compare a WNUM against a room
 *
 * Checks whether the given WNUM matches the specified room's area and vnum.
 * Clone rooms (rooms with a non-NULL source) will never match - compare
 * against the source template room instead.
 *
 * @param wnum  The WNUM to compare against
 * @param room  The room to match
 * @return      true if the WNUM matches the room
 */
bool wnum_match_room(WNUM wnum, ROOM_INDEX_DATA *room)
{
    if (!room) return false;
    if (!room->area || room->vnum < 1) return false;
    if (room->source) return false; // Clone rooms will not match, use room->source

    return wnum.pArea == room->area && wnum.vnum == room->vnum;
}

/**
 * wnum_match_obj - Compare a WNUM against an object instance
 *
 * Checks whether the given WNUM matches the object's index data
 * (area and vnum). Validates the object and its index data before comparing.
 *
 * @param wnum  The WNUM to compare against
 * @param obj   The object instance to match
 * @return      true if the WNUM matches the object's index
 */
bool wnum_match_obj(WNUM wnum, OBJ_DATA *obj)
{
    if (!IS_VALID(obj)) return false;
    if (!obj->pIndexData) return false;
    if (!obj->pIndexData->area || obj->pIndexData->vnum < 1) return false;

    return obj->pIndexData->area == wnum.pArea && obj->pIndexData->vnum == wnum.vnum;
}

/**
 * wnum_match_mob - Compare a WNUM against a mobile instance
 *
 * Checks whether the given WNUM matches the mobile's index data
 * (area and vnum). Only matches NPCs - returns false for players.
 *
 * @param wnum  The WNUM to compare against
 * @param ch    The character to match (must be NPC)
 * @return      true if the WNUM matches the mobile's index
 */
bool wnum_match_mob(WNUM wnum, CHAR_DATA *ch)
{
    if (!IS_VALID(ch) || !IS_NPC(ch)) return false;
    if (!ch->pIndexData) return false;
    if (!ch->pIndexData->area || ch->pIndexData->vnum < 1) return false;

    return ch->pIndexData->area == wnum.pArea && ch->pIndexData->vnum == wnum.vnum;
}

/**
 * wnum_match_token - Compare a WNUM against a token instance
 *
 * Checks whether the given WNUM matches the token's index data
 * (area and vnum).
 *
 * @param wnum   The WNUM to compare against
 * @param token  The token instance to match
 * @return       true if the WNUM matches the token's index
 */
bool wnum_match_token(WNUM wnum, TOKEN_DATA *token)
{
    if (!IS_VALID(token)) return false;
    if (!token->pIndexData) return false;
    if (!token->pIndexData->area || token->pIndexData->vnum < 1) return false;

    return token->pIndexData->area == wnum.pArea && token->pIndexData->vnum == wnum.vnum;
}

/**
 * get_room_wnum - Extract a WNUM from a room
 *
 * Populates the given WNUM with the room's area and vnum.
 * If room is NULL, zeros out the WNUM.
 *
 * @param room  The room to extract from (can be NULL)
 * @param wnum  Output WNUM to populate
 */
void get_room_wnum(ROOM_INDEX_DATA *room, WNUM *wnum)
{
    if (wnum) {
        if (room) {
            wnum->pArea = room->area;
            wnum->vnum = room->vnum;
        } else {
            wnum->pArea = NULL;
            wnum->vnum = 0;
        }
    }
}

AREA_REGION *get_room_region(ROOM_INDEX_DATA *room)
{
    if (!room) return NULL;

    if (room->source) return NULL;
    if (!room->area) return NULL;

    if (!room->region) return &room->area->region;

    return room->region;
}

void area_region_add_room(AREA_REGION *region, ROOM_INDEX_DATA *room)
{
    if (!IS_VALID(region) || !room || !room->area)
        return;

    if (room->region == region)
        return;

    if (IS_VALID(room->region) && room->region->rooms)
        list_remlink(room->region->rooms, room, false);

    room->region = region;

    if (region->rooms)
        list_appendlink(region->rooms, room);
}

void area_region_remove_room(ROOM_INDEX_DATA *room)
{
    if (!room)
        return;

    if (IS_VALID(room->region) && room->region->rooms)
        list_remlink(room->region->rooms, room, false);

    room->region = NULL;
}

AREA_REGION *get_area_region_by_uid(AREA_DATA *area, long uid)
{
    if (!area) return NULL;

    if (!uid) return &area->region;
    if (!area->regions) return NULL;

    ITERATOR it;
    AREA_REGION *region;

    iterator_start(&it, area->regions);
    while((region = (AREA_REGION *)iterator_nextdata(&it)))
    {
        if (region->uid == uid)
            break;
    }
    iterator_stop(&it);

    return region;
}

/**
 * get_mob_wnum - Extract a WNUM from a mobile index
 *
 * Populates the given WNUM with the mobile's area and vnum.
 * If mob is NULL, zeros out the WNUM.
 *
 * @param mob   The mobile index to extract from (can be NULL)
 * @param wnum  Output WNUM to populate
 */
void get_mob_wnum(MOB_INDEX_DATA *mob, WNUM *wnum)
{
    if (wnum) {
        if (mob) {
            wnum->pArea = mob->area;
            wnum->vnum = mob->vnum;
        } else {
            wnum->pArea = NULL;
            wnum->vnum = 0;
        }
    }
}

/**
 * get_obj_wnum - Extract a WNUM from an object index
 *
 * Populates the given WNUM with the object's area and vnum.
 * If obj is NULL, zeros out the WNUM.
 *
 * @param obj   The object index to extract from (can be NULL)
 * @param wnum  Output WNUM to populate
 */
void get_obj_wnum(OBJ_INDEX_DATA *obj, WNUM *wnum)
{
    if (wnum) {
        if (obj) {
            wnum->pArea = obj->area;
            wnum->vnum = obj->vnum;
        } else {
            wnum->pArea = NULL;
            wnum->vnum = 0;
        }
    }
}

/**
 * get_token_wnum - Extract a WNUM from a token index
 *
 * Populates the given WNUM with the token's area and vnum.
 * If token is NULL, zeros out the WNUM.
 *
 * @param token The token index to extract from (can be NULL)
 * @param wnum  Output WNUM to populate
 */
void get_token_wnum(TOKEN_INDEX_DATA *token, WNUM *wnum)
{
    if (wnum) {
        if (token) {
            wnum->pArea = token->area;
            wnum->vnum = token->vnum;
        } else {
            wnum->pArea = NULL;
            wnum->vnum = 0;
        }
    }
}

/**
 * widevnum_string - Convert WNUM to string format for display/saving
 *
 * Formats a WNUM as a string suitable for display or serialization.
 * Uses a rotating buffer system to allow multiple calls within a
 * single statement (e.g., in printf).
 *
 * Output format:
 * - Relative format "#vnum" if pArea matches pRefArea
 * - Absolute format "auid#vnum" otherwise
 * - Special case "0#vnum" if pArea is NULL
 *
 * Buffer rotation:
 * Uses 4 rotating buffers so you can call this function up to 4 times
 * in a single statement. This is common in display code like:
 * sprintf(buf, "North: %s  South: %s  East: %s  West: %s",
 *         widevnum_string(...), widevnum_string(...), ...)
 *
 * @param pArea    Area containing the entity
 * @param vnum     Vnum within the area
 * @param pRefArea Reference area for relative format (can be NULL)
 * @return         Static buffer containing formatted string
 */
const char *widevnum_string(AREA_DATA *pArea, long vnum, AREA_DATA *pRefArea)
{
    static int i = 0;
    static char output[4][MSL];
    
    i = (i + 1) & 3;  // Rotate through 4 buffers
    
    if (!pArea) {
        sprintf(output[i], "0#%ld", vnum);
    } else if (pArea == pRefArea) {
        sprintf(output[i], "#%ld", vnum);
    } else {
        sprintf(output[i], "%ld#%ld", pArea->uid, vnum);
    }
    
    return output[i];
}

const char *widevnum_string_wnum(WNUM wnum, AREA_DATA *pRefArea)
{
    return widevnum_string(wnum.pArea, wnum.vnum, pRefArea);
}

const char *widevnum_string_mobile(MOB_INDEX_DATA *mob, AREA_DATA *pRefArea)
{
    if (mob && mob->area)
        return widevnum_string(mob->area, mob->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_object(OBJ_INDEX_DATA *obj, AREA_DATA *pRefArea)
{
    if (obj && obj->area)
        return widevnum_string(obj->area, obj->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_room(ROOM_INDEX_DATA *room, AREA_DATA *pRefArea)
{
    if (room && room->area)
        return widevnum_string(room->area, room->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_token(TOKEN_INDEX_DATA *token, AREA_DATA *pRefArea)
{
    if (token && token->area)
        return widevnum_string(token->area, token->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_blueprint(BLUEPRINT *bp, AREA_DATA *pRefArea)
{
    if (bp && bp->area)
        return widevnum_string(bp->area, bp->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_blueprint_section(BLUEPRINT_SECTION *bs, AREA_DATA *pRefArea)
{
    if (bs && bs->area)
        return widevnum_string(bs->area, bs->vnum, pRefArea);
    return "0#0";
}
const char *widevnum_string_dungeon(DUNGEON_INDEX_DATA *dng, AREA_DATA *pRefArea)
{
    if (dng && dng->area)
        return widevnum_string(dng->area, dng->vnum, pRefArea);
    return "0#0";
}
const char *widevnum_string_ship(SHIP_INDEX_DATA *ship, AREA_DATA *pRefArea)
{
    if (ship && ship->area)
        return widevnum_string(ship->area, ship->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_event(EVENT_INDEX_DATA *event, AREA_DATA *pRefArea)
{
    if (event && event->area)
        return widevnum_string(event->area, event->vnum, pRefArea);
    return "0#0";
}

const char *widevnum_string_script(SCRIPT_DATA *script, AREA_DATA *pRefArea)
{
    if (script && script->area)
        return widevnum_string(script->area, script->vnum, pRefArea);
    return "0#0";
}

// Default pronoun sets based on body_type
const struct body_type_info_type body_type_info[BODY_TYPE_MAX] =
{
    { "neutral", "it",   "it",   "its",    "its",    "itself", VERB_FORM_SINGULAR },
    { "masculine",    "he",   "him",  "his",    "his",    "himself", VERB_FORM_SINGULAR },
    { "feminine",  "she",  "her",  "her",    "hers",   "herself", VERB_FORM_SINGULAR },
    { "other",   "they", "them", "their",  "theirs", "themself", VERB_FORM_PLURAL }
};

const char *get_he_she(CHAR_DATA *ch) {
    if (ch->pronoun_he_she && ch->pronoun_he_she[0] != '\0') {
        return ch->pronoun_he_she;
    }
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].default_he_she;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].default_he_she;
}

const char *get_him_her(CHAR_DATA *ch) {
    if (ch->pronoun_him_her && ch->pronoun_him_her[0] != '\0') {
        return ch->pronoun_him_her;
    }
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].default_him_her;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].default_him_her;
}

const char *get_his_her(CHAR_DATA *ch) { // Possessive Adjective
    if (ch->pronoun_his_her && ch->pronoun_his_her[0] != '\0') {
        return ch->pronoun_his_her;
    }
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].default_his_her;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].default_his_her;
}

const char *get_his_hers(CHAR_DATA *ch) { // Possessive Pronoun
    if (ch->pronoun_his_hers && ch->pronoun_his_hers[0] != '\0') {
        return ch->pronoun_his_hers;
    }
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].default_his_hers;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].default_his_hers;
}

const char *get_himself_herself(CHAR_DATA *ch) {
    if (ch->pronoun_himself_herself && ch->pronoun_himself_herself[0] != '\0') {
        return ch->pronoun_himself_herself;
    }
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].default_himself_herself;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].default_himself_herself;
}

const char *get_body_type_name(CHAR_DATA *ch) {
    if (ch->body_type >= 0 && ch->body_type < BODY_TYPE_MAX) {
        return body_type_info[ch->body_type].name;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].name;
}

const char *get_verb_form(CHAR_DATA *character, const char *singular, const char *plural) {
    if (!character) {
        return singular;
    }

    verb_form_preference_t preference = character->verb_preference;

    if (preference == VERB_FORM_DEFAULT) {
        if (character->body_type >= 0 && character->body_type < BODY_TYPE_MAX) {
            preference = body_type_info[character->body_type].verb_preference;
        } else {
            preference = body_type_info[BODY_TYPE_NEUTRAL].verb_preference;
        }
    }
    
    if (preference == VERB_FORM_DEFAULT) {
        const char *subj_pronoun = get_he_she(character);
        if (subj_pronoun && (str_cmp(subj_pronoun, "they") == 0 || str_cmp(subj_pronoun, "They") == 0)) {
            return plural;
        }
        return singular;
    }

    switch (preference) {
        case VERB_FORM_SINGULAR:
            return singular;
        case VERB_FORM_PLURAL:
            return plural;
        default: 
            return singular; 
    }
}
void display_pronoun_examples(CHAR_DATA *ch_viewer, const char *subj, const char *obj, const char *poss_adj, const char *poss_pron, const char *refl, verb_form_preference_t vpref) {
    char buf[MSL*2]; // Increased buffer size
    char temp_subj_cap[MIL];

    if (!ch_viewer) return;
    if (!subj || !obj || !poss_adj || !poss_pron || !refl) {
        send_to_char("Error: One or more pronoun components are missing for example display.\n\r", ch_viewer);
        return;
    }

    strncpy(temp_subj_cap, subj, MIL-1);
    temp_subj_cap[MIL-1] = '\0';
    temp_subj_cap[0] = UPPER(temp_subj_cap[0]);


    // Determine verb forms (simplified for generic display)
    const char *verb_s_walk = "walks";
    const char *verb_p_walk = "walk";
    const char *chosen_walk;

    const char *verb_s_see = "sees";
    const char *verb_p_see = "see";
    const char *chosen_see;

    // Logic for choosing verb form based on vpref and subjective pronoun "they"
    bool use_plural_default = (!str_cmp(subj, "they") || !str_cmp(subj, "They"));

    if (vpref == VERB_FORM_PLURAL) {
        chosen_walk = verb_p_walk;
        chosen_see = verb_p_see;
    } else if (vpref == VERB_FORM_SINGULAR) {
        chosen_walk = verb_s_walk;
        chosen_see = verb_s_see;
    } else { // VERB_FORM_DEFAULT
        chosen_walk = use_plural_default ? verb_p_walk : verb_s_walk;
        chosen_see = use_plural_default ? verb_p_see : verb_s_see;
    }
    send_to_char("{WExample Sentences:{x\n\r", ch_viewer);
    sprintf(buf, "  Subjective:       {C%s{x %s to the east.\n\r", temp_subj_cap, chosen_walk); send_to_char(buf, ch_viewer);
    sprintf(buf, "  Objective:        You see {C%s{x.\n\r", obj); send_to_char(buf, ch_viewer);
    sprintf(buf, "  Possessive Adj:   This is {C%s{x sword.\n\r", poss_adj); send_to_char(buf, ch_viewer);
    sprintf(buf, "  Possessive Pron:  The sword is {C%s{x.\n\r", poss_pron); send_to_char(buf, ch_viewer);
    sprintf(buf, "  Reflexive:        {C%s{x %s {C%s{x in the mirror.\n\r", temp_subj_cap, chosen_see, refl); send_to_char(buf, ch_viewer);
}

const char *get_body_type_name_from_val(body_type_t btype) {
    if (btype >= 0 && btype < BODY_TYPE_MAX) {
        return body_type_info[btype].name;
    }
    return body_type_info[BODY_TYPE_NEUTRAL].name; // Fallback
}

void reset_pronouns_to_body_type(CHAR_DATA *ch, body_type_t new_body_type)
{
    if (!ch || new_body_type < 0 || new_body_type >= BODY_TYPE_MAX) {
        return; // Invalid character or body type
    }

    // Reset pronouns to the default for the new body type
    ch->pronoun_he_she = str_dup(body_type_info[new_body_type].default_he_she);
    ch->pronoun_him_her = str_dup(body_type_info[new_body_type].default_him_her);
    ch->pronoun_his_her = str_dup(body_type_info[new_body_type].default_his_her);
    ch->pronoun_his_hers = str_dup(body_type_info[new_body_type].default_his_hers);
    ch->pronoun_himself_herself = str_dup(body_type_info[new_body_type].default_himself_herself);
    
    // Set verb preference based on body type
    ch->verb_preference = body_type_info[new_body_type].verb_preference;
}

/**
 * char_set_race - Change a character's race with full property recalculation
 *
 * Central function for all race changes (script altermob, remort, polymorph).
 * Recalculates permanent affects, immunities, resistances, vulnerabilities,
 * form, parts, size, racial skills, and stat caps. Calls affect_fix_char()
 * to rebuild active flags from the new perm baseline.
 *
 * When RACE_CHANGE_OVERLAY is used with an orace, the new race's properties
 * are merged with the original race (affects are OR'd, res/vuln/form/parts
 * use new race, skills are unioned, stat caps use per-stat max).
 *
 * @param ch        Character to modify
 * @param new_race  Target RACE_DATA (NULL to revert to orace)
 * @param flags     RACE_CHANGE_* flags controlling behavior
 */
void char_set_race(CHAR_DATA *ch, RACE_DATA *new_race, long flags)
{
    RACE_DATA *old_race;

    if (!ch)
        return;

    /* Handle revert: restore orace as current race */
    if (IS_SET(flags, RACE_CHANGE_REVERT)) {
        if (!ch->orace)
            return;

        new_race = ch->orace;
        ch->orace = NULL;
        REMOVE_BIT(flags, RACE_CHANGE_SAVE_ORIGINAL);
        REMOVE_BIT(flags, RACE_CHANGE_OVERLAY);
    }

    if (!new_race)
        return;

    old_race = ch->race;

    /* Save original race before changing (only if not already saved) */
    if (IS_SET(flags, RACE_CHANGE_SAVE_ORIGINAL) && !ch->orace)
        ch->orace = old_race;

    /* Swap the race pointer */
    ch->race = new_race;

    /* --- Recalculate permanent baseline flags --- */

    if (IS_SET(flags, RACE_CHANGE_OVERLAY) && ch->orace) {
        /* Overlay mode: merge original + new race properties */
        RACE_DATA *orig = ch->orace;

        /* Affects: OR both races */
        ch->affected_by_perm[0] = orig->aff[0] | new_race->aff[0];
        ch->affected_by_perm[1] = orig->aff[1] | new_race->aff[1];

        /* Immunities: OR both races */
        ch->imm_flags_perm = orig->imm | new_race->imm;

        /* Resistances/vulnerabilities: new race wins */
        ch->res_flags_perm  = new_race->res;
        ch->vuln_flags_perm = new_race->vuln;

        /* Physical: new race wins */
        ch->form  = new_race->form;
        ch->parts = new_race->parts & ~ch->lostparts;
        ch->size  = new_race->min_size;
    } else {
        /* Full replacement */
        ch->affected_by_perm[0] = new_race->aff[0];
        ch->affected_by_perm[1] = new_race->aff[1];
        ch->imm_flags_perm  = new_race->imm;
        ch->res_flags_perm  = new_race->res;
        ch->vuln_flags_perm = new_race->vuln;

        ch->form  = new_race->form;
        ch->parts = new_race->parts & ~ch->lostparts;
        ch->size  = new_race->min_size;
    }

    /* --- Racial skills --- */
    if (!IS_SET(flags, RACE_CHANGE_KEEP_SKILLS) && !IS_NPC(ch)) {
        /* Add new racial skills */
        if (new_race->skills) {
            ITERATOR it;
            char *skill_name;
            iterator_start(&it, new_race->skills);
            while ((skill_name = (char *)iterator_nextdata(&it)))
                group_add(ch, skill_name, false);
            iterator_stop(&it);
        }

        /* In overlay mode, also ensure original race skills are present */
        if (IS_SET(flags, RACE_CHANGE_OVERLAY) && ch->orace && ch->orace->skills) {
            ITERATOR it;
            char *skill_name;
            iterator_start(&it, ch->orace->skills);
            while ((skill_name = (char *)iterator_nextdata(&it)))
                group_add(ch, skill_name, false);
            iterator_stop(&it);
        }
    }

    /* --- Stat caps --- */
    if (!IS_SET(flags, RACE_CHANGE_KEEP_STATS) && !IS_NPC(ch)) {
        for (int i = 0; i < MAX_STATS; i++) {
            int cap = new_race->max_stats[i];

            /* In overlay mode, use the higher cap of both races */
            if (IS_SET(flags, RACE_CHANGE_OVERLAY) && ch->orace
                && ch->orace->max_stats[i] > cap)
                cap = ch->orace->max_stats[i];

            if (ch->perm_stat[i] > cap)
                set_perm_stat(ch, i, cap);
        }
    }

    /* --- Rebuild active flags from new perm baseline --- */
    affect_fix_char(ch);

    if (!IS_SET(flags, RACE_CHANGE_SILENT) && !IS_NPC(ch)) {
        printf_to_char(ch, "Your race has changed to %s.\n\r",
                       new_race->name ? new_race->name : "unknown");
    }
}