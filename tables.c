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


const struct sex_type sex_table[] =
{
   {	"none"		},
   {	"male"		},
   {	"female"	},
   {	"either"	},
   {	NULL		}
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
    {	"practice",				ACT_PRACTICE,			true	},
    {	"blacksmith",			ACT_BLACKSMITH,			true	},
    {	"crew_seller",			ACT_CREW_SELLER,		true	},
    {   "no_lore",				ACT_NO_LORE,			true	},
    {	"undead",				ACT_UNDEAD,				true	},
// (P)
    {	"cleric",				ACT_CLERIC,				true	},
    {	"mage",					ACT_MAGE,				true	},
    {	"thief",				ACT_THIEF,				true	},
    {	"warrior",				ACT_WARRIOR,			true	},
    {	"animated",				ACT_ANIMATED,			false	},
    {	"nopurge",				ACT_NOPURGE,			true	},
    {	"outdoors",				ACT_OUTDOORS,			true	},
    {   "restringer",			ACT_IS_RESTRINGER,		true	},
    {	"indoors",				ACT_INDOORS,			true	},
    {	"questor",				ACT_QUESTOR,			true	},
    {	"healer",				ACT_IS_HEALER,			true	},
    {	"stay_locale",			ACT_STAY_LOCALE,		true	},
    {	"update_always",		ACT_UPDATE_ALWAYS,		true	},
    {	"changer",				ACT_IS_CHANGER,			true	},
    {   "banker",				ACT_IS_BANKER,			true    },
    {	NULL,					0,	false	}
};


const struct flag_type act2_flags[]=
{
    {   "churchmaster",			ACT2_CHURCHMASTER,		true	},
    {   "noquest",				ACT2_NOQUEST,			true    },
    {   "plane_tunneler",		ACT2_PLANE_TUNNELER,	true    },
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
    {   "advanced_trainer",     ACT2_ADVANCED_TRAINER,  true    },
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
    {   "Normal"  	},
    {   "Solid"    	},
    {   "Weak"  	},
    {   "Strong" 	}
};


const struct flag_type armour_strength_table[] =
{
    {	"None",		OBJ_ARMOUR_NOSTRENGTH,	true   	},
    {   "Light",	OBJ_ARMOUR_LIGHT,   true  	},
    {   "Medium",	OBJ_ARMOUR_MEDIUM,   true    	},
    {   "Strong",	OBJ_ARMOUR_STRONG,   true  	},
    {   "Heavy",	OBJ_ARMOUR_HEAVY,   true 	},
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
    {   "compass",      PLR_COMPASS,    false   },
    {   "autocatalyst", PLR_AUTOCAT,    false   },
    {   "staff",        PLR_STAFF,          false   },
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
    {   "silence",		A,     	true    },
    {   "evasion",		B,	true	},
    {   "cloak_guile",  	C,	true	},
    {   "warcry",		D,	true    },
    {   "light_shroud", 	E,	true	},
    {   "healing_aura", 	F,	true	},
    {   "energy_field", 	G,	true	},
    {   "spell_shield", 	H,	true	},
    {   "spell_deflection", 	I,  	true    },
    {   "avatar_shield",	J,	true    },
    {   "fatigue",		K,	true	},
    {   "paralysis",		L,	true	},
    {   "neurotoxin",		M,	true	},
    {	"toxin",		N,	true	},
    {   "electrical_barrier",	O,	true	},
    {	"fire_barrier",		P,	true	},
    {	"frost_barrier",	Q,	true	},
    {	"improved_invis",	R,	true	},
    {	"ensnare",		S,	true	},
    {   "see_cloak",		T,	true    },
    {	"stone_skin",		U,	true	},
    {	"morphlock",		AFF2_MORPHLOCK,		true	},
    {	"deathsight",		AFF2_DEATHSIGHT,	true	},
    {	"immobile",		AFF2_IMMOBILE,	true	},
    {	"protected",		AFF2_PROTECTED,	true	},
    {   NULL,           	0,      0       }
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
    {	NULL,			0,	0	}
};


const struct flag_type form_flags[] =
{
    {	"edible",		FORM_EDIBLE,		true	},
    {	"poison",		FORM_POISON,		true	},
    {	"magical",		FORM_MAGICAL,		true	},
    {	"instant_decay",	FORM_INSTANT_DECAY,	true	},
    {	"other",		FORM_OTHER,		true	},
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
    {	NULL,		0,		0	},
};



const struct flag_type area_who_titles[] = {
    {	"blank",	AREA_BLANK,		true	},
    {	"abyss",	AREA_ABYSS,		true	},
    {	"aerial",	AREA_AERIAL,		true	},
    {	"arena",	AREA_ARENA,		true	},
    {	"at_sea",	AREA_AT_SEA,		true	},
    {	"battle",	AREA_BATTLE,		true	},
    {	"castle",	AREA_CASTLE,		true	},
    {	"cavern",	AREA_CAVERN,		true	},
    {	"chat",		AREA_CHAT,		true	},
    {	"church",	AREA_CHURCH,		true	},
    {	"cult",		AREA_CULT,		true	},
    {	"dungeon",	AREA_DUNGEON,		true	},
    {	"eden",		AREA_EDEN,		true	},
    {	"forest",	AREA_FOREST,		true	},
    {	"fort",		AREA_FORT,		true	},
    {	"home",		AREA_HOME,		true	},
    {	"immortal",	AREA_OFFICE,		true	},
    {	"inn",		AREA_INN,		true	},
    {	"isle",		AREA_ISLE,		true	},
    {	"jungle",	AREA_JUNGLE,		true	},
    {	"keep",		AREA_KEEP,		true	},
    {	"limbo",	AREA_LIMBO,		true	},
    {	"mountain",	AREA_MOUNTAIN,		true	},
    {	"netherworld",	AREA_NETHERWORLD,	true	},
    {	"on_ship",	AREA_ON_SHIP,		true	},
    {	"outpost",	AREA_OUTPOST,		true	},
    {	"palace",	AREA_PALACE,		true	},
    {	"pg",		AREA_PG,		true	},
    {	"planar",	AREA_PLANAR,		true	},
    {	"pyramid",	AREA_PYRAMID,		true	},
    {	"ruins",	AREA_RUINS,		true	},
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
    {	"instance",		AREA_INSTANCE,		true	},
    {	"duty",	AREA_DUTY,			true	},
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
    {   "social",       AREA_SOCIAL,        true    },
    {   "housing",      AREA_HOUSING,       true    },
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
//    {	"locked",		EX_LOCKED,		true	},
//    {	"pickproof",		EX_PICKPROOF,		true	},
    {   "nopass",		EX_NOPASS,		true	},
//    {   "easy",			EX_EASY,		true	},
//    {   "hard",			EX_HARD,		true	},
//    {	"infuriating",		EX_INFURIATING,		true	},
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
    {	"environment",		EX_ENVIRONMENT,		true	},
    {	"nounlink",		EX_NOUNLINK,		true	},
    {	"prevfloor",		EX_PREVFLOOR,		false	},
    {	"nextfloor",		EX_NEXTFLOOR,		false	},
    {	"nosearch",			EX_NOSEARCH,		true	},
    {	"mustsee",			EX_MUSTSEE,			true	},
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
    {	"created",		LOCK_CREATED,		false	},
    {	NULL,			0,					0		}
};

const struct flag_type portal_exit_flags[] =
{
    {   "door",			EX_ISDOOR,		true    },
    {	"closed",		EX_CLOSED,		true	},
//    {	"locked",		EX_LOCKED,		true	},
//    {	"pickproof",		EX_PICKPROOF,		true	},
    {   "nopass",		EX_NOPASS,		true	},
//    {   "easy",			EX_EASY,		true	},
//    {   "hard",			EX_HARD,		true	},
//    {	"infuriating",		EX_INFURIATING,		true	},
    {	"noclose",		EX_NOCLOSE,		true	},
    {	"nolock",		EX_NOLOCK,		true	},
//    {   "hidden",		EX_HIDDEN,		true	},			// Portals can be made hidden in other ways
    {   "found",		EX_FOUND,		true	},
    {   "broken",		EX_BROKEN,		true	},
    {   "nobash",		EX_NOBASH,		true    },
    {   "walkthrough",		EX_WALKTHROUGH,		true    },
    {   "nobar",		EX_NOBAR,		true    },
    {   "aerial",		EX_AERIAL,		true	},
    {   "nohunt",		EX_NOHUNT,		true	},
    {	"environment",		EX_ENVIRONMENT,		true	},
    {	"prevfloor",		EX_PREVFLOOR,		true	},
    {	"nextfloor",		EX_NEXTFLOOR,		true	},
    {	"nosearch",			EX_NOSEARCH,		true	},
    {	"mustsee",			EX_MUSTSEE,			true	},
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
    {	"cpk",				ROOM_CPK,				true	},
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
    {   "vault",                ROOM_VAULT,             true    },
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
    {	"staff",		ITEM_STAFF,		true	},
    {	"weapon",		ITEM_WEAPON,		true	},
    {	"treasure",		ITEM_TREASURE,		true	},
    {	"armour",		ITEM_ARMOUR,		true	},
    {	"potion",		ITEM_POTION,		true	},
    {	"furniture",		ITEM_FURNITURE,		true	},
    {	"trash",		ITEM_TRASH,		true	},
    {	"container",		ITEM_CONTAINER,		true	},
    {	"drinkcontainer",	ITEM_DRINK_CON,		true	},
    {	"key",			ITEM_KEY,		true	},
    {	"food",			ITEM_FOOD,		true	},
    {	"money",		ITEM_MONEY,		true	},
    {	"boat",			ITEM_BOAT,		true	},
    {	"npccorpse",		ITEM_CORPSE_NPC,	true	},
    {	"pc corpse",		ITEM_CORPSE_PC,		false	},
    {	"fountain",		ITEM_FOUNTAIN,		true	},
    {	"pill",			ITEM_PILL,		true	},
    {	"protect",		ITEM_PROTECT,		true	},
    {	"map",			ITEM_MAP,		true	},
    {   "portal",		ITEM_PORTAL,		true	},
    {   "catalyst",		ITEM_CATALYST,		true	},
    {	"roomkey",		ITEM_ROOM_KEY,		true	},
    { 	"gem",			ITEM_GEM,		true	},
    {	"jewelry",		ITEM_JEWELRY,		true	},
    {	"jukebox",		ITEM_JUKEBOX,		true	},
    {	"artifact",		ITEM_ARTIFACT,		true	},
    {   "shares",		ITEM_SHARECERT,		true	},
    {   "flame_room_object",	ITEM_ROOM_FLAME,	true	},
    {   "instrument",		ITEM_INSTRUMENT,	true	},
    {   "seed",			ITEM_SEED,		true	},
    {   "cart",			ITEM_CART,		true	},
    {   "ship",			ITEM_SHIP,		true	},
    {   "room_darkness_object",	ITEM_ROOM_DARKNESS,	true	},
    {   "ranged_weapon",	ITEM_RANGED_WEAPON,	true	},
    {   "sextant",		ITEM_SEXTANT,		true	},
    {   "weapon_container",	ITEM_WEAPON_CONTAINER,	true	},
    {   "room_roomshield_object",	ITEM_ROOM_ROOMSHIELD,	true	},
    {	"book",			ITEM_BOOK,		true	},
    {   "stinking_cloud",	ITEM_STINKING_CLOUD,	true	},
    {   "smoke_bomb",       	ITEM_SMOKE_BOMB,	true	},
    {   "herb",			ITEM_HERB,		true	},
    /*
    {   "Herb (Hylaxis)",       ITEM_HERB_1,		true	},
    {   "Herb (Rhotail)",       ITEM_HERB_2,		true	},
    {   "Herb (Viagrol)",       ITEM_HERB_3,		true	},
    {   "Herb (Guamal)",        ITEM_HERB_4,		true	},
    {   "Herb (Satrix)",        ITEM_HERB_5,		true	},
    {   "Herb (Falsz)",         ITEM_HERB_6,		true	},
    */
    {   "spell_trap",       	ITEM_SPELL_TRAP,	true	},
    {   "withering_cloud",  	ITEM_WITHERING_CLOUD,	true	},
    {   "bank",			ITEM_BANK,		true 	},
    {   "keyring",		ITEM_KEYRING,		true 	},
    {	"ice_storm",		ITEM_ICE_STORM,		true	},
    {	"flower",		ITEM_FLOWER,		true	},
    {   "trade_type",		ITEM_TRADE_TYPE,	true	},
    {   "empty_vial",	        ITEM_EMPTY_VIAL,	true	},
    {   "blank_scroll",		ITEM_BLANK_SCROLL,	true    },
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
    {	"glow",			ITEM_GLOW,		true	},
    {	"hum",			ITEM_HUM,		true	},
    {   "no_restring",		ITEM_NORESTRING,	true	},
    {   "nokeyring",		ITEM_NOKEYRING,		true    },
    {	"evil",			ITEM_EVIL,		true	},
    {	"invis",		ITEM_INVIS,		true	},
    {	"magic",		ITEM_MAGIC,		true	},
    {	"nodrop",		ITEM_NODROP,		true	},
    {	"bless",		ITEM_BLESS,		true	},
    {	"antigood",		ITEM_ANTI_GOOD,		true	},
    {	"antievil",		ITEM_ANTI_EVIL,		true	},
    {	"antineutral",		ITEM_ANTI_NEUTRAL,	true	},
    {	"noremove",		ITEM_NOREMOVE,		true	},
    {	"inventory",		ITEM_INVENTORY,		false   },
    {	"nopurge",		ITEM_NOPURGE,		true	},
    {	"rotdeath",		ITEM_ROT_DEATH,		true	},
    {	"nosteal",		ITEM_NOSTEAL,		true	},
    {   "hidden",		ITEM_HIDDEN,		false   },
    {   "nolocate",		ITEM_NOLOCATE,		true	},
    {	"meltdrop",		ITEM_MELT_DROP,		true	},
    //{	"hadtimer",		ITEM_HAD_TIMER,		true	},
    //{	"sellextract",		ITEM_SELL_EXTRACT,	true	},
    {	"burnproof",		ITEM_BURN_PROOF,	true	},
    {	"freezeproof",		ITEM_FREEZE_PROOF,	true	},
    {	"nouncurse",		ITEM_NOUNCURSE,		true	},
    {	"headless",		ITEM_NOSKULL,		false   },
    {	"planted",		ITEM_PLANTED,		false   },
    {	"permanent",		ITEM_PERMANENT,		false   },
    {	NULL,			0,			0	}
};


