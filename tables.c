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
#include "tables.h"
#include "scripts.h"
#include "magic.h"

const struct hint_type hintsTable[] =
{
    {"The map shows you a small map of the surrounding area. To turn it off type 'map'.\n\r"},
    {"Players with an {W[H]{M by their name are designated helpers. Feel free to ask them for help or advice.\n\r"},
    {"If you need any help try asking over the 'helper' channel to page a helper or immortal.\n\r"},
    {"If you are lost you can type 'recall' at any time to return to a familiar place.\n\r"},
    {"You can recover mana or hit points by sleeping or resting. To do this, type 'sleep' or 'rest'. Type 'wake' or 'stand' once you have finished.\n\r"},
    {"You can type \"hints\" at any time to remove these messages.\n\r"},
    {"When you feel able, the mayor in Olaria offers quests. Quests give\n\ryour money, experience, pracs and quest points that can be used to buy magical\n\requipment.\n\r"},
    {"You can turn off channels by typing the name of the channel without anything\n\rafter. You can find the names of all channels by typing 'channels'.\n\r"},
    {"Type 'help auction' for information on how to use the auction channel.\n\r"},
//    {"The goblin airship is located in the Plith town square. It can take you to all other towns and cities for a price.\n\r"},
    {"There are 120 levels in Sentience. Every 30 levels you must multiclass, keeping your previous class skills.\n\r"},
    {"If you die after level 10 and enter the bottomless pit, you will be transferred to a magical maze in the plane of the dead. If you can manage to find Geldoff the Warlock, you can say \"back alive\" to be instantly resurrected for 15,000 deity points.\n\r"},
    {"Type \"score\" to see information about your character.\n\r"},
    {"Churches are groups of players bound by religion and belief in a common cause. To see a list of \n\rthe churches type \"church list\". To get more information about churches type \"help churches\".\n\r"},
    {"To handle more than one object at a time you can use #.<keyword>. For example, if you have two items with the\n\rkeyword sword in your inventory, you can look at the second one by typing \"look 2.sword\"."},
    {"If you happen to die while you're still a new player, you will be revived instantly. However, make sure to re-equip your equipment after you come back to life."},
    {"To toggle various settings on your character, type \"toggle <setting>\". To see a list of your settings type \"toggle\"."},
    {"To increase your character's permanent attributes, such as strength and hit points, train them at a trainer (train <attribute>). To improve your skills, practice them at a trainer (practice <skill>). Both will greatly improve your combat skills."}

};



const struct wepHitDice wepHitDiceTable[] =
{
    {1,8},
    {2,5},
    {2,6},
    {4,4},
    {4,5},
    {5,4},
    {5,5},
    {5,6},
    {5,7},
    {5,8},
    {5,9},
    {5,10},
    {5,11},
    {6,9},
    {6,10},
    {6,12},
    {6,13},
    {6,14},
    {6,15},
    {6,16},
    {6,17},
    {6,18},
    {6,19},
    {6,20},
    {6,21},
    {6,22},
    {6,23},
    {6,24},
    {6,25},
    {6,26},
    {6,27},
    {6,28},
    {7,25},
    {6,30},
    {6,31},
    {6,32},
    {6,33},
    {7,30},
    {8,26},
    {7,31},
    {6,37},
    {8,28},
    {8,29},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
    {8,33},
};


const struct damDiceType damDiceTypeTable[] =
{
    {2},
    {2},
    {3},
    {4},
    {3},
    {3},
    {4},
    {4},
    {5},
    {5},
    {4},
    {4},
    {5},
    {5},
    {5},
    {5},
    {4},
    {5},
    {5},
    {5},
    {5},
    {6},
    {6},
    {6},
    {6},
    {6},
    {6},
    {7},
    {7},
    {6},
    {6},
    {6},
    {6},
    {7},
    {7},
    {7},
    {7},
    {8},
    {8},
    {8},
    {8},
    {8},
    {9},
    {9},
    {9},
    {9},
    {10},
    {10},
    {10},
    {10},
    {10},
    {10},
    {11},
    {11},
    {11},
    {11},
    {12},
    {12},
    {12},
    {12},
    {12},
    {12},
    {13},
    {13},
    {13},
    {13},
    {14},
    {14},
    {14},
    {14},
    {14},
    {14},
    {15},
    {15},
    {15},
    {15},
    {15},
    {16},
    {16},
    {16},
    {16},
    {16},
    {17},
    {17},
    {17},
    {17},
    {18},
    {18},
    {18},
    {18},
    {18},
    {18},
    {19},
    {19},
    {19},
    {19},
    {20},
    {20},
    {20},
    {20},
    {20},
    {20},
    {21},
    {21},
    {21},
    {18},
    {18},
    {18},
    {18},
    {18},
    {19},
    {19},
    {19},
    {19},
    {19},
    {19},
    {20},
    {20},
    {20},
    {20},
    {20},
    {20},
    {21},
    {21},
    {21},
    {21},
    {21},
    {21},
    {22},
    {22},
    {22},
    {22},
    {22},
    {22},
    {23},
    {23},
    {23},
    {23},
    {23},
    {23},
    {24},
    {24},
    {24},
    {24},
    {24},
    {24},
    {25},
    {25},
    {25},
    {25},
    {26},
    {26},
    {26},
    {26},
    {26},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {30},
    {31},
    {31},
    {31},
    {31},
    {31},
    {31},
    {31},
    {31},
    {31},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {27},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {28},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29},
    {29}
};


// Church ranks
const struct church_band_rank_type church_band_rank_table[] =
{
       { "Initiate", 	"Initiate" 	}, //0
       { "Member", 	"Member" 	}, //1
       { "Advisor", 	"Advisor" 	}, //2
       { "Leader", 	"Leader" 	}  //3
};


const struct church_cult_rank_type church_cult_rank_table[] =
{
       { "Follower", 	"Follower" 	}, //4
       { "Brethren", 	"Brethren" 	}, //5
       { "Disciple", 	"Disciple" 	}, //6
       { "Chieftan", 	"Enchantress"	}  //7
};


const struct church_order_rank_type church_order_rank_table[] =
{
       { "Knave", 	"Maiden" 	}, //8
       { "Squire", 	"Shieldmaiden"	}, //9
       { "Knight", 	"Knight"	}, //10
       { "Warmaster", 	"Warmistress" 	}  //11
};

const struct church_church_rank_type church_church_rank_table[] =
{
       { "Chaplain", 	"Chaplain" 	}, //12
       { "Priest", 	"Priestess" 	}, //13
       { "Bishop", 	"Bishop" 	}, //14
       { "Cardinal", 	"Cardinal" 	}  //15
};


const struct talk_type vampire_talk_table[] =
{
	{"th", "z"},
	{"wh", "v"}
};


const struct court_rank_type court_rank_table[] =
{
	{ "Squire", 	"Maid" 		},
	{ "Earl", 	"Dame"   	},
	{ "Count", 	"Countess" 	},
	{ "Baron", 	"Baroness" 	},
	{ "Duke", 	"Duchess" 	},
	{ "Lord", 	"Lady" 		},
	{ "Prince", 	"Princess" 	}
};


const struct string_type object_damage_table[] =
{
	{ "" 				},
	{ "(Used)" 			},
	{ "{Y(Slightly Worn){x" 	},
	{ "{Y(Worn){x" 			},
	{ "{Y(Badly Worn){x" 		},
	{ "{y(Slightly Damaged){x" 	},
	{ "{y(Damaged){x" 		},
	{ "{r(Badly Damaged){x" 	},
	{ "{D(Falling Apart){x" 	},
	{ "{D(Crumbling){x" 		}
};


const struct position_type position_table[] =
{
    {	"dead",			"dead"	},
    {	"mortally wounded",	"mort"	},
    {	"incapacitated",	"incap"	},
    {	"stunned",		"stun"	},
    {	"sleeping",		"sleep"	},
    {	"resting",		"rest"	},
    {   "sitting",		"sit"   },
    {	"fighting",		"fight"	},
    {	"standing",		"stand"	},
    // @@@NIB : 20070126 : realized it was missing from this table!
    {   "feigned",		"feign"	},
    {   "heldup",		"heldup"},
    {	NULL,			NULL	}
};


const struct flag_type sex_table[] =
{
   {	"none",     SEX_NEUTRAL,    TRUE },
   {	"male",		SEX_MALE,       TRUE },
   {	"female",	SEX_FEMALE,     TRUE },
   {	"either",	SEX_EITHER,     TRUE },
   {	NULL,		0,              FALSE }
};

const struct size_type size_table[] =
{
    {	"tiny"		},
    {	"small" 	},
    {	"medium"	},
    {	"large"		},
    {	"huge", 	},
    {	"giant" 	},
    {	NULL		}
};

// Flags
const struct flag_type act_flags[] =
{
    {	"npc",					ACT_IS_NPC,				FALSE	},
    {	"sentinel",				ACT_SENTINEL,			TRUE	},
    {	"scavenger",			ACT_SCAVENGER,			TRUE	},
    {	"protected",			ACT_PROTECTED,			TRUE	},
    {	"mount",				ACT_MOUNT,				TRUE	},
    {	"aggressive",			ACT_AGGRESSIVE,			TRUE	},
    {	"stay_area",			ACT_STAY_AREA,			TRUE	},
    {	"wimpy",				ACT_WIMPY,				TRUE	},
    {	"pet",					ACT_PET,				TRUE	},
    {	"train",				ACT_TRAIN,				TRUE	},
    {	"practice",				ACT_PRACTICE,			TRUE	},
    {	"blacksmith",			ACT_BLACKSMITH,			TRUE	},
    {	"crew_seller",			ACT_CREW_SELLER,		TRUE	},
    {   "no_lore",				ACT_NO_LORE,			TRUE	},
    {	"undead",				ACT_UNDEAD,				TRUE	},
// (P)
    {	"cleric",				ACT_CLERIC,				TRUE	},
    {	"mage",					ACT_MAGE,				TRUE	},
    {	"thief",				ACT_THIEF,				TRUE	},
    {	"warrior",				ACT_WARRIOR,			TRUE	},
    {	"animated",				ACT_ANIMATED,			FALSE	},
    {	"nopurge",				ACT_NOPURGE,			TRUE	},
    {	"outdoors",				ACT_OUTDOORS,			TRUE	},
    {   "restringer",			ACT_IS_RESTRINGER,		TRUE	},
    {	"indoors",				ACT_INDOORS,			TRUE	},
    {	"questor",				ACT_QUESTOR,			TRUE	},
    {	"healer",				ACT_IS_HEALER,			TRUE	},
    {	"stay_locale",			ACT_STAY_LOCALE,		TRUE	},
    {	"update_always",		ACT_UPDATE_ALWAYS,		TRUE	},
    {	"changer",				ACT_IS_CHANGER,			TRUE	},
    {   "banker",				ACT_IS_BANKER,			TRUE    },
    {	NULL,					0,	FALSE	}
};


const struct flag_type act2_flags[]=
{
    {   "churchmaster",			ACT2_CHURCHMASTER,		TRUE	},
    {   "noquest",				ACT2_NOQUEST,			TRUE    },
    {   "stay_region",  		ACT2_STAY_REGION,   	TRUE    },
    {	"no_hunt",				ACT2_NO_HUNT,			TRUE	},
    {   "airship_seller",		ACT2_AIRSHIP_SELLER,	TRUE    },
    {   "wizi_mob",				ACT2_WIZI_MOB,			TRUE    },
    {   "trader",				ACT2_TRADER,			TRUE    },
    {   "loremaster",			ACT2_LOREMASTER,		TRUE	},
    {   "no_resurrect",			ACT2_NO_RESURRECT,		TRUE    },
    {   "drop_eq",				ACT2_DROP_EQ,			TRUE	},
    {   "gq_master",			ACT2_GQ_MASTER,			TRUE    },
    {   "wilds_wanderer",		ACT2_WILDS_WANDERER,	TRUE	},
    {   "ship_quest_master",	ACT2_SHIP_QUESTMASTER,	TRUE	},
    {   "reset_once",			ACT2_RESET_ONCE,		TRUE	},
    {	"see_all",				ACT2_SEE_ALL,			TRUE	},
    {   "no_chase",				ACT2_NO_CHASE,			TRUE	},
    {	"takes_skulls",			ACT2_TAKES_SKULLS,		TRUE	},
    {	"pirate",				ACT2_PIRATE,			TRUE	},
    {	"player_hunter",		ACT2_PLAYER_HUNTER,		TRUE	},
    {	"invasion_leader",		ACT2_INVASION_LEADER,	TRUE	},
    {	"invasion_mob",			ACT2_INVASION_MOB,		TRUE	},
    {   "see_wizi",				ACT2_SEE_WIZI,			TRUE	},
    {   "soul_deposit",			ACT2_SOUL_DEPOSIT,		TRUE	},
    {   "use_skills_only",		ACT2_USE_SKILLS_ONLY,	TRUE	},
    {   "can_level",			ACT2_CANLEVEL,			TRUE	},
    {   "no_xp",				ACT2_NO_XP,				TRUE	},
    {   "hired",				ACT2_HIRED,				FALSE	},
    {   "renewer",				ACT2_RENEWER,			TRUE	},
	{	"show_in_wilds",		ACT2_SHOW_IN_WILDS,		TRUE	},
    {   "instance_mob",			ACT2_INSTANCE_MOB,		FALSE	},
    {   NULL,					0,						FALSE	}
};

const struct flag_type *act_flagbank[] =
{
    act_flags,
    act2_flags,
    NULL
};


const struct string_type fragile_table[] =
{
    {   "Normal"  	},
    {   "Solid"    	},
    {   "Weak"  	},
    {   "Strong" 	}
};


const struct flag_type armour_strength_table[] =
{
    {	"None",		OBJ_ARMOUR_NOSTRENGTH,	TRUE   	},
    {   "Light",	OBJ_ARMOUR_LIGHT,   TRUE  	},
    {   "Medium",	OBJ_ARMOUR_MEDIUM,   TRUE    	},
    {   "Strong",	OBJ_ARMOUR_STRONG,   TRUE  	},
    {   "Heavy",	OBJ_ARMOUR_HEAVY,   TRUE 	},
    {	NULL, 0, FALSE }
};


const struct flag_type plr_flags[] =
{
    {	"npc",			PLR_IS_NPC,		FALSE	},
    {	"excommunicated",	PLR_EXCOMMUNICATED,	FALSE	},
    {	"pk",			PLR_PK,			FALSE	},
    {	"autoexit",		PLR_AUTOEXIT,		FALSE	},
    {	"autoloot",		PLR_AUTOLOOT,		FALSE	},
    {	"autosac",		PLR_AUTOSAC,		FALSE	},
    {	"autogold",		PLR_AUTOGOLD,		FALSE	},
    {   "autoolc",      PLR_AUTOOLC,        FALSE   },
    {	"autosplit",		PLR_AUTOSPLIT,		FALSE	},
    {   "autosetname",		PLR_AUTOSETNAME,	FALSE   },
    {	"holylight",		PLR_HOLYLIGHT,		FALSE	},
    {	"autoeq",		PLR_AUTOEQ,		FALSE	},
    {	"nosummon",		PLR_NOSUMMON,		FALSE	},
    {	"nofollow",		PLR_NOFOLLOW,		FALSE	},
    {	"colour",		PLR_COLOUR,		FALSE	},
    {	"notify",		PLR_NOTIFY,		TRUE	},
    {	"log",			PLR_LOG,		FALSE	},
    {	"deny",			PLR_DENY,		FALSE	},
    {	"freeze",		PLR_FREEZE,		FALSE	},
    {	"helper",		PLR_HELPER,		FALSE	},
    {   "botter",		PLR_BOTTER,		FALSE   },
    {	"showdamage",		PLR_SHOWDAMAGE,		FALSE	},
    {	"no_challenge",		PLR_NO_CHALLENGE,	FALSE	},
    {	"no_resurrect",		PLR_NO_RESURRECT,	FALSE	},
    {	"pursuit",		PLR_PURSUIT,		FALSE	},
    {	NULL,			0,	0			}
};

const struct flag_type plr2_flags[] =
{
    {	"autosurvey",		PLR_AUTOSURVEY,		FALSE	},
    {	"sacrifice_all",		PLR_SACRIFICE_ALL,		FALSE	},
    {	"no_wake",		PLR_NO_WAKE,		FALSE	},
    {	"holyaura",		PLR_HOLYAURA,		FALSE	},
    {	"mobile",		PLR_MOBILE,		FALSE	},
    {	"favskills",	PLR_FAVSKILLS,	FALSE	},
    {   "holywarp",     PLR_HOLYWARP,   FALSE   },
    {   "no_reckoning",     PLR_NORECKONING,   FALSE   },
    {   "no_lore",     PLR_NOLORE,   FALSE   },
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
    {	"blind",		AFF_BLIND,	TRUE	},
    {	"invisible",		AFF_INVISIBLE,	TRUE	},
    {	"detect_evil",		AFF_DETECT_EVIL,	TRUE	},
    {	"detect_invis",		AFF_DETECT_INVIS,	TRUE	},
    {	"detect_magic",		AFF_DETECT_MAGIC,	TRUE	},
    {	"detect_hidden",	AFF_DETECT_HIDDEN,	TRUE	},
    {	"detect_good",		AFF_DETECT_GOOD,	TRUE	},
    {	"sanctuary",		AFF_SANCTUARY,	TRUE	},
    {	"faerie_fire",		AFF_FAERIE_FIRE,	TRUE	},
    {	"infrared",		AFF_INFRARED,	TRUE	},
    {	"curse",		AFF_CURSE,	TRUE	},
    {   "death_grip",		AFF_DEATH_GRIP,	TRUE    },
    {	"poison",		AFF_POISON,	TRUE	},
    {	"sneak",		AFF_SNEAK,	TRUE	},
    {	"hide",			AFF_HIDE,	TRUE	},
    {	"sleep",		AFF_SLEEP,	TRUE	},
    {	"charm",		AFF_CHARM,	TRUE	},
    {	"flying",		AFF_FLYING,	TRUE	},
    {	"pass_door",		AFF_PASS_DOOR,	TRUE	},
    {	"haste",		AFF_HASTE,	TRUE	},
    {	"calm",			AFF_CALM,	TRUE	},
    {	"plague",		AFF_PLAGUE,	TRUE	},
    {	"weaken",		AFF_WEAKEN,	TRUE	},
    {	"frenzy",		AFF_FRENZY,	TRUE	},
    {	"berserk",		AFF_BERSERK,	TRUE	},
    {	"swim",			AFF_SWIM,	TRUE	},
    {	"regeneration",		AFF_REGENERATION,	TRUE	},
    {	"slow",			AFF_SLOW,	TRUE	},
    {   "web",                  AFF_WEB,     TRUE    },
    {	NULL,			0,	0	}
};


const struct flag_type affect2_flags[] =
{
    {   "silence",		        AFF2_SILENCE,     	TRUE    },
    {   "evasion",		        AFF2_EVASION,	TRUE	},
    {   "cloak_guile",  	    AFF2_CLOAK_OF_GUILE,	TRUE	},
    {   "warcry",		        AFF2_WARCRY,	TRUE    },
    {   "light_shroud", 	    AFF2_LIGHT_SHROUD,	TRUE	},
    {   "healing_aura", 	    AFF2_HEALING_AURA,	TRUE	},
    {   "energy_field", 	    AFF2_ENERGY_FIELD,	TRUE	},
    {   "spell_shield", 	    AFF2_SPELL_SHIELD,	TRUE	},
    {   "spell_deflection", 	AFF2_SPELL_DEFLECTION,  	TRUE    },
    {   "avatar_shield",	    AFF2_AVATAR_SHIELD,	TRUE    },
    {   "fatigue",		        AFF2_FATIGUE,	TRUE	},
    {   "paralysis",	    	AFF2_PARALYSIS,	TRUE	},
    {   "neurotoxin",	    	AFF2_NEUROTOXIN,	TRUE	},
    {	"toxin",	        	AFF2_TOXIN,	TRUE	},
    {   "electrical_barrier",	AFF2_ELECTRICAL_BARRIER,	TRUE	},
    {	"fire_barrier",	    	AFF2_FIRE_BARRIER,	TRUE	},
    {	"frost_barrier",    	AFF2_FROST_BARRIER,	TRUE	},
    {	"improved_invis",   	AFF2_IMPROVED_INVIS,	TRUE	},
    {	"ensnare",      		AFF2_ENSNARE,	TRUE	},
    {   "see_cloak",    		AFF2_SEE_CLOAK,	TRUE    },
    {	"stone_skin",   		AFF2_STONE_SKIN,	TRUE	},
    {	"morphlock",    		AFF2_MORPHLOCK,		TRUE	},
    {	"deathsight",   		AFF2_DEATHSIGHT,	TRUE	},
    {	"immobile",     		AFF2_IMMOBILE,	TRUE	},
    {	"protected",    		AFF2_PROTECTED,	TRUE	},
    {   "aggressive",           AFF2_AGGRESSIVE,    TRUE    },
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
    {	"area_attack",		A,	TRUE	},
    {	"backstab",		B,	TRUE	},
    {	"bash",			C,	TRUE	},
    {	"berserk",		D,	TRUE	},
    {	"disarm",		E,	TRUE	},
    {	"dodge",		F,	TRUE	},
    {	"fade",			G,	TRUE	},
    {	"kick",			I,	TRUE	},
    {	"dirt_kick",		J,	TRUE	},
    {	"parry",		K,	TRUE	},
    {	"rescue",		L,	TRUE	},
    {	"tail",			M,	TRUE	},
    {	"trip",			N,	TRUE	},
    {	"crush",		O,	TRUE	},
    {	"assist_all",		P,	TRUE	},
    {	"assist_align",		Q,	TRUE	},
    {	"assist_race",		R,	TRUE	},
    {	"assist_players",	S,	TRUE	},
    {	"assist_npc",		Z,	TRUE	},
    {	"assist_guard",		T,	TRUE	},
    {	"assist_vnum",		U,	TRUE	},
    {   "magic", 		Y,	TRUE	},
    {	NULL,			0,	0	}
};


const struct flag_type imm_flags[] =
{
    {	"summon",	IMM_SUMMON,		TRUE	},
    {   "charm",        IMM_CHARM,            	TRUE    },
    {   "magic",        IMM_MAGIC,            	TRUE    },
    {   "weapon",       IMM_WEAPON,           	TRUE    },
    {   "bash",         IMM_BASH,             	TRUE    },
    {   "pierce",       IMM_PIERCE,           	TRUE    },
    {   "slash",        IMM_SLASH,            	TRUE    },
    {   "fire",         IMM_FIRE,             	TRUE    },
    {   "cold",         IMM_COLD,             	TRUE    },
    {   "light",     	IMM_LIGHT,            	TRUE    },
    {   "lightning",    IMM_LIGHTNING,        	TRUE    },
    {   "acid",         IMM_ACID,             	TRUE    },
    {   "poison",       IMM_POISON,           	TRUE    },
    {   "negative",     IMM_NEGATIVE,         	TRUE    },
    {   "holy",         IMM_HOLY,             	TRUE    },
    {   "energy",       IMM_ENERGY,           	TRUE    },
    {   "mental",       IMM_MENTAL,           	TRUE    },
    {   "disease",      IMM_DISEASE,          	TRUE    },
    {   "drowning",     IMM_DROWNING,         	TRUE    },
    {	"sound",	IMM_SOUND,		TRUE	},
    {	"wood",		IMM_WOOD,		TRUE	},
    {	"silver",	IMM_SILVER,		TRUE	},
    {	"iron",		IMM_IRON,		TRUE	},
    {   "kill",		IMM_KILL, 		TRUE    },
    {	"water",	IMM_WATER,		TRUE	},
    {	"air",		IMM_AIR,		TRUE	},	// @@@NIB : 20070125
    {	"earth",	IMM_EARTH,		TRUE	},	// @@@NIB : 20070125
    {	"plant",	IMM_PLANT,		TRUE	},	// @@@NIB : 20070125
    {   "suffocation",  IMM_SUFFOCATION,    TRUE    },
    {	NULL,			0,	0	}
};


