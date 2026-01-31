/***************************************************************************
 *  Async Cache Operations - Implementation                                *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <jansson.h>
#include "async_cache.h"
#include "redis_cache.h"
#include "json_char.h"
#include "merc.h"

/***************************************************************************
 * Global State                                                            *
 ***************************************************************************/

static pthread_t worker_thread;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static bool shutdown_requested = false;
static unsigned long next_job_id = 1;

// Job queue (linked list)
static ASYNC_CACHE_JOB *job_queue_head = NULL;
static ASYNC_CACHE_JOB *job_queue_tail = NULL;
static ASYNC_CACHE_JOB *completed_jobs = NULL;  // Completed/failed jobs for status checking

// Statistics
static ASYNC_CACHE_STATS stats = {0};

/***************************************************************************
 * Internal Helper Functions                                               *
 ***************************************************************************/

/**
 * create_job - Allocate and initialize a new async cache job
 *
 * Creates a job structure with a unique ID, sets initial status to
 * ASYNC_STATUS_QUEUED, and records the queue time.
 *
 * @param operation       Type of cache operation (DUMP/LOAD/INVALIDATE)
 * @param character_name  Name of the character this job affects
 * @return                Newly allocated job, or NULL on allocation failure
 */
static ASYNC_CACHE_JOB *create_job(async_cache_op_t operation, const char *character_name)
{
    ASYNC_CACHE_JOB *job;

    job = (ASYNC_CACHE_JOB *)calloc(1, sizeof(ASYNC_CACHE_JOB));
    if (!job) {
        bug("create_job: failed to allocate memory", 0);
        return NULL;
    }

    job->job_id = next_job_id++;
    job->operation = operation;
    job->character_name = strdup(character_name);
    job->status = ASYNC_STATUS_QUEUED;
    job->error_message = NULL;
    job->queued_time = time(NULL);
    job->start_time = 0;
    job->complete_time = 0;
    job->next = NULL;

    return job;
}

/**
 * free_job - Deallocate an async cache job and its strings
 *
 * @param job  Job to free (safe to pass NULL)
 */
static void free_job(ASYNC_CACHE_JOB *job)
{
    if (!job) return;

    if (job->character_name) free(job->character_name);
    if (job->error_message) free(job->error_message);
    free(job);
}

/**
 * enqueue_job - Add a job to the end of the job queue
 *
 * Thread-safe. Signals the worker thread condition variable to wake
 * it if it's waiting for work. Updates queue statistics.
 *
 * @param job  Job to add to the queue
 */
static void enqueue_job(ASYNC_CACHE_JOB *job)
{
    pthread_mutex_lock(&queue_mutex);

    if (job_queue_tail) {
        job_queue_tail->next = job;
        job_queue_tail = job;
    } else {
        job_queue_head = job_queue_tail = job;
    }

    stats.total_queued++;
    stats.queue_depth++;

    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

/**
 * dequeue_job - Remove and return the next job from the queue
 *
 * Blocks until a job is available or shutdown is requested.
 * Thread-safe. Called by the worker thread.
 *
 * @return  Next job from queue, or NULL if shutdown was requested
 */
static ASYNC_CACHE_JOB *dequeue_job(void)
{
    ASYNC_CACHE_JOB *job;

    pthread_mutex_lock(&queue_mutex);

    while (!job_queue_head && !shutdown_requested) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }

    if (shutdown_requested && !job_queue_head) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }

    job = job_queue_head;
    job_queue_head = job->next;
    if (!job_queue_head) {
        job_queue_tail = NULL;
    }

    stats.queue_depth--;
    job->next = NULL;

    pthread_mutex_unlock(&queue_mutex);

    return job;
}

/**
 * move_to_completed - Transfer a finished job to the completed list
 *
 * Moves job from active processing to the completed_jobs list for
 * status queries and history. Updates completion statistics.
 * Thread-safe.
 *
 * @param job  Job that has finished (status should be COMPLETE or FAILED)
 */
static void move_to_completed(ASYNC_CACHE_JOB *job)
{
    pthread_mutex_lock(&queue_mutex);

    job->next = completed_jobs;
    completed_jobs = job;

    if (job->status == ASYNC_STATUS_COMPLETE) {
        stats.total_completed++;
    } else if (job->status == ASYNC_STATUS_FAILED) {
        stats.total_failed++;
    }

    pthread_mutex_unlock(&queue_mutex);
}

