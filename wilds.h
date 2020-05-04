/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/* external routines */
extern void assign_area_vnum args(( long vnum ));                    /* OLC */

/* external global variables */
extern long top_exit;

/* internal globals */
long top_wilds;
long top_wilds_terrain;
long top_wilds_vlink;
long top_wilds_vroom;
WILDS_DATA *wilds_free;
WILDS_TERRAIN *wilds_terrain_free;
WILDS_VLINK *wilds_vlink_free;

long next_wilds_uid;

/* Structure types */
/* typedef struct    wilds_vlink      WILDS_VLINK; */

/* Exported local routines */
char            *vlinkage_bit_name  args ((int vlinktype ));
void            load_wilds args (( FILE *fp, AREA_DATA *pArea ));
void            save_wilds args (( FILE *fp, AREA_DATA *pArea ));
WILDS_DATA      *new_wilds args (( void ));
void            free_wilds args (( WILDS_DATA *pWilds ));

ROOM_INDEX_DATA *create_wilds_vroom args ((WILDS_DATA *pWilds, int x, int y));
void            destroy_wilds_vroom args ((ROOM_INDEX_DATA *room));
WILDS_DATA	*get_wilds_from_uid args ((AREA_DATA *pArea, long uid));
ROOM_INDEX_DATA *get_wilds_vroom args (( WILDS_DATA *pWilds, int x, int y ));
int             get_wilds_vroom_x_by_dir args (( WILDS_DATA *pWilds, int x, int y, int door ));
int             get_wilds_vroom_y_by_dir args (( WILDS_DATA *pWilds, int x, int y, int door ));
void            char_to_vroom args(( CHAR_DATA *ch, WILDS_DATA *pWilds, int x, int y ));

WILDS_TERRAIN   *new_terrain args (( WILDS_DATA *pWilds ));
WILDS_TERRAIN   *fread_terrain args (( FILE *fp, WILDS_DATA *pWilds ));
void            fwrite_terrain args ((FILE *fp, WILDS_TERRAIN *pTerrain));
WILDS_TERRAIN   *get_terrain_by_coors args (( WILDS_DATA *pWilds, int x, int y ));
WILDS_TERRAIN   *get_terrain_by_token args (( WILDS_DATA *pWilds, char token ));
bool            add_terrain args (( WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain ));
bool            del_terrain args (( WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain ));

WILDS_VLINK     *new_vlink args(( void ));
WILDS_VLINK     *fread_vlink args(( FILE *fp ));
WILDS_VLINK	*get_vlink_from_uid args ((WILDS_DATA *pWilds, long uid));
WILDS_VLINK	*get_vlink_from_index args ((WILDS_DATA *pWilds, long index));
void            add_vlink args (( WILDS_DATA *pWilds, WILDS_VLINK *pVLink ));
bool            link_vlink args (( WILDS_VLINK *pVLink ));
bool            unlink_vlink args (( WILDS_VLINK *pVLink ));
void		fix_vlinks ();

bool            check_for_bad_room args (( WILDS_DATA *pWilds, int x, int y ));
bool            map_char_cmp args (( WILDS_DATA *pWilds, int x, int y, char *check ));
int             get_squares_to_show_x args (( int bonus_view ));
int             get_squares_to_show_y args (( int bonus_view ));
void            show_map_to_char_wyx args(( WILDS_DATA *pWilds, int wx, int wy, CHAR_DATA *to,
                                        int vx, int vy, int bonus_view_x, int bonus_view_y, bool olc ));
void            show_map_to_char args(( CHAR_DATA *ch, CHAR_DATA *to,
                                        int bonus_view_x, int bonus_view_y, bool olc ));

WILDS_VLINK *vroom_get_to_vlink args((WILDS_DATA *pWilds, int x, int y, int door));
void show_vroom_header_to_char args((WILDS_TERRAIN *pTerrain, WILDS_DATA *pWilds, int wx, int wy, CHAR_DATA *to));

#define WILDS_FORMAT_TERRAINMAP		0
#define WILDS_FORMAT_ROOMMAZE		1
#define WILDS_FORMAT_TERRAINMAPMAZE	2

/*
 * vlink type flags
 */
#define VLINK_UNLINKED		0
#define VLINK_TO_WILDS		(A)
#define VLINK_FROM_WILDS	(B)
#define VLINK_PORTAL		(C)

/* These types allow us to use read_room_new() routine to load up both normal rooms
 * and wilds terrain type rooms. This has the positive benefit of when new things
 * are added to the room structures, we don't need to update the wilds code as well.
 */
#define ROOMTYPE_NORMAL		0
#define ROOMTYPE_TERRAIN	1

/*
 * Wilds Terrain template definition.
 */
struct    wilds_terrain
{
    WILDS_TERRAIN   *prev;
    WILDS_TERRAIN   *next;
    WILDS_DATA      *pWilds;
    bool             valid;

    char            mapchar;
    char            *showchar;
    char            *showname;
    char            *briefdesc;
    bool            nonroom;
    ROOM_INDEX_DATA *template;
};



/*
 * Wilds Struct definition.
 */
struct wilds_data
{
    WILDS_DATA      *next;
    AREA_DATA       *pArea;
    bool            valid;

    long            uid;             /* Unique IDentifying number */
    int             wilds_format;    /* to accommodate the maze code through multiple formats */
    char            *name;
    char            *staticmap;      /* The actual wilds map to be loaded and saved */
    char            *map;	     /* The working map */
    int             map_size_x;
    int             map_size_y;
    int             startx;
    int             starty;
    int             sector_size_x;   /* Dynamic wilds sector management */
    int             sector_size_y;   /* These variables dictate the dimensions of a sector */
    char            cDefaultTerrain;
    WILDS_TERRAIN   *pTerrain;
    WILDS_VLINK     *pVLink;
    int             loaded_rooms;    /* Dynamically loaded vroom count for wilds v2 */
    LLIST *loaded_vrooms;
    int             loaded_mobs;
    CHAR_DATA       *char_list;        /* Statically allocated list of pointers to lists of characters */
    int             loaded_objs;
    OBJ_DATA        *obj_list;        /* Statically allocated list of pointers to lists of objs */
    int             nplayer;
    bool            empty;
    int             age;            /* current age */
    int             repop;          /* age to repop at */
};



struct wilds_vlink
{
    WILDS_DATA      *pWilds;
    WILDS_VLINK     *next;
    bool            valid;

    long            uid;
    ROOM_INDEX_DATA *pWildsVroom;
    ROOM_INDEX_DATA *pDestRoom;
    int             wildsorigin_x;
    int             wildsorigin_y;
    int             door;
    char            *map_tile;
    long            destvnum;
    int             default_linkage;
    int             current_linkage;
    char            *orig_description;
    char            *orig_keyword;
    long            orig_rs_flags;
    long            orig_key;
    int             orig_lock;
    int             orig_pick;
    char            *rev_description;
    char            *rev_keyword;
    long            rev_rs_flags;
    long            rev_key;
    int             rev_lock;
    int             rev_pick;
};



