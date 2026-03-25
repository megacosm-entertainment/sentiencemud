/**
 * mxp_links.h - Standardized MXP link/tooltip generation
 *
 * Provides helper functions for building MXP <send> tags with multiple
 * commands and hints. All functions append directly to a BUFFER, avoiding
 * static buffer issues entirely.
 *
 * All functions gracefully degrade: when MXP is not enabled for the
 * descriptor, they append the plain display text.
 */

#ifndef MXP_LINKS_H
#define MXP_LINKS_H

#include "merc.h"
#include "sentience_link.h"

typedef struct mxp_cmd_hint {
    const char *cmd;
    const char *hint;
    bool staff_only;   /* If true, only shown to IS_IMMORTAL characters */
} mxp_cmd_hint_t;

/**
 * mxp_link - Append an MXP send tag with a single command and optional hint
 *
 * @param d        Descriptor (NULL-safe, appends plain text if no MXP)
 * @param buf      Output BUFFER to append to
 * @param text     Display text shown to the user
 * @param command  Command executed on click
 * @param hint     Tooltip text (may be NULL)
 */
void mxp_link(descriptor_t *d, BUFFER *buf, const char *text,
              const char *command, const char *hint);

/**
 * mxp_link_multi - Append an MXP send tag with multiple commands/hints
 *
 * MXP clients show a menu when multiple commands are provided.
 * Commands and hints are pipe-separated in the href/hint attributes.
 *
 * @param d        Descriptor (NULL-safe)
 * @param buf      Output BUFFER to append to
 * @param text     Display text
 * @param items    Array of command/hint pairs
 * @param nitems   Number of items in the array
 */
void mxp_link_multi(descriptor_t *d, BUFFER *buf, const char *text,
                    const mxp_cmd_hint_t *items, int nitems);

/**
 * mxp_obj_link - Clickable object link with stat/show/edit/purge actions
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param obj    Object instance (uses pIndexData for vnum, id[] for UIDs)
 * @param text   Display text (e.g., obj->short_descr)
 */
void mxp_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                  const char *text);

/**
 * mxp_obj_id_link - Clickable object ID link with stat/purge actions
 *
 * Appends the object's id[0]/id[1] as clickable text.
 *
 * @param d    Descriptor
 * @param buf  Output BUFFER
 * @param obj  Object instance
 */
void mxp_obj_id_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj);

/**
 * mxp_obj_vnum_link - Clickable object vnum link with show/edit actions
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param obj    Object index data
 * @param text   Display text
 */
void mxp_obj_vnum_link(descriptor_t *d, BUFFER *buf, OBJ_INDEX_DATA *obj,
                       const char *text);

/**
 * mxp_mob_link - Clickable mobile link with stat/show/edit actions
 *
 * Automatically detects NPC vs player and uses the appropriate commands.
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param mob    Mobile instance
 * @param text   Display text (e.g., mob->short_descr)
 */
void mxp_mob_link(descriptor_t *d, BUFFER *buf, CHAR_DATA *mob,
                  const char *text);

/**
 * mxp_room_link - Clickable room link with show/edit/goto actions
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param room   Room index data
 * @param text   Display text (e.g., "Room 1#500")
 */
void mxp_room_link(descriptor_t *d, BUFFER *buf, ROOM_INDEX_DATA *room,
                   const char *text);

/**
 * mxp_player_link - Clickable player name link with stat action
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param name   Player name
 * @param text   Display text (may differ from name)
 */
void mxp_player_link(descriptor_t *d, BUFFER *buf, const char *name,
                     const char *text);

/**
 * mxp_help_link - Clickable help topic link
 *
 * @param d        Descriptor
 * @param buf      Output BUFFER
 * @param keyword  Help keyword
 * @param text     Display text
 */
void mxp_help_link(descriptor_t *d, BUFFER *buf, const char *keyword,
                   const char *text);

/**
 * mxp_command_link - Clickable command link with optional tooltip
 *
 * @param d        Descriptor
 * @param buf      Output BUFFER
 * @param command  Command to execute
 * @param hint     Tooltip (may be NULL)
 * @param text     Display text
 */
void mxp_command_link(descriptor_t *d, BUFFER *buf, const char *command,
                      const char *hint, const char *text);

void mxp_link_prompt(descriptor_t *d, BUFFER *buf, const char *text,
                     const char *command, const char *hint);

#endif /* MXP_LINKS_H */
