#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ctype.h>

#include "../merc.h"
#include "niblang.h"
#include "script.h"
#include "interpret.h"

const struct flag_type sex_table[] =
{
   {    "neuter",   SEX_NEUTRAL,    true },
   {	"male",		SEX_MALE,       true },
   {	"female",	SEX_FEMALE,     true },
   {	"either",	SEX_EITHER,     true },
   {	NULL,		0,              false }
};

// Flags
const struct flag_type act_flags[] =
{
    {	"npc",					ACT_IS_NPC,				false	},
    {	"sentinel",				ACT_SENTINEL,			true	},
    {	"scavenger",			ACT_SCAVENGER,			true	},
    {	"protected",			ACT_PROTECTED,			true	},
    {	"mount",				ACT_MOUNT,				true	},
    {	"aggressive",			ACT_AGGRESSIVE,			true	},
    {	"stay_area",			ACT_STAY_AREA,			true	},
    {	"wimpy",				ACT_WIMPY,				true	},
    {	"pet",					ACT_PET,				true	},
    {	"train",				ACT_TRAIN,				true	},
    {	"practice",				ACT_PRACTICE,			false	},
    {	"blacksmith",			ACT_BLACKSMITH,			true	},
    {	"crew_seller",			ACT_CREW_SELLER,		true	},
    {   "no_lore",				ACT_NO_LORE,			true	},
    {	"undead",				ACT_UNDEAD,				true	},
    {   "team_animal",          ACT_TEAM_ANIMAL,        true    },
    {	"cleric",				ACT_CLERIC,				true	},
    {	"mage",					ACT_MAGE,				true	},
    {	"thief",				ACT_THIEF,				true	},
    {	"warrior",				ACT_WARRIOR,			true	},
    {	"animated",				ACT_ANIMATED,			false	},
    {	"nopurge",				ACT_NOPURGE,			true	},
    {	"outdoors",				ACT_OUTDOORS,			true	},
    {   "restringer",			ACT_IS_RESTRINGER,		true	},
    {	"indoors",				ACT_INDOORS,			true	},
    {	"stay_locale",			ACT_STAY_LOCALE,		true	},
    {	"update_always",		ACT_UPDATE_ALWAYS,		true	},
    {	"changer",				ACT_IS_CHANGER,			true	},
    {   "banker",				ACT_IS_BANKER,			true    },
    {   "can_be_led",           ACT_CAN_BE_LED,         true    },
    {	NULL,					0,	false	}
};


const struct flag_type act2_flags[]=
{
    {   "churchmaster",			ACT2_CHURCHMASTER,		true	},
    {   "noquest",				ACT2_NOQUEST,			true    },
    {   "stay_region",  		ACT2_STAY_REGION,   	true    },
    {	"no_hunt",				ACT2_NO_HUNT,			true	},
    {   "airship_seller",		ACT2_AIRSHIP_SELLER,	true    },
    {   "wizi_mob",				ACT2_WIZI_MOB,			true    },
    {   "trader",				ACT2_TRADER,			true    },
    {   "loremaster",			ACT2_LOREMASTER,		true	},
    {   "no_resurrect",			ACT2_NO_RESURRECT,		true    },
    {   "drop_eq",				ACT2_DROP_EQ,			true	},
    {   "gq_master",			ACT2_GQ_MASTER,			true    },
    {   "wilds_wanderer",		ACT2_WILDS_WANDERER,	true	},
    {   "ship_quest_master",	ACT2_SHIP_QUESTMASTER,	true	},
    {   "reset_once",			ACT2_RESET_ONCE,		true	},
    {	"see_all",				ACT2_SEE_ALL,			true	},
    {   "no_chase",				ACT2_NO_CHASE,			true	},
    {	"takes_skulls",			ACT2_TAKES_SKULLS,		true	},
    {	"pirate",				ACT2_PIRATE,			true	},
    {	"player_hunter",		ACT2_PLAYER_HUNTER,		true	},
    {	"invasion_leader",		ACT2_INVASION_LEADER,	true	},
    {	"invasion_mob",			ACT2_INVASION_MOB,		true	},
    {   "see_wizi",				ACT2_SEE_WIZI,			true	},
    {   "soul_deposit",			ACT2_SOUL_DEPOSIT,		true	},
    {   "use_skills_only",		ACT2_USE_SKILLS_ONLY,	true	},
    {   "can_level",			ACT2_CANLEVEL,			true	},
    {   "no_xp",				ACT2_NO_XP,				true	},
    {   "hired",				ACT2_HIRED,				false	},
    {   "renewer",				ACT2_RENEWER,			true	},
	{	"show_in_wilds",		ACT2_SHOW_IN_WILDS,		true	},
    {   "instance_mob",			ACT2_INSTANCE_MOB,		false	},
    {   NULL,					0,						false	}
};

const struct flag_type *act_flagbank[] =
{
    act_flags,
    act2_flags,
    NULL
};


const struct flag_type armour_strength_table[] =
{
    {	"none",		OBJ_ARMOUR_NOSTRENGTH,	true   	},
    {   "light",	OBJ_ARMOUR_LIGHT,   true  	},
    {   "medium",	OBJ_ARMOUR_MEDIUM,   true    	},
    {   "strong",	OBJ_ARMOUR_STRONG,   true  	},
    {   "heavy",	OBJ_ARMOUR_HEAVY,   true 	},
    {	NULL, 0, false }
};


const struct flag_type plr_flags[] =
{
    {	"npc",			PLR_IS_NPC,		false	},
    {	"excommunicated",	PLR_EXCOMMUNICATED,	false	},
    {	"pk",			PLR_PK,			false	},
    {	"autoexit",		PLR_AUTOEXIT,		false	},
    {	"autoloot",		PLR_AUTOLOOT,		false	},
    {	"autosac",		PLR_AUTOSAC,		false	},
    {	"autogold",		PLR_AUTOGOLD,		false	},
    {   "autoolc",      PLR_AUTOOLC,        false   },
    {	"autosplit",		PLR_AUTOSPLIT,		false	},
    {   "autosetname",		PLR_AUTOSETNAME,	false   },
    {	"holylight",		PLR_HOLYLIGHT,		false	},
    {	"autoeq",		PLR_AUTOEQ,		false	},
    {	"nosummon",		PLR_NOSUMMON,		false	},
    {	"nofollow",		PLR_NOFOLLOW,		false	},
    {	"colour",		PLR_COLOUR,		false	},
    {	"notify",		PLR_NOTIFY,		true	},
    {	"log",			PLR_LOG,		false	},
    {	"deny",			PLR_DENY,		false	},
    {	"freeze",		PLR_FREEZE,		false	},
    {	"helper",		PLR_HELPER,		false	},
    {   "botter",		PLR_BOTTER,		false   },
    {	"showdamage",		PLR_SHOWDAMAGE,		false	},
    {	"no_challenge",		PLR_NO_CHALLENGE,	false	},
    {	"no_resurrect",		PLR_NO_RESURRECT,	false	},
    {	"pursuit",		PLR_PURSUIT,		false	},
    {	NULL,			0,	0			}
};

const struct flag_type plr2_flags[] =
{
    {	"autosurvey",		PLR_AUTOSURVEY,		false	},
    {	"sacrifice_all",		PLR_SACRIFICE_ALL,		false	},
    {	"no_wake",		PLR_NO_WAKE,		false	},
    {	"holyaura",		PLR_HOLYAURA,		false	},
    {	"mobile",		PLR_MOBILE,		false	},
    {	"favskills",	PLR_FAVSKILLS,	false	},
    {   "holywarp",     PLR_HOLYWARP,   false   },
    {   "compass",      PLR_COMPASS,    false   },
    {   "no_reckoning",     PLR_NORECKONING,   false   },
    {   "no_lore",     PLR_NOLORE,   false   },
    {	"holypersona",		PLR_HOLYPERSONA,		false	},
    {   "autoafk",      PLR_AUTOAFK,        false   },
    {   "hide_idle",    PLR_HIDE_IDLE,      false   },
    {   "show_timestamps",  PLR_SHOW_TIMESTAMPS,    false},
    {	NULL,			0,	0			}
};

const struct flag_type *plr_flagbank[] =
{
    plr_flags,
    plr2_flags,
    NULL
};



const struct flag_type affect_flags[] =
{
    {	"blind",		AFF_BLIND,	true	},
    {	"invisible",		AFF_INVISIBLE,	true	},
    {	"detect_evil",		AFF_DETECT_EVIL,	true	},
    {	"detect_invis",		AFF_DETECT_INVIS,	true	},
    {	"detect_magic",		AFF_DETECT_MAGIC,	true	},
    {	"detect_hidden",	AFF_DETECT_HIDDEN,	true	},
    {	"detect_good",		AFF_DETECT_GOOD,	true	},
    {	"sanctuary",		AFF_SANCTUARY,	true	},
    {	"faerie_fire",		AFF_FAERIE_FIRE,	true	},
    {	"infrared",		AFF_INFRARED,	true	},
    {	"curse",		AFF_CURSE,	true	},
    {   "death_grip",		AFF_DEATH_GRIP,	true    },
    {	"poison",		AFF_POISON,	true	},
    {	"sneak",		AFF_SNEAK,	true	},
    {	"hide",			AFF_HIDE,	true	},
    {	"sleep",		AFF_SLEEP,	true	},
    {	"charm",		AFF_CHARM,	true	},
    {	"flying",		AFF_FLYING,	true	},
    {	"pass_door",		AFF_PASS_DOOR,	true	},
    {	"haste",		AFF_HASTE,	true	},
    {	"calm",			AFF_CALM,	true	},
    {	"plague",		AFF_PLAGUE,	true	},
    {	"weaken",		AFF_WEAKEN,	true	},
    {	"frenzy",		AFF_FRENZY,	true	},
    {	"berserk",		AFF_BERSERK,	true	},
    {	"swim",			AFF_SWIM,	true	},
    {	"regeneration",		AFF_REGENERATION,	true	},
    {	"slow",			AFF_SLOW,	true	},
    {   "web",                  AFF_WEB,     true    },
    {	NULL,			0,	0	}
};


const struct flag_type affect2_flags[] =
{
    {   "silence",		        AFF2_SILENCE,     	true    },
    {   "evasion",		        AFF2_EVASION,	true	},
    {   "cloak_guile",  	    AFF2_CLOAK_OF_GUILE,	true	},
    {   "warcry",		        AFF2_WARCRY,	true    },
    {   "light_shroud", 	    AFF2_LIGHT_SHROUD,	true	},
    {   "healing_aura", 	    AFF2_HEALING_AURA,	true	},
    {   "energy_field", 	    AFF2_ENERGY_FIELD,	true	},
    {   "spell_shield", 	    AFF2_SPELL_SHIELD,	true	},
    {   "spell_deflection", 	AFF2_SPELL_DEFLECTION,  	true    },
    {   "avatar_shield",	    AFF2_AVATAR_SHIELD,	true    },
    {   "fatigue",		        AFF2_FATIGUE,	true	},
    {   "paralysis",	    	AFF2_PARALYSIS,	true	},
    {   "neurotoxin",	    	AFF2_NEUROTOXIN,	true	},
    {	"toxin",	        	AFF2_TOXIN,	true	},
    {   "electrical_barrier",	AFF2_ELECTRICAL_BARRIER,	true	},
    {	"fire_barrier",	    	AFF2_FIRE_BARRIER,	true	},
    {	"frost_barrier",    	AFF2_FROST_BARRIER,	true	},
    {	"improved_invis",   	AFF2_IMPROVED_INVIS,	true	},
    {	"ensnare",      		AFF2_ENSNARE,	true	},
    {   "see_cloak",    		AFF2_SEE_CLOAK,	true    },
    {	"stone_skin",   		AFF2_STONE_SKIN,	true	},
    {	"morphlock",    		AFF2_MORPHLOCK,		true	},
    {	"deathsight",   		AFF2_DEATHSIGHT,	true	},
    {	"immobile",     		AFF2_IMMOBILE,	true	},
    {	"protected",    		AFF2_PROTECTED,	true	},
    {   "aggressive",           AFF2_AGGRESSIVE,    true    },
    {   NULL,               	0,      0       }
};

const struct flag_type *affect_flagbank[] =
{
    affect_flags,
    affect2_flags,
    NULL
};


const struct flag_type off_flags[] =
{
    {	"area_attack",		A,	true	},
    {	"backstab",		B,	true	},
    {	"bash",			C,	true	},
    {	"berserk",		D,	true	},
    {	"disarm",		E,	true	},
    {	"dodge",		F,	true	},
    {	"fade",			G,	true	},
    {	"kick",			I,	true	},
    {	"dirt_kick",		J,	true	},
    {	"parry",		K,	true	},
    {	"rescue",		L,	true	},
    {	"tail",			M,	true	},
    {	"trip",			N,	true	},
    {	"crush",		O,	true	},
    {	"assist_all",		P,	true	},
    {	"assist_align",		Q,	true	},
    {	"assist_race",		R,	true	},
    {	"assist_players",	S,	true	},
    {	"assist_npc",		Z,	true	},
    {	"assist_guard",		T,	true	},
    {	"assist_vnum",		U,	true	},
    {   "magic", 		Y,	true	},
    {	NULL,			0,	0	}
};


