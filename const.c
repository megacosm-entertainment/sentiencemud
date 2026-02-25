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
#include <stdio.h>
#include <time.h>
#include "merc.h"
#include "magic.h"
#include "interp.h"

/*
 * VIZZWILDS - Dont need this anymore.
 * map area entrance table
 *
const struct map_exit_type      map_exit_table[] =
{
    {   WILDERNESS_MAIN, 429, 448, 11168 },   PLITHeast
    {   WILDERNESS_MAIN, 426, 451, 10763 },  PLITHsouth
    {   WILDERNESS_MAIN, 219, 419, 4000 },   REZA
    {   WILDERNESS_MAIN, 351, 410, 8000 },   Aethilforge
    {   WILDERNESS_MAIN, 305, 463, 266328 },   Pyramid
    {   WILDERNESS_MAIN, 299, 500, 6499 },   Wyvern Keep
    {   WILDERNESS_MAIN, 432, 498, 201 },   Goblin Fort
    {   WILDERNESS_MAIN, 446, 440, 3945 },   Kalandor
    {   WILDERNESS_MAIN, 370, 383, 1600 },   Dungeon Mystica
    {   WILDERNESS_MAIN, 481, 387, 6951 },   Olaria
    {   WILDERNESS_MAIN, 238, 649, 152504 },   Mordrakes Tower
    {   WILDERNESS_MAIN, 1366, 891, 265282 },  Achaues West
    {   WILDERNESS_MAIN, 1372, 885, 265200 },  Achaeus North
    {   WILDERNESS_MAIN, 1378, 891, 265229 },  Achaues East
    {   WILDERNESS_MAIN, 1372, 896, 265501 },  Achaeus South
    {   WILDERNESS_MAIN, 1398, 833, 32 },  Dwa Vygwa
    {   WILDERNESS_MAIN, 1306, 810, 151500 },  Lartin Castle
    {   WILDERNESS_MAIN, 1283, 882, 7317 },  Lestat Mountain
    {   WILDERNESS_MAIN, 1218, 913, 1400 },  Road House
    {   WILDERNESS_MAIN, 1305, 1161, 261201 },  Temple
    {   WILDERNESS_MAIN, 1322, 229, 264201 },  Undersea

    {   WILDERNESS_MAIN, 1183, 810, 264102 }, Bone Mountain
    {   WILDERNESS_MAIN, 1173, 881, 260201 }, Atmic Caverns

    {   0,				     0, 0, 0  	}
};
*/

const struct item_type token_table [] =
{
    {	TOKEN_GENERAL,		"general"		},
    {	TOKEN_QUEST,		"quest"			},
    {	TOKEN_AFFECT,		"affect"		},
    {	TOKEN_SKILL,		"skill"			},
    {	TOKEN_SPELL,		"spell"			},
    {	TOKEN_SONG,			"song"			},
    {	0,			NULL			}
};


const struct item_type          auto_war_table [] =
{
    {   AUTO_WAR_GENOCIDE, 	"genocide" 		},
    {   AUTO_WAR_JIHAD,    	"jihad"    		},
    {   AUTO_WAR_FREE_FOR_ALL,  "battle royale"   	},
    {   0,			NULL 		      	}
};


const struct item_type          boat_table [] =
{
    {   SHIP_SAILING_BOAT, 	"sailing boat" 	},
//    {   SHIP_CARGO_SHIP,   	"cargo ship"   	},
//    {   SHIP_ADVENTURER_SHIP, 	"adventurer"	},
//    {   SHIP_GALLEON_SHIP,    	"galleon"   	},
//    {   SHIP_FRIGATE_SHIP,    	"frigate"   	},
//    {   SHIP_WAR_GALLEON_SHIP, 	"war galleon"	},
    {   SHIP_AIR_SHIP,		"airship"   	},
    {   0,		       	NULL       	}
};

const struct item_type          npc_boat_table [] =
{
    {  NPC_SHIP_COAST_GUARD,  	"military" 	},
    {  NPC_SHIP_PIRATE,         "pirate" 	},
    {  NPC_SHIP_BOUNTY_HUNTER,  "bounty hunter"	},
    {  NPC_SHIP_ADVENTURER,     "adventurer"	},
    {  NPC_SHIP_TRADER,         "trader"	},
    {  NPC_SHIP_AIR_SHIP,       "airship"	},
    {  0,		       	NULL      	}
};


const struct item_type          npc_sub_type_boat_table [] =
{
    {  NPC_SHIP_SUB_TYPE_NONE,                  "None" },
    {  NPC_SHIP_SUB_TYPE_COAST_GUARD_ATHEMIA, 	"Coast Guard - Athemia" },
        {  NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA,   "Coast Guard - Seralia" },
    {  NPC_SHIP_SUB_TYPE_LIGHT_TRADER,   "Light Trader (random light cargo)" },
    {  NPC_SHIP_SUB_TYPE_MEDIUM_TRADER,   "Medium Trader (more cargo)" },
    {  NPC_SHIP_SUB_TYPE_TREASURE_BOAT,   "Treasure Boat" },
    {  0,		       			NULL       		}
};


const struct item_type          ship_state_table []           =
{
  { NPC_SHIP_STATE_STOPPED, "stopped" },
  { NPC_SHIP_STATE_SAILING, "sailing" },
  { NPC_SHIP_STATE_ATTACKING, "attacking" },
  { NPC_SHIP_STATE_BOARDING, "boarding" },
  { NPC_SHIP_STATE_FLEEING, "fleeing" },
  { NPC_SHIP_STATE_CHASING, "chasing" }
};

/* NPC ship rating*/
const struct rep_type	    rating_table [] =
{
    {   NPC_SHIP_RATING_UNKNOWN, 	"Unknown" 	       , 0	},
    {   NPC_SHIP_RATING_RECOGNIZED, "Recognized" 		 , 25 },
    {   NPC_SHIP_RATING_WELLKNOWN, 	"Wellknown" 		 , 250},
    {   NPC_SHIP_RATING_FAMOUS, 	  "Famous" 		     , 500},
    {   NPC_SHIP_RATING_NOTORIOUS, 	"Notorious" 		 , 1000},
    {   NPC_SHIP_RATING_INFAMOUS, 	"Infamous" 		   , 1500},
    {   NPC_SHIP_RATING_WANTED, 	"{R!!!WANTED!!!{x" , 50000},
    {   0,				NULL,  60000	}
};


/* NPC ship rank*/
const struct rank_type	    rank_table [] =
{
    {   NPC_SHIP_RANK_NONE, 	    	"None" 		    ,0},
    {   NPC_SHIP_RANK_ENSIGN,       "Ensign" 	    ,10},
    {   NPC_SHIP_RANK_LIEUTENANT,   "Lieutenant"  ,100	},
    {   NPC_SHIP_RANK_COMMANDER,    "Commander"   ,500	},
    {   NPC_SHIP_RANK_CAPTAIN, 	    "Captain" 	  ,1000},
    {   NPC_SHIP_RANK_COMMODORE, 	  "Commodore"   ,5000	},
    {   NPC_SHIP_RANK_VICE_ADMIRAL, "Vice Admiral", 15000 	},
    {   NPC_SHIP_RANK_ADMIRAL, 		  "Admiral" 	  , 30000},
    {   NPC_SHIP_RANK_PIRATE, 	  	"Pirate"      , 10000000	},
    {   0,				NULL		}
};

/*
const   struct  crew_type       crew_table [] =
{
    { 	get_reserved_vnum("mob_sailor_diseased"), 	"Diseased Sailor", 	100 	},
    { 	get_reserved_vnum("mob_sailor_dirty"),    	"Dirty Sailor",    	200 	},
    { 	get_reserved_vnum("mob_sailor_burly"),    	"Burly Sailor",    	400 	},
    { 	get_reserved_vnum("mob_sailor_trained"),  	"Trained Sailor",  	700 	},
    { 	get_reserved_vnum("mob_sailor_mercenary"),	"Mercenary Pirate",	800 	},
    { 	get_reserved_vnum("mob_sailor_elite"),    	"Elite Sailor",    	1000	},
    { 	-1,                       	NULL,              	0	}
};
*/

const struct item_type		item_table	[]	=
{
    {	ITEM_LIGHT,		"light"				},
    {	ITEM_SCROLL,		"scroll"			},
    {	ITEM_WAND,		"wand"				},
    {   ITEM_STAFF,		"staff"				},
    {   ITEM_WEAPON,		"weapon"			},
    {   ITEM_TREASURE,		"treasure"			},
    {   ITEM_ARMOUR,		"armour"				},
    {	ITEM_POTION,		"potion"			},
    {	ITEM_CLOTHING,		"clothing"			},
    {   ITEM_FURNITURE,		"furniture"			},
    {	ITEM_TRASH,		"trash"				},
    {	ITEM_CONTAINER,		"container"			},
    {	ITEM_DRINK_CON, 	"drink"				},
    {	ITEM_KEY,		"key"				},
    {	ITEM_FOOD,		"food"				},
    {	ITEM_MONEY,		"money"				},
    {	ITEM_BOAT,		"boat"				},
    {	ITEM_CORPSE_NPC,	"npc_corpse"			},
    {	ITEM_CORPSE_PC,		"pc_corpse"			},
    {   ITEM_FOUNTAIN,		"fountain"			},
    {	ITEM_PILL,		"pill"				},
    {	ITEM_PROTECT,		"protect"			},
    {	ITEM_MAP,		"map"				},
    {	ITEM_PORTAL,		"portal"			},
    {	ITEM_CATALYST,		"catalyst"			},
    {	ITEM_ROOM_KEY,		"room_key"			},
    {	ITEM_GEM,		"gem"				},
    {	ITEM_JEWELRY,		"jewelry"			},
    {   ITEM_JUKEBOX,		"jukebox"			},
    {   ITEM_ARTIFACT,		"artifact"			},
    {   ITEM_SHARECERT, 	"shares"			},
    {   ITEM_ROOM_FLAME, 	"room_flame_object"		},
    {   ITEM_INSTRUMENT, 	"instrument"			},
    {   ITEM_SEED, 		"seed"				},
    {   ITEM_CART, 		"cart"				},
    {   ITEM_SHIP, 		"ship"				},
    {   ITEM_ROOM_DARKNESS, 	"room_darkness_object"		},
    {   ITEM_RANGED_WEAPON, 	"ranged_weapon"			},
    {   ITEM_SEXTANT, 		"sextant"			},
    {   ITEM_WEAPON_CONTAINER,	"weapon_container"		},
    {	ITEM_ROOM_ROOMSHIELD,	"room_roomshield_object"	},
    {	ITEM_BOOK,		"book"				},
    {   ITEM_STINKING_CLOUD, 	"stinking_cloud"		},
    {	ITEM_SMOKE_BOMB,	"smoke_bomb"			},
    {	ITEM_SPELL_TRAP,	"spell_trap"			},
    {	ITEM_WITHERING_CLOUD,	"withering_cloud"		},
    {   ITEM_BANK,		"bank" 				},
    {   ITEM_KEYRING,		"keyring" 			},
    {   ITEM_TRADE_TYPE,	"trade_type" 			},
    {	ITEM_ICE_STORM,		"ice_storm" 			},
    {	ITEM_FLOWER,		"flower"			},
    {   ITEM_HERB,		"herb",				},
    {   ITEM_EMPTY_VIAL,	"empty_vial"			},
    {   ITEM_BLANK_SCROLL,	"blank_scroll" 			},
    {   ITEM_MIST,		"mist"				},
    {	ITEM_SHRINE,		"shrine"			},
    {   ITEM_WHISTLE,   "whistle"    },
    {   ITEM_SHOVEL,   "shovel"    },
    {   ITEM_TATTOO,   "tattoo"    },
    {   ITEM_INK,   "ink"    },
    {	ITEM_PART,		"part"	},
    {	ITEM_TELESCOPE,		"telescope"	},
    {	ITEM_COMPASS,		"compass"	},
    {	ITEM_WHETSTONE,		"whetstone"	},
    {	ITEM_CHISEL,		"chisel" },
    {	ITEM_PICK,			"pick"	},
    {	ITEM_TINDERBOX,		"tinderbox"},
    {	ITEM_DRYING_CLOTH,	"drying_cloth"},
    {	ITEM_NEEDLE,		"needle"},
    {	ITEM_BODY_PART,		"body_part"},

    {   0,			NULL				}
};



/* List of create food items*/
const   long    food_table[] =
{
    100066,
    100067,
    100068,
    100069,
    100070,
    100071,
    100072,
    100073,
    100074,
    100075,
    100076,
    0
};


const   struct  tunneler_place_type  tunneler_place_table[] =
{
/*  name		price	to_vnum*/
    { "Olaria", 	1000, 	6975 	},
    { "Reza", 		5000, 	4000 	},
    { "Goblin Fort", 	4500, 	201 	},
    { NULL, 		0,	0	}
};


const	struct	weapon_type	weapon_table	[]	=
{
/*  name		vnum				type			gsn*/
    { "sword",		0,				WEAPON_SWORD		},
    { "mace",		0,				WEAPON_MACE 		},
    { "dagger",		0,				WEAPON_DAGGER		},
    { "axe",	    	0,				WEAPON_AXE		},
    { "staff",	    	0,				WEAPON_SPEAR		},
    { "flail",	    	0,				WEAPON_FLAIL		},
    { "whip",	    	0,				WEAPON_WHIP		},
    { "polearm",	0,				WEAPON_POLEARM		},
    { "quarterstaff",	0,       			WEAPON_QUARTERSTAFF 	},
    { "stake",		0,				WEAPON_STAKE	 	},
    { "arrow",		0,	                	WEAPON_ARROW			},
    { "bolt",		0,              		WEAPON_BOLT			},
    { "throwable",	0,             			WEAPON_THROWABLE			},
    { "exotic",		0,				WEAPON_EXOTIC		},
    { "dart",		0,	                	WEAPON_DART			},	/* @@@NIB : 20070126*/
    { "harpoon",	0,              		WEAPON_HARPOON		},	/* @@@NIB : 20070126 : instead of the harpoon skill, since it's basically a short spear*/
    { NULL,		0,				0			}
};


