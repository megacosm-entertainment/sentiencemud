#ifndef __INTERPRET_H__
#define __INTERPRET_H__

#define MAX_STACK 1024

struct nib_script_stack_lvalue_s
{
	NIB_SCRIPT_STACK_TYPE type;
	union {
		void **pointer;
		void *raw;
		int *number32;
		short *number16;
		long *number;
		bool *b;
		double *d;
		char **str;
		utf8char_t *ch;
		WNUM *wnum;

		ACCOUNT_DATA **account;
		AFFECT_DATA **affect;
		AREA_DATA **area;
		//CHANNEL_DATA **channel;
		CLASS_DATA **clazz;
		DUNGEON **dungeon;
		EXIT_DATA **ex;
		INSTANCE **instance;
		LIQUID **liquid;
		MAIL_DATA **mail;
		MATERIAL **material;
		MISSION_DATA **mission;
		CHAR_DATA **mobile;
		NOTE_DATA **note;
		OBJ_DATA **object;
		CHURCH_DATA **org;	// Change to ORG_DATA when done
		// QUEST_DATA **quest;
		RACE_DATA **race;
		REPUTATION_INDEX_RANK_DATA **rank;
		REPUTATION_DATA **reputation;
		ROOM_INDEX_DATA **room;
		SECTOR_DATA **sector;
		SHIP_DATA **ship;
		SKILL_DATA **skill;
		TOKEN_DATA **token;
		WILDS_DATA **wilds;
		// WORLD_DATA **world;


		struct {
			long *value;
			const struct flag_type *table;
			long bit;
		} bit;

		struct {
			long *number;
			const struct flag_type *table;
		} stat;		// FLAG and STAT

		struct {
			int *number;
			const struct flag_type *table;
		} stat32;

		struct {
			short *number;
			const struct flag_type *table;
		} stat16;

		struct {
			long *bits;
			const struct flag_type **bank;
			int banks;
		} flagbank;		// FLAG_BANK

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST **list;
		} list;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			long length;
			size_t size;
			void **ptr;
		} array;
	} _;
};

struct nib_script_stack_s
{
	NIB_SCRIPT_STACK_TYPE type;

	union {
		void *pointer;			// Used just to check the types that are raw pointers
		long i;
		double d;
		bool b;
		char *str;
		utf8char_t ch;
		WNUM wnum;

		struct {
			long number;
			const struct flag_type *table;
		} stat;

		struct {
			long *bits;
			const struct flag_type **bank;
			int banks;
		} flagbank;		// FLAG_BANK

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
		} list;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			bool shared;
			ITERATOR it;
		} iter;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			long length;
			size_t size;
			void *ptr;
		} array;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			bool shared;
			long length;
			size_t size;
			long index;
			void *ptr;
		} indexer;	// Used for indexing an array

		struct {
			char *str;
			char *cur;
			bool shared;
		} stringer;

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

		NIB_SCRIPT_LVALUE lvalue;
	} _;
};

// Local variable with their actual runtime values stored
struct nib_local_runtime_var_s
{
	char *name;
	NIB_SCRIPT_STACK_TYPE type;
	bool constant;			// Just to guard against writing

	union {
		long i;				// Used by NUMBER, FLAG and STAT
		double f;
		bool b;
		char *str;
		utf8char_t ch;
		WNUM wnum;
		struct {
			NIB_SCRIPT_STACK_TYPE type;
			bool constant;
			LLIST *list;
		} list;

		struct {
			long number;
			const struct flag_type *table;
		} stat;

		struct {
			long *bits;
			const struct flag_type **bank;
			int banks;
		} flagbank;		// FLAG_BANK

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			bool constant;
			long length;
			size_t size;
			void *ptr;
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

typedef struct nib_script_disassembled_line_s NIB_DISASSEMBLED_LINE;
struct nib_script_disassembled_line_s
{
	long address;
	char *str;
	bool is_comment;
};

struct nib_script_runtime_s
{
	NIB_SCRIPT *script;

	int n_locals;
	NIB_LOCAL_RUNTIME_VAR *locals;

	NIB_SCRIPT_STACK stack[MAX_STACK];

	// Indices
	nib_address_t pc;
	nib_address_t sp;

	int last_return;

	// Debugging information
	LLIST *disassembly;

