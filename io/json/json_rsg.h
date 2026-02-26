#ifndef JSON_RSG_H
#define JSON_RSG_H

#include "../../merc.h"

bool json_rsg_save_generators(RANDOM_STRING *rsg_list, long next_uid, int *saved_count);
RANDOM_STRING *json_rsg_load_generators(long *next_uid_out, int *loaded_count);

#endif
