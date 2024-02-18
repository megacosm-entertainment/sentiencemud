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
#include "interp.h"
#include "tables.h"
#include "scripts.h"
#include "magic.h"
#include "music.h"
#include "classes.h"

const struct hint_type hintsTable[] =
{
    {"The map shows you a small map of the surrounding area. To turn it off type 'toggle map'.\n\r"},
    {"Players with an {W[H]{M by their name are designated helpers. Feel free to ask them for help or advice.\n\r"},
    {"If you need any help try asking over the 'helper' channel to page a helper or immortal.\n\r"},
    {"If you are lost you can type 'recall' at any time to return to a familiar place.\n\r"},
    {"You can recover mana or hit points by sleeping or resting. To do this, type 'sleep' or 'rest'. Type 'wake' or 'stand' once you have finished.\n\r"},
    {"You can type \"hints\" at any time to remove these messages.\n\r"},
    {"There is a guide available for new players at https://playerguide.sentiencemud.net.\n\r"},
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
   {    "neuter",   SEX_NEUTRAL,    true },
   {	"male",		SEX_MALE,       true },
   {	"female",	SEX_FEMALE,     true },
   {	"either",	SEX_EITHER,     true },
   {	NULL,		0,              false }
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


const struct string_type fragile_table[] =
{
    {   "Weak"  	},
    {   "Normal"  	},
    {   "Strong" 	},
    {   "Solid"    	},
    {   NULL        }
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
    {	"nolock",		EX_NOLOCK,		true	},
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
    {	"nolock",		EX_NOLOCK,		true	},
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
	{	"dark",				ROOM_DARK,				true	},
	{	"gods_only",		ROOM_GODS_ONLY,			true	},
	{	"imp_only",			ROOM_IMP_ONLY,			true	},
	{	"indoors",			ROOM_INDOORS,			true	},
	{	"newbies_only",		ROOM_NEWBIES_ONLY,		true	},
	{	"no_map",			ROOM_NOMAP,				true	},
	{	"no_mob",			ROOM_NO_MOB,			true	},
	{	"no_recall",		ROOM_NO_RECALL,			true	},
	{	"no_wander",		ROOM_NO_WANDER,			true	},
	{	"noview",			ROOM_NOVIEW,			true	},
	{	"pet_shop",			ROOM_PET_SHOP,			true	},
	{	"private",			ROOM_PRIVATE,			true	},
	{	"safe",				ROOM_SAFE,				true	},
	{	"solitary",			ROOM_SOLITARY,			true	},
	{	"arena",			ROOM_ARENA,				true	},
	{	"bank",				ROOM_BANK,				true	},
	{	"chaotic",			ROOM_CHAOTIC,			true	},
	{	"dark_attack",		ROOM_ATTACK_IF_DARK,	true	},
	{	"death_trap",		ROOM_DEATH_TRAP,		true	},
	{	"helm",				ROOM_SHIP_HELM,			true	},
	{	"locker",			ROOM_LOCKER,			true	},
	{	"nocomm",			ROOM_NOCOMM,			true	},
	{	"nomagic",			ROOM_NOMAGIC,			true	},
	{	"nowhere",			ROOM_NOWHERE,			true	},
	{	"pk",				ROOM_PK,				true	},
	{	"real_estate",		ROOM_HOUSE_UNSOLD,		true	},
	{	"rocks",			ROOM_ROCKS,				true	},
	{	"ship_shop",		ROOM_SHIP_SHOP,			true	},
	{	"underwater",		ROOM_UNDERWATER,		true	},
	{	"view_wilds",		ROOM_VIEWWILDS,			true	},
	{	NULL,	0,	0	}
};


const struct flag_type room2_flags[] =
{
    {	"alchemy",				ROOM_ALCHEMY,			true	},
    {	"bar",					ROOM_BAR,				true	},
    {	"briars",				ROOM_BRIARS,			true	},
    {	"citymove",				ROOM_CITYMOVE,			true	},
    {	"drain_mana",			ROOM_DRAIN_MANA,		true	},
    {	"hard_magic",			ROOM_HARD_MAGIC,		true	},
    {	"multiplay",			ROOM_MULTIPLAY,			true	},
    {	"slow_magic",			ROOM_SLOW_MAGIC,		true	},
    {	"toxic_bog",			ROOM_TOXIC_BOG,			true	},
    {	"virtual_room",			ROOM_VIRTUAL_ROOM,		false	},
    {   "fire",					ROOM_FIRE,				true    },
    {   "icy",					ROOM_ICY,				true    },
    {   "no_quest",				ROOM_NO_QUEST,			true    },
    {   "no_quit",				ROOM_NO_QUIT,			true 	},
    {   "post_office",			ROOM_POST_OFFICE,		true	},
    {	"underground",			ROOM_UNDERGROUND,		true	},
    {	"vis_on_map",			ROOM_VISIBLE_ON_MAP,	true	},
    {	"no_floor",				ROOM_NOFLOOR,			true	},
    {	"clone_persist",		ROOM_CLONE_PERSIST,		true	},
    {	"blueprint",			ROOM_BLUEPRINT, 		true	},
    {	"no_clone",				ROOM_NOCLONE,			true	},
    {	"no_get_random",		ROOM_NO_GET_RANDOM,		true	},
    {	"always_update",		ROOM_ALWAYS_UPDATE,		true	},
    {	"safe_harbor",			ROOM_SAFE_HARBOR,		true	},
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
    {	"npccorpse",		ITEM_CORPSE_NPC,	true	},
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


/*
 * Used when adding an affect to tell where it goes.
 * See addaffect and delaffect in act_olc.c
 */
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

const struct flag_type wear_loc_names[] =
{
	{ "NONE",			WEAR_NONE,	true },
	{ "LIGHT",			WEAR_LIGHT,	true },
	{ "FINGER_L",		WEAR_FINGER_L,	true },
	{ "FINGER_R",		WEAR_FINGER_R,	true },
	{ "NECK_1",		WEAR_NECK_1,	true },
	{ "NECK_2",		WEAR_NECK_2,	true },
	{ "BODY",			WEAR_BODY,	true },
	{ "HEAD",			WEAR_HEAD,	true },
	{ "LEGS",			WEAR_LEGS,	true },
	{ "FEET",			WEAR_FEET,	true },
	{ "HANDS",			WEAR_HANDS,	true },
	{ "ARMS",			WEAR_ARMS,	true },
	{ "SHIELD",		WEAR_SHIELD,	true },
	{ "ABOUT",			WEAR_ABOUT,	true },
	{ "WAIST",			WEAR_WAIST,	true },
	{ "WRIST_L",		WEAR_WRIST_L,	true },
	{ "WRIST_R",		WEAR_WRIST_R,	true },
	{ "WIELD",			WEAR_WIELD,	true },
	{ "HOLD",			WEAR_HOLD,	true },
	{ "SECONDARY",		WEAR_SECONDARY,	true },
	{ "RING_FINGER",	WEAR_RING_FINGER,	true },
	{ "BACK",			WEAR_BACK,	true },
	{ "SHOULDER",		WEAR_SHOULDER,	true },
	{ "ANKLE_L",		WEAR_ANKLE_L,	true },
	{ "ANKLE_R",		WEAR_ANKLE_R,	true },
	{ "EAR_L",			WEAR_EAR_L,	true },
	{ "EAR_R",			WEAR_EAR_R,	true },
	{ "EYES",			WEAR_EYES,	true },
	{ "FACE",			WEAR_FACE,	true },
	{ "TATTOO_HEAD",	WEAR_TATTOO_HEAD,	true },
	{ "TATTOO_TORSO",	WEAR_TATTOO_TORSO,	true },
	{ "TATTOO_UPPER_ARM_L",	WEAR_TATTOO_UPPER_ARM_L,	true },
	{ "TATTOO_UPPER_ARM_R",	WEAR_TATTOO_UPPER_ARM_R,	true },
	{ "TATTOO_UPPER_LEG_L",	WEAR_TATTOO_UPPER_LEG_L,	true },
	{ "TATTOO_UPPER_LEG_R",	WEAR_TATTOO_UPPER_LEG_R,	true },
	{ "LODGED_HEAD",	WEAR_LODGED_HEAD,	true },
	{ "LODGED_TORSO",	WEAR_LODGED_TORSO,	true },
	{ "LODGED_ARM_L",	WEAR_LODGED_ARM_L,	true },
	{ "LODGED_ARM_R",	WEAR_LODGED_ARM_R,	true },
	{ "LODGED_LEG_L",	WEAR_LODGED_LEG_L,	true },
	{ "LODGED_LEG_R",	WEAR_LODGED_LEG_R,	true },
	{ "ENTANGLED",		WEAR_ENTANGLED,	true },
	{ "CONCEALED",		WEAR_CONCEALED,	true },
	{ "FLOATING",		WEAR_FLOATING,	true },
        { "TATTOO_LOWER_ARM_L",        WEAR_TATTOO_LOWER_ARM_L,       true },
        { "TATTOO_LOWER_ARM_R",       WEAR_TATTOO_LOWER_ARM_R,      true },
        { "TATTOO_LOWER_LEG_L",       WEAR_TATTOO_LOWER_LEG_L,      true },
        { "TATTOO_LOWER_LEG_R",       WEAR_TATTOO_LOWER_LEG_R,      true },
        { "TATTOO_SHOULDER_L",       WEAR_TATTOO_SHOULDER_L,      true },
        { "TATTOO_SHOULDER_R",       WEAR_TATTOO_SHOULDER_R,      true },
        { "TATTOO_BACK",       WEAR_TATTOO_BACK,      true },
        { "TATTOO_NECK",       WEAR_TATTOO_NECK,      true },

    {	NULL,			0,			0	}

};


const struct flag_type wear_loc_strings[] =
{
    {	"in the inventory",	WEAR_NONE,		true	},
    {	"as a light",		WEAR_LIGHT,		true	},
    {	"on the left finger",	WEAR_FINGER_L,		true	},
    {	"on the right finger",	WEAR_FINGER_R,		true	},
    {	"around the neck (1)",	WEAR_NECK_1,		true	},
    {	"around the neck (2)",	WEAR_NECK_2,		true	},
    {	"on the body",		WEAR_BODY,		true	},
    {	"over the head",	WEAR_HEAD,		true	},
    {	"on the legs",		WEAR_LEGS,		true	},
    {	"on the feet",		WEAR_FEET,		true	},
    {	"on the hands",		WEAR_HANDS,		true	},
    {	"on the arms",		WEAR_ARMS,		true	},
    {	"as a shield",		WEAR_SHIELD,		true	},
    {	"about the shoulders",	WEAR_ABOUT,		true	},
    {	"around the waist",	WEAR_WAIST,		true	},
    {	"on the left wrist",	WEAR_WRIST_L,		true	},
    {	"on the right wrist",	WEAR_WRIST_R,		true	},
    {	"wielded",		WEAR_WIELD,		true	},
    {	"held in the hands",	WEAR_HOLD,		true	},
    {	"off-hand",		WEAR_SECONDARY,		true	},	// NIB : 20070121 : was missing
    {	"on the ring finger",	WEAR_RING_FINGER,	true	},	// NIB : 20070121 : was missing
    {	"on the back",		WEAR_BACK,		true	},	// NIB : 20070121 : was missing
    {	"on the shoulders",	WEAR_SHOULDER,		true	},	// NIB : 20070121 : was missing
    {	"around left ankle",	WEAR_ANKLE_L,		true	},
    {	"around right ankle",	WEAR_ANKLE_R,		true	},
    {	"on the left ear",	WEAR_EAR_L,		true	},
    {	"on the right ear",	WEAR_EAR_R,		true	},
    {	"over the eyes",	WEAR_EYES,		true	},
    {	"on the face",		WEAR_FACE,		true	},
    {	"tattooed on head",	WEAR_TATTOO_HEAD,	true	},
    {	"tattooed on torso",	WEAR_TATTOO_TORSO,	true	},
    {	"tattooed on upper left arm",	WEAR_TATTOO_UPPER_ARM_L,	true	},
    {	"tattooed on upper right arm",	WEAR_TATTOO_UPPER_ARM_R,	true	},
    {	"tattooed on upper left leg",	WEAR_TATTOO_UPPER_LEG_L,	true	},
    {	"tattooed on upper right leg",	WEAR_TATTOO_UPPER_LEG_R,	true	},
    {	"lodged in head",	WEAR_LODGED_HEAD,	true	},
    {	"lodged in torso",	WEAR_LODGED_TORSO,	true	},
    {	"lodged in left arm",	WEAR_LODGED_ARM_L,	true	},
    {	"lodged in right arm",	WEAR_LODGED_ARM_R,	true	},
    {	"lodged in left leg",	WEAR_LODGED_LEG_L,	true	},
    {	"lodged in right leg",	WEAR_LODGED_LEG_R,	true	},
    {	"entangled",		WEAR_ENTANGLED,		true	},
    {	"concealed from view",	WEAR_CONCEALED,		true	},
    {	"floating in the air",	WEAR_FLOATING,		true	},
    {   "tattooed on lower left arm",     WEAR_TATTOO_LOWER_ARM_L,       true    },
    {   "tattooed on lower right arm",    WEAR_TATTOO_LOWER_ARM_R,      true    },
    {   "tattooed on lower left leg",   WEAR_TATTOO_LOWER_LEG_L,      true    },
    {   "tattooed on lower right leg",  WEAR_TATTOO_LOWER_LEG_R,      true    },
    {   "tattooed on left shoulder",   WEAR_TATTOO_SHOULDER_L,      true    },
    {   "tattooed on right shoulder",  WEAR_TATTOO_SHOULDER_R,      true    },
    {   "tattooed on back",  WEAR_TATTOO_BACK,      true    },
    {   "tattooed on neck",  WEAR_TATTOO_NECK,      true    },
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

const struct weapon_ammo_type weapon_ammo_table[] =
{
    { WEAPON_BLOWGUN,   AMMO_DART },
    { WEAPON_BOW,       AMMO_ARROW },
    { WEAPON_CROSSBOW,  AMMO_BOLT },
    //{ WEAPON_GUN,      AMMO_BULLET },
    { WEAPON_UNKNOWN,   AMMO_NONE }
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
// @@@NIB : 20070120 : Added for use in CHK_POS ifcheck
    {   "feigned",	POS_FEIGN,        	true    },	// @@@NIB : 20070126 : made selectable in OLC
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
    {0},        // 0 - Never Used, just here because the level is 1-based, the array is 0-based
    {250},      // 1
    {500},      // 2
    {1000},     // 3
    {2500},     // 4
    {3000},     // 5
    {4000},     // 6
    {7500},     // 7
    {8000},     // 8
    {9000},     // 9
    {10000},    // 10
    {12500},    // 11
    {15000},    // 12
    {17500},    // 13
    {18500},    // 14
    {20500},    // 15
    {25000},    // 16
    {40000},    // 17
    {60000},    // 18
    {90000},    // 19
    {100000},   // 20
    {120000},   // 21
    {130000},   // 22
    {130000},   // 23
    {150000},   // 24
    {175000},   // 25
    {250000},   // 26
    {500000},   // 27
    {750000},   // 28
    {850000},   // 29
    {1000000},  // 30
    {1500000},  // 31 - becomes significantly harder
    {2000000},  // 32
    {3000000},  // 33
    {4000000},  // 34
    {5000000},  // 35
    {7500000},  // 36
    {10000000}, // 37
    {12000000}, // 38
    {15000000}, // 39
    {0},        // 40 - Current Max Level
#if 0
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
#endif
};


const struct flag_type room_condition_flags[] =
{
    { 	"season",	CONDITION_SEASON,	true	},
    { 	"sky",		CONDITION_SKY,		true    },
    { 	"hour",		CONDITION_HOUR,		true	},
    { 	"script",	CONDITION_SCRIPT,	true	},
    {	NULL,		0,			true    }
};


const struct npc_ship_hotspot_type npc_ship_hotspot_table[] =
{
    {   427, 208,           1 },//NPC_SHIP_OLARIA_SAILING },
    {   0,   0,             0 }
};


const struct flag_type      place_flags[]           =
{
    { 	"Nowhere",    			PLACE_NOWHERE, 				true	},
    { 	"Wilderness",			PLACE_WILDERNESS,			true	},
    { 	"First Continent", 		PLACE_FIRST_CONTINENT, 		true	},
    { 	"Second Continent", 	PLACE_SECOND_CONTINENT, 	true	},
    { 	"Third Continent",	 	PLACE_THIRD_CONTINENT, 		true	},
    { 	"Fourth Continent", 	PLACE_FOURTH_CONTINENT, 	true	},
    { 	"Island", 				PLACE_ISLAND, 				true	},
    { 	"Other Plane",			PLACE_OTHER_PLANE,			true	},
    { 	"Abyss",				PLACE_ABYSS,				true	},
    { 	"Eden",					PLACE_EDEN,					true	},
    { 	"Netherworld",			PLACE_NETHERWORLD,			true	},
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


const struct flag_type  channel_flags[] =
{
    {	"ct",			FLAG_CT,		true	},
    {	"flame",		FLAG_FLAMING,		true	},
    {	"gossip",		FLAG_GOSSIP,		true	},
    {	"helper",		FLAG_HELPER,		true	},
    {	"music",		FLAG_MUSIC,		true	},
    {	"ooc",			FLAG_OOC,		true	},
    {	"quote",		FLAG_QUOTE,		true	},
    {	"tells",		FLAG_TELLS,		true	},
    {	"yell",			FLAG_YELL,		true	},
    {	NULL,			0,				false	}
};


const struct flag_type project_flags[] =
{
    {	"open",			PROJECT_OPEN,		true	},
    {	"assigned",		PROJECT_ASSIGNED,	true	},
    {	"hold",			PROJECT_HOLD,		true	},
    {	NULL,			0,			false	}
};


const struct flag_type immortal_flags[] =
{
    {	"Head Coder",		IMMORTAL_HEADCODER,	true	},
    {	"Head Builder",		IMMORTAL_HEADBUILDER, 	true	},
    {	"Administrator",	IMMORTAL_HEADADMIN, 	true	},
    {	"Staff",		IMMORTAL_STAFF,		true	},
    {	"Balance",		IMMORTAL_HEADBALANCE,	true	},
    {	"Player Relations",	IMMORTAL_HEADPR,	true	},
    {	"Coder",		IMMORTAL_CODER,		true	},
    {	"Builder",		IMMORTAL_BUILDER,	true	},
    {	"Housing",		IMMORTAL_HOUSING,	true	},
    {	"Churches",		IMMORTAL_CHURCHES,	true	},
    {	"Helpfiles",		IMMORTAL_HELPFILES,	true	},
    {	"Weddings",		IMMORTAL_WEDDINGS,	true	},
    {	"Testing",		IMMORTAL_TESTING,	true	},
    {	"Events",		IMMORTAL_EVENTS,	true	},
    {	"Website",		IMMORTAL_WEBSITE,	true	},
    {	"Story",		IMMORTAL_STORY,		true	},
    {	"Secretary",		IMMORTAL_SECRETARY,	true	},
    {	NULL,			0,			false	}
};

// @@@NIB : 20070120 : Added this matching IMM/RES/VULN to DAM_* codes
// Used by damage_class_lookup()
// Gets the DAM_* code for the given damage class
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
		false,false,true,100,100,100,RAWKILL_WITHERED,0,3,6,1,3,100,
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
		false,false,true,45,100,90,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,false,true,80,100,70,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,false,true,50,100,90,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,false,true,50,100,30,RAWKILL_SKELETAL,0,3,6,1,3,100,
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
		true,false,false,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,true,false,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		true,false,true,0,10,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,true,false,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,true,false,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,true,false,0,0,-1,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,false,true,0,100,100,RAWKILL_NOCORPSE,0,3,6,25,40,100,
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
		false,false,false,80,100,95,RAWKILL_SKELETAL,0,3,6,1,5,100,
		PART_HIDE|PART_SCALES
	},
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
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // DAM_DEATH
};

// When this becomes a linked list..... yeah
#define BLEND(a,b,c)	{ RAWKILL_##a, RAWKILL_##b, RAWKILL_##c, false }
#define BLENDD(a,b,c)	{ RAWKILL_##a, RAWKILL_##b, RAWKILL_##c, true }
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
	{ "charred",	CORPSE_CHARRED,			true },
	{ "frozen",		CORPSE_FROZEN,			true },
	{ "melted",		CORPSE_MELTED,			true },
	{ "withered",	CORPSE_WITHERED,		true },
	{ "pk",			CORPSE_PKDEATH,			true },
	{ "arena",		CORPSE_ARENADEATH,		true },
	{ "immortal",	CORPSE_IMMORTAL,		true },
	{ NULL,			0,						false }

};

const struct flag_type variable_types[] = {
	{"bool",			VAR_BOOLEAN,			true},
	{"integer",			VAR_INTEGER,			true},
	{"string",			VAR_STRING,				true},
	{"string_s",		VAR_STRING_S,			true},
	{"room",			VAR_ROOM,				true},
	{"exit",			VAR_EXIT,				true},
	{"mobile",			VAR_MOBILE,				true},
	{"object",			VAR_OBJECT,				true},
	{"token",			VAR_TOKEN,				true},
	{"area",			VAR_AREA,				true},
    {"aregion",         VAR_AREA_REGION,        true},
	{"skill",			VAR_SKILL,				true},
	{"skillinfo",		VAR_SKILLINFO,			true},
    {"song",            VAR_SONG,               true},
	{"connection",		VAR_CONNECTION,			true},
	{"affect",			VAR_AFFECT,				true},
	{"wilds",			VAR_WILDS,				true},
	{"church",			VAR_CHURCH,				true},
	{"clone_room",		VAR_CLONE_ROOM,			true},
	{"wilds_room",		VAR_WILDS_ROOM,			true},
	{"door",			VAR_DOOR,				true},
	{"clone_door",		VAR_CLONE_DOOR,			true},
	{"wilds_door",		VAR_WILDS_DOOR,			true},
	{"mobile_id",		VAR_MOBILE_ID,			true},
	{"object_id",		VAR_OBJECT_ID,			true},
	{"token_id",		VAR_TOKEN_ID,			true},
	{"skillinfo_id",	VAR_SKILLINFO_ID,		true},
	{"area_id",			VAR_AREA_ID,			true},
    {"aregion_id",      VAR_AREA_REGION_ID,     true},
	{"wilds_id",		VAR_WILDS_ID,			true},
	{"church_id",		VAR_CHURCH_ID,			true},
	{"variable",		VAR_VARIABLE,			true},
	{"bllist_room",		VAR_BLLIST_ROOM,		true},
	{"bllist_mob",		VAR_BLLIST_MOB,			true},
	{"bllist_obj",		VAR_BLLIST_OBJ,			true},
	{"bllist_tok",		VAR_BLLIST_TOK,			true},
	{"bllist_exit",		VAR_BLLIST_EXIT,		true},
	{"bllist_skill",	VAR_BLLIST_SKILL,		true},
	{"bllist_area",		VAR_BLLIST_AREA,		true},
    {"bllist_aregion",  VAR_BLLIST_AREA_REGION, true},
	{"bllist_wilds",	VAR_BLLIST_WILDS,		true},
	{"pllist_str",		VAR_PLLIST_STR,			true},
	{"pllist_conn",		VAR_PLLIST_CONN,		true},
	{"pllist_room",		VAR_PLLIST_ROOM,		true},
	{"pllist_mob",		VAR_PLLIST_MOB,			true},
	{"pllist_obj",		VAR_PLLIST_OBJ,			true},
	{"pllist_tok",		VAR_PLLIST_TOK,			true},
    {"pllist_area",     VAR_PLLIST_AREA,        true},
    {"pllist_aregion",  VAR_PLLIST_AREA_REGION, true},
	{"pllist_church",	VAR_PLLIST_CHURCH,		true},
	{"pllist_variable",	VAR_PLLIST_VARIABLE,	true},
	{ NULL,				VAR_UNKNOWN,			false }

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


const struct flag_type wilderness_regions[] =
{
    { "Arena Island",        REGION_ARENA_ISLAND, true },
    { "Central Ocean",       REGION_CENTRAL_OCEAN, true },
    { "Dragon Island",       REGION_DRAGON_ISLAND, true },
    { "Eastern Ocean",       REGION_EASTERN_OCEAN, true },
    { "First Continent",     REGION_FIRST_CONTINENT, true },
    { "Fourth Continent",    REGION_FOURTH_CONTINENT, true },
    { "Mordrake Island",     REGION_MORDRAKE_ISLAND, true },
    { "North Pole",          REGION_NORTH_POLE, true },
    { "Northern Ocean",      REGION_NORTHERN_OCEAN, true },
    { "Second Continent",    REGION_SECOND_CONTINENT, true },
    { "South Pole",          REGION_SOUTH_POLE, true },
    { "Southern Ocean",      REGION_SOUTHERN_OCEAN, true },
    { "Temple Island",       REGION_TEMPLE_ISLAND, true },
    { "Third Continent",     REGION_THIRD_CONTINENT, true },
    { "Undersea",            REGION_UNDERSEA, true },
    { "Western Ocean",       REGION_WESTERN_OCEAN, true },
    { "Unknown",             REGION_UNKNOWN, false},
    { NULL,                  REGION_UNKNOWN,             false }

	// Overworld regions

	// Abyss regions

	// Other regions

};

const struct flag_type area_region_flags[] =
{
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

const struct flag_type trigger_slots[] = 
{
    { "action",         TRIGSLOT_ACTION,        true },
    { "animate",        TRIGSLOT_ANIMATE,       true },
    { "attacks",        TRIGSLOT_ATTACKS,       true },
    { "combatstyle",    TRIGSLOT_COMBATSTYLE,   true },
    { "damage",         TRIGSLOT_DAMAGE,        true },
    { "fight",          TRIGSLOT_FIGHT,         true },
    { "general",        TRIGSLOT_GENERAL,       true },
    { "hits",           TRIGSLOT_HITS,          true },
    { "interrupt",      TRIGSLOT_INTERRUPT,     true },
    { "move",           TRIGSLOT_MOVE,          true },
    { "random",         TRIGSLOT_RANDOM,        true },
    { "repop",          TRIGSLOT_REPOP,         true },
    { "speech",         TRIGSLOT_SPEECH,        true },
    { "spell",          TRIGSLOT_SPELL,         true },
    { "verb",           TRIGSLOT_VERB,          true },
    { NULL,             -1,                     false}
};

const struct flag_type builtin_trigger_types[] = 
{
    { "act",		        TRIG_ACT,	true },
    { "afterdeath",		    TRIG_AFTERDEATH,	true },
    { "afterkill",		    TRIG_AFTERKILL,	true },
    { "aggression",         TRIG_AGGRESSION, true },
    { "ammo_break",         TRIG_AMMO_BREAK, true },
    { "ammo_mana",          TRIG_AMMO_MANA, true },
    { "ammo_scripted",      TRIG_AMMO_SCRIPTED, true },
    { "animate",		    TRIG_ANIMATE,	true },
    { "assist",		        TRIG_ASSIST,	true },
    { "attack_backstab",	TRIG_ATTACK_BACKSTAB,	true },
    { "attack_bash",		TRIG_ATTACK_BASH,	true },
    { "attack_behead",		TRIG_ATTACK_BEHEAD,	true },
    { "attack_bite",		TRIG_ATTACK_BITE,	true },
    { "attack_blackjack",	TRIG_ATTACK_BLACKJACK,	true },
    { "attack_circle",		TRIG_ATTACK_CIRCLE,	true },
    { "attack_counter",		TRIG_ATTACK_COUNTER,	true },
    { "attack_cripple",		TRIG_ATTACK_CRIPPLE,	true },
    { "attack_dirtkick",	TRIG_ATTACK_DIRTKICK,	true },
    { "attack_disarm",		TRIG_ATTACK_DISARM,	true },
    { "attack_intimidate",	TRIG_ATTACK_INTIMIDATE,	true },
    { "attack_kick",		TRIG_ATTACK_KICK,	true },
    { "attack_rend",		TRIG_ATTACK_REND,	true },
    { "attack_slit",		TRIG_ATTACK_SLIT,	true },
    { "attack_smite",		TRIG_ATTACK_SMITE,	true },
    { "attack_tailkick",	TRIG_ATTACK_TAILKICK,	true },
    { "attack_trample",		TRIG_ATTACK_TRAMPLE,	true },
    { "attack_turn",		TRIG_ATTACK_TURN,	true },
    { "barrier",		    TRIG_BARRIER,	true },
    { "block_aggression",   TRIG_BLOCK_AGGRESSION, true },
    { "blow",		        TRIG_BLOW,	true },
    { "blueprint_schematic",TRIG_BLUEPRINT_SCHEMATIC,	true },
    { "board",		        TRIG_BOARD,	true },
    { "book_page",			TRIG_BOOK_PAGE,	true },
    { "book_page_attach",	TRIG_BOOK_PAGE_ATTACH,	true },
    { "book_page_preattach",TRIG_BOOK_PAGE_PREATTACH,	true },
    { "book_page_prerip",	TRIG_BOOK_PAGE_PRERIP,	true },
    { "book_page_rip",		TRIG_BOOK_PAGE_RIP,	true },
    { "book_preread",		TRIG_BOOK_PREREAD,	true },
    { "book_preseal",		TRIG_BOOK_PRESEAL,	true },
    { "book_prewrite",		TRIG_BOOK_PREWRITE,	true },
    { "book_read",			TRIG_BOOK_READ,	true },
    { "book_seal",			TRIG_BOOK_SEAL,	true },
    { "book_write_text",	TRIG_BOOK_WRITE_TEXT,	true },
    { "book_write_title",	TRIG_BOOK_WRITE_TITLE,	true },
    { "brandish",	    	TRIG_BRANDISH,	true },
    { "bribe",		        TRIG_BRIBE,	true },
    { "bury",		        TRIG_BURY,	true },
    { "buy",		        TRIG_BUY,	true },
    { "caninterrupt",		TRIG_CANINTERRUPT,	true },
    { "cause_aggression",   TRIG_CAUSE_AGGRESSION,  true },
    { "catalyst",		    TRIG_CATALYST,	true },
    { "catalyst_full",		TRIG_CATALYST_FULL,	true },
    { "catalyst_source",	TRIG_CATALYST_SOURCE,	true },
    { "check_aggression",   TRIG_CHECK_AGGRESSION, true },
    { "check_buyer",		TRIG_CHECK_BUYER,	true },
    { "check_damage",		TRIG_CHECK_DAMAGE,	true },
    { "clone_extract",		TRIG_CLONE_EXTRACT,	true },
    { "close",		        TRIG_CLOSE,	true },
    { "collapse",           TRIG_COLLAPSE, true },
    { "combat_style",		TRIG_COMBAT_STYLE,	true },
    { "compass",            TRIG_COMPASS, true},
    { "completed",		    TRIG_COMPLETED,	true },
    { "contract_complete",	TRIG_CONTRACT_COMPLETE,	true },
    { "custom_price",		TRIG_CUSTOM_PRICE,	true },
    { "damage",		        TRIG_DAMAGE,	true },
    { "death",		        TRIG_DEATH,	true },
    { "death_protection",	TRIG_DEATH_PROTECTION,	true },
    { "death_timer",		TRIG_DEATH_TIMER,	true },
    { "defense",	    	TRIG_DEFENSE,	true },
    { "delay",		        TRIG_DELAY,	true },
    { "drink",		        TRIG_DRINK,	true },
    { "drop",		        TRIG_DROP,	true },
    { "dungeon_commenced",  TRIG_DUNGEON_COMMENCED, true },
    { "dungeon_schematic",	TRIG_DUNGEON_SCHEMATIC,	true },
    { "eat",		        TRIG_EAT,	true },
    { "emote",		        TRIG_EMOTE,	true },
    { "emoteat",		    TRIG_EMOTEAT,	true },
    { "emoteself",		    TRIG_EMOTESELF,	true },
    { "emptied",            TRIG_EMPTIED, true },
    { "entry",		        TRIG_ENTRY,	true },
    { "exall",		        TRIG_EXALL,	true },
    { "examine",		    TRIG_EXAMINE,	true },
    { "exit",		        TRIG_EXIT,	true },
    { "expand",             TRIG_EXPAND, true },
    { "expire",		        TRIG_EXPIRE,	true },
    { "extinguish",         TRIG_EXTINGUISH,    true },
    { "extract",		    TRIG_EXTRACT,	true },
    { "failed",		        TRIG_FAILED,	true },
    { "fight",		        TRIG_FIGHT,	true },
    { "fill",               TRIG_FILL,  true },
    { "filled",             TRIG_FILLED, true },
    { "flee",		        TRIG_FLEE,	true },
    { "fluid_emptied",      TRIG_FLUID_EMPTIED, true },
    { "fluid_filled",       TRIG_FLUID_FILLED, true },
    { "forcedismount",		TRIG_FORCEDISMOUNT,	true },
    { "get",		        TRIG_GET,	true },
    { "give",		        TRIG_GIVE,	true },
    { "grall",		        TRIG_GRALL,	true },
    { "greet",		        TRIG_GREET,	true },
    { "grouped",		    TRIG_GROUPED,	true },
    { "grow",		        TRIG_GROW,	true },
    { "hidden",		        TRIG_HIDDEN,	true },
    { "hide",		        TRIG_HIDE,	true },
    { "hit",		        TRIG_HIT,	true },
    { "hitch",		        TRIG_HITCH,	true },
    { "hitgain",		    TRIG_HITGAIN,	true },
    { "hpcnt",		        TRIG_HPCNT,	true },
    { "hunt_found",         TRIG_HUNT_FOUND,    true },
    { "identify",		    TRIG_IDENTIFY,	true },
    { "ignite",             TRIG_IGNITE,    true },
    { "inspect",		    TRIG_INSPECT,	true },
    { "interrupt",		    TRIG_INTERRUPT,	true },
    { "kill",		        TRIG_KILL,	true },
    { "knock",		        TRIG_KNOCK,	true },
    { "knocking",		    TRIG_KNOCKING,	true },
    { "land",		        TRIG_LAND,	true },
    { "lead",		        TRIG_LEAD,	true },
    { "level",		        TRIG_LEVEL,	true },
    { "lock",		        TRIG_LOCK,	true },
    { "login",		        TRIG_LOGIN,	true },
    { "lookat",             TRIG_LOOK_AT, true },
    { "lore",		        TRIG_LORE,	true },
    { "lore_ex",		    TRIG_LORE_EX,	true },
    { "managain",		    TRIG_MANAGAIN,	true },
    { "mission_cancel",     TRIG_MISSION_CANCEL,	true },
    { "mission_complete",   TRIG_MISSION_COMPLETE,	true },
    { "mission_incomplete", TRIG_MISSION_INCOMPLETE,	true },
    { "mission_part",       TRIG_MISSION_PART,	true },
    { "moon",		        TRIG_MOON,	true },
    { "mount",		        TRIG_MOUNT,	true },
    { "move_char",		    TRIG_MOVE_CHAR,	true },
    { "movegain",		    TRIG_MOVEGAIN,	true },
    { "multiclass",		    TRIG_MULTICLASS,	true },
    { "open",		        TRIG_OPEN,	true },
    { "orient",             TRIG_ORIENT, true },
    { "postmission",	    TRIG_POSTMISSION,	true },
    { "postreckoning",      TRIG_POSTRECKONING, true },
    { "pour",               TRIG_POUR,  true },
    { "practice",		    TRIG_PRACTICE,	true },
    { "practicetoken",		TRIG_PRACTICETOKEN,	true },
    { "preaggressive",      TRIG_PREAGGRESSIVE, true },
    { "preanimate",		    TRIG_PREANIMATE,	true },
    { "preassist",		    TRIG_PREASSIST,	true },
    { "prebite",		    TRIG_PREBITE,	true },
    { "prebrandish",        TRIG_PREBRANDISH,   true },
    { "prebuy",		        TRIG_PREBUY,	true },
    { "prebuy_obj",		    TRIG_PREBUY_OBJ,	true },
    { "precast",		    TRIG_PRECAST,	true },
    { "precollapse",        TRIG_PRECOLLAPSE, true},
    { "precompass",         TRIG_PRECOMPASS, true},
    { "predeath",		    TRIG_PREDEATH,	true },
    { "predismount",		TRIG_PREDISMOUNT,	true },
    { "predrink",		    TRIG_PREDRINK,	true },
    { "predrop",		    TRIG_PREDROP,	true },
    { "preeat",		        TRIG_PREEAT,	true },
    { "preenter",		    TRIG_PREENTER,	true },
    { "preexpand",          TRIG_PREEXPAND, true},
    { "preextinguish",      TRIG_PREEXTINGUISH,    true },
    { "preflee",		    TRIG_PREFLEE,	true },
    { "preget",		        TRIG_PREGET,	true },
    { "prehide",		    TRIG_PREHIDE,	true },
    { "prehide_in",		    TRIG_PREHIDE_IN,	true },
    { "prehitch",           TRIG_PREHITCH,   true },
    { "preignite",          TRIG_PREIGNITE,    true },
    { "prekill",		    TRIG_PREKILL,	true },
    { "prelead",		    TRIG_PRELEAD,	true },
    { "prelock",		    TRIG_PRELOCK,	true },
    { "premission",		    TRIG_PREMISSION,	true },
    { "premount",		    TRIG_PREMOUNT,	true },
    { "preorient",          TRIG_PREORIENT, true },
    { "prepractice",		TRIG_PREPRACTICE,	true },
    { "prepracticeother",	TRIG_PREPRACTICEOTHER,	true },
    { "prepracticethat",	TRIG_PREPRACTICETHAT,	true },
    { "prepracticetoken",	TRIG_PREPRACTICETOKEN,	true },
    { "prepull",            TRIG_PREPULL,   true },
    { "preput",		        TRIG_PREPUT,	true },
    { "prerecall",		    TRIG_PRERECALL,	true },
    { "prerecite",		    TRIG_PRERECITE,	true },
    { "prereckoning",		TRIG_PRERECKONING,	true },
    { "prerehearse",		TRIG_PREREHEARSE,	true },
    { "preremove",		    TRIG_PREREMOVE,	true },
    { "prerenew",		    TRIG_PRERENEW,	true },
    { "prerest",		    TRIG_PREREST,	true },
    { "preresurrect",		TRIG_PRERESURRECT,	true },
    { "preround",		    TRIG_PREROUND,	true },
    { "presell",		    TRIG_PRESELL,	true },
    { "presit",		        TRIG_PRESIT,	true },
    { "presleep",		    TRIG_PRESLEEP,	true },
    { "prespell",		    TRIG_PRESPELL,	true },
    { "prestand",		    TRIG_PRESTAND,	true },
    { "prestepoff",		    TRIG_PRESTEPOFF,	true },
    { "pretrain",		    TRIG_PRETRAIN,	true },
    { "pretraintoken",		TRIG_PRETRAINTOKEN,	true },
    { "preunhitch",         TRIG_PREUNHITCH,   true },
    { "preunlead",		    TRIG_PREUNLEAD,	true },
    { "preunlock",		    TRIG_PREUNLOCK,	true },
    { "preunyoke",          TRIG_PREUNYOKE,   true },
    { "prewake",		    TRIG_PREWAKE,	true },
    { "prewear",		    TRIG_PREWEAR,	true },
    { "prewimpy",		    TRIG_PREWIMPY,	true },
    { "preyoke",            TRIG_PREYOKE,   true },
    { "prezap",             TRIG_PREZAP,    true },
    { "pull",		        TRIG_PULL,	true },
    { "pull_on",		    TRIG_PULL_ON,	true },
    { "pulse",		        TRIG_PULSE,	true },
    { "push",		        TRIG_PUSH,	true },
    { "push_on",		    TRIG_PUSH_ON,	true },
    { "put",		        TRIG_PUT,	true },
    { "quit",		        TRIG_QUIT,	true },
    { "random",		        TRIG_RANDOM,	true },
    { "readycheck",         TRIG_READYCHECK,    true },
    { "recall",		        TRIG_RECALL,	true },
    { "recite",		        TRIG_RECITE,	true },
    { "reckoning",		    TRIG_RECKONING,	true },
    { "regen",	        	TRIG_REGEN,	true },
    { "regen_hp",		    TRIG_REGEN_HP,	true },
    { "regen_mana",		    TRIG_REGEN_MANA,	true },
    { "regen_move",		    TRIG_REGEN_MOVE,	true },
    { "remort",		        TRIG_REMORT,	true },
    { "remove",		        TRIG_REMOVE,	true },
    { "renew",		        TRIG_RENEW,	true },
    { "renew_list",		    TRIG_RENEW_LIST,	true },
    { "repop",		        TRIG_REPOP,	true },
    { "reputation_gain",    TRIG_REPUTATION_GAIN,    true },
    { "reputation_paragon", TRIG_REPUTATION_PARAGON,    true },
    { "reputation_pregain", TRIG_REPUTATION_PREGAIN,    true },
    { "reputation_rankup",  TRIG_REPUTATION_RANKUP,     true },
    { "reset",		        TRIG_RESET,	true },
    { "rest",		        TRIG_REST,	true },
    { "restocked",		    TRIG_RESTOCKED,	true },
    { "restore",		    TRIG_RESTORE,	true },
    { "resurrect",		    TRIG_RESURRECT,	true },
    { "room_footer",		TRIG_ROOM_FOOTER,	true },
    { "room_header",		TRIG_ROOM_HEADER,	true },
    { "save",		        TRIG_SAVE,	true },
    { "sayto",		        TRIG_SAYTO,	true },
    { "shop_open",          TRIG_SHOP_OPEN, true},
    { "showcommands",		TRIG_SHOWCOMMANDS,	true },
    { "showexit",		    TRIG_SHOWEXIT,	true },
    { "sit",		        TRIG_SIT,	true },
    { "skill_berserk",		TRIG_SKILL_BERSERK,	true },
    { "skill_sneak",		TRIG_SKILL_SNEAK,	true },
    { "skill_warcry",		TRIG_SKILL_WARCRY,	true },
    { "sleep",		        TRIG_SLEEP,	true },
    { "speech",		        TRIG_SPEECH,	true },
    { "spell",		        TRIG_SPELL,	true },
    { "spellcast",		    TRIG_SPELLCAST,	true },
    { "spellpenetrate",		TRIG_SPELLPENETRATE,	true },
    { "spellreflect",		TRIG_SPELLREFLECT,	true },
    { "spell_cure",		    TRIG_SPELL_CURE,	true },
    { "spell_dispel",		TRIG_SPELL_DISPEL,	true },
    { "stand",		        TRIG_STAND,	true },
    { "start_combat",		TRIG_START_COMBAT,	true },
    { "stepoff",		    TRIG_STEPOFF,	true },
    { "stripaffect",		TRIG_STRIPAFFECT,	true },
    { "takeoff",		    TRIG_TAKEOFF,	true },
    { "throw",		        TRIG_THROW,	true },
    { "tick",		        TRIG_TICK,	true },
    { "timer_expire",       TRIG_TIMER_EXPIRE,  true },
    { "toggle_custom",		TRIG_TOGGLE_CUSTOM,	true },
    { "toggle_list",		TRIG_TOGGLE_LIST,	true },
    { "token_brandish",     TRIG_TOKEN_BRANDISH,    true },
	{ "token_brew",         TRIG_TOKEN_BREW,	true },
	{ "token_equip",        TRIG_TOKEN_EQUIP,	true },
	{ "token_given",        TRIG_TOKEN_GIVEN,	true },
    { "token_imbue",		TRIG_TOKEN_IMBUE,	true },
	{ "token_ink",      	TRIG_TOKEN_INK,	true },
    { "token_interrupt",    TRIG_TOKEN_INTERRUPT, true },
	{ "token_prebrew",  	TRIG_TOKEN_PREBREW,	true },
	{ "token_preimbue",   	TRIG_TOKEN_PREIMBUE,	true },
	{ "token_preink",   	TRIG_TOKEN_PREINK,	true },
	{ "token_prescribe",	TRIG_TOKEN_PRESCRIBE,	true },
	{ "token_presong",      TRIG_TOKEN_PRESONG,	true },
    { "token_pulse",        TRIG_TOKEN_PULSE,   true },
	{ "token_quaff",    	TRIG_TOKEN_QUAFF,	true },
	{ "token_recite",   	TRIG_TOKEN_RECITE,	true },
	{ "token_removed",  	TRIG_TOKEN_REMOVED,	true },
	{ "token_scribe",   	TRIG_TOKEN_SCRIBE,	true },
	{ "token_song",         TRIG_TOKEN_SONG,	true },
	{ "token_touch",    	TRIG_TOKEN_TOUCH,	true },
	{ "token_zap",      	TRIG_TOKEN_ZAP,	true },
    { "touch",		        TRIG_TOUCH,	true },
    { "toxingain",		    TRIG_TOXINGAIN,	true },
    { "turn",		        TRIG_TURN,	true },
    { "turn_on",		    TRIG_TURN_ON,	true },
    { "ungrouped",		    TRIG_UNGROUPED,	true },
    { "unhitch",            TRIG_UNHITCH,   true },
    { "unlead",		        TRIG_UNLEAD,	true },
    { "unlock",		        TRIG_UNLOCK,	true },
    { "unyoke",             TRIG_UNYOKE,   true },
    { "use",		        TRIG_USE,	true },
    { "usewith",		    TRIG_USEWITH,	true },
    { "verb",		        TRIG_VERB,	true },
    { "verbself",		    TRIG_VERBSELF,	true },
    { "wake",		        TRIG_WAKE,	true },
    { "weapon_blocked",		TRIG_WEAPON_BLOCKED,	true },
    { "weapon_caught",		TRIG_WEAPON_CAUGHT,	true },
    { "weapon_parried",		TRIG_WEAPON_PARRIED,	true },
    { "wear",		        TRIG_WEAR,	true },
    { "whisper",		    TRIG_WHISPER,	true },
    { "wimpy",		        TRIG_WIMPY,	true },
    { "xpbonus",		    TRIG_XPBONUS,	true },
    { "xpcompute",		    TRIG_XPCOMPUTE,	true },
    { "xpgain",		        TRIG_XPGAIN,	true },
    { "yoke",               TRIG_YOKE,   true },
    { "zap",		        TRIG_ZAP,	true },
    { NULL,                 -1,         false }
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

const struct gln_type gln_table[] =
{
    { "acid", &gln_acid, &liquid_acid },
    { "blood", &gln_blood, &liquid_blood },
    { "potion", &gln_potion, &liquid_potion },
    { "water", &gln_water, &liquid_water },
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
    { "silence",        prescribe_silence },
    { NULL, NULL }
};

const struct scribe_func_type scribe_func_table[] =
{
    { NULL, NULL }
};

const struct recite_func_type recite_func_table[] =
{
    { "identify",       recite_identify },
    { "silence",        recite_silence },
    { "word_of_recall", recite_word_of_recall },
    { NULL, NULL }
};

const struct preink_func_type preink_func_table[] =
{
    { "silence",        preink_silence },
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
    { "silence",            touch_silence },
    { NULL,                 NULL }
};

const struct preimbue_func_type preimbue_func_table[] =
{
    { "silence",            preimbue_silence },
    { NULL, NULL }
};

const struct imbue_func_type imbue_func_table[] =
{
    { NULL, NULL }
};

const struct zap_func_type zap_func_table[] =
{
    { "chain_lightning",    zap_chain_lightning },
    { "silence",            zap_silence },
    { NULL,                 NULL }
};

const struct brandish_func_type brandish_func_table[] =
{
    { "silence",            brandish_silence },
    { NULL,                 NULL }
};

const struct equip_func_type equip_func_table[] =
{
    { "armour",             equip_armour },
    { "haste",              equip_haste },
    { "invis",              equip_invis },
    { "silence",            equip_silence },
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
	{ "imbue",	            &gsn_imbue,	            &gsk_imbue },
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

const struct song_func_type presong_func_table[] =
{
    { NULL,                 NULL }
};

const struct song_func_type song_func_table[] =
{
	{ "another_gate",   	song_another_gate },
	{ "aquatic_polka",  	song_aquatic_polka },
	{ "awareness_jig",  	song_awareness_jig },
    { "blessed_be",     	song_blessed_be },
	{ "curse_of_the_abyss",	song_curse_of_the_abyss },
	{ "dark_cloud",     	song_dark_cloud },
    { "debugging",          song_debugging },               // Strictly for getting this all working
	{ "dwarven_tale",   	song_dwarven_tale },
	{ "fade_to_black",  	song_fade_to_black },
	{ "fat_owl_hopping",	song_fat_owl_hopping },
	{ "firefly_tune",   	song_firefly_tune },
	{ "fireworks",      	song_fireworks },
	{ "pretty_in_pink", 	song_pretty_in_pink },
	{ "purple_mist",    	song_purple_mist },
	{ "rigor",          	song_rigor },
	{ "stormy_weather", 	song_stormy_weather },
	{ "swamp_song",     	song_swamp_song },
    { NULL,                 NULL }
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
    { "no_haggle",          PRACTICE_ENTRY_NO_HAGGLE,       true  },
    { NULL,                 0,                              false }
};

const struct flag_type material_flags[] =
{
    { NULL,                 0,                              false }
};

const struct gm_type gm_table[] =
{
    { "iron", &gm_iron },
    { "silver", &gm_silver },
    { NULL, NULL }
};


const struct gcl_type gcl_table[] =
{
    { "adept",          &gcl_adept },
    { "adventurer",     &gcl_adventurer },
    { "alchemist",      &gcl_alchemist },
    { "archaeologist",  &gcl_archaeologist },
    { "archmage",       &gcl_archmage },
    { "armorer",        &gcl_armorer },
    { "assassin",       &gcl_assassin },
    { "bard",           &gcl_bard },
    { "blacksmith",     &gcl_blacksmith },
    { "botanist",       &gcl_botanist },
    { "carpenter",      &gcl_carpenter },
    { "crusader",       &gcl_crusader },
    { "culinarian",     &gcl_culinarian },
    { "destroyer",      &gcl_destroyer },
    { "druid",          &gcl_druid },
    { "enchanter",      &gcl_enchanter },
    { "engineer",       &gcl_engineer },
    { "fisher",         &gcl_fisher },
    { "geomancer",      &gcl_geomancer },
    { "gladiator",      &gcl_gladiator },
    { "highwayman",     &gcl_highwayman },
    { "illusionist",    &gcl_illusionist },
    { "inscription",    &gcl_inscription },
    { "jewelcrafter",   &gcl_jewelcrafter },
    { "leatherworker",  &gcl_leatherworker },
    { "marauder",       &gcl_marauder },
    { "mariner",        &gcl_mariner },
    { "miner",          &gcl_miner },
    { "monk",           &gcl_monk },
    { "necromancer",    &gcl_necromancer },
    { "ninja",          &gcl_ninja },
    { "paladin",        &gcl_paladin },
    { "priest",         &gcl_priest },
    { "ranger",         &gcl_ranger },
    { "rogue",          &gcl_rogue },
    { "sage",           &gcl_sage },
    { "shaman",         &gcl_shaman },
    { "skinner",        &gcl_skinner },
    { "sorcerer",       &gcl_sorcerer },
    { "stonemason",     &gcl_stonemason },
    { "warlord",        &gcl_warlord },
    { "weaver",         &gcl_weaver },
    { "witch",          &gcl_witch },
    { "wizard",         &gcl_wizard },
    { NULL, NULL }
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

const struct gr_type gr_table[] =
{
    { "angel", &gr_angel },
    { "avatar", &gr_avatar },
    { "berserker", &gr_berserker },
    { "changeling", &gr_changeling },
    { "colossus", &gr_colossus },
    { "demon", &gr_demon },
    { "draconian", &gr_draconian },
    { "dragon", &gr_dragon },
    { "drow", &gr_drow },
    { "dwarf", &gr_dwarf },
    { "elf", &gr_elf },
    { "fiend", &gr_fiend },
    { "hell baron", &gr_hell_baron },
    { "human", &gr_human },
    { "lich", &gr_lich },
    { "minotaur", &gr_minotaur },
    { "mystic", &gr_mystic },
    { "naga", &gr_naga },
    { "seraph", &gr_seraph },
    { "shaper", &gr_shaper },
    { "sith", &gr_sith },
    { "slayer", &gr_slayer },
    { "specter", &gr_specter },
    { "titan", &gr_titan },
    { "vampire", &gr_vampire },
    { "wraith", &gr_wraith },
    { NULL, NULL }
};

const struct class_leave_type class_leave_table[] =
{
    {NULL, NULL}
};

const struct class_enter_type class_enter_table[] =
{
    {NULL, NULL}
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
    {"player",      STAFF_PLAYER,           true},
    {"gimp",        STAFF_GIMP,             true},
    {"immortal",    STAFF_IMMORTAL,         true},
    {"ascendant",   STAFF_ASCENDANT,        true},
    {"supremacy",   STAFF_SUPREMACY,        true},
    {"creator",     STAFF_CREATOR,          true},
    {"implementor", STAFF_IMPLEMENTOR,      true},

    {NULL,          0,                      false}
};

const struct flag_type command_types[] =
{
	{ "none",		CMDTYPE_NONE,	    true },
	{ "move",		CMDTYPE_MOVE,	    true },
	{ "combat",		CMDTYPE_COMBAT,	    true },
	{ "object",		CMDTYPE_OBJECT,	    true },
	{ "info",		CMDTYPE_INFO,	    true },
	{ "comm",		CMDTYPE_COMM,	    true },
	{ "racial",		CMDTYPE_RACIAL,	    true },
	{ "ooc",		CMDTYPE_OOC,	    true },
	{ "immortal",	CMDTYPE_IMMORTAL,	true },
	{ "olc",		CMDTYPE_OLC,	    true },
	{ "admin",		CMDTYPE_ADMIN,	    true },
    {NULL,          0,                  false}

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

const struct global_sector_type global_sector_table[] =
{
    {"city",                &gsct_city },
    {"inside",              &gsct_inside },
    {"underwater_noswim",   &gsct_underwater_noswim },
    {"underwater_swim",     &gsct_underwater_swim },
    {"water_noswim",        &gsct_water_noswim },
    {"water_swim",          &gsct_water_swim },
    {NULL,                  NULL }
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
