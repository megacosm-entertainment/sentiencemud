#ifndef __RESERVED_H__
#define __RESERVED_H__

/* Forward declarations to avoid dependency issues */
struct char_data;
typedef struct char_data CHAR_DATA;
struct list_type;
typedef struct list_type LLIST;

/* Reserved item types */
#define RESERVED_MOB     0
#define RESERVED_OBJ     1
#define RESERVED_ROOM    2
#define RESERVED_AREA    3
#define RESERVED_SKILL   4
#define RESERVED_FLAG    5
#define RESERVED_COMMAND 6

/* Security level needed for the reserved editor */
#define MIN_SECURITY_RESERVED 8

/* Reserved item structure */
typedef struct reserved_data {
    char *name;            /* Name of the reserved item (e.g., "MOB_VNUM_DEATH") */
    int type;              /* Type (MOB, OBJ, ROOM, etc) */
    int id;                /* The VNUM or other ID value */
    bool removable;        /* Whether this item can be deleted */
    char *description;     /* Optional description */
} RESERVED_DATA;

/* Function macros */
#define RESERVED(fun)  bool fun(CHAR_DATA *ch, char *argument)

/* Function prototypes */
void init_reserved(void);
void load_reserved(void);
void save_reserved(void);
RESERVED_DATA *find_reserved(const char *name);
RESERVED_DATA *find_reserved_by_id(int id, int type);
int get_reserved_vnum(const char *name);
void init_reserved_defaults(void);
const char *reserved_types_get_name(int type);

/* Editor command */
void do_reserved(CHAR_DATA *ch, char *argument);

/* Global variables */
extern LLIST *reserved_vnums;
extern bool reserved_changed;

#endif /* __RESERVED_H__ */