const struct flag_type imm_flags[] =
{
    {	"summon",	IMM_SUMMON,		true	},
    {   "charm",        IMM_CHARM,            	true    },
    {   "magic",        IMM_MAGIC,            	true    },
    {   "weapon",       IMM_WEAPON,           	true    },
    {   "bash",         IMM_BASH,             	true    },
    {   "pierce",       IMM_PIERCE,           	true    },
    {   "slash",        IMM_SLASH,            	true    },
    {   "fire",         IMM_FIRE,             	true    },
    {   "cold",         IMM_COLD,             	true    },
    {   "light",     	IMM_LIGHT,            	true    },
    {   "lightning",    IMM_LIGHTNING,        	true    },
    {   "acid",         IMM_ACID,             	true    },
    {   "poison",       IMM_POISON,           	true    },
    {   "negative",     IMM_NEGATIVE,         	true    },
    {   "holy",         IMM_HOLY,             	true    },
    {   "energy",       IMM_ENERGY,           	true    },
    {   "mental",       IMM_MENTAL,           	true    },
    {   "disease",      IMM_DISEASE,          	true    },
    {   "drowning",     IMM_DROWNING,         	true    },
    {	"sound",	IMM_SOUND,		true	},
    {	"wood",		IMM_WOOD,		true	},
    {	"silver",	IMM_SILVER,		true	},
    {	"iron",		IMM_IRON,		true	},
    {   "kill",		IMM_KILL, 		true    },
    {	"water",	IMM_WATER,		true	},
    {	"air",		IMM_AIR,		true	},	// @@@NIB : 20070125
    {	"earth",	IMM_EARTH,		true	},	// @@@NIB : 20070125
    {	"plant",	IMM_PLANT,		true	},	// @@@NIB : 20070125
    {   "suffocation",  IMM_SUFFOCATION,    true    },
    {	NULL,			0,	0	}
};


const struct flag_type form_flags[] =
{
    {	"edible",		FORM_EDIBLE,		true	},
    {	"poison",		FORM_POISON,		true	},
    {	"magical",		FORM_MAGICAL,		true	},
    {	"instant_decay",	FORM_INSTANT_DECAY,	true	},
    {	"other",		FORM_OTHER,		true	},
    {   "no_breathing", FORM_NO_BREATHING,  true    },
    {	"animal",		FORM_ANIMAL,		true	},
    {	"sentient",		FORM_SENTIENT,		true	},
    {	"undead",		FORM_UNDEAD,		true	},
    {	"construct",		FORM_CONSTRUCT,		true	},
    {	"mist",			FORM_MIST,		true	},
    {	"intangible",		FORM_INTANGIBLE,	true	},
    {	"biped",		FORM_BIPED,		true	},
    {	"centaur",		FORM_CENTAUR,		true	},
    {	"insect",		FORM_INSECT,		true	},
    {	"spider",		FORM_SPIDER,		true	},
    {	"crustacean",		FORM_CRUSTACEAN,	true	},
    {	"worm",			FORM_WORM,		true	},
    {	"blob",			FORM_BLOB,		true	},
    {   "plant",        FORM_PLANT,     true    },
    {	"mammal",		FORM_MAMMAL,		true	},
    {	"bird",			FORM_BIRD,		true	},
    {	"reptile",		FORM_REPTILE,		true	},
    {	"snake",		FORM_SNAKE,		true	},
    {	"dragon",		FORM_DRAGON,		true	},
    {	"amphibian",		FORM_AMPHIBIAN,		true	},
    {	"fish",			FORM_FISH,		true	},
    {	"cold_blood",		FORM_COLD_BLOOD,	true	},
    {	"object",		FORM_OBJECT,		true	},
    {	NULL,			0,			0	}
};


const struct flag_type part_flags[] =
{
    {	"arms",			PART_ARMS,			true	},
    {	"brains",		PART_BRAINS,		true	},
    {	"claws",		PART_CLAWS,			true	},
    {	"ear",			PART_EAR,			true	},
    {	"eye",			PART_EYE,			true	},
    {	"eyestalks",	PART_EYESTALKS,		true	},
    {	"fangs",		PART_FANGS,			true	},
    {	"feet",			PART_FEET,			true	},
    {	"fingers",		PART_FINGERS,		true	},
    {	"fins",			PART_FINS,			true	},
    {	"gills",		PART_GILLS,			true	},
    {	"guts",			PART_GUTS,			true	},
    {	"hands",		PART_HANDS,			true	},
    {	"head",			PART_HEAD,			true	},
    {	"heart",		PART_HEART,			true	},
    {	"hide",			PART_HIDE,			true	},
    {	"horns",		PART_HORNS,			true	},
    {	"legs",			PART_LEGS,			true	},
    {	"long_tongue",	PART_LONG_TONGUE,	true	},
    {   "lungs",        PART_LUNGS,         true    },
    {	"scales",		PART_SCALES,		true	},
    {   "skull",        PART_SKULL,         true    },
    {	"tail",			PART_TAIL,			true	},
    {	"tentacles",	PART_TENTACLES,		true	},
    {	"tusks",		PART_TUSKS,			true	},
    {	"wings",		PART_WINGS,			true	},
    {	NULL,			0,					0	}
};


const struct flag_type comm_flags[] =
{
    {	"quiet",		COMM_QUIET,		true	},
    {   "nowiz",		COMM_NOWIZ,		true	},
    {   "noclangossip",		COMM_NOAUCTION,		true	},
    {   "nogossip",		COMM_NOGOSSIP,		true	},
    {   "nomusic",		COMM_NOMUSIC,		true	},
    {   "noclan",		COMM_NOCT,		true	},
    {   "compact",		COMM_COMPACT,		true	},
    {   "brief",		COMM_BRIEF,		true	},
    {   "prompt",		COMM_PROMPT,		true	},
    {   "telnet_ga",		COMM_TELNET_GA,		true	},
    {   "no_flaming",		COMM_NO_FLAMING,	true	},
    {   "noyell",		COMM_NOYELL,		true    },
    {   "noautowar",		COMM_NOAUTOWAR,		false	},
    {   "notell",		COMM_NOTELL,		false	},
    {   "nochannels",		COMM_NOCHANNELS,	false	},
    {   "noquote",		COMM_NOQUOTE,		false	},
    {   "afk",			COMM_AFK,		true	},
    {   "ooc",			COMM_NO_OOC,		true	},
    {   "hints",		COMM_NOHINTS,		true	},
    {   "nobattlespam",		COMM_NOBATTLESPAM,	true	},
    {   "nomap",		COMM_NOMAP,		true	},
    {	"notells",		COMM_NOTELLS,		true	},
    {	NULL,			0,			0	}
};

const struct flag_type area_who_display[] = {
	{	"      ",	AREA_BLANK,		true	},
	{	"Abyss",	AREA_ABYSS,		true	},
	{	"Arena",	AREA_ARENA,		true	},
	{	"At Sea",	AREA_AT_SEA,		true	},
	{	"Battle",	AREA_BATTLE,		true	},
	{	"Castle",	AREA_CASTLE,		true	},
	{	"Cavern",	AREA_CAVERN,		true	},
	{	"Church",	AREA_CHURCH,		true	},
	{	"Cosmos",	AREA_OFFICE,		true	},
	{	"Cult",		AREA_CULT,		true	},
	{	"Dungn",	AREA_DUNGEON,		true	},
	{	"Eden",		AREA_EDEN,		true	},
	{	"Forest",	AREA_FOREST,		true	},
	{	"Fort",		AREA_FORT,		true	},
	{	"Home",		AREA_HOME,		true	},
	{	"Inn",		AREA_INN,		true	},
	{	"Isle",		AREA_ISLE,		true	},
	{	"Jungle",	AREA_JUNGLE,		true	},
	{	"Keep",		AREA_KEEP,		true	},
	{	"Limbo",	AREA_LIMBO,		true	},
	{	"Mount",	AREA_MOUNTAIN,		true	},
	{	"Nether",	AREA_NETHERWORLD,	true	},
	{	"Outpst",	AREA_OUTPOST,		true	},
	{	"Palace",	AREA_PALACE,		true	},
	{	"Planar",	AREA_PLANAR,		true	},
	{	"Pyramd",	AREA_PYRAMID,		true	},
	{	"Rift",		AREA_CHAT,		true	},
	{	"Ruins",	AREA_RUINS,		true	},
	{	"Ship",		AREA_ON_SHIP,		true	},
	{	"Sky",		AREA_AERIAL,		true	},
	{	"Swamp",	AREA_SWAMP,		true	},
	{	"Temple",	AREA_TEMPLE,		true	},
	{	"Tomb",		AREA_TOMB,		true	},
	{	"Tower",	AREA_TOWER,		true	},
	{	"Towne",	AREA_TOWNE,		true	},
	{	"Tundra",	AREA_TUNDRA,		true	},
	{	"PoA",		AREA_PG,		true	},
	{	"Ocean",	AREA_UNDERSEA,		true	},
	{	"Villa",	AREA_VILLAGE,		true	},
	{	"Vulcan",	AREA_VOLCANO,		true	},
	{	"Wilder",	AREA_WILDER,		true	},
	{	"Instce",	AREA_INSTANCE,		true	},
	{	"Duty",		AREA_DUTY,			true	},
    {   "City",     AREA_CITY,          true    },
    {   "Park",     AREA_PARK,          true    },
    {   "Citadl",   AREA_CITADEL,       true    },
    {   "Sky",      AREA_SKY,           true    },
    {   "Hills",    AREA_HILLS,         true    },
	{	NULL,		0,		0	},
};



const struct flag_type area_who_titles[] = {
	{	"abyss",	AREA_ABYSS,		true	},
	{	"aerial",	AREA_AERIAL,		true	},
	{	"arena",	AREA_ARENA,		true	},
	{	"at_sea",	AREA_AT_SEA,		true	},
	{	"battle",	AREA_BATTLE,		true	},
	{	"blank",	AREA_BLANK,		true	},
	{	"castle",	AREA_CASTLE,		true	},
	{	"cavern",	AREA_CAVERN,		true	},
	{	"chat",		AREA_CHAT,		true	},
	{	"church",	AREA_CHURCH,		true	},
	{	"citadel",   AREA_CITADEL,       true    },
	{	"city",     AREA_CITY,          true    },
	{	"cult",		AREA_CULT,		true	},
	{	"dungeon",	AREA_DUNGEON,		true	},
	{	"duty",	AREA_DUTY,			true	},
	{	"eden",		AREA_EDEN,		true	},
	{	"forest",	AREA_FOREST,		true	},
	{	"fort",		AREA_FORT,		true	},
	{	"hills",    AREA_HILLS,         true    },
	{	"home",		AREA_HOME,		true	},
	{	"immortal",	AREA_OFFICE,		true	},
	{	"inn",		AREA_INN,		true	},
	{	"instance",		AREA_INSTANCE,		true	},
	{	"isle",		AREA_ISLE,		true	},
	{	"jungle",	AREA_JUNGLE,		true	},
	{	"keep",		AREA_KEEP,		true	},
	{	"limbo",	AREA_LIMBO,		true	},
	{	"mountain",	AREA_MOUNTAIN,		true	},
	{	"netherworld",	AREA_NETHERWORLD,	true	},
	{	"on_ship",	AREA_ON_SHIP,		true	},
	{	"outpost",	AREA_OUTPOST,		true	},
	{	"palace",	AREA_PALACE,		true	},
	{	"park",     AREA_PARK,          true    },
	{	"pg",		AREA_PG,		true	},
	{	"planar",	AREA_PLANAR,		true	},
	{	"pyramid",	AREA_PYRAMID,		true	},
	{	"ruins",	AREA_RUINS,		true	},
	{	"sky",      AREA_SKY,           true    },
	{	"swamp",	AREA_SWAMP,		true	},
	{	"temple",	AREA_TEMPLE,		true	},
	{	"tomb",		AREA_TOMB,		true	},
	{	"tower",	AREA_TOWER,		true	},
	{	"towne",	AREA_TOWNE,		true	},
	{	"tundra",	AREA_TUNDRA,		true	},
	{	"undersea",	AREA_UNDERSEA,		true	},
	{	"village",	AREA_VILLAGE,		true	},
	{	"volcano",	AREA_VOLCANO,		true	},
	{	"wilderness",	AREA_WILDER,		true	},
	{	NULL,		0,		0	},
};



const struct flag_type area_flags[] =
{
    {	"none",			AREA_NONE,		false	},
    {	"changed",		AREA_CHANGED,		true	},
    {	"added",		AREA_ADDED,		true    },
    {	"loading",		AREA_LOADING,		false	},
    {   "no_map",		AREA_NOMAP,		true    },
    {   "dark",			AREA_DARK,		true    },
    {	"testport",		AREA_TESTPORT,		true	},
    {	"no_recall",	AREA_NO_RECALL,		true	},
    {	"no_rooms",		AREA_NO_ROOMS,		true	},
    {	"newbie",		AREA_NEWBIE,		true	},
    {	"no_get_random",AREA_NO_GET_RANDOM,	true	},
    {	"no_fading",	AREA_NO_FADING,		true	},
    {	"blueprint",	AREA_BLUEPRINT,		true	},
    {	"locked",		AREA_LOCKED,		true	},
    {   "low_level",    AREA_LOW_LEVEL,     true    },
    {   "immortal",     AREA_IMMORTAL,      true    },
    {   "persist",      AREA_PERSIST,       true    },
    {   "keep_live",    AREA_KEEP_LIVE,     true    },
    {   "social",       AREA_SOCIAL,        true    },
    {   "housing",      AREA_HOUSING,       true    },
    {   "immortal",     AREA_IMMORTAL,      true    },
    {	NULL,			0,			0	}
};


