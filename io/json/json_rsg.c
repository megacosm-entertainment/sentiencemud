#include <ctype.h>
#include <dirent.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../merc.h"
#include "json_common.h"
#include "json_rsg.h"

#define RSG_JSON_FILE SYSTEM_DIR "rsg.json"
#define RSG_JSON_DIR SYSTEM_DIR "rsg_generators"
#define RSG_JSON_VERSION 1
#define RSG_JSON_GENERATOR_FORMAT "rsg_generator"

static bool rsg_filename_matches(const char *name)
{
    size_t len;

    if (IS_NULLSTR(name))
        return false;

    len = strlen(name);
    if (name[0] == '.')
        return false;

    if (len < 6)
        return false;

    return !str_cmp(name + len - 5, ".json");
}

static bool rsg_parse_placeholder(const char *ptr, int *consumed,
                                  char *class_name, size_t class_name_size)
{
    const char *p;
    size_t len = 0;

    if (!ptr || ptr[0] != '{')
        return false;

    if (ptr[1] == '{')
        return false;

    p = ptr + 1;
    if (!*p || !(isalpha((unsigned char)*p) || *p == '_'))
        return false;

    while (*p && *p != '}') {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-'))
            return false;

        if (class_name && class_name_size > 0 && len < class_name_size - 1)
            class_name[len] = *p;
        len++;
        p++;
    }

    if (*p != '}' || len == 0)
        return false;

    if (class_name && class_name_size > 0) {
        if (len >= class_name_size)
            len = class_name_size - 1;
        class_name[len] = '\0';
    }

    if (consumed)
        *consumed = (int)(p - ptr + 1);

    return true;
}

static void rsg_slugify_name(const char *name, char *out, size_t out_size)
{
    size_t i = 0;

    if (!out || out_size == 0)
        return;

    if (IS_NULLSTR(name)) {
        snprintf(out, out_size, "generator");
        return;
    }

    while (*name && i < out_size - 1) {
        unsigned char c = (unsigned char)*name;
        if (isalnum(c))
            out[i++] = (char)tolower(c);
        else if (c == '_' || c == '-' || isspace(c))
            out[i++] = '_';
        name++;
    }

    out[i] = '\0';
    if (i == 0)
        snprintf(out, out_size, "generator");
}

static void rsg_generator_path(const RANDOM_STRING *rsg, char *out, size_t out_size)
{
    char slug[MIL];

    if (!out || out_size == 0)
        return;

    if (!rsg || rsg->uid <= 0) {
        snprintf(out, out_size, "%s/rsg_0_generator.json", RSG_JSON_DIR);
        return;
    }

    rsg_slugify_name(rsg->name, slug, sizeof(slug));
    snprintf(out, out_size, "%s/rsg_%06ld_%s.json", RSG_JSON_DIR, rsg->uid, slug);
}

static void rsg_remove_saved_generator_files(void)
{
    DIR *dir;
    struct dirent *ent;

    dir = opendir(RSG_JSON_DIR);
    if (!dir)
        return;

    while ((ent = readdir(dir)) != NULL) {
        char path[MSL];

        if (!rsg_filename_matches(ent->d_name))
            continue;

        snprintf(path, sizeof(path), "%s/%s", RSG_JSON_DIR, ent->d_name);
        remove(path);
    }

    closedir(dir);
}

static RANDOM_STRING *rsg_find_by_uid(RANDOM_STRING *rsg_list, long uid)
{
    RANDOM_STRING *rsg;

    for (rsg = rsg_list; rsg; rsg = rsg->next)
        if (rsg->uid == uid)
            return rsg;

    return NULL;
}

static RANDOM_STRING *rsg_find_by_name(RANDOM_STRING *rsg_list, const char *name)
{
    RANDOM_STRING *rsg;

    if (IS_NULLSTR(name))
        return NULL;

    for (rsg = rsg_list; rsg; rsg = rsg->next)
        if (!str_cmp(rsg->name, name))
            return rsg;

    return NULL;
}

static void rsg_list_prepend(RANDOM_STRING **rsg_list, RANDOM_STRING *rsg)
{
    if (!rsg || !rsg_list)
        return;

    rsg->next = *rsg_list;
    *rsg_list = rsg;
}

