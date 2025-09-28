#ifndef __FUNCS_H__
#define __FUNCS_H__

// Assumes DECL_METHOD_FUNC defined

// Functions
DECL_METHOD_FUNC(function_random_percent);
DECL_METHOD_FUNC(function_print_msg);
DECL_METHOD_FUNC(function_reckoning);

// NUMBER methods
DECL_METHOD_FUNC(number_random_value);

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

// AREA Methods
DECL_METHOD_FUNC(area_get_room);

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

// OBJECT methods

// QUEST methods

// ROOM methods
DECL_METHOD_FUNC(room_get_exits);

// SHIP methods

// TOKEN methods

#endif
