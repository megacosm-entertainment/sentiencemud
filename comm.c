 /***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Thanks to abaddon for proof-reading our comm.c and pointing out bugs.  *
 *  Any remaining bugs are, of course, our work, not his.  :)              *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/**
 * @file comm.c
 * @brief Core communication and I/O handling for the MUD server
 *
 * This file contains all of the OS-dependent functionality including:
 *   - Server startup, signals, and BSD sockets for TCP/IP
 *   - Main game loop and timing
 *   - Descriptor (connection) management
 *   - Input/output processing with telnet, TLS, and WebSocket support
 *   - Protocol negotiation (MCCP compression, MSP sound, GMCP)
 *
 * Data flow for input:
 *    game_loop() ---> read_from_descriptor() ---> read()
 *    game_loop() ---> read_from_buffer()
 *
 * Data flow for output:
 *    game_loop() ---> process_output() ---> write_to_descriptor() -> write()
 *
 * The OS-dependent functions are read_from_descriptor() and write_to_descriptor().
 *
 * @note Original comment: -- Furey  26 Jan 1993
 */

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "log.h"
/* VIZZWILDS - support for plogf() and printf_to_char() functions*/
#include <stdarg.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "strings.h"
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "scripts.h"
#include "tables.h"
#include "wilds.h"
#include "event_types.h"
#include "class_data.h"
#include "protocol.h"
#include "bootstrap/bootstrap.h"
#include "io/cache/redis_cache.h"
#include "io/cache/async_cache.h"
#include "io/json/json_persist.h"
#include "channel_service.h"
#include "traits.h"
#include "account/unlock.h"
#include "wilderness_storage.h"

/*
 * Socket and TCP/IP stuff.
 */
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "telnet.h"
const	char	echo_off_str	[] = { IAC, WILL, TELOPT_ECHO, '\0' };
const	char	echo_on_str	[] = { IAC, WONT, TELOPT_ECHO, '\0' };
const	char 	go_ahead_str	[] = { IAC, GA, '\0' };
const   char    compress_will   [] = { IAC, WILL, TELOPT_COMPRESS2, '\0' };
const   char    compress_do     [] = { IAC, DO, TELOPT_COMPRESS2, '\0' };
const   char    compress_dont   [] = { IAC, DONT, TELOPT_COMPRESS2, '\0' };

/* MSP strings */
const   char    msp_will        [] = { IAC, WILL, TELOPT_MSP, '\0' };
const   char    msp_do          [] = { IAC, DO, TELOPT_MSP, '\0' };
const   char    msp_dont        [] = { IAC, DONT, TELOPT_MSP, '\0' };


/*
 * OS-dependent declarations.
 */


void show_form_state(CHAR_DATA *ch);
/* VIZZWILDS */
void join_world args ((DESCRIPTOR_DATA * d));


/*
 * External functions.
 */
extern void boat_attack(CHAR_DATA *ch);
extern void init_string_space();



/**
 * @name Global Variables
 * @{
 */
bool			is_test_port;           /**< True if running on test/dev port */
bool			test_mode = false;      /**< True to run integration tests and exit */
char			test_pattern[256];      /**< Test pattern filter (e.g., "unit", "wnum", "all") */
char            runtime_game_root[MAX_INPUT_LENGTH]; /**< Optional runtime override for /sentience root */
int 		    telnet_port;            /**< Port for unencrypted telnet connections */
int				tls_port;               /**< Port for TLS-encrypted connections */
int				websocket_port;         /**< Port for WebSocket TLS connections */
GLOBAL_DATA         gconfig;		    /**< Persistent global config (UID tracking, etc.) */
GAME_SETTINGS_DATA  game_settings;      /**< Runtime game configuration settings */
LLIST *conn_players;                    /**< List of connected player descriptors */
LLIST *conn_immortals;                  /**< List of connected immortal descriptors */
LLIST *conn_online;                     /**< List of all online descriptors */
DESCRIPTOR_DATA *   descriptor_list;	/**< Head of linked list of all open descriptors */
DESCRIPTOR_DATA *   d_next;		        /**< Next descriptor to process in game loop */
FILE *		    fpReserve;		        /**< Reserved file handle for emergencies */
bool		    god;		            /**< If true, all new chars are gods (debug mode) */
bool		    merc_down;		        /**< If true, server is shutting down */
bool		    wizlock;		        /**< If true, only immortals can connect */
bool		    newlock;		        /**< If true, no new characters can be created */
char		    str_boot_time[MAX_INPUT_LENGTH]; /**< Boot time as formatted string */
time_t		    current_time;	        /**< Current time of this pulse */
bool		    MOBtrigger = true;      /**< Controls whether act() triggers mob scripts */
LLIST *loaded_areas;                    /**< List of all loaded areas */
SSL_CTX *ctx;                           /**< Global OpenSSL context for TLS connections */
int ssl_errors_since_reset = 0;         /**< SSL error counter for circuit breaker */
time_t last_ssl_error = 0;              /**< Timestamp of last SSL error */
LLIST *ssl_ctx_cleanup_queue = NULL;    /**< Queue of old SSL contexts awaiting cleanup */
bool it_debug = false;                  /**< Integration test debug mode */
/** @} */
/*
 * OS-dependent local functions.
 */
void	game_loop		args((int control_telnet, int control_tls, int control_websocket));
int	init_socket		args((int port));
int init_tls_socket	args((int port));
void	init_descriptor		args((int control, int control_telnet, int control_tls, int control_websocket));
bool	read_from_descriptor	args((DESCRIPTOR_DATA *d));
bool	write_to_descriptor	args((DESCRIPTOR_DATA *d, char *txt, int length));
bool	write_to_descriptor_2	args((DESCRIPTOR_DATA *d, char *txt, int length));


/*
 * Other local functions (OS-independent).
 */
bool	check_parse_name	args((char *name));
//Temporarily disabling due to reconnect crash
//CHAR_DATA *find_existing_player	args((char *name));
bool	check_reconnect		args((DESCRIPTOR_DATA *d, char *name, bool fConn));
bool	check_playing		args((DESCRIPTOR_DATA *d, char *name));
int	main			args((int argc, char **argv));
int	run_integration_tests	args((const char *pattern));
bool	process_output		args((DESCRIPTOR_DATA *d, bool fPrompt));
void	read_from_buffer	args((DESCRIPTOR_DATA *d));
void	stop_idling		args((CHAR_DATA *ch));
void    bust_a_prompt           args((CHAR_DATA *ch));
bool acceptablePassword(DESCRIPTOR_DATA *d, char *pass);
void add_possible_subclasses(CHAR_DATA *ch, char *string);
void add_possible_races(ACCOUNT_DATA *account, char *string);


#define MAX_LOGFILE		1000000
char logfile_std[MIL];
char logfile_err[MIL];


/**
 * RedirectSTDOUT - Redirect stdout to a timestamped log file
 *
 * Creates a new log file with the current timestamp and redirects stdout
 * to it. Uses unbuffered output to ensure log entries are written immediately.
 * Called during server startup before the game loop begins.
 *
 * Log files are named: LOG_DIR/sent_YYYY-MM-DD-HH:MM:SS.log
 */
static void RedirectSTDOUT(void)
{
    FILE *newfp;
    char log_time[100];

    /* Redirect standard input and standard output*/
    strftime(log_time, 100, "%F-%X", localtime(&current_time));
    sprintf(logfile_std, LOG_DIR "sent_%s.log",log_time);
    if(!(newfp = freopen(logfile_std,"a",stdout))) { /* This happens on NT*/
#if !defined(stdout)
        stdout = fopen(logfile_std,"a");
#else
        if((newfp = fopen(logfile_std,"a")))
            *stdout = *newfp;
#endif
    }

    fseek(stdout,0,SEEK_END);
    setbuf(stdout,NULL); /* No buffering*/
    printf("\n");
}

/**
 * RedirectSTDERR - Redirect stderr to a timestamped error log file
 *
 * Creates a new error log file with the current timestamp and redirects
 * stderr to it. Uses unbuffered output for immediate error logging.
 *
 * Error files are named: LOG_DIR/sent_YYYY-MM-DD-HH:MM:SS.err
 */
static void RedirectSTDERR(void)
{
    FILE *newfp;
    char log_time[100];

    /* Redirect standard input and standard output*/
    strftime(log_time, 100, "%F-%X", localtime(&current_time));
    sprintf(logfile_err, LOG_DIR "sent_%s.err",log_time);
    if(!(newfp = freopen(logfile_err,"a",stderr))) { /* This happens on NT*/
#if !defined(stdout)
        stdout = fopen(logfile_err,"a");
#else
        if((newfp = fopen(logfile_err,"a")))
            *stdout = *newfp;
#endif
    }

    fseek(stderr,0,SEEK_END);
    setbuf(stderr,NULL); /* No buffering*/
}

/**
 * RedirectOutput - Redirect both stdout and stderr to log files
 *
 * Convenience function that calls RedirectSTDOUT() and RedirectSTDERR().
 */
static void RedirectOutput(void)
{
    RedirectSTDOUT();
    RedirectSTDERR();
}


/**
 * CleanupSTDOUT - Close stdout log file and delete if empty
 *
 * Closes the current stdout log file and removes it from disk if it
 * contains no output. Called during log rotation and server shutdown.
 */
static void CleanupSTDOUT(void)
{
    FILE *file;
    int empty;

    fclose(stdout);

    /* See if the files have any output in them*/
    if((file = fopen(logfile_std,"rb"))) {
        empty = (fgetc(file) == EOF) ? 1 : 0;
        fclose(file);
        if(empty)
            remove(logfile_std);
    }
}

/**
 * CleanupSTDERR - Close stderr log file and delete if empty
 *
 * Closes the current stderr error log file and removes it from disk if
 * it contains no output. Called during log rotation and server shutdown.
 */
static void CleanupSTDERR(void)
{
    FILE *file;
    int empty;

    fclose(stderr);

    /* See if the files have any output in them*/
    if((file = fopen(logfile_err,"rb"))) {
        empty = (fgetc(file) == EOF) ? 1 : 0;
        fclose(file);
        if(empty)
            remove(logfile_err);
    }
}


/**
 * CleanupLogs - Clean up both stdout and stderr log files
 *
 * Convenience function that calls CleanupSTDOUT() and CleanupSTDERR().
 * Called during server shutdown.
 */
static void CleanupLogs(void)
{
    CleanupSTDOUT();
    CleanupSTDERR();
}

/**
 * check_logfile - Rotate log files if they exceed max size
 *
 * Checks if either stdout or stderr log files have exceeded the configured
 * max_logfile_size. If so, closes the current file and opens a new one with
 * a fresh timestamp. Called once per game loop iteration.
 */
static void check_logfile(void)
{
    if(ftell(stdout) > game_settings.max_logfile_size) {
        CleanupSTDOUT();
        RedirectSTDOUT();
    }

    if(ftell(stderr) > game_settings.max_logfile_size) {
        CleanupSTDERR();
        RedirectSTDERR();
    }
}

/**
 * parse_options - Parse command-line arguments for the server
 *
 * Parses command-line arguments to configure server startup options.
 * Sets global flags based on the arguments provided.
 *
 * Supported options:
 *   [port]      - Numeric port number (must be > 1024)
 *   -N          - Start with newlock enabled (no new character creation)
 *   -T          - Start in test port mode
 *   -W          - Start with wizlock enabled (immortals only)
 *   -test       - Run integration tests and exit
 *   -test:pat   - Run only tests matching pattern (unit, wnum, all)
 *   -?          - Show usage (returns false to trigger help display)
 *
 * @param argc  Argument count from main()
 * @param argv  Argument vector from main()
 * @return      true on successful parse, false on error or help request
 */
bool parse_options(int argc, char **argv)
{
    int i;

    for(i = 1; i < argc; i++ )
    {
        if( is_number(argv[i]))
        {
            int p = atoi(argv[i]);

            if( p <= 1024 )
            {
                fprintf(stderr, "Port number must be above 1024.");
                return false;
            }

            telnet_port = p;
        }
        else if ( !strncmp(argv[i], "--bootstrap", 11) )
        {
            // Bootstrap options (--bootstrap-auto, --bootstrap-username=, etc.)
            // Already handled in detect_bootstrap_mode(), skip here
            continue;
        }
        else if ( !strncmp(argv[i], "--data-root=", 12) ||
                  !strncmp(argv[i], "--game-root=", 12) )
        {
            // Runtime root overrides are handled before normal initialization.
            continue;
        }
        else if ( argv[i][0] == '-' && (strlen(argv[i]) >= 2) )
        {
            switch( argv[i][1] )
            {
                case 'N':
                    newlock = true;
                    break;

                case 'T':
                    is_test_port = true;
                    break;

                case 'W':
                    wizlock = true;
                    break;

                case 'b':
                    // Bootstrap mode: -bootstrap or --bootstrap-*
                    // These are handled early in main(), before parse_options()
                    // Just skip them here to avoid "Invalid option" error
                    break;

                case 't':
                    // Test mode: -test or -test:pattern
                    if(!strncmp(argv[i], "-test", 5)) {
                        test_mode = true;
                        if(argv[i][5] == ':' && argv[i][6]) {
                            // Extract test pattern: -test:unit
                            strncpy(test_pattern, argv[i] + 6, sizeof(test_pattern) - 1);
                            test_pattern[sizeof(test_pattern) - 1] = '\0';
                        } else {
                            // Default to all tests
                            strcpy(test_pattern, "all");
                        }
                    } else {
                        fprintf(stderr, "Invalid option found.");
                        return false;
                    }
                    break;

                case '?':
                    // Silently return
                    return false;

                default:
                    fprintf(stderr, "Invalid option found.");
                    return false;
            }
        }
        else {
            fprintf(stderr, "Invalid argument found.");
            return false;
        }

    }
    return true;
}

/**
 * detect_test_mode_args - Early detection of test mode from arguments
 *
 * Scans command-line arguments for -test flag before full parsing.
 * This allows test mode to be detected early so logging can be configured
 * appropriately before boot_db() runs.
 *
 * @param argc  Argument count from main()
 * @param argv  Argument vector from main()
 */
static void detect_test_mode_args(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && !strncmp(argv[i], "-test", 5))
        {
            test_mode = true;
            if (argv[i][5] == ':' && argv[i][6])
            {
                strncpy(test_pattern, argv[i] + 6, sizeof(test_pattern) - 1);
                test_pattern[sizeof(test_pattern) - 1] = '\0';
            }
            else
            {
                strcpy(test_pattern, "all");
            }
            return;
        }
    }
}

void set_runtime_game_root(const char *root)
{
    runtime_game_root[0] = '\0';

    if (!root || !root[0]) {
        return;
    }

    snprintf(runtime_game_root, sizeof(runtime_game_root), "%s", root);

    size_t len = strlen(runtime_game_root);
    while (len > 1 && runtime_game_root[len - 1] == '/') {
        runtime_game_root[len - 1] = '\0';
        len--;
    }
}

const char *resolve_game_path(const char *path, char *buffer, size_t buffer_size)
{
    static const char *default_root = "/sentience";
    size_t default_root_len;

    if (!path) {
        return NULL;
    }

    if (!runtime_game_root[0] || !buffer || buffer_size == 0) {
        return path;
    }

    if (path[0] != '/') {
        snprintf(buffer, buffer_size, "%s/%s", runtime_game_root, path);
        return buffer;
    }

    default_root_len = strlen(default_root);
    if (strncmp(path, default_root, default_root_len) != 0) {
        return path;
    }

    if (path[default_root_len] != '/' && path[default_root_len] != '\0') {
        return path;
    }

    snprintf(buffer, buffer_size, "%s%s", runtime_game_root, path + default_root_len);
    return buffer;
}

static const char *detect_data_root_override(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "--data-root=", 12)) {
            return argv[i] + 12;
        }
        if (!strncmp(argv[i], "--game-root=", 12)) {
            return argv[i] + 12;
        }
    }

    const char *env_root = getenv("SENTIENCE_DATA_ROOT");
    if (env_root && env_root[0]) {
        return env_root;
    }

    return NULL;
}


/**
 * main - Server entry point
 *
 * Initializes all server subsystems and enters the main game loop.
 * Performs the following sequence:
 *   1. Initialize logging system
 *   2. Detect test mode from arguments
 *   3. Create global lists (gc_mobiles, gc_objects, conn_players, etc.)
 *   4. Initialize string space and time
 *   5. Load game settings
 *   6. Parse command-line options
 *   7. Initialize network sockets (telnet, TLS, WebSocket)
 *   8. Initialize Redis cache (optional)
 *   9. Load database (boot_db)
 *   10. Run integration tests if in test mode
 *   11. Enter game_loop
 *   12. Clean up and shutdown on exit
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return      0 on normal termination, non-zero on error or test failure
 */