const struct flag_type form_flags[] =
{
    {	"edible",		FORM_EDIBLE,		TRUE	},
    {	"poison",		FORM_POISON,		TRUE	},
    {	"magical",		FORM_MAGICAL,		TRUE	},
    {	"instant_decay",	FORM_INSTANT_DECAY,	TRUE	},
    {	"other",		FORM_OTHER,		TRUE	},
    {   "no_breathing", FORM_NO_BREATHING,  TRUE    },
    {	"animal",		FORM_ANIMAL,		TRUE	},
    {	"sentient",		FORM_SENTIENT,		TRUE	},
    {	"undead",		FORM_UNDEAD,		TRUE	},
    {	"construct",		FORM_CONSTRUCT,		TRUE	},
    {	"mist",			FORM_MIST,		TRUE	},
    {	"intangible",		FORM_INTANGIBLE,	TRUE	},
    {	"biped",		FORM_BIPED,		TRUE	},
    {	"centaur",		FORM_CENTAUR,		TRUE	},
    {	"insect",		FORM_INSECT,		TRUE	},
    {	"spider",		FORM_SPIDER,		TRUE	},
    {	"crustacean",		FORM_CRUSTACEAN,	TRUE	},
    {	"worm",			FORM_WORM,		TRUE	},
    {	"blob",			FORM_BLOB,		TRUE	},
    {   "plant",        FORM_PLANT,     TRUE    },
    {	"mammal",		FORM_MAMMAL,		TRUE	},
    {	"bird",			FORM_BIRD,		TRUE	},
    {	"reptile",		FORM_REPTILE,		TRUE	},
    {	"snake",		FORM_SNAKE,		TRUE	},
    {	"dragon",		FORM_DRAGON,		TRUE	},
    {	"amphibian",		FORM_AMPHIBIAN,		TRUE	},
    {	"fish",			FORM_FISH,		TRUE	},
    {	"cold_blood",		FORM_COLD_BLOOD,	TRUE	},
    {	"object",		FORM_OBJECT,		TRUE	},
    {	NULL,			0,			0	}
};


const struct flag_type part_flags[] =
{
    {	"arms",			PART_ARMS,			TRUE	},
    {	"brains",		PART_BRAINS,		TRUE	},
    {	"claws",		PART_CLAWS,			TRUE	},
    {	"ear",			PART_EAR,			TRUE	},
    {	"eye",			PART_EYE,			TRUE	},
    {	"eyestalks",	PART_EYESTALKS,		TRUE	},
    {	"fangs",		PART_FANGS,			TRUE	},
    {	"feet",			PART_FEET,			TRUE	},
    {	"fingers",		PART_FINGERS,		TRUE	},
    {	"fins",			PART_FINS,			TRUE	},
    {	"gills",		PART_GILLS,			TRUE	},
    {	"guts",			PART_GUTS,			TRUE	},
    {	"hands",		PART_HANDS,			TRUE	},
    {	"head",			PART_HEAD,			TRUE	},
    {	"heart",		PART_HEART,			TRUE	},
    {	"hide",			PART_HIDE,			TRUE	},
    {	"horns",		PART_HORNS,			TRUE	},
    {	"legs",			PART_LEGS,			TRUE	},
    {	"long_tongue",	PART_LONG_TONGUE,	TRUE	},
    {   "lungs",        PART_LUNGS,         TRUE    },
    {	"scales",		PART_SCALES,		TRUE	},
    {	"tail",			PART_TAIL,			TRUE	},
    {	"tentacles",	PART_TENTACLES,		TRUE	},
    {	"tusks",		PART_TUSKS,			TRUE	},
    {	"wings",		PART_WINGS,			TRUE	},
    {	NULL,			0,					0	}
};


const struct flag_type comm_flags[] =
{
    {	"quiet",		COMM_QUIET,		TRUE	},
    {   "nowiz",		COMM_NOWIZ,		TRUE	},
    {   "noclangossip",		COMM_NOAUCTION,		TRUE	},
    {   "nogossip",		COMM_NOGOSSIP,		TRUE	},
    {   "nomusic",		COMM_NOMUSIC,		TRUE	},
    {   "noclan",		COMM_NOCT,		TRUE	},
    {   "compact",		COMM_COMPACT,		TRUE	},
    {   "brief",		COMM_BRIEF,		TRUE	},
    {   "prompt",		COMM_PROMPT,		TRUE	},
    {   "telnet_ga",		COMM_TELNET_GA,		TRUE	},
    {   "no_flaming",		COMM_NO_FLAMING,	TRUE	},
    {   "noyell",		COMM_NOYELL,		TRUE    },
    {   "noautowar",		COMM_NOAUTOWAR,		FALSE	},
    {   "notell",		COMM_NOTELL,		FALSE	},
    {   "nochannels",		COMM_NOCHANNELS,	FALSE	},
    {   "noquote",		COMM_NOQUOTE,		FALSE	},
    {   "afk",			COMM_AFK,		TRUE	},
    {   "ooc",			COMM_NO_OOC,		TRUE	},
    {   "hints",		COMM_NOHINTS,		TRUE	},
    {   "nobattlespam",		COMM_NOBATTLESPAM,	TRUE	},
    {   "nomap",		COMM_NOMAP,		TRUE	},
    {	"notells",		COMM_NOTELLS,		TRUE	},
    {	NULL,			0,			0	}
};

const struct flag_type area_who_display[] = {
	{	"      ",	AREA_BLANK,		TRUE	},
	{	"Abyss",	AREA_ABYSS,		TRUE	},
	{	"Arena",	AREA_ARENA,		TRUE	},
	{	"At Sea",	AREA_AT_SEA,		TRUE	},
	{	"Battle",	AREA_BATTLE,		TRUE	},
	{	"Castle",	AREA_CASTLE,		TRUE	},
	{	"Cavern",	AREA_CAVERN,		TRUE	},
	{	"Church",	AREA_CHURCH,		TRUE	},
	{	"Cosmos",	AREA_OFFICE,		TRUE	},
	{	"Cult",		AREA_CULT,		TRUE	},
	{	"Dungn",	AREA_DUNGEON,		TRUE	},
	{	"Eden",		AREA_EDEN,		TRUE	},
	{	"Forest",	AREA_FOREST,		TRUE	},
	{	"Fort",		AREA_FORT,		TRUE	},
	{	"Home",		AREA_HOME,		TRUE	},
	{	"Inn",		AREA_INN,		TRUE	},
	{	"Isle",		AREA_ISLE,		TRUE	},
	{	"Jungle",	AREA_JUNGLE,		TRUE	},
	{	"Keep",		AREA_KEEP,		TRUE	},
	{	"Limbo",	AREA_LIMBO,		TRUE	},
	{	"Mount",	AREA_MOUNTAIN,		TRUE	},
	{	"Nether",	AREA_NETHERWORLD,	TRUE	},
	{	"Outpst",	AREA_OUTPOST,		TRUE	},
	{	"Palace",	AREA_PALACE,		TRUE	},
	{	"Planar",	AREA_PLANAR,		TRUE	},
	{	"Pyramd",	AREA_PYRAMID,		TRUE	},
	{	"Rift",		AREA_CHAT,		TRUE	},
	{	"Ruins",	AREA_RUINS,		TRUE	},
	{	"Ship",		AREA_ON_SHIP,		TRUE	},
	{	"Sky",		AREA_AERIAL,		TRUE	},
	{	"Swamp",	AREA_SWAMP,		TRUE	},
	{	"Temple",	AREA_TEMPLE,		TRUE	},
	{	"Tomb",		AREA_TOMB,		TRUE	},
	{	"Tower",	AREA_TOWER,		TRUE	},
	{	"Towne",	AREA_TOWNE,		TRUE	},
	{	"Tundra",	AREA_TUNDRA,		TRUE	},
	{	"PoA",		AREA_PG,		TRUE	},
	{	"Ocean",	AREA_UNDERSEA,		TRUE	},
	{	"Villa",	AREA_VILLAGE,		TRUE	},
	{	"Vulcan",	AREA_VOLCANO,		TRUE	},
	{	"Wilder",	AREA_WILDER,		TRUE	},
	{	"Instce",	AREA_INSTANCE,		TRUE	},
	{	"Duty",		AREA_DUTY,			TRUE	},
    {   "City",     AREA_CITY,          TRUE    },
    {   "Park",     AREA_PARK,          TRUE    },
    {   "Citadl",   AREA_CITADEL,       TRUE    },
    {   "Sky",      AREA_SKY,           TRUE    },
    {   "Hills",    AREA_HILLS,         TRUE    },
	{	NULL,		0,		0	},
};



const struct flag_type area_who_titles[] = {
	{	"abyss",	AREA_ABYSS,		TRUE	},
	{	"aerial",	AREA_AERIAL,		TRUE	},
	{	"arena",	AREA_ARENA,		TRUE	},
	{	"at_sea",	AREA_AT_SEA,		TRUE	},
	{	"battle",	AREA_BATTLE,		TRUE	},
	{	"blank",	AREA_BLANK,		TRUE	},
	{	"castle",	AREA_CASTLE,		TRUE	},
	{	"cavern",	AREA_CAVERN,		TRUE	},
	{	"chat",		AREA_CHAT,		TRUE	},
	{	"church",	AREA_CHURCH,		TRUE	},
	{	"citadel",   AREA_CITADEL,       TRUE    },
	{	"city",     AREA_CITY,          TRUE    },
	{	"cult",		AREA_CULT,		TRUE	},
	{	"dungeon",	AREA_DUNGEON,		TRUE	},
	{	"duty",	AREA_DUTY,			TRUE	},
	{	"eden",		AREA_EDEN,		TRUE	},
	{	"forest",	AREA_FOREST,		TRUE	},
	{	"fort",		AREA_FORT,		TRUE	},
	{	"hills",    AREA_HILLS,         TRUE    },
	{	"home",		AREA_HOME,		TRUE	},
	{	"immortal",	AREA_OFFICE,		TRUE	},
	{	"inn",		AREA_INN,		TRUE	},
	{	"instance",		AREA_INSTANCE,		TRUE	},
	{	"isle",		AREA_ISLE,		TRUE	},
	{	"jungle",	AREA_JUNGLE,		TRUE	},
	{	"keep",		AREA_KEEP,		TRUE	},
	{	"limbo",	AREA_LIMBO,		TRUE	},
	{	"mountain",	AREA_MOUNTAIN,		TRUE	},
	{	"netherworld",	AREA_NETHERWORLD,	TRUE	},
	{	"on_ship",	AREA_ON_SHIP,		TRUE	},
	{	"outpost",	AREA_OUTPOST,		TRUE	},
	{	"palace",	AREA_PALACE,		TRUE	},
	{	"park",     AREA_PARK,          TRUE    },
	{	"pg",		AREA_PG,		TRUE	},
	{	"planar",	AREA_PLANAR,		TRUE	},
	{	"pyramid",	AREA_PYRAMID,		TRUE	},
	{	"ruins",	AREA_RUINS,		TRUE	},
	{	"sky",      AREA_SKY,           TRUE    },
	{	"swamp",	AREA_SWAMP,		TRUE	},
	{	"temple",	AREA_TEMPLE,		TRUE	},
	{	"tomb",		AREA_TOMB,		TRUE	},
	{	"tower",	AREA_TOWER,		TRUE	},
	{	"towne",	AREA_TOWNE,		TRUE	},
	{	"tundra",	AREA_TUNDRA,		TRUE	},
	{	"undersea",	AREA_UNDERSEA,		TRUE	},
	{	"village",	AREA_VILLAGE,		TRUE	},
	{	"volcano",	AREA_VOLCANO,		TRUE	},
	{	"wilderness",	AREA_WILDER,		TRUE	},
	{	NULL,		0,		0	},
};



const struct flag_type area_flags[] =
{
    {	"none",			AREA_NONE,		FALSE	},
    {	"changed",		AREA_CHANGED,		TRUE	},
    {	"added",		AREA_ADDED,		TRUE    },
    {	"loading",		AREA_LOADING,		FALSE	},
    {   "no_map",		AREA_NOMAP,		TRUE    },
    {   "dark",			AREA_DARK,		TRUE    },
    {	"testport",		AREA_TESTPORT,		TRUE	},
    {	"no_recall",	AREA_NO_RECALL,		TRUE	},
    {	"no_rooms",		AREA_NO_ROOMS,		TRUE	},
    {	"newbie",		AREA_NEWBIE,		TRUE	},
    {	"no_get_random",AREA_NO_GET_RANDOM,	TRUE	},
    {	"no_fading",	AREA_NO_FADING,		TRUE	},
    {	"blueprint",	AREA_BLUEPRINT,		TRUE	},
    {	"locked",		AREA_LOCKED,		TRUE	},
    {	NULL,			0,			0	}
};


const struct flag_type church_flags[] =
{
    {	"show_pks",		CHURCH_SHOW_PKS,	TRUE 	},
    {   "allow_crosszones",	CHURCH_ALLOW_CROSSZONES,TRUE 	},
    {	"public_motd",	CHURCH_PUBLIC_MOTD,	TRUE	},
    {	"public_rules",	CHURCH_PUBLIC_RULES,	TRUE	},
    {	"public_info",	CHURCH_PUBLIC_INFO,	TRUE	},
    {	NULL,			0,			0	},
};


const struct flag_type sex_flags[] =
{
    {	"male",			SEX_MALE,		TRUE	},
    {	"female",		SEX_FEMALE,		TRUE	},
    {	"neutral",		SEX_NEUTRAL,		TRUE	},
    {   "random",               3,                      TRUE    },
    {	"none",			SEX_NEUTRAL,		TRUE	},
    {	NULL,			0,			0	}
};


const struct flag_type exit_flags[] =
{
    {   "door",			EX_ISDOOR,		TRUE    },
    {	"closed",		EX_CLOSED,		TRUE	},
//    {	"locked",		EX_LOCKED,		TRUE	},
//    {	"pickproof",		EX_PICKPROOF,		TRUE	},
    {   "nopass",		EX_NOPASS,		TRUE	},
//    {   "easy",			EX_EASY,		TRUE	},
//    {   "hard",			EX_HARD,		TRUE	},
//    {	"infuriating",		EX_INFURIATING,		TRUE	},
    {	"noclose",		EX_NOCLOSE,		TRUE	},
    {	"nolock",		EX_NOLOCK,		TRUE	},
    {   "hidden",		EX_HIDDEN,		TRUE	},
    {   "found",		EX_FOUND,		TRUE	},
    {   "broken",		EX_BROKEN,		TRUE	},
    {   "nobash",		EX_NOBASH,		TRUE    },
    {   "walkthrough",		EX_WALKTHROUGH,		TRUE    },
    {   "barred",       EX_BARRED,      TRUE    },
    {   "nobar",		EX_NOBAR,		TRUE    },
    {   "vlink",		EX_VLINK,		FALSE	},
    {   "aerial",		EX_AERIAL,		TRUE	},
    {   "nohunt",		EX_NOHUNT,		TRUE	},
//    {	"environment",		EX_ENVIRONMENT,		TRUE	},
    {	"nounlink",		EX_NOUNLINK,		TRUE	},
    {	"prevfloor",		EX_PREVFLOOR,		FALSE	},
    {	"nextfloor",		EX_NEXTFLOOR,		FALSE	},
    {	"nosearch",			EX_NOSEARCH,		TRUE	},
    {	"mustsee",			EX_MUSTSEE,			TRUE	},
    {   "transparent",      EX_TRANSPARENT,     TRUE    },
    {	NULL,			0,			0	}
};

const struct flag_type lock_flags[] =
{
	{	"locked",		LOCK_LOCKED,		TRUE	},
	{	"magic",		LOCK_MAGIC,			TRUE	},
	{	"snap_key",		LOCK_SNAPKEY,		TRUE	},
	{	"script",		LOCK_SCRIPT,		TRUE	},
	{	"noremove",		LOCK_NOREMOVE,		TRUE	},
	{	"broken",		LOCK_BROKEN,		TRUE	},
	{	"jammed",		LOCK_JAMMED,		TRUE	},
	{	"nojam",		LOCK_NOJAM,			TRUE	},
    {   "nomagic",      LOCK_NOMAGIC,       TRUE    },
    {   "noscript",     LOCK_NOSCRIPT,      TRUE    },
    {   "final",        LOCK_FINAL,         FALSE   },
	{	"created",		LOCK_CREATED,		FALSE	},
	{	NULL,			0,					0		}
};

const struct flag_type portal_exit_flags[] =
{
    {   "door",			EX_ISDOOR,		TRUE    },
    {	"closed",		EX_CLOSED,		TRUE	},
//    {	"locked",		EX_LOCKED,		TRUE	},
//    {	"pickproof",		EX_PICKPROOF,		TRUE	},
    {   "nopass",		EX_NOPASS,		TRUE	},
//    {   "easy",			EX_EASY,		TRUE	},
//    {   "hard",			EX_HARD,		TRUE	},
//    {	"infuriating",		EX_INFURIATING,		TRUE	},
    {	"noclose",		EX_NOCLOSE,		TRUE	},
    {	"nolock",		EX_NOLOCK,		TRUE	},
//    {   "hidden",		EX_HIDDEN,		TRUE	},			// Portals can be made hidden in other ways
    {   "found",		EX_FOUND,		TRUE	},
    {   "broken",		EX_BROKEN,		TRUE	},
    {   "nobash",		EX_NOBASH,		TRUE    },
    {   "walkthrough",		EX_WALKTHROUGH,		TRUE    },
    {   "barred",       EX_BARRED,      TRUE    },
    {   "nobar",		EX_NOBAR,		TRUE    },
    {   "aerial",		EX_AERIAL,		TRUE	},
    {   "nohunt",		EX_NOHUNT,		TRUE	},
    {	"environment",		EX_ENVIRONMENT,		TRUE	},
    {	"prevfloor",		EX_PREVFLOOR,		TRUE	},
    {	"nextfloor",		EX_NEXTFLOOR,		TRUE	},
    {	"nosearch",			EX_NOSEARCH,		TRUE	},
    {	"mustsee",			EX_MUSTSEE,			TRUE	},
    {   "transparent",      EX_TRANSPARENT,     TRUE    },
    {	NULL,			0,			0	}
};


const struct flag_type door_resets[] =
{
    {	"open and unlocked",	0,		TRUE	},
    {	"closed and unlocked",	1,		TRUE	},
    {	"closed and locked",	2,		TRUE	},
    {	NULL,			0,		0	}
};


const struct flag_type room_flags[] =
{
	{	"dark",				ROOM_DARK,				TRUE	},
	{	"gods_only",		ROOM_GODS_ONLY,			TRUE	},
	{	"imp_only",			ROOM_IMP_ONLY,			TRUE	},
	{	"indoors",			ROOM_INDOORS,			TRUE	},
	{	"newbies_only",		ROOM_NEWBIES_ONLY,		TRUE	},
	{	"no_map",			ROOM_NOMAP,				TRUE	},
	{	"no_mob",			ROOM_NO_MOB,			TRUE	},
	{	"no_recall",		ROOM_NO_RECALL,			TRUE	},
	{	"no_wander",		ROOM_NO_WANDER,			TRUE	},
	{	"noview",			ROOM_NOVIEW,			TRUE	},
	{	"pet_shop",			ROOM_PET_SHOP,			TRUE	},
	{	"private",			ROOM_PRIVATE,			TRUE	},
	{	"safe",				ROOM_SAFE,				TRUE	},
	{	"solitary",			ROOM_SOLITARY,			TRUE	},
	{	"arena",			ROOM_ARENA,				TRUE	},
	{	"bank",				ROOM_BANK,				TRUE	},
	{	"chaotic",			ROOM_CHAOTIC,			TRUE	},
	{	"dark_attack",		ROOM_ATTACK_IF_DARK,	TRUE	},
	{	"death_trap",		ROOM_DEATH_TRAP,		TRUE	},
	{	"helm",				ROOM_SHIP_HELM,			TRUE	},
	{	"locker",			ROOM_LOCKER,			TRUE	},
	{	"nocomm",			ROOM_NOCOMM,			TRUE	},
	{	"nomagic",			ROOM_NOMAGIC,			TRUE	},
	{	"nowhere",			ROOM_NOWHERE,			TRUE	},
	{	"pk",				ROOM_PK,				TRUE	},
	{	"real_estate",		ROOM_HOUSE_UNSOLD,		TRUE	},
	{	"rocks",			ROOM_ROCKS,				TRUE	},
	{	"ship_shop",		ROOM_SHIP_SHOP,			TRUE	},
	{	"underwater",		ROOM_UNDERWATER,		TRUE	},
	{	"view_wilds",		ROOM_VIEWWILDS,			TRUE	},
	{	NULL,	0,	0	}
};


const struct flag_type room2_flags[] =
{
    {	"alchemy",				ROOM_ALCHEMY,			TRUE	},
    {	"bar",					ROOM_BAR,				TRUE	},
    {	"briars",				ROOM_BRIARS,			TRUE	},
    {	"citymove",				ROOM_CITYMOVE,			TRUE	},
    {	"drain_mana",			ROOM_DRAIN_MANA,		TRUE	},
    {	"hard_magic",			ROOM_HARD_MAGIC,		TRUE	},
    {	"multiplay",			ROOM_MULTIPLAY,			TRUE	},
    {	"slow_magic",			ROOM_SLOW_MAGIC,		TRUE	},
    {	"toxic_bog",			ROOM_TOXIC_BOG,			TRUE	},
    {	"virtual_room",			ROOM_VIRTUAL_ROOM,		FALSE	},
    {   "fire",					ROOM_FIRE,				TRUE    },
    {   "icy",					ROOM_ICY,				TRUE    },
    {   "no_quest",				ROOM_NO_QUEST,			TRUE    },
    {   "no_quit",				ROOM_NO_QUIT,			TRUE 	},
    {   "post_office",			ROOM_POST_OFFICE,		TRUE	},
    {	"underground",			ROOM_UNDERGROUND,		TRUE	},
    {	"vis_on_map",			ROOM_VISIBLE_ON_MAP,	TRUE	},
    {	"no_floor",				ROOM_NOFLOOR,			TRUE	},
    {	"clone_persist",		ROOM_CLONE_PERSIST,		TRUE	},
    {	"blueprint",			ROOM_BLUEPRINT, 		TRUE	},
    {	"no_clone",				ROOM_NOCLONE,			TRUE	},
    {	"no_get_random",		ROOM_NO_GET_RANDOM,		TRUE	},
    {	"always_update",		ROOM_ALWAYS_UPDATE,		TRUE	},
    {	"safe_harbor",			ROOM_SAFE_HARBOR,		TRUE	},
    {	NULL,			0,			0	}

};

const struct flag_type *room_flagbank[] =
{
    room_flags,
    room2_flags,
    NULL
};



const struct flag_type sector_flags[] =
{
    {	"inside",		SECT_INSIDE,		TRUE	},
    {	"city",			SECT_CITY,		TRUE	},
    {	"field",		SECT_FIELD,		TRUE	},
    {	"forest",		SECT_FOREST,		TRUE	},
    {	"hills",		SECT_HILLS,		TRUE	},
    {	"mountain",		SECT_MOUNTAIN,		TRUE	},
    {	"swim",			SECT_WATER_SWIM,	TRUE	},
    {	"noswim",		SECT_WATER_NOSWIM,	TRUE	},
    {   "tundra",		SECT_TUNDRA,		TRUE	},
    {	"air",			SECT_AIR,		TRUE	},
    {	"desert",		SECT_DESERT,		TRUE	},
    {	"netherworld",		SECT_NETHERWORLD,	TRUE	},
    {   "dock",			SECT_DOCK,		TRUE    },
    {   "enchanted_forest",	SECT_ENCHANTED_FOREST, 	TRUE 	},
    {   "toxic_bog",		SECT_TOXIC_BOG, 	TRUE 	},	// @@@NIB : 20070126
    {   "cursed_sanctum",	SECT_CURSED_SANCTUM, 	TRUE 	},	// @@@NIB : 20070126
    {   "bramble",		SECT_BRAMBLE,	 	TRUE 	},	// @@@NIB : 20070126
    {	"swamp",		SECT_SWAMP,		TRUE	},
    {	"acid",			SECT_ACID,		TRUE	},
    {	"lava",			SECT_LAVA,		TRUE	},
    {	"snow",			SECT_SNOW,		TRUE	},
    {	"ice",			SECT_ICE,		TRUE	},
    {	"cave",			SECT_CAVE,		TRUE	},
    {	"underwater",		SECT_UNDERWATER,	TRUE	},
    {	"deep_underwater",	SECT_DEEP_UNDERWATER,	TRUE	},
    {	"jungle",		SECT_JUNGLE,		TRUE	},
    {	NULL,			0,			0	}
};