const struct flag_type extra2_flags[] =
{
    {   "all_remort",		ITEM_ALL_REMORT,	true    },
    {   "locker",		ITEM_LOCKER,		true    },
    {   "true_sight",		ITEM_TRUESIGHT,		true	},
    {   "scare",		ITEM_SCARE,		true	},
    {   "sustains",		ITEM_SUSTAIN,		true	},
    {   "enchanted",		ITEM_ENCHANTED,		false   },
    {   "emits_light",		ITEM_EMITS_LIGHT,	true    },
    {   "float_user",		ITEM_FLOAT_USER,	true    },
    {   "see_hidden",		ITEM_SEE_HIDDEN,	true    },
    {   "weed",			ITEM_WEED,		false   },
    {   "super-strong",		ITEM_SUPER_STRONG,	true    },
    {   "remort_only",		ITEM_REMORT_ONLY,	true    },
    {   "no_lore",		ITEM_NO_LORE,		true    },
    {   "sell_once",		ITEM_SELL_ONCE,		true    },
    {   "no_hunt",		ITEM_NO_HUNT,		true    },
    {   "no_resurrect",		ITEM_NO_RESURRECT,	false   },
    {   "no_discharge",		ITEM_NO_DISCHARGE,	true	},
    {   "no_donate",		ITEM_NO_DONATE,		true    },
    {   "kept",			ITEM_KEPT,		true	},
    {   "singular",		ITEM_SINGULAR,		true	},
    {	"no_loot",		ITEM_NO_LOOT,		true	},
    {	"no_enchant",		ITEM_NO_ENCHANT,	true	},
    {   "no_container",		ITEM_NO_CONTAINER,	true	},
    {   "third_eye",		ITEM_THIRD_EYE,		false   },
    {   "buried",		ITEM_BURIED,		false   },
    {   "no_locker",		ITEM_NOLOCKER,		true	},
    {	"no_auction",		ITEM_NOAUCTION,		true	},
    {	"keep_value",		ITEM_KEEP_VALUE,	true	},
    {   NULL,			0,			0	}
};

const struct flag_type extra3_flags[] =
{
    {	"exclude_list",		ITEM_EXCLUDE_LIST,	true	},
    {	"no_transfer",		ITEM_NO_TRANSFER,	true	},
    {	"always_loot",		ITEM_ALWAYS_LOOT,	true	},
    {	"force_loot",		ITEM_FORCE_LOOT,	false	},
    {	"can_dispel",		ITEM_CAN_DISPEL,	true	},
    {	"keep_equipped",	ITEM_KEEP_EQUIPPED,	true	},
    {   "no_animate",		ITEM_NO_ANIMATE,	false   },
    {	"rift_update",		ITEM_RIFT_UPDATE,	true	},
    {	"show_in_wilds",	ITEM_SHOW_IN_WILDS,	true	},
    {   "activated",        ITEM_ACTIVATED,     false   },
    {	"instance_obj",		ITEM_INSTANCE_OBJ,	false	},
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
    {	"take",			ITEM_TAKE,		true	},
    {	"finger",		ITEM_WEAR_FINGER,	true	},
    {	"neck",			ITEM_WEAR_NECK,		true	},
    {	"body",			ITEM_WEAR_BODY,		true	},
    {	"head",			ITEM_WEAR_HEAD,		true	},
    {	"legs",			ITEM_WEAR_LEGS,		true	},
    {	"feet",			ITEM_WEAR_FEET,		true	},
    {	"hands",		ITEM_WEAR_HANDS,	true	},
    {	"arms",			ITEM_WEAR_ARMS,		true	},
    {	"shield",		ITEM_WEAR_SHIELD,	true	},
    {	"about",		ITEM_WEAR_ABOUT,	true	},
    {	"waist",		ITEM_WEAR_WAIST,	true	},
    {	"wrist",		ITEM_WEAR_WRIST,	true	},
    {	"wield",		ITEM_WIELD,		true	},
    {	"hold",			ITEM_HOLD,		true	},
    {   "nosac",		ITEM_NO_SAC,		true	},
    {	"wearfloat",		ITEM_WEAR_FLOAT,	true	},
    {   "ring_finger",		ITEM_WEAR_RING_FINGER,  true    },
    {   "back",			ITEM_WEAR_BACK,		true    },
    {   "shoulder",		ITEM_WEAR_SHOULDER,	true	},
    {   "face",			ITEM_WEAR_FACE,		true	},
    {   "eyes",			ITEM_WEAR_EYES,		true	},
    {   "ear",			ITEM_WEAR_EAR,		true	},
    {   "ankle",		ITEM_WEAR_ANKLE,	true	},
    {   "conceals",		ITEM_CONCEALS,		true	},
    {	"tabard",		ITEM_WEAR_TABARD,	true	},
    {	NULL,			0,			0	}
};


/*
 * Used when adding an affect to tell where it goes.
 * See addaffect and delaffect in act_olc.c
 */
const struct flag_type apply_flags[] =
{
    {	"none",			APPLY_NONE,		true	},
    {	"strength",		APPLY_STR,		true	},
    {	"dexterity",		APPLY_DEX,		true	},
    {	"intelligence",		APPLY_INT,		true	},
    {	"wisdom",		APPLY_WIS,		true	},
    {	"constitution",		APPLY_CON,		true	},
    {	"sex",			APPLY_SEX,		true	},
    {	"mana",			APPLY_MANA,		true	},
    {	"hp",			APPLY_HIT,		true	},
    {	"move",			APPLY_MOVE,		true	},
    {	"gold",			APPLY_GOLD,		true	},
    {	"ac",			APPLY_AC,		true	},
    {	"hitroll",		APPLY_HITROLL,		true	},
    {	"damroll",		APPLY_DAMROLL,		true	},
    {	"skill",		APPLY_SKILL,		false	},
    {	"spellaffect",		APPLY_SPELL_AFFECT,	false	},
    {	NULL,			0,			0	}
};

const struct flag_type apply_flags_full[] =
{
    {	"none",			APPLY_NONE,		true	},
    {	"strength",		APPLY_STR,		true	},
    {	"dexterity",		APPLY_DEX,		true	},
    {	"intelligence",		APPLY_INT,		true	},
    {	"wisdom",		APPLY_WIS,		true	},
    {	"constitution",		APPLY_CON,		true	},
    {	"sex",			APPLY_SEX,		true	},
    {	"mana",			APPLY_MANA,		true	},
    {	"hp",			APPLY_HIT,		true	},
    {	"move",			APPLY_MOVE,		true	},
    {	"gold",			APPLY_GOLD,		true	},
    {	"ac",			APPLY_AC,		true	},
    {	"hitroll",		APPLY_HITROLL,		true	},
    {	"damroll",		APPLY_DAMROLL,		true	},
    {	"skill",		APPLY_SKILL,		true	},
    {	"spellaffect",		APPLY_SPELL_AFFECT,	true	},
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
    {	NULL,		0,		0	}
};


