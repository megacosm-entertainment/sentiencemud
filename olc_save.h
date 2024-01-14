/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

extern char *help_greeting;

/* function declarations */
void save_area_list();

/* write areas */
void save_area_new( AREA_DATA *area );
void save_rooms_new( FILE *fp, AREA_DATA *area );
void save_mobiles_new( FILE *fp, AREA_DATA *area );
void save_objects_new( FILE *fp, AREA_DATA *area );
void save_room_new( FILE *fp, ROOM_INDEX_DATA *room, int recordtype );
void save_mobile_new( FILE *fp, MOB_INDEX_DATA *mob );
void save_object_new( FILE *fp, OBJ_INDEX_DATA *obj );
void save_missionary_new( FILE *fp, MISSIONARY_DATA *missonary, AREA_DATA *pRefArea );
void save_scripts_new( FILE *fp, AREA_DATA *area );
void save_practice_data( FILE *fp, PRACTICE_DATA *data, AREA_DATA *area );
void save_shop_new( FILE *fp, SHOP_DATA *shop, AREA_DATA *pRefArea );
void save_ship_crew_index_new( FILE *fp, SHIP_CREW_INDEX_DATA *crew );
void save_spell(FILE *fp, SPELL_DATA *spell);
void save_tokens(FILE *fp, AREA_DATA *area);
void save_token(FILE *fp, TOKEN_INDEX_DATA *token);

/* read areas */
AREA_DATA *read_area_new( FILE *fp );
ROOM_INDEX_DATA *read_room_new( FILE *fp, AREA_DATA *area, int roomtype );
MOB_INDEX_DATA *read_mobile_new( FILE *fp, AREA_DATA *area );
OBJ_INDEX_DATA *read_object_new( FILE *fp, AREA_DATA *area );
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);
EXTRA_DESCR_DATA *read_extra_descr_new( FILE *fp );
CONDITIONAL_DESCR_DATA *read_conditional_descr_new( FILE *fp );
EXIT_DATA *read_exit_new( FILE *fp, AREA_DATA *area );
RESET_DATA *read_reset_new( FILE *fp, AREA_DATA *area );
AFFECT_DATA *read_obj_affect_new (FILE *fp);
AFFECT_DATA *read_obj_catalyst_new (FILE *fp);
PRACTICE_DATA *read_practice_data(FILE *fp, AREA_DATA *area);
SHOP_DATA *read_shop_new( FILE *fp, AREA_DATA *area);
SHIP_CREW_INDEX_DATA *read_ship_crew_index_new( FILE *fp );
MISSIONARY_DATA *read_missionary_new( FILE *fp, AREA_DATA *area);
TOKEN_INDEX_DATA *read_token( FILE *fp, AREA_DATA *area);

/* help files */
void save_helpfiles_new();
void read_helpfiles_new();
void save_help_category_new( FILE *fp, HELP_CATEGORY *hCat );
void save_help_new( FILE *fp, HELP_DATA *help );
HELP_CATEGORY *read_help_category_new( FILE *fp );
HELP_DATA *read_help_new( FILE *fp );

/* wilderness */
void read_virtual_rooms( FILE *fp, AREA_DATA *area );
void make_virtual_area(AREA_DATA *area);
void create_virtual_room_new( AREA_DATA *area, long vnum, int x, int y,
                          long parent, long base_vnum, int sizex, int sizey);
bool check_for_bad_room_new(AREA_DATA *area, int x, int y);
bool map_char_cmp_new(AREA_DATA *area, int x, int y, char *check);
void assign_area_vnum_new( AREA_DATA *area, long vnum );
