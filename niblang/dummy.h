#ifndef __DUMMY_H__
#define __DUMMY_H__

// Everything about this needs to be removed and resolved once integrated into the main code

#define AFF_BLIND (A)
#define AFF_INVISIBLE (B)
#define AFF_DETECT_EVIL (C)
#define AFF_DETECT_INVIS (D)
#define AFF_DETECT_MAGIC (E)
#define AFF_DETECT_HIDDEN (F)
#define AFF_DETECT_GOOD (G)
#define AFF_SANCTUARY (H)
#define AFF_FAERIE_FIRE (I)
#define AFF_INFRARED (J)
#define AFF_CURSE (K)
#define AFF_DEATH_GRIP (L)
#define AFF_POISON (M)
/*				(N) */
/*				(O) */
#define AFF_SNEAK (P)
#define AFF_HIDE (Q)
#define AFF_SLEEP (R)
#define AFF_CHARM (S)
#define AFF_FLYING (T)
#define AFF_PASS_DOOR (U)
#define AFF_HASTE (V)
#define AFF_CALM (W)
#define AFF_PLAGUE (X)
#define AFF_WEAKEN (Y)
#define AFF_FRENZY (Z)
#define AFF_BERSERK (aa)
#define AFF_SWIM (bb)
#define AFF_REGENERATION (cc)
#define AFF_SLOW (dd)
#define AFF_WEB (ee)

#define AREA_NONE 0
#define AREA_CHANGED (A)
#define AREA_ADDED (B)
#define AREA_LOADING (C)
#define AREA_DARK (D)
#define AREA_NOMAP (E)
#define AREA_TESTPORT (F)
#define AREA_NO_RECALL (G)
#define AREA_NO_ROOMS (H)
#define AREA_NEWBIE (I)
#define AREA_NO_GET_RANDOM (J)
#define AREA_NO_FADING (K)
#define AREA_BLUEPRINT (L) // Area is used to hold rooms used for Blueprints.  Will block VLINKs
#define AREA_LOCKED (M)    // Area requires the player to unlock the area first
#define AREA_LOW_LEVEL (N) // Area is considered low level
#define AREA_IMMORTAL (O)  // Area is an immortal zone.
#define AREA_KEEP_LIVE (X) // Area's live data will not be overwritten when the area resets
#define AREA_PERSIST (Y)   // Area's live data will save to persist data
#define AREA_NO_SAVE (Z)
#define AREA_SOCIAL (aa)  // Area is meant for socializing.
#define AREA_HOUSING (bb) // Area is meant for housing.


typedef struct list_type LLIST;
typedef struct list_link_type LLIST_LINK;
typedef void *LISTCOPY_FUNC(void *src);
typedef void LISTDESTROY_FUNC(void *data);
typedef struct iterator_type ITERATOR;
typedef struct script_var_type VARIABLE, *pVARIABLE, **ppVARIABLE;
typedef struct script_type_s SCRIPT_DATA;

enum variable_enum {
	VAR_UNKNOWN,
	VAR_BOOLEAN,
	VAR_NUMBER,
	VAR_FLOAT,
	VAR_STRING,
	VAR_STRING_S,		/* Shared, allocated elsewhere! */
	VAR_WIDEVNUM,
	VAR_AREA,
	VAR_MAX
};


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


typedef struct area_data AREA_DATA;
struct area_data
{
	char *name;
	char *description;
	flag_value_t flags;
};

typedef struct wide_vnum_type
{
    AREA_DATA *pArea;
    long vnum;
} WNUM;


struct script_var_type {
	pVARIABLE next;
	char *name;
	bool save;
	bool index;
	bool readonly;
	int type;
	union {
		void *raw;
		long address;
		long num;
		double flt;
		bool b;
		char *str;
		AREA_DATA *area;
		WNUM wnum;
	} _;
};

bool variable_init();
void variable_cleanup();
pVARIABLE variable_get(const char *name);
pVARIABLE variable_new_number(const char *name, long value);
pVARIABLE variable_new_float(const char *name, double value);
pVARIABLE variable_new_bool(const char *name, bool value);
pVARIABLE variable_new_string(const char *name, char *str);
pVARIABLE variable_new_shared_string(const char *name, char *str);
pVARIABLE variable_new_widevnum(const char *name, AREA_DATA *area, long vnum);
pVARIABLE variable_new_area(const char *name, AREA_DATA *area);


#endif
