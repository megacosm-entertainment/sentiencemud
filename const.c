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
    {	"links",	0,		0,		COMM_LINKS,	false,	STAFF_PLAYER,	SETTING_ON	},
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
    {   "restrings",    0,  PLR_SHOW_RESTRINGS,0,   false, STAFF_PLAYER,    SETTING_OFF},
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

/* A table to store NPC skill levels for the time being, so that we don't have to recalculate
   every time get_skill is called. Initialized in boot_db. (Syn) */

int mob_skill_table[MAX_MOB_SKILL_LEVEL];


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
