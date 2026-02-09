/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "db.h"
#include "olc.h"
#include "tables.h"
#include "wilds.h"
#include "io/json/json_church.h"

bool is_trusted(CHURCH_PLAYER_DATA *member, char *command);
char *get_chrank(CHURCH_PLAYER_DATA *member);
char *get_chsize_from_number(int size);
char *time_for_log(void);
void church_log(CHURCH_DATA *church, char *string);
void do_chadvance(CHAR_DATA *ch, char *argument);
void do_chcolour(CHAR_DATA *ch, char *argument);
void do_chconvert(CHAR_DATA *ch, char *argument);
void do_chdeduct(CHAR_DATA *ch, char *argument);
void do_chdelete(CHAR_DATA * ch, char *argument);
void do_chdeposit(CHAR_DATA *ch, char *argument);
void do_chdonate(CHAR_DATA *ch, char *argument);
void do_chexcommunicate(CHAR_DATA * ch, char *argument);
void do_chflag(CHAR_DATA *ch, char *argument);
void do_chinfo(CHAR_DATA *ch, char *argument);
void do_chlog(CHAR_DATA *ch, char *argument);
void do_chmotd(CHAR_DATA * ch, char *argument);
void do_choverthrow(CHAR_DATA *ch, char *argument);
void do_chrules(CHAR_DATA * ch, char *argument);
void do_chtoggle(CHAR_DATA * ch, char *argument);
void do_chtransfer(CHAR_DATA *ch, char *argument);
void do_chtreasure(CHAR_DATA *ch, char *argument);
void do_chtrust(CHAR_DATA *ch, char *argument);
void do_churchset(CHAR_DATA *ch, char *argument);
void do_chwhere(CHAR_DATA * ch, char *argument);
void do_chwithdraw(CHAR_DATA *ch, char *argument);
void show_chlist_to_char(CHAR_DATA *ch);
void show_church_commands(CHAR_DATA *ch);
void show_church_info(CHURCH_DATA *church, CHAR_DATA *ch);
void write_churches_new();
void read_churches_new();
void write_church(CHURCH_DATA *church, FILE *fp);
void write_church_member(CHURCH_PLAYER_DATA *member, FILE *fp);
void add_church_to_list(CHURCH_DATA *church, CHURCH_DATA *list);
bool is_excommunicated(CHAR_DATA *ch);
void get_church_id(CHURCH_DATA *church);
void variable_dynamic_fix_church(CHURCH_DATA *church);

void do_chpermission(CHAR_DATA *ch, char *argument);
void do_chranks(CHAR_DATA *ch, char *argument);
void do_chsetrank(CHAR_DATA *ch, char *argument); 
void do_chdefaultrank(CHAR_DATA *ch, char *argument);
bool has_church_permission(CHURCH_PLAYER_DATA *member, long permission);
bool is_church_leader(CHAR_DATA *ch, CHURCH_DATA *church);
bool is_church_officer(CHAR_DATA *ch, CHURCH_DATA *church);

bool remove_church_rank(CHURCH_DATA *church, CHURCH_RANK_DATA *rank);
void assign_church_member_ranks(CHURCH_DATA *church);
char *get_default_legacy_rank_name(CHURCH_DATA *church, int rank, int sex);
void convert_church_ranks(CHURCH_DATA *church);
void upgrade_church_ranks(CHURCH_DATA *church);
CHURCH_RANK_DATA *new_church_rank(void);
void free_church_rank(CHURCH_RANK_DATA *rank);
CHURCH_TREASURE_ROOM *create_church_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room, bool is_default);
bool can_access_treasure_room(CHURCH_PLAYER_DATA *member, CHURCH_TREASURE_ROOM *treasure);
bool add_rank_to_treasure_room(CHURCH_TREASURE_ROOM *treasure, CHURCH_RANK_DATA *rank);
bool remove_rank_from_treasure_room(CHURCH_TREASURE_ROOM *treasure, CHURCH_RANK_DATA *rank);
void do_chsetmemberrank(CHAR_DATA *ch, char *argument);
bool is_church_owner(CHAR_DATA *ch, CHURCH_DATA *church);
bool can_modify_church_member(CHAR_DATA *ch, CHURCH_PLAYER_DATA *target);
void do_chuserperm(CHAR_DATA *ch, char *argument);
void do_chupgrade(CHAR_DATA *ch, char *argument);
void show_church_ranks(CHAR_DATA *ch);
void handle_rank_add(CHAR_DATA *ch, char *name, char *type_str, char *argument);
void handle_rank_remove(CHAR_DATA *ch, char *rank_str);
void handle_rank_type(CHAR_DATA *ch, char *rank_str, char *type_str);
void handle_rank_name(CHAR_DATA *ch, char *rank_str, char *gender_str, char *name);
void handle_rank_rename(CHAR_DATA *ch, char *rank_str, char *name);
void do_chrank(CHAR_DATA *ch, char *argument);
CHURCH_RANK_DATA *find_rank_by_name(CHURCH_DATA *church, const char *name);
long new_church_rank_uid(CHURCH_DATA *church);
bool has_processed_file(const char *filename);
void add_to_processed_files(const char *filename);
void add_church_log_entry(CHURCH_DATA *church, char *author, char *text, flag_t categories, bool system_generated);
void chtoggle_complete(CHAR_DATA *ch, bool enable_pk);
bool is_meta_category(flag_t category_flag);
void display_church_logs(CHAR_DATA *ch, CHURCH_DATA *church, 
                        CHURCH_LOG_ENTRY **entries, int count,
                        char *search_text, char *search_author, flag_t search_categories);
static int cmp_church_uid(void *a, void *b);


/**
 * MAX_PROCESSED_FILES - Limit for tracking already-processed log files
 *
 * Used to prevent re-reading the same legacy church log files during
 * data migration.
 */
    #define MAX_PROCESSED_FILES 100
    char *processed_files[MAX_PROCESSED_FILES];
    int num_processed_files = 0;

/* Church commands 
const struct church_command_type church_command_table[] =
{
    { "create",			CHURCH_RANK_NONE,	do_chcreate			},
    { "info",			CHURCH_RANK_NONE,	do_chinfo			},
    { "list",			CHURCH_RANK_NONE,	do_chlist			},

    { "deposit",		CHURCH_RANK_A,		do_chdeposit		},
    { "donate",			CHURCH_RANK_A, 		do_chdonate			},
    { "gohall",			CHURCH_RANK_A,		do_chgohall			},
    { "motd",			CHURCH_RANK_A,		do_chmotd		 	},
    { "quit",			CHURCH_RANK_A,		do_chrem			},
    { "rules",			CHURCH_RANK_A,		do_chrules 			},
    { "talk",			CHURCH_RANK_A, 		do_chtalk			},
    { "treasure",		CHURCH_RANK_A,		do_chtreasure		},
    { "where",			CHURCH_RANK_A,		do_chwhere 			},

    { "balance",		CHURCH_RANK_B,		do_chbalance 		},
    { "withdraw",		CHURCH_RANK_B,		do_chwithdraw 		},

    { "add",			CHURCH_RANK_D, 		do_chadd			},
    { "colour",			CHURCH_RANK_D, 		do_chcolour			},
    { "convert",		CHURCH_RANK_D,		do_chconvert 		},
    { "delmember",		CHURCH_RANK_D,		do_chrem			},
    { "demote",			CHURCH_RANK_D,		do_chdem			},
    { "excommunicate",	CHURCH_RANK_D,		do_chexcommunicate 	},
    { "overthrow",		CHURCH_RANK_D, 		do_choverthrow		},
    { "promote",		CHURCH_RANK_D, 		do_chprom			},
    { "set",			CHURCH_RANK_D, 		do_churchset		},
    { "setflag",		CHURCH_RANK_D, 		do_chflag			},
    { "toggle",			CHURCH_RANK_D, 		do_chtoggle			},
    { "transfer",		CHURCH_RANK_D, 		do_chtransfer		},
    { "trust",			CHURCH_RANK_D, 		do_chtrust			},

     Immortal commands
    { "delete",			CHURCH_RANK_IMM,	do_chdelete 		},
    { "advance",		CHURCH_RANK_IMM, 	do_chadvance		},
    { "deduct",			CHURCH_RANK_IMM,	do_chdeduct 		},

    { NULL,				-1,					NULL				}
};
*/

/**
 * church_command_table - Dispatch table for all church subcommands
 *
 * Maps command names to their handler functions and permission requirements.
 * Each entry specifies:
 * - command: Subcommand name (e.g., "create", "deposit")
 * - permission: Required CHURCH_PERM_* flags (0 = no permission needed)
 * - function: Handler function pointer
 * - membership: true if must be in a church to use
 * - admin: true if immortal-only command
 *
 * Commands are grouped by access level:
 * - Public: create, info, list (no church needed)
 * - Member: deposit, donate, gohall, talk, treasure, etc.
 * - Officer: add, colour, convert, delmember, permission, rank, etc.
 * - Admin: delete, advance, deduct (immortal only)
 */
const struct church_command_type church_command_table[] =
{
    /* Anyone can run these */
    { "create",         CHURCH_PERM_NONE,   do_chcreate, false        },
    { "info",           CHURCH_PERM_NONE,   do_chinfo, false          },
    { "list",           CHURCH_PERM_NONE,   do_chlist, false          },

    /* Need to be in a church for these */
    { "deposit",     CHURCH_PERM_NONE,   do_chdeposit, true       },
    { "donate",         CHURCH_PERM_NONE,   do_chdonate, true        },
    { "gohall",         CHURCH_PERM_GOHALL, do_chgohall, true        },
    { "motd",           CHURCH_PERM_NONE,   do_chmotd, true          },
    { "quit",           CHURCH_PERM_NONE,   do_chrem, true           },
    { "rules",          CHURCH_PERM_NONE,  do_chrules, true         },
    { "talk",           CHURCH_PERM_TALK,   do_chtalk, true          },
    { "treasure",       CHURCH_PERM_TREASURE,   do_chtreasure, true      },
    { "where",          CHURCH_PERM_NONE,   do_chwhere, true         },
    { "withdraw",       CHURCH_PERM_WITHDRAW|CHURCH_PERM_FINANCES,   do_chwithdraw, true      },
    { "balance",      CHURCH_PERM_BALANCE|CHURCH_PERM_FINANCES,   do_chbalance, true       },
    { "add",        CHURCH_PERM_ADD,   do_chadd, true           },
    { "colour",         CHURCH_PERM_MANAGE,   do_chcolour, true        },
    { "convert",        CHURCH_PERM_MANAGE,   do_chconvert, true       },
    { "delmember",      CHURCH_PERM_REMOVE, do_chrem, true           },
    { "excommunicate",  CHURCH_PERM_MEMBERS,   do_chexcommunicate, true},
    { "overthrow",      CHURCH_PERM_MANAGE,   do_choverthrow, true     },
    { "permission",     CHURCH_PERM_PERMS,   do_chpermission, true    },
    { "rank",          CHURCH_PERM_RANKS,   do_chrank, true         },
    { "set",            CHURCH_PERM_MANAGE,   do_churchset, true       },
    { "setflag",        CHURCH_PERM_MANAGE,   do_chflag, true          },
    { "setrank",        CHURCH_PERM_MEMBERS,   do_chsetmemberrank, true       },
    { "toggle",         CHURCH_PERM_MANAGE,   do_chtoggle, true        },
    { "transfer",       CHURCH_PERM_FINANCES,   do_chtransfer, true      },
    { "trust",          CHURCH_PERM_MANAGE,   do_chtrust, true         },
    { "upgrade",        CHURCH_PERM_MANAGE,   do_chupgrade, true       },
    { "defaultrank",     CHURCH_PERM_MANAGE,   do_chdefaultrank, true   },
    { "log",             CHURCH_PERM_VIEWLOG,    do_chlog, true },



    {"delete",        CHURCH_PERM_NONE,   do_chdelete, false, true        },
    {"advance",        CHURCH_PERM_NONE,   do_chadvance, false, true       },
    {"deduct",         CHURCH_PERM_NONE,   do_chdeduct, false, true        },
    { NULL, -1, NULL, false }
};

/**
 * lookup_church_command - Find a command name in the church command table
 *
 * @param string  Command name to look up
 * @return        Exact command name if found, NULL otherwise
 */
char *lookup_church_command (char *string)
{
    int i;

    i = 0;
    while (church_command_table[i].command != NULL)
    {
    if (!str_cmp(church_command_table[i].command, string))
        return church_command_table[i].command;

    i++;
    }

    return NULL;
}


/**
 * church_get_min_positions - Calculate minimum member slots for a church size
 *
 * Returns the minimum number of member positions allowed for a given
 * church size tier. Formula: POSITIONS = (11 * SIZE + 19) / 3
 *
 * Results by size:
 * - BAND (1):   10 positions
 * - CULT (2):   13 positions
 * - ORDER (3):  17 positions
 * - CHURCH (4): 21 positions
 *
 * @param size  Church size tier (CHURCH_SIZE_*)
 * @return      Minimum position count
 */
int church_get_min_positions(int size)
{
    // (SIZE-1)*(21-10)/(4-1) = (POSITIONS - 10)
    // POSITIONS = (11 * SIZE + 19) / 3

    // BAND(1) = 10
    // CULT(2) = 13
    // ORDER(3) = 17
    // CHURCH(4) = 21

    return (11 * size + 19) / 3;
}


/**
 * show_church_commands - Display available church commands to a player
 *
 * Lists all church commands the player has permission to use.
 * Considers immortal status, church membership, and permission flags.
 * Formats output in 4 columns.
 *
 * @param ch  Character to show commands to
 */
void show_church_commands(CHAR_DATA *ch)
{
    char buf[MSL];
    char buf2[MSL];
    int i, shown = 0;

    sprintf(buf, "Church Commands:\n\r");
    for (i = 0; church_command_table[i].command != NULL; i++)
    {
        bool can_use = false;

        // Admin-only commands: only visible to staff
        if (church_command_table[i].admin == true) {
            if (IS_IMMORTAL(ch))
                can_use = true;
        }
        // Commands available to everyone (no permission required, not admin)
        else if (church_command_table[i].permission == CHURCH_PERM_NONE)
            can_use = true;
        // Commands requiring church membership and permissions
        else if (ch->church_member != NULL &&
                 has_church_permission(ch->church_member, church_command_table[i].permission))
            can_use = true;

        if (can_use)
        {
            sprintf(buf2, " %-13s", church_command_table[i].command);
            strcat(buf, buf2);

            if (++shown % 4 == 0)
                strcat(buf, "\n\r");
        }
    }

    if (shown % 4 != 0)
        strcat(buf, "\n\r");

    send_to_char(buf, ch);
}


/**
 * do_church - Main church command dispatcher
 *
 * Entry point for all church-related commands. Parses the subcommand
 * and dispatches to the appropriate handler from church_command_table.
 *
 * Validates:
 * - Command exists in table
 * - Player has required church membership (if membership=true)
 * - Player has required permissions
 * - Player is not excommunicated (limited commands if so)
 *
 * @param ch        Character using the church command
 * @param argument  Subcommand and arguments
 */
void do_church(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    int i;

    argument = one_argument(argument, arg);

    if (lookup_church_command(arg) == NULL)
    {
        if (arg[0] != '\0')
            send_to_char("There is no such command.\n\r", ch);

        show_church_commands(ch);
        send_to_char("\n\rFor more info see 'help church'\n\r", ch);
        return;
    }

    /* locate command in the table */
    for (i = 0; church_command_table[i].command != NULL; i++)
    {
        if (!str_cmp(church_command_table[i].command, arg))
            break;
    }

    // Check if command requires church membership
    if (church_command_table[i].membership &&
        ch->church == NULL)
    {
        send_to_char("You must be in a church to use that command.\n\r", ch);
        return;
    }

    // Check permission requirements
    if (church_command_table[i].permission != CHURCH_PERM_NONE &&
        ch->church_member != NULL &&
        !has_church_permission(ch->church_member, church_command_table[i].permission) &&
        !IS_IMMORTAL(ch))
    {
        send_to_char("You don't have permission to use that command.\n\r", ch);
        return;
    }

    // Handle excommunicated members
    if (is_excommunicated(ch) &&
        !(!str_cmp(church_command_table[i].command, "list") ||
          !str_cmp(church_command_table[i].command, "rules") ||
          !str_cmp(church_command_table[i].command, "quit")))
    {
        send_to_char("You have been excommunicated from the church.\n\r", ch);
        send_to_char("You may only use the following commands:\n\r", ch);
        send_to_char("LIST   RULES   QUIT\n\r", ch);
        return;
    }

    // Special case for quit
    if (!str_cmp(church_command_table[i].command, "quit"))
        sprintf(argument, "%s", ch->name);

    (church_command_table[i].function)(ch, argument);
}


/**
 * do_chadd - Add a player to the church
 *
 * Adds a target player (present in room) to the caller's church.
 * Must be at a church administration office (ACT2_CHURCHMASTER NPC).
 *
 * Restrictions:
 * - Target must not already be in a church
 * - Target must be a player, not an NPC
 * - Church must not be at max capacity
 * - Target alignment must match church alignment rules
 *
 * New member is assigned the church's default rank.
 * Logs the addition and announces globally.
 *
 * @param ch        Church officer adding the member
 * @param argument  Name of player to add
 */
void do_chadd(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    CHAR_DATA *target;
    CHURCH_PLAYER_DATA *member;
    CHURCH_PLAYER_DATA *new_member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    bool found = false;
    int i;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("For use on CHURCH ADD:\n\rHelp Church\n\r", ch);
    return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
     temp_char = temp_char->next_in_room)
    {
    if (IS_NPC(temp_char)
    && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
            found = true;
    }

    if (!found)
    {
    send_to_char("You must be at an administration office.\n\r", ch);
    return;
    }

    if ((target = get_char_room(ch, NULL, arg)) == NULL)
    {
    send_to_char("They aren't here.\n\r", ch);
    return;
    }

    if (ch->church == NULL)
    {
    send_to_char("You aren't in a registered group.\n\r", ch);
    return;
    }

    if (target->church != NULL)
    {
    send_to_char("That person is already in a registered group.\n\r", ch);
    return;
    }

    if (IS_NPC(target))
    {
    send_to_char("You may only add players to your church.\n\r", ch);
    return;
    }

    if (ch->church->alignment == CHURCH_EVIL
    && target->alignment > 0)
    {
    send_to_char
    ("Only evil and neutral races can join an evil aligned group.\n\r", ch);
    return;
    }

    if (ch->church->alignment == CHURCH_GOOD
    && target->alignment < 0)
    {
    send_to_char
        ("Only benevolent and neutral races can join a good aligned group.\n\r",
         ch);
    return;
    }

    i = 0;
    for (member = ch->church->people; member != NULL; member = member->next)
        i++;

    if (i >= ch->church->max_positions)
    {
    send_to_char("Your group is already full.\n\r", ch);
    return;
    }

    new_member = new_church_player();
    new_member->ch = target;
    new_member->name = str_dup(target->name);
    new_member->rank = ch->church->default_rank;
    new_member->church = ch->church;
    new_member->sex = target->sex;
    new_member->alignment = target->alignment;

    new_member->next = ch->church->people;
    ch->church->people = new_member;

    list_addlink(ch->church->online_players, target);
    list_addlink(ch->church->roster, target);

    target->church = ch->church;
    target->church_member = new_member;
    target->church_name = str_dup(ch->church->name);

    sprintf(buf, "{YYou have joined %s.{x\n\r", ch->church->name);
    send_to_char(buf, target);
    sprintf(buf, "{YYou have added %s.{x\n\r", target->name);
    send_to_char(buf, ch);
    sprintf(buf, "{Y[%s has joined %s]{x\n\r", target->name, ch->church->name);
    gecho(buf);

    sprintf(buf, "%s adds %s.", ch->name, target->name);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_MEMBERS, true);

    save_church(ch->church);
}


/**
 * do_chrules - View or edit church rules
 *
 * With no argument: Displays the church's rules text.
 * With "edit": Opens the string editor to modify rules.
 *
 * @param ch        Church member viewing/editing rules
 * @param argument  "edit" to modify, or empty to view
 */
void do_chrules(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }

    output = new_buf();

    if (ch->church->rules == NULL)
    {
        send_to_char("No rules have been set yet.\n\r", ch);
        return;
    }

    add_buf(output, ch->church->rules);

    page_to_char(buf_string(output), ch);

    free_buf(output);
    return;
    }

    if (!str_cmp(arg, "edit"))
    {
        // After successful edit
        string_append(ch, &ch->church->rules);
        
        // Add log entry
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "%s edited the church rules.", ch->name);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
        
        save_church(ch->church);
        return;
    }

    send_to_char("Eh?\n\r", ch);
}


/**
 * do_chmotd - View or edit church Message of the Day
 *
 * With no argument: Displays the church's MOTD.
 * With "edit": Opens the string editor to modify MOTD.
 *
 * @param ch        Church member viewing/editing MOTD
 * @param argument  "edit" to modify, or empty to view
 */
void do_chmotd(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }

    output = new_buf();

    if (ch->church->motd == NULL)
    {
        send_to_char("No motd has been set yet.\n\r", ch);
        return;
    }

    add_buf(output, ch->church->motd);

    page_to_char(buf_string(output), ch);

    free_buf(output);
    return;
    }

    if (!str_cmp(arg, "edit"))
    {
        // After successful edit
        string_append(ch, &ch->church->motd);
        
        // Add log entry
        char buf[MAX_STRING_LENGTH];
        sprintf(buf, "%s edited the church MOTD.", ch->name);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
        
        save_church(ch->church);
        return;
    }

    send_to_char("Syntax:\n\rCHURCH MOTD\n\rCHURCH MOTD EDIT\n\r", ch);
}


/**
 * do_chrem - Remove a member from the church
 *
 * Removes a member (or self) from the church. Immortals can remove
 * from any church. Regular members can only remove themselves or
 * (with CHURCH_PERM_MEMBERS) other members they outrank.
 *
 * Special handling:
 * - Founder/owner leaving causes church disbandment (prompts confirmation)
 * - Self-removal prompts for confirmation
 * - Cannot remove members that outrank you
 *
 * @param ch        Player removing member
 * @param argument  Member name, "self", or "me"
 */