const struct flag_type type_flags[] =
{
    {	"light",		ITEM_LIGHT,		TRUE	},
    {	"scroll",		ITEM_SCROLL,		TRUE	},
    {	"wand",			ITEM_WAND,		TRUE	},
    {	"staff",		ITEM_STAFF,		TRUE	},
    {	"weapon",		ITEM_WEAPON,		TRUE	},
    {	"treasure",		ITEM_TREASURE,		TRUE	},
    {	"armour",		ITEM_ARMOUR,		TRUE	},
    {	"furniture",		ITEM_FURNITURE,		TRUE	},
    {	"trash",		ITEM_TRASH,		TRUE	},
    {	"container",		ITEM_CONTAINER,		TRUE	},
    {	"fluidcontainer",	ITEM_FLUID_CONTAINER,		TRUE	},
    {	"key",			ITEM_KEY,		TRUE	},
    {	"food",			ITEM_FOOD,		TRUE	},
    {	"money",		ITEM_MONEY,		TRUE	},
    {	"boat",			ITEM_BOAT,		TRUE	},
    {	"npccorpse",		ITEM_CORPSE_NPC,	TRUE	},
    {	"pc corpse",		ITEM_CORPSE_PC,		FALSE	},
    {	"pill",			ITEM_PILL,		TRUE	},
    {	"protect",		ITEM_PROTECT,		TRUE	},
    {	"map",			ITEM_MAP,		TRUE	},
    {   "portal",		ITEM_PORTAL,		TRUE	},
    {   "catalyst",		ITEM_CATALYST,		TRUE	},
    {	"roomkey",		ITEM_ROOM_KEY,		TRUE	},
    { 	"gem",			ITEM_GEM,		TRUE	},
    {	"jewelry",		ITEM_JEWELRY,		TRUE	},
    {	"jukebox",		ITEM_JUKEBOX,		TRUE	},
    {	"artifact",		ITEM_ARTIFACT,		TRUE	},
    {   "shares",		ITEM_SHARECERT,		TRUE	},
    {   "flame_room_object",	ITEM_ROOM_FLAME,	TRUE	},
    {   "instrument",		ITEM_INSTRUMENT,	TRUE	},
    {   "seed",			ITEM_SEED,		TRUE	},
    {   "cart",			ITEM_CART,		TRUE	},
    {   "ship",			ITEM_SHIP,		TRUE	},
    {   "room_darkness_object",	ITEM_ROOM_DARKNESS,	TRUE	},
    {   "ranged_weapon",	ITEM_RANGED_WEAPON,	TRUE	},
    {   "sextant",		ITEM_SEXTANT,		TRUE	},
    {   "weapon_container",	ITEM_WEAPON_CONTAINER,	FALSE	},
    {   "room_roomshield_object",	ITEM_ROOM_ROOMSHIELD,	TRUE	},
    {	"book",			ITEM_BOOK,		TRUE	},
    {	"page",			ITEM_PAGE,		TRUE	},
    {   "stinking_cloud",	ITEM_STINKING_CLOUD,	TRUE	},
    {   "smoke_bomb",       	ITEM_SMOKE_BOMB,	TRUE	},
    {   "herb",			ITEM_HERB,		TRUE	},
    {   "spell_trap",       	ITEM_SPELL_TRAP,	TRUE	},
    {   "withering_cloud",  	ITEM_WITHERING_CLOUD,	TRUE	},
    {   "bank",			ITEM_BANK,		TRUE 	},
    {   "keyring",		ITEM_KEYRING,		FALSE 	},
    {	"ice_storm",		ITEM_ICE_STORM,		TRUE	},
    {	"flower",		ITEM_FLOWER,		TRUE	},
    {   "trade_type",		ITEM_TRADE_TYPE,	TRUE	},
    {   "empty_vial",	        ITEM_EMPTY_VIAL,	TRUE	},
    {   "blank_scroll",		ITEM_BLANK_SCROLL,	TRUE    },
    {	"mist",			ITEM_MIST,		TRUE	},
    {	"shrine",		ITEM_SHRINE,		TRUE	},
    {   "whistle",		ITEM_WHISTLE, TRUE  },
    {   "shovel",		ITEM_SHOVEL,		TRUE	},
    //{   "tool",			ITEM_TOOL,			TRUE	},	// @@@NIB : 20070215
    {   "tattoo",		ITEM_TATTOO,		TRUE	},
    {   "ink",			ITEM_INK,			TRUE	},
    {   "part",			ITEM_PART,			TRUE	},
	{	"telescope",	ITEM_TELESCOPE,		TRUE	},
	{	"compass",		ITEM_COMPASS,		TRUE	},
	{	"whetstone",	ITEM_WHETSTONE,		TRUE	},
	{	"chisel",		ITEM_CHISEL,		TRUE	},
	{	"pick",			ITEM_PICK,			TRUE	},
	{	"tinderbox",	ITEM_TINDERBOX,		TRUE	},
	{	"drying_cloth",	ITEM_DRYING_CLOTH,	TRUE	},
	{	"needle",		ITEM_NEEDLE,		TRUE	},
	{	"body_part",	ITEM_BODY_PART,		TRUE	},
    {	NULL,			0,			0	}
};

const struct flag_type extra_flags[] =
{
	{	"antievil",		ITEM_ANTI_EVIL,		TRUE	},
	{	"antigood",		ITEM_ANTI_GOOD,		TRUE	},
	{	"antineutral",	ITEM_ANTI_NEUTRAL,	TRUE	},
	{	"bless",		ITEM_BLESS,		    TRUE	},
    {   "bought",       ITEM_SHOP_BOUGHT,   FALSE   },
	{	"burnproof",	ITEM_BURN_PROOF,	TRUE	},
	{	"evil",			ITEM_EVIL,	    	TRUE	},
	{	"freezeproof",	ITEM_FREEZE_PROOF,	TRUE	},
	{	"glow",			ITEM_GLOW,          TRUE	},
	{	"headless",		ITEM_NOSKULL,		FALSE   },
	{	"hidden",		ITEM_HIDDEN,		FALSE   },
	{	"holy",         ITEM_HOLY,          FALSE   },	
	{	"hum",			ITEM_HUM,	    	TRUE	},
	{	"inventory",	ITEM_INVENTORY,		FALSE   },
	{	"invis",		ITEM_INVIS,		    TRUE	},
	{	"magic",		ITEM_MAGIC,	    	TRUE	},
	{	"meltdrop",		ITEM_MELT_DROP,		TRUE	},
	{	"no_restring",	ITEM_NORESTRING,	TRUE	},
	{	"nodrop",		ITEM_NODROP,		TRUE	},
	{	"nokeyring",	ITEM_NOKEYRING,	   	TRUE    },
	{	"nolocate",		ITEM_NOLOCATE,		TRUE	},
	{	"nopurge",		ITEM_NOPURGE,		TRUE	},
	{	"noquest",		ITEM_NOQUEST,		TRUE	},
	{	"noremove",		ITEM_NOREMOVE,		TRUE	},
	{	"nosteal",		ITEM_NOSTEAL,		TRUE	},
	{	"nouncurse",	ITEM_NOUNCURSE,		TRUE	},
	{	"permanent",	ITEM_PERMANENT,     FALSE   },
	{	"planted",		ITEM_PLANTED,		FALSE   },
	{	"rotdeath",		ITEM_ROT_DEATH,		TRUE	},
	{	"shockproof",	ITEM_SHOCK_PROOF,	TRUE	},
    {	NULL,			0,			0	}
};


const struct flag_type extra2_flags[] =
{
	{   "all_remort",		ITEM_ALL_REMORT,	TRUE    },
	{   "buried",	    	ITEM_BURIED,		FALSE   },
	{   "emits_light",		ITEM_EMITS_LIGHT,	TRUE    },
	{   "enchanted",		ITEM_ENCHANTED,		FALSE   },
	{   "float_user",		ITEM_FLOAT_USER,	TRUE    },
	{   "keep_value",		ITEM_KEEP_VALUE,	TRUE	},
	{   "kept",		    	ITEM_KEPT,		    FALSE	},
	{   "locker",		    ITEM_LOCKER,		TRUE    },
	{   "no_auction",		ITEM_NOAUCTION,		TRUE	},
	{   "no_container",		ITEM_NO_CONTAINER,	TRUE	},
	{   "no_discharge",		ITEM_NO_DISCHARGE,	TRUE	},
	{   "no_donate",		ITEM_NO_DONATE,		TRUE    },
	{   "no_enchant",		ITEM_NO_ENCHANT,	TRUE	},
	{   "no_hunt",		    ITEM_NO_HUNT,		TRUE    },
	{   "no_locker",		ITEM_NOLOCKER,		TRUE	},
	{   "no_loot",	    	ITEM_NO_LOOT,		TRUE	},
	{   "no_lore",		    ITEM_NO_LORE,		TRUE    },
	{   "no_resurrect",		ITEM_NO_RESURRECT,	FALSE   },
	{   "remort_only",		ITEM_REMORT_ONLY,	TRUE    },
	{   "scare",		    ITEM_SCARE, 		TRUE	},
	{   "see_hidden",		ITEM_SEE_HIDDEN,	TRUE    },
	{   "sell_once",		ITEM_SELL_ONCE,		TRUE    },
	{   "singular",	    	ITEM_SINGULAR,		TRUE	},
	{   "super-strong",		ITEM_SUPER_STRONG,	TRUE    },
	{   "sustains",		    ITEM_SUSTAIN,		TRUE	},
	{   "third_eye",		ITEM_THIRD_EYE,		FALSE   },
	{   "trapped",	    	ITEM_TRAPPED,		FALSE   },
	{   "true_sight",		ITEM_TRUESIGHT,		TRUE	},
    {   NULL,			0,			0	}
};

const struct flag_type extra3_flags[] =
{
    {	"always_loot",		ITEM_ALWAYS_LOOT,	TRUE	},
    {	"can_dispel",		ITEM_CAN_DISPEL,	TRUE	},
    {	"exclude_list",		ITEM_EXCLUDE_LIST,	TRUE	},
    {	"force_loot",		ITEM_FORCE_LOOT,	FALSE	},
    {	"instance_obj",		ITEM_INSTANCE_OBJ,	FALSE	},
    {	"keep_equipped",	ITEM_KEEP_EQUIPPED,	TRUE	},
    {   "no_animate",		ITEM_NO_ANIMATE,	FALSE   },
    {	"no_transfer",		ITEM_NO_TRANSFER,	TRUE	},
    {	"rift_update",		ITEM_RIFT_UPDATE,	TRUE	},
    {	"show_in_wilds",	ITEM_SHOW_IN_WILDS,	TRUE	},
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
    {	"take",			ITEM_TAKE,      		TRUE	},
    {	"finger",		ITEM_WEAR_FINGER,   	TRUE	},
    {	"neck",			ITEM_WEAR_NECK, 		TRUE	},
    {	"body",			ITEM_WEAR_BODY, 		TRUE	},
    {	"head",			ITEM_WEAR_HEAD, 		TRUE	},
    {	"legs",			ITEM_WEAR_LEGS, 		TRUE	},
    {	"feet",			ITEM_WEAR_FEET, 		TRUE	},
    {	"hands",		ITEM_WEAR_HANDS,    	TRUE	},
    {	"arms",			ITEM_WEAR_ARMS, 		TRUE	},
    {	"shield",		ITEM_WEAR_SHIELD,   	TRUE	},
    {	"about",		ITEM_WEAR_ABOUT,    	TRUE	},
    {	"waist",		ITEM_WEAR_WAIST,    	TRUE	},
    {	"wrist",		ITEM_WEAR_WRIST,    	TRUE	},
    {	"wield",		ITEM_WIELD,		        TRUE	},
    {	"hold",			ITEM_HOLD,	    	    TRUE	},
    {   "nosac",		ITEM_NO_SAC,	    	TRUE	},
    {	"float",		ITEM_WEAR_FLOAT,    	TRUE	},
    {   "ring_finger",	ITEM_WEAR_RING_FINGER,  TRUE    },
    {   "back",			ITEM_WEAR_BACK, 		TRUE    },
    {   "shoulder",		ITEM_WEAR_SHOULDER, 	TRUE	},
    {   "face",			ITEM_WEAR_FACE, 		TRUE	},
    {   "eyes",			ITEM_WEAR_EYES, 		TRUE	},
    {   "ear",			ITEM_WEAR_EAR,  		TRUE	},
    {   "ankle",		ITEM_WEAR_ANKLE,    	TRUE	},
    {   "conceals",		ITEM_CONCEALS,  		TRUE	},
    {	"tabard",		ITEM_WEAR_TABARD,   	TRUE	},
    {   "tattoo",       ITEM_WEAR_TATTOO,       TRUE    },
    {	NULL,			0,          			0       }
};


/*
 * Used when adding an affect to tell where it goes.
 * See addaffect and delaffect in act_olc.c
 */
const struct flag_type apply_flags[] =
{
    {	"none",			    APPLY_NONE,		    TRUE	},
    {	"ac",			    APPLY_AC,		    TRUE	},
    {	"strength",		    APPLY_STR,		    TRUE	},
    {	"dexterity",	    APPLY_DEX,		    TRUE	},
    {	"intelligence",	    APPLY_INT,		    TRUE	},
    {	"wisdom",		    APPLY_WIS,		    TRUE	},
    {	"constitution",	    APPLY_CON,		    TRUE	},
    {	"sex",			    APPLY_SEX,		    TRUE	},
    {	"mana",			    APPLY_MANA,		    TRUE	},
    {	"hp",			    APPLY_HIT,		    TRUE	},
    {	"move",			    APPLY_MOVE,		    TRUE	},
    {	"hitroll",		    APPLY_HITROLL,		TRUE	},
    {	"damroll",		    APPLY_DAMROLL,		TRUE	},
    {	"skill",		    APPLY_SKILL,		FALSE	},
    {	"spellaffect",		APPLY_SPELL_AFFECT,	FALSE	},
    {   "xpboost",          APPLY_XPBOOST,      TRUE    },
    {	NULL,			0,			0	}
};

const struct flag_type wear_loc_names[] =
{
	{ "NONE",			WEAR_NONE,	TRUE },
	{ "LIGHT",			WEAR_LIGHT,	TRUE },
	{ "FINGER_L",		WEAR_FINGER_L,	TRUE },
	{ "FINGER_R",		WEAR_FINGER_R,	TRUE },
	{ "NECK_1",		WEAR_NECK_1,	TRUE },
	{ "NECK_2",		WEAR_NECK_2,	TRUE },
	{ "BODY",			WEAR_BODY,	TRUE },
	{ "HEAD",			WEAR_HEAD,	TRUE },
	{ "LEGS",			WEAR_LEGS,	TRUE },
	{ "FEET",			WEAR_FEET,	TRUE },
	{ "HANDS",			WEAR_HANDS,	TRUE },
	{ "ARMS",			WEAR_ARMS,	TRUE },
	{ "SHIELD",		WEAR_SHIELD,	TRUE },
	{ "ABOUT",			WEAR_ABOUT,	TRUE },
	{ "WAIST",			WEAR_WAIST,	TRUE },
	{ "WRIST_L",		WEAR_WRIST_L,	TRUE },
	{ "WRIST_R",		WEAR_WRIST_R,	TRUE },
	{ "WIELD",			WEAR_WIELD,	TRUE },
	{ "HOLD",			WEAR_HOLD,	TRUE },
	{ "SECONDARY",		WEAR_SECONDARY,	TRUE },
	{ "RING_FINGER",	WEAR_RING_FINGER,	TRUE },
	{ "BACK",			WEAR_BACK,	TRUE },
	{ "SHOULDER",		WEAR_SHOULDER,	TRUE },
	{ "ANKLE_L",		WEAR_ANKLE_L,	TRUE },
	{ "ANKLE_R",		WEAR_ANKLE_R,	TRUE },
	{ "EAR_L",			WEAR_EAR_L,	TRUE },
	{ "EAR_R",			WEAR_EAR_R,	TRUE },
	{ "EYES",			WEAR_EYES,	TRUE },
	{ "FACE",			WEAR_FACE,	TRUE },
	{ "TATTOO_HEAD",	WEAR_TATTOO_HEAD,	TRUE },
	{ "TATTOO_TORSO",	WEAR_TATTOO_TORSO,	TRUE },
	{ "TATTOO_UPPER_ARM_L",	WEAR_TATTOO_UPPER_ARM_L,	TRUE },
	{ "TATTOO_UPPER_ARM_R",	WEAR_TATTOO_UPPER_ARM_R,	TRUE },
	{ "TATTOO_UPPER_LEG_L",	WEAR_TATTOO_UPPER_LEG_L,	TRUE },
	{ "TATTOO_UPPER_LEG_R",	WEAR_TATTOO_UPPER_LEG_R,	TRUE },
	{ "LODGED_HEAD",	WEAR_LODGED_HEAD,	TRUE },
	{ "LODGED_TORSO",	WEAR_LODGED_TORSO,	TRUE },
	{ "LODGED_ARM_L",	WEAR_LODGED_ARM_L,	TRUE },
	{ "LODGED_ARM_R",	WEAR_LODGED_ARM_R,	TRUE },
	{ "LODGED_LEG_L",	WEAR_LODGED_LEG_L,	TRUE },
	{ "LODGED_LEG_R",	WEAR_LODGED_LEG_R,	TRUE },
	{ "ENTANGLED",		WEAR_ENTANGLED,	TRUE },
	{ "CONCEALED",		WEAR_CONCEALED,	TRUE },
	{ "FLOATING",		WEAR_FLOATING,	TRUE },
        { "TATTOO_LOWER_ARM_L",        WEAR_TATTOO_LOWER_ARM_L,       TRUE },
        { "TATTOO_LOWER_ARM_R",       WEAR_TATTOO_LOWER_ARM_R,      TRUE },
        { "TATTOO_LOWER_LEG_L",       WEAR_TATTOO_LOWER_LEG_L,      TRUE },
        { "TATTOO_LOWER_LEG_R",       WEAR_TATTOO_LOWER_LEG_R,      TRUE },
        { "TATTOO_SHOULDER_L",       WEAR_TATTOO_SHOULDER_L,      TRUE },
        { "TATTOO_SHOULDER_R",       WEAR_TATTOO_SHOULDER_R,      TRUE },
        { "TATTOO_BACK",       WEAR_TATTOO_BACK,      TRUE },
        { "TATTOO_NECK",       WEAR_TATTOO_NECK,      TRUE },

    {	NULL,			0,			0	}

};


const struct flag_type wear_loc_strings[] =
{
    {	"in the inventory",	WEAR_NONE,		TRUE	},
    {	"as a light",		WEAR_LIGHT,		TRUE	},
    {	"on the left finger",	WEAR_FINGER_L,		TRUE	},
    {	"on the right finger",	WEAR_FINGER_R,		TRUE	},
    {	"around the neck (1)",	WEAR_NECK_1,		TRUE	},
    {	"around the neck (2)",	WEAR_NECK_2,		TRUE	},
    {	"on the body",		WEAR_BODY,		TRUE	},
    {	"over the head",	WEAR_HEAD,		TRUE	},
    {	"on the legs",		WEAR_LEGS,		TRUE	},
    {	"on the feet",		WEAR_FEET,		TRUE	},
    {	"on the hands",		WEAR_HANDS,		TRUE	},
    {	"on the arms",		WEAR_ARMS,		TRUE	},
    {	"as a shield",		WEAR_SHIELD,		TRUE	},
    {	"about the shoulders",	WEAR_ABOUT,		TRUE	},
    {	"around the waist",	WEAR_WAIST,		TRUE	},
    {	"on the left wrist",	WEAR_WRIST_L,		TRUE	},
    {	"on the right wrist",	WEAR_WRIST_R,		TRUE	},
    {	"wielded",		WEAR_WIELD,		TRUE	},
    {	"held in the hands",	WEAR_HOLD,		TRUE	},
    {	"off-hand",		WEAR_SECONDARY,		TRUE	},	// NIB : 20070121 : was missing
    {	"on the ring finger",	WEAR_RING_FINGER,	TRUE	},	// NIB : 20070121 : was missing
    {	"on the back",		WEAR_BACK,		TRUE	},	// NIB : 20070121 : was missing
    {	"on the shoulders",	WEAR_SHOULDER,		TRUE	},	// NIB : 20070121 : was missing
    {	"around left ankle",	WEAR_ANKLE_L,		TRUE	},
    {	"around right ankle",	WEAR_ANKLE_R,		TRUE	},
    {	"on the left ear",	WEAR_EAR_L,		TRUE	},
    {	"on the right ear",	WEAR_EAR_R,		TRUE	},
    {	"over the eyes",	WEAR_EYES,		TRUE	},
    {	"on the face",		WEAR_FACE,		TRUE	},
    {	"tattooed on head",	WEAR_TATTOO_HEAD,	TRUE	},
    {	"tattooed on torso",	WEAR_TATTOO_TORSO,	TRUE	},
    {	"tattooed on upper left arm",	WEAR_TATTOO_UPPER_ARM_L,	TRUE	},
    {	"tattooed on upper right arm",	WEAR_TATTOO_UPPER_ARM_R,	TRUE	},
    {	"tattooed on upper left leg",	WEAR_TATTOO_UPPER_LEG_L,	TRUE	},
    {	"tattooed on upper right leg",	WEAR_TATTOO_UPPER_LEG_R,	TRUE	},
    {	"lodged in head",	WEAR_LODGED_HEAD,	TRUE	},
    {	"lodged in torso",	WEAR_LODGED_TORSO,	TRUE	},
    {	"lodged in left arm",	WEAR_LODGED_ARM_L,	TRUE	},
    {	"lodged in right arm",	WEAR_LODGED_ARM_R,	TRUE	},
    {	"lodged in left leg",	WEAR_LODGED_LEG_L,	TRUE	},
    {	"lodged in right leg",	WEAR_LODGED_LEG_R,	TRUE	},
    {	"entangled",		WEAR_ENTANGLED,		TRUE	},
    {	"concealed from view",	WEAR_CONCEALED,		TRUE	},
    {	"floating in the air",	WEAR_FLOATING,		TRUE	},
    {   "tattooed on lower left arm",     WEAR_TATTOO_LOWER_ARM_L,       TRUE    },
    {   "tattooed on lower right arm",    WEAR_TATTOO_LOWER_ARM_R,      TRUE    },
    {   "tattooed on lower left leg",   WEAR_TATTOO_LOWER_LEG_L,      TRUE    },
    {   "tattooed on lower right leg",  WEAR_TATTOO_LOWER_LEG_R,      TRUE    },
    {   "tattooed on left shoulder",   WEAR_TATTOO_SHOULDER_L,      TRUE    },
    {   "tattooed on right shoulder",  WEAR_TATTOO_SHOULDER_R,      TRUE    },
    {   "tattooed on back",  WEAR_TATTOO_BACK,      TRUE    },
    {   "tattooed on neck",  WEAR_TATTOO_NECK,      TRUE    },
    {	NULL,			0,			0	}
};


