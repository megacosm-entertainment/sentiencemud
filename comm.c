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

#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "scripts.h"
#include "tables.h"
#include "wilds.h"
#include "sha256.h"

/*
 * Malloc debugging stuff.
 */
#if defined(sun)
#undef MALLOC_DEBUG
#endif

#if defined(MALLOC_DEBUG)
#include <malloc.h>
extern	int	malloc_debug	args((int ));
extern	int	malloc_verify	args((void));
#endif


/*
 * Signal handling.
 * Apollo has a problem with __attribute(atomic) in signal.h,
 *   I dance around it.
 */
#if defined(apollo)
#define __attribute(x)
#endif

#if defined(unix)
#include <signal.h>
#endif

#if defined(apollo)
#undef __attribute
#endif


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
#if	defined(_AIX)
#include <sys/select.h>
int	accept		args((int s, struct sockaddr *addr, int *addrlen));
int	bind		args((int s, struct sockaddr *name, int namelen));
void	bzero		args((char *b, int length));
int	getpeername	args((int s, struct sockaddr *name, int *namelen));
int	getsockname	args((int s, struct sockaddr *name, int *namelen));
int	gettimeofday	args((struct timeval *tp, struct timezone *tzp));
int	listen		args((int s, int backlog));
int	setsockopt	args((int s, int level, int optname, void *optval,
			    int optlen));
int	socket		args((int domain, int type, int protocol));
#endif

#if	defined(apollo)
#include <unistd.h>
void	bzero		args((char *b, int length));
#endif

#if	defined(__hpux)
int	accept		args((int s, void *addr, int *addrlen));
int	bind		args((int s, const void *addr, int addrlen));
void	bzero		args((char *b, int length));
int	getpeername	args((int s, void *addr, int *addrlen));
int	getsockname	args((int s, void *name, int *addrlen));
int	gettimeofday	args((struct timeval *tp, struct timezone *tzp));
int	listen		args((int s, int backlog));
int	setsockopt	args((int s, int level, int optname,
 				const void *optval, int optlen));
int	socket		args((int domain, int type, int protocol));
#endif

#if	defined(interactive)
#include <net/errno.h>
#include <sys/fnctl.h>
#endif

#if	defined(linux)
/*
    Linux shouldn't need these. If you have a problem compiling, try
    uncommenting these functions.
*/
/*
int accept    args( ( int s, struct sockaddr *addr, int *addrlen ) );
int bind    args( ( int s, struct sockaddr *name, int namelen ) );
int getpeername args( ( int s, struct sockaddr *name, int *namelen ) );
int getsockname args( ( int s, struct sockaddr *name, int *namelen ) );
int listen    args( ( int s, int backlog ) );
*/

int	close		args((int fd));
/* int	gettimeofday	args((struct timeval *tp, struct timezone *tzp)); */
/*int	read		args((int fd, char *buf, int nbyte));*/
int	select		args((int width, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout));
int	socket		args((int domain, int type, int protocol));
/*int	write		args((int fd, char *buf, int nbyte));*/
#endif

#if	defined(MIPS_OS)
extern	int		errno;
#endif

#if	defined(NeXT)
int	close		args((int fd));
int	fcntl		args((int fd, int cmd, int arg));
#if	!defined(htons)
u_short	htons		args((u_short hostshort));
#endif
#if	!defined(ntohl)
u_long	ntohl		args((u_long hostlong));
#endif
int	read		args((int fd, char *buf, int nbyte));
int	select		args((int width, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout));
int	write		args((int fd, char *buf, int nbyte));
#endif

#if	defined(sequent)
int	accept		args((int s, struct sockaddr *addr, int *addrlen));
int	bind		args((int s, struct sockaddr *name, int namelen));
int	close		args((int fd));
int	fcntl		args((int fd, int cmd, int arg));
int	getpeername	args((int s, struct sockaddr *name, int *namelen));
int	getsockname	args((int s, struct sockaddr *name, int *namelen));
int	gettimeofday	args((struct timeval *tp, struct timezone *tzp));
#if	!defined(htons)
u_short	htons		args((u_short hostshort));
#endif
int	listen		args((int s, int backlog));
#if	!defined(ntohl)
u_long	ntohl		args((u_long hostlong));
#endif
int	read		args((int fd, char *buf, int nbyte));
int	select		args((int width, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout));
int	setsockopt	args((int s, int level, int optname, caddr_t optval,

			    int optlen));
int	socket		args((int domain, int type, int protocol));
int	write		args((int fd, char *buf, int nbyte));
#endif

/* This includes Solaris Sys V as well */
#if defined(sun)
int	accept		args((int s, struct sockaddr *addr, int *addrlen));
int	bind		args((int s, struct sockaddr *name, int namelen));
void	bzero		args((char *b, int length));
int	close		args((int fd));
int	getpeername	args((int s, struct sockaddr *name, int *namelen));
int	getsockname	args((int s, struct sockaddr *name, int *namelen));
int	listen		args((int s, int backlog));
int	read		args((int fd, char *buf, int nbyte));
int	select		args((int width, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout));

#if !defined(__SVR4)
int	gettimeofday	args((struct timeval *tp, struct timezone *tzp));

#if defined(SYSV)
int setsockopt		args((int s, int level, int optname,
			    const char *optval, int optlen));
#else
int	setsockopt	args((int s, int level, int optname, void *optval,
			    int optlen));
#endif
#endif
int	socket		args((int domain, int type, int protocol));
int	write		args((int fd, char *buf, int nbyte));
#endif

#if defined(ultrix)
int	accept		args((int s, struct sockaddr *addr, int *addrlen));
int	bind		args((int s, struct sockaddr *name, int namelen));
void	bzero		args((char *b, int length));
int	close		args((int fd));
int	getpeername	args((int s, struct sockaddr *name, int *namelen));
int	getsockname	args((int s, struct sockaddr *name, int *namelen));
int	gettimeofday	args((struct timeval *tp, struct timezone *tzp));
int	listen		args((int s, int backlog));
int	read		args((int fd, char *buf, int nbyte));
int	select		args((int width, fd_set *readfds, fd_set *writefds,
			    fd_set *exceptfds, struct timeval *timeout));
int	setsockopt	args((int s, int level, int optname, void *optval,
			    int optlen));
int	socket		args((int domain, int type, int protocol));
int	write		args((int fd, char *buf, int nbyte));
#endif


void show_form_state(CHAR_DATA *ch);
/* VIZZWILDS */
void join_world args ((DESCRIPTOR_DATA * d));


/*
 * External functions.
 */
extern void boat_attack(CHAR_DATA *ch);

extern void save_area_list();
extern void save_area_new(AREA_DATA *area);

/*
 * Global variables.
 */
bool			is_test_port;
int 		    port;
GLOBAL_DATA         gconfig;		/* Vizz - UID Tracking, and any other persistent global config info */
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
bool		    MOBtrigger = TRUE;  /* act() switch                 */
LLIST *loaded_areas;

/*
 * OS-dependent local functions.
 */
void	game_loop_unix		args((int control));
int	init_socket		args((int port));
void	init_descriptor		args((int control));
bool	read_from_descriptor	args((DESCRIPTOR_DATA *d));
bool	write_to_descriptor	args((DESCRIPTOR_DATA *d, char *txt, int length));
bool	write_to_descriptor_2	args((int desc, char *txt, int length));


/*
 * Other local functions (OS-independent).
 */
bool	check_parse_name	args((char *name));
//Temporarily disabling due to reconnect crash
//CHAR_DATA *find_existing_player	args((char *name));
bool	check_reconnect		args((DESCRIPTOR_DATA *d, char *name, bool fConn));
bool	check_playing		args((DESCRIPTOR_DATA *d, char *name));
int	main			args((int argc, char **argv));
void	nanny			args((DESCRIPTOR_DATA *d, char *argument));
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

extern bool fBootstrap;

static void RedirectSTDOUT(void)
{
	FILE *newfp;

	/* Redirect standard input and standard output*/
	sprintf(logfile_std,"../log/sent%d_%d.log",port,(int)time(NULL));
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

	/* Redirect standard input and standard output*/
	sprintf(logfile_err,"../log/sent%d_%d.err",port,(int)time(NULL));
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
	if(ftell(stdout) > MAX_LOGFILE) {
		CleanupSTDOUT();
		RedirectSTDOUT();
	}

	if(ftell(stderr) > MAX_LOGFILE) {
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
				return FALSE;
			}

			port = p;
		}
		else if ( argv[i][0] == '-' && (strlen(argv[i]) == 2) )
		{
			switch( argv[i][1] )
			{
				case 'n':
				case 'N':
					newlock = TRUE;
					break;

				case 't':
				case 'T':
					is_test_port = TRUE;
					break;

				case 'w':
				case 'W':
					wizlock = TRUE;
					break;

				case '?':
					// Silently return
					return FALSE;

				default:
					fprintf(stderr, "Invalid option found.");
					return FALSE;
			}
		}
		else {
			fprintf(stderr, "Invalid argument found.");
			return FALSE;
		}

	}
	return TRUE;
}


