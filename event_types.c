#include <string.h>

#include "merc.h"
#include "interp.h"
#include "event_types.h"

static const char *event_alias_for_invasion(const char *argument)
{
    static char arg[MIL];

    if (IS_NULLSTR(argument))
        return "invasion";

    one_argument((char *)argument, arg);
    return IS_NULLSTR(arg) ? "invasion" : arg;
}

void event_legacy_war_command(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || !str_prefix(arg, "statistics") || !str_prefix(arg, "status")) {
        do_function(ch, &do_event, "status");
        return;
    }

    if (!str_prefix(arg, "join")) {
        send_to_char("The legacy war system is deprecated; joining via event system.\n\r", ch);
        do_function(ch, &do_event, "join");
        return;
    }

    if (!str_prefix(arg, "leave")) {
        do_function(ch, &do_event, "leave");
        return;
    }

    send_to_char("Syntax: war <join|statistics>\n\r", ch);
    send_to_char("        event <status|join|leave>\n\r", ch);
}

void event_legacy_autowar_command(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg);

    send_to_char("The legacy 'autowar' system is deprecated.\n\r", ch);
    send_to_char("Use: event start autowar | event stop autowar | event info autowar\n\r", ch);

    if (!str_cmp(arg, "stop")) {
        do_function(ch, &do_event, "stop autowar");
        return;
    }

    if (arg[0] == '\0' || !str_cmp(arg, "show") || !str_cmp(arg, "status") || !str_cmp(arg, "list")) {
        do_function(ch, &do_event, "info autowar");
        return;
    }

    do_function(ch, &do_event, "start autowar");
}

void event_legacy_startinvasion_command(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
        return;

    send_to_char("The legacy 'startinvasion' system is deprecated.\n\r", ch);
    send_to_char("Use: event start <name> (default: invasion)\n\r", ch);
    do_function(ch, &do_event, formatf("start %s", event_alias_for_invasion(argument)));
}

void event_legacy_gq_command(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("The legacy 'gq' system is deprecated.\n\r", ch);
        send_to_char("Use: event list | event info gq | event start gq | event stop gq\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "show") || !str_cmp(arg, "list") || !str_cmp(arg, "info")) {
        do_function(ch, &do_event, "info gq");
        return;
    }

    if (!str_cmp(arg, "on") || !str_cmp(arg, "start")) {
        do_function(ch, &do_event, "start gq");
        return;
    }

    if (!str_cmp(arg, "off") || !str_cmp(arg, "stop")) {
        do_function(ch, &do_event, "stop gq");
        return;
    }

    send_to_char("The legacy 'gq' subcommands are deprecated.\n\r", ch);
    send_to_char("Use: event list | event info gq | event start gq | event stop gq\n\r", ch);
}
