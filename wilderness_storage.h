#ifndef WILDERNESS_STORAGE_H
#define WILDERNESS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct wilds_data WILDS_DATA;
typedef struct area_data AREA_DATA;

#define WILDERNESS_STORAGE_VERSION 1
#define WILDERNESS_STATE_VERSION 1
#define WILDERNESS_MODS_VERSION 1

bool wilderness_storage_init(void);
void wilderness_storage_shutdown(void);
void wilderness_storage_pulse(void);
void wilderness_storage_save_all_now(void);
void wilderness_storage_save_area_now(AREA_DATA *pArea);

void wilderness_storage_checksum_hex(const void *data, size_t len, char out_hex[65]);

bool wilderness_storage_build_state_path(const WILDS_DATA *pWilds, char *out, size_t out_size);
bool wilderness_storage_build_mods_path(const WILDS_DATA *pWilds, char *out, size_t out_size);
bool wilderness_storage_build_wmap_path(const WILDS_DATA *pWilds, char *out, size_t out_size);
bool wilderness_storage_build_wterr_path(const WILDS_DATA *pWilds, char *out, size_t out_size);
bool wilderness_storage_build_vlinks_path(const WILDS_DATA *pWilds, char *out, size_t out_size);
bool wilderness_storage_build_images_dir_path(const WILDS_DATA *pWilds, char *out, size_t out_size);

#endif
