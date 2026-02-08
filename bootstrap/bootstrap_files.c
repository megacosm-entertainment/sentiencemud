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
#include <jansson.h>
#include "../merc.h"
#include "bootstrap_internal.h"

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
        "data/help", "data/notes", "data/orgs", "data/persist",
        "data/stats", "data/traits", "data/dump", "data/tests",
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
    fprintf(fp, "NextAreaUID 2\n");
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
    json_object_set_new(room, "sector", json_integer(0));

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
