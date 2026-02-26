/***************************************************************************
 *  JSON Project Format - Project Data Serialization                       *
 *                                                                          *
 *  This file contains serialization for project management data:          *
 *  - Projects with builders, inquiries, and nested replies                *
 *  - STRING_DATA area lists serialized as JSON string arrays              *
 *                                                                          *
 *  Storage: data/world/projects.json (replaces projects.dat)              *
 ***************************************************************************/

#ifndef JSON_PROJECTS_H
#define JSON_PROJECTS_H

#include "../../merc.h"

bool json_load_projects(const char *path);
bool json_save_projects(const char *path);

#endif /* JSON_PROJECTS_H */
