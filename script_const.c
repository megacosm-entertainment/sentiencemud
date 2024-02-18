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
	{"enactor",		ENTITY_ENACTOR,		ENT_MOBILE,		"The mobile that triggers the script execution" },
	{"obj1",		ENTITY_OBJ1,		ENT_OBJECT,		"Primary object" },
	{"obj2",		ENTITY_OBJ2,		ENT_OBJECT,		"Secondary object" },
	{"victim",		ENTITY_VICTIM,		ENT_MOBILE,		"Primary mobile victim" },
	{"victim2",		ENTITY_VICTIM2,		ENT_MOBILE,		"Secondary mobile victim" },
	{"target",		ENTITY_TARGET,		ENT_MOBILE,		"A remembered target mobile" },
	{"random",		ENTITY_RANDOM,		ENT_MOBILE,		"A random mobile in the current room" },
	{"here",		ENTITY_HERE,		ENT_ROOM,		"Current room" },
	{"this",		ENTITY_SELF,		ENT_UNKNOWN,	"The entity executing the script.  Adapts to the type of entity" },
	{"self",		ENTITY_SELF,		ENT_UNKNOWN,	"The entity executing the script.  Adapts to the type of entity" },
	{"phrase",		ENTITY_PHRASE,		ENT_STRING,		"Either what fired the trigger or a string passed along as an argument string" },
	{"trigger",		ENTITY_TRIGGER,		ENT_STRING,		"String or number that is needed to fire the trigger" },
	{"prior",		ENTITY_PRIOR,		ENT_PRIOR,		"Prior script info in the call stack" },
	{"game",		ENTITY_GAME,		ENT_GAME,		"Global game settings" },
	{"null",		ENTITY_NULL,		ENT_NULL,		"Represents nothing, none or null" },
	{"token",		ENTITY_TOKEN,		ENT_TOKEN,		"Token reference for certain triggers" },
	{"register1",	ENTITY_REGISTER1,	ENT_NUMBER,		"Register 1 for the script call (read only)" },
	{"register2",	ENTITY_REGISTER2,	ENT_NUMBER,		"Register 2 for the script call (read only)" },
	{"register3",	ENTITY_REGISTER3,	ENT_NUMBER,		"Register 3 for the script call (read only)" },
	{"register4",	ENTITY_REGISTER4,	ENT_NUMBER,		"Register 4 for the script call (read only)" },
	{"register5",	ENTITY_REGISTER5,	ENT_NUMBER,		"Register 5 for the script call (read only)" },
	{"mxp",			ENTITY_MXP,			ENT_STRING,		"Outputs the start of an MXP packet" },
	{"tab",			ENTITY_MXP,			ENT_STRING,		"Outputs the TAB character" },
	{"true",		ENTITY_TRUE,		ENT_BOOLEAN,	"Explicit TRUE boolean" },
	{"false",		ENTITY_FALSE,		ENT_BOOLEAN,	"Explicit FALSE boolean" },
	{NULL,			0,					ENT_UNKNOWN,	NULL	}
};

ENT_FIELD entity_types[] = {
	{"num",				ENTITY_VAR_NUM,			ENT_NUMBER,			"Number" },
	{"str",				ENTITY_VAR_STR,			ENT_STRING,			"String" },
	{"mob",				ENTITY_VAR_MOB,			ENT_MOBILE,			"Mobile" },
	{"obj",				ENTITY_VAR_OBJ,			ENT_OBJECT,			"Object" },
	{"room",			ENTITY_VAR_ROOM,		ENT_ROOM,			"Room" },
	{"exit",			ENTITY_VAR_EXIT,		ENT_EXIT,			"Exit" },
	{"token",			ENTITY_VAR_TOKEN,		ENT_TOKEN,			"Token" },
	{"area",			ENTITY_VAR_AREA,		ENT_AREA,			"Area" },
	{"aregion",			ENTITY_VAR_AREA_REGION,	ENT_AREA_REGION,	"Area Region" },
	{"skill",			ENTITY_VAR_SKILL,		ENT_SKILL,			"Skill" },
	{"skillgroup",		ENTITY_VAR_SKILLGROUP,	ENT_SKILLGROUP,		"Skill Group" },
	{"skillinfo",		ENTITY_VAR_SKILLINFO,	ENT_SKILLINFO,		"Skill Info" },
	{"song",			ENTITY_VAR_SONG,		ENT_SONG,			"Song" },
	{"race",			ENTITY_VAR_RACE,		ENT_RACE,			"Race" },
	{"class",			ENTITY_VAR_CLASS,		ENT_CLASS,			"Class" },
	{"classlevel",		ENTITY_VAR_CLASSLEVEL,	ENT_CLASSLEVEL,		"Class Level data" },
	{"aff",				ENTITY_VAR_AFFECT,		ENT_AFFECT,			"Affect" },
	{"book_page",		ENTITY_VAR_BOOK_PAGE,	ENT_BOOK_PAGE,		"Object (Page)" },
	{"food_buff",		ENTITY_VAR_FOOD_BUFF,	ENT_FOOD_BUFF,		"Food Buff" },
	{"compartment",		ENTITY_VAR_COMPARTMENT,	ENT_COMPARTMENT ,	"Furniture Compartment" },
	{"waypoint",		ENTITY_VAR_WAYPOINT,	ENT_WAYPOINT,		"Map Waypoint" },
	{"stock",			ENTITY_VAR_SHOP_STOCK,	ENT_SHOP_STOCK,		"Shop Stock" },
	{"conn",			ENTITY_VAR_CONN,		ENT_CONN,			"Connection" },
	{"church",			ENTITY_VAR_CHURCH,		ENT_CHURCH,			"Church" },
	{"var",				ENTITY_VAR_VARIABLE,	ENT_VARIABLE,		"Variable" },
	{"dynlist_exit",	ENTITY_VAR_BLLIST_EXIT,	ENT_BLLIST_EXIT,	"Dynamic List of exits" },
	{"dynlist_mob",		ENTITY_VAR_BLLIST_MOB,	ENT_BLLIST_MOB,		"Dynamic List of mobiles" },
	{"dynlist_obj",		ENTITY_VAR_BLLIST_OBJ,	ENT_BLLIST_OBJ,		"Dynamic List of objects" },
	{"dynlist_room",	ENTITY_VAR_BLLIST_ROOM,	ENT_BLLIST_ROOM,	"Dynamic List of rooms" },
	{"dynlist_skill",	ENTITY_VAR_BLLIST_SKILL,	ENT_BLLIST_SKILL,	"Dynamic List of skills" },
	{"dynlist_token",	ENTITY_VAR_BLLIST_TOK,	ENT_BLLIST_TOK,		"Dynamic List of tokens" },
	{"dynlist_area",	ENTITY_VAR_BLLIST_AREA,	ENT_BLLIST_AREA,	"Dynamic List of areas" },
	{"dynlist_aregion",	ENTITY_VAR_BLLIST_AREA_REGION,	ENT_BLLIST_AREA_REGION,	"Dynamic List of area regions" },
	{"dynlist_wilds",	ENTITY_VAR_BLLIST_WILDS,	ENT_BLLIST_WILDS,	"Dynamic List of wilds" },
	{"list_conn",		ENTITY_VAR_PLLIST_CONN,	ENT_PLLIST_CONN,	"List of connections" },
	{"list_mob",		ENTITY_VAR_PLLIST_MOB,	ENT_PLLIST_MOB,		"List of mobiles" },
	{"list_obj",		ENTITY_VAR_PLLIST_OBJ,	ENT_PLLIST_OBJ,		"List of objects" },
	{"list_room",		ENTITY_VAR_PLLIST_ROOM,	ENT_PLLIST_ROOM,	"List of rooms" },
	{"list_str",		ENTITY_VAR_PLLIST_STR,	ENT_PLLIST_STR,		"List of strings" },
	{"list_token",		ENTITY_VAR_PLLIST_TOK,	ENT_PLLIST_TOK,		"List of tokens" },
	{"list_area",		ENTITY_VAR_PLLIST_AREA,	ENT_PLLIST_AREA,	"List of areas" },
	{"list_aregion",	ENTITY_VAR_PLLIST_AREA_REGION,	ENT_PLLIST_AREA_REGION,	"List of area regions" },
	{"list_church",		ENTITY_VAR_PLLIST_CHURCH,	ENT_PLLIST_CHURCH,	"List of churches" },
	{"list_book_page",	ENTITY_VAR_PLLIST_BOOK_PAGE,	ENT_PLLIST_BOOK_PAGE,	"List of book pages" },
	{"list_food_buff",	ENTITY_VAR_PLLIST_FOOD_BUFF,	ENT_PLLIST_FOOD_BUFF,	"List of food buffs" },
	{"list_compartment",ENTITY_VAR_PLLIST_COMPARTMENT,	ENT_PLLIST_COMPARTMENT,	"List of furniture compartments" },
	{"dice",			ENTITY_VAR_DICE,		ENT_DICE,			"Dice" },
	{"sect",			ENTITY_VAR_SECTION,		ENT_SECTION,		"Instance Section" },
	{"inst",			ENTITY_VAR_INSTANCE,	ENT_INSTANCE,		"Instance" },
	{"dung",			ENTITY_VAR_DUNGEON,		ENT_DUNGEON,		"Dungeon" },
	{"ship",			ENTITY_VAR_SHIP,		ENT_SHIP,			"Ship" },
	{"mobindex",		ENTITY_VAR_MOBINDEX,	ENT_MOBINDEX ,		"Mobile Index" },
	{"objindex",		ENTITY_VAR_OBJINDEX,	ENT_OBJINDEX ,		"Object Index" },
	{"tokindex",		ENTITY_VAR_TOKENINDEX,	ENT_TOKENINDEX ,	"Token Index" },
	{"blueprint",		ENTITY_VAR_BLUEPRINT,	ENT_BLUEPRINT ,		"Blueprint" },
	{"bpsection",		ENTITY_VAR_BLUEPRINT_SECTION,	ENT_BLUEPRINT_SECTION,	"Blueprint Section" },
	{"dngindex",		ENTITY_VAR_DUNGEONINDEX,	ENT_DUNGEONINDEX,	"Dungeon Index" },
	{"shipindex",		ENTITY_VAR_SHIPINDEX,		ENT_SHIPINDEX,	"Ship Index" },
	{"lockstate",		ENTITY_VAR_LOCK_STATE,		ENT_LOCK_STATE,	"Lock State" },
	{"spell",			ENTITY_VAR_SPELL,			ENT_SPELL,		"Spell data" },
	{"liquid",			ENTITY_VAR_LIQUID,			ENT_LIQUID,		"Liquid" },
	{"material",		ENTITY_VAR_MATERIAL,		ENT_MATERIAL,	"Material" },
	{"mission",			ENTITY_VAR_MISSION,			ENT_MISSION,	"Generated mission" },
	{"missionpart",		ENTITY_VAR_MISSION_PART,	ENT_MISSION_PART,	"Generated mission part" },
	{"reputation",		ENTITY_VAR_REPUTATION,		ENT_REPUTATION,		"Reputation" },
	{"repindex",		ENTITY_VAR_REPUTATION_INDEX,	ENT_REPUTATION_INDEX,	"Reputation Index" },
	{"reprank",			ENTITY_VAR_REPUTATION_RANK,		ENT_REPUTATION_RANK,	"Reputation Rank" },
	{NULL,				0,								ENT_UNKNOWN,	NULL	}
};

