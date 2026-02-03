/***************************************************************************
 *  JSON Mail Format - Mail Package Serialization with WNUM Support       *
 *                                                                          *
 *  This file contains serialization for mail packages:                    *
 *  - Mail metadata (sender, recipient, dates, status)                     *
 *  - Embedded objects within mail packages                                *
 *  - from_location and to_location as WNUM references                     *
 *  - Script tracking for automated mail systems                           *
 *                                                                          *
 *  Storage: data/mail.json (replaces mail.dat)                            *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <jansson.h>
#include "merc.h"
#include "json_mail.h"
#include "recycle.h"

/***************************************************************************
 * External References                                                     *
 ***************************************************************************/

extern MAIL_DATA *mail_list;
extern void obj_to_mail(OBJ_DATA *obj, MAIL_DATA *mail);
extern json_t *json_persist_object_to_json(OBJ_DATA *obj);
extern OBJ_DATA *json_persist_json_to_object(json_t *json);

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define MAIL_JSON_FILE "data/mail.json"
#define MAIL_DAT_FILE  "data/system/mail.dat"

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

/**
 * Create a WNUM reference as a JSON string
 * Format: "area_uid#vnum" or bare "vnum"
 */
static json_t *wnum_to_json_str(long area_uid, long vnum)
{
    char buf[256];
    
    if (area_uid > 0) {
        snprintf(buf, sizeof(buf), "%ld#%ld", area_uid, vnum);
    } else {
        snprintf(buf, sizeof(buf), "%ld", vnum);
    }
    
    return json_string(buf);
}

/**
 * Parse a WNUM from JSON string
 * Supports both "area_uid#vnum" and bare "vnum"
 * Returns area_uid (0 if not specified) and vnum
 */
static void parse_wnum_from_json(json_t *json, long *area_uid, long *vnum)
{
    const char *str;
    
    *area_uid = 0;
    *vnum = 0;
    
    if (!json || !json_is_string(json)) {
        return;
    }
    
    str = json_string_value(json);
    if (!str) {
        return;
    }
    
    /* Check for area_uid#vnum format */
    if (strchr(str, '#')) {
        sscanf(str, "%ld#%ld", area_uid, vnum);
    } else {
        /* Bare vnum */
        *vnum = atol(str);
    }
}

/***************************************************************************
 * Mail Serialization                                                      *
 ***************************************************************************/

/**
 * Convert a mail package to JSON
 */
json_t *mail_to_json(MAIL_DATA *mail)
{
    json_t *json, *array;
    OBJ_DATA *obj;
    
    if (!mail) {
        return NULL;
    }
    
    json = json_object();
    
    /* Basic fields */
    if (mail->sender) {
        json_object_set_new(json, "sender", json_string(mail->sender));
    }
    
    if (mail->recipient) {
        json_object_set_new(json, "recipient", json_string(mail->recipient));
    }
    
    if (mail->message) {
        json_object_set_new(json, "message", json_string(mail->message));
    }
    
    /* Timestamps */
    json_object_set_new(json, "sent_date", json_integer((json_int_t)mail->sent_date));
    json_object_set_new(json, "expire_date", json_integer((json_int_t)mail->expire_date));
    json_object_set_new(json, "deliver_date", json_integer((json_int_t)mail->deliver_date));
    
    /* Status and flags */
    json_object_set_new(json, "status", json_integer(mail->status));
    json_object_set_new(json, "picked_up", json_boolean(mail->picked_up));
    json_object_set_new(json, "scripted", json_boolean(mail->scripted));
    json_object_set_new(json, "return_service", json_boolean(mail->return_service));
    json_object_set_new(json, "timestamp_expiration", json_boolean(mail->timestamp_expiration));
    
    /* Script tracking */
    if (mail->originating_script != 0) {
        json_object_set_new(json, "originating_script", json_integer(mail->originating_script));
    }
    
    if (mail->orig_script_type > -1) {
        json_object_set_new(json, "orig_script_type", json_integer(mail->orig_script_type));
    }
    
    if (mail->collect_script != 0) {
        json_object_set_new(json, "collect_script", json_integer(mail->collect_script));
    }
    
    if (mail->expire_script != 0) {
        json_object_set_new(json, "expire_script", json_integer(mail->expire_script));
    }
    
    /* Location references with WNUM support */
    if (mail->from_location != 0) {
        ROOM_INDEX_DATA *from_room = get_room_index_global(mail->from_location);
        long from_auid = (from_room && from_room->area) ? from_room->area->uid : 0;
        json_object_set_new(json, "from_location", wnum_to_json_str(from_auid, mail->from_location));
    }
    
    if (mail->to_location != 0) {
        ROOM_INDEX_DATA *to_room = get_room_index_global(mail->to_location);
        long to_auid = (to_room && to_room->area) ? to_room->area->uid : 0;
        json_object_set_new(json, "to_location", wnum_to_json_str(to_auid, mail->to_location));
    }
    
    /* Objects in package */
    if (mail->objects) {
        array = json_array();
        for (obj = mail->objects; obj != NULL; obj = obj->next_content) {
            json_t *obj_json = json_persist_object_to_json(obj);
            if (obj_json) {
                json_array_append_new(array, obj_json);
            }
        }
        json_object_set_new(json, "objects", array);
    }
    
    return json;
}

/**
 * Convert JSON to a mail package
 */