int main(int argc, char **argv)
{
    const char *data_root_override = detect_data_root_override(argc, argv);
    char zlog_conf_buf[MAX_INPUT_LENGTH];
    const char *zlog_conf_path;

    runtime_game_root[0] = '\0';

    detect_bootstrap_mode(argc, argv);

    if (data_root_override && data_root_override[0]) {
        set_runtime_game_root(data_root_override);

        if (chdir(data_root_override) != 0) {
            fprintf(stderr, "Failed to use data root '%s': %s\n", data_root_override, strerror(errno));
            return 1;
        }
        fprintf(stderr, "Using data root: %s\n", runtime_game_root);
    } else if (bootstrap_mode && bootstrap_root && bootstrap_root[0]) {
        set_runtime_game_root(bootstrap_root);
    }

    zlog_conf_path = resolve_game_path(ZLOG_CONF, zlog_conf_buf, sizeof(zlog_conf_buf));

    int rc = log_init(zlog_conf_path);
    if (rc && bootstrap_mode) {
        const char *bootstrap_fallback_conf = "bootstrap/bootstrap_data/system/zlog.conf";
        rc = log_init(bootstrap_fallback_conf);
        if (!rc) {
            fprintf(stderr, "log_init fallback succeeded using %s\n", bootstrap_fallback_conf);
        }
    }
    if (rc) {
        if (bootstrap_mode) {
            fprintf(stderr, "log_init failed (rc=%d), continuing bootstrap with stderr logging only\n", rc);
        } else {
            fprintf(stderr, "log_init failed\n");
            return -1;
        }
    }

    detect_test_mode_args(argc, argv);
    if (test_mode) {
        log_set_unit_test_only(true);
    }

    /* Check for bootstrap mode */
    if (bootstrap_mode) {
        /* Run bootstrap - creates minimal data files and first account */
        if (run_bootstrap() != 0) {
            fprintf(stderr, "Bootstrap failed. Exiting.\n");
            return 1;
        }
        /* Bootstrap successful - exit now (user should run again normally) */
        return 0;
    }

    struct timeval now_time;
    int control_telnet = 0;
    int control_tls = 0;
    int control_websocket = 0;
    ITERATOR iter;
    void *data;
    static GAME_SETTINGS_DATA game_settings_zero;
    signal(SIGPIPE, SIG_IGN);

    /*
     * Memory debugging if needed.
     */
#if defined(MALLOC_DEBUG)
    malloc_debug(2);
#endif

    gc_mobiles = list_create(false);
    if(!gc_mobiles)
    {
        perror("Could not create 'gc_mobiles'");
        exit(1);
    }

    gc_objects  = list_create(false);
    if(!gc_objects)
    {
        perror("Could not create 'gc_objects'");
        exit(1);
    }

    gc_rooms = list_create(false);
    if(!gc_rooms)
    {
        perror("Could not create 'gc_rooms'");
        exit(1);
    }

    gc_tokens = list_create(false);
    if(!gc_tokens)
    {
        perror("Could not create 'gc_tokens'");
        exit(1);
    }

    conn_players = list_create(false);
    if(!conn_players) {
        perror("Could not create 'conn_players'");
        exit(1);
    }

    conn_immortals = list_create(false);
    if(!conn_immortals) {
        perror("Could not create 'conn_immortals'");
        exit(1);
    }

    conn_online = list_create(false);
    if(!conn_online) {
        perror("Could not create 'conn_online'");
        exit(1);
    }
    loaded_areas = list_create(false);
    if(!loaded_areas) {
        perror("Could not create 'loaded_areas'");
        exit(1);
    }
    loaded_wilds = list_create(false);
    if(!loaded_wilds) {
        perror("Could not create 'loaded_wilds'");
        exit(1);
    }
    list_churches = list_create(false);
    if(!list_churches) {
        perror("Could not create 'list_churches'");
        exit(1);
    }
    persist_mobs = list_create(false);
    if(!persist_mobs) {
        perror("Could not create 'persist_mobs'");
        exit(1);
    }
    persist_objs = list_create(false);
    if(!persist_objs) {
        perror("Could not create 'persist_objs'");
        exit(1);
    }
    persist_rooms = list_create(false);
    if(!persist_rooms) {
        perror("Could not create 'persist_rooms'");
        exit(1);
    }
    loaded_chars = list_create(false);
    if(!loaded_chars) {
        perror("Could not create 'loaded_chars'");
        exit(1);
    }

    loaded_accounts = list_create(false);
    if(!loaded_accounts) {
        perror("Could not create 'loaded_accounts'");
        exit(1);
    }
// Temporarily disabling for reconnect crash.
/*
    loaded_players = list_create(false);
    if(!loaded_players) {
        perror("Could not create 'loaded_players'");
        exit(1);
    }
*/
    loaded_objects = list_create(false);
    if(!loaded_objects) {
        perror("Could not create 'loaded_objects'");
        exit(1);
    }

    loaded_groups = list_create(false);
    if(!loaded_groups) {
        perror("Could not create 'loaded_groups'");
        exit(1);
    }
    loaded_obj_hash_init();

    init_string_space();

    /*
     * Init time.
     */
    gettimeofday(&now_time, NULL);
    current_time = (time_t) now_time.tv_sec;
    strcpy(str_boot_time, ctime(&current_time));

    /*
     * Reserve one channel for our use.
     */
    {
    char null_file_buf[MAX_INPUT_LENGTH];
    const char *null_file = resolve_game_path(NULL_FILE, null_file_buf, sizeof(null_file_buf));

    if ((fpReserve = fopen(null_file, "r")) == NULL)
    {
    perror(null_file);
    exit(1);
    }
    }

    game_settings = game_settings_zero;
    if (game_settings_read()==1) exit(1);
    plogf(LOG_INIT, "Global game settings loaded.");

    // Install crash handler for stack traces and core dumps
    log_install_crash_handler(game_settings.crash_dump_dir[0] ?
        game_settings.crash_dump_dir : NULL);

    /*
     * Get the port number.
     */
    if (game_settings.telnet_port)
        telnet_port = game_settings.telnet_port;
    if (game_settings.tls_port)
        tls_port = game_settings.tls_port;
    if (game_settings.websocket_tls_port)
        websocket_port = game_settings.websocket_tls_port;
    if (game_settings.testport || game_settings.dev_server)
        is_test_port = true;

    if (game_settings.dev_server)
        {
            newlock = true;
            wizlock = true;
        }

    if( !parse_options(argc, argv) )
    {
        fprintf(stderr, "Usage: %s [port #] [-NTW] [-test[:pattern]] [--data-root=PATH]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "\tport #\t\tListening port for the server (>1024).  Default is 9000.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "\t-N\t\tStart up with newlock active.\n");
        fprintf(stderr, "\t-T\t\tStart up in Test Port mode.\n");
        fprintf(stderr, "\t-W\t\tStart up with wizlock active.\n");
        fprintf(stderr, "\t-test\t\tRun integration tests and exit.\n");
        fprintf(stderr, "\t-test:unit\tRun only unit tests.\n");
        fprintf(stderr, "\t-test:wnum\tRun only widevnum tests.\n");
        fprintf(stderr, "\t-test:all\tRun all tests (default).\n");
        fprintf(stderr, "\t--data-root=PATH\tSet runtime root for relative-path access (experimental).\n");
        fprintf(stderr, "\t--game-root=PATH\tAlias for --data-root.\n");
        fprintf(stderr, "\tSENTIENCE_DATA_ROOT\tEnvironment fallback for relative-path root override.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "\t-?\t\tShow this screen.\n");
        fprintf(stderr, "\n");
        exit(1);
    }
#if 0
    if (argc > 1)
    {
        if (!is_number(argv[1]))
        {
            fprintf(stderr, "Usage: %s [port #] [-T]\n", argv[0]);
            exit(1);
        }
        else if ((port = atoi(argv[1])) <= 1024)
        {
            fprintf(stderr, "Port number must be above 1024.\n");
            exit(1);
        }
    }
#endif

    //if(port == PORT_TEST) newlock = true;	/* The alpha port is initially set to newlock*/
    //if(port == PORT_TEST) wizlock = true;	/* Newlock/Wizlock all ports for now */

    //if(port == PORT_TEST || port == PORT_ALPHA || port == PORT_SYN) is_test_port = true;

    if (!test_mode) {
        RedirectOutput();
    }

    /* Vizz - load up our list of UIDs. Without this, we cannot assign unique UIDs to things */
    //gconfig = gconfig_zero;
    //if (gconfig_read()==1) exit(1);



    /*
     * Run the game.
     */
    if ((!game_settings.enable_telnet && game_settings.telnet_port) && (!game_settings.enable_tls && game_settings.tls_port && !IS_NULLSTR(game_settings.ssl_cert_path) && !IS_NULLSTR(game_settings.ssl_key_path)))
    {
        fprintf(stderr, "No available connection options. Please set enable_telnet and telnet_port, and/or enable_tls and tls_port, along with ssl_cert_path and ssl_key_path in the game_settings table.\n");
        exit(1);
    }

    // Skip network and cache infrastructure in test mode - tests only need game data
    if (!test_mode)
    {
        if (!channel_service_init()) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Channel service unavailable - using existing direct channel flow");
        }

        if (game_settings.enable_telnet)
        {
            control_telnet = init_socket(telnet_port);
            log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Telnet socket bound to port %d.", telnet_port);
        }
        if (game_settings.enable_tls && game_settings.tls_port)
        {
            control_tls = init_tls_socket(tls_port);
            log_message_f(LOG_LEVEL_INFO, LOG_INIT, "TLS socket bound to port %d.", tls_port);
        }
        if (game_settings.enable_websocket_tls && game_settings.websocket_tls_port)
        {
            control_websocket = init_tls_socket(websocket_port);
            log_message_f(LOG_LEVEL_INFO, LOG_INIT, "WebSocket TLS socket bound to port %d.", websocket_port);
        }
        log_message(LOG_LEVEL_INFO, LOG_INIT, "Socket initialization complete");

        // Initialize Redis cache early (optional - game works without it)
        // This must happen before boot_db() so areas can warm the cache during boot
        if (!redis_init()) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Redis cache unavailable - character list display will be slower");
        }
    }

    boot_db();

    if (!test_mode)
    {
        // Post-boot Redis cache warming (only if Redis is available)
        if (redis_is_available()) {
            // Warm cache with recently active characters (Phase 1 - currently no-op)
            // Future: This will pre-cache character.json files in Phase 3
            redis_warm_cache(100);

            // Warm cache with loaded persist entities (Phase 2)
            json_persist_warm_cache();

            // Start background persist worker (Phase 2 - async writes through Redis)
            if (!json_persist_worker_start()) {
                log_message(LOG_LEVEL_WARN, LOG_WARN, "Persist worker failed to start - using synchronous writes");
            }
        }

        // Initialize async cache system for background dump/load operations
        if (!async_cache_init()) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Async cache system failed to initialize");
        }

        if (!wilds_wildgen_init()) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Wildgen worker failed to initialize");
        }

        if (!wilderness_storage_init()) {
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Wilderness storage system failed to initialize");
        }

        log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Sentience is up on port %d.", telnet_port);
    }
    
    // Check if we're running in test mode
    if (test_mode) {
#ifdef BUILD_TESTS
        int test_result = run_integration_tests(test_pattern);
        log_message_f(LOG_LEVEL_INFO, LOG_INIT, "Integration tests completed with result: %d", test_result);
        exit(test_result);
#else
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Test mode requested but MUD was not compiled with BUILD_TESTS");
        exit(1);
#endif
    }
    
    game_loop(control_telnet, control_tls, control_websocket);

    wilderness_storage_shutdown();

    // Stop wilderness wildgen worker before tearing down world data lists.
    wilds_wildgen_shutdown();

    groups_clear_all();
    list_destroy(conn_players);
    list_destroy(conn_immortals);
    list_destroy(conn_online);
    list_destroy(loaded_chars);
    // Temporarily disabling for reconnect crash.
    //list_destroy(loaded_players);
    list_destroy(loaded_objects);
    list_destroy(loaded_groups);
    list_destroy(persist_mobs);
    list_destroy(persist_objs);
    list_destroy(persist_rooms);
    list_destroy(gc_mobiles);
    list_destroy(gc_objects);
    list_destroy(gc_rooms);
    list_destroy(gc_tokens);
    iterator_start(&iter, loaded_areas);
    while((data = iterator_nextdata(&iter)))
        free_mem(data, sizeof(LLIST_AREA_DATA));
    iterator_stop(&iter);
    list_destroy(loaded_areas);
    iterator_start(&iter, loaded_wilds);
    while((data = iterator_nextdata(&iter)))
        free_mem(data, sizeof(LLIST_WILDS_DATA));
    iterator_stop(&iter);
    list_destroy(loaded_wilds);
    list_destroy(list_churches);
    if (game_settings.enable_telnet)
            close (control_telnet);
        if (game_settings.enable_tls)
        close(control_tls);


    save_commands();
    list_destroy(commands_list);

    if (gconfig_write()==1)
    {
        perrf(LOG_INIT, "comm.c, main(): Failed to write our gconfig.rc file!");
        perrf(LOG_INIT, "                Current UID's are:");
        perrf(LOG_INIT, "                                   NextAreaUID:	%ld", gconfig.next_area_uid);
        perrf(LOG_INIT, "                                   NextWildsUID:	%ld", gconfig.next_wilds_uid);
    }

    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!
    // @@@@FIXME: FREE EVERYTHING!!!!

    /*
     * That's all, folks.
     */
    log_message(LOG_LEVEL_INFO, LOG_INFO, "Normal termination of game.");

    // Shutdown async cache system (wait for pending operations)
    async_cache_shutdown();

    // Stop background persist worker (flushes dirty queue to disk)
    json_persist_worker_stop();

    // Shutdown channel service transport layer
    channel_service_shutdown();

    // Shutdown Redis connection
    redis_shutdown();

    log_shutdown();
    CleanupLogs();

    exit(0);
    return 0;
}

/**
 * init_socket - Initialize a TCP listening socket
 *
 * Creates and configures a TCP socket for accepting telnet connections.
 * Sets SO_REUSEADDR to allow quick server restarts and SO_DONTLINGER
 * to avoid lingering on close.
 *
 * @param port  Port number to bind to (must be > 1024)
 * @return      File descriptor of the listening socket
 *
 * @note Exits the process on failure (socket/bind/listen errors are fatal)
 */
int init_socket(int port)
{
    static struct sockaddr_in sa_zero;
    struct sockaddr_in sa;
    int x = 1;
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    perror("Init_socket: socket");
    exit(1);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
    (char *) &x, sizeof(x)) < 0)
    {
    perror("Init_socket: SO_REUSEADDR");
    close(fd);
    exit(1);
    }

#if defined(SO_DONTLINGER) && !defined(SYSV)
    {
    struct	linger	ld;

    ld.l_onoff  = 1;
    ld.l_linger = 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_DONTLINGER,
    (char *) &ld, sizeof(ld)) < 0)
    {
        perror("Init_socket: SO_DONTLINGER");
        close(fd);
        exit(1);
    }
    }
#endif

    sa		    = sa_zero;
    sa.sin_family   = AF_INET;
    sa.sin_port	    = htons(port);

    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0)
    {
    perror("Init socket: bind");
    close(fd);
    exit(1);
    }


    if (listen(fd, 3) < 0)
    {
    perror("Init socket: listen");
    close(fd);
    exit(1);
    }

    return fd;
}

/**
 * init_tls_socket - Initialize a TLS listening socket
 *
 * Creates and configures a TCP socket for TLS connections. Also initializes
 * the OpenSSL library and creates the global SSL context with configured
 * certificates.
 *
 * @param port  Port number to bind to (must be > 1024)
 * @return      File descriptor of the listening socket
 *
 * @note Exits the process on failure. SSL context is stored in global 'ctx'.
 */
int init_tls_socket(int port)
{
    static struct sockaddr_in sa_zero;
    struct sockaddr_in sa;
    int x = 1;
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    perror("Init_socket: socket");
    exit(1);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
    (char *) &x, sizeof(x)) < 0)
    {
    perror("Init_socket: SO_REUSEADDR");
    close(fd);
    exit(1);
    }

#if defined(SO_DONTLINGER) && !defined(SYSV)
    {
    struct	linger	ld;

    ld.l_onoff  = 1;
    ld.l_linger = 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_DONTLINGER,
    (char *) &ld, sizeof(ld)) < 0)
    {
        perror("Init_socket: SO_DONTLINGER");
        close(fd);
        exit(1);
    }
    }
#endif

    sa		    = sa_zero;
    sa.sin_family   = AF_INET;
    sa.sin_port	    = htons(port);

    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0)
    {
    perror("Init socket: bind");
    close(fd);
    exit(1);
    }


    if (listen(fd, 3) < 0)
    {
    perror("Init socket: listen");
    close(fd);
    exit(1);
    }

    // Initialize SSL library
    init_openssl_library();

    // Create SSL context
    ctx = create_context();

    // Configure SSL context
    configure_context(ctx);

    return fd;
}

/**
 * game_loop - Main server loop
 *
 * The heart of the MUD server. Runs continuously until merc_down is set.
 * Each iteration performs:
 *   1. Select on all file descriptors (control sockets + client descriptors)
 *   2. Accept new connections on control sockets
 *   3. Process pending TLS/WebSocket handshakes with timeout
 *   4. Kick out connections with exceptions
 *   5. Read input from all ready descriptors
 *   6. Process commands from input buffers
 *   7. Run update_handler() for game world updates
 *   8. Write output to all ready descriptors
 *   9. Check for idle/timeout connections
 *   10. Sleep to maintain PULSE_PER_SECOND timing
 *   11. Run garbage collection
 *   12. Check log file rotation
 *
 * @param control_telnet     File descriptor for telnet listening socket
 * @param control_tls        File descriptor for TLS listening socket
 * @param control_websocket  File descriptor for WebSocket listening socket
 */