void do_chrem(CHAR_DATA *ch, char *argument)
{
    CHURCH_PLAYER_DATA *member;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    bool found;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("Remove whom?\n\r", ch);
    return;
    }

    member = NULL;

    if (IS_IMMORTAL(ch))
    {
    CHURCH_DATA *church;
    found = false;
    ITERATOR it;
    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
    {
        for (member = church->people; member != NULL; member = member->next)
        {
            if (!str_prefix(member->name, arg))
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    iterator_stop(&it);

        if (!found)
        {
            send_to_char("Member not found.\n\r", ch);
            return;
        }

        if (!str_cmp(ch->name, member->name))
        {
            act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }
        else
        {
            sprintf(buf, "{Y[You removed %s from %s]{x", member->name, church->name);
            act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

            if (member->ch != NULL)
                act("{YYou have been removed by $N.{x", member->ch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }

        remove_member(member);
    }
    else
    {
        if (ch->church == NULL)
        {
            send_to_char("You aren't in a registered group.\n\r", ch);
            return;
        }

        found = false;
        for (member = ch->church->people; member != NULL; member = member->next)
        {
            if (!str_prefix(member->name, arg) ||
                (!str_cmp(member->name, ch->name) &&
                    (!str_cmp(arg, "self") || !str_cmp(arg, "me"))))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            send_to_char("Member not found.\n\r", ch);
            return;
        }

        if (!IS_IMMORTAL(ch) &&
            !has_church_permission(ch->church_member, CHURCH_PERM_MEMBERS) &&
            str_cmp(arg, ch->name) && str_cmp(arg, "self") &&
            str_cmp(arg, "me"))
        {
            act("Only a leader may remove members.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

    // With this:
    if (!can_modify_church_member(ch, member))
    {
        send_to_char("You don't have permission to remove that member.\n\r", ch);
        return;
    }
    
    // Also, for the special case where the owner is leaving:
    if ((!str_cmp(ch->name, arg) || !str_cmp(arg, "self") || !str_cmp(arg, "me")) &&
        is_church_owner(ch, ch->church))
    {
        send_to_char("{RWarning: {xIf you leave your church it will be disbanded.\n\r", ch);
        send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
        ch->remove_question = member;
    }

        if ((!str_cmp(ch->name, arg) || !str_cmp(arg, "self") || !str_cmp(arg, "me")) &&
            !str_cmp(member->church->founder, ch->name))
        {
            send_to_char("{RWarning: {xIf you leave your church it will be disbanded.\n\r", ch);
            send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
            ch->remove_question = member;
        }
        else if (!str_cmp(ch->name, arg) || !str_cmp(arg, "self") || !str_cmp(arg, "me"))
        {
            send_to_char("{RWarning: {xIf you leave this church you will be shunned by the gods.\n\r", ch);
            send_to_char("You will NOT lose all deity points and ALL pneuma.\n\r", ch);
            send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
            ch->remove_question = member;
        }
        else
        {
            sprintf(buf, "{YYou have removed %s.{x", member->name);
            act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            sprintf(buf, "{Y[%s has been removed from %s]{x\n\r", member->name, ch->church->name);
            gecho(buf);

            sprintf(buf, "%s removes %s.", ch->name, member->name);
            add_church_log_entry(ch->church, ch->name, buf, CHLOG_MEMBERS, true);

            if (member->ch != NULL)
                act("{YYou have been removed by $N.{x", member->ch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            remove_member(member);
        }
    }

    save_church(ch->church);
    return;
}


/**
 * remove_member - Internal function to remove a member from church data
 *
 * Unlinks the member from the church's people list, clears the
 * character's church pointers, removes from online_players and roster
 * lists, and frees the CHURCH_PLAYER_DATA structure.
 *
 * @param member  Member data to remove and free
 */
void remove_member(CHURCH_PLAYER_DATA * member)
{
    CHURCH_PLAYER_DATA *prev_member;
    CHURCH_PLAYER_DATA *member2;
    bool found;
    char buf[MAX_STRING_LENGTH];

    if (member->church == NULL)
    {
        sprintf(buf, "remove_member: ch with null church %s",
            member->name);
    return;
    }

    found = false;
    prev_member = NULL;
    for (member2 = member->church->people; member2 != NULL;
     prev_member = member2, member2 = member2->next)
    {
    if (member2 == member)
    {
        found = true;
        break;
    }
    }

    if (!found)
    return;

    if (prev_member != NULL)
    prev_member->next = member->next;
    else
    member->church->people = member->next;

    if (member->ch != NULL)
    {
    member->ch->church = NULL;
    free_string(member->ch->church_name);

    member->ch->church_member = NULL;
        list_remlink(member->church->online_players, member->ch, false);
    }

       list_remlink(member->church->roster, member->name, false);

    free_church_player(member);
}


/**
 * do_chprom - Deprecated promote command
 *
 * Displays message directing users to use 'church setmemberrank' instead.
 */
void do_chprom(CHAR_DATA *ch, char *argument)
{
    send_to_char("The promote command has been replaced by 'church setmemberrank'.\n\r", ch);
    send_to_char("Syntax: church setmemberrank <character> <rank#>\n\r", ch);
}


/**
 * do_chdem - Deprecated demote command
 *
 * Displays message directing users to use 'church setmemberrank' instead.
 */
void do_chdem(CHAR_DATA *ch, char *argument)
{
    send_to_char("The demote command has been replaced by 'church setmemberrank'.\n\r", ch);
    send_to_char("Syntax: church setmemberrank <character> <rank#>\n\r", ch);
}


/**
 * do_chgohall - Teleport to church hall/recall point
 *
 * Transports the member to the church's designated recall point.
 * Only available for churches of CULT size or larger.
 *
 * Cross-zone travel (between continents) costs pneuma and deity points
 * from the church treasury, and can be disabled by leadership.
 *
 * Restrictions:
 * - Must not be fighting, dead, sleeping, cursed
 * - Must not be in wilderness (water blocks the magic)
 * - Must not have no_recall timer
 * - Church must have a valid recall point set
 *
 * @param ch        Member teleporting
 * @param argument  Unused
 */
void do_chgohall(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    long pneuma_cost;
    long dp_cost;
    char buf[MSL];

    if (ch->church == NULL)
    {
    send_to_char("You must be in a registered group.\n\r", ch);
    return;
    }

    if (is_dead(ch))
    return;

    if (ch->church->size < CHURCH_SIZE_CULT)
    {
    send_to_char("Your group does not have a temple.\n\r", ch);
    return;
    }

    if (PULLING_CART(ch))
    {
    send_to_char("You must first drop what you are pulling.\n\r", ch);
    return;
    }

    if (IS_AFFECTED(ch, AFF_CURSE))
    {
    send_to_char("The curse keeps you where you are.\n\r", ch);
    return;
    }

    if ((location = location_to_room(&ch->church->recall_point)) == NULL)
    {
    send_to_char("You don't have a recall point.\n\r", ch);
    return;
    }

    if (ch->fighting != NULL)
    {
    send_to_char("You are fighting!\n\r", ch);
    return;
    }

    if (IS_DEAD(ch))
    {
        send_to_char("You can't gohall while dead.\n\r", ch);
        return;
    }
    if (ch->position == POS_SLEEPING)
    {
        send_to_char("Wake up first!\n\r", ch);
        return;
    }

    if (IN_WILDERNESS(ch))
    {
        send_to_char("The magic binding you to your church is powerless over water.\n\r", ch);
        return;
    }

    if (ch->no_recall > 0)
    {
        send_to_char("You can't summon enough energy.\n\r", ch);
        return;
    }

    if ( !can_escape(ch) )
        return;

    pneuma_cost = 500;
    dp_cost = 50000;

    /* within areas (non-wilderness) */
    if ((ch->in_room->area->place_flags == PLACE_NOWHERE ||
        ch->in_room->area->place_flags == PLACE_OTHER_PLANE ||
        ch->in_room->area->place_flags == PLACE_ISLAND ||
        !is_same_place(ch->in_room, location)) &&
        !IS_WILDERNESS(ch->in_room))
    {
    if (!IS_SET(ch->church->settings, CHURCH_ALLOW_CROSSZONES) &&
        ch->church_member->rank->rank_type < RANK_TYPE_LEADER  && !has_church_permission(ch->church_member, CHURCH_PERM_GH_CROSS))
    {
        send_to_char("Your church leader has forsaken members from gohalling cross-zone.\n\r", ch);
        return;
    }

        if (ch->church->pneuma < pneuma_cost || ch->church->dp < dp_cost)
        {
            sprintf(buf,
                "It costs %ld pneuma and %ld dp to recall that far.\n\r"
                "Your church doesn't have enough.\n\r", pneuma_cost, dp_cost);
            send_to_char(buf, ch);
            return;
        }

        sprintf(buf, "{RWARNING:{x you are about to recall cross-zone.\n\rThis will cost your church %ld pneuma and %ld karma.\n\r", pneuma_cost, dp_cost);
        send_to_char(buf, ch);

        send_to_char("Are you sure you want to do this? (yes/no)\n\r", ch);

        ch->cross_zone_question = true;
        return;
    }

    if (!str_cmp(ch->in_room->area->name, "Wilderness"))
    {

    if (((location->area->place_flags == PLACE_FIRST_CONTINENT) && get_region(ch->in_room) != REGION_FIRST_CONTINENT) ||
        ((location->area->place_flags == PLACE_SECOND_CONTINENT) && get_region(ch->in_room) != REGION_SECOND_CONTINENT) ||
        ((location->area->place_flags == PLACE_THIRD_CONTINENT) && get_region(ch->in_room) != REGION_THIRD_CONTINENT) ||
        ((location->area->place_flags == PLACE_FOURTH_CONTINENT) && get_region(ch->in_room) != REGION_FOURTH_CONTINENT))
    {
        if (!IS_SET(ch->church->settings, CHURCH_ALLOW_CROSSZONES) && !has_church_permission(ch->church_member, CHURCH_PERM_GH_CROSS))
        {
        send_to_char("Your church leader has forsaken members from gohalling cross-zone.\n\r", ch);
        return;
        }

        if (ch->church->pneuma < pneuma_cost
        || ch->church->dp < dp_cost)
        {
        sprintf(buf,
            "It costs %ld pneuma and %ld dp to recall that far.\n\r"
            "Your church doesn't have enough.\n\r",
            pneuma_cost, dp_cost);
        send_to_char(buf, ch);
        return;
        }

        sprintf(buf,
            "{RWARNING:{x you are about to recall cross-zone.\n\rThis will cost your church %ld pneuma and %ld karma.\n\r", pneuma_cost, dp_cost);
        send_to_char(buf, ch);

        send_to_char("Are you sure you want to do this? (yes/no)\n\r", ch);

        ch->cross_zone_question = true;
        return;
    }
    }

    act("{R$n disappears, leaving a resounding echo of discord.{X", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(ch);
    char_to_room(ch, location_to_room(&ch->church->recall_point));
    act("$n appears in the room.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    do_function(ch, &do_look, "auto");
}


/**
 * do_chflag - Set the church's display flag/tag
 *
 * Changes the short flag/tag displayed with the church name.
 * Must be at a church administration office. Maximum 16 characters
 * (not counting color codes).
 *
 * @param ch        Church officer setting the flag
 * @param argument  New flag text (supports color codes)
 */
void do_chflag(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    char arg1[MIL];
    char buf[MSL];
    bool found = false;

    argument = one_argument_norm(argument, arg1);

    if (ch->church == NULL)
    {
    send_to_char("You must be in a registered group.\n\r", ch);
    return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
     temp_char = temp_char->next_in_room)
    {
    if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
    {
        found = true;
        break;
    }
    }

    if (!found)
    {
    send_to_char("You must be at a group administration office.\n\r", ch);
    return;
    }

    if (arg1[0] == '\0')
    {
    send_to_char("church setflag 'flag'", ch);
    return;
    }

    if (strlen_no_colours(arg1) > 16)
    {
        sprintf(buf, "Sorry %s, that flag is too long.", pers(ch, temp_char));
    do_say(temp_char, buf);
        return;
    }

    act("$n scribbles something down on a piece of parchment.",
    temp_char, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{C$n says 'Very well $N, your flag has now been changed.'{x",
    temp_char, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    if (ch->church->flag != NULL)
        free_string(ch->church->flag);

    smash_tilde(arg1);
    ch->church->flag = str_dup(arg1);
    
    // Add log entry
    sprintf(buf, "%s changed the church flag to '%s'.", ch->name, arg1);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
    save_church(ch->church);
    return;

    
}


/**
 * do_chdeposit - Deposit resources into church treasury
 *
 * Transfers deity points, pneuma, or gold from the player to the
 * church treasury. Must be at a church administration office.
 * Tracks individual member contributions.
 *
 * Syntax: church deposit dp|pneuma|gold <amount>
 *
 * @param ch        Member making the deposit
 * @param argument  Resource type and amount
 */
void do_chdeposit(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg1[MIL];
    char arg2[MAX_STRING_LENGTH];
    bool found;
    int amount;

    if (ch->church == NULL)
    {
    send_to_char(
    "You must be in a registered group to deposit.\n\r", ch);
    return;
    }


    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
     temp_char = temp_char->next_in_room)
    {
    if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
        found = true;
    }

    if (!found)
    {
    send_to_char("You must be at a group administration office.\n\r", ch);
    return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    amount = atoi(arg2);

    if (IS_NULLSTR(arg1) || IS_NULLSTR(arg2)
    || (str_cmp(arg1, "dp")
         && str_cmp(arg1, "pneuma")
         && str_cmp(arg1,"gold")))
    {
        send_to_char("Syntax: church deposit dp|pneuma|gold <amount>\n\r", ch);
    return;
    }

    if (!is_number(arg2))
    {
    send_to_char("You must provide a number.\n\r", ch);
    return;
    }

    if (amount <= 0)
    {
    send_to_char("Invalid amount.\n\r", ch);
    return;
    }

    if ((!str_cmp(arg1, "dp") && amount > ch->deitypoints)
     || (!str_cmp(arg1, "pneuma") && amount > ch->pneuma)
     || (!str_cmp(arg1, "gold") && amount > ch->gold))
    {
    send_to_char("You don't have that much.\n\r", ch);
    return;
    }

    if (!str_cmp(arg1, "pneuma"))
    {
        ch->church->pneuma += amount;
        ch->pneuma -= amount;
    ch->church_member->dep_pneuma += amount;
        sprintf(buf, "{Y[%d pneuma transferred.]{x\n\r", amount);
    }

    if (!str_cmp(arg1, "dp"))
    {
        ch->church->dp += amount;
        ch->deitypoints -= amount;
        ch->church_member->dep_dp += amount;
        sprintf(buf, "{Y[%d deity points transferred.]{x\n\r", amount);
    }

    if (!str_cmp(arg1, "gold"))
    {
        ch->church->gold += amount;
        ch->gold -= amount;
        ch->church_member->dep_gold += amount;
        sprintf(buf, "{Y[%d gold deposited.]{x\n\r", amount);
    }

    send_to_char(buf, ch);

    sprintf(buf, "%s deposited %d %s to the church account.", 
            ch->name, amount, arg1);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_DEPOSIT, true);
    
    save_church(ch->church);
    save_char_obj(ch);
    return;
}


/**
 * do_chbalance - Display church treasury balance
 *
 * Shows the church's current pneuma, karma (deity points), and gold.
 * Must be at a church administration office.
 * Immortals can view any church's balance by number.
 *
 * @param ch        Member checking balance
 * @param argument  Church number (immortal only)
 */
void do_chbalance(CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *temp_char;
    CHURCH_DATA *church;
    char buf[MAX_STRING_LENGTH];
    char arg[MSL];
    bool found = false;

    argument = one_argument(argument, arg);

    if (ch->church == NULL && !IS_IMMORTAL(ch))
    {
    send_to_char("You must be in a registered group.\n\r", ch);
    return;
    }

    if (IS_IMMORTAL(ch))
    {
    if (arg[0] == '\0')
    {
        send_to_char("Syntax: church balance <#>\n\r", ch);
        return;
    }

    if ((church = find_church(atoi(arg))) == NULL)
    {
        send_to_char("Church not found.\n\r", ch);
        return;
    }

    sprintf(buf, "%s has %ld pneuma, %ld karma, and %ld gold.\n\r",
        church->name,
        church->pneuma,
        church->dp,
        church->gold);
    send_to_char(buf, ch);
    return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
     temp_char = temp_char->next_in_room)
    {
    if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
        found = true;
    }

    if (!found)
    {
    send_to_char("You must be at a group administration office.\n\r",
             ch);
    return;
    }

    sprintf(buf,
    "{CErrol says 'You have %ld pneuma, %ld karma, and %ld gold in your account.'{x\n\r",
        ch->church->pneuma, ch->church->dp, ch->church->gold);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
}


/**
 * do_chcreate - Create a new church (Band)
 *
 * Creates a new player organization starting as a Band (smallest size).
 * Requires 3,000,000 deity points and must be at an administration office.
 * Creator becomes the founder and is assigned leader rank.
 *
 * Syntax: church create "name" 'flag' evil|good|neutral
 *
 * The alignment determines which players can join:
 * - evil: only evil/neutral alignment players
 * - good: only good/neutral alignment players
 * - neutral: any alignment
 *
 * @param ch        Player creating the church
 * @param argument  Name, flag, and alignment
 */
void do_chcreate(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *player;
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    bool found;

    if (ch->church != NULL)
    {
    send_to_char(
        "You're in a church already!\n\r", ch);
    return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
     temp_char = temp_char->next_in_room)
    {
        log_string(temp_char->name);
    if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
        found = true;
    }

    if (!found)
    {
    send_to_char("You must be at an administration office.\n\r", ch);
    return;
    }

    if (ch->deitypoints < 3000000)
    {
    send_to_char(
        "You must have 3,000,000 deity points to create a band.\n\r", ch);
    return;
    }

if (list_size(list_churches) >= MAX_CHURCHES) {
    sprintf(buf, "Sentience only allows %d religions.\n\r", MAX_CHURCHES);
    send_to_char(buf, ch);
    return;
}

    argument = one_argument_norm(argument, arg1);
    argument = one_argument_norm(argument, arg2);
    argument = one_argument(argument, arg3);

    if (strlen(arg2) > 25)
    {
        arg2[25] = '\0';
    }

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
    send_to_char("CHURCH CREATE \"name\" 'flag' evil|good|neutral\n\r", ch);
    send_to_char(
    "For information on creating a band:\n\rHelp Church\n\r", ch);
    return;
    }

    if (str_cmp(arg3, "evil")
    && str_cmp(arg3, "good")
    && str_cmp(arg3, "neutral"))
    {
    send_to_char(
    "Your group must be registered as evil, good or neutral.\n\r", ch);
    return;
    }

    church = NULL;
    player = NULL;
    if( (church = new_church()) &&
        (player = new_church_player()) &&
        list_appendlink(church->online_players, ch) &&
        list_appendlink(church->roster, ch->name) &&
        list_appendlink(list_churches,church) ) {


        church->name = str_dup(arg1);
        church->version = VERSION_CHURCH;
        church->max_positions = 10;
        church->pneuma = 0;
        church->size = CHURCH_SIZE_BAND;
        church->flag = str_dup(arg2);
        church->founder_last_login = current_time;
        church->created = current_time;
        church->rules = str_dup("No rules have been set yet.\n\r");
        church->motd = str_dup("No motd has been set yet.\n\r");
        church->founder = str_dup(ch->name);
        church->owner = str_dup(ch->name);

        get_church_id(church);

        if (!str_cmp(arg3, "evil"))
            church->alignment = CHURCH_EVIL;
        else if (!str_cmp(arg3, "good"))
            church->alignment = CHURCH_GOOD;
        else
            church->alignment = CHURCH_NEUTRAL;

        player->next = NULL;
        player->ch = ch;
        player->name = str_dup(ch->name);
        player->church = church;
        player->sex = ch->sex;

        church->people = player;
        ch->church = church;
        ch->church_name = str_dup(church->name);
        ch->church_member = player;

        ch->deitypoints -= 3000000;

        sprintf(buf, "{Y[%s has registered the Band of %s{Y]{x\n\r", ch->name, church->name);
        gecho(buf);

    // After creating the church structure
    initialize_church_ranks(church);
    
    // Set the founder to the leader rank
    CHURCH_RANK_DATA *leader_rank = NULL;
    for (leader_rank = church->ranks; leader_rank; leader_rank = leader_rank->next) {
        if (leader_rank->rank_type == RANK_TYPE_LEADER)
            break;
    }
    
    // Assign the leader rank to the founder
    if (leader_rank) {
        player->rank = leader_rank;
    }

        save_church(ch->church);
    } else {
        if( church ) {
            list_remlink(list_churches, church, false);
            free_church(church);
        }
        if( player ) free_church_player( player);
        send_to_char("The gods do not smile upon you at this moment.", ch);
    }
}


/**
 * do_chdelete - Admin command to delete a church
 *
 * Marks a church as deleted (soft delete) rather than completely removing it.
 * The deleted church is saved to preserve data and removed from active lists.
 * Requires implementor status with security level 9.
 *
 * Syntax: church delete <church_number>
 *
 * @param ch        Staff member deleting the church
 * @param argument  Church number from the list
 */
void do_chdelete(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    int counter = 0;

    argument = one_argument(argument, arg);

    if(IS_NPC(ch) || !IS_IMMORTAL(ch) || (!IS_IMPLEMENTOR(ch) || ch->pcdata->security < 9)) {
        send_to_char("You can't do that.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("This command is used to delete churches.\n\r", ch);
        send_to_char("Usage: church delete <church no.>\n\r", ch);
        send_to_char("Example: church delete 1\n\r", ch);
        send_to_char("\n\rThis would delete church 1.\n\r", ch);
        send_to_char("\n\r", ch);

        show_chlist_to_char(ch);
    }
    else
    {
        CHURCH_DATA *church = NULL;

counter = 0;
ITERATOR it;
iterator_start(&it, list_churches);
while ((church = (CHURCH_DATA *)iterator_nextdata(&it))) {
    counter++;
    if (counter == atoi(arg))
        break;
}
iterator_stop(&it);

if (church == NULL) {
    send_to_char("Church number not found.\n\r", ch);
    return;
}

        // Mark the church as deleted instead of completely removing it
        church->deleted = true;
        
        // Save the church to preserve it with the deleted flag
        save_church(church);
        
        // Now remove from active lists
        list_remlink(list_churches, church, false);
        
// Mark the church as deleted instead of completely removing it
church->deleted = true;

// Save the church to preserve it with the deleted flag
save_church(church);

// Now remove from active lists
list_remlink(list_churches, church, false);

send_to_char("Church marked as deleted.\n\r", ch);

char buf[MAX_STRING_LENGTH];
sprintf(buf, "Church %s (UID %ld) has been deleted by %s.",
        church->name, church->uid, ch->name);
log_string(buf);
return;

        send_to_char("Church marked as deleted.\n\r", ch);
        
        sprintf(buf, "Church %s (UID %ld) has been deleted by %s.",
                church->name, church->uid, ch->name);
        log_string(buf);
        return;
    }
}


/**
 * do_chlist - List all churches or show details for a specific church
 *
 * With no argument: Shows a summary list of all churches.
 * With a church number: Shows detailed member roster for that church.
 *
 * Member list shows:
 * - Online status (asterisk marker)
 * - Name and rank
 * - Excommunicated status
 * - Founder marker
 *
 * @param ch        Character viewing the list
 * @param argument  Church number to show details, or empty for list
 */
void do_chlist(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int counter = 0;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        show_chlist_to_char(ch);
    }
    else
    {
    if ((church = find_church(atoi(arg))) == NULL)
    {
        send_to_char("Group number not found.\n\r", ch);
        return;
    }

    send_to_char("{YThe ", ch);
    if (church->size == CHURCH_SIZE_BAND)
    {
        send_to_char("Band", ch);
    }
    else if (church->size == CHURCH_SIZE_CULT)
    {
        send_to_char("Cult", ch);
    }
    else if (church->size == CHURCH_SIZE_ORDER)
    {
        send_to_char("Order", ch);
    }
    else if (church->size == CHURCH_SIZE_CHURCH)
    {
        send_to_char("Church", ch);
    }

    sprintf(buf, " of %s. %s{x\n\r",
            church->name,
            (IS_IMMORTAL(ch)) ?
            ((char *) ctime(&church->founder_last_login)) :
            "");
    send_to_char(buf, ch);
    send_to_char("{YNo.  Name                  Rank{x\n\r", ch);
    send_to_char("{Y-----------------------------------------{x\n\r", ch);
    counter = 0;
    for (member = church->people; member != NULL; member = member->next)
    {
        bool online = false;
        DESCRIPTOR_DATA *d;

        if (member->ch != NULL && member->ch->desc != NULL)
        {
                CHAR_DATA *wch = NULL;

        d = member->ch->desc;

        if (d->connected == CON_PLAYING)
        {
             wch = (d->original != NULL) ? d->original : d->character;
        }

        if (wch != NULL)
             online = true;
        }

        counter++;
        sprintf(buf, "{G%-3d %s{Y%-21s %-15s %s%s{x\n\r",
            counter,
             online ? "{M*" : " ",
            member->name,
            get_chrank(member),
            IS_SET(member->flags, CHURCH_PLAYER_EXCOMMUNICATED) ? "{RExcommunicated{x"
                 : "",
            !str_cmp(member->name, church->founder) ? "{G[F]{X" : "");
        send_to_char(buf, ch);
    }

    send_to_char("{Y-----------------------------------------{x\n\r",
             ch);
    sprintf(buf, "{Y%d member(s).{x\n\r", counter);
    send_to_char(buf, ch);
    }
}


/**
 * do_chtalk - Church communication channel
 *
 * Sends a message to all online members of the same church.
 * With no argument, toggles the church talk channel on/off (COMM_NOCT).
 *
 * Messages are formatted with church colors and optionally show
 * the sender's personal flag.
 *
 * @param ch        Church member speaking
 * @param argument  Message to send, or empty to toggle channel
 */
void do_chtalk(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];
    int counter;

    counter = 0;

    if (ch->church == NULL)
    {
    send_to_char("You aren't in a church.\n\r", ch);
    return;
    }

    if (argument[0] == '\0')
    {
        if (IS_SET(ch->comm, COMM_NOCT))
    {
        send_to_char("You will now hear church talks.\n\r", ch);
        REMOVE_BIT(ch->comm, COMM_NOCT);
    }
    else
    {
        send_to_char("You will no longer hear church talks.\n\r", ch);
        SET_BIT(ch->comm, COMM_NOCT);
    }

    return;
    }

    if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
    {
        send_to_char("You can't seem to gather enough energy to do it.\n\r", ch);
    return;
    }

    for (d = descriptor_list; d != NULL; d = d->next)
    {
    CHAR_DATA *victim;

    victim = d->original ? d->original : d->character;

    if (d->connected == CON_PLAYING
    && d->character != ch
    && !is_ignoring(d->character, ch)
    && !IS_SET(d->character->comm, COMM_NOCT)
    && d->character->church == ch->church)
    {
        counter++;
        if (!IS_NPC(ch) && ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_CT))
        {
        sprintf(buf, "%s[%s%s%s] says '%s %s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            ch->name,
            ch->church->colour2,
            ch->pcdata->flag,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        }
        else
        {
        sprintf(buf, "%s[%s%s%s] says '%s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            ch->name,
            ch->church->colour2,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        }
        send_to_char(buf, d->character);
    }
    }

    if (ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(ch, FLAG_CT))
    {
    if (counter > 1)
    {
        sprintf(buf, "%s[%s%d%s] people heard you say '%s %s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            counter,
            ch->church->colour2,
            ch->pcdata->flag,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        send_to_char(buf, ch);
    }
    else if (counter == 1)
    {
        sprintf(buf, "%s[%s%d%s] person heard you say '%s %s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            counter,
            ch->church->colour2,
            ch->pcdata->flag,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        send_to_char(buf, ch);
    }
    else
        send_to_char("No one hears your voice.\n\r", ch);
    }
    else
    {
    if (counter > 1)
    {
        sprintf(buf, "%s[%s%d%s] people heard you say '%s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            counter,
            ch->church->colour2,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        send_to_char(buf, ch);
    }
    else if (counter == 1)
    {
        sprintf(buf, "%s[%s%d%s] person heard you say '%s%s%s'{x\n\r",
            ch->church->colour2,
            ch->church->colour1,
            counter,
            ch->church->colour2,
            ch->church->colour1,
            argument,
            ch->church->colour2);
        send_to_char(buf, ch);
    }
    else
        send_to_char("No one hears your voice.\n\r", ch);
    }

}


/**
 * get_chrank - Get the display title for a church member's rank
 *
 * Returns the appropriate gender-specific rank title based on the
 * member's sex field.
 *
 * TODO: This sex-based title system needs to be reworked to use the
 * pronoun system instead. The male/female/neutral titles should be
 * replaced with a single title or pronoun-aware formatting.
 *
 * @param member  Church member to get rank title for
 * @return        Rank title string, or "Unknown" if member is invalid
 */
char *get_chrank(CHURCH_PLAYER_DATA *member)
{
    // Check for null pointers to avoid crashes
    if (member == NULL || member->church == NULL || member->rank == NULL)
        return "Unknown";

    // Return the appropriate gender-specific rank name
    // TODO: Migrate to pronoun system
    if (member->sex == SEX_FEMALE && member->rank->title_female)
        return member->rank->title_female;
    else if (member->sex == SEX_NEUTRAL && member->rank->title_neutral)
        return member->rank->title_neutral;
    else
        return member->rank->title_male;
}


/**
 * do_chexcommunicate - Toggle excommunication status on a church member
 *
 * Excommunicated members remain in the church but lose access to most
 * church commands. They can only use LIST, RULES, and QUIT.
 * Leaders cannot be excommunicated. Toggles the status if already set.
 *
 * @param ch        Church leader excommunicating/restoring a member
 * @param argument  Name of member to excommunicate
 */
void do_chexcommunicate(CHAR_DATA * ch, char *argument)
{
    CHURCH_PLAYER_DATA *member;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char
        ("For use on CHURCH EXCOMMUNICATE: \n\rHelp Church\n\r", ch);
    return;
    }

    for (member = ch->church->people; member != NULL; member = member->next)
    {
    if (!str_cmp(member->name, arg))
        break;
    }

    if (member == NULL)
    {
    send_to_char("That isn't a member of your church.\n\r", ch);
    return;
    }

    if (member->rank->rank_type == RANK_TYPE_LEADER)
    {
        act("You may not excommunicate a church leader.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (!can_modify_church_member(ch, member))
    {
        send_to_char("You may not excommunicate the church owner.\n\r", ch);
        return;
    }

    if (IS_SET(member->flags, CHURCH_PLAYER_EXCOMMUNICATED))
    {
        REMOVE_BIT(member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
        sprintf(buf, "{R%s is no longer excommunicated.{x\n\r", member->name);
        church_echo(ch->church, buf);
        
        // Add log entry
        char log_buf[MAX_STRING_LENGTH];
        sprintf(log_buf, "%s removed excommunication from %s.", ch->name, member->name);
        add_church_log_entry(ch->church, ch->name, log_buf, CHLOG_MEMBERS, true);
        
        return;
    }
    else
    {
        SET_BIT(member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
        sprintf(buf, "{R%s has been excommunicated.{x\n\r", member->name);
        church_echo(ch->church, buf);
        
        // Add log entry
        char log_buf[MAX_STRING_LENGTH];
        sprintf(log_buf, "%s excommunicated %s from the church.", ch->name, member->name);
        add_church_log_entry(ch->church, ch->name, log_buf, CHLOG_MEMBERS, true);
        
        return;
    }
}


/**
 * show_chlist_to_char - Display formatted list of all registered churches
 *
 * Shows a table of all churches with: number, PK status, name, max positions,
 * alignment (Good/Neutral/Evil), and size (Band/Cult/Order/Church).
 * Used by do_chlist and other commands that need to show available churches.
 *
 * @param ch  Character to display the list to
 */
void show_chlist_to_char(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;
    int i;

    send_to_char("{YRegistered Factions within Sentience:{x\n\r", ch);
    send_to_char(
    "{YNo. PK  Name                          Max   Alignment    Size{x\n\r", ch);
    line(ch , 83, NULL, NULL);
    i = 0;
ITERATOR it;
i = 0;
iterator_start(&it, list_churches);
while ((church = (CHURCH_DATA *)iterator_nextdata(&it))) {
    i++;
    sprintf(buf, "{G%-3d{x {R%-3s{x %-30.30s %-4d %-15s{x",
        i,
        church->pk == true ? "PK" : "",
        church->name,
        church->max_positions,
        church->alignment == CHURCH_EVIL ?
        "{REvil" : (church->alignment == CHURCH_GOOD ?
            "{WGood" : "{xNeutral"));
    send_to_char(buf, ch);

    if (church->size == CHURCH_SIZE_BAND)
        send_to_char("Band   ", ch);
    else if (church->size == CHURCH_SIZE_CULT)
        send_to_char("Cult   ", ch);
    else if (church->size == CHURCH_SIZE_ORDER)
        send_to_char("Order  ", ch);
    else if (church->size == CHURCH_SIZE_CHURCH)
        send_to_char("Church ", ch);

    sprintf(buf, "%s\n\r", church->flag);
    send_to_char(buf, ch);
}
iterator_stop(&it);

    line (ch, 83, NULL, NULL);
    sprintf(buf, "{Y%d group(s) found.{x\n\r", i);

    send_to_char(buf, ch);
}


/**
 * msg_church_members - Send a message to all online church members
 *
 * Iterates through all members of the church and sends the given message
 * to those who are currently online (member->ch != NULL).
 *
 * @param church    Church whose members should receive the message
 * @param argument  Message text to send
 */
void msg_church_members(CHURCH_DATA *church, char *argument)
{
    CHURCH_PLAYER_DATA *member;

    if (church == NULL)
        return;

    for (member = church->people; member != NULL; member = member->next)
    {
    if (member->ch != NULL)
        send_to_char(argument, member->ch);
    }
}

/**
 * do_chupgrade - Player command to upgrade church to the next size tier
 *
 * Allows church owners or members with MANAGE permission to upgrade
 * the church size (Band -> Cult -> Order -> Church). Requires:
 * - Being at an administration office (room with ACT2_CHURCHMASTER NPC)
 * - Sufficient deity points and pneuma in church treasury
 *
 * Upgrade costs:
 * - Band to Cult: 5,000,000 DP + 50,000 pneuma
 * - Cult to Order: 10,000,000 DP + 100,000 pneuma
 * - Order to Church: 25,000,000 DP + 250,000 pneuma
 *
 * On success, deducts resources, increases size, updates max_positions,
 * announces globally, and logs the upgrade.
 *
 * @param ch        Character attempting the upgrade
 * @param argument  Unused
 */
void do_chupgrade(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    bool found = false;
    
    if ((church = ch->church) == NULL) {
        send_to_char("You aren't even in a church.\n\r", ch);
        return;
    }
    
    // Check if player has permission to upgrade the church
    if (!is_church_owner(ch, church) && 
        !has_church_permission(ch->church_member, CHURCH_PERM_MANAGE)) {
        send_to_char("You don't have permission to upgrade your church.\n\r", ch);
        return;
    }
    
    // Check if at administration office
    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
         temp_char = temp_char->next_in_room) {
        if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER)) {
            found = true;
            break;
        }
    }

    if (!found) {
        send_to_char("You must be at an administration office.\n\r", ch);
        return;
    }
    
    // Check if church is already at maximum size
    if (church->size >= CHURCH_SIZE_CHURCH) {
        send_to_char("Your church has already reached its maximum size.\n\r", ch);
        return;
    }
    
    // Define upgrade costs based on current size
    long dp_cost = 0;
    long pneuma_cost = 0;
    
    switch(church->size) {
        case CHURCH_SIZE_BAND:  // Band to Cult
            dp_cost = 5000000;
            pneuma_cost = 50000;
            break;
            
        case CHURCH_SIZE_CULT:  // Cult to Order
            dp_cost = 10000000;
            pneuma_cost = 100000;
            break;
            
        case CHURCH_SIZE_ORDER: // Order to Church
            dp_cost = 25000000;
            pneuma_cost = 250000;
            break;
            
        default:
            pbugf(LOG_ERROR, "do_chupgrade: invalid church size");
            return;
    }
    
    // Check if church has enough resources
    if (church->dp < dp_cost || church->pneuma < pneuma_cost) {
        sprintf(buf, "Upgrading from %s to %s requires %ld deity points and %ld pneuma.\n\r"
                     "Your church has %ld deity points and %ld pneuma.\n\r",
                get_chsize_from_number(church->size),
                get_chsize_from_number(church->size + 1),
                dp_cost, pneuma_cost,
                church->dp, church->pneuma);
        send_to_char(buf, ch);
        return;
    }
    
    // All checks passed, upgrade the church
    church->dp -= dp_cost;
    church->pneuma -= pneuma_cost;
    church->size += 1;
    
    // Update their max roster size if necessary
    int max_pos = church_get_min_positions(church->size);
    church->max_positions = UMAX(church->max_positions, max_pos);
    
    // Announce the upgrade
    sprintf(buf, "{Y[%s has upgraded from %s to %s!]{x\n\r",
            church->name, 
            get_chsize_from_number(church->size - 1),
            get_chsize_from_number(church->size));
    gecho(buf);
    
    // Log the upgrade
    sprintf(buf, "%s upgraded the church from %s to %s for %ld DP and %ld pneuma.",
            ch->name,
            get_chsize_from_number(church->size - 1),
            get_chsize_from_number(church->size),
            dp_cost, pneuma_cost);
            add_church_log_entry(church, ch->name, buf, CHLOG_LEADERSHIP, true);
    
    save_church(church);
}

/**
 * do_chadvance - Staff command to advance a church's size tier for free
 *
 * Allows staff with SUPREMACY rank and IMMORTAL_CHURCHES duty to promote
 * a church to the next size tier without requiring resource costs.
 * Shows church list if no argument given.
 *
 * Syntax: church advance <church#>
 *
 * @param ch        Staff member issuing the command
 * @param argument  Church number from the list
 */
void do_chadvance(CHAR_DATA *ch, char* argument)
{
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;

    argument = one_argument(argument, arg);

if (ch->pcdata->staff_rank < STAFF_SUPREMACY ||
    ch->pcdata->immortal == NULL ||
    !IS_SET(ch->pcdata->immortal->duties, IMMORTAL_CHURCHES))    {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("Advance which church?\n\r", ch);
    show_chlist_to_char(ch);
        return;
    }

    if (!is_number(arg))
    {
        send_to_char("That's not even a number!\n\r", ch);
        show_chlist_to_char(ch);
        return;
    }

    if ((church = find_church(atoi (arg))) == NULL)
    {
        send_to_char("No such church.\n\r", ch);
        return;
    }

    if (church->size == CHURCH_SIZE_CHURCH)
    {
        send_to_char("They're already at the maximum level.\n\r", ch);
        return;
    }

    church->size += 1;

    // Update their max roster size if necessary
    int max_pos = church_get_min_positions(church->size);
    church->max_positions = UMAX(church->max_positions, max_pos);

    sprintf(buf, "{Y[%s is now %s %s!]{x\n\r",
        church->name,
        church->size == CHURCH_SIZE_ORDER ? "an" : "a",
    get_chsize_from_number(church->size));

    gecho(buf);

    save_church(ch->church);
}


/**
 * do_chdeduct - Staff command to deduct resources from a church treasury
 *
 * Allows staff with SUPREMACY rank and IMMORTAL_CHURCHES duty to remove
 * pneuma, deity points, or gold from a church's treasury. Notifies all
 * online church members when resources are deducted.
 *
 * Syntax: church deduct <church#> <pneuma|dp|gold> <amount>
 *
 * @param ch        Staff member issuing the command
 * @param argument  Church number, resource type, and amount
 */
void do_chdeduct(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char buf[2*MAX_STRING_LENGTH];
    int amt;
    int i;
    CHURCH_DATA *church;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->pcdata->staff_rank < STAFF_SUPREMACY ||
        ch->pcdata->immortal == NULL ||
        !IS_SET(ch->pcdata->immortal->duties, IMMORTAL_CHURCHES))
    {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
        send_to_char(
        "Syntax:\n\rchurch deduct <church#> <pneuma|dp|gold> <amt>\n\r", ch);
        show_chlist_to_char(ch);
        return;
    }

    i = 0;
    ITERATOR it;
    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
    {
        i++;
        if (i == atoi(arg))
            break;
    }
    iterator_stop(&it);

    if (church == NULL)
    {
        send_to_char("No such church.\n\r", ch);
        return;
    }

    if (str_cmp(arg2, "dp")
        && str_cmp(arg2, "pneuma")
        && str_cmp(arg2, "gold"))
    {
        send_to_char(
        "Syntax:\n\rchurch <church#> <pneuma|dp|gold> <amt>\n\r", ch);
        show_chlist_to_char(ch);
        return;
    }

    amt = atoi(arg3);

    if (amt < 0)
    {
        send_to_char("Invalid amount.\n\r", ch);
        return;
    }

    if ((!str_cmp(arg2,"pneuma") && amt > church->pneuma)
        || (!str_cmp(arg2, "dp") && amt > church->dp)
        || (!str_cmp(arg2, "gold") && amt > church->gold))
    {
        send_to_char("They don't have that much.\n\r", ch);
        return;
    }

    if (!str_cmp(arg2, "pneuma")) church->pneuma -= amt;

    if (!str_cmp(arg2, "dp")) church->dp -= amt;

    if (!str_cmp(arg2, "gold")) church->gold -= amt;

    sprintf(buf,
        "{Y[%s has deducted %d %s from your church.]{x\n\r",
        ch->name, amt, arg2);

    msg_church_members(church, buf);

    sprintf(buf, "You have deducted %d %s from %s.\n\r",
        amt, arg2, church->name);
    send_to_char(buf, ch);

    save_church(ch->church);
}


/**
 * get_chsize_from_number - Convert church size enum to display string
 *
 * @param size  Church size constant (CHURCH_SIZE_BAND, _CULT, _ORDER, _CHURCH)
 * @return      Human-readable size name ("Band", "Cult", "Order", "Church")
 */
char *get_chsize_from_number(int size)
{
    if (size == CHURCH_SIZE_BAND)
        return "Band";
    else if (size == CHURCH_SIZE_CULT)
     return "Cult";
    else if (size == CHURCH_SIZE_ORDER)
     return "Order";
    else
     return "Church";
}


/**
 * do_chinfo - Display or edit church information
 *
 * Multi-purpose command with different behaviors:
 * - No args: Shows own church's member contribution stats (pneuma, karma, gold,
 *   and optionally PK stats). Also shows any relics in treasure rooms.
 * - "edit": Opens string editor to modify church's public info text.
 *   Requires leader rank or MANAGE permission.
 * - <number>: Shows public info for another church (boxed display with
 *   name, optional PK stats, and info text).
 * - Staff with ASCENDANT rank can use show_church_info for detailed view.
 *
 * @param ch        Character viewing/editing info
 * @param argument  "edit", church number, or empty
 */
void do_chinfo(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char arg[MSL];
    char buf[MAX_STRING_LENGTH];
    char buf2[MSL];
    BUFFER *buffer;
    int i;
    int x;
    int box_width;

    argument = one_argument(argument, arg);

    if (arg[0] != '\0')
    {
    /* Edit your church info*/
    if (!str_cmp(arg, "edit"))
    {
        if ((church = ch->church) == NULL || ch->church_member == NULL)
        {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
        }

    if (ch->church_member->rank->rank_type < RANK_TYPE_LEADER
        && !has_church_permission(ch->church_member, CHURCH_PERM_MANAGE))
    {
        send_to_char("You aren't high enough rank to do that.\n\r", ch);
        return;
    }

        string_append(ch, &ch->church->info);
        save_church(ch->church);
        return;
    }

    /* imms can look up info on churches for convenience*/
    if (IS_STAFF(ch, STAFF_ASCENDANT))
    {
        if ((church = find_church(atoi(arg))) == NULL)
        {
        send_to_char("There is no such church.\n\r", ch);
        return;
        }

        show_church_info(church, ch);
        return;
    }

        /* Look up info of another church*/
    if ((church = find_church(atoi(arg))) == NULL)
    {
        send_to_char("There is no such church.\n\r", ch);
        return;
    }

    box_width = 70;

    buffer = new_buf();

        /* Top edge*/
    add_buf(buffer, "{b.");
    for (x = 0; x < box_width; x++) {
        add_buf(buffer, "-");
    }
    add_buf(buffer, ".{x\n\r");

    /* Blank line*/
    add_buf(buffer, "{b|");
    for (x = 0; x < box_width; x++)
        add_buf(buffer, " ");

    add_buf(buffer, "{b|\n\r");

        /* Name*/
    sprintf(buf, "{b|    {WThe %s of %s{x",
        get_chsize_from_number(church->size), church->name);
    for (x = strlen(buf) - 7; x < box_width; x++)
        strcat(buf, " ");

    strcat(buf, "{b|{x\n\r");
    add_buf(buffer, buf);

    add_buf(buffer, "{b|");

    for (x = 0; x < box_width; x++)
        add_buf(buffer, " ");

    add_buf(buffer, "{b|\n\r");

    /* Date created
    sprintf(buf2, "{b|    {xDate Created: %s{x",
        church->created == 0 ? "No Record" : time_string);
    strcat(buf, buf2);

    free_string(time_string);
    */

    /* PK record*/
    if (IS_SET(church->settings, CHURCH_SHOW_PKS))
    {
        sprintf(buf, "{b|    {YPlayer kills:          {x%ld", church->pk_wins);
        for (x = strlen(buf) - 7; x < box_width; x++)
        strcat(buf, " ");

        strcat(buf, "{b|{x\n\r");
        add_buf(buffer, buf);

        sprintf(buf, "{b|    {YChaotic player kills:  {x%ld", church->cpk_wins);
        for (x = strlen(buf) - 7; x < box_width; x++)
        strcat(buf, " ");

        strcat(buf, "{b|{x\n\r");
        add_buf(buffer, buf);

        sprintf(buf, "{b|    {YWars won:              {x%ld", church->wars_won);
        for (x = strlen(buf) - 7; x < box_width; x++)
        strcat(buf, " ");

        strcat(buf, "{b|{x\n\r");
        add_buf(buffer, buf);
    }

    /* Blank line*/
    add_buf(buffer, "{b|");
    for (x = 0; x < box_width; x++)
        add_buf(buffer, " ");

    add_buf(buffer, "{b|\n\r");

    /* Info*/
    sprintf(buf, "{b|    {x");

        for (i = 0, x = 6; church->info[i] != '\0'; i++)
    {
        if (church->info[i] == '\n')
        {
        i++;
        for (; x < box_width + 2; x++)
            strcat(buf, " ");

        strcat(buf, "{b|\n\r");
        strcat(buf, "{b|    {x");
        x = 6;
        continue;
        }

        sprintf(buf2, "%c", church->info[i]);
        strcat(buf, buf2);
        if (church->info[i] != '{'
        &&  !(i > 0 && church->info[i-1] == '{'))
        x++;

        if (x == box_width - 2)
        {
        for (x = 0; x < 4; x++)
            strcat(buf, " ");

        x = 6;
        strcat(buf, "{b|{x\n\r");
        strcat(buf, "{b|    {x");
        }
    }

    for (; x < box_width + 2; x++)
        strcat(buf, " ");

    strcat(buf, "{b|\n\r");

    add_buf(buffer, buf);

    /* Bottom edge*/
    add_buf(buffer, "{b``");
    for (x = 0; x < box_width; x++) {
        add_buf(buffer, "-");
    }
    add_buf(buffer, "'{x\n\r");

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return;
    }
    else
    {
    church = ch->church;
    if (church == NULL)
    {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }
    }

    /* Show own church info*/
    if (IS_SET(church->settings, CHURCH_SHOW_PKS))
    {
    sprintf(buf,
        "{Y #  %-12s %-10s %-10s %-10s %-8s %-8s %-4s{x\n\r",
        "Name", "Pneuma", "Karma", "Gold", "PK", "CPK", "Wars");
    send_to_char(buf, ch);
    line(ch,78, NULL, NULL);
    i = 0;

    for (member = church->people; member != NULL; member = member->next)
    {
        i++;

        sprintf(buf,
        "{Y%2d){x %-12s %-10ld %-10ld %-8ld",
        i,
        member->name,
        member->dep_pneuma,
        member->dep_dp,
        member->dep_gold);

        sprintf(buf2, "%4ld-%-4ld", member->pk_wins, member->pk_losses);
        strcat(buf, buf2);

        sprintf(buf2, "%4ld-%-4ld", member->cpk_wins, member->cpk_losses);
        strcat(buf, buf2);

        sprintf(buf2, "%4ld\n\r", member->wars_won);
        strcat(buf, buf2);

        send_to_char(buf, ch);
    }

    line(ch,78, NULL, NULL);
    }
    else
    {
    sprintf(buf,
        "{Y #  %-12s %-10s %-10s %-10s{x\n\r",
        "Name", "Pneuma", "Karma", "Gold");
    send_to_char(buf, ch);
    line(ch, 55, NULL, NULL);
    i = 0;

    for (member = church->people; member != NULL; member = member->next)
    {
        i++;

        sprintf(buf,
        "{Y%2d){x %-12s %-10ld %-10ld %-8ld\n\r",
        i,
        member->name,
        member->dep_pneuma,
        member->dep_dp,
        member->dep_gold);

        send_to_char(buf, ch);
    }

    line(ch, 55, NULL, NULL);
    }

    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;
    iterator_start(&it, church->treasure_rooms);
    while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
        if( treasure->room != NULL )
        {
            ROOM_INDEX_DATA *room = treasure->room;
            for (OBJ_DATA *obj = room->contents; obj != NULL; obj = obj->next_content)
            {
                if (is_relic(obj->pIndexData))
                {
                    sprintf(buf, "{M*{x Your church is currently in the possession of %s.\n\r", obj->short_descr);
                    send_to_char(buf, ch);
                }
            }
        }
    }
    iterator_stop(&it);
}


/**
 * do_chtransfer - Transfer resources from own church to another church
 *
 * Allows church leaders or members with MANAGE permission to transfer
 * pneuma, deity points (dp), or gold from their church treasury to another.
 * Must be at an administration office (room with ACT2_CHURCHMASTER NPC).
 * Cannot transfer more than the church possesses.
 *
 * Syntax: church transfer <church#> <pneuma|dp|gold> <amount>
 *
 * @param ch        Character initiating the transfer
 * @param argument  Target church number, resource type, and amount
 */
void do_chtransfer(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MIL];
    char arg3[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;
    long amount;
    bool found_admin = false;
    CHAR_DATA *temp_char;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL)
    {
        send_to_char("You aren't even in a church!\n\r", ch);
        return;
    }

    found_admin = false;
    for (temp_char = ch->in_room->people;
          temp_char != NULL;
          temp_char = temp_char->next_in_room)
    {
        if (IS_NPC(temp_char)
        && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
    {
        found_admin = true;
        break;
    }
    }

    if (!found_admin)
    {
        send_to_char(
        "You must be at an administration office.\n\r", ch);
    return;
    }

    if (arg[0]  == '\0'
    ||   arg2[0] == '\0'
    ||   arg3[0] == '\0'
    || !is_number(arg)
    || !is_number(arg3)
    || (str_cmp(arg2, "pneuma")
         && str_cmp(arg2, "dp")
         && str_cmp(arg2, "gold")))
    {
        send_to_char(
            "Syntax: church transfer <#> <pneuma|dp|gold> <amount>\n\r", ch);
    show_chlist_to_char(ch);
    return;
    }

    if ((church = find_church(atoi(arg))) == NULL )
    {
        send_to_char("No church with that number was found.\n\r", ch);
    return;
    }

    if (church == ch->church)
    {
     send_to_char("That would be quite pointless.\n\r", ch);
    return;
    }

    amount = atol(arg3);

    if ((!str_cmp(arg2, "pneuma") && amount > ch->church->pneuma)
    || (!str_cmp(arg2, "dp") && amount > ch->church->dp)
    || (!str_cmp(arg2, "gold") && amount > ch->church->gold))
    {
        send_to_char("You don't have that much.\n\r", ch);
        return;
    }

    if (!str_cmp(arg2, "pneuma"))
    {
        ch->church->pneuma -= amount;
    church->pneuma     += amount;

    sprintf(buf,
        "{Y[%s transferred %ld pneuma into %s's account.]\n\r",
        ch->name, amount, church->name);
    msg_church_members(ch->church, buf);

    sprintf(buf,
    "{Y[%s transferred %ld pneuma from %s into your account.]{x\n\r",
        ch->name, amount, ch->church->name);
    msg_church_members(church, buf);
    return;
    }

    if (!str_cmp(arg2, "dp"))
    {
        ch->church->dp    -= amount;
    church->dp        += amount;

    sprintf(buf,
        "{Y[%s transferred %ld karma into %s's account.]\n\r",
        ch->name, amount, church->name);
    msg_church_members(ch->church, buf);

    sprintf(buf,
        "{Y[%s transferred %ld karma from %s into your account.]{x\n\r",
        ch->name, amount, ch->church->name);
    msg_church_members(church, buf);
    return;
    }

    if (!str_cmp(arg2, "gold"))
    {
        ch->church->gold -= amount;
    church->gold     += amount;

    sprintf(buf,
        "{Y[%s transferred %ld gold into %s's account.]\n\r",
        ch->name, amount, church->name);
        msg_church_members(ch->church, buf);

        sprintf(buf,
        "{Y[%s transferred %ld gold from %s into your account.]{x\n\r",
                ch->name, amount, ch->church->name);
        msg_church_members(church, buf);
        return;
    }

    // After successful transfer, add a detailed log entry
    sprintf(buf, "%s transferred %ld %s to %s.",
        ch->name, amount, arg2, church->name);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_TRANSFER, true);
    
    save_church(ch->church);
}


/**
 * do_chwithdraw - Withdraw resources from church treasury
 *
 * Two modes of operation:
 * 1. Basic: Withdraw to self - any member can withdraw pneuma, dp, or gold
 * 2. Leader: Withdraw to another player - requires leader rank or WITHDRAW perm
 *
 * Must be at an administration office (room with ACT2_CHURCHMASTER NPC).
 * Notifies members with WITHDRAW or FINANCES permission of the transaction.
 *
 * Syntax:
 * - church withdraw <pneuma|dp|gold> <amount>
 * - church withdraw <person> <pneuma|dp|gold> <amount>
 *
 * @param ch        Character withdrawing resources
 * @param argument  Resource type, amount, and optionally target player
 */
void do_chwithdraw(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char buf[2*MAX_STRING_LENGTH];
    CHAR_DATA *victim = NULL;
    CHAR_DATA *mob;
    CHURCH_PLAYER_DATA *member;
    long amt;
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL)
    {
        send_to_char("You aren't even in a church.\n\r", ch);
        return;
    }

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
    if (IS_NPC(mob) && IS_SET(mob->act[1], ACT2_CHURCHMASTER))
    {
        found = true;
        break;
    }
    }

    if (!found)
    {
    send_to_char("You can't do that here.\n\r", ch);
    return;
    }

    if ( arg[0] == '\0'
    ||   arg2[0] == '\0')
    {
        send_to_char(
        "Syntax: church withdraw <pneuma|dp|gold> <amount>\n\r"
        "        church withdraw <person> <pneuma|dp|gold> <amount> (leaders only)\n\r", ch);
    return;
    }

    if (arg3[0] != '\0')
    {
    if (ch->church_member->rank->rank_type < RANK_TYPE_LEADER
        && !has_church_permission(ch->church_member, CHURCH_PERM_WITHDRAW))
    {
        send_to_char("Only a leader may transfer balance to church members.\n\r", ch);
        return;
    }

    if (arg3[0] == '\0'
    || (str_cmp(arg2, "pneuma")
         && str_cmp(arg2, "dp")
         && str_cmp(arg2, "gold")))
    {
        send_to_char("Syntax: church withdraw <person> <pneuma|dp|gold> <amount>\n\r", ch);
        return;
    }

    victim = get_char_room(ch, NULL, arg);
    if (victim == NULL)
    {
        send_to_char("They must be in the same room as you.\n\r", ch);
        return;
    }

    amt = atol(arg3);
    if ((!str_cmp(arg2, "pneuma") && ch->church->pneuma < amt)
    || (!str_cmp(arg2, "dp") && ch->church->dp < amt)
    || (!str_cmp(arg2, "gold") && ch->church->gold < amt))
    {
        sprintf(buf, "Your church doesn't have that much %s.\n\r", arg2);
        send_to_char(buf, ch);
        return;
    }

    if (!str_cmp(arg2, "pneuma"))
    {
        sprintf(buf, "{Y[You transferred %ld pneuma to %s.]{x\n\r",
            amt, victim->name);
        send_to_char(buf, ch);

        sprintf(buf, "{Y[%s has transferred %ld pneuma to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
        send_to_char(buf, victim);

        for (member = ch->church->people; member != NULL;
              member = member->next)
        {
        if (member->ch != NULL && member->ch->desc != NULL
        && (!has_church_permission(ch->church_member, CHURCH_PERM_WITHDRAW) || !has_church_permission(ch->church_member, CHURCH_PERM_FINANCES)) && member->ch != ch
        && member->ch != victim)
        {
            sprintf(buf, "{Y[%s has transferred %ld pneuma to %s.]{x\n\r", ch->name, amt, victim->name);
            send_to_char(buf, member->ch);
        }
        }

        ch->church->pneuma -= amt;
        victim->pneuma += amt;
        return;
    }

    if (!str_cmp(arg2, "dp"))
    {
        sprintf(buf, "{Y[You transferred %ld dp to %s.]{x\n\r",
            amt, victim->name);
        send_to_char(buf, ch);

        sprintf(buf, "{Y[%s has transferred %ld dp to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
        send_to_char(buf, victim);

        for (member = ch->church->people; member != NULL;
              member = member->next)
        {
        if (member->ch != NULL && member->ch->desc != NULL
        && (!has_church_permission(ch->church_member, CHURCH_PERM_WITHDRAW) || !has_church_permission(ch->church_member, CHURCH_PERM_FINANCES)) && member->ch != ch
        && member->ch != victim)
        {
            sprintf(buf, "{Y[%s has transferred %ld dp to %s.]{x\n\r", ch->name, amt, victim->name);
            send_to_char(buf, member->ch);
        }
        }

        ch->church->dp -= amt;
        victim->deitypoints += amt;
        return;
    }

    if (!str_cmp(arg2, "gold"))
    {
        sprintf(buf, "{Y[You transferred %ld gold to %s.]{x\n\r",
            amt, victim->name);
        send_to_char(buf, ch);

        sprintf(buf, "{Y[%s has transferred %ld gold to you from %s.]{x\n\r", ch->name, amt, ch->church->name);
        send_to_char(buf, victim);

        for (member = ch->church->people; member != NULL;
              member = member->next)
        {
    if (ch->church_member->rank->rank_type < RANK_TYPE_LEADER
        && !has_church_permission(ch->church_member, CHURCH_PERM_WITHDRAW))
    {
        send_to_char("Only a leader may transfer balance to church members.\n\r", ch);
        return;
    }
        }

        ch->church->gold -= amt;
        victim->gold += amt;
        return;
    }
    }
    else
    {
    amt = atol(arg2);
    if (amt <= 0)
    {
        send_to_char("That amount is invalid.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "pneuma"))
    {
        if (amt > ch->church_member->dep_pneuma)
        {
        send_to_char(
            "You can't take out more than you put in.\n\r", ch);
        return;
        }

        if (amt > ch->church->pneuma)
        {
        send_to_char(
            "Sorry, there's not that much in the account.\n\r", ch);
        return;
        }

        sprintf(buf, "{Y[%s has withdrawn %ld pneuma.]{x\n\r",
            ch->name, amt);
        msg_church_members(ch->church, buf);

        /*
           if (ch->church->log != NULL)
           new_log = str_dup(ch->church->log);
           else
           new_log = str_dup("");

           free_string(ch->church->log);

           sprintf(new_log, "\n\r[%s] %s withdrew %ld pneuma.",
           (char *) ctime(&current_time), ch->name, amt);
           ch->church->log = new_log; */

        ch->church->pneuma -= amt;
        ch->church_member->dep_pneuma -= amt;
        ch->pneuma += amt;
    }

    if (!str_cmp(arg, "dp") || !str_cmp(arg, "karma"))
    {
        if (amt > ch->church_member->dep_dp)
        {
        send_to_char(
            "You can't take out more than you put in.\n\r", ch);
        return;
        }

        if (amt > ch->church->dp)
        {
        send_to_char(
            "Sorry, there's not that much in the account.\n\r", ch);
        return;
        }

        sprintf(buf, "{Y[%s has withdrawn %ld karma.]{x\n\r",
            ch->name, amt);
        msg_church_members(ch->church, buf);
        /*
        sprintf(buf, "\n\r[%s] %s withdrew %ld karma.",
        (char *) ctime(&current_time), ch->name, amt);
        strcat(ch->church->log, buf);
         */
        ch->church->dp -= amt;
        ch->church_member->dep_dp -= amt;
        ch->deitypoints += amt;
    }

    if (!str_cmp(arg, "gold"))
    {
        if (amt > ch->church_member->dep_gold)
        {
        send_to_char(
            "You can't take out more than you put in.\n\r", ch);
        return;
        }

        if (amt > ch->church->gold)
        {
        send_to_char(
            "Sorry, there's not that much in the account.\n\r", ch);
        return;
        }

        sprintf(buf, "{Y[%s has withdrawn %ld gold.]{x\n\r",
            ch->name, amt);
        msg_church_members(ch->church, buf);
        /*
           sprintf(buf, "\n\r[%s] %s withdrew %ld gold.",
           (char *) ctime(&current_time), ch->name, amt);
           strcat(ch->church->log, buf);
         */
        ch->church->gold -= amt;
        ch->church_member->dep_gold -= amt;
        ch->gold += amt;
    }
    }
        // After successful withdrawal, add a log entry
    if (!str_cmp(arg, "pneuma") || !str_cmp(arg, "dp") || !str_cmp(arg, "gold"))
    {

        sprintf(buf, "%s withdrew %ld %s from the church account.",
            ch->name, amt, arg);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_WITHDRAWAL, true);
    }
    else if (arg3[0] != '\0')
    {
        // Log if withdrawing to another player

        sprintf(buf, "%s withdrew %ld %s from the church account and gave it to %s.",
            ch->name, amt, arg2, victim->name);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_WITHDRAWAL, true);
    }
    
    save_church(ch->church);
}


/**
 * do_chlog - View, add, edit, and search church activity logs
 *
 * Comprehensive logging system for church activities. Requires OFFICER rank
 * or VIEWLOG permission to view, EDITLOG permission to add/edit entries.
 *
 * Subcommands:
 * - (no args): Display all log entries (most recent first)
 * - <number>: View full details of a specific log entry
 * - categories: List all available log categories
 * - add <category>: Create new log entry in specified category (opens editor)
 * - edit <entry_id>: Edit an existing entry (own entries only, or leader)
 * - search <text>: Search entry text content
 * - search author <name>: Search by author name
 * - search category <name>: Search by category (supports meta-categories)
 *
 * System-generated entries (from withdrawals, transfers, etc.) cannot be edited.
 * Uses ED_CHLOG editor mode for multi-line entry creation/editing.
 *
 * @param ch        Character viewing/editing logs
 * @param argument  Subcommand and arguments
 */
void do_chlog(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_LOG_ENTRY *entry;
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    BUFFER *output;
    
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    
    if (ch->church == NULL) {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }
    
    church = ch->church;
    
    if (ch->church_member->rank->rank_type < RANK_TYPE_OFFICER 
        && !has_church_permission(ch->church_member, CHURCH_PERM_VIEWLOG)) {
        send_to_char("You don't have permission to do that.\n\r", ch);
        return;
    }
    
    // List available categories
    if (!str_cmp(arg1, "categories")) {
        output = new_buf();
        add_buf(output, "{YAvailable log categories:{x\n\r");
        add_buf(output, "-----------------------\n\r");
        
        for (int i = 0; church_log_category_flags[i].name != NULL; i++) {
            if (church_log_category_flags[i].settable) {
                sprintf(buf, "  {M%-12s{x - %s%s\n\r", 
                        church_log_category_flags[i].name,
                        church_log_category_flags[i].description ? 
                            church_log_category_flags[i].description : "No description",
                        is_meta_category(church_log_category_flags[i].bit) ? 
                            " {G(Meta-category){x" : "");
                add_buf(output, buf);
            }
        }
        
        add_buf(output, "\n\r{YUsage:{x\n\r");
        add_buf(output, "church log add <category> - Add a log entry in the specified category\n\r");
        add_buf(output, "church log search category <name> - Search for entries in a category\n\r");
        
        page_to_char(buf_string(output), ch);
        free_buf(output);
        return;
    }
    
    // View specific log entry
    if (arg1[0] != '\0' && is_number(arg1)) {
        long entry_id = atol(arg1);
        entry = NULL;
        
        // Find the entry with this ID
        for (entry = church->log_entries; entry; entry = entry->next) {
            if (entry->entry_id == entry_id)
                break;
        }
        
        if (!entry) {
            send_to_char("No log entry with that number was found.\n\r", ch);
            return;
        }
        
        // Display the full entry details
        sprintf(buf, "{YLog Entry #{C%ld{Y:{x\n\r", entry->entry_id);
        send_to_char(buf, ch);
        
        // Show timestamp
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&entry->timestamp));
        sprintf(buf, "{CTimestamp:{x %s\n\r", time_str);
        send_to_char(buf, ch);
        
        // Show author
        if (entry->author) {
            sprintf(buf, "{CAuthor:{x %s%s\n\r", 
                entry->author,
                entry->system_generated ? " {Y[Auto-generated]{x" : "");
            send_to_char(buf, ch);
        } else {
            send_to_char("{CAuthor:{x {Y(SYSTEM){x\n\r", ch);
        }
        
        // Show category if available
        if (entry->categories) {
            sprintf(buf, "{CCategory:{x {M%s{x\n\r", 
                    flag_string(church_log_category_flags, entry->categories));
            send_to_char(buf, ch);
        }
        
        // Show content
        send_to_char("{CContent:{x\n\r", ch);
        send_to_char(entry->text, ch);
        send_to_char("\n\r", ch);
        
        // Show edit option if allowed
        if (has_church_permission(ch->church_member, CHURCH_PERM_EDITLOG) &&
            (!entry->system_generated) &&
            (!entry->author || !str_cmp(entry->author, ch->name) || 
             ch->church_member->rank->rank_type == RANK_TYPE_LEADER)) {
            send_to_char("\n\r{YTo edit this entry:{x church log edit ", ch);
            sprintf(buf, "%ld\n\r", entry->entry_id);
            send_to_char(buf, ch);
        }
        
        return;
    }
    
    // Add new entry
    if (!str_cmp(arg1, "add")) {
        if (!has_church_permission(ch->church_member, CHURCH_PERM_EDITLOG)) {
            send_to_char("You don't have permission to add log entries.\n\r", ch);
            return;
        }
        
        // If no category specified, show list of valid categories
        if (arg2[0] == '\0') {
            send_to_char("{YAvailable categories:{x\n\r", ch);
            send_to_char("-----------------------\n\r", ch);
            for (int i = 0; church_log_category_flags[i].name != NULL; i++) {
                if (church_log_category_flags[i].settable) {
                    sprintf(buf, "  {M%s{x\n\r", church_log_category_flags[i].name);
                    send_to_char(buf, ch);
                }
            }
            send_to_char("\n\rUsage: church log add <category>\n\r", ch);
            return;
        }
        
        // Check if specified category is valid
        flag_t category = flag_value(church_log_category_flags, arg2);
        if (category == NO_FLAG) {
            send_to_char("{RInvalid category.{x Use one of the following:\n\r", ch);
            for (int i = 0; church_log_category_flags[i].name != NULL; i++) {
                if (church_log_category_flags[i].settable) {
                    sprintf(buf, "  {M%s{x\n\r", church_log_category_flags[i].name);
                    send_to_char(buf, ch);
                }
            }
            return;
        }
        
        // Category is valid, proceed with log entry
        ch->temp_log_category = category;
        sprintf(buf, "Creating new log entry in category: {M%s{x\n\r", 
                flag_string(church_log_category_flags, category));
        send_to_char(buf, ch);
        
        // Start the string editor with our callback
        send_to_char("Enter a new log entry. Type @ when done.\n\r", ch);
        string_append(ch, &ch->temp_log_entry);
        ch->desc->editor = ED_CHLOG;
        
        return;
    }
    
    // Edit existing entry
    if (!str_cmp(arg1, "edit")) {
        if (!has_church_permission(ch->church_member, CHURCH_PERM_EDITLOG)) {
            send_to_char("You don't have permission to edit log entries.\n\r", ch);
            return;
        }
        
        if (!arg2[0] || !is_number(arg2)) {
            send_to_char("Which log entry do you want to edit?\n\r", ch);
            send_to_char("Syntax: church log edit <entry_id>\n\r", ch);
            return;
        }
        
        long entry_id = atol(arg2);
        entry = NULL;
        
        // Find the entry with this ID
        for (entry = church->log_entries; entry; entry = entry->next) {
            if (entry->entry_id == entry_id)
                break;
        }
        
        if (!entry) {
            send_to_char("No log entry with that number was found.\n\r", ch);
            return;
        }
        
        // Check if the character can edit this entry
        if (entry->system_generated) {
            send_to_char("You cannot edit system-generated log entries.\n\r", ch);
            return;
        }
        
        if (entry->author && str_cmp(entry->author, ch->name) && 
            ch->church_member->rank->rank_type != RANK_TYPE_LEADER) {
            send_to_char("You can only edit your own log entries.\n\r", ch);
            return;
        }
        
        // Start editing
        ch->temp_log_entry = str_dup(entry->text);
        ch->temp_log_category = entry->categories;
        ch->temp_log_entry_id = entry->entry_id;
        
        sprintf(buf, "Editing log entry #%ld.\n\r", entry_id);
        send_to_char(buf, ch);
        string_append(ch, &ch->temp_log_entry);
        ch->desc->editor = ED_CHLOG;
        
        return;
    }
    
    // Search functionality
    if (!str_cmp(arg1, "search")) {
        if (!has_church_permission(ch->church_member, CHURCH_PERM_VIEWLOG)) {
            send_to_char("You don't have permission to search church logs.\n\r", ch);
            return;
        }
        
        // Create array to hold entries for search results
        CHURCH_LOG_ENTRY *entries[MAX_CHURCH_LOG_ENTRIES];
        int count = 0;
        
        // Populate the array with entries
        for (entry = church->log_entries; entry; entry = entry->next) {
            entries[count++] = entry;
            if (count >= MAX_CHURCH_LOG_ENTRIES)
                break;  // Safety check
        }
        
        // Determine search type and execute search
        char *search_text = NULL;
        char *search_author = NULL;
        flag_t search_categories = 0;
        
        if (!str_cmp(arg2, "author")) {
            search_author = argument;
            
            if (!search_author || search_author[0] == '\0') {
                send_to_char("Whose log entries do you want to search for?\n\r", ch);
                send_to_char("Syntax: church log search author <name>\n\r", ch);
                return;
            }
        }
        else if (!str_cmp(arg2, "category")) {
            // Convert category name to flag
            search_categories = flag_value(church_log_category_flags, argument);
            
            if (search_categories == NO_FLAG) {
                send_to_char("Invalid category. Available categories are:\n\r", ch);
                for (int i = 0; church_log_category_flags[i].name != NULL; i++) {
                    if (church_log_category_flags[i].settable) {
                        sprintf(buf, "  {M%-20s{x%s\n\r", 
                                church_log_category_flags[i].name,
                                is_meta_category(church_log_category_flags[i].bit) ? 
                                    " (meta-category)" : "");
                        send_to_char(buf, ch);
                    }
                }
                return;
            }
            
            // If it's a meta-category, expand it
            for (int i = 0; church_log_meta_categories[i].flag != 0; i++) {
                if (church_log_meta_categories[i].flag == search_categories) {
                    search_categories |= church_log_meta_categories[i].included;
                    break;
                }
            }
        }
        else {
            // Combine arg2 and argument for text search
            static char combined_text[2*MAX_STRING_LENGTH];
            combined_text[0] = '\0';
            
            if (arg2[0] != '\0') {
                if (argument[0] != '\0') {
                    sprintf(combined_text, "%s %s", arg2, argument);
                } else {
                    strcpy(combined_text, arg2);
                }
                search_text = combined_text;
            }
            
            if (!search_text || search_text[0] == '\0') {
                send_to_char("What text do you want to search for?\n\r", ch);
                send_to_char("Syntax: church log search <text>\n\r", ch);
                send_to_char("      : church log search author <name>\n\r", ch);
                send_to_char("      : church log search category <name>\n\r", ch);
                return;
            }
        }
        
        // Display the search results
        display_church_logs(ch, church, entries, count, search_text, search_author, search_categories);
        return;
    }
    
    // Display the log (default)
    // Create an array to hold entries for reverse display
    CHURCH_LOG_ENTRY *entries[MAX_CHURCH_LOG_ENTRIES];
    int count = 0;
    
    // Populate the array with entries
    for (entry = church->log_entries; entry; entry = entry->next) {
        entries[count++] = entry;
        if (count >= MAX_CHURCH_LOG_ENTRIES)
            break;  // Safety check
    }
    
    // Display the log entries without any filtering
    display_church_logs(ch, church, entries, count, NULL, NULL, 0);
}


/**
 * do_choverthrow - Forcibly take ownership of a church
 *
 * Allows any member to become the church owner by overthrowing current
 * leadership. All other members are demoted to rank A. Cannot be used
 * on churches that have reached CHURCH_SIZE_CHURCH (maximum size).
 * Announces the overthrow globally.
 *
 * Note: This is a drastic action with no confirmation. The lack of
 * permission checks may be intentional for small organizations or
 * may need review.
 *
 * @param ch        Character attempting the overthrow
 * @param argument  Unused
 */
void do_choverthrow(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    CHURCH_PLAYER_DATA *member;
    char buf[MSL];

    if ((church = ch->church) == NULL)
    {
      send_to_char("You aren't even in a church.\n\r", ch);
     return;
    }

    if (is_church_owner(ch, church))
    {
        send_to_char("That would be pointless, you're already the owner.\n\r", ch);
        return;
    }

    if (church->size == CHURCH_SIZE_CHURCH)
    {
    send_to_char("You can't overthrow a church that big.\n\r", ch);
    return;
    }

    member = NULL;
    for (member = church->people; member != NULL; member = member->next)
    {
    if (member != ch->church_member) /* demote everyone else */
        member->rank = CHURCH_RANK_A;
    }

    sprintf(buf,"{Y[%s has overthrown the %s of %s!]{x\n\r",
        ch->name,
        get_chsize_from_number(church->size),
        church->name);
    gecho(buf);

    free_string(church->owner);
    church->owner = str_dup(ch->name);
    
    // Add log entry about the overthrow
    char log_buf[MAX_STRING_LENGTH];
    sprintf(log_buf, "%s has overthrown the church and become the new owner!", ch->name);
    add_church_log_entry(ch->church, NULL, log_buf, CHLOG_LEADERSHIP, true);
    
    save_church(ch->church);
}


/**
 * do_chwhere - Locate online church members
 *
 * Shows the current location (room name) of all online members of the
 * character's church. Optionally can search for a specific member by name.
 *
 * Syntax:
 * - church where: List all online members and their locations
 * - church where <name>: Find a specific member
 *
 * @param ch        Character requesting location info
 * @param argument  Optional member name to search for
 */
void do_chwhere(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    DESCRIPTOR_DATA *d;
    bool found;

    one_argument(argument, arg);

    if (ch->church == NULL)
    {
    send_to_char("You aren't in a church.\n\r", ch);
    return;
    }

    if (arg[0] == '\0')
    {
    send_to_char(
    "You detect the location of your fellow members:\n\r{x", ch);
    found = false;
    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING
        &&  (victim = d->character) != NULL
        &&  victim->church != NULL
        &&  victim->church == ch->church && !IS_NPC(victim)
        &&  victim->in_room != NULL)
        {
        found = true;
            sprintf(buf, "{Y%-28s{X %s\n\r",
                pers(victim, ch), victim->in_room->name);
        send_to_char(buf, ch);
        }
    }
    if (!found)
        send_to_char("None\n\r", ch);
    }
}


/**
 * do_chtoggle - Toggle church PK (player killing) status
 *
 * Enables or disables PK mode for the church. Requires being at an
 * administration office (room with ACT2_CHURCHMASTER NPC).
 *
 * Enabling PK: Shows warning and sets pk_question flag for confirmation.
 * Disabling PK: Calls chtoggle_complete directly (costs pneuma per game_settings).
 *
 * @param ch        Character toggling PK status
 * @param argument  Unused
 */
void do_chtoggle(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    bool found = false;
    char buf[MSL];

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }

    found = false;
    for (temp_char = ch->in_room->people; temp_char != NULL;
         temp_char = temp_char->next_in_room) {
        if (IS_NPC(temp_char) &&
                IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
            found = true;
    }

    if (!found) {
        send_to_char
            ("You must be at an administration office.\n\r",
             ch);
        return;
    }

    if (ch->church->pk == true)
    {
        // Call the central function to handle disabling PK
        chtoggle_complete(ch, false);
    }
    else
    {
        // We still use the confirmation prompt for enabling PK
        send_to_char("{RWarning: {xYour church will be a player killing church!\n\r", ch);
        sprintf(buf, "{RYou will need to pay %d pneuma to disable this later.{x\n\r",
                game_settings.org_disable_pk_pneuma_cost);
        send_to_char(buf, ch);
        send_to_char("{YAre you sure you want to do this?{x\n\r", ch);
        ch->pk_question = true;
    }
}

/**
 * find_char_church - Find a character's church by name lookup
 *
 * @deprecated Marked for removal. Just use ch->church directly instead.
 * This function iterates through all churches searching by name, which
 * is redundant when ch->church already holds the direct reference.
 *
 * Note: Contains debug output (sends church name to character).
 *
 * @param ch  Character to find church for
 * @return    Church data pointer or NULL
 */
CHURCH_DATA *find_char_church(CHAR_DATA * ch)
{
    CHURCH_DATA *chr;
    chr = NULL;

    if (ch->church == NULL)
    return NULL;

    for (chr = church_first; chr != NULL; chr = chr->next)
    {
    send_to_char(ch->church_name, ch);

    if (!str_cmp(ch->church_name, chr->name))
        return chr;
    }

    return NULL;
}


/**
 * find_char_position_in_church - Get character's rank type in their church
 *
 * @param ch  Character to check
 * @return    Rank type (RANK_TYPE_MEMBER, RANK_TYPE_OFFICER, RANK_TYPE_LEADER)
 *            or -1 if not in a church or no rank assigned
 */
int find_char_position_in_church(CHAR_DATA *ch)
{
    if (ch->church != NULL && ch->church_member != NULL && ch->church_member->rank != NULL)
        return ch->church_member->rank->rank_type;
    else
        return -1;
}


/**
 * find_church - Find a church by its position in the list
 *
 * Returns the church at the specified 1-based index in list_churches.
 * Used to look up churches by the number shown in show_chlist_to_char.
 *
 * @param number  1-based index from church list display
 * @return        Church data pointer or NULL if index out of range
 */
CHURCH_DATA *find_church(int number)
{
    CHURCH_DATA *church;
    ITERATOR it;

    iterator_start(&it, list_churches);

    while( (church = (CHURCH_DATA *)iterator_nextdata(&it)) && --number > 0);

    iterator_stop(&it);

    return church;
}

/**
 * find_church_name - Find a church by exact name match
 *
 * Searches list_churches for a church with the given name (case-insensitive).
 *
 * @param name  Church name to search for
 * @return      Church data pointer or NULL if not found
 */
CHURCH_DATA *find_church_name(char *name)
{
    CHURCH_DATA *church;
    ITERATOR it;

    iterator_start(&it, list_churches);

    while( (church = (CHURCH_DATA *)iterator_nextdata(&it)) ) {
        if( !str_cmp( church->name, name ) )
            break;
    }

    iterator_stop(&it);

    return church;
}

/**
 * is_treasure_room - Check if a room is a church treasure room
 *
 * If church is NULL, searches all churches to see if the room belongs
 * to any church's treasure room list. If church is specified, only
 * checks that specific church's treasure rooms.
 *
 * @param church  Church to check, or NULL to check all churches
 * @param room    Room to test
 * @return        true if room is a treasure room, false otherwise
 */
bool is_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room)
{
    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;


    if (!church) {
        ITERATOR cit;

        iterator_start(&cit, list_churches);
        while(( church = (CHURCH_DATA *)iterator_nextdata(&cit)))
            if( is_treasure_room(church, room) )
                break;
        iterator_stop(&cit);

        return church && true;
    }

    iterator_start(&it, church->treasure_rooms);
    while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
        if( treasure->room == room )
            break;
    }
    iterator_stop(&it);

    return treasure && true;
}


/**
 * do_chcolour - Set custom colors for church talk channel
 *
 * Allows setting two color codes that will be used for the church
 * communication channel. Validates color codes before accepting.
 * Logs the change to church activity log.
 *
 * Syntax: church colour <colour1> <colour2>
 * Examples: church colour {Y {B
 *           church colour {[F345] {[B555]
 *
 * @param ch        Character setting colors
 * @param argument  Two color codes
 */
void do_chcolour(CHAR_DATA *ch, char *argument)
{
    char arg[32], arg2[32];
    char buf[MAX_STRING_LENGTH];


    argument = one_argument_norm(argument, arg);
    argument = one_argument_norm(argument, arg2);

    if (ch->church == NULL) {
        send_to_char("You aren't in a church.\n\r", ch);
        return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0') {
        send_to_char(
            "Syntax: church colour <colour1> <colour2>\n\r"
            "Ex.: church colour {Y {B\n\r"
            "Ex.: church colour {[F345] {[B555]{X\n\r", ch);
        return;
    }

    // Limit length and validate
    if (strlen(arg) > 31 || strlen(arg2) > 31 ||
        !is_valid_colour_code(arg) || !is_valid_colour_code(arg2)) {
        send_to_char("Invalid colour code. Use codes like {{Y, {{B, {{[F345], etc. See 'help color'.\n\r", ch);
        return;
    }

    // Save the codes (free old if needed)
    free_string(ch->church->colour1);
    free_string(ch->church->colour2);
    ch->church->colour1 = str_dup(arg);
    ch->church->colour2 = str_dup(arg2);

    send_to_char("Church colours set.\n\r", ch);

    sprintf(buf, "%s changed the church colours to %scolour1{x and %scolour2{x.",
            ch->name, arg, arg2);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);

    save_church(ch->church);
}


/**
 * do_chtrust - Deprecated trust command stub
 *
 * @deprecated Replaced by 'church userperm' command.
 * Displays message directing users to the new command syntax.
 *
 * @param ch        Character (unused)
 * @param argument  Arguments (unused)
 */
void do_chtrust(CHAR_DATA *ch, char *argument)
{
    send_to_char("The trust command has been replaced with 'church userperm'.\n\r", ch);
    send_to_char("Syntax: church userperm <character> <permission> [on|off]\n\r", ch);
    return;
}

/**
 * append_church_log - Add an entry to a church's activity log
 *
 * Wrapper that creates a system-generated log entry with CHLOG_GENERAL
 * category. Also logs to the server log for debugging.
 *
 * @param church  Church to add log entry to
 * @param string  Log message text
 */
void append_church_log(CHURCH_DATA *church, char *string)
{
    char buf[MSL];
    
    if (church == NULL) {
        pbugf(LOG_ERROR, "append_church_log: null church.");
        return;
    }
    
    sprintf(buf, "append_church_log: appended %s to log of %s",
        string, church->name);
    log_string(buf);
    
    // Add as system-generated entry
    add_church_log_entry(church, NULL, string, CHLOG_GENERAL, true);
}


/**
 * time_for_log - Get current time as string without newline
 *
 * Returns a str_dup'd string of the current time, with the trailing
 * newline removed (ctime normally includes one).
 *
 * @return  Allocated string with formatted time (caller must free)
 */
char *time_for_log(void)
{
    char buf[MSL];
    char buf2[MSL];
    int i;

    sprintf(buf, "%s", ctime(&current_time));
    i = 0;
    while (buf[i] != '\n')
    {
    buf2[i] = buf[i];
    i++;
    }

    buf2[i] = '\0';

    return str_dup(buf2);
}


/**
 * show_church_info - Display detailed church information to staff
 *
 * Shows comprehensive church data for staff members including:
 * name, flag, founder, owner, resources (pneuma/gold/karma),
 * max positions, size, alignment, PK stats, hall location,
 * complete member roster with ranks and permissions.
 *
 * Called by do_chinfo when staff uses number argument.
 *
 * @param church  Church to display
 * @param ch      Staff member viewing the info
 */
void show_church_info(CHURCH_DATA *church, CHAR_DATA *ch)
{
    BUFFER *buffer;
    CHURCH_PLAYER_DATA *member;
    char buf[2*MSL];
    char buf2[MSL];
    int i;

    buffer = new_buf();

    sprintf(buf, "{BChurch Info:{x\n\r");
    add_buf(buffer, buf);

    sprintf(buf, "{YName:{x %s\n\r", church->name);
    add_buf(buffer, buf);

    sprintf(buf, "{YFlag:{x %s\n\r", church->flag);
    add_buf(buffer, buf);

    sprintf(buf, "{YFounder:{x %s\n\r", church->founder);
    add_buf(buffer, buf);

    sprintf(buf, "{YPneuma:{x %ld\n\r", church->pneuma);
    add_buf(buffer, buf);

    sprintf(buf, "{YGold:{x %ld\n\r", church->gold);
    add_buf(buffer, buf);

    sprintf(buf, "{YKarma:{x %ld\n\r", church->dp);
    add_buf(buffer, buf);

    sprintf(buf, "{YMax positions:{x %d\n\r", church->max_positions);
    add_buf(buffer, buf);

    switch(church->size)
    {
    case CHURCH_SIZE_BAND:
        strcpy(buf2, "Band");
        break;
    case CHURCH_SIZE_CULT:
        strcpy(buf2, "Cult");
        break;
    case CHURCH_SIZE_ORDER:
        strcpy(buf2, "Order");
        break;
    case CHURCH_SIZE_CHURCH:
        strcpy(buf2, "Church");
        break;
    default:
        strcpy(buf2, "Unknown");
        break;
    }

    sprintf(buf, "{YSize:{x %s\n\r", buf2);
    add_buf(buffer, buf);

    if (church->alignment == CHURCH_GOOD)
    sprintf(buf2, "Good");
    else if (church->alignment == CHURCH_EVIL)
    sprintf(buf2, "Evil");
    else
    sprintf(buf2, "Neutral");

    sprintf(buf, "{YAlignment:{x %s\n\r", buf2);
    add_buf(buffer, buf);

    sprintf(buf, "{YRecall Point:{x %ld - %s\n\r",
        church->recall_point.id[0],
    get_room_index(find_area_by_vnum(church->recall_point.id[0], NULL) ? find_area_by_vnum(church->recall_point.id[0], NULL) : get_system_area_fallback(), church->recall_point.id[0]) == NULL ?
        "none" : get_room_index(find_area_by_vnum(church->recall_point.id[0], NULL) ? find_area_by_vnum(church->recall_point.id[0], NULL) : get_system_area_fallback(), church->recall_point.id[0])->name);
    add_buf(buffer, buf);

    sprintf(buf, "{YKey:{x %ld - %s\n\r",
        church->key,
    get_obj_index(find_area_by_vnum(church->key, NULL) ? find_area_by_vnum(church->key, NULL) : get_system_area_fallback(), church->key) == NULL ?
        "none" : get_obj_index(find_area_by_vnum(church->key, NULL) ? find_area_by_vnum(church->key, NULL) : get_system_area_fallback(), church->key)->short_descr);
    add_buf(buffer, buf);

    sprintf(buf, "{YTreasure Room(s):{x\n\r");
    add_buf(buffer, buf);

    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;
    iterator_start(&it, church->treasure_rooms);
    while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) ) {
        if( treasure->room != NULL )
        {
            ROOM_INDEX_DATA *room = treasure->room;
            sprintf(buf, "{x\t\t%s - %s{x\n\r", widevnum_string_room(room, NULL), room->name);
            add_buf(buffer,buf);
        }
    }
    iterator_stop(&it);
    

    sprintf(buf, "{YPK record:{x %ld wins, %ld losses\n\r",
        church->pk_wins, church->pk_losses);
    add_buf(buffer, buf);

    sprintf(buf, "{YCPK record:{x %ld wins, %ld losses\n\r",
        church->cpk_wins, church->cpk_losses);
    add_buf(buffer, buf);

    sprintf(buf, "{YWars Won:{x %ld\n\r", church->wars_won);
    add_buf(buffer, buf);

    sprintf(buf, "{YPK:{x %s\n\r", church->pk ? "yes" : "no");
    add_buf(buffer, buf);

    sprintf(buf, "{YColours:{x %sColour One{x and %sColour Two{x\n\r",
        church->colour1, church->colour2);
    add_buf(buffer, buf);

    sprintf(buf, "{BMember Info:{x\n\r");
    add_buf(buffer, buf);

    sprintf(buf,
        "{Y%-12s %-10s %-10s %-10s %-8s %-8s %-4s{x\n\r{x",
    "Name", "Pneuma", "Karma", "Gold", "PK", "CPK", "Wars");
    add_buf(buffer, buf);

    i = 0;
    for (member = church->people; member != NULL; member = member->next)
    {
        i++;

    sprintf(buf,
        "%-12s %-10ld %-10ld %-8ld",
        member->name,
        member->dep_pneuma,
        member->dep_dp,
        member->dep_gold);

    sprintf(buf2, "%4ld-%-4ld", member->pk_wins, member->pk_losses);
    strcat(buf, buf2);

    sprintf(buf2, "%4ld-%-4ld", member->cpk_wins, member->cpk_losses);
    strcat(buf, buf2);

    sprintf(buf2, "%4ld\n\r", member->wars_won);
    strcat(buf, buf2);

        add_buf(buffer, buf);
    }

    page_to_char(buf_string(buffer), ch);

    free_buf(buffer);
}


