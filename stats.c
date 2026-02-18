/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*
 * Leaderboard system - Redis sorted sets with in-memory cache.
 * Replaces the old shell-script-generated .info file approach.
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <jansson.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "interp.h"
#include "io/cache/redis_cache.h"


/* Global leaderboard data */
LEADERBOARD_DATA leaderboards[MAX_LEADERBOARDS];
time_t leaderboard_refresh_time;
time_t leaderboard_backup_time;

/* Static metadata for each leaderboard, matching the old .info file content */
static const struct {
    int         type;
    const char *redis_key;
    const char *report_name;
    const char *description;
    const char *col_name;
    const char *col_value;
    bool        descending;
    bool        is_derived;
} lb_init_table[] = {
    {
        REPORT_TOP_PLAYER_KILLERS, "pkers",
        "{YTop 10 {WPlayer Killers {Yof Sentience{x",
        "Player v Player combat can occur in the arena, in specially marked rooms, "
        "or if players carry the {R[PK]{x flag. These players are truly skilled fighters "
        "who are adept at destroying all those who challenge them. These players have "
        "valiantly fought their way into our Top 10 Player Killers of Sentience!",
        "Player Name", "Player Kills",
        true, false
    },
    {
        REPORT_TOP_CPLAYER_KILLERS, "cpkers",
        "{YTop 10 {WChaotic Player Killers {Yof Sentience{x",
        "Chaotic player killing is dangerous and risky. If a player is killed in chaotic "
        "combat, their equipment does not vanish to the netherworld but remains to be "
        "plundered by the victor. For this reason chaotic player killers are both feared "
        "and revered for their legendary combat prowess.",
        "Player Name", "Chaotic Player Kills",
        true, false
    },
    {
        REPORT_TOP_WEALTHIEST, "wealthiest",
        "{YTop 10 {WWealthiest {YPlayers of Sentience{x",
        "There are many ways to make a shiny silver coin in Sentience, however some "
        "players are just plain cunning. These players have the gift and are proudly "
        "listed as the top 10 wealthiest players of Sentience!",
        "Player Name", "Bank Balance (gold)",
        true, false
    },
    {
        REPORT_TOP_WORST_RATIO, "ratio",
        "{YTop 10 {WWorst Player Killers {Yof Sentience{x",
        "These players may think of themselves as cold blooded killing machines but "
        "their kill ratios speak differently! These players have the worst win / loss "
        "ratio of all time. To make it into this chart, a player must have won/lost "
        "50 player vs player combat battles.",
        "Player Name", "% of victorious battles",
        false, true
    },
    {
        REPORT_TOP_MONSTER_KILLERS, "monsters",
        "{YTop 10 {WMonster Killers {Yof Sentience{x",
        "With every level, creatures are mindlessly slain. Ever stopped to think who "
        "has slain the most creatures? Here are the top monster murderers of Sentience!",
        "Player Name", "Monsters Killed",
        true, false
    },
    {
        REPORT_TOP_QUESTS, "quests",
        "{YTop 10 {WAdventurers {Yof Sentience{x",
        "Quests are found everywhere in Sentience, however special quests are granted "
        "by quest masters and offer quest points to purchase special items. Quests "
        "involve a variety of adventuring, slaying and locating items. Questing offers "
        "a way of improving a players character. The following ten players have completed "
        "the most quests, and are therefore the Top 10 Adventurers of Sentience!",
        "Player Name", "Quests Completed",
        true, false
    },
    {
        REPORT_TOP_BEST_RATIO, "bestratio",
        "{YTop 10 {WBest Player Killers {Yof Sentience{x",
        "These players are the ultimate warriors, combining skill, cunning, and "
        "ruthless efficiency. They boast the best win/loss ratios in player combat. "
        "To make it into this chart, a player must have won/lost 50 player vs player "
        "combat battles.",
        "Player Name", "% of victorious battles",
        true, true
    },
    {
        REPORT_TOP_DEATHS, "deaths",
        "{YTop 10 {WDeaths {Yof Sentience{x",
        "No matter how hard they try, some players just keep ending up in the "
        "Netherworld. These are the players of Sentience who have died the most.",
        "Player Name", "Deaths",
        true, false
    },
};

/**
 * format_display_value - Format a score for display based on board type
 *
 * @param lb     Leaderboard data
 * @param score  Raw numeric score
 * @param buf    Output buffer (at least 64 bytes)
 */
static void format_display_value(LEADERBOARD_DATA *lb, double score, char *buf)
{
    if (lb->is_derived) {
        sprintf(buf, "%.2f%%", score * 100.0);
    } else if (score == (double)(long)score) {
        sprintf(buf, "%ld", (long)score);
    } else {
        sprintf(buf, "%.0f", score);
    }
}

