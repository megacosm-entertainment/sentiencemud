#ifndef __INTERPRET_H__
#define __INTERPRET_H__

#define MAX_STACK 1024

struct nib_script_stack_lvalue_s
{
	NIB_SCRIPT_STACK_TYPE type;
	union {
		long *number;
		bool *b;
		double *d;
		char **str;
		utf8char_t *ch;
		WNUM *wnum;

		AREA_DATA **area;
		CHAR_DATA **mobile;
		ROOM_INDEX_DATA **room;

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
			NIB_SCRIPT_STACK_TYPE type;
			LLIST **list;
		} list;
	} _;
};

struct nib_script_stack_s
{
	NIB_SCRIPT_STACK_TYPE type;

	union {
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
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			ITERATOR it;
		} list;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			ITERATOR it;
		} iter;

		AREA_DATA *area;
		// DUNGEON *dung;
		// INSTANCE *inst;
		CHAR_DATA *mobile;
		// OBJ_DATA *object;
		// QUEST_DATA *quest;
		ROOM_INDEX_DATA *room;
		// SHIP_DATA *ship;
		// TOKEN_DATA *token;

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
			LLIST *list;
		} list;

		struct {
			long number;
			const struct flag_type *table;
		} stat;

		AREA_DATA *area;
		// DUNGEON *dung;
		// INSTANCE *inst;
		CHAR_DATA *mobile;
		// OBJ_DATA *obj;
		// QUEST_DATA *quest;
		ROOM_INDEX_DATA *room;
		// SHIP_DATA *ship;
		// TOKEN_DATA *token;
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

struct nib_script_argument_s
{
	NIB_SCRIPT_STACK_TYPE type;

	union {
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
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			ITERATOR it;
		} list;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			ITERATOR it;
		} iter;

		AREA_DATA *area;
		// DUNGEON dung;
		// INSTANCE inst;
		CHAR_DATA *mobile;
		// OBJ_DATA object;
		// QUEST_DATA quest;
		ROOM_INDEX_DATA *room;
		// SHIP_DATA ship;
		// TOKEN_DATA token;
	} _;
};

#define __push(t,n) bool nib_push_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t value);
__push(long,number)
__push(double,float)
__push(bool,boolean)
__push(utf8char_t,char)
__push(const char *,string)
__push(char *,string_shared)
bool nib_push_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_widevnum (NIB_SCRIPT_RUNTIME *nsr, WNUM *value);
bool nib_push_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table);
bool nib_push_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long value, const struct flag_type *table);
__push(AREA_DATA *,area)
__push(CHAR_DATA *,mobile)
__push(ROOM_INDEX_DATA *,room)
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
__peek(WNUM,widevnum)
bool nib_peek_stack_flag (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table);
bool nib_peek_stack_stat (NIB_SCRIPT_RUNTIME *nsr, int offset, long *output, const struct flag_type **table);
__peek(AREA_DATA *,area)
__peek(CHAR_DATA *,mobile)
__peek(ROOM_INDEX_DATA *,room)
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
__pop(WNUM,widevnum)
bool nib_pop_stack_flag (NIB_SCRIPT_RUNTIME *nsr, long *value, const struct flag_type **table);
bool nib_pop_stack_stat (NIB_SCRIPT_RUNTIME *nsr, long *value, const struct flag_type **table);
__pop(AREA_DATA *,area)
__pop(CHAR_DATA *,mobile)
__pop(ROOM_INDEX_DATA *,room)
__pop(NIB_SCRIPT_LVALUE,lvalue)
#undef __pop

#endif
