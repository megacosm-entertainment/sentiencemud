#ifndef __NIBLANG_H__
#define __NIBLANG_H__

#define NIB_GLOBAL_SCOPE	(-1)

typedef enum { false = 0, true = 1 } bool;

typedef struct list_type LLIST;
typedef struct list_link_type LLIST_LINK;
typedef void *LISTCOPY_FUNC(void *src);
typedef void LISTDESTROY_FUNC(void *data);
typedef struct iterator_type ITERATOR;

struct list_link_type
{
    LLIST_LINK *next;
    LLIST_LINK *prev;
    void *data;
};

struct list_type
{
    LLIST *next;
    LLIST_LINK *head;
    LLIST_LINK *tail;
    unsigned long ref;
    unsigned long size;
    LISTCOPY_FUNC *copier;
    LISTDESTROY_FUNC *deleter;
    bool valid;
    bool purge;
};

struct iterator_type
{
    LLIST *list;
    LLIST_LINK *current;
    bool moved;
};


typedef enum expressionOperationType
{
	expoVALUE = 0,
	expoADD,
	expoSUBTRACT,
	expoMULTIPLY,
	expoDIVIDE,
	expoEXPONENT,
	expoSHIFTLEFT,
	expoSHIFTRIGHT,
	expoEQ,
	expoNOTEQ,
	expoLT,
	expoLTE,
	expoGT,
	expoGTE,
	expoNOT,
	expoAND,
	expoOR,
	expoXOR,
	expoBITNOT,
	expoBITAND,
	expoBITOR,
	expoBITXOR,
	expoPREFIX,
	expoSUFFIX,
	expoINFIX
} EXPRESSION_OPERATION;

typedef enum expressionResultType
{
	exprAUTO = -1,
	exprBOOLEAN = 0,
	exprINTEGER,
	exprFLOAT
} EXPRESSION_RESULT;

typedef struct expressionNodeType
{
	EXPRESSION_RESULT result;
    EXPRESSION_OPERATION op;

	union {
		int i;
		double d;
		bool b;
	} value; /* /< valid only when type is expoVALUE */

    struct expressionNodeType *left; /* /<  left side of the tree */
    struct expressionNodeType *right; /* /< right side of the tree */
} EXPRESSION_NODE;

EXPRESSION_NODE *createInteger(int value);
EXPRESSION_NODE *createFloat(double value);
EXPRESSION_NODE *createBoolean(bool value);

EXPRESSION_NODE *createOperation(EXPRESSION_OPERATION op, EXPRESSION_RESULT result, EXPRESSION_NODE *left, EXPRESSION_NODE *right);
void deleteExpressionNode(EXPRESSION_NODE *b);

typedef struct nib_memory_buffer {
    short state;		// error state of the buffer
    int size;			// size in k
	int len;			// length of data in buffer
    char *buffer;		// actual buffer
} NIB_BUFFER;

typedef enum nib_primary_types
{
	NT_UNKNOWN = 0,
	NT_BOOLEAN,
	NT_NUMBER,
	NT_FLOAT,
	NT_STRING,
	NT_MAP,
	NT_AREA,
	NT_DUNGEON,
	NT_INSTANCE,
	NT_MOBILE,
	NT_OBJECT,
	NT_QUEST,
	NT_ROOM,
	NT_SHIP,
	NT_TOKEN,
	NT_WIDEVNUM,
	NT_ANY
} NIB_PRIMARY_TYPE;

typedef enum nib_type_class {
	NTC_PRIMARY = 0,
	NTC_FLAG,
	NTC_LIST,
	NTC_ARRAY
} NIB_TYPE_CLASS;

typedef struct nib_type NIB_TYPE;

struct nib_type {
	bool _static;		// Statically defined, do not "free"

	int type_class;

	union {
		NIB_PRIMARY_TYPE primary;	// Used by type_class PRIMARY
		struct {
			int bits;
			LLIST *names;			// Only used if the bits is 0
		} flag;						// Used by type_class FLAG
		NIB_TYPE *type;				// Used by type_class LIST
	} _;

	char *name;
};


typedef enum nibOpCodes {
	opGETSTRING,			// Get string #N from the string table
	opJUMP,					// Jump to the address
	opJUMP_ZERO,			// Test an expression, jump to address if zero
	opJUMP_NOT_ZERO,		// Test an expression, jump to address if not zero
} NIB_OP_CODE;

typedef struct nib_variable_type NIB_VARIABLE;

struct nib_variable_type {
	char *name; 	// Duplicated name, needs to be freed
	NIB_TYPE *type;
	int scope;
	int id;
};

typedef struct nib_scope_tree_node NIB_SCOPE_NODE;

struct nib_scope_tree_node {
	int scope;

	NIB_SCOPE_NODE *parent;		// Ancestors
	NIB_SCOPE_NODE *child;		// Descendants
	NIB_SCOPE_NODE *next;		// Sibling nodes
};


// compile.c
LLIST *nib_create_string_list();
LLIST *nib_create_variable_list();
bool nib_compile_script(const char *src);
void nib_cleanup_compile();
void nib_dump_global_variables();
void nib_dump_local_variables();
NIB_VARIABLE *nib_get_global_variable(const char *name);
NIB_VARIABLE *nib_get_local_variable(const char *name);
void nib_add_global_variable(NIB_VARIABLE *var);
void nib_add_local_variable(NIB_VARIABLE *var);


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
bool mem_buffer_append_byte(NIB_BUFFER *buffer, char ch);
bool mem_buffer_append_short(NIB_BUFFER *buffer, short data);
bool mem_buffer_append_int(NIB_BUFFER *buffer, int data);
bool mem_buffer_append_long(NIB_BUFFER *buffer, long data);
bool mem_buffer_append(NIB_BUFFER *buffer, char *data, int len);
void mem_buffer_clear(NIB_BUFFER *buffer);
char *mem_buffer_get(NIB_BUFFER *buffer);


// scopetree.c
void nib_init_scopetree();
void nib_cleanup_scopetree();
int nib_get_scope();
int nib_push_scope();
int nib_pop_scope();
bool nib_in_scope(int scope);
void nib_dump_scopetree();

// types.c
extern NIB_TYPE *nibtype_bool;
extern NIB_TYPE *nibtype_int;
extern NIB_TYPE *nibtype_float;
extern NIB_TYPE *nibtype_string;
extern NIB_TYPE *nibtype_flag;
extern NIB_TYPE *nibtype_map;
extern NIB_TYPE *nibtype_widevnum;

// Internal entity types
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
NIB_TYPE *new_nib_type_list(NIB_TYPE *elem);
void free_nib_type(NIB_TYPE *type);
char *nib_get_typename(NIB_TYPE *type);

// utils.c
int str_cmp(const char *astr, const char *bstr);

// variables.c
NIB_VARIABLE *nib_new_variable(char *name, NIB_TYPE *type, int scope);
NIB_VARIABLE *nib_copy_variable(NIB_VARIABLE *src);
void nib_free_variable(NIB_VARIABLE *var);

#endif /* __NIBLANG_H__ */