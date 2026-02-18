#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include "merc.h"

void event_legacy_war_command(CHAR_DATA *ch, char *argument);
void event_legacy_autowar_command(CHAR_DATA *ch, char *argument);
void event_legacy_startinvasion_command(CHAR_DATA *ch, char *argument);
void event_legacy_gq_command(CHAR_DATA *ch, char *argument);

#endif
