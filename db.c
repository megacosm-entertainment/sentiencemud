/***************************************************************************
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
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include "log.h"
#include "strings.h"
#include "merc.h"
#include "db.h"
#include "math.h"
#include "io/json/json_instance.h"
#include "recycle.h"
#include "tables.h"
#include "olc_save.h"
#include "scripts.h"
#include "wilds.h"
#include "io/json/json_persist.h"
#include "io/json/json_area.h"
#include "io/json/json_obj_types.h"
#include "io/cache/redis_cache.h"
#include "traits.h"
#include "skill_data.h"
#include "class_data.h"
#include "skill_group.h"
#include "song_data.h"
#include "item_types.h"
#include "channel_registry.h"
#include "io/json/json_localization.h"

static void emit_db_wiz_event(const char *plain_message,
                              const char *staff_message,
                              long wiz_flag,
                              const char *action,
                              const char *category)
{
    log_context_t ctx = {
        .actor_type = "system",
        .actor_name = "db",
        .action = action,
    };

    log_event_t ev = {
        .severity = EVENT_SEV_INFO,
        .category = category ? category : LOG_INFO,
        .plain_message = plain_message ? plain_message : "db event",
        .staff_message = staff_message,
        .wiznet_flag = wiz_flag,
        .context = &ctx,
        .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
    };

    log_emit_event(&ev, NULL);
}



/*
#if !defined(OLD_RAND)
#if !defined(linux)
long random();
#endif
void srandom(unsigned int);
int getpid();
time_t time(time_t *tloc);
#endif
*/


#define BOOT_ERROR_MAX (1024 * 1024) // 1MB buffer for boot errors
static char boot_error_buf[BOOT_ERROR_MAX];
static size_t boot_error_len = 0;

static inline int legacy_obj_index_value_get(const OBJ_INDEX_DATA *obj, int slot)
{
    if (!obj || slot < 0 || slot > 7)
        return 0;
    return obj->value[slot];
}

static char *reset_room_expand_field(pVARIABLE vars, const char *src, ROOM_INDEX_DATA *pRoom, const char *field)
{
    char *expanded;

    if (!src)
        return NULL;

    expanded = variables_expand_text_dup(vars, src);

    if (strstr(src, "$<") && strstr(expanded, "$<"))
    {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "reset_room: unresolved variable placeholder in room %ld field '%s'",
            pRoom ? pRoom->vnum : 0,
            field ? field : "unknown");
    }

    return expanded;
}

static void reset_room_expand_text(ROOM_INDEX_DATA *pRoom)
{
    EXTRA_DESCR_DATA *ed;
    CONDITIONAL_DESCR_DATA *cd;
    int door;

    if (!pRoom || !pRoom->progs)
        return;

    {
        const char *name_template = pRoom->source ? pRoom->source->name : pRoom->name;
        const char *desc_template = pRoom->source ? pRoom->source->description : pRoom->description;

        if (name_template)
        {
            char *expanded_name = reset_room_expand_field(pRoom->progs->vars, name_template, pRoom, "name");
            free_string(pRoom->name);
            pRoom->name = expanded_name;
        }

        if (desc_template)
        {
            char *expanded_desc = reset_room_expand_field(pRoom->progs->vars, desc_template, pRoom, "description");
            free_string(pRoom->description);
            pRoom->description = expanded_desc;
        }
    }

    for (ed = pRoom->extra_descr; ed; ed = ed->next)
    {
        if (ed->keyword)
        {
            char *expanded_keyword = reset_room_expand_field(pRoom->progs->vars, ed->keyword, pRoom, "extra.keyword");
            free_string(ed->keyword);
            ed->keyword = expanded_keyword;
        }

        if (ed->description)
        {
            char *expanded_desc = reset_room_expand_field(pRoom->progs->vars, ed->description, pRoom, "extra.description");
            free_string(ed->description);
            ed->description = expanded_desc;
        }
    }

    for (cd = pRoom->conditional_descr; cd; cd = cd->next)
    {
        if (cd->description)
        {
            char *expanded_cond_desc = reset_room_expand_field(pRoom->progs->vars, cd->description, pRoom, "conditional.description");
            free_string(cd->description);
            cd->description = expanded_cond_desc;
        }
    }

    for (door = 0; door < MAX_DIR; door++)
    {
        EXIT_DATA *ex = pRoom->exit[door];

        if (!ex)
            continue;

        if (ex->keyword)
        {
            char *expanded_keyword = reset_room_expand_field(pRoom->progs->vars, ex->keyword, pRoom, "exit.keyword");
            free_string(ex->keyword);
            ex->keyword = expanded_keyword;
        }

        if (ex->short_desc)
        {
            char *expanded_short = reset_room_expand_field(pRoom->progs->vars, ex->short_desc, pRoom, "exit.short_desc");
            free_string(ex->short_desc);
            ex->short_desc = expanded_short;
        }

        if (ex->long_desc)
        {
            char *expanded_long = reset_room_expand_field(pRoom->progs->vars, ex->long_desc, pRoom, "exit.long_desc");
            free_string(ex->long_desc);
            ex->long_desc = expanded_long;
        }

        if (ex->door.material)
        {
            char *expanded_material = reset_room_expand_field(pRoom->progs->vars, ex->door.material, pRoom, "exit.material");
            free_string(ex->door.material);
            ex->door.material = expanded_material;
        }
    }
}

static inline void legacy_obj_index_value_set(OBJ_INDEX_DATA *obj, int slot, int value)
{
    if (!obj || slot < 0 || slot > 7)
        return;
    obj->value[slot] = value;
}

// Central boot error logging function
void boot_error_log(const char *fmt, ...)
{
    va_list args;
    char tmp[1024];
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    size_t tmp_len = strlen(tmp);
    if (boot_error_len + tmp_len + 2 < BOOT_ERROR_MAX) {
        strcpy(boot_error_buf + boot_error_len, tmp);
        boot_error_len += tmp_len;
        boot_error_buf[boot_error_len++] = '\n';
        boot_error_buf[boot_error_len] = '\0';
    }
}

// VERSION_ROOM_002 special defines
#define VR_002_EX_LOCKED		(C)
#define VR_002_EX_PICKPROOF		(F)
#define VR_002_EX_EASY			(H)
#define VR_002_EX_HARD			(I)
#define VR_002_EX_INFURIATING	(J)


/* externals for counting purposes */
extern  OBJ_DATA  *obj_free;
extern  CHAR_DATA *char_free;
extern  DESCRIPTOR_DATA *descriptor_free;
extern  PC_DATA   *pcdata_free;
extern  AFFECT_DATA *affect_free;
extern  CHAT_ROOM_DATA  *chat_room_free;
extern  ROOM_INDEX_DATA *room_index_free;
extern  EXIT_DATA       *exit_free;
extern  CHAT_OP_DATA  *chat_op_free;
extern  CHAT_BAN_DATA   *chat_ban_free;
extern  AREA_DATA       *area_free;
extern  MOB_INDEX_DATA  *mob_index_free;
extern  OBJ_INDEX_DATA  *obj_index_free;
extern  EXTRA_DESCR_DATA * extra_descr_free;
extern  RESET_DATA  *reset_free;
extern	GLOBAL_DATA gconfig;
extern	LLIST *loaded_instances;
extern	LLIST *loaded_dungeons;
extern	LLIST *loaded_ships;
LLIST *loaded_special_keys;
LLIST *commands_list = NULL;

void free_room_index( ROOM_INDEX_DATA *pRoom );
void load_instances();
LLIST *pending_changes = NULL;

/* Reading of keys*/
#if defined(KEY)
#undef KEY
#endif

#define IS_KEY(literal)		(!str_cmp(word,literal))

#define KEY(literal, field, value) \
    if (IS_KEY(literal)) { \
        field = value; \
        fMatch = true; \
        break; \
    }

#define SKEY(literal, field) \
    if (IS_KEY(literal)) { \
        free_string(field); \
        field = fread_string(fp); \
        fMatch = true; \
        break; \
    }

#define FKEY(literal, field) \
    if (IS_KEY(literal)) { \
        field = true; \
        fMatch = true; \
        break; \
    }

#define FVKEY(literal, field, string, tbl) \
    if (IS_KEY(literal)) { \
        field = script_flag_value(tbl, string); \
        fMatch = true; \
        break; \
    }

#define FVDKEY(literal, field, string, tbl, bad, def) \
    if (!str_cmp(word, literal)) { \
        field = script_flag_value(tbl, string); \
        if( field == bad ) { \
            field = def; \
        } \
        fMatch = true; \
        break; \
    }

/* externals for counting purposes */
extern	OBJ_DATA	*obj_free;
extern	PC_DATA		*pcdata_free;
extern	RESET_DATA	*reset_free;
extern  AFFECT_DATA	*affect_free;
extern  AREA_DATA       *area_free;
extern  CHAT_BAN_DATA   *chat_ban_free;
extern  CHAT_OP_DATA	*chat_op_free;
extern  CHAT_ROOM_DATA  *chat_room_free;
extern  DESCRIPTOR_DATA *descriptor_free;
extern  EXIT_DATA       *exit_free;
extern  EXTRA_DESCR_DATA * extra_descr_free;
extern  MOB_INDEX_DATA	*mob_index_free;
extern  OBJ_INDEX_DATA	*obj_index_free;
extern  ROOM_INDEX_DATA *room_index_free;

int disconnect_timeout = 30;
int limbo_timeout = 12;

/*
 * Globals.
 */

LLIST *gc_mobiles;
LLIST *gc_objects;
LLIST *gc_rooms;
LLIST *gc_tokens;
AREA_DATA *		eden_area;
AREA_DATA *		netherworld_area;
AREA_DATA *		wilderness_area;
AUCTION_DATA            auction_info;
BOUNTY_DATA * 		bounty_list;
CHAT_ROOM_DATA *	chat_room_list;
CHURCH_DATA *		church_first;
CHURCH_DATA * 		church_list;
LLIST *list_churches;
GQ_DATA			global_quest;
HELP_CATEGORY *		topHelpCat;
HELP_DATA *		help_first;
HELP_DATA *		help_last;
MAIL_DATA *		mail_list;
NOTE_DATA *		note_free;
NOTE_DATA *		note_list;
NPC_SHIP_DATA *		plith_airship;
SCRIPT_DATA *mprog_list;
SCRIPT_DATA *oprog_list;
SCRIPT_DATA *rprog_list;
SCRIPT_DATA *tprog_list;
SCRIPT_DATA *aprog_list;
SCRIPT_DATA *iprog_list;
SCRIPT_DATA *dprog_list;
SCRIPT_DATA *qprog_list;
SCRIPT_DATA *eprog_list;
PROJECT_DATA *		project_list;
bool			projects_changed;
SHOP_DATA *		shop_first;
SHOP_DATA *		shop_last;
TIME_INFO_DATA		time_info;
TRADE_ITEM *	        trade_produce_list;
WEATHER_DATA 		weather_info;
BLUEPRINT_SECTION		*blueprint_section_hash[MAX_KEY_HASH];
BLUEPRINT				*blueprint_hash[MAX_KEY_HASH];
DUNGEON_INDEX_DATA		*dungeon_index_hash[MAX_KEY_HASH];
SHIP_INDEX_DATA			*ship_index_hash[MAX_KEY_HASH];
REPUTATION_INDEX_DATA  *reputation_index_hash[MAX_KEY_HASH];
QUEST_INDEX_V2_DATA    *quest_index_v2_list;
QUEST_INDEX_DATA       *quest_index_list;

bool			global;
char			bug_buf[2*MAX_INPUT_LENGTH];
char *			help_greeting;
char			log_buf[2*MAX_INPUT_LENGTH];
char			*reboot_by;
char 			*reboot_reason;
bool			reboot_shutdown;
int				down_timer;
int				pre_reckoning;
int				reckoning_duration = 30;
int				reckoning_intensity = 100;
int				reckoning_cooldown = 0;
int				reckoning_chance = 5;
time_t			reboot_timer;
time_t			reckoning_timer;
time_t			reckoning_cooldown_timer;
PROG_DATA *		prog_data_virtual;
char *			room_name_virtual;
bool			objRepop;
long gc_total_processed = 0;
long gc_calls = 0;
long gc_max_time = 0;
/* This variable serves as a placeholder to make sure that obj repop scripts
   are only triggered by newly created objects instead of any objects. This
   is necesarry because I put the triggering mechanism in obj_to_char() and
   obj_to_room(). When the object is given to a char or a room, this variable
   is toggled off, and the object will then no longer trigger repop scripts. */

/*
 * Locals.
 */
WNUM wnum_zero;  // Zero-initialized WNUM constant for comparisons/initialization

AREA_DATA *area_first;
AREA_DATA *area_last;
AREA_DATA *current_area;
CHAR_DATA *hunt_last;
MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];
TOKEN_INDEX_DATA *token_index_hash[MAX_KEY_HASH];
char str_empty[1];
char *string_hash[MAX_KEY_HASH];
char *string_space;
char *top_string;
long top_affect;
long top_affliction;
long top_area;
long top_chat_ban;
long top_chat_op;
long top_chatroom;
long top_ed;
long top_exit;
long top_extra_descr;
long top_help;
long top_mob_index;
long top_mprog_index;
long top_npc_ship;
long top_obj_index;
long top_reset;
long top_room;
long top_shop;
long top_quest;
long top_quest_part;
long top_oprog_index;
long top_rprog_index;
long top_vnum_mob;
long top_vnum_npc_ship;
long top_vnum_obj;
long top_vnum_room;
long top_wilderness_exit;
long top_help_index = 0;
long mobile_count = 0;
long newmobs = 0;
long newobjs = 0;
long top_auction;
long top_church;
long top_church_player;
long top_descriptor;
long top_ship;
long top_ship_crew;
long top_vroom;
long top_waypoint;

long top_aprog_index;
long top_iprog_index;
long top_dprog_index;
long top_qprog_index;

LLIST *loaded_chars;
// Temporarily disabled for reconnect crash.
//LLIST *loaded_players;
LLIST *loaded_objects;
LLIST *loaded_groups;
LLIST *persist_mobs;
LOADED_OBJ_HASH_ENTRY *loaded_obj_hash[LOADED_OBJ_HASH_SIZE];
LLIST *persist_objs;
LLIST *persist_rooms;
LLIST *loaded_accounts;
LLIST *reserved_vnums;

static void resolve_newbie_tables(void)
{
    struct {
        int class_index;
        const char *reserved_name;
    } class_weapons[] = {
        { CLASS_MAGE,    "OBJ_VNUM_NEWB_QUARTERSTAFF" },
        { CLASS_CLERIC,  "OBJ_VNUM_NEWB_QUARTERSTAFF" },
        { CLASS_THIEF,   "OBJ_VNUM_NEWB_DAGGER" },
        { CLASS_WARRIOR, "OBJ_VNUM_NEWB_SWORD" },
        { -1, NULL }
    };
    const char *newbie_eq_names[] = {
        "OBJ_VNUM_NEWB_ARMOUR",
        "OBJ_VNUM_NEWB_CLOAK",
        "OBJ_VNUM_NEWB_LEGGINGS",
        "OBJ_VNUM_NEWB_BOOTS",
        "OBJ_VNUM_NEWB_HELM",
        NULL
    };
    int i;

    for (i = 0; class_weapons[i].reserved_name != NULL; i++) {
        OBJ_INDEX_DATA *obj = get_reserved_obj_index(class_weapons[i].reserved_name);
        if (obj) {
            /* Update CLASS_DATA entries that match this base class type */
            CLASS_DATA *clz;
            for (clz = class_first(); clz; clz = clz->next) {
                if (clz->type == class_weapons[i].class_index && clz->weapon == 0)
                    clz->weapon = obj->vnum;
            }
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                "resolve_newbie_tables: reserved %s not found.",
                class_weapons[i].reserved_name);
        }
    }

    for (i = 0; newbie_eq_names[i] != NULL; i++) {
        OBJ_INDEX_DATA *obj = get_reserved_obj_index(newbie_eq_names[i]);
        if (obj) {
            newbie_eq_table[i].vnum = obj->vnum;
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                "resolve_newbie_tables: reserved %s not found.",
                newbie_eq_names[i]);
        }
    }
}


TOKEN_DATA *global_tokens = NULL;



/*
 * Memory management.
 */
#define			MAX_STRING	10000000
#define			MAX_PERM_BLOCK  5000000
#define			MAX_MEM_LIST	11

void *			rgFreeList	[MAX_MEM_LIST];
const int		rgSizeList	[MAX_MEM_LIST]	=
{
    16, 32, 64, 128, 256, 1024, 2048, 4096, 8192, 16384, 32768-64
};

long			numStrings = 0;
long			stringSpace = 0;
int			nAllocString;
int			sAllocString;
int			nAllocPerm;
int			sAllocPerm;

bool			fBootDb;
FILE *			fpArea;
char			strArea[MAX_INPUT_LENGTH];


/*
 * Local booting procedures.
*/
void init_mm(void);
void crypto_init(void);
void load_shares(void);
void fix_objprogs(void);
void fix_roomprogs(void);
void load_reboot_objs(void);
void load_socials(FILE *fp);
void load_notes(void);
void load_bans(void);
void fix_rooms(void);
void fix_object_locks(void);
void fix_portal_destinations(void);
void fix_object_type_data(void);
void fix_area_fields(void);
void fix_index_inheritance(void);
void fix_mobprogs(void);
void reset_area(AREA_DATA * pArea);
void chance_create_mob(ROOM_INDEX_DATA *pRoom, MOB_INDEX_DATA *pMobIndex, int chance);
void reset_npc_sailing_boats (void);
void fix_tokenprogs(void);
void check_area_versions(void);
void migrate_shopkeeper_resets(AREA_DATA *area);
void fix_areaprogs(void);
void fix_questprogs(void);
void fix_instanceprogs(void);
void fix_dungeonprogs(void);
void fix_dungeon_rooms(void);
void fix_dungeon_floors(void);
void fix_blueprint_references(void);
void fix_events(void);
void area_dependencies_rebuild_all(void);



bool persist_load(void);

void init_string_space()
{
    if ((string_space = calloc(1, MAX_STRING)) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Boot_db: can't alloc %d string space.", MAX_STRING);
        exit(1);
    }
    top_string	= string_space;
}

/*
 * Fixup cross-area reset references after all areas are loaded.
 * During deserialization, area UIDs are stored in WNUM_LOAD format.
 * This function converts them to WNUM format with proper area pointers.
 */
void fixup_area_reset_references(void)
{
    AREA_DATA *area;
    ITERATOR it;
    ROOM_INDEX_DATA *room;
    int fixed = 0, failed = 0;
    
    for (area = area_first; area; area = area->next) {
        iterator_start(&it, area->room_list);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) != NULL) {
            for (RESET_DATA *reset = room->reset_first; reset; reset = reset->next) {
                // Convert arg1 WNUM_LOAD to WNUM for entity-referencing commands
                switch (reset->command) {
                    case 'M': case 'O': case 'G': case 'E':
                    {
                        long auid = reset->arg1.load.auid;
                        long vnum = reset->arg1.load.vnum;
                        
                        if (auid) {
                            AREA_DATA *found_area = get_area_index(auid);
                            if (found_area) {
                                reset->arg1.wnum.pArea = found_area;
                                reset->arg1.wnum.vnum = vnum;
                                fixed++;
                            } else {
                                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, 
                                    "fixup_area_reset_references: Could not resolve area UID %ld for reset '%c' in room %ld",
                                    auid, reset->command, room->vnum);
                                reset->arg1.wnum.pArea = NULL;
                                reset->arg1.wnum.vnum = vnum;
                                failed++;
                            }
                        } else {
                            // auid=0 means legacy (use current area)
                            reset->arg1.wnum.pArea = NULL;
                            reset->arg1.wnum.vnum = vnum;
                        }
                        break;
                    }
                    
                    case 'P':
                    {
                        // Convert arg1 (object)
                        long auid1 = reset->arg1.load.auid;
                        long vnum1 = reset->arg1.load.vnum;
                        
                        if (auid1) {
                            AREA_DATA *found_area = get_area_index(auid1);
                            if (found_area) {
                                reset->arg1.wnum.pArea = found_area;
                                reset->arg1.wnum.vnum = vnum1;
                                fixed++;
                            } else {
                                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                    "fixup_area_reset_references: Could not resolve object area UID %ld for reset 'P' in room %ld",
                                    auid1, room->vnum);
                                reset->arg1.wnum.pArea = NULL;
                                reset->arg1.wnum.vnum = vnum1;
                                failed++;
                            }
                        } else {
                            reset->arg1.wnum.pArea = NULL;
                            reset->arg1.wnum.vnum = vnum1;
                        }
                        
                        // Convert arg3 (container)
                        long auid3 = reset->arg3.load.auid;
                        long vnum3 = reset->arg3.load.vnum;
                        
                        if (auid3) {
                            AREA_DATA *found_area = get_area_index(auid3);
                            if (found_area) {
                                reset->arg3.wnum.pArea = found_area;
                                reset->arg3.wnum.vnum = vnum3;
                                fixed++;
                            } else {
                                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                    "fixup_area_reset_references: Could not resolve container area UID %ld for reset 'P' in room %ld",
                                    auid3, room->vnum);
                                reset->arg3.wnum.pArea = NULL;
                                reset->arg3.wnum.vnum = vnum3;
                                failed++;
                            }
                        } else {
                            reset->arg3.wnum.pArea = NULL;
                            reset->arg3.wnum.vnum = vnum3;
                        }
                        break;
                    }
                }
            }
        }
        iterator_stop(&it);
    }
    
    if (fixed > 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Fixed up %d cross-area reset references", fixed);
    }
    if (failed > 0) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to resolve %d cross-area reset references", failed);
    }
}

void area_dependency_clear(AREA_DATA *area)
{
    AREA_DEPENDENCY *dependency;
    AREA_DEPENDENCY *next;

    if (!area)
        return;

    for (dependency = area->dependencies; dependency != NULL; dependency = next)
    {
        next = dependency->next;
        free_string(dependency->source_type);
        free_string(dependency->source_name);
        free_string(dependency->reference_type);
        free_string(dependency->target_area_name);
        free_string(dependency->target_type);
        free_string(dependency->target_name);
        free_mem(dependency, sizeof(*dependency));
    }

    area->dependencies = NULL;
    area->dependency_count = 0;
}

void area_dependency_add(AREA_DATA *area, const char *source_type, long source_vnum,
    const char *source_name, const char *reference_type, long target_area_uid,
    const char *target_area_name, const char *target_type, long target_vnum,
    const char *target_name)
{
    AREA_DEPENDENCY *dependency;

    if (!area || target_area_uid <= 0 || target_vnum <= 0)
        return;

    dependency = alloc_mem(sizeof(*dependency));
    if (!dependency)
        return;

    dependency->next = area->dependencies;
    dependency->source_type = str_dup(source_type ? source_type : "unknown");
    dependency->source_vnum = source_vnum;
    dependency->source_name = str_dup(source_name ? source_name : "");
    dependency->reference_type = str_dup(reference_type ? reference_type : "reference");
    dependency->target_area_uid = target_area_uid;
    dependency->target_area_name = str_dup(target_area_name ? target_area_name : "");
    dependency->target_type = str_dup(target_type ? target_type : "entity");
    dependency->target_vnum = target_vnum;
    dependency->target_name = str_dup(target_name ? target_name : "");

    area->dependencies = dependency;
    area->dependency_count++;
}

static void area_dependency_add_resolved(AREA_DATA *source_area, const char *source_type,
    long source_vnum, const char *source_name, const char *reference_type,
    AREA_DATA *target_area, const char *target_type, long target_vnum,
    const char *target_name)
{
    if (!source_area || !target_area)
        return;

    if (target_area->uid <= 0 || target_area->uid == source_area->uid)
        return;

    area_dependency_add(source_area, source_type, source_vnum, source_name,
        reference_type, target_area->uid, target_area->name,
        target_type, target_vnum, target_name);
}

static void area_dependency_scan_prog_bank(AREA_DATA *source_area, const char *source_type,
    long source_vnum, const char *source_name, LLIST **progs)
{
    int slot;

    if (!source_area || !progs)
        return;

    for (slot = 0; slot < TRIGSLOT_MAX; slot++)
    {
        ITERATOR it;
        PROG_LIST *trigger;

        if (!progs[slot])
            continue;

        iterator_start(&it, progs[slot]);
        while ((trigger = (PROG_LIST *)iterator_nextdata(&it)) != NULL)
        {
            AREA_DATA *target_area = NULL;
            SCRIPT_DATA *script = trigger->script;
            long target_vnum = 0;

            if (script && script->area)
            {
                target_area = script->area;
                target_vnum = script->vnum;
            }
            else if (trigger->script_is_widevnum && trigger->script_load.auid > 0)
            {
                target_area = get_area_from_uid(trigger->script_load.auid);
                target_vnum = trigger->script_load.vnum;
            }
            else
            {
                WNUM script_wnum;
                if (resolve_widevnum(trigger->vnum, source_area, &script_wnum))
                {
                    target_area = script_wnum.pArea;
                    target_vnum = script_wnum.vnum;
                }
            }

            if (!target_area || target_vnum <= 0)
                continue;

            area_dependency_add_resolved(source_area, source_type, source_vnum,
                source_name, "script_trigger", target_area, "script",
                target_vnum, script ? script->name : "");
        }
        iterator_stop(&it);
    }
}

void area_dependencies_rebuild_for_area(AREA_DATA *area)
{
    int hash_index;

    if (!area)
        return;

    area_dependency_clear(area);

    if (area->post_office_wnum.vnum > 0)
    {
        AREA_DATA *target_area = area->post_office_wnum.pArea ? area->post_office_wnum.pArea : area;
        ROOM_INDEX_DATA *target_room = target_area ? get_room_index(target_area, area->post_office_wnum.vnum) : NULL;

        area_dependency_add_resolved(area, "area", area->uid, area->name,
            "post_office", target_area, "room", area->post_office_wnum.vnum,
            target_room ? target_room->name : "");
    }

    if (area->airship_land_wnum.vnum > 0)
    {
        AREA_DATA *target_area = area->airship_land_wnum.pArea;
        ROOM_INDEX_DATA *target_room = target_area ? get_room_index(target_area, area->airship_land_wnum.vnum) : NULL;

        if (!target_area)
            target_area = find_area_by_vnum(area->airship_land_wnum.vnum, NULL);

        area_dependency_add_resolved(area, "area", area->uid, area->name,
            "airship_land", target_area, "room", area->airship_land_wnum.vnum,
            target_room ? target_room->name : "");
    }

    area_dependency_scan_prog_bank(area, "area", area->uid, area->name,
        area->progs ? area->progs->progs : NULL);

    for (TRADE_ITEM *trade = area->trade_list; trade != NULL; trade = trade->next)
    {
        AREA_DATA *target_area;
        OBJ_INDEX_DATA *target_obj;

        if (trade->obj_wnum.vnum <= 0)
            continue;

        target_area = trade->obj_wnum.pArea ? trade->obj_wnum.pArea : area;
        target_obj = target_area ? get_obj_index(target_area, trade->obj_wnum.vnum) : NULL;

        area_dependency_add_resolved(area, "trade", trade->obj_wnum.vnum,
            trade_table[trade->trade_type].name, "trade_item", target_area,
            "object", trade->obj_wnum.vnum, target_obj ? target_obj->short_descr : "");
    }

    for (hash_index = 0; hash_index < MAX_KEY_HASH; hash_index++)
    {
        ROOM_INDEX_DATA *room;
        MOB_INDEX_DATA *mob;
        OBJ_INDEX_DATA *obj;
        TOKEN_INDEX_DATA *token;
        BLUEPRINT *blueprint;
        DUNGEON_INDEX_DATA *dungeon_index;

        for (room = area->room_index_hash[hash_index]; room != NULL; room = room->next)
        {
            int door;

            if (room->parent_wnum.vnum > 0)
            {
                AREA_DATA *parent_area = room->parent_wnum.pArea;
                ROOM_INDEX_DATA *parent_room = parent_area ? get_room_index(parent_area, room->parent_wnum.vnum) : NULL;

                area_dependency_add_resolved(area, "room", room->vnum, room->name,
                    "parent_room", parent_area, "room", room->parent_wnum.vnum,
                    parent_room ? parent_room->name : "");
            }

            for (RESET_DATA *reset = room->reset_first; reset != NULL; reset = reset->next)
            {
                switch (reset->command)
                {
                    case 'M':
                    {
                        AREA_DATA *target_area = reset->arg1.wnum.pArea ? reset->arg1.wnum.pArea : area;
                        MOB_INDEX_DATA *target_mob = get_mob_index(target_area, reset->arg1.wnum.vnum);

                        area_dependency_add_resolved(area, "room", room->vnum, room->name,
                            "reset:M", target_area, "mobile", reset->arg1.wnum.vnum,
                            target_mob ? target_mob->short_descr : "");
                        break;
                    }

                    case 'O':
                    case 'G':
                    case 'E':
                    {
                        AREA_DATA *target_area = reset->arg1.wnum.pArea ? reset->arg1.wnum.pArea : area;
                        OBJ_INDEX_DATA *target_obj = get_obj_index(target_area, reset->arg1.wnum.vnum);

                        area_dependency_add_resolved(area, "room", room->vnum, room->name,
                            formatf("reset:%c", reset->command), target_area, "object",
                            reset->arg1.wnum.vnum, target_obj ? target_obj->short_descr : "");
                        break;
                    }

                    case 'P':
                    {
                        AREA_DATA *target_obj_area = reset->arg1.wnum.pArea ? reset->arg1.wnum.pArea : area;
                        AREA_DATA *target_container_area = reset->arg3.wnum.pArea ? reset->arg3.wnum.pArea : area;
                        OBJ_INDEX_DATA *target_obj = get_obj_index(target_obj_area, reset->arg1.wnum.vnum);
                        OBJ_INDEX_DATA *target_container = get_obj_index(target_container_area, reset->arg3.wnum.vnum);

                        area_dependency_add_resolved(area, "room", room->vnum, room->name,
                            "reset:P_object", target_obj_area, "object", reset->arg1.wnum.vnum,
                            target_obj ? target_obj->short_descr : "");

                        area_dependency_add_resolved(area, "room", room->vnum, room->name,
                            "reset:P_container", target_container_area, "object", reset->arg3.wnum.vnum,
                            target_container ? target_container->short_descr : "");
                        break;
                    }
                }
            }

            for (door = 0; door <= 9; door++)
            {
                EXIT_DATA *exit_data = room->exit[door];

                if (!exit_data)
                    continue;

                if (exit_data->u1.to_room && exit_data->u1.to_room->area)
                {
                    area_dependency_add_resolved(area, "room", room->vnum, room->name,
                        formatf("exit:%s", dir_name[door]), exit_data->u1.to_room->area,
                        "room", exit_data->u1.to_room->vnum,
                        exit_data->u1.to_room->name);
                }

                if (exit_data->door.lock.key_wnum.vnum > 0)
                {
                    AREA_DATA *key_area = exit_data->door.lock.key_wnum.pArea ? exit_data->door.lock.key_wnum.pArea : area;
                    OBJ_INDEX_DATA *key_obj = get_obj_index(key_area, exit_data->door.lock.key_wnum.vnum);

                    area_dependency_add_resolved(area, "room", room->vnum, room->name,
                        "exit_key", key_area, "object", exit_data->door.lock.key_wnum.vnum,
                        key_obj ? key_obj->short_descr : "");
                }

                if (exit_data->door.rs_lock.key_wnum.vnum > 0)
                {
                    AREA_DATA *key_area = exit_data->door.rs_lock.key_wnum.pArea ? exit_data->door.rs_lock.key_wnum.pArea : area;
                    OBJ_INDEX_DATA *key_obj = get_obj_index(key_area, exit_data->door.rs_lock.key_wnum.vnum);

                    area_dependency_add_resolved(area, "room", room->vnum, room->name,
                        "exit_rs_key", key_area, "object", exit_data->door.rs_lock.key_wnum.vnum,
                        key_obj ? key_obj->short_descr : "");
                }
            }

            area_dependency_scan_prog_bank(area, "room", room->vnum, room->name,
                room->progs ? room->progs->progs : NULL);
        }

        for (mob = area->mob_index_hash[hash_index]; mob != NULL; mob = mob->next)
        {
            if (mob->parent_wnum.vnum > 0)
            {
                AREA_DATA *parent_area = mob->parent_wnum.pArea;
                MOB_INDEX_DATA *parent_mob = parent_area ? get_mob_index(parent_area, mob->parent_wnum.vnum) : NULL;

                area_dependency_add_resolved(area, "mobile", mob->vnum, mob->short_descr,
                    "parent_mobile", parent_area, "mobile", mob->parent_wnum.vnum,
                    parent_mob ? parent_mob->short_descr : "");
            }

            if (mob->corpse_wnum.vnum > 0)
            {
                AREA_DATA *corpse_area = mob->corpse_wnum.pArea ? mob->corpse_wnum.pArea : area;
                OBJ_INDEX_DATA *corpse_obj = get_obj_index(corpse_area, mob->corpse_wnum.vnum);

                area_dependency_add_resolved(area, "mobile", mob->vnum, mob->short_descr,
                    "corpse_object", corpse_area, "object", mob->corpse_wnum.vnum,
                    corpse_obj ? corpse_obj->short_descr : "");
            }

            if (mob->zombie_wnum.vnum > 0)
            {
                AREA_DATA *zombie_area = mob->zombie_wnum.pArea ? mob->zombie_wnum.pArea : area;
                OBJ_INDEX_DATA *zombie_obj = get_obj_index(zombie_area, mob->zombie_wnum.vnum);

                area_dependency_add_resolved(area, "mobile", mob->vnum, mob->short_descr,
                    "zombie_object", zombie_area, "object", mob->zombie_wnum.vnum,
                    zombie_obj ? zombie_obj->short_descr : "");
            }

            area_dependency_scan_prog_bank(area, "mobile", mob->vnum, mob->short_descr,
                mob->progs);
        }

        for (obj = area->obj_index_hash[hash_index]; obj != NULL; obj = obj->next)
        {
            if (obj->parent_wnum.vnum > 0)
            {
                AREA_DATA *parent_area = obj->parent_wnum.pArea;
                OBJ_INDEX_DATA *parent_obj = parent_area ? get_obj_index(parent_area, obj->parent_wnum.vnum) : NULL;

                area_dependency_add_resolved(area, "object", obj->vnum, obj->short_descr,
                    "parent_object", parent_area, "object", obj->parent_wnum.vnum,
                    parent_obj ? parent_obj->short_descr : "");
            }

            if (obj->lock && obj->lock->key_wnum.vnum > 0)
            {
                AREA_DATA *key_area = obj->lock->key_wnum.pArea ? obj->lock->key_wnum.pArea : area;
                OBJ_INDEX_DATA *key_obj = get_obj_index(key_area, obj->lock->key_wnum.vnum);

                area_dependency_add_resolved(area, "object", obj->vnum, obj->short_descr,
                    "object_key", key_area, "object", obj->lock->key_wnum.vnum,
                    key_obj ? key_obj->short_descr : "");
            }

            if (obj->item_type == ITEM_PORTAL)
            {
                long dest_vnum = obj->_portal ? PORTAL(obj)->params[0] : legacy_obj_index_value_get(obj, 3);
                long dest_area_uid = obj->_portal ? PORTAL(obj)->params[4] : legacy_obj_index_value_get(obj, 4);
                long portal_flags = obj->_portal ? PORTAL(obj)->flags : legacy_obj_index_value_get(obj, 2);

                if (dest_vnum > 0 && !IS_SET(portal_flags, GATE_DUNGEON))
                {
                    AREA_DATA *dest_area = dest_area_uid > 0
                        ? get_area_from_uid(dest_area_uid)
                        : find_area_by_vnum(dest_vnum, area);
                    ROOM_INDEX_DATA *dest_room = dest_area ? get_room_index(dest_area, dest_vnum) : NULL;

                    area_dependency_add_resolved(area, "object", obj->vnum, obj->short_descr,
                        "portal_destination", dest_area, "room", dest_vnum,
                        dest_room ? dest_room->name : "");
                }
            }

            area_dependency_scan_prog_bank(area, "object", obj->vnum, obj->short_descr,
                obj->progs);
        }

        for (token = area->token_index_hash[hash_index]; token != NULL; token = token->next)
        {
            area_dependency_scan_prog_bank(area, "token", token->vnum, token->name,
                token->progs);
        }

        for (blueprint = area->blueprint_hash[hash_index]; blueprint != NULL; blueprint = blueprint->next)
        {
            area_dependency_scan_prog_bank(area, "blueprint", blueprint->vnum,
                blueprint->name, blueprint->progs);
        }

        for (dungeon_index = area->dungeon_index_hash[hash_index]; dungeon_index != NULL; dungeon_index = dungeon_index->next)
        {
            area_dependency_scan_prog_bank(area, "dungeon", dungeon_index->vnum,
                dungeon_index->name, dungeon_index->progs);
        }
    }
}

