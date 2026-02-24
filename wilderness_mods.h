#ifndef WILDERNESS_MODS_H
#define WILDERNESS_MODS_H

#include <stdbool.h>

typedef struct wilds_data WILDS_DATA;

typedef struct wilderness_mod_record
{
    int x;
    int y;
    char old_tile;
    char new_tile;
    int old_elevation;
    int new_elevation;
    bool permanent;
    long authored_by_uid;
    long authored_at;
} WILDERNESS_MOD_RECORD;

bool wilderness_mods_init(void);
void wilderness_mods_shutdown(void);
void wilderness_mods_pulse(void);

bool wilderness_mods_load(WILDS_DATA *pWilds);
bool wilderness_mods_save(WILDS_DATA *pWilds);
void wilderness_mods_mark_dirty(WILDS_DATA *pWilds, const char *reason);

#endif