void game_loop(int control_telnet, int control_tls, int control_websocket)
{
    static struct timeval null_time;
    struct timeval last_time;
    struct timeval curr_time;
    DESCRIPTOR_DATA *d, *d_next;

    signal(SIGPIPE, SIG_IGN);
    gettimeofday(&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;

    /* Main loop */
    while (!merc_down)
    {
        fd_set in_set;
        fd_set out_set;
        fd_set exc_set;
        int maxdesc = 0;

#if defined(MALLOC_DEBUG)
        if (malloc_verify() != 1)
            abort();
#endif

        /*
         * Poll all active descriptors.
         */
        FD_ZERO(&in_set);
        FD_ZERO(&out_set);
        FD_ZERO(&exc_set);
        
        // Set up listening sockets
        if (control_telnet != -1 && game_settings.enable_telnet)
        {
            FD_SET(control_telnet, &in_set);
            maxdesc = control_telnet;
        }
        
        if (control_tls != -1 && game_settings.enable_tls)
        {
            FD_SET(control_tls, &in_set);
            maxdesc = UMAX(maxdesc, control_tls);
        }

        if (control_websocket != -1 && game_settings.enable_websocket_tls)
        {
            FD_SET(control_websocket, &in_set);
            maxdesc = UMAX(maxdesc, control_websocket);
        }

        // Process descriptor lists
        for (d = descriptor_list; d; d = d->next)
        {
            FD_SET(d->descriptor, &in_set);
            FD_SET(d->descriptor, &exc_set);
            FD_SET(d->descriptor, &out_set);
        
            
            maxdesc = UMAX(maxdesc, d->descriptor);
        }

        if (select(maxdesc+1, &in_set, &out_set, &exc_set, &null_time) < 0)
        {
            switch (errno)
            {
                case EBADF:
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Invalid file descriptor passed to Select()");
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                case EINTR:
log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "A non-blocked signal was caught.");
                    break;
                case EINVAL:
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Negative 'n' descriptor passed to Select()");
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                case ENOMEM:
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Select() was unable to allocate memory for internal tables.");
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                default:
                    log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Unknown error.");
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
            }
        }

        /*
         * New connections?
         */
        if (control_telnet != -1 && FD_ISSET(control_telnet, &in_set))
            init_descriptor(control_telnet, control_telnet, control_tls, control_websocket);

        if (control_tls != -1 && FD_ISSET(control_tls, &in_set))
            init_descriptor(control_tls, control_telnet, control_tls, control_websocket);

        if (control_websocket != -1 && FD_ISSET(control_websocket, &in_set))
            init_descriptor(control_websocket, control_telnet, control_tls, control_websocket);    

        /*
         * Process outstanding handshakes first (TLS, WebSocket, etc.)
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;

            // Skip descriptors without connections or that aren't in handshake mode
            if (!d->conn || !d->conn->handshake_in_progress)
                continue;

            // Check for handshake timeouts (5 seconds to prevent slowloris attacks)
            if (current_time - d->conn->last_activity > 5) {
                log_message_f(LOG_LEVEL_WARN, LOG_WARN, "Closing stalled %s handshake connection",
                           connection_get_protocol_name(d->conn));
                close_socket(d);
                continue;
            }

            // Process handshakes ready for activity
            if (FD_ISSET(d->descriptor, &in_set) || FD_ISSET(d->descriptor, &out_set)) {
                if (connection_process_handshake(d->conn)) {
                    // Handshake completed successfully
                    d->conn->last_activity = current_time;

                    // Sync legacy field
                    d->tls_handshake_in_progress = d->conn->handshake_in_progress;

                    // For WebSocket connections, send greeting now that handshake is complete
                    if (d->conn->type == CONN_TYPE_WEBSOCKET_TLS) {
                        // Protocol negotiation (GMCP for WebSocket)
                        if (d->proto) {
                            protocol_negotiate(d->proto);
                        }

                        // Send greeting
                        if (help_greeting[0] == '.')
                            write_to_buffer(d, help_greeting + 1, 0);
                        else
                            write_to_buffer(d, help_greeting, 0);

                        if (!IS_NULLSTR(game_settings.login_string))
                        {
                            write_to_buffer(d, game_settings.login_string, 0);
                            write_to_buffer(d, "\n\r", 0);
                        }
                        else
                        {
                            write_to_buffer(d, "By what name do you wish to be known? ", 0);
                        }
                    }

                    // Log successful handshake when debugging
                    if (game_settings.dev_server)
                        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "%s handshake completed successfully",
                                   connection_get_protocol_name(d->conn));
                } else {
                    // Check if handshake failed (vs still in progress)
                    if (d->conn->state != CONN_STATE_CONNECTING) {
                        log_message_f(LOG_LEVEL_WARN, LOG_WARN, "%s handshake failed",
                                   connection_get_protocol_name(d->conn));
                        close_socket(d);
                        continue;
                    }
                }
            }
        }

        /*
         * Kick out the freaky folks.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            if (FD_ISSET(d->descriptor, &exc_set))
            {
                FD_CLR(d->descriptor, &in_set);
                FD_CLR(d->descriptor, &out_set);
                if (d->character && d->connected == CON_PLAYING)
                {
                    save_char_obj(d->character);
                }
                d->outtop = 0;
                close_socket(d);
            }
        }

        /*
         * Process input.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            d->fcommand = false;

            // Don't process input for connections in TLS handshake
            if (d->ssl && d->tls_handshake_in_progress)
                continue;

            if (FD_ISSET(d->descriptor, &in_set))
            {
                if (d->character != NULL)
                    d->character->timer = 0;

                if (!read_from_descriptor(d))
                {
                    FD_CLR(d->descriptor, &out_set);

                    if (d->character != NULL && d->connected == CON_PLAYING)
                        save_char_obj(d->character);

                    d->outtop = 0;
                    close_socket(d);
                    continue;
                }

                d->muted = 0;
            }

            // Process command queues
            if (d->character != NULL && d->character->wait > 0)
            {
                --d->character->wait;
                continue;
            }

            /* decrease timers for things like casting, brew, paroxysm etc */
            if (d->character != NULL)
                update_pc_timers(d->character);

            read_from_buffer(d);
            if (d->incomm[0] != '\0')
            {
                d->fcommand = true;
                if (d->pProtocol != NULL)
                    d->pProtocol->WriteOOB = 0;
                stop_idling(d->character);

                /* OLC */
                if (d->showstr_point)
                    show_string(d, d->incomm);
                else if (d->pString) {
                    string_add(d->character, d->incomm);
                } else
                    switch (d->connected)
                    {
                        case CON_PLAYING:
                            if (!run_olc_editor(d))
                                substitute_alias(d, d->incomm);
                            break;
                        default:
                            nanny(d, d->incomm);
                            break;
                    }

                d->incomm[0] = '\0';
            }
        }

        /*
         * Autonomous game motion.
         */
        update_handler();

        /*
         * Process output.
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            
            // Skip output processing for connections in TLS handshake
            if (d->ssl && d->tls_handshake_in_progress)
                continue;

            if ((d->fcommand || d->outtop > 0) && FD_ISSET(d->descriptor, &out_set))
            {
                if (!process_output(d, true))
                {
                    if (d->character != NULL && d->connected == CON_PLAYING) {
                        save_char_obj(d->character);
                    }
                    d->outtop = 0;
                    close_socket(d);
                }
            }
        }

        /*
         * Check for idle/timeout connections
         */
        for (d = descriptor_list; d != NULL; d = d_next) {
            d_next = d->next;
            
            // Close connections with stalled handshakes (5 seconds to prevent slowloris)
            if (d->conn && d->conn->handshake_in_progress &&
                current_time - d->conn->last_activity > 5) {
                log_message_f(LOG_LEVEL_WARN, LOG_WARN, "Closing stalled %s handshake connection",
                           connection_get_protocol_name(d->conn));
                close_socket(d);
                continue;
            }
            
            // Only timeout normal connections if not fully logged in
            if ((d->connected == CON_GET_ACCOUNT_NAME || d->connected == CON_GET_OLD_PASSWORD) && 
                current_time - d->last_activity > 120 && 
                !d->healthcheck) {
                log_message(LOG_LEVEL_INFO, LOG_INFO, "Closing idle connection (timeout).");
                close_socket(d);
            }
        }

        /*
         * Synchronize to a clock.
         * Sleep(last_time + 1/PULSE_PER_SECOND - now).
         * Careful here of signed versus unsigned arithmetic.
         */
        {
            gettimeofday(&curr_time, NULL);
            long secDelta = ((int) last_time.tv_sec) - ((int) curr_time.tv_sec);
            long usecDelta = ((int) last_time.tv_usec) - ((int) curr_time.tv_usec)
                + 1000000 / PULSE_PER_SECOND;

            while (usecDelta < 0)
            {
                usecDelta += 1000000;
                secDelta  -= 1;
            }

            while (usecDelta >= 1000000)
            {
                usecDelta -= 1000000;
                secDelta  += 1;
            }

            if (secDelta > 0 || (secDelta == 0 && usecDelta > 0))
            {
                struct timeval stall_time;
                stall_time.tv_usec = usecDelta;
                stall_time.tv_sec  = secDelta;
                
        if (select(0, NULL, NULL, NULL, &stall_time) < 0)
        {
            switch (errno)
            {
            case EBADF:
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Invalid file descriptor passed to Select()");
                perror("Game_loop: select: stall");
                exit(1);
                break;
            case EINTR:	log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "A non-blocked signal was caught.");
                break;
            case EINVAL:
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Negative 'n' descriptor passed to Select()");
                perror("Game_loop: select: stall");
                exit(1);
                break;
            case ENOMEM:
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Select() was unable to allocate memory for internal tables.");
                perror("Game_loop: select: stall");
                exit(1);
                break;
            default:
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "Unknown error.");
                perror("Game_loop: select: stall");
                exit(1);
                break;
            }
/*	    	perror("Game_loop: select: stall");*/
/*		    exit(1);*/
        }
            }
        }

    // Garbage collect
    process_garbage_collection();

    /* Check to see if the logfiles have overflowed*/
    check_logfile();

    gettimeofday(&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;
    }
}


/**
 * init_descriptor - Accept a new connection and create descriptor
 *
 * Accepts a new connection from one of the listening sockets and creates
 * a DESCRIPTOR_DATA for it. Determines connection type (TCP, TLS, WebSocket)
 * based on which control socket triggered. Sets up:
 *   - Connection abstraction layer
 *   - Protocol negotiation layer
 *   - Legacy SSL fields for backwards compatibility
 *   - Host name resolution
 *   - Ban checking
 *   - Greeting display (deferred for WebSocket until handshake completes)
 *
 * @param control            The control socket that received the connection
 * @param control_telnet     Telnet control socket (for comparison)
 * @param control_tls        TLS control socket (for comparison)
 * @param control_websocket  WebSocket control socket (for comparison)
 */
void init_descriptor(int control, int control_telnet, int control_tls, int control_websocket)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *dnew = NULL;
    struct sockaddr_in sock;
    struct hostent *from;
    int desc;
    socklen_t size;
    connection_t *conn;

    size = sizeof(sock);
    getsockname(control, (struct sockaddr *) &sock, &size);
    if ((desc = accept(control, (struct sockaddr *) &sock, &size)) < 0)
    {
        perror("New_descriptor: accept");
        return;
    }

    // Create descriptor
    dnew = new_descriptor();
    dnew->last_activity = current_time;
    dnew->healthcheck = false;
    dnew->descriptor = desc;

    // Create connection object based on which control socket accepted it
    if (control == control_websocket) {
        conn = connection_websocket_tls_create(desc, dnew);
    } else if (control == control_tls) {
        conn = connection_tls_create(desc, dnew);
    } else {
        conn = connection_tcp_create(desc, dnew);
    }

    if (!conn) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "init_descriptor: Failed to create connection object");
        close(desc);
        free_descriptor(dnew);
        return;
    }

    // Store connection in descriptor
    dnew->conn = conn;

    // Create protocol layer for this connection
    dnew->proto = protocol_layer_create_for_connection(conn, dnew);
    if (!dnew->proto) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "init_descriptor: Failed to create protocol layer");
        connection_close(conn);
        connection_free(conn);
        close(desc);
        free_descriptor(dnew);
        return;
    }

    // Maintain legacy fields for backward compatibility during migration
    if (conn->type == CONN_TYPE_TLS || conn->type == CONN_TYPE_WEBSOCKET_TLS) {
        dnew->ssl = (SSL*)conn->proto_data;
        dnew->tls_handshake_in_progress = conn->handshake_in_progress;
    } else {
        dnew->ssl = NULL;
        dnew->tls_handshake_in_progress = false;
    }
    dnew->connected = CON_GET_ACCOUNT_NAME;
    dnew->showstr_head = NULL;
    dnew->showstr_point = NULL;
    dnew->outsize = 2000;
    dnew->pEdit = NULL;
    dnew->pString = NULL;
    dnew->editor = 0;
    dnew->outbuf = alloc_mem(dnew->outsize);
    dnew->pProtocol = ProtocolCreate();

    size = sizeof(sock);
    if (getpeername(desc, (struct sockaddr *) &sock, &size) < 0)
    {
        perror("New_descriptor: getpeername");
        dnew->host = str_dup("(unknown)");
    }
    else
    {
        int addr;
        addr = ntohl(sock.sin_addr.s_addr);
        sprintf(buf, "%d.%d.%d.%d",
            (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
            (addr >>  8) & 0xFF, (addr      ) & 0xFF
        );
        if (game_settings.dev_server || check_ban(buf, BAN_ALL)) {
            log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Sock.sinaddr:  %s", buf);
        }
        from = gethostbyaddr((char *) &sock.sin_addr,
            sizeof(sock.sin_addr), AF_INET);
        dnew->host = str_dup(from ? from->h_name : buf);
    }

    if (check_ban(dnew->host, BAN_ALL))
    {
        char banmsg[MIL];
        sprintf(banmsg, "Your site has been banned from %s\n\r", game_settings.game_name);
        write_to_descriptor_2(dnew, banmsg, 0);
        close(desc);
        free_descriptor(dnew);
        return;
    }

    dnew->next = descriptor_list;
    descriptor_list = dnew;

    // For WebSocket connections, defer greeting until after WebSocket handshake completes
    // For telnet/TLS, send greeting immediately
    if (conn->type != CONN_TYPE_WEBSOCKET_TLS) {
        ProtocolNegotiate(dnew);

        // Negotiate protocol capabilities through new protocol layer
        if (dnew->proto) {
            protocol_negotiate(dnew->proto);
        }

        write_to_buffer(dnew, compress_will, 0);

        if (help_greeting[0] == '.')
            write_to_buffer(dnew, help_greeting + 1, 0);
        else
            write_to_buffer(dnew, help_greeting, 0);

        if (conn->type == CONN_TYPE_TCP && game_settings.enable_tls && game_settings.enable_insecure_warning && game_settings.insecure_warning_msg != NULL)
        {
            sprintf(buf, "{R%s{x\n\r{XIf your client supports it, encrypted connection is available on port %d\n\r", game_settings.insecure_warning_msg, game_settings.tls_port);
            write_to_buffer(dnew, buf, 0);
        }

        if (!IS_NULLSTR(game_settings.login_string))
        {
            write_to_buffer(dnew, game_settings.login_string, 0);
            write_to_buffer(dnew, "\n\r", 0);
        }
        else
        {
            write_to_buffer(dnew, "By what name do you wish to be known? ", 0);
        }
    }
}


/**
 * close_socket - Close a connection and clean up descriptor
 *
 * Performs a clean shutdown of a connection:
 *   - Flushes any pending output
 *   - Logs disconnection for playing characters
 *   - Notifies room of link loss if playing
 *   - Frees character data (or leaves link-dead for reconnect)
 *   - Removes from descriptor_list
 *   - Ends MCCP compression if active
 *   - Destroys protocol handlers
 *   - Closes connection (TLS shutdown, socket close)
 *   - Decrements account refcount and frees if zero
 *   - Frees the descriptor structure
 *
 * @param dclose  The descriptor to close
 */
void close_socket(DESCRIPTOR_DATA *dclose)
{
    CHAR_DATA *ch;

    if (dclose->outtop > 0)
        process_output(dclose, false);

    if ((ch = dclose->character) != NULL)
    {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Closing link to %s.", ch->name);
        /* cut down on wiznet spam when rebooting */
        if (dclose->connected == CON_PLAYING && !merc_down)
        {
            if (ch->invis_level < STAFF_IMMORTAL)
                act("$n has lost $s link.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            wiznet("$N has lost $S link.",ch,NULL,WIZ_LINKS,0,0);

            ch->desc = NULL;
        }
        else if (dclose->reconnecting && dclose->reconnect_ch) 
        {
            // Disconnected during reconnection process - don't free the reconnect character
            // It remains link-dead and available for future reconnect attempts
            if (dclose->reconnect_ch->desc == dclose) {
                dclose->reconnect_ch->desc = NULL;
            }
            
            // Still need to free the temporary character
            free_char(dclose->character);
        }
        else
        {
            free_char(dclose->original ? dclose->original : dclose->character);
        }
    }

    if (d_next == dclose)
        d_next = d_next->next;

    if (dclose == descriptor_list)
    {
        descriptor_list = descriptor_list->next;
    }
    else
    {
        DESCRIPTOR_DATA *d;

        for (d = descriptor_list; d && d->next != dclose; d = d->next)
            ;
        if (d != NULL)
            d->next = dclose->next;
        else
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Close_socket: dclose not found.");
    }

    if (dclose->out_compress) {
        deflateEnd(dclose->out_compress);
        free_mem(dclose->out_compress_buf, COMPRESS_BUF_SIZE);
        free_mem(dclose->out_compress, sizeof(z_stream));
    }

    ProtocolDestroy(dclose->pProtocol);

    // Free protocol layer
    if (dclose->proto) {
        protocol_layer_free(dclose->proto);
        dclose->proto = NULL;
    }

    // Close and free connection using abstraction layer
    if (dclose->conn) {
        connection_close(dclose->conn);
        connection_free(dclose->conn);
        dclose->conn = NULL;

        // Clear legacy fields
        dclose->ssl = NULL;
        dclose->tls_handshake_in_progress = false;
    } else {
        // Fallback for descriptors created before connection abstraction
        shutdown(dclose->descriptor, SHUT_RDWR);
        close(dclose->descriptor);
    }

    if (dclose->account) {
        ACCOUNT_DATA *account = dclose->account;
        bool account_in_use_elsewhere = false;
        DESCRIPTOR_DATA *d;

        for (d = descriptor_list; d; d = d->next) {
            if (d != dclose && d->account == account) {
                account_in_use_elsewhere = true;
                break;
            }
        }

        if (account->refcount > 0)
            account->refcount--;

        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "close_socket: Account %s refcount decreased to %d",
                   account->username, account->refcount);

        if (account->refcount <= 0) {
            if (account_in_use_elsewhere) {
                log_message_f(LOG_LEVEL_BUG, LOG_ERROR,
                    "close_socket: Account %s refcount <= 0 but still referenced by another descriptor; deferring free",
                    account->username);
                account->refcount = 1;
            } else {
                log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "close_socket: Freeing account %s (refcount %d)",
                           account->username, account->refcount);
                list_remlink(loaded_accounts, account, false);
                free_account(account);
            }
        }
        dclose->account = NULL;
    }

    free_descriptor(dclose);
    return;
}


/**
 * read_from_descriptor - Read raw data from a connection
 *
 * Reads available data from the connection into the input buffer.
 * Uses the connection abstraction layer (connection_read) for TLS/WebSocket
 * transparency. Also handles:
 *   - Input overflow detection
 *   - Health check requests (responds with "OK" and marks for close)
 *   - Protocol processing via ProtocolInput (telnet negotiation, etc.)
 *
 * @param d  The descriptor to read from
 * @return   true if read succeeded (may have no data), false on error/disconnect
 */
