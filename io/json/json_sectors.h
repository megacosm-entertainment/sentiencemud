#ifndef JSON_SECTORS_H
#define JSON_SECTORS_H

#include <stdbool.h>
#include <stddef.h>

#include "../../sectors_runtime.h"

bool json_sector_data_save(const SECTOR_RUNTIME_DATA *sectors, size_t count);
bool json_sector_data_load(SECTOR_RUNTIME_DATA *sectors, size_t count);
bool json_sector_data_bootstrap_if_missing(const SECTOR_RUNTIME_DATA *sectors, size_t count);

#endif