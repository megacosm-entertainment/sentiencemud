#ifndef WILDERNESS_WMAP_H
#define WILDERNESS_WMAP_H

#include <stdbool.h>

typedef struct wilds_data WILDS_DATA;

#define WILDERNESS_WMAP_VERSION 1U

bool wilderness_wmap_init(void);
void wilderness_wmap_shutdown(void);

bool wilderness_wmap_load(WILDS_DATA *pWilds);
bool wilderness_wmap_save(WILDS_DATA *pWilds);

#endif