bool read_from_descriptor(DESCRIPTOR_DATA *d)
{
    int iStart;

    d->last_activity = current_time;

    static char read_buf[MAX_PROTOCOL_BUFFER];
    read_buf[0] = '\0';

    if (d->incomm[0] != '\0')
        return true;

    iStart = 0;
    if (strlen(d->inbuf) >= sizeof(d->inbuf) - 10)
    {
        log_message_f(LOG_LEVEL_WARN, LOG_WARN, "%s input overflow!", d->host);
        write_to_descriptor(d, "\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
        return false;
    }

    for (;;)
    {
        int nRead = 0;
        int bytes_read;

        // Use connection abstraction
        if (d->conn) {
            if (!connection_read(d->conn, read_buf + iStart, sizeof(read_buf) - 10 - iStart, &bytes_read)) {
                // Connection error or closed
                return false;
            }
            nRead = bytes_read;
        } else {
            // Fallback for legacy connections (shouldn't happen)
            nRead = read(d->descriptor, read_buf + iStart, sizeof(read_buf) - 10 - iStart);
        }

        if (nRead > 0)
        {
            read_buf[nRead] = '\0';
            iStart += nRead;

            // Check for health check
            if (strncmp(read_buf, "HEALTH_CHECK", 12) == 0)
            {
                // Mark as health check
                d->healthcheck = true;

                // Send quick response using connection abstraction
                const char *response = "OK\r\n";
                int bytes_written;
                if (d->conn) {
                    connection_write(d->conn, response, strlen(response), &bytes_written);
                } else {
                    write(d->descriptor, response, strlen(response));
                }

                // Don't process further - close socket in next game loop
                return true;
            }

            if (read_buf[iStart - 1] == '\n' || read_buf[iStart - 1] == '\r')
                break;
        }
        else if (nRead == 0)
        {
            // Connection idle (EAGAIN) or needs more data
            if (iStart > 0)
                break;  // Have data, process it
            else
                break;  // No data yet, try again later
        }
        else
        {
            // Error in non-connection path
            if (errno == EWOULDBLOCK)
                break;

            perror("Read_from_descriptor");
            return false;
        }
    }

    if (iStart > 0) {
        read_buf[iStart] = '\0';
        ProtocolInput(d, read_buf, iStart, d->inbuf);
    }
    return true;
}


/**
 * read_from_buffer - Extract one command line from input buffer
 *
 * Transfers one line from d->inbuf to d->incomm for processing.
 * Performs canonical input processing:
 *   - Waits for newline before extracting
 *   - Handles backspace character
 *   - Filters non-printable characters
 *   - Enforces MAX_INPUT_LENGTH limit
 *   - Detects spam/repeat abuse (100+ repeats triggers auto-quit)
 *   - Implements '!' for last command repeat
 *   - Shifts remaining data in input buffer
 *
 * @param d  The descriptor to process
 */
void read_from_buffer(DESCRIPTOR_DATA *d)
{
    int i, j, k;

    /*
     * Hold horses if pending command already.
     */
    if (d->incomm[0] != '\0')
    return;

    /*
     * Look for at least one new line.
     */
    for (i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
    if (d->inbuf[i] == '\0')
        return;
    }
    //log_stringf("d->inbuf: %u %d -- %s", sizeof(d->inbuf), strlen(d->inbuf), d->inbuf);

    /*
     * Canonical input processing.
     */
    for (i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
    if (k >= MAX_INPUT_LENGTH - 2)
    {
        write_to_descriptor(d, "Line too long.\n\r", 0);

        /* skip the rest of the line */
        for (; d->inbuf[i] != '\0'; i++)
        {
        if (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
            break;
        }
        d->inbuf[i]   = '\n';
        d->inbuf[i+1] = '\0';
        break;
    }

    if (d->inbuf[i] == '\b' && k > 0)
        --k;
    else if (ISASCII(d->inbuf[i]) && ISPRINT(d->inbuf[i]))
        d->incomm[k++] = d->inbuf[i];
    /*    else if (d->inbuf[i] == (signed char)IAC) {
            if (!memcmp(&d->inbuf[i], compress_do, strlen(compress_do))) {
                i += strlen(compress_do) - 1;
                compressStart(d);
            }
            else if (!memcmp(&d->inbuf[i], compress_dont, strlen(compress_dont))) {
                i += strlen(compress_dont) - 1;
                compressEnd(d);
            }
            else if (!memcmp(&d->inbuf[i], msp_do, strlen(msp_do)))
            {
                i += strlen(msp_do) - 1;
                SET_BIT(d->bits, DESCRIPTOR_MSP);
            }
            else if (!memcmp(&d->inbuf[i], msp_dont, strlen(msp_dont)))
            {
                i += strlen(msp_dont) - 1;
                REMOVE_BIT(d->bits, DESCRIPTOR_MSP);
            }

        }*/
    }

    /*
     * Finish off the line.
     */
    if (k == 0)
    d->incomm[k++] = ' ';
    d->incomm[k] = '\0';

    /*
     * Deal with bozos with #repeat 1000 ...
     */

    if (k > 1 || d->incomm[0] == '!')
    {
        if (d->incomm[0] != '!' && strcmp(d->incomm, d->inlast))
    {
        d->repeat = 0;
    }
    else
    {
        if (++d->repeat >= 100
            && d->character
        && d->connected == CON_PLAYING
        && !IS_IMMORTAL(d->character))
        {
        log_message_f(LOG_LEVEL_WARN, LOG_WARN, "%s input spamming!", d->host);

        wiznet("Spam spam spam $N spam spam spam!",
               d->character,NULL,WIZ_SPAM,0,get_staff_rank(d->character));
        if (d->incomm[0] == '!')
            wiznet(d->inlast,d->character,NULL,WIZ_SPAM,0,
            get_staff_rank(d->character));
        else
            wiznet(d->incomm,d->character,NULL,WIZ_SPAM,0,
            get_staff_rank(d->character));

        d->repeat = 0;

        write_to_descriptor(d,
            "\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
        strcpy(d->incomm, "quit");

        }
    }
    }


    /*
     * Do '!' substitution.
     */
    if (d->incomm[0] == '!')
    strcpy(d->incomm, d->inlast);
    else
    strcpy(d->inlast, d->incomm);

    /*
     * Shift the input buffer.
     */
    while (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
    i++;
    for (j = 0; (d->inbuf[j] = d->inbuf[i+j]) != '\0'; j++)
    ;
    return;
}


/**
 * process_output - Process and send output buffer to connection
 *
 * Handles output processing for a descriptor:
 *   - Sends pager continuation prompt if in paging mode
 *   - Sends string editor prompt if editing
 *   - Generates and sends player prompt if playing (bust_a_prompt)
 *   - Displays battle prompt with enemy health
 *   - Sends appropriate prompts for login states
 *   - Writes the accumulated output buffer to the connection
 *
 * @param d        The descriptor to send output for
 * @param fPrompt  If true, add appropriate prompt to output
 * @return         true on success, false if write failed (connection error)
 */
bool process_output(DESCRIPTOR_DATA *d, bool fPrompt)
{
    extern bool merc_down;

    if (d->pProtocol->WriteOOB)
    {
        // Do nothing for OOB
    }
    else if (!merc_down)
    {
        if (d->showstr_point)
        {
            write_to_buffer(d, "{x[Hit Return to continue]\n\r", 0);
            if (!d->pProtocol->bSGA)
                write_to_buffer(d, GoAheadStr, 0);

        }
        else if (fPrompt && d->pString && d->connected == CON_PLAYING)
        {
            write_to_buffer(d, "> ", 2);
            if (!d->pProtocol->bSGA)
                write_to_buffer(d, GoAheadStr, 0);
        }
        else if (fPrompt && d->connected == CON_PLAYING)
        {
            CHAR_DATA *ch = d->character;
            CHAR_DATA *victim;

            // Battle prompt
            if ((victim = ch->fighting) != NULL && can_see(ch, victim) && ch->in_room == victim->in_room)
            {
                int percent;
                char wound[100];
                char buf[2 * MAX_STRING_LENGTH];
                char buf2[MSL];

                if (victim->max_hit > 0)
                    percent = victim->hit * 100 / victim->max_hit;
                else
                    percent = -1;

                if (percent >= 100)
                    strcpy(wound, "is in excellent condition.");
                else if (percent >= 90)
                    strcpy(wound, "has a few scratches.");
                else if (percent >= 80)
                    strcpy(wound, "has a few scratches and bruises.");
                else if (percent >= 70)
                    strcpy(wound, "has some small wounds.");
                else if (percent >= 60)
                    strcpy(wound, "has some small wounds and bruises.");
                else if (percent >= 50)
                    strcpy(wound, "has some nasty wounds and scratches.");
                else if (percent >= 40)
                    strcpy(wound, "looks pretty hurt.");
                else if (percent >= 30)
                    strcpy(wound, "looks very hurt.");
                else if (percent >= 20)
                    strcpy(wound, "is in awful condition.");
                else if (percent >= 10)
                    strcpy(wound, "is barely clinging to life.");
                else
                    strcpy(wound, "is on the verge of death.");

                if (IS_SET(ch->comm, COMM_SHOW_FORM_STATE))
                    show_form_state(ch);

                sprintf(buf2, "%s", pers(victim, ch));
                buf2[0] = UPPER(buf2[0]);

                sprintf(buf, "{M%s %s \n\r{x", buf2, wound);
                buf[0] = UPPER(buf[0]);
                write_to_buffer(d, buf, 0);
            }

            ch = d->original ? d->original : d->character;
            if (!IS_SET(ch->comm, COMM_COMPACT))
                write_to_buffer(d, "\n\r", 2);

            if (IS_SET(ch->comm, COMM_PROMPT))
                bust_a_prompt(d->character);

            if (!d->pProtocol->bSGA)
                write_to_buffer(d, GoAheadStr, 0);

            if (IS_SET(ch->comm, COMM_TELNET_GA))
                write_to_buffer(d, go_ahead_str, 0);
        }
else if (fPrompt && !d->showstr_point && !d->pString)
{
    // Only add prompts for states that don't already include them in their handlers
    switch (d->connected) {
        // Menu states - these use "Enter choice: "
        case CON_ACCOUNT_MENU:
        case CON_CHARACTER_MENU:
        case CON_ACCOUNT_MFA_MENU:
        case CON_CHARACTER_MFA_MENU:
            write_to_buffer(d, "Enter choice: ", 0);
            break;
            
        // Login states with specific prompts
        case CON_GET_ACCOUNT_NAME:
        

            break;
        case CON_GET_ACCOUNT_PASSWORD:
        case CON_CHANGE_ACCOUNT_PASSWORD:
        case CON_CHANGE_PASSWORD:
        case CON_GET_OLD_PASSWORD:
        case CON_NEW_ACCOUNT_PASSWORD:
        case CON_GET_NEW_PASSWORD:
        case CON_STAFF_PASSWORD:
            write_to_buffer(d, "Password: ", 0);
            break;
        case CON_CONFIRM_ACCOUNT_PASSWORD:
        case CON_CONFIRM_CHARACTER_PASSWORD:
        case CON_CHANGE_PASSWORD_CONFIRM:
        case CON_CONFIRM_NEW_PASSWORD:
        case CON_CONFIRM_STAFF_PASSWORD:
            write_to_buffer(d, "Confirm password: ", 0);
            break;
        case CON_GET_ACCOUNT_EMAIL:
        case CON_CHANGE_ACCOUNT_EMAIL:
        case CON_GET_EMAIL:
        case CON_CHANGE_CHARACTER_EMAIL:
        case CON_GET_STAFF_EMAIL:
            write_to_buffer(d, "Email: ", 0);
            break;
        case CON_GET_ACCOUNT_MFA:
        case CON_GET_MFA:
        case CON_GET_CHAR_MFA:
        case CON_GET_ACCOUNT_MFA_FOR_CHAR:
        case CON_ACCOUNT_MFA_VERIFY_FOR_SETTINGS:
        case CON_ACCOUNT_MFA_CONFIRM:
        case CON_CHARACTER_MFA_VERIFY:
        case CON_CHARACTER_MFA_VERIFY_FOR_SETTINGS:
        case CON_CHARACTER_MFA_CONFIRM:
        case CON_VERIFY_UNLINK_MFA:
        case CON_VERIFY_DELETE_MFA:
            write_to_buffer(d, "MFA code: ", 0);
            break;
        case CON_GET_CHAR_PASSWORD:
        case CON_LINK_CHARACTER_PASSWORD:
            write_to_buffer(d, "Character password: ", 0);
            break;
        case CON_CREATING_NEW_CHAR:
            write_to_buffer(d, "Character name: ", 0);
            break;
        case CON_CREATING_NEW_STAFF_CHAR:
            write_to_buffer(d, "Staff character name: ", 0);
            break;
        case CON_LINK_CHARACTER_NAME:
            write_to_buffer(d, "Character to link: ", 0);
            break;
        case CON_SET_UNLINK_PASSWORD:
            write_to_buffer(d, "New password: ", 0);
            break;
        case CON_VERIFY_ACCOUNT_EMAIL_CHANGE:
        case CON_VERIFY_CHARACTER_EMAIL_CHANGE:
            write_to_buffer(d, "Verification code: ", 0);
            break;
        case CON_CONFIRM_DELETE_CHARACTER:
        case CON_CONFIRM_NEW_NAME:
            // These expect yes/no or specific confirmation text
            // Don't add a default prompt
            break;
            
        // Add telnet GA signal for most states
        default:
            // Don't add any default prompt text - rely on handler functions
            break;
    }
    
    // Add telnet GA for relevant states
    if (d->connected != CON_PLAYING && 
        d->connected != CON_READ_MOTD &&
        d->connected != CON_READ_IMOTD) {
        write_to_buffer(d, go_ahead_str, 0);
    }
}

    }

    if (d->outtop == 0)
        return true;

    if (!write_to_descriptor(d, d->outbuf, d->outtop))
    {
        d->outtop = 0;
        return false;
    }
    else
    {
        d->outtop = 0;
        return true;
    }
}


/**
 * bust_a_prompt - Generate and send player-customizable prompt
 *
 * Parses the player's prompt string and substitutes variables:
 *   %h/%H  - Current/max hit points (color-coded)
 *   %m/%M  - Current/max mana (color-coded)
 *   %v/%V  - Current/max movement (color-coded)
 *   %x/%X  - Experience/exp to next level
 *   %q/%Q  - Quest points/quests completed
 *   %g/%s  - Gold/silver
 *   %p/%t  - Practice/train sessions
 *   %P     - Pneuma
 *   %b     - Bank balance
 *   %a     - Alignment (numeric or good/neutral/evil)
 *   %r/%R  - Room name/vnum (R shows wilds coords for immortals)
 *   %z     - Zone name (immortals only)
 *   %e     - Available exits
 *   %o/%O  - OLC editor name/vnum
 *   %w/%W  - Current/max carry weight
 *   %i/%I  - Current/max carry items
 *   %C     - Coin weight
 *   %c     - Newline
 *   %+     - Server description
 *   %-     - Connection security indicator
 *   %_     - Character name
 *   %%     - Literal percent sign
 *   %<x>   - Script variable lookup
 *
 * Also displays status indicators: [WRITING NOTE], [MAIL], [NOTE], etc.
 *
 * Originally coded by Morgenes for Aldara Mud.
 *
 * @param ch  The character to generate the prompt for
 *
 * @refactor Consider supporting named field syntax in addition to shortcuts.
 *           For example: %cur_hp, %max_hp, %cur_mana, %max_mana, etc.
 *           This would improve readability and allow for more fields without
 *           running out of single-character codes.
 */
void bust_a_prompt(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    const char *str;
    const char *i;
    char *point, *p;
    //char *pbuff;
    //char buffer[ MAX_STRING_LENGTH*2 ];
    char doors[MAX_INPUT_LENGTH];
    EXIT_DATA *pexit;
    bool found;
    const char *dir_name[] = {"N","E","S","W","U","D","NE","NW","SE","SW"};
    int door;

    if(ch->desc && ch->desc->input && !ch->desc->inputString) {
        send_to_char(ch->desc->input_prompt ? ch->desc->input_prompt : " >", ch);
        send_to_char("{x \n\r", ch);
        return;
    }

    if (ch->pk_question || ch->remove_question)
    {
    send_to_char("{Y({xY{R/{xN{Y){x\n\r", ch);
    return;
    }

    if( ch->remort_question )
    {
        send_to_char("{YAre you ready to be reborn? (yes/no){x\n\r", ch);
        return;
    }

    if (ch->orace_question)
    {
        send_to_char("{YSelect your original race (before you transformed):{x\n\r", ch);
        return;
    }

    if (ch->pnote != NULL)
    send_to_char("{Y[WRITING NOTE]{x", ch);

    if (has_mail(ch))
    send_to_char("{R[MAIL]{x", ch);

    if (count_note(ch, NOTE_NOTE))
    send_to_char("{G[NOTE]{x", ch);

    if (count_note(ch, NOTE_NEWS))
    send_to_char("{Y[NEWS]{x", ch);

    if (count_note(ch, NOTE_CHANGES))
    send_to_char("{R[CHANGES]{x", ch);

    if (ch->mail != NULL)
        send_to_char("{R[UNSENT MAIL]{x", ch);

    if (ch->ambush != NULL)
    send_to_char("{Y[Ambushing]{x", ch);

    if (ch->hunting != NULL)
    send_to_char("{G[Hunting]{x", ch);

    if (IS_SET(ch->affected_by[0], AFF_INVISIBLE)
    || IS_SET(ch->affected_by[1], AFF2_IMPROVED_INVIS))
    send_to_char("{B[*]{x", ch);

    if (IS_MORPHED(ch) && race_get_trait_bool(ch->race, "can_shapeshift"))
    send_to_char("{G[{YSHAPED{G]{x ", ch);

    if (IS_SHIFTED(ch))
    send_to_char("{G[{YSHIFTED{G]{x ", ch);

    if (IS_IMMORTAL(ch) && count_project_inquiries(ch) > 0)
    send_to_char("{g[{GINQUIRY{g]{x ", ch);

    if (IS_IMMORTAL(ch) && channel_service_staff_report_count() > 0)
        printf_to_char(ch, "{R[{WREPORT:%d{R]{x ", channel_service_staff_report_count());

    if (MOUNTED(ch))
    {
    sprintf(buf, "{Y<%ldmv>{x", ch->mount->move);
    send_to_char(buf, ch);
    }

    point = buf;
    str = ch->prompt;
    if(!str || str[0] == '\0')
    {
    if (MOUNTED(ch))
            sprintf(buf, "{B<{x%ld{Bhp {x%ld{Bm {x%ld{Bmv>{Y< %ldmv >{x ",
        ch->hit, ch->mana, ch->move, ch->mount->move);
    else
            sprintf(buf, "{B<{x%ld{Bhp {x%ld{Bm {x%ld{Bmv>{x ",
        ch->hit, ch->mana, ch->move);

    send_to_char(buf, ch);
    return;
    }

   if (IS_SET(ch->comm,COMM_AFK))
   {
       sprintf(buf, "{D<AFK>{x\n\r");
       send_to_char(buf,ch);
       return;
   }

   if (IS_SOCIAL(ch))
   {
       if (ch->in_room->chat_room != NULL)
       {
           if (is_op(ch->in_room->chat_room, ch->name))
           sprintf(buf, "{B<{Y@{x#%s{B>{x \n\r",
               ch->in_room->chat_room->name);
       else
           sprintf(buf, "{B<{x#%s{B>{x \n\r",
           ch->in_room->chat_room->name);

       send_to_char(buf, ch );
       }
       else
           send_to_char("{B<{xChat{B>{x\n\r", ch);

       return;
   }

   while(*str != '\0')
   {
      if(*str != '%')
      {
         *point++ = *str++;
         continue;
      }
      ++str;
    switch(*str) {
    default : i = " "; break;
    case 'e':
        found = false;
        doors[0] = '\0';
        for (door = 0; door < 10; door++) {
            if ((pexit = ch->in_room->exit [door]) && pexit ->u1.to_room &&
                (can_see_room(ch,pexit->u1.to_room) ||
                    (IS_AFFECTED(ch,AFF_INFRARED) && !IS_AFFECTED(ch,AFF_BLIND))) &&
                !IS_SET(pexit->exit_info,EX_CLOSED)) {
                found = true;
                strcat(doors,dir_name[door]);
            }
        }
        if (!found) strcat(buf,"none");
        sprintf(buf2,"%s",doors);
        i = buf2;
        break;
    case 'c' :
        sprintf(buf2,"%s","\n\r");
        i = buf2;
        break;
    case 'h' :
        if (ch->hit > ch->max_hit)
            sprintf(buf2, "{W%ld{x", ch->hit);
        else if (ch->hit < ch->max_hit / 2)
            sprintf(buf2, "{R%ld{x", ch->hit);
        else if (ch->hit < 2 * ch->max_hit / 3)
            sprintf(buf2, "{G%ld{x", ch->hit);
        else
            sprintf(buf2, "{x%ld", ch->hit);

        i = buf2;
        break;
    case 'H' :
        sprintf(buf2, "%ld", ch->max_hit);
        i = buf2;
        break;
    case 'm' :
        if (ch->mana < ch->max_mana / 2)
            sprintf(buf2, "{R%ld{x", ch->mana);
        else if (ch->mana < 2 * ch->max_mana / 3)
            sprintf(buf2, "{G%ld{x", ch->mana);
        else
            sprintf(buf2, "{x%ld", ch->mana);
        i = buf2;
        break;
    case 'M' :
        sprintf(buf2, "%ld", ch->max_mana);
        i = buf2; break;
    case 'v' :
        if (ch->move < ch->max_move / 2)
            sprintf(buf2, "{R%ld{x", ch->move);
        else if (ch->move < 2 * ch->max_move / 3)
            sprintf(buf2, "{G%ld{x", ch->move);
        else
            sprintf(buf2, "{x%ld", ch->move);
        i = buf2;
        break;
    case 'V' :
        sprintf(buf2, "%ld", ch->max_move);
        i = buf2; break;
    case 'x' :
        sprintf(buf2, "%ld", ch->exp);
        i = buf2; break;
    case 'X' :
        sprintf(buf2, "%ld", IS_NPC(ch) ? 0 :
        exp_per_level(ch, NULL, ch->pcdata->points) - ch->exp);
        i = buf2; break;
    case 'Q' :
        sprintf(buf2, "%ld", IS_NPC(ch) ? 0 : ch->pcdata->quests_completed);
        i = buf2; break;
    case 'q' :
        sprintf(buf2, "%d", IS_NPC(ch) ? 0 : ch->questpoints);
        i = buf2; break;
    case 'p' :
        sprintf(buf2, "%d", IS_NPC(ch) ? 0 : ch->practice);
        i = buf2; break;
    case 'P' :
        sprintf(buf2, "%ld", IS_NPC(ch) ? 0 : ch->pneuma);
        i = buf2; break;
    case 't' :
        sprintf(buf2,"%d", IS_NPC(ch) ? 0 : ch->train);
        i = buf2; break;
    case 'b' :
        sprintf(buf2, "%ld", IS_NPC(ch) ? 0 : ch->pcdata->bankbalance);
        i = buf2; break;
    case 'g' :
        sprintf(buf2, "%ld", ch->gold);
        i = buf2; break;
    case 's' :
        sprintf(buf2, "%ld", ch->silver);
        i = buf2; break;
    case 'a' :
        if(ch->level > 9)
            sprintf(buf2, "%d", ch->alignment);
        else
            sprintf(buf2, "%s", IS_GOOD(ch) ? "good" : IS_EVIL(ch) ? "evil" : "neutral");
        i = buf2; break;
    case 'r' :
        if(ch->in_room != NULL)
            sprintf(buf2, "%s",
                ((!IS_NPC(ch) && IS_SET(ch->act[0],PLR_HOLYLIGHT)) ||
                (!IS_AFFECTED(ch,AFF_BLIND) && !room_is_dark(ch->in_room)))
                ? ch->in_room->name : "darkness");
        else
            sprintf(buf2, " ");
        i = buf2; break;
    case 'R' :
        /* VIZZWILDS */
        if(IS_IMMORTAL(ch)) {
            if (ch->in_room) {
                if (ch->in_wilds)
                    sprintf(buf2, "(%ld, %ld)", ch->in_room->x, ch->in_room->y);
                else
                    sprintf(buf2, "%s", widevnum_string_room(ch->in_room, NULL));
            } else
                sprintf(buf2, " ");
        } else
            sprintf(buf2, " ");
        i = buf2; break;
    case 'z' :
        if(IS_IMMORTAL(ch) && ch->in_room != NULL)
            sprintf(buf2, "%s", ch->in_room->area->name);
        else
            sprintf(buf2, " ");
        i = buf2; break;
    case '+':
        sprintf(buf2, game_settings.server_description);
        i = buf2; break;
    case '-':
        sprintf(buf2, ch->desc->ssl ? "{G[SECURE]{X" : "{R[INSECURE]{X");
        i = buf2; break;
    case '_':
        sprintf(buf2, ch->name);
        i = buf2; break;
    case '%' :
        sprintf(buf2, "%%");
        i = buf2; break;
    case 'o' :
        sprintf(buf2, "%s", olc_ed_name(ch));
        i = buf2; break;
    case 'O' :
        sprintf(buf2, "%s", olc_ed_vnum(ch));
        i = buf2; break;
    case 'w' :
        sprintf(buf2, "%ld", get_carry_weight(ch));
        i = buf2; break;
    case 'W' :
        sprintf(buf2, "%d", can_carry_w(ch));
        i = buf2; break;
    case 'i' :
        sprintf(buf2, "%d", ch->carry_number);
        i = buf2; break;
    case 'I' :
        sprintf(buf2, "%d", can_carry_n(ch));
        i = buf2; break;
    case 'C' :
        sprintf(buf2, "%ld", COIN_WEIGHT(ch));
        i = buf2; break;
    case 'J' :
        sprintf(buf2, "%s", IS_IMMORTAL(ch) ? ch->pcdata->immortal->build_project!= NULL ? ch->pcdata->immortal->build_project->name : "" : "N/A");
        i = buf2; break;

    case '<':
        p = buf2;
        ++str;
        while(*str && *str != '>') *p++ = *str++;
        *p = '\0';

        i = get_script_prompt_string(ch,buf2);
        break;
    }
      if(*str) ++str;
      while((*point = *i) != '\0')
         ++point, ++i;
   }
   *point	= '\0';
   //pbuff	= buffer;
   //colourconv(pbuff, buf, ch);
   write_to_buffer(ch->desc, buf, 0);
}


/**
 * write_to_buffer - Append text to descriptor's output buffer
 *
 * Queues text for later transmission. The buffer accumulates output until
 * process_output() sends it. Handles:
 *   - Protocol output processing (color codes, telnet escaping)
 *   - Automatic length calculation if not provided
 *   - Initial newline injection for fresh output
 *   - Dynamic buffer expansion (doubles up to 128KB max)
 *   - Buffer overflow protection (closes socket on overflow)
 *
 * @param d       The descriptor to write to
 * @param txt     The text to append
 * @param length  Length of text, or 0 to auto-calculate
 */
void write_to_buffer(DESCRIPTOR_DATA *d, const char *txt, int length)
{
    if( d->muted > 0 ) return;

    txt = ProtocolOutput(d,txt,&length);
    if (d->pProtocol->WriteOOB > 0)
        --d->pProtocol->WriteOOB;
    /*
     * Find length in case caller didn't.
     */
    if (length <= 0)
    length = strlen(txt);

    /*
     * Initial \n\r if needed.
     */
//    if (d->outtop == 0 && !d->fcommand)
    if (d->outtop == 0 && !d->fcommand && !d->pProtocol->WriteOOB)
    {
    d->outbuf[0]	= '\n';
    d->outbuf[1]	= '\r';
    d->outtop	= 2;
    }

    /*
     * Expand the buffer as needed.
     */
    while (d->outtop + length >= d->outsize)
    {
    char *outbuf;

        if (d->outsize >= 128000)
    {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Buffer overflow. Closing.\n\r");
        close_socket(d);
        return;
     }
    outbuf      = alloc_mem(2 * d->outsize);
    strncpy(outbuf, d->outbuf, d->outtop);
    free_mem(d->outbuf, d->outsize);
    d->outbuf   = outbuf;
    d->outsize *= 2;
    }

    /*
     * Copy.
     */
    strncpy(d->outbuf + d->outtop, txt, length);
    d->outtop += length;
    return;
}


/**
 * write_to_descriptor_2 - Low-level output function
 *
 * Writes a block of text directly to the connection. This is the lowest
 * level output function that actually transmits data. Uses the connection
 * abstraction layer for TLS/WebSocket transparency. Writes in 4KB blocks
 * to avoid issues with very long outputs.
 *
 * If compression is active, delegates to writeCompressed() instead.
 *
 * @param d       The descriptor to write to
 * @param txt     The text to write
 * @param length  Length of text to write
 * @return        true on success, false on connection error
 */
bool write_to_descriptor_2(DESCRIPTOR_DATA *d, char *txt, int length)
{
    int iStart;
    int nWrite;
    int nBlock;

    d->last_activity = current_time;

    if (d->out_compress)
        return writeCompressed(d, txt, length);

    for (iStart = 0; iStart < length; iStart += nWrite)
    {
        nBlock = UMIN(length - iStart, 4096);
        int bytes_written;

        // Use connection abstraction
        if (d->conn) {
            if (!connection_write(d->conn, txt + iStart, nBlock, &bytes_written)) {
                // Connection error or closed
                return false;
            }
            nWrite = bytes_written;

            // If nothing was written (EAGAIN), break and try again later
            if (nWrite == 0)
                break;
        } else {
            // Fallback for legacy connections (shouldn't happen)
            nWrite = write(d->descriptor, txt + iStart, nBlock);
            if (nWrite < 0) {
                if (errno == EPIPE || errno == EWOULDBLOCK) {
                    return false;
                }
                perror("Write_to_descriptor_2");
                return false;
            }
        }
    }

    return true;
}


/**
 * write_to_descriptor - Write text to descriptor with compression support
 *
 * Wrapper around write_to_descriptor_2 that handles MCCP compression.
 * If compression is enabled for the descriptor, uses writeCompressed().
 * Otherwise falls through to write_to_descriptor_2().
 *
 * @param d       The descriptor to write to
 * @param txt     The text to write
 * @param length  Length of text to write
 * @return        true on success, false on connection error
 */
bool write_to_descriptor(DESCRIPTOR_DATA *d, char *txt, int length)
{
    if (d->out_compress)
        return writeCompressed(d, txt, length);
    else
        return write_to_descriptor_2(d, txt, length);
}


#define DEBUG		true

/*
void join_world(DESCRIPTOR_DATA * d)
{
    CHAR_DATA *ch;
    char buf[MSL];

    ch = d->character;
    plogf (LOG_INFO, "nanny.c, join_world(): Placing character in game.");
    if (ch->pcdata == NULL || ch->pcdata->pwd[0] == '\0')
    {
        send_to_char ("Warning! Null password!\n\r", ch);
        send_to_char ("Please report old password with bug.\n\r",
                      ch);
        send_to_char ("Type 'password null <new password>' to fix.\n\r",
                      ch);
    }

    list_appendlink(loaded_char, ch);

    d->connected = CON_PLAYING;
    reset_char (ch);

    if (ch->level == 0)
    {
        if(global.mud_ansicolour)
            SET_BIT (ch->act[0], PLR_COLOUR);
        if(global.mud_telnetga)
            SET_BIT (ch->comm, COMM_TELNET_GA);

        ch->perm_stat[ch_get_trait_int(ch, "primary_stat")] += 3;

        ch->level = 1;
        ch->tot_level = 1;
        ch->exp = exp_per_level (ch, NULL, ch->pcdata->points);
        ch->hit = ch->max_hit;
        ch->mana = ch->max_mana;
        ch->move = ch->max_move;
        ch->train = 3;
        ch->practice = 5;
        sprintf (buf, "the %s", title_table[ch->class][ch->level]
                 [ch->normal_sex == SEX_FEMALE ? 1 : 0]);
        set_title (ch, buf);

        {
            OBJ_INDEX_DATA *map_index = get_reserved_obj_index("obj_map");
            if (map_index)
                obj_to_char(create_object(map_index, 0), ch);
        }

        {
            ROOM_INDEX_DATA *school_room = get_reserved_room_index("room_begin_new_character");
            if (!school_room)
                school_room = get_reserved_room_index("room_limbo");
            if (!school_room) {
                log_message(LOG_LEVEL_BUG, LOG_ERROR, "join_world: no room_begin_new_character or room_limbo reserved.");
                return;
            }
            char_to_room(ch, school_room);
        }
        send_to_char ("\n\r", ch);
        do_function (ch, &do_help, "newbie info");
    }
    else
    {
        if (ch->in_room != NULL)
        {
            plogf(LOG_INFO, "nanny.c, join_world(): Transferring char to Real Room");
            char_to_room (ch, ch->in_room);
        }
        else
        {
            if (ch->in_wilds != NULL)
            {
                plogf(LOG_INFO, "nanny.c, join_world(): Transferring char to VRoom");
                char_to_vroom (ch, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
            }
            else
            {
                if (IS_IMMORTAL (ch))
                {
                    {
                        ROOM_INDEX_DATA *chat_room = get_reserved_room_index("room_chat_lobby");
                        if (!chat_room)
                            chat_room = get_reserved_room_index("room_limbo");
                        if (!chat_room) {
                            log_message(LOG_LEVEL_BUG, LOG_ERROR, "join_world: no room_chat_lobby or room_limbo reserved.");
                            return;
                        }
                        char_to_room(ch, chat_room);
                    }
                }
                else
                {
                    {
                        ROOM_INDEX_DATA *limbo_room = get_reserved_room_index("room_limbo");
                        if (!limbo_room) {
                            log_message(LOG_LEVEL_BUG, LOG_ERROR, "join_world: no room_limbo reserved.");
                            return;
                        }
                        char_to_room(ch, limbo_room);
                    }
                }
            }
        }
    }

    send_to_char ("\n\r", ch);
    do_function (ch, &do_last, "");
    send_to_char(
                 "12345678901234567890123456789012345678901234567890123456789012345678901234567890\n\r", ch);
    send_to_char(
                 "       Make sure you can see the above line (80 chars) all on one line---------^\n\r", ch);
    act ("$n has entered the game.", ch, NULL, NULL, TO_ROOM);
    do_function (ch, &do_look, "auto");
    event_notify_active_events_for_char(ch, true);

    wiznet ("$N has left real life behind.", ch, NULL,
            WIZ_LOGINS, WIZ_SITES, get_staff_rank (ch));

    if (ch->pet != NULL)
    {
        char_to_room (ch->pet, ch->in_room);
        act ("$n has entered the game.", ch->pet, NULL, NULL,
             TO_ROOM);
    }

    return;
}
*/




/**
 * check_parse_name - Validate a character name for acceptability
 *
 * Performs comprehensive validation of a proposed character name:
 *   - Rejects reserved words (sentience, all, auto, self, someone, etc.)
 *   - Enforces length limits (3-12 characters)
 *   - Requires alphabetic characters only
 *   - Rejects names with only I and L (anti-lll twit check)
 *   - Prevents excessive capitalization
 *   - Prevents naming after existing mob names
 *   - Detects and disconnects duplicate newbie attempts
 *
 * @param name  The name to validate
 * @return      true if name is acceptable, false otherwise
 */
bool check_parse_name(char *name)
{
    DESCRIPTOR_DATA *d, *dnext;
    int count = 0;

    /*
     * Reserved words.
     */
    if (is_exact_name(name,
    "sentience all auto her his immortal its self somebody someone something the you your loner"))
    {
    return false;
    }

    /*
     * Length restrictions.
     */
    if (strlen(name) <  3)
    return false;

    if (strlen(name) > 12)
    return false;

    /*
     * Alphanumerics only.
     * Lock out IllIll twits.
     */
    {
    char *pc;
    bool fIll,adjcaps = false,cleancaps = false;
     int total_caps = 0;

    fIll = true;
    for (pc = name; *pc != '\0'; pc++)
    {
        if (!ISALPHA(*pc))
        return false;

        if (ISUPPER(*pc)) /* ugly anti-caps hack */
        {
        if (adjcaps)
            cleancaps = true;
        total_caps++;
        adjcaps = true;
        }
        else
        adjcaps = false;

        if (LOWER(*pc) != 'i' && LOWER(*pc) != 'l')
        fIll = false;
    }

    if (fIll)
        return false;

    if (cleancaps || (total_caps > (strlen(name)) / 2 && strlen(name) < 3))
        return false;
    }

   /*
    * Prevent players from naming themselves after mobs.
    */
    {
    extern MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
    MOB_INDEX_DATA *pMobIndex;
    int iHash;

    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (pMobIndex  = mob_index_hash[iHash];
          pMobIndex != NULL;
          pMobIndex  = pMobIndex->next)
        {
        if (is_name(name, pMobIndex->player_name))
            return false;
        }
    }
    }

    /* Vizz -
     * check names of people playing. Yes, this is necessary for multiple
     * newbies with the same name (thanks Saro)
     */
    if (descriptor_list)
    {
        count=0;
        for (d = descriptor_list; d != NULL; d = dnext)
    {
            dnext=d->next;
            if (d->connected!=CON_PLAYING  && d->character && d->character->name
            && d->character->name[0] && !str_cmp(d->character->name,name))
        {
                  count++;
          close_socket(d);
        }
        }
        if (count)
    {
            sprintf(log_buf,"Double newbie alert (%s)",name);
            wiznet(log_buf,NULL,NULL,WIZ_LOGINS,0,0);

            return false;
        }
    }

    return true;
}
// Temporarily disabling for reconnect crash
/*
CHAR_DATA *find_existing_player(char *name)
{
    CHAR_DATA *ch;
    ITERATOR cit;

    iterator_start(&cit, loaded_players);
    while(( ch = (CHAR_DATA *)iterator_nextdata(&cit)))
    {
        if (!IS_NPC(ch) &&
            !str_cmp(name, ch->name)) {
            iterator_stop(&cit);

            return ch;
        }
    }
    iterator_stop(&cit);

    return NULL;
}
*/

/**
 * check_reconnect - Check for and handle reconnection to link-dead character
 *
 * Searches for an existing character with the given name that is link-dead
 * (in loaded_chars but without a descriptor). If found:
 *   - Sets up reconnect_ch pointer
 *   - Determines authentication requirements (MFA, password)
 *   - Either completes reconnection immediately or prompts for auth
 *
 * Authentication is required for:
 *   - Staff characters when require_2fa_staff is enabled
 *   - Characters with MFA configured
 *   - Characters with passwords set
 *
 * @param d      The descriptor attempting to reconnect
 * @param name   The character name to check
 * @param fConn  If true, actually perform the reconnection; if false, just check
 * @return       true if character found (reconnecting), false otherwise
 */
bool check_reconnect(DESCRIPTOR_DATA *d, char *name, bool fConn)
{
    CHAR_DATA *ch;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool found = false;
    bool authentication_needed = false;
    ITERATOR cit;

    // Skip if we're already reconnecting to prevent double handling
    if (d->reconnecting && d->reconnect_ch)
        return true;

    iterator_start(&cit, loaded_chars);
    while((ch = (CHAR_DATA *)iterator_nextdata(&cit)) && !found)
    {
        if (!IS_NPC(ch) && 
            (!str_cmp(name, ch->name)) && 
            ch != d->character)
        {
            found = true;
            break;
        }
    }
    iterator_stop(&cit);
    
    if (!found)
        return false;
    
    // Add logging to help diagnose issues
    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "check_reconnect: Found existing character %s, descriptor: %s",
                ch->name, ch->desc ? "connected" : "linkdead");
    
    // Set reconnect_ch regardless of authentication - we'll need it later
    d->reconnect_ch = ch;
    
    // If not actually connecting yet, just set this up for auth checks
    if (!fConn) {
        d->reconnecting = true;
        return true;
    }
    
    // Handle authentication requirements
    if (!DEV_SKIP_MFA) {
        // Improve the MFA check to properly handle empty strings
        bool has_mfa = false;
        if (d->account) {
            get_character_auth_data(ch, d->account, &acct_char);
            has_mfa = acct_char ? !IS_NULLSTR(acct_char->mfa_key) : false;
        }
        
        if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff) {
            // If character has MFA, verify that
            if (has_mfa) {
                write_to_buffer(d, "\n\rReconnecting - This character has MFA enabled.\n\r", 0);
                ProtocolNoEcho(d, true);
                d->reconnecting = true;
                d->connected = CON_GET_CHAR_MFA;
                authentication_needed = true;
            } 
            // Otherwise, verify account MFA only if it's actually set
            else if (d->account && !IS_NULLSTR(d->account->mfa_key)) {
                write_to_buffer(d, "\n\rReconnecting - Staff account MFA verification required.\n\r", 0);
                ProtocolNoEcho(d, true);
                d->reconnecting = true;
                d->connected = CON_GET_ACCOUNT_MFA_FOR_CHAR;
                authentication_needed = true;
            }
        }
        // Regular character with MFA
        else if (has_mfa) {
            write_to_buffer(d, "\n\rReconnecting - This character has MFA enabled.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->reconnecting = true;
            d->connected = CON_GET_CHAR_MFA;
            authentication_needed = true;
        }
    }

    // Check for character password - use account_character data
    if (!authentication_needed && !DEV_SKIP_PASSWORD && acct_char && !IS_NULLSTR(acct_char->pwd)) {
        write_to_buffer(d, "\n\rReconnecting - Password verification required.\n\r", 0);
        ProtocolNoEcho(d, true);
        d->reconnecting = true;
        d->connected = CON_GET_CHAR_PASSWORD;
        authentication_needed = true;
    }

    // If authentication is needed, don't complete the reconnect yet
    if (authentication_needed) {
        return true;
    }

    // No authentication needed, complete the reconnection immediately
    d->reconnecting = true;
    complete_reconnect(d);
    return true;
}

/**
 * complete_reconnect - Finish reconnection after authentication
 *
 * Called after successful authentication to complete the reconnection
 * process. Performs:
 *   - Frees temporary character from login if different from reconnect target
 *   - Links descriptor to the reconnecting character
 *   - Clears reconnection flags
 *   - Resets inactivity timer
 *   - Sets playing state
 *   - Places character if not in a room
 *   - Announces reconnection to room
 *   - Logs reconnection
 *   - Sends MXP version tag
 *   - Shows room description
 *
 * @param d  The descriptor that completed authentication
 */
void complete_reconnect(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch;

    if (!d->reconnect_ch || !d->reconnecting) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "complete_reconnect: Missing reconnect_ch or reconnecting flag");
        return;
    }
    
    ch = d->reconnect_ch;
    
    // Free any temporary character loaded during login
    if (d->character && d->character != ch) {
        free_char(d->character);
    }
    
    // Connect the descriptor to the reconnecting character
    d->character = ch;
    ch->desc = d;
    
    // Clear reconnection flags
    d->reconnect_ch = NULL;
    d->reconnecting = false;
    
    // Reset inactivity timer
    ch->timer = 0;
    
    // Set playing state
    d->connected = CON_PLAYING;
    
    // Notify player of reconnection
    send_to_char("\n\r{GReconnecting to game...{x\n\r", ch);
    
    // Place character if they're not already in a room
    if (!ch->in_room) {
        ROOM_INDEX_DATA *recall = get_reserved_room_index("room_default_recall");
        if (recall)
            char_to_room(ch, recall);
        else
            char_to_room(ch, get_reserved_room_index("room_limbo"));
    }
    
    // Announce reconnection to room
    act("$n has reconnected.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    
    // Log the reconnection
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "%s@%s reconnected.", ch->name, d->host);
    wiznet("$N has reconnected.", ch, NULL, WIZ_LINKS, 0, 0);
    
    // Update protocol
    MXPSendTag(d, "<VERSION>");
    
    // Show room to player
    do_function(ch, &do_look, "auto");
    event_notify_active_events_for_char(ch, true);
}

