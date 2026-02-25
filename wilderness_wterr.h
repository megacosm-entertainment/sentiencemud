#ifndef WILDERNESS_WTERR_H
#define WILDERNESS_WTERR_H

#include <stdbool.h>

typedef struct wilds_data WILDS_DATA;

#define WILDERNESS_WTERR_VERSION 2

bool wilderness_wterr_init(void);
void wilderness_wterr_shutdown(void);

bool wilderness_wterr_load(WILDS_DATA *pWilds);
bool wilderness_wterr_save(WILDS_DATA *pWilds);

#endif