static void rsg_append_pattern(RANDOM_STRING *rsg, RANDOM_PATTERN *pattern)
{
    if (!rsg->p_head)
        rsg->p_head = pattern;
    else
        rsg->p_tail->next = pattern;

    rsg->p_tail = pattern;
    rsg->patterns++;
}

static void rsg_append_class(RANDOM_STRING *rsg, RANDOM_CLASS *cls)
{
    if (!rsg->c_head)
        rsg->c_head = cls;
    else
        rsg->c_tail->next = cls;

    rsg->c_tail = cls;
    rsg->classes++;
}

static void rsg_append_entry(RANDOM_CLASS *cls, RANDOM_STRING_ENTRY *entry)
{
    if (!cls->e_head)
        cls->e_head = entry;
    else
        cls->e_tail->next = entry;

    cls->e_tail = entry;
    cls->entries++;
}

static RANDOM_CLASS *rsg_class_find_by_name(RANDOM_STRING *rsg, const char *name)
{
    RANDOM_CLASS *cls;

    if (!rsg || IS_NULLSTR(name))
        return NULL;

    for (cls = rsg->c_head; cls; cls = cls->next)
        if (!str_cmp(cls->name, name))
            return cls;

    return NULL;
}

static void rsg_pattern_rebuild_refs(RANDOM_STRING *rsg, RANDOM_PATTERN *pattern)
{
    char class_name[MIL];
    int consumed;
    int class_count = 0;
    RANDOM_CLASS *found;
    RANDOM_CLASS **new_refs = NULL;
    const char *ptr;

    if (!rsg || !pattern)
        return;

    if (pattern->c_list) {
        free_mem(pattern->c_list, sizeof(RANDOM_CLASS *) * pattern->classes);
        pattern->c_list = NULL;
    }
    pattern->classes = 0;

    ptr = pattern->name;
    while (*ptr) {
        int i = 0;
        bool already_added = false;

        if (*ptr == '{' && *(ptr + 1) == '{') {
            ptr += 2;
            continue;
        }

        if (*ptr != '{') {
            ptr++;
            continue;
        }

        if (!rsg_parse_placeholder(ptr, &consumed, class_name, sizeof(class_name))) {
            ptr++;
            continue;
        }
        ptr += consumed;

        found = rsg_class_find_by_name(rsg, class_name);
        if (!found)
            continue;

        for (i = 0; i < class_count; i++) {
            if (new_refs[i] == found) {
                already_added = true;
                break;
            }
        }

        if (already_added)
            continue;

        RANDOM_CLASS **grown = alloc_mem(sizeof(RANDOM_CLASS *) * (class_count + 1));
        for (i = 0; i < class_count; i++)
            grown[i] = new_refs[i];
        grown[class_count++] = found;

        if (new_refs)
            free_mem(new_refs, sizeof(RANDOM_CLASS *) * (class_count - 1));
        new_refs = grown;
    }

    pattern->classes = class_count;
    pattern->c_list = new_refs;
}

static void rsg_rebuild_all_pattern_refs(RANDOM_STRING *rsg)
{
    RANDOM_PATTERN *pattern;

    for (pattern = rsg->p_head; pattern; pattern = pattern->next)
        rsg_pattern_rebuild_refs(rsg, pattern);
}

static json_t *rsg_entry_to_json(RANDOM_STRING_ENTRY *entry)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "weight", json_integer(entry->weight));
    json_object_set_new(obj, "text", json_string(entry->str ? entry->str : ""));
    return obj;
}

static json_t *rsg_class_to_json(RANDOM_CLASS *cls)
{
    json_t *obj = json_object();
    json_t *entries = json_array();
    RANDOM_STRING_ENTRY *entry;

    json_object_set_new(obj, "uid", json_integer(cls->uid));
    json_object_set_new(obj, "name", json_string(cls->name ? cls->name : ""));

    for (entry = cls->e_head; entry; entry = entry->next)
        json_array_append_new(entries, rsg_entry_to_json(entry));

    json_object_set_new(obj, "entries", entries);
    return obj;
}

static json_t *rsg_pattern_to_json(RANDOM_PATTERN *pattern)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "weight", json_integer(pattern->weight));
    json_object_set_new(obj, "template", json_string(pattern->name ? pattern->name : ""));
    return obj;
}

