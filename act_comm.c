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
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 **************************************************************************/

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

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"

/* This macro strips a string of colours and concantenates it into a local buffer.
   Necesarry to avoid memory leaks. */
#define STRIP_COLOUR(string, buffer) \
	do { \
		char *no_colour = nocolour(string); \
		strcat((buffer), no_colour); \
		free_string(no_colour); \
	} while(0)

void do_clear (CHAR_DATA * ch, char *argument)
{
	send_to_char ("\x01B[2J\x01B[H", ch);
}

void do_delet(CHAR_DATA *ch, char *argument)
{
	send_to_char("You must type the full command to delete yourself.\n\r",ch);
}


void do_delete(CHAR_DATA *ch, char *argument)
{
	char strsave[MAX_INPUT_LENGTH];

	if (IS_NPC(ch)) return;

	if (ch->tot_level > 15 || IS_REMORT(ch)) {
		send_to_char("To have your character deleted contact the Immortals.\n\r"
				"For security purposes those over level 15 may not delete themselves.\n\r", ch);
		return;
	}

	if (ch->pcdata->confirm_delete) {
		if (argument[0]) {
			send_to_char("Delete status removed.\n\r",ch);
			ch->pcdata->confirm_delete = false;
		} else {
			sprintf( strsave, "%s%c/%s",PLAYER_DIR,tolower(ch->name[0]),
			capitalize( ch->name ) );
			wiznet("$N turns $Mself into line noise.",ch,NULL,0,0,0);
			stop_fighting(ch,true);
			do_function(ch, &do_quit, NULL);
			unlink(strsave);
		}
		return;
	}

	if (argument[0]) {
		send_to_char("Syntax is 'delete' with no argument.\n\r",ch);
		return;
	}

	send_to_char("Type delete again to confirm this command.\n\r",ch);
	send_to_char("{RWARNING: this command is irreversible.{x\n\r",ch);
	send_to_char("Typing delete with an argument will undo delete status.\n\r", ch);
	ch->pcdata->confirm_delete = true;
	wiznet("$N is contemplating deletion.",ch,NULL,0,0,get_trust(ch));
}

/*  Lists all channels and their status */
void do_channels(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];

	send_to_char("{Ychannel        status{x\n\r",ch);
	send_to_char("{Y---------------------{x\n\r",ch);

	send_to_char("announcements  ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOANNOUNCE) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("gossip         ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOGOSSIP) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("OOC            ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NO_OOC) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("yell           ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOYELL) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("flaming        ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NO_FLAMING) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("war            ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOAUTOWAR) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("quotes         ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOQUOTE) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	if (IS_IMMORTAL(ch)) {
		send_to_char("god channel    ",ch);
		send_to_char((IS_SET(ch->comm,COMM_NOWIZ) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);
	}

	if (IS_SET(ch->act[0], PLR_HELPER) || IS_IMMORTAL(ch)) {
		send_to_char("helper channel ",ch);
		send_to_char((IS_SET(ch->comm,COMM_NOHELPER) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);
	}

	if (global) {
		send_to_char("GQ channel     ",ch);
		send_to_char((IS_SET(ch->comm,COMM_NOGQ) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);
	}


	send_to_char("tells          ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOTELLS) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("auction        ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOAUCTION) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("music          ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOMUSIC) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("notify         ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOMUSIC) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("quiet mode     ",ch);
	send_to_char((IS_SET(ch->comm,COMM_QUIET) ? "{WON{X\n\r" : "{DOFF{X\n\r"),ch);

	send_to_char("battle spam    ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOBATTLESPAM) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	send_to_char("form state     ", ch);
	send_to_char((IS_SET(ch->comm,COMM_SHOW_FORM_STATE) ? "{WON{X\n\r" : "{DOFF{X\n\r"),ch);

	if (ch->church) {
		send_to_char("church talks   ",ch);
		send_to_char((IS_SET(ch->comm,COMM_NOCT) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);
	}

	send_to_char("hints mode     ",ch);
	send_to_char((IS_SET(ch->comm,COMM_NOHINTS) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

	if (IS_SET(ch->comm,COMM_AFK)) send_to_char("You are AFK.\n\r",ch);

	if (ch->lines != PAGELEN) {
		if (ch->lines) {
			sprintf(buf,"You display %d lines of scroll.\n\r",ch->lines+2);
			send_to_char(buf,ch);
		} else
			send_to_char("Scroll buffering is off.\n\r",ch);
	}

	if (ch->prompt) {
		sprintf(buf,"Your current prompt is: %s\n\r",ch->prompt);
		send_to_char(buf,ch);
	}

	if (!ch->pcdata->flag || IS_NULLSTR(ch->pcdata->flag))
		send_to_char("You currently have no flag.\n\r", ch);
	else {
		sprintf(buf, "Your flag is: %s{x\n\r", ch->pcdata->flag);
		send_to_char(buf, ch);
	}

	if (!ch->pcdata->channel_flags)
		send_to_char("You will not see player flags on any channels.\n\r", ch);
	else {
		sprintf(buf, "Player flags will be displayed on: %s\n\r", flag_string(channel_flags, ch->pcdata->channel_flags));
		send_to_char(buf, ch);
	}

	if (IS_SET(ch->comm,COMM_NOTELL)) send_to_char("You cannot use tells.\n\r",ch);

	if (IS_SET(ch->comm,COMM_NOCHANNELS)) send_to_char("You cannot use channels.\n\r",ch);
}


void do_quiet(CHAR_DATA *ch, char * argument)
{
	if (IS_SET(ch->comm,COMM_QUIET))
		send_to_char("Quiet mode removed.\n\r",ch);
	else
		send_to_char("Quiet mode set.\n\r",ch);
	TOGGLE_BIT(ch->comm,COMM_QUIET);
}


void do_afk(CHAR_DATA *ch, char * argument)
{
	if (IS_NPC(ch)) return;

	if (IS_SET(ch->comm,COMM_AFK)) {
		send_to_char("AFK mode removed. Type 'replay' to see tells.\n\r",ch);

		free_string(ch->pcdata->afk_message);
		ch->pcdata->afk_message = NULL;
	} else {
		send_to_char("You are now in AFK mode.\n\r",ch);
		ch->pcdata->afk_message = NULL;

		if (argument[0]) {
			if (strlen(argument) > 250) argument[250] = '\0';
			send_to_char("AFK message set.\n\r", ch);

			ch->pcdata->afk_message = str_dup(argument);
		}
	}
	TOGGLE_BIT(ch->comm,COMM_AFK);
}


void do_replay(CHAR_DATA *ch, char *argument)
{
	if (IS_NPC(ch)) {
		send_to_char("You can't replay.\n\r",ch);
		return;
	}

	if (!buf_string(ch->pcdata->buffer)[0]) {
		send_to_char("You have no tells to replay.\n\r",ch);
		return;
	}

	page_to_char(buf_string(ch->pcdata->buffer),ch);
	clear_buf(ch->pcdata->buffer);
}


bool can_speak_channels(CHAR_DATA *ch)
{
	if (IS_SET(ch->comm,COMM_QUIET)) {
		send_to_char("You must turn off quiet mode first.\n\r",ch);
		return false;
	}

	if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM)) {
		send_to_char("No one can hear you.\n\r", ch);
		return false;
	}

	if (IS_SET(ch->comm,COMM_NOCHANNELS)) {
		send_to_char("The gods have revoked your channel priviliges.\n\r",ch);
		return false;
	}

	return true;
}

void do_ooc(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NO_OOC))
			send_to_char("Out of character channel is now ON.\n\r",ch);
		else
			send_to_char("Out of character channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NO_OOC);
	} else if(can_speak_channels(ch)) {
		REMOVE_BIT(ch->comm,COMM_NO_OOC);

		buf[0] = '\0';
		STRIP_COLOUR(argument, buf);

		if(!buf[0]) {
			send_to_char("Is that all you want to say?\n\r",ch);
			return;
		}

		if (!IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(ch, FLAG_OOC))
			sprintf(msg, "{gOOC, you say: {G%s {G%s\n\r", ch->pcdata->flag, buf);
		else
			sprintf(msg, "{gOOC, you say: {G%s{x\n\r", buf);
		send_to_char(msg, ch);
		for (d = descriptor_list; d; d = d->next) {
			CHAR_DATA *victim = d->original ? d->original : d->character;

			if (d->connected == CON_PLAYING && d->character != ch &&
				!IS_SET(victim->comm,COMM_NO_OOC) &&
				!IS_SET(victim->comm,COMM_QUIET) &&
				!is_ignoring(d->character, ch)) {
				if (!IS_NPC(d->character) && !IS_NPC(ch) && ch->pcdata->flag &&
					SHOW_CHANNEL_FLAG(d->character, FLAG_OOC))
					sprintf(msg, "%s {G%s", ch->pcdata->flag, buf);
				else
					sprintf(msg, "{G%s", buf);

				act_new("{g$$n says OOC: {G$t{x", ch, d->character,NULL,NULL,NULL, msg,NULL, TO_VICT,POS_SLEEPING,NULL);
			}
		}
	}
}


void church_echo(CHURCH_DATA *church, char *message)
{
	DESCRIPTOR_DATA *d;

	if (!church) {
		bug("Attempted to church_echo from null church.", 0);
		return;
	}

	for (d = descriptor_list; d; d = d->next) {
		CHAR_DATA *victim = d->original ? d->original : d->character;

		if (d->connected == CON_PLAYING && victim->church == church)
			send_to_char(message, victim);
	}
}


void gecho(char *message)
{
	DESCRIPTOR_DATA *d;
	for (d = descriptor_list; d; d = d->next)
		if (d->connected == CON_PLAYING)
			send_to_char(message, (d->original ? d->original : d->character));
}


void do_gossip(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NOGOSSIP))
			send_to_char("Gossip channel is now ON.\n\r",ch);
		else
			send_to_char("Gossip channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NOGOSSIP);
	} else if(can_speak_channels(ch)) {

	        /* prevent 1-letter string editor commands with no argument */
		if (strlen(argument) == 1 &&
		       (argument[0] == 'h' ||
			argument[0] == 's' ||
			argument[0] == 'f' ||
			argument[0] == 'c'))// ||
		       // argument[0] == '/'))
		{
			send_to_char("Are you sure that's all you want to say?\n\r", ch);
			return;
		}

		if (!str_prefix("r ", argument)) {
		    send_to_char("You're not in the string editor!\n\r", ch);
		    return;
		}

		if (!str_prefix("ld ", argument) || !str_prefix("lr ", argument) || !str_prefix("li ", argument) || !str_prefix("/ ", argument)) {
		    send_to_char("You're not in the string editor.\n\r", ch);
		    return;
		}


		REMOVE_BIT(ch->comm,COMM_NOGOSSIP);

		/* Make the words drunk if needed */
		if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
			argument = makedrunk(argument,ch);

		buf[0] = '\0';
		STRIP_COLOUR(argument, buf);
		if(!buf[0]) {
			send_to_char("Is that all you want to say?\n\r",ch);
			return;
		}


		if (!IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(ch, FLAG_GOSSIP))
			sprintf(msg, "{MYou gossip '%s {M%s{M'{x\n\r", ch->pcdata->flag, buf);
		else
			sprintf(msg, "{MYou gossip '%s{M'{x\n\r", buf);
		send_to_char(msg, ch);

		for (d = descriptor_list; d; d = d->next) {
			CHAR_DATA *victim = d->original ? d->original : d->character;

			if (d->connected == CON_PLAYING && d->character && d->character != ch &&
				!IS_SET(victim->comm,COMM_NOGOSSIP) &&
				!IS_SET(victim->comm,COMM_QUIET) &&
				!is_ignoring(d->character, ch)) {
				if (!IS_NPC(d->character) && !IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(d->character, FLAG_GOSSIP))
					sprintf(msg, "%s {M%s", ch->pcdata->flag, buf);
				else
					sprintf(msg, "%s", buf);
				act_new("{M$$n gossips '$t{M'{x", ch, d->character,NULL,NULL,NULL, msg,NULL, TO_VICT,POS_SLEEPING,NULL);
			}
		}
	}
}