/**
 * leaderboard_update_inmemory - Insert or update an entry in the in-memory sorted array
 *
 * Maintains a sorted array of up to MAX_LEADERBOARD_ENTRIES entries.
 * If the player is already present, updates their score. Otherwise inserts
 * if the score qualifies for the top N.
 *
 * @param lb         Leaderboard to update
 * @param name       Player name
 * @param score      New score value
 */
static void leaderboard_update_inmemory(LEADERBOARD_DATA *lb, const char *name, double score)
{
    int i;
    int existing = -1;

    /* Check if player already exists in the list */
    for (i = 0; i < lb->count; i++) {
        if (!str_cmp(lb->entries[i].name, name)) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        lb->entries[existing].score = score;
        format_display_value(lb, score, lb->entries[existing].display_value);
    } else {
        /* Check if score qualifies for the list */
        if (lb->count >= MAX_LEADERBOARD_ENTRIES) {
            int last = lb->count - 1;
            bool qualifies;

            if (lb->descending)
                qualifies = (score > lb->entries[last].score);
            else
                qualifies = (score < lb->entries[last].score);

            if (!qualifies)
                return;

            /* Replace the last entry */
            existing = last;
            strncpy(lb->entries[existing].name, name, MAX_INPUT_LENGTH - 1);
            lb->entries[existing].name[MAX_INPUT_LENGTH - 1] = '\0';
            lb->entries[existing].score = score;
            format_display_value(lb, score, lb->entries[existing].display_value);
        } else {
            /* Add new entry at the end */
            existing = lb->count;
            strncpy(lb->entries[existing].name, name, MAX_INPUT_LENGTH - 1);
            lb->entries[existing].name[MAX_INPUT_LENGTH - 1] = '\0';
            lb->entries[existing].score = score;
            format_display_value(lb, score, lb->entries[existing].display_value);
            lb->count++;
        }
    }

    /* Bubble sort to maintain order (only 10 entries max) */
    for (i = 0; i < lb->count - 1; i++) {
        int j;
        for (j = 0; j < lb->count - i - 1; j++) {
            bool swap;

            if (lb->descending)
                swap = (lb->entries[j].score < lb->entries[j + 1].score);
            else
                swap = (lb->entries[j].score > lb->entries[j + 1].score);

            if (swap) {
                LEADERBOARD_ENTRY tmp = lb->entries[j];
                lb->entries[j] = lb->entries[j + 1];
                lb->entries[j + 1] = tmp;
            }
        }
    }
}

/**
 * leaderboard_init_all - Initialize all leaderboard metadata from static table
 *
 * Called once at boot. Populates report names, descriptions, column headers,
 * and sort direction for each leaderboard. Does not load any scores.
 */
void leaderboard_init_all(void)
{
    int i;
    size_t num_boards = sizeof(lb_init_table) / sizeof(lb_init_table[0]);

    memset(leaderboards, 0, sizeof(leaderboards));

    for (i = 0; i < (int)num_boards && i < MAX_LEADERBOARDS; i++) {
        leaderboards[i].type        = lb_init_table[i].type;
        leaderboards[i].redis_key   = lb_init_table[i].redis_key;
        leaderboards[i].report_name = lb_init_table[i].report_name;
        leaderboards[i].description = lb_init_table[i].description;
        leaderboards[i].col_name    = lb_init_table[i].col_name;
        leaderboards[i].col_value   = lb_init_table[i].col_value;
        leaderboards[i].descending  = lb_init_table[i].descending;
        leaderboards[i].is_derived  = lb_init_table[i].is_derived;
        leaderboards[i].count       = 0;
        leaderboards[i].last_refresh = 0;
    }

    leaderboard_refresh_time = current_time;
    leaderboard_backup_time = current_time;

    log_string("Leaderboard system initialized.");
}

/**
 * leaderboard_update_score - Update a player's score on a specific leaderboard
 *
 * Updates both Redis (if available) and the in-memory cache.
 *
 * @param type   Leaderboard type constant (REPORT_TOP_*)
 * @param name   Player name
 * @param score  New score value
 */
void leaderboard_update_score(int type, const char *name, double score)
{
    if (type < 0 || type >= MAX_LEADERBOARDS || !name || name[0] == '\0')
        return;

    LEADERBOARD_DATA *lb = &leaderboards[type];

    if (!lb->redis_key)
        return;

    /* Update Redis */
    redis_leaderboard_update(lb->redis_key, name, score);

    /* Update in-memory cache */
    leaderboard_update_inmemory(lb, name, score);
}

/**
 * leaderboard_on_player_kill - Update all relevant boards after a PvP kill
 *
 * Called after a player kill is recorded. Updates PK, CPK, ratio, and death
 * boards for both the killer and victim.
 *
 * @param ch      The killer
 * @param victim  The victim
 */