void area_dependencies_rebuild_all(void)
{
    AREA_DATA *area;
    long total_records = 0;

    for (area = area_first; area != NULL; area = area->next)
    {
        area_dependencies_rebuild_for_area(area);
        total_records += area->dependency_count;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_INIT,
        "Built area dependency map (%ld total cross-area references)",
        total_records);
}


/* Top-level booting function*/
void boot_db(void)
{
    int i;
    FILE *fp;
    static GLOBAL_DATA gconfig_zero;

    log_init(ZLOG_CONF);

    // If shutdown.txt exists, nuke it.
    {
        char shutdown_file_buf[MAX_INPUT_LENGTH];
        const char *shutdown_file = resolve_game_path(SHUTDOWN_FILE, shutdown_file_buf, sizeof(shutdown_file_buf));
        unlink(shutdown_file);
    }
    /*
     * Init some data space stuff.
     */
    {
    if ((string_space = calloc(1, MAX_STRING)) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Boot_db: can't alloc %d string space.", MAX_STRING);
        exit(1);
    }
    top_string	= string_space;
    fBootDb		= true;
    }

    gconfig = gconfig_zero;
    if (gconfig_read()==1) exit(1);

    /*
     * Init random number generator.
     */
    init_mm();

    if (!script_validate_trigger_table()) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR,
                    "Trigger table validation failed; startup state is unsafe.");
    }


    crypto_init();
    global_quest.mobs = NULL;
    global_quest.objects = NULL;

    auction_info.item           = NULL;
    auction_info.owner          = NULL;
    auction_info.high_bidder    = NULL;
    auction_info.current_bid    = 0;
    auction_info.status         = 0;
    auction_info.gold_held	= 0;
    auction_info.silver_held	= 0;

//    logAll = true; /*setting this on for now*/

    /*
     * Set time and weather.
     */
    {
    long lhour, lday, lmonth;
    int counter;

    lhour		= (current_time - 650336715)
            / (PULSE_TICK / PULSE_PER_SECOND);
    time_info.hour	= lhour  % 24;
    lday		= lhour  / 24;
    time_info.day	= lday   % 35;
    lmonth		= lday   / 35;
    time_info.month	= lmonth % 12;
    time_info.year	= lmonth / 12;

    for (counter = 0; counter < SECT_MAX; counter++)
    {

    if (time_info.hour <  5) weather_info.sunlight = SUN_DARK;
    else if (time_info.hour <  6) weather_info.sunlight = SUN_RISE;
    else if (time_info.hour < 19) weather_info.sunlight = SUN_LIGHT;
    else if (time_info.hour < 20) weather_info.sunlight = SUN_SET;
    else                            weather_info.sunlight = SUN_DARK;

    weather_info.change	= 0;
    weather_info.mmhg	= 960;
    if (time_info.month >= 7 && time_info.month <=12)
        weather_info.mmhg += number_range(1, 50);
    else
        weather_info.mmhg += number_range(1, 80);

         if (weather_info.mmhg <=  980) weather_info.sky = SKY_LIGHTNING;
    else if (weather_info.mmhg <= 1000) weather_info.sky = SKY_RAINING;
    else if (weather_info.mmhg <= 1020) weather_info.sky = SKY_CLOUDY;
    else                                  weather_info.sky = SKY_CLOUDLESS;
    }

    lhour = (lhour+MOON_OFFSET) % MOON_PERIOD;
    lhour = (lhour+MOON_PERIOD) % MOON_PERIOD;

    if(lhour <= (MOON_CARDINAL_HALF)) time_info.moon = MOON_NEW;
    else if(lhour < (MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_CRESCENT;
    else if(lhour <= (MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FIRST_QUARTER;
    else if(lhour < (2*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_GIBBOUS;
    else if(lhour <= (2*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FULL;
    else if(lhour < (3*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_GIBBOUS;
    else if(lhour <= (3*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_LAST_QUARTER;
    else if(lhour < (4*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_CRESCENT;
    else time_info.moon = MOON_NEW;

    }


    /* First initialize the pending changes list */
    if (!pending_changes) {
        pending_changes = list_create(false);
        if (!pending_changes) {
            fprintf(stderr, "Error: Failed to create pending_changes list.\n");
        }
    }
    
    if (!reserved_vnums) {
        reserved_vnums = list_create(false);
        if (!reserved_vnums) {
            fprintf(stderr, "Error: Failed to create reserved_vnums list.\n");
        }
    }

    if (!load_localizations())
    {
        // Error already reported
        exit(1);
    }

    load_reserved();
    script_validate_entity_tables();

    /* Load settings and changesets */
    load_changesets();

    /*
     * Assign gsn's for skills which have them.
     * Syn - while we're at it, let's set up an NPC skills table as well, to reduce
     * processor load.
     */
    {
    int lev;

    for (lev = 0; lev != MAX_MOB_SKILL_LEVEL; lev++)
        mob_skill_table[lev] = 40 + 19 * log10(lev);

    }

    // Load trait definitions (must precede load_races)
    load_trait_definitions();

    // Load races from JSON files (new race system)
    load_races();

    // Load skills from JSON files (new skill system)
    // On first run, bootstraps from legacy skill_table[] and saves JSON files.
    load_skill_data();

    // Load skill groups from data/skill_groups/ JSON files.
    load_skill_groups();

    // Load songs from data/songs.json.
    load_songs();

    // Load classes from JSON files
    load_class_data();

    // Load random string generators from JSON (rsgedit cache/persistence)
    load_rsg_data();

    // Load liquids from JSON (liqedit cache/persistence)
    // On first run, bootstraps from liqedit defaults and saves liquids.json.
    load_liquid_data();

    // Load materials from JSON (matedit cache/persistence)
    // On first run, bootstraps from legacy material_table[] and saves materials.json.
    load_material_data();

    // Load sector settings from JSON (sectoredit cache/persistence)
    // On first run, bootstraps from built-in movement_loss defaults and saves sectors.json.
    load_sector_data();

    // Load corpse definitions from JSON (corpsedit cache/persistence)
    // On first run, bootstraps from built-in corpse_info_table defaults and saves corpses.json.
    load_corpse_data();

    // Initialize certain lists
    loaded_instances = list_create(false);
    loaded_dungeons = list_create(false);
    loaded_ships = list_create(false);
    loaded_special_keys = list_create(false);

    // Initialize item type metadata and compatibility rules
    item_types_init();

    if (!load_commands()) exit(1);
    log_message(LOG_LEVEL_INFO, LOG_INIT, "commands loaded.");

    /*
     * Read in all the area files.
     */
    {
        FILE *fpList;
        char area_list_buf[MAX_INPUT_LENGTH];
        char area_dir_buf[MAX_INPUT_LENGTH];
        const char *area_list_path = resolve_game_path(AREA_LIST, area_list_buf, sizeof(area_list_buf));
        const char *area_dir_path = resolve_game_path(AREA_DIR, area_dir_buf, sizeof(area_dir_buf));
        //char log_buf[MAX_STRING_LENGTH];

        log_message(LOG_LEVEL_INFO, LOG_INIT, "Loading areas from area.lst file...");

        if ((fpList = fopen(area_list_path, "r")) == NULL) {
            perror(area_list_path);
            exit(1);
        }

        for (; ;)
        {
            AREA_DATA *area = NULL;
            LLIST_AREA_DATA *link;

            strcpy(strArea, fread_word(fpList));
            if (strArea[0] == '$')
                break;

            if (!str_cmp(strArea, "help.are") || !str_cmp(strArea, "social.are"))
                continue;

            /* Resolve area list entries by stem so extensions are optional.
             * Accepts entries like: "foo", "foo.are", or "foo.json". */
            char stem[MAX_STRING_LENGTH];
            char json_filename[MAX_STRING_LENGTH + 10];
            char json_fullpath[MAX_STRING_LENGTH + 30];
            const char *dot = strrchr(strArea, '.');

            if (dot != NULL) {
                size_t stem_len = dot - strArea;
                if (stem_len >= sizeof(stem))
                    stem_len = sizeof(stem) - 1;
                strncpy(stem, strArea, stem_len);
                stem[stem_len] = '\0';
            } else {
                strncpy(stem, strArea, sizeof(stem) - 1);
                stem[sizeof(stem) - 1] = '\0';
            }

            snprintf(json_filename, sizeof(json_filename), "%s.json", stem);
            snprintf(json_fullpath, sizeof(json_fullpath), "%s%s", area_dir_path, json_filename);

            if (access(json_fullpath, F_OK) == 0) {
                log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Loading area from JSON: %s", json_fullpath);
                area = json_area_load(json_filename);
                if (area) {
                    log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Successfully loaded JSON area: %s", json_filename);
                } else {
                    log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                        "Failed to load JSON area: %s", json_fullpath);
                }
            } else {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                    "JSON area file not found: %s", json_fullpath);
            }

            if (!area) {
                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to load area: %s", strArea);
                exit(2);
            }

            area->next = NULL;

            if (area_first == NULL)
                area_first = area;
            else
                area_last->next = area;
            area_last = area;

            // Add to script usable list
            link = alloc_mem(sizeof(LLIST_AREA_DATA));
            link->area = area;
            link->uid = area->uid;
            if( !list_appendlink(loaded_areas, link) ) {
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Failed to add area to loaded list due to memory issues with 'list_appendlink'.");
                abort();
            }
        }

        fpArea = NULL;

        fclose(fpList);
    }

    /*
     * Fix up exits.
     * Declare db booting over.
     * Reset all areas once.
     * Load up the notes and ban files.
     */
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_rooms");
    fix_rooms();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving object lock keys");
    fix_object_locks();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving portal destination areas");
    fix_portal_destinations();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Ensuring object type data structs");
    fix_object_type_data();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving area/mob/trade widevnum fields");
    fix_area_fields();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving room/mob/object parent inheritance");
    fix_index_inheritance();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving channel requirement token widevnums");
    channel_registry_fix_requirements();
    resolve_newbie_tables();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving quest-v2 widevnum fields");
    fix_quests_v2();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_vlinks");
    fix_vlinks();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_shops");
    fix_shops();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Skipping legacy blueprints.dat/dungeons.dat loaders (using area JSON only)");

    // Ships are now loaded from area JSON files
    // log_message(LOG_LEVEL_INFO, LOG_INIT, "Loading ships");
    // load_ships();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing variable_index_fix");
    variable_index_fix();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing variable_fix");
    variable_fix_global();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_mobprogs");
    fix_mobprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_objprogs");
    fix_objprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_roomprogs");
    fix_roomprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_tokenprogs");
    fix_tokenprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_areaprogs");
    fix_areaprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_instanceprogs");
    fix_instanceprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_dungeonprogs");
    fix_dungeonprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing fix_questprogs");
    fix_questprogs();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing dungeon room references");
    fix_dungeon_rooms();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing blueprint room references");
    fix_blueprint_references();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing dungeon floor references");
    fix_dungeon_floors();
    fix_events();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Loading persistance");
    if(!persist_load()) {
        perror("Persistance");
        exit(1);
    }

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Opening churches, new format");
    read_churches_new();

    /* Fixup cross-area reset references after all areas are loaded but BEFORE area_update */
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving cross-area reset references");
    fixup_area_reset_references();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Building cross-area dependency map");
    area_dependencies_rebuild_all();

    fBootDb	= false;
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing area_update");
    area_update(true);
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing load_notes");
    load_notes();
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing load_bans");
    load_bans();
    //log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing load_reboot_objs");
    //load_reboot_objs();


    log_message(LOG_LEVEL_INFO, LOG_INIT, "Opening projects");
    read_projects();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Opening immortal staff");
    read_immstaff();

    load_socials_file();
    /* Legacy social.are loading path removed (JSON/dat loader handles socials). */

    help_greeting = str_dup("hello");

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing read_gq");
    read_gq();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing read_chat_rooms");
    read_chat_rooms();

//    log_message(LOG_LEVEL_INFO, LOG_INIT, "Reading permanent objs");
//    read_permanent_objs();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Loading instances");
    load_instances();

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Reading helpfiles");
    read_helpfiles_new();

    index_helpfiles(1, topHelpCat);

/*    load_sailing_boats();*/
/*    load_npc_ships();*/
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Doing read_mail");
    read_mail();
/*  reset_npc_sailing_boats();*/
    leaderboard_init_all();
    leaderboard_load_backup();
    leaderboard_seed_redis();

    /* set global attributes*/

    /* default global boosts should start at nothing (100%)*/
    for (i = 0; boost_table[i].name != NULL; i++)
    {
    boost_table[i].boost = 100;
    boost_table[i].timer = 0;
    }

    reckoning_timer = 0;
    reckoning_cooldown_timer = 0;
    pre_reckoning = 0;
    reckoning_duration = 30;
    reckoning_intensity = 100;
    reckoning_chance = 5;
    objRepop = false;

    prog_data_virtual = new_prog_data();

    if ((fp = fopen("church_pks.txt", "r")) != NULL) {
    update_church_pks();
    fclose(fp);

    }

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Checking area versions");
    check_area_versions();

    gconfig_write();

    /* Queue areas for async Redis cache warming */
    if (redis_is_available()) {
        AREA_DATA *area;
        int queued = 0;

        log_message(LOG_LEVEL_INFO, LOG_INIT, "Queueing areas for Redis cache warming");
        for (area = area_first; area; area = area->next) {
            if (area->file_name && area->file_name[0]) {
                /* Serialize and queue for async caching */
                char *json_str = json_area_serialize_to_string(area);
                if (json_str) {
                    redis_queue_area_cache_warm(area->file_name, json_str);
                    free(json_str);
                    queued++;
                }
            }
        }
        log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Queued %d areas for cache warming", queued);
    }

    // Send boot errors to coders staff duty
    send_boot_errors_to_coders();
}


int get_this_class(CHAR_DATA *ch, int sn)
{
    int this_class;
    int level;

    if (skill_table[sn].name == NULL)
    return 9999;

    this_class = 9999;

    if (ch->pcdata->class_mage != -1
        && (level = skill_table[sn].skill_level[ch->pcdata->class_mage]) < 31)
    {
    this_class = ch->pcdata->class_mage;
    }
    else
    if (ch->pcdata->class_cleric != -1
        && (level = skill_table[sn].skill_level[ch->pcdata->class_cleric]) < 31)
    {
        this_class = ch->pcdata->class_cleric;
    }
    else
        if (ch->pcdata->class_thief != -1
            && (level = skill_table[sn].skill_level[ch->pcdata->class_thief]) < 31)
        {
        this_class = ch->pcdata->class_thief;
        }
        else
        if (ch->pcdata->class_warrior != -1
            && (level = skill_table[sn].skill_level[ch->pcdata->class_warrior]) < 31)
        {
            this_class = ch->pcdata->class_warrior;
        }

    if (race_get_trait_bool(ch->race, "classless_skills")) {
    if (skill_table[sn].skill_level[0] < 31)
        return 0;
    if (skill_table[sn].skill_level[1] < 31)
        return 1;
    if (skill_table[sn].skill_level[2] < 31)
        return 2;
    if (skill_table[sn].skill_level[3] < 31)
        return 3;

    return 0;
    }

    return this_class;
}


void new_reset(ROOM_INDEX_DATA *pR, RESET_DATA *pReset)
{
    RESET_DATA *pr;

    if (!pR)
       return;

    pr = pR->reset_last;

    if (!pr)
    {
        pR->reset_first = pReset;
        pR->reset_last  = pReset;
    }
    else
    {
        pR->reset_last->next = pReset;
        pR->reset_last       = pReset;
        pR->reset_last->next = NULL;
    }

    top_reset++;
}


/**
 * fix_portal_destinations - Resolve bare portal destination vnums to area UIDs
 *
 * After all areas are loaded, iterates every object index across all areas.
 * For portals with a destination vnum but no destination area UID,
 * looks up which area owns that vnum and stores its UID in the canonical
 * typed portal data (params[4]), keeping legacy value[4] synchronized.
 * Skips dungeon portals.
 */
void fix_portal_destinations(void)
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *obj;
    int iHash;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
            {
                if (obj->item_type == ITEM_PORTAL)
                {
                    long dest_vnum = 0;
                    long dest_area_uid = 0;
                    long portal_flags = 0;

                    if (obj->_portal)
                    {
                        dest_vnum = PORTAL(obj)->params[0];
                        dest_area_uid = PORTAL(obj)->params[4];
                        portal_flags = PORTAL(obj)->flags;
                    }
                    else
                    {
                        dest_vnum = legacy_obj_index_value_get(obj, 3);
                        dest_area_uid = legacy_obj_index_value_get(obj, 4);
                        portal_flags = legacy_obj_index_value_get(obj, 2);
                    }

                    if (dest_vnum > 0
                    &&  dest_area_uid == 0
                    &&  !IS_SET(portal_flags, GATE_DUNGEON))
                    {
                        AREA_DATA *dest_area = find_area_by_vnum(dest_vnum, pArea);
                        if (dest_area)
                        {
                            legacy_obj_index_value_set(obj, 4, dest_area->uid);
                            if (obj->_portal)
                                PORTAL(obj)->params[4] = dest_area->uid;
                        }
                    }
                }
            }
        }
    }
}


/*
 * fix_object_type_data - Ensure all object indexes have their type structs
 *
 * After all areas are loaded, iterates every object index. For any object
 * whose primary item_type supports typed data (has_typed_data == true) but
 * is missing its type struct, attempts to:
 *   1. Migrate from legacy value[] array (if any non-zero values exist)
 *   2. Otherwise, allocate a default (zeroed) struct
 *
 * This handles JSON areas that were saved after value[] was removed from
 * the save path but before type_data was populated — the data was never
 * migrated, leaving objects with neither values nor type structs.
 */
void fix_object_type_data(void)
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *obj;
    int iHash;
    int migrated = 0;
    int allocated = 0;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
            {
                int type = obj->item_type;

                /* Skip invalid/unknown types */
                if (type <= 0 || type >= ITEM__MAX)
                    continue;

                /* Only care about types that should have typed data */
                if (!item_type_info[type].has_typed_data)
                    continue;

                /* Check if the type struct is already present via type_flags */
                if (TBIT_TST(obj->type_flags, type))
                    continue;

                /* Type struct is missing — check if value[] has any data to migrate */
                bool has_values = false;
                for (int i = 0; i < 8; i++)
                {
                    if (obj->value[i] != 0)
                    {
                        has_values = true;
                        break;
                    }
                }

                if (has_values)
                {
                    /* Migrate from value[] array */
                    obj_index_migrate_values_to_types(obj);
                    migrated++;
                }
                else
                {
                    /* No values to migrate — allocate a default struct */
                    obj_index_alloc_type_data(obj, type);
                    allocated++;
                }
            }
        }
    }

    if (migrated > 0 || allocated > 0)
    {
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
            "fix_object_type_data: migrated %d objects from value[], "
            "allocated %d default type structs", migrated, allocated);
    }
}


/*
 * fix_object_locks - Resolve key_load references to key_wnum for object index locks
 *
 * After all areas are loaded, translates the persistent key_load (auid + vnum)
 * into runtime key_wnum (pArea pointer + vnum) for every object index that has a lock.
 */
void fix_object_locks(void)
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *obj;
    int iHash;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
            {
                if (obj->lock && obj->lock->key_load.vnum > 0)
                {
                    obj->lock->key_wnum.pArea = obj->lock->key_load.auid > 0
                        ? get_area_from_uid(obj->lock->key_load.auid) : pArea;
                    obj->lock->key_wnum.vnum = obj->lock->key_load.vnum;
                }
            }
        }
    }
}

/*
 * fix_area_fields - Resolve WNUM_LOAD fields on areas and mob indices
 *
 * After all areas are loaded, translates persistent area_uid + vnum pairs
 * into runtime pArea + vnum for: area post_office, airship_land_spot,
 * mob index corpse/zombie obj vnums, and trade item obj vnums.
 */
void fix_area_fields(void)
{
    AREA_DATA *pArea;
    MOB_INDEX_DATA *mob;
    int iHash;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        /* Area-level fields (always area-local for post_office, cross-area for airship) */
        if (pArea->post_office_load.vnum > 0)
        {
            if (pArea->post_office_load.auid <= 0)
                pArea->post_office_load.auid = pArea->uid;

            pArea->post_office_wnum.pArea = pArea->post_office_load.auid > 0
                ? get_area_from_uid(pArea->post_office_load.auid) : pArea;
            pArea->post_office_wnum.vnum = pArea->post_office_load.vnum;
        }

        if (pArea->airship_land_load.vnum > 0)
        {
            if (pArea->airship_land_load.auid <= 0)
            {
                AREA_DATA *resolved_area = find_area_by_vnum(pArea->airship_land_load.vnum, NULL);
                if (resolved_area)
                    pArea->airship_land_load.auid = resolved_area->uid;
            }

            pArea->airship_land_wnum.pArea = pArea->airship_land_load.auid > 0
                ? get_area_from_uid(pArea->airship_land_load.auid)
                : find_area_by_vnum(pArea->airship_land_load.vnum, NULL);
            pArea->airship_land_wnum.vnum = pArea->airship_land_load.vnum;
        }

        /* Mob index corpse/zombie object vnums */
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next)
            {
                if (mob->corpse_load.vnum > 0)
                {
                    mob->corpse_wnum.pArea = mob->corpse_load.auid > 0
                        ? get_area_from_uid(mob->corpse_load.auid) : pArea;
                    mob->corpse_wnum.vnum = mob->corpse_load.vnum;
                }

                if (mob->zombie_load.vnum > 0)
                {
                    mob->zombie_wnum.pArea = mob->zombie_load.auid > 0
                        ? get_area_from_uid(mob->zombie_load.auid) : pArea;
                    mob->zombie_wnum.vnum = mob->zombie_load.vnum;
                }

                for (MOB_REPUTATION_DATA *rep = mob->mob_reputations; rep != NULL; rep = rep->next)
                {
                    if (rep->reputation_load.vnum > 0)
                    {
                        rep->reputation = get_reputation_index_auid(rep->reputation_load.auid, rep->reputation_load.vnum);
                        if (!IS_VALID(rep->reputation))
                        {
                            pbugf(LOG_ERROR,
                                  "fix_area_fields: mob %s has invalid reputation %ld#%ld",
                                  widevnum_string(mob->area, mob->vnum, NULL),
                                  rep->reputation_load.auid,
                                  rep->reputation_load.vnum);
                        }
                    }
                    else
                    {
                        rep->reputation = NULL;
                    }
                }
            }
        }

        /* Trade item object vnums */
        for (TRADE_ITEM *trade = pArea->trade_list; trade != NULL; trade = trade->next)
        {
            if (trade->obj_load.vnum > 0)
            {
                trade->obj_wnum.pArea = trade->obj_load.auid > 0
                    ? get_area_from_uid(trade->obj_load.auid) : pArea;
                trade->obj_wnum.vnum = trade->obj_load.vnum;
            }
        }

        /* Shop and stock reputation references */
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next)
            {
                if (mob->pShop)
                {
                    if (mob->pShop->reputation_load.vnum > 0)
                    {
                        mob->pShop->reputation = get_reputation_index_auid(
                            mob->pShop->reputation_load.auid,
                            mob->pShop->reputation_load.vnum);
                    }
                    else
                    {
                        mob->pShop->reputation = NULL;
                    }

                    for (SHOP_STOCK_DATA *stock = mob->pShop->stock; stock != NULL; stock = stock->next)
                    {
                        if (stock->reputation_load.vnum > 0)
                        {
                            stock->reputation = get_reputation_index_auid(
                                stock->reputation_load.auid,
                                stock->reputation_load.vnum);
                        }
                        else
                        {
                            stock->reputation = NULL;
                        }
                    }
                }

                if (mob->pTrainer)
                {
                    for (TRAINER_ENTRY *entry = mob->pTrainer->entries; entry != NULL; entry = entry->next)
                    {
                        if (entry->reputation_load.vnum > 0)
                        {
                            entry->reputation = get_reputation_index_auid(
                                entry->reputation_load.auid,
                                entry->reputation_load.vnum);
                        }
                        else
                        {
                            entry->reputation = NULL;
                        }
                    }
                }
            }
        }
    }
}

static bool has_any_prog_bank(LLIST **progs)
{
    int slot;

    if (!progs)
        return false;

    for (slot = 0; slot < TRIGSLOT_MAX; slot++)
    {
        if (progs[slot] && list_size(progs[slot]) > 0)
            return true;
    }

    return false;
}

static bool same_trigger(const PROG_LIST *a, const PROG_LIST *b)
{
    if (!a || !b)
        return false;

    if (a->trig_type != b->trig_type)
        return false;

    if (a->vnum != b->vnum)
        return false;

    return !str_cmp(a->trig_phrase, b->trig_phrase);
}

static void inherit_prog_bank(LLIST ***dest_bank, LLIST **src_bank)
{
    int slot;
    ITERATOR it;
    PROG_LIST *src_trigger;

    if (!dest_bank || !src_bank)
        return;

    if (!*dest_bank)
        *dest_bank = new_prog_bank();

    for (slot = 0; slot < TRIGSLOT_MAX; slot++)
    {
        if (!src_bank[slot] || list_size(src_bank[slot]) < 1)
            continue;

        iterator_start(&it, src_bank[slot]);
        while ((src_trigger = (PROG_LIST *)iterator_nextdata(&it)))
        {
            bool exists = false;
            ITERATOR dit;
            PROG_LIST *dst_trigger;

            iterator_start(&dit, (*dest_bank)[slot]);
            while ((dst_trigger = (PROG_LIST *)iterator_nextdata(&dit)))
            {
                if (same_trigger(dst_trigger, src_trigger))
                {
                    exists = true;
                    break;
                }
            }
            iterator_stop(&dit);

            if (!exists)
            {
                PROG_LIST *copy = new_trigger();
                copy->trig_type = src_trigger->trig_type;
                copy->trig_phrase = str_dup(src_trigger->trig_phrase);
                copy->trig_number = src_trigger->trig_number;
                copy->numeric = src_trigger->numeric;
                copy->trig_is_widevnum = src_trigger->trig_is_widevnum;
                copy->trig_load = src_trigger->trig_load;
                copy->trig_wnum = src_trigger->trig_wnum;
                copy->vnum = src_trigger->vnum;
                copy->script_is_widevnum = src_trigger->script_is_widevnum;
                copy->script_load = src_trigger->script_load;
                copy->script = src_trigger->script;

                list_appendlink((*dest_bank)[slot], copy);
            }
        }
        iterator_stop(&it);
    }
}

static void inherit_index_vars_with_override(ppVARIABLE child_vars, pVARIABLE parent_vars)
{
    pVARIABLE parent_src;
    pVARIABLE child_saved = NULL;

    if (!child_vars || !parent_vars)
        return;

    if (*child_vars)
        variable_copylist(child_vars, &child_saved, true);

    variable_freelist(child_vars);

    parent_src = parent_vars;
    variable_copylist(&parent_src, child_vars, true);

    if (child_saved)
    {
        variable_copylist(&child_saved, child_vars, true);
        variable_freelist(&child_saved);
    }
}

static void apply_room_parent_inheritance(ROOM_INDEX_DATA *room, ROOM_INDEX_DATA **stack, int depth)
{
    int i;
    ROOM_INDEX_DATA *parent;

    if (!room || room->parent_inherited)
        return;

    if (!room->parent)
    {
        room->parent_inherited = true;
        return;
    }

    if (depth >= 64)
    {
        pbugf(LOG_ERROR, "apply_room_parent_inheritance: depth exceeded for room %s", widevnum_string_room(room, NULL));
        return;
    }

    for (i = 0; i < depth; i++)
    {
        if (stack[i] == room)
        {
            pbugf(LOG_ERROR, "apply_room_parent_inheritance: cycle detected at room %s", widevnum_string_room(room, NULL));
            room->parent = NULL;
            return;
        }
    }

    stack[depth] = room;
    apply_room_parent_inheritance(room->parent, stack, depth + 1);

    parent = room->parent;
    if (!parent || parent == room)
    {
        room->parent_inherited = true;
        return;
    }

    if (room->rs_room_flag[0] == 0 && room->rs_room_flag[1] == 0)
    {
        room->rs_room_flag[0] = parent->rs_room_flag[0];
        room->rs_room_flag[1] = parent->rs_room_flag[1];
    }

    if (room_rs_sector_type(room) == SECT_INSIDE)
        room_set_rs_sector_type(room, room_rs_sector_type(parent));

    if (room->rs_heal_rate == 100)
        room->rs_heal_rate = parent->rs_heal_rate;
    if (room->rs_mana_rate == 100)
        room->rs_mana_rate = parent->rs_mana_rate;
    if (room->rs_move_rate == 100)
        room->rs_move_rate = parent->rs_move_rate;

    if (IS_NULLSTR(room->name))
        room->name = str_dup(parent->name);
    if (IS_NULLSTR(room->description))
        room->description = str_dup(parent->description);

    if (room->progs && parent->progs && !has_any_prog_bank(room->progs->progs) && has_any_prog_bank(parent->progs->progs))
        inherit_prog_bank(&room->progs->progs, parent->progs->progs);

    inherit_index_vars_with_override(&room->index_vars, parent->index_vars);

    room->parent_inherited = true;
}

static void apply_mob_parent_inheritance(MOB_INDEX_DATA *mob, MOB_INDEX_DATA **stack, int depth)
{
    int i;
    MOB_INDEX_DATA *parent;

    if (!mob || mob->parent_inherited)
        return;

    if (!mob->parent)
    {
        mob->parent_inherited = true;
        return;
    }

    if (depth >= 64)
    {
        pbugf(LOG_ERROR, "apply_mob_parent_inheritance: depth exceeded for mobile %s", widevnum_string_mobile(mob, NULL));
        return;
    }

    for (i = 0; i < depth; i++)
    {
        if (stack[i] == mob)
        {
            pbugf(LOG_ERROR, "apply_mob_parent_inheritance: cycle detected at mobile %s", widevnum_string_mobile(mob, NULL));
            mob->parent = NULL;
            return;
        }
    }

    stack[depth] = mob;
    apply_mob_parent_inheritance(mob->parent, stack, depth + 1);

    parent = mob->parent;
    if (!parent || parent == mob)
    {
        mob->parent_inherited = true;
        return;
    }

    if (mob->act[0] == ACT_IS_NPC && mob->act[1] == 0)
    {
        mob->act[0] = parent->act[0];
        mob->act[1] = parent->act[1];
    }

    if (!str_cmp(mob->player_name, "no name")) {
        free_string(mob->player_name);
        mob->player_name = str_dup(parent->player_name);
    }
    if (!str_cmp(mob->short_descr, "(no short description)")) {
        free_string(mob->short_descr);
        mob->short_descr = str_dup(parent->short_descr);
    }
    if (!str_cmp(mob->long_descr, "(no long description)\n\r")) {
        free_string(mob->long_descr);
        mob->long_descr = str_dup(parent->long_descr);
    }
    if (IS_NULLSTR(mob->description)) {
        free_string(mob->description);
        mob->description = str_dup(parent->description);
    }

    if (mob->affected_by[0] == 0 && mob->affected_by[1] == 0)
    {
        mob->affected_by[0] = parent->affected_by[0];
        mob->affected_by[1] = parent->affected_by[1];
    }

    if (mob->alignment == 0)
        mob->alignment = parent->alignment;
    if (mob->level == 0)
        mob->level = parent->level;
    if (mob->hitroll == 0)
        mob->hitroll = parent->hitroll;

    if (mob->hit.number == 0 && mob->hit.size == 0 && mob->hit.bonus == 0)
        mob->hit = parent->hit;
    if (mob->mana.number == 0 && mob->mana.size == 0 && mob->mana.bonus == 0)
        mob->mana = parent->mana;
    if (mob->damage.number == 0 && mob->damage.size == 0 && mob->damage.bonus == 0)
        mob->damage = parent->damage;

    if (mob->ac[AC_PIERCE] == 0 && mob->ac[AC_BASH] == 0
    && mob->ac[AC_SLASH] == 0 && mob->ac[AC_EXOTIC] == 0)
    {
        mob->ac[AC_PIERCE] = parent->ac[AC_PIERCE];
        mob->ac[AC_BASH] = parent->ac[AC_BASH];
        mob->ac[AC_SLASH] = parent->ac[AC_SLASH];
        mob->ac[AC_EXOTIC] = parent->ac[AC_EXOTIC];
    }

    if (mob->dam_type == 0)
        mob->dam_type = parent->dam_type;
    if (mob->off_flags == 0)
        mob->off_flags = parent->off_flags;
    if (mob->imm_flags == 0)
        mob->imm_flags = parent->imm_flags;
    if (mob->res_flags == 0)
        mob->res_flags = parent->res_flags;
    if (mob->vuln_flags == 0)
        mob->vuln_flags = parent->vuln_flags;

    if (mob->start_pos == POS_STANDING)
        mob->start_pos = parent->start_pos;
    if (mob->default_pos == POS_STANDING)
        mob->default_pos = parent->default_pos;

    if (mob->wealth == 0)
        mob->wealth = parent->wealth;
    if (mob->form == 0)
        mob->form = parent->form;
    if (mob->parts == 0)
        mob->parts = parent->parts;
    if (mob->move == 0)
        mob->move = parent->move;
    if (mob->attacks == 0)
        mob->attacks = parent->attacks;

    if (!str_cmp(mob->material, "unknown")) {
        free_string(mob->material);
        mob->material = str_dup(parent->material);
    }
    if (!str_cmp(mob->owner, "(no owner)")) {
        free_string(mob->owner);
        mob->owner = str_dup(parent->owner);
    }
    if (!str_cmp(mob->skeywds, "none")) {
        free_string(mob->skeywds);
        mob->skeywds = str_dup(parent->skeywds);
    }

    if (IS_NULLSTR(mob->list_name)) {
        free_string(mob->list_name);
        mob->list_name = str_dup(parent->list_name);
    }
    if (IS_NULLSTR(mob->list_keywords)) {
        free_string(mob->list_keywords);
        mob->list_keywords = str_dup(parent->list_keywords);
    }
    if (IS_NULLSTR(mob->tags)) {
        free_string(mob->tags);
        mob->tags = str_dup(parent->tags);
    }
    if (IS_NULLSTR(mob->auto_tags)) {
        free_string(mob->auto_tags);
        mob->auto_tags = str_dup(parent->auto_tags);
    }

    if (mob->body_type == BODY_TYPE_NEUTRAL)
        mob->body_type = parent->body_type;
    if (IS_NULLSTR(mob->pronoun_he_she)) {
        free_string(mob->pronoun_he_she);
        mob->pronoun_he_she = str_dup(parent->pronoun_he_she);
    }
    if (IS_NULLSTR(mob->pronoun_him_her)) {
        free_string(mob->pronoun_him_her);
        mob->pronoun_him_her = str_dup(parent->pronoun_him_her);
    }
    if (IS_NULLSTR(mob->pronoun_his_her)) {
        free_string(mob->pronoun_his_her);
        mob->pronoun_his_her = str_dup(parent->pronoun_his_her);
    }
    if (IS_NULLSTR(mob->pronoun_his_hers)) {
        free_string(mob->pronoun_his_hers);
        mob->pronoun_his_hers = str_dup(parent->pronoun_his_hers);
    }
    if (IS_NULLSTR(mob->pronoun_himself_herself)) {
        free_string(mob->pronoun_himself_herself);
        mob->pronoun_himself_herself = str_dup(parent->pronoun_himself_herself);
    }
    if (mob->verb_preference == VERB_FORM_DEFAULT)
        mob->verb_preference = parent->verb_preference;

    if (mob->size == SIZE_MEDIUM)
        mob->size = parent->size;

    if (!mob->progs && parent->progs)
        mob->progs = new_prog_bank();
    if (!has_any_prog_bank(mob->progs) && has_any_prog_bank(parent->progs))
        inherit_prog_bank(&mob->progs, parent->progs);

    inherit_index_vars_with_override(&mob->index_vars, parent->index_vars);

    mob->parent_inherited = true;
}

