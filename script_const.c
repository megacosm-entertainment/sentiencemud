/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "tables.h"
#include "scripts.h"

char *opcode_names[OP_LASTCODE] = {
    "END",
    "IF",
    "ELSEIF",
    "ELSE",
    "ENDIF",
    "COMMAND",
    "GOTOLINE",
    "FOR",
    "ENDFOR",
    "EXITFOR",
    "LIST",
    "ENDLIST",
    "EXITLIST",
    "WHILE",
    "ENDWHILE",
    "EXITWHILE",
    "SWITCH",
    "ENDSWITCH",
    "EXITSWITCH",
    "MOB <command>",
    "OBJ <command>",
    "ROOM <command>",
    "TOKEN <command>",
    "TOKEN <command> (other)",
    "AREA <command>",
    "INSTANCE <command>",
    "DUNGEON <command>",
};

char *ifcheck_param_type_names[IFCP_MAX] = {
    "NONE",
    "NUMBER",
    "STRING",
    "MOBILE",
    "OBJECT",
    "ROOM",
    "TOKEN",
    "AREA",
    "EXIT"
};

ENT_FIELD entity_primary[] = {
    {"enactor",	ENTITY_ENACTOR,		ENT_MOBILE	},
    {"obj1",	ENTITY_OBJ1,		ENT_OBJECT	},
    {"obj2",	ENTITY_OBJ2,		ENT_OBJECT	},
    {"victim",	ENTITY_VICTIM,		ENT_MOBILE	},
    {"victim2",	ENTITY_VICTIM2,		ENT_MOBILE	},
    {"target",	ENTITY_TARGET,		ENT_MOBILE	},
    {"random",	ENTITY_RANDOM,		ENT_MOBILE	},
    {"here",	ENTITY_HERE,		ENT_ROOM	},
    {"this",	ENTITY_SELF,		ENT_UNKNOWN	},
    {"self",	ENTITY_SELF,		ENT_UNKNOWN	},
    {"phrase",	ENTITY_PHRASE,		ENT_STRING	},
    {"trigger",	ENTITY_TRIGGER,		ENT_STRING	},
    {"prior",	ENTITY_PRIOR,		ENT_PRIOR	},
    {"game",	ENTITY_GAME,		ENT_GAME	},
    {"null",	ENTITY_NULL,		ENT_NULL	},
    {"token",	ENTITY_TOKEN,		ENT_TOKEN	},
    {"register1",	ENTITY_REGISTER1,	ENT_NUMBER	},
    {"register2",	ENTITY_REGISTER2,	ENT_NUMBER	},
    {"register3",	ENTITY_REGISTER3,	ENT_NUMBER	},
    {"register4",	ENTITY_REGISTER4,	ENT_NUMBER	},
    {"register5",	ENTITY_REGISTER5,	ENT_NUMBER	},
    {"mxp",			ENTITY_MXP,		ENT_STRING		},
    {"tab",			ENTITY_MXP,		ENT_STRING		},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_types[] = {
    {"num",				ENTITY_VAR_NUM,			ENT_NUMBER		},
    {"str",				ENTITY_VAR_STR,			ENT_STRING		},
    {"mob",				ENTITY_VAR_MOB,			ENT_MOBILE		},
    {"obj",				ENTITY_VAR_OBJ,			ENT_OBJECT		},
    {"room",			ENTITY_VAR_ROOM,		ENT_ROOM		},
    {"exit",			ENTITY_VAR_EXIT,		ENT_EXIT		},
    {"token",			ENTITY_VAR_TOKEN,		ENT_TOKEN		},
    {"area",			ENTITY_VAR_AREA,		ENT_AREA		},
    {"skill",			ENTITY_VAR_SKILL,		ENT_SKILL		},
    {"skillinfo",		ENTITY_VAR_SKILLINFO,	ENT_SKILLINFO	},
    {"aff",				ENTITY_VAR_AFFECT,		ENT_AFFECT		},
    {"conn",			ENTITY_VAR_CONN,		ENT_CONN		},
    {"church",			ENTITY_VAR_CHURCH,		ENT_CHURCH		},
    {"var",				ENTITY_VAR_VARIABLE,	ENT_VARIABLE	},
    {"dynlist_exit",	ENTITY_VAR_BLLIST_EXIT,	ENT_BLLIST_EXIT	},
    {"dynlist_mob",		ENTITY_VAR_BLLIST_MOB,	ENT_BLLIST_MOB	},
    {"dynlist_obj",		ENTITY_VAR_BLLIST_OBJ,	ENT_BLLIST_OBJ	},
    {"dynlist_room",	ENTITY_VAR_BLLIST_ROOM,	ENT_BLLIST_ROOM	},
    {"dynlist_skill",	ENTITY_VAR_BLLIST_SKILL,	ENT_BLLIST_SKILL	},
    {"dynlist_token",	ENTITY_VAR_BLLIST_TOK,	ENT_BLLIST_TOK	},
    {"dynlist_area",	ENTITY_VAR_BLLIST_AREA,	ENT_BLLIST_AREA	},
    {"dynlist_wilds",	ENTITY_VAR_BLLIST_WILDS,	ENT_BLLIST_WILDS	},
    {"list_conn",		ENTITY_VAR_PLLIST_CONN,	ENT_PLLIST_CONN	},
    {"list_mob",		ENTITY_VAR_PLLIST_MOB,	ENT_PLLIST_MOB	},
    {"list_obj",		ENTITY_VAR_PLLIST_OBJ,	ENT_PLLIST_OBJ	},
    {"list_room",		ENTITY_VAR_PLLIST_ROOM,	ENT_PLLIST_ROOM	},
    {"list_str",		ENTITY_VAR_PLLIST_STR,	ENT_PLLIST_STR	},
    {"list_token",		ENTITY_VAR_PLLIST_TOK,	ENT_PLLIST_TOK	},
    {"list_church",		ENTITY_VAR_PLLIST_CHURCH,	ENT_PLLIST_CHURCH	},
    {"dice",			ENTITY_VAR_DICE,		ENT_DICE	},
    {"sect",			ENTITY_VAR_SECTION,		ENT_SECTION	},
    {"inst",			ENTITY_VAR_INSTANCE,	ENT_INSTANCE	},
    {"dung",			ENTITY_VAR_DUNGEON,		ENT_DUNGEON	},
    {"ship",			ENTITY_VAR_SHIP,		ENT_SHIP	},
    {NULL,				0,						ENT_UNKNOWN	}
};

ENT_FIELD entity_prior[] = {
    {"mob",		ENTITY_PRIOR_MOB,	ENT_MOBILE	},
    {"obj",		ENTITY_PRIOR_OBJ,	ENT_OBJECT	},
    {"room",	ENTITY_PRIOR_ROOM,	ENT_ROOM	},
    {"token",	ENTITY_PRIOR_TOKEN,	ENT_TOKEN	},
    {"enactor",	ENTITY_PRIOR_ENACTOR,	ENT_MOBILE	},
    {"obj1",	ENTITY_PRIOR_OBJ1,	ENT_OBJECT	},
    {"obj2",	ENTITY_PRIOR_OBJ2,	ENT_OBJECT	},
    {"victim",	ENTITY_PRIOR_VICTIM,	ENT_MOBILE	},
    {"target",	ENTITY_PRIOR_TARGET,	ENT_MOBILE	},
    {"random",	ENTITY_PRIOR_RANDOM,	ENT_MOBILE	},
    {"here",	ENTITY_PRIOR_HERE,	ENT_ROOM	},
    {"phrase",	ENTITY_PRIOR_PHRASE,	ENT_STRING	},
    {"trigger",	ENTITY_PRIOR_TRIGGER,	ENT_STRING	},
    {"register1",	ENTITY_PRIOR_REGISTER1,	ENT_NUMBER	},
    {"register2",	ENTITY_PRIOR_REGISTER2,	ENT_NUMBER	},
    {"register3",	ENTITY_PRIOR_REGISTER3,	ENT_NUMBER	},
    {"register4",	ENTITY_PRIOR_REGISTER4,	ENT_NUMBER	},
    {"register5",	ENTITY_PRIOR_REGISTER5,	ENT_NUMBER	},
    {"prior",	ENTITY_PRIOR_PRIOR,	ENT_PRIOR	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_game[] = {
    {"name",			ENTITY_GAME_NAME,				ENT_STRING	},
    {"port",			ENTITY_GAME_PORT,				ENT_NUMBER	},
    {"players",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN },
    {"mortals",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN },
    {"morts",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN },
    {"immortals",		ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN },
    {"imms",			ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN },
    {"staff",			ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN },
    {"online",			ENTITY_GAME_ONLINE,				ENT_PLLIST_CONN },
    {"areas",			ENTITY_GAME_AREAS,				ENT_BLLIST_AREA	},
    {"zones",			ENTITY_GAME_AREAS,				ENT_BLLIST_AREA	},
    {"wilds",			ENTITY_GAME_WILDS,				ENT_BLLIST_WILDS	},
    {"maps",			ENTITY_GAME_WILDS,				ENT_BLLIST_WILDS	},
    {"persist",			ENTITY_GAME_PERSIST,			ENT_PERSIST },
    {"churches",		ENTITY_GAME_CHURCHES,			ENT_PLLIST_CHURCH },
    {"ships",			ENTITY_GAME_SHIPS,				ENT_ILLIST_SHIPS },
    {"relicpower",		ENTITY_GAME_RELIC_POWER,		ENT_OBJECT	},
    {"damagerelic",		ENTITY_GAME_RELIC_POWER,		ENT_OBJECT	},
    {"relicknowledge",	ENTITY_GAME_RELIC_KNOWLEDGE,	ENT_OBJECT	},
    {"xprelic",			ENTITY_GAME_RELIC_KNOWLEDGE,	ENT_OBJECT	},
    {"reliclostsouls",	ENTITY_GAME_RELIC_LOSTSOULS,	ENT_OBJECT	},
    {"pneumarelic",		ENTITY_GAME_RELIC_LOSTSOULS,	ENT_OBJECT	},
    {"relichealth",		ENTITY_GAME_RELIC_HEALTH,		ENT_OBJECT	},
    {"hprelic",			ENTITY_GAME_RELIC_HEALTH,		ENT_OBJECT	},
    {"relicmagic",		ENTITY_GAME_RELIC_MAGIC,		ENT_OBJECT	},
    {"manarelic",		ENTITY_GAME_RELIC_MAGIC,		ENT_OBJECT	},
    
    {"reserved_mob", ENTITY_GAME_RESERVED_MOBILE, ENT_RESERVED_MOBILE },
{"reserved_obj",  ENTITY_GAME_RESERVED_OBJECT,   ENT_RESERVED_OBJECT },
{"reserved_room",  ENTITY_GAME_RESERVED_ROOM,    ENT_RESERVED_ROOM },
{"reserved_area",   ENTITY_GAME_RESERVED_AREA,    ENT_RESERVED_AREA },
{"reserved_token",  ENTITY_GAME_RESERVED_TOKEN,   ENT_RESERVED_TOKEN },
{"reserved_rprog",  ENTITY_GAME_RESERVED_RPROG,   ENT_RESERVED_RPROG },
{"reserved_oprog",  ENTITY_GAME_RESERVED_OPROG,   ENT_RESERVED_OPROG },
{"reserved_mprog",  ENTITY_GAME_RESERVED_MPROG,   ENT_RESERVED_MPROG },
{"reserved_tprog",  ENTITY_GAME_RESERVED_TPROG,   ENT_RESERVED_TPROG },
{"reserved_aprog",  ENTITY_GAME_RESERVED_APROG,   ENT_RESERVED_APROG },

{"setting",	ENTITY_GAME_SETTINGS,			ENT_GAME_SETTING	},
{"settings", ENTITY_GAME_SETTINGS,			ENT_GAME_SETTING },

    {"time_human",		ENTITY_GAME_TIME_HUMAN,			ENT_STRING	},

    {NULL,			0,							ENT_UNKNOWN	}
};

ENT_FIELD entity_persist[] = {
    {"mobiles",		ENTITY_PERSIST_MOBS,		ENT_PLLIST_MOB	},
    {"objects",		ENTITY_PERSIST_OBJS,		ENT_PLLIST_OBJ	},
    {"rooms",		ENTITY_PERSIST_ROOMS,		ENT_PLLIST_ROOM	},
    {NULL,			0,							ENT_UNKNOWN}
};

ENT_FIELD entity_boolean[] = {
    {"true",		ENTITY_BOOLEAN_TRUE_FALSE,	ENT_STRING},
    {"yes",			ENTITY_BOOLEAN_YES_NO,		ENT_STRING},
    {"on",			ENTITY_BOOLEAN_ON_OFF,		ENT_STRING},
    {NULL,			0,							ENT_UNKNOWN}
};

ENT_FIELD entity_number[] = {
    {"abs",		ENTITY_NUM_ABS,		ENT_NUMBER	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_string[] = {
    {"len",		ENTITY_STR_LEN,		ENT_NUMBER	},
    {"length",	ENTITY_STR_LEN,		ENT_NUMBER	},
    {"lower",	ENTITY_STR_LOWER,	ENT_STRING	},
    {"upper",	ENTITY_STR_UPPER,	ENT_STRING	},
    {"capital",	ENTITY_STR_CAPITAL,	ENT_STRING	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_mobile[] = {
    {"affects",			ENTITY_MOB_AFFECTS,			ENT_OLLIST_AFF	},
    {"area",			ENTITY_MOB_AREA,			ENT_AREA	},
    {"bedroll",			ENTITY_MOB_FURNITURE,		ENT_OBJECT	},
    {"bodytype_val",		ENTITY_MOB_BODY_TYPE_VALUE, ENT_NUMBER },
    {"carrying",		ENTITY_MOB_CARRYING,		ENT_OLLIST_OBJ	},
    {"cart",			ENTITY_MOB_CART,			ENT_OBJECT	},
    {"castspell",		ENTITY_MOB_CASTSPELL,		ENT_SKILL	},
    {"casttarget",		ENTITY_MOB_CASTTARGET,		ENT_STRING	},
    {"casttoken",		ENTITY_MOB_CASTTOKEN,		ENT_TOKEN	},
    {"checkpoint",		ENTITY_MOB_CHECKPOINT,		ENT_ROOM	},
    {"church",			ENTITY_MOB_CHURCH,			ENT_CHURCH	},
    {"class",			ENTITY_MOB_CLASS,			ENT_CLASS	},
    {"classlevel",		ENTITY_MOB_CLASSLEVEL,		ENT_CLASSLEVEL	},
    {"clonerooms",		ENTITY_MOB_CLONEROOMS,		ENT_BLLIST_ROOM	},
    {"connection",		ENTITY_MOB_CONNECTION,		ENT_CONN	},
    {"created",			ENTITY_MOB_CREATED,			ENT_NUMBER  },
    {"created_delta",	ENTITY_MOB_CREATED_DELTA,	ENT_NUMBER  },
    {"created_human",	ENTITY_MOB_CREATED_HUMAN,	ENT_STRING  },
    {"eq",				ENTITY_MOB_EQUIPMENT,		ENT_EQUIPMENT	},
    {"fulldesc",		ENTITY_MOB_FULLDESC,		ENT_STRING	},
    {"gender",			ENTITY_MOB_SEX,				ENT_STRING	},
    {"group",			ENTITY_MOB_GROUP,			ENT_GROUP },
    {"he",				ENTITY_MOB_HE,				ENT_STRING	},
    {"him",				ENTITY_MOB_HIM,				ENT_STRING	},
    {"himself",			ENTITY_MOB_HIMSELF,			ENT_STRING	},
    {"his",				ENTITY_MOB_HIS,				ENT_STRING	},
    {"hisobj",			ENTITY_MOB_HIS_O,			ENT_STRING	},
    {"home",			ENTITY_MOB_HOUSE,			ENT_ROOM	},
    {"house",			ENTITY_MOB_HOUSE,			ENT_ROOM	},
    {"hunting",			ENTITY_MOB_HUNTING,			ENT_MOBILE	},
    {"instrument",		ENTITY_MOB_INSTRUMENT,		ENT_OBJECT	},
    {"inv",				ENTITY_MOB_CARRYING,		ENT_OLLIST_OBJ	},
    {"last_login",		ENTITY_MOB_LASTLOGIN,		ENT_NUMBER	},
    {"last_login_delta", ENTITY_MOB_LASTLOGIN_DELTA, ENT_NUMBER	},
    {"last_login_human", ENTITY_MOB_LASTLOGIN_HUMAN,	ENT_STRING },
    {"last_logoff",		ENTITY_MOB_LASTLOGOFF,		ENT_NUMBER	},
    {"last_logoff_delta", ENTITY_MOB_LASTLOGOFF_DELTA, ENT_NUMBER	},
    {"last_logoff_human", ENTITY_MOB_LASTLOGOFF_HUMAN,	ENT_STRING},
    {"leader",			ENTITY_MOB_LEADER,			ENT_MOBILE	},
    {"level",			ENTITY_MOB_LEVEL,			ENT_NUMBER	},
    {"long",			ENTITY_MOB_LONG,			ENT_STRING	},
    {"master",			ENTITY_MOB_MASTER,			ENT_MOBILE	},
    {"mount",			ENTITY_MOB_MOUNT,			ENT_MOBILE	},
    {"name",			ENTITY_MOB_NAME,			ENT_STRING	},
    {"next",			ENTITY_MOB_NEXT,			ENT_MOBILE	},
    {"numgrouped",		ENTITY_MOB_NUMGROUPED,		ENT_NUMBER	},
    {"on",				ENTITY_MOB_FURNITURE,		ENT_OBJECT	},
    {"opponent",		ENTITY_MOB_OPPONENT,		ENT_MOBILE	},
    {"owner",			ENTITY_MOB_OWNER,			ENT_MOBILE	},
    {"played",			ENTITY_MOB_PLAYED,			ENT_NUMBER	},
    {"prey",			ENTITY_MOB_HUNTING,			ENT_MOBILE	},
    {"race",			ENTITY_MOB_RACE,			ENT_STRING	},
    {"racedata",		ENTITY_MOB_RACEDATA,		ENT_RACE	},
    {"originalrace",		ENTITY_MOB_ORIGINALRACE,		ENT_STRING	},
    {"originalracedata",	ENTITY_MOB_ORIGINALRACEDATA,	ENT_RACE	},
    {"recall",			ENTITY_MOB_RECALL,			ENT_ROOM	},
    {"rider",			ENTITY_MOB_RIDER,			ENT_MOBILE	},
    {"room",			ENTITY_MOB_ROOM,			ENT_ROOM	},
    {"session_time",	ENTITY_MOB_SESSIONTIME,		ENT_NUMBER  },
    {"sex",				ENTITY_MOB_SEX,				ENT_STRING	},
    {"short",			ENTITY_MOB_SHORT,			ENT_STRING	},
    {"song",			ENTITY_MOB_SONG,			ENT_SONG	},
    {"songtarget",		ENTITY_MOB_SONGTARGET,		ENT_STRING	},
    {"songtoken",		ENTITY_MOB_SONGTOKEN,		ENT_TOKEN	},
    {"target",			ENTITY_MOB_TARGET,			ENT_MOBILE	},
    {"tokens",			ENTITY_MOB_TOKENS,			ENT_OLLIST_TOK	},
    {"vars",			ENTITY_MOB_VARIABLES,		ENT_ILLIST_VARIABLE	},
    {"worn",			ENTITY_MOB_WORN,			ENT_PLLIST_OBJ },
    {"index",			ENTITY_MOB_INDEX,			ENT_MOBINDEX },
    {"act",				ENTITY_MOB_ACT,				ENT_BITMATRIX },
    {"affected",		ENTITY_MOB_AFFECT,			ENT_BITMATRIX },
    {"offense",			ENTITY_MOB_OFF,				ENT_BITVECTOR },
    {"immune",			ENTITY_MOB_IMMUNE,			ENT_BITVECTOR },
    {"resist",			ENTITY_MOB_RESIST,			ENT_BITVECTOR },
    {"verbpref_value",  ENTITY_MOB_VERB_PREF_VALUE,     ENT_NUMBER}, // Raw verb_preference enum value
    {"event_uid",       ENTITY_MOB_EVENT_SOURCE_UID,    ENT_NUMBER },
    {"event_source_uid", ENTITY_MOB_EVENT_SOURCE_UID,   ENT_NUMBER },
    {"event_instance",  ENTITY_MOB_EVENT_SOURCE_INSTANCE, ENT_NUMBER },
    {"event_source_instance", ENTITY_MOB_EVENT_SOURCE_INSTANCE, ENT_NUMBER },
    {"event_bracket",   ENTITY_MOB_EVENT_BRACKET,       ENT_NUMBER },
    {"event_active",    ENTITY_MOB_EVENT_ACTIVE,        ENT_NUMBER },
    {"event_kills",     ENTITY_MOB_EVENT_KILLS,         ENT_NUMBER },
    {"event_items",     ENTITY_MOB_EVENT_ITEMS,         ENT_NUMBER },
    {"event_goal",      ENTITY_MOB_EVENT_GOAL,          ENT_NUMBER },
    {"event_phase",     ENTITY_MOB_EVENT_PHASE,         ENT_STRING },

    {"vuln",			ENTITY_MOB_VULN,			ENT_BITVECTOR },

    {"tempstring",		ENTITY_MOB_TEMPSTRING,		ENT_STRING },

    {NULL,				0,							ENT_UNKNOWN	}
};

ENT_FIELD entity_object[] = {
    {"affects",		ENTITY_OBJ_AFFECTS,	ENT_OLLIST_AFF	},
    {"area",		ENTITY_OBJ_AREA,	ENT_AREA	},
    {"carrier",		ENTITY_OBJ_CARRIER,	ENT_MOBILE	},
    {"clonerooms",	ENTITY_OBJ_CLONEROOMS,	ENT_BLLIST_ROOM	},
    {"container",	ENTITY_OBJ_CONTAINER,	ENT_OBJECT	},
    {"contents",	ENTITY_OBJ_CONTENTS,	ENT_OLLIST_OBJ	},
    {"ed",			ENTITY_OBJ_EXTRADESC,	ENT_EXTRADESC	},
    {"fulldesc",		ENTITY_OBJ_FULLDESC,	ENT_STRING	},
    {"in",			ENTITY_OBJ_CONTAINER,	ENT_OBJECT	},
    {"inv",			ENTITY_OBJ_CONTENTS,	ENT_OLLIST_OBJ	},
    {"items",		ENTITY_OBJ_CONTENTS,	ENT_OLLIST_OBJ	},
    {"long",		ENTITY_OBJ_LONG,	ENT_STRING	},
    {"level",		ENTITY_OBJ_LEVEL,	ENT_NUMBER	},
    {"name",		ENTITY_OBJ_NAME,	ENT_STRING	},
    {"next",		ENTITY_OBJ_NEXT,	ENT_OBJECT	},
    {"on",			ENTITY_OBJ_FURNITURE,	ENT_OBJECT	},
    {"owner",		ENTITY_OBJ_OWNER,	ENT_STRING	},
    {"room",		ENTITY_OBJ_ROOM,	ENT_ROOM	},
    {"short",		ENTITY_OBJ_SHORT,	ENT_STRING	},
    {"target",		ENTITY_OBJ_TARGET,	ENT_MOBILE	},
    {"tokens",		ENTITY_OBJ_TOKENS,	ENT_OLLIST_TOK	},
    {"user",		ENTITY_OBJ_CARRIER,	ENT_MOBILE	},
    {"wearer",		ENTITY_OBJ_CARRIER,	ENT_MOBILE	},
    {"vars",		ENTITY_OBJ_VARIABLES,		ENT_ILLIST_VARIABLE	},
    {"index",		ENTITY_OBJ_INDEX,			ENT_OBJINDEX },
    {"extra",		ENTITY_OBJ_EXTRA,			ENT_BITMATRIX },
    {"wear",		ENTITY_OBJ_WEAR,			ENT_BITVECTOR },
    {"ship",		ENTITY_OBJ_SHIP,			ENT_SHIP		},

    {"event_uid",   ENTITY_OBJ_EVENT_SOURCE_UID, ENT_NUMBER },
    {"event_source_uid", ENTITY_OBJ_EVENT_SOURCE_UID, ENT_NUMBER },
    {"event_instance", ENTITY_OBJ_EVENT_SOURCE_INSTANCE, ENT_NUMBER },
    {"event_source_instance", ENTITY_OBJ_EVENT_SOURCE_INSTANCE, ENT_NUMBER },
    {"event_bracket", ENTITY_OBJ_EVENT_BRACKET, ENT_NUMBER },
    {"event_active", ENTITY_OBJ_EVENT_ACTIVE, ENT_NUMBER },
    {"event_kills", ENTITY_OBJ_EVENT_KILLS, ENT_NUMBER },
    {"event_items", ENTITY_OBJ_EVENT_ITEMS, ENT_NUMBER },
    {"event_goal", ENTITY_OBJ_EVENT_GOAL, ENT_NUMBER },
    {"event_phase", ENTITY_OBJ_EVENT_PHASE, ENT_STRING },

    // Typed data sub-entities
    {"armor",		ENTITY_OBJ_ARMOR_DATA,		ENT_OBJ_ARMOR		},
    {"armour",		ENTITY_OBJ_ARMOR_DATA,		ENT_OBJ_ARMOR		},
    {"body_part",	ENTITY_OBJ_BODY_PART_DATA,	ENT_OBJ_BODY_PART	},
    {"book",		ENTITY_OBJ_BOOK_DATA,		ENT_OBJ_BOOK		},
    {"cart",		ENTITY_OBJ_CART_DATA,		ENT_OBJ_CART		},
    {"compass",		ENTITY_OBJ_COMPASS_DATA,	ENT_OBJ_COMPASS		},
    {"container_data", ENTITY_OBJ_CONTAINER_DATA,	ENT_OBJ_CONTAINER	},
    {"corpse",		ENTITY_OBJ_CORPSE_DATA,		ENT_OBJ_CORPSE		},
    {"fluid",		ENTITY_OBJ_FLUID_CON_DATA,	ENT_OBJ_FLUID_CON	},
    {"drink",		ENTITY_OBJ_FLUID_CON_DATA,	ENT_OBJ_FLUID_CON	},
    {"food",		ENTITY_OBJ_FOOD_DATA,		ENT_OBJ_FOOD		},
    {"furniture",	ENTITY_OBJ_FURNITURE_DATA,	ENT_OBJ_FURNITURE	},
    {"herb",		ENTITY_OBJ_HERB_DATA,		ENT_OBJ_HERB		},
    {"ink",			ENTITY_OBJ_INK_DATA,		ENT_OBJ_INK			},
    {"instrument",	ENTITY_OBJ_INSTRUMENT_DATA,	ENT_OBJ_INSTRUMENT	},
    {"item_ship",	ENTITY_OBJ_ITEM_SHIP_DATA,	ENT_OBJ_ITEM_SHIP	},
    {"jewelry",		ENTITY_OBJ_JEWELRY_DATA,	ENT_OBJ_JEWELRY		},
    {"light",		ENTITY_OBJ_LIGHT_DATA,		ENT_OBJ_LIGHT		},
    {"map",			ENTITY_OBJ_MAP_DATA,		ENT_OBJ_MAP			},
    {"mist",		ENTITY_OBJ_MIST_DATA,		ENT_OBJ_MIST		},
    {"money",		ENTITY_OBJ_MONEY_DATA,		ENT_OBJ_MONEY		},
    {"page",		ENTITY_OBJ_PAGE_DATA,		ENT_OBJ_PAGE		},
    {"portal",		ENTITY_OBJ_PORTAL_DATA,		ENT_OBJ_PORTAL		},
    {"scroll",		ENTITY_OBJ_SCROLL_DATA,		ENT_OBJ_SCROLL		},
    {"seed",		ENTITY_OBJ_SEED_DATA,		ENT_OBJ_SEED		},
    {"sextant",		ENTITY_OBJ_SEXTANT_DATA,	ENT_OBJ_SEXTANT		},
    {"tattoo",		ENTITY_OBJ_TATTOO_DATA,		ENT_OBJ_TATTOO		},
    {"telescope",	ENTITY_OBJ_TELESCOPE_DATA,	ENT_OBJ_TELESCOPE	},
    {"tool",		ENTITY_OBJ_TOOL_DATA,		ENT_OBJ_TOOL		},
    {"trade",		ENTITY_OBJ_TRADE_DATA,		ENT_OBJ_TRADE		},
    {"wand",		ENTITY_OBJ_WAND_DATA,		ENT_OBJ_WAND		},
    {"staff",		ENTITY_OBJ_WAND_DATA,		ENT_OBJ_WAND		},
    {"weapon",		ENTITY_OBJ_WEAPON_DATA,		ENT_OBJ_WEAPON		},
    {"weapon_con",	ENTITY_OBJ_WEAPON_CON_DATA,	ENT_OBJ_WEAPON_CON	},
    {NULL,			0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_room[] = {
    {"area",		ENTITY_ROOM_AREA,			ENT_AREA	},
    {"clonerooms",	ENTITY_ROOM_CLONEROOMS,		ENT_BLLIST_ROOM	},
    {"clones",		ENTITY_ROOM_CLONES,			ENT_BLLIST_ROOM	},
    {"desc",		ENTITY_ROOM_DESC,			ENT_STRING	},
    {"down",		ENTITY_ROOM_DOWN,			ENT_EXIT	},
    {"east",		ENTITY_ROOM_EAST,			ENT_EXIT	},
    {"ed",			ENTITY_ROOM_EXTRADESC,		ENT_EXTRADESC	},
    {"env_mob",		ENTITY_ROOM_ENVIRON_MOB,	ENT_MOBILE	},
    {"env_obj",		ENTITY_ROOM_ENVIRON_OBJ,	ENT_OBJECT	},
    {"env_room",	ENTITY_ROOM_ENVIRON_ROOM,	ENT_ROOM	},
    {"env_token",	ENTITY_ROOM_ENVIRON_TOKEN,	ENT_TOKEN	},
    {"environ",		ENTITY_ROOM_ENVIRON,		ENT_ROOM	},
    {"environment",	ENTITY_ROOM_ENVIRON,		ENT_ROOM	},
    {"extern",		ENTITY_ROOM_ENVIRON,		ENT_ROOM	},
    {"mobiles",		ENTITY_ROOM_MOBILES,		ENT_OLLIST_MOB	},
    {"flags",		ENTITY_ROOM_FLAGS,			ENT_BITMATRIX },
    {"name",		ENTITY_ROOM_NAME,			ENT_STRING	},
    {"north",		ENTITY_ROOM_NORTH,			ENT_EXIT	},
    {"northeast",	ENTITY_ROOM_NORTHEAST,		ENT_EXIT	},
    {"northwest",	ENTITY_ROOM_NORTHWEST,		ENT_EXIT	},
    {"objects",		ENTITY_ROOM_OBJECTS,		ENT_OLLIST_OBJ	},
    {"outside",		ENTITY_ROOM_ENVIRON,		ENT_ROOM	},
    {"south",		ENTITY_ROOM_SOUTH,			ENT_EXIT	},
    {"southeast",	ENTITY_ROOM_SOUTHEAST,		ENT_EXIT	},
    {"southwest",	ENTITY_ROOM_SOUTHWEST,		ENT_EXIT	},
    {"target",		ENTITY_ROOM_TARGET,			ENT_MOBILE	},
    {"tokens",		ENTITY_ROOM_TOKENS,			ENT_OLLIST_TOK	},
    {"up",			ENTITY_ROOM_UP,				ENT_EXIT	},
    {"west",		ENTITY_ROOM_WEST,			ENT_EXIT	},
    {"wilds",		ENTITY_ROOM_WILDS,			ENT_WILDS	},
    {"vars",		ENTITY_ROOM_VARIABLES,		ENT_ILLIST_VARIABLE	},
    {"section",		ENTITY_ROOM_SECTION,		ENT_SECTION	},
    {"instance",	ENTITY_ROOM_INSTANCE,		ENT_INSTANCE	},
    {"dungeon",		ENTITY_ROOM_DUNGEON,		ENT_DUNGEON	},
    {"ship",		ENTITY_ROOM_SHIP,			ENT_SHIP	},
    {NULL,			0,							ENT_UNKNOWN	}
};

ENT_FIELD entity_exit[] = {
    {"name",	ENTITY_EXIT_NAME,	ENT_STRING	},
    {"door",	ENTITY_EXIT_DOOR,	ENT_NUMBER	},
    {"src",		ENTITY_EXIT_SOURCE,	ENT_ROOM	},
    {"here",	ENTITY_EXIT_SOURCE,	ENT_ROOM	},
    {"source",	ENTITY_EXIT_SOURCE,	ENT_ROOM	},
    {"dest",	ENTITY_EXIT_REMOTE,	ENT_ROOM	},
    {"remote",	ENTITY_EXIT_REMOTE,	ENT_ROOM	},
    {"destination",	ENTITY_EXIT_REMOTE,	ENT_ROOM	},
    {"state",	ENTITY_EXIT_STATE,	ENT_STRING	},
    {"mate",	ENTITY_EXIT_MATE,	ENT_EXIT	},
    {"north",	ENTITY_EXIT_NORTH,	ENT_EXIT	},
    {"east",	ENTITY_EXIT_EAST,	ENT_EXIT	},
    {"south",	ENTITY_EXIT_SOUTH,	ENT_EXIT	},
    {"west",	ENTITY_EXIT_WEST,	ENT_EXIT	},
    {"up",		ENTITY_EXIT_UP,		ENT_EXIT	},
    {"down",	ENTITY_EXIT_DOWN,	ENT_EXIT	},
    {"northeast",	ENTITY_EXIT_NORTHEAST,	ENT_EXIT	},
    {"northwest",	ENTITY_EXIT_NORTHWEST,	ENT_EXIT	},
    {"southeast",	ENTITY_EXIT_SOUTHEAST,	ENT_EXIT	},
    {"southwest",	ENTITY_EXIT_SOUTHWEST,	ENT_EXIT	},
    {"next",	ENTITY_EXIT_NEXT,	ENT_EXIT	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_token[] = {
    {"name",	ENTITY_TOKEN_NAME,	ENT_STRING	},
    {"owner",	ENTITY_TOKEN_OWNER,	ENT_MOBILE	},
    {"object",	ENTITY_TOKEN_OBJECT,	ENT_OBJECT	},
    {"room",	ENTITY_TOKEN_ROOM,	ENT_ROOM	},
    {"timer",	ENTITY_TOKEN_TIMER,	ENT_NUMBER	},
    {"val0",	ENTITY_TOKEN_VAL0,	ENT_NUMBER	},
    {"val1",	ENTITY_TOKEN_VAL1,	ENT_NUMBER	},
    {"val2",	ENTITY_TOKEN_VAL2,	ENT_NUMBER	},
    {"val3",	ENTITY_TOKEN_VAL3,	ENT_NUMBER	},
    {"val4",	ENTITY_TOKEN_VAL4,	ENT_NUMBER	},
    {"val5",	ENTITY_TOKEN_VAL5,	ENT_NUMBER	},
    {"val6",	ENTITY_TOKEN_VAL6,	ENT_NUMBER	},
    {"val7",	ENTITY_TOKEN_VAL7,	ENT_NUMBER	},
    {"next",	ENTITY_TOKEN_NEXT,	ENT_TOKEN	},
    {"vars",	ENTITY_TOKEN_VARIABLES,		ENT_ILLIST_VARIABLE	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_area[] = {
    {"name",	ENTITY_AREA_NAME,	ENT_STRING	},
    {"recall",	ENTITY_AREA_RECALL,	ENT_ROOM	},
    {"post",	ENTITY_AREA_POSTOFFICE,	ENT_ROOM	},
    {"lower",	ENTITY_AREA_LOWERVNUM,	ENT_NUMBER	},
    {"upper",	ENTITY_AREA_UPPERVNUM,	ENT_NUMBER	},
    {"minlevel", ENTITY_AREA_MINLEVEL,	ENT_NUMBER	},
    {"maxlevel", ENTITY_AREA_MAXLEVEL,	ENT_NUMBER	},
    {"rooms",	ENTITY_AREA_ROOMS,	ENT_PLLIST_ROOM	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_wilds[] = {
    {"name",	ENTITY_WILDS_NAME,		ENT_STRING },
    {"width",	ENTITY_WILDS_WIDTH,		ENT_NUMBER },
    {"height",	ENTITY_WILDS_HEIGHT,	ENT_NUMBER },
    {"vrooms",	ENTITY_WILDS_VROOMS,	ENT_PLLIST_ROOM },
    {NULL,		0,			ENT_UNKNOWN }
};

ENT_FIELD entity_conn[] = {
    {"player",		ENTITY_CONN_PLAYER,			ENT_MOBILE	},
    {"original",	ENTITY_CONN_ORIGINAL,		ENT_MOBILE	},
    {"host",		ENTITY_CONN_HOST,			ENT_STRING	},
    {"connection",	ENTITY_CONN_CONNECTION,		ENT_NUMBER	},
    {"snooper",		ENTITY_CONN_SNOOPER,		ENT_CONN	},
    {"client",		ENTITY_CONN_CLIENT,			ENT_STRING	},
    {"secure",		ENTITY_CONN_SECURE,			ENT_BOOLEAN },
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_church[] = {
    {"name",		ENTITY_CHURCH_NAME,			ENT_STRING	},
    {"size",		ENTITY_CHURCH_SIZE,			ENT_STRING	},
    {"flag",		ENTITY_CHURCH_FLAG,			ENT_STRING	},
    {"founder",		ENTITY_CHURCH_FOUNDER,		ENT_MOBILE	},
    {"founder_login",	ENTITY_CHURCH_FOUNDER_LOGIN,	ENT_NUMBER	},
    {"founder_login_human",	ENTITY_CHURCH_FOUNDER_LOGIN_HUMAN,	ENT_STRING	},
    {"founder_name",ENTITY_CHURCH_FOUNDER_NAME,	ENT_STRING	},
    {"motd",		ENTITY_CHURCH_MOTD,			ENT_STRING	},
    {"rules",		ENTITY_CHURCH_RULES,		ENT_STRING	},
    {"info",		ENTITY_CHURCH_INFO,			ENT_STRING	},
    {"recall",		ENTITY_CHURCH_RECALL,		ENT_ROOM	},
    {"treasure",	ENTITY_CHURCH_TREASURE,		ENT_PLLIST_ROOM	},
    {"key",			ENTITY_CHURCH_KEY,			ENT_NUMBER	},
    {"online",		ENTITY_CHURCH_ONLINE,		ENT_PLLIST_CONN	},
    {"roster",		ENTITY_CHURCH_ROSTER,		ENT_PLLIST_STR	},
    {NULL,			0,							ENT_UNKNOWN	}
};

ENT_FIELD entity_list[] = {
    {"count",	ENTITY_LIST_SIZE,	ENT_NUMBER	},
    {"size",	ENTITY_LIST_SIZE,	ENT_NUMBER	},
    {"len",		ENTITY_LIST_SIZE,	ENT_NUMBER	},
    {"length",	ENTITY_LIST_SIZE,	ENT_NUMBER	},
    {"random",	ENTITY_LIST_RANDOM,	ENT_UNKNOWN	},
    {"first",	ENTITY_LIST_FIRST,	ENT_UNKNOWN	},
    {"last",	ENTITY_LIST_LAST,	ENT_UNKNOWN	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_skill[] = {
    {"gsn",       ENTITY_SKILL_GSN,       ENT_NUMBER  },
    {"spell",     ENTITY_SKILL_SPELL,     ENT_NUMBER  },
    {"name",      ENTITY_SKILL_NAME,      ENT_STRING  },
    {"display",   ENTITY_SKILL_DISPLAY,   ENT_STRING  },
    {"description", ENTITY_SKILL_DESCRIPTION, ENT_STRING },
    {"summary",   ENTITY_SKILL_SUMMARY,   ENT_STRING  },
    {"comments",  ENTITY_SKILL_COMMENTS,  ENT_STRING  },
    {"uid",       ENTITY_SKILL_UID,       ENT_NUMBER  },
    {"isspell",   ENTITY_SKILL_ISSPELL,   ENT_BOOLEAN },
    {"flags",     ENTITY_SKILL_FLAGS,     ENT_BITVECTOR },
    {"difficulty", ENTITY_SKILL_DIFFICULTY, ENT_NUMBER },
    {"beats",     ENTITY_SKILL_BEATS,     ENT_NUMBER  },
    {"timer",     ENTITY_SKILL_BEATS,     ENT_NUMBER  },
    {"target",    ENTITY_SKILL_TARGET,    ENT_NUMBER  },
    {"position",  ENTITY_SKILL_POSITION,  ENT_NUMBER  },
    {"wearoff",   ENTITY_SKILL_WEAROFF,   ENT_STRING  },
    {"object",    ENTITY_SKILL_OBJECT,    ENT_STRING  },
    {"dispel",    ENTITY_SKILL_DISPEL,    ENT_STRING  },
    {"noun",      ENTITY_SKILL_NOUN,      ENT_STRING  },
    {"mana",      ENTITY_SKILL_MANA,      ENT_NUMBER  },
    {"race",      ENTITY_SKILL_RACE,      ENT_RACE    },
    {"inktype1",  ENTITY_SKILL_INK_TYPE1, ENT_NUMBER  },
    {"inksize1",  ENTITY_SKILL_INK_SIZE1, ENT_NUMBER  },
    {"inktype2",  ENTITY_SKILL_INK_TYPE2, ENT_NUMBER  },
    {"inksize2",  ENTITY_SKILL_INK_SIZE2, ENT_NUMBER  },
    {"inktype3",  ENTITY_SKILL_INK_TYPE3, ENT_NUMBER  },
    {"inksize3",  ENTITY_SKILL_INK_SIZE3, ENT_NUMBER  },
    /* Legacy class-specific fields (deprecated) */
    {"levelwarrior",  ENTITY_SKILL_LEVEL_WARRIOR,      ENT_NUMBER },
    {"levelcleric",   ENTITY_SKILL_LEVEL_CLERIC,       ENT_NUMBER },
    {"levelmage",     ENTITY_SKILL_LEVEL_MAGE,         ENT_NUMBER },
    {"levelthief",    ENTITY_SKILL_LEVEL_THIEF,        ENT_NUMBER },
    {"diffwarrior",   ENTITY_SKILL_DIFFICULTY_WARRIOR,  ENT_NUMBER },
    {"diffcleric",    ENTITY_SKILL_DIFFICULTY_CLERIC,   ENT_NUMBER },
    {"diffmage",      ENTITY_SKILL_DIFFICULTY_MAGE,     ENT_NUMBER },
    {"diffthief",     ENTITY_SKILL_DIFFICULTY_THIEF,    ENT_NUMBER },
    {NULL,        0,              ENT_UNKNOWN }
};

ENT_FIELD entity_skill_info[] = {
    {"skill",	ENTITY_SKILLINFO_SKILL,		ENT_SKILL	},
    {"owner",	ENTITY_SKILLINFO_OWNER,		ENT_MOBILE	},
    {"token",	ENTITY_SKILLINFO_TOKEN,		ENT_TOKEN	},
    {"rating",	ENTITY_SKILLINFO_RATING,	ENT_NUMBER	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_affect[] = {
    {"name",	ENTITY_AFFECT_NAME,	ENT_STRING	},
    {"group",	ENTITY_AFFECT_GROUP,	ENT_NUMBER	},
    {"skill",	ENTITY_AFFECT_SKILL,	ENT_NUMBER	},
    {"location",	ENTITY_AFFECT_LOCATION,	ENT_NUMBER	},
    {"mod",		ENTITY_AFFECT_MOD,	ENT_NUMBER	},
    {"duration",	ENTITY_AFFECT_TIMER,	ENT_NUMBER	},
    {"timer",	ENTITY_AFFECT_TIMER,	ENT_NUMBER	},
    {"level",	ENTITY_AFFECT_LEVEL,	ENT_NUMBER	},
    {NULL,		0,			ENT_UNKNOWN	}
};

ENT_FIELD entity_song[] = {
    {"number",  ENTITY_SONG_NUMBER, ENT_NUMBER  },
    {"name",    ENTITY_SONG_NAME,   ENT_STRING  },
    {"uid",     ENTITY_SONG_UID,    ENT_NUMBER  },
    {"flags",   ENTITY_SONG_FLAGS,  ENT_BITVECTOR },
    {"spell1",  ENTITY_SONG_SPELL1, ENT_SKILL   },
    {"spell2",  ENTITY_SONG_SPELL2, ENT_SKILL   },
    {"spell3",  ENTITY_SONG_SPELL3, ENT_SKILL   },
    {"target",  ENTITY_SONG_TARGET, ENT_NUMBER  },
    {"beats",   ENTITY_SONG_BEATS,  ENT_NUMBER  },
    {"mana",    ENTITY_SONG_MANA,   ENT_NUMBER  },
    {"level",   ENTITY_SONG_LEVEL,  ENT_NUMBER  },
    {NULL,      0,          ENT_UNKNOWN }
};

ENT_FIELD entity_race[] = {
    {"name",          ENTITY_RACE_NAME,         ENT_STRING    },
    {"description",   ENTITY_RACE_DESCRIPTION,  ENT_STRING    },
    {"comments",      ENTITY_RACE_COMMENTS,     ENT_STRING    },
    {"uid",           ENTITY_RACE_UID,          ENT_NUMBER    },
    {"id",            ENTITY_RACE_ID,           ENT_STRING    },
    {"playable",      ENTITY_RACE_PLAYABLE,     ENT_BOOLEAN   },
    {"starting",      ENTITY_RACE_STARTING,     ENT_BOOLEAN   },
    {"act",           ENTITY_RACE_ACT,          ENT_BITMATRIX },
    {"affects",       ENTITY_RACE_AFFECTS,      ENT_BITMATRIX },
    {"offense",       ENTITY_RACE_OFFENSE,      ENT_BITVECTOR },
    {"immune",        ENTITY_RACE_IMMUNE,       ENT_BITVECTOR },
    {"resist",        ENTITY_RACE_RESIST,       ENT_BITVECTOR },
    {"vuln",          ENTITY_RACE_VULN,         ENT_BITVECTOR },
    {"form",          ENTITY_RACE_FORM,         ENT_BITVECTOR },
    {"parts",         ENTITY_RACE_PARTS,        ENT_BITVECTOR },
    {"who",           ENTITY_RACE_WHO,          ENT_STRING    },
    {"minsize",       ENTITY_RACE_SIZE_MIN,     ENT_NUMBER    },
    {"maxsize",       ENTITY_RACE_SIZE_MAX,     ENT_NUMBER    },
    {"alignment",     ENTITY_RACE_ALIGNMENT,    ENT_NUMBER    },
    {NULL,            0,                        ENT_UNKNOWN   }
};

ENT_FIELD entity_class[] = {
    {"name",          ENTITY_CLASS_NAME,         ENT_STRING    },
    {"description",   ENTITY_CLASS_DESCRIPTION,  ENT_STRING    },
    {"comments",      ENTITY_CLASS_COMMENTS,     ENT_STRING    },
    {"uid",           ENTITY_CLASS_UID,          ENT_NUMBER    },
    {"type",          ENTITY_CLASS_TYPE,         ENT_NUMBER    },
    {"flags",         ENTITY_CLASS_FLAGS,        ENT_BITVECTOR },
    {"stat",          ENTITY_CLASS_PRIMARY_STAT, ENT_NUMBER    },
    {"maxlevel",      ENTITY_CLASS_MAX_LEVEL,    ENT_NUMBER    },
    {"hpmin",         ENTITY_CLASS_HP_MIN,       ENT_NUMBER    },
    {"hpmax",         ENTITY_CLASS_HP_MAX,       ENT_NUMBER    },
    {"gainsmana",     ENTITY_CLASS_GAINS_MANA,   ENT_BOOLEAN   },
    {NULL,            0,                         ENT_UNKNOWN   }
};

ENT_FIELD entity_classlevel[] = {
    {"class",   ENTITY_CLASSLEVEL_CLASS,   ENT_CLASS   },
    {"level",   ENTITY_CLASSLEVEL_LEVEL,   ENT_NUMBER  },
    {"xp",      ENTITY_CLASSLEVEL_XP,      ENT_NUMBER  },
    {"title",   ENTITY_CLASSLEVEL_TITLE,   ENT_STRING  },
    {NULL,      0,                         ENT_UNKNOWN }
};

ENT_FIELD entity_skillentry[] = {
    {"skill",    ENTITY_SKILLENTRY_SKILL,    ENT_SKILL    },
    {"song",     ENTITY_SKILLENTRY_SONG,     ENT_SONG     },
    {"rating",   ENTITY_SKILLENTRY_RATING,   ENT_NUMBER   },
    {"mod",      ENTITY_SKILLENTRY_MOD,      ENT_NUMBER   },
    {"isspell",  ENTITY_SKILLENTRY_ISSPELL,  ENT_BOOLEAN  },
    {"source",   ENTITY_SKILLENTRY_SOURCE,   ENT_NUMBER   },
    {"flags",    ENTITY_SKILLENTRY_FLAGS,    ENT_BITVECTOR },
    {"token",    ENTITY_SKILLENTRY_TOKEN,    ENT_TOKEN    },
    {NULL,       0,                          ENT_UNKNOWN  }
};

ENT_FIELD entity_variable[] = {
    {"name",	ENTITY_VARIABLE_NAME,	ENT_STRING	},
    {"type",	ENTITY_VARIABLE_TYPE,	ENT_STRING	},
    {"save",	ENTITY_VARIABLE_SAVE,	ENT_NUMBER	},
    {NULL,		0,			ENT_UNKNOWN	}

};

ENT_FIELD entity_group[] = {
    {"owner",			ENTITY_GROUP_OWNER,				ENT_MOBILE	},
    {"leader",			ENTITY_GROUP_LEADER,			ENT_MOBILE	},
    {"ally",			ENTITY_GROUP_ALLY,				ENT_MOBILE	},
    {"member",			ENTITY_GROUP_MEMBER,			ENT_MOBILE		},
    {"members",			ENTITY_GROUP_MEMBERS,			ENT_ILLIST_MOB_GROUP	},
    {"size",			ENTITY_GROUP_SIZE,				ENT_NUMBER	},
    {NULL,		0,						ENT_UNKNOWN	}

};

ENT_FIELD entity_dice[] = {
    {"number",		ENTITY_DICE_NUMBER,	ENT_NUMBER },
    {"size",		ENTITY_DICE_SIZE,	ENT_NUMBER },
    {"bonus",		ENTITY_DICE_BONUS,	ENT_NUMBER },
    {"roll",		ENTITY_DICE_ROLL,	ENT_NUMBER },
    {"last",		ENTITY_DICE_LAST,	ENT_NUMBER },
    {NULL,		0,						ENT_UNKNOWN	}

};

ENT_FIELD entity_mobindex[] = {
    {"vnum",                    ENTITY_MOBINDEX_VNUM,                   ENT_NUMBER },
    {"wnum",                    ENTITY_MOBINDEX_WNUM,                   ENT_WIDEVNUM },
    {"level",                   ENTITY_MOBINDEX_LEVEL,                  ENT_NUMBER },
    {"loaded",                  ENTITY_MOBINDEX_LOADED,                 ENT_NUMBER },
    {NULL,                       0,                                       ENT_UNKNOWN },
};

ENT_FIELD entity_objindex[] = {
    {"vnum",                    ENTITY_OBJINDEX_VNUM,                   ENT_NUMBER },
    {"wnum",                    ENTITY_OBJINDEX_WNUM,                   ENT_WIDEVNUM },
    {"level",                   ENTITY_OBJINDEX_LEVEL,                  ENT_NUMBER },
    {"loaded",                  ENTITY_OBJINDEX_LOADED,                 ENT_NUMBER },
    {"inrooms",                 ENTITY_OBJINDEX_INROOMS,                ENT_NUMBER },
    {"inmail",                  ENTITY_OBJINDEX_INMAIL,                 ENT_NUMBER },
    {"carried",                 ENTITY_OBJINDEX_CARRIED,                ENT_NUMBER },
    {"lockered",                ENTITY_OBJINDEX_LOCKERED,               ENT_NUMBER },
    {"incontainer",             ENTITY_OBJINDEX_INCONTAINER,            ENT_NUMBER },
    {NULL,                       0,                                       ENT_UNKNOWN },
};

ENT_FIELD entity_instance_section[] = {
    {"rooms",			ENTITY_SECTION_ROOMS,			ENT_PLLIST_ROOM	},
    {"instance",		ENTITY_SECTION_INSTANCE,		ENT_INSTANCE },
    {"map",				ENTITY_SECTION_MAP,				ENT_STRING },
    {"mapobj",			ENTITY_SECTION_MAP_OBJ,			ENT_OBJECT },
    {"mapobject",		ENTITY_SECTION_MAP_OBJ,			ENT_OBJECT },
    {"map_obj",		ENTITY_SECTION_MAP_OBJ,			ENT_OBJECT },
    {"mapobjindex",	ENTITY_SECTION_MAP_OBJ_INDEX,	ENT_OBJINDEX },
    {"map_objindex",	ENTITY_SECTION_MAP_OBJ_INDEX,	ENT_OBJINDEX },
    {"mapobjectindex", ENTITY_SECTION_MAP_OBJ_INDEX,	ENT_OBJINDEX },
    {"mapmob",			ENTITY_SECTION_MAP_MOB,			ENT_MOBILE },
    {"mapmobile",		ENTITY_SECTION_MAP_MOB,			ENT_MOBILE },
    {"map_mob",		ENTITY_SECTION_MAP_MOB,			ENT_MOBILE },
    {"mapmobindex",	ENTITY_SECTION_MAP_MOB_INDEX,	ENT_MOBINDEX },
    {"map_mobindex",	ENTITY_SECTION_MAP_MOB_INDEX,	ENT_MOBINDEX },
    {"mapmobileindex", ENTITY_SECTION_MAP_MOB_INDEX,	ENT_MOBINDEX },
    {NULL,				0,								ENT_UNKNOWN	}
};


ENT_FIELD entity_instance[] = {
    {"name",			ENTITY_INSTANCE_NAME,			ENT_STRING			},
    {"sections",		ENTITY_INSTANCE_SECTIONS,		ENT_ILLIST_SECTIONS	},
    {"owners",			ENTITY_INSTANCE_OWNERS,			ENT_BLLIST_MOB		},
    {"object",			ENTITY_INSTANCE_OBJECT,			ENT_OBJECT			},
    {"dungeon",			ENTITY_INSTANCE_DUNGEON,		ENT_DUNGEON			},
//	{"quest",			ENTITY_INSTANCE_QUEST,			ENT_QUEST			},
    {"ship",			ENTITY_INSTANCE_SHIP,			ENT_SHIP			},
    {"floor",			ENTITY_INSTANCE_FLOOR,			ENT_NUMBER			},
    {"entry",			ENTITY_INSTANCE_ENTRY,			ENT_ROOM			},
    {"exit",			ENTITY_INSTANCE_EXIT,			ENT_ROOM			},
    {"recall",			ENTITY_INSTANCE_RECALL,			ENT_ROOM			},
    {"environ",			ENTITY_INSTANCE_ENVIRON,		ENT_ROOM			},
    {"rooms",			ENTITY_INSTANCE_ROOMS,			ENT_PLLIST_ROOM		},
    {"players",			ENTITY_INSTANCE_PLAYERS,		ENT_PLLIST_MOB		},
    {"mobiles",			ENTITY_INSTANCE_MOBILES,		ENT_PLLIST_MOB		},
    {"objects",			ENTITY_INSTANCE_OBJECTS,		ENT_PLLIST_OBJ		},
    {"bosses",			ENTITY_INSTANCE_BOSSES,			ENT_PLLIST_MOB		},
    {"specialrooms",	ENTITY_INSTANCE_SPECIAL_ROOMS,	ENT_ILLIST_SPECIALROOMS		},
    {NULL,				0,								ENT_UNKNOWN			}
};

ENT_FIELD entity_dungeon[] = {
    {"name",			ENTITY_DUNGEON_NAME,			ENT_STRING			},
    {"floors",			ENTITY_DUNGEON_FLOORS,			ENT_ILLIST_INSTANCES},
    {"desc",			ENTITY_DUNGEON_DESC,			ENT_STRING			},
    {"owners",			ENTITY_DUNGEON_OWNERS,			ENT_BLLIST_MOB		},
    {"entry",			ENTITY_DUNGEON_ENTRY,			ENT_ROOM			},
    {"exit",			ENTITY_DUNGEON_EXIT,			ENT_ROOM			},
    {"rooms",			ENTITY_DUNGEON_ROOMS,			ENT_PLLIST_ROOM		},
    {"players",			ENTITY_DUNGEON_PLAYERS,			ENT_PLLIST_MOB		},
    {"mobiles",			ENTITY_DUNGEON_MOBILES,			ENT_PLLIST_MOB		},
    {"objects",			ENTITY_DUNGEON_OBJECTS,			ENT_PLLIST_OBJ		},
    {"bosses",			ENTITY_DUNGEON_BOSSES,			ENT_PLLIST_MOB		},
    {"specialrooms",	ENTITY_DUNGEON_SPECIAL_ROOMS,	ENT_ILLIST_SPECIALROOMS		},
    {NULL,				0,								ENT_UNKNOWN			}
};

ENT_FIELD entity_ship[] = {
    {"name",			ENTITY_SHIP_NAME,				ENT_STRING			},
    {"object",			ENTITY_SHIP_OBJECT,				ENT_OBJECT			},
    {NULL,				0,								ENT_UNKNOWN			}
};

ENT_FIELD entity_quest_part[] = {
    {NULL,				0,								ENT_UNKNOWN			}
};

ENT_FIELD entity_quest[] = {
    {NULL,				0,								ENT_UNKNOWN			}
};

///////////////////////////////////////////////////////////////////////////////
// Object typed data sub-entity field tables
///////////////////////////////////////////////////////////////////////////////

ENT_FIELD entity_obj_weapon[] = {
    {"class",			ENTITY_OBJ_WEAPON_CLASS,		ENT_NUMBER	},
    {"weapon_class",	ENTITY_OBJ_WEAPON_CLASS,		ENT_NUMBER	},
    {"damage_type",		ENTITY_OBJ_WEAPON_DAMAGE_TYPE,	ENT_NUMBER	},
    {"flags",			ENTITY_OBJ_WEAPON_FLAGS,		ENT_BITVECTOR	},
    {"dice",			ENTITY_OBJ_WEAPON_DICE,			ENT_DICE	},
    {"damage",			ENTITY_OBJ_WEAPON_DICE,			ENT_DICE	},
    {"range",			ENTITY_OBJ_WEAPON_RANGE,		ENT_NUMBER	},
    {"max_mana",		ENTITY_OBJ_WEAPON_MAX_MANA,		ENT_NUMBER	},
    {"charges",			ENTITY_OBJ_WEAPON_CHARGES,		ENT_NUMBER	},
    {"max_charges",		ENTITY_OBJ_WEAPON_MAX_CHARGES,	ENT_NUMBER	},
    {"cooldown",		ENTITY_OBJ_WEAPON_COOLDOWN,		ENT_NUMBER	},
    {"recharge_time",	ENTITY_OBJ_WEAPON_RECHARGE_TIME, ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_armor[] = {
    {"type",			ENTITY_OBJ_ARMOR_TYPE,			ENT_NUMBER	},
    {"armor_type",		ENTITY_OBJ_ARMOR_TYPE,			ENT_NUMBER	},
    {"strength",		ENTITY_OBJ_ARMOR_STRENGTH,		ENT_NUMBER	},
    {"armor_strength",	ENTITY_OBJ_ARMOR_STRENGTH,		ENT_NUMBER	},
    {"pierce",			ENTITY_OBJ_ARMOR_PROT_PIERCE,	ENT_NUMBER	},
    {"bash",			ENTITY_OBJ_ARMOR_PROT_BASH,		ENT_NUMBER	},
    {"slash",			ENTITY_OBJ_ARMOR_PROT_SLASH,	ENT_NUMBER	},
    {"exotic",			ENTITY_OBJ_ARMOR_PROT_EXOTIC,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_container[] = {
    {"flags",			ENTITY_OBJ_CONTAINER_FLAGS,		ENT_BITVECTOR	},
    {"max_weight",		ENTITY_OBJ_CONTAINER_MAX_WEIGHT, ENT_NUMBER	},
    {"weight_mult",		ENTITY_OBJ_CONTAINER_WEIGHT_MULT, ENT_NUMBER},
    {"max_items",		ENTITY_OBJ_CONTAINER_MAX_ITEMS,	ENT_NUMBER	},
    {"max_volume",		ENTITY_OBJ_CONTAINER_MAX_VOLUME, ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_fluid_con[] = {
    {"capacity",		ENTITY_OBJ_FLUID_CON_CAPACITY,	ENT_NUMBER	},
    {"amount",			ENTITY_OBJ_FLUID_CON_AMOUNT,	ENT_NUMBER	},
    {"liquid",			ENTITY_OBJ_FLUID_CON_LIQUID,	ENT_NUMBER	},
    {"poison",			ENTITY_OBJ_FLUID_CON_POISON,	ENT_NUMBER	},
    {"flags",			ENTITY_OBJ_FLUID_CON_FLAGS,		ENT_BITVECTOR	},
    {"refill_rate",		ENTITY_OBJ_FLUID_CON_REFILL_RATE, ENT_NUMBER},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_food[] = {
    {"hunger",			ENTITY_OBJ_FOOD_HUNGER,			ENT_NUMBER	},
    {"full",			ENTITY_OBJ_FOOD_FULL,			ENT_NUMBER	},
    {"poison",			ENTITY_OBJ_FOOD_POISON,			ENT_NUMBER	},
    {"timer",			ENTITY_OBJ_FOOD_TIMER,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_furniture[] = {
    {"flags",			ENTITY_OBJ_FURNITURE_FLAGS,		ENT_BITVECTOR	},
    {"max_people",		ENTITY_OBJ_FURNITURE_MAX_PEOPLE, ENT_NUMBER	},
    {"max_weight",		ENTITY_OBJ_FURNITURE_MAX_WEIGHT, ENT_NUMBER	},
    {"heal_rate",		ENTITY_OBJ_FURNITURE_HEAL_RATE,	ENT_NUMBER	},
    {"mana_rate",		ENTITY_OBJ_FURNITURE_MANA_RATE,	ENT_NUMBER	},
    {"move_rate",		ENTITY_OBJ_FURNITURE_MOVE_RATE,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_portal[] = {
    {"exit",			ENTITY_OBJ_PORTAL_EXIT,			ENT_BITVECTOR	},
    {"flags",			ENTITY_OBJ_PORTAL_FLAGS,		ENT_BITVECTOR	},
    {"charges",			ENTITY_OBJ_PORTAL_CHARGES,		ENT_NUMBER	},
    {"type",			ENTITY_OBJ_PORTAL_TYPE,			ENT_NUMBER	},
    {"destination",		ENTITY_OBJ_PORTAL_DESTINATION,	ENT_NUMBER	},
    {"dest",			ENTITY_OBJ_PORTAL_DESTINATION,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_light[] = {
    {"flags",			ENTITY_OBJ_LIGHT_FLAGS,			ENT_BITVECTOR	},
    {"duration",		ENTITY_OBJ_LIGHT_DURATION,		ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_money[] = {
    {"silver",			ENTITY_OBJ_MONEY_SILVER,		ENT_NUMBER	},
    {"gold",			ENTITY_OBJ_MONEY_GOLD,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_wand[] = {
    {"max_mana",		ENTITY_OBJ_WAND_MAX_MANA,		ENT_NUMBER	},
    {"charges",			ENTITY_OBJ_WAND_CHARGES,		ENT_NUMBER	},
    {"max_charges",		ENTITY_OBJ_WAND_MAX_CHARGES,	ENT_NUMBER	},
    {"cooldown",		ENTITY_OBJ_WAND_COOLDOWN,		ENT_NUMBER	},
    {"recharge_time",	ENTITY_OBJ_WAND_RECHARGE_TIME,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_corpse[] = {
    {"corpse_type",		ENTITY_OBJ_CORPSE_CORPSE_TYPE,	ENT_NUMBER	},
    {"resurrection",	ENTITY_OBJ_CORPSE_RESURRECTION,	ENT_NUMBER	},
    {"animation",		ENTITY_OBJ_CORPSE_ANIMATION,	ENT_NUMBER	},
    {"body_parts",		ENTITY_OBJ_CORPSE_BODY_PARTS,	ENT_NUMBER	},
    {"mobile_vnum",		ENTITY_OBJ_CORPSE_MOBILE_VNUM,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_instrument[] = {
    {"type",			ENTITY_OBJ_INSTRUMENT_TYPE,		ENT_NUMBER	},
    {"flags",			ENTITY_OBJ_INSTRUMENT_FLAGS,	ENT_BITVECTOR	},
    {"beats_min",		ENTITY_OBJ_INSTRUMENT_BEATS_MIN, ENT_NUMBER	},
    {"beats_max",		ENTITY_OBJ_INSTRUMENT_BEATS_MAX, ENT_NUMBER	},
    {"mana_min",		ENTITY_OBJ_INSTRUMENT_MANA_MIN,	ENT_NUMBER	},
    {"mana_max",		ENTITY_OBJ_INSTRUMENT_MANA_MAX,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_seed[] = {
    {"growth_time",		ENTITY_OBJ_SEED_GROWTH_TIME,	ENT_NUMBER	},
    {"object_vnum",		ENTITY_OBJ_SEED_OBJECT_VNUM,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_cart[] = {
    {"flags",			ENTITY_OBJ_CART_FLAGS,			ENT_BITVECTOR	},
    {"min_strength",	ENTITY_OBJ_CART_MIN_STRENGTH,	ENT_NUMBER	},
    {"move_delay",		ENTITY_OBJ_CART_MOVE_DELAY,		ENT_NUMBER	},
    {"capacity",		ENTITY_OBJ_CART_CAPACITY,		ENT_NUMBER	},
    {"max_items",		ENTITY_OBJ_CART_MAX_ITEMS,		ENT_NUMBER	},
    {"weight_mult",		ENTITY_OBJ_CART_WEIGHT_MULT,	ENT_NUMBER	},
    {"vanish_time",		ENTITY_OBJ_CART_VANISH_TIME,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_item_ship[] = {
    {"weight",			ENTITY_OBJ_ITEM_SHIP_WEIGHT,	ENT_NUMBER	},
    {"move_delay",		ENTITY_OBJ_ITEM_SHIP_MOVE_DELAY, ENT_NUMBER	},
    {"min_crew",		ENTITY_OBJ_ITEM_SHIP_MIN_CREW,	ENT_NUMBER	},
    {"capacity",		ENTITY_OBJ_ITEM_SHIP_CAPACITY,	ENT_NUMBER	},
    {"max_crew",		ENTITY_OBJ_ITEM_SHIP_MAX_CREW,	ENT_NUMBER	},
    {"first_room",		ENTITY_OBJ_ITEM_SHIP_FIRST_ROOM, ENT_NUMBER	},
    {"hit_points",		ENTITY_OBJ_ITEM_SHIP_HIT_POINTS, ENT_NUMBER	},
    {"max_guns",		ENTITY_OBJ_ITEM_SHIP_MAX_GUNS,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_sextant[] = {
    {"accuracy",		ENTITY_OBJ_SEXTANT_ACCURACY,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_weapon_con[] = {
    {"max_weight",		ENTITY_OBJ_WEAPON_CON_MAX_WEIGHT, ENT_NUMBER},
    {"weapon_type",		ENTITY_OBJ_WEAPON_CON_WEAPON_TYPE, ENT_NUMBER},
    {"max_items",		ENTITY_OBJ_WEAPON_CON_MAX_ITEMS, ENT_NUMBER	},
    {"weight_mult",		ENTITY_OBJ_WEAPON_CON_WEIGHT_MULT, ENT_NUMBER},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_book[] = {
    {"flags",			ENTITY_OBJ_BOOK_FLAGS,			ENT_BITVECTOR	},
    {"current_page",	ENTITY_OBJ_BOOK_CURRENT_PAGE,	ENT_NUMBER	},
    {"open_page",		ENTITY_OBJ_BOOK_OPEN_PAGE,		ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_herb[] = {
    {"type",			ENTITY_OBJ_HERB_TYPE,			ENT_NUMBER	},
    {"healing",			ENTITY_OBJ_HERB_HEALING,		ENT_NUMBER	},
    {"regenerative",	ENTITY_OBJ_HERB_REGENERATIVE,	ENT_NUMBER	},
    {"refreshing",		ENTITY_OBJ_HERB_REFRESHING,		ENT_NUMBER	},
    {"immunity",		ENTITY_OBJ_HERB_IMMUNITY,		ENT_BITVECTOR	},
    {"resistance",		ENTITY_OBJ_HERB_RESISTANCE,		ENT_BITVECTOR	},
    {"vulnerability",	ENTITY_OBJ_HERB_VULNERABILITY,	ENT_BITVECTOR	},
    {"spell",			ENTITY_OBJ_HERB_SPELL,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_mist[] = {
    {"obscure_mobs",	ENTITY_OBJ_MIST_OBSCURE_MOBS,	ENT_NUMBER	},
    {"obscure_objs",	ENTITY_OBJ_MIST_OBSCURE_OBJS,	ENT_NUMBER	},
    {"obscure_room",	ENTITY_OBJ_MIST_OBSCURE_ROOM,	ENT_NUMBER	},
    {"icy",				ENTITY_OBJ_MIST_ICY,			ENT_NUMBER	},
    {"fiery",			ENTITY_OBJ_MIST_FIERY,			ENT_NUMBER	},
    {"acidic",			ENTITY_OBJ_MIST_ACIDIC,			ENT_NUMBER	},
    {"stink",			ENTITY_OBJ_MIST_STINK,			ENT_NUMBER	},
    {"wither",			ENTITY_OBJ_MIST_WITHER,			ENT_NUMBER	},
    {"toxic",			ENTITY_OBJ_MIST_TOXIC,			ENT_NUMBER	},
    {"shock",			ENTITY_OBJ_MIST_SHOCK,			ENT_NUMBER	},
    {"fog",				ENTITY_OBJ_MIST_FOG,			ENT_NUMBER	},
    {"sleep",			ENTITY_OBJ_MIST_SLEEP,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_trade[] = {
    {"trade_type",		ENTITY_OBJ_TRADE_TRADE_TYPE,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_tattoo[] = {
    {"touches",			ENTITY_OBJ_TATTOO_TOUCHES,		ENT_NUMBER	},
    {"fading_chance",	ENTITY_OBJ_TATTOO_FADING_CHANCE, ENT_NUMBER	},
    {"fading_rate",		ENTITY_OBJ_TATTOO_FADING_RATE,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_ink[] = {
    {"type0",			ENTITY_OBJ_INK_TYPE0,			ENT_NUMBER	},
    {"type1",			ENTITY_OBJ_INK_TYPE1,			ENT_NUMBER	},
    {"type2",			ENTITY_OBJ_INK_TYPE2,			ENT_NUMBER	},
    {"amount0",			ENTITY_OBJ_INK_AMOUNT0,			ENT_NUMBER	},
    {"amount1",			ENTITY_OBJ_INK_AMOUNT1,			ENT_NUMBER	},
    {"amount2",			ENTITY_OBJ_INK_AMOUNT2,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_telescope[] = {
    {"distance",		ENTITY_OBJ_TELESCOPE_DISTANCE,	ENT_NUMBER	},
    {"min_distance",	ENTITY_OBJ_TELESCOPE_MIN_DISTANCE, ENT_NUMBER},
    {"max_distance",	ENTITY_OBJ_TELESCOPE_MAX_DISTANCE, ENT_NUMBER},
    {"bonus_view",		ENTITY_OBJ_TELESCOPE_BONUS_VIEW, ENT_NUMBER	},
    {"heading",			ENTITY_OBJ_TELESCOPE_HEADING,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_compass[] = {
    {"accuracy",		ENTITY_OBJ_COMPASS_ACCURACY,	ENT_NUMBER	},
    {"wuid",			ENTITY_OBJ_COMPASS_WUID,		ENT_NUMBER	},
    {"x",				ENTITY_OBJ_COMPASS_X,			ENT_NUMBER	},
    {"y",				ENTITY_OBJ_COMPASS_Y,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_body_part[] = {
    {"parts",			ENTITY_OBJ_BODY_PART_PARTS,		ENT_NUMBER	},
    {"race_uid",		ENTITY_OBJ_BODY_PART_RACE_UID,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_scroll[] = {
    {"max_mana",		ENTITY_OBJ_SCROLL_MAX_MANA,		ENT_NUMBER	},
    {"flags",			ENTITY_OBJ_SCROLL_FLAGS,		ENT_BITVECTOR	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_tool[] = {
    {"type",			ENTITY_OBJ_TOOL_TYPE,			ENT_NUMBER	},
    {"tier",			ENTITY_OBJ_TOOL_TIER,			ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_jewelry[] = {
    {"max_mana",		ENTITY_OBJ_JEWELRY_MAX_MANA,	ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_map[] = {
    {"wuid",			ENTITY_OBJ_MAP_WUID,			ENT_NUMBER	},
    {"x",				ENTITY_OBJ_MAP_X,				ENT_NUMBER	},
    {"y",				ENTITY_OBJ_MAP_Y,				ENT_NUMBER	},
    {NULL,				0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_obj_page[] = {
    {"page_no",			ENTITY_OBJ_PAGE_PAGE_NO,		ENT_NUMBER	},
    {"title",			ENTITY_OBJ_PAGE_TITLE,			ENT_STRING	},
    {"text",			ENTITY_OBJ_PAGE_TEXT,			ENT_STRING	},
    {NULL,				0,								ENT_UNKNOWN	}
};

struct _entity_type_info entity_type_info[] = {
    { ENT_PRIMARY,		ENT_PRIMARY,		entity_primary,				true	},
    { ENT_BOOLEAN,		ENT_BOOLEAN,		entity_boolean,				false	},
    { ENT_NUMBER,		ENT_NUMBER,			entity_number,				false	},
    { ENT_STRING,		ENT_STRING,			entity_string,				false	},
    { ENT_MOBILE,		ENT_MOBILE,			entity_mobile,				true	},
    { ENT_OBJECT,		ENT_OBJECT,			entity_object,				true	},
    { ENT_ROOM,			ENT_ROOM,			entity_room,				true	},
    { ENT_EXIT,			ENT_EXIT,			entity_exit,				false	},
    { ENT_TOKEN,		ENT_TOKEN,			entity_token,				true	},
    { ENT_AREA,			ENT_AREA,			entity_area,				true	},
    { ENT_SKILL,		ENT_SKILL,			entity_skill,				false	},
    { ENT_SKILLINFO,	ENT_SKILLINFO,		entity_skill_info,			false	},
    { ENT_CONN,			ENT_CONN,			entity_conn,				false	},
    { ENT_AFFECT,		ENT_AFFECT,			entity_affect,				false	},
    { ENT_EXTRADESC,	ENT_EXTRADESC,		NULL,						false	},
    { ENT_HELP,			ENT_HELP,			NULL,						false	},
    { ENT_PRIOR,		ENT_PRIOR,			entity_prior,				false	},
    { ENT_MOBILE_ID,	ENT_MOBILE_ID,		entity_mobile,				false	},
    { ENT_OBJECT_ID,	ENT_OBJECT_ID,		entity_object,				false	},
    { ENT_TOKEN_ID,		ENT_TOKEN_ID,		entity_token,				false	},
    { ENT_AREA_ID,		ENT_AREA_ID,		entity_area,				false	},
    { ENT_SKILLINFO_ID,	ENT_SKILLINFO_ID,	entity_skill_info,			false	},
    { ENT_CLONE_ROOM,	ENT_WILDS_ROOM,		entity_room,				false	},
    { ENT_CLONE_DOOR,	ENT_WILDS_DOOR,		entity_exit,				false	},
    { ENT_WILDS,		ENT_WILDS,			entity_wilds,				false	},
    { ENT_GAME,			ENT_GAME,			entity_game,				false	},
    { ENT_CHURCH,		ENT_CHURCH,			entity_church,				false	},
    { ENT_BLLIST_MIN,	ENT_BLLIST_MAX,		entity_list,				false	},
    { ENT_PLLIST_MIN,	ENT_PLLIST_MAX,		entity_list,				false	},
    { ENT_OLLIST_MIN,	ENT_OLLIST_MAX,		entity_list,				false	},
    { ENT_ILLIST_MIN,	ENT_ILLIST_MAX,		NULL,						false	},
    { ENT_NULL,			ENT_NULL,			NULL,						false	},
    { ENT_VARIABLE,		ENT_VARIABLE,		entity_variable,			false	},
    { ENT_PERSIST,		ENT_PERSIST,		entity_persist,				false	},
    { ENT_GROUP,		ENT_GROUP,			entity_group,				false	},
    { ENT_DICE,			ENT_DICE,			entity_dice,				false	},
    { ENT_MOBINDEX,		ENT_MOBINDEX,		entity_mobindex,			false	},
    { ENT_OBJINDEX,		ENT_OBJINDEX,		entity_objindex,			false	},
    { ENT_SECTION,		ENT_SECTION,		entity_instance_section,	false	},
    { ENT_INSTANCE,		ENT_INSTANCE,		entity_instance,			false	},
    { ENT_DUNGEON,		ENT_DUNGEON,		entity_dungeon,				false	},
    { ENT_SONG,			ENT_SONG,			entity_song,				false	},
    { ENT_RACE,			ENT_RACE,			entity_race,				false	},
    { ENT_CLASS,		ENT_CLASS,			entity_class,				false	},
    { ENT_CLASSLEVEL,	ENT_CLASSLEVEL,		entity_classlevel,			false	},
    { ENT_SKILLENTRY,	ENT_SKILLENTRY,		entity_skillentry,			false	},
    
    { ENT_RESERVED_MOBILE,  ENT_RESERVED_MOBILE,  NULL,                    false },
    { ENT_RESERVED_OBJECT,  ENT_RESERVED_OBJECT,  NULL,            false },
    { ENT_RESERVED_ROOM,    ENT_RESERVED_ROOM,    NULL,                    false },
    { ENT_RESERVED_AREA,    ENT_RESERVED_AREA,    NULL,                    false },
    { ENT_RESERVED_TOKEN,   ENT_RESERVED_TOKEN,   NULL,                    false },
    { ENT_RESERVED_RPROG,   ENT_RESERVED_RPROG,   NULL,                    false },
    { ENT_RESERVED_OPROG,   ENT_RESERVED_OPROG,   NULL,                    false },
    { ENT_RESERVED_MPROG,   ENT_RESERVED_MPROG,   NULL,                    false },
    { ENT_RESERVED_TPROG,   ENT_RESERVED_TPROG,   NULL,                    false },
    { ENT_RESERVED_APROG,   ENT_RESERVED_APROG,   NULL,                    false },
    { ENT_GAME_SETTING,     ENT_GAME_SETTING,     NULL,                    false },
    { ENT_TOKEN_INDEX,      ENT_TOKEN_INDEX,      NULL,                    false },
    { ENT_SCRIPT_DATA,        ENT_SCRIPT_DATA,        NULL,                    false },

    // Object typed data sub-entities
    { ENT_OBJ_WEAPON,      ENT_OBJ_WEAPON,     entity_obj_weapon,          false },
    { ENT_OBJ_ARMOR,       ENT_OBJ_ARMOR,      entity_obj_armor,           false },
    { ENT_OBJ_CONTAINER,   ENT_OBJ_CONTAINER,  entity_obj_container,       false },
    { ENT_OBJ_FLUID_CON,   ENT_OBJ_FLUID_CON,  entity_obj_fluid_con,      false },
    { ENT_OBJ_FOOD,        ENT_OBJ_FOOD,       entity_obj_food,            false },
    { ENT_OBJ_FURNITURE,   ENT_OBJ_FURNITURE,  entity_obj_furniture,       false },
    { ENT_OBJ_PORTAL,      ENT_OBJ_PORTAL,     entity_obj_portal,          false },
    { ENT_OBJ_LIGHT,       ENT_OBJ_LIGHT,      entity_obj_light,           false },
    { ENT_OBJ_MONEY,       ENT_OBJ_MONEY,      entity_obj_money,           false },
    { ENT_OBJ_WAND,        ENT_OBJ_WAND,       entity_obj_wand,            false },
    { ENT_OBJ_CORPSE,      ENT_OBJ_CORPSE,     entity_obj_corpse,          false },
    { ENT_OBJ_INSTRUMENT,  ENT_OBJ_INSTRUMENT, entity_obj_instrument,      false },
    { ENT_OBJ_SEED,        ENT_OBJ_SEED,       entity_obj_seed,            false },
    { ENT_OBJ_CART,         ENT_OBJ_CART,       entity_obj_cart,            false },
    { ENT_OBJ_ITEM_SHIP,   ENT_OBJ_ITEM_SHIP,  entity_obj_item_ship,      false },
    { ENT_OBJ_SEXTANT,     ENT_OBJ_SEXTANT,    entity_obj_sextant,         false },
    { ENT_OBJ_WEAPON_CON,  ENT_OBJ_WEAPON_CON, entity_obj_weapon_con,      false },
    { ENT_OBJ_BOOK,        ENT_OBJ_BOOK,       entity_obj_book,            false },
    { ENT_OBJ_HERB,        ENT_OBJ_HERB,       entity_obj_herb,            false },
    { ENT_OBJ_MIST,        ENT_OBJ_MIST,       entity_obj_mist,            false },
    { ENT_OBJ_TRADE,       ENT_OBJ_TRADE,      entity_obj_trade,           false },
    { ENT_OBJ_TATTOO,      ENT_OBJ_TATTOO,     entity_obj_tattoo,          false },
    { ENT_OBJ_INK,         ENT_OBJ_INK,        entity_obj_ink,             false },
    { ENT_OBJ_TELESCOPE,   ENT_OBJ_TELESCOPE,  entity_obj_telescope,       false },
    { ENT_OBJ_COMPASS,     ENT_OBJ_COMPASS,    entity_obj_compass,         false },
    { ENT_OBJ_BODY_PART,   ENT_OBJ_BODY_PART,  entity_obj_body_part,       false },
    { ENT_OBJ_SCROLL,      ENT_OBJ_SCROLL,     entity_obj_scroll,          false },
    { ENT_OBJ_TOOL,        ENT_OBJ_TOOL,       entity_obj_tool,            false },
    { ENT_OBJ_JEWELRY,     ENT_OBJ_JEWELRY,    entity_obj_jewelry,         false },
    { ENT_OBJ_MAP,         ENT_OBJ_MAP,        entity_obj_map,             false },
    { ENT_OBJ_PAGE,        ENT_OBJ_PAGE,       entity_obj_page,            false },

    { ENT_UNKNOWN,		ENT_UNKNOWN,		NULL,						false	},
};


// Trigger types
struct trigger_type trigger_table	[] = {
//	name,					alias, 		type,					slot,			mob?,	obj?,	room?,	token?, area?, instance?, dungeon?
{	"act",					NULL,		TRIG_ACT,				TRIGSLOT_ACTION,		true,	true,	true,	true,	false,	false,	false	},
{	"afterdeath",			NULL,		TRIG_AFTERDEATH,		TRIGSLOT_REPOP,			false,	false,	false,	true,	false,	false,	false	},
{	"afterkill",			NULL,		TRIG_AFTERKILL,			TRIGSLOT_FIGHT,			true,	true,	true,	true,	false,	false,	false	},
{	"animate",				NULL,		TRIG_ANIMATE,			TRIGSLOT_ANIMATE,		true,	false,	false,	true,	false,	false,	false	},
{	"assist",				NULL,		TRIG_ASSIST,			TRIGSLOT_FIGHT,			true,	false,	false,	true,	false,	false,	false	},
{	"attack",				NULL,		TRIG_ATTACK,			TRIGSLOT_FIGHT,			true,	false,	false,	true,	false,	false,	false	},
{	"attack_backstab",		NULL,		TRIG_ATTACK_BACKSTAB,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_bash",			NULL,		TRIG_ATTACK_BASH,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_behead",		NULL,		TRIG_ATTACK_BEHEAD,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_bite",			NULL,		TRIG_ATTACK_BITE,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_blackjack",		NULL,		TRIG_ATTACK_BLACKJACK,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_circle",		NULL,		TRIG_ATTACK_CIRCLE,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_counter",		NULL,		TRIG_ATTACK_COUNTER,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_cripple",		NULL,		TRIG_ATTACK_CRIPPLE,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_dirtkick",		NULL,		TRIG_ATTACK_DIRTKICK,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_disarm",		NULL,		TRIG_ATTACK_DISARM,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_intimidate",	NULL,		TRIG_ATTACK_INTIMIDATE,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_kick",			NULL,		TRIG_ATTACK_KICK,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_rend",			NULL,		TRIG_ATTACK_REND,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_slit",			NULL,		TRIG_ATTACK_SLIT,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_smite",			NULL,		TRIG_ATTACK_SMITE,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_tailkick",		NULL,		TRIG_ATTACK_TAILKICK,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_trample",		NULL,		TRIG_ATTACK_TRAMPLE,	TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"attack_turn",			NULL,		TRIG_ATTACK_TURN,		TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"barrier",				NULL,		TRIG_BARRIER,			TRIGSLOT_ATTACKS,		true,	false,	false,	true,	false,	false,	false	},
{	"blow",					NULL,		TRIG_BLOW,				TRIGSLOT_GENERAL,		false,	true,	false,	false,	false,	false,	false	},
{	"board",				NULL,		TRIG_BOARD,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"brandish",				NULL,		TRIG_BRANDISH,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"bribe",				NULL,		TRIG_BRIBE,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"bury",					NULL,		TRIG_BURY,				TRIGSLOT_GENERAL,		false,	true,	true,	true,	false,	false,	false	},
{	"buy",					NULL,		TRIG_BUY,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"caninterrupt",			NULL,		TRIG_CANINTERRUPT,		TRIGSLOT_INTERRUPT,		false,	false,	false,	true,	false,	false,	false	},
{	"catalyst",				NULL,		TRIG_CATALYST,			TRIGSLOT_GENERAL,		false,  true,	false,	false,	false,	false,	false	},
{	"catalystfull",			NULL,		TRIG_CATALYST_FULL,		TRIGSLOT_GENERAL,		false,  true,	false,	false,	false,	false,	false	},
{	"catalystsrc",			NULL,		TRIG_CATALYST_SOURCE,	TRIGSLOT_GENERAL,		false,  true,	false,	false,	false,	false,	false	},
{	"check_buyer",			NULL,		TRIG_CHECK_BUYER,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"check_damage",			NULL,		TRIG_CHECK_DAMAGE,		TRIGSLOT_DAMAGE,		true,	true,	true,	true,	false,	false,	false	},
{	"clone_extract",		NULL,		TRIG_CLONE_EXTRACT,		TRIGSLOT_GENERAL,		false,  false,	true,	true,	false,	false,	false	},
{	"close",				NULL,		TRIG_CLOSE,				TRIGSLOT_GENERAL,		false,  true,	true,	true,	false,	false,	false	},
{	"combatstyle",			NULL,		TRIG_COMBAT_STYLE,		TRIGSLOT_COMBATSTYLE,	false, false, false, true,	false,	false,	false	},	// Only on tokens
{	"completed",			NULL,		TRIG_COMPLETED,			TRIGSLOT_REPOP,			false,	false,	false,	false,	false,	true,	true	},
{	"contract_complete",	NULL,		TRIG_CONTRACT_COMPLETE,	TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"custom_price",			NULL,		TRIG_CUSTOM_PRICE,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"damage",				NULL,		TRIG_DAMAGE,			TRIGSLOT_DAMAGE,		true,	true,	true,	true,	false,	false,	false	},
{	"death",				NULL,		TRIG_DEATH,				TRIGSLOT_REPOP,			true,	false,	true,	true,	false,	false,	false	},
{	"death_protection",		NULL,		TRIG_DEATH_PROTECTION,	TRIGSLOT_REPOP,			true,	false,	true,	true,	false,	false,	false	},
{	"death_timer",			NULL,		TRIG_DEATH_TIMER,		TRIGSLOT_REPOP,			true,	false,	true,	true,	false,	false,	false	},
{	"defense",				NULL,		TRIG_DEFENSE,			TRIGSLOT_COMBATSTYLE,	true, false, false, true,	false,	false,	false	},	// Only on tokens
{	"delay",				NULL,		TRIG_DELAY,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"drink",				NULL,		TRIG_DRINK,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"drop",					NULL,		TRIG_DROP,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"dungeon_commenced",	NULL,		TRIG_DUNGEON_COMMENCED,	TRIGSLOT_GENERAL,		false,	false,	false,	false,	false,	false,	true	},
{	"dungeon_schematic",	NULL,		TRIG_DUNGEON_SCHEMATIC,	TRIGSLOT_GENERAL,		false,	false,	false,	false,	false,	false,	true	},
{	"eat",					NULL,		TRIG_EAT,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"emote",				NULL,		TRIG_EMOTE,				TRIGSLOT_ACTION,		true,	false,	false,	true,	false,	false,	false	},
{	"emoteat",				NULL,		TRIG_EMOTEAT,			TRIGSLOT_ACTION,		true,	false,	false,	true,	false,	false,	false	},
{	"emoteself",			NULL,		TRIG_EMOTESELF,			TRIGSLOT_ACTION,		false,	false,	false,	true,	false,	false,	false	},
{	"entry",				NULL,		TRIG_ENTRY,				TRIGSLOT_MOVE,			true,	true,	true,	true,	false,	true,	true	},
{	"exall",				NULL,		TRIG_EXALL,				TRIGSLOT_MOVE,			true,	true,	true,	true,	false,	false,	false	},
{	"examine",				NULL,		TRIG_EXAMINE,			TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"exit",					NULL,		TRIG_EXIT,				TRIGSLOT_MOVE,			true,	true,	true,	true,	false,	false,	false	},
{	"expire",				NULL,		TRIG_EXPIRE,			TRIGSLOT_REPOP,			false,	false,	false,	true,	false,	false,	false	},
{	"extract",				NULL,		TRIG_EXTRACT,			TRIGSLOT_REPOP,			true,	true,	true,	false,	false,	false,	false	},
{	"failed",				NULL,		TRIG_FAILED,			TRIGSLOT_GENERAL,		false,	false,	false,	false,	false,	true,	true	},
{	"fight",				NULL,		TRIG_FIGHT,				TRIGSLOT_FIGHT,			true,	true,	true,	true,	false,	false,	false	},
{	"flee",					NULL,		TRIG_FLEE,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"forcedismount",		NULL,		TRIG_FORCEDISMOUNT,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"get",					NULL,		TRIG_GET,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"give",					NULL,		TRIG_GIVE,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"grall",				NULL,		TRIG_GRALL,				TRIGSLOT_MOVE,			true,	true,	true,	true,	false,	false,	false	},
{	"greet",				NULL,		TRIG_GREET,				TRIGSLOT_MOVE,			true,	true,	true,	true,	false,	false,	false	},
{	"grouped",				NULL,		TRIG_GROUPED,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"grow",					NULL,		TRIG_GROW,				TRIGSLOT_RANDOM,		false,	true,	false,	true,	false,	false,	false	},
{	"hidden",				NULL,		TRIG_HIDDEN,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"hide",					NULL,		TRIG_HIDE,				TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"hit",					NULL,		TRIG_HIT,				TRIGSLOT_HITS,			true,	false,	false,	true,	false,	false,	false	},
{	"hitgain",				NULL,		TRIG_HITGAIN,			TRIGSLOT_RANDOM,		true,	false,	false,	true,	false,	false,	false	},
{	"hpcnt",				NULL,		TRIG_HPCNT,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"identify",				NULL,		TRIG_IDENTIFY,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"inspect",				NULL,		TRIG_INSPECT,			TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"inspect_custom",		NULL,		TRIG_INSPECT_CUSTOM,	TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"interrupt",			NULL,		TRIG_INTERRUPT,			TRIGSLOT_INTERRUPT,		false,	false,	false,	true,	false,	false,	false	},
{	"kill",					NULL,		TRIG_KILL,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"knock",				NULL,		TRIG_KNOCK,				TRIGSLOT_GENERAL,		false,	false,	true,	true,	false,	false,	false	},
{	"knocking",				NULL,		TRIG_KNOCKING,			TRIGSLOT_GENERAL,		false,	false,	true,	true,	false,	false,	false	},
{	"land",					NULL,		TRIG_LAND,				TRIGSLOT_MOVE,			true,	false,	false,	true,	false,	false,	false	},
{	"level",				NULL,		TRIG_LEVEL,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"login",				NULL,		TRIG_LOGIN,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"lore",					NULL,		TRIG_LORE,				TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"lorex",				NULL,		TRIG_LORE_EX,			TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"managain",				NULL,		TRIG_MANAGAIN,			TRIGSLOT_RANDOM,		true,	false,	false,	true,	false,	false,	false	},
{	"moon",					NULL,		TRIG_MOON,				TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"mount",				NULL,		TRIG_MOUNT,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"move_char",			NULL,		TRIG_MOVE_CHAR,			TRIGSLOT_MOVE,			true,	false,	true,	true,	false,	false,	false	},
{	"movegain",				NULL,		TRIG_MOVEGAIN,			TRIGSLOT_RANDOM,		true,	false,	false,	true,	false,	false,	false	},
{	"multiclass",			NULL,		TRIG_MULTICLASS,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"open",					NULL,		TRIG_OPEN,				TRIGSLOT_GENERAL,		false,  true,   true,	true,	false,	false,	false	},
{	"postquest",			NULL,		TRIG_POSTQUEST,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"practice",				NULL,		TRIG_PRACTICE,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"practicetoken",		NULL,		TRIG_PRACTICETOKEN,		TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"preanimate",			NULL,		TRIG_PREANIMATE,		TRIGSLOT_REPOP,			true,	true,	true,	true,	false,	false,	false	},
{	"preassist",			NULL,		TRIG_PREASSIST,			TRIGSLOT_FIGHT,			true,	false,	false,	true,	false,	false,	false	},
{	"prebite",				NULL,		TRIG_PREBITE,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"prebuy",				NULL,		TRIG_PREBUY,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prebuy_obj",			NULL,		TRIG_PREBUY_OBJ,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"precast",				NULL,		TRIG_PRECAST,			TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"predeath",				NULL,		TRIG_PREDEATH,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"predismount",			NULL,		TRIG_PREDISMOUNT,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"predrink",				NULL,		TRIG_PREDRINK,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"predrop",				NULL,		TRIG_PREDROP,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"preeat",				NULL,		TRIG_PREEAT,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"preenter",				NULL,		TRIG_PREENTER,			TRIGSLOT_MOVE,			false,	true,	true,	true,	false,	false,	false	},
{	"preflee",				NULL,		TRIG_PREFLEE,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"preget",				NULL,		TRIG_PREGET,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"prehide",				NULL,		TRIG_PREHIDE,			TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"prehidein",			NULL,		TRIG_PREHIDE_IN,		TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"prekill",				NULL,		TRIG_PREKILL,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"premount",				NULL,		TRIG_PREMOUNT,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prepractice",			NULL,		TRIG_PREPRACTICE,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prepracticeother",		NULL,		TRIG_PREPRACTICEOTHER,	TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prepracticethat",		NULL,		TRIG_PREPRACTICETHAT,	TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prepracticetoken",		NULL,		TRIG_PREPRACTICETOKEN,	TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"preput",				NULL,		TRIG_PREPUT,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"prequest",				NULL,		TRIG_PREQUEST,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prerecall",			NULL,		TRIG_PRERECALL,			TRIGSLOT_GENERAL,		false,	false,	true,	true,	false,	false,	false	},
{	"prerecite",			NULL,		TRIG_PRERECITE,			TRIGSLOT_GENERAL,		false,	true,	true,	true,	false,	false,	false	},
{	"prereckoning",			NULL,		TRIG_PRERECKONING,		TRIGSLOT_RANDOM,		true,	true,	true,	true,	true,	false,	false	},
{	"prerehearse",			NULL,		TRIG_PREREHEARSE,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"preremove",			NULL,		TRIG_PREREMOVE,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"prerenew",				NULL,		TRIG_PRERENEW,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"prerest",				NULL,		TRIG_PREREST,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"preresurrect",			NULL,		TRIG_PRERESURRECT,		TRIGSLOT_REPOP,			true,	true,	true,	true,	false,	false,	false	},
{	"preround",				NULL,		TRIG_PREROUND,			TRIGSLOT_FIGHT,			true,	false,	false,	true,	false,	false,	false	},
{	"presell",				NULL,		TRIG_PRESELL,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"presit",				NULL,		TRIG_PRESIT,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"presleep",				NULL,		TRIG_PRESLEEP,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"prespell",				NULL,		TRIG_PRESPELL,			TRIGSLOT_SPELL,			false,	false,	false,	true,	false,	false,	false	},
{	"prestand",				NULL,		TRIG_PRESTAND,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"pretrain",				NULL,		TRIG_PRETRAIN,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"pretraintoken",		NULL,		TRIG_PRETRAINTOKEN,		TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"prewake",				NULL,		TRIG_PREWAKE,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"prewear",				NULL,		TRIG_PREWEAR,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"prewimpy",				NULL,		TRIG_PREWIMPY,			TRIGSLOT_FIGHT,			true,	true,	true,	true,	false,	false,	false	},
{	"pull",					NULL,		TRIG_PULL,				TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"pullon",				NULL,		TRIG_PULL_ON,			TRIGSLOT_GENERAL,		true,	true,	false,	true,	false,	false,	false	},
{	"pulse",				NULL,		TRIG_PULSE,				TRIGSLOT_RANDOM,		true,	false,	false,	true,	false,	false,	false	},
{	"push",					NULL,		TRIG_PUSH,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"pushon",				NULL,		TRIG_PUSH_ON,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"put",					NULL,		TRIG_PUT,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"quest_cancel",			NULL,		TRIG_QUEST_CANCEL,		TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"quest_complete",		NULL,		TRIG_QUEST_COMPLETE,	TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"quest_incomplete",		NULL,		TRIG_QUEST_INCOMPLETE,	TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"quest_part",			NULL,		TRIG_QUEST_PART,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"quit",					NULL,		TRIG_QUIT,				TRIGSLOT_GENERAL,		false,	false,	false,	true,	false,	false,	false	},
{	"random",				NULL,		TRIG_RANDOM,			TRIGSLOT_RANDOM,		true,	true,	true,	true,	true,	true,	true	},
{	"recall",				NULL,		TRIG_RECALL,			TRIGSLOT_GENERAL,		false,	false,	true,	true,	false,	false,	false	},
{	"recite",				NULL,		TRIG_RECITE,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"reckoning",			NULL,		TRIG_RECKONING,			TRIGSLOT_RANDOM,		true,	true,	true,	true,	true,	false,	false	},
{	"regen",				NULL,		TRIG_REGEN,				TRIGSLOT_RANDOM,		true,	true,	true,	true,	false,	false,	false	},
{	"regen_hp",				NULL,		TRIG_REGEN_HP,			TRIGSLOT_RANDOM,		true,	true,	true,	true,	false,	false,	false	},
{	"regen_mana",			NULL,		TRIG_REGEN_MANA,		TRIGSLOT_RANDOM,		true,	true,	true,	true,	false,	false,	false	},
{	"regen_move",			NULL,		TRIG_REGEN_MOVE,		TRIGSLOT_RANDOM,		true,	true,	true,	true,	false,	false,	false	},
{	"remort",				NULL,		TRIG_REMORT,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"remove",				NULL,		TRIG_REMOVE,			TRIGSLOT_GENERAL,		false,  true,	false,	true,	false,	false,	false	},
{	"renew",				NULL,		TRIG_RENEW,				TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"renew_list",			NULL,		TRIG_RENEW_LIST,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"repop",				NULL,		TRIG_REPOP,				TRIGSLOT_REPOP,			true,	true,	false,	true,	false,	true,	true	},
{	"reset",				NULL,		TRIG_RESET,				TRIGSLOT_REPOP,			false,	false,	true,	true,	true,	true,	true	},
{	"rest",					NULL,		TRIG_REST,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"restocked",			NULL,		TRIG_RESTOCKED,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"restore",				NULL,		TRIG_RESTORE,			TRIGSLOT_REPOP,			true,	false,	false,	true,	false,	false,	false	},
{	"resurrect",			NULL,		TRIG_RESURRECT,			TRIGSLOT_REPOP,			true,	false,	false,	true,	false,	false,	false	},
{	"save",					NULL,		TRIG_SAVE,				TRIGSLOT_REPOP,			true,	true,	false,	true,	false,	false,	false	},
{	"sayto",				NULL,		TRIG_SAYTO,				TRIGSLOT_SPEECH,		true,	true,	false,	true,	false,	false,	false	},
{	"showcommands",			NULL,		TRIG_SHOWCOMMANDS,		TRIGSLOT_VERB,			true,	true,	true,	true,	false,	false,	false	},
{	"sit",					NULL,		TRIG_SIT,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"skill_berserk",		NULL,		TRIG_SKILL_BERSERK,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"skill_sneak",			NULL,		TRIG_SKILL_SNEAK,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"skill_warcry",			NULL,		TRIG_SKILL_WARCRY,		TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"sleep",				NULL,		TRIG_SLEEP,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"speech",				NULL,		TRIG_SPEECH,			TRIGSLOT_SPEECH,		true,	true,	true,	true,	false,	false,	false	},
{	"spell",				NULL,		TRIG_SPELL,				TRIGSLOT_SPELL,			false,	false,	false,	true,	false,	false,	false	},
{	"spellbeat",			NULL,		TRIG_SPELLBEAT,			TRIGSLOT_SPELL,			false,	false,	false,	true,	false,	false,	false	},
{	"spellcast",			NULL,		TRIG_SPELLCAST,			TRIGSLOT_SPELL,			true,	true,	true,	true,	false,	false,	false	},
{	"spellinter",			NULL,		TRIG_SPELLINTER,		TRIGSLOT_SPELL,			false,	false,	false,	true,	false,	false,	false	},
{	"spellpenetrate",		NULL,		TRIG_SPELLPENETRATE,	TRIGSLOT_SPELL,			false,	false,	false,	true,	false,	false,	false	},
{	"spellreflect",			NULL,		TRIG_SPELLREFLECT,		TRIGSLOT_SPELL,			true,	false,	false,	true,	false,	false,	false	},
{	"spell_cure",			NULL,		TRIG_SPELL_CURE,		TRIGSLOT_SPELL,			true,	false,	false,	true,	false,	false,	false	},
{	"spell_dispel",			NULL,		TRIG_SPELL_DISPEL,		TRIGSLOT_SPELL,			true,	true,	false,	true,	false,	false,	false	},
{	"stand",				NULL,		TRIG_STAND,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"startcombat",			NULL,		TRIG_START_COMBAT,		TRIGSLOT_FIGHT,			true,	true,	true,	true,	false,	false,	false	},
{	"stripaffect",			NULL,		TRIG_STRIPAFFECT,		TRIGSLOT_REPOP,			false,	false,	false,	true,	false,	false,	false	},
{	"takeoff",				NULL,		TRIG_TAKEOFF,			TRIGSLOT_MOVE,			true,	false,	false,	true,	false,	false,	false	},
{	"throw",				NULL,		TRIG_THROW,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"tick",					NULL,		TRIG_TICK,				TRIGSLOT_RANDOM,		true,	true,	true,	true,	true,	true,	true	},
{	"token_given",			NULL,		TRIG_TOKEN_GIVEN,		TRIGSLOT_REPOP,			false,	false,	false,	true,	false,	false,	false	},
{	"token_removed",		NULL,		TRIG_TOKEN_REMOVED,		TRIGSLOT_REPOP,			false,	false,	false,	true,	false,	false,	false	},
{	"touch",				NULL,		TRIG_TOUCH,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"toxingain",			NULL,		TRIG_TOXINGAIN,			TRIGSLOT_RANDOM,		true,	false,	false,	true,	false,	false,	false	},
{	"turn",					NULL,		TRIG_TURN,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"turnon",				NULL,		TRIG_TURN_ON,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"ungrouped",			NULL,		TRIG_UNGROUPED,			TRIGSLOT_GENERAL,		true,	false,	false,	true,	false,	false,	false	},
{	"use",					NULL,		TRIG_USE,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"usewith",				NULL,		TRIG_USEWITH,			TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	"verb",					NULL,		TRIG_VERB,				TRIGSLOT_VERB,			true,	true,	true,	true,	false,	false,	false	},
{	"verbself",				NULL,		TRIG_VERBSELF,			TRIGSLOT_VERB,			false,	false,	false,	true,	false,	false,	false	},
{	"wake",					NULL,		TRIG_WAKE,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"weapon_blocked",		NULL,		TRIG_WEAPON_BLOCKED,	TRIGSLOT_ATTACKS,		false,	true,	false,	true,	false,	false,	false	},
{	"weapon_caught",		NULL,		TRIG_WEAPON_CAUGHT,		TRIGSLOT_ATTACKS,		false,	true,	false,	true,	false,	false,	false	},
{	"weapon_parried",		NULL,		TRIG_WEAPON_PARRIED,	TRIGSLOT_ATTACKS,		false,	true,	false,	true,	false,	false,	false	},
{	"wear",					NULL,		TRIG_WEAR,				TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"whisper",				NULL,		TRIG_WHISPER,			TRIGSLOT_SPEECH,		true,	false,	false,	true,	false,	false,	false	},
{	"wimpy",				NULL,		TRIG_WIMPY,				TRIGSLOT_FIGHT,			true,	true,	true,	true,	false,	false,	false	},
{	"xpgain",				NULL,		TRIG_XPGAIN,			TRIGSLOT_GENERAL,		true,	true,	true,	true,	false,	false,	false	},
{	"zap",					NULL,		TRIG_ZAP,				TRIGSLOT_GENERAL,		false,	true,	false,	true,	false,	false,	false	},
{	NULL,					NULL,		0,						TRIGSLOT_GENERAL,		false,	false,	false,	false,	false,	false,	false	}
};
int trigger_table_size = elementsof(trigger_table);


IFCHECK_DATA ifcheck_table[] = {
    // name					prog type	params	return	function				help reference
    { "eventsourceinstance",IFC_ANY,	"E",   true,   ifc_eventsourceinstance, "ifcheck eventsourceinstance" },
    { "eventsourcebracket",IFC_ANY,	"E",   true,   ifc_eventsourcebracket, "ifcheck eventsourcebracket" },
    { "eventsourceuid",    IFC_ANY,	"E",   true,   ifc_eventsourceuid,      "ifcheck eventsourceuid" },
    { "eventbracket",      IFC_ANY,	"E",   true,   ifc_eventbracket,        "ifcheck eventbracket" },
    { "eventkills",        IFC_ANY,	"S",   true,   ifc_eventkills,          "ifcheck eventkills" },
    { "eventitems",        IFC_ANY,	"S",   true,   ifc_eventitems,          "ifcheck eventitems" },
    { "eventgoal",         IFC_ANY,	"S",   true,   ifc_eventgoal,           "ifcheck eventgoal" },
    { "eventphase",        IFC_ANY,	"SS",  false,  ifc_eventphase,          "ifcheck eventphase" },
    { "eventactive",       IFC_ANY,	"S",   false,  ifc_eventactive,         "ifcheck eventactive" },
    { "haseventsource",    IFC_ANY,	"E",   false,  ifc_haseventsource,      "ifcheck haseventsource" },
    { "abs",				IFC_ANY,	"N",	false,	ifc_abs,				"ifcheck abs" },
    { "act",				IFC_ANY,	"ES",	false,	ifc_act,				"ifcheck act" },
    { "act2",				IFC_ANY,	"ES",	false,	ifc_act2,				"ifcheck act2" },
    { "affectbit",			IFC_ANY,	"ES",	true,	ifc_affectbit,			"ifcheck affectbit" },
    { "affectbit2",			IFC_ANY,	"ES",	true,	ifc_affectbit2,			"ifcheck affectbit2" },
    { "affected",			IFC_ANY,	"ES",	false,	ifc_affected,			"ifcheck affected" },
    { "affected2",			IFC_ANY,	"ES",	false,	ifc_affected2,			"ifcheck affected2" },
    { "affectedname",		IFC_ANY,	"ES",	false,	ifc_affectedname,		"ifcheck affectedname" },
    { "affectedspell",		IFC_ANY,	"ES",	false,	ifc_affectedspell,		"ifcheck affectedspell" },
    { "affectgroup",		IFC_ANY,	"ES",	true,	ifc_affectgroup,		"ifcheck affectgroup" },
    { "affectlocation",		IFC_ANY,	"ES",	true,	ifc_affectlocation,		"ifcheck affectlocation" },
    { "affectmodifier",		IFC_ANY,	"ES",	true,	ifc_affectmodifier,		"ifcheck affectmodifier" },
    { "affectskill",		IFC_ANY,	"ES",	true,	ifc_affectskill,		"ifcheck affectskill" },
    { "affecttimer",		IFC_ANY,	"ES",	true,	ifc_affecttimer,		"ifcheck affecttimer" },
    { "age",				IFC_ANY,	"E",	true,	ifc_age,				"ifcheck age" },
    { "align",				IFC_ANY,	"E",	true,	ifc_align,				"ifcheck align" },
    { "angle",				IFC_ANY,	"E",	true,	ifc_angle,				"ifcheck angle" },
    { "areahasland",		IFC_ANY,	"",		false,	ifc_areahasland,		"ifcheck areahasland" },
    { "areaid",				IFC_ANY,	"",		true,	ifc_areaid,				"ifcheck areaid" },
    { "arealandx",			IFC_ANY,	"",		true,	ifc_arealandx,			"ifcheck arealandx" },
    { "arealandy",			IFC_ANY,	"",		true,	ifc_arealandy,			"ifcheck arealandy" },
    { "arenafights",		IFC_ANY,	"E",	true,	ifc_arenafights,		"ifcheck arenafights" },
    { "arenaloss",			IFC_ANY,	"E",	true,	ifc_arenaloss,			"ifcheck arenaloss" },
    { "arenaratio",			IFC_ANY,	"E",	true,	ifc_arenaratio,			"ifcheck arenaratio" },
    { "arenawins",			IFC_ANY,	"E",	true,	ifc_arenawins,			"ifcheck arenawins" },
    { "areax",				IFC_ANY,	"",		true,	ifc_areax,				"ifcheck areax" },
    { "areay",				IFC_ANY,	"",		true,	ifc_areay,				"ifcheck areay" },
    { "bankbalance",		IFC_ANY,	"",		true,	ifc_bankbalance,		"ifcheck bankbalance" },
    { "bit",				IFC_ANY,	"",		false,	ifc_bit,				"ifcheck bit" },
    { "boost",				IFC_ANY,	"",		true,	ifc_boost,				"ifcheck boost" },
    { "boosttimer",			IFC_ANY,	"",		true,	ifc_boosttimer,			"ifcheck boosttimer" },
    { "candrop",			IFC_ANY,	"E",	false,	ifc_candrop,			"ifcheck candrop" },
    { "canget",				IFC_ANY,	"E",	false,	ifc_canget,				"ifcheck canget" },
    { "canhunt",			IFC_ANY,	"E",	false,	ifc_canhunt,			"ifcheck canhunt" },
    { "canpractice",		IFC_ANY,	"ES",	false,	ifc_canpractice,		"ifcheck canpractice" },
    { "canpull",			IFC_ANY,	"E",	false,	ifc_canpull,			"ifcheck canpull" },
    { "canput",				IFC_ANY,	"E",	false,	ifc_canput,				"ifcheck canput" },
    { "canscare",			IFC_ANY,	"E",	false,	ifc_canscare,			"ifcheck canscare" },
    { "carriedby",			IFC_O,		"E",	false,	ifc_carriedby,			"ifcheck carriedby" },
    { "carries",			IFC_ANY,	"ES",	false,	ifc_carries,			"ifcheck carries" },
    { "carryleft",			IFC_ANY,	"E",	true,	ifc_carryleft,			"ifcheck carryleft" },
    { "church",				IFC_ANY,	"ES",	false,	ifc_church,				"ifcheck church" },
    { "churchhasrelic",		IFC_ANY,	"ES",	false,	ifc_churchhasrelic,		"ifcheck churchhasrelic" },
    { "churchonline",		IFC_ANY,	"E",	true,	ifc_churchonline,		"ifcheck churchonline" },
    { "churchrank",			IFC_ANY,	"E",	true,	ifc_churchrank,			"ifcheck churchrank" },
    { "churchsize",			IFC_ANY,	"E",	true,	ifc_churchsize,			"ifcheck churchsize" },
    { "clan",				IFC_NONE,	"ES",	false,	ifc_clan,				"ifcheck clan" },
    { "class",				IFC_ANY,	"ES",	false,	ifc_class,				"ifcheck class" },
    { "classcount",			IFC_ANY,	"E",	true,	ifc_classcount,			"ifcheck classcount" },
    { "classlevel",			IFC_ANY,	"ES",	true,	ifc_classlevel,			"ifcheck classlevel" },
    { "clones",				IFC_M,		"E",	true,	ifc_clones,				"ifcheck clones" },
    { "comm",				IFC_ANY,	"ES",	false,	ifc_comm,				"ifcheck comm" },
    { "container",			IFC_ANY,	"ES",	false,	ifc_container,			"ifcheck container" },
    { "cos",				IFC_ANY,	"N",	true,	ifc_cos,				"ifcheck cos" },
    { "cpkfights",			IFC_ANY,	"E",	true,	ifc_cpkfights,			"ifcheck cpkfights" },
    { "cpkloss",			IFC_ANY,	"E",	true,	ifc_cpkloss,			"ifcheck cpkloss" },
    { "cpkratio",			IFC_ANY,	"E",	true,	ifc_cpkratio,			"ifcheck cpkratio" },
    { "cpkwins",			IFC_ANY,	"E",	true,	ifc_cpkwins,			"ifcheck cpkwins" },
    { "curhit",				IFC_ANY,	"E",	true,	ifc_curhit,				"ifcheck curhit" },
    { "curmana",			IFC_ANY,	"E",	true,	ifc_curmana,			"ifcheck curmana" },
    { "curmove",			IFC_ANY,	"E",	true,	ifc_curmove,			"ifcheck curmove" },
    { "damtype",			IFC_ANY,	"ES",	false,	ifc_damtype,			"ifcheck damtype" },
    { "danger",				IFC_ANY,	"E",	true,	ifc_danger,				"ifcheck danger" },
    { "day",				IFC_ANY,	"",		true,	ifc_day,				"ifcheck day" },
    { "death",				IFC_ANY,	"ES",	false,	ifc_death,				"ifcheck death" },
    { "deathcount",			IFC_ANY,	"E",	true,	ifc_deathcount,			"ifcheck deathcount" },
    { "deitypoint",			IFC_ANY,	"E",	true,	ifc_deity,				"ifcheck deitypoint" },
    { "dice",				IFC_ANY,	"NNn",	true,	ifc_dice,				"ifcheck dice" },
    { "drunk",				IFC_ANY,	"E",	true,	ifc_drunk,				"ifcheck drunk" },
    { "dungeonflag",		IFC_ANY,	"E",	false,	ifc_dungeonflag,		"ifcheck dungeonflag" },
    { "exists",				IFC_NONE,	"S",	false,	ifc_exists,				"ifcheck exists" },
    { "exitexists",			IFC_ANY,	"eS",	false,	ifc_exitexists,			"ifcheck exitexists" },
    { "exitflag",			IFC_ANY,	"ESS",	false,	ifc_exitflag,			"ifcheck exitflag" },
    { "findpath",			IFC_ANY,	"",		true,	ifc_findpath,			"ifcheck findpath" },
    { "flagact",			IFC_ANY,	"",		true,	ifc_flag_act,			"ifcheck flagact" },
    { "flagact2",			IFC_ANY,	"",		true,	ifc_flag_act2,			"ifcheck flagact2" },
    { "flagaffect",			IFC_ANY,	"",		true,	ifc_flag_affect,		"ifcheck flagaffect" },
    { "flagaffect2",		IFC_ANY,	"",		true,	ifc_flag_affect2,		"ifcheck flagaffect2" },
    { "flagcomm",			IFC_ANY,	"",		true,	ifc_flag_comm,			"ifcheck flagcomm" },
    { "flagcontainer",		IFC_ANY,	"",		true,	ifc_flag_container,		"ifcheck flagcontainer" },
    { "flagcorpse",			IFC_ANY,	"",		true,	ifc_flag_corpse,		"ifcheck flagcorpse" },
    { "flagexit",			IFC_ANY,	"",		true,	ifc_flag_exit,			"ifcheck flagexit" },
    { "flagextra",			IFC_ANY,	"",		true,	ifc_flag_extra,			"ifcheck flagextra" },
    { "flagextra2",			IFC_ANY,	"",		true,	ifc_flag_extra2,		"ifcheck flagextra2" },
    { "flagextra3",			IFC_ANY,	"",		true,	ifc_flag_extra3,		"ifcheck flagextra3" },
    { "flagextra4",			IFC_ANY,	"",		true,	ifc_flag_extra4,		"ifcheck flagextra4" },
    { "flagform",			IFC_ANY,	"",		true,	ifc_flag_form,			"ifcheck flagform" },
    { "flagfurniture",		IFC_ANY,	"",		true,	ifc_flag_furniture,		"ifcheck flagfurniture" },
    { "flagimm",			IFC_ANY,	"",		true,	ifc_flag_imm,			"ifcheck flagimm" },
    { "flaginterrupt",		IFC_ANY,	"",		true,	ifc_flag_interrupt,		"ifcheck flaginterrupt" },
    { "flagoff",			IFC_ANY,	"",		true,	ifc_flag_off,			"ifcheck flagoff" },
    { "flagpart",			IFC_ANY,	"",		true,	ifc_flag_part,			"ifcheck flagpart" },
    { "flagportal",			IFC_ANY,	"",		true,	ifc_flag_portal,		"ifcheck flagportal" },
    { "flagres",			IFC_ANY,	"",		true,	ifc_flag_res,			"ifcheck flagres" },
    { "flagroom",			IFC_ANY,	"",		true,	ifc_flag_room,			"ifcheck flagroom" },
    { "flagroom2",			IFC_ANY,	"",		true,	ifc_flag_room2,			"ifcheck flagroom2" },
    { "flagvuln",			IFC_ANY,	"",		true,	ifc_flag_vuln,			"ifcheck flagvuln" },
    { "flagweapon",			IFC_ANY,	"",		true,	ifc_flag_weapon,		"ifcheck flagweapon" },
    { "flagwear",			IFC_ANY,	"",		true,	ifc_flag_wear,			"ifcheck flagwear" },
    { "fullness",			IFC_ANY,	"E",	true,	ifc_fullness,			"ifcheck fullness" },
    { "furniture",			IFC_ANY,	"ES",	false,	ifc_furniture,			"ifcheck furniture" },
    { "gold",				IFC_ANY,	"E",	true,	ifc_gold,				"ifcheck gold" },
    { "groundweight",		IFC_ANY,	"E",	true,	ifc_groundweight,		"ifcheck groundweight" },
    { "groupcon",			IFC_ANY,	"E",	true,	ifc_groupcon,			"ifcheck groupcon" },
    { "groupdex",			IFC_ANY,	"E",	true,	ifc_groupdex,			"ifcheck groupdex" },
    { "grouphit",			IFC_ANY,	"E",	true,	ifc_grouphit,			"ifcheck groupwis" },
    { "groupint",			IFC_ANY,	"E",	true,	ifc_groupint,			"ifcheck groupint" },
    { "groupmana",			IFC_ANY,	"E",	true,	ifc_groupmana,			"ifcheck groupwis" },
    { "groupmaxhit",		IFC_ANY,	"E",	true,	ifc_groupmaxhit,		"ifcheck groupwis" },
    { "groupmaxmana",		IFC_ANY,	"E",	true,	ifc_groupmaxmana,		"ifcheck groupwis" },
    { "groupmaxmove",		IFC_ANY,	"E",	true,	ifc_groupmaxmove,		"ifcheck groupwis" },
    { "groupmove",			IFC_ANY,	"E",	true,	ifc_groupmove,			"ifcheck groupwis" },
    { "groupstr",			IFC_ANY,	"E",	true,	ifc_groupstr,			"ifcheck groupstr" },
    { "groupwis",			IFC_ANY,	"E",	true,	ifc_groupwis,			"ifcheck groupwis" },
    { "grpsize",			IFC_ANY,	"E",	true,	ifc_grpsize,			"ifcheck grpsize" },
    { "handsfull",			IFC_ANY,	"E",	false,	ifc_handsfull,			"ifcheck handsfull" },
    { "has",				IFC_ANY,	"ES",	false,	ifc_has,				"ifcheck has" },
    { "haschurch",			IFC_ANY,	"ES",	false,	ifc_haschurch,			"ifcheck haschurch"},
    { "hascatalyst",		IFC_ANY,	"ES",	true,	ifc_hascatalyst,		"ifcheck hascatalyst" },
    { "hascheckpoint",		IFC_ANY,	"ES",	true,	ifc_hascheckpoint,		"ifcheck hascheckpoint" },
    { "hasenviroment",		IFC_ANY,	"ES",	true,	ifc_hasenvironment,		"ifcheck hasenvironment" },
    { "hasprompt",			IFC_ANY,	"E",	false,	ifc_hasprompt,			"ifcheck hasprompt" },
    { "hasqueue",			IFC_ANY,	"E",	false,	ifc_hasqueue,			"ifcheck hasqueue" },
    { "hasreputation",		IFC_ANY,	"EN",	false,	ifc_hasreputation,		"ifcheck hasreputation" },
    { "hasship",			IFC_NONE,	"E",	false,	ifc_hasship,			"ifcheck hasship" },
    { "hasspell",			IFC_ANY,	"ES",	false,	ifc_hasspell,			"ifcheck hasspell" },
    { "hassubclass",		IFC_ANY,	"ES",	false,	ifc_hassubclass,		"ifcheck hassubclass" },
    { "hastarget",			IFC_ANY,	"E",	false,	ifc_hastarget,			"ifcheck hastarget" },
    { "hastoken",			IFC_ANY,	"EN",	false,	ifc_hastoken,			"ifcheck hastoken" },
    { "hasvlink",			IFC_ANY,	"",		true,	ifc_hasvlink,			"ifcheck hasvlink" },
    { "healregen",			IFC_ANY,	"E",	true,	ifc_healregen,			"ifcheck healregen" },
    { "hired",				IFC_ANY,	"E",	true,	ifc_hired,				"ifcheck hired" },
    { "hitdamage",			IFC_ANY,	"E",	true,	ifc_hitdamage,			"ifcheck hitdamage" },
    { "hitdamclass",		IFC_ANY,	"E",	false,	ifc_hitdamclass,		"ifcheck hitdamclass" },
    { "hitdamtype",			IFC_ANY,	"E",	false,	ifc_hitdamtype,			"ifcheck hitdamtype" },
    { "hitdicebonus",		IFC_ANY,	"E",	true,	ifc_hitdicebonus,		"ifcheck hitdicebonus" },
    { "hitdicenumber",		IFC_ANY,	"E",	true,	ifc_hitdicenumber,		"ifcheck hitdicenumber" },
    { "hitdicetype",		IFC_ANY,	"E",	true,	ifc_hitdicetype,		"ifcheck hitdicetype" },
    { "hitskilltype",		IFC_ANY,	"E",	false,	ifc_hitskilltype,		"ifcheck hitskilltype" },
    { "hour",				IFC_ANY,	"E",	true,	ifc_hour,				"ifcheck hour" },
    { "hasclass",			IFC_ANY,	"ES",	false,	ifc_hasclass,			"ifcheck hasclass" },
    { "hasskill",			IFC_ANY,	"ES",	false,	ifc_hasskill,			"ifcheck hasskill" },
    { "hassong",			IFC_ANY,	"ES",	false,	ifc_hassong,			"ifcheck hassong" },
    { "hastrait",			IFC_ANY,	"ES",	false,	ifc_hastrait,			"ifcheck hastrait" },
    { "hpcnt",				IFC_ANY,	"E",	true,	ifc_hpcnt,				"ifcheck hpcnt" },
    { "hunger",				IFC_ANY,	"E",	true,	ifc_hunger,				"ifcheck hunger" },
    { "id",					IFC_ANY,	"E",	true,	ifc_id,					"ifcheck id" },
    { "id2",				IFC_ANY,	"E",	true,	ifc_id2,				"ifcheck id2" },
    { "identical",			IFC_ANY,	"EE",	false,	ifc_identical,			"ifcheck identical" },
    { "imm",				IFC_ANY,	"ES",	false,	ifc_imm,				"ifcheck imm" },
//	{ "inchurch",			IFC_ANY,	"EE",	false,	ifc_inchurch,			"ifcheck inchurch" },	// already possible with church
    { "innature",			IFC_ANY,	"E",	false,	ifc_innature,			"ifcheck innature" },
    { "inputwait",			IFC_ANY,	"E",	false,	ifc_inputwait,			"ifcheck inputwait" },
    { "inwilds",			IFC_ANY,	"",		false,	ifc_inwilds,			"ifcheck inwilds" },
    { "isactive",			IFC_ANY,	"E",	false,	ifc_isactive,			"ifcheck isactive" },
    { "isaffectcustom",		IFC_ANY,	"E",	false,	ifc_isaffectcustom,		"ifcheck isaffectcusom" },
    { "isaffectgroup",		IFC_ANY,	"E",	false,	ifc_isaffectgroup,		"ifcheck isaffectgroup" },
    { "isaffectskill",		IFC_ANY,	"E",	false,	ifc_isaffectskill,		"ifcheck isaffectskill" },
    { "isaffectwhere",		IFC_ANY,	"E",	false,	ifc_isaffectwhere,		"ifcheck isaffectwhere" },
    { "isangel",			IFC_ANY,	"E",	false,	ifc_isangel,			"ifcheck isangel" },
    { "isareaunlocked",		IFC_ANY,	"E",	false,	ifc_isareaunlocked,		"ifcheck isareaunlocked" },
    { "isdungeonunlocked",	IFC_ANY,	"E",	false,	ifc_isdungeonunlocked,	"ifcheck isdungeonunlocked" },
    { "isboss",				IFC_ANY,	"E",	false,	ifc_isboss,				"ifcheck isboss" },
    { "isbrewing",			IFC_ANY,	"Es",	false,	ifc_isbrewing,			"ifcheck isbrewing" },
    { "isbusy",				IFC_ANY,	"E",	false,	ifc_isbusy,				"ifcheck isbusy" },
    { "iscastfailure",		IFC_ANY,	"Es",	false,	ifc_iscastfailure,		"ifcheck iscastfailure" },
    { "iscasting",			IFC_ANY,	"Es",	false,	ifc_iscasting,			"ifcheck iscasting" },
    { "iscastrecovered",	IFC_ANY,	"Es",	false,	ifc_iscastrecovered,	"ifcheck iscastrecovered" },
    { "iscastroomblocked",	IFC_ANY,	"Es",	false,	ifc_iscastroomblocked,	"ifcheck iscastroomblocked" },
    { "iscastsuccess",		IFC_ANY,	"Es",	false,	ifc_iscastsuccess,		"ifcheck iscastsuccess" },
    { "ischarm",			IFC_ANY,	"E",	false,	ifc_ischarm,			"ifcheck ischarm" },
    { "ischurchexcom",		IFC_ANY,	"E",	false,	ifc_ischurchexcom,		"ifcheck ischurchexcom" },
    { "ischurchpk",			IFC_ANY,	"E",	false,	ifc_ischurchpk,			"ifcheck ischurchpk" },
    { "isclass",			IFC_ANY,	"ES",	false,	ifc_isclass,			"ifcheck isclass" },
    { "iscloneroom",		IFC_ANY,	"E",	false,	ifc_iscloneroom,		"ifcheck iscloneroom" },
    { "iscpkproof",			IFC_ANY,	"E",	false,	ifc_iscpkproof,			"ifcheck iscpkproof" },
    { "iscrosszone",		IFC_ANY,	"E",	false,	ifc_iscrosszone,		"ifcheck iscrosszone" },
    { "isdead",				IFC_ANY,	"E",	false,	ifc_isdead,				"ifcheck dead" },
    { "isdelay",			IFC_ANY,	"E",	false,	ifc_isdelay,			"ifcheck isdelay" },
    { "isdemon",			IFC_ANY,	"E",	false,	ifc_isdemon,			"ifcheck isdemon" },
    { "isevil",				IFC_ANY,	"E",	false,	ifc_isevil,				"ifcheck isevil" },
    { "isfading",			IFC_ANY,	"Es",	false,	ifc_isfading,			"ifcheck isfading" },
    { "isfighting",			IFC_ANY,	"Ee",	false,	ifc_isfighting,			"ifcheck isfighting" },
    { "isflying",			IFC_ANY,	"E",	false,	ifc_isflying,			"ifcheck isflying" },
    { "isfollow",			IFC_ANY,	"E",	false,	ifc_isfollow,			"ifcheck isfollow" },
    { "isgood",				IFC_ANY,	"E",	false,	ifc_isgood,				"ifcheck isgood" },
    { "ishired",			IFC_ANY,	"E",	false,	ifc_ishired,			"ifcheck ishired" },
    { "ishunting",			IFC_ANY,	"E",	false,	ifc_ishunting,			"ifcheck ishunting" },
    { "isimmort",			IFC_ANY,	"E",	false,	ifc_isimmort,			"ifcheck isimmort" },
    { "iskey",				IFC_ANY,	"E",	false,	ifc_iskey,				"ifcheck iskey" },
    { "isleader",			IFC_ANY,	"E",	false,	ifc_isleader,			"ifcheck isleader" },
    { "ismobile",			IFC_ANY,	"E",	false,	ifc_ismobile,			"ifcheck ismobile" },
    { "ismoonup",			IFC_ANY,	"",		false,	ifc_ismoonup,			"ifcheck ismoonup" },
    { "ismorphed",			IFC_ANY,	"E",	false,	ifc_ismorphed,			"ifcheck ismorphed" },
    { "ismystic",			IFC_ANY,	"E",	false,	ifc_ismystic,			"ifcheck ismystic" },
    { "isneutral",			IFC_ANY,	"E",	false,	ifc_isneutral,			"ifcheck isneutral" },
    { "isnpc",				IFC_ANY,	"E",	false,	ifc_isnpc,				"ifcheck isnpc" },
    { "isobject",			IFC_ANY,	"E",	false,	ifc_isobject,			"ifcheck isobject" },
    { "ison",				IFC_ANY,	"E",	false,	ifc_ison,				"ifcheck ison" },
    { "isowner",			IFC_ANY,	"E",	false,	ifc_isowner,			"ifcheck isowner" },
    { "ispc",				IFC_ANY,	"E",	false,	ifc_ispc,				"ifcheck ispc" },
    { "ispersist",			IFC_ANY,	"E",	false,	ifc_ispersist,			"ifcheck ispersist" },
    { "ispk",				IFC_ANY,	"E",	false,	ifc_ispk,				"ifcheck ispk" },
    { "isprey",				IFC_ANY,	"E",	false,	ifc_isprey,				"ifcheck isprey" },
    { "isprog",				IFC_ANY,	"ES",	false,	ifc_isprog,				"ifcheck isprog" },
    { "ispulling",			IFC_ANY,	"Ee",	false,	ifc_ispulling,			"ifcheck ispulling" },
    { "ispullingrelic",		IFC_ANY,	"Es",	false,	ifc_ispullingrelic,		"ifcheck ispullingrelic" },
    { "isquesting",			IFC_ANY,	"E",	false,	ifc_isquesting,			"ifcheck isquesting" },
    { "isremort",			IFC_ANY,	"E",	false,	ifc_isremort,			"ifcheck isremort" },
    { "isrepairable",		IFC_ANY,	"E",	false,	ifc_isrepairable,		"ifcheck isrepairable" },
    { "isrestrung",			IFC_ANY,	"E",	false,	ifc_isrestrung,			"ifcheck isrestrung" },
    { "isridden",			IFC_ANY,	"E",	false,	ifc_isridden,			"ifcheck isridden" },
    { "isrider",			IFC_ANY,	"E",	false,	ifc_isrider,			"ifcheck isrider" },
    { "isriding",			IFC_ANY,	"E",	false,	ifc_isriding,			"ifcheck isriding" },
    { "isroom",				IFC_ANY,	"E",	false,	ifc_isroom,				"ifcheck isroom" },
    { "isroomdark",			IFC_ANY,	"E",	false,	ifc_isroomdark,			"ifcheck isroomdark" },
    { "issafe",				IFC_ANY,	"Es",	false,	ifc_issafe,				"ifcheck issafe" },
    { "isscribing",			IFC_ANY,	"Es",	false,	ifc_isscribing,			"ifcheck isscribing" },
    { "isshifted",			IFC_ANY,	"E",	false,	ifc_isshifted,			"ifcheck isshifted" },
    { "isshooting",			IFC_ANY,	"Es",	false,	ifc_isshooting,			"ifcheck isshooting" },
    { "isshopkeeper",		IFC_ANY,	"E",	false,	ifc_isshopkeeper,		"ifcheck isshopkeeper" },
    { "isspell",			IFC_ANY,	"E",	false,	ifc_isspell,			"ifcheck isspell" },
    { "issubclass",			IFC_ANY,	"ES",	false,	ifc_issubclass,			"ifcheck issubclass" },
    { "issustained",		IFC_ANY,	"E",	false,	ifc_issustained,		"ifcheck issustained" },
    { "istarget",			IFC_ANY,	"E",	false,	ifc_istarget,			"ifcheck istarget" },
    { "istattooing",		IFC_ANY,	"E",	false,	ifc_istattooing,		"ifcheck istattooing" },
    { "istoken",			IFC_ANY,	"E",	false,	ifc_istoken,			"ifcheck istoken" },
    { "istreasureroom",		IFC_ANY,	"EE",	false,	ifc_istreasureroom,		"ifcheck istreasureroom" },
    { "isvisible",			IFC_ANY,	"E",	false,	ifc_isvisible,			"ifcheck isvisible" },
    { "isvisibleto",		IFC_ANY,	"E",	false,	ifc_isvisibleto,		"ifcheck isvisibleto" },
    { "isworn",				IFC_ANY,	"E",	false,	ifc_isworn,				"ifcheck isworn" },
    { "lastreturn",			IFC_ANY,	"",		true,	ifc_lastreturn,			"ifcheck lastreturn" },
    { "level",				IFC_ANY,	"E",	true,	ifc_level,				"ifcheck level" },
    { "liquid",				IFC_ANY,	"ES",	false,	ifc_liquid,				"ifcheck liquid" },
    { "listcontains",		IFC_ANY,	"EE",	false,	ifc_listcontains,		"ifcheck listcontains" },
    { "loaded",				IFC_ANY,	"",		true,	ifc_loaded,				"ifcheck loaded" },
    { "lostparts",			IFC_ANY,	"ES",	false,	ifc_lostparts,			"ifcheck lostparts" },
    { "manaregen",			IFC_ANY,	"E",	true,	ifc_manaregen,			"ifcheck manaregen" },
    { "manastore",			IFC_ANY,	"E",	true,	ifc_manastore,			"ifcheck manastore" },
    { "maparea",			IFC_ANY,	"",		true,	ifc_maparea,			"ifcheck maparea" },
    { "mapheight",			IFC_ANY,	"",		true,	ifc_mapheight,			"ifcheck mapheight" },
    { "mapid",				IFC_ANY,	"",		true,	ifc_mapid,				"ifcheck mapid" },
    { "mapvalid",			IFC_ANY,	"",		false,	ifc_mapvalid,			"ifcheck mapvalid" },
    { "mapwidth",			IFC_ANY,	"",		true,	ifc_mapwidth,			"ifcheck mapwidth" },
    { "mapx",				IFC_ANY,	"",		true,	ifc_mapx,				"ifcheck mapx" },
    { "mapy",				IFC_ANY,	"",		true,	ifc_mapy,				"ifcheck mapy" },
    { "material",			IFC_ANY,	"ES",	false,	ifc_material,			"ifcheck material" },
    { "max",				IFC_ANY,	"E",	true,	ifc_max,				"ifcheck max" },
    { "maxcarry",			IFC_ANY,	"E",	true,	ifc_maxcarry,			"ifcheck maxcarry" },
    { "maxhit",				IFC_ANY,	"E",	true,	ifc_maxhit,				"ifcheck maxhit" },
    { "maxmana",			IFC_ANY,	"E",	true,	ifc_maxmana,			"ifcheck maxmana" },
    { "maxmove",			IFC_ANY,	"E",	true,	ifc_maxmove,			"ifcheck maxmove" },
    { "maxweight",			IFC_ANY,	"E",	true,	ifc_maxweight,			"ifcheck maxweight" },
    { "maxxp",				IFC_ANY,	"E",	true,	ifc_maxxp,				"ifcheck maxxp" },
    { "min",				IFC_ANY,	"E",	true,	ifc_min,				"ifcheck min" },
    { "mobclones",			IFC_ANY,	"E",	true,	ifc_mobclones,			"ifcheck mobclones" },
    { "mobexists",			IFC_ANY,	"S",	false,	ifc_mobexists,			"ifcheck mobexists" },
    { "mobhere",			IFC_ANY,	"S",	false,	ifc_mobhere,			"ifcheck mobhere" },
    { "mobs",				IFC_ANY,	"E",	true,	ifc_mobs,				"ifcheck mobs" },
    { "mobsize",			IFC_ANY,	"E",	true,	ifc_mobsize,			"ifcheck mobsize" },
    { "money",				IFC_ANY,	"E",	true,	ifc_money,				"ifcheck money" },
    { "monkills",			IFC_ANY,	"E",	true,	ifc_monkills,			"ifcheck monkills" },
    { "month",				IFC_ANY,	"",		true,	ifc_month,				"ifcheck month" },
    { "moonphase",			IFC_ANY,	"",		true,	ifc_moonphase,			"ifcheck moonphase" },
    { "moveregen",			IFC_ANY,	"E",	true,	ifc_moveregen,			"ifcheck moveregen" },
    { "name",				IFC_ANY,	"ES",	false,	ifc_name,				"ifcheck name" },
    { "numenchants",		IFC_ANY,	"ES",	true,	ifc_numenchants,		"ifcheck numenchants" },
    { "objclones",			IFC_ANY,	"E",	true,	ifc_objclones,			"ifcheck objclones" },
    { "objcond",			IFC_ANY,	"E",	true,	ifc_objcond,			"ifcheck objcond" },
    { "objcorpse",			IFC_ANY,	"ES",	false,	ifc_objcorpse,			"ifcheck objcorpse" },
    { "objcost",			IFC_ANY,	"E",	true,	ifc_objcost,			"ifcheck objcost" },
    { "objexists",			IFC_ANY,	"S",	false,	ifc_objexists,			"ifcheck objexists" },
    { "objextra",			IFC_ANY,	"ES",	false,	ifc_objextra,			"ifcheck objextra" },
    { "objextra2",			IFC_ANY,	"ES",	false,	ifc_objextra2,			"ifcheck objextra2" },
    { "objextra3",			IFC_ANY,	"ES",	false,	ifc_objextra3,			"ifcheck objextra3" },
    { "objextra4",			IFC_ANY,	"ES",	false,	ifc_objextra4,			"ifcheck objextra4" },
    { "objfrag",			IFC_ANY,	"E",	false,	ifc_objfrag,			"ifcheck objfrag" },
    { "objhere",			IFC_ANY,	"ES",	false,	ifc_objhere,			"ifcheck objhere" },
    { "objmaxweight",		IFC_ANY,	"E",	true,	ifc_objmaxweight,		"ifcheck objmaxweight" },
    { "objranged",			IFC_ANY,	"ES",	false,	ifc_objranged,			"ifcheck objranged" },
    { "objtimer",			IFC_ANY,	"E",	true,	ifc_objtimer,			"ifcheck objtimer" },
    { "objtype",			IFC_ANY,	"E",	false,	ifc_objtype,			"ifcheck objtype" },
    { "objval0",			IFC_ANY,	"E",	true,	ifc_objval0,			"ifcheck objval0" },
    { "objval1",			IFC_ANY,	"E",	true,	ifc_objval1,			"ifcheck objval1" },
    { "objval2",			IFC_ANY,	"E",	true,	ifc_objval2,			"ifcheck objval2" },
    { "objval3",			IFC_ANY,	"E",	true,	ifc_objval3,			"ifcheck objval3" },
    { "objval4",			IFC_ANY,	"E",	true,	ifc_objval4,			"ifcheck objval4" },
    { "objval5",			IFC_ANY,	"E",	true,	ifc_objval5,			"ifcheck objval5" },
    { "objval6",			IFC_ANY,	"E",	true,	ifc_objval6,			"ifcheck objval6" },
    { "objval7",			IFC_ANY,	"E",	true,	ifc_objval7,			"ifcheck objval7" },
    { "objweapon",			IFC_ANY,	"ES",	false,	ifc_objweapon,			"ifcheck objweapon" },
    { "objweaponstat",		IFC_ANY,	"ES",	false,	ifc_objweaponstat,		"ifcheck objweaponstat" },
    { "objwear",			IFC_ANY,	"ES",	false,	ifc_objwear,			"ifcheck objwear" },
    { "objwearloc",			IFC_ANY,	"ES",	false,	ifc_objwearloc,			"ifcheck objwear" },
    { "objweight",			IFC_ANY,	"E",	true,	ifc_objweight,			"ifcheck objweight" },
    { "objweightleft",		IFC_ANY,	"E",	true,	ifc_objweightleft,		"ifcheck objweightleft" },
    { "off",				IFC_ANY,	"ES",	false,	ifc_off,				"ifcheck off" },
    { "order",				IFC_MO,		"E",	true,	ifc_order,				"ifcheck order" },
    { "parts",				IFC_ANY,	"ES",	false,	ifc_parts,				"ifcheck parts" },
    { "people",				IFC_ANY,	"E",	true,	ifc_people,				"ifcheck people" },
    { "permcon",			IFC_ANY,	"E",	true,	ifc_permcon,			"ifcheck permcon" },
    { "permdex",			IFC_ANY,	"E",	true,	ifc_permdex,			"ifcheck permdex" },
    { "permint",			IFC_ANY,	"E",	true,	ifc_permint,			"ifcheck permint" },
    { "permstr",			IFC_ANY,	"E",	true,	ifc_permstr,			"ifcheck permstr" },
    { "permwis",			IFC_ANY,	"E",	true,	ifc_permwis,			"ifcheck permwis" },
    { "pgroupcon",			IFC_ANY,	"E",	true,	ifc_pgroupcon,			"ifcheck pgroupcon" },
    { "pgroupdex",			IFC_ANY,	"E",	true,	ifc_pgroupdex,			"ifcheck pgroupdex" },
    { "pgroupint",			IFC_ANY,	"E",	true,	ifc_pgroupint,			"ifcheck pgroupint" },
    { "pgroupstr",			IFC_ANY,	"E",	true,	ifc_pgroupstr,			"ifcheck pgroupstr" },
    { "pgroupwis",			IFC_ANY,	"E",	true,	ifc_pgroupwis,			"ifcheck pgroupwis" },
    { "pkfights",			IFC_ANY,	"E",	true,	ifc_pkfights,			"ifcheck pkfights" },
    { "pkloss",				IFC_ANY,	"E",	true,	ifc_pkloss,				"ifcheck pkloss" },
    { "pkratio",			IFC_ANY,	"E",	true,	ifc_pkratio,			"ifcheck pkratio" },
    { "pkwins",				IFC_ANY,	"E",	true,	ifc_pkwins,				"ifcheck pkwins" },
    { "playerexists",		IFC_ANY,	"S",	true,	ifc_playerexists,		"ifcheck playerexists" },
    { "players",			IFC_ANY,	"E",	true,	ifc_players,			"ifcheck players" },
    { "pneuma",				IFC_ANY,	"E",	true,	ifc_pneuma,				"ifcheck pneuma" },
    { "portal",				IFC_ANY,	"ES",	false,	ifc_portal,				"ifcheck portal" },
    { "portalexit",			IFC_ANY,	"ES",	false,	ifc_portalexit,			"ifcheck portalexit" },
    { "pos",				IFC_ANY,	"ES",	false,	ifc_pos,				"ifcheck pos" },
    { "practices",			IFC_ANY,	"E",	true,	ifc_practices,			"ifcheck practices" },
    { "protocol",			IFC_ANY,	"ES",	false,	ifc_protocol,			"ifcheck protocol" },
    { "questpoint",			IFC_ANY,	"E",	true,	ifc_quest,				"ifcheck questpoint" },
    { "race",				IFC_ANY,	"ES",	false,	ifc_race,				"ifcheck race" },
    { "rand",				IFC_ANY,	"Nn",	false,	ifc_rand,				"ifcheck rand" },
    { "randpoint",			IFC_ANY,	"Nn",	false,	ifc_randpoint,			"ifcheck randpoint" },
    { "reckoning",			IFC_ANY,	"",		true,	ifc_reckoning,			"ifcheck reckoning" },
    { "reckoningchance",	IFC_ANY,	"",		true,	ifc_reckoningchance,	"ifcheck reckoningchance" },
    { "reckoningcooldown",	IFC_ANY,	"",		true,	ifc_reckoningcooldown,	"ifcheck reckoningcooldown" },
    { "reckoningduration",	IFC_ANY,	"",		true,	ifc_reckoningduration,	"ifcheck reckoningduration" },
    { "reckoningintensity",	IFC_ANY,	"",		true,	ifc_reckoningintensity,	"ifcheck reckoningintensity" },
    { "register",			IFC_ANY,	"N",	true,	ifc_register,			"ifcheck register" },
    { "res",				IFC_ANY,	"ES",	false,	ifc_res,				"ifcheck res" },
    { "roll",				IFC_ANY,	"E",	false,	ifc_roll,				"ifcheck roll" },
    { "room",				IFC_ANY,	"E",	true,	ifc_room,				"ifcheck room" },
    { "roomflag",			IFC_ANY,	"ES",	false,	ifc_roomflag,			"ifcheck roomflag" },
    { "roomflag2",			IFC_ANY,	"ES",	false,	ifc_roomflag2,			"ifcheck roomflag2" },
    { "roomviewwilds",		IFC_ANY,	"E",	true,	ifc_roomviewwilds,		"ifcheck roomviewwilds" },
    { "roomweight",			IFC_ANY,	"E",	true,	ifc_roomweight,			"ifcheck roomweight" },
    { "roomwilds",			IFC_ANY,	"E",	true,	ifc_roomwilds,			"ifcheck roomwilds" },
    { "roomx",				IFC_ANY,	"E",	true,	ifc_roomx,				"ifcheck roomx" },
    { "roomy",				IFC_ANY,	"E",	true,	ifc_roomy,				"ifcheck roomy" },
    { "roomz",				IFC_ANY,	"E",	true,	ifc_roomz,				"ifcheck roomz" },
    { "samegroup",			IFC_ANY,	"ES",	false,	ifc_samegroup,			"ifcheck samegroup" },
    { "scriptsecurity",		IFC_ANY,	"",		true,	ifc_scriptsecurity,		"ifcheck systemtime" },
    { "sectionflag",		IFC_ANY,	"E",	false,	ifc_sectionflag,		"ifcheck sectionflag" },
    { "sector",				IFC_ANY,	"ES",	false,	ifc_sector,				"ifcheck sector" },
    { "sex",				IFC_ANY,	"E",	true,	ifc_sex,				"ifcheck sex" },
    { "shiptype",			IFC_ANY,	"ES",	false,	ifc_shiptype,			"ifcheck shiptype" },
    { "sign",				IFC_ANY,	"N",	true,	ifc_sign,				"ifcheck sign" },
    { "silver",				IFC_ANY,	"E",	true,	ifc_silver,				"ifcheck silver" },
    { "sin",				IFC_ANY,	"N",	true,	ifc_sin,				"ifcheck sin" },
    { "skeyword",			IFC_ANY,	"ES",	false,	ifc_skeyword,			"ifcheck skeyword" },
    { "skill",				IFC_ANY,	"ES",	true,	ifc_skill,				"ifcheck skill" },
    { "statcon",			IFC_ANY,	"E",	true,	ifc_statcon,			"ifcheck statcon" },
    { "statdex",			IFC_ANY,	"E",	true,	ifc_statdex,			"ifcheck statdex" },
    { "statint",			IFC_ANY,	"E",	true,	ifc_statint,			"ifcheck statint" },
    { "statstr",			IFC_ANY,	"E",	true,	ifc_statstr,			"ifcheck statstr" },
    { "statwis",			IFC_ANY,	"E",	true,	ifc_statwis,			"ifcheck statwis" },
    { "stoned",				IFC_ANY,	"E",	true,	ifc_stoned,				"ifcheck stoned" },
    { "strlen",				IFC_ANY,	"E",	true,	ifc_strlen,				"ifcheck strlen" },
    { "strprefix",			IFC_ANY,	"E",	false,	ifc_strprefix,			"ifcheck strprefix" },
    { "sublevel",			IFC_ANY,	"E",	true,	ifc_sublevel,			"ifcheck sublevel" },
    { "sunlight",			IFC_ANY,	"E",	true,	ifc_sunlight,			"ifcheck sunlight" },
    { "systemtime",			IFC_ANY,	"",		true,	ifc_systemtime,			"ifcheck systemtime" },
    { "tempstore1",			IFC_ANY,	"E",	true,	ifc_tempstore1,			"ifcheck tempstore1" },
    { "tempstore2",			IFC_ANY,	"E",	true,	ifc_tempstore2,			"ifcheck tempstore2" },
    { "tempstore3",			IFC_ANY,	"E",	true,	ifc_tempstore3,			"ifcheck tempstore3" },
    { "tempstore4",			IFC_ANY,	"E",	true,	ifc_tempstore4,			"ifcheck tempstore4" },
    { "tempstring",			IFC_ANY,	"ES",	false,	ifc_tempstring,			"ifcheck tempstring" },
    { "testhardmagic",		IFC_ANY,	"ES",	false,	ifc_testhardmagic,		"ifcheck testhardmagic" },
    { "testskill",			IFC_ANY,	"ES",	false,	ifc_testskill,			"ifcheck testskill" },
    { "testslowmagic",		IFC_ANY,	"ES",	false,	ifc_testslowmagic,		"ifcheck testslowmagic" },
    { "testtokenspell",		IFC_ANY,	"ES",	false,	ifc_testtokenspell,		"ifcheck testtokenspell" },
    { "thirst",				IFC_ANY,	"E",	true,	ifc_thirst,				"ifcheck thirst" },
    { "timeofday",			IFC_ANY,	"S",	false,	ifc_timeofday,			"ifcheck timeofday" },
    { "timer",				IFC_ANY,	"",		true,	ifc_timer,				"ifcheck timer" },
    { "tokencount",			IFC_ANY,	"En",	true,	ifc_tokencount,			"ifcheck tokencount" },
    { "tokenexists",		IFC_ANY,	"N",	false,	ifc_tokenexists,		"ifcheck tokenexists" },
    { "tokentimer",			IFC_ANY,	"EN",	true,	ifc_tokentimer,			"ifcheck tokentimer" },
    { "tokenvalue",			IFC_ANY,	"ENN",	true,	ifc_tokenvalue,			"ifcheck tokenvalue" },
    { "totalfights",		IFC_ANY,	"E",	true,	ifc_totalfights,		"ifcheck totalfights" },
    { "totalloss",			IFC_ANY,	"E",	true,	ifc_totalloss,			"ifcheck totalloss" },
    { "totalpkfights",		IFC_ANY,	"E",	true,	ifc_totalpkfights,		"ifcheck totalpkfights" },
    { "totalpkloss",		IFC_ANY,	"E",	true,	ifc_totalpkloss,		"ifcheck totalpkloss" },
    { "totalpkratio",		IFC_ANY,	"E",	true,	ifc_totalpkratio,		"ifcheck totalpkratio" },
    { "totalpkwins",		IFC_ANY,	"E",	true,	ifc_totalpkwins,		"ifcheck totalpkwins" },
    { "totalquests",		IFC_ANY,	"E",	true,	ifc_totalquests,		"ifcheck totalquests" },
    { "totalratio",			IFC_ANY,	"E",	true,	ifc_totalratio,			"ifcheck totalratio" },
    { "totalwins",			IFC_ANY,	"E",	true,	ifc_totalwins,			"ifcheck totalwins" },
    { "toxin",				IFC_ANY,	"ES",	true,	ifc_toxin,				"ifcheck toxin" },
    { "traitint",			IFC_ANY,	"ES",	true,	ifc_traitint,			"ifcheck traitint" },
    { "traitstring",		IFC_ANY,	"ES",	false,	ifc_traitstring,		"ifcheck traitstring" },
    { "trains",				IFC_ANY,	"E",	true,	ifc_trains,				"ifcheck trains" },
    { "uses",				IFC_ANY,	"ES",	false,	ifc_uses,				"ifcheck uses" },
    { "valueac",			IFC_ANY,	"",		true,	ifc_value_ac,			"ifcheck valueac" },
    { "valueacstr",			IFC_ANY,	"",		true,	ifc_value_acstr,		"ifcheck valueacstr" },
    { "valuedamage",		IFC_ANY,	"",		true,	ifc_value_damage,		"ifcheck valuedamage" },
    { "valueposition",		IFC_ANY,	"",		true,	ifc_value_position,		"ifcheck valueposition" },
    { "valueranged",		IFC_ANY,	"",		true,	ifc_value_ranged,		"ifcheck valueranged" },
    { "valuerelic",			IFC_ANY,	"",		true,	ifc_value_relic,		"ifcheck valuerelic" },
    { "valuesector",		IFC_ANY,	"",		true,	ifc_value_sector,		"ifcheck valuesector" },
    { "valuesize",			IFC_ANY,	"",		true,	ifc_value_size,			"ifcheck valuesize" },
    { "valuetoxin",			IFC_ANY,	"",		true,	ifc_value_toxin,		"ifcheck valuetoxin" },
    { "valuetype",			IFC_ANY,	"",		true,	ifc_value_type,			"ifcheck valuetype" },
    { "valueweapon",		IFC_ANY,	"",		true,	ifc_value_weapon,		"ifcheck valueweapon" },
    { "valuewear",			IFC_ANY,	"",		true,	ifc_value_wear,			"ifcheck valuewear" },
    { "varbool",			IFC_ANY,	"S",	false,	ifc_varbool,			"ifcheck varbool" },
    { "vardefined",			IFC_ANY,	"S",	false,	ifc_vardefined,			"ifcheck vardefined" },
    { "varexit",			IFC_ANY,	"SS",	false,	ifc_varexit,			"ifcheck varexit" },
    { "varnumber",			IFC_ANY,	"S",	true,	ifc_varnumber,			"ifcheck varnumber" },
    { "varstring",			IFC_ANY,	"SS",	false,	ifc_varstring,			"ifcheck varstring" },
    { "vnum",				IFC_ANY,	"E",	true,	ifc_vnum,				"ifcheck vnum" },
    { "vuln",				IFC_ANY,	"ES",	false,	ifc_vuln,				"ifcheck vuln" },
    { "weapon",				IFC_ANY,	"ES",	false,	ifc_weapon,				"ifcheck weapon" },
    { "weapontype",			IFC_ANY,	"ES",	false,	ifc_weapontype,			"ifcheck weapontype" },
    { "weaponskill",		IFC_ANY,	"ES",	true,	ifc_weaponskill,		"ifcheck weaponskill" },
    { "wears",				IFC_ANY,	"ES",	false,	ifc_wears,				"ifcheck wears" },
    { "wearused",			IFC_ANY,	"ES",	false,	ifc_wearused,			"ifcheck wearused" },
    { "weight",				IFC_ANY,	"E",	true,	ifc_weight,				"ifcheck weight" },
    { "weightleft",			IFC_ANY,	"E",	true,	ifc_weightleft,			"ifcheck weightleft" },
    { "wimpy",				IFC_ANY,	"E",	true,	ifc_wimpy,				"ifcheck wimpy" },
    { "word",				IFC_ANY,	"E",	false,	ifc_word,				"ifcheck word" },
    { "wornby",				IFC_O,		"E",	false,	ifc_wornby,				"ifcheck wornby" },
    { "xp",					IFC_ANY,	"",		true,	ifc_xp,					"ifcheck xp" },
    { "year",				IFC_ANY,	"",		true,	ifc_year,				"ifcheck year" },
    { NULL, 				0,			"",		false,	NULL,					"" }
};

OPCODE_FUNC opcode_table[OP_LASTCODE] = {
    opc_end,
    opc_if,
    opc_if,
    opc_else,
    opc_endif,
    opc_command,
    opc_gotoline,
    opc_for,
    opc_endfor,
    opc_exitfor,
    opc_list,
    opc_endlist,
    opc_exitlist,
    opc_while,
    opc_endwhile,
    opc_exitwhile,
    opc_switch,
    opc_endswitch,
    opc_exitswitch,
    opc_mob,
    opc_obj,
    opc_room,
    opc_token,
    opc_tokenother,
    opc_area,
    opc_instance,
    opc_dungeon,
};

char *script_operators[] = { "==", ">=", "<=", ">", "<", "!=", "&", NULL };

// The operator comparison matrix
int script_expression_stack_action[CH_MAX][STK_MAX] = {
// Stack Top:	-(neg)	!	:	%	/	*	-	+	(	empty	  // Current Operator VVV
    {	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH },   // (
    {	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH },   // -(neg)
    {	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH },   // !
    {	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH,	PUSH },   // :
    {	POP,	POP,	POP,	POP,	POP,	POP,	PUSH,	PUSH,	PUSH,	PUSH },   // %
    {	POP,	POP,	POP,	POP,	POP,	POP,	PUSH,	PUSH,	PUSH,	PUSH },   // /
    {	POP,	POP,	POP,	POP,	POP,	POP,	PUSH,	PUSH,	PUSH,	PUSH },   // *
    {	POP,	POP,	POP,	POP,	POP,	POP,	POP,	POP,	PUSH,	PUSH },   // -
    {	POP,	POP,	POP,	POP,	POP,	POP,	POP,	POP,	PUSH,	PUSH },   // +
    {	POP,	POP,	POP,	POP,	POP,	POP,	POP,	POP,	DELETE,	ERROR1 }, // )
    {	POP,	POP,	POP,	POP,	POP,	POP,	POP,	POP,	ERROR2,	DONE }    // end-of-string
};

// Conversions from CH_* to STK_* codes
int script_expression_tostack[CH_MAX+1] = { STK_OPEN, STK_NEG, STK_NOT, STK_RAND, STK_MOD, STK_DIV, STK_MUL, STK_SUB, STK_ADD, STK_MAX, STK_MAX, STK_MAX };

// Number of operands needed for operator when performing it
int script_expression_argstack[STK_MAX+1] = {1,1,2,2,2,2,2,2,0,0,0};


const char *male_female  [] = { "neuter",  "male",  "female" };
const char *he_she  [] = { "it",  "he",  "she" };
const char *him_her [] = { "it",  "him", "her" };
const char *his_her [] = { "its",  "his", "her" };
const char *his_hers_obj [] = { "its",  "his", "her" };
const char *his_hers [] = { "its", "his", "hers" };
const char *himself [] = { "itself", "himself", "herself" };


const char *exit_states[] = {
    "open",
    "bashed",
    "closed",
    "closed and locked",
    "closed and barred",
    "closed, locked and barred"
};


const struct flag_type script_flags[] = {
    { "wiznet", SCRIPT_WIZNET, true },
    { "disabled", SCRIPT_DISABLED, true },
    { "secured", SCRIPT_SECURED, true },
    { "system", SCRIPT_SYSTEM, true },
    { NULL, 0, false },
};

const struct flag_type interrupt_action_types[] = {
    {	"bind",		INTERRUPT_BIND,		true },
    {	"bomb",		INTERRUPT_BOMB,		true },
    {	"brew",		INTERRUPT_BREW,		true },
    {	"cast",		INTERRUPT_CAST,		true },
    {	"fade",		INTERRUPT_FADE,		true },
    {	"hide",		INTERRUPT_HIDE,		true },
    {	"music",	INTERRUPT_MUSIC,	true },
    {	"ranged",	INTERRUPT_RANGED,	true },
    {	"recite",	INTERRUPT_RECITE,	true },
    {	"repair",	INTERRUPT_REPAIR,	true },
    {	"resurrect",	INTERRUPT_RESURRECT,	true },
    {	"reverie",	INTERRUPT_REVERIE,	true },
    {	"scribe",	INTERRUPT_SCRIBE,	true },
    {	"script",	INTERRUPT_SCRIPT,	true },
    {	"trance",	INTERRUPT_TRANCE,	true },
    {	"silent",	INTERRUPT_SILENT,	true },
    {	NULL,		0,			false }
};

const char *cmd_operator_table[] = {
    "=",	// OPR_ASSIGN
    "+",	// OPR_ADD
    "-",	// OPR_SUB
    "*",	// OPR_MULT
    "/",	// OPR_DIV
    "%",	// OPR_MOD
    "++",	// OPR_INC
    "--",	// OPR_DEC
    "<",	// OPR_MIN
    ">",	// OPR_MAX
    "&",	// OPR_AND
    "|",	// OPR_OR
    "!",	// OPR_NOT
    "^",	// OPR_XOR
    NULL
};

const bool cmd_operator_info[OPR_END][3] = {
//	needs_value,	arith,	bitwise,
    {true,			false,	false},		// OPR_ASSIGN
    {true,			true,	false},		// OPR_ADD
    {true,			true,	false},		// OPR_SUB
    {true,			true,	false},		// OPR_MULT
    {true,			true,	false},		// OPR_DIV
    {true,			true,	false},		// OPR_MOD
    {false,			true,	false},		// OPR_INC
    {false,			true,	false},		// OPR_DEC
    {true,			false,	false},		// OPR_MIN
    {true,			false,	false},		// OPR_MAX
    {true,			false,	true},		// OPR_AND
    {true,			false,	true},		// OPR_OR
    {true,			false,	true},		// OPR_NOT
    {true,			false,	true},		// OPR_XOR
};