const struct flag_type container_flags[] =
{
    {	"closeable",		CONT_CLOSEABLE,	true	},
//    {	"pickproof",		CONT_PICKPROOF,	true	},
    {	"closed",		CONT_CLOSED,	true	},
//    {	"locked",		CONT_LOCKED,	true	},
    {	"puton",		CONT_PUT_ON,	true	},
//    {	"snapkey",		CONT_SNAPKEY,	true	},	// @@@NIB : 20070126
    {	"pushopen",		CONT_PUSHOPEN,	true	},	// @@@NIB : 20070126
    {	"closelock",		CONT_CLOSELOCK,	true	},	// @@@NIB : 20070126
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
    {   "exotic",        RANGED_WEAPON_EXOTIC,  true    },
    {   "crossbow",      RANGED_WEAPON_CROSSBOW,true    },
    {   "bow",           RANGED_WEAPON_BOW,     true    },
    {   "blowgun",	RANGED_WEAPON_BLOWGUN,	true    },	// @@@NIB : 20070126 : darts!
    {   "harpoon",	RANGED_WEAPON_HARPOON,	true    },	// @@@NIB : 20070126 : harpoons!
    {   NULL,          	 0,                     true    }
};


const struct flag_type weapon_class[] =
{
    {   "exotic",	WEAPON_EXOTIC,		true    },
    {   "sword",	WEAPON_SWORD,		true    },
    {   "dagger",	WEAPON_DAGGER,		true    },
    {   "spear",	WEAPON_SPEAR,		true    },
    {   "mace",		WEAPON_MACE,		true    },
    {   "axe",		WEAPON_AXE,		true    },
    {   "flail",	WEAPON_FLAIL,		true    },
    {   "whip",		WEAPON_WHIP,		true    },
    {   "polearm",	WEAPON_POLEARM,		true    },
    {	"stake",	WEAPON_STAKE,		true	},
    {	"quarterstaff",	WEAPON_QUARTERSTAFF,	true	},
    {	"arrow",	WEAPON_ARROW,		true	},
    {	"bolt",		WEAPON_BOLT,		true	},
    {	"dart",		WEAPON_DART,		true	},
    {	"harpoon",	WEAPON_HARPOON,		true	},
    {	"throwable",	WEAPON_THROWABLE,	false	},
    {   NULL,		0,			0       }
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
    {   "dungeon",			GATE_DUNGEON,	true	},
    {   "dungeonrandom",	GATE_DUNGEONRANDOM,	true	},
    {   "instancerandom",	GATE_INSTANCERANDOM,	true	},
    {   "sectionrandom",	GATE_SECTIONRANDOM,	true	},
    {   NULL,		0,			0	}
};


const struct flag_type furniture_flags[]=
{
    {   "stand_at",	STAND_AT,	true	},
    {	"stand_on",	STAND_ON,	true	},
    {	"stand_in",	STAND_IN,	true	},
    {	"sit_at",	SIT_AT,		true	},
    {	"sit_on",	SIT_ON,		true	},
    {	"sit_in",	SIT_IN,		true	},
    {	"rest_at",	REST_AT,	true	},
    {	"rest_on",	REST_ON,	true	},
    {	"rest_in",	REST_IN,	true	},
    {	"sleep_at",	SLEEP_AT,	true	},
    {	"sleep_on",	SLEEP_ON,	true	},
    {	"sleep_in",	SLEEP_IN,	true	},
    {	"put_at",	PUT_AT,		true	},
    {	"put_on",	PUT_ON,		true	},
    {	"put_in",	PUT_IN,		true	},
    {	"put_inside",	PUT_INSIDE,	true	},
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
    {	"purge_death",		TOKEN_PURGE_DEATH,		true	},
    {	"purge_idle",		TOKEN_PURGE_IDLE,		true	},
    {	"purge_quit",		TOKEN_PURGE_QUIT,		true	},
    {	"purge_reboot",		TOKEN_PURGE_REBOOT,		true	},
    {	"purge_rift",		TOKEN_PURGE_RIFT,		true	},
    {	"reverse_timer",	TOKEN_REVERSETIMER,		true	},
    {	"no_skill_test",	TOKEN_NOSKILLTEST,		true	},
    {	"singular",			TOKEN_SINGULAR,			true	},
    {	"see_all",			TOKEN_SEE_ALL,			true	},
    {	"permanent",		TOKEN_PERMANENT,		true	},
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
    {NULL, 0, 0}
};


// @@@NIB : 20070120 : Added for relic ifchecks
// Labels the reserved vnum of each relic to their function
// This includes both the boost name and relic name for interchangability
//	within scripts, since scripters dealing with relics ought to be able
//	to use whichever name they wish for identifying the relic.
const struct flag_type relic_types[] = {
    // Boost name
    {"damage",OBJ_VNUM_RELIC_EXTRA_DAMAGE,true},
    {"xp",OBJ_VNUM_RELIC_EXTRA_XP,true},
    {"pneuma",OBJ_VNUM_RELIC_EXTRA_PNEUMA,true},
    {"hp",OBJ_VNUM_RELIC_HP_REGEN,true},
    {"mana",OBJ_VNUM_RELIC_MANA_REGEN,true},
    // Relic name
    {"power",OBJ_VNUM_RELIC_EXTRA_DAMAGE,true},
    {"knowledge",OBJ_VNUM_RELIC_EXTRA_XP,true},
    {"soul",OBJ_VNUM_RELIC_EXTRA_PNEUMA,true},
    {"health",OBJ_VNUM_RELIC_HP_REGEN,true},
    {"magic",OBJ_VNUM_RELIC_MANA_REGEN,true},
    {NULL, 0, 0},
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
    "watery"
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

    {	NULL,		0,		0	}
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
    { "vocal",		INSTRUMENT_VOCAL,		false },
    { "any",		INSTRUMENT_ANY,			true },
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
    { NULL,			0,						false }
};

const struct flag_type corpse_object_flags[] = {
    { "cpk",		CORPSE_CPKDEATH,		true },
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
    {"skill",			VAR_SKILL,				true},
    {"skillinfo",		VAR_SKILLINFO,			true},
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
    {"bllist_wilds",	VAR_BLLIST_WILDS,		true},
    {"pllist_str",		VAR_PLLIST_STR,			true},
    {"pllist_conn",		VAR_PLLIST_CONN,		true},
    {"pllist_room",		VAR_PLLIST_ROOM,		true},
    {"pllist_mob",		VAR_PLLIST_MOB,			true},
    {"pllist_obj",		VAR_PLLIST_OBJ,			true},
    {"pllist_tok",		VAR_PLLIST_TOK,			true},
    {"pllist_church",	VAR_PLLIST_CHURCH,		true},
    {"pllist_variable",	VAR_PLLIST_VARIABLE,	true},
    { NULL,				VAR_UNKNOWN,			false }

};


const struct flag_type skill_flags[] = {
    {"practice",		SKILL_PRACTICE,			true},
    {"improve",			SKILL_IMPROVE,			true},
    {"favourite",		SKILL_FAVOURITE,			false},	// This is set manually
    { NULL,				0,			false }
};


const struct flag_type shop_flags[] =
{
    { "stock_only",		SHOPFLAG_STOCK_ONLY,	true	},
    { "hide_shop",		SHOPFLAG_HIDE_SHOP,		true	},
    { "no_haggle",		SHOPFLAG_NO_HAGGLE,		true	},
    { NULL,				0,						false	}
};

const struct flag_type blueprint_section_flags[] =
{
    { "no_rotate",		BSFLAG_NO_ROTATE,		true	},
    { NULL,				0,						false	}
};

const struct flag_type blueprint_section_types[] =
{
    { "static",			BSTYPE_STATIC,			true	},
    { NULL,				0,						false	}
};

const struct flag_type instance_flags[] =
{
    { "completed",			INSTANCE_COMPLETED,			false	},
    { "destroy",			INSTANCE_DESTROY,			false	},
    { "idle_on_complete",	INSTANCE_IDLE_ON_COMPLETE,	true	},
    { "no_idle",			INSTANCE_NO_IDLE,			true	},
    { "no_save",			INSTANCE_NO_SAVE,			true	},
    { NULL,					0,							false	}
};

