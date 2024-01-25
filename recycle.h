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

AREA_REGION *new_area_region();
void free_area_region(AREA_REGION *region);


/* mob id and memory procedures */
#define MD MEM_DATA
long 	get_pc_id args( (void) );
void get_mob_id(CHAR_DATA *ch);
void get_token_id(TOKEN_DATA *token);
void get_obj_id(OBJ_DATA *obj);
void get_vroom_id(ROOM_INDEX_DATA *vroom);
void get_ship_id(SHIP_DATA *ship);
void get_instance_id(INSTANCE *inst);
void get_dungeon_id(DUNGEON *dng);
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

MISSIONARY_DATA *new_missionary_data();
void free_missionary_data(MISSIONARY_DATA *q);

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

BLUEPRINT_EXIT_DATA *new_blueprint_exit_data();
void free_blueprint_exit_data(BLUEPRINT_EXIT_DATA *ex);

BLUEPRINT_WEIGHTED_LINK_DATA *new_weighted_random_link();
void free_weighted_random_link(BLUEPRINT_WEIGHTED_LINK_DATA *weighted);

BLUEPRINT_LAYOUT_LINK_DATA *new_blueprint_layout_link_data();
void free_blueprint_layout_link_data(BLUEPRINT_LAYOUT_LINK_DATA *data);

BLUEPRINT_LAYOUT_SECTION_DATA *new_blueprint_layout_section_data();
void free_blueprint_layout_section_data(BLUEPRINT_LAYOUT_SECTION_DATA *data);

BLUEPRINT_WEIGHTED_SECTION_DATA *new_weighted_random_section();
void free_weighted_random_section(BLUEPRINT_WEIGHTED_SECTION_DATA *weighted);

MAZE_WEIGHTED_ROOM *new_maze_weighted_room();
void free_maze_weighted_room(MAZE_WEIGHTED_ROOM *room);

MAZE_FIXED_ROOM *new_maze_fixed_room();
void free_maze_fixed_room(MAZE_FIXED_ROOM *room);

BLUEPRINT *new_blueprint();
void free_blueprint(BLUEPRINT *bp);

NAMED_SPECIAL_EXIT *new_named_special_exit();
void free_named_special_exit(NAMED_SPECIAL_EXIT *special);

INSTANCE_SECTION *new_instance_section();
void free_instance_section(INSTANCE_SECTION *section);

NAMED_SPECIAL_ROOM *new_named_special_room();
void free_named_special_room(NAMED_SPECIAL_ROOM *special);

NAMED_SPECIAL_EXIT *new_named_special_exit();
void free_named_special_exit(NAMED_SPECIAL_EXIT *special);

INSTANCE *new_instance();
void free_instance(INSTANCE *instance);

DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *new_weighted_random_floor();
void free_weighted_random_floor(DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted);

DUNGEON_INDEX_WEIGHTED_EXIT_DATA *new_weighted_random_exit();
void free_weighted_random_exit(DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted);

DUNGEON_INDEX_LEVEL_DATA *new_dungeon_index_level();
void free_dungeon_index_level(DUNGEON_INDEX_LEVEL_DATA *dungeon_level);

DUNGEON_INDEX_SPECIAL_ROOM *new_dungeon_index_special_room();
void free_dungeon_index_special_room(DUNGEON_INDEX_SPECIAL_ROOM *special);

DUNGEON_INDEX_SPECIAL_EXIT *new_dungeon_index_special_exit();
void free_dungeon_index_special_exit(DUNGEON_INDEX_SPECIAL_EXIT *special);

DUNGEON_INDEX_DATA *new_dungeon_index();
void free_dungeon_index(DUNGEON_INDEX_DATA *dungeon);

DUNGEON *new_dungeon();
void free_dungeon(DUNGEON *dungeon);

LOCK_STATE_KEY *new_lock_state_key();
void free_lock_state_key(LOCK_STATE_KEY *data);

LOCK_STATE *new_lock_state();
LOCK_STATE *copy_lock_state(LOCK_STATE *src);
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

AURA_DATA *new_aura_data();
void free_aura_data(AURA_DATA *aura);

SECTOR_DATA *new_sector_data();
void free_sector_data(SECTOR_DATA *sector);

// Item Multi-typing

// Ammo
AMMO_DATA *new_ammo_data();
AMMO_DATA *copy_ammo_data(AMMO_DATA *src);
void free_ammo_data(AMMO_DATA *data);

// Armor
ADORNMENT_DATA *new_adornment_data();
ADORNMENT_DATA *copy_adornment_data(ADORNMENT_DATA *src);
void free_adornment_data(ADORNMENT_DATA *data);

ARMOR_DATA *new_armor_data();
ARMOR_DATA *copy_armor_data(ARMOR_DATA *src);
void free_armor_data(ARMOR_DATA *data);

// Book
BOOK_PAGE *new_book_page();
BOOK_PAGE *copy_book_page(BOOK_PAGE *src);
void free_book_page(BOOK_PAGE *page);

BOOK_DATA *new_book_data();
BOOK_DATA *copy_book_data(BOOK_DATA *src);
void free_book_data(BOOK_DATA *data);

// Container
CONTAINER_FILTER *new_container_filter();
void free_container_filter(CONTAINER_FILTER *filter);

