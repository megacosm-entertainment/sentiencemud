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
#include <stdarg.h>
#include "merc.h"
#include "db.h"
#include "math.h"
#include "recycle.h"
#include "tables.h"
#include "olc_save.h"
#include "scripts.h"
#include "wilds.h"

#if !defined(OLD_RAND)
#if !defined(linux)
long random();
#endif
void srandom(unsigned int);
int getpid();
time_t time(time_t *tloc);
#endif

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

void free_room_index( ROOM_INDEX_DATA *pRoom );
void load_instances();
INSTANCE *instance_load(FILE *fp);
DUNGEON *dungeon_load(FILE *fp);

/* Reading of keys*/
#if defined(KEY)
#undef KEY
#endif

#define IS_KEY(literal)		(!str_cmp(word,literal))

#define KEY(literal, field, value) \
	if (IS_KEY(literal)) { \
		field = value; \
		fMatch = TRUE; \
		break; \
	}

#define SKEY(literal, field) \
	if (IS_KEY(literal)) { \
		free_string(field); \
		field = fread_string(fp); \
		fMatch = TRUE; \
		break; \
	}

#define FKEY(literal, field) \
	if (IS_KEY(literal)) { \
		field = TRUE; \
		fMatch = TRUE; \
		break; \
	}

#define FVKEY(literal, field, string, tbl) \
	if (IS_KEY(literal)) { \
		field = script_flag_value(tbl, string); \
		fMatch = TRUE; \
		break; \
	}

#define FVDKEY(literal, field, string, tbl, bad, def) \
	if (!str_cmp(word, literal)) { \
		field = script_flag_value(tbl, string); \
		if( field == bad ) { \
			field = def; \
		} \
		fMatch = TRUE; \
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

/*
 * Globals.
 */
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

bool			global;
char			bug_buf[2*MAX_INPUT_LENGTH];
char *			help_greeting;
char			log_buf[2*MAX_INPUT_LENGTH];
char			*reboot_by;
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
/* This variable serves as a placeholder to make sure that obj repop scripts
   are only triggered by newly created objects instead of any objects. This
   is necesarry because I put the triggering mechanism in obj_to_char() and
   obj_to_room(). When the object is given to a char or a room, this variable
   is toggled off, and the object will then no longer trigger repop scripts. */

sh_int	gsn_acid_blast;
sh_int	gsn_acid_breath;
sh_int	gsn_acro;
sh_int	gsn_afterburn;
sh_int	gsn_air_spells;
sh_int	gsn_ambush;
sh_int	gsn_animate_dead;
sh_int	gsn_archery;
sh_int	gsn_armour;
sh_int	gsn_athletics;
sh_int	gsn_avatar_shield;
sh_int	gsn_axe;
sh_int	gsn_backstab;
sh_int	gsn_bar;
sh_int	gsn_bash;
sh_int	gsn_behead;
sh_int	gsn_berserk;
sh_int	gsn_bind;
sh_int	gsn_bite;
sh_int	gsn_blackjack;
sh_int	gsn_bless;
sh_int	gsn_blindness;
sh_int	gsn_blowgun;
sh_int	gsn_bomb;
sh_int	gsn_bow;
sh_int	gsn_breath;
sh_int	gsn_brew;
sh_int	gsn_burgle;
sh_int	gsn_burning_hands;
sh_int	gsn_call_familiar;
sh_int	gsn_call_lightning;
sh_int	gsn_calm;
sh_int	gsn_cancellation;
sh_int	gsn_catch;
sh_int	gsn_cause_critical;
sh_int	gsn_cause_light;
sh_int	gsn_cause_serious;
sh_int	gsn_chain_lightning;
sh_int	gsn_channel;
sh_int	gsn_charge;
sh_int	gsn_charm_person;
sh_int	gsn_chill_touch;
sh_int	gsn_circle;
sh_int	gsn_cloak_of_guile;
sh_int	gsn_colour_spray;
sh_int	gsn_combine;
sh_int	gsn_consume;
sh_int	gsn_continual_light;
sh_int	gsn_control_weather;
sh_int	gsn_cosmic_blast;
sh_int	gsn_counterspell;
sh_int	gsn_create_food;
sh_int	gsn_create_rose;
sh_int	gsn_create_spring;
sh_int	gsn_create_water;
sh_int	gsn_crippling_touch;
sh_int	gsn_crossbow;
sh_int	gsn_cure_blindness;
sh_int	gsn_cure_critical;
sh_int	gsn_cure_disease;
sh_int	gsn_cure_light;
sh_int	gsn_cure_poison;
sh_int	gsn_cure_serious;
sh_int	gsn_cure_toxic;
sh_int	gsn_curse;
sh_int	gsn_dagger;
sh_int	gsn_death_grip;
sh_int	gsn_deathbarbs;
sh_int	gsn_deathsight;
sh_int	gsn_deception;
sh_int	gsn_deep_trance;
sh_int	gsn_demonfire;
sh_int	gsn_destruction;
sh_int	gsn_detect_hidden;
sh_int	gsn_detect_invis;
sh_int	gsn_detect_magic;
sh_int	gsn_detect_traps;
sh_int	gsn_dirt;
sh_int	gsn_dirt_kicking;
sh_int	gsn_disarm;
sh_int	gsn_discharge;
sh_int	gsn_dispel_evil;
sh_int	gsn_dispel_good;
sh_int	gsn_dispel_magic;
sh_int	gsn_dispel_room;
sh_int	gsn_dodge;
sh_int	gsn_dual;
sh_int	gsn_eagle_eye;
sh_int	gsn_earth_spells;
sh_int	gsn_earthquake;
sh_int	gsn_electrical_barrier;
sh_int	gsn_enchant_armour;
sh_int	gsn_enchant_weapon;
sh_int	gsn_energy_drain;
sh_int	gsn_energy_field;
sh_int	gsn_enhanced_damage;
sh_int	gsn_ensnare;
sh_int	gsn_entrap;
sh_int	gsn_envenom;
sh_int	gsn_evasion;
sh_int	gsn_exorcism;
sh_int	gsn_exotic;
sh_int	gsn_fade;
sh_int	gsn_faerie_fire;
sh_int	gsn_faerie_fog;
sh_int	gsn_fast_healing;
sh_int	gsn_fatigue;
sh_int	gsn_feign;
sh_int	gsn_fire_barrier;
sh_int	gsn_fire_breath;
sh_int	gsn_fire_cloud;
sh_int	gsn_fire_spells;
sh_int	gsn_fireball;
sh_int	gsn_fireproof;
sh_int	gsn_flail;
sh_int	gsn_flamestrike;
sh_int	gsn_flight;
sh_int	gsn_fly;
sh_int	gsn_fourth_attack;
sh_int	gsn_frenzy;
sh_int	gsn_frost_barrier;
sh_int	gsn_frost_breath;
sh_int	gsn_gas_breath;
sh_int	gsn_gate;
sh_int	gsn_giant_strength;
sh_int	gsn_glorious_bolt;
sh_int	gsn_haggle;
sh_int	gsn_hand_to_hand;
sh_int	gsn_harm;
sh_int	gsn_harpooning;
sh_int	gsn_haste;
sh_int	gsn_heal;
sh_int	gsn_healing_aura;
sh_int	gsn_healing_hands;
sh_int	gsn_hide;
sh_int	gsn_holdup;
sh_int	gsn_holy_shield;
sh_int	gsn_holy_sword;
sh_int	gsn_holy_word;
sh_int	gsn_holy_wrath;
sh_int	gsn_hunt;
sh_int	gsn_ice_storm;
sh_int	gsn_identify;
sh_int	gsn_improved_invisibility;
sh_int	gsn_inferno;
sh_int	gsn_infravision;
sh_int	gsn_infuse;
sh_int	gsn_intimidate;
sh_int	gsn_invis;
sh_int	gsn_judge;
sh_int	gsn_kick;
sh_int	gsn_kill;
sh_int	gsn_leadership;
sh_int	gsn_light_shroud;
sh_int	gsn_lightning_bolt;
sh_int	gsn_lightning_breath;
sh_int	gsn_locate_object;
sh_int	gsn_lore;
sh_int	gsn_mace;
sh_int	gsn_magic_missile;
sh_int	gsn_martial_arts;
sh_int	gsn_mass_healing;
sh_int	gsn_mass_invis;
sh_int	gsn_master_weather;
sh_int	gsn_maze;
sh_int	gsn_meditation;
sh_int	gsn_mob_lore;
sh_int	gsn_momentary_darkness;
sh_int	gsn_morphlock;
sh_int	gsn_mount_and_weapon_style;
sh_int	gsn_music;
sh_int	gsn_neurotoxin;
sh_int	gsn_nexus;
sh_int	gsn_offhanded;
sh_int	gsn_parry;
sh_int	gsn_pass_door;
sh_int	gsn_peek;
sh_int	gsn_pick_lock;
sh_int	gsn_plague;
sh_int	gsn_poison;
sh_int	gsn_polearm;
sh_int	gsn_possess;
sh_int	gsn_pursuit;
sh_int	gsn_quarterstaff;
sh_int	gsn_raise_dead;
sh_int	gsn_recall;
sh_int	gsn_recharge;
sh_int	gsn_refresh;
sh_int	gsn_regeneration;
sh_int	gsn_remove_curse;
sh_int	gsn_rending;
sh_int	gsn_repair;
sh_int	gsn_rescue;
sh_int	gsn_resurrect;
sh_int	gsn_reverie;
sh_int	gsn_riding;
sh_int	gsn_room_shield;
sh_int	gsn_sanctuary;
sh_int	gsn_scan;
sh_int	gsn_scribe;
sh_int	gsn_scrolls;
sh_int	gsn_scry;
sh_int	gsn_second_attack;
sh_int	gsn_sense_danger;
sh_int	gsn_shape;
sh_int	gsn_shield;
sh_int	gsn_shield_block;
sh_int	gsn_shield_weapon_style;
sh_int	gsn_shift;
sh_int	gsn_shocking_grasp;
sh_int	gsn_silence;
sh_int	gsn_single_style;
sh_int	gsn_skull;
sh_int	gsn_sleep;
sh_int	gsn_slit_throat;
sh_int	gsn_slow;
sh_int	gsn_smite;
sh_int	gsn_sneak;
sh_int	gsn_spear;
sh_int	gsn_spell_deflection;
sh_int	gsn_spell_shield;
sh_int	gsn_spell_trap;
sh_int	gsn_spirit_rack;
sh_int	gsn_stake;
sh_int	gsn_starflare;
sh_int	gsn_staves;
sh_int	gsn_steal;
sh_int	gsn_stone_skin;
sh_int	gsn_stone_spikes;
sh_int	gsn_subvert;
sh_int	gsn_summon;
sh_int	gsn_survey;
sh_int	gsn_swerve;
sh_int	gsn_sword;
sh_int	gsn_sword_and_dagger_style;
sh_int	gsn_tail_kick;
sh_int	gsn_tattoo;
sh_int	gsn_temperance;
sh_int	gsn_third_attack;
sh_int	gsn_third_eye;
sh_int	gsn_throw;
sh_int	gsn_titanic_attack;
sh_int	gsn_toxic_fumes;
sh_int	gsn_toxins;
sh_int	gsn_trackless_step;
sh_int	gsn_trample;
sh_int	gsn_trip;
sh_int	gsn_turn_undead;
sh_int	gsn_two_handed_style;
sh_int	gsn_underwater_breathing;
sh_int	gsn_vision;
sh_int	gsn_wands;
sh_int	gsn_warcry;
sh_int	gsn_water_spells;
sh_int	gsn_weaken;
sh_int	gsn_weaving;
sh_int	gsn_web;
sh_int	gsn_whip;
sh_int	gsn_wilderness_spear_style;
sh_int	gsn_wind_of_confusion;
sh_int	gsn_withering_cloud;
sh_int	gsn_word_of_recall;

sh_int	gsn_ice_shards;
sh_int	gsn_stone_touch;
sh_int	gsn_glacial_wave;
sh_int	gsn_earth_walk;
sh_int	gsn_flash;
sh_int	gsn_shriek;
sh_int	gsn_dark_shroud;
sh_int	gsn_soul_essence;

sh_int gprn_human;
sh_int gprn_elf;
sh_int gprn_dwarf;
sh_int gprn_titan;
sh_int gprn_vampire;
sh_int gprn_drow;
sh_int gprn_sith;
sh_int gprn_draconian;
sh_int gprn_slayer;
sh_int gprn_minotaur;
sh_int gprn_angel;
sh_int gprn_mystic;
sh_int gprn_demon;
sh_int gprn_lich;
sh_int gprn_avatar;
sh_int gprn_seraph;
sh_int gprn_berserker;
sh_int gprn_colossus;
sh_int gprn_fiend;
sh_int gprn_specter;
sh_int gprn_naga;
sh_int gprn_dragon;
sh_int gprn_changeling;
sh_int gprn_hell_baron;
sh_int gprn_wraith;
sh_int gprn_shaper;


sh_int grn_human;
sh_int grn_elf;
sh_int grn_dwarf;
sh_int grn_titan;
sh_int grn_vampire;
sh_int grn_drow;
sh_int grn_sith;
sh_int grn_draconian;
sh_int grn_slayer;
sh_int grn_minotaur;
sh_int grn_angel;
sh_int grn_mystic;
sh_int grn_demon;
sh_int grn_lich;
sh_int grn_avatar;
sh_int grn_seraph;
sh_int grn_berserker;
sh_int grn_colossus;
sh_int grn_fiend;
sh_int grn_specter;
sh_int grn_naga;
sh_int grn_dragon;
sh_int grn_changeling;
sh_int grn_hell_baron;
sh_int grn_wraith;
sh_int grn_shaper;
sh_int grn_were_changed;
sh_int grn_mob_vampire;
sh_int grn_bat;
sh_int grn_werewolf;
sh_int grn_bear;
sh_int grn_bugbear;
sh_int grn_cat;
sh_int grn_centipede;
sh_int grn_dog;
sh_int grn_doll;
sh_int grn_fido;
sh_int grn_fox;
sh_int grn_goblin;
sh_int grn_hobgoblin;
sh_int grn_kobold;
sh_int grn_lizard;
sh_int grn_doxian;
sh_int grn_orc;
sh_int grn_pig;
sh_int grn_rabbit;
sh_int grn_school_monster;
sh_int grn_snake;
sh_int grn_song_bird;
sh_int grn_golem;
sh_int grn_unicorn;
sh_int grn_griffon;
sh_int grn_troll;
sh_int grn_water_fowl;
sh_int grn_giant;
sh_int grn_wolf;
sh_int grn_wyvern;
sh_int grn_nileshian;
sh_int grn_skeleton;
sh_int grn_zombie;
sh_int grn_wisp;
sh_int grn_insect;
sh_int grn_gnome;
sh_int grn_angel_mob;
sh_int grn_demon_mob;
sh_int grn_rodent;
sh_int grn_treant;
sh_int grn_horse;
sh_int grn_bird;
sh_int grn_fungus;
sh_int grn_unique;


/*
 * Locals.
 */
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

LLIST *loaded_chars;
// Temporarily disabled for reconnect crash.
//LLIST *loaded_players;
LLIST *loaded_objects;
LLIST *persist_mobs;
LLIST *persist_objs;
LLIST *persist_rooms;

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
void load_shares(void);
void fix_objprogs(void);
void fix_roomprogs(void);
void load_reboot_objs(void);
void load_socials(FILE *fp);
void load_notes(void);
void load_bans(void);
void fix_rooms(void);
void fix_mobprogs(void);
void reset_area(AREA_DATA * pArea);
void chance_create_mob(ROOM_INDEX_DATA *pRoom, MOB_INDEX_DATA *pMobIndex, int chance);
void reset_npc_sailing_boats (void);
void fix_tokenprogs(void);
void check_area_versions(void);
void migrate_shopkeeper_resets(AREA_DATA *area);
void fix_areaprogs(void);
void fix_instanceprogs(void);
void fix_dungeonprogs(void);


bool persist_load(void);

/* Top-level booting function*/
void boot_db(void)
{
    int i;
    FILE *fp;

    /*
     * Init some data space stuff.
     */
    {
	if ((string_space = calloc(1, MAX_STRING)) == NULL)
	{
	    bug("Boot_db: can't alloc %d string space.", MAX_STRING);
	    exit(1);
	}
	top_string	= string_space;
	fBootDb		= TRUE;
    }

    /*
     * Init random number generator.
     */
    init_mm();

    global_quest.mobs = NULL;
    global_quest.objects = NULL;

    auction_info.item           = NULL;
    auction_info.owner          = NULL;
    auction_info.high_bidder    = NULL;
    auction_info.current_bid    = 0;
    auction_info.status         = 0;
    auction_info.gold_held	= 0;
    auction_info.silver_held	= 0;

//    logAll = TRUE; /*setting this on for now*/

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

    /*
     * Assign gsn's for skills which have them.
     * Syn - while we're at it, let's set up an NPC skills table as well, to reduce
     * processor load.
     */
    {
	int sn, lev;

	for (sn = 0; sn < MAX_SKILL; sn++)
	{
	    if (skill_table[sn].pgsn != NULL)
		*skill_table[sn].pgsn = sn;
	}

	for (lev = 0; lev != MAX_MOB_SKILL_LEVEL; lev++)
	    mob_skill_table[lev] = 40 + 19 * log10(lev);

	for (sn = 0; race_table[sn].name; sn++)
	{
	    if (race_table[sn].pgrn)
		*race_table[sn].pgrn = sn;
	}
	for (sn = 0; pc_race_table[sn].name; sn++)
	{
	    if (pc_race_table[sn].pgrn)
		*pc_race_table[sn].pgrn = sn;
	}

    }

	// Initialize certain lists
	loaded_instances = list_create(FALSE);
	loaded_dungeons = list_create(FALSE);
	loaded_ships = list_create(FALSE);
	loaded_special_keys = list_create(FALSE);

    /*
     * Read in all the area files.
     */
    {
		FILE *fpList;
        char log_buf[MAX_STRING_LENGTH];

        log_string("db.c, boot_db: Loading areas from area.lst file...");

		if ((fpList = fopen(AREA_LIST, "r")) == NULL) {
			perror(AREA_LIST);
			exit(1);
		}

		for (; ;)
		{
			AREA_DATA *area;
			LLIST_AREA_DATA *link;

			strcpy(strArea, fread_word(fpList));
			if (strArea[0] == '$')
				break;

			if (!str_cmp(strArea, "help.are") || !str_cmp(strArea, "social.are"))
				continue;

			if ((fpArea = fopen(strArea, "r")) == NULL) {
				perror(strArea);
				exit(1);
			}

			sprintf(log_buf, "Loading areafile '%s'", strArea);
			log_string(log_buf);


			area = read_area_new(fpArea);
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
				bug("Failed to add area to loaded list due to memory issues with 'list_appendlink'.", 0);
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
    log_string("Doing fix_rooms");
    fix_rooms();
    log_string("Doing fix_vlinks");
    fix_vlinks();

    log_string("Loading blueprints");
	load_blueprints();

    log_string("Loading dungeon definitions");
	load_dungeons();

	log_string("Loading ships");
	load_ships();

    log_string("Doing variable_index_fix");
    variable_index_fix();
    log_string("Doing variable_fix");
    variable_fix_global();
    log_string("Doing fix_mobprogs");
    fix_mobprogs();
    log_string("Doing fix_objprogs");
    fix_objprogs();
    log_string("Doing fix_roomprogs");
    fix_roomprogs();
    log_string("Doing fix_tokenprogs");
    fix_tokenprogs();
    log_string("Doing fix_areaprogs");
    fix_areaprogs();
    log_string("Doing fix_instanceprogs");
    fix_instanceprogs();
    log_string("Doing fix_dungeonprogs");
    fix_dungeonprogs();

    log_string("Loading persistance");
    if(!persist_load()) {
		perror("Persistance");
	    exit(1);
	}

    log_string("Opening churches, new format");
    read_churches_new();

    fBootDb	= FALSE;
    log_string("Doing generate_poa_resets");
    generate_poa_resets(-1);
    log_string("Doing area_update");
    area_update(TRUE);
    log_string("Doing load_notes");
    load_notes();
    log_string("Doing load_bans");
    load_bans();
    //log_string("Doing load_reboot_objs");
    //load_reboot_objs();


    log_string("Opening projects");
    read_projects();

    log_string("Opening immortal staff");
    read_immstaff();

    if ((fp = fopen("social.are", "r")) != NULL)
    {
	log_string("Doing load_socials...");
	fread_word(fp);
	load_socials(fp);
	fclose(fp);
    }

    help_greeting = str_dup("hello");

    log_string("Doing read_gq");
    read_gq();

    log_string("Doing read_chat_rooms");
    read_chat_rooms();

    log_string("Reading permanent objs");
    read_permanent_objs();

	log_string("Loading instances");
	load_instances();

    log_string("Reading helpfiles");
    read_helpfiles_new();

    index_helpfiles(1, topHelpCat);

/*    load_sailing_boats();*/
/*    load_npc_ships();*/
//    log_string("Doing read_mail");
//    read_mail();
/*  reset_npc_sailing_boats();*/
    load_statistics();

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
    objRepop = FALSE;

    prog_data_virtual = new_prog_data();

    if ((fp = fopen("church_pks.txt", "r")) != NULL) {
	update_church_pks();
	fclose(fp);

    }

    log_string("Checking area versions");
    check_area_versions();

    gconfig_write();
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

    if (IS_ANGEL(ch) || IS_MYSTIC(ch) || IS_DEMON(ch)) {
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


/*
 * Translate all room exits from virtual to real.
 * Has to be done after all rooms are read in.
 * Check for bad reverse exits.
 */
void fix_rooms(void)
{
    ROOM_INDEX_DATA *room;
    EXIT_DATA *pexit;
    int iHash;
    int door;

    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
	for (room = room_index_hash[iHash]; room != NULL; room = room->next)
	{
	    bool fexit;

	    fexit = FALSE;
	    for (door = 0; door <= 9; door++)
	    {
		if ((pexit = room->exit[door]) != NULL)
		{
		    if (pexit->u1.vnum <= 0
		    ||   get_room_index(pexit->u1.vnum) == NULL)
			pexit->u1.to_room = NULL;
		    else
		    {
		   	fexit = TRUE;
			pexit->u1.to_room = get_room_index(pexit->u1.vnum);
		    }
		}
	    }

	    /* only do this for non-wilds rooms*/
	    if (!fexit && room->wilds == NULL)
		SET_BIT(room->room_flags,ROOM_NO_MOB);


		/* Fix it so that rooms that have wilderness coords will link to the proper wilderness*/
	    if(room->w) room->viewwilds = get_wilds_from_uid(NULL,room->w);
	}
    }
}


/*
 * Translate mobprog vnums pointers to real code
 */
void fix_mobprogs(void)
{
	MOB_INDEX_DATA *mob;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (mob = mob_index_hash[iHash]; mob != NULL; mob = mob->next) if(mob->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( mob->progs[slot] ) {
				iterator_start(&it, mob->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_MPROG))) {
						bug("Fix_mobprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_mobprogs: on mobile %ld", mob->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
			}
		}
	}
}


void fix_objprogs(void)
{
	OBJ_INDEX_DATA *obj;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (obj = obj_index_hash[iHash]; obj != NULL; obj = obj->next) if(obj->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( obj->progs[slot] ) {
				iterator_start(&it, obj->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_OPROG))) {
						bug("Fix_objprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_objprogs: on object %ld", obj->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
			}
		}
	}
}

void fix_roomprogs(void)
{
	ROOM_INDEX_DATA *room;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (room = room_index_hash[iHash]; room != NULL; room = room->next) if(room->progs->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( room->progs->progs[slot] ) {
				iterator_start(&it, room->progs->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_RPROG))) {
						bug("Fix_roomprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_roomprogs: on room %ld", room->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
			}

		}
	}
}

void fix_tokenprogs(void)
{
	TOKEN_INDEX_DATA *token;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (token = token_index_hash[iHash]; token != NULL; token = token->next) if(token->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( token->progs[slot] ) {
				iterator_start(&it, token->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_TPROG))) {
						bug("Fix_tokenprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_tokenprogs: on token %ld", token->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
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
				if (!(trigger->script = get_script_index(trigger->vnum, PRG_APROG))) {
					bug("fix_areaprogs: code vnum %d not found.", trigger->vnum);
					bug("fix_areaprogs: on area %ld", pArea->anum);
					exit(1);
				}
			}
			iterator_stop(&it);
		}
	}
}