const struct flag_type wear_loc_flags[] =
{
    {	"none",		WEAR_NONE,		TRUE	},
    {	"light",	WEAR_LIGHT,		TRUE	},
    {	"lfinger",	WEAR_FINGER_L,		TRUE	},
    {	"rfinger",	WEAR_FINGER_R,		TRUE	},
    {	"neck1",	WEAR_NECK_1,		TRUE	},
    {	"neck2",	WEAR_NECK_2,		TRUE	},
    {	"body",		WEAR_BODY,		TRUE	},
    {	"head",		WEAR_HEAD,		TRUE	},
    {	"legs",		WEAR_LEGS,		TRUE	},
    {	"feet",		WEAR_FEET,		TRUE	},
    {	"hands",	WEAR_HANDS,		TRUE	},
    {	"arms",		WEAR_ARMS,		TRUE	},
    {	"shield",	WEAR_SHIELD,		TRUE	},
    {	"about",	WEAR_ABOUT,		TRUE	},
    {	"waist",	WEAR_WAIST,		TRUE	},
    {	"lwrist",	WEAR_WRIST_L,		TRUE	},
    {	"rwrist",	WEAR_WRIST_R,		TRUE	},
    {	"wielded",	WEAR_WIELD,		TRUE	},
    {	"hold",		WEAR_HOLD,		TRUE	},
    {	"secondary",	WEAR_SECONDARY,		TRUE	},	// NIB : 20070121 : was missing
    {	"ringfinger",	WEAR_RING_FINGER,	TRUE	},	// NIB : 20070121 : was missing
    {	"back",		WEAR_BACK,		TRUE	},	// NIB : 20070121 : was missing
    {	"shoulder",	WEAR_SHOULDER,		TRUE	},	// NIB : 20070121 : was missing
    {	"lankle",	WEAR_ANKLE_L,		TRUE	},
    {	"rankle",	WEAR_ANKLE_R,		TRUE	},
    {	"lear",		WEAR_EAR_L,		TRUE	},
    {	"rear",		WEAR_EAR_R,		TRUE	},
    {	"eyes",		WEAR_EYES,		TRUE	},
    {	"face",		WEAR_FACE,		TRUE	},
    {	"tattoohead",	WEAR_TATTOO_HEAD,	TRUE	},
    {	"tattootorso",	WEAR_TATTOO_TORSO,	TRUE	},
    {	"tattooupperlarm",	WEAR_TATTOO_UPPER_ARM_L,	TRUE	},
    {	"tattooupperrarm",	WEAR_TATTOO_UPPER_ARM_R,	TRUE	},
    {	"tattooupperlleg",	WEAR_TATTOO_UPPER_LEG_L,	TRUE	},
    {	"tattooupperrleg",	WEAR_TATTOO_UPPER_LEG_R,	TRUE	},
    {	"lodgedhead",	WEAR_LODGED_HEAD,	TRUE	},
    {	"lodgedtorso",	WEAR_LODGED_TORSO,	TRUE	},
    {	"lodgedlarm",	WEAR_LODGED_ARM_L,	TRUE	},
    {	"lodgedrarm",	WEAR_LODGED_ARM_R,	TRUE	},
    {	"lodgedlleg",	WEAR_LODGED_LEG_L,	TRUE	},
    {	"lodgedrleg",	WEAR_LODGED_LEG_R,	TRUE	},
    {	"entangled",	WEAR_ENTANGLED,		TRUE	},
    {	"concealed",	WEAR_CONCEALED,		TRUE	},
    {	"floating",	WEAR_FLOATING,		TRUE	},
    {   "tattoolowerlarm",   WEAR_TATTOO_LOWER_ARM_L,       TRUE    },
    {   "tattoolowerrarm",  WEAR_TATTOO_LOWER_ARM_R,      TRUE    },
    {   "tattoolowerlleg",   WEAR_TATTOO_LOWER_LEG_L,      TRUE    },
    {   "tattoolowerrleg",   WEAR_TATTOO_LOWER_LEG_R,      TRUE    },
    {   "tattoolshoulder",   WEAR_TATTOO_SHOULDER_L,      TRUE    },
    {   "tattoorshoulder",   WEAR_TATTOO_SHOULDER_R,      TRUE    },
    {   "tattooback",   WEAR_TATTOO_BACK,       TRUE    },
    {   "tattooneck",   WEAR_TATTOO_NECK,       TRUE    },
    {	NULL,		0,		0	}
};


const struct flag_type container_flags[] =
{
    {   "catalysts",    CONT_CATALYSTS, TRUE    },
    {	"closeable",	CONT_CLOSEABLE,	TRUE	},
    {	"closed",		CONT_CLOSED,	TRUE	},
    {	"closelock",	CONT_CLOSELOCK,	TRUE	},	// @@@NIB : 20070126
    {   "no_duplicates",CONT_NO_DUPLICATES, TRUE    },
    {	"pushopen",		CONT_PUSHOPEN,	TRUE	},	// @@@NIB : 20070126
    {	"puton",		CONT_PUT_ON,	TRUE	},
    {   "singular",     CONT_SINGULAR,  TRUE    },
    {   "transparent",  CONT_TRANSPARENT,   TRUE    },
    {	NULL,			0,		0	}
};


const struct flag_type ac_type[] =
{
    {   "pierce",        AC_PIERCE,            TRUE    	},
    {   "bash",          AC_BASH,              TRUE    	},
    {   "slash",         AC_SLASH,             TRUE    	},
    {   "exotic",        AC_EXOTIC,            TRUE    	},
    {   NULL,            0,                    0       	}
};


const struct flag_type size_flags[] =
{
    {   "tiny",          SIZE_TINY,            TRUE    	},
    {   "small",         SIZE_SMALL,           TRUE    	},
    {   "medium",        SIZE_MEDIUM,          TRUE    	},
    {   "large",         SIZE_LARGE,           TRUE    	},
    {   "huge",          SIZE_HUGE,            TRUE    	},
    {   "giant",         SIZE_GIANT,           TRUE    	},
    {   NULL,              0,                    0     	},
};


const struct flag_type ranged_weapon_class[] =
{
    {   "exotic",        RANGED_WEAPON_EXOTIC,  TRUE    },
    {   "crossbow",      RANGED_WEAPON_CROSSBOW,TRUE    },
    {   "bow",           RANGED_WEAPON_BOW,     TRUE    },
    {   "blowgun",	RANGED_WEAPON_BLOWGUN,	TRUE    },	// @@@NIB : 20070126 : darts!
    {   "harpoon",	RANGED_WEAPON_HARPOON,	TRUE    },	// @@@NIB : 20070126 : harpoons!
    {   NULL,          	 0,                     TRUE    }
};


const struct flag_type weapon_class[] =
{
    {   "exotic",	WEAPON_EXOTIC,		TRUE    },
    {   "sword",	WEAPON_SWORD,		TRUE    },
    {   "dagger",	WEAPON_DAGGER,		TRUE    },
    {   "spear",	WEAPON_SPEAR,		TRUE    },
    {   "mace",		WEAPON_MACE,		TRUE    },
    {   "axe",		WEAPON_AXE,		TRUE    },
    {   "flail",	WEAPON_FLAIL,		TRUE    },
    {   "whip",		WEAPON_WHIP,		TRUE    },
    {   "polearm",	WEAPON_POLEARM,		TRUE    },
    {	"stake",	WEAPON_STAKE,		TRUE	},
    {	"quarterstaff",	WEAPON_QUARTERSTAFF,	TRUE	},
    {	"arrow",	WEAPON_ARROW,		TRUE	},
    {	"bolt",		WEAPON_BOLT,		TRUE	},
    {	"dart",		WEAPON_DART,		TRUE	},
    {	"harpoon",	WEAPON_HARPOON,		TRUE	},
    {	"throwable",	WEAPON_THROWABLE,	FALSE	},
    {   NULL,		0,			0       }
};


const struct flag_type weapon_type2[] =
{
    {	"acidic",	WEAPON_ACIDIC,		TRUE	},	// @@@NIB : 20070209
    {	"annealed",	WEAPON_ANNEALED,	TRUE	},	// @@@NIB : 20070209
//    {	"barbed",	WEAPON_BARBED,		TRUE	},	// @@@NIB : 20070209
    {	"blaze",	WEAPON_BLAZE,		TRUE	},	// @@@NIB : 20070209
//    {	"chipped",	WEAPON_CHIPPED,		TRUE	},	// @@@NIB : 20070209
    {	"dull",		WEAPON_DULL,		TRUE	},	// @@@NIB : 20070209
    {	"offhand",	WEAPON_OFFHAND,		TRUE	},	// @@@NIB : 20070209
    {	"onehand",	WEAPON_ONEHAND,		TRUE	},	// @@@NIB : 20070209
    {	"poison",	WEAPON_POISON,		TRUE	},
    {	"resonate",	WEAPON_RESONATE,	TRUE	},	// @@@NIB : 20070209
    {	"shocking",	WEAPON_SHOCKING,      	TRUE    },
    {	"suckle",	WEAPON_SUCKLE,		TRUE	},	// @@@NIB : 20070209
    {	"throwable",	WEAPON_THROWABLE,	TRUE	},
    {   "flaming",      WEAPON_FLAMING,       	TRUE    },
    {   "frost",        WEAPON_FROST,         	TRUE    },
    {   "sharp",        WEAPON_SHARP,         	TRUE    },
    {   "twohands",     WEAPON_TWO_HANDS,     	TRUE    },
    {   "vampiric",     WEAPON_VAMPIRIC,      	TRUE    },
    {   "vorpal",       WEAPON_VORPAL,        	TRUE    },
    {   NULL,           0,          		0	},
};


const struct flag_type res_flags[] =
{
    {	"summon",	RES_SUMMON,		TRUE	},
    {   "charm",        RES_CHARM,            	TRUE    },
    {   "magic",        RES_MAGIC,            	TRUE    },
    {   "weapon",       RES_WEAPON,           	TRUE    },
    {   "bash",         RES_BASH,             	TRUE    },
    {   "pierce",       RES_PIERCE,           	TRUE    },
    {   "slash",        RES_SLASH,            	TRUE    },
    {   "fire",         RES_FIRE,             	TRUE    },
    {   "cold",         RES_COLD,             	TRUE    },
    {   "light",     	RES_LIGHT,            	TRUE    },
    {   "lightning",    RES_LIGHTNING,        	TRUE    },
    {   "acid",         RES_ACID,             	TRUE    },
    {   "poison",       RES_POISON,           	TRUE    },
    {   "negative",     RES_NEGATIVE,         	TRUE    },
    {   "holy",         RES_HOLY,             	TRUE    },
    {   "energy",       RES_ENERGY,           	TRUE    },
    {   "mental",       RES_MENTAL,           	TRUE    },
    {   "disease",      RES_DISEASE,          	TRUE    },
    {   "drowning",     RES_DROWNING,         	TRUE    },
    {	"sound",	RES_SOUND,		TRUE	},
    {	"wood",		RES_WOOD,		TRUE	},
    {	"silver",	RES_SILVER,		TRUE	},
    {	"iron",		RES_IRON,		TRUE	},
    {   "kill",		RES_KILL, 		TRUE    },
    {	"water",	RES_WATER,		TRUE	},
    {	"air",		RES_AIR,		TRUE	},	// @@@NIB : 20070125
    {	"earth",	RES_EARTH,		TRUE	},	// @@@NIB : 20070125
    {	"plant",	RES_PLANT,		TRUE	},	// @@@NIB : 20070125
    {   "suffocation",  RES_SUFFOCATION,    TRUE    },
    {   NULL,          	0,            		0    	}
};


const struct flag_type vuln_flags[] =
{
    {	"summon",	VULN_SUMMON,		TRUE	},
    {	"charm",	VULN_CHARM,		TRUE	},
    {   "magic",        VULN_MAGIC,           	TRUE    },
    {   "weapon",       VULN_WEAPON,          	TRUE    },
    {   "bash",         VULN_BASH,            	TRUE    },
    {   "pierce",       VULN_PIERCE,          	TRUE    },
    {   "slash",        VULN_SLASH,           	TRUE    },
    {   "fire",         VULN_FIRE,            	TRUE    },
    {   "cold",         VULN_COLD,            	TRUE    },
    {   "light",        VULN_LIGHT,           	TRUE    },
    {   "lightning",    VULN_LIGHTNING,       	TRUE    },
    {   "acid",         VULN_ACID,            	TRUE    },
    {   "poison",       VULN_POISON,          	TRUE    },
    {   "negative",     VULN_NEGATIVE,        	TRUE    },
    {   "holy",         VULN_HOLY,            	TRUE    },
    {   "energy",       VULN_ENERGY,          	TRUE    },
    {   "mental",       VULN_MENTAL,          	TRUE    },
    {   "disease",      VULN_DISEASE,         	TRUE    },
    {   "drowning",     VULN_DROWNING,        	TRUE    },
    {	"sound",	VULN_SOUND,		TRUE	},
    {   "wood",         VULN_WOOD,            	TRUE    },
    {   "silver",       VULN_SILVER,          	TRUE    },
    {   "iron",         VULN_IRON,		TRUE    },
    {   "kill",		VULN_KILL,	       	TRUE    },
    {	"water",	VULN_WATER,		TRUE	},
    {	"air",		VULN_AIR,		TRUE	},	// @@@NIB : 20070125
    {	"earth",	VULN_EARTH,		TRUE	},	// @@@NIB : 20070125
    {	"plant",	VULN_PLANT,		TRUE	},	// @@@NIB : 20070125
    {   "suffocation",  VULN_SUFFOCATION,   TRUE    },
    {   NULL,           0,                    	0       }
};

const struct flag_type spell_position_flags[] =
{
    {   "dead",       	POS_DEAD,            	TRUE    },
    {   "sleeping",     POS_SLEEPING,        	TRUE    },
    {   "resting",      POS_RESTING,         	TRUE    },
    {   "sitting",      POS_SITTING,         	TRUE    },
    {   "standing",     POS_STANDING,        	TRUE    },
    {   NULL,           0,                    	0       }
};


const struct flag_type position_flags[] =
{
    {   "dead",       	POS_DEAD,            	FALSE   },
    {   "mortal",       POS_MORTAL,          	FALSE   },
    {   "incap",        POS_INCAP,           	FALSE   },
    {   "stunned",      POS_STUNNED,         	FALSE   },
    {   "sleeping",     POS_SLEEPING,        	TRUE    },
    {   "resting",      POS_RESTING,         	TRUE    },
    {   "sitting",      POS_SITTING,         	TRUE    },
    {   "fighting",     POS_FIGHTING,        	FALSE   },
    {   "standing",     POS_STANDING,        	TRUE    },
// @@@NIB : 20070120 : Added for use in CHK_POS ifcheck
    {   "feigned",	POS_FEIGN,        	TRUE    },	// @@@NIB : 20070126 : made selectable in OLC
    {   "heldup",	POS_HELDUP,        	FALSE   },
    {   NULL,           0,                    	0       }
};


const struct flag_type portal_flags[]=
{
    {	"arearandom",		GATE_AREARANDOM,	TRUE	},
    {	"candragitems",		GATE_CANDRAGITEMS,	TRUE	},
    {	"force_brief",		GATE_FORCE_BRIEF,	TRUE	},
    {	"gravity",			GATE_GRAVITY,		FALSE	},	// @@@NIB : 20070126 : Not imped yet
    {	"no_curse",			GATE_NOCURSE,		TRUE	},
    {	"noprivacy",		GATE_NOPRIVACY,		TRUE	},	// @@@NIB : 20070126
    {	"nosneak",			GATE_NOSNEAK,		TRUE	},	// @@@NIB : 20070126
    {	"random",			GATE_RANDOM,		TRUE	},
    {	"safe",				GATE_SAFE,			FALSE	},	// @@@NIB : 20070126 : Not imped yet
    {	"silententry",		GATE_SILENTENTRY,	TRUE	},	// @@@NIB : 20070126
    {	"silentexit",		GATE_SILENTEXIT,	TRUE	},	// @@@NIB : 20070126
    {	"sneak",			GATE_SNEAK,			TRUE	},	// @@@NIB : 20070126
    {	"turbulent",		GATE_TURBULENT,		FALSE	},	// @@@NIB : 20070126 : Not imped yet
    {   "buggy",			GATE_BUGGY,			TRUE	},
    {   "go_with",			GATE_GOWITH,		TRUE	},
    {   "normal_exit",		GATE_NORMAL_EXIT,	TRUE	},
    {   NULL,		0,			0	}
};


const struct flag_type furniture_flags[]=
{
    {   "above",    FURNITURE_ABOVE, TRUE },
    {   "at",       FURNITURE_AT, TRUE },
    {   "in",       FURNITURE_IN, TRUE },
    {   "on",       FURNITURE_ON, TRUE },
    {   "under",    FURNITURE_UNDER, TRUE },
    {	NULL,		0,		0	}
};


const	struct	flag_type	apply_types	[]	=
{
    {	"affects",	TO_AFFECTS,	TRUE	},
    {	"object",	TO_OBJECT,	TRUE	},
    {	"object2",	TO_OBJECT2,	TRUE	},
    {	"object3",	TO_OBJECT3,	TRUE	},
    {	"object4",	TO_OBJECT4,	TRUE	},
    {	"immune",	TO_IMMUNE,	TRUE	},
    {	"resist",	TO_RESIST,	TRUE	},
    {	"vuln",		TO_VULN,	TRUE	},
    {	"weapon",	TO_WEAPON,	TRUE	},
    {	NULL,		0,		TRUE	}
};

const	struct	flag_type	food_buff_types	[]	=
{
    {	"affects",	TO_AFFECTS,	TRUE	},
    {	"immune",	TO_IMMUNE,	TRUE	},
    {	"resist",	TO_RESIST,	TRUE	},
    {	"vuln",		TO_VULN,	TRUE	},
    {	NULL,		0,		TRUE	}
};


const	struct	bit_type	bitvector_type	[]	=
{
    {	affect_flags,	"affect"		},
    {	apply_flags,	"apply"			},
    {	imm_flags,	"imm"			},
    {	res_flags,	"res"			},
    {	vuln_flags,	"vuln"			},
    {	weapon_type2,	"weapon"		}
};


const struct exp_table exp_per_level_table[] =
{
    {250}, // 1
    {500}, // 2
    {1000}, // 3
    {2500}, // 4
    {3000}, // 5
    {4000}, // 6
    {7500}, // 7
    {8000}, // 8
    {9000}, // 9
    {10000}, // 10
    {12500}, // 11
    {15000}, // 12
    {17500}, // 13
    {18500}, // 14
    {20500}, // 15
    {25000}, // 16
    {40000}, // 17
    {60000}, // 18
    {90000}, // 19
    {100000}, // 20
    {120000}, // 21
    {130000}, // 22
    {130000}, // 23
    {150000}, // 24
    {175000}, // 25
    {250000}, // 26
    {500000}, // 27
    {750000}, // 28
    {850000}, // 29
    {1000000}, // 30

    {5000}, // 1
    {7500}, // 2
    {10000}, // 3
    {12500}, // 4
    {15000}, // 5
    {19000}, // 6
    {22000}, // 7
    {30000}, // 8
    {40000}, // 9
    {80000}, // 10
    {100000}, // 11
    {150000}, // 12
    {200000}, // 13
    {300000}, // 14
    {400000}, // 15
    {500000}, // 16
    {600000}, // 17
    {700000}, // 18
    {800000}, // 19
    {900000}, // 20
    {1000000}, // 21
    {1250000}, // 22
    {1500000}, // 23
    {1750000}, // 24
    {2000000}, // 25
    {2250000}, // 26
    {2500000}, // 27
    {2750000}, // 28
    {3000000}, // 29
    {3000000}, // 30

    {2500}, // 1
    {5000}, // 2
    {15000}, // 3
    {25000}, // 4
    {50000}, // 5
    {75000}, // 6
    {150000}, // 7
    {250000}, // 8
    {300000}, // 9
    {400000}, // 10
    {500000}, // 11
    {750000}, // 12
    {1000000}, // 13
    {1250000}, // 14
    {1500000}, // 15
    {2500000}, // 16
    {2750000}, // 17
    {3250000}, // 18
    {4250000}, // 19
    {5750000}, // 20
    {6250000}, // 21
    {6500000}, // 22
    {7000000}, // 23
    {7250000}, // 24
    {7750000}, // 25
    {8250000}, // 26
    {8500000}, // 27
    {8750000}, // 28
    {9000000}, // 29
    {9000000}, // 30

    {2500}, // 1
    {5000}, // 2
    {15000}, // 3
    {25000}, // 4
    {45000}, // 5
    {50000}, // 6
    {75000}, // 7
    {125000}, // 8
    {250000}, // 9
    {500000}, // 10
    {750000}, // 11
    {1000000}, // 12
    {1500000}, // 13
    {2500000}, // 14
    {3500000}, // 15
    {4500000}, // 16
    {5500000}, // 17
    {6500000}, // 18
    {7500000}, // 19
    {8500000}, // 20
    {9500000}, // 21
    {10000000}, // 22
    {10500000}, // 23
    {11000000}, // 24
    {12000000}, // 25
    {12500000}, // 26
    {13000000}, // 27
    {13500000}, // 28
    {14500000}, // 29
    {15000000} //  30
};


const struct flag_type room_condition_flags[] =
{
    { 	"season",	CONDITION_SEASON,	TRUE	},
    { 	"sky",		CONDITION_SKY,		TRUE    },
    { 	"hour",		CONDITION_HOUR,		TRUE	},
    { 	"script",	CONDITION_SCRIPT,	TRUE	},
    {	NULL,		0,			TRUE    }
};


const struct npc_ship_hotspot_type npc_ship_hotspot_table[] =
{
    {   427, 208,           1 },//NPC_SHIP_OLARIA_SAILING },
    {   0,   0,             0 }
};


const struct flag_type      place_flags[]           =
{
    { 	"Nowhere",    			PLACE_NOWHERE, 				TRUE	},
    { 	"Wilderness",			PLACE_WILDERNESS,			TRUE	},
    { 	"First Continent", 		PLACE_FIRST_CONTINENT, 		TRUE	},
    { 	"Second Continent", 	PLACE_SECOND_CONTINENT, 	TRUE	},
    { 	"Third Continent",	 	PLACE_THIRD_CONTINENT, 		TRUE	},
    { 	"Fourth Continent", 	PLACE_FOURTH_CONTINENT, 	TRUE	},
    { 	"Island", 				PLACE_ISLAND, 				TRUE	},
    { 	"Other Plane",			PLACE_OTHER_PLANE,			TRUE	},
    { 	"Abyss",				PLACE_ABYSS,				TRUE	},
    { 	"Eden",					PLACE_EDEN,					TRUE	},
    { 	"Netherworld",			PLACE_NETHERWORLD,			TRUE	},
    {  	NULL,			0, 				FALSE 	}
};


const struct flag_type	token_flags[] =
{
    {	"purge_death",		TOKEN_PURGE_DEATH,		TRUE	},
    {	"purge_idle",		TOKEN_PURGE_IDLE,		TRUE	},
    {	"purge_quit",		TOKEN_PURGE_QUIT,		TRUE	},
    {	"purge_reboot",		TOKEN_PURGE_REBOOT,		TRUE	},
    {	"purge_rift",		TOKEN_PURGE_RIFT,		TRUE	},
    {	"reverse_timer",	TOKEN_REVERSETIMER,		TRUE	},
    {	"no_skill_test",	TOKEN_NOSKILLTEST,		TRUE	},
    {	"singular",			TOKEN_SINGULAR,			TRUE	},
    {	"see_all",			TOKEN_SEE_ALL,			TRUE	},
    {	"permanent",		TOKEN_PERMANENT,		TRUE	},
    {	"spellbeats",		TOKEN_SPELLBEATS,		TRUE	},
    {	NULL,			0,				FALSE	}
};


const struct flag_type  channel_flags[] =
{
    {	"ct",			FLAG_CT,		TRUE	},
    {	"flame",		FLAG_FLAMING,		TRUE	},
    {	"gossip",		FLAG_GOSSIP,		TRUE	},
    {	"helper",		FLAG_HELPER,		TRUE	},
    {	"music",		FLAG_MUSIC,		TRUE	},
    {	"ooc",			FLAG_OOC,		TRUE	},
    {	"quote",		FLAG_QUOTE,		TRUE	},
    {	"tells",		FLAG_TELLS,		TRUE	},
    {	"yell",			FLAG_YELL,		TRUE	},
    {	NULL,			0,				FALSE	}
};


const struct flag_type project_flags[] =
{
    {	"open",			PROJECT_OPEN,		TRUE	},
    {	"assigned",		PROJECT_ASSIGNED,	TRUE	},
    {	"hold",			PROJECT_HOLD,		TRUE	},
    {	NULL,			0,			FALSE	}
};


const struct flag_type immortal_flags[] =
{
    {	"Head Coder",		IMMORTAL_HEADCODER,	TRUE	},
    {	"Head Builder",		IMMORTAL_HEADBUILDER, 	TRUE	},
    {	"Administrator",	IMMORTAL_HEADADMIN, 	TRUE	},
    {	"Staff",		IMMORTAL_STAFF,		TRUE	},
    {	"Balance",		IMMORTAL_HEADBALANCE,	TRUE	},
    {	"Player Relations",	IMMORTAL_HEADPR,	TRUE	},
    {	"Coder",		IMMORTAL_CODER,		TRUE	},
    {	"Builder",		IMMORTAL_BUILDER,	TRUE	},
    {	"Housing",		IMMORTAL_HOUSING,	TRUE	},
    {	"Churches",		IMMORTAL_CHURCHES,	TRUE	},
    {	"Helpfiles",		IMMORTAL_HELPFILES,	TRUE	},
    {	"Weddings",		IMMORTAL_WEDDINGS,	TRUE	},
    {	"Testing",		IMMORTAL_TESTING,	TRUE	},
    {	"Events",		IMMORTAL_EVENTS,	TRUE	},
    {	"Website",		IMMORTAL_WEBSITE,	TRUE	},
    {	"Story",		IMMORTAL_STORY,		TRUE	},
    {	"Secretary",		IMMORTAL_SECRETARY,	TRUE	},
    {	NULL,			0,			FALSE	}
};

