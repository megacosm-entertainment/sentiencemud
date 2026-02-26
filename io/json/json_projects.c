/***************************************************************************
 *  JSON Project Format - Project Data Serialization                       *
 *                                                                          *
 *  This file contains serialization for project management data:          *
 *  - Projects with builders, inquiries, and nested replies                *
 *  - STRING_DATA area lists serialized as JSON string arrays              *
 *                                                                          *
 *  Storage: data/world/projects.json (replaces projects.dat)              *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>
#include "../../merc.h"
#include "json_projects.h"
#include "json_common.h"
#include "../../recycle.h"

extern PROJECT_DATA *project_list;
extern PROJECT_INQUIRY_DATA *project_inquiry_list;

/***************************************************************************
 * Forward Declarations                                                    *
 ***************************************************************************/

static json_t *inquiry_to_json(PROJECT_INQUIRY_DATA *pinq);
static PROJECT_INQUIRY_DATA *json_to_inquiry(json_t *json, PROJECT_DATA *project,
                                             PROJECT_INQUIRY_DATA *parent);

/***************************************************************************
 * Serialization Helpers                                                   *
 ***************************************************************************/

static json_t *builder_to_json(PROJECT_BUILDER_DATA *pb)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "name", json_string_safe(pb->name));
    json_object_set_new(obj, "minutes", json_integer(pb->minutes));
    json_object_set_new(obj, "assigned", json_integer((long)pb->assigned));

    return obj;
}

static json_t *inquiry_to_json(PROJECT_INQUIRY_DATA *pinq)
{
    json_t *obj = json_object();
    json_t *replies_arr = json_array();
    PROJECT_INQUIRY_DATA *reply;

    json_object_set_new(obj, "sender", json_string_safe(pinq->sender));
    json_object_set_new(obj, "subject", json_string_safe(pinq->subject));
    json_object_set_new(obj, "text", json_string_safe(pinq->text));
    json_object_set_new(obj, "date", json_integer((long)pinq->date));
    json_object_set_new(obj, "closed", json_integer((long)pinq->closed));

    if (pinq->closed_by && str_cmp(pinq->closed_by, "(null)"))
        json_object_set_new(obj, "closed_by", json_string(pinq->closed_by));
    else
        json_object_set_new(obj, "closed_by", json_null());

    for (reply = pinq->replies; reply != NULL; reply = reply->next)
        json_array_append_new(replies_arr, inquiry_to_json(reply));

    json_object_set_new(obj, "replies", replies_arr);

    return obj;
}

static json_t *project_to_json(PROJECT_DATA *project)
{
    json_t *obj = json_object();
    json_t *areas_arr = json_array();
    json_t *builders_arr = json_array();
    json_t *inquiries_arr = json_array();
    STRING_DATA *str;
    PROJECT_BUILDER_DATA *pb;
    PROJECT_INQUIRY_DATA *pinq;

    json_object_set_new(obj, "name", json_string_safe(project->name));
    json_object_set_new(obj, "leader", json_string_safe(project->leader));
    json_object_set_new(obj, "description", json_string_safe(project->description));
    json_object_set_new(obj, "summary", json_string_safe(project->summary));
    json_object_set_new(obj, "security", json_integer(project->security));
    json_object_set_new(obj, "project_flags", json_integer(project->project_flags));
    json_object_set_new(obj, "created", json_integer((long)project->created));
    json_object_set_new(obj, "completed", json_integer(project->completed));

    for (str = project->areas; str != NULL; str = str->next)
        json_array_append_new(areas_arr, json_string_safe(str->string));
    json_object_set_new(obj, "areas", areas_arr);

    for (pb = project->builders; pb != NULL; pb = pb->next)
        json_array_append_new(builders_arr, builder_to_json(pb));
    json_object_set_new(obj, "builders", builders_arr);

    for (pinq = project->inquiries; pinq != NULL; pinq = pinq->next)
        json_array_append_new(inquiries_arr, inquiry_to_json(pinq));
    json_object_set_new(obj, "inquiries", inquiries_arr);

    return obj;
}

/***************************************************************************
 * Save                                                                    *
 ***************************************************************************/

bool json_save_projects(const char *path)
{
    json_t *root = json_object();
    json_t *projects_arr = json_array();
    PROJECT_DATA *project;

    json_object_set_new(root, "version", json_integer(1));

    for (project = project_list; project != NULL; project = project->next)
        json_array_append_new(projects_arr, project_to_json(project));

    json_object_set_new(root, "projects", projects_arr);

    if (!json_file_save(root, path, "json_save_projects", JSON_INDENT(2) | JSON_PRESERVE_ORDER))
        return false;

    log_string("project.c, save_projects - projects saved to JSON");
    return true;
}

/***************************************************************************
 * Deserialization Helpers                                                 *
 ***************************************************************************/

