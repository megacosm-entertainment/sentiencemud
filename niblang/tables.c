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

const struct flag_type affect_flags[] =
{
	{	"blind",			AFF_BLIND,			true	},
	{	"invisible",		AFF_INVISIBLE,		true	},
	{	"detect_evil",		AFF_DETECT_EVIL,	true	},
	{	"detect_invis",		AFF_DETECT_INVIS,	true	},
	{	"detect_magic",		AFF_DETECT_MAGIC,	true	},
	{	"detect_hidden",	AFF_DETECT_HIDDEN,	true	},
	{	"detect_good",		AFF_DETECT_GOOD,	true	},
	{	"sanctuary",		AFF_SANCTUARY,		true	},
	{	"faerie_fire",		AFF_FAERIE_FIRE,	true	},
	{	"infrared",			AFF_INFRARED,		true	},
	{	"curse",			AFF_CURSE,			true	},
	{   "death_grip",		AFF_DEATH_GRIP,		true    },
	{	"poison",			AFF_POISON,			true	},
	{	"sneak",			AFF_SNEAK,			true	},
	{	"hide",				AFF_HIDE,			true	},
	{	"sleep",			AFF_SLEEP,			true	},
	{	"charm",			AFF_CHARM,			true	},
	{	"flying",			AFF_FLYING,			true	},
	{	"pass_door",		AFF_PASS_DOOR,		true	},
	{	"haste",			AFF_HASTE,			true	},
	{	"calm",				AFF_CALM,			true	},
	{	"plague",			AFF_PLAGUE,			true	},
	{	"weaken",			AFF_WEAKEN,			true	},
	{	"frenzy",			AFF_FRENZY,			true	},
	{	"berserk",			AFF_BERSERK,		true	},
	{	"swim",				AFF_SWIM,			true	},
	{	"regeneration",		AFF_REGENERATION,	true	},
	{	"slow",				AFF_SLOW,			true	},
	{   "web",				AFF_WEB,			true	},
	{	NULL,				0,					false	}
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
