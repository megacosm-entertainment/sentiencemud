#ifndef __DUMMY_H__
#define __DUMMY_H__

// Everything about this needs to be removed and resolved once integrated into the main code

#define SCRIPT_WIZNET		(A)	/* The script will wiznet to WIZ_SCRIPTS */
#define SCRIPT_DISABLED		(B)	/* The script must be turned off by an IMP */
#define SCRIPT_LUA			(C)	/* This script is a LUA compiled script. */
#define SCRIPT_SECURED		(D)	/* This script will reset security settings to its values */
#define SCRIPT_SYSTEM		(E)	// A system script, may ONLY be called when security is SYSTEM security
#define SCRIPT_INSPECT		(Z)	/* Inspect the script for restricted actions */

#define SCRIPTEXEC_HALT		(A)	/* Kill script execution because the controller entity had been destructed */

/* This should be moved to merc.h and made general */
#define INTERRUPT_CAST		(A)
#define INTERRUPT_MUSIC		(B)
#define INTERRUPT_BREW		(C)
#define INTERRUPT_REPAIR	(D)
#define INTERRUPT_HIDE		(E)
#define INTERRUPT_BIND		(F)
#define INTERRUPT_BOMB		(G)
#define INTERRUPT_RECITE	(H)
#define INTERRUPT_REVERIE	(I)
#define INTERRUPT_TRANCE	(J)
#define INTERRUPT_SCRIBE	(K)
#define INTERRUPT_RANGED	(L)
#define INTERRUPT_RESURRECT	(M)
#define INTERRUPT_FADE		(N)
#define INTERRUPT_INK		(O)
#define INTERRUPT_IMBUE		(P)
#define INTERRUPT_SCRIPT	(dd)	/* Used to interrupt whatever script action is going, that is up to the individual scripts to determine that! */
#define INTERRUPT_SILENT	(ee)	/* Used to make the interrupt SILENT */

#define TRANSFER_MODE_SILENT	0
#define TRANSFER_MODE_PORTAL	1
#define TRANSFER_MODE_MOVEMENT	2

#define TOKEN_OWNER_NONE	0
#define TOKEN_OWNER_MOB		1
#define TOKEN_OWNER_OBJ		2
#define TOKEN_OWNER_ROOM	3

enum variable_enum {
	VAR_UNKNOWN = 0,
	VAR_BOOLEAN,
	VAR_NUMBER,
	VAR_FLOAT,
	VAR_CHAR,
	VAR_STRING,
	VAR_STRING_S,		// Shared, allocated elsewhere
	VAR_WIDEVNUM,
	VAR_TIME,
	VAR_DICE,
	VAR_FLAG,
	VAR_FLAG_BANK,
	VAR_STAT,
	VAR_LIST,
	VAR_LIST_S,			// Shared, allocated elsewhere
	VAR_ARRAY,
	VAR_ARRAY_S,
	VAR_ACCOUNT,
	VAR_AFFECT,
	VAR_AREA,
	VAR_CHANNEL,
	VAR_CLASS,
	VAR_DUNGEON,
	VAR_EXIT,
	VAR_INSTANCE,
	VAR_LIQUID,
	VAR_MAIL,
	VAR_MATERIAL,
	VAR_MISSION,
	VAR_MOBILE,
	VAR_NOTE,
	VAR_OBJECT,
	VAR_ORG,
	VAR_QUEST,
	VAR_RACE,
	VAR_RANK,
	VAR_REPUTATION,
	VAR_ROOM,
	VAR_SECTOR,
	VAR_SKILL,
	VAR_SHIP,
	VAR_TOKEN,
	VAR_WILDS,
	VAR_WORLD,
	VAR_MAX
};


// struct list_link_type
// {
//     LLIST_LINK *next;
//     LLIST_LINK *prev;
//     void *data;
// };

// struct list_type
// {
//     LLIST *next;
//     LLIST_LINK *head;
//     LLIST_LINK *tail;
//     unsigned long ref;
//     unsigned long size;
//     LISTCOPY_FUNC *copier;
//     LISTDESTROY_FUNC *deleter;
//     bool valid;
//     bool purge;
// };

// struct iterator_type
// {
//     LLIST *list;
//     LLIST_LINK *current;
//     bool moved;
// };


// typedef struct area_data AREA_DATA;
// typedef struct mob_index_data MOB_INDEX_DATA;
// typedef struct char_data CHAR_DATA;
// typedef struct room_index_data ROOM_INDEX_DATA;

// struct area_data
// {
// 	long uid;
// 	char *name;
// 	char *description;
// 	flag_value_t flags;
// 	LLIST *rooms;
// };