/**
 * reconnect_char - Alternative reconnection handler for existing connections
 *
 * Handles reconnection when the character is already linked to a descriptor.
 * Used when taking over an existing connection. Performs:
 *   - Socket health check for long-idle connections
 *   - Token relationship fixup (ltokens to tokens linked list)
 *   - Sets playing state
 *   - Announces reconnection
 *   - Logs reconnection
 *   - Updates protocol settings
 *   - Adds connection to tracking lists
 *
 * @param d  The descriptor reconnecting
 */
void reconnect_char(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    LLIST_LINK *link;
    TOKEN_DATA *token;

    if (!ch) return;

if (ch && ch->desc) {
    // Test if connection is still valid after long idle
    if (ch->timer > 10) {
        // Simple non-blocking test write to verify socket is healthy
        char test_byte = 0;
        int result;
        
        if (ch->desc->ssl)
            result = SSL_write(ch->desc->ssl, &test_byte, 0);
        else
            result = send(ch->desc->descriptor, &test_byte, 0, MSG_DONTWAIT);
            
        if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Socket is probably dead, close it properly
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Detected broken connection during reconnect");
            close_socket(ch->desc);
            return;
        }
    }
}


    // Fix token relationships
    if (ch->tokens == NULL && ch->ltokens != NULL) {
        for (link = ch->ltokens->head; link; link = link->next) {
            token = (TOKEN_DATA *)link->data;
            if (token && token->player == ch) {
                token->next = ch->tokens;
                ch->tokens = token;
            }
        }
    }
    
    // Set to playing state immediately
    d->connected = CON_PLAYING;
    d->reconnecting = false;
    
    // Send reconnection message
    send_to_char("Reconnecting. Type replay to see missed tells.\n\r", ch);
    act("$n has reconnected.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    
    // Log the reconnection
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "%s@%s reconnected.", ch->name, d->host);
    wiznet("$N has relinked.", ch, NULL, WIZ_LINKS, 0, 0);
    
    // Update protocol settings
    MXPSendTag(d, "<VERSION>");
    
    // Add connection to tracking
    connection_add(d);
}