/**
 * do_churchset - Toggle church configuration settings
 *
 * Toggles boolean settings defined in church_flags. Examples include
 * CHURCH_SHOW_PKS (show PK stats publicly) and other display options.
 * Logs changes to church activity log.
 *
 * Syntax: church set <field>
 *
 * @param ch        Character changing settings
 * @param argument  Setting name from church_flags
 */
void do_churchset(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    CHURCH_DATA *church;
    long value;
    char buf[MSL];

    if ((church = ch->church) == NULL)
    {
    send_to_char("You aren't even in a church.\n\r", ch);
    return;
    }

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("Syntax: church set <field>\n\r"
                 "For fields see \"help church\"", ch);
    return;
    }

    /* find value to toggle */
    if ((value = flag_value(church_flags, arg)) == NO_FLAG)
    {
    send_to_char("There is no such setting. See \"help church\" for available settings.\n\r", ch);
    return;
    }

    if (IS_SET(church->settings, value))
    {
        REMOVE_BIT(church->settings, value);
        send_to_char("Setting toggled OFF.\n\r", ch);

        sprintf(buf, "%s turned setting '%s' OFF.", ch->name, arg);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
    }
    else
    {
        SET_BIT(church->settings, value);
        send_to_char("Setting toggled ON.\n\r", ch);

        sprintf(buf, "%s turned setting '%s' ON.", ch->name, arg);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
    }

    save_church(ch->church);

}