static void apply_obj_parent_inheritance(OBJ_INDEX_DATA *obj, OBJ_INDEX_DATA **stack, int depth)
{
    int i;
    OBJ_INDEX_DATA *parent;

    if (!obj || obj->parent_inherited)
        return;

    if (!obj->parent)
    {
        obj->parent_inherited = true;
        return;
    }

    if (depth >= 64)
    {
        pbugf(LOG_ERROR, "apply_obj_parent_inheritance: depth exceeded for object %s", widevnum_string_object(obj, NULL));
        return;
    }

    for (i = 0; i < depth; i++)
    {
        if (stack[i] == obj)
        {
            pbugf(LOG_ERROR, "apply_obj_parent_inheritance: cycle detected at object %s", widevnum_string_object(obj, NULL));
            obj->parent = NULL;
            return;
        }
    }

    stack[depth] = obj;
    apply_obj_parent_inheritance(obj->parent, stack, depth + 1);

    parent = obj->parent;
    if (!parent || parent == obj)
    {
        obj->parent_inherited = true;
        return;
    }

    if (obj->item_type == ITEM_TRASH)
        obj->item_type = parent->item_type;

    if (obj->extra[0] == 0 && obj->extra[1] == 0 && obj->extra[2] == 0 && obj->extra[3] == 0)
    {
        obj->extra[0] = parent->extra[0];
        obj->extra[1] = parent->extra[1];
        obj->extra[2] = parent->extra[2];
        obj->extra[3] = parent->extra[3];
    }

    if (obj->wear_flags == 0)
        obj->wear_flags = parent->wear_flags;

    if (!obj->progs && parent->progs)
        obj->progs = new_prog_bank();
    if (!has_any_prog_bank(obj->progs) && has_any_prog_bank(parent->progs))
        inherit_prog_bank(&obj->progs, parent->progs);

    inherit_index_vars_with_override(&obj->index_vars, parent->index_vars);

    obj->parent_inherited = true;
}

void fix_index_inheritance(void)
{
    AREA_DATA *pArea;
    int iHash;
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *mob;
    OBJ_INDEX_DATA *obj;
    ROOM_INDEX_DATA *room_stack[64];
    MOB_INDEX_DATA *mob_stack[64];
    OBJ_INDEX_DATA *obj_stack[64];

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (room = pArea->room_index_hash[iHash]; room != NULL; room = room->next)
            {
                AREA_DATA *parent_area;

                room->parent = NULL;
                room->parent_wnum.pArea = NULL;
                room->parent_wnum.vnum = 0;
                room->parent_inherited = false;

                if (room->parent_load.vnum <= 0)
                    continue;

                parent_area = room->parent_load.auid > 0
                    ? get_area_from_uid(room->parent_load.auid)
                    : room->area;

                if (!parent_area)
                    parent_area = find_area_by_vnum(room->parent_load.vnum, room->area);

                if (!parent_area)
                {
                    pbugf(LOG_ERROR, "fix_index_inheritance: unresolved room parent %ld#%ld for room %s",
                          room->parent_load.auid, room->parent_load.vnum,
                          widevnum_string_room(room, NULL));
                    continue;
                }

                room->parent_wnum.pArea = parent_area;
                room->parent_wnum.vnum = room->parent_load.vnum;
                room->parent = get_room_index(parent_area, room->parent_load.vnum);
            }

            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next)
            {
                AREA_DATA *parent_area;

                mob->parent = NULL;
                mob->parent_wnum.pArea = NULL;
                mob->parent_wnum.vnum = 0;
                mob->parent_inherited = false;

                if (mob->parent_load.vnum <= 0)
                    continue;

                parent_area = mob->parent_load.auid > 0
                    ? get_area_from_uid(mob->parent_load.auid)
                    : mob->area;

                if (!parent_area)
                    parent_area = find_area_by_vnum(mob->parent_load.vnum, mob->area);

                if (!parent_area)
                {
                    pbugf(LOG_ERROR, "fix_index_inheritance: unresolved mobile parent %ld#%ld for mobile %s",
                          mob->parent_load.auid, mob->parent_load.vnum,
                          widevnum_string_mobile(mob, NULL));
                    continue;
                }

                mob->parent_wnum.pArea = parent_area;
                mob->parent_wnum.vnum = mob->parent_load.vnum;
                mob->parent = get_mob_index(parent_area, mob->parent_load.vnum);
            }

            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
            {
                AREA_DATA *parent_area;

                obj->parent = NULL;
                obj->parent_wnum.pArea = NULL;
                obj->parent_wnum.vnum = 0;
                obj->parent_inherited = false;

                if (obj->parent_load.vnum <= 0)
                    continue;

                parent_area = obj->parent_load.auid > 0
                    ? get_area_from_uid(obj->parent_load.auid)
                    : obj->area;

                if (!parent_area)
                    parent_area = find_area_by_vnum(obj->parent_load.vnum, obj->area);

                if (!parent_area)
                {
                    pbugf(LOG_ERROR, "fix_index_inheritance: unresolved object parent %ld#%ld for object %s",
                          obj->parent_load.auid, obj->parent_load.vnum,
                          widevnum_string_object(obj, NULL));
                    continue;
                }

                obj->parent_wnum.pArea = parent_area;
                obj->parent_wnum.vnum = obj->parent_load.vnum;
                obj->parent = get_obj_index(parent_area, obj->parent_load.vnum);
            }
        }
    }

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (room = pArea->room_index_hash[iHash]; room != NULL; room = room->next)
                apply_room_parent_inheritance(room, room_stack, 0);

            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next)
                apply_mob_parent_inheritance(mob, mob_stack, 0);

            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
                apply_obj_parent_inheritance(obj, obj_stack, 0);
        }
    }
}


/*
 * Translate all room exits from virtual to real.
 * Has to be done after all rooms are read in.
 * Check for bad reverse exits.
 */
void fix_rooms(void)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *room;
    EXIT_DATA *pexit;
    int iHash;
    int door;


    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for (room = pArea->room_index_hash[iHash]; room != NULL; room = room->next)
            {
                bool fexit;

                fexit = false;
                for (door = 0; door <= 9; door++)
                {
                    if ((pexit = room->exit[door]) != NULL)
                    {
                        // Find which area owns the destination vnum
                        // Check for stored area UID first (widevnum/cross-area exits)
                        // then fall back to legacy vnum range search
                        AREA_DATA *dest_area = NULL;
                        if (pexit->wilds.area_uid > 0 && pexit->wilds.wilds_uid == 0) {
                            // Explicit area UID stored (widevnum cross-area exit)
                            dest_area = get_area_from_uid(pexit->wilds.area_uid);
                        }
                        if (!dest_area) {
                            // Fall back to legacy vnum range search
                            dest_area = find_area_by_vnum(pexit->u1.vnum, NULL);
                        }
                        if (pexit->u1.vnum <= 0 || !dest_area
                        ||   get_room_index(dest_area, pexit->u1.vnum) == NULL)
                            pexit->u1.to_room = NULL;
                        else
                        {
                           fexit = true;
                            pexit->u1.to_room = get_room_index(dest_area, pexit->u1.vnum);
                        }

                        // Resolve lock key references
                        if (pexit->door.lock.key_load.vnum > 0)
                        {
                            AREA_DATA *key_area = pexit->door.lock.key_load.auid > 0
                                ? get_area_from_uid(pexit->door.lock.key_load.auid) : pArea;
                            pexit->door.lock.key_wnum.pArea = key_area;
                            pexit->door.lock.key_wnum.vnum = pexit->door.lock.key_load.vnum;
                        }

                        if (pexit->door.rs_lock.key_load.vnum > 0)
                        {
                            AREA_DATA *key_area = pexit->door.rs_lock.key_load.auid > 0
                                ? get_area_from_uid(pexit->door.rs_lock.key_load.auid) : pArea;
                            pexit->door.rs_lock.key_wnum.pArea = key_area;
                            pexit->door.rs_lock.key_wnum.vnum = pexit->door.rs_lock.key_load.vnum;
                        }
                    }
                }

                /* only do this for non-wilds rooms*/
                if (!fexit && room->wilds == NULL)
                    SET_BIT(room->room_flag[0],ROOM_NO_MOB);


                /* Fix it so that rooms that have wilderness coords will link to the proper wilderness*/
                if(room->w) room->viewwilds = get_wilds_from_uid(NULL,room->w);
            }
        }
    }
}


/*
 * Translate mobprog vnums pointers to real code
 */
void fix_mobprogs(void)
{
    AREA_DATA *pArea;
    MOB_INDEX_DATA *mob;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next) if(mob->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( mob->progs[slot] ) {
                    iterator_start(&it, mob->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_MPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_MPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_mobprogs: script %s not found for mobile %s (%s)",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_mobile(mob, NULL),
                                mob->short_descr ? mob->short_descr : "(no short description)");
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }
            }
        }
    }
}


void fix_objprogs(void)
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *obj;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next) if(obj->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( obj->progs[slot] ) {
                    iterator_start(&it, obj->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_OPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_OPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_objprogs: script %s not found for object %s",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_object(obj, NULL));
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }
            }
        }
    }
}

void fix_roomprogs(void)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *room;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (room = pArea->room_index_hash[iHash]; room != NULL; room = room->next) if(room->progs->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( room->progs->progs[slot] ) {
                    iterator_start(&it, room->progs->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_RPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_RPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_roomprogs: script %s not found for room %s",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_room(room, NULL));
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }

            }
        }
    }
}

void fix_tokenprogs(void)
{
    AREA_DATA *pArea;
    TOKEN_INDEX_DATA *token;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (token = pArea->token_index_hash[iHash]; token != NULL; token = token->next) if(token->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( token->progs[slot] ) {
                    iterator_start(&it, token->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_TPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_TPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_tokenprogs: script %s not found for token %s",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_token(token, NULL));
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }
            }
        }
    }
}

/*
 * Translate mobprog vnums pointers to real code
 */
void fix_areaprogs(void)
{
    AREA_DATA *pArea;
    PROG_LIST *trigger;
    ITERATOR it;
    int slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) if(pArea->progs->progs) {
        for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( pArea->progs->progs[slot] ) {
            iterator_start(&it, pArea->progs->progs[slot]);
            while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                WNUM script_wnum;
                AREA_DATA *script_area = NULL;
                trigger->script = NULL;
                if (trigger->script_is_widevnum) {
                    script_area = get_area_index(trigger->script_load.auid);
                    if (script_area)
                        trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_APROG);
                }
                if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                    trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_APROG);

                if (!trigger->script) {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                        "fix_areaprogs: script %s not found for area %ld (%s)",
                        trigger->script_is_widevnum
                            ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                            : widevnum_string(pArea, trigger->vnum, NULL),
                        pArea->uid,
                        pArea->name ? pArea->name : "(unnamed area)");
                    exit(1);
                }

                if (!trigger->script_is_widevnum && trigger->script->area) {
                    trigger->script_is_widevnum = true;
                    trigger->script_load.auid = trigger->script->area->uid;
                    trigger->script_load.vnum = trigger->script->vnum;
                }

                // Resolve widevnum trigger phrases
                if (trigger->trig_is_widevnum) {
                    resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                }
            }
            iterator_stop(&it);
        }
    }
}

void fix_instanceprogs(void)
{
    AREA_DATA *pArea;
    BLUEPRINT *blueprint;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (blueprint = pArea->blueprint_hash[iHash]; blueprint != NULL; blueprint = blueprint->next) if(blueprint->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( blueprint->progs[slot] ) {
                    iterator_start(&it, blueprint->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_IPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_IPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_instanceprogs: script %s not found for blueprint %s",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_blueprint(blueprint, NULL));
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }
            }
        }
    }
}


void fix_dungeonprogs(void)
{
    AREA_DATA *pArea;
    DUNGEON_INDEX_DATA *dungeon_index;
    PROG_LIST *trigger;
    ITERATOR it;
    int iHash, slot;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (dungeon_index = pArea->dungeon_index_hash[iHash]; dungeon_index != NULL; dungeon_index = dungeon_index->next) if(dungeon_index->progs) {
                for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( dungeon_index->progs[slot] ) {
                    iterator_start(&it, dungeon_index->progs[slot]);
                    while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                        WNUM script_wnum;
                        AREA_DATA *script_area = NULL;
                        trigger->script = NULL;
                        if (trigger->script_is_widevnum) {
                            script_area = get_area_index(trigger->script_load.auid);
                            if (script_area)
                                trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_DPROG);
                        }
                        if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                            trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_DPROG);

                        if (!trigger->script) {
                            log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                                "Fix_dungeonprogs: script %s not found for dungeon_index %s",
                                trigger->script_is_widevnum
                                    ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                                    : widevnum_string(pArea, trigger->vnum, NULL),
                                widevnum_string_dungeon(dungeon_index, NULL));
                            exit(1);
                        }

                        if (!trigger->script_is_widevnum && trigger->script->area) {
                            trigger->script_is_widevnum = true;
                            trigger->script_load.auid = trigger->script->area->uid;
                            trigger->script_load.vnum = trigger->script->vnum;
                        }

                        // Resolve widevnum trigger phrases
                        if (trigger->trig_is_widevnum) {
                            resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                        }
                    }
                    iterator_stop(&it);
                }
            }
        }
    }
}

void fix_questprogs(void)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    PROG_LIST *trigger;
    ITERATOR it;
    int slot;

    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next) {
        AREA_DATA *pArea = quest_index_v2->area;

        if (!pArea || !quest_index_v2->progs)
            continue;

        for (slot = 0; slot < TRIGSLOT_MAX; slot++) if (quest_index_v2->progs[slot]) {
            iterator_start(&it, quest_index_v2->progs[slot]);
            while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
                WNUM script_wnum;
                AREA_DATA *script_area = NULL;

                trigger->script = NULL;
                if (trigger->script_is_widevnum) {
                    script_area = get_area_index(trigger->script_load.auid);
                    if (script_area)
                        trigger->script = get_script_index(script_area, trigger->script_load.vnum, PRG_QPROG);
                }
                if (!trigger->script && resolve_widevnum(trigger->vnum, pArea, &script_wnum))
                    trigger->script = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_QPROG);

                if (!trigger->script) {
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                        "fix_questprogs: script %s not found for quest index %s",
                        trigger->script_is_widevnum
                            ? widevnum_string(script_area, trigger->script_load.vnum, NULL)
                            : widevnum_string(pArea, trigger->vnum, NULL),
                        widevnum_string(pArea, quest_index_v2->vnum, NULL));
                    exit(1);
                }

                if (!trigger->script_is_widevnum && trigger->script->area) {
                    trigger->script_is_widevnum = true;
                    trigger->script_load.auid = trigger->script->area->uid;
                    trigger->script_load.vnum = trigger->script->vnum;
                }

                if (trigger->trig_is_widevnum) {
                    resolve_wnum_load(&trigger->trig_load, &trigger->trig_wnum, pArea);
                }
            }
            iterator_stop(&it);
        }
    }
}

void fix_dungeon_rooms(void)
{
    AREA_DATA *pArea;
    DUNGEON_INDEX_DATA *dungeon;
    int iHash;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing dungeon room references");

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (dungeon = pArea->dungeon_index_hash[iHash]; dungeon != NULL; dungeon = dungeon->next) {
                
                /* Resolve entry room */
                if (dungeon->entry_ref.load.vnum > 0) {
                    AREA_DATA *target_area = NULL;
                    
                    if (dungeon->entry_ref.load.auid > 0) {
                        target_area = get_area_index(dungeon->entry_ref.load.auid);
                    }
                    
                    if (target_area) {
                        dungeon->entry_room = get_room_index(target_area, dungeon->entry_ref.load.vnum);
                    } else {
                        /* Legacy: search all areas when area_uid is 0 */
                        AREA_DATA *search_area;
                        for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                            dungeon->entry_room = get_room_index(search_area, dungeon->entry_ref.load.vnum);
                            if (dungeon->entry_room)
                                break;
                        }
                    }
                    
                    if (!dungeon->entry_room) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Dungeon '%s' (vnum %ld in %s): entry room %ld#%ld not found",
                            dungeon->name ? dungeon->name : "unnamed", 
                            dungeon->vnum, 
                            pArea->name,
                            dungeon->entry_ref.load.auid, 
                            dungeon->entry_ref.load.vnum);
                    }
                }
                
                /* Resolve exit room */
                if (dungeon->exit_ref.load.vnum > 0) {
                    AREA_DATA *target_area = NULL;
                    
                    if (dungeon->exit_ref.load.auid > 0) {
                        target_area = get_area_index(dungeon->exit_ref.load.auid);
                    }
                    
                    if (target_area) {
                        dungeon->exit_room = get_room_index(target_area, dungeon->exit_ref.load.vnum);
                    } else {
                        /* Legacy: search all areas when area_uid is 0 */
                        AREA_DATA *search_area;
                        for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                            dungeon->exit_room = get_room_index(search_area, dungeon->exit_ref.load.vnum);
                            if (dungeon->exit_room)
                                break;
                        }
                    }
                    
                    if (!dungeon->exit_room) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Dungeon '%s' (vnum %ld in %s): exit room %ld#%ld not found",
                            dungeon->name ? dungeon->name : "unnamed", 
                            dungeon->vnum, 
                            pArea->name,
                            dungeon->exit_ref.load.auid, 
                            dungeon->exit_ref.load.vnum);
                    }
                }
            }
        }
    }
}

/**
 * fix_dungeon_floors - Resolve floor blueprint references at boot time
 *
 * During JSON loading, dungeon floors are stored as WNUM_LOAD pointers
 * (area uid + vnum pairs). This function resolves them to actual BLUEPRINT
 * pointers by looking up each reference. The original list is replaced with
 * a new list containing resolved BLUEPRINT pointers.
 *
 * Must be called after all areas and blueprints are loaded and after
 * fix_blueprint_references() has run.
 */
void fix_dungeon_floors(void)
{
    AREA_DATA *pArea;
    DUNGEON_INDEX_DATA *dungeon;
    int iHash;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing dungeon floor references");

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (dungeon = pArea->dungeon_index_hash[iHash]; dungeon != NULL; dungeon = dungeon->next) {
                if (!dungeon->floors || list_size(dungeon->floors) == 0)
                    continue;

                LLIST *resolved_floors = list_create(false);
                WNUM_LOAD *wload;
                ITERATOR it;

                iterator_start(&it, dungeon->floors);
                while ((wload = (WNUM_LOAD *)iterator_nextdata(&it))) {
                    AREA_DATA *target_area = NULL;
                    BLUEPRINT *bp = NULL;

                    if (wload->auid > 0) {
                        target_area = get_area_index(wload->auid);
                    }

                    if (target_area) {
                        bp = get_blueprint_for_area(target_area, wload->vnum);
                    } else {
                        /* Legacy: search all areas when area_uid is 0 */
                        AREA_DATA *search_area;
                        for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                            bp = get_blueprint_for_area(search_area, wload->vnum);
                            if (bp)
                                break;
                        }
                    }

                    if (bp) {
                        list_appendlink(resolved_floors, bp);
                    } else {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Dungeon '%s' (vnum %ld in %s): floor blueprint %ld#%ld not found",
                            dungeon->name ? dungeon->name : "unnamed",
                            dungeon->vnum,
                            pArea->name,
                            wload->auid, wload->vnum);
                    }
                }
                iterator_stop(&it);

                list_destroy(dungeon->floors);
                dungeon->floors = resolved_floors;
            }
        }
    }
}

/**
 * resolve_wnum_load - Resolve a WNUM_LOAD to a full WNUM with area pointer
 *
 * Converts a WNUM_LOAD (auid + vnum pair from JSON/file storage) into a
 * resolved WNUM by looking up the area from the stored UID. Falls back to
 * the provided reference area when no area UID is stored.
 *
 * @param load      Source WNUM_LOAD containing auid and vnum
 * @param wnum      Destination WNUM to populate with area pointer and vnum
 * @param pRefArea  Fallback area when load->auid is not set
 */
void resolve_wnum_load(WNUM_LOAD *load, WNUM *wnum, AREA_DATA *pRefArea)
{
	if (load->auid > 0)
		wnum->pArea = get_area_from_uid(load->auid);
	else
		wnum->pArea = pRefArea;
	wnum->vnum = load->vnum;
}

/**
 * fix_events - Resolve widevnum references in event definitions after all areas are loaded
 *
 * Iterates all event indices and resolves each roster entry's WNUM_LOAD
 * (auid + vnum) to ensure entry->vnum is set to the local vnum for the
 * resolved area. Falls back to the event's own area when no area UID was
 * stored (legacy data without an explicit area qualifier).
 */
void fix_events(void)
{
    AREA_DATA *pArea;
    int iHash;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resolving event widevnum references");

    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        EVENT_INDEX_DATA *event_index;

        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (event_index = pArea->event_index_hash[iHash]; event_index != NULL; event_index = event_index->next_hash) {
                EVT_ROSTER_ENTRY *entry;

                for (entry = event_index->roster; entry != NULL; entry = entry->next) {
                    AREA_DATA *target_area;

                    if (entry->wnum_load.vnum <= 0)
                        continue;

                    target_area = entry->wnum_load.auid > 0
                        ? get_area_from_uid(entry->wnum_load.auid)
                        : pArea;

                    if (!target_area) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Event '%s' (vnum %ld in %s): roster entry area uid %ld not found, "
                            "falling back to event area",
                            event_index->name ? event_index->name : "unnamed",
                            event_index->vnum, pArea->name,
                            entry->wnum_load.auid);
                        target_area = pArea;
                    }

                    entry->vnum = entry->wnum_load.vnum;
                }
            }
        }
    }
}

/*
 * Fix blueprint room references after all areas are loaded.
 * Resolves WNUM_LOAD unions to actual ROOM_INDEX_DATA pointers.
 */
void fix_blueprint_references(void)
{
    AREA_DATA *pArea;
    BLUEPRINT_SECTION *bs;
    BLUEPRINT *bp;
    BLUEPRINT_LINK *bl;
    BLUEPRINT_SPECIAL_ROOM *special;
    int iHash;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Fixing blueprint room references");

    /* Fix blueprint sections */
    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (bs = pArea->blueprint_section_hash[iHash]; bs != NULL; bs = bs->next) {
                
                /* Resolve rooms_area for room range */
                if (bs->lower_vnum_ref.load.vnum > 0) {
                    AREA_DATA *target_area = get_area_from_uid(bs->lower_vnum_ref.load.auid);
                    
                    if (target_area) {
                        bs->rooms_area = target_area;
                    } else {
                        /* Fallback to section's own area */
                        bs->rooms_area = bs->area;
                    }
                } else {
                    bs->rooms_area = bs->area;
                }
                
                /* Resolve recall room */
                if (bs->recall_ref.load.vnum > 0) {
                    AREA_DATA *target_area = NULL;
                    
                    if (bs->recall_ref.load.auid > 0) {
                        target_area = get_area_index(bs->recall_ref.load.auid);
                    }
                    
                    if (target_area) {
                        bs->recall_room = get_room_index(target_area, bs->recall_ref.load.vnum);
                    } else {
                        /* Legacy: search all areas when area_uid is 0 */
                        AREA_DATA *search_area;
                        for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                            bs->recall_room = get_room_index(search_area, bs->recall_ref.load.vnum);
                            if (bs->recall_room)
                                break;
                        }
                    }
                    
                    if (!bs->recall_room) {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Blueprint section '%s' (vnum %ld in %s): recall room %ld#%ld not found",
                            bs->name ? bs->name : "unnamed", 
                            bs->vnum, 
                            pArea->name,
                            bs->recall_ref.load.auid, 
                            bs->recall_ref.load.vnum);
                    }
                }
                
                /* Resolve link rooms */
                for (bl = bs->links; bl != NULL; bl = bl->next) {
                    if (bl->room_ref.load.vnum > 0) {
                        AREA_DATA *target_area = NULL;
                        
                        if (bl->room_ref.load.auid > 0) {
                            target_area = get_area_index(bl->room_ref.load.auid);
                        }
                        
                        if (target_area) {
                            bl->room = get_room_index(target_area, bl->room_ref.load.vnum);
                        } else {
                            /* Legacy: search all areas when area_uid is 0 */
                            AREA_DATA *search_area;
                            for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                                bl->room = get_room_index(search_area, bl->room_ref.load.vnum);
                                if (bl->room)
                                    break;
                            }
                        }
                        
                        if (!bl->room) {
                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                "Blueprint section '%s' (vnum %ld in %s): link room %ld#%ld not found",
                                bs->name ? bs->name : "unnamed", 
                                bs->vnum, 
                                pArea->name,
                                bl->room_ref.load.auid, 
                                bl->room_ref.load.vnum);
                        }
                    }
                }

                /* Resolve maze template exit template key references */
                if (bs->maze_templates) {
                    ITERATOR mt_it;
                    MAZE_WEIGHTED_ROOM *mwr;
                    iterator_start(&mt_it, bs->maze_templates);
                    while ((mwr = (MAZE_WEIGHTED_ROOM *)iterator_nextdata(&mt_it))) {
                        /* Resolve room pointer */
                        if (mwr->room_ref.load.vnum > 0 && !mwr->room) {
                            AREA_DATA *target = mwr->room_ref.load.auid > 0
                                ? get_area_from_uid(mwr->room_ref.load.auid) : pArea;
                            if (target)
                                mwr->room = get_room_index(target, mwr->room_ref.load.vnum);
                        }

                        /* Resolve exit template key */
                        if (mwr->exit_template.lock.key_load.vnum > 0) {
                            AREA_DATA *key_area = mwr->exit_template.lock.key_load.auid > 0
                                ? get_area_from_uid(mwr->exit_template.lock.key_load.auid) : pArea;
                            mwr->exit_template.lock.key_wnum.pArea = key_area;
                            mwr->exit_template.lock.key_wnum.vnum = mwr->exit_template.lock.key_load.vnum;
                        }
                    }
                    iterator_stop(&mt_it);
                }

                /* Resolve maze fixed room pointers */
                if (bs->maze_fixed_rooms) {
                    ITERATOR mf_it;
                    MAZE_FIXED_ROOM *mfr;
                    iterator_start(&mf_it, bs->maze_fixed_rooms);
                    while ((mfr = (MAZE_FIXED_ROOM *)iterator_nextdata(&mf_it))) {
                        if (mfr->room_ref.load.vnum > 0 && !mfr->room) {
                            AREA_DATA *target = mfr->room_ref.load.auid > 0
                                ? get_area_from_uid(mfr->room_ref.load.auid) : pArea;
                            if (target)
                                mfr->room = get_room_index(target, mfr->room_ref.load.vnum);
                        }
                    }
                    iterator_stop(&mf_it);
                }

                /* Resolve maze map object/mobile references */
                if (bs->map_data) {
                    MAZE_MAP_DATA *mmd = bs->map_data;

                    mmd->obj = NULL;
                    if (mmd->obj_ref.load.vnum > 0) {
                        AREA_DATA *target_area = NULL;

                        if (mmd->obj_ref.load.auid > 0) {
                            target_area = get_area_from_uid(mmd->obj_ref.load.auid);
                        }

                        if (target_area) {
                            mmd->obj = get_obj_index(target_area, mmd->obj_ref.load.vnum);
                        } else {
                            /* Legacy: search all areas when area_uid is 0 */
                            AREA_DATA *search_area;
                            for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                                mmd->obj = get_obj_index(search_area, mmd->obj_ref.load.vnum);
                                if (mmd->obj)
                                    break;
                            }
                        }

                        if (!mmd->obj) {
                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                "Blueprint section '%s' (vnum %ld in %s): maze map object %ld#%ld not found",
                                bs->name ? bs->name : "unnamed",
                                bs->vnum,
                                pArea->name,
                                mmd->obj_ref.load.auid,
                                mmd->obj_ref.load.vnum);
                        }
                    }

                    mmd->mob = NULL;
                    if (mmd->mob_ref.load.vnum > 0) {
                        AREA_DATA *target_area = NULL;

                        if (mmd->mob_ref.load.auid > 0) {
                            target_area = get_area_from_uid(mmd->mob_ref.load.auid);
                        }

                        if (target_area) {
                            mmd->mob = get_mob_index(target_area, mmd->mob_ref.load.vnum);
                        } else {
                            /* Legacy: search all areas when area_uid is 0 */
                            AREA_DATA *search_area;
                            for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                                mmd->mob = get_mob_index(search_area, mmd->mob_ref.load.vnum);
                                if (mmd->mob)
                                    break;
                            }
                        }

                        if (!mmd->mob) {
                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                "Blueprint section '%s' (vnum %ld in %s): maze map carrier mob %ld#%ld not found",
                                bs->name ? bs->name : "unnamed",
                                bs->vnum,
                                pArea->name,
                                mmd->mob_ref.load.auid,
                                mmd->mob_ref.load.vnum);
                        }
                    }
                }
            }
        }
    }
    
    /* Fix blueprint special rooms */
    for (pArea = area_first; pArea != NULL; pArea = pArea->next) {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
            for (bp = pArea->blueprint_hash[iHash]; bp != NULL; bp = bp->next) {
                ITERATOR it;
                
                // Resolve section references
                iterator_start(&it, bp->sections);
                BLUEPRINT_SECTION_REF *section_ref;
                while((section_ref = (BLUEPRINT_SECTION_REF *)iterator_nextdata(&it))) {
                    if (section_ref->section_ref.load.vnum > 0) {
                        AREA_DATA *target_area = NULL;
                        
                        if (section_ref->section_ref.load.auid > 0) {
                            target_area = get_area_index(section_ref->section_ref.load.auid);
                        }
                        
                        if (target_area) {
                            section_ref->section = get_blueprint_section_for_area(target_area, section_ref->section_ref.load.vnum);
                        } else {
                            /* Legacy: search all areas when area_uid is 0 */
                            AREA_DATA *search_area;
                            for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                                section_ref->section = get_blueprint_section_for_area(search_area, section_ref->section_ref.load.vnum);
                                if (section_ref->section)
                                    break;
                            }
                        }
                        
                        if (!section_ref->section) {
                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                "Blueprint '%s' (vnum %ld in %s): section %ld#%ld not found",
                                bp->name ? bp->name : "unnamed", 
                                bp->vnum, 
                                pArea->name,
                                section_ref->section_ref.load.auid, 
                                section_ref->section_ref.load.vnum);
                        }
                    }
                }
                iterator_stop(&it);
                
                // Resolve special room references
                iterator_start(&it, bp->special_rooms);
                while((special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&it))) {
                    
                    if (special->room_ref.load.vnum > 0) {
                        AREA_DATA *target_area = NULL;
                        
                        if (special->room_ref.load.auid > 0) {
                            target_area = get_area_index(special->room_ref.load.auid);
                        }
                        
                        if (target_area) {
                            special->room = get_room_index(target_area, special->room_ref.load.vnum);
                        } else {
                            /* Legacy: search all areas when area_uid is 0 */
                            AREA_DATA *search_area;
                            for (search_area = area_first; search_area != NULL; search_area = search_area->next) {
                                special->room = get_room_index(search_area, special->room_ref.load.vnum);
                                if (special->room)
                                    break;
                            }
                        }
                        
                        if (!special->room) {
                            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                "Blueprint '%s' (vnum %ld in %s): special room '%s' %ld#%ld not found",
                                bp->name ? bp->name : "unnamed", 
                                bp->vnum, 
                                pArea->name,
                                special->name ? special->name : "unnamed",
                                special->room_ref.load.auid, 
                                special->room_ref.load.vnum);
                        }
                    }
                }
                iterator_stop(&it);
            }
        }
    }
    
    /* Fix ship index references */
    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        int iHash;
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            SHIP_INDEX_DATA *ship;
            for (ship = pArea->ship_index_hash[iHash]; ship != NULL; ship = ship->next)
            {
                /* Resolve blueprint reference using WNUM_LOAD */
                if (ship->blueprint_ref.load.vnum > 0 && !ship->blueprint)
                {
                    AREA_DATA *target_area = get_area_from_uid(ship->blueprint_ref.load.auid);
                    
                    if (target_area) {
                        ship->blueprint = get_blueprint_for_area(target_area, ship->blueprint_ref.load.vnum);
                    }
                    
                    /* Fall back to global search if not found */
                    if (!ship->blueprint) {
                        ship->blueprint = get_blueprint(ship->blueprint_ref.load.vnum);
                    }
                    
                    if (!ship->blueprint)
                    {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Ship '%s' (vnum %ld in %s): blueprint %lu#%ld not found",
                            ship->name ? ship->name : "unnamed",
                            ship->vnum,
                            pArea->name,
                            ship->blueprint_ref.load.auid,
                            ship->blueprint_ref.load.vnum);
                    }
                }
                
                /* Resolve ship_object reference using WNUM_LOAD */
                if (ship->ship_object_ref.load.vnum > 0 && !ship->ship_object)
                {
                    AREA_DATA *target_area = get_area_from_uid(ship->ship_object_ref.load.auid);
                    
                    if (target_area) {
                        ship->ship_object = get_obj_index(target_area, ship->ship_object_ref.load.vnum);
                    }
                    
                    /* If not found in target area, search all areas (legacy support) */
                    if (!ship->ship_object)
                    {
                        AREA_DATA *search_area;
                        for (search_area = area_first; search_area != NULL; search_area = search_area->next)
                        {
                            ship->ship_object = get_obj_index(search_area, ship->ship_object_ref.load.vnum);
                            if (ship->ship_object)
                                break;
                        }
                    }
                    
                    if (!ship->ship_object)
                    {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Ship '%s' (vnum %ld in %s): ship_object %lu#%ld not found",
                            ship->name ? ship->name : "unnamed",
                            ship->vnum,
                            pArea->name,
                            ship->ship_object_ref.load.auid,
                            ship->ship_object_ref.load.vnum);
                    }
                }

                /* Resolve captain mob reference using WNUM_LOAD */
                if (ship->captain_ref.load.vnum > 0 && !ship->captain)
                {
                    AREA_DATA *target_area = get_area_from_uid(ship->captain_ref.load.auid);

                    if (target_area)
                        ship->captain = get_mob_index(target_area, ship->captain_ref.load.vnum);

                    if (!ship->captain)
                        ship->captain = get_mob_index_global(ship->captain_ref.load.vnum);

                    if (!ship->captain)
                    {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Ship '%s' (vnum %ld in %s): captain mob %lu#%ld not found",
                            ship->name ? ship->name : "unnamed",
                            ship->vnum,
                            pArea->name,
                            ship->captain_ref.load.auid,
                            ship->captain_ref.load.vnum);
                    }
                }

                /* Resolve crew mob definition references */
                if (ship->crew_defs && list_size(ship->crew_defs) > 0)
                {
                    ITERATOR cd_it;
                    SHIP_CREW_DEF *cd;
                    iterator_start(&cd_it, ship->crew_defs);
                    while ((cd = (SHIP_CREW_DEF *)iterator_nextdata(&cd_it)) != NULL)
                    {
                        if (cd->mob_ref.load.vnum > 0 && !cd->mob)
                        {
                            AREA_DATA *target_area = get_area_from_uid(cd->mob_ref.load.auid);

                            if (target_area)
                                cd->mob = get_mob_index(target_area, cd->mob_ref.load.vnum);

                            if (!cd->mob)
                                cd->mob = get_mob_index_global(cd->mob_ref.load.vnum);

                            if (!cd->mob)
                            {
                                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                    "Ship '%s' (vnum %ld in %s): crew mob %lu#%ld not found",
                                    ship->name ? ship->name : "unnamed",
                                    ship->vnum,
                                    pArea->name,
                                    cd->mob_ref.load.auid,
                                    cd->mob_ref.load.vnum);
                            }
                        }
                    }
                    iterator_stop(&cd_it);
                }

                /* Resolve faction reputation reference */
                if (ship->faction_ref.load.vnum > 0 && !ship->faction)
                {
                    AREA_DATA *target_area = get_area_from_uid(ship->faction_ref.load.auid);

                    if (target_area)
                        ship->faction = get_reputation_index(target_area, ship->faction_ref.load.vnum);

                    if (!ship->faction)
                    {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                            "Ship '%s' (vnum %ld in %s): faction reputation %lu#%ld not found",
                            ship->name ? ship->name : "unnamed",
                            ship->vnum,
                            pArea->name,
                            ship->faction_ref.load.auid,
                            ship->faction_ref.load.vnum);
                    }
                }

                /* Resolve schedule stop room references */
                if (ship->schedule_stops && list_size(ship->schedule_stops) > 0) {
                    ITERATOR sch_it;
                    SHIP_SCHEDULE_STOP *stop;
                    iterator_start(&sch_it, ship->schedule_stops);
                    while ((stop = (SHIP_SCHEDULE_STOP *)iterator_nextdata(&sch_it))) {
                        if (stop->location_type == STOP_LOC_ROOM
                            && stop->room_ref.load.vnum > 0
                            && !stop->dock_room) {
                            AREA_DATA *stop_area = get_area_from_uid(stop->room_ref.load.auid);
                            if (stop_area)
                                stop->dock_room = get_room_index(stop_area, stop->room_ref.load.vnum);
                            if (!stop->dock_room) {
                                log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                                    "Ship '%s' (vnum %ld): schedule stop '%s' room %ld#%ld not found",
                                    ship->name ? ship->name : "unnamed",
                                    ship->vnum,
                                    stop->name ? stop->name : "unnamed",
                                    stop->room_ref.load.auid,
                                    stop->room_ref.load.vnum);
                            }
                        }
                    }
                    iterator_stop(&sch_it);
                }
                
            }
        }
    }
}


