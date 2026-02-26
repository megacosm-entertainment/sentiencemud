/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvements copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefiting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	Sentience MUD improvements copyright (C) 2000-2026                *
*       Nibelung Enterprises                                              *
*       All Rights Reserved                                               *
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <jansson.h>
#include "../../merc.h"
#include "json_note.h"
#include "json_common.h"
#include "../../recycle.h"

/***************************************************************************
 * External References                                                     *
 ***************************************************************************/

extern NOTE_DATA *note_list;
extern NOTE_DATA *news_list;
extern NOTE_DATA *changes_list;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/



/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

/**
 * recipient_type_to_string - Convert a NOTE_RECIPIENT_TYPE enum to string
 *
 * @param type  The recipient type enum value
 * @return      Static string representation of the type
 */
static const char *recipient_type_to_string(NOTE_RECIPIENT_TYPE type)
{
    switch (type) {
        case NOTE_RECIPIENT_CHARACTER:  return "character";
        case NOTE_RECIPIENT_ACCOUNT:    return "account";
        case NOTE_RECIPIENT_CHURCH:     return "church";
        case NOTE_RECIPIENT_STAFF_RANK: return "staff_rank";
        case NOTE_RECIPIENT_STAFF_DUTY: return "staff_duty";
        case NOTE_RECIPIENT_ALL:        return "all";
        default:                        return "character";
    }
}

/**
 * string_to_recipient_type - Convert a string to NOTE_RECIPIENT_TYPE enum
 *
 * @param str  String representation of the recipient type
 * @return     The corresponding enum value, defaults to NOTE_RECIPIENT_CHARACTER
 */
static NOTE_RECIPIENT_TYPE string_to_recipient_type(const char *str)
{
    if (!str) return NOTE_RECIPIENT_CHARACTER;

    if (!strcmp(str, "account"))    return NOTE_RECIPIENT_ACCOUNT;
    if (!strcmp(str, "church"))     return NOTE_RECIPIENT_CHURCH;
    if (!strcmp(str, "staff_rank")) return NOTE_RECIPIENT_STAFF_RANK;
    if (!strcmp(str, "staff_duty")) return NOTE_RECIPIENT_STAFF_DUTY;
    if (!strcmp(str, "all"))        return NOTE_RECIPIENT_ALL;

    return NOTE_RECIPIENT_CHARACTER;
}

/**
 * get_note_files - Map a note type to its file paths, list, and label
 *
 * @param type         Note type (NOTE_NOTE, NOTE_NEWS, NOTE_CHANGES)
 * @param json_path    Output: path to the JSON file
 * @param legacy_path  Output: path to the legacy .not file
 * @param list         Output: pointer to the linked list head pointer
 * @param label        Output: human-readable label for logging
 * @return             true if the type is valid, false otherwise
 */
static bool get_note_files(int type, const char **json_path,
    const char **legacy_path, NOTE_DATA ***list, const char **label)
{
    switch (type) {
        case NOTE_NOTE:
            *json_path   = NOTE_JSON_FILE;
            *legacy_path = NOTE_NOT_FILE;
            *list        = &note_list;
            *label       = "notes";
            return true;
        case NOTE_NEWS:
            *json_path   = NEWS_JSON_FILE;
            *legacy_path = NEWS_NOT_FILE;
            *list        = &news_list;
            *label       = "news";
            return true;
        case NOTE_CHANGES:
            *json_path   = CHANGES_JSON_FILE;
            *legacy_path = CHANGES_NOT_FILE;
            *list        = &changes_list;
            *label       = "changes";
            return true;
        default:
            return false;
    }
}

/***************************************************************************
 * Note Serialization                                                      *
 ***************************************************************************/

/**
 * json_note_serialize - Serialize a single note to a JSON object
 *
 * @param note  Note data to serialize
 * @return      JSON object or NULL on error
 */
json_t *json_note_serialize(NOTE_DATA *note)
{
    if (!note) return NULL;

    json_t *json = json_object();

    json_object_set_new(json, "sender", json_string_safe(note->sender));
    json_object_set_new(json, "date", json_string_safe(note->date));
    json_object_set_new(json, "date_stamp", json_integer((json_int_t)note->date_stamp));
    json_object_set_new(json, "subject", json_string_safe(note->subject));
    json_object_set_new(json, "text", json_string_safe(note->text));
    json_object_set_new(json, "to_list", json_string_safe(note->to_list));
    json_object_set_new(json, "recipient_type", json_string(recipient_type_to_string(note->recipient_type)));

    if (note->to_characters && note->to_characters[0])
        json_object_set_new(json, "to_characters", json_string(note->to_characters));

    if (note->to_accounts && note->to_accounts[0])
        json_object_set_new(json, "to_accounts", json_string(note->to_accounts));

    if (note->to_churches && note->to_churches[0])
        json_object_set_new(json, "to_churches", json_string(note->to_churches));

    if (note->to_staff_ranks && note->to_staff_ranks[0])
        json_object_set_new(json, "to_staff_ranks", json_string(note->to_staff_ranks));

    if (note->to_staff_duties && note->to_staff_duties[0])
        json_object_set_new(json, "to_staff_duties", json_string(note->to_staff_duties));

    return json;
}