/**
 * do_chconvert - Convert church alignment (founder only)
 *
 * Allows the church founder to convert from neutral to good or evil.
 * Only neutral churches can change alignment. The founder's personal
 * alignment must be compatible (can't convert to good if evil, etc).
 *
 * Cost: 10,000 pneuma + 2,500,000 karma
 *
 * WARNING: Members incompatible with the new alignment will be removed
 * on their next login.
 *
 * Sets ch->pcdata->convert_church for confirmation prompt.
 *
 * Syntax: church convert <good|neutral|evil>
 *
 * @param ch        Church founder
 * @param argument  Target alignment
 */
void do_chconvert(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    char arg[MSL];
    char buf[MSL];
    int align = -1;
    long pneuma_cost;
    long dp_cost;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0'
    || (str_cmp(arg, "good") && str_cmp(arg, "evil")
        && str_cmp(arg, "neutral")))
    {
    send_to_char("Syntax: church convert <good|neutral|evil>\n\r", ch);
    return;
    }

    if (!str_cmp(arg, "good"))
    align = CHURCH_GOOD;
    else if (!str_cmp(arg, "neutral"))
    align = CHURCH_NEUTRAL;
    else
    align = CHURCH_EVIL;

    if ((church = ch->church) == NULL)
    {
    send_to_char("You aren't even in a church.\n\r", ch);
    return;
    }

    if (str_cmp(ch->name, church->founder))
    {
    send_to_char("Only the founder of a church can change its faith.\n\r", ch);
    return;
    }

    if (align == church->alignment)
    {
    send_to_char("That would be pointless. Your church already follows that alignment.\n\r", ch);
    return;
    }

    if ((ch->alignment < 0 && align == CHURCH_GOOD)
    ||   (ch->alignment > 0 && align == CHURCH_EVIL))
    {
    send_to_char("You cannot convert to that alignment as you, the founder, cannot follow that faith.\n\r", ch);
    return;
    }

    if (church->alignment != CHURCH_NEUTRAL)
    {
    send_to_char("Only neutral churches can change their faith.\n\r", ch);
    return;
    }

    pneuma_cost = 10000;
    dp_cost = 2500000;
    if (church->pneuma < pneuma_cost || church->dp < dp_cost)
    {
    sprintf(buf, "It costs %ld pneuma and %ld karma to convert your alignment. You don't have enough.\n\r", pneuma_cost, dp_cost);
    send_to_char(buf, ch);
    return;
    }

    ch->pcdata->convert_church = align;
    sprintf(buf, "Are you SURE you want to convert to the faith of %s? (y/n)\n\r"
                  "{R***WARNING***:{x all members who cannot follow that faith will be removed on their next login!!!\n\r",
       ch->pcdata->convert_church == CHURCH_GOOD ? "the Pious" :
      ch->pcdata->convert_church == CHURCH_NEUTRAL ? "Neutrality" :
      "Malice");
    send_to_char(buf, ch);

    // After successful conversion (in the "yes" confirmation handler):
    sprintf(buf, "%s converted the church alignment to %s.",
        ch->name, 
        ch->church->alignment == CHURCH_GOOD ? "good" : 
        ch->church->alignment == CHURCH_EVIL ? "evil" : "neutral");
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_GEN_SETTINGS, true);
    
    save_church(ch->church);
}


/**
 * do_chdonate - Donate an item to church treasure room
 *
 * Transfers an object from the player's inventory to one of the church's
 * treasure rooms. Excommunicated members cannot donate. Items with timers
 * or NO_DONATE flag cannot be donated.
 *
 * Each treasure room has a max capacity (MAX_CHURCH_TREASURE) and may have
 * minimum rank requirements for access (except the first/default room).
 *
 * Syntax: church donate <object> [room_number]
 *
 * @param ch        Character donating
 * @param argument  Object name and optional treasure room number
 */
void do_chdonate(CHAR_DATA *ch, char *argument)
{
    // church donate <obj>[ <room no>]
    OBJ_DATA *obj;
    char arg[MIL];

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church!\n\r", ch);
        return;
    }

    if(is_excommunicated(ch))
    {
        send_to_char("You have been excommunicated.\n\r", ch);
        return;
    }

    int avail = church_available_treasure_rooms(ch);

    if (avail < 1)
    {
        send_to_char("You do not have access to a treasure room.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
        send_to_char("You don't have that object.\n\r", ch);
        return;
    }

    int roomno = 1;
    if( !IS_NULLSTR(argument) )
    {
        if(!is_number(argument))
        {
            send_to_char("That is not a number.\n\r", ch);
            return;
        }

        roomno = atoi(argument);

        if( roomno < 1 || roomno > avail)
        {
            send_to_char("That is not a valid room number.\n\rPlease review the list in {WCHURCH TREASURE LIST{x.\n\r", ch);
            return;
        }
    }


    CHURCH_TREASURE_ROOM *treasure = get_church_treasure_room(ch, ch->church, roomno);
    if( !treasure || !treasure->room )
    {
        send_to_char("Something went wrong.  Could not find the treasure room.\n\r", ch);
        return;
    }

    // Only check rooms after the default room
    if (roomno > 1 && ch->church_member->rank->rank_type < treasure->min_rank)
    {
        send_to_char("You do not have permission to use that room.\n\r", ch);
        return;
    }

    if (count_items_list_nest(treasure->room->contents) > MAX_CHURCH_TREASURE)
    {
        send_to_char("That church temple treasure room is quite full already.\n\rPlease try another room.\n\r", ch);
        return;
    }

    if (obj->timer > 0 || IS_SET(obj->extra[1], ITEM_NO_DONATE))
    {
        act("You cannot donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
        send_to_char("It's stuck to you.\n\r", ch);
        return;
    }

    act("You toss $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n tosses $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    obj_from_char(obj);
    obj_to_room(obj, treasure->room);

/*
#if 1


    send_to_char("Donation is disabled for the time being.\n\r", ch);
#else
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a church!\n\r", ch);
    return;
    }

    if (!list_size(ch->church->treasure_rooms))
    {
        send_to_char("Your church doesn't have a treasure room.\n\r", ch);
        return;
    }

    if ((obj = get_obj_carry(ch, argument, ch)) == NULL)
    {
    send_to_char("You don't have that object.\n\r", ch);
    return;
    }

    if (count_items_list_nest(room->contents) > MAX_CHURCH_TREASURE)
    {
        send_to_char("Your church temple treasure room is quite full already.\n\r", ch);
    return;
    }

    if (obj->timer > 0 || IS_SET(obj->extra[1], ITEM_NO_DONATE))
    {
        act("You cannot donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
        send_to_char("It's stuck to you.\n\r", ch);
    return;
    }

    act("You toss $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n tosses $p into the air and it disappears into a swirling vortex.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    obj_from_char(obj);
    obj_to_room(obj, room);
#endif
*/

    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "%s donated %s to treasure room %d.", 
            ch->name, obj->short_descr, roomno);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_TREASURE, true);
    
    save_church(ch->church);
}


/**
 * write_churches_new - Save all churches to individual files
 *
 * Iterates through list_churches and calls save_church() for each.
 * Each church is saved to its own file based on UID.
 */
void write_churches_new()
{
    CHURCH_DATA *church;
    ITERATOR it;

    log_string("Writing all churches...");

    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it))) {
        save_church(church);
    }
    iterator_stop(&it);
}


/**
 * write_church - Write a single church to a file stream
 *
 * Outputs church data in a tagged format for persistence.
 * Includes all church properties, ranks, members, treasure rooms.
 *
 * File format uses "#CHURCH" section with "Key Value" pairs.
 *
 * @param church  Church data to write
 * @param fp      Open file pointer to write to
 */
void write_church(CHURCH_DATA *church, FILE *fp)
{
    char buf[MSL];
    ITERATOR it;
    ROOM_INDEX_DATA *room;

    fprintf(fp, "#CHURCH\n");
    fprintf(fp, "Name %s~\n", church->name);
    fprintf(fp, "UID %ld\n", church->uid);
    fprintf(fp, "Version %d\n", VERSION_CHURCH);
    fprintf(fp, "Deleted %d\n", church->deleted ? 1 : 0);
    fprintf(fp, "Alignment %d\n", church->alignment);
    fprintf(fp, "CPKLosses %ld\n", church->cpk_losses);
    fprintf(fp, "CPKWins %ld\n", church->cpk_wins);
    fprintf(fp, "Colour1 %s~\n", church->colour1);
    fprintf(fp, "Colour2 %s~\n", church->colour2);
    fprintf(fp, "DeityPoints %ld\n", church->dp);
    fprintf(fp, "Flag %s~\n", fix_string(church->flag));
    fprintf(fp, "Founder %s~\n", church->founder);
    fprintf(fp, "Owner %s~\n", church->owner);
    fprintf(fp, "LastLoginOwner %ld\n", (long int)church->owner_last_login);
    fprintf(fp, "LastLogID %ld\n", church->last_log_entry_id);
    fprintf(fp, "Gold %ld\n", church->gold);
    fprintf(fp, "Key %ld\n", church->key ? church->key : 0);
    fprintf(fp, "MaxPositions %d\n", church->max_positions);
    fprintf(fp, "PKLosses %ld\n", church->pk_losses);
    fprintf(fp, "PKWins %ld\n", church->pk_wins);
    fprintf(fp, "Pneuma %ld\n", church->pneuma);
    fprintf(fp, "RecallPoint %ld\n", church->recall_point.id[0]);
    fprintf(fp, "Settings %s\n", fwrite_flag(church->settings, buf));
    fprintf(fp, "Size %d\n", church->size);
    fprintf(fp, "ToggledPK %d\n", church->pk);
    fprintf(fp, "WarsWon %ld\n", church->wars_won);
    fprintf(fp, "CofferRent %ld\n", church->coffer_rent);
    fprintf(fp, "StoragePerms %ld\n", church->storage_permissions);

    // Write church content
    fprintf(fp, "Motd %s~\n", fix_string(church->motd));
    fprintf(fp, "Rules %s~\n", fix_string(church->rules));
    if (church->info != NULL)
        fprintf(fp, "Info %s~\n", fix_string(church->info));
        
        // Write treasure rooms
    iterator_start(&it, church->treasure_rooms);
    CHURCH_TREASURE_ROOM *treasure;
    while((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        room = treasure->room;
        
        if(room->wilds)
            fprintf(fp, "TreasureVRoom %ld %ld %ld %ld %d\n", 
                room->wilds->uid, room->x, room->y, room->z, 
                treasure->is_default ? 1 : 0);
        else
            fprintf(fp, "TreasureRoom %ld %d\n", 
                room->vnum, treasure->is_default ? 1 : 0);
            
        // Write rank access list
        if (!treasure->is_default && treasure->allowed_ranks) {
            ITERATOR rank_it;
            CHURCH_RANK_DATA *rank;
            int rank_index;
            
            iterator_start(&rank_it, treasure->allowed_ranks);
            while ((rank = (CHURCH_RANK_DATA *)iterator_nextdata(&rank_it))) {
                // Find the rank index
                rank_index = 0;
                CHURCH_RANK_DATA *r = church->ranks;
                while (r && r != rank) {
                    r = r->next;
                    rank_index++;
                }
                
                // Only write valid indices
                if (r == rank) {
                    fprintf(fp, "TreasureAccess %d\n", rank_index);
                }
            }
            iterator_stop(&rank_it);
        }
    
    iterator_stop(&it);
    }

    if (church->default_rank) {
        fprintf(fp, "DefaultRankUID %ld\n", church->default_rank->uid);
    }
    
    // Write church ranks with the new flags field
    fprintf(fp, "NumRanks %d\n", church->num_ranks);
    
    CHURCH_RANK_DATA *rank;
    int i = 0;
    for (rank = church->ranks; rank; rank = rank->next, i++) {
        fprintf(fp, "Rank %d\n", i);
        fprintf(fp, "RankUID %ld\n", rank->uid);
        fprintf(fp, "RankName %s~\n", rank->rank_name ? rank->rank_name : rank->title_male);
        fprintf(fp, "RankTitleMale %s~\n", rank->title_male);
        fprintf(fp, "RankTitleFemale %s~\n", rank->title_female);
        fprintf(fp, "RankTitleNeutral %s~\n", rank->title_neutral);
        fprintf(fp, "RankType %d\n", rank->rank_type);
        fprintf(fp, "RankPermissions %ld\n", rank->permissions);
        fprintf(fp, "RankFlags %ld\n", rank->flags);
        fprintf(fp, "EndRank\n");
    }
    
    // Write church coffer items
    if (church->coffer)
    {
        fprintf(fp, "#COFFER\n");
        OBJ_DATA *obj;
        for (obj = church->coffer; obj != NULL; obj = obj->next_content)
        {
            fwrite_obj_new(NULL, obj, fp, 0);
        }
        fprintf(fp, "#ENDCOFFER\n");
    }

    // Write church members
    if (church->people != NULL)
        write_church_member(church->people, fp);

    
    CHURCH_LOG_ENTRY *entry;
    for (entry = church->log_entries; entry; entry = entry->next) {
        fprintf(fp, "#LOG\n");
        fprintf(fp, "EntryID %ld\n", entry->entry_id);
        fprintf(fp, "Timestamp %ld\n", entry->timestamp);
        fprintf(fp, "System %d\n", entry->system_generated ? 1 : 0);
        if (entry->author)
            fprintf(fp, "Author %s~\n", entry->author);
        fprintf(fp, "Text %s~\n", fix_string(entry->text));
        fprintf(fp, "#ENDLOG\n");
    }

    fprintf(fp, "#-CHURCH\n");
}


/**
 * write_church_member - Write a church member entry to file
 *
 * Recursively writes all members (next first, then current).
 * Each member is output as a #MEMBER section with stats and rank.
 *
 * @param member  Member data to write
 * @param fp      Open file pointer
 */
void write_church_member(CHURCH_PLAYER_DATA *member, FILE *fp)
{
    if (member->next != NULL)
        write_church_member(member->next, fp);

    fprintf(fp, "\n#MEMBER %s~\n", member->name);
    fprintf(fp, "Alignment %d\n", member->alignment);
    fprintf(fp, "CPKLosses %ld\n", member->cpk_losses);
    fprintf(fp, "CPKWins %ld\n", member->cpk_wins);
    fprintf(fp, "DepositedDp %ld\n", member->dep_dp);
    fprintf(fp, "DepositedGold %ld\n", member->dep_gold);
    fprintf(fp, "DepositedPneuma %ld\n", member->dep_pneuma);
    fprintf(fp, "Flags %ld\n", member->flags);
    fprintf(fp, "PersonalPerms %ld\n", member->personal_permissions);
    fprintf(fp, "PKLosses %ld\n", member->pk_losses);
    fprintf(fp, "PKWins %ld\n", member->pk_wins);
    
    // Store rank by name for better persistence across church structure changes
    if (member->rank)
        fprintf(fp, "RankUID %ld\n", member->rank->uid);
    
    fprintf(fp, "Sex %d\n", member->sex);
    fprintf(fp, "WarsWon %ld\n", member->wars_won);
    fprintf(fp, "#-MEMBER\n");


}


/**
 * add_church_to_list - Append a church to the end of a linked list
 *
 * @param church  Church to add
 * @param list    Head of existing list (must not be NULL)
 */
void add_church_to_list(CHURCH_DATA *church, CHURCH_DATA *list)
{
    CHURCH_DATA *tmp;

    church->next = NULL;

    for (tmp = list; tmp->next != NULL; tmp = tmp->next)
    ;

    tmp->next = church;
}


/**
 * read_churches_new - Load all churches from disk at boot time
 *
 * Reads churches from individual files in ORG_DIR directory.
 * Handles legacy format migration from single churches.dat file.
 * Validates and links churches to the global list_churches.
 *
 * After loading, resolves member character pointers and assigns
 * ranks to members.
 */
void read_churches_new()
{
    DIR *dir;
    struct dirent *entry;
    FILE *fp;
    char filename[512];
    CHURCH_DATA *church;

    log_string("Reading churches...");
    
    // First check for the legacy churches.dat file
    sprintf(filename, "%schurches.dat", ORG_DIR);
    if ((fp = fopen(filename, "r")) != NULL) {
        log_string("Found legacy churches.dat file - converting to new format");
        
        // Read churches from the legacy format
        int legacy_count = 0;
        
        // Read safely to prevent EOF issues
        for (;;) {
            if (feof(fp)) {
                log_string("Reached end of churches.dat file");
                break;
            }
            
            char *word = fread_word(fp);
            
            if (!word || word[0] == '\0') {
                log_string("Warning: Encountered empty word in churches.dat - possible corruption");
                break;
            }
            
            if (!str_cmp(word, "#ENDFILE"))
                break;
                
            if (!str_cmp(word, "#CHURCH")) {
                church = read_church(fp);
                
                if (church) {
                    // Add to lists

                    
if (!list_appendlink(list_churches, church)) {
    log_string("Failed to add church to list");
    free_church(church);
    continue;
}
                    
                    save_church_json(church);  // Save as JSON
                    legacy_count++;
                    log_string(formatf("Converted church %s (UID %ld) to JSON format",
                        church->name, church->uid));
                }
            }
        }
        
        fclose(fp);
        
        // Rename the legacy file to prevent re-reading
        char backup_name[512];
        sprintf(backup_name, "%schurches.dat.bak", ORG_DIR);
        rename(filename, backup_name);
        
        log_string(formatf("Converted %d churches from legacy format to JSON", legacy_count));
    }
    
    // Now read the individual church files
    if ((dir = opendir(ORG_DIR)) == NULL) {
        pbugf(LOG_ERROR, "read_churches_new: can't open church directory");
        return;
    }
    
    // Read each file in the directory
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Check for .json extension first (preferred)
        bool is_json = (strlen(entry->d_name) > 5 && 
                       !strcmp(entry->d_name + strlen(entry->d_name) - 5, ".json"));
        bool is_org = (strlen(entry->d_name) > 4 && 
                      !strcmp(entry->d_name + strlen(entry->d_name) - 4, ".org"));
        
        if (!is_json && !is_org)
            continue;
        
        // Build full filename
        snprintf(filename, sizeof(filename), "%s%s", ORG_DIR, entry->d_name);
        
        church = NULL;
        
        if (is_json) {
            // Load JSON format
            if (!load_church_json(filename, &church)) {
                log_string(formatf("Failed to load church JSON file: %s", filename));
                continue;
            }
        } else {
            // Load legacy .org format
            if ((fp = fopen(filename, "r")) == NULL) {
                log_string(formatf("Failed to open church file: %s", filename));
                continue;
            }
            
            // Read first line to check format
            char first_line[256];
            bool correct_format = false;
            if (fgets(first_line, sizeof(first_line), fp) != NULL) {
                if (strstr(first_line, "#CHURCH")) {
                    correct_format = true;
                }
            }
            
            if (!correct_format) {
                log_string(formatf("Invalid church file format: %s", filename));
                fclose(fp);
                continue;
            }
            
            // Reset file position to beginning
            rewind(fp);
            
            // Read the church data
            church = read_church(fp);
            fclose(fp);
            
            if (church) {
                // Migrate to JSON and archive old file
                log_string(formatf("Migrating church %s to JSON format", church->name));
                save_church_json(church);
                
                char archive_name[520];  // filename is 512, +8 for ".old" suffix
                snprintf(archive_name, sizeof(archive_name), "%s.old", filename);
                rename(filename, archive_name);
            }
        }
        
        if (church == NULL) {
            log_string(formatf("Failed to read church from file: %s", filename));
            continue;
        }

        if (church->deleted) {
            log_string(formatf("Skipping deleted church: %s (UID %ld)",
                church->name, church->uid));
            free_church(church);
            continue;
        }
        
        // Validate UID
        if (church->uid == 0) {
            log_string(formatf("Church %s has invalid UID 0, assigning new UID", church->name));
            get_church_id(church);
        }
        
        // Check for duplicate UID
        CHURCH_DATA *existing = NULL;
        ITERATOR it;
        iterator_start(&it, list_churches);
        while ((existing = (CHURCH_DATA *)iterator_nextdata(&it))) {
            if (existing->uid == church->uid) {
                log_string(formatf("WARNING: Duplicate church UID %ld for %s and %s",
                    church->uid, existing->name, church->name));
                get_church_id(church); // Assign a new UID
                break;
            }
        }
        iterator_stop(&it);
        
        if (!existing) {
            // Add to lists
            if (!list_appendlink(list_churches, church)) {
                log_string(formatf("Failed to add church %s to list", church->name));
                free_church(church);
                continue;
            }
            
            count++;
        } else {
            free_church(church);
        }
    }
    
    closedir(dir);
    log_string(formatf("Loaded %d churches from individual files", count));
    
    // Sort the list by UID after loading
    if (list_churches && list_size(list_churches) > 1)
        list_quicksort(list_churches, cmp_church_uid);
}


