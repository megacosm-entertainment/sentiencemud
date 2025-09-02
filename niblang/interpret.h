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
		char *ch;
		WNUM *wnum;

		AREA_DATA **area;
		CHAR_DATA **mobile;
		ROOM_INDEX_DATA **room;

		struct {
			long *value;
			long bit;
		} flag;

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
		char ch;
		WNUM wnum;

		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
			ITERATOR it;
		} list;

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
		char ch;
		WNUM wnum;
		struct {
			NIB_SCRIPT_STACK_TYPE type;
			LLIST *list;
		} list;

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

struct nib_script_runtime_s
{
	NIB_SCRIPT *script;

	int n_locals;
	NIB_LOCAL_RUNTIME_VAR *locals;

	NIB_SCRIPT_STACK stack[MAX_STACK];

	// Indices
	int pc;
	int sp;

	int last_return;
};

#define __push(t,n) bool nib_push_stack_##n (NIB_SCRIPT_RUNTIME *nsr, t value);
__push(long,number)
__push(double,float)
__push(bool,boolean)
__push(char,char)
__push(char *,string)
__push(char *,string_shared)
bool nib_push_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
bool nib_push_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST *value, NIB_SCRIPT_STACK_TYPE type);
__push(WNUM,widevnum)
__push(long,flag)
__push(long,stat)
__push(AREA_DATA *,area)
__push(CHAR_DATA *,mobile)
__push(ROOM_INDEX_DATA *,room)
__push(NIB_SCRIPT_LVALUE *,lvalue)
__push(NIB_LOCAL_RUNTIME_VAR *,local_var)
#undef __push

NIB_SCRIPT_STACK_TYPE nib_peek_stack(NIB_SCRIPT_RUNTIME *nsr);
#define __peek(t,n) bool nib_peek_stack_##n (NIB_SCRIPT_RUNTIME *nsr, int offset, t *value);
__peek(long,number)
__peek(double,float)
__peek(bool,boolean)
__peek(char,char)
__peek(char *,string)
__peek(char *,string_shared)
bool nib_peek_stack_list (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_peek_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, int offset, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
__peek(WNUM,widevnum)
__peek(long,flag)
__peek(long,stat)
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
__pop(char,char)
__pop(char *,string)
__pop(char *,string_shared)
bool nib_pop_stack_list (NIB_SCRIPT_RUNTIME *nsr, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
bool nib_pop_stack_list_shared (NIB_SCRIPT_RUNTIME *nsr, LLIST **value, NIB_SCRIPT_STACK_TYPE *type);
__pop(WNUM,widevnum)
__pop(long,flag)
__pop(long,stat)
__pop(AREA_DATA *,area)
__pop(CHAR_DATA *,mobile)
__pop(ROOM_INDEX_DATA *,room)
__pop(NIB_SCRIPT_LVALUE,lvalue)
#undef __pop

#endif
