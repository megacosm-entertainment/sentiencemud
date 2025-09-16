#ifndef __NIBLANG_H__
#define __NIBLANG_H__

#define NIB_GLOBAL_SCOPE	(-1)

#define A 1L						// 0
#define B 2L						// 1
#define C 4L						// 2
#define D 8L						// 3
#define E 16L						// 4
#define F 32L						// 5
#define G 64L						// 6
#define H 128L						// 7
#define I 256L						// 8
#define J 512L						// 9
#define K 1024L						// 10
#define L 2048L						// 11
#define M 4096L						// 12
#define N 8192L						// 13
#define O 16384L					// 14
#define P 32768L					// 15
#define Q 65536L					// 16
#define R 131072L					// 17
#define S 262144L					// 18
#define T 524288L					// 19
#define U 1048576L					// 20
#define V 2097152L					// 21
#define W 4194304L					// 22
#define X 8388608L					// 23
#define Y 16777216L					// 24
#define Z 33554432L					// 25
#define aa 67108864L				// 26
#define bb 134217728L				// 27
#define cc 268435456L				// 28
#define dd 536870912L				// 29
#define ee 1073741824L				// 30
#define ff 2147483648L				// 31	(sign bit for 32-bit int)
#define gg 4294967296L				// 32
#define hh 8589934592L				// 33
#define ii 17179869184L				// 34
#define jj 34359738368L				// 35
#define kk 68719476736L				// 36
#define ll 137438953472L			// 37
#define mm 274877906944L			// 38
#define nn 549755813888L			// 39
#define oo 1099511627776L			// 40
#define pp 2199023255552L			// 41
#define qq 4398046511104L			// 42
#define rr 8796093022208L			// 43
#define ss 17592186044416L			// 44
#define tt 35184372088832L			// 45
#define uu 70368744177664L			// 46
#define vv 140737488355328L			// 47
#define ww 281474976710656L			// 48
#define xx 562949953421312L			// 49
#define yy 1125899906842624L		// 50
#define zz 2251799813685248L		// 51
#define aaa 4503599627370496L		// 52
#define bbb 9007199254740992L		// 53
#define ccc 18014398509481984L		// 54
#define ddd 36028797018963968L		// 55
#define eee 72057594037927936L		// 56
#define fff 144115188075855872L		// 57
#define ggg 288230376151711744L		// 58
#define hhh 576460752303423488L		// 59
#define iii 1152921504606846976L	// 60
#define jjj 2305843009213693952L	// 61
#define kkk 4611686018427387904L	// 62
#define lll 9223372036854775808L	// 63 (sign bit for 64-bit int)

#define IS_SET(v,b)		(((v) & (b)) && true)
#define bitsize(t)		(sizeof(t) * 8)

#define SCPERR_SUCCESS		 	(0)			// Success
#define SCPERR_FAILURE			(-1)		// General failure
#define SCPERR_MATH				(-2)		// Math error (eg. division by zero)
#define SCPERR_INVALID			(-3)		// Invalid operation
#define SCPERR_MEMORY			(-4)		// Memory allocation error
#define SCPERR_STACK			(-5)		// Stack under/overflow
#define SCPERR_FIELD			(-6)		// Invalid field
#define SCPERR_METHOD			(-7)		// Invalid method
#define SCPERR_FUNCTION			(-8)		// Invalid function

#include "typedefs.h"

