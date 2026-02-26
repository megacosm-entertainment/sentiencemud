/***************************************************************************
 *  Async Cache Operations - Non-blocking cache dump/load                  *
 *                                                                         *
 *  Provides background thread operations for cache persistence without   *
 *  blocking the main game loop.                                          *
 ***************************************************************************/

#ifndef ASYNC_CACHE_H
#define ASYNC_CACHE_H

#include "../../merc.h"
#include "redis_cache.h"

/***************************************************************************
 * Configuration                                                           *
 ***************************************************************************/

#define MAX_ASYNC_QUEUE 100  // Maximum pending async operations

/***************************************************************************
 * Operation Types                                                         *
 ***************************************************************************/

typedef enum {
    ASYNC_CACHE_DUMP,     // Dump character from Redis to disk
    ASYNC_CACHE_LOAD,     // Load character from disk to Redis
    ASYNC_CACHE_INVALIDATE // Remove character from Redis
} async_cache_op_t;

typedef enum {
    ASYNC_STATUS_QUEUED,
    ASYNC_STATUS_RUNNING,
    ASYNC_STATUS_COMPLETE,
    ASYNC_STATUS_FAILED
} async_cache_status_t;

/***************************************************************************
 * Data Structures                                                         *
 ***************************************************************************/

// Operation tracking structure
typedef struct async_cache_job {
    unsigned long job_id;           // Unique job ID
    async_cache_op_t operation;     // Operation type
    char *character_name;           // Character name
    async_cache_status_t status;    // Current status
    char *error_message;            // Error message if failed
    time_t queued_time;             // When job was queued
    time_t start_time;              // When job started
    time_t complete_time;           // When job completed
    struct async_cache_job *next;   // Linked list
} ASYNC_CACHE_JOB;

// Statistics
typedef struct async_cache_stats {
    unsigned long total_queued;
    unsigned long total_completed;
    unsigned long total_failed;
    unsigned long currently_running;
    unsigned long queue_depth;
} ASYNC_CACHE_STATS;

/***************************************************************************
 * Core Functions                                                          *
 ***************************************************************************/

// Initialize async cache system
bool async_cache_init(void);

// Shutdown async cache system (wait for pending operations)
void async_cache_shutdown(void);

/***************************************************************************
 * Async Operations                                                        *
 ***************************************************************************/

// Queue a cache dump operation (Redis → disk)
// Returns job ID, or 0 on failure
unsigned long async_cache_dump(const char *character_name);

// Queue a cache load operation (disk → Redis)
// Returns job ID, or 0 on failure
unsigned long async_cache_load(const char *character_name);

// Queue a cache invalidation (remove from Redis)
// Returns job ID, or 0 on failure
unsigned long async_cache_invalidate(const char *character_name);

/***************************************************************************
 * Job Management                                                          *
 ***************************************************************************/

// Check job status
async_cache_status_t async_cache_job_status(unsigned long job_id);

// Get job details
ASYNC_CACHE_JOB *async_cache_job_info(unsigned long job_id);

// Cancel a pending job (can't cancel running jobs)
bool async_cache_job_cancel(unsigned long job_id);

// Get list of all jobs (for admin display)
ASYNC_CACHE_JOB *async_cache_job_list(void);

/***************************************************************************
 * Statistics                                                              *
 ***************************************************************************/

// Get async cache statistics
ASYNC_CACHE_STATS *async_cache_get_stats(void);

// Print statistics to character
void async_cache_print_stats(CHAR_DATA *ch);

/***************************************************************************
 * Admin Commands                                                          *
 ***************************************************************************/

// These will be called from do_cachedump, do_cacheload, etc.
// Defined here for reference, implemented in act_wiz.c

void do_cachedump(CHAR_DATA *ch, char *argument);
void do_cacheload(CHAR_DATA *ch, char *argument);
void do_cachejobs(CHAR_DATA *ch, char *argument);

#endif /* ASYNC_CACHE_H */