// typedef struct wide_vnum_type
// {
//     AREA_DATA *pArea;
//     long vnum;
// } WNUM;

// struct mob_index_data
// {
// 	AREA_DATA *area;
// 	long vnum;
// };

// struct char_data
// {
// 	MOB_INDEX_DATA *pIndexData;
// 	char *name;
// 	char *short_descr;
// 	char *long_descr;
// 	char *description;
// };

// struct room_index_data
// {
// 	AREA_DATA *area;
// 	long vnum;
// 	char *name;
// 	char *description;
// 	LLIST *people;
// };

struct script_var_type {
	pVARIABLE next;
	char *name;
	bool save;
	bool index;
	bool readonly;		// Readonly within the script, it can only be set in the script editor.
	int type;
	union {
		void *raw;
		long address;
		long num;
		double flt;
		bool b;
		utf8char_t ch;
		char *str;
		WNUM wnum;
		DICE_DATA dice;
		time_t timestamp;
		struct {
			long number;
			struct flag_type *table;
			const char *table_name;
		} stat;
		struct {
			long *bits;
			const struct flag_type **bank;
			int banks;
		} flagbank;
		struct {
			LLIST *list;
			bool constant;
			int type;
		} list;
		struct {
			void *ptr;
			int type;
			long length;
			size_t size;
			bool constant;
		} array;
		ACCOUNT_DATA *account;
		AFFECT_DATA *affect;
		AREA_DATA *area;
		//CHANNEL_DATA *channel;
		CLASS_DATA *clazz;
		DUNGEON *dungeon;
		EXIT_DATA *ex;
		INSTANCE *instance;
		LIQUID *liquid;
		MAIL_DATA *mail;
		MATERIAL *material;
		MISSION_DATA *mission;
		CHAR_DATA *mobile;
		NOTE_DATA *note;
		OBJ_DATA *object;
		CHURCH_DATA *org;	// Change to ORG_DATA when done
		// QUEST_DATA *quest;
		RACE_DATA *race;
		REPUTATION_INDEX_RANK_DATA *rank;
		REPUTATION_DATA *reputation;
		ROOM_INDEX_DATA *room;
		SECTOR_DATA *sector;
		SHIP_DATA *ship;
		SKILL_DATA *skill;
		TOKEN_DATA *token;
		WILDS_DATA *wilds;
		// WORLD_DATA *world;
	} _;
};

bool variable_init();
void variable_cleanup();
pVARIABLE variable_get(const char *name);
pVARIABLE variable_new_number(const char *name, long value);
pVARIABLE variable_new_float(const char *name, double value);
pVARIABLE variable_new_bool(const char *name, bool value);
pVARIABLE variable_new_char(const char *name, utf8char_t ch);
pVARIABLE variable_new_string_raw(const char *name, char *str);
pVARIABLE variable_new_string(const char *name, char *str);
pVARIABLE variable_new_shared_string(const char *name, char *str);
pVARIABLE variable_new_widevnum(const char *name, AREA_DATA *area, long vnum);
pVARIABLE variable_new_dice(const char *name, DICE_DATA *dice);
pVARIABLE variable_new_flag(const char *name, long number, struct flag_type *table, const char *table_name);
pVARIABLE variable_new_flagbank(const char *name, long *bits, const struct flag_type **bank);
pVARIABLE variable_new_stat(const char *name, long number, struct flag_type *table, const char *table_name);
pVARIABLE variable_new_list_raw(const char *name, LLIST *list, int type, bool constant);
pVARIABLE variable_new_list(const char *name, LLIST *list, int type, bool constant);
pVARIABLE variable_new_shared_list(const char *name, LLIST *list, int type, bool constant);
pVARIABLE variable_new_area(const char *name, AREA_DATA *area);
// pVARIABLE variable_new_dungeon(const char *name, DUNGEON *dungeon);
// pVARIABLE variable_new_instance(const char *name, INSTANCE *instance);
pVARIABLE variable_new_mobile(const char *name, CHAR_DATA *mobile);
// pVARIABLE variable_new_object(const char *name, OBJ_DATA *object);
// pVARIABLE variable_new_quest(const char *name, QUEST_DATA *quest);
pVARIABLE variable_new_room(const char *name, ROOM_INDEX_DATA *room);
// pVARIABLE variable_new_ship(const char *name, SHIP_DATA *ship);
// pVARIABLE variable_new_token(const char *name, TOKEN_DATA *token);
void variable_get_string(pVARIABLE var, char *buf, int buf_len, int char_len);
const char *variable_get_typename(pVARIABLE var);

#endif