void leaderboard_on_player_kill(CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (!ch || !victim || IS_NPC(ch) || IS_NPC(victim))
        return;

    /* Update PK kills for the killer */
    leaderboard_update_score(REPORT_TOP_PLAYER_KILLERS, ch->name,
        (double)ch->player_kills);

    /* Update CPK kills if applicable */
    leaderboard_update_score(REPORT_TOP_CPLAYER_KILLERS, ch->name,
        (double)ch->cpk_kills);

    /* Update deaths for victim */
    leaderboard_update_score(REPORT_TOP_DEATHS, victim->name,
        (double)victim->deaths);

    /* Update ratio for both */
    leaderboard_update_ratio(ch);
    leaderboard_update_ratio(victim);
}

/**
 * leaderboard_update_ratio - Recompute and update PvP win ratio for a player
 *
 * Calculates kills/(kills+deaths) ratio and updates both the worst and best
 * ratio boards. Also stores the raw kills/deaths in Redis for recomputation.
 * Requires minimum LEADERBOARD_MIN_RATIO_FIGHTS total fights to qualify.
 *
 * @param ch  The player to update
 */
void leaderboard_update_ratio(CHAR_DATA *ch)
{
    int total_kills, total_deaths, total_fights;
    double ratio;

    if (!ch || IS_NPC(ch))
        return;

    total_kills = ch->player_kills + ch->cpk_kills + ch->arena_kills;
    total_deaths = ch->player_deaths + ch->cpk_deaths + ch->arena_deaths;
    total_fights = total_kills + total_deaths;

    /* Store raw data in Redis for server-side recomputation */
    redis_leaderboard_set_ratio_data(ch->name, total_kills, total_deaths);

    /* Only qualify for the ratio board with enough fights */
    if (total_fights < LEADERBOARD_MIN_RATIO_FIGHTS)
        return;

    ratio = (double)total_kills / (double)total_fights;

    leaderboard_update_score(REPORT_TOP_WORST_RATIO, ch->name, ratio);
    leaderboard_update_score(REPORT_TOP_BEST_RATIO, ch->name, ratio);
}

/**
 * leaderboard_update_wealth - Update the wealthiest leaderboard for a player
 *
 * @param ch  The player whose bank balance changed
 */
void leaderboard_update_wealth(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return;

    leaderboard_update_score(REPORT_TOP_WEALTHIEST, ch->name,
        (double)ch->pcdata->bankbalance);
}

/**
 * leaderboard_on_login - Push all of a character's stats to leaderboards on login
 *
 * Makes the leaderboard system self-healing: as players log in, their
 * historical stats are pushed to Redis and the in-memory cache without
 * requiring a manual migration.
 *
 * @param ch  The character who just logged in
 */
void leaderboard_on_login(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch))
        return;

    if (ch->player_kills > 0)
        leaderboard_update_score(REPORT_TOP_PLAYER_KILLERS, ch->name,
            (double)ch->player_kills);

    if (ch->cpk_kills > 0)
        leaderboard_update_score(REPORT_TOP_CPLAYER_KILLERS, ch->name,
            (double)ch->cpk_kills);

    if (ch->monster_kills > 0)
        leaderboard_update_score(REPORT_TOP_MONSTER_KILLERS, ch->name,
            (double)ch->monster_kills);

    if (ch->deaths > 0)
        leaderboard_update_score(REPORT_TOP_DEATHS, ch->name,
            (double)ch->deaths);

    if (ch->pcdata && ch->pcdata->quests_completed > 0)
        leaderboard_update_score(REPORT_TOP_QUESTS, ch->name,
            (double)ch->pcdata->quests_completed);

    leaderboard_update_wealth(ch);
    leaderboard_update_ratio(ch);
}

/**
 * leaderboard_refresh_one - Refresh a single leaderboard from Redis
 *
 * @param lb  The leaderboard to refresh
 */