/**
 * read_church - Parse and load a single church from file
 *
 * Reads church data from an open file stream. Handles both legacy
 * format (name first) and new format (#CHURCH header). Parses all
 * church properties, ranks, members, coffer items, and log entries.
 *
 * Creates a new church structure and populates it from file data.
 * Links members to their ranks by UID after loading.
 *
 * @param fp  Open file pointer positioned at church data
 * @return    Allocated and populated CHURCH_DATA, or NULL on error
 */
CHURCH_DATA *read_church(FILE *fp)
{
    char *word;
    CHURCH_DATA *church;
    bool fMatch;
    long default_rank_uid = 0;

    // Skip the initial #CHURCH section header if present
    word = fread_word(fp);
    
    church = new_church();
    church->max_rank_uid = 1000; // Default value for new churches
    church->version = 0; // Default to legacy version

    // Handle legacy format where the first entry is the church name
    if (str_cmp(word, "#CHURCH")) {
        // Put back the word we just read
        //ungetc(' ', fp); // Add a space
        for (int i = strlen(word) - 1; i >= 0; i--) {
            ungetc(word[i], fp);
        }
        
        // Now read the full string which is the church name
        church->name = fread_string(fp);
        fMatch = true;
    }

    for (; ;) {
        if (feof(fp)) {
            log_string(formatf("Unexpected EOF while reading church %s", 
                church->name ? church->name : "unknown"));
            break;
        }

        word = fread_word(fp);
        fMatch = false;

        if (!str_cmp(word, "#-CHURCH"))
            break;

        switch (word[0]) {
            case '#':
if (!str_cmp(word, "#MEMBER")) {
    // Read a member
    CHURCH_PLAYER_DATA *member = read_church_member(fp);
    if (member) {
        // Add to end of the list to preserve file order and avoid self-loop
        member->next = NULL;
        member->church = church;

        if (!church->people) {
            church->people = member;
        } else {
            CHURCH_PLAYER_DATA *last = church->people;
            // Walk to the end, with safety against accidental loops
            int safety = 0;
            while (last->next && safety++ < 1000) {
                if (last->next == last) {
                    log_string("Detected self-referential member in church->people list!");
                    last->next = NULL;
                    break;
                }
                last = last->next;
            }
            last->next = member;
        }

        // Add to roster
        if (!list_appendlink(church->roster, member->name)) {
            log_string(formatf("Failed to add member %s to roster of church %s",
                member->name, church->name ? church->name : "unknown"));
        }
    }
    fMatch = true;
}
                else if (!str_cmp(word, "#COFFER")) {
                    // Read coffer items
                    for (;;) {
                        if (feof(fp)) {
                            log_string("Unexpected EOF in coffer section");
                            break;
                        }
                        
                        word = fread_word(fp);
                        if (!str_cmp(word, "#ENDCOFFER"))
                            break;
                            
                        // Put back the first word for object reading
                        ungetc(' ', fp);
                        for (int i = strlen(word) - 1; i >= 0; i--) {
                            ungetc(word[i], fp);
                        }
                        
                        // Read the object
                        OBJ_DATA *obj = fread_obj_new(fp);
                        if (obj) {
                            obj->next_content = church->coffer;
                            church->coffer = obj;
                        }
                    }
                    fMatch = true;
                }
                else if (!str_cmp(word, "#LOG")) {
                    // Read a log entry
                    CHURCH_LOG_ENTRY *entry = alloc_mem(sizeof(CHURCH_LOG_ENTRY));
                    entry->author = NULL;
                    entry->text = NULL;
                    entry->timestamp = current_time;
                    entry->entry_id = 0;
                    entry->system_generated = false;
                    entry->next = NULL;
                    entry->categories = 0;
                    
                    for (;;) {
                        word = fread_word(fp);
                        
                        if (!str_cmp(word, "#ENDLOG"))
                            break;
                        
                        if (!str_cmp(word, "EntryID"))
                            entry->entry_id = fread_number(fp);
                        else if (!str_cmp(word, "Timestamp"))
                            entry->timestamp = fread_number(fp);
                        else if (!str_cmp(word, "System"))
                            entry->system_generated = (fread_number(fp) == 1);
                        else if (!str_cmp(word, "Author"))
                            entry->author = fread_string(fp);
                        else if (!str_cmp(word, "Text"))
                            entry->text = fread_string(fp);
                        else if (!str_cmp(word, "Categories"))
                            entry->categories = fread_number(fp);
                    }
                    
                    // Add to end of list
                    if (!church->log_entries) {
                        church->log_entries = entry;
                    } else {
                        CHURCH_LOG_ENTRY *temp;
                        for (temp = church->log_entries; temp->next; temp = temp->next)
                            ;
                        temp->next = entry;
                    }
                    
                    // Increment count
                    church->log_entry_count++;
                    
                    // Track highest log entry ID
                    if (entry->entry_id > church->last_log_entry_id) {
                        church->last_log_entry_id = entry->entry_id;
                    }
                    
                    fMatch = true;
                }
                break;

            case 'A':
                KEY("Alignment", church->alignment, fread_number(fp));
                break;

            case 'C':
                KEY("CPKLosses", church->cpk_losses, fread_number(fp));
                KEY("CPKWins", church->cpk_wins, fread_number(fp));
                KEY("Colour1", church->colour1, fread_string(fp));
                KEY("Colour2", church->colour2, fread_string(fp));
                KEY("CofferRent", church->coffer_rent, fread_number(fp));
                KEY("Created", church->created, fread_number(fp));
                break;

            case 'D':
                KEY("DefaultRankUID", default_rank_uid, fread_number(fp));
                // Handle DeityPoints with multiple possible formats
                if (!str_cmp(word, "DeityPoints") || 
                    !str_cmp(word, "DP") || 
                    !str_cmp(word, "Karma")) {
                    church->dp = fread_number(fp);
                    fMatch = true;
                }
                if (!str_cmp(word, "Deleted")) {
                    church->deleted = (fread_number(fp) == 1);
                    fMatch = true;
                }
                break;

            case 'F':
                KEY("Flag", church->flag, fread_string(fp));
                KEY("Founder", church->founder, fread_string(fp));
                KEY("FounderLastLogin", church->founder_last_login, fread_number(fp));
                break;

            case 'G':
                KEY("Gold", church->gold, fread_number(fp));
                break;

            case 'I':
                KEY("Info", church->info, fread_string(fp));
                break;

            case 'K':
                KEY("Key", church->key, fread_number(fp));
                break;

            case 'L':
                KEY("LastLoginFounder", church->founder_last_login, fread_number(fp));
                KEY("LastLoginOwner", church->owner_last_login, fread_number(fp));
                KEY("LastLogID", church->last_log_entry_id, fread_number(fp));
                break;

            case 'M':
                KEY("MaxPositions", church->max_positions, fread_number(fp));
                KEY("Motd", church->motd, fread_string(fp));
                break;

            case 'N':
                KEY("Name", church->name, fread_string(fp));
                KEY("NumRanks", church->num_ranks, fread_number(fp));
                break;
                
            case 'O':
                KEY("Owner", church->owner, fread_string(fp));
                break;

            case 'P':
                KEY("PKLosses", church->pk_losses, fread_number(fp));
                KEY("PKWins", church->pk_wins, fread_number(fp));
                KEY("Pneuma", church->pneuma, fread_number(fp));
                break;

            case 'R':
                KEY("Rules", church->rules, fread_string(fp));
                KEY("RecallPoint", church->recall_point.id[0], fread_number(fp));
                
                if (!str_cmp(word, "Rank")) {
                    CHURCH_RANK_DATA *rank = new_church_rank();
                    
                    // Read all rank data
                    for (;;) {
                        if (feof(fp)) {
                            log_string("Unexpected EOF in rank section");
                            break;
                        }
                        
                        word = fread_word(fp);
                        
                        if (!str_cmp(word, "EndRank"))
                            break;
                            
                        if (!str_cmp(word, "RankUID"))
                            rank->uid = fread_number(fp);
                        else if (!str_cmp(word, "RankName"))
                            rank->rank_name = fread_string(fp);
                        else if (!str_cmp(word, "RankTitleMale"))
                            rank->title_male = fread_string(fp);
                        else if (!str_cmp(word, "RankTitleFemale"))
                            rank->title_female = fread_string(fp);
                        else if (!str_cmp(word, "RankTitleNeutral"))
                            rank->title_neutral = fread_string(fp);
                        else if (!str_cmp(word, "RankType"))
                            rank->rank_type = fread_number(fp);
                        else if (!str_cmp(word, "RankPermissions"))
                            rank->permissions = fread_number(fp);
                        else if (!str_cmp(word, "RankFlags"))
                            rank->flags = fread_number(fp);
                    }
                    
                    // Add to church ranks
                    if (church->ranks == NULL) {
                        church->ranks = rank;
                    } else {
                        CHURCH_RANK_DATA *temp;
                        for (temp = church->ranks; temp->next != NULL; temp = temp->next)
                            ;
                        temp->next = rank;
                    }
                    
                    if (rank->uid > church->max_rank_uid)
                        church->max_rank_uid = rank->uid;
                        
                    fMatch = true;
                }
                break;
 


            case 'S':
                KEY("Settings", church->settings, fread_flag(fp));
                KEY("Size", church->size, fread_number(fp));
                KEY("StoragePerms", church->storage_permissions, fread_number(fp));
                break;

            case 'T':
                KEY("ToggledPK", church->pk, fread_number(fp));
                                if (!str_cmp(word, "TreasureRoom")) {
                    long vnum = fread_number(fp);
                    AREA_DATA *room_area = find_area_by_vnum(vnum, NULL);
                    if (!room_area) room_area = get_system_area_fallback();
                    ROOM_INDEX_DATA *room = get_room_index(room_area, vnum);
                    bool is_default = (fread_number(fp) == 1);
                    
                    if (room) {
                        CHURCH_TREASURE_ROOM *treasure = create_church_treasure_room(church, room, is_default);
                        if (!treasure) {
                            log_string(formatf("Failed to create treasure room for church %s",
                                church->name ? church->name : "unknown"));
                        }
                    } else {
                        log_string(formatf("Warning: Church %s references unknown treasure room %ld",
                            church->name ? church->name : "unknown", vnum));
                    }
                    fMatch = true;
                }
                                if (!str_cmp(word, "TreasureVRoom")) {
                    WILDS_DATA *wilds;
                    ROOM_INDEX_DATA *room;
                    int x, y, z;
                    
                    wilds = get_wilds_from_uid(NULL, fread_number(fp));
                    x = fread_number(fp);
                    y = fread_number(fp);
                    z = fread_number(fp);
                    bool is_default = (fread_number(fp) == 1);
                    
                    room = get_wilds_vroom(wilds, x, y);
                    if (!room) {
                        room = create_wilds_vroom(wilds, x, y);
                    }
                    
                    if (room) {
                        CHURCH_TREASURE_ROOM *treasure = create_church_treasure_room(church, room, is_default);
                        if (!treasure) {
                            log_string(formatf("Failed to create wilderness treasure room for church %s",
                                church->name ? church->name : "unknown"));
                        }
                    } else {
                        log_string(formatf("Warning: Church %s references invalid wilderness room (%ld,%d,%d,%d)",
                            church->name ? church->name : "unknown", 
                            wilds ? wilds->uid : 0, x, y, z));
                    }
                    fMatch = true;
                }
                
                if (!str_cmp(word, "TreasureAccess")) {
                    int rank_index = fread_number(fp);
                    
                    // Get the last added treasure room
                    CHURCH_TREASURE_ROOM *treasure = NULL;
                    if (list_size(church->treasure_rooms) > 0) {
                        ITERATOR it;

                        iterator_start(&it, church->treasure_rooms);
                        while (iterator_hasdata(&it)) {
                            treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it);
                        }
                        iterator_stop(&it);
                    }
                    
                    // Get the rank at the given index
                    if (treasure) {
                        CHURCH_RANK_DATA *rank = church->ranks;
                        for (int i = 0; i < rank_index && rank; i++, rank = rank->next)
                            ;
                        
                        if (rank) {
                            add_rank_to_treasure_room(treasure, rank);
                        } else {
                            log_string(formatf("Warning: Church %s references invalid rank index %d",
                                church->name ? church->name : "unknown", rank_index));
                        }
                    }
                    
                    fMatch = true;
                }
                break;

            case 'U':
                KEY("UID", church->uid, fread_number(fp));
                break;

            case 'V':
                KEY("Version", church->version, fread_number(fp));
                break;

            case 'W':
                KEY("WarsWon", church->wars_won, fread_number(fp));
                break;
        }
        
        if (!fMatch) {
            log_string(formatf("read_church: no match for word '%s' in %s",
                word, church->name ? church->name : "unknown church"));
            fread_to_eol(fp);
        }
    }

    // Perform post-processing
    if (church->deleted) {
        log_string(formatf("Church %s is marked as deleted", church->name));
    }

    // Force their max positions to meet the minimum requirements, if lower
    int min_positions = church_get_min_positions(church->size);
    if (church->max_positions < min_positions) {
        log_string(formatf("Church %s max positions %d adjusted to minimum %d",
            church->name, church->max_positions, min_positions));
        church->max_positions = min_positions;
    }

    if (church->info == NULL) {
        church->info = str_dup("No info set.~");
    }

    // Handle default rank assignment
    if (default_rank_uid > 0) {
        CHURCH_RANK_DATA *rank;
        for (rank = church->ranks; rank; rank = rank->next) {
            if (rank->uid == default_rank_uid) {
                church->default_rank = rank;
                break;
            }
        }
    }
    
    // If no default rank was set or found, set it to the lowest non-leader rank
    if (!church->default_rank) {
        CHURCH_RANK_DATA *best_rank = NULL;
        CHURCH_RANK_DATA *rank;
        
        for (rank = church->ranks; rank; rank = rank->next) {
            if (rank->rank_type < RANK_TYPE_LEADER &&
                (!best_rank || rank->rank_type < best_rank->rank_type)) {
                best_rank = rank;
            }
        }
        
        church->default_rank = best_rank;
    }

    // Validate that all ranks have a uid and update max_rank_uid if needed
    CHURCH_RANK_DATA *rank;
    for (rank = church->ranks; rank; rank = rank->next) {
        if (rank->uid == 0) {
            rank->uid = ++church->max_rank_uid;
        } else if (rank->uid > church->max_rank_uid) {
            church->max_rank_uid = rank->uid;
        }
        
        // Ensure that all leader ranks have ALL permissions
        // This guarantees leaders get new permissions automatically when they're added to the system
        if (rank->rank_type == RANK_TYPE_LEADER || rank->uid == 1) {
            rank->permissions = -1; // All bits set
        }
    }
    
    // Assign ranks to members that need them
    CHURCH_PLAYER_DATA *member;
    for (member = church->people; member; member = member->next) {
        // If member has no rank assigned or the rank UID doesn't exist
        if (!member->rank) {
            bool found = false;
            long uid = member->rank_uid;
            
            // If we have a UID, try to find the matching rank
            if (uid > 0) {
                for (rank = church->ranks; rank; rank = rank->next) {
                    if (rank->uid == uid) {
                        member->rank = rank;
                        found = true;
                        break;
                    }
                }
            }
            
            // If no rank found, assign the default rank
            if (!found && church->default_rank) {
                member->rank = church->default_rank;
            }
        }
    }
    
    // Ensure church has an owner - default to founder if not set
    if (!church->owner || church->owner[0] == '\0') {
        if (church->founder && church->founder[0] != '\0') {
            church->owner = str_dup(church->founder);
            log_string(formatf("Church %s: Set missing owner to founder %s",
                church->name ? church->name : "unknown", church->founder));
        } else {
            // If no founder either, this is a serious data issue
            log_string(formatf("WARNING: Church %s has no owner or founder defined",
                church->name ? church->name : "unknown"));
        }
    }
    
    if (church->uid == 0)
        get_church_id(church);
        
    // Fix dynamic variables
    variable_dynamic_fix_church(church);

    // Handle legacy churches without ranks
    if (!church->ranks && (church->version < VERSION_CHURCH_001 || church->version == 0)) {
        // Convert old church to new rank system
        convert_church_ranks(church);
        church->version = VERSION_CHURCH; // Update to current version
    } else if (church->num_ranks < 2) {
        // Ensure we have at least 2 ranks
        upgrade_church_ranks(church);
    }

    // Set up log entry tracking
    if (church->log_entry_count > 0) {
        // If we have log entries but no valid highest ID, find it
        if (church->last_log_entry_id == 0) {
            CHURCH_LOG_ENTRY *entry;
            for (entry = church->log_entries; entry; entry = entry->next) {
                if (entry->entry_id > church->last_log_entry_id) {
                    church->last_log_entry_id = entry->entry_id;
                }
            }
        }
    }
    
    return church;
}


/**
 * read_church_member - Parse a single member entry from church file
 *
 * Reads member data from a #MEMBER section. Handles both legacy rank
 * numbers (0-3) and new rank UIDs. Member name is read from section header.
 *
 * @param fp  File pointer positioned after #MEMBER keyword
 * @return    Allocated CHURCH_PLAYER_DATA with parsed values
 */
CHURCH_PLAYER_DATA *read_church_member(FILE *fp)
{
    CHURCH_PLAYER_DATA *member;
    char *word;
    bool fMatch;
    char *name = fread_string(fp);

    member = new_church_player();
    member->name = name;
    
    for (;;) {
        word = fread_word(fp);
        fMatch = false;
        
        if (!str_cmp(word, "#-MEMBER"))
            break;
            
        switch (UPPER(word[0])) {
            case 'A':
                KEY("Alignment", member->alignment, fread_number(fp));
                break;
                
            case 'C':
                KEY("CPKLosses", member->cpk_losses, fread_number(fp));
                KEY("CPKWins", member->cpk_wins, fread_number(fp));
                break;
                
            case 'D':
                KEY("DepositedDp", member->dep_dp, fread_number(fp));
                KEY("DepositedGold", member->dep_gold, fread_number(fp));
                KEY("DepositedPneuma", member->dep_pneuma, fread_number(fp));
                break;
                
            case 'F':
                KEY("Flags", member->flags, fread_number(fp));
                break;
                
            case 'P':
                KEY("PersonalPerms", member->personal_permissions, fread_number(fp));
                KEY("PKLosses", member->pk_losses, fread_number(fp));
                KEY("PKWins", member->pk_wins, fread_number(fp));
                break;
                
            case 'R':
                if (!str_cmp(word, "Rank")) {
                    // Legacy 0-3 rank system 
                    int rank_num = fread_number(fp);
                    member->old_rank = rank_num;
                    fMatch = true;
                }
                KEY("RankUID", member->rank_uid, fread_number(fp));
                break;
                
            case 'S':
                KEY("Sex", member->sex, fread_number(fp));
                break;
                
            case 'W':
                KEY("WarsWon", member->wars_won, fread_number(fp));
                break;
        }
        
        if (!fMatch) {
            log_string(formatf("read_church_member: no match for word '%s' in member %s",
                word, member->name));
            fread_to_eol(fp);
        }
    }
    
    return member;
}


/**
 * is_in_treasure_room - Check if an object is in any church treasure room
 *
 * @param obj  Object to check
 * @return     true if object's room is a treasure room, false otherwise
 */
bool is_in_treasure_room(OBJ_DATA *obj)
{
    ROOM_INDEX_DATA *room = obj->in_room;

    if (room == NULL)
        return false;

    return is_treasure_room(NULL, room);
}

/**
 * vnum_in_treasure_room - Check if object with given vnum exists in treasure rooms
 *
 * Searches all treasure rooms of the church for any object matching
 * the specified vnum.
 *
 * @param church  Church to search
 * @param vnum    Object vnum to look for
 * @return        true if found, false otherwise
 */
bool vnum_in_treasure_room(CHURCH_DATA *church, long vnum)
{
    CHURCH_TREASURE_ROOM *treasure;
    OBJ_DATA *obj = NULL;
    ITERATOR rit, oit;

    iterator_start(&rit, church->treasure_rooms);
    while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&rit)) && !obj) {
        iterator_start(&oit, treasure->room->lcontents);
        while( (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
            if( obj->pIndexData->vnum == vnum )
                break;
        }
        iterator_stop(&oit);
    }
    iterator_stop(&rit);

    return obj && true;
}


/**
 * update_church_pks - Swap PK and CPK (chaotic PK) statistics
 *
 * Swaps the pk_wins/losses with cpk_wins/losses for all churches.
 * Called periodically to rotate statistics between regular and
 * chaotic PK tracking periods.
 */
void update_church_pks(void)
{
    CHURCH_DATA *church;
    int pk_wins;
    int pk_losses;
    ITERATOR it;

    log_string("update_church_pks: updating church PKs...");
    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it))) {
        pk_wins = church->cpk_wins;
        pk_losses = church->cpk_losses;

        church->cpk_wins = church->pk_wins;
        church->cpk_losses = church->pk_losses;
        church->pk_wins = pk_wins;
        church->pk_losses = pk_losses;
    }
    iterator_stop(&it);
}


/**
 * is_excommunicated - Check if character is excommunicated from their church
 *
 * @param ch  Character to check
 * @return    true if excommunicated, false if not in church or not excommunicated
 */
bool is_excommunicated(CHAR_DATA *ch)
{
    if (ch->church == NULL || ch->church_member == NULL)
        return false;

    return IS_SET(ch->church_member->flags, CHURCH_PLAYER_EXCOMMUNICATED);
}

/**
 * church_add_treasure_room - Add a treasure room to a church
 *
 * Creates a new treasure room entry for the church. Default rooms are
 * accessible by all members. Non-default rooms automatically grant
 * access to leader and officer ranks.
 *
 * @param church     Church to add room to
 * @param room       Room to designate as treasure room
 * @param is_default true for universal access, false for rank-restricted
 * @return           true on success, false on failure
 */
bool church_add_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room, bool is_default)
{
    if (!church || !room)
        return false;
        
    CHURCH_TREASURE_ROOM *treasure = create_church_treasure_room(church, room, is_default);
    
    if (!treasure)
        return false;
    
    // If not default, then add the leader rank to allowed ranks
    if (!is_default) {
        CHURCH_RANK_DATA *rank;
        for (rank = church->ranks; rank; rank = rank->next) {
            if (rank->rank_type == RANK_TYPE_LEADER || 
                rank->rank_type == RANK_TYPE_OFFICER) {
                add_rank_to_treasure_room(treasure, rank);
            }
        }
    }
    
    return true;
}

/**
 * church_remove_treasure_room - Remove a treasure room from a church
 *
 * Finds and removes the treasure room entry for the given room.
 * Cleans up the allowed_ranks list and frees associated memory.
 *
 * @param church  Church to remove room from
 * @param room    Room to remove from treasure rooms
 */
void church_remove_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room)
{
    if (!church || !room)
        return;
        
    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;

    iterator_start(&it, church->treasure_rooms);
    while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        if (treasure->room == room)
            break;
    }
    iterator_stop(&it);

    if (treasure != NULL) {
        // Clean up the allowed_ranks list if it exists
        if (treasure->allowed_ranks) {
            list_destroy(treasure->allowed_ranks);
            treasure->allowed_ranks = NULL;
        }
        
        // Free the name if it exists
        if (treasure->name) {
            free_string(treasure->name);
            treasure->name = NULL;
        }
        
        list_remlink(church->treasure_rooms, treasure, false);
        free_mem(treasure, sizeof(CHURCH_TREASURE_ROOM));
    }
}

/**
 * get_church_treasure_room - Get the nth accessible treasure room
 *
 * Returns the nth treasure room that the character can access.
 * If ch is NULL, returns the nth room overall. Skips rooms the
 * character doesn't have permission to access.
 *
 * @param ch      Character checking access (or NULL for no access check)
 * @param church  Church to search
 * @param nth     1-based index of room to retrieve (among accessible rooms)
 * @return        Treasure room pointer or NULL if not found/accessible
 */
CHURCH_TREASURE_ROOM *get_church_treasure_room(CHAR_DATA *ch, CHURCH_DATA *church, int nth)
{
    if (ch != NULL) {
        if (IS_NPC(ch) || ch->church != church || is_excommunicated(ch))
            return NULL;
    }

    if (nth < 1)
        return NULL;

    CHURCH_TREASURE_ROOM *treasure = NULL;
    ITERATOR it;
    int found_count = 0;

    iterator_start(&it, church->treasure_rooms);
    while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        bool can_access = false;
        
        if (ch == NULL) {
            // If no character provided, count all rooms
            can_access = true;
        } else if (can_access_treasure_room(ch->church_member, treasure)) {
            can_access = true;
        }
        
        if (can_access && ++found_count == nth) {
            break;
        }
    }
    iterator_stop(&it);

    return treasure;
}




/**
 * church_set_treasure_room_rank - Set minimum rank for treasure room access
 *
 * @param church    Church owning the treasure room
 * @param nth       1-based index of treasure room
 * @param min_rank  Minimum rank type required for access
 * @return          true if room found and updated, false otherwise
 */
bool church_set_treasure_room_rank(CHURCH_DATA *church, int nth, int min_rank)
{
    if( nth < 1 ) return false;

    CHURCH_TREASURE_ROOM *treasure;
    ITERATOR it;

    iterator_start(&it, church->treasure_rooms);
    while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        if( !--nth)
        {
            treasure->min_rank = min_rank;
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

/**
 * church_available_treasure_rooms - Count treasure rooms accessible to character
 *
 * Returns the number of treasure rooms the character can access.
 * Leaders and those with TREASURE_ALL permission can access all rooms.
 *
 * @param ch  Character to check
 * @return    Number of accessible treasure rooms, 0 if invalid/excommunicated
 */
int church_available_treasure_rooms(CHAR_DATA *ch)
{
    if (ch == NULL || IS_NPC(ch) || ch->church == NULL || is_excommunicated(ch))
        return 0;

    CHURCH_TREASURE_ROOM *treasure = NULL;
    ITERATOR it;
    int count = 0;

    // Leaders can access all rooms
    if (ch->church_member->rank->rank_type == RANK_TYPE_LEADER ||
        has_church_permission(ch->church_member, CHURCH_PERM_TREASURE_ALL)) {
        return list_size(ch->church->treasure_rooms);
    }

    // Count rooms player can access
    iterator_start(&it, ch->church->treasure_rooms);
    while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it))) {
        if (can_access_treasure_room(ch->church_member, treasure)) {
            count++;
        }
    }
    iterator_stop(&it);

    return count;
}

/**
 * do_chtreasure - Manage and view church treasure rooms
 *
 * Subcommands:
 * - list: Show all treasure rooms accessible to the character
 * - access <room#> <rank#> [add|remove]: Manage rank access to rooms
 *   (leaders only)
 *
 * Excommunicated members cannot access this command.
 *
 * @param ch        Character using the command
 * @param argument  Subcommand and arguments
 */
void do_chtreasure(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *temp_char;
    char buf[MAX_STRING_LENGTH];
    char arg[MIL];
    char arg2[MIL];
    char arg3[MIL];
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL)
    {
        send_to_char("You aren't in a registered group.\n\r", ch);
        return;
    }

    if (is_excommunicated(ch))
    {
        send_to_char("You have been excommunicated. That information is forbidden.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("CHURCH TREASURE LIST\n\r", ch);
        if (is_church_leader(ch, ch->church))
            send_to_char("                ACCESS <room#> <rank#> [add|remove]\n\r", ch);
        return;
    }

    if(!str_cmp(arg, "list"))
    {
        CHURCH_TREASURE_ROOM *treasure;
        ITERATOR it;
        bool skipped = false;
        int i = 0;
        
        iterator_start(&it, ch->church->treasure_rooms);
        while ((treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)))
        {
            // Check if player can access this treasure room
            if (!can_access_treasure_room(ch->church_member, treasure))
            {
                skipped = true;
                continue;
            }

            if (i == 0) {
                sprintf(buf, "{YTreasure rooms for {W%s{Y:\n\r", ch->church->name);
                send_to_char(buf, ch);
                send_to_char("{Y========================================================{x\n\r", ch);
            }

            i++;
            sprintf(buf, "%2d [{W%s{x] {Y%s{x\n\r", i, 
                    treasure->is_default ? "DEFAULT" : "SECURED",
                    treasure->room->name);
            send_to_char(buf, ch);
            
            // Show access list for leaders
            if (is_church_leader(ch, ch->church) ||
                has_church_permission(ch->church_member, CHURCH_PERM_TREASURE_ALL)) {
                
                CHURCH_RANK_DATA *rank;
                ITERATOR rank_it;
                bool has_ranks = false;
                
                send_to_char("  {CAccess: ", ch);
                
                if (treasure->is_default) {
                    send_to_char("{WAll members have access{x\n\r", ch);
                } else {
                    iterator_start(&rank_it, treasure->allowed_ranks);
                    while ((rank = (CHURCH_RANK_DATA *)iterator_nextdata(&rank_it))) {
                        sprintf(buf, "%s{W%s{x ", 
                                has_ranks ? ", " : "", rank->title_male);
                        send_to_char(buf, ch);
                        has_ranks = true;
                    }
                    iterator_stop(&rank_it);
                    
                    if (!has_ranks) {
                        send_to_char("{RNo ranks have access!{x", ch);
                    }
                    send_to_char("\n\r", ch);
                }
            }
        }
        iterator_stop(&it);

        if (i == 0) {
            if (skipped)
                send_to_char("There are no treasure rooms available to you.\n\r", ch);
            else
                send_to_char("Your church has no treasure rooms yet.\n\r", ch);
        }

        return;
    }

    if(!str_cmp(arg, "access"))
    {
        if (!is_church_leader(ch, ch->church) &&
            !has_church_permission(ch->church_member, CHURCH_PERM_TREASURE_ALL))
        {
            send_to_char("Only church leaders can manage treasure room access.\n\r", ch);
            return;
        }

        found = false;
        for (temp_char = ch->in_room->people; temp_char != NULL; temp_char = temp_char->next_in_room)
        {
            if (IS_NPC(temp_char) && IS_SET(temp_char->act[1], ACT2_CHURCHMASTER))
                found = true;
        }

        if (!found)
        {
            send_to_char("You must be at an administration office.\n\r", ch);
            return;
        }

        if (arg2[0] == '\0' || !is_number(arg2))
        {
            send_to_char("Syntax: church treasure access <room#> <rank#> [add|remove]\n\r", ch);
            return;
        }

        int roomno = atoi(arg2);
        if (roomno < 1 || roomno > list_size(ch->church->treasure_rooms))
        {
            send_to_char("Room number is out of range.\n\r", ch);
            return;
        }

        CHURCH_TREASURE_ROOM *treasure = get_church_treasure_room(NULL, ch->church, roomno);
        if (!treasure)
        {
            send_to_char("Could not find the specified treasure room.\n\r", ch);
            return;
        }

        if (treasure->is_default)
        {
            send_to_char("You cannot modify access for the default room - all members already have access.\n\r", ch);
            return;
        }

        // Just show access list if no rank specified
        if (arg3[0] == '\0' || !is_number(arg3)) {
            CHURCH_RANK_DATA *rank;
            ITERATOR rank_it;
            int rank_count = 0;
            
            send_to_char("Ranks with access to this treasure room:\n\r", ch);
            send_to_char("----------------------------------------\n\r", ch);
            
            iterator_start(&rank_it, treasure->allowed_ranks);
            while ((rank = (CHURCH_RANK_DATA *)iterator_nextdata(&rank_it))) {
                rank_count++;
                sprintf(buf, "%2d. {W%s{x\n\r", rank_count, rank->title_male);
                send_to_char(buf, ch);
            }
            iterator_stop(&rank_it);
            
            if (rank_count == 0) {
                send_to_char("No ranks have been granted access yet.\n\r", ch);
            }
            
            send_to_char("\n\rUse 'church treasure access <room#> <rank#> add' to grant access.\n\r", ch);
            return;
        }

        int rank_num = atoi(arg3);
        CHURCH_RANK_DATA *rank = ch->church->ranks;
        for (int i = 1; rank && i < rank_num; i++)
            rank = rank->next;
            
        if (!rank) {
            sprintf(buf, "Rank must be between 1 and %d.\n\r", ch->church->num_ranks);
            send_to_char(buf, ch);
            return;
        }

        // Default is to add
        bool should_add = true;
        
        // Check for add/remove argument
        if (argument[0] != '\0') {
            if (!str_prefix(argument, "remove")) {
                should_add = false;
            } else if (!str_prefix(argument, "add")) {
                should_add = true;
            } else {
                send_to_char("Specify 'add' or 'remove' to manage access.\n\r", ch);
                return;
            }
        }
        
        // Add or remove the rank
        if (should_add) {
            if (add_rank_to_treasure_room(treasure, rank)) {
                sprintf(buf, "Rank '%s' now has access to room %d.\n\r", 
                        rank->title_male, roomno);
                send_to_char(buf, ch);
                
                sprintf(buf, "%s granted '%s' rank access to treasure room %s.",
                        ch->name, rank->title_male, treasure->room->name);
                        add_church_log_entry(ch->church, ch->name, buf, CHLOG_TREASURE, true);
            } else {
                send_to_char("Failed to grant access.\n\r", ch);
            }
        } else {
            if (remove_rank_from_treasure_room(treasure, rank)) {
                sprintf(buf, "Rank '%s' no longer has access to room %d.\n\r", 
                        rank->title_male, roomno);
                send_to_char(buf, ch);
                
                sprintf(buf, "%s removed '%s' rank access from treasure room %s.",
                        ch->name, rank->title_male, treasure->room->name);
                add_church_log_entry(ch->church, ch->name, buf, CHLOG_TREASURE, true);

            } else {
                send_to_char("Failed to remove access or rank didn't have access.\n\r", ch);
            }
        }

        save_church(ch->church);
        return;
    }

    send_to_char("CHURCH TREASURE LIST\n\r", ch);
    if (is_church_leader(ch, ch->church))
        send_to_char("                ACCESS <room#> <rank#> [add|remove]\n\r", ch);
    return;
}