// @@@NIB : 20070120 : Added this matching IMM/RES/VULN to DAM_* codes
// Used by damage_class_lookup()
// Gets the DAM_* code for the given damage class
const struct flag_type damage_classes[] = {
	{"acid", DAM_ACID, TRUE},
	{"air", DAM_AIR, TRUE},
	{"bash", DAM_BASH, TRUE},
	{"charm", DAM_CHARM, TRUE},
	{"cold", DAM_COLD, TRUE},
	{"disease", DAM_DISEASE, TRUE},
	{"drowning", DAM_DROWNING, TRUE},
	{"earth", DAM_EARTH, TRUE},
	{"energy", DAM_ENERGY, TRUE},
	{"fire", DAM_FIRE, TRUE},
	{"holy", DAM_HOLY, TRUE},
	{"light", DAM_LIGHT, TRUE},
	{"lightning", DAM_LIGHTNING, TRUE},
	{"magic", DAM_MAGIC, TRUE},
	{"mental", DAM_MENTAL, TRUE},
	{"negative", DAM_NEGATIVE, TRUE},
	{"pierce", DAM_PIERCE, TRUE},
	{"plant", DAM_PLANT, TRUE},
	{"poison", DAM_POISON, TRUE},
	{"slash", DAM_SLASH, TRUE},
	{"sound", DAM_SOUND, TRUE},
	{"water", DAM_WATER, TRUE},
    {"suffocating", DAM_SUFFOCATING, TRUE},
	{NULL, 0, 0}
};

const struct flag_type corpse_types[] = {
	{"charred",RAWKILL_CHARRED,TRUE},
	{"dissolve",RAWKILL_DISSOLVE,TRUE},
	{"explode",RAWKILL_EXPLODE,TRUE},
	{"flay",RAWKILL_FLAY,TRUE},
	{"frozen",RAWKILL_FROZEN,TRUE},
	{"iceblock",RAWKILL_ICEBLOCK,TRUE},
	{"incinerate",RAWKILL_INCINERATE,TRUE},
	{"melted",RAWKILL_MELTED,TRUE},
	{"nocorpse",RAWKILL_NOCORPSE,TRUE},
	{"normal",RAWKILL_NORMAL,TRUE},
	{"shatter",RAWKILL_SHATTER,TRUE},
	{"skeletal",RAWKILL_SKELETAL,TRUE},
	{"stone",RAWKILL_STONE,TRUE},
	{"withered",RAWKILL_WITHERED,TRUE},
	{NULL, 0, 0},
};

const struct corpse_info corpse_info_table[] = {
	{	// RAWKILL_NORMAL
		"corpse %s",
		"the corpse of %s",
		"{yThe corpse of %s is lying here.{x",
		"The corpse of %s is lying here.",
		"the headless corpse of %s",
		"{yThe headless corpse of %s is lying here.{x",
		"The headless corpse of %s is lying here.",
		"animated corpse %s",
		"The animated corpse of %s staggers around here.\n\r",
		"It is decaying and quite smelly.\n\r",
		NULL,
		NULL,
		"$p dries into a withered husk.",
		"{RYou reach down and rip out the skull of %s.{x",
		"{RWith the sound of ripping flesh, $n rips out the skull of %s!{x",
		"{RYou try to remove the skull from $p, but mutilate it in the process.{x",
		"{R$n tries to remove the skull from $p, but mutilates it in the process.{x",
		FALSE,FALSE,TRUE,100,100,100,RAWKILL_WITHERED,0,3,6,1,3,100,
		0
	}, {	// RAWKILL_CHARRED
		"corpse %s",
		"the charred corpse of %s",
		"{DThe charred corpse of %s is lying here.{x",
		"The charred corpse of %s is lying here.",
		"the headless charred corpse of %s",
		"{DThe headless charred corpse of %s is lying here.{x",
		"The headless charred corpse of %s is lying here.",
		"animated charred corpse %s",
		"The charred corpse of %s hobbles about.\n\r",
		"The smell of charred flesh fills the air.\n\r",
		"{YThe last thing you feel is your flesh charring...{x",
		"{DThe flesh on $n chars to blackened crisp.{x",
		"$p decays into dust.",
		"{DYou reach down and rip out the skull of %s, snapping burned flesh away.{x",
		"{DWith burned flesh snapping away, $n rips out the skull of %s!{x",
		"{DYou try to remove the skull from $p, but cause it to crumble into ashes!{x",
		"{D$n tries to remove the skull from $p, but causes it to crumble into ashes!{x",
		FALSE,FALSE,TRUE,45,100,90,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		0
	}, {	// RAWKILL_FROZEN
		"corpse %s",
		"the frozen corpse of %s",
		"{cThe frozen corpse of %s is lying here.{x",
		"The frozen corpse of %s is lying here.",
		"the headless frozen corpse of %s",
		"{cThe headless frozen corpse of %s is lying here.{x",
		"The headless frozen corpse of %s is lying here.",
		"animated frozen corpse %s",
		"The frozen corpse of %s lurches around here.\n\r",
		"Frost covers its skin, chilling the air as it lurches about.\n\r",
		NULL,
		"{C$n's body freezes over.{x",
		"$p decays into dust.",
		"{CYou reach down and rip out the skull of %s.{x",
		"{CWith the sound of ice breaking, $n rips out the skull of %s!{x",
		"{CYou try to remove the skull from $p, but it shatters from being disturbed.{x",
		"{C$n tries to remove the skull from $p, but it shatters from being disturbed.{x",
		FALSE,FALSE,TRUE,80,100,70,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		0
	}, {	// RAWKILL_MELTED
		"corpse %s",
		"the corpse of %s",
		"{gThe corpse of %s is lying here, partially melted.{x",
		"The corpse of %s is lying here, partially melted.",
		"the headless corpse of %s",
		"{gThe headless corpse of %s is lying here, partially melted.{x",
		"The corpse of %s is lying here, partially melted.",
		"animated corpse %s",
		"The corpse of %s staggers around here with flesh oozing off.\n\r",
		"Melted by a mighty blast of acid, the flesh on this corpse slowly oozes off\n\r"
		"in disgusting rivulets.\n\r",
		"{GThe last thing you feel is your flesh melting...{x",
		"{GThe flesh on $n oozes off as it melts!{x",
		"$p decays into dust.",
		"{RYou reach down and rip out the skull of %s.{x",
		"{RWith the sound of ripping flesh, $n rips out the skull of %s!{x",
		"{RYou try to remove the skull from $p, but mutilate it in the process.{x",
		"{R$n tries to remove the skull from $p, but mutilates it in the process.{x",
		FALSE,FALSE,TRUE,50,100,90,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		PART_HIDE
	}, {	// RAWKILL_WITHERED
		"corpse %s",
		"the corpse of %s",
		"{xThe withered corpse of %s is lying here.{x",
		"The corpse of %s is lying here, completely withered.",
		"the headless corpse of %s",
		"{xThe headless withered corpse of %s is lying here.{x",
		"The corpse of %s is lying here, completely withered.",
		"animated withered corpse %s",
		"The withered husk of %s staggers about with skin flaking off.\n\r",
		"Dried to a husk, this corpse looks as if it will disintegrate within moments.\n\r"
		"Skin flakes off with each step made.\n\r",
		NULL,
		"{W$n withers into a dried husk!{x",
		"The flesh on $p crumbles into a pile of dust, leaving behind a skeleton.",
		"{yYou reach down and rip out the skull of %s, with dust and dried skin falling away.{x",
		"{yWith dust and dried skin falling away, $n rips out the skull of %s!{x",
		"{yYou try to remove the skull from $p, but cause it to crumble into dust!{x",
		"{y$n tries to remove the skull from $p, but causes it to crumble into dust!{x",
		FALSE,FALSE,TRUE,50,100,30,RAWKILL_SKELETAL,0,3,6,1,3,100,
		0
	}, {	// RAWKILL_ICEBLOCK
		"iceblock %s",
		"%s encased in ice",
		"{WThe body of %s is trapped in a block of ice.{x",
		"Entombed in a block of ice, the body of %s remains trapped in a frozen stasis.",
		"%s encased in ice",
		"{WThe headless body of %s is trapped in a block of ice.{x",
		"Entombed in a block of ice, the headless body of %s remains trapped in a frozen stasis.",
		NULL,
		NULL,
		NULL,
		"{WEverything goes white as a wall of ice surrounds you!{x",
		"{W$n turns into a block of ice!{x",
		"$p crumbles away.",
		NULL,
		NULL,
		NULL,
		NULL,
		TRUE,FALSE,FALSE,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		0
	}, {	// RAWKILL_INCINERATE
		"ashes %s",
		"the ashes of %s",
		"{DThe burning ashes of %s lie in a pile, ready to blow away.{x",
		"{DThe burning ashes of %s lie in a pile, ready to blow away.{x",
		"the ashes of %s",
		"{DThe burning ashes of %s lie in a pile, ready to blow away.{x",
		"{DThe burning ashes of %s lie in a pile, ready to blow away.{x",
		NULL,
		NULL,
		NULL,
		"{YThe last thing you feel is your flesh charring...{x",
		"{RFlames consume $n!{x",
		"$p blow away...",
		NULL,
		NULL,
		NULL,
		NULL,
		FALSE,TRUE,FALSE,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		-1
	}, {	// RAWKILL_STONE
		"statue %s",
		"the statue of %s",
		"{xThe body of %s stands here stoned.",
		"The body of %s remains trapped as a petrified statue.",
		"the headless statue of %s",
		"{xThe headless body of %s stands here stoned.",
		"The headless body of %s remains trapped as a petrified statue.",
		"animated statue %s",
		"The petrified visage of %s lurches about slowly.",
		"Prefectly preserved, the flesh of stone holds static everything that mars\n\r"
		"its stoney surface.\n\r",
		"{DEverything goes black suddenly...{x",
		"{x$n turns into a stone statue!",
		"$p crumbles into dust.",
		NULL,
		NULL,
		NULL,
		NULL,
		TRUE,FALSE,TRUE,0,10,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		0
	}, {	// RAWKILL_SHATTER
		"remains %s",
		"the shattered remains of %s",
		"The shattered remains of %s lay scattered about.",
		"The shattered remains of %s lay scattered about.",
		"the shattered remains of %s",
		"The shattered remains of %s lay scattered about.",
		"The shattered remains of %s lay scattered about.",
		NULL,
		NULL,
		NULL,
		"{WYou feel your body exploding...{x",
		"{W$n shatters!{x",
		"$p decays into dust.",
		NULL,
		NULL,
		NULL,
		NULL,
		FALSE,TRUE,FALSE,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		-1
	}, {	// RAWKILL_EXPLODE
		"remains %s",
		"the remains of %s",
		"{rThe bloody remains of %s lie in a pile.{x",
		"{DThe bloody remains of %s lie in a pile.{x",
		"the remains of %s",
		"{rThe bloody remains of %s lie in a pile.{x",
		"{DThe bloody remains of %s lie in a pile.{x",
		NULL,
		NULL,
		NULL,
		"{RYou feel your body exploding...{x",
		"{R$n's body explodes into a bloody mess!{x",
		"$p decays into dust.",
		NULL,
		NULL,
		NULL,
		NULL,
		FALSE,TRUE,FALSE,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		-1
	}, {	// RAWKILL_DISSOLVE
		"gooey mess %s",
		"the gooey mess of %s",
		"{gThe gooey mess of %s puddles here.{x",
		"What used to be the body of %s now exists as a gooey puddle.",
		"the gooey mess of %s",
		"{gThe gooey mess of %s puddles here.{x",
		"What used to be the body of %s now exists as a gooey puddle.",
		NULL,
		NULL,
		NULL,
		"{GYou feel your body dissolving...{x",
		"{G$n's body dissolves into a gooey mess!{x",
		"$p dries up, leaving a stain.",
		NULL,
		NULL,
		NULL,
		NULL,
		FALSE,TRUE,FALSE,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		-1
	}, {	// RAWKILL_SKELETAL
		"skeleton %s",
		"the skeleton of %s",
		"{xThe skeleton of %s is lying here.{x",
		"{xThe skeleton of %s is lying here.{x",
		"the skeleton of %s",
		"{xThe skeleton of %s is lying here.{x",
		"{xThe skeleton of %s is lying here.{x",
		"animated skeleton %s",
		"The animated skeleton of %s lurches around here.\n\r",
		"Dark recesses in the skull penetrate back, carrying with their stare their\n\r"
		"soulless presence.\n\r",
		NULL,
		"{WThe flesh of $n disintegrates immediately, leaving behind a skeleton!{x",
		"$p collapses into a pile of bones.",
		"{RYou reach down and rip off the skull of %s.{x",
		"{RWith the sound of bones snapping, $n rips off the skull of %s!{x",
		"{RYou try to remove the skull from $p, but mutilate it in the process.{x",
		"{R$n tries to remove the skull from $p, but mutilates it in the process.{x",
		FALSE,FALSE,TRUE,0,100,100,RAWKILL_NOCORPSE,0,3,6,25,40,100,
		PART_HIDE|PART_GUTS|PART_EYE|PART_EAR|PART_BRAINS|PART_HEART|PART_LONG_TONGUE|PART_EYESTALKS|PART_TENTACLES|PART_SCALES
	}, {	// RAWKILL_FLAY
		"corpse %s",
		"the corpse of %s",
		"{rThe corpse of %s is lying here, missing all traces of skin.{x",
		"All the skin has been flayed from the corpse of %s.",
		"the headless corpse of %s",
		"{rThe headless corpse of %s is lying here, missing all traces of skin.{x",
		"All the skin has been flayed from the headless corpse of %s.",
		"animated corpse %s",
		"The animated corpse of %s staggers around here, missing all manner of skin.\n\r",
		"It is decaying and quite smelly.\n\r",
		NULL,
		"{RAll the skin peels away from $n!{x",
		"The flesh on $p rots away, leaving behind a skeleton.",
		"{RYou reach down and rip out the skull of %s.{x",
		"{RWith the sound of ripping flesh, $n rips out the skull of %s!{x",
		"{RYou try to remove the skull from $p, but mutilate it in the process.{x",
		"{R$n tries to remove the skull from $p, but mutilates it in the process.{x",
		FALSE,FALSE,FALSE,80,100,95,RAWKILL_SKELETAL,0,3,6,1,5,100,
		PART_HIDE|PART_SCALES
	},
};

const struct flag_type time_of_day_flags[] = {
	{ "aftermidnight",	TOD_AFTERMIDNIGHT, TRUE},
	{ "afternoon",		TOD_AFTERNOON, TRUE},
	{ "dawn",		TOD_DAWN, TRUE},
	{ "day",		TOD_DAY, TRUE},
	{ "dusk",		TOD_DUSK, TRUE},
	{ "evening",		TOD_EVENING, TRUE},
	{ "midnight",		TOD_MIDNIGHT, TRUE},
	{ "morning",		TOD_MORNING, TRUE},
	{ "night",		TOD_NIGHT, TRUE},
	{ "noon",		TOD_NOON, TRUE},
	{ NULL,			0, FALSE},
};

const struct flag_type death_types[] = {
	 { "alive",	DEATHTYPE_ALIVE, TRUE},
	 { "attack",	DEATHTYPE_ATTACK, TRUE},
	 { "behead",	DEATHTYPE_BEHEAD, TRUE},
	 { "breath",	DEATHTYPE_BREATH, TRUE},
	 { "damage",	DEATHTYPE_DAMAGE, TRUE},
	 { "killspell",	DEATHTYPE_KILLSPELL, TRUE},
	 { "magic",	DEATHTYPE_MAGIC, TRUE},
	 { "rawkill",	DEATHTYPE_RAWKILL, TRUE},
	 { "rocks",	DEATHTYPE_ROCKS, TRUE},
	 { "slit",	DEATHTYPE_SLIT, TRUE},
	 { "smite",	DEATHTYPE_SMITE, TRUE},
	 { "stake",	DEATHTYPE_STAKE, TRUE},
	 { "toxin",	DEATHTYPE_TOXIN, TRUE},
//	 { "trap",	DEATHTYPE_TRAP, TRUE},
	{ NULL,			0, FALSE},
};

const struct flag_type tool_types[] = {
	{ "none",		TOOL_NONE, TRUE },
	{ "whetstone",		TOOL_WHETSTONE, TRUE },
	{ "chisel",		TOOL_CHISEL, TRUE },
	{ "pick",		TOOL_PICK, TRUE },
	{ "shovel",		TOOL_SHOVEL, TRUE },
	{ "tinderbox",		TOOL_TINDERBOX, TRUE },
	{ "drying_cloth",	TOOL_DRYING_CLOTH, TRUE },
	{ "small_needle",	TOOL_SMALL_NEEDLE, TRUE },
	{ "large_needle",	TOOL_LARGE_NEEDLE, TRUE },
	{ NULL,			0, FALSE },
};

const struct flag_type catalyst_types[] = {
	{ "none",	CATALYST_NONE, TRUE},
	{ "acid",	CATALYST_ACID, TRUE},
	{ "air",	CATALYST_AIR, TRUE},
	{ "astral",	CATALYST_ASTRAL, TRUE},
	{ "blood",	CATALYST_BLOOD, TRUE},
	{ "body",	CATALYST_BODY, TRUE},
	{ "chaos",	CATALYST_CHAOS, TRUE},
	{ "cosmic",	CATALYST_COSMIC, TRUE},
	{ "darkness",	CATALYST_DARKNESS, TRUE},
	{ "death",	CATALYST_DEATH, TRUE},
	{ "earth",	CATALYST_EARTH, TRUE},
	{ "energy",	CATALYST_ENERGY, TRUE},
	{ "fire",	CATALYST_FIRE, TRUE},
	{ "holy",	CATALYST_HOLY, TRUE},
	{ "ice",	CATALYST_ICE, TRUE},
	{ "law",	CATALYST_LAW, TRUE},
	{ "light",	CATALYST_LIGHT, TRUE},
	{ "mana",	CATALYST_MANA, TRUE},
	{ "metallic",	CATALYST_METALLIC, TRUE},
	{ "mind",	CATALYST_MIND, TRUE},
	{ "nature",	CATALYST_NATURE, TRUE},
	{ "shock",	CATALYST_SHOCK, TRUE},
	{ "soul",	CATALYST_SOUL, TRUE},
	{ "sound",	CATALYST_SOUND, TRUE},
	{ "toxin",	CATALYST_TOXIN, TRUE},
	{ "water",	CATALYST_WATER, TRUE},
	{ NULL,		0, FALSE },
};


const char *catalyst_descs[] = {
	"unknown",
	"acidic",
	"windy",
	"astral",
	"bloody",
	"physical",
	"chaotic",
	"cosmic",
	"deathly",
	"earthly",
	"energetic",
	"firey",
	"holy",
	"icy",
	"lawful",
	"solar",
	"mental",
	"natural",
	"sonic",
	"toxic",
	"watery",
    "electric",
    "void",
    "metallic",
    "magical",
    "soul",
};

const struct flag_type catalyst_method_types[] = {
	{ "carry",	CATALYST_CARRY, TRUE},
	{ "room",	CATALYST_ROOM, TRUE},
	{ "hold",	CATALYST_HOLD, TRUE},
	{ "containers",	CATALYST_CONTAINERS, TRUE},
	{ "worn",	CATALYST_WORN, TRUE},
	{ "active",	CATALYST_ACTIVE, TRUE},
	{ NULL,		0, FALSE },
};

const struct flag_type boolean_types[] = {
	{ "true",	TRUE, TRUE},
	{ "false",	FALSE, TRUE},
	{ "yes",	TRUE, TRUE},
	{ "no",		FALSE, TRUE},
	{ NULL,		0, FALSE },
};

