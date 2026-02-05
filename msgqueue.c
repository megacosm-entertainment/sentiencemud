/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/**
 * Message queue handler.
 */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "recycle.h"

typedef struct msgbuf {
    long mtype;
    char mtext[16000];
} message_buf;

void process_message_queue( )
{
    int msqid;
    int msgflg = IPC_CREAT | 0666;
    int result;
    int pid;
    key_t key;
    message_buf rbuf;
    message_buf sbuf;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char *argument;
    size_t buf_length = 0;

    key = 1234;

    /* Create and try and get queue */
    if ( (msqid = msgget(key, msgflg )) < 0) {
        pbugf(LOG_ERROR, "Couldn't open message queue.");
        perror("msgget");
        //exit( 1 );
    }

    /* Attempt to get a message off the queue */
    result = msgrcv(msqid, &rbuf, MAX_STRING_LENGTH, 1, IPC_NOWAIT );

    /* Did we just receive a message */
    /* Format: PID command args */
    if ( result > 0 )
    {
        /* Get text */
        argument = rbuf.mtext;
        
        sprintf( buf, "Received '%s' from queue %d with result %d", 
                argument,
                key,
                result );
        log_string( buf );

        argument = one_argument( argument, arg );

        /* First argument must be the process id */
        if ( is_number( arg ) )
        {
            // Record process ID of sender
            pid = atoi( arg );

            /* Message type "process id" */
            sbuf.mtype = pid;

            sprintf( sbuf.mtext, "Error." );

            /* Try to process command */
            argument = one_argument( argument, arg );

            /****************************************************************************
             * Try to process a "Help" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "help" ) )
            {
                BUFFER *output = NULL;

                //output = get_help( 120, argument);

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "No help with that topic.\n\r");
                }
                else
                {
                    strcpy(sbuf.mtext, buf_string(output));
                }

                free_buf(output);
            }
            else
            /****************************************************************************
             * Try to process a "churchlist" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "churchlist" ) )
            {
                BUFFER *output = NULL;

                output = get_churches_html();

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading Churches.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "playerlist" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "playerlist" ) )
            {
                BUFFER *output = NULL;

                output = get_players_html();

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading Players.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "top10pkers" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10pkers" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_PLAYER_KILLERS );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "top10cpkers" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10cpkers" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_CPLAYER_KILLERS );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "top10monsters" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10monsters" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_MONSTER_KILLERS );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "top10wealthiest" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10wealthiest" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_WEALTHIEST );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }

            else
            /****************************************************************************
             * Try to process a "top10ratio" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10ratio" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_WORST_RATIO );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }
            else
            /****************************************************************************
             * Try to process a "top10quests" command.
             ****************************************************************************/
            if ( !str_cmp( arg, "top10quests" ) )
            {
                BUFFER *output = NULL;

                output = get_stats_html( REPORT_TOP_QUESTS );

                if ( output == NULL )
                {
                    sprintf(sbuf.mtext, "Error reading statistics.\n\r");
                }
                else
                {
                    char *temp;
                    temp = format_and_colour_html( buf_string( output ) );

                    strcpy(sbuf.mtext, temp);

                    free_string( temp );
                }

                free_buf(output);

            }


            sprintf( buf, "Sending message: %s with PID=%ld", sbuf.mtext, sbuf.mtype );
            log_string( buf );

            /*
             * Send a message.
             */
            buf_length = strlen(sbuf.mtext) + 1 ;
            if (msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT) < 0) {
                printf ("%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.mtext, buf_length);
                perror("msgsnd");
                //exit(1);
            }
        }
    }
}