// Instructions
enum nib_instructions_e {
	NI_ILLEGAL = 0,
	NI_LVALUE_LOCAL,		// Load the address of the local variable onto the stack
	NI_LVALUE_GLOBAL,		// Load the global variable reference onto the stack (server script pVARIABLE)
	NI_LVALUE_SELF,			// Loads the reference for the SELF onto the stack
	NI_LVALUE_BIT,			// Load and push a flag bit reference onto the stack
	NI_LVALUE_FIELD,
	NI_CALL_FUNCTION,		// Makes a function call
	NI_CALL_METHOD,
	NI_LOAD_NUMBER,			// Load a number onto stack
	NI_LOAD_FLOAT,			// Load a float onto stack
	NI_LOAD_CHAR,
	NI_LOAD_STRING,			// Load a string literal onto stack
	NI_LOAD_WIDEVNUM,		// Pops 2 (area, vnum) pushes combined WNUM onto stack
	NI_LOAD_FLAG,
	NI_LOAD_FLAG_TABLE,		// Contains the TABLE pointer
	NI_LOAD_STAT,
	NI_NEW_LIST,			// Pushes an empty list (of the given list type) onto the stack
	NI_NULL,				// Pushes a NST_NULL onto the stack
	NI_TRUE,
	NI_FALSE,
	NI_CONST0,				// Load a constant 0 (or false) onto stack
	NI_CONST1,				// Load a constant 1 (or true) onto stack
	NI_NCONST1,				// Load a constant -1 onto stack
	NI_FCONST0,				// Load a constant 0.0 onto stack
	NI_DUP,					// Duplicate the top of the stack
	NI_POP,					// Pop the top of the stack
	NI_POPN,				// Pops the top N slots off the stack
	NI_RETURN,				// Terminates program: pop 1 for return value
	NI_RETURN_BYTE,			// Terminates program: encoded with return byte code
	NI_JUMP,				// Jump to the given program address.
	NI_JUMP_ZERO,			//  .. when the top of stack is zero
	NI_JUMP_NOT_ZERO,		//  .. when the top of stack is not zero
	NI_SWITCH,				// Switch statement: pop 1 for switching value
	NI_INC,					// Increment: pop 1, update lvalue (does not put anything on the stack)
	NI_DEC,					// Decrement: pop 1, update lvalue (does not put anything on the stack)
	NI_POST_INC,			// Increment: pop 1, update lvalue, push prior value
	NI_POST_DEC,			// Decrement: pop 1, update lvalue, push prior value
	NI_PRE_INC,				// Increment: pop 1, update lvalue, push new value
	NI_PRE_DEC,				// Decrement: pop 1, update lvalue, push new value

	NI_ITER_START,
	NI_ITER_STOP,
	NI_ITER_NEXT,

	NI_LAND,
	NI_LOR,
	NI_LXOR,
	NI_LNOT,

	NI_ASSIGN,				// Assignment: pop 2, store rhs to lhs, push rhs
	NI_VOID_ASSIGN,			// Assignment: pop 2, store rhs to lhs

	NI_NEG,					// Unary Negate
	NI_ADD,					// Binary addition: pop 2, add, push 1
	NI_SUBT,				// Binary subtraction: pop 2, subtract, push 1
	NI_MULT,				// Binary multiplication: pop 2, multiply, push 1
	NI_MOD,					// Binary modulo: pop 2, checks divisor isn't zero, modulo, push remainder
	NI_DIV,					// Binary division: pop 2, checks divisor isn't zero, divide, push quotient

	NI_EQ,
	NI_NEQ,
	NI_LT,
	NI_LE,
	NI_GT,
	NI_GE,

	NI_BAND,
	NI_BOR,
	NI_BXOR,
	NI_BNOT,

	NI_LSH,
	NI_RSH,
	NI_RSHL,

	NI_STR_PREFIX,
	NI_STR_INFIX,
	NI_STR_SUFFIX,

	NI_ADD_EQ,
	NI_VOID_ADD_EQ,
	NI_SUBT_EQ,
	NI_MULT_EQ,
	NI_MOD_EQ,
	NI_DIV_EQ,

	NI_BAND_EQ,
	NI_BOR_EQ,
	NI_BXOR_EQ,

	NI_LSH_EQ,
	NI_RSH_EQ,
	NI_RSHL_EQ,

	NI_GET_AREA,		// Pops 1 (number/string), gets the area, pushes onto stack

	NI__MAX
};

enum nib_script_stack_type_e
{
	NST_VOID = -2,			// No return
	NST_FUNCTION = -1,		// Only used when getting the context for function calls
	NST_UNKNOWN = 0,
	NST_BOOLEAN,
	NST_NUMBER,
	NST_FLOAT,
	NST_STRING,
	NST_CHAR,
	NST_MAP,
	NST_WIDEVNUM,
	NST_FLAG,
	NST_FLAG_BIT,		// Can only be an LVALUE; treated as BOOLEAN
	NST_STAT,
	NST_LIST,
	NST_AREA,
	NST_DUNGEON,
	NST_INSTANCE,
	NST_MOBILE,
	NST_OBJECT,
	NST_QUEST,
	NST_ROOM,
	NST_SHIP,
	NST_TOKEN,
	NST__MAX,