void fix_instanceprogs(void)
{
	BLUEPRINT *blueprint;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (blueprint = blueprint_hash[iHash]; blueprint != NULL; blueprint = blueprint->next) if(blueprint->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( blueprint->progs[slot] ) {
				iterator_start(&it, blueprint->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_IPROG))) {
						bug("Fix_instanceprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_instanceprogs: on blueprint %ld", blueprint->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
			}
		}
	}
}


void fix_dungeonprogs(void)
{
	DUNGEON_INDEX_DATA *dungeon_index;
	PROG_LIST *trigger;
	ITERATOR it;
	int iHash, slot;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (dungeon_index = dungeon_index_hash[iHash]; dungeon_index != NULL; dungeon_index = dungeon_index->next) if(dungeon_index->progs) {
			for (slot = 0; slot < TRIGSLOT_MAX; slot++) if( dungeon_index->progs[slot] ) {
				iterator_start(&it, dungeon_index->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					if (!(trigger->script = get_script_index(trigger->vnum, PRG_DPROG))) {
						bug("Fix_dungeonprogs: code vnum %d not found.", trigger->vnum);
						bug("Fix_dungeonprogs: on dungeon_index %ld", dungeon_index->vnum);
						exit(1);
					}
				}
				iterator_stop(&it);
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
	      obj = create_object(get_obj_index(OBJ_VNUM_ABYSS_PORTAL), 0, TRUE);
	      obj_to_vroom(obj, pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y);
            }
        }
    }

    return;
}