static void leaderboard_refresh_one(LEADERBOARD_DATA *lb)
{
    char *names[MAX_LEADERBOARD_ENTRIES];
    double scores[MAX_LEADERBOARD_ENTRIES];
    int count, i;

    memset(names, 0, sizeof(names));
    memset(scores, 0, sizeof(scores));

    if (lb->is_derived) {
        /* Ratio boards use the special hash-based query */
        count = redis_leaderboard_get_ratio_data(MAX_LEADERBOARD_ENTRIES,
            names, scores, LEADERBOARD_MIN_RATIO_FIGHTS);
    } else if (lb->descending) {
        count = redis_leaderboard_get_top(lb->redis_key, MAX_LEADERBOARD_ENTRIES,
            names, scores);
    } else {
        count = redis_leaderboard_get_bottom(lb->redis_key, MAX_LEADERBOARD_ENTRIES,
            names, scores);
    }

    /* Clear existing entries */
    lb->count = 0;
    memset(lb->entries, 0, sizeof(lb->entries));

    for (i = 0; i < count && i < MAX_LEADERBOARD_ENTRIES; i++) {
        if (!names[i])
            continue;

        strncpy(lb->entries[lb->count].name, names[i], MAX_INPUT_LENGTH - 1);
        lb->entries[lb->count].name[MAX_INPUT_LENGTH - 1] = '\0';
        lb->entries[lb->count].score = scores[i];
        format_display_value(lb, scores[i], lb->entries[lb->count].display_value);
        lb->count++;

        free(names[i]);
    }

    /* Free any remaining names if count was capped */
    for (; i < MAX_LEADERBOARD_ENTRIES; i++) {
        if (names[i])
            free(names[i]);
    }

    lb->last_refresh = current_time;
}

/**
 * leaderboard_refresh_from_redis - Pull top entries from Redis into memory for all boards
 *
 * Called periodically (every 5 minutes) to keep the in-memory cache fresh.
 */
void leaderboard_refresh_from_redis(void)
{
    int i;

    if (!redis_is_available())
        return;

    for (i = 0; i < MAX_LEADERBOARDS; i++) {
        if (leaderboards[i].redis_key)
            leaderboard_refresh_one(&leaderboards[i]);
    }

    leaderboard_refresh_time = current_time;
    log_string("Leaderboards refreshed from Redis.");
}

/**
 * leaderboard_load_backup - Load leaderboard data from JSON backup on disk
 *
 * Used at boot to populate in-memory data before Redis is consulted.
 *
 * @return true if backup was loaded successfully
 */
bool leaderboard_load_backup(void)
{
    json_t *root, *boards, *board, *entries_arr, *entry;
    json_error_t error;
    int i;

    root = json_load_file(LEADERBOARD_JSON_FILE, 0, &error);
    if (!root) {
        log_string("No leaderboard backup found, starting fresh.");
        return false;
    }

    boards = json_object_get(root, "boards");
    if (!json_is_array(boards)) {
        json_decref(root);
        return false;
    }

    for (i = 0; i < (int)json_array_size(boards) && i < MAX_LEADERBOARDS; i++) {
        LEADERBOARD_DATA *lb;
        int type, j;

        board = json_array_get(boards, i);
        if (!json_is_object(board))
            continue;

        type = (int)json_integer_value(json_object_get(board, "type"));
        if (type < 0 || type >= MAX_LEADERBOARDS)
            continue;

        lb = &leaderboards[type];
        entries_arr = json_object_get(board, "entries");
        if (!json_is_array(entries_arr))
            continue;

        lb->count = 0;
        for (j = 0; j < (int)json_array_size(entries_arr) && j < MAX_LEADERBOARD_ENTRIES; j++) {
            const char *name;

            entry = json_array_get(entries_arr, j);
            if (!json_is_object(entry))
                continue;

            name = json_string_value(json_object_get(entry, "name"));
            if (!name || name[0] == '\0')
                continue;

            strncpy(lb->entries[lb->count].name, name, MAX_INPUT_LENGTH - 1);
            lb->entries[lb->count].name[MAX_INPUT_LENGTH - 1] = '\0';
            lb->entries[lb->count].score = json_number_value(json_object_get(entry, "score"));
            format_display_value(lb, lb->entries[lb->count].score,
                lb->entries[lb->count].display_value);
            lb->count++;
        }

        lb->last_refresh = current_time;
    }

    json_decref(root);
    log_string("Leaderboard backup loaded.");
    return true;
}

/**
 * leaderboard_save_backup - Write leaderboard data to JSON backup on disk
 *
 * Uses temp+rename for atomic writes.
 */
void leaderboard_save_backup(void)
{
    json_t *root, *boards;
    char tmp_path[256];
    int i;

    root = json_object();
    boards = json_array();

    for (i = 0; i < MAX_LEADERBOARDS; i++) {
        LEADERBOARD_DATA *lb = &leaderboards[i];
        json_t *board, *entries_arr;
        int j;

        if (!lb->redis_key)
            continue;

        board = json_object();
        json_object_set_new(board, "type", json_integer(lb->type));
        json_object_set_new(board, "key", json_string(lb->redis_key));

        entries_arr = json_array();
        for (j = 0; j < lb->count; j++) {
            json_t *entry = json_object();
            json_object_set_new(entry, "name", json_string(lb->entries[j].name));
            json_object_set_new(entry, "score", json_real(lb->entries[j].score));
            json_array_append_new(entries_arr, entry);
        }

        json_object_set_new(board, "entries", entries_arr);
        json_array_append_new(boards, board);
    }

    json_object_set_new(root, "boards", boards);
    json_object_set_new(root, "saved_at", json_integer((json_int_t)current_time));

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", LEADERBOARD_JSON_FILE);
    if (json_dump_file(root, tmp_path, JSON_INDENT(2) | JSON_PRESERVE_ORDER) == 0) {
        rename(tmp_path, LEADERBOARD_JSON_FILE);
    } else {
        pbugf(LOG_ERROR, "Failed to save leaderboard backup to %s", tmp_path);
    }

    json_decref(root);
    leaderboard_backup_time = current_time;
}