	// Types invalid for LVALUEs
	NST_STRING_S = NST__MAX,	// String is not to be freed when popped
	NST_LIST_S,					// Lists not created by the script
	NST_ITERATOR,
	NST_NULL,					// Explicitly a null pointer
	NST_LVALUE,
};


#define MAX_FLAG_BITS		(bitsize(flag_value_t))

#define DECL_METHOD_FUNC(f)	int nib_method_func_##f (NIB_SCRIPT_RUNTIME *nsr, int argc, NIB_SCRIPT_ARG *argv, NIB_SCRIPT_ARG *output)

#include "dummy.h"

struct flag_type
{
    char *name;
    long bit;
    bool settable;
	char *description;
};

struct flag_type_lookup
{
	struct flag_type_lookup *next;
	char *name;
	struct flag_type *table;
	bool internal;
};


#include "script.h"




typedef struct nib_memory_buffer {
    short state;		// error state of the buffer
    int size;			// size in k
	int len;			// length of data in buffer
    nib_bytecode_p buffer;		// actual buffer
} NIB_BUFFER;


typedef struct nib_variable_type NIB_VARIABLE;

struct nib_variable_type {
	char *name; 	// Duplicated name, needs to be freed
	NIB_TYPE *type;
	NIB_SCRIPT_STACK_TYPE stype;
	NIB_SCRIPT_STACK_TYPE stype2;	// For NST_LIST subtype
	int scope;
	int id;
	bool constant;
	bool initialized;

	// Add value union?
};

typedef struct nib_scope_tree_node NIB_SCOPE_NODE;

struct nib_scope_tree_node {
	int scope;

	NIB_SCOPE_NODE *parent;		// Ancestors
	NIB_SCOPE_NODE *head;		// Descendants
	NIB_SCOPE_NODE *tail;		// Descendants
	NIB_SCOPE_NODE *next;		// Sibling nodes
};

typedef struct nib_field_type NIB_FIELD;
struct nib_field_type
{
	char *name;
	bool readonly;
	NIB_TYPE *type;
	NIB_SCRIPT_STACK_TYPE stype;
	NIB_SCRIPT_STACK_TYPE stype2;	// For LIST
	size_t offset;

	METHOD_FUNC *method;		// If it is a field-method, instead of an offset

	int id;						// ID is unique to the type
};

typedef struct nib_method_type NIB_METHOD;

struct nib_method_type
{
	char *name;
	NIB_TYPE *result;		// Use NULL to indicate no return
	NIB_SCRIPT_STACK_TYPE sresult;
	NIB_SCRIPT_STACK_TYPE sresult2;	// For LIST

	int nparams;
	NIB_TYPE **params;

	char *method_name;		// Copy of internal method name
	METHOD_FUNC *method;

	int id;
};

struct nib_method_func_type
{
	char *name;
	METHOD_FUNC *func;
};

typedef struct statement_s
{
    bool may_return         : 1;  /* The statement may issue a return.   */
    bool may_break          : 1;  /* The statement may issue a break.    */
    bool may_continue       : 1;  /* The statement may issue a continue. */
    bool may_finish         : 1;  /* The statement may finish without
                                   * a break or return.
                                   */
    bool is_empty           : 1;  /* There is no real statement.         */
    bool warned_dead_code   : 1;  /* We already warned about dead code.  */
} NIB_STATEMENT;



typedef struct nib_script_comment_s NIB_SCRIPT_COMMENT;

struct nib_script_comment_s {
	long address;
	char *comment;
};

struct lvalue_s {
	char *name;				// Name of variable to be used
	NIB_TYPE *type;			// Resolved type of expression
	flag_value_t flags;

	nib_bytecode_p lhs;		// Used for assigning a value
	size_t lhs_len;

	nib_bytecode_p rhs;		// Used for reading a value
	size_t rhs_len;
};

struct rvalue_s {
	char *name;				// Name of variable to be used
	NIB_TYPE *type;			// Resolved type of expression
	bool needs_use;
	bool needs_pop;
	flag_value_t flags;

	nib_bytecode_p rhs;		// Used for reading a value
	size_t rhs_len;
};

struct lrvalue_s {
	char *name;				// Name of variable to be used
	NIB_TYPE *type;			// Resolved type of expression
	bool needs_use;
	bool needs_pop;
	flag_value_t flags;

