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

/*
 * This file contains all of the OS-dependent stuff:
 *   startup, signals, BSD sockets for tcp/ip, i/o, timing.
 *
 * The data flow for input is:
 *    Game_loop ---> Read_from_descriptor ---> Read
 *    Game_loop ---> Read_from_buffer
 *
 * The data flow for output is:
 *    Game_loop ---> Process_Output ---> Write_to_descriptor -> Write
 *
 * The OS-dependent functions are Read_from_descriptor and Write_to_descriptor.
 * -- Furey  26 Jan 1993
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
#include <zlib.h>
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



/*
 * Global variables.
 */
bool			is_test_port;
int 		    telnet_port;
int				tls_port;
GLOBAL_DATA         gconfig;		/* Vizz - UID Tracking, and any other persistent global config info */
GAME_SETTINGS_DATA  game_settings;
LLIST *conn_players;
LLIST *conn_immortals;
LLIST *conn_online;
DESCRIPTOR_DATA *   descriptor_list;	/* All open descriptors		*/
DESCRIPTOR_DATA *   d_next;		/* Next descriptor in loop	*/
FILE *		    fpReserve;		/* Reserved file handle		*/
bool		    god;		/* All new chars are gods!	*/
bool		    merc_down;		/* Shutdown			*/
bool		    wizlock;		/* Game is wizlocked		*/
bool		    newlock;		/* Game is newlocked		*/
char		    str_boot_time[MAX_INPUT_LENGTH];
time_t		    current_time;	/* time of this pulse */
time_t			stats_load_time;
bool		    MOBtrigger = true;  /* act() switch                 */
LLIST *loaded_areas;
SSL_CTX *ctx;
int ssl_errors_since_reset = 0;
time_t last_ssl_error = 0;
LLIST *ssl_ctx_cleanup_queue = NULL;
static unsigned char crypto_key[AES_KEY_SIZE]; // Server-side key
static bool key_initialized = false;

/*
 * OS-dependent local functions.
 */
void	game_loop		args((int control_telnet, int control_tls));
int	init_socket		args((int port));
int init_tls_socket	args((int port));
void	init_descriptor		args((int control, bool is_tls));
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
bool	process_output		args((DESCRIPTOR_DATA *d, bool fPrompt));
void	read_from_buffer	args((DESCRIPTOR_DATA *d));
void	stop_idling		args((CHAR_DATA *ch));
void    bust_a_prompt           args((CHAR_DATA *ch));
bool acceptablePassword(DESCRIPTOR_DATA *d, char *pass);
void add_possible_subclasses(CHAR_DATA *ch, char *string);
void add_possible_races(CHAR_DATA *ch, char *string);


#define MAX_LOGFILE		1000000
char logfile_std[MIL];
char logfile_err[MIL];


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

static void RedirectOutput(void)
{
	RedirectSTDOUT();
	RedirectSTDERR();
}


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