void reset_wilds(WILDS_DATA *pWilds)
{
    WILDS_VLINK *pVLink;
    OBJ_DATA *obj;

    if (pWilds->pVLink)
    {
        for (pVLink = pWilds->pVLink;pVLink;pVLink = pVLink->next)
        {
            if (IS_SET(pVLink->current_linkage, VLINK_PORTAL))
            {
          OBJ_INDEX_DATA *pObjIndex = get_reserved_obj_index("obj_portal_abyss");
          if (pObjIndex)
          {
              obj = create_object(pObjIndex, 0, true);
              obj_to_vroom(obj, pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y);
          }
          else
          {
              log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "generate_wilds_objs: Cannot find abyss portal object.");
          }
            }
        }
    }

    return;
}


void reset_area(AREA_DATA *pArea)
{
    ROOM_INDEX_DATA *pRoom;
    ITERATOR it;

/*  VIZZWILDS - disabled this legacy hack - now handled by vlinks!*/
/*
    if (!str_cmp(pArea->name , "Netherworld"))
    {
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *temp_room;
    bool found = false;

    temp_room = get_reserved_room_index("ROOM_VNUM_ABYSS_GATE");
    for (obj = temp_room->contents; obj != NULL; obj = obj->next_content)
    {
        if (obj->pIndexData == get_reserved_obj_index("obj_portal_abyss"))
        {
        found = true;
        break;
        }
    }

    if (!found)
    {
        OBJ_INDEX_DATA *pObjIndex = get_reserved_obj_index("obj_portal_abyss");
        ROOM_INDEX_DATA *pRoomIndex = get_reserved_room_index("ROOM_VNUM_ABYSS_GATE");
        if (pObjIndex && pRoomIndex)
        {
            obj = create_object(pObjIndex, 0, true);
            obj_to_room(obj, pRoomIndex);
        }
        else
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "load_area_db: Cannot create abyss portal (reserved obj_portal_abyss or ROOM_VNUM_ABYSS_GATE not found).");
        }
    }

  }
*/
    /* Global quest!! resets mobs, if GQ is on.*/
    if (global)
    global_reset();

/*
    if (!str_cmp(pArea->name , "Wilderness"))
    {
    for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
    {
        if ((pRoom = get_room_index(vnum)))
        {
        if (pRoom->parent == 5000001) Silverfern forest
        {
            Butterfly
            chance_create_mob(pRoom, get_mob_index(100001), 5);
        }
        if (pRoom->parent == 5000002)  Wharf
        {
        seagull
            chance_create_mob(pRoom, get_mob_index(100002), 5);
        }
        if (pRoom->parent == 5000003)  Paved
        {
            Paved Road
            chance_create_mob(pRoom, get_mob_index(100007), 2);
            chance_create_mob(pRoom, get_mob_index(100012), 2);
            chance_create_mob(pRoom, get_mob_index(100013), 2);
            chance_create_mob(pRoom, get_mob_index(100014), 2);
            chance_create_mob(pRoom, get_mob_index(100015), 2);
            chance_create_mob(pRoom, get_mob_index(100016), 2);
            chance_create_mob(pRoom, get_mob_index(100017), 2);
        }
        if (pRoom->parent == 5000004) Sandy Beach
        {
            chance_create_mob(pRoom, get_mob_index(100002), 5);
        }
        if (pRoom->parent == 5000007) Devil's Den Forest
        {
            chance_create_mob(pRoom, get_mob_index(100003), 5);
            chance_create_mob(pRoom, get_mob_index(100008), 5);
            chance_create_mob(pRoom, get_mob_index(100009), 5);
        }
        if (pRoom->parent == 5000008) Grassy Field
        {
            chance_create_mob(pRoom, get_mob_index(100003), 2);
            chance_create_mob(pRoom, get_mob_index(100007), 2);
            chance_create_mob(pRoom, get_mob_index(100001), 2);
        }
        if (pRoom->parent == 5000011)  Mountains
        {
            chance_create_mob(pRoom, get_mob_index(100004), 5);
            chance_create_mob(pRoom, get_mob_index(100005), 5);
            chance_create_mob(pRoom, get_mob_index(100006), 5);
        }
        }
    }
    }
    else
    */

    p_percent2_trigger(pArea, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RESET, NULL);

    iterator_start(&it, pArea->room_list);
    while ((pRoom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) != NULL)
        reset_room(pRoom, false);
    iterator_stop(&it);
}

void room_update(ROOM_INDEX_DATA *room)
{
    //char   buf[MAX_STRING_LENGTH];

    TOKEN_DATA *token;
    ITERATOR it;
    if (room->progs->delay > 0 && --room->progs->delay <= 0)
        p_percent_trigger(NULL, NULL, room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_DELAY, NULL);

    p_percent_trigger(NULL, NULL, room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

    // Prereckoning
    if (pre_reckoning > 0 && reckoning_timer > 0)
        p_percent_trigger(NULL, NULL, room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECKONING, NULL);

    // Reckoning
    if (!pre_reckoning && reckoning_timer > 0)
        p_percent_trigger(NULL, NULL, room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECKONING, NULL);

    // Update tokens on room. Remove the one for which the timer has run out.
    iterator_start(&it, room->ltokens);
    while((token=(TOKEN_DATA*)iterator_nextdata(&it)))
    {
        if (IS_SET(token->flags, TOKEN_REVERSETIMER))
        {
            ++token->timer;
        }
        else if (token->timer > 0)
        {
            --token->timer;
            if (token->timer <= 0) {
                if( room->source )
                    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "room update: token %s(%ld) clone room %s(%ld, %1d:%1d) was extracted because of timer",
                        token->name, token->pIndexData->vnum, room->name, room->vnum, (int)room->id[0], (int)room->id[1]);
                else if( room->wilds )
                    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "room update: token %s(%ld) wilds room %s(%ld, %ld, %ld) was extracted because of timer",
                        token->name, token->pIndexData->vnum, room->name, room->wilds->uid, room->x, room->y);
                else
                    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "room update: token %s(%ld) room %s(%ld) was extracted because of timer",
                        token->name, token->pIndexData->vnum, room->name, room->vnum);
                p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_EXPIRE, NULL);
                token_from_room(token);
                free_token(token);
            }
        }
    }
    iterator_stop(&it);
}

void area_update(bool fBoot)
{
    ITERATOR it;
    AREA_DATA *pArea;
    char buf[MAX_STRING_LENGTH];
    int hash;
    ROOM_INDEX_DATA *room;
    /* VIZZWILDS */
    WILDS_DATA *pWilds;

    /* Loop through every area*/
    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        /* Increment the area's age*/
        pArea->age++;

        p_percent2_trigger(pArea, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

        // Prereckoning
        if (pre_reckoning > 0 && reckoning_timer > 0)
            p_percent2_trigger(pArea, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECKONING, NULL);

        // Reckoning
        if (!pre_reckoning && reckoning_timer > 0)
            p_percent2_trigger(pArea, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RECKONING, NULL);


        /* Check area's age and reset if necessary*/
        if (fBoot || (pArea->age >= pArea->repop || (pArea->repop == 0 && pArea->age > 15) || pArea->age >= 120))
        {
            plogf(LOG_INFO, "Resetting area %s.", pArea->name);
            reset_area(pArea);
            sprintf(buf,"%s has just been reset.",pArea->name);
            emit_db_wiz_event(buf, buf, WIZ_RESETS, "area_reset", LOG_INFO);
            pArea->age = 0;

            if (pArea->nplayer == 0)
                pArea->empty = true;

            /* If the area contains wilds sectors*/
            if (pArea->wilds)
            {
                /* Loop through all wilds in this area*/
                for(pWilds = pArea->wilds;pWilds;pWilds = pWilds->next)
                {
                    if (fBoot)
                        continue;

                    pWilds->age++;

                    if (pWilds->age >= pWilds->repop)
                    {
                        plogf(LOG_INFO, "Resetting wilds uid %ld, '%s'...", pWilds->uid, pWilds->name);
                        pWilds->age = 0;

                        // This.. doesn't do anything?
                    }
                }
            }
        }
    }

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (hash = 0; hash < MAX_KEY_HASH; hash++)
        {
            for (room = pArea->room_index_hash[hash]; room; room = room->next)
            {
                // Persistant rooms are handled separately!
                if (!room->persist && can_room_update(room))
                {
                    room_update(room);
                }
            }
        }
    }

    // Update persistant rooms at all times
    iterator_start(&it, persist_rooms);
    while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) ))
    {
        room_update(room);
    }
    iterator_stop(&it);

    instance_update();
}

void migrate_shopkeeper_resets(AREA_DATA *area)
{
    if( area->version_area < VERSION_AREA_003 )
    {
        ROOM_INDEX_DATA *room;
        OBJ_INDEX_DATA *obj;
        // Migrate all shopkeeper related resets over to shop stock
        ITERATOR it;
        iterator_start(&it, area->room_list);
        while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) != NULL)
        {
            RESET_DATA *curr, *prev, *next;
            MOB_INDEX_DATA *last_mob = NULL;

            for(prev = NULL, curr = room->reset_first; curr; curr = next)
            {
                next = curr->next;

                switch(curr->command)
                {
                case 'M':
                    last_mob = get_mob_index(room->area, curr->arg1.wnum.vnum);
                    break;

                case 'G':
                    // Mob must be a shopkeeper, the object must exist and the object must not be money

                    if( (last_mob != NULL) &&
                        (last_mob->pShop != NULL) &&
                        ((obj = get_obj_index(room->area, curr->arg1.wnum.vnum)) != NULL) &&
                        (obj->item_type != ITEM_MONEY))
                    {

                        SHOP_STOCK_DATA *stock = new_shop_stock();
                        if(!stock) {
                            prev = curr;
                            break;	// SHOULD ABORT?
                        }

                        // Generate stock entry
                        stock->type = STOCK_OBJECT;
                        stock->entity.wnum.pArea = NULL;
                        stock->entity.wnum.vnum = obj->vnum;
                        stock->silver = obj->cost;
                        stock->discount = last_mob->pShop->discount;

                        stock->next = last_mob->pShop->stock;
                        last_mob->pShop->stock = stock;

                        // Prune this RESET

                        if(prev != NULL)
                            prev->next = next;
                        else
                            room->reset_first = next;

                        if(next == NULL)
                            room->reset_last = prev;

                        free_reset_data(curr);

                        SET_BIT(area->area_flags, AREA_CHANGED);
                        continue;
                    }
                    break;
                }

                // Only advance if nothing happened
                prev = curr;
            }
        }
        iterator_stop(&it);

        // If nothing else has changed besides being one version behind 003,
        //	move version to 003 automatically so it doesn't need to save
        if( (area->version_area == VERSION_AREA_002) && !IS_SET(area->area_flags, AREA_CHANGED) )
            area->version_area = VERSION_AREA_003;
    }
}


void reset_room(ROOM_INDEX_DATA *pRoom, bool force)
{
    RESET_DATA  *pReset;
    CHAR_DATA   *pMob;
    CHAR_DATA	*mob;
    OBJ_DATA    *pObj;
    CHAR_DATA   *LastMob = NULL;
    OBJ_DATA    *LastObj = NULL;
    int iExit;
    int level = 0;
    bool last;
    bool instanced = false;

    // Invalid room or the room is persistant (and not forced)
    if (!pRoom || (pRoom->persist && !force))
        return;

    pMob = NULL;
    last = false;

    /*
     * Forced room resets (builder reload/resetroom) should restore room
     * variables back to index defaults before running resets.
     */
    if (force)
    {
        variable_freelist(&pRoom->progs->vars);
        variable_copylist(&pRoom->index_vars, &pRoom->progs->vars, false);
        variables_resolve_rsg_bindings(&pRoom->progs->vars);
        reset_room_expand_text(pRoom);
    }

    // Reset all of the mutable things
    pRoom->room_flag[0] = pRoom->rs_room_flag[0];
    pRoom->room_flag[1] = pRoom->rs_room_flag[1];
    pRoom->heal_rate = pRoom->rs_heal_rate;
    pRoom->mana_rate = pRoom->rs_mana_rate;
    pRoom->move_rate = pRoom->rs_move_rate;
    room_set_sector_type(pRoom, room_rs_sector_type(pRoom));
    if (location_isset(&pRoom->recall))
    {
        location_clear(&pRoom->recall);
    }
    if (rs_location_isset(&pRoom->rs_recall))
    {
        pRoom->recall.wuid = pRoom->rs_recall.wuid;
        pRoom->recall.id[0] = pRoom->rs_recall.id[0];
        pRoom->recall.id[1] = pRoom->rs_recall.id[1];
        pRoom->recall.id[2] = pRoom->rs_recall.id[2];
    }

    for (iExit = 0;  iExit < MAX_DIR;  iExit++)
    {
        EXIT_DATA *pExit;
        if ((pExit = pRoom->exit[iExit]))
        {
            pExit->exit_info = pExit->rs_flags;
            pExit->door.lock = pExit->door.rs_lock;
            if ((pExit->u1.to_room != NULL) &&
                ((pExit = pExit->u1.to_room->exit[rev_dir[iExit]])))
            {
                /* nail the other side */
                pExit->exit_info = pExit->rs_flags;
                pExit->door.lock = pExit->door.rs_lock;
            }
        }
    }

    for (pReset = pRoom->reset_first; pReset != NULL; pReset = pReset->next)
    {
        MOB_INDEX_DATA  *pMobIndex;
        OBJ_INDEX_DATA  *pObjIndex;
        OBJ_INDEX_DATA  *pObjToIndex;
        ROOM_INDEX_DATA *pRoomIndex;
        //char buf[MAX_STRING_LENGTH];
        int count,limit=0;

        instanced = false;

        switch (pReset->command)
        {
        default:
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: bad command %c.", pReset->command);
            audit_log_reset_error(pRoom, pReset, formatf("Reset_room: bad command %c.", pReset->command));
            break;

        case 'M':
        {
            AREA_DATA *mob_area = pReset->arg1.wnum.pArea;
            pMobIndex = get_mob_index(mob_area ? mob_area : pRoom->area, pReset->arg1.wnum.vnum);
            if (!pMobIndex && !mob_area)
                pMobIndex = get_mob_index_global(pReset->arg1.wnum.vnum);

            if (!pMobIndex)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'M': bad vnum %ld.", pReset->arg1.wnum.vnum);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'M': bad vnum %ld.", pReset->arg1.wnum.vnum));
                continue;
            }

            if ((pRoomIndex = get_room_index(pRoom->area, pReset->arg3.value)) == NULL)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_area: 'R': bad vnum %ld.", pReset->arg3.value);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_area: 'R': bad vnum %ld.", pReset->arg3.value));
                continue;
            }

            // When the room is in an instance
            if( pRoom->instance_section != NULL )
            {
                count = instance_count_mob(pRoom->instance_section->instance, pMobIndex);

                char buf[MSL];
                sprintf(buf, "reset_room(M): %ld -> %ld = %d / %ld", pRoom->vnum, pMobIndex->vnum, count, pReset->arg2);
                emit_db_wiz_event(buf, buf, WIZ_TESTING, "reset_room_mob_cap", LOG_DEBUG);

                if( count >= pReset->arg2 )
                {
                    last = false;
                    break;
                }

                instanced = true;
            }
            else if (pMobIndex->count >= pReset->arg2)
            {
                last = false;
                break;
            }

            count = 0;
            for (mob = pRoom->people; mob != NULL; mob = mob->next_in_room)
            {
                if (mob->pIndexData == pMobIndex && !IS_SET(mob->act[0], ACT_ANIMATED))
                {
                    count++;
                    if (count >= pReset->arg4)
                    {
                        last = false;
                        break;
                    }
                }
            }

            if (count >= pReset->arg4)
                break;

            pMob = create_mobile(pMobIndex, false);
            if( instanced )
                SET_BIT(pMob->act[1], ACT2_INSTANCE_MOB);

            /*
            * Some more hard coding.
            */
            if (room_is_dark(pRoom))
                SET_BIT(pMob->affected_by[0], AFF_INFRARED);

            char_to_room(pMob, pRoom);
            pMob->home_room = pRoom;

            LastMob = pMob;
            level  = URANGE(0, pMob->level - 2, LEVEL_HERO - 1); /* -1 ROM */
            last = true;
            objRepop = true;
            p_percent_trigger(pMob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
            break;
        }

        case 'O':
        {
            AREA_DATA *obj_area = pReset->arg1.wnum.pArea;
            pObjIndex = get_obj_index(obj_area ? obj_area : pRoom->area, pReset->arg1.wnum.vnum);
            if (!pObjIndex && !obj_area)
                pObjIndex = get_obj_index_global(pReset->arg1.wnum.vnum);

            if (!pObjIndex)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'O' 1 : bad vnum %ld (args: %ld %ld %ld %ld)", pReset->arg1.wnum.vnum, pReset->arg1.wnum.vnum, pReset->arg2, pReset->arg3.value, pReset->arg4);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'O' 1 : bad vnum %ld (args: %ld %ld %ld %ld)", pReset->arg1.wnum.vnum, pReset->arg1.wnum.vnum, pReset->arg2, pReset->arg3.value, pReset->arg4));
                continue;
            }

            if (!(pRoomIndex = get_room_index(pRoom->area, pReset->arg3.value)))
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'O' 2 : bad vnum %ld (args: %ld %ld %ld %ld)", pReset->arg3.value, pReset->arg1.wnum.vnum, pReset->arg2, pReset->arg3.value, pReset->arg4);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'O' 2 : bad vnum %ld (args: %ld %ld %ld %ld)", pReset->arg3.value, pReset->arg1.wnum.vnum, pReset->arg2, pReset->arg3.value, pReset->arg4));
                continue;
            }

            int players;

            if( IS_VALID(pRoom->instance_section) )
            {
                INSTANCE *instance = pRoom->instance_section->instance;

                if( IS_VALID(instance) )
                {
                    // Dungeons must check across all floors of the dungeon
                    if( IS_VALID(instance->dungeon) )
                        players = list_size(instance->dungeon->players);
                    else
                        players = list_size(instance->players);

                }
                else
                    players = 0;

                instanced = true;
            }
            else
                players = pRoom->area->nplayer;	// In a normal area


            if (players > 0 || count_obj_list(pObjIndex, pRoom->contents) > 0)
            {
                last = false;
                break;
            }

            pObj = create_object(pObjIndex, UMIN(number_fuzzy(level), LEVEL_HERO -1) , true);
            if( instanced )
                SET_BIT(pObj->extra[2], ITEM_INSTANCE_OBJ);
            pObj->cost = 0;
            objRepop = true;
            obj_to_room(pObj, pRoom);
            last = true;
            break;
        }

        case 'P':
        {
            AREA_DATA *obj_area = pReset->arg1.wnum.pArea;
            AREA_DATA *container_area = pReset->arg3.wnum.pArea;

            pObjIndex = get_obj_index(obj_area ? obj_area : pRoom->area, pReset->arg1.wnum.vnum);
            if (!pObjIndex && !obj_area)
                pObjIndex = get_obj_index_global(pReset->arg1.wnum.vnum);

            if (!pObjIndex)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'P': bad vnum %ld.", pReset->arg1.wnum.vnum);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'P': bad vnum %ld.", pReset->arg1.wnum.vnum));
                continue;
            }

            pObjToIndex = get_obj_index(container_area ? container_area : pRoom->area, pReset->arg3.wnum.vnum);
            if (!pObjToIndex && !container_area)
                pObjToIndex = get_obj_index_global(pReset->arg3.wnum.vnum);

            if (!pObjToIndex)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'P': bad vnum %ld.", pReset->arg3.wnum.vnum);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'P': bad vnum %ld.", pReset->arg3.wnum.vnum));
                continue;
            }

            if (pReset->arg2 > 50) /* old format */
                limit = 6;
            else if (pReset->arg2 == -1) /* no limit */
                limit = 999;
            else
                limit = pReset->arg2;

            {
                int obj_count;
                int players;

                if (pRoom->instance_section != NULL)
                {
                    INSTANCE *instance = pRoom->instance_section->instance;
                    obj_count = instance_count_obj(instance, pObjIndex);

                    if (IS_VALID(instance->dungeon))
                        players = list_size(instance->dungeon->players);
                    else
                        players = list_size(instance->players);

                    instanced = true;
                }
                else
                {
                    obj_count = pObjIndex->count;
                    players = pRoom->area->nplayer;
                }

                if (players > 0 ||
                    (LastObj = get_obj_type(pObjToIndex, pRoom)) == NULL ||
                    (LastObj->in_room == NULL && !last) ||
                    (obj_count >= limit) ||
                    (count = count_obj_list(pObjIndex, LastObj->contains)) > pReset->arg4)
                {
                    last = false;
                    break;
                }
            }
            /* lastObj->level  -  ROM */

            while (count < pReset->arg4)
            {
                pObj = create_object(pObjIndex, number_fuzzy(LastObj->level), true);
                if (instanced)
                    SET_BIT(pObj->extra[2], ITEM_INSTANCE_OBJ);
                obj_to_obj(pObj, LastObj);
                count++;
                if (instanced)
                {
                    if (instance_count_obj(pRoom->instance_section->instance, pObjIndex) >= limit)
                        break;
                }
                else if (pObjIndex->count >= limit)
                    break;
            }

            /* fix object lock state! */
            obj_set_legacy_value_slot(LastObj, 1,
                legacy_obj_index_value_get(LastObj->pIndexData, 1));
            last = true;
            break;
        }

        case 'G':
        case 'E':
        {
            AREA_DATA *obj_area = pReset->arg1.wnum.pArea;
            pObjIndex = get_obj_index(obj_area ? obj_area : pRoom->area, pReset->arg1.wnum.vnum);
            if (!pObjIndex && !obj_area)
                pObjIndex = get_obj_index_global(pReset->arg1.wnum.vnum);

            if (!pObjIndex)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'E' or 'G': bad vnum %ld.", pReset->arg1.wnum.vnum);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'E' or 'G': bad vnum %ld.", pReset->arg1.wnum.vnum));
                continue;
            }

            if (!last)
                break;

            if (!LastMob)
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'E' or 'G': null mob for vnum %ld.", pReset->arg1.wnum.vnum);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'E' or 'G': null mob for vnum %ld.", pReset->arg1.wnum.vnum));
                last = false;
                break;
            }

            {
                int limit;
                int obj_count;

                if (pReset->arg2 > 50)
                    limit = 6;
                else if (pReset->arg2 == -1 || pReset->arg2 == 0)
                    limit = 999;
                else
                    limit = pReset->arg2;

                if (pRoom->instance_section != NULL)
                {
                    obj_count = instance_count_obj(pRoom->instance_section->instance, pObjIndex);
                    instanced = true;
                }
                else
                    obj_count = pObjIndex->count;

                if (obj_count < limit || number_range(0,4) == 0)
                {
                    pObj = create_object(pObjIndex, UMIN(number_fuzzy(level), LEVEL_HERO - 1), true);
                    if (instanced)
                        SET_BIT(pObj->extra[2], ITEM_INSTANCE_OBJ);
                }
                else
                    break;
            }

            objRepop = true;
            obj_to_char(pObj, LastMob);
            if (pReset->command == 'E')
                equip_char(LastMob, pObj, pReset->arg3.value);
            last = true;
            break;
        }

        case 'D':
            break;

        case 'R':
            if (!(pRoomIndex = get_room_index(pRoom->area, pReset->arg1.value)))
            {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Reset_room: 'R': bad vnum %ld.", pReset->arg1.value);
                audit_log_reset_error(pRoom, pReset, formatf("Reset_room: 'R': bad vnum %ld.", pReset->arg1.value));
                continue;
            }

            {
                EXIT_DATA *pExit;
                int d0;
                int d1;

                for (d0 = 0; d0 < pReset->arg2 - 1; d0++)
                {
                    d1                   = number_range(d0, pReset->arg2-1);
                    pExit                = pRoomIndex->exit[d0];
                    pRoomIndex->exit[d0] = pRoomIndex->exit[d1];
                    pRoomIndex->exit[d1] = pExit;
                }
            }
            break;
        }
    }

    p_percent_trigger(NULL, NULL, pRoom, NULL, NULL, NULL, NULL,NULL, NULL, TRIG_RESET, NULL);
}


void chance_create_mob(ROOM_INDEX_DATA *pRoom, MOB_INDEX_DATA *pMobIndex, int chance)
{
    CHAR_DATA *pMobile;
    OBJ_DATA *obj = NULL;

    if (!pMobIndex)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "chance_create_mob: NULL pMobIndex.");
        return;
    }

    /* don't do for now
    if (pMobIndex == get_reserved_mob_index("mob_abyss_gatekeeper"))
    {
       if (pMobIndex->count > 0)
       return;
       else
       {
       OBJ_INDEX_DATA *pObjIndex = get_reserved_obj_index("OBJ_VNUM_KEY_ABYSS");
       if (pObjIndex)
           obj = create_object(pObjIndex, 30, true);
       }
       }
       else
    */

    if (number_percent() <= chance)
    {
    pMobile = create_mobile(pMobIndex, false);
    if (obj)
        obj_to_char(obj, pMobile);
    char_to_room(pMobile, pRoom);
    }
}

void copy_shop_stock(SHOP_DATA *to_shop, SHOP_STOCK_DATA *from_stock)
{
    if( from_stock->next )
        copy_shop_stock(to_shop, from_stock->next);

    SHOP_STOCK_DATA *to_stock = new_shop_stock();

    to_stock->silver = from_stock->silver;
    to_stock->qp = from_stock->qp;
    to_stock->dp = from_stock->dp;
    to_stock->pneuma = from_stock->pneuma;
    to_stock->quantity = from_stock->quantity;
    to_stock->max_quantity = from_stock->quantity;
    to_stock->restock_rate = from_stock->restock_rate;
    to_stock->type = from_stock->type;
    to_stock->duration = ( from_stock->duration > 0 ) ? from_stock->duration : -1;
    to_stock->singular = from_stock->singular;
    to_stock->discount = URANGE(0,from_stock->discount,100);
    to_stock->level = from_stock->level;
    to_stock->entity.wnum = from_stock->entity.wnum;
    to_stock->reputation = from_stock->reputation;
    to_stock->reputation_load = from_stock->reputation_load;
    to_stock->min_reputation_rank = from_stock->min_reputation_rank;
    to_stock->max_reputation_rank = from_stock->max_reputation_rank;
    to_stock->min_show_rank = from_stock->min_show_rank;
    to_stock->max_show_rank = from_stock->max_show_rank;
    switch(to_stock->type)
    {
    case STOCK_OBJECT:
        if(to_stock->entity.wnum.vnum > 0)
            to_stock->obj = to_stock->entity.wnum.pArea ? 
                get_obj_index(to_stock->entity.wnum.pArea, to_stock->entity.wnum.vnum) : 
                get_obj_index_global(to_stock->entity.wnum.vnum);
        break;
    case STOCK_PET:
    case STOCK_MOUNT:
    case STOCK_GUARD:
    case STOCK_CREW:
        if(to_stock->entity.wnum.vnum > 0)
            to_stock->mob = to_stock->entity.wnum.pArea ? 
                get_mob_index(to_stock->entity.wnum.pArea, to_stock->entity.wnum.vnum) : 
                get_mob_index_global(to_stock->entity.wnum.vnum);
        break;
    case STOCK_SHIP:
        if(to_stock->entity.wnum.vnum > 0)
            to_stock->ship = to_stock->entity.wnum.pArea ?
                get_ship_index_for_area(to_stock->entity.wnum.pArea, to_stock->entity.wnum.vnum) :
                get_ship_index(to_stock->entity.wnum.vnum);
        break;
    case STOCK_CUSTOM:
        free_string(to_stock->custom_keyword);
        to_stock->custom_keyword = str_dup(from_stock->custom_keyword);
        break;
    }

    free_string(to_stock->custom_price);
    to_stock->custom_price = str_dup(from_stock->custom_price);

    free_string(to_stock->custom_descr);
    to_stock->custom_descr = str_dup(from_stock->custom_descr);

    to_stock->next = to_shop->stock;
    to_shop->stock = to_stock;
}

