#ifndef __FUNCS_H__
#define __FUNCS_H__

// Assumes DECL_METHOD_FUNC defined

// Functions
DECL_METHOD_FUNC(function_random_percent);
DECL_METHOD_FUNC(function_print_msg);
DECL_METHOD_FUNC(function_reckoning);
DECL_METHOD_FUNC(function_get_time);

// NUMBER methods
DECL_METHOD_FUNC(number_random_value);

// TIME methods
DECL_METHOD_FUNC(time_add);
DECL_METHOD_FUNC(time_add_minutes);
DECL_METHOD_FUNC(time_add_hours);
DECL_METHOD_FUNC(time_add_days);
DECL_METHOD_FUNC(time_add_months);


// FLOAT methods

// BOOLEAN methods

// CHAR methods

// STRING methods
DECL_METHOD_FUNC(string_length);

// FLAG methods

// LIST methods
DECL_METHOD_FUNC(list_add);
DECL_METHOD_FUNC(list_insert);
DECL_METHOD_FUNC(list_remove);
DECL_METHOD_FUNC(list_size);

// ARRAY methods
DECL_METHOD_FUNC(array_length);

// STAT methods

// MAP methods

// WIDEVNUM methods

// AFFECT methods
DECL_METHOD_FUNC(affect_is_permanent);

// AREA Methods
DECL_METHOD_FUNC(area_get_room);

// CLASS Methods
DECL_METHOD_FUNC(class_display);
DECL_METHOD_FUNC(class_who);

// DICE Methods
DECL_METHOD_FUNC(dice_roll);

// DUNGEON methods

// EXIT
DECL_METHOD_FUNC(exit_get_door);
DECL_METHOD_FUNC(exit_get_direction);
DECL_METHOD_FUNC(exit_get_mate);
DECL_METHOD_FUNC(exit_get_north);
DECL_METHOD_FUNC(exit_get_northeast);
DECL_METHOD_FUNC(exit_get_east);
DECL_METHOD_FUNC(exit_get_southeast);
DECL_METHOD_FUNC(exit_get_south);
DECL_METHOD_FUNC(exit_get_southwest);
DECL_METHOD_FUNC(exit_get_west);
DECL_METHOD_FUNC(exit_get_northwest);
DECL_METHOD_FUNC(exit_get_up);
DECL_METHOD_FUNC(exit_get_down);
DECL_METHOD_FUNC(exit_is_oneway);
DECL_METHOD_FUNC(exit_is_twoway);

// INSTANCE methods

// MOBILE methods
DECL_METHOD_FUNC(mobile_get_widevnum);
DECL_METHOD_FUNC(mobile_is_pc);
DECL_METHOD_FUNC(mobile_get_equipment);

// OBJECT methods

// QUEST methods

// RANK methods
DECL_METHOD_FUNC(rank_color);

// REPUTATION methods
DECL_METHOD_FUNC(reputation_name);
DECL_METHOD_FUNC(reputation_description);
DECL_METHOD_FUNC(reputation_comments);
DECL_METHOD_FUNC(reputation_widevnum);
DECL_METHOD_FUNC(reputation_rank);
DECL_METHOD_FUNC(reputation_maximum_rank);
DECL_METHOD_FUNC(reputation_ranks);
DECL_METHOD_FUNC(reputation_initial_rank);
DECL_METHOD_FUNC(reputation_initial_reputation);

// ROOM methods
DECL_METHOD_FUNC(room_get_exits);
DECL_METHOD_FUNC(room_set_sector);
DECL_METHOD_FUNC(room_reset);

// SHIP methods

// TOKEN methods
DECL_METHOD_FUNC(token_owner_type);
DECL_METHOD_FUNC(token_get_index_value);

#endif
