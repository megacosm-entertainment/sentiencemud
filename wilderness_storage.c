#include <errno.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_storage.h"
#include "wilderness_state.h"
#include "wilderness_mods.h"
#include "wilderness_wmap.h"
#include "wilderness_wterr.h"
#include "wilderness_vlinks.h"

#define WILDERNESS_STORAGE_ROOT WORLD_DIR "wilderness_state"
#define WILDERNESS_IMAGES_DIR_NAME "images"
#define WILDERNESS_STATE_FILENAME "wilderness_state.json"
#define WILDERNESS_MODS_FILENAME "wilderness_mods.json"
#define WILDERNESS_WMAP_FILENAME "wilderness.wmap"
#define WILDERNESS_WTERR_FILENAME "wilderness.wterr.json"
#define WILDERNESS_VLINKS_FILENAME "wilderness_vlinks.json"

static bool wilderness_storage_ready = false;
static time_t wilderness_storage_last_autosave = 0;

static bool wilderness_storage_ensure_directory(const char *path);

static void wilderness_storage_slugify_name(const char *name, char *out, size_t out_size)
{
    size_t i = 0;
    bool last_was_sep = false;

    if (!out || out_size < 2)
        return;

    if (IS_NULLSTR(name))
        name = "wilds";

    while (*name && i < out_size - 1)
    {
        unsigned char ch = (unsigned char)*name;

        if (isalnum(ch))
        {
            out[i++] = (char)tolower(ch);
            last_was_sep = false;
        }
        else if (!last_was_sep)
        {
            out[i++] = '_';
            last_was_sep = true;
        }

        name++;
    }

    while (i > 0 && out[i - 1] == '_')
        i--;

    if (i == 0)
    {
        snprintf(out, out_size, "wilds");
        return;
    }

    out[i] = '\0';
}

static bool wilderness_storage_build_wilds_dir_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char slug[MIL];

    if (!pWilds || !out || out_size < 2)
        return false;

    wilderness_storage_slugify_name(pWilds->name, slug, sizeof(slug));
    return snprintf(out, out_size, "%s/%ld_%s", WILDERNESS_STORAGE_ROOT, pWilds->uid, slug) < (int)out_size;
}

static bool wilderness_storage_ensure_wilds_paths(const WILDS_DATA *pWilds)
{
    char wilds_dir[MSL];
    char images_dir[MSL];

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    if (!wilderness_storage_ensure_directory(wilds_dir))
        return false;

    if (snprintf(images_dir, sizeof(images_dir), "%s/%s", wilds_dir, WILDERNESS_IMAGES_DIR_NAME) >= (int)sizeof(images_dir))
        return false;

    return wilderness_storage_ensure_directory(images_dir);
}

static bool wilderness_storage_build_legacy_state_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    if (!pWilds || !out || out_size < 2)
        return false;

    return snprintf(out, out_size, "%s/wilderness_%ld_state.json", WILDERNESS_STORAGE_ROOT, pWilds->uid) < (int)out_size;
}

static bool wilderness_storage_build_legacy_mods_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    if (!pWilds || !out || out_size < 2)
        return false;

    return snprintf(out, out_size, "%s/wilderness_%ld_mods.json", WILDERNESS_STORAGE_ROOT, pWilds->uid) < (int)out_size;
}

static bool wilderness_storage_build_legacy_wmap_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    if (!pWilds || !out || out_size < 2)
        return false;

    return snprintf(out, out_size, "%s/wilderness_%ld.wmap", WILDERNESS_STORAGE_ROOT, pWilds->uid) < (int)out_size;
}

static bool wilderness_storage_build_legacy_wterr_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    if (!pWilds || !out || out_size < 2)
        return false;

    return snprintf(out, out_size, "%s/wilderness_%ld.wterr.json", WILDERNESS_STORAGE_ROOT, pWilds->uid) < (int)out_size;
}

static bool wilderness_storage_build_legacy_vlinks_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    if (!pWilds || !out || out_size < 2)
        return false;

    return snprintf(out, out_size, "%s/wilderness_%ld_vlinks.json", WILDERNESS_STORAGE_ROOT, pWilds->uid) < (int)out_size;
}

static void wilderness_storage_migrate_file(const char *legacy_path, const char *new_path, const char *label, long uid)
{
    struct stat st;

    if (IS_NULLSTR(legacy_path) || IS_NULLSTR(new_path) || IS_NULLSTR(label))
        return;

    if (stat(legacy_path, &st) != 0 || !S_ISREG(st.st_mode))
        return;

    if (stat(new_path, &st) == 0 && S_ISREG(st.st_mode))
        return;

    if (rename(legacy_path, new_path) == 0)
    {
        plogf(LOG_INFO,
            "wilderness_storage: migrated legacy %s for wilds uid %ld (%s -> %s)",
            label,
            uid,
            legacy_path,
            new_path);
    }
    else
    {
        perrf(LOG_WARN,
            "wilderness_storage: failed migrating legacy %s for wilds uid %ld (%s -> %s)",
            label,
            uid,
            legacy_path,
            new_path);
    }
}