/**
 * leaderboard_seed_redis - On boot, push backup data to Redis if boards are empty
 *
 * If Redis has data (ZCARD > 0 for any board), refresh from Redis instead.
 * If Redis is empty, seed it from the backup we just loaded.
 */
void leaderboard_seed_redis(void)
{
    int i;
    bool redis_has_data = false;

    if (!redis_is_available()) {
        log_string("Redis not available, using backup data only.");
        return;
    }

    /* Check if Redis already has leaderboard data */
    for (i = 0; i < MAX_LEADERBOARDS; i++) {
        if (leaderboards[i].redis_key &&
            redis_leaderboard_count(leaderboards[i].redis_key) > 0) {
            redis_has_data = true;
            break;
        }
    }

    if (redis_has_data) {
        log_string("Redis has leaderboard data, refreshing from Redis.");
        leaderboard_refresh_from_redis();
    } else {
        /* Push backup data to Redis */
        log_string("Seeding Redis with backup leaderboard data.");
        for (i = 0; i < MAX_LEADERBOARDS; i++) {
            LEADERBOARD_DATA *lb = &leaderboards[i];
            int j;

            if (!lb->redis_key || lb->is_derived)
                continue;

            for (j = 0; j < lb->count; j++) {
                redis_leaderboard_update(lb->redis_key,
                    lb->entries[j].name, lb->entries[j].score);
            }
        }
    }
}


/*
 * =========================================================================
 * Display functions - reimplemented to use leaderboards[] instead of stat_table[]
 * =========================================================================
 */

/**
 * do_stats - Player command to display leaderboards
 *
 * Syntax: stats <pkers|cpkers|quests|monsters|wealthiest|ratio|deaths>
 */
void do_stats(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output = NULL;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Usage: Stats <pkers|cpkers|quests|monsters|wealthiest|ratio|deaths>\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "pkers"))
        output = get_stats(REPORT_TOP_PLAYER_KILLERS);
    else if (!str_cmp(arg, "cpkers"))
        output = get_stats(REPORT_TOP_CPLAYER_KILLERS);
    else if (!str_cmp(arg, "wealthiest"))
        output = get_stats(REPORT_TOP_WEALTHIEST);
    else if (!str_cmp(arg, "monsters"))
        output = get_stats(REPORT_TOP_MONSTER_KILLERS);
    else if (!str_cmp(arg, "ratio"))
        output = get_stats(REPORT_TOP_WORST_RATIO);
    else if (!str_cmp(arg, "quests"))
        output = get_stats(REPORT_TOP_QUESTS);
    else if (!str_cmp(arg, "deaths"))
        output = get_stats(REPORT_TOP_DEATHS);
    else {
        send_to_char("Usage: Stats <pkers|cpkers|quests|monsters|wealthiest|ratio|deaths>\n\r", ch);
        return;
    }

    page_to_char(buf_string(output), ch);
    free_buf(output);
}

/**
 * get_stats - Generate formatted leaderboard output for in-game display
 *
 * @param type  Leaderboard type constant (REPORT_TOP_*)
 * @return      BUFFER containing formatted output (caller must free)
 */
BUFFER *get_stats(int type)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    BUFFER *output;
    LEADERBOARD_DATA *lb;
    int i;

    output = new_buf();

    if (type < 0 || type >= MAX_LEADERBOARDS || !leaderboards[type].report_name) {
        sprintf(buf, "Sorry, these stats are currently unavailable.\n\r");
        add_buf(output, buf);
        pbugf(LOG_ERROR, "Stats for type %d not available!", type);
        return output;
    }

    lb = &leaderboards[type];

    add_buf(output, lb->report_name);
    add_buf(output, "\r\n\r\n");
    add_buf(output, lb->description);
    add_buf(output, "\r\n\r\n");

    sprintf(buf2, "Rank %%-%ds %%s\r\n", 44 - (int)strlen(lb->col_value) / 2);
    sprintf(buf, buf2, lb->col_name, lb->col_value);
    add_buf(output, buf);

    add_buf(output, "{B----------{C------------------{W------------{C------------------{B---------{x");
    add_buf(output, "\r\n");

    for (i = 0; i < MAX_LEADERBOARD_ENTRIES; i++) {
        if (i < lb->count && lb->entries[i].name[0] != '\0') {
            sprintf(buf, " {Y#%-4d{x %-40s %s\n\r",
                i + 1, lb->entries[i].name, lb->entries[i].display_value);
        } else {
            sprintf(buf, " {Y#%-4d{x %-40s %s\n\r", i + 1, "---", "---");
        }
        add_buf(output, buf);
    }

    return output;
}

