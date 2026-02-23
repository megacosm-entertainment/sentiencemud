#include <string.h>

#include "../merc.h"
#include "../account/penalty.h"
#include "../account/preferences.h"
#include "channel_policy.h"

static bool channel_policy_is_tell_family(const char *channel_id)
{
    if (IS_NULLSTR(channel_id))
        return false;

    return !str_cmp(channel_id, "tell") || !str_cmp(channel_id, "gtell");
}

bool channel_policy_sender_revoked(CHAR_DATA *ch, const char *channel_id)
{
    ACCOUNT_DATA *account;

    if (!ch)
        return true;

    if (IS_NPC(ch))
        return false;

    account = (ch->desc ? ch->desc->account : NULL);

    if (account && !IS_NULLSTR(channel_id)
        && has_channel_penalty(account, channel_id, ch->name))
        return true;

    if (channel_policy_is_tell_family(channel_id)) {
        if (account && has_penalty(account, PENALTY_NOTELL, ch->name))
            return true;

        if (IS_SET(ch->comm, COMM_NOTELL))
            return true;
    }

    return false;
}

bool channel_policy_global_revoked(CHAR_DATA *ch)
{
    ACCOUNT_DATA *account;

    if (!ch)
        return true;

    if (IS_SET(ch->comm, COMM_NOCHANNELS))
        return true;

    if (IS_NPC(ch))
        return false;

    account = (ch->desc ? ch->desc->account : NULL);
    if (account && has_penalty(account, PENALTY_NOCHANNELS, ch->name))
        return true;

    return false;
}

bool channel_policy_tell_delivery_allowed(CHAR_DATA *sender,
                                          CHAR_DATA *recipient,
                                          bool include_quiet,
                                          CHANNEL_TELL_POLICY_BLOCK *out_block)
{
    bool bypass_ignore_notells;

    if (out_block)
        *out_block = CHANNEL_TELL_POLICY_ALLOW;

    if (!sender || !recipient) {
        if (out_block)
            *out_block = CHANNEL_TELL_POLICY_BLOCK_PREF;
        return false;
    }

    bypass_ignore_notells = (!IS_NPC(sender) && IS_IMMORTAL(sender)
                             && sender->tot_level >= recipient->tot_level);

    if (!IS_NPC(recipient) && !IS_NPC(sender)
        && is_ignoring(recipient, sender)
        && !bypass_ignore_notells)
    {
        if (out_block)
            *out_block = CHANNEL_TELL_POLICY_BLOCK_IGNORE;
        return false;
    }

    if (!pref_check_channel(recipient, "tells") && !bypass_ignore_notells) {
        if (out_block)
            *out_block = CHANNEL_TELL_POLICY_BLOCK_PREF;
        return false;
    }

    if (include_quiet
        && IS_SET(recipient->comm, COMM_QUIET)
        && !IS_IMMORTAL(sender)
        && !bypass_ignore_notells
        && !can_tell_while_quiet(sender, recipient))
    {
        if (out_block)
            *out_block = CHANNEL_TELL_POLICY_BLOCK_QUIET;
        return false;
    }

    return true;
}