	nib_bytecode_p lhs;		// Used for assigning a value
	size_t lhs_len;

	nib_bytecode_p rhs;		// Used for reading a value
	size_t rhs_len;
};


struct nib_bc_statment_s {
	struct nib_bc_statment_s *next;
	int address;
};

struct nib_break_s {
	struct nib_break_s *prev;
	struct nib_bc_statment_s *stmts;
};

struct nib_continue_s {
	struct nib_continue_s *prev;
	struct nib_bc_statment_s *stmts;
};


// compile.c
extern struct nib_break_s *nib_break_address;
extern struct nib_continue_s *nib_continue_address;

void push_nib_break_address();
void push_nib_break_statement(int address);
void update_nib_break_statements(int address);
void push_nib_continue_address();
void push_nib_continue_statement(int address);
void update_nib_continue_statements(int address);
void pop_nib_break_address();
void pop_nib_continue_address();

LLIST *nib_create_comment_list();
LLIST *nib_create_string_list();
LLIST *nib_create_variable_list();
LLIST *nib_create_type_list();
NIB_SCRIPT *nib_compile_script(const char *src, NIB_SCRIPT_CLASS sc);
void nib_cleanup_compile();
void nib_dump_global_variables();
void nib_dump_local_variables();
NIB_VARIABLE *nib_get_global_variable_byid(short id);
NIB_VARIABLE *nib_get_global_variable(const char *name);
NIB_VARIABLE *nib_get_local_variable_byid(short id);
NIB_VARIABLE *nib_get_local_variable(const char *name);
void nib_add_global_variable(NIB_VARIABLE *var);
void nib_add_local_variable(NIB_VARIABLE *var);
const char *nib_get_string(int index);
int nib_get_string_in_storage(const char *str);
int nib_add_string_to_storage(const char *str);
void nib_dump_string_storage();
void nib_dump_program();
void nib_script_comment_add(long address, char *comment);



// flags.c
bool nib_register_flag_table(const char *name, const struct flag_type *table);
bool nib_register_stat_table(const char *name, const struct flag_type *table);
const struct flag_type *nib_lookup_flag_table(LLIST *created, const char *name);
const char *nib_get_flag_table_name(LLIST *created, const struct flag_type *table);
const struct flag_type *nib_lookup_stat_table(LLIST *created, const char *name);
const char *nib_get_stat_table_name(LLIST *created, const struct flag_type *table);
const char *nib_get_flag_string(const struct flag_type *table, long bits);
const char *nib_get_stat_string(const struct flag_type *table, long bits);
bool nib_find_flag_value(const struct flag_type *table, const char *name, bool *settable, flag_value_t *output);
bool nib_flag_add_table(LLIST *names, char *table_name);
bool nib_stat_add_table(LLIST *names, char *table_name);
struct flag_type_lookup *nib_flag_copy_table(struct flag_type_lookup *src);
void nib_flag_free_table(struct flag_type_lookup *lookup);
bool nib_flag_tables_init();
void nib_flag_tables_cleanup();
int nib_add_used_table(const struct flag_type *table);

// interpret.c
int nib_interpret_script(NIB_SCRIPT *script /* add arguments */);
NIB_SCRIPT_RUNTIME *nib_step_execute_init(NIB_SCRIPT *script /* add arguments*/ );
bool nib_is_execution_done(NIB_SCRIPT_RUNTIME *nsr);
bool nib_step_execute(NIB_SCRIPT_RUNTIME *nsr);
void nib_step_execute_cleanup(NIB_SCRIPT_RUNTIME *nsr);
void nib_step_execute_show(NIB_SCRIPT_RUNTIME *nsr, int rows, int cols);
int nib_get_last_return(NIB_SCRIPT_RUNTIME *nsr);
const char *nib_get_debug(NIB_SCRIPT_RUNTIME *nsr);

