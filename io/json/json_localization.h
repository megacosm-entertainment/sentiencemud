#ifndef JSON_LOCALIZATION_H
#define JSON_LOCALIZATION_H

#include "../../utils/localization.h"
#include "../../merc.h"
#include <jansson.h>

LOCALIZATION_DATA *localization_reload(const char *iso_name);
bool load_localizations(void);

#endif  // JSON_LOCALIZATION_H