const	struct	weapon_type	ranged_weapon_table	[]	=
{
    { "crossbow",	0,	RANGED_WEAPON_CROSSBOW	},
    { "bow",		0,	RANGED_WEAPON_BOW	},
    { "exotic",		0,	RANGED_WEAPON_EXOTIC	},	/* @@@NIB : 20070126*/
    { "blowgun",	0,	RANGED_WEAPON_BLOWGUN	},	/* @@@NIB : 20070126*/
    { "harpoon",	0,	RANGED_WEAPON_HARPOON	},	/* @@@NIB : 20070126*/
    { NULL,		0,	0		}
};

const	int	size_weight[] = { 0, 10, 50, 120, 450, 4500 };


const	struct	trade_type	trade_table	[]	=
{
/*  type			name			live?	minprice	maxprice*/
    { TRADE_NONE,     	  	"None", 		false, 	0, 	0 	},
    { TRADE_WEAPONS, 	  	"Weapons", 		false, 	4000, 	4200 	},
    { TRADE_FARMING_EQ,    	"Farming Equipment", 	false, 	9700, 	10000 	},
    { TRADE_PRECIOUS_GEMS, 	"Precious Gems", 	false, 	98000, 	98100 	},
    { TRADE_IRON_ORE,	  	"Iron Ore", 		false, 	2500, 	2700 	},
    { TRADE_WOOD,	  	"Wood", 		false, 	1000, 	1100 	},
    { TRADE_FARMING_FOOD,  	"Farming Food", 	false,	2000, 	3000 	},
    { TRADE_SLAVES,   	  	"Slaves", 		true, 	2500000, 	2550000 	},
    { TRADE_PIGS,          	"Pigs", 		true, 	10000, 	10400 	},
    { TRADE_PAPER, 	 	"Paper", 		false, 	5000, 	5300 	},
    { TRADE_GOLD, 	  	"Gold", 		false, 	500000, 	502000 	},
    { TRADE_SILVER,  	  	"Silver", 		false, 	200000, 	205000 	},
    { TRADE_SPICES,  	  	"Spices", 		false, 	15000, 	15700 	},
    { TRADE_CANNONS, 	  	"Cannons", 		false, 	50000, 	52000 	},
    { TRADE_EXOTIC_SEEDS,  	"Exotic Seeds", 	false, 	5000, 	10000 	},
    { TRADE_SPECIAL,  	  	"Special Cargo", 	false, 	5000, 	10000 	},
    { TRADE_REAGENTS,      	"Alchemy Reagents", 	false, 	2000, 	2350 	},
    { TRADE_PASSENGER,     	"Passenger", 		false, 	5000, 	10000 	},
    { TRADE_CONTRABAND, 	"Contraband", 		false, 	1500000, 	1515000 	},
    { TRADE_LAST, 	  	"", 			false, 	0,  	0 	},
    { -1, 			"", 			false, 	0,  	0 	}
};


/* Rooms in the Plith docks*/
const   long   			plith_docks_table[] 		=
{
    5701876, 5703414, 5704952, 5706490, 5708028, 5709566, 5711104, 5712642, 5714180, 5715718, 5717256, -1
};

/* Treasure vnums*/
const long          treasure_table[] =
{
    // TODO: TEMPORARY
    265555
};
const   struct wiznet_type      wiznet_table    [] =
{
   {    "on",           WIZ_ON,         STAFF_IMMORTAL },
   {    "prefix",	WIZ_PREFIX,	STAFF_IMMORTAL },
   {    "ticks",        WIZ_TICKS,      STAFF_IMMORTAL },
   {    "logins",       WIZ_LOGINS,     STAFF_IMMORTAL },
   {    "links",        WIZ_LINKS,      STAFF_IMPLEMENTOR },
   {	"newbies",	WIZ_NEWBIE,	STAFF_IMMORTAL },
   {	"spam",		WIZ_SPAM,	STAFF_ASCENDANT },
   {    "deaths",       WIZ_DEATHS,     STAFF_IMMORTAL },
   {    "resets",       WIZ_RESETS,     STAFF_ASCENDANT },
   {    "mobdeaths",    WIZ_MOBDEATHS,  STAFF_ASCENDANT },
   {	"penalties",	WIZ_PENALTIES,	STAFF_ASCENDANT },
   {	"levels",	WIZ_LEVELS,	STAFF_IMMORTAL },
   {	"load",		WIZ_LOAD,	STAFF_SUPREMACY },
   {	"restore",	WIZ_RESTORE,	STAFF_SUPREMACY },
   {	"switches",	WIZ_SWITCHES,	STAFF_SUPREMACY },
   {	"secure",	WIZ_SECURE,	STAFF_IMPLEMENTOR },
   {	"memcheck",	WIZ_MEMCHECK,	STAFF_CREATOR },
   {    "immlog",	WIZ_IMMLOG,	STAFF_IMPLEMENTOR },
   {    "testing",	WIZ_TESTING,	STAFF_IMPLEMENTOR },
   {	"building",	WIZ_BUILDING,	STAFF_IMMORTAL },
   {	"scripts",	WIZ_SCRIPTS,	STAFF_CREATOR },
   {	"ships",	WIZ_SHIPS,	STAFF_IMMORTAL },
   {	"bugs",	WIZ_BUGS,	STAFF_IMPLEMENTOR },
   {	"helps",	WIZ_HELPS,	STAFF_IMMORTAL },
   {    "commands", WIZ_VERBS, STAFF_IMMORTAL},
   {	NULL,		0,		0  }
};