int main(int argc, char **argv)
{
    static GLOBAL_DATA gconfig_zero;
    struct timeval now_time;
    int control;
    ITERATOR iter;
    void *data;

    /*
     * Memory debugging if needed.
     */
#if defined(MALLOC_DEBUG)
    malloc_debug(2);
#endif

	conn_players = list_create(FALSE);
	if(!conn_players) {
		perror("Could not create 'conn_players'");
		exit(1);
	}

	conn_immortals = list_create(FALSE);
	if(!conn_immortals) {
		perror("Could not create 'conn_immortals'");
		exit(1);
	}

	conn_online = list_create(FALSE);
	if(!conn_online) {
		perror("Could not create 'conn_online'");
		exit(1);
	}
	loaded_areas = list_create(FALSE);
	if(!loaded_areas) {
		perror("Could not create 'loaded_areas'");
		exit(1);
	}
	loaded_wilds = list_create(FALSE);
	if(!loaded_wilds) {
		perror("Could not create 'loaded_wilds'");
		exit(1);
	}
	list_churches = list_create(FALSE);
	if(!list_churches) {
		perror("Could not create 'list_churches'");
		exit(1);
	}
	persist_mobs = list_create(FALSE);
	if(!persist_mobs) {
		perror("Could not create 'persist_mobs'");
		exit(1);
	}
	persist_objs = list_create(FALSE);
	if(!persist_objs) {
		perror("Could not create 'persist_objs'");
		exit(1);
	}
	persist_rooms = list_create(FALSE);
	if(!persist_rooms) {
		perror("Could not create 'persist_rooms'");
		exit(1);
	}
	loaded_chars = list_create(FALSE);
	if(!loaded_chars) {
		perror("Could not create 'loaded_chars'");
		exit(1);
	}
// Temporarily disabling for reconnect crash.
/*
	loaded_players = list_create(FALSE);
	if(!loaded_players) {
		perror("Could not create 'loaded_players'");
		exit(1);
	}
*/
	loaded_objects = list_create(FALSE);
	if(!loaded_objects) {
		perror("Could not create 'loaded_objects'");
		exit(1);
	}



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

    /*
     * Get the port number.
     */
    port = 9000;
    is_test_port = FALSE;
    newlock = FALSE;
    wizlock = FALSE;

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

    //if(port == PORT_TEST) newlock = TRUE;	/* The alpha port is initially set to newlock*/
    //if(port == PORT_TEST) wizlock = TRUE;	/* Newlock/Wizlock all ports for now */

    //if(port == PORT_TEST || port == PORT_ALPHA || port == PORT_SYN) is_test_port = TRUE;

    RedirectOutput();

    /* Vizz - load up our list of UIDs. Without this, we cannot assign unique UIDs to things */
    gconfig = gconfig_zero;
    if (gconfig_read()==1) exit(1);
	log_string("Global config loaded.");

    /*
     * Run the game.
     */
    control = init_socket(port);
	sprintf(log_buf, "Socket bound to port %d.", port);
	log_string(log_buf);
    boot_db();
    sprintf(log_buf, "Sentience is up on port %d.", port);
    log_string(log_buf);
    #ifdef IMC
    imc_startup( FALSE, -1, FALSE );
    #endif
    game_loop_unix(control);
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
    close (control);
	#ifdef IMC
	SERVER_DATA *server;
	extern SERVER_DATA *first_server;
	for( server = first_server; server; server = server->next )
	imc_shutdown(FALSE, server);
	#endif

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

void game_loop_unix(int control)
{
    static struct timeval null_time;
    struct timeval last_time;

    signal(SIGPIPE, SIG_IGN);
    gettimeofday(&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;

    /* Main loop */
    while (!merc_down)
    {
	fd_set in_set;
	fd_set out_set;
	fd_set exc_set;
	DESCRIPTOR_DATA *d;
	int maxdesc;

#if defined(MALLOC_DEBUG)
	if (malloc_verify() != 1)
	    abort();
#endif

	/*
	 * Poll all active descriptors.
	 */
	FD_ZERO(&in_set );
	FD_ZERO(&out_set);
	FD_ZERO(&exc_set);
	FD_SET(control, &in_set);
	maxdesc	= control;
	for (d = descriptor_list; d; d = d->next)
	{
	    maxdesc = UMAX(maxdesc, d->descriptor);
	    FD_SET(d->descriptor, &in_set );
	    FD_SET(d->descriptor, &out_set);
	    FD_SET(d->descriptor, &exc_set);
	}

	if (select(maxdesc+1, &in_set, &out_set, &exc_set, &null_time) < 0)
	{
	    switch (errno)
		{
		case EBADF:
		bug ("Invalid file descriptor passed to Select()", 0);
	    perror("Game_loop: select: poll");
		exit(1);
		break;
		case EINTR:	bug("A non-blocked signal was caught.", 0);
		break;
		case EINVAL:
		bug ("Negative \'n\' descriptor passed to Select()", 0);
	    perror("Game_loop: select: poll");
		exit(1);
		break;
		case ENOMEM:
		bug ("Select() was unable to allocate memory for internal tables.", 0);
	    perror("Game_loop: select: poll");
		exit(1);
		break;
		default:
		bug ("Unknown error.", 0);
	    perror("Game_loop: select: poll");
		exit(1);
	    break;
	}
/*	    perror("Game_loop: select: poll");*/
/*	    exit(1);*/
	}

	/*
	 * New connection?
	 */
	if (FD_ISSET(control, &in_set))
	    init_descriptor(control);

	/*
	 * Kick out the freaky folks.
	 */
	for (d = descriptor_list; d != NULL; d = d_next)
	{
	    d_next = d->next;
	    if (FD_ISSET(d->descriptor, &exc_set))
	    {
		FD_CLR(d->descriptor, &in_set );
		FD_CLR(d->descriptor, &out_set);
		if (d->character && d->connected == CON_PLAYING)
		{
		    save_char_obj(d->character);
		}
		d->outtop	= 0;
		close_socket(d);
	    }
	}

	/*
	 * Process input.
	 */
	for (d = descriptor_list; d != NULL; d = d_next)
	{
	    d_next	= d->next;
	    d->fcommand	= FALSE;

	    if (FD_ISSET(d->descriptor, &in_set))
	    {
		if (d->character != NULL)
		    d->character->timer = 0;

		if (!read_from_descriptor(d))
		{
		    FD_CLR(d->descriptor, &out_set);

		    if (d->character != NULL
			    &&   d->connected == CON_PLAYING)
			save_char_obj(d->character);

		    d->outtop	= 0;
		    close_socket(d);
		    continue;
		}

		    d->muted = 0;		// Force it to unmute every time they give any kind of command
	    }

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
	       d->fcommand	= TRUE;
		if(d->pProtocol != NULL)
		    d->pProtocol->WriteOOB = 0;
	       stop_idling(d->character);

	       /* OLC */
	       if (d->showstr_point)
		   show_string(d, d->incomm);
	       else
		   if (d->pString) {
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

	       d->incomm[0]	= '\0';
	   }
    }

#ifdef IMC
imc_loop();
#endif

    /*
     * Autonomous game motion.
     */
    update_handler();

    /*
     * Process message queues.
     */
    /*process_message_queue();*/

    /*
     * Output.
     */
    for (d = descriptor_list; d != NULL; d = d_next)
    {
	d_next = d->next;

	if ((d->fcommand || d->outtop > 0)
		&&   FD_ISSET(d->descriptor, &out_set))
	{
	    if (!process_output(d, TRUE))
	    {
		if (d->character != NULL
			&& d->connected == CON_PLAYING) {
		    save_char_obj(d->character);
		}
		d->outtop	= 0;
		close_socket(d);
	    }
	}
    }

    /*
     * Synchronize to a clock.
     * Sleep(last_time + 1/PULSE_PER_SECOND - now).
     * Careful here of signed versus unsigned arithmetic.
     */
	{
	    struct timeval now_time;
	    long secDelta;
	    long usecDelta;

	    gettimeofday(&now_time, NULL);
	    usecDelta	= ((int) last_time.tv_usec) - ((int) now_time.tv_usec)
		+ 1000000 / PULSE_PER_SECOND;
	    secDelta	= ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
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

	/* Check to see if the logfiles have overflowed*/
	check_logfile();

	gettimeofday(&last_time, NULL);
	current_time = (time_t) last_time.tv_sec;
    }
}


void init_descriptor(int control)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *dnew;
    struct sockaddr_in sock;
    struct hostent *from;
    int desc;
    socklen_t size;

    size = sizeof(sock);
    getsockname( control, (struct sockaddr *) &sock, &size );
    if ( ( desc = accept( control, (struct sockaddr *) &sock, &size) ) < 0 )
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
	return;
    }

    /*
     * Cons a new descriptor.
     */
    dnew = new_descriptor();

    dnew->descriptor	= desc;
    dnew->connected	= CON_GET_NAME;
    dnew->showstr_head	= NULL;
    dnew->showstr_point = NULL;
    dnew->outsize	= 2000;
    dnew->pEdit		= NULL;			/* OLC */
    dnew->pString	= NULL;			/* OLC */
    dnew->editor	= 0;			/* OLC */
    dnew->outbuf	= alloc_mem(dnew->outsize);
    dnew->pProtocol	= ProtocolCreate();

    size = sizeof(sock);
    if (getpeername(desc, (struct sockaddr *) &sock, &size) < 0)
    {
	perror("New_descriptor: getpeername");
	dnew->host = str_dup("(unknown)");
    }
    else
    {
	/*
	 * Would be nice to use inet_ntoa here but it takes a struct arg,
	 * which ain't very compatible between gcc and system libraries.
	 */
	int addr;

	addr = ntohl(sock.sin_addr.s_addr);
	sprintf(buf, "%d.%d.%d.%d",
	    (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
	    (addr >>  8) & 0xFF, (addr      ) & 0xFF
	   );
	sprintf(log_buf, "Sock.sinaddr:  %s", buf);
	log_string(log_buf);
	from = gethostbyaddr((char *) &sock.sin_addr,
	    sizeof(sock.sin_addr), AF_INET);
	dnew->host = str_dup(from ? from->h_name : buf);
    }

    /*
     * Swiftest: I added the following to ban sites.  I don't
     * endorse banning of sites, but Copper has few descriptors now
     * and some people from certain sites keep abusing access by
     * using automated 'autodialers' and leaving connections hanging.
     *
     * Furey: added suffix check by request of Nickel of HiddenWorlds.
     */
    if (check_ban(dnew->host,BAN_ALL))
    {
	write_to_descriptor_2(desc,
	    "Your site has been banned from Sentience.\n\r", 0);
	close(desc);
	free_descriptor(dnew);
	return;
    }
    /*
     * Init descriptor data.
     */
    dnew->next			= descriptor_list;
    descriptor_list		= dnew;
    ProtocolNegotiate(dnew);

    /*
     * Send the greeting.
     */

    /* mccp: tell the client we support compression */
    write_to_buffer(dnew, compress_will, 0);

    /* msp: tell the client we support msp */
    write_to_buffer(dnew, msp_will, 0);

    if (help_greeting[0] == '.')
	write_to_buffer(dnew, help_greeting+1, 0);
    else
	write_to_buffer(dnew, help_greeting  , 0);

    write_to_buffer(dnew, "By what name do you wish to be known? ", 0);
}


void close_socket(DESCRIPTOR_DATA *dclose)
{
    CHAR_DATA *ch;

    if (dclose->outtop > 0)
	process_output(dclose, FALSE);

    if (dclose->snoop_by != NULL)
    {
	write_to_buffer(dclose->snoop_by,
	    "Your victim has left the game.\n\r", 0);
    }

    {
	DESCRIPTOR_DATA *d;

	for (d = descriptor_list; d != NULL; d = d->next)
	{
	    if (d->snoop_by == dclose)
		d->snoop_by = NULL;
	}
    }

    if ((ch = dclose->character) != NULL)
    {
	sprintf(log_buf, "Closing link to %s.", ch->name);
	log_string(log_buf);
	/* cut down on wiznet spam when rebooting */
	if (dclose->connected == CON_PLAYING && !merc_down)
	{
	    if (ch->invis_level < LEVEL_IMMORTAL)
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

    close(dclose->descriptor);
    free_descriptor(dclose);
    return;
}


bool read_from_descriptor(DESCRIPTOR_DATA *d)
{
    int iStart;

    static char read_buf[MAX_PROTOCOL_BUFFER];
    read_buf[0] = '\0';

    /* Hold horses if pending command already. */
    if (d->incomm[0] != '\0')
	return TRUE;

    /* Check for overflow. */
//    iStart = strlen(d->inbuf);
//    if (iStart >= sizeof(d->inbuf) - 10)
    iStart = 0;
    if(strlen(d->inbuf) >= sizeof(d->inbuf) - 10)
    {
	sprintf(log_buf, "%s input overflow!", d->host);
	log_string(log_buf);
	write_to_descriptor(d,
	    "\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
	return FALSE;
    }

    /* Snarf input. */
    for (; ;)
    {
	int nRead;
/*
	nRead = read(d->descriptor, d->inbuf + iStart,
	    sizeof(d->inbuf) - 10 - iStart);
	if (nRead > 0)
	{
	    iStart += nRead;
	    if (d->inbuf[iStart-1] == '\n' || d->inbuf[iStart-1] == '\r')
		break;
	}
*/
	nRead = read( d->descriptor, read_buf + iStart,
	    sizeof(read_buf) - 10 - iStart );
	if ( nRead > 0 )
	{
	    iStart += nRead;
	    if ( read_buf[iStart-1] == '\n' || read_buf[iStart-1] == '\r' )
		break;
	}
	else if (nRead == 0)
	{
	    log_string("EOF encountered on read.");
	    return FALSE;
	}
	else if (errno == EWOULDBLOCK)
	    break;
	else
	{
	    perror("Read_from_descriptor");
	    return FALSE;
	}
    }

//    d->inbuf[iStart] = '\0';
    read_buf[iStart] = '\0';
    ProtocolInput(d,read_buf,iStart,d->inbuf);
    return TRUE;
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
	else if (isascii(d->inbuf[i]) && isprint(d->inbuf[i]))
	    d->incomm[k++] = d->inbuf[i];
        else if (d->inbuf[i] == (signed char)IAC) {
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

        }
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
		else if (++d->repeat >= 100 &&
			d->character &&
			d->connected == CON_PLAYING &&
			!IS_IMMORTAL(d->character))
	    {
			sprintf(log_buf, "%s input spamming!", d->host);
			log_string(log_buf);

			wiznet("Spam spam spam $N spam spam spam!",
				d->character,NULL,WIZ_SPAM,0,get_trust(d->character));
			if (d->incomm[0] == '!')
				wiznet(d->inlast,d->character,NULL,WIZ_SPAM,0,
				get_trust(d->character));
			else
				wiznet(d->incomm,d->character,NULL,WIZ_SPAM,0,
				get_trust(d->character));

			d->repeat = 0;

			write_to_descriptor(d,
				"\n\r*** PUT A LID ON IT!!! ***\n\r", 0);
			strcpy(d->incomm, "quit");

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
    for (j = 0; (d->inbuf[j] = d->inbuf[i+j]) != '\0'; j++);
    return;
}


/*
 * Low level output function.
 */
bool process_output(DESCRIPTOR_DATA *d, bool fPrompt)
{
    extern bool merc_down;

    /*
     * Bust a prompt.
     */
	if(d->pProtocol->WriteOOB)
		;
    else if (!merc_down)
    {
		if (d->showstr_point)
			write_to_buffer(d, "[Hit Return to continue]\n\r", 0);
		else if (fPrompt && d->pString && d->connected == CON_PLAYING)
			write_to_buffer(d, "> ", 2);
		else if (fPrompt && d->connected == CON_PLAYING)
		{
			CHAR_DATA *ch;
			CHAR_DATA *victim;

			ch = d->character;

			/* battle prompt */
			if ((victim = ch->fighting) != NULL && can_see(ch,victim) && ch->in_room == victim->in_room)
			{
				int percent;
				char wound[100];
				char *pbuff;
				char buf[2*MAX_STRING_LENGTH];
				char buf2[MSL];
				char buffer[MAX_STRING_LENGTH*2];

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
				/*
				if (percent >= 100)
					sprintf(wound,"is in excellent condition.");
				else if (percent >= 90)
					sprintf(wound,"has a few scratches.");
				else if (percent >= 75)
					sprintf(wound,"has some small wounds and bruises.");
				else if (percent >= 50)
					sprintf(wound,"has quite a few wounds.");
				else if (percent >= 30)
					sprintf(wound,"has some big nasty wounds and scratches.");
				else if (percent >= 15)
					sprintf(wound,"looks pretty hurt.");
				else if (percent >= 0)
					sprintf(wound,"is in awful condition.");
				else
					sprintf(wound,"is bleeding to death.");
				*/
				if (IS_SET(ch->comm, COMM_SHOW_FORM_STATE))
					show_form_state(ch);

				sprintf(buf2, "%s", pers(victim, ch));
				buf2[0] = UPPER(buf2[0]);

				sprintf(buf,"{M%s %s \n\r{x", buf2, wound);
				buf[0]	= UPPER(buf[0]);
				pbuff	= buffer;
				colourconv(pbuff, buf, d->character);
				write_to_buffer(d, buffer, 0);
			}


			ch = d->original ? d->original : d->character;
			if (!IS_SET(ch->comm, COMM_COMPACT))
				write_to_buffer(d, "\n\r", 2);


			if (IS_SET(ch->comm, COMM_PROMPT))
				bust_a_prompt(d->character);

			if (IS_SET(ch->comm,COMM_TELNET_GA))
				write_to_buffer(d,go_ahead_str,0);
		}
	}

    /*
     * Short-circuit if nothing to write.
     */
	if (d->outtop == 0)
		return TRUE;

    /*
     * Snoop-o-rama.
     */
	if (d->snoop_by != NULL)
	{
		if (d->character != NULL)
			write_to_buffer(d->snoop_by, d->character->name,0);
		write_to_buffer(d->snoop_by, "> ", 2);
		write_to_buffer(d->snoop_by, d->outbuf, d->outtop);
	}

    /*
     * OS-dependent output.
     */
	if (!write_to_descriptor(d, d->outbuf, d->outtop))
	{
		d->outtop = 0;
		return FALSE;
	}
	else
	{
		d->outtop = 0;
		return TRUE;
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
    char *pbuff;
    char buffer[ MAX_STRING_LENGTH*2 ];
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

    if (IS_SET(ch->affected_by, AFF_INVISIBLE)
    || IS_SET(ch->affected_by2, AFF2_IMPROVED_INVIS))
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

   if (IS_SET(ch->comm,COMM_SOCIAL))
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
		found = FALSE;
		doors[0] = '\0';
		for (door = 0; door < 10; door++) {
			if ((pexit = ch->in_room->exit [door]) && pexit ->u1.to_room &&
				(can_see_room(ch,pexit->u1.to_room) ||
					(IS_AFFECTED(ch,AFF_INFRARED) && !IS_AFFECTED(ch,AFF_BLIND))) &&
				!IS_SET(pexit->exit_info,EX_CLOSED)) {
				found = TRUE;
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
				((!IS_NPC(ch) && IS_SET(ch->act,PLR_HOLYLIGHT)) ||
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
					sprintf(buf2, "%ld#%ld", ch->in_room->area->uid, ch->in_room->vnum);
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
		sprintf(buf2, "%ld", (ch->silver / 50 + ch->gold/30));
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
   pbuff	= buffer;
   colourconv(pbuff, buf, ch);
   write_to_buffer(ch->desc, buffer, 0);
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

        if (d->outsize >= 32000)
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
bool write_to_descriptor_2(int desc, char *txt, int length)
{
    int iStart;
    int nWrite;
    int nBlock;

    if (length <= 0)
	length = strlen(txt);

    for (iStart = 0; iStart < length; iStart += nWrite)
    {
	nBlock = UMIN(length - iStart, 4096);
	if ((nWrite = write(desc, txt + iStart, nBlock)) < 0)
	    { perror("Write_to_descriptor"); return FALSE; }
    }

    return TRUE;
}


/* mccp: write_to_descriptor wrapper */
bool write_to_descriptor(DESCRIPTOR_DATA *d, char *txt, int length)
{
    if (d->out_compress)
        return writeCompressed(d, txt, length);
    else
        return write_to_descriptor_2(d->descriptor, txt, length);
}


#define DEBUG		TRUE

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
 * Deal with sockets that haven't logged in yet.
 */
void nanny(DESCRIPTOR_DATA *d, char *argument)
{
	DESCRIPTOR_DATA *d_old, *d_next, *d2;
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	char races[MSL];
	char classes[MSL];
	char subclasses[MSL];
	CHAR_DATA *ch;
	char *pwdnew;
	int iClass,race,i,weapon;
	bool fOld;
	long playernum;
	HELP_DATA *help;
	long vector, *field;
    char strsave[MAX_INPUT_LENGTH];
    FILE *fp;

	iClass = -1;

	while (isspace(*argument))
		argument++;

	ch = d->character;

	switch (d->connected) {
	default:
		bug("Nanny: bad d->connected %d.", d->connected);
		connection_remove(d);
		close_socket(d);
		return;

	case CON_GET_NAME:
		if (!argument[0]) {
			close_socket(d);
			return;
		}

		argument[0] = UPPER(argument[0]);
		if (!check_parse_name(argument)) {
			write_to_buffer(d, "Illegal name, try another.\n\rName: ", 0);
			return;
		}

		/* Ban old names -- Gairun - 20111219 */
		sprintf(strsave, "%s%c/%s", OLD_PLAYER_DIR, tolower(argument[0]), capitalize(argument));
		if ((fp = fopen(strsave, "r")) != NULL)
		{
			fclose(fp);
			write_to_buffer(d, "Old names are not allowed.\n\rName: ", 0);
			return;
		}

		// Temporarily disabling for reconnect crash.
		// Check if the player is already playing
		/*
		if ((ch = find_existing_player(argument)) != NULL)
		{
			fOld = TRUE;
			d->character = ch;
		}
		else
		{*/
			fOld = load_char_obj(d, argument);

			ch = d->character;

			if (IS_SET(ch->act, PLR_DENY))
			{
				sprintf(log_buf, "Denying access to %s@%s.", argument, d->host);
				log_string(log_buf);
				write_to_buffer(d, "You are denied access.\n\r", 0);
				close_socket(d);
				return;
			}

			if (check_ban(d->host,BAN_PERMIT))
			{
				write_to_buffer(d,"Your site has been banned from Sentience.\n\r",0);
				close_socket(d);
				return;
			}

			// Adding back in for reconnect crash
			if (check_reconnect(d, argument, FALSE))
				fOld = TRUE;
			else
			{
			if (wizlock && !IS_IMMORTAL(ch))
			{
				write_to_buffer(d, "The game is wizlocked.\n\r", 0);
				close_socket(d);
				return;
			}
		}

		/* Old player */
		if (fOld)
		{
			write_to_buffer(d, "Password: ", 0);
			ProtocolNoEcho(d,true);
			d->connected = CON_GET_OLD_PASSWORD;

#if 0
			if (port != PORT_SYN) {
				write_to_buffer(d, "Password: ", 0);
				ProtocolNoEcho(d,true);
				d->connected = CON_GET_OLD_PASSWORD;
			}

			/* Syn - placed here for ease of testing so that I don't have to spam through
				pw entry/motd's every single time I boot the game. DEBUG is a definition
				as a safeguard just in case someone runs it on PORT_SYN for whatever reason. */
			else if (DEBUG == TRUE)
			{
				write_to_buffer(d, "Welcome back, Master.\n\r", 0);
				if (check_playing(d,ch->name))
					return;

				if (check_reconnect(d,ch->name,TRUE))
					return;

				reset_char(ch);

				list_appendlink(loaded_chars, ch);
				list_appendlink(loaded_players, ch);

				char_to_room(ch, ch->in_room);
				d->connected = CON_PLAYING;
				do_function(d->character, &do_look, "");
			}

#endif
			return;
		}
		else
		{
			/* New player */
			if (newlock)
			{
				write_to_buffer(d, "The game is newlocked.\n\r", 0);
				close_socket(d);
				return;
			}

			if (check_ban(d->host,BAN_NEWBIES))
			{
				write_to_buffer(d, "New players are not allowed from your site.\n\r",0);
				close_socket(d);
				return;
			}

			if(port == PORT_ALPHA) newlock = TRUE;	/* Reset the newlock, even if this one fails to do anything...*/

			sprintf(buf, "\n\rDo you want to create a character named %s (Y/N)? ", argument);
			write_to_buffer(d, buf, 0);
			d->connected = CON_CONFIRM_NEW_NAME;
			return;
		}
		break;

	case CON_GET_OLD_PASSWORD:
#if defined(unix)
		write_to_buffer(d, "\n\r", 2);
#endif
		if (ch->pcdata->pwd_vers < 1)
		{
			if (strcmp(crypt(argument, ch->pcdata->pwd), ch->pcdata->pwd))
			{
				if (strcmp(argument, ch->pcdata->pwd))
				{
					/* Log bad password attempts*/
					sprintf(log_buf, "Denying access to %s@%s (bad password).",
					ch->name, d->host);
					log_string(log_buf);
					wiznet(log_buf,NULL,NULL,WIZ_LOGINS,0,get_trust(ch));
					write_to_buffer(d, "Wrong password.\n\r", 0);
					close_socket(d);
					return;
				}
			}
		}
		else
		{
			if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
			{
				/* Log bad password attempts */
				sprintf(log_buf, "Denying access to %s@%s (bad password).", ch->name, d->host);
				log_string(log_buf);
				wiznet(log_buf,NULL,NULL,WIZ_LOGINS,0,get_trust(ch));
				write_to_buffer(d, "Wrong password.\n\r", 0);
				close_socket(d);
				return;
			}
		}


		//	write_to_buffer(d, echo_on_str, 0);
		ProtocolNoEcho(d,false);

		if (check_playing(d,ch->name))
			return;

		if (check_reconnect(d, ch->name, TRUE))
			return;

		sprintf(log_buf, "%s@%s has connected.", ch->name, d->host);
		log_string(log_buf);

		ch->pcdata->old_pwd = str_dup(ch->pcdata->pwd);

		/* OLD character who doesn't have an email on file with us will be prompted for it here. */
		if (ch->pcdata->email == NULL) {
			write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you in case you lose your password.\n\r"
				"It will not be distributed to any third parties or abused in any way.\n\r", 0);
			send_to_char("\n\rEnter your e-mail address: ", ch);
			d->connected = CON_GET_EMAIL;
			return;
		}
        if (ch->pcdata->need_change_pw == TRUE || ch->pcdata->pwd_vers < 1) {
	        send_to_char("\n\rYou are required to set a new password. Please do so now.\n\rPassword: ",ch);
	        d->connected = CON_CHANGE_PASSWORD;
	        return;
        }
		if (IS_IMMORTAL(ch))
		{
			send_to_char("{BWelcome, Immortal.{x\n\r\n\r", ch);
			do_function(ch, &do_imotd, "");
			if(ch->tot_level >= MAX_LEVEL) {
				if(wizlock) send_to_char("\n\r{b-{B==={C=={W[ {YWIZLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
				if(newlock) send_to_char("\n\r{b-{B==={C=={W[ {GNEWLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
			}
			send_to_char("\n\r{WCurrent active projects:{x\n\r", ch);
			do_function(ch, &do_project, "list open");
			send_to_char("[Hit Return to continue]\n\r", ch);
			d->connected = CON_READ_IMOTD;
		}
		else
		{
			do_function(ch, &do_motd, "");
			d->connected = CON_READ_MOTD;
		}

		break;

	case CON_CHANGE_PASSWORD:
//		send_to_char("\n\rPassword: ", ch);

		if (argument[0] == '\0')
			return;
		if (ch->pcdata->pwd_vers < 1) {
		if (!strcmp(crypt(argument, ch->pcdata->old_pwd), ch->pcdata->old_pwd))
		{
			send_to_char("Password must be DIFFERENT from your current password!\n\rPassword: ", ch);
			return;
		}
		}
		else
		{
			if (!strcmp(sha256_crypt(argument), ch->pcdata->old_pwd))
		{
			send_to_char("Password must be DIFFERENT from your current password!\n\rPassword: ", ch);
			return;
		}
		}


		if (!acceptablePassword(d, argument))
			return;

		pwdnew = sha256_crypt(argument);

		free_string(ch->pcdata->pwd);
		ch->pcdata->pwd	= str_dup(pwdnew);
		write_to_buffer(d, "Please retype new password: ", 0);

		ch->pcdata->need_change_pw = FALSE;
		d->connected = CON_CHANGE_PASSWORD_CONFIRM;
		break;

	case CON_CHANGE_PASSWORD_CONFIRM:
		if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
		{
			write_to_buffer(d, "Passwords don't match.\n\rPassword: ", 0);
			d->connected = CON_CHANGE_PASSWORD;
			return;
		}

		send_to_char("\n\r\n\r{Y***{x {RThank you. Please remember to never give your password to anybody.{Y *** {x\n\r\n\r", ch);
		if (ch->pcdata->pwd_vers < 1){
			ch->pcdata->pwd_vers = 1;
		}
		save_char_obj(d->character);
//		write_to_buffer(d, echo_on_str, 0);
		ProtocolNoEcho(d,false);

		if (IS_IMMORTAL(ch))
		{
			do_function(ch, &do_imotd, "");
			d->connected = CON_READ_IMOTD;
		}
		else
		{
			do_function(ch, &do_motd, "");
			d->connected = CON_READ_MOTD;
		}

		break;

	case CON_BREAK_CONNECT:
		switch(*argument)
		{
		case 'y' : case 'Y':
			for (d_old = descriptor_list; d_old != NULL; d_old = d_next)
			{
				d_next = d_old->next;
				if (d_old == d || d_old->character == NULL)
					continue;

				if (str_cmp(ch->name,d_old->original ?
					d_old->original->name : d_old->character->name))
				continue;

				close_socket(d_old);
			}
			if (check_reconnect(d,ch->name,TRUE))
				return;
			write_to_buffer(d,"Reconnect attempt failed.\n\rName: ",0);
			if (d->character != NULL)
			{
				free_char(d->character);
				d->character = NULL;
			}
			d->connected = CON_GET_NAME;
			break;

		case 'n' : case 'N':
			write_to_buffer(d,"Name: ",0);
			if (d->character != NULL)
			{
				free_char(d->character);
				d->character = NULL;
			}
			d->connected = CON_GET_NAME;
			break;

		default:
			write_to_buffer(d,"Please type Y or N? ",0);
			break;
		}
		break;

	case CON_CONFIRM_NEW_NAME:
		switch (*argument)
		{
		case 'y': case 'Y':
//			sprintf(buf, "\n\rEnter a password for %s: %s", ch->name, echo_off_str);
			ProtocolNoEcho(d, true);
			sprintf(buf, "\n\rEnter a password for %s: ", ch->name);
			write_to_buffer(d, buf, 0);
			d->connected = CON_GET_NEW_PASSWORD;
			break;

		case 'n': case 'N':
			write_to_buffer(d, "Name: ", 0);
			free_char(d->character);
			d->character = NULL;
			d->connected = CON_GET_NAME;
			break;

		default:
			write_to_buffer(d, "Please type yes or no: ", 0);
			break;
		}
		break;

	case CON_GET_NEW_PASSWORD:
#if defined(unix)
		write_to_buffer(d, "\n\r", 2);
#endif
		if (!acceptablePassword(d, argument))
			return;

		pwdnew = sha256_crypt(argument);

		free_string(ch->pcdata->pwd);
		ch->pcdata->pwd	= str_dup(pwdnew);
		ch->pcdata->pwd_vers = 1;
		write_to_buffer(d, "Please retype password: ", 0);
		d->connected = CON_CONFIRM_NEW_PASSWORD;

		ch->pcdata->need_change_pw = FALSE;
		break;

	case CON_CONFIRM_NEW_PASSWORD:
#if defined(unix)
		write_to_buffer(d, "\n\r", 2);
#endif

		if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
		{
			write_to_buffer(d, "Passwords don't match.\n\r\n\rRetype password: ", 0);
			d->connected = CON_GET_NEW_PASSWORD;
			return;
		}

//		write_to_buffer(d, echo_on_str, 0);
		ProtocolNoEcho(d,false);

		write_to_buffer(d,	"\n\rPlease enter a valid e-mail address at which we can reach you in case you lose your password.\n\r"
							"It will not be distributed to any third parties or abused in any way.\n\r", 0);

		send_to_char("\n\rEnter your e-mail address: ", ch);

		d->connected = CON_GET_EMAIL;
		break;

	case CON_GET_ASCII:
		switch (argument[0])
		{
		case 'y': case 'Y':
			SET_BIT(ch->act, PLR_COLOUR);
			break;
		case 'n': case 'N':
			break;
		default:
			write_to_buffer(d, "Yes/No.\n\rWould you like ascii colour? ", 0);
			return;
		}

		send_to_char("\n\r{r-----{R======{D//// {WWelcome to the world of Sentience! {D\\\\{R======{r-----{x\n\r",ch);
		write_to_buffer(d, "\n\r", 2);

		wiznet("Newbie alert!  $N sighted.",ch,NULL,WIZ_NEWBIE,0,0);
		/* wiznet(log_buf,NULL,NULL,WIZ_NEWBIE,0,get_trust(ch));*/

		send_to_char("\n\r{YChoose an alignment ({GGood/Neutral/Evil{Y):{x ", ch);
		d->connected = CON_GET_ALIGNMENT;
		break;

	case CON_GET_ALIGNMENT:
		switch (argument[0])
		{
		case 'g' : case 'G' : ch->alignment = 750;  break;
		case 'n' : case 'N' : ch->alignment = 0;	break;
		case 'e' : case 'E' : ch->alignment = -750; break;
		default:
			if (argument[0] != '\0')
				write_to_buffer(d,"That's not a valid alignment.\n\r",0);

			send_to_char("\n\r{YChoose an alignment ({GGood/Neutral/Evil{Y):{x ", ch);
			return;
		}

		/* Evil*/
		// Align checks out
		if (ch->alignment < 0)
		{
		    send_to_char("\n\r{xYou have chosen to be {REvil{x.\n\r\n\r", ch);

			send_to_char("{YThe following races are available to you: \n\r", ch);
			send_to_char("{GDrow        {B - Dark elves who are masters of tact and dexterity.\n\r", ch);
			send_to_char("{GVampire     {B - The walking dead. Lots of extra skills but lots of vulnerabilities.\n\r", ch); //-- Disabled by Gairun 20111219
			send_to_char("{GSith        {B - Half man, half snake. Natural hunt, toxins, and a nasty tail.\n\r", ch);
			send_to_char("{GMinotaur    {B - The ultimate warrior. Very strong, tough and has a thick warm coat, but vulnerable to fire. \n\r", ch);
		}

		/* Good*/
		if (ch->alignment > 0)
		{
			send_to_char("\n\r{xYou have chosen to be {WGood{x.\n\r\n\r", ch);

			send_to_char("{YThe following races are available to you:\n\r", ch);
			send_to_char("{GDraconian   {B - Dragon/human cross. Can fly and breathe fire, frost, acid, gas, or lightning.\n\r", ch);
			send_to_char("{GSlayer      {B - Ancient holy fighters who can shapeshift into beasts. 25%% extra damage against evil.\n\r", ch); //-- Disabled by Gairun 20111219
			send_to_char("{GTitan       {B - Very strong, have an extra attack, vulnerable to lightning.\n\r", ch);
			send_to_char("{GElf         {B - Very high stats, resistant to magic and fast mana regen.\n\r", ch);
		}

		/* Neutral*/
		if (ch->alignment == 0)
		{
			send_to_char("\n\r{xYou have chosen to be {GNeutral{x.\n\r\n\r", ch);
			send_to_char("{GDwarf       {B - Hardy, great at combat, and masters of craftwork.\n\r", ch);
			send_to_char("{GHuman       {B - Average stats, no particular strengths or vunerabilities.\n\r", ch);
			send_to_char("{GLich        {B - Undead lords of magic - many magical powers but physically weak.\n\r", ch); //-- Disabled by Gairun 20111219
//		    send_to_char("{DPraxis      {D - Coming soon!\n\r",ch);
		}

		send_to_char("\n\r{xYou will now be asked which race you would like your character to\n\r", ch);
		send_to_char("{xbelong to. Each race has its advantages and disadvantages. You can\n\r", ch);
		send_to_char("{xinspect each of the races by typing \"help <race>\". To get a summary\n\r", ch);
		send_to_char("{xof all the races, type \"help\".\n\r", ch);

		send_to_char("\n\r{YChoose your race (type \"help <race>\" for more information):{x ", ch);
		d->connected = CON_GET_NEW_RACE;
		break;

	case CON_GET_NEW_RACE:
		one_argument(argument,arg);

		sprintf(races, "\n\r{YChoose your race");
		add_possible_races(ch, races);
		strcat(races, "{Y:{x ");

		if (!strcmp(arg,"help"))
		{
			argument = one_argument(argument,arg);
			if (argument[0] == '\0' || !str_prefix(argument, "races"))
			{
				send_to_char("{b++++++{B------{C++++++ {WRACES SUMMARY {C++++++{B------{b++++++{x\n\r\n\r", ch);
				if ((help = lookup_help_exact("races grid", 0, topHelpCat)) != NULL)
					send_to_char(help->text, ch);
			}
			else
			{
				if ((race = race_lookup(argument)) != 0 &&
					(help = lookup_help_exact(race_table[race].name, 0, topHelpCat)) != NULL)
				{
					sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
					send_to_char(buf, ch);
					send_to_char(help->text, ch);
				}
				else
					send_to_char("That's not a race.\n\r", ch);
			}

			send_to_char(races, ch);
			break;
		}

		if (arg[0] == '\0') {
			send_to_char(races, ch);
			break;
		}

		if ((race = race_lookup(argument)) == 0) {
			send_to_char("There is no such race.\n\r", ch);
			send_to_char(races, ch);
			break;
		}

		if (!race_table[race].pc_race || race == grn_shaper) {
			send_to_char("That isn't a player race.\n\r", ch);
			send_to_char(races, ch);
			break;
		}

		if (pc_race_table[race].remort) {
			send_to_char("You cannot choose that race.\n\r", ch);
			send_to_char(races, ch);
			break;
		}

		if ((ch->alignment == 0 && pc_race_table[race].alignment != ALIGN_NONE) ||
			(ch->alignment  < 0 && pc_race_table[race].alignment != ALIGN_EVIL) ||
			(ch->alignment  > 0 && pc_race_table[race].alignment != ALIGN_GOOD))
		{
			if (ch->alignment == 0)		send_to_char("That is not a neutral aligned race.\n\r", ch);
			else if (ch->alignment < 0) send_to_char("That is not an evil aligned race.\n\r", ch);
			else						send_to_char("That is not a good aligned race.\n\r", ch);

			send_to_char(races, ch);
			break;
		}
		ch->race = race;

		/* initialize stats */
		for (i = 0; i < MAX_STATS; i++) {
			ch->perm_stat[i] = pc_race_table[race].stats[i];
			ch->dirty_stat[i] = TRUE;
		}
		ch->act2        = ch->act2|race_table[race].act2;
		ch->affected_by = ch->affected_by|race_table[race].aff;

		ch->imm_flags_perm = race_table[race].imm;
		ch->res_flags_perm = race_table[race].res;
		ch->vuln_flags_perm = race_table[race].vuln;
		/* 20203003 - Tieryo - Fixing racial affects */
		ch->affected_by_perm = race_table[race].aff;
		ch->affected_by2_perm = race_table[race].aff2;

		ch->imm_flags	= ch->imm_flags|race_table[race].imm;
		ch->res_flags	= ch->res_flags|race_table[race].res;
		ch->vuln_flags	= ch->vuln_flags|race_table[race].vuln;
		ch->form	= race_table[race].form;
		ch->parts	= race_table[race].parts;

		/* add skills */
		for (i = 0; i < 5; i++)
		{
			if (pc_race_table[race].skills[i] == NULL)
				break;

			group_add(ch,pc_race_table[race].skills[i],FALSE);
		}

		ch->size = pc_race_table[race].size;

		send_to_char("\n\r{YWhat is your sex (M/F)?{x ", ch);
		d->connected = CON_GET_NEW_SEX;
		break;

	case CON_GET_NEW_SEX:
		switch (argument[0])
		{
		case 'm': case 'M': ch->sex = ch->pcdata->true_sex = SEX_MALE; break;
		case 'f': case 'F': ch->sex = ch->pcdata->true_sex = SEX_FEMALE; break;
		default:
			if (argument[0] != '\0')
				send_to_char("{xThat's not a sex.\n\r", ch);

			send_to_char("\n\r{YWhat is your sex (M/F)?{x ", ch);
			return;
		}

		send_to_char("\n\rIn Sentience, there are four main classes to choose from. From \n\r", ch);
		send_to_char("these four classes you may choose a subclass that belong to these\n\r", ch);
		send_to_char("classes. Within each subclass you must complete 30 levels before\n\r", ch);
		send_to_char("advancing to master another class, inheriting each skill set\n\r", ch);
		send_to_char("as you go. After 120 levels you may REMORT and master four\n\r", ch);
		send_to_char("brand new subclasses.\n\r\n\r", ch);

		send_to_char("For help on a specific class, type help <class>.\n\r\n\r", ch);

		strcpy(buf, "{YSelect the class you would like to begin with {B[{C");
		for (iClass = 0; iClass < MAX_CLASS; iClass++)
		{
			if (iClass > 0)
				strcat(buf, " ");
			strcat(buf, class_table[iClass].name);
		}
		strcat(buf, "{B]{Y:{x ");
		send_to_char(buf, ch);
		d->connected = CON_GET_NEW_CLASS;
		break;

	case CON_GET_NEW_CLASS:
		sprintf(classes, "\n\r{YChoose your class {B[{Cmage cleric thief warrior{B]{Y:{x ");
		if (!str_prefix("help", argument))
		{
			argument = one_argument(argument,arg);
			if (argument[0] == '\0')
			{
				send_to_char("{b++++++{B------{C++++++ {WCLASSES AND SUBCLASSES{C++++++{B------{b++++++{x\n\r\n\r", ch);
				if ((help = lookup_help_exact("classes professions", 0, topHelpCat)) != NULL)
					send_to_char(help->text, ch);
			}
			else
			{
				if ((iClass = class_lookup(argument)) != -1 &&
					(help = lookup_help_exact(class_table[iClass].name, 0, topHelpCat)) != NULL)
				{
					sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
					send_to_char(buf, ch);
					send_to_char(help->text, ch);
				}
				else
					send_to_char("That's not a class.\n\r", ch);
			}

			send_to_char(classes, ch);
			break;
		}

		if (argument[0] == '\0') {
			send_to_char(classes, ch);
			break;
		}

		if ((iClass = class_lookup(argument)) == -1)
		{
			send_to_char("{xThat's not a class.\n\r", ch);
			send_to_char(classes, ch);
			break;
		}

		ch->pcdata->class_current = iClass;

		switch(iClass)
		{
		case CLASS_MAGE:
			ch->pcdata->class_mage = CLASS_MAGE;
			ch->pcdata->class_cleric = -1;
			ch->pcdata->class_thief = -1;
			ch->pcdata->class_warrior = -1;
			break;
		case CLASS_CLERIC:
			ch->pcdata->class_mage = -1;
			ch->pcdata->class_cleric = CLASS_CLERIC;
			ch->pcdata->class_thief = -1;
			ch->pcdata->class_warrior = -1;
			break;
		case CLASS_THIEF:
			ch->pcdata->class_mage = -1;
			ch->pcdata->class_cleric = -1;
			ch->pcdata->class_thief = CLASS_THIEF;
			ch->pcdata->class_warrior = -1;
			break;
		case CLASS_WARRIOR:
			ch->pcdata->class_mage = -1;
			ch->pcdata->class_cleric = -1;
			ch->pcdata->class_thief = -1;
			ch->pcdata->class_warrior = CLASS_WARRIOR;
			break;
		}

		send_to_char("\n\r{xFor each class there are a possible of three subclasses. Each subclass\n\r", ch);
		send_to_char("is for a particular alignment. A good aligned person may choose from\n\r", ch);
		send_to_char("the neutral or good subclasses. An evil aligned person may choose\n\r", ch);
		send_to_char("from either evil, or neutral subclasses. A neutral aligned person\n\r", ch);
		send_to_char("however may choose from either good, neutral or evil aligned subclasses.\n\r", ch);
		send_to_char("For help on a specific subclass, type help <subclass name>.\n\r\n\r", ch);

		strcpy(buf, "{YSelect the subclass you would like to begin with ");
		add_possible_subclasses(ch, buf);
		strcat(buf, "{Y:{x ");

		send_to_char(buf, ch);
		d->connected = CON_GET_SUB_CLASS;
		break;

	case CON_GET_SUB_CLASS:
		sprintf(subclasses, "\n\r{YChoose your subclass ");
		add_possible_subclasses(ch, subclasses);
		strcat(subclasses, "{Y:{x ");

		if (!str_prefix("help", argument))
		{
			argument = one_argument(argument,arg);
			if (argument[0] == '\0' || !str_prefix(argument, "subclasses") || !str_prefix(argument, "classes"))
			{
				send_to_char("{b++++++{B------{C++++++ {WCLASSES AND SUBCLASSES {C++++++{B------{b++++++{x\n\r\n\r", ch);
				if ((help = lookup_help_exact("classes professions", 0, topHelpCat)) != NULL)
					send_to_char(help->text, ch);
			}
			else
			{
				sprintf(buf, "%s", argument);
				for (iClass = 0; iClass < MAX_SUB_CLASS; iClass++)
				{
					if (!str_prefix(buf, sub_class_table[iClass].name[ch->sex]) &&
						!sub_class_table[iClass].remort)
						break;
				}

				if (iClass == MAX_SUB_CLASS)
					send_to_char("That's not a subclass.\n\r", ch);
				else
				{
					/* Kind of a hack for now*/
					if (!str_cmp(sub_class_table[iClass].name[ch->sex], "witch") ||
						!str_cmp(sub_class_table[iClass].name[ch->sex], "warlock"))
						sprintf(buf, "Warlock Witch");
					else if (!str_cmp(sub_class_table[iClass].name[ch->sex], "sorcerer") ||
							!str_cmp(sub_class_table[iClass].name[ch->sex], "sorceress"))
						sprintf(buf, "Sorcerer Sorceress");
					else
						sprintf(buf, sub_class_table[iClass].name[ch->sex]);

					if ((help = lookup_help_exact(buf, 0, topHelpCat)) != NULL)
					{
						sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
						send_to_char(buf, ch);
						send_to_char(help->text, ch);
					}
				}
			}

			send_to_char(subclasses, ch);
			return;
		}

		if (argument[0] == '\0') {
			send_to_char(subclasses, ch);
			return;
		}

		iClass = sub_class_lookup(ch, argument);
		if (iClass == -1)
		{
			send_to_char("{xThat's not a subclass you can choose.\n\r", ch);
			send_to_char(subclasses, ch);
			return;
		}

		ch->pcdata->sub_class_current = iClass;

		if (ch->pcdata->class_mage != -1)
			ch->pcdata->sub_class_mage = iClass;
		else if (ch->pcdata->class_cleric != -1)
			ch->pcdata->sub_class_cleric = iClass;
		else if (ch->pcdata->class_thief != -1)
			ch->pcdata->sub_class_thief = iClass;
		else if (ch->pcdata->class_warrior != -1)
			ch->pcdata->sub_class_warrior = iClass;

		sprintf(log_buf, "%s@%s new player.", ch->name, d->host);
		log_string(log_buf);

		SET_BIT(ch->act, PLR_NO_CHALLENGE);

		group_add(ch,"global skills",FALSE);
		group_add(ch,class_table[ch->pcdata->class_current].base_group,FALSE);
		group_add(ch, sub_class_table[ch->pcdata->sub_class_current].default_group, FALSE);

		/* Make it so no notes appear*/
		ch->pcdata->last_note = current_time;
		ch->pcdata->last_idea = current_time;
		ch->pcdata->last_penalty = current_time;
		ch->pcdata->last_news = current_time;
		ch->pcdata->last_changes = current_time;

		send_to_char("\n\r{YPress ENTER to begin your journey, adventurer!{W\n\r", ch);
		buf[0] = '\0';

		/* Set up default toggles*/
		for (i = 0; pc_set_table[i].name != NULL; i++)
		{
			if (pc_set_table[i].default_state == SETTING_ON && ch->tot_level >= pc_set_table[i].min_level)
			{
				if (pc_set_table[i].vector != 0)
				{
					vector = pc_set_table[i].vector;
					field = &ch->act;
				}
				else if (pc_set_table[i].vector2 != 0)
				{
					vector = pc_set_table[i].vector2;
					field = &ch->act2;
				}
				else if (pc_set_table[i].vector_comm != 0)
				{
					vector = pc_set_table[i].vector_comm;
					field = &ch->comm;
				}
				else
					continue;

				if (pc_set_table[i].inverted)
				{
					REMOVE_BIT(*field, vector);
				}
				else
				{
					SET_BIT(*field, vector);
				}
			}
		}

		ch->level     = 0;
		ch->tot_level = 0;

		/* Set up weapon skill*/
		switch (ch->pcdata->class_current)
		{
		case CLASS_MAGE:	weapon = gsn_quarterstaff;	break;
		case CLASS_CLERIC:	weapon = gsn_quarterstaff;	break;
		case CLASS_THIEF:	weapon = gsn_dagger;		break;
		case CLASS_WARRIOR:	weapon = gsn_sword;		break;
		default:
			bug("nanny: bad current class in weapon pick", 0);
			weapon = gsn_sword;
			break;
		}

		ch->pcdata->learned[weapon] = 50;
		d->connected = CON_READ_MOTD;
		break;

	case CON_READ_IMOTD:
		write_to_buffer(d,"\n\r",2);
		do_function(ch, &do_motd, "");
		d->connected = CON_READ_MOTD;
		break;

	case CON_READ_MOTD:
		/* VIZZMARK */
		if (ch->pcdata == NULL || ch->pcdata->pwd[0] == '\0')
		{
			write_to_buffer(d, "Warning! Null password!\n\r",0);
			write_to_buffer(d, "Type 'password null <new password>' to fix.\n\r",0);
		}

		list_appendlink(loaded_chars, ch);
		// Temprarily disabled for reconnect crash
		// list_appendlink(loaded_players, ch);
		d->connected	= CON_PLAYING;

		if (ch->pcdata->old_pwd != NULL)
		{
			free_string(ch->pcdata->old_pwd);
			ch->pcdata->old_pwd = NULL;
		}

		reset_char(ch);

		/* Show how many players on */
		playernum = 0;
		for (d2 = descriptor_list; d2 != NULL; d2 = d2->next)
		{
			if (d2->connected == CON_PLAYING && d2 != d &&
				can_see(d->character, d2->character))
				playernum++;
		}

	/*	 No the one that logged on isn't playing yet!*/
	/*		CON_READ_MOTD != CON_PLAYING*/
	/*        if (playernum != 0)*/
	/*	    --playernum; // One less because the one who just logged in is a player*/

		sprintf(buf, "{MThe current system time is {x%s{x\r", ctime(&current_time));
		send_to_char(buf, ch);

		sprintf(buf, "{MLast reboot was at {x%s{x\r", str_boot_time);
		send_to_char(buf, ch);

		sprintf(buf, "{MThere are currently {W%ld{M players online.{x\n\r", playernum);
		send_to_char(buf, ch);

		bool moved_to_room = FALSE;

		///////////////////////////////////////////////
		// New player
		if (ch->level == 0)
		{
			int prime_stat = class_table[ch->pcdata->class_current].attr_prime;
			ch->perm_stat[prime_stat] += 3;
			ch->dirty_stat[prime_stat] = TRUE;

			ch->exp	= 0;
			ch->hit	= ch->max_hit;
			ch->mana	= ch->max_mana;
			ch->move	= ch->max_move;
			ch->train	 = 3;
			ch->practice = 5;
			sprintf(buf, "{x");
			set_title(ch, buf);
			if (fBootstrap)
			{
				log_string("Bootstrapping game");
				// Bootstrap the building process and make this player a MAX_LEVEL
				send_to_char("{WBOOTSTRAPPING SENTIENCE!{x\n\r", ch);
				sprintf(buf, "Upgrading you to level {Y%d{x.\n\r", MAX_LEVEL);
				send_to_char(buf, ch);

				ch->level = MAX_LEVEL;
				ch->tot_level = MAX_LEVEL;
				ch->pcdata->security = 9;
			    free_string(ch->prompt);
    			ch->prompt = str_dup("{x[%o][%O] %R - %h> %c");

				IMMORTAL_DATA *immortal = new_immortal();

				immortal->name = str_dup(ch->name);
				immortal->imm_flag = str_dup("{R  Immortal  {x");
				immortal->created = current_time;

				/* start them off as unassigned */
				immortal->next = immortal_list;
				immortal_list = immortal;

				ch->pcdata->immortal = immortal;
				SET_BIT(ch->act, PLR_HOLYLIGHT);
				SET_BIT(ch->act2, PLR_HOLYWARP);
				SET_BIT(ch->act2, PLR_HOLYAURA);

				// Create bootstrap area
				AREA_DATA *pArea = new_area();
				pArea->uid = gconfig.next_area_uid++;
				free_string(pArea->name);
				pArea->anum = 1;
				top_area = 1;
				pArea->name = str_dup("Bootstrap");
				free_string(pArea->file_name);
				pArea->file_name = str_dup("bootstrap.are");
				area_first = pArea;		// area_first is NULL for fBootstrap to be set TRUE
				area_last = pArea;

				ROOM_INDEX_DATA *pRoom = new_room_index();
				pRoom->area = pArea;
				list_appendlink(pArea->room_list, pRoom);
				pRoom->vnum	= 1;

				int iHash = pRoom->vnum % MAX_KEY_HASH;
				pRoom->next	= pArea->room_index_hash[iHash];
				pArea->room_index_hash[iHash] = pRoom;

				pArea->top_vnum_room = 1;

				// Update the reserved rooms to this location.
				room_wnum_default.pArea = pArea;
				room_wnum_default.vnum = 1;

				room_wnum_school = room_wnum_default;
				room_wnum_death = room_wnum_default;
				room_wnum_temple = room_wnum_default;
				room_wnum_chat = room_wnum_default;
				room_wnum_limbo = room_wnum_default;
				room_wnum_arena = room_wnum_default;
				room_wnum_donation = room_wnum_default;

				// Save the bootstrapping
				gconfig_write();
				log_string("Saving bootstrapped area");
				save_area_list();

				for (pArea = area_first; pArea; pArea = pArea->next)
				{
					save_area_new(pArea);

					REMOVE_BIT(pArea->area_flags, AREA_CHANGED);
				}

				ch->in_room = pRoom;

				do_function(ch, &do_changes, "catchup");
				save_char_obj(ch);
				send_to_char("\n\r",ch);
				send_to_char("Bootstrapping process complete.\n\r", ch);

				fBootstrap = FALSE;
			}
			else
			{
				moved_to_room = TRUE;
				char_to_room(ch, room_index_school);
				do_function(ch, &do_changes, "catchup");
				SET_BIT(ch->comm, COMM_NO_OOC);
				SET_BIT(ch->comm, COMM_NO_FLAMING);
				send_to_char("\n\r",ch);
				for (d2 = descriptor_list; d2 != NULL; d2 = d2->next)
				{
					if (d2->connected == CON_PLAYING && d2->character != ch &&
						!IS_SET(d2->character->comm, COMM_NOANNOUNCE))
					{
						act("{MThe Town Crier Announces 'All welcome $N, a new adventurer to Sentience!'{x",
							d2->character,ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					}
				}

				ch->level = 1;
				ch->tot_level = 1;
			}
		}

		// Fix variable and dungeon referencs
		variable_dynamic_fix_mobile(ch);
		resolve_dungeons_player(ch);
		resolve_instances_player(ch);
		resolve_ships_player(ch);


		if (!moved_to_room)
		{
			if (ch->in_room != NULL)
			{
				char login_buf[MSL];
				sprintf(login_buf, "ROOM: %ld#%ld\n\r", ch->in_room->area->uid, ch->in_room->vnum);
				send_to_char(login_buf, ch);

				char_to_room(ch, ch->in_room);
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
						char_to_room (ch, room_index_chat);
					}
					else
					{
						char_to_room (ch, room_index_temple);
					}
				}
			}
		}

		act("$$n has entered the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        MXPSendTag(d,"<VERSION>");
		for (d2 = descriptor_list; d2 != NULL; d2 = d2->next)
		{
			if (d2->connected == CON_PLAYING && !IS_IMMORTAL(d->character) &&
				d2->character != ch && IS_SET(d2->character->comm, COMM_NOTIFY))
				act("{B$$N has entered the game.{x", d2->character, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

		/* Kick chars of wrong align out of their church*/
		if (ch->church != NULL)
		{
			if ((ch->alignment < 0 && ch->church->alignment == CHURCH_GOOD) ||
				(ch->alignment > 0 && ch->church->alignment == CHURCH_EVIL))
			{
				act("{YAs you enter Sentience, you feel your church's faith has been changed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{YYou feel your psychic link to $T being severed.{x", ch, NULL, NULL, NULL, NULL, NULL, ch->church->name, TO_CHAR);
				remove_member(ch->church_member);
				ch->church = NULL;
			}
		}

		/* Send a message to the church*/
		if (ch->church != NULL)
		{
			sprintf(buf, "{Y[%s has entered the game.]{x\n\r", ch->name);
			church_echo(ch->church, buf);
			list_addlink(ch->church->online_players, ch);
		}

		/* Unscrew people's classes, subclasses and skills if they are messed up somehow.*/
		if (!IS_IMMORTAL(ch))
		{
			descrew_subclasses(d->character);

			if (!has_correct_classes(d->character))
				fix_broken_classes(d->character);

			update_skills(d->character);
		}

		// Add connection to appropriate lists
		connection_add(d);

		wiznet("$N has entered the game.", d->character, NULL, WIZ_LOGINS, 0, 0);

		do_function(ch, &do_look, "auto");

		do_function(ch, &do_unread, "");

		// LOGIN TRIGGER
		script_login(ch);
		break;

	/* Get the player's e-mail if it's not in the pfile already */
	case CON_GET_EMAIL:
		if (argument[0] == '\0') {
			send_to_char("Enter your e-mail address: ", ch);
			return;
		}

		if (strlen(argument) < 5 || str_infix("@", argument)) {
			send_to_char("\n\rInvalid e-mail address. Enter your e-mail address: ", ch);
			return;
		}

		ch->pcdata->email = str_dup(argument);

		/* New char, continue with the char creation process */
		if (ch->tot_level == 0) {
			write_to_buffer(d, "\n\rWould you like ascii colour (Y/N)? ", 0);
			d->connected = CON_GET_ASCII;
		} else { /* Old char, send them on their merry way */
			write_to_buffer(d, "\n\rYour e-mail address has been saved.\n\r\n\r[Hit Return to continue]\n\r", 0);
			if (IS_IMMORTAL(ch))
				d->connected = CON_READ_IMOTD;
			else
				d->connected = CON_READ_MOTD;
		}
	}
}


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
	return FALSE;
    }

    /*
     * Length restrictions.
     */
    if (strlen(name) <  3)
	return FALSE;

    if (strlen(name) > 12)
	return FALSE;

    /*
     * Alphanumerics only.
     * Lock out IllIll twits.
     */
    {
	char *pc;
	bool fIll,adjcaps = FALSE,cleancaps = FALSE;
 	int total_caps = 0;

	fIll = TRUE;
	for (pc = name; *pc != '\0'; pc++)
	{
	    if (!isalpha(*pc))
		return FALSE;

	    if (isupper(*pc)) /* ugly anti-caps hack */
	    {
		if (adjcaps)
		    cleancaps = TRUE;
		total_caps++;
		adjcaps = TRUE;
	    }
	    else
		adjcaps = FALSE;

	    if (LOWER(*pc) != 'i' && LOWER(*pc) != 'l')
		fIll = FALSE;
	}

	if (fIll)
	    return FALSE;

	if (cleancaps || (total_caps > (strlen(name)) / 2 && strlen(name) < 3))
	    return FALSE;
    }

#if 0
   /*
    * Prevent players from naming themselves after mobs.
	* TODO: ADD A BETTER CHECK FOR THIS
    */
    {
	//extern MOB_INDEX_DATA *mob_index_hash[MAX_KEY_HASH];
	MOB_INDEX_DATA *pMobIndex;
	int iHash;

	
	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
	    for (pMobIndex  = mob_index_hash[iHash];
		  pMobIndex != NULL;
		  pMobIndex  = pMobIndex->next)
	    {
		if (is_name(name, pMobIndex->player_name))
		    return FALSE;
	    }
	}
    }
#endif

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

            return FALSE;
        }
    }

    return TRUE;
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
bool check_reconnect(DESCRIPTOR_DATA *d, char *name, bool fConn)
{
    CHAR_DATA *ch;
    ITERATOR cit;
    // Reverting for reconnect crash
    //iterator_start(&cit, loaded_players);
    iterator_start(&cit, loaded_chars);
    while(( ch = (CHAR_DATA *)iterator_nextdata(&cit)))
    {
		if (!IS_NPC(ch) &&
			(!fConn || ch->desc == NULL) &&
			!str_cmp(d->character->name, ch->name)) {
		    if (!fConn) {
				free_string(d->character->pcdata->pwd);
				d->character->pcdata->pwd = str_dup(ch->pcdata->pwd);
			} else {
				CHURCH_DATA *church;
				CHURCH_PLAYER_DATA *member;

				if (d->character->pet) {
					CHAR_DATA *pet=d->character->pet;

					char_to_room(pet, room_index_limbo);
					stop_follower(pet,TRUE);
					extract_char(pet,TRUE);
                }

				// Temporarily adding this back for reconnect crash
				free_char(d->character);
				d->character = ch;
				ch->desc	 = d;
				ch->timer	 = 0;
				send_to_char("Reconnecting.  Type replay to see missed tells.\n\r", ch);
				act("$$n has reconnected.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				sprintf(log_buf, "%s@%s reconnected.", ch->name, d->host);
				log_string(log_buf);
				wiznet("$N has relinked.", ch,NULL,WIZ_LINKS,0,0);

				d->connected = CON_PLAYING;
				MXPSendTag(d,"<VERSION>");

				if (get_eq_char(ch, WEAR_LIGHT) != NULL)
					ch->in_room->light++;

				/* resync char with his church_member both ways on reconnect */
				for (church = church_list; church != NULL; church = church->next) {
					if (church == ch->church) {
						for (member = church->people; member != NULL; member = member->next) {
							if (!str_cmp(member->name, ch->name) && member->ch == NULL)
								member->ch = ch;
						}
				    }
				}
		    }
		    iterator_stop(&cit);

			return TRUE;
		}
    }
    iterator_stop(&cit);

    return FALSE;
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
	&&   dold->connected != CON_GET_NAME
	&&   dold->connected != CON_GET_OLD_PASSWORD
	&&   !str_cmp(name, dold->original
	         ? dold->original->name : dold->character->name))
	{
	    write_to_buffer(d, "That character is already playing.\n\r",0);
	    write_to_buffer(d, "Do you wish to connect anyway (Y/N)?",0);
	    d->connected = CON_BREAK_CONNECT;
	    return TRUE;
	}
    }

    return FALSE;
}


void stop_idling(CHAR_DATA *ch)
{
	if (ch == NULL ||
		ch->desc == NULL ||
		ch->desc->connected != CON_PLAYING ||
		ch->was_in_room == NULL ||
		ch->in_room != room_index_limbo)
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
void send_to_char(const char *txt, CHAR_DATA *ch)
{
    const	char 	*point;
    		char 	*point2;
    		char 	buf[ MAX_STRING_LENGTH*4 ];
		int	skip = 0;

    buf[0] = '\0';
    point2 = buf;

    /*
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
    */

    if(txt && ch->desc)
	{
		bool capitalize = FALSE;
	    if(IS_SET(ch->act, PLR_COLOUR))
	    {
			for(point = txt ; *point ; point++)
	        {
			    if(*point == '{')
			    {
					point++;

					if( *point == '+' )
						capitalize = TRUE;
					else {
						skip = colour(*point, ch, point2);
						point2 += skip;
					}
					continue;
			    }

			    if( capitalize && isalpha(*point) )
				{
			    	*point2 = UPPER(*point);	// Make uppercase
			    	capitalize = FALSE;
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
						capitalize = TRUE;

					continue;
				}
			    if( capitalize && isalpha(*point) )
				{
			    	*point2 = UPPER(*point);	// Make uppercase
			    	capitalize = FALSE;
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


/*
 * Page to one char, new colour version, by Lope.
 */
void page_to_char(const char *txt, CHAR_DATA *ch)
{
    const	char	*point;
    		char	*point2;
    		char	*buf;
    		char cbuf[20];
		int	skip = 0, len;

    if(txt && ch->desc)
	{
		bool capitalize = FALSE;
	    if(IS_SET(ch->act, PLR_COLOUR))
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
						capitalize = TRUE;
					else {
						skip = colour(*point, ch, point2);
						point2+=skip;
					}
					continue;
				}
			    if( capitalize && isalpha(*point) )
				{
			    	*point2 = UPPER(*point);	// Make uppercase
			    	capitalize = FALSE;
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
						capitalize = TRUE;
					continue;
				}
			    if( capitalize && isalpha(*point) )
				{
			    	*point2 = UPPER(*point);	// Make uppercase
			    	capitalize = FALSE;
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
			write_to_buffer(d,buffer,strlen(buffer));
			for (chk = d->showstr_point; *chk && isspace(*chk); chk++);

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
    char 		*pbuff;
    char 		buffer[ MAX_STRING_LENGTH*2 ];
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
	    see_all = FALSE;
            ++str;

            if( *str == '$' )
            {
				see_all = TRUE;
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
	{   pbuff = buffer;
	    colourconv(pbuff, buf, to);
            write_to_buffer(to->desc, buffer, 0);
	}

	else
	if (MOBtrigger)
	    p_act_trigger(buf, to, NULL, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }

    if (MOBtrigger && (type == TO_ROOM || type == TO_NOTVICT))
    {
	OBJ_DATA *obj, *obj_next;
	CHAR_DATA *tch, *tch_next;

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

	    for (obj = tch->carrying; obj; obj = obj_next)
	    {
		obj_next = obj->next_content;
		p_act_trigger(buf, NULL, obj, NULL, ch, vch, vch2, obj1, obj2, TRIG_ACT);
	    }
	}

	p_act_trigger(buf, NULL, NULL, ch->in_room, ch, vch, vch2, obj1, obj2, TRIG_ACT);
    }
}


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
    bool capitalize = FALSE;

    if(ch->desc && txt)
    {
	if(IS_SET(ch->act, PLR_COLOUR))
	{
	    for(point = txt ; *point ; point++)
	    {
		if(*point == '{')
		{
		    point++;
		    if(*point == '+')
		    	capitalize = TRUE;
		    else {
			    skip = colour(*point, ch, buffer);
		    	while(skip-- > 0)
				++buffer;
			}
		    continue;
		}
		if( capitalize && isalpha(*point) ) {
			*buffer = UPPER(*point);
			capitalize = FALSE;
		} else
			*buffer = *point;
		*++buffer = '\0';
	    }
	    *buffer = '\0';
	}
	else
	{
	    for(point = txt ; *point ; point++)
	    {
		if(*point == '{')
		{
		    point++;
		    if(*point == '+')
		    	capitalize = TRUE;

		    continue;
		}
		if( capitalize && isalpha(*point) ) {
			*buffer = UPPER(*point);
			capitalize = FALSE;
		} else
			*buffer = *point;
		*++buffer = '\0';
	    }
	    *buffer = '\0';
	}
    }
}

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
    sh_int dir;

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
    bool lower = FALSE;
    bool upper = FALSE;
    bool number = FALSE;
    char *p;

    if (strlen(pass) < 5)
    {
	write_to_buffer(d,
	    "Password must be at least five characters long.\n\rPassword: ", 0);
	return FALSE;
    }

    for (p = pass; *p != '\0'; p++)
    {
        if (*p >= 'a' && *p <= 'z')
	    lower = TRUE;

	if (*p >= 'A' && *p <= 'Z')
	    upper = TRUE;

	if (*p >= '0' && *p <= '9')
	    number = TRUE;
    }

    if (!lower)
    {
    	write_to_buffer(d, "Password must contain a lowercase letter.\n\rPassword: ", 0);
	return FALSE;
    }

    if (!upper)
    {
    	write_to_buffer(d, "Password must contain an uppercase letter.\n\rPassword: ", 0);
	return FALSE;
    }

    if (!number)
    {
    	write_to_buffer(d, "Password must contain a number.\n\rPassword: ", 0);
	return FALSE;
    }

    for (p = pass; *p != '\0'; p++)
    {
	if (*p == '~')
	{
	    write_to_buffer(d,
		"New password not acceptable, try again.\n\rPassword: ",
		0);
	    return FALSE;
	}
    }

    return TRUE;
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
    bool found = FALSE;

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

	    found = TRUE;
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
				list_remlink(conn_immortals, d);
			else
				list_remlink(conn_players, d);
			list_remlink(conn_online, d);
		}
	}
}
