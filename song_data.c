/***************************************************************************
 *  Song Data System - Implementation                                      *
 *                                                                         *
 *  Data-driven song definitions loaded from JSON files.                   *
 *  Replaces the legacy static music_table[] array in const.c.             *
 *                                                                         *
 *  TODO (future expansion):                                               *
 *  - Add token/script references (presong_fun, song_fun) for             *
 *    fully scriptable song effects instead of spell1/2/3 strings.         *
 *  - Add instrument type requirements.                                    *
 *  - Add class/race restrictions.                                         *
 ***************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "recycle.h"
#include "song_data.h"
#include <jansson.h>

/***************************************************************************
 * Module State                                                            *
 ***************************************************************************/

static LLIST *songs_list = NULL;    /* LLIST of SONG_DATA * */
static int top_song_uid = 0;

/***************************************************************************
 * Internal: Target Name Mapping                                           *
 ***************************************************************************/

static const char *target_to_string(int target)
{
    switch (target) {
    case TAR_IGNORE:            return "ignore";
    case TAR_CHAR_OFFENSIVE:    return "offensive";
    case TAR_CHAR_DEFENSIVE:    return "defensive";
    case TAR_CHAR_SELF:         return "self";
    case TAR_OBJ_CHAR_DEF:     return "obj_char_def";
    case TAR_OBJ_CHAR_OFF:     return "obj_char_off";
    case TAR_CHAR_FORMATION:    return "formation";
    default:                    return "ignore";
    }
}

static int target_from_string(const char *str)
{
    if (!str) return TAR_IGNORE;
    if (!str_cmp(str, "ignore"))        return TAR_IGNORE;
    if (!str_cmp(str, "offensive"))     return TAR_CHAR_OFFENSIVE;
    if (!str_cmp(str, "defensive"))     return TAR_CHAR_DEFENSIVE;
    if (!str_cmp(str, "self"))          return TAR_CHAR_SELF;
    if (!str_cmp(str, "obj_char_def"))  return TAR_OBJ_CHAR_DEF;
    if (!str_cmp(str, "obj_char_off"))  return TAR_OBJ_CHAR_OFF;
    if (!str_cmp(str, "formation"))     return TAR_CHAR_FORMATION;
    return TAR_IGNORE;
}

/***************************************************************************
 * Internal: Allocation                                                    *
 ***************************************************************************/

/**
 * new_song_data - Allocate and initialize a new SONG_DATA
 *
 * Uses alloc_perm for persistent allocation consistent with
 * the project's memory model.
 *
 * @return  Pointer to zeroed SONG_DATA
 */
static SONG_DATA *new_song_data(void)
{
    SONG_DATA *song = (SONG_DATA *)alloc_perm(sizeof(SONG_DATA));

    song->name   = NULL;
    song->uid    = 0;
    song->level  = 0;
    song->mana   = 0;
    song->target = TAR_IGNORE;
    song->beats  = 0;
    song->spell1 = NULL;
    song->spell2 = NULL;
    song->spell3 = NULL;
    song->flags  = SONG_NONE;

    return song;
}

/***************************************************************************
 * Lookup Functions                                                        *
 ***************************************************************************/

SONG_DATA *song_lookup(const char *name)
{
    ITERATOR it;
    SONG_DATA *song;

    if (!name || !name[0] || !songs_list)
        return NULL;

    iterator_start(&it, songs_list);
    while ((song = (SONG_DATA *)iterator_nextdata(&it))) {
        if (!str_prefix(name, song->name))
            break;
    }
    iterator_stop(&it);

    return song;
}

SONG_DATA *song_lookup_uid(int uid)
{
    ITERATOR it;
    SONG_DATA *song;

    if (uid < 0 || !songs_list)
        return NULL;

    iterator_start(&it, songs_list);
    while ((song = (SONG_DATA *)iterator_nextdata(&it))) {
        if (song->uid == uid)
            break;
    }
    iterator_stop(&it);

    return song;
}

int song_count(void)
{
    return songs_list ? list_size(songs_list) : 0;
}