void reset_area(AREA_DATA *pArea)
{
    ROOM_INDEX_DATA *pRoom;
    long vnum;

/*  VIZZWILDS - disabled this legacy hack - now handled by vlinks!*/
/*
    if (!str_cmp(pArea->name , "Netherworld"))
    {
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *temp_room;
	bool found = FALSE;

	temp_room = get_room_index(ROOM_VNUM_ABYSS_GATE);
	for (obj = temp_room->contents; obj != NULL; obj = obj->next_content)
	{
	    if (obj->pIndexData->vnum == OBJ_VNUM_ABYSS_PORTAL)
	    {
		found = TRUE;
		break;
	    }
	}

	if (!found)
	{
	    obj = create_object(get_obj_index(OBJ_VNUM_ABYSS_PORTAL), 0, TRUE);
	    obj_to_room(obj, get_room_index(ROOM_VNUM_ABYSS_GATE));
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
		if (pRoom->parent == 5000001) // Silverfern forest*
		{
		    Butterfly
		    chance_create_mob(pRoom, get_mob_index(100001), 5);
		}
		if (pRoom->parent == 5000002)  Wharf
		{
		*seagull
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

	for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
	{
	    if ((pRoom = get_room_index(vnum)))
			reset_room(pRoom);
	}
}

void room_update(ROOM_INDEX_DATA *room)
{
	char buf[MAX_STRING_LENGTH];

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
					sprintf(buf, "room update: token %s(%ld) clone room %s(%ld, %1d:%1d) was extracted because of timer",
						token->name, token->pIndexData->vnum, room->name, room->vnum, (int)room->id[0], (int)room->id[1]);
				else if( room->wilds )
					sprintf(buf, "room update: token %s(%ld) wilds room %s(%ld, %ld, %ld) was extracted because of timer",
						token->name, token->pIndexData->vnum, room->name, room->wilds->uid, room->x, room->y);
				else
					sprintf(buf, "room update: token %s(%ld) room %s(%ld) was extracted because of timer",
						token->name, token->pIndexData->vnum, room->name, room->vnum);
				log_string(buf);
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

		/* Check area's age and reset if necessary*/
		if (fBoot || (pArea->age >= pArea->repop || (pArea->repop == 0 && pArea->age > 15) || pArea->age >= 120))
		{
			plogf("db.c, area_update: Resetting area %s.", pArea->name);
			reset_area(pArea);
			sprintf(buf,"%s has just been reset.",pArea->name);
			wiznet(buf,NULL,NULL,WIZ_RESETS,0,0);
			pArea->age = 0;

			if (pArea->nplayer == 0)
				pArea->empty = TRUE;

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
						plogf("Resetting wilds uid %ld, '%s'...", pWilds->uid, pWilds->name);
						pWilds->age = 0;

						// This.. doesn't do anything?
					}
				}
			}
		}
	}

	for (hash = 0; hash < MAX_KEY_HASH; hash++)
	{
		for (room = room_index_hash[hash]; room; room = room->next)
		{
			// Persistant rooms are handled separately!
			if (!room->persist && can_room_update(room))
			{
				room_update(room);
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
					last_mob = get_mob_index(curr->arg1);
					break;

				case 'G':
					// Mob must be a shopkeeper, the object must exist and the object must not be money

					if( (last_mob != NULL) &&
						(last_mob->pShop != NULL) &&
						((obj = get_obj_index(curr->arg1)) != NULL) &&
						(obj->item_type != ITEM_MONEY))
					{

						SHOP_STOCK_DATA *stock = new_shop_stock();
						if(!stock) {
							prev = curr;
							break;	// SHOULD ABORT?
						}

						// Generate stock entry
						stock->type = STOCK_OBJECT;
						stock->vnum = obj->vnum;
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


void reset_room(ROOM_INDEX_DATA *pRoom)
{
	RESET_DATA  *pReset;
	CHAR_DATA   *pMob;
	CHAR_DATA	*mob;
	OBJ_DATA    *pObj;
	OBJ_DATA    *obj;
	CHAR_DATA   *LastMob = NULL;
	OBJ_DATA    *LastObj = NULL;
	int iExit;
	int level = 0;
	bool last;
	bool instanced = FALSE;
	int i;
	int c;

	// Invalid room or the room is persistant
	if (!pRoom || pRoom->persist)
		return;

	pMob = NULL;
	last = FALSE;

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
		char buf[MAX_STRING_LENGTH];
		int count,limit=0;

		instanced = FALSE;

		switch (pReset->command)
		{
		default:
			bug("Reset_room: bad command %c.", pReset->command);
			break;

		case 'M':
			if (!(pMobIndex = get_mob_index(pReset->arg1)))
			{
				bug("Reset_room: 'M': bad vnum %ld.", pReset->arg1);
				continue;
			}

			if ((pRoomIndex = get_room_index(pReset->arg3)) == NULL)
			{
				bug("Reset_area: 'R': bad vnum %ld.", pReset->arg3);
				continue;
			}

			// When the room is in an instance
			if( pRoom->instance_section != NULL )
			{
				count = instance_count_mob(pRoom->instance_section->instance, pMobIndex);

				char buf[MSL];
				sprintf(buf, "reset_room(M): %ld -> %ld = %d / %ld", pRoom->vnum, pMobIndex->vnum, count, pReset->arg2);
				wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

				if( count >= pReset->arg2 )
				{
					last = FALSE;
					break;
				}

				instanced = TRUE;
			}
			else if (pMobIndex->count >= pReset->arg2)
			{
				last = FALSE;
				break;
			}

			count = 0;
			for (mob = pRoom->people; mob != NULL; mob = mob->next_in_room)
			{
				if (mob->pIndexData == pMobIndex && !IS_SET(mob->act, ACT_ANIMATED))
				{
					count++;
					if (count >= pReset->arg4)
					{
						last = FALSE;
						break;
					}
				}
			}

			if (count >= pReset->arg4)
				break;

			pMob = create_mobile(pMobIndex, FALSE);
			if( instanced )
				SET_BIT(pMob->act2, ACT2_INSTANCE_MOB);

			/*
			* Some more hard coding.
			*/
			if (room_is_dark(pRoom))
				SET_BIT(pMob->affected_by, AFF_INFRARED);

			char_to_room(pMob, pRoom);
			pMob->home_room = pRoom;

			/* Give some pneuma to POA mobs.*/
			if (!str_cmp(pMob->in_room->area->name, "Maze-Level1"))
			{
				i = number_range(1,2);
				for (c = 0; c < i; c++)
				{
					obj = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 1, FALSE);
					obj_to_char(obj, pMob);
				}
			}
			else if (!str_cmp(pMob->in_room->area->name, "Maze-Level2"))
			{
				i = number_range(2,3);
				for (c = 0; c < i; c++)
				{
					obj = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 1, FALSE);
					obj_to_char(obj, pMob);
				}
			}
			else if (!str_cmp(pMob->in_room->area->name, "Maze-Level3"))
			{
				i = number_range(3,4);
				for (c = 0; c < i; c++)
				{
					obj = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 1, FALSE);
					obj_to_char(obj, pMob);
				}
			}
			else if (!str_cmp(pMob->in_room->area->name, "Maze-Level4"))
			{
				i = number_range(4,6);
				for (c = 0; c < i; c++)
				{
					obj = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 1, FALSE);
					obj_to_char(obj, pMob);
				}
			}
			else if (!str_cmp(pMob->in_room->area->name, "Maze-Level5"))
			{
				i = number_range(10,15);
				for (c = 0; c < i; c++)
				{
					obj = create_object(get_obj_index(OBJ_VNUM_BOTTLED_SOUL), 1, FALSE);
					obj_to_char(obj, pMob);
				}
			}

			LastMob = pMob;
			level  = URANGE(0, pMob->level - 2, LEVEL_HERO - 1); /* -1 ROM */
			last = TRUE;
			objRepop = TRUE;
			p_percent_trigger(pMob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
			break;

		case 'O':
			if (!(pObjIndex = get_obj_index(pReset->arg1)))
			{
				bug("Reset_room: 'O' 1 : bad vnum %ld", pReset->arg1);
				sprintf (buf,"%ld %ld %ld %ld",pReset->arg1, pReset->arg2, pReset->arg3, pReset->arg4);
				bug(buf,1);
				continue;
			}

			if (!(pRoomIndex = get_room_index(pReset->arg3)))
			{
				bug("Reset_room: 'O' 2 : bad vnum %ld.", pReset->arg3);
				sprintf (buf,"%ld %ld %ld %ld",pReset->arg1, pReset->arg2, pReset->arg3,
				pReset->arg4);
				bug(buf,1);
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

				instanced = TRUE;
			}
			else
				players = pRoom->area->nplayer;	// In a normal area


			if (players > 0 || count_obj_list(pObjIndex, pRoom->contents) > 0)
			{
				last = FALSE;
				break;
			}

			pObj = create_object(pObjIndex, UMIN(number_fuzzy(level), LEVEL_HERO -1) , TRUE);
			if( instanced )
				SET_BIT(pObj->extra3_flags, ITEM_INSTANCE_OBJ);
			pObj->cost = 0;
			objRepop = TRUE;
			obj_to_room(pObj, pRoom);
			last = TRUE;
			break;

		case 'P':
			if (!(pObjIndex = get_obj_index(pReset->arg1)))
			{
				bug("Reset_room: 'P': bad vnum %ld.", pReset->arg1);
				continue;
			}

			if (!(pObjToIndex = get_obj_index(pReset->arg3)))
			{
				bug("Reset_room: 'P': bad vnum %ld.", pReset->arg3);
				continue;
			}

			if (pReset->arg2 > 50) /* old format */
				limit = 6;
			else if (pReset->arg2 == -1) /* no limit */
				limit = 999;
			else
				limit = pReset->arg2;

			if (pRoom->area->nplayer > 0 ||
				(LastObj = get_obj_type(pObjToIndex, pRoom)) == NULL ||
				(LastObj->in_room == NULL && !last) ||
				(pObjIndex->count >= limit) ||
				(count = count_obj_list(pObjIndex, LastObj->contains)) > pReset->arg4 )
			{
				last = FALSE;
				break;
			}
			/* lastObj->level  -  ROM */

			while (count < pReset->arg4)
			{
				pObj = create_object(pObjIndex, number_fuzzy(LastObj->level), TRUE);
				obj_to_obj(pObj, LastObj);
				count++;
				if (pObjIndex->count >= limit)
					break;
			}

			/* fix object lock state! */
			LastObj->value[1] = LastObj->pIndexData->value[1];
			last = TRUE;
			break;

		case 'G':
		case 'E':
			if (!(pObjIndex = get_obj_index(pReset->arg1)))
			{
				bug("Reset_room: 'E' or 'G': bad vnum %ld.", pReset->arg1);
				continue;
			}

			if (!last)
				break;

			if (!LastMob)
			{
				bug("Reset_room: 'E' or 'G': null mob for vnum %ld.",
				pReset->arg1);
				last = FALSE;
				break;
			}

			{
				int limit;
				if (pReset->arg2 > 50)
					limit = 6;
				else if (pReset->arg2 == -1 || pReset->arg2 == 0)
					limit = 999;
				else
					limit = pReset->arg2;

				if (pObjIndex->count < limit || number_range(0,4) == 0)
				{
					pObj = create_object(pObjIndex, UMIN(number_fuzzy(level), LEVEL_HERO - 1), TRUE);
				}
				else
					break;
			}

			objRepop = TRUE;
			obj_to_char(pObj, LastMob);
			if (pReset->command == 'E')
				equip_char(LastMob, pObj, pReset->arg3);
			last = TRUE;
			break;

		case 'D':
			break;

		case 'R':
			if (!(pRoomIndex = get_room_index(pReset->arg1)))
			{
				bug("Reset_room: 'R': bad vnum %ld.", pReset->arg1);
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

    /* don't do for now
    if (pMobIndex->vnum == MOB_VNUM_GATEKEEPER_ABYSS)
    {
       if (pMobIndex->count > 0)
       return;
       else
       {
       obj = create_object(get_obj_index(OBJ_VNUM_KEY_ABYSS), 30, TRUE);
       }
       }
       else
    */

    if (number_percent() <= chance)
    {
	pMobile = create_mobile(pMobIndex, FALSE);
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
	to_stock->vnum = from_stock->vnum;
	switch(to_stock->type)
	{
	case STOCK_OBJECT:
		if(to_stock->vnum > 0)
			to_stock->obj = get_obj_index(to_stock->vnum);
		break;
	case STOCK_PET:
	case STOCK_MOUNT:
	case STOCK_GUARD:
		if(to_stock->vnum > 0)
			to_stock->mob = get_mob_index(to_stock->vnum);
		break;
	case STOCK_SHIP:
		if(to_stock->vnum > 0)
			to_stock->ship = get_ship_index(to_stock->vnum);
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

	if( to_shop->shipyard > 0 )
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

	if( from_shop->stock )
		copy_shop_stock(to_shop, from_shop->stock);
}


/* Create a mobile from a mob index template.*/
CHAR_DATA *create_mobile(MOB_INDEX_DATA *pMobIndex, bool persistLoad)
{
	CHAR_DATA *mob;
	AFFECT_DATA af;
	GQ_MOB_DATA *gq_mob;
	const struct race_type *race;
	int i;

	mobile_count++;

	if (pMobIndex == NULL)
	{
		bug("Create_mobile: NULL pMobIndex.", 0);
		exit(1);
	}

	mob = new_char();

	mob->pIndexData	= pMobIndex;

	mob->name			= str_dup(pMobIndex->player_name);
	mob->short_descr	= str_dup(pMobIndex->short_descr);
	mob->long_descr		= str_dup(pMobIndex->long_descr);
	mob->description	= str_dup(pMobIndex->description);

	if (pMobIndex->owner != NULL)
		mob->owner	= str_dup(pMobIndex->owner);
	else
		mob->owner	= str_dup("(no owner)");

	get_mob_id(mob);
	mob->spec_fun		= pMobIndex->spec_fun;
	mob->prompt			= NULL;

	mob->progs			= new_prog_data();
	mob->progs->progs	= pMobIndex->progs;
	variable_copylist(&pMobIndex->index_vars,&mob->progs->vars,FALSE);

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
		if (IS_SET(pMobIndex->act, ACT_IS_BANKER))
		{
			wealth = 1000000;
			SET_BIT(mob->act, ACT_PROTECTED);
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

	mob->act 				= pMobIndex->act;
	mob->act2				= pMobIndex->act2;
	mob->comm				= COMM_NOCHANNELS|COMM_NOTELL;
	mob->affected_by		= pMobIndex->affected_by;
	mob->affected_by2		= pMobIndex->affected_by2;
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

	if (mob->sex == 3) /* random sex */
		mob->sex = number_range(1,2);

	mob->race				= pMobIndex->race;
	race = &race_table[mob->race];
	mob->form				= pMobIndex->form;
	mob->parts				= pMobIndex->parts;
	mob->size				= pMobIndex->size;
	mob->material			= str_dup(pMobIndex->material);
	mob->corpse_type		= pMobIndex->corpse_type;
	mob->corpse_vnum		= pMobIndex->corpse;

	mob->affected_by_perm	= race->aff;
	mob->affected_by2_perm	= race->aff2;
	mob->imm_flags_perm		= pMobIndex->imm_flags;
	mob->res_flags_perm		= pMobIndex->res_flags;
	mob->vuln_flags_perm	= pMobIndex->vuln_flags;


	for (i = 0; i < MAX_STATS; i ++)
		set_perm_stat_range(mob, i, 11 + mob->level/4, 0, 25);

	if (IS_SET(mob->act,ACT_WARRIOR))
	{
		mob->perm_stat[STAT_STR] += 3;
		mob->perm_stat[STAT_INT] -= 1;
		mob->perm_stat[STAT_CON] += 2;
	}

	if (IS_SET(mob->act,ACT_THIEF))
	{
		mob->perm_stat[STAT_DEX] += 3;
		mob->perm_stat[STAT_INT] += 1;
		mob->perm_stat[STAT_WIS] -= 1;
	}

	if (IS_SET(mob->act,ACT_CLERIC))
	{
		mob->perm_stat[STAT_WIS] += 3;
		mob->perm_stat[STAT_DEX] -= 1;
		mob->perm_stat[STAT_STR] += 1;
	}

	if (IS_SET(mob->act,ACT_MAGE))
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


		memset(&af,0,sizeof(af));
		af.slot	= WEAR_NONE;	// None of the subsequent affects are from worn objects

		/* Put spells on here*/
		if (IS_AFFECTED(mob,AFF_INVISIBLE))
		{
			af.group		= IS_SET(race->aff,AFF_INVISIBLE)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_invis;
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
			af.group		= IS_SET(race->aff,AFF_DETECT_INVIS)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type 		= gsn_detect_invis;
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
			af.group		= IS_SET(race->aff,AFF_DETECT_HIDDEN)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_detect_hidden;
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
			af.group		= IS_SET(race->aff,AFF_SANCTUARY)?AFFGROUP_RACIAL:AFFGROUP_DIVINE;
			af.where		= TO_AFFECTS;
			af.type			= gsn_sanctuary;
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
			af.group		= IS_SET(race->aff,AFF_INFRARED)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_infravision;
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
			af.group		= IS_SET(race->aff,AFF_DEATH_GRIP)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_death_grip;
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
			af.group		= IS_SET(race->aff,AFF_FLYING)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_fly;
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
			af.group		= IS_SET(race->aff,AFF_PASS_DOOR)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_pass_door;
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
			af.group		= IS_SET(race->aff,AFF_HASTE)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_haste;
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
			af.type			= gsn_warcry;
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
			af.group		= IS_SET(race->aff2,AFF2_LIGHT_SHROUD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_light_shroud;
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
			af.group		= IS_SET(race->aff2,AFF2_HEALING_AURA)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_healing_aura;
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
			af.group		= IS_SET(race->aff2,AFF2_ENERGY_FIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_energy_field;
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
			af.group		= IS_SET(race->aff2,AFF2_SPELL_SHIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_spell_shield;
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
			af.group		= IS_SET(race->aff2,AFF2_SPELL_DEFLECTION)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_spell_deflection;
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
			af.group		= IS_SET(race->aff2,AFF2_AVATAR_SHIELD)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_avatar_shield;
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
			af.group		= IS_SET(race->aff2,AFF2_ELECTRICAL_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_electrical_barrier;
			af.level		= mob->level;
			af.duration		= -1;
			af.location		= 0;
			af.modifier		= 0;
			af.bitvector	= 0;
			af.bitvector2	= AFF2_ELECTRICAL_BARRIER;
			affect_to_char(mob,&af);
			REMOVE_BIT(mob->affected_by2_perm, af.bitvector2);	// NIBS - why only this one?
		}

		if (IS_AFFECTED2(mob, AFF2_FIRE_BARRIER))
		{
			af.group		= IS_SET(race->aff2,AFF2_FIRE_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_fire_barrier;
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
			af.group		= IS_SET(race->aff2,AFF2_FROST_BARRIER)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_frost_barrier;
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
			af.group		= IS_SET(race->aff2,AFF2_IMPROVED_INVIS)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_improved_invisibility;
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
			af.group		= IS_SET(race->aff2,AFF2_STONE_SKIN)?AFFGROUP_RACIAL:AFFGROUP_MAGICAL;
			af.where		= TO_AFFECTS;
			af.type			= gsn_stone_skin;
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
		if (pMobIndex->vnum == gq_mob->vnum)
			gq_mob->count++;
    }

	if(pMobIndex->persist)
		persist_addmobile(mob);

	// make sure nothing has 0 hp
	mob->max_hit = UMAX(1, mob->max_hit);

	return mob;
}


CHAR_DATA *clone_mobile(CHAR_DATA *parent)
{
	CHAR_DATA *clone;
    int i;
    AFFECT_DATA *paf;

    if (parent == NULL || !IS_NPC(parent))
		return NULL;

	clone = create_mobile(parent->pIndexData, FALSE);
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
    clone->act		= parent->act;
    clone->act2		= parent->act2;
    clone->comm		= parent->comm;
    clone->imm_flags	= parent->imm_flags;
    clone->res_flags	= parent->res_flags;
    clone->vuln_flags	= parent->vuln_flags;
    clone->invis_level	= parent->invis_level;
    clone->affected_by	= parent->affected_by;
    clone->affected_by2	= parent->affected_by2;
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

    clone->affected_by_perm = parent->affected_by_perm;
    clone->affected_by2_perm = parent->affected_by2_perm;
	clone->imm_flags_perm = parent->imm_flags_perm;
	clone->res_flags_perm = parent->res_flags_perm;
	clone->vuln_flags_perm = parent->vuln_flags_perm;


    for (i = 0; i < 4; i++)
    	clone->armour[i]	= parent->armour[i];

    for (i = 0; i < MAX_STATS; i++)
    {
		clone->perm_stat[i]	= parent->perm_stat[i];
		clone->mod_stat[i]	= parent->mod_stat[i];
		clone->dirty_stat[i] = TRUE;
    }

	clone->damage.number = parent->damage.number;
	clone->damage.size = parent->damage.size;
	clone->damage.bonus = parent->damage.bonus;
	clone->damage.last_roll = -1;

    /* now add the affects */
    for (paf = parent->affected; paf != NULL; paf = paf->next)
        affect_to_char(clone,paf);

    variable_freelist(&clone->progs->vars);
    variable_copylist(&parent->progs->vars,&clone->progs->vars,FALSE);

	if(parent->persist && !clone->persist)
		persist_addmobile(clone);

	return clone;
}


OBJ_DATA *create_object_noid(OBJ_INDEX_DATA *pObjIndex, int level, bool affects)
{
    AFFECT_DATA *paf;
    SPELL_DATA *spell, *spell_new;
    OBJ_DATA *obj;
    GQ_OBJ_DATA *gq_obj;

    if (pObjIndex == NULL)
    {
	bug("Create_object: NULL pObjIndex.", 0);
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
    obj->material	= str_dup(pObjIndex->material);
    obj->condition	= pObjIndex->condition;
    obj->times_allowed_fixed	= pObjIndex->times_allowed_fixed;
    obj->fragility	= pObjIndex->fragility;
    obj->item_type	= pObjIndex->item_type;
    obj->extra_flags	= pObjIndex->extra_flags & ~(ITEM_INVENTORY | ITEM_PERMANENT | ITEM_NOSKULL | ITEM_PLANTED);
    obj->extra2_flags   = pObjIndex->extra2_flags & ~(ITEM_ENCHANTED | ITEM_NO_RESURRECT | ITEM_THIRD_EYE | ITEM_BURIED | ITEM_UNSEEN );
    obj->extra3_flags   = pObjIndex->extra3_flags & ~(ITEM_FORCE_LOOT | ITEM_NO_ANIMATE );
    obj->extra4_flags   = pObjIndex->extra4_flags;
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
		obj->lock->key_vnum = pObjIndex->lock->key_vnum;
		obj->lock->flags = pObjIndex->lock->flags;
		obj->lock->pick_chance = pObjIndex->lock->pick_chance;
	}

    /*
     * Mess with object properties.
     */
    switch (obj->item_type)
    {
	case ITEM_LIGHT:
	    if (obj->value[2] >= 999)
		obj->value[2] = -1;
	    break;

	case ITEM_CATALYST:
	    if (!obj->value[1]) obj->value[1] = 1; /* Fix zero charge catalysts to single uses*/
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
	    break;

	case ITEM_FOOD:
	    obj->timer = obj->value[4];
            break;

	default:
	    bug("create_object: vnum %ld bad type.", pObjIndex->vnum);

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

	for (paf = pObjIndex->catalyst; paf != NULL; paf = paf->next)
	{
	    if (number_percent() < paf->random || paf->random == 100)
		catalyst_to_obj(obj,paf);
	}
    }

    variable_copylist(&pObjIndex->index_vars,&obj->progs->vars,FALSE);

    obj->num_enchanted = 0;
    obj->version = VERSION_OBJECT_000;
    obj->locker = FALSE;

	list_appendlink(loaded_objects, obj);
    pObjIndex->count++;


    /* If loading a relic for whatever reason, update the pointers here.*/
    if (pObjIndex->vnum == OBJ_VNUM_RELIC_EXTRA_DAMAGE)
	damage_relic = obj;

    if (pObjIndex->vnum == OBJ_VNUM_RELIC_EXTRA_XP)
	xp_relic = obj;

    if (pObjIndex->vnum == OBJ_VNUM_RELIC_EXTRA_PNEUMA)
	pneuma_relic = obj;

    if (pObjIndex->vnum == OBJ_VNUM_RELIC_HP_REGEN)
	hp_regen_relic = obj;

    if (pObjIndex->vnum == OBJ_VNUM_RELIC_MANA_REGEN)
	mana_regen_relic = obj;

    /* Keep GQ count*/
    for (gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next)
    {
	if (pObjIndex->vnum == gq_obj->vnum)
	    gq_obj->count++;
    }

    if(pObjIndex->persist) {
		log_stringf("create_object_noid: Adding object %ld to persistance.", pObjIndex->vnum);
    	persist_addobject(obj);
	}

    return obj;
}

OBJ_DATA *create_object(OBJ_INDEX_DATA *pObjIndex, int level, bool affects)
{
    OBJ_DATA *obj;

    obj = create_object_noid(pObjIndex,level,affects);

	if( obj )
	{
		// Copy the extra descriptions
		for (EXTRA_DESCR_DATA *ed = pObjIndex->extra_descr; ed != NULL; ed = ed->next)
		{
			EXTRA_DESCR_DATA *ed_new	= new_extra_descr();
			ed_new->keyword				= str_dup(ed->keyword);
			ed_new->description			= str_dup(ed->description);
			ed_new->next				= obj->extra_descr;
			obj->extra_descr			= ed_new;
		}

    	get_obj_id(obj);
	}

    return obj;
}


void clone_object(OBJ_DATA *parent, OBJ_DATA *clone)
{
    int i;
    AFFECT_DATA *paf, *paf_next;
    EXTRA_DESCR_DATA *ed,*ed_new;

    if (parent == NULL || clone == NULL)
	return;

    /* remove the affects first so we don't get dual affects */
    for (paf = clone->affected; paf != NULL; paf = paf_next) {
        paf_next = paf->next;

	affect_remove_obj(clone, paf);
    }

    for (paf = clone->catalyst; paf != NULL; paf = paf_next) {
        paf_next = paf->next;

	free_affect(paf);
    }

    clone->affected = NULL;
    clone->catalyst = NULL;

    /* start fixing the object */
    clone->name 	= str_dup(parent->name);
    clone->short_descr 	= str_dup(parent->short_descr);
    clone->description	= str_dup(parent->description);
    clone->item_type	= parent->item_type;
    clone->extra_flags	= parent->extra_flags;
    clone->extra2_flags	= parent->extra2_flags;
    clone->extra3_flags	= parent->extra3_flags;
    clone->extra4_flags	= parent->extra4_flags;
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

    /* affects */
    for (paf = parent->affected; paf != NULL; paf = paf->next)
		affect_to_obj(clone,paf);

    /* catalyst affects */
    for (paf = parent->catalyst; paf != NULL; paf = paf->next)
		catalyst_to_obj(clone,paf);

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
    variable_copylist(&parent->progs->vars,&clone->progs->vars,FALSE);

	if(parent->persist && !clone->persist) {
		log_stringf("clone_object: Adding object %ld to persistance.", clone->pIndexData->vnum);

		persist_addobject(clone);
	}
}


char *get_extra_descr(const char *name, EXTRA_DESCR_DATA *ed)
{
    for (; ed != NULL; ed = ed->next)
    {
	if (is_name((char *) name, ed->keyword))
	    return ed->description;
    }
    return NULL;
}


/*
 * Translates mob virtual number to its mob index struct.
 */
MOB_INDEX_DATA *get_mob_index(long vnum)
{
    MOB_INDEX_DATA *mob;

    for (mob = mob_index_hash[vnum % MAX_KEY_HASH]; mob != NULL; mob = mob->next)
    {
	if (mob->vnum == vnum)
	    return mob;
    }

    if (fBootDb)
    {
	bug("Get_mob_index: bad vnum %ld.", vnum);
	exit(1);
    }

    return NULL;
}


/*
 * Translates mob virtual number to its obj index struct.
 */
OBJ_INDEX_DATA *get_obj_index(long vnum)
{
    OBJ_INDEX_DATA *obj;

    for (obj = obj_index_hash[vnum % MAX_KEY_HASH]; obj != NULL; obj = obj->next)
    {
	if (obj->vnum == vnum)
	    return obj;
    }

    if (fBootDb)
    {
	bug("Get_obj_index: bad vnum %ld.", vnum);
	exit(1);
    }

    return NULL;
}


/*
 * Translates room virtual number to its room index struct.
 */
ROOM_INDEX_DATA *get_room_index(long vnum)
{
    ROOM_INDEX_DATA *room;

    for (room = room_index_hash[vnum % MAX_KEY_HASH]; room != NULL; room = room->next)
    {
	if (room->vnum == vnum)
	    return room;
    }

    if (fBootDb)
    {
	bug("Get_room_index: bad vnum %ld.", vnum);
        return NULL;
    }

    return NULL;
}


TOKEN_INDEX_DATA *get_token_index(long vnum)
{
    TOKEN_INDEX_DATA *token_index;

    for (token_index = token_index_hash[vnum % MAX_KEY_HASH]; token_index != NULL; token_index = token_index->next)
    {
	if (token_index->vnum == vnum)
	    return token_index;
    }

    return NULL;
}

bool is_singular_token(TOKEN_INDEX_DATA *index)
{
	if(!index) return FALSE;

	// These can allow multiples
	if(index->type == TOKEN_GENERAL || index->type == TOKEN_QUEST || index->type == TOKEN_AFFECT)
		if(!IS_SET(index->flags, TOKEN_SINGULAR))
			return FALSE;

	return TRUE;
}


SCRIPT_DATA *get_script_index(long  vnum, int type)
{
    SCRIPT_DATA *prg;

    switch (type)
    {
	case PRG_MPROG:
	    prg = mprog_list;
	    break;
	case PRG_OPROG:
	    prg = oprog_list;
	    break;
	case PRG_RPROG:
	    prg = rprog_list;
	    break;
	case PRG_TPROG:
	    prg = tprog_list;
	    break;
	case PRG_APROG:
	    prg = aprog_list;
	    break;
	case PRG_IPROG:
	    prg = iprog_list;
	    break;
	case PRG_DPROG:
	    prg = dprog_list;
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



/*
 * Read a letter from a file.
 */
char fread_letter(FILE *fp)
{
    char c;

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    return c;
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
    while (isspace(c));

    number = 0;

    sign   = FALSE;
    if (c == '+')
    {
	c = getc(fp);
    }
    else if (c == '-')
    {
	sign = TRUE;
	c = getc(fp);
    }

    if (!isdigit(c))
    {
	bug("Fread_number: bad format (%c).", c);
	exit(1);
    }

    while (isdigit(c))
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
    bool negative = FALSE;

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    if (c == '-')
    {
	negative = TRUE;
	c = getc(fp);
    }

    number = 0;

    if (!isdigit(c))
    {
	while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'))
	{
	    number += flag_convert(c);
	    c = getc(fp);
	}
    }

    while (isdigit(c))
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
	bug("Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
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
    while (isspace(c));

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
		bug("Fread_string: EOF", 0);
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

    if (char_special[EOF-EOF] != TRUE)
    {
	char_special[EOF -  EOF] = TRUE;
	char_special['\n' - EOF] = TRUE;
	char_special['\r' - EOF] = TRUE;
    }

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
	bug("Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
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
    while (isspace(c));

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
		bug("Fread_string_eol  EOF", 0);
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


char *fread_string_new(FILE *fp)
{
    char c;
    char pLast;
    int i = 0;
    char newStr[MSL];

    /*
     * Skip blanks.
     * Read first char.
     */
    do
        c = getc(fp);
    while (isspace(c));

    if (c == '~')
	return &str_empty[0];

    newStr[i] = c;
    i++;
    for (;;)
    {
	pLast = getc(fp);

	if (pLast == '~' || pLast == EOF)
	    break;

	switch (pLast)
	{
	    default:
	    	newStr[i] = pLast;
		i++;
		break;


	    case '\n':
		newStr[i] = '\n';
		newStr[i + 1] = '\r';

		i += 2;
		break;
	}
    }

    newStr[i] = '\0';

    return str_dup(newStr);
}


char *fread_string_eol_new(FILE *fp)
{
    char c;
    char pLast;
    int i = 0;
    char newStr[MSL];

    /*
     * Skip blanks.
     * Read first char.
     */
    do
        c = getc(fp);
    while (isspace(c));

    if (c == '~')
	return &str_empty[0];

    newStr[i] = c;
    i++;
    for (;;)
    {
	pLast = getc(fp);

	if (pLast == '\n' || pLast == EOF)
	    break;

	newStr[i] = pLast;
	i++;
    }

    newStr[i] = '\0';

    return str_dup(newStr);
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
    char cEnd;

    do
    {
	cEnd = getc(fp);
    }
    while (isspace(cEnd));

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
	*pword = getc(fp);
	if (cEnd == ' ' ? isspace(*pword) : *pword == cEnd)
	{
	    if (cEnd == ' ')
		ungetc(*pword, fp);
	    *pword = '\0';
	    return word;
	}
    }

    bug("Fread_word: word too long.", 0);
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
        bug("Alloc_mem: size %d too large.", sMem);
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
	return calloc(1,sMem);
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
        bug("Attempt to recycle invalid memory of size %d.",sMem);
        bug("Magic: %08X.",*magic);
        bug("Magic NUM: %08X.",MAGIC_NUM);
        bug((char*) pMem + sizeof(*magic),0);
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
        bug("Free_mem: size %d too large.", sMem);
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
	bug("Alloc_perm: %d too large.", sMem);
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
	return calloc(1,sMem);
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

#if defined (OLD_RAND)
static  int     rgiState[2+55];
#endif

void init_mm()
{
#if defined (OLD_RAND)
    int *piState;
    int iState;

    piState     = &rgiState[2];

    piState[-2] = 55 - 55;
    piState[-1] = 55 - 24;

    piState[0]  = ((int) current_time) & ((1 << 30) - 1);
    piState[1]  = 1;
    for (iState = 2; iState < 55; iState++)
    {
        piState[iState] = (piState[iState-1] + piState[iState-2])
                        & ((1 << 30) - 1);
    }
#else
    srandom(time(NULL)^getpid());
#endif
    return;
}


long number_mm(void)
{
#if defined (OLD_RAND)
    int *piState;
    int iState1;
    int iState2;
    int iRand;

    piState             = &rgiState[2];
    iState1             = piState[-2];
    iState2             = piState[-1];
    iRand               = (piState[iState1] + piState[iState2])
                        & ((1 << 30) - 1);
    piState[iState1]    = iRand;
    if (++iState1 == 55)
        iState1 = 0;
    if (++iState2 == 55)
        iState2 = 0;
    piState[-2]         = iState1;
    piState[-1]         = iState2;
    return iRand >> 6;
#else
    return random() >> 6;
#endif
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
int str_cmp(const char *astr, const char *bstr)
{
    char ch;
    if (astr == NULL)
    {
	bug("Str_cmp: null astr.", 0);
	return -1;
    }

    if (bstr == NULL)
    {
	bug("Str_cmp: null bstr.", 0);
	return 1;
    }

    for (; *astr || *bstr; astr++, bstr++) {
	if ((ch = (LOWER(*astr) - LOWER(*bstr))))
	    return ch;
    }

    return 0;
}

// str_cmp, ignoring color codes
int str_cmp_nocolour(const char *astr, const char *bstr)
{
	char *ncastr, *ncbstr, *nca, *ncb;
    char ch;
    if (astr == NULL)
    {
	bug("Str_cmp: null astr.", 0);
	return -1;
    }

    if (bstr == NULL)
    {
	bug("Str_cmp: null bstr.", 0);
	return 1;
    }

    nca = ncastr = nocolour(astr);
    ncb = ncbstr = nocolour(bstr);

    for (; *ncastr || *ncbstr; ncastr++, ncbstr++) {
		if ((ch = (LOWER(*ncastr) - LOWER(*ncbstr)))) {
			free_string(nca);
			free_string(ncb);
		    return ch;
		}
    }

	free_string(nca);
	free_string(ncb);
    return 0;
}


/*
 * Compare strings, case insensitive, for prefix matching.
 * Return TRUE if astr not a prefix of bstr
 *   (compatibility with historical functions).
 */
bool str_prefix(const char *astr, const char *bstr)
{
    if (astr == NULL)
    {
	bug("Strn_cmp: null astr.", 0);
	return TRUE;
    }

    if (bstr == NULL)
    {
	bug("Strn_cmp: null bstr.", 0);
	return TRUE;
    }

    for (; *astr; astr++, bstr++)
    {
	if (LOWER(*astr) != LOWER(*bstr))
	    return TRUE;
    }

    return FALSE;
}


/*
 * Compare strings, case insensitive, for match anywhere.
 * Returns TRUE is astr not part of bstr.
 *   (compatibility with historical functions).
 */
bool str_infix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;
    int ichar;
    char c0;

    if ((c0 = LOWER(astr[0])) == '\0')
	return FALSE;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);

    for (ichar = 0; ichar <= sstr2 - sstr1; ichar++)
    {
	if (c0 == LOWER(bstr[ichar]) && !str_prefix(astr, bstr + ichar))
	    return FALSE;
    }

    return TRUE;
}


/*
 * Compare strings, case insensitive, for suffix matching.
 * Return TRUE if astr not a suffix of bstr
 *   (compatibility with historical functions).
 */
bool str_suffix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);
    if (sstr1 <= sstr2 && !str_cmp(astr, bstr + sstr2 - sstr1))
	return FALSE;
    else
	return TRUE;
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
    static char strcap[MAX_STRING_LENGTH];
    int i;

    for (i = 0; str[i] != '\0'; i++)
	strcap[i] = LOWER(str[i]);
    strcap[i] = '\0';
    strcap[0] = UPPER(strcap[0]);
    return strcap;
}


/*
 * Append a string to a file.
 */
void append_file(CHAR_DATA *ch, char *file, char *str)
{
    FILE *fp;

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

    fpReserve = fopen(NULL_FILE, "r");
    return;
}


void bug(const char *str, int param)
{
    char buf[MAX_STRING_LENGTH];

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
	log_string(buf);
    }

    strcpy(buf, "[*****] BUG: ");
    sprintf(buf + strlen(buf), str, param);
    log_string(buf);
}


/*
 * Writes a string to the log.
 */
void log_string(const char *str)
{
    char *strtime;

    strtime                    = ctime(&current_time);
    strtime[strlen(strtime)-1] = '\0';
    printf("%s :: %s\n", strtime, str);
    return;
}

void log_stringf(const char *fmt,...)
{
	char buf[2 * MSL];
	va_list args;
	va_start (args, fmt);
	vsprintf (buf, fmt, args);
	va_end (args);

	log_string (buf);
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
        obj = create_object(get_obj_index(OBJ_VNUM_SHARD), 0, TRUE);
        pRoom = get_random_room(NULL, 0);

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
	bug("Get_npc_ship_index: bad vnum %ld.", vnum);
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

    log_string("Resetting npc boats...");
    if ((pArea = get_wilderness_area()) == NULL)
	return;

/*
    if (plith_airship == NULL)
    {
	 Plith airship is vnum 1*/
	npc_ship = create_npc_sailing_boat(1);
	plith_airship = npc_ship;

	/* Put in Town Square of Plith*/
	obj_to_room(npc_ship->ship->ship, get_room_index(ROOM_VNUM_PLITH_AIRSHIP));
    }
*/

    for(i = 2; i <= top_vnum_npc_ship; i++) {
	npc_ship_index = ship_index_hash[ i ];

	if (npc_ship_index == NULL)
	    continue;

	log_string("Resetting boat");
	npc_ship = create_npc_sailing_boat(npc_ship_index->vnum);

	index = (long)((long)npc_ship_index->original_y *
	               (long)pArea->map_size_x +
			   (long)npc_ship_index->original_x + (long)pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

	/* If the npc airship then set airship*/
	if (npc_ship->pShipData->npc_type == NPC_SHIP_AIR_SHIP)
	{
	    plith_airship = npc_ship;
	    index = ROOM_VNUM_TEMPLE;
	}

	if (get_room_index(index) == NULL)
	{
	    bug("While resetting npc boats, the room index was NULL.", index);
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
	log_string("Couldn't find area Wilderness.");

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
	if(str) while(isspace(*str)) ++str;
	return str;
}

void strip_newline(char *buf, bool append)
{
	int len = strlen(buf);
	while(len > 0 && isspace(buf[len-1])) --len;
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

			REMOVE_BIT(area->area_flags, AREA_CHANGED);
			save_area_new(area);
		}
	}
}

char *fread_string_len(FILE *fp)
{
    char *plast;
    char c;
    long i,len;

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
	bug("Fread_string_new: MAX_STRING %d exceeded.", MAX_STRING);
	exit(1);
    }

	/* Taken from fread_number and reduced to just positive numbers*/
	/* Skip whitespaces*/
    do
	c = getc(fp);
    while (isspace(c));

    len = 0;

    if (!isdigit(c)) {
	bug("Fread_string_new: bad format (%c).", c);
	exit(1);
    }

    while (isdigit(c))
    {
	len = len * 10 + c - '0';
	c      = getc(fp);
    }

    ungetc(c, fp);

	if(len < 1 || len > MSL) {
		bug("Fread_string_new: bad string size (%d).", len);
		exit(1);
	}


    if ((*plast++ = c) == '~')
	return &str_empty[0];

    for (i = 0;i < len;i++)
    {
	/*
	 * Back off the char type lookup,
	 *   it was too dirty for portability.
	 *   -- Furey
	 */

	switch (*plast = getc(fp)) {
	    default:
		plast++;
		break;

	    case EOF:
		/* temp fix */
		bug("Fread_string_new: EOF", 0);
		return NULL;
		/* exit(1); */
		break;
	}
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

char *fread_file(FILE *fp)
{
    char *plast;

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
    {
	bug("Fread_string_new: MAX_STRING %d exceeded.", MAX_STRING);
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

	if(IS_SET(source->room2_flags, ROOM_NOCLONE)) return NULL;

	/* Can't clone a purely virtual room... ever*/
	if(IS_SET(source->room2_flags,ROOM_VIRTUAL_ROOM)) return NULL;

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
	vroom->room_flags = source->room_flags;
	vroom->room2_flags = source->room2_flags | ROOM_VIRTUAL_ROOM;
	REMOVE_BIT(vroom->room2_flags, ROOM_BLUEPRINT);					// Clones can never be "blueprint" rooms
	vroom->sector_type = source->sector_type;
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
		ed2->description = str_dup(ed->description);

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

	/* If enabled, copy all contents */
	if(objects) {
		/* Clone non-takable objects, no contents though...*/
		for(obj = source->contents; obj; obj = obj->next_content) if(!CAN_WEAR(obj,ITEM_TAKE)) {
			obj2 = create_object(obj->pIndexData,0,FALSE);
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
	ROOM_INDEX_DATA *vroom = create_virtual_room_nouid(source, TRUE,links,resets);

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
	if(IS_SET(source->room2_flags,ROOM_VIRTUAL_ROOM))
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
					clone->progs->extract_when_done = TRUE;
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
		if(!environ || environ == clone) environ = get_room_index(11001);

		/* Emptying the room of players will not set off the wilderness check in char_from_room*/
		/*	since no wilderness room will EVER use this.*/
		for(ch = clone->people; ch; ch = ch_next) {
			ch_next = ch->next_in_room;

			if(IS_NPC(ch) && !ch->persist)
				extract_char(ch, TRUE);
			else
			{
				char_from_room(ch);
				char_to_room(ch,environ);
			}
		}

	} else {
		// Dump all corpses or takable items to a special place
		environ = get_room_index(8);		// FIXME: change this to a define

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
		if(!environ || environ == clone) environ = get_room_index(11001);

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
	free_room_index(clone);

//	sprintf(buf,"extract_clone_room(%lu, %lu, %lu) clone extracted", room->vnum, id1, id2);
//	wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);

	return true;
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
//	fprintf(fp, "ExtF %ld\n",		obj->extra_flags);
//	fprintf(fp, "Ext2F %ld\n",		obj->extra2_flags);
//	fprintf(fp, "Ext3F %ld\n",		obj->extra3_flags);
//	fprintf(fp, "Ext4F %ld\n",		obj->extra4_flags);
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

//	fprintf(fp, "Val  %d %d %d %d %d %d %d %d\n",
//		obj->value[0], obj->value[1], obj->value[2], obj->value[3],
//		obj->value[4], obj->value[5], obj->value[6], obj->value[7]);

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
		bug("Failed to add mobile as persistant due to memory issues with 'list_appendlink'.",0);
		if( fBootDb )
			abort();
	} else
		mob->persist = TRUE;
}

void persist_addobject(register OBJ_DATA *obj)
{
	log_stringf("persist_addobject: Adding object %ld to persistance.", obj->pIndexData->vnum);

	if( list_hasdata(persist_objs, obj)) {
		log_stringf("persist_addobject: Object %ld already in persistance.", obj->pIndexData->vnum);
		return;
	}

	if( !list_appendlink(persist_objs, obj) ) {
		bug("Failed to add object as persistant due to memory issues with 'list_appendlink'.",0);
		if( fBootDb )
			abort();
	} else {
		obj->persist = TRUE;
		log_stringf("persist_addobject: Object %ld flagged as persistant.", obj->pIndexData->vnum);
	}
}

void persist_addroom(register ROOM_INDEX_DATA *room)
{
	// Chat rooms cannot be made persistant
	if( room->chat_room || (room->area && room->area->area_who == AREA_CHAT) )
		return;

	// Clone rooms require the source to be flagged as clone persist
	if( room->source && !IS_SET(room->source->room2_flags, ROOM_CLONE_PERSIST))
		return;

	if( list_hasdata(persist_rooms, room)) return;

	if( !list_appendlink(persist_rooms, room) ) {
		bug("Failed to add room as persistant due to memory issues with 'list_appendlink'.",0);
		if( fBootDb )
			abort();
	} else
		room->persist = TRUE;
}

void persist_removemobile(register CHAR_DATA *mob)
{
	list_remlink(persist_mobs, mob);
	mob->persist = FALSE;
}

void persist_removeobject(register OBJ_DATA *obj)
{
	list_remlink(persist_objs, obj);
	obj->persist = FALSE;
}

void persist_removeroom(register ROOM_INDEX_DATA *room)
{
	list_remlink(persist_rooms, room);
	room->persist = FALSE;
}

void persist_save_scriptdata(FILE *fp, PROG_DATA *prog)
{
	pVARIABLE var;

	log_stringf("%s: Saving variables...", __FUNCTION__);
	for( var = prog->vars; var; var = var->next) {
		log_stringf("%s: Variable %s%s", __FUNCTION__, var->name, (var->save ? " - Saving...":""));
		if(var->save)
			variable_fwrite( var, fp );
	}
	log_stringf("%s: Saving variables... Done.", __FUNCTION__);
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
	int i;

	if (multiple && obj->next_content)
		persist_save_object(fp, obj->next_content, multiple);

	log_stringf("persist_save: saving object %08lX:%08lX.", obj->id[0], obj->id[1]);

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
	fprintf(fp, "Extra %ld\n", obj->extra_flags);				// **
	fprintf(fp, "Extra2 %ld\n", obj->extra2_flags);				// **
	fprintf(fp, "Extra3 %ld\n", obj->extra3_flags);				// **
	fprintf(fp, "Extra4 %ld\n", obj->extra4_flags);				// **
	fprintf(fp, "WearFlags %d\n", obj->wear_flags);				// **
	fprintf(fp, "ItemType %d\n", obj->item_type);				// **

	fprintf(fp, "PermExtra %ld\n", obj->extra_flags_perm);				// **
	fprintf(fp, "PermExtra2 %ld\n", obj->extra2_flags_perm);				// **
	fprintf(fp, "PermExtra3 %ld\n", obj->extra3_flags_perm);				// **
	fprintf(fp, "PermExtra4 %ld\n", obj->extra4_flags_perm);				// **

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

	for(i = 0; i < 8; i++)
		fprintf(fp, "Value %d %d\n", i, obj->value[i]);			// **

	if( obj->lock )
	{
		fprintf(fp, "Lock %ld '%s' %d\n",
			obj->lock->key_vnum,
			flag_string(lock_flags, obj->lock->flags),
			obj->lock->pick_chance);
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
	for (paf = obj->catalyst; paf != NULL; paf = paf->next)
	{
		if( IS_NULLSTR(paf->custom_name) )
		{
			fprintf(fp, "%s '%s' %3d %3d %3d\n",
				((paf->where == TO_CATALYST_ACTIVE) ? "CataA" : "Cata"),
				flag_string( catalyst_types, paf->type ),
				paf->level,
				paf->modifier,
				paf->duration);
		}
		else
		{
			fprintf(fp, "%s '%s' %3d %3d %3d %s\n",
				((paf->where == TO_CATALYST_ACTIVE) ? "CataNA" : "CataN"),
				flag_string( catalyst_types, paf->type ),
				paf->level,
				paf->modifier,
				paf->duration,
				paf->custom_name);
		}
	}

	// Extra Descriptions
	for (ed = obj->extra_descr; ed; ed = ed->next)
		fprintf(fp, "ExDe %s~ %s~\n", ed->keyword, ed->description);	// **

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

void persist_save_mobile(FILE *fp, CHAR_DATA *ch)
{
	AFFECT_DATA *paf;
	int i = 0;

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
	fprintf(fp, "Race %s~\n", race_table[ch->race].name);
	fprintf(fp, "Sex  %d\n", ch->sex);
	fprintf(fp, "Levl %d\n", ch->level);
	fprintf(fp, "TLevl %d\n", ch->tot_level);


	// Save location
	if (ch->in_room) {	// **
		if(ch->in_room->wilds)		fprintf(fp, "Vroom %ld %ld %ld\n", ch->in_room->wilds->uid, ch->in_room->x, ch->in_room->y);
		else if(ch->in_room->source)	fprintf(fp, "CloneRoom %ld %ld %ld\n", ch->in_room->source->vnum, ch->in_room->id[0], ch->in_room->id[1]);
		else				fprintf(fp, "Room %ld\n", ch->in_room->vnum);
	} else
		fprintf(fp, "Room %d\n", ROOM_VNUM_DEFAULT);

	if (IS_SITH(ch)) {
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
	fprintf(fp, "Act  %s\n", print_flags(ch->act));
	fprintf(fp, "Act2 %s\n", print_flags(ch->act2));
	fprintf(fp, "AfBy %s\n", print_flags(ch->affected_by));
	fprintf(fp, "AfBy2 %s\n", print_flags(ch->affected_by2));
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
	if (ch->corpse_vnum)
		fprintf(fp, "CorpseVnum %ld\n", ch->corpse_vnum);



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

	// Save Variables
	if( ch->progs )
		persist_save_scriptdata(fp,ch->progs);

	// Save Tokens
	if( ch->tokens )
		persist_save_token(fp, ch->tokens);

	// Contents
	if (ch->carrying)
		persist_save_object(fp, ch->carrying, true);

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
		ex->door.lock.key_vnum,
		flag_string(lock_flags, ex->door.lock.flags),
		ex->door.lock.pick_chance,
		ex->door.rs_lock.key_vnum,
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
	fprintf(fp, "RoomFlags %s~\n", flag_string(room_flags, room->room_flags));
	fprintf(fp, "RoomFlags2 %s~\n", flag_string(room2_flags, room->room2_flags));
	fprintf(fp, "Sector %s~\n", flag_string(sector_flags, room->sector_type));

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
			persist_save_mobile(fp, ch);

	fprintf(fp, "#-ROOM\n\n");
}

bool check_persist_environment( CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room )
{
	if( ch ) {
		if( !IS_NPC(ch) ) return TRUE;	// It's a player

		if( ch->in_room && ch->in_room->persist) return TRUE;

		return check_persist_environment( NULL, NULL, ch->in_room );

	} else if (obj) {
		if( obj->locker )
			return TRUE;	// They are in some player's locker, thus on a player, which is a persistant environment
		else if( obj->in_room ) {
			if( obj->in_room->persist) return TRUE;

			return check_persist_environment( NULL, NULL, obj->in_room );
		} else if( obj->carried_by ) {
			if( !IS_NPC(obj->carried_by ) ) return TRUE;	// Players are a special kind of persistance
			if( obj->carried_by->persist) return TRUE;

			return check_persist_environment( obj->carried_by, NULL, NULL );
		} else if( obj->in_obj ) {
			if( obj->in_obj->persist) return TRUE;

			return check_persist_environment( NULL, obj->in_obj, NULL );
		}
	}

	return FALSE;
}

// Rules for when a persistant entity is saved:
// * Persistant Entities are only saved if they are in a NON-PERSISTANT environment.
// * Persistant Objects save everything.
// * Persistant Mobiles save everything.
// * Persistant Rooms only save scripting information as well as conditional elements, as well as contents (objects and mobiles)
void persist_save(void)
{
	FILE *fp;
	register CHAR_DATA *ch;
	register OBJ_DATA *obj;
	register ROOM_INDEX_DATA *room;
	ITERATOR it;

	log_stringf("persist_save: Saving persistance...");

	if (!(fp = fopen(PERSIST_FILE, "w"))) {
		bug("persist.save: Couldn't open file.",0);
	} else {
		// Save objects
		iterator_start(&it, persist_objs);
		while(( obj = (OBJ_DATA *)iterator_nextdata(&it) )) {
			log_stringf("persist_save: checking to save persistant object %08lX:%08lX.", obj->id[0], obj->id[1]);
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

	log_stringf("persist_save: done.");
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

TOKEN_DATA *persist_load_token(FILE *fp)
{
	TOKEN_DATA *token;
	TOKEN_INDEX_DATA *token_index;
	int vtype;
	long vnum;
	char buf[MSL];
	char *word;
	bool fMatch;

	log_string("persist_load: #TOKEN");

	vnum = fread_number(fp);
	if ((token_index = get_token_index(vnum)) == NULL) {
		sprintf(buf, "persist_load_token: no token index found for vnum %ld", vnum);
		bug(buf, 0);
		return NULL;
	}

	token = new_token();
	token->pIndexData = token_index;
	token->name = str_dup(token_index->name);
	token->description = str_dup(token_index->description);
	token->type = token_index->type;
	token->flags = token_index->flags;
	token->progs = new_prog_data();
	token->progs->progs = token_index->progs;
	token_index->loaded++;
	token->id[0] = token->id[1] = 0;
	token->global_next = global_tokens;
	global_tokens = token;

	variable_copylist(&token_index->index_vars,&token->progs->vars,FALSE);

	for (; ;) {
		word   = feof(fp) ? "#-TOKEN" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "#-TOKEN"))
			break;

		log_stringf("%s: %s", __FUNCTION__, word);

		switch (UPPER(word[0])) {
			case 'T':
				KEY("Timer",	token->timer,		fread_number(fp));
				break;

			case 'U':
				KEY("UId",	token->id[0],		fread_number(fp));
				KEY("UId2",	token->id[1],		fread_number(fp));
				break;

			case 'V':
				if (!str_cmp(word, "Value")) {
					int i = fread_number(fp);
					token->value[i] = fread_number(fp);
					fMatch = TRUE;
				}

				if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
					variable_fread(&token->progs->vars, vtype, fp);
					fMatch = TRUE;
				}

				break;
		}

		if (!fMatch) {
			sprintf(buf, "persist_load_token: no match for word %s", word);
			bug(buf, 0);
			fread_to_eol(fp);
		}
	}

	// Do loading cleanup

	get_token_id(token);

	variable_dynamic_fix_token(token);
	persist_fix_environment_token(token);

	log_string("persist_load: #-TOKEN");

	return token;
}


OBJ_DATA *persist_load_object(FILE *fp)
{
	char buf[MIL];
	ROOM_INDEX_DATA *here = NULL, *deep_here = NULL;
	OBJ_INDEX_DATA *obj_index;
	OBJ_DATA *obj;
	char *word;
	long vnum;
	int vtype;
	bool good = TRUE, fMatch;

	log_string("persist_load: #OBJECT");

	vnum = fread_number(fp);
	obj_index = get_obj_index(vnum);
	if( !obj_index )
		return NULL;

	obj = create_object_noid(obj_index, -1, FALSE);
	if( !obj )
		return NULL;
	obj->version = VERSION_OBJECT_000;
	obj->id[0] = obj->id[1] = 0;

	for (;good;) {
		word = feof(fp) ? "#-OBJECT" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "#-OBJECT"))
			break;

		log_stringf("%s: %s", __FUNCTION__, word);

		switch (UPPER(word[0])) {
			case '*':
				fMatch = TRUE;
				fread_to_eol(fp);
				break;

			// Load up subentities
			case '#':
				if (!str_cmp(word,"#OBJECT")) {
					OBJ_DATA *item = persist_load_object(fp);

					fMatch = TRUE;
					if( item ) {
						obj_to_obj(item, obj);
					} else
						good = FALSE;

					break;
				}
				if (!str_cmp(word,"#TOKEN")) {
					TOKEN_DATA *token = persist_load_token(fp);

					fMatch = TRUE;
					if( token )
						token_to_obj(token, obj);
					else
						good = FALSE;

					break;
				}
				break;

			case 'A':
				// Mobile Affect
				if (!str_cmp(word,"AffMob")) {
					AFFECT_DATA *paf;

					paf = new_affect();

					paf->type = -1;

					paf->where = fread_number(fp);
					paf->group = fread_number(fp);
					paf->level = fread_number(fp);
					paf->duration = fread_number(fp);
					paf->modifier = fread_number(fp);
					paf->location = fread_number(fp);
					if(paf->location == APPLY_SKILL) {
						int sn = skill_lookup(fread_word(fp));
						if(sn < 0) {
							paf->location = APPLY_NONE;
							paf->modifier = 0;
						} else
							paf->location += sn;
					}
					paf->bitvector = fread_number(fp);
					if(obj->version >= VERSION_OBJECT_003)
						paf->bitvector = fread_number(fp);
					paf->next = obj->affected;
					obj->affected = paf;
					fMatch = TRUE;
					break;
				}

				// Object Affect (Skill Number)
				if (!str_cmp(word,"AffObjSk")) {
					AFFECT_DATA *paf;
					int sn;

					paf = new_affect();

					sn = skill_lookup(fread_word(fp));
					if (sn < 0)
						bug("persist_load_object: unknown skill.",0);
					else
						paf->type = sn;

					paf->where = fread_number(fp);
					paf->group = fread_number(fp);
					paf->level = fread_number(fp);
					paf->duration = fread_number(fp);
					paf->modifier = fread_number(fp);
					paf->location = fread_number(fp);
					if(paf->location == APPLY_SKILL) {
						int sn = skill_lookup(fread_word(fp));
						if(sn < 0) {
							paf->location = APPLY_NONE;
							paf->modifier = 0;
						} else
							paf->location += sn;
					}
					paf->bitvector = fread_number(fp);
					if(obj->version >= VERSION_OBJECT_003)
						paf->bitvector = fread_number(fp);
					paf->next = obj->affected;
					obj->affected = paf;
					fMatch = TRUE;
					break;
				}

				// Object Affect (Custom Name)
				if (!str_cmp(word, "AffObjNm")) {
					AFFECT_DATA *paf;
					char *name;

					paf = new_affect();

					name = create_affect_cname(fread_word(fp));
					if (!name) {
						log_string("persist_load_object: could not create affect name.");
						free_affect(paf);
					} else {
						paf->custom_name = name;

						paf->type = -1;
						paf->where = fread_number(fp);
						paf->group = fread_number(fp);
						paf->level = fread_number(fp);
						paf->duration = fread_number(fp);
						paf->modifier = fread_number(fp);
						paf->location = fread_number(fp);
						if(paf->location == APPLY_SKILL) {
							int sn = skill_lookup(fread_word(fp));
							if(sn < 0) {
								paf->location = APPLY_NONE;
								paf->modifier = 0;
							} else
								paf->location += sn;
						}
						paf->bitvector = fread_number(fp);
						if(obj->version >= VERSION_OBJECT_003)
							paf->bitvector = fread_number(fp);
						paf->next = obj->affected;
						obj->affected = paf;
					}
					fMatch = TRUE;
					break;
				}

				break;
			case 'B':
				break;
			case 'C':
				if (!str_cmp(word, "Cata")) {
					AFFECT_DATA *paf;

					paf = new_affect();

					paf->type = flag_value(catalyst_types,fread_word(fp));
					if(paf->type == NO_FLAG) {
						log_string("persist_load_object: invalid catalyst type.");
						free_affect(paf);
					} else {
						paf->where = TO_CATALYST_DORMANT;
						paf->level = fread_number(fp);
						paf->modifier = fread_number(fp);
						paf->duration = fread_number(fp);
						paf->custom_name = NULL;
						paf->next = obj->catalyst;
						obj->catalyst = paf;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word, "CataA")) {
					AFFECT_DATA *paf;

					paf = new_affect();

					paf->type = flag_value(catalyst_types,fread_word(fp));
					if(paf->type == NO_FLAG) {
						log_string("persist_load_object: invalid catalyst type.");
						free_affect(paf);
					} else {
						paf->where = TO_CATALYST_ACTIVE;
						paf->level = fread_number(fp);
						paf->modifier = fread_number(fp);
						paf->duration = fread_number(fp);
						paf->custom_name = NULL;
						paf->next = obj->catalyst;
						obj->catalyst = paf;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word, "CataN")) {
					AFFECT_DATA *paf;

					paf = new_affect();

					paf->type = flag_value(catalyst_types,fread_word(fp));
					if(paf->type == NO_FLAG) {
						log_string("persist_load_object: invalid catalyst type.");
						free_affect(paf);
					} else {
						paf->where = TO_CATALYST_DORMANT;
						paf->level = fread_number(fp);
						paf->modifier = fread_number(fp);
						paf->duration = fread_number(fp);
						paf->custom_name = fread_string_eol(fp);
						paf->next = obj->catalyst;
						obj->catalyst = paf;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word, "CataNA")) {
					AFFECT_DATA *paf;

					paf = new_affect();

					paf->type = flag_value(catalyst_types,fread_word(fp));
					if(paf->type == NO_FLAG) {
						log_string("persist_load_object: invalid catalyst type.");
						free_affect(paf);
					} else {
						paf->where = TO_CATALYST_ACTIVE;
						paf->level = fread_number(fp);
						paf->modifier = fread_number(fp);
						paf->duration = fread_number(fp);
						paf->custom_name = fread_string_eol(fp);
						paf->next = obj->catalyst;
						obj->catalyst = paf;
					}
					fMatch = TRUE;
					break;
				}

				if( !str_cmp(word, "CloneRoom") ) {
					ROOM_INDEX_DATA *src;
					int v = fread_number(fp);
					int a = fread_number(fp);
					int b = fread_number(fp);

					if( (src = get_room_index(v)) )
						here = get_clone_room(src, a, b);

					fMatch = TRUE;
				}
				KEY("Cond",	obj->condition,		fread_number(fp));
				KEY("Cost",	obj->cost,		fread_number(fp));
				break;
			case 'D':
				if( !str_cmp(word, "DeepCloneRoom") ) {
					ROOM_INDEX_DATA *src;
					int v = fread_number(fp);
					int a = fread_number(fp);
					int b = fread_number(fp);

					if( (src = get_room_index(v)) )
						deep_here = get_clone_room(src, a, b);

					fMatch = TRUE;
				}
				if( !str_cmp(word, "DeepRoom") ) {
					long rvnum = fread_number(fp);
					deep_here = get_room_index(rvnum);
					fMatch = TRUE;
				}
				if( !str_cmp(word, "DeepVroom") ) {
					ROOM_INDEX_DATA *r;
					WILDS_DATA *wilds;
					int w = fread_number(fp);
					int x = fread_number(fp);
					int y = fread_number(fp);

					if( (wilds = get_wilds_from_uid(NULL, w)) ) {
						if( !(r = get_wilds_vroom(wilds, x, y)) )
							r = create_wilds_vroom(wilds, x, y);

						deep_here = r;
					}

					fMatch = TRUE;
				}
				break;
			case 'E':
				KEY("Enchanted",	obj->num_enchanted,	fread_number(fp));
				KEY("Extra",		obj->extra_flags,	fread_number(fp));
				KEY("Extra2",		obj->extra2_flags,	fread_number(fp));
				KEY("Extra3",		obj->extra3_flags,	fread_number(fp));
				KEY("Extra4",		obj->extra4_flags,	fread_number(fp));
				if ( !str_cmp(word,"ExDe") ) {
					EXTRA_DESCR_DATA *ed;

					ed = new_extra_descr();
					ed->keyword = fread_string(fp);
					ed->description	= fread_string(fp);
					ed->next = obj->extra_descr;
					obj->extra_descr = ed;
					fMatch = TRUE;
				}
				break;
			case 'F':
				KEY("Fixed",		obj->times_fixed,	fread_number(fp));
				KEY("Fragility",	obj->fragility,		fread_number(fp));
				KEY("FullDesc",		obj->full_description,	fread_string(fp));
				break;
			case 'G':
				break;
			case 'H':
				break;
			case 'I':
				KEY("ItemType",		obj->item_type,		fread_number(fp));
				break;
			case 'J':
				break;
			case 'K':
				break;
			case 'L':
				KEY("LastWearLoc",	obj->last_wear_loc,	fread_number(fp));
				KEY("Level",		obj->level,		fread_number(fp));
				KEY("LoadedBy",		obj->loaded_by,		fread_string(fp));
				if( !str_cmp(word,"Lock") )
				{
					if( !obj->lock )
					{
						obj->lock = new_lock_state();
					}

					obj->lock->key_vnum = fread_number(fp);
					obj->lock->flags = script_flag_value(lock_flags, fread_word(fp));
					obj->lock->pick_chance = fread_number(fp);

					fMatch = TRUE;
					break;
				}
				FKEY("Locker",		obj->locker);
				KEY("LongDesc",		obj->description,	fread_string(fp));
				break;
			case 'M':
				break;
			case 'N':
				KEY("Name",		obj->name,			fread_string(fp));
				break;
			case 'O':
				KEY("OldDescr",		obj->old_description,		fread_string(fp));
				KEY("OldFullDescr",	obj->old_full_description,	fread_string(fp));
				KEY("OldName",		obj->old_name,				fread_string(fp));
				KEY("OldShort",		obj->old_short_descr,		fread_string(fp));
				KEY("Owner",		obj->owner,					fread_string(fp));
				KEY("OwnerName",	obj->owner_name,			fread_string(fp));
				KEY("OwnerShort",	obj->owner_short,			fread_string(fp));
				break;
			case 'P':
				KEY("PermExtra",		obj->extra_flags_perm,	fread_number(fp));
				KEY("PermExtra2",		obj->extra2_flags_perm,	fread_number(fp));
				KEY("PermExtra3",		obj->extra3_flags_perm,	fread_number(fp));
				KEY("PermExtra4",		obj->extra4_flags_perm,	fread_number(fp));
				KEY("PermWeapon",		obj->weapon_flags_perm,	fread_number(fp));
				FKEY("Persist",		obj->persist);
				break;
			case 'Q':
				break;
			case 'R':
				if( !str_cmp(word, "Room") ) {
					long rvnum = fread_number(fp);
					here = get_room_index(rvnum);
					fMatch = TRUE;
				}
				break;
			case 'S':
				KEY("ShortDesc",	obj->short_descr,	fread_string(fp));
				if (!str_cmp(word, "SpellNew")) {
					int sn;
					SPELL_DATA *spell;

					fMatch = TRUE;
					if ( (sn = skill_lookup(fread_string(fp))) > 0 ) {
						spell = new_spell();
						spell->sn = sn;
						spell->level = fread_number(fp);
						spell->repop = fread_number(fp);

						spell->next = obj->spells;
						obj->spells = spell;
					} else {
						sprintf(buf, "Bad spell name for %s (%ld).", obj->short_descr, obj->pIndexData->vnum);
						bug(buf,0);
					}
				}

				break;
			case 'T':
				KEY("Timer",		obj->timer,			fread_number(fp));
				KEY("TimesAllowedFixed",obj->times_allowed_fixed,	fread_number(fp));
				break;
			case 'U':
				KEY("UID",		obj->id[0],		fread_number(fp));
				KEY("UID2",		obj->id[1],		fread_number(fp));
				break;
			case 'V':
				if( !str_cmp(word, "Value") ) {
					int idx = fread_number(fp);
					int val = fread_number(fp);

					if( idx >= 0 && idx < 8 )
						obj->value[idx] = val;

					fMatch = TRUE;
				}
				KEY("Version",		obj->version,		fread_number(fp));
				if( !str_cmp(word, "Vroom") ) {
					ROOM_INDEX_DATA *r;
					WILDS_DATA *wilds;
					int w = fread_number(fp);
					int x = fread_number(fp);
					int y = fread_number(fp);

					if( (wilds = get_wilds_from_uid(NULL, w)) ) {
						if( !(r = get_wilds_vroom(wilds, x, y)) )
							r = create_wilds_vroom(wilds, x, y);

						here = r;
					}

					fMatch = TRUE;
				}

				if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
					variable_fread(&obj->progs->vars, vtype, fp);
					fMatch = TRUE;
				}
				break;
			case 'W':
				KEY("WearFlags",	obj->wear_flags,	fread_number(fp));
				KEY("WearLoc",		obj->wear_loc,		fread_number(fp));
				KEY("Weight",		obj->weight,		fread_number(fp));
				break;
			case 'X':
				break;
			case 'Y':
				break;
			case 'Z':
				break;
		}

		if (!fMatch)
			fread_to_eol(fp);
	}


	get_obj_id(obj);
	fix_object(obj);

	if( !here ) here = deep_here;

	if( here ) obj->in_room = here;

	if(obj->persist) persist_addobject(obj);

	variable_dynamic_fix_object(obj);
	persist_fix_environment_object(obj);

	log_string("persist_load: #-OBJECT");

	return obj;
}

CHAR_DATA *persist_load_mobile(FILE *fp)
{
	char buf[MSL];
	MOB_INDEX_DATA *index;
	CHAR_DATA *ch;
	ROOM_INDEX_DATA *here = NULL, *deep_here = NULL;
	char *word;
	long vnum;
	int i, sn;
	int vtype;
	bool good = TRUE, fMatch;

	log_string("persist_load: #MOBILE");

	vnum = fread_number(fp);
	index = get_mob_index(vnum);
	if( !index )
		return NULL;

	ch = create_mobile(index, TRUE);
	if( !ch )
		return NULL;
	ch->version = VERSION_MOBILE_000;
	ch->id[0] = ch->id[1] = 0;
	for(i = 0; i < MAX_STATS; i++)
		ch->dirty_stat[i] = TRUE;

	for (;good;) {
		word = feof(fp) ? "#-MOBILE" : fread_word(fp);
		fMatch = FALSE;

		if (!str_cmp(word, "#-MOBILE"))
			break;

		log_stringf("%s: %s", __FUNCTION__, word);

		switch (UPPER(word[0])) {
			case '*':
				fMatch = TRUE;
				fread_to_eol(fp);
				break;

			// Load up subentities
			case '#':
				if (!str_cmp(word,"#OBJECT")) {
					OBJ_DATA *item = persist_load_object(fp);

					fMatch = TRUE;
					if( item )
						obj_to_char(item, ch);
					else
						good = FALSE;

					if( item->wear_loc != WEAR_NONE ) {
						list_addlink(ch->lworn, item);
					}

					break;
				}
				if (!str_cmp(word,"#TOKEN")) {
					TOKEN_DATA *token = persist_load_token(fp);

					fMatch = TRUE;
					if( token )
						token_to_char(token, ch);
					else
						good = FALSE;

					break;
				}
				if (!str_cmp(word,"#SHOP")) {
					SHOP_DATA *shop = read_shop_new(fp);

					fMatch = TRUE;
					if( shop )
						ch->shop = shop;
					else
						good = FALSE;

					break;
				}
				break;
			case 'A':
				if(IS_KEY("ACs")) {
					for(i = 0; i < 4; ch->armour[i++] = fread_number(fp));

					fMatch = TRUE;
				}
				KEY("Act",		ch->act,			fread_flag(fp));
				KEY("Act2",		ch->act2,			fread_flag(fp));
				KEY("AfBy",		ch->affected_by,	fread_flag(fp));
				KEY("AfBy2",	ch->affected_by2,	fread_flag(fp));

				if(IS_KEY("Affcg")) {
					AFFECT_DATA *paf = new_affect();

					if( paf ) {
						sn = skill_lookup(fread_word(fp));
						if (sn < 0)
							log_string("fread_char: unknown skill.");
						else
		                    paf->type = sn;
						paf->custom_name = NULL;
						paf->group = flag_value(affgroup_mobile_flags, fread_word(fp));
						if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
						paf->where  = fread_number(fp);
						paf->level      = fread_number(fp);
						paf->duration   = fread_number(fp);
						paf->modifier   = fread_number(fp);
						paf->location   = fread_number(fp);
						if(paf->location == APPLY_SKILL) {
							int sn = skill_lookup(fread_word(fp));
							if(sn < 0) {
								paf->location = APPLY_NONE;
								paf->modifier = 0;
							} else
								paf->location += sn;
						}
						paf->bitvector = fread_number(fp);
						paf->bitvector2 = fread_number(fp);
						if( ch->version >= VERSION_MOBILE_001)
							paf->slot = fread_number(fp);

						paf->next = ch->affected;
						ch->affected = paf;
					}
					fMatch = TRUE;
				}

				if(IS_KEY("Affcgn")) {
					AFFECT_DATA *paf = new_affect();

					if( paf ) {
						paf->custom_name = create_affect_cname(fread_word(fp));
						if(!paf->custom_name) {
							free_affect(paf);
						} else {
							paf->type = -1;
							paf->group = flag_value(affgroup_mobile_flags, fread_word(fp));
							if(paf->group == NO_FLAG) paf->group = AFFGROUP_MAGICAL;
							paf->where  = fread_number(fp);
							paf->level      = fread_number(fp);
							paf->duration   = fread_number(fp);
							paf->modifier   = fread_number(fp);
							paf->location   = fread_number(fp);
							if(paf->location == APPLY_SKILL) {
								int sn = skill_lookup(fread_word(fp));
								if(sn < 0) {
									paf->location = APPLY_NONE;
									paf->modifier = 0;
								} else
									paf->location += sn;
							}
							paf->bitvector = fread_number(fp);
							paf->bitvector2 = fread_number(fp);
							if( ch->version >= VERSION_MOBILE_001)
								paf->slot = fread_number(fp);
							paf->next = ch->affected;
							ch->affected = paf;
						}
					}
					fMatch = TRUE;
				}

				KEY("Alig",		ch->alignment,		fread_number(fp));
				if(IS_KEY("AMod")) {
					set_mod_stat(ch, STAT_STR, fread_number(fp));
					set_mod_stat(ch, STAT_INT, fread_number(fp));
					set_mod_stat(ch, STAT_WIS, fread_number(fp));
					set_mod_stat(ch, STAT_DEX, fread_number(fp));
					set_mod_stat(ch, STAT_CON, fread_number(fp));
					fMatch = TRUE;
				}

				if(IS_KEY("Attr")) {
					set_perm_stat(ch, STAT_STR, fread_number(fp));
					set_perm_stat(ch, STAT_INT, fread_number(fp));
					set_perm_stat(ch, STAT_WIS, fread_number(fp));
					set_perm_stat(ch, STAT_DEX, fread_number(fp));
					set_perm_stat(ch, STAT_CON, fread_number(fp));
					fMatch = TRUE;
				}
				break;
			case 'B':
				break;
			case 'C':
				if(IS_KEY("CloneRoom")) {
					long v = fread_number(fp);
					unsigned long id1 = fread_number(fp);
					unsigned long id2 = fread_number(fp);

					ROOM_INDEX_DATA *source = get_room_index(v);


					here = get_clone_room(source, id1, id2);

					fMatch = TRUE;
				}
				KEY("Comm",			ch->comm,			fread_flag(fp));
				KEY("CorpseType",	ch->corpse_type,	fread_number(fp));
				KEY("CorpseVnum",	ch->corpse_vnum,	fread_number(fp));
//				KEY("CorpseZombie",	ch->zombie,		fread_number(fp));
				break;
			case 'D':
				KEY("Dam",				ch->damroll,			fread_number(fp));
				FKEY("Dead",			ch->dead);
				KEY("DefaultPos",		ch->default_pos,		fread_number(fp));
				SKEY("Desc",			ch->description);
				KEY("DeathTimeLeft",	ch->time_left_death,	fread_number(fp));

				if( !str_cmp(word, "DeepCloneRoom") ) {
					ROOM_INDEX_DATA *src;
					int v = fread_number(fp);
					int a = fread_number(fp);
					int b = fread_number(fp);

					if( (src = get_room_index(v)) )
						deep_here = get_clone_room(src, a, b);

					fMatch = TRUE;
				}
				if( !str_cmp(word, "DeepRoom") ) {
					long rvnum = fread_number(fp);
					deep_here = get_room_index(rvnum);
					fMatch = TRUE;
				}
				if( !str_cmp(word, "DeepVroom") ) {
					ROOM_INDEX_DATA *r;
					WILDS_DATA *wilds;
					int w = fread_number(fp);
					int x = fread_number(fp);
					int y = fread_number(fp);

					if( (wilds = get_wilds_from_uid(NULL, w)) ) {
						if( !(r = get_wilds_vroom(wilds, x, y)) )
							r = create_wilds_vroom(wilds, x, y);

						deep_here = r;
					}

					fMatch = TRUE;
				}

				KEY("DeityPnts",		ch->deitypoints,		fread_number(fp));
				break;
			case 'E':
				KEY("Exp",		ch->exp,		fread_number(fp));
				break;
			case 'F':
				break;
			case 'G':
				KEY("Gold",		ch->gold,		fread_number(fp));
				break;
			case 'H':
				if(IS_KEY("HMV")) {
					ch->hit = fread_number(fp);
					ch->max_hit = fread_number(fp);
					ch->mana = fread_number(fp);
					ch->max_mana = fread_number(fp);
					ch->move = fread_number(fp);
					ch->max_move = fread_number(fp);
					fMatch = TRUE;
				}
				KEY("Hit",	ch->hitroll,	fread_number(fp));
				KEY("Home",	ch->home,		fread_number(fp));
				break;
			case 'I':
				KEY("Immune",	ch->imm_flags,	fread_flag(fp));
				KEY("ImmunePerm",	ch->imm_flags_perm,	fread_flag(fp));
				break;
			case 'J':
				break;
			case 'K':
				break;
			case 'L':
				KEY("Levl",			ch->level,		fread_number(fp));
				SKEY("LnD",			ch->long_descr);
				KEY("LostParts",	ch->lostparts,	fread_flag(fp));
				break;
			case 'M':
				SKEY("Material",	ch->material);
				break;
			case 'N':
				SKEY("Name",		ch->name);
				break;
			case 'O':
				KEY("OffFlags",		ch->off_flags,	fread_flag(fp));
				SKEY("Owner",		ch->owner);
				break;
			case 'P':
				KEY("Parts",		ch->parts,		fread_number(fp));
				FKEY("Persist",		ch->persist);
				KEY("Pneuma",		ch->pneuma,		fread_number(fp));
				KEY("Pos",			ch->position,	fread_number(fp));
				KEY("Prac",			ch->practice,	fread_number(fp));
				break;
			case 'Q':
				KEY("QuestPnts",	ch->questpoints,		fread_number(fp));
				break;
			case 'R':
				if(IS_KEY("Race")) {
					char *name = fread_string(fp);

					// Default to Human if the race is not found.
					if( !(ch->race = race_lookup(name)) )
						ch->race = grn_human;

					fMatch = TRUE;
				}

				if(IS_KEY("RepopRoom")) {
					ch->recall.id[0] = fread_number(fp);
					ch->recall.id[1] =
					ch->recall.id[2] =
					ch->recall.id[3] = 0;
					fMatch = TRUE;
				}

				if(IS_KEY("RepopRoomC")) {
					ch->recall.id[0] = fread_number(fp);
					ch->recall.id[1] = fread_number(fp);
					ch->recall.id[2] = fread_number(fp);
					ch->recall.id[3] = 0;
					fMatch = TRUE;
				}

				if(IS_KEY("RepopRoomW")) {
					ch->recall.id[0] = fread_number(fp);
					ch->recall.id[1] = fread_number(fp);
					ch->recall.id[2] = fread_number(fp);
					ch->recall.id[3] = fread_number(fp);
					fMatch = TRUE;
				}

				KEY("Resist",	ch->res_flags,	fread_flag(fp));
				KEY("ResistPerm",	ch->res_flags_perm,	fread_flag(fp));

				if(IS_KEY("Room")) {
					here = get_room_index(fread_number(fp));

					fMatch = TRUE;
				}
				break;
			case 'S':
				KEY("Save",		ch->saving_throw,	fread_number(fp));
				KEY("Sex",		ch->sex,		fread_number(fp));
				SKEY("ShD",		ch->short_descr);
				KEY("Silv",		ch->silver,		fread_number(fp));
				KEY("Size",		ch->size,		fread_number(fp));
				//SKEY("Skeywds",	ch->skeywds);
				KEY("StartPos",	ch->start_pos,	fread_number(fp));
				break;
			case 'T':
				KEY("TLevl",	ch->tot_level,		fread_number(fp));

				if( !str_prefix("Toxn", word) ) {
					int toxin;
					for(toxin = 0; toxin < MAX_TOXIN && str_cmp(word+4, toxin_table[toxin].name); toxin++);

					if( toxin < MAX_TOXIN)
						ch->toxin[toxin] = fread_number(fp);
					else {
						sprintf(buf,"%s:%s bad toxin type", __FILE__, __FUNCTION__);
						bug(buf, 0);
						fread_to_eol(fp);
					}
					fMatch = TRUE;
				}

				KEY("Trai",		ch->train,	fread_number(fp));
				break;
			case 'U':
				KEY("UID",		ch->id[0],		fread_number(fp));
				KEY("UID2",		ch->id[1],		fread_number(fp));
				break;
			case 'V':
				KEY("Version",	ch->version,	fread_number(fp));

				if( !str_cmp(word, "Vroom") ) {
					ROOM_INDEX_DATA *r;
					WILDS_DATA *wilds;
					int w = fread_number(fp);
					int x = fread_number(fp);
					int y = fread_number(fp);

					if( (wilds = get_wilds_from_uid(NULL, w)) ) {
						if( !(r = get_wilds_vroom(wilds, x, y)) )
							r = create_wilds_vroom(wilds, x, y);

						here = r;
					}

					fMatch = TRUE;
				}

				// Variables
				if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
					variable_fread(&ch->progs->vars, vtype, fp);
					fMatch = TRUE;
				}

				KEY("Vuln",	ch->vuln_flags,	fread_flag(fp));
				KEY("VulnPerm",	ch->vuln_flags_perm,	fread_flag(fp));

				break;
			case 'W':
				//KEY("Wealth",	ch->wealth,	fread_number(fp));
				KEY("Wimp",		ch->wimpy,	fread_number(fp));
				break;
			case 'X':
				break;
			case 'Y':
				break;
			case 'Z':
				break;
		}

		if (!fMatch)
			fread_to_eol(fp);
	}


	get_mob_id(ch);

	if( !here ) here = deep_here;

	if( !here ) here = get_room_index(ROOM_VNUM_DEFAULT);

	if( here ) ch->in_room = here;

	if(ch->persist) persist_addmobile(ch);

	variable_dynamic_fix_mobile(ch);
	persist_fix_environment_mobile(ch);

	log_string("persist_load: #-MOBILE");

	return ch;
}

EXIT_DATA *persist_load_exit(FILE *fp)
{
//	char buf[MSL];
	EXIT_DATA *ex;
	char *word;
	bool fMatch;

	log_string("persist_load: #EXIT");

	ex = new_exit();
	if( !ex ) return NULL;

	ex->orig_door = parse_direction(fread_word(fp));
	log_stringf("%s: ex->orig_door = %d", __FUNCTION__, ex->orig_door);

	for (;;) {
		word = feof(fp) ? "#-EXIT" : fread_word(fp);
		fMatch = FALSE;

		log_stringf("%s: %s", __FUNCTION__, word);

		if (!str_cmp(word, "#-EXIT"))
			break;

		switch (UPPER(word[0])) {
			case '*':
				fMatch = TRUE;
				fread_to_eol(fp);
				break;

			// Load up subentities
			case '#':
				break;
			case 'A':
				break;
			case 'B':
				break;
			case 'C':
				break;
			case 'D':
				if( !str_cmp(word, "DestRoom") ) {
					ROOM_INDEX_DATA *room = NULL, *clone;
					int w = fread_number(fp);
					int x = fread_number(fp);
					int y = fread_number(fp);
					int z = fread_number(fp);

					if( w > 0 ) {	// By this point, ALL wilds should be loaded
						ex->wilds.wilds_uid = w;
						ex->wilds.x = x;
						ex->wilds.y = y;
					} else {
						if( y > 0 || z > 0 ) {		// Not guaranteed that the clone room has been created
							room = get_room_index( x );

							if( room ) {
								//log_string("get_clone_room: persist_load_exit");
								if( !(clone = get_clone_room( room, y, z )) ) {
									// Create the room
									if( (clone = create_virtual_room_nouid(room, FALSE, FALSE, FALSE)) ) {
										clone->id[0] = y;
										clone->id[1] = z;
									}
								}
								room = clone;
							}
						} else
							room = get_room_index( x );
						ex->u1.to_room = room;
					}

					fMatch = TRUE;
					break;
				}

				if( !str_cmp(word, "Door") ) {
					ex->door.rs_lock.key_vnum = fread_number(fp);
					ex->door.rs_lock.flags = 0;
					ex->door.rs_lock.pick_chance = 100;
					ex->door.lock = ex->door.rs_lock;
					ex->door.strength = fread_number(fp);
					fMatch = TRUE;
					break;
				}

				if( !str_cmp(word, "DoorLock") ) {
					ex->door.lock.key_vnum = fread_number(fp);
					ex->door.lock.flags = script_flag_value(lock_flags, fread_string(fp));
					if( ex->door.lock.flags == NO_FLAG ) ex->door.lock.flags = 0;
					ex->door.lock.pick_chance = fread_number(fp);

					ex->door.rs_lock.key_vnum = ex->door.lock.key_vnum;
					ex->door.rs_lock.flags = script_flag_value(lock_flags, fread_string(fp));
					if( ex->door.rs_lock.flags == NO_FLAG ) ex->door.rs_lock.flags = 0;
					ex->door.rs_lock.pick_chance = fread_number(fp);

					ex->door.strength = fread_number(fp);
					fMatch = TRUE;
					break;
				}

				if( !str_cmp(word, "DoorLockReset") ) {
					ex->door.lock.key_vnum = fread_number(fp);
					ex->door.lock.flags = script_flag_value(lock_flags, fread_string(fp));
					if( ex->door.lock.flags == NO_FLAG ) ex->door.lock.flags = 0;
					ex->door.lock.pick_chance = fread_number(fp);

					ex->door.rs_lock.key_vnum = fread_number(fp);
					ex->door.rs_lock.flags = script_flag_value(lock_flags, fread_string(fp));
					if( ex->door.rs_lock.flags == NO_FLAG ) ex->door.rs_lock.flags = 0;
					ex->door.rs_lock.pick_chance = fread_number(fp);

					ex->door.strength = fread_number(fp);
					fMatch = TRUE;
					break;
				}

				SKEY("DoorMat", ex->door.material);
				break;
			case 'E':
				break;
			case 'F':
				FVDKEY("Flags", ex->exit_info, fread_string(fp), exit_flags, NO_FLAG, 0);
				break;
			case 'G':
				break;
			case 'H':
				break;
			case 'I':
				break;
			case 'J':
				break;
			case 'K':
				SKEY("Keyword",	ex->keyword);
				break;
			case 'L':
				SKEY("LongDesc",	ex->long_desc);
				break;
			case 'M':
				break;
			case 'N':
				break;
			case 'O':
				break;
			case 'P':
				break;
			case 'Q':
				break;
			case 'R':
				FVDKEY("ResetFlags", ex->rs_flags, fread_string(fp), exit_flags, NO_FLAG, 0);
				break;
			case 'S':
				SKEY("ShortDesc",	ex->short_desc);
				break;
			case 'T':
				break;
			case 'U':
				break;
			case 'V':
				break;
			case 'W':
				break;
			case 'X':
				break;
			case 'Y':
				break;
			case 'Z':
				break;
		}

		if (!fMatch)
			fread_to_eol(fp);
	}

	log_string("persist_load: #-EXIT");

	return ex;
}

ROOM_INDEX_DATA *persist_load_room(FILE *fp, char rtype)
{
	char buf[MSL];
	ROOM_INDEX_DATA *room;
	WILDS_DATA *wilds;
	long vnum;
	int w, x, y, z;
	char *word;
	int vtype;
	bool good = TRUE;
	bool fMatch;

	if( rtype == 'R' ) {
		log_string("persist_load: #ROOM");
		vnum = fread_number(fp);

		room = get_room_index(vnum);

		if( !room ) {
			sprintf(buf, "persist_load_room: undefined room index at vnum %ld.", vnum);
			bug(buf,0);
			return NULL;
		}
	} else if( rtype == 'V' ) {
		log_string("persist_load: #VROOM");
		w = fread_number(fp);
		x = fread_number(fp);
		y = fread_number(fp);
		z = fread_number(fp);
		wilds = get_wilds_from_uid(NULL, w);

		if( !wilds ) {
			sprintf(buf, "persist_load_room: undefined wilds uid %d.", w);
			bug(buf,0);
			return NULL;
		}

		room = get_wilds_vroom(wilds, x, y);
		if( !room ) {
			room = create_wilds_vroom(wilds, x, y);

			if( !room ) {
				sprintf(buf, "persist_load_room: unable to create vroom for wilds %d at (%d,%d).", w, x, y);
				bug(buf,0);
				return NULL;
			}
		}

		room->z = z;

	} else if( rtype == 'C' ) {
		ROOM_INDEX_DATA *source;

		log_string("persist_load: #CROOM");

		vnum = fread_number(fp);

		source = get_room_index(vnum);

		if( !source ) {
			fread_to_eol(fp);
			sprintf(buf, "persist_load_room: undefined room index at vnum %ld.", vnum);
			bug(buf,0);
			return NULL;
		}

		x = fread_number(fp);
		y = fread_number(fp);

		// Find the clone
		//log_string("get_clone_room: persist_load_room");
		room = get_clone_room(source,x,y);
		if( !room ) {
			room = create_virtual_room_nouid( source, FALSE, FALSE, FALSE );
			if( !room ) {
				sprintf(buf, "persist_load_room: could not create clone room for %ld with uid %08X:%08X.", vnum, x, y);
				bug(buf,0);
				return NULL;
			}

			room->id[0] = x;
			room->id[1] = y;
		}

	} else
		return NULL;

	room->version = VERSION_ROOM_000;

	for (;good;) {
		word = feof(fp) ? "#-ROOM" : fread_word(fp);
		fMatch = FALSE;

		log_stringf("%s: %s", __FUNCTION__, word);

		if (!str_cmp(word, "#-ROOM"))
			break;

		switch (UPPER(word[0])) {
			case '*':
				fMatch = TRUE;
				fread_to_eol(fp);
				break;

			// Load up subentities
			case '#':
				if (!str_cmp(word,"#EXIT")) {
					EXIT_DATA *ex = persist_load_exit(fp);

					fMatch = TRUE;
					if( ex ) {
						if( room->exit[ex->orig_door] )
							free_exit(room->exit[ex->orig_door]);

						ex->from_room = room;
						room->exit[ex->orig_door] = ex;
					} else
						good = FALSE;

					break;
				}
				if (!str_cmp(word,"#MOBILE")) {
					CHAR_DATA *mob = persist_load_mobile(fp);

					fMatch = TRUE;
					if( mob ) {
						char_to_room(mob, room);
					} else
						good = FALSE;

					break;
				}
				if (!str_cmp(word,"#OBJECT")) {
					OBJ_DATA *item = persist_load_object(fp);

					fMatch = TRUE;
					if( item ) {
						obj_to_room(item, room);
					} else
						good = FALSE;

					break;
				}
				if (!str_cmp(word,"#TOKEN")) {
					TOKEN_DATA *token = persist_load_token(fp);

					fMatch = TRUE;
					if( token )
						token_to_room(token, room);
					else
						good = FALSE;

					break;
				}
				break;
			case 'A':
				break;
			case 'B':
				break;
			case 'C':
				break;
			case 'D':
				SKEY("Desc",	room->description);
				break;
			case 'E':
				if (!str_cmp(word,"EnvironMOB")) {
					CHAR_DATA *mob;

					x = fread_number(fp);
					y = fread_number(fp);

					mob = idfind_mobile(x,y);
					if(mob) {
						room_to_environment(room, mob, NULL, NULL, NULL);
					} else {
						room->environ_type = -ENVIRON_MOBILE;
						room->environ.clone.source = NULL;
						room->environ.clone.id[0] = x;
						room->environ.clone.id[1] = y;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word,"EnvironOBJ")) {
					OBJ_DATA *obj;

					x = fread_number(fp);
					y = fread_number(fp);

					obj = idfind_object(x,y);
					if(obj) {
						room_to_environment(room, NULL, obj, NULL, NULL);
					} else {
						room->environ_type = -ENVIRON_OBJECT;
						room->environ.clone.source = NULL;
						room->environ.clone.id[0] = x;
						room->environ.clone.id[1] = y;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word,"EnvironTOK")) {
					TOKEN_DATA *token;

					x = fread_number(fp);
					y = fread_number(fp);

					token = idfind_token(x,y);
					if(token) {
						room_to_environment(room, NULL, NULL, NULL, token);
					} else {
						room->environ_type = -ENVIRON_OBJECT;
						room->environ.clone.source = NULL;
						room->environ.clone.id[0] = x;
						room->environ.clone.id[1] = y;
					}
					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word,"EnvironCROOM")) {
					ROOM_INDEX_DATA *source_room, *environ_room;

					vnum = fread_number(fp);
					x = fread_number(fp);
					y = fread_number(fp);

					source_room = get_room_index(vnum);
					if(source_room) {
						environ_room = get_clone_room(source_room,x,y);

						if(environ_room) {
							room_to_environment(room, NULL, NULL, environ_room, NULL);
						} else {
							// This might not have been loaded yet!
							room->environ_type = -ENVIRON_ROOM;
							room->environ.clone.source = source_room;
							room->environ.clone.id[0] = x;
							room->environ.clone.id[1] = y;
						}
					}

					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word,"EnvironVROOM")) {
					ROOM_INDEX_DATA *environ_room;

					w = fread_number(fp);
					x = fread_number(fp);
					y = fread_number(fp);
					z = fread_number(fp);

					wilds = get_wilds_from_uid(NULL, w);

					if( wilds ) {
						environ_room = get_wilds_vroom(wilds, x, y);
						if( !environ_room )
							environ_room = create_wilds_vroom(wilds, x, y);

						if( environ_room ) {
							environ_room->z = z;
							room_to_environment(room, NULL, NULL, environ_room, NULL);
						}
					}

					fMatch = TRUE;
					break;
				}
				if (!str_cmp(word,"EnvironROOM")) {
					ROOM_INDEX_DATA *environ_room = get_room_index(fread_number(fp));
					if(environ_room)
						room_to_environment(room, NULL, NULL, environ_room, NULL);

					fMatch = TRUE;
					break;
				}
				break;
			case 'F':
				break;
			case 'G':
				break;
			case 'H':
				KEY("HealRate",		room->heal_rate,	fread_number(fp));
				break;
			case 'I':
				break;
			case 'J':
				break;
			case 'K':
				break;
			case 'L':
				KEY("Locale",		room->locale,	fread_number(fp));
				break;
			case 'M':
				KEY("ManaRate",		room->mana_rate,	fread_number(fp));
				KEY("MoveRate",		room->move_rate,	fread_number(fp));
				break;
			case 'N':
				SKEY("Name",		room->name);
				break;
			case 'O':
				SKEY("Owner",		room->owner);
				break;
			case 'P':
				FKEY("Persist",		room->persist);
				break;
			case 'Q':
				break;
			case 'R':
				FVDKEY("RoomFlags", room->room_flags, fread_string(fp), room_flags, NO_FLAG, 0);
				FVDKEY("RoomFlags2", room->room2_flags, fread_string(fp), room2_flags, NO_FLAG, 0);
				if( !str_cmp(word, "RoomRecall") ) {
					room->recall.wuid = fread_number(fp);
					room->recall.id[0] = fread_number(fp);
					room->recall.id[1] = fread_number(fp);
					room->recall.id[2] = fread_number(fp);
					fMatch = TRUE;
					break;
				}
				break;
			case 'S':
				FVDKEY("Sector", room->sector_type, fread_string(fp), sector_flags, NO_FLAG, SECT_INSIDE);
				break;
			case 'T':
				break;
			case 'U':
				break;
			case 'V':
				if( (vtype = variable_fread_type(word)) != VAR_UNKNOWN ) {
					variable_fread(&room->progs->vars, vtype, fp);
					fMatch = TRUE;
				}
				if( !str_cmp(word, "ViewWilds") ) {
					w = fread_number(fp);

					wilds = get_wilds_from_uid(NULL, w);

					// This is non-fatal if non-existant.  It will just clear it.
					if( !wilds ) {
						sprintf(buf, "persist_load_room: undefined wilds UID for viewwilds %d.", w);
						bug(buf,0);
					}

					room->viewwilds = wilds;

					fMatch = TRUE;
					break;
				}
				break;
			case 'W':
				break;
			case 'X':
				if( !str_cmp(word, "XYZ") ) {
					if( room->wilds ) {
						fread_to_eol(fp);
						sprintf(buf, "persist_load_room: XYZ coordinates found for wilds room %ld @ (%ld, %ld).", room->wilds->uid, room->x, room->y);
						bug(buf,0);
					} else {
						room->x = fread_number(fp);
						room->y = fread_number(fp);
						room->z = fread_number(fp);
					}
					fMatch = TRUE;
					break;
				}
				break;
			case 'Y':
				break;
			case 'Z':
				break;
		}

		if (!fMatch)
			fread_to_eol(fp);
	}

	if( room->version < VERSION_ROOM_002 )
	{

		// Correct exits
		for( int e = 0; e < MAX_DIR; e++)
		{
			EXIT_DATA *ex = room->exit[e];

			if( !ex ) continue;

			// Correct RESETS
			if( IS_SET(ex->rs_flags, VR_002_EX_LOCKED) )
			{
				SET_BIT(ex->door.rs_lock.flags, LOCK_LOCKED);
			}

			if( IS_SET(ex->rs_flags, VR_002_EX_PICKPROOF) )
			{
				ex->door.rs_lock.pick_chance = 0;
			}
			else if( IS_SET(ex->rs_flags, VR_002_EX_INFURIATING) )
			{
				ex->door.rs_lock.pick_chance = 10;
			}
			else if( IS_SET(ex->rs_flags, VR_002_EX_HARD) )
			{
				ex->door.rs_lock.pick_chance = 40;
			}
			else if( IS_SET(ex->rs_flags, VR_002_EX_EASY) )
			{
				ex->door.rs_lock.pick_chance = 80;
			}
			else
			{
				ex->door.rs_lock.pick_chance = 100;
			}

			REMOVE_BIT(ex->rs_flags, (VR_002_EX_LOCKED|VR_002_EX_PICKPROOF|VR_002_EX_INFURIATING|VR_002_EX_HARD|VR_002_EX_EASY));

			// Correct Active
			if( IS_SET(ex->exit_info, VR_002_EX_LOCKED) )
			{
				SET_BIT(ex->door.lock.flags, LOCK_LOCKED);
			}

			if( IS_SET(ex->exit_info, VR_002_EX_PICKPROOF) )
			{
				ex->door.lock.pick_chance = 0;
			}
			else if( IS_SET(ex->exit_info, VR_002_EX_INFURIATING) )
			{
				ex->door.lock.pick_chance = 10;
			}
			else if( IS_SET(ex->exit_info, VR_002_EX_HARD) )
			{
				ex->door.lock.pick_chance = 40;
			}
			else if( IS_SET(ex->exit_info, VR_002_EX_EASY) )
			{
				ex->door.lock.pick_chance = 80;
			}
			else
			{
				ex->door.lock.pick_chance = 100;
			}

			REMOVE_BIT(ex->exit_info, (VR_002_EX_LOCKED|VR_002_EX_PICKPROOF|VR_002_EX_INFURIATING|VR_002_EX_HARD|VR_002_EX_EASY));
		}
	}


	room->version = VERSION_ROOM;

	if(room->persist) persist_addroom(room);

	log_string("persist_load: #-ROOM");

	return room;
}

bool persist_load(void)
{
	FILE *fp;
	char *word;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	bool good = TRUE;

	log_string("persist_load: loading persist entities...");

	if (!(fp = fopen(PERSIST_FILE, "r"))) {
		bug("persist.dat: Couldn't open file.",0);
		return TRUE;
	} else {
		while(good) {
			word = fread_word(fp);

			if(!str_cmp(word,"#ROOM")) {
				room = persist_load_room(fp, 'R');
				if(!room) good = FALSE;

			} else if(!str_cmp(word,"#VROOM")) {
				room = persist_load_room(fp, 'V');
				if(!room) good = FALSE;


			} else if(!str_cmp(word,"#CROOM")) {
				room = persist_load_room(fp, 'C');
				if(room) {
					variable_dynamic_fix_clone_room(room);
					persist_fix_environment_room(room);
				} else
					good = FALSE;

			} else if(!str_cmp(word,"#MOBILE")) {
				ch = persist_load_mobile(fp);

				if( ch ) {
					if( ch->in_room ) {
						char_to_room(ch, ch->in_room);
						variable_dynamic_fix_mobile(ch);
						persist_fix_environment_mobile(ch);
					} else {
						extract_char(ch,TRUE);
						good = FALSE;
					}
				} else
					good = FALSE;
			} else if(!str_cmp(word,"#OBJECT")) {
				obj = persist_load_object(fp);

				if( obj ) {
					obj->locker = FALSE;
					if( obj->in_room ) {
						obj_to_room(obj, obj->in_room);
						variable_dynamic_fix_object(obj);
						persist_fix_environment_object(obj);
					} else {
						extract_obj(obj);
						good = FALSE;
					}
				} else
					good = FALSE;

			} else if(!str_cmp(word,"#END"))
				break;
		}

		fclose(fp);
	}

	if(good)
		log_string("persist_load: done...");
	else
		log_string("persist_load: error...");


	return good;
}

bool save_instances()
{
	ITERATOR it;
	INSTANCE *instance;
	DUNGEON *dungeon;
	SHIP_DATA *ship;

	FILE *fp = fopen(INSTANCES_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save instances.dat", 0);
		return false;
	}

	// Save dungeons
	iterator_start(&it, loaded_dungeons);
	while( (dungeon = (DUNGEON *)iterator_nextdata(&it)) )
	{
		// Skip dungeons that cannot save
		if( !IS_SET(dungeon->flags, (DUNGEON_NO_SAVE|DUNGEON_DESTROY)) )
		{
			dungeon_save(fp, dungeon);
		}
	}
	iterator_stop(&it);

	// Save ships
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		ship_save(fp, ship);
	}
	iterator_stop(&it);

	// Save the rest of the instances
	iterator_start(&it, loaded_instances);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		// Skip dungeon instances and instances that cannot save
		if( !IS_VALID(instance->dungeon) && !IS_VALID(instance->ship) && !IS_SET(instance->flags, (INSTANCE_NO_SAVE|INSTANCE_DESTROY))  )
			instance_save(fp, instance);
	}
	iterator_stop(&it);

	fprintf(fp, "#END\n\r\n\r");

	fclose(fp);

	return true;
}


void load_instances()
{
	FILE *fp = fopen(INSTANCES_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load instances.dat", 0);
		return;
	}

	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if (!str_cmp(word, "#INSTANCE"))
		{
			INSTANCE *instance = instance_load(fp);

			if( instance )
			{
				list_appendlink(loaded_instances, instance);
			}

			fMatch = TRUE;
			continue;
		}
		else if (!str_cmp(word, "#DUNGEON"))
		{
			DUNGEON *dungeon = dungeon_load(fp);

			if( dungeon )
			{
				list_appendlink(loaded_dungeons, dungeon);
			}

			fMatch = TRUE;
			continue;
		}
		else if (!str_cmp(word, "#SHIP"))
		{
			SHIP_DATA *ship = ship_load(fp);

			if(ship)
			{
				list_appendlink(loaded_ships, ship);
			}

			fMatch = TRUE;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_instances: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	resolve_ships();

	fclose(fp);
}