// list.c
LLIST *list_create(bool purge);
LLIST *list_createx(bool purge, LISTCOPY_FUNC copier, LISTDESTROY_FUNC deleter);
LLIST *list_copy(LLIST *src);
void list_clear(LLIST *lp);
void list_purge(LLIST *lp);
void list_destroy(LLIST *lp);
void list_cull(LLIST *lp);
void list_addref(LLIST *lp);
void list_remref(LLIST *lp);
bool list_addlink(LLIST *lp, void *data);
bool list_appendlink(LLIST *lp, void *data);
bool list_appendlist(LLIST *lp, LLIST *src);
void list_remlink(LLIST *lp, void *data, bool del);
void *list_randomdata(LLIST *lp);
void *list_nthdata(LLIST *lp, int nth);
void **list_nthdataptr(LLIST *lp, int nth);
void list_remnthlink(LLIST *lp, register int nth, bool del);
bool list_contains(LLIST *lp, register void *ptr, int (*cmp)(void *a, void *b));
bool list_hasdata(LLIST *lp, register void *ptr);
int list_size(LLIST *lp);
int list_getindex(LLIST *lp, void *data);
bool list_movelink(LLIST *lp, int from, int to);
bool list_insertlink(LLIST *lp, void *data, int to);
void iterator_start(ITERATOR *it, LLIST *lp);
void iterator_start_nth(ITERATOR *it, LLIST *lp, int nth);
LLIST_LINK *iterator_next(ITERATOR *it);
void *iterator_nextdata(ITERATOR *it);
void **iterator_nextdataptr(ITERATOR *it);
void *iterator_prevdata(ITERATOR *it);
void *iterator_currentdata(ITERATOR *it);
void iterator_remcurrent(ITERATOR *it);
void iterator_reset(ITERATOR *it);
void iterator_stop(ITERATOR *it);
bool iterator_insert_before(ITERATOR *it, void *data);
bool iterator_insert_after(ITERATOR *it, void *data);
bool list_isvalid(LLIST *lp);
void *list_last(LLIST *list);
bool iterator_hasdata(ITERATOR *it);

// mem.c
NIB_BUFFER *new_mem_buffer_size(int size);
NIB_BUFFER *new_mem_buffer();
void free_mem_buffer(NIB_BUFFER *buffer);
bool mem_buffer_append_byte(NIB_BUFFER *buffer, nib_bytecode_t ch);
bool mem_buffer_append_short(NIB_BUFFER *buffer, short data);
bool mem_buffer_append_int(NIB_BUFFER *buffer, int data);
bool mem_buffer_append_long(NIB_BUFFER *buffer, long data);
bool mem_buffer_append_float(NIB_BUFFER *buffer, double data);
bool mem_buffer_append_pointer(NIB_BUFFER *buffer, void *data);
bool mem_buffer_update_short(NIB_BUFFER *buffer, int offset, short data);
bool mem_buffer_update_int(NIB_BUFFER *buffer, int offset, int data);
bool mem_buffer_update_long(NIB_BUFFER *buffer, int offset, long data);
bool mem_buffer_update_float(NIB_BUFFER *buffer, int offset, double data);
bool mem_buffer_update_pointer(NIB_BUFFER *buffer, int offset, void *data);
bool mem_buffer_append(NIB_BUFFER *buffer, nib_bytecode_p data, int len);
bool mem_buffer_extend(NIB_BUFFER *buffer, int offset, int len);
bool mem_buffer_prune(NIB_BUFFER *buffer, int offset, int len);
void mem_buffer_clear(NIB_BUFFER *buffer);
nib_bytecode_p mem_buffer_get(NIB_BUFFER *buffer);

// methods.c
size_t *nib_field_offset_lookup(NIB_TYPE *context, char *name);
bool nib_field_valid_context(NIB_TYPE *context);
NIB_FIELD *nib_field_get_byid(NIB_SCRIPT_STACK_TYPE context, int id);
NIB_FIELD *nib_field_get(NIB_TYPE *context, char *name);
bool nib_field_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool readonly, size_t offset, METHOD_FUNC *method);
bool nib_method_valid_context(NIB_TYPE *context);
METHOD_FUNC *nib_method_func_lookup(const char *name);
NIB_METHOD *new_nib_method(char *name, NIB_TYPE *ret, LLIST *params, char *method_name, METHOD_FUNC *method_func);
void free_nib_method(NIB_METHOD *method);
NIB_METHOD *nib_method_get_byid(NIB_SCRIPT_STACK_TYPE context, int id);
NIB_METHOD *nib_method_get(NIB_TYPE *context, char *name, LLIST *params);
bool nib_method_exists(NIB_TYPE *context, char *name, LLIST *params);
bool nib_method_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, LLIST *params, char *method_name, METHOD_FUNC *method_func);
bool nib_methods_init();
void nib_methods_cleanup();
void nib_method_get_prototype(NIB_METHOD *method, char *buffer, size_t max_len);