void do_flame(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NO_FLAMING))
			send_to_char("{RFLAMING{x channel is now ON.\n\r",ch);
		else
			send_to_char("{RFLAMING{x channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NO_FLAMING);
	} else if(can_speak_channels(ch)) {
		REMOVE_BIT(ch->comm,COMM_NO_FLAMING);

		/* Make the words drunk if needed */
		if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
			argument = makedrunk(argument,ch);

		buf[0] = '\0';
		STRIP_COLOUR(argument, buf);
		if(!buf[0]) {
			send_to_char("Is that all you want to say?\n\r",ch);
			return;
		}

		if (!IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(ch, FLAG_FLAMING))
			sprintf(msg, "{r({WF{r): You flame '%s {r%s{r'{x\n\r", ch->pcdata->flag, buf);
		else
			sprintf(msg, "{r({WF{r): You flame '{r%s{r'{x\n\r", buf);
		send_to_char(msg, ch);
		for (d = descriptor_list; d; d = d->next) {
			CHAR_DATA *victim = d->original ? d->original : d->character;

			if (d->connected == CON_PLAYING && d->character != ch &&
				!IS_SET(victim->comm,COMM_NO_FLAMING) &&
				!IS_SET(victim->comm,COMM_QUIET) &&
				!is_ignoring(d->character, ch)) {
				if (!IS_NPC(d->character) && !IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(d->character, FLAG_FLAMING))
					sprintf(msg, "%s {r%s", ch->pcdata->flag, buf);
				else
					sprintf(msg, "{r%s", buf);
				act_new("{r({WF{r): $$n flames '$t{r'{x", ch, d->character,NULL,NULL,NULL,msg,NULL, TO_VICT,POS_SLEEPING,NULL);
			}
		}
	}
}


void do_helper(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NOHELPER)) {
			if(!IS_SET(ch->act[0], PLR_HELPER) && !IS_IMMORTAL(ch)) {
				send_to_char("Only helpers may toggle this channel on.\n\r", ch);
				return;
			}

			send_to_char("Helper channel is now ON.\n\r",ch);
		} else
			send_to_char("Helper channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NOHELPER);
	} else {
		if (IS_SET(ch->comm,COMM_QUIET)) {
			send_to_char("You must turn off quiet mode first.\n\r",ch);
			return;
		}

		buf[0] = '\0';
		STRIP_COLOUR(argument, buf);
		if(!buf[0]) {
			send_to_char("Is that all you want to say?\n\r",ch);
			return;
		}

		if (!IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(ch, FLAG_HELPER))
			sprintf(msg, "{Y({BH{Y)--> You say:{Y '%s {Y%s{Y'{x\n\r", ch->pcdata->flag, buf);
		else
			sprintf(msg, "{Y({BH{Y)--> You say:{Y '%s{Y'{x\n\r", buf);
		send_to_char(msg, ch);

		for (d = descriptor_list; d; d = d->next) {
			CHAR_DATA *victim = d->original ? d->original : d->character;

			if (d->connected == CON_PLAYING && victim != ch &&
				!IS_SET(victim->comm,COMM_NOHELPER) &&
				(IS_SET(victim->act[0], PLR_HELPER) || IS_IMMORTAL(victim)) &&
				!IS_SET(victim->comm,COMM_QUIET) && !is_ignoring(victim, ch)) {
				if (!IS_NPC(victim) && !IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(victim, FLAG_HELPER))
					sprintf(msg, "%s {Y%s", ch->pcdata->flag, buf);
				else
					sprintf(msg, "%s", buf);

				sprintf(msg, "{Y({BH{Y)--> %s: %s{Y'{x\n\r", ch->name, buf);
				send_to_char(msg, victim);
			}
		}
	}
}


void do_hints(CHAR_DATA *ch, char *argument)
{
	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NOHINTS))
			send_to_char("Hints channel is now ON.\n\r", ch);
		else
			send_to_char("Hints channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NOHINTS);
	} else
		send_to_char("You cannot talk over the hints channel.\n\r", ch);
}


void do_music(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NOMUSIC))
			send_to_char("Music channel is now ON.\n\r",ch);
		else
			send_to_char("Music channel is now OFF.\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NOMUSIC);
	} else if(can_speak_channels(ch)) {
		REMOVE_BIT(ch->comm,COMM_NOMUSIC);

		if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
			argument = makedrunk(argument,ch);

		buf[0] = '\0';
		STRIP_COLOUR(argument, buf);
		if(!buf[0]) {
			send_to_char("Is that all you want to say?\n\r",ch);
			return;
		}

		if (!IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(ch, FLAG_MUSIC))
			sprintf(msg, "{Y(o/~): %s {Y%s{x\n\r", ch->pcdata->flag, buf);
		else
			sprintf(msg, "{Y(o/~): %s{x\n\r", buf);
		send_to_char(msg, ch);
		for (d = descriptor_list; d; d = d->next) {
			CHAR_DATA *victim = d->original ? d->original : d->character;

			if (d->connected == CON_PLAYING && d->character != ch &&
				!IS_SET(victim->comm,COMM_NOMUSIC) &&
				!IS_SET(victim->comm,COMM_QUIET) &&
				!is_ignoring(d->character, ch)) {
				if (!IS_NPC(victim) && !IS_NPC(ch) && ch->pcdata->flag && SHOW_CHANNEL_FLAG(victim, FLAG_MUSIC))
					sprintf(msg, "%s {Y%s", ch->pcdata->flag, buf);
				else
					sprintf(msg, "%s", buf);

				act_new("{Y($$n): o/~ $t{x", ch,d->character,NULL,NULL,NULL, msg,NULL,TO_VICT,POS_SLEEPING,NULL);
			}
		}
	}
}


void do_immtalk(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	DESCRIPTOR_DATA *d;

	if (!argument[0]) {
		if (IS_SET(ch->comm,COMM_NOWIZ))
			send_to_char("Immortal channel is now ON\n\r",ch);
		else
			send_to_char("Immortal channel is now OFF\n\r",ch);
		TOGGLE_BIT(ch->comm,COMM_NOWIZ);
	} else {
		REMOVE_BIT(ch->comm,COMM_NOWIZ);

		sprintf(buf, "{B[{G$n{B]: %s{x", argument);
		act_new("{B[{G$n{B]: $t{x",ch,NULL,NULL,NULL,NULL,argument,NULL,TO_CHAR,POS_DEAD,NULL);
		for (d = descriptor_list; d; d = d->next) {
			if (d->connected == CON_PLAYING && IS_IMMORTAL(d->character) &&
				!IS_SET(d->character->comm,COMM_NOWIZ))
				act_new("{B[{G$$n{B]: $t{x",ch,d->character,NULL,NULL,NULL,argument,NULL,TO_VICT,POS_DEAD,NULL);
		}
	}
}


