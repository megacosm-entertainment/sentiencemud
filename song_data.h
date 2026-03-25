/***************************************************************************
 *  Song Data System - Public API                                          *
 *                                                                         *
 *  Data-driven song definitions loaded from JSON files.                   *
 ***************************************************************************/

#ifndef SONG_DATA_H
#define SONG_DATA_H

/* Forward declarations — full struct is in merc.h */
typedef struct song_data SONG_DATA;

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/



/* Song flags (bitfield, reserved for future expansion) */
#define SONG_NONE               0
/* TODO: Add flags for instrument requirements, class restrictions, etc. */

/***************************************************************************
 * Lookup Functions                                                        *
 ***************************************************************************/

/**
 * song_lookup - Find a song by name prefix match
 *
 * Searches the loaded song list for a song whose name matches
 * the given prefix (case-insensitive).
 *
 * @param name  Song name or prefix to search for
 * @return      Pointer to SONG_DATA, or NULL if not found
 */
SONG_DATA *song_lookup(const char *name);

/**
 * song_lookup_uid - Find a song by unique ID
 *
 * @param uid   The song's unique identifier
 * @return      Pointer to SONG_DATA, or NULL if not found
 */
SONG_DATA *song_lookup_uid(int uid);

/**
 * song_count - Get the number of loaded songs
 *
 * @return  Number of songs currently loaded
 */
int song_count(void);

/**
 * song_get_list - Get the internal songs list for iteration
 *
 * Returns the LLIST of all loaded SONG_DATA entries.
 * Caller must use ITERATOR/iterator_start() to traverse.
 *
 * @return  LLIST pointer (may be NULL if songs not loaded)
 */
LLIST *song_get_list(void);

/***************************************************************************
 * Lifecycle Functions                                                     *
 ***************************************************************************/

/**
 * load_songs - Load song definitions from JSON
 *
 * Loads songs from SONGS_FILE. If the file does not exist, logs an error
 * (music_table[] was removed in Phase 9).
 *
 * Called during boot sequence in db.c.
 *
 * @return  true on success, false on error
 */
bool load_songs(void);

/**
 * save_songs - Save all song definitions to JSON
 *
 * Writes the current song list to SONGS_FILE.
 *
 * @return  true on success, false on error
 */
bool save_songs(void);

/**
 * bootstrap_songs - Stub (music_table[] removed in Phase 9)
 *
 * Previously generated SONG_DATA entries from the static music_table[].
 * Now logs an error since the table no longer exists.
 *
 * @return  false always
 */
bool bootstrap_songs(void);

/**
 * cleanup_songs - Free all song data
 *
 * Called during shutdown to free the songs list.
 */
void cleanup_songs(void);

#endif /* SONG_DATA_H */