static void CleanupLogs(void)
{
	CleanupSTDOUT();
	CleanupSTDERR();
}

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
		else if ( argv[i][0] == '-' && (strlen(argv[i]) == 2) )
		{
			switch( argv[i][1] )
			{

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


int main(int argc, char **argv)
{
    
    struct timeval now_time;
    int control_telnet = 0;
	int control_tls = 0;
    ITERATOR iter;
    void *data;
	static GAME_SETTINGS_DATA game_settings_zero;

    /*
     * Memory debugging if needed.
     */
#if defined(MALLOC_DEBUG)
    malloc_debug(2);
#endif

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
    if ((fpReserve = fopen(NULL_FILE, "r")) == NULL)
    {
	perror(NULL_FILE);
	exit(1);
    }

	game_settings = game_settings_zero;
	if (game_settings_read()==1) exit(1);
	log_string("Global game settings loaded.");

    /*
     * Get the port number.
     */
	if (game_settings.telnet_port)
		telnet_port = game_settings.telnet_port;
	if (game_settings.tls_port)
		tls_port = game_settings.tls_port;
	if (game_settings.testport || game_settings.dev_server)
    	is_test_port = true;
	
	if (game_settings.dev_server)
		{
			newlock = true;
			wizlock = true;
		}

    if( !parse_options(argc, argv) )
    {
		fprintf(stderr, "Usage: %s [port #] [-NTW]\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "\tport #\tListening port for the server (>1024).  Default is 9000.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-N\tStart up with newlock active.\n");
		fprintf(stderr, "\t-T\tStart up in Test Port mode.\n");
		fprintf(stderr, "\t-W\tStart up with wizlock active.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-?\tShow this screen.\n");
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

    RedirectOutput();

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

	if (game_settings.enable_telnet)
	{
    	control_telnet = init_socket(telnet_port);
		sprintf(log_buf, "Telnet socket bound to port %d.", telnet_port);
		log_string(log_buf);
	}
	if (game_settings.enable_tls && game_settings.tls_port)
	{
		control_tls = init_tls_socket(tls_port);
		sprintf(log_buf, "TLS socket bound to port %d.", tls_port);
		log_string(log_buf);
	}

    boot_db();

    sprintf(log_buf, "Sentience is up on %d.", telnet_port);
    log_string(log_buf);
    game_loop(control_telnet, control_tls);
	list_destroy(conn_players);
	list_destroy(conn_immortals);
	list_destroy(conn_online);
	list_destroy(loaded_chars);
	// Temporarily disabling for reconnect crash.
	//list_destroy(loaded_players);
	list_destroy(loaded_objects);
	list_destroy(persist_mobs);
	list_destroy(persist_objs);
	list_destroy(persist_rooms);
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
        plogf("comm.c, main(): Failed to write our gconfig.rc file!");
        plogf("                Current UID's are:");
        plogf("                                   NextAreaUID:	%ld", gconfig.next_area_uid);
        plogf("                                   NextWildsUID:	%ld", gconfig.next_wilds_uid);
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
    log_string("Normal termination of game.");

    CleanupLogs();

    exit(0);
    return 0;
}

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

void game_loop(int control_telnet, int control_tls)
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
                    bug("Invalid file descriptor passed to Select()", 0);
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                case EINTR:
                    bug("A non-blocked signal was caught.", 0);
                    break;
                case EINVAL:
                    bug("Negative 'n' descriptor passed to Select()", 0);
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                case ENOMEM:
                    bug("Select() was unable to allocate memory for internal tables.", 0);
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
                default:
                    bug("Unknown error.", 0);
                    perror("Game_loop: select: poll");
                    exit(1);
                    break;
            }
        }

        /*
         * New connections?
         */
        if (control_telnet != -1 && FD_ISSET(control_telnet, &in_set))
            init_descriptor(control_telnet, false);
        
        if (control_tls != -1 && FD_ISSET(control_tls, &in_set))
            init_descriptor(control_tls, true);

        /*
         * Process outstanding TLS handshakes first
         */
        for (d = descriptor_list; d != NULL; d = d_next)
        {
            d_next = d->next;
            
            // Skip descriptors without SSL or that aren't in handshake mode
            if (!d->ssl || !d->tls_handshake_in_progress)
                continue;
            
            // Check for handshake timeouts
            if (current_time - d->last_activity > 10) {
                log_string("Closing stalled TLS handshake connection");
                close_socket(d);
                continue;
            }
            
            // Process handshakes ready for activity
            if (FD_ISSET(d->descriptor, &in_set) || FD_ISSET(d->descriptor, &out_set)) {
                int ret = SSL_accept(d->ssl);
                if (ret == 1) {
                    // Handshake completed successfully
                    d->tls_handshake_in_progress = false;
                    d->last_activity = current_time;
                    
                    // Log successful handshake when debugging
                    if (game_settings.dev_server)
                        log_string("TLS handshake completed successfully");
                } 
                else if (ret <= 0) {
                    int err = SSL_get_error(d->ssl, ret);
                    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                        // Fatal handshake error - clean up properly
                        BIO *bio = BIO_new(BIO_s_mem());
                        ERR_print_errors(bio);
                        
                        char ssl_err_buf[MAX_STRING_LENGTH];
                        char *bio_data;
                        long bio_len = BIO_get_mem_data(bio, &bio_data);
                        
                        if (bio_len >= MAX_STRING_LENGTH)
                            bio_len = MAX_STRING_LENGTH - 1;
                        memcpy(ssl_err_buf, bio_data, bio_len);
                        ssl_err_buf[bio_len] = '\0';
                        BIO_free(bio);
                        
                        sprintf(log_buf, "TLS handshake failed: %d\nSSL errors: %s", 
                                err, ssl_err_buf);
                        log_string(log_buf);
                        
                        // Update circuit breaker
                        ssl_errors_since_reset++;
                        last_ssl_error = current_time;
                        
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
            
            // Close connections with stalled TLS handshakes (10 seconds)
            if (d->ssl && d->tls_handshake_in_progress && 
                current_time - d->last_activity > 10) {
                log_string("Closing stalled TLS handshake connection");
                close_socket(d);
                continue;
            }
            
            // Only timeout normal connections if not fully logged in
            if ((d->connected == CON_GET_ACCOUNT_NAME || d->connected == CON_GET_OLD_PASSWORD) && 
                current_time - d->last_activity > 120 && 
                !d->healthcheck) {
                log_string("Closing idle connection (timeout).");
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
	    		bug ("Invalid file descriptor passed to Select()", 0);
	    		perror("Game_loop: select: stall");
			    exit(1);
	    		break;
			case EINTR:	bug("A non-blocked signal was caught.", 0);
		    	break;
			case EINVAL:
	    		bug ("Negative \'n\' descriptor passed to Select()", 0);
	    		perror("Game_loop: select: stall");
			    exit(1);
	    		break;
			case ENOMEM:
	    		bug ("Select() was unable to allocate memory for internal tables.", 0);
	    		perror("Game_loop: select: stall");
			    exit(1);
	    		break;
			default:
	    		bug ("Unknown error.", 0);
	    		perror("Game_loop: select: stall");
			    exit(1);
	    		break;
			}
/*	    	perror("Game_loop: select: stall");*/
/*		    exit(1);*/
		}
            }
        }

	// Garbate collect

	/* Check to see if the logfiles have overflowed*/
	check_logfile();

	gettimeofday(&last_time, NULL);
	current_time = (time_t) last_time.tv_sec;
    }
}