static json_t *rsg_generator_to_json(RANDOM_STRING *rsg)
{
    json_t *obj = json_object();
    json_t *patterns = json_array();
    json_t *classes = json_array();
    RANDOM_PATTERN *pattern;
    RANDOM_CLASS *cls;

    json_object_set_new(obj, "uid", json_integer(rsg->uid));
    json_object_set_new(obj, "name", json_string(rsg->name ? rsg->name : ""));
    json_object_set_new(obj, "description", json_string(rsg->descr ? rsg->descr : ""));

    for (pattern = rsg->p_head; pattern; pattern = pattern->next)
        json_array_append_new(patterns, rsg_pattern_to_json(pattern));

    for (cls = rsg->c_head; cls; cls = cls->next)
        json_array_append_new(classes, rsg_class_to_json(cls));

    json_object_set_new(obj, "patterns", patterns);
    json_object_set_new(obj, "classes", classes);
    return obj;
}

static bool rsg_load_class_entries(RANDOM_CLASS *cls, json_t *entries)
{
    size_t i;
    json_t *entry_obj;

    if (!entries || !json_is_array(entries))
        return true;

    json_array_foreach(entries, i, entry_obj) {
        RANDOM_STRING_ENTRY *entry;
        const char *text;

        if (!json_is_object(entry_obj))
            continue;

        text = json_get_string(entry_obj, "text", "");

        entry = alloc_mem(sizeof(*entry));
        memset(entry, 0, sizeof(*entry));
        entry->weight = json_get_int(entry_obj, "weight", 1);
        if (entry->weight < 1)
            entry->weight = 1;
        entry->str = str_dup(text);

        rsg_append_entry(cls, entry);
    }

    return true;
}

static RANDOM_CLASS *rsg_class_from_json(json_t *obj)
{
    RANDOM_CLASS *cls;
    const char *name;
    json_t *entries;

    if (!obj || !json_is_object(obj))
        return NULL;

    name = json_get_string(obj, "name", NULL);
    if (IS_NULLSTR(name))
        return NULL;

    cls = alloc_mem(sizeof(*cls));
    memset(cls, 0, sizeof(*cls));

    cls->uid = json_get_int(obj, "uid", 0);
    cls->name = str_dup(name);

    entries = json_object_get(obj, "entries");
    rsg_load_class_entries(cls, entries);
    return cls;
}

static RANDOM_PATTERN *rsg_pattern_from_json(json_t *obj)
{
    RANDOM_PATTERN *pattern;
    const char *template;

    if (!obj || !json_is_object(obj))
        return NULL;

    template = json_get_string(obj, "template", NULL);
    if (IS_NULLSTR(template))
        return NULL;

    pattern = alloc_mem(sizeof(*pattern));
    memset(pattern, 0, sizeof(*pattern));
    pattern->weight = json_get_int(obj, "weight", 1);
    if (pattern->weight < 1)
        pattern->weight = 1;
    pattern->name = str_dup(template);
    return pattern;
}

static RANDOM_STRING *rsg_generator_from_json(json_t *obj)
{
    RANDOM_STRING *rsg;
    const char *name;
    const char *descr;
    json_t *patterns;
    json_t *classes;
    json_t *pattern_obj;
    json_t *class_obj;
    size_t i;

    if (!obj || !json_is_object(obj))
        return NULL;

    name = json_get_string(obj, "name", NULL);
    if (IS_NULLSTR(name))
        return NULL;

    descr = json_get_string(obj, "description", "");

    rsg = alloc_mem(sizeof(*rsg));
    memset(rsg, 0, sizeof(*rsg));
    rsg->uid = json_get_int(obj, "uid", 0);
    rsg->name = str_dup(name);
    rsg->descr = str_dup(descr);

    classes = json_object_get(obj, "classes");
    if (classes && json_is_array(classes)) {
        json_array_foreach(classes, i, class_obj) {
            RANDOM_CLASS *cls = rsg_class_from_json(class_obj);
            if (cls)
                rsg_append_class(rsg, cls);
        }
    }

    patterns = json_object_get(obj, "patterns");
    if (patterns && json_is_array(patterns)) {
        json_array_foreach(patterns, i, pattern_obj) {
            RANDOM_PATTERN *pattern = rsg_pattern_from_json(pattern_obj);
            if (pattern)
                rsg_append_pattern(rsg, pattern);
        }
    }

    rsg_rebuild_all_pattern_refs(rsg);
    return rsg;
}

