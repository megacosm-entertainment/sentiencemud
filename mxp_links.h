/**
 * mxp_links.h - Standardized MXP link/tooltip generation
 *
 * Provides helper functions for building MXP <send> tags with multiple
 * commands and hints. Uses a dedicated ring buffer pool to avoid conflicts
 * with formatf() and MXPBuildTag() static buffers.
 *
 * All functions gracefully degrade: when MXP is not enabled for the
 * descriptor, they return the plain display text.
 */

#ifndef MXP_LINKS_H
#define MXP_LINKS_H

#include "merc.h"

typedef struct {
    const char *cmd;
    const char *hint;
} mxp_cmd_hint_t;

/**
 * mxp_link - Build an MXP send tag with a single command and optional hint
 *
 * @param d        Descriptor (NULL-safe, returns text if no MXP)
 * @param text     Display text shown to the user
 * @param command  Command executed on click
 * @param hint     Tooltip text (may be NULL)
 * @return         MXP-wrapped string or plain text
 */
const char *mxp_link(descriptor_t *d, const char *text,
                     const char *command, const char *hint);

/**
 * mxp_link_multi - Build an MXP send tag with multiple commands/hints
 *
 * MXP clients show a menu when multiple commands are provided.
 * Commands and hints are pipe-separated in the href/hint attributes.
 *
 * @param d        Descriptor (NULL-safe)
 * @param text     Display text
 * @param items    Array of command/hint pairs
 * @param nitems   Number of items in the array
 * @return         MXP-wrapped string or plain text
 */
const char *mxp_link_multi(descriptor_t *d, const char *text,
                           const mxp_cmd_hint_t *items, int nitems);

/**
 * mxp_obj_link - Clickable object link with stat/show/edit/purge actions
 *
 * @param d      Descriptor
 * @param obj    Object instance (uses pIndexData for vnum, id[] for UIDs)
 * @param text   Display text (e.g., obj->short_descr)
 * @return       MXP-wrapped string or plain text
 */
const char *mxp_obj_link(descriptor_t *d, OBJ_DATA *obj, const char *text);

/**
 * mxp_obj_id_link - Clickable object ID link with stat/purge actions
 *
 * Displays the object's id[0]/id[1] as clickable text.
 *
 * @param d    Descriptor
 * @param obj  Object instance
 * @return     MXP-wrapped string or plain text
 */
const char *mxp_obj_id_link(descriptor_t *d, OBJ_DATA *obj);

/**
 * mxp_obj_vnum_link - Clickable object vnum link with show/edit actions
 *
 * @param d      Descriptor
 * @param obj    Object index data
 * @param text   Display text
 * @return       MXP-wrapped string or plain text
 */
const char *mxp_obj_vnum_link(descriptor_t *d, OBJ_INDEX_DATA *obj,
                              const char *text);

/**
 * mxp_mob_link - Clickable mobile link with stat/show/edit actions
 *
 * @param d      Descriptor
 * @param mob    Mobile instance
 * @param text   Display text (e.g., mob->short_descr)
 * @return       MXP-wrapped string or plain text
 */
const char *mxp_mob_link(descriptor_t *d, CHAR_DATA *mob, const char *text);

/**
 * mxp_room_link - Clickable room link with show/edit/goto actions
 *
 * @param d      Descriptor
 * @param room   Room index data
 * @param text   Display text (e.g., "Room 1#500")
 * @return       MXP-wrapped string or plain text
 */
const char *mxp_room_link(descriptor_t *d, ROOM_INDEX_DATA *room,
                          const char *text);

/**
 * mxp_player_link - Clickable player name link with stat action
 *
 * @param d      Descriptor
 * @param name   Player name
 * @param text   Display text (may differ from name)
 * @return       MXP-wrapped string or plain text
 */
const char *mxp_player_link(descriptor_t *d, const char *name,
                            const char *text);

/**
 * mxp_help_link - Clickable help topic link
 *
 * @param d        Descriptor
 * @param keyword  Help keyword
 * @param text     Display text
 * @return         MXP-wrapped string or plain text
 */
const char *mxp_help_link(descriptor_t *d, const char *keyword,
                          const char *text);

/**
 * mxp_command_link - Clickable command link with optional tooltip
 *
 * @param d        Descriptor
 * @param command  Command to execute
 * @param hint     Tooltip (may be NULL)
 * @param text     Display text
 * @return         MXP-wrapped string or plain text
 */
const char *mxp_command_link(descriptor_t *d, const char *command,
                             const char *hint, const char *text);

#endif /* MXP_LINKS_H */