void init_descriptor(int control, bool is_tls)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *dnew = NULL;
    struct sockaddr_in sock;
    struct hostent *from;
    int desc;
    socklen_t size;

    size = sizeof(sock);
    getsockname(control, (struct sockaddr *) &sock, &size);
    if ((desc = accept(control, (struct sockaddr *) &sock, &size)) < 0)
    {
        perror("New_descriptor: accept");
        return;
    }

#if !defined(FNDELAY)
#define FNDELAY O_NDELAY
#endif

    if (fcntl(desc, F_SETFL, FNDELAY) == -1)
    {
        perror("New_descriptor: fcntl: FNDELAY");
        close(desc);
        return;
    }

    dnew = new_descriptor();
    dnew->last_activity = current_time;
    dnew->healthcheck = false;

    if (is_tls) {
        dnew->ssl = SSL_new(ctx);
        dnew->tls_handshake_in_progress = true;
        if (dnew->ssl == NULL) {
            // Create a memory BIO to capture OpenSSL errors
            BIO *bio = BIO_new(BIO_s_mem());
            ERR_print_errors(bio);
            
            // Extract the error messages to a buffer
            char ssl_err_buf[MAX_STRING_LENGTH];
            char *bio_data;
            long bio_len = BIO_get_mem_data(bio, &bio_data);
            
            // Copy and null-terminate the error data
            if (bio_len >= MAX_STRING_LENGTH)
                bio_len = MAX_STRING_LENGTH - 1;
            memcpy(ssl_err_buf, bio_data, bio_len);
            ssl_err_buf[bio_len] = '\0';
            BIO_free(bio);
            
            // Update circuit breaker counters
            ssl_errors_since_reset++;
            last_ssl_error = current_time;
            
            sprintf(log_buf, "New_descriptor: SSL_new failed\nSSL errors: %s", ssl_err_buf);
            bug(log_buf, 0);
            
            close(desc);
            free_descriptor(dnew);
            return;
        }

        if (SSL_set_fd(dnew->ssl, desc) == 0) {
            // Create a memory BIO to capture OpenSSL errors
            BIO *bio = BIO_new(BIO_s_mem());
            ERR_print_errors(bio);
            
            // Extract the error messages to a buffer
            char ssl_err_buf[MAX_STRING_LENGTH];
            char *bio_data;
            long bio_len = BIO_get_mem_data(bio, &bio_data);
            
            // Copy and null-terminate the error data
            if (bio_len >= MAX_STRING_LENGTH)
                bio_len = MAX_STRING_LENGTH - 1;
            memcpy(ssl_err_buf, bio_data, bio_len);
            ssl_err_buf[bio_len] = '\0';
            BIO_free(bio);
            
            // Update circuit breaker counters
            ssl_errors_since_reset++;
            last_ssl_error = current_time;
            
            sprintf(log_buf, "New_descriptor: SSL_set_fd failed\nSSL errors: %s", ssl_err_buf);
            bug(log_buf, 0);
            
            SSL_free(dnew->ssl);
            dnew->ssl = NULL;
            close(desc);
            free_descriptor(dnew);
            return;
        }

        int ret = SSL_accept(dnew->ssl);
        if (ret <= 0) {
            int err = SSL_get_error(dnew->ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                dnew->tls_handshake_in_progress = true;
            } else {
                // Create a memory BIO to capture OpenSSL errors
                BIO *bio = BIO_new(BIO_s_mem());
                ERR_print_errors(bio);
                
                // Extract the error messages to a buffer
                char ssl_err_buf[MAX_STRING_LENGTH];
                char *bio_data;
                long bio_len = BIO_get_mem_data(bio, &bio_data);
                
                // Copy and null-terminate the error data
                if (bio_len >= MAX_STRING_LENGTH)
                    bio_len = MAX_STRING_LENGTH - 1;
                memcpy(ssl_err_buf, bio_data, bio_len);
                ssl_err_buf[bio_len] = '\0';
                BIO_free(bio);
                
                // Update circuit breaker counters
                ssl_errors_since_reset++;
                last_ssl_error = current_time;
                
                sprintf(log_buf, "TLS handshake failed with error: %d\nSSL errors: %s\nSSL state: %s",
                        err, ssl_err_buf, SSL_state_string_long(dnew->ssl));
                log_string(log_buf);
                
                SSL_free(dnew->ssl);
                dnew->ssl = NULL;
                close(desc);
                free_descriptor(dnew);
                return;
            }
        } else {
            dnew->tls_handshake_in_progress = false;
        }
    } else {
        dnew->ssl = NULL;
    }

    dnew->descriptor = desc;
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
            sprintf(log_buf, "Sock.sinaddr:  %s", buf);
            log_string(log_buf);
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
    ProtocolNegotiate(dnew);

    write_to_buffer(dnew, compress_will, 0);

    if (help_greeting[0] == '.')
        write_to_buffer(dnew, help_greeting + 1, 0);
    else
        write_to_buffer(dnew, help_greeting, 0);

    if (!is_tls && game_settings.enable_tls && game_settings.enable_insecure_warning && game_settings.insecure_warning_msg != NULL)
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