/**
 * church_announce_theft - Broadcast theft from church treasure room globally
 *
 * Called when someone takes an item from a treasure room they don't
 * belong to (or are excommunicated from). Announces the theft to all
 * online players.
 *
 * @param ch   Character who took the item
 * @param obj  Object that was taken, or NULL for generic message
 */
void church_announce_theft(CHAR_DATA *ch, OBJ_DATA *obj)
{
    char buf[MAX_STRING_LENGTH];
    CHURCH_DATA *church;
    ITERATOR it;

    iterator_start(&it, list_churches);
    while ((church = (CHURCH_DATA *)iterator_nextdata(&it))) {
        if ((ch->church != church || is_excommunicated(ch)) && is_treasure_room(church, ch->in_room)) {
            if (obj != NULL) {
                sprintf(buf, "{Y%s has stolen %s from a %s treasure room!{x\n\r", ch->name, obj->short_descr, church->name);
            } else {
                sprintf(buf, "{Y%s has stolen from a %s treasure room!{x\n\r", ch->name, church->name);
            }
            gecho(buf);
        }
    }
    iterator_stop(&it);
}

/**
 * has_church_permission - Check if a church member has a specific permission
 *
 * Permission hierarchy:
 * 1. Owners always have all permissions
 * 2. Leaders (RANK_TYPE_LEADER) always have all permissions
 * 3. CHURCH_PERM_NONE always returns true
 * 4. Personal permissions granted to the member
 * 5. Permissions granted to the member's rank
 *
 * @param member      Church member to check
 * @param permission  CHURCH_PERM_* flag to check
 * @return            true if permission granted, false otherwise
 */
bool has_church_permission(CHURCH_PLAYER_DATA *member, long permission)
{
    if (!member || !member->church || !member->rank)
        return false;

    // Owner always has all permissions
    if (member->church->owner && !str_cmp(member->name, member->church->owner))
        return true;

    // Leaders always have all permissions
    if (member->rank->rank_type == RANK_TYPE_LEADER)
        return true;

    // No permission required
    if (permission == CHURCH_PERM_NONE)
        return true;

    // Check if the member has been personally granted this permission
    if (IS_SET(member->personal_permissions, permission))
        return true;

    // Check if the permission is granted to this rank
    if (IS_SET(member->rank->permissions, permission))
        return true;

    return false;
}

/**
 * is_church_leader - Check if character is a church leader
 *
 * @param ch      Character to check
 * @param church  Church to check membership in
 * @return        true if member has RANK_TYPE_LEADER, false otherwise
 */
bool is_church_leader(CHAR_DATA *ch, CHURCH_DATA *church)
{
    if (!ch || !ch->church_member || ch->church != church)
        return false;

    return (ch->church_member->rank->rank_type == RANK_TYPE_LEADER);
}


/**
 * is_church_officer - Check if character is a church officer or higher
 *
 * @param ch      Character to check
 * @param church  Church to check membership in
 * @return        true if officer or leader rank type
 */
bool is_church_officer(CHAR_DATA *ch, CHURCH_DATA *church)
{
    if (!ch || !ch->church_member || ch->church != church)
        return false;

    return (ch->church_member->rank->rank_type >= RANK_TYPE_OFFICER);
}


/**
 * do_chsetrank - Set gendered titles for a church rank
 *
 * Allows church leaders to customize male/female/neutral titles
 * for each rank position.
 *
 * TODO: This command's sex-based title system needs to be reworked
 * to use the pronoun system. Consider replacing with a single title
 * or pronoun-aware title formatting.
 *
 * Syntax: church setrank <rank#> <male|female|neutral> <name>
 *
 * @param ch        Church leader setting the rank title
 * @param argument  Rank number, gender, and new title
 */
void do_chsetrank(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }
    
    if (!is_church_leader(ch, ch->church) && !has_church_permission(ch->church_member, CHURCH_PERM_RANKS)) {
        send_to_char("Only church leaders can set rank names.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0') {
        send_to_char("Syntax: church setrank <rank#> <male/female/neutral> <name>\n\r", ch);
        send_to_char("Example: church setrank 1 male Knight\n\r", ch);
        send_to_char("Example: church setrank 1 female Dame\n\r", ch);
        
        // Show current rank names
        send_to_char("\n\rCurrent rank names:\n\r", ch);
        
        int i = 1;
        CHURCH_RANK_DATA *rank;
        for (rank = ch->church->ranks; rank; rank = rank->next, i++) {
            sprintf(buf, "Rank %d: Male=%s, Female=%s, Neutral=%s (%s)\n\r", 
                i, 
                rank->title_male ? rank->title_male : "(default)",
                rank->title_female ? rank->title_female : "(default)",
                rank->title_neutral ? rank->title_neutral : "(default)",
                rank->rank_type == RANK_TYPE_LEADER ? "Leader" : 
                rank->rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member");
            send_to_char(buf, ch);
        }
        
        return;
    }
    
    // Get rank number (1-based for user, 0-based internally)
    int rank_num = atoi(arg1);
    if (rank_num < 1) {
        send_to_char("Invalid rank number.\n\r", ch);
        return;
    }
    
    // Find the rank by position in the linked list
    CHURCH_RANK_DATA *rank = NULL;
    int i = 1;
    
    for (rank = ch->church->ranks; rank; rank = rank->next, i++) {
        if (i == rank_num)
            break;
    }
    
    if (!rank) {
        sprintf(buf, "Rank must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Determine gender
    int gender;
    if (!str_prefix(arg2, "male"))
        gender = 0;
    else if (!str_prefix(arg2, "female"))
        gender = 1;
    else if (!str_prefix(arg2, "neutral"))
        gender = 2;
    else {
        send_to_char("Specify 'male', 'female', or 'neutral'.\n\r", ch);
        return;
    }
    
    // Set the new rank name
    if (gender == 0) {
        if (rank->title_male)
            free_string(rank->title_male);
        rank->title_male = str_dup(arg3);
    } else if (gender == 1) {
        if (rank->title_female)
            free_string(rank->title_female);
        rank->title_female = str_dup(arg3);
    } else {
        if (rank->title_neutral)
            free_string(rank->title_neutral);
        rank->title_neutral = str_dup(arg3);
    }
    
    sprintf(buf, "Rank %d %s title set to '%s'.\n\r", 
            rank_num, 
            gender == 0 ? "male" : (gender == 1 ? "female" : "neutral"), 
            arg3);
    send_to_char(buf, ch);
    
    // Log the change
    sprintf(buf, "%s changed the %s title for rank %d to '%s'.",
            ch->name, 
            gender == 0 ? "male" : (gender == 1 ? "female" : "neutral"), 
            rank_num, 
            arg3);
            add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);
    
    save_church(ch->church);
}

/**
 * do_chpermission - View and manage church rank permissions
 *
 * Subcommands:
 * - (no args): Show own rank and personal permissions
 * - <rank#>: Show permissions for specific rank
 * - <name>: Show permissions for a specific member
 * - <rank#> <permission> [on|off]: Toggle or set a permission for a rank
 *
 * Protected ranks cannot have permissions changed. Leaders always have
 * all permissions.
 *
 * @param ch        Character using the command
 * @param argument  Rank number or member name, optional permission and state
 */
void do_chpermission(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }

    if (!is_church_leader(ch, ch->church) &&
        !has_church_permission(ch->church_member, CHURCH_PERM_PERMS)) {
        send_to_char("Only church leaders can set rank permissions.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0') {
        // Show only the user's current rank permissions and personal permissions
        CHURCH_RANK_DATA *rank = ch->church_member->rank;
        
        send_to_char("Your Current Permissions:\n\r", ch);
        send_to_char("-----------------------\n\r", ch);
        
        // Show personal permissions first
        send_to_char("{YPersonal permissions:{x\n\r", ch);
        bool has_personal = false;
        for (int i = 0; church_permission_flags[i].name; i++) {
            if (church_permission_flags[i].settable && 
                IS_SET(ch->church_member->personal_permissions, church_permission_flags[i].bit)) {
                sprintf(buf, "  %-20s: YES\n\r", church_permission_flags[i].name);
                send_to_char(buf, ch);
                has_personal = true;
            }
        }
        
        if (!has_personal) {
            send_to_char("  No personal permissions granted.\n\r", ch);
        }
        
        // Then show rank permissions
        sprintf(buf, "\n\r{YPermissions from your rank (%s):{x\n\r", rank->title_male);
        send_to_char(buf, ch);
        
        for (int j = 0; church_permission_flags[j].name; j++) {
            if (church_permission_flags[j].settable) {
                sprintf(buf, "  %-20s: %s\n\r", 
                        church_permission_flags[j].name,
                        IS_SET(rank->permissions, church_permission_flags[j].bit) ? 
                        "YES" : "NO");
                send_to_char(buf, ch);
            }
        }
        
        // Show usage info
        send_to_char("\n\rTo view permissions for a specific rank: church permission <rank#>\n\r", ch);
        send_to_char("To view permissions for a member: church permission <name>\n\r", ch);
        send_to_char("To change permissions: church permission <rank#> <permission> [on|off]\n\r", ch);
        
        return;
    }
    
    // Check if arg1 is a rank number
    if (is_number(arg1)) {
        int rank_num = atoi(arg1);
        if (rank_num < 1) {
            send_to_char("Invalid rank number.\n\r", ch);
            return;
        }
        
        CHURCH_RANK_DATA *rank = ch->church->ranks;
        for (int i = 1; rank && i < rank_num; i++)
            rank = rank->next;
            
        if (!rank) {
            sprintf(buf, "Rank must be between 1 and %d.\n\r", ch->church->num_ranks);
            send_to_char(buf, ch);
            return;
        }
        
        // If no second argument, just display the rank's permissions
        if (arg2[0] == '\0') {
            sprintf(buf, "Permissions for rank '%s':\n\r", rank->title_male);
            send_to_char(buf, ch);
            
            for (int i = 0; church_permission_flags[i].name; i++) {
                if (church_permission_flags[i].settable) {
                    sprintf(buf, "  %-20s: %s\n\r", 
                            church_permission_flags[i].name,
                            IS_SET(rank->permissions, church_permission_flags[i].bit) ? 
                            "YES" : "NO");
                    send_to_char(buf, ch);
                }
            }
            
            return;
        }
    

        if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED)) {
            send_to_char("That rank's permissions are protected and cannot be changed.\n\r", ch);
            return;
        }
        
        // Toggle a permission
        int permission = flag_value(church_permission_flags, arg2);
        if (permission == NO_FLAG) {
            send_to_char("Unknown permission. For a list of permissions, type 'church permission'\n\r", ch);
            return;
        }
        
        // Toggle or set the permission
        if (argument[0] == '\0') {
            // Toggle
            if (IS_SET(rank->permissions, permission)) {
                REMOVE_BIT(rank->permissions, permission);
                sprintf(buf, "Permission %s removed from rank '%s'.\n\r", arg2, rank->title_male);
            } else {
                SET_BIT(rank->permissions, permission);
                sprintf(buf, "Permission %s granted to rank '%s'.\n\r", arg2, rank->title_male);
            }
        } else if (!str_prefix(argument, "on")) {
            SET_BIT(rank->permissions, permission);
            sprintf(buf, "Permission %s granted to rank '%s'.\n\r", arg2, rank->title_male);
        } else if (!str_prefix(argument, "off")) {
            REMOVE_BIT(rank->permissions, permission);
            sprintf(buf, "Permission %s removed from rank '%s'.\n\r", arg2, rank->title_male);
        } else {
            send_to_char("Specify 'on' or 'off' to set permission.\n\r", ch);
            return;
        }
        
        send_to_char(buf, ch);
        
        // Log the change
        sprintf(buf, "%s changed permission %s for rank '%s' to %s.",
            ch->name, arg2, rank->title_male,
            IS_SET(rank->permissions, permission) ? "ON" : "OFF");
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_PERMISSIONS, true);        
        save_church(ch->church);
        return;
    } else {
        // Check if arg1 is a member name
        CHURCH_PLAYER_DATA *member = NULL;
        for (member = ch->church->people; member != NULL; member = member->next) {
            if (!str_cmp(member->name, arg1))
                break;
        }
        
        if (member) {
            sprintf(buf, "Permissions for %s:\n\r", member->name);
            send_to_char(buf, ch);
            send_to_char("-----------------------\n\r", ch);
            
            // Show personal permissions
            send_to_char("{YPersonal permissions:{x\n\r", ch);
            bool has_personal = false;
            for (int i = 0; church_permission_flags[i].name; i++) {
                if (church_permission_flags[i].settable && 
                    IS_SET(member->personal_permissions, church_permission_flags[i].bit)) {
                    sprintf(buf, "  %-20s: YES\n\r", church_permission_flags[i].name);
                    send_to_char(buf, ch);
                    has_personal = true;
                }
            }
            
            if (!has_personal) {
                send_to_char("  No personal permissions granted.\n\r", ch);
            }
            
            // Show rank permissions
            sprintf(buf, "\n\r{YPermissions from rank (%s):{x\n\r", 
                  member->rank->title_male);
            send_to_char(buf, ch);
            
            for (int j = 0; church_permission_flags[j].name; j++) {
                if (church_permission_flags[j].settable) {
                    sprintf(buf, "  %-20s: %s\n\r", 
                        church_permission_flags[j].name,
                        IS_SET(member->rank->permissions, church_permission_flags[j].bit) ? 
                        "YES" : "NO");
                    send_to_char(buf, ch);
                }
            }
            return;
        } else {
            // Unknown permission or member name
            int permission = flag_value(church_permission_flags, arg1);
            if (permission == NO_FLAG) {
                send_to_char("Unknown permission or member name.\n\r", ch);
                return;
            }
            
            // Continue with existing code for permission setting
            send_to_char("You must specify a rank number to modify permissions.\n\r", ch);
            send_to_char("Syntax: church permission <rank#> <permission> [on|off]\n\r", ch);
            return;
        }
    }
}

/**
 * can_access_church_storage - Check if a character can access church storage
 *
 * Access is granted if:
 * - Character is an immortal
 * - Character is a member with CHURCH_PERM_STORAGE, CHURCH_PERM_GET_STORAGE,
 *   or CHURCH_PERM_PUT_STORAGE
 *
 * Access is denied if:
 * - Character is not a member of the church
 * - Character is excommunicated
 *
 * @param ch      Character to check
 * @param church  Church whose storage to access
 * @return        true if access allowed, false otherwise
 */
bool can_access_church_storage(CHAR_DATA *ch, CHURCH_DATA *church)
{
    // Immortals can always access
    if (IS_IMMORTAL(ch)) return true;
    
    // Must be a member of the church
    if (ch->church != church) return false;
    
    // Can't access if excommunicated
    if (is_excommunicated(ch)) return false;
    
    // CHURCH_PERM_STORAGE allows full access to storage
    if (has_church_permission(ch->church_member, CHURCH_PERM_STORAGE))
        return true;
    
    // For list and info commands, any storage-related permission is enough
    if (has_church_permission(ch->church_member, CHURCH_PERM_GET_STORAGE) ||
        has_church_permission(ch->church_member, CHURCH_PERM_PUT_STORAGE))
        return true;
    
    return false;
}

/**
 * convert_church_ranks - Convert legacy church to new rank system
 *
 * Converts churches from the old 4-rank (A/B/C/D) system to the new
 * flexible rank system with permission flags. Creates 4 default ranks:
 * - Leader (D): All permissions, protected
 * - Officer (C): Most administrative permissions
 * - Trusted (B): Basic member permissions
 * - Member (A): Minimal permissions, protected
 *
 * Also updates all existing members to point to the appropriate new
 * ranks based on their old rank values.
 *
 * TODO: Sex-based title lookup (get_default_legacy_rank_name) needs
 * to be reworked to use the pronoun system.
 *
 * @param church  Church to convert
 */
void convert_church_ranks(CHURCH_DATA *church)
{
    log_stringf("convert_church_ranks: Converting church ranks for %s", church->name);
    // First, clear any existing ranks
    while (church->ranks) {
        CHURCH_RANK_DATA *rank = church->ranks;
        church->ranks = rank->next;
        free_church_rank(rank);
    }
    church->num_ranks = 0;

    // Assign permissions to match the new command table
    long perm_member  = CHURCH_PERM_GOHALL | CHURCH_PERM_TALK | CHURCH_PERM_TREASURE;
    long perm_officer = perm_member | CHURCH_PERM_WITHDRAW | CHURCH_PERM_BALANCE | CHURCH_PERM_MOTD | CHURCH_PERM_RULES | CHURCH_PERM_STORAGE | CHURCH_PERM_VIEWLOG;
    long perm_leader  = ~0; // All permissions

    CHURCH_RANK_DATA *ranks[4];

    // D (highest) - Leader
    char *male_name_d = get_default_legacy_rank_name(church, CHURCH_RANK_D, SEX_MALE);
    char *female_name_d = get_default_legacy_rank_name(church, CHURCH_RANK_D, SEX_FEMALE);
    ranks[0] = add_church_rank(church, male_name_d, male_name_d, female_name_d, male_name_d, perm_leader, RANK_TYPE_LEADER);

    // C - Officer
    char *male_name_c = get_default_legacy_rank_name(church, CHURCH_RANK_C, SEX_MALE);
    char *female_name_c = get_default_legacy_rank_name(church, CHURCH_RANK_C, SEX_FEMALE);
    ranks[1] = add_church_rank(church, male_name_c, male_name_c, female_name_c, male_name_c, perm_officer, RANK_TYPE_OFFICER);

    // B - Trusted Member (optional, can be same as member)
    char *male_name_b = get_default_legacy_rank_name(church, CHURCH_RANK_B, SEX_MALE);
    char *female_name_b = get_default_legacy_rank_name(church, CHURCH_RANK_B, SEX_FEMALE);
    ranks[2] = add_church_rank(church, male_name_b, male_name_b, female_name_b, male_name_b, perm_member, RANK_TYPE_MEMBER);

    // A (lowest) - Member
    char *male_name_a = get_default_legacy_rank_name(church, CHURCH_RANK_A, SEX_MALE);
    char *female_name_a = get_default_legacy_rank_name(church, CHURCH_RANK_A, SEX_FEMALE);
    ranks[3] = add_church_rank(church, male_name_a, male_name_a, female_name_a, male_name_a, perm_member, RANK_TYPE_MEMBER);

    // Set default rank to lowest member
    church->default_rank = ranks[3];

    // Mark leader and member as protected
    ranks[0]->flags = CHURCH_RANK_PROTECTED;
    ranks[3]->flags = CHURCH_RANK_PROTECTED;

    // Update church members to point to their appropriate ranks
    CHURCH_PLAYER_DATA *member;
    for (member = church->people; member != NULL; member = member->next) {
        int old_rank_val = member->old_rank;
        if (old_rank_val < 0 || old_rank_val >= 4) old_rank_val = 0;
        switch (old_rank_val) {
            case 0: member->rank = ranks[3]; member->rank_uid = ranks[3]->uid; break;
            case 1: member->rank = ranks[2]; member->rank_uid = ranks[2]->uid; break;
            case 2: member->rank = ranks[1]; member->rank_uid = ranks[1]->uid; break;
            case 3: member->rank = ranks[0]; member->rank_uid = ranks[0]->uid; break;
            default: member->rank = ranks[3]; member->rank_uid = ranks[3]->uid; break;
        }
    }
}

/**
 * get_church_rank_by_index - Get a rank by its 0-based index in the list
 *
 * @param church  Church to search
 * @param index   0-based index of the rank
 * @return        Rank at that index, or first rank if index out of bounds
 */
CHURCH_RANK_DATA *get_church_rank_by_index(CHURCH_DATA *church, int index)
{
    CHURCH_RANK_DATA *rank = church->ranks;
    int i = 0;
    
    while (rank && i < index) {
        rank = rank->next;
        i++;
    }
    
    return rank ? rank : church->ranks; // Default to lowest if not found
}

/**
 * do_chrank - Main command for managing church ranks
 *
 * Provides access to all rank management subcommands. Requires leader rank
 * or CHURCH_PERM_RANKS permission.
 *
 * Subcommands:
 * - [list]: Show all ranks with their titles and permissions
 * - add <name> <member|officer|leader>: Create a new rank
 * - remove <rank#>: Delete a rank (protected ranks cannot be removed)
 * - type <rank#> <type>: Change rank type (member/officer/leader)
 * - title <rank#> <gender> <name>: Set gender-specific title
 * - rename <rank#> <newname>: Rename a rank's base name
 *
 * @param ch        Character managing ranks
 * @param argument  Subcommand and arguments
 */
void do_chrank(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    //char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }

    if (!is_church_leader(ch, ch->church) &&
        !has_church_permission(ch->church_member, CHURCH_PERM_RANKS)) {
        send_to_char("Only church leaders or authorized members can manage ranks.\n\r", ch);
        return;
    }
    
    // Default: list ranks
    if (arg1[0] == '\0' || !str_cmp(arg1, "list")) {
        // Show current ranks
        show_church_ranks(ch);
        return;
    }
    
    // Adding ranks
    if (!str_cmp(arg1, "add")) {
        handle_rank_add(ch, arg2, arg3, argument);
        return;
    }
    
    // Removing ranks
    if (!str_cmp(arg1, "remove")) {
        handle_rank_remove(ch, arg2);
        return;
    }
    
    // Changing rank type
    if (!str_cmp(arg1, "type")) {
        handle_rank_type(ch, arg2, arg3);
        return;
    }
    
    // Setting rank names
    if (!str_cmp(arg1, "title")) {
        handle_rank_name(ch, arg2, arg3, argument);
        return;
    }

    // Renaming ranks
    if (!str_cmp(arg1, "rename")) {
        handle_rank_rename(ch, arg2, argument);
        return;
    }
    
    // Show usage
    send_to_char("Church Rank Commands:\n\r", ch);
    send_to_char("CHURCH RANK [LIST]                        - Show all ranks\n\r", ch);
    if (ch->church->num_ranks < game_settings.org_max_ranks) {
        send_to_char("CHURCH RANK ADD <name> <member|officer|leader> - Add a new rank\n\r", ch);
    }
    send_to_char("CHURCH RANK REMOVE <rank#>                - Remove a rank\n\r", ch);
    send_to_char("CHURCH RANK RENAME <rank#> <newname>      - Rename a rank\n\r", ch);
    send_to_char("CHURCH RANK TYPE <rank#> <type>           - Change rank type\n\r", ch);
    send_to_char("CHURCH RANK TITLE <rank#> <gender> <name>  - Set gender-specific name\n\r", ch);
}

/**
 * handle_rank_rename - Handler for 'church rank rename' subcommand
 *
 * Changes the base name of a rank. Protected ranks cannot be renamed.
 * Checks for duplicate names before allowing the change.
 *
 * @param ch        Character renaming the rank
 * @param rank_str  Rank number as string
 * @param new_name  New name for the rank
 */
void handle_rank_rename(CHAR_DATA *ch, char *rank_str, char *new_name)
{
    char buf[MAX_STRING_LENGTH];

    // Check arguments
    if (rank_str[0] == '\0' || !is_number(rank_str) || new_name[0] == '\0') {
        send_to_char("Syntax: church rank rename <rank#> <newname>\n\r", ch);
        return;
    }
    
    int rank_num = atoi(rank_str);
    if (rank_num < 1 || rank_num > ch->church->num_ranks) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        send_to_char("That rank doesn't exist.\n\r", ch);
        return;
    }
    
    // Check for protected rank
    if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED)) {
        send_to_char("That rank is protected and cannot be renamed.\n\r", ch);
        return;
    }
    
    // Check for duplicate rank names
    CHURCH_RANK_DATA *check_rank;
    for (check_rank = ch->church->ranks; check_rank; check_rank = check_rank->next) {
        if (check_rank != rank && 
            !str_cmp(check_rank->rank_name, new_name)) {
            send_to_char("A rank with that name already exists in your church.\n\r", ch);
            return;
        }
    }
    
    // Store the old name for log message
    char old_name[MAX_INPUT_LENGTH];
    strcpy(old_name, rank->rank_name ? rank->rank_name : rank->title_male);
    
    // Update the base name
    if (rank->rank_name)
        free_string(rank->rank_name);
    rank->rank_name = str_dup(new_name);
    
    sprintf(buf, "Renamed rank from '%s' to '%s'.\n\r", old_name, new_name);
    send_to_char(buf, ch);
    
    // Log the change
    sprintf(buf, "%s renamed rank from '%s' to '%s'.",
            ch->name, old_name, new_name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);    
    save_church(ch->church);
}

/**
 * handle_rank_add - Handler for 'church rank add' subcommand
 *
 * Creates a new rank in the church with the specified name and type.
 * Enforces maximum rank limit from game_settings.org_max_ranks.
 * Checks for duplicate names before creating.
 *
 * @param ch        Character adding the rank
 * @param name      Name for the new rank
 * @param type_str  Rank type: "member", "officer", or "leader"
 * @param argument  Additional arguments (unused)
 */
void handle_rank_add(CHAR_DATA *ch, char *name, char *type_str, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    // Check arguments
    if (name[0] == '\0' || type_str[0] == '\0') {
        send_to_char("Syntax: church rank add <name> <member|officer|leader>\n\r", ch);
        return;
    }

    // Check for maximum ranks
    if (ch->church->num_ranks >= game_settings.org_max_ranks) {
        sprintf(buf, "Your church already has the maximum allowed ranks (%d).\n\r",
               game_settings.org_max_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Get rank type
    int rank_type;
    if (!str_prefix(type_str, "member"))
        rank_type = RANK_TYPE_MEMBER;
    else if (!str_prefix(type_str, "officer"))
        rank_type = RANK_TYPE_OFFICER;
    else if (!str_prefix(type_str, "leader"))
        rank_type = RANK_TYPE_LEADER;
    else {
        send_to_char("Invalid rank type. Use 'member', 'officer', or 'leader'.\n\r", ch);
        return;
    }
    
    // Don't allow more than one leader type
    if (rank_type == RANK_TYPE_LEADER) {
        CHURCH_RANK_DATA *rank;
        for (rank = ch->church->ranks; rank; rank = rank->next) {
            if (rank->rank_type == RANK_TYPE_LEADER) {
                send_to_char("Your church already has a leader rank.\n\r", ch);
                return;
            }
        }
    }
    
    // Add the new rank with default permissions based on type
    long permissions = 0;
    if (rank_type >= RANK_TYPE_MEMBER)
        permissions |= CHURCH_PERM_GOHALL | CHURCH_PERM_TALK;
        
    if (rank_type >= RANK_TYPE_OFFICER)
        permissions |= CHURCH_PERM_WITHDRAW | CHURCH_PERM_MOTD | CHURCH_PERM_RULES | 
                      CHURCH_PERM_BALANCE;
        
    if (rank_type == RANK_TYPE_LEADER)
        permissions = ~0; // All permissions
    
    CHURCH_RANK_DATA *new_rank = add_church_rank(ch->church, name, name, name, name, 
                                               permissions, rank_type);
    
    sprintf(buf, "Added new rank '%s' as %s.\n\r", 
            name, rank_type == RANK_TYPE_LEADER ? "Leader" : 
            rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member");
    send_to_char(buf, ch);
    
    // Log the change
    sprintf(buf, "%s added the %s rank '%s' to the church.", ch->name, new_rank->rank_type == RANK_TYPE_LEADER ? "Leader" : 
            rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member" , new_rank->rank_name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);    
    save_church(ch->church);
}

/**
 * handle_rank_remove - Handler for 'church rank remove' subcommand
 *
 * Removes a rank from the church. Cannot remove:
 * - Protected ranks
 * - The last remaining rank
 * - The only leader rank
 *
 * Members with the removed rank will need to be reassigned.
 *
 * @param ch        Character removing the rank
 * @param rank_str  Rank number as string
 */
void handle_rank_remove(CHAR_DATA *ch, char *rank_str)
{
    char buf[MAX_STRING_LENGTH];

    // Check argument
    if (rank_str[0] == '\0' || !is_number(rank_str)) {
        send_to_char("Syntax: church rank remove <rank#>\n\r", ch);
        return;
    }
    
    int rank_num = atoi(rank_str);
    if (rank_num < 1 || rank_num > ch->church->num_ranks) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Can't remove if it's the only rank
    if (ch->church->num_ranks <= 1) {
        send_to_char("Your church must have at least one rank.\n\r", ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        send_to_char("That rank doesn't exist.\n\r", ch);
        return;
    }
    
    // Check for protected rank
    if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED)) {
        send_to_char("That rank is protected and cannot be removed.\n\r", ch);
        return;
    }
    
    // Don't allow removing the last leader rank
    if (rank->rank_type == RANK_TYPE_LEADER) {
        int leader_count = 0;
        CHURCH_RANK_DATA *temp;
        for (temp = ch->church->ranks; temp; temp = temp->next) {
            if (temp->rank_type == RANK_TYPE_LEADER)
                leader_count++;
        }
        
        if (leader_count <= 1) {
            send_to_char("You cannot remove the only leader rank.\n\r", ch);
            return;
        }
    }
    
    // Store the name for the log message
    char rank_name[MAX_INPUT_LENGTH];
    strcpy(rank_name, rank->title_male);
    
    // Remove the rank
    if (remove_church_rank(ch->church, rank)) {
        sprintf(buf, "Removed rank '%s' from church.\n\r", rank_name);
        send_to_char(buf, ch);
        
        sprintf(buf, "%s removed rank '%s' from the church.", ch->name, rank_name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);        
        save_church(ch->church);
    } else {
        send_to_char("Failed to remove the rank.\n\r", ch);
    }
}

/**
 * handle_rank_type - Handler for 'church rank type' subcommand
 *
 * Changes the type (member/officer/leader) of an existing rank.
 * Protected ranks cannot have their type changed.
 *
 * @param ch        Character changing the rank type
 * @param rank_str  Rank number as string
 * @param type_str  New type: "member", "officer", or "leader"
 */
void handle_rank_type(CHAR_DATA *ch, char *rank_str, char *type_str)
{
    char buf[MAX_STRING_LENGTH];

    // Check arguments
    if (rank_str[0] == '\0' || !is_number(rank_str) || type_str[0] == '\0') {
        send_to_char("Syntax: church rank type <rank#> <member|officer|leader>\n\r", ch);
        return;
    }

    int rank_num = atoi(rank_str);
    if (rank_num < 1 || rank_num > ch->church->num_ranks) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        send_to_char("That rank doesn't exist.\n\r", ch);
        return;
    }
    
    // Check for protected rank
    if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED)) {
        send_to_char("That rank is protected and cannot be modified.\n\r", ch);
        return;
    }
    
    // Get new rank type
    int rank_type;
    if (!str_prefix(type_str, "member"))
        rank_type = RANK_TYPE_MEMBER;
    else if (!str_prefix(type_str, "officer"))
        rank_type = RANK_TYPE_OFFICER;
    else if (!str_prefix(type_str, "leader"))
        rank_type = RANK_TYPE_LEADER;
    else {
        send_to_char("Invalid rank type. Use 'member', 'officer', or 'leader'.\n\r", ch);
        return;
    }
    
    // Check if changing from leader to non-leader - don't allow if it's the only leader
    if (rank->rank_type == RANK_TYPE_LEADER && rank_type != RANK_TYPE_LEADER) {
        int leader_count = 0;
        CHURCH_RANK_DATA *temp;
        for (temp = ch->church->ranks; temp; temp = temp->next) {
            if (temp->rank_type == RANK_TYPE_LEADER)
                leader_count++;
        }
        
        if (leader_count <= 1) {
            send_to_char("You cannot change the type of the only leader rank.\n\r", ch);
            return;
        }
    }
    
    // Check if changing to leader - don't allow if there's already a leader
    if (rank_type == RANK_TYPE_LEADER && rank->rank_type != RANK_TYPE_LEADER) {
        CHURCH_RANK_DATA *temp;
        for (temp = ch->church->ranks; temp; temp = temp->next) {
            if (temp != rank && temp->rank_type == RANK_TYPE_LEADER) {
                send_to_char("Your church already has a leader rank.\n\r", ch);
                return;
            }
        }
    }
    
    // Change the rank type
    rank->rank_type = rank_type;
    
    // Adjust permissions based on type
    if (rank_type == RANK_TYPE_MEMBER) {
        rank->permissions = CHURCH_PERM_GOHALL | CHURCH_PERM_TALK;
    } else if (rank_type == RANK_TYPE_OFFICER) {
        rank->permissions = CHURCH_PERM_GOHALL | CHURCH_PERM_TALK | CHURCH_PERM_WITHDRAW | 
                          CHURCH_PERM_MOTD | CHURCH_PERM_RULES | CHURCH_PERM_BALANCE;
    } else if (rank_type == RANK_TYPE_LEADER) {
        rank->permissions = ~0; // All permissions
    }
    
    sprintf(buf, "Changed '%s' to rank type %s.\n\r", 
            rank->title_male,
            rank_type == RANK_TYPE_LEADER ? "leader" : 
            rank_type == RANK_TYPE_OFFICER ? "officer" : "member");
    send_to_char(buf, ch);
    
    // Log the change
    sprintf(buf, "%s changed rank '%s' to type %s.",
            ch->name,
            rank->title_male,
            rank_type == RANK_TYPE_LEADER ? "leader" : 
            rank_type == RANK_TYPE_OFFICER ? "officer" : "member");
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);    
    save_church(ch->church);
}

