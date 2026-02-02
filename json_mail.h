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

#ifndef JSON_MAIL_H
#define JSON_MAIL_H

#include <jansson.h>
#include "merc.h"

/* Save all mail to JSON file */
bool save_mail_json(void);

/* Load all mail from JSON file */
bool load_mail_json(void);

/* Convert a single mail package to JSON */
json_t *mail_to_json(MAIL_DATA *mail);

/* Convert JSON to a single mail package */
MAIL_DATA *json_to_mail(json_t *json);

#endif /* JSON_MAIL_H */