void close_socket(DESCRIPTOR_DATA *dclose)
{
    CHAR_DATA *ch;

    if (dclose->outtop > 0)
	process_output(dclose, false);

    if ((ch = dclose->character) != NULL)
    {
		sprintf(log_buf, "Closing link to %s.", ch->name);
		log_string(log_buf);
		/* cut down on wiznet spam when rebooting */
			if (dclose->connected == CON_PLAYING && !merc_down)
			{
	    		if (ch->invis_level < STAFF_IMMORTAL)
					act("$n has lost $s link.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				wiznet("$N has lost $S link.",ch,NULL,WIZ_LINKS,0,0);

	    		ch->desc = NULL;
			}
			else
			{
	    		free_char(dclose->original ? dclose->original :
				dclose->character);
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
	    	bug("Close_socket: dclose not found.", 0);
    }

    if (dclose->out_compress) {
        deflateEnd(dclose->out_compress);
        free_mem(dclose->out_compress_buf, COMPRESS_BUF_SIZE);
        free_mem(dclose->out_compress, sizeof(z_stream));
    }

    ProtocolDestroy(dclose->pProtocol);

    // Properly shut down TLS/SSL connection with complete error handling
    if (dclose->ssl != NULL) {
        int ret, err;
        
        // Only attempt graceful shutdown if not in handshake mode
        if (!dclose->tls_handshake_in_progress) {
            ret = SSL_shutdown(dclose->ssl);
            
            // If SSL_shutdown returns 0, it means we've sent close_notify but haven't 
            // received one back - one more call is needed for a complete shutdown
            if (ret == 0) {
                // Second call to complete bidirectional shutdown
                SSL_shutdown(dclose->ssl);
            } 
            else if (ret < 0) {
                // Handle shutdown errors to prevent SSL context corruption
                err = SSL_get_error(dclose->ssl, ret);
                if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                    // Create memory BIO to capture OpenSSL errors
                    BIO *bio = BIO_new(BIO_s_mem());
                    ERR_print_errors(bio);
                    
                    // Extract the error messages to a buffer
                    char ssl_err_buf[MAX_STRING_LENGTH];
                    char *bio_data;
                    long bio_len = BIO_get_mem_data(bio, &bio_data);
                    
                    // Copy and null-terminate the error data
                    if (bio_len >= MAX_STRING_LENGTH)
                        bio_len = MAX_STRING_LENGTH - 1;
                    memcpy(ssl_err_buf, bio_data, bio_len);
                    ssl_err_buf[bio_len] = '\0';
                    BIO_free(bio);
                    
                    // Log the SSL error
                    sprintf(log_buf, "SSL_shutdown error: %d\nSSL errors: %s", 
                            err, ssl_err_buf);
                    log_string(log_buf);
                }
            }
        }
        
        // Always free the SSL object
        SSL_free(dclose->ssl);
        dclose->ssl = NULL;
    }

	if (dclose->account) {
    	dclose->account->refcount--;
    	if (dclose->account->refcount <= 0) {
        	list_remlink(loaded_accounts, dclose->account, false);
        	free_account(dclose->account);
    	}
    	dclose->account = NULL;
	}
    
	// Gracefully shut down the socket before closing to avoid lingering FIN_WAIT2
    shutdown(dclose->descriptor, SHUT_RDWR);
    close(dclose->descriptor);

    free_descriptor(dclose);
    return;
}


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
        sprintf(log_buf, "%s input overflow!", d->host);
        log_string(log_buf);
        write_to_descriptor(d, "\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
        return false;
    }

    for (;;)
    {
        int nRead = 0;
        if (d->ssl)
        {
            do {
                nRead = SSL_read(d->ssl, read_buf + iStart, sizeof(read_buf) - 10 - iStart);
                if (nRead <= 0) {
                    int err = SSL_get_error(d->ssl, nRead);
                    if (err == SSL_ERROR_WANT_READ) {
                        break;
                    } else if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
                        return false;
                    } else {
                        // Create a memory BIO to capture OpenSSL errors
                        BIO *bio = BIO_new(BIO_s_mem());
                        ERR_print_errors(bio);
                        
                        // Extract the error messages to a buffer
                        char ssl_err_buf[MAX_STRING_LENGTH];
                        char *bio_data;
                        long bio_len = BIO_get_mem_data(bio, &bio_data);
                        
                        // Copy and null-terminate the error data
                        if (bio_len >= MAX_STRING_LENGTH)
                            bio_len = MAX_STRING_LENGTH - 1;
                        memcpy(ssl_err_buf, bio_data, bio_len);
                        ssl_err_buf[bio_len] = '\0';
                        BIO_free(bio);
                        
                        // Update circuit breaker counters
                        ssl_errors_since_reset++;
                        last_ssl_error = current_time;
                        
                        // Log using the standard pattern
                        sprintf(log_buf, "SSL_read failed with error: %d\nSSL errors: %s\nSSL state: %s", 
                                err, ssl_err_buf, SSL_state_string_long(d->ssl));
                        bug(log_buf, 0);
                        
                        return false;
                    }
                }
            } while (nRead <= 0);
        }
        else
        {
            nRead = read(d->descriptor, read_buf + iStart, sizeof(read_buf) - 10 - iStart);
        }
        
        if (nRead > 0)
        {
            read_buf[nRead] = '\0';
            iStart += nRead;
            
            // Check for health check (both TLS and non-TLS)
            if (strncmp(read_buf, "HEALTH_CHECK", 12) == 0)
            {
                // Mark as health check
                d->healthcheck = true;
                
                // Send quick response
                const char *response = "OK\r\n";
                if (d->ssl)
                    SSL_write(d->ssl, response, strlen(response));
                else
                    write(d->descriptor, response, strlen(response));
                
                // Don't process further - close socket in next game loop
                return true;
            }
            
            if (read_buf[iStart - 1] == '\n' || read_buf[iStart - 1] == '\r')
                break;
        }
        else if (nRead == 0)
        {
            return false;
        }
        else if (errno == EWOULDBLOCK)
            break;
        else
        {
            perror("Read_from_descriptor");
            return false;
        }
    }

    read_buf[iStart] = '\0';
    ProtocolInput(d, read_buf, iStart, d->inbuf);
    return true;
}