/**
 * get_stats_for_html - Generate HTML-formatted leaderboard output
 *
 * @param type  Leaderboard type constant (REPORT_TOP_*)
 * @return      BUFFER containing HTML output (caller must free)
 */
BUFFER *get_stats_for_html(int type)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    BUFFER *output;
    LEADERBOARD_DATA *lb;
    int i;

    output = new_buf();

    if (type < 0 || type >= MAX_LEADERBOARDS || !leaderboards[type].report_name)
        return output;

    lb = &leaderboards[type];

    add_buf(output, lb->report_name);
    add_buf(output, "\r\n\r\n{w");
    add_buf(output, lb->description);

    sprintf(buf2, "</td></tr><tr><td valign=\"top\"><table width=\"80%%\" border=\"0\" style=\"padding: 15px;\"><tr><td width=\"10%%\">Rank</td><td width=\"70%%\">%%-%ds</td><td width=\"20%%\">%%s</td></td></tr>",
        44 - (int)strlen(lb->col_value) / 2);
    sprintf(buf, buf2, lb->col_name, lb->col_value);
    add_buf(output, buf);

    add_buf(output, "<tr><td colspan=\"3\">{B----------{C------------------{W--------------------------------------{C------------------{B---------{x</td></tr>");

    for (i = 0; i < MAX_LEADERBOARD_ENTRIES; i++) {
        if (i < lb->count && lb->entries[i].name[0] != '\0') {
            sprintf(buf, "<tr><td width=\"10%%\">{Y#%-4d{x</td><td width=\"70%%\">%-40s</td><td width=\"20%%\">%s</td></tr>",
                i + 1, lb->entries[i].name, lb->entries[i].display_value);
        } else {
            sprintf(buf, "<tr><td width=\"10%%\">{Y#%-4d{x</td><td width=\"70%%\">%-40s</td><td width=\"20%%\">%s</td></tr>",
                i + 1, "---", "---");
        }
        add_buf(output, buf);
    }

    add_buf(output, "</table>");
    return output;
}


/*
 * =========================================================================
 * Migration - scan character files and populate leaderboards
 * =========================================================================
 */

/* Stats extracted from a character file for leaderboard seeding */
typedef struct {
    char    name[MAX_INPUT_LENGTH];
    long    player_kills;
    long    cpk_kills;
    long    arena_kills;
    long    monster_kills;
    long    deaths;
    long    player_deaths;
    long    cpk_deaths;
    long    arena_deaths;
    long    bankbalance;
    long    quests_completed;
} CHAR_LEADERBOARD_STATS;

/**
 * read_stats_from_old_pfile - Extract leaderboard stats from old text pfile
 *
 * @param path   Path to the pfile
 * @param stats  Output struct to populate
 * @return       true if stats were read successfully
 */
static bool read_stats_from_old_pfile(const char *path, CHAR_LEADERBOARD_STATS *stats)
{
    FILE *fp;
    char line[MAX_STRING_LENGTH];

    memset(stats, 0, sizeof(*stats));

    fp = fopen(path, "r");
    if (!fp)
        return false;

    while (fgets(line, sizeof(line), fp)) {
        char key[64];
        long val;

        /* Strip trailing whitespace */
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == '\n' || *end == '\r' || *end == ' '))
            *end-- = '\0';

        /* Parse "Name Foo~" */
        if (sscanf(line, "Name %63[^~]", key) == 1 && stats->name[0] == '\0') {
            strncpy(stats->name, key, MAX_INPUT_LENGTH - 1);
            stats->name[MAX_INPUT_LENGTH - 1] = '\0';
            /* Trim trailing spaces from name */
            end = stats->name + strlen(stats->name) - 1;
            while (end > stats->name && *end == ' ')
                *end-- = '\0';
            continue;
        }

        /* Parse "Key value" pairs */
        if (sscanf(line, "%63s %ld", key, &val) == 2) {
            if (!strcmp(key, "PKKills"))              stats->player_kills = val;
            else if (!strcmp(key, "CPKKills"))         stats->cpk_kills = val;
            else if (!strcmp(key, "ArenaKills"))       stats->arena_kills = val;
            else if (!strcmp(key, "MonsterKills"))     stats->monster_kills = val;
            else if (!strcmp(key, "DeathCount"))       stats->deaths = val;
            else if (!strcmp(key, "PKCount"))          stats->player_deaths = val;
            else if (!strcmp(key, "CPKCount"))         stats->cpk_deaths = val;
            else if (!strcmp(key, "ArenaCount"))       stats->arena_deaths = val;
            else if (!strcmp(key, "Bank"))             stats->bankbalance = val;
            else if (!strcmp(key, "QuestsCompleted"))  stats->quests_completed = val;
        }
    }

    fclose(fp);
    return (stats->name[0] != '\0');
}