	char debug[1024];
};

// struct nib_script_argument_s
// {
// 	NIB_SCRIPT_STACK_TYPE type;

// 	union {
// 		long i;
// 		double d;
// 		bool b;
// 		char *str;
// 		utf8char_t ch;
// 		WNUM wnum;

// 		struct {
// 			long number;
// 			const struct flag_type *table;
// 		} stat;

// 		struct {
// 			NIB_SCRIPT_STACK_TYPE type;
// 			bool constant;
// 			LLIST *list;
// 		} list;

// 		struct {
// 			NIB_SCRIPT_STACK_TYPE type;
// 			bool constant;
// 			LLIST *list;
// 			ITERATOR it;
// 		} iter;

// 		struct {
// 			NIB_SCRIPT_STACK_TYPE type;
// 			bool constant;
// 			long length;
// 			size_t size;
// 			void *ptr;
// 		} array;

// 		struct {
// 			NIB_SCRIPT_STACK_TYPE type;
// 			bool constant;
// 			long length;
// 			long index;
// 			void *ptr;
// 		} indexer;

// 		ACCOUNT_DATA *account;
// 		AFFECT_DATA *affect;
// 		AREA_DATA *area;
// 		//CHANNEL_DATA *channel;
// 		CLASS_DATA *clazz;
// 		DUNGEON *dungeon;
// 		EXIT_DATA *ex;
// 		INSTANCE *instance;
// 		LIQUID *liquid;
// 		MAIL_DATA *mail;
// 		MATERIAL *material;
// 		MISSION_DATA *mission;
// 		CHAR_DATA *mobile;
// 		NOTE_DATA *note;
// 		OBJ_DATA *object;
// 		CHURCH_DATA *org;	// Change to ORG_DATA when done
// 		// QUEST_DATA *quest;
// 		RACE_DATA *race;
// 		REPUTATION_INDEX_RANK_DATA *rank;
// 		REPUTATION_DATA *reputation;
// 		ROOM_INDEX_DATA *room;
// 		SHIP_DATA *ship;
// 		SKILL_DATA *skill;
// 		TOKEN_DATA *token;
// 		WILDS_DATA *wilds;
// 		// WORLD_DATA *world;
// 	} _;
// };

#define __push(t,n) bool nib_push_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t value);
__push(long,number)
__push(double,float)
__push(bool,boolean)
__push(utf8char_t,char)
__push(const char *,string)
__push(char *,string_shared)
bool nib_push_stack_list_raw (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_array_raw (NIB_SCRIPT_RUNTIME *nsr, void *value, NIB_SCRIPT_STACK_TYPE type, size_t size, long length);
bool nib_push_stack_array (NIB_SCRIPT_RUNTIME *nsr, void *value, NIB_SCRIPT_STACK_TYPE type, size_t size, long length);
bool nib_push_stack_array_shared (NIB_SCRIPT_RUNTIME *nsr, void *value, NIB_SCRIPT_STACK_TYPE type, size_t size, long length);
bool nib_push_stack_widevnum (NIB_SCRIPT_RUNTIME *nsr, WNUM *value);
bool nib_push_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table);
bool nib_push_stack_flagbank (NIB_SCRIPT_RUNTIME *nsr, long* bits, const struct flag_type **bank, int banks);
bool nib_push_stack_flagbank_shared (NIB_SCRIPT_RUNTIME *nsr, long* bits, const struct flag_type **bank, int banks);
bool nib_push_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table);
__push(ACCOUNT_DATA *,account)
__push(AFFECT_DATA *,affect)
__push(AREA_DATA *,area)
//__push(CHANNEL_DATA *,channel)
__push(CLASS_DATA *,class)
__push(DUNGEON *,dungeon)
__push(EXIT_DATA *,exit)
__push(INSTANCE *,instance)
__push(LIQUID *,liquid)
__push(MAIL_DATA *,mail)
__push(MATERIAL *,material)
__push(MISSION_DATA *,mission)
__push(CHAR_DATA *,mobile)
__push(NOTE_DATA *,note)
__push(OBJ_DATA *,object)
__push(CHURCH_DATA *,org)
// __push(QUEST_DATA *,quest)
__push(RACE_DATA *,race)
__push(REPUTATION_INDEX_RANK_DATA *,rank)
__push(REPUTATION_DATA *,reputation)
__push(ROOM_INDEX_DATA *,room)
__push(SECTOR_DATA *,sector)
__push(SHIP_DATA *,ship)
__push(SKILL_DATA *,skill)
__push(TOKEN_DATA *,token)
__push(WILDS_DATA *,wilds)
//__push(WORLD_DATA *,world)
__push(NIB_SCRIPT_LVALUE *,lvalue)
__push(NIB_LOCAL_RUNTIME_VAR *,local_var)
#undef __push

NIB_SCRIPT_STACK_TYPE nib_peek_stack(NIB_SCRIPT_RUNTIME *nsr);
NIB_SCRIPT_STACK_TYPE nib_peek_stack_offset(NIB_SCRIPT_RUNTIME *nsr, int offset);
#define __peek(t,n) bool nib_peek_stack_##n (NIB_SCRIPT_RUNTIME *nsr, int offset, t *value);
__peek(long,number)
__peek(double,float)
__peek(bool,boolean)
__peek(utf8char_t,char)
__peek(char *,string)
__peek(char *,string_shared)
bool nib_peek_stack_list (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_peek_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_peek_stack_array (NIB_SCRIPT_RUNTIME *nsr, int offset, void **value, NIB_SCRIPT_STACK_TYPE *type, size_t *size, long *length);
bool nib_peek_stack_array_shared (NIB_SCRIPT_RUNTIME *nsr, int offset, void **value, NIB_SCRIPT_STACK_TYPE *type, size_t *size, long *length);
__peek(WNUM,widevnum)
bool nib_peek_stack_flag (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table);
bool nib_peek_stack_flagbank (NIB_SCRIPT_RUNTIME *nsr, int offset, long **output, const struct flag_type ***bank, int *banks);
bool nib_peek_stack_flagbank_shared (NIB_SCRIPT_RUNTIME *nsr, int offset, long **output, const struct flag_type ***bank, int *banks);
bool nib_peek_stack_stat (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table);
__peek(ACCOUNT_DATA *,account)
__peek(AFFECT_DATA *,affect)
__peek(AREA_DATA *,area)
//__peek(CHANNEL_DATA *,channel)
__peek(CLASS_DATA *,class)
__peek(DUNGEON *,dungeon)
__peek(EXIT_DATA *,exit)
__peek(INSTANCE *,instance)
__peek(LIQUID *,liquid)
__peek(MAIL_DATA *,mail)
__peek(MATERIAL *,material)
__peek(MISSION_DATA *,mission)
__peek(CHAR_DATA *,mobile)
__peek(NOTE_DATA *,note)
__peek(OBJ_DATA *,object)
__peek(CHURCH_DATA *,org)
// __peek(QUEST_DATA *,quest)
__peek(RACE_DATA *,race)
__peek(REPUTATION_INDEX_RANK_DATA *,rank)
__peek(REPUTATION_DATA *,reputation)
__peek(ROOM_INDEX_DATA *,room)
__peek(SECTOR_DATA *,sector)
__peek(SHIP_DATA *,ship)
__peek(SKILL_DATA *,skill)
__peek(TOKEN_DATA *,token)
__peek(WILDS_DATA *,wilds)
//__peek(WORLD_DATA *,world)
__peek(NIB_SCRIPT_LVALUE,lvalue)
#undef __peek

bool nib_pop_stack (NIB_SCRIPT_RUNTIME *nsr);
#define __pop(t,n) bool nib_pop_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t *value);
__pop(long,number)
__pop(double,float)
__pop(bool,boolean)
__pop(utf8char_t,char)
__pop(char *,string)
__pop(char *,string_shared)
bool nib_pop_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_pop_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_pop_stack_array (NIB_SCRIPT_RUNTIME *nsr, void **value, NIB_SCRIPT_STACK_TYPE *type, size_t *size, long *length);
bool nib_pop_stack_array_shared (NIB_SCRIPT_RUNTIME *nsr, void **value, NIB_SCRIPT_STACK_TYPE *type, size_t *size, long *length);
__pop(WNUM,widevnum)
bool nib_pop_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long *value, const struct flag_type **table);
bool nib_pop_stack_flagbank (NIB_SCRIPT_RUNTIME *nsr, long **output, const struct flag_type ***bank, int *banks);
bool nib_pop_stack_flagbank_shared (NIB_SCRIPT_RUNTIME *nsr, long **output, const struct flag_type ***bank, int *banks);
bool nib_pop_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long *value, const struct flag_type **table);
__pop(ACCOUNT_DATA *,account)
__pop(AFFECT_DATA *,affect)
__pop(AREA_DATA *,area)
//__pop(CHANNEL_DATA *,channel)
__pop(CLASS_DATA *,class)
__pop(DUNGEON *,dungeon)
__pop(EXIT_DATA *,exit)
__pop(INSTANCE *,instance)
__pop(LIQUID *,liquid)
__pop(MAIL_DATA *,mail)
__pop(MATERIAL *,material)
__pop(MISSION_DATA *,mission)
__pop(CHAR_DATA *,mobile)
__pop(NOTE_DATA *,note)
__pop(OBJ_DATA *,object)
__pop(CHURCH_DATA *,org)
// __pop(QUEST_DATA *,quest)
__pop(RACE_DATA *,race)
__pop(REPUTATION_INDEX_RANK_DATA *,rank)
__pop(REPUTATION_DATA *,reputation)
__pop(ROOM_INDEX_DATA *,room)
__pop(SECTOR_DATA *,sector)
__pop(SHIP_DATA *,ship)
__pop(SKILL_DATA *,skill)
__pop(TOKEN_DATA *,token)
__pop(WILDS_DATA *,wilds)
//__pop(WORLD_DATA *,world)
__pop(NIB_SCRIPT_LVALUE,lvalue)
#undef __pop

#endif