/*
 * Transfer one line from input buffer to input line.
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
		sprintf(log_buf, "%s input spamming!", d->host);
		log_string(log_buf);

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


/*
 * Low level output function.
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


/*
 * Bust a prompt (player settable prompt)
 * coded by Morgenes for Aldara Mud
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

	if (!IS_NPC(ch) && ch->pcdata->mfa_question)
	{
		send_to_char("{YMFA Code:{X\n\r", ch);
		return;
	}

    if (ch->pk_question || ch->remove_question)
    {
	send_to_char("{Y({xY{R/{xN{Y){x\n\r", ch);
	return;
    }

    if( ch->remort_question )
    {
		send_to_char("{YSelect your first remort class:{x\n\r", ch);
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

    if (IS_MORPHED(ch) && IS_VAMPIRE(ch))
	send_to_char("{G[{YSHAPED{G]{x ", ch);

    if (IS_SHIFTED(ch))
	send_to_char("{G[{YSHIFTED{G]{x ", ch);

    if (IS_IMMORTAL(ch) && count_project_inquiries(ch) > 0)
	send_to_char("{g[{GINQUIRY{g]{x ", ch);

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
		exp_per_level(ch,ch->pcdata->points) - ch->exp);
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
					sprintf(buf2, "%ld", ch->in_room->vnum);
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


/*
 * Append onto an output buffer.
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
	    bug("Buffer overflow. Closing.\n\r",0);
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


/*
 * Lowest level output function.
 * Write a block of text to the file descriptor.
 * If this gives errors on very long blocks (like 'ofind all'),
 *   try lowering the max block size.
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
        if (d->ssl) {
            nWrite = SSL_write(d->ssl, txt + iStart, nBlock);
            if (nWrite <= 0) {
                int err = SSL_get_error(d->ssl, nWrite);
                if (err == SSL_ERROR_WANT_WRITE) {
                    // The operation didn't complete; try again later
                    break;
                } else if (err == SSL_ERROR_SYSCALL && errno == EPIPE) {
                    // Explicitly handle EPIPE here
                    return false;
                } else {
                    fprintf(stderr, "SSL_write failed with error: %d\n", err);
                    ERR_print_errors_fp(stderr);
                    return false;
                }
            }
        } else {
            nWrite = write(d->descriptor, txt + iStart, nBlock);
            if (nWrite < 0) {
                                if (errno == EPIPE) {
                    // Explicitly handle EPIPE here
                    return false; 
                }
                perror("Write_to_descriptor_2");
                return false;
            }
        }
    }

    return true;
}


/* mccp: write_to_descriptor wrapper */
bool write_to_descriptor(DESCRIPTOR_DATA *d, char *txt, int length)
{
    if (d->out_compress)
        return writeCompressed(d, txt, length);
    else
		return write_to_descriptor_2(d, txt, length);
}


#define DEBUG		true