const struct flag_type church_flags[] =
{
    {	"show_pks",		CHURCH_SHOW_PKS,	true 	},
    {   "allow_crosszones",	CHURCH_ALLOW_CROSSZONES,true 	},
    {	"public_motd",	CHURCH_PUBLIC_MOTD,	true	},
    {	"public_rules",	CHURCH_PUBLIC_RULES,	true	},
    {	"public_info",	CHURCH_PUBLIC_INFO,	true	},
    {	NULL,			0,			0	},
};


const struct flag_type sex_flags[] =
{
    {	"male",			SEX_MALE,		true	},
    {	"female",		SEX_FEMALE,		true	},
    {	"neutral",		SEX_NEUTRAL,		true	},
    {   "random",               3,                      true    },
    {	"none",			SEX_NEUTRAL,		true	},
    {	NULL,			0,			0	}
};


const struct flag_type exit_flags[] =
{
    {   "door",			EX_ISDOOR,		true    },
    {	"closed",		EX_CLOSED,		true	},
    {   "nopass",		EX_NOPASS,		true	},
    {	"noclose",		EX_NOCLOSE,		true	},
    {   "hidden",		EX_HIDDEN,		true	},
    {   "found",		EX_FOUND,		true	},
    {   "broken",		EX_BROKEN,		true	},
    {   "nobash",		EX_NOBASH,		true    },
    {   "walkthrough",		EX_WALKTHROUGH,		true    },
    {   "barred",       EX_BARRED,      true    },
    {   "nobar",		EX_NOBAR,		true    },
    {   "vlink",		EX_VLINK,		false	},
    {   "aerial",		EX_AERIAL,		true	},
    {   "nohunt",		EX_NOHUNT,		true	},
//    {	"environment",		EX_ENVIRONMENT,		true	},
    {	"nounlink",		EX_NOUNLINK,		true	},
    {	"prevfloor",		EX_PREVFLOOR,		false	},
    {	"nextfloor",		EX_NEXTFLOOR,		false	},
    {	"nosearch",			EX_NOSEARCH,		true	},
    {	"mustsee",			EX_MUSTSEE,			true	},
    {   "transparent",      EX_TRANSPARENT,     true    },
    {   "nomob",            EX_NOMOB,           true    },
    {   "nowander",         EX_NOWANDER,        true    },
    {	NULL,			0,			0	}
};

const struct flag_type lock_flags[] =
{
	{	"locked",		LOCK_LOCKED,		true	},
	{	"magic",		LOCK_MAGIC,			true	},
	{	"snap_key",		LOCK_SNAPKEY,		true	},
	{	"script",		LOCK_SCRIPT,		true	},
	{	"noremove",		LOCK_NOREMOVE,		true	},
	{	"broken",		LOCK_BROKEN,		true	},
	{	"jammed",		LOCK_JAMMED,		true	},
	{	"nojam",		LOCK_NOJAM,			true	},
    {   "nomagic",      LOCK_NOMAGIC,       true    },
    {   "noscript",     LOCK_NOSCRIPT,      true    },
    {   "final",        LOCK_FINAL,         false   },
	{	"created",		LOCK_CREATED,		false	},
	{	NULL,			0,					0		}
};

const struct flag_type portal_exit_flags[] =
{
    {   "door",			EX_ISDOOR,		true    },
    {	"closed",		EX_CLOSED,		true	},
    {   "nopass",		EX_NOPASS,		true	},
    {	"noclose",		EX_NOCLOSE,		true	},
    {   "found",		EX_FOUND,		true	},
    {   "broken",		EX_BROKEN,		true	},
    {   "nobash",		EX_NOBASH,		true    },
    {   "walkthrough",		EX_WALKTHROUGH,		true    },
    {   "barred",       EX_BARRED,      true    },
    {   "nobar",		EX_NOBAR,		true    },
    {   "aerial",		EX_AERIAL,		true	},
    {   "nohunt",		EX_NOHUNT,		true	},
    {	"environment",		EX_ENVIRONMENT,		true	},
    {	"prevfloor",		EX_PREVFLOOR,		true	},
    {	"nextfloor",		EX_NEXTFLOOR,		true	},
    {	"nosearch",			EX_NOSEARCH,		true	},
    {	"mustsee",			EX_MUSTSEE,			true	},
    {   "transparent",      EX_TRANSPARENT,     true    },
    {   "nomob",            EX_NOMOB,           true    },
    {	NULL,			0,			0	}
};


const struct flag_type door_resets[] =
{
    {	"open and unlocked",	0,		true	},
    {	"closed and unlocked",	1,		true	},
    {	"closed and locked",	2,		true	},
    {	NULL,			0,		0	}
};


const struct flag_type room_flags[] =
{
	{	"arena",			ROOM_ARENA,				true	},
	{	"bank",				ROOM_BANK,				true	},
	{	"chaotic",			ROOM_CHAOTIC,			true	},
	{	"dark",				ROOM_DARK,				true	},
	{	"dark_attack",		ROOM_ATTACK_IF_DARK,	true	},
	{	"death_trap",		ROOM_DEATH_TRAP,		true	},
	{	"gods_only",		ROOM_GODS_ONLY,			true	},
	{	"helm",				ROOM_SHIP_HELM,			true	},
	{	"imp_only",			ROOM_IMP_ONLY,			true	},
	{	"indoors",			ROOM_INDOORS,			true	},
	{	"locker",			ROOM_LOCKER,			true	},
	{	"newbies_only",		ROOM_NEWBIES_ONLY,		true	},
	{	"no_map",			ROOM_NOMAP,				true	},
	{	"no_mob",			ROOM_NO_MOB,			true	},
	{	"no_recall",		ROOM_NO_RECALL,			true	},
	{	"no_wander",		ROOM_NO_WANDER,			true	},
	{	"nocomm",			ROOM_NOCOMM,			true	},
	{	"nomagic",			ROOM_NOMAGIC,			true	},
	{	"nowhere",			ROOM_NOWHERE,			true	},
	{	"noview",			ROOM_NOVIEW,			true	},
	{	"pk",				ROOM_PK,				true	},
	{	"private",			ROOM_PRIVATE,			true	},
	{	"real_estate",		ROOM_HOUSE_UNSOLD,		true	},
	{	"rocks",			ROOM_ROCKS,				true	},
	{	"safe",				ROOM_SAFE,				true	},
	{	"solitary",			ROOM_SOLITARY,			true	},
	{	"underwater",		ROOM_UNDERWATER,		true	},
	{	"view_wilds",		ROOM_VIEWWILDS,			true	},
	{	NULL,	0,	0	}
};


const struct flag_type room2_flags[] =
{
    {	"alchemy",				ROOM_ALCHEMY,			true	},
    {	"always_update",		ROOM_ALWAYS_UPDATE,		true	},
    {	"bar",					ROOM_BAR,				true	},
    {	"blueprint",			ROOM_BLUEPRINT, 		true	},
    {	"briars",				ROOM_BRIARS,			true	},
    {	"citymove",				ROOM_CITYMOVE,			true	},
    {	"clone_persist",		ROOM_CLONE_PERSIST,		true	},
    {	"drain_mana",			ROOM_DRAIN_MANA,		true	},
    {   "fire",					ROOM_FIRE,				true    },
    {	"hard_magic",			ROOM_HARD_MAGIC,		true	},
    {   "icy",					ROOM_ICY,				true    },
    {   "keep_live",            ROOM_KEEP_LIVE,         true    },
    {	"multiplay",			ROOM_MULTIPLAY,			true	},
    {	"no_clone",				ROOM_NOCLONE,			true	},
    {	"no_floor",				ROOM_NOFLOOR,			true	},
    {	"no_get_random",		ROOM_NO_GET_RANDOM,		true	},
    {   "no_quest",				ROOM_NO_QUEST,			true    },
    {   "no_quit",				ROOM_NO_QUIT,			true 	},
    {   "post_office",			ROOM_POST_OFFICE,		true	},
    {	"safe_harbor",			ROOM_SAFE_HARBOR,		true	},
    {	"slow_magic",			ROOM_SLOW_MAGIC,		true	},
    {	"toxic_bog",			ROOM_TOXIC_BOG,			true	},
    {	"underground",			ROOM_UNDERGROUND,		true	},
    {	"virtual_room",			ROOM_VIRTUAL_ROOM,		false	},
    {	"vis_on_map",			ROOM_VISIBLE_ON_MAP,	true	},
    {   "vault",				ROOM_VAULT,				true	},
    {	NULL,			0,			0	}

};

const struct flag_type *room_flagbank[] =
{
    room_flags,
    room2_flags,
    NULL
};



const struct flag_type sector_types[] =
{
    {	"inside",		SECT_INSIDE,		true	},
    {	"city",			SECT_CITY,		true	},
    {	"field",		SECT_FIELD,		true	},
    {	"forest",		SECT_FOREST,		true	},
    {	"hills",		SECT_HILLS,		true	},
    {	"mountain",		SECT_MOUNTAIN,		true	},
    {	"swim",			SECT_WATER_SWIM,	true	},
    {	"noswim",		SECT_WATER_NOSWIM,	true	},
    {   "tundra",		SECT_TUNDRA,		true	},
    {	"air",			SECT_AIR,		true	},
    {	"desert",		SECT_DESERT,		true	},
    {	"netherworld",		SECT_NETHERWORLD,	true	},
    {   "dock",			SECT_DOCK,		true    },
    {   "enchanted_forest",	SECT_ENCHANTED_FOREST, 	true 	},
    {   "toxic_bog",		SECT_TOXIC_BOG, 	true 	},	// @@@NIB : 20070126
    {   "cursed_sanctum",	SECT_CURSED_SANCTUM, 	true 	},	// @@@NIB : 20070126
    {   "bramble",		SECT_BRAMBLE,	 	true 	},	// @@@NIB : 20070126
    {	"swamp",		SECT_SWAMP,		true	},
    {	"acid",			SECT_ACID,		true	},
    {	"lava",			SECT_LAVA,		true	},
    {	"snow",			SECT_SNOW,		true	},
    {	"ice",			SECT_ICE,		true	},
    {	"cave",			SECT_CAVE,		true	},
    {	"underwater",		SECT_UNDERWATER,	true	},
    {	"deep_underwater",	SECT_DEEP_UNDERWATER,	true	},
    {	"jungle",		SECT_JUNGLE,		true	},
    {   "dirt_road",    SECT_DIRT_ROAD,     true    },
    {   "paved_road",   SECT_PAVED_ROAD,    true    },
    {	NULL,			0,			0	}
};


const struct flag_type type_flags[] =
{
    {	"light",		ITEM_LIGHT,		true	},
    {	"scroll",		ITEM_SCROLL,		true	},
    {	"wand",			ITEM_WAND,		true	},
    {	"weapon",		ITEM_WEAPON,		true	},
    {   "ammo",         ITEM_AMMO,          true    },
    {	"treasure",		ITEM_TREASURE,		true	},
    {	"armour",		ITEM_ARMOUR,		true	},
    {	"furniture",		ITEM_FURNITURE,		true	},
    {	"trash",		ITEM_TRASH,		true	},
    {	"container",		ITEM_CONTAINER,		true	},
    {	"fluidcontainer",	ITEM_FLUID_CONTAINER,		true	},
    {	"key",			ITEM_KEY,		true	},
    {	"food",			ITEM_FOOD,		true	},
    {	"money",		ITEM_MONEY,		true	},
    {   "corpse",       ITEM_CORPSE,    true    },
    {	"npccorpse",		ITEM_CORPSE_NPC,	false	},
    {	"pc corpse",		ITEM_CORPSE_PC,		false	},
    {	"pill",			ITEM_PILL,		true	},
    {	"protect",		ITEM_PROTECT,		false	},
    {	"map",			ITEM_MAP,		true	},
    {   "portal",		ITEM_PORTAL,		true	},
    {   "catalyst",		ITEM_CATALYST,		true	},
    {	"roomkey",		ITEM_ROOM_KEY,		true	},
    { 	"gem",			ITEM_GEM,		true	},
    {	"jewelry",		ITEM_JEWELRY,		true	},
    {	"jukebox",		ITEM_JUKEBOX,		true	},
    {	"artifact",		ITEM_ARTIFACT,		true	},
    {   "shares",		ITEM_SHARECERT,		true	},
    {   "flame_room_object",	ITEM_ROOM_FLAME,	false	},
    {   "instrument",		ITEM_INSTRUMENT,	true	},
    {   "seed",			ITEM_SEED,		true	},
    {   "cart",			ITEM_CART,		true	},
    {   "ship",			ITEM_SHIP,		true	},
    {   "room_darkness_object",	ITEM_ROOM_DARKNESS,	true	},
    {   "sextant",		ITEM_SEXTANT,		true	},
    {   "room_roomshield_object",	ITEM_ROOM_ROOMSHIELD,	true	},
    {	"book",			ITEM_BOOK,		true	},
    {	"page",			ITEM_PAGE,		true	},
    {   "stinking_cloud",	ITEM_STINKING_CLOUD,	false	},
    {   "smoke_bomb",       	ITEM_SMOKE_BOMB,	true	},
    {   "herb",			ITEM_HERB,		true	},
    {   "spell_trap",       	ITEM_SPELL_TRAP,	true	},
    {   "withering_cloud",  	ITEM_WITHERING_CLOUD,	false	},
    {   "bank",			ITEM_BANK,		true 	},
    {	"ice_storm",		ITEM_ICE_STORM,		false	},
    {	"flower",		ITEM_FLOWER,		true	},
    {   "trade_type",		ITEM_TRADE_TYPE,	true	},
    {	"mist",			ITEM_MIST,		true	},
    {	"shrine",		ITEM_SHRINE,		true	},
    {   "whistle",		ITEM_WHISTLE, true  },
    {   "shovel",		ITEM_SHOVEL,		true	},
    //{   "tool",			ITEM_TOOL,			true	},	// @@@NIB : 20070215
    {   "tattoo",		ITEM_TATTOO,		true	},
    {   "ink",			ITEM_INK,			true	},
    {   "part",			ITEM_PART,			true	},
	{	"telescope",	ITEM_TELESCOPE,		true	},
	{	"compass",		ITEM_COMPASS,		true	},
	{	"whetstone",	ITEM_WHETSTONE,		true	},
	{	"chisel",		ITEM_CHISEL,		true	},
	{	"pick",			ITEM_PICK,			true	},
	{	"tinderbox",	ITEM_TINDERBOX,		true	},
	{	"drying_cloth",	ITEM_DRYING_CLOTH,	true	},
	{	"needle",		ITEM_NEEDLE,		true	},
	{	"body_part",	ITEM_BODY_PART,		true	},
    {	NULL,			0,			0	}
};

