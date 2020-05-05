#ifndef __MERC_H__
#define __MERC_H__

// 2014-05-21 NIB - comment this to return SHOWDAMAGE functionality to immortals and testport only
#define DEBUG_ALLOW_SHOW_DAMAGE

#define DEBUG_LINES
#ifdef DEBUG_LINES
#define __D__ log_stringf("%s:%s:%d",__FILE__, __func__,__LINE__);
#else
#define __D__
#endif

/**************************************************************************
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

/*@@@NIB These should be in the standard header for ALL files...*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "protocol.h"
#include "sha256.h"

#define args( list )			list
#define DECLARE_DO_FUN( fun )		DO_FUN    fun
#define DECLARE_SPEC_FUN( fun )		SPEC_FUN  fun
#define DECLARE_SPELL_FUN( fun )	SPELL_FUN fun
#define DECLARE_OBJ_FUN( fun )		OBJ_FUN	  fun
#define DECLARE_ROOM_FUN( fun )		ROOM_FUN  fun
#define SPELL_FUNC(s)	bool s (int sn, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc)



/* System calls */
int unlink();
int system();

#if	!defined(FALSE)
#define FALSE	 0
#endif

#if	!defined(TRUE)
#define TRUE	 1
#endif

#define TRISTATE 2

#if	!defined(false)
#define false	 0
#endif

#if	!defined(true)
#define true	 1
#endif

#if	defined(_AIX)
typedef int				sh_int;
typedef int				bool;
#define unix
#else
typedef short   int			sh_int;
typedef unsigned char			bool;
#endif

#define TELOPT_COMPRESS 	85

#define COMPRESS_BUF_SIZE 	16384

/* MSP: http://www.zuggsoft.com/zmud/msp.htm */
#define DESCRIPTOR_MSP          A

struct sound_type {
    int sound;
    char *tag;
};

/* Combat */
#define         SOUND_HIT_1		0
#define         SOUND_HIT_2		1
#define         SOUND_HIT_3		2
#define         SOUND_MISS_0		3
#define         SOUND_MISS_1		4
#define         SOUND_PARRY_0		5
#define         SOUND_BREATH    	6
#define         SOUND_PUNCH_1   	7
#define         SOUND_PUNCH_2   	8
#define         SOUND_PUNCH_3   	9
/* Ambient */
#define         SOUND_CITY      	10
#define         SOUND_BIRD_1    	11
#define         SOUND_NIGHT    	 	12
#define         SOUND_RAIN      	13
#define         SOUND_STORM     	14
#define         SOUND_LIGHTNING 	15
#define         SOUND_WIND      	16
#define         SOUND_UNDERWATER 	17
#define         SOUND_WOLVES 		18
#define         SOUND_ROOSTER 		19
#define         SOUND_FIRE    		20
/* Misc */
#define         SOUND_SKULL   		21
#define         SOUND_SPLASH  		22
/* Ships */
#define         SOUND_SINK    		23
#define         SOUND_CANNON  		24
#define         SOUND_CANNON_SPLASH 	25
#define         SOUND_CANNON_EXPLODE 	26
/* Deaths */
#define         SOUND_MALE     		27
#define         SOUND_FEMALE   		28
#define         SOUND_TELEPORT  	29
#define         SOUND_BIRD_2    	30
#define         SOUND_BIRD_3    	31
#define         SOUND_OWL       	32
#define         SOUND_OFF       	33

/*
 * String and memory management parameters.
 */
#define	MAX_KEY_HASH		 9204
#define MAX_STRING_LENGTH	 4608
#define MAX_INPUT_LENGTH	 256
#define PAGELEN			 22

#define MSL MAX_STRING_LENGTH
#define MIL MAX_INPUT_LENGTH

/* Purge version - anything that is below this version should be considered invalid and to be wiped
	Useful for players that are too different */
#define VERSION_DB_PURGE	0x00000000
#define VERSION_AREA_PURGE	0x00000000
#define VERSION_MOBILE_PURGE	0x00000000
#define VERSION_OBJECT_PURGE	0x00000000
#define VERSION_ROOM_PURGE	0x00000000
#define VERSION_PLAYER_PURGE	0x00000000
#define VERSION_TOKEN_PURGE	0x00000000
#define VERSION_AFFECT_PURGE	0x00000000
#define VERSION_SCRIPT_PURGE	0x00000000
#define VERSION_WILDS_PURGE	0x00000000

/* Base version - initial verison when version isn't present */
#define VERSION_DB_000		0x00FFFFFF
#define VERSION_AREA_000	0x00FFFFFF
#define VERSION_MOBILE_000	0x00FFFFFF
#define VERSION_OBJECT_000	0x00FFFFFF
#define VERSION_ROOM_000	0x00FFFFFF
#define VERSION_PLAYER_000	0x00FFFFFF
#define VERSION_TOKEN_000	0x00FFFFFF
#define VERSION_AFFECT_000	0x00FFFFFF
#define VERSION_SCRIPT_000	0x02000000
#define VERSION_WILDS_000	0x00FFFFFF

#define VERSION_GAME		"DEV 0.0.1"

#define VERSION_DB_001		0x01000001

#define VERSION_AREA_001	0x01000001

#define VERSION_AREA_002	0x01000001
//	Change #1: Forces the AREA_NEWBIE flag on Alendith

#define VERSION_AREA_003	0x01000002
//	Change #2: Migrate shopkeeper resets over to shop stock data

#define VERSION_MOBILE_001	0x01000001
//  Change #1: Update to affects to include object worn location

#define VERSION_PLAYER_001	0x01000000

#define VERSION_PLAYER_002	0x01000001
//	Change #1: Automatic conversion of the spell name WITHER into WITHERING CLOUD

#define VERSION_PLAYER_003	0x01000002
//  Change #1: If a player's locker rent exists and is expired, forgive it.

#define VERSION_PLAYER_004	0x01000003
//  Change #1: Update to affects to include object worn location

#define VERSION_PLAYER_005	0x01000004
//	Change #1: Existin Immortals will have HOLYWARP turned on automagically


#define VERSION_OBJECT_001	0x01000000

#define VERSION_OBJECT_002	0x01000001
//	Change #1: Initializes objects to use the perm values for flags manipulated by affects

#define VERSION_OBJECT_003	0x01000002
//  Change #1: Added bitvector2 to affect output

#define VERSION_OBJECT_004	0x01000003
//	Change #1: lock states

#define VERSION_ROOM_001	0x01000001
//  Change #1: lock states

#define VERSION_ROOM_002	0x01000002
//	Change #2: forgot persistent exits

#define VERSION_DB			VERSION_DB_001
#define VERSION_AREA		VERSION_AREA_003
#define VERSION_MOBILE		0x01000000
#define VERSION_OBJECT		VERSION_OBJECT_004
#define VERSION_ROOM		VERSION_ROOM_002
#define VERSION_PLAYER		VERSION_PLAYER_005
#define VERSION_TOKEN		0x01000000
#define VERSION_AFFECT		0x01000000
#define VERSION_SCRIPT		0x02000000
#define VERSION_WILDS		0x01000000
#define VERSION_DATABASE	0x01000000

/* Structures */
typedef struct	affect_data		AFFECT_DATA;
typedef struct	area_data		AREA_DATA;
typedef struct	auction_data		AUCTION_DATA;
typedef struct	auto_war		AUTO_WAR;
typedef struct	ban_data		BAN_DATA;
typedef struct	bounty_data		BOUNTY_DATA;
typedef struct	char_data		CHAR_DATA;
typedef struct  event_data		EVENT_DATA;
typedef struct	invasion_quest		INVASION_QUEST;
typedef struct	chat_room_data		CHAT_ROOM_DATA;
typedef struct	descriptor_data		DESCRIPTOR_DATA;
typedef struct	exit_data		EXIT_DATA;
typedef struct	destination_data	DESTINATION_DATA;
typedef struct	extra_descr_data	EXTRA_DESCR_DATA;
typedef struct	global_data		GLOBAL_DATA;
typedef struct	help_category		HELP_CATEGORY;
typedef struct	help_data		HELP_DATA;
typedef struct	mob_index_data		MOB_INDEX_DATA;
typedef struct	note_data		NOTE_DATA;
typedef struct	npc_ship_data		NPC_SHIP_DATA;
typedef struct	npc_ship_hotspot_data	NPC_SHIP_HOTSPOT_DATA;
typedef struct	npc_ship_hotspot_type	NPC_SHIP_HOTSPOT_TYPE;
typedef struct	npc_ship_index_data	NPC_SHIP_INDEX_DATA;
typedef struct	obj_data		OBJ_DATA;
typedef struct	obj_index_data		OBJ_INDEX_DATA;
typedef struct	spell_data		SPELL_DATA;
typedef struct	pc_data			PC_DATA;
typedef struct	questor_data	QUESTOR_DATA;
typedef struct	quest_data		QUEST_DATA;
typedef struct	quest_part_data		QUEST_PART_DATA;
typedef struct	reset_data		RESET_DATA;
typedef struct	room_index_data		ROOM_INDEX_DATA;
typedef struct	ship_crew_data		SHIP_CREW_DATA;
typedef struct	ship_data		SHIP_DATA;
typedef struct	shop_stock_data	SHOP_STOCK_DATA;
typedef struct	shop_data		SHOP_DATA;
typedef struct	shop_request_data	SHOP_REQUEST_DATA;
typedef struct	time_info_data		TIME_INFO_DATA;
typedef struct	trade_area_data		TRADE_AREA_DATA;
typedef struct	storm_data		STORM_DATA;
typedef struct	trade_item		TRADE_ITEM;
typedef struct	trade_type		TRADE_TYPE;
typedef struct	waypoint_data		WAYPOINT_DATA;
typedef struct 	buf_type	 	BUFFER;
typedef struct  ambush_data             AMBUSH_DATA;
typedef struct  chat_ban_data		CHAT_BAN_DATA;
typedef struct  chat_op_data		CHAT_OP_DATA;
typedef struct  church_data             CHURCH_DATA;
typedef struct  church_player_data      CHURCH_PLAYER_DATA;
typedef struct	church_treasure_room_data	CHURCH_TREASURE_ROOM;
typedef struct  conditional_descr_data  CONDITIONAL_DESCR_DATA;
typedef struct  gq_data			GQ_DATA;
typedef struct  gq_mob_data		GQ_MOB_DATA;
typedef struct  gq_obj_data		GQ_OBJ_DATA;
typedef struct  ignore_data		IGNORE_DATA;
typedef struct  mail_data		MAIL_DATA;
typedef struct  prog_data		PROG_DATA;
typedef struct  prog_code               PROG_CODE;
typedef struct  prog_list              	PROG_LIST;
typedef struct  quest_index_data        QUEST_INDEX_DATA;
typedef struct  quest_index_part_data   QUEST_INDEX_PART_DATA;
typedef struct  quest_list		QUEST_LIST;
typedef struct  stat_data		STAT_DATA;
typedef struct  string_data		STRING_DATA; /* for lists of strings */
typedef struct	weather_data		WEATHER_DATA;
typedef struct  token_index_data	TOKEN_INDEX_DATA;
typedef struct  token_data		TOKEN_DATA;
typedef struct  command_data		COMMAND_DATA;
typedef struct  project_data		PROJECT_DATA;
typedef struct  project_builder_data	PROJECT_BUILDER_DATA;
typedef struct  project_inquiry_data	PROJECT_INQUIRY_DATA;
typedef struct  immortal_data		IMMORTAL_DATA;
typedef struct	log_entry_data		LOG_ENTRY_DATA;
typedef struct  string_vector_data	STRING_VECTOR;
typedef struct mob_index_skill_data MOB_INDEX_SKILL_DATA;
typedef struct mob_skill_data MOB_SKILL_DATA;
typedef struct list_type LLIST;
typedef struct list_link_type LLIST_LINK;
typedef struct list_link_area_data LLIST_AREA_DATA;
typedef struct list_link_wilds_data LLIST_WILDS_DATA;
typedef struct list_link_uid_data LLIST_UID_DATA;
typedef struct list_link_room_data LLIST_ROOM_DATA;
typedef struct list_link_exit_data LLIST_EXIT_DATA;
typedef struct list_link_skill_data LLIST_SKILL_DATA;
typedef struct iterator_type ITERATOR;

typedef struct olc_point_category_type POINT_CATEGORY;
typedef struct olc_point_data OLC_POINT_DATA;
typedef struct olc_point_boost_data OLC_POINT_BOOST;
typedef struct olc_point_usage_data OLC_POINT_USAGE;
typedef struct olc_point_area_data OLC_POINT_AREA;

typedef struct dice_data DICE_DATA;

/* VIZZWILDS */
typedef struct    wilds_vlink      WILDS_VLINK;
typedef struct    wilds_data       WILDS_DATA;
typedef struct    wilds_terrain    WILDS_TERRAIN;

// Blueprints
typedef struct blueprint_link_data BLUEPRINT_LINK;
typedef struct blueprint_section_data BLUEPRINT_SECTION;
typedef struct static_blueprint_link STATIC_BLUEPRINT_LINK;
typedef struct blueprint_data BLUEPRINT;
typedef struct instance_section_data INSTANCE_SECTION;
typedef struct instance_data INSTANCE;

// Dungeons
typedef struct dungeon_index_special_room_data DUNGEON_INDEX_SPECIAL_ROOM;
typedef struct dungeon_floor_data DUNGEON_FLOOR_DATA;
typedef struct dungeon_index_data DUNGEON_INDEX_DATA;
typedef struct dungeon_data DUNGEON;

#define MEMTYPE_MOB		'M'
#define MEMTYPE_OBJ		'O'
#define MEMTYPE_ROOM	'R'
#define MEMTYPE_TOKEN	'T'
#define MEMTYPE_AREA	'A'

#define IS_MEMTYPE(ptr,typ)		(ptr ? (*((char *)((void *)(ptr))) == (typ)) : FALSE)
#define SET_MEMTYPE(ptr,typ)	(ptr)->__type = (typ)

#define MAX_TEMPSTORE	4

/* Functions */
typedef	void DO_FUN	(CHAR_DATA *ch, char *argument);
typedef bool SPEC_FUN	(CHAR_DATA *ch);
typedef bool SPELL_FUN	(int sn, int level, CHAR_DATA *ch, void *vo, int target, int obj_wear_loc);
typedef void OBJ_FUN	(OBJ_DATA *obj, char *argument);
typedef void ROOM_FUN	(ROOM_INDEX_DATA *room, char *argument);
typedef bool CHAR_TEST	(CHAR_DATA *ch, CHAR_DATA *ach, CHAR_DATA *bch);	/* NIB : 20070122 : For act_new( ) */
typedef bool VISIT_FUNC (ROOM_INDEX_DATA *room, void *argv[], int argc, int depth, int door);
typedef void *LISTCOPY_FUNC (void *src);
typedef void LISTDESTROY_FUNC (void *data);

typedef struct ifcheck_data IFCHECK_DATA;
typedef struct script_data SCRIPT_DATA;
typedef struct script_code SCRIPT_CODE;
typedef struct script_varinfo SCRIPT_VARINFO;
typedef struct script_control_block SCRIPT_CB;
typedef struct script_parameter SCRIPT_PARAM;
typedef bool (*IFC_FUNC)(SCRIPT_VARINFO *info, CHAR_DATA *mob,OBJ_DATA *obj,ROOM_INDEX_DATA *room, TOKEN_DATA *token, int *ret,int argc,SCRIPT_PARAM **argv);
typedef bool (*OPCODE_FUNC)(SCRIPT_CB *block);
typedef struct entity_field_type ENT_FIELD;
typedef struct script_var_type VARIABLE, *pVARIABLE, **ppVARIABLE;
typedef struct script_boolexp BOOLEXP;

typedef struct location_type {
	unsigned long wuid;
	unsigned long id[3];

	/* if wuid!=0, wilds reference
	 if wuid==0 and id[0] == 0, no recall
	 if wuid==0 and id[0] != 0 and id[1:2] == 0, static room */
} LOCATION;

#define SKILLSRC_NORMAL			0	// Gained through normal acquisition (Impossible for tokens)
#define SKILLSRC_SCRIPT			1	// Gained via script - can be given/revoked by GRANTSKILL/REVOKESKILL
#define SKILLSRC_SCRIPT_PERM	2	// Similar to SCRIPT but cannot be revoked once set.
#define SKILLSRC_AFFECT			3	// Gained from an affect - can be given by an affect that gives such a thing

#define SKILL_SPELL				(A)
#define SKILL_PRACTICE			(B)
#define SKILL_IMPROVE			(C)
#define SKILL_FAVOURITE			(D)

#define SKILL_AUTOMATIC			(SKILL_PRACTICE|SKILL_IMPROVE)

typedef struct skill_entry_type {
	struct skill_entry_type *next;
	char source;		// Source of the skill
	long flags;
	bool practice;		// Can this be practiced/trained?
	bool improve;		// Can this improve through use?
	bool isspell;		// Whether this is a spell;
	sh_int sn;			// Skill Number
	sh_int song;		// Song Number
	TOKEN_DATA *token;	// Skill/Spell Token, NULL if this is a built-in skill
} SKILL_ENTRY;

struct script_data {
	SCRIPT_DATA *next;
	AREA_DATA *area;
	char *name;
	int type;
	int vnum;
	char *src;
	char *edit_src;
	SCRIPT_CODE *code;
	int lines;
	long flags;
	int depth;	/* Maximum call depth allowed by script */
	int security;	/* IMP only control over runtime aspects */
	int run_security;	// Minimum security needed to RUN this script
    char *comments;
};

struct script_varinfo {
	SCRIPT_CB *block;
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	TOKEN_DATA *token;
	AREA_DATA *area;
	INSTANCE *instance;
	DUNGEON *dungeon;
	VARIABLE **var;
	CHAR_DATA *ch;
	OBJ_DATA *obj1;
	OBJ_DATA *obj2;
	CHAR_DATA *vch;
	CHAR_DATA *vch2;
	CHAR_DATA *rch;
	CHAR_DATA **targ;
	TOKEN_DATA *tok;
	PROG_DATA *progs;
	ROOM_INDEX_DATA *location;
	int registers[5];
	char phrase[MSL];
	char trigger[MSL];
};

struct corpse_info {
	char *name;
	char *short_descr;
	char *long_descr;
	char *full_descr;
	char *short_headless;
	char *long_headless;
	char *full_headless;
	char *animate_name;
	char *animate_long;
	char *animate_descr;
	char *victim_message;
	char *room_message;
	char *decay_message;
	char *skull_success;
	char *skull_success_other;
	char *skull_fail;
	char *skull_fail_other;
	bool owner_loot;	/* Only the other of this corpse can loot it */
	bool headless;		/* Corpse is inherently headless */
	bool animate_headless;	/* Allow animation if headless */
	int resurrect_chance;
	int animation_chance;	/* Ability to animate the corpse */
	int skulling_chance;	/* Ability to skull the corpse */
	int decay_type;		/* Type corpse it switches into upon decay (or none) */
	int decay_rate;		/* Chance of the corpse deteriorating some each tick
				 	For each 100 rate, the condition will decrease
					1%.  Any leftover chance will fall onto a
					random percent roll. */
	int decay_npctimer_min;
	int decay_npctimer_max;
	int decay_pctimer_min;
	int decay_pctimer_max;
	int decay_spill_chance;	/* Chance an object will drop from the corpse on decay */
	int lost_bodyparts;
};

struct corpse_blend_type {
/*	struct corpse_blend_type *next; */
	int type1;	/* Type on the mobile */
	int type2;	/* Type at death */
	int result;	/* Resulting type */
	bool dual;	/* Whether type1/type2 are interchangeable; if so, both ways are checked */
};

typedef struct random_string_pattern RANDOM_PATTERN;
typedef struct random_string_class RANDOM_CLASS;
typedef struct random_string_entry RANDOM_STRING_ENTRY;
typedef struct random_string_data RANDOM_STRING;

struct random_string_pattern {
	RANDOM_PATTERN	*next;

	int		weight;
	char		*name;

	int		classes;
	RANDOM_CLASS	**c_list;
};

struct random_string_class {
	RANDOM_CLASS	*next;

	long		uid;	/* ID of the class in the RSG */
	char *		name;	/* Display name of generator */

};

struct random_string_entry {
	RANDOM_STRING_ENTRY *next;
	int		weight;
	char *		str;
};

struct random_string_data {
	RANDOM_STRING *next;

	long		uid;	/* ID of string generator */
	char *		name;	/* Display name of generator */
	char *		descr;	/* Description of generator */

	long		patterns;
	RANDOM_PATTERN	*p_head;
	RANDOM_PATTERN	*p_tail;

	long		classes;
	RANDOM_CLASS	*c_head;
	RANDOM_CLASS	*c_tail;
};

struct list_link_type {
	LLIST_LINK *next;
	void *data;
};

struct list_type {
	LLIST *next;
	LLIST_LINK *head;
	LLIST_LINK *tail;
	unsigned long ref;
	unsigned long size;
	LISTCOPY_FUNC *copier;
	LISTDESTROY_FUNC *deleter;
	bool valid;
	bool purge;
};

struct iterator_type {
	LLIST *list;
	LLIST_LINK *current;
	bool moved;
};

struct list_link_area_data {
	AREA_DATA *area;
	long uid;
};

struct list_link_wilds_data {
	WILDS_DATA *wilds;
	long uid;
};

struct list_link_uid_data {
	void *ptr;
	unsigned long id[2];
};

struct list_link_room_data {
	ROOM_INDEX_DATA *room;
	unsigned long id[4];
};

struct list_link_exit_data {
	ROOM_INDEX_DATA *room;
	unsigned long id[4];
	int door;
};

struct list_link_skill_data {
	CHAR_DATA *mob;
	int sn;
	TOKEN_DATA *tok;
	unsigned long mid[2];
	unsigned long tid[2];
};

struct dice_data {
	int number;
	int size;
	int bonus;
	long last_roll;
};

#define OLC_PNT_MOBILE		'M'
#define OLC_PNT_OBJECT		'O'
#define OLC_PNT_ROOM		'R'
#define OLC_PNT_TOKEN		'T'

/*
Object:
	obj_cost
	obj_weight
	obj_repairs
	obj_fragility
	obj_spells
	obj_spells_def
	obj_spells_off
	obj_spells_obj
	obj_aff_str
	obj_aff_dex
	obj_aff_int
	obj_aff_wis
	obj_aff_con
	obj_aff_mana
	obj_aff_hit
	obj_aff_move
	obj_aff_other
	obj_catalyst		Dormant only.  Active requires special permission
	obj_extra_low		Low tier extra flags (1 point per flag)
	obj_extra_mid		Mid tier extra flags (1 point per flag)
	obj_extra_high		High tier extra flags (1 point per flag)

	obj_armour_pierce	Deviation from calculated armour (% based)
	obj_armour_bash		Deviation from calculated armour (% based)
	obj_armour_slash		Deviation from calculated armour (% based)
	obj_armour_exotic	Deviation from calculated armour (% based)
	obj_armour_strength	Stronger armour requires more points

	obj_weapon_damage	Deviation from calculated damage
	obj_weapon_flags	Special weapon flags that we'd like to require points (1 point per flag)

	obj_ranged_damage	Deviation from calculated damage
	obj_ranged_range	Projectile distance (further distance requires more points)

	obj_furn_heal
	obj_furn_mana
	obj_furn_move

	obj_wandstaff_level
	obj_wandstaff_charge


Mobile:

*/

#define OLC_PNTCAT_OBJ_COST				0	//
#define OLC_PNTCAT_OBJ_WEIGHT			0	//
#define OLC_PNTCAT_OBJ_REPAIRS			0	//
#define OLC_PNTCAT_OBJ_FRAGILITY		0	//
#define OLC_PNTCAT_OBJ_SPELLS			0	//
#define OLC_PNTCAT_OBJ_SPELLS_DEF		0	//
#define OLC_PNTCAT_OBJ_SPELLS_OFF		0	//
#define OLC_PNTCAT_OBJ_SPELLS_OBJ		0	//
#define OLC_PNTCAT_OBJ_AFF_STR			0	//
#define OLC_PNTCAT_OBJ_AFF_DEX			0	//
#define OLC_PNTCAT_OBJ_AFF_INT			0	//
#define OLC_PNTCAT_OBJ_AFF_WIS			0	//
#define OLC_PNTCAT_OBJ_AFF_CON			0	//
#define OLC_PNTCAT_OBJ_AFF_MANA			0	//
#define OLC_PNTCAT_OBJ_AFF_HIT			0	//
#define OLC_PNTCAT_OBJ_AFF_MOVE			0	//
#define OLC_PNTCAT_OBJ_AFF_OTHER		0	//
#define OLC_PNTCAT_OBJ_CATALYST			0	// Dormant only.  Active requires special permission
#define OLC_PNTCAT_OBJ_EXTRA_LOW		0	// Low tier extra flags (1 point per flag)
#define OLC_PNTCAT_OBJ_EXTRA_MID		0	// Mid tier extra flags (1 point per flag)
#define OLC_PNTCAT_OBJ_EXTRA_HIGH		0	// High tier extra flags (1 point per flag)

#define OLC_PNTCAT_OBJ_ARMOUR_PIERCE		0	// Deviation from calculated armour (% based)
#define OLC_PNTCAT_OBJ_ARMOUR_BASH		0	// Deviation from calculated armour (% based)
#define OLC_PNTCAT_OBJ_ARMOUR_SLASH		0	// Deviation from calculated armour (% based)
#define OLC_PNTCAT_OBJ_ARMOUR_EXOTIC		0	// Deviation from calculated armour (% based)
#define OLC_PNTCAT_OBJ_ARMOUR_STRENGTH	0	// Stronger armour requires more points

#define OLC_PNTCAT_OBJ_WEAPON_DAMAGE	0	// Deviation from calculated damage
#define OLC_PNTCAT_OBJ_WEAPON_FLAGS		0	// Special weapon flags that we'd like to require points (1 point per flag)
#define OLC_PNTCAT_OBJ_WEAPON_RANGE		0	// Projectile distance (further distance requires more points)

#define OLC_PNTCAT_OBJ_FURN_HEAL		0	//
#define OLC_PNTCAT_OBJ_FURN_MANA		0	//
#define OLC_PNTCAT_OBJ_FURN_MOVE		0	//

#define OLC_PNTCAT_OBJ_WANDSTAFF_LEVEL	0	//
#define OLC_PNTCAT_OBJ_WANDSTAFF_CHARGE	0	//

#define OLC_PNTCAT_OBJ_FOOD_HUNGER		0
#define OLC_PNTCAT_OBJ_FOOD_FULL		0

#define OLC_PNTCAT_MAX					0

struct olc_point_category_type {
	char *name;
	char *description;
	int category;
	int type;
	int subtypes[5];
};

struct olc_point_boost_data {
	OLC_POINT_BOOST *next;
	int category;
	int usage;
	int imp;		// Boosted points granted by an IMP or by claiming an area boost
	int area;		// Boosted points pulled from the area's boosted allotment
};

struct olc_point_usage_data {
	OLC_POINT_USAGE *next;
	int category;
	int usage;
};

struct olc_point_data {
	OLC_POINT_BOOST *boosts;
	OLC_POINT_USAGE *usage;
	int budget;
	int total;
};

struct olc_point_area_data {
	OLC_POINT_AREA *next;
	int category;
	int claimed;
	int boost;
};



#define SKILL_NAME(sn) (((sn) > 0 && (sn) < MAX_SKILL) ? skill_table[(sn)].name : "")

/*
 * This is used for fight.c in defences. The game will only look at a max
 * level of MAX_MOB_LEVEL so that players still shield block etc mobs which
 * are high level. It stops mobs that have been set to too high a level.
 */
#define MAX_MOB_LEVEL          150

/*
 * Game parameters.
 */
#define AUCTION_LENGTH          5
#define LEVEL_HERO		120
#define LEVEL_IMMORTAL		150
#define LEVEL_NEWBIE		10
#define MAX_TREASURES           1
#define MAX_IMMORTAL_GROUPS     6
#define MAX_ALIAS	        80
#define MAX_BUILDER_IDLE_MINUTES 30
#define MAX_CHAT_ROOMS		100
#define MAX_CHURCH_TREASURE     100
#define MAX_CHURCHES            13
#define MAX_CLASS		4
#define MAX_CLASS_LEVEL		30
#define MAX_DAMAGE_MESSAGE	70		/* @@@NIB : 20070125 */
#define MAX_GQ_PER_TYPE		200
#define MAX_GROUP		30
#define MAX_IN_CHAT_ROOM	50
#define MAX_IN_GROUP		40
#define MAX_ITEMS_IN_LOCKER	30
#define MAX_LEVEL		155
#define MAX_MOB_SKILL_LEVEL	1000
#define MAX_NPC_SHIP_MOBS	10
#define MAX_PC_RACE		27
#define MAX_POA_LEVELS		5
#define MAX_POSTAL_ITEMS        50
#define MAX_POSTAL_WEIGHT	150
#define MAX_REPRANK_CONTINENTS  3
#define MAX_SHIP_ROOMS		10
#define MAX_SKILL		300
#define MAX_SOCIALS		256
#define MAX_SONGS		30
#define MAX_STOCKS		10
#define MAX_SUB_CLASS		24
#define MAX_TOXIN		4
#define MAX_WILDERNESS_EXITS	100000
#define MAX_WORDS		45424
#define SILVER_PER_KG		500
#define MINS_PER_DEATH		8
#define PORT_NORMAL	        9000
#define PORT_TEST		9999
#define PORT_RAE	        7777
#define PORT_ALPHA		7680
#define PULSE_AREA		(60 * PULSE_PER_SECOND)
#define PULSE_AUCTION           (20 * PULSE_PER_SECOND)
#define PULSE_MAIL		(30 * PULSE_PER_SECOND)
#define PULSE_MOBILE		( 4 * PULSE_PER_SECOND)
#define PULSE_MUSIC		( 6 * PULSE_PER_SECOND)
#define PULSE_PER_SECOND	  4
#define PULSE_EVENT		(PULSE_PER_SECOND)
#define PULSE_TICK		(60 * PULSE_PER_SECOND)
#define PULSE_VIOLENCE		( 3 * PULSE_PER_SECOND)
#define PULSE_AGGR		PULSE_PER_SECOND

#define RECKONING_CHANCE_MIN		0
#define RECKONING_CHANCE_MAX_RESET	25
#define RECKONING_CHANCE_MAX		100
#define RECKONING_CHANCE(ch)		URANGE(RECKONING_CHANCE_MIN, (ch), RECKONING_CHANCE_MAX)
#define RECKONING_CHANCE_RESET(ch)	URANGE(RECKONING_CHANCE_MIN, (ch), RECKONING_CHANCE_MAX_RESET)

#define RECKONING_COOLDOWN_MIN		0
#define RECKONING_COOLDOWN_USE_MIN	10
#define RECKONING_COOLDOWN_MAX		10080
#define RECKONING_COOLDOWN(cd)		URANGE(RECKONING_COOLDOWN_MIN, (cd), RECKONING_COOLDOWN_MAX)

#define RECKONING_DURATION_DEFAULT	30
#define RECKONING_DURATION_MIN		10
#define RECKONING_DURATION_MAX		1440
#define RECKONING_DURATION(dur)		URANGE(RECKONING_DURATION_MIN, (dur), RECKONING_DURATION_MAX)

#define RECKONING_INTENSITY_DEFAULT	100
#define RECKONING_INTENSITY_MIN		10
#define RECKONING_INTENSITY_MAX		200
#define RECKONING_INTENSITY(in)		URANGE(RECKONING_INTENSITY_MIN, (in), RECKONING_INTENSITY_MAX)



#ifdef IMC
	#include "imc.h"
#endif

#define WILDERNESS_CHAR_EXIT_SIGHT 1
#define WILDERNESS_OBJ_EXIT_SIGHT  2

#define WILDERNESS_VNUM_OFFSET 100


/* Syn - PC races defined here so we don't have to use magical numbers. */
#define PC_RACE_HUMAN		1
#define PC_RACE_ELF	        2
#define PC_RACE_DWARF		3
#define PC_RACE_TITAN		4
#define PC_RACE_VAMPIRE		5
#define PC_RACE_DROW		6
#define PC_RACE_SITH	        7
#define PC_RACE_DRACONIAN	8
#define PC_RACE_SLAYER		9
#define PC_RACE_MINOTAUR	10
#define PC_RACE_ANGEL		11
#define PC_RACE_MYSTIC		12
#define PC_RACE_DEMON		13
#define PC_RACE_LICH		14
#define PC_RACE_AVATAR		15
#define PC_RACE_SERAPH		16
#define PC_RACE_BERSERKER	17
#define PC_RACE_COLOSSUS	18
#define PC_RACE_FIEND		19
#define PC_RACE_SPECTER	        20
#define PC_RACE_NAGA		21
#define PC_RACE_DRAGON		22
#define PC_RACE_CHANGELING	23
#define PC_RACE_HELLBARON	24
#define PC_RACE_WRAITH		25


/*
 * Colour stuff by Lope of Loping Through The MUD
 */
#define CLEAR		"\033[0m"	/* Resets Colour	*/
#define C_RED		"\033[0;31m"	/* Normal Colours	*/
#define C_GREEN		"\033[0;32m"
#define C_YELLOW	"\033[0;33m"
#define C_BLUE		"\033[0;34m"
#define C_MAGENTA	"\033[0;35m"
#define C_CYAN		"\033[0;36m"
#define C_WHITE		"\033[0;37m"
#define C_D_GREY	"\033[1;30m"  	/* Light Colours		*/
#define C_B_RED		"\033[1;31m"
#define C_B_GREEN	"\033[1;32m"
#define C_B_YELLOW	"\033[1;33m"
#define C_B_BLUE	"\033[1;34m"
#define C_B_MAGENTA	"\033[1;35m"
#define C_B_CYAN	"\033[1;36m"
#define C_B_WHITE	"\033[1;37m"
#define C_BK_BLACK	"\033[40m"  	/* Background Colours		*/
#define C_BK_RED	"\033[41m"
#define C_BK_GREEN	"\033[42m"
#define C_BK_YELLOW	"\033[43m"
#define C_BK_BLUE	"\033[44m"
#define C_BK_MAGENTA	"\033[45m"
#define C_BK_CYAN	"\033[46m"
#define C_BK_WHITE	"\033[47m"

/*
 * Site ban structure.
 */
#define BAN_SUFFIX		A
#define BAN_PREFIX		B
#define BAN_NEWBIES		C
#define BAN_ALL			D
#define BAN_PERMIT		E
#define BAN_PERMANENT		F

#include <zlib.h>

struct	ban_data
{
    BAN_DATA *	next;
    bool	valid;
    sh_int	ban_flags;
    sh_int	level;
    char *	name;
};

struct buf_type
{
    BUFFER *    next;
    bool        valid;
    sh_int      state;  /* error state of the buffer */
    int		size;   /* size in k */
    char *      string; /* buffer's string */
};


/*
 * Time and weather stuff.
 */
#define SUN_DARK		0
#define SUN_RISE		1
#define SUN_LIGHT		2
#define SUN_SET			3

#define SKY_CLOUDLESS		0
#define SKY_CLOUDY		1
#define SKY_RAINING		2
#define SKY_LIGHTNING		3
#define SKY_MAX         	4

/* 20070603 : NIB : Time of Day flags for the ifcheck */
#define TOD_AFTERMIDNIGHT	A
#define TOD_AFTERNOON		B
#define TOD_DAWN		C
#define TOD_DAY			D
#define TOD_DUSK		E
#define TOD_EVENING		F
#define TOD_MIDNIGHT		G
#define TOD_MORNING		H
#define TOD_NIGHT		I
#define TOD_NOON		J

#define MOON_NEW		0
#define MOON_WAXING_CRESCENT	1
#define MOON_FIRST_QUARTER	2
#define MOON_WAXING_GIBBOUS	3
#define MOON_FULL		4
#define MOON_WANING_GIBBOUS	5
#define MOON_LAST_QUARTER	6
#define MOON_WANING_CRESCENT	7

#define MOON_PERIOD		(800)	/* Hours, 33.33333... days */
#define MOON_OFFSET		(225)	/* Ahead 225 hours */
#define MOON_CARDINAL_STEP	(MOON_PERIOD / 4)
#define MOON_CARDINAL_WIDTH	(50)	/* Number of hours a given cardinal phase lasts */
#define MOON_CARDINAL_HALF	(MOON_CARDINAL_WIDTH/2)

struct	time_info_data
{
    int		hour;
    int		day;
    int		month;
    int		year;
    int		moon;
};

#define UID_MASK	0xFF
#define UID_INC		0x100

/* Vizz - track unique instances of entities */
struct global_data
{
    long	next_area_uid;
    long	next_wilds_uid;
    long	next_vlink_uid;
    unsigned long next_mob_uid[4];	/* next read: [0:1], next write: [2:3] */
    unsigned long next_obj_uid[4];	/* next read: [0:1], next write: [2:3] */
    unsigned long next_token_uid[4];	/* next read: [0:1], next write: [2:3] */
    unsigned long next_vroom_uid[4];
    long	next_church_uid;

    long	db_version;
};

struct bounty_data
{
    char *name;
    BOUNTY_DATA *next_bounty;
    long amount;
};

struct church_player_data
{
    CHURCH_PLAYER_DATA *next;
    CHURCH_DATA *church;
    CHAR_DATA *ch;
    char *name;
    long flags;

    /* variable data. is lost forever if player leaves church. */
    long cpk_losses;
    long cpk_wins;
    long dep_dp;
    long dep_gold;
    long dep_pneuma;
    long pk_losses;
    long pk_wins;
    long wars_won;

    /* Commands you can entrust a church member with */
    STRING_DATA *commands;

    int alignment;
    int rank;
    int sex;
};

#define CHURCH_PLAYER_EXCOMMUNICATED	(A)

#define COURT_RANK_SQUIRE   	0
#define COURT_RANK_EARL     	1
#define COURT_RANK_COUNT    	2
#define COURT_RANK_BARON    	3
#define COURT_RANK_DUKE     	4
#define COURT_RANK_LORD     	5
#define COURT_RANK_PRINCE   	6

#define MAX_CHURCH_SIZE		4
#define CHURCH_SIZE_BAND    	1
#define CHURCH_SIZE_CULT    	2
#define CHURCH_SIZE_ORDER   	3
#define CHURCH_SIZE_CHURCH  	4

#define MAX_CHURCH_RANK		4
#define CHURCH_RANK_NONE	-1
#define CHURCH_RANK_A   	0
#define CHURCH_RANK_B   	1
#define CHURCH_RANK_C   	2
#define CHURCH_RANK_D   	3
#define CHURCH_RANK_IMM		4

#define CHURCH_GOOD     	1
#define CHURCH_EVIL     	2
#define CHURCH_NEUTRAL  	3

/* church settings */
#define CHURCH_SHOW_PKS		(A)
#define CHURCH_ALLOW_CROSSZONES (B)
#define CHURCH_PUBLIC_MOTD	(C)
#define CHURCH_PUBLIC_RULES	(D)
#define CHURCH_PUBLIC_INFO	(E)

struct church_data
{
    CHURCH_DATA 	*next;
    CHURCH_PLAYER_DATA 	*people;

	long		uid;

    char 		*name;
    char 		*flag;
    char 		*founder;
    char 		*motd;
    char 		*rules;
    char		*info;
    char 		*log;

    long 		pneuma;
    long 		gold;
    long 		dp;

    int 		max_positions;
    int 		size;
    int 		alignment;

    LOCATION 		recall_point;
    LLIST *treasure_rooms;
    long 		key;

    long 		pk_wins;
    long 		pk_losses;
    long 		cpk_wins;
    long 		cpk_losses;
    long 		wars_won;

    long 		settings;

    time_t 		created;
    time_t 		founder_last_login;

    bool 		pk;

    char 		colour1;
    char 		colour2;

    LLIST *online_players;
    LLIST *roster;
};

struct church_treasure_room_data
{
	ROOM_INDEX_DATA *room;
	int min_rank;				// Which ranks can use it
};


/*
 * Connected state for a channel.
 */
#define CON_PLAYING			 0
#define CON_GET_NAME			 1
#define CON_GET_OLD_PASSWORD		 2
#define CON_CONFIRM_NEW_NAME		 3
#define CON_GET_NEW_PASSWORD		 4
#define CON_CONFIRM_NEW_PASSWORD	 5
#define CON_GET_NEW_RACE		 6
#define CON_GET_NEW_SEX			 7
#define CON_GET_NEW_CLASS		 8
#define CON_GET_ALIGNMENT		 9
/*#define CON_DEFAULT_CHOICE		10 */
/*#define CON_GEN_GROUPS		11 */
/*#define CON_PICK_WEAPON		12 */
#define CON_READ_IMOTD			13
#define CON_READ_MOTD			14
#define CON_BREAK_CONNECT		15
#define CON_GET_ASCII			16
#define CON_GET_SUB_CLASS       	17
#define CON_OLD_SUBCLASS  	     	18
#define CON_SUBCLASS_CHOOSE             19
#define CON_CHANGE_PASSWORD		20
#define CON_CHANGE_PASSWORD_CONFIRM     21
#define CON_GET_EMAIL			22


/* Places */
enum {
	PLACE_NOWHERE = 0,
	PLACE_FIRST_CONTINENT,
	PLACE_SECOND_CONTINENT,
	PLACE_ISLAND,
	PLACE_OTHER_PLANE,
	PLACE_ABYSS,
	PLACE_EDEN,
	PLACE_NETHERWORLD
};

/* Regions for wilderness */
enum {
	REGION_UNKNOWN = 0,
	REGION_FIRST_CONTINENT,
	REGION_SECOND_CONTINENT,
	REGION_MORDRAKE_ISLAND,
	REGION_TEMPLE_ISLAND,
	REGION_UNDERSEA,
	REGION_OCEAN
};

/* For random functions - getting random mobs, objs etc */
#define BOTH_CONTINENTS 	0
#define FIRST_CONTINENT 	1
#define SECOND_CONTINENT 	2

/* Alignments */
#define ALIGN_GOOD		1
#define ALIGN_NONE		0
#define ALIGN_EVIL	       -1

/*
 * Descriptor (channel) structure.
 */
struct	descriptor_data
{
    DESCRIPTOR_DATA *	next;
    DESCRIPTOR_DATA *	snoop_by;
    CHAR_DATA *		character;
    CHAR_DATA *		original;
    bool		valid;
    char *		host;
    sh_int		descriptor;
    sh_int		connected;
    bool		fcommand;
    char		inbuf		[4 * MAX_INPUT_LENGTH];
    char		incomm		[4 * MAX_INPUT_LENGTH];
    char		inlast		[MAX_INPUT_LENGTH];
    int			repeat;
    char *		outbuf;
    int			outsize;
    int			outtop;
    char *		showstr_head;
    char *		showstr_point;
    long                bits;           /* MSP, MXP, etc. */
    protocol_t *	pProtocol;

    /* mccp: support data */
    z_stream *          out_compress;
    unsigned char *     out_compress_buf;

    void *              pEdit;		/* OLC */
    HELP_CATEGORY	*hCat;		/* hedit */
    char **             pString;	/* OLC */
    int			editor;		/* OLC */

    /* Input function */
    bool		input;
    char *		inputString;		// Temporary holding variable for string editor
    long		input_script;
    char *		input_var;
    char *		input_prompt;
    CHAR_DATA *		input_mob;
    OBJ_DATA *		input_obj;
    ROOM_INDEX_DATA *	input_room;
    TOKEN_DATA *	input_tok;

    unsigned int		muted;			// All text heading to the output will be blocked

};


/*
 * Attribute bonus structures.
 */
struct	str_app_type
{
    sh_int	tohit;
    sh_int	todam;
    sh_int	carry;
    sh_int	wield;
};


struct	int_app_type
{
    sh_int	learn;
};


struct	wis_app_type
{
    sh_int	practice;
};


struct	dex_app_type
{
    sh_int	defensive;
};


struct	con_app_type
{
    sh_int	hitp;
    sh_int	shock;
};


/*
 * TO types for act.
 */
/* #define TO_ROOM		0 */
#define TO_NOTVICT		1
#define TO_VICT			2
#define TO_CHAR			3
#define TO_ALL			4
#define TO_THIRD		5
#define TO_NOTTHIRD		6
#define TO_FUNC			10	/* NIB : 20070122 */
#define TO_NOTFUNC		11	/* NIB : 20070122 */

/*
 * Help types
 */
struct	help_category
{
    HELP_CATEGORY *next;
    char *name;
    char *description;

    unsigned int min_level;
    unsigned int security;

    /* bookkeeping info */
    char *creator;
    time_t created;
    char *modified_by;
    time_t modified;

    char *builders;

    HELP_CATEGORY *up;
    HELP_CATEGORY *inside_cats;
    HELP_DATA *inside_helps;
};

struct	help_data
{
    char *	creator;
    time_t	created;
    char *	modified_by;
    char *	builders;
    time_t	modified;
    HELP_DATA *	next;
    HELP_CATEGORY *hCat;
    int		min_level;
    int		security;
    char *	keyword;
    char *	text;
    int		index;
    STRING_DATA *related_topics;
};

/*
 * Shop types.
 */
#define MAX_TRADE	 5

#define SHOPFLAG_STOCK_ONLY		(A)		// Only allow buyback of listed stock
#define SHOPFLAG_HIDE_SHOP		(B)		// Hides shop from the minimap
#define SHOPFLAG_NO_HAGGLE		(C)		// Block haggling

struct	shop_data
{
    SHOP_DATA *	next;			/* Next shop in list		*/
    long	keeper;			/* Vnum of shop keeper mob	*/
    sh_int	buy_type [MAX_TRADE];	/* Item types shop will buy	*/
    sh_int	profit_buy;		/* Cost multiplier for buying	*/
    sh_int	profit_sell;		/* Cost multiplier for selling	*/
    sh_int	open_hour;		/* First opening hour		*/
    sh_int	close_hour;		/* First closing hour		*/

    int restock_interval;		// How long between restocking checks, in minutes
    time_t next_restock;

    int		flags;
    int		discount;			// Possible percent discount you can get from haggling (0-100)

    SHOP_STOCK_DATA *stock;
};

#define STOCK_CUSTOM	0
#define STOCK_OBJECT	1
#define STOCK_PET		2
#define STOCK_MOUNT		3
#define STOCK_GUARD		4

struct shop_stock_data
{
	SHOP_STOCK_DATA *next;

	int level;					// Minimum level required to buy it
								// If level is less than 1..
								//   - if vnum > 0 => object level
								//   - else => clamped 1

	long silver;
	long qp;
	long dp;
	long pneuma;
	char *custom_price;			// Custom pricing (supercedes other pricing values)

	int discount;				// How much of a discount is the shopkeeper willing to haggle (0-100%)

	int quantity;				// Number of units available
	int max_quantity;			// Total number of units available
	int restock_rate;			// How manu units will get restocked per reset cycle (<1 == never)

	MOB_INDEX_DATA *mob;
	OBJ_INDEX_DATA *obj;
	int type;
	long vnum;

	int duration;				// How long will the stock item last (in-game hours)

	char *custom_keyword;		// Concept / Special object
	char *custom_descr;

	bool singular;				// Can only buy one unit at a time
};

struct shop_request_data
{
	SHOP_STOCK_DATA *stock;
	OBJ_DATA *obj;
};

/*
 * Per-class stuff.
 */
#define MAX_STATS 	5
#define STAT_STR 	0
#define STAT_INT	1
#define STAT_WIS	2
#define STAT_DEX	3
#define STAT_CON	4

struct	class_type
{
    char *	name;			/* the full name of the class */
    int		attr_prime;		/* Prime attribute		*/
    long	weapon;			/* First weapon			*/
    int		hp_min;			/* Min hp gained on leveling	*/
    int		hp_max;			/* Max hp gained on leveling	*/
    bool	fMana;			/* Class gains mana on level	*/
    char *	base_group;		/* base skills gained		*/
};

struct	sub_class_type
{
    char *	name[3];		/* the full name of the class */
    char * 	who_name[3];		/* Name for 'who': neutral, male and female */
    int		class;			/* Class (i hate this) */
    char *	default_group;		/* default skills gained	*/
    int		alignment;		/* alignment if applicable */
    bool	remort;			/* remort-only class? */
    int		prereq[2];		/* prerequisites for the remort classes */
    long	objects[5];		/* starting objects */
};

#define SETTING_OFF	0
#define SETTING_ON	1

struct  player_setting_type
{
    char *	name;
    long	vector;
    long	vector2;
    long	vector_comm;
    bool	inverted;
    int		min_level;
    int		default_state;
};

struct  newbie_eq_type
{
    long 	vnum;
    int		wear_loc;
};

struct item_type
{
    int		type;
    char *	name;
};

struct map_exit_type
{
  int type;  /* Wilderness type */
  int x, y;        /* Page coords */
  long room_vnum;  /* Room vnum */
};

struct weapon_type
{
    char *	name;
    long	vnum;
    sh_int	type;
    sh_int	*gsn;
};

struct wiznet_type
{
    char *	name;
    long 	flag;
    int		level;
};

struct attack_type
{
    char *	name;			/* name */
    char *	noun;			/* message */
    int   	damage;			/* damage class */
};


struct race_type
{
    char *	name;			/* call name of the race */
    bool	pc_race;		/* can be chosen by pcs */
    sh_int	*pgrn;
    sh_int	*pgprn;			/* PC race pointer */
    long	act;			/* act bits for the race */
    long	act2;			/* Vizz - act2 bits for the race */
    long	aff;			/* aff bits for the race */
    long	aff2;			/* aff2 bits for the race */
    long	off;			/* off bits for the race */
    long	imm;			/* imm bits for the race */
    long        res;			/* res bits for the race */
    long	vuln;			/* vuln bits for the race */
    long	form;			/* default form flag for the race */
    long	parts;			/* default parts for the race */
};

struct pc_race_type  /* additional data for pc races */
{
    char * name;			/* MUST be in race_type */
    char * who_name;
    char * skills[9];		  	/* bonus skills for the race */
    int    stats[MAX_STATS];		/* starting stats */
    int	   max_stats[MAX_STATS];	/* maximum stats */
    int    max_vital_stats[3];		/* max hit points, mana, and move */
    int	   size;
    int    alignment;			/* good, neutral or evil */
    sh_int	*pgrn;
    sh_int	*prgrn;		/* Pointer to the REMORT race */
    bool   remort;			/* is it a remort race? */
    long   objects[5];			/* starting objects */
};

#define MAX_HIT 	0
#define MAX_MANA 	1
#define MAX_MOVE 	2

struct spec_type
{
    char * 	name;			/* special function name */
    SPEC_FUN *	function;		/* the function */
};

struct toxin_type
{
    char *	name;
    int 	cost[2];		/* how much % toxin it costs [min, max] */
    int		difficulty;		/* how difficult to land (%) */
    SPELL_FUN * spell;			/* what it does */
    bool 	naga;			/* naga-only? */
};

#define MAX_HERB		        7

struct herb_type
{
    char *	name;
    int 	sector;			/* which sector type can it be found in */
    long	stat[3];		/* how much stats it restores [hit, mana, move] */
    long	imm;			/* immunity bitfield */
    long	res;			/* resistance bitfield */
    long	vuln;			/* vulnerability bitfield */
    SPELL_FUN * spell;			/* spell function for special spells */
};

#define BOOST_EXPERIENCE	0
#define BOOST_DAMAGE		1
#define BOOST_QP		2
#define BOOST_PNEUMA		3
#define BOOST_RECKONING		4

struct boost_type
{
    char *	name;			/* name */
    char * 	colour_name;		/* the name of the boost displayed in gecho (cosmetic */
    int 	boost;			/* what % the current boost is, normal is 100% */
    time_t	timer;			/* this is the time the boost expires. */
};

/*
struct stat_type
{
};
*/


/* Notes */
#define NOTE_NOTE	0
#define NOTE_IDEA	1
#define NOTE_PENALTY	2
#define NOTE_NEWS	3
#define NOTE_CHANGES	4

struct	note_data
{
    NOTE_DATA *	next;
    bool 	valid;
    sh_int	type;
    char *	sender;
    char *	date;
    char *	to_list;
    char *	subject;
    char *	text;
    time_t  	date_stamp;
};

#define	AFFGROUP_RACIAL		1
#define AFFGROUP_METARACIAL	2
#define AFFGROUP_BIOLOGICAL	3
#define AFFGROUP_MENTAL		4
#define AFFGROUP_DIVINE		5
#define AFFGROUP_MAGICAL	6
#define AFFGROUP_PHYSICAL	7

#define AFFGROUP_INHERENT	101
#define AFFGROUP_ENCHANT	102
#define AFFGROUP_WEAPON		103
#define AFFGROUP_PORTAL		104
#define AFFGROUP_CONTAINER	105



/* Affects */
struct	affect_data
{
    AFFECT_DATA *	next;
    bool		valid;
    sh_int		group;
    sh_int		where;
    sh_int		type;
    sh_int		level;
    sh_int		duration;
    sh_int		location;
    sh_int		modifier;
    long		bitvector;
    long		bitvector2;
    sh_int 		random;
    char 		*custom_name;
    sh_int		slot;
};

/* where definitions */
#define TO_AFFECTS	0
#define TO_OBJECT	1
#define TO_IMMUNE	2
#define TO_RESIST	3
#define TO_VULN		4
#define TO_WEAPON	5
#define TO_ROOM		6
#define TO_AFFECTS2	7
#define TO_OBJECT2	8
#define TO_OBJECT3	9
#define TO_OBJECT4	10

#define TO_CATALYST_DORMANT	20
#define TO_CATALYST_ACTIVE	21

/* sith toxins */
#define TOXIN_PARALYZE 	0
#define TOXIN_WEAKNESS 	1
#define TOXIN_NEURO    	2
#define TOXIN_VENOM    	3

/* reverie types */
#define HIT_TO_MANA 	0
#define MANA_TO_HIT 	1


typedef struct affliction_data AFFLICTION_DATA;
struct affliction_data {
	AFFLICTION_DATA *next;
	int type;
	int state;
	int timer;
	int level;
	int values[4];
	bool valid;
};

typedef struct affliction_type AFFLICTION_TYPE;
struct affliction_type {
	char *name;
	int *index;
	bool (*state)(CHAR_DATA *ch,AFFLICTION_DATA *aff);
	void (*info)(CHAR_DATA *ch,AFFLICTION_DATA *aff, char *buf);
};

/*
 * Well known mob virtual numbers.
 */
#define MOB_VNUM_DEATH		  	6502
#define MOB_VNUM_OBJCASTER		6509
#define MOB_VNUM_REFLECTION           	6530
#define MOB_VNUM_SLAYER               	6531
#define MOB_VNUM_WEREWOLF             	6532
#define MOB_VNUM_MAYOR_PLITH		11000
#define MOB_VNUM_RAVAGE			11017
#define MOB_VNUM_STIENER		11500
#define MOB_VNUM_CHANGELING		100025
#define MOB_VNUM_DARK_WRAITH   		100050
#define MOB_VNUM_GATEKEEPER_ABYSS     	102000
#define MOB_VNUM_SAILOR_BURLY         	157002
#define MOB_VNUM_SAILOR_DIRTY         	157001
#define MOB_VNUM_SAILOR_DISEASED      	157000
#define MOB_VNUM_SAILOR_ELITE         	157005
#define MOB_VNUM_SAILOR_MERCENARY     	157004
#define MOB_VNUM_SAILOR_TRAINED       	157003
#define MOB_VNUM_GELDOFF	   	300001

#define MOB_VNUM_PIRATE_HUNTER_1 100200 /* < lvl 60 */
#define MOB_VNUM_PIRATE_HUNTER_2 100201 /* < lvl 90 */
#define MOB_VNUM_PIRATE_HUNTER_3 100202 /* < lvl 120 */

#define VNUM_QUESTOR_1 		6777	/* Roscharch */
#define VNUM_QUESTOR_2 		265817	/* Alemnos */

#define MOB_VNUM_INVASION_LEADER_GOBLIN 100203
#define MOB_VNUM_INVASION_LEADER_SKELETON 100204
#define MOB_VNUM_INVASION_LEADER_BANDIT 100207
#define MOB_VNUM_INVASION_LEADER_PIRATE 100205

#define MOB_VNUM_INVASION_GOBLIN 100213
#define MOB_VNUM_INVASION_SKELETON 100214
#define MOB_VNUM_INVASION_BANDIT 100217
#define MOB_VNUM_INVASION_PIRATE 100215

/* weather storm types */
#define WEATHER_NONE            0
#define WEATHER_RAIN_STORM      1
#define WEATHER_LIGHTNING_STORM 2
#define WEATHER_SNOW_STORM      3
#define WEATHER_HURRICANE       4
#define WEATHER_TORNADO         5

/* bitfields */
#define A		 	1
#define B			2
#define C			4
#define D			8
#define E			16
#define F			32
#define G			64
#define H			128
#define I			256
#define J			512
#define K		        1024
#define L		 	2048
#define M			4096
#define N		 	8192
#define O			16384
#define P			32768 /* Limit of signed int */
#define Q			65536
#define R			131072
#define S			262144
#define T			524288
#define U			1048576
#define V			2097152
#define W			4194304
#define X			8388608
#define Y			16777216
#define Z			33554432
#define aa			67108864
#define bb			134217728
#define cc			268435456
#define dd			536870912
#define ee			1073741824

/*
 * ACT bits for mobs.
 */
#define ACT_IS_NPC		(A)
#define ACT_SENTINEL	    	(B)
#define ACT_SCAVENGER	      	(C)
#define ACT_PROTECTED	      	(D)
#define ACT_AGGRESSIVE		(F)
#define ACT_MOUNT		(E)
#define ACT_STAY_AREA		(G)
#define ACT_WIMPY		(H)
#define ACT_PET			(I)
#define ACT_TRAIN		(J)
#define ACT_PRACTICE		(K)
#define ACT_BLACKSMITH		(L)
#define ACT_CREW_SELLER		(M)
#define ACT_NO_LORE             (N)
#define ACT_UNDEAD		(O)
#define ACT_CLERIC		(Q)
#define ACT_MAGE		(R)
#define ACT_THIEF		(S)
#define ACT_WARRIOR		(T)
#define ACT_ANIMATED            (U)
#define ACT_NOPURGE		(V)
#define ACT_OUTDOORS		(W)
#define ACT_IS_RESTRINGER	(X)
#define ACT_INDOORS		(Y)
#define ACT_QUESTOR		(Z)
#define ACT_IS_HEALER		(aa)
#define ACT_STAY_LOCALE		(bb)
#define ACT_UPDATE_ALWAYS	(cc)
#define ACT_IS_CHANGER		(dd)
#define ACT_IS_BANKER		(ee)

/* Act2 flags */
#define ACT2_CHURCHMASTER       (A)
#define ACT2_NOQUEST		(B)
#define ACT2_PLANE_TUNNELER	(C)
#define ACT2_NO_HUNT		(D)
#define ACT2_AIRSHIP_SELLER     (E)
#define ACT2_WIZI_MOB		(F)
#define ACT2_TRADER		(G)
#define ACT2_LOREMASTER		(H)
#define ACT2_NO_RESURRECT       (I)
#define ACT2_DROP_EQ		(J)
#define ACT2_GQ_MASTER		(K)
#define ACT2_WILDS_WANDERER	(L)
#define ACT2_SHIP_QUESTMASTER   (M)
#define ACT2_RESET_ONCE		(N)
#define ACT2_SEE_ALL		(O)
#define ACT2_NO_CHASE		(P)
#define ACT2_TAKES_SKULLS	(Q)
#define ACT2_PIRATE		(R)
#define ACT2_PLAYER_HUNTER 	(S)
#define ACT2_INVASION_LEADER 	(T)
#define ACT2_INVASION_MOB    	(U)
#define ACT2_SEE_WIZI		(V)
#define ACT2_SOUL_DEPOSIT	(W)
#define ACT2_USE_SKILLS_ONLY	(X)
#define ACT2_INSTANCE_MOB	(Z)				// Mob is spawned by the instance, does not get added to the dungeon lists
#define ACT2_CANLEVEL		(aa)
#define ACT2_NO_XP			(bb)
#define ACT2_HIRED			(cc)
#define ACT2_RENEWER		(dd)			// Allows the mob to handle the "renew" command


/* Has_done flags - this is for commands which only are allowed */
/* to be used once in combat. Currently just reverie. */
#define DONE_REVERIE		(A)

/* damage classes */
#define DAM_NONE                0
#define DAM_BASH                1
#define DAM_PIERCE              2
#define DAM_SLASH               3
#define DAM_FIRE                4
#define DAM_COLD                5
#define DAM_LIGHTNING           6
#define DAM_ACID                7
#define DAM_POISON              8
#define DAM_NEGATIVE            9
#define DAM_HOLY                10
#define DAM_ENERGY              11
#define DAM_MENTAL              12
#define DAM_DISEASE             13
#define DAM_DROWNING            14
#define DAM_LIGHT		15
#define DAM_OTHER               16
#define DAM_HARM		17
#define DAM_CHARM		18
#define DAM_SOUND		19
#define DAM_BITE		20
#define DAM_VORPAL		21
#define DAM_BACKSTAB		22
#define DAM_MAGIC		23
#define DAM_WATER		24	/* @@@NIB : 20070120 */
#define DAM_EARTH		25	/* @@@NIB : 20070124 */
#define DAM_PLANT		26	/* @@@NIB : 20070124 */
#define DAM_AIR			27	/* @@@NIB : 20070124 */
#define DAM_MAX			28

/* OFF bits for mobiles */
#define OFF_AREA_ATTACK         (A)
#define OFF_BACKSTAB            (B)
#define OFF_BASH                (C)
#define OFF_BERSERK             (D)
#define OFF_DISARM              (E)
#define OFF_DODGE               (F)
#define OFF_FADE                (G)
/* #define OFF_FAST                (H) FREE */
#define OFF_KICK                (I)
#define OFF_KICK_DIRT           (J)
#define OFF_PARRY               (K)
#define OFF_RESCUE              (L)
#define OFF_TAIL                (M)
#define OFF_TRIP                (N)
#define OFF_CRUSH		(O)
#define ASSIST_ALL       	(P)
#define ASSIST_ALIGN	        (Q)
#define ASSIST_RACE    	     	(R)
#define ASSIST_PLAYERS      	(S)
#define ASSIST_GUARD        	(T)
#define ASSIST_VNUM		(U)
#define ASSIST_NPC		(Z)
#define OFF_BITE                (W)
#define OFF_BREATH              (X)
#define OFF_MAGIC		(Y)

/* return values for check_imm */
#define IS_NORMAL		0
#define IS_IMMUNE		1
#define IS_RESISTANT		2
#define IS_VULNERABLE		3

/* IMM bits for mobs */
#define IMM_SUMMON              (A)
#define IMM_CHARM               (B)
#define IMM_MAGIC               (C)
#define IMM_WEAPON              (D)
#define IMM_BASH                (E)
#define IMM_PIERCE              (F)
#define IMM_SLASH               (G)
#define IMM_FIRE                (H)
#define IMM_COLD                (I)
#define IMM_LIGHTNING           (J)
#define IMM_ACID                (K)
#define IMM_POISON              (L)
#define IMM_NEGATIVE            (M)
#define IMM_HOLY                (N)
#define IMM_ENERGY              (O)
#define IMM_MENTAL              (P)
#define IMM_DISEASE             (Q)
#define IMM_DROWNING            (R)
#define IMM_LIGHT		(S)
#define IMM_SOUND		(T)
#define IMM_WOOD                (X)
#define IMM_SILVER              (Y)
#define IMM_IRON                (Z)
#define IMM_BITE                (aa)
#define IMM_KILL		(bb)
#define IMM_WATER		(cc)
#define IMM_EARTH		(dd)	/* @@@NIB : 20070125 */
#define IMM_PLANT		(ee)	/* @@@NIB : 20070125 */
#define IMM_AIR			(U)	/* @@@NIB : 20070125 */

/* RES bits for mobs */
#define RES_SUMMON		(A)
#define RES_CHARM		(B)
#define RES_MAGIC               (C)
#define RES_WEAPON              (D)
#define RES_BASH                (E)
#define RES_PIERCE              (F)
#define RES_SLASH               (G)
#define RES_FIRE                (H)
#define RES_COLD                (I)
#define RES_LIGHTNING           (J)
#define RES_ACID                (K)
#define RES_POISON              (L)
#define RES_NEGATIVE            (M)
#define RES_HOLY                (N)
#define RES_ENERGY              (O)
#define RES_MENTAL              (P)
#define RES_DISEASE             (Q)
#define RES_DROWNING            (R)
#define RES_LIGHT		(S)
#define RES_SOUND		(T)
#define RES_WOOD                (X)
#define RES_SILVER              (Y)
#define RES_IRON                (Z)
#define RES_BITE                (aa)
#define RES_KILL		(bb)
#define RES_WATER		(cc)
#define RES_EARTH		(dd)	/* @@@NIB : 20070125 */
#define RES_PLANT		(ee)	/* @@@NIB : 20070125 */
#define RES_AIR			(U)	/* @@@NIB : 20070125 */

/* VULN bits for mobs */
#define VULN_SUMMON		(A)
#define VULN_CHARM		(B)
#define VULN_MAGIC              (C)
#define VULN_WEAPON             (D)
#define VULN_BASH               (E)
#define VULN_PIERCE             (F)
#define VULN_SLASH              (G)
#define VULN_FIRE               (H)
#define VULN_COLD               (I)
#define VULN_LIGHTNING          (J)
#define VULN_ACID               (K)
#define VULN_POISON             (L)
#define VULN_NEGATIVE           (M)
#define VULN_HOLY               (N)
#define VULN_ENERGY             (O)
#define VULN_MENTAL             (P)
#define VULN_DISEASE            (Q)
#define VULN_DROWNING           (R)
#define VULN_LIGHT		(S)
#define VULN_SOUND		(T)
#define VULN_WOOD               (X)
#define VULN_SILVER             (Y)
#define VULN_IRON		(Z)
#define VULN_BITE		(aa)
#define VULN_KILL		(bb)
#define VULN_WATER		(cc)
#define VULN_EARTH		(dd)	/* @@@NIB : 20070125 */
#define VULN_PLANT		(ee)	/* @@@NIB : 20070125 */
#define VULN_AIR		(U)	/* @@@NIB : 20070125 */

/* body form */
#define FORM_EDIBLE             (A)
#define FORM_POISON             (B)
#define FORM_MAGICAL            (C)
#define FORM_INSTANT_DECAY      (D)
#define FORM_OTHER              (E)
/* actual form */
#define FORM_ANIMAL             (G)
#define FORM_SENTIENT           (H)
#define FORM_UNDEAD             (I)
#define FORM_CONSTRUCT          (J)
#define FORM_MIST               (K)
#define FORM_INTANGIBLE         (L)
#define FORM_BIPED              (M)
#define FORM_CENTAUR            (N)
#define FORM_INSECT             (O)
#define FORM_SPIDER             (P)
#define FORM_CRUSTACEAN         (Q)
#define FORM_WORM               (R)
#define FORM_BLOB		(S)
#define FORM_MAMMAL             (V)
#define FORM_BIRD               (W)
#define FORM_REPTILE            (X)
#define FORM_SNAKE              (Y)
#define FORM_DRAGON             (Z)
#define FORM_AMPHIBIAN          (aa)
#define FORM_FISH               (bb)
#define FORM_COLD_BLOOD		(cc)
#define FORM_OBJECT		(dd)

/* body parts */
#define PART_HEAD               (A)
#define PART_ARMS               (B)
#define PART_LEGS               (C)
#define PART_HEART              (D)
#define PART_BRAINS             (E)
#define PART_GUTS               (F)
#define PART_HANDS              (G)
#define PART_FEET               (H)
#define PART_FINGERS            (I)
#define PART_EAR                (J)
#define PART_EYE		(K)
#define PART_LONG_TONGUE        (L)
#define PART_EYESTALKS          (M)
#define PART_TENTACLES          (N)
#define PART_FINS               (O)
#define PART_WINGS              (P)
#define PART_TAIL               (Q)
#define PART_GILLS				(R)
/* for combat */
#define PART_CLAWS              (U)
#define PART_FANGS              (V)
#define PART_HORNS              (W)
#define PART_SCALES             (X)
#define PART_TUSKS		(Y)
#define PART_HIDE		(Z)

/*
 * Bits for 'affected_by'.
 */
#define AFF_BLIND		(A)
#define AFF_INVISIBLE		(B)
#define AFF_DETECT_EVIL		(C)
#define AFF_DETECT_INVIS	(D)
#define AFF_DETECT_MAGIC	(E)
#define AFF_DETECT_HIDDEN	(F)
#define AFF_DETECT_GOOD		(G)
#define AFF_SANCTUARY		(H)
#define AFF_FAERIE_FIRE		(I)
#define AFF_INFRARED		(J)
#define AFF_CURSE		(K)
#define AFF_DEATH_GRIP		(L)
#define AFF_POISON		(M)
/*				(N) */
/*				(O) */
#define AFF_SNEAK		(P)
#define AFF_HIDE		(Q)
#define AFF_SLEEP		(R)
#define AFF_CHARM		(S)
#define AFF_FLYING		(T)
#define AFF_PASS_DOOR		(U)
#define AFF_HASTE		(V)
#define AFF_CALM		(W)
#define AFF_PLAGUE		(X)
#define AFF_WEAKEN		(Y)
#define AFF_FRENZY	        (Z)
#define AFF_BERSERK		(aa)
#define AFF_SWIM		(bb)
#define AFF_REGENERATION        (cc)
#define AFF_SLOW		(dd)
#define AFF_WEB			(ee)

/* Aff2 flags */
#define AFF2_SILENCE		(A)
#define AFF2_EVASION		(B)
#define AFF2_CLOAK_OF_GUILE	(C)
#define AFF2_WARCRY		(D)
#define AFF2_LIGHT_SHROUD	(E)
#define AFF2_HEALING_AURA	(F)
#define AFF2_ENERGY_FIELD	(G)
#define AFF2_SPELL_SHIELD       (H)
#define AFF2_SPELL_DEFLECTION   (I)
#define AFF2_AVATAR_SHIELD      (J)
#define AFF2_FATIGUE		(K)
#define AFF2_PARALYSIS		(L)
#define AFF2_NEUROTOXIN		(M)
#define AFF2_TOXIN		(N)
#define AFF2_ELECTRICAL_BARRIER (O)
#define AFF2_FIRE_BARRIER	(P)
#define AFF2_FROST_BARRIER	(Q)
#define AFF2_IMPROVED_INVIS    	(R)
#define AFF2_ENSNARE		(S)
#define AFF2_SEE_CLOAK		(T)
#define AFF2_STONE_SKIN		(U)
#define AFF2_MORPHLOCK		(V)
#define AFF2_DEATHSIGHT		(W)
#define AFF2_IMMOBILE		(X)
#define AFF2_PROTECTED		(Y)
#define AFF2_DARK_SHROUD	(Z)
/*				(aa)
				(bb)
				(cc)
				(dd)
				(ee) */

#define AFFFLAG_NODISPEL		(A)		// Affect cannot be removed by dispel
#define AFFFLAG_NOCANCEL		(B)		// Affect cannot be removed by cancellation

/* Sex */
#define SEX_NEUTRAL		      0
#define SEX_MALE		      1
#define SEX_FEMALE		      2

/* AC types */
#define AC_PIERCE			0
#define AC_BASH				1
#define AC_SLASH			2
#define AC_EXOTIC			3

/* dice */
#define DICE_NUMBER			0
#define DICE_TYPE			1
#define DICE_BONUS			2

/* size */
#define SIZE_TINY			0
#define SIZE_SMALL			1
#define SIZE_MEDIUM			2
#define SIZE_LARGE			3
#define SIZE_HUGE			4
#define SIZE_GIANT			5

/*
 * Well known object virtual numbers.
 */
#define OBJ_VNUM_SILVER_ONE	      1
#define OBJ_VNUM_GOLD_ONE	      2
#define OBJ_VNUM_GOLD_SOME	      3
#define OBJ_VNUM_SILVER_SOME	      4
#define OBJ_VNUM_COINS		      5
#define OBJ_VNUM_CORPSE_NPC	     10
#define OBJ_VNUM_CORPSE_PC	     11
#define OBJ_VNUM_SEVERED_HEAD	     12
#define OBJ_VNUM_TORN_HEART	     13
#define OBJ_VNUM_SLICED_ARM	     14
#define OBJ_VNUM_SLICED_LEG	     15
#define OBJ_VNUM_GUTS		     16
#define OBJ_VNUM_BRAINS		     17
#define OBJ_VNUM_MUSHROOM	     20
#define OBJ_VNUM_LIGHT_BALL	     21
#define OBJ_VNUM_SPRING		     22
#define OBJ_VNUM_DISC		     23
#define OBJ_VNUM_PORTAL		     25

#define OBJ_VNUM_NEWB_QUARTERSTAFF 3740
#define OBJ_VNUM_NEWB_DAGGER	   3741
#define OBJ_VNUM_NEWB_SWORD        3742
#define OBJ_VNUM_NEWB_ARMOUR        3747
#define OBJ_VNUM_NEWB_CLOAK        3748
#define OBJ_VNUM_NEWB_LEGGINGS     3749
#define OBJ_VNUM_NEWB_BOOTS        3750
#define OBJ_VNUM_NEWB_HELM         3751
#define OBJ_VNUM_NEWB_HARMONICA    3752

#define OBJ_VNUM_ROSE		   100001
#define OBJ_VNUM_PIT		   100002
#define OBJ_VNUM_WHISTLE	   100003
#define OBJ_VNUM_HEALING_LOCKET	   100005
#define OBJ_VNUM_STARCHART	   100010
#define OBJ_VNUM_ARGYLE_RING	   100032
#define OBJ_VNUM_SHIELD_DRAGON	   100050
#define OBJ_VNUM_SWORD_MISHKAL     100053
#define OBJ_VNUM_GOLDEN_APPLE	   100056
#define OBJ_VNUM_GLASS_HAMMER      100057
#define OBJ_VNUM_PAWN_TICKET	   100058
#define OBJ_VNUM_CURSED_ORB	   100059
#define OBJ_VNUM_GOLD_WHISTLE      100098
#define OBJ_VNUM_SWORD_SENT	   100105
#define OBJ_VNUM_SHARD		   100109

#define OBJ_VNUM_KEY_ABYSS	   102000

#define OBJ_VNUM_BOTTLED_SOUL      150002
#define OBJ_VNUM_SAILING_BOAT_MAST 	157000
#define OBJ_VNUM_SAILING_BOAT      	157001
#define OBJ_VNUM_SAILING_BOAT_DEBRIS 	157002
#define OBJ_VNUM_SAILING_BOAT_CANNON 	157003
#define OBJ_VNUM_CARGO_SHIP          	157004
#define OBJ_VNUM_AIR_SHIP          157005
#define OBJ_VNUM_SAILING_BOAT_WHEEL 157006
#define OBJ_VNUM_SAILING_BOAT_SEXTANT 157007
#define OBJ_VNUM_FRIGATE_SHIP      157008
#define OBJ_VNUM_TRADE_CANNON  	   157009
#define OBJ_VNUM_GALLEON_SHIP      157010

#define OBJ_VNUM_GOBLIN_WHISTLE  157023
#define OBJ_VNUM_PIRATE_HEAD  157024
#define OBJ_VNUM_INVASION_LEADER_HEAD  157025

#define OBJ_VNUM_SHACKLES	   6504
#define OBJ_VNUM_DEATH_BOOK	   6505
#define OBJ_VNUM_INFERNO	   6506
#define OBJ_VNUM_SKULL		   6507
#define OBJ_VNUM_GOLD_SKULL	   6508
#define OBJ_VNUM_EMPTY_VIAL	   6509
#define OBJ_VNUM_POTION		   6510
#define OBJ_VNUM_BLANK_SCROLL	   6511
#define OBJ_VNUM_SCROLL  	   6512
#define OBJ_VNUM_LEATHER_JACKET    6513
#define OBJ_VNUM_GREEN_TIGHTS      6514
#define OBJ_VNUM_SANDALS           6515
#define OBJ_VNUM_BLACK_CLOAK       6516
#define OBJ_VNUM_RED_CLOAK         6517
#define OBJ_VNUM_BROWN_TUNIC       6518
#define OBJ_VNUM_BROWN_ROBE        6519
#define OBJ_VNUM_FEATHERED_ROBE    6520
#define OBJ_VNUM_FEATHERED_STICK   6521
#define OBJ_VNUM_GREEN_ROBE        6522
#define OBJ_VNUM_QUEST_SCROLL      6524
#define OBJ_VNUM_TREASURE_MAP      100602
#define OBJ_VNUM_RELIC_EXTRA_DAMAGE         6526
#define OBJ_VNUM_RELIC_EXTRA_XP             6527
#define OBJ_VNUM_RELIC_EXTRA_PNEUMA         6528
#define OBJ_VNUM_RELIC_HP_REGEN             6529
#define OBJ_VNUM_RELIC_MANA_REGEN           6530
#define OBJ_VNUM_ROOM_DARKNESS     6531
#define OBJ_VNUM_ROOMSHIELD        6532
#define OBJ_VNUM_HARMONICA	   6533
#define OBJ_VNUM_SMOKE_BOMB        6534
#define OBJ_VNUM_STINKING_CLOUD    6535
#define OBJ_VNUM_SPELL_TRAP 	   6536
#define OBJ_VNUM_WITHERING_CLOUD   6537
#define OBJ_VNUM_ICE_STORM	   6538
#define OBJ_VNUM_EMPTY_TATTOO      6539

#define OBJ_VNUM_DARK_WRAITH_EQ    100502
#define OBJ_VNUM_ABYSS_PORTAL      2000001


#define ROOM_VNUM_GALLEON_NEST   157040
#define ROOM_VNUM_GALLEON_HELM   157041
#define ROOM_VNUM_GALLEON_DECK   157042
#define ROOM_VNUM_GALLEON_CABIN  157043
#define ROOM_VNUM_GALLEON_BOW  157044
#define ROOM_VNUM_GALLEON_STERN  157045
#define ROOM_VNUM_GALLEON_CARGO  157047
#define ROOM_VNUM_GALLEON_TREASURE   157048

#define ROOM_VNUM_FRIGATE_NEST   157030
#define ROOM_VNUM_FRIGATE_HELM   157031
#define ROOM_VNUM_FRIGATE_DECK   157032
#define ROOM_VNUM_FRIGATE_CABIN  157033
#define ROOM_VNUM_FRIGATE_BOW  157034
#define ROOM_VNUM_FRIGATE_STERN  157035
#define ROOM_VNUM_FRIGATE_CARGO  157036
#define ROOM_VNUM_ABYSS_GATE	2067572

/* Herbs */
#define HERB_NONE   			0
#define HERB_HYLAXIS			1
#define HERB_RHOTAIL			2
#define HERB_VIAGROL			3
#define HERB_GUAMAL			4
#define HERB_SATRIX			5
#define HERB_FALSZ			6



/*
 * Item types.
 */
#define ITEM_LIGHT		      1
#define ITEM_SCROLL		      2
#define ITEM_WAND		      3
#define ITEM_STAFF		      4
#define ITEM_WEAPON		      5
#define ITEM_TREASURE		      8
#define ITEM_ARMOUR		      9
#define ITEM_POTION		     10
#define ITEM_CLOTHING		     11
#define ITEM_FURNITURE		     12
#define ITEM_TRASH		     13
#define ITEM_CONTAINER		     15
#define ITEM_DRINK_CON		     17
#define ITEM_KEY		     18
#define ITEM_FOOD		     19
#define ITEM_MONEY		     20
#define ITEM_BOAT		     22
#define ITEM_CORPSE_NPC		     23
#define ITEM_CORPSE_PC		     24
#define ITEM_FOUNTAIN		     25
#define ITEM_PILL		     26
#define ITEM_PROTECT		     27
#define ITEM_MAP		     28
#define ITEM_PORTAL		     29
#define ITEM_CATALYST		     30
#define ITEM_ROOM_KEY		     31
#define ITEM_GEM		     32
#define ITEM_JEWELRY		     33
#define ITEM_JUKEBOX		     34
#define ITEM_ARTIFACT		     35
#define ITEM_SHARECERT		     36
#define ITEM_ROOM_FLAME		     37
#define ITEM_INSTRUMENT		     38
#define ITEM_SEED		     39
#define ITEM_CART		     40
#define ITEM_SHIP		     41
#define ITEM_ROOM_DARKNESS	     42
#define ITEM_RANGED_WEAPON	     43
#define ITEM_SEXTANT		     44
#define ITEM_WEAPON_CONTAINER	     45
#define ITEM_ROOM_ROOMSHIELD         46
#define ITEM_BOOK		     47
#define ITEM_SMOKE_BOMB		     48
#define ITEM_STINKING_CLOUD	     49
#define ITEM_HERB  		     50
/*
#define ITEM_HERB_2		     51
#define ITEM_HERB_3		     52
#define ITEM_HERB_4		     53
#define ITEM_HERB_5		     54
#define ITEM_HERB_6		     55
*/
#define ITEM_SPELL_TRAP		     56
#define ITEM_WITHERING_CLOUD         57
#define ITEM_BANK		     59
#define ITEM_KEYRING	             60
#define ITEM_TRADE_TYPE 	     61
#define ITEM_ICE_STORM		     62
#define ITEM_FLOWER		     63
#define ITEM_EMPTY_VIAL		     64
#define ITEM_BLANK_SCROLL	     65
#define ITEM_MIST		     66
#define ITEM_SHRINE		     67
#define ITEM_WHISTLE                 68
#define ITEM_SHOVEL 	     	     69
#define ITEM_TOOL		     70
#define ITEM_PIPE		     71
#define ITEM_TATTOO			72
#define ITEM_INK			73
#define ITEM_PART		     74
#define ITEM_COMMODITY			75

/*
 * Extra flags.
 */
#define ITEM_GLOW		(A)
#define ITEM_HUM		(B)
#define ITEM_NORESTRING		(C)
#define ITEM_NOKEYRING		(D)
#define ITEM_EVIL		(E)
#define ITEM_INVIS		(F)
#define ITEM_MAGIC		(G)
#define ITEM_NODROP		(H)
#define ITEM_BLESS		(I)
#define ITEM_ANTI_GOOD		(J)
#define ITEM_ANTI_EVIL		(K)
#define ITEM_ANTI_NEUTRAL	(L)
#define ITEM_NOREMOVE		(M)
#define ITEM_INVENTORY		(N)
#define ITEM_NOPURGE		(O)
#define ITEM_ROT_DEATH		(P)
#define ITEM_NOSTEAL		(Q)
#define ITEM_HOLY		(R)
#define ITEM_HIDDEN		(S)
#define ITEM_NOLOCATE		(T)
#define ITEM_MELT_DROP		(U)
/* #define ITEM_HAD_TIMER          (V)
  #define ITEM_SELL_EXTRACT	(W) */
#define ITEM_FREEZE_PROOF	(X)
#define ITEM_BURN_PROOF		(Y)
#define ITEM_NOUNCURSE		(Z)
#define ITEM_NOSKULL	        (aa)
#define ITEM_PLANTED	        (bb)
#define ITEM_PERMANENT	        (cc)
#define ITEM_SHOP_BOUGHT        (dd) /* Free */
#define ITEM_NOQUEST		(ee)

/*
 * Extra2 object flags.
 */
#define ITEM_ALL_REMORT	 	(A)
#define ITEM_LOCKER		(B)
#define ITEM_TRUESIGHT		(C)
#define ITEM_SCARE		(D)
#define ITEM_SUSTAIN		(E)
#define ITEM_ENCHANTED		(F)
#define ITEM_EMITS_LIGHT	(G)
#define ITEM_FLOAT_USER		(H)
#define ITEM_SEE_HIDDEN		(I)
#define ITEM_TRAPPED		(J)
#define ITEM_WEED		(K)
#define ITEM_SUPER_STRONG	(L)
#define ITEM_REMORT_ONLY	(M)
#define ITEM_NO_LORE		(N)
#define ITEM_SELL_ONCE		(O)
#define ITEM_NO_HUNT		(P)
#define ITEM_NO_RESURRECT       (Q)
#define ITEM_NO_DISCHARGE       (R)
#define ITEM_NO_DONATE		(S)
#define ITEM_KEPT		(T)
#define ITEM_SINGULAR		(U)
#define ITEM_NO_ENCHANT		(V)
#define ITEM_NO_LOOT		(W)
#define ITEM_NO_CONTAINER       (X)
#define ITEM_THIRD_EYE          (Y)
#define ITEM_UNSEEN		(Z)
#define ITEM_BURIED		(aa)
#define ITEM_NOLOCKER		(bb)
#define ITEM_NOAUCTION		(cc)	/* Can't be auctioned */
#define ITEM_KEEP_VALUE		(dd)	/* Keep value when donated */

/* Extra3 */

#define ITEM_EXCLUDE_LIST	(A)	/* Exclude the object from the room content list. */
#define ITEM_NO_TRANSFER	(B)	// Block the object from being transferred by a script
#define ITEM_ALWAYS_LOOT	(C)	// Item will always be left behind, overriding both no_loot, no_drop and nouncurse.
#define ITEM_FORCE_LOOT		(D)	// Temporary version of ITEM_ALWAYS_LOOT, is removed off object when left behind.  Usually used by commands.
#define ITEM_CAN_DISPEL		(E)	// Allows the 'dispel room' spell to target it.
#define ITEM_KEEP_EQUIPPED	(F)	// Item will not be unequipped on death.
#define ITEM_NO_ANIMATE		(G)	// Similar to ITEM_NO_RESURRECT, but designed for animate dead, instead
#define ITEM_RIFT_UPDATE	(H)	// Allows the item to update in the rift.
#define ITEM_INSTANCE_OBJ	(Z)	// Object is part of an instance

/* Extra4 */


/*
 * Wear flags.
 */
#define ITEM_TAKE				(A)
#define ITEM_WEAR_FINGER		(B)
#define ITEM_WEAR_NECK			(C)
#define ITEM_WEAR_BODY			(D)
#define ITEM_WEAR_HEAD			(E)
#define ITEM_WEAR_LEGS			(F)
#define ITEM_WEAR_FEET			(G)
#define ITEM_WEAR_HANDS			(H)
#define ITEM_WEAR_ARMS			(I)
#define ITEM_WEAR_SHIELD		(J)
#define ITEM_WEAR_ABOUT			(K)
#define ITEM_WEAR_WAIST			(L)
#define ITEM_WEAR_WRIST			(M)
#define ITEM_WIELD				(N)
#define ITEM_HOLD				(O)
#define ITEM_NO_SAC				(P)
#define ITEM_WEAR_FLOAT			(Q)
#define ITEM_WEAR_RING_FINGER   (R)
#define ITEM_WEAR_BACK			(S)
#define ITEM_WEAR_SHOULDER		(T)
#define ITEM_WEAR_FACE			(U)
#define ITEM_WEAR_EYES			(V)
#define ITEM_WEAR_EAR			(W)
#define ITEM_WEAR_ANKLE			(X)
#define ITEM_CONCEALS			(Y)
#define ITEM_WEAR_TABARD		(Z)

#define ITEM_NONWEAR			(ITEM_TAKE|ITEM_NO_SAC|ITEM_CONCEALS)


/* trade items */
#define TRADE_NONE              0
#define TRADE_WEAPONS           1
#define TRADE_FARMING_EQ	2
#define TRADE_PRECIOUS_GEMS	3
#define TRADE_IRON_ORE		4
#define TRADE_WOOD		5
#define TRADE_FARMING_FOOD	6
#define TRADE_SLAVES 		7
#define TRADE_PIGS		8
#define TRADE_PAPER		9
#define TRADE_GOLD		10
#define TRADE_SILVER		11
#define TRADE_SPICES		12
#define TRADE_CANNONS		13
#define TRADE_EXOTIC_SEEDS	14
#define TRADE_SPECIAL 		15
#define TRADE_REAGENTS 		16
#define TRADE_PASSENGER		17
#define TRADE_CONTRABAND        18
#define TRADE_LAST		-1

/* Commodities
  value 0 - type */
#define COMMODITY_NONE		0
#define COMMODITY_ORE		1
#define COMMODITY_GEM		2	/* Raw, unextracted gems */
#define COMMODITY_STONE		3
#define COMMODITY_METAL		4
#define COMMODITY_WOOD		5
#define COMMODITY_CLOTH		6
#define COMMODITY_LEATHER	7

/* value 1 - subtype, generic list
   value 2 - quality, percentage (0: poor, 100: best)
   value 3 - quantity */



/* weapon class */
#define WEAPON_UNKNOWN         -1
#define WEAPON_EXOTIC			0
#define WEAPON_SWORD			1
#define WEAPON_DAGGER			2
#define WEAPON_SPEAR			3
#define WEAPON_MACE				4
#define WEAPON_AXE				5
#define WEAPON_FLAIL			6
#define WEAPON_WHIP				7
#define WEAPON_POLEARM			8
#define WEAPON_STAKE			9
#define WEAPON_QUARTERSTAFF		10
#define WEAPON_ARROW			11
#define WEAPON_BOLT				12
#define WEAPON_DART				13	/* @@@NIB : 20070126 */
#define WEAPON_HARPOON			14	/* @@@NIB : 20070126 */
#define WEAPON_TYPE_MAX			15

/* ranged weapon class */
#define RANGED_WEAPON_EXOTIC		0
#define RANGED_WEAPON_CROSSBOW		1
#define RANGED_WEAPON_BOW			2
#define RANGED_WEAPON_BLOWGUN		3	/* @@@NIB : 20070126 */
#define RANGED_WEAPON_HARPOON		4	/* @@@NIB : 20070126 */
#define RANGED_WEAPON_SPEAR			5

/* weapon types */
#define WEAPON_FLAMING		(A)
#define WEAPON_FROST		(B)
#define WEAPON_VAMPIRIC		(C)
#define WEAPON_SHARP		(D)
#define WEAPON_VORPAL		(E)
#define WEAPON_TWO_HANDS	(F)
#define WEAPON_SHOCKING		(G)
#define WEAPON_POISON		(H)
#define WEAPON_THROWABLE	(I)
#define WEAPON_ACIDIC		(J)	/* @@@NIB : 20070209 */
#define WEAPON_ANNEALED		(K)	/* @@@NIB : 20070209 */
#define WEAPON_BARBED		(L)	/* @@@NIB : 20070209 */
#define WEAPON_CHIPPED		(M)	/* @@@NIB : 20070209 */
#define WEAPON_DULL		(N)	/* @@@NIB : 20070209 */
#define WEAPON_OFFHAND		(O)	/* @@@NIB : 20070209 */
#define WEAPON_ONEHAND		(P)	/* @@@NIB : 20070209 */
#define WEAPON_RESONATE		(Q)	/* @@@NIB : 20070209 */
#define WEAPON_BLAZE		(R)	/* @@@NIB : 20070209 */
#define WEAPON_SUCKLE		(S)	/* @@@NIB : 20070209 */


// Instrument class
#define INSTRUMENT_VOCAL			-2
#define INSTRUMENT_ANY				-1
#define INSTRUMENT_NONE				0
#define INSTRUMENT_WIND_REED		1	// Clarinet, Saxophone, bagpipes
#define INSTRUMENT_WIND_FLUTE		2	// Flute, Piccolo
#define INSTRUMENT_WIND_BRASS		3	// Trumpet
#define INSTRUMENT_DRUM				4	// Drums
#define INSTRUMENT_PERCUSSION		5	// Symbols, triangles, etc
#define INSTRUMENT_CHORDED			6	// Pianos, harpsicords
#define INSTRUMENT_STRING			7	// Violins, harps
#define INSTRUMENT_MAX				8	// Anything beyond this is considered "custom"

// Instrument flags
#define INSTRUMENT_ONEHANDED		(A)



/* gate flags */
#define GATE_NORMAL_EXIT	(A)
#define GATE_NOCURSE		(B)
#define GATE_GOWITH		(C)
#define GATE_BUGGY		(D)
#define GATE_RANDOM		(E)
#define GATE_AREARANDOM		(F)
#define GATE_GRAVITY		(G)	/* @@@NIB : 20070126 */
#define GATE_NOSNEAK		(H)	/* @@@NIB : 20070126 */
#define GATE_NOPRIVACY		(I)	/* @@@NIB : 20070126 */
#define GATE_SAFE		(J)	/* @@@NIB : 20070126 */
#define GATE_SILENTENTRY	(K)	/* @@@NIB : 20070126 */
#define GATE_SILENTEXIT		(L)	/* @@@NIB : 20070126 */
#define GATE_SNEAK		(M)	/* @@@NIB : 20070126 */
#define GATE_TURBULENT		(N)	/* @@@NIB : 20070126 */
#define GATE_CANDRAGITEMS	(O)
#define GATE_FORCE_BRIEF	(P)
#define GATE_DUNGEON		(Q)
#define GATE_SECTIONRANDOM	(R)
#define GATE_INSTANCERANDOM (S)
#define GATE_DUNGEONRANDOM	(T)

/* furniture flags */
#define STAND_AT		(A)
#define STAND_ON		(B)
#define STAND_IN		(C)
#define SIT_AT			(D)
#define SIT_ON			(E)
#define SIT_IN			(F)
#define REST_AT			(G)
#define REST_ON			(H)
#define REST_IN			(I)
#define SLEEP_AT		(J)
#define SLEEP_ON		(K)
#define SLEEP_IN		(L)
#define PUT_AT			(M)
#define PUT_ON			(N)
#define PUT_IN			(O)
#define PUT_INSIDE		(P)

/*
 * Apply types (for affects).
 */
#define APPLY_NONE			0
#define APPLY_STR			1
#define APPLY_DEX			2
#define APPLY_INT			3
#define APPLY_WIS			4
#define APPLY_CON			5
#define APPLY_SEX			6
#define APPLY_MANA			12
#define APPLY_HIT			13
#define APPLY_MOVE			14
#define APPLY_GOLD			15
#define APPLY_AC			17
#define APPLY_HITROLL			18
#define APPLY_DAMROLL			19
#define APPLY_FRAGILITY			20		/* Objects */
#define APPLY_CONDITION			21		/* Objects */
#define APPLY_WEIGHT			22		/* Objects */
#define APPLY_AGGRESSION		23		/* Mobiles */
#define APPLY_SPELL_AFFECT		50

#define APPLY_SKILL			100
#define APPLY_SKILL_MAX			(APPLY_SKILL + MAX_SKILL)

#define APPLY_MAX		     20

/*
 * Values for containers (value[1]).
 */
#define CONT_CLOSEABLE		(A)
#define CONT_PICKPROOF		(B)
#define CONT_CLOSED		(C)
#define CONT_LOCKED		(D)
#define CONT_PUT_ON		(E)
#define CONT_SNAPKEY		(F)	/* @@@NIB : 20070126 */
#define CONT_PUSHOPEN		(G)	/* @@@NIB : 20070126 */
#define CONT_CLOSELOCK		(H)	/* @@@NIB : 20070126 */

/* Types of tools */
enum {
	TOOL_NONE = 0,
	TOOL_WHETSTONE,		/* Sharpening weapons */
	TOOL_CHISEL,		/* Carving gems */
	TOOL_PICK,		/* Picking locks */
	TOOL_SHOVEL,		/* Why is there an ITEM_SHOVEL?! */
	TOOL_TINDERBOX,		/* Light fires or pipes */
	TOOL_DRYING_CLOTH,	/* Used to dry plants for smoking! */
	TOOL_SMALL_NEEDLE,	/* Used to sew things */
	TOOL_LARGE_NEEDLE	/* Used to sew things */
};



/*
 * Well known room virtual numbers.
 */
#define ROOM_VNUM_DEFAULT	   	1
#define ROOM_VNUM_LIMBO		   	2
#define ROOM_VNUM_CHAT                  342
#define ROOM_VNUM_MYSTICA               1611
#define ROOM_VNUM_TEMPLE                11001
#define ROOM_VNUM_DONATION              11174
#define ROOM_VNUM_ALTAR                 11001
#define ROOM_VNUM_SCHOOL                3700
#define ROOM_VNUM_NDEATH                3734
#define ROOM_VNUM_EVIL                  11022
#define ROOM_VNUM_GOOD                  11051
#define ROOM_VNUM_PLITH_AIRSHIP         11451
#define ROOM_VNUM_ARENA                 10513
#define ROOM_VNUM_DEATH                 6502

#define ROOM_VNUM_SEA_DEEP	        5000005
#define ROOM_VNUM_SEA_SHALLOW	        5000006
#define ROOM_VNUM_SEA_PLITH_HARBOUR	5000027
#define ROOM_VNUM_SEA_SOUTHERN_HARBOUR	5000048
#define ROOM_VNUM_SEA_NORTHERN_HARBOUR	5000049
#define ROOM_VNUM_PIER                  5000002

#define ROOM_VNUM_SAILING_BOAT_HELM	157000
#define ROOM_VNUM_SAILING_BOAT_NEST	157001
#define ROOM_VNUM_SAILING_BOAT_STERN	157002


#define ROOM_VNUM_CARGO_SHIP_HELM  157010
#define ROOM_VNUM_CARGO_SHIP_NEST  157011
#define ROOM_VNUM_CARGO_SHIP_BOW     157012
#define ROOM_VNUM_CARGO_SHIP_STERN   157013
#define ROOM_VNUM_CARGO_SHIP_CARGO   157014

#define ROOM_VNUM_AIR_SHIP_HELM	 	157020
#define ROOM_VNUM_AIR_SHIP_DECK	 	157021

/* Area numbers */
#define AREA_VNUM_SHIP_CARGO     	45
#define AREA_VNUM_SHIP_FRIGATE   	46
#define AREA_VNUM_SHIP_GALLEON   	47
#define AREA_VNUM_SHIP_WAR_GALLEON 	48

/*
 * Room flags.
 */
#define ROOM_DARK		(A)
#define ROOM_BANK		(B)
#define ROOM_NO_MOB		(C)
#define ROOM_INDOORS		(D)
#define ROOM_NO_WANDER		(E)
#define ROOM_SHIP_HELM		(F)
#define ROOM_NOCOMM		(G)
#define ROOM_VIEWWILDS		(H)
#define ROOM_SHIP_SHOP		(I)
#define ROOM_PRIVATE		(J)
#define ROOM_SAFE		(K)
#define ROOM_SOLITARY		(L)
#define ROOM_PET_SHOP		(M)
#define ROOM_NO_RECALL		(N)
#define ROOM_IMP_ONLY		(O)
#define ROOM_GODS_ONLY		(P)
#define ROOM_NOVIEW  		(Q)
#define ROOM_NEWBIES_ONLY	(R)
#define ROOM_NOMAP		(S)
#define ROOM_NOWHERE		(T)
#define ROOM_PK		        (U)
#define ROOM_CPK		(V)
#define ROOM_ARENA		(W)
#define ROOM_UNDERWATER		(X)
#define ROOM_ROCKS	        (Y)
#define ROOM_ATTACK_IF_DARK	(Z)
#define ROOM_DEATH_TRAP		(aa)
#define ROOM_LOCKER		(bb)
#define ROOM_HOUSE_UNSOLD	(cc)
#define ROOM_NOMAGIC		(dd)
#define ROOM_MOUNT_SHOP		(ee)

/*
 * Room2 flags
 */
#define ROOM_NO_QUIT		(A)
#define ROOM_NO_QUEST		(B)
#define ROOM_ICY		(C)
#define ROOM_FIRE		(D)
#define ROOM_POST_OFFICE	(E)
#define ROOM_ALCHEMY		(F)
#define ROOM_BAR    		(G)
#define ROOM_BRIARS		(H)	/* @@@NIB : 20070126  */
#define ROOM_TOXIC_BOG		(I)	/* @@@NIB : 20070126 */
#define ROOM_SLOW_MAGIC		(J)	/* @@@NIB : 20070126 : makes magic take longer to cast */
#define ROOM_HARD_MAGIC		(K)	/* @@@NIB : 20070126 : makes magic harder to cast */
#define ROOM_DRAIN_MANA		(L)	/* @@@NIB : 20070126 : drains mana */
#define ROOM_MULTIPLAY		(M)
#define ROOM_CITYMOVE		(N)
/* VIZZWILDS */
#define ROOM_VIRTUAL_ROOM		(O)
#define ROOM_PURGE_EMPTY	(P)
#define ROOM_UNDERGROUND	(Q)
#define ROOM_VISIBLE_ON_MAP	(R)	/* Used with coords - will show people in room on VMAP, even if !ROOM_VIEWWILDS */
#define ROOM_NOFLOOR		(S)	/* The room requires you to be flying, on non-takable furniture or floating furniture */
#define ROOM_CLONE_PERSIST	(T)	/* Set on rooms that can be cloned.  If set, allows clone rooms to be made persistant */
#define ROOM_BLUEPRINT		(U)	// Room is used in a blueprint.  Allows setting the coordinates without the wilderness
#define ROOM_NOCLONE		(V)	// Room cannot be used for cloning
#define ROOM_NO_GET_RANDOM	(W)	// Room cannot be selected on a random search
#define ROOM_ALWAYS_UPDATE	(Z)	/* Allows the room to perform scripting even if the area is empty */

#define ENVIRON_NONE		0	// Special case to indicate the clone room is free floating
#define ENVIRON_ROOM		1
#define ENVIRON_MOBILE		2
#define ENVIRON_OBJECT		3
#define ENVIRON_TOKEN		4	// Environment floats based upon owner of token

/*
 * Directions.
 */
#define DIR_NORTH		      0
#define DIR_EAST		      1
#define DIR_SOUTH		      2
#define DIR_WEST		      3
#define DIR_UP			      4
#define DIR_DOWN		      5
#define DIR_NORTHEAST		      6
#define DIR_NORTHWEST		      7
#define DIR_SOUTHEAST		      8
#define DIR_SOUTHWEST		      9

/*
 * Exit flags.
 * Used in #ROOMS.
 */
#define EX_ISDOOR		      (A)
#define EX_CLOSED		      (B)
#define EX_LOCKED		      (C)
#define EX_HIDDEN		      (D)
#define EX_FOUND		      (E)
#define EX_PICKPROOF		      (F)
#define EX_NOPASS		      (G)
#define EX_EASY			      (H)
#define EX_HARD			      (I)
#define EX_INFURIATING		      (J)
#define EX_NOCLOSE		      (K)
#define EX_NOLOCK		      (L)
#define EX_BROKEN		      (M)
#define EX_BARRED		      (N)
#define EX_NOBASH		      (O)
#define EX_WALKTHROUGH		      (P)
#define EX_NOBAR		      (Q)
/* VIZZWILDS */
#define EX_VLINK                      (R)
#define EX_AERIAL			(S)
#define EX_NOHUNT			(T)
#define EX_ENVIRONMENT			(U)
#define EX_NOUNLINK				(V)
#define EX_PREVFLOOR			(W)
#define EX_NEXTFLOOR			(X)
#define EX_NOSEARCH				(Y)		// Makes hidden exits unsearchable
#define EX_MUSTSEE				(Z)		// Requires the exit be visible to use it

/*
 * Area Who Flags
 */
enum {
	AREA_BLANK = 0,
	AREA_ABYSS,
	AREA_AERIAL,
	AREA_ARENA,
	AREA_AT_SEA,
	AREA_BATTLE,
	AREA_CASTLE,
	AREA_CAVERN,
	AREA_CHAT,
	AREA_CHURCH,
	AREA_CULT,
	AREA_DUNGEON,
	AREA_EDEN,
	AREA_FOREST,
	AREA_FORT,
	AREA_HOME,
	AREA_INN,
	AREA_ISLE,
	AREA_JUNGLE,
	AREA_KEEP,
	AREA_LIMBO,
	AREA_MOUNTAIN,
	AREA_NETHERWORLD,
	AREA_OFFICE,
	AREA_ON_SHIP,
	AREA_OUTPOST,
	AREA_PALACE,
	AREA_PG,
	AREA_PLANAR,
	AREA_PYRAMID,
	AREA_RUINS,
	AREA_SOCIAL,
	AREA_SWAMP,
	AREA_TEMPLE,
	AREA_TOMB,
	AREA_TOWER,
	AREA_TOWNE,
	AREA_TUNDRA,
	AREA_UNDERSEA,
	AREA_VILLAGE,
	AREA_VOLCANO,
	AREA_WILDER,
	AREA_INSTANCE,
	AREA_DUTY,
	AREA_WHO_MAX
};

/*
 * Wilderness types
 */
#define WILDERNESS_MAIN        (A)
#define WILDERNESS_NETHERWORLD (B)
#define WILDERNESS_EDEN        (C)

/*
 * Area flags.
 */
#define AREA_NONE       	0
#define AREA_CHANGED    	(A)
#define AREA_ADDED      	(B)
#define AREA_LOADING    	(C)
#define AREA_DARK			(D)
#define AREA_NOMAP			(E)
#define AREA_TESTPORT		(F)
#define AREA_NO_RECALL		(G)
#define AREA_NO_ROOMS		(H)
#define AREA_NEWBIE			(I)
#define AREA_NO_GET_RANDOM	(J)
#define AREA_NO_FADING		(K)
#define AREA_BLUEPRINT		(L)		// Area is used to hold rooms used for Blueprints.  Will block VLINKs
#define AREA_LOCKED			(M)		// Area requires the player to unlock the area first
#define AREA_NO_SAVE		(Z)

/*
 * Sector types.
 * Used in #ROOMS.
 */
#define SECT_INSIDE		      0
#define SECT_CITY		      1
#define SECT_FIELD		      2
#define SECT_FOREST		      3
#define SECT_HILLS		      4
#define SECT_MOUNTAIN		      5
#define SECT_WATER_SWIM		      6
#define SECT_WATER_NOSWIM	      7
#define SECT_TUNDRA		      8
#define SECT_AIR		      9
#define SECT_DESERT		     10
#define SECT_NETHERWORLD	     11
#define SECT_DOCK		     12
#define SECT_ENCHANTED_FOREST	     13
#define SECT_TOXIC_BOG		14
#define SECT_CURSED_SANCTUM	15
#define SECT_BRAMBLE		16
#define SECT_SWAMP		17
#define SECT_ACID		18
#define SECT_LAVA		19
#define SECT_SNOW		20
#define SECT_ICE		21
#define SECT_CAVE		22
#define SECT_UNDERWATER		23
#define SECT_DEEP_UNDERWATER	24
#define SECT_JUNGLE		25
#define SECT_MAX		26


#define WEAR_PARAM_SEEN		0
#define WEAR_PARAM_AUTOEQ	1
#define WEAR_PARAM_REMOVE	2
#define WEAR_PARAM_SHIFTED	3
#define WEAR_PARAM_AFFECTS	4
#define WEAR_PARAM_UNEQ_DEATH	5
#define WEAR_PARAM_ALWAYSREMOVE	6

#define WEAR_REMOVEEQ(x)			(wear_params[(x)][WEAR_PARAM_REMOVE])
#define WEAR_AUTOEQUIP(x)			(wear_params[(x)][WEAR_PARAM_AUTOEQ])
#define WEAR_UNEQUIP_DEATH(x)		(wear_params[(x)][WEAR_PARAM_UNEQ_DEATH])
#define WEAR_ALWAYSREMOVE(x)		(wear_params[(x)][WEAR_PARAM_ALWAYSREMOVE])

/*
 * Equipment wear locations.
 */
#define WEAR_NONE		-1
#define WEAR_LIGHT		0
#define WEAR_FINGER_L		1
#define WEAR_FINGER_R		2
#define WEAR_NECK_1		3
#define WEAR_NECK_2		4
#define WEAR_BODY		5
#define WEAR_HEAD		6
#define WEAR_LEGS		7
#define WEAR_FEET		8
#define WEAR_HANDS		9
#define WEAR_ARMS		10
#define WEAR_SHIELD		11
#define WEAR_ABOUT		12
#define WEAR_WAIST		13
#define WEAR_WRIST_L		14
#define WEAR_WRIST_R		15
#define WEAR_WIELD		16
#define WEAR_HOLD		17
#define WEAR_SECONDARY		18
#define WEAR_RING_FINGER	19
#define WEAR_BACK		20
#define WEAR_SHOULDER		21
#define WEAR_ANKLE_L		22
#define WEAR_ANKLE_R		23
#define WEAR_EAR_L		24
#define WEAR_EAR_R		25
#define WEAR_EYES		26
#define WEAR_FACE		27
#define WEAR_TATTOO_HEAD	28
#define WEAR_TATTOO_TORSO	29
#define WEAR_TATTOO_UPPER_ARM_L	30
#define WEAR_TATTOO_UPPER_ARM_R	31
#define WEAR_TATTOO_UPPER_LEG_L	32
#define WEAR_TATTOO_UPPER_LEG_R	33
#define WEAR_LODGED_HEAD	34
#define WEAR_LODGED_TORSO	35
#define WEAR_LODGED_ARM_L	36
#define WEAR_LODGED_ARM_R	37
#define WEAR_LODGED_LEG_L	38
#define WEAR_LODGED_LEG_R	39
#define WEAR_ENTANGLED		40
#define WEAR_CONCEALED		41
#define WEAR_FLOATING		42
#define WEAR_TATTOO_LOWER_ARM_L	43
#define WEAR_TATTOO_LOWER_ARM_R	44
#define WEAR_TATTOO_LOWER_LEG_L	45
#define WEAR_TATTOO_LOWER_LEG_R	46
#define WEAR_TATTOO_SHOULDER_L	47
#define WEAR_TATTOO_SHOULDER_R	48
#define WEAR_TATTOO_BACK	49
#define WEAR_TABARD			50
#define MAX_WEAR		51

/*
 * Conditions.
 */
#define COND_DRUNK		      0
#define COND_FULL		      1
#define COND_THIRST		      2
#define COND_HUNGER		      3
#define COND_STONED		      4

/*
 * Positions.
 */
#define POS_DEAD		      0
#define POS_MORTAL		      1
#define POS_INCAP		      2
#define POS_STUNNED		      3
#define POS_SLEEPING		      4
#define POS_RESTING		      5
#define POS_SITTING		      6
#define POS_FIGHTING		      7
#define POS_STANDING		      8
#define POS_FEIGN		      9
#define POS_HELDUP		      10

#define PLR_IS_NPC		(A)
#define PLR_EXCOMMUNICATED	(B)
#define PLR_PK			(C)
#define PLR_AUTOEXIT		(D)
#define PLR_AUTOLOOT		(E)
#define PLR_AUTOSAC             (F)
#define PLR_AUTOGOLD		(G)
#define PLR_AUTOSPLIT		(H)
#define PLR_AUTOSPELL	        (I)
#define PLR_AUTOSETNAME		(J)
#define PLR_HOLYLIGHT		(N)
#define PLR_SHOWDAMAGE          (O)
#define PLR_AUTOEQ	        (P)
#define PLR_NOSUMMON		(Q)
#define PLR_NOFOLLOW		(R)
#define PLR_COLOUR		(T)
#define PLR_NOTIFY		(U)
#define PLR_LOG			(W)
#define PLR_DENY		(X)
#define PLR_FREEZE		(Y)
#define PLR_BUILDING		(Z)
#define PLR_HELPER		(aa)
#define PLR_BOTTER		(bb)
#define PLR_NO_CHALLENGE	(cc)
#define PLR_NO_RESURRECT	(dd)
#define PLR_PURSUIT 		(ee)

/* Plr2 flags */
#define PLR_AUTOSURVEY		(A)
#define PLR_SACRIFICE_ALL	(B)
#define PLR_NO_WAKE		(C)
#define PLR_HOLYAURA		(D)
#define PLR_MOBILE			(E)
#define PLR_FAVSKILLS		(F)
#define PLR_HOLYWARP		(G)
#define PLR_NORECKONING		(H)

#define COMM_QUIET              (A)
#define COMM_NOMUSIC           	(B)
#define COMM_NOWIZ              (C)
#define COMM_NOAUCTION          (D)
#define COMM_NOGOSSIP           (E)
#define COMM_NOANNOUNCE         (F)
#define COMM_NOHELPER           (G)
#define COMM_NOCT		(H)
#define COMM_SOCIAL		(I)
#define COMM_NOTIFY		(J)
#define COMM_NOHINTS		(K)
#define COMM_COMPACT		(L)
#define COMM_BRIEF		(M)
#define COMM_PROMPT		(N)
#define COMM_FLAGS		(O)
#define COMM_TELNET_GA		(P)
#define COMM_NO_FLAMING         (Q)
#define COMM_NOGQ		(R)
#define COMM_NO_OOC		(S)
#define COMM_NOYELL             (T)
#define COMM_NOAUTOWAR   	(U)
#define COMM_NOTELL		(V)
#define COMM_NOCHANNELS		(W)
#define COMM_NOQUOTE		(Y)
#define COMM_AFK		(Z)
#define COMM_NOBATTLESPAM	(aa)
#define COMM_NOMAP		(cc)
#define COMM_NOTELLS		(dd)
#define COMM_SHOW_FORM_STATE    (ee)


/* WIZnet flags */
#define WIZ_ON			(A)
#define WIZ_TICKS		(B)
#define WIZ_LOGINS		(C)
/* #define WIZ_SITES		(D) */
#define WIZ_LINKS		(E)
#define WIZ_DEATHS		(F)
#define WIZ_RESETS		(G)
#define WIZ_MOBDEATHS		(H)
/* #define WIZ_FLAGS		(I) */
#define WIZ_PENALTIES		(J)
/* #define WIZ_SACCING		(K) */
#define WIZ_LEVELS		(L)
#define WIZ_SECURE		(M)
#define WIZ_SWITCHES		(N)
#define WIZ_SNOOPS		(O)
#define WIZ_RESTORE		(P)
#define WIZ_LOAD		(Q)
#define WIZ_NEWBIE		(R)
#define WIZ_PREFIX		(S)
#define WIZ_SPAM		(T)
#define WIZ_MEMCHECK		(U)
#define WIZ_IMMLOG		(V)
#define WIZ_TESTING		(W)
#define WIZ_BUILDING		(X)
#define WIZ_SCRIPTS		(Y)
#define WIZ_SHIPS		(Z)

/*
 * Autowars
 */
#define ROOM_VNUM_AUTO_WAR 	10001
#define AUTO_WAR_GENOCIDE 	(0)
#define AUTO_WAR_JIHAD    	(1)
#define AUTO_WAR_FREE_FOR_ALL 	(2)


#define RAWKILL_TYPE1		(-4)
#define RAWKILL_TYPE2		(-3)
#define RAWKILL_ANY		(-2)
#define RAWKILL_NOCORPSE	(-1)	/* Leaves no corpse */
#define RAWKILL_NORMAL		(0)	/* Leaves a corpse (normal dead body) */
#define RAWKILL_CHARRED		(1)	/* Leaves a CHARRED corpse */
#define RAWKILL_FROZEN		(2)	/* Leaves a FROZEN corpse */
#define RAWKILL_MELTED		(3)	/* Leaves a MELTED corpse */
#define RAWKILL_WITHERED	(4)	/* Leaves a WITHERED corpse (and very brittle!) */
#define RAWKILL_ICEBLOCK	(5)	/* Leaves a corpse incased in ICE */
#define RAWKILL_INCINERATE	(6)	/* Leaves a pile of ashes instead of corpse. */
#define RAWKILL_STONE		(7)	/* Turns the corpse into a statue. */
#define RAWKILL_SHATTER		(8)	/* Shatters the corpse, leaving debris */
#define RAWKILL_EXPLODE		(9)	/* Explodes the corpse, leaving body parts */
#define RAWKILL_DISSOLVE	(10)	/* Turns the body into a puddle of goo. */
#define RAWKILL_SKELETAL	(11)	/* Leaves a SKELETON */
#define RAWKILL_FLAY		(12)	/* Tears the outer layer of flesh off */
#define RAWKILL_MAX		(13)

#define CORPSE_CPKDEATH		(A)	/* The corpse was killed in CPK */
#define CORPSE_OWNERLOOT	(B)	/* Only allows the owner of the corpse to loot before full decay */
#define CORPSE_CHARRED		(C)
#define CORPSE_FROZEN		(D)
#define CORPSE_MELTED		(E)
#define CORPSE_WITHERED		(F)
#define CORPSE_PKDEATH		(G)	// The corpse was killed in a room with PK allowed, or player was flagged PK
#define CORPSE_ARENADEATH	(H)
#define CORPSE_IMMORTAL		(Z)	/* Corpse came from an immortal */

#define DEATHTYPE_ALIVE		0
#define DEATHTYPE_ATTACK	1
#define DEATHTYPE_RAWKILL	2	/* Any rawkill (scripted or slays) */
#define DEATHTYPE_KILLSPELL	3
#define DEATHTYPE_SLIT		4
#define DEATHTYPE_ROCKS		6
#define DEATHTYPE_BEHEAD	7
#define DEATHTYPE_SMITE		8
#define DEATHTYPE_MAGIC		9
#define DEATHTYPE_TOXIN		10
#define DEATHTYPE_DAMAGE	11	/* damage function */
#define DEATHTYPE_STAKE		12
#define DEATHTYPE_BREATH	13


#define CATALYST_ROOM		(A)	/* Searches the room */
#define CATALYST_CONTAINERS	(B)	/* Searches JUST your containers */
#define CATALYST_CARRY		(C)	/* Searches JUST your carried items */
#define CATALYST_WORN		(D)	/* Searches JUST your EQ */
#define CATALYST_HOLD		(E)	/* Checks to see if you are HOLD/WIELDing it */
#define CATALYST_ACTIVE		(F) /* Checks to see if an object catalyst has the extra3->ITEM_ACTIVATED flag set */
#define CATALYST_INVENTORY	(CATALYST_CONTAINERS|CATALYST_CARRY|CATALYST_WORN|CATALYST_HOLD)
#define CATALYST_HERE		(CATALYST_ROOM|CATALYST_INVENTORY)

#define CATALYST_NONE		0
#define CATALYST_ACID		1
#define CATALYST_AIR		2
#define CATALYST_ASTRAL		3	/* astrals, warpstones, etc....bloody good show! (this is a hack to preserve existing objects) */
#define CATALYST_BLOOD		4
#define CATALYST_BODY		5
#define CATALYST_CHAOS		6
#define CATALYST_COSMIC		7
#define CATALYST_DEATH		8
#define CATALYST_EARTH		9
#define CATALYST_ENERGY		10
#define CATALYST_FIRE		11
#define CATALYST_HOLY		12
#define CATALYST_ICE		13
#define CATALYST_LAW		14
#define CATALYST_LIGHT		15
#define CATALYST_MIND		16
#define CATALYST_NATURE		17
#define CATALYST_SOUND		18
#define CATALYST_TOXIN		19
#define CATALYST_WATER		20
#define CATALYST_SHOCK		21
#define CATALYST_DARKNESS	22
#define CATALYST_METALLIC	23
#define CATALYST_MANA		24
#define CATALYST_SOUL		25
#define CATALYST_MAX		26

#define CATALYST_MAXSTRENGTH	100

#define MAGIC_WEAR_SPELL	9000
#define MAGIC_SCRIPT_SPELL	18000

struct tattoo_design_data {
	char *name;		/* Name of design */
	int size;
	int difficulty;
	int essence[3][2];
};

/*
 * Prototype for a mob.
 */
struct	mob_index_data
{
    MOB_INDEX_DATA *	next;
    SPEC_FUN *		spec_fun;
    SHOP_DATA *		pShop;
    QUESTOR_DATA *	pQuestor;
    LLIST **        progs;
    QUEST_LIST *	quests;
    bool	persist;

    AREA_DATA *		area;
    long		vnum;
    sh_int		count;
    sh_int		killed;
    char *		player_name;
    char *		short_descr;
    char *		long_descr;
    char *		description;
    char *              sig;
    char *		creator_sig;
    long		act;
    long		act2;
    long		affected_by;
    long		affected_by2;
    sh_int		alignment;
    sh_int		level;
    sh_int		hitroll;
    DICE_DATA	hit;
    DICE_DATA	mana;
    DICE_DATA	damage;
    sh_int		ac[4];
    sh_int 		dam_type;
    long		off_flags;
    long		imm_flags;
    long		res_flags;
    int 		vuln_flags;
    sh_int		start_pos;
    sh_int		default_pos;

    sh_int		sex;
    sh_int		race;
    long		wealth;
    long		form;
    long		parts;
    sh_int		size;
    char *		material;
    /*long		mprog_flags; */
    long 		move;
    int			attacks;

    char *		owner; /* mainly for personal mounts */
    char *		skeywds; /* script keywords */
    pVARIABLE		index_vars;
    long		corpse;
    long		zombie;	/* Animated corpse */
    int			corpse_type;
    char *      comments;

	MOB_INDEX_SKILL_DATA *skills;

	bool		boss;
};


struct mail_data
{
    MAIL_DATA 	*next;

    OBJ_DATA 	*objects; 	/* objects in the package */
    char 	*sender; 	/* who sent it */
    char 	*recipient; 	/* who receives it */
    time_t 	sent_date; 	/* when sent */
    char 	*message; 	/* message included */
    bool	picked_up;	/* has it been picked up ? */
    int		status;		/* for keeping track of the mail is */
};

#define MAIL_BEING_DELIVERED	1
#define MAIL_DELIVERED		10
#define MAIL_DELETE	        2500
#define MAIL_RETURN	        15000


struct auction_data
{
    AUCTION_DATA   *next;
    OBJ_DATA  	   *item;
    CHAR_DATA 	   *owner;
    CHAR_DATA 	   *high_bidder;
    int 	   status;
    long	   current_bid;
    long 	   minimum_bid;
    long           silver_held;
    long           gold_held;
};


struct gq_data
{
    GQ_MOB_DATA *mobs;
    GQ_OBJ_DATA *objects;
};

struct questor_data
{
	QUESTOR_DATA *next;
	bool valid;

	long scroll;

	// Appearance data
	char *keywords;
	char *short_descr;
	char *long_descr;
	char *header;
	char *prefix;
	char *suffix;
	char *footer;

	int line_width;
};

#define QUESTOR_MOB		0
#define QUESTOR_OBJ		1
#define QUESTOR_ROOM	2

/* For randomly generated quests */
struct quest_data
{
    QUEST_DATA *        next;
    QUEST_PART_DATA *   parts;
    int					questgiver_type;
    long                questgiver;
    int					questreceiver_type;
    long				questreceiver;

    bool		msg_complete;
    bool		generating;
    bool		scripted;
};


struct quest_part_data
{
    QUEST_PART_DATA *   next;

    OBJ_DATA 		*pObj;
    char *		description;

    int			index;

    long		minutes;	// How many minutes is expected to complete this task?

    long 		obj;
    long		mob;
    long		room;
    long		obj_sac;
    long		mob_rescue;
    bool		custom_task;
    bool		complete;
};


/* For immortal/builder-made quests (can only be done once per char) */
struct quest_index_data
{
    QUEST_INDEX_DATA *next;
    AREA_DATA *area;
    QUEST_INDEX_PART_DATA *parts;

    long vnum;
    char *name;
    long exp_reward;
    int qp_reward;
    int prac_reward;
    int train_reward;
    int gold_reward;
    int silver_reward;
};


#define QUEST_PART_NONE		0
#define QUEST_PART_GET_OBJ	1
#define QUEST_PART_SAC_OBJ	2
#define QUEST_PART_GIVE_OBJ	3
#define QUEST_PART_DROP_OBJ	4
#define QUEST_PART_HIDE_OBJ	5
#define QUEST_PART_BURY_OBJ	6
#define QUEST_PART_DONATE_OBJ	7
#define QUEST_PART_RESCUE_MOB	8
#define QUEST_PART_SLAY_MOB	9
#define QUEST_PART_BRING_MOB	10

struct quest_index_part_data
{
    QUEST_INDEX_PART_DATA *next;

    int type;
    long phrase[7];
};


/* used in pMobIndex */
struct quest_list
{
    QUEST_LIST *next;

    long vnum;
};

#define MAX_TOKEN_VALUES 	8

struct token_index_data
{
    TOKEN_INDEX_DATA	*next;
    AREA_DATA		*area;
    LLIST		**progs;
    long 		vnum;

    char 		*name;
    char		*description;
    char        *comments;
    int			type;
    long 		flags;
    int			timer;
    long		loaded;		/* @@@NIB : 20070127 : for "tokenexists" ifcheck */
    long		value[MAX_TOKEN_VALUES];
    char 		*value_name[MAX_TOKEN_VALUES];

    EXTRA_DESCR_DATA	*ed;
    pVARIABLE		index_vars;
};

/* Token types */
#define TOKEN_GENERAL		0
#define TOKEN_QUEST	        1
#define TOKEN_AFFECT		2
#define TOKEN_SKILL		3
#define TOKEN_SPELL		4
#define TOKEN_SONG		5

/* Token flags */
#define TOKEN_PURGE_DEATH	(A)
#define TOKEN_PURGE_IDLE	(B)
#define TOKEN_PURGE_QUIT	(C)
#define TOKEN_PURGE_REBOOT 	(D)
#define TOKEN_PURGE_RIFT	(E)
#define TOKEN_REVERSETIMER	(F)
#define TOKEN_CASTING		(G)	/* Prevents the token from being touched by extract (except on character extraction) */
#define TOKEN_NOSKILLTEST	(H)	/* Doesn't do a skill test when action is completed */
#define TOKEN_SINGULAR		(I)
#define TOKEN_SEE_ALL		(J)	// Allows the token to ignore SIGHT rules
#define TOKEN_SPELLBEATS	(K)	// Allows the spell token to fire TRIG_SPELLBEATs while casting (as this can be spammy)
#define TOKEN_PERMANENT		(Z)	/* May not be removed unless the source is extracted.
									For players, this will be make them removable only by editting pfiles or through specific calls in the code */

/* Token Spell Values  */
#define TOKVAL_SPELL_RATING	0
#define TOKVAL_SPELL_DIFFICULTY	1
#define TOKVAL_SPELL_TARGET	2
#define TOKVAL_SPELL_POSITION	3
#define TOKVAL_SPELL_MANA	4
#define TOKVAL_SPELL_LEARN	5

struct token_data
{
	char __type;
	TOKEN_INDEX_DATA	*pIndexData;
	TOKEN_DATA		*global_next;
	TOKEN_DATA 		*next;
	TOKEN_DATA		*mate;
	CHAR_DATA		*player;
	OBJ_DATA		*object;
	ROOM_INDEX_DATA		*room;
	PROG_DATA		*progs;
	EVENT_DATA		*events;
	EVENT_DATA		*events_tail;
	ROOM_INDEX_DATA	*clone_rooms;
    LLIST	*lclonerooms;

	bool		valid;

	char 		*name;
	char		*description;
	int			type;
	long 		flags;
	int			timer;
	unsigned long	id[2];
	long		value[MAX_TOKEN_VALUES];

	EXTRA_DESCR_DATA	*ed;

	SKILL_ENTRY *skill;		// Is the token used in a skill entry?

    int			tempstore[MAX_TEMPSTORE];		/* Temporary storage values for script processing */
};


struct command_data
{
    COMMAND_DATA	*next;

    char 		*name;
};


/* For looking up classes in get_profession */
#define CLASS_CURRENT		-2
#define SUBCLASS_CURRENT	-3

#define SUBCLASS_MAGE		4
#define SUBCLASS_CLERIC		5
#define SUBCLASS_THIEF		6
#define SUBCLASS_WARRIOR	7

#define SECOND_SUBCLASS_MAGE	8
#define SECOND_SUBCLASS_CLERIC  9
#define SECOND_SUBCLASS_THIEF   10
#define SECOND_SUBCLASS_WARRIOR 11

#define SECOND_CLASS_MAGE	12
#define SECOND_CLASS_CLERIC	13
#define SECOND_CLASS_THIEF	14
#define SECOND_CLASS_WARRIOR	15

#define CLASS_NPC              -1
#define CLASS_MAGE 		0
#define CLASS_CLERIC 		1
#define CLASS_THIEF 		2
#define CLASS_WARRIOR 		3

#define CLASS_WARRIOR_MARAUDER 	0
#define CLASS_WARRIOR_GLADIATOR	1
#define CLASS_WARRIOR_PALADIN  	2
#define CLASS_MAGE_NECROMANCER 	3
#define CLASS_MAGE_SORCERER    	4
#define CLASS_MAGE_WIZARD      	5
#define CLASS_CLERIC_WITCH     	6
#define CLASS_CLERIC_DRUID      7
#define CLASS_CLERIC_MONK    	8
#define CLASS_THIEF_ASSASSIN   	9
#define CLASS_THIEF_ROGUE      	10
#define CLASS_THIEF_BARD       	11

#define CLASS_WARRIOR_WARLORD  	12
#define CLASS_WARRIOR_DESTROYER	13
#define CLASS_WARRIOR_CRUSADER 	14
#define CLASS_MAGE_ARCHMAGE    	15
#define CLASS_MAGE_GEOMANCER   	16
#define CLASS_MAGE_ILLUSIONIST 	17
#define CLASS_CLERIC_ALCHEMIST 	18
#define CLASS_CLERIC_RANGER    	19
#define CLASS_CLERIC_ADEPT     	20
#define CLASS_THIEF_HIGHWAYMAN 	21
#define CLASS_THIEF_NINJA      	22
#define CLASS_THIEF_SAGE       	23

#define SHIFTED_NONE   		0
#define SHIFTED_WEREWOLF   	1
#define SHIFTED_SLAYER 		2

/* Cap on queued events to prevent infinite recursion. */
#define MAX_EVENT_CALL_DEPTH    5

#define EVENT_MINTYPE		0
#define EVENT_MAXTYPE		4
#define EVENT_MOBQUEUE		0
#define EVENT_OBJQUEUE		1
#define EVENT_ROOMQUEUE		2
#define EVENT_TOKENQUEUE	3
#define EVENT_ECHO		4

struct event_data
{
   EVENT_DATA 	*next;         	/* Next in world list */
   EVENT_DATA	*next_event;	/* Next for that particular entity for speed */
   bool      	valid;
   int       	delay;
   void		*entity; 	/* Points to the memory space of the object/room/mob/token/... */
   int          event_type; 	/* Tells us what kind of structure it is so we can type cast accordingly */
   int      	action;         /* What type of action - currently just wait or function */
   int		depth;		/* @@@NIB current call depth at the time of creation */
   int		security;	/* @@@NIB current security level at the time of creation */
   void		*function;      /* Pointer to function to execute - void so OBJ_FUN and ROOM_FUN can also be used */
   char   	*args;		/* String arguments */
   SCRIPT_VARINFO *info;	/* @@@NIB : 20070123 : Script variable information */
};

#define OBJ_LAYERS		(1+1+5)
#define OBJ_LAYER_SKIN1		0
#define OBJ_LAYER_SKIN2		1
#define OBJ_LAYER_OUTER		(OBJ_LAYERS-1)

struct limb_data {
	int hit;
	int max_hit;
	int flags;
	AFFECT_DATA *affects;
	OBJ_DATA *obj[OBJ_LAYERS];
};

#define MAGICCAST_SUCCESS		0
#define MAGICCAST_FAILURE		1
#define MAGICCAST_ROOMBLOCK		2
#define MAGICCAST_SCRIPT		3	// Failure caused by scripting -

/*
 * One character (PC or NPC).
 */
struct	char_data
{
	char __type;
    CHAR_DATA *		next;
    CHAR_DATA *		next_persist;
    CHAR_DATA *		next_in_room;
    CHAR_DATA *		next_in_crew;
    CHAR_DATA *		next_in_hunting;
    CHAR_DATA *   	next_in_auto_war;
    CHAR_DATA *   	next_in_invasion;
    CHAR_DATA *		master;
    CHAR_DATA *		leader;
    CHAR_DATA *		pet;
    CHAR_DATA *		fighting;
    CHAR_DATA *		heldup;
    CHAR_DATA *		reply;
    PROG_DATA *		progs;
    TOKEN_DATA *	tokens;

    SPEC_FUN *		spec_fun;
    MOB_INDEX_DATA *	pIndexData;
    DESCRIPTOR_DATA *	desc;
    AFFECT_DATA *	affected;
    NOTE_DATA *		pnote;
    OBJ_DATA *		carrying;
    OBJ_DATA *		locker;
    MAIL_DATA *		mail;
    ROOM_INDEX_DATA *	home_room; /* for mobs to wander back home */
    ROOM_INDEX_DATA *	clone_rooms;
    time_t 		locker_rent;
    OBJ_DATA *		on;
    ROOM_INDEX_DATA *	in_room;
    ROOM_INDEX_DATA *	was_in_room;
    WILDS_DATA *was_in_wilds;
	ROOM_INDEX_DATA *	checkpoint;
	SHOP_DATA		* shop;

    /* VIZZWILDS */
    CHAR_DATA *        prev_in_wilds;
    CHAR_DATA *        next_in_wilds;
    WILDS_DATA *       in_wilds;
    int                at_wilds_x;
    int                at_wilds_y;
    int			was_at_wilds_x;
    int			was_at_wilds_y;
    long		was_in_room_id[2];

    PC_DATA *		pcdata;
    AMBUSH_DATA *	ambush;
    EVENT_DATA *	events;
	EVENT_DATA		*events_tail;
    bool		valid;
    bool		persist;

    char *		name;
    char *		short_descr;
    char *		long_descr;
    char *		description;
    char *		prompt;

    unsigned long	id[2];
    int			version;
    int			new_version;
    int			num_grouped;

    /*int			group; Syn - unused and unnecessary */
    int			sex;

    int			race;
    int			orace;
    int			level;
    int			tot_level;
    int			trust;
    int			played;
    int			lines;  /* for the pager */

    time_t		logon;

    /* timers */
    int			timer;
    int			wait;
    int			daze;
    int			cast;
    int			panic;
    int			music;
    int			brew;
    int			scribe;
    int			bind;
    int			recite;
    int			resurrect;
    int			ranged;
    int			hide;
    int			paroxysm;
    int			bomb;
    int			bashed;
    int			reverie;
    int			trance;
    int			paralyzed;
    int			pk_timer;
    int 		no_recall;
    int			inking;

    OBJ_DATA		*recite_scroll;
    OBJ_DATA 		*resurrect_target;
    char			*music_target;
    int				song_num;
    SCRIPT_DATA		*song_script;		/* The scripted spell to be done */
    TOKEN_DATA		*sont_token;		/* The token from which the script call is made */
    int				song_mana;
    OBJ_DATA		*song_instrument;

    int				script_wait;
    SCRIPT_DATA		*script_wait_success;
    SCRIPT_DATA		*script_wait_failure;
    SCRIPT_DATA		*script_wait_pulse;
    unsigned long	script_wait_id[2];

	CHAR_DATA		*script_wait_mob;
	OBJ_DATA		*script_wait_obj;
	ROOM_INDEX_DATA	*script_wait_room;
	TOKEN_DATA		*script_wait_token;

    int			fade;
    int			fade_dir;
    int			force_fading;		// Caused by scripting
    int			ship_move;
    int			ship_attack;

    /* airships only, cause they dont follow room links */
    float		ship_delta_time;
    int			ship_time;
    float		ship_delta_x;
    float		ship_delta_y;
    int			ship_src_x;
    int			ship_src_y;

    int			ship_dest_x;
    int			ship_dest_y;
    /* Only really for flying ships. Usually set to make everyone die incase someone kills the captain. */
    int        		ship_crash_time;

    /* This used to send airship back to plith. When the ship arrives it sets this. Then after 2 hours, it gos back to Plith again. */
    int			ship_arrival_time;
    int			ship_depart_time;

    int			brew_sn;
    int			scribe_sn;
    int			scribe_sn2;
    int			scribe_sn3;

    int			projectile_dir;
    OBJ_DATA *		projectile_weapon;
    int			projectile_range;
    OBJ_DATA *		projectile;
    char *		projectile_victim;

    CHAR_DATA 		*bind_victim;
    CHAR_DATA		*pursuit_by;

    /* Spell casting and targeting */
    int			cast_sn;
    int 		cast_level;
    char		*cast_target_name;
    SCRIPT_DATA		*cast_script;		/* The scripted spell to be done */
    TOKEN_DATA		*cast_token;		/* The token from which the script call is made */
    TOKEN_DATA		*song_token;
    char		*music_target_name;
    int			cast_mana;
    float		cast_mana_value;
    float		cast_mana_scale;

    /* invasion - invasion the leader belongs to */
    INVASION_QUEST *invasion_quest;

    long		hit;
    long		max_hit;
    long		mana;
    long		max_mana;
    long		move;
    long		max_move;

    long		gold;
    long		silver;
    long		exp;
    long		maxexp;

    long		act;
    long		act2;

    long		comm;
    long		wiznet;
    long		imm_flags_perm;		// 20140514 NIB - Used for the inherent flags for immunity - either from mob index or player's race
    long		res_flags_perm;		// 20140514 NIB - Used for the inherent flags for resistance - either from mob index or player's race
    long		vuln_flags_perm;	// 20140514 NIB - Used for the inherent flags for vulnerability - either from mob index or player's race
    long		imm_flags;
    long		res_flags;
    long		vuln_flags;
    long		has_done;
    int			invis_level;
    int			incog_level;
    long		affected_by;
    long		affected_by2;
    long		affected_by_perm;	// 20140514 NIB - Used for the inherent flags for affects - either from mob index or player's race
    long		affected_by2_perm;	// 20140514 NIB - Used for the inherent flags for affects2 - either from mob index or player's race
    int			position;
    int			practice;
    int			train;
    int			carry_weight;
    int			carry_number;
    int			saving_throw;
    int			alignment;
    int			hitroll;
    int			damroll;
    int			armour[4];
    int			wimpy;

    /* stats */
    int			perm_stat[MAX_STATS];
    int			mod_stat[MAX_STATS];
    int			cur_stat[MAX_STATS];
    bool		dirty_stat[MAX_STATS];

    /* parts */
    long		form;
    long		parts;
    long		lostparts;	/* Stuff you've lost! */
    int			size;
    char *		material;

    /* for mobs */
    long		off_flags;
    DICE_DATA	damage;
    int			dam_type;
    int			start_pos;
    int			default_pos;
    time_t      	hired_to;

    /* mount */
    CHAR_DATA *		mount;
    CHAR_DATA *		rider;
    bool		riding;

    int			time_left_death;
    int			maze_time_left;
    bool        	dead;

    /* shifting/morphing */
    int			shifted;
    bool		morphed;

    long        	home;

    /* Reverie */
    int			reverie_amount;
    int			reverie_type;

    /* Kill and death tallies */
    int 		deaths;
    int 		player_kills;
    int 		cpk_kills;
    int 		arena_kills;
    long 		monster_kills;
    int 		wars_won;
    int 		player_deaths;
    int 		cpk_deaths;
    int 		arena_deaths;

    /* Church */
    CHURCH_DATA *	church;
    char *		church_name;
    CHURCH_PLAYER_DATA *church_member;
    CHURCH_PLAYER_DATA *remove_question;

    /* Quest */
    QUEST_DATA *	quest;
    unsigned int     	questpoints;
    int              	nextquest;
    int              	countdown;

    CHAR_DATA *		hunting;

    CHAR_DATA *		challenged;
    CHAR_DATA *		challenger;
    long 		deitypoints;
    long 		pneuma;

    OBJ_DATA *		pulled_cart;

    /*
    This is for mobs - lets them know they belong to a ship if they are crew.
     */
    SHIP_DATA *		belongs_to_ship;


    /* Lets a person know which ship they have boarded */
    SHIP_DATA *		boarded_ship;

    bool 		has_head;
    LOCATION 		before_social;
    /*int 		court_rank; */

    /* Sith toxins */
    int 		toxin[MAX_TOXIN];
    int 		bitten;
    int 		bitten_type;
    int 		bitten_level;

    /* Dwarven repair */
    int			repair;
    OBJ_DATA		*repair_obj;
    int			repair_amt;

    bool 		pk_question;
    bool 		cross_zone_question;
    bool 		personal_pk_question;
    bool		remort_question;

    bool 		in_war;

    char 		*owner;
    int			ships_destroyed;

    /* Used for mobs who are after a target */
    char      		*target_name;
    int			queued;

    /* @@@NIB */
    int			death_type;		/* How you died last (0 for alive!) */
    int			set_death_type;

    int			corpse_type;
    long		corpse_vnum;

    /* @@@NIB */
    int			wildview_bonus_x;
    int			wildview_bonus_y;

    /* @@@NIB */
    int			ink_sn;
    int			ink_sn2;
    int			ink_sn3;
    int			ink_loc;
    CHAR_DATA		*ink_target;

    /* @@@NIB -  Set and used with HIT triggers only.  Any other time, these will be cleared
    		These allow for the mob to CONTROL what injures it, as well as allow for tokens
    		to create simulated damage reduction/amplication */
    int			hit_damage;		/* Amount of damage inflicted by the hit */
    int			hit_type;		/* Type of damage given */
    int			hit_class;		/* Class of damage given */
    int			skill_chance;		/* Current chance used in a given action */
    int			tempstore[MAX_TEMPSTORE];		/* Temporary storage values for script processing */
    char *		tempstring;
    int			manastore;		/* A storage for "mana" other than the character's mana */

	MOB_SKILL_DATA *mob_skills;

	// Sorted skills and spells (merging skills and tokens)
	SKILL_ENTRY *sorted_skills;
//	SKILL_ENTRY *sorted_spells;
	SKILL_ENTRY *sorted_songs;

	LOCATION		recall;

    LLIST *		llocker;
    LLIST *		lcarrying;
    LLIST *		lworn;
    LLIST *		ltokens;
    LLIST *		levents;
    LLIST *		lquests;	// Eventually, we will have a quest log of sorts
    LLIST *		lclonerooms;
    LLIST *		laffected;

    LLIST *		lgroup;

    int			deathsight_vision;
    int			cast_successful;	// Flag set when the casting is started indicating whether the result is successful
    								// - This will allow channeling spell tokens to know if the spell would be successful.
    								// - This will be modifiable by spell tokesn to force it to be a failure,
    								// -   or, if not already performed, attempt to switch the failure to a success.

    								// 0 = SUCCESS
    								// 1 = SKILL FAILURE
    								// 2 = ROOM FAILURE
    								// 3 = SCRIPTED
	bool		casting_recovered;	// Flag set if a spell recovery was attempted, regardless if it was successful.
	char		*casting_failure_message;

	bool		in_damage_function;	// If set, it will prevent damage_new from working on the character

/*
	struct char_data_stats {

	} stats;
	*/
	struct char_data_vitals {
		int mana;		/* Currently held mana */
		int mana_limit;		/* The maximum held mana one can have before bad things start to happen */
		int mana_sustained;	/* The minimum mana one must hold to sustain active spells. */
		int mana_charge;	/* The amount of mana charged */
		int mana_charge_eff;	/* The effective charged mana (mana_charge after diminishing returns) */
	} vitals;
	struct char_data_combat {
		int damage;		/* Amount of damage inflicted by the hit */
		int type;		/* Type of damage given */
		int wclass;		/* Class of damage given */
	} combat;
	struct char_data_healing {
		int healing;		/* Amount of healing done */
		int type;		/* Type of healing given */
	} healing;
	struct char_data_skills {
		int chance;		/* Used to modify skill chances */
		int stage;
	} skills;
	struct char_data_actions {
		int beats;
	} actions;
};


/* These values are used in a bitfield to store what type of channels will
   display those lovely colourful 10 character or less channel tags. */
#define FLAG_GOSSIP	(A)
#define FLAG_OOC	(B)
#define FLAG_YELL	(C)
#define FLAG_FLAMING	(D)
#define FLAG_QUOTE	(E)
#define FLAG_HELPER	(F)
#define FLAG_TELLS	(G)
#define FLAG_MUSIC	(H)
#define FLAG_CT		(I)

#define SHOW_CHANNEL_FLAG(ch, flag)  (!IS_NPC(ch) && IS_SET((ch)->pcdata->channel_flags, (flag)))

/*
 * Data which only PC's have.
 */
struct	pc_data
{
    PC_DATA *		next;
    BUFFER * 		buffer;
    IGNORE_DATA *	ignoring;
    OBJ_DATA *		corpse;
    COMMAND_DATA *	commands;
    IMMORTAL_DATA *	immortal; 	/* Encapsulates imm-staff data. NULL for mortals. */
    bool		valid;
    char *		pwd;
    char *		old_pwd;
    char *		title;
    char *		afk_message;
    char *		email;	/* person's email address */
    char *		flag;
    long	        channel_flags;
    long		creation_date;
    time_t              last_note;
    time_t              last_idea;
    time_t              last_penalty;
    time_t              last_news;
    time_t              last_changes;
    time_t		last_logoff;
    time_t		last_login;
    time_t		last_project_inquiry;

    int			class_current;
    int			sub_class_current;
    int			class_mage;
    int			second_class_mage;
    int			sub_class_mage;
    int			second_sub_class_mage;
    int			class_cleric;
    int			second_class_cleric;
    int			sub_class_cleric;
    int			second_sub_class_cleric;
    int			class_thief;
    int			second_class_thief;
    int			sub_class_thief;
    int			second_sub_class_thief;
    int			class_warrior;
    int			second_class_warrior;
    int			sub_class_warrior;
    int			second_sub_class_warrior;

    long		perm_hit;
    long		perm_mana;
    long		perm_move;
    int			true_sex;
    int			last_level;
    int			condition	[5];
    bool		songs_learned[MAX_SONGS];
    int			learned		[MAX_SKILL];
    int			mod_learned	[MAX_SKILL];
    bool		group_known	[MAX_GROUP];
    long		points;
    bool              	confirm_delete;
    char *		alias[MAX_ALIAS];
    char * 		alias_sub[MAX_ALIAS];
    int 		security;/* Builder security */

    int 		challenge_delay;
    signed long		bankbalance;
    long		hit_before;
    long		mana_before;
    long		move_before;

    long		quests_completed;
    LOCATION		room_before_arena;
    STRING_DATA		*vis_to_people; /* vis to this list of names */
    STRING_DATA		*quiet_people; /* these people can tell w/ quiet */

    /*QUEST_INDEX_DATA    *quests; / keep track of indexed quests and their parts

    char *  	    	owner_of_boat_before_logoff;
  long  	 	vnum_of_boat_before_logoff;


    int 		rank[3];
    int 		reputation[3];
    long  	ship_quest_points[3];
    long		bounty[3];
     */

    int 		convert_church; /* convert church? */
    int 		need_change_pw; /* for when we need to force people to change pw's */
    int 		danger_range;

    bool		quit_on_input;

    LOCATION		recall;
    PROJECT_INQUIRY_DATA *inquiry_subject; /* Prompts for subject upon addition to a project inquiry */
    STRING_VECTOR	*script_prompts;

    int         pwd_vers; /* Password version, added for sha256 */

    #ifdef IMC
        IMC_CHARDATA *imcchardata;
    #endif

    LLIST *unlocked_areas;
};


/*
 * Liquids.
 */
#define LIQ_WATER        0

#define LIQ_AFF_PROOF	0
#define LIQ_AFF_FULL	1
#define LIQ_AFF_THIRST	2
#define LIQ_AFF_HUNGER	3
#define LIQ_AFF_SSIZE	4
#define LIQ_AFF_FUEL	5	/* Fuel source quality */
#define LIQ_AFF_VAPOR	6	/* Vapor tendancy (VAPOR+FUEL+FLAME == FIRE!) */
#define LIQ_AFF_MAX	7

struct	liq_type
{
    char *	liq_name;
    char *	liq_colour;
    sh_int	liq_affect[LIQ_AFF_MAX];
};


/*
 * Extra description data for a room or object.
 */
struct	extra_descr_data
{
    EXTRA_DESCR_DATA *next;	/* Next in list                     */
    bool valid;
    char *keyword;              /* Keyword in look/examine          */
    char *description;          /* What to see                      */
};


struct  conditional_descr_data
{
    CONDITIONAL_DESCR_DATA *next;

    int condition;
    int phrase;

    char *description;
};


#define LOCK_LOCKED 		(A)		// Currently locked
#define LOCK_MAGIC			(B)		// Requires magic to break
#define LOCK_SNAPKEY		(C)		// Key snaps on use
#define LOCK_SCRIPT			(D)		// Requires a script to alter the lock to unlock it.
#define LOCK_NOREMOVE		(E)		// Cannot be removed by scripting
#define LOCK_BROKEN			(F)		// Lock has been damaged physically
#define LOCK_JAMMED			(G)		// Locking mechanism has been jammed
#define LOCK_NOJAM			(H)		// Lock does not allow being jammed

#define LOCK_CREATED		(Z)		// Lock was created by a script, so allows full alter exit manipulation

typedef struct lock_state_data {
	long key_vnum;
	int pick_chance;		// 0 = impossible (normally), 100 trivial
	int flags;
} LOCK_STATE;


/*
 * Prototype for an object.
 */
struct	obj_index_data
{
    OBJ_INDEX_DATA *	next;
    EXTRA_DESCR_DATA *	extra_descr;
    AFFECT_DATA *	affected;
    AFFECT_DATA *	catalyst;
    AREA_DATA *		area;
    bool	persist;

    LLIST **	progs;
    char *		name;
    char *		short_descr;
    char *		description;
    char * 		full_description;
    long		vnum;
    char *		material;
    sh_int		item_type;
    long 		extra_flags;
    long 		extra2_flags;
    long		extra3_flags;
    long		extra4_flags;
    long		wear_flags;
    sh_int		level;
    sh_int 		condition;
    sh_int		count;
    sh_int		weight;
    long		cost;
    sh_int 		fragility;
    sh_int		times_allowed_fixed;
    long		value[8];
    SPELL_DATA 		*spells;
    char *		imp_sig;
    char *		creator_sig;
    int 		points;
    bool 		update;
    int			timer;
    char *		skeywds; /* Script keywords */
    char *      comments;
    pVARIABLE		index_vars;

	int light;		/* Inherent light [-1000 to 1000] */
	int alpha;		/* Transparency of object [0,1000] (0.0% to 100.0%) */
	int heat;		/* How much heat is in it [0,100000] */
	int moisture;		/* How much moisture is in it [0,1000] */

	long inrooms;
	long inmail;
	long carried;
	long lockered;
	long incontainer;

	LOCK_STATE *lock;
};


/* A spell on an obj */
struct spell_data
{
    SPELL_DATA 		*next;

    int 		sn;
    int 		level;
    int			repop;
};


/*
 * One object.
 */
struct	obj_data
{
	char __type;
    OBJ_DATA *		next;		/* for the world list */
    OBJ_DATA *		next_persist;	// Next object in the persistance list
    OBJ_DATA *		next_content;
    OBJ_DATA *          prev_in_wilds;
    OBJ_DATA *          next_in_wilds;
    OBJ_DATA *		contains;
    OBJ_DATA *		mate;
    MAIL_DATA *		in_mail;
    PROG_DATA *		progs;
    int 		nest;
    OBJ_DATA *		in_obj;
    OBJ_DATA *		on;
    CHAR_DATA *		carried_by;
    CHAR_DATA *		pulled_by;
    EXTRA_DESCR_DATA *	extra_descr;
    AFFECT_DATA *	affected;
    AFFECT_DATA *	catalyst;
    OBJ_INDEX_DATA *	pIndexData;
    ROOM_INDEX_DATA *	in_room;
    ROOM_INDEX_DATA *	clone_rooms;
    EVENT_DATA *	events;
	EVENT_DATA		*events_tail;
    WILDS_DATA *        in_wilds;
    TOKEN_DATA *	tokens;
    int                 x;
    int                 y;
    unsigned long	id[2];
    bool		valid;
    bool		persist;
    int			num_enchanted;
    int			version; /* to keep objects updated */
    char *	        owner;
    char *		name;
    char *		short_descr;
    char *		description;
    char *		full_description;
    char *		old_name;
    char *		old_short_descr;
    char *		old_description;
    char *		old_full_description;
    char *		loaded_by;
    int			item_type;
    long		extra_flags;
    long		extra2_flags;
    long		extra3_flags;
    long		extra4_flags;
    int 		wear_flags;
    int			wear_loc;
    int			weight;
    long		cost;
    int			level;
    int 		condition;
    char *		material;
    int			timer;
    int 		fragility;
    int			times_allowed_fixed;
    int			times_fixed;
    int 		value	[8];
    SPELL_DATA		*spells;
    long		orig_vnum;

    LOCK_STATE		*lock;

    SHIP_DATA		*ship;

    int			trap_dam;
    int 		last_wear_loc;
    bool		locker;
    int			nest_clones;

    /* Used for pirate heads */
    int     pirate_reputation;
    int			queued;

	int light;		/* Inherent light [-1000 to 1000] */
	int alpha;		/* Transparency of object [0,1000] (0.0% to 100.0%) */
	int heat;		/* How much heat is in it [0,100000] */
	int moisture;		/* How much moisture is in it [0,1000] */

	LLIST *lcontains;
	LLIST *ltokens;
	LLIST *lclonerooms;

	/* 20140508 NIB - Used by corpses (at first) */
    char *owner_name;	/* Used to indicate the original mob's name for use decaying the corpse. */
    char *owner_short;	/* Used to indicate the original mob's short for use decaying the corpse. */

    long extra_flags_perm;
    long extra2_flags_perm;
    long extra3_flags_perm;
    long extra4_flags_perm;
    long weapon_flags_perm;	// Used by weapon objects for use with TO_WEAPON

    int			tempstore[MAX_TEMPSTORE];		/* Temporary storage values for script processing */

};

/* fragility */
#define OBJ_FRAGILE_NORMAL 	0
#define OBJ_FRAGILE_SOLID  	1
#define OBJ_FRAGILE_WEAK 	2
#define OBJ_FRAGILE_STRONG 	3

#define OBJ_ARMOUR_NOSTRENGTH 	0
#define OBJ_ARMOUR_LIGHT 	1
#define OBJ_ARMOUR_MEDIUM 	2
#define OBJ_ARMOUR_STRONG 	3
#define OBJ_ARMOUR_HEAVY 	4

#define OBJ_XPGAIN_GROUP	1		// XP gained from the group_gain function, which is only received when a kill is made

struct destination_data {
	ROOM_INDEX_DATA *room;	// For an existing room
	int wx;
	int wy;
	WILDS_DATA *wilds;
};

/*
 * Exit data.
 */
struct	exit_data
{
	ROOM_INDEX_DATA *from_room;
	EXIT_DATA *next;
	bool valid;

	union {
		ROOM_INDEX_DATA *to_room;
		long vnum;
	} u1;
	int exit_info;
	char *keyword;
	char *short_desc;  /* short description for displaying only */
	char *long_desc;   /* long desc for closer scrutiny */
	int rs_flags;
	int orig_door;

	struct door_data {
		sh_int strength;
		char *material;
		LOCK_STATE lock;
		LOCK_STATE rs_lock;
//		int rs_lock_flags;
//		int rs_pick_chance;
	} door;

	struct {
		ROOM_INDEX_DATA *source;
		int id[2];
	} croom;

	/* VIZZWILDS */
	struct wilds_loc_data {
		int x;
		int y;
		long area_uid;
		long wilds_uid;
	} wilds;
};

struct flag_stat_type
{
    const struct flag_type *structure;
    bool stat;
};

/*
 * Reset commands:
 *   '*': comment
 *   'M': read a mobile
 *   'O': read an object
 *   'P': put object in object
 *   'G': give object to mobile
 *   'E': equip object to mobile
 *   'D': set state of door
 *   'R': randomize room exits
 *   'S': stop (end of list)
 */

/*
 * Area-reset definition.
 */
struct	reset_data
{
    RESET_DATA *	next;
    char		command;
    long		arg1;
    long 		arg2;
    long		arg3;
    long		arg4;
};

/*
 * Area definition.
 */
struct	area_data {

	char __type;
	AREA_DATA *next;
	RESET_DATA *reset_first;
	RESET_DATA *reset_last;
	ROOM_INDEX_DATA *vrooms;
/* VIZZWILDS */
	WILDS_DATA *wilds;
	long uid;

	char *file_name;
	char *name;
	char *credits;
    char *  description;
    char *  comments;
	sh_int age;
	sh_int nplayer;
	sh_int low_range;
	sh_int high_range;
	long min_vnum;
	long max_vnum;
	bool empty;
	char *builders;
	long anum;
	long area_flags;
	int security;
	bool open;

	int x;
	int y;

	/* Areas have a landing coord in the wilds. This will be useful if we want flying
	 creatures that travel around. */
	int land_x;
	int land_y;
	long airship_land_spot;

	char *map;
	int map_size_x;
	int map_size_y;
	int startx;
	int starty;
	int endx;
	int endy;
	LOCATION recall;
	bool reset_once;
	int area_who;
/*	long area_who_flags;
	long area_who2_flags; */
	long place_flags;
	int repop; /* repop time in minutes */
	int version_area;
	int version_mobile;
	int version_object;
	int version_room;
	int version_token;
	int version_script;
	int version_wilds;

	SHIP_DATA *ship_list;
	TRADE_ITEM *trade_list;

	long post_office;

	INVASION_QUEST *invasion_quest;

	/* For weather - list of all storms (wilderness only) */
	STORM_DATA *storm;

	/* The storm the area is affected by */
	STORM_DATA *affected_by_storm;

	/* If storm is close, warn that its coming */
	STORM_DATA *storm_close;

#if 0
	MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
	OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
	ROOM_INDEX_DATA *room_index_hash[MAX_KEY_HASH];
	TOKEN_INDEX_DATA *token_index_hash[MAX_KEY_HASH];
	SCRIPT_DATA *mprog_list;
	SCRIPT_DATA *oprog_list;
	SCRIPT_DATA *rprog_list;
	SCRIPT_DATA *tprog_list;

	long top_mob_index;
	long top_obj_index;
	long top_room;

	long top_mprog_index;
	long top_oprog_index;
	long top_rprog_index;
	long top_tprog_index;
	long top_vnum_mob;
	long top_vnum_npc_ship;
	long top_vnum_obj;
	long top_vnum_room;
	long top_vroom;
#endif

	LLIST *room_list;


	OLC_POINT_BOOST *points;	// Boosted point allotments for the entire area
								// Can be used with "<editor> boostclaim <type> <points>"
								// Can be returned with "<editor> boostrefund <type> <points>"
								//		Can only be returned if resulting point reduction on the bucket doesn't cause the used number to exceed the total.
								//		Only the imp-boost can do this.

    PROG_DATA *		progs;
    pVARIABLE		index_vars;
};

struct storm_data
{
  STORM_DATA *next;

  int x;
  int y;
  int radius;

  /* direction */
  float dx;
  float dy;
  int speed;

  /* How long the storm will hang around */
  int life;
  int counter;

  int storm_type;
};

/* autowars */
struct auto_war
{
    AUTO_WAR *next;

    CHAR_DATA *team_players;

    int war_type;

    int min_players;

    int min;
    int max;
};

#define CONT_SERALIA 	0
#define CONT_ATHEMIA 	1
#define CONT_PIRATE  	2

/* trade */
struct trade_item
{
    TRADE_ITEM 	*next;
    TRADE_ITEM 	*next_trade_produce;

    int  	area;
    int  	trade_type;
    long 	max_price;
    long 	min_price;
    long 	max_qty;
    long 	replenish_amount;
    long 	replenish_time;
    int  	replenish_current_time;
    long 	obj_vnum;
    long 	buy_price;
    long 	sell_price;
    long 	qty;
};

struct trade_type
{
    int 	trade_type;

    char 	*name;
    bool 	live_cargo;
    long 	min_price;
    long 	max_price;
};

/* ships */
#define SHIP_SPEED_STOPPED    	0
#define SHIP_SPEED_HALF_SPEED 	1
#define SHIP_SPEED_FULL_SPEED 	2

#define SHIP_DAMAGE_GRIND     	0
#define SHIP_DAMAGE_FIRE      	1

#define SHIP_ATTACK_STOPPED     0
#define SHIP_ATTACK_LOADING     1
#define SHIP_ATTACK_FIRING      2
#define SHIP_ATTACK_FIRED       3

struct crew_type
{
    int type;
    char *name;
    long price;
};

struct rank_type
{
    int type;
    char *name;
    long qp;
};

struct rep_type
{
    int type;
    char *name;
    long points;
};

#define SHIP_SAILING_BOAT   	0
#define SHIP_CARGO_SHIP         1
#define SHIP_ADVENTURER_SHIP    2
#define SHIP_GALLEON_SHIP       3
#define SHIP_FRIGATE_SHIP       4
#define SHIP_WAR_GALLEON_SHIP   5
#define SHIP_AIR_SHIP		6

#define NPC_SHIP_RATING_UNKNOWN        0
#define NPC_SHIP_RATING_RECOGNIZED     1
#define NPC_SHIP_RATING_WELLKNOWN      2
#define NPC_SHIP_RATING_FAMOUS         3
#define NPC_SHIP_RATING_NOTORIOUS      4
#define NPC_SHIP_RATING_INFAMOUS       5
#define NPC_SHIP_RATING_WANTED         6

#define NPC_SHIP_RANK_NONE	       0
#define NPC_SHIP_RANK_ENSIGN       1
#define NPC_SHIP_RANK_LIEUTENANT   2
#define NPC_SHIP_RANK_COMMANDER	   3
#define NPC_SHIP_RANK_CAPTAIN	     4
#define NPC_SHIP_RANK_COMMODORE	   5
#define NPC_SHIP_RANK_VICE_ADMIRAL 6
#define NPC_SHIP_RANK_ADMIRAL 	   7
#define NPC_SHIP_RANK_PIRATE	     8

#define NPC_SHIP_COAST_GUARD 	       0
#define NPC_SHIP_PIRATE                1
#define NPC_SHIP_BOUNTY_HUNTER         2
#define NPC_SHIP_ADVENTURER            3
#define NPC_SHIP_TRADER                4
#define NPC_SHIP_AIR_SHIP              5

#define NPC_SHIP_SUB_TYPE_NONE 0
#define NPC_SHIP_SUB_TYPE_COAST_GUARD_ATHEMIA 1
#define NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA 2
#define NPC_SHIP_SUB_TYPE_LIGHT_TRADER        3
#define NPC_SHIP_SUB_TYPE_MEDIUM_TRADER       4
#define NPC_SHIP_SUB_TYPE_TREASURE_BOAT       5

#define NPC_SHIP_STATE_STOPPED         0
#define NPC_SHIP_STATE_SAILING         1
#define NPC_SHIP_STATE_ATTACKING       2
#define NPC_SHIP_STATE_BOARDING        3
#define NPC_SHIP_STATE_FLEEING         4
#define NPC_SHIP_STATE_CHASING         5

/* Reports */
#define REPORT_TOP_PLAYER_KILLERS      0
#define REPORT_TOP_CPLAYER_KILLERS     1
#define REPORT_TOP_WEALTHIEST          2
#define REPORT_TOP_WORST_RATIO         3
#define REPORT_TOP_MONSTER_KILLERS     4
#define REPORT_TOP_QUESTS              5
#define REPORT_TOP_BEST_RATIO          6

/* ship waypoints */
struct waypoint_data
{
    WAYPOINT_DATA *next;

    /* destination coords */
    int x;
    int y;

    /* time to emabark times */
    sh_int hour;
    sh_int day;
    sh_int month;
};

struct ship_crew_data
{
    SHIP_CREW_DATA *next;
    long vnum;
};

struct npc_ship_hotspot_type
{
    int x;
    int y;
    int type;
};

struct npc_ship_index_data
{
    NPC_SHIP_INDEX_DATA *	 next;
    SHIP_CREW_DATA *crew;
    MOB_INDEX_DATA *captain;
		OBJ_DATA *cargo;

    long vnum;

    char *name;
    char *		 flag;
    long		 gold;

    int                  ship_type;
    int                  npc_type;
    int			 npc_sub_type;

    int			 original_x;
    int		         original_y;
    char * 		 area;

    WAYPOINT_DATA *	 waypoint_list;

    int			 ships_destroyed;
    long		 plunder_captured;
    char *		 current_name;
    int chance_repop;
    int      initial_ships_destroyed;
};

struct stat_data
{
    char 	*report_name;
    char 	*description;
    int 	columns;
    char	 *column[2];
    char 	*name[10];
    char 	*value[10];
};

struct npc_ship_data
{
    NPC_SHIP_DATA *	 next;
    SHIP_DATA *          ship;
    NPC_SHIP_INDEX_DATA *pShipData;

    int                  from_hotspot;
    CHAR_DATA *		 trigger_char;
    CHAR_DATA *		 captain;
    int			 state;
};

struct ship_type
{
    sh_int               ship_type;

    long                 rooms[MAX_SHIP_ROOMS];
};

struct ship_data
{
    SHIP_DATA *  	 next;
    OBJ_DATA *  	 ship;
    NPC_SHIP_DATA *	 npc_ship;

    CHAR_DATA *  	 owner;

    CHAR_DATA *		 crew_list;
    char      *      	 owner_name;
    char      *      	 flag;
    long		 first_room;

    sh_int 		 dir;
    sh_int		 speed;
    long		 hit;

    sh_int		 ship_type;
    ROOM_INDEX_DATA *    ship_rooms[MAX_SHIP_ROOMS];
    ROOM_INDEX_DATA *    last_room[3];
    sh_int		 num_rooms;

    int			 cannons;
    char *		 ship_name;

    sh_int		 max_crew;
    sh_int       min_crew;

    sh_int 		 attack_position;
    SHIP_DATA *  	 ship_attacked;
    CHAR_DATA *		 char_attacked;

    SHIP_DATA *		 ship_chased;
    ROOM_INDEX_DATA *	 destination;
    SHIP_DATA *		 boarded_by;

    WAYPOINT_DATA *	 waypoint_list;
    WAYPOINT_DATA *	 current_waypoint;

    /* When scuttled show different steps of scuttling */
    sh_int               scuttle_time;
    OBJ_DATA *	 	cannons_obj;

    bool pk;
};

/*
 * Drunk struct
 */
struct struckdrunk
{
    int     min_drunk_level;
    int     number_of_rep;
    char    *replacement[11];
};

/*
 * Room type.
 */
struct	room_index_data
{
	char __type;
    ROOM_INDEX_DATA *	next;
    ROOM_INDEX_DATA *	next_persist;
    ROOM_INDEX_DATA *	next_clone;	/* next clone in the chain for its environment */
    ROOM_INDEX_DATA *	clone_rooms;
    ROOM_INDEX_DATA *	clones;		/* Clones of THIS room */
    PROG_DATA *		progs;
    CHAR_DATA *		people;
    OBJ_DATA *		contents;
    TOKEN_DATA *	tokens;
    CHAT_ROOM_DATA *	chat_room;
    EXTRA_DESCR_DATA *	extra_descr;
    CONDITIONAL_DESCR_DATA *conditional_descr;
    AREA_DATA *		area;
    ROOM_INDEX_DATA *	source;
    INSTANCE_SECTION		*instance_section;
    bool		persist;
    int version;

/* VIZZWILDS */
    WILDS_DATA *        wilds;
    EXIT_DATA *	        exit[10];
    RESET_DATA *	reset_first;
    RESET_DATA *	reset_last;
    EVENT_DATA *	events;
	EVENT_DATA		*events_tail;
    char *		name;
    char *		description;
    char *		owner;
    char *      comments;
    long		vnum;
    long		room_flags;
    long		room2_flags;
    int			light;
    int			sector_type;
    int			heal_rate;
    int 		mana_rate;
    int			move_rate;

    WILDS_TERRAIN *	parent_template;
    WILDS_DATA *        viewwilds;
    long		w;
    long 		x;
    long 		y;
    long 		z;

    char 		*home_owner;
    SHIP_DATA 		*ship;
    int			queued;
    pVARIABLE		index_vars;

    unsigned long	visited;
    unsigned long	clone_id;

    long		locale;	/* Used to group adjacent rooms to the same locale */
    unsigned long	id[2];	/* Used for cloned rooms only.  If the room is not virtual, aka static, this will be { 0,0 } */

	LLIST *		lentity;
	LLIST *		lpeople;
	LLIST *		lcontents;
	LLIST *		levents;
	LLIST *		ltokens;
	LLIST *		lclonerooms;

	int environ_type;
	union {
		ROOM_INDEX_DATA *room;
		CHAR_DATA *mob;
		OBJ_DATA *obj;
		TOKEN_DATA *token;
		struct {
			ROOM_INDEX_DATA *source;
			unsigned long id[2];		// Used for mobs, objs and other clone rooms for delayed loading/dynamic links
		} clone;
	} environ;
	bool force_destruct;

	LOCATION recall;

    int			tempstore[MAX_TEMPSTORE];		/* Temporary storage values for script processing */
};



#define BSFLAG_NO_ROTATE	(A)		// Blueprint Section cannot be rotated

#define BSTYPE_STATIC		1		// Only allowed section type in static blueprints




struct blueprint_link_data {
	BLUEPRINT_LINK *next;
	bool valid;

	char *name;					// Display name of section link

	long vnum;					// Vnum of ROOM
	int door;					// Exit in ROOM

	ROOM_INDEX_DATA *room;		// Resolved room
	EXIT_DATA *ex;				// Resolved exit

	bool used;					// Used in generating layouts, indicating whether the link has already been used
};


struct blueprint_section_data {
	BLUEPRINT_SECTION *next;
	bool valid;

	long vnum;

	char *name;
	char *description;
	char *comments;

	int type;
	int flags;

	long recall;		// The recall of the blueprint, must be within range
	long lower_vnum;
	long upper_vnum;

	BLUEPRINT_LINK *links;
};


struct static_blueprint_link {
	STATIC_BLUEPRINT_LINK *next;
	bool valid;

	BLUEPRINT *blueprint;

	int section1;	// Index of the section in the list of sections
	int link1;		// Index of the link in the section

	int section2;
	int link2;
};

#define BLUEPRINT_MODE_STATIC		0	// Fixed layout, Fixed sections
#define BLUEPRINT_MODE_DYNAMIC		1	// Fixed layout, Variable sections
#define BLUEPRINT_MODE_PROCEDURAL	2	// Variable layout, Variable sections

typedef struct blueprint_special_room_data BLUEPRINT_SPECIAL_ROOM;
struct blueprint_special_room_data {
	BLUEPRINT_SPECIAL_ROOM *next;
	bool valid;

	char *name;

	int section;
	long vnum;					// Vnum of ROOM
};


struct blueprint_data {
	BLUEPRINT *next;
	bool valid;

	long vnum;

	char *name;
	char *description;
	char *comments;

	int area_who;

	int repop;

	int flags;

	int mode;

	LLIST *sections;						// BLUEPRINT_SECTION
	LLIST *special_rooms;

	// BLUEPRINT_MODE_STATIC
	STATIC_BLUEPRINT_LINK *static_layout;
	int static_recall;						// Which section has the recall?

	int static_entry_section;
	int static_entry_link;
	int static_exit_section;
	int static_exit_link;

    LLIST **        progs;
    pVARIABLE		index_vars;
};

#define INSTANCE_NO_SAVE			(A)
#define INSTANCE_COMPLETED			(B)
#define INSTANCE_IDLE_ON_COMPLETE	(C)
#define INSTANCE_NO_IDLE			(D)
#define INSTANCE_DESTROY			(Z)

#define INSTANCE_DESTROY_TIMEOUT	5
#define INSTANCE_IDLE_TIMEOUT		15		// Minutes before a dungeon is purged from not having players in it.


struct instance_section_data {
	INSTANCE_SECTION *next;
	bool valid;

	BLUEPRINT_SECTION *section;
	BLUEPRINT *blueprint;
	INSTANCE *instance;

	LLIST *rooms;
};

typedef struct named_special_room_data NAMED_SPECIAL_ROOM;
struct named_special_room_data {
	NAMED_SPECIAL_ROOM *next;
	bool valid;

	char *name;
	ROOM_INDEX_DATA *room;
};

struct instance_data {
	INSTANCE *next;
	bool valid;

	BLUEPRINT *blueprint;		// Source blueprint
	LLIST *sections;			// Sections created

	int floor;					// Floor identifier, used in traversing multi-level dungeons
								// Defaults to 0 when not used

	int flags;

	ROOM_INDEX_DATA *recall;	// Location in the instance that is considered the recall point
								//   Leave NULL to have no recall
								// Instance rooms will override normal recall checks

	ROOM_INDEX_DATA *entrance;	// Entry room to the instance as defined by the blueprint

	ROOM_INDEX_DATA *exit;		// Exit room from the instance as defined by the blueprint

	DUNGEON *dungeon;			// Dungeon owner of the instance

	OBJ_DATA *object;				// Object owner of the instance
	unsigned long object_uid[2];	//   If the object owner is extracted, all players inside will be
									//   dropped to the room of the object.

	LLIST *player_owners;

	ROOM_INDEX_DATA *environ;	// Explicit environment for instance
								//   If NULL, will use the current room of the object
								//   Must be a static room

	LLIST *players;
	LLIST *mobiles;					// List of mobiles (include players) not part of the instance
	LLIST *objects;
	LLIST *bosses;					// List of INSTANCE MOB BOSSES

	LLIST *rooms;
	LLIST *special_rooms;

	int age;
	int idle_timer;
	bool empty;

    PROG_DATA *		progs;		// Script data
};

#define DUNGEON_NO_SAVE				(A)		// Dungeon will not save to disk.
#define DUNGEON_COMPLETED			(B)		// Dungeon has completed
#define DUNGEON_IDLE_ON_COMPLETE	(C)		// Only update the idle_timer when DUNGEON_COMPLETED has been set
#define DUNGEON_NO_IDLE				(D)		//
#define DUNGEON_DESTROY				(Z)		// Flagged for destruction.
											//   Will handle the idle timer regardless if dungeon is empty
#define DUNGEON_DESTROY_TIMEOUT	5
#define DUNGEON_IDLE_TIMEOUT	15		// Minutes before a dungeon is purged from not having players in it.


struct dungeon_index_special_room_data
{
	DUNGEON_INDEX_SPECIAL_ROOM *next;
	bool valid;

	char *name;

	int floor;
	int section;
	long vnum;
};

struct dungeon_index_data
{
	DUNGEON_INDEX_DATA *next;
	bool valid;

	long vnum;

	char *name;
	char *description;
	char *comments;

	int area_who;

	LLIST *floors;
	LLIST *special_rooms;
	long entry_room;
	long exit_room;

	int repop;

	int flags;						// Potential flags

	char *zone_out;
	char *zone_out_portal;			// Zoneout if there is a dungeon portal defined
	char *zone_out_mount;			// Zoneout when riding a mount

    LLIST **        progs;
    pVARIABLE		index_vars;
};

struct dungeon_data
{
	DUNGEON *next;
	bool valid;
	bool empty;

	DUNGEON_INDEX_DATA *index;		// Dungeon definition

	long uid[2];					// UID of the dungeon instance

	LLIST *floors;
	LLIST *special_rooms;
	ROOM_INDEX_DATA *entry_room;
	ROOM_INDEX_DATA *exit_room;

	int flags;						// Potential instanced flags

	LLIST *player_owners;			// Player owners of the dungeon
									//  Must be a SUPERSET of the players list inside the dungeon

	LLIST *players;					// List of players inside the dungeon
	LLIST *mobiles;					// List of mobiles (include players) inside the dungeon not part of the instance
	LLIST *objects;					// List of objects not part of the dungeon
	LLIST *bosses;					// List of INSTANCE MOB BOSSES

	LLIST *rooms;

	int age;

	int idle_timer;					// Timer before it is purged automatically
									// If normally zero, the dungeon will never purge without
									//   explicitly being forced by the code

    PROG_DATA *		progs;
};



/* conditions for conditional descs */
#define CONDITION_NONE 	 	0 /* never used */
#define CONDITION_SEASON 	1 /* winter, spring, summer, fall */
#define CONDITION_SKY    	2 /* cloudless, cloudy, rainy, stormy */
#define CONDITION_HOUR   	3 /* 0-23 */
#define CONDITION_SCRIPT	4

/* for the tunneler in plith */
struct  tunneler_place_type
{
    char *name;
    int price;
    long vnum;
};

/* time */
#define HOURS_PER_DAY   	24
#define DAYS_PER_WEEK   	7
#define DAYS_PER_MONTH  	30
#define MONTHS_PER_YEAR		12

#define DAYS_PER_YEAR   	(DAYS_PER_MONTH * MONTHS_PER_YEAR)
#define HOUR_SUNRISE		(HOURS_PER_DAY / 4)
#define HOUR_DAY_BEGIN		(HOUR_SUNRISE + 1)
#define HOUR_NOON           	(HOURS_PER_DAY / 2)
#define HOUR_SUNSET		((HOURS_PER_DAY / 4) * 3)
#define HOUR_NIGHT_BEGIN	(HOUR_SUNSET + 1)
#define HOUR_MIDNIGHT		HOURS_PER_DAY

#define SEASON_WINTER		0
#define SEASON_SPRING		1
#define SEASON_SUMMER		2
#define SEASON_FALL		3
#define SEASON_MAX         	4

struct	weather_data
{
    int 	sunlight;
    int		sky;
    int 	mmhg;
    int		change;
};

/* global weather table */
extern		AUTO_WAR *	        auto_war;
extern		NPC_SHIP_DATA *		plith_airship;
extern		WEATHER_DATA		weather_info;
extern		bool			merc_down;
extern		char 			*reboot_by;
extern		time_t			reboot_timer;
extern 		int			auto_war_battle_timer;
extern 		int			auto_war_timer;
extern 		int			down_timer;
extern          AUCTION_DATA            auction_info;
extern          GQ_DATA			global_quest;
extern		bool			objRepop;
extern 		EVENT_DATA 		*events;
extern		EVENT_DATA		*events_tail;
extern		int			call_depth;

/* Timer that reckoning is active */
extern          time_t			reckoning_timer;
extern          time_t			reckoning_cooldown_timer;

/* Which hour it is before the reckoning hits */
extern          int			pre_reckoning;

/* 20070606 : NIB : Chance the reckoning will occur on a full moon at night... */
extern          int			reckoning_chance;
extern          int			reckoning_duration;
extern          int			reckoning_intensity;
extern          int			reckoning_cooldown;

/*
 * Types of attacks.
 * Must be non-overlapping with spell/skill types,
 * but may be arbitrary beyond that.
 */
#define TYPE_UNDEFINED               -1
#define TYPE_HIT                     1000

/*
 * Target types.
 */
#define TAR_IGNORE		    0 /* no target, or a string ie better/worse */
#define TAR_CHAR_OFFENSIVE	    1 /* character - offensive */
#define TAR_CHAR_DEFENSIVE	    2 /* character - defense, can cast on any1 */
#define TAR_CHAR_SELF		    3 /* self ONLY */
#define TAR_OBJ_INV		    4 /* obj in the inv ONLY */
#define TAR_OBJ_CHAR_DEF	    5 /* obj OR offensive char */
#define TAR_OBJ_CHAR_OFF	    6 /* obj OR any char */
#define TAR_IGNORE_CHAR_DEF	    7 /* any char in the world */
#define TAR_OBJ_GROUND		    8 /* obj on the ground */
#define TAR_CHAR_FORMATION	    9 /* char's formation only */

#define TARGET_CHAR		    0
#define TARGET_OBJ		    1
#define TARGET_ROOM		    2
#define TARGET_NONE		    3


/* Church command type */
struct church_command_type
{
    char 	*command;		/* the command */
    int 	rank;			/* minimum rank to use */
    DO_FUN	*function;		/* function call */
};


/*
 * Skills include spells as a particular case.
 */
struct	skill_type
{
    char *	name;			/* Name of skill		*/
    sh_int	skill_level[MAX_CLASS];	/* Level needed by class	*/
    sh_int	rating[MAX_CLASS];	/* How hard it is to learn	*/
    SPELL_FUN *	spell_fun;		/* Spell pointer (for spells)	*/
    sh_int	target;			/* Legal targets		*/
    sh_int	minimum_position;	/* Position for caster / user	*/
    sh_int *	pgsn;			/* Pointer to associated gsn	*/
    /* sh_int	slot;		 	Syn- reusing this as a racial skill toggle. */
    int 	race;			/* If it's a racial skill ONLY, this is the race number. If not, its -1.
					   This doesn't apply for skills that can be gotten from classes, like archery. */

    sh_int	min_mana;		/* Minimum mana used		*/
    sh_int	beats;			/* Waiting time after use	*/
    char *	noun_damage;		/* Damage message		*/
    char *	msg_off;		/* Wear off message		*/
    char *	msg_obj;		/* Wear off message for obects	*/
    char *	msg_disp;
    int		inks[3][2];
};

struct mob_index_skill_data {
	MOB_INDEX_SKILL_DATA *next;
	sh_int sn;		/* Which skill */
	sh_int ratings[6];	/* multi-point %rating mapping, -1 terminated */
	sh_int chance;		/* Chance of being added at all */
};

struct mob_skill_data {
	MOB_SKILL_DATA *next;
	sh_int sn;		/* Which skill */
	sh_int rating;
};


struct material_type
{
    char *name;		/* Name of the material */
    int strength;	/* How strong it is, 1 is weakest, 10 is strongest. */
    int value;      	/* Value rating, 1-lowest, 10-highest(rarest) */
};

struct music_type
{
    char *	name;			/* Name of skill		*/
    int         level;
    char *	spell1;		        /* Spell pointer (for spells)	*/
    char *	spell2;	        	/* Spell pointer (for spells)	*/
    char *	spell3;   		/* Spell pointer (for spells)	*/
    sh_int	beats;			/* Waiting time after use	*/
    sh_int      mana;
    sh_int	target;			/* Legal targets		*/
};

struct  group_type
{
    char *	name;
    sh_int	rating[MAX_CLASS];
    char *	spells[MAX_IN_GROUP];
};

#define RPROG_VNUM_PLAYER_INIT 1	// Called when a player/immortal logs in, to give them various tokens and whatnot that are needed from the start

/*
 * Program triggers
 */
enum trigger_index_enum {
	TRIG_NONE = -1,
	TRIG_ACT,
	TRIG_AFTERDEATH,	/* Fired just after you die */
	TRIG_AFTERKILL,		/* Called after someome kills a target.  TODO: Damage will become forbidden in this trigger. */
	TRIG_ANIMATE,
	TRIG_ASSIST,
	TRIG_ATTACK_BACKSTAB,
	TRIG_ATTACK_BASH,
	TRIG_ATTACK_BEHEAD,
	TRIG_ATTACK_BITE,
	TRIG_ATTACK_BLACKJACK,
	TRIG_ATTACK_CIRCLE,
	TRIG_ATTACK_COUNTER,
	TRIG_ATTACK_CRIPPLE,
	TRIG_ATTACK_DIRTKICK,
	TRIG_ATTACK_DISARM,
	TRIG_ATTACK_INTIMIDATE,
	TRIG_ATTACK_KICK,
	TRIG_ATTACK_REND,
	TRIG_ATTACK_SLIT,
	TRIG_ATTACK_SMITE,
	TRIG_ATTACK_TAILKICK,
	TRIG_ATTACK_TRAMPLE,
	TRIG_ATTACK_TURN,
	TRIG_BARRIER,
	TRIG_BLOW,
	TRIG_BOARD,
	TRIG_BRANDISH,
	TRIG_BRIBE,
	TRIG_BURY,
	TRIG_BUY,
	TRIG_CANINTERRUPT,
	TRIG_CATALYST,
	TRIG_CATALYST_FULL,
	TRIG_CATALYST_SOURCE,
	TRIG_CHECK_BUYER,		// Called when a stock item is not an object, used to check whether the buyer can GET the item
	TRIG_CHECK_DAMAGE,
	TRIG_CLONE_EXTRACT,
	TRIG_CLOSE,
	TRIG_COMBAT_STYLE,
	TRIG_COMPLETED,
	TRIG_CONTRACT_COMPLETE,
	TRIG_CUSTOM_PRICE,		// Called when a stock item has custom pricing
	TRIG_DAMAGE,
	TRIG_DEATH,
	TRIG_DEATH_PROTECTION,
	TRIG_DEATH_TIMER,
	TRIG_DEFENSE,
	TRIG_DELAY,
	TRIG_DRINK,
	TRIG_DROP,
	TRIG_EAT,
	TRIG_EMOTE,			// NIB : 20140508 : untargeted emote
	TRIG_EMOTEAT,		// NIB : 20140508 : targeted emote
	TRIG_EMOTESELF,		// NIB : 20140508 : self-targeted emote
	TRIG_ENTRY,
	TRIG_EXALL,
	TRIG_EXAMINE,
	TRIG_EXIT,
	TRIG_EXPIRE,		/* NIB : 20070124 : token expiration */
	TRIG_EXTRACT,
	TRIG_FIGHT,
	TRIG_FLEE,
	TRIG_FORCEDISMOUNT,
	TRIG_GET,
	TRIG_GIVE,
	TRIG_GRALL,
	TRIG_GREET,
	TRIG_GROUPED,
	TRIG_GROW,			// NIB 20140513 - trigger for seeds to have custom growth results besides sprouting a plant.
	TRIG_HIDDEN,		// After the mob has hidden (mob and token only)
	TRIG_HIDE,			// Act of hiding (mob, object and token)
	TRIG_HIT,
	TRIG_HPCNT,
	TRIG_IDENTIFY,
	TRIG_INSPECT,		// Called when asking a shopkeeper to inspect a custom stock item
	TRIG_INTERRUPT,
	TRIG_KILL,
	TRIG_KNOCK,
	TRIG_KNOCKING,
	TRIG_LAND,
	TRIG_LEVEL,
	TRIG_LOGIN,
	TRIG_LORE,
	TRIG_LORE_EX,
	TRIG_MOON,
	TRIG_MOUNT,
	TRIG_MOVE_CHAR,
	TRIG_MULTICLASS,	// Called when a player multiclasses
	TRIG_OPEN,
	TRIG_POSTQUEST,			// Called after all quest rewards and messages are given
	TRIG_PRACTICE,
	TRIG_PRACTICETOKEN,
	TRIG_PREANIMATE,
	TRIG_PREASSIST,
	TRIG_PREBITE,
	TRIG_PREBUY,
	TRIG_PREBUY_OBJ,
	TRIG_PRECAST,
	TRIG_PREDEATH,
	TRIG_PREDISMOUNT,
	TRIG_PREDRINK,
	TRIG_PREDROP,
	TRIG_PREEAT,
	TRIG_PREENTER,
	TRIG_PREFLEE,
	TRIG_PREGET,
	TRIG_PREHIDE,			// NIB 20140522 - trigger for testing if you can hide yourself or the object in question
	TRIG_PREHIDE_IN,		// NIB 20140522 - trigger for testing if the container or mob you are trying to hide an object in will allow it
	TRIG_PREKILL,
	TRIG_PREMOUNT,
	TRIG_PREPRACTICE,
	TRIG_PREPRACTICEOTHER,
	TRIG_PREPRACTICETHAT,
	TRIG_PREPRACTICETOKEN,
	TRIG_PREPUT,
	TRIG_PREQUEST,			// Allows custom checking for questing, also allows setting the number of quest parts.
	TRIG_PRERECALL,
	TRIG_PRERECITE,
	TRIG_PRERECKONING,
	TRIG_PREREHEARSE,
	TRIG_PREREMOVE,
	TRIG_PRERENEW,			// Used by "RENEW" to get QP cost and whether the item can be renewed
	TRIG_PREREST,
	TRIG_PRERESURRECT,
	TRIG_PREROUND,
	TRIG_PRESELL,
	TRIG_PRESIT,
	TRIG_PRESLEEP,
	TRIG_PRESPELL,
	TRIG_PRESTAND,
	TRIG_PRETRAIN,
	TRIG_PRETRAINTOKEN,
	TRIG_PREWAKE,
	TRIG_PREWEAR,
	TRIG_PREWIMPY,
	TRIG_PULL,
	TRIG_PULL_ON,		/* NIB : 20070121 */
	TRIG_PUSH,
	TRIG_PUSH_ON,		/* NIB : 20070121 */
	TRIG_PUT,
	TRIG_QUEST_CANCEL,
	TRIG_QUEST_COMPLETE,	// Prior to awards being given, called when the quest turned in complete, allowing editing of the awards
	TRIG_QUEST_INCOMPLETE,	// Prior to awards being given, called when the quest turned in incomplete , allowing editing of the awards
	TRIG_QUEST_PART,		// Used to generate a custom quest part when selected.
	TRIG_QUIT,
	TRIG_RANDOM,
	TRIG_RECALL,
	TRIG_RECITE,
	TRIG_RECKONING,
	TRIG_REGEN,		/*  Custom regenerations */
	TRIG_REGEN_HP,		/* Modification on ALL hp regens */
	TRIG_REGEN_MANA,	/* Modification on ALL mana regens */
	TRIG_REGEN_MOVE,	/* Modification on ALL move regens */
	TRIG_REMORT,		// Called when a player remorts
	TRIG_REMOVE,		/* NIB : 20070120 */
	TRIG_RENEW,			// Used by "RENEW"
	TRIG_RENEW_LIST,	// Used by "RENEW LIST"
	TRIG_REPOP,
	TRIG_RESET,
	TRIG_REST,
	TRIG_RESTOCKED,
	TRIG_RESTORE,
	TRIG_RESURRECT,
	TRIG_SAVE,
	TRIG_SAYTO,		/* NIB : 20070121 */
	TRIG_SIT,
	TRIG_SKILL_BERSERK,
	TRIG_SKILL_SNEAK,
	TRIG_SKILL_WARCRY,
	TRIG_SLEEP,
	TRIG_SPEECH,
	TRIG_SPELL,
	TRIG_SPELLBEAT,
	TRIG_SPELLCAST,
	TRIG_SPELLINTER,
	TRIG_SPELLPENETRATE,
	TRIG_SPELLREFLECT,
	TRIG_SPELL_CURE,
	TRIG_SPELL_DISPEL,
	TRIG_STAND,
	TRIG_START_COMBAT,
	TRIG_STRIPAFFECT,
	TRIG_TAKEOFF,
	TRIG_THROW,
	TRIG_TOKEN_GIVEN,
	TRIG_TOKEN_REMOVED,
	TRIG_TOUCH,
	TRIG_TURN,
	TRIG_TURN_ON,		/* NIB : 20070121 */
	TRIG_UNGROUPED,
	TRIG_USE,
	TRIG_USEWITH,
	TRIG_VERB,
	TRIG_VERBSELF,
	TRIG_WAKE,
	TRIG_WEAPON_BLOCKED,
	TRIG_WEAPON_CAUGHT,
	TRIG_WEAPON_PARRIED,
	TRIG_WEAR,
	TRIG_WHISPER,
	TRIG_WIMPY,
	TRIG_XPGAIN,
	TRIG_ZAP
};

/* NIB : 20070124 : Trigger slot types */
#define TRIGSLOT_GENERAL		0	/* General triggers, that don't have special needs */
#define TRIGSLOT_SPEECH			1	/* speech, whisper, sayto */
#define TRIGSLOT_RANDOM			2	/* random */
#define TRIGSLOT_MOVE			3	/* greet/all, exit/all, entry */
#define TRIGSLOT_ACTION			4	/* act/emote */
#define TRIGSLOT_FIGHT			5	/* fight */
#define TRIGSLOT_REPOP			6	/* repop, death, expire */
#define TRIGSLOT_VERB			7	/* verbs ONLY */
#define TRIGSLOT_ATTACKS		8
#define TRIGSLOT_HITS			9	/* for the TRIG_HITS - ONLY */
#define TRIGSLOT_DAMAGE			10	/* for the TRIG_DAMAGE - ONLY */
#define TRIGSLOT_SPELL			11	/* for the TRIG_SPELL* - ONLY */
#define TRIGSLOT_INTERRUPT		12
#define TRIGSLOT_COMBATSTYLE	13	// for TRIG_COMBAT_STYLE only
#define TRIGSLOT_ANIMATE	14
#define TRIGSLOT_MAX			15

/* program types */
#define PRG_MPROG	0	// Mobiles
#define PRG_OPROG	1	// Objects
#define PRG_RPROG	2	// Rooms
#define PRG_TPROG	3	// Tokens
#define PRG_APROG	4	// Area
#define PRG_IPROG	5	// Instances
#define PRG_DPROG	6	// Dungeons

#define NEWEST_OBJ_VERSION 1

struct trigger_type {
	char *name;		// Cannonical name
	char *alias;	// Aliases for the trigger
	int type;		// Trigger type
	int slot;		// Trigger slot for grouping similar triggers together
	bool mob;
	bool obj;
	bool room;
	bool token;
	bool area;
	bool instance;
	bool dungeon;
};

#define PROG_NODESTRUCT		(A)		/* Used to indicate the item is already destructing and should not fire any destructions */
#define PROG_AT			(B)		/* Used to indicate the entity is doing a remote action via at, bars certain scripted actions */
#define PROG_NODAMAGE	(C)		// The script may not do damage at any stage
#define PROG_NORAWKILL	(D)		// The script may not do rawkill at any stage

#define PROG_FLAG(x,y)		(((x)->progs) && IS_SET((x)->progs->entity_flags,(y)))

struct prog_data
{
    PROG_DATA 		*next;
    LLIST 		**progs;	/* This will be allocated into a BANK of LISTs */

    CHAR_DATA *		target;
    int			delay;
    long		tog_flags;
    int			lastreturn;	/* @@@NIB : 20070123 */
    long		entity_flags;
    int			script_ref;	// Counts the number of scripts referencing this entity in a trigger function
    						//  If > 0 it will prevent the entity from being destroyed, flagging it for later consumption
    bool 		extract_when_done;		// Flag set to extract the parent entity when the script_ref reaches zero
    bool		extract_fPull;
    pVARIABLE		vars;
};

struct prog_list
{
	int		trig_type;
	char *		trig_phrase;
	int			trig_number;	// atoi(trig_phrase)
	bool		numeric;
	long		vnum;
	SCRIPT_DATA *	script;		/* @@@NIB : 20070123  */
	PROG_LIST *	next;
	bool		valid;
};

struct prog_code
{
    long                vnum;
    char *              code;
    PROG_CODE *         next;
};

struct  chat_room_data
{
    CHAT_ROOM_DATA *next;
    CHAT_OP_DATA *ops;	/* ops of the room */
    CHAT_BAN_DATA *bans;	/* people banned from the room */
    char *name;		/* name */
    char *topic;	/* topic */
    char *password; 	/* password protection, "none" is none */
    char *created_by;	/* person who created it */
    int max_people;	/* limit of people allowed */
    int curr_people;	/* current amt. of people */
    bool permanent;	/* does it save after reboots, etc? */
    long vnum;		/* room vnum */
};

struct  chat_op_data
{
    CHAT_OP_DATA *next;
    CHAT_ROOM_DATA *chat_room;
    char *name;		/* operator's name */
};

struct  chat_ban_data
{
    CHAT_BAN_DATA *next;
    CHAT_ROOM_DATA *chat_room;
    char *name;	/* banned persons name */
    char *banned_by;/* who banned the person */
};

struct  ignore_data
{
    IGNORE_DATA *next;
    char *name;
    char *reason;
};

struct  string_vector_data
{
    STRING_VECTOR *next;
    char *key;
    char *string;
};

struct  string_data
{
    STRING_DATA *next;
    char *string;
};

struct  ambush_data
{
    AMBUSH_DATA *next;

    int type;
    int min_level;
    int max_level;
    char *command;
};

struct gq_mob_data
{
    GQ_MOB_DATA *next;

    long vnum;
    int class; /* 1, 2, 3, 4 */
    bool group; /* group mob? */
    long obj; /* what obj vnum to repop with? */
    int count; /* how many in the game? */
    int max; /* max to repop */
};

struct gq_obj_data
{
    GQ_OBJ_DATA *next;

    long vnum;

    int qp_reward;
    int prac_reward;
    long exp_reward;
    int silver_reward;
    int gold_reward;
    int repop;
    int max;
    int count;
};


#define AMBUSH_ALL 	0
#define AMBUSH_PC  	1
#define AMBUSH_NPC 	2

/* Immortal duty bits.
   The first block are the "boss" titles. Not to be confused with supervisors, since
   a plain old "builder" can still lead a project without having to be head builder.
   */
#define IMMORTAL_HEADCODER		(A)
#define IMMORTAL_HEADBUILDER		(B)
#define IMMORTAL_HEADADMIN		(C)
#define IMMORTAL_HEADBALANCE		(D)
#define IMMORTAL_HEADPR			(E)

/* Staff is an odd duck because it only takes one person */
#define IMMORTAL_STAFF			(F)

/* This block defines the subordinate roles, presided over by the above. */
#define IMMORTAL_CODER			(G)
#define IMMORTAL_BUILDER		(H)
#define IMMORTAL_HOUSING		(I)
#define IMMORTAL_CHURCHES		(J)
#define IMMORTAL_HELPFILES		(K)
#define IMMORTAL_WEDDINGS		(L)
#define IMMORTAL_TESTING		(M)
#define IMMORTAL_EVENTS			(N)
#define IMMORTAL_WEBSITE		(O)
#define IMMORTAL_STORY			(P)
#define IMMORTAL_SECRETARY		(Q)



/* Immortal staff management. */
struct immortal_data
{
    IMMORTAL_DATA	*next;
    PC_DATA 		*pc;   		/* Link back to pc to determine age */

    time_t		created;        /* Time immhood assigned for seniority */

    char 		*leader;        /* Who is dominating who? */
    char 		*name;
    long		duties;		/* Bit-field for assigning duties */
    PROJECT_DATA *      build_project; 	/* Area currently building in, NULL for none */
    PROJECT_BUILDER_DATA *builder;	/* Direct pointer to project builder for performance */
    char *		imm_flag; 	/* personal imm flags for fun */
    time_t		last_olc_command; /* Time last OLC command issued */
    char *		bamfin;		/* poofin */
    char *		bamfout;	/* poofout */
};

/* Project management system. */

/* Flags for projects, mainly used in do_plist. */
#define PROJECT_OPEN			(A)
#define PROJECT_ASSIGNED		(B)
#define PROJECT_HOLD			(C)

/* Types of access a person may have to a project. Can maybe even be expanded to areas. */
#define ACCESS_NONE			0
#define ACCESS_VIEW			1	/* Can view the project. */
#define ACCESS_BUILD			2	/* Can build in the project. */
#define ACCSES_EDIT			3	/* Can edit the project using PEDIT. */

struct project_data
{
    PROJECT_DATA 	*next;
    STRING_DATA         *areas; 		/* Areas associated with project */
    PROJECT_BUILDER_DATA * builders;		/* List of builders with logged hrs/cmds */
    bool 		valid;
    char		*name;			/* Name of the project */
    char 		*leader; 		/* Head of the project */
    char		*summary; 		/* A brief (1 sentence) description */
    char		*description;		/* A more in-depth description, edited with string editor */
    int			security; 		/* Minimum security to view project */
    int 		completed;		/* % completed */
    long		project_flags;		/* Project flags */
    PROJECT_INQUIRY_DATA *inquiries; 		/* List of player inquiries to be answered */
    time_t		created;		/* Date project was created */
};


/* Builder under a project. */
struct project_builder_data
{
    PROJECT_DATA	*project;
    PROJECT_BUILDER_DATA *next;

    char		*name;			/* Builder name */
    STRING_DATA		*commands;		/* Last 20 OLC commands */
    long		minutes; 		/* total # minutes working on this project */
    time_t		assigned; 		/* date assigned to the project */
};


/* Inquiry (essentially a note) within a project */
struct project_inquiry_data
{
    PROJECT_DATA		*project;	/* Project this inquiry belongs to */
    PROJECT_INQUIRY_DATA	*next;		/* Next in project list */
    PROJECT_INQUIRY_DATA	*next_global;	/* Next in list of ALL inquiries for ALL projects */
    PROJECT_INQUIRY_DATA        *replies;	/* Replies to this inquiry */
    PROJECT_INQUIRY_DATA	*parent;	/* Parent post for replies */
    LOG_ENTRY_DATA		*log;		/* Log entries. */
    int				num_log_entries; /* Number of log entries (for performance) */
    char			*sender;
    char			*subject;
    char			*text;
    time_t			date;
    time_t			closed;
    char 			*closed_by;
};

/* Very simple log entry. Used in a linked list which is saved to a file every time
/   MAX_LOG_ENTRIES is collected */

#define MAX_LOG_ENTRIES	        1000

struct log_entry_data
{
    LOG_ENTRY_DATA *next;

    time_t			date;		/* Time the logged event happened */
    char			*text;		/* The log message */
};


/*
 * These are skill_lookup return values for common skills and spells.
 */
extern sh_int	gsn_acid_blast;
extern sh_int	gsn_acid_breath;
extern sh_int	gsn_acro;
extern sh_int	gsn_afterburn;
extern sh_int	gsn_air_spells;
extern sh_int	gsn_ambush;
extern sh_int	gsn_animate_dead;
extern sh_int	gsn_archery;
extern sh_int	gsn_armour;
extern sh_int	gsn_athletics;
extern sh_int	gsn_avatar_shield;
extern sh_int	gsn_axe;
extern sh_int	gsn_backstab;
extern sh_int	gsn_bar;
extern sh_int	gsn_bash;
extern sh_int	gsn_behead;
extern sh_int	gsn_berserk;
extern sh_int	gsn_bind;
extern sh_int	gsn_bite;
extern sh_int	gsn_blackjack;
extern sh_int	gsn_bless;
extern sh_int	gsn_blindness;
extern sh_int	gsn_blowgun;
extern sh_int	gsn_bomb;
extern sh_int	gsn_bow;
extern sh_int	gsn_breath;
extern sh_int	gsn_brew;
extern sh_int	gsn_burgle;
extern sh_int	gsn_burning_hands;
extern sh_int	gsn_call_familiar;
extern sh_int	gsn_call_lightning;
extern sh_int	gsn_calm;
extern sh_int	gsn_cancellation;
extern sh_int	gsn_catch;
extern sh_int	gsn_cause_critical;
extern sh_int	gsn_cause_light;
extern sh_int	gsn_cause_serious;
extern sh_int	gsn_chain_lightning;
extern sh_int	gsn_channel;
extern sh_int	gsn_charge;
extern sh_int	gsn_charm_person;
extern sh_int	gsn_chill_touch;
extern sh_int	gsn_circle;
extern sh_int	gsn_cloak_of_guile;
extern sh_int	gsn_colour_spray;
extern sh_int	gsn_combine;
extern sh_int	gsn_consume;
extern sh_int	gsn_continual_light;
extern sh_int	gsn_control_weather;
extern sh_int	gsn_cosmic_blast;
extern sh_int	gsn_counterspell;
extern sh_int	gsn_create_food;
extern sh_int	gsn_create_rose;
extern sh_int	gsn_create_spring;
extern sh_int	gsn_create_water;
extern sh_int	gsn_crippling_touch;
extern sh_int	gsn_crossbow;
extern sh_int	gsn_cure_blindness;
extern sh_int	gsn_cure_critical;
extern sh_int	gsn_cure_disease;
extern sh_int	gsn_cure_light;
extern sh_int	gsn_cure_poison;
extern sh_int	gsn_cure_serious;
extern sh_int	gsn_cure_toxic;
extern sh_int	gsn_curse;
extern sh_int	gsn_dagger;
extern sh_int	gsn_death_grip;
extern sh_int	gsn_deathbarbs;
extern sh_int	gsn_deathsight;
extern sh_int	gsn_deception;
extern sh_int	gsn_deep_trance;
extern sh_int	gsn_demonfire;
extern sh_int	gsn_destruction;
extern sh_int	gsn_detect_hidden;
extern sh_int	gsn_detect_invis;
extern sh_int	gsn_detect_magic;
extern sh_int	gsn_detect_traps;
extern sh_int	gsn_dirt;
extern sh_int	gsn_dirt_kicking;
extern sh_int	gsn_disarm;
extern sh_int	gsn_discharge;
extern sh_int	gsn_dispel_evil;
extern sh_int	gsn_dispel_good;
extern sh_int	gsn_dispel_magic;
extern sh_int	gsn_dispel_room;
extern sh_int	gsn_dodge;
extern sh_int	gsn_dual;
extern sh_int	gsn_eagle_eye;
extern sh_int	gsn_earth_spells;
extern sh_int	gsn_earthquake;
extern sh_int	gsn_electrical_barrier;
extern sh_int	gsn_enchant_armour;
extern sh_int	gsn_enchant_weapon;
extern sh_int	gsn_energy_drain;
extern sh_int	gsn_energy_field;
extern sh_int	gsn_enhanced_damage;
extern sh_int	gsn_ensnare;
extern sh_int	gsn_entrap;
extern sh_int	gsn_envenom;
extern sh_int	gsn_evasion;
extern sh_int	gsn_exorcism;
extern sh_int	gsn_exotic;
extern sh_int	gsn_fade;
extern sh_int	gsn_faerie_fire;
extern sh_int	gsn_faerie_fog;
extern sh_int	gsn_fast_healing;
extern sh_int	gsn_fatigue;
extern sh_int	gsn_feign;
extern sh_int	gsn_fire_barrier;
extern sh_int	gsn_fire_breath;
extern sh_int	gsn_fire_cloud;
extern sh_int	gsn_fire_spells;
extern sh_int	gsn_fireball;
extern sh_int	gsn_fireproof;
extern sh_int	gsn_flail;
extern sh_int	gsn_flamestrike;
extern sh_int	gsn_flight;
extern sh_int	gsn_fly;
extern sh_int	gsn_fourth_attack;
extern sh_int	gsn_frenzy;
extern sh_int	gsn_frost_barrier;
extern sh_int	gsn_frost_breath;
extern sh_int	gsn_gas_breath;
extern sh_int	gsn_gate;
extern sh_int	gsn_giant_strength;
extern sh_int	gsn_glorious_bolt;
extern sh_int	gsn_haggle;
extern sh_int	gsn_hand_to_hand;
extern sh_int	gsn_harm;
extern sh_int	gsn_harpooning;
extern sh_int	gsn_haste;
extern sh_int	gsn_heal;
extern sh_int	gsn_healing_aura;
extern sh_int	gsn_healing_hands;
extern sh_int	gsn_hide;
extern sh_int	gsn_holdup;
extern sh_int	gsn_holy_shield;
extern sh_int	gsn_holy_sword;
extern sh_int	gsn_holy_word;
extern sh_int	gsn_holy_wrath;
extern sh_int	gsn_hunt;
extern sh_int	gsn_ice_storm;
extern sh_int	gsn_identify;
extern sh_int	gsn_improved_invisibility;
extern sh_int	gsn_inferno;
extern sh_int	gsn_infravision;
extern sh_int	gsn_infuse;
extern sh_int	gsn_intimidate;
extern sh_int	gsn_invis;
extern sh_int	gsn_judge;
extern sh_int	gsn_kick;
extern sh_int	gsn_kill;
extern sh_int	gsn_leadership;
extern sh_int	gsn_light_shroud;
extern sh_int	gsn_lightning_bolt;
extern sh_int	gsn_lightning_breath;
extern sh_int	gsn_locate_object;
extern sh_int	gsn_lore;
extern sh_int	gsn_mace;
extern sh_int	gsn_magic_missile;
extern sh_int	gsn_martial_arts;
extern sh_int	gsn_mass_healing;
extern sh_int	gsn_mass_invis;
extern sh_int	gsn_master_weather;
extern sh_int	gsn_maze;
extern sh_int	gsn_meditation;
extern sh_int	gsn_mob_lore;
extern sh_int	gsn_momentary_darkness;
extern sh_int	gsn_morphlock;
extern sh_int	gsn_mount_and_weapon_style;
extern sh_int	gsn_music;
extern sh_int	gsn_neurotoxin;
extern sh_int	gsn_nexus;
extern sh_int	gsn_parry;
extern sh_int	gsn_pass_door;
extern sh_int	gsn_peek;
extern sh_int	gsn_pick_lock;
extern sh_int	gsn_plague;
extern sh_int	gsn_poison;
extern sh_int	gsn_polearm;
extern sh_int	gsn_possess;
extern sh_int	gsn_pursuit;
extern sh_int	gsn_quarterstaff;
extern sh_int	gsn_raise_dead;
extern sh_int	gsn_recall;
extern sh_int	gsn_recharge;
extern sh_int	gsn_refresh;
extern sh_int	gsn_regeneration;
extern sh_int	gsn_remove_curse;
extern sh_int	gsn_rending;
extern sh_int	gsn_repair;
extern sh_int	gsn_rescue;
extern sh_int	gsn_resurrect;
extern sh_int	gsn_reverie;
extern sh_int	gsn_riding;
extern sh_int	gsn_room_shield;
extern sh_int	gsn_sanctuary;
extern sh_int	gsn_scan;
extern sh_int	gsn_scribe;
extern sh_int	gsn_scrolls;
extern sh_int	gsn_scry;
extern sh_int	gsn_second_attack;
extern sh_int	gsn_sense_danger;
extern sh_int	gsn_shape;
extern sh_int	gsn_shield;
extern sh_int	gsn_shield_block;
extern sh_int	gsn_shield_weapon_style;
extern sh_int	gsn_shift;
extern sh_int	gsn_shocking_grasp;
extern sh_int	gsn_silence;
extern sh_int	gsn_single_style;
extern sh_int	gsn_skull;
extern sh_int	gsn_sleep;
extern sh_int	gsn_slit_throat;
extern sh_int	gsn_slow;
extern sh_int	gsn_smite;
extern sh_int	gsn_sneak;
extern sh_int	gsn_spear;
extern sh_int	gsn_spell_deflection;
extern sh_int	gsn_spell_shield;
extern sh_int	gsn_spell_trap;
extern sh_int	gsn_spirit_rack;
extern sh_int	gsn_stake;
extern sh_int	gsn_starflare;
extern sh_int	gsn_staves;
extern sh_int	gsn_steal;
extern sh_int	gsn_stone_skin;
extern sh_int	gsn_stone_spikes;
extern sh_int	gsn_subvert;
extern sh_int	gsn_summon;
extern sh_int	gsn_survey;
extern sh_int	gsn_swerve;
extern sh_int	gsn_sword;
extern sh_int	gsn_sword_and_dagger_style;
extern sh_int	gsn_tail_kick;
extern sh_int	gsn_tattoo;
extern sh_int	gsn_temperance;
extern sh_int	gsn_third_attack;
extern sh_int	gsn_third_eye;
extern sh_int	gsn_throw;
extern sh_int	gsn_titanic_attack;
extern sh_int	gsn_toxic_fumes;
extern sh_int	gsn_toxins;
extern sh_int	gsn_trackless_step;
extern sh_int	gsn_trample;
extern sh_int	gsn_trip;
extern sh_int	gsn_turn_undead;
extern sh_int	gsn_two_handed_style;
extern sh_int	gsn_underwater_breathing;
extern sh_int	gsn_vision;
extern sh_int	gsn_wands;
extern sh_int	gsn_warcry;
extern sh_int	gsn_water_spells;
extern sh_int	gsn_weaken;
extern sh_int	gsn_weaving;
extern sh_int	gsn_web;
extern sh_int	gsn_whip;
extern sh_int	gsn_wilderness_spear_style;
extern sh_int	gsn_wind_of_confusion;
extern sh_int	gsn_withering_cloud;
extern sh_int	gsn_word_of_recall;

extern sh_int	gsn_ice_shards;
extern sh_int	gsn_stone_touch;
extern sh_int	gsn_glacial_wave;
extern sh_int	gsn_earth_walk;
extern sh_int	gsn_flash;
extern sh_int	gsn_shriek;
extern sh_int	gsn_dark_shroud;
extern sh_int	gsn_soul_essence;






extern sh_int gprn_human;
extern sh_int gprn_elf;
extern sh_int gprn_dwarf;
extern sh_int gprn_titan;
extern sh_int gprn_vampire;
extern sh_int gprn_drow;
extern sh_int gprn_sith;
extern sh_int gprn_draconian;
extern sh_int gprn_slayer;
extern sh_int gprn_minotaur;
extern sh_int gprn_angel;
extern sh_int gprn_mystic;
extern sh_int gprn_demon;
extern sh_int gprn_lich;
extern sh_int gprn_avatar;
extern sh_int gprn_seraph;
extern sh_int gprn_berserker;
extern sh_int gprn_colossus;
extern sh_int gprn_fiend;
extern sh_int gprn_specter;
extern sh_int gprn_naga;
extern sh_int gprn_dragon;
extern sh_int gprn_changeling;
extern sh_int gprn_hell_baron;
extern sh_int gprn_wraith;
extern sh_int gprn_shaper;


extern sh_int grn_human;
extern sh_int grn_elf;
extern sh_int grn_dwarf;
extern sh_int grn_titan;
extern sh_int grn_vampire;
extern sh_int grn_drow;
extern sh_int grn_sith;
extern sh_int grn_draconian;
extern sh_int grn_slayer;
extern sh_int grn_minotaur;
extern sh_int grn_angel;
extern sh_int grn_mystic;
extern sh_int grn_demon;
extern sh_int grn_lich;
extern sh_int grn_avatar;
extern sh_int grn_seraph;
extern sh_int grn_berserker;
extern sh_int grn_colossus;
extern sh_int grn_fiend;
extern sh_int grn_specter;
extern sh_int grn_naga;
extern sh_int grn_dragon;
extern sh_int grn_changeling;
extern sh_int grn_hell_baron;
extern sh_int grn_wraith;
extern sh_int grn_shaper;
extern sh_int grn_were_changed;
extern sh_int grn_mob_vampire;
extern sh_int grn_bat;
extern sh_int grn_werewolf;
extern sh_int grn_bear;
extern sh_int grn_bugbear;
extern sh_int grn_cat;
extern sh_int grn_centipede;
extern sh_int grn_dog;
extern sh_int grn_doll;
extern sh_int grn_fido;
extern sh_int grn_fox;
extern sh_int grn_goblin;
extern sh_int grn_hobgoblin;
extern sh_int grn_kobold;
extern sh_int grn_lizard;
extern sh_int grn_doxian;
extern sh_int grn_orc;
extern sh_int grn_pig;
extern sh_int grn_rabbit;
extern sh_int grn_school_monster;
extern sh_int grn_snake;
extern sh_int grn_song_bird;
extern sh_int grn_golem;
extern sh_int grn_unicorn;
extern sh_int grn_griffon;
extern sh_int grn_troll;
extern sh_int grn_water_fowl;
extern sh_int grn_giant;
extern sh_int grn_wolf;
extern sh_int grn_wyvern;
extern sh_int grn_nileshian;
extern sh_int grn_skeleton;
extern sh_int grn_zombie;
extern sh_int grn_wisp;
extern sh_int grn_insect;
extern sh_int grn_gnome;
extern sh_int grn_angel_mob;
extern sh_int grn_demon_mob;
extern sh_int grn_rodent;
extern sh_int grn_treant;
extern sh_int grn_horse;
extern sh_int grn_bird;
extern sh_int grn_fungus;
extern sh_int grn_unique;




/*
 * Utility macros.
 */
#define IS_VALID(data)		((data) != NULL && (data)->valid)
#define VALIDATE(data)		((data)->valid = TRUE)
#define INVALIDATE(data)	((data)->valid = FALSE)
#define UMIN(a, b)		((a) < (b) ? (a) : (b))
#define UMAX(a, b)		((a) > (b) ? (a) : (b))
#define URANGE(a, b, c)		((b) < (a) ? (a) : ((b) > (c) ? (c) : (b)))
#define LOWER(c)		((c) >= 'A' && (c) <= 'Z' ? (c)+'a'-'A' : (c))
#define UPPER(c)		((c) >= 'a' && (c) <= 'z' ? (c)+'A'-'a' : (c))
#define IS_SET(flag, bit)	((flag) & (bit))
#define SET_BIT(var, bit)	((var) |= (bit))
#define REMOVE_BIT(var, bit)	((var) &= ~(bit))
#define TOGGLE_BIT(var, bit)	((var) ^= (bit))

// Forces the 'bit' on 'a' to equal the corresponding 'bit' on 'b'
#define MERGE_BIT(a, b, bit)	((a) = ((a) & ~(bit)) | ((b) & (bit)))

#define IS_NULLSTR(str)		((str) == NULL || (str)[0] == '\0')
#define ENTRE(min,num,max)	( ((min) < (num)) && ((num) < (max)) )
#define CHECK_POS(a, b, c)	{							\
					(a) = (b);					\
					if ( (a) < 0 )					\
					bug( "CHECK_POS : " c " == %d < 0", a );	\
				}

#define MSG(function)		{ if (!silent) 		\
				     (function); 	\
				                	\
				return FALSE; }
/*
 * Character macros.
 */
#define SAME_PLACE_AREA(from, to) ( find_area("Wilderness") == from ? ( from->x < (from->map_size_x/2) ? \
 			to->place_flags == PLACE_FIRST_CONTINENT : to->place_flags == PLACE_SECOND_CONTINENT \
 			) : from->place_flags == to->place_flags && from->place_flags != PLACE_NOWHERE )

#define SAME_PLACE(from, to) ( find_area("Wilderness") == from->area ? ( from->x < (from->area->map_size_x/2) ? \
			to->area->place_flags == PLACE_FIRST_CONTINENT : to->area->place_flags == PLACE_SECOND_CONTINENT \
			) : from->area->place_flags == to->area->place_flags && from->area->place_flags != PLACE_NOWHERE )

#define IS_OUTSIDE(ch)	( (ch)->in_room->wilds || \
		(!IS_SET((ch)->in_room->room_flags,ROOM_INDOORS) && \
			(ch)->in_room->sector_type != SECT_INSIDE && (ch)->in_room->sector_type != SECT_NETHERWORLD ) )

#define IS_SOCIAL(ch)	  (IS_SET((ch)->comm, COMM_SOCIAL))
#define IS_PK(ch)         (((ch)->church != NULL &&     \
			   (ch)->church->pk == TRUE ) || IS_SET((ch)->act,PLR_PK))
#define IS_QUESTING(ch)	        ( ch->quest != NULL )
#define IS_UNDEAD(ch)		(IS_SET((ch)->act, ACT_UNDEAD) || \
				 IS_SET((ch)->form, FORM_UNDEAD))
#define IS_MSP(ch)              (ch->desc ? IS_MSP_DESC( ch->desc ) : FALSE)
#define IS_MSP_DESC(d)          (IS_SET( d->bits, DESCRIPTOR_MSP ))
#define sqr(a) 			( a * a )
#define IS_DEAD(ch)		(ch->dead == TRUE)
#define IS_NPC(ch)		(IS_SET((ch)->act, ACT_IS_NPC))
#define IS_BOSS(ch)		(IS_NPC(ch) && ((ch)->pIndexData->boss))
#define IS_NPC_SHIP(ship)	(ship->npc_ship != NULL)
#define IS_IMMORTAL(ch)		(get_trust(ch) >= LEVEL_IMMORTAL && !IS_NPC(ch) && ch->pcdata->immortal != NULL)
#define IS_HERO(ch)		(get_trust(ch) >= LEVEL_HERO)
#define IS_TRUSTED(ch,level)	(get_trust((ch)) >= (level))
#define IS_AFFECTED(ch, sn)	(IS_SET((ch)->affected_by, (sn)))
#define IS_AFFECTED2(ch, sn)	(IS_SET((ch)->affected_by2, (sn)))
#define IN_NATURAL_FORM(ch)	(ch->natural_form)

#define GET_AGE(ch)		((int) (17 + ((ch)->played \
				    + current_time - (ch)->logon )/72000))

#define IS_GOOD(ch)		(ch->alignment >= 350)
#define IS_EVIL(ch)		(ch->alignment <= -350)
#define IS_NEUTRAL(ch)		(!IS_GOOD(ch) && !IS_EVIL(ch))

#define IS_AWAKE(ch)		(ch->position > POS_SLEEPING)
#define GET_AC(ch,type)		((ch)->armour[type])
#define GET_HITROLL(ch)	\
		((ch)->hitroll+str_app[get_curr_stat(ch,STAT_STR)].tohit)

/* 40 logeX - kind of maxes around 100 */
#define GET_DAMROLL(ch) \
		((ch)->damroll+str_app[get_curr_stat(ch,STAT_STR)].todam)

#define IS_OBJCASTER(ch)        (IS_SET((ch)->act, ACT_IS_NPC) \
		                 && (ch)->pIndexData->vnum == MOB_VNUM_OBJCASTER)

/* Wilderness macros. */
#define ROOM(room)		((room)->parent == -1 ? (room) : ((get_room_index((room)->parent))))

/* Race checks! */
#define IS_DROW(ch)		(ch->race == grn_drow || ch->race == grn_specter)
#define IS_MINOTAUR(ch)		(ch->race == grn_minotaur || ch->race == grn_hell_baron)
#define IS_SITH(ch)		(ch->race == grn_sith || ch->race == grn_naga)
#define IS_LICH(ch)		(ch->race == grn_lich || ch->race == grn_wraith)
#define IS_HUMAN(ch)		(ch->race == grn_human || ch->race == grn_avatar)
#define IS_DWARF(ch)		(ch->race == grn_dwarf || ch->race == grn_berserker)
#define IS_TITAN(ch)		(ch->race == grn_titan || ch->race == grn_colossus)
#define IS_SAGE(ch)		(get_profession((ch), SECOND_SUBCLASS_THIEF) == CLASS_THIEF_SAGE)
#define IS_ANGEL(ch)		(ch->race == grn_angel)
#define IS_MYSTIC(ch)		(ch->race == grn_mystic)
#define IS_DEMON(ch)		(ch->race == grn_demon)
#define IS_REMORT(ch)		(race_table[ch->race].pgprn && pc_race_table[*race_table[ch->race].pgprn].remort)
#define IS_VAMPIRE(ch)		(ch->race == grn_vampire || ch->race == grn_fiend)
#define IS_SLAYER(ch)		(ch->race == grn_slayer || ch->race == grn_changeling)
#define IS_DRACONIAN(ch)	(ch->race == grn_draconian || ch->race == grn_dragon)
#define IS_DRAGON(ch)		(ch->race == grn_dragon)
#define IS_ELF(ch)		(ch->race == grn_elf || ch->race == grn_seraph)


#define WAIT_STATE(ch, npulse)	((ch)->wait = UMAX((ch)->wait, (npulse)))
#define NO_RECALL_STATE(ch, npulse) ((ch)->no_recall = UMAX((ch)->no_recall, (npulse)))
#define DAZE_STATE(ch, npulse)  ((ch)->daze = UMAX((ch)->daze, (npulse)))
#define CAST_STATE(ch, npulse)  ((ch)->cast = UMAX((ch)->cast, (npulse)))
#define PANIC_STATE(ch, npulse)  ((ch)->panic = UMAX((ch)->panic, (npulse)))
#define RESURRECT_STATE(ch, npulse)  ((ch)->resurrect = UMAX((ch)->resurrect, (npulse)))
#define FADE_STATE(ch, npulse)  ((ch)->fade = UMAX((ch)->fade, (npulse)))
#define MUSIC_STATE(ch, npulse)  ((ch)->music = UMAX((ch)->music, (npulse)))
#define BREW_STATE(ch, npulse)  ((ch)->brew = UMAX((ch)->brew, (npulse)))
#define TATTOO_STATE(ch, npulse)  ((ch)->inking = UMAX((ch)->inking, (npulse)))
#define PAROXYSM_STATE(ch, npulse) ((ch)->paroxysm = UMAX((ch)->brew, (npulse)))
#define BIND_STATE(ch, npulse)  ((ch)->bind = UMAX((ch)->bind, (npulse)))
#define BOMB_STATE(ch, npulse)  ((ch)->bomb = UMAX((ch)->bomb, (npulse)))
#define HIDE_STATE(ch, npulse)  ((ch)->hide = UMAX((ch)->hide, (npulse)))
#define RECITE_STATE(ch, npulse)  ((ch)->recite = UMAX((ch)->recite, (npulse)))
#define SCRIBE_STATE(ch, npulse)  ((ch)->scribe = UMAX((ch)->scribe, (npulse)))
#define RANGED_STATE(ch, npulse)  ((ch)->ranged = UMAX((ch)->ranged, (npulse)))
#define SHIP_STATE(ch, npulse)  ((ch)->ship_move = UMAX((ch)->ship_move, (npulse)))
#define SHIP_ATTACK_STATE(ch, npulse)  ((ch)->ship_attack = UMAX((ch)->ship_attack, (npulse)))
#define REVERIE_STATE(ch, npulse)  ((ch)->reverie = UMAX((ch)->reverie, (npulse)))
#define COIN_WEIGHT(ch) 	((ch)->gold/200 + (ch)->silver/800)
#define get_carry_weight(ch)	((ch)->carry_weight + COIN_WEIGHT(ch))
#define PULLING_CART(ch) \
		((!IS_NPC(ch) && ch->pulled_cart) ? ch->pulled_cart : NULL)
#define MOUNTED(ch) \
		((!IS_NPC(ch) && ch->mount && ch->riding) ? ch->mount : NULL)
#define RIDDEN(ch) \
		((IS_NPC(ch) && ch->rider && ch->riding) ? ch->rider : NULL)
#define IS_DRUNK(ch)		((ch->pcdata->condition[COND_DRUNK] > 10))


#define IS_STONED(ch)		((ch->pcdata->condition[COND_STONED] > 3))
#define IS_SICK_STONED(ch)	((ch->pcdata->condition[COND_STONED] > 10))
#define HAS_TRIGGER_MOB(ch, trig)	(has_trigger((ch)->pIndexData->progs, (trig)))
#define HAS_TRIGGER_OBJ(obj, trig)	(has_trigger((obj)->pIndexData->progs, (trig)))
#define HAS_TRIGGER_ROOM(room, trig)	(has_trigger((room)->progs->progs, (trig)))
#define HAS_TRIGGER_TOKEN(token, trig)	(has_trigger((token)->pIndexData->progs, (trig)))

#define IS_SWITCHED( ch )       ( ch->desc && ch->desc->original )
#define IS_BUILDER(ch, Area)	( !IS_NPC(ch) && !IS_SWITCHED( ch ) &&	  \
				( ch->pcdata->security >= Area->security  \
				|| strstr( Area->builders, ch->name )	  \
				|| strstr( Area->builders, "All" ) ) )
#define IS_MORPHED(ch)          (ch->morphed == TRUE)
#define IN_CHURCH(ch)           (ch->church != NULL)
#define IS_CHURCH_EVIL(ch)      (ch->church != NULL && ch->church->alignment == CHURCH_EVIL)
#define IS_CHURCH_GOOD(ch)      (ch->church != NULL && ch->church->alignment == CHURCH_GOOD)
#define IS_CHURCH_NEUTRAL(ch)   (ch->church != NULL && ch->church->alignment == CHURCH_NEUTRAL)
#define IS_SHIFTED_WEREWOLF(ch)	(ch->shifted == SHIFTED_WEREWOLF )
#define IS_SHIFTED_SLAYER(ch)	(ch->shifted == SHIFTED_SLAYER )
#define IS_SHIFTED(ch)		(ch->shifted != SHIFTED_NONE )
#define IS_SAFE(ch) (IS_SET(ch->in_room->room_flags, ROOM_SAFE))
#define ON_QUEST(ch) (ch->quest != NULL)
#define IS_INVASION_LEADER(ch)   ( IS_SET(ch->act2, ACT2_INVASION_LEADER ))
#define IS_PIRATE(ch)   (!IS_NPC(ch) ? (ch->pcdata->rank[CONT_SERALIA] == NPC_SHIP_RANK_PIRATE || \
    ch->pcdata->rank[CONT_ATHEMIA] == NPC_SHIP_RANK_PIRATE) : ( IS_SET(ch->act2, ACT2_PIRATE )))
#define IN_SERALIA(area) ( (area->place_flags == PLACE_FIRST_CONTINENT) )
#define IN_ATHEMIA(area) ( (area->place_flags == PLACE_SECOND_CONTINENT) )

#define IS_PIRATE_IN_AREA(ch)		( !IS_NPC(ch) && (((ch->in_room->area->place_flags == PLACE_FIRST_CONTINENT) && \
			ch->pcdata->rank[CONT_SERALIA] == NPC_SHIP_RANK_PIRATE) || \
					((ch->in_room->area->place_flags == PLACE_SECOND_CONTINENT) && \
			ch->pcdata->rank[CONT_ATHEMIA] == NPC_SHIP_RANK_PIRATE)))
#define ON_SHIP(ch)             ((ch)->in_room->area == get_sailing_boat_area() \
          			&& (ch)->in_room->ship != NULL)
#define IN_SHIP_NEST(ch)        (ch->in_room->vnum == ROOM_VNUM_SAILING_BOAT_NEST)

#define IS_SHIP_IN_HARBOUR(ship) (ship->ship->in_room->vnum == ROOM_VNUM_SEA_PLITH_HARBOUR || \
		ship->ship->in_room->vnum == ROOM_VNUM_SEA_SOUTHERN_HARBOUR || \
		ship->ship->in_room->vnum == ROOM_VNUM_SEA_NORTHERN_HARBOUR )
/* VIZZWILDS */
#define IN_WILDERNESS(ch)  (ch->in_wilds)
#define IS_WILDERNESS(in_room)	(in_room->wilds)
#define IN_NETHERWORLD(ch)  (IN_WILDERNESS(ch) && !str_cmp(ch->in_wilds->name, "Netherworld"))

#define IN_CHAT(ch)  (!str_cmp(ch->in_room->area->name, "Elysium"))
#define IN_EDEN(ch)	(ch->in_room->area == eden_area)
/* NIB : 20070122 : Added the NULL parameter at the end */
#define act(format,ch,v1,v2,o1,o2,a1,a2,type)\
	act_new((format),(ch),(v1),(v2),(o1),(o2),(a1),(a2),(type),POS_RESTING,NULL)

#define damage(a,b,c,d,e,f) damage_new(( a ),( b ),NULL,( c ),( d ),( e ),( f ))


/*
 * Object macros.
 */
#define CAN_WEAR(obj, part)	(IS_SET((obj)->wear_flags,  (part)))
#define IS_OBJ_STAT(obj, stat)	(IS_SET((obj)->extra_flags,(stat)))
#define IS_OBJ2_STAT(obj,stat)  (IS_SET((obj)->extra2_flags,(stat)))
#define IS_WEAPON_STAT(obj,stat)(IS_SET((obj)->value[4],(stat)))
#define WEIGHT_MULT(obj)	((obj)->item_type == ITEM_CONTAINER ? \
	(obj)->value[4] : 100)
#define CORPSE_TYPE(obj)	((obj)->value[0])
#define CORPSE_RESURRECT(obj)	((obj)->value[1])
#define CORPSE_ANIMATE(obj)	((obj)->value[2])
#define CORPSE_PARTS(obj)	((obj)->value[3])
#define CORPSE_FLAGS(obj)	((obj)->value[4])
#define CORPSE_MOBILE(obj)	((obj)->value[5])

/*
 * Description macros.
 */
#define HANDLE(ch)  ( (IS_NPC((ch)) \
                      || IS_SWITCHED(ch) \
		      || IS_MORPHED(ch)) ? (ch)->short_descr : (ch)->name )
#define VNUM(ch)  ((ch)->pIndexData ? (ch)->pIndexData->vnum : 0)

/* For loading and writing objects */
#define MAX_NEST 100

extern          int 			nest_level;
extern		OBJ_DATA	*	rgObjNest[MAX_NEST];

#if defined(KEY)
#undef KEY
#endif

#define KEY( literal, field, value )					\
				if ( !str_cmp( word, literal ) )	\
				{					\
				    field  = value;			\
				    fMatch = TRUE;			\
				    break;				\
				}

#define elementsof(x)	(sizeof(x) / sizeof(x[0]))

/* provided to free strings */
#if defined(KEYS)
#undef KEYS
#endif

#define KEYS( literal, field, value )					\
				if ( !str_cmp( word, literal ) )	\
				{					\
				    free_string(field);			\
				    field  = value;			\
				    fMatch = TRUE;			\
				    break;				\
				}

/*
 * Structure for a social in the socials table.
 */
struct	social_type
{
    char      name[20];
    char *    char_no_arg;
    char *    others_no_arg;
    char *    char_found;
    char *    others_found;
    char *    vict_found;
    char *    char_not_found;
    char *    char_auto;
    char *    others_auto;
};

struct invasion_quest
{
    CHAR_DATA *leader;
    CHAR_DATA *invasion_mob_list;
    INVASION_QUEST *next;
    int max_level;
    time_t expires;
};

/*
 * Global constants.
 */
extern	const	struct	str_app_type	str_app		[52];
extern	const	struct	int_app_type	int_app		[52];
extern	const	struct	wis_app_type	wis_app		[52];
extern	const	struct	dex_app_type	dex_app		[52];
extern	const	struct	con_app_type	con_app		[52];
extern	const		char *  	words_table	[];
extern	const	int	size_weight[];
extern	const	struct	trade_type	trade_table	[];
extern	const		long  		plith_docks_table[];
extern	const		long  		treasure_table[];
extern  const           long            food_table[];
extern  const   struct  rep_type	rating_table    [];
extern  const   struct  map_exit_type map_exit_table    [];
extern  const   struct  rank_type	rank_table      [];
extern	const	struct	class_type	class_table	[MAX_CLASS];
extern	const	struct	sub_class_type	sub_class_table [];
extern	const	struct	weapon_type	weapon_table	[];
extern	const	struct	weapon_type	ranged_weapon_table	[];
extern	const	struct	crew_type	crew_table	[];
extern	const	struct	tunneler_place_type	tunneler_place_table	[];
extern	const	struct	item_type	auto_war_table	[];
extern	const	struct	item_type	boat_table	[];
extern	const	struct	item_type	npc_boat_table	[];
extern  const  	struct	item_type	npc_sub_type_boat_table [];
extern  const struct  item_type ship_state_table  [];
extern	const	struct	music_type	music_table	[];
extern  const   struct  material_type 	material_table  [];
extern  const   struct  item_type	item_table	[];
extern  const   struct  item_type       token_table     [];
extern	const	struct	player_setting_type	pc_set_table	[];
extern	const	struct	wiznet_type	wiznet_table	[];
extern	const	struct	attack_type	attack_table	[];
extern  const	struct  race_type	race_table	[];
extern	const	struct	pc_race_type	pc_race_table	[];
extern  const	struct	spec_type	spec_table	[];
extern	const	struct	liq_type	liq_table	[];
extern	const	struct	skill_type	skill_table	[MAX_SKILL];
extern          int                     mob_skill_table [MAX_MOB_SKILL_LEVEL];
extern  const   struct  church_command_type church_command_table [];
extern  const   struct  group_type      group_table	[MAX_GROUP];
extern          struct social_type      social_table	[MAX_SOCIALS];
extern	const	struct	rep_type	rating_table	[];
extern	const	struct	sound_type	sound_table	[];
extern  const   struct  newbie_eq_type  newbie_eq_table [];
extern  const   struct  toxin_type      toxin_table     [MAX_TOXIN];
extern  const   struct  herb_type       herb_table      [MAX_HERB];
extern  	struct  boost_type	boost_table	[];
extern  STAT_DATA		stat_table	[10];
extern  IMMORTAL_DATA *immortal_groups[MAX_IMMORTAL_GROUPS];

/*
 * Global variables.
 */
extern		BOUNTY_DATA	  *	bounty_list;
extern		CHAR_DATA	  *     casting_list;
extern		CHAT_ROOM_DATA    *     chat_room_list;
extern		CHURCH_DATA	  *	church_list;
extern		DESCRIPTOR_DATA   *	descriptor_list;
extern		FILE *			fpReserve;
extern		HELP_DATA	  *	help_first;
extern		MAIL_DATA	  *	mail_list;
extern		NPC_SHIP_DATA	  *	npc_ship_list;
extern		QUEST_INDEX_DATA  *	quest_index_list;
extern		SHOP_DATA	  *	shop_first;
extern		TIME_INFO_DATA		time_info;
extern		TRADE_ITEM	  *	trade_produce_list;
extern		char 		  *	help_greeting;
extern		bool			MOBtrigger;
extern		bool			global;
extern		bool			logAll;
extern		char			bug_buf		[];
extern		char			log_buf		[];
extern		time_t			current_time;
extern          SCRIPT_DATA       *     mprog_list;
extern          SCRIPT_DATA       *     oprog_list;
extern          SCRIPT_DATA       *     rprog_list;
extern          SCRIPT_DATA       *     tprog_list;
extern          SCRIPT_DATA       *     aprog_list;
extern          SCRIPT_DATA       *     iprog_list;
extern          SCRIPT_DATA       *     dprog_list;
extern          ROOM_INDEX_DATA   *	room_index_hash[MAX_KEY_HASH];
extern		PROG_DATA	  *	prog_data_virtual;
extern		char		  *     room_name_virtual;
/* Direct reference to the relics for performance */
extern          OBJ_DATA *		pneuma_relic;
extern          OBJ_DATA *		damage_relic;
extern          OBJ_DATA *		xp_relic;
extern          OBJ_DATA *		hp_regen_relic;
extern          OBJ_DATA *		mana_regen_relic;

/* New malloc fields. */
extern		long			numStrings;
extern		long			stringSpace;
extern		long			numChars;
extern		long			charSpace;
extern		IMMORTAL_DATA		*immortal_list;
extern		IMMORTAL_DATA		*unassigned_immortal_list;


/*
 * OS-dependent declarations.
 * These are all very standard library functions,
 *  but some systems have incomplete or non-ansi header files.
 */
#if	defined(linux)
char *	crypt		args( ( const char *key, const char *salt ) );
#endif

#if	defined(MIPS_OS)
char *	crypt		args( ( const char *key, const char *salt ) );
#endif

#if	defined(NeXT)
char *	crypt		args( ( const char *key, const char *salt ) );
#endif

#if	defined(sequent)
char *	crypt		args( ( const char *key, const char *salt ) );
int	fclose		args( ( FILE *stream ) );
int	fprintf		args( ( FILE *stream, const char *format, ... ) );
int	fread		args( ( void *ptr, int size, int n, FILE *stream ) );
int	fseek		args( ( FILE *stream, long offset, int ptrname ) );
void	perror		args( ( const char *s ) );
int	ungetc		args( ( int c, FILE *stream ) );
#endif

#if	defined(sun)
char *	crypt		args( ( const char *key, const char *salt ) );
int	fclose		args( ( FILE *stream ) );
int	fprintf		args( ( FILE *stream, const char *format, ... ) );
#if	defined(SYSV)
siz_t	fread		args( ( void *ptr, size_t size, size_t n,
			    FILE *stream) );
#elif !defined(__SVR4)
int	fread		args( ( void *ptr, int size, int n, FILE *stream ) );
#endif
int	fseek		args( ( FILE *stream, long offset, int ptrname ) );
void	perror		args( ( const char *s ) );
int	ungetc		args( ( int c, FILE *stream ) );
#endif

#if	defined(ultrix)
char *	crypt		args( ( const char *key, const char *salt ) );
#endif

/*
 * The crypt(3) function is not available on some operating systems.
 * In particular, the U.S. Government prohibits its export from the
 *   United States to foreign countries.
 * Turn on NOCRYPT to keep passwords in plain text.
 */
#if	defined(NOCRYPT)
#define crypt(s1, s2)	(s1)
#endif


/*
 * Data files used by the server.
 *
 * AREA_LIST contains a list of areas to boot.
 * All files are read in completely at bootup.
 * Most output files (bug, idea, typo, shutdown) are append-only.
 *
 * The NULL_FILE is held open so that we have a stream handle in reserve,
 * so players can go ahead and telnet to all the other descriptors.
 * Then we close it whenever we need to open a file (e.g. a save file).
 */
#define PLAYER_DIR      "../player/"        	/* Player files */
#define OLD_PLAYER_DIR	"../player.old/"
#define GOD_DIR         "../player/staff/"  		/* Staff Pfiles */
#define TEMP_FILE	"../player/romtmp"
#define NULL_FILE	"/dev/null"		/* To reserve one stream */
#define DATA_DIR		"../data/"
#define WORLD_DIR		"../data/world/"
#define BOAT_DIR		"../data/world/boats/"
#define SYSTEM_DIR		"../data/system/"
#define NOTE_DIR		"../data/notes/"
#define ORG_DIR			"../data/orgs/"
#define DUMP_DIR		"../data/dump/"
#define STATS_DIR		"../data/stats/"
#define HELP_DIR		"../data/help/"

/*World files - Regarding things specifically for the game world. */
#define PROJECTS_FILE	WORLD_DIR "projects.dat"
#define STAFF_FILE		WORLD_DIR "staff.dat"
#define PERM_OBJS_FILE	WORLD_DIR "perm_objs.dat"
#define PERSIST_FILE	WORLD_DIR "persist.dat"
#define GQ_FILE			WORLD_DIR "gq.dat"
#define AREA_LIST       WORLD_DIR "area.lst"  		/* List of areas*/
#define HELP_FILE		WORLD_DIR "help.dat"
/*Boat data */
#define SAILING_FILE	BOAT_DIR "sailing.dat"
#define NPC_SHIPS_FILE	BOAT_DIR "npc_ships.dat"
/*System Data
#define BUG_FILE        SYSTEM_DIR "bugs.txt" 		/ For 'bug' and bug() Unused
#define TYPO_FILE       SYSTEM_DIR "typos.txt" 		/ For 'typo' Unused */
#define SHUTDOWN_FILE   SYSTEM_DIR "shutdown.txt"		/* For 'shutdown'*/
#define MAINTENANCE_FILE   SYSTEM_DIR "maintenance.txt"		/* For 'shutdown'*/
#define BAN_FILE		SYSTEM_DIR "ban.txt"
/*#define MUSIC_FILE	SYSTEM_DIR	"music.txt"		Unused */
#define CHAT_FILE		SYSTEM_DIR "chat_rooms.dat"
#define MAIL_FILE		SYSTEM_DIR "mail.dat"
#define CONFIG_FILE		SYSTEM_DIR "gconfig.rc"
/*Notes of all kinds */
#define NOTE_FILE       NOTE_DIR "notes.not"		/* For 'notes'*/
/*#define PENALTY_FILE	NOTE_DIR "penal.not"		Unused */
#define NEWS_FILE		NOTE_DIR "news.not"
#define CHANGES_FILE	NOTE_DIR "chang.not"
/*Orgs (more to come here) */
#define CHURCHES_FILE	ORG_DIR "churches.dat"
/*Dump commands, the obj/skill/help db's */
#define OBJ_DB_FILE		DUMP_DIR "object_db.txt"
#define SKILLS_DB_FILE	DUMP_DIR "skills_db.txt"
#define HELP_DB_FILE	DUMP_DIR "help_db.txt"

#define BLUEPRINTS_FILE		WORLD_DIR "blueprints.dat"
#define DUNGEONS_FILE		WORLD_DIR "dungeons.dat"
#define INSTANCES_FILE		WORLD_DIR "instances.dat"


/* POST msg queue */
#define MSGQUEUE	1111

/*
 * Function prototypes.
 */
#define CD	CHAR_DATA
#define MID	MOB_INDEX_DATA
#define OD	OBJ_DATA
#define OID	OBJ_INDEX_DATA
#define RID	ROOM_INDEX_DATA
#define SF	SPEC_FUN
#define AD	AFFECT_DATA
#define PC	PROG_CODE
#define SD	SHIP_DATA
#define NSD	NPC_SHIP_DATA
#define NID	NPC_SHIP_INDEX_DATA

/* act_comm.c */
bool add_grouped	args( ( CHAR_DATA *ch, CHAR_DATA *master, bool show ) );
bool is_same_group	args( ( CHAR_DATA *ach, CHAR_DATA *bch ) );
void add_follower	args( ( CHAR_DATA *ch, CHAR_DATA *master, bool show ) );
void check_sex	args( ( CHAR_DATA *ch) );
void church_echo	args( ( CHURCH_DATA *church, char *message ) );
void crier_announce     args( ( char *argument ) );
void die_follower	args( ( CHAR_DATA *ch ) );
void do_say( CHAR_DATA *ch, char *argument );
void double_xp( CHAR_DATA *victim );
void echo_around     args( ( ROOM_INDEX_DATA *pRoom, char *message) );
void gecho		args( ( char *message ) );
void nuke_pets	args( ( CHAR_DATA *ch ) );
void room_echo       args( ( ROOM_INDEX_DATA *pRoom, char *message) );
void sector_echo     args( ( AREA_DATA *pArea, char *message, int sector) );
void area_echo     args( ( AREA_DATA *pArea, char *message) );
void stop_follower	args( ( CHAR_DATA *ch, bool show ) );
void stop_grouped	args( ( CHAR_DATA *ch ) );

/* bit.c */
char *flag_string( const struct flag_type *flag_table, long bits );
char *flag_string_commas( const struct flag_type *flag_table, long bits );
long flag_value( const struct flag_type *flag_table, char *argument);
char *	affect_loc_name	args( ( int location ) );
char *	affect_bit_name	args( ( long vector ) );
char *	affect2_bit_name	args( ( long vector ) );
char *	affects_bit_name	args( ( long vector, long vector2 ) );
char *	extra_bit_name	args( ( long extra_flags ) );
char * 	wear_bit_name	args( ( int wear_flags ) );
char *	act_bit_name	args( ( int act_type, long act_flags ) );
char *	off_bit_name	args( ( int off_flags ) );
char *  imm_bit_name	args( ( int imm_flags ) );
char * 	form_bit_name	args( ( long form_flags ) );
char *	part_bit_name	args( ( int part_flags ) );
char *	weapon_bit_name	args( ( int weapon_flags ) );
char *  comm_bit_name	args( ( int comm_flags ) );
char *	cont_bit_name	args( ( int cont_flags) );
char *channel_flag_bit_name(int channel_flags);

/* lookup.c */
int	church_lookup	(const char *name);
int	position_lookup	(const char *name);
int 	sex_lookup	(const char *name);
int 	size_lookup	(const char *name);
int flag_lookup (const char *name, const struct flag_type *flag_table);
char *flag_name(const struct flag_type *flag_table, int bit);
int damage_class_lookup (const char *name);	/* @@@NIB */
int toxin_lookup (const char *name);	/* @@@NIB */

/* hunt.c */
int find_path( long in_room_vnum, long out_room_vnum, CHAR_DATA *ch, int depth, int in_zone);
CHAR_DATA *get_char_area( CHAR_DATA *ch, char *argument );
void hunt_victim args( ( CHAR_DATA *ch ) );

/* weather.c */
void update_weather_for_chars();
void storm_affect_char_background args((CHAR_DATA *ch, int storm_type));
void storm_affect_char args((CHAR_DATA *ch, int storm_type));
int get_storm_for_room(ROOM_INDEX_DATA *pRoom);
void update_weather args((void));

/* invasion.c */
INVASION_QUEST* create_invasion_quest(AREA_DATA *pArea, int max_level, long p_leader_vnum, long p_mob_vnum);
void extract_invasion_quest(INVASION_QUEST *quest);
void check_invasion_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *victim);

/* treasuremap.c */
OBJ_DATA* create_treasure_map();

/* act_info.c */
char* get_wilderness_map args((AREA_DATA *pArea, int lx, int ly, int bonus_view_x, int bonus_view_y));
CHURCH_DATA *find_char_church args( (CHAR_DATA *ch) );
DECLARE_DO_FUN( do_look );
char *find_desc_for_room( ROOM_INDEX_DATA *room,CHAR_DATA *viewer);
char *get_char_where args((CHAR_DATA * ch));
int find_char_position_in_church(CHAR_DATA * ch);
int fstr_len(char *a);
void remove_member args( ( CHURCH_PLAYER_DATA *member ) );
void set_title	args( ( CHAR_DATA *ch, char *title ) );
void show_list_to_char(OBJ_DATA * list, CHAR_DATA * ch, bool fShort, bool fShowNothing);
/* VIZZWILDS
 * void show_map_to_char( CHAR_DATA *ch, CHAR_DATA *to, int bonus_view_x, int bonus_view_y);
 */
void show_room_description( CHAR_DATA *ch, ROOM_INDEX_DATA *room );
void show_help_to_ch( CHAR_DATA *ch, HELP_DATA *help );

/* stats.c */
BUFFER *get_stats( int type );

/* act_move.c */
bool can_move( CHAR_DATA *ch, ROOM_INDEX_DATA *room );
bool can_move_room( CHAR_DATA *ch, int door, ROOM_INDEX_DATA *room );
bool check_ice( CHAR_DATA *ch, bool show );
bool room_check( CHAR_DATA *ch, ROOM_INDEX_DATA *room );
int find_door( CHAR_DATA *ch, char *arg, bool show );
int get_player_classnth args( ( CHAR_DATA *ch ) );
void check_ambush( CHAR_DATA *ch );
void check_see_hidden( CHAR_DATA *ch );
void check_traps( CHAR_DATA *ch, bool show );
void drunk_walk( CHAR_DATA *ch, int door );
void fade_end       args( ( CHAR_DATA *ch ) );
void hide_end       args( ( CHAR_DATA *ch ) );
ROOM_INDEX_DATA *exit_destination(EXIT_DATA *pexit);
bool exit_destination_data(EXIT_DATA *pexit, DESTINATION_DATA *pdest);
void move_char	args( ( CHAR_DATA *ch, int door, bool follow ) );
void move_char_new( CHAR_DATA *ch, int door );
OBJ_DATA *get_key( CHAR_DATA *ch, int vnum );
void use_key( CHAR_DATA *ch, OBJ_DATA *key );
bool move_success args( ( CHAR_DATA *ch ) );
bool check_room_flames( CHAR_DATA *ch, bool show );
bool check_rocks( CHAR_DATA *ch, bool show );
void check_room_shield_source( CHAR_DATA *ch, bool show );

/* music.c */
void    music_end       args( ( CHAR_DATA *ch ) );
bool was_bard( CHAR_DATA *ch );

/* shoot.c */
CHAR_DATA *search_dir_name( CHAR_DATA *ch, char *argument, int direction, int range );
int get_distance( CHAR_DATA *ch, char *argument, int direction, int range );
int get_ranged_skill( CHAR_DATA * );
void ranged_end( CHAR_DATA *ch );

/* act_obj.c */
bool can_loot(CHAR_DATA *ch, OBJ_DATA *obj);
bool is_extra_damage_relic_in_room args( (ROOM_INDEX_DATA *room) );
bool is_extra_pneuma_relic_in_room args( (ROOM_INDEX_DATA *room) );
bool is_extra_xp_relic_in_room args( (ROOM_INDEX_DATA *room) );
bool is_hp_regen_relic_in_room args( (ROOM_INDEX_DATA *room) );
bool is_mana_regen_relic_in_room args( (ROOM_INDEX_DATA *room) );
bool remove_obj( CHAR_DATA *ch, int iWear, bool fReplace );
CHAR_DATA *find_keeper( CHAR_DATA *ch );
int get_cost( CHAR_DATA *keeper, OBJ_DATA *obj, bool fBuy );
long adjust_keeper_price(CHAR_DATA *keeper, long price, bool fBuy);
bool get_stock_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, SHOP_REQUEST_DATA *request, char *argument);
OBJ_DATA *get_obj_keeper( CHAR_DATA *ch, CHAR_DATA *keeper, char *argument );
void bomb_end( CHAR_DATA *ch);
void brew_end( CHAR_DATA *ch, sh_int sn );
void do_restring( CHAR_DATA *ch, char *argument );
void obj_to_keeper( OBJ_DATA *obj, CHAR_DATA *ch );
void recite_end( CHAR_DATA *ch);
void removeall(CHAR_DATA *ch);
void repair_end( CHAR_DATA *ch );
void save_last_wear( CHAR_DATA *ch );
void scribe_end( CHAR_DATA *ch, sh_int sn, sh_int sn2, sh_int sn3 );
void ink_end( CHAR_DATA *ch, CHAR_DATA *victim, sh_int loc, sh_int sn, sh_int sn2, sh_int sn3 );
int get_wear_loc(CHAR_DATA *ch, OBJ_DATA *obj);
void wear_obj( CHAR_DATA *ch, OBJ_DATA *obj, bool fReplace);
void change_money(CHAR_DATA *ch, CHAR_DATA *changer, long gold, long silver);

/* act_obj2.c */
void reset_obj( OBJ_DATA *obj );

/* act_wiz.c */
int gconfig_read(void);
int gconfig_write(void);
void do_chset( CHAR_DATA *ch, char *argument );
void save_shares	args( ( void ) );
void wiznet(char *string, CHAR_DATA *ch, OBJ_DATA *obj, long flag, long flag_skip, int min_level );

/* alias.c */
void 	substitute_alias args( (DESCRIPTOR_DATA *d, char *input) );

/* ban.c */
bool check_ban	args( ( char *site, int type) );

/* comm.c */
void printf_to_char( CHAR_DATA *, char *, ... );
void show_string( struct descriptor_data *d, char *input);
void close_socket( DESCRIPTOR_DATA *dclose );
void write_to_buffer( DESCRIPTOR_DATA *d, const char *txt, int length );
void send_to_char	args( ( const char *txt, CHAR_DATA *ch ) );
void page_to_char	args( ( const char *txt, CHAR_DATA *ch ) );
void act_new ( char *format, CHAR_DATA *ch, CHAR_DATA *vch, CHAR_DATA *vch2, OBJ_DATA *obj, OBJ_DATA *obj2, void *arg1, void *arg2, int type, int min_pos, CHAR_TEST char_func);
char *stptok            args( (const char *s, char *tok, size_t toklen, char *brk));
int	colour		args( ( char type, CHAR_DATA *ch, char *string ) );
void	colourconv	args( ( char *buffer, const char *txt, CHAR_DATA *ch ) );
void	send_to_char_bw	args( ( const char *txt, CHAR_DATA *ch ) );
void	page_to_char_bw	args( ( const char *txt, CHAR_DATA *ch ) );
void	update_pc_timers( CHAR_DATA *ch );
void    plogf             args( ( char * fmt, ... ) );

/* auction.c */
void    auction_update  args( ( void ) );
void    auction_channel args( ( char *msg ) );

/* autowar.c */
void    test_for_end_of_war args( ( void ) );
void	auto_war_time_finish args( ( void ) );
void    start_war 		args( ( void ) );
void    scatter_players args( ( void ) );
void    auto_war_echo   args( ( char *message ) );
void 	war_channel( char *msg );

/* db.c */
TOKEN_INDEX_DATA *get_token_index(long vnum);
bool is_singular_token(TOKEN_INDEX_DATA *index);
int     get_this_class args( ( CHAR_DATA *ch, int sn ) );
void	reset_area      args( ( AREA_DATA * pArea ) );
void	reset_room	args( ( ROOM_INDEX_DATA *pRoom ) );
char *	print_flags	args( ( long flag ));
void	boot_db		args( ( void ) );
void	area_update	args( ( bool fBoot ) );
void    check_objects   args( ( void ) );
void    check_mobs      args( ( void ) );
CD *	create_mobile	args( ( MOB_INDEX_DATA *pMobIndex, bool persistLoad ) );
CD *	clone_mobile	args( ( CHAR_DATA *parent ) );
OD *	create_object_noid	args( ( OBJ_INDEX_DATA *pObjIndex, int level, bool affects ) );
OD *	create_object	args( ( OBJ_INDEX_DATA *pObjIndex, int level, bool affects ) );
void	clone_object	args( ( OBJ_DATA *parent, OBJ_DATA *clone ) );
void	clear_char	args( ( CHAR_DATA *ch ) );
char *	get_extra_descr	args( ( const char *name, EXTRA_DESCR_DATA *ed ) );
MID *	get_mob_index	args( ( long vnum ) );
OID *	get_obj_index	args( ( long vnum ) );
RID *	get_room_index	args( ( long vnum ) );
NID *	get_npc_ship_index args( ( long vnum ) );
PC *	get_prog_index args( ( long vnum, int type ) );
SCRIPT_DATA *	get_script_index args( ( long vnum, int type ) );
char	fread_letter	args( ( FILE *fp ) );
long	fread_number	args( ( FILE *fp ) );
long 	fread_flag	args( ( FILE *fp ) );
char *	fread_string	args( ( FILE *fp ) );
char *  fread_string_eol args(( FILE *fp ) );
void	fread_to_eol	args( ( FILE *fp ) );
char *	fread_word	args( ( FILE *fp ) );
long	flag_convert	args( ( char letter) );
void *	alloc_mem	args( ( int sMem ) );
void *	alloc_perm	args( ( long sMem ) );
void	free_mem	args( ( void *pMem, int sMem ) );
void	str_upper	args( ( char *src, char *dest ) );
void	str_lower	args( ( char *src, char *dest ) );
char *	str_dup		args( ( const char *str ) );
void	free_string	args( ( char *pstr ) );
int	number_fuzzy	args( ( int number ) );
int	number_range	args( ( int from, int to ) );
int	number_percent	args( ( void ) );
int	number_door	args( ( void ) );
int	number_bits	args( ( int width ) );
long     number_mm       args( ( void ) );
long	dice		args( ( int number, int size ) );
int	interpolate	args( ( int level, int value_00, int value_32 ) );
void	smash_tilde	args( ( char *str ) );
int	str_cmp				args( ( const char *astr, const char *bstr ) );
int str_cmp_nocolour	args( ( const char *astr, const char *bstr ) );
bool	str_prefix	args( ( const char *astr, const char *bstr ) );
bool	str_infix	args( ( const char *astr, const char *bstr ) );
bool	str_suffix	args( ( const char *astr, const char *bstr ) );
char *	strreplace	args( ( char *Str, char *OldStr, char *NewStr ) );
char *	capitalize	args( ( const char *str ) );
char *	cap		args( ( const char *str ) );
void	append_file	args( ( CHAR_DATA *ch, char *file, char *str ) );
void	bug		args( ( const char *str, int param ) );
void	log_string	args( ( const char *str ) );
void	tail_chain	args( ( void ) );
void    boat_attack (CHAR_DATA *ch );
/* VIZZWILDS
 * int 	get_squares_to_show_x(ROOM_INDEX_DATA *pRoom, int bonus_view);
 * int 	get_squares_to_show_y(ROOM_INDEX_DATA *pRoom, int bonus_view);
 */
RID     *create_and_map_room( ROOM_INDEX_DATA *room, long vnum, SHIP_DATA *ship);
void 	create_exit( ROOM_INDEX_DATA *from, ROOM_INDEX_DATA *to, int door, int flags);
void 	bitten_end( CHAR_DATA *ch );
void 	load_newhelps( FILE *fp, char *fname );
void 	new_reset( ROOM_INDEX_DATA *, RESET_DATA *);
char 	*fix_string( const char *str );
AREA_DATA *get_wilderness_area ( void );
char *skip_whitespace(register char *str);

/* db2.c */
AREA_DATA *get_random_area( CHAR_DATA *ch, int continent, bool no_get_random );
CHAR_DATA *get_random_mob_area( CHAR_DATA *ch, AREA_DATA *area);
CHAR_DATA *get_random_mob( CHAR_DATA *ch, int continent );
MOB_INDEX_DATA *get_random_mob_index( AREA_DATA *area ) ;
OBJ_DATA *get_random_obj_area( CHAR_DATA *ch, AREA_DATA *area, ROOM_INDEX_DATA *room);
OBJ_DATA *get_random_obj( CHAR_DATA *ch, int continent );
ROOM_INDEX_DATA *get_random_room_list_byflags( CHAR_DATA *ch, LLIST *rooms, int n_room_flags, int n_room2_flags );
ROOM_INDEX_DATA *get_random_room_area_byflags( CHAR_DATA *ch, AREA_DATA *area, int n_room_flags, int n_room2_flags );
ROOM_INDEX_DATA *get_random_room( CHAR_DATA *ch, int continent );
char *nocolour(const char *string);
char *short_to_name (const char *short_desc);
int strlen_no_colours( const char *str );
void fix_short_description( char *short_descr );
void generate_poa_resets( int level );
void global_reset( void );
void load_area_trade( AREA_DATA *pArea, FILE *fp );
void load_npc_ships();
void load_statistics();
void read_chat_rooms();
void read_mail( void );
void write_chat_rooms();
void write_mail( void );
ROOM_INDEX_DATA *get_random_room_area( CHAR_DATA *ch, AREA_DATA *area );
void load_stat( char *filename, int type );
void write_help_to_disk(HELP_CATEGORY *hcat, HELP_DATA *help);

/* effects.c */
void acid_effect( void *vo, int level, int dam, int target );
void cold_effect( void *vo, int level, int dam, int target );
void fire_effect( void *vo, int level, int dam, int target );
void hurt_vampires( CHAR_DATA *ch );
void poison_effect( void *vo, int level, int dam, int target );
void shock_effect( void *vo, int level, int dam, int target );
void toxin_apply_affect( CHAR_DATA *ch, CHAR_DATA *victim, int type, bool show_effect);
void toxin_effect( void *vo, int level, int dam, int target );
void damage_vampires( CHAR_DATA *ch, int dam );
void vamp_sun_message( CHAR_DATA *ch, int dam );
void toxic_fumes_effect( CHAR_DATA *victim, CHAR_DATA *ch );

/* fight.c */
CHAR_DATA* create_player_hunter(long vnum, CHAR_DATA *target);
bool check_acro( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool check_catch( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield);
bool check_dodge( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool check_parry( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool check_shield_block( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool check_shield_block_projectile( CHAR_DATA *ch, CHAR_DATA *victim, char *attack, OBJ_DATA *projectile );
bool check_spear_block( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool check_speed_swerve( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield );
bool damage_new( CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *weapon, int dam, int dt, int class, bool show );
bool is_safe( CHAR_DATA *ch, CHAR_DATA *victim, bool show );
bool is_safe_spell( CHAR_DATA *ch, CHAR_DATA *victim, bool area );
bool one_hit( CHAR_DATA *ch, CHAR_DATA *victim, int dt, bool secondary );
int xp_compute( CHAR_DATA *gch, CHAR_DATA *victim, int total_levels );
void bind_end( CHAR_DATA *ch );
void check_assist( CHAR_DATA *ch, CHAR_DATA *victim );
void check_killer( CHAR_DATA *ch, CHAR_DATA *victim);
void dam_message( CHAR_DATA *ch, CHAR_DATA *victim, int dam, int dt, bool immune );
void death_cry( CHAR_DATA *ch, bool has_head, bool messages );
void death_mob_echo( CHAR_DATA *victim );
OBJ_DATA *disarm( CHAR_DATA *ch, CHAR_DATA *victim );
void group_gain( CHAR_DATA *ch, CHAR_DATA *victim );
void set_corpse_data(OBJ_DATA *corpse, int corpse_type);
int blend_corpsetypes (int t1, int t2);
OBJ_DATA *make_corpse( CHAR_DATA *ch, bool has_head, int corpse_type, bool messages );
void mob_hit( CHAR_DATA *ch, CHAR_DATA *victim, int dt );
void multi_hit( CHAR_DATA *ch, CHAR_DATA *victim, int dt );
void player_kill( CHAR_DATA *ch, CHAR_DATA *victim );
int damage_to_corpse(int dam_type);
OBJ_DATA *raw_kill( CHAR_DATA *victim, bool has_head, bool messages, int corpse_type);
void resurrect_end( CHAR_DATA *ch );
void enter_combat(CHAR_DATA *ch, CHAR_DATA *victim, bool silent);
bool set_fighting( CHAR_DATA *ch, CHAR_DATA *victim);
void stop_fighting( CHAR_DATA *ch, bool fBoth);
void stop_holdup( CHAR_DATA *ch);
void update_pos( CHAR_DATA *victim );
void violence_update( void );


/* fight2.c */
void set_pk_timer(CHAR_DATA *ch, CHAR_DATA *victim, int time);
bool check_evasion(CHAR_DATA *ch);
void shift_char(CHAR_DATA *ch, bool silent);

/* drunk.c */
char    *makedrunk      args( (char *string, CHAR_DATA *ch) );

/* gq.c */
void write_gq( void );
void read_gq( void );

/* msgqueue.c */
void	process_message_queue( void );

/* html.c */
BUFFER  *get_churches_html( void );
BUFFER  *get_players_html( void );
char	*format_and_colour_html( char *buf );
BUFFER  *get_stats_html( int type );
BUFFER  *get_stats_for_html( int type );

/* boat.c */
void award_ship_quest_points args((int area_type, CHAR_DATA *ch, int points));
void award_reputation_points args((int area_type, CHAR_DATA *ch, int points));
void    create_and_add_cannons args(( SHIP_DATA *ship, int number ));
void  set_pirate_status( CHAR_DATA *ch, int region, int bounty );
bool  has_enough_crew args( ( SHIP_DATA *ship ) );
void    transfer_cargo args( ( SHIP_DATA *source, SHIP_DATA *destination ) );
void    add_move_waypoint args( ( SHIP_DATA *ship, int x, int y ) );
void    clear_waypoints args( ( SHIP_DATA *ship ) );
SD      *create_new_sailing_boat args( (char *owner_name, int ship_type) );
NSD     *create_npc_sailing_boat args( (long vnum) );
void    boat_move   args( ( CHAR_DATA *ch ) );
void    boat_echo       args( ( SHIP_DATA *ship, char *str ) );
void    boat_damage     args( ( SHIP_DATA *ship, long amount, int type) );
void    extract_boat    args( ( SHIP_DATA *ship ) );
void    extract_npc_boat args( ( NPC_SHIP_DATA *npc_ship ) );
void    save_sailing_boats args( ( void ) );
void    load_sailing_boats args( ( void ) );
AREA_DATA *get_sailing_boat_area args(( void ));
void    stop_boarding args( ( SHIP_DATA *ship ) );
bool    is_boat_safe  args( ( CHAR_DATA *ch, SHIP_DATA *ship, SHIP_DATA *ship2) );
void    make_ship_crew_return_to_ship args(( SHIP_DATA *boarding_ship ));
sh_int  get_rating args( ( int ships_destroyed ) );
sh_int  get_player_reputation args( ( int reputation_points ) );
CHAR_DATA *get_captain args( ( SHIP_DATA *ship ) );

/* recycle.c */
EXTRA_DESCR_DATA *new_extra_descr(void);

/* mem.c */
EXIT_DATA *new_wilderness_exit args( ( void ) );
INVASION_QUEST *new_invasion_quest(void);
STORM_DATA *new_storm_data(void);
AFFECT_DATA *new_affect(void);
AMBUSH_DATA *new_ambush( void );
AREA_DATA *new_area( void );
AUTO_WAR *new_auto_war  args( ( int war_type, int min_players, int min_level, int max_level ) );
CHAT_BAN_DATA *new_chat_ban( void );
CHAT_OP_DATA *new_chat_op( void );
CHAT_ROOM_DATA *new_chat_room( void );
CHURCH_DATA *new_church      args( ( void ) );
CHURCH_PLAYER_DATA *new_church_player args( ( void ) );
COMMAND_DATA *new_command(void);
CONDITIONAL_DESCR_DATA *new_conditional_descr(void);
EXIT_DATA *new_exit args( ( void ) );
GQ_MOB_DATA *new_gq_mob( void );
GQ_OBJ_DATA *new_gq_obj( void );
HELP_CATEGORY *new_help_category();
IGNORE_DATA *new_ignore( void );
MAIL_DATA *new_mail( void );
MOB_INDEX_DATA *new_mob_index( void );
NPC_SHIP_INDEX_DATA *new_npc_ship_index args ( ( void ) );
OBJ_INDEX_DATA *new_obj_index( void );
PROG_DATA *new_prog_data(void);
LLIST **new_prog_bank(void);
PROG_LIST *new_trigger(void);
QUEST_INDEX_DATA *new_quest_index( void );
QUEST_DATA *new_quest( void );
QUEST_LIST *new_quest_list( void );
QUEST_PART_DATA *new_quest_part(void);
QUESTOR_DATA *new_questor_data( void );
RESET_DATA *new_reset_data( void );
ROOM_INDEX_DATA *new_room_index( void );
SHIP_CREW_DATA *new_ship_crew args( ( void ) );
SHOP_DATA *new_shop( void );
SPELL_DATA *new_spell(void);
STRING_DATA *new_string_data( void );
TOKEN_DATA *new_token(void);
TOKEN_INDEX_DATA *new_token_index(void);
WAYPOINT_DATA *new_waypoint args( ( void ) );
void free_wilderness_exit args( ( EXIT_DATA *pexit ) );
void free_invasion_quest( INVASION_QUEST *quest );
void free_storm_data( STORM_DATA *storm_data );
void free_affect( AFFECT_DATA *af );
void free_ambush( AMBUSH_DATA *ambush );
void free_auto_war  args( ( AUTO_WAR *auto_war ) );
void free_chat_ban( CHAT_BAN_DATA *pBan );
void free_chat_op( CHAT_OP_DATA *pOp );
void free_chat_room( CHAT_ROOM_DATA *pChat );
void free_church     args( ( CHURCH_DATA *pChurch ) );
void free_church_player args( ( CHURCH_PLAYER_DATA *pMember ) );
void free_command(COMMAND_DATA *command);
void free_conditional_descr( CONDITIONAL_DESCR_DATA *cd );
void free_exit( EXIT_DATA *pExit );
void free_extra_descr	args( ( EXTRA_DESCR_DATA *pExtra ) );
void free_gq_mob( GQ_MOB_DATA *gq_mob );
void free_gq_obj( GQ_OBJ_DATA *gq_obj );
void free_help( HELP_DATA *pHelp );
void free_help_category( HELP_CATEGORY *hcat );
void free_ignore( IGNORE_DATA *ignore );
void free_mail( MAIL_DATA *mail );
void free_obj_index( OBJ_INDEX_DATA *pObj );
void free_obj(OBJ_DATA *obj );
void free_prog_data(PROG_DATA *pr_dat);
void free_trigger(PROG_LIST *trigger);
void free_prog_list(LLIST **pr_list);
void free_quest_index( QUEST_INDEX_DATA *quest_index );
void free_quest( QUEST_DATA *pQuest );
void free_quest_list( QUEST_LIST *quest_list );
void free_quest_part( QUEST_PART_DATA *pPart );
void free_reset_data( RESET_DATA *pReset );
void free_ship_crew( SHIP_CREW_DATA *crew );
void free_shop( SHOP_DATA *pShop );
void free_spell(SPELL_DATA *spell);
void free_string_data( STRING_DATA *string );
void free_token( TOKEN_DATA *token );
void free_token_index( TOKEN_INDEX_DATA *token_index);
void free_trade_item(TRADE_ITEM *item);
void free_waypoint args( ( WAYPOINT_DATA *waypoint ) );
void new_trade_item	args ( ( AREA_DATA *area, sh_int type, long replenish_time, long replenish_amount, long max_qty, long min_price, long max_price, long obj_vnum ) );
HELP_DATA *new_help(void);
void free_help(HELP_DATA *);
EVENT_DATA *new_event(void);
void free_event(EVENT_DATA *event);
PROJECT_DATA *new_project(void);
void free_project(PROJECT_DATA *proj);
PROJECT_BUILDER_DATA *new_project_builder(void);
void free_project_builder(PROJECT_BUILDER_DATA *pb);
PROJECT_INQUIRY_DATA *new_project_inquiry(void);
void free_project_inquiry(PROJECT_INQUIRY_DATA *pinq);
IMMORTAL_DATA *new_immortal(void);
void free_immortal(IMMORTAL_DATA *immortal);
LOG_ENTRY_DATA *new_log_entry(void);
void free_log_entry(LOG_ENTRY_DATA *log);
char *create_affect_cname(char *name);
char *get_affect_cname(char *name);

/* quest.c */
bool generate_quest( CHAR_DATA *ch, CHAR_DATA *questman );
void quest_update(void);
bool generate_quest_part( CHAR_DATA *ch, CHAR_DATA *questman, QUEST_PART_DATA *part, int partno );
bool is_quest_item( OBJ_DATA *obj );
bool is_quest_token( OBJ_DATA *obj );
int count_quest_parts( CHAR_DATA *ch );
QUEST_INDEX_DATA *get_quest_index( long vnum );
void check_quest_rescue_mob( CHAR_DATA *ch, bool show );
void check_quest_retrieve_obj( CHAR_DATA *ch, OBJ_DATA *obj, bool show );
void check_quest_slay_mob( CHAR_DATA *ch, CHAR_DATA *mob, bool show );
void check_quest_totally_complete( CHAR_DATA *ch, bool show );
void check_quest_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool show);
bool check_quest_custom_task(CHAR_DATA *ch, int task, bool show);

/* handler.c */
int get_coord_distance( int x1, int y1, int x2, int y2 );
CHAR_DATA *get_player(char *name);
void	affect_fix_char( CHAR_DATA *ch );
void    affect_modify( CHAR_DATA *ch, AFFECT_DATA *paf, bool fAdd );
void	char_to_team    args( ( CHAR_DATA *ch ) );
void	char_from_team  args( ( CHAR_DATA *ch ) );
void	char_to_invasion    args( ( CHAR_DATA *ch, INVASION_QUEST *quest ) );
void char_from_invasion(CHAR_DATA *ch, INVASION_QUEST *quest);
int count_items_list( OBJ_DATA *list );
int count_items_list_nest( OBJ_DATA *list );
AREA_DATA *find_area_at_land_coords    args((int x, int y ));
AREA_DATA *find_area_at_coords         args((int x, int y ));
bool    is_on_ship      args((CHAR_DATA *ch, SHIP_DATA *ship));
ROOM_INDEX_DATA *find_location( CHAR_DATA *ch, char *arg );
AREA_DATA *find_area    args((char *name ));
int     parse_direction args(( char *arg ));
int     get_trade_item args(( char *arg ));
TRADE_ITEM *find_trade_item args(( AREA_DATA* pArea, char *arg ));
char    *upper_first ( char *arg );
CD      *get_cart_pulled( OBJ_DATA *obj );
AD  	*affect_find args( (AFFECT_DATA *paf, int sn));
void	affect_check	args( (CHAR_DATA *ch, int where, long vector, long vector2) );
int	count_users	args( (OBJ_DATA *obj) );
void 	deduct_cost	args( (CHAR_DATA *ch, int cost) );
void	affect_enchant	args( (OBJ_DATA *obj) );
int 	check_immune	args( (CHAR_DATA *ch, sh_int dam_type) );
int 	material_lookup args( ( const char *name) );
int	weapon_lookup	args( ( const char *name) );
int	weapon_type	args( ( const char *name) );
int	ranged_weapon_type	args( ( const char *name) );
char 	*weapon_name	args( ( int weapon_Type) );
char 	*ranged_weapon_name	args( ( int weapon_Type) );
char	*item_name	args( ( int item_type) );
int	attack_lookup	args( ( const char *name) );
long	wiznet_lookup	args( ( const char *name) );
int	class_lookup	args( ( const char *name) );
int	sub_class_lookup(CHAR_DATA *ch, const char *);
int	sub_class_search(const char *);
bool	is_church	args( (CHAR_DATA *ch) );
bool	is_same_church	args( (CHAR_DATA *ch, CHAR_DATA *victim));
bool	is_old_mob	args ( (CHAR_DATA *ch) );
int	get_skill	args( ( CHAR_DATA *ch, int sn ) );
int	get_weapon_sn	args( ( CHAR_DATA *ch ) );
int	get_objweapon_sn	args( (OBJ_DATA *obj) );
int	get_weapon_skill args(( CHAR_DATA *ch, int sn ) );
int     get_age         args( ( CHAR_DATA *ch ) );
void	reset_char	args( ( CHAR_DATA *ch )  );
int	get_trust	args( ( CHAR_DATA *ch ) );

void set_mod_stat(CHAR_DATA *ch, int stat, int value);
void add_mod_stat(CHAR_DATA *ch, int stat, int adjust);
void set_perm_stat(CHAR_DATA *ch, int stat, int value);
void set_perm_stat_range(CHAR_DATA *ch, int stat, int value, int mn, int mx);
void add_perm_stat(CHAR_DATA *ch, int stat, int adjust);

int	get_curr_stat	args( ( CHAR_DATA *ch, int stat ) );
int 	get_max_train	args( ( CHAR_DATA *ch, int stat ) );
int	can_carry_n	args( ( CHAR_DATA *ch ) );
int	can_carry_w	args( ( CHAR_DATA *ch ) );
bool	is_name		args( ( char *str, char *namelist ) );
bool	is_exact_name	args( ( char *str, char *namelist ) );
void	affect_to_char	args( ( CHAR_DATA *ch, AFFECT_DATA *paf ) );
void	affect_to_obj	args( ( OBJ_DATA *obj, AFFECT_DATA *paf ) );
void	catalyst_to_obj	args( ( OBJ_DATA *obj, AFFECT_DATA *paf ) );
void	affect_to_room	args( ( ROOM_INDEX_DATA *room, AFFECT_DATA *paf ) );
void	affect_remove	args( ( CHAR_DATA *ch, AFFECT_DATA *paf ) );
bool	affect_removeall_obj	args( ( OBJ_DATA *obj ) );
bool	affect_remove_obj args( (OBJ_DATA *obj, AFFECT_DATA *paf ) );
void	affect_strip	args( ( CHAR_DATA *ch, int sn ) );
void	affect_strip_obj	args( ( OBJ_DATA *obj, int sn ) );
void	affect_strip_name	args( ( CHAR_DATA *ch, char *name ) );
void	affect_strip_name_obj	args( ( OBJ_DATA *obj, char *name ) );
void	affect_stripall_wearloc args( (CHAR_DATA *ch, int wear_loc) );
bool	is_affected	args( ( CHAR_DATA *ch, int sn ) );
bool	is_affected_obj	args( ( OBJ_DATA *obj, int sn ) );
bool	is_affected_name	args( ( CHAR_DATA *ch, char *name ) );
bool	is_affected_name_obj	args( ( OBJ_DATA *obj, char *name ) );
void	affect_join	args( ( CHAR_DATA *ch, AFFECT_DATA *paf ) );
void	affect_join_obj	args( ( OBJ_DATA *obj, AFFECT_DATA *paf ) );
void	affect_join_full	args( ( CHAR_DATA *ch, AFFECT_DATA *paf ) );
void	affect_join_full_obj	args( ( OBJ_DATA *obj, AFFECT_DATA *paf ) );
int     count_char_locker   args( ( CHAR_DATA *ch ) );
void	char_from_room	args( ( CHAR_DATA *ch ) );
void	char_from_crew	args( ( CHAR_DATA *ch ) );
void	char_to_room	args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex ) );
void    char_to_room_static args( (CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex ) );
void	char_to_crew	args( ( CHAR_DATA *ch, SHIP_DATA *ship ) );
void	obj_to_char	args( ( OBJ_DATA *obj, CHAR_DATA *ch ) );
void	obj_from_char	args( ( OBJ_DATA *obj ) );
void	obj_from_locker	args( ( OBJ_DATA *obj ) );
int	apply_ac	args( ( OBJ_DATA *obj, int iWear, int type ) );
OD *	get_eq_char	args( ( CHAR_DATA *ch, int iWear ) );
void	equip_char	args( ( CHAR_DATA *ch, OBJ_DATA *obj, int iWear ) );
int	unequip_char	args( ( CHAR_DATA *ch, OBJ_DATA *obj, bool show ) );
int	count_obj_list	args( ( OBJ_INDEX_DATA *obj, OBJ_DATA *list ) );
void	obj_from_room	args( ( OBJ_DATA *obj ) );
void	obj_to_room	args( ( OBJ_DATA *obj, ROOM_INDEX_DATA *pRoomIndex ) );
void    obj_to_vroom    args( ( OBJ_DATA *obj, WILDS_DATA *pWilds, int x, int y) );
void	obj_to_obj	args( ( OBJ_DATA *obj, OBJ_DATA *obj_to ) );
void	obj_to_locker	args( ( OBJ_DATA *obj, CHAR_DATA *ch ) );
void	obj_from_obj	args( ( OBJ_DATA *obj ) );
void extract_token(TOKEN_DATA *token);
void	extract_church	args( ( CHURCH_DATA *church ) );
void	extract_obj	args( ( OBJ_DATA *obj ) );
void	extract_char	args( ( CHAR_DATA *ch, bool fPull ) );
CD *	get_char_room		args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument ) );
CD *	get_char_world	args( ( CHAR_DATA *ch, char *argument ) );
CD *    find_char_world args( ( CHAR_DATA *ch, char *argument ) );
OD *	get_obj_type	args( ( OBJ_INDEX_DATA *pObjIndexData, ROOM_INDEX_DATA *pRoom ) );
OD *	get_obj_list	args( ( CHAR_DATA *ch, char *argument, OBJ_DATA *list ) );
OD *	get_obj_list_number	args( ( CHAR_DATA *ch, char *argument, int *number, OBJ_DATA *list ) );
OD *	get_obj_carry	args( ( CHAR_DATA *ch, char *argument, CHAR_DATA *viewer ) );
OD *	get_obj_carry_number	args( ( CHAR_DATA *ch, char *argument, int *nth, CHAR_DATA *viewer ) );
OD *	get_obj_vnum_carry	args( ( CHAR_DATA *ch, long vnum, CHAR_DATA *viewer ) );
OD *	get_obj_locker	args( ( CHAR_DATA *ch, char *argument ) );
OD *	get_obj_wear	args( ( CHAR_DATA *ch, char *argument, bool character ));
OD *	get_obj_wear_number	args( ( CHAR_DATA *ch, char *argument, int *nth, bool character ));
OD *	get_obj_inv			args( (CHAR_DATA *ch, char *argument, bool worn));
OD *	get_obj_inv_only	args( (CHAR_DATA *ch, char *argument, bool worn));
OD *	get_obj_here	args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument ) );
OD *	get_obj_here_number	args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *room, char *argument, int *nth ) );
OD *	get_obj_world	args( ( CHAR_DATA *ch, char *argument ) );
OD *	create_money	args( ( int gold, int silver ) );
int	get_obj_number	args( ( OBJ_DATA *obj ) );
int	get_obj_weight	args( ( OBJ_DATA *obj ) );
int	get_obj_weight_container	args( ( OBJ_DATA *obj ) );
bool	room_is_dark	args( ( ROOM_INDEX_DATA *pRoomIndex ) );
bool	is_room_owner	args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *room) );
bool	room_is_private	args( ( ROOM_INDEX_DATA *pRoomIndex, CHAR_DATA *) );
bool	can_see		args( ( CHAR_DATA *ch, CHAR_DATA *victim ) );
bool	can_see_obj	args( ( CHAR_DATA *ch, OBJ_DATA *obj ) );
bool	can_see_room	args( ( CHAR_DATA *ch, ROOM_INDEX_DATA *pRoomIndex) );
int get_carry_weight	args( ( CHAR_DATA *ch ) );
void hunt_char 		args( (CHAR_DATA *ch, CHAR_DATA *victim ) );
void	resurrect_pc   args ( ( CHAR_DATA *ch ) );
bool is_global_mob( CHAR_DATA *ch );
void line( CHAR_DATA *ch, int length );
char *pers( CHAR_DATA *ch, CHAR_DATA *looker );
bool can_see_shift( CHAR_DATA *ch, CHAR_DATA *victim );
char *extra2_bit_name( long extra2_flags );
char *extra3_bit_name( long extra3_flags );
char *extra4_bit_name( long extra4_flags );
bool is_ignoring( CHAR_DATA *victim, CHAR_DATA *ch );
bool both_hands_full( CHAR_DATA *ch );
bool one_hand_full( CHAR_DATA *ch );
bool is_sustained( CHAR_DATA *ch );
bool can_scare( CHAR_DATA *ch );
int get_obj_number_container( OBJ_DATA *obj );
bool is_relic( OBJ_INDEX_DATA *obj );
char *res_bit_name(int res_flags);
char *vuln_bit_name(int vuln_flags);
bool is_on_continent_1( CHAR_DATA *ch );
bool is_on_continent_2( CHAR_DATA *ch );
bool is_dislinked( ROOM_INDEX_DATA *pRoom );
bool is_good_church( CHAR_DATA *ch );
bool is_evil_church( CHAR_DATA *ch );
bool wields_item_type( CHAR_DATA *ch, int weapon_type );
char   *pirate_name_generator args(( void ));
int get_remort_race( CHAR_DATA *ch );
bool is_darked( ROOM_INDEX_DATA *room );
bool is_dead( CHAR_DATA *ch );
void deduct_move( CHAR_DATA *ch, int amount );
bool is_wearable( OBJ_DATA *obj );
bool is_using_anyone( OBJ_DATA *obj );
void return_from_maze( CHAR_DATA *ch );
CHAR_DATA *get_char_world_vnum( CHAR_DATA *ch, long vnum );
int get_number_in_container( OBJ_DATA *obj );
AREA_DATA *find_area_kwd( char *keyword );
long get_dp_value( OBJ_DATA *obj );
bool can_tell_while_quiet( CHAR_DATA *ch, CHAR_DATA *victim );
CHAR_DATA *get_char_world_index( CHAR_DATA *ch, MOB_INDEX_DATA *pMobIndex );
bool can_hunt( CHAR_DATA *ch, CHAR_DATA *victim );
int get_region( CHAR_DATA *ch );
int get_weight_coins( long silver, long gold );
bool check_ice_storm( ROOM_INDEX_DATA *room );
bool player_exists( char *argument );
void move_obj_into_container( CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container );
OBJ_DATA *get_skull( CHAR_DATA *ch, char *owner );
int count_exits( ROOM_INDEX_DATA *room );
bool is_room_pk( ROOM_INDEX_DATA *room, bool arena );
bool is_pk( CHAR_DATA *ch );
int get_num_dir( char *arg );
bool dislink_room( ROOM_INDEX_DATA *pRoom );
bool is_in_nature( CHAR_DATA *ch );
int is_pk_safe_range( ROOM_INDEX_DATA *room, int depth, int reverse_dir );
void stop_hunt( CHAR_DATA *ch, bool dead );
bool is_pulling_relic( CHAR_DATA *ch );
int index_helpfiles( int index, HELP_CATEGORY *hCat );
int get_profession(CHAR_DATA *ch, int class_type);
void set_profession(CHAR_DATA *ch, int class_type, int class_value);
ROOM_INDEX_DATA *obj_room(OBJ_DATA *obj);
ROOM_INDEX_DATA *token_room(TOKEN_DATA *token);
void exit_name(ROOM_INDEX_DATA *room, int door, char *kwd);
bool can_give_obj(CHAR_DATA *ch, OBJ_DATA *obj, CHAR_DATA *victim, bool silent);
bool can_drop_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent);
bool can_get_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent);
bool can_put_obj(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container, MAIL_DATA *mail, bool silent);
bool can_sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool silent);
ROOM_INDEX_DATA *find_safe_room(ROOM_INDEX_DATA *from_room, int depth, bool crossarea);
bool can_clear_exit(ROOM_INDEX_DATA *room);
TOKEN_DATA *give_token(TOKEN_INDEX_DATA *token_index, CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room);
void token_from_char(TOKEN_DATA *token);
void token_to_char(TOKEN_DATA *token, CHAR_DATA *ch);
void token_to_char_ex(TOKEN_DATA *token, CHAR_DATA *ch, char source, long flags);
TOKEN_DATA *get_token_list(LLIST *tokens, long vnum, int count);
TOKEN_DATA *get_token_char(CHAR_DATA *ch, long vnum, int count);
void token_from_obj(TOKEN_DATA *token);
void token_to_obj(TOKEN_DATA *token, OBJ_DATA *obj);
TOKEN_DATA *get_token_obj(OBJ_DATA *obj, long vnum, int count);
void token_from_room(TOKEN_DATA *token);
void token_to_room(TOKEN_DATA *token, ROOM_INDEX_DATA *room);
TOKEN_DATA *get_token_room(ROOM_INDEX_DATA *room, long vnum, int count);
void fix_magic_object_index(OBJ_INDEX_DATA *obj);
void extract_event(EVENT_DATA *event);
void extract_project_inquiry(PROJECT_INQUIRY_DATA *pinq);
void log_string_to_list(char *argument, LOG_ENTRY_DATA *list);
void save_logs();
int get_curr_group_stat(CHAR_DATA *ch, int stat);
int get_perm_group_stat(CHAR_DATA *ch, int stat);
int get_church_online_count(CHAR_DATA *ch);
int get_room_weight(ROOM_INDEX_DATA *room, bool mobs, bool objs, bool ground);
bool is_float_user(CHAR_DATA *ch);
int has_catalyst(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int method,int min_strength, int max_strength);
int use_catalyst_obj(CHAR_DATA *ch,ROOM_INDEX_DATA *room,OBJ_DATA *obj,int type,int left,int min_strength, int max_strength, bool active, bool show);
int use_catalyst_here(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int amount,int min_strength, int max_strength, bool active, bool show);
int use_catalyst(CHAR_DATA *ch,ROOM_INDEX_DATA *room,int type,int method,int amount,int min_strength, int max_strength, bool show);
void move_cart(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool delay);
void visit_rooms(ROOM_INDEX_DATA *room, VISIT_FUNC *func, int depth, void *argv[], int argc, bool closed);

/* help.c */
HELP_DATA *find_helpfile( char *keyword, HELP_CATEGORY *hcat );
HELP_CATEGORY *find_help_category( char *name, HELP_CATEGORY *list );
void insert_help( HELP_DATA *help, HELP_DATA **list );
HELP_CATEGORY *find_help_category_exact( char *name, HELP_CATEGORY *hcat);
HELP_DATA *lookup_help( char *keyword, int viewer_level, HELP_CATEGORY *hCat);
HELP_DATA *lookup_help_exact( char *keyword, int viewer_level, HELP_CATEGORY *hCat);
HELP_DATA *lookup_help_index( unsigned int index, int viewer_level, HELP_CATEGORY *hCat );
void lookup_category_multiple( char *keyword, int viewer_level, HELP_CATEGORY *hcat, BUFFER *buffer );
void lookup_help_multiple( char *keyword, int viewer_level, HELP_CATEGORY *hcat, BUFFER *buffer );
bool is_help_keyword(char *kwd, char *help_kwd);
int count_num_helps( char *keyword, int viewer_level, HELP_CATEGORY *hcat );
void save_helpfiles_new();
void read_helpfiles_new();
void save_help_category_new( FILE *fp, HELP_CATEGORY *hCat );
void save_help_new( FILE *fp, HELP_DATA *help );
HELP_CATEGORY *read_help_category_new( FILE *fp );
HELP_DATA *read_help_new( FILE *fp );

/* interp.c */
bool check_social( CHAR_DATA *ch, char *command, char *argument );
void	interpret	args( ( CHAR_DATA *ch, char *argument ) );
bool	is_number	args( ( char *arg ) );
int	number_argument	args( ( char *argument, char *arg ) );
int	mult_argument	args( ( char *argument, char *arg) );
char *	one_argument_norm	args( ( char *argument, char *arg_first ) );
char *	one_argument	args( ( char *argument, char *arg_first ) );
char *  one_caseful_argument   args( ( char *argument, char *arg_first ) );
void do_function (CHAR_DATA *ch, DO_FUN *do_fun, char *argument);
void stop_casting( CHAR_DATA *ch, bool messages );
void stop_music( CHAR_DATA *ch, bool messages );
void stop_ranged( CHAR_DATA *ch, bool messages );
bool is_allowed( char *command );
bool is_granted_command(CHAR_DATA *ch, char *name);

/* magic.c */
void	say_spell	args((CHAR_DATA *ch, int sn));
SPELL_FUNC(spell_null);
void    mob_cast        args( ( CHAR_DATA *ch, int sn, int level, char *argument) );
void    mob_cast_end    args( ( CHAR_DATA *ch) );
int	find_spell	args( ( CHAR_DATA *ch, const char *name) );
int 	mana_cost 	(CHAR_DATA *ch, int min_mana, int level);
int	skill_lookup	args( ( const char *name ) );
OD*	get_warp_stone	args( ( CHAR_DATA *ch ) );
bool	saves_spell	args( ( int level, CHAR_DATA *victim, sh_int dam_type ) );
void	obj_cast_spell	args( ( int sn, int level, CHAR_DATA *ch,
				    CHAR_DATA *victim, OBJ_DATA *obj ) );
void obj_cast( int sn, int level, OBJ_DATA *obj, ROOM_INDEX_DATA *room, char *argument);
bool can_gate( CHAR_DATA *ch, CHAR_DATA *victim );
void cast_end( CHAR_DATA *ch );
bool can_escape( CHAR_DATA *ch );
bool saves_dispel( CHAR_DATA *ch, CHAR_DATA *victim, int spell_level );
bool check_dispel(CHAR_DATA *ch, CHAR_DATA *victim, int sn);

/* magic2.c */
void reverie_end args( ( CHAR_DATA *ch, int amount ) );
void trance_end(CHAR_DATA *ch);
bool check_spell_deflection( CHAR_DATA *ch, CHAR_DATA *victim, int sn);
bool check_spell_deflection_token( CHAR_DATA *ch, CHAR_DATA *victim, TOKEN_DATA *tok, SCRIPT_DATA *script,char *target_name);

/* mail.c */
bool has_mail( CHAR_DATA *ch );
void check_new_mail( CHAR_DATA *ch );
void mail_update( void );
void obj_to_mail( OBJ_DATA *obj, MAIL_DATA *mail );
void obj_from_mail( OBJ_DATA *obj );
void mail_from_list( MAIL_DATA *mail );
int count_items_mail(MAIL_DATA *mail);
int count_weight_mail(MAIL_DATA *mail);

/* scripts.c */
int	program_flow	args( ( long vnum, char *source, CHAR_DATA *mob,
				OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
				CHAR_DATA *ch, const void *arg1,
				const void *arg2 ) );

int p_act_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type);
int p_exact_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type);
int p_name_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type);
int p_percent_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase);
int p_percent_token_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, TOKEN_DATA *tok, int type, char *phrase);
int p_percent2_trigger(AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase);
int p_number_trigger(int number, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase);
int p_bribe_trigger(CHAR_DATA *mob, CHAR_DATA *ch, int amount);
int p_exit_trigger(CHAR_DATA *ch, int dir, int type);
int p_direction_trigger(CHAR_DATA *ch, ROOM_INDEX_DATA *here, int dir, int type, int trigger);
int p_give_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, CHAR_DATA *ch, OBJ_DATA *dropped, int type);
int p_use_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type);
int p_use_on_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, char *argument);
int p_use_with_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, OBJ_DATA *obj1, OBJ_DATA *obj2, CHAR_DATA *victim, CHAR_DATA *victim2);
int p_greet_trigger(CHAR_DATA *ch, int type);
int	p_hprct_trigger(CHAR_DATA *mob, CHAR_DATA *ch);
int p_emoteat_trigger(CHAR_DATA *mob, CHAR_DATA *ch, char *emote);
int p_emote_trigger(CHAR_DATA *ch, char *emote);


int	script_login(CHAR_DATA *ch);
CHAR_DATA *get_random_char( CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token);

/* mob_cmds.c */
bool 	has_trigger(LLIST **, int);
/*int 	trigger_value(char *name, int type);
void	mob_interpret	args( ( CHAR_DATA *ch, char *argument ) );
void	obj_interpret	args( ( OBJ_DATA *obj, char *argument ) );
void    room_interpret	args( ( ROOM_INDEX_DATA *room, char *argument ) );
void    tokenother_interpret(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, char *argument);
void    token_interpret(TOKEN_DATA *token, char *argument);
int mpcmd_lookup(char *command);
int opcmd_lookup(char *command);
int rpcmd_lookup(char *command);*/


/* save.c */
bool find_class_skill( CHAR_DATA *ch, int class );
bool load_char_obj	args( ( DESCRIPTOR_DATA *d, char *name ) );
bool update_object( OBJ_DATA *obj );
OBJ_DATA *fread_obj_new( FILE *fp );
void cleanup_affects( OBJ_DATA *obj );
void descrew_subclasses( CHAR_DATA *);
void fix_broken_classes( CHAR_DATA *ch );
void fix_character( CHAR_DATA *ch );
void fix_object( OBJ_DATA *obj );
void fread_char      args( ( CHAR_DATA *ch,  FILE *fp ) );
void fread_char	args( ( CHAR_DATA *ch,  FILE *fp ) );
void fread_mount     args( ( CHAR_DATA *ch,  FILE *fp ) );
void fwrite_char     args( ( CHAR_DATA *ch,  FILE *fp ) );
void fwrite_mount    args( ( CHAR_DATA *pet, FILE *fp) );
void fwrite_obj( CHAR_DATA *ch,  OBJ_DATA  *obj, FILE *fp, int iNest, bool locker);
void fwrite_obj_new( CHAR_DATA *ch, OBJ_DATA *obj, FILE *fp, int iNest);
void fwrite_pet      args( ( CHAR_DATA *pet, FILE *fp) );
void read_permanent_objs ( );
void save_char_obj	args( ( CHAR_DATA *ch ) );
void write_permanent_objs ( );
void fwrite_token(TOKEN_DATA *token, FILE *fp);
void fwrite_skills(CHAR_DATA *ch, FILE *fp);
TOKEN_DATA *fread_token(FILE *fp);
void fread_skill(FILE *fp, CHAR_DATA *ch);
void fwrite_quest_part(FILE *fp, QUEST_PART_DATA *part);
QUEST_PART_DATA *fread_quest_part(FILE *fp);



/* skills.c */
bool 	parse_gen_groups args( ( CHAR_DATA *ch,char *argument ) );
void 	list_group_costs args( ( CHAR_DATA *ch ) );
void    list_group_known args( ( CHAR_DATA *ch ) );
long 	exp_per_level	args( ( CHAR_DATA *ch, long points ) );
void 	check_improve	args( ( CHAR_DATA *ch, int sn, bool success, int multiplier ) );
void check_improve_show( CHAR_DATA *ch, int sn, bool success, int multiplier, bool show );
int 	group_lookup	args( (const char *name) );
void	gn_add		args( ( CHAR_DATA *ch, int gn) );
void 	gn_remove	args( ( CHAR_DATA *ch, int gn) );
void 	group_add	args( ( CHAR_DATA *ch, const char *name, bool deduct) );
void	group_remove	args( ( CHAR_DATA *ch, const char *name) );
bool had_skill( CHAR_DATA *ch, int sn );
bool has_subclass_skill( int subclass, int skill );
bool can_practice( CHAR_DATA *ch, int sn );
bool has_class_skill ( int class, int sn );
bool is_global_skill( int sn );
bool should_have_skill( CHAR_DATA *ch, int sn );
void update_skills( CHAR_DATA *ch );
void fix_subclasses( CHAR_DATA *);
bool has_correct_classes( CHAR_DATA *ch );
void show_multiclass_choices(CHAR_DATA *ch, CHAR_DATA *looker);
bool can_choose_subclass(CHAR_DATA *ch, int subclass);

/* special.c */
SF *	spec_lookup	args( ( const char *name ) );
char *	spec_name	args( ( SPEC_FUN *function ) );

/* teleport.c */
RID *	room_by_name	args( ( char *target, int level, bool error) );

/* update.c */
void	healing_locket_update args( ( CHAR_DATA *ch ) );
void	advance_level	args( ( CHAR_DATA *ch, bool hide ) );
void	gain_exp	args( ( CHAR_DATA *ch, int gain ) );
void	gain_condition	args( ( CHAR_DATA *ch, int iCond, int value ) );
void	update_handler	args( ( void ) );
void    pneuma_relic_update args( ( void ) );
void    bitten_update( CHAR_DATA *ch );
void locket_update( OBJ_DATA *obj );
void update_hunting_pc( CHAR_DATA *ch );
void update_public_boat( int time );
void update_area_trade( void );
void 	time_update(void);
void	ship_hotspot_update(void);
void event_update(void);

/* word_spell.c */
void   read_words         args( ( void ) );
void   free_words  	  args( ( void ) );
bool   spell_check_word   args( ( char *input_word ) );
char  *word_check_line   args( ( char *input_word ) );

/* mccp.c */
bool compressStart(DESCRIPTOR_DATA *desc);
bool compressEnd(DESCRIPTOR_DATA *desc);
bool processCompressed(DESCRIPTOR_DATA *desc);
bool writeCompressed(DESCRIPTOR_DATA *desc, char *txt, int length);

/* mount.c */
void    do_mount        args( ( CHAR_DATA *ch, char *argument ) );
void    do_dismount     args( ( CHAR_DATA *ch, char *argument ) );
void	do_buy_mount	args( ( CHAR_DATA *ch, char *argument ) );

/* string.c */
void	string_edit	args( ( CHAR_DATA *ch, char **pString ) );
void    string_append   args( ( CHAR_DATA *ch, char **pString ) );
char *	string_replace_static	args( ( char * orig, char * old, char * new ) );
char *	string_replace	args( ( char * orig, char * old, char * new ) );
void    string_add      args( ( CHAR_DATA *ch, char *argument ) );
char *  format_paragraph args( (char *oldstring) );
char *  format_string   args( ( char *oldstring /*, bool fSpace */ ) );
char *  first_arg       args( ( char *argument, char *arg_first, bool fCase ) );
char *	string_unpad	args( ( char * argument ) );
char *	string_proper	args( ( char * argument ) );

/* olc.c */
AREA_DATA *get_area_data args( ( long anum ) );
NPC_SHIP_DATA *get_npc_ship_data args( ( long vnum ) );
bool	run_olc_editor	args( ( DESCRIPTOR_DATA *d ) );
char	*olc_ed_name	args( ( CHAR_DATA *ch ) );
char	*olc_ed_vnum	args( ( CHAR_DATA *ch ) );
int calc_obj_armour args ( (int level, int strength) );
void set_weapon_dice( OBJ_INDEX_DATA *objIndex );
void set_weapon_dice_obj( OBJ_DATA *obj );
void set_armour_obj( OBJ_DATA *obj );
void set_armour( OBJ_INDEX_DATA *objIndex );
void set_mob_damdice( MOB_INDEX_DATA *pMobIndex );
void set_mob_hitdice( MOB_INDEX_DATA *pMob );
void set_mob_manadice(MOB_INDEX_DATA *pMobIndex);
void set_mob_movedice(MOB_INDEX_DATA *pMobIndex);
bool has_access_area( CHAR_DATA *ch, AREA_DATA *area );
bool has_access_helpcat( CHAR_DATA *ch, HELP_CATEGORY *hcat );
bool has_access_help( CHAR_DATA *ch, HELP_DATA *help);
void add_reset( ROOM_INDEX_DATA *room, RESET_DATA *pReset, int index );
bool has_imp_sig( MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj );
void use_imp_sig( MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj );

/* olc_act.c */
AREA_DATA *get_vnum_area( long vnum );
bool rp_change_exit args( ( ROOM_INDEX_DATA *pRoom, char *argument, int door));
int get_armour_strength(char *argument);

/* olc_act2.c */
char *condition_type_to_name ( int type );
char *condition_phrase_to_name ( int type, int phrase );

/* olc_save.c */
void fix_fulldesc( OBJ_INDEX_DATA *pObjIndex );
void save_area args( ( AREA_DATA *pArea ) );
void save_help( void );
void save_quest( FILE *fp, QUEST_INDEX_DATA *pQuestIndex );
void save_quests( FILE *fp, AREA_DATA *pArea );
void save_ship args( (FILE *fp, NPC_SHIP_INDEX_DATA *pShipIndex) );
void save_npc_ships();
char *fwrite_flag( long flags, char buf[] );

/* wilderness.c */
void  remove_wilderness_exits args(( void ));
void  generate_wilderness_exits( AREA_DATA *pArea );
void    generate_wilderness_exit( ROOM_INDEX_DATA *pRoomIndex );
/* VIZZWILDS
 * bool    check_for_bad_room(AREA_DATA *pArea, int x, int y);
 */
void  load_rooms  args( ( FILE *fp ) );
/* VIZZWILDS
 * bool    map_char_cmp(AREA_DATA *pArea, int x, int y, char *check);
 */
ROOM_INDEX_DATA *get_wilderness_room_for_exit(ROOM_INDEX_DATA *room, int dir);

/* note.c */
int count_note         args( ( CHAR_DATA *ch, int type ));
int count_spool(CHAR_DATA *ch, NOTE_DATA *spool);
void save_notes(int type);
void load_notes(void);
void parse_note(CHAR_DATA *ch, char *argument, int type);
void load_thread(char *name, NOTE_DATA **list, int type, time_t free_time);
void append_note(NOTE_DATA *pnote);
bool is_note_to( CHAR_DATA *ch, NOTE_DATA *pnote );
void note_attach( CHAR_DATA *ch, int type );
void note_remove( CHAR_DATA *ch, NOTE_DATA *pnote, bool delete);
bool hide_note( CHAR_DATA *ch, NOTE_DATA *pnote );
void update_read(CHAR_DATA *ch, NOTE_DATA *pnote);
int count_note( CHAR_DATA *ch, int type);

/* lookup.c */
int	race_lookup	args( ( const char *name) );
int	item_lookup	args( ( const char *name) );
int	liq_lookup	args( ( const char *name) );
char 	*get_weapon_class(OBJ_INDEX_DATA *obj);

/* social.c */
bool	is_op( CHAT_ROOM_DATA *chat, char *arg );
void	extract_chat_room ( CHAT_ROOM_DATA *chat );

/* church.c */
void write_churches_new();
void read_churches_new();
CHURCH_DATA *read_church( FILE *fp );
CHURCH_PLAYER_DATA *read_church_member( FILE *fp );
void msg_church_members( CHURCH_DATA *church, char *argument ) ;
char *get_chrank( CHURCH_PLAYER_DATA *member );
char *get_chsize_from_number( int size );
bool is_treasure_room( CHURCH_DATA *church, ROOM_INDEX_DATA *room );
void append_church_log( CHURCH_DATA *church, char *string );
CHURCH_DATA *find_church( int number );
CHURCH_DATA *find_church_name(char *name);
bool is_in_treasure_room(OBJ_DATA *obj);
bool vnum_in_treasure_room(CHURCH_DATA *church, long vnum);
void update_church_pks(void);

/* house.c */
void write_houses( void );

/* timer.c */
void  wait_wait         (CHAR_DATA *, int, int);
void  wait_function     (void *, SCRIPT_VARINFO *info, int event_type, int delay, void *do_fun, char *);
EVENT_DATA *create_event(int, int);
void wipe_owned_events(EVENT_DATA *ev);	/* @@@NIB */
void wipe_clearinfo_mobile(CHAR_DATA *mob);
void wipe_clearinfo_object(OBJ_DATA *obj);
void wipe_clearinfo_token(TOKEN_DATA *token);
void wipe_clearinfo_room(ROOM_INDEX_DATA *room);

/* project.c */
void save_projects();
void save_project(FILE *fp, PROJECT_DATA *project);
void save_project_builder(FILE *fp, PROJECT_BUILDER_DATA *pb);
void save_project_inquiry(FILE *fp, PROJECT_INQUIRY_DATA *pinq);
void read_projects();
PROJECT_DATA *read_project(FILE *fp);
PROJECT_BUILDER_DATA *read_project_builder(FILE *fp);
PROJECT_INQUIRY_DATA *read_project_inquiry(FILE *fp);
int count_replies(PROJECT_INQUIRY_DATA *pinq);
PROJECT_INQUIRY_DATA *get_last_post(PROJECT_INQUIRY_DATA *pinq);
PROJECT_DATA *find_project(char *argument);
PROJECT_BUILDER_DATA *find_project_builder(PROJECT_DATA *project, char *argument);
PROJECT_INQUIRY_DATA *find_project_inquiry(PROJECT_DATA *project, char *argument);
int count_project_inquiries(CHAR_DATA *ch);
void show_oldest_unread_inquiry(CHAR_DATA *ch);
bool has_access_project(CHAR_DATA *ch, PROJECT_DATA *project, int access_type);
bool can_view_project(CHAR_DATA *ch, PROJECT_DATA *project);
bool can_edit_project(CHAR_DATA *ch, PROJECT_DATA *project);
void show_project_inquiry(PROJECT_INQUIRY_DATA *pinq, CHAR_DATA *ch);
void show_project_inquiries(PROJECT_DATA *project, CHAR_DATA *ch);
long get_total_minutes(PROJECT_DATA *project);


/* staff.c */
void save_immstaff();
void save_immortal(FILE *fp, IMMORTAL_DATA *immortal);
void read_immstaff();
IMMORTAL_DATA *read_immortal(FILE *fp);
void do_staffduty(CHAR_DATA *ch, char *argument);
IMMORTAL_DATA *find_immortal(char *argument);
void do_staffdelete(CHAR_DATA *ch, char *argument);
void do_staffsupervisor(CHAR_DATA *ch, char *argument);
void remove_immortal(IMMORTAL_DATA *immortal);
void show_immortal(IMMORTAL_DATA *immortal, CHAR_DATA *ch);
void print_immortal_info(IMMORTAL_DATA *immortal, CHAR_DATA *ch);

SCRIPT_DATA *new_script(void);
void free_script_code(SCRIPT_CODE *code, int lines);
void free_script(SCRIPT_DATA *s);
void variable_clearfield(int type, void *ptr);
void add_immortal(IMMORTAL_DATA *immortal);



#undef CD
#undef MID
#undef OD
#undef OID
#undef NID
#undef RID
#undef SF
#undef AD

/* Used in save.c to load objects that don't exist. */
#define OBJ_VNUM_DUMMY	30


#define MAX_DIR	10
#define NO_FLAG -99	/* Must not be used in flags or stats. */

/*
 * Global Constants
 */
extern	char *	const	dir_name        [];
int parse_door(char *name);
extern	const	sh_int	rev_dir         [];
extern	const	struct	spec_type	spec_table	[];

/*
 * Global variables
 */
extern	bool is_test_port;
extern  int port;
extern  int pulse_point;
extern	AREA_DATA *area_first;
extern	AREA_DATA *area_last;
extern	SHOP_DATA *shop_last;
extern	CHAR_DATA *hunt_last;
extern  HELP_CATEGORY *topHelpCat;
extern  HELP_DATA *help_first;
extern  HELP_DATA *help_last;
extern	SHOP_DATA *shop_first;
extern	long	   top_affect;
extern	long	   top_affliction;
extern  int  nAllocString;
extern  int  nAllocPerm;

/* OLC/areas */
extern	long top_area;
extern	long top_ed;
extern	long top_exit;
extern	long top_help;
extern	long top_mob_index;
extern	long top_obj_index;
extern	long top_reset;
extern	long top_room;
extern	long top_ship_index;
extern	long top_shop;
extern  long top_vroom;
extern  long top_help_index;
extern  long top_wilderness_exit;

/* quests */
extern	long top_quest;
extern	long top_quest_part;

/* church */
extern  long top_church;
extern	long top_church_player;
extern	LLIST *list_churches;

/* links */
extern 	long top_descriptor;

/* boats */
extern 	long top_ship_crew;
extern  long top_waypoint;

/* chat */
extern	long top_chatroom;
extern	long top_chat_op;
extern	long top_chat_ban;

/* wilderness */
extern  EXIT_DATA *wilderness_exits;

/* vnum */
extern	long top_vnum_mob;
extern	long top_vnum_obj;
extern	long top_vnum_room;
extern	long top_vnum_npc_ship;
extern  long top_extra_descr;

/* projects */
extern  PROJECT_DATA 		*project_list;
extern  PROJECT_INQUIRY_DATA 		*project_inquiry_list;
extern  bool			projects_changed;

extern	AREA_DATA *		eden_area;
extern	AREA_DATA *		wilderness_area;
extern	AREA_DATA *		netherworld_area;

extern	char			str_empty       [1];

extern	MOB_INDEX_DATA *	mob_index_hash  [MAX_KEY_HASH];
extern	OBJ_INDEX_DATA *	obj_index_hash  [MAX_KEY_HASH];
extern	ROOM_INDEX_DATA *	room_index_hash [MAX_KEY_HASH];
extern	NPC_SHIP_INDEX_DATA *	ship_index_hash [MAX_KEY_HASH];
extern  TOKEN_INDEX_DATA *	token_index_hash [MAX_KEY_HASH];

extern long top_mprog_index;
extern long top_oprog_index;
extern long top_rprog_index;
extern long top_npc_ship;

/* help */
extern int motd;
extern int imotd;
extern int rules;
extern int wizlist;

/* scripts.c */
extern int script_security;
extern int script_call_depth;
extern int script_lastreturn;

extern LLIST *persist_mobs;
extern LLIST *persist_objs;
extern LLIST *persist_rooms;
extern TOKEN_DATA *global_tokens;
// Temporarily disbaled for reconnect crash
//extern LLIST *loaded_players;
extern LLIST *loaded_chars;
extern LLIST *loaded_objects;

extern LLIST *conn_players;
extern LLIST *conn_immortals;
extern LLIST *conn_online;
extern LLIST *loaded_areas;		// LLIST_AREA_DATA format
extern LLIST *loaded_wilds;

extern BLUEPRINT_SECTION *blueprint_section_hash[MAX_KEY_HASH];
extern BLUEPRINT *blueprint_hash[MAX_KEY_HASH];
extern DUNGEON_INDEX_DATA *dungeon_index_hash[MAX_KEY_HASH];


void connection_add(DESCRIPTOR_DATA *d);
void connection_remove(DESCRIPTOR_DATA *d);


/* act_info.c */
extern int wear_params[MAX_WEAR][7];

char *get_script_prompt_string(CHAR_DATA *ch, char *key);
bool script_spell_deflection(CHAR_DATA *ch, CHAR_DATA *victim, TOKEN_DATA *token, SCRIPT_DATA *script, int mana);
void token_skill_improve( CHAR_DATA *ch, TOKEN_DATA *token, bool success, int multiplier );
int sub_class_search(const char *name);

bool string_vector_add(STRING_VECTOR **head, char *key, char *string);
void string_vector_free(STRING_VECTOR *v);
void string_vector_remove(STRING_VECTOR **head, char *key);
void string_vector_freeall(STRING_VECTOR *head);
STRING_VECTOR *string_vector_find(register STRING_VECTOR *head, char *key);
void string_vector_set(register STRING_VECTOR **head, char *key, char *string);

CHAR_DATA *idfind_mobile(register unsigned long id1, register unsigned long id2);
CHAR_DATA *idfind_player(register unsigned long id1, register unsigned long id2);
OBJ_DATA *idfind_object(unsigned long id1, unsigned long id2);
TOKEN_DATA *idfind_token(register unsigned long id1, register unsigned long id2);
TOKEN_DATA *idfind_token_char(CHAR_DATA *ch, register unsigned long id1, register unsigned long id2);
TOKEN_DATA *idfind_token_object(OBJ_DATA *obj, register unsigned long id1, register unsigned long id2);
TOKEN_DATA *idfind_token_room(ROOM_INDEX_DATA *room, register unsigned long id1, register unsigned long id2);

void editor_start(CHAR_DATA *ch, char **string, bool append);
void editor_input(CHAR_DATA *ch, char *argument);

ROOM_INDEX_DATA *idfind_vroom(register unsigned long id1, register unsigned long id2);
ROOM_INDEX_DATA *get_environment(ROOM_INDEX_DATA *room);
bool mobile_is_flying(CHAR_DATA *mob);

ROOM_INDEX_DATA *create_virtual_room_nouid(ROOM_INDEX_DATA *source, bool objects,bool links,bool resets);
ROOM_INDEX_DATA *create_virtual_room(ROOM_INDEX_DATA *source,bool links,bool resets);
ROOM_INDEX_DATA *get_clone_room(register ROOM_INDEX_DATA *source, register unsigned long id1, register unsigned long id2);
bool room_is_clone(ROOM_INDEX_DATA *room);
bool extract_clone_room(ROOM_INDEX_DATA *room, unsigned long id1, unsigned long id2, bool destruct);
bool check_vision(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool blind, bool dark);
void room_from_environment(ROOM_INDEX_DATA *room);
bool room_to_environment(ROOM_INDEX_DATA *clone,CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token);

ROOM_INDEX_DATA *wilds_seek_down(register WILDS_DATA *wilds, register int x, register int y, register int z, bool ground);
int obj_nest_clones(OBJ_DATA *obj);
void obj_set_nest_clones(OBJ_DATA *obj, bool add);
void obj_update_nest_clones(OBJ_DATA *obj);
void show_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool remote, bool silent, bool automatic);

void log_stringf(const char *fmt,...);
bool interrupt_script( CHAR_DATA *ch, bool silent );
CHAR_DATA *obj_carrier(OBJ_DATA *obj);

bool area_has_read_access(CHAR_DATA *ch, AREA_DATA *area);
bool area_has_write_access(CHAR_DATA *ch, AREA_DATA *area);

ROOM_INDEX_DATA *location_to_room(LOCATION *loc);
void location_from_room(LOCATION *loc,ROOM_INDEX_DATA *room);
ROOM_INDEX_DATA *get_recall_room(CHAR_DATA *ch);
void location_clear(LOCATION *loc);
void location_set(LOCATION *loc, unsigned long a, unsigned long b, unsigned long c, unsigned long d);
bool location_isset(LOCATION *loc);

void strip_newline(char *buf, bool append);

float diminishing_returns(float val, float scale);
float diminishing_inverse(float val, float scale);

SKILL_ENTRY *skill_entry_findname( SKILL_ENTRY *list, char *str );
SKILL_ENTRY *skill_entry_findsong( SKILL_ENTRY *list, int song );
SKILL_ENTRY *skill_entry_findsn( SKILL_ENTRY *list, int sn );
SKILL_ENTRY *skill_entry_findtoken( SKILL_ENTRY *list, TOKEN_DATA *token );
SKILL_ENTRY *skill_entry_findtokenindex( SKILL_ENTRY *list, TOKEN_INDEX_DATA *token_index );
void skill_entry_addskill (CHAR_DATA *ch, int sn, TOKEN_DATA *token, char source, long flags);
void skill_entry_addspell (CHAR_DATA *ch, int sn, TOKEN_DATA *token, char source, long flags);
void skill_entry_addsong (CHAR_DATA *ch, int song, TOKEN_DATA *token, char source);
void skill_entry_remove (SKILL_ENTRY **list, int sn, int song, TOKEN_DATA *token, bool isspell);
void skill_entry_removeentry (SKILL_ENTRY **list, SKILL_ENTRY *entry);
void skill_entry_removeskill (CHAR_DATA *ch, int sn, TOKEN_DATA *token);
void skill_entry_removespell (CHAR_DATA *ch, int sn, TOKEN_DATA *token);
void skill_entry_removesong (CHAR_DATA *ch, int song, TOKEN_DATA *token);
int token_skill_rating( TOKEN_DATA *token);
int token_skill_mana( TOKEN_DATA *token);
int skill_entry_rating (CHAR_DATA *ch, SKILL_ENTRY *entry);
int skill_entry_mod(CHAR_DATA *ch, SKILL_ENTRY *entry);
int skill_entry_level (CHAR_DATA *ch, SKILL_ENTRY *entry);
int skill_entry_mana (CHAR_DATA *ch, SKILL_ENTRY *entry);
int skill_entry_learn (CHAR_DATA *ch, SKILL_ENTRY *entry);
char *skill_entry_name (SKILL_ENTRY *entry);
void remort_player(CHAR_DATA *ch, int remort_class);

void persist_addmobile(CHAR_DATA *mob);
void persist_addobject(OBJ_DATA *obj);
void persist_addroom(ROOM_INDEX_DATA *room);
void persist_removemobile(CHAR_DATA *mob);
void persist_removeobject(OBJ_DATA *obj);
void persist_removeroom(ROOM_INDEX_DATA *room);
void persist_save(void);
bool persist_load(void);

LLIST *list_create(bool purge);
LLIST *list_createx(bool purge, LISTCOPY_FUNC copier, LISTDESTROY_FUNC deleter);
LLIST *list_copy(LLIST *src);
void list_clear(LLIST *lp);
void list_purge(LLIST *lp);
void list_destroy(LLIST *lp);
void list_cull(LLIST *lp);
void list_addref(LLIST *lp);
void list_remref(LLIST *lp);
bool list_addlink(LLIST *lp, void *data);
bool list_appendlink(LLIST *lp, void *data);
bool list_appendlist(LLIST *lp, LLIST *src);
void list_remlink(LLIST *lp, void *data);
void *list_nthdata(LLIST *lp, int nth);
void list_remnthlink(LLIST *lp, register int nth);
bool list_hasdata(LLIST *lp, register void *ptr);
int list_size(LLIST *lp);
void iterator_start(ITERATOR *it, LLIST *lp);
void iterator_start_nth(ITERATOR *it, LLIST *lp, int nth);
LLIST_LINK *iterator_next(ITERATOR *it);
void *iterator_nextdata(ITERATOR *it);
void iterator_remcurrent(ITERATOR *it);
void iterator_reset(ITERATOR *it);
void iterator_stop(ITERATOR *it);

bool list_isvalid(LLIST *lp);

AREA_DATA *get_area_data args ((long anum));
AREA_DATA *get_area_from_uid args ((long uid));

void sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, char *name);
void give_money(CHAR_DATA *ch, OBJ_DATA *container, int gold, int silver, bool indent);
void get_money_from_obj(CHAR_DATA *ch, OBJ_DATA *container);
bool obj_has_money(CHAR_DATA *ch, OBJ_DATA *container);
void loot_corpse(CHAR_DATA *ch, OBJ_DATA *corpse);

int music_lookup( char *name);
bool is_char_busy(CHAR_DATA *ch);
bool obj_has_spell(OBJ_DATA *obj, char *name);
void restore_char(CHAR_DATA *ch, CHAR_DATA *whom, int percent);

typedef bool (*pVISIT_ROOM_LINE_FUNC)(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data );
typedef void (*pVISIT_ROOM_END_FUNC)(ROOM_INDEX_DATA *room, CHAR_DATA *ch, int depth, int door, void *data, bool canceled );
void visit_room_direction(CHAR_DATA *ch, ROOM_INDEX_DATA *start_room, int max_depth, int door, void *data, pVISIT_ROOM_LINE_FUNC func, pVISIT_ROOM_END_FUNC end_func);

long dice_roll(DICE_DATA *d);
void dice_copy(DICE_DATA *a, DICE_DATA *b);

TOKEN_DATA *create_token(TOKEN_INDEX_DATA *token_index);

BOOLEXP *new_boolexp();
void free_boolexp(BOOLEXP *boolexp);

int do_flee_full(CHAR_DATA *ch, char *argument, bool conceal, bool pursue);

CHURCH_TREASURE_ROOM *get_church_treasure_room(CHAR_DATA *ch, CHURCH_DATA *church, int nth);
bool church_add_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room, int min_rank);
void church_remove_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room);
bool church_set_treasure_room_rank(CHURCH_DATA *church, int nth, int min_rank);
int church_get_min_positions(int size);
int church_available_treasure_rooms(CHAR_DATA *ch);
void church_announce_theft(CHAR_DATA *ch, OBJ_DATA *obj);

int get_colour_width(char *text);
char *get_shop_stock_price(SHOP_STOCK_DATA *stock);
char *get_shop_purchase_price(long silver, long qp, long dp, long pneuma);
long haggle_price(CHAR_DATA *ch, CHAR_DATA *keeper, int chance, int number, long base_price, long funds, int discount, bool *haggled, bool silent);
char *get_stock_description(SHOP_STOCK_DATA *stock);
void show_basic_mob_lore(CHAR_DATA *ch, CHAR_DATA *victim);
SHOP_STOCK_DATA *get_stockonly_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, char *argument);
bool is_pullable(OBJ_DATA *obj);

OBJ_DATA *generate_quest_scroll(CHAR_DATA *ch, char *questgiver, long vnum, char *header, char *footer, char *prefix, char *suffix, int width);
OBJ_DATA *get_obj_world_index(CHAR_DATA *ch, OBJ_INDEX_DATA *pObjIndex, bool all);

void load_blueprints();
bool save_blueprints();
bool valid_section_link(BLUEPRINT_LINK *bl);
BLUEPRINT_LINK *get_section_link(BLUEPRINT_SECTION *bs, int link);
bool valid_static_link(STATIC_BLUEPRINT_LINK *sbl);
BLUEPRINT_SECTION *get_blueprint_section(long vnum);
BLUEPRINT_SECTION *get_blueprint_section_byroom(long vnum);
BLUEPRINT *get_blueprint(long vnum);
INSTANCE *create_instance(BLUEPRINT *blueprint);
bool can_edit_blueprints(CHAR_DATA *ch);
bool rooms_in_same_section(long vnum1, long vnum2);
int instance_section_count_mob(INSTANCE_SECTION *section, MOB_INDEX_DATA *pMobIndex);
int instance_count_mob(INSTANCE *instance, MOB_INDEX_DATA *pMobIndex);
void instance_update();
void instance_save(FILE *fp, INSTANCE *instance);
bool save_instances();
void instance_echo(INSTANCE *instance, char *text);
void extract_instance(INSTANCE *instance);
ROOM_INDEX_DATA *section_random_room(CHAR_DATA *ch, INSTANCE_SECTION *section);
ROOM_INDEX_DATA *instance_random_room(CHAR_DATA *ch, INSTANCE *instance);
ROOM_INDEX_DATA *instance_section_get_room_byvnum(INSTANCE_SECTION *section, long vnum);
ROOM_INDEX_DATA *get_instance_special_room(INSTANCE *instance, int index);
ROOM_INDEX_DATA *get_instance_special_room_byname(INSTANCE *instance, char *name);
void instance_addowner_player(INSTANCE *instance, CHAR_DATA *ch);
void instance_addowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2);
void instance_removeowner_player(INSTANCE *instance, CHAR_DATA *ch);
void instance_removeowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2);
bool instance_isowner_player(INSTANCE *instance, CHAR_DATA *ch);
bool instance_isowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2);
bool instance_canswitch_player(INSTANCE *instance, CHAR_DATA *ch);
bool instance_isorphaned(INSTANCE *instance);
char *instance_get_ownership(INSTANCE *instance);
bool instance_can_idle(INSTANCE *instance);
INSTANCE *get_room_instance(ROOM_INDEX_DATA *room);

void load_dungeons();
bool save_dungeons();
bool can_edit_dungeons(CHAR_DATA *ch);
DUNGEON_INDEX_DATA *get_dungeon_index(long vnum);
ROOM_INDEX_DATA *spawn_dungeon_player(CHAR_DATA *ch, long vnum, int floor);
void dungeon_save(FILE *fp, DUNGEON *dungeon);
void dungeon_check_empty(DUNGEON *dungeon);
void dungeon_echo(DUNGEON *dungeon, char *text);
ROOM_INDEX_DATA *dungeon_random_room(CHAR_DATA *ch, DUNGEON *dungeon);
DUNGEON *get_room_dungeon(ROOM_INDEX_DATA *room);
OBJ_DATA *get_room_dungeon_portal(ROOM_INDEX_DATA *room, long vnum);
ROOM_INDEX_DATA *get_dungeon_special_room(DUNGEON *dungeon, int index);
ROOM_INDEX_DATA *get_dungeon_special_room_byname(DUNGEON *dungeon, char *name);
int dungeon_count_mob(DUNGEON *dungeon, MOB_INDEX_DATA *pMobIndex);
void extract_dungeon(DUNGEON *dungeon);
void dungeon_addowner_player(DUNGEON *dungeon, CHAR_DATA *ch);
void dungeon_addowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2);
void dungeon_removeowner_player(DUNGEON *dungeon, CHAR_DATA *ch);
void dungeon_removeowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2);
bool dungeon_isowner_player(DUNGEON *dungeon, CHAR_DATA *ch);
bool dungeon_isowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2);
bool dungeon_canswitch_player(DUNGEON *dungeon, CHAR_DATA *ch);
bool dungeon_isorphaned(DUNGEON *dungeon);
bool dungeon_can_idle(DUNGEON *dungeon);


bool can_room_update(ROOM_INDEX_DATA *room);

extern  bool			blueprints_changed;
extern  bool			dungeons_changed;

void persist_save_room(FILE *fp, ROOM_INDEX_DATA *room);
ROOM_INDEX_DATA *persist_load_room(FILE *fp, char rtype);

void resolve_dungeons_player(CHAR_DATA *ch);
void resolve_instances();
void resolve_instances_player(CHAR_DATA *ch);

void detach_dungeons_player(CHAR_DATA *ch);
void detach_instances_player(CHAR_DATA *ch);

extern long top_iprog_index;
extern long top_dprog_index;

bool is_area_unlocked(CHAR_DATA *ch, AREA_DATA *area);
bool is_room_unlocked(CHAR_DATA *ch, ROOM_INDEX_DATA *room);
void player_unlock_area(CHAR_DATA *ch, AREA_DATA *area);


#endif /* !def __MERC_H__ */