/**
 * check_playing - Check if character name is already connected
 *
 * Searches all descriptors for an existing connection with the given
 * character name. If found, prompts the user to confirm taking over
 * the existing connection.
 *
 * @param d     The new descriptor attempting to login
 * @param name  The character name to check
 * @return      true if character found (prompting for takeover), false otherwise
 */
bool check_playing(DESCRIPTOR_DATA *d, char *name)
{
    DESCRIPTOR_DATA *dold;

    for (dold = descriptor_list; dold; dold = dold->next)
    {
    if (dold != d
    &&   dold->character != NULL
    &&   dold->connected != CON_GET_ACCOUNT_NAME
    &&   dold->connected != CON_GET_OLD_PASSWORD
    &&   !str_cmp(name, dold->original
             ? dold->original->name : dold->character->name))
    {
        write_to_buffer(d, "That character is already playing.\n\r",0);
        write_to_buffer(d, "Do you wish to connect anyway (Y/N)?",0);
        d->connected = CON_BREAK_CONNECT;
        return true;
    }
    }

    return false;
}


/**
 * stop_idling - Return an idle character from limbo to their previous room
 *
 * When a character idles too long, they are moved to limbo and their
 * previous location is saved in was_in_room. This function returns them
 * when they send any input. Handles:
 *   - Regular rooms
 *   - Instance/clone rooms (validates room ID matches)
 *   - Wilderness virtual rooms
 *   - Announces return to the room
 *
 * @param ch  The character who has stopped idling
 */