LLIST *song_get_list(void)
{
    return songs_list;
}

/***************************************************************************
 * Internal: Insert in sorted order                                        *
 ***************************************************************************/

/**
 * insert_song - Insert a song into songs_list in alphabetical order
 *
 * @param song  The SONG_DATA to insert
 */
static void insert_song(SONG_DATA *song)
{
    ITERATOR it;
    SONG_DATA *sg;

    iterator_start(&it, songs_list);
    while ((sg = (SONG_DATA *)iterator_nextdata(&it))) {
        if (str_cmp(song->name, sg->name) < 0) {
            iterator_insert_before(&it, song);
            iterator_stop(&it);
            return;
        }
    }
    iterator_stop(&it);

    list_appendlink(songs_list, song);
}

/***************************************************************************
 * Bootstrap: Generate from legacy music_table[]                           *
 ***************************************************************************/

bool bootstrap_songs(void)
{
    int i;

    log_string("bootstrap_songs: generating SONG_DATA from music_table[]");

    if (!songs_list)
        songs_list = list_create(false);

    for (i = 0; i < MAX_SONGS && music_table[i].name; i++) {
        SONG_DATA *song = new_song_data();

        song->name   = str_dup(music_table[i].name);
        song->uid    = i;
        song->level  = music_table[i].level;
        song->mana   = music_table[i].mana;
        song->target = music_table[i].target;
        song->beats  = music_table[i].beats;
        song->spell1 = music_table[i].spell1 ? str_dup(music_table[i].spell1) : NULL;
        song->spell2 = music_table[i].spell2 ? str_dup(music_table[i].spell2) : NULL;
        song->spell3 = music_table[i].spell3 ? str_dup(music_table[i].spell3) : NULL;
        song->flags  = SONG_NONE;

        insert_song(song);

        if (song->uid > top_song_uid)
            top_song_uid = song->uid;
    }

    log_stringf("bootstrap_songs: created %d songs (top_uid=%d)", i, top_song_uid);
    return true;
}

/***************************************************************************
 * JSON Load                                                               *
 ***************************************************************************/

/**
 * load_song_from_json - Parse a single SONG_DATA from a JSON object
 *
 * @param obj   JSON object representing one song
 * @return      New SONG_DATA, or NULL on error
 */
static SONG_DATA *load_song_from_json(json_t *obj)
{
    SONG_DATA *song;
    const char *str;
    json_t *val;

    if (!json_is_object(obj))
        return NULL;

    str = json_string_value(json_object_get(obj, "name"));
    if (!str || !str[0]) {
        log_string("load_song_from_json: song missing 'name' field");
        return NULL;
    }

    song = new_song_data();
    song->name = str_dup(str);

    val = json_object_get(obj, "uid");
    song->uid = val ? (int)json_integer_value(val) : 0;

    val = json_object_get(obj, "level");
    song->level = val ? (int)json_integer_value(val) : 0;

    val = json_object_get(obj, "mana");
    song->mana = val ? (int16_t)json_integer_value(val) : 0;

    val = json_object_get(obj, "beats");
    song->beats = val ? (int16_t)json_integer_value(val) : 0;

    str = json_string_value(json_object_get(obj, "target"));
    song->target = target_from_string(str);

    str = json_string_value(json_object_get(obj, "spell1"));
    song->spell1 = (str && str[0]) ? str_dup(str) : NULL;

    str = json_string_value(json_object_get(obj, "spell2"));
    song->spell2 = (str && str[0]) ? str_dup(str) : NULL;

    str = json_string_value(json_object_get(obj, "spell3"));
    song->spell3 = (str && str[0]) ? str_dup(str) : NULL;

    val = json_object_get(obj, "flags");
    song->flags = val ? (long)json_integer_value(val) : SONG_NONE;

    return song;
}