const  struct player_setting_type    pc_set_table[] =
{
    /*  name            plr bit          plr2 bit, 	comm bit,    	backwards, min level to toggle, default setting */
    {	"afk",		0,		 0,		COMM_AFK,	false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"autoeq",	PLR_AUTOEQ,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autoexit",	PLR_AUTOEXIT,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autogold",	PLR_AUTOGOLD,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autoloot",	PLR_AUTOLOOT,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autosac",	PLR_AUTOSAC,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autosetname",	PLR_AUTOSETNAME, 0,		0,		false,	STAFF_IMMORTAL,	SETTING_ON	},
    {	"autosplit",	PLR_AUTOSPLIT,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"autosurvey",	0,		 PLR_AUTOSURVEY,0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"battlespam",	0,		 0,		COMM_NOBATTLESPAM,true,	STAFF_PLAYER,	SETTING_OFF	},
    {	"brief",	0,		 0,		COMM_BRIEF,	false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"colour",	PLR_COLOUR,	 0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"compact",	0,		 0,		COMM_COMPACT,	false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"formstate",	0,		 0,		COMM_SHOW_FORM_STATE,false,STAFF_PLAYER,	SETTING_ON	},
    {   "holyaura",	0,		PLR_HOLYAURA,	 0,		false,	STAFF_IMMORTAL,	SETTING_OFF	},
    {   "holylight",	PLR_HOLYLIGHT,	 0,		0,		false,	STAFF_IMMORTAL,	SETTING_OFF	},
    {   "holywarp",	0,		PLR_HOLYWARP,	 0,		false,	STAFF_IMMORTAL,	SETTING_OFF	},
    {	"map",		0,		 0,		COMM_NOMAP,	true,	STAFF_PLAYER,	SETTING_OFF	},
    {	"mxp",		0,		0,		COMM_MXP,	false,	STAFF_PLAYER,	SETTING_ON	},
    {	"nochallenge",	PLR_NO_CHALLENGE,0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"nofollow",	PLR_NOFOLLOW,	 0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {   "nolore",	0,		PLR_NOLORE,	 0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {   "noreckoning",	0,		PLR_NORECKONING,	 0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"noresurrect",	PLR_NO_RESURRECT,0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"nosummon",	PLR_NOSUMMON,	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {   "nowake",	0,		 PLR_NO_WAKE,	0,		false,  STAFF_PLAYER,      SETTING_OFF	},
    {   "notify",	0,	 	 0,		COMM_NOTIFY,	false,	STAFF_PLAYER,	SETTING_ON	},
    {   "eventnotify",0,	 	 0,		0,		false,	STAFF_PLAYER,	SETTING_ON	},
    {	"prompt",	0,		 0,		COMM_PROMPT,	false,	STAFF_PLAYER,	SETTING_ON	},
    {	"pursuit",	PLR_PURSUIT,	 0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"quiet",	0,		 0,		COMM_QUIET,	false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"sacrifice_all",	0,	 PLR_SACRIFICE_ALL,0,		false,  STAFF_PLAYER,      SETTING_OFF	},
    {	"showdamage",	PLR_SHOWDAMAGE,	 0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	},
    {	"mobile",	PLR_MOBILE,	0,	0, false,	STAFF_PLAYER, SETTING_OFF },
    {	"favskills",	0,	PLR_FAVSKILLS,	0, false,	STAFF_PLAYER, SETTING_OFF },
    {	"olctabs",	0,	0,	0,	false,	STAFF_IMMORTAL,	SETTING_ON },
    {	"compass",	0,	PLR_COMPASS,	0,	false,	STAFF_PLAYER,	SETTING_ON},
    {	"autocatalyst", 0, PLR_AUTOCAT,	0,	false, STAFF_PLAYER,	SETTING_ON},
    {	"autoafk",	0,	PLR_AUTOAFK,	0,	false, STAFF_PLAYER,	SETTING_OFF},
    {	"hideidle",	0,	PLR_HIDE_IDLE,	0,	false, STAFF_PLAYER,	SETTING_OFF},
    {	"timestamps",	0,	PLR_SHOW_TIMESTAMPS,0,	false, STAFF_PLAYER,	SETTING_OFF},
/*    {	"building",     PLR_BUILDING,	 0,		0,		false,  STAFF_IMMORTAL,	SETTING_OFF	},*/
    {	NULL,		0,		 0,		0,		false,	STAFF_PLAYER,	SETTING_OFF	}
};


const 	struct attack_type	attack_table	[MAX_DAMAGE_MESSAGE]	=
{
/*	name		noun			damage type*/
    { 	"none",		"hit",			-1		},
    {	"slice",	"slice",		DAM_SLASH	},
    {   "stab",		"stab",			DAM_PIERCE	},
    {	"slash",	"slash",		DAM_SLASH	},
    {	"whip",		"whip",			DAM_SLASH	},
    {   "claw",		"claw",			DAM_SLASH	},
    {	"blast",	"blast",		DAM_BASH	},
    {   "pound",	"pound",		DAM_BASH	},
    {	"crush",	"crush",		DAM_BASH	},
    {   "grep",		"grep",			DAM_SLASH	},
    {	"bite",		"bite",			DAM_PIERCE	},
    {   "pierce",	"pierce",		DAM_PIERCE	},
    {   "suction",	"suction",		DAM_BASH	},
    {	"beating",	"beating",		DAM_BASH	},
    {   "digestion",	"digestion",		DAM_ACID	},
    {	"charge",	"charge",		DAM_BASH	},
    { 	"slap",		"slap",			DAM_BASH	},
    {	"punch",	"punch",		DAM_BASH	},
    {	"wrath",	"wrath",		DAM_ENERGY	},
    {	"magic",	"magic",		DAM_MAGIC	},
    {   "divine",	"divine power",		DAM_HOLY	},
    {   "holy",  	"holy fire",		DAM_HOLY	},
    {	"cleave",	"cleave",		DAM_SLASH	},
    {	"scratch",	"scratch",		DAM_PIERCE	},
    {   "peck",		"peck",			DAM_PIERCE	},
    {   "peckb",	"peck",			DAM_BASH	},
    {   "chop",		"chop",			DAM_SLASH	},
    {   "sting",	"sting",		DAM_PIERCE	},
    {   "smash",	"smash",		DAM_BASH	},
    {   "shbite",	"shocking bite",	DAM_LIGHTNING	},
    {	"flbite",	"flaming bite", 	DAM_FIRE	},
    {	"frbite",	"freezing bite", 	DAM_COLD	},
    {	"acbite",	"acidic bite", 		DAM_ACID	},
    {	"chomp",	"chomp",		DAM_PIERCE	},
    {  	"drain",	"life drain",		DAM_NEGATIVE	},
    {  	"decay",	"decaying touch",	DAM_NEGATIVE	},
    {   "thrust",	"thrust",		DAM_PIERCE	},
    {   "slime",	"slime",		DAM_ACID	},
    {	"shock",	"shock",		DAM_LIGHTNING	},
    {   "thwack",	"thwack",		DAM_BASH	},
    {   "flame",	"flame",		DAM_FIRE	},
    {   "chill",	"chill",		DAM_COLD	},
    {   "vorpal",	"slash",		DAM_VORPAL	},
    {   "purify",	"purifying light",	DAM_HOLY 	},
    {   "crblow",       "crippling blow", 	DAM_NEGATIVE  	},
/* @@@NIB : 20070123*/
    {	"acrid",	"acrid spray", 		DAM_ACID	},
    {	"blight",	"blighting touch",	DAM_DISEASE	},
    {	"boiling",	"boiling spray",	DAM_WATER	},	/* Fire Too?*/
    {	"corrode",	"corrosion", 		DAM_ACID	},
    {	"discharge",	"discharge",		DAM_LIGHTNING	},
    {	"flog",		"flog",			DAM_SLASH	},
    {	"fumes",	"caustic fumes",	DAM_ACID	},
    {	"lacerate",	"laceration",		DAM_SLASH	},
    {	"lash",		"lash",			DAM_SLASH	},
    {	"lbolt",	"lightning bolt",	DAM_LIGHTNING	},
    {	"pain",		"pain",			DAM_MENTAL	},
    {	"pestilence",	"pestilence",		DAM_DISEASE	},
    {	"scorch",	"scorching",		DAM_FIRE	},
    {	"scourge",	"scourge",		DAM_SLASH	},
    {	"scream",	"scream",		DAM_SOUND	},
    {	"shining",	"shining light",	DAM_LIGHT	},
    {	"shriek",	"deafening shriek",	DAM_SOUND	},
    {	"spike",	"spike",		DAM_PIERCE	},
    {	"spores",	"spores",		DAM_DISEASE	},
    {	"surge",	"surge",		DAM_LIGHTNING	},
    {	"torrent",	"watery torrent",	DAM_WATER	},
    {	"toxblast",	"toxic blast",		DAM_POISON	},
    {	"venom",	"venom",		DAM_POISON	},
    {	"winbrth",	"wintery breath",	DAM_COLD	},
    {   NULL,		NULL,			0		}
};


/* Attribute bonus tables.*/
/* Adjusted to go up to handle stats up to 50 now. This needs replaced by a proper formula. -- Areo*/
const	struct	str_app_type	str_app		[52]		=
{
    { -5, -4,   0,  0 },  /* 0 */
    { -5, -4,   6,  1 },  /* 1 */
    { -3, -2,   6,  2 },  /* 2 */
    { -3, -1,  15,  3 },  /* 3 */
    { -2, -1,  40,  4 },  /* 4 */
    { -2, -1,  70,  5 },  /* 5 */
    { -1,  0,  95,  6 },
    { -1,  0, 110,  7 },
    {  0,  0, 130,  8 },
    {  0,  0, 150,  9 },
    {  0,  0, 175, 10 }, /* 10 */
    {  0,  0, 180, 11 }, /* 11 */
    {  0,  0, 190, 12 }, /* 12 */
    {  0,  0, 200, 13 }, /* 13 */
    {  0,  1, 210, 14 },
    {  1,  1, 230, 15 }, /* 15 */
    {  1,  2, 250, 16 },
    {  2,  3, 300, 22 },
    {  2,  3, 350, 25 }, /* 18 */
    {  3,  4, 400, 30 }, /* 19 */
    {  3,  5, 450, 35 }, /* 20 */
    {  4,  6, 500, 40 }, /* 21 */
    {  4,  6, 550, 45 }, /* 22 */
    {  5,  7, 600, 50 }, /* 23 */
    {  5,  8, 650, 55 }, /* 24 */
    {  6,  9, 700, 60 }, /* 25 */
    {  6, 10, 750, 65 },
    {  7, 11, 775, 70 },
    {  7, 11, 800, 75 },
    {  8, 12, 825, 80 },
    {  8, 12, 850, 85 }, /* 30 */
    {  9, 13, 875, 90 },
    {  9, 13, 900, 95 },
    { 10, 14, 925, 100 },
    { 10, 14, 950, 105 },
    { 11, 15, 975, 110 }, /* 35 */
    { 11, 15, 1000, 115 },
    { 12, 15, 1025, 120 },
    { 12, 15, 1025, 120 },
    { 13, 16, 1050, 120 },
    { 13, 16, 1050, 120 }, /* 40 */
    { 14, 17, 1075, 120 },
    { 14, 17, 1075, 120 },
    { 15, 18, 1100, 120 },
    { 15, 18, 1100, 120 },
    { 16, 19, 1100, 120 }, /* 45 */
    { 16, 19, 1100, 120 },
    { 16, 19, 1125, 120 },
    { 16, 19, 1125, 120 },
    { 17, 20, 1125, 120 },
    { 17, 20, 1125, 120 }, /* 50 */
};


const	struct	int_app_type	int_app		[52]		=
{
    {  3 },	/*  0 */
    {  5 },	/*  1 */
    {  7 },
    {  8 },	/*  3 */
    {  9 },
    { 10 },	/*  5 */
    { 11 },
    { 12 },
    { 13 },
    { 15 },
    { 17 },	/* 10 */
    { 19 },
    { 22 },
    { 25 },
    { 28 },
    { 31 },	/* 15 */
    { 34 },
    { 37 },
    { 40 },	/* 18 */
    { 44 },
    { 49 },	/* 20 */
    { 55 },
    { 60 },
    { 70 },
    { 80 },
    { 85 },	/* 25 */
    { 91 },
    { 92 },
    { 95 },
    { 98 },
    { 99 },    /* 30 */
    { 100 },
    { 103 },
    { 105 },
    { 108 },
    { 114 },   /* 35 */
    { 118 },
    { 122 },
    { 126 },
    { 128 },
    { 130 },	/* 40 */
    { 131 },
    { 132 },
    { 133 },
    { 134 },
    { 135 },	/* 45 */
    { 136 },
    { 137 },
    { 138 },
    { 139 },
    { 140 },		/* 50 */
};


const	struct	wis_app_type	wis_app		[52]		=
{
    { 0 },	/*  0 */
    { 0 },	/*  1 */
    { 0 },
    { 0 },	/*  3 */
    { 0 },
    { 1 },	/*  5 */
    { 1 },
    { 1 },
    { 1 },
    { 1 },
    { 1 },	/* 10 */
    { 1 },
    { 1 },
    { 1 },
    { 1 },
    { 2 },	/* 15 */
    { 2 },
    { 2 },
    { 3 },	/* 18 */
    { 3 },
    { 3 },	/* 20 */
    { 3 },
    { 4 },
    { 4 },
    { 4 },
    { 5 },	/* 25 */
    { 5 },
    { 5 },
    { 5 },
    { 6 },
    { 6 },	/* 30 */
    { 6 },
    { 6 },
    { 7 },
    { 7 },
    { 7 }, /* 35 */
    { 7 },
    { 8 },
    { 8 },
    { 8 },
    { 8 }, /* 40 */
    { 8 },
    { 9 },
    { 9 },
    { 9 },
    { 9 }, /* 45 */
    { 9 },
    { 10 },
    { 10 },
    { 10 },
    { 10 }, /* 50 */
};


const	struct	con_app_type	con_app		[52]		=
{
    { -4, 20 },   /*  0 */
    { -3, 25 },   /*  1 */
    { -2, 30 },
    { -2, 35 },	  /*  3 */
    { -1, 40 },
    { -1, 45 },   /*  5 */
    { -1, 50 },
    {  0, 55 },
    {  0, 60 },
    {  0, 65 },
    {  0, 70 },   /* 10 */
    {  0, 75 },
    {  0, 80 },
    {  0, 85 },
    {  0, 88 },
    {  1, 90 },   /* 15 */
    {  2, 95 },
    {  2, 97 },
    {  3, 99 },   /* 18 */
    {  3, 99 },
    {  4, 99 },   /* 20 */
    {  4, 99 },
    {  5, 99 },
    {  6, 99 },
    {  7, 99 },
    {  8, 99 },    /* 25 */
    {  8, 99 },
    {  8, 99 },
    {  8, 99 },
    {  8, 99 },
    {  9, 99 }, /* 30 */
    {  9, 99 },
    {  9, 99 },
    {  9, 99 },
    {  9, 99 },
    { 10, 99 }, /* 35 */
    { 10, 99 },
    { 10, 99 },
    { 10, 99 },
    { 10, 99 },
    { 11, 99 }, /* 40 */
    { 11, 99 },
    { 11, 99 },
    { 11, 99 },
    { 11, 99 }, /* 45 */
    { 12, 99 },
    { 12, 99 },
    { 12, 99 },
    { 12, 99 },
    { 12, 99 }, /* 50 */

};


const struct  material_type material_table [] =
{
/*  name,	strength1-10,	value1-10(1 lowest)*/
    { "air",		1,  1   },
    { "dirt",		1,  1   },
    { "dust",		1,  1   },
    { "paper", 	  	1,  1  	},
    { "organic",  	1,  1  	},
    { "fruit",  	1,  1  	},
    { "meat",  		1,  1  	},
    { "moss",  		1,  1  	},
    { "leaves",  	1,  1  	},
    { "skin",	  	1,  1  	},
    { "ink",	  	1,  1  	},
    { "flesh",    	1,  1  	},
    { "clay",	  	1,  1  	},
    { "mud",	  	1,  1  	},
    { "cloth", 	  	1,  1  	},
    { "cotton", 	1,  1  	},
    { "fur", 		1,  1  	},
    { "light", 	 	1,  1 	},
    { "wool",	  	1,  1  	},
    { "energy",	 	1,  1  	},
    { "fire",		1,  1 	},
    { "water",		1,  1 	},
    { "glass",	   	1,  1  	},
    { "feathers", 	1,  1 	},
    { "intestines", 	1,  1 	},
    { "muscle", 	1,  1 	},
    { "parchment", 	1,  2  	},
    { "rubber",		1,  2  	},
    { "slime", 		1,  3 	},
    { "bark",		1,  1 	},
    { "wicker",		1,  1   },
    { "reed",		1,  1   },
    { "sand",		1,  1 	},
    { "salt",		1,  1 	},
    { "phosphorus",	1,  4   },
    { "sulfur",		1,  4   },
    { "hemp", 	  	2,  2  	},
    { "leather", 	2,  1  	},
    { "porcelain",	2,  6   },
    { "crystal", 	3,  7 	},
    { "ceramic", 	3,  2  	},
    { "wood", 		4,  1  	},
    { "ice",	  	4,  2  	},
    { "tin",		4,  1	},
    { "brass",		4,  2  	},
    { "copper", 	4,  2  	},
    { "zinc",		4,  4   },
    { "bone",	  	4,  1  	},
    { "shell",	  	4,  1  	},
    { "metal",		5,  1	},
    { "bronze", 	5,  1  	},
    { "iron", 		5,  1  	},
    { "steel", 		5,  1  	},
    { "lead",		6,  2	},
    { "onyx",		6,  3  	},
    { "ivory",		6,  6  	},
    { "silk",		6,  4  	},
    { "silver",		6,  6  	},
    { "nickel",		6,  3	},
    { "quartz", 	6,  7  	},
    { "obsidian", 	6,  6 	},
    { "gemstone", 	6,  6 	},
    { "spidersilk",	7,  4  	},
    { "ceramic",	7,  2	},
    { "ruby", 		7,  7 	},
    { "sapphire", 	7,  7 	},
    { "emerald", 	7,  7 	},
    { "amethyst",	7,  7	},
    { "opal",		7,  7	},
    { "pearl",		7,  7	},
    { "topaz",		7,  7	},
    { "gold", 		8,  8  	},
    { "granite", 	8,  1 	},
    { "concrete", 	8,  1  	},
    { "stone",		8,  1	},
    { "marble",		8,  6   },
    { "platinum", 	8,  9  	},
    { "moonstone",	9,  10	},
    { "dragonscale",	10, 9	},
    { "diamond", 	10, 10 	},
    { "mithril", 	10, 10 	},
    { "papyrus", 	4, 4 	},
    { "plant",		1, 1	},
    { "unknown",	1, 1	},
    { NULL,  		0,  0  	},
};


struct  newbie_eq_type  newbie_eq_table[] =
{
    {	0,	WEAR_BODY	},
    {	0,	WEAR_ABOUT	},
    {	0,	WEAR_LEGS	},
    {	0,	WEAR_FEET	},
    {	0,	WEAR_HEAD	},
    {	-1,			WEAR_NONE	},
};


const   struct  music_type 	music_table 	[MAX_SONGS] 	=
{
/*  name,	level gained, spell1,spell2,spell3, play length, mana cost, target*/
    { "Purple Mist", 			1, "armour", "shield", NULL, 12, 75, TAR_CHAR_DEFENSIVE },
    { "Fireworks", 				2, "magic missile", "lightning bolt", NULL, 9, 50, TAR_CHAR_OFFENSIVE },
    { "A Dwarven Tale", 		3, "stone skin", "infravision", NULL, 12, 75, TAR_CHAR_DEFENSIVE },
    { "Fade to Black", 			5, "improved invisibility", NULL, NULL, 8, 75, TAR_CHAR_DEFENSIVE },
    { "Pretty in Pink", 		7, "faerie fire", NULL, NULL, 4, 75, TAR_CHAR_OFFENSIVE },
    { "Aquatic Polka", 			8, "underwater breathing", NULL, NULL, 12, 75, TAR_CHAR_DEFENSIVE},
    { "Another Gate", 			9, "dispel magic", NULL, NULL, 8, 50, TAR_CHAR_OFFENSIVE },
    { "Awareness Jig", 			11, "detect invis", "detect magic", "detect hidden", 10, 100, TAR_CHAR_DEFENSIVE},
    { "Swamp Song", 			13, "poison", "acid blast", NULL, 9, 65, TAR_CHAR_OFFENSIVE },
    { "Fat Owl Hopping",	 	14, "giant strength", "fly", NULL, 12, 70, TAR_CHAR_DEFENSIVE },
    { "Stormy Weather", 		15, "call lightning", "call lightning", "call lightning", 9, 50, TAR_IGNORE },
    { "Rigor", 					18, "death grip", "frenzy", NULL, 18, 125, TAR_CHAR_DEFENSIVE },
    { "Firefly Tune", 			20, "heal", "refresh", NULL, 8, 50, TAR_CHAR_FORMATION },
    { "Blessed Be", 			22, "cure poison", "cure disease", "cure blindness", 24, 75, TAR_CHAR_DEFENSIVE },
    { "Dark Cloud", 			25, "blindness", NULL, NULL, 14, 75, TAR_CHAR_OFFENSIVE },
    { "Curse of the Abyss", 	28, "fireball", "energy drain", "demonfire", 13, 100, TAR_CHAR_OFFENSIVE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE },
    { NULL, 					0, NULL, NULL, NULL, 0, 0, TAR_IGNORE }
};


const	struct	liq_type	liq_table	[]	=
{
/*  name			colour		proof, full, thirst, food, ssize*/
    { "water",			"clear",	{   0, 1, 10, 0, 16 }	},
    { "beer",			"amber",	{  12, 1,  8, 1, 12 }	},
    { "red wine",		"burgundy",	{  30, 1,  8, 1,  5 }	},
    { "ale",			"brown",	{  15, 1,  8, 1, 12 }	},
    { "dark ale",		"dark",		{  16, 1,  8, 1, 12 }	},

    { "whisky",			"golden",	{ 120, 1,  5, 0,  2 }	},
    { "lemonade",		"pink",		{   0, 1,  9, 2, 12 }	},
    { "firebreather",		"boiling",	{ 190, 0,  4, 0,  2 }	},
    { "local specialty",	"clear",	{ 151, 1,  3, 0,  2 }	},
    { "slime mold juice",	"green",	{   0, 2, -8, 1,  2 }	},

    { "stew",			"brown",	{   0, 10, 10, 10, 12}  },
    { "milk",			"white",	{   0, 2,  9, 3,   12 }	},
    { "tea",			"tan",		{   0, 1,  8, 0,   6 }	},
    { "coffee",			"black",	{   0, 1,  8, 0,   6 }	},
    { "blood",			"red",		{   0, 2, 0, 0,   6 }	},

    { "salt water",		"clear",	{   0, 1, -2, 0,  1 }	},
    { "coke",			"brown",	{   0, 2,  9, 2, 12 }	},
    { "root beer",		"brown",	{   0, 2,  9, 2, 12 }   },
    { "elvish wine",		"green",	{  35, 2,  8, 1,  5 }   },
    { "white wine",		"golden",	{  28, 1,  8, 1,  5 }   },

    { "champagne",		"golden",	{  32, 1,  8, 1,  5 }   },
    { "mead",			"honey-coloured",{  34, 2,  8, 2, 12 }   },
    { "rose wine",		"pink",		{  26, 1,  8, 1,  5 }	},
    { "benedictine wine",	"burgundy",	{  40, 1,  8, 1,  5 }   },
    { "vodka",			"clear",	{ 130, 1,  5, 0,  2 }   },

    { "cranberry juice",	"red",		{   0, 1,  9, 2, 12 }	},
    { "orange juice",		"orange",	{   0, 2,  9, 3, 12 }   },
    { "absinthe",		"green",	{ 200, 1,  4, 0,  2 }	},
    { "brandy",			"golden",	{  80, 1,  5, 0,  4 }	},
    { "aquavit",		"clear",	{ 140, 1,  5, 0,  2 }	},

    { "schnapps",		"clear",	{  90, 1,  5, 0,  2 }   },
    { "ice wine",		"purple",	{  50, 2,  6, 1,  5 }	},
    { "amontillado",		"burgundy",	{  35, 2,  8, 1,  5 }	},
    { "sherry",			"red",		{  38, 2,  7, 1,  5 }   },
    { "framboise",		"red",		{  50, 1,  7, 1,  5 }   },

    { "rum",			"amber",	{ 151, 1,  4, 0,  2 }	},
    { "cordial",		"clear",	{ 100, 1,  5, 0,  2 }   },

    { "ammonia",		"pale green",	{   0, 1, 0, 0, 10  }	},
    { "grog",			"dark brown",	{  40, 2, 4, 0, 12  }	},
    { "snake oil",		"viscous",	{   0, 2, 1, 2, 10  }	},
    { "vinegar",		"pungent",	{   0, 2, 1, 3, 8   } 	},
    { "acetone",		"clear",	{   0, 1, 0, 0, 9   }	},

    { NULL,			NULL,		{   0, 0,  0, 0,  0 }	}
};


/*
 * The skill and spell table.
 */

const struct skill_type	skill_table [MAX_SKILL]	=
{
    /*
    {
    name,	{level needed by each class}, {difficulty to learn for each class},
    spell pointer,	legal targets,	min position for caster/user,
    gsn pointer,	racial skill,	min mana used,	waiting time after use,
    damage message,	wear off message,	wear off message(for objects)
    }
    */
    {
        "reserved",
        { 1, 1, 1, 1 }, { 200, 200, 200, 200},
        NULL, TAR_IGNORE, POS_STANDING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "acid blast",
        { 16, 31, 31, 16 }, { 4, 4, 4, 4},
        spell_acid_blast, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 4,
        "acid blast", "!Acid Blast!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "acid breath",
        { 11, 11, 11, 11 }, { 10, 1, 2, 2},
        spell_acid_breath, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 100, 2,
        "blast of acid", "!Acid Breath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "acrobatics",
        { 31, 31, 3, 31 }, { 8, 8, 8, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Acrobatics!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "afterburn",
        { 20, 31, 31, 31 }, { 15, 25, 25, 25 },
        spell_afterburn, TAR_IGNORE, POS_STANDING,
        0, 150, 10,
        "scorching fireball", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "air spells",
        {1, 31, 31, 31}, {15, 20, 30, 30},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "ambush",
        { 31, 31, 22, 31 }, { 20, 18, 13, 14 },
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "animate dead",
        { 12, 31, 31, 31 }, { 5, 5, 4, 4},
        spell_animate_dead, TAR_OBJ_GROUND, POS_STANDING,
        -1, 75, 8,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "archery",
        { 10, 10, 7, 8 }, { 15, 15, 10, 12},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "shot", "!Archery!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "armour",
        { 3, 1, 31, 3 }, { 2, 2, 5, 5},
        spell_armour, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 5, 2,
        "", "{CYou feel less armoured.{x", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "athletics",
        { 31, 31, 25, 5 }, { 14, 14, 14, 8 },
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "avatar shield",
        { 31, 10, 31, 31 }, { 3, 5, 2, 2},
        spell_avatar_shield, TAR_CHAR_SELF, POS_STANDING,
        -1, 75, 12,
        "", "{CYou feel less resistant to evil attacks.{x", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_HOLY, 2 } }
    }, {
        "axe",
        { 1, 1, 1, 1 }, { 6, 6, 5, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Axe!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "backstab",
        { 31, 31, 15, 31 }, { 0, 0, 5, 0},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 6,
        "backstab", "!Backstab!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bar",
        { 31, 31, 12, 31 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "bar", "!Bar!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bash",
        { 15, 12, 11, 10}, { 11, 5, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 9,
        "bash", "!Bash!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "behead",
        { 31, 31, 31, 18 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Behead!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "berserk",
        { 2, 2, 2, 2 }, { 9, 8, 6, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_DWARF, 0, 24,
        "", "{CYou feel your pulse slow down.{x", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bind",
        { 31, 31, 31, 8 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Bind!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bite",
        { 1, 1, 1, 1 }, { 2, 2, 2, 2},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "bite", "!Bite!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "blackjack",
        { 31, 31, 21, 31 }, { 0, 0, 5, 0},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "blackjack", "!Blackjack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bless",
        { 10, 4, 31, 4 }, { 2, 2, 4, 4},
        spell_bless, TAR_OBJ_CHAR_DEF, POS_STANDING,
        -1, 5, 2,
        "", "{CYou feel less righteous.{x", "$p's holy aura fades.", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "blindness",
        { 6, 4, 31, 6 }, { 3, 3, 4, 5},
        spell_blindness, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 75, 8,
        "", "You can see again.", "", "$n is no longer blinded.",
        { { CATALYST_MIND, 1 },{ CATALYST_BODY, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "blowgun",
        { 1, 1, 1, 1 }, { 4, 4, 4, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Blowgun!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bomb",
        { 31, 31, 14, 31 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "bomb", "!Bomb!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "bow",
        { 1, 1, 1, 1 }, { 10, 10, 8, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "arrow", "!Bow!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "breath",
        { 1, 1, 1, 1 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_DRACONIAN, 0, 12,
        "breath", "!Breath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "brew",
        { 31, 13, 31, 31 }, { 8, 8, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Brew!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "burgle",
        { 31, 31, 15, 31 }, { 0, 0, 9, 0},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 0,
        "", "!Burgle!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "burning hands",
        { 3, 31, 31, 31 }, { 3, 3, 3, 3},
        spell_burning_hands, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 4,
        "burning hands", "!Burning Hands!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "call familiar",
        { 31, 16, 31, 31 }, { 4, 8, 2, 2},
        spell_call_familiar, TAR_IGNORE, POS_STANDING,
        -1, 5, 12,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "call lightning",
        { 13, 9, 31, 9 }, { 7, 7, 6, 7},
        spell_call_lightning, TAR_IGNORE, POS_FIGHTING,
        -1, 15, 8,
        "lightning bolt", "!Call Lightning!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "calm",
        { 18, 8, 31, 8 }, { 5, 5, 4, 4},
        spell_calm, TAR_IGNORE, POS_FIGHTING,
        -1, 30, 8,
        "", "{CYou have lost your peace of mind.{x", "", "$n no longer looks so peaceful...",
        { { CATALYST_MIND, 1 },{ CATALYST_HOLY, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "cancellation",
        { 9, 31, 31, 31 }, { 4, 4, 4, 4},
        spell_cancellation, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 20, 12,
        "", "!cancellation!", "", "",
        { { CATALYST_BODY, 5 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "catch",
        { 31, 14, 31, 31 }, { 8, 10, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Catch!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cause critical",
        { 31, 16, 31, 16 }, { 4, 4, 4, 4},
        spell_cause_critical, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 6,
        "spell", "!Cause Critical!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cause light",
        { 31, 1, 31, 1 }, { 1, 1, 2, 2},
        spell_cause_light, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 2,
        "spell", "!Cause Light!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cause serious",
        { 31, 4, 31, 4 }, { 1, 3, 2, 2},
        spell_cause_serious, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 17, 6,
        "spell", "!Cause Serious!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "chain lightning",
        { 18, 31, 31, 31 }, { 4, 8, 2, 2},
        spell_chain_lightning, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 75, 12,
        "lightning", "!Chain Lightning!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "channel",
        { 14, 31, 31, 31 }, { 4, 1, 2, 2},
        spell_channel, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 9,
        "channel", "!Channel!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "charge",
        { 1, 1, 1, 1}, { 4, 4, 4, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_MINOTAUR, 0, 3,
        "charge", "!Charge!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "charm person",
        { 10, 31, 31, 31 }, { 4, 7, 2, 2},
        spell_charm_person, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 5, 12,
        "", "{YYou feel more self-confident.{x", "", "$n regains $s free will.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "chill touch",
        { 2, 31, 31, 31 }, { 3, 4, 2, 2},
        spell_chill_touch, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "chilling touch", "{CYou feel less cold.{x", "", "$n looks to shiver less.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "circle",
        { 31, 31, 15, 15 }, { 0, 0, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 16,
        "circle", "!Circle!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cloak of guile",
        { 31, 31, 6, 31 }, { 5, 5, 5, 5},
        spell_cloak_of_guile, TAR_CHAR_SELF, POS_STANDING,
        -1, 50, 12,
        "", "{YYou are no longer invisible to creatures.{x", "", "$n is no longer invisible to creatures.",
        { { CATALYST_BODY, 10 },{ CATALYST_ASTRAL, 5 },{ CATALYST_LAW, 5 } }
    }, {
        "colour spray",
        { 8, 31, 31, 31 }, { 3, 3, 2, 2},
        spell_colour_spray, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "colour spray", "!Colour Spray!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "combine",
        { 31, 9, 31, 31 }, { 15, 12, 10, 9 },
        spell_null, TAR_IGNORE, POS_RESTING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "consume",
        { 1, 1, 1, 1 }, { 1, 1, 1, 1},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_VAMPIRE, 0, 6,
        "", "!Consume!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "continual light",
        { 6, 4, 31, 6 }, { 2, 3, 2, 2},
        spell_continual_light, TAR_OBJ_INV, POS_STANDING,
        -1, 7, 6,
        "", "!Continual Light!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "control weather",
        { 15, 19, 31, 31 }, { 3, 3, 2, 2},
        spell_control_weather, TAR_IGNORE, POS_STANDING,
        -1, 25, 4,
        "", "!Control Weather!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cosmic blast",
        { 31, 25, 31, 31 }, { 1, 8, 2, 2},
        spell_cosmic_blast, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 75, 10,
        "cosmic blast", "!Cosmic Blast!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "counterspell",
        { 22, 22, 31, 31 }, { 6, 6, 5, 5},
        spell_counter_spell, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 100, 1,
        "counterspell", "!Counterspell!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "create food",
        { 31, 5, 31, 5 }, { 1, 3, 2, 2},
        spell_create_food, TAR_IGNORE, POS_STANDING,
        -1, 5, 2,
        "", "!Create Food!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "create rose",
        {15, 5, 31, 31}, {6, 4, 8, 9},
        spell_create_rose, TAR_IGNORE, POS_STANDING,
        0, 50, 8,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "create spring",
        { 8, 31, 31, 8 }, { 1, 3, 2, 2},
        spell_create_spring, TAR_IGNORE, POS_STANDING,
        -1, 20, 6,
        "", "!Create Spring!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "create water",
        { 31, 3, 31, 3 }, { 1, 3, 2, 2},
        spell_create_water, TAR_OBJ_INV, POS_STANDING,
        -1, 5, 2,
        "", "!Create Water!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "crippling touch",
        { 5, 5, 3, 1 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_LICH, 0, 0,
        "", "!crippling touch!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "crossbow",
        { 1, 1, 1, 1 }, { 10, 10, 9, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "bolt", "!Crossbow!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cure blindness",
        { 31, 6, 31, 6 }, { 1, 4, 2, 2},
        spell_cure_blindness, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 5, 4,
        "", "!Cure Blindness!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "cure critical",
        { 10, 10, 10, 10 }, { 4, 4, 4, 4},
        spell_cure_critical, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 35, 6,
        "", "!Cure Critical!", "", "",
        { { CATALYST_BODY, 4 },{ CATALYST_BLOOD, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "cure disease",
        { 31, 9, 31, 9 }, { 1, 3, 2, 2},
        spell_cure_disease, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 20, 6,
        "", "!Cure Disease!", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_BLOOD, 1 },{ CATALYST_TOXIN, 3 } }
    }, {
        "cure light",
        { 1, 1, 1, 1 }, { 1, 2, 2, 2},
        spell_cure_light, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 10, 2,
        "", "!Cure Light!", "", "",
        { { CATALYST_BODY, 1 },{ CATALYST_BLOOD, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "cure poison",
        { 31, 10, 31, 10 }, { 1, 4, 2, 2},
        spell_cure_poison, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 5, 2,
        "", "!Cure Poison!", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_BLOOD, 1 },{ CATALYST_TOXIN, 3 } }
    }, {
        "cure serious",
        { 7, 7, 7, 7 }, { 4, 4, 2, 2},
        spell_cure_serious, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "", "!Cure Serious!", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_BLOOD, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "cure toxic",
        { 31, 31, 31, 31 }, { 31, 31, 31, 31},
        spell_cure_toxic, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 5, 2,
        "", "!Cure Toxic!", "", "",
        { { CATALYST_BODY, 5 },{ CATALYST_TOXIN, 2 },{ CATALYST_DEATH, 2 } }
    }, {
        "curse",
        { 10, 10, 31, 10 }, { 1, 4, 2, 2},
        spell_curse, TAR_OBJ_CHAR_OFF, POS_FIGHTING,
        -1, 20, 6,
        "curse", "{CThe curse wears off.{x", "$p is no longer impure.", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dagger",
        { 1, 1, 1, 1 }, { 2, 3, 2, 2},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Dagger!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dark shroud",
        { 10, 31, 31, 31 }, { 2, 1, 2, 2},
        spell_dark_shroud, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 40, 4,
        "", "{WEverything seems brighter as your dark shroud fades.{x", "", "",
        { { CATALYST_MIND, 2 },{ CATALYST_DEATH, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "death grip",
        { 13, 31, 31, 31 }, { 6, 1, 2, 2},
        spell_death_grip, TAR_CHAR_SELF, POS_STANDING,
        -1, 20, 6,
        "", "Your grip on your weapon loosens.", "", "$n's grip on $s weapon loosens.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "deathbarbs",
        { 31, 31, 13, 31 }, { 6, 6, 6, 6},
        spell_deathbarbs, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 100, 8,
        "deathbarbs", "!Deathbarbs!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "deathsight",
        { 5, 31, 31, 31 }, { 2, 1, 2, 2},
        spell_deathsight, TAR_CHAR_SELF, POS_STANDING,
        -1, 15, 4,
        "", "{DYour perception of the dead fades.{x", "", "",
        { { CATALYST_MIND, 2 },{ CATALYST_DEATH, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "deception",
        { 8, 8, 8, 8 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "deception", "!Deception!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "deep trance",
        { 8, 5, 31, 31 }, { 7, 6, 8, 9 },
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "demonfire",
        { 31, 25, 31, 31 }, { 1, 8, 2, 2},
        spell_demonfire, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 8,
        "torments", "!Demonfire!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "destruction",
        { 8, 31, 31, 31 }, { 5, 10, 10, 10 },
        spell_destruction, TAR_OBJ_INV, POS_STANDING,
        0, 250, 14,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "detect hidden",
        { 12, 12, 12, 12 }, { 3, 3, 2, 2},
        spell_detect_hidden, TAR_CHAR_SELF, POS_STANDING,
        -1, 5, 2,
        "", "{YYou feel less aware of your surroundings.{x", "", "",
        { { CATALYST_MIND, 2 },{ CATALYST_LIGHT, 1 },{ CATALYST_EARTH, 1 } }
    }, {
        "detect invis",
        { 3, 8, 31, 8 }, { 3, 3, 2, 2},
        spell_detect_invis, TAR_CHAR_SELF, POS_FIGHTING,
        -1, 5, 2,
        "", "{YYou no longer see invisible objects.{x", "", "",
        { { CATALYST_MIND, 2 },{ CATALYST_LIGHT, 1 },{ CATALYST_AIR, 1 } }
    }, {
        "detect magic",
        { 2, 6, 31, 6 }, { 3, 3, 2, 2},
        spell_detect_magic, TAR_CHAR_SELF, POS_STANDING,
        -1, 5, 2,
        "", "{CThe detect magic wears off.{x", "", "",
        { { CATALYST_MIND, 2 },{ CATALYST_LIGHT, 1 },{ CATALYST_ENERGY, 1 } }
    }, {
        "detect traps",
        {31, 31, 12, 31}, {25, 22, 8, 15},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dirt kicking",
        { 16, 16, 16, 16 }, { 20, 20, 4, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 7,
        "kicked dirt", "You rub the dirt out of your eyes.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "disarm",
        { 31, 31, 31, 11 }, { 20, 20, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 12,
        "", "!Disarm!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "discharge",
        { 17, 31, 31, 31 }, { 8, 2, 4, 4},
        spell_discharge, TAR_OBJ_INV, POS_STANDING,
        -1, 250, 16,
        "", "!Discharge!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dispel evil",
        { 31, 15, 31, 15 }, { 4, 3, 2, 2},
        spell_dispel_evil, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "dispel evil", "!Dispel Evil!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dispel good",
        { 31, 15, 31, 15 }, { 4, 4, 2, 2},
        spell_dispel_good, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "dispel good", "!Dispel Good!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dispel magic",
        { 16, 15, 31, 31 }, { 6, 6, 2, 2},
        spell_dispel_magic, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 8,
        "", "!Dispel Magic!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dispel room",
        { 18, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_dispel_room, TAR_IGNORE, POS_STANDING,
        -1, 100, 16,
        "", "!Dispel Room!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dodge",
        { 31, 8, 5, 5 }, { 8, 8, 4, 6},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Dodge!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "dual wield",
        { 31, 31, 31, 5 }, { 5, 5, 10, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Dual!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "eagle eye",
        { 21, 31, 31, 31 }, { 8, 6, 6, 6},
        spell_eagle_eye, TAR_CHAR_SELF, POS_STANDING,
        -1, 50, 8,
        "", "", "", "",
        { { CATALYST_AIR, 4 },{ CATALYST_COSMIC, 4 },{ CATALYST_NONE, 0 } }
    }, {
        "earth spells",
        {1, 31, 31, 31}, {15, 20, 30, 30},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "earth walk",
        { 20, 31, 31, 31 }, { 15, 25, 25, 25 },
        spell_earth_walk, TAR_IGNORE_CHAR_DEF, POS_STANDING,
        0, 150, 10,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "earthquake",
        { 15, 10, 15, 10 }, { 1, 5, 2, 5},
        spell_earthquake, TAR_IGNORE, POS_FIGHTING,
        -1, 25, 6,
        "earthquake", "!Earthquake!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "electrical barrier",
        { 20, 31, 31, 31}, { 15, 15, 0, 0 },
        spell_electrical_barrier, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 100, 8,
        "electrical wave", "{CThe electrical barrier surrounding you vanishes.{x", "", "The hazy blue barrier around $n vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 1 },{ CATALYST_ENERGY, 2 } }
    }, {
        "enchant armour",
        { 10, 15, 31, 31 }, { 3, 2, 4, 4 },
        spell_enchant_armour, TAR_OBJ_INV, POS_STANDING,
        -1, 100, 8,
        "", "!Enchant Armour!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "enchant weapon",
        { 17, 15, 31, 31 }, { 4, 2, 4, 4},
        spell_enchant_weapon, TAR_OBJ_INV, POS_STANDING,
        -1, 100, 8,
        "", "!Enchant Weapon!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "energy drain",
        { 15, 31, 31, 31 }, { 4, 1, 2, 2},
        spell_energy_drain, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 6,
        "energy drain", "!Energy Drain!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "energy field",
        { 31, 31, 31, 17 }, { 10, 10, 2, 8},
        spell_energy_field, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 8,
        "", "{WThe humming noise around you fades.{x", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 1 },{ CATALYST_ENERGY, 2 } }
    }, {
        "enhanced damage",
        { 31, 22, 15, 13 }, { 10, 9, 5, 6},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Enhanced Damage!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "ensnare",
        {15, 31, 31, 31}, {6, 10, 11, 11},
        spell_ensnare, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 7,
        "", "The vines clutching you wither and fall away.", "", "The vines clutching $n crumble.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "entrap",
        {8, 31, 31, 31}, {5, 9, 10, 10},
        spell_entrap, TAR_OBJ_INV, POS_STANDING,
        -1, 45, 10,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "envenom",
        { 31, 31, 10, 31 }, { 20,20, 5, 20 },
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 36,
        "", "!Envenom!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "evasion",
        { 31, 31, 20, 31 }, { 6, 6, 8, 6},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "You no longer feel evasive.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "exorcism",
        { 31, 15, 31, 31 }, { 4, 5, 2, 2},
        spell_exorcism, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 12,
        "exorcism", "!Exorcism!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "exotic",
        { 1, 1, 1, 1 }, { 2, 3, 2, 2},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Exotic!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fade",
        { 1, 1, 1, 1 }, { 10, 10, 10, 10},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "fade", "!Fade!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "faerie fire",
        { 6, 3, 31, 6 }, { 4, 4, 2, 2},
        spell_faerie_fire, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 5, 6,
        "faerie fire", "{MThe pink aura around you fades away.{x", "", "$n's outline fades.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "faerie fog",
        { 14, 31, 31, 14 }, { 4, 1, 2, 2},
        spell_faerie_fog, TAR_IGNORE, POS_STANDING,
        -1, 12, 6,
        "faerie fog", "!Faerie Fog!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fast healing",
        { 12, 12, 11, 6 }, { 8, 5, 6, 4},
        spell_null, TAR_IGNORE, POS_SLEEPING,
        -1, 0, 0,
        "", "!Fast Healing!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fatigue",
        { 14, 31, 31, 31 }, { 5, 5, 5, 5},
        spell_fatigue, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 35, 6,
        "spell_fatigue", "You regain your stamina.", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_CHAOS, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "feign",
        { 8, 8, 8, 8 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "feign", "!Feign!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fire barrier",
        { 12, 31, 31, 31}, { 10, 10, 0, 0 },
        spell_fire_barrier, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 50, 8,
        "fire wave", "{RThe flames protecting you vanish.{x", "", "The fire shield surrounding $n vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 1 },{ CATALYST_FIRE, 2 } }
    }, {
        "fire breath",
        { 1, 1, 1, 1 }, { 10, 1, 2, 2},
        spell_fire_breath, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 200, 2,
        "blast of flame", "The smoke leaves your eyes.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fire cloud",
        { 13, 31, 31, 31 }, { 8, 6, 2, 2},
        spell_fire_cloud, TAR_IGNORE, POS_STANDING,
        -1, 50, 12,
        "fire cloud", "!Fire Cloud!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fire spells",
        {1, 31, 31, 31}, {15, 20, 30, 30},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fireball",
        { 7, 31, 31, 31 }, { 2, 2, 2, 2},
        spell_fireball, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 6,
        "fireball", "!Fireball!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fireproof",
        { 12, 12, 31, 12 }, { 6, 6, 2, 2},
        spell_fireproof, TAR_OBJ_INV, POS_STANDING,
        -1, 10, 6,
        "", "", "{R$p's protective aura fades.{x", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "flail",
        { 1, 1, 1, 1 }, { 6, 3, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Flail!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "flamestrike",
        { 31, 17, 31, 31 }, { 6, 6, 2, 2},
        spell_flamestrike, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 8,
        "flamestrike", "!Flamestrike!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "flash",
        { 20, 31, 31, 31 }, { 15, 25, 25, 25 },
        spell_flash, TAR_IGNORE, POS_STANDING,
        0, 150, 10,
        "", "", "", "",
        { { CATALYST_LIGHT, 5 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "flight",
        { 1, 1, 1, 1 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Flight!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fly",
        { 10, 17, 31, 17 }, { 4, 4, 2, 2},
        spell_fly, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 10, 4,
        "", "{CYou slowly float to the ground.{x", "", "$n falls to the ground!",
        { { CATALYST_AIR, 2 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "fourth attack",
        { 31, 31, 31, 24 }, { 20, 20, 10, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Fourth Attack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "frenzy",
        { 31, 24, 31, 28 }, { 1, 5, 2, 8},
        spell_frenzy, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 30, 8,
        "", "{CYour rage ebbs.{x", "", "$n no longer looks so wild.",
        { { CATALYST_MIND, 2 },{ CATALYST_CHAOS, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "frost barrier",
        { 15, 31, 31, 31}, { 12, 12, 0, 0 },
        spell_frost_barrier, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 8,
        "frost wave", "{BThe air around you heats up as the frost barrier dissipates.{x", "", "The frost shield surrounding $n vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 1 },{ CATALYST_ICE, 2 } }
    }, {
        "frost breath",
        { 7, 7, 7, 7 }, { 10, 1, 2, 2},
        spell_frost_breath, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 125, 2,
        "blast of frost", "!Frost Breath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "gas breath",
        { 15, 15, 15, 15 }, { 10, 1, 2, 2},
        spell_gas_breath, TAR_IGNORE, POS_FIGHTING,
        -1, 175, 2,
        "blast of gas", "!Gas Breath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "gate",
        { 18, 18, 31, 31 }, { 8, 8, 2, 2},
        spell_gate, TAR_IGNORE_CHAR_DEF, POS_FIGHTING,
        -1, 80, 6,
        "", "!Gate!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "giant strength",
        { 15, 31, 31, 18 }, { 4, 4, 2, 4},
        spell_giant_strength, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 20, 4,
        "", "{YYou feel weaker.{x", "", "$n no longer looks so mighty.",
        { { CATALYST_BODY, 2 },{ CATALYST_EARTH, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "glacial wave",
        { 20, 31, 31, 31 }, { 15, 25, 25, 25 },
        spell_glacial_wave, TAR_IGNORE, POS_STANDING,
        0, 150, 10,
        "glacial wave", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "glorious bolt",
        { 31, 25, 31, 31 }, { 2, 8, 4, 4},
        spell_glorious_bolt, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 200, 10,
        "glorious bolt", "!Glorious Bolt!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "haggle",
        { 31, 31, 5, 31 }, { 5, 8, 3, 6},
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 0,
        "", "!Haggle!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "hand to hand",
        { 1, 1, 1, 1 }, { 8, 5, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Hand to Hand!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "harm",
        { 31, 24, 31, 24 }, { 1, 5, 2, 5},
        spell_harm, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 35, 6,
        "harm spell", "!Harm!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "harpooning",
        { 1, 1, 1, 1 }, { 4, 4, 4, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Thar She Blows!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "haste",
        { 8, 31, 31, 31 }, { 6, 1, 2, 2},
        spell_haste, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 30, 6,
        "", "{YYou feel yourself slow down.{x", "", "$n is no longer moving so quickly.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_CHAOS, 1 } }
    }, {
        "heal",
        { 15, 15, 15, 15 }, { 6, 6, 6, 6},
        spell_heal, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 50, 7,
        "", "!Heal!", "", "",
        { { CATALYST_BODY, 5 },{ CATALYST_BLOOD, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "healing aura",
        { 31, 31, 31, 15 }, { 6, 6, 2, 2},
        spell_healing_aura, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 6,
        "healing_aura", "Your healing aura fades.", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_BLOOD, 4 } }
    }, {
        "healing hands",
        { 15, 15, 15, 14 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "healing hands", "!Healing Hands!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "hide",
        { 13, 13, 1, 13 }, { 4, 6, 6, 6},
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 12,
        "", "!Hide!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "holdup",
        { 31, 31, 10, 31 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "holdup", "!Holdup!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "holy shield",
        { 31, 31, 31, 12 }, { 2, 2, 4, 8},
        spell_holy_shield, TAR_OBJ_INV, POS_STANDING,
        -1, 5, 2,
        "", "", "The runes on $p fade.", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "holy sword",
        { 31, 31, 31, 14 }, { 2, 2, 4, 8},
        spell_holy_sword, TAR_OBJ_INV, POS_STANDING,
        -1, 5, 2,
        "", "", "The runes on $p fade.", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "holy word",
        { 31, 25, 31, 31 }, { 2, 8, 4, 4},
        spell_holy_word, TAR_IGNORE, POS_FIGHTING,
        -1, 200, 16,
        "divine wrath", "!Holy Word!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "holy wrath",
        { 1, 1, 1, 1 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_SLAYER, 0, 0,
        "", "!holy wrath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "hunt",
        { 17, 17, 17, 17 }, { 10, 10, 10, 4},
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 7,
        "", "!Hunt!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "ice shards",
        { 7, 31, 31, 31 }, { 2, 2, 2, 2},
        spell_ice_shards, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 6,
        "shards of ice", "!Ice Shards!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "ice storm",
        { 15, 31, 31, 31 }, { 8, 15, 20, 20 },
        spell_ice_storm, TAR_IGNORE, POS_STANDING,
        0, 110, 9,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "identify",
        { 8, 10, 31, 31 }, { 2, 2, 2, 2},
        spell_identify, TAR_OBJ_INV, POS_STANDING,
        -1, 12, 4,
        "", "!Identify!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "improved invisibility",
        { 9, 31, 31, 31 }, { 4, 1, 2, 2},
        spell_improved_invisibility, TAR_CHAR_SELF, POS_STANDING,
        -1, 50, 8,
        "", "{CYou are no longer invisible.{x", "", "$n fades into existance.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_AIR, 5 } }
    }, {
        "inferno",
        { 18, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_inferno, TAR_IGNORE, POS_STANDING,
        -1, 50, 6,
        "inferno", "!Inferno!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "infravision",
        { 4, 31, 31, 31 }, { 2, 1, 2, 2},
        spell_infravision, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 5, 4,
        "", "{CYou no longer see in the dark.{x", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_LIGHT, 1 } }
    }, {
        "infuse",
        { 31, 15, 31, 31}, { 20, 8, 20, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 12,
        "infuse", "!Infuse!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "intimidate",
        { 31, 31, 31, 13 }, { 20, 20, 20, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 24,
        "intimidate", "!Intimidate!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "invisibility",
        { 5, 31, 31, 31 }, { 4, 1, 2, 2},
        spell_invis, TAR_OBJ_CHAR_DEF, POS_STANDING,
        -1, 5, 4,
        "", "{CYou are no longer invisible.{x", "$p fades into view.", "$n fades into existance.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_AIR, 2 } }
    }, {
        "judge",
        { 31, 31, 6, 31 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "judge", "!Judge!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "kick",
        { 31, 8, 31, 8 }, { 20, 4, 6, 4},
        spell_null, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 0, 12,
        "kick", "!Kick!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "kill",
        { 12, 31, 31, 31 }, { 5, 5, 4, 4},
        spell_kill, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 150, 10,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "leadership",
        { 31, 31, 31, 18 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Leadership!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "light shroud",
        { 31, 31, 31, 16 }, { 10, 10, 2, 8},
        spell_light_shroud, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 8,
        "", "{WThe white shroud around your body fades.{x", "", "The light shroud around $n's body vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_LIGHT, 2 },{ CATALYST_HOLY, 2 } }
    }, {
        "lightning bolt",
        { 3, 31, 31, 31 }, { 2, 2, 2, 2},
        spell_lightning_bolt, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "lightning bolt", "!Lightning Bolt!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "lightning breath",
        { 21, 21, 21, 21 }, { 1, 1, 2, 2},
        spell_lightning_breath, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 150, 2,
        "blast of lightning", "!Lightning Breath!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "locate object",
        { 9, 10, 31, 31 }, { 5, 5, 2, 2},
        spell_locate_object, TAR_IGNORE, POS_STANDING,
        -1, 20, 6,
        "", "!Locate Object!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "lore",
        { 14, 11, 11, 31 }, { 6, 6, 4, 8},
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 36,
        "", "!Lore!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "mace",
        { 1, 1, 1, 1 }, { 5, 2, 3, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Mace!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "magic missile",
        { 1, 31, 31, 31 }, { 1, 1, 2, 2},
        spell_magic_missile, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 6,
        "magic missile", "!Magic Missile!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "martial arts",
        { 31, 1, 5, 3 }, { 15, 10, 12, 10 },
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "mass healing",
        { 31, 27, 31, 31 }, { 8, 8, 4, 4},
        spell_mass_healing, TAR_IGNORE, POS_STANDING,
        -1, 300, 8,
        "", "!Mass Healing!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "mass invis",
        { 22, 31, 31, 31 }, { 8, 7, 2, 2},
        spell_mass_invis, TAR_IGNORE, POS_STANDING,
        -1, 20, 8,
        "", "{CYou are no longer invisible.{x", "", "$n fades into existance.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "master weather",
        { 22, 31, 31, 31 }, { 8, 9, 12, 15 },
        spell_master_weather, TAR_IGNORE, POS_STANDING,
        -1, 200, 12,
        "master weather", "!Master Weather!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "maze",
        { 22, 31, 31, 31 }, { 5, 5, 4, 4},
        spell_maze, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 100, 8,
        "", "!Maze!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "meditation",
        { 6, 6, 31, 31 }, { 5, 5, 8, 8},
        spell_null, TAR_IGNORE, POS_SLEEPING,
        -1, 0, 0,
        "", "Meditation", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "mob lore",
        { 31, 31, 14, 31 }, { 6, 6, 6, 6},
        spell_null, TAR_IGNORE, POS_RESTING,
        -1, 0, 36,
        "", "!Mob Lore!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "momentary darkness",
        { 20, 31, 31, 31 }, { 6, 6, 6, 6},
        spell_momentary_darkness, TAR_IGNORE, POS_FIGHTING,
        -1, 500, 15,
        "momentary darkness", "!Momentary Darkness!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "morphlock",
        { 31, 31, 31, 31 }, { 31, 31, 31, 31 },
        spell_morphlock, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 12,
        "", "Your body feels free.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "mount and weapon style",
        { 31, 31, 31, 6 }, { 10, 9, 5, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Mount And Weapon Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "music",
        { 1, 1, 1, 1 }, { 2, 3, 5, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Music!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "navigation",
        { 1, 1, 1, 1 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Navigation!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "neurotoxin",
        { 31, 31, 31, 31 }, {0,0,0,0},
        spell_toxin_neurotoxin, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 0, 1 ,
        "","","","",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "nexus",
        { 23, 31, 31, 31 }, { 8, 8, 4, 4},
        spell_nexus, TAR_IGNORE_CHAR_DEF, POS_STANDING,
        -1, 150, 8,
        "", "!Nexus!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "parry",
        { 31, 31, 31, 15 }, { 8, 8, 6, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Parry!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "pass door",
        { 19, 31, 31, 31 }, { 6, 1, 2, 2},
        spell_pass_door, TAR_CHAR_SELF, POS_STANDING,
        -1, 20, 4,
        "", "{CYou feel solid again.{x", "", "$n becomes solid again.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_AIR, 2 } }
    }, {
        "peek",
        { 31, 31, 11, 31 }, { 5, 7, 3, 6},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 0,
        "", "!Peek!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "pick lock",
        { 31, 31, 7, 31 }, { 8, 8, 4, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 3,
        "", "!Pick!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "plague",
        { 21, 21, 31, 23 }, { 3, 6, 2, 2},
        spell_plague, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 6,
        "sickness", "Your sores vanish.", "", "",
        { { CATALYST_BODY, 5 },{ CATALYST_TOXIN, 2 },{ CATALYST_DEATH, 1 } }
    }, {
        "poison",
        { 12, 14, 31, 17 }, { 3, 6, 2, 2},
        spell_poison, TAR_OBJ_CHAR_OFF, POS_FIGHTING,
        -1, 10, 6,
        "poison", "You feel less sick.", "The poison on $p dries up.", "",
        { { CATALYST_BODY, 5 },{ CATALYST_TOXIN, 2 },{ CATALYST_BLOOD, 1 } }
    }, {
        "polearm",
        { 1, 1, 1, 1 }, { 6, 6, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Polearm!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "possess",
        { 31, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_null, TAR_IGNORE_CHAR_DEF, POS_STANDING,
        -1, 100, 12,
        "Possess", "!Possess!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "pursuit",
        { 31, 31, 31, 13}, { 10, 10, 10, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 12,
        "pursuit", "!Persue!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "quarterstaff",
        { 1, 1, 1, 1 }, { 3, 3, 3, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Quarterstaff!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "raise dead",
        { 28, 31, 31, 31 }, { 12, 12, 12, 12},
        spell_raise_dead, TAR_OBJ_GROUND, POS_STANDING,
        -1, 1000, 8,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "recharge",
        { 9, 31, 31, 23 }, { 6, 1, 2, 2 },
        spell_recharge, TAR_OBJ_INV, POS_STANDING,
        -1, 60, 6,
        "", "!Recharge!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "refresh",
        { 31, 6, 31, 5 }, { 2, 6, 2, 2},
        spell_refresh, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 12, 2,
        "refresh", "!Refresh!", "", "",
        { { CATALYST_BODY, 1 },{ CATALYST_NATURE, 1 },{ CATALYST_NONE, 0 } }
    }, {
        "regeneration",
        { 31, 18, 31, 31 }, { 6, 8, 6, 6},
        spell_regeneration, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 12,
        "regeneration", "You stop regenerating.", "", "",
        { { CATALYST_BODY, 5 },{ CATALYST_COSMIC, 5 },{ CATALYST_BLOOD, 5 } }
    }, {
        "remove curse",
        { 31, 18, 31, 18 }, { 3, 6, 2, 2},
        spell_remove_curse, TAR_OBJ_CHAR_DEF, POS_STANDING,
        -1, 5, 4,
        "", "!Remove Curse!", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_NATURE, 1 },{ CATALYST_LAW, 2 } }
    }, {
        "rending",
        { 2, 2, 2, 1 }, { 8, 8, 7, 6},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_LICH, 0, 0,
        "", "!rending!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "repair",
        { 11, 11, 11, 11 }, { 21, 15, 13, 11 },
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_DWARF, 0, 10,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "rescue",
        { 31, 31, 31, 3 }, { 20, 20, 20, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 12,
        "", "!Rescue!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "resurrect",
        { 1, 1, 1, 1 }, { 15, 15, 15, 15},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "resurrect", "!Resurrect!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "reverie",
        { 31, 15, 31, 15 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Reverie!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "riding",
        {31,31,31,5}, {20,20,20,12},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "room shield",
        { 15, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_room_shield, TAR_IGNORE, POS_STANDING,
        -1, 75, 6,
        "room shield", "!Room Shield!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sanctuary",
        { 20, 26, 31, 31 }, { 10, 10, 2, 2},
        spell_sanctuary, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 75, 8,
        "", "{WThe white aura around your body fades.{x", "", "The white aura around $n's body vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 1 },{ CATALYST_HOLY, 2 } }
    }, {
        "scan",
        { 31, 31, 8, 31 }, { 11, 11, 5, 8 },
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "scribe",
        { 31, 19, 31, 31 }, { 8, 8, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Scribe!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "scrolls",
        { 1, 1, 1, 1 }, { 2, 3, 5, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "", "!Scrolls!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "scry",
        {16, 16, 16, 16 }, { 15, 17, 19, 25},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_LICH, 0, 12,
        "", "!Scry!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "second attack",
        { 10, 10, 10, 8 }, { 10, 8, 5, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Second Attack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sense danger",
        {5, 4, 3, 3}, {8, 6, 5, 4},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_SITH, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shape",
        { 1, 1, 1, 1 }, { 2, 2, 2, 2},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_VAMPIRE, 0, 12,
        "", "!Shape!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shield",
        { 8, 31, 31, 8 }, { 2, 2, 2, 2},
        spell_shield, TAR_CHAR_DEFENSIVE, POS_STANDING,
        -1, 12, 4,
        "", "{WYour force shield shimmers then fades away.{x", "", "The shield protecting $n vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "shield and weapon style",
        { 31, 31, 31, 6 }, { 10, 9, 5, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Shield And Weapon Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shield block",
        { 7, 7, 7, 7 }, { 6, 4, 6, 2},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Shield!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shift",
        { 1, 1, 1, 1 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Shift!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shocking grasp",
        { 10, 31, 31, 31 }, { 2, 2, 2, 2},
        spell_shocking_grasp, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 15, 4,
        "shocking grasp", "!Shocking Grasp!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "shriek",
        { 7, 31, 31, 31 }, { 2, 2, 2, 2},
        spell_shriek, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 6,
        "deafening shriek", "!SHRIEK!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "silence",
        { 25, 31, 31, 31 }, { 8, 1, 2, 2},
        spell_silence, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 12,
        "", "Your throat clears.", "", "$n is no longer silenced.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "single weapon style",
        { 1, 1, 1, 1 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Single Weapon Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "skull",
        { 7, 7, 7, 7 }, { 2, 3, 5, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "!Skull!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sleep",
        { 16, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_sleep, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 15, 6,
        "", "You feel less tired.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "slit throat",
        { 31, 31, 19, 31 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "breath", "!Slit Throat!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "slow",
        { 23, 31, 31, 31 }, { 6, 6, 2, 2},
        spell_slow, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 30, 8,
        "", "You feel yourself speed up.", "", "$n is no longer moving so slowly.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "smite",
        {31, 31, 31, 15}, {22, 15, 23, 10},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "strike", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sneak",
        { 10, 10, 10, 10 }, { 6, 4, 4, 6},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "", "You no longer feel stealthy.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "soul essence",
        { 1, 1, 1, 1 }, { 12, 12, 12, 12},
        spell_soul_essence, TAR_IGNORE, POS_STANDING,
        -1, 500, 20,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "spear",
        { 1, 1, 1, 1 }, { 4, 4, 4, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Spear!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "spell deflection",
        { 15, 31, 31, 31 }, { 14, 1, 2, 2},
        spell_spell_deflection, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 50, 6,
        "spell_spell_deflection", "The dazzling crimson aura around you dissipates.", "", "The crimson aura around $n's body vanishes.",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_LAW, 5 } }
    }, {
        "spell shield",
        { 31, 31, 31, 20 }, { 4, 1, 2, 10},
        spell_spell_shield, TAR_CHAR_DEFENSIVE, POS_FIGHTING,
        -1, 50, 8,
        "spell_shield", "The spell shield around you fades away.", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_ENERGY, 2 } }
    }, {
        "spell trap",
        { 24, 31, 31, 31 }, { 8, 6, 2, 2},
        spell_spell_trap, TAR_IGNORE, POS_STANDING,
        -1, 200, 12,
        "spell_trap", "!Spell Trap!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "spirit rack",
        { 11, 11, 11, 11 }, { 7, 7, 7, 6},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_LICH, 0, 12,
        "spirit rack", "!spirit rack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "stake",
        { 1, 1, 1, 1 }, { 4, 4, 6, 3},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_SLAYER, 0, 9,
        "", "!Stake!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "starflare",
        {20, 31, 31, 31}, {9, 10, 15, 18},
        spell_starflare, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 150, 12,
        "starflare", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "staves",
        { 1, 1, 1, 1 }, { 2, 3, 5, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 10,
        "", "!Staves!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "steal",
        { 31, 31, 16, 31 }, { 20, 20, 4, 20},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 8,
        "", "!Steal!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "stone skin",
        { 4, 6, 31, 21}, { 4, 4, 2, 2},
        spell_stone_skin, TAR_CHAR_SELF, POS_STANDING,
        -1, 12, 4,
        "", "Your skin feels soft again.", "", "$n's skin regains its normal texture.",
        { { CATALYST_BODY, 2 },{ CATALYST_EARTH, 3 },{ CATALYST_NONE, 0 } }
    }, {
        "stone spikes",
        {16, 31, 31, 31}, {8, 12, 14, 15},
        spell_stone_spikes, TAR_IGNORE, POS_STANDING,
        0, 125, 10,
        "stone spikes", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "stone touch",
        { 4, 6, 31, 21}, { 6, 6, 4, 4},
        spell_stone_touch, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 150, 10,
        "", "Your skin feels soft again.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "subvert",
        { 31, 31, 26, 31 }, { 0, 0, 5, 0},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "subvert", "!Subvert!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "summon",
        { 15, 12, 31, 31 }, { 6, 6, 2, 2},
        spell_summon, TAR_IGNORE_CHAR_DEF, POS_STANDING,
        -1, 50, 6,
        "", "!Summon!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "survey",
        { 8, 8, 8, 8 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 24,
        "survey", "!Survey!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "swerve",
        { 1, 1, 1, 1 }, { 8, 8, 4, 6},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_DROW, 0, 0,
        "", "!Swerve!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sword",
        { 1, 1, 1, 1}, { 5, 6, 3, 2},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!sword!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "sword and dagger style",
        { 31, 31, 31, 6 }, { 10, 9, 5, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "deadly slice", "!Sword And Dagger Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "tail kick",
        { 1, 1, 1, 1 }, { 5, 5, 5, 5},
        spell_null, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        PC_RACE_SITH, 0, 12,
        "tail kick", "!Tail Kick!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "tattoo",
        { 31, 19, 31, 31 }, { 8, 8, 6, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Tattoo!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "temperance",
        {5,4,3,2}, {3,5,6,6},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_VAMPIRE, 0, 12,
        "","","","",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "third attack",
        { 31, 31, 31, 18 }, { 20, 20, 10, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Third Attack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "third eye",
        { 31, 16, 31, 31 }, { 6, 5, 2, 2},
        spell_third_eye, TAR_OBJ_INV, POS_STANDING,
        -1, 55, 6,
        "", "!Third Eye!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "throw",
        { 31, 12, 12, 31 }, { 0, 8, 8, 0},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 4,
        "throw", "!Throw!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "titanic attack",
        { 1, 1, 1, 1 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        PC_RACE_TITAN, 0, 0,
        "", "!Titanic Attack!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "toxic fumes",
        { 31, 31, 31, 31 }, { 31, 31, 31, 31},
        spell_toxic_fumes, TAR_CHAR_OFFENSIVE, POS_STANDING,
        -1, 70, 10,
        "toxic fumes", "The affects of the toxic fumes subside.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "toxins",
        { 13, 13, 13, 13 }, { 5, 5, 5, 5},
        spell_null, TAR_IGNORE, POS_STANDING,
        PC_RACE_SITH, 0, 12,
        "", "!Toxins!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "trackless step",
        {31, 7, 31, 31 }, {18, 10, 13, 11},
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "trample",
        {31,31,31,11}, {23,22,23,11},
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 8,
        "charge", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "turn undead",
        { 31, 20, 31, 31 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 12,
        "turn_undead", "!Turn Undead!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "two-handed weapon style",
        { 31, 31, 31, 11 }, { 10, 9, 5, 10},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Two Handed Weapon Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "underwater breathing",
        { 7, 12, 31, 31 }, { 7, 1, 2, 2},
        spell_underwater_breathing, TAR_CHAR_SELF, POS_STANDING,
        -1, 70, 10,
        "", "The gills behind your ears disappear.", "", "",
        { { CATALYST_BODY, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_WATER, 2 } }
    }, {
        "vision",
        { 31, 9, 31, 31 }, { 3, 8, 2, 2},
        spell_vision, TAR_IGNORE, POS_STANDING,
        -1, 50, 6,
        "", "", "", "",
        { { CATALYST_AIR, 2 },{ CATALYST_COSMIC, 2 },{ CATALYST_NONE, 0 } }
    }, {
        "wands",
        { 1, 1, 1, 1 }, { 2, 3, 5, 8},
        spell_null, TAR_IGNORE, POS_STANDING,
        -1, 0, 10,
        "", "!Wands!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "warcry",
        { 31, 31, 31, 18 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 10,
        "", "Your adrenaline thins out and your breathing returns to normal.", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "water spells",
        {1, 31, 31, 31}, {15, 20, 30, 30},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "weaken",
        { 13, 15, 31, 31 }, { 5, 5, 2, 2},
        spell_weaken, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 20, 6,
        "spell", "{YYou feel stronger.{x", "", "$n looks stronger.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "weaving",
        { 31, 31, 31, 15 }, { 8, 8, 8, 8},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 12,
        "", "!Weaving!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "web",
        { 24, 31, 31, 31 }, { 8, 1, 2, 2},
        spell_web, TAR_CHAR_OFFENSIVE, POS_FIGHTING,
        -1, 50, 12,
        "", "The webs holding you in place disappear.", "", "The webs around $n disappear.",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "whip",
        { 1, 1, 1, 1}, { 6, 5, 5, 4},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Whip!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "wilderness spear style",
        { 31, 18, 31, 31 }, { 5, 8, 5, 5},
        spell_null, TAR_IGNORE, POS_FIGHTING,
        -1, 0, 0,
        "", "!Wilderness Spear Style!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "wind of confusion",
        { 25, 31, 31, 31 }, { 14, 1, 2, 2},
        spell_wind_of_confusion, TAR_IGNORE, POS_FIGHTING,
        -1, 100, 12,
        "wind of confusion", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "withering cloud",
        { 14, 31, 31, 31 }, { 8, 6, 2, 2},
        spell_withering_cloud, TAR_IGNORE, POS_STANDING,
        -1, 50, 12,
        "withering cloud", "!Withering Cloud!", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "word of recall",
        { 12, 12, 12, 12 }, { 4, 4, 2, 2},
        spell_word_of_recall, TAR_CHAR_SELF, POS_RESTING,
        -1, 5, 8,
        "", "!Word of Recall!", "", "",
        { { CATALYST_LAW, 5 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }, {
        "none",
        {31, 31, 31, 31}, {100,100,100,100},
        spell_null, TAR_IGNORE, POS_STANDING,
        0, 0, 0,
        "", "", "", "",
        { { CATALYST_NONE, 0 },{ CATALYST_NONE, 0 },{ CATALYST_NONE, 0 } }
    }

};





/* A table to store NPC skill levels for the time being, so that we don't have to recalculate
   every time get_skill is called. Initialized in boot_db. (Syn) */

int mob_skill_table[MAX_MOB_SKILL_LEVEL];


const struct group_type group_table [MAX_GROUP] =
{
    /*
    {
    name,		rating for each class,
    skills/spells
    }
    */
    {
    "global skills",	{ 0, 0, 0, 0 },
    { "scrolls", "staves", "wands", "hand to hand", "single weapon style", "lore", NULL }
    },

    {
    "mage skills",		{ 0, -1, -1, -1 },
    { "dagger", "quarterstaff", "lightning bolt", "dispel magic",
      "shield", "stone skin", "fireball", "magic missile",
      "shocking grasp", "burning hands", "chain lightning", "invis",
      "armour", "control weather", "haste", "giant strength",
      "infravision", "colour spray", "earthquake", "charm person",
      "pass door", "fly", "invisibility", "inferno", NULL
    }
    },

    {
    "cleric skills",	{ -1, 0, -1, -1 },
    { "mace", "flail", "quarterstaff", "sanctuary", "create food",
      "cure light", "cure serious", "cure critical", "heal",
      "refresh", "cure blindness", "cure disease", "cure poison",
      "create water", "continual light", "create spring",
      "cause light", "cause serious", "cause critical", "harm",
      "control weather", "detect hidden", "flamestrike",
      "detect invis", "detect magic", "remove curse", "calm",
      "identify", "create rose", "word of recall", "meditation", NULL
    }
    },

    {
    "thief skills",		{ -1, -1, 0, -1 },
    { "steal", "dagger", "sword", "hide", "sneak", "peek", "acrobatics", "second attack",
      NULL}
    },

    {
    "warrior skills",	{ -1, -1, -1, 0 },
    { "sword", "axe", "dagger", "spear", "whip",
      "second attack", "bash", "dodge", "fast healing",
      "enhanced damage", "parry", "third attack", "kick",
      NULL}
    },

    /* Necromancer - death energies */
    {
    "necromancer skills",      { 4, 4, 8, 8 },
     { "feign", "animate dead", "raise dead",
      "energy drain", "kill", "chill touch", "deathsight",
      NULL },
    },

    /* Sorcerer - master of combat magic */
    {
    "sorcerer skills",      { 4, 4, 8, 8 },
     { "web", "silence", "sleep", "death grip", "slow",
      "channel", "summon", "weaken", "faerie fire",
      "blindness", "cancellation", "acid blast", NULL }
    },

    /* Wizard - guardian of knowledge, truth and magical energies */
    {
    "wizard skills",      { 4, 4, 8, 8 },
     { "electrical barrier", "light shroud", "entrap",
      "sanctuary", "locate object", "fireproof", "lore",
      "blindness", "healing hands", "sanctuary", "improved invisibility",
      "acid blast", "room shield", "starflare", "dispel room",
      "gate", "nexus", "summon", "mass invis", "afterburn", NULL },
    },

    /* Witch/Warlock - voodoo witchcraft doctor */
    {
    "witch skills",      { 4, 4, 8, 8 },
     { "skull", "counterspell", "third eye", "dispel good",
      "poison", "curse", "plague", "spear", "demonfire",
       "fly", "blindness", "mass healing", NULL },
    },

    /* Druid - in tune with nature */
    {
    "druid skills",      { 4, 4, 8, 8 },
     { "dispel evil", "dispel good", "brew", "cosmic blast",
          "enchant armour", "enchant weapon", "underwater breathing",
      "fireproof", "summon", "scribe", "lore",
      "earthquake", "call lightning",
      "bless", "archery", "bow", NULL},
    },

    /* Monk - master of body control and hand to hand */
    {
    "monk skills",      { 4, 4, 8, 8 },
     { "counterspell", "dodge", "dispel evil", "frenzy",
      "fireproof", "catch", "bless", "stone skin",
      "armour", "dirt kicking", "kick",
      "lore", "mass healing",
      "holy word", "reverie", "martial arts", "deep trance", NULL },
    },

    /* Assassin - hired killers */
    {
    "assassin skills",      { 4, 4, 8, 8 },
     { "blackjack", "enhanced damage", "slit throat",
      "envenom", "backstab", "hunt", "crossbow", NULL },
    },

    /* Rogue - burglar and pickpocket */
    {
    "rogue skills",      { 4, 4, 8, 8 },
     { "circle", "pick lock", "backstab", "detect traps",
      "blackjack", "hunt", "haggle", "scan", NULL },
    },

    /* Bard - travelling musician, has magical powers */
    {
    "bard skills",      { 4, 4, 8, 8 },
     { "music", "lore", "haggle", NULL },
    },

    /* Marauder - brute strength and ferocity, vicious warrior */
    {
    "marauder skills",      { 4, 4, 8, 8 },
     { "fourth attack", "dirt kicking",
      "two-handed weapon style", "intimidate",
      "polearm", NULL },
    },

    /* Gladiator - master of combat skills */
    {
    "gladiator skills",      { 4, 4, 8, 8 },
     { "circle", "dual wield",
      "rescue", "disarm", "two-handed weapon style",
      "shield and weapon style",
      "exotic", "polearm", "shield block", "mace", NULL },
    },

    /* Paladin - holy warrior with some spells */
    {
    "paladin skills",      { 4, 4, 8, 8 },
     { "refresh", "cure critical", "rescue", "dual wield",
      "disarm", "armour", "bless",
      "shield and weapon style", "remove curse",
      "cure disease", "cure poison", "shield block",
      "stone skin", "cure blindness",
      "polearm", "cure serious", "cure light", "cause light",
      "cause serious", "continual light",
      "mount and weapon style", "smite", NULL },
    },

    /* Remort Classes */

    /* Archmage - master of mana and inner energy */
    {
        "archmage skills", { 4, 4, 8, 8 },
    { "ice storm", "fire cloud", "discharge",
      "eagle eye", "spell trap", "withering cloud",
      "maze", "destruction", NULL },
    },

    /* Geomancer - elemental skills and affects. */
    {
    "geomancer skills", { 4, 4, 8, 8 },
    { "electrical barrier", "frost barrier", "fire barrier",
      "master weather", "ensnare", "stone spikes", /* "earth spells", "fire spells",
      "water spells", "air spells",*/ NULL },
    },

    /* Illusionist - master of illusion */
    {
    "illusionist skills", { 4, 4, 8, 8 },
    { "momentary darkness", "spell deflection", "wind of confusion", NULL },
    },

    /* Adept - ultra holy cleric fighter */
    {
        "adept skills", { 4, 4, 8, 8 },
        { "avatar shield", "glorious bolt", "light shroud",
      "exorcism", "turn undead", NULL },
    },

    /* Alchemist - master of materials, that can involve magic */
    {
        "alchemist skills", { 4, 4, 8, 8 },
    { "brew", "scribe", "throw", "recharge", "combine", NULL },
    },

    /* Ranger - super druid */
    {
        "ranger skills", { 4, 4, 8, 8 },
    { "survey", "vision", "call familiar", "wilderness spear style",
      "shield and weapon style", "infuse", "trackless step", "archery",
      "bow", "crossbow", "hunt", NULL },
    },

    /* Highwayman - master of thieving */
    {
        "highwayman skills", { 4, 4, 8, 8 },
        { "holdup", "bar", "pick lock", "backstab", "blackjack", "ambush", NULL },
    },

    /* Ninja - ultra secretive super fighter */
    {
        "ninja skills", { 4, 4, 8, 8 },
        { "throw", "bomb", "evasion", "cloak of guile", "deathbarbs",
      "sword and dagger style", "dual wield", "slit throat", NULL },
    },

    /* Sage - master of knowledge in all forms */
    {
        "sage skills", { 4, 4, 8, 8 },
        { "judge", "mob lore", "deception", NULL },
    },

    /* Warlord - overlord, leader, etc */
    {
        "warlord skills", { 4, 4, 8, 8 },
        { "leadership", "bind", "pursuit", "weaving", "healing aura",
      "energy field", "spell shield", "dual wield", "shield block",
      "shield and weapon style", "two-handed weapon style", NULL  },
    },

    /* Destroyer - ultra-vicious fighter */
    {
        "destroyer skills", { 4, 4, 8, 8 },
        { "warcry", "behead", "fourth attack", "athletics",
      "two-handed weapon style", NULL },
    },

    /* Crusader - travelling warrior */
    {
        "crusader skills", { 4, 4, 8, 8 },
        { "holy sword", "holy shield", "riding", "trample", "smite",
      "mount and weapon style", "shield and weapon style", NULL }
    },

    {
    NULL,	{ 0, 0, 0, 0 },
    { }
    }
};


const struct script_type script_type_table[] =
{
/* Script Types and their commands */
    { PRG_MPROG, "MobProg", "mp"},
    { PRG_OPROG, "ObjProg", "op"},
    { PRG_RPROG, "RoomProg", "rp"},
    { PRG_TPROG, "TokenProg", "tp"},
    { PRG_APROG, "AreaProg", "ap"},
    { PRG_IPROG, "InstanceProg", "ip"},
    { PRG_DPROG, "DungeonProg", "dp"},
    { PRG_QPROG, "QuestProg", "qp"},
    { PRG_EPROG, "EventProg", "ep"},
    { -1, NULL, NULL }
};

/* MSP sounds (for the future)*/
const struct sound_type sound_table[] =
{
/*	 sound,			tag*/
    {    SOUND_HIT_1,       "!!SOUND(combat/hit1.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_HIT_2,       "!!SOUND(combat/hit2.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_HIT_3,       "!!SOUND(combat/hit3.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_MISS_0,       "!!SOUND(combat/miss0.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_MISS_1,       "!!SOUND(combat/miss1.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_PARRY_0,       "!!SOUND(combat/parry.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_BREATH,       "!!SOUND(combat/breath.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_PUNCH_1,       "!!SOUND(combat/punch1.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_PUNCH_2,       "!!SOUND(combat/punch2.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },
    {    SOUND_PUNCH_3,       "!!SOUND(combat/punch3.wav V=100 L=1 P=100 T=COMBAT U=http://sentience.megacosm.net)" },

    {    SOUND_CITY,       "!!SOUND(amb/city.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_BIRD_1,       " !!SOUND(amb/bird_0.wav V=90 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_NIGHT,       "!!SOUND(amb/night.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_RAIN,       "!!SOUND(amb/rain.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_STORM,       "!!SOUND(amb/storm.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_LIGHTNING,       "!!SOUND(amb/lightning.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },

    {    SOUND_WIND,       "!!SOUND(amb/wind.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_UNDERWATER,       " !!SOUND(amb/underwater.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_WOLVES,       "!!SOUND(amb/howl.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_ROOSTER,       "!!SOUND(amb/rooster.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_FIRE,       "!!SOUND(amb/fire.wav V=80 L=-1 P=10 T=AMBIENT U=http://sentience.megacosm.net)" },

    {    SOUND_SKULL,       "!!SOUND(misc/skull.wav V=100 L=1 P=90 T=MISC U=http://sentience.megacosm.net)" },
    {    SOUND_SPLASH,       "!!SOUND(misc/splash.wav V=100 L=1 P=90 T=MISC U=http://sentience.megacosm.net)" },

    {    SOUND_SINK,       "!!SOUND(ship/sink.wav V=100 L=1 P=90 T=SHIP U=http://sentience.megacosm.net)" },
    {    SOUND_CANNON,       "!!SOUND(ship/cannon.wav V=100 L=1 P=90 T=SHIP U=http://sentience.megacosm.net)" },
    {    SOUND_CANNON_SPLASH,       "!!SOUND(ship/ship_splash.wav V=100 L=1 P=90 T=SHIP U=http://sentience.megacosm.net)" },
    {    SOUND_CANNON_EXPLODE,       "!!SOUND(ship/explode.wav V=100 L=1 P=90 T=SHIP U=http://sentience.megacosm.net)" },

    {    SOUND_MALE,       "!!SOUND(death/male.wav V=100 L=1 P=90 T=DEATH U=http://sentience.megacosm.net)" },
    {    SOUND_FEMALE,       "!!SOUND(death/female.wav V=100 L=1 P=90 T=DEATH U=http://sentience.megacosm.net)" },

    {    SOUND_TELEPORT,       "!!SOUND(misc/female.wav V=100 L=1 P=90 T=MISC U=http://sentience.megacosm.net)" },
    {    SOUND_BIRD_2,       "!!SOUND(amb/bird2.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_BIRD_3,       "!!SOUND(amb/bird3.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_OWL,       "!!SOUND(amb/owl.wav V=100 L=1 P=90 T=AMBIENT U=http://sentience.megacosm.net)" },
    {    SOUND_OFF,       "!!SOUND(off)" }
};


/* Sith toxin types*/
const struct toxin_type toxin_table[MAX_TOXIN] =
{
    {	"paralysis", 	{10,15},	25,	spell_toxin_paralysis,	false	},
    {	"weakness",	{10,15},	45,	spell_toxin_weakness,	false	},
    {	"neurotoxin",	{10,15},	45,	spell_toxin_neurotoxin,	false	},
    {	"venom",	{10,15},	30,	spell_toxin_venom,	false	},
};


const struct herb_type herb_table[MAX_HERB] =
{
    { "none",		SECT_INSIDE,			{0,  0,  0 },
      0,		0,				0,		spell_null	},

    { "hylaxis",	SECT_FOREST,			{20, 20, 20},
      0,		RES_POISON,			0,		spell_null	},

    { "rhotail",	SECT_FIELD,			{50, 0, 75},
      0,		RES_PIERCE | RES_BASH,		0,		spell_null	},

    { "varghrol",	SECT_DESERT,			{25, 100, 25},
      IMM_FIRE,		RES_ACID | RES_COLD,		0,		spell_null	},

    { "guamal",		SECT_HILLS,			{50, 25, 50},
      0,		RES_WEAPON,			0,		spell_null	},

    { "satrix",		SECT_TUNDRA,			{75, 75, 75},
      0,		RES_MAGIC,			0,		spell_null	},

    { "falsz",		SECT_NETHERWORLD,		{100,100,100},
      0,		RES_HOLY | RES_LIGHT,		0,		spell_null	}
};


struct boost_type boost_table[] =
{
    { "experience",	"{GEXPERIENCE",			100,		0		},
    { "damage",		"{RDAMAGE",			100,		0		},
    { "qp",		"{MQUEST POINTS",		100,		0		},
    { "pneuma",		"{CPNEUMA",			100,		0		},
    { "reckoning",	"{rRECKONING",		100,		0		},

    { NULL,		NULL,				-1,		-1		}
};