void copy_shop(SHOP_DATA *to_shop, SHOP_DATA *from_shop)
{
    int iTrade;
    to_shop->keeper = from_shop->keeper;
    for(iTrade = 0; iTrade < MAX_TRADE; iTrade++)
        to_shop->buy_type[iTrade] = from_shop->buy_type[iTrade];

    to_shop->profit_buy = from_shop->profit_buy;
    to_shop->profit_sell = from_shop->profit_sell;
    to_shop->open_hour = from_shop->open_hour;
    to_shop->close_hour = from_shop->close_hour;
    to_shop->flags = from_shop->flags;
    to_shop->restock_interval = from_shop->restock_interval;
    if( to_shop->restock_interval > 0 )
        to_shop->next_restock = current_time + to_shop->restock_interval * 60;

    if( from_shop->shipyard > 0 )
    {
        to_shop->shipyard = from_shop->shipyard;
        to_shop->shipyard_region[0][0] = from_shop->shipyard_region[0][0];
        to_shop->shipyard_region[0][1] = from_shop->shipyard_region[0][1];
        to_shop->shipyard_region[1][0] = from_shop->shipyard_region[1][0];
        to_shop->shipyard_region[1][1] = from_shop->shipyard_region[1][1];

        free_string(to_shop->shipyard_description);
        to_shop->shipyard_description = str_dup(from_shop->shipyard_description);
    }

    to_shop->discount = URANGE(0,from_shop->discount,100);
    to_shop->reputation = from_shop->reputation;
    to_shop->reputation_load = from_shop->reputation_load;
    to_shop->min_reputation_rank = from_shop->min_reputation_rank;

    if( from_shop->stock )
        copy_shop_stock(to_shop, from_shop->stock);
}

SHIP_CREW_DATA *copy_ship_crew(SHIP_CREW_INDEX_DATA *index)
{
    SHIP_CREW_DATA *crew = new_ship_crew();
    if( crew )
    {
        crew->scouting = index->scouting;
        crew->gunning = index->gunning;
        crew->oarring = index->oarring;
        crew->mechanics = index->mechanics;
        crew->navigation = index->navigation;
        crew->leadership = index->leadership;
    }
    return crew;
}

/* Create a mobile from a mob index template.*/
CHAR_DATA *create_mobile(MOB_INDEX_DATA *pMobIndex, bool persistLoad)
{
    CHAR_DATA *mob;
    AFFECT_DATA af;
    GQ_MOB_DATA *gq_mob;
    /* race is now mob->race which is RACE_DATA* */
    int i;

    mobile_count++;

    if (pMobIndex == NULL)
    {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Create_mobile: NULL pMobIndex.");
        exit(1);
    }

    mob = new_char();

    mob->pIndexData	= pMobIndex;

    if (pMobIndex->owner != NULL)
        mob->owner	= str_dup(pMobIndex->owner);
    else
        mob->owner	= str_dup("(no owner)");

    get_mob_id(mob);
    mob->spec_fun		= pMobIndex->spec_fun;
    mob->prompt			= NULL;

    mob->progs			= new_prog_data();
    mob->progs->progs	= pMobIndex->progs;
    variable_copylist(&pMobIndex->index_vars,&mob->progs->vars,false);
    variables_resolve_rsg_bindings(&mob->progs->vars);

    mob->deitypoints    = 0;
    mob->questpoints    = 0;
    mob->pneuma         = 0;

    /* give them some cash money */
    if (pMobIndex->wealth == 0)
    {
        mob->silver = 0;
        mob->gold   = 0;
    }
    else
    {
        long wealth;

        /* make sure bankers always have change money */
        if (IS_SET(pMobIndex->act[0], ACT_IS_BANKER))
        {
            wealth = 1000000;
            SET_BIT(mob->act[0], ACT_PROTECTED);
        }
        else
            wealth = number_range(pMobIndex->wealth/2, 3 * pMobIndex->wealth/2);

        /* make it easier on n00bs */
        /*	if (pMobIndex->area == find_area("Plith")
        ||   pMobIndex->area == find_area("Realm of Alendith"))
        wealth *= 2;*/

        mob->gold = number_range(wealth/200,wealth/100);
        mob->silver = wealth - (mob->gold * 100);
    }

    mob->act[0]				= pMobIndex->act[0];
    mob->act[1]				= pMobIndex->act[1];
    mob->comm				= COMM_NOCHANNELS|COMM_NOTELL;
    mob->affected_by[0]		= pMobIndex->affected_by[0];
    mob->affected_by[1]		= pMobIndex->affected_by[1];
    mob->alignment			= pMobIndex->alignment;
    mob->level				= pMobIndex->level;
    mob->tot_level			= pMobIndex->level;
    mob->hitroll			= pMobIndex->hitroll;
    mob->damroll			= pMobIndex->damage.bonus;
    mob->max_hit			= dice_roll(&pMobIndex->hit);
    mob->hit				= mob->max_hit;
    mob->max_mana			= dice_roll(&pMobIndex->mana);
    mob->mana				= mob->max_mana;
    mob->move				= pMobIndex->move;
    mob->max_move			= pMobIndex->move;
    mob->damage.number		= pMobIndex->damage.number;
    mob->damage.size		= pMobIndex->damage.size;
    mob->damage.bonus		= 0;
    mob->damage.last_roll	= -1;
    mob->dam_type			= pMobIndex->dam_type;
    if (mob->dam_type == 0)
    {
        switch(number_range(1,3))
        {
            case (1): mob->dam_type = attack_lookup("slash");	break;	// NIBS: Used to be constants, but I made them lookups
            case (2): mob->dam_type = attack_lookup("pound");	break;
            case (3): mob->dam_type = attack_lookup("pierce");	break;
        }
    }
    for (i = 0; i < 4; i++)
        mob->armour[i]	= pMobIndex->ac[i];

    mob->off_flags		= pMobIndex->off_flags;
    mob->imm_flags		= pMobIndex->imm_flags;
    mob->res_flags		= pMobIndex->res_flags;
    mob->vuln_flags		= pMobIndex->vuln_flags;
    mob->start_pos		= pMobIndex->start_pos;
    mob->default_pos	= pMobIndex->default_pos;
    mob->sex			= pMobIndex->sex;
    mob->body_type		= pMobIndex->body_type;
    mob->pronoun_he_she = str_dup(pMobIndex->pronoun_he_she);
    mob->pronoun_him_her = str_dup(pMobIndex->pronoun_him_her);
    mob->pronoun_his_her = str_dup(pMobIndex->pronoun_his_her);
    mob->pronoun_himself_herself = str_dup(pMobIndex->pronoun_himself_herself);
    mob->pronoun_his_hers = str_dup(pMobIndex->pronoun_his_hers);
    mob->verb_preference = pMobIndex->verb_preference;

    if (mob->sex == 3) /* random sex */
        mob->sex = number_range(1,2);

        // NEW: carry body type and pronoun/verb settings from index to instance
    mob->body_type = pMobIndex->body_type;
    mob->verb_preference = pMobIndex->verb_preference;

    // If index has custom pronouns, copy them; otherwise, reset to defaults for the body type
    if (pMobIndex->pronoun_he_she || pMobIndex->pronoun_him_her ||
        pMobIndex->pronoun_his_her || pMobIndex->pronoun_his_hers ||
        pMobIndex->pronoun_himself_herself)
    {
        if (pMobIndex->pronoun_he_she)          mob->pronoun_he_she          = str_dup(pMobIndex->pronoun_he_she);
        if (pMobIndex->pronoun_him_her)         mob->pronoun_him_her         = str_dup(pMobIndex->pronoun_him_her);
        if (pMobIndex->pronoun_his_her)         mob->pronoun_his_her         = str_dup(pMobIndex->pronoun_his_her);
        if (pMobIndex->pronoun_his_hers)        mob->pronoun_his_hers        = str_dup(pMobIndex->pronoun_his_hers);
        if (pMobIndex->pronoun_himself_herself) mob->pronoun_himself_herself = str_dup(pMobIndex->pronoun_himself_herself);
    }
    else
    {
        // Apply body-type defaults (uses body_type_info[] underneath)
        reset_pronouns_to_body_type(mob, mob->body_type);
    }

    // If body type is RANDOM, pick a concrete type now and set defaults
    if (mob->body_type == BODY_TYPE_RANDOM) {
        int pick = number_range(BODY_TYPE_NEUTRAL, BODY_TYPE_FEMALE); // tweak if you want to include OTHER
        mob->body_type = (body_type_t)pick;
        reset_pronouns_to_body_type(mob, mob->body_type);
    }

    mob->race				= pMobIndex->race;

    variables_resolve_entity_fields_mob(&mob->progs->vars, mob);
    mob->name = variables_expand_text_dup(mob->progs->vars, pMobIndex->player_name);
    mob->short_descr = variables_expand_text_dup(mob->progs->vars, pMobIndex->short_descr);
    mob->long_descr = variables_expand_text_dup(mob->progs->vars, pMobIndex->long_descr);
    mob->description = variables_expand_text_dup(mob->progs->vars, pMobIndex->description);

    mob->form				= pMobIndex->form;
    mob->parts				= pMobIndex->parts;
    mob->size				= pMobIndex->size;
    mob->material			= str_dup(pMobIndex->material);
    mob->corpse_type		= pMobIndex->corpse_type;
    mob->corpse_load		= pMobIndex->corpse_load;
    mob->corpse_wnum		= pMobIndex->corpse_wnum;

    mob->affected_by_perm[0]	= mob->race ? mob->race->aff[0] : 0;
    mob->affected_by_perm[1]	= mob->race ? mob->race->aff[1] : 0;
    mob->imm_flags_perm		= pMobIndex->imm_flags;
    mob->res_flags_perm		= pMobIndex->res_flags;
    mob->vuln_flags_perm	= pMobIndex->vuln_flags;


    for (i = 0; i < MAX_STATS; i ++)
        set_perm_stat_range(mob, i, 11 + mob->level/4, 0, 25);

    if (IS_SET(mob->act[0],ACT_WARRIOR))
    {
        mob->perm_stat[STAT_STR] += 3;
        mob->perm_stat[STAT_INT] -= 1;
        mob->perm_stat[STAT_CON] += 2;
    }

    if (IS_SET(mob->act[0],ACT_THIEF))
    {
        mob->perm_stat[STAT_DEX] += 3;
        mob->perm_stat[STAT_INT] += 1;
        mob->perm_stat[STAT_WIS] -= 1;
    }

    if (IS_SET(mob->act[0],ACT_CLERIC))
    {
        mob->perm_stat[STAT_WIS] += 3;
        mob->perm_stat[STAT_DEX] -= 1;
        mob->perm_stat[STAT_STR] += 1;
    }

    if (IS_SET(mob->act[0],ACT_MAGE))
    {
        mob->perm_stat[STAT_INT] += 3;
        mob->perm_stat[STAT_STR] -= 1;
        mob->perm_stat[STAT_DEX] += 1;
    }

    mob->perm_stat[STAT_STR] += mob->size - SIZE_MEDIUM;
    mob->perm_stat[STAT_CON] += (mob->size - SIZE_MEDIUM) / 2;

    if (!persistLoad)
    {
        if(pMobIndex->pShop != NULL)
        {
            mob->shop = new_shop();
            copy_shop(mob->shop, pMobIndex->pShop);
        }

        if(pMobIndex->pCrew != NULL)
        {
            mob->crew = copy_ship_crew(pMobIndex->pCrew);
        }


        memset(&af,0,sizeof(af));
        af.slot	= WEAR_NONE;	// None of the subsequent affects are from worn objects

        /* Put spells on here*/
        if (IS_AFFECTED(mob,AFF_INVISIBLE))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_INVISIBLE)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("invis");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= (mob->level - 4) / 7;
            if( af.modifier < 1 ) af.modifier = 1;
            af.bitvector	= AFF_INVISIBLE;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_DETECT_INVIS))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_DETECT_INVIS)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type 		= skill_resolve_gsn("detect invis");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= AFF_DETECT_INVIS;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_DETECT_HIDDEN))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_DETECT_HIDDEN)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("detect hidden");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= AFF_DETECT_HIDDEN;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_SANCTUARY))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_SANCTUARY)?AFFGROUP_RACIAL:AFFGROUP_DIVINE;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("sanctuary");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_NONE;
            af.modifier		= 0;
            af.bitvector	= AFF_SANCTUARY;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_INFRARED))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_INFRARED)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("infravision");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_NONE;
            af.modifier		= 0;
            af.bitvector	= AFF_INFRARED;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_DEATH_GRIP))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_DEATH_GRIP)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("death grip");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_NONE;
            af.modifier		= 0;
            af.bitvector	= AFF_DEATH_GRIP;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_FLYING))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_FLYING)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("fly");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_NONE;
            af.modifier		= 0;
            af.bitvector	= AFF_FLYING;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_PASS_DOOR))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_PASS_DOOR)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("pass door");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_NONE;
            af.modifier		= 0;
            af.bitvector	= AFF_PASS_DOOR;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED(mob,AFF_HASTE))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[0] : 0,AFF_HASTE)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("haste");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= APPLY_DEX;
            af.modifier		= (mob->level - 4) / 7;
            if( af.modifier < 1 ) af.modifier = 1;
            af.bitvector	= AFF_HASTE;
            af.bitvector2	= 0;
            affect_to_char(mob, &af);
        }

        if (IS_AFFECTED2(mob, AFF2_WARCRY))
        {
            af.group		= AFFGROUP_PHYSICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("warcry");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_WARCRY;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_LIGHT_SHROUD))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_LIGHT_SHROUD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("light shroud");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_LIGHT_SHROUD;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_HEALING_AURA))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_HEALING_AURA)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("healing aura");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_HEALING_AURA;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_ENERGY_FIELD))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_ENERGY_FIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("energy field");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_ENERGY_FIELD;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_SPELL_SHIELD))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_SPELL_SHIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("spell shield");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_SPELL_SHIELD;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_SPELL_DEFLECTION))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_SPELL_DEFLECTION)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("spell deflection");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_SPELL_DEFLECTION;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_AVATAR_SHIELD))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_AVATAR_SHIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("avatar shield");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_AVATAR_SHIELD;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_ELECTRICAL_BARRIER))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_ELECTRICAL_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("electrical barrier");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_ELECTRICAL_BARRIER;
            affect_to_char(mob,&af);
            REMOVE_BIT(mob->affected_by_perm[1], af.bitvector2);	// NIBS - why only this one?
        }

        if (IS_AFFECTED2(mob, AFF2_FIRE_BARRIER))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_FIRE_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("fire barrier");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_FIRE_BARRIER;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_FROST_BARRIER))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_FROST_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("frost barrier");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_FROST_BARRIER;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_IMPROVED_INVIS))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_IMPROVED_INVIS)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("improved invisibility");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_IMPROVED_INVIS;
            affect_to_char(mob,&af);
        }

        if (IS_AFFECTED2(mob, AFF2_STONE_SKIN))
        {
            af.group		= IS_SET(mob->race ? mob->race->aff[1] : 0,AFF2_STONE_SKIN)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
            af.where		= TO_AFFECTS;
            af.type			= skill_resolve_gsn("stone skin");
    af.skill = skill_find_uid(af.type);
            af.level		= mob->level;
            af.duration		= -1;
            af.location		= 0;
            af.modifier		= 0;
            af.bitvector	= 0;
            af.bitvector2	= AFF2_STONE_SKIN;
            affect_to_char(mob,&af);
        }
    }
    mob->position = mob->start_pos;

    mob->max_move = pMobIndex->move;
    mob->move = pMobIndex->move;

    /* link the mob to the world list */
    list_appendlink(loaded_chars, mob);

    /* Animate dead mobs don't add to the list.*/
    pMobIndex->count++;

    /* Keep GQ count*/
    for (gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next)
    {
        if (wnum_match(gq_mob->vnum_wnum, pMobIndex->area, pMobIndex->vnum))
            gq_mob->count++;
    }

    if(pMobIndex->persist)
        persist_addmobile(mob);

    // make sure nothing has 0 hp
    mob->max_hit = UMAX(1, mob->max_hit);

    // Copy reputation rewards from index to instance so scripts can mutate safely.
    {
        MOB_REPUTATION_DATA *prev = NULL;
        MOB_REPUTATION_DATA *rep;

        for (rep = pMobIndex->mob_reputations; rep; rep = rep->next)
        {
            MOB_REPUTATION_DATA *new_rep = copy_mob_reputation_data(rep);
            new_rep->next = NULL;

            if (prev)
                prev->next = new_rep;
            else
                mob->mob_reputations = new_rep;

            prev = new_rep;
        }
    }

    return mob;
}


CHAR_DATA *clone_mobile(CHAR_DATA *parent)
{
    CHAR_DATA *clone;
    int i;
    AFFECT_DATA *paf;

    if (parent == NULL || !IS_NPC(parent))
        return NULL;

    clone = create_mobile(parent->pIndexData, false);
    if(!clone)
        return NULL;

    clone->name 	= str_dup(parent->name);
    clone->version	= parent->version;
    clone->short_descr	= str_dup(parent->short_descr);
    clone->long_descr	= str_dup(parent->long_descr);
    clone->description	= str_dup(parent->description);
    /*clone->group	= parent->group;*/
    clone->sex		= parent->sex;
    /*clone->cclass	= parent->class;*/
    clone->race		= parent->race;
    clone->level	= parent->level;
    clone->tot_level	= parent->level;
    clone->trust	= 0;
    clone->timer	= parent->timer;
    clone->wait		= parent->wait;
    clone->hit		= parent->hit;
    clone->max_hit	= parent->max_hit;
    clone->mana		= parent->mana;
    clone->max_mana	= parent->max_mana;
    clone->move		= parent->move;
    clone->max_move	= parent->max_move;
    clone->gold		= parent->gold;
    clone->silver	= parent->silver;
    clone->exp		= parent->exp;
    clone->act[0]		= parent->act[0];
    clone->act[1]		= parent->act[1];
    clone->comm		= parent->comm;
    clone->imm_flags	= parent->imm_flags;
    clone->res_flags	= parent->res_flags;
    clone->vuln_flags	= parent->vuln_flags;
    clone->invis_level	= parent->invis_level;
    clone->affected_by[0]	= parent->affected_by[0];
    clone->affected_by[1]	= parent->affected_by[1];
    clone->position	= parent->position;
    clone->practice	= parent->practice;
    clone->train	= parent->train;
    clone->saving_throw	= parent->saving_throw;
    clone->alignment	= parent->alignment;
    clone->hitroll	= parent->hitroll;
    clone->damroll	= parent->damroll;
    clone->wimpy	= parent->wimpy;
    clone->form		= parent->form;
    clone->parts	= parent->parts;
    clone->size		= parent->size;
    clone->material	= str_dup(parent->material);
    clone->off_flags	= parent->off_flags;
    clone->dam_type	= parent->dam_type;
    clone->start_pos	= parent->start_pos;
    clone->default_pos	= parent->default_pos;
    clone->spec_fun	= parent->spec_fun;

    clone->affected_by_perm[0] = parent->affected_by_perm[0];
    clone->affected_by_perm[1] = parent->affected_by_perm[1];
    clone->imm_flags_perm = parent->imm_flags_perm;
    clone->res_flags_perm = parent->res_flags_perm;
    clone->vuln_flags_perm = parent->vuln_flags_perm;


    for (i = 0; i < 4; i++)
        clone->armour[i]	= parent->armour[i];

    for (i = 0; i < MAX_STATS; i++)
    {
        clone->perm_stat[i]	= parent->perm_stat[i];
        clone->mod_stat[i]	= parent->mod_stat[i];
        clone->dirty_stat[i] = true;
    }

    clone->damage.number = parent->damage.number;
    clone->damage.size = parent->damage.size;
    clone->damage.bonus = parent->damage.bonus;
    clone->damage.last_roll = -1;

    /* now add the affects */
    for (paf = parent->affected; paf != NULL; paf = paf->next)
        affect_to_char(clone,paf);

    variable_freelist(&clone->progs->vars);
    variable_copylist(&parent->progs->vars,&clone->progs->vars,false);

    if(parent->persist && !clone->persist)
        persist_addmobile(clone);

    return clone;
}

OBJ_DATA *create_object_noid(OBJ_INDEX_DATA *pObjIndex, int level, bool affects, bool add_to_loaded_objs)
{
    AFFECT_DATA *paf;
    CATALYST_DATA *cat;
    SPELL_DATA *spell, *spell_new;
    OBJ_DATA *obj;
    GQ_OBJ_DATA *gq_obj;

    if (pObjIndex == NULL)
    {
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Create_object: NULL pObjIndex.");
    return NULL;
    }

    obj = new_obj();

    obj->pIndexData	= pObjIndex;
    obj->in_room	= NULL;

    obj->level = pObjIndex->level;
    obj->wear_loc	= -1;
    obj->last_wear_loc	= -1;

    obj->progs 		= new_prog_data();
    obj->progs->progs	= pObjIndex->progs;

    obj->name		= str_dup(pObjIndex->name);
    obj->short_descr	= str_dup(pObjIndex->short_descr);
    obj->description	= str_dup(pObjIndex->description);
    if (pObjIndex->full_description != NULL
    && pObjIndex->full_description[0] != '\0')
    obj->full_description = str_dup(pObjIndex->full_description);
    else
    obj->full_description = str_dup(pObjIndex->description);
    obj->old_name 	= NULL;
    obj->old_short_descr 	= NULL;
    obj->old_description        = NULL;
    obj->loaded_by      = NULL;
    obj->script_created = false;
    obj->created_script_load.vnum = 0;
    obj->created_script_type = 0;
    obj->creation_time = current_time;
    obj->material	= str_dup(pObjIndex->material);
    obj->condition	= pObjIndex->condition;
    obj->times_allowed_fixed	= pObjIndex->times_allowed_fixed;
    obj->fragility	= pObjIndex->fragility;
    obj->item_type	= pObjIndex->item_type;
    obj->extra[0]	= pObjIndex->extra[0] & ~(ITEM_INVENTORY | ITEM_PERMANENT | ITEM_NOSKULL | ITEM_PLANTED);
    obj->extra[1]   = pObjIndex->extra[1] & ~(ITEM_ENCHANTED | ITEM_NO_RESURRECT | ITEM_THIRD_EYE | ITEM_BURIED | ITEM_UNSEEN );
    obj->extra[2]   = pObjIndex->extra[2] & ~(ITEM_FORCE_LOOT | ITEM_NO_ANIMATE );
    obj->extra[3]   = pObjIndex->extra[3];
    obj->wear_flags	= pObjIndex->wear_flags;
    obj->value[0]	= pObjIndex->value[0];
    obj->value[1]	= pObjIndex->value[1];
    obj->value[2]	= pObjIndex->value[2];
    obj->value[3]	= pObjIndex->value[3];
    obj->value[4]	= pObjIndex->value[4];
    obj->value[5]	= pObjIndex->value[5];
    obj->value[6]	= pObjIndex->value[6];
    obj->value[7]	= pObjIndex->value[7];
    obj->weight		= pObjIndex->weight;
    obj->cost           = pObjIndex->cost;
    obj->timer		= pObjIndex->timer;

    if( pObjIndex->lock )
    {
        obj->lock = new_lock_state();
        obj->lock->key_load = pObjIndex->lock->key_load;
        obj->lock->key_wnum = pObjIndex->lock->key_wnum;
        obj->lock->flags = pObjIndex->lock->flags;
        obj->lock->pick_chance = pObjIndex->lock->pick_chance;
    }

    /*
     * Mess with object properties.
     */
    switch (obj->item_type)
    {
    case ITEM_LIGHT:
        if (obj_get_legacy_value_slot(obj, 2) >= 999)
            obj_set_legacy_value_slot(obj, 2, -1);
        break;

    case ITEM_CATALYST:
        if (!obj_get_legacy_value_slot(obj, 1))
            obj_set_legacy_value_slot(obj, 1, 1); /* Fix zero charge catalysts to single uses*/
        break;

    case ITEM_BOOK:
    case ITEM_HERB:
    case ITEM_FURNITURE:
    case ITEM_TRADE_TYPE:
    case ITEM_SEED:
    case ITEM_SEXTANT:
    case ITEM_INSTRUMENT:
    case ITEM_CART:
    case ITEM_SHARECERT:
    case ITEM_SMOKE_BOMB:
    case ITEM_ROOM_ROOMSHIELD:
    case ITEM_ROOM_DARKNESS:
    case ITEM_STINKING_CLOUD:
    case ITEM_WITHERING_CLOUD:
    case ITEM_ROOM_FLAME:
    case ITEM_ARTIFACT:
    case ITEM_TRASH:
    case ITEM_CONTAINER:
    case ITEM_WEAPON_CONTAINER:
    case ITEM_DRINK_CON:
    case ITEM_KEY:
    case ITEM_BOAT:
    case ITEM_CORPSE_NPC:
    case ITEM_CORPSE_PC:
    case ITEM_RANGED_WEAPON:
    case ITEM_FOUNTAIN:
    case ITEM_MAP:
    case ITEM_SHIP:
    case ITEM_CLOTHING:
    case ITEM_PORTAL:
    case ITEM_TREASURE:
    case ITEM_SPELL_TRAP:
    case ITEM_ROOM_KEY:
    case ITEM_GEM:
    case ITEM_JEWELRY:
    case ITEM_JUKEBOX:
    case ITEM_SCROLL:
    case ITEM_WAND:
    case ITEM_STAFF:
    case ITEM_WEAPON:
    case ITEM_ARMOUR:
    case ITEM_BANK:
    case ITEM_KEYRING:
    case ITEM_POTION:
    case ITEM_PILL:
    case ITEM_ICE_STORM:
    case ITEM_FLOWER:
    case ITEM_MIST:
    case ITEM_MONEY:
    case ITEM_WHISTLE:
    case ITEM_SHOVEL:
    case ITEM_SHRINE:
    case ITEM_INK:
    case ITEM_TATTOO:
        break;

    case ITEM_FOOD:
        obj->timer = obj_get_legacy_value_slot(obj, 4);
            break;

    default:
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "create_object: vnum %ld bad type.", pObjIndex->vnum);

        break;
    }

    /* Random affects on objs*/
    if (affects)
    {
    for (paf = pObjIndex->affected; paf != NULL; paf = paf->next)
    {
        if (number_percent() < paf->random || paf->random == 100)
        affect_to_obj(obj,paf);
    }

    for (spell = pObjIndex->spells; spell != NULL; spell = spell->next)
    {
        if (number_percent() < spell->repop || spell->repop == 100)
        {
        spell_new = new_spell();
        spell_new->sn = spell->sn;
        spell_new->level = spell->level;

        spell_new->next = obj->spells;
        obj->spells = spell_new;
        }
    }

    for (cat = pObjIndex->catalyst; cat != NULL; cat = cat->next)
    {
        if (number_percent() < cat->random || cat->random == 100)
        catalyst_to_obj(obj,cat);
    }
    }

    variable_copylist(&pObjIndex->index_vars,&obj->progs->vars,false);
    variables_resolve_rsg_bindings(&obj->progs->vars);

    free_string(obj->name);
    obj->name = variables_expand_text_dup(obj->progs->vars, pObjIndex->name);
    free_string(obj->short_descr);
    obj->short_descr = variables_expand_text_dup(obj->progs->vars, pObjIndex->short_descr);
    free_string(obj->description);
    obj->description = variables_expand_text_dup(obj->progs->vars, pObjIndex->description);
    free_string(obj->full_description);
    if (!IS_NULLSTR(pObjIndex->full_description))
        obj->full_description = variables_expand_text_dup(obj->progs->vars, pObjIndex->full_description);
    else
        obj->full_description = variables_expand_text_dup(obj->progs->vars, pObjIndex->description);

    obj->num_enchanted = 0;
    obj->version = VERSION_OBJECT_000;
    obj->locker = false;

    // Copy type-specific data structs from the template (canonical source).
    // Falls back to migrating from value[] if the template has no type data.
    if (!TBIT_EMPTY(pObjIndex->type_flags)) {
        obj_copy_type_data_from_index(obj, pObjIndex);
    } else {
        obj_migrate_values_to_types(obj);
    }

    if (add_to_loaded_objs)
    {
        list_appendlink(loaded_objects, obj);
        loaded_obj_hash_add(obj);
        pObjIndex->count++;
    }


    /* If loading a relic for whatever reason, update the pointers here.*/
    if (pObjIndex == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_DAMAGE"))
    damage_relic = obj;

    if (pObjIndex == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_XP"))
    xp_relic = obj;

    if (pObjIndex == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_PNEUMA"))
    pneuma_relic = obj;

    if (pObjIndex == get_reserved_obj_index("OBJ_VNUM_RELIC_HP_REGEN"))
    hp_regen_relic = obj;

    if (pObjIndex == get_reserved_obj_index("OBJ_VNUM_RELIC_MANA_REGEN"))
    mana_regen_relic = obj;

    /* Keep GQ count*/
    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
    {
    if (wnum_match(gq_obj->vnum_wnum, pObjIndex->area, pObjIndex->vnum))
        gq_obj->count++;
    }

    if(pObjIndex->persist) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "create_object_noid: Adding object %ld to persistance.", pObjIndex->vnum);
        persist_addobject(obj);
    }

    return obj;
}

OBJ_DATA *create_object(OBJ_INDEX_DATA *pObjIndex, int level, bool affects)
{
    OBJ_DATA *obj;

    obj = create_object_noid(pObjIndex,level,affects, true);

    if( obj )
    {
        // Copy the extra descriptions
        for (EXTRA_DESCR_DATA *ed = pObjIndex->extra_descr; ed != NULL; ed = ed->next)
        {
            EXTRA_DESCR_DATA *ed_new	= new_extra_descr();
            ed_new->keyword				= variables_expand_text_dup(obj->progs->vars, ed->keyword);
            if( ed->description )
                ed_new->description			= variables_expand_text_dup(obj->progs->vars, ed->description);
            else
                ed_new->description			= NULL;
            ed_new->next				= obj->extra_descr;
            obj->extra_descr			= ed_new;
        }

        if( pObjIndex->waypoints )
        {
            obj->waypoints = list_copy(pObjIndex->waypoints);
        }

        get_obj_id(obj);
    }

    return obj;
}


void clone_object(OBJ_DATA *parent, OBJ_DATA *clone)
{
    int i;
    AFFECT_DATA *paf, *paf_next;
    CATALYST_DATA *cat, *cat_next;
    EXTRA_DESCR_DATA *ed,*ed_new;

    if (parent == NULL || clone == NULL)
    return;

    /* remove the affects first so we don't get dual affects */
    for (paf = clone->affected; paf != NULL; paf = paf_next) {
        paf_next = paf->next;

    affect_remove_obj(clone, paf);
    }

    for (cat = clone->catalyst; cat != NULL; cat = cat_next) {
        cat_next = cat->next;

    free_catalyst(cat);
    }

    clone->affected = NULL;
    clone->catalyst = NULL;
    if( clone->waypoints )
    {
        list_destroy(clone->waypoints);
        clone->waypoints = NULL;
    }

    /* start fixing the object */
    clone->name 	= str_dup(parent->name);
    clone->short_descr 	= str_dup(parent->short_descr);
    clone->description	= str_dup(parent->description);
    clone->item_type	= parent->item_type;
    clone->extra[0]	= parent->extra[0];
    clone->extra[1]	= parent->extra[1];
    clone->extra[2]	= parent->extra[2];
    clone->extra[3]	= parent->extra[3];
    clone->wear_flags	= parent->wear_flags;
    clone->weight	= parent->weight;
    clone->cost		= parent->cost;
    clone->level	= parent->level;
    clone->condition	= parent->condition;
    clone->material	= str_dup(parent->material);
    clone->timer	= parent->timer;
    clone->num_enchanted = parent->num_enchanted;

    for (i = 0;  i < 8; i ++)
        clone->value[i]	= parent->value[i];

    /* Type-specific data */
    obj_free_type_data(clone);
    obj_copy_type_data(clone, parent);

    /* affects */
    for (paf = parent->affected; paf != NULL; paf = paf->next)
        affect_to_obj(clone,paf);

    /* catalyst affects */
    for (cat = parent->catalyst; cat != NULL; cat = cat->next)
        catalyst_to_obj(clone,cat);

    // Free loaded extra description
    if( clone->extra_descr )
    {
        EXTRA_DESCR_DATA *ed_next;

        for(ed = clone->extra_descr; ed; ed = ed_next)
        {
            ed_next = ed->next;
            free_extra_descr(ed);
        }
        clone->extra_descr = NULL;
    }

    if( parent->waypoints )
    {
        clone->waypoints = list_copy(parent->waypoints);
    }

    /* extended desc */
    for (ed = parent->extra_descr; ed != NULL; ed = ed->next)
    {
        ed_new                  = new_extra_descr();
        ed_new->keyword    	= str_dup(ed->keyword);
        ed_new->description     = str_dup(ed->description);
        ed_new->next           	= clone->extra_descr;
        clone->extra_descr  	= ed_new;
    }

    variable_freelist(&clone->progs->vars);
    variable_copylist(&parent->progs->vars,&clone->progs->vars,false);

    if(parent->persist && !clone->persist) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "clone_object: Adding object %ld to persistance.", clone->pIndexData->vnum);

        persist_addobject(clone);
    }
}


EXTRA_DESCR_DATA *get_extra_descr(const char *name, EXTRA_DESCR_DATA *ed)
{
    for (; ed != NULL; ed = ed->next)
    {
    if (is_name((char *) name, ed->keyword))
        return ed;
    }
    return NULL;
}


/*
 * Translates mob virtual number to its mob index struct.
 */
MOB_INDEX_DATA *get_mob_index(AREA_DATA *pArea, long vnum)
{
    MOB_INDEX_DATA *mob;

    if (!pArea) return NULL;

    for (mob = pArea->mob_index_hash[vnum % MAX_KEY_HASH]; mob != NULL; mob = mob->next)
    {
    if (mob->vnum == vnum)
        return mob;
    }

    if (fBootDb)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Get_mob_index: bad vnum %ld.", vnum);
    log_get_stacktrace(1);

    return NULL;
    }

    return NULL;
}

MOB_INDEX_DATA *get_mob_index_global(long vnum)
{
    WNUM wnum;
    char vnum_str[32];
    
    snprintf(vnum_str, sizeof(vnum_str), "%ld", vnum);
    
    // Use parse_widevnum with NULL context for global search
    // This provides backwards compatibility and centralized vnum resolution
    if (parse_widevnum(vnum_str, NULL, &wnum) && wnum.pArea) {
        return get_mob_index(wnum.pArea, wnum.vnum);
    }
    
    return NULL;
}


/*
 * Translates mob virtual number to its obj index struct.
 */
OBJ_INDEX_DATA *get_obj_index(AREA_DATA *pArea, long vnum)
{
    OBJ_INDEX_DATA *obj;

    if (!pArea) return NULL;

    for (obj = pArea->obj_index_hash[vnum % MAX_KEY_HASH]; obj != NULL; obj = obj->next)
    {
    if (obj->vnum == vnum)
        return obj;
    }

    if (fBootDb)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
        "Get_obj_index: bad vnum %ld (widevnum %ld#%ld, area '%s').",
        vnum,
        pArea ? pArea->uid : 0,
        vnum,
        (pArea && pArea->name) ? pArea->name : "(unknown)");
    log_get_stacktrace(1);
    return NULL;
    }

    return NULL;
}