const struct flag_type extra_flags[] =
{
	{	"antievil",		ITEM_ANTI_EVIL,		true	},
	{	"antigood",		ITEM_ANTI_GOOD,		true	},
	{	"antineutral",	ITEM_ANTI_NEUTRAL,	true	},
	{	"bless",		ITEM_BLESS,		    true	},
    {   "bought",       ITEM_SHOP_BOUGHT,   false   },
	{	"burnproof",	ITEM_BURN_PROOF,	true	},
    {   "ephemeral",    ITEM_EPHEMERAL,     true    },      // Indicate the item disappears after use or the next game tick.  Use case is generated ammo that isn't meant to remain after doing damage.
	{	"evil",			ITEM_EVIL,	    	true	},
	{	"freezeproof",	ITEM_FREEZE_PROOF,	true	},
	{	"glow",			ITEM_GLOW,          true	},
	{	"headless",		ITEM_NOSKULL,		false   },
	{	"hidden",		ITEM_HIDDEN,		false   },
	{	"holy",         ITEM_HOLY,          false   },	
	{	"hum",			ITEM_HUM,	    	true	},
	{	"inventory",	ITEM_INVENTORY,		false   },
	{	"invis",		ITEM_INVIS,		    true	},
	{	"magic",		ITEM_MAGIC,	    	true	},
	{	"meltdrop",		ITEM_MELT_DROP,		true	},
	{	"no_restring",	ITEM_NORESTRING,	true	},
	{	"nodrop",		ITEM_NODROP,		true	},
	{	"nokeyring",	ITEM_NOKEYRING,	   	true    },
	{	"nolocate",		ITEM_NOLOCATE,		true	},
	{	"nopurge",		ITEM_NOPURGE,		true	},
	{	"noquest",		ITEM_NOQUEST,		true	},
	{	"noremove",		ITEM_NOREMOVE,		true	},
	{	"nosteal",		ITEM_NOSTEAL,		true	},
	{	"nouncurse",	ITEM_NOUNCURSE,		true	},
	{	"permanent",	ITEM_PERMANENT,     false   },
	{	"planted",		ITEM_PLANTED,		false   },
	{	"rotdeath",		ITEM_ROT_DEATH,		true	},
	{	"shockproof",	ITEM_SHOCK_PROOF,	true	},
    {	NULL,			0,			0	}
};


const struct flag_type extra2_flags[] =
{
	{   "all_remort",		ITEM_ALL_REMORT,	true    },
	{   "buried",	    	ITEM_BURIED,		false   },
    {   "created",          ITEM_CREATED,       false   },
	{   "emits_light",		ITEM_EMITS_LIGHT,	true    },
	{   "enchanted",		ITEM_ENCHANTED,		false   },
	{   "float_user",		ITEM_FLOAT_USER,	true    },
	{   "keep_value",		ITEM_KEEP_VALUE,	true	},
	{   "kept",		    	ITEM_KEPT,		    false	},
	{   "locker",		    ITEM_LOCKER,		true    },
	{   "no_auction",		ITEM_NOAUCTION,		true	},
	{   "no_container",		ITEM_NO_CONTAINER,	true	},
	{   "no_discharge",		ITEM_NO_DISCHARGE,	true	},
	{   "no_donate",		ITEM_NO_DONATE,		true    },
	{   "no_enchant",		ITEM_NO_ENCHANT,	true	},
	{   "no_hunt",		    ITEM_NO_HUNT,		true    },
	{   "no_locker",		ITEM_NOLOCKER,		true	},
	{   "no_loot",	    	ITEM_NO_LOOT,		true	},
	{   "no_lore",		    ITEM_NO_LORE,		true    },
	{   "no_resurrect",		ITEM_NO_RESURRECT,	false   },
	{   "remort_only",		ITEM_REMORT_ONLY,	true    },
	{   "scare",		    ITEM_SCARE, 		true	},
	{   "see_hidden",		ITEM_SEE_HIDDEN,	true    },
	{   "sell_once",		ITEM_SELL_ONCE,		true    },
	{   "singular",	    	ITEM_SINGULAR,		true	},
	{   "super-strong",		ITEM_SUPER_STRONG,	true    },
	{   "sustains",		    ITEM_SUSTAIN,		true	},
	{   "third_eye",		ITEM_THIRD_EYE,		false   },
	{   "trapped",	    	ITEM_TRAPPED,		false   },
	{   "true_sight",		ITEM_TRUESIGHT,		true	},
    {   NULL,			0,			0	}
};

const struct flag_type extra3_flags[] =
{
    {	"always_loot",		ITEM_ALWAYS_LOOT,	true	},
    {	"can_dispel",		ITEM_CAN_DISPEL,	true	},
    {	"exclude_list",		ITEM_EXCLUDE_LIST,	true	},
    {	"force_loot",		ITEM_FORCE_LOOT,	false	},
    {	"instance_obj",		ITEM_INSTANCE_OBJ,	false	},
    {	"keep_equipped",	ITEM_KEEP_EQUIPPED,	true	},
    {   "no_animate",		ITEM_NO_ANIMATE,	false   },
    {	"no_transfer",		ITEM_NO_TRANSFER,	true	},
    {	"rift_update",		ITEM_RIFT_UPDATE,	true	},
    {	"show_in_wilds",	ITEM_SHOW_IN_WILDS,	true	},
    {   "stolen",           ITEM_STOLEN,        true    },
    {   NULL,			0,			0	}
};

const struct flag_type extra4_flags[] =
{
    {   NULL,			0,			0	}
};

const struct flag_type *extra_flagbank[] =
{
    extra_flags,
    extra2_flags,
    extra3_flags,
    extra4_flags,
    NULL
};

const struct flag_type wear_flags[] =
{
    {	"take",			ITEM_TAKE,      		true	},
    {	"finger",		ITEM_WEAR_FINGER,   	true	},
    {	"neck",			ITEM_WEAR_NECK, 		true	},
    {	"body",			ITEM_WEAR_BODY, 		true	},
    {	"head",			ITEM_WEAR_HEAD, 		true	},
    {	"legs",			ITEM_WEAR_LEGS, 		true	},
    {	"feet",			ITEM_WEAR_FEET, 		true	},
    {	"hands",		ITEM_WEAR_HANDS,    	true	},
    {	"arms",			ITEM_WEAR_ARMS, 		true	},
    {	"shield",		ITEM_WEAR_SHIELD,   	true	},
    {	"about",		ITEM_WEAR_ABOUT,    	true	},
    {	"waist",		ITEM_WEAR_WAIST,    	true	},
    {	"wrist",		ITEM_WEAR_WRIST,    	true	},
    {	"wield",		ITEM_WIELD,		        true	},
    {	"hold",			ITEM_HOLD,	    	    true	},
    {   "nosac",		ITEM_NO_SAC,	    	true	},
    {	"float",		ITEM_WEAR_FLOAT,    	true	},
    {   "ring_finger",	ITEM_WEAR_RING_FINGER,  true    },
    {   "back",			ITEM_WEAR_BACK, 		true    },
    {   "shoulder",		ITEM_WEAR_SHOULDER, 	true	},
    {   "face",			ITEM_WEAR_FACE, 		true	},
    {   "eyes",			ITEM_WEAR_EYES, 		true	},
    {   "ear",			ITEM_WEAR_EAR,  		true	},
    {   "ankle",		ITEM_WEAR_ANKLE,    	true	},
    {   "conceals",		ITEM_CONCEALS,  		true	},
    {	"tabard",		ITEM_WEAR_TABARD,   	true	},
    {   "tattoo",       ITEM_WEAR_TATTOO,       true    },
    {	NULL,			0,          			0       }
};


const struct flag_type apply_flags[] =
{
    {	"none",			    APPLY_NONE,		    true	},
    {	"ac",			    APPLY_AC,		    true	},
    {	"strength",		    APPLY_STR,		    true	},
    {	"dexterity",	    APPLY_DEX,		    true	},
    {	"intelligence",	    APPLY_INT,		    true	},
    {	"wisdom",		    APPLY_WIS,		    true	},
    {	"constitution",	    APPLY_CON,		    true	},
    {	"sex",			    APPLY_SEX,		    true	},
    {	"mana",			    APPLY_MANA,		    true	},
    {	"hp",			    APPLY_HIT,		    true	},
    {	"move",			    APPLY_MOVE,		    true	},
    {	"hitroll",		    APPLY_HITROLL,		true	},
    {	"damroll",		    APPLY_DAMROLL,		true	},
    {	"skill",		    APPLY_SKILL,		false	},
    {	"spellaffect",		APPLY_SPELL_AFFECT,	false	},
    {   "xpboost",          APPLY_XPBOOST,      true    },
    {   "saves",            APPLY_SAVES,        false   },
    {	NULL,			0,			0	}
};


const struct flag_type wear_loc_flags[] =
{
    {	"none",		WEAR_NONE,		true	},
    {	"light",	WEAR_LIGHT,		true	},
    {	"lfinger",	WEAR_FINGER_L,		true	},
    {	"rfinger",	WEAR_FINGER_R,		true	},
    {	"neck1",	WEAR_NECK_1,		true	},
    {	"neck2",	WEAR_NECK_2,		true	},
    {	"body",		WEAR_BODY,		true	},
    {	"head",		WEAR_HEAD,		true	},
    {	"legs",		WEAR_LEGS,		true	},
    {	"feet",		WEAR_FEET,		true	},
    {	"hands",	WEAR_HANDS,		true	},
    {	"arms",		WEAR_ARMS,		true	},
    {	"shield",	WEAR_SHIELD,		true	},
    {	"about",	WEAR_ABOUT,		true	},
    {	"waist",	WEAR_WAIST,		true	},
    {	"lwrist",	WEAR_WRIST_L,		true	},
    {	"rwrist",	WEAR_WRIST_R,		true	},
    {	"wielded",	WEAR_WIELD,		true	},
    {	"hold",		WEAR_HOLD,		true	},
    {	"secondary",	WEAR_SECONDARY,		true	},	// NIB : 20070121 : was missing
    {	"ringfinger",	WEAR_RING_FINGER,	true	},	// NIB : 20070121 : was missing
    {	"back",		WEAR_BACK,		true	},	// NIB : 20070121 : was missing
    {	"shoulder",	WEAR_SHOULDER,		true	},	// NIB : 20070121 : was missing
    {	"lankle",	WEAR_ANKLE_L,		true	},
    {	"rankle",	WEAR_ANKLE_R,		true	},
    {	"lear",		WEAR_EAR_L,		true	},
    {	"rear",		WEAR_EAR_R,		true	},
    {	"eyes",		WEAR_EYES,		true	},
    {	"face",		WEAR_FACE,		true	},
    {	"tattoohead",	WEAR_TATTOO_HEAD,	true	},
    {	"tattootorso",	WEAR_TATTOO_TORSO,	true	},
    {	"tattooupperlarm",	WEAR_TATTOO_UPPER_ARM_L,	true	},
    {	"tattooupperrarm",	WEAR_TATTOO_UPPER_ARM_R,	true	},
    {	"tattooupperlleg",	WEAR_TATTOO_UPPER_LEG_L,	true	},
    {	"tattooupperrleg",	WEAR_TATTOO_UPPER_LEG_R,	true	},
    {	"lodgedhead",	WEAR_LODGED_HEAD,	true	},
    {	"lodgedtorso",	WEAR_LODGED_TORSO,	true	},
    {	"lodgedlarm",	WEAR_LODGED_ARM_L,	true	},
    {	"lodgedrarm",	WEAR_LODGED_ARM_R,	true	},
    {	"lodgedlleg",	WEAR_LODGED_LEG_L,	true	},
    {	"lodgedrleg",	WEAR_LODGED_LEG_R,	true	},
    {	"entangled",	WEAR_ENTANGLED,		true	},
    {	"concealed",	WEAR_CONCEALED,		true	},
    {	"floating",	WEAR_FLOATING,		true	},
    {   "tattoolowerlarm",   WEAR_TATTOO_LOWER_ARM_L,       true    },
    {   "tattoolowerrarm",  WEAR_TATTOO_LOWER_ARM_R,      true    },
    {   "tattoolowerlleg",   WEAR_TATTOO_LOWER_LEG_L,      true    },
    {   "tattoolowerrleg",   WEAR_TATTOO_LOWER_LEG_R,      true    },
    {   "tattoolshoulder",   WEAR_TATTOO_SHOULDER_L,      true    },
    {   "tattoorshoulder",   WEAR_TATTOO_SHOULDER_R,      true    },
    {   "tattooback",   WEAR_TATTOO_BACK,       true    },
    {   "tattooneck",   WEAR_TATTOO_NECK,       true    },
    {	NULL,		0,		0	}
};