bool load_songs(void)
{
    json_t *root, *arr;
    json_error_t error;
    size_t i;

    top_song_uid = 0;

    if (songs_list) {
        list_destroy(songs_list);
        songs_list = NULL;
    }

    songs_list = list_create(false);

    /* Try to load from JSON file */
    root = json_load_file(SONGS_FILE, 0, &error);
    if (!root) {
        /* No file exists — bootstrap from music_table[] */
        log_stringf("load_songs: %s not found, bootstrapping from music_table[]", SONGS_FILE);

        if (!bootstrap_songs())
            return false;

        /* Save the bootstrapped data for next boot */
        save_songs();
        return true;
    }

    /* Validate format */
    const char *fmt = json_string_value(json_object_get(root, "_format"));
    if (!fmt || str_cmp(fmt, "song_data")) {
        log_stringf("load_songs: invalid format '%s' in %s", fmt ? fmt : "(null)", SONGS_FILE);
        json_decref(root);
        return false;
    }

    /* Load songs array */
    arr = json_object_get(root, "songs");
    if (!json_is_array(arr)) {
        log_string("load_songs: missing 'songs' array");
        json_decref(root);
        return false;
    }

    for (i = 0; i < json_array_size(arr); i++) {
        SONG_DATA *song = load_song_from_json(json_array_get(arr, i));
        if (song) {
            insert_song(song);
            if (song->uid > top_song_uid)
                top_song_uid = song->uid;
        }
    }

    json_decref(root);

    log_stringf("load_songs: loaded %d songs from %s (top_uid=%d)",
                song_count(), SONGS_FILE, top_song_uid);
    return true;
}

/***************************************************************************
 * JSON Save                                                               *
 ***************************************************************************/

/**
 * song_to_json - Serialize a single SONG_DATA to a JSON object
 *
 * @param song  The song to serialize
 * @return      New json_t object (caller must decref), or NULL on error
 */
static json_t *song_to_json(SONG_DATA *song)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "name", json_string(song->name));
    json_object_set_new(obj, "uid", json_integer(song->uid));
    json_object_set_new(obj, "level", json_integer(song->level));
    json_object_set_new(obj, "mana", json_integer(song->mana));
    json_object_set_new(obj, "beats", json_integer(song->beats));
    json_object_set_new(obj, "target", json_string(target_to_string(song->target)));

    if (song->spell1)
        json_object_set_new(obj, "spell1", json_string(song->spell1));
    if (song->spell2)
        json_object_set_new(obj, "spell2", json_string(song->spell2));
    if (song->spell3)
        json_object_set_new(obj, "spell3", json_string(song->spell3));

    if (song->flags != SONG_NONE)
        json_object_set_new(obj, "flags", json_integer(song->flags));

    return obj;
}

bool save_songs(void)
{
    json_t *root, *arr;
    ITERATOR it;
    SONG_DATA *song;

    if (!songs_list) {
        log_string("save_songs: no songs_list to save");
        return false;
    }

    /* Ensure directory exists */
    mkdir(SONGS_DIR, 0755);

    root = json_object();
    json_object_set_new(root, "_format", json_string("song_data"));
    json_object_set_new(root, "_version", json_integer(1));

    arr = json_array();
    iterator_start(&it, songs_list);
    while ((song = (SONG_DATA *)iterator_nextdata(&it))) {
        json_t *sobj = song_to_json(song);
        if (sobj)
            json_array_append_new(arr, sobj);
    }
    iterator_stop(&it);

    json_object_set_new(root, "songs", arr);

    if (json_dump_file(root, SONGS_FILE, JSON_INDENT(4) | JSON_SORT_KEYS) != 0) {
        log_stringf("save_songs: failed to write %s", SONGS_FILE);
        json_decref(root);
        return false;
    }

    json_decref(root);
    log_stringf("save_songs: saved %d songs to %s", song_count(), SONGS_FILE);
    return true;
}

/***************************************************************************
 * Cleanup                                                                 *
 ***************************************************************************/

void cleanup_songs(void)
{
    if (songs_list) {
        /* Note: alloc_perm memory is not actually freed, but we clean up
         * the list structure and NULLify the pointer for safety. */
        list_destroy(songs_list);
        songs_list = NULL;
    }
    top_song_uid = 0;
}