OBJ_INDEX_DATA *get_obj_index_global(long vnum)
{
    WNUM wnum;
    char vnum_str[32];
    
    snprintf(vnum_str, sizeof(vnum_str), "%ld", vnum);
    
    // Use parse_widevnum with NULL context for global search
    // This provides backwards compatibility and centralized vnum resolution
    if (parse_widevnum(vnum_str, NULL, &wnum) && wnum.pArea) {
        return get_obj_index(wnum.pArea, wnum.vnum);
    }
    
    return NULL;
}


/*
 * Translates room virtual number to its room index struct.
 */
ROOM_INDEX_DATA *get_room_index(AREA_DATA *pArea, long vnum)
{
    ROOM_INDEX_DATA *room;

    if (!pArea) return NULL;

    for (room = pArea->room_index_hash[vnum % MAX_KEY_HASH]; room != NULL; room = room->next)
    {
    if (room->vnum == vnum)
        return room;
    }

    if (fBootDb)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
        "Get_room_index: bad vnum %ld (widevnum %ld#%ld, area '%s').",
        vnum,
        pArea ? pArea->uid : 0,
        vnum,
        (pArea && pArea->name) ? pArea->name : "(unknown)");
        return NULL;
    }

    return NULL;
}

ROOM_INDEX_DATA *get_room_index_global(long vnum)
{
    WNUM wnum;
    char vnum_str[32];
    
    snprintf(vnum_str, sizeof(vnum_str), "%ld", vnum);
    
    // Use parse_widevnum with NULL context for global search
    // This provides backwards compatibility and centralized vnum resolution
    if (parse_widevnum(vnum_str, NULL, &wnum) && wnum.pArea) {
        return get_room_index(wnum.pArea, wnum.vnum);
    }
    
    return NULL;
}


TOKEN_INDEX_DATA *get_token_index(AREA_DATA *pArea, long vnum)
{
    TOKEN_INDEX_DATA *token_index;

    if (!pArea) return NULL;

    for (token_index = pArea->token_index_hash[vnum % MAX_KEY_HASH]; token_index != NULL; token_index = token_index->next)
    {
    if (token_index->vnum == vnum)
        return token_index;
    }

    return NULL;
}

TOKEN_INDEX_DATA *get_token_index_global(long vnum)
{
    AREA_DATA *pArea;
    TOKEN_INDEX_DATA *token_index;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        token_index = get_token_index(pArea, vnum);
        if (token_index)
            return token_index;
    }

    return NULL;
}

TOKEN_INDEX_DATA *get_token_index_wnum(WNUM wnum)
{
    if (!wnum.pArea)
        return NULL;
    return get_token_index(wnum.pArea, wnum.vnum);
}

bool is_singular_token(TOKEN_INDEX_DATA *index)
{
    if(!index) return false;

    // These can allow multiples
    if(index->type == TOKEN_GENERAL || index->type == TOKEN_QUEST || index->type == TOKEN_AFFECT)
        if(!IS_SET(index->flags, TOKEN_SINGULAR))
            return false;

    return true;
}


SCRIPT_DATA *get_script_index(AREA_DATA *pArea, long vnum, int type)
{
    SCRIPT_DATA *prg;

    if (!pArea) return NULL;

    switch (type)
    {
    case PRG_MPROG:
        prg = pArea->mprog_list;
        break;
    case PRG_OPROG:
        prg = pArea->oprog_list;
        break;
    case PRG_RPROG:
        prg = pArea->rprog_list;
        break;
    case PRG_TPROG:
        prg = pArea->tprog_list;
        break;
    case PRG_APROG:
        prg = pArea->aprog_list;
        break;
    case PRG_IPROG:
        prg = pArea->iprog_list;
        break;
    case PRG_DPROG:
        prg = pArea->dprog_list;
        break;
    case PRG_QPROG:
        prg = pArea->qprog_list;
        break;
    case PRG_EPROG:
        prg = pArea->eprog_list;
        break;
    default:
        return NULL;
    }

    for(; prg; prg = prg->next) {
    if (prg->vnum == vnum)
            return(prg);
    }
    return NULL;
}

// Global script lookup - searches all areas
SCRIPT_DATA *get_script_index_global(long vnum, int type)
{
    AREA_DATA *pArea;
    SCRIPT_DATA *prg;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        if ((prg = get_script_index(pArea, vnum, type)) != NULL)
            return prg;
    }
    return NULL;
}


/*
 * Read a letter from a file.
 */
char fread_letter(FILE *fp)
{
    int c;

    do
    {
    c = getc(fp);
    if (c == EOF) return '\0';
    }
    while (ISSPACE(c));

    return (char)c;
}


char *fwrite_flag(long flags, char buf[])
{
    char offset;
    char *cp;

    buf[0] = '\0';

    if (flags == 0)
    {
    strcpy(buf, "0");
    return buf;
    }

    /* 32 -- number of bits in a long */

    for (offset = 0, cp = buf; offset < 32; offset++)
    if (flags & ((long)1 << offset))
    {
        if (offset <= 'Z' - 'A')
        *(cp++) = 'A' + offset;
        else
        *(cp++) = 'a' + offset - ('Z' - 'A' + 1);
    }

    *cp = '\0';

    return buf;
}


/*
 * Read a number from a file.
 */
long fread_number(FILE *fp)
{
    long number;
    bool sign;
    char c;

    do
    {
    c = getc(fp);
    }
    while (ISSPACE(c));

    number = 0;

    sign   = false;
    if (c == '+')
    {
    c = getc(fp);
    }
    else if (c == '-')
    {
    sign = true;
    c = getc(fp);
    }

    if (!ISDIGIT(c))
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_number: bad format (%c).", c);
    exit(1);
    }

    while (ISDIGIT(c))
    {
    number = number * 10 + c - '0';
    c      = getc(fp);
    }

    if (sign)
    number = 0 - number;

    if (c == '|')
    number += fread_number(fp);
    else if (c != ' ')
    ungetc(c, fp);

    return number;
}


long fread_flag(FILE *fp)
{
    long number;
    char c;
    bool negative = false;

    do
    {
    c = getc(fp);
    }
    while (ISSPACE(c));

    if (c == '-')
    {
    negative = true;
    c = getc(fp);
    }

    number = 0;

    if (!ISDIGIT(c))
    {
    while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'))
    {
        number += flag_convert(c);
        c = getc(fp);
    }
    }

    while (ISDIGIT(c))
    {
    number = number * 10 + c - '0';
    c = getc(fp);
    }

    if (c == '|')
    number += fread_flag(fp);

    else if  (c != ' ')
    ungetc(c,fp);

    if (negative)
    return -1 * number;

    return number;
}


long flag_convert(char letter)
{
    long bitsum = 0;
    char i;

    if ('A' <= letter && letter <= 'Z')
    {
    bitsum = 1;
    for (i = letter; i > 'A'; i--)
        bitsum *= 2;
    }
    else if ('a' <= letter && letter <= 'z')
    {
    bitsum = 67108864; /* 2^26 */
    for (i = letter; i > 'a'; i --)
        bitsum *= 2;
    }

    return bitsum;
}


/*
 * Read and allocate space for a string from a file.
 * These strings are read-only and shared.
 * Strings are hashed:
 *   each string prepended with hash pointer to prev string,
 *   hash code is simply the string length.
 *   this function takes 40% to 50% of boot-up time.
 */
char *fread_string(FILE *fp)
{
    char *plast;
    char c;

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
    exit(1);
    }

    /*
     * Skip blanks.
     * Read first char.
     */
    do
    {
    c = getc(fp);
    }
    while (ISSPACE(c));

    if ((*plast++ = c) == '~')
    return &str_empty[0];

    for (;;)
    {
    /*
     * Back off the char type lookup,
     *   it was too dirty for portability.
     *   -- Furey
     */

    switch (*plast = getc(fp))
    {
        default:
        plast++;
        break;

        case EOF:
        /* temp fix */
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Fread_string: EOF");
        return NULL;
        /* exit(1); */
        break;

        case '\n':
        plast++;
        *plast++ = '\r';
        break;

        case '\r':
        break;

        case '~':
        plast++;
        {
            union
            {
            char *	pc;
            char	rgc[sizeof(char *)];
            } u1;
            int ic;
            int iHash;
            char *pHash;
            char *pHashPrev;
            char *pString;

            plast[-1] = '\0';
            iHash     = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
            for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
            {
            for (ic = 0; ic < sizeof(char *); ic++)
                u1.rgc[ic] = pHash[ic];
            pHashPrev = u1.pc;
            pHash    += sizeof(char *);

            if (top_string[sizeof(char *)] == pHash[0]
                &&   !strcmp(top_string+sizeof(char *)+1, pHash+1))
                return pHash;
            }

            if (fBootDb)
            {
            pString		= top_string;
            top_string		= plast;
            u1.pc		= string_hash[iHash];
            for (ic = 0; ic < sizeof(char *); ic++)
                pString[ic] = u1.rgc[ic];
            string_hash[iHash]	= pString;

            nAllocString += 1;
            sAllocString += top_string - pString;
            return pString + sizeof(char *);
            }
            else
            {
            return str_dup(top_string + sizeof(char *));
            }
        }
    }
    }
}


/* Returns a string without \r and ~. */
char *fix_string(const char *str)
{
    static char strfix[MAX_STRING_LENGTH * 2];
    int i;
    int o;
    if (str == NULL)
    return &str_empty[0];
    for (o = i = 0; str[i+o] != '\0'; i++)
    {
    if (str[i+o] == '\r' || str[i+o] == '~')
        o++;
    strfix[i] = str[i+o];
    if (i >= MAX_STRING_LENGTH * 2 - 1) {
        break;
    }
    }
    strfix[i] = '\0';
    return strfix;
}


char *fread_string_eol(FILE *fp)
{
    static bool char_special[256-EOF];
    char *plast;
    char c;

    if (char_special[EOF-EOF] != true)
    {
    char_special[EOF -  EOF] = true;
    char_special['\n' - EOF] = true;
    char_special['\r' - EOF] = true;
    }

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
    exit(1);
    }

    /*
     * Skip blanks.
     * Read first char.
     */
    do
    {
    c = getc(fp);
    }
    while (ISSPACE(c));

    if ((*plast++ = c) == '\n')
    return &str_empty[0];

    for (;;)
    {
    if (!char_special[ (*plast++ = getc(fp)) - EOF ])
        continue;

    switch (plast[-1])
    {
        default:
        break;

        case EOF:
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Fread_string_eol EOF");
        exit(1);
        break;

        case '\n':  case '\r':
        {
            union
            {
            char *      pc;
            char        rgc[sizeof(char *)];
            } u1;
            int ic;
            int iHash;
            char *pHash;
            char *pHashPrev;
            char *pString;

            plast[-1] = '\0';
            iHash     = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
            for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
            {
            for (ic = 0; ic < sizeof(char *); ic++)
                u1.rgc[ic] = pHash[ic];
            pHashPrev = u1.pc;
            pHash    += sizeof(char *);

            if (top_string[sizeof(char *)] == pHash[0]
                &&   !strcmp(top_string+sizeof(char *)+1, pHash+1))
                return pHash;
            }

            if (fBootDb)
            {
            pString             = top_string;
            top_string          = plast;
            u1.pc               = string_hash[iHash];
            for (ic = 0; ic < sizeof(char *); ic++)
                pString[ic] = u1.rgc[ic];
            string_hash[iHash]  = pString;

            nAllocString += 1;
            sAllocString += top_string - pString;
            return pString + sizeof(char *);
            }
            else
            {
            return str_dup(top_string + sizeof(char *));
            }
        }
    }
    }
}


/*
 * Read to end of line (for comments).
 */
void fread_to_eol(FILE *fp)
{
    char c;

    do
    {
    c = getc(fp);
    }
    while (c != '\n' && c != '\r');

    do
    {
    c = getc(fp);
    }
    while (c == '\n' || c == '\r');

    ungetc(c, fp);
    return;
}


/*
 * Read one word (into static buffer).
 */
char *fread_word(FILE *fp)
{
    static char word[MAX_INPUT_LENGTH];
    char *pword;
    int cEnd;

    if (feof(fp)) {
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Fread_word: EOF encountered");
    return str_dup("");
}

    do
    {
    cEnd = getc(fp);
    if (cEnd == EOF) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Fread_word: EOF encountered while skipping whitespace");
        word[0] = '\0';
        return word;
    }
    }
    while (ISSPACE(cEnd));

    if (cEnd == '\'' || cEnd == '"')
    {
    pword   = word;
    }
    else
    {
    word[0] = cEnd;
    pword   = word+1;
    cEnd    = ' ';
    }

    for (; pword < word + MAX_INPUT_LENGTH; pword++)
    {
    int ch = getc(fp);
    if (ch == EOF)
    {
        *pword = '\0';
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_word: EOF mid-word (%s).", word);
        return word;
    }
    *pword = (char)ch;
    if (cEnd == ' ' ? ISSPACE(*pword) : *pword == cEnd)
    {
        if (cEnd == ' ')
        ungetc(*pword, fp);
        *pword = '\0';
        return word;
    }
    }

    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_word: word too long (%s).", word);
    exit(1);
    return NULL;
}


/*
 * Allocate some ordinary memory,
 *   with the expectation of freeing it someday.
 */
void *alloc_mem(int sMem)
{
#if 0
    void *pMem;
    int *magic;
    int iList;

    sMem += sizeof(*magic);

    for (iList = 0; iList < MAX_MEM_LIST; iList++)
    {
        if (sMem <= rgSizeList[iList])
            break;
    }

    if (iList == MAX_MEM_LIST)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alloc_mem: size %d too large.", sMem);
        exit(1);
    }

    if (rgFreeList[iList] == NULL)
    {
        pMem              = alloc_perm(rgSizeList[iList]);
    }
    else
    {
        pMem              = rgFreeList[iList];
        rgFreeList[iList] = * ((void **) rgFreeList[iList]);
    }

    magic = (int *) pMem;
    *magic = MAGIC_NUM;
    pMem += sizeof(*magic);

    return pMem;
#else
    void *pMem = calloc(1, sMem);
    if (!pMem) {
        perror("alloc_mem");
        abort();
    }
    return pMem;
#endif
}


/*
 * Free some memory.
 * Recycle it back onto the free list for blocks of that size.
 */
void free_mem(void *pMem, int sMem)
{
#if 0
    int iList;
    int *magic;

    pMem -= sizeof(*magic);
    magic = (int *) pMem;

    if (*magic != MAGIC_NUM)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Attempt to recycle invalid memory of size %d. Magic: %08X, expected: %08X",sMem, *magic, MAGIC_NUM);
    abort();
        return;
    }

    *magic = 0;
    sMem += sizeof(*magic);

    for (iList = 0; iList < MAX_MEM_LIST; iList++)
    {
        if (sMem <= rgSizeList[iList])
            break;
    }

    if (iList == MAX_MEM_LIST)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Free_mem: size %d too large.", sMem);
        abort();
    }

    * ((void **) pMem) = rgFreeList[iList];
    rgFreeList[iList]  = pMem;

    return;
#else
    free(pMem);
#endif
}


/*
 * Allocate some permanent memory.
 * Permanent memory is never freed,
 * pointers into it may be copied safely.
 */
void *alloc_perm(long sMem)
{
#if 0
    static char *pMemPerm;
    static long iMemPerm;
    void *pMem;

    /* Scale the memory up to the next highest block size*/
    while (sMem % sizeof(long) != 0)
    sMem++;

    /* They asked for too much memory*/
    if (sMem > MAX_PERM_BLOCK)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Alloc_perm: %d too large.", sMem);
    abort();
    }

    if (pMemPerm == NULL || iMemPerm + sMem > MAX_PERM_BLOCK)
    {
    iMemPerm = 0;
    if ((pMemPerm = calloc(1, MAX_PERM_BLOCK)) == NULL)
    {
        perror("Alloc_perm");
        exit(1);
    }
    }

    pMem        = pMemPerm + iMemPerm;
    iMemPerm   += sMem;
    nAllocPerm += 1;
    sAllocPerm += sMem;
    return pMem;
#else
    void *pMem = calloc(1, sMem);
    if (!pMem) {
        perror("alloc_perm");
        abort();
    }
    return pMem;
#endif
}


/*
 * Duplicate a string into dynamic memory.
 * Fread_strings are read-only and shared.
 */
char *str_dup(const char *str)
{
    char *str_new;

    if (IS_NULLSTR(str))
    return &str_empty[0];

    if (str >= string_space && str < top_string)
    return (char *) str;

    str_new = alloc_mem(strlen(str) + 1);
    strcpy(str_new, str);

    nAllocString += 1;
    return str_new;
}


/*
 * Free a string.
 * Null is legal here to simplify callers.
 * Read-only shared strings are not touched.
 */
void free_string(char *pstr)
{
    if (pstr == NULL
        ||   pstr == &str_empty[0]
        || (pstr >= string_space && pstr < top_string))
    return;

    free_mem(pstr, strlen(pstr) + 1);
    nAllocString -= 1;
}


/*
 * Stick a little fuzz on a number.
 */
int number_fuzzy(int number)
{
    switch (number_bits(2))
    {
    case 0:  number -= 1; break;
    case 3:  number += 1; break;
    }

    return UMAX(1, number);
}


/*
 * Generate a random number.
 */
int number_range(int from, int to)
{
    int power;
    int number;

    if (from == 0 && to == 0)
    return 0;

    if ((to = to - from + 1) <= 1)
    return from;

    for (power = 2; power < to; power <<= 1)
    ;

    while ((number = number_mm() & (power -1)) >= to)
    ;

    return from + number;
}


/*
 * Generate a percentile roll.
 */
int number_percent(void)
{
    int percent;

    while ((percent = number_mm() & (128-1)) > 99)
    ;

    return percent;
}


/*
 * Generate a random door.
 */
int number_door(void)
{
    int door;

    while ((door = number_mm() & (16-1)) >= MAX_DIR)
    ;

    return door;
}


int number_bits(int width)
{
    return number_mm() & ((1 << width) - 1);
}


/*
 * I've gotten too many bad reports on OS-supplied random number generators.
 * This is the Mitchell-Moore algorithm from Knuth Volume II.
 * Best to leave the constants alone unless you've read Knuth.
 * -- Furey
 */

/* I noticed streaking with this random number generator, so I switched
   back to the system srandom call.  If this doesn't work for you,
   define OLD_RAND to use the old system -- Alander */

void init_mm()
{
    srandom(time(NULL)^getpid());
    return;
}


long number_mm(void)
{
    return random() >> 6;
}


/*
 * Roll some dice.
 */
long dice(int number, int size)
{
    int idice;
    long sum;

    switch (size)
    {
    case 0: return 0;
    case 1: return number;
    }

    for (idice = 0, sum = 0; idice < number; idice++)
    sum += number_range(1, size);

    return sum;
}


/*
 * Simple linear interpolation.
 */
int interpolate(int level, int value_00, int value_32)
{
    return value_00 + level * (value_32 - value_00) / 32;
}


/*
 * Removes the tildes from a string.
 * Used for player-entered strings that go into disk files.
 */
void smash_tilde(char *str)
{
    for (; *str != '\0'; str++)
    {
    if (*str == '~')
        *str = '-';
    }
}


/* @@@NIB : 20070123 : Returns < 0 if A < B, > 0 if A > B, 0 if A = B*/
int str_cmp(register const char *astr, register const char *bstr)
{
    if (astr == NULL)
    {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Str_cmp: null astr.");
        return -1;
    }

    if (bstr == NULL)
    {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Str_cmp: null bstr.");
        return 1;
    }

    for (; *astr || *bstr; astr = utf8_nextchar(astr), bstr = utf8_nextchar(bstr)) {
        unichar_t a = utf8_tolower(utf8_getchar(astr));
        unichar_t b = utf8_tolower(utf8_getchar(bstr));
        if (a != b)
            return a - b;
    }

    return 0;
}

// str_cmp, ignoring color codes
int str_cmp_nocolour(const char *astr, const char *bstr)
{
    char *ncastr, *ncbstr, *nca, *ncb;
    if (astr == NULL)
    {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Str_cmp: null astr.");
        return -1;
    }

    if (bstr == NULL)
    {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Str_cmp: null bstr.");
        return 1;
    }

    nca = ncastr = nocolour(astr);
    ncb = ncbstr = nocolour(bstr);

    for (; *ncastr || *ncbstr; ncastr = utf8_nextchar(ncastr), ncbstr = utf8_nextchar(ncbstr)) {
        unichar_t a = utf8_tolower(utf8_getchar(ncastr));
        unichar_t b = utf8_tolower(utf8_getchar(ncbstr));
        if (a != b) {
            free_string(nca);
            free_string(ncb);
            return a - b;
        }
    }

    free_string(nca);
    free_string(ncb);
    return 0;
}


/*
 * Compare strings, case insensitive, for prefix matching.
 * Return true if astr not a prefix of bstr
 *   (compatibility with historical functions).
 */
bool str_prefix(const char *astr, const char *bstr)
{
    if (astr == NULL)
    {
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Strn_cmp: null astr.");
    return true;
    }

    if (bstr == NULL)
    {
    log_message(LOG_LEVEL_BUG, LOG_ERROR, "Strn_cmp: null bstr.");
    return true;
    }

    for (; *astr; astr++, bstr++)
    {
    if (LOWER(*astr) != LOWER(*bstr))
        return true;
    }

    return false;
}


/*
 * Compare strings, case insensitive, for match anywhere.
 * Returns true is astr not part of bstr.
 *   (compatibility with historical functions).
 */
bool str_infix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;
    int ichar;
    char c0;

    if ((c0 = LOWER(astr[0])) == '\0')
    return false;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);

    for (ichar = 0; ichar <= sstr2 - sstr1; ichar++)
    {
    if (c0 == LOWER(bstr[ichar]) && !str_prefix(astr, bstr + ichar))
        return false;
    }

    return true;
}


/*
 * Compare strings, case insensitive, for suffix matching.
 * Return true if astr not a suffix of bstr
 *   (compatibility with historical functions).
 */
bool str_suffix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);
    if (sstr1 <= sstr2 && !str_cmp(astr, bstr + sstr2 - sstr1))
    return false;
    else
    return true;
}

void str_lower(register char *src,register char *dest)
{
    if(!dest) dest = src;
    while(*src) {
        *dest++ = LOWER(*src);
        ++src;
    }
    *dest = 0;
}

void str_upper(register char *src,register char *dest)
{
    if(!dest) dest = src;
    while(*src) {
        *dest++ = UPPER(*src);
        ++src;
    }
    *dest = 0;
}





/*
 * Returns an initial-capped string.
 */
char *capitalize(const char *str)
{
    static char strcap[8][MSL];
    static int i = 0;
    unichar_t cp;

    i = (i + 1) & 7;

    register char *w = strcap[i];
    if (*str)
    {
        // Make the first character uppercase
        cp = utf8_getchar(str);
        w = utf8_put(w, utf8_toupper(cp));
        str = utf8_nextchar(str);

        // Make the rest lowercase
        while(*str)
        {
            cp = utf8_getchar(str);
            w = utf8_put(w, utf8_tolower(cp));
            str = utf8_nextchar(str);
        }
    }
    *w = '\0';
    return strcap[i];
}


/*
 * Append a string to a file.
 */
void append_file(CHAR_DATA *ch, char *file, char *str)
{
    FILE *fp;
    char null_file_buf[MAX_INPUT_LENGTH];
    const char *null_file;

    if (IS_NPC(ch) || str[0] == '\0')
    return;

    fclose(fpReserve);
    if ((fp = fopen(file, "a")) == NULL)
    {
    perror(file);
    send_to_char("Could not open the file!\n\r", ch);
    }
    else
    {
    fprintf(fp, "[%8ld] %s: %s\n",
        ch->in_room ? ch->in_room->vnum : 0, ch->name, str);
    fclose(fp);
    }

    null_file = resolve_game_path(NULL_FILE, null_file_buf, sizeof(null_file_buf));
    fpReserve = fopen(null_file, "r");
    return;
}


void bug(const char *str, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list args;

    if (fpArea != NULL)
    {
    int iLine;
    int iChar;

    if (fpArea == stdin)
    {
        iLine = 0;
    }
    else
    {
        iChar = ftell(fpArea);
        fseek(fpArea, 0, 0);
        for (iLine = 0; ftell(fpArea) < iChar; iLine++)
        {
        while (getc(fpArea) != '\n')
            ;
        }
        fseek(fpArea, iChar, 0);
    }

    sprintf(buf, "[*****] FILE: %s LINE: %d", strArea, iLine);
    log_message(LOG_LEVEL_BUG, "sentience", buf);
            if (fBootDb && game_settings.note_boot_errors)
            boot_error_log("%s", buf);
    }

    strcpy(buf, "[*****] BUG: ");
    va_start(args, str);
    vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), str, args);
    va_end(args);

    pbug(LOG_ERROR, buf);
        if (fBootDb && game_settings.note_boot_errors)
        boot_error_log("%s", buf);
}


/*
 * Writes a string to the log.
 */
void log_string(const char *str)
{
    plog(LOG_INFO, str);
}

void log_stringf(const char *fmt,...)
{
    char buf[2 * MSL];
    va_list args;
    va_start (args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end (args);
    plog(LOG_INFO, buf);
}


/*
 * This function is here to aid in debugging.
 * If the last expression in a function is another function call,
 *   gcc likes to generate a JMP instead of a CALL.
 * This is called "tail chaining."
 * It hoses the debugger call stack for that call.
 * So I make this the last call in certain critical functions,
 *   where I really need the call stack to be right for debugging!
 *
 * If you don't understand this, then LEAVE IT ALONE.
 * Don't remove any calls to tail_chain anywhere.
 *
 * -- Furey
 */
void tail_chain(void)
{
    return;
}


/* Load reboot objects such as shards*/
void load_reboot_objs()
{
    int counter;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *pRoom;

    for (counter = 0; counter < 3; counter++)
    {
    pRoom = get_random_room(NULL, 0);
    if (!pRoom)
        continue;

    OBJ_INDEX_DATA *pObjIndex = get_reserved_obj_index("obj_black_moonstone_shard");
    if (!pObjIndex)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "load_reboot_objs: Cannot find black moonstone shard object.");
        continue;
    }
    obj = create_object(pObjIndex, 0, true);
    obj_to_room(obj, pRoom);
    }
}



NPC_SHIP_INDEX_DATA *get_npc_ship_index(long vnum)
{
#if 0
    NPC_SHIP_INDEX_DATA *npc_ship;

    for (npc_ship  = ship_index_hash[vnum % MAX_KEY_HASH];
      npc_ship != NULL;
      npc_ship  = npc_ship->next)
    {
    if (npc_ship->vnum == vnum)
        return npc_ship;
    }

    if (fBootDb)
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Get_npc_ship_index: bad vnum %ld.", vnum);
        return NULL;
    /*exit(1);*/
    }
#endif
    return NULL;
}


void reset_npc_sailing_boats()
{
#if 0
    NPC_SHIP_DATA *npc_ship;
    NPC_SHIP_INDEX_DATA *npc_ship_index;
    AREA_DATA *pArea;
    int i;
    long index;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resetting npc boats...");
    if ((pArea = get_wilderness_area()) == NULL)
    return;

/*
    if (plith_airship == NULL)
    {
     Plith airship is vnum 1*/
    npc_ship = create_npc_sailing_boat(1);
    plith_airship = npc_ship;

    /* Put in Town Square of Plith*/
    {
        ROOM_INDEX_DATA *airship_room = get_reserved_room_index("ROOM_VNUM_PLITH_AIRSHIP");
        if (!airship_room) {
            log_message(LOG_LEVEL_BUG, LOG_ERROR, "Resetting npc boats: reserved ROOM_VNUM_PLITH_AIRSHIP not found.");
        } else {
            obj_to_room(npc_ship->ship->ship, airship_room);
        }
    }
    }
*/

    for(i = 2; i <= top_vnum_npc_ship; i++) {
    npc_ship_index = ship_index_hash[ i ];

    if (npc_ship_index == NULL)
        continue;

    log_message(LOG_LEVEL_INFO, LOG_INIT, "Resetting boat");
    npc_ship = create_npc_sailing_boat(npc_ship_index->vnum);

    index = (long)((long)npc_ship_index->original_y *
                   (long)pArea->map_size_x +
               (long)npc_ship_index->original_x + (long)pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

    /* If the npc airship then set airship*/
    if (npc_ship->pShipData->npc_type == NPC_SHIP_AIR_SHIP)
    {
        plith_airship = npc_ship;
        {
            ROOM_INDEX_DATA *temple_room = get_reserved_room_index("room_default_recall");
            if (!temple_room) {
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Resetting npc boats: reserved room_default_recall not found.");
                continue;
            }
            index = temple_room->vnum;
        }
    }

    if (get_room_index(index) == NULL)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "While resetting npc boats, the room index was NULL (index %ld).", index);
        continue;
    }

    obj_to_room(npc_ship->ship->ship, get_room_index(index));
    }
#endif
}


AREA_DATA *get_wilderness_area()
{
    AREA_DATA *temp;

    for (temp = area_first; temp != NULL; temp = temp->next)
    {
    if (!str_cmp(temp->name, "Wilderness"))
        break;
    }

    if (temp == NULL)
    log_message(LOG_LEVEL_WARN, LOG_WARN, "Couldn't find area Wilderness.");

    return temp;
}

void do_memory(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int i = 0, i2 = 0, num_pcs = 0;
    float mb, mbf;
    CHAR_DATA *fch;
    ROOM_INDEX_DATA *room;
    MOB_INDEX_DATA *mob_i;
    OBJ_INDEX_DATA *obj_i;
    CHAT_ROOM_DATA *chat;
    AFFECT_DATA *af;
    EXIT_DATA *exit;
    DESCRIPTOR_DATA *d;
    AREA_DATA *area;
    /*EXTRA_DESCR_DATA *ed;*/
    /*RESET_DATA *reset;*/
    /* VIZZWILDS*/
    WILDS_DATA *wilds;
    WILDS_TERRAIN *terrain;
    WILDS_VLINK *vlink;
    ITERATOR it;

    send_to_char("{YType      Amt        MBytes          FreeAmt    FreeMBytes{x\n\r", ch);
    send_to_char("{Y----------------------------------------------------------{x\n\r", ch);
    /* basics */

    /* Descriptors */
    i = 0;
    for (d = descriptor_free; d != NULL; d = d->next) i++;
    mb = top_descriptor * (float)(sizeof(*d))/1000000;
    mbf = i * (float)(sizeof(*d))/1000000;
    sprintf(buf, "Descrp    %-10ld %-15.3f %-10d %-15.3f\n\r", top_descriptor, mb, i, mbf);
    send_to_char(buf, ch);

    /* rooms */
    i = 0;
    for (room = room_index_free; room != NULL; room = room->next) i++;
    mb = top_room * (float)(sizeof(*room))/1000000;
    mbf = i * (float)(sizeof(*room))/1000000;
    sprintf(buf, "Rooms     %-10ld %-15.3f %-10d %-15.3f\n\r", top_room, mb, i, mbf);
    send_to_char(buf, ch);

    /* Mobiles */
    i = 0;  i2 = 0;
    iterator_start(&it, loaded_chars);
    while(( fch = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (fch->pcdata != NULL)
            num_pcs++;
        else
            i++;
    }
    iterator_stop(&it);
    for (fch = char_free; fch != NULL; fch = fch->next) i2++;

    mb = i * (float)(sizeof(*fch))/1000000;
    mbf = i2 * (float)(sizeof(*fch))/1000000;
    sprintf(buf,  "Mobs      %-10d %-15.3f %-10d %-15.3f\n\r", i, mb, i2, mbf);
    send_to_char(buf ,ch);

    i = 0;
    for (af = affect_free; af != NULL; af = af->next) i++;
    mb = top_affect * (float)(sizeof(*af))/1000000;
    mbf = i * (float)(sizeof(*af))/1000000;
    sprintf(buf, "Affects   %-10ld %-15.3f %-10d %-15.3f\n\r", top_affect, mb, i, mbf);
    send_to_char(buf, ch);

    /* areas */
    i = 0;
    for (area = area_free; area != NULL; area = area->next) i++;
    mb = top_area * (float)(sizeof(*area))/1000000;
    mbf = i * (float)(sizeof(*area))/1000000;
    sprintf(buf, "Areas     %-10ld %-15.3f %-10d %-15.3f\n\r", top_area, mb, i, mbf);
    send_to_char(buf, ch);


    /*
     * Chat
     */
    /* chat rooms */
    i = 0;
    for (chat = chat_room_free; chat != NULL; chat = chat->next) i++;
    mb = top_chatroom * (float)(sizeof(*chat))/1000000;
    mb = i * (float)(sizeof(*chat))/1000000;
    sprintf(buf, "Chats     %-10ld %-15.3f %-10d %-15.3f\n\r", top_chatroom, mb, i, mbf);
    send_to_char(buf, ch);

    /*
     * OLC
     */

    /* mob index */
    i = 0;
    for (mob_i = mob_index_free; mob_i != NULL; mob_i = mob_i->next) i++;
    mb = top_mob_index * (float)(sizeof(*mob_i))/1000000;
    mbf = i * (float)(sizeof(*mob_i))/1000000;
    sprintf(buf, "Mobs_indx %-10ld %-15.3f %-10d %-15.3f\n\r", top_mob_index, mb, i, mbf);
    send_to_char(buf, ch);

    /* obj index */
    i = 0;
    for (obj_i = obj_index_free; obj_i != NULL; obj_i = obj_i->next) i++;
    mb = top_obj_index * (float)(sizeof(*obj_i))/1000000;
    mbf = i * (float)(sizeof(*obj_i))/1000000;
    sprintf(buf, "Obj_indx  %-10ld %-15.3f %-10d %-15.3f\n\r", top_obj_index, mb, i, mbf);
    send_to_char(buf, ch);

    /* Exits */
    i = 0;
    for (exit = exit_free; exit != NULL; exit = exit->next) i++;
    mb = top_exit * (float)(sizeof(*exit))/1000000;
    mbf = i * (float)(sizeof(*exit))/1000000;
    sprintf(buf, "Exits     %-10ld %-15.3f %-10d %-15.3f\n\r", top_exit, mb, i, mbf);
    send_to_char(buf, ch);

    /* VIZZWILDS*/
    /* wilds */
    i = 0;
    for (wilds = wilds_free; wilds != NULL; wilds = wilds->next) i++;
    mb = top_wilds * (float)(sizeof(*wilds))/1000000;
    mbf = i * (float)(sizeof(*wilds))/1000000;
    sprintf(buf, "Wilds     %-10ld %-15.3f %-10d %-15.3f\n\r", top_wilds, mb, i, mbf);
    send_to_char(buf, ch);

    /* wilds terrains */
    i = 0;
    for (terrain = wilds_terrain_free; terrain != NULL; terrain = terrain->next) i++;
    mb = top_wilds_terrain * (float)(sizeof(*terrain))/1000000;
    mbf = i * (float)(sizeof(*terrain))/1000000;
    sprintf(buf, "Terrains  %-10ld %-15.3f %-10d %-15.3f\n\r", top_wilds_terrain, mb, i, mbf);
    send_to_char(buf, ch);

    mb = top_wilds_vroom * (float)(sizeof(*room))/1000000;
    sprintf(buf, "Vrooms    %-10ld %-15.3f\n\r", top_wilds_vroom, mb);
    send_to_char(buf, ch);

    /* wilds vlinks */
    i = 0;
    for (vlink = wilds_vlink_free; vlink != NULL; vlink = vlink->next) i++;
    mb = top_wilds_vlink * (float)(sizeof(*vlink))/1000000;
    mbf = i * (float)(sizeof(*vlink))/1000000;
    sprintf(buf, "VLinks    %-10ld %-15.3f %-10d %-15.3f\n\r", top_wilds_vlink, mb, i, 	mbf);
    send_to_char(buf, ch);

    sprintf(buf, "Strings   %-10d %-15.3f\n\r", nAllocString, (float) sAllocString/1000000);
    send_to_char(buf, ch);

    send_to_char("{Y----------------------------------------------------------{x\n\r", ch);

    sprintf(buf, "Total     %-10d %-15.3f\n\r", nAllocPerm, (float) sAllocPerm/1000000);
    send_to_char(buf, ch);
}

/* @@@NIB : 20070123 : does what it says...*/
char *skip_whitespace(register char *str)
{
    if(str) while(ISSPACE(*str)) ++str;
    return str;
}

void strip_newline(char *buf, bool append)
{
    int len = strlen(buf);
    while(len > 0 && ISSPACE(buf[len-1])) --len;
    if(append) {
        buf[len++] = '\n';
        buf[len++] = '\r';
    }
    buf[len] = 0;
}

void check_area_versions(void)
{
    AREA_DATA *area;

    for (area = area_first; area; area = area->next)
    {
        migrate_shopkeeper_resets(area);
    }


    for (area = area_first; area; area = area->next) {
        if( IS_SET(area->area_flags, AREA_CHANGED) ||
            area->version_area != VERSION_AREA ||
            area->version_mobile != VERSION_MOBILE ||
            area->version_object != VERSION_OBJECT ||
            area->version_room != VERSION_ROOM ||
            area->version_token != VERSION_TOKEN ||
            area->version_script != VERSION_SCRIPT ||
            area->version_wilds != VERSION_WILDS) {

            area->version_area = VERSION_AREA;
            area->version_mobile = VERSION_MOBILE;
            area->version_object = VERSION_OBJECT;
            area->version_room = VERSION_ROOM;
            area->version_token = VERSION_TOKEN;
            area->version_script = VERSION_SCRIPT;
            area->version_wilds = VERSION_WILDS;

            REMOVE_BIT(area->area_flags, AREA_CHANGED);
            save_area_new(area);
        }
    }
}

char *fread_file(FILE *fp)
{
    char *plast;

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Fread_string_new: MAX_STRING %d exceeded.", MAX_STRING);
    exit(1);
    }

    while(!feof(fp) && !ferror(fp)) {
        *plast++ = getc(fp);
    }

    plast++;
    {
        union
        {
        char *	pc;
        char	rgc[sizeof(char *)];
        } u1;
        int ic;
        int iHash;
        char *pHash;
        char *pHashPrev;
        char *pString;

        plast[-1] = '\0';
        iHash     = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
        for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
        {
        for (ic = 0; ic < sizeof(char *); ic++)
            u1.rgc[ic] = pHash[ic];
        pHashPrev = u1.pc;
        pHash    += sizeof(char *);

        if (top_string[sizeof(char *)] == pHash[0]
            &&   !strcmp(top_string+sizeof(char *)+1, pHash+1))
            return pHash;
        }

        if (fBootDb)
        {
        pString		= top_string;
        top_string		= plast;
        u1.pc		= string_hash[iHash];
        for (ic = 0; ic < sizeof(char *); ic++)
            pString[ic] = u1.rgc[ic];
        string_hash[iHash]	= pString;

        nAllocString += 1;
        sAllocString += top_string - pString;
        return pString + sizeof(char *);
        }
        else
        {
        return str_dup(top_string + sizeof(char *));
        }
    }
}