const struct flag_type container_flags[] =
{
    {   "catalysts",    CONT_CATALYSTS, true    },
    {	"closeable",	CONT_CLOSEABLE,	true	},
    {	"closed",		CONT_CLOSED,	true	},
    {	"closelock",	CONT_CLOSELOCK,	true	},	// @@@NIB : 20070126
    {   "no_duplicates",CONT_NO_DUPLICATES, true    },
    {	"pushopen",		CONT_PUSHOPEN,	true	},	// @@@NIB : 20070126
    {	"puton",		CONT_PUT_ON,	true	},
    {   "singular",     CONT_SINGULAR,  true    },
    {   "transparent",  CONT_TRANSPARENT,   true    },
    {	NULL,			0,		0	}
};


const struct flag_type ac_type[] =
{
    {   "pierce",        AC_PIERCE,            true    	},
    {   "bash",          AC_BASH,              true    	},
    {   "slash",         AC_SLASH,             true    	},
    {   "exotic",        AC_EXOTIC,            true    	},
    {   NULL,            0,                    0       	}
};


const struct flag_type size_flags[] =
{
    {   "tiny",          SIZE_TINY,            true    	},
    {   "small",         SIZE_SMALL,           true    	},
    {   "medium",        SIZE_MEDIUM,          true    	},
    {   "large",         SIZE_LARGE,           true    	},
    {   "huge",          SIZE_HUGE,            true    	},
    {   "giant",         SIZE_GIANT,           true    	},
    {   NULL,              0,                    0     	},
};


const struct flag_type ranged_weapon_class[] =
{
    {   NULL,          	 0,                     false    }
};

const struct flag_type ammo_types[] =
{
    {   "none",       	AMMO_NONE,              true    },
    {   "acid",         AMMO_ACID,              false   },  // Only usable by a sprayer
    {   "arrow",       	AMMO_ARROW,             true    },
    {   "bolt",       	AMMO_BOLT,              true    },
    {   "bullet",      	AMMO_BULLET,            true    },
    {   "dart",       	AMMO_DART,              true    },
    {   "fuel",         AMMO_FUEL,              false   },  // Only usable by a flame thrower
    {   "potion",       AMMO_POTION,            false   },  // Only usable by a sprayer
    {   NULL,          	0,                      false   }
};

const struct flag_type weapon_class[] =
{
    {   "acidsprayer",  WEAPON_ACID_SPRAYER,    true    },
    {   "axe",	    	WEAPON_AXE,		        true    },
    {	"blowgun",  	WEAPON_BLOWGUN,	    	true	},
    {	"bow",        	WEAPON_BOW, 	    	true	},
    {   "chemosprayer", WEAPON_POTION_SPRAYER,  true    },
    {	"crossbow",  	WEAPON_CROSSBOW,		true	},
    {   "dagger",   	WEAPON_DAGGER,  		true    },
    {   "exotic",   	WEAPON_EXOTIC,  		true    },
    {   "flail",    	WEAPON_FLAIL,	    	true    },
    {   "flamethrower", WEAPON_FLAMETHROWER,    true    },
    {   "gun",          WEAPON_GUN,             true    },
    {	"harpoon",  	WEAPON_HARPOON,	    	true	},
    {   "mace",	    	WEAPON_MACE,    		true    },
    {   "polearm",  	WEAPON_POLEARM,	    	true    },
    {	"quarterstaff",	WEAPON_QUARTERSTAFF,	true	},
    {   "spear",    	WEAPON_SPEAR,   		true    },
    {	"stake",    	WEAPON_STAKE,	    	true	},
    {   "sword",    	WEAPON_SWORD,   		true    },
    {   "whip",	    	WEAPON_WHIP,	    	true    },
    {   NULL,	    	0,          			false       }
};

const struct flag_type weapon_type2[] =
{
    {	"acidic",	WEAPON_ACIDIC,		true	},	// @@@NIB : 20070209
    {	"annealed",	WEAPON_ANNEALED,	true	},	// @@@NIB : 20070209
//    {	"barbed",	WEAPON_BARBED,		true	},	// @@@NIB : 20070209
    {	"blaze",	WEAPON_BLAZE,		true	},	// @@@NIB : 20070209
//    {	"chipped",	WEAPON_CHIPPED,		true	},	// @@@NIB : 20070209
    {	"dull",		WEAPON_DULL,		true	},	// @@@NIB : 20070209
    {	"offhand",	WEAPON_OFFHAND,		true	},	// @@@NIB : 20070209
    {	"onehand",	WEAPON_ONEHAND,		true	},	// @@@NIB : 20070209
    {	"poison",	WEAPON_POISON,		true	},
    {	"resonate",	WEAPON_RESONATE,	true	},	// @@@NIB : 20070209
    {	"shocking",	WEAPON_SHOCKING,      	true    },
    {	"suckle",	WEAPON_SUCKLE,		true	},	// @@@NIB : 20070209
    {	"throwable",	WEAPON_THROWABLE,	true	},
    {   "flaming",      WEAPON_FLAMING,       	true    },
    {   "frost",        WEAPON_FROST,         	true    },
    {   "sharp",        WEAPON_SHARP,         	true    },
    {   "twohands",     WEAPON_TWO_HANDS,     	true    },
    {   "vampiric",     WEAPON_VAMPIRIC,      	true    },
    {   "vorpal",       WEAPON_VORPAL,        	true    },
    {   NULL,           0,          		0	},
};


const struct flag_type res_flags[] =
{
    {	"summon",	RES_SUMMON,		true	},
    {   "charm",        RES_CHARM,            	true    },
    {   "magic",        RES_MAGIC,            	true    },
    {   "weapon",       RES_WEAPON,           	true    },
    {   "bash",         RES_BASH,             	true    },
    {   "pierce",       RES_PIERCE,           	true    },
    {   "slash",        RES_SLASH,            	true    },
    {   "fire",         RES_FIRE,             	true    },
    {   "cold",         RES_COLD,             	true    },
    {   "light",     	RES_LIGHT,            	true    },
    {   "lightning",    RES_LIGHTNING,        	true    },
    {   "acid",         RES_ACID,             	true    },
    {   "poison",       RES_POISON,           	true    },
    {   "negative",     RES_NEGATIVE,         	true    },
    {   "holy",         RES_HOLY,             	true    },
    {   "energy",       RES_ENERGY,           	true    },
    {   "mental",       RES_MENTAL,           	true    },
    {   "disease",      RES_DISEASE,          	true    },
    {   "drowning",     RES_DROWNING,         	true    },
    {	"sound",	RES_SOUND,		true	},
    {	"wood",		RES_WOOD,		true	},
    {	"silver",	RES_SILVER,		true	},
    {	"iron",		RES_IRON,		true	},
    {   "kill",		RES_KILL, 		true    },
    {	"water",	RES_WATER,		true	},
    {	"air",		RES_AIR,		true	},	// @@@NIB : 20070125
    {	"earth",	RES_EARTH,		true	},	// @@@NIB : 20070125
    {	"plant",	RES_PLANT,		true	},	// @@@NIB : 20070125
    {   "suffocation",  RES_SUFFOCATION,    true    },
    {   NULL,          	0,            		0    	}
};


const struct flag_type vuln_flags[] =
{
    {	"summon",	VULN_SUMMON,		true	},
    {	"charm",	VULN_CHARM,		true	},
    {   "magic",        VULN_MAGIC,           	true    },
    {   "weapon",       VULN_WEAPON,          	true    },
    {   "bash",         VULN_BASH,            	true    },
    {   "pierce",       VULN_PIERCE,          	true    },
    {   "slash",        VULN_SLASH,           	true    },
    {   "fire",         VULN_FIRE,            	true    },
    {   "cold",         VULN_COLD,            	true    },
    {   "light",        VULN_LIGHT,           	true    },
    {   "lightning",    VULN_LIGHTNING,       	true    },
    {   "acid",         VULN_ACID,            	true    },
    {   "poison",       VULN_POISON,          	true    },
    {   "negative",     VULN_NEGATIVE,        	true    },
    {   "holy",         VULN_HOLY,            	true    },
    {   "energy",       VULN_ENERGY,          	true    },
    {   "mental",       VULN_MENTAL,          	true    },
    {   "disease",      VULN_DISEASE,         	true    },
    {   "drowning",     VULN_DROWNING,        	true    },
    {	"sound",	VULN_SOUND,		true	},
    {   "wood",         VULN_WOOD,            	true    },
    {   "silver",       VULN_SILVER,          	true    },
    {   "iron",         VULN_IRON,		true    },
    {   "kill",		VULN_KILL,	       	true    },
    {	"water",	VULN_WATER,		true	},
    {	"air",		VULN_AIR,		true	},	// @@@NIB : 20070125
    {	"earth",	VULN_EARTH,		true	},	// @@@NIB : 20070125
    {	"plant",	VULN_PLANT,		true	},	// @@@NIB : 20070125
    {   "suffocation",  VULN_SUFFOCATION,   true    },
    {   NULL,           0,                    	0       }
};

const struct flag_type spell_position_flags[] =
{
    {   "dead",       	POS_DEAD,            	true    },
    {   "sleeping",     POS_SLEEPING,        	true    },
    {   "resting",      POS_RESTING,         	true    },
    {   "sitting",      POS_SITTING,         	true    },
    {   "standing",     POS_STANDING,        	true    },
    {   NULL,           0,                    	0       }
};

const struct flag_type log_flags[] =
{
    {   "normal",       LOG_NORMAL,           	true    },
    {   "always",       LOG_ALWAYS,           	true    },
    {   "never",        LOG_NEVER,            	true    },
    {   NULL,           0,                    	0       }
};

const struct flag_type position_flags[] =
{
    {   "dead",       	POS_DEAD,            	false   },
    {   "mortal",       POS_MORTAL,          	false   },
    {   "incap",        POS_INCAP,           	false   },
    {   "stunned",      POS_STUNNED,         	false   },
    {   "sleeping",     POS_SLEEPING,        	true    },
    {   "resting",      POS_RESTING,         	true    },
    {   "sitting",      POS_SITTING,         	true    },
    {   "fighting",     POS_FIGHTING,        	false   },
    {   "standing",     POS_STANDING,        	true    },
    {   "feigned",	POS_FEIGN,        	true    },
    {   "heldup",	POS_HELDUP,        	false   },
    {   NULL,           0,                    	0       }
};


const struct flag_type portal_flags[]=
{
    {	"arearandom",		GATE_AREARANDOM,	true	},
    {	"candragitems",		GATE_CANDRAGITEMS,	true	},
    {	"force_brief",		GATE_FORCE_BRIEF,	true	},
    {	"gravity",			GATE_GRAVITY,		false	},	// @@@NIB : 20070126 : Not imped yet
    {	"no_curse",			GATE_NOCURSE,		true	},
    {	"noprivacy",		GATE_NOPRIVACY,		true	},	// @@@NIB : 20070126
    {	"nosneak",			GATE_NOSNEAK,		true	},	// @@@NIB : 20070126
    {	"random",			GATE_RANDOM,		true	},
    {	"safe",				GATE_SAFE,			false	},	// @@@NIB : 20070126 : Not imped yet
    {	"silententry",		GATE_SILENTENTRY,	true	},	// @@@NIB : 20070126
    {	"silentexit",		GATE_SILENTEXIT,	true	},	// @@@NIB : 20070126
    {	"sneak",			GATE_SNEAK,			true	},	// @@@NIB : 20070126
    {	"turbulent",		GATE_TURBULENT,		false	},	// @@@NIB : 20070126 : Not imped yet
    {   "buggy",			GATE_BUGGY,			true	},
    {   "go_with",			GATE_GOWITH,		true	},
    {   "normal_exit",		GATE_NORMAL_EXIT,	true	},
    {   NULL,		0,			0	}
};

const struct flag_type furniture_flags[]=
{
    {   "keep_occupants",    FURNITURE_KEEP_OCCUPANTS, true },
    {	NULL,		0,		0	}
};


const struct flag_type furniture_action_flags[]=
{
    {   "above",    FURNITURE_ABOVE, true },
    {   "at",       FURNITURE_AT, true },
    {   "in",       FURNITURE_IN, true },
    {   "on",       FURNITURE_ON, true },
    {   "under",    FURNITURE_UNDER, true },
    {	NULL,		0,		0	}
};