static PROJECT_BUILDER_DATA *json_to_builder(json_t *json, PROJECT_DATA *project)
{
    PROJECT_BUILDER_DATA *pb = new_project_builder();
    json_t *val;

    val = json_object_get(json, "name");
    if (val && json_is_string(val))
        pb->name = str_dup(json_string_value(val));

    val = json_object_get(json, "minutes");
    if (val) pb->minutes = json_integer_value(val);

    val = json_object_get(json, "assigned");
    if (val) pb->assigned = (time_t)json_integer_value(val);

    pb->project = project;
    return pb;
}

static void add_inquiry_to_global_list(PROJECT_INQUIRY_DATA *pinq)
{
    if (project_inquiry_list == NULL) {
        project_inquiry_list = pinq;
    } else {
        pinq->next_global = project_inquiry_list;
        project_inquiry_list = pinq;
    }
}

static PROJECT_INQUIRY_DATA *json_to_inquiry(json_t *json, PROJECT_DATA *project,
                                             PROJECT_INQUIRY_DATA *parent)
{
    PROJECT_INQUIRY_DATA *pinq = new_project_inquiry();
    json_t *val, *replies_arr, *elem;
    size_t idx;
    PROJECT_INQUIRY_DATA *reply, *last_reply = NULL;

    val = json_object_get(json, "sender");
    if (val && json_is_string(val))
        pinq->sender = str_dup(json_string_value(val));

    val = json_object_get(json, "subject");
    if (val && json_is_string(val))
        pinq->subject = str_dup(json_string_value(val));

    val = json_object_get(json, "text");
    if (val && json_is_string(val))
        pinq->text = str_dup(json_string_value(val));

    val = json_object_get(json, "date");
    if (val) pinq->date = (time_t)json_integer_value(val);

    val = json_object_get(json, "closed");
    if (val) pinq->closed = (time_t)json_integer_value(val);

    val = json_object_get(json, "closed_by");
    if (val && json_is_string(val))
        pinq->closed_by = str_dup(json_string_value(val));
    else
        pinq->closed_by = str_dup("(null)");

    pinq->project = project;
    pinq->parent = parent;

    add_inquiry_to_global_list(pinq);

    replies_arr = json_object_get(json, "replies");
    if (replies_arr && json_is_array(replies_arr)) {
        json_array_foreach(replies_arr, idx, elem) {
            reply = json_to_inquiry(elem, project, pinq);
            JSON_APPEND_LINK(pinq->replies, last_reply, reply);
        }
    }

    return pinq;
}

/***************************************************************************
 * Load                                                                    *
 ***************************************************************************/

bool json_load_projects(const char *path)
{
    json_t *root, *projects_arr, *elem;
    size_t idx;

    if (access(path, F_OK) != 0)
        return false;

    root = json_file_load(path, "projects", &projects_arr, "json_load_projects");
    if (!root)
        return false;

    json_array_foreach(projects_arr, idx, elem) {
        PROJECT_DATA *project = new_project();
        json_t *val, *arr, *sub;
        size_t i;
        STRING_DATA *str, *last_str = NULL;
        PROJECT_BUILDER_DATA *pb, *last_pb = NULL;
        PROJECT_INQUIRY_DATA *pinq, *last_pinq = NULL;

        val = json_object_get(elem, "name");
        if (val && json_is_string(val))
            project->name = str_dup(json_string_value(val));

        val = json_object_get(elem, "leader");
        if (val && json_is_string(val))
            project->leader = str_dup(json_string_value(val));

        val = json_object_get(elem, "description");
        if (val && json_is_string(val))
            project->description = str_dup(json_string_value(val));

        val = json_object_get(elem, "summary");
        if (val && json_is_string(val))
            project->summary = str_dup(json_string_value(val));

        val = json_object_get(elem, "security");
        if (val) project->security = json_integer_value(val);

        val = json_object_get(elem, "project_flags");
        if (val) project->project_flags = json_integer_value(val);

        val = json_object_get(elem, "created");
        if (val) project->created = (time_t)json_integer_value(val);

        val = json_object_get(elem, "completed");
        if (val) project->completed = json_integer_value(val);

        arr = json_object_get(elem, "areas");
        if (arr && json_is_array(arr)) {
            json_array_foreach(arr, i, sub) {
                if (!json_is_string(sub)) continue;
                str = new_string_data();
                str->string = str_dup(json_string_value(sub));
                if (project->areas == NULL)
                    project->areas = str;
                else
                    last_str->next = str;
                last_str = str;
            }
        }

        arr = json_object_get(elem, "builders");
        if (arr && json_is_array(arr)) {
            json_array_foreach(arr, i, sub) {
                pb = json_to_builder(sub, project);
                JSON_APPEND_LINK(project->builders, last_pb, pb);
            }
        }

        arr = json_object_get(elem, "inquiries");
        if (arr && json_is_array(arr)) {
            json_array_foreach(arr, i, sub) {
                pinq = json_to_inquiry(sub, project, NULL);
                JSON_APPEND_LINK(project->inquiries, last_pinq, pinq);
            }
        }

        project->next = project_list;
        project_list = project;

        log_stringf("json_load_projects: loaded project %s (%s), leader %s",
                     project->name, project->summary, project->leader);
    }

    json_decref(root);
    log_string("Projects loaded from projects.json");
    return true;
}