void do_say(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH], msg[MSL];
	int i;
	char *second;
	bool break_line = true;

	if (!argument[0]) {
	send_to_char("Say what?\n\r", ch);
	return;
	}

	if (IS_AFFECTED2(ch, AFF2_SILENCE))
	{
	send_to_char("You attempt to say something but fail!\n\r", ch);
	act("$n opens his mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	argument = makedrunk(argument,ch);

	msg[0] = '\0';
	STRIP_COLOUR(argument, msg);

	if(!msg[0]) {
		send_to_char("Say what?\n\r", ch);
		return;
	}

	buf[0] = '\0';
	for (i = 0; msg[i] != '\0'; i++)
	{
	if (msg[i] == '!'
	&& msg[i+1] != ' ')
	break_line = false;
	}

	if (break_line)
	{
	second = stptok(msg, buf, sizeof(buf), "!");
	while(*second == ' ')
	second++;

	if (*second != '\0')
	{
	sprintf(buf2, "{C'$T!{C' exclaims $n. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM);
	sprintf(buf2, "{C'$T!{C' you exclaim. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR);
	return;
	}
	}

	for (i = 0; msg[i] != '\0'; i++)
	{
	if (msg[i] == '?'
	&& msg[i+1] != ' ')
	break_line = false;
	}

	if (break_line)
	{
	second = stptok(msg, buf, sizeof(buf), "?");
	while(*second == ' ')
	second++;

	if (*second != '\0')
	{
	sprintf(buf2, "{C'$T?{C' asks $n. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM);
	sprintf(buf2, "{C'$T?{C' you ask. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR);
	return;
	}

	}

	for (i = 0; msg[i] != '\0'; i++)
	{
	if (msg[i] == '.'
	&& msg[i+1] != ' ') break_line = false;
	}

	if (break_line)
	{
	second = stptok(msg, buf, sizeof(buf), ".");
	while(*second == ' ')
	second++;

	if (*second != '\0')
	{
	sprintf(buf2, "{C'$T.{C' says $n. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM);
	sprintf(buf2, "{C'$T.{C' you say. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR);
	return;
	}
	}

	if (msg[strlen(msg)-1] == '!')
	{
	if (number_percent() < 50)
	{
	act("{C'$T{C' exclaims $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{C'$T{C' you exclaim.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	else
	{
	act("{C$n exclaims, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{CYou exclaim, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	}
	else
	if (msg[strlen(msg)-1] == '?')
	{
	if (number_percent() < 50)
	{
	act("{C$n asks, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{CYou ask, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	else
	{
	act("{C'$T{C' asks $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{C'$T{C' you ask.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	}
	else
	{
	if (number_percent() < 50)
	{
	act("{C$n says, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{CYou say, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	else
	{
	act("{C'$T{C' says $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM);
	act("{C'$T{C' you say.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR);
	}
	}

	if (!IS_NPC(ch) || IS_SWITCHED(ch))
	{
	CHAR_DATA *mob, *mob_next;
	OBJ_DATA *obj, *obj_next;

	for (mob = ch->in_room->people; mob != NULL; mob = mob_next) {
		mob_next = mob->next_in_room;
		if (!IS_NPC(mob) || mob->position == mob->pIndexData->default_pos)
			p_act_trigger(msg, mob, NULL, NULL, ch, NULL, NULL,NULL, NULL,TRIG_SPEECH  ,0,0,0,0,0);

		for (obj = mob->carrying; obj; obj = obj_next) {
			obj_next = obj->next_content;
			p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH,0,0,0,0,0);
		}
	}

	for (obj = ch->in_room->contents; obj; obj = obj_next) {
	obj_next = obj->next_content;
	p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH,0,0,0,0,0);
	}

	p_act_trigger(msg, NULL, NULL, ch->in_room, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH,0,0,0,0,0);
	}
}

void do_tells(CHAR_DATA *ch, char *argument)
{
	if (IS_SET(ch->comm, COMM_NOTELLS))
	{
	REMOVE_BIT(ch->comm, COMM_NOTELLS);
	act("You will now receive tells.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}
	else
	{
	SET_BIT(ch->comm, COMM_NOTELLS);
	act("You will no longer receive tells.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}
}


void do_tell(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char buf[MAX_STRING_LENGTH];
	char msg[2*MSL];
	CHAR_DATA *victim;

	if (IS_SET(ch->comm, COMM_NOTELL))
	{
		send_to_char("Your tells have been revoked.\n\r", ch);
		return;
	}

	if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
	{
		send_to_char("You can't seem to gather enough energy to do it.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if (arg[0] == '\0' || argument[0] == '\0')
	{
		send_to_char("Tell whom what?\n\r", ch);
		return;
	}

	if ((victim = get_char_world(NULL, arg)) == NULL || (IS_NPC(victim) && victim->in_room != ch->in_room))
	{
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (IS_SWITCHED(victim))
	{
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	/* AO 010217 respect wizi only */
	if (IS_IMMORTAL(victim) && victim->invis_level > ch->tot_level) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (is_ignoring(victim, ch) && !(!IS_NPC(ch) && IS_IMMORTAL(ch) && ch->tot_level >= victim->tot_level))
	{
		IGNORE_DATA *ignore;

		for (ignore = victim->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
		{
			if (!str_cmp(ignore->name, ch->name)) break;
		}

		sprintf(buf, "{R$E is ignoring you.{x\n\r{RReason:{x %s", ignore->reason);
		act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (IS_SET(ch->comm, COMM_QUIET) && !can_tell_while_quiet(victim, ch))
	{
		send_to_char("You must turn off quiet mode first.\n\r", ch);
		return;
	}

	if (IS_SET(victim->comm, COMM_NOTELLS) && !(!IS_NPC(ch) && IS_IMMORTAL(ch) && ch->tot_level >= victim->tot_level))
	{
		send_to_char("Your message didn't get through.\n\r", ch);
		return;
	}

	buf[0] = '\0';
	STRIP_COLOUR(argument, buf);
	if(!buf[0]) {
		send_to_char("Tell whom what?\n\r",ch);
		return;
	}


	if (victim->desc == NULL && !IS_NPC(victim))
	{
		act("$N has lost $S link... try again later.", ch,victim,NULL, NULL, NULL, NULL, NULL,TO_CHAR);

		if (!IS_NPC(ch)) {
				if(ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_TELLS))
					sprintf(msg, "{R%s tells you '%s {R%s'{x\n\r", ch->name, ch->pcdata->flag, buf);
				else
					sprintf(msg, "{R%s tells you '%s'{x\n\r", ch->name, buf);
		} else
			sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(ch, victim), buf);

		msg[2] = UPPER(msg[2]);

		add_buf(victim->pcdata->buffer,msg);
		return;
	}

	if (IS_SET(victim->comm,COMM_QUIET) && !IS_IMMORTAL(ch) &&
		!(!IS_NPC(ch) && IS_IMMORTAL(ch) && ch->tot_level >= victim->tot_level) &&
		!can_tell_while_quiet(ch, victim))
	{
		act("$E is not receiving tells.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (IS_SET(victim->comm,COMM_AFK))
	{
		if (!IS_NPC(ch)) {
				if(ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_TELLS))
					sprintf(msg, "{R%s tells you '%s {R%s'{x\n\r", ch->name, ch->pcdata->flag, buf);
				else
					sprintf(msg, "{R%s tells you '%s'{x\n\r", ch->name, buf);
		} else
			sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(ch, victim), buf);

		msg[2] = UPPER(msg[2]);
		add_buf(victim->pcdata->buffer,msg);

		sprintf(buf, "{R$E is AFK, and has been idle for %d minutes.{x", victim->timer);
		act(buf, ch,victim,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		sprintf(buf, "{RMessage: %s{x\n\r",
			victim->pcdata->afk_message == NULL ? "none" : victim->pcdata->afk_message);
		send_to_char(buf, ch);
		return;
	}

	if (!IS_NPC(ch) && ch->pcdata->flag != NULL && IS_SET(ch->pcdata->channel_flags, FLAG_TELLS))
		sprintf(msg, "{RYou tell %s '%s {R%s{R'{x\n\r", pers(victim, ch), ch->pcdata->flag, buf);
	else
		sprintf(msg, "{RYou tell %s '%s{R'{x\n\r", pers(victim, ch), buf);

	send_to_char(msg, ch);

	if (!IS_NPC(ch)) {
			if(ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_TELLS))
				sprintf(msg, "{R%s tells you '%s {R%s'{x\n\r", ch->name, ch->pcdata->flag, buf);
			else
				sprintf(msg, "{R%s tells you '%s'{x\n\r", ch->name, buf);
	} else
		sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(ch, victim), buf);

	msg[2] = UPPER(msg[2]);
	send_to_char(msg, victim);

	if (victim->timer > 1)
	{
	sprintf(buf, "{RNote: $E has been idle for %d minutes.{x", victim->timer);
	act(buf, ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	}

	victim->reply = ch;
	ch->reply     = victim;

	if (!IS_NPC(ch))
		p_act_trigger(argument, victim, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH,0,0,0,0,0);
}



void do_reply(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];

	if ((victim = ch->reply) == NULL /*|| !can_see(ch, victim)*/) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	sprintf(buf, "'%s' ", victim->name);	// Was pers(victim, ch), which posed a problem for shaped vampires and shifted vampires/slayers
	strcat(buf, argument);

	do_function(ch, &do_tell, buf);
	return;

}


void do_yell(CHAR_DATA *ch, char *argument)
{
	char buf[MSL], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (IS_SET(ch->comm, COMM_NOCHANNELS))
	{
	send_to_char("You can't yell.\n\r", ch);
	return;
	}

	if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
	{
	send_to_char("You can't seem to gather enough energy to do it.\n\r",
	ch);
	return;
	}

	if (argument[0] == '\0')
	{
	if (IS_SET(ch->comm, COMM_NOYELL))
	{
	REMOVE_BIT(ch->comm, COMM_NOYELL);
	send_to_char("You will now hear yells.\n\r", ch);
	}
	else
	{
	SET_BIT(ch->comm, COMM_NOYELL);
	send_to_char("You will no longer hear yells.\n\r", ch);
	}

	return;
	}

	/* Make the words drunk if needed */
	if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	argument = makedrunk(argument,ch);

	buf[0] = '\0';
	STRIP_COLOUR(argument, buf);
	if(!buf[0]) {
		send_to_char("Is that all you want to say?\n\r",ch);
		return;
	}

	if (!IS_NPC(ch) && ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(ch, FLAG_YELL))
	sprintf(msg, "{YYou yell '%s {Y%s'{x\n\r", ch->pcdata->flag, buf);
	else
	sprintf(msg, "{YYou yell '%s'{x\n\r", buf);

	send_to_char(msg, ch);

	for (d = descriptor_list; d != NULL; d = d->next)
	{
	if (d->connected == CON_PLAYING
	&& d->character != ch
	&& d->character->in_room != NULL
	&& d->character->in_room->area == ch->in_room->area
	&& !IS_SET(d->character->comm,COMM_QUIET)
	&& !is_ignoring(d->character, ch)
	&& !IS_SET(d->character->comm, COMM_NOYELL))
	{
	if (!IS_NPC(ch) && ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(d->character, FLAG_YELL))
	sprintf(msg, "%s {Y%s", ch->pcdata->flag, buf);
	else
	sprintf(msg, "%s", buf);

	act("{Y$n yells '$t{Y'{x",ch,d->character, NULL, NULL, NULL, msg, NULL,TO_VICT);
	}
	}
}


void do_emote(CHAR_DATA *ch, char *argument)
{
	if (IS_SWITCHED(ch))
	{
	send_to_char("You can't show your emotions.\n\r", ch);
	return;
	}

	if (argument[0] == '\0')
	{
	send_to_char("Emote what?\n\r", ch);
	return;
	}

	MOBtrigger = false;
	act("$n $T{x", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_ROOM);
	act("$n $T{x", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_CHAR);
	MOBtrigger = true;
}


void do_quit(CHAR_DATA *ch, char *argument)
{
	DESCRIPTOR_DATA *d,*d_next;
	OBJ_DATA *obj;
	AFFECT_DATA *paf;
	int id[2];
	TOKEN_DATA *token, *token_next;
	char buf[MSL];

	if (IS_SWITCHED(ch))
	{
	send_to_char("You can't quit in morphed form.\n\r", ch);
	return;
	}

	if (IS_NPC(ch))
	return;

	if (!ch->pcdata->quit_on_input) {
		if (ch->position == POS_FIGHTING)
		{
		send_to_char("No way! You are fighting.\n\r", ch);
		return;
		}

		if (auction_info.high_bidder == ch
		||  auction_info.owner == ch)
		{
		send_to_char("You still have a stake in the auction!\n\r",ch);
		return;
		}

		if (ch->position < POS_STUNNED)
		{
		send_to_char("You're not DEAD yet.\n\r", ch);
		return;
		}

		if (IS_SET(ch->in_room->room_flag[1], ROOM_NO_QUIT))
		{
		send_to_char("You can't quit here.\n\r", ch);
		return;
		}

		/* If person is putting together a package give them back items etc */
		if (ch->mail != NULL)
		{
		OBJ_DATA *obj;
		OBJ_DATA *obj_next;

		for (obj = ch->mail->objects; obj != NULL; obj = obj_next)
		{
		obj_next = obj->next_content;

		obj_from_mail(obj);
		obj_to_char(obj, ch);
		act("You take $p from your package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n takes $p from $s package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		}

		send_to_char("You discard your mail package.\n\r", ch);
		free_mail(ch->mail);
		ch->mail = NULL;
		}
/* WHY???? AO 010417
		if (ch->quest != NULL)
		{
		send_to_char("You can't quit, you're still on a quest!\n\r", ch);
		return;
		}
*/
		if(argument) {
			p_percent_trigger( ch, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUIT, NULL,0,0,0,0,0);
			// This requested input, pause and wait for input before quiting)
			if(ch->desc && ch->desc->input) {
				ch->pcdata->quit_on_input = true;
				return;
			}
		}
	}

	/* Make sure to unshift them first. Leave ch->shifted ON so we know to re-shift them
	   back on login.*/
	if (IS_SHIFTED(ch)) {
	shift_char(ch, true);
	ch->shifted = IS_VAMPIRE(ch) ? SHIFTED_WEREWOLF : SHIFTED_SLAYER;
	}

	if (ch->pulled_cart != NULL)
	do_function(ch, &do_drop, ch->pulled_cart->name);

	if (ch->ambush != NULL)
	send_to_char("You stop your ambush.\n\r", ch);

	/* remove any PURGE_QUIT tokens on the character */
	for (token = ch->tokens; token != NULL; token = token_next) {
	token_next = token->next;

	if (IS_SET(token->flags, TOKEN_PURGE_QUIT)) {
		p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL,0,0,0,0,0);

		sprintf(buf, "char update: token %s(%ld) char %s(%ld) was purged on quit",
		token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
		log_string(buf);
		token_from_char(token);
		free_token(token);
	}
	}


	send_to_char(
	"Alas, all good things must come to an end.\n\r", ch);
	send_to_char(
	"{MThanks for playing {WS E N T I E N C E{M.\n\r", ch);
	send_to_char(
	"{MWe hope you enjoyed your stay and see you again soon!{x\n\r", ch);

	act("$$n has left the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	sprintf(log_buf, "%s has quit.", ch->name);

	log_string(log_buf);
	wiznet("$N rejoins the real world.",
	ch, NULL, WIZ_LOGINS, 0, get_trust(ch));

	/* save wearing info */
	save_last_wear(ch);

	/* Remove hitpoint type affects */
	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
	if (obj->wear_loc != WEAR_NONE)
	{
	for (paf = obj->affected; paf != NULL; paf = paf->next)
	affect_modify(ch, paf, false);
	}
	}

	/* Reset imms bank accounts */
	if (IS_IMMORTAL(ch)
	&&  ch->tot_level < MAX_LEVEL)
	{
	ch->gold = 0;
	ch->silver = 0;
	ch->pcdata->bankbalance = 0;
	}

	// Reset manastore to zero - even on imms
	ch->manastore = 0;

	save_char_obj(ch);

	if (MOUNTED(ch))
	{
	CHAR_DATA *mount;

	die_follower(ch);

	mount = MOUNTED(ch);

	ch->mount = NULL;
	ch->riding = false;

	mount->rider = NULL;
	mount->riding = false;

	if (!str_cmp(mount->pIndexData->owner, ch->name))
	{
	act("$n wanders on home.", mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	if (mount->home_room != NULL)
	{
	char_from_room(mount);
	char_to_room(mount, mount->home_room);
	}
	else
	extract_char(mount, true);
	}
	}

	ITERATOR mit;
	MISSION_DATA *mission;
	iterator_start(&mit, ch->missions);
	while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
	{
	    for (MISSION_PART_DATA *part = mission->parts; part != NULL; part = part->next) {
			if (part->pObj != NULL)
			    extract_obj(part->pObj);
	    }
	}
	iterator_stop(&mit);

	id[0] = ch->id[0];
	id[1] = ch->id[1];

	d = ch->desc;

	connection_remove(d);

	extract_char(ch, true);

	if (d != NULL)
	close_socket(d);

	/* toast evil cheating bastards (?) */
	for (d = descriptor_list; d != NULL; d = d_next)
	{
	CHAR_DATA *tch;

	d_next = d->next;
	tch = d->original ? d->original : d->character;
	if (tch && tch->id[0] == id[0] && tch->id[1] == id[1])
	{
	extract_char(tch,true);
	close_socket(d);
	}
	}
}


void do_save(CHAR_DATA *ch, char *argument)
{
	if (IS_SWITCHED(ch))
	{
	send_to_char("You can't save in morphed form.\n\r", ch);
	return;
	}

	if (IS_NPC(ch))
	return;

	save_char_obj(ch);
	send_to_char("Saving.\n\r", ch);

	if (!IS_IMMORTAL(ch))
	WAIT_STATE(ch, PULSE_VIOLENCE);
}


void do_follow(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	CHAR_DATA *rch;

	one_argument(argument, arg);

	if (is_dead(ch))
	return;

	if (arg[0] == '\0')
	{
	send_to_char("Follow whom?\n\r", ch);
	return;
	}

	if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}

	if (IS_DEAD(victim))
	{
	send_to_char("You can't follow a shadow.\n\r", ch);
	return;
	}

	if (IS_AFFECTED(ch, AFF_CHARM) && ch->master != NULL)
	{
	act("But you'd rather follow $N!", ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (victim == ch)
	{
	if (ch->master == NULL)
	{
	send_to_char("You already follow yourself.\n\r", ch);
	return;
	}
	stop_follower(ch,true);
	return;
	}

	if (!IS_NPC(victim) && IS_SET(victim->act[0],PLR_NOFOLLOW) && !IS_IMMORTAL(ch))
	{
	act("$N doesn't seem to want any followers.\n\r", ch,victim,NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
	{
	if (rch->master == ch)
	{
	send_to_char("You can't follow someone else while you have followers of your own.\n\r", ch);
	return;
	}
	}

	REMOVE_BIT(ch->act[0],PLR_NOFOLLOW);

	if (ch->master != NULL)
	stop_follower(ch,true);

	add_follower(ch, victim,true);
}


void add_follower(CHAR_DATA *ch, CHAR_DATA *master, bool show)
{
	if (!IS_VALID(ch))
	{
	bug("Add_follower: invalid ch.", 0);
	return;
	}

	if (ch->master != NULL)
	{
	bug("Add_follower: non-null master.", 0);
	return;
	}

	ch->master = master;
	stop_grouped(ch);

	if (can_see(master, ch)) {
	if(show) act("$n now follows you.", ch, master, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	}

	if(show) act("You now follow $N.",  ch, master, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
}

void stop_follower(CHAR_DATA *ch, bool show)
	{
	if (!IS_VALID(ch))
	{
	bug("Stop_follower: invalid ch.", 0);
	return;
	}

	if (ch->master == NULL)
	{
	bug("Stop_follower: null master.", 0);
	return;
	}

	if (IS_AFFECTED(ch, AFF_CHARM))
	{
	REMOVE_BIT(ch->affected_by[0], AFF_CHARM);
	affect_strip(ch, gsk_charm_person);
	}

	if (can_see(ch->master, ch) && ch->in_room) {
	if(show) {
	act("$n stops following you.",     ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
	act("You stop following $N.",      ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
	}
	}

	if (IS_VALID(ch->master->pet))
	ch->master->pet = NULL;

	ch->master = NULL;
	stop_grouped(ch);
}


/*  Nukes charmed monsters and pets */
void nuke_pets(CHAR_DATA *ch)
{
	CHAR_DATA *pet;

	if ((pet = ch->pet) != NULL)
	{
	stop_follower(pet,true);

	if (pet->in_room != NULL)
	act("$N slowly fades away.",ch,pet, NULL,NULL, NULL, NULL, NULL,TO_NOTVICT);

	extract_char(pet,true);
	}

	ch->pet = NULL;

	if (ch->mount)
	{
	if (ch->mount != NULL)
	{
	ch->mount->rider = NULL;
	ch->mount->riding = false;
	}
	}

	ch->mount = NULL;
	ch->riding = false;
}


void die_follower(CHAR_DATA *ch)
{
	CHAR_DATA *fch;
	ITERATOR it;

	if (ch->master != NULL)
	{
		ch->master->pet = NULL;
		stop_follower(ch,true);
	}

	stop_grouped(ch);

	iterator_start(&it, loaded_chars);
	while(( fch = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (fch->master == ch)
			stop_follower(fch,true);

		if (fch->leader == ch)
			add_grouped(fch, fch, true);
	}
	iterator_stop(&it);
}


void do_order(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;

	argument = one_argument(argument, arg );
	argument = one_argument(argument, arg2);

	if (!str_cmp(arg2,"delete") || !str_cmp(arg2,"mob"))
	{
	send_to_char("That will NOT be done.\n\r",ch);
	return;
	}

	if (arg[0] == '\0' || argument[0] == '\0')
	{
	send_to_char("Order whom to do what?\n\r", ch);
	return;
	}

	if (IS_AFFECTED(ch, AFF_CHARM))
	{
	send_to_char("You feel like taking, not giving, orders.\n\r", ch);
	return;
	}

	if (!strcmp(arg, "mount"))
	{
	if (!ch->mount)
	{
	send_to_char("Your don't have a mount.\n\r", ch);
	return;
	}

	if (ch->mount->in_room != ch->in_room)
	{
	send_to_char("Your mount isn't here!\n\r", ch);
	return;
	}
	else
	victim = ch->mount;
	}
	else if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}

	if (victim == ch)
	{
	send_to_char("Just do it.\n\r", ch);
	return;
	}

	if (!IS_AFFECTED(victim, AFF_CHARM) || victim->master != ch
	||  (IS_IMMORTAL(victim) && victim->trust >= ch->trust))
	{
	act("$N has no interest in taking orders from you.",
	ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if ( !str_prefix(arg2, "north") ||
	!str_prefix(arg2, "south") ||
	!str_prefix(arg2, "west") ||
	!str_prefix(arg2, "east") ||
	!str_prefix(arg2, "stand") ||
	!str_prefix(arg2, "eat") ||
	!str_prefix(arg2, "drink") ||
	!str_prefix(arg2, "sit") ||
	!str_prefix(arg2, "rest") ||
	!str_prefix(arg2, "sleep") ||
	!str_prefix(arg2, "open") ||
	!str_prefix(arg2, "close"))
	{
	sprintf(buf,"%s orders you to \'%s\'.", ch->name, argument);
	send_to_char(buf, victim);
	interpret(victim, argument);
	return;
	}
	else
	{
	act("$N ignores your orders.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}
}


void do_ungroup(CHAR_DATA *ch, char *argument)
{
	send_to_char("Ungrouping.\n\r", ch);
	die_follower(ch);
}


void do_group(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	ITERATOR it;

	one_argument(argument, arg);

	/* Show group status */
	if (arg[0] == '\0') {
		CHAR_DATA *gch;
		CHAR_DATA *leader;

		leader = (ch->leader != NULL) ? ch->leader : ch;
		sprintf(buf, "{Y%s's group is currently formed by:\n\r", pers(leader, ch));
		send_to_char(buf, ch);

		iterator_start(&it, loaded_chars);
		while(( gch = (CHAR_DATA *)iterator_nextdata(&it)))
		{
			if (is_same_group(gch, ch) || gch == ch) {
				char name[MSL];

				// TODO: show relative location
				sprintf(name, "%s", pers(gch, ch)) ;
				name[0] = UPPER(name[0]);
				sprintf(buf,
					"{B[{G%3d {Y%-6.6s{B] {G%-15.15s {w%6ld{B/{w%ld {Bhp {w%6ld{B/{w%ld {Bmana {w%6ld{B/{w%ld {Bmv{x\n\r",
					gch->level,
					IS_NPC(gch) ? " NPC  " : (IS_VALID(gch->race) ? gch->race->who : "      "),
					name,
					gch->hit,   gch->max_hit,
					gch->mana,  gch->max_mana,
					gch->move,  gch->max_move);
				send_to_char(buf, ch);
			}
		}
		iterator_stop(&it);
		return;
	}

	/* Put someone else into your group (or remove them)*/
	if ((victim = get_char_room(ch, NULL, arg)) == NULL) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (ch->master != NULL || (ch->leader != NULL && ch->leader != ch)) {
		send_to_char("But you are following someone else!\n\r", ch);
		return;
	}

	if (victim->master != ch && ch != victim) {
		act_new("$N isn't following you.",ch,victim,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
		return;
	}

	if (IS_AFFECTED(victim,AFF_CHARM)) {
		send_to_char("You can't remove charmed mobs from your group.\n\r",ch);
		return;
	}

	if (IS_AFFECTED(ch,AFF_CHARM)) {
		act_new("You like your master too much to leave $m!", ch,victim,NULL,NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
		return;
	}

	if (is_same_group(victim, ch) && ch != victim) {
		stop_grouped(victim);
		act_new("$n removes $N from $s group.", ch,victim,NULL,NULL,NULL,NULL,NULL,TO_NOTVICT,POS_RESTING,NULL);
		act_new("$n removes you from $s group.", ch,victim,NULL,NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
		act_new("You remove $N from your group.", ch,victim,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
		return;
	}

	if (ch == victim) {
		send_to_char("That would be pointless.\n\r", ch);
		return;
	}

	if (!add_grouped(victim, ch,true))
		return;

	act_new("$N joins $n's group.",ch,victim,NULL,NULL,NULL,NULL,NULL,TO_NOTVICT,POS_RESTING,NULL);
	act_new("You join $n's group.",ch,victim,NULL,NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
	act_new("$N joins your group.",ch,victim,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
}


void do_split(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg1[MAX_INPUT_LENGTH],arg2[MAX_INPUT_LENGTH];
	CHAR_DATA *gch;
	int members;
	long amount_gold = 0, amount_silver = 0;
	long share_gold, share_silver;
	long extra_gold, extra_silver;

	argument = one_argument(argument, arg1);
	one_argument(argument, arg2);

	if (arg1[0] == '\0')
	{
	send_to_char("Split how much?\n\r", ch);
	return;
	}

	amount_silver = atol(arg1);

	if (arg2[0] != '\0')
	amount_gold = atol(arg2);

	if (amount_gold < 0 || amount_silver < 0)
	{
	send_to_char("Your group wouldn't like that.\n\r", ch);
	return;
	}

	if (amount_gold == 0 && amount_silver == 0)
	{
	send_to_char("You hand out zero coins, but no one notices.\n\r", ch);
	return;
	}

	if (ch->gold <  amount_gold || ch->silver < amount_silver)
	{
	send_to_char("You don't have that much to split.\n\r", ch);
	return;
	}

	members = 0;
	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if (is_same_group(gch, ch) && !IS_AFFECTED(gch,AFF_CHARM))
	members++;
	}

	if (members < 2)
	{
	send_to_char("Just keep it all.\n\r", ch);
	return;
	}

	share_silver = amount_silver / members;
	extra_silver = amount_silver % members;

	share_gold   = amount_gold / members;
	extra_gold   = amount_gold % members;

	if (share_gold == 0 && share_silver == 0)
	{
	send_to_char("Don't even bother, cheapskate.\n\r", ch);
	return;
	}

	ch->silver	-= amount_silver;
	ch->silver	+= share_silver + extra_silver;
	ch->gold 	-= amount_gold;
	ch->gold 	+= share_gold + extra_gold;

	if (share_silver > 0)
	{
	sprintf(buf,
	"You split %ld silver coins. Your share is %ld silver.\n\r",
	amount_silver,share_silver + extra_silver);
	send_to_char(buf,ch);
	}

	if (share_gold > 0)
	{
	sprintf(buf,
	"You split %ld gold coins. Your share is %ld gold.\n\r",
	amount_gold,share_gold + extra_gold);
	send_to_char(buf,ch);
	}

	if (share_gold == 0)
	{
	sprintf(buf,"$n splits %ld silver coins. Your share is %ld silver.",
	amount_silver,share_silver);
	}
	else if (share_silver == 0)
	{
	sprintf(buf,"$n splits %ld gold coins. Your share is %ld gold.",
	amount_gold,share_gold);
	}
	else
	{
	sprintf(buf,
	"$n splits %ld silver and %ld gold coins, giving you %ld silver and %ld gold.\n\r",
	amount_silver,amount_gold,share_silver,share_gold);
	}

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if (gch != ch && is_same_group(gch,ch) && gch->pcdata != NULL)
	{
	act(buf, ch, gch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	gch->gold += share_gold;
	gch->silver += share_silver;
	}
	}
}


/*    Group chat */
void do_gtell(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *gch;
	bool another_person = false;
	ITERATOR it;

	if (argument[0] == '\0') {
		send_to_char("Tell your group what?\n\r", ch);
		return;
	}

	if (IS_SET(ch->comm, COMM_NOTELL)) {
		send_to_char("Your message didn't get through!\n\r", ch);
		return;
	}

	iterator_start(&it, loaded_chars);
	while(( gch = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (is_same_group(gch, ch)) {
			act_new("{C$$n tells the group '$t'{x", ch,gch,NULL,NULL,NULL,argument,NULL,TO_VICT,POS_SLEEPING,NULL);
			if (gch != ch)
				another_person = true;
		}
	}
	iterator_stop(&it);

	if (another_person) {
		send_to_char("{CYou tell your group '", ch);
		send_to_char(argument, ch);
		send_to_char("'{x\n\r", ch);
	} else
		send_to_char("There are no members in your group.\n\r", ch);

	return;
}


bool is_same_group(CHAR_DATA *ach, CHAR_DATA *bch)
{
	if (ach == NULL || bch == NULL)
	return false;

	if (ach == bch)
	return true;

	/* Mount fix */
	if (ach == MOUNTED(bch) || bch == MOUNTED(ach))
	return true;

	if (ach->leader != NULL)
	ach = ach->leader;

	if (bch->leader != NULL)
	bch = bch->leader;

	return ach == bch;
}


void do_colour(CHAR_DATA *ch, char *argument)
{
	char arg[ MAX_STRING_LENGTH ];

	argument = one_argument(argument, arg);

	if (!*arg)
	{
	if(!IS_SET(ch->act[0], PLR_COLOUR))
	{
	SET_BIT(ch->act[0], PLR_COLOUR);
	send_to_char("{bC{ro{yl{co{gr{x is now {rON!{x\n\r", ch);
	}
	else
	{
	send_to_char_bw("Colour is now OFF.\n\r", ch);
	REMOVE_BIT(ch->act[0], PLR_COLOUR);
	}

	return;
	}
	else
	{
	send_to_char_bw("Colour Configuration is unavailable in this\n\r", ch);
	send_to_char_bw("version of colour, sorry\n\r", ch);
	}
}

bool add_grouped(CHAR_DATA *ch, CHAR_DATA *master, bool show)
{
	if (master->num_grouped >= 9)
	{
	if(show) {
	send_to_char("You may only have 9 people in your group.\n\r", master);
	act("$N's group is currently full.", ch, master, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	return false;
	}

	if (ch != master) {
		master->num_grouped++;
		if( !list_hasdata(master->lgroup, ch))
			list_appendlink(master->lgroup, ch);
	}

	ch->leader = master;

	p_percent_trigger( ch, NULL, NULL, NULL, master, NULL, NULL, NULL, NULL, TRIG_GROUPED, NULL,0,0,0,0,0);

	return true;
}


void stop_grouped(CHAR_DATA *ch)
{
	CHAR_DATA *leader;
	if (!IS_VALID(ch))
	{
	bug("stop_grouped: invalid ch.", 0);
	return;
	}

	if (ch->leader != ch)
		if (ch->leader != NULL) {
			ch->leader->num_grouped--;
			if( list_hasdata(ch->leader->lgroup, ch))
				list_remlink(ch->leader->lgroup, ch, true);
		}

	leader = ch->leader;

	ch->leader = NULL;
	
	if (!IS_NPC(ch))
	{
		ch->pcdata->last_ready_check = 0;
		ch->pcdata->readycheck_answer = false;
	}

	p_percent_trigger( ch, NULL, NULL, NULL, leader, NULL, NULL, NULL, NULL, TRIG_UNGROUPED, NULL,0,0,0,0,0);

}



/*    Town crier announcement.*/
void crier_announce(char *argument)
{
	char buf[MAX_STRING_LENGTH];
	DESCRIPTOR_DATA *d;

	sprintf(buf, "{MThe Town Crier announces '%s{M'{x\n\r", argument);
	for (d = descriptor_list; d != NULL; d = d->next)
	{
	CHAR_DATA *victim;

	victim = d->original ? d->original : d->character;

	if (d->connected == CON_PLAYING
	&&  !IS_SET(victim->comm,COMM_NOANNOUNCE)
	&&  !IS_SET(victim->comm,COMM_QUIET))
	send_to_char(buf, victim);
	}
}


/*  Toggle crier announcements */
void do_announcements(CHAR_DATA *ch, char *argument)
{
	if (IS_SET(ch->comm,COMM_NOANNOUNCE))
	{
	send_to_char("Announcements are now ON.\n\r",ch);
	REMOVE_BIT(ch->comm,COMM_NOANNOUNCE);
	}
	else
	{
	send_to_char("Announcements are now OFF.\n\r",ch);
	SET_BIT(ch->comm,COMM_NOANNOUNCE);
	}
}


/*  Award double exp in name of a victim. */
void double_xp(CHAR_DATA *victim)
{
	struct tm *exp_time;
	char buf[MAX_STRING_LENGTH];

	sprintf(buf, "{gIn ceremony of %s, Sentience is blessed with 30 minutes of {GD{YO{GU{YB{GL{YE {GE{YX{GP{YE{GR{YI{GE{YN{GC{YE{G!{x\n\r", victim->name);
	gecho(buf);

	if (boost_table[BOOST_EXPERIENCE].timer == 0)
	exp_time = localtime(&current_time);
	else
	exp_time = localtime(&boost_table[BOOST_EXPERIENCE].timer);
	exp_time->tm_min += 30;
	boost_table[BOOST_EXPERIENCE].timer = mktime(exp_time);
	boost_table[BOOST_EXPERIENCE].boost = 200;
}


void do_ignore(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	char buf[MAX_STRING_LENGTH];
	IGNORE_DATA *ignore;
	IGNORE_DATA *ignore_prev;
	CHAR_DATA *victim;
	FILE *fp;
	char player_name[MAX_STRING_LENGTH];
	bool found_char;
	bool remove = false;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
	int i = 0;

	sprintf(buf, "{Y #   %-12s %s{x\n\r",
	"Person", "Reason");
	send_to_char(buf, ch);

	line(ch, 35);
	for (ignore = ch->pcdata->ignoring; ignore != NULL;
	ignore = ignore->next)
	{
	i++;
	sprintf(buf, "{Y%2d): {x%-12s (%.50s)\n\r",
	i,
	capitalize(ignore->name),
	ignore->reason);
	send_to_char(buf, ch);
	}

	if (i == 0)
	send_to_char("No ignores found.\n\r", ch);

	line(ch, 35);

	return;
	}

	/* Remove one */
	ignore_prev = NULL;
	for (ignore = ch->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
	{
	if (!str_prefix(arg, ignore->name))
	{
	remove = true;
	break;
	}

	ignore_prev = ignore;
	}

	if (remove)
	{
	sprintf(buf,
	"You are no longer ignoring %s.\n\r", capitalize(ignore->name));
	send_to_char(buf, ch);
	if (ignore_prev == NULL)
	ch->pcdata->ignoring = ignore->next;
	else
	ignore_prev->next = ignore->next;

	free_ignore(ignore);
	return;
	}

	/* Add a new one */
	if (!str_cmp(arg, ch->name))
	{
	send_to_char("You try very hard to ignore yourself, "
	"but fail. Rats!\n\r", ch);
	return;
	}

	if (strlen(arg) > 12)
	arg[12] = '\0';

	if ((victim = get_char_world(ch, arg)) != NULL
	&& !IS_NPC(victim))
	{
	found_char = true;
	sprintf(arg, "%s", capitalize(victim->name));
	}
	else
	{
	sprintf(player_name, "%s%c/%s", PLAYER_DIR, tolower(arg[0]), capitalize(arg));
	if ((fp = fopen(player_name, "r")) == NULL)
	{
	found_char = false;
	}
	else
	{
	found_char = true;
	fclose (fp);
	}
	}

	if (!found_char)
	{
	send_to_char("That player doesn't exist.\n\r", ch);
	return;
	}

	ignore = new_ignore();

	ignore->name = str_dup(arg);

	if (argument[0] == '\0')
	ignore->reason = str_dup("None");
	else
	ignore->reason = str_dup(argument);

	ignore->next = ch->pcdata->ignoring;
	ch->pcdata->ignoring = ignore;

	sprintf(buf, "You are now ignoring %s.\n\r", capitalize(arg));
	send_to_char(buf, ch);
}


void do_notify(CHAR_DATA *ch, char *argument)
{
	if (IS_NPC(ch))
	return;

	if (IS_SET(ch->comm, COMM_NOTIFY))
	{
	send_to_char("You will no longer be notified of people entering the game.\n\r", ch);
	REMOVE_BIT(ch->comm, COMM_NOTIFY);
	}
	else
	{
	send_to_char("You will now be notified when people enter the game.\n\r", ch);
	SET_BIT(ch->comm, COMM_NOTIFY);
	}
}


/*  Toggle showing formation's HP percentage in combat.*/
void do_formstate(CHAR_DATA *ch, char *argument)
{
	if (IS_SET(ch->comm, COMM_SHOW_FORM_STATE))
	{
	REMOVE_BIT(ch->comm, COMM_SHOW_FORM_STATE);
	act("You will no longer see your formation's health in combat.",
	ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
	SET_BIT(ch->comm, COMM_SHOW_FORM_STATE);
	act("You will now see your formation's health in combat.",
	ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
}


/*  Echos to all rooms in an area with the given sector.*/
void sector_echo(AREA_DATA *area, char *message, int sector)
{
	DESCRIPTOR_DATA *d;

	if (area == NULL)
	return;

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->connected == CON_PLAYING &&
			d->character->in_room != NULL &&
			!room_is_clone(d->character->in_room) &&
			!IS_VALID(d->character->in_room->instance_section) &&
			d->character->in_room->area == area &&
			d->character->in_room->sector_type == sector)
		{
			if (IS_IMMORTAL(d->character))
				send_to_char("SECTOR ECHO> ", d->character);
			send_to_char(message, d->character);
			send_to_char("\n\r", d->character);
		}
	}
}

void area_echo(AREA_DATA *area, char *message)
{
	DESCRIPTOR_DATA *d;

	if (area == NULL)
	return;

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->connected == CON_PLAYING &&
			d->character->in_room != NULL &&
			!room_is_clone(d->character->in_room) &&
			!IS_VALID(d->character->in_room->instance_section) &&
			d->character->in_room->area == area)
		{
			if (IS_IMMORTAL(d->character))
				send_to_char("AREA ECHO> ", d->character);
			send_to_char(message, d->character);
			send_to_char("\n\r", d->character);
		}
	}
}


/*  Toggle or hide "<empty>" equipment slots.*/
void do_autoeq(CHAR_DATA *ch, char *argument)
{
	if (IS_SET(ch->act[0], PLR_AUTOEQ))
	{
	send_to_char("You will no longer see empty equipment spots.\n\r", ch);
	REMOVE_BIT(ch->act[0], PLR_AUTOEQ);
	return;
	}
	else
	{
	send_to_char("You will now see empty equipment spots.\n\r", ch);
	SET_BIT(ch->act[0], PLR_AUTOEQ);
	return;
	}
}


/*  Quiet list -- allows certain people to tell you while you have quiet on.*/
void do_qlist(CHAR_DATA *ch, char *argument)
{
	char arg[MSL];
	char buf[MSL];
	char player_name[MSL];
	bool found_char;
	FILE *fp;
	STRING_DATA *string;
	STRING_DATA *string_prev;
	bool found;
	int i;

	argument = one_argument(argument, arg);
	arg[0] = UPPER(arg[0]);
	if (strlen(arg) > 12)
	arg[12] = '\0';

	if (arg[0] == '\0' || !str_cmp(arg, "show"))
	{
	send_to_char("{YQuiet list:{x\n\r", ch);
	line(ch, 45);
	i = 0;
	for (string = ch->pcdata->quiet_people; string != NULL;
	string = string->next)
	{
	sprintf(buf, "{Y%2d):{x %s\n\r", i + 1, string->string);
	send_to_char(buf, ch);
	i++;
	}

	if (i == 0)
	send_to_char("Nobody.\n\r", ch);

	line(ch, 45);

	return;
	}

	/* take everyone off */
	if (!str_cmp(arg, "clear"))
	{
	STRING_DATA *string_next;

	for (string = ch->pcdata->quiet_people; string != NULL; string = string_next)
	{
	string_next = string->next;
	do_function(ch, &do_qlist, string->string);
	}

	send_to_char("Quiet list cleared.\n\r", ch);
	return;
	}

	if (!str_cmp(arg, ch->name))
	{
	send_to_char("That would be pointless.\n\r", ch);
	return;
	}

	found = false;
	string_prev = NULL;
	for (string = ch->pcdata->quiet_people; string != NULL;
	string = string->next)
	{
	if (!str_prefix(arg, string->string))
	{
	found = true;
	break;
	}

	string_prev = string;
	}

	if (found)
	{
	act("Removed $t from quiet list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR);
	if (string_prev != NULL)
	string_prev->next = string->next;
	else
	ch->pcdata->quiet_people = string->next;
	free_string_data(string);
	return;
	}
	else
	{
	CHAR_DATA *victim;

	i = 0;
	for (string = ch->pcdata->quiet_people; string != NULL;
	string = string->next)
	i++;

	if (i > 19)
	{
	send_to_char("Sorry, maximum is 20 people.\n\r", ch);
	return;
	}

	if ((victim = get_char_world(ch, arg)) != NULL
	&& !IS_NPC(victim))
	{
	found_char = true;
	sprintf(arg, "%s", capitalize(victim->name));
	}
	else
	{
	sprintf(player_name, "%s%c/%s", PLAYER_DIR, tolower(arg[0]), capitalize(arg));
	if ((fp = fopen(player_name, "r")) == NULL)
	{
	found_char = false;
	}
	else
	{
	found_char = true;
	fclose (fp);
	}
	}

	if (!found_char)
	{
	send_to_char("That player doesn't exist.\n\r", ch);
	return;
	}

	string = new_string_data();
	string->string = str_dup(arg);
	act("Added $t to your quiet list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR);
	string->next = ch->pcdata->quiet_people;
	ch->pcdata->quiet_people = string;
	}
}

void do_whisper(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;

	argument = one_argument(argument, arg);
	if (arg[0] == '\0' || argument[0] == '\0')
	{
	send_to_char("Syntax: whisper <person> <message>\n\r", ch);
	return;
	}

	if (IS_AFFECTED2(ch, AFF2_SILENCE))
	{
	send_to_char("You attempt to say something but fail!\n\r", ch);
	act("$n opens his mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't in the room.\n\r", ch);
	return;
	}

	if (ch == victim)
	{
	send_to_char("That would be pointless.\n\r", ch);
	return;
	}

	act("{CYou whisper to $N '$t'{x", ch, victim,NULL,NULL, NULL, argument, NULL, TO_CHAR);
	act("{C$n whispers something to $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("{C$n whispers to you '$t'{x", ch, victim,NULL,NULL, NULL, argument, NULL, TO_VICT);

	/* This should only do whisper trigger?
	    if (!IS_NPC(ch))
		p_act_trigger(argument, victim, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH,0,0,0,0,0); */

	if (!IS_NPC(ch))
	p_act_trigger(argument, victim, NULL, NULL, ch, NULL, NULL, NULL, NULL,TRIG_WHISPER,0,0,0,0,0);
}

/*  Catchup on notes, news and changes*/
void do_catchup(CHAR_DATA *ch, char *argument)
{
	do_function(ch, &do_note, "catchup");
	do_function(ch, &do_news, "catchup");
	do_function(ch, &do_changes, "catchup");
	send_to_char("Done.\n\r", ch);
}


/*  Set sense-danger range (sith) */
void do_danger(CHAR_DATA *ch, char *argument)
{
	int range;
	int max_range;
	char buf[MSL];

	if (get_skill(ch, gsk_sense_danger) == 0)
	{
	send_to_char("You can't sense danger from players.\n\r", ch);
	return;
	}

	max_range = URANGE(1, get_skill(ch, gsk_sense_danger) / 10 - 1, 9);

	if (!is_number(argument))
	{
	sprintf(buf, "Syntax: danger <0-%d>\n\r", max_range);
	send_to_char(buf, ch);
	return;
	}

	if ((range = atoi(argument)) < 0 || range > max_range)
	{
	sprintf(buf, "Syntax: danger <0-%d>\n\r", max_range);
	send_to_char(buf, ch);
	return;
	}

	sprintf(buf, "Set danger range to %d.\n\r", range);
	send_to_char(buf, ch);

	ch->pcdata->danger_range = range;
}


void do_toggle(CHAR_DATA *ch, char *argument)
{
	char arg[MSL];
	char buf[2*MSL];
	char status[MSL];
	bool found;
	int i;
	long *field;
	long vector;

	/* Show current settings */
	if (argument[0] == '\0')
	{
		sprintf(buf, "{Y%-15s %s{x\n\r", "Setting", "Status");
		send_to_char(buf, ch);

		line(ch, 22);

		for (i = 0; pc_set_table[i].name != NULL; i++)
		{
			if (ch->tot_level >= pc_set_table[i].min_level)
			{
				if (pc_set_table[i].vector != 0)
				{
					vector = pc_set_table[i].vector;
					field = &ch->act[0];
				}
				else if (pc_set_table[i].vector2 != 0)
				{
					vector = pc_set_table[i].vector2;
					field = &ch->act[1];
				}
				else if (pc_set_table[i].vector_comm != 0)
				{
					vector = pc_set_table[i].vector_comm;
					field = &ch->comm;
				}
				else
				{
					sprintf(buf, "do_toggle: no good vector/field for setting %s", pc_set_table[i].name);
					bug(buf, 0);
					return;
				}

				if (pc_set_table[i].inverted)
				{
					if (IS_SET(*field, vector))
						sprintf(status, "{DOFF{x");
					else
						sprintf(status, "{WON{x");
				}
				else
				{
					if (IS_SET(*field, vector))
						sprintf(status, "{WON{x");
					else
						sprintf(status, "{DOFF{x");
				}

				sprintf(buf, "%-15s %s\n\r", pc_set_table[i].name, status);
				send_to_char(buf, ch);
			}
		}

		p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TOGGLE_LIST, NULL, 0, 0, 0, 0, 0);
		return;
	}

	/* Toggle a setting */
	argument = one_argument(argument, arg);

	found = false;
	for (i = 0; pc_set_table[i].name != NULL; i++)
	{
		if (!str_prefix(arg, pc_set_table[i].name) &&
			ch->tot_level >= pc_set_table[i].min_level) {
			found = true;
			break;
		}
	}

	if (!found)
	{
		if (!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TOGGLE_CUSTOM, arg, 0, 0, 0, 0, 0))
			send_to_char("That is not a valid setting.\n\r", ch);
		return;
	}

	if (pc_set_table[i].vector != 0)
	{
		vector = pc_set_table[i].vector;
		field = &ch->act[0];
	}
	else if (pc_set_table[i].vector2 != 0)
	{
		vector = pc_set_table[i].vector2;
		field = &ch->act[1];
	}
	else if (pc_set_table[i].vector_comm != 0)
	{
		vector = pc_set_table[i].vector_comm;
		field = &ch->comm;
	}
	else
	{
		sprintf(buf, "do_toggle: no good vector/field for setting %s", pc_set_table[i].name);
		bug(buf, 0);
		return;
	}

	if (pc_set_table[i].inverted)
	{
		if (IS_SET(*field, vector))
		{
			REMOVE_BIT(*field, vector);
			act("$t is now {WON{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR);
		}
		else
		{
			SET_BIT(*field, vector);
			act("$t is now {DOFF{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR);
		}
	}
	else
	{
		if (IS_SET(*field, vector))
		{
			REMOVE_BIT(*field, vector);
			act("$t is now {DOFF{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR);
		}
		else
		{
			SET_BIT(*field, vector);
			act("$t is now {WON{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR);
		}
	}
}

void do_quote(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH], msg[2*MSL];
	DESCRIPTOR_DATA *d;

	if (argument[0] == '\0')
	{
	if (IS_SET(ch->comm,COMM_NOQUOTE))
	{
	send_to_char("Quote channel is now ON.\n\r",ch);
	REMOVE_BIT(ch->comm,COMM_NOQUOTE);
	}
	else
	{
	send_to_char("Quote channel is now OFF.\n\r",ch);
	SET_BIT(ch->comm,COMM_NOQUOTE);
	}
	}
	else  /* gossip message sent, turn gossip on if it isn't already */
	{
	if (IS_SET(ch->comm,COMM_QUIET))
	{
	send_to_char("You must turn off quiet mode first.\n\r",ch);
	return;
	}

	if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
	{
	send_to_char("You can't seem to gather enough energy to do it.\n\r",
	ch);
	return;
	}

	if (IS_SET(ch->comm,COMM_NOCHANNELS))
	{
	send_to_char("The gods have revoked your channel priviliges.\n\r", ch);
	return;

	}

	if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	argument = makedrunk(argument,ch);

	REMOVE_BIT(ch->comm,COMM_NOQUOTE);

	buf[0] = '\0';
	STRIP_COLOUR(argument, buf);
	if(!buf[0]) {
		send_to_char("Is that all you want to say?\n\r",ch);
		return;
	}

	if (!IS_NPC(ch) && ch->pcdata->flag != NULL && IS_SET(ch->pcdata->channel_flags, FLAG_QUOTE))
	sprintf(msg, "{XYou quote {D\"%s {W%s{D\"{x\n\r", ch->pcdata->flag, buf);
	else
	sprintf(msg, "{XYou quote {D\"{W%s{D\"{x\n\r", buf);

	send_to_char(msg, ch);

	for (d = descriptor_list; d != NULL; d = d->next)
	{
	CHAR_DATA *victim;

	victim = d->original ? d->original : d->character;

	if (d->connected == CON_PLAYING &&
	d->character != NULL &&
	d->character != ch &&
	!IS_SET(victim->comm,COMM_NOQUOTE) &&
	!IS_SET(victim->comm,COMM_QUIET) &&
	!is_ignoring(d->character, ch))
	{
	if (!IS_NPC(ch) && ch->pcdata->flag != NULL
	&& !IS_NPC(victim) && IS_SET(victim->pcdata->channel_flags, FLAG_QUOTE))
	sprintf(msg, "%s {W%s", ch->pcdata->flag, buf);
	else
	sprintf(msg, "%s", buf);
	act_new("{x$$n quotes {D\"{W$t{D\"{x", ch, d->character,NULL,NULL,NULL, msg,NULL, TO_VICT,POS_SLEEPING,NULL);
	}
	}
	}
}


/* Allows people to view and change their set e-mail. */
void do_email(CHAR_DATA *ch, char *argument)
{
	char arg[MSL];
	char buf[MSL];

	if (IS_NPC(ch)) {
	bug("do_email: NPC", 0);
	return;
	}

	argument = one_argument(argument, arg);
	if (arg[0] == '\0') {
	sprintf(buf, "Your e-mail address is currently: {W%s{x.\n\r", ch->pcdata->email);
	send_to_char(buf, ch);
	return;
	}

	if (strlen(arg) < 5 || str_infix("@", arg)) {
	send_to_char("Invalid e-mail address.\n\r", ch);
	return;
	}

	free_string(ch->pcdata->email);
	ch->pcdata->email = str_dup(arg);
	sprintf(buf, "Your e-mail address has been set to {W%s{x.\n\r", ch->pcdata->email);
	send_to_char(buf, ch);
}


void do_flag(CHAR_DATA *ch, char *argument)
{
	char arg[MSL];
	char buf[2*MSL];
	int value;
	/*char *c;
	 int colours; */

	argument = one_argument_norm(argument, arg);

	if (IS_NPC(ch)) {
	bug("do_flag: NPC", 0);
	return;
	}

	if (arg[0] == '\0') {
	send_to_char("Syntax: flag <new flag|none>\n\r"
	"        flag <gossip|ooc|yell|flame|quote|helper|tells|music|ct>\n\r", ch);
	return;
	}

	if ((value = flag_value(channel_flags, arg)) != NO_FLAG) {
	if (!IS_SET(ch->pcdata->channel_flags, value)) {
	SET_BIT(ch->pcdata->channel_flags, value);
	sprintf(buf, "You will now see player flags on the %s channel.\n\r", flag_name(channel_flags, value));
	} else {
	REMOVE_BIT(ch->pcdata->channel_flags, value);
	sprintf(buf, "You will no longer see player flags on the %s channel.\n\r", flag_name(channel_flags, value));
	}

	send_to_char(buf, ch);
	return;
	}

	if (!str_cmp(arg, "none")) {
	send_to_char("Flag removed.\n\r", ch);
	free_string(ch->pcdata->flag);
	ch->pcdata->flag = NULL;
	return;
	}

	if (strlen_no_colours(arg) > 10) {
	send_to_char("Flag length limit is 10 characters (not counting colour codes).\n\r", ch);
	return;
	}
	/*
	colours = 0;
	for (c = arg; *c != '\0'; c++) {
	if (*c == '{')
	colours++;
	}


	if (colours > 5) {
	send_to_char("You may only use 5 colour codes in your flag.\n\r", ch);
	return;
	}
	*/
	free_string(ch->pcdata->flag);
	ch->pcdata->flag = str_dup(arg);
	sprintf(buf, "You have changed your flag to \"%s{x\".\n\r", arg);
	send_to_char(buf, ch);
}

/*    NIB : 20070121 : Targeted speech to mobiles*/
void do_sayto(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	char buf[MSL];
	char buf2[MSL], msg[MSL];
	int i;
	char *second;
	CHAR_DATA *victim;
	bool break_line = true;

	if (!argument[0]) {
	send_to_char("Say to whom and what?\n\r", ch);
	return;
	}

	argument = one_argument(argument,arg);
	if (!(victim = get_char_room(ch, NULL, arg))) {
	send_to_char("They aren't in the room.\n\r", ch);
	return;
	}
	if (victim == ch) {
	send_to_char("Talking to yourself is a sure sign that you need help.\n\r", ch);
	return;
	}

	if (!argument[0]) {
	send_to_char("Say what?\n\r", ch);
	return;
	}

	if (IS_AFFECTED2(ch, AFF2_SILENCE)) {
	send_to_char("You attempt to say something but fail!\n\r", ch);
	act("$n opens his mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	argument = makedrunk(argument,ch);

	msg[0] = '\0';
	STRIP_COLOUR(argument, msg);

	buf[0] = '\0';
	for (i = 0; msg[i]; i++)
	if (msg[i] == '!' && msg[i+1] != ' ')
	break_line = false;

	if (break_line) {
	second = stptok(msg, buf, sizeof(buf), "!");
	while(*second == ' ')
	second++;

	if (*second) {
	sprintf(buf2, "{C'$t!{C' exclaims $n to $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL,buf,NULL, TO_NOTVICT);
	sprintf(buf2, "{C'$t!{C' exclaims $n to you. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL,buf, NULL, TO_VICT);
	sprintf(buf2, "{C'$t!{C' you exclaim to $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL,buf, NULL, TO_CHAR);
	return;
	}
	}

	for (i = 0; msg[i]; i++)
	if (msg[i] == '?' && msg[i+1] != ' ')
	break_line = false;

	if (break_line) {
	second = stptok(msg, buf, sizeof(buf), "?");
	while(*second == ' ')
	second++;

	if (*second) {
	sprintf(buf2, "{C'$t!{C' asks $n to $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_NOTVICT);
	sprintf(buf2, "{C'$t!{C' $n asks you. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_VICT);
	sprintf(buf2, "{C'$t!{C' you ask $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_CHAR);
	return;
	}
	}

	for (i = 0; msg[i] != '\0'; i++)
	if (msg[i] == '.' && msg[i+1] != ' ')
	break_line = false;

	if (break_line) {
	second = stptok(msg, buf, sizeof(buf), ".");
	while(*second == ' ')
	second++;

	if (*second) {
	sprintf(buf2, "{C'$t!{C' says $n to $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_NOTVICT);
	sprintf(buf2, "{C'$t!{C' says $n to you. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_VICT);
	sprintf(buf2, "{C'$t!{C' you say to $N. '");
	strcat(buf2, second);
	strcat(buf2, "'{x");
	act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_CHAR);
	return;
	}
	}

	i = strlen(msg)-1;
	if (msg[i] == '!') {
	if (number_percent() < 50) {
	act("{C'$t{C' exclaims $n to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C'$t{C' exclaims $n to you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{C'$t{C' you exclaim to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	} else {
	act("{C$n exclaims to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C$n exclaims to you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{CYou exclaim to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	}
	} else if (msg[i] == '?') {
	if (number_percent() < 50) {
	act("{C$n asks $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C$n asks you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{CYou ask $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	} else {
	act("{C'$t{C' asks $n to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C'$t{C' $n asks you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{C'$t{C' you ask $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	}
	} else {
	if (number_percent() < 50) {
	act("{C$n says to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C$n says to you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{CYou say to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	} else {
	act("{C'$t{C' says $n to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT);
	act("{C'$t{C' says $n to you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT);
	act("{C'$t{C' you say to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR);
	}
	}

	if ((!IS_NPC(ch) || IS_SWITCHED(ch)) &&
	(!IS_NPC(victim) || victim->position == victim->pIndexData->default_pos))
	p_act_trigger(msg, victim, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SAYTO,0,0,0,0,0);

}

/*  NIB : 20070121 : Targeted speech to objects*/
void do_intone(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	char msg[MSL];
	OBJ_DATA *obj;

	if (!argument[0]) {
	send_to_char("Intone to what and what?\n\r", ch);
	return;
	}

	argument = one_argument(argument,arg);
	if (!(obj = get_obj_here(ch, NULL, arg))) {
	act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);
	return;
	}

	if (!argument[0]) {
	send_to_char("Say what?\n\r", ch);
	return;
	}

	if (IS_AFFECTED2(ch, AFF2_SILENCE)) {
	send_to_char("You attempt to say something but fail!\n\r", ch);
	act("$n opens his mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	argument = makedrunk(argument,ch);

	msg[0] = '\0';
	STRIP_COLOUR(argument, msg);

	act("{C$n intones to $p '$t'{x", ch, NULL, NULL, obj, NULL, msg, NULL, TO_ROOM);
	act("{CYou intone to $p '$t'{x", ch, NULL, NULL, obj, NULL, msg, NULL, TO_CHAR);

	if ((!IS_NPC(ch) || IS_SWITCHED(ch)))
	p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL,TRIG_SAYTO,0,0,0,0,0);
}