/**
 * handle_rank_name - Handler for 'church rank title' subcommand
 *
 * Sets the gender-specific title for a rank (male, female, or neutral).
 *
 * TODO: This sex-based title system needs to be reworked to use the
 * pronoun system instead.
 *
 * @param ch          Character setting the title
 * @param rank_str    Rank number as string
 * @param gender_str  Gender: "male", "female", or "neutral"
 * @param name        New title for that gender
 */
void handle_rank_name(CHAR_DATA *ch, char *rank_str, char *gender_str, char *name)
{
    char buf[MAX_STRING_LENGTH];

    // Check arguments
    if (rank_str[0] == '\0' || !is_number(rank_str) ||
        gender_str[0] == '\0' || name[0] == '\0') {
        send_to_char("Syntax: church rank title <rank#> <male|female|neutral> <name>\n\r", ch);
        return;
    }
    
    int rank_num = atoi(rank_str);
    if (rank_num < 1 || rank_num > ch->church->num_ranks) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        send_to_char("That rank doesn't exist.\n\r", ch);
        return;
    }
    
    // Determine gender
    int gender;
    if (!str_prefix(gender_str, "male"))
        gender = SEX_MALE;
    else if (!str_prefix(gender_str, "female"))
        gender = SEX_FEMALE;
    else if (!str_prefix(gender_str, "neutral"))
        gender = SEX_NEUTRAL;
    else {
        send_to_char("Specify 'male', 'female', or 'neutral' for gender.\n\r", ch);
        return;
    }
    
    // Set the name based on gender
    if (gender == SEX_MALE) {
        if (rank->title_male)
            free_string(rank->title_male);
        rank->title_male = str_dup(name);
    } else if (gender == SEX_FEMALE) {
        if (rank->title_female)
            free_string(rank->title_female);
        rank->title_female = str_dup(name);
    } else { // neutral
        if (rank->title_neutral)
            free_string(rank->title_neutral);
        rank->title_neutral = str_dup(name);
    }
    
    sprintf(buf, "Rank %d %s title set to '%s'.\n\r", 
            rank_num, 
            gender == SEX_MALE ? "male" : (gender == SEX_FEMALE ? "female" : "neutral"), 
            name);
    send_to_char(buf, ch);
    
    // Log the change
    sprintf(buf, "%s changed the %s title for rank %d to '%s'.",
            ch->name, 
            gender == SEX_MALE ? "male" : (gender == SEX_FEMALE ? "female" : "neutral"), 
            rank_num, 
            name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);    
    save_church(ch->church);
}

/**
 * show_church_ranks - Display all ranks with titles and types
 *
 * Shows a formatted list of all church ranks including:
 * - Rank number and base name
 * - Rank type (Leader/Officer/Member)
 * - Gender-specific titles (male/female/neutral)
 * - Protected status
 *
 * Also warns if church exceeds the maximum rank limit.
 *
 * @param ch  Character to display ranks to
 */
void show_church_ranks(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    
    if (!ch->church) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }
    
    send_to_char("Church Ranks:\n\r", ch);
    send_to_char("-----------------------\n\r", ch);
    
    int i = 1;
    CHURCH_RANK_DATA *rank;
    
    for (rank = ch->church->ranks; rank; rank = rank->next, i++) {
        sprintf(buf, "%2d. {W%s{x (%s) \n\r", 
               i, rank->rank_name, 
               rank->rank_type == RANK_TYPE_LEADER ? "Leader" : 
               rank->rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member");
        send_to_char(buf, ch);
        
        sprintf(buf, "    {CTitles:{x {CMale:{x %-20s {CFemale:{x %-20s {CNeutral:{x %-20s\n\r",
                rank->title_male ? rank->title_male : "(none)",
                rank->title_female ? rank->title_female : "(none)",
                rank->title_neutral ? rank->title_neutral : "(none)");
        send_to_char(buf, ch);
        
        if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED))
            send_to_char("    {Y[Protected Rank]{x\n\r", ch);
            
        send_to_char("\n\r", ch);
    }
    
    
    // Add warning if ranks exceed the limit
    if (ch->church->num_ranks > game_settings.org_max_ranks) {
        sprintf(buf, "\n\r{RWARNING:{x Your church has %d ranks, which exceeds the game limit of %d.\n\r"
                     "You can continue to manage existing ranks but cannot add new ones.\n\r",
                ch->church->num_ranks, game_settings.org_max_ranks);
        send_to_char(buf, ch);
    }
}

/**
 * do_chranks - Alternative rank management command (leaders only)
 *
 * Similar to do_chrank but with simpler interface and stricter
 * permission (leaders only, no CHURCH_PERM_RANKS).
 *
 * Subcommands:
 * - (no args): List all ranks
 * - add <name>: Add a new member rank
 * - remove <rank#>: Remove a rank
 *
 * @param ch        Church leader managing ranks
 * @param argument  Subcommand and arguments
 */
void do_chranks(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }

    if (!is_church_leader(ch, ch->church)) {
        send_to_char("Only church leaders can manage ranks.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0') {
        // Show current ranks
        send_to_char("Church Ranks:\n\r", ch);
        send_to_char("-----------------------\n\r", ch);
        
        int i = 1;
        CHURCH_RANK_DATA *rank;
        for (rank = ch->church->ranks; rank; rank = rank->next, i++) {
            sprintf(buf, "%2d. %-20s (%s)\n\r", 
                   i, rank->title_male, 
                   rank->rank_type == RANK_TYPE_LEADER ? "Leader" : 
                   rank->rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member");
            send_to_char(buf, ch);
        }
        
        // Add warning if ranks exceed the limit
        if (ch->church->num_ranks > game_settings.org_max_ranks) {
            sprintf(buf, "\n\r{RWARNING:{x Your church has %d ranks, which exceeds the game limit of %d.\n\r"
                         "You can continue to manage existing ranks but cannot add new ones.\n\r",
                    ch->church->num_ranks, game_settings.org_max_ranks);
            send_to_char(buf, ch);
        }
        
        send_to_char("\n\rCommands:\n\r", ch);
        if (ch->church->num_ranks < game_settings.org_max_ranks) {
            send_to_char("CHURCH RANKS ADD <name> <member|officer|leader> - Add a new rank\n\r", ch);
        }
        send_to_char("CHURCH RANKS REMOVE <rank#> - Remove a rank\n\r", ch);
        send_to_char("CHURCH RANKS TYPE <rank#> <member|officer|leader> - Change a rank's type\n\r", ch);
        return;
    }
    
    if (!str_cmp(arg1, "add")) {
        if (arg2[0] == '\0' || arg3[0] == '\0') {
            send_to_char("Syntax: church ranks add <name> <member|officer|leader>\n\r", ch);
            return;
        }
        
        // Check for maximum ranks
        if (ch->church->num_ranks >= game_settings.org_max_ranks) {
            sprintf(buf, "Your church already has the maximum allowed ranks (%d).\n\r", 
                   game_settings.org_max_ranks);
            send_to_char(buf, ch);
            return;
        }
        
        // Get rank type
        int rank_type;
        if (!str_prefix(arg3, "member"))
            rank_type = RANK_TYPE_MEMBER;
        else if (!str_prefix(arg3, "officer"))
            rank_type = RANK_TYPE_OFFICER;
        else if (!str_prefix(arg3, "leader"))
            rank_type = RANK_TYPE_LEADER;
        else {
            send_to_char("Invalid rank type. Use 'member', 'officer', or 'leader'.\n\r", ch);
            return;
        }
        
        // Don't allow more than one leader type
        if (rank_type == RANK_TYPE_LEADER) {
            CHURCH_RANK_DATA *rank;
            for (rank = ch->church->ranks; rank; rank = rank->next) {
                if (rank->rank_type == RANK_TYPE_LEADER) {
                    send_to_char("Your church already has a leader rank.\n\r", ch);
                    return;
                }
            }
        }
        
        // Add the new rank with default permissions based on type
        long permissions = 0;
        if (rank_type >= RANK_TYPE_MEMBER)
            permissions |= CHURCH_PERM_GOHALL;
            
        if (rank_type >= RANK_TYPE_OFFICER)
            permissions |= CHURCH_PERM_WITHDRAW | CHURCH_PERM_MOTD | CHURCH_PERM_RULES;
            
        if (rank_type == RANK_TYPE_LEADER)
            permissions = ~0; // All permissions
        
        //CHURCH_RANK_DATA *new_rank = add_church_rank(ch->church, arg2, arg2, arg2, 
        //                                           permissions, rank_type);
        
        sprintf(buf, "Added new rank '%s' as %s.\n\r", 
                arg2, rank_type == RANK_TYPE_LEADER ? "Leader" : 
                rank_type == RANK_TYPE_OFFICER ? "Officer" : "Member");
        send_to_char(buf, ch);
        
        sprintf(buf, "%s added rank '%s' to the church.", ch->name, arg2);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);        
        save_church(ch->church);
        return;
    }
    
if (!str_cmp(arg1, "remove")) {
    if (arg2[0] == '\0' || !is_number(arg2)) {
        send_to_char("Syntax: church ranks remove <rank#>\n\r", ch);
        return;
    }
    
    int rank_num = atoi(arg2);
    if (rank_num < 1 || rank_num > ch->church->num_ranks) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Can't remove if it's the only rank
    if (ch->church->num_ranks <= 1) {
        send_to_char("Your church must have at least one rank.\n\r", ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        send_to_char("That rank doesn't exist.\n\r", ch);
        return;
    }
    
    // Check for protected rank - MOVED HERE after we've found the rank
    if (IS_SET(rank->flags, CHURCH_RANK_PROTECTED)) {
        send_to_char("That rank is protected and cannot be removed.\n\r", ch);
        return;
    }
    
    // Don't allow removing the last leader rank
    if (rank->rank_type == RANK_TYPE_LEADER) {
            int leader_count = 0;
            CHURCH_RANK_DATA *temp;
            for (temp = ch->church->ranks; temp; temp = temp->next) {
                if (temp->rank_type == RANK_TYPE_LEADER)
                    leader_count++;
            }
            
            if (leader_count <= 1) {
                send_to_char("You cannot remove the only leader rank.\n\r", ch);
                return;
            }
        }
        
        // Store the name for the log message
        char rank_name[MIL];
        strcpy(rank_name, rank->title_male);
        
        // Remove the rank
        if (remove_church_rank(ch->church, rank)) {
            sprintf(buf, "Removed rank '%s' from church.\n\r", rank_name);
            send_to_char(buf, ch);
            
            sprintf(buf, "%s removed rank '%s' from the church.", ch->name, rank_name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);            
            save_church(ch->church);
        } else {
            send_to_char("Failed to remove the rank.\n\r", ch);
        }
        
        return;
    }
    
    if (!str_cmp(arg1, "type")) {
        if (arg2[0] == '\0' || !is_number(arg2) || arg3[0] == '\0') {
            send_to_char("Syntax: church ranks type <rank#> <member|officer|leader>\n\r", ch);
            return;
        }
        
        int rank_num = atoi(arg2);
        if (rank_num < 1 || rank_num > ch->church->num_ranks) {
            sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
            send_to_char(buf, ch);
            return;
        }
        
        // Find the rank
        CHURCH_RANK_DATA *rank = ch->church->ranks;
        for (int i = 1; rank && i < rank_num; i++)
            rank = rank->next;
            
        if (!rank) {
            send_to_char("That rank doesn't exist.\n\r", ch);
            return;
        }
        
        // Get new rank type
        int rank_type;
        if (!str_prefix(arg3, "member"))
            rank_type = RANK_TYPE_MEMBER;
        else if (!str_prefix(arg3, "officer"))
            rank_type = RANK_TYPE_OFFICER;
        else if (!str_prefix(arg3, "leader"))
            rank_type = RANK_TYPE_LEADER;
        else {
            send_to_char("Invalid rank type. Use 'member', 'officer', or 'leader'.\n\r", ch);
            return;
        }
        
        // Don't allow removing the last leader rank
        if (rank->rank_type == RANK_TYPE_LEADER && rank_type != RANK_TYPE_LEADER) {
            int leader_count = 0;
            CHURCH_RANK_DATA *temp;
            for (temp = ch->church->ranks; temp; temp = temp->next) {
                if (temp->rank_type == RANK_TYPE_LEADER)
                    leader_count++;
            }
            
            if (leader_count <= 1) {
                send_to_char("You cannot change the type of the only leader rank.\n\r", ch);
                return;
            }
        }
        
        // Change the rank type
        rank->rank_type = rank_type;
        
        // Adjust permissions based on type
        if (rank_type == RANK_TYPE_MEMBER) {
            rank->permissions = CHURCH_PERM_GOHALL;
        } else if (rank_type == RANK_TYPE_OFFICER) {
            rank->permissions = CHURCH_PERM_GOHALL | CHURCH_PERM_WITHDRAW | 
                              CHURCH_PERM_MOTD | CHURCH_PERM_RULES;
        } else if (rank_type == RANK_TYPE_LEADER) {
            rank->permissions = ~0; // All permissions
        }
        
        sprintf(buf, "Changed '%s' to rank type %s.\n\r", 
                rank->title_male,
                rank_type == RANK_TYPE_LEADER ? "leader" : 
                rank_type == RANK_TYPE_OFFICER ? "officer" : "member");
        send_to_char(buf, ch);
        
        sprintf(buf, "%s changed rank '%s' to type %s.",
                ch->name,
                rank->title_male,
                rank_type == RANK_TYPE_LEADER ? "leader" : 
                rank_type == RANK_TYPE_OFFICER ? "officer" : "member");
add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);        
        save_church(ch->church);
        return;
    }
    
    // If we get here, show usage
    send_to_char("Church Rank Commands:\n\r", ch);
    if (ch->church->num_ranks < game_settings.org_max_ranks) {
        send_to_char("CHURCH RANKS ADD <name> <member|officer|leader> - Add a new rank\n\r", ch);
    }
    send_to_char("CHURCH RANKS REMOVE <rank#> - Remove a rank\n\r", ch);
    send_to_char("CHURCH RANKS TYPE <rank#> <member|officer|leader> - Change a rank's type\n\r", ch);
}

/**
 * add_church_rank - Create and add a new rank to a church
 *
 * Creates a rank with the specified name, gender-specific titles,
 * permissions, and type. Assigns a unique UID and adds to end of
 * the church's rank list.
 *
 * @param church        Church to add rank to
 * @param rank_name     Base/internal name for the rank
 * @param male_name     Display title for male members
 * @param female_name   Display title for female members
 * @param neutral_name  Display title for neutral members
 * @param permissions   CHURCH_PERM_* flags for this rank
 * @param rank_type     RANK_TYPE_MEMBER, RANK_TYPE_OFFICER, or RANK_TYPE_LEADER
 * @return              Newly created rank structure
 */
CHURCH_RANK_DATA *add_church_rank(CHURCH_DATA *church, char *rank_name, const char *male_name,
                                const char *female_name, const char *neutral_name,
                                long permissions, int rank_type)
{
    CHURCH_RANK_DATA *rank, *temp;
    
    // Create new rank
    rank = new_church_rank();
    rank->uid = new_church_rank_uid(church);
    rank->rank_name = str_dup(rank_name); // Use the church-specific UID
    rank->title_male = str_dup(male_name);
    rank->title_female = str_dup(female_name);
    rank->title_neutral = str_dup(neutral_name);
    rank->permissions = permissions;
    rank->rank_type = rank_type;
    rank->next = NULL;
    
    // Add to end of list
    if (!church->ranks) {
        church->ranks = rank;
    } else {
        for (temp = church->ranks; temp->next; temp = temp->next)
            ;
        temp->next = rank;
    }
    
    church->num_ranks++;
    return rank;
}

/**
 * remove_church_rank - Remove a rank from a church
 *
 * Removes the specified rank from the church's rank list. Members who
 * had this rank are automatically reassigned to an adjacent rank.
 * Cannot remove the last remaining rank.
 *
 * @param church  Church to remove rank from
 * @param rank    Rank to remove
 * @return        true if removed successfully, false if failed
 */
bool remove_church_rank(CHURCH_DATA *church, CHURCH_RANK_DATA *rank)
{
    CHURCH_RANK_DATA *prev = NULL, *curr;
    CHURCH_PLAYER_DATA *member;
    
    // Can't remove if it's the only rank
    if (church->num_ranks <= 1)
        return false;
    
    // Find the rank in the list
    for (curr = church->ranks; curr; prev = curr, curr = curr->next) {
        if (curr == rank)
            break;
    }
    
    if (!curr) // Rank not found
        return false;
    
    // Remove from list
    if (prev)
        prev->next = curr->next;
    else
        church->ranks = curr->next;
    
    // Update any members with this rank to the next lower rank
    CHURCH_RANK_DATA *new_rank = prev ? prev : church->ranks;
    for (member = church->people; member; member = member->next) {
        if (member->rank == rank)
            member->rank = new_rank;
    }
    
    free_church_rank(curr);
    church->num_ranks--;
    
    return true;
}

/**
 * get_highest_rank - Get the last rank in the church's rank list
 *
 * Returns the rank at the end of the linked list. Note: This assumes
 * ranks are ordered from lowest to highest in the list.
 *
 * @param church  Church to get rank from
 * @return        Last rank in list, or NULL if no ranks
 */
CHURCH_RANK_DATA *get_highest_rank(CHURCH_DATA *church)
{
    CHURCH_RANK_DATA *rank = church->ranks;
    
    if (!rank)
        return NULL;
        
    while (rank->next)
        rank = rank->next;
        
    return rank;
}


/**
 * assign_church_member_ranks - Link members to their rank structures
 *
 * Iterates through all church members and assigns their rank pointers.
 * Uses multiple fallback strategies:
 * 1. Match by rank UID (preferred)
 * 2. Match by old_rank index (legacy compatibility)
 * 3. Founder gets a leader rank
 * 4. Owner gets a leader rank
 * 5. Default to church's default_rank or first rank
 *
 * Logs warnings for members whose ranks couldn't be found.
 *
 * @param church  Church whose members need rank assignment
 */
void assign_church_member_ranks(CHURCH_DATA *church)
{
    CHURCH_PLAYER_DATA *member;
    bool found_rank;

    for (member = church->people; member; member = member->next) {
        found_rank = false;

        // First try to find rank by UID if available
        if (member->rank_uid > 0) {
            CHURCH_RANK_DATA *rank;
            
            for (rank = church->ranks; rank && !found_rank; rank = rank->next) {
                if (rank->uid == member->rank_uid) {
                    member->rank = rank;
                    found_rank = true;
                    break;
                }
            }
            
            // Log if UID was provided but rank wasn't found
            if (!found_rank) {
                char buf[MAX_STRING_LENGTH];
                sprintf(buf, "assign_church_member_ranks: Could not find rank with UID %ld for member %s in church %s",
                       member->rank_uid, member->name, church->name);
                log_string(buf);
            }
        }
        
        // If rank not found by UID, try by old rank index for backward compatibility
        if (!found_rank && member->old_rank >= 0) {
            int i = 0;
            CHURCH_RANK_DATA *rank;
            
            for (rank = church->ranks; rank && i < member->old_rank; rank = rank->next)
                i++;
                
            if (rank) {
                member->rank = rank;
                member->rank_uid = rank->uid; // Store UID for future lookups
                found_rank = true;
            }
        }
        
        // Special case for founder - always assign to a leader rank
        if (!found_rank && !str_cmp(member->name, church->founder)) {
            CHURCH_RANK_DATA *rank;
            for (rank = church->ranks; rank; rank = rank->next) {
                if (rank->rank_type == RANK_TYPE_LEADER) {
                    member->rank = rank;
                    member->rank_uid = rank->uid; // Store UID for future lookups
                    found_rank = true;
                    break;
                }
            }
        }
        
        // Special case for owner - ensure they have a leader rank
        if (!found_rank && church->owner && !str_cmp(member->name, church->owner)) {
            CHURCH_RANK_DATA *rank;
            for (rank = church->ranks; rank; rank = rank->next) {
                if (rank->rank_type == RANK_TYPE_LEADER) {
                    member->rank = rank;
                    member->rank_uid = rank->uid; // Store UID for future lookups
                    found_rank = true;
                    break;
                }
            }
        }
        
        // If still no rank found, assign to default rank
        if (!found_rank) {
            if (church->default_rank) {
                member->rank = church->default_rank;
                member->rank_uid = church->default_rank->uid; // Store UID
            } else {
                // If no default rank is set, use the lowest rank
                member->rank = church->ranks;
                if (member->rank)
                    member->rank_uid = member->rank->uid; // Store UID
            }
            
            // Log this situation
            char buf[MAX_STRING_LENGTH];
            sprintf(buf, "assign_church_member_ranks: %s's rank was not found in church %s, assigned to %s",
                    member->name, church->name, 
                    member->rank ? member->rank->title_male : "unknown");
            log_string(buf);
        }
    }
}


/**
 * get_default_legacy_rank_name - Get default rank title from legacy tables
 *
 * Returns the default rank title for a given church size, rank level,
 * and sex. Used for backwards compatibility when migrating old churches.
 *
 * TODO: This sex-based title system needs to be reworked to use the
 * pronoun system instead. Many of the gendered titles (Shieldmaiden/Knave,
 * Priestess/Priest, Enchantress/Chieftan) should be replaced with
 * gender-neutral alternatives or pronoun-aware formatting.
 *
 * @param church  Church (used for size to determine tier titles)
 * @param rank    Legacy rank index (CHURCH_RANK_A through CHURCH_RANK_D)
 * @param sex     SEX_MALE, SEX_FEMALE, or SEX_NEUTRAL
 * @return        Default rank title string
 */
char *get_default_legacy_rank_name(CHURCH_DATA *church, int rank, int sex)
{
    // Rank tables were indexed with these defines:
    // CHURCH_RANK_A = 0, CHURCH_RANK_B = 1, CHURCH_RANK_C = 2, CHURCH_RANK_D = 3

    // Make sure rank is in bounds
    if (rank < 0 || rank > 3)
        return "Unknown";

    // Female is index 1, Male is index 0 in the old tables
    int gender_index = (sex == SEX_FEMALE) ? 1 : 0;

    switch(church->size) {
        case CHURCH_SIZE_BAND: // 1
            switch(rank) {
                case 0: return (gender_index == 1) ? "Initiate" : "Initiate";
                case 1: return (gender_index == 1) ? "Member" : "Member";
                case 2: return (gender_index == 1) ? "Advisor" : "Advisor";
                case 3: return (gender_index == 1) ? "Leader" : "Leader";
            }
            break;
            
        case CHURCH_SIZE_CULT: // 2
            switch(rank) {
                case 0: return (gender_index == 1) ? "Follower" : "Follower";
                case 1: return (gender_index == 1) ? "Brethren" : "Brethren";
                case 2: return (gender_index == 1) ? "Disciple" : "Disciple";
                case 3: return (gender_index == 1) ? "Enchantress" : "Chieftan";
            }
            break;
            
        case CHURCH_SIZE_ORDER: // 3
            switch(rank) {
                case 0: return (gender_index == 1) ? "Maiden" : "Knave";
                case 1: return (gender_index == 1) ? "Shieldmaiden" : "Squire";
                case 2: return (gender_index == 1) ? "Knight" : "Knight";
                case 3: return (gender_index == 1) ? "Warmistress" : "Warmaster";
            }
            break;
            
        case CHURCH_SIZE_CHURCH: // 4
            switch(rank) {
                case 0: return (gender_index == 1) ? "Chaplain" : "Chaplain";
                case 1: return (gender_index == 1) ? "Priestess" : "Priest";
                case 2: return (gender_index == 1) ? "Bishop" : "Bishop";
                case 3: return (gender_index == 1) ? "Cardinal" : "Cardinal";
            }
            break;
            
        default:
            return "Unknown";
    }
    
    return "Unknown"; // Fallback
}


void initialize_church_ranks(CHURCH_DATA *church)
{
    // Leader: all permissions
    CHURCH_RANK_DATA *leader_rank = add_church_rank(
        church, "Leader", "Leader", "Leader", "Leader",
        ~0, RANK_TYPE_LEADER);

    // Officer: most management permissions
    (void)add_church_rank(
        church, "Officer", "Officer", "Officer", "Officer",
        CHURCH_PERM_GOHALL | CHURCH_PERM_TALK | CHURCH_PERM_TREASURE |
        CHURCH_PERM_WITHDRAW | CHURCH_PERM_BALANCE | CHURCH_PERM_MOTD |
        CHURCH_PERM_RULES | CHURCH_PERM_STORAGE | CHURCH_PERM_VIEWLOG |
        CHURCH_PERM_MANAGE | CHURCH_PERM_MEMBERS | CHURCH_PERM_RANKS |
        CHURCH_PERM_PERMS | CHURCH_PERM_FINANCES,
        RANK_TYPE_OFFICER);

    // Member: basic permissions
    CHURCH_RANK_DATA *member_rank = add_church_rank(
        church, "Member", "Member", "Member", "Member",
        CHURCH_PERM_GOHALL | CHURCH_PERM_TALK | CHURCH_PERM_TREASURE,
        RANK_TYPE_MEMBER);

    // Set as default rank for new members
    church->default_rank = member_rank;

    // Mark these ranks as protected
    member_rank->flags = CHURCH_RANK_PROTECTED;
    leader_rank->flags = CHURCH_RANK_PROTECTED;
}

/**
 * do_chdefaultrank - Set the default rank for new church members
 *
 * Allows church leaders to specify which rank new members should receive
 * when they join. Cannot set a leader rank as the default.
 *
 * Syntax:
 * - church defaultrank: Show current default rank
 * - church defaultrank <rank#>: Set new default rank
 *
 * @param ch        Church leader
 * @param argument  Rank number or empty to show current
 */
