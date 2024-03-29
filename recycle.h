/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,	   *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *									   *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael	   *
 *  Chastain, Michael Quan, and Mitchell Tse.				   *
 *									   *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc	   *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.						   *
 *									   *
 *  Much time and thought has gone into this software and you are	   *
 *  benefitting.  We hope that you share your changes too.  What goes	   *
 *  around, comes around.						   *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/* externs */
extern char str_empty[1];
extern long mobile_count;

/* stuff for providing a crash-proof buffer */

#define MAX_BUF		16384
#define MAX_BUF_LIST 	16
#define BASE_BUF 	1024

/* valid states */
#define BUFFER_SAFE	0
#define BUFFER_OVERFLOW	1
#define BUFFER_FREED 	2

SKILL_ENTRY *new_skill_entry();
void free_skill_entry(SKILL_ENTRY *entry);

/* note recycling */
#define ND NOTE_DATA
ND	*new_note args( (void) );
void	free_note args( (NOTE_DATA *note) );
#undef ND

/* ban data recycling */
#define BD BAN_DATA
BD	*new_ban args( (void) );
void	free_ban args( (BAN_DATA *ban) );
#undef BD

/* descriptor recycling */
#define DD DESCRIPTOR_DATA
DD	*new_descriptor args( (void) );
void	free_descriptor args( (DESCRIPTOR_DATA *d) );
#undef DD

/* extra descr recycling */
#define ED EXTRA_DESCR_DATA
ED	*new_extra_descr args( (void) );
void	free_extra_descr args( (EXTRA_DESCR_DATA *ed) );
#undef ED

/* affect recycling */
#define AD AFFECT_DATA
AD	*new_affect args( (void) );
void	free_affect args( (AFFECT_DATA *af) );
#undef AD

/* object recycling */
#define OD OBJ_DATA
OD	*new_obj args( (void) );
void	free_obj args( (OBJ_DATA *obj) );
#undef OD

/* character recyling */
#define CD CHAR_DATA
#define PD PC_DATA
CD	*new_char args( (void) );
void	free_char args( (CHAR_DATA *ch) );
PD	*new_pcdata args( (void) );
void	free_pcdata args( (PC_DATA *pcdata) );
#undef PD
#undef CD


/* mob id and memory procedures */
#define MD MEM_DATA
long 	get_pc_id args( (void) );
void get_mob_id(CHAR_DATA *ch);
void get_token_id(TOKEN_DATA *token);
void get_obj_id(OBJ_DATA *obj);
void get_vroom_id(ROOM_INDEX_DATA *vroom);
void get_ship_id(SHIP_DATA *ship);
#undef MD

/* buffer procedures */
BUFFER	*new_buf args( (void) );
BUFFER  *new_buf_size args( (int size) );
void	free_buf args( (BUFFER *buffer) );
bool	add_buf_char args( (BUFFER *buffer, char ch) );
bool	add_buf args( (BUFFER *buffer, char *string) );
void	clear_buf args( (BUFFER *buffer) );
char	*buf_string args( (BUFFER *buffer) );

HELP_DATA *	new_help	args( ( void ) );

LLIST_UID_DATA *new_list_uid_data();
void free_list_uid_data(LLIST_UID_DATA *luid);

QUESTOR_DATA *new_questor_data();
void free_questor_data(QUESTOR_DATA *q);

OLC_POINT_BOOST *new_olc_point_boost();
void free_olc_point_boost(OLC_POINT_BOOST *boost);

SHOP_STOCK_DATA *new_shop_stock();
void free_shop_stock(SHOP_STOCK_DATA *pStock);

SCRIPT_PARAM *new_script_param();
void free_script_param(SCRIPT_PARAM *arg);

BLUEPRINT_LINK *new_blueprint_link();
void free_blueprint_link(BLUEPRINT_LINK *bl);

BLUEPRINT_SECTION *new_blueprint_section();
void free_blueprint_section(BLUEPRINT_SECTION *bs);

STATIC_BLUEPRINT_LINK *new_static_blueprint_link();
void free_static_blueprint_link(STATIC_BLUEPRINT_LINK *bl);

BLUEPRINT_SPECIAL_ROOM *new_blueprint_special_room();
void free_blueprint_special_room(BLUEPRINT_SPECIAL_ROOM *special);

BLUEPRINT *new_blueprint();
void free_blueprint(BLUEPRINT *bp);

INSTANCE_SECTION *new_instance_section();
void free_instance_section(INSTANCE_SECTION *section);

NAMED_SPECIAL_ROOM *new_named_special_room();
void free_named_special_room(NAMED_SPECIAL_ROOM *special);

INSTANCE *new_instance();
void free_instance(INSTANCE *instance);

DUNGEON_INDEX_SPECIAL_ROOM *new_dungeon_index_special_room();
void free_dungeon_index_special_room(DUNGEON_INDEX_SPECIAL_ROOM *special);

DUNGEON_INDEX_DATA *new_dungeon_index();
void free_dungeon_index(DUNGEON_INDEX_DATA *dungeon);

DUNGEON *new_dungeon();
void free_dungeon(DUNGEON *dungeon);

LOCK_STATE *new_lock_state();
void free_lock_state(LOCK_STATE *state);

SHIP_INDEX_DATA *new_ship_index();
void free_ship_index(SHIP_INDEX_DATA *ship);

SHIP_DATA *new_ship();
void free_ship(SHIP_DATA *ship);

SPECIAL_KEY_DATA *new_special_key();
void free_special_key(SPECIAL_KEY_DATA *sk);

LLIST *new_waypoints_list();
WAYPOINT_DATA *clone_waypoint(WAYPOINT_DATA *waypoint);

SHIP_ROUTE *new_ship_route();
void free_ship_route(SHIP_ROUTE *route);

SHIP_CREW_INDEX_DATA *new_ship_crew_index();
void free_ship_crew_index(SHIP_CREW_INDEX_DATA *crew);

CMD_DATA *new_cmd();
void free_cmd(CMD_DATA *cmd);