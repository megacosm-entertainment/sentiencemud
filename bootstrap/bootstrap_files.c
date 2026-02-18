/**
 * bootstrap_files.c - File creation helpers for bootstrap
 *
 * Creates all the minimal required data files for a fresh installation.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>
#include "../merc.h"
#include "../skill_data.h"
#include "../skill_group.h"
#include "../song_data.h"
#include "../class_data.h"
#include "bootstrap.h"
#include "bootstrap_internal.h"

static bool ensure_area_in_area_list(const char *filename)
{
    FILE *fp = fopen("data/world/area.lst", "r");
    if (!fp) {
        return false;
    }

    char lines[256][256];
    int line_count = 0;
    bool exists = false;
    bool has_end = false;

    while (line_count < 256 && fgets(lines[line_count], sizeof(lines[line_count]), fp)) {
        size_t len = strlen(lines[line_count]);
        while (len > 0 && (lines[line_count][len - 1] == '\n' || lines[line_count][len - 1] == '\r')) {
            lines[line_count][--len] = '\0';
        }

        if (lines[line_count][0] == '$') {
            has_end = true;
        } else if (!str_cmp(lines[line_count], filename)) {
            exists = true;
        }
        line_count++;
    }
    fclose(fp);

    if (exists) {
        return true;
    }

    fp = fopen("data/world/area.lst", "w");
    if (!fp) {
        return false;
    }

    for (int i = 0; i < line_count; i++) {
        if (lines[i][0] == '$') {
            continue;
        }
        fprintf(fp, "%s\n", lines[i]);
    }

    fprintf(fp, "%s\n", filename);
    fprintf(fp, "$\n");
    fclose(fp);

    if (!has_end) {
        return true;
    }

    return true;
}

static bool directory_exists_local(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path, 0755) == 0;
}

static bool copy_file_contents(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) {
        return false;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    char buffer[8192];
    size_t bytes;
    bool ok = true;

    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, bytes, out) != bytes) {
            ok = false;
            break;
        }
    }

    if (ferror(in)) {
        ok = false;
    }

    fclose(in);
    if (fclose(out) != 0) {
        ok = false;
    }

    return ok;
}

static bool copy_directory_recursive(const char *src, const char *dst)
{
    DIR *dir = opendir(src);
    if (!dir) {
        return false;
    }

    if (!ensure_directory(dst)) {
        closedir(dir);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!str_cmp(entry->d_name, ".") || !str_cmp(entry->d_name, "..")) {
            continue;
        }

        char src_path[1024];
        char dst_path[1024];
        struct stat st;

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        if (stat(src_path, &st) != 0) {
            closedir(dir);
            return false;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!copy_directory_recursive(src_path, dst_path)) {
                closedir(dir);
                return false;
            }
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (!copy_file_contents(src_path, dst_path)) {
            closedir(dir);
            return false;
        }
    }

    closedir(dir);
    return true;
}

/**
 * file_exists - Check if a file exists
 *
 * @param path  File path to check
 * @return      true if file exists and is readable
 */
bool file_exists(const char *path)
{
    return (access(path, F_OK) == 0);
}

/**
 * ensure_directory_structure - Create required directories if missing
 *
 * Creates all directories needed for the game to function.
 *
 * @return true if all directories exist or were created successfully
 */