static void wilderness_storage_migrate_legacy_files(const WILDS_DATA *pWilds)
{
    char legacy_state[MSL];
    char legacy_mods[MSL];
    char legacy_wmap[MSL];
    char legacy_wterr[MSL];
    char legacy_vlinks[MSL];
    char new_state[MSL];
    char new_mods[MSL];
    char new_wmap[MSL];
    char new_wterr[MSL];
    char new_vlinks[MSL];

    if (!pWilds)
        return;

    if (!wilderness_storage_build_legacy_state_path(pWilds, legacy_state, sizeof(legacy_state))
    || !wilderness_storage_build_legacy_mods_path(pWilds, legacy_mods, sizeof(legacy_mods))
    || !wilderness_storage_build_legacy_wmap_path(pWilds, legacy_wmap, sizeof(legacy_wmap))
    || !wilderness_storage_build_legacy_wterr_path(pWilds, legacy_wterr, sizeof(legacy_wterr))
    || !wilderness_storage_build_legacy_vlinks_path(pWilds, legacy_vlinks, sizeof(legacy_vlinks))
    || !wilderness_storage_build_state_path(pWilds, new_state, sizeof(new_state))
    || !wilderness_storage_build_mods_path(pWilds, new_mods, sizeof(new_mods))
    || !wilderness_storage_build_wmap_path(pWilds, new_wmap, sizeof(new_wmap))
    || !wilderness_storage_build_wterr_path(pWilds, new_wterr, sizeof(new_wterr))
    || !wilderness_storage_build_vlinks_path(pWilds, new_vlinks, sizeof(new_vlinks)))
        return;

    wilderness_storage_migrate_file(legacy_state, new_state, "state", pWilds->uid);
    wilderness_storage_migrate_file(legacy_mods, new_mods, "mods", pWilds->uid);
    wilderness_storage_migrate_file(legacy_wmap, new_wmap, "wmap", pWilds->uid);
    wilderness_storage_migrate_file(legacy_wterr, new_wterr, "wterr", pWilds->uid);
    wilderness_storage_migrate_file(legacy_vlinks, new_vlinks, "vlinks", pWilds->uid);
}

static void wilderness_storage_save_wilds(WILDS_DATA *pWilds)
{
    if (!pWilds)
        return;

    if (!wilderness_storage_ensure_wilds_paths(pWilds))
    {
        perrf(LOG_ERROR, "wilderness_storage: failed to ensure wilds storage path for uid %ld", pWilds->uid);
        return;
    }

    wilderness_wmap_save(pWilds);
    wilderness_wterr_save(pWilds);
    wilderness_mods_save(pWilds);
    wilderness_vlinks_save(pWilds);
    wilderness_state_save(pWilds);
}

static void wilderness_storage_load_all(void)
{
    ITERATOR iter;
    LLIST_WILDS_DATA *data;
    int loaded = 0;

    if (!loaded_wilds)
        return;

    iterator_start(&iter, loaded_wilds);
    while ((data = (LLIST_WILDS_DATA *)iterator_nextdata(&iter)))
    {
        if (!data || !data->wilds)
            continue;

        if (!wilderness_storage_ensure_wilds_paths(data->wilds))
        {
            perrf(LOG_ERROR, "wilderness_storage: failed to ensure wilds storage path for uid %ld", data->wilds->uid);
            continue;
        }

        wilderness_storage_migrate_legacy_files(data->wilds);

        wilderness_wmap_load(data->wilds);
        wilderness_wterr_load(data->wilds);
        wilderness_mods_load(data->wilds);
        wilderness_vlinks_load(data->wilds);
        wilderness_state_load(data->wilds);

        wilderness_wmap_save(data->wilds);
        wilderness_wterr_save(data->wilds);
        wilderness_mods_save(data->wilds);
        wilderness_vlinks_save(data->wilds);
        loaded++;
    }
    iterator_stop(&iter);

    plogf(LOG_DEBUG, "wilderness_storage: initialized load pass for %d wilderness instance(s)", loaded);
}

static void wilderness_storage_save_all(void)
{
    ITERATOR iter;
    LLIST_WILDS_DATA *data;
    int saved = 0;

    if (!loaded_wilds)
        return;

    iterator_start(&iter, loaded_wilds);
    while ((data = (LLIST_WILDS_DATA *)iterator_nextdata(&iter)))
    {
        if (!data || !data->wilds)
            continue;

        wilderness_storage_save_wilds(data->wilds);
        saved++;
    }
    iterator_stop(&iter);

    plogf(LOG_DEBUG, "wilderness_storage: save pass completed for %d wilderness instance(s)", saved);
}

static bool wilderness_storage_ensure_directory(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);

    if (mkdir(path, 0775) == 0)
        return true;

    return errno == EEXIST;
}