const struct flag_type dungeon_flags[] =
{
    { "completed",			DUNGEON_COMPLETED,			false	},
    { "destroy",			DUNGEON_DESTROY,			false	},
    { "idle_on_complete",	DUNGEON_IDLE_ON_COMPLETE,	true	},
    { "no_idle",			DUNGEON_NO_IDLE,			true	},
    { "no_save",			DUNGEON_NO_SAVE,			true	},
    { "scripted_levels",    DUNGEON_SCRIPTED_LEVELS,    false   },
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

const struct flag_type command_types[] =
{
    { "unsorted",		CMDTYPE_NONE,	    true },
    { "movement",		CMDTYPE_MOVE,	    true },
    { "combat",		CMDTYPE_COMBAT,	    true },
    { "object",		CMDTYPE_OBJECT,	    true },
    { "information",		CMDTYPE_INFO,	    true },
    { "communication",		CMDTYPE_COMM,	    true },
    { "racial",		CMDTYPE_RACIAL,	    true },
    { "configuration",		CMDTYPE_OOC,	    true },
    { "immortal",	CMDTYPE_IMMORTAL,	true },
    { "olc",		CMDTYPE_OLC,	    true },
    { "admin",		CMDTYPE_ADMIN,	    true },
    { "newbie",		CMDTYPE_NEWBIE,	    true },
    {NULL,          0,                  false}

};

const struct flag_type command_addl_types[] =
{
    { "unsorted",		CMD_TYPE_NONE,	    true },
    { "movement",		CMD_TYPE_MOVE,	    true },
    { "combat",		CMD_TYPE_COMBAT,	    true },
    { "object",		CMD_TYPE_OBJECT,	    true },
    { "information",		CMD_TYPE_INFO,	    true },
    { "communication",		CMD_TYPE_COMM,	    true },
    { "racial",		CMD_TYPE_RACIAL,	    true },
    { "configuration",		CMD_TYPE_OOC,	    true },
    { "immortal",	CMD_TYPE_IMMORTAL,	true },
    { "olc",		CMD_TYPE_OLC,	    true },
    { "admin",		CMD_TYPE_ADMIN,	    true },
    { "newbie",		CMD_TYPE_NEWBIE,	    true },
    {NULL,          0,                  false}

};

const struct flag_type command_flags[] =
{
    {"hide_lists",       CMD_HIDE_LISTS,     true },
    {"is_ooc",           CMD_IS_OOC,         true },
    {NULL,               0,                  false}
};

const struct do_func_type do_func_table[] =
{
        { "do_north",                   do_north },
        { "do_east",                    do_east },
        { "do_south",                   do_south },
        { "do_west",                    do_west },
        { "do_up",                      do_up },
        { "do_down",                    do_down },
        { "do_northeast",                       do_northeast },
        { "do_northwest",                       do_northwest },
        { "do_southeast",                       do_southeast },
        { "do_southwest",                       do_southwest },
        { "do_northeast",                       do_northeast },
        { "do_northwest",                       do_northwest },
        { "do_southeast",                       do_southeast },
        { "do_southwest",                       do_southwest },
        { "do_help",                    do_help },
        { "do_area",                    do_area },
        { "do_areas",                   do_areas },
        { "do_at",                      do_at },
        { "do_auction",                 do_auction },
        { "do_bar",                     do_bar },
        { "do_bomb",                    do_bomb },
        { "do_buy",                     do_buy },
        { "do_cast",                    do_cast },
        { "do_channels",                        do_channels },
        { "do_danger",                  do_danger },
        { "do_donate",                  do_donate },
        { "do_evasion",                 do_evasion },
        { "do_exits",                   do_exits },
        { "do_fade",                    do_fade },
        { "do_get",                     do_get },
        { "do_goto",                    do_goto },
        { "do_goxy",                    do_goxy },
        { "do_group",                   do_group },
        { "do_kill",                    do_kill },
        { "do_wear",                    do_wear },
        { "do_inventory",                       do_inventory },
        { "do_judge",                   do_judge },
        { "do_kill",                    do_kill },
        { "do_look",                    do_look },
        { "do_music",                   do_music },
        { "do_order",                   do_order },
        { "do_practice",                        do_practice },
        { "do_pull",                    do_pull },
        { "do_push",                    do_push },
        { "do_quest",                   do_quest },
        { "do_rehearse",                        do_rehearse },
        { "do_renew",                   do_renew },
        { "do_reply",                   do_reply },
        { "do_rest",                    do_rest },
        { "do_resurrect",                       do_resurrect },
        { "do_sit",                     do_sit },
        { "do_stand",                   do_stand },
        { "do_stat",                    do_stat },
        { "do_tell",                    do_tell },
        { "do_tells",                   do_tells },
        { "do_turn",                    do_turn },
        { "do_unlock",                  do_unlock },
        { "do_ungroup",                 do_ungroup },
        { "do_use",                     do_use },
        { "do_wear",                    do_wear },
        { "do_conceal",                 do_conceal },
        { "do_affects",                 do_affects },
        { "do_challenge",                       do_challenge },
        { "do_changes",                 do_changes },
        { "do_commands",                        do_commands },
        { "do_consider",                        do_consider },
        { "do_count",                   do_count },
        { "do_credits",                 do_credits },
        { "do_dice",                    do_dice },
        { "do_equipment",                       do_equipment },
        { "do_examine",                 do_examine },
        { "do_flag",                    do_flag },
        { "do_worth",                   do_worth },
        { "do_help",                    do_help },
        { "do_house",                   do_house },
        { "do_intimidate",                      do_intimidate },
        { "do_motd",                    do_motd },
        { "do_compress",                        do_compress },
        { "do_news",                    do_news },
        { "do_look",                    do_look },
        { "do_repair",                  do_repair },
        { "do_report",                  do_report },
        { "do_rules",                   do_rules },
        { "do_score",                   do_score },
        { "do_scry",                    do_scry },
        { "do_skills",                  do_skills },
        { "do_socials",                 do_socials },
        { "do_spells",                  do_spells },
        { "do_stats",                   do_stats },
        { "do_time",                    do_time },
        { "do_showversion",                     do_showversion },
        { "do_war",                     do_war },
        { "do_weather",                 do_weather },
        { "do_who_new",                 do_who_new },
        { "do_whois",                   do_whois },
        { "do_wizlist",                 do_wizlist },
        { "do_alias",                   do_alias },
        { "do_toggle",                  do_toggle },
        { "do_colour",                  do_colour },
        { "do_colour",                  do_colour },
        { "do_toggle",                  do_toggle },
        { "do_description",                     do_description },
        { "do_delet",                   do_delet },
        { "do_delete",                  do_delete },
        { "do_email",                   do_email },
        { "do_password",                        do_password },
        { "do_prompt",                  do_prompt },
        { "do_showdamage",                      do_showdamage },
        { "do_scroll",                  do_scroll },
        { "do_title",                   do_title },
        { "do_toggle",                  do_toggle },
        { "do_unalias",                 do_unalias },
        { "do_wimpy",                   do_wimpy },
        { "do_afk",                     do_afk },
        { "do_announcements",                   do_announcements },
        { "do_catchup",                 do_catchup },
        { "do_chat",                    do_chat },
        { "do_chat",                    do_chat },
        { """,                  " },
        { "do_gossip",                  do_gossip },
        { "do_gtell",                   do_gtell },
        { "do_say",                     do_say },
        { "do_sayto",                   do_sayto },
        { "do_emote",                   do_emote },
        { "do_flame",                   do_flame },
        { "do_formstate",                       do_formstate },
        { "do_gossip",                  do_gossip },
        { "do_gtell",                   do_gtell },
        { "do_helper",                  do_helper },
        { "do_hints",                   do_hints },
        { "do_ignore",                  do_ignore },
        { "do_intone",                  do_intone },
        { "do_note",                    do_note },
        { "do_notify",                  do_notify },
        { "do_ooc",                     do_ooc },
        { "do_pursuit",                 do_pursuit },
        { "do_qlist",                   do_qlist },
        { "do_quiet",                   do_quiet },
        { "do_quote",                   do_quote },
        { "do_replay",                  do_replay },
        { "do_say",                     do_say },
        { "do_unread",                  do_unread },
        { "do_whisper",                 do_whisper },
        { "do_yell",                    do_yell },
        { "do_activate",                        do_activate },
        { "do_bind",                    do_bind },
        { "do_blow",                    do_blow },
        { "do_brandish",                        do_brandish },
        { "do_brew",                    do_brew },
        { "do_close",                   do_close },
        { "do_combine",                 do_combine },
        { "do_activate",                        do_activate },
        { "do_deposit",                 do_deposit },
        { "do_drink",                   do_drink },
        { "do_drop",                    do_drop },
        { "do_eat",                     do_eat },
        { "do_envenom",                 do_envenom },
        { "do_fill",                    do_fill },
        { "do_give",                    do_give },
        { "do_hands",                   do_hands },
//        { "do_heal",                    do_heal },
        { "do_wear",                    do_wear },
        { "do_infuse",                  do_infuse },
        { "do_inspect",                 do_inspect },
        { "do_keep",                    do_keep },
        { "do_knock",                   do_knock },
        { "do_list",                    do_list },
        { "do_lock",                    do_lock },
        { "do_locker",                  do_locker },
        { "do_lore",                    do_lore },
        { "do_mail",                    do_mail },
        { "do_open",                    do_open },
        { "do_pick",                    do_pick },
        { "do_plant",                   do_plant },
        { "do_pour",                    do_pour },
        { "do_put",                     do_put },
        { "do_quaff",                   do_quaff },
        { "do_recite",                  do_recite },
        { "do_remove",                  do_remove },
        { "do_restring",                        do_restring },
        { "do_sacrifice",                       do_sacrifice },
        { "do_scribe",                  do_scribe },
        { "do_sell",                    do_sell },
        { "do_skull",                   do_skull },
        { "do_strike",                  do_strike },
        { "do_get",                     do_get },
        { "do_unrestring",                      do_unrestring },
        { "do_value",                   do_value },
        { "do_wear",                    do_wear },
        { "do_zap",                     do_zap },
        { "do_affix",                   do_affix },
        { "do_ink",                     do_ink },
        { "do_ruboff",                  do_ruboff },
        { "do_touch",                   do_touch },
        { "do_takeoff",                 do_takeoff },
        { "do_land",                    do_land },
        { "do_expand",                  do_expand },
        { "do_collapse",                        do_collapse },
        { "do_bite",                    do_bite },
        { "do_consume",                 do_consume },
        { "do_bite",                    do_bite },
        { "do_shape",                   do_shape },
        { "do_shift",                   do_shift },
        { "do_toxins",                  do_toxins },
        { "do_ambush",                  do_ambush },
        { "do_backstab",                        do_backstab },
        { "do_bash",                    do_bash },
        { "do_behead",                  do_behead },
        { "do_berserk",                 do_berserk },
        { "do_blackjack",                       do_blackjack },
        { "do_breathe",                 do_breathe },
        { "do_backstab",                        do_backstab },
        { "do_charge",                  do_charge },
        { "do_circle",                  do_circle },
        { "do_dirt",                    do_dirt },
        { "do_disarm",                  do_disarm },
        { "do_feign",                   do_feign },
        { "do_flee",                    do_flee },
        { "do_holdup",                  do_holdup },
        { "do_kick",                    do_kick },
        { "do_kill",                    do_kill },
        { "do_pk",                      do_pk },
        { "do_rack",                    do_rack },
        { "do_rescue",                  do_rescue },
        { "do_shoot",                   do_shoot },
        { "do_slit",                    do_slit },
        { "do_smite",                   do_smite },
        { "do_stake",                   do_stake },
        { "do_tail_kick",                       do_tail_kick },
        { "do_throw",                   do_throw },
        { "do_trample",                 do_trample },
        { "do_warcry",                  do_warcry },
        { "do_weave",                   do_weave },
        { "do_mob",                     do_mob },
        { "do_ship",                    do_ship },
//        { "do_steer",                   do_steer },
//        { "do_scuttle",                 do_scuttle },
//        { "do_aim",                     do_aim },
//        { "do_boat_chase",                      do_boat_chase },
//        { "do_crew",                    do_crew },
        { "do_dig",                     do_dig },
        { "do_bank",                    do_bank },
        { "do_church",                  do_church },
        { "do_clear",                   do_clear },
        { "do_convert",                 do_convert },
        { "do_chtalk",                  do_chtalk },
        { "do_disembark",                       do_disembark },
        { "do_dismount",                        do_dismount },
        { "do_enter",                   do_enter },
        { "do_follow",                  do_follow },
        { "do_enter",                   do_enter },
        { "do_gohome",                  do_gohome },
        { "do_hide",                    do_hide },
        { "do_hunt",                    do_hunt },
        { "do_mount",                   do_mount },
        { "do_multi",                   do_multi },
        { "do_play",                    do_play },
        { "do_quit",                    do_quit },
        { "do_recall",                  do_recall },
        { "do_return",                  do_return },
        { "do_reverie",                 do_reverie },
        { "do_save",                    do_save },
        { "do_scan",                    do_scan },
        { "do_search",                  do_search },
        { "do_secondary",                       do_secondary },
        { "do_sleep",                   do_sleep },
        { "do_sneak",                   do_sneak },
        { "do_split",                   do_split },
        { "do_steal",                   do_steal },
        { "do_survey",                  do_survey },
        { "do_train",                   do_train },
        { "do_visible",                 do_visible },
        { "do_wake",                    do_wake },
        { "do_warp",                    do_warp },
        { "do_where",                   do_where },
        { "do_whistle",                 do_whistle },
        { "do_addcommand",                      do_addcommand },
        { "do_advance",                 do_advance },
        { "do_alevel",                  do_alevel },
        { "do_allow",                   do_allow },
        { "do_aload",                   do_aload },
        { "do_arealinks",                       do_arealinks },
        { "do_areset",                  do_areset },
        { "do_assignhelper",                    do_assignhelper },
        { "do_autosetname",                     do_autosetname },
        { "do_autowar",                 do_autowar },
        { "do_ban",                     do_ban },
        { "do_besteq",                  do_besteq },
        { "do_boost",                   do_boost },
        { "do_botter",                  do_botter },
        { "do_build",                   do_build },
        { "do_clone",                   do_clone },
        { "do_deny",                    do_deny },
        { "do_disconnect",                      do_disconnect },
        { "do_dump",                    do_dump },
        { "do_immtalk",                 do_immtalk },
        { "do_evict",                   do_evict },
        { "do_force",                   do_force },
        { "do_freeze",                  do_freeze },
        { "do_echo",                    do_echo },
        { "do_gq",                      do_gq },
        { "do_holyaura",                        do_holyaura },
        { "do_holylight",                       do_holylight },
        { "do_holywarp",                        do_holywarp },
        { "do_housemove",                       do_housemove },
        { "do_immflag",                 do_immflag },
        { "do_immtalk",                 do_immtalk },
        { "do_immortalise",                     do_immortalise },
        { "do_memory",                  do_memory },
        { "do_imotd",                   do_imotd },
        { "do_incognito",                       do_incognito },
        { "do_invis",                   do_invis },
        { "do_junk",                    do_junk },
        { "do_load",                    do_load },
        { "do_log",                     do_log },
        { "do_mlevel",                  do_mlevel },
        { "do_mpdump",                  do_mpdump },
        { "do_mpstat",                  do_mpstat },
        { "do_mwhere",                  do_mwhere },
        { "do_newlock",                 do_newlock },
        { "do_nochannels",                      do_nochannels },
        { "do_notell",                  do_notell },
        { "do_olevel",                  do_olevel },
        { "do_otransfer",                       do_otransfer },
        { "do_owhere",                  do_owhere },
        { "do_peace",                   do_peace },
        { "do_pecho",                   do_pecho },
        { "do_permban",                 do_permban },
        { "do_bamfin",                  do_bamfin },
        { "do_bamfout",                 do_bamfout },
        { "do_purge",                   do_purge },
        { "do_reboo",                   do_reboo },
        { "do_reboot",                  do_reboot },
        { "do_reckonin",                        do_reckonin },
        { "do_reckoning",                       do_reckoning },
        { "do_remcommand",                      do_remcommand },
        { "do_restore",                 do_restore },
        { "do_rwhere",                  do_rwhere },
        { "do_set",                     do_set },
        { "do_shutdow",                 do_shutdow },
        { "do_shutdown",                        do_shutdown },
        { "do_slay",                    do_slay },
        { "do_sockets",                 do_sockets },
        { "do_staff",                   do_staff },
        { "do_startinvasion",                   do_startinvasion },
        { "do_string",                  do_string },
        { "do_switch",                  do_switch },
        { "do_testport",                do_testport },
        { "do_test",                    do_test },
        { "do_tlist",                   do_tlist },
        { "do_token",                   do_token },
        { "do_tshow",                   do_tshow },
        { "do_trance",                  do_trance },
        { "do_transfer",                        do_transfer },
        { "do_uninvis",                 do_uninvis },
        { "do_uninvis",                 do_uninvis },
        { "do_vislist",                 do_vislist },
        { "do_vlinks",                  do_vlinks },
        { "do_vnum",                    do_vnum },
        { "do_wizhelp",                 do_wizhelp },
        { "do_invis",                   do_invis },
        { "do_wizlock",                 do_wizlock },
        { "do_wiznet",                  do_wiznet },
        { "do_zecho",                   do_zecho },
        { "do_zot",                     do_zot },
        { "do_aedit",                   do_aedit },
        { "do_alist",                   do_alist },
        { "do_asave_new",                       do_asave_new },
        { "do_asearch",                 do_asearch },
        { "do_dislink",                 do_dislink },
        { "do_olc",                     do_olc },
        { "do_hedit",                   do_hedit },
        { "do_ifchecks",                        do_ifchecks },
        { "do_mcopy",                   do_mcopy },
        { "do_medit",                   do_medit },
        { "do_mlist",                   do_mlist },
        { "do_mpcopy",                  do_mpcopy },
        { "do_mpdelete",                        do_mpdelete },
        { "do_mpedit",                  do_mpedit },
        { "do_mplist",                  do_mplist },
        { "do_mshow",                   do_mshow },
        { "do_ocopy",                   do_ocopy },
        { "do_oedit",                   do_oedit },
        { "do_olist",                   do_olist },
        { "do_opcopy",                  do_opcopy },
        { "do_opdelete",                        do_opdelete },
        { "do_opdump",                  do_opdump },
        { "do_opedit",                  do_opedit },
        { "do_oplist",                  do_oplist },
        { "do_opstat",                  do_opstat },
        { "do_oshow",                   do_oshow },
        { "do_pedit",                   do_pedit },
        { "do_pinquiry",                        do_pinquiry },
        { "do_plist",                   do_plist },
        { "do_pdelete",                 do_pdelete },
        { "do_project",                 do_project },
        { "do_pshow",                   do_pshow },
        { "do_rcopy",                   do_rcopy },
        { "do_redit",                   do_redit },
        { "do_resets",                  do_resets },
        { "do_rjunk",                   do_rjunk },
        { "do_rlist",                   do_rlist },
        { "do_rpcopy",                  do_rpcopy },
        { "do_rpdelete",                        do_rpdelete },
        { "do_rpdump",                  do_rpdump },
        { "do_rpedit",                  do_rpedit },
        { "do_rplist",                  do_rplist },
        { "do_rpstat",                  do_rpstat },
        { "do_rshow",                   do_rshow },
        { "do_tedit",                   do_tedit },
        { "do_tpdump",                  do_tpdump },
        { "do_tpedit",                  do_tpedit },
        { "do_tplist",                  do_tplist },
        { "do_tpstat",                  do_tpstat },
        { "do_wedit",                   do_wedit },
        { "do_vledit",                  do_vledit },
        { "do_wlist",                   do_wlist },
        { "do_slist",                   do_slist },
        { "do_sadd",                    do_sadd },
        { "do_sdelete",                 do_sdelete },
        { "do_ssupervisor",                     do_ssupervisor },
        { "do_sduty",                   do_sduty },
        { "do_bpedit",                  do_bpedit },
        { "do_bplist",                  do_bplist },
        { "do_bpshow",                  do_bpshow },
        { "do_bsedit",                  do_bsedit },
        { "do_bslist",                  do_bslist },
        { "do_bsshow",                  do_bsshow },
        { "do_instance",                        do_instance },
        { "do_dngedit",                 do_dngedit },
        { "do_dnglist",                 do_dnglist },
        { "do_dngshow",                 do_dngshow },
        { "do_dungeon",                 do_dungeon },
        { "do_apdump",                  do_apdump },
        { "do_apedit",                  do_apedit },
        { "do_aplist",                  do_aplist },
        { "do_ipdump",                  do_ipdump },
        { "do_ipedit",                  do_ipedit },
        { "do_iplist",                  do_iplist },
        { "do_dpdump",                  do_dpdump },
        { "do_dpedit",                  do_dpedit },
        { "do_dplist",                  do_dplist },
        { "do_shedit",                  do_shedit },
        { "do_shlist",                  do_shlist },
        { "do_shshow",                  do_shshow },
        { "do_ships",                   do_ships },
        { "do_spawntreasuremap",                        do_spawntreasuremap },
        { "do_reloadstats",                     do_reloadstats },
        { "do_cmdlist",                 do_cmdlist },
        { "do_cmdedit",                 do_cmdedit },
        { "do_cmdshow",                 do_cmdshow },
        { "do_testemail",                do_testemail },
        { "do_pwreset",                 do_pwreset  },
        { "do_lvlaudit",                do_lvlaudit },
        { "do_keygen",                     do_keygen },
        { "do_mfareset",                do_mfareset },
        { "do_logout",                do_logout },
        { "do_gameedit",                 do_gameedit },
        { "do_accnote", do_accnote },
        { "do_vault", do_vault },
        { "do_coffer", do_coffer },
        { "do_acctlink", do_acctlink },
        { "do_acctunlink", do_acctunlink },
        { "do_socialedit", do_socialedit },
        { "do_reserved", do_reserved },
        { "do_pronouns", do_pronouns },
        { "do_gcstats", do_gcstats },
        { "do_cachestats", do_cachestats },
        { "do_cacheinfo", do_cacheinfo },
        { "do_cachedump", do_cachedump },
        { "do_cacheload", do_cacheload },
        { "do_cachejobs", do_cachejobs },
        { "do_cachestop", do_cachestop },
        { NULL, NULL }
};

/* Table mapping connection states to display strings */

const struct con_state_info con_states[] = {
    { CON_PLAYING,                        "Playing" },
    { CON_GET_NAME,                       "Get name" },
    { CON_GET_OLD_PASSWORD,               "Get old password" },
    { CON_CONFIRM_NEW_NAME,               "Confirm new name" },
    { CON_GET_NEW_PASSWORD,               "Get new password" },
    { CON_CONFIRM_NEW_PASSWORD,           "Confirm new password" },
    { CON_GET_NEW_RACE,                   "Get new race" },
    { CON_GET_NEW_SEX,                    "Get new sex" },
    { CON_GET_NEW_BODY_TYPE,              "Get body type" },
    { CON_GET_NEW_CLASS,                  "Get new class" },
    { CON_GET_ALIGNMENT,                  "Get alignment" },
    /* 10: CON_DEFAULT_CHOICE (unused) */
    /* 11: CON_GEN_GROUPS (unused) */
    /* 12: CON_PICK_WEAPON (unused) */
    { CON_READ_IMOTD,                     "Reading IMOTD" },
    { CON_READ_MOTD,                      "Reading MOTD" },
    { CON_BREAK_CONNECT,                  "Breaking connection" },
    { CON_GET_ASCII,                      "Get ASCII" },
    { CON_GET_SUB_CLASS,                  "Get subclass" },
    { CON_OLD_SUBCLASS,                   "Old subclass" },
    { CON_SUBCLASS_CHOOSE,                "Choose subclass" },
    { CON_CHANGE_PASSWORD,                "Change password" },
    { CON_CHANGE_PASSWORD_CONFIRM,        "Confirm password change" },
    { CON_GET_EMAIL,                      "Get email" },
    { CON_CONFIRM_EMAIL_FOR_RESET,        "Confirm email for reset" },
    { CON_GET_MFA,                        "Get MFA code" },
    { CON_GET_ACCOUNT_NAME,               "Get account name" },
    { CON_GET_ACCOUNT_PASSWORD,           "Get account password" },
    { CON_NEW_ACCOUNT_PASSWORD,           "New account password" },
    { CON_CONFIRM_ACCOUNT_PASSWORD,       "Confirm account password" },
    { CON_GET_ACCOUNT_EMAIL,              "Get account email" },
    { CON_ACCOUNT_MENU,                   "Account menu" },
    { CON_CREATING_NEW_CHAR,              "Creating new character" },
    { CON_SELECT_CHARACTER,               "Select character" },
    { CON_CONFIRM_ACCOUNT_NAME,           "Confirm account name" },
    { CON_LINK_CHARACTER_NAME,            "Link character name" },
    { CON_LINK_CHARACTER_PASSWORD,        "Link character password" },
    { CON_LINK_CHARACTER_MFA,             "Link character MFA" },
    { CON_CHARACTER_MENU,                 "Character menu" },
    { CON_CHARACTER_PASSWORD,             "Character password" },
    { CON_CONFIRM_CHARACTER_PASSWORD,     "Confirm character password" },
    { CON_CHARACTER_MFA_TOGGLE,           "Character MFA toggle" },
    { CON_CONFIRM_DELETE_CHARACTER,       "Confirm delete character" },
    { CON_GET_CHAR_PASSWORD,              "Get character password" },
    { CON_GET_CHAR_MFA,                   "Get character MFA" },
    { CON_CHARACTER_MFA_VERIFY,           "Verify character MFA" },
    { CON_ACCOUNT_MFA_VERIFY,             "Verify account MFA" },
    { CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET, "Confirm account email for reset" },
    { CON_CHANGE_ACCOUNT_PASSWORD,        "Change account password" },
    { CON_CONFIRM_ACCOUNT_PASSWORD_CHANGE, "Confirm account password change" },
    { CON_GET_ACCOUNT_MFA,                "Get account MFA" },
    { CON_CHANGE_ACCOUNT_EMAIL,           "Change account email" },
    { CON_VERIFY_ACCOUNT_PASSWORD,        "Verify account password" },
    { CON_ACCOUNT_MFA_MENU,               "Account MFA menu" },
    { CON_GET_ACCOUNT_MFA_FOR_CHAR,       "Get account MFA for character" },
    { CON_VERIFY_DELETE_PASSWORD,         "Verify delete password" },
    { CON_VERIFY_DELETE_MFA,              "Verify delete MFA" },
    { CON_CREATING_NEW_STAFF_CHAR,        "Creating new staff character" },
    { CON_GET_STAFF_EMAIL,                "Get staff email" },
    { CON_STAFF_MFA_PROMPT,               "Get staff MFA" },
    { CON_STAFF_PASSWORD,                 "Get staff password" },
    { CON_CONFIRM_STAFF_PASSWORD,         "Confirm staff password" },
    { CON_CHARACTER_MFA_VERIFY_FOR_SETTINGS, "Verify character MFA for settings" },
    { CON_CHARACTER_MFA_MENU,             "Character MFA menu" },
    { CON_CHARACTER_MFA_CONFIRM,          "Confirm character MFA" },
    { CON_CHARACTER_MFA_DISABLE_CONFIRM,  "Confirm disable character MFA" },
    { CON_ACCOUNT_MFA_VERIFY_FOR_SETTINGS, "Verify account MFA for settings" },
    { CON_ACCOUNT_MFA_CONFIRM,            "Confirm account MFA" },
    { CON_ACCOUNT_MFA_DISABLE_CONFIRM,    "Confirm disable account MFA" },
    { CON_VERIFY_ACCOUNT_EMAIL_CHANGE,    "Verify account email change" },
    { CON_CHANGE_CHARACTER_EMAIL,         "Change character email" },
    { CON_VERIFY_CHARACTER_EMAIL_CHANGE,  "Verify character email change" },
    { CON_VERIFY_UNLINK_PASSWORD,         "Verify unlink password" },
    { CON_VERIFY_UNLINK_MFA,              "Verify unlink MFA" },
    { CON_SET_UNLINK_PASSWORD,            "Set unlink password" },
    { CON_VERIFY_CHARACTER_DELETE,        "Verify character delete" },
    { CON_CHARACTER_DELETE,               "Character delete" },
    { CON_CONFIRM_DEFAULT_PRONOUNS,       "Confirm default pronouns" },
    { CON_SET_CUSTOM_PRONOUN_SUBJ,        "Set subject pronoun" },
    { CON_SET_CUSTOM_PRONOUN_OBJ,         "Set object pronoun" },
    { CON_SET_CUSTOM_PRONOUN_POSS_ADJ,    "Set possessive adjective" },
    { CON_SET_CUSTOM_PRONOUN_POSS_PRON,   "Set possessive pronoun" },
    { CON_SET_CUSTOM_PRONOUN_REFL,        "Set reflexive pronoun" },
    { CON_SET_CUSTOM_VERB_PREF,           "Set verb preference" },
    { CON_SET_CUSTOM_PRONOUNS_CONFIRM,    "Confirm custom pronouns" },
    { 0, NULL }  /* Terminator */
};

const struct flag_type acct_flags[] =
{
    {	"create_staff",			ACCT_CAN_CREATE_STAFF,				true	},
    {   "link_chars",           ACCT_CAN_LINK,  true },
    {   "unlink_chars",         ACCT_CAN_UNLINK, true },
    {	NULL,	0,	0	}
};

// ...existing code...





const struct game_setting_type game_settings_table[] = {
    /* Core Settings - NOT overridable by environment variables */
    { "env_var_prefix",      &game_settings.env_var_prefix,       SETTING_TYPE_STRING, SETTING_CAT_CORE,     "Prefix for environment variable overrides",                  false, false, false },
    { "secrets_mount",       &game_settings.secrets_mount,        SETTING_TYPE_STRING, SETTING_CAT_CORE,     "Path to Doppler/secrets mount (JSON format)",                 false, false, false },

    /* Email Settings */
    { "email_enable",        &game_settings.enable_email,         SETTING_TYPE_BOOL,   SETTING_CAT_EMAIL,    "Enable email functionality",                                 true,  false, false },
    { "email_username",      &game_settings.email_username,       SETTING_TYPE_STRING, SETTING_CAT_EMAIL,    "Username for the email account",                             true,  false, true },
    { "email_password",      &game_settings.email_password,       SETTING_TYPE_STRING, SETTING_CAT_EMAIL,    "Password for the email account",                             true,  false, true },
    { "email_host",          &game_settings.email_host,           SETTING_TYPE_STRING, SETTING_CAT_EMAIL,    "Hostname of the email server",                               true,  false, false },
    { "email_port",          &game_settings.email_port,           SETTING_TYPE_INT,    SETTING_CAT_EMAIL,    "Port that the email server is listening on",                 true,  false, false },
    { "email_from_addr",     &game_settings.email_from_addr,      SETTING_TYPE_STRING, SETTING_CAT_EMAIL,    "Email address that will appear in the 'from' field",         true,  false, false },
    { "email_from_name",     &game_settings.email_from_name,      SETTING_TYPE_STRING, SETTING_CAT_EMAIL,    "Name that will appear in the 'from' field",                  true,  false, false },

    /* Org Settings*/
    { "org_max_ranks",        &game_settings.org_max_ranks,        SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "How many ranks can an organization have",                    true,  false, false },
    { "max_orgs",            &game_settings.max_orgs,             SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Maximum organizations that can exist",                       true,  false, false },
    { "org_pk_cost",        &game_settings.org_disable_pk_pneuma_cost,           SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Cost to disable PK for your church",               true,  false, false },
    { "system_area",        &game_settings.system_area,           SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "Default/system area name or UID for reserved and widevnum fallback", true, false, false },




    /* Mission Settings */
    { "max_mission_allowance", &game_settings.max_mission_allowance, SETTING_TYPE_INT, SETTING_CAT_MISSION,  "Maximum mission allowances a player can have",               true,  false, false },
    { "inc_missions",        &game_settings.inc_missions,         SETTING_TYPE_INT,    SETTING_CAT_MISSION,  "Number of missions a player accrues on allowance tick",      true,  false, false },
    { "max_missions",        &game_settings.max_missions,         SETTING_TYPE_INT,    SETTING_CAT_MISSION,  "Maximum missions a player can run simultaneously",           true,  false, false },

    /* Locker Settings */
    { "lockers_enabled",     &game_settings.lockers_enabled,      SETTING_TYPE_BOOL,   SETTING_CAT_LOCKER,   "Are lockers enabled",                                        true,  false, false },
    { "locker_rent_enabled", &game_settings.locker_rent_enabled,  SETTING_TYPE_BOOL,   SETTING_CAT_LOCKER,   "Do lockers cost rent",                                       true,  false, false },
    { "max_locker_weight",   &game_settings.max_locker_weight,    SETTING_TYPE_INT,    SETTING_CAT_LOCKER,   "Maximum weight a player can have in their locker",           true,  false, false },
    { "max_locker_items",    &game_settings.max_locker_items,     SETTING_TYPE_INT,    SETTING_CAT_LOCKER,   "Maximum items a player can have in their locker",            true,  false, false },
    { "locker_rent_cost",    &game_settings.locker_rent_cost,     SETTING_TYPE_INT,    SETTING_CAT_LOCKER,   "Cost to rent a locker",                                      true,  false, false },
    { "locker_rent_time",    &game_settings.locker_rent_time,     SETTING_TYPE_INT,    SETTING_CAT_LOCKER,   "Duration rent grants access (in days)",                      true,  false, false },
    { "locker_rent_time_max", &game_settings.locker_rent_time_max, SETTING_TYPE_INT,   SETTING_CAT_LOCKER,   "Maximum duration a locker can be rented at once (in days)",  true,  false, false },
    { "locker_add_cost_tier", &game_settings.locker_additional_cost_per_tier, SETTING_TYPE_INT, SETTING_CAT_LOCKER, "Additional cost per tier of locker",                  true,  false, false },
    { "locker_add_slots_tier", &game_settings.locker_additional_slots_per_tier, SETTING_TYPE_INT, SETTING_CAT_LOCKER, "Additional slots per tier of locker",                true,  false, false },
    { "locker_add_weight_tier", &game_settings.locker_additional_weight_per_tier, SETTING_TYPE_INT, SETTING_CAT_LOCKER, "Additional weight per tier of locker",            true,  false, false },
    { "locker_tier_max",     &game_settings.locker_tier_max,      SETTING_TYPE_INT,    SETTING_CAT_LOCKER,   "Maximum upgrades that can be applied to a locker",           true,  false, false },

    /* Vault Settings */
    { "max_vault_weight",    &game_settings.max_vault_weight,     SETTING_TYPE_INT,    SETTING_CAT_VAULT,    "Maximum weight a player can have in their vault",            true,  false, false },
    { "max_vault_items",     &game_settings.max_vault_items,      SETTING_TYPE_INT,    SETTING_CAT_VAULT,    "Maximum items a player can have in their vault",             true,  false, false },
    { "vault_enabled",       &game_settings.vault_enabled,        SETTING_TYPE_BOOL,   SETTING_CAT_VAULT,    "Is the vault enabled",                                       true,  false, false },
    { "vault_rent",          &game_settings.vault_rent,           SETTING_TYPE_BOOL,   SETTING_CAT_VAULT,    "Does the vault cost rent",                                   true,  false, false },
    { "vault_rent_per_char", &game_settings.vault_rent_per_char,  SETTING_TYPE_BOOL,   SETTING_CAT_VAULT,    "Does vault rent cost per character",                         true,  false, false },
    { "vault_rent_cost",     &game_settings.vault_rent_cost,      SETTING_TYPE_INT,    SETTING_CAT_VAULT,    "Base rental cost for the vault",                             true,  false, false },
    { "vault_rent_time",     &game_settings.vault_rent_time,      SETTING_TYPE_INT,    SETTING_CAT_VAULT,    "Duration rent grants access (in days)",                      true,  false, false },
    { "vault_rent_time_max", &game_settings.vault_rent_time_max,  SETTING_TYPE_INT, SETTING_CAT_VAULT, "Maximum duration of vault rental", true, false, false },
    { "vault_add_cost_char", &game_settings.vault_additional_cost_per_char, SETTING_TYPE_INT, SETTING_CAT_VAULT, "Additional cost per character",                          true,  false, false },
    { "vault_add_slots_char", &game_settings.vault_additional_slots_per_char, SETTING_TYPE_INT, SETTING_CAT_VAULT, "Additional slots per character",                       true,  false, false },
    { "vault_add_weight_char", &game_settings.vault_additional_weight_per_char, SETTING_TYPE_INT, SETTING_CAT_VAULT, "Additional weight per character",                    true,  false, false },
    { "vault_require_room", &game_settings.vault_require_room, SETTING_TYPE_BOOL, SETTING_CAT_VAULT, "Do we require a room to be in the vault?", true, false, false },

    /* Coffer Settings */
    { "coffer_enabled",      &game_settings.coffer_enabled,       SETTING_TYPE_BOOL,   SETTING_CAT_COFFER,   "Is the coffer enabled",                                      true,  false, false },
    { "coffer_rent",         &game_settings.coffer_rent,          SETTING_TYPE_BOOL,   SETTING_CAT_COFFER,   "Does the coffer cost rent",                                  true,  false, false },
    { "max_coffer_weight",   &game_settings.max_coffer_weight,    SETTING_TYPE_INT,    SETTING_CAT_COFFER,   "Maximum weight an organization can have in their coffer",    true,  false, false },
    { "max_coffer_items",    &game_settings.max_coffer_items,     SETTING_TYPE_INT,    SETTING_CAT_COFFER,   "Maximum items an organization can have in their coffer",     true,  false, false },
    { "coffer_rent_currency", &game_settings.coffer_rent_currency, SETTING_TYPE_STRING, SETTING_CAT_COFFER,  "Currency used for coffer rent",                              true,  false, false },
    { "coffer_rent_time",  &game_settings.coffer_rent_time,   SETTING_TYPE_INT,    SETTING_CAT_COFFER,   "Duration rent grants access to an org (in days)",            true,  false, false },
    { "coffer_rent_time_max", &game_settings.coffer_rent_time_max, SETTING_TYPE_INT, SETTING_CAT_COFFER, "Maximum duration of coffer rental", true, false, false },

    /* Global Settings */
    { "game_name",           &game_settings.game_name,            SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "Name of the game, used in MSSP",                             true,  true, false },
    { "login_string",        &game_settings.login_string,         SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "Login string presented on connection",                       true,  false, false },
    { "server_description",  &game_settings.server_description,   SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "Description of server",                                      true,  false, false },
    { "testport",            &game_settings.testport,             SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Is this a testport",                                         true,  true, false  },
    { "dev_server",          &game_settings.dev_server,           SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Is this a dev/alpha server",                                 true,  true, false  },
    { "enable_passwd",       &game_settings.enable_passwd,        SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Enable passwords",                                           true,  false, false },
    { "enable_mfa",          &game_settings.enable_mfa,           SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Enable multifactor authentication",                          true,  false, false },
    { "wizlock",             &game_settings.wizlock,              SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Prevent non-staff character logins",                         true,  false, false },
    { "wizlock_msg",         &game_settings.wizlock_msg,          SETTING_TYPE_STRING, SETTING_CAT_SECURITY, "Message for wizlocked game",                                 true,  false, false },
    { "new_acct_lock",       &game_settings.new_acct_lock,        SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Prevent making new accounts",                                true,  false, false },
    { "new_acct_lock_msg",   &game_settings.new_acct_lock_msg,    SETTING_TYPE_STRING, SETTING_CAT_SECURITY, "Message for new account lock",                               true,  false, false },
    { "new_char_lock",       &game_settings.new_char_lock,        SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Prevent making new characters on existing accounts",         true,  false, false },
    { "new_char_lock_msg",   &game_settings.new_char_lock_msg,    SETTING_TYPE_STRING, SETTING_CAT_SECURITY, "Message for new character lock",                             true,  false, false },
    { "logall",              &game_settings.logall,               SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Log everything",                                             true,  false, false },
    { "require_email_verify", &game_settings.require_email_verif, SETTING_TYPE_BOOL, SETTING_CAT_SECURITY, "Require email verification for accounts",               true,  false, false },
    { "require_2fa_all",     &game_settings.require_2fa_all,      SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Require all accounts to have multifactor auth",              true,  false, false },
    { "require_2fa_staff",   &game_settings.require_2fa_staff,    SETTING_TYPE_BOOL,   SETTING_CAT_SECURITY, "Require staff accounts to have MFA",                         true,  false, false },
    { "require_uniq_pass_staff", &game_settings.require_uniq_pass_staff, SETTING_TYPE_BOOL, SETTING_CAT_SECURITY, "Require staff characters to have player-level password", true, false, false },
    { "allow_mp_acct_all", &game_settings.allow_mp_acct_all, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Allow multiple characters from one account",            true,  false, false },
    { "allow_mp_acct_staff", &game_settings.allow_mp_acct_staff, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Allow multiple logins from staff accounts",         true,  false, false },
    { "allow_mp_host_all", &game_settings.allow_mp_host_all, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Allow multiple accounts from one host",                 true,  false, false },
    { "allow_mp_host_staff", &game_settings.allow_mp_host_staff, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Allow multiple accounts from one host if one is staff", true, false, false },
    { "allow_link_all",      &game_settings.allow_link_all,       SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Allow all accounts to link chars",                           true,  false, false },
    { "allow_unlink_all",    &game_settings.allow_unlink_all,     SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Allow all accounts to unlink chars",                         true,  false, false },
    { "alignment_system",    &game_settings.alignment_system,     SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Use alignment system",                                       true,  false, false },
    { "restrict_races_align", &game_settings.restrict_races_align, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Restrict races by alignment",                         true, false, false },
    { "restrict_classes_align", &game_settings.restrict_classes_align, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Restrict classes by alignment",                   true, false, false },
    { "max_login_attempts",  &game_settings.max_login_attempts,   SETTING_TYPE_INT,    SETTING_CAT_SECURITY, "Maximum login attempts before disconnecting",                true,  false, false },
    { "idle_time",           &game_settings.idle_time,            SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Ticks until a user is considered idle",                      true,  false, false },
    { "idle_disconnect_time", &game_settings.idle_disconnect_time, SETTING_TYPE_INT,   SETTING_CAT_GLOBAL,   "Ticks until an idle user is disconnected",                   true,  false, false },
    { "max_alias",           &game_settings.max_alias,            SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Maximum aliases a player can have",                          true,  false, false },
    { "max_characters",      &game_settings.max_characters,       SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Maximum characters a player can have",                       true,  false, false },
    { "enable_telnet",       &game_settings.enable_telnet,        SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Allow plaintext connections",                                true,  true, false  },
    { "telnet_port",         &game_settings.telnet_port,          SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Plaintext telnet port",                                      true,  true, false  },
    { "enable_tls",          &game_settings.enable_tls,           SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Allow TLS connections",                                      true,  true, false  },
    { "tls_port",            &game_settings.tls_port,             SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "TLS port",                                                   true,  true, false  },
    { "enable_websocket_tls", &game_settings.enable_websocket_tls, SETTING_TYPE_BOOL,  SETTING_CAT_GLOBAL,   "Allow websocket connections",                                true,  true, false  },
    { "websocket_tls_port",  &game_settings.websocket_tls_port,   SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Websocket port",                                             true,  true, false  },
    { "enable_web",          &game_settings.enable_web,           SETTING_TYPE_BOOL,   SETTING_CAT_GLOBAL,   "Allow web connections",                                      true,  true, false  },
    { "ssl_cert_path",       &game_settings.ssl_cert_path,        SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "SSL certificate path",                                       true,  true, false  },
    { "ssl_key_path",        &game_settings.ssl_key_path,         SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,   "SSL key path",                                               true,  true, false  },
    { "enable_insecure_warning", &game_settings.enable_insecure_warning, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Show warning for insecure connections",                   true,  false, false },
    { "insecure_warning_msg", &game_settings.insecure_warning_msg, SETTING_TYPE_STRING, SETTING_CAT_GLOBAL,  "Message for insecure connections",                           true,  false, false },
    { "max_logfile_size",    &game_settings.max_logfile_size,     SETTING_TYPE_INT,    SETTING_CAT_GLOBAL,   "Size to start rotating logs (in MB)",                        true,  false, false },
    { "note_boot_errors",   &game_settings.note_boot_errors, SETTING_TYPE_BOOL, SETTING_CAT_GLOBAL, "Sends notes with boot errors to 'coder' and 'head coder'", true, true, false },
    { "character_delete",    &game_settings.character_delete_delay_days, SETTING_TYPE_INT, SETTING_CAT_GLOBAL, "Number of days before a character is purged when flagged for deletion.", true, false, false },



    /* MSSP Settings */
    { "mssp_players",         &game_settings.mssp_players,         SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Current players (auto-updated)",                             false, false, false },
    { "mssp_uptime",          &game_settings.mssp_uptime,          SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Server uptime (auto-updated)",                               false, false, false },
    { "mssp_crawl_delay",     &game_settings.mssp_crawl_delay,     SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "How often crawlers should return",                           true,  false, false },
    { "mssp_hostname",        &game_settings.mssp_hostname,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Server hostname",                                            true,  false, false },
    { "mssp_port",            &game_settings.mssp_port,            SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Server port",                                                true,  false, false },
    { "mssp_tls_port",        &game_settings.mssp_tls_port,        SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "TLS server port",                                            true,  false, false },
    { "mssp_codebase",        &game_settings.mssp_codebase,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Name of the codebase",                                       true,  false, false },
    { "mssp_contact",         &game_settings.mssp_contact,         SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Contact email address",                                      true,  false, false },
    { "mssp_created",         &game_settings.mssp_created,         SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Year the MUD was created",                                   true,  false, false },
    { "mssp_ip",              &game_settings.mssp_ip,              SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Current or new IP address",                                  true,  false, false },
    { "mssp_language",        &game_settings.mssp_language,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Language used",                                              true,  false, false },
    { "mssp_location",        &game_settings.mssp_location,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Country where server is located",                            true,  false, false },
    { "mssp_minimum_age",     &game_settings.mssp_minimum_age,     SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Minimum age requirement",                                    true,  false, false },
    { "mssp_website",         &game_settings.mssp_website,         SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "URL to MUD website",                                         true,  false, false },
    { "mssp_family",          &game_settings.mssp_family,          SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "MUD family",                                                 true,  false, false },
    { "mssp_genre",           &game_settings.mssp_genre,           SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Genre",                                                      true,  false, false },
    { "mssp_status",          &game_settings.mssp_status,          SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Development status",                                         true,  false, false },
    { "mssp_gamesystem",      &game_settings.mssp_gamesystem,      SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Game system",                                                true,  false, false },
    { "mssp_intermud",        &game_settings.mssp_intermud,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Intermud protocol",                                          true,  false, false },
    { "mssp_subgenre",        &game_settings.mssp_subgenre,        SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Subgenre",                                                   true,  false, false },
    { "mssp_discord_server",  &game_settings.mssp_discord_server,  SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Discord server URL",                                         true,  false, false },
    { "mssp_areas",           &game_settings.mssp_areas,           SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of areas",                                            true,  false, false },
    { "mssp_helpfiles",       &game_settings.mssp_helpfiles,       SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of helpfiles",                                        true,  false, false },
    { "mssp_mobiles",         &game_settings.mssp_mobiles,         SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of unique mobs",                                      true,  false, false },
    { "mssp_objects",         &game_settings.mssp_objects,         SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of unique objects",                                   true,  false, false },
    { "mssp_rooms",           &game_settings.mssp_rooms,           SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of unique rooms",                                     true,  false, false },
    { "mssp_classes",         &game_settings.mssp_classes,         SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of player classes",                                   true,  false, false },
    { "mssp_levels",          &game_settings.mssp_levels,          SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of player levels",                                    true,  false, false },
    { "mssp_races",           &game_settings.mssp_races,           SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of player races",                                     true,  false, false },
    { "mssp_skills",          &game_settings.mssp_skills,          SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Number of player skills",                                    true,  false, false },
    { "mssp_dbsize",          &game_settings.mssp_dbsize,          SETTING_TYPE_INT,    SETTING_CAT_MSSP,    "Database size",                                              true,  false, false },
    { "mssp_ansi",            &game_settings.mssp_ansi,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "ANSI color code support",                                    true,  false, false },
    { "mssp_gmcp",            &game_settings.mssp_gmcp,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "GMCP support",                                               true,  false, false },
    { "mssp_mccp",            &game_settings.mssp_mccp,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "MCCP support",                                               true,  false, false },
    { "mssp_mcp",             &game_settings.mssp_mcp,             SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "MCP support",                                                true,  false, false },
    { "mssp_msdp",            &game_settings.mssp_msdp,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "MSDP support",                                               true,  false, false },
    { "mssp_msp",             &game_settings.mssp_msp,             SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "MSP support",                                                true,  false, false },
    { "mssp_mxp",             &game_settings.mssp_mxp,             SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "MXP support",                                                true,  false, false },
    { "mssp_pueb",            &game_settings.mssp_pueb,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Pueblo/UE support",                                          true,  false, false },
    { "mssp_utf8",            &game_settings.mssp_utf8,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "UTF-8 support",                                              true,  false, false },
    { "mssp_vt100",           &game_settings.mssp_vt100,           SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "VT100 support",                                              true,  false, false },
    { "mssp_xterm256",        &game_settings.mssp_xterm256,        SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "256 color support",                                          true,  false, false },
    { "mssp_xtermtrue",       &game_settings.mssp_xtermtrue,       SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "True color support",                                         true,  false, false },
    { "mssp_atcp",            &game_settings.mssp_atcp,            SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "ATCP support",                                               true,  false, false },
    { "mssp_ssl",             &game_settings.mssp_ssl,             SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "SSL support",                                                true,  false, false },
    { "mssp_pay2play",        &game_settings.mssp_pay2play,        SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Pay to play",                                                true,  false, false },
    { "mssp_pay4perks",       &game_settings.mssp_pay4perks,       SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Pay for perks",                                              true,  false, false },
    { "mssp_hiring_builders", &game_settings.mssp_hiring_builders, SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Looking for builders",                                       true,  false, false },
    { "mssp_hiring_coders",   &game_settings.mssp_hiring_coders,   SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Looking for coders",                                         true,  false, false },
    { "mssp_adult_material",  &game_settings.mssp_adult_material,  SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Contains mature content",                                    true,  false, false },
    { "mssp_multiclass",      &game_settings.mssp_multiclass,      SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Multiclassing allowed",                                      true,  false, false },
    { "mssp_newbie_friendly", &game_settings.mssp_newbie_friendly, SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Newbie friendly",                                            true,  false, false },
    { "mssp_player_cities",   &game_settings.mssp_player_cities,   SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Player cities",                                              true,  false, false },
    { "mssp_player_clans",    &game_settings.mssp_player_clans,    SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Player clans",                                               true,  false, false },
    { "mssp_player_crafting", &game_settings.mssp_player_crafting, SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Player crafting",                                            true,  false, false },
    { "mssp_player_guilds",   &game_settings.mssp_player_guilds,   SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Player guilds",                                              true,  false, false },
    { "mssp_equipment_system", &game_settings.mssp_equipment_system, SETTING_TYPE_STRING, SETTING_CAT_MSSP,  "Equipment system",                                           true,  false, false },
    { "mssp_multiplaying",    &game_settings.mssp_multiplaying,    SETTING_TYPE_STRING, SETTING_CAT_MSSP,    "Multiplaying allowed",                                       true,  false, false },
    { "mssp_playerkilling",   &game_settings.mssp_playerkilling,   SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Player killing allowed",                                     true,  false, false },
    { "mssp_quest_system",    &game_settings.mssp_quest_system,    SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Quest system",                                               true,  false, false },
    { "mssp_roleplaying",     &game_settings.mssp_roleplaying,     SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Roleplaying enforced",                                       true,  false, false },
    { "mssp_training_system", &game_settings.mssp_training_system, SETTING_TYPE_BOOL,   SETTING_CAT_MSSP,    "Training system",                                            true,  false, false },
    { "mssp_world_originality", &game_settings.mssp_world_originality, SETTING_TYPE_BOOL, SETTING_CAT_MSSP,  "Based on established setting",                               true,  false, false },

    /* Redis Settings */
    { "redis_enable",         &game_settings.enable_redis,         SETTING_TYPE_BOOL,   SETTING_CAT_REDIS,    "Enable Redis caching",                                       true,  false, false },
    { "redis_host",           &game_settings.redis_host,           SETTING_TYPE_STRING, SETTING_CAT_REDIS,    "Redis server hostname",                                      true,  false, false },
    { "redis_port",           &game_settings.redis_port,           SETTING_TYPE_INT,    SETTING_CAT_REDIS,    "Redis server port",                                          true,  false, false },
    { "redis_password",       &game_settings.redis_password,       SETTING_TYPE_STRING, SETTING_CAT_REDIS,    "Redis authentication password",                              true,  false, true },
    { "redis_timeout_sec",    &game_settings.redis_timeout_sec,    SETTING_TYPE_INT,    SETTING_CAT_REDIS,    "Redis connection timeout (seconds)",                         true,  false, false },
    { "redis_timeout_usec",   &game_settings.redis_timeout_usec,   SETTING_TYPE_INT,    SETTING_CAT_REDIS,    "Redis connection timeout (microseconds)",                    true,  false, false },

    /* Debug/Logging Settings */
    { "crash_dump_dir",       &game_settings.crash_dump_dir,       SETTING_TYPE_STRING, SETTING_CAT_DEBUG,    "Directory for crash dumps and core files",                   true,  false, false },

    { NULL, NULL, 0, 0, NULL, false, false }  /* Terminator */
};

/* Setting category names for display purposes */
/* NOTE: SETTING_CAT_* values start at 1, not 0, so index 0 is unused */
const char *setting_category_names[] = {
    NULL,        // index 0 - unused (SETTING_CAT values start at 1)
    "Core",      // SETTING_CAT_CORE = 1
    "Email",     // SETTING_CAT_EMAIL = 2
    "Missions",  // SETTING_CAT_MISSION = 3
    "Lockers",   // SETTING_CAT_LOCKER = 4
    "Vault",     // SETTING_CAT_VAULT = 5
    "Coffers",   // SETTING_CAT_COFFER = 6
    "Global",    // SETTING_CAT_GLOBAL = 7
    "Security",  // SETTING_CAT_SECURITY = 8
    "MSSP",      // SETTING_CAT_MSSP = 9
    "Redis",     // SETTING_CAT_REDIS = 10
    "Debug"      // SETTING_CAT_DEBUG = 11
};

/* Setting type names for display purposes */
const char *setting_type_names[] = {
    "Boolean",
    "Integer",
    "String",
    "ExtStr",
    "Float",
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

const struct flag_type church_log_category_flags[] =
{
    { "members",       CHLOG_MEMBERS,       true },
    { "deposit",       CHLOG_DEPOSIT,       true },
    { "withdrawal",    CHLOG_WITHDRAWAL,    true },
    { "transfer",      CHLOG_TRANSFER,      true }, // This is a meta-category
    { "pk",             CHLOG_PK,      true },
    { "storage",       CHLOG_STORAGE,       true },
    { "ranks",         CHLOG_RANKS,         true },
    { "permissions",   CHLOG_PERMISSIONS,   true },
    { "recruitment",   CHLOG_RECRUITMENT,   true },
    { "leadership",    CHLOG_LEADERSHIP,    false },
    { "general",       CHLOG_GENERAL,       true },
    { "storage_fees", CHLOG_STORAGE_FEES, false },
    { "finances",   CHLOG_FINANCES, false },
    { "gen_settings", CHLOG_GEN_SETTINGS, false},
    { "treasure", CHLOG_TREASURE, true },
    { "membership settings", CHLOG_MEMBERSHIP_SETTINGS, false },

    { NULL,            0,                   false }
};

// Meta-category mappings
const CHURCH_LOG_META_CATEGORY church_log_meta_categories[] = {
    { CHLOG_FINANCES,  CHLOG_DEPOSIT|CHLOG_WITHDRAWAL|CHLOG_TRANSFER|CHLOG_STORAGE_FEES },
    { CHLOG_LEADERSHIP, CHLOG_RANKS|CHLOG_PERMISSIONS },
    { CHLOG_SETTINGS, CHLOG_PK|CHLOG_GEN_SETTINGS },
    { CHLOG_MEMBERSHIP_SETTINGS, CHLOG_MEMBERS|CHLOG_RECRUITMENT|CHLOG_RANKS|CHLOG_PERMISSIONS },
    { 0, 0 }
};

const struct flag_type staff_ranks[] =
{
    {"player",      STAFF_PLAYER,            false},

    {"gimp",        STAFF_GIMP,             false},
    {"immortal",    STAFF_IMMORTAL,         true},
    {"ascendant",   STAFF_ASCENDANT,        true},
    {"supremacy",   STAFF_SUPREMACY,        true},
    {"creator",     STAFF_CREATOR,          true},
    {"implementor", STAFF_IMPLEMENTOR,      true},
    {NULL,          0,                      false}
};

const struct flag_type body_types[] =
{
    {"neutral", BODY_TYPE_NEUTRAL, true},
    {"masculine", BODY_TYPE_MALE, true},
    {"feminine", BODY_TYPE_FEMALE, true},
    {"other", BODY_TYPE_OTHER, true},
    {"random", BODY_TYPE_RANDOM, true},
    {NULL, 0, false}

};