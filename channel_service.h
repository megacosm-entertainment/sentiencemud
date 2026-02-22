#ifndef CHANNEL_SERVICE_H
#define CHANNEL_SERVICE_H

#include <stdbool.h>
#include "channel_transport.h"
#include "channel_registry.h"   /* CHANNEL_SCOPE, CHANNEL_DEF_DATA, CHANNEL_ROUTE_TARGETS */

/* Forward declaration from merc.h */
typedef struct char_data        CHAR_DATA;
typedef struct descriptor_data  DESCRIPTOR_DATA;

/* Backward-compatibility alias — existing call-sites that reference
 * CHANNEL_DEFINITION continue to work without changes.              */
typedef CHANNEL_DEF_DATA CHANNEL_DEFINITION;

bool channel_service_init(void);
void channel_service_shutdown(void);
void channel_service_pulse(void);

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text);
bool channel_service_send_directed(CHAR_DATA *sender, const char *channel_id,
                                   CHAR_DATA *recipient, const char *raw_text);
const char *channel_service_backend_name(void);
int channel_service_describe_subscriptions(CHAR_DATA *ch, char *out, size_t out_size);
bool channel_can_deliver_to_descriptor(CHAR_DATA *sender,
                                       DESCRIPTOR_DATA *desc,
                                       long comm_block_flag,
                                       bool honor_quiet,
                                       bool honor_ignore,
                                       CHAR_DATA **out_victim);

#endif