// scopetree.c
void nib_init_scopetree();
void nib_cleanup_scopetree();
int nib_get_scope();
int nib_get_max_scope();
int nib_push_scope();
int nib_pop_scope();
bool nib_in_scope(int scope);
void nib_dump_scopetree();


// types.c
extern NIB_TYPE *nibtype_null;
extern NIB_TYPE *nibtype_void;
extern NIB_TYPE *nibtype_any;
extern NIB_TYPE *nibtype_bool;
extern NIB_TYPE *nibtype_char;
extern NIB_TYPE *nibtype_int;
extern NIB_TYPE *nibtype_float;
extern NIB_TYPE *nibtype_string;
extern NIB_TYPE *nibtype_flag;
extern NIB_TYPE *nibtype_list;
extern NIB_TYPE *nibtype_stat;
extern NIB_TYPE *nibtype_map;
extern NIB_TYPE *nibtype_widevnum;
extern NIB_TYPE *nibtype_varargs;

// Internal entity types (not exhaustive)
extern NIB_TYPE *nibtype_area;
extern NIB_TYPE *nibtype_dungeon;
extern NIB_TYPE *nibtype_instance;
extern NIB_TYPE *nibtype_mobile;
extern NIB_TYPE *nibtype_object;
extern NIB_TYPE *nibtype_quest;
extern NIB_TYPE *nibtype_room;
extern NIB_TYPE *nibtype_ship;
extern NIB_TYPE *nibtype_token;

NIB_TYPE *new_nib_type_flag(int bits);
NIB_TYPE *new_nib_type_flag_named(LLIST *names);
NIB_TYPE *new_nib_type_flag_table(const struct flag_type *table);
NIB_TYPE *new_nib_type_stat_named(LLIST *names);
NIB_TYPE *new_nib_type_stat_table(const struct flag_type *table);
NIB_TYPE *new_nib_type_list(NIB_TYPE *elem);
void free_nib_type(NIB_TYPE *type);
NIB_TYPE *nib_type_copy(NIB_TYPE *src);
char *nib_get_typename(NIB_SCRIPT *context, NIB_TYPE *type);
int nib_type_get_flag_index(NIB_TYPE *type, const char *str);
NIB_TYPE *nib_combine_types(NIB_TYPE *a, NIB_TYPE *b);
bool are_nib_types_equal(NIB_TYPE *a, NIB_TYPE *b);

// utils.c
extern unsigned long nib_allocations;

long number_range(long from, long to);
void ltoa(register long num, register char *output);
// UTF-8 aware
int utf8_bytes(utf8char_t ch);
char *utf8_getbytes(utf8char_t ch);
char *utf8_nextchar(const char *str);
char *utf8_skip(register const char *str, register size_t len);
utf8char_t utf8_getchar(const char *str);
size_t utf8_strlen(const char *str);
int utf8_str_cmp(const char *astr, const char *bstr);
bool utf8_str_prefix(const char *astr, const char *bstr);
bool utf8_str_infix(const char *astr, const char *bstr);
bool utf8_str_suffix(const char *astr, const char *bstr);
bool utf8_isvalid(utf8char_t ch);
bool utf8_isprint(utf8char_t ch);
const char *utf8_getnchars(const char *str, int len);
// Not UTF-8 aware
int str_cmp(const char *astr, const char *bstr);
bool str_prefix(const char *astr, const char *bstr);
bool str_infix(const char *astr, const char *bstr);
bool str_suffix(const char *astr, const char *bstr);
char *nib_strdup(const char *str);
void *nib_malloc(size_t size);
void *nib_calloc(size_t count, size_t size);
void nib_free(void *data);
void nib_ledger_cleanup();
void nib_ledger_display();
void hex_dump(void *addr, size_t size);

// variables.c
NIB_VARIABLE *nib_new_variable(char *name, NIB_TYPE *type, int scope, bool constant);
NIB_VARIABLE *nib_copy_variable(NIB_VARIABLE *src);
void nib_free_variable(NIB_VARIABLE *var);


#endif /* __NIBLANG_H__ */