void plogf (char *fmt, ...)
{
    char buf[2 * MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    log_string (buf);
}

/*
void join_world(DESCRIPTOR_DATA * d)
{
    CHAR_DATA *ch;
    char buf[MSL];

    ch = d->character;
    plogf ("nanny.c, join_world(): Placing character in game.");
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

        ch->perm_stat[class_table[ch->class].attr_prime] += 3;

        ch->level = 1;
        ch->tot_level = 1;
        ch->exp = exp_per_level (ch, ch->pcdata->points);
        ch->hit = ch->max_hit;
        ch->mana = ch->max_mana;
        ch->move = ch->max_move;
        ch->train = 3;
        ch->practice = 5;
        sprintf (buf, "the %s", title_table[ch->class][ch->level]
                 [ch->normal_sex == SEX_FEMALE ? 1 : 0]);
        set_title (ch, buf);

        obj_to_char (create_object (get_obj_index (OBJ_VNUM_MAP), 0),
                     ch);

        char_to_room (ch, get_room_index (ROOM_VNUM_SCHOOL));
        send_to_char ("\n\r", ch);
        do_function (ch, &do_help, "newbie info");
    }
    else
    {
        if (ch->in_room != NULL)
        {
            plogf("nanny.c, join_world(): Transferring char to Real Room");
            char_to_room (ch, ch->in_room);
        }
        else
        {
            if (ch->in_wilds != NULL)
            {
                plogf("nanny.c, join_world(): Transferring char to VRoom");
                char_to_vroom (ch, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
            }
            else
            {
                if (IS_IMMORTAL (ch))
                {
                    char_to_room (ch, get_room_index (ROOM_VNUM_CHAT));
                }
                else
                {
                    char_to_room (ch, get_room_index (ROOM_VNUM_LIMBO));
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




/*
 * Parse a name for acceptability.
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

/*
 * Look for link-dead player to reconnect.
 */
/*
 * Look for link-dead player to reconnect.
 */
bool check_reconnect(DESCRIPTOR_DATA *d, char *name, bool fConn)
{
    CHAR_DATA *ch;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool found = false;
    ITERATOR cit;

    iterator_start(&cit, loaded_chars);
    while((ch = (CHAR_DATA *)iterator_nextdata(&cit)) && !found)
    {
        if (!IS_NPC(ch) && 
            (!fConn || ch->desc == NULL) && 
            !str_cmp(d->character->name, ch->name)) 
        {
            if (!fConn) {
                free_string(d->character->pcdata->pwd);
                d->character->pcdata->pwd = str_dup(ch->pcdata->pwd);
                iterator_stop(&cit);
                return true;
            } else {
                CHAR_DATA *old_char = d->character;

                // Handle pet cleanup from incoming connection if needed
                if (old_char->pet) {
                    CHAR_DATA *pet = old_char->pet;
                    char_to_room(pet, get_room_index(get_reserved_vnum("room_limbo")));
                    stop_follower(pet, true);
                    extract_char(pet, true);
                }

                // Preserve any temporary descriptor data we need
                ch->timer = 0;  // Reset idle timer
                
                // Switch the descriptor to the existing character
                ch->desc = d;
                d->character = ch;  // Point to the existing character
                d->original = NULL; // Make sure we're not switched
                
                // Now free the temporary character that was created during login
                free_char(old_char);
                d->reconnecting = true;
                found = true;  // Mark as found so iterator_stop works properly
                
                // Find the account character entry for the character
                if (d->account) {
                    ITERATOR it;
                    iterator_start(&it, d->account->characters);
                    while ((acct_char = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
                        if (!str_cmp(acct_char->name, ch->name)) {
                            break;
                        }
                    }
                    iterator_stop(&it);
                }

                // Handle special authentication cases
                if (!DEV_SKIP_MFA) {
                    bool has_mfa = acct_char ? acct_char->mfa_key != NULL : false;
                    
                    if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff) {
                        // If character has MFA, verify that
                        if (has_mfa) {
                            write_to_buffer(d, "\n\rReconnecting - This character has MFA enabled.\n\r", 0);
                            ProtocolNoEcho(d, true);
                            d->connected = CON_GET_CHAR_MFA;
                            break;
                        } 
                        // Otherwise, verify account MFA
                        else if (!IS_NULLSTR(d->account->mfa_key)) {
                            write_to_buffer(d, "\n\rReconnecting - Staff account MFA verification required.\n\r", 0);
                            ProtocolNoEcho(d, true);
                            d->connected = CON_GET_ACCOUNT_MFA_FOR_CHAR;
                            break;
                        }
                    }
                    // Regular character with MFA
                    else if (has_mfa) {
                        write_to_buffer(d, "\n\rReconnecting - This character has MFA enabled.\n\r", 0);
                        ProtocolNoEcho(d, true);
                        d->connected = CON_GET_CHAR_MFA;
                        break;
                    }
                }
                
                // Check for character password - use account_character data
                if (!DEV_SKIP_PASSWORD) {
                    bool has_password = acct_char ? !IS_NULLSTR(acct_char->pwd) : false;
                    
                    if (has_password) {
                        write_to_buffer(d, "\n\rReconnecting: This character requires password verification.\n\r", 0);
                        ProtocolNoEcho(d, true);
                        d->connected = CON_GET_CHAR_PASSWORD;
                        break;
                    }
                }
                
                // Normal reconnect process - no special auth needed
                reconnect_char(d);
                break;
            }
        }
    }
    iterator_stop(&cit);
    
    return found;
}

void reconnect_char(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    char buf[MAX_STRING_LENGTH];
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
            log_string("Detected broken connection during reconnect");
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
    act("$n has reconnected.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    
    // Log the reconnection
    sprintf(buf, "%s@%s reconnected.", ch->name, d->host);
    log_string(buf);
    wiznet("$N has relinked.", ch, NULL, WIZ_LINKS, 0, 0);
    
    // Update protocol settings
    MXPSendTag(d, "<VERSION>");
    
    // Add connection to tracking
    connection_add(d);
}

/*
 * Check if already playing.
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


void stop_idling(CHAR_DATA *ch)
{
	if (ch == NULL ||
		ch->desc == NULL ||
		ch->desc->connected != CON_PLAYING ||
		ch->was_in_room == NULL ||
		ch->in_room != get_room_index(get_reserved_vnum("room_limbo")))
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
    act("$n has returned from the void.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}


/*
 * Write to one char.
 */
void send_to_char_bw(const char *txt, CHAR_DATA *ch)
{
/*    write_to_buffer(ch->desc, txt, strlen(txt));*/
    if (txt != NULL && ch->desc != NULL)
        write_to_buffer(ch->desc, txt, strlen(txt));
}

/*
 * Write to one char, new colour version, by Lope.
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

/*
 * Send a page to one char.
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


/* string pager */
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
			if (IS_SET(d->character->act[0], PLR_COLOUR))
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


void act_new(char *format, CHAR_DATA *ch,
		CHAR_DATA *vch, CHAR_DATA *vch2,
		OBJ_DATA *obj1, OBJ_DATA *obj2,
		void *arg1, void *arg2,
		int type, int min_pos, CHAR_TEST char_func)
{
    static char * const he_she  [] = { "it",  "he",  "she" };
    static char * const him_her [] = { "it",  "him", "her" };
    static char * const his_her [] = { "its", "his", "her" };


    CHAR_DATA 		*to;
//    CHAR_DATA 		*vch = (CHAR_DATA *) arg2;
//    CHAR_DATA 		*vch2 = (CHAR_DATA *) arg1;
//    OBJ_DATA 		*obj1 = (OBJ_DATA  *) arg1;
//    OBJ_DATA 		*obj2 = (OBJ_DATA  *) arg2;
    const 	char 	*str;
    char 		*i = NULL;
    char 		*point;
    //char 		*pbuff;
//    char 		buffer[ MAX_STRING_LENGTH*2 ];
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
            bug("Act: null vch with TO_VICT.", 0);
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



//            if (!arg2 && *str >= 'A' && *str <= 'Z')
//            {
//                bug("Act: missing arg2 for code %d.", *str);
//                i = " <@@@> ";
//            }
//            else
//            {
                switch (*str)
                {
                default:  bug("Act: bad code %d.", *str);
                          i = " <@@@> ";
                          break;
                /* Thx alex for 't' idea */
                case 't': if (arg1) i = (char *) arg1;
                          else bug("Act: bad code $t for 'arg1'",0);
                          break;
                case 'T': if (arg2) i = (char *) arg2;
                          else bug("Act: bad code $T for 'arg2'",0);
                          break;
                case 'v': if (vch2&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(vch2)))
							i = ch->name;
                          else
							i = pers(vch2,  to );
                          }
                          else bug("Act: bad code $v for 'vch2' or 'to'",0);
                          break;
                case 'n': if (ch&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(ch)))
							i = ch->name;
                          else
							i = pers(ch,  to );
                          }
                          else bug("Act: bad code $n for 'ch' or 'to'",0);
                          break;
                case 'N': if (vch&&to) {
                          if (see_all || (to->tot_level >= 150 && !IS_NPC(vch)))
							i = vch->name;
                          else
							i = pers(vch,  to );
                          }
                          else bug("Act: bad code $N for 'ch' or 'to'",0);
                          break;
                case 'e': if (ch) i = he_she  [URANGE(0, ch  ->sex, 2)];
                          else bug("Act: bad code $e for 'ch'",0);
                          break;
                case 'E': if (vch) i = he_she  [URANGE(0, vch ->sex, 2)];
                          else bug("Act: bad code $E for 'ch'",0);
                          break;
                case 'm': if (ch) i = him_her [URANGE(0, ch  ->sex, 2)];
                          else bug("Act: bad code $m for 'ch'",0);
                          break;
                case 'M': if (vch) i = him_her [URANGE(0, vch ->sex, 2)];
                          else bug("Act: bad code $M for 'ch'",0);
                          break;
                case 's': if (ch) i = his_her [URANGE(0, ch  ->sex, 2)];
                          else bug("Act: bad code $s for 'ch'",0);
                          break;
                case 'S': if (vch) i = his_her [URANGE(0, vch ->sex, 2)];
                          else bug("Act: bad code $S for 'ch'",0);
                          break;

                case 'p': if (to&&obj1) i = (see_all || can_see_obj(to, obj1))
                            ? obj1->short_descr
                            : "something";
                          else bug("Act: bad code $p for 'to' or 'obj1'",0);
                    break;

                case 'P': if (to&&obj2) i = (see_all || can_see_obj(to, obj2))
                            ? obj2->short_descr
                            : "something";
                          else bug("Act: bad code $P for 'to' or 'obj2'",0);
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
//            }

            ++str;
            if( i != NULL ) {
	            while ((*point = *i) != '\0')
	                ++point, ++i;
			} else {
				strcpy(point, "<NULL>");
				point += 6;
			}
        }

        *point++ = '\n';
        *point++ = '\r';
	*point   = '\0';
        /*buf[0]   = UPPER(buf[0]);*/
        sprintf(buf, "%s", upper_first(&buf[0]));
	if (to->desc != NULL)
	{//   pbuff = buffer;
	    //colourconv(pbuff, buf, to);
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
     while(*str != '\0')
     {
         *point++ = *str++;
     }
     *point   = '\0';

    for(obj = ch->in_room->contents; obj; obj = obj_next)
    {
        obj_next = obj->next_content;
        p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }

    for(tch = ch; tch; tch = tch_next)
    {
        tch_next = tch->next_in_room;

        // Use iterator for lcarrying instead of direct traversal
            iterator_start(&it, tch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
            {
                p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
            }
            iterator_stop(&it);
            
            // Also iterate through lworn items
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
void printf_to_char (CHAR_DATA * ch, char *fmt, ...)
{
    char buf[MSL];
    va_list args;
    va_start (args, fmt);
    vsprintf (buf, fmt, args);
    va_end (args);

    send_to_char (buf, ch);
}


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


/* echo at a room */
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


/* echo around a room */
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


/* show state of formation, used in battle prompt */
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


/* Count down PC timers.*/
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
	    act("The dangerous blood aura surrounding $n fades away.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
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
	    act("{RPANIC! You are overcome with FEAR and attmpts to FLEE!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    act("{R$n is overcome with FEAR and attmpts to FLEE!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
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
	    if (number_percent() > get_skill(ch, gsn_deep_trance) - 10)
	    {
		send_to_char("{YYou lose your meditative focus as something grabs your attention.{x\n\r", ch);
		act("{Y$n loses $s meditative focus as something grabs $s attention.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
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
	if (number_percent() < (2 + 9 * get_skill(ch, gsn_hunt)/100))
	    update_hunting_pc(ch);
    }
}


void add_possible_races(CHAR_DATA *ch, char *string)
{
    char buf[MSL];
    int i;
    bool found = false;

    sprintf(buf, " {B[{C");
    for (i = 1; i < MAX_PC_RACE; i++)
    {
	if ((!pc_race_table[i].remort)
	&& ((ch->alignment == 0 && pc_race_table[i].alignment == ALIGN_NONE)
	||  (ch->alignment  < 0 && pc_race_table[i].alignment == ALIGN_EVIL)
	||  (ch->alignment  > 0 && pc_race_table[i].alignment == ALIGN_GOOD)))
	{
	    if (found)
		strcat(buf, " ");

	    found = true;
	    strcat(buf, pc_race_table[i].name);
	}
    }

    strcat(buf, "{B]{x");

    strcat(string, buf);
}


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
	if (!sub_class_table[i].remort
	&&  ch->pcdata->class_current == sub_class_table[i].class)
	{
	    if ((align == ALIGN_GOOD && sub_class_table[i].alignment == ALIGN_EVIL)
            ||  (align == ALIGN_EVIL && sub_class_table[i].alignment == ALIGN_GOOD))
		continue;

            count++;

	    if (count > 1)
		strcat(string, " ");

	    sprintf(buf, "%s", sub_class_table[i].name[ch->sex]);
	    buf[0] = UPPER(buf[0]);
	    strcat(string, buf);
	}
    }

    strcat(string, "{B]{x");
}


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

/*
 * Initialize the SSL cleanup queue
 * Call this during boot sequence
 */
void init_ssl_cleanup_queue(void)
{
    ssl_ctx_cleanup_queue = list_create(false);
    if (!ssl_ctx_cleanup_queue) {
        bug("Could not create SSL cleanup queue", 0);
        exit(1);
    }
}

/*
 * Add an SSL context to the cleanup queue
 */
typedef struct ssl_cleanup_data {
    SSL_CTX *ctx;
    time_t time_added;
} SSL_CLEANUP_DATA;

void add_ssl_ctx_to_cleanup(SSL_CTX *old_ctx)
{
    if (!old_ctx)
        return;
        
    SSL_CLEANUP_DATA *data;
    
    data = (SSL_CLEANUP_DATA *)malloc(sizeof(SSL_CLEANUP_DATA));
    data->ctx = old_ctx;
    data->time_added = current_time;
    
    list_appendlink(ssl_ctx_cleanup_queue, data);
    log_string("SSL context added to cleanup queue");
}

/*
 * Process the SSL context cleanup queue
 * Free contexts that have been in the queue for sufficient time
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
            log_string("Freed old SSL context from cleanup queue");
        }
    }
    iterator_stop(&it);
}

/*
 * Updated SSL context refresh function
 * Uses the cleanup queue for safe context disposal
 */
void refresh_ssl_context(void)
{
    static time_t last_refresh = 0;
    
    // Refresh once per hour by default, or when circuit breaker triggers
    if (current_time - last_refresh < 3600 && ssl_errors_since_reset < 5)
        return;
        
    log_string("Refreshing SSL context...");
    
    // Create new context
    SSL_CTX *new_ctx = create_context();
    if (!new_ctx) {
        log_string("ERROR: Failed to create new SSL context");
        return;
    }
    
    // Configure the new context
    if (!configure_context(new_ctx)) {
        log_string("ERROR: Failed to configure new SSL context");
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
    log_string("SSL context refreshed successfully");
}