/**
 * read_stats_from_json_pfile - Extract leaderboard stats from JSON pfile
 *
 * @param path   Path to the JSON file
 * @param stats  Output struct to populate
 * @return       true if stats were read successfully
 */
static bool read_stats_from_json_pfile(const char *path, CHAR_LEADERBOARD_STATS *stats)
{
    json_t *root, *character;
    json_error_t error;
    const char *name;

    memset(stats, 0, sizeof(*stats));

    root = json_load_file(path, 0, &error);
    if (!root)
        return false;

    character = json_object_get(root, "character");
    if (!json_is_object(character)) {
        json_decref(root);
        return false;
    }

    name = json_string_value(json_object_get(character, "name"));
    if (!name || name[0] == '\0') {
        json_decref(root);
        return false;
    }

    strncpy(stats->name, name, MAX_INPUT_LENGTH - 1);
    stats->name[MAX_INPUT_LENGTH - 1] = '\0';

    stats->player_kills     = json_integer_value(json_object_get(character, "player_kills"));
    stats->cpk_kills        = json_integer_value(json_object_get(character, "cpk_kills"));
    stats->arena_kills      = json_integer_value(json_object_get(character, "arena_kills"));
    stats->monster_kills    = json_integer_value(json_object_get(character, "monster_kills"));
    stats->deaths           = json_integer_value(json_object_get(character, "deaths"));
    stats->player_deaths    = json_integer_value(json_object_get(character, "player_deaths"));
    stats->cpk_deaths       = json_integer_value(json_object_get(character, "cpk_deaths"));
    stats->arena_deaths     = json_integer_value(json_object_get(character, "arena_deaths"));
    stats->bankbalance      = json_integer_value(json_object_get(character, "bankbalance"));
    stats->quests_completed = json_integer_value(json_object_get(character, "quests_completed"));

    json_decref(root);
    return true;
}

/**
 * leaderboard_process_stats - Feed extracted stats into all leaderboards
 *
 * @param stats  Stats from a character file
 */
static void leaderboard_process_stats(CHAR_LEADERBOARD_STATS *stats)
{
    int total_kills, total_deaths, total_fights;

    if (stats->player_kills > 0)
        leaderboard_update_score(REPORT_TOP_PLAYER_KILLERS, stats->name,
            (double)stats->player_kills);

    if (stats->cpk_kills > 0)
        leaderboard_update_score(REPORT_TOP_CPLAYER_KILLERS, stats->name,
            (double)stats->cpk_kills);

    if (stats->bankbalance > 0)
        leaderboard_update_score(REPORT_TOP_WEALTHIEST, stats->name,
            (double)stats->bankbalance);

    if (stats->monster_kills > 0)
        leaderboard_update_score(REPORT_TOP_MONSTER_KILLERS, stats->name,
            (double)stats->monster_kills);

    if (stats->quests_completed > 0)
        leaderboard_update_score(REPORT_TOP_QUESTS, stats->name,
            (double)stats->quests_completed);

    if (stats->deaths > 0)
        leaderboard_update_score(REPORT_TOP_DEATHS, stats->name,
            (double)stats->deaths);

    /* Ratio calculation */
    total_kills = stats->player_kills + stats->cpk_kills + stats->arena_kills;
    total_deaths = stats->player_deaths + stats->cpk_deaths + stats->arena_deaths;
    total_fights = total_kills + total_deaths;

    redis_leaderboard_set_ratio_data(stats->name, total_kills, total_deaths);

    if (total_fights >= LEADERBOARD_MIN_RATIO_FIGHTS) {
        double ratio = (double)total_kills / (double)total_fights;
        leaderboard_update_score(REPORT_TOP_WORST_RATIO, stats->name, ratio);
        leaderboard_update_score(REPORT_TOP_BEST_RATIO, stats->name, ratio);
    }
}

/**
 * leaderboard_migrate - Scan all character files and populate leaderboards
 *
 * @param ch  The admin running the command (for output)
 * @return    Number of characters processed
 */