CONTAINER_DATA *new_container_data();
CONTAINER_DATA *copy_container_data(CONTAINER_DATA *src);
void free_container_data(CONTAINER_DATA *data);

// Fluid Containers
FLUID_CONTAINER_DATA *new_fluid_container_data();
FLUID_CONTAINER_DATA *copy_fluid_container_data(FLUID_CONTAINER_DATA *src);
void free_fluid_container_data(FLUID_CONTAINER_DATA *data);

// Food
FOOD_BUFF_DATA *new_food_buff_data();
void free_food_buff_data(FOOD_BUFF_DATA *data);

FOOD_DATA *new_food_data();
FOOD_DATA *copy_food_data(FOOD_DATA *src);
void free_food_data(FOOD_DATA *data);

// Furniture
FURNITURE_COMPARTMENT *new_furniture_compartment();
void free_furniture_compartment(FURNITURE_COMPARTMENT *data);

FURNITURE_DATA *new_furniture_data();
FURNITURE_DATA *copy_furniture_data(FURNITURE_DATA *src);
void free_furniture_data(FURNITURE_DATA *data);

// Ink
INK_DATA *new_ink_data();
INK_DATA *copy_ink_data();
void free_ink_data(INK_DATA *data);

// Instrument
INSTRUMENT_DATA *new_instrument_data();
INSTRUMENT_DATA *copy_instrument_data();
void free_instrument_data(INSTRUMENT_DATA *data);

// Jewelry
JEWELRY_DATA *new_jewelry_data();
JEWELRY_DATA *copy_jewelry_data(JEWELRY_DATA *src);
void free_jewelry_data(JEWELRY_DATA *data);

// Light
LIGHT_DATA *new_light_data();
LIGHT_DATA *copy_light_data(LIGHT_DATA *src);
void free_light_data(LIGHT_DATA *data);

// Mist
MIST_DATA *new_mist_data();
MIST_DATA *copy_mist_data(MIST_DATA *src);
void free_mist_data(MIST_DATA *data);

// Money
MONEY_DATA *new_money_data();
MONEY_DATA *copy_money_data(MONEY_DATA *src);
void free_money_data(MONEY_DATA *data);

// Portal
PORTAL_DATA *new_portal_data();
PORTAL_DATA *copy_portal_data(PORTAL_DATA *src, bool repop);
void free_portal_data(PORTAL_DATA *data);

// Scroll
SCROLL_DATA *new_scroll_data();
SCROLL_DATA *copy_scroll_data(SCROLL_DATA *src);
void free_scroll_data(SCROLL_DATA *data);

// Tattoo
TATTOO_DATA *new_tattoo_data();
TATTOO_DATA *copy_tattoo_data(TATTOO_DATA *src);
void free_tattoo_data(TATTOO_DATA *data);

// Wand
WAND_DATA *new_wand_data();
WAND_DATA *copy_wand_data(WAND_DATA *src);
void free_wand_data(WAND_DATA *data);

// Weapon
WEAPON_DATA *new_weapon_data();
WEAPON_DATA *copy_weapon_data(WEAPON_DATA *src);
void free_weapon_data(WEAPON_DATA *data);


RACE_DATA *new_race_data();
void free_race_data(RACE_DATA *data);

CLASS_DATA *new_class_data();
void free_class_data(CLASS_DATA *data);

CLASS_LEVEL *new_class_level();
void free_class_level(CLASS_LEVEL *cl);

SKILL_CLASS_LEVEL *new_skill_class_level();
void free_skill_class_level(SKILL_CLASS_LEVEL *data);


LIQUID *new_liquid();
void free_liquid(LIQUID *liq);

MATERIAL *new_material();
void free_material(MATERIAL *data);

SKILL_DATA *new_skill_data();
void free_skill_data(SKILL_DATA *skill);

SKILL_GROUP *new_skill_group_data();
void free_skill_group_data(SKILL_GROUP *group);

SONG_DATA *new_song_data();
void free_song_data(SONG_DATA *song);


REPUTATION_INDEX_RANK_DATA *new_reputation_index_rank_data();
void free_reputation_index_rank_data(REPUTATION_INDEX_RANK_DATA *data);

REPUTATION_INDEX_DATA *new_reputation_index_data();
void free_reputation_index_data(REPUTATION_INDEX_DATA *data);

REPUTATION_DATA *new_reputation_data();
void free_reputation_data(REPUTATION_DATA *data);


MOB_REPUTATION_DATA *new_mob_reputation_data();
MOB_REPUTATION_DATA *copy_mob_reputation_data(MOB_REPUTATION_DATA *src);
void free_mob_reputation_data(MOB_REPUTATION_DATA *data);

PRACTICE_COST_DATA *new_practice_cost_data();
PRACTICE_COST_DATA *copy_practice_cost_data(PRACTICE_COST_DATA *src);
void free_practice_cost_data(PRACTICE_COST_DATA *cost_data);

PRACTICE_ENTRY_DATA *new_practice_entry_data();
PRACTICE_ENTRY_DATA *copy_practice_entry_data(PRACTICE_ENTRY_DATA *src);
void free_practice_entry_data(PRACTICE_ENTRY_DATA *entry_data);

PRACTICE_DATA *new_practice_data();
PRACTICE_DATA *copy_practice_data(PRACTICE_DATA *src);
void free_practice_data(PRACTICE_DATA *data);