void stop_idling(CHAR_DATA *ch)
{
    if (ch == NULL ||
        ch->desc == NULL ||
        ch->desc->connected != CON_PLAYING ||
        ch->was_in_room == NULL ||
        ch->in_room != get_reserved_room_index("room_limbo"))
        return;

    if( ch->was_in_room_id[0] || ch->was_in_room_id[1] )
    {
        // If this is a clone room but isn't the same one...
        if( !ch->was_in_room->source ||
            ch->was_in_room->id[0] != ch->was_in_room_id[0] ||
            ch->was_in_room->id[1] != ch->was_in_room_id[1])
            return;

        ch->timer = 0;
        char_from_room(ch);
        char_to_room(ch, ch->was_in_room);

    }
    else if( ch->was_in_wilds )
    {
        ch->timer = 0;
        char_from_room(ch);
        char_to_vroom(ch, ch->was_in_wilds, ch->was_at_wilds_x, ch->was_at_wilds_y);
    }
    else
    {
        ch->timer = 0;
        char_from_room(ch);
        char_to_room(ch, ch->was_in_room);
    }

    ch->was_in_room = NULL;
    act("$n has returned from the void.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}


/**
 * send_to_char_bw - Send text to a character without color processing
 *
 * Writes text directly to the character's descriptor without any color
 * code interpretation. Used for raw output.
 *
 * @param txt  The text to send
 * @param ch   The character to send to
 */
void send_to_char_bw(const char *txt, CHAR_DATA *ch)
{
/*    write_to_buffer(ch->desc, txt, strlen(txt));*/
    if (txt != NULL && ch->desc != NULL)
        write_to_buffer(ch->desc, txt, strlen(txt));
}

/**
 * send_to_char - Send text to a character with color support
 *
 * Sends text to the character's descriptor. If the character has color
 * enabled (PLR_COLOUR), color codes are processed. Otherwise, color
 * codes are stripped using nocolour().
 *
 * Color version by Lope.
 *
 * @param txt  The text to send (may contain {x color codes)
 * @param ch   The character to send to
 */
void send_to_char( const char *txt, CHAR_DATA *ch )
{
    if ( txt != NULL && ch->desc != NULL )
    {
        if (IS_SET(ch->act[0], PLR_COLOUR))
        {
            write_to_buffer( ch->desc, txt, strlen(txt) );
        }
        else
        {
            write_to_buffer(ch->desc, nocolour(txt), strlen_no_colours(txt));
        }
    }
    return;
}
/*
void send_to_char(const char *txt, CHAR_DATA *ch)
{
    const	char 	*point;
            char 	*point2;
            char 	buf[ MAX_STRING_LENGTH*4 ];
        int	skip = 0;

    buf[0] = '\0';
    point2 = buf;

    
    if (!IS_NPC(ch) && IS_STONED(ch))
    {
    char colchar;
    int col;
    col = number_range(0, 12);

    *point2 = '{';
    point2++;
    switch(col) {
        case 0 :
        colchar = 'D';
        break;
        case 1 :
        colchar = 'R';
        break;
        case 2 :
        colchar = 'W';
        break;
        case 3 :
        colchar = 'G';
        break;
        case 4 :
        colchar = 'Y';
        break;
        case 5 :
        colchar = 'C';
        break;
        case 6 :
        colchar = 'B';
        break;
        case 7 :
        colchar = 'w';
        break;
        case 8 :
        colchar = 'y';
        break;
        case 9 :
        colchar = 'g';
        break;
        case 10 :
        colchar = 'b';
        break;
        case 11 :
        colchar = 'y';
        break;
        case 12 :
        colchar = 'M';
        break;
        default:
        colchar = 'W';
        break;
    }
    *point2 = colchar;
    point2++;
    *point2 = '\0';
    }
    

    if(txt && ch->desc)
    {
        bool capitalize = false;
        if(IS_SET(ch->act[0], PLR_COLOUR))
        {
            for(point = txt ; *point ; point++)
            {
                if(*point == '{')
                {
                    point++;

                    if( *point == '+' )
                        capitalize = true;
                    else {
                        skip = colour_new(*point, ch, point2);
                        point2 += skip;
                    }
                    continue;
                }

                if( capitalize && ISALPHA(*point) )
                {
                    *point2 = UPPER(*point);	// Make uppercase
                    capitalize = false;
                }
                else
                    *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer(ch->desc, buf, point2 - buf);
        }
        else
        {
            for(point = txt ; *point ; point++)
                {
                if(*point == '{')
                {
                    point++;
                    if( *point == '+' )
                        capitalize = true;

                    continue;
                }
                if( capitalize && ISALPHA(*point) )
                {
                    *point2 = UPPER(*point);	// Make uppercase
                    capitalize = false;
                }
                else
                    *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            write_to_buffer(ch->desc, buf, point2 - buf);
        }
    }
    return;
}
*/

/**
 * page_to_char_bw - Send pageable text to a character without color
 *
 * Sends text that can be paged through if it exceeds the character's
 * line limit. If lines == 0, sends all at once. Otherwise, uses the
 * pager (show_string) for "[Hit Return to continue]" prompts.
 *
 * @param txt  The text to page
 * @param ch   The character to send to
 */
void page_to_char_bw(const char *txt, CHAR_DATA *ch)
{
    if (txt == NULL || ch->desc == NULL)
    return;

    if (ch->lines == 0)
    {
    send_to_char_bw(txt,ch);
    return;
    }

    ch->desc->showstr_head = malloc(strlen(txt) + 1);
    strcpy(ch->desc->showstr_head,txt);
    ch->desc->showstr_point = ch->desc->showstr_head;
    show_string(ch->desc,"");
}

/**
 * page_to_char - Send pageable text to a character with color support
 *
 * Sends text that can be paged through if it exceeds the character's
 * line limit. If lines == 0, sends all at once. Otherwise, uses the
 * pager (show_string) for "[Hit Return to continue]" prompts.
 *
 * Color version by Lope.
 *
 * @param txt  The text to page (may contain color codes)
 * @param ch   The character to send to
 */
void page_to_char(const char *txt, CHAR_DATA *ch)
{
    if (txt == NULL || ch->desc == NULL)
    return;

    if (ch->lines == 0)
    {
    send_to_char(txt,ch);
    return;
    }

    ch->desc->showstr_head = malloc(strlen(txt) + 1);
    strcpy(ch->desc->showstr_head,txt);
    ch->desc->showstr_point = ch->desc->showstr_head;

    show_string(ch->desc,"");

}

/*
 * Page to one char, new colour version, by Lope.
 */
/*
void page_to_char(const char *txt, CHAR_DATA *ch)
{
    const	char	*point;
            char	*point2;
            char	*buf;
            char cbuf[20];
        int	skip = 0, len;

    if(txt && ch->desc)
    {
        bool capitalize = false;
        if(IS_SET(ch->act[0], PLR_COLOUR))
        {			
            for(point = txt, len = 1 ; *point ; point++)
                {
                if(*point == '{')
                {
                    point++;
                    if( *point != '+' )
                        len += colour(*point, ch, cbuf);
                    continue;
                }
                len++;
            }
            buf = malloc(len);
            buf[0] = '\0';
            point2 = buf;
            for(point = txt ; *point ; point++)
            {
                if(*point == '{')
                {
                    point++;
                    if( *point == '+')
                        capitalize = true;
                    else {
                        skip = colour(*point, ch, point2);
                        point2+=skip;
                    }
                    continue;
                }
                if( capitalize && ISALPHA(*point) )
                {
                    *point2 = UPPER(*point);	// Make uppercase
                    capitalize = false;
                }
                else
                    *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            ch->desc->showstr_head  = malloc(len);
            strcpy(ch->desc->showstr_head, buf);
            ch->desc->showstr_point = ch->desc->showstr_head;
            show_string(ch->desc, "");
            
        }
        else
        {
            len = strlen(txt) + 1;
            buf = malloc(len);
            buf[0] = '\0';
            point2 = buf;
            for(point = txt ; *point ; point++)
            {
                if(*point == '{')
                {
                    point++;
                    if( *point == '+')
                        capitalize = true;
                    continue;
                }
                if( capitalize && ISALPHA(*point) )
                {
                    *point2 = UPPER(*point);	// Make uppercase
                    capitalize = false;
                }
                else
                    *point2 = *point;
                *++point2 = '\0';
            }
            *point2 = '\0';
            ch->desc->showstr_head  = malloc(strlen(buf) + 1);
            strcpy(ch->desc->showstr_head, buf);
            ch->desc->showstr_point = ch->desc->showstr_head;
            show_string(ch->desc, "");
        }
        free(buf);
    }

}
*/


/**
 * show_string - Display paginated text one screen at a time
 *
 * Implements the string pager for long outputs. Shows lines up to the
 * character's configured line limit, then waits for input to continue.
 * Any non-empty input cancels the pager and frees the text.
 *
 * Respects the character's PLR_COLOUR setting for output.
 *
 * @param d      The descriptor showing the paged text
 * @param input  User input (empty to continue, non-empty to cancel)
 */
void show_string(struct descriptor_data *d, char *input)
{
    char *buffer;
    char buf[MAX_INPUT_LENGTH];
    register char *scan, *chk;
    int lines = 0, toggle = 1;
    int show_lines;

    one_argument(input,buf);
    if (buf[0] != '\0')
    {
        if (d->showstr_head)
        {
            free(d->showstr_head);
            d->showstr_head = 0;
        }
        d->showstr_point  = 0;
        return;
    }

    if( *d->showstr_point == '\r' )
        d->showstr_point++;

    int len = strlen(d->showstr_point);
    buffer = malloc(len + 1);

    if (d->character)
        show_lines = d->character->lines;
    else
        show_lines = 0;

    for (scan = buffer; ; scan++, d->showstr_point++)
    {
        if (((*scan = *d->showstr_point) == '\n' || *scan == '\r') && (toggle = -toggle) < 0)
            lines++;
        else if (!*scan || (show_lines > 0 && lines >= show_lines))
        {
            *scan = '\0';
            if (d->character && IS_SET(d->character->act[0], PLR_COLOUR))
                write_to_buffer(d,buffer,strlen(buffer));
            else
                write_to_buffer(d,nocolour(buffer),strlen_no_colours(buffer));

            for (chk = d->showstr_point; *chk && ISSPACE(*chk); chk++);

            if (!*chk)
            {
                if (d->showstr_head)
                {
                    free(d->showstr_head);
                    d->showstr_head = NULL;
                }
                d->showstr_point  = NULL;
            }

            free(buffer);
            return;
        }
    }
}


/**
 * act_new - Send formatted action message to characters in a room
 *
 * The core messaging function for actions in the game world. Formats a
 * message with variable substitution and sends it to appropriate recipients
 * based on the type parameter.
 *
 * Variable substitutions:
 *   $n/$N  - Actor/victim short name (visibility-aware)
 *   $$n    - Actor name (always visible, for high-level chars)
 *   $e/$E  - Actor/victim he/she/they
 *   $m/$M  - Actor/victim him/her/them
 *   $s/$S  - Actor/victim his/her/their
 *   $v     - Third character (vch2) name
 *   $z/$Z  - Actor/victim verb form
 *   $p/$P  - First/second object short description
 *   $t/$T  - String arguments (arg1/arg2)
 *   $d     - Door keyword from arg2
 *
 * Type values:
 *   TO_CHAR     - Send only to actor
 *   TO_VICT     - Send only to victim
 *   TO_ROOM     - Send to room except actor
 *   TO_NOTVICT  - Send to room except actor and victim
 *   TO_THIRD    - Send only to vch2
 *   TO_NOTTHIRD - Send to room except actor, victim, and vch2
 *   TO_FUNC     - Send to chars passing char_func test
 *   TO_NOTFUNC  - Send to chars failing char_func test
 *
 * Also triggers TRIG_ACT scripts on mobs and objects in the room.
 *
 * @param format     Format string with $ substitutions
 * @param ch         Actor character
 * @param vch        Victim character (optional)
 * @param vch2       Third character (optional)
 * @param ch_verb    Verb form for actor (used with $z)
 * @param vch_verb   Verb form for victim (used with $Z)
 * @param obj1       First object (optional)
 * @param obj2       Second object (optional)
 * @param arg1       First string argument (optional)
 * @param arg2       Second string argument (optional)
 * @param type       Recipient type (TO_CHAR, TO_ROOM, etc.)
 * @param min_pos    Minimum position to receive message
 * @param char_func  Filter function for TO_FUNC/TO_NOTFUNC (optional)
 */
void act_new(char *format, CHAR_DATA *ch,
        CHAR_DATA *vch, CHAR_DATA *vch2,
        const char *ch_verb, const char *vch_verb, /* These are already const char* */
        OBJ_DATA *obj1, OBJ_DATA *obj2,
        void *arg1, void *arg2,
        int type, int min_pos, CHAR_TEST char_func)
{



    CHAR_DATA 		*to;
    const 	char 	*str;
    const 	char 	*i = NULL;
    char 		*point;
    char 		buf[ MAX_STRING_LENGTH   ];
    char 		fname[ MAX_INPUT_LENGTH  ];
    bool		see_all;


    /*
     * Discard null and zero-length messages.
     */
    if (!format || !*format)
        return;

    /* discard null rooms and chars */
    if (!ch || !ch->in_room)
    return;

    to = ch->in_room->people;
    if (type == TO_VICT)
    {
        if (!vch)
        {
            log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: null vch with TO_VICT.");
            return;
        }

    if (!vch->in_room)
        return;

    to = vch->in_room->people;
    }

    for (; to ; to = to->next_in_room)
    {
    if ((!IS_NPC(to) && !to->desc )
    ||   (!IS_SWITCHED(to) && IS_NPC(to) && !HAS_TRIGGER_MOB(to, TRIG_ACT))
    ||    to->position < min_pos)
            continue;

        if ((type == TO_CHAR) && to != ch)
            continue;
        if (type == TO_VICT && (to != vch || to == ch))
            continue;
        if (type == TO_ROOM && to == ch)
            continue;
        if (type == TO_NOTVICT && (to == ch || to == vch))
            continue;
        if (type == TO_THIRD && (to != vch2))
            continue;
        if (type == TO_NOTTHIRD && (to == ch || to == vch || to == vch2))
            continue;
        /* NIB : 20070122 : Specifying a function test to determine who should see this*/
        if (type == TO_FUNC && (!char_func || !(*char_func)(ch,vch,to)))
        continue;
        if (type == TO_NOTFUNC && (!char_func || (*char_func)(ch,vch,to)))
        continue;

        point   = buf;
        str     = format;
        while (*str != '\0')
        {
            if (*str != '$')
            {
                *point++ = *str++;
                continue;
            }
        see_all = false;
            ++str;

            if( *str == '$' )
            {
                see_all = true;
                ++str;
            }

                switch (*str)
                {
                default:  log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code %d.", *str);
                          i = " <@@@> ";
                          break;
                /* Thx alex for 't' idea */
                case 't': if (arg1) i = (const char *) arg1; /* Cast to const char * */
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $t for 'arg1'");
                          break;
                case 'T': if (arg2) i = (const char *) arg2; /* Cast to const char * */
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $T for 'arg2'");
                          break;
                case 'v': if (vch2&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(vch2)))
                            i = ch->name; 
                          else
                            i = pers(vch2,  to ); 
                          }
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $v for 'vch2' or 'to'");
                          break;
                case 'n': if (ch&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(ch)))
                            i = ch->name;
                          else
                            i = pers(ch,  to );
                          }
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $n for 'ch' or 'to'");
                          break;
                case 'N': if (vch&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(vch)))
                            i = vch->name;
                          else
                            i = pers(vch,  to );
                          }
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $N for 'ch' or 'to'"); 
                          break;
                case 'e': if (ch) i = get_he_she(ch);
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $e for 'ch'");
                          break;
                case 'E': if (vch) i = get_he_she(vch); 
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $E for 'vch'");
                          break;
                case 'm': if (ch) i = get_him_her(ch); 
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $m for 'ch'");
                          break;
                case 'M': if (vch) i = get_him_her(vch);
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $M for 'vch'"); 
                          break;
                case 's': if (ch) i = get_his_her(ch); 
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $s for 'ch'");
                          break;
                case 'S': if (vch) i = get_his_her(vch); 
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $S for 'vch'"); 
                          break;
                /* ADDED NEW PRONOUN CASES - ensure get_his_hers and get_himself_herself are declared and defined */
                /* Assuming you might add these, for example:
                case 'f': // Reflexive: himself/herself
                    if (ch) i = get_himself_herself(ch);
                    else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $f for 'ch'");
                    break;
                case 'F': // Reflexive: himself/herself for vch
                    if (vch) i = get_himself_herself(vch);
                    else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $F for 'vch'");
                    break;
                case 'q': // Possessive Pronoun: his/hers
                    if (ch) i = get_his_hers(ch);
                    else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $q for 'ch'");
                    break;
                case 'Q': // Possessive Pronoun: his/hers for vch
                    if (vch) i = get_his_hers(vch);
                    else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $Q for 'vch'");
                    break;
                */
                case 'z': if (ch) i = ch_verb;
                            else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $z for 'ch'");
                            break;
                case 'Z': if (vch) i = vch_verb; 
                        else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $Z for 'vch'");
                        break;

                case 'p': if (to&&obj1) i = (see_all || can_see_obj(to, obj1))
                            ? obj1->short_descr  
                            : "something";       
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $p for 'to' or 'obj1'");
                    break;

                case 'P': if (to&&obj2) i = (see_all || can_see_obj(to, obj2))
                            ? obj2->short_descr
                            : "something";
                          else log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Act: bad code $P for 'to' or 'obj2'");
                    break;

                case 'd':
                    if (arg2 == NULL || ((char *) arg2)[0] == '\0') 
                    {
                        i = "door";
                    }
                    else
                    {
                        one_argument((char *) arg2, fname);
                        i = fname;
                    }
                    break;
                }

            ++str;
            if( i != NULL ) {
                while ((*point = *i) != '\0')
                    ++point, ++i;
            } else {

                if (point + 7 < buf + MAX_STRING_LENGTH) {
                    *point++ = '<'; *point++ = 'N'; *point++ = 'U'; *point++ = 'L'; *point++ = 'L'; *point++ = '>';
                } else {
                    
                }
            }
        }

        *point++ = '\n';
        *point++ = '\r';
    *point   = '\0';
        /*buf[0]   = UPPER(buf[0]);*/
        // Ensure upper_first does not write out of bounds or expect a non-const char* if buf is effectively const here.
        // If upper_first modifies in place and returns char*, it's fine.
        // sprintf(buf, "%s", upper_first(&buf[0])); // This is redundant if upper_first modifies in-place.
        // A direct call might be: upper_first(buf);
        // For safety, if upper_first returns a new buffer, ensure it's handled. Assuming it modifies in place:
        upper_first(buf); // Assuming upper_first modifies buf in place and handles its own safety.

    if (to->desc != NULL)
    {
            write_to_buffer(to->desc, buf, 0);
    }
    else
    if (MOBtrigger) 
        p_act_trigger(buf, to, NULL, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }

    if (MOBtrigger && (type == TO_ROOM || type == TO_NOTVICT))
    {
    OBJ_DATA *obj, *obj_next;
    CHAR_DATA *tch, *tch_next;
        ITERATOR it;


     point   = buf;
     str     = format; 
     while(*str != '\0' && (point - buf < MAX_STRING_LENGTH -1)) 
     {
         *point++ = *str++;
     }
     *point   = '\0'; 

    for(obj = ch->in_room->contents; obj; obj = obj_next)
    {
        obj_next = obj->next_content;
        p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }


    for(tch = ch->in_room->people; tch; tch = tch_next) 
    {
        tch_next = tch->next_in_room;

            iterator_start(&it, tch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
            }
            iterator_stop(&it);
            
            iterator_start(&it, tch->lworn);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
            }
            iterator_stop(&it);
    }

    p_act_trigger(buf, NULL, NULL, ch->in_room, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }
}

/*
int colour(char type, CHAR_DATA *ch, char *string)
{
    char code[20];
    char *p = '\0';

    if (!ch) {
        log_string("Char was null in colour.");
        return 0;
    }
 
    if(IS_NPC(ch) && !IS_SWITCHED(ch)) return(0);

    switch(type) {
    default: strcpy(code, CLEAR); break;
    case 'x': strcpy(code, CLEAR); break;
    case 'b': strcpy(code, C_BLUE); break;
    case 'c': strcpy(code, C_CYAN); break;
    case 'g': strcpy(code, C_GREEN); break;
    case 'm': strcpy(code, C_MAGENTA); break;
    case 'r': strcpy(code, C_RED); break;
    case 'w': strcpy(code, C_WHITE); break;
    case 'y': strcpy(code, C_YELLOW); break;
    case 'B': strcpy(code, C_B_BLUE); break;
    case 'C': strcpy(code, C_B_CYAN); break;
    case 'G': strcpy(code, C_B_GREEN); break;
    case 'M': strcpy(code, C_B_MAGENTA); break;
    case 'R': strcpy(code, C_B_RED); break;
    case 'W': strcpy(code, C_B_WHITE); break;
    case 'Y': strcpy(code, C_B_YELLOW); break;
    case 'D': strcpy(code, C_D_GREY); break;
    case '0': strcpy(code, C_BK_BLACK); break;
    case '1': strcpy(code, C_BK_BLUE); break;
    case '2': strcpy(code, C_BK_CYAN); break;
    case '3': strcpy(code, C_BK_GREEN); break;
    case '4': strcpy(code, C_BK_MAGENTA); break;
    case '5': strcpy(code, C_BK_RED); break;
    case '6': strcpy(code, C_BK_WHITE); break;
    case '7': strcpy(code, C_BK_YELLOW); break;
    case 'i': strcpy(code, "\033[5m"); break;
    case 'v': strcpy(code, "\033[7m"); break;
    case '{': strcpy(code, "{"); break;
    }

    p = code;
    while(*p)
        *string++ = *p++;
    *string = '\0';

    return(strlen(code));
}


void colourconv(char *buffer, const char *txt, CHAR_DATA *ch)
{
    const char *point;
    int skip = 0;
    bool capitalize = false;

    if(ch->desc && txt)
    {
    if(IS_SET(ch->act[0], PLR_COLOUR))
    
        {	
            write_to_buffer( ch->desc, txt, strlen(txt) );
        }
        else
        {
            write_to_buffer(ch->desc, nocolour(txt), strlen_no_colours(txt));
        }
    
    }
}
*/

/**
 * printf_to_char - Send formatted text to a character
 *
 * Convenience function combining sprintf and send_to_char.
 * Uses printf-style format string and variable arguments.
 *
 * @param ch   The character to send to
 * @param fmt  Printf-style format string
 * @param ...  Variable arguments for format
 */
