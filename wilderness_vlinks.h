#ifndef WILDERNESS_VLINKS_H
#define WILDERNESS_VLINKS_H

#include <stdbool.h>

typedef struct wilds_data WILDS_DATA;

#define WILDERNESS_VLINKS_VERSION 1

bool wilderness_vlinks_init(void);
void wilderness_vlinks_shutdown(void);

bool wilderness_vlinks_load(WILDS_DATA *pWilds);
bool wilderness_vlinks_save(WILDS_DATA *pWilds);

#endif