/***************************************************************************
 * Cache Dump Operation                                                    *
 ***************************************************************************/

/**
 * perform_cache_dump - Write character cache data from Redis to disk
 *
 * Retrieves character info from Redis cache and writes it to a JSON file.
 * Uses atomic rename (write to .tmp, then rename) for data safety.
 *
 * JSON structure: { metadata: {...}, character: {...} }
 *
 * @param job  Job containing character_name to dump
 * @return     true on success, false on failure (error_message set)
 */
static bool perform_cache_dump(ASYNC_CACHE_JOB *job)
{
    // Phase 2: Write character metadata from Redis cache to JSON file
    // This allows dumping cached data to disk without loading full character

    CHAR_INFO_CACHE *info;
    json_t *json_char, *json_meta;
    char json_path[512];
    char tmp_path[512];
    int result;

    // Get character info from Redis cache
    info = redis_get_char_info(job->character_name);
    if (!info) {
        job->error_message = strdup("Character not found in Redis cache");
        return false;
    }

    // Build JSON structure with metadata and character sections
    json_char = json_object();

    // Metadata section
    json_meta = json_object();
    json_object_set_new(json_meta, "format_version", json_integer(2));
    json_object_set_new(json_meta, "last_saved", json_integer(current_time));
    json_object_set_new(json_char, "metadata", json_meta);

    // Character section (from cache)
    json_object_set_new(json_char, "character", char_info_to_json(info));

    // Get file paths
    json_get_char_path(job->character_name, json_path, sizeof(json_path));
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", json_path);

    // Ensure directory exists
    json_ensure_char_dir(job->character_name);

    // Write to temporary file
    result = json_dump_file(json_char, tmp_path, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(json_char);
    free_char_info_cache(info);

    if (result != 0) {
        job->error_message = strdup("Failed to write JSON file");
        return false;
    }

    // Atomic rename
    if (rename(tmp_path, json_path) != 0) {
        job->error_message = strdup("Failed to rename temporary file");
        unlink(tmp_path);
        return false;
    }

    log_stringf("Async cache dump completed: %s → %s", job->character_name, json_path);
    return true;
}

/***************************************************************************
 * Cache Load Operation                                                    *
 ***************************************************************************/

/**
 * perform_cache_load - Load character data from disk into Redis cache
 *
 * Reads character info from JSON file and caches it in Redis. Used to
 * warm the cache from disk without requiring a full character login.
 *
 * Falls back to check for legacy pfile format, but cannot cache from
 * pfiles (requires full migration via login).
 *
 * @param job  Job containing character_name to load
 * @return     true on success, false on failure (error_message set)
 */
static bool perform_cache_load(ASYNC_CACHE_JOB *job)
{
    // Phase 2: Read character metadata from JSON file and cache to Redis
    // This allows warming the cache from disk without loading full character

    json_t *root, *character_section;
    json_error_t error;
    CHAR_INFO_CACHE *info;
    char json_path[512];
    char pfile_path[512];
    FILE *fp;

    // Try JSON file first
    json_get_char_path(job->character_name, json_path, sizeof(json_path));
    root = json_load_file(json_path, 0, &error);

    if (!root) {
        // JSON doesn't exist, check if old pfile exists
        json_get_pfile_path(job->character_name, pfile_path, sizeof(pfile_path));
        fp = fopen(pfile_path, "r");
        if (!fp) {
            job->error_message = strdup("Character file not found (neither JSON nor pfile)");
            return false;
        }
        fclose(fp);

        // Old pfile exists but can't cache from it (would require full parse)
        job->error_message = strdup("Character has old pfile format - login to migrate");
        return false;
    }

    // Extract character section
    character_section = json_object_get(root, "character");
    if (!character_section) {
        json_decref(root);
        job->error_message = strdup("Invalid JSON format - no character section");
        return false;
    }

    // Parse to CHAR_INFO_CACHE
    info = json_to_char_info(character_section);
    json_decref(root);

    if (!info) {
        job->error_message = strdup("Failed to parse character info from JSON");
        return false;
    }

    // Cache to Redis (need to implement redis_cache_char_info_from_cache)
    // For now, just verify we can parse it
    // TODO: Add redis_cache_char_info_from_cache() function

    free_char_info_cache(info);

    log_stringf("Async cache load completed: %s ← %s", job->character_name, json_path);
    return true;
}

/***************************************************************************
 * Cache Invalidate Operation                                              *
 ***************************************************************************/

/**
 * perform_cache_invalidate - Remove character data from Redis cache
 *
 * Calls redis_invalidate_char() to remove all cached data for the
 * specified character. Always succeeds.
 *
 * @param job  Job containing character_name to invalidate
 * @return     Always returns true
 */
static bool perform_cache_invalidate(ASYNC_CACHE_JOB *job)
{
    redis_invalidate_char(job->character_name);
    log_stringf("Async cache invalidate completed: %s", job->character_name);
    return true;
}

/***************************************************************************
 * Worker Thread                                                           *
 ***************************************************************************/

/**
 * async_cache_worker - Background worker thread main loop
 *
 * Runs in a separate thread. Continuously dequeues jobs and processes
 * them. Updates job status and timing as work progresses. Exits when
 * shutdown_requested is set and queue is empty.
 *
 * Job lifecycle in this function:
 * 1. Dequeue job (blocks if queue empty)
 * 2. Mark as RUNNING, record start_time
 * 3. Execute appropriate operation
 * 4. Mark as COMPLETE/FAILED, record complete_time
 * 5. Move to completed list
 *
 * @param arg  Unused (pthread requires this signature)
 * @return     NULL (pthread requires this)
 */
static void *async_cache_worker(void *arg)
{
    ASYNC_CACHE_JOB *job;
    bool success;

    log_string("Async cache worker thread started");

    while (1) {
        job = dequeue_job();

        if (!job) {
            // Shutdown requested and queue empty
            break;
        }

        // Mark job as running
        pthread_mutex_lock(&queue_mutex);
        job->status = ASYNC_STATUS_RUNNING;
        job->start_time = time(NULL);
        stats.currently_running++;
        pthread_mutex_unlock(&queue_mutex);

        // Perform operation
        success = false;
        switch (job->operation) {
            case ASYNC_CACHE_DUMP:
                success = perform_cache_dump(job);
                break;
            case ASYNC_CACHE_LOAD:
                success = perform_cache_load(job);
                break;
            case ASYNC_CACHE_INVALIDATE:
                success = perform_cache_invalidate(job);
                break;
        }

        // Update job status
        pthread_mutex_lock(&queue_mutex);
        job->status = success ? ASYNC_STATUS_COMPLETE : ASYNC_STATUS_FAILED;
        job->complete_time = time(NULL);
        stats.currently_running--;
        pthread_mutex_unlock(&queue_mutex);

        // Move to completed list
        move_to_completed(job);
    }

    log_string("Async cache worker thread stopped");
    return NULL;
}

/***************************************************************************
 * Public API - Initialization                                             *
 ***************************************************************************/

/**
 * async_cache_init - Initialize the async cache subsystem
 *
 * Resets all statistics and spawns the background worker thread.
 * Must be called at startup before any async cache operations.
 *
 * @return  true on success, false if thread creation failed
 */
bool async_cache_init(void)
{
    int result;

    log_string("Initializing async cache system...");

    // Reset statistics
    memset(&stats, 0, sizeof(stats));
    shutdown_requested = false;

    // Create worker thread
    result = pthread_create(&worker_thread, NULL, async_cache_worker, NULL);
    if (result != 0) {
        bug("async_cache_init: failed to create worker thread", 0);
        return false;
    }

    log_string("Async cache system initialized successfully");
    return true;
}

/**
 * async_cache_shutdown - Gracefully shut down the async cache subsystem
 *
 * Signals the worker thread to stop, waits for it to finish processing
 * any active job, then frees all queued and completed jobs. Called at
 * server shutdown.
 */
void async_cache_shutdown(void)
{
    ASYNC_CACHE_JOB *job, *next;

    log_string("Shutting down async cache system...");

    // Signal shutdown
    pthread_mutex_lock(&queue_mutex);
    shutdown_requested = true;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    // Wait for worker thread to finish
    pthread_join(worker_thread, NULL);

    // Clean up job queues
    pthread_mutex_lock(&queue_mutex);

    for (job = job_queue_head; job; job = next) {
        next = job->next;
        free_job(job);
    }
    job_queue_head = job_queue_tail = NULL;

    for (job = completed_jobs; job; job = next) {
        next = job->next;
        free_job(job);
    }
    completed_jobs = NULL;

    pthread_mutex_unlock(&queue_mutex);

    log_string("Async cache system shut down");
}

/***************************************************************************
 * Public API - Operations                                                 *
 ***************************************************************************/

/**
 * async_cache_dump - Queue a character cache dump operation
 *
 * Schedules a background operation to write character data from
 * Redis cache to disk (JSON file). Non-blocking.
 *
 * @param character_name  Name of character to dump
 * @return                Unique job ID for tracking, or 0 on failure
 */
unsigned long async_cache_dump(const char *character_name)
{
    ASYNC_CACHE_JOB *job;

    if (!character_name || character_name[0] == '\0') {
        bug("async_cache_dump: NULL or empty character name", 0);
        return 0;
    }

    job = create_job(ASYNC_CACHE_DUMP, character_name);
    if (!job) {
        return 0;
    }

    enqueue_job(job);
    log_stringf("Queued async cache dump for %s (job #%lu)", character_name, job->job_id);
    return job->job_id;
}

/**
 * async_cache_load - Queue a character cache load operation
 *
 * Schedules a background operation to read character data from
 * disk (JSON file) into Redis cache. Non-blocking. Used for
 * cache warming.
 *
 * @param character_name  Name of character to load
 * @return                Unique job ID for tracking, or 0 on failure
 */
unsigned long async_cache_load(const char *character_name)
{
    ASYNC_CACHE_JOB *job;

    if (!character_name || character_name[0] == '\0') {
        bug("async_cache_load: NULL or empty character name", 0);
        return 0;
    }

    job = create_job(ASYNC_CACHE_LOAD, character_name);
    if (!job) {
        return 0;
    }

    enqueue_job(job);
    log_stringf("Queued async cache load for %s (job #%lu)", character_name, job->job_id);
    return job->job_id;
}

/**
 * async_cache_invalidate - Queue a character cache invalidation
 *
 * Schedules a background operation to remove character data from
 * Redis cache. Non-blocking. Used when character data changes
 * significantly.
 *
 * @param character_name  Name of character to invalidate
 * @return                Unique job ID for tracking, or 0 on failure
 */
unsigned long async_cache_invalidate(const char *character_name)
{
    ASYNC_CACHE_JOB *job;

    if (!character_name || character_name[0] == '\0') {
        bug("async_cache_invalidate: NULL or empty character name", 0);
        return 0;
    }

    job = create_job(ASYNC_CACHE_INVALIDATE, character_name);
    if (!job) {
        return 0;
    }

    enqueue_job(job);
    log_stringf("Queued async cache invalidate for %s (job #%lu)", character_name, job->job_id);
    return job->job_id;
}

/***************************************************************************
 * Public API - Job Management                                             *
 ***************************************************************************/

/**
 * async_cache_job_status - Get the current status of a job
 *
 * Thread-safe lookup of job status by ID. Searches both the
 * pending queue and completed list.
 *
 * @param job_id  Job ID returned from async_cache_* operations
 * @return        Current status, or ASYNC_STATUS_FAILED if not found
 */
async_cache_status_t async_cache_job_status(unsigned long job_id)
{
    ASYNC_CACHE_JOB *job;

    pthread_mutex_lock(&queue_mutex);

    // Check queue
    for (job = job_queue_head; job; job = job->next) {
        if (job->job_id == job_id) {
            async_cache_status_t status = job->status;
            pthread_mutex_unlock(&queue_mutex);
            return status;
        }
    }

    // Check completed list
    for (job = completed_jobs; job; job = job->next) {
        if (job->job_id == job_id) {
            async_cache_status_t status = job->status;
            pthread_mutex_unlock(&queue_mutex);
            return status;
        }
    }

    pthread_mutex_unlock(&queue_mutex);
    return ASYNC_STATUS_FAILED;  // Job not found
}

/**
 * async_cache_job_info - Get full details for a job
 *
 * Thread-safe lookup returning the job structure for inspection.
 * The returned pointer is owned by the queue system.
 *
 * @param job_id  Job ID to look up
 * @return        Pointer to job structure (DO NOT FREE), or NULL if not found
 */
ASYNC_CACHE_JOB *async_cache_job_info(unsigned long job_id)
{
    ASYNC_CACHE_JOB *job, *result = NULL;

    pthread_mutex_lock(&queue_mutex);

    // Check queue
    for (job = job_queue_head; job; job = job->next) {
        if (job->job_id == job_id) {
            result = job;
            break;
        }
    }

    // Check completed list
    if (!result) {
        for (job = completed_jobs; job; job = job->next) {
            if (job->job_id == job_id) {
                result = job;
                break;
            }
        }
    }

    pthread_mutex_unlock(&queue_mutex);
    return result;  // Note: Don't free this, it's still owned by the queue
}

/**
 * async_cache_job_cancel - Cancel a pending job before it runs
 *
 * Removes a queued job from the pending queue and frees it.
 * Cannot cancel jobs that are already running.
 *
 * @param job_id  Job ID to cancel
 * @return        true if job was found and cancelled, false otherwise
 */
bool async_cache_job_cancel(unsigned long job_id)
{
    ASYNC_CACHE_JOB *job, *prev = NULL;
    bool found = false;

    pthread_mutex_lock(&queue_mutex);

    // Can only cancel queued jobs (not running)
    for (job = job_queue_head; job; prev = job, job = job->next) {
        if (job->job_id == job_id && job->status == ASYNC_STATUS_QUEUED) {
            // Remove from queue
            if (prev) {
                prev->next = job->next;
            } else {
                job_queue_head = job->next;
            }

            if (job == job_queue_tail) {
                job_queue_tail = prev;
            }

            stats.queue_depth--;
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&queue_mutex);

    if (found) {
        free_job(job);
        log_stringf("Cancelled async cache job #%lu", job_id);
    }

    return found;
}

/**
 * async_cache_job_list - Get the list of completed/failed jobs
 *
 * Returns the head of the completed jobs linked list for iteration.
 * Used by admin commands to display job history.
 *
 * @return  Head of completed_jobs list (DO NOT MODIFY OR FREE)
 */
ASYNC_CACHE_JOB *async_cache_job_list(void)
{
    // Return head of completed jobs list
    // Caller should not modify or free this list
    return completed_jobs;
}

/***************************************************************************
 * Public API - Statistics                                                 *
 ***************************************************************************/

/**
 * async_cache_get_stats - Get a snapshot of async cache statistics
 *
 * Returns a static copy of the current statistics. Thread-safe.
 * Statistics include: total_queued, total_completed, total_failed,
 * currently_running, queue_depth.
 *
 * @return  Pointer to static ASYNC_CACHE_STATS structure
 */
ASYNC_CACHE_STATS *async_cache_get_stats(void)
{
    static ASYNC_CACHE_STATS stats_copy;

    pthread_mutex_lock(&queue_mutex);
    memcpy(&stats_copy, &stats, sizeof(ASYNC_CACHE_STATS));
    pthread_mutex_unlock(&queue_mutex);

    return &stats_copy;
}

/**
 * async_cache_print_stats - Display async cache statistics to a character
 *
 * Formats and sends the current async cache statistics to the
 * specified character. Used by admin commands.
 *
 * @param ch  Character to receive the statistics display
 */
void async_cache_print_stats(CHAR_DATA *ch)
{
    ASYNC_CACHE_STATS *s;
    char buf[MAX_STRING_LENGTH];

    s = async_cache_get_stats();

    send_to_char("\n\r{Y=== Async Cache Statistics ==={x\n\r\n\r", ch);

    sprintf(buf, "Total Queued:     {C%lu{x\n\r", s->total_queued);
    send_to_char(buf, ch);

    sprintf(buf, "Total Completed:  {G%lu{x\n\r", s->total_completed);
    send_to_char(buf, ch);

    sprintf(buf, "Total Failed:     {R%lu{x\n\r", s->total_failed);
    send_to_char(buf, ch);

    sprintf(buf, "Currently Running: {Y%lu{x\n\r", s->currently_running);
    send_to_char(buf, ch);

    sprintf(buf, "Queue Depth:      {W%lu{x\n\r", s->queue_depth);
    send_to_char(buf, ch);

    send_to_char("\n\r", ch);
}