bool wilderness_storage_init(void)
{
    if (wilderness_storage_ready)
        return true;

    if (!wilderness_storage_ensure_directory(WILDERNESS_STORAGE_ROOT))
    {
        perrf(LOG_ERROR, "wilderness_storage_init: failed to ensure storage root '%s'", WILDERNESS_STORAGE_ROOT);
        return false;
    }

    if (!wilderness_state_init())
    {
        perrf(LOG_ERROR, "wilderness_storage_init: wilderness state init failed");
        return false;
    }

    if (!wilderness_mods_init())
    {
        perrf(LOG_ERROR, "wilderness_storage_init: wilderness mods init failed");
        wilderness_state_shutdown();
        return false;
    }

    if (!wilderness_wmap_init())
    {
        perrf(LOG_ERROR, "wilderness_storage_init: wilderness wmap init failed");
        wilderness_mods_shutdown();
        wilderness_state_shutdown();
        return false;
    }

    if (!wilderness_wterr_init())
    {
        perrf(LOG_ERROR, "wilderness_storage_init: wilderness wterr init failed");
        wilderness_wmap_shutdown();
        wilderness_mods_shutdown();
        wilderness_state_shutdown();
        return false;
    }

    if (!wilderness_vlinks_init())
    {
        perrf(LOG_ERROR, "wilderness_storage_init: wilderness vlinks init failed");
        wilderness_wterr_shutdown();
        wilderness_wmap_shutdown();
        wilderness_mods_shutdown();
        wilderness_state_shutdown();
        return false;
    }

    wilderness_storage_ready = true;
    wilderness_storage_last_autosave = current_time;
    wilderness_storage_load_all();
    return true;
}

void wilderness_storage_shutdown(void)
{
    if (!wilderness_storage_ready)
        return;

    wilderness_storage_save_all();

    wilderness_vlinks_shutdown();
    wilderness_wterr_shutdown();
    wilderness_wmap_shutdown();
    wilderness_mods_shutdown();
    wilderness_state_shutdown();
    wilderness_storage_ready = false;
}

void wilderness_storage_pulse(void)
{
    if (!wilderness_storage_ready)
        return;

    wilderness_state_pulse();
    wilderness_mods_pulse();

    if (wilderness_storage_last_autosave <= 0 || (current_time - wilderness_storage_last_autosave) >= 60)
    {
        wilderness_storage_save_all();
        wilderness_storage_last_autosave = current_time;
    }
}

void wilderness_storage_save_all_now(void)
{
    if (!wilderness_storage_ready)
        return;

    wilderness_storage_save_all();
    wilderness_storage_last_autosave = current_time;
}

void wilderness_storage_save_area_now(AREA_DATA *pArea)
{
    WILDS_DATA *pWilds;

    if (!wilderness_storage_ready || !pArea)
        return;

    for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
        wilderness_storage_save_wilds(pWilds);

    wilderness_storage_last_autosave = current_time;
}

void wilderness_storage_checksum_hex(const void *data, size_t len, char out_hex[65])
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    size_t i;

    if (!out_hex)
        return;

    if (!data || len == 0)
    {
        out_hex[0] = '\0';
        return;
    }

    SHA256((const unsigned char *)data, len, digest);

    for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out_hex + (i * 2), "%02x", digest[i]);

    out_hex[64] = '\0';
}

bool wilderness_storage_build_state_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char wilds_dir[MSL];

    if (!pWilds || !out || out_size < 2)
        return false;

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    return snprintf(out, out_size, "%s/%s", wilds_dir, WILDERNESS_STATE_FILENAME) < (int)out_size;
}

bool wilderness_storage_build_mods_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char wilds_dir[MSL];

    if (!pWilds || !out || out_size < 2)
        return false;

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    return snprintf(out, out_size, "%s/%s", wilds_dir, WILDERNESS_MODS_FILENAME) < (int)out_size;
}

bool wilderness_storage_build_wmap_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char wilds_dir[MSL];

    if (!pWilds || !out || out_size < 2)
        return false;

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    return snprintf(out, out_size, "%s/%s", wilds_dir, WILDERNESS_WMAP_FILENAME) < (int)out_size;
}

bool wilderness_storage_build_wterr_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char wilds_dir[MSL];

    if (!pWilds || !out || out_size < 2)
        return false;

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    return snprintf(out, out_size, "%s/%s", wilds_dir, WILDERNESS_WTERR_FILENAME) < (int)out_size;
}

bool wilderness_storage_build_vlinks_path(const WILDS_DATA *pWilds, char *out, size_t out_size)
{
    char wilds_dir[MSL];

    if (!pWilds || !out || out_size < 2)
        return false;

    if (!wilderness_storage_build_wilds_dir_path(pWilds, wilds_dir, sizeof(wilds_dir)))
        return false;

    return snprintf(out, out_size, "%s/%s", wilds_dir, WILDERNESS_VLINKS_FILENAME) < (int)out_size;
}
