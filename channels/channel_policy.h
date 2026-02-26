#ifndef CHANNEL_POLICY_H
#define CHANNEL_POLICY_H

#include <stdbool.h>

typedef struct char_data CHAR_DATA;

typedef enum channel_tell_policy_block {
	CHANNEL_TELL_POLICY_ALLOW = 0,
	CHANNEL_TELL_POLICY_BLOCK_IGNORE,
	CHANNEL_TELL_POLICY_BLOCK_PREF,
	CHANNEL_TELL_POLICY_BLOCK_QUIET
} CHANNEL_TELL_POLICY_BLOCK;

bool channel_policy_sender_revoked(CHAR_DATA *ch, const char *channel_id);
bool channel_policy_global_revoked(CHAR_DATA *ch);
bool channel_policy_tell_delivery_allowed(CHAR_DATA *sender,
										  CHAR_DATA *recipient,
										  bool include_quiet,
										  CHANNEL_TELL_POLICY_BLOCK *out_block);

#endif