char *fread_filename(char *filename)
{
    FILE *fp;
    char *str;

    if(!filename || !*filename || !(fp = fopen(filename,"r"))) return NULL;

    str = fread_file(fp);

    fclose(fp);

    return str;
}

ROOM_INDEX_DATA *create_virtual_room_nouid(ROOM_INDEX_DATA *source, bool objects, bool links, bool resets)
{
    ROOM_INDEX_DATA *vroom;
    EXTRA_DESCR_DATA *ed, *ed2;
    CONDITIONAL_DESCR_DATA *cd, *cd2;
    OBJ_DATA *obj, *obj2;
    EXIT_DATA *ex, *ex2;
    int door;

    if(!source) return NULL;

    if(source->source) source = source->source;

    if(IS_SET(source->room_flag[1], ROOM_NOCLONE)) return NULL;

    /* Can't clone a purely virtual room... ever*/
    if(IS_SET(source->room_flag[1],ROOM_VIRTUAL_ROOM)) return NULL;

    vroom = new_room_index();
    if(!vroom)
        return NULL;

    vroom->source = source;
    vroom->area = source->area;
    vroom->vnum = source->vnum;
    vroom->id[0] = 0;
    vroom->id[1] = 0;	/* ID will be assigned using the wrapper function or called by the persistant loader*/

    vroom->name = str_dup(source->name);
    vroom->description = str_dup(source->description);
    vroom->room_flag[0] = source->room_flag[0];
    vroom->room_flag[1] = source->room_flag[1] | ROOM_VIRTUAL_ROOM;
    REMOVE_BIT(vroom->room_flag[0], ROOM_CHAOTIC);                    // Clone/instance rooms must not be chaotic
    REMOVE_BIT(vroom->room_flag[1], ROOM_BLUEPRINT);					// Clones can never be "blueprint" rooms
    room_set_sector_type(vroom, room_sector_type(source));
    vroom->viewwilds = source->viewwilds;
    vroom->w = source->w;
    vroom->x = source->x;
    vroom->y = source->y;
    vroom->z = source->z;
    vroom->owner = str_dup("");
    vroom->heal_rate = source->heal_rate;
    vroom->mana_rate = source->mana_rate;
    vroom->move_rate = source->move_rate;
    vroom->visited = 0;

    /* Copy extra descriptions*/
    for(ed = source->extra_descr; ed; ed = ed->next) {
        ed2 = new_extra_descr();
        ed2->keyword = str_dup(ed->keyword);
        if( ed->description )
            ed2->description = str_dup(ed->description);
        else
            ed2->description = NULL;

        ed2->next = vroom->extra_descr;
        vroom->extra_descr = ed2;
    }

    /* Copy condition descriptions*/
    for(cd = source->conditional_descr; cd; cd = cd->next) {
        cd2 = new_conditional_descr();
        cd2->condition = cd->condition;
        cd2->description = str_dup(cd->description);
        cd2->phrase = cd->phrase;

        cd2->next = vroom->conditional_descr;
        vroom->conditional_descr = cd2;
    }

    /* Copy index variables*/
    variable_copylist(&source->index_vars,&vroom->progs->vars,false);
    variables_resolve_rsg_bindings(&vroom->progs->vars);

    {
        char *expanded_name = variables_expand_text_dup(vroom->progs->vars, vroom->name);
        char *expanded_desc = variables_expand_text_dup(vroom->progs->vars, vroom->description);
        free_string(vroom->name);
        vroom->name = expanded_name;
        free_string(vroom->description);
        vroom->description = expanded_desc;
    }

    for(ed2 = vroom->extra_descr; ed2; ed2 = ed2->next) {
        char *expanded_keyword = variables_expand_text_dup(vroom->progs->vars, ed2->keyword);
        free_string(ed2->keyword);
        ed2->keyword = expanded_keyword;
        if (ed2->description) {
            char *expanded_ed_desc = variables_expand_text_dup(vroom->progs->vars, ed2->description);
            free_string(ed2->description);
            ed2->description = expanded_ed_desc;
        }
    }

    for(cd2 = vroom->conditional_descr; cd2; cd2 = cd2->next) {
        char *expanded_cond_desc = variables_expand_text_dup(vroom->progs->vars, cd2->description);
        free_string(cd2->description);
        cd2->description = expanded_cond_desc;
    }

    for(door = 0; door < MAX_DIR; door++) {
        if ((ex2 = vroom->exit[door])) {
            char *expanded_keyword = variables_expand_text_dup(vroom->progs->vars, ex2->keyword);
            char *expanded_short = variables_expand_text_dup(vroom->progs->vars, ex2->short_desc);
            char *expanded_long = variables_expand_text_dup(vroom->progs->vars, ex2->long_desc);
            char *expanded_material = variables_expand_text_dup(vroom->progs->vars, ex2->door.material);
            free_string(ex2->keyword);
            ex2->keyword = expanded_keyword;
            free_string(ex2->short_desc);
            ex2->short_desc = expanded_short;
            free_string(ex2->long_desc);
            ex2->long_desc = expanded_long;
            free_string(ex2->door.material);
            ex2->door.material = expanded_material;
        }
    }

    /* If enabled, copy all contents */
    if(objects) {
        /* Clone non-takable objects, no contents though...*/
        for(obj = source->contents; obj; obj = obj->next_content) if(!CAN_WEAR(obj,ITEM_TAKE)) {
            obj2 = create_object(obj->pIndexData,0,false);
            clone_object(obj,obj2);

            obj_to_room(obj2,vroom);
        }
    }

    /* Create the exit link assignments... this will ONLY ever be used by blueprints*/
    /*  The links will only store the room vnums, akin to the boot process*/
    /*  All exits will be resolved after all rooms are created*/
    /*  Vlinks are stripped as well as non-environment exits leading nowhere*/
    if(links) {
        for(door = 0; door < MAX_DIR; door++)
            if((ex = source->exit[door]) &&
            !IS_SET(ex->exit_info,EX_VLINK) &&
            (IS_SET(ex->exit_info,EX_ENVIRONMENT) || ex->u1.to_room)) {

            vroom->exit[door] = ex2 = new_exit();

            ex2->u1.vnum = ex2->u1.to_room ? ex2->u1.to_room->vnum : 0;
            ex2->exit_info = ex->exit_info;
            ex2->keyword = str_dup(ex->keyword);
            ex2->short_desc = str_dup(ex->short_desc);
            ex2->long_desc = str_dup(ex->long_desc);
            ex2->rs_flags = ex->rs_flags;
            ex2->orig_door = ex->orig_door;
            ex2->door.strength = ex->door.strength;
            ex2->door.material = str_dup(ex->door.material);
            ex2->door.lock = ex->door.lock;
            ex2->door.rs_lock = ex->door.rs_lock;
            ex2->from_room = vroom;
        }
    }

    if(resets)
    {
        for(RESET_DATA *pResetOld = source->reset_first; pResetOld; pResetOld = pResetOld->next)
        {
            RESET_DATA *pResetNew = new_reset_data();

            pResetNew->command = pResetOld->command;
            pResetNew->arg1 = pResetOld->arg1;
            pResetNew->arg2 = pResetOld->arg2;
            pResetNew->arg3 = pResetOld->arg3;
            pResetNew->arg4 = pResetOld->arg4;

            add_reset(vroom, pResetNew, 0);
        }
    }

    vroom->next = source->clones;
    source->clones = vroom;

    // Copy persistance
    if(source->persist) persist_addroom(vroom);
    return vroom;
}

ROOM_INDEX_DATA *create_virtual_room(ROOM_INDEX_DATA *source,bool links, bool resets)
{
    ROOM_INDEX_DATA *vroom = create_virtual_room_nouid(source, true,links,resets);

    get_vroom_id(vroom);

    return vroom;
}

ROOM_INDEX_DATA *get_clone_room(register ROOM_INDEX_DATA *source, register unsigned long id1, register unsigned long id2)
{
    register ROOM_INDEX_DATA *room;
    if(!source) return NULL;

    if(source->source) return get_clone_room(source->source,id1,id2);

    //log_stringf("get_clone_room: search for %ld, %lu, %lu", source->vnum, id1, id2);

    /* Can't clone a purely virtual room... ever*/
    if(IS_SET(source->room_flag[1],ROOM_VIRTUAL_ROOM))
    {
        //log_string("get_clone_room: source is a virtual room");
        return NULL;
    }

    for(room = source->clones; room; room = room->next)
    {
        //log_stringf("get_clone_room: clone %lu, %lu", room->id[0], room->id[1]);

        if(room->id[0] == id1 && room->id[1] == id2)
        {
            //log_string("get_clone_room: clone found");
            return room;
        }
    }

    //log_string("get_clone_room: clone not found");

    return NULL;
}

bool room_is_clone(ROOM_INDEX_DATA *room)
{
    return (room && room->source);
}

bool extract_clone_room(ROOM_INDEX_DATA *room, unsigned long id1, unsigned long id2, bool destruct)
{
    ROOM_INDEX_DATA **rlink, *clone;
    ROOM_INDEX_DATA *dest, *environ;
    CHAR_DATA *ch, *ch_next;
    OBJ_DATA *obj, *obj_next;
    int door, rev_door;
//	char buf[MSL];

    if(!room) return false;

    if(room->source) room = room->source;

//	sprintf(buf,"extract_clone_room(%lu, %lu, %lu) called", room->vnum, id1, id2);
//	wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);


/*	if(room->vnum == 11001) return false;	  Oh, hell, no*/

    /* Remove the clone from the chain*/
    rlink = &room->clones;
    for(clone = room->clones; clone; rlink = &clone->next, clone = clone->next) {
        if(clone->id[0] == id1 && clone->id[1] == id2) {
            if(clone->progs) {
                if(clone->progs->script_ref > 0) {
                    clone->progs->extract_when_done = true;
                    return false;
                }
            }

            if(PROG_FLAG(clone,PROG_AT)) return false;
            *rlink = clone->next;
            break;
        }
    }

    if(!clone) {
//		sprintf(buf,"extract_clone_room(%lu, %lu, %lu) clone not found", room->vnum, id1, id2);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);
        return false;
    }

    if (clone->gc || list_hasdata(gc_rooms, clone))
        return true;

    /* Prevents infinite loops*/
    if(clone->progs && PROG_FLAG(clone,PROG_NODESTRUCT)) {
//		sprintf(buf,"extract_clone_room(%lu, %lu, %lu) clone already being destructed", room->vnum, id1, id2);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);
        return false;
    }

    /* Do extraction stuff*/
    if(clone->progs && room->progs && room->progs->progs) {
//		sprintf(buf,"extract_clone_room(%lu, %lu, %lu) calling EXTRACT trigger", room->vnum, id1, id2);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);
        SET_BIT(clone->progs->entity_flags,PROG_NODESTRUCT);
        p_percent_trigger(NULL, NULL, clone, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_EXTRACT, NULL);
    }

    /* Destroy all exits*/
    for(door = 0; door < MAX_DIR; door++) if(clone->exit[door]) {
//		sprintf(buf,"extract_clone_room(%lu, %lu, %lu) removing door %d", room->vnum, id1, id2, door);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);
        if((dest = clone->exit[door]->u1.to_room)) {
            rev_door = rev_dir[door];

            if(dest->exit[rev_door] && dest->exit[rev_door]->u1.to_room &&
                dest->exit[rev_door]->u1.to_room == clone) {
                free_exit(dest->exit[rev_door]);
                dest->exit[rev_door] = NULL;
            }
        }

        free_exit(clone->exit[door]);
        clone->exit[door] = NULL;
    }

    /* Dump objects into the environment*/
    if(destruct || clone->force_destruct) {
        for(obj = clone->contents; obj; obj = obj_next) {
            obj_next = obj->next_content;
            extract_obj(obj);
        }

        /* Transfer all players in the clone to its environment or the Beginning*/
        environ = get_environment(clone);
        if(!environ || environ == clone) environ = get_room_index(room->area, 11001);

        /* Emptying the room of players will not set off the wilderness check in char_from_room*/
        /*	since no wilderness room will EVER use this.*/
        for(ch = clone->people; ch; ch = ch_next) {
            ch_next = ch->next_in_room;

            if(IS_NPC(ch) && !ch->persist)
                extract_char(ch, true);
            else
            {
                char_from_room(ch);
                char_to_room(ch,environ);
            }
        }

    } else {
        // Dump all corpses or takable items to a special place
        environ = get_room_index(room->area, 8);		// FIXME: change this to a define

        while(clone->contents) {
            for(obj = clone->contents; obj; obj = obj_next) {
                obj_next = obj->next_content;

                if(obj->item_type == ITEM_CORPSE_NPC || obj->item_type == ITEM_CORPSE_PC || CAN_WEAR(obj, ITEM_TAKE)) {
                    obj_from_room(obj);
                    obj_to_room(obj,environ);
                } else {
                    extract_obj(obj);
                }
            }
        }

        /* Transfer all players in the clone to its environment or the Beginning*/
        environ = get_environment(clone);
        if(!environ || environ == clone) environ = get_room_index(room->area, 11001);

        /* Emptying the room of players will not set off the wilderness check in char_from_room*/
        /*	since no wilderness room will EVER use this.*/
        for(ch = clone->people; ch; ch = ch_next) {
            ch_next = ch->next_in_room;

            char_from_room(ch);
            char_to_room(ch,environ);
        }

    }

    clone->contents = NULL;
    clone->people = NULL;

    /* Remove from its environment*/
    room_from_environment(clone);
    list_appendlink(gc_rooms, clone);
    clone->gc = true;

//	sprintf(buf,"extract_clone_room(%lu, %lu, %lu) clone extracted", room->vnum, id1, id2);
//	wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);

    return true;
}

typedef struct clone_extract_task_data CLONE_EXTRACT_TASK;
struct clone_extract_task_data {
    CLONE_EXTRACT_TASK *next;
    ROOM_INDEX_DATA *room;
    unsigned long id1;
    unsigned long id2;
    bool destruct;
};

static CLONE_EXTRACT_TASK *clone_extract_task_head = NULL;
static CLONE_EXTRACT_TASK *clone_extract_task_tail = NULL;

void queue_clone_room_extract(ROOM_INDEX_DATA *room, unsigned long id1, unsigned long id2, bool destruct)
{
    CLONE_EXTRACT_TASK *task;

    if (!room)
        return;

    if (room->source)
        room = room->source;

    task = alloc_mem(sizeof(CLONE_EXTRACT_TASK));
    if (!task)
        return;

    task->next = NULL;
    task->room = room;
    task->id1 = id1;
    task->id2 = id2;
    task->destruct = destruct;

    if (clone_extract_task_tail)
        clone_extract_task_tail->next = task;
    else
        clone_extract_task_head = task;

    clone_extract_task_tail = task;
}

static int process_clone_extract_queue(struct timeval *start_time, long *elapsed_ms)
{
    struct timeval current_time;
    const long max_gc_time_ms = 5;
    const int max_gc_items = 100;
    int processed = 0;

    while (clone_extract_task_head && processed < max_gc_items)
    {
        CLONE_EXTRACT_TASK *task = clone_extract_task_head;

        gettimeofday(&current_time, NULL);
        *elapsed_ms = (current_time.tv_sec - start_time->tv_sec) * 1000 +
                      (current_time.tv_usec - start_time->tv_usec) / 1000;

        if (*elapsed_ms >= max_gc_time_ms)
            break;

        clone_extract_task_head = task->next;
        if (!clone_extract_task_head)
            clone_extract_task_tail = NULL;

        extract_clone_room(task->room, task->id1, task->id2, task->destruct);
        free_mem(task, sizeof(CLONE_EXTRACT_TASK));
        processed++;
    }

    gettimeofday(&current_time, NULL);
    *elapsed_ms = (current_time.tv_sec - start_time->tv_sec) * 1000 +
                  (current_time.tv_usec - start_time->tv_usec) / 1000;

    return processed;
}


//#if 0
//void fwrite_persist_obj_new(OBJ_DATA *obj, FILE *fp, int iNest)
//{
//	EXTRA_DESCR_DATA *ed;
//	AFFECT_DATA *paf;
//	char buf[MSL];

//	/*
//	* Slick recursion to write lists backwards,
//	* so loading them will load in forwards order.
//	*/
//	if (obj->next_content)
//		fwrite_persist_obj_new(obj->next_content, fp, iNest);

//	fprintf(fp, "#OBJECT\n");

//	fprintf(fp, "Vnum %ld\n", obj->pIndexData->vnum);
//	fprintf(fp, "UId %ld\n", obj->id[0]);
//	fprintf(fp, "UId2 %ld\n", obj->id[1]);
//	fprintf(fp, "Version %d\n", VERSION_OBJECT);

//	fprintf(fp, "Nest %d\n", iNest);

//	/* these data are only used if they do not match the defaults */
//	fprintf(fp, "Name %s~\n",		obj->name);
//	fprintf(fp, "ShD  %s~\n",		obj->short_descr);
//	fprintf(fp, "Desc %s~\n",		obj->description);
//	fprintf(fp, "FullD %s~\n",		fix_string(obj->full_description));
//	fprintf(fp, "ExtF %ld\n",		obj->extra[0]);
//	fprintf(fp, "Ext2F %ld\n",		obj->extra[1]);
//	fprintf(fp, "Ext3F %ld\n",		obj->extra[2]);
//	fprintf(fp, "Ext4F %ld\n",		obj->extra[3]);
//	fprintf(fp, "WeaF %d\n",		obj->wear_flags);
//	fprintf(fp, "Ityp %d\n",		obj->item_type);
//	fprintf(fp, "Room %ld\n",		obj->in_room->vnum);
//	fprintf(fp,"Enchanted_times %d\n",	obj->num_enchanted);
//	fprintf(fp, "Cond %d\n",		obj->condition);
//	fprintf(fp, "Fixed %d\n",		obj->times_fixed);
//	if (obj->owner)				fprintf(fp, "Owner %s~\n", obj->owner);
//	if (obj->old_short_descr)		fprintf(fp, "OldShort %s~\n", obj->old_short_descr);
//	if (obj->old_description)		fprintf(fp, "OldDescr %s~\n", obj->old_description);
//	if (obj->old_full_description)		fprintf(fp, "OldFullDescr %s~\n", obj->old_full_description);
//	if (obj->loaded_by)			fprintf(fp, "LoadedBy %s~\n", obj->loaded_by);
//	fprintf(fp, "Fragility %d\n",		obj->fragility);
//	fprintf(fp, "TimesAllowedFixed %d\n",	obj->times_allowed_fixed);
//	if (obj->locker)			fprintf(fp, "Locker %d\n", obj->locker);

//	/* variable data */
//	fprintf(fp, "Wear %d\n",		obj->wear_loc);
//	fprintf(fp, "LastWear %d\n",		obj->last_wear_loc);
//	fprintf(fp, "Lev  %d\n",		obj->level);
//	fprintf(fp, "Time %d\n",		obj->timer);
//	fprintf(fp, "Cost %ld\n",		obj->cost);

//	/* Legacy value-slot dump intentionally removed; type data is canonical. */

//	if (obj->spells)
//		save_spell(fp, obj->spells);

//	/* This is for spells on the objects.*/
//	for (paf = obj->affected; paf; paf = paf->next) {
//		if (paf->type < 0 || paf->type >= MAX_SKILL || paf->custom_name)
//			continue;

//		if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
//			if(!skill_table[paf->location - APPLY_SKILL].name) continue;
//			fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld\n",
//				skill_table[paf->type].name,
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				APPLY_SKILL,
//				skill_table[paf->location - APPLY_SKILL].name,
//				paf->bitvector);
//		} else {
//			fprintf(fp, "Affcg '%s' %3d %3d %3d %3d %3d %3d %10ld\n",
//				skill_table[paf->type].name,
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				paf->location,
//				paf->bitvector);
//		}
//	}

//	/* Custom named affects*/
//	for (paf = obj->affected; paf; paf = paf->next) {
//		if (!paf->custom_name) continue;

//		if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
//			if(!skill_table[paf->location - APPLY_SKILL].name) continue;
//			fprintf(fp, "Affcgn '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld\n",
//				paf->custom_name,
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				APPLY_SKILL,
//				skill_table[paf->location - APPLY_SKILL].name,
//				paf->bitvector);
//		} else {
//			fprintf(fp, "Affcgn '%s' %3d %3d %3d %3d %3d %3d %10ld\n",
//				paf->custom_name,
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				paf->location,
//				paf->bitvector);
//		}
//	}

//	/* for random affect eq*/
//	for (paf = obj->affected; paf; paf = paf->next) {
//		/* filter out "none" and "unknown" affects, as well as custom named affects */
//		if (paf->type != -1 || paf->custom_name != NULL ||
//			((paf->location < APPLY_SKILL || paf->location >= APPLY_SKILL_MAX) && !str_cmp(flag_string(apply_flags, paf->location), "none")))
//			continue;

//		if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
//			if(!skill_table[paf->location - APPLY_SKILL].name) continue;
//			fprintf(fp, "Affrg %3d %3d %3d %3d %3d %3d '%s' %10ld\n",
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				APPLY_SKILL,
//				skill_table[paf->location - APPLY_SKILL].name,
//				paf->bitvector);
//		} else {
//			fprintf(fp, "Affrg %3d %3d %3d %3d %3d %3d %10ld\n",
//				paf->where,
//				paf->group,
//				paf->level,
//				paf->duration,
//				paf->modifier,
//				paf->location,
//				paf->bitvector);
//		}
//	}

//	/* for catalysts*/
//	for (paf = obj->catalyst; paf; paf = paf->next) {
//		fprintf(fp, "Cata '%s' %3d %3d %3d\n",
//			flag_string( catalyst_types, paf->type ),
//			paf->level,
//			paf->modifier,
//			paf->duration);
//	}

//	for (ed = obj->extra_descr; ed != NULL; ed = ed->next) {
//		fprintf(fp, "ExDe %s~ %s~\n",
//		ed->keyword, ed->description);
//	}

//	if(obj->progs && obj->progs->vars) {
//		pVARIABLE var;

//		for(var = obj->progs->vars; var; var = var->next)
//			variable_fwrite(var,fp);
//	}

//	fprintf(fp, "#-OBJECT\n\n");

//	if (obj->contains)
//		fwrite_persist_obj_new(ch, obj->contains, fp, iNest + 1);


//}
//#endif

void persist_addmobile(register CHAR_DATA *mob)
{
    // Players are NOT allowed
    if( !IS_NPC(mob) ) return;

    if( list_hasdata(persist_mobs, mob)) return;

    if( !list_appendlink(persist_mobs, mob) ) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Failed to add mobile as persistant due to memory issues with 'list_appendlink'.");
        if( fBootDb )
            abort();
    } else
        mob->persist = true;
}

void persist_addobject(register OBJ_DATA *obj)
{
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "persist_addobject: Adding object %ld to persistance.", obj->pIndexData->vnum);

    if( list_hasdata(persist_objs, obj)) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "persist_addobject: Object %ld already in persistance.", obj->pIndexData->vnum);
        return;
    }

    if( !list_appendlink(persist_objs, obj) ) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Failed to add object as persistant due to memory issues with 'list_appendlink'.");
        if( fBootDb )
            abort();
    } else {
        obj->persist = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "persist_addobject: Object %ld flagged as persistant.", obj->pIndexData->vnum);
    }
}

void persist_addroom(register ROOM_INDEX_DATA *room)
{
    // Chat rooms cannot be made persistant
    if( room->chat_room || (room->area && room->area->area_who == AREA_CHAT) )
        return;

    // Clone rooms require the source to be flagged as clone persist
    if( room->source && !IS_SET(room->source->room_flag[1], ROOM_CLONE_PERSIST))
        return;

    if( list_hasdata(persist_rooms, room)) return;

    if( !list_appendlink(persist_rooms, room) ) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Failed to add room as persistant due to memory issues with 'list_appendlink'.");
        if( fBootDb )
            abort();
    } else
        room->persist = true;
}

void persist_removemobile(register CHAR_DATA *mob)
{
    list_remlink(persist_mobs, mob, false);
    mob->persist = false;
}

void persist_removeobject(register OBJ_DATA *obj)
{
    list_remlink(persist_objs, obj, false);
    obj->persist = false;
}

void persist_removeroom(register ROOM_INDEX_DATA *room)
{
    list_remlink(persist_rooms, room, false);
    room->persist = false;
}

void persist_save_scriptdata(FILE *fp, PROG_DATA *prog)
{
    pVARIABLE var;

    // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
    // log_stringf("%s: Saving variables...", __FUNCTION__);
    for( var = prog->vars; var; var = var->next) {
        // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
        // log_stringf("%s: Variable %s%s", __FUNCTION__, var->name, (var->save ? " - Saving...":""));
        if(var->save)
            variable_fwrite( var, fp );
    }
    // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
    // log_stringf("%s: Saving variables... Done.", __FUNCTION__);
}

void persist_save_location(FILE *fp, LOCATION *loc, char *prefix)
{
    fprintf(fp, "%s %ld %ld %ld %ld\n", prefix, loc->wuid, loc->id[0], loc->id[1], loc->id[2]);
}

void persist_save_token(FILE *fp, TOKEN_DATA *token)
{
    int i;

    /* recursion so that lists read in correct order instead of flipping */
    if ( token->next )
        persist_save_token (fp, token->next);

    fprintf(fp, "#TOKEN %ld\n", token->pIndexData->vnum);

    fprintf(fp, "UId %d\n", (int)token->id[0]);
    fprintf(fp, "UId2 %d\n", (int)token->id[1]);
    fprintf(fp, "Timer %d\n", token->timer);
    for (i = 0; i < MAX_TOKEN_VALUES; i++)
        fprintf(fp, "Value %d %ld\n", i, token->value[i]);

    if( token->progs )
        persist_save_scriptdata(fp,token->progs);

    fprintf(fp, "#-TOKEN\n\n");

}

void persist_save_object(FILE *fp, OBJ_DATA *obj, bool multiple)
{
    EXTRA_DESCR_DATA *ed;
    AFFECT_DATA *paf;

    if (multiple && obj->next_content)
        persist_save_object(fp, obj->next_content, multiple);

    // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
    // log_stringf("persist_save: saving object %08lX:%08lX.", obj->id[0], obj->id[1]);

    // Save all object information, including persistance (in case it is saved elsewhere)
    fprintf(fp, "#OBJECT %ld\n", obj->pIndexData->vnum);
    fprintf(fp, "Version %d\n", VERSION_OBJECT);				// **
    fprintf(fp, "UID %ld\n", obj->id[0]);					// **
    fprintf(fp, "UID2 %ld\n", obj->id[1]);					// **
    if(obj->persist) fprintf(fp, "Persist\n");				// **

    /* these data are only used if they do not match the defaults */
    fprintf(fp, "Name %s~\n", obj->name);					// **
    fprintf(fp, "ShortDesc %s~\n", obj->short_descr);			// **
    fprintf(fp, "LongDesc %s~\n", obj->description);			// **
    fprintf(fp, "FullDesc %s~\n", fix_string(obj->full_description));	// **
    fprintf(fp, "Extra %ld\n", obj->extra[0]);				// **
    fprintf(fp, "Extra2 %ld\n", obj->extra[1]);				// **
    fprintf(fp, "Extra3 %ld\n", obj->extra[2]);				// **
    fprintf(fp, "Extra4 %ld\n", obj->extra[3]);				// **
    fprintf(fp, "WearFlags %d\n", obj->wear_flags);				// **
    fprintf(fp, "ItemType %d\n", obj->item_type);				// **

    fprintf(fp, "PermExtra %ld\n", obj->extra_perm[0]);				// **
    fprintf(fp, "PermExtra2 %ld\n", obj->extra_perm[1]);				// **
    fprintf(fp, "PermExtra3 %ld\n", obj->extra_perm[2]);				// **
    fprintf(fp, "PermExtra4 %ld\n", obj->extra_perm[3]);				// **

    if( obj->item_type == ITEM_WEAPON )
        fprintf(fp, "PermWeapon %ld\n", obj->weapon_flags_perm);

    // Save location
    if (obj->in_room) {	// **
        if(obj->in_room->wilds)		fprintf(fp, "Vroom %ld %ld %ld\n", obj->in_room->wilds->uid, obj->in_room->x, obj->in_room->y);
        else if(obj->in_room->source)	fprintf(fp, "CloneRoom %ld %ld %ld\n", obj->in_room->source->vnum, obj->in_room->id[0], obj->in_room->id[1]);
        else				fprintf(fp, "Room %ld\n", obj->in_room->vnum);
    }

    fprintf(fp, "Enchanted %d\n", obj->num_enchanted);	// **
    fprintf(fp, "Weight %d\n", obj->weight);		// **
    fprintf(fp, "Cond %d\n", obj->condition);		// **
    fprintf(fp, "Fixed %d\n", obj->times_fixed);		// **

    if (obj->owner)			fprintf(fp, "Owner %s~\n", obj->owner);				// **
    if (obj->old_name)	fprintf(fp, "OldName %s~\n", obj->old_name);		// **
    if (obj->old_short_descr)	fprintf(fp, "OldShort %s~\n", obj->old_short_descr);		// **
    if (obj->old_description)	fprintf(fp, "OldDescr %s~\n", obj->old_description);		// **
    if (obj->old_full_description)	fprintf(fp, "OldFullDescr %s~\n", obj->old_full_description);	// **
    if (obj->loaded_by)		fprintf(fp, "LoadedBy %s~\n", obj->loaded_by);			// **

    fprintf(fp, "Fragility %d\n", obj->fragility);				// **
    fprintf(fp, "TimesAllowedFixed %d\n", obj->times_allowed_fixed);	// **
    if (obj->locker) fprintf(fp, "Locker\n");				// **

    fprintf(fp, "WearLoc %d\n", obj->wear_loc);				// **
    fprintf(fp, "LastWearLoc %d\n", obj->last_wear_loc);			// **
    fprintf(fp, "Level  %d\n", obj->level);					// **
    fprintf(fp, "Timer %d\n", obj->timer);					// **
    fprintf(fp, "Cost %ld\n", obj->cost);					// **

    /* Type-specific data (canonical, as JSON) — replaces legacy value[] */
    {
        json_t *td = obj_type_data_to_json(obj);
        if (td) {
            char *td_str = json_dumps(td, JSON_COMPACT | JSON_SORT_KEYS);
            if (td_str) {
                fprintf(fp, "TypeData %s~\n", td_str);
                free(td_str);
            }
            json_decref(td);
        }
    }

    if( obj->lock )
    {
        fprintf(fp, "Lock %ld '%s' %d\n",
            obj->lock->key_load.vnum,
            flag_string(lock_flags, obj->lock->flags),
            obj->lock->pick_chance);
    }

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

    if (obj->spells)
        save_spell(fp, obj->spells);		// SpellNew **

    // This is for spells on the objects.
    for (paf = obj->affected; paf != NULL; paf = paf->next) {
        if (paf->type < 0 || paf->type >= MAX_SKILL || paf->custom_name)
            continue;

        if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
            if(!skill_table[paf->location - APPLY_SKILL].name) continue;
            fprintf(fp, "AffObjSk '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
                skill_table[paf->type].name,
                paf->where,
                paf->group,
                paf->level,
                paf->duration,
                paf->modifier,
                APPLY_SKILL,
                skill_table[paf->location - APPLY_SKILL].name,
                paf->bitvector,
                paf->bitvector2);	// **
        } else {
            fprintf(fp, "AffObjSk '%s' %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
                skill_table[paf->type].name,
                paf->where,
                paf->group,
                paf->level,
                paf->duration,
                paf->modifier,
                paf->location,
                paf->bitvector,
                paf->bitvector2);	// **
        }
    }

    for (paf = obj->affected; paf != NULL; paf = paf->next) {
        if (!paf->custom_name) continue;

        if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
            if(!skill_table[paf->location - APPLY_SKILL].name) continue;
            fprintf(fp, "AffObjNm '%s' %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
                paf->custom_name,
                paf->where,
                paf->group,
                paf->level,
                paf->duration,
                paf->modifier,
                APPLY_SKILL,
                skill_table[paf->location - APPLY_SKILL].name,
                paf->bitvector,
                paf->bitvector2);	// **
        } else {
            fprintf(fp, "AffObjNm '%s' %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
                paf->custom_name,
                paf->where,
                paf->group,
                paf->level,
                paf->duration,
                paf->modifier,
                paf->location,
                paf->bitvector,
                paf->bitvector2);	// **
        }
    }

    // for random affect eq
    for (paf = obj->affected; paf != NULL; paf = paf->next) {
        /* filter out "none" and "unknown" affects, as well as custom named affects */
        if (paf->type != -1 || paf->custom_name != NULL
            || ((paf->location < APPLY_SKILL || paf->location >= APPLY_SKILL_MAX) && !str_cmp(flag_string(apply_flags, paf->location), "none")))
            continue;

        if(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX) {
            if(!skill_table[paf->location - APPLY_SKILL].name) continue;
                fprintf(fp, "AffMob %3d %3d %3d %3d %3d %3d '%s' %10ld %10ld\n",
                    paf->where,
                    paf->group,
                    paf->level,
                    paf->duration,
                    paf->modifier,
                    APPLY_SKILL,
                    skill_table[paf->location - APPLY_SKILL].name,
                    paf->bitvector,
                    paf->bitvector2);	// **
            } else {
                fprintf(fp, "AffMob %3d %3d %3d %3d %3d %3d %10ld %10ld\n",
                    paf->where,
                    paf->group,
                    paf->level,
                    paf->duration,
                    paf->modifier,
                    paf->location,
                    paf->bitvector,
                    paf->bitvector2);	// **
        }
    }

    // for catalysts
    for (CATALYST_DATA *cat = obj->catalyst; cat != NULL; cat = cat->next)
    {
        if( IS_NULLSTR(cat->custom_name) )
        {
            fprintf(fp, "%s '%s' %3d %3d %3d\n",
                ((cat->where == TO_CATALYST_ACTIVE) ? "CataA" : "Cata"),
                flag_string( catalyst_types, cat->type ),
                cat->level,
                cat->modifier,
                cat->duration);
        }
        else
        {
            fprintf(fp, "%s '%s' %3d %3d %3d %s\n",
                ((cat->where == TO_CATALYST_ACTIVE) ? "CataNA" : "CataN"),
                flag_string( catalyst_types, cat->type ),
                cat->level,
                cat->modifier,
                cat->duration,
                cat->custom_name);
        }
    }

    // Extra Descriptions
    for (ed = obj->extra_descr; ed; ed = ed->next)
    {
        if( ed->description )
            fprintf(fp, "ExDe %s~ %s~\n", ed->keyword, ed->description);
        else
            fprintf(fp, "ExDeEnv %s~\n", ed->keyword);
    }

    // Original Mob Owner Information (for corpses so far)
    if( !IS_NULLSTR(obj->owner_name) )
        fprintf(fp, "OwnerName %s~\n", obj->owner_name);

    if( !IS_NULLSTR(obj->owner_short) )
        fprintf(fp, "OwnerShort %s~\n", obj->owner_short);


    // Save Variables
    if( obj->progs )
        persist_save_scriptdata(fp,obj->progs);

    // Save Tokens
    if( obj->tokens )
        persist_save_token(fp, obj->tokens);

    if( obj->contains )
        persist_save_object(fp, obj->contains, true);

    fprintf(fp, "#-OBJECT\n\n");
}

