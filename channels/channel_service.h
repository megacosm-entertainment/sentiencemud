#ifndef CHANNEL_SERVICE_H
#define CHANNEL_SERVICE_H

#include <stdbool.h>
#include "channel_transport.h"
#include "channel_registry.h"   /* CHANNEL_SCOPE, CHANNEL_DEF_DATA, CHANNEL_ROUTE_TARGETS */

/* Forward declaration from merc.h */
typedef struct char_data        CHAR_DATA;
typedef struct descriptor_data  DESCRIPTOR_DATA;
typedef struct obj_data         OBJ_DATA;

/* Backward-compatibility alias — existing call-sites that reference
 * CHANNEL_DEFINITION continue to work without changes.              */
typedef CHANNEL_DEF_DATA CHANNEL_DEFINITION;

typedef struct channel_history_entry {
    char report_id[64];
    char reports_json[256];
    char channel_id[32];
    char sender_name[64];
    char recipient_name[64];
    char message_text[1024];
    time_t timestamp;
} CHANNEL_HISTORY_ENTRY;

typedef struct channel_staff_report_entry {
    char report_id[64];
    char queue_name[128];
    char channel_id[32];
    char reason[64];
    char reporter_name[64];
    char detail_text[2048];
    time_t timestamp;
} CHANNEL_STAFF_REPORT_ENTRY;

bool channel_service_init(void);
void channel_service_shutdown(void);
void channel_service_pulse(void);

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text);
bool channel_service_channel_available_for_sender(CHAR_DATA *sender,
                                                  const CHANNEL_DEF_DATA *def);
bool channel_service_send_directed(CHAR_DATA *sender, const char *channel_id,
                                   CHAR_DATA *recipient, const char *raw_text);
bool channel_service_send_room_targeted(CHAR_DATA *sender, const char *channel_id,
                                        CHAR_DATA *target, const char *raw_text);
bool channel_service_send_object_targeted(CHAR_DATA *sender, const char *channel_id,
                                          OBJ_DATA *target, const char *raw_text);
int channel_service_history_recent(CHAR_DATA *viewer,
                                   const char *channel_id,
                                   int limit,
                                   CHANNEL_HISTORY_ENTRY *out_entries,
                                   int out_cap);
bool channel_service_history_by_index(CHAR_DATA *viewer,
                                      const char *channel_id,
                                      int index,
                                      CHANNEL_HISTORY_ENTRY *out_entry);
bool channel_service_history_by_report_id(CHAR_DATA *viewer,
                                          const char *channel_id,
                                          const char *report_id,
                                          CHANNEL_HISTORY_ENTRY *out_entry);
bool channel_service_apply_preference_filters(const char *channel_id,
                                              CHAR_DATA *recipient,
                                              const char *plain_text,
                                              char *out_text,
                                              size_t out_text_sz);
bool channel_service_report_message(CHAR_DATA *reporter,
                                    const char *channel_id,
                                    const char *report_id,
                                    const char *notes);
int channel_service_staff_report_recent(CHANNEL_STAFF_REPORT_ENTRY *out_entries,
                                        int out_cap,
                                        bool include_acknowledged);
int channel_service_staff_report_count(void);
bool channel_service_staff_report_by_id(const char *report_id,
                                        CHANNEL_STAFF_REPORT_ENTRY *out_entry,
                                        bool *out_acknowledged);
bool channel_service_staff_report_by_index(int index,
                                           bool include_acknowledged,
                                           CHANNEL_STAFF_REPORT_ENTRY *out_entry,
                                           bool *out_acknowledged);
bool channel_service_staff_report_ack(const char *report_id,
                                      const char *staff_name);
int channel_service_staff_report_purge_acknowledged(void);
int channel_service_staff_report_trim(int max_entries);
const char *channel_service_backend_name(void);
int channel_service_describe_subscriptions(CHAR_DATA *ch, char *out, size_t out_size);
bool channel_can_deliver_to_descriptor(CHAR_DATA *sender,
                                       DESCRIPTOR_DATA *desc,
                                       long comm_block_flag,
                                       bool honor_quiet,
                                       bool honor_ignore,
                                       bool honor_wizi,
                                       CHAR_DATA **out_victim);

#endif
