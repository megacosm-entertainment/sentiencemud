/***************************************************************************
 *  channel_moderation.h — Per-channel moderation command surface          *
 *                                                                         *
 *  Moderator commands that apply channel-specific penalties via the       *
 *  existing account/penalty system using PENALTY_CHAN_MUTE and            *
 *  PENALTY_CHAN_WARN.  Enforcement is handled in channel_service_send().  *
 ***************************************************************************/

#ifndef CHANNEL_MODERATION_H
#define CHANNEL_MODERATION_H

typedef struct char_data CHAR_DATA;

void do_chanmute(CHAR_DATA *ch, char *argument);
void do_chanban(CHAR_DATA *ch, char *argument);
void do_chanwarn(CHAR_DATA *ch, char *argument);
void do_chanunmute(CHAR_DATA *ch, char *argument);
void do_chanpenalties(CHAR_DATA *ch, char *argument);

#endif /* CHANNEL_MODERATION_H */