void printf_to_char (CHAR_DATA * ch, char *fmt, ...)
{
    char buf[MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    send_to_char (buf, ch);
}


/**
 * stptok - String tokenizer with multiple break characters
 *
 * Extracts a token from a string, stopping at any character in brk.
 * Similar to strtok but doesn't modify the source string.
 *
 * @param s       Source string to tokenize
 * @param tok     Buffer to store extracted token
 * @param toklen  Size of token buffer
 * @param brk     String of break characters
 * @return        Pointer to next character after break, or NULL if done
 */
char *stptok(const char *s, char *tok, size_t toklen, char *brk)
{
    char *lim, *b;

    if (s == NULL)
    return NULL;

    if (!*s)
    return NULL;

    lim = tok + toklen - 1;
    while (*s && tok < lim)
    {
    for (b = brk; *b; b++)
    {
        if (*s == *b)
        {
        *tok = 0;
        for (++s, b = brk; *s && *b; ++b)
        {
            if (*s == *b)
            {
            ++s;
            b = brk;
            }
        }
        return (char *)s;
        }
    }
    *tok++ = *s++;
    }
    *tok = 0;
    return (char *)s;
}


/**
 * room_echo - Send a message to all players in a room
 *
 * Sends the message to all non-NPC characters in the specified room.
 *
 * @param pRoom    The room to echo to
 * @param message  The message to send
 */
void room_echo(ROOM_INDEX_DATA *pRoom, char *message)
{
    CHAR_DATA *ch;
    if(!pRoom || !message || !*message) return;
    for (ch = pRoom->people; ch != NULL; ch = ch->next_in_room)
    {
    if (!IS_NPC(ch))
     {
        send_to_char(message, ch);
    }
    }
}


/**
 * echo_around - Send a message to all rooms adjacent to a room
 *
 * Sends the message to all players in rooms connected by exits
 * to the specified room. Useful for distant sounds or effects.
 *
 * @param pRoom    The center room (message goes to adjacent rooms)
 * @param message  The message to send
 */
void echo_around(ROOM_INDEX_DATA *pRoom, char *message)
{
    EXIT_DATA *pexit;
    int16_t dir;

    for (dir = 0; dir < MAX_DIR; dir++)
    {
    if ((pexit = pRoom->exit[dir]) != NULL
        &&  pexit->u1.to_room != NULL)
    {
        room_echo(pexit->u1.to_room, message);
    }
    }
    return;
}


/**
 * show_form_state - Display group member health in battle prompt
 *
 * Shows the health status of all group members in the same room.
 * Displays name and health percentage with color coding:
 *   - Red: Below 50% health
 *   - Green: 50-67% health
 *   - Normal: Above 67% health
 *
 * Displayed when COMM_SHOW_FORM_STATE is set.
 *
 * @param ch  The character viewing the formation state
 */
void show_form_state(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    CHAR_DATA *fch;
    int i;

    sprintf(buf, "{x");

    i = 0;
    for (fch = ch->in_room->people; fch != NULL; fch = fch->next_in_room)
    {
    if (is_same_group(fch, ch) && fch != ch)
    {
        sprintf(buf2, "%s{Y[%s%.0f%%{x{Y] {x",
            IS_NPC(fch) ? fch->short_descr : fch->name,
        fch->hit < fch->max_hit / 2 ? "{R" :
            fch->hit < fch->max_hit/1.5 ? "{G" : "{x",
            (float) fch->hit/fch->max_hit * 100);

        /* cap first letter */
        if (i == 0)
        buf2[0] = UPPER(buf2[0]);

        strcat(buf, buf2);
        i++;
        if (i % 5 == 0)
        strcat(buf, "\n\r");
    }
    }

    buf[0] = UPPER(buf[0]);

    if (i > 0)
    strcat(buf, "\n\r");
    send_to_char(buf, ch);
}


/**
 * acceptablePassword - Validate password strength requirements
 *
 * Checks that a password meets minimum security requirements:
 *   - At least 5 characters long
 *   - Contains at least one lowercase letter
 *   - Contains at least one uppercase letter
 *   - Contains at least one digit
 *   - Does not contain tilde (~) character
 *
 * Sends appropriate error message to descriptor if validation fails.
 *
 * @param d     The descriptor (for error messages)
 * @param pass  The password to validate
 * @return      true if password is acceptable, false otherwise
 */
bool acceptablePassword(DESCRIPTOR_DATA *d, char *pass)
{
    bool lower = false;
    bool upper = false;
    bool number = false;
    char *p;

    if (strlen(pass) < 5)
    {
    write_to_buffer(d,
        "Password must be at least five characters long.\n\rPassword: ", 0);
    return false;
    }

    for (p = pass; *p != '\0'; p++)
    {
        if (*p >= 'a' && *p <= 'z')
        lower = true;

    if (*p >= 'A' && *p <= 'Z')
        upper = true;

    if (*p >= '0' && *p <= '9')
        number = true;
    }

    if (!lower)
    {
        write_to_buffer(d, "Password must contain a lowercase letter.\n\rPassword: ", 0);
    return false;
    }

    if (!upper)
    {
        write_to_buffer(d, "Password must contain an uppercase letter.\n\rPassword: ", 0);
    return false;
    }

    if (!number)
    {
        write_to_buffer(d, "Password must contain a number.\n\rPassword: ", 0);
    return false;
    }

    for (p = pass; *p != '\0'; p++)
    {
    if (*p == '~')
    {
        write_to_buffer(d,
        "New password not acceptable, try again.\n\rPassword: ",
        0);
        return false;
    }
    }

    return true;
}


/**
 * update_pc_timers - Decrement and process player action timers
 *
 * Called each pulse for connected players. Decrements various action
 * timers and triggers completion handlers when they reach zero:
 *   - daze: Stun/daze cooldown
 *   - cast: Spell casting (triggers cast_end)
 *   - bind: Wound binding (triggers bind_end)
 *   - bomb: Bomb making (triggers bomb_end)
 *   - bashed: Knocked down recovery (auto-stand)
 *   - resurrect: Resurrection spell (triggers resurrect_end)
 *   - brew: Potion brewing (triggers brew_end)
 *   - pk_timer: PvP cooldown
 *   - recite: Scroll recitation (triggers recite_end)
 *   - paroxysm: Paralysis effect
 *   - panic: Fear effect (triggers flee attempt)
 *   - repair: Item repair (triggers repair_end)
 *   - no_recall: Recall prevention
 *   - hide: Hiding attempt (triggers hide_end)
 *   - fade: Fading effect (triggers fade_end)
 *   - reverie: Meditation (triggers reverie_end)
 *   - trance: Deep meditation (triggers trance_end)
 *   - scribe: Scroll scribing (triggers scribe_end)
 *   - inking: Tattoo inking (triggers ink_end)
 *   - music: Playing music (triggers music_end)
 *   - script_wait: Script delay (triggers script_end_success/pulse)
 *   - ranged: Ranged aiming (triggers ranged_end)
 *   - hunting: Auto-hunt movement
 */
void update_pc_timers(CHAR_DATA *ch)
{
    if (ch != NULL && ch->daze > 0)
    --ch->daze;

    if (ch != NULL && ch->cast > 0)
    {
    --ch->cast;
    if (ch->cast <= 0)
        cast_end(ch);
     else if(ch->cast_token && IS_SET(ch->cast_token->pIndexData->flags, TOKEN_SPELLBEATS))
         p_percent_trigger(NULL, NULL, NULL, ch->cast_token, ch, NULL, NULL, NULL, NULL, TRIG_SPELLBEAT, NULL);
    }

    /* Decrease delay on characters binding */
    if (ch != NULL && ch->bind > 0)
    {
    --ch->bind;
    if (ch->bind <= 0)
        bind_end(ch);
    }

    /* Decrease delay on characters bomb making */
    if (ch != NULL && ch->bomb > 0)
    {
    --ch->bomb;
    if (ch->bomb <= 0)
        bomb_end(ch);
    }

    /* Decrease delay on characters who have been bashed.
     * This is so they stand up automatically and bash
     * isnt too powerful. */
    if (ch != NULL && ch->bashed > 0)
    {
    --ch->bashed;
    if (ch->bashed <= 0)
    {
        send_to_char("You scramble to your feet!\n\r", ch);
        if (ch->fighting != NULL)
        ch->position = POS_FIGHTING;
        else
        ch->position = POS_STANDING;
    }
    }

    if (ch != NULL && ch->resurrect > 0)
    {
    --ch->resurrect;
    if (ch->resurrect <= 0)
        resurrect_end(ch);
    }

    if (ch != NULL && ch->brew > 0)
    {
    --ch->brew;
    if (ch->brew <= 0) {
        brew_end(ch, ch->brew_sn);
        ch->brew_sn = 0;  /* NIB : 20070121 : Reset this for the ifchecks*/
    }
    }

    if (ch != NULL && ch->pk_timer> 0)
    {
    --ch->pk_timer;
    if (ch->pk_timer == 0) {
        ch->pk_timer = 0;
        send_to_char("You feel the dangerous blood aura fade away.\n\r", ch);
        act("The dangerous blood aura surrounding $n fades away.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    }

    if (ch != NULL && ch->recite > 0)
    {
    --ch->recite;
    if (ch->recite <= 0)
        recite_end(ch);
    }

    /* Decrease delay on characters in a paroxysm */
    if (ch != NULL && ch->paroxysm > 0)
    {
    --ch->paroxysm;
    if (ch->paroxysm <= 0)
    {
        send_to_char("You regain control of your motions as your paroxysm ends.\n\r", ch);
        ch->paroxysm = 0;
    }
    }

    /* Decrease delay on characters in a panic */
    if (ch != NULL && ch->panic > 0)
    {
    --ch->panic;
    if (ch->panic <= 0)
    {
        act("{RPANIC! You are overcome with FEAR and attempts to FLEE!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{R$n is overcome with FEAR and attempts to FLEE!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        do_function(ch, &do_flee, NULL);
        ch->panic = 0;
    }
    }

    /* Decrease delay on characters repairing */
    if (ch != NULL && ch->repair > 0)
    {
    --ch->repair;
    if (ch->repair <= 0)
        repair_end(ch);
    }

    if (ch != NULL && ch->no_recall > 0)
    --ch->no_recall;

    /* Decrease wait for hide */
    if (ch != NULL && ch->hide > 0)
    {
    --ch->hide;
    if (ch->hide <= 0)
        hide_end(ch);
    }

    /* Decrease wait for fade */
    if (ch != NULL && ch->fade > 0)
    {
    --ch->fade;
    if (ch->fade <= 0)
        fade_end(ch);
    }

    /* Decrease delay on characters in a reverie */
    if (ch != NULL && ch->reverie > 0)
    {
    --ch->reverie;
    if (ch->reverie <= 0)
        reverie_end(ch, ch->reverie_amount);
    }

    if (ch != NULL && ch->trance > 0)
    {
    --ch->trance;
    if (number_percent() < 4)
    {
        if (number_percent() > get_skill(ch, skill_resolve_gsn("deep trance")) - 10)
        {
        send_to_char("{YYou lose your meditative focus as something grabs your attention.{x\n\r", ch);
        act("{Y$n loses $s meditative focus as something grabs $s attention.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        ch->trance = 0;
        }
    }
    else if (ch->trance <= 0)
        trance_end(ch);
    }

    if (ch != NULL && ch->scribe > 0)
    {
    --ch->scribe;
    if (ch->scribe <= 0) {
        scribe_end(ch, ch->scribe_sn, ch->scribe_sn2, ch->scribe_sn3);
        ch->scribe_sn = 0;  /* NIB : 20070121 : Reset this for the ifchecks*/
    }
    }

    if (ch != NULL && ch->inking > 0)
    {
    --ch->inking;
    if (ch->inking <= 0) {
        ink_end(ch, ch->ink_target, ch->ink_loc, ch->ink_sn, ch->ink_sn2, ch->ink_sn3);
        ch->ink_sn = 0;
    }
    }

    if (ch != NULL && ch->music > 0)
    {
        --ch->music;
        if (ch->music <= 0)
            music_end(ch);
    }


    if( ch != NULL && ch->script_wait > 0)
    {
        //printf_to_char(ch, "script_wait: %d\n\r", ch->script_wait);
        --ch->script_wait;
        if (ch->script_wait <= 0)
            script_end_success(ch);
        else
            script_end_pulse(ch);
    }


    if (ch != NULL && ch->ranged > 0)
    {
    --ch->ranged;
    if (ch->fighting != NULL && number_percent() > get_ranged_skill(ch) + get_curr_stat(ch, STAT_DEX))
    {
        send_to_char("You lose your aim and lower your weapon.\n\r", ch);
        ch->ranged = 0;
    }
    else
    if (ch->ranged <= 0)
        ranged_end(ch);
    }

    /* Update autohunt, move towards target*/
    if (ch != NULL && ch->hunting != NULL)
    {
    if (number_percent() < (2 + 9 * get_skill(ch, skill_resolve_gsn("hunt"))/100))
        update_hunting_pc(ch);
    }
}


/**
 * add_possible_races - Append available race names to a string
 *
 * Appends a formatted list of all races available for character creation,
 * including starting races and any account-unlocked races.
 *
 * @param account The account creating the character (for unlock checks)
 * @param string  The string to append race list to
 */
void add_possible_races(ACCOUNT_DATA *account, char *string)
{
    char buf[MSL];
    RACE_DATA *race;
    bool found = false;

    sprintf(buf, " {B[{C");
    for (race = race_list; race; race = race->next)
    {
        if (race_available_for_creation(race, account))
        {
            if (found)
                strcat(buf, " ");

            found = true;
            strcat(buf, race->name);
        }
    }

    strcat(buf, "{B]{x");

    strcat(string, buf);
}


/**
 * add_possible_subclasses - Append available subclass names to a string
 *
 * Appends a formatted list of non-remort subclasses that match the
 * character's current class and alignment. Used during character creation
 * to show valid subclass choices.
 *
 * @param ch      The character being created
 * @param string  The string to append subclass list to
 */
void add_possible_subclasses(CHAR_DATA *ch, char *string)
{
    char buf[MSL];
    int i;
    int count;
    int align = ALIGN_NONE;

    if (ch->alignment < 0)
    align = ALIGN_EVIL;

    if (ch->alignment > 0)
    align = ALIGN_GOOD;


    strcat(string, "{B[{C");

    count = 0;
    for (i = 0; i < MAX_SUB_CLASS; i++)
    {
    CLASS_DATA *sc = class_from_legacy(0, i);
    if (!sc) continue;
    if (!(sc->flags & CLASS_REMORT_ONLY)
    &&  ch->pcdata->class_current == sub_class_legacy_type(i))
    {
        if ((align == ALIGN_GOOD && sub_class_legacy_alignment(i) == ALIGN_EVIL)
            ||  (align == ALIGN_EVIL && sub_class_legacy_alignment(i) == ALIGN_GOOD))
        continue;

            count++;

        if (count > 1)
        strcat(string, " ");

        sprintf(buf, "%s", class_display_ch(sc, ch));
        buf[0] = UPPER(buf[0]);
        strcat(string, buf);
    }
    }

    strcat(string, "{B]{x");
}


/**
 * connection_add - Add a descriptor to connection tracking lists
 *
 * Adds the descriptor to the appropriate tracking lists based on the
 * character type:
 *   - conn_immortals: For immortal characters
 *   - conn_players: For mortal characters
 *   - conn_online: For all connected characters
 *
 * Uses the original character if switched (possessed mob).
 *
 * @param d  The descriptor to add
 */
void connection_add(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch;

    if(d) {
        ch = d->original ? d->original : d->character;

        if(ch && !IS_NPC(ch)) {
            if(IS_IMMORTAL(ch))
                list_addlink(conn_immortals, d);
            else
                list_addlink(conn_players, d);
            list_addlink(conn_online, d);
        }
    }
}

/**
 * connection_remove - Remove a descriptor from connection tracking lists
 *
 * Removes the descriptor from the tracking lists it was added to by
 * connection_add(). Called during disconnect.
 *
 * @param d  The descriptor to remove
 */
void connection_remove(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch;

    if(d) {
        ch = d->original ? d->original : d->character;

        if(ch && !IS_NPC(ch)) {
            if(IS_IMMORTAL(ch))
                list_remlink(conn_immortals, d, false);
            else
                list_remlink(conn_players, d, false);
            list_remlink(conn_online, d, false);
        }
    }
}

/**
 * init_ssl_cleanup_queue - Initialize the SSL context cleanup queue
 *
 * Creates the list used to track old SSL contexts awaiting cleanup.
 * Must be called during boot sequence before any SSL contexts are created.
 *
 * @note Exits the process on failure (memory allocation error).
 */
void init_ssl_cleanup_queue(void)
{
    ssl_ctx_cleanup_queue = list_create(false);
    if (!ssl_ctx_cleanup_queue) {
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Could not create SSL cleanup queue");
        exit(1);
    }
}

/**
 * SSL_CLEANUP_DATA - Structure for tracking SSL contexts pending cleanup
 */
typedef struct ssl_cleanup_data {
    SSL_CTX *ctx;        /**< The SSL context to be freed */
    time_t time_added;   /**< When the context was added to cleanup queue */
} SSL_CLEANUP_DATA;

/**
 * add_ssl_ctx_to_cleanup - Queue an old SSL context for deferred cleanup
 *
 * Adds an SSL context to the cleanup queue instead of freeing it immediately.
 * This allows existing connections using the old context to complete naturally
 * before the context is freed.
 *
 * @param old_ctx  The SSL context to queue for cleanup
 */
void add_ssl_ctx_to_cleanup(SSL_CTX *old_ctx)
{
    if (!old_ctx)
        return;

    SSL_CLEANUP_DATA *data;

    data = (SSL_CLEANUP_DATA *)malloc(sizeof(SSL_CLEANUP_DATA));
    data->ctx = old_ctx;
    data->time_added = current_time;

    list_appendlink(ssl_ctx_cleanup_queue, data);
    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "SSL context added to cleanup queue");
}

/**
 * process_ssl_cleanup_queue - Free old SSL contexts that have aged out
 *
 * Iterates through the cleanup queue and frees any SSL contexts that have
 * been queued for more than 5 minutes. This ensures no active connections
 * are still using the context before it's freed.
 *
 * Called periodically from the game loop.
 */
void process_ssl_cleanup_queue(void)
{
    SSL_CLEANUP_DATA *data;
    ITERATOR it;

    if (list_size(ssl_ctx_cleanup_queue) == 0)
        return;

    iterator_start(&it, ssl_ctx_cleanup_queue);
    while ((data = (SSL_CLEANUP_DATA *)iterator_nextdata(&it))) {
        // Wait 5 minutes before freeing contexts to ensure no active connections
        if (current_time - data->time_added > 300) {
            // Safe to free this context now
            SSL_CTX_free(data->ctx);
            list_remlink(ssl_ctx_cleanup_queue, data, true);
            log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "Freed old SSL context from cleanup queue");
        }
    }
    iterator_stop(&it);
}

/**
 * refresh_ssl_context - Reload SSL certificates and create fresh context
 *
 * Creates a new SSL context with reloaded certificates, replacing the old
 * one. The old context is queued for deferred cleanup. Triggered by:
 *   - Hourly automatic refresh
 *   - SSL error circuit breaker (5+ errors since last refresh)
 *
 * This allows certificate updates without server restart and helps recover
 * from transient SSL errors.
 */
void refresh_ssl_context(void)
{
    static time_t last_refresh = 0;

    // Refresh once per hour by default, or when circuit breaker triggers
    if (current_time - last_refresh < 3600 && ssl_errors_since_reset < 5)
        return;

    log_message(LOG_LEVEL_INFO, LOG_INFO, "Refreshing SSL context...");

    // Create new context
    SSL_CTX *new_ctx = create_context();
    if (!new_ctx) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to create new SSL context");
        return;
    }

    // Configure the new context
    if (!configure_context(new_ctx)) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to configure new SSL context");
        SSL_CTX_free(new_ctx);
        return;
    }

    // Store the old context for cleanup
    SSL_CTX *old_ctx = ctx;

    // Replace the old context
    ctx = new_ctx;

    // Add the old context to the cleanup queue
    if (old_ctx)
        add_ssl_ctx_to_cleanup(old_ctx);

    last_refresh = current_time;
    ssl_errors_since_reset = 0;
    log_message(LOG_LEVEL_INFO, LOG_INFO, "SSL context refreshed successfully");
}