static bool rsg_register_loaded_generator(RANDOM_STRING **rsg_list, RANDOM_STRING *rsg,
    long *max_uid, int *loaded_count)
{
    if (!rsg)
        return false;

    if (rsg->uid <= 0)
        rsg->uid = (*max_uid) + 1;

    if (rsg_find_by_uid(*rsg_list, rsg->uid) || rsg_find_by_name(*rsg_list, rsg->name))
        return false;

    if (rsg->uid > *max_uid)
        *max_uid = rsg->uid;

    rsg_list_prepend(rsg_list, rsg);
    (*loaded_count)++;
    return true;
}

static void rsg_load_legacy_aggregate(RANDOM_STRING **rsg_list, long *max_uid, int *loaded_count)
{
    json_t *root;
    json_t *generators;
    json_t *rsg_obj;
    size_t i;
    FILE *fp;

    fp = fopen(RSG_JSON_FILE, "r");
    if (!fp)
        return;
    fclose(fp);

    root = json_file_load(RSG_JSON_FILE, NULL, NULL, "json_rsg_load_generators");
    if (!root)
        return;

    generators = json_object_get(root, "generators");
    if (!generators || !json_is_array(generators)) {
        json_decref(root);
        return;
    }

    json_array_foreach(generators, i, rsg_obj) {
        RANDOM_STRING *rsg = rsg_generator_from_json(rsg_obj);
        rsg_register_loaded_generator(rsg_list, rsg, max_uid, loaded_count);
    }

    json_decref(root);
}

bool json_rsg_save_generators(RANDOM_STRING *rsg_list, long next_uid, int *saved_count)
{
    RANDOM_STRING *rsg;
    int local_saved = 0;

    mkdir(RSG_JSON_DIR, 0755);
    rsg_remove_saved_generator_files();

    for (rsg = rsg_list; rsg; rsg = rsg->next) {
        char path[MSL];
        json_t *root = json_object();
        json_object_set_new(root, "_format", json_string(RSG_JSON_GENERATOR_FORMAT));
        json_object_set_new(root, "version", json_integer(RSG_JSON_VERSION));
        json_object_set_new(root, "next_uid", json_integer(next_uid));
        json_object_set_new(root, "generator", rsg_generator_to_json(rsg));
        rsg_generator_path(rsg, path, sizeof(path));

        if (!json_file_save(root, path, "json_rsg_save_generators",
            JSON_INDENT(2) | JSON_PRESERVE_ORDER)) {
            return false;
        }

        local_saved++;
    }

    if (saved_count)
        *saved_count = local_saved;

    return true;
}

RANDOM_STRING *json_rsg_load_generators(long *next_uid_out, int *loaded_count)
{
    RANDOM_STRING *rsg_list = NULL;
    DIR *dir;
    struct dirent *ent;
    long max_uid = 0;
    int local_loaded = 0;

    dir = opendir(RSG_JSON_DIR);
    if (dir) {
        while ((ent = readdir(dir)) != NULL) {
            char path[MSL];
            json_t *root;
            json_t *generator_obj;
            RANDOM_STRING *rsg;

            if (!rsg_filename_matches(ent->d_name))
                continue;

            snprintf(path, sizeof(path), "%s/%s", RSG_JSON_DIR, ent->d_name);
            root = json_file_load(path, NULL, NULL, "json_rsg_load_generators");
            if (!root)
                continue;

            generator_obj = json_object_get(root, "generator");
            if (!generator_obj || !json_is_object(generator_obj)) {
                json_decref(root);
                continue;
            }

            rsg = rsg_generator_from_json(generator_obj);
            rsg_register_loaded_generator(&rsg_list, rsg, &max_uid, &local_loaded);
            json_decref(root);
        }

        closedir(dir);
    }

    if (local_loaded == 0)
        rsg_load_legacy_aggregate(&rsg_list, &max_uid, &local_loaded);

    if (next_uid_out) {
        *next_uid_out = max_uid + 1;
        if (*next_uid_out <= max_uid)
            *next_uid_out = max_uid + 1;
    }

    if (loaded_count)
        *loaded_count = local_loaded;

    return rsg_list;
}