const	struct	flag_type	apply_types	[]	=
{
    {	"affects",	TO_AFFECTS,	true	},
    {	"object",	TO_OBJECT,	true	},
    {	"object2",	TO_OBJECT2,	true	},
    {	"object3",	TO_OBJECT3,	true	},
    {	"object4",	TO_OBJECT4,	true	},
    {	"immune",	TO_IMMUNE,	true	},
    {	"resist",	TO_RESIST,	true	},
    {	"vuln",		TO_VULN,	true	},
    {	"weapon",	TO_WEAPON,	true	},
    {	NULL,		0,		true	}
};

const	struct	flag_type	food_buff_types	[]	=
{
    {	"affects",	TO_AFFECTS,	true	},
    {	"immune",	TO_IMMUNE,	true	},
    {	"resist",	TO_RESIST,	true	},
    {	"vuln",		TO_VULN,	true	},
    {	NULL,		0,		true	}
};



const struct flag_type room_condition_flags[] =
{
    { 	"season",	CONDITION_SEASON,	true	},
    { 	"sky",		CONDITION_SKY,		true    },
    { 	"hour",		CONDITION_HOUR,		true	},
    { 	"script",	CONDITION_SCRIPT,	true	},
    {	NULL,		0,			true    }
};


const struct flag_type      place_flags[]           =
{
    { 	"nowhere",    			PLACE_NOWHERE, 				true	},
    { 	"wilderness",			PLACE_WILDERNESS,			true	},
    { 	"first_continent", 		PLACE_FIRST_CONTINENT, 		true	},
    { 	"second_continent", 	PLACE_SECOND_CONTINENT, 	true	},
    { 	"third_continent",	 	PLACE_THIRD_CONTINENT, 		true	},
    { 	"fourth_continent", 	PLACE_FOURTH_CONTINENT, 	true	},
    { 	"island", 				PLACE_ISLAND, 				true	},
    { 	"other_plane",			PLACE_OTHER_PLANE,			true	},
    { 	"abyss",				PLACE_ABYSS,				true	},
    { 	"eden",					PLACE_EDEN,					true	},
    { 	"netherworld",			PLACE_NETHERWORLD,			true	},
    {  	NULL,			0, 				false 	}
};


const struct flag_type	token_flags[] =
{
    {   "hide_name",        TOKEN_HIDE_NAME,        true    },
    {	"no_skill_test",	TOKEN_NOSKILLTEST,		true	},
    {	"permanent",		TOKEN_PERMANENT,		true	},
    {	"purge_death",		TOKEN_PURGE_DEATH,		true	},
    {	"purge_idle",		TOKEN_PURGE_IDLE,		true	},
    {	"purge_quit",		TOKEN_PURGE_QUIT,		true	},
    {	"purge_reboot",		TOKEN_PURGE_REBOOT,		true	},
    {	"purge_rift",		TOKEN_PURGE_RIFT,		true	},
    {	"reverse_timer",	TOKEN_REVERSETIMER,		true	},
    {	"see_all",			TOKEN_SEE_ALL,			true	},
    {	"singular",			TOKEN_SINGULAR,			true	},
    {	"spellbeats",		TOKEN_SPELLBEATS,		true	},
    {	NULL,			0,				false	}
};




const struct flag_type project_flags[] =
{
    {	"open",			PROJECT_OPEN,		true	},
    {	"assigned",		PROJECT_ASSIGNED,	true	},
    {	"hold",			PROJECT_HOLD,		true	},
    {	NULL,			0,			false	}
};


const struct flag_type damage_classes[] = {
	{"acid", DAM_ACID, true},
	{"air", DAM_AIR, true},
	{"bash", DAM_BASH, true},
	{"charm", DAM_CHARM, true},
	{"cold", DAM_COLD, true},
	{"disease", DAM_DISEASE, true},
	{"drowning", DAM_DROWNING, true},
	{"earth", DAM_EARTH, true},
	{"energy", DAM_ENERGY, true},
	{"fire", DAM_FIRE, true},
	{"holy", DAM_HOLY, true},
	{"light", DAM_LIGHT, true},
	{"lightning", DAM_LIGHTNING, true},
	{"magic", DAM_MAGIC, true},
	{"mental", DAM_MENTAL, true},
	{"negative", DAM_NEGATIVE, true},
	{"pierce", DAM_PIERCE, true},
	{"plant", DAM_PLANT, true},
	{"poison", DAM_POISON, true},
	{"slash", DAM_SLASH, true},
	{"sound", DAM_SOUND, true},
	{"water", DAM_WATER, true},
    {"suffocating", DAM_SUFFOCATING, true},
    {"death", DAM_DEATH, true},
	{NULL, 0, 0}
};

const struct flag_type corpse_types[] = {
	{"charred",RAWKILL_CHARRED,true},
	{"dissolve",RAWKILL_DISSOLVE,true},
	{"explode",RAWKILL_EXPLODE,true},
	{"flay",RAWKILL_FLAY,true},
	{"frozen",RAWKILL_FROZEN,true},
	{"iceblock",RAWKILL_ICEBLOCK,true},
	{"incinerate",RAWKILL_INCINERATE,true},
	{"melted",RAWKILL_MELTED,true},
	{"nocorpse",RAWKILL_NOCORPSE,true},
	{"normal",RAWKILL_NORMAL,true},
	{"shatter",RAWKILL_SHATTER,true},
	{"skeletal",RAWKILL_SKELETAL,true},
	{"stone",RAWKILL_STONE,true},
	{"withered",RAWKILL_WITHERED,true},
	{NULL, 0, 0},
};


const struct flag_type time_of_day_flags[] = {
	{ "aftermidnight",	TOD_AFTERMIDNIGHT, true},
	{ "afternoon",		TOD_AFTERNOON, true},
	{ "dawn",		TOD_DAWN, true},
	{ "day",		TOD_DAY, true},
	{ "dusk",		TOD_DUSK, true},
	{ "evening",		TOD_EVENING, true},
	{ "midnight",		TOD_MIDNIGHT, true},
	{ "morning",		TOD_MORNING, true},
	{ "night",		TOD_NIGHT, true},
	{ "noon",		TOD_NOON, true},
	{ NULL,			0, false},
};

const struct flag_type death_types[] = {
	 { "alive",	DEATHTYPE_ALIVE, true},
	 { "attack",	DEATHTYPE_ATTACK, true},
	 { "behead",	DEATHTYPE_BEHEAD, true},
	 { "breath",	DEATHTYPE_BREATH, true},
	 { "damage",	DEATHTYPE_DAMAGE, true},
	 { "killspell",	DEATHTYPE_KILLSPELL, true},
	 { "magic",	DEATHTYPE_MAGIC, true},
	 { "rawkill",	DEATHTYPE_RAWKILL, true},
	 { "rocks",	DEATHTYPE_ROCKS, true},
	 { "slit",	DEATHTYPE_SLIT, true},
	 { "smite",	DEATHTYPE_SMITE, true},
	 { "stake",	DEATHTYPE_STAKE, true},
	 { "toxin",	DEATHTYPE_TOXIN, true},
//	 { "trap",	DEATHTYPE_TRAP, true},
	{ NULL,			0, false},
};

const struct flag_type tool_types[] = {
	{ "none",		TOOL_NONE, true },
	{ "whetstone",		TOOL_WHETSTONE, true },
	{ "chisel",		TOOL_CHISEL, true },
	{ "pick",		TOOL_PICK, true },
	{ "shovel",		TOOL_SHOVEL, true },
	{ "tinderbox",		TOOL_TINDERBOX, true },
	{ "drying_cloth",	TOOL_DRYING_CLOTH, true },
	{ "small_needle",	TOOL_SMALL_NEEDLE, true },
	{ "large_needle",	TOOL_LARGE_NEEDLE, true },
	{ NULL,			0, false },
};

const struct flag_type catalyst_types[] = {
	{ "none",	CATALYST_NONE, true},
	{ "acid",	CATALYST_ACID, true},
	{ "air",	CATALYST_AIR, true},
	{ "astral",	CATALYST_ASTRAL, true},
	{ "blood",	CATALYST_BLOOD, true},
	{ "body",	CATALYST_BODY, true},
	{ "chaos",	CATALYST_CHAOS, true},
	{ "cosmic",	CATALYST_COSMIC, true},
	{ "darkness",	CATALYST_DARKNESS, true},
	{ "death",	CATALYST_DEATH, true},
	{ "earth",	CATALYST_EARTH, true},
	{ "energy",	CATALYST_ENERGY, true},
	{ "fire",	CATALYST_FIRE, true},
	{ "holy",	CATALYST_HOLY, true},
	{ "ice",	CATALYST_ICE, true},
	{ "law",	CATALYST_LAW, true},
	{ "light",	CATALYST_LIGHT, true},
	{ "mana",	CATALYST_MANA, true},
	{ "metallic",	CATALYST_METALLIC, true},
	{ "mind",	CATALYST_MIND, true},
	{ "nature",	CATALYST_NATURE, true},
	{ "shock",	CATALYST_SHOCK, true},
	{ "soul",	CATALYST_SOUL, true},
	{ "sound",	CATALYST_SOUND, true},
	{ "toxin",	CATALYST_TOXIN, true},
	{ "water",	CATALYST_WATER, true},
	{ NULL,		0, false },
};


const struct flag_type catalyst_method_types[] = {
	{ "carry",	CATALYST_CARRY, true},
	{ "room",	CATALYST_ROOM, true},
	{ "hold",	CATALYST_HOLD, true},
	{ "containers",	CATALYST_CONTAINERS, true},
	{ "worn",	CATALYST_WORN, true},
	{ "active",	CATALYST_ACTIVE, true},
	{ NULL,		0, false },
};

const struct flag_type boolean_types[] = {
	{ "true",	true, true},
	{ "false",	false, true},
	{ "yes",	true, true},
	{ "no",		false, true},
	{ NULL,		0, false },
};


const struct flag_type tattoo_loc_flags[] =
{
    {	"head",		WEAR_TATTOO_HEAD,	true	},
    {	"body",		WEAR_TATTOO_TORSO,	true	},
    {	"torso",	WEAR_TATTOO_TORSO,	true	},
    {	"upper_left_arm",	WEAR_TATTOO_UPPER_ARM_L,	true	},
    {	"upper_right_arm",	WEAR_TATTOO_UPPER_ARM_R,	true	},
    {	"upper_left_leg",	WEAR_TATTOO_UPPER_LEG_L,	true	},
    {	"upper_right_leg",	WEAR_TATTOO_UPPER_LEG_R,	true	},
    {   "lower_left_arm",         WEAR_TATTOO_LOWER_ARM_L,       true    },
    {   "lower_right_arm",         WEAR_TATTOO_LOWER_ARM_R,      true    },
    {   "lower_left_leg",        WEAR_TATTOO_LOWER_LEG_L,      true    },
    {   "lower_right_leg",       WEAR_TATTOO_LOWER_LEG_R,        true    },
    {   "left_shoulder",      WEAR_TATTOO_SHOULDER_L,        true    },
    {   "right_shoulder",       WEAR_TATTOO_SHOULDER_R,        true    },
    {   "back",      WEAR_TATTOO_BACK,        true    },
    {   "neck",      WEAR_TATTOO_NECK,        true    },
    {	NULL,		0,		0	}
};

const struct flag_type affgroup_flags[] =
{
	{ "racial",     AFFGROUP_RACIAL,	true	},
	{ "metaracial",	AFFGROUP_METARACIAL,	true	},
	{ "biological",	AFFGROUP_BIOLOGICAL,	true	},
	{ "mental",     AFFGROUP_MENTAL,	true	},
	{ "divine",     AFFGROUP_DIVINE,	true	},
	{ "magical",	AFFGROUP_MAGICAL,	true	},
	{ "physical",	AFFGROUP_PHYSICAL,	true	},
	{ "inherent",	AFFGROUP_INHERENT,	true	},
	{ "enchant",	AFFGROUP_ENCHANT,	true	},
	{ "weapon",     AFFGROUP_WEAPON,	true	},
	{ "portal",     AFFGROUP_PORTAL,	true	},
	{ "container",	AFFGROUP_CONTAINER,	true	},
	{ NULL,		0,			false	}
};


const struct flag_type affgroup_mobile_flags[] =
{
	{ "racial",	AFFGROUP_RACIAL,	true	},
	{ "metaracial",	AFFGROUP_METARACIAL,	true	},
	{ "biological",	AFFGROUP_BIOLOGICAL,	true	},
	{ "mental",	AFFGROUP_MENTAL,	true	},
	{ "divine",	AFFGROUP_DIVINE,	true	},
	{ "magical",	AFFGROUP_MAGICAL,	true	},
	{ "physical",	AFFGROUP_PHYSICAL,	true	},
	{ NULL,		0,			false	}
};

const struct flag_type affgroup_object_flags[] =
{
	{ "inherent",	AFFGROUP_INHERENT,	true	},
	{ "enchant",	AFFGROUP_ENCHANT,	true	},
	{ "weapon",	AFFGROUP_WEAPON,	true	},
	{ "portal",	AFFGROUP_PORTAL,	true	},
	{ "container",	AFFGROUP_CONTAINER,	true	},
	{ NULL,		0,			false	}
};

