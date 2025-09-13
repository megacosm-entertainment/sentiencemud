#ifndef __SCRIPT_H__
#define __SCRIPT_H__

/*
	Expects the following defined:
	.	bool
	.	WNUM (widevnum)
	.	LLIST
	.	AREA_DATA
	.	CHAR_DATA
	.	ROOM_INDEX_DATA
	.
	.	pVARIABLE
	.	All of the BIT flags (A, B, C, etc)
*/

// SWITCH CASE DATA

typedef enum nib_script_class_s
{
	NSC_AREA = 0,
	NSC_DUNGEON,
	NSC_INSTANCE,
	NSC_MOBILE,
	NSC_OBJECT,
	NSC_ROOM,
	NSC_TOKEN
} NIB_SCRIPT_CLASS;

typedef enum nib_primary_types
{
	NT_UNKNOWN = 0,
	NT_BOOLEAN,
	NT_NUMBER,
	NT_FLOAT,
	NT_CHAR,
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
	NT_ANYPTR
} NIB_PRIMARY_TYPE;

typedef enum nib_type_class {
	NTC_VOID = 0,		// Indicates nothing
	NTC_ANY,			// Indicates anything
	NTC_PRIMARY,
	NTC_FLAG,
	NTC_STAT,
	NTC_LIST,
	NTC_ARRAY,
	NTC_VARARGS		// Special type used for function prototypes
} NIB_TYPE_CLASS;


struct nib_type {
	bool _static;		// Statically defined, do not "free"

	int type_class;

	union {
		int dummy;
		NIB_PRIMARY_TYPE primary;	// Used by type_class PRIMARY
		struct {
			int bits;
			LLIST *names;			// Only used if the bits is 0
			const struct flag_type *table;
		} flag;						// Used by type_class FLAG
		struct {
			LLIST *names;
			const struct flag_type *table;
		} stat;
		NIB_TYPE *type;				// Used by type_class LIST
	} _;

	char *name;
};


struct nib_script_global_variable_s
{
	NIB_TYPE *type;
	NIB_SCRIPT_STACK_TYPE stype;
	pVARIABLE var;	// Actual global variable
};

struct nib_script_local_variable_s
{
	char *name;
	NIB_TYPE *type;
	NIB_SCRIPT_STACK_TYPE stype;
	bool constant;			// Just to guard against writing
};

struct nib_script_type_s
{
	// NIB_SCRIPT *next;
	// long vnum;

	NIB_SCRIPT_CLASS script_class;	// Is this an area prog? mob prog? what?

	char *src;

	LLIST *comments;

	int code_len;
	nib_bytecode_p code;

	// TODO: switch cases

	int n_globals;
	NIB_GLOBAL_VAR *globals;

	int n_locals;
	NIB_LOCAL_VAR *locals;

	int n_tables;
	struct flag_type **tables;	// Combined FLAG and STAT table usage

	LLIST *flag_tables;
	LLIST *stat_tables;

	int n_strings;
	char **strings;
};


NIB_SCRIPT *new_nib_script(const char *src, NIB_SCRIPT_CLASS sc);
void free_nib_script(NIB_SCRIPT *script);
void nib_decompile_code(NIB_SCRIPT *script);
void nib_dump_script_tables(NIB_SCRIPT *script);

#endif