ENT_FIELD entity_prior[] = {
	{"mob",			ENTITY_PRIOR_MOB,		ENT_MOBILE,		"Previous mobile $SELF" },
	{"obj",			ENTITY_PRIOR_OBJ,		ENT_OBJECT,		"Previous object $SELF" },
	{"room",		ENTITY_PRIOR_ROOM,		ENT_ROOM,		"Previous room $SELF" },
	{"token",		ENTITY_PRIOR_TOKEN,		ENT_TOKEN,		"Previous token $SELF" },
	// area
	// instance
	// dungeon
	{"enactor",		ENTITY_PRIOR_ENACTOR,	ENT_MOBILE,		"Previous $ENACTOR" },
	{"obj1",		ENTITY_PRIOR_OBJ1,		ENT_OBJECT,		"Previous $OBJ1" },
	{"obj2",		ENTITY_PRIOR_OBJ2,		ENT_OBJECT,		"Previous $OBJ2" },
	{"victim",		ENTITY_PRIOR_VICTIM,	ENT_MOBILE,		"Previous $VICTIM" },
	// victim2
	{"target",		ENTITY_PRIOR_TARGET,	ENT_MOBILE,		"Previous $TARGET" },
	{"random",		ENTITY_PRIOR_RANDOM,	ENT_MOBILE,		"Previous $RANDOM" },
	{"here",		ENTITY_PRIOR_HERE,		ENT_ROOM,		"Previous $HERE" },
	{"phrase",		ENTITY_PRIOR_PHRASE,	ENT_STRING,		"Previous $PHRASE" },
	{"trigger",		ENTITY_PRIOR_TRIGGER,	ENT_STRING,		"Previous $TRIGGER" },
	{"register1",	ENTITY_PRIOR_REGISTER1,	ENT_NUMBER,		"Previous $REGISTER1" },
	{"register2",	ENTITY_PRIOR_REGISTER2,	ENT_NUMBER,		"Previous $REGISTER2" },
	{"register3",	ENTITY_PRIOR_REGISTER3,	ENT_NUMBER,		"Previous $REGISTER3" },
	{"register4",	ENTITY_PRIOR_REGISTER4,	ENT_NUMBER,		"Previous $REGISTER4" },
	{"register5",	ENTITY_PRIOR_REGISTER5,	ENT_NUMBER,		"Previous $REGISTER5" },
	{"prior",		ENTITY_PRIOR_PRIOR,		ENT_PRIOR,		"Prior script info on call stack" },
	{NULL,			0,						ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_game[] = {
	{"name",			ENTITY_GAME_NAME,				ENT_STRING,			"Game name" },
	{"port",			ENTITY_GAME_PORT,				ENT_NUMBER,			"Port number" },
	{"players",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN,	"List of players" },
	{"mortals",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN,	"List of players" },
	{"morts",			ENTITY_GAME_PLAYERS,			ENT_PLLIST_CONN,	"List of players" },
	{"immortals",		ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN,	"List of immortals" },
	{"imms",			ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN,	"List of immortals" },
	{"staff",			ENTITY_GAME_IMMORTALS,			ENT_PLLIST_CONN,	"List of immortals" },
	{"online",			ENTITY_GAME_ONLINE,				ENT_PLLIST_CONN,	"List of everyone" },
	{"areas",			ENTITY_GAME_AREAS,				ENT_BLLIST_AREA,	"List of areas" },
	{"zones",			ENTITY_GAME_AREAS,				ENT_BLLIST_AREA,	"List of areas" },
	{"wilds",			ENTITY_GAME_WILDS,				ENT_BLLIST_WILDS,	"List of wilds" },
	{"maps",			ENTITY_GAME_WILDS,				ENT_BLLIST_WILDS,	"List of wilds" },
	{"persist",			ENTITY_GAME_PERSIST,			ENT_PERSIST,		"Persistent entities" },
	{"churches",		ENTITY_GAME_CHURCHES,			ENT_PLLIST_CHURCH,	"List of churches" },
	{"ships",			ENTITY_GAME_SHIPS,				ENT_ILLIST_SHIPS,	"List of loaded ships (iterator only)" },
	{"relicpower",		ENTITY_GAME_RELIC_POWER,		ENT_OBJECT,			"Relic of Power instance" },
	{"damagerelic",		ENTITY_GAME_RELIC_POWER,		ENT_OBJECT,			"Relic of Power instance" },
	{"relicknowledge",	ENTITY_GAME_RELIC_KNOWLEDGE,	ENT_OBJECT,			"Relic of Knowledge instance" },
	{"xprelic",			ENTITY_GAME_RELIC_KNOWLEDGE,	ENT_OBJECT,			"Relic of Knowledge instance" },
	{"reliclostsouls",	ENTITY_GAME_RELIC_LOSTSOULS,	ENT_OBJECT,			"Relic of Lost Souls instance" },
	{"pneumarelic",		ENTITY_GAME_RELIC_LOSTSOULS,	ENT_OBJECT,			"Relic of Lost Souls instance" },
	{"relichealth",		ENTITY_GAME_RELIC_HEALTH,		ENT_OBJECT,			"Relic of Health instance" },
	{"hprelic",			ENTITY_GAME_RELIC_HEALTH,		ENT_OBJECT,			"Relic of Health instance" },
	{"relicmagic",		ENTITY_GAME_RELIC_MAGIC,		ENT_OBJECT,			"Relic of Magic instance" },
	{"manarelic",		ENTITY_GAME_RELIC_MAGIC,		ENT_OBJECT,			"Relic of Maga instance" },
	{"reserved_mob",	ENTITY_GAME_RESERVED_MOBILE,	ENT_RESERVED_MOBILE,"Reserved mobile registry" },
	{"reserved_obj",	ENTITY_GAME_RESERVED_OBJECT,	ENT_RESERVED_OBJECT,"Reserved object registry" },
	{"reserved_room",	ENTITY_GAME_RESERVED_ROOM,		ENT_RESERVED_ROOM,	"Reserved room registry" },
	{NULL,				0,								ENT_UNKNOWN, 		NULL }
};

ENT_FIELD entity_persist[] = {
	{"mobiles",		ENTITY_PERSIST_MOBS,		ENT_PLLIST_MOB,		"List of persistent mobiles" },
	{"objects",		ENTITY_PERSIST_OBJS,		ENT_PLLIST_OBJ,		"List of persistent objects" },
	{"rooms",		ENTITY_PERSIST_ROOMS,		ENT_PLLIST_ROOM,	"List of persistent rooms" },
	{NULL,			0,							ENT_UNKNOWN,		 NULL }
};

ENT_FIELD entity_boolean[] = {
	{"true",		ENTITY_BOOLEAN_TRUE_FALSE,	ENT_STRING,		"Displays {Wtrue{x or {Wfalse{x" },
	{"yes",			ENTITY_BOOLEAN_YES_NO,		ENT_STRING,		"Displays {Wyes{x or {Wno{x" },
	{"on",			ENTITY_BOOLEAN_ON_OFF,		ENT_STRING,		"Displays {Won{x or {Woff{x" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_number[] = {
	{"abs",			ENTITY_NUM_ABS,				ENT_NUMBER,		"Absolute value of number" },
	{"padleft:N",		0,						ENT_STRING,		"Padded left for N characters wide"},
	{"padright:N",		0,						ENT_STRING,		"Padded right for N characters wide"},
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_string[] = {
	{"len",				ENTITY_STR_LEN,			ENT_NUMBER,		"Length of string" },
	{"length",			ENTITY_STR_LEN,			ENT_NUMBER,		"Length of string" },
	{"lower",			ENTITY_STR_LOWER,		ENT_STRING,		"String in lowercase" },
	{"upper",			ENTITY_STR_UPPER,		ENT_STRING,		"String in uppercase" },
	{"capital",			ENTITY_STR_CAPITAL,		ENT_STRING,		"String capitalized" },
	{"padleft:N",		0,						ENT_STRING,		"Padded left for N characters wide"},
	{"padright:N",		0,						ENT_STRING,		"Padded right for N characters wide"},
	{NULL,				0,						ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_widevnum[] = {
	{"area",			ENTITY_WNUM_AREA,				ENT_AREA,				"Area of widevnum" },
	{"vnum",			ENTITY_WNUM_VNUM,				ENT_NUMBER,				"Local number within the area" },
	{"mobindex",		ENTITY_WNUM_MOBINDEX,			ENT_MOBINDEX,			"MobIndex at this widevnum" },
	{"objindex",		ENTITY_WNUM_OBJINDEX,			ENT_OBJINDEX,			"ObjIndex at this widevnum" },
	{"tokenindex",		ENTITY_WNUM_TOKENINDEX,			ENT_TOKENINDEX,			"TokenIndex at this widevnum" },
	{"blueprint",		ENTITY_WNUM_BLUEPRINT,			ENT_BLUEPRINT,			"Blueprint at this widevnum" },
	{"bpsection",		ENTITY_WNUM_BLUEPRINT_SECTION,	ENT_BLUEPRINT_SECTION,	"Blueprint section at this widevnum" },
	{"dungeonindex",	ENTITY_WNUM_DUNGEONINDEX,		ENT_DUNGEONINDEX,		"DungeonIndex at this widevnum" },
	{"shipindex",		ENTITY_WNUM_SHIPINDEX,			ENT_SHIPINDEX,			"ShipIndex at this widevnum" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_mobile[] = {
	{"affects",					ENTITY_MOB_AFFECTS,					ENT_OLLIST_AFF,		"List of affects" },
	{"area",					ENTITY_MOB_AREA,					ENT_AREA,			"Area of mobile (NPC)" },
	{"bedroll",					ENTITY_MOB_FURNITURE,				ENT_OBJECT,			"Current furniture" },
	{"carrying",				ENTITY_MOB_CARRYING,				ENT_OLLIST_OBJ,		"Inventory" },
	{"cart",					ENTITY_MOB_CART,					ENT_OBJECT,			"Current object being pulled" },
	{"castspell",				ENTITY_MOB_CASTSPELL,				ENT_SKILL,			"Current skill being cast" },
	{"casttarget",				ENTITY_MOB_CASTTARGET,				ENT_STRING,			"Current cast target string" },
	{"casttoken",				ENTITY_MOB_CASTTOKEN,				ENT_TOKEN,			"Current cast token" },
	{"checkpoint",				ENTITY_MOB_CHECKPOINT,				ENT_ROOM,			"Current checkpoint" },
	{"church",					ENTITY_MOB_CHURCH,					ENT_CHURCH,			"Church" },
	{"class",					ENTITY_MOB_CLASS,					ENT_CLASSLEVEL,		"Current class level info" },
	{"classes",					ENTITY_MOB_CLASSES,					ENT_ILLIST_CLASSES,	"List of class levels" },
	{"clonerooms",				ENTITY_MOB_CLONEROOMS,				ENT_BLLIST_ROOM,	"Clone rooms associated with this mobile" },
	{"connection",				ENTITY_MOB_CONNECTION,				ENT_CONN,			"Connection linked to mobile" },
	{"eq_about",				ENTITY_MOB_EQ_ABOUT,				ENT_OBJECT,			"Object worn on about slot" },
	{"eq_ankle1",				ENTITY_MOB_EQ_ANKLE1,				ENT_OBJECT,			"Object worn on left ankle slot" },
	{"eq_ankle2",				ENTITY_MOB_EQ_ANKLE2,				ENT_OBJECT,			"Object worn on right ankle slot" },
	{"eq_arms",					ENTITY_MOB_EQ_ARMS,					ENT_OBJECT,			"Object worn on arms slot" },
	{"eq_back",					ENTITY_MOB_EQ_BACK,					ENT_OBJECT,			"Object worn on back slot" },
	{"eq_body",					ENTITY_MOB_EQ_BODY,					ENT_OBJECT,			"Object worn on body slot" },
	{"eq_concealed",			ENTITY_MOB_EQ_CONCEALED,			ENT_OBJECT,			"Object concealed from view" },
	{"eq_ear1",					ENTITY_MOB_EQ_EAR1,					ENT_OBJECT,			"Object worn on left ear slot" },
	{"eq_ear2",					ENTITY_MOB_EQ_EAR2,					ENT_OBJECT,			"Object worn on right ear slot" },
	{"eq_entangled",			ENTITY_MOB_EQ_ENTANGLED,			ENT_OBJECT,			"Object entangling mobile" },
	{"eq_eyes",					ENTITY_MOB_EQ_EYES,					ENT_OBJECT,			"Object worn over eyes slot" },
	{"eq_face",					ENTITY_MOB_EQ_FACE,					ENT_OBJECT,			"Object worn over face slot" },
	{"eq_feet",					ENTITY_MOB_EQ_FEET,					ENT_OBJECT,			"Object worn on feet slot" },
	{"eq_finger1",				ENTITY_MOB_EQ_FINGER1,				ENT_OBJECT,			"Object worn on left finger slot" },
	{"eq_finger2",				ENTITY_MOB_EQ_FINGER2,				ENT_OBJECT,			"Object worn on right finger slot" },
	{"eq_hands",				ENTITY_MOB_EQ_HANDS,				ENT_OBJECT,			"Object worn on hands slot" },
	{"eq_head",					ENTITY_MOB_EQ_HEAD,					ENT_OBJECT,			"Object worn on head slot" },
	{"eq_hold",					ENTITY_MOB_EQ_HOLD,					ENT_OBJECT,			"Object held" },
	{"eq_legs",					ENTITY_MOB_EQ_LEGS,					ENT_OBJECT,			"Object worn on legs slot" },
	{"eq_light",				ENTITY_MOB_EQ_LIGHT,				ENT_OBJECT,			"Object used as a light" },
	{"eq_lodged_arm1",			ENTITY_MOB_EQ_LODGED_ARM1,			ENT_OBJECT,			"Object lodged in left arm" },
	{"eq_lodged_arm2",			ENTITY_MOB_EQ_LODGED_ARM2,			ENT_OBJECT,			"Object lodged in right arm" },
	{"eq_lodged_head",			ENTITY_MOB_EQ_LODGED_HEAD,			ENT_OBJECT,			"Object lodged in head" },
	{"eq_lodged_leg1",			ENTITY_MOB_EQ_LODGED_LEG1,			ENT_OBJECT,			"Object lodged in left leg" },
	{"eq_lodged_leg2",			ENTITY_MOB_EQ_LODGED_LEG2,			ENT_OBJECT,			"Object lodged in right leg" },
	{"eq_lodged_torso",			ENTITY_MOB_EQ_LODGED_TORSO,			ENT_OBJECT,			"Object lodged in torso" },
	{"eq_neck1",				ENTITY_MOB_EQ_NECK1,				ENT_OBJECT,			"Object worn around first neck slot" },
	{"eq_neck2",				ENTITY_MOB_EQ_NECK2,				ENT_OBJECT,			"Object worn around second neck slot" },
	{"eq_ring",					ENTITY_MOB_EQ_RING,					ENT_OBJECT,			"Object worn on ring finger slot" },
	{"eq_shield",				ENTITY_MOB_EQ_SHIELD,				ENT_OBJECT,			"Object used as shield" },
	{"eq_shoulder",				ENTITY_MOB_EQ_SHOULDER,				ENT_OBJECT,			"Object worn over shoulder slot" },
	{"eq_tattoo_back",			ENTITY_MOB_EQ_TATTOO_BACK,			ENT_OBJECT,			"Object tattooed on back slot" },
	{"eq_tattoo_head",			ENTITY_MOB_EQ_TATTOO_HEAD,			ENT_OBJECT,			"Object tattooed on head slot" },
	{"eq_tattoo_neck",			ENTITY_MOB_EQ_TATTOO_NECK,			ENT_OBJECT,			"Object tattooed on neck slot" },
	{"eq_tattoo_lower_arm1",	ENTITY_MOB_EQ_TATTOO_LOWER_ARM1,	ENT_OBJECT,			"Object tattooed on left lower arm slot" },
	{"eq_tattoo_lower_arm2",	ENTITY_MOB_EQ_TATTOO_LOWER_ARM2,	ENT_OBJECT,			"Object tattooed on right lower arm slot" },
	{"eq_tattoo_lower_leg1",	ENTITY_MOB_EQ_TATTOO_LOWER_LEG1,	ENT_OBJECT,			"Object tattooed on left lower leg slot" },
	{"eq_tattoo_lower_leg2",	ENTITY_MOB_EQ_TATTOO_LOWER_LEG2,	ENT_OBJECT,			"Object tattooed on right lower leg slot" },
	{"eq_tattoo_shoulder1",		ENTITY_MOB_EQ_TATTOO_SHOULDER1,		ENT_OBJECT,			"Object tattooed on left shoulder slot" },
	{"eq_tattoo_shoulder2",		ENTITY_MOB_EQ_TATTOO_SHOULDER2,		ENT_OBJECT,			"Object tattooed on right shoulder slot" },
	{"eq_tattoo_torso",			ENTITY_MOB_EQ_TATTOO_TORSO,			ENT_OBJECT,			"Object tattooed on torso slot" },
	{"eq_tattoo_upper_arm1",	ENTITY_MOB_EQ_TATTOO_UPPER_ARM1,	ENT_OBJECT,			"Object tattooed on left upper arm slot" },
	{"eq_tattoo_upper_arm2",	ENTITY_MOB_EQ_TATTOO_UPPER_ARM2,	ENT_OBJECT,			"Object tattooed on right upper arm slot" },
	{"eq_tattoo_upper_leg1",	ENTITY_MOB_EQ_TATTOO_UPPER_LEG1,	ENT_OBJECT,			"Object tattooed on left upper leg slot" },
	{"eq_tattoo_upper_leg2",	ENTITY_MOB_EQ_TATTOO_UPPER_LEG2,	ENT_OBJECT,			"Object tattooed on right upper leg slot" },
	{"eq_waist",				ENTITY_MOB_EQ_WAIST,				ENT_OBJECT,			"Object worn around waist slot" },
	{"eq_wield1",				ENTITY_MOB_EQ_WIELD1,				ENT_OBJECT,			"Object wielded in main hand" },
	{"eq_wield2",				ENTITY_MOB_EQ_WIELD2,				ENT_OBJECT,			"Object wielded in off hand" },
	{"eq_wrist1",				ENTITY_MOB_EQ_WRIST1,				ENT_OBJECT,			"Object worn on left wrist slot" },
	{"eq_wrist2",				ENTITY_MOB_EQ_WRIST2,				ENT_OBJECT,			"Object worn on right wrist slot" },
	{"fulldesc",				ENTITY_MOB_FULLDESC,				ENT_STRING,			"Full description" },
	{"group",					ENTITY_MOB_GROUP,					ENT_GROUP,			"Group data" },
	{"he",						ENTITY_MOB_HE,						ENT_STRING,			"Third person subject pronoun" },
	{"him",						ENTITY_MOB_HIM,						ENT_STRING,			"Third person object pronoun" },
	{"himself",					ENTITY_MOB_HIMSELF,					ENT_STRING,			"Third person self pronoun" },
	{"his",						ENTITY_MOB_HIS,						ENT_STRING,			"Third person possessive pronoun" },
	{"hisobj",					ENTITY_MOB_HIS_O,					ENT_STRING,			"Third person possessive object pronoun" },
	{"home",					ENTITY_MOB_HOUSE,					ENT_ROOM,			"Home" },
	{"house",					ENTITY_MOB_HOUSE,					ENT_ROOM,			"Home" },
	{"hunting",					ENTITY_MOB_HUNTING,					ENT_MOBILE,			"Mobile currently hunting" },
	{"instrument",				ENTITY_MOB_INSTRUMENT,				ENT_OBJECT,			"Instrument used while playing music" },
	{"inv",						ENTITY_MOB_CARRYING,				ENT_OLLIST_OBJ,		"Inventory" },
	{"leader",					ENTITY_MOB_LEADER,					ENT_MOBILE,			"Leader of group" },
	{"long",					ENTITY_MOB_LONG,					ENT_STRING,			"Room description" },
	{"master",					ENTITY_MOB_MASTER,					ENT_MOBILE,			"Mobile currently following" },
	{"mount",					ENTITY_MOB_MOUNT,					ENT_MOBILE,			"Mobile currently riding" },
	{"name",					ENTITY_MOB_NAME,					ENT_STRING,			"Name" },
	{"next",					ENTITY_MOB_NEXT,					ENT_MOBILE,			"Next mobile in the room" },
	{"numgrouped",				ENTITY_MOB_NUMGROUPED,				ENT_NUMBER,			"Number of people grouped with mobile" },
	{"on",						ENTITY_MOB_FURNITURE,				ENT_OBJECT,			"Current furniture" },
	{"opponent",				ENTITY_MOB_OPPONENT,				ENT_MOBILE,			"Who the mobile is fighting" },
	{"owner",					ENTITY_MOB_OWNER,					ENT_MOBILE,			"Owner" },
	{"prey",					ENTITY_MOB_HUNTING,					ENT_MOBILE,			"Mobile currently hunting" },
	{"race",					ENTITY_MOB_RACE,					ENT_RACE,			"Race" },
	{"recall",					ENTITY_MOB_RECALL,					ENT_ROOM,			"Current recall point" },
	{"rider",					ENTITY_MOB_RIDER,					ENT_MOBILE,			"Mobile riding this mobile" },
	{"room",					ENTITY_MOB_ROOM,					ENT_ROOM,			"Current location" },
	{"sex",						ENTITY_MOB_SEX,						ENT_STAT,			"Sex" },
	{"short",					ENTITY_MOB_SHORT	,				ENT_STRING,			"Short descriptiong" },
	{"song",					ENTITY_MOB_SONG,					ENT_SKILLENTRY,		"Skill entry of song being played" },
	{"songtarget",				ENTITY_MOB_SONGTARGET,				ENT_STRING,			"Target of song being played" },
	{"songtoken",				ENTITY_MOB_SONGTOKEN,				ENT_TOKEN,			"Token of song being played" },
	{"target",					ENTITY_MOB_TARGET,					ENT_MOBILE,			"Currently remembered target" },
	{"tokens",					ENTITY_MOB_TOKENS,					ENT_OLLIST_TOK,		"List of tokens on mobile" },
	{"vars",					ENTITY_MOB_VARIABLES,				ENT_ILLIST_VARIABLE,"List of variables on mobile" },
	{"worn",					ENTITY_MOB_WORN,					ENT_PLLIST_OBJ,		"Worn objects" },
	{"index",					ENTITY_MOB_INDEX,					ENT_MOBINDEX,		"Index of mob (NPC)" },
	{"act",						ENTITY_MOB_ACT,						ENT_BITMATRIX,		"Act flags" },
	{"affected",				ENTITY_MOB_AFFECT,					ENT_BITMATRIX,		"Affect flags" },
	{"offense",					ENTITY_MOB_OFF,						ENT_BITVECTOR,		"Offense flags" },
	{"immune",					ENTITY_MOB_IMMUNE,					ENT_BITVECTOR,		"Immunity flags" },
	{"resist",					ENTITY_MOB_RESIST,					ENT_BITVECTOR,		"Resistance flags" },
	{"vuln",					ENTITY_MOB_VULN,					ENT_BITVECTOR,		"Vulnerability flags" },
	{"tempstring",				ENTITY_MOB_TEMPSTRING,				ENT_STRING,			"Temporary string for scripting" },
	{"pmount",					ENTITY_MOB_PMOUNT,					ENT_WIDEVNUM,		"Personal Mount (widevnum)" },
	{"catalystusage",			ENTITY_MOB_CATALYST_USAGE,			ENT_CATALYST_USAGE,	"Table for catalyst usage for various magical activities" },
	{"stache",					ENTITY_MOB_STACHE,					ENT_PLLIST_OBJ,		"List of stached objects" },
	{"size",					ENTITY_MOB_SIZE,					ENT_STAT,			"Size stat" },
	{"reputations",				ENTITY_MOB_REPUTATIONS,				ENT_ILLIST_REPUTATION,"List of reputations (iterator only)" },
	{"reputation",				ENTITY_MOB_REPUTATION,				ENT_REPUTATION,		"Reputation involved in the current trigger" },
	{"factions",				ENTITY_MOB_FACTIONS,				ENT_ILLIST_REPUTATION_INDEX,"List of factions (reputation indexes) (iterator only)" },
	{"missions",				ENTITY_MOB_MISSIONS,				ENT_ILLIST_MISSIONS,"List of active generated missions (iterator only)" },
	{"compartment",				ENTITY_MOB_COMPARTMENT,				ENT_COMPARTMENT,	"Current furniture compartment" },
	{"shop",					ENTITY_MOB_SHOP,					ENT_SHOP,			"Current shopkeeper data (NPC)" },
	{"crew",					ENTITY_MOB_CREW,					ENT_CREW,			"Current ship crew data (NPC)"},
	{NULL,						0,									ENT_UNKNOWN,		NULL }
};


ENT_FIELD entity_reputation[] = {
	{"name",			ENTITY_REPUTATION_NAME,			ENT_STRING,				"Name of reputation" },
	{"index",			ENTITY_REPUTATION_INDEX,		ENT_REPUTATION_INDEX,	"Index of reputation" },
	{"flags",			ENTITY_REPUTATION_FLAGS,		ENT_BITVECTOR,			"Reputation flags" },
	{"rank",			ENTITY_REPUTATION_RANK,			ENT_REPUTATION_RANK,	"Current rank in reputation" },
	{"maxrank",			ENTITY_REPUTATION_MAXRANK,		ENT_REPUTATION_RANK,	"Maximum attained rank in reputation earned" },
	{"paragon",			ENTITY_REPUTATION_PARAGON,		ENT_NUMBER,				"Paragon level" },
	{"reputation",		ENTITY_REPUTATION_REPUTATION,	ENT_NUMBER,				"Reputation points" },
	{"token",			ENTITY_REPUTATION_TOKEN,		ENT_TOKEN,				"Token used by reputation" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};


ENT_FIELD entity_reputation_index[] = {
	{"name",			ENTITY_REPINDEX_NAME,				ENT_STRING,					"Name of reputation" },
	{"wnum",			ENTITY_REPINDEX_WNUM,				ENT_WIDEVNUM,				"Widevnum of reputation" },
	{"flags",			ENTITY_REPINDEX_FLAGS,				ENT_BITVECTOR,				"Reputation flags" },
	{"desc",			ENTITY_REPINDEX_DESCRIPTION,		ENT_STRING,					"Description" },
	{"description",		ENTITY_REPINDEX_DESCRIPTION,		ENT_STRING,					"Description" },
	{"info",			ENTITY_REPINDEX_DESCRIPTION,		ENT_STRING,					"Description" },
	{"comments",		ENTITY_REPINDEX_COMMENTS,			ENT_STRING,					"Builder's comments" },
	{"builder",			ENTITY_REPINDEX_COMMENTS,			ENT_STRING,					"Builder's comments" },
	{"initial_rank",	ENTITY_REPINDEX_INITIAL_RANK,		ENT_REPUTATION_RANK,		"Starting rank" },
	{"initial_rep",		ENTITY_REPINDEX_INITIAL_REPUTATION,	ENT_NUMBER,					"Starting reputation points" },
	{"ranks",			ENTITY_REPINDEX_RANKS,				ENT_PLLIST_REPUTATION_RANK,	"List of ranks" },
	{"token",			ENTITY_REPINDEX_TOKEN,				ENT_TOKENINDEX,				"Token index used by reputation" },
	{NULL,				0,									ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_reputation_rank[] = {
	{"name",			ENTITY_REPRANK_NAME,		ENT_STRING,		"Name" },
	{"desc",			ENTITY_REPRANK_DESCRIPTION,	ENT_STRING,		"Description" },
	{"description",		ENTITY_REPRANK_DESCRIPTION,	ENT_STRING,		"Description" },
	{"info",			ENTITY_REPRANK_DESCRIPTION,	ENT_STRING,		"Description" },
	{"comments",		ENTITY_REPRANK_COMMENTS,	ENT_STRING,		"Builder's comments" },
	{"builder",			ENTITY_REPRANK_COMMENTS,	ENT_STRING,		"Builder's comments" },
	{"color",			ENTITY_REPRANK_COLOR,		ENT_STRING,		"Color of rank" },
	{"flags",			ENTITY_REPRANK_FLAGS,		ENT_BITVECTOR,	"Rank flags" },
	{"uid",				ENTITY_REPRANK_UID,			ENT_NUMBER,		"Rank UID" },
	{"ordinal",			ENTITY_REPRANK_ORDINAL,		ENT_NUMBER,		"Position of rank in list" },
	{"capacity",		ENTITY_REPRANK_CAPACITY,	ENT_NUMBER,		"Number of points in rank" },
	{NULL,				0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object[] = {
	{"affects",			ENTITY_OBJ_AFFECTS,					ENT_OLLIST_AFF,				"List of affects on object" },
	{"area",			ENTITY_OBJ_AREA,					ENT_AREA,					"Area of object" },
	{"carrier",			ENTITY_OBJ_CARRIER,					ENT_MOBILE,					"Mobile carrying the object" },
	{"clonerooms",		ENTITY_OBJ_CLONEROOMS,				ENT_BLLIST_ROOM,			"Clone rooms associated with object" },
	{"container",		ENTITY_OBJ_CONTAINER,				ENT_OBJECT,					"Object containing object" },
	{"contents",		ENTITY_OBJ_CONTENTS,				ENT_OLLIST_OBJ,				"Contents of object" },
	{"ed",				ENTITY_OBJ_EXTRADESC,				ENT_EXTRADESC,				"Extra description registry" },
	{"fulldesc",		ENTITY_OBJ_FULLDESC,				ENT_STRING,					"Full description" },
	{"in",				ENTITY_OBJ_CONTAINER,				ENT_OBJECT,					"Object containing object" },
	{"inv",				ENTITY_OBJ_CONTENTS,				ENT_OLLIST_OBJ,				"Contents of object" },
	{"items",			ENTITY_OBJ_CONTENTS,				ENT_OLLIST_OBJ,				"Contents of object" },
	{"long",			ENTITY_OBJ_LONG,					ENT_STRING,					"Room description" },
	{"name",			ENTITY_OBJ_NAME,					ENT_STRING,					"Keywords of object" },
	{"next",			ENTITY_OBJ_NEXT,					ENT_OBJECT,					"Next object in list" },
	{"on",				ENTITY_OBJ_FURNITURE,				ENT_OBJECT,					"Furniture object is on (disabled)" },
	{"owner",			ENTITY_OBJ_OWNER,					ENT_STRING,					"Mobile owner of object" },
	{"room",			ENTITY_OBJ_ROOM,					ENT_ROOM,					"Room description" },
	{"short",			ENTITY_OBJ_SHORT,					ENT_STRING,					"Short description" },
	{"target",			ENTITY_OBJ_TARGET,					ENT_MOBILE,					"Currently remembered target" },
	{"tokens",			ENTITY_OBJ_TOKENS,					ENT_OLLIST_TOK,				"List of tokens" },
	{"user",			ENTITY_OBJ_CARRIER,					ENT_MOBILE,					"Mobile carrying the object" },
	{"wearer",			ENTITY_OBJ_CARRIER,					ENT_MOBILE,					"Mobile carrying the object" },
	{"vars",			ENTITY_OBJ_VARIABLES,				ENT_ILLIST_VARIABLE,		"List of variables" },
	{"index",			ENTITY_OBJ_INDEX,					ENT_OBJINDEX,				"Index of object" },
	{"extra",			ENTITY_OBJ_EXTRA,					ENT_BITMATRIX,				"Extra flags" },
	{"wear",			ENTITY_OBJ_WEAR,					ENT_BITVECTOR,				"Wear flags" },
	{"ship",			ENTITY_OBJ_SHIP,					ENT_SHIP,					"Ship data" },
	{"stache",			ENTITY_OBJ_STACHE,					ENT_PLLIST_OBJ,				"List of objects stached" },
	{"islockered",		ENTITY_OBJ_ISLOCKERED,				ENT_BOOLEAN,				"Is the object lockered" },
	{"isstached",		ENTITY_OBJ_ISSTACHED,				ENT_BOOLEAN,				"Is the object stached" },
	{"material",		ENTITY_OBJ_MATERIAL,				ENT_MATERIAL,				"Object material" },
	{"class",			ENTITY_OBJ_CLASS,					ENT_CLASS,					"Class restriction" },
	{"classtype",		ENTITY_OBJ_CLASSTYPE,				ENT_STAT,					"Class type restriction" },
	{"race",			ENTITY_OBJ_RACE,					ENT_RACE,					"Racial restriction" },
	{"ammo_data",		ENTITY_OBJ_TYPE_AMMO,				ENT_OBJECT_AMMO,			"Ammo multitype data" },
	{"armor_data",		ENTITY_OBJ_TYPE_ARMOR,				ENT_OBJECT_ARMOR,			"Armor multitype data" },
	{"book_page",		ENTITY_OBJ_TYPE_PAGE,				ENT_BOOK_PAGE,				"Page multitype data" },
	{"book_data",		ENTITY_OBJ_TYPE_BOOK,				ENT_OBJECT_BOOK,			"Book multitype data" },
	{"cart_data",		ENTITY_OBJ_TYPE_CART,				ENT_OBJECT_CART,			"Cart multitype data" },
	{"compass_data",	ENTITY_OBJ_TYPE_COMPASS,			ENT_OBJECT_COMPASS,			"Compass multitype data" },
	{"container_data",	ENTITY_OBJ_TYPE_CONTAINER,			ENT_OBJECT_CONTAINER,		"Container multitype data" },
	{"fluid_data",		ENTITY_OBJ_TYPE_FLUID_CONTAINER,	ENT_OBJECT_FLUID_CONTAINER,	"Fluid Container multitype data" },
	{"food_data",		ENTITY_OBJ_TYPE_FOOD,				ENT_OBJECT_FOOD,			"Food multitype data" },
	{"furniture_data",	ENTITY_OBJ_TYPE_FURNITURE,			ENT_OBJECT_FURNITURE,		"Furniture multitype data" },
	{"ink_data",		ENTITY_OBJ_TYPE_INK,				ENT_OBJECT_INK,				"Ink multitype data" },
	{"instrument_data",	ENTITY_OBJ_TYPE_INSTRUMENT,			ENT_OBJECT_INSTRUMENT,		"Instrument multitype data" },
	{"jewelry_data",	ENTITY_OBJ_TYPE_JEWELRY,			ENT_OBJECT_JEWELRY,			"Jewelry multitype data" },
	{"light_data",		ENTITY_OBJ_TYPE_LIGHT,				ENT_OBJECT_LIGHT,			"Light multitype data" },
	{"map_data",		ENTITY_OBJ_TYPE_MAP,				ENT_OBJECT_MAP,				"Map multitype data" },
	{"mist_data",		ENTITY_OBJ_TYPE_MIST,				ENT_OBJECT_MIST,			"Mist multitype data" },
	{"money_data",		ENTITY_OBJ_TYPE_MONEY,				ENT_OBJECT_MONEY,			"Money multitype data" },
	{"portal_data",		ENTITY_OBJ_TYPE_PORTAL,				ENT_OBJECT_PORTAL,			"Portal multitype data" },
	{"scroll_data",		ENTITY_OBJ_TYPE_SCROLL,				ENT_OBJECT_SCROLL,			"Scroll multitype data" },
	{"sextant_data",	ENTITY_OBJ_TYPE_SEXTANT,			ENT_OBJECT_SEXTANT,			"Sextant multitype data" },
	{"tattoo_data",		ENTITY_OBJ_TYPE_TATTOO,				ENT_OBJECT_TATTOO,			"Tattoo multitype data" },
	{"telescope_data",	ENTITY_OBJ_TYPE_TELESCOPE,			ENT_OBJECT_TELESCOPE,		"Telescope multitype data" },
	{"wand_data",		ENTITY_OBJ_TYPE_WAND,				ENT_OBJECT_WAND,			"Wand multitype data" },
	{"weapon_data",		ENTITY_OBJ_TYPE_WEAPON,				ENT_OBJECT_WEAPON,			"Weapon multitype data" },
	{NULL,				0,									ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_object_ammo[] = {
	{"type",		ENTITY_OBJ_AMMO_TYPE,			ENT_STAT,		"Type of ammo" },
	{"attack",		ENTITY_OBJ_AMMO_DAMAGE_TYPE,	ENT_ATTACK,		"Damage type" },
	{"flags",		ENTITY_OBJ_AMMO_FLAGS,			ENT_BITVECTOR,	"Weapon flags" },
	{"damage",		ENTITY_OBJ_AMMO_DAMAGE,			ENT_DICE,		"Damage dice" },
	{"msgbreak",	ENTITY_OBJ_AMMO_MESSAGE_BREAK,	ENT_STRING,		"Message when ammo {Ybreaks{x" },
	{NULL,			0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_armor[] = {
	{"type",			ENTITY_OBJ_ARMOR_TYPE,			ENT_STAT,				"Armor type" },
	{"strength",		ENTITY_OBJ_ARMOR_STRENGTH,		ENT_STAT,				"Strength of the armor" },
	{"protections",		ENTITY_OBJ_ARMOR_PROTECTIONS,	ENT_ARMOR_PROTECTIONS,	"Protection table" },
	{"adornments",		ENTITY_OBJ_ARMOR_ADORNMENTS,	ENT_ADORNMENTS,			"Adornments" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_adornments[] = {
	{"max",				ENTITY_ADORNMENTS_MAX,			ENT_NUMBER,			"Maximum number adornments allowed" },
	{"[N]",				0,								ENT_ADORNMENT,		"Accesses the Nth adornment slot" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_adornment[] = {
	{"type",			ENTITY_ADORNMENT_TYPE,			ENT_STAT,		"Type of adornment" },
	{"name",			ENTITY_ADORNMENT_NAME,			ENT_STRING,		"Name" },
	{"short",			ENTITY_ADORNMENT_SHORT,			ENT_STRING,		"Short description" },
	{"description",		ENTITY_ADORNMENT_DESCRIPTION,	ENT_STRING,		"Description" },
	{"spell",			ENTITY_ADORNMENT_SPELL,			ENT_SPELL,		"Spell on adornment" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_book[] = {
	{"name",				ENTITY_OBJ_BOOK_NAME,		ENT_STRING,				"Name of book" },
	{"short",				ENTITY_OBJ_BOOK_SHORT,		ENT_STRING,				"Short of book" },
	{"flags",				ENTITY_OBJ_BOOK_FLAGS,		ENT_BITVECTOR,			"Book flags" },
	{"current",				ENTITY_OBJ_BOOK_CURRENT,	ENT_BOOK_PAGE,			"Current page" },
	{"opener",				ENTITY_OBJ_BOOK_OPENER,		ENT_BOOK_PAGE,			"Initial page when opened" },
	{"pages",				ENTITY_OBJ_BOOK_PAGES,		ENT_PLLIST_BOOK_PAGE,	"List of pages" },
	{"lock",				ENTITY_OBJ_BOOK_LOCK,		ENT_LOCK_STATE,			"Lock for book" },
	{NULL,					0,							ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_object_cart[] = {
	{"flags",				ENTITY_OBJ_CART_FLAGS,			ENT_BITVECTOR,	"Cart flags" },
	{"minstrength",			ENTITY_OBJ_CART_MIN_STRENGTH,	ENT_NUMBER,		"Minimum group strength needed to move the cart" },
	{"movedelay",			ENTITY_OBJ_CART_MOVE_DELAY,		ENT_NUMBER,		"Movement delay when moving the cart" },
	{NULL,					0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_compass[] = {
	{"accuracy",	ENTITY_OBJ_COMPASS_ACCURACY, 		ENT_NUMBER,		"Accuracy" },
	{"wilds",		ENTITY_OBJ_COMPASS_WILDS,			ENT_WILDS,		"Wilderness of target" },
	{"x",			ENTITY_OBJ_COMPASS_X,				ENT_NUMBER,		"X Coordinate of target" },
	{"y",			ENTITY_OBJ_COMPASS_Y,				ENT_NUMBER,		"Y Coordinate of target" },
	{"target",		ENTITY_OBJ_COMPASS_TARGET,			ENT_ROOM,		"Target room" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_container[] = {
	{"name",				ENTITY_OBJ_CONTAINER_NAME,				ENT_STRING,			"Name of container" },
	{"short",				ENTITY_OBJ_CONTAINER_SHORT,				ENT_STRING,			"Short of container" },
	{"flags",				ENTITY_OBJ_CONTAINER_FLAGS,				ENT_BITVECTOR,		"Container flags" },
	{"max_weight",			ENTITY_OBJ_CONTAINER_MAX_WEIGHT,		ENT_NUMBER,			"Maximum weight capacity" },
	{"weight_multiplier",	ENTITY_OBJ_CONTAINER_WEIGHT_MULTIPLIER,	ENT_NUMBER,			"Weight multiplier" },
	{"max_volume",			ENTITY_OBJ_CONTAINER_MAX_VOLUME,		ENT_NUMBER,			"Maximum volume capacity" },
	{"lock",				ENTITY_OBJ_CONTAINER_LOCK,				ENT_LOCK_STATE,		"Lock on container" },
	{NULL,					0,										ENT_UNKNOWN,		 NULL }
};

ENT_FIELD entity_object_fluid_container[] = {
	{"name",				ENTITY_OBJ_FLUID_CONTAINER_NAME,		ENT_STRING,			"Name of fluid container" },
	{"short",				ENTITY_OBJ_FLUID_CONTAINER_SHORT,		ENT_STRING,			"Short of fluid container" },
	{"flags",				ENTITY_OBJ_FLUID_CONTAINER_FLAGS,		ENT_BITVECTOR,		"Fluid container flags" },
	{"liquid",				ENTITY_OBJ_FLUID_CONTAINER_LIQUID,		ENT_LIQUID,			"Liquid in fluid container" },
	{"amount",				ENTITY_OBJ_FLUID_CONTAINER_AMOUNT,		ENT_NUMBER,			"Current amount of liquid" },
	{"capacity",			ENTITY_OBJ_FLUID_CONTAINER_CAPACITY,	ENT_NUMBER,			"Maximum amount of liquid" },
	{"refillrate",			ENTITY_OBJ_FLUID_CONTAINER_REFILL_RATE,	ENT_NUMBER,			"Refill rate per tick" },
	{"poison",				ENTITY_OBJ_FLUID_CONTAINER_POISON,		ENT_NUMBER,			"How poisoned is the liquid" },
	{"poisonrate",			ENTITY_OBJ_FLUID_CONTAINER_POISON_RATE,	ENT_NUMBER,			"Poisoning rate per tick" },
	{"spells",				ENTITY_OBJ_FLUID_CONTAINER_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells for potion" },
	{"lock",				ENTITY_OBJ_FLUID_CONTAINER_LOCK,		ENT_LOCK_STATE,		"Lock on fluid container" },
	{NULL,					0,										ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_food[] = {
	{"hunger",		ENTITY_OBJ_FOOD_HUNGER,	ENT_NUMBER,				"Hunger replenished" },
	{"full",		ENTITY_OBJ_FOOD_FULL,	ENT_NUMBER,				"Fullness replenished" },
	{"poison",		ENTITY_OBJ_FOOD_POISON,	ENT_NUMBER,				"How poisoned is it" },
	{"buffs",		ENTITY_OBJ_FOOD_BUFFS,	ENT_PLLIST_FOOD_BUFF,	"List of food buffs" },
	{NULL,			0,						ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_object_furniture[] = {
	{"main",		ENTITY_OBJ_FURNITURE_MAIN_COMPARTMENT,	ENT_COMPARTMENT,		"Main compartment" },
	{"flags",		ENTITY_OBJ_FURNITURE_FLAGS,				ENT_BITVECTOR,			"Furniture flags" },
	{"compartments",ENTITY_OBJ_FURNITURE_COMPARTMENTS,		ENT_PLLIST_COMPARTMENT, "List of compartments" },
	{NULL,			0,										ENT_UNKNOWN,			NULL },
};

ENT_FIELD entity_object_ink[] = {
	{"types",		ENTITY_OBJ_INK_TYPES,			ENT_INK_TYPES,		"Ink types" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_instrument[] = {
	{"type",		ENTITY_OBJ_INSTRUMENT_TYPE,			ENT_STAT,					"Type of instrument" },
	{"flags",		ENTITY_OBJ_INSTRUMENT_FLAGS,		ENT_BITVECTOR,				"Instrument flags" },
	{"beats",		ENTITY_OBJ_INSTRUMENT_BEATS,		ENT_RANGE,					"Beats range" },
	{"mana",		ENTITY_OBJ_INSTRUMENT_MANA,			ENT_RANGE,					"Mana range" },
	{"reservoirs",	ENTITY_OBJ_INSTRUMENT_RESERVOIRS,	ENT_INSTRUMENT_RESERVOIRS,	"Reservoirs of catalysts" },
	{NULL,			0,									ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_object_jewelry[] = {
	{"maxmana",		ENTITY_OBJ_JEWELRY_MAXMANA,		ENT_NUMBER,			"Maximum mana allowed for imbuing" },
	{"spells",		ENTITY_OBJ_JEWELRY_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_light[] = {
	{"duration",	ENTITY_OBJ_LIGHT_DURATION,	ENT_NUMBER,		"Duration of light" },
	{"flags",		ENTITY_OBJ_LIGHT_FLAGS,		ENT_BITVECTOR,	"Light flags" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_map[] = {
	{"wilds",		ENTITY_OBJ_MAP_WILDS,		ENT_WILDS,				"Wilds of map target" },
	{"x",			ENTITY_OBJ_MAP_X,			ENT_NUMBER,				"X coordinate of map target" },
	{"y",			ENTITY_OBJ_MAP_Y,			ENT_NUMBER,				"Y coordinate of map target" },
	{"target",		ENTITY_OBJ_MAP_TARGET,		ENT_ROOM,				"Target room" },
	{"waypoints",	ENTITY_OBJ_MAP_WAYPOINTS,	ENT_ILLIST_WAYPOINTS,	"List of map waypoints" },
	{NULL,			0,							ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_object_mist[] = {
	{"mobiles",		ENTITY_OBJ_MIST_OBSCURE_MOBILES,	ENT_NUMBER,		"Chance of obscuring each mobile in room" },
	{"objects",		ENTITY_OBJ_MIST_OBSCURE_OBJECTS,	ENT_NUMBER,		"Chance of obscuring each object in room" },
	{"room",		ENTITY_OBJ_MIST_OBSCURE_ROOM,		ENT_NUMBER,		"Chance of obscuring the room from outside" },
	{"icy",			ENTITY_OBJ_MIST_ICY,				ENT_NUMBER,		"Chance of getting icy effect" },
	{"fiery",		ENTITY_OBJ_MIST_FIERY,				ENT_NUMBER,		"Chance of getting fiery effect" },
	{"acidic",		ENTITY_OBJ_MIST_ACIDIC,				ENT_NUMBER,		"Chance of damagine weapons (not implemented)" },
	{"stink",		ENTITY_OBJ_MIST_STINK,				ENT_NUMBER,		"Chance of causing mobiles to flee" },
	{"wither",		ENTITY_OBJ_MIST_WITHER,				ENT_NUMBER,		"Chance of doing nasty stuff to mobiles" },
	{"toxic",		ENTITY_OBJ_MIST_TOXIC,				ENT_NUMBER,		"Chance of inflicting toxic fumes" },
	{"shock",		ENTITY_OBJ_MIST_SHOCK,				ENT_NUMBER,		"Chance of shocking mobiles" },
	{"fog",			ENTITY_OBJ_MIST_FOG,				ENT_NUMBER,		"Chance of corroding objects" },
	{"sleep",		ENTITY_OBJ_MIST_SLEEP,				ENT_NUMBER,		"Chance of putting mobiles to sleep" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_money[] = {
	{"silver",		ENTITY_OBJ_MONEY_SILVER,	ENT_NUMBER,		"Silver coins" },
	{"gold",		ENTITY_OBJ_MONEY_GOLD,		ENT_NUMBER,		"Gold coins" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_portal[] = {
	{"name",		ENTITY_OBJ_PORTAL_NAME,			ENT_STRING,			"Name of portal" },
	{"short",		ENTITY_OBJ_PORTAL_SHORT,		ENT_STRING,			"Short of portal" },
	{"flags",		ENTITY_OBJ_PORTAL_FLAGS,		ENT_BITVECTOR,		"Portal flags" },
	{"exit",		ENTITY_OBJ_PORTAL_EXIT,			ENT_BITVECTOR,		"Exit flags" },
	{"type",		ENTITY_OBJ_PORTAL_TYPE,			ENT_NUMBER,			"Portal type" },
	{"room",		ENTITY_OBJ_PORTAL_DESTINATION,	ENT_ROOM,			"Destination" },
	{"target",		ENTITY_OBJ_PORTAL_DESTINATION,	ENT_ROOM,			"Destination" },
	{"destination",	ENTITY_OBJ_PORTAL_DESTINATION,	ENT_ROOM,			"Destination" },
	{"param0",		ENTITY_OBJ_PORTAL_PARAM0,		ENT_NUMBER,			"Portal parameter 0" },
	{"param1",		ENTITY_OBJ_PORTAL_PARAM1,		ENT_NUMBER,			"Portal parameter 1" },
	{"param2",		ENTITY_OBJ_PORTAL_PARAM2,		ENT_NUMBER,			"Portal parameter 2" },
	{"param3",		ENTITY_OBJ_PORTAL_PARAM3,		ENT_NUMBER,			"Portal parameter 3" },
	{"param4",		ENTITY_OBJ_PORTAL_PARAM4,		ENT_NUMBER,			"Portal parameter 4" },
	{"lock",		ENTITY_OBJ_PORTAL_LOCK,			ENT_LOCK_STATE,		"Lock on portal" },
	{"spells",		ENTITY_OBJ_PORTAL_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells done upon entry" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_scroll[] = {
	{"maxmana",		ENTITY_OBJ_SCROLL_MAXMANA,		ENT_NUMBER,			"Maximum mana for scribing" },
	{"spells",		ENTITY_OBJ_SCROLL_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_sextant[] = {
	{"accuracy",	ENTITY_OBJ_SEXTANT_ACCURACY,	ENT_NUMBER,			"Accuracy" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_tattoo[] = {
	{"touches",		ENTITY_OBJ_TATTOO_TOUCHES,		ENT_NUMBER,			"Number of touches remaining" },
	{"fade",		ENTITY_OBJ_TATTOO_FADE,			ENT_NUMBER,			"Chance of fading when touched" },
	{"fading",		ENTITY_OBJ_TATTOO_FADING,		ENT_NUMBER,			"Increase in fading chance per touch" },
	{"spells",		ENTITY_OBJ_TATTOO_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_telescope[] = {
	{"distance",	ENTITY_OBJ_TELESCOPE_DISTANCE,		ENT_NUMBER,		"Current distance" },
	{"min",			ENTITY_OBJ_TELESCOPE_MIN_DISTANCE,	ENT_NUMBER,		"Minimum distance" },
	{"max",			ENTITY_OBJ_TELESCOPE_MAX_DISTANCE,	ENT_NUMBER,		"Maximum distance" },
	{"bonusview",	ENTITY_OBJ_TELESCOPE_BONUS_VIEW,	ENT_NUMBER,		"Bonus view of wilds" },
	{"heading",		ENTITY_OBJ_TELESCOPE_HEADING,		ENT_NUMBER,		"Current heading when on the ground" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_object_wand[] = {
	{"charges",		ENTITY_OBJ_WAND_CHARGES,		ENT_NUMBER,			"Number of charges remaining" },
	{"maxcharges",	ENTITY_OBJ_WAND_MAXCHARGES,		ENT_NUMBER,			"Total number of charges available" },
	{"cooldown",	ENTITY_OBJ_WAND_COOLDOWN,		ENT_NUMBER,			"Current recharge cooldown" },
	{"recharge",	ENTITY_OBJ_WAND_RECHARGE,		ENT_NUMBER,			"Recharge rate" },
	{"maxmana",		ENTITY_OBJ_WAND_MAXMANA,		ENT_NUMBER,			"Maximum mana for imbuing" },
	{"spells",		ENTITY_OBJ_WAND_SPELLS,			ENT_ILLIST_SPELLS,	"List of spells" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_object_weapon[] = {
	{"class",		ENTITY_OBJ_WEAPON_CLASS,		ENT_STAT,			"Type of weapon" },
	{"attacks",		ENTITY_OBJ_WEAPON_ATTACKS,		ENT_WEAPON_ATTACKS,	"Attack points" },
	{"ammo",		ENTITY_OBJ_WEAPON_AMMO,			ENT_STAT,			"Type of projectile" },
	{"range",		ENTITY_OBJ_WEAPON_RANGE,		ENT_NUMBER,			"Maximum distance for projectiles" },
	{"charges",		ENTITY_OBJ_WEAPON_CHARGES,		ENT_NUMBER,			"Number of charges remaining" },
	{"maxcharges",	ENTITY_OBJ_WEAPON_MAXCHARGES,	ENT_NUMBER,			"Total number of charges" },
	{"cooldown",	ENTITY_OBJ_WEAPON_COOLDOWN,		ENT_NUMBER,			"Current recharge cooldown" },
	{"recharge",	ENTITY_OBJ_WEAPON_RECHARGE,		ENT_NUMBER,			"Recharge rate" },
	{"maxmana",		ENTITY_OBJ_WEAPON_MAXMANA,		ENT_NUMBER,			"Maximum mana for imbuing" },
	{"spells",		ENTITY_OBJ_WEAPON_SPELLS,		ENT_ILLIST_SPELLS,	"List of spells" },
	{NULL,			0,								ENT_UNKNOWN	}
};

ENT_FIELD entity_room[] =
{
	{"area",		ENTITY_ROOM_AREA,			ENT_AREA,			"Area of room" },
	{"clonerooms",	ENTITY_ROOM_CLONEROOMS,		ENT_BLLIST_ROOM,	"Clone rooms associated with room" },
	{"clones",		ENTITY_ROOM_CLONES,			ENT_BLLIST_ROOM,	"Clone rooms associated with room" },
	{"desc",		ENTITY_ROOM_DESC,			ENT_STRING,			"Full description" },
	{"down",		ENTITY_ROOM_DOWN,			ENT_EXIT,			"Down exit" },
	{"east",		ENTITY_ROOM_EAST,			ENT_EXIT,			"East exit" },
	{"ed",			ENTITY_ROOM_EXTRADESC,		ENT_EXTRADESC,		"Extra description registry" },
	{"env_mob",		ENTITY_ROOM_ENVIRON_MOB,	ENT_MOBILE,			"Environment owner of clone room" },
	{"env_obj",		ENTITY_ROOM_ENVIRON_OBJ,	ENT_OBJECT,			"Environment owner of clone room" },
	{"env_room",	ENTITY_ROOM_ENVIRON_ROOM,	ENT_ROOM,			"Environment owner of clone room" },
	{"env_token",	ENTITY_ROOM_ENVIRON_TOKEN,	ENT_TOKEN,			"Environment owner of clone room" },
	{"environ",		ENTITY_ROOM_ENVIRON,		ENT_ROOM,			"Environment outside room" },
	{"environment",	ENTITY_ROOM_ENVIRON,		ENT_ROOM,			"Environment outside room" },
	{"exits",		ENTITY_ROOM_EXITS,			ENT_ARRAY_EXITS,	"Array of exits" },
	{"extern",		ENTITY_ROOM_ENVIRON,		ENT_ROOM,			"Environment outside room" },
	{"flags",		ENTITY_ROOM_FLAGS,			ENT_BITMATRIX,		"Room flags" },
	{"mobiles",		ENTITY_ROOM_MOBILES,		ENT_OLLIST_MOB,		"People in room" },
	{"name",		ENTITY_ROOM_NAME,			ENT_STRING,			"Name of room" },
	{"north",		ENTITY_ROOM_NORTH,			ENT_EXIT,			"North exit" },
	{"northeast",	ENTITY_ROOM_NORTHEAST,		ENT_EXIT,			"Northeast exit" },
	{"northwest",	ENTITY_ROOM_NORTHWEST,		ENT_EXIT,			"Northwest exit" },
	{"objects",		ENTITY_ROOM_OBJECTS,		ENT_OLLIST_OBJ,		"Contents" },
	{"outside",		ENTITY_ROOM_ENVIRON,		ENT_ROOM,			"Environment outside room" },
	{"region",		ENTITY_ROOM_REGION,			ENT_AREA_REGION,	"Area region for room" },
	{"sector",		ENTITY_ROOM_SECTOR,			ENT_SECTOR,			"Terrain sector" },
	{"sectorflags",	ENTITY_ROOM_SECTORFLAGS,	ENT_BITVECTOR,		"Active terrain sector flags"},
	{"south",		ENTITY_ROOM_SOUTH,			ENT_EXIT,			"South exit" },
	{"southeast",	ENTITY_ROOM_SOUTHEAST,		ENT_EXIT,			"Southeast exit" },
	{"southwest",	ENTITY_ROOM_SOUTHWEST,		ENT_EXIT,			"Southwest exit" },
	{"target",		ENTITY_ROOM_TARGET,			ENT_MOBILE,			"Current remembered target" },
	{"tokens",		ENTITY_ROOM_TOKENS,			ENT_OLLIST_TOK,		"List of tokens" },
	{"up",			ENTITY_ROOM_UP,				ENT_EXIT,			"Up exit" },
	{"west",		ENTITY_ROOM_WEST,			ENT_EXIT,			"West exit" },
	{"wilds",		ENTITY_ROOM_WILDS,			ENT_WILDS,			"Wilderness linked to room" },
	{"vars",		ENTITY_ROOM_VARIABLES,		ENT_ILLIST_VARIABLE,"List of variables" },
	{"section",		ENTITY_ROOM_SECTION,		ENT_SECTION,		"Instance section associated with room" },
	{"instance",	ENTITY_ROOM_INSTANCE,		ENT_INSTANCE,		"Instance associated with room" },
	{"dungeon",		ENTITY_ROOM_DUNGEON,		ENT_DUNGEON,		"Dungeon associated with room" },
	{"ship",		ENTITY_ROOM_SHIP,			ENT_SHIP,			"Ship associated with room" },
	{NULL,			0,							ENT_UNKNOWN	}
};

ENT_FIELD entity_array_exits[] = {
	{"random",		ENTITY_ARRAY_EXITS_RANDOM,	ENT_EXIT,			"Random exit" },
	{"any",			ENTITY_ARRAY_EXITS_ANY,		ENT_EXIT,			"Random valid exit" },
	{"open",		ENTITY_ARRAY_EXITS_OPEN,	ENT_EXIT,			"Random open exit" },
	{"[N]",			0,							ENT_EXIT,			"Get particular exit at index N (1-10)"},
	{NULL,			0,							ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_sector[] = {
	{"name",			ENTITY_SECTOR_NAME,			ENT_STRING,		"Name of sector" },
	{"description",		ENTITY_SECTOR_DESCRIPTION,	ENT_STRING,		"Description" },
	{"comments",		ENTITY_SECTOR_COMMENTS,		ENT_STRING,		"Builder's comments" },
	{"class",			ENTITY_SECTOR_CLASS,		ENT_STAT,		"Type of sector" },
	{"flags",			ENTITY_SECTOR_FLAGS,		ENT_BITVECTOR,	"Sector flags" },
	{"move_cost",		ENTITY_SECTOR_MOVE_COST,	ENT_NUMBER,		"Movement cost" },
	{"hp_regen",		ENTITY_SECTOR_HP_REGEN,		ENT_NUMBER,		"Health regen rate" },
	{"mana_regen",		ENTITY_SECTOR_MANA_REGEN,	ENT_NUMBER,		"Mana regen rate" },
	{"move_regen",		ENTITY_SECTOR_MOVE_REGEN,	ENT_NUMBER,		"Move regen rate" },
	{"soil",			ENTITY_SECTOR_SOIL,			ENT_NUMBER,		"Soil availability" },
	{"affinity",		ENTITY_SECTOR_AFFINITY,		ENT_AFFINITIES,	"Elemental affinity table" },
	{"hidemsgs",		ENTITY_SECTOR_HIDEMSGS,		ENT_PLLIST_STR,	"List of hide messages" },
	{NULL,				0,							ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_affinity[] = {
	{"type",			ENTITY_AFFINITY_TYPE,		ENT_STAT,			"Type of element" },
	{"size",			ENTITY_AFFINITY_SIZE,		ENT_NUMBER,			"Amount of affinity in element" },
	{NULL,				0,							ENT_UNKNOWN,		NULL }
};


ENT_FIELD entity_exit[] = {
	{"name",		ENTITY_EXIT_NAME,		ENT_STRING,		"Name of exit" },
	{"door",		ENTITY_EXIT_DOOR,		ENT_NUMBER,		"Direction of exit" },
	{"flags",		ENTITY_EXIT_FLAGS,		ENT_BITVECTOR,	"Exit flags" },
	{"src",			ENTITY_EXIT_SOURCE,		ENT_ROOM,		"Source room" },
	{"here",		ENTITY_EXIT_SOURCE,		ENT_ROOM,		"Source room" },
	{"source",		ENTITY_EXIT_SOURCE,		ENT_ROOM,		"Source room" },
	{"dest",		ENTITY_EXIT_REMOTE,		ENT_ROOM,		"Destination room" },
	{"remote",		ENTITY_EXIT_REMOTE,		ENT_ROOM,		"Destination room" },
	{"destination",	ENTITY_EXIT_REMOTE,		ENT_ROOM,		"Destination room" },
	{"state",		ENTITY_EXIT_STATE,		ENT_STRING,		"Current state of exit" },
	{"mate",		ENTITY_EXIT_MATE,		ENT_EXIT,		"Reverse exit in destination" },
	{"north",		ENTITY_EXIT_NORTH,		ENT_EXIT,		"North exit in destination" },
	{"east",		ENTITY_EXIT_EAST,		ENT_EXIT,		"East exit in destination" },
	{"south",		ENTITY_EXIT_SOUTH,		ENT_EXIT,		"South exit in destination" },
	{"west",		ENTITY_EXIT_WEST,		ENT_EXIT,		"West exit in destination" },
	{"up",			ENTITY_EXIT_UP,			ENT_EXIT,		"Up exit in destination" },
	{"down",		ENTITY_EXIT_DOWN,		ENT_EXIT,		"Down exit in destination" },
	{"northeast",	ENTITY_EXIT_NORTHEAST,	ENT_EXIT,		"Northeast exit in destination" },
	{"northwest",	ENTITY_EXIT_NORTHWEST,	ENT_EXIT,		"Northwest exit in destination" },
	{"southeast",	ENTITY_EXIT_SOUTHEAST,	ENT_EXIT,		"Southeast exit in destination" },
	{"southwest",	ENTITY_EXIT_SOUTHWEST,	ENT_EXIT,		"Southwest exit in destination" },
	{"next",		ENTITY_EXIT_NEXT,		ENT_EXIT,		"Next exit in room" },
	{NULL,			0,						ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_token[] = {
	{"name",		ENTITY_TOKEN_NAME,			ENT_STRING,			"Name of token" },
	{"index",		ENTITY_TOKEN_INDEX,			ENT_TOKENINDEX,		"Index of token" },
	{"owner",		ENTITY_TOKEN_OWNER,			ENT_MOBILE,			"Mobile that has the token" },
	{"object",		ENTITY_TOKEN_OBJECT,		ENT_OBJECT,			"Object that has the token" },
	{"room",		ENTITY_TOKEN_ROOM,			ENT_ROOM,			"Room that has the token" },
	{"timer",		ENTITY_TOKEN_TIMER,			ENT_NUMBER,			"Current timer (resolution is based upon owner" },
	{"val0",		ENTITY_TOKEN_VAL0,			ENT_NUMBER,			"Value 0" },
	{"val1",		ENTITY_TOKEN_VAL1,			ENT_NUMBER,			"Value 1" },
	{"val2",		ENTITY_TOKEN_VAL2,			ENT_NUMBER,			"Value 2" },
	{"val3",		ENTITY_TOKEN_VAL3,			ENT_NUMBER,			"Value 3" },
	{"val4",		ENTITY_TOKEN_VAL4,			ENT_NUMBER,			"Value 4" },
	{"val5",		ENTITY_TOKEN_VAL5,			ENT_NUMBER,			"Value 5" },
	{"val6",		ENTITY_TOKEN_VAL6,			ENT_NUMBER,			"Value 6" },
	{"val7",		ENTITY_TOKEN_VAL7,			ENT_NUMBER,			"Value 7" },
	{"val8",		ENTITY_TOKEN_VAL8,			ENT_NUMBER,			"Value 8" },
	{"val9",		ENTITY_TOKEN_VAL9,			ENT_NUMBER,			"Value 9" },
	{"next",		ENTITY_TOKEN_NEXT,			ENT_TOKEN,			"Next token in list" },
	{"vars",		ENTITY_TOKEN_VARIABLES,		ENT_ILLIST_VARIABLE,"List of variables" },
	{"skill",		ENTITY_TOKEN_SKILL,			ENT_SKILLENTRY,		"Skill entry associated with token" },
	{"affects",		ENTITY_TOKEN_AFFECTS,		ENT_ILLIST_AFFECTS,	"List of affects associated with token" },
	{"reputation",	ENTITY_TOKEN_REPUTATION,	ENT_REPUTATION,		"Reputation associated with token" },
	{NULL,			0,							ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_area[] = {
	{"name",		ENTITY_AREA_NAME,			ENT_STRING,				"Name of area" },
	{"credits",		ENTITY_AREA_CREDITS,		ENT_STRING,				"Builder credits" },
	{"description",	ENTITY_AREA_DESCRIPTION,	ENT_STRING,				"Description for area" },
	{"comments",	ENTITY_AREA_COMMENTS,		ENT_STRING,				"Builder's comments" },
	{"builders",	ENTITY_AREA_BUILDERS,		ENT_STRING,				"Authorized builders" },
	{"uid",			ENTITY_AREA_UID,			ENT_NUMBER,				"UID of area" },
	{"security",	ENTITY_AREA_SECURITY,		ENT_NUMBER,				"Minimum security needed to edit in area (when not authorized)" },
	{"region",		ENTITY_AREA_REGION,			ENT_AREA_REGION,		"Default region" },
	{"regions",		ENTITY_AREA_REGIONS,		ENT_BLLIST_AREA_REGION,	"List of additional regions" },
	{"flags",		ENTITY_AREA_FLAGS,			ENT_BITVECTOR,			"Area flags" },
	{"wilds",		ENTITY_AREA_WILDS,			ENT_WILDS,				"Wilderness associate with area" },
	{"nplayer",		ENTITY_AREA_NPLAYER,		ENT_NUMBER,				"Number of players in the area" },
	{"age",			ENTITY_AREA_AGE,			ENT_NUMBER,				"Current age of area (used for repop)" },
	{"isempty",		ENTITY_AREA_ISEMPTY,		ENT_BOOLEAN,			"Indicates whether the area has no players" },
	{"isopen",		ENTITY_AREA_ISOPEN,			ENT_BOOLEAN,			"Indicates whether the area is open to players" },
	{"rooms",		ENTITY_AREA_ROOMS,			ENT_PLLIST_ROOM,		"List of rooms in area" },
	{NULL,			0,							ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_area_regions[] =
{
	{"name",		ENTITY_AREA_REGION_NAME,		ENT_STRING,			"Name of region" },
	{"description",	ENTITY_AREA_REGION_DESCRIPTION,	ENT_STRING,			"Description of region" },
	{"comments",	ENTITY_AREA_REGION_COMMENTS,	ENT_STRING,			"Builder's comments" },
	{"recall",		ENTITY_AREA_REGION_RECALL,		ENT_ROOM,			"Recall room of region" },
	{"post",		ENTITY_AREA_REGION_POSTOFFICE,	ENT_ROOM,			"Post office for region" },
	{"rooms",		ENTITY_AREA_REGION_ROOMS,		ENT_PLLIST_ROOM,	"List of rooms in region" },
	{"x",			ENTITY_AREA_REGION_X,			ENT_NUMBER,			"X coordinate of region (for wilds)" },
	{"y",			ENTITY_AREA_REGION_Y,			ENT_NUMBER,			"Y coordinate of region (for wilds)" },
	{"landx",		ENTITY_AREA_REGION_LAND_X,		ENT_NUMBER,			"Landing X coordinate for region (for wilds)" },
	{"landy",		ENTITY_AREA_REGION_LAND_Y,		ENT_NUMBER,			"Landing Y coordinate for region (for wilds)" },
	{"airship",		ENTITY_AREA_REGION_AIRSHIP,		ENT_ROOM,			"Airship Landing room for region" },
	{"flags",		ENTITY_AREA_REGION_FLAGS,		ENT_BITVECTOR,		"Region flags" },
	{"who",			ENTITY_AREA_REGION_WHO,			ENT_STAT,			"Who label" },
	{"place",		ENTITY_AREA_REGION_PLACE,		ENT_BITVECTOR,		"Place flags" },
	{"savage",		ENTITY_AREA_REGION_SAVAGE,		ENT_NUMBER,			"Savagery level" },
	{NULL,			0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_wilds[] = {
	{"name",	ENTITY_WILDS_NAME,		ENT_STRING,			"Name of wilds" },
	{"width",	ENTITY_WILDS_WIDTH,		ENT_NUMBER,			"Width of the wilds" },
	{"height",	ENTITY_WILDS_HEIGHT,	ENT_NUMBER,			"Height of the wilds" },
	{"vrooms",	ENTITY_WILDS_VROOMS,	ENT_PLLIST_ROOM,	"List of loaded virtual rooms" },
	{NULL,		0,						ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_conn[] = {
	{"player",		ENTITY_CONN_PLAYER,			ENT_MOBILE,		"Current mobile associated with connection" },
	{"original",	ENTITY_CONN_ORIGINAL,		ENT_MOBILE,		"Original mobile associated with connection" },
	{"host",		ENTITY_CONN_HOST,			ENT_STRING,		"Host string" },
	{"connection",	ENTITY_CONN_CONNECTION,		ENT_NUMBER,		"Connection number" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_church[] = {
	{"name",		ENTITY_CHURCH_NAME,			ENT_STRING,			"Name of church" },
	{"size",		ENTITY_CHURCH_SIZE,			ENT_STAT,			"Church size" },
	{"flag",		ENTITY_CHURCH_FLAG,			ENT_STRING,			"Church flag string for {Wwho{x" },
	{"founder",		ENTITY_CHURCH_FOUNDER,		ENT_MOBILE,			"Founder player (if online)" },
	{"founder_name",ENTITY_CHURCH_FOUNDER_NAME,	ENT_STRING,			"Founder player name" },
	{"motd",		ENTITY_CHURCH_MOTD,			ENT_STRING,			"Message of the Day" },
	{"rules",		ENTITY_CHURCH_RULES,		ENT_STRING,			"Church rules" },
	{"info",		ENTITY_CHURCH_INFO,			ENT_STRING,			"Church info" },
	{"recall",		ENTITY_CHURCH_RECALL,		ENT_ROOM,			"Church hall recall room" },
	{"treasure",	ENTITY_CHURCH_TREASURE,		ENT_PLLIST_ROOM,	"List of church treasure rooms" },
	{"key",			ENTITY_CHURCH_KEY,			ENT_OBJINDEX,		"Index of church hall key" },
	{"online",		ENTITY_CHURCH_ONLINE,		ENT_PLLIST_CONN,	"List of online members" },
	{"roster",		ENTITY_CHURCH_ROSTER,		ENT_PLLIST_STR,		"List of member names" },
	{NULL,			0,							ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_list[] = {
	{"count",	ENTITY_LIST_SIZE,	ENT_NUMBER,		"Size of list" },
	{"size",	ENTITY_LIST_SIZE,	ENT_NUMBER,		"Size of list" },
	{"len",		ENTITY_LIST_SIZE,	ENT_NUMBER,		"Size of list" },
	{"length",	ENTITY_LIST_SIZE,	ENT_NUMBER,		"Size of list" },
	{"random",	ENTITY_LIST_RANDOM,	ENT_UNKNOWN,	"Random element in the list" },
	{"first",	ENTITY_LIST_FIRST,	ENT_UNKNOWN,	"First element of the list" },
	{"last",	ENTITY_LIST_LAST,	ENT_UNKNOWN,	"Last element of the list" },
	{NULL,		0,					ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_skill[] = {
	// TODO: Add *all* the deflection messages
	{"gsn",				ENTITY_SKILL_GSN,			ENT_NUMBER,				"Global skill number" },
	{"token",			ENTITY_SKILL_TOKEN,			ENT_TOKENINDEX,			"Index of token for scripted skills" },
	{"spell",			ENTITY_SKILL_SPELL,			ENT_BOOLEAN,			"Indicates whether the skill is a spell" },
	{"name",			ENTITY_SKILL_NAME,			ENT_STRING,				"Name of skill" },
	{"display",			ENTITY_SKILL_DISPLAY,		ENT_STRING,				"Display string of skill" },
	{"beats",			ENTITY_SKILL_BEATS,			ENT_NUMBER,				"How long the skill takes to activate/cast" },
	{"timer",			ENTITY_SKILL_BEATS,			ENT_NUMBER,				"How long the skill takes to activate/cast" },
	// TODO: Update for class changes
	{"target",			ENTITY_SKILL_TARGET,		ENT_STAT,				"Target type of skill" },
	{"position",		ENTITY_SKILL_POSITION,		ENT_STAT,				"Minimum position needed to do the skill" },
	{"wearoff",			ENTITY_SKILL_WEAROFF,		ENT_STRING,				"Wear off message (for affects)" },
	{"object",			ENTITY_SKILL_OBJECT,		ENT_STRING,				"Wear off message from objects (for affects)" },
	{"dispel",			ENTITY_SKILL_DISPEL,		ENT_STRING,				"Dispel message (for affects)" },
	{"noun",			ENTITY_SKILL_NOUN,			ENT_STRING,				"Damage noun" },
	{"mana",			ENTITY_SKILL_MANA_CAST,		ENT_NUMBER,				"Mana cost to cast spell" },
	{"brew_mana",		ENTITY_SKILL_MANA_BREW,		ENT_NUMBER,				"Mana cost to brew spell" },
	{"scribe_mana",		ENTITY_SKILL_MANA_SCRIBE,	ENT_NUMBER,				"Mana cost to scribe spell" },
	{"imbue_mana",		ENTITY_SKILL_MANA_IMBUE,	ENT_NUMBER,				"Mana cost to imbue spell" },
	{"inks",			ENTITY_SKILL_INKS,			ENT_INK_TYPES,			"Required ink types" },
	{"values",			ENTITY_SKILL_VALUES,		ENT_SKILL_VALUES,		"Custom skill values table" },
	{"valuenames",		ENTITY_SKILL_VALUENAMES,	ENT_SKILL_VALUENAMES,	"Custom skill value name table" },
	{NULL,				0,							ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_skillgroups[] = {
	{"name",	ENTITY_SKILLGROUP_NAME,		ENT_STRING,					"Name of skill group" },
	{"contents",ENTITY_SKILLGROUP_CONTENTS,	ENT_ILLIST_SKILLGROUPS,		"List of skills/groups" },
	{NULL,		0,							ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_skillentry[] = {
	{"flags",		ENTITY_SKILLENTRY_FLAGS,		ENT_BITVECTOR,	"Skill entry flags" },
	{"isspell",		ENTITY_SKILLENTRY_ISSPELL,		ENT_BOOLEAN,	"Indicates whether the skill entry is a spell" },
	{"mod",			ENTITY_SKILLENTRY_MOD_RATING,	ENT_NUMBER,		"How much has the rating been modified" },
	{"rating",		ENTITY_SKILLENTRY_RATING,		ENT_NUMBER,		"Effective rating" },
	{"skill",		ENTITY_SKILLENTRY_SKILL,		ENT_SKILL,		"Skill reference" },
	{"song",		ENTITY_SKILLENTRY_SONG,			ENT_SONG,		"Song reference" },
	{"source",		ENTITY_SKILLENTRY_SOURCE,		ENT_STAT,		"How the entry was provided" },
	{"token",		ENTITY_SKILLENTRY_TOKEN,		ENT_TOKEN,		"Token associated with skill entry" },
	{NULL,			0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_skill_info[] = {
	{"skill",	ENTITY_SKILLINFO_SKILL,		ENT_SKILL,		"Skill reference" },
	{"owner",	ENTITY_SKILLINFO_OWNER,		ENT_MOBILE,		"MObile owner of skill" },
	{"token",	ENTITY_SKILLINFO_TOKEN,		ENT_TOKEN,		"Token associated with skill" },
	{"rating",	ENTITY_SKILLINFO_RATING,	ENT_NUMBER,		"Skill rating" },
	{NULL,		0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_lock_state[] = {
	{"key",			ENTITY_LOCKSTATE_KEY,			ENT_OBJINDEX,			"Index of the key" },
	{"pick",		ENTITY_LOCKSTATE_PICK,			ENT_NUMBER,				"Pick chance" },
	{"flags",		ENTITY_LOCKSTATE_FLAGS,			ENT_BITVECTOR,			"Lock flags" },
	{"specialkeys", ENTITY_LOCKSTATE_SPECIALKEYS,	ENT_ILLIST_SPECIALKEYS,	"List of special key objects (iterator only)" },
	{NULL,			0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_affect[] = {
	{"name",		ENTITY_AFFECT_NAME,		ENT_STRING,		"Display name of affect" },
	{"group",		ENTITY_AFFECT_GROUP,	ENT_NUMBER,		"Affect group" },
	{"skill",		ENTITY_AFFECT_SKILL,	ENT_SKILL,		"Skill associated with the affect" },
	{"catalyst",	ENTITY_AFFECT_CATALYST,	ENT_STAT,		"Catalyst type (for catalyst entries)" },
	{"where",		ENTITY_AFFECT_WHERE,	ENT_NUMBER,		"Where the affect will be applied" },
	{"location",	ENTITY_AFFECT_LOCATION,	ENT_NUMBER,		"What will the affect modify" },
	{"mod",			ENTITY_AFFECT_MOD,		ENT_NUMBER,		"How much the affect modifies" },
	// TODO: Turn into a bitmatrix
	{"bits",		ENTITY_AFFECT_BITS,		ENT_BITVECTOR,	"Affected_by flags" },
	{"bits2",		ENTITY_AFFECT_BITS2,	ENT_BITVECTOR,	"Affected_by2 flags" },
	{"duration",	ENTITY_AFFECT_TIMER,	ENT_NUMBER,		"Duration of affect" },
	{"timer",		ENTITY_AFFECT_TIMER,	ENT_NUMBER,		"Duration of affect" },
	{"level",		ENTITY_AFFECT_LEVEL,	ENT_NUMBER,		"Level of the affect" },
	// TODO: Add token
	{NULL,			0,						ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_book_page[] = {
	{"number",		ENTITY_BOOK_PAGE_NUMBER,	ENT_NUMBER,		"Page number" },
	{"title",		ENTITY_BOOK_PAGE_TITLE,		ENT_STRING,		"Page title" },
	{"text",		ENTITY_BOOK_PAGE_TEXT,		ENT_STRING,		"Text of page" },
	{"book",		ENTITY_BOOK_PAGE_BOOK,		ENT_OBJINDEX,	"Index of book" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_food_buff[] = {
	{"where",		ENTITY_FOOD_BUFF_WHERE,		ENT_NUMBER,		"Where the food buff will be applied" },
	{"location",	ENTITY_FOOD_BUFF_LOCATION,	ENT_NUMBER,		"What the food buff will modify" },
	{"mod",			ENTITY_FOOD_BUFF_MOD,		ENT_NUMBER,		"How much will the food buff modify" },
	// TODO: Convert this into BITMATRIX
	{"bits",		ENTITY_FOOD_BUFF_BITS,		ENT_BITVECTOR,	"Affected_by flags" },
	{"bits2",		ENTITY_FOOD_BUFF_BITS2,		ENT_BITVECTOR,	"Affected_by2 flags" },
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};


ENT_FIELD entity_compartment[] = {
	{"name",			ENTITY_COMPARTMENT_NAME,			ENT_STRING,		"Name of compartment" },
	{"short",			ENTITY_COMPARTMENT_SHORT,			ENT_STRING,		"Short of compartment" },
	{"description",		ENTITY_COMPARTMENT_DESC,			ENT_STRING,		"Description of the compartment" },
	{"flags",			ENTITY_COMPARTMENT_FLAGS,			ENT_BITVECTOR,	"Compartment flags" },
	{"max_occupants",	ENTITY_COMPARTMENT_MAX_OCCUPANTS,	ENT_NUMBER,		"Maximum number of occupants" },
	{"max_weight",		ENTITY_COMPARTMENT_MAX_WEIGHT,		ENT_NUMBER,		"Maximum weight allowed" },
	{"standing",		ENTITY_COMPARTMENT_STANDING,		ENT_BITVECTOR,	"Action flags for standing" },
	{"hanging",			ENTITY_COMPARTMENT_HANGING,			ENT_BITVECTOR,	"Action flags for hanging (not implemented yet)" },
	{"sitting",			ENTITY_COMPARTMENT_SITTING,			ENT_BITVECTOR,	"Action flags for sitting" },
	{"resting",			ENTITY_COMPARTMENT_RESTING,			ENT_BITVECTOR,	"Action flags for resting" },
	{"sleeping",		ENTITY_COMPARTMENT_SLEEPING,		ENT_BITVECTOR,	"Action flags for sleeping" },
	{"health",			ENTITY_COMPARTMENT_HEALTH,			ENT_NUMBER,		"Health regeneration rate" },
	{"mana",			ENTITY_COMPARTMENT_MANA,			ENT_NUMBER,		"Mana regeneration rate" },
	{"move",			ENTITY_COMPARTMENT_MOVE,			ENT_NUMBER,		"Move regeneration rate" },
	{"lock",			ENTITY_COMPARTMENT_LOCK,			ENT_LOCK_STATE,	"Lock for compartment" },
	{NULL,				0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_ink_type[] = {
	{"catalyst",	ENTITY_INK_TYPE_CATALYST,		ENT_STAT,		"Elemental type in ink" },
	{"type",		ENTITY_INK_TYPE_CATALYST,		ENT_STAT,		"Elemental type in ink" },
	{"amount",		ENTITY_INK_TYPE_AMOUNT,			ENT_NUMBER,		"Strength of element in ink" },
	{NULL,			0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_instrument_reservoir[] = {
	{"catalyst",	ENTITY_INSTRUMENT_RESERVOIR_CATALYST,		ENT_STAT,		"Elemental type of reservoir" },
	{"type",		ENTITY_INSTRUMENT_RESERVOIR_CATALYST,		ENT_STAT,		"Elemental type of reservoir" },
	{"amount",		ENTITY_INSTRUMENT_RESERVOIR_AMOUNT,			ENT_NUMBER,		"Current amount in the reservoir" },
	{"capacity",	ENTITY_INSTRUMENT_RESERVOIR_CAPACITY,		ENT_NUMBER,		"Total amount in the reservoir" },
	{NULL,			0,											ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_weapon_attack[] = {
	{"name",		ENTITY_WEAPON_ATTACK_NAME,			ENT_STRING,		"Name of attack point" },
	{"short",		ENTITY_WEAPON_ATTACK_SHORT,			ENT_STRING,		"Short of attack point" },
	{"type",		ENTITY_WEAPON_ATTACK_TYPE,			ENT_ATTACK,		"Damage type for attack point" },		// It's going to be similar to ENT_STAT but has to use the attack_lookup
	{"flags",		ENTITY_WEAPON_ATTACK_FLAGS,			ENT_BITVECTOR,	"Weapon flags for attack point" },
	{"damage",		ENTITY_WEAPON_ATTACK_DAMAGE,		ENT_DICE,		"Damage dice for attack point" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_attack_type[] = {
	{"name",		ENTITY_ATTACK_NAME,					ENT_STRING,		"Name of attack" },
	{"noun",		ENTITY_ATTACK_NOUN,					ENT_STRING,		"Display noun of the attack" },
	{"type",		ENTITY_ATTACK_TYPE,					ENT_STAT,		"Damage type of the attack" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_liquid[] = {
	{"name",			ENTITY_LIQUID_NAME,				ENT_STRING,		"Name of liquid" },
	{"color",			ENTITY_LIQUID_COLOR,			ENT_STRING,		"Color string for liquid" },
	{"uid",				ENTITY_LIQUID_UID,				ENT_NUMBER,		"UID of liquid" },
	{"used",			ENTITY_LIQUID_USED,				ENT_NUMBER,		"Usage counter (not implemented yet)" },
	{"flammable",		ENTITY_LIQUID_FLAMMABLE,		ENT_BOOLEAN,	"Indicates whether the liquid is flammable" },
	{"proof",			ENTITY_LIQUID_PROOF,			ENT_NUMBER,		"Alcohol proof (0-200)" },
	{"full",			ENTITY_LIQUID_FULL,				ENT_NUMBER,		"Replenishment of {Wfullness{x" },
	{"thirst",			ENTITY_LIQUID_THIRST,			ENT_NUMBER,		"Replenishment of {Wthirst{x" },
	{"hunger",			ENTITY_LIQUID_HUNGER,			ENT_NUMBER,		"Replenishment of {Whunger{x" },
	{"fuelunit",		ENTITY_LIQUID_FUEL_UNIT,		ENT_NUMBER,		"How many units of fuel is consumed per usage" },
	{"fuelduration",	ENTITY_LIQUID_FUEL_DURATION,	ENT_NUMBER,		"How long fuel lasts per usage" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_material[] = {
	{"name",			ENTITY_MATERIAL_NAME,			ENT_STRING,		"Name of material" },
	{"class",			ENTITY_MATERIAL_CLASS,			ENT_STAT,		"Material class" },
	{"flags",			ENTITY_MATERIAL_FLAGS,			ENT_BITVECTOR,	"Material flags" },
	{"flammable",		ENTITY_MATERIAL_FLAMMABLE,		ENT_NUMBER,		"How likely the material burns" },
	{"corrodibility",	ENTITY_MATERIAL_CORRODIBILITY,	ENT_NUMBER,		"How likely the material corrodes" },
	{"strength",		ENTITY_MATERIAL_STRENGTH,		ENT_NUMBER,		"Default material strength" },
	{"value",			ENTITY_MATERIAL_VALUE,			ENT_NUMBER,		"Default material value" },
	{"burned",			ENTITY_MATERIAL_BURNED,			ENT_MATERIAL,	"New material, if burned" },
	{"corroded",		ENTITY_MATERIAL_CORRODED,		ENT_MATERIAL,	"New material, if corroded" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_song[] = {
	{"name",	ENTITY_SONG_NAME,	ENT_STRING,		"Name of song" },
	{"uid",		ENTITY_SONG_UID,	ENT_NUMBER,		"UID of song" },
	{"flags",	ENTITY_SONG_FLAGS,	ENT_BITVECTOR,	"Song flags" },
	{"token",	ENTITY_SONG_TOKEN,	ENT_TOKENINDEX,	"Token associated with song" },
	{"target",	ENTITY_SONG_TARGET,	ENT_STAT,		"Target type of song" },
	{"beats",	ENTITY_SONG_BEATS,	ENT_NUMBER,		"Time to play the song" },
	{"mana",	ENTITY_SONG_MANA,	ENT_NUMBER,		"Mana cost of song" },
	{"level",	ENTITY_SONG_LEVEL,	ENT_NUMBER,		"Level of song" },
	{NULL,		0,					ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_spelldata[] = {
	{"name",	ENTITY_SPELLDATA_NAME,		ENT_STRING,		"Name of spell" },
	{"skill",	ENTITY_SPELLDATA_SKILL,		ENT_SKILL,		"Skill reference" },
	{"level",	ENTITY_SPELLDATA_LEVEL,		ENT_NUMBER,		"Level of spell" },
	{"chance",	ENTITY_SPELLDATA_CHANCE,	ENT_NUMBER,		"Chance the spell is applied" },
	{NULL,		0,							ENT_UNKNOWN,	NULL }
};

// TODO: Update this
ENT_FIELD entity_variable[] = {
	{"name",	ENTITY_VARIABLE_NAME,	ENT_STRING,		"Name of variable" },
	{"type",	ENTITY_VARIABLE_TYPE,	ENT_STRING,		"Type of variable" },
	{"save",	ENTITY_VARIABLE_SAVE,	ENT_NUMBER,		"Indicates whether the variable saves" },
	{NULL,		0,						ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_race[] = {
	{"name",			ENTITY_RACE_NAME,				ENT_STRING,			"Name of race" },
	{"description",		ENTITY_RACE_DESCRIPTION,		ENT_STRING,			"Description" },
	{"comments",		ENTITY_RACE_COMMENTS,			ENT_STRING,			"Builder's comments" },
	{"uid",				ENTITY_RACE_UID,				ENT_NUMBER,			"UID of race" },
	{"playable",		ENTITY_RACE_PLAYABLE,			ENT_BOOLEAN,		"Indicates the race is available for players" },
	{"starting",		ENTITY_RACE_STARTING,			ENT_BOOLEAN,		"Indicates the race is one of the starting races without unlock" },
	{"act",				ENTITY_RACE_ACT,				ENT_BITMATRIX,		"Default {WACT{x flags" },
	{"affects",			ENTITY_RACE_AFFECTS,			ENT_BITMATRIX,		"Default {WAFFECT{x flags" },
	{"offense",			ENTITY_RACE_OFFENSE,			ENT_BITVECTOR,		"Default {WOFFENSE{x flags" },
	{"immune",			ENTITY_RACE_IMMUNE,				ENT_BITVECTOR,		"Default {WIMMUNITY{x flags" },
	{"resist",			ENTITY_RACE_RESIST,				ENT_BITVECTOR,		"Default {WRESISTANCE{x flags" },
	{"vuln",			ENTITY_RACE_VULN,				ENT_BITVECTOR,		"Default {WVULNERABILITY{x flags" },
	{"form",			ENTITY_RACE_FORM,				ENT_BITVECTOR,		"Default {WFORM{x flags" },
	{"parts",			ENTITY_RACE_PARTS,				ENT_BITVECTOR,		"Default {WPART{x flags" },
	{"isremort",		ENTITY_RACE_ISREMORT,			ENT_BOOLEAN,		"Indicates the race is a remort race" },
	{"remort",			ENTITY_RACE_REMORT,				ENT_RACE,			"Specifies what this race remorts into" },
	{"who",				ENTITY_RACE_WHO,				ENT_STRING,			"How the race displays on {Wwho{x" },
	{"skills",			ENTITY_RACE_SKILLS,				ENT_ILLIST_SKILLS,	"List of racial skills (iterator only)" },
	{"startingstats",	ENTITY_RACE_STARTING_STATS,		ENT_STATS_TABLE,	"Starting stat table" },
	{"maxstats",		ENTITY_RACE_MAX_STATS,			ENT_STATS_TABLE,	"Maximum stat table" },
	{"maxvitals",		ENTITY_RACE_MAX_VITALS,			ENT_VITALS_TABLE,	"Maximum vitals table" },
	{"size",			ENTITY_RACE_SIZE,				ENT_RANGE,			"Size range" },
	{"alignment",		ENTITY_RACE_DEFAULT_ALIGNMENT,	ENT_NUMBER,			"Default alignment" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_class[] = {
	{"name",		ENTITY_CLASS_NAME,			ENT_STRING,				"Name of class" },
	{"description",	ENTITY_CLASS_DESCRIPTION,	ENT_STRING,				"Description" },
	{"comments",	ENTITY_CLASS_COMMENTS,		ENT_STRING,				"Builder's comments" },
	{"uid",			ENTITY_CLASS_UID,			ENT_NUMBER,				"UID of class" },
	{"display",		ENTITY_CLASS_DISPLAY,		ENT_SEX_STRING_TABLE,	"Sex string table for display purposes to the player" },
	{"who",			ENTITY_CLASS_WHO,			ENT_SEX_STRING_TABLE,	"Sex string table for shown in {Wwho{x" },
	{"type",		ENTITY_CLASS_TYPE,			ENT_STAT,				"Type of class" },
	{"flags",		ENTITY_CLASS_FLAGS,			ENT_BITVECTOR,			"Class flags" },
	{"groups",		ENTITY_CLASS_GROUPS,		ENT_ILLIST_SKILLGROUPS,	"List of skill groups (iterator only)" },
	{"stat",		ENTITY_CLASS_PRIMARY_STAT,	ENT_STAT,				"Primary stat" },
	{"maxlevel",	ENTITY_CLASS_MAX_LEVEL,		ENT_NUMBER,				"Maximum level that can be attained in this class" },
	{NULL,			0,							ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_classlevel[] = {
	{"class",		ENTITY_CLASSLEVEL_CLASS,	ENT_CLASS,		"Class reference" },
	{"level",		ENTITY_CLASSLEVEL_LEVEL,	ENT_NUMBER,		"Current level" },
	// TODO: Add XP?
	{NULL,			0,							ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_mission[] = {
	{"giver",			ENTITY_MISSION_GIVER,					ENT_WIDEVNUM,				"Widevnum of mission giver" },
	{"givertype",		ENTITY_MISSION_GIVER_TYPE,				ENT_STAT,					"Type of mission giver" },
	{"receiver",		ENTITY_MISSION_RECEIVER,				ENT_WIDEVNUM,				"Widevnum of mission receiver" },
	{"receivertype",	ENTITY_MISSION_RECEIVER_TYPE,			ENT_STAT,					"Type of mission receiver" },
	{"generating",		ENTITY_MISSION_GENERATING,				ENT_BOOLEAN,				"Indicates the mission is still generating" },
	{"scripted",		ENTITY_MISSION_SCRIPTED,				ENT_BOOLEAN,				"Indicates the mission is scripted" },
	{"timer",			ENTITY_MISSION_TIMER,					ENT_NUMBER,					"Time remaining for mission" },
	{"class",			ENTITY_MISSION_CLASS,					ENT_CLASS,					"Class associated with mission" },
	{"classtype",		ENTITY_MISSION_CLASS_TYPE,				ENT_STAT,					"Class type associated with mission" },
	{"restricted",		ENTITY_MISSION_CLASS_RESTRICTED,		ENT_BOOLEAN,				"Indicates the mission is class restricted" },
	{"type_restricted",	ENTITY_MISSION_CLASS_TYPE_RESTRICTED,	ENT_BOOLEAN,				"Indicates the mission is class type restricted" },
	{"parts",			ENTITY_MISSION_PARTS,					ENT_OLLIST_MISSION_PARTS,	"List of mission parts" },
	{NULL,				0,										ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_mission_part[] = {
	{"index",		ENTITY_MISSION_PART_INDEX,			ENT_NUMBER,		"Part number" },
	{"object",		ENTITY_MISSION_PART_OBJECT,			ENT_OBJECT,		"Actual object for object tasks" },
	{"description",	ENTITY_MISSION_PART_DESCRIPTION,	ENT_STRING,		"Description of part task" },
	{"minutes",		ENTITY_MISSION_PART_MINUTES,		ENT_NUMBER,		"Alloted time" },
	{"area",		ENTITY_MISSION_PART_AREA,			ENT_AREA,		"Area of target" },
	{"slay",		ENTITY_MISSION_PART_SLAY,			ENT_WIDEVNUM,	"Slay mobile target" },
	{"retrieve",	ENTITY_MISSION_PART_RETRIEVE,		ENT_WIDEVNUM,	"Retrieve object target" },
	{"travel",		ENTITY_MISSION_PART_TRAVEL,			ENT_WIDEVNUM,	"Travel to location target" },
	{"sacrifice",	ENTITY_MISSION_PART_SACRIFICE,		ENT_WIDEVNUM,	"Sacrifice object target" },
	{"rescue",		ENTITY_MISSION_PART_RESCUE,			ENT_WIDEVNUM,	"Rescue mobile target" },
	{"custom",		ENTITY_MISSION_PART_CUSTOM,			ENT_BOOLEAN,	"Is a custom task" },
	{"complete",	ENTITY_MISSION_PART_COMPLETE,		ENT_BOOLEAN,	"Is the task complete" },
	{"mission",		ENTITY_MISSION_PART_MISSION,		ENT_MISSION,	"Mission reference" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_waypoint[] = {
	{"name",		ENTITY_WAYPOINT_NAME,				ENT_STRING,		"Name of waypoint" },
	{"wilds",		ENTITY_WAYPOINT_WILDS,				ENT_WILDS,		"Wilds of target room" },
	{"x",			ENTITY_WAYPOINT_X,					ENT_NUMBER,		"X coordinate of target room" },
	{"y",			ENTITY_WAYPOINT_Y,					ENT_NUMBER,		"Y coordinate of target room" },
	{"target",		ENTITY_WAYPOINT_TARGET,				ENT_ROOM,		"Target room" },
	{NULL,			0,									ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_group[] = {
	{"owner",		ENTITY_GROUP_OWNER,		ENT_MOBILE,				"Owner of group" },
	{"leader",		ENTITY_GROUP_LEADER,	ENT_MOBILE,				"Leader of group" },
	{"ally",		ENTITY_GROUP_ALLY,		ENT_MOBILE,				"Random group member in the room, not the owner" },
	{"member",		ENTITY_GROUP_MEMBER,	ENT_MOBILE,				"Random group member in the room, including owner" },
	{"members",		ENTITY_GROUP_MEMBERS,	ENT_ILLIST_MOB_GROUP,	"Memders in the current room (iterator only)" },
	{"size",		ENTITY_GROUP_SIZE,		ENT_NUMBER,				"Size of group" },
	{NULL,			0,						ENT_UNKNOWN,			NULL }

};

ENT_FIELD entity_dice[] = {
	{"number",		ENTITY_DICE_NUMBER,	ENT_NUMBER,		"Number of dice" },
	{"size",		ENTITY_DICE_SIZE,	ENT_NUMBER,		"Size of each die" },
	{"bonus",		ENTITY_DICE_BONUS,	ENT_NUMBER,		"Bonus added" },
	{"roll",		ENTITY_DICE_ROLL,	ENT_NUMBER,		"Roll a random value" },
	{"last",		ENTITY_DICE_LAST,	ENT_NUMBER,		"Last rolled value" },
	{NULL,			0,					ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_range[] = {
	{"min",		ENTITY_RANGE_MIN,		ENT_NUMBER,		"Minimum value of range" },
	{"max",		ENTITY_RANGE_MAX,		ENT_NUMBER,		"Maximum value of range" },
	{"average",	ENTITY_RANGE_AVERAGE,	ENT_NUMBER,		"Average value of range" },
	{"span",	ENTITY_RANGE_SPAN,		ENT_NUMBER,		"Span of values in range" },
	{"value",	ENTITY_RANGE_VALUE,		ENT_NUMBER,		"Random value in range" },
	{NULL,		0,						ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_mobindex[] = {
	{"wnum",			ENTITY_MOBINDEX_WNUM,			ENT_WIDEVNUM,	"Widevnum of mobile" },
	{"loaded",			ENTITY_MOBINDEX_LOADED,			ENT_NUMBER,		"Number of mobiles loaded for this index" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_objindex[] = {
	{"wnum",			ENTITY_OBJINDEX_WNUM,			ENT_WIDEVNUM,		"Widevnum of object" },
	{"loaded",			ENTITY_OBJINDEX_LOADED,			ENT_NUMBER,			"Number of objects loaded for this index" },
	{"inrooms",			ENTITY_OBJINDEX_INROOMS,		ENT_NUMBER,			"Number of objects in rooms" },
	{"inmail",			ENTITY_OBJINDEX_INMAIL,			ENT_NUMBER,			"Number of objects in the mail system" },
	{"carried",			ENTITY_OBJINDEX_CARRIED,		ENT_NUMBER,			"Number of objects carried by mobiles" },
	{"lockered",		ENTITY_OBJINDEX_LOCKERED,		ENT_NUMBER,			"Number of objects in lockers" },
	{"incontainer",		ENTITY_OBJINDEX_INCONTAINER,	ENT_NUMBER,			"Number of objects in containers" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_tokenindex[] = {
	{"wnum",			ENTITY_TOKENINDEX_WNUM,			ENT_WIDEVNUM,		"Widevnum of token" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_instance_section[] = {
	{"rooms",			ENTITY_SECTION_ROOMS,			ENT_PLLIST_ROOM,		"List of rooms in section" },
	{"blueprint",		ENTITY_SECTION_INDEX,			ENT_BLUEPRINT_SECTION,	"Blueprint of sections" },
	{"instance",		ENTITY_SECTION_INSTANCE,		ENT_INSTANCE,			"Instance containing section" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_instance[] = {
	{"name",			ENTITY_INSTANCE_NAME,			ENT_STRING,				"Name of instance" },
	{"index",			ENTITY_INSTANCE_BLUEPRINT,		ENT_BLUEPRINT,			"Blueprint of instance" },
	{"sections",		ENTITY_INSTANCE_SECTIONS,		ENT_ILLIST_SECTIONS,	"List of instance sections (iterator only)" },
	{"owners",			ENTITY_INSTANCE_OWNERS,			ENT_BLLIST_MOB,			"List of instance owners" },
	{"object",			ENTITY_INSTANCE_OBJECT,			ENT_OBJECT,				"Object associated with instance" },
	{"dungeon",			ENTITY_INSTANCE_DUNGEON,		ENT_DUNGEON,			"Dungeon associated with instance" },
	{"ship",			ENTITY_INSTANCE_SHIP,			ENT_SHIP,				"Ship associated with instance" },
	{"floor",			ENTITY_INSTANCE_FLOOR,			ENT_NUMBER,				"Floor number of instance" },
	{"entry",			ENTITY_INSTANCE_ENTRY,			ENT_ROOM,				"Entry room of instance" },
	{"exit",			ENTITY_INSTANCE_EXIT,			ENT_ROOM,				"Exit room of instance" },
	{"recall",			ENTITY_INSTANCE_RECALL,			ENT_ROOM,				"Recall room of instance" },
	{"environ",			ENTITY_INSTANCE_ENVIRON,		ENT_ROOM,				"Environment outside instance" },
	{"rooms",			ENTITY_INSTANCE_ROOMS,			ENT_PLLIST_ROOM,		"List of rooms in instance" },
	{"players",			ENTITY_INSTANCE_PLAYERS,		ENT_PLLIST_MOB,			"List of players in instance" },
	{"mobiles",			ENTITY_INSTANCE_MOBILES,		ENT_PLLIST_MOB,			"List of instance mobiles" },
	{"objects",			ENTITY_INSTANCE_OBJECTS,		ENT_PLLIST_OBJ,			"List of instance objects" },
	{"bosses",			ENTITY_INSTANCE_BOSSES,			ENT_PLLIST_MOB,			"List of instance bosses" },
	{"specialrooms",	ENTITY_INSTANCE_SPECIAL_ROOMS,	ENT_ILLIST_SPECIALROOMS,"List of special rooms (iterator only)" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_blueprint[] = {
	{"wnum",			ENTITY_BLUEPRINT_WNUM,			ENT_WIDEVNUM,		"Widevnum of blueprint" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};

ENT_FIELD entity_blueprint_section[] = {
	{"wnum",			ENTITY_BLUEPRINT_SECTION_WNUM,	ENT_WIDEVNUM,		"Widevnum of blueprint section" },
	{NULL,				0,								ENT_UNKNOWN,		NULL }
};


ENT_FIELD entity_dungeon[] = {
	{"name",			ENTITY_DUNGEON_NAME,			ENT_STRING,					"Name of dungeon" },
	{"index",			ENTITY_DUNGEON_INDEX,			ENT_DUNGEONINDEX,			"Index of dungeon" },
	{"floors",			ENTITY_DUNGEON_FLOORS,			ENT_ILLIST_INSTANCES,		"List of floors (iterator only)" },
	{"desc",			ENTITY_DUNGEON_DESC,			ENT_STRING,					"Description" },
	{"owners",			ENTITY_DUNGEON_OWNERS,			ENT_BLLIST_MOB,				"List of dungeon owners" },
	{"entry",			ENTITY_DUNGEON_ENTRY,			ENT_ROOM,					"Entry room (outside)" },
	{"exit",			ENTITY_DUNGEON_EXIT,			ENT_ROOM,					"Exit room (outside)" },
	{"rooms",			ENTITY_DUNGEON_ROOMS,			ENT_PLLIST_ROOM,			"List of rooms in the dungeon" },
	{"players",			ENTITY_DUNGEON_PLAYERS,			ENT_PLLIST_MOB,				"List of players in the dungeon" },
	{"mobiles",			ENTITY_DUNGEON_MOBILES,			ENT_PLLIST_MOB,				"List of dungeon mobiles" },
	{"objects",			ENTITY_DUNGEON_OBJECTS,			ENT_PLLIST_OBJ,				"List of dungeon objects" },
	{"bosses",			ENTITY_DUNGEON_BOSSES,			ENT_PLLIST_MOB,				"List of dungeon bosses" },
	{"specialrooms",	ENTITY_DUNGEON_SPECIAL_ROOMS,	ENT_ILLIST_SPECIALROOMS,	"List of special rooms (iterator only)" },
	{NULL,				0,								ENT_UNKNOWN,				NULL }
};

ENT_FIELD entity_dungeon_index[] = {
	{"wnum",			ENTITY_DUNGEONINDEX_WNUM,		ENT_WIDEVNUM,	"Widevnum of dungeon" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_ship[] = {
	{"name",			ENTITY_SHIP_NAME,				ENT_STRING,		"Name of ship" },
	{"index",			ENTITY_SHIP_INDEX,				ENT_SHIPINDEX,	"Index of ship" },
	{"object",			ENTITY_SHIP_OBJECT,				ENT_OBJECT,		"Ship object" },
	// TODO: Add other stuff
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_ship_index[] = {
	{"wnum",			ENTITY_SHIPINDEX_WNUM,			ENT_WIDEVNUM,	"Widevnum of ship" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_quest_part[] = {
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_quest[] = {
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_shop[] = {
	{"buy",				ENTITY_SHOP_PROFIT_BUY,			ENT_NUMBER,				"Buying markup" },
	{"sell",			ENTITY_SHOP_PROFIT_SELL,		ENT_NUMBER,				"Selling markdown" },
	{"open",			ENTITY_SHOP_HOUR_OPEN,			ENT_NUMBER,				"Hour when shop opens" },
	{"closed",			ENTITY_SHOP_HOUR_CLOSED,		ENT_NUMBER,				"Hour when shop closes" },
	{"buytype",			ENTITY_SHOP_BUYTYPE,			ENT_SHOP_BUYTYPES,		"Table of buy back types" },
	{"restock",			ENTITY_SHOP_RESTOCK,			ENT_NUMBER,				"Restocking interval" },
	{"flags",			ENTITY_SHOP_FLAGS,				ENT_BITVECTOR,			"Shop flags" },
	{"discount",		ENTITY_SHOP_DISCOUNT,			ENT_NUMBER,				"Overall discount rate" },
	{"shipyard",		ENTITY_SHOP_SHIPYARD,			ENT_SHOP_SHIPYARD,		"Shipyard location for ship stock" },
	{"reputation",		ENTITY_SHOP_REPUTATION,			ENT_REPUTATION_INDEX,	"Reputation needed to interact with the shop" },
	{"rank",			ENTITY_SHOP_REPUTATION_RANK,	ENT_REPUTATION_RANK,	"Minimum reputation rank needed to interact with the shop" },
	{"stock",			ENTITY_SHOP_STOCK,				ENT_OLLIST_SHOP_STOCK,	"List of stock items" },
	{"keeper",			ENTITY_SHOP_KEEPER,				ENT_MOBILE,				"Shopkeeper mobile" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_shop_shipyard[] = {
	{"wilds",			ENTITY_SHIPYARD_WILDS,			ENT_WILDS,		"Wilderness of shipyard" },
	{"x1",				ENTITY_SHIPYARD_X1,				ENT_NUMBER,		"Leftmost coordinate of shipyard" },
	{"y1",				ENTITY_SHIPYARD_Y1,				ENT_NUMBER,		"Topmost coordinate of shipyard" },
	{"x2",				ENTITY_SHIPYARD_X2,				ENT_NUMBER,		"Rightmost coordinate of shipyard" },
	{"y2",				ENTITY_SHIPYARD_Y2,				ENT_NUMBER,		"Bottommost coordinate of shipyard" },
	{"description",		ENTITY_SHIPYARN_DESCRIPTION,	ENT_STRING,		"Description given to buyer to give directions to shipyard" },
	{NULL,				0,								ENT_UNKNOWN,	NULL }
};

ENT_FIELD entity_shop_stock[] = {
	{"level",			ENTITY_STOCK_LEVEL,				ENT_NUMBER,				"Level required to purhase stock" },
	{"silver",			ENTITY_STOCK_SILVER,			ENT_NUMBER,				"Silver/gold currency cost" },
	{"mp",				ENTITY_STOCK_MP,				ENT_NUMBER,				"Mission point cost" },
	{"dp",				ENTITY_STOCK_DP,				ENT_NUMBER,				"Deity point cost" },
	{"pneuma",			ENTITY_STOCK_PNEUMA,			ENT_NUMBER,				"Pneuma cost" },
	{"reppoints",		ENTITY_STOCK_REP_POINTS,		ENT_NUMBER,				"Reputation point cost" },
	{"paragon",			ENTITY_STOCK_PARAGON_LEVELS,	ENT_NUMBER,				"Paragon level cost" },
	{"custom",			ENTITY_STOCK_CUSTOM_PRICE,		ENT_STRING,				"Custom pricing" },
	{"check",			ENTITY_STOCK_CHECK_PRICE,		ENT_WIDEVNUM,			"Custom pricing check script widevnum" },
	{"discount",		ENTITY_STOCK_DISCOUNT,			ENT_NUMBER,				"Stock discount" },
	{"quantity",		ENTITY_STOCK_QUANTITY,			ENT_NUMBER,				"Current quantity available" },
	{"maxquantity",		ENTITY_STOCK_MAX_QUANTITY,		ENT_NUMBER,				"Maximum quantity available" },
	{"restock",			ENTITY_STOCK_RESTOCK_RATE,		ENT_NUMBER,				"Restocking rate" },
	{"mobile",			ENTITY_STOCK_MOBILE,			ENT_MOBINDEX,			"Index for mobile stock item" },
	{"object",			ENTITY_STOCK_OBJECT,			ENT_OBJINDEX,			"Index for object stock item" },
	{"ship",			ENTITY_STOCK_SHIP,				ENT_SHIPINDEX,			"Index for ship stock item" },
	{"type",			ENTITY_STOCK_TYPE,				ENT_STAT,				"Type of stock item" },
	{"duration",		ENTITY_STOCK_DURATION,			ENT_NUMBER,				"Duration of item after purchase" },
	{"keyword",			ENTITY_STOCK_CUSTOM_KEYWORD,	ENT_STRING,				"Custom keyword (for custom item)" },
	{"description",		ENTITY_STOCK_CUSTOM_DESCRIPTION,ENT_STRING,				"Custom description (for custem item)" },
	{"singular",		ENTITY_STOCK_SINGULAR,			ENT_BOOLEAN,			"Indicates whether you can have only one" },
	{"reputation",		ENTITY_STOCK_REPUTATION,		ENT_REPUTATION_INDEX,	"Reputation required to purchase item" },
	{"minrank",			ENTITY_STOCK_MIN_RANK,			ENT_REPUTATION_RANK,	"Minimum reputation rank needed to purchase item" },
	{"maxrank",			ENTITY_STOCK_MAX_RANK,			ENT_REPUTATION_RANK,	"Maximum reputation rank needed to purchase item" },
	{"minshow",			ENTITY_STOCK_MIN_SHOW_RANK,		ENT_REPUTATION_RANK,	"Minimum reputation rank needed to see the item" },
	{"maxshow",			ENTITY_STOCK_MAX_SHOW_RANK,		ENT_REPUTATION_RANK,	"Maximum reputation rank needed to see the item" },
	{"shop",			ENTITY_STOCK_SHOP,				ENT_SHOP,				"Shop reference" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

ENT_FIELD entity_crew[] = {
	{"scouting",			ENTITY_CREW_SCOUTING,		ENT_NUMBER,		"Skill as a scout" },
	{"gunning",				ENTITY_CREW_GUNNING,		ENT_NUMBER,		"Skill as a gunner" },
	{"oarring",				ENTITY_CREW_OARRING,		ENT_NUMBER,		"Skill as an oarsman" },
	{"mechanics",			ENTITY_CREW_MECHANICS,		ENT_NUMBER,		"Skill as a mechanic" },
	{"navigation",			ENTITY_CREW_NAVIGATION,		ENT_NUMBER,		"Skill as a navigator" },
	{"leadership",			ENTITY_CREW_LEADERSHIP,		ENT_NUMBER,		"Skill as a commander" },
	{NULL,				0,								ENT_UNKNOWN,			NULL }
};

struct _entity_type_info entity_type_info[] = {
	{ ENT_PRIMARY,		ENT_PRIMARY,		entity_primary,				true,	false },
	{ ENT_BOOLEAN,		ENT_BOOLEAN,		entity_boolean,				false,	false },
	{ ENT_NUMBER,		ENT_NUMBER,			entity_number,				false,	false },
	{ ENT_STRING,		ENT_STRING,			entity_string,				false,	false },
	{ ENT_MOBILE,		ENT_MOBILE,			entity_mobile,				true,	false },
	{ ENT_OBJECT,		ENT_OBJECT,			entity_object,				true,	false },
	{ ENT_ROOM,			ENT_ROOM,			entity_room,				true,	false },
	{ ENT_ARRAY_EXITS,	ENT_ARRAY_EXITS,	entity_array_exits,			false,	true },
	{ ENT_EXIT,			ENT_EXIT,			entity_exit,				false,	false },
	{ ENT_TOKEN,		ENT_TOKEN,			entity_token,				true,	false },
	{ ENT_AREA,			ENT_AREA,			entity_area,				true,	false },
	{ ENT_SKILL,		ENT_SKILL,			entity_skill,				false,	false },
	{ ENT_SKILLGROUP,	ENT_SKILLGROUP,		entity_skillgroups,			false,	false },
	{ ENT_SKILLENTRY,	ENT_SKILLENTRY,		entity_skillentry,			false,	false },
	{ ENT_SKILLINFO,	ENT_SKILLINFO,		entity_skill_info,			false,	false },
	{ ENT_SPELL,		ENT_SPELL,			entity_spelldata,			false,	false },
	{ ENT_SONG,			ENT_SONG,			entity_song,				false,	false },
	{ ENT_RACE,			ENT_RACE,			entity_race,				false,	false },
	{ ENT_CLASS,		ENT_CLASS,			entity_class,				false,	false },
	{ ENT_CLASSLEVEL,	ENT_CLASSLEVEL,		entity_classlevel,			false,	false },
	{ ENT_MISSION,		ENT_MISSION,		entity_mission,				false,	false },
	{ ENT_MISSION_PART,	ENT_MISSION_PART,	entity_mission_part,		false,	false },
	{ ENT_CONN,			ENT_CONN,			entity_conn,				false,	false },
	{ ENT_AFFECT,		ENT_AFFECT,			entity_affect,				false,	false },
	{ ENT_BOOK_PAGE,	ENT_BOOK_PAGE,		entity_book_page,			false,	false },
	{ ENT_FOOD_BUFF,	ENT_FOOD_BUFF,		entity_food_buff,			false,	false },
	{ ENT_COMPARTMENT,	ENT_COMPARTMENT,	entity_compartment,			false,	false },
	{ ENT_INK_TYPE,		ENT_INK_TYPE,		entity_ink_type,			false,	false },
	{ ENT_INSTRUMENT_RESERVOIR, ENT_INSTRUMENT_RESERVOIR, entity_instrument_reservoir, false, false },
	{ ENT_WEAPON_ATTACK,	ENT_WEAPON_ATTACK,	entity_weapon_attack,	false,	false },
	{ ENT_ATTACK,		ENT_ATTACK,			entity_attack_type,			false,	false },
	{ ENT_ADORNMENTS,	ENT_ADORNMENTS,		entity_adornments,			false,	false },
	{ ENT_ADORNMENT,	ENT_ADORNMENT,		entity_adornment,			false,	false },
	{ ENT_SHOP,			ENT_SHOP,			entity_shop,				false,	false },
	{ ENT_SHOP_SHIPYARD,	ENT_SHOP_SHIPYARD,	entity_shop_shipyard,	false,	false },
	{ ENT_SHOP_STOCK,	ENT_SHOP_STOCK,		entity_shop_stock,			false,	false },
	{ ENT_CREW,			ENT_CREW,			entity_crew,				false,	false },
	{ ENT_LIQUID,		ENT_LIQUID,			entity_liquid,				false,	false },
	{ ENT_MATERIAL,		ENT_MATERIAL,		entity_material,			false,	false },
	{ ENT_SECTOR,		ENT_SECTOR,			entity_sector,				false,	false },
	{ ENT_AFFINITY,		ENT_AFFINITY,		entity_affinity,			false,	false },
	{ ENT_WAYPOINT,		ENT_WAYPOINT,		entity_waypoint,			false,	false },
	{ ENT_EXTRADESC,	ENT_EXTRADESC,		NULL,						false,	false },
	{ ENT_HELP,			ENT_HELP,			NULL,						false,	false },
	{ ENT_PRIOR,		ENT_PRIOR,			entity_prior,				false,	false },
	{ ENT_MOBILE_ID,	ENT_MOBILE_ID,		entity_mobile,				false,	false },
	{ ENT_OBJECT_ID,	ENT_OBJECT_ID,		entity_object,				false,	false },
	{ ENT_TOKEN_ID,		ENT_TOKEN_ID,		entity_token,				false,	false },
	{ ENT_AREA_ID,		ENT_AREA_ID,		entity_area,				false,	false },
	{ ENT_SKILLINFO_ID,	ENT_SKILLINFO_ID,	entity_skill_info,			false,	false },
	{ ENT_CLONE_ROOM,	ENT_WILDS_ROOM,		entity_room,				false,	false },
	{ ENT_CLONE_DOOR,	ENT_WILDS_DOOR,		entity_exit,				false,	false },
	{ ENT_WILDS,		ENT_WILDS,			entity_wilds,				false,	false },
	{ ENT_GAME,			ENT_GAME,			entity_game,				false,	false },
	{ ENT_CHURCH,		ENT_CHURCH,			entity_church,				false,	false },
	{ ENT_BLLIST_MIN,	ENT_BLLIST_MAX,		entity_list,				false,	true },
	{ ENT_PLLIST_MIN,	ENT_PLLIST_MAX,		entity_list,				false,	true },
	{ ENT_OLLIST_MIN,	ENT_OLLIST_MAX,		entity_list,				false,	true },
	{ ENT_ILLIST_MIN,	ENT_ILLIST_MAX,		NULL,						false,	false },
	{ ENT_NULL,			ENT_NULL,			NULL,						false,	false },
	{ ENT_VARIABLE,		ENT_VARIABLE,		entity_variable,			false,	false },
	{ ENT_PERSIST,		ENT_PERSIST,		entity_persist,				false,	false },
	{ ENT_GROUP,		ENT_GROUP,			entity_group,				false,	false },
	{ ENT_DICE,			ENT_DICE,			entity_dice,				false,	false },
	{ ENT_RANGE,		ENT_RANGE,			entity_range,				false,	false },
	{ ENT_MOBINDEX,		ENT_MOBINDEX,		entity_mobindex,			false,	false },
	{ ENT_OBJINDEX,		ENT_OBJINDEX,		entity_objindex,			false,	false },
	{ ENT_TOKENINDEX,		ENT_TOKENINDEX,		entity_tokenindex,			false,	false },
	{ ENT_SECTION,		ENT_SECTION,		entity_instance_section,	false,	false },
	{ ENT_INSTANCE,		ENT_INSTANCE,		entity_instance,			true,	false },
	{ ENT_DUNGEON,		ENT_DUNGEON,		entity_dungeon,				true,	false },
	{ ENT_BLUEPRINT_SECTION,		ENT_BLUEPRINT_SECTION,		entity_blueprint_section,	false,	false },
	{ ENT_BLUEPRINT,		ENT_BLUEPRINT,		entity_blueprint,			false,	false },
	{ ENT_DUNGEONINDEX,		ENT_DUNGEONINDEX,		entity_dungeon_index,				false,	false },
	{ ENT_WIDEVNUM,		ENT_WIDEVNUM,		entity_widevnum,			false,	false },
	{ ENT_OBJECT_AMMO, ENT_OBJECT_AMMO, entity_object_ammo, false,	false },
	{ ENT_OBJECT_ARMOR, ENT_OBJECT_ARMOR, entity_object_armor, false,	false },
	{ ENT_OBJECT_BOOK, ENT_OBJECT_BOOK, entity_object_book, false,	false },
	{ ENT_OBJECT_CART, ENT_OBJECT_CART, entity_object_cart, false,	false },
	{ ENT_OBJECT_COMPASS, ENT_OBJECT_COMPASS, entity_object_compass, false,	false },
	{ ENT_OBJECT_CONTAINER, ENT_OBJECT_CONTAINER, entity_object_container, false,	false },
	{ ENT_OBJECT_FLUID_CONTAINER, ENT_OBJECT_FLUID_CONTAINER, entity_object_fluid_container, false, false },
	{ ENT_OBJECT_FOOD, ENT_OBJECT_FOOD, entity_object_food, false,	false },
	{ ENT_OBJECT_FURNITURE, ENT_OBJECT_FURNITURE, entity_object_furniture, false,	false },
	{ ENT_OBJECT_INK, ENT_OBJECT_INK, entity_object_ink, false,	false },
	{ ENT_OBJECT_INSTRUMENT, ENT_OBJECT_INSTRUMENT, entity_object_instrument, false,	false },
	{ ENT_OBJECT_JEWELRY, ENT_OBJECT_JEWELRY, entity_object_jewelry, false,	false },
	{ ENT_OBJECT_LIGHT, ENT_OBJECT_LIGHT, entity_object_light, false,	false },
	{ ENT_OBJECT_MAP, ENT_OBJECT_MAP, entity_object_map, false,	false },
	{ ENT_OBJECT_MIST, ENT_OBJECT_MIST, entity_object_mist, false,	false },
	{ ENT_OBJECT_MONEY, ENT_OBJECT_MONEY, entity_object_money, false,	false },
	{ ENT_OBJECT_PAGE, ENT_OBJECT_PAGE, entity_book_page, false,	false },
	{ ENT_OBJECT_PORTAL, ENT_OBJECT_PORTAL, entity_object_portal, false,	false },
	{ ENT_OBJECT_SCROLL, ENT_OBJECT_SCROLL, entity_object_scroll, false,	false },
	{ ENT_OBJECT_SEXTANT, ENT_OBJECT_SEXTANT, entity_object_sextant, false,	false },
	{ ENT_OBJECT_TATTOO, ENT_OBJECT_TATTOO, entity_object_tattoo, false,	false },
	{ ENT_OBJECT_TELESCOPE, ENT_OBJECT_TELESCOPE, entity_object_telescope, false,	false },
	{ ENT_OBJECT_WAND, ENT_OBJECT_WAND, entity_object_wand, false,	false },
	{ ENT_OBJECT_WEAPON, ENT_OBJECT_WEAPON, entity_object_weapon, false,	false },
	{ ENT_SKILL_VALUES, ENT_SKILL_VALUES, NULL, false, true },
	{ ENT_SKILL_VALUENAMES, ENT_SKILL_VALUENAMES, NULL, false, true },
	{ ENT_REPUTATION, ENT_REPUTATION, entity_reputation, false, false },
	{ ENT_REPUTATION_INDEX, ENT_REPUTATION_INDEX, entity_reputation_index, false, false },
	{ ENT_REPUTATION_RANK, ENT_REPUTATION_RANK, entity_reputation_rank, false, false },
	{ ENT_UNKNOWN,		ENT_UNKNOWN,		NULL,						false,	false },
};



#if 0
// Trigger types - Keep this in case we need to reset triggers for some reason
struct trigger_type trigger_table	[] = {
//	name,					alias, 		type,					slot,					mob?,	obj?,	room?,	token?, area?, instance?, dungeon?
{	"act",					NULL,		TRIG_ACT,				TRIGSLOT_ACTION,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"afterdeath",			NULL,		TRIG_AFTERDEATH,		TRIGSLOT_REPOP,			PRG_TPROG	},
{	"afterkill",			NULL,		TRIG_AFTERKILL,			TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"animate",				NULL,		TRIG_ANIMATE,			TRIGSLOT_ANIMATE,		(PRG_MPROG|PRG_TPROG)	},
{	"assist",				NULL,		TRIG_ASSIST,			TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_TPROG)	},
{	"attack_backstab",		NULL,		TRIG_ATTACK_BACKSTAB,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_bash",			NULL,		TRIG_ATTACK_BASH,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_behead",		NULL,		TRIG_ATTACK_BEHEAD,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_bite",			NULL,		TRIG_ATTACK_BITE,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_blackjack",		NULL,		TRIG_ATTACK_BLACKJACK,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_circle",		NULL,		TRIG_ATTACK_CIRCLE,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_counter",		NULL,		TRIG_ATTACK_COUNTER,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_cripple",		NULL,		TRIG_ATTACK_CRIPPLE,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_dirtkick",		NULL,		TRIG_ATTACK_DIRTKICK,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_disarm",		NULL,		TRIG_ATTACK_DISARM,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_intimidate",	NULL,		TRIG_ATTACK_INTIMIDATE,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_kick",			NULL,		TRIG_ATTACK_KICK,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_rend",			NULL,		TRIG_ATTACK_REND,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_slit",			NULL,		TRIG_ATTACK_SLIT,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_smite",			NULL,		TRIG_ATTACK_SMITE,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_tailkick",		NULL,		TRIG_ATTACK_TAILKICK,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_trample",		NULL,		TRIG_ATTACK_TRAMPLE,	TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"attack_turn",			NULL,		TRIG_ATTACK_TURN,		TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"barrier",				NULL,		TRIG_BARRIER,			TRIGSLOT_ATTACKS,		(PRG_MPROG|PRG_TPROG)	},
{	"blow",					NULL,		TRIG_BLOW,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"blueprint_schematic",	NULL,		TRIG_BLUEPRINT_SCHEMATIC, TRIGSLOT_GENERAL,		PRG_IPROG	},
{	"book_page",			NULL,		TRIG_BOOK_PAGE,			TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_page_prerip",		NULL,		TRIG_BOOK_PAGE_PRERIP,	TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_page_rip",		NULL,		TRIG_BOOK_PAGE_RIP,		TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_preread",			NULL,		TRIG_BOOK_PREREAD,		TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_preseal",			NULL,		TRIG_BOOK_PRESEAL,		TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_prewrite",		NULL,		TRIG_BOOK_PREWRITE,		TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_read",			NULL,		TRIG_BOOK_READ,			TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_seal",			NULL,		TRIG_BOOK_SEAL,			TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_write_text",		NULL,		TRIG_BOOK_WRITE_TEXT,	TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"book_write_title",		NULL,		TRIG_BOOK_WRITE_TITLE,	TRIGSLOT_GENERAL		(PRG_OPROG|PRG_TPROG)},
{	"board",				NULL,		TRIG_BOARD,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"brandish",				NULL,		TRIG_BRANDISH,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"bribe",				NULL,		TRIG_BRIBE,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"bury",					NULL,		TRIG_BURY,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"buy",					NULL,		TRIG_BUY,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"caninterrupt",			NULL,		TRIG_CANINTERRUPT,		TRIGSLOT_INTERRUPT,		PRG_TPROG	},
{	"catalyst",				NULL,		TRIG_CATALYST,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"catalystfull",			NULL,		TRIG_CATALYST_FULL,		TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"catalystsrc",			NULL,		TRIG_CATALYST_SOURCE,	TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"check_buyer",			NULL,		TRIG_CHECK_BUYER,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"check_damage",			NULL,		TRIG_CHECK_DAMAGE,		TRIGSLOT_DAMAGE,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"clone_extract",		NULL,		TRIG_CLONE_EXTRACT,		TRIGSLOT_GENERAL,		(PRG_RPROG|PRG_TPROG)	},
{	"close",				NULL,		TRIG_CLOSE,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"combatstyle",			NULL,		TRIG_COMBAT_STYLE,		TRIGSLOT_COMBATSTYLE,	PRG_TPROG	},	// Only on tokens
{	"completed",			NULL,		TRIG_COMPLETED,			TRIGSLOT_REPOP,			(PRG_IPROG|PRG_DPROG)	},
{	"contract_complete",	NULL,		TRIG_CONTRACT_COMPLETE,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"custom_price",			NULL,		TRIG_CUSTOM_PRICE,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"damage",				NULL,		TRIG_DAMAGE,			TRIGSLOT_DAMAGE,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"death",				NULL,		TRIG_DEATH,				TRIGSLOT_REPOP,			(PRG_MPROG|PRG_RPROG|PRG_TPROG)	},
{	"death_protection",		NULL,		TRIG_DEATH_PROTECTION,	TRIGSLOT_REPOP,			(PRG_MPROG|PRG_RPROG|PRG_TPROG)	},
{	"death_timer",			NULL,		TRIG_DEATH_TIMER,		TRIGSLOT_REPOP,			(PRG_MPROG|PRG_RPROG|PRG_TPROG)	},
{	"defense",				NULL,		TRIG_DEFENSE,			TRIGSLOT_COMBATSTYLE,	(PRG_MPROG|PRG_TPROG)	},	// Only on tokens
{	"delay",				NULL,		TRIG_DELAY,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"drink",				NULL,		TRIG_DRINK,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"drop",					NULL,		TRIG_DROP,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"dungeon_schematic",	NULL,		TRIG_DUNGEON_SCHEMATIC,	TRIGSLOT_GENERAL,		PRG_DPROG	},
{	"eat",					NULL,		TRIG_EAT,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"emote",				NULL,		TRIG_EMOTE,				TRIGSLOT_ACTION,		(PRG_MPROG|PRG_TPROG)	},
{	"emoteat",				NULL,		TRIG_EMOTEAT,			TRIGSLOT_ACTION,		(PRG_MPROG|PRG_TPROG)	},
{	"emoteself",			NULL,		TRIG_EMOTESELF,			TRIGSLOT_ACTION,		PRG_TPROG	},
{	"emptied",				NULL,		TRIG_EMPTIED,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)},
{	"entry",				NULL,		TRIG_ENTRY,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_IPROG|PRG_DPROG)	},
{	"exall",				NULL,		TRIG_EXALL,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"examine",				NULL,		TRIG_EXAMINE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"exit",					NULL,		TRIG_EXIT,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"expire",				NULL,		TRIG_EXPIRE,			TRIGSLOT_REPOP,			PRG_TPROG	},
{	"extract",				NULL,		TRIG_EXTRACT,			TRIGSLOT_REPOP,			(PRG_MPROG|PRG_OPROG|PRG_RPROG)	},
{	"failed",				NULL,		TRIG_FAILED,			TRIGSLOT_REPOP,			(PRG_IPROG|PRG_DPROG)	},
{	"fight",				NULL,		TRIG_FIGHT,				TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"fill",					NULL,		TRIG_FILL,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)},
{	"filled",				NULL,		TRIG_FILLED,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)},
{	"flee",					NULL,		TRIG_FLEE,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"forcedismount",		NULL,		TRIG_FORCEDISMOUNT,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"get",					NULL,		TRIG_GET,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"give",					NULL,		TRIG_GIVE,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"grall",				NULL,		TRIG_GRALL,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"greet",				NULL,		TRIG_GREET,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"grouped",				NULL,		TRIG_GROUPED,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"grow",					NULL,		TRIG_GROW,				TRIGSLOT_RANDOM,		(PRG_OPROG|PRG_TPROG)	},
{	"hidden",				NULL,		TRIG_HIDDEN,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"hide",					NULL,		TRIG_HIDE,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"hit",					NULL,		TRIG_HIT,				TRIGSLOT_HITS,			(PRG_MPROG|PRG_TPROG)	},
{	"hitgain",				NULL,		TRIG_HITGAIN,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_TPROG)	},
{	"hpcnt",				NULL,		TRIG_HPCNT,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"identify",				NULL,		TRIG_IDENTIFY,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"inspect",				NULL,		TRIG_INSPECT,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"inspect_custom",		NULL,		TRIG_INSPECT_CUSTOM,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"interrupt",			NULL,		TRIG_INTERRUPT,			TRIGSLOT_INTERRUPT,		PRG_TPROG	},
{	"kill",					NULL,		TRIG_KILL,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"knock",				NULL,		TRIG_KNOCK,				TRIGSLOT_GENERAL,		(PRG_RPROG|PRG_TPROG)	},
{	"knocking",				NULL,		TRIG_KNOCKING,			TRIGSLOT_GENERAL,		(PRG_RPROG|PRG_TPROG)	},
{	"land",					NULL,		TRIG_LAND,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_TPROG)	},
{	"level",				NULL,		TRIG_LEVEL,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"lock",					NULL,		TRIG_LOCK,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"login",				NULL,		TRIG_LOGIN,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"lore",					NULL,		TRIG_LORE,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"lorex",				NULL,		TRIG_LORE_EX,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"managain",				NULL,		TRIG_MANAGAIN,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_TPROG)	},
{	"moon",					NULL,		TRIG_MOON,				TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"mount",				NULL,		TRIG_MOUNT,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"move_char",			NULL,		TRIG_MOVE_CHAR,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_RPROG|PRG_TPROG)	},
{	"movegain",				NULL,		TRIG_MOVEGAIN,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_TPROG)	},
{	"multiclass",			NULL,		TRIG_MULTICLASS,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"open",					NULL,		TRIG_OPEN,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"postquest",			NULL,		TRIG_POSTQUEST,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"pour",					NULL,		TRIG_POUR,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)},
{	"practice",				NULL,		TRIG_PRACTICE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"practicetoken",		NULL,		TRIG_PRACTICETOKEN,		TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"preanimate",			NULL,		TRIG_PREANIMATE,		TRIGSLOT_REPOP,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"preassist",			NULL,		TRIG_PREASSIST,			TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_TPROG)	},
{	"prebite",				NULL,		TRIG_PREBITE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prebuy",				NULL,		TRIG_PREBUY,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prebuy_obj",			NULL,		TRIG_PREBUY_OBJ,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"precast",				NULL,		TRIG_PRECAST,			TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"predeath",				NULL,		TRIG_PREDEATH,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"predismount",			NULL,		TRIG_PREDISMOUNT,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"predrink",				NULL,		TRIG_PREDRINK,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"predrop",				NULL,		TRIG_PREDROP,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"preeat",				NULL,		TRIG_PREEAT,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"preenter",				NULL,		TRIG_PREENTER,			TRIGSLOT_MOVE,			(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"preflee",				NULL,		TRIG_PREFLEE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"preget",				NULL,		TRIG_PREGET,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"prehide",				NULL,		TRIG_PREHIDE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"prehidein",			NULL,		TRIG_PREHIDE_IN,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"prekill",				NULL,		TRIG_PREKILL,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prelock",				NULL,		TRIG_PRELOCK,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"premount",				NULL,		TRIG_PREMOUNT,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prepractice",			NULL,		TRIG_PREPRACTICE,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prepracticeother",		NULL,		TRIG_PREPRACTICEOTHER,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prepracticethat",		NULL,		TRIG_PREPRACTICETHAT,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prepracticetoken",		NULL,		TRIG_PREPRACTICETOKEN,	TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"preput",				NULL,		TRIG_PREPUT,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"prequest",				NULL,		TRIG_PREQUEST,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prerecall",			NULL,		TRIG_PRERECALL,			TRIGSLOT_GENERAL,		(PRG_RPROG|PRG_TPROG)	},
{	"prerecite",			NULL,		TRIG_PRERECITE,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prereckoning",			NULL,		TRIG_PRERECKONING,		TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prerehearse",			NULL,		TRIG_PREREHEARSE,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"preremove",			NULL,		TRIG_PREREMOVE,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"prerenew",				NULL,		TRIG_PRERENEW,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"prerest",				NULL,		TRIG_PREREST,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"preresurrect",			NULL,		TRIG_PRERESURRECT,		TRIGSLOT_REPOP,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"preround",				NULL,		TRIG_PREROUND,			TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_TPROG)	},
{	"presell",				NULL,		TRIG_PRESELL,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"presit",				NULL,		TRIG_PRESIT,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"presleep",				NULL,		TRIG_PRESLEEP,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prespell",				NULL,		TRIG_PRESPELL,			TRIGSLOT_SPELL,			PRG_TPROG	},
{	"prestand",				NULL,		TRIG_PRESTAND,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prestepoff",			NULL,		TRIG_PRESTEPOFF,		TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"pretrain",				NULL,		TRIG_PRETRAIN,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"pretraintoken",		NULL,		TRIG_PRETRAINTOKEN,		TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"preunlock",			NULL,		TRIG_PREUNLOCK,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prewake",				NULL,		TRIG_PREWAKE,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"prewear",				NULL,		TRIG_PREWEAR,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"prewimpy",				NULL,		TRIG_PREWIMPY,			TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"pull",					NULL,		TRIG_PULL,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"pullon",				NULL,		TRIG_PULL_ON,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"pulse",				NULL,		TRIG_PULSE,				TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_TPROG)	},
{	"push",					NULL,		TRIG_PUSH,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"pushon",				NULL,		TRIG_PUSH_ON,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"put",					NULL,		TRIG_PUT,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"quest_cancel",			NULL,		TRIG_QUEST_CANCEL,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"quest_complete",		NULL,		TRIG_QUEST_COMPLETE,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"quest_incomplete",		NULL,		TRIG_QUEST_INCOMPLETE,	TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"quest_part",			NULL,		TRIG_QUEST_PART,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"quit",					NULL,		TRIG_QUIT,				TRIGSLOT_GENERAL,		PRG_TPROG	},
{	"random",				NULL,		TRIG_RANDOM,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG|PRG_APROG|PRG_IPROG|PRG_DPROG)	},
{	"recall",				NULL,		TRIG_RECALL,			TRIGSLOT_GENERAL,		(PRG_RPROG|PRG_TPROG)	},
{	"recite",				NULL,		TRIG_RECITE,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"reckoning",			NULL,		TRIG_RECKONING,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"regen",				NULL,		TRIG_REGEN,				TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"regen_hp",				NULL,		TRIG_REGEN_HP,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"regen_mana",			NULL,		TRIG_REGEN_MANA,		TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"regen_move",			NULL,		TRIG_REGEN_MOVE,		TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"remort",				NULL,		TRIG_REMORT,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"remove",				NULL,		TRIG_REMOVE,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"renew",				NULL,		TRIG_RENEW,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"renew_list",			NULL,		TRIG_RENEW_LIST,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"repop",				NULL,		TRIG_REPOP,				TRIGSLOT_REPOP,			(PRG_MPROG|PRG_OPROG|PRG_TPROG|PRG_IPROG|PRG_DPROG)	},
{	"reset",				NULL,		TRIG_RESET,				TRIGSLOT_REPOP,			(PRG_RPROG|PRG_TPROG|PRG_APROG|PRG_IPROG|PRG_DPROG)	},
{	"rest",					NULL,		TRIG_REST,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"restocked",			NULL,		TRIG_RESTOCKED,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"restore",				NULL,		TRIG_RESTORE,			TRIGSLOT_REPOP,			(PRG_MPROG|PRG_TPROG)	},
{	"resurrect",			NULL,		TRIG_RESURRECT,			TRIGSLOT_REPOP,			(PRG_MPROG|PRG_TPROG)	},
{	"save",					NULL,		TRIG_SAVE,				TRIGSLOT_REPOP,			(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"sayto",				NULL,		TRIG_SAYTO,				TRIGSLOT_SPEECH,		(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"showcommands",			NULL,		TRIG_SHOWCOMMANDS,		TRIGSLOT_VERB,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"showexit",				NULL,		TRIG_SHOWEXIT,			TRIGSLOT_MOVE,			(PRG_RPROG|PRG_TPROG)	},
{	"sit",					NULL,		TRIG_SIT,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"skill_berserk",		NULL,		TRIG_SKILL_BERSERK,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"skill_sneak",			NULL,		TRIG_SKILL_SNEAK,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"skill_warcry",			NULL,		TRIG_SKILL_WARCRY,		TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"sleep",				NULL,		TRIG_SLEEP,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"speech",				NULL,		TRIG_SPEECH,			TRIGSLOT_SPEECH,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"spell",				NULL,		TRIG_SPELL,				TRIGSLOT_SPELL,			PRG_TPROG	},
{	"spellbeat",			NULL,		TRIG_SPELLBEAT,			TRIGSLOT_SPELL,			PRG_TPROG	},
{	"spellcast",			NULL,		TRIG_SPELLCAST,			TRIGSLOT_SPELL,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"spellinter",			NULL,		TRIG_SPELLINTER,		TRIGSLOT_SPELL,			PRG_TPROG	},
{	"spellpenetrate",		NULL,		TRIG_SPELLPENETRATE,	TRIGSLOT_SPELL,			PRG_TPROG	},
{	"spellreflect",			NULL,		TRIG_SPELLREFLECT,		TRIGSLOT_SPELL,			(PRG_MPROG|PRG_TPROG)	},
{	"spell_cure",			NULL,		TRIG_SPELL_CURE,		TRIGSLOT_SPELL,			(PRG_MPROG|PRG_TPROG)	},
{	"spell_dispel",			NULL,		TRIG_SPELL_DISPEL,		TRIGSLOT_SPELL,			(PRG_MPROG|PRG_OPROG|PRG_TPROG)	},
{	"stand",				NULL,		TRIG_STAND,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"startcombat",			NULL,		TRIG_START_COMBAT,		TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"stepoff",				NULL,		TRIG_STEPOFF,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"stripaffect",			NULL,		TRIG_STRIPAFFECT,		TRIGSLOT_REPOP,			PRG_TPROG	},
{	"takeoff",				NULL,		TRIG_TAKEOFF,			TRIGSLOT_MOVE,			(PRG_MPROG|PRG_TPROG)	},
{	"throw",				NULL,		TRIG_THROW,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"tick",					NULL,		TRIG_TICK,				TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG|PRG_APROG|PRG_IPROG|PRG_DPROG)	},
{	"token_given",			NULL,		TRIG_TOKEN_GIVEN,		TRIGSLOT_REPOP,			PRG_TPROG	},
{	"token_removed",		NULL,		TRIG_TOKEN_REMOVED,		TRIGSLOT_REPOP,			PRG_TPROG	},
{	"touch",				NULL,		TRIG_TOUCH,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"toxingain",			NULL,		TRIG_TOXINGAIN,			TRIGSLOT_RANDOM,		(PRG_MPROG|PRG_TPROG)	},
{	"turn",					NULL,		TRIG_TURN,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"turnon",				NULL,		TRIG_TURN_ON,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"ungrouped",			NULL,		TRIG_UNGROUPED,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"unlock",				NULL,		TRIG_UNLOCK,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"use",					NULL,		TRIG_USE,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"usewith",				NULL,		TRIG_USEWITH,			TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	"verb",					NULL,		TRIG_VERB,				TRIGSLOT_VERB,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"verbself",				NULL,		TRIG_VERBSELF,			TRIGSLOT_VERB,			PRG_TPROG	},
{	"wake",					NULL,		TRIG_WAKE,				TRIGSLOT_MOVE,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"weapon_blocked",		NULL,		TRIG_WEAPON_BLOCKED,	TRIGSLOT_ATTACKS,		(PRG_OPROG|PRG_TPROG)	},
{	"weapon_caught",		NULL,		TRIG_WEAPON_CAUGHT,		TRIGSLOT_ATTACKS,		(PRG_OPROG|PRG_TPROG)	},
{	"weapon_parried",		NULL,		TRIG_WEAPON_PARRIED,	TRIGSLOT_ATTACKS,		(PRG_OPROG|PRG_TPROG)	},
{	"wear",					NULL,		TRIG_WEAR,				TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"whisper",				NULL,		TRIG_WHISPER,			TRIGSLOT_SPEECH,		(PRG_MPROG|PRG_TPROG)	},
{	"wimpy",				NULL,		TRIG_WIMPY,				TRIGSLOT_FIGHT,			(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"xpbonus",				NULL,		TRIG_XPBONUS,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"xpcompute",			NULL,		TRIG_XPCOMPUTE,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_TPROG)	},
{	"xpgain",				NULL,		TRIG_XPGAIN,			TRIGSLOT_GENERAL,		(PRG_MPROG|PRG_OPROG|PRG_RPROG|PRG_TPROG)	},
{	"zap",					NULL,		TRIG_ZAP,				TRIGSLOT_GENERAL,		(PRG_OPROG|PRG_TPROG)	},
{	NULL,					NULL,		0,						TRIGSLOT_GENERAL,		0	}
};
int trigger_table_size = elementsof(trigger_table);
#endif

IFCHECK_DATA ifcheck_table[] = {
	// name					prog type	params	return	function				help reference
	{ "abs",				IFC_ANY,	"N",	true,	ifc_abs,				"ifcheck abs" },
	{ "affectbit",			IFC_ANY,	"ES",	true,	ifc_affectbit,			"ifcheck affectbit" },
	{ "affectbit2",			IFC_ANY,	"ES",	true,	ifc_affectbit2,			"ifcheck affectbit2" },
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
	{ "class",				IFC_NONE,	"ES",	false,	ifc_class,				"ifcheck class" },
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
	{ "gc",					IFC_ANY,	"E",	false,	ifc_gc,					"ifcheck gc" },
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
	{ "hascatalyst",		IFC_ANY,	"ES",	false,	ifc_hascatalyst,		"ifcheck hascatalyst" },
	{ "hascheckpoint",		IFC_ANY,	"ES",	false,	ifc_hascheckpoint,		"ifcheck hascheckpoint" },
	{ "hasclass",			IFC_ANY,	"ES",	false,	ifc_hasclass,			"ifcheck hasclass" },
	{ "hasenviroment",		IFC_ANY,	"ES",	true,	ifc_hasenvironment,		"ifcheck hasenvironment" },
	{ "hasfaction",			IFC_ANY,	"E",	false,	ifc_hasfaction,			"ifcheck hasfaction" },
	{ "hasprompt",			IFC_ANY,	"E",	false,	ifc_hasprompt,			"ifcheck hasprompt" },
	{ "hasqueue",			IFC_ANY,	"E",	false,	ifc_hasqueue,			"ifcheck hasqueue" },
	{ "hasreputation",		IFC_ANY,	"E",	false,	ifc_hasreputation,		"ifcheck hasreputation" },
	{ "hasship",			IFC_NONE,	"E",	false,	ifc_hasship,			"ifcheck hasship" },
	{ "hasspell",			IFC_ANY,	"ES",	false,	ifc_hasspell,			"ifcheck hasspell" },
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
	{ "isbook",				IFC_ANY,	"E",	false,	ifc_isbook,				"ifcheck isbook" },
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
	{ "iscontainer",		IFC_ANY,	"E",	false,	ifc_iscontainer,		"ifcheck iscontainer" },
	{ "iscpkproof",			IFC_ANY,	"E",	false,	ifc_iscpkproof,			"ifcheck iscpkproof" },
	{ "iscrosszone",		IFC_ANY,	"E",	false,	ifc_iscrosszone,		"ifcheck iscrosszone" },
	{ "isdead",				IFC_ANY,	"E",	false,	ifc_isdead,				"ifcheck dead" },
	{ "isdelay",			IFC_ANY,	"E",	false,	ifc_isdelay,			"ifcheck isdelay" },
	{ "isdemon",			IFC_ANY,	"E",	false,	ifc_isdemon,			"ifcheck isdemon" },
	{ "isevil",				IFC_ANY,	"E",	false,	ifc_isevil,				"ifcheck isevil" },
	{ "isexitvisible",		IFC_ANY,	"EES",	false,	ifc_isexitvisible,		"ifcheck isexitvisible" },
	{ "isfading",			IFC_ANY,	"Es",	false,	ifc_isfading,			"ifcheck isfading" },
	{ "isfighting",			IFC_ANY,	"Ee",	false,	ifc_isfighting,			"ifcheck isfighting" },
	{ "isfluidcontainer",	IFC_ANY,	"E",	false,	ifc_isfluidcontainer,	"ifcheck isfluidcontainer" },
	{ "isflying",			IFC_ANY,	"E",	false,	ifc_isflying,			"ifcheck isflying" },
	{ "isfollow",			IFC_ANY,	"E",	false,	ifc_isfollow,			"ifcheck isfollow" },
	{ "isfood",				IFC_ANY,	"E",	false,	ifc_isfood,				"ifcheck isfood" },
	{ "isfurniture",		IFC_ANY,	"E",	false,	ifc_isfurniture,		"ifcheck isfurniture" },
	{ "isgood",				IFC_ANY,	"E",	false,	ifc_isgood,				"ifcheck isgood" },
	{ "ishired",			IFC_ANY,	"E",	false,	ifc_ishired,			"ifcheck ishired" },
	{ "ishunting",			IFC_ANY,	"E",	false,	ifc_ishunting,			"ifcheck ishunting" },
	{ "isimmort",			IFC_ANY,	"E",	false,	ifc_isimmort,			"ifcheck isimmort" },
	{ "iskey",				IFC_ANY,	"E",	false,	ifc_iskey,				"ifcheck iskey" },
	{ "isleader",			IFC_ANY,	"E",	false,	ifc_isleader,			"ifcheck isleader" },
	{ "islight",			IFC_ANY,	"E",	false,	ifc_islight,			"ifcheck islight" },
	{ "ismobile",			IFC_ANY,	"E",	false,	ifc_ismobile,			"ifcheck ismobile" },
	{ "ismoney",			IFC_ANY,	"E",	false,	ifc_ismoney,			"ifcheck ismoney" },
	{ "ismoonup",			IFC_ANY,	"",		false,	ifc_ismoonup,			"ifcheck ismoonup" },
	{ "ismorphed",			IFC_ANY,	"E",	false,	ifc_ismorphed,			"ifcheck ismorphed" },
	{ "ismystic",			IFC_ANY,	"E",	false,	ifc_ismystic,			"ifcheck ismystic" },
	{ "isneutral",			IFC_ANY,	"E",	false,	ifc_isneutral,			"ifcheck isneutral" },
	{ "isnpc",				IFC_ANY,	"E",	false,	ifc_isnpc,				"ifcheck isnpc" },
	{ "isobject",			IFC_ANY,	"E",	false,	ifc_isobject,			"ifcheck isobject" },
	{ "ison",				IFC_ANY,	"E",	false,	ifc_ison,				"ifcheck ison" },
	{ "isowner",			IFC_ANY,	"E",	false,	ifc_isowner,			"ifcheck isowner" },
	{ "ispage",				IFC_ANY,	"E",	false,	ifc_ispage,				"ifcheck ispage" },
	{ "ispc",				IFC_ANY,	"E",	false,	ifc_ispc,				"ifcheck ispc" },
	{ "ispersist",			IFC_ANY,	"E",	false,	ifc_ispersist,			"ifcheck ispersist" },
	{ "ispk",				IFC_ANY,	"E",	false,	ifc_ispk,				"ifcheck ispk" },
	{ "isportal",			IFC_ANY,	"E",	false,	ifc_isportal,			"ifcheck isportal" },
	{ "isprey",				IFC_ANY,	"E",	false,	ifc_isprey,				"ifcheck isprey" },
	{ "isprog",				IFC_ANY,	"ES",	false,	ifc_isprog,				"ifcheck isprog" },
	{ "ispulling",			IFC_ANY,	"Ee",	false,	ifc_ispulling,			"ifcheck ispulling" },
	{ "ispullingrelic",		IFC_ANY,	"E",	false,	ifc_ispullingrelic,		"ifcheck ispullingrelic" },
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
	{ "issustained",		IFC_ANY,	"E",	false,	ifc_issustained,		"ifcheck issustained" },
	{ "istarget",			IFC_ANY,	"E",	false,	ifc_istarget,			"ifcheck istarget" },
	{ "istattooing",		IFC_ANY,	"E",	false,	ifc_istattooing,		"ifcheck istattooing" },
	{ "istoken",			IFC_ANY,	"E",	false,	ifc_istoken,			"ifcheck istoken" },
	{ "istreasureroom",		IFC_ANY,	"EE",	false,	ifc_istreasureroom,		"ifcheck istreasureroom" },
	{ "isvalid",			IFC_ANY,	"E",	false,	ifc_isvalid,			"ifcheck isvalid" },
	{ "isvaliditem",		IFC_ANY,	"E",	false,	ifc_isvaliditem,		"ifcheck isvaliditem" },
	{ "isvisible",			IFC_ANY,	"E",	false,	ifc_isvisible,			"ifcheck isvisible" },
	{ "isvisibleto",		IFC_ANY,	"E",	false,	ifc_isvisibleto,		"ifcheck isvisibleto" },
	{ "iswnum",				IFC_ANY,	"E",	false,	ifc_iswnum,				"ifcheck iswnum" },
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
	{ "missionpoint",		IFC_ANY,	"E",	true,	ifc_mission,			"ifcheck missionpoint" },
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
	{ "number",				IFC_ANY,	"ES",	true,	ifc_number,				"ifcheck number" },
	{ "numenchants",		IFC_ANY,	"ES",	true,	ifc_numenchants,		"ifcheck numenchants" },
	{ "objclones",			IFC_ANY,	"E",	true,	ifc_objclones,			"ifcheck objclones" },
	{ "objcond",			IFC_ANY,	"E",	true,	ifc_objcond,			"ifcheck objcond" },
	{ "objcorpse",			IFC_ANY,	"ES",	false,	ifc_objcorpse,			"ifcheck objcorpse" },
	{ "objcost",			IFC_ANY,	"E",	true,	ifc_objcost,			"ifcheck objcost" },
	{ "objexists",			IFC_ANY,	"S",	false,	ifc_objexists,			"ifcheck objexists" },
	{ "objfrag",			IFC_ANY,	"E",	false,	ifc_objfrag,			"ifcheck objfrag" },
	{ "objhere",			IFC_ANY,	"ES",	false,	ifc_objhere,			"ifcheck objhere" },
	{ "objmaxrepairs",			IFC_ANY,	"E",	true,	ifc_objmaxrepairs,			"ifcheck objmaxrepairs" },
	{ "objmaxweight",		IFC_ANY,	"E",	true,	ifc_objmaxweight,		"ifcheck objmaxweight" },
	{ "objranged",			IFC_ANY,	"ES",	false,	ifc_objranged,			"ifcheck objranged" },
	{ "objrepairs",			IFC_ANY,	"E",	true,	ifc_objrepairs,			"ifcheck objrepairs" },
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
	{ "objval8",			IFC_ANY,	"E",	true,	ifc_objval8,			"ifcheck objval8" },
	{ "objval9",			IFC_ANY,	"E",	true,	ifc_objval9,			"ifcheck objval9" },
	{ "objweapon",			IFC_ANY,	"ES",	false,	ifc_objweapon,			"ifcheck objweapon" },
	{ "objweaponstat",		IFC_ANY,	"ES",	false,	ifc_objweaponstat,		"ifcheck objweaponstat" },
	{ "objwear",			IFC_ANY,	"ES",	false,	ifc_objwear,			"ifcheck objwear" },
	{ "objwearloc",			IFC_ANY,	"ES",	false,	ifc_objwearloc,			"ifcheck objwear" },
	{ "objweight",			IFC_ANY,	"E",	true,	ifc_objweight,			"ifcheck objweight" },
	{ "objweightleft",		IFC_ANY,	"E",	true,	ifc_objweightleft,		"ifcheck objweightleft" },
	{ "off",				IFC_ANY,	"ES",	false,	ifc_off,				"ifcheck off" },
	{ "onmission",			IFC_ANY,	"E",	false,	ifc_onmission,			"ifcheck onmission" },
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
	{ "savage",				IFC_ANY,	"E",	true,	ifc_savage,				"ifcheck savage" },
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
	{ "tempstore5",			IFC_ANY,	"E",	true,	ifc_tempstore5,			"ifcheck tempstore5" },
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
	{ "totalmissions",		IFC_ANY,	"E",	true,	ifc_totalmissions,		"ifcheck totalmissions" },
	{ "totalpkfights",		IFC_ANY,	"E",	true,	ifc_totalpkfights,		"ifcheck totalpkfights" },
	{ "totalpkloss",		IFC_ANY,	"E",	true,	ifc_totalpkloss,		"ifcheck totalpkloss" },
	{ "totalpkratio",		IFC_ANY,	"E",	true,	ifc_totalpkratio,		"ifcheck totalpkratio" },
	{ "totalpkwins",		IFC_ANY,	"E",	true,	ifc_totalpkwins,		"ifcheck totalpkwins" },
	{ "totalratio",			IFC_ANY,	"E",	true,	ifc_totalratio,			"ifcheck totalratio" },
	{ "totalwins",			IFC_ANY,	"E",	true,	ifc_totalwins,			"ifcheck totalwins" },
	{ "toxin",				IFC_ANY,	"ES",	true,	ifc_toxin,				"ifcheck toxin" },
	{ "trains",				IFC_ANY,	"E",	true,	ifc_trains,				"ifcheck trains" },
	{ "uses",				IFC_ANY,	"ES",	false,	ifc_uses,				"ifcheck uses" },
	{ "valueac",			IFC_ANY,	"",		true,	ifc_value_ac,			"ifcheck valueac" },
	{ "valueacstr",			IFC_ANY,	"",		true,	ifc_value_acstr,		"ifcheck valueacstr" },
	{ "valuedamage",		IFC_ANY,	"",		true,	ifc_value_damage,		"ifcheck valuedamage" },
	{ "valueportaltype",	IFC_ANY,	"ES",	true,	ifc_value_portaltype,	"ifcheck valueportaltype" },
	{ "valueposition",		IFC_ANY,	"",		true,	ifc_value_position,		"ifcheck valueposition" },
	{ "valueranged",		IFC_ANY,	"",		true,	ifc_value_ranged,		"ifcheck valueranged" },
	{ "valuesector",		IFC_ANY,	"",		true,	ifc_value_sector,		"ifcheck valuesector" },
	{ "valuesize",			IFC_ANY,	"",		true,	ifc_value_size,			"ifcheck valuesize" },
	{ "valuetoxin",			IFC_ANY,	"",		true,	ifc_value_toxin,		"ifcheck valuetoxin" },
	{ "valuetype",			IFC_ANY,	"",		true,	ifc_value_type,			"ifcheck valuetype" },
	{ "valueweapon",		IFC_ANY,	"",		true,	ifc_value_weapon,		"ifcheck valueweapon" },
	{ "valuewear",			IFC_ANY,	"",		true,	ifc_value_wear,			"ifcheck valuewear" },
	{ "varbool",			IFC_ANY,	"S",	false,	ifc_varbool,			"ifcheck varbool" },
	{ "vardefined",			IFC_ANY,	"SS",	false,	ifc_vardefined,			"ifcheck vardefined" },
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
	{ "wnumvalid",			IFC_ANY,	"E",	false,	ifc_wnumvalid,			"ifcheck wnumvalid" },
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
	{	"imbue", INTERRUPT_IMBUE, true },
	{	"ink", INTERRUPT_INK, true },
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