const struct flag_type spell_target_types[] = {
	{ "defensive",		TAR_CHAR_DEFENSIVE,	true	},
	{ "formation",		TAR_CHAR_FORMATION,	true	},
	{ "ignore",		TAR_IGNORE,		true	},
	{ "inventory",		TAR_OBJ_INV,		true	},
	{ "char_world",		TAR_IGNORE_CHAR_DEF,	true	},
	{ "obj_char_off",	TAR_OBJ_CHAR_OFF,	true	},
	{ "obj_defensive",	TAR_OBJ_CHAR_DEF,	true	},
	{ "obj_ground",		TAR_OBJ_GROUND,		true	},
	{ "offensive",		TAR_CHAR_OFFENSIVE,	true	},
	{ "self",		TAR_CHAR_SELF,		true	},
	{ NULL,			0,			false	}
};

const struct flag_type song_target_types[] = {
	{ "defensive",		TAR_CHAR_DEFENSIVE,	true	},
	{ "formation",		TAR_CHAR_FORMATION,	true	},
	{ "ignore",			TAR_IGNORE,		true	},
	{ "obj_char_off",	TAR_OBJ_CHAR_OFF,	true	},
	{ "obj_defensive",	TAR_OBJ_CHAR_DEF,	true	},
	{ "offensive",		TAR_CHAR_OFFENSIVE,	true	},
	{ "self",		TAR_CHAR_SELF,		true	},
	{ NULL,			0,			false	}
};

const struct flag_type moon_phases[] = {
	{ "new",		MOON_NEW,		true	},
	{ "waxing_crescent",	MOON_WAXING_CRESCENT,	true	},
	{ "first_quarter",	MOON_FIRST_QUARTER,	true	},
	{ "waxing_gibbous",	MOON_WAXING_GIBBOUS,	true	},
	{ "full",		MOON_FULL,		true	},
	{ "waning_gibbous",	MOON_WANING_GIBBOUS,	true	},
	{ "last_quarter",	MOON_LAST_QUARTER,	true	},
	{ "waning_crescent",	MOON_WANING_CRESCENT,	true	},
	{ NULL,			0,			false	}
};

const struct flag_type player_conditions[] = {
	{ "drunk",	COND_DRUNK,		true },
	{ "full",	COND_FULL,		true },
	{ "thirst",	COND_THIRST,	true },
	{ "hunger",	COND_HUNGER,	true },
	{ "stoned",	COND_STONED,	true },
	{ NULL,		-1,				false }
};

const struct flag_type instrument_types[] = {
	{ "none",		INSTRUMENT_NONE,		false },
	{ "reed",		INSTRUMENT_WIND_REED,	true },
	{ "flute",		INSTRUMENT_WIND_FLUTE,	true },
	{ "brass",		INSTRUMENT_WIND_BRASS,	true },
	{ "drum",		INSTRUMENT_DRUM,		true },
	{ "percussion",	INSTRUMENT_PERCUSSION,	true },
	{ "chorded",	INSTRUMENT_CHORDED,		true },
	{ "string",		INSTRUMENT_STRING,		true },
	{ NULL,			0,						false }
};

const struct flag_type instrument_flags[] = {
	{ "onehand",	INSTRUMENT_ONEHANDED,	true	},
    { "tuned",      INSTRUMENT_TUNED,       false },        // Can only be set with commands
	{ NULL,			0,						false }
};

const struct flag_type corpse_object_flags[] = {
	{ "chaotic",	CORPSE_CHAOTICDEATH,	true },
	{ "owner_loot",	CORPSE_OWNERLOOT,		true },
	{ "pk",			CORPSE_PKDEATH,			true },
	{ "arena",		CORPSE_ARENADEATH,		true },
	{ "immortal",	CORPSE_IMMORTAL,		true },
	{ NULL,			0,						false }

};



const struct flag_type skill_entry_flags[] = {
	{"practice",		SKILL_PRACTICE,			true},
	{"improve",			SKILL_IMPROVE,			true},
	{"favourite",		SKILL_FAVOURITE,			false},	// This is set manually
    {"cross_class",     SKILL_CROSS_CLASS,      true},  // Borrowed from SKILL_DATA for instances where an affect makes the skill cross class
	{ NULL,				0,			false }
};


const struct flag_type shop_flags[] =
{
    { "disabled",       SHOPFLAG_DISABLED,      true    },
	{ "hide_shop",		SHOPFLAG_HIDE_SHOP,		true	},
	{ "no_haggle",		SHOPFLAG_NO_HAGGLE,		true	},
	{ "stock_only",		SHOPFLAG_STOCK_ONLY,	true	},
	{ NULL,				0,						false	}
};

const struct flag_type blueprint_section_flags[] =
{
	{ NULL,				0,						false	}
};

const struct flag_type blueprint_section_types[] =
{
    { "static",         BSTYPE_STATIC,          true    },
    { "maze",           BSTYPE_MAZE,            true    },
	{ NULL,				0,						false	}
};

const struct flag_type blueprint_flags[] =
{
    {"scripted_layout", BLUEPRINT_SCRIPTED_LAYOUT, false},
    {NULL,              0,                         false}
};

const struct flag_type instance_flags[] =
{
	{ "completed",			INSTANCE_COMPLETED,			false	},
	{ "failed", 			INSTANCE_FAILED,			false	},
	{ "destroy",			INSTANCE_DESTROY,			false	},
	{ "idle_on_complete",	INSTANCE_IDLE_ON_COMPLETE,	true	},
	{ "no_idle",			INSTANCE_NO_IDLE,			true	},
	{ "no_save",			INSTANCE_NO_SAVE,			true	},
	{ NULL,					0,							false	}
};

const struct flag_type dungeon_flags[] =
{
    { "commenced",          DUNGEON_COMMENCED,          false   },
	{ "completed",			DUNGEON_COMPLETED,			false	},
	{ "destroy",			DUNGEON_DESTROY,			false	},
	{ "failed", 			DUNGEON_FAILED, 			false	},
    { "failure_on_empty",   DUNGEON_FAILURE_ON_EMPTY,   true    },
    { "failure_on_wipe",    DUNGEON_FAILURE_ON_WIPE,    true    },
    { "group_commence",     DUNGEON_GROUP_COMMENCE,     true    },
	{ "idle_on_complete",	DUNGEON_IDLE_ON_COMPLETE,	true	},
	{ "no_idle",			DUNGEON_NO_IDLE,			true	},
	{ "no_save",			DUNGEON_NO_SAVE,			true	},
    { "scripted_levels",    DUNGEON_SCRIPTED_LEVELS,    false   },
    { "shared",             DUNGEON_SHARED,             true    },
	{ NULL,					0,							false	}
};

const struct flag_type transfer_modes[] =
{
	{ "silent",			TRANSFER_MODE_SILENT,	true	},
	{ "portal",			TRANSFER_MODE_PORTAL,	true	},
	{ "movement",		TRANSFER_MODE_MOVEMENT,	true	},
	{ NULL,				0,						false	}
};

const struct flag_type ship_class_types[] =
{
	{ "sailboat",		SHIP_SAILING_BOAT,		true	},
	{ "airship",		SHIP_AIR_SHIP,			true	},
	{ NULL,				0,						false	}
};

const struct flag_type ship_flags[] =
{
	{ "protected",		SHIP_PROTECTED,			true	},
	{ NULL,				0,						false	}
};

const struct flag_type portal_gatetype[] =
{
    { "arearandom",             GATETYPE_AREARANDOM,			    true	},
    { "arearecall",             GATETYPE_AREARECALL,			    true	},
    { "dungeon",                GATETYPE_DUNGEON,   			    true	},
    { "dungeonfloor",           GATETYPE_DUNGEONFLOOR,              true    },
    { "dungeonfloorspecial",    GATETYPE_DUNGEON_FLOOR_SPECIAL,     true    },
    { "dungeonrandom",          GATETYPE_DUNGEONRANDOM,			    true	},
    { "dungeonrandomfloor",     GATETYPE_DUNGEON_RANDOM_FLOOR,      true    },
    { "dungeonspecialroom",     GATETYPE_DUNGEON_SPECIAL,           true    },
    { "environment",            GATETYPE_ENVIRONMENT,			    true	},
    { "instance",               GATETYPE_INSTANCE,      		    true	},
    { "instancerandom",         GATETYPE_INSTANCERANDOM,		    true	},
    { "instancesectionmaze",    GATETYPE_BLUEPRINT_SECTION_MAZE,    true    },
    { "instancespecialroom",    GATETYPE_BLUEPRINT_SPECIAL,         true    },
    { "normal",                 GATETYPE_NORMAL,		    	    true	},
    { "random",                 GATETYPE_RANDOM,                    true    },
    { "regionrandom",           GATETYPE_REGIONRANDOM,              true    },
    { "regionrecall",           GATETYPE_REGIONRECALL,			    true	},
    { "sectionrandom",          GATETYPE_SECTIONRANDOM,			    true	},
    { "wilds",                  GATETYPE_WILDS,		    	        true	},
    { "wildsrandom",            GATETYPE_WILDSRANDOM,               true    },
    { NULL,             0,                              false   }
};

const struct flag_type area_region_flags[] =
{
    { "keep_live",      AREA_REGION_KEEP_LIVE,      true    },
    { "no_recall",      AREA_REGION_NO_RECALL,      true    },
    { NULL,             0,             false }
};

const struct flag_type death_release_modes[] =
{
    { "normal",                 DEATH_RELEASE_NORMAL,        true    },
    { "release_to_start",       DEATH_RELEASE_TO_START,      true    },
    { "release_to_floor",       DEATH_RELEASE_TO_FLOOR,      true    },
    { "release_to_checkpoint",  DEATH_RELEASE_TO_CHECKPOINT, true    },
    { "release_failure",        DEATH_RELEASE_FAILURE,       true    },
    { NULL,                     0,                           false   }
};


const struct flag_type script_spaces[] =
{
    { "mobile",     PRG_MPROG, true },
    { "object",     PRG_OPROG, true },
    { "room",       PRG_RPROG, true },
    { "token",      PRG_TPROG, true },
    { "area",       PRG_APROG, true },
    { "instance",   PRG_IPROG, true },
    { "dungeon",    PRG_DPROG, true },
    { NULL, 0, false }
};

const struct flag_type light_flags[] =
{
    { "active",                 LIGHT_IS_ACTIVE,            true },
    { "no_extinguish",          LIGHT_NO_EXTINGUISH,        true },
    { "remove_on_extinguish",   LIGHT_REMOVE_ON_EXTINGUISH, true },
    { NULL, 0, false }
};

const struct flag_type compartment_flags[] =
{
    { "allow_move",     COMPARTMENT_ALLOW_MOVE,     true },
    { "brief",          COMPARTMENT_BRIEF,          true },
    { "closeable",      COMPARTMENT_CLOSEABLE,      true },
    { "closed",         COMPARTMENT_CLOSED,         true },
    { "closelock",      COMPARTMENT_CLOSELOCK,      true },
    { "inside",         COMPARTMENT_INSIDE,         true },
    { "pushopen",       COMPARTMENT_PUSHOPEN,       true },
    { "transparent",    COMPARTMENT_TRANSPARENT,    true },
    { "underwater",     COMPARTMENT_UNDERWATER,     true },
    { NULL, 0, false }
};

const struct flag_type stock_types[] =
{
    { "crew",           STOCK_CREW,                 true },
    { "custom",         STOCK_CUSTOM,               true },
    { "guard",          STOCK_GUARD,                true },
    { "mount",          STOCK_MOUNT,                true },
    { "object",         STOCK_OBJECT,               true },
    { "pet",            STOCK_PET,                  true },
    { "ship",           STOCK_SHIP,                 true },
    { NULL, 0, false }
};

const struct flag_type prog_entity_flags[] =
{
    { "at",             PROG_AT,                    true },
    { "nodamage",       PROG_NODAMAGE,              true },
    { "nodestruct",     PROG_NODESTRUCT,            true },
    { "norawkill",      PROG_NORAWKILL,             true },
    { "silent",         PROG_SILENT,                true },
    { NULL, 0, false }
};

const struct flag_type book_flags[] =
{
    { "closeable",   BOOK_CLOSEABLE,     true },
    { "closed",      BOOK_CLOSED,        true },
    { "closelock",   BOOK_CLOSELOCK,     true },
    { "pushopen",    BOOK_PUSHOPEN,      true },
    { "no_rip",      BOOK_NO_RIP,        true },
    { "writable",    BOOK_WRITABLE,      true },
    { NULL, 0, false }
};

const struct flag_type fluid_con_flags[] =
{
    { "closeable",           FLUID_CON_CLOSEABLE,           true },
    { "closed",              FLUID_CON_CLOSED,              true },
    { "closelock",           FLUID_CON_CLOSELOCK,           true },
    { "destroy_on_consume",  FLUID_CON_DESTROY_ON_CONSUME,  true },
    { "pushopen",            FLUID_CON_PUSHOPEN,            true },
    { NULL, 0, false }
};

const struct flag_type scroll_flags[] =
{
    { "destroy_on_recite",  SCROLL_DESTROY_ON_RECITE,       true },
    { NULL, 0, false }
};


const struct flag_type skill_flags[] =
{
    { "can_brew",           SKILL_CAN_BREW,         true  },
    { "can_cast",           SKILL_CAN_CAST,         true  },
    { "can_imbue",          SKILL_CAN_IMBUE,        true  },
    { "can_ink",            SKILL_CAN_INK,          true  },
    { "can_scribe",         SKILL_CAN_SCRIBE,       true  },
    { "cross_class",        SKILL_CROSS_CLASS,      true },
    { "no_improve",         SKILL_NO_IMPROVE,       true },
    { "no_practice",        SKILL_NO_PRACTICE,      true },
    { "spell_pulse",        SKILL_SPELL_PULSE,      true  },
    { NULL,                 0,                      false }
};