bool ensure_directory_structure(void)
{
    const char *dirs[] = {
        "data", "data/system", "data/world", "data/races",
        "data/tests", "data/tests/unit", "data/tests/integration",
        "data/skills", "data/skill_groups", "data/classes",
        "data/help", "data/notes", "data/orgs", "data/persist",
        "data/stats", "data/traits", "data/dump",
        "area", "accounts", "characters", "logs",
        "accounts/a", "accounts/b", "accounts/c", "accounts/d",
        "accounts/e", "accounts/f", "accounts/g", "accounts/h",
        "accounts/i", "accounts/j", "accounts/k", "accounts/l",
        "accounts/m", "accounts/n", "accounts/o", "accounts/p",
        "accounts/q", "accounts/r", "accounts/s", "accounts/t",
        "accounts/u", "accounts/v", "accounts/w", "accounts/x",
        "accounts/y", "accounts/z",
        "characters/a", "characters/b", "characters/c", "characters/d",
        "characters/e", "characters/f", "characters/g", "characters/h",
        "characters/i", "characters/j", "characters/k", "characters/l",
        "characters/m", "characters/n", "characters/o", "characters/p",
        "characters/q", "characters/r", "characters/s", "characters/t",
        "characters/u", "characters/v", "characters/w", "characters/x",
        "characters/y", "characters/z",
        NULL
    };

    for (int i = 0; dirs[i] != NULL; i++) {
        struct stat st;
        if (stat(dirs[i], &st) != 0) {
            if (mkdir(dirs[i], 0755) != 0) {
                fprintf(stderr, "Failed to create directory: %s\n", dirs[i]);
                return false;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Path exists but is not a directory: %s\n", dirs[i]);
            return false;
        }
    }

    return true;
}

/**
 * create_gconfig_rc - Create minimal gconfig.rc file
 *
 * @return true on success
 */
bool create_gconfig_rc(void)
{
    FILE *fp = fopen("data/system/gconfig.rc", "w");
    if (!fp) {
        perror("Failed to create gconfig.rc");
        return false;
    }

    fprintf(fp, "DBversion 16777217\n");
    fprintf(fp, "NextMobUID 1 0\n");
    fprintf(fp, "NextObjUID 1 0\n");
    fprintf(fp, "NextTokenUID 1 0\n");
    fprintf(fp, "NextVRoomUID 1 0\n");
    fprintf(fp, "NextShipUID 1 0\n");
    fprintf(fp, "NextAreaUID %d\n", bootstrap_ci_fixtures ? 4 : 2);
    fprintf(fp, "NextWildsUID 0\n");
    fprintf(fp, "NextVlinkUID 0\n");
    fprintf(fp, "NextChurchUID 1\n");
    fprintf(fp, "END\n");

    fclose(fp);
    return true;
}

/**
 * create_area_lst - Create minimal area.lst file
 *
 * @return true on success
 */
bool create_area_lst(void)
{
    FILE *fp = fopen("data/world/area.lst", "w");
    if (!fp) {
        perror("Failed to create area.lst");
        return false;
    }

    fprintf(fp, "limbo.json\n");
    fprintf(fp, "$\n");

    fclose(fp);
    return true;
}

/**
 * create_limbo_area - Create minimal limbo.json area file
 *
 * @return true on success
 */
bool create_limbo_area(void)
{
    json_t *root = json_object();
    json_t *area = json_object();
    json_t *vnums = json_object();
    json_t *metadata = json_object();
    json_t *levels = json_object();
    json_t *rooms = json_array();
    json_t *room = json_object();
    json_t *room_flags = json_array();

    json_object_set_new(root, "schema_version", json_string("1.0.0"));

    json_object_set_new(area, "uid", json_integer(1));
    json_object_set_new(area, "name", json_string("Limbo"));
    json_object_set_new(area, "filename", json_string("limbo.json"));

    json_object_set_new(vnums, "min", json_integer(1));
    json_object_set_new(vnums, "max", json_integer(10));
    json_object_set_new(area, "vnums", vnums);

    json_object_set_new(metadata, "builders", json_string("Bootstrap"));
    json_object_set_new(metadata, "security", json_integer(9));

    json_object_set_new(levels, "min", json_integer(0));
    json_object_set_new(levels, "max", json_integer(0));
    json_object_set_new(metadata, "levels", levels);

    json_object_set_new(area, "metadata", metadata);
    json_object_set_new(root, "area", area);

    json_object_set_new(room, "vnum", json_integer(1));
    json_object_set_new(room, "name", json_string("The Void"));
    json_object_set_new(room, "description",
        json_string("You float in an endless void. This is a bootstrap room created\n"
                   "for initial game setup. Use OLC commands to build your world.\n\r"));

    json_array_append_new(room_flags, json_string("indoors"));
    json_array_append_new(room_flags, json_string("no_mob"));
    json_array_append_new(room_flags, json_string("safe"));
    json_object_set_new(room, "flags", room_flags);
    json_object_set_new(room, "sector", json_string("inside"));

    json_array_append_new(rooms, room);
    json_object_set_new(root, "rooms", rooms);

    if (json_dump_file(root, "area/limbo.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to write limbo.json\n");
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}

/**
 * create_human_race - Create minimal human race file if missing
 *
 * @return true on success (or if races already exist)
 */
bool create_human_race(void)
{
    DIR *dir;
    struct dirent *ent;
    int race_count = 0;

    dir = opendir("data/races/");
    if (dir) {
        while ((ent = readdir(dir)) != NULL) {
            size_t len = strlen(ent->d_name);
            if (len > 5 && strcmp(ent->d_name + len - 5, ".json") == 0) {
                race_count++;
            }
        }
        closedir(dir);
    }

    if (race_count > 0) {
        return true;
    }

    fprintf(stderr, "\nWARNING: No race files found in data/races/\n");
    fprintf(stderr, "Creating minimal human.json for bootstrap...\n");

    json_t *root = json_object();
    json_t *physical = json_object();
    json_t *form = json_array();
    json_t *parts = json_array();
    json_t *combat = json_object();
    json_t *attributes = json_object();
    json_t *stats = json_object();
    json_t *max_vitals = json_object();

    json_object_set_new(root, "_version", json_string("1.0"));
    json_object_set_new(root, "_format", json_string("race_data"));
    json_object_set_new(root, "id", json_string("human"));
    json_object_set_new(root, "uid", json_integer(1));
    json_object_set_new(root, "name", json_string("human"));
    json_object_set_new(root, "playable", json_boolean(true));

    json_object_set_new(physical, "min_size", json_integer(2));
    json_object_set_new(physical, "max_size", json_integer(2));

    json_array_append_new(form, json_string("sentient"));
    json_array_append_new(form, json_string("biped"));
    json_object_set_new(physical, "form", form);

    json_array_append_new(parts, json_string("head"));
    json_array_append_new(parts, json_string("arms"));
    json_array_append_new(parts, json_string("legs"));
    json_array_append_new(parts, json_string("heart"));
    json_array_append_new(parts, json_string("brains"));
    json_array_append_new(parts, json_string("guts"));
    json_array_append_new(parts, json_string("hands"));
    json_array_append_new(parts, json_string("feet"));
    json_array_append_new(parts, json_string("fingers"));
    json_array_append_new(parts, json_string("ear"));
    json_array_append_new(parts, json_string("eye"));
    json_object_set_new(physical, "parts", parts);
    json_object_set_new(root, "physical", physical);

    json_object_set_new(combat, "affects", json_array());
    json_object_set_new(combat, "immunities", json_array());
    json_object_set_new(combat, "resistances", json_array());
    json_object_set_new(root, "combat", combat);

    json_object_set_new(stats, "str", json_integer(13));
    json_object_set_new(stats, "int", json_integer(13));
    json_object_set_new(stats, "wis", json_integer(13));
    json_object_set_new(stats, "dex", json_integer(13));
    json_object_set_new(stats, "con", json_integer(13));
    json_object_set_new(attributes, "stats", stats);

    json_object_set_new(max_vitals, "hp", json_integer(3000));
    json_object_set_new(max_vitals, "mana", json_integer(3000));
    json_object_set_new(max_vitals, "move", json_integer(3000));
    json_object_set_new(attributes, "max_vitals", max_vitals);
    json_object_set_new(root, "attributes", attributes);

    if (json_dump_file(root, "data/races/human.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to write human.json\n");
        json_decref(root);
        return false;
    }

    json_decref(root);
    printf("  Created minimal human.json\n");
    return true;
}

/**
 * create_trait_definitions - Create baseline traits.json when missing
 *
 * Provides a minimal but valid trait definition set so the trait system
 * loads cleanly during fresh bootstrap runs.
 *
 * @return true on success
 */
bool create_trait_definitions(void)
{
    json_t *root;
    json_t *traits;

    if (file_exists("data/traits/traits.json")) {
        return true;
    }

    root = json_object();
    traits = json_array();

    json_object_set_new(root, "_format", json_string("trait_definitions"));
    json_object_set_new(root, "_version", json_integer(1));

    {
        json_t *t = json_object();
        json_object_set_new(t, "id", json_string("sentient"));
        json_object_set_new(t, "name", json_string("Sentient"));
        json_object_set_new(t, "description", json_string("Can use player-facing communication and class systems."));
        json_object_set_new(t, "category", json_string("core"));
        json_object_set_new(t, "type", json_string("boolean"));
        json_object_set_new(t, "default", json_boolean(true));
        json_array_append_new(traits, t);
    }

    {
        json_t *t = json_object();
        json_object_set_new(t, "id", json_string("humanoid"));
        json_object_set_new(t, "name", json_string("Humanoid"));
        json_object_set_new(t, "description", json_string("Uses standard humanoid movement and equipment assumptions."));
        json_object_set_new(t, "category", json_string("body"));
        json_object_set_new(t, "type", json_string("boolean"));
        json_object_set_new(t, "default", json_boolean(true));
        json_array_append_new(traits, t);
    }

    {
        json_t *t = json_object();
        json_object_set_new(t, "id", json_string("size_class"));
        json_object_set_new(t, "name", json_string("Size Class"));
        json_object_set_new(t, "description", json_string("Generic size class hint for scripts and content logic."));
        json_object_set_new(t, "category", json_string("body"));
        json_object_set_new(t, "type", json_string("integer"));
        json_object_set_new(t, "default", json_integer(2));
        json_array_append_new(traits, t);
    }

    json_object_set_new(root, "traits", traits);

    if (json_dump_file(root, "data/traits/traits.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to write data/traits/traits.json\n");
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}

/**
 * generate_default_game_data - Generate JSON gameplay datasets from code defaults
 *
 * This drives generation during bootstrap so a new install has a runnable
 * JSON baseline immediately.
 *
 * @return true on success
 */
bool generate_default_game_data(void)
{
    log_string("bootstrap: generating default gameplay data files");

    load_skill_data();
    load_skill_groups();
    if (!load_songs()) {
        fprintf(stderr, "Failed to generate/load songs data\n");
        return false;
    }
    load_class_data();

    load_liquid_data();
    load_material_data();
    load_sector_data();
    load_corpse_data();

    if (!load_commands()) {
        fprintf(stderr, "Failed to generate/load commands data\n");
        return false;
    }

    return true;
}

/**
 * create_game_settings - Create minimal game_settings.json
 *
 * @return true on success
 */
bool create_game_settings(void)
{
    json_t *root = json_object();
    json_t *core = json_object();
    json_t *email = json_object();
    json_t *crypto = json_object();

    json_object_set_new(root, "_version", json_string("1.0"));
    json_object_set_new(root, "_format", json_string("game_settings"));

    json_object_set_new(core, "env_var_prefix", json_string("SENTIENCE_"));
    json_object_set_new(core, "secrets_mount", json_string("/sentience/data/system/.setting_override"));
    json_object_set_new(root, "core", core);

    json_object_set_new(email, "email_enable", json_boolean(false));
    json_object_set_new(root, "email", email);

    /* Crypto settings - defaults to file-based key storage */
    json_object_set_new(crypto, "use_passphrase", json_boolean(false));
    json_object_set_new(crypto, "passphrase", json_string(""));
    json_object_set_new(crypto, "passphrase_previous", json_string(""));
    json_object_set_new(crypto, "salt_file", json_string(""));
    json_object_set_new(crypto, "key_version", json_integer(1));
    json_object_set_new(root, "crypto", crypto);

    json_object_set_new(root, "dev_server", json_boolean(false));
    json_object_set_new(root, "testport", json_boolean(false));

    if (json_dump_file(root, "data/system/game_settings.json", JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to write game_settings.json\n");
        json_decref(root);
        return false;
    }

    json_decref(root);
    return true;
}

static bool create_reserved_fixture_area(void)
{
    json_error_t error;
    json_t *reserved_root = json_load_file("data/system/reserved.json", 0, &error);
    if (!reserved_root) {
        fprintf(stderr, "Failed to load reserved.json for fixture generation: %s\n", error.text);
        return false;
    }

    json_t *entities = json_object_get(reserved_root, "entities");
    if (!json_is_array(entities)) {
        json_decref(reserved_root);
        fprintf(stderr, "reserved.json missing entities array\n");
        return false;
    }

    json_t *root = json_object();
    json_t *area = json_object();
    json_t *vnums = json_object();
    json_t *metadata = json_object();
    json_t *levels = json_object();
    json_t *rooms = json_array();
    json_t *objects = json_array();
    json_t *mobiles = json_array();

    json_object_set_new(root, "schema_version", json_string("1.0.0"));
    json_object_set_new(area, "uid", json_integer(2));
    json_object_set_new(area, "name", json_string("Bootstrap Reserved Fixture"));
    json_object_set_new(area, "filename", json_string("bootstrap_reserved_fixture.json"));
    json_object_set_new(vnums, "min", json_integer(1));
    json_object_set_new(vnums, "max", json_integer(999));
    json_object_set_new(area, "vnums", vnums);
    json_object_set_new(metadata, "builders", json_string("Bootstrap"));
    json_object_set_new(metadata, "security", json_integer(9));
    json_object_set_new(levels, "min", json_integer(0));
    json_object_set_new(levels, "max", json_integer(120));
    json_object_set_new(metadata, "levels", levels);
    json_object_set_new(area, "metadata", metadata);
    json_object_set_new(root, "area", area);

    json_t *room0 = json_object();
    json_t *room0_flags = json_array();
    json_object_set_new(room0, "vnum", json_integer(1));
    json_object_set_new(room0, "name", json_string("Reserved Fixture Staging"));
    json_object_set_new(room0, "description",
        json_string("Bootstrap-generated staging room for reserved fixture entities.\n\r"));
    json_array_append_new(room0_flags, json_string("indoors"));
    json_array_append_new(room0_flags, json_string("safe"));
    json_object_set_new(room0, "flags", room0_flags);
    json_object_set_new(room0, "sector", json_integer(0));
    json_array_append_new(rooms, room0);

    int obj_vnum = 10;
    int mob_vnum = 400;
    int room_vnum = 700;

    size_t i;
    json_t *entry;
    json_array_foreach(entities, i, entry) {
        json_t *name_json = json_object_get(entry, "name");
        json_t *type_json = json_object_get(entry, "type");
        const char *name = json_is_string(name_json) ? json_string_value(name_json) : "reserved_entity";
        const char *type = json_is_string(type_json) ? json_string_value(type_json) : "obj";

        if (!str_cmp(type, "obj")) {
            json_t *obj = json_object();
            char short_descr[128];
            char long_descr[160];
            snprintf(short_descr, sizeof(short_descr), "%s", name);
            snprintf(long_descr, sizeof(long_descr), "%s lies here.", name);
            json_object_set_new(obj, "vnum", json_integer(obj_vnum++));
            json_object_set_new(obj, "name", json_string(short_descr));
            json_object_set_new(obj, "short_descr", json_string(short_descr));
            json_object_set_new(obj, "long_descr", json_string(long_descr));
            json_object_set_new(obj, "description", json_string("Reserved fixture object."));
            json_object_set_new(obj, "item_type", json_string("trash"));
            json_object_set_new(obj, "level", json_integer(1));
            json_object_set_new(obj, "weight", json_integer(1));
            json_object_set_new(obj, "cost", json_integer(0));
            json_array_append_new(objects, obj);
        } else if (!str_cmp(type, "mob")) {
            json_t *mob = json_object();
            char short_descr[128];
            char long_descr[160];
            snprintf(short_descr, sizeof(short_descr), "%s", name);
            snprintf(long_descr, sizeof(long_descr), "%s stands here.", name);
            json_object_set_new(mob, "vnum", json_integer(mob_vnum++));
            json_object_set_new(mob, "name", json_string(short_descr));
            json_object_set_new(mob, "short_descr", json_string(short_descr));
            json_object_set_new(mob, "long_descr", json_string(long_descr));
            json_object_set_new(mob, "description", json_string("Reserved fixture mobile."));
            json_object_set_new(mob, "level", json_integer(1));
            json_object_set_new(mob, "race", json_string("human"));
            json_array_append_new(mobiles, mob);
        } else if (!str_cmp(type, "room")) {
            json_t *room = json_object();
            json_t *flags = json_array();
            char room_name[128];
            snprintf(room_name, sizeof(room_name), "%s", name);
            json_object_set_new(room, "vnum", json_integer(room_vnum++));
            json_object_set_new(room, "name", json_string(room_name));
            json_object_set_new(room, "description", json_string("Reserved fixture room.\n\r"));
            json_array_append_new(flags, json_string("indoors"));
            json_array_append_new(flags, json_string("safe"));
            json_object_set_new(room, "flags", flags);
            json_object_set_new(room, "sector", json_integer(0));
            json_array_append_new(rooms, room);
        }
    }

    json_object_set_new(root, "rooms", rooms);
    json_object_set_new(root, "objects", objects);
    json_object_set_new(root, "mobiles", mobiles);

    int rc = json_dump_file(root, "area/bootstrap_reserved_fixture.json", JSON_INDENT(2));
    json_decref(root);
    json_decref(reserved_root);
    return rc == 0;
}

static bool create_dummy_fixture_area(void)
{
    json_t *root = json_object();
    json_t *area = json_object();
    json_t *vnums = json_object();
    json_t *metadata = json_object();
    json_t *levels = json_object();
    json_t *rooms = json_array();
    json_t *objects = json_array();
    json_t *mobiles = json_array();

    json_object_set_new(root, "schema_version", json_string("1.0.0"));
    json_object_set_new(area, "uid", json_integer(3));
    json_object_set_new(area, "name", json_string("Bootstrap Dummy Fixture"));
    json_object_set_new(area, "filename", json_string("bootstrap_dummy_fixture.json"));
    json_object_set_new(vnums, "min", json_integer(1));
    json_object_set_new(vnums, "max", json_integer(200));
    json_object_set_new(area, "vnums", vnums);
    json_object_set_new(metadata, "builders", json_string("Bootstrap"));
    json_object_set_new(metadata, "security", json_integer(9));
    json_object_set_new(levels, "min", json_integer(0));
    json_object_set_new(levels, "max", json_integer(120));
    json_object_set_new(metadata, "levels", levels);
    json_object_set_new(area, "metadata", metadata);
    json_object_set_new(root, "area", area);

    json_t *room = json_object();
    json_t *room_flags = json_array();
    json_object_set_new(room, "vnum", json_integer(1));
    json_object_set_new(room, "name", json_string("Dummy Fixture Room"));
    json_object_set_new(room, "description", json_string("Room containing generic dummy entities for tests.\n\r"));
    json_array_append_new(room_flags, json_string("indoors"));
    json_array_append_new(room_flags, json_string("safe"));
    json_object_set_new(room, "flags", room_flags);
    json_object_set_new(room, "sector", json_string("inside"));
    json_array_append_new(rooms, room);

    json_t *mob = json_object();
    json_object_set_new(mob, "vnum", json_integer(10));
    json_object_set_new(mob, "name", json_string("dummy_mobile"));
    json_object_set_new(mob, "short_descr", json_string("a dummy mobile"));
    json_object_set_new(mob, "long_descr", json_string("A dummy mobile stands here."));
    json_object_set_new(mob, "description", json_string("Generic bootstrap dummy mobile."));
    json_object_set_new(mob, "level", json_integer(1));
    json_object_set_new(mob, "race", json_string("human"));
    json_array_append_new(mobiles, mob);

    json_t *obj = json_object();
    json_object_set_new(obj, "vnum", json_integer(20));
    json_object_set_new(obj, "name", json_string("dummy_object"));
    json_object_set_new(obj, "short_descr", json_string("a dummy object"));
    json_object_set_new(obj, "long_descr", json_string("A dummy object lies here."));
    json_object_set_new(obj, "description", json_string("Generic bootstrap dummy object."));
    json_object_set_new(obj, "item_type", json_string("trash"));
    json_object_set_new(obj, "level", json_integer(1));
    json_object_set_new(obj, "weight", json_integer(1));
    json_object_set_new(obj, "cost", json_integer(0));
    json_array_append_new(objects, obj);

    json_object_set_new(root, "rooms", rooms);
    json_object_set_new(root, "objects", objects);
    json_object_set_new(root, "mobiles", mobiles);

    int rc = json_dump_file(root, "area/bootstrap_dummy_fixture.json", JSON_INDENT(2));
    json_decref(root);
    return rc == 0;
}

bool create_ci_test_fixture_areas(void)
{
    if (!file_exists("data/system/reserved.json")) {
        fprintf(stderr, "reserved.json is required before generating CI fixture areas\n");
        return false;
    }

    if (!create_reserved_fixture_area()) {
        return false;
    }

    if (!create_dummy_fixture_area()) {
        return false;
    }

    if (!ensure_area_in_area_list("bootstrap_reserved_fixture.json")) {
        return false;
    }
    if (!ensure_area_in_area_list("bootstrap_dummy_fixture.json")) {
        return false;
    }

    return true;
}

bool create_ci_test_data_files(void)
{
    const char *source_candidates[] = {
        "src/tests/data",
        "/sentience/src/tests/data",
        NULL
    };

    const char *source_dir = NULL;
    for (int i = 0; source_candidates[i] != NULL; i++) {
        if (directory_exists_local(source_candidates[i])) {
            source_dir = source_candidates[i];
            break;
        }
    }

    if (!source_dir) {
        fprintf(stderr, "No source test data directory found for CI fixture copy\n");
        return false;
    }

    if (!copy_directory_recursive(source_dir, "data/tests")) {
        fprintf(stderr, "Failed to copy CI test data from %s to data/tests (%s)\n",
                source_dir, strerror(errno));
        return false;
    }

    return true;
}