/**
 * json_note_deserialize - Deserialize a JSON object into a NOTE_DATA
 *
 * Allocates a new note via new_note() and populates fields from JSON.
 * The note->type field is NOT set here; the caller must set it after
 * deserialization based on which file was loaded.
 *
 * @param json  JSON object to deserialize
 * @return      New NOTE_DATA or NULL on error
 */
NOTE_DATA *json_note_deserialize(json_t *json)
{
    if (!json || json_is_null(json)) return NULL;

    NOTE_DATA *note = new_note();
    const char *str;

    str = json_string_value(json_object_get(json, "sender"));
    note->sender = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "date"));
    note->date = str_dup(str ? str : "");

    json_t *stamp = json_object_get(json, "date_stamp");
    if (stamp) note->date_stamp = (time_t)json_integer_value(stamp);

    str = json_string_value(json_object_get(json, "subject"));
    note->subject = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "text"));
    note->text = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "to_list"));
    note->to_list = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "recipient_type"));
    note->recipient_type = string_to_recipient_type(str);

    str = json_string_value(json_object_get(json, "to_characters"));
    note->to_characters = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "to_accounts"));
    note->to_accounts = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "to_churches"));
    note->to_churches = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "to_staff_ranks"));
    note->to_staff_ranks = str_dup(str ? str : "");

    str = json_string_value(json_object_get(json, "to_staff_duties"));
    note->to_staff_duties = str_dup(str ? str : "");

    note->valid = true;

    return note;
}

/***************************************************************************
 * File I/O                                                                *
 ***************************************************************************/

/**
 * json_save_notes - Save all notes of a given type to their JSON file
 *
 * Walks the linked list for the specified note type and serializes
 * each entry into a JSON array, then writes to the appropriate file.
 *
 * @param type  Note type (NOTE_NOTE, NOTE_NEWS, or NOTE_CHANGES)
 * @return      true on success, false on error
 */
bool json_save_notes(int type)
{
    const char *json_path, *legacy_path, *label;
    NOTE_DATA **list;

    if (!get_note_files(type, &json_path, &legacy_path, &list, &label))
        return false;

    json_t *root = json_object();
    json_object_set_new(root, "version", json_integer(1));

    json_t *notes_array = json_array();

    for (NOTE_DATA *pnote = *list; pnote != NULL; pnote = pnote->next) {
        json_t *note_json = json_note_serialize(pnote);
        if (note_json) {
            json_array_append_new(notes_array, note_json);
        }
    }

    json_object_set_new(root, "notes", notes_array);

    if (!json_file_save(root, json_path, "json_save_notes", JSON_INDENT(2)))
        return false;

    log_stringf("Saved %s to %s", label, json_path);
    return true;
}

/**
 * json_load_notes - Load all notes of a given type from their JSON file
 *
 * Checks for the JSON file first; if not found, checks for the legacy
 * .not file and returns false to let the legacy loader handle it.
 * If neither file exists, returns true (empty list is not an error).
 *
 * @param type  Note type (NOTE_NOTE, NOTE_NEWS, or NOTE_CHANGES)
 * @return      true on success, false on error or legacy fallback needed
 */
bool json_load_notes(int type)
{
    const char *json_path, *legacy_path, *label;
    NOTE_DATA **list;

    if (!get_note_files(type, &json_path, &legacy_path, &list, &label))
        return false;

    if (access(json_path, F_OK) != 0) {
        if (access(legacy_path, F_OK) == 0) {
            log_stringf("%s not found, legacy %s will be loaded", json_path, legacy_path);
            return false;
        }
        log_stringf("No %s file found (tried .json and .not)", label);
        return true;
    }

    json_t *notes_array;
    json_t *root = json_file_load(json_path, "notes", &notes_array, "json_load_notes");
    if (!root)
        return false;

    NOTE_DATA *last = NULL;
    size_t idx;
    json_t *elem;

    json_array_foreach(notes_array, idx, elem) {
        NOTE_DATA *note = json_note_deserialize(elem);
        if (note) {
            note->type = type;
            JSON_APPEND_LINK(*list, last, note);
        }
    }

    json_decref(root);

    log_stringf("Loaded %s from %s", label, json_path);
    return true;
}