const struct flag_type class_flags[] =
{
    { "combative",          CLASS_COMBATIVE,        true },
    { "no_level",           CLASS_NO_LEVEL,         true },
    { NULL,                 0,                      false }
};


const struct flag_type song_flags[] =
{
    { "instrument_only",    SONG_INSTRUMENT_ONLY,   true  },
    { "voice_only",         SONG_VOICE_ONLY,        true  },
    { NULL,                 0,                      false }
};

const struct flag_type reputation_flags[] =
{
    { "at_war",             REPUTATION_AT_WAR,      false   },      // Only toggled in game-+
    { "hidden",             REPUTATION_HIDDEN,      true    },
    { "peaceful",           REPUTATION_PEACEFUL,    true    },
    { NULL,                 0,                      false }
};

const struct flag_type reputation_rank_flags[] =
{
    { "hostile",            REPUTATION_RANK_HOSTILE,        true  },
    { "no_rank_up",         REPUTATION_RANK_NORANKUP,       true  },
    { "paragon",            REPUTATION_RANK_PARAGON,        true  },
    { "peaceful",           REPUTATION_RANK_PEACEFUL,       true  },
    { "reset_paragon",      REPUTATION_RANK_RESET_PARAGON,  true  },
    { NULL,                 0,                      false }
};

const struct flag_type practice_entry_flags[] =
{
    { "no_haggle",          PRACTICE_ENTRY_NO_HAGGLE,       true  },
    { NULL,                 0,                              false }
};

const struct flag_type material_classes[] =
{
    { "none",               MATERIAL_CLASS_NONE,            false },
    { "liquid",             MATERIAL_CLASS_LIQUID,          true  },
    { "wood",               MATERIAL_CLASS_WOOD,            true  },
    { "stone",              MATERIAL_CLASS_STONE,           true  },
    { "metal",              MATERIAL_CLASS_METAL,           true  },
    { "gem",                MATERIAL_CLASS_GEM,             true  },
    { "flesh",              MATERIAL_CLASS_FLESH,           true  },
    { "plant",              MATERIAL_CLASS_PLANT,           true  },
    { "cloth",              MATERIAL_CLASS_CLOTH,           true  },
    { "energy",             MATERIAL_CLASS_ENERGY,          true  },
    { "leather",            MATERIAL_CLASS_LEATHER,         true  },
    { "scale",              MATERIAL_CLASS_SCALE,           true  },
    { "gas",                MATERIAL_CLASS_GAS,             true  },
    { "organic",            MATERIAL_CLASS_ORGANIC,         true  },
    { "earth",              MATERIAL_CLASS_EARTH,           true  },
    { NULL,                 0,                              false }
};

const struct flag_type material_flags[] =
{
    { NULL,                 0,                              false }
};

const struct flag_type class_types[] =
{
    { "none",       CLASS_NONE,         false },
    { "cleric",     CLASS_CLERIC,       true },
    { "crafting",   CLASS_CRAFTING,     true },
    { "explorer",   CLASS_EXPLORER,     true },
    { "gathering",  CLASS_GATHERING,    true },
    { "mage",       CLASS_MAGE,         true },
    { "thief",      CLASS_THIEF,        true },
    { "warrior",    CLASS_WARRIOR,      true },
    { NULL,         0,                  false }
};

const struct flag_type stat_types[] =
{
    { "none",           STAT_NONE,          true },
    { "constitution",   STAT_CON,           true },
    { "dexterity",      STAT_DEX,           true },
    { "intelligence",   STAT_INT,           true },
    { "strength",       STAT_STR,           true },
    { "wisdom",         STAT_WIS,           true },
    { NULL,             0,                  false }
};

const struct flag_type vital_types[] =
{
    { "hp",             0,                  true },
    { "mana",           1,                  true },
    { "move",           2,                  true },
    { NULL,             0,                  false }
};

const struct flag_type missionary_types[] =
{
    {"mob",     MISSIONARY_MOB,     true},
    {"obj",     MISSIONARY_OBJ,     true},
    {"room",    MISSIONARY_ROOM,    true},
    {NULL,      0,                  false }
};

const struct flag_type armour_types[] =
{
    {"none",    ARMOR_TYPE_NONE,    false },
    {"cloth",   ARMOR_TYPE_CLOTH,   true },
    {"leather", ARMOR_TYPE_LEATHER, true },
    {"mail",    ARMOR_TYPE_MAIL,    true },
    {"plate",   ARMOR_TYPE_PLATE,   true },
    {NULL,      0,                  false }
};

const struct flag_type armour_protection_types[] =
{
    {"bash",    ARMOR_BASH,         true },
    {"pierce",  ARMOR_PIERCE,       true },
    {"slash",   ARMOR_SLASH,        true },
    {"magic",   ARMOR_MAGIC,        true },
    {NULL,      0,                  false }
};


const struct flag_type adornment_types[] =
{
    {"none",        ADORNMENT_NONE,         false},
    {"embroidery",  ADORNMENT_EMBROIDERY,   true},
    {"rune",        ADORNMENT_RUNE,         true},
    {"gem",         ADORNMENT_GEM,          true},
    {NULL,          0,                      false}
};

const struct flag_type staff_ranks[] =
{
    {"player",      STAFF_PLAYER,           false},
    {"gimp",        STAFF_GIMP,             false},
    {"immortal",    STAFF_IMMORTAL,         true},
    {"ascendant",   STAFF_ASCENDANT,        true},
    {"supremacy",   STAFF_SUPREMACY,        true},
    {"creator",     STAFF_CREATOR,          true},
    {"implementor", STAFF_IMPLEMENTOR,      true},
    {NULL,          0,                      false}
};

const struct flag_type sector_flags[] =
{
	{ "aerial", 		SECTOR_AERIAL,  	true },
	{ "briars",	    	SECTOR_BRIARS,  	true },
    { "city_lights",    SECTOR_CITY_LIGHTS, true },
	{ "crumbles",		SECTOR_CRUMBLES,	true },
	{ "deep_water",		SECTOR_DEEP_WATER,	true },
    { "drain_mana",     SECTOR_DRAIN_MANA,  true },
	{ "flame",  		SECTOR_FLAME,   	true },
	{ "frozen", 		SECTOR_FROZEN,  	true },
	{ "hard_magic",		SECTOR_HARD_MAGIC,	true },
	{ "indoors",		SECTOR_INDOORS, 	true },
    { "melts",          SECTOR_MELTS,       true },
    { "nature",         SECTOR_NATURE,      true },
    { "no_fade",        SECTOR_NO_FADE,     true },
    { "no_gate",        SECTOR_NO_GATE,     true },
    { "no_gohall",      SECTOR_NO_GOHALL,   true },
    { "no_hide_obj",    SECTOR_NO_HIDE_OBJ, true },
	{ "no_magic",		SECTOR_NO_MAGIC,	true },
	{ "no_soil",		SECTOR_NO_SOIL, 	true },
    { "sleep_drain",    SECTOR_SLEEP_DRAIN, true },
	{ "slow_magic",		SECTOR_SLOW_MAGIC,	true },
	{ "toxic",		    SECTOR_TOXIC,   	true },
	{ "underwater",		SECTOR_UNDERWATER,	true },
    {NULL,              0,                  false}
};

const struct flag_type sector_classes[] =
{
	{ "none",          	SECTCLASS_NONE,         true },
	{ "abyss",          SECTCLASS_ABYSS,        true },
	{ "air",            SECTCLASS_AIR,          true },
	{ "arctic",         SECTCLASS_ARCTIC,       true },
	{ "city",          	SECTCLASS_CITY,         true },
	{ "desert",         SECTCLASS_DESERT,       true },
	{ "dungeon",        SECTCLASS_DUNGEON,      true },
	{ "forest",         SECTCLASS_FOREST,       true },
	{ "hazardous",      SECTCLASS_HAZARDOUS,    true },
	{ "hills",          SECTCLASS_HILLS,        true },
	{ "jungle",         SECTCLASS_JUNGLE,       true },
	{ "mountains",      SECTCLASS_MOUNTAINS,    true },
    { "nether",         SECTCLASS_NETHER,       true },
	{ "plains",         SECTCLASS_PLAINS,       true },
	{ "roads",          SECTCLASS_ROADS,        true },
	{ "subarctic",      SECTCLASS_SUBARCTIC,    true },
	{ "swamp",          SECTCLASS_SWAMP,        true },
	{ "underground",    SECTCLASS_UNDERGROUND,  true },
	{ "vulcan",         SECTCLASS_VULCAN,       true },
	{ "water",          SECTCLASS_WATER,        true },
    {NULL,              0,                      false}
};

const struct flag_type cart_flags[] =
{
    {"mount_only",          CART_MOUNT_ONLY,        true},
    {"team_animal_only",    CART_TEAM_ANIMAL_ONLY,  true},
    {NULL,                  0,                      false}
};

const struct flag_type skill_sources[] =
{
    {"normal",              SKILLSRC_NORMAL,        true },
    {"script",              SKILLSRC_SCRIPT,        true },
    {"script_perm",         SKILLSRC_SCRIPT_PERM,   true },
    {"affect",              SKILLSRC_AFFECT,        true },
    {NULL,                  0,                      false}
};

const struct flag_type church_sizes[] =
{
    {"band",            CHURCH_SIZE_BAND,           true },
    {"cult",            CHURCH_SIZE_CULT,           true },
    {"order",           CHURCH_SIZE_ORDER,          true },
    {"church",          CHURCH_SIZE_CHURCH,         true },
    {NULL,              0,                          false}
};


const struct flag_type acct_flags[] =
{
	{	"create_staff",			ACCT_CAN_CREATE_STAFF,				true	},
    {   "link_chars",           ACCT_CAN_LINK,  true },
    {   "unlink_chars",         ACCT_CAN_UNLINK, true },
	{	NULL,	0,	0	}
};

const struct flag_type church_permission_flags[] =
{
    { "gohall",          CHURCH_PERM_GOHALL,          true, "Member can use 'church gohall'." },
    { "withdraw",        CHURCH_PERM_WITHDRAW,        true,  "Member can withdraw dp/pneuma/gold from church balance."},
    { "info",            CHURCH_PERM_INFO,            true,  "Member can see church info."},
    { "motd",            CHURCH_PERM_MOTD,            true,  "Member can edit the church MOTD." },
    { "rules",           CHURCH_PERM_RULES,           true,  "Member can edit the church rules." },
    { "remove",          CHURCH_PERM_REMOVE,          true, "Member can remove other members." },
    { "manage_storage",         CHURCH_PERM_STORAGE,         true, "Member has full get/put access to church storage." },
    { "get_storage",     CHURCH_PERM_GET_STORAGE,     true, "Member can get items from church storage, but cannot add anything. Why use this?" },
    { "put_storage",     CHURCH_PERM_PUT_STORAGE,     true, "Member can put items into storage but not remove them. Suckers!" },
    { "treasure",        CHURCH_PERM_TREASURE,        true, "Member has access to the treasure room feature." },
    { "gohall_crosszone",CHURCH_PERM_GH_CROSS,true, "Member can gohall from other continents. Requires church setting to be on." },
    { "ranks"           , CHURCH_PERM_RANKS,           true, "Member can manage ranks." },
    { "permissions",  CHURCH_PERM_PERMS,   true, "Member can manage permissions." },
    { "talk",            CHURCH_PERM_TALK, true, "Member is allowed to use church talk."},
    { "finances",      CHURCH_PERM_FINANCES, true, "Member is allowed access to church finances (implies withdraw)."},
    { "upgrade",       CHURCH_PERM_UPGRADE, true, "Member is allowed to upgrade the church if resources are available." },
    { "admin",         CHURCH_PERM_MANAGE, true, "Member is allowed to manage the church." },
    { "viewlog",       CHURCH_PERM_VIEWLOG, true, "Member is allowed to view the church log." },
    { "balance",       CHURCH_PERM_BALANCE, true, "Member is allowed to balance the church." },
    { "treasure_all",  CHURCH_PERM_TREASURE_ALL, true, "Member can access all treasure rooms." },
    { "treasure_manage", CHURCH_PERM_TREASURE_MANAGE, true, "Member can manage treasure rooms." },
    { "add",            CHURCH_PERM_ADD, true, "Member can add people to the church." },
    { "members",       CHURCH_PERM_MEMBERS, true, "Member can manage members' ranks."},
    { "editlog",        CHURCH_PERM_EDITLOG, true, "Member can add entries to the church log, and edit their own." },
    { NULL,              0,                           false }
};

const struct flag_type rank_type_flags[] =
{
    { "member",       RANK_TYPE_MEMBER,     true },
    { "officer",      RANK_TYPE_OFFICER,    true },
    { "leader",       RANK_TYPE_LEADER,     true },
    { NULL,           0,                    false }
};

const struct flag_type token_types[] =
{
	{ "affect",		TOKEN_AFFECT,	true },
	{ "general",	TOKEN_GENERAL,	true },
	{ "quest",		TOKEN_QUEST,	true },
	{ "skill",		TOKEN_SKILL,	true },
	{ "spell",		TOKEN_SPELL,	true },
	{ "song",		TOKEN_SONG,		true },
	{ NULL,			0,				false }
};