void save_ship_crew(FILE *fp, SHIP_CREW_DATA *crew)
{
    fprintf(fp, "#CREW\n");
    fprintf(fp, "Scouting %d\n", crew->scouting);
    fprintf(fp, "Gunning %d\n", crew->gunning);
    fprintf(fp, "Oarring %d\n", crew->oarring);
    fprintf(fp, "Mechanics %d\n", crew->mechanics);
    fprintf(fp, "Navigation %d\n", crew->navigation);
    fprintf(fp, "Leadership %d\n", crew->leadership);
    fprintf(fp, "#-CREW\n");
}

void persist_save_mobile(FILE *fp, CHAR_DATA *ch)
{
    AFFECT_DATA *paf;
    int i = 0;
    ITERATOR it;
    OBJ_DATA *obj;

    fprintf(fp, "#MOBILE %ld\n", ch->pIndexData->vnum);
    fprintf(fp, "Version %d\n", VERSION_MOBILE);

    // Save all mobile information, including persistance
    // VERSION MUST ALWAYS BE THE FIRST FIELD!!!
    fprintf(fp, "Name %s~\n", ch->name);
    fprintf(fp, "UID   %ld\n", ch->id[0]);
    fprintf(fp, "UID2  %ld\n", ch->id[1]);
    if(ch->persist)
        fprintf(fp, "Persist\n");

    // Will eventually allow mobs to "die" like this instead of simply being extracted
    if (ch->dead) {
        fprintf(fp, "DeathTimeLeft %d\n", ch->time_left_death);
        fprintf(fp, "Dead\n");
        if(ch->recall.wuid)
            fprintf(fp, "RepopRoomW %lu %lu %lu %lu\n", 	ch->recall.wuid, ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
        else if(ch->recall.id[1] || ch->recall.id[2])
            fprintf(fp, "RepopRoomC %lu %lu %lu\n", 	ch->recall.id[0], ch->recall.id[1], ch->recall.id[2]);
        else
            fprintf(fp, "RepopRoom %ld\n", 	ch->recall.id[0]);
    }

    fprintf(fp, "Owner %s~\n", ch->owner);
    fprintf(fp, "ShD  %s~\n", ch->short_descr);
    fprintf(fp, "LnD  %s~\n", ch->long_descr);
    fprintf(fp, "Desc %s~\n", fix_string(ch->description));
    fprintf(fp, "Race %s~\n", ch->race ? ch->race->name : "human");
    fprintf(fp, "Sex  %d\n", ch->sex);
    fprintf(fp, "Levl %d\n", ch->level);
    fprintf(fp, "TLevl %d\n", ch->tot_level);


    // Save location
    if (ch->in_room) {	// **
        if(ch->in_room->wilds)		fprintf(fp, "Vroom %ld %ld %ld\n", ch->in_room->wilds->uid, ch->in_room->x, ch->in_room->y);
        else if(ch->in_room->source)	fprintf(fp, "CloneRoom %ld %ld %ld\n", ch->in_room->source->vnum, ch->in_room->id[0], ch->in_room->id[1]);
        else				fprintf(fp, "Room %ld\n", ch->in_room->vnum);
    } else {
        ROOM_INDEX_DATA *default_room = get_reserved_room_index("room_default");
        fprintf(fp, "Room %ld\n", default_room ? default_room->vnum : 0);
    }

    if (race_has_trait(ch->race, "toxin_system")) {
        for (i = 0; i < MAX_TOXIN; i++)
            fprintf(fp, "Toxn%s %d\n", toxin_table[i].name, ch->toxin[i]);
    }

    fprintf(fp, "HMV  %ld %ld %ld %ld %ld %ld\n",
        ch->hit, ch->max_hit, ch->mana, ch->max_mana, ch->move, ch->max_move);
    fprintf(fp, "ManaStore  %d\n", ch->manastore);

    fprintf(fp, "Gold %ld\n", UMAX(0,ch->gold));
    fprintf(fp, "Silv %ld\n", UMAX(0,ch->silver));

    fprintf(fp, "Pneuma %ld\n", ch->pneuma);
    fprintf(fp, "Home %ld\n", ch->home);
    fprintf(fp, "QuestPnts %d\n", ch->questpoints);
    fprintf(fp, "DeityPnts %ld\n", ch->deitypoints);

    fprintf(fp, "Exp  %ld\n", ch->exp);
    fprintf(fp, "Act  %s\n", print_flags(ch->act[0]));
    fprintf(fp, "Act2 %s\n", print_flags(ch->act[1]));
    fprintf(fp, "AfBy %s\n", print_flags(ch->affected_by[0]));
    fprintf(fp, "AfBy2 %s\n", print_flags(ch->affected_by[1]));
    fprintf(fp, "OffFlags %s\n", print_flags(ch->off_flags));
    fprintf(fp, "Immune %s\n", print_flags(ch->imm_flags));
    fprintf(fp, "ImmunePerm %s\n", print_flags(ch->imm_flags_perm));
    fprintf(fp, "Resist %s\n", print_flags(ch->res_flags));
    fprintf(fp, "ResistPerm %s\n", print_flags(ch->res_flags_perm));
    fprintf(fp, "Vuln %s\n", print_flags(ch->vuln_flags));
    fprintf(fp, "VulnPerm %s\n", print_flags(ch->vuln_flags_perm));
    fprintf(fp, "StartPos %d\n", ch->start_pos);
    fprintf(fp, "DefaultPos %d\n", ch->default_pos);

    fprintf(fp, "Parts %ld\n", ch->parts);
    fprintf(fp, "Size %d\n", ch->size);
    fprintf(fp, "Material %s~\n", (!ch->material[0] ? "Unknown" : ch->material));
    if (ch->corpse_type)
        fprintf(fp, "CorpseType %ld\n", (long int)ch->corpse_type);
    if (ch->corpse_load.vnum)
        fprintf(fp, "CorpseVnum %ld\n", ch->corpse_wnum.vnum);



    fprintf(fp, "Comm %s\n", print_flags(ch->comm));
    fprintf(fp, "Pos  %d\n", ch->position == POS_FIGHTING ? POS_STANDING : ch->position);
    fprintf(fp, "Prac %d\n", UMAX(0,ch->practice));
    fprintf(fp, "Trai %d\n", UMAX(0,ch->train));
    fprintf(fp, "Save  %d\n", ch->saving_throw);
    fprintf(fp, "Alig  %d\n", ch->alignment);
    fprintf(fp, "Hit   %d\n", ch->hitroll);
    fprintf(fp, "Dam   %d\n", ch->damroll);
    fprintf(fp, "ACs %d %d %d %d\n", ch->armour[0],ch->armour[1],ch->armour[2],ch->armour[3]);
    fprintf(fp, "Wimp  %d\n", UMAX(0,ch->wimpy));
    fprintf(fp, "Attr %d %d %d %d %d\n",
        ch->perm_stat[STAT_STR],
        ch->perm_stat[STAT_INT],
        ch->perm_stat[STAT_WIS],
        ch->perm_stat[STAT_DEX],
        ch->perm_stat[STAT_CON]);

    fprintf (fp, "AMod %d %d %d %d %d\n",
        ch->mod_stat[STAT_STR],
        ch->mod_stat[STAT_INT],
        ch->mod_stat[STAT_WIS],
        ch->mod_stat[STAT_DEX],
        ch->mod_stat[STAT_CON]);

    fprintf(fp, "LostParts  %s\n", print_flags(ch->lostparts));

    for (paf = ch->affected; paf != NULL; paf = paf->next) {
        if (!paf->custom_name && (paf->type < 0 || paf->type>= MAX_SKILL))
            continue;

        fprintf(fp, "%s '%s' '%s' %3d %3d %3d %3d %3d %10ld %10ld %3d\n",
            (paf->custom_name?"Affcgn":"Affcg"),
            (paf->custom_name?paf->custom_name:skill_table[paf->type].name),
            flag_string(affgroup_mobile_flags,paf->group),
            paf->where,
            paf->level,
            paf->duration,
            paf->modifier,
            paf->location,
            paf->bitvector,
            paf->bitvector2,
            paf->slot);
    }

    if( ch->shop )
        save_shop_new(fp, ch->shop);

    if( ch->crew )
        save_ship_crew(fp, ch->crew);

    // Save Variables
    if( ch->progs )
        persist_save_scriptdata(fp,ch->progs);

    // Save Tokens
    if( ch->tokens )
        persist_save_token(fp, ch->tokens);

    // Contents - iterate through lcarrying
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            persist_save_object(fp, obj, false);
        }
        iterator_stop(&it);
    }
    
    // Also save worn items
    if (ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            persist_save_object(fp, obj, false);
        }
        iterator_stop(&it);
    }

    fprintf(fp, "#-MOBILE\n\n");
}

void persist_save_exit(FILE *fp, EXIT_DATA *ex)
{
    LOCATION loc;

    // Skip wilderness exits
    if( IS_SET(ex->exit_info, EX_VLINK) )
        return;

    fprintf(fp, "#EXIT %s\n", dir_name[ex->orig_door]);

    if( ex->u1.to_room ) {
        location_from_room(&loc, ex->u1.to_room);
        if( location_isset( &loc ) )
            persist_save_location(fp, &loc, "DestRoom");
    } else if(ex->wilds.wilds_uid > 0) {
        fprintf(fp, "DestRoom %ld %d %d 0\n", ex->wilds.wilds_uid, ex->wilds.x, ex->wilds.y);
    }

    fprintf(fp, "Keyword %s~\n", ex->keyword);
    fprintf(fp, "ShortDesc %s~\n", ex->short_desc);
    fprintf(fp, "LongDesc %s~\n", ex->long_desc);

    fprintf(fp, "Flags %s~\n", flag_string(exit_flags, ex->exit_info));
    fprintf(fp, "ResetFlags %s~\n", flag_string(exit_flags, ex->rs_flags));

    fprintf(fp, "DoorLockReset %ld %s~ %d %ld %s~ %d %d\n",
        ex->door.lock.key_load.vnum,
        flag_string(lock_flags, ex->door.lock.flags),
        ex->door.lock.pick_chance,
        ex->door.rs_lock.key_load.vnum,
        flag_string(lock_flags, ex->door.rs_lock.flags),
        ex->door.rs_lock.pick_chance,
        ex->door.strength);
    if(!IS_NULLSTR(ex->door.material))
        fprintf(fp, "DoorMat %s~\n", ex->door.material);

    fprintf(fp, "#-EXIT\n\n");
}

void persist_save_room_environment(FILE *fp, ROOM_INDEX_DATA *clone)
{
    ROOM_INDEX_DATA *room;

    if(!clone || !room_is_clone(clone)) return;

    if(clone->environ_type == ENVIRON_ROOM) {
        if(clone->environ.room) {
            // The room must be persistent, wilds or static
            room = clone->environ.room;

            // Wilderness room
            if(room->wilds)
                fprintf(fp, "EnvironVROOM %ld %ld %ld %ld\n", room->wilds->uid, room->x, room->y, room->z);

            // Normal room
            else if(!room->source)
                fprintf(fp, "EnvironROOM %ld\n", room->vnum);

            // Persistent room (by here it *should* be a clone room, but be safe)
            else if(room->persist) {
                if(room->wilds)
                    fprintf(fp, "EnvironVROOM %ld %ld %ld %ld\n", room->wilds->uid, room->x, room->y, room->z);
                else if(room->source)
                    fprintf(fp, "EnvironCROOM %ld %ld %ld\n", room->source->vnum, room->id[0], room->id[1]);
                else
                    fprintf(fp, "EnvironROOM %ld\n", room->vnum);
            }
        }
    }

    else if(clone->environ_type == ENVIRON_MOBILE) {
        if(clone->environ.mob) {
            CHAR_DATA *mob = clone->environ.mob;
            fprintf(fp, "EnvironMOB %ld %ld\n", mob->id[0], mob->id[1]);
        }
    }

    else if(clone->environ_type == ENVIRON_OBJECT) {
        if(clone->environ.obj) {
            OBJ_DATA *obj = clone->environ.obj;
            fprintf(fp, "EnvironOBJ %ld %ld\n", obj->id[0], obj->id[1]);
        }
    }

    else if(clone->environ_type == ENVIRON_TOKEN) {
        if(clone->environ.token) {
            TOKEN_DATA *token = clone->environ.token;
            fprintf(fp, "EnvironTOK %ld %ld\n", token->id[0], token->id[1]);
        }
    }

    else if(clone->environ_type == -ENVIRON_ROOM && clone->environ.clone.source) {
        fprintf(fp, "EnvironCROOM %ld %ld %ld\n", clone->environ.clone.source->vnum, clone->environ.clone.id[0], clone->environ.clone.id[1]);
    }

    else if(clone->environ_type == -ENVIRON_MOBILE) {
        fprintf(fp, "EnvironMOB %ld %ld\n", clone->environ.clone.id[0], clone->environ.clone.id[1]);
    }

    else if(clone->environ_type == -ENVIRON_OBJECT) {
        fprintf(fp, "EnvironOBJ %ld %ld\n", clone->environ.clone.id[0], clone->environ.clone.id[1]);
    }

    else if(clone->environ_type == -ENVIRON_TOKEN) {
        fprintf(fp, "EnvironTOK %ld %ld\n", clone->environ.clone.id[0], clone->environ.clone.id[1]);
    }
}


void persist_save_room(FILE *fp, ROOM_INDEX_DATA *room)
{
    CHAR_DATA *ch;
    int i;
    SHIP_DATA *ship = get_room_ship(room);

    if( room->source ) {
        fprintf(fp, "#CROOM %ld %ld %ld\n", room->source->vnum, room->id[0], room->id[1]);		// **
        fprintf(fp, "XYZ %ld %ld %ld\n", room->x, room->y, room->z);					// **

        persist_save_room_environment(fp, room);
    } else if( room->wilds )
        fprintf(fp, "#VROOM %ld %ld %ld %ld\n", room->wilds->uid, room->x, room->y, room->z);		// **
    else {
        fprintf(fp, "#ROOM %ld\n", room->vnum);								// **
        fprintf(fp, "XYZ %ld %ld %ld\n", room->x, room->y, room->z);					// **
    }

    if(room->viewwilds)
        fprintf(fp, "ViewWilds %ld\n", room->viewwilds->uid);						// **

    fprintf(fp, "Name %s~\n", room->name);									// **
    fprintf(fp, "Desc %s~\n", fix_string(room->description));						// **
    if( !IS_NULLSTR(room->owner) )
        fprintf(fp, "Owner %s~\n", room->owner);					// **

    if(room->persist) fprintf(fp, "Persist\n");
    fprintf(fp, "Locale %ld\n", room->locale);
    fprintf(fp, "room_flags %s~\n", print_flags(room->room_flag[0]));
    fprintf(fp, "room_flags2 %s~\n", print_flags(room->room_flag[1]));
    fprintf(fp, "Sector %s~\n", print_flags(room_sector_type(room)));

    if (room->heal_rate != 100) fprintf(fp, "HealRate %d\n", room->heal_rate);
    if (room->mana_rate != 100) fprintf(fp, "ManaRate %d\n", room->mana_rate);
    if (room->move_rate != 100) fprintf(fp, "MoveRate %d\n", room->move_rate);

    if (location_isset(&room->recall))
        persist_save_location( fp, &room->recall, "RoomRecall" );

    // Resets?

    // Extra Descriptions?

    // Conditional Descriptions?

    // Save Exits
    for( i=0; i < MAX_DIR; i++)
        if( room->exit[i] )
            persist_save_exit(fp,room->exit[i]);

    // Save Variables
    if( room->progs )
        persist_save_scriptdata(fp,room->progs);

    // Save Tokens
    if( room->tokens )
        persist_save_token(fp, room->tokens);

    // Save Objects
    if( room->contents )
        persist_save_object(fp, room->contents, true);

    // Save NPCs
    for( ch = room->people; ch; ch = ch->next_in_room )
        if( IS_NPC(ch) )
        {
            // Exclude crew members of a ship as they will be handled separately
            if( !IS_VALID(ship) || !list_hasdata(ship->crew, ch) )
                persist_save_mobile(fp, ch);
        }

    fprintf(fp, "#-ROOM\n\n");
}

bool check_persist_environment( CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room )
{
    if( ch ) {
        if( !IS_NPC(ch) ) return true;	// It's a player

        if( ch->in_room && ch->in_room->persist) return true;

        return check_persist_environment( NULL, NULL, ch->in_room );

    } else if (obj) {
        if( obj->locker )
            return true;	// They are in some player's locker, thus on a player, which is a persistant environment
        else if( obj->in_obj ) {
            if( obj->in_obj->persist) return true;

            return check_persist_environment( NULL, obj->in_obj, NULL );
        } else if( obj->carried_by ) {
            if( !IS_NPC(obj->carried_by ) ) return true;	// Players are a special kind of persistance
            if( obj->carried_by->persist) return true;

            return check_persist_environment( obj->carried_by, NULL, NULL );
        } else if( obj->in_room ) {
            if( obj->in_room->persist) return true;

            return check_persist_environment( NULL, NULL, obj->in_room );
        }
    }

    return false;
}

// Rules for when a persistant entity is saved:
// * Persistant Entities are only saved if they are in a NON-PERSISTANT environment.
// * Persistant Objects save everything.
// * Persistant Mobiles save everything.
// * Persistant Rooms only save scripting information as well as conditional elements, as well as contents (objects and mobiles)
void persist_save(void)
{
    FILE *fp;
    char persist_file_buf[MAX_INPUT_LENGTH];
    const char *persist_file = resolve_game_path(PERSIST_FILE, persist_file_buf, sizeof(persist_file_buf));
    register CHAR_DATA *ch;
    register OBJ_DATA *obj;
    register ROOM_INDEX_DATA *room;
    ITERATOR it;

//  Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
//	log_stringf("persist_save: Saving persistance...");

    if (!(fp = fopen(persist_file, "w"))) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "persist.save: Couldn't open file.");
    } else {
        // Save objects
        iterator_start(&it, persist_objs);
        while(( obj = (OBJ_DATA *)iterator_nextdata(&it) )) {
            // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
            // log_stringf("persist_save: checking to save persistant object %08lX:%08lX.", obj->id[0], obj->id[1]);
            if( !check_persist_environment( NULL, obj, NULL) ) {
                persist_save_object(fp, obj, false);
            }
        }
        iterator_stop(&it);

        // Save mobiles
        iterator_start(&it, persist_mobs);
        while(( ch = (CHAR_DATA *)iterator_nextdata(&it) )) {
            if( !check_persist_environment( ch, NULL, NULL) )
                persist_save_mobile(fp, ch);
        }
        iterator_stop(&it);

        // Save rooms
        iterator_start(&it, persist_rooms);
        while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) )) {
            if( !check_persist_environment( NULL, NULL, room) )
                persist_save_room(fp, room);
        }
        iterator_stop(&it);


        fprintf(fp, "#END\n");
        fclose(fp);
    }

    /* Also save to JSON format */
    json_persist_save_all();

    // Removing persist_save and persist_save_scriptdata log lines as they're flooding the logs
    // log_stringf("persist_save: done.");
}

void persist_fix_environment_room(ROOM_INDEX_DATA *clone)
{
    ROOM_INDEX_DATA *room;
    ITERATOR it;
    iterator_start(&it, persist_rooms);
    while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) )) {
        if(room->environ_type == -ENVIRON_ROOM) {
            if(room->environ.clone.source == clone->source &&
                room->environ.clone.id[0] == clone->id[0] &&
                room->environ.clone.id[1] == clone->id[1]) {
                room->environ_type = ENVIRON_NONE;
                room_to_environment(room, NULL, NULL, clone, NULL);
            }
        }
    }
    iterator_stop(&it);
}

void persist_fix_environment_object(OBJ_DATA *obj)
{
    ROOM_INDEX_DATA *room;
    ITERATOR it;
    iterator_start(&it, persist_rooms);
    while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) )) {
        if(room->environ_type == -ENVIRON_OBJECT) {
            if(	room->environ.clone.id[0] == obj->id[0] &&
                room->environ.clone.id[1] == obj->id[1]) {
                room->environ_type = ENVIRON_NONE;
                room_to_environment(room, NULL, obj, NULL, NULL);
            }
        }
    }
    iterator_stop(&it);
}

void persist_fix_environment_mobile(CHAR_DATA *mob)
{
    ROOM_INDEX_DATA *room;
    ITERATOR it;
    iterator_start(&it, persist_rooms);
    while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) )) {
        if(room->environ_type == -ENVIRON_MOBILE) {
            if(	room->environ.clone.id[0] == mob->id[0] &&
                room->environ.clone.id[1] == mob->id[1]) {
                room->environ_type = ENVIRON_NONE;
                room_to_environment(room, mob, NULL, NULL, NULL);
            }
        }
    }
    iterator_stop(&it);
}


void persist_fix_environment_token(TOKEN_DATA *token)
{
    ROOM_INDEX_DATA *room;
    ITERATOR it;
    iterator_start(&it, persist_rooms);
    while(( room = (ROOM_INDEX_DATA *)iterator_nextdata(&it) )) {
        if(room->environ_type == -ENVIRON_TOKEN) {
            if(	room->environ.clone.id[0] == token->id[0] &&
                room->environ.clone.id[1] == token->id[1]) {
                room->environ_type = ENVIRON_NONE;
                room_to_environment(room, NULL, NULL, NULL, token);
            }
        }
    }
    iterator_stop(&it);
}


bool persist_load(void)
{
    log_message(LOG_LEVEL_INFO, LOG_INIT, "persist_load: loading persist entities...");

    /* Initialize JSON persist directory structure */
    if (!json_persist_init()) {
        perr(LOG_INIT, "Failed to initialise JSON persist directories.");
    }

    /* Load from JSON files */
    perr(LOG_INIT, "Attempting to load from JSON files...");
    if (json_persist_load_all()) {
        perr(LOG_INIT, "Successfully loaded from JSON files");
        return true;
    }

    log_message(LOG_LEVEL_ERROR, LOG_ERROR, "persist_load: failed to load persist data");
    return false;
}


bool save_instances()
{
    // Use new JSON format
    if (!json_save_instances()) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "Failed to save instances as JSON");
        return false;
    }
    
    return true;
}


void load_instances()
{
    char instances_json_buf[MAX_INPUT_LENGTH];
    const char *instances_json = resolve_game_path(INSTANCES_FILE_JSON, instances_json_buf, sizeof(instances_json_buf));

    /* Try persist directories first */
    int loaded = json_load_instances();
    if (loaded > 0) {
        log_stringf("Loaded %d entities from persist directories", loaded);
        resolve_ships();
        return;
    }

    /* Try monolithic instances.json */
    if (json_load_instances_file(instances_json)) {
        log_string("Loaded instances from monolithic JSON file");
        resolve_ships();

        log_string("Migrating instances to persist directory format...");
        json_save_instances();

        char old_path[256];
        snprintf(old_path, sizeof(old_path), "%s.old", instances_json);
        rename(instances_json, old_path);
        log_stringf("Archived old instances.json to %s", old_path);
        return;
    }

    log_message(LOG_LEVEL_BUG, LOG_ERROR, "No instances file found (tried persist dirs and .json)");
}


void send_boot_errors_to_coders()
{
    if (boot_error_len == 0)
        return;

    const size_t chunk_size = 3800; // Leave room for headers, etc.
    size_t offset = 0;
    int note_num = 1;

    // Get boot time string
    char timebuf[64];
    time_t now = current_time;
    struct tm *tm_info = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    while (offset < boot_error_len) {
        size_t len = (boot_error_len - offset > chunk_size) ? chunk_size : boot_error_len - offset;

        // Find the last newline within the chunk
        size_t end = offset + len;
        if (end < boot_error_len) {
            size_t last_nl = offset;
            for (size_t i = offset; i < end; ++i) {
                if (boot_error_buf[i] == '\n')
                    last_nl = i + 1;
            }
            // If we found a newline, break there; otherwise, use the chunk size
            if (last_nl > offset)
                end = last_nl;
        } else {
            end = boot_error_len;
        }

        size_t note_len = end - offset;
        char note_body[4000];
        if (note_len >= sizeof(note_body))
            note_len = sizeof(note_body) - 1;
        strncpy(note_body, boot_error_buf + offset, note_len);
        note_body[note_len] = '\0';

        // Compose and send the note
        NOTE_DATA *note = new_note();
        note->sender = str_dup("Boot System");
        note->to_staff_duties = str_dup("coder 'head coder'");

        char subj[256];
        snprintf(subj, sizeof(subj), "Boot Errors [%s] (Part %d)", timebuf, note_num);
        note->subject = str_dup(subj);
        note->date_stamp = current_time + note_num;
        note->text = str_dup(note_body);
        note->date = str_dup(timebuf);
        note->type = NOTE_NOTE;
        note->recipient_type = NOTE_RECIPIENT_STAFF_DUTY;

        append_note(note);

        offset = end;
        note_num++;
    }
    boot_error_len = 0;
    boot_error_buf[0] = '\0';
}


// Add these to global declarations
#define MAX_GC_TIME_PER_TICK 5 /* Maximum milliseconds to spend on GC per tick */
#define MAX_GC_ITEMS_PER_CATEGORY 100 /* Fallback max items if we can't measure time */

/*
 * Process pending garbage collection with time budgeting
 * Returns number of items processed
 */
int process_garbage_collection(void)
{
    struct timeval start_time, current_time;
    long elapsed_ms;
    int total_processed = 0;
    int processed;
    gc_calls++;
    
    // Start the timer
    gettimeofday(&start_time, NULL);

    // Process deferred clone extraction first (can be expensive)
    processed = process_clone_extract_queue(&start_time, &elapsed_ms);
    total_processed += processed;
    if (elapsed_ms >= MAX_GC_TIME_PER_TICK)
        return total_processed;
    
    // Process tokens first (usually lightweight)
    processed = 0;
    if (list_size(gc_tokens) > 0) {
        ITERATOR it;
        TOKEN_DATA *token;
        LLIST *temp_list = list_create(false);
        
        iterator_start(&it, gc_tokens);
        while ((token = (TOKEN_DATA *)iterator_nextdata(&it))) {
            // Check time budget
            gettimeofday(&current_time, NULL);
            elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
            
            if (elapsed_ms >= MAX_GC_TIME_PER_TICK || processed >= MAX_GC_ITEMS_PER_CATEGORY)
                break;
                
            list_appendlink(temp_list, token);
            processed++;
        }
        iterator_stop(&it);
        
        // Free the collected tokens
        iterator_start(&it, temp_list);
        while ((token = (TOKEN_DATA *)iterator_nextdata(&it))) {
            list_remlink(gc_tokens, token, false);
            free_token(token);
        }
        iterator_stop(&it);
        
        list_destroy(temp_list);
        total_processed += processed;
        
        // Early return if time budget exceeded
        if (elapsed_ms >= MAX_GC_TIME_PER_TICK)
            return total_processed;
    }
    
    // Process objects next (medium weight)
    processed = 0;
    if (list_size(gc_objects) > 0) {
        ITERATOR it;
        OBJ_DATA *obj;
        LLIST *temp_list = list_create(false);
        
        iterator_start(&it, gc_objects);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            // Check time budget
            gettimeofday(&current_time, NULL);
            elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
            
            if (elapsed_ms >= MAX_GC_TIME_PER_TICK || processed >= MAX_GC_ITEMS_PER_CATEGORY)
                break;
                
            list_appendlink(temp_list, obj);
            processed++;
        }
        iterator_stop(&it);
        
        // Free the collected objects
        iterator_start(&it, temp_list);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            list_remlink(gc_objects, obj, false);
            free_obj(obj);
        }
        iterator_stop(&it);
        
        list_destroy(temp_list);
        total_processed += processed;
        
        // Early return if time budget exceeded
        if (elapsed_ms >= MAX_GC_TIME_PER_TICK)
            return total_processed;
    }
    
    // Process rooms next
    processed = 0;
    if (list_size(gc_rooms) > 0) {
        ITERATOR it;
        ROOM_INDEX_DATA *room;
        LLIST *temp_list = list_create(false);
        
        iterator_start(&it, gc_rooms);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it))) {
            // Check time budget
            gettimeofday(&current_time, NULL);
            elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
            
            if (elapsed_ms >= MAX_GC_TIME_PER_TICK || processed >= MAX_GC_ITEMS_PER_CATEGORY)
                break;
                
            list_appendlink(temp_list, room);
            processed++;
        }
        iterator_stop(&it);
        
        // Free the collected rooms
        iterator_start(&it, temp_list);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it))) {
            list_remlink(gc_rooms, room, false);
            free_room_index(room);
        }
        iterator_stop(&it);
        
        list_destroy(temp_list);
        total_processed += processed;
        
        // Early return if time budget exceeded
        if (elapsed_ms >= MAX_GC_TIME_PER_TICK)
            return total_processed;
    }
    
    // Process mobs last (heaviest)
    processed = 0;
    if (list_size(gc_mobiles) > 0) {
        ITERATOR it;
        CHAR_DATA *mob;
        LLIST *temp_list = list_create(false);
        
        iterator_start(&it, gc_mobiles);
        while ((mob = (CHAR_DATA *)iterator_nextdata(&it))) {
            // Check time budget
            gettimeofday(&current_time, NULL);
            elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000;
            
            if (elapsed_ms >= MAX_GC_TIME_PER_TICK || processed >= MAX_GC_ITEMS_PER_CATEGORY)
                break;
                
            list_appendlink(temp_list, mob);
            processed++;
        }
        iterator_stop(&it);
        
        // Free the collected mobs
        iterator_start(&it, temp_list);
        while ((mob = (CHAR_DATA *)iterator_nextdata(&it))) {
            list_remlink(gc_mobiles, mob, false);
            free_char(mob);
        }
        iterator_stop(&it);
        
        list_destroy(temp_list);
        total_processed += processed;
    }

gc_total_processed += total_processed;
gettimeofday(&current_time, NULL);
elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 + 
             (current_time.tv_usec - start_time.tv_usec) / 1000;
if (elapsed_ms > gc_max_time) 
    gc_max_time = elapsed_ms;
    
return total_processed;
}