MAIL_DATA *json_to_mail(json_t *json)
{
    MAIL_DATA *mail;
    json_t *value, *array;
    size_t idx;
    json_t *elem;
    long area_uid, vnum;
    
    if (!json || json_is_null(json)) {
        return NULL;
    }
    
    mail = new_mail();
    
    /* Basic fields */
    value = json_object_get(json, "sender");
    if (value) mail->sender = str_dup(json_string_value(value));
    
    value = json_object_get(json, "recipient");
    if (value) mail->recipient = str_dup(json_string_value(value));
    
    value = json_object_get(json, "message");
    if (value) mail->message = str_dup(json_string_value(value));
    
    /* Timestamps */
    value = json_object_get(json, "sent_date");
    if (value) mail->sent_date = (time_t)json_integer_value(value);
    
    value = json_object_get(json, "expire_date");
    if (value) mail->expire_date = (time_t)json_integer_value(value);
    
    value = json_object_get(json, "deliver_date");
    if (value) mail->deliver_date = (time_t)json_integer_value(value);
    
    /* Status and flags */
    value = json_object_get(json, "status");
    if (value) mail->status = json_integer_value(value);
    
    value = json_object_get(json, "picked_up");
    if (value) mail->picked_up = json_is_true(value);
    
    value = json_object_get(json, "scripted");
    if (value) mail->scripted = json_is_true(value);
    
    value = json_object_get(json, "return_service");
    if (value) mail->return_service = json_is_true(value);
    
    value = json_object_get(json, "timestamp_expiration");
    if (value) mail->timestamp_expiration = json_is_true(value);
    
    /* Script tracking */
    value = json_object_get(json, "originating_script");
    if (value) mail->originating_script = json_integer_value(value);
    
    value = json_object_get(json, "orig_script_type");
    if (value) mail->orig_script_type = json_integer_value(value);
    
    value = json_object_get(json, "collect_script");
    if (value) mail->collect_script = json_integer_value(value);
    
    value = json_object_get(json, "expire_script");
    if (value) mail->expire_script = json_integer_value(value);
    
    /* Location references */
    value = json_object_get(json, "from_location");
    if (value) {
        parse_wnum_from_json(value, &area_uid, &vnum);
        mail->from_location = vnum;
    }
    
    value = json_object_get(json, "to_location");
    if (value) {
        parse_wnum_from_json(value, &area_uid, &vnum);
        mail->to_location = vnum;
    }
    
    /* Objects */
    array = json_object_get(json, "objects");
    if (array && json_is_array(array)) {
        json_array_foreach(array, idx, elem) {
            OBJ_DATA *obj = json_persist_json_to_object(elem);
            if (obj) {
                obj_to_mail(obj, mail);
            }
        }
    }
    
    return mail;
}

/***************************************************************************
 * File I/O                                                                *
 ***************************************************************************/

/**
 * Save all mail to JSON file
 */
bool save_mail_json(void)
{
    json_t *root, *array;
    MAIL_DATA *mail;
    int ret;
    
    root = json_object();
    array = json_array();
    
    /* Convert all mail to JSON */
    for (mail = mail_list; mail != NULL; mail = mail->next) {
        json_t *mail_json = mail_to_json(mail);
        if (mail_json) {
            json_array_append_new(array, mail_json);
        }
    }
    
    json_object_set_new(root, "mail", array);
    json_object_set_new(root, "version", json_integer(1));
    
    /* Write to file */
    ret = json_dump_file(root, MAIL_JSON_FILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);
    
    if (ret != 0) {
        log_string("save_mail_json: Failed to write mail.json");
        return false;
    }
    
    log_string("Mail saved to mail.json");
    return true;
}

/**
 * Load all mail from JSON file
 */
bool load_mail_json(void)
{
    json_t *root, *array;
    json_error_t error;
    size_t idx;
    json_t *elem;
    MAIL_DATA *mail, *mail_last = NULL;
    
    /* Check if JSON file exists */
    if (access(MAIL_JSON_FILE, F_OK) != 0) {
        /* Try legacy .dat file */
        if (access(MAIL_DAT_FILE, F_OK) == 0) {
            log_string("mail.json not found, legacy mail.dat will be loaded by read_mail()");
            return false;
        }
        log_string("No mail file found (tried .json and .dat)");
        return true; /* Not an error, just no mail */
    }
    
    /* Load JSON file */
    root = json_load_file(MAIL_JSON_FILE, 0, &error);
    if (!root) {
        log_stringf("load_mail_json: JSON parse error on line %d: %s", error.line, error.text);
        return false;
    }
    
    /* Parse mail array */
    array = json_object_get(root, "mail");
    if (!array || !json_is_array(array)) {
        log_string("load_mail_json: 'mail' array not found in JSON");
        json_decref(root);
        return false;
    }
    
    /* Load each mail package */
    json_array_foreach(array, idx, elem) {
        mail = json_to_mail(elem);
        if (mail) {
            /* Add to mail list */
            mail->next = NULL;
            if (mail_list == NULL) {
                mail_list = mail;
            } else {
                mail_last->next = mail;
            }
            mail_last = mail;
        }
    }
    
    json_decref(root);
    
    /* If we loaded an empty JSON file, try .dat fallback */
    if (mail_list == NULL) {
        if (access(MAIL_DAT_FILE, F_OK) == 0) {
            log_string("mail.json is empty, falling back to mail.dat");
            return false;
        }
    }
    
    log_string("Mail loaded from mail.json");
    return true;
}