void do_chdefaultrank(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);

    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }

    if (!is_church_leader(ch, ch->church)) {
        send_to_char("Only church leaders can set the default rank.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0') {
        sprintf(buf, "The current default rank for new members is '%s'.\n\r", 
                ch->church->default_rank ? ch->church->default_rank->title_male : "none");
        send_to_char(buf, ch);
        send_to_char("To change it, use: CHURCH DEFAULTRANK <rank#>\n\r", ch);
        return;
    }
    
    // Get rank number (1-based for user)
    int rank_num = atoi(arg1);
    if (rank_num < 1 || !is_number(arg1)) {
        send_to_char("Invalid rank number.\n\r", ch);
        return;
    }
    
    // Find the rank
    CHURCH_RANK_DATA *rank = ch->church->ranks;
    for (int i = 1; rank && i < rank_num; i++)
        rank = rank->next;
        
    if (!rank) {
        sprintf(buf, "Rank number must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Don't allow setting a leader rank as default
    if (rank->rank_type == RANK_TYPE_LEADER) {
        send_to_char("You cannot set a leader rank as the default for new members.\n\r", ch);
        return;
    }
    
    if (ch->church->default_rank) {
        sprintf(buf, "Default rank changed from '%s' to '%s'.",
                ch->church->default_rank->title_male,
                rank->title_male);
    } else {
        sprintf(buf, "Default rank set to '%s'.",
                rank->title_male);
    }
    
        // After successfully changing the default rank
    sprintf(buf, "%s changed the default rank for new members to '%s'.", 
            ch->name, rank->title_male);
    add_church_log_entry(ch->church, ch->name, buf, CHLOG_RANKS, true);
    
    save_church(ch->church);
}

/**
 * upgrade_church_ranks - Ensure church has minimum required ranks
 *
 * Checks if the church has at least one member rank and one leader rank.
 * If missing, creates protected default ranks with standard permissions.
 * Skips churches that already have 4 or more ranks.
 *
 * @param church  Church to upgrade
 */
void upgrade_church_ranks(CHURCH_DATA *church)
{
    bool has_member_rank = false;
    bool has_leader_rank = false;
    CHURCH_RANK_DATA *rank;

    if (church->num_ranks >= 4) {
        log_string(formatf("upgrade_church_ranks: Church %s has %d ranks, skipping upgrade",
            church->name, church->num_ranks));
        return;
    }
    
    // Check if church already has the required protected ranks
    for (rank = church->ranks; rank; rank = rank->next) {
        // Debug log to see what's happening
        log_string(formatf("upgrade_church_ranks: Church %s rank %s type %d flags %ld", 
            church->name, rank->title_male, rank->rank_type, rank->flags));
            
        // Check for member rank (fixed condition)
        if (rank->rank_type == RANK_TYPE_MEMBER) {
            has_member_rank = true;
            log_string(formatf("Found member rank: %s", rank->title_male));
        }
            
        // Check for leader rank (fixed condition)
        if (rank->rank_type == RANK_TYPE_LEADER) {
            has_leader_rank = true;
            log_string(formatf("Found leader rank: %s", rank->title_male));
        }
    }
    
    // If no protected member rank, create one
    if (!has_member_rank) {
        CHURCH_RANK_DATA *member_rank = add_church_rank(church, "Member",
            "Member", "Member", "Member",
            CHURCH_PERM_GOHALL | CHURCH_PERM_TALK,
            RANK_TYPE_MEMBER);
            
        member_rank->flags = CHURCH_RANK_PROTECTED;
        
        // Set as default if no default rank
        if (!church->default_rank)
            church->default_rank = member_rank;
    }
    
    // If no protected leader rank, create one
    if (!has_leader_rank) {
        CHURCH_RANK_DATA *leader_rank = add_church_rank(church, "Leader",
            "Leader", "Leader", "Leader",
            ~0, // All permissions
            RANK_TYPE_LEADER);
            
        leader_rank->flags = CHURCH_RANK_PROTECTED;
    }
}

/**
 * can_access_treasure_room - Check if a member can access a treasure room
 *
 * Access is granted if any of these are true:
 * - Treasure room is the default (all members can access)
 * - Member is the church founder
 * - Member has CHURCH_PERM_TREASURE_ALL permission
 * - Member's rank is leader type
 * - Member's rank is in the treasure room's allowed_ranks list
 *
 * @param member    Church member to check
 * @param treasure  Treasure room to check access for
 * @return          true if member can access, false otherwise
 */
bool can_access_treasure_room(CHURCH_PLAYER_DATA *member, CHURCH_TREASURE_ROOM *treasure)
{
    ITERATOR it;
    CHURCH_RANK_DATA *rank;

    if (!member || !treasure)
        return false;

    // Special cases that always grant access
    if (treasure->is_default)
        return true;

    if (!str_cmp(member->name, member->church->founder))
        return true;

    if (has_church_permission(member, CHURCH_PERM_TREASURE_ALL))
        return true;

    if (member->rank && member->rank->rank_type == RANK_TYPE_LEADER)
        return true;
        
    // Check if member's rank is in the allowed list
    bool has_access = false;
    iterator_start(&it, treasure->allowed_ranks);
    while ((rank = (CHURCH_RANK_DATA *)iterator_nextdata(&it))) {
        if (rank == member->rank) {
            has_access = true;
            break;
        }
    }
    iterator_stop(&it);
    
    return has_access;
}

/**
 * add_rank_to_treasure_room - Grant a rank access to a treasure room
 *
 * Adds a church rank to the treasure room's allowed_ranks list,
 * allowing members of that rank to access the treasure room.
 * Creates the allowed_ranks list if it doesn't exist.
 * No-op if the rank is already in the list.
 *
 * @param treasure  Treasure room to modify
 * @param rank      Church rank to grant access
 * @return          true on success, false on allocation failure
 */
bool add_rank_to_treasure_room(CHURCH_TREASURE_ROOM *treasure, CHURCH_RANK_DATA *rank)
{
    // Create the list if it doesn't exist
    if (!treasure->allowed_ranks) {
        treasure->allowed_ranks = list_create(false);
        if (!treasure->allowed_ranks)
            return false;
    }
    
    // Check if rank is already in the list
    ITERATOR it;
    CHURCH_RANK_DATA *existing;
    bool already_exists = false;
    
    iterator_start(&it, treasure->allowed_ranks);
    while ((existing = (CHURCH_RANK_DATA *)iterator_nextdata(&it))) {
        if (existing == rank) {
            already_exists = true;
            break;
        }
    }
    iterator_stop(&it);
    
    // Add if not already in list
    if (!already_exists) {
        return list_appendlink(treasure->allowed_ranks, rank);
    }
    
    return true; // Already exists, no error
}

/**
 * remove_rank_from_treasure_room - Revoke a rank's access to a treasure room
 *
 * Removes a church rank from the treasure room's allowed_ranks list,
 * preventing members of that rank from accessing the treasure room
 * (unless they have access through other means like permissions or leader status).
 *
 * @param treasure  Treasure room to modify
 * @param rank      Church rank to revoke access
 * @return          true if rank was found and removed, false if not found or invalid args
 */
bool remove_rank_from_treasure_room(CHURCH_TREASURE_ROOM *treasure, CHURCH_RANK_DATA *rank)
{
    if (!treasure || !treasure->allowed_ranks)
        return false;
    
    // Check if rank is in the list before attempting to remove
    bool found = false;
    ITERATOR it;
    CHURCH_RANK_DATA *existing;
    
    iterator_start(&it, treasure->allowed_ranks);
    while ((existing = (CHURCH_RANK_DATA *)iterator_nextdata(&it))) {
        if (existing == rank) {
            found = true;
            break;
        }
    }
    iterator_stop(&it);
    
    if (found) {
        // The rank was found, so remove it
        list_remlink(treasure->allowed_ranks, rank, false);
        return true;
    }
    
    // Rank wasn't in the list
    return false;
}

/**
 * create_church_treasure_room - Create a new treasure room for a church
 *
 * Allocates and initializes a CHURCH_TREASURE_ROOM structure, then adds
 * it to the church's treasure_rooms list. The treasure room can optionally
 * be marked as the default room (accessible to all members).
 *
 * @param church     Church to add the treasure room to
 * @param room       Room index data for the physical room location
 * @param is_default If true, all church members can access this room
 * @return           Pointer to the new treasure room, or NULL on failure
 */
CHURCH_TREASURE_ROOM *create_church_treasure_room(CHURCH_DATA *church, ROOM_INDEX_DATA *room, bool is_default)
{
    CHURCH_TREASURE_ROOM *treasure = alloc_mem(sizeof(CHURCH_TREASURE_ROOM));
    
    if (!treasure)
        return NULL;
        
    treasure->room = room;
    treasure->is_default = is_default;
    treasure->name = NULL;
    treasure->min_rank = 0;
    treasure->allowed_ranks = list_create(false);
    
    if (!treasure->allowed_ranks) {
        free_mem(treasure, sizeof(CHURCH_TREASURE_ROOM));
        return NULL;
    }
    
    if (!list_appendlink(church->treasure_rooms, treasure)) {
        list_destroy(treasure->allowed_ranks);
        free_mem(treasure, sizeof(CHURCH_TREASURE_ROOM));
        return NULL;
    }
    
    return treasure;
}

/**
 * do_chsetmemberrank - Player command to change a church member's rank
 *
 * Allows church leaders (or those with CHURCH_PERM_MEMBERS) to change
 * the rank of other church members. Enforces hierarchy rules:
 * - Cannot change ranks of members at or above your own rank (unless founder)
 * - Cannot assign ranks higher than your own (unless founder)
 * - Only leaders can assign the leader rank
 *
 * Syntax: church setmemberrank <character> <rank#>
 *
 * Logs the change and notifies the affected member if online.
 *
 * @param ch        Player executing the command
 * @param argument  Command arguments: "<character> <rank#>"
 */
void do_chsetmemberrank(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_PLAYER_DATA *member;
    CHURCH_RANK_DATA *rank;
    
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    
    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }
    
    if (!is_church_leader(ch, ch->church) && 
        !has_church_permission(ch->church_member, CHURCH_PERM_MEMBERS)) {
        send_to_char("You don't have permission to change member ranks.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0' || arg2[0] == '\0') {
        send_to_char("Syntax: church setmemberrank <character> <rank#>\n\r", ch);
        return;
    }
    
    // Find the member
    for (member = ch->church->people; member != NULL; member = member->next) {
        if (!str_cmp(member->name, arg1))
            break;
    }
    
    if (member == NULL) {
        send_to_char("That person is not a member of your church.\n\r", ch);
        return;
    }
    
    // The church founder can only be changed by themselves
    if (!can_modify_church_member(ch, member))
    {
        send_to_char("You don't have permission to change that member's rank.\n\r", ch);
        return;
    }
    
    // Member rank type must be <= your rank type unless you're the founder
    if (member->rank->rank_type > ch->church_member->rank->rank_type && 
        str_cmp(ch->name, ch->church->founder)) {
        send_to_char("You can't change the rank of someone of higher rank than yourself.\n\r", ch);
        return;
    }
    
    // Find the target rank
    int rank_num = atoi(arg2);
    if (rank_num < 1 || !is_number(arg2)) {
        send_to_char("Invalid rank number.\n\r", ch);
        return;
    }
    
    int i = 1;
    for (rank = ch->church->ranks; rank && i < rank_num; rank = rank->next, i++);
    
    if (!rank) {
        sprintf(buf, "Rank must be between 1 and %d.\n\r", ch->church->num_ranks);
        send_to_char(buf, ch);
        return;
    }
    
    // Can't assign leader rank unless you're the founder or a leader
    if (rank->rank_type == RANK_TYPE_LEADER && 
        !is_church_leader(ch, ch->church) &&
        str_cmp(ch->name, ch->church->founder)) {
        send_to_char("Only leaders can assign the leader rank.\n\r", ch);
        return;
    }
    
    // Can't assign a rank higher than your own unless you're the founder
    if (rank->rank_type > ch->church_member->rank->rank_type && 
        str_cmp(ch->name, ch->church->founder)) {
        send_to_char("You can't assign a rank higher than your own.\n\r", ch);
        return;
    }
    
    // Set the new rank
    member->rank = rank;
    
    sprintf(buf, "You have set %s's rank to %s.\n\r", 
            member->name, get_chrank(member));
    send_to_char(buf, ch);
    
    if (member->ch && member->ch != ch) {
        sprintf(buf, "Your rank has been changed to %s by %s.\n\r", 
                get_chrank(member), ch->name);
        send_to_char(buf, member->ch);
    }
    
    // Log the change
    sprintf(buf, "%s changed %s's rank to %s.", 
            ch->name, member->name, get_chrank(member));
add_church_log_entry(ch->church, ch->name, buf, CHLOG_MEMBERS, true);    
    save_church(ch->church);
}

/**
 * is_church_owner - Check if a character is the owner of a church
 *
 * The owner has ultimate control over the church and can perform
 * any action, including transferring ownership and modifying other leaders.
 *
 * @param ch      Character to check
 * @param church  Church to check ownership of
 * @return        true if ch is the church owner, false otherwise
 */
bool is_church_owner(CHAR_DATA *ch, CHURCH_DATA *church)
{
    if (!ch || !church || !church->owner)
        return false;

    return (!str_cmp(ch->name, church->owner));
}

/**
 * can_modify_church_member - Check if a character can modify another member's settings
 *
 * Determines whether ch has permission to change target's rank, permissions,
 * or other attributes. Rules:
 * - The church owner can modify anyone
 * - Non-leaders cannot modify anyone
 * - Leaders can modify other members but not the owner
 *
 * @param ch      Character attempting to make modifications
 * @param target  Church member being modified
 * @return        true if ch can modify target, false otherwise
 */
bool can_modify_church_member(CHAR_DATA *ch, CHURCH_PLAYER_DATA *target)
{
    if (!ch || !ch->church || !ch->church_member || !target)
        return false;
    
    // Owner can modify anyone
    if (is_church_owner(ch, ch->church))
        return true;
        
    // Non-owners can only modify non-owners
    if (!is_church_leader(ch, ch->church))
        return false;
        
    // Leaders can't modify the owner
    if (!str_cmp(target->name, ch->church->owner))
        return false;
        
    // Leaders can modify other members including other leaders
    return true;
}

/**
 * do_chowner - Player command to transfer church ownership
 *
 * Only the current church owner can transfer ownership to another member.
 * With no argument, displays the current owner. When transferring:
 * - The new owner must be a church member
 * - The new owner is automatically promoted to leader rank if not already
 *
 * Syntax: church owner [membername]
 *
 * Logs the transfer and notifies the new owner if online.
 *
 * @param ch        Player executing the command (must be church owner)
 * @param argument  Optional member name to transfer ownership to
 */
void do_chowner(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_PLAYER_DATA *member;
    
    argument = one_argument(argument, arg);
    
    if (ch->church == NULL)
    {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }
    
    if (!is_church_owner(ch, ch->church))
    {
        send_to_char("Only the church owner can transfer ownership.\n\r", ch);
        return;
    }
    
    if (arg[0] == '\0')
    {
        sprintf(buf, "The current church owner is: %s\n\r", ch->church->owner);
        send_to_char(buf, ch);
        send_to_char("To transfer ownership, use: CHURCH OWNER <membername>\n\r", ch);
        return;
    }
    
    // Find the member
    for (member = ch->church->people; member != NULL; member = member->next)
    {
        if (!str_cmp(member->name, arg))
            break;
    }
    
    if (member == NULL)
    {
        send_to_char("That person is not a member of your church.\n\r", ch);
        return;
    }
    
    // Check if already the owner
    if (!str_cmp(member->name, ch->church->owner))
    {
        send_to_char("That person is already the church owner.\n\r", ch);
        return;
    }
    
    // Transfer ownership
    free_string(ch->church->owner);
    ch->church->owner = str_dup(member->name);
    
    // Ensure the new owner has a leader rank
    CHURCH_RANK_DATA *leader_rank = NULL;
    for (leader_rank = ch->church->ranks; leader_rank; leader_rank = leader_rank->next)
    {
        if (leader_rank->rank_type == RANK_TYPE_LEADER)
            break;
    }
    
    if (leader_rank && member->rank->rank_type != RANK_TYPE_LEADER)
    {
        member->rank = leader_rank;
    }
    
    sprintf(buf, "You have transferred church ownership to %s.\n\r", member->name);
    send_to_char(buf, ch);
    
    if (member->ch)
    {
        sprintf(buf, "%s has transferred church ownership to you.\n\r", ch->name);
        send_to_char(buf, member->ch);
    }
    
    // Log the ownership transfer
    sprintf(buf, "%s transferred church ownership to %s.", ch->name, member->name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_LEADERSHIP, true);
    save_church(ch->church);
}

/**
 * do_chuserperm - Player command to manage individual member permissions
 *
 * Allows church leaders to grant or revoke personal permissions for
 * individual members, separate from their rank-based permissions.
 * Personal permissions supplement rank permissions.
 *
 * Syntax:
 *   church userperm list              - Show available permissions
 *   church userperm <char>            - Show member's current permissions
 *   church userperm <char> <perm>     - Toggle permission
 *   church userperm <char> <perm> on  - Grant permission
 *   church userperm <char> <perm> off - Revoke permission
 *
 * Leaders cannot modify the owner's permissions. Logs all changes.
 *
 * @param ch        Player executing the command (must be church leader)
 * @param argument  Command arguments
 */
void do_chuserperm(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHURCH_PLAYER_DATA *member;
    
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    
    if (ch->church == NULL) {
        send_to_char("You are not in a church.\n\r", ch);
        return;
    }
    
    if (!is_church_leader(ch, ch->church)) {
        send_to_char("Only church leaders can manage member permissions.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0') {
        send_to_char("Syntax: church userperm <character> [permission] [on|off]\n\r", ch);
        send_to_char("Use 'church userperm list' to see a list of available permissions.\n\r", ch);
        return;
    }
    
    // Show available permissions
    if (!str_cmp(arg1, "list")) {
        send_to_char("Available permissions:\n\r", ch);
        send_to_char("--------------------\n\r", ch);
        
        for (int i = 0; church_permission_flags[i].name; i++) {
            if (church_permission_flags[i].settable) {
                sprintf(buf, "%-20s - %s\n\r", 
                        church_permission_flags[i].name, 
                        church_permission_flags[i].description ? 
                        church_permission_flags[i].description : "No description");
                send_to_char(buf, ch);
            }
        }
        return;
    }
    
    // Find the member
    for (member = ch->church->people; member != NULL; member = member->next) {
        if (!str_cmp(member->name, arg1))
            break;
    }
    
    if (member == NULL) {
        send_to_char("That person is not a member of your church.\n\r", ch);
        return;
    }
    
    // Can't modify permissions of the owner or ranks higher than yours unless you're the owner
    if (!can_modify_church_member(ch, member)) {
        send_to_char("You can't modify that member's permissions.\n\r", ch);
        return;
    }
    
    // Show current permissions for this member
    if (arg2[0] == '\0') {
        sprintf(buf, "Personal permissions for %s:\n\r", member->name);
        send_to_char(buf, ch);
        send_to_char("--------------------\n\r", ch);
        
        bool has_any = false;
        for (int i = 0; church_permission_flags[i].name; i++) {
            if (church_permission_flags[i].settable && 
                IS_SET(member->personal_permissions, church_permission_flags[i].bit)) {
                sprintf(buf, "%-20s\n\r", church_permission_flags[i].name);
                send_to_char(buf, ch);
                has_any = true;
            }
        }
        
        if (!has_any) {
            send_to_char("No personal permissions granted.\n\r", ch);
        }
        
        send_to_char("\nPermissions granted by rank:\n\r", ch);
        send_to_char("-------------------------\n\r", ch);
        
        has_any = false;
        for (int i = 0; church_permission_flags[i].name; i++) {
            if (church_permission_flags[i].settable && 
                IS_SET(member->rank->permissions, church_permission_flags[i].bit)) {
                sprintf(buf, "%-20s\n\r", church_permission_flags[i].name);
                send_to_char(buf, ch);
                has_any = true;
            }
        }
        
        if (!has_any) {
            send_to_char("No permissions granted through rank.\n\r", ch);
        }
        
        return;
    }
    
    // Find the permission
    int permission = flag_value(church_permission_flags, arg2);
    if (permission == NO_FLAG) {
        send_to_char("Unknown permission. Use 'church userperm list' to see available permissions.\n\r", ch);
        return;
    }
    
    // Check if the permission is already granted by rank
    if (IS_SET(member->rank->permissions, permission)) {
        send_to_char("That permission is already granted by the member's rank.\n\r", ch);
        return;
    }
    
    // Toggle or set the permission
    if (arg3[0] == '\0') {
        // Toggle
        if (IS_SET(member->personal_permissions, permission)) {
            REMOVE_BIT(member->personal_permissions, permission);
            sprintf(buf, "Permission %s removed from %s.\n\r", arg2, member->name);
        } else {
            SET_BIT(member->personal_permissions, permission);
            sprintf(buf, "Permission %s granted to %s.\n\r", arg2, member->name);
        }
    } else if (!str_prefix(arg3, "on")) {
        SET_BIT(member->personal_permissions, permission);
        sprintf(buf, "Permission %s granted to %s.\n\r", arg2, member->name);
    } else if (!str_prefix(arg3, "off")) {
        REMOVE_BIT(member->personal_permissions, permission);
        sprintf(buf, "Permission %s removed from %s.\n\r", arg2, member->name);
    } else {
        send_to_char("Specify 'on' or 'off' to set permission.\n\r", ch);
        return;
    }
    
    send_to_char(buf, ch);
    
    // Notify the member if they're online
    if (member->ch && member->ch != ch) {
        if (IS_SET(member->personal_permissions, permission)) {
            sprintf(buf, "%s has granted you the %s permission.\n\r", ch->name, arg2);
        } else {
            sprintf(buf, "%s has removed the %s permission from you.\n\r", ch->name, arg2);
        }
        send_to_char(buf, member->ch);
    }
    
    // Log the change
    sprintf(buf, "%s %s personal permission %s for %s.",
        ch->name, 
        IS_SET(member->personal_permissions, permission) ? "granted" : "removed",
        arg2, 
        member->name);
add_church_log_entry(ch->church, ch->name, buf, CHLOG_PERMISSIONS, true);    
    save_church(ch->church);
}


/**
 * save_church - Persist a church to disk
 *
 * Saves church data to an individual .org file in ORG_DIR.
 * Uses atomic write (write to .tmp, then rename) for safety.
 * Filename format: UID_normalizedname.org
 *
 * @param church  Church to save (must not be NULL)
 */
void save_church(CHURCH_DATA *church)
{
    if (!church) {
        pbugf(LOG_ERROR, "save_church: null church");
        return;
    }

    // Create church directory if it doesn't exist
    mkdir(ORG_DIR, 0755);
    
    // Save as JSON
    if (!save_church_json(church)) {
        pbugf(LOG_ERROR, "save_church: JSON save failed");
    }
}

/**
 * find_rank_by_name - Look up a church rank by its name
 *
 * Searches the church's rank list for a rank matching the given name.
 * Comparison is case-insensitive.
 *
 * @param church  Church to search within
 * @param name    Rank name to find
 * @return        Pointer to the rank if found, NULL otherwise
 */
CHURCH_RANK_DATA *find_rank_by_name(CHURCH_DATA *church, const char *name)
{
    CHURCH_RANK_DATA *rank;
    
    for (rank = church->ranks; rank; rank = rank->next) {
        if (!str_cmp(rank->rank_name, name))
            return rank;
    }
    
    return NULL;
}

/**
 * new_church_rank_uid - Generate a unique ID for a new church rank
 *
 * Increments and returns the church's max_rank_uid counter.
 * Each rank within a church has a unique ID that persists across saves.
 *
 * @param church  Church to generate a rank UID for
 * @return        New unique rank ID, or 0 if church is NULL
 */
long new_church_rank_uid(CHURCH_DATA *church)
{
    if (!church)
        return 0;

    return ++church->max_rank_uid;
}

/**
 * has_processed_file - Check if a church file has already been processed
 *
 * Used during church loading to prevent processing the same file twice
 * (e.g., when both old and new format files exist). Checks against
 * the processed_files array.
 *
 * @param filename  Filename to check
 * @return          true if already processed, false otherwise
 */
bool has_processed_file(const char *filename)
{
    for (int i = 0; i < num_processed_files; i++) {
        if (!strcmp(processed_files[i], filename))
            return true;
    }
    return false;
}

/**
 * add_to_processed_files - Mark a church file as processed
 *
 * Adds a filename to the processed_files tracking array.
 * Used during church loading to prevent duplicate loading.
 * Silently drops entries if MAX_PROCESSED_FILES is reached.
 *
 * @param filename  Filename to mark as processed
 */
void add_to_processed_files(const char *filename)
{
    if (num_processed_files < MAX_PROCESSED_FILES) {
        processed_files[num_processed_files++] = str_dup(filename);
    }
}

/**
 * add_church_log_entry - Add a new entry to the church's activity log
 *
 * Creates and appends a log entry to the church's log. Entries are
 * timestamped and assigned a unique ID. If the log exceeds
 * MAX_CHURCH_LOG_ENTRIES, oldest entries are pruned.
 * Automatically saves the church after adding the entry.
 *
 * @param church           Church to add the log entry to
 * @param author           Name of the person who created the entry (can be NULL for system)
 * @param text             Log entry text
 * @param categories       Bitmask of CHLOG_* category flags
 * @param system_generated true if auto-generated, false if player-written
 */
void add_church_log_entry(CHURCH_DATA *church, char *author, char *text, flag_t categories, bool system_generated)
{
    CHURCH_LOG_ENTRY *entry, *temp;
    
    if (!church)
        return;
    
    // Create new entry using the recycling allocator
    entry = new_church_log_entry();
    entry->author = author ? str_dup(author) : NULL;
    entry->text = str_dup(text);
    entry->categories = categories;
    entry->timestamp = current_time;
    entry->entry_id = ++church->last_log_entry_id;
    entry->system_generated = system_generated;
    
    // Add to end of list
    if (!church->log_entries) {
        church->log_entries = entry;
    } else {
        for (temp = church->log_entries; temp->next; temp = temp->next)
            ;
        temp->next = entry;
    }
    
    // Increment count and prune if necessary
    church->log_entry_count++;
    
    // Remove oldest entries if we exceed the maximum
    while (church->log_entry_count > MAX_CHURCH_LOG_ENTRIES) {
        temp = church->log_entries;
        church->log_entries = temp->next;
        
        free_church_log_entry(temp);
        
        church->log_entry_count--;
    }
    
    // Save the church after log changes
    save_church(church);
}

/**
 * chtoggle_complete - Complete the PK status toggle for a church
 *
 * Called after confirmation to enable or disable the church's
 * player-killing status. Enabling PK is free; disabling costs
 * 5000 pneuma. Broadcasts the change globally and logs it.
 *
 * @param ch         Character toggling PK status (must be in a church)
 * @param enable_pk  true to enable PK, false to disable
 */
void chtoggle_complete(CHAR_DATA *ch, bool enable_pk)
{
    char buf[MAX_STRING_LENGTH];
    
    if (enable_pk) {
        // Enable PK - no pneuma cost when enabling
        ch->church->pk = true;
        sprintf(buf, "{Y[%s is now a PLAYER KILLING church!]{x\n\r", ch->church->name);
        gecho(buf);
        
        // Log the change
        sprintf(buf, "%s enabled player killing status for the church.", ch->name);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_PK, true);
    } else {
        // Disable PK - charge pneuma
        if (ch->church->pneuma >= 5000)
            ch->church->pneuma -= 5000;
        else {
            send_to_char("It costs 5000 pneuma to toggle off your PK flag.\n\r", ch);
            return;
        }
        
        ch->church->pk = false;
        sprintf(buf, "{Y[%s is no longer a player killing church!]{x\n\r", ch->church->name);
        gecho(buf);
        
        // Log the change
        sprintf(buf, "%s disabled player killing status for the church.", ch->name);
        add_church_log_entry(ch->church, ch->name, buf, CHLOG_PK, true);
    }
    
    save_church(ch->church);
}

/**
 * string_end_chlog - Callback when player finishes editing a church log entry
 *
 * Called when the string editor is closed for a church log entry.
 * Validates the text, cleans up trailing whitespace, and either
 * creates a new log entry or updates an existing one (based on
 * ch->temp_log_entry_id). Resets editor state when complete.
 *
 * Uses ch->temp_log_entry for the text, ch->temp_log_category for
 * the category, and ch->temp_log_entry_id for edits (0 = new entry).
 *
 * @param ch  Character who was editing the log entry
 */
void string_end_chlog(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) {
        pbugf(LOG_ERROR, "string_end_chlog: NULL character or descriptor");
        return;
    }
    
    // Check if we have valid church info
    if (!ch->church || !ch->church_member) {
        pbugf(LOG_ERROR, "string_end_chlog: Character not in church");
        ch->desc->pString = NULL;
        ch->desc->editor = 0;
        return;
    }

    // Get the edited text from the string editor
    char *text = ch->temp_log_entry;
    
    // Check if the entry is empty
    if (!text || text[0] == '\0') {
        send_to_char("You must enter some text for the log entry.\n\r", ch);
        // Reset the string editor
        ch->temp_log_entry = NULL;
        ch->desc->editor = 0;
        return;
    }
    
    // Remove trailing newlines and carriage returns
    int len = strlen(text);
    while (len > 0 && (text[len-1] == '\n' || text[len-1] == '\r')) {
        text[len-1] = '\0';
        len--;
    }
    
    // Add the entry to the church log
    send_to_char("Log entry saved.\n\r", ch);
    
    // Get category flag from temp storage
    flag_t category = ch->temp_log_category;
    
    // If editing an existing entry
    if (ch->desc->editor == ED_CHLOG && ch->temp_log_entry_id > 0) {
        // Find the entry
        CHURCH_LOG_ENTRY *entry = NULL;
        for (entry = ch->church->log_entries; entry; entry = entry->next) {
            if (entry->entry_id == ch->temp_log_entry_id)
                break;
        }
        
        if (entry) {
            // Update entry text
            free_string(entry->text);
            entry->text = str_dup(text);
            
            // Log the edit
            char buf[MAX_STRING_LENGTH];
            sprintf(buf, "%s edited log entry #%ld.", ch->name, entry->entry_id);
            add_church_log_entry(ch->church, ch->name, buf, CHLOG_GENERAL, true);
        }
    } else {
        // New entry
        add_church_log_entry(ch->church, ch->name, text, category, false);
    }
    
    // Clean up the editor state
    ch->temp_log_entry = NULL;
    ch->temp_log_category = 0;
    ch->temp_log_entry_id = 0;
    ch->desc->editor = 0;
}

/**
 * display_church_logs - Render church log entries to a player
 *
 * Formats and displays log entries with optional filtering by text,
 * author, or category. Shows entries newest-first with truncated
 * preview text. Includes navigation help for viewing full entries.
 *
 * @param ch                Character to display logs to
 * @param church            Church whose logs are being viewed
 * @param entries           Array of log entry pointers to display
 * @param count             Number of entries in the array
 * @param search_text       Optional text filter (NULL or empty to skip)
 * @param search_author     Optional author filter (NULL or empty to skip)
 * @param search_categories Optional category bitmask filter (0 to skip)
 */
void display_church_logs(CHAR_DATA *ch, CHURCH_DATA *church,
                        CHURCH_LOG_ENTRY **entries, int count,
                        char *search_text, char *search_author, flag_t search_categories)
{
    char buf[MAX_STRING_LENGTH];
    BUFFER *output;
    bool found = false;
    
    output = new_buf();
    
    // Create search description string for the header
    char search_desc[100] = "";
    if (search_text && search_text[0] != '\0')
        sprintf(search_desc, " (search: {G%s{x)", search_text);
    else if (search_author && search_author[0] != '\0')
        sprintf(search_desc, " (author: {G%s{x)", search_author);
    else if (search_categories)
        sprintf(search_desc, " (category: {G%s{x)", flag_string(church_log_category_flags, search_categories));
    
    sprintf(buf, "{YThe Log of %s%s:{x\n\r", church->name, search_desc);
    add_buf(output, buf);
    
    // Display entries in reverse order (newest first)
    for (int i = count - 1; i >= 0; i--) {
        CHURCH_LOG_ENTRY *entry = entries[i];
        
        // Apply search filters if any are specified
        if (search_text && search_text[0] != '\0' && 
            !str_infix(search_text, entry->text))
            continue;
            
        if (search_author && search_author[0] != '\0' && 
            (!entry->author || str_infix(search_author, entry->author)))
            continue;
            
        if (search_categories && !(entry->categories & search_categories))
            continue;
        
        // Format the entry for display
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&entry->timestamp));
        
        // Start with the entry ID and timestamp
        sprintf(buf, "{W%6ld{x ", entry->entry_id);
        add_buf(output, buf);
        
        sprintf(buf, "{C[%s]{x ", time_str);
        add_buf(output, buf);
        
        // Add author information
        if (entry->author) {
            sprintf(buf, "{G(%s%s){x ", 
                    entry->author,
                    entry->system_generated ? " {Y[Auto]{G" : "");
            add_buf(output, buf);
        } else {
            add_buf(output, "{Y(SYSTEM){x ");
        }
        
        // Add category if available - properly use flag_string
        if (entry->categories) {
            const char *category_str = flag_string(church_log_category_flags, entry->categories);
            if (category_str && category_str[0] != '\0') {
                sprintf(buf, "{M[%s]{x ", category_str);
                add_buf(output, buf);
            }
        }
        
        // Create a clean version of the text with newlines replaced by spaces
        char clean_text[MAX_STRING_LENGTH];
        int clean_len = 0;
        int j;
        
        // Get only the first line or up to 40 chars, whichever comes first
        for (j = 0; entry->text[j] != '\0' && clean_len < sizeof(clean_text)-1; j++) {
            if (entry->text[j] == '\n' || entry->text[j] == '\r') {
                // Stop at the first newline - we only want the first line
                break;
            } else {
                clean_text[clean_len++] = entry->text[j];
            }
        }
        clean_text[clean_len] = '\0';
        
        // Truncate the text to about 40 chars
        char truncated_text[44]; // 40 chars + room for ellipsis
        strncpy(truncated_text, clean_text, 40);
        truncated_text[40] = '\0';
        
        // Add ellipsis if text was truncated OR if we hit a newline
        if (strlen(clean_text) > 40 || (entry->text[j] == '\n' || entry->text[j] == '\r')) {
            strcat(truncated_text, "...");
        }
        
        // Add the truncated text
        sprintf(buf, "%s\n\r", truncated_text);
        add_buf(output, buf);
        
        found = true;
    }
    
    // Display search results or empty message
    if (!found) {
        if (search_text && search_text[0] != '\0')
            add_buf(output, "{YNo entries found containing that text.{x\n\r");
        else if (search_author && search_author[0] != '\0')
            add_buf(output, "{YNo entries found by that author.{x\n\r");
        else if (search_categories)
            add_buf(output, "{YNo entries found in that category.{x\n\r");
        else
            add_buf(output, "{YThe log is empty.{x\n\r");
    }
    
    // Add help text
    add_buf(output, "\n\r{YUse 'church log <number>' to view the full text of an entry.{x\n\r");
    
    if (has_church_permission(ch->church_member, CHURCH_PERM_EDITLOG)) {
        add_buf(output, "{YUse 'church log add <category>' to add a new entry.{x\n\r");
        add_buf(output, "{YUse 'church log search <text>' to search entries.{x\n\r");
        add_buf(output, "{YUse 'church log search author <name>' to search by author.{x\n\r");
        add_buf(output, "{YUse 'church log search category <name>' to search by category.{x\n\r");
        add_buf(output, "{YUse 'church log categories' to list available categories.{x\n\r");
    }
    
    page_to_char(buf_string(output), ch);
    free_buf(output);
}

/**
 * is_meta_category - Check if a log category is a meta-category
 *
 * Meta-categories are special categories that aggregate multiple
 * regular categories (e.g., "all" or "system"). These are used for
 * filtering but cannot be directly assigned to log entries.
 *
 * @param category_flag  Category flag to check
 * @return               true if it's a meta-category, false otherwise
 */
bool is_meta_category(flag_t category_flag)
{
    for (int i = 0; church_log_meta_categories[i].flag != 0; i++) {
        if (church_log_meta_categories[i].flag == category_flag) {
            return true;
        }
    }
    return false;
}

/**
 * cmp_church_uid - Comparison function for sorting churches by UID
 *
 * Used with sorting functions to order churches by their unique ID.
 * Returns standard comparison result (-1, 0, 1).
 *
 * @param a  First church (as void pointer)
 * @param b  Second church (as void pointer)
 * @return   -1 if a < b, 1 if a > b, 0 if equal
 */
static int cmp_church_uid(void *a, void *b)
{
    CHURCH_DATA *p1 = (CHURCH_DATA *)a;
    CHURCH_DATA *p2 = (CHURCH_DATA *)b;

    if (p1->uid < p2->uid)
        return -1;
    if (p1->uid > p2->uid)
        return 1;
    return 0;
}