const int dam_to_corpse[DAM_MAX][11] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_NONE
	{ 8, 1, RAWKILL_EXPLODE, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_BASH
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_PIERCE
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_SLASH
	{ 8, 3, RAWKILL_CHARRED, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_FIRE
	{ 8, 3, RAWKILL_FROZEN, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_COLD
	{ 16, 1, RAWKILL_INCINERATE, 5, RAWKILL_CHARRED, 0, 0, 0, 0, 0, 0 }, // DAM_LIGHTNING
	{ 16, 1, RAWKILL_DISSOLVE, 5, RAWKILL_MELTED, 0, 0, 0, 0, 0, 0 }, // DAM_ACID
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_POISON
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_NEGATIVE
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_HOLY
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_ENERGY
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_MENTAL
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_DISEASE
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_DROWNING
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_LIGHT
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_OTHER
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_HARM
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_CHARM
	{ 100, 2, RAWKILL_SHATTER, 5, RAWKILL_EXPLODE, 0, 0, 0, 0, 0, 0 }, // DAM_SOUND
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_BITE
	{ 100, 100, RAWKILL_EXPLODE, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_VORPAL
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_BACKSTAB
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_MAGIC
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_WATER
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_EARTH
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_PLANT
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_AIR
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_SUFFOCATING
};

// When this becomes a linked list..... yeah
#define BLEND(a,b,c)	{ RAWKILL_##a, RAWKILL_##b, RAWKILL_##c, FALSE }
#define BLENDD(a,b,c)	{ RAWKILL_##a, RAWKILL_##b, RAWKILL_##c, TRUE }
const struct corpse_blend_type corpse_blending[] = {
	BLENDD(FLAY,WITHERED,WITHERED),
	BLENDD(ICEBLOCK,FROZEN,ICEBLOCK),
	BLENDD(ICEBLOCK,CHARRED,NORMAL),
	BLENDD(ICEBLOCK,INCINERATE,CHARRED),
	BLENDD(EXPLODE,SHATTER,NOCORPSE),
	BLENDD(EXPLODE,INCINERATE,NOCORPSE),
	BLENDD(EXPLODE,DISSOLVE,NOCORPSE),
	BLENDD(EXPLODE,ANY,EXPLODE),
	BLENDD(SHATTER,INCINERATE,NOCORPSE),
	BLENDD(SHATTER,DISSOLVE,NOCORPSE),
	BLENDD(SHATTER,ANY,SHATTER),
	BLENDD(INCINERATE,DISSOLVE,NOCORPSE),
	BLENDD(INCINERATE,ANY,INCINERATE),
	BLENDD(DISSOLVE,ANY,DISSOLVE),
	BLENDD(NORMAL,ANY,TYPE2),
	BLENDD(NOCORPSE,ANY,NOCORPSE),
	BLEND(ANY,ANY,TYPE1)
};


const struct flag_type tattoo_loc_flags[] =
{
    {	"head",		WEAR_TATTOO_HEAD,	TRUE	},
    {	"body",		WEAR_TATTOO_TORSO,	TRUE	},
    {	"torso",	WEAR_TATTOO_TORSO,	TRUE	},
    {	"upper_left_arm",	WEAR_TATTOO_UPPER_ARM_L,	TRUE	},
    {	"upper_right_arm",	WEAR_TATTOO_UPPER_ARM_R,	TRUE	},
    {	"upper_left_leg",	WEAR_TATTOO_UPPER_LEG_L,	TRUE	},
    {	"upper_right_leg",	WEAR_TATTOO_UPPER_LEG_R,	TRUE	},
    {   "lower_left_arm",         WEAR_TATTOO_LOWER_ARM_L,       TRUE    },
    {   "lower_right_arm",         WEAR_TATTOO_LOWER_ARM_R,      TRUE    },
    {   "lower_left_leg",        WEAR_TATTOO_LOWER_LEG_L,      TRUE    },
    {   "lower_right_leg",       WEAR_TATTOO_LOWER_LEG_R,        TRUE    },
    {   "left_shoulder",      WEAR_TATTOO_SHOULDER_L,        TRUE    },
    {   "right_shoulder",       WEAR_TATTOO_SHOULDER_R,        TRUE    },
    {   "back",      WEAR_TATTOO_BACK,        TRUE    },
    {   "neck",      WEAR_TATTOO_NECK,        TRUE    },
    {	NULL,		0,		0	}
};


const struct flag_type affgroup_mobile_flags[] =
{
	{ "racial",	AFFGROUP_RACIAL,	TRUE	},
	{ "metaracial",	AFFGROUP_METARACIAL,	TRUE	},
	{ "biological",	AFFGROUP_BIOLOGICAL,	TRUE	},
	{ "mental",	AFFGROUP_MENTAL,	TRUE	},
	{ "divine",	AFFGROUP_DIVINE,	TRUE	},
	{ "magical",	AFFGROUP_MAGICAL,	TRUE	},
	{ "physical",	AFFGROUP_PHYSICAL,	TRUE	},
	{ NULL,		0,			FALSE	}
};

const struct flag_type affgroup_object_flags[] =
{
	{ "inherent",	AFFGROUP_INHERENT,	TRUE	},
	{ "enchant",	AFFGROUP_ENCHANT,	TRUE	},
	{ "weapon",	AFFGROUP_WEAPON,	TRUE	},
	{ "portal",	AFFGROUP_PORTAL,	TRUE	},
	{ "container",	AFFGROUP_CONTAINER,	TRUE	},
	{ NULL,		0,			FALSE	}
};

const struct flag_type spell_target_types[] = {
	{ "defensive",		TAR_CHAR_DEFENSIVE,	TRUE	},
	{ "formation",		TAR_CHAR_FORMATION,	TRUE	},
	{ "ignore",		TAR_IGNORE,		TRUE	},
	{ "inventory",		TAR_OBJ_INV,		TRUE	},
	{ "char_world",		TAR_IGNORE_CHAR_DEF,	TRUE	},
	{ "obj_char_off",	TAR_OBJ_CHAR_OFF,	TRUE	},
	{ "obj_defensive",	TAR_OBJ_CHAR_DEF,	TRUE	},
	{ "obj_ground",		TAR_OBJ_GROUND,		TRUE	},
	{ "offensive",		TAR_CHAR_OFFENSIVE,	TRUE	},
	{ "self",		TAR_CHAR_SELF,		TRUE	},
	{ NULL,			0,			FALSE	}
};

const struct flag_type song_target_types[] = {
	{ "defensive",		TAR_CHAR_DEFENSIVE,	TRUE	},
	{ "formation",		TAR_CHAR_FORMATION,	TRUE	},
	{ "ignore",			TAR_IGNORE,		TRUE	},
	{ "obj_char_off",	TAR_OBJ_CHAR_OFF,	TRUE	},
	{ "obj_defensive",	TAR_OBJ_CHAR_DEF,	TRUE	},
	{ "offensive",		TAR_CHAR_OFFENSIVE,	TRUE	},
	{ "self",		TAR_CHAR_SELF,		TRUE	},
	{ NULL,			0,			FALSE	}
};

const struct flag_type moon_phases[] = {
	{ "new",		MOON_NEW,		TRUE	},
	{ "waxing_crescent",	MOON_WAXING_CRESCENT,	TRUE	},
	{ "first_quarter",	MOON_FIRST_QUARTER,	TRUE	},
	{ "waxing_gibbous",	MOON_WAXING_GIBBOUS,	TRUE	},
	{ "full",		MOON_FULL,		TRUE	},
	{ "waning_gibbous",	MOON_WANING_GIBBOUS,	TRUE	},
	{ "last_quarter",	MOON_LAST_QUARTER,	TRUE	},
	{ "waning_crescent",	MOON_WANING_CRESCENT,	TRUE	},
	{ NULL,			0,			FALSE	}
};

const struct flag_type player_conditions[] = {
	{ "drunk",	COND_DRUNK,		TRUE },
	{ "full",	COND_FULL,		TRUE },
	{ "thirst",	COND_THIRST,	TRUE },
	{ "hunger",	COND_HUNGER,	TRUE },
	{ "stoned",	COND_STONED,	TRUE },
	{ NULL,		-1,				FALSE }
};

const struct flag_type instrument_types[] = {
	{ "vocal",		INSTRUMENT_VOCAL,		FALSE },
	{ "any",		INSTRUMENT_ANY,			TRUE },
	{ "none",		INSTRUMENT_NONE,		FALSE },
	{ "reed",		INSTRUMENT_WIND_REED,	TRUE },
	{ "flute",		INSTRUMENT_WIND_FLUTE,	TRUE },
	{ "brass",		INSTRUMENT_WIND_BRASS,	TRUE },
	{ "drum",		INSTRUMENT_DRUM,		TRUE },
	{ "percussion",	INSTRUMENT_PERCUSSION,	TRUE },
	{ "chorded",	INSTRUMENT_CHORDED,		TRUE },
	{ "string",		INSTRUMENT_STRING,		TRUE },
	{ NULL,			0,						FALSE }
};

const struct flag_type instrument_flags[] = {
	{ "onehand",	INSTRUMENT_ONEHANDED,	TRUE	},
	{ NULL,			0,						FALSE }
};

const struct flag_type corpse_object_flags[] = {
	{ "chaotic",	CORPSE_CHAOTICDEATH,	TRUE },
	{ "owner_loot",	CORPSE_OWNERLOOT,		TRUE },
	{ "charred",	CORPSE_CHARRED,			TRUE },
	{ "frozen",		CORPSE_FROZEN,			TRUE },
	{ "melted",		CORPSE_MELTED,			TRUE },
	{ "withered",	CORPSE_WITHERED,		TRUE },
	{ "pk",			CORPSE_PKDEATH,			TRUE },
	{ "arena",		CORPSE_ARENADEATH,		TRUE },
	{ "immortal",	CORPSE_IMMORTAL,		TRUE },
	{ NULL,			0,						FALSE }

};

const struct flag_type variable_types[] = {
	{"bool",			VAR_BOOLEAN,			TRUE},
	{"integer",			VAR_INTEGER,			TRUE},
	{"string",			VAR_STRING,				TRUE},
	{"string_s",		VAR_STRING_S,			TRUE},
	{"room",			VAR_ROOM,				TRUE},
	{"exit",			VAR_EXIT,				TRUE},
	{"mobile",			VAR_MOBILE,				TRUE},
	{"object",			VAR_OBJECT,				TRUE},
	{"token",			VAR_TOKEN,				TRUE},
	{"area",			VAR_AREA,				TRUE},
    {"aregion",         VAR_AREA_REGION,        TRUE},
	{"skill",			VAR_SKILL,				TRUE},
	{"skillinfo",		VAR_SKILLINFO,			TRUE},
    {"song",            VAR_SONG,               TRUE},
	{"connection",		VAR_CONNECTION,			TRUE},
	{"affect",			VAR_AFFECT,				TRUE},
	{"wilds",			VAR_WILDS,				TRUE},
	{"church",			VAR_CHURCH,				TRUE},
	{"clone_room",		VAR_CLONE_ROOM,			TRUE},
	{"wilds_room",		VAR_WILDS_ROOM,			TRUE},
	{"door",			VAR_DOOR,				TRUE},
	{"clone_door",		VAR_CLONE_DOOR,			TRUE},
	{"wilds_door",		VAR_WILDS_DOOR,			TRUE},
	{"mobile_id",		VAR_MOBILE_ID,			TRUE},
	{"object_id",		VAR_OBJECT_ID,			TRUE},
	{"token_id",		VAR_TOKEN_ID,			TRUE},
	{"skillinfo_id",	VAR_SKILLINFO_ID,		TRUE},
	{"area_id",			VAR_AREA_ID,			TRUE},
    {"aregion_id",      VAR_AREA_REGION_ID,     TRUE},
	{"wilds_id",		VAR_WILDS_ID,			TRUE},
	{"church_id",		VAR_CHURCH_ID,			TRUE},
	{"variable",		VAR_VARIABLE,			TRUE},
	{"bllist_room",		VAR_BLLIST_ROOM,		TRUE},
	{"bllist_mob",		VAR_BLLIST_MOB,			TRUE},
	{"bllist_obj",		VAR_BLLIST_OBJ,			TRUE},
	{"bllist_tok",		VAR_BLLIST_TOK,			TRUE},
	{"bllist_exit",		VAR_BLLIST_EXIT,		TRUE},
	{"bllist_skill",	VAR_BLLIST_SKILL,		TRUE},
	{"bllist_area",		VAR_BLLIST_AREA,		TRUE},
    {"bllist_aregion",  VAR_BLLIST_AREA_REGION, TRUE},
	{"bllist_wilds",	VAR_BLLIST_WILDS,		TRUE},
	{"pllist_str",		VAR_PLLIST_STR,			TRUE},
	{"pllist_conn",		VAR_PLLIST_CONN,		TRUE},
	{"pllist_room",		VAR_PLLIST_ROOM,		TRUE},
	{"pllist_mob",		VAR_PLLIST_MOB,			TRUE},
	{"pllist_obj",		VAR_PLLIST_OBJ,			TRUE},
	{"pllist_tok",		VAR_PLLIST_TOK,			TRUE},
    {"pllist_area",     VAR_PLLIST_AREA,        TRUE},
    {"pllist_aregion",  VAR_PLLIST_AREA_REGION, TRUE},
	{"pllist_church",	VAR_PLLIST_CHURCH,		TRUE},
	{"pllist_variable",	VAR_PLLIST_VARIABLE,	TRUE},
	{ NULL,				VAR_UNKNOWN,			FALSE }

};


const struct flag_type skill_entry_flags[] = {
	{"practice",		SKILL_PRACTICE,			TRUE},
	{"improve",			SKILL_IMPROVE,			TRUE},
	{"favourite",		SKILL_FAVOURITE,			FALSE},	// This is set manually
	{ NULL,				0,			FALSE }
};


const struct flag_type shop_flags[] =
{
	{ "stock_only",		SHOPFLAG_STOCK_ONLY,	TRUE	},
	{ "hide_shop",		SHOPFLAG_HIDE_SHOP,		TRUE	},
	{ "no_haggle",		SHOPFLAG_NO_HAGGLE,		TRUE	},
	{ NULL,				0,						FALSE	}
};

const struct flag_type blueprint_section_flags[] =
{
	{ NULL,				0,						FALSE	}
};

const struct flag_type blueprint_section_types[] =
{
    { "static",         BSTYPE_STATIC,          TRUE    },
    { "maze",           BSTYPE_MAZE,            TRUE    },
	{ NULL,				0,						FALSE	}
};

const struct flag_type blueprint_flags[] =
{
    {"scripted_layout", BLUEPRINT_SCRIPTED_LAYOUT, FALSE},
    {NULL,              0,                         FALSE}
};

const struct flag_type instance_flags[] =
{
	{ "completed",			INSTANCE_COMPLETED,			FALSE	},
	{ "failed", 			INSTANCE_FAILED,			FALSE	},
	{ "destroy",			INSTANCE_DESTROY,			FALSE	},
	{ "idle_on_complete",	INSTANCE_IDLE_ON_COMPLETE,	TRUE	},
	{ "no_idle",			INSTANCE_NO_IDLE,			TRUE	},
	{ "no_save",			INSTANCE_NO_SAVE,			TRUE	},
	{ NULL,					0,							FALSE	}
};

const struct flag_type dungeon_flags[] =
{
    { "commenced",          DUNGEON_COMMENCED,          FALSE   },
	{ "completed",			DUNGEON_COMPLETED,			FALSE	},
	{ "destroy",			DUNGEON_DESTROY,			FALSE	},
	{ "failed", 			DUNGEON_FAILED, 			FALSE	},
    { "failure_on_empty",   DUNGEON_FAILURE_ON_EMPTY,   TRUE    },
    { "failure_on_wipe",    DUNGEON_FAILURE_ON_WIPE,    TRUE    },
    { "group_commence",     DUNGEON_GROUP_COMMENCE,     TRUE    },
	{ "idle_on_complete",	DUNGEON_IDLE_ON_COMPLETE,	TRUE	},
	{ "no_idle",			DUNGEON_NO_IDLE,			TRUE	},
	{ "no_save",			DUNGEON_NO_SAVE,			TRUE	},
    { "scripted_levels",    DUNGEON_SCRIPTED_LEVELS,    FALSE   },
    { "shared",             DUNGEON_SHARED,             TRUE    },
	{ NULL,					0,							FALSE	}
};

const struct flag_type transfer_modes[] =
{
	{ "silent",			TRANSFER_MODE_SILENT,	TRUE	},
	{ "portal",			TRANSFER_MODE_PORTAL,	TRUE	},
	{ "movement",		TRANSFER_MODE_MOVEMENT,	TRUE	},
	{ NULL,				0,						FALSE	}
};

const struct flag_type ship_class_types[] =
{
	{ "sailboat",		SHIP_SAILING_BOAT,		TRUE	},
	{ "airship",		SHIP_AIR_SHIP,			TRUE	},
	{ NULL,				0,						FALSE	}
};

const struct flag_type ship_flags[] =
{
	{ "protected",		SHIP_PROTECTED,			TRUE	},
	{ NULL,				0,						FALSE	}
};

const struct flag_type portal_gatetype[] =
{
    { "arearandom",             GATETYPE_AREARANDOM,			    TRUE	},
    { "arearecall",             GATETYPE_AREARECALL,			    TRUE	},
    { "dungeon",                GATETYPE_DUNGEON,   			    TRUE	},
    { "dungeonfloor",           GATETYPE_DUNGEONFLOOR,              TRUE    },
    { "dungeonfloorspecial",    GATETYPE_DUNGEON_FLOOR_SPECIAL,     TRUE    },
    { "dungeonrandom",          GATETYPE_DUNGEONRANDOM,			    TRUE	},
    { "dungeonrandomfloor",     GATETYPE_DUNGEON_RANDOM_FLOOR,      TRUE    },
    { "dungeonspecialroom",     GATETYPE_DUNGEON_SPECIAL,           TRUE    },
    { "environment",            GATETYPE_ENVIRONMENT,			    TRUE	},
    { "instance",               GATETYPE_INSTANCE,      		    TRUE	},
    { "instancerandom",         GATETYPE_INSTANCERANDOM,		    TRUE	},
    { "instancesectionmaze",    GATETYPE_BLUEPRINT_SECTION_MAZE,    TRUE    },
    { "instancespecialroom",    GATETYPE_BLUEPRINT_SPECIAL,         TRUE    },
    { "normal",                 GATETYPE_NORMAL,		    	    TRUE	},
    { "random",                 GATETYPE_RANDOM,                    TRUE    },
    { "regionrandom",           GATETYPE_REGIONRANDOM,              TRUE    },
    { "regionrecall",           GATETYPE_REGIONRECALL,			    TRUE	},
    { "sectionrandom",          GATETYPE_SECTIONRANDOM,			    TRUE	},
    { "wilds",                  GATETYPE_WILDS,		    	        TRUE	},
    { "wildsrandom",            GATETYPE_WILDSRANDOM,               TRUE    },
    { NULL,             0,                              FALSE   }
};


const struct flag_type wilderness_regions[] =
{
    { "Arena Island",        REGION_ARENA_ISLAND, TRUE },
    { "Central Ocean",       REGION_CENTRAL_OCEAN, TRUE },
    { "Dragon Island",       REGION_DRAGON_ISLAND, TRUE },
    { "Eastern Ocean",       REGION_EASTERN_OCEAN, TRUE },
    { "First Continent",     REGION_FIRST_CONTINENT, TRUE },
    { "Fourth Continent",    REGION_FOURTH_CONTINENT, TRUE },
    { "Mordrake Island",     REGION_MORDRAKE_ISLAND, TRUE },
    { "North Pole",          REGION_NORTH_POLE, TRUE },
    { "Northern Ocean",      REGION_NORTHERN_OCEAN, TRUE },
    { "Second Continent",    REGION_SECOND_CONTINENT, TRUE },
    { "South Pole",          REGION_SOUTH_POLE, TRUE },
    { "Southern Ocean",      REGION_SOUTHERN_OCEAN, TRUE },
    { "Temple Island",       REGION_TEMPLE_ISLAND, TRUE },
    { "Third Continent",     REGION_THIRD_CONTINENT, TRUE },
    { "Undersea",            REGION_UNDERSEA, TRUE },
    { "Western Ocean",       REGION_WESTERN_OCEAN, TRUE },
    { "Unknown",             REGION_UNKNOWN, FALSE},
    { NULL,                  REGION_UNKNOWN,             FALSE }

	// Overworld regions

	// Abyss regions

	// Other regions

};

const struct flag_type area_region_flags[] =
{
    { "no_recall",      AREA_REGION_NO_RECALL,      TRUE    },
    { NULL,             0,             FALSE }
};

const struct flag_type death_release_modes[] =
{
    { "normal",                 DEATH_RELEASE_NORMAL,        TRUE    },
    { "release_to_start",       DEATH_RELEASE_TO_START,      TRUE    },
    { "release_to_floor",       DEATH_RELEASE_TO_FLOOR,      TRUE    },
    { "release_to_checkpoint",  DEATH_RELEASE_TO_CHECKPOINT, TRUE    },
    { "release_failure",        DEATH_RELEASE_FAILURE,       TRUE    },
    { NULL,                     0,                           FALSE   }
};

const struct flag_type trigger_slots[] = 
{
    { "action",         TRIGSLOT_ACTION,        TRUE },
    { "animate",        TRIGSLOT_ANIMATE,       TRUE },
    { "attacks",        TRIGSLOT_ATTACKS,       TRUE },
    { "combatstyle",    TRIGSLOT_COMBATSTYLE,   TRUE },
    { "damage",         TRIGSLOT_DAMAGE,        TRUE },
    { "fight",          TRIGSLOT_FIGHT,         TRUE },
    { "general",        TRIGSLOT_GENERAL,       TRUE },
    { "hits",           TRIGSLOT_HITS,          TRUE },
    { "interrupt",      TRIGSLOT_INTERRUPT,     TRUE },
    { "move",           TRIGSLOT_MOVE,          TRUE },
    { "random",         TRIGSLOT_RANDOM,        TRUE },
    { "repop",          TRIGSLOT_REPOP,         TRUE },
    { "speech",         TRIGSLOT_SPEECH,        TRUE },
    { "spell",          TRIGSLOT_SPELL,         TRUE },
    { "verb",           TRIGSLOT_VERB,          TRUE },
    { NULL,             -1,                     FALSE}
};

const struct flag_type builtin_trigger_types[] = 
{
    { "act",		        TRIG_ACT,	TRUE },
    { "afterdeath",		    TRIG_AFTERDEATH,	TRUE },
    { "afterkill",		    TRIG_AFTERKILL,	TRUE },
    { "aggression",         TRIG_AGGRESSION, TRUE },
    { "animate",		    TRIG_ANIMATE,	TRUE },
    { "assist",		        TRIG_ASSIST,	TRUE },
    { "attack_backstab",	TRIG_ATTACK_BACKSTAB,	TRUE },
    { "attack_bash",		TRIG_ATTACK_BASH,	TRUE },
    { "attack_behead",		TRIG_ATTACK_BEHEAD,	TRUE },
    { "attack_bite",		TRIG_ATTACK_BITE,	TRUE },
    { "attack_blackjack",	TRIG_ATTACK_BLACKJACK,	TRUE },
    { "attack_circle",		TRIG_ATTACK_CIRCLE,	TRUE },
    { "attack_counter",		TRIG_ATTACK_COUNTER,	TRUE },
    { "attack_cripple",		TRIG_ATTACK_CRIPPLE,	TRUE },
    { "attack_dirtkick",	TRIG_ATTACK_DIRTKICK,	TRUE },
    { "attack_disarm",		TRIG_ATTACK_DISARM,	TRUE },
    { "attack_intimidate",	TRIG_ATTACK_INTIMIDATE,	TRUE },
    { "attack_kick",		TRIG_ATTACK_KICK,	TRUE },
    { "attack_rend",		TRIG_ATTACK_REND,	TRUE },
    { "attack_slit",		TRIG_ATTACK_SLIT,	TRUE },
    { "attack_smite",		TRIG_ATTACK_SMITE,	TRUE },
    { "attack_tailkick",	TRIG_ATTACK_TAILKICK,	TRUE },
    { "attack_trample",		TRIG_ATTACK_TRAMPLE,	TRUE },
    { "attack_turn",		TRIG_ATTACK_TURN,	TRUE },
    { "barrier",		    TRIG_BARRIER,	TRUE },
    { "block_aggression",   TRIG_BLOCK_AGGRESSION, TRUE },
    { "blow",		        TRIG_BLOW,	TRUE },
    { "blueprint_schematic",TRIG_BLUEPRINT_SCHEMATIC,	TRUE },
    { "board",		        TRIG_BOARD,	TRUE },
    { "book_page",			TRIG_BOOK_PAGE,	TRUE },
    { "book_page_attach",	TRIG_BOOK_PAGE_ATTACH,	TRUE },
    { "book_page_preattach",TRIG_BOOK_PAGE_PREATTACH,	TRUE },
    { "book_page_prerip",	TRIG_BOOK_PAGE_PRERIP,	TRUE },
    { "book_page_rip",		TRIG_BOOK_PAGE_RIP,	TRUE },
    { "book_preread",		TRIG_BOOK_PREREAD,	TRUE },
    { "book_preseal",		TRIG_BOOK_PRESEAL,	TRUE },
    { "book_prewrite",		TRIG_BOOK_PREWRITE,	TRUE },
    { "book_read",			TRIG_BOOK_READ,	TRUE },
    { "book_seal",			TRIG_BOOK_SEAL,	TRUE },
    { "book_write_text",	TRIG_BOOK_WRITE_TEXT,	TRUE },
    { "book_write_title",	TRIG_BOOK_WRITE_TITLE,	TRUE },
    { "brandish",	    	TRIG_BRANDISH,	TRUE },
    { "bribe",		        TRIG_BRIBE,	TRUE },
    { "bury",		        TRIG_BURY,	TRUE },
    { "buy",		        TRIG_BUY,	TRUE },
    { "caninterrupt",		TRIG_CANINTERRUPT,	TRUE },
    { "cause_aggression",   TRIG_CAUSE_AGGRESSION,  TRUE },
    { "catalyst",		    TRIG_CATALYST,	TRUE },
    { "catalyst_full",		TRIG_CATALYST_FULL,	TRUE },
    { "catalyst_source",	TRIG_CATALYST_SOURCE,	TRUE },
    { "check_aggression",   TRIG_CHECK_AGGRESSION, TRUE },
    { "check_buyer",		TRIG_CHECK_BUYER,	TRUE },
    { "check_damage",		TRIG_CHECK_DAMAGE,	TRUE },
    { "clone_extract",		TRIG_CLONE_EXTRACT,	TRUE },
    { "close",		        TRIG_CLOSE,	TRUE },
    { "combat_style",		TRIG_COMBAT_STYLE,	TRUE },
    { "completed",		    TRIG_COMPLETED,	TRUE },
    { "contract_complete",	TRIG_CONTRACT_COMPLETE,	TRUE },
    { "custom_price",		TRIG_CUSTOM_PRICE,	TRUE },
    { "damage",		        TRIG_DAMAGE,	TRUE },
    { "death",		        TRIG_DEATH,	TRUE },
    { "death_protection",	TRIG_DEATH_PROTECTION,	TRUE },
    { "death_timer",		TRIG_DEATH_TIMER,	TRUE },
    { "defense",	    	TRIG_DEFENSE,	TRUE },
    { "delay",		        TRIG_DELAY,	TRUE },
    { "drink",		        TRIG_DRINK,	TRUE },
    { "drop",		        TRIG_DROP,	TRUE },
    { "dungeon_commenced",  TRIG_DUNGEON_COMMENCED, TRUE },
    { "dungeon_schematic",	TRIG_DUNGEON_SCHEMATIC,	TRUE },
    { "eat",		        TRIG_EAT,	TRUE },
    { "emote",		        TRIG_EMOTE,	TRUE },
    { "emoteat",		    TRIG_EMOTEAT,	TRUE },
    { "emoteself",		    TRIG_EMOTESELF,	TRUE },
    { "emptied",            TRIG_EMPTIED, TRUE },
    { "entry",		        TRIG_ENTRY,	TRUE },
    { "exall",		        TRIG_EXALL,	TRUE },
    { "examine",		    TRIG_EXAMINE,	TRUE },
    { "exit",		        TRIG_EXIT,	TRUE },
    { "expire",		        TRIG_EXPIRE,	TRUE },
    { "extinguish",         TRIG_EXTINGUISH,    TRUE },
    { "extract",		    TRIG_EXTRACT,	TRUE },
    { "failed",		        TRIG_FAILED,	TRUE },
    { "fight",		        TRIG_FIGHT,	TRUE },
    { "fill",               TRIG_FILL,  TRUE },
    { "filled",             TRIG_FILLED, TRUE },
    { "flee",		        TRIG_FLEE,	TRUE },
    { "fluid_emptied",      TRIG_FLUID_EMPTIED, TRUE },
    { "fluid_filled",       TRIG_FLUID_FILLED, TRUE },
    { "forcedismount",		TRIG_FORCEDISMOUNT,	TRUE },
    { "get",		        TRIG_GET,	TRUE },
    { "give",		        TRIG_GIVE,	TRUE },
    { "grall",		        TRIG_GRALL,	TRUE },
    { "greet",		        TRIG_GREET,	TRUE },
    { "grouped",		    TRIG_GROUPED,	TRUE },
    { "grow",		        TRIG_GROW,	TRUE },
    { "hidden",		        TRIG_HIDDEN,	TRUE },
    { "hide",		        TRIG_HIDE,	TRUE },
    { "hit",		        TRIG_HIT,	TRUE },
    { "hitgain",		    TRIG_HITGAIN,	TRUE },
    { "hpcnt",		        TRIG_HPCNT,	TRUE },
    { "identify",		    TRIG_IDENTIFY,	TRUE },
    { "ignite",             TRIG_IGNITE,    TRUE },
    { "inspect",		    TRIG_INSPECT,	TRUE },
    { "interrupt",		    TRIG_INTERRUPT,	TRUE },
    { "kill",		        TRIG_KILL,	TRUE },
    { "knock",		        TRIG_KNOCK,	TRUE },
    { "knocking",		    TRIG_KNOCKING,	TRUE },
    { "land",		        TRIG_LAND,	TRUE },
    { "level",		        TRIG_LEVEL,	TRUE },
    { "lock",		        TRIG_LOCK,	TRUE },
    { "login",		        TRIG_LOGIN,	TRUE },
    { "lookat",             TRIG_LOOK_AT, TRUE },
    { "lore",		        TRIG_LORE,	TRUE },
    { "lore_ex",		    TRIG_LORE_EX,	TRUE },
    { "managain",		    TRIG_MANAGAIN,	TRUE },
    { "moon",		        TRIG_MOON,	TRUE },
    { "mount",		        TRIG_MOUNT,	TRUE },
    { "move_char",		    TRIG_MOVE_CHAR,	TRUE },
    { "movegain",		    TRIG_MOVEGAIN,	TRUE },
    { "multiclass",		    TRIG_MULTICLASS,	TRUE },
    { "open",		        TRIG_OPEN,	TRUE },
    { "postquest",		    TRIG_POSTQUEST,	TRUE },
    { "pour",               TRIG_POUR,  TRUE },
    { "practice",		    TRIG_PRACTICE,	TRUE },
    { "practicetoken",		TRIG_PRACTICETOKEN,	TRUE },
    { "preaggressive",      TRIG_PREAGGRESSIVE, TRUE },
    { "preanimate",		    TRIG_PREANIMATE,	TRUE },
    { "preassist",		    TRIG_PREASSIST,	TRUE },
    { "prebite",		    TRIG_PREBITE,	TRUE },
    { "prebrandish",        TRIG_PREBRANDISH,   TRUE },
    { "prebuy",		        TRIG_PREBUY,	TRUE },
    { "prebuy_obj",		    TRIG_PREBUY_OBJ,	TRUE },
    { "precast",		    TRIG_PRECAST,	TRUE },
    { "predeath",		    TRIG_PREDEATH,	TRUE },
    { "predismount",		TRIG_PREDISMOUNT,	TRUE },
    { "predrink",		    TRIG_PREDRINK,	TRUE },
    { "predrop",		    TRIG_PREDROP,	TRUE },
    { "preeat",		        TRIG_PREEAT,	TRUE },
    { "preenter",		    TRIG_PREENTER,	TRUE },
    { "preextinguish",      TRIG_PREEXTINGUISH,    TRUE },
    { "preflee",		    TRIG_PREFLEE,	TRUE },
    { "preget",		        TRIG_PREGET,	TRUE },
    { "prehide",		    TRIG_PREHIDE,	TRUE },
    { "prehide_in",		    TRIG_PREHIDE_IN,	TRUE },
    { "preignite",          TRIG_PREIGNITE,    TRUE },
    { "prekill",		    TRIG_PREKILL,	TRUE },
    { "prelock",		    TRIG_PRELOCK,	TRUE },
    { "premount",		    TRIG_PREMOUNT,	TRUE },
    { "prepractice",		TRIG_PREPRACTICE,	TRUE },
    { "prepracticeother",	TRIG_PREPRACTICEOTHER,	TRUE },
    { "prepracticethat",	TRIG_PREPRACTICETHAT,	TRUE },
    { "prepracticetoken",	TRIG_PREPRACTICETOKEN,	TRUE },
    { "preput",		        TRIG_PREPUT,	TRUE },
    { "prequest",		    TRIG_PREQUEST,	TRUE },
    { "prerecall",		    TRIG_PRERECALL,	TRUE },
    { "prerecite",		    TRIG_PRERECITE,	TRUE },
    { "prereckoning",		TRIG_PRERECKONING,	TRUE },
    { "prerehearse",		TRIG_PREREHEARSE,	TRUE },
    { "preremove",		    TRIG_PREREMOVE,	TRUE },
    { "prerenew",		    TRIG_PRERENEW,	TRUE },
    { "prerest",		    TRIG_PREREST,	TRUE },
    { "preresurrect",		TRIG_PRERESURRECT,	TRUE },
    { "preround",		    TRIG_PREROUND,	TRUE },
    { "presell",		    TRIG_PRESELL,	TRUE },
    { "presit",		        TRIG_PRESIT,	TRUE },
    { "presleep",		    TRIG_PRESLEEP,	TRUE },
    { "prespell",		    TRIG_PRESPELL,	TRUE },
    { "prestand",		    TRIG_PRESTAND,	TRUE },
    { "prestepoff",		    TRIG_PRESTEPOFF,	TRUE },
    { "pretrain",		    TRIG_PRETRAIN,	TRUE },
    { "pretraintoken",		TRIG_PRETRAINTOKEN,	TRUE },
    { "preunlock",		    TRIG_PREUNLOCK,	TRUE },
    { "prewake",		    TRIG_PREWAKE,	TRUE },
    { "prewear",		    TRIG_PREWEAR,	TRUE },
    { "prewimpy",		    TRIG_PREWIMPY,	TRUE },
    { "prezap",             TRIG_PREZAP,    TRUE },
    { "pull",		        TRIG_PULL,	TRUE },
    { "pull_on",		    TRIG_PULL_ON,	TRUE },
    { "pulse",		        TRIG_PULSE,	TRUE },
    { "push",		        TRIG_PUSH,	TRUE },
    { "push_on",		    TRIG_PUSH_ON,	TRUE },
    { "put",		        TRIG_PUT,	TRUE },
    { "quest_cancel",		TRIG_QUEST_CANCEL,	TRUE },
    { "quest_complete",		TRIG_QUEST_COMPLETE,	TRUE },
    { "quest_incomplete",	TRIG_QUEST_INCOMPLETE,	TRUE },
    { "quest_part",		    TRIG_QUEST_PART,	TRUE },
    { "quit",		        TRIG_QUIT,	TRUE },
    { "random",		        TRIG_RANDOM,	TRUE },
    { "readycheck",         TRIG_READYCHECK,    TRUE },
    { "recall",		        TRIG_RECALL,	TRUE },
    { "recite",		        TRIG_RECITE,	TRUE },
    { "reckoning",		    TRIG_RECKONING,	TRUE },
    { "regen",	        	TRIG_REGEN,	TRUE },
    { "regen_hp",		    TRIG_REGEN_HP,	TRUE },
    { "regen_mana",		    TRIG_REGEN_MANA,	TRUE },
    { "regen_move",		    TRIG_REGEN_MOVE,	TRUE },
    { "remort",		        TRIG_REMORT,	TRUE },
    { "remove",		        TRIG_REMOVE,	TRUE },
    { "renew",		        TRIG_RENEW,	TRUE },
    { "renew_list",		    TRIG_RENEW_LIST,	TRUE },
    { "repop",		        TRIG_REPOP,	TRUE },
    { "reset",		        TRIG_RESET,	TRUE },
    { "rest",		        TRIG_REST,	TRUE },
    { "restocked",		    TRIG_RESTOCKED,	TRUE },
    { "restore",		    TRIG_RESTORE,	TRUE },
    { "resurrect",		    TRIG_RESURRECT,	TRUE },
    { "room_footer",		TRIG_ROOM_FOOTER,	TRUE },
    { "room_header",		TRIG_ROOM_HEADER,	TRUE },
    { "save",		        TRIG_SAVE,	TRUE },
    { "sayto",		        TRIG_SAYTO,	TRUE },
    { "showcommands",		TRIG_SHOWCOMMANDS,	TRUE },
    { "showexit",		    TRIG_SHOWEXIT,	TRUE },
    { "sit",		        TRIG_SIT,	TRUE },
    { "skill_berserk",		TRIG_SKILL_BERSERK,	TRUE },
    { "skill_sneak",		TRIG_SKILL_SNEAK,	TRUE },
    { "skill_warcry",		TRIG_SKILL_WARCRY,	TRUE },
    { "sleep",		        TRIG_SLEEP,	TRUE },
    { "speech",		        TRIG_SPEECH,	TRUE },
    { "spell",		        TRIG_SPELL,	TRUE },
    { "spellcast",		    TRIG_SPELLCAST,	TRUE },
    { "spellpenetrate",		TRIG_SPELLPENETRATE,	TRUE },
    { "spellreflect",		TRIG_SPELLREFLECT,	TRUE },
    { "spell_cure",		    TRIG_SPELL_CURE,	TRUE },
    { "spell_dispel",		TRIG_SPELL_DISPEL,	TRUE },
    { "stand",		        TRIG_STAND,	TRUE },
    { "start_combat",		TRIG_START_COMBAT,	TRUE },
    { "stepoff",		    TRIG_STEPOFF,	TRUE },
    { "stripaffect",		TRIG_STRIPAFFECT,	TRUE },
    { "takeoff",		    TRIG_TAKEOFF,	TRUE },
    { "throw",		        TRIG_THROW,	TRUE },
    { "tick",		        TRIG_TICK,	TRUE },
    { "toggle_custom",		TRIG_TOGGLE_CUSTOM,	TRUE },
    { "toggle_list",		TRIG_TOGGLE_LIST,	TRUE },
    { "token_brandish",     TRIG_TOKEN_BRANDISH,    TRUE },
	{ "token_brew",         TRIG_TOKEN_BREW,	TRUE },
	{ "token_equip",        TRIG_TOKEN_EQUIP,	TRUE },
	{ "token_given",        TRIG_TOKEN_GIVEN,	TRUE },
    { "token_imbue",		TRIG_TOKEN_IMBUE,	TRUE },
	{ "token_ink",      	TRIG_TOKEN_INK,	TRUE },
    { "token_interrupt",    TRIG_TOKEN_INTERRUPT, TRUE },
	{ "token_prebrew",  	TRIG_TOKEN_PREBREW,	TRUE },
	{ "token_preimbue",   	TRIG_TOKEN_PREIMBUE,	TRUE },
	{ "token_preink",   	TRIG_TOKEN_PREINK,	TRUE },
	{ "token_prescribe",	TRIG_TOKEN_PRESCRIBE,	TRUE },
    { "token_pulse",        TRIG_TOKEN_PULSE,   TRUE },
	{ "token_quaff",    	TRIG_TOKEN_QUAFF,	TRUE },
	{ "token_recite",   	TRIG_TOKEN_RECITE,	TRUE },
	{ "token_removed",  	TRIG_TOKEN_REMOVED,	TRUE },
	{ "token_scribe",   	TRIG_TOKEN_SCRIBE,	TRUE },
	{ "token_touch",    	TRIG_TOKEN_TOUCH,	TRUE },
	{ "token_zap",      	TRIG_TOKEN_ZAP,	TRUE },
    { "touch",		        TRIG_TOUCH,	TRUE },
    { "toxingain",		    TRIG_TOXINGAIN,	TRUE },
    { "turn",		        TRIG_TURN,	TRUE },
    { "turn_on",		    TRIG_TURN_ON,	TRUE },
    { "ungrouped",		    TRIG_UNGROUPED,	TRUE },
    { "unlock",		        TRIG_UNLOCK,	TRUE },
    { "use",		        TRIG_USE,	TRUE },
    { "usewith",		    TRIG_USEWITH,	TRUE },
    { "verb",		        TRIG_VERB,	TRUE },
    { "verbself",		    TRIG_VERBSELF,	TRUE },
    { "wake",		        TRIG_WAKE,	TRUE },
    { "weapon_blocked",		TRIG_WEAPON_BLOCKED,	TRUE },
    { "weapon_caught",		TRIG_WEAPON_CAUGHT,	TRUE },
    { "weapon_parried",		TRIG_WEAPON_PARRIED,	TRUE },
    { "wear",		        TRIG_WEAR,	TRUE },
    { "whisper",		    TRIG_WHISPER,	TRUE },
    { "wimpy",		        TRIG_WIMPY,	TRUE },
    { "xpbonus",		    TRIG_XPBONUS,	TRUE },
    { "xpcompute",		    TRIG_XPCOMPUTE,	TRUE },
    { "xpgain",		        TRIG_XPGAIN,	TRUE },
    { "zap",		        TRIG_ZAP,	TRUE },
    { NULL,                 -1,         FALSE }
};

const struct flag_type script_spaces[] =
{
    { "mobile",     PRG_MPROG, TRUE },
    { "object",     PRG_OPROG, TRUE },
    { "room",       PRG_RPROG, TRUE },
    { "token",      PRG_TPROG, TRUE },
    { "area",       PRG_APROG, TRUE },
    { "instance",   PRG_IPROG, TRUE },
    { "dungeon",    PRG_DPROG, TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type light_flags[] =
{
    { "active",                 LIGHT_IS_ACTIVE,            TRUE },
    { "no_extinguish",          LIGHT_NO_EXTINGUISH,        TRUE },
    { "remove_on_extinguish",   LIGHT_REMOVE_ON_EXTINGUISH, TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type compartment_flags[] =
{
    { "allow_move",     COMPARTMENT_ALLOW_MOVE,     TRUE },
    { "closeable",      COMPARTMENT_CLOSEABLE,      TRUE },
    { "closed",         COMPARTMENT_CLOSED,         TRUE },
    { "closelock",      COMPARTMENT_CLOSELOCK,      TRUE },
    { "inside",         COMPARTMENT_INSIDE,         TRUE },
    { "pushopen",       COMPARTMENT_PUSHOPEN,       TRUE },
    { "transparent",    COMPARTMENT_TRANSPARENT,    TRUE },
    { "underwater",     COMPARTMENT_UNDERWATER,     TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type stock_types[] =
{
    { "crew",           STOCK_CREW,                 TRUE },
    { "custom",         STOCK_CUSTOM,               TRUE },
    { "guard",          STOCK_GUARD,                TRUE },
    { "mount",          STOCK_MOUNT,                TRUE },
    { "object",         STOCK_OBJECT,               TRUE },
    { "pet",            STOCK_PET,                  TRUE },
    { "ship",           STOCK_SHIP,                 TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type prog_entity_flags[] =
{
    { "at",             PROG_AT,                    TRUE },
    { "nodamage",       PROG_NODAMAGE,              TRUE },
    { "nodestruct",     PROG_NODESTRUCT,            TRUE },
    { "norawkill",      PROG_NORAWKILL,             TRUE },
    { "silent",         PROG_SILENT,                TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type book_flags[] =
{
    { "closeable",   BOOK_CLOSEABLE,     TRUE },
    { "closed",      BOOK_CLOSED,        TRUE },
    { "closelock",   BOOK_CLOSELOCK,     TRUE },
    { "pushopen",    BOOK_PUSHOPEN,      TRUE },
    { "no_rip",      BOOK_NO_RIP,        TRUE },
    { "writable",    BOOK_WRITABLE,      TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type fluid_con_flags[] =
{
    { "closeable",           FLUID_CON_CLOSEABLE,           TRUE },
    { "closed",              FLUID_CON_CLOSED,              TRUE },
    { "closelock",           FLUID_CON_CLOSELOCK,           TRUE },
    { "destroy_on_consume",  FLUID_CON_DESTROY_ON_CONSUME,  TRUE },
    { "pushopen",            FLUID_CON_PUSHOPEN,            TRUE },
    { NULL, 0, FALSE }
};

const struct flag_type scroll_flags[] =
{
    { "destroy_on_recite",  SCROLL_DESTROY_ON_RECITE,       TRUE },
    { NULL, 0, FALSE }
};

const struct gln_type gln_table[] =
{
    { "water", &gln_water, &liquid_water },
    { "blood", &gln_blood, &liquid_blood },
    { "potion", &gln_potion, &liquid_potion },
    { NULL, NULL, NULL }
};

const struct spell_func_type prespell_func_table[] =
{
    { "silence",            prespell_silence },
    { NULL,     NULL }
};

const struct spell_func_type spell_func_table[] =
{
    { "none",               spell_null },
	{ "acid_blast",	        spell_acid_blast },
	{ "acid_breath",	    spell_acid_breath },
	{ "afterburn",	        spell_afterburn },
	{ "animate_dead",	    spell_animate_dead },
	{ "armour",	            spell_armour },
	{ "avatar_shield",	    spell_avatar_shield },
	{ "bless",      	    spell_bless },
	{ "blindness",       	spell_blindness },
	{ "burning_hands",  	spell_burning_hands },
	{ "call_familiar",  	spell_call_familiar },
	{ "call_lightning", 	spell_call_lightning },
	{ "calm",         	    spell_calm },
	{ "cancellation",   	spell_cancellation },
	{ "cause_critical", 	spell_cause_critical },
	{ "cause_light",	    spell_cause_light },
	{ "cause_serious",	    spell_cause_serious },
	{ "chain_lightning",	spell_chain_lightning },
	//{ "change_sex",	        spell_change_sex },
	{ "channel",	        spell_channel },
	{ "charm_person",	    spell_charm_person },
	{ "chill_touch",	    spell_chill_touch },
	{ "cloak_of_guile",	    spell_cloak_of_guile },
	{ "colour_spray",	    spell_colour_spray },
	{ "continual_light",	spell_continual_light },
	{ "control_weather",	spell_control_weather },
	{ "cosmic_blast",	    spell_cosmic_blast },
	{ "counter_spell",	    spell_counter_spell },
	{ "create_rose",	    spell_create_rose },
	{ "create_spring",	    spell_create_spring },
	{ "create_water",	    spell_create_water },
	//{ "crucify",	        spell_crucify },
	{ "cure_blindness",	    spell_cure_blindness },
	{ "cure_critical",	    spell_cure_critical },
	{ "cure_disease",	    spell_cure_disease },
	{ "cure_light",	        spell_cure_light },
	{ "cure_poison",	    spell_cure_poison },
	{ "cure_serious",	    spell_cure_serious },
	{ "cure_toxic",	        spell_cure_toxic },
	{ "curse",	            spell_curse },
	{ "dark_shroud",	    spell_dark_shroud },
	//{ "dark_sight",	        spell_dark_sight },
	{ "death_grip",	        spell_death_grip },
	{ "deathbarbs",	        spell_deathbarbs },
	{ "deathsight",	        spell_deathsight },
	{ "demonfire",	        spell_demonfire },
	{ "destruction",	    spell_destruction },
	//{ "detect_evil",	    spell_detect_evil },
	//{ "detect_good",	    spell_detect_good },
	{ "detect_hidden",	    spell_detect_hidden },
	{ "detect_invis",	    spell_detect_invis },
	{ "detect_magic",	    spell_detect_magic },
	{ "discharge",	        spell_discharge },
	{ "dispel_evil",	    spell_dispel_evil },
	{ "dispel_good",	    spell_dispel_good },
	{ "dispel_magic",	    spell_dispel_magic },
	{ "dispel_room",	    spell_dispel_room },
	{ "eagle_eye",	        spell_eagle_eye },
	{ "earth_walk",	        spell_earth_walk },
	{ "earthquake",	        spell_earthquake },
	{ "electrical_barrier",	spell_electrical_barrier },
	{ "enchant_armour",	    spell_enchant_armour },
	{ "enchant_weapon",	    spell_enchant_weapon },
	{ "energy_drain",	    spell_energy_drain },
	{ "energy_field",	    spell_energy_field },
	//{ "enervation",	        spell_enervation },
	{ "ensnare",	        spell_ensnare },
	{ "entrap",	            spell_entrap },
	{ "exorcism",	        spell_exorcism },
	{ "faerie_fire",	    spell_faerie_fire },
	{ "faerie_fog",	        spell_faerie_fog },
	//{ "false_dawn",	        spell_false_dawn },
	//{ "farsight",	        spell_farsight },
	{ "fatigue",	        spell_fatigue },
	{ "fire_barrier",	    spell_fire_barrier },
	{ "fire_breath",	    spell_fire_breath },
	{ "fire_cloud",	        spell_fire_cloud },
	{ "fireball",	        spell_fireball },
	{ "fireproof",	        spell_fireproof },
	{ "flamestrike",	    spell_flamestrike },
	{ "flash",	            spell_flash },
	//{ "floating_disc",	    spell_floating_disc },
	{ "fly",	            spell_fly },
	{ "frenzy",	            spell_frenzy },
	{ "frost_barrier",	    spell_frost_barrier },
	{ "frost_breath",	    spell_frost_breath },
	{ "gas_breath",	        spell_gas_breath },
	{ "gate",	            spell_gate },
	//{ "general_purpose",	spell_general_purpose },
	{ "giant_strength",	    spell_giant_strength },
	{ "glacial_wave",	    spell_glacial_wave },
	{ "glorious_bolt",	    spell_glorious_bolt },
	//{ "hallucination",	    spell_hallucination },
	{ "harm",	            spell_harm },
	{ "haste",	            spell_haste },
	{ "heal",	            spell_heal },
	{ "healing_aura",	    spell_healing_aura },
	//{ "heat_metal",	        spell_heat_metal },         // TODO: Reimplement this!
	//{ "high_explosive",	    spell_high_explosive },
	{ "holy_shield",	    spell_holy_shield },
	{ "holy_sword",	        spell_holy_sword },
	{ "holy_word",	        spell_holy_word },
	{ "ice_shards",	        spell_ice_shards },
	{ "ice_storm",	        spell_ice_storm },
	{ "identify",	        spell_identify },
	{ "improved_invis",	    spell_improved_invisibility },
	{ "inferno",	        spell_inferno },
	{ "infravision",	    spell_infravision },
	{ "invis",	            spell_invis },
	{ "kill",	            spell_kill },
	//{ "know_alignment",	    spell_know_alignment },
	{ "light_shroud",	    spell_light_shroud },
	{ "lightning_bolt",	    spell_lightning_bolt },
	{ "lightning_breath",	spell_lightning_breath },
	{ "locate_object",	    spell_locate_object },
	{ "magic_missile",	    spell_magic_missile },
	{ "mass_healing",	    spell_mass_healing },
	{ "mass_invis",	        spell_mass_invis },
	{ "master_weather",	    spell_master_weather },
	{ "maze",	            spell_maze },
	{ "momentary_darkness",	spell_momentary_darkness },
	{ "morphlock",	        spell_morphlock },
	{ "nexus",	            spell_nexus },
	{ "paralysis",	        spell_paralysis },
	{ "pass_door",	        spell_pass_door },
	{ "plague",	            spell_plague },
	{ "poison",	            spell_poison },
	//{ "portal",	            spell_portal },
	//{ "possess",	        spell_possess },
	//{ "protection_evil",	spell_protection_evil },
	//{ "protection_good",	spell_protection_good },
	{ "raise_dead",	        spell_raise_dead },
	//{ "ray_of_truth",	    spell_ray_of_truth },
	{ "recharge",	        spell_recharge },
	{ "reflection",	        spell_reflection },
	{ "refresh",	        spell_refresh },
	{ "regeneration",	    spell_regeneration },
	{ "remove_curse",	    spell_remove_curse },
	//{ "reverie",	        spell_reverie },
	{ "room_shield",	    spell_room_shield },
	{ "sanctuary",	        spell_sanctuary },
	{ "shield",	            spell_shield },
	{ "shocking_grasp",	    spell_shocking_grasp },
	{ "shriek",	            spell_shriek },
	{ "silence",	        spell_silence },
	{ "sleep",	            spell_sleep },
	{ "slow",	            spell_slow },
	{ "soul_essence",	    spell_soul_essence },
	{ "spell_deflection",	spell_spell_deflection },
	{ "spell_shield",	    spell_spell_shield },
	{ "spell_trap",	        spell_spell_trap },
	{ "starflare",	        spell_starflare },
	{ "stinking_cloud",	    spell_stinking_cloud },
	{ "stone_skin",	        spell_stone_skin },
	{ "stone_spikes",	    spell_stone_spikes },
	{ "stone_touch",	    spell_stone_touch },
	{ "summon",	            spell_summon },
	//{ "teleport",	        spell_teleport },
	{ "third_eye",	        spell_third_eye },
	{ "toxic_fumes",	    spell_toxic_fumes },
	{ "toxin_neurotoxin",	spell_toxin_neurotoxin },
	{ "toxin_paralysis",	spell_toxin_paralysis },
	{ "toxin_venom",	    spell_toxin_venom },
	{ "toxin_weakness",	    spell_toxin_weakness },
	{ "underwater_breathing",	spell_underwater_breathing },
	//{ "ventriloquate",	    spell_ventriloquate },
	{ "vision",	            spell_vision },
	//{ "vocalize",	        spell_vocalize },               // TODO: Finish this spell
	{ "weaken",	            spell_weaken },
	{ "web",	            spell_web },
	{ "wind_of_confusion",	spell_wind_of_confusion },
	{ "withering_cloud",	spell_withering_cloud },
	{ "word_of_recall",	    spell_word_of_recall },
    { NULL, NULL }
};


const struct spell_func_type pulse_func_table[] =
{
    { NULL,     NULL }
};

const struct spell_func_type interrupt_func_table[] =
{
    { NULL,     NULL }
};

const struct prebrew_func_type prebrew_func_table[] =
{
    { "silence",            prebrew_silence     },
    { NULL, NULL }
};

const struct brew_func_type brew_func_table[] =
{
    { "silence",            brew_silence     },
    { NULL, NULL }
};

const struct quaff_func_type quaff_func_table[] =
{
    { "silence",            quaff_silence     },
    { NULL, NULL }
};

const struct prescribe_func_type prescribe_func_table[] =
{
    { NULL, NULL }
};

const struct scribe_func_type scribe_func_table[] =
{
    { NULL, NULL }
};

const struct recite_func_type recite_func_table[] =
{
    { "identify",       recite_identify },
    { "word_of_recall", recite_word_of_recall },
    { NULL, NULL }
};

const struct preink_func_type preink_func_table[] =
{
    { NULL, NULL }
};

const struct ink_func_type ink_func_table[] =
{
    { NULL, NULL }
};

const struct touch_func_type touch_func_table[] =
{
    { "armour",             touch_armour },
    { "chain_lightning",    touch_chain_lightning },
    { "fly",                touch_fly },
    { "haste",              touch_haste },
    { NULL,                 NULL }
};

const struct zap_func_type zap_func_table[] =
{
    { "chain_lightning",    zap_chain_lightning },
    { NULL,                 NULL }
};



const struct gsn_type gsn_table[] =
{
	{ "acid_blast", 	    &gsn_acid_blast, 	    &gsk_acid_blast },
	{ "acid_breath",	    &gsn_acid_breath,	    &gsk_acid_breath },
	{ "acrobatics",         &gsn_acro,              &gsk_acro },
	{ "afterburn",  	    &gsn_afterburn,  	    &gsk_afterburn },
	{ "air_spells", 	    &gsn_air_spells, 	    &gsk_air_spells },
	{ "ambush", 	        &gsn_ambush, 	        &gsk_ambush },
	{ "animate_dead",   	&gsn_animate_dead,   	&gsk_animate_dead },
	{ "archery",	        &gsn_archery,	        &gsk_archery },
	{ "armour",             &gsn_armour,             &gsk_armour },
	{ "athletics",  	    &gsn_athletics,  	    &gsk_athletics },
	{ "avatar_shield",  	&gsn_avatar_shield,  	&gsk_avatar_shield },
	{ "axe",        	    &gsn_axe,        	    &gsk_axe },
	{ "backstab",         	&gsn_backstab,         	&gsk_backstab },
	{ "bar",	            &gsn_bar,	            &gsk_bar },
	{ "bash",   	        &gsn_bash,   	        &gsk_bash },
	{ "behead",	            &gsn_behead,	            &gsk_behead },
	{ "berserk",	        &gsn_berserk,	        &gsk_berserk },
	{ "bind",	            &gsn_bind,	            &gsk_bind },
	{ "bite",   	        &gsn_bite,   	        &gsk_bite },
	{ "blackjack",	        &gsn_blackjack,	        &gsk_blackjack },
	{ "bless",	            &gsn_bless,	            &gsk_bless },
	{ "blindness",  	    &gsn_blindness,  	    &gsk_blindness },
	{ "blowgun",	        &gsn_blowgun,	        &gsk_blowgun },
	{ "bomb",	            &gsn_bomb,	            &gsk_bomb },
	{ "bow",	            &gsn_bow,	            &gsk_bow },
	{ "breath", 	        &gsn_breath, 	        &gsk_breath },
	{ "brew",	            &gsn_brew,	            &gsk_brew },
	{ "burgle",	            &gsn_burgle,	            &gsk_burgle },
	{ "burning_hands",	    &gsn_burning_hands,	    &gsk_burning_hands },
	{ "call_familiar",	    &gsn_call_familiar,	    &gsk_call_familiar },
	{ "call_lightning",	    &gsn_call_lightning,	    &gsk_call_lightning },
	{ "calm",	            &gsn_calm,	            &gsk_calm },
	{ "cancellation",	    &gsn_cancellation,	    &gsk_cancellation },
	{ "catch",	            &gsn_catch,	            &gsk_catch },
	{ "cause_critical",	    &gsn_cause_critical,	    &gsk_cause_critical },
	{ "cause_light",	    &gsn_cause_light,	    &gsk_cause_light },
	{ "cause_serious",	    &gsn_cause_serious,	    &gsk_cause_serious },
	{ "chain_lightning",	&gsn_chain_lightning,	&gsk_chain_lightning },
	{ "channel",	        &gsn_channel,	        &gsk_channel },
	{ "charge",	            &gsn_charge,	            &gsk_charge },
	{ "charm_person",	    &gsn_charm_person,	    &gsk_charm_person },
	{ "chill_touch",	    &gsn_chill_touch,	    &gsk_chill_touch },
	{ "circle",	            &gsn_circle,	            &gsk_circle },
	{ "cloak_of_guile",	    &gsn_cloak_of_guile,	    &gsk_cloak_of_guile },
	{ "colour_spray",	    &gsn_colour_spray,	    &gsk_colour_spray },
	{ "combine",	        &gsn_combine,	        &gsk_combine },
	{ "consume",	        &gsn_consume,	        &gsk_consume },
	{ "continual_light",	&gsn_continual_light,	&gsk_continual_light },
	{ "control_weather",	&gsn_control_weather,	&gsk_control_weather },
	{ "cosmic_blast",	    &gsn_cosmic_blast,	    &gsk_cosmic_blast },
	{ "counterspell",	    &gsn_counterspell,	    &gsk_counterspell },
	{ "create_food",	    &gsn_create_food,	    &gsk_create_food },
	{ "create_rose",	    &gsn_create_rose,	    &gsk_create_rose },
	{ "create_spring",	    &gsn_create_spring,	    &gsk_create_spring },
	{ "create_water",	    &gsn_create_water,	    &gsk_create_water },
	{ "crippling_touch",	&gsn_crippling_touch,	&gsk_crippling_touch },
	{ "crossbow",	        &gsn_crossbow,	        &gsk_crossbow },
	{ "cure_blindness",	    &gsn_cure_blindness,	    &gsk_cure_blindness },
	{ "cure_critical",	    &gsn_cure_critical,	    &gsk_cure_critical },
	{ "cure_disease",	    &gsn_cure_disease,	    &gsk_cure_disease },
	{ "cure_light",	        &gsn_cure_light,	        &gsk_cure_light },
	{ "cure_poison",	    &gsn_cure_poison,	    &gsk_cure_poison },
	{ "cure_serious",	    &gsn_cure_serious,	    &gsk_cure_serious },
	{ "cure_toxic",	        &gsn_cure_toxic,	        &gsk_cure_toxic },
	{ "curse",	            &gsn_curse,	            &gsk_curse },
	{ "dagger",	            &gsn_dagger,	            &gsk_dagger },
	{ "dark_shroud",	    &gsn_dark_shroud,	    &gsk_dark_shroud },
	{ "death_grip",	        &gsn_death_grip,	        &gsk_death_grip },
	{ "deathbarbs",	        &gsn_deathbarbs,	        &gsk_deathbarbs },
	{ "deathsight",	        &gsn_deathsight,	        &gsk_deathsight },
	{ "deception",	        &gsn_deception,	        &gsk_deception },
	{ "deep_trance",	    &gsn_deep_trance,	    &gsk_deep_trance },
	{ "demonfire",	        &gsn_demonfire,	        &gsk_demonfire },
	{ "destruction",	    &gsn_destruction,	    &gsk_destruction },
	{ "detect_hidden",	    &gsn_detect_hidden,	    &gsk_detect_hidden },
	{ "detect_invis",	    &gsn_detect_invis,	    &gsk_detect_invis },
	{ "detect_magic",	    &gsn_detect_magic,	    &gsk_detect_magic },
	{ "detect_traps",	    &gsn_detect_traps,	    &gsk_detect_traps },
	{ "dirt",	            &gsn_dirt,	            &gsk_dirt },
	{ "dirt_kicking",	    &gsn_dirt_kicking,	    &gsk_dirt_kicking },
	{ "disarm",	            &gsn_disarm,	            &gsk_disarm },
	{ "discharge",	        &gsn_discharge,	        &gsk_discharge },
	{ "dispel_evil",	    &gsn_dispel_evil,	    &gsk_dispel_evil },
	{ "dispel_good",	    &gsn_dispel_good,	    &gsk_dispel_good },
	{ "dispel_magic",	    &gsn_dispel_magic,	    &gsk_dispel_magic },
	{ "dispel_room",	    &gsn_dispel_room,	    &gsk_dispel_room },
	{ "dodge",	            &gsn_dodge,	            &gsk_dodge },
	{ "dual",	            &gsn_dual,	            &gsk_dual },
	{ "eagle_eye",	        &gsn_eagle_eye,	        &gsk_eagle_eye },
	{ "earth_spells",	    &gsn_earth_spells,	    &gsk_earth_spells },
	{ "earth_walk",	        &gsn_earth_walk,	        &gsk_earth_walk },
	{ "earthquake",	        &gsn_earthquake,	        &gsk_earthquake },
	{ "electrical_barrier",	&gsn_electrical_barrier,	&gsk_electrical_barrier },
	{ "enchant_armour",	    &gsn_enchant_armour,	    &gsk_enchant_armour },
	{ "enchant_weapon",	    &gsn_enchant_weapon,	    &gsk_enchant_weapon },
	{ "energy_drain",	    &gsn_energy_drain,	    &gsk_energy_drain },
	{ "energy_field",	    &gsn_energy_field,	    &gsk_energy_field },
	{ "enhanced_damage",	&gsn_enhanced_damage,	&gsk_enhanced_damage },
	{ "ensnare",	        &gsn_ensnare,	        &gsk_ensnare },
	{ "entrap",	            &gsn_entrap,	            &gsk_entrap },
	{ "envenom",	        &gsn_envenom,	        &gsk_envenom },
	{ "evasion",	        &gsn_evasion,	        &gsk_evasion },
	{ "exorcism",	        &gsn_exorcism,	        &gsk_exorcism },
	{ "exotic",	            &gsn_exotic,	            &gsk_exotic },
	{ "fade",	            &gsn_fade,	            &gsk_fade },
	{ "faerie_fire",	    &gsn_faerie_fire,	    &gsk_faerie_fire },
	{ "faerie_fog",	        &gsn_faerie_fog,	        &gsk_faerie_fog },
	{ "fast_healing",	    &gsn_fast_healing,	    &gsk_fast_healing },
	{ "fatigue",	        &gsn_fatigue,	        &gsk_fatigue },
	{ "feign",	            &gsn_feign,	            &gsk_feign },
	{ "fire_barrier",	    &gsn_fire_barrier,	    &gsk_fire_barrier },
	{ "fire_breath",	    &gsn_fire_breath,	    &gsk_fire_breath },
	{ "fire_cloud",	        &gsn_fire_cloud,	        &gsk_fire_cloud },
	{ "fire_spells",	    &gsn_fire_spells,	    &gsk_fire_spells },
	{ "fireball",	        &gsn_fireball,	        &gsk_fireball },
	{ "fireproof",	        &gsn_fireproof,	        &gsk_fireproof },
	{ "flail",	            &gsn_flail,	            &gsk_flail },
	{ "flamestrike",	    &gsn_flamestrike,	    &gsk_flamestrike },
	{ "flash",	            &gsn_flash,	            &gsk_flash },
	{ "flight",	            &gsn_flight,	            &gsk_flight },
	{ "fly",	            &gsn_fly,	            &gsk_fly },
	{ "fourth_attack",	    &gsn_fourth_attack,	    &gsk_fourth_attack },
	{ "frenzy",	            &gsn_frenzy,	            &gsk_frenzy },
	{ "frost_barrier",	    &gsn_frost_barrier,	    &gsk_frost_barrier },
	{ "frost_breath",	    &gsn_frost_breath,	    &gsk_frost_breath },
	{ "gas_breath",	        &gsn_gas_breath,	        &gsk_gas_breath },
	{ "gate",	            &gsn_gate,	            &gsk_gate },
	{ "giant_strength",	    &gsn_giant_strength,	    &gsk_giant_strength },
	{ "glacial_wave",	    &gsn_glacial_wave,	    &gsk_glacial_wave },
	{ "glorious_bolt",	    &gsn_glorious_bolt,	    &gsk_glorious_bolt },
	{ "haggle",	            &gsn_haggle,	            &gsk_haggle },
	{ "hand_to_hand",	    &gsn_hand_to_hand,	    &gsk_hand_to_hand },
	{ "harm",	            &gsn_harm,	            &gsk_harm },
	{ "harpooning",	        &gsn_harpooning,	        &gsk_harpooning },
	{ "haste",	            &gsn_haste,	            &gsk_haste },
	{ "heal",	            &gsn_heal,	            &gsk_heal },
	{ "healing_aura",	    &gsn_healing_aura,	    &gsk_healing_aura },
	{ "healing_hands",	    &gsn_healing_hands,	    &gsk_healing_hands },
	{ "hide",	            &gsn_hide,	            &gsk_hide },
	{ "holdup",	            &gsn_holdup,	            &gsk_holdup },
	{ "holy_shield",	    &gsn_holy_shield,	    &gsk_holy_shield },
	{ "holy_sword",	        &gsn_holy_sword,	        &gsk_holy_sword },
	{ "holy_word",	        &gsn_holy_word,	        &gsk_holy_word },
	{ "holy_wrath",	        &gsn_holy_wrath,	        &gsk_holy_wrath },
	{ "hunt",	            &gsn_hunt,	            &gsk_hunt },
	{ "ice_shards",	        &gsn_ice_shards,	        &gsk_ice_shards },
	{ "ice_storm",	        &gsn_ice_storm,	        &gsk_ice_storm },
	{ "identify",	        &gsn_identify,	        &gsk_identify },
	{ "improved_invis",	    &gsn_improved_invisibility,	    &gsk_improved_invisibility },
	{ "inferno",	        &gsn_inferno,	        &gsk_inferno },
	{ "infravision",	    &gsn_infravision,	    &gsk_infravision },
	{ "infuse",	            &gsn_infuse,	            &gsk_infuse },
	{ "intimidate",	        &gsn_intimidate,	        &gsk_intimidate },
	{ "invis",	            &gsn_invis,	            &gsk_invis },
	{ "judge",	            &gsn_judge,	            &gsk_judge },
	{ "kick",	            &gsn_kick,	            &gsk_kick },
	{ "kill",	            &gsn_kill,	            &gsk_kill },
	{ "leadership",	        &gsn_leadership,	        &gsk_leadership },
	{ "light_shroud",	    &gsn_light_shroud,	    &gsk_light_shroud },
	{ "lightning_bolt",	    &gsn_lightning_bolt,	    &gsk_lightning_bolt },
	{ "lightning_breath",	&gsn_lightning_breath,	&gsk_lightning_breath },
	{ "locate_object",	    &gsn_locate_object,	    &gsk_locate_object },
	{ "lore",	            &gsn_lore,	            &gsk_lore },
	{ "mace",	            &gsn_mace,	            &gsk_mace },
	{ "magic_missile",	    &gsn_magic_missile,	    &gsk_magic_missile },
	{ "martial_arts",	    &gsn_martial_arts,	    &gsk_martial_arts },
	{ "mass_healing",	    &gsn_mass_healing,	    &gsk_mass_healing },
	{ "mass_invis",	        &gsn_mass_invis,	        &gsk_mass_invis },
	{ "master_weather",	    &gsn_master_weather,	    &gsk_master_weather },
	{ "maze",	            &gsn_maze,	            &gsk_maze },
	{ "meditation",	        &gsn_meditation,	        &gsk_meditation },
	{ "mob_lore",	        &gsn_mob_lore,	        &gsk_mob_lore },
	{ "momentary_darkness",	&gsn_momentary_darkness,	&gsk_momentary_darkness },
	{ "morphlock",	        &gsn_morphlock,	        &gsk_morphlock },
	{ "mount_and_weapon_style",	&gsn_mount_and_weapon_style,	&gsk_mount_and_weapon_style },
	{ "music",	            &gsn_music,	            &gsk_music },
	{ "navigation",	        &gsn_navigation,	        &gsk_navigation },
	{ "neurotoxin",	        &gsn_neurotoxin,	        &gsk_neurotoxin },
	{ "nexus",	            &gsn_nexus,	            &gsk_nexus },
	{ "parry",	            &gsn_parry,	            &gsk_parry },
	{ "pass_door",	        &gsn_pass_door,	        &gsk_pass_door },
	{ "peek",	            &gsn_peek,	            &gsk_peek },
	{ "pick_lock",	        &gsn_pick_lock,	        &gsk_pick_lock },
	{ "plague",	            &gsn_plague,	            &gsk_plague },
	{ "poison",	            &gsn_poison,	            &gsk_poison },
	{ "polearm",	        &gsn_polearm,	        &gsk_polearm },
	{ "possess",	        &gsn_possess,	        &gsk_possess },
	{ "pursuit",	        &gsn_pursuit,	        &gsk_pursuit },
	{ "quarterstaff",	    &gsn_quarterstaff,	    &gsk_quarterstaff },
	{ "raise_dead",	        &gsn_raise_dead,	        &gsk_raise_dead },
	{ "recall",	            &gsn_recall,	            &gsk_recall },
	{ "recharge",	        &gsn_recharge,	        &gsk_recharge },
	{ "refresh",	        &gsn_refresh,	        &gsk_refresh },
	{ "regeneration",	    &gsn_regeneration,	    &gsk_regeneration },
	{ "remove_curse",	    &gsn_remove_curse,	    &gsk_remove_curse },
	{ "rending",	        &gsn_rending,	        &gsk_rending },
	{ "repair",	            &gsn_repair,	            &gsk_repair },
	{ "rescue",	            &gsn_rescue,	            &gsk_rescue },
	{ "resurrect",	        &gsn_resurrect,	        &gsk_resurrect },
	{ "reverie",	        &gsn_reverie,	        &gsk_reverie },
	{ "riding",	            &gsn_riding,	            &gsk_riding },
	{ "room_shield",	    &gsn_room_shield,	    &gsk_room_shield },
	{ "sanctuary",	        &gsn_sanctuary,	        &gsk_sanctuary },
	{ "scan",	            &gsn_scan,	            &gsk_scan },
	{ "scribe",	            &gsn_scribe,	            &gsk_scribe },
	{ "scrolls",	        &gsn_scrolls,	        &gsk_scrolls },
	{ "scry",	            &gsn_scry,	            &gsk_scry },
	{ "second_attack",	    &gsn_second_attack,	    &gsk_second_attack },
	{ "sense_danger",	    &gsn_sense_danger,	    &gsk_sense_danger },
	{ "shape",	            &gsn_shape,	            &gsk_shape },
	{ "shield",	            &gsn_shield,	            &gsk_shield },
	{ "shield_block",	    &gsn_shield_block,	    &gsk_shield_block },
	{ "shield_weapon_style",	&gsn_shield_weapon_style,	&gsk_shield_weapon_style },
	{ "shift",	            &gsn_shift,	            &gsk_shift },
	{ "shocking_grasp",	    &gsn_shocking_grasp,	    &gsk_shocking_grasp },
	{ "shriek",	            &gsn_shriek,	            &gsk_shriek },
	{ "silence",	        &gsn_silence,	        &gsk_silence },
	{ "single_style",	    &gsn_single_style,	    &gsk_single_style },
	{ "skull",	            &gsn_skull,	            &gsk_skull },
	{ "sleep",	            &gsn_sleep,	            &gsk_sleep },
	{ "slit_throat",	    &gsn_slit_throat,	    &gsk_slit_throat },
	{ "slow",	            &gsn_slow,	            &gsk_slow },
	{ "smite",	            &gsn_smite,	            &gsk_smite },
	{ "sneak",	            &gsn_sneak,	            &gsk_sneak },
	{ "soul_essence",	    &gsn_soul_essence,	    &gsk_soul_essence },
	{ "spear",	            &gsn_spear,	            &gsk_spear },
	{ "spell_deflection",	&gsn_spell_deflection,	&gsk_spell_deflection },
	{ "spell_shield",	    &gsn_spell_shield,	    &gsk_spell_shield },
	{ "spell_trap",	        &gsn_spell_trap,	        &gsk_spell_trap },
	{ "spirit_rack",	    &gsn_spirit_rack,	    &gsk_spirit_rack },
	{ "stake",	            &gsn_stake,	            &gsk_stake },
	{ "starflare",	        &gsn_starflare,	        &gsk_starflare },
	{ "staves",	            &gsn_staves,	            &gsk_staves },
	{ "steal",	            &gsn_steal,	            &gsk_steal },
	{ "stone_skin",	        &gsn_stone_skin,	        &gsk_stone_skin },
	{ "stone_spikes",	    &gsn_stone_spikes,	    &gsk_stone_spikes },
	{ "stone_touch",	    &gsn_stone_touch,	    &gsk_stone_touch },
	{ "subvert",	        &gsn_subvert,	        &gsk_subvert },
	{ "summon",	            &gsn_summon,	            &gsk_summon },
	{ "survey",	            &gsn_survey,	            &gsk_survey },
	{ "swerve",	            &gsn_swerve,	            &gsk_swerve },
	{ "sword",	            &gsn_sword,	            &gsk_sword },
	{ "sword_and_dagger_style",	&gsn_sword_and_dagger_style,	&gsk_sword_and_dagger_style },
	{ "tail_kick",	        &gsn_tail_kick,	        &gsk_tail_kick },
	{ "tattoo",	            &gsn_tattoo,	            &gsk_tattoo },
	{ "temperance",	        &gsn_temperance,	        &gsk_temperance },
	{ "third_attack",	    &gsn_third_attack,	    &gsk_third_attack },
	{ "third_eye",	        &gsn_third_eye,	        &gsk_third_eye },
	{ "throw",	            &gsn_throw,	            &gsk_throw },
	{ "titanic_attack",	    &gsn_titanic_attack,	    &gsk_titanic_attack },
	{ "toxic_fumes",	    &gsn_toxic_fumes,	    &gsk_toxic_fumes },
	{ "toxins",	            &gsn_toxins,	            &gsk_toxins },
	{ "trackless_step",	    &gsn_trackless_step,	    &gsk_trackless_step },
	{ "trample",	        &gsn_trample,	        &gsk_trample },
	{ "trip",	            &gsn_trip,	            &gsk_trip },
	{ "turn_undead",	    &gsn_turn_undead,	    &gsk_turn_undead },
	{ "two_handed_style",	&gsn_two_handed_style,	&gsk_two_handed_style },
	{ "underwater_breathing",	&gsn_underwater_breathing,	&gsk_underwater_breathing },
	{ "vision",	            &gsn_vision,	            &gsk_vision },
	{ "wands",	            &gsn_wands,	            &gsk_wands },
	{ "warcry",	            &gsn_warcry,	            &gsk_warcry },
	{ "water_spells",	    &gsn_water_spells,	    &gsk_water_spells },
	{ "weaken",	            &gsn_weaken,	            &gsk_weaken },
	{ "weaving",	        &gsn_weaving,	        &gsk_weaving },
	{ "web",	            &gsn_web,	            &gsk_web },
	{ "whip",	            &gsn_whip,	            &gsk_whip },
	{ "wilderness_spear_style",	&gsn_wilderness_spear_style,	&gsk_wilderness_spear_style },
	{ "wind_of_confusion",	&gsn_wind_of_confusion,	&gsk_wind_of_confusion },
	{ "withering_cloud",	&gsn_withering_cloud,	&gsk_withering_cloud },
	{ "word_of_recall",	    &gsn_word_of_recall,	    &gsk_word_of_recall },
    { NULL, NULL }
};

const struct flag_type skill_flags[] =
{
    { "can_brew",           SKILL_CAN_BREW,         TRUE  },
    { "can_cast",           SKILL_CAN_CAST,         TRUE  },
    { "can_imbue",          SKILL_CAN_IMBUE,        TRUE  },
    { "can_ink",            SKILL_CAN_INK,          TRUE  },
    { "can_scribe",         SKILL_CAN_SCRIBE,       TRUE  },
    { "cross_class",        SKILL_CROSS_CLASS,      FALSE },    // NYI
    { "spell_pulse",        SKILL_SPELL_PULSE,      TRUE  },
    { NULL,                 0,                      FALSE }
};