static int leaderboard_migrate(CHAR_DATA *ch)
{
    DIR *dirp, *subdirp;
    struct dirent *dp, *subdp;
    CHAR_LEADERBOARD_STATS stats;
    char path[512];
    char player_dir_buf[256];
    const char *player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    int count = 0;
    char letter;

    /* Scan characters/[a-z]/ subdirectories for old-format pfiles */
    dirp = opendir(player_dir);
    if (!dirp) {
        send_to_char("Unable to open character directory.\n\r", ch);
        return 0;
    }

    while ((dp = readdir(dirp)) != NULL) {
        /* Look for single-letter subdirectories */
        if (strlen(dp->d_name) != 1 || !islower(dp->d_name[0]))
            continue;

        letter = dp->d_name[0];
        snprintf(path, sizeof(path), "%s%c", player_dir, letter);

        subdirp = opendir(path);
        if (!subdirp)
            continue;

        while ((subdp = readdir(subdirp)) != NULL) {
            if (subdp->d_name[0] == '.')
                continue;

            /* Check for .json extension */
            size_t namelen = strlen(subdp->d_name);
            if (namelen > 5 && !strcmp(subdp->d_name + namelen - 5, ".json")) {
                snprintf(path, sizeof(path), "%s%c/%s", player_dir, letter, subdp->d_name);
                if (read_stats_from_json_pfile(path, &stats)) {
                    leaderboard_process_stats(&stats);
                    count++;
                }
            } else {
                snprintf(path, sizeof(path), "%s%c/%s", player_dir, letter, subdp->d_name);
                if (read_stats_from_old_pfile(path, &stats)) {
                    leaderboard_process_stats(&stats);
                    count++;
                }
            }
        }

        closedir(subdirp);
    }

    closedir(dirp);

    /* Also scan for top-level .json files (e.g., characters/Xev.json) */
    dirp = opendir(player_dir);
    if (dirp) {
        while ((dp = readdir(dirp)) != NULL) {
            size_t namelen = strlen(dp->d_name);
            if (namelen > 5 && !strcmp(dp->d_name + namelen - 5, ".json")) {
                snprintf(path, sizeof(path), "%s%s", player_dir, dp->d_name);
                if (read_stats_from_json_pfile(path, &stats)) {
                    leaderboard_process_stats(&stats);
                    count++;
                }
            }
        }
        closedir(dirp);
    }

    return count;
}

/**
 * do_leaderboard - Admin command for leaderboard management
 *
 * Syntax:
 *   leaderboard migrate  - scan all character files and populate boards
 *   leaderboard refresh  - force Redis refresh
 *   leaderboard backup   - force JSON backup
 *   leaderboard status   - show board counts and last refresh times
 */
void do_leaderboard(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Syntax: leaderboard <migrate|refresh|backup|status>\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "migrate")) {
        int count;

        send_to_char("Starting leaderboard migration from character files...\n\r", ch);
        count = leaderboard_migrate(ch);
        sprintf(buf, "Migration complete. Processed %d character files.\n\r", count);
        send_to_char(buf, ch);

        leaderboard_save_backup();
        send_to_char("Backup saved.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "refresh")) {
        if (!redis_is_available()) {
            send_to_char("Redis is not available.\n\r", ch);
            return;
        }
        leaderboard_refresh_from_redis();
        send_to_char("Leaderboards refreshed from Redis.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "backup")) {
        leaderboard_save_backup();
        send_to_char("Leaderboard backup saved.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "status")) {
        int i;

        send_to_char("{YLeaderboard Status{x\n\r", ch);
        send_to_char("{B----------------------------------------------{x\n\r", ch);

        for (i = 0; i < MAX_LEADERBOARDS; i++) {
            LEADERBOARD_DATA *lb = &leaderboards[i];
            long redis_count = 0;

            if (!lb->redis_key)
                continue;

            if (redis_is_available())
                redis_count = redis_leaderboard_count(lb->redis_key);

            sprintf(buf, "  {W%-12s{x  Memory: %d/10   Redis: %ld   Last refresh: %s",
                lb->redis_key, lb->count, redis_count,
                lb->last_refresh > 0 ? ctime(&lb->last_refresh) : "never\n\r");
            send_to_char(buf, ch);
        }

        sprintf(buf, "\n\r  Next refresh in: %ld seconds\n\r",
            (leaderboard_refresh_time + 300) - current_time);
        send_to_char(buf, ch);
        sprintf(buf, "  Next backup in:  %ld seconds\n\r",
            (leaderboard_backup_time + 3600) - current_time);
        send_to_char(buf, ch);
        return;
    }

    send_to_char("Syntax: leaderboard <migrate|refresh|backup|status>\n\r", ch);
}
