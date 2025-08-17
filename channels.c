/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "db.h"
#include "olc.h"
#include "tables.h"
#include "wilds.h"

/*
Legacy Format:
[] = optional

channels.dat:
Version version
Channel name~
End

channels/<name>.channel
Title title~
Description description~
[Reserved reserved~]
Command command~
[Alias alias~]
[MinPosition dead]
Flags flags
Colors color1~ color2~
BroadcastFmt format~
SpeakerFmt format~
TargetFmt format~
ViewerFmt format~
[CanJoin widevnum]
[CanSee widevnum]
[CanSpeak widevnum]
[CanRead widevnum]
[CanModerate widevnum]
[CanRemove widevnum]
[OnJoin widevnum]
[OnLeave widevnum]
[GetTitle widevnum]
[GetColors widevnum]
HistoryLen number
HistoryView number
PunishmentExpire number

#HISTORY
Speaker account~ character~
[Target account~ character~]
[Banner banner~]
Message message~
Timestamp number
#-HISTORY

#REPORT
Reporter account~ character~
Speaker account~ character~
Reason type
Summary summary~

#CONTEXT						// Same reading as HISTORY
Speaker account~ character~
[Target account~ character~]
[Banner banner~]
Message message~
Timestamp number
#-CONTEXT

#-REPORT

#PUNISHMENT
Moderator account~ character~
Speaker account~ character~
Punishment type
Reason reason~
Sentence number number
#-PUNISHMENT

End
*/


#define VERSION_CHAN_000		0x00000000
#define VERSION_CHAN_001		0x01000000

#define VERSION_CHAN			VERSION_CHAN_001

#define KEYSCR(literal, field, refuid, type) \
    if (!str_cmp(word, literal))   \
    {                              \
		WNUM_LOAD load = fread_widevnum(fp, refuid);	\
        field = get_script_index_auid(load.auid, load.vnum, type);	\
        fMatch = true;             \
        break;                     \
    }


CHANNEL_DATA *gchan_orgtalk;

struct reserved_channel_type
{
	char *name;
	CHANNEL_DATA **pgchan;
} reserved_channels[] = {
	{ "orgtalk",		&gchan_orgtalk	},
	{ NULL,				NULL			}
};

char *get_reserved_channel_name(CHANNEL_DATA **pgchan)
{
	if (pgchan == NULL) return NULL;

	for(int i = 0; reserved_channels[i].name != NULL; i++)
	{
		if (reserved_channels[i].pgchan == pgchan)
			return reserved_channels[i].name;
	}

	return NULL;
}

CHANNEL_DATA **get_reserver_channel_ptr(char *name)
{
	if (IS_NULLSTR(name)) return NULL;

	for(int i = 0; reserved_channels[i].name != NULL; i++)
	{
		if (!str_prefix(name, reserved_channels[i].name))
			return reserved_channels[i].pgchan;
	}

	return NULL;
}


CHANNEL_DATA *get_channel_data(const char *name)
{
	ITERATOR it;
	CHANNEL_DATA *ch;
	
	iterator_start(&it, channels_list);
	while((ch = (CHANNEL_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(name, ch->name))
			break;
	}
	iterator_stop(&it);

	return ch;
}


void delete_channel_data(void *ptr)
{
	free_channel_data((CHANNEL_DATA *)ptr);
}


LLIST *channels_list = NULL;
int channels_version = VERSION_CHAN_000;

CHANNEL_HISTORY_DATA *load_channel_history(FILE *fp, char *closer)
{
	CHANNEL_HISTORY_DATA *history = new_channel_history();

	char buf[MSL];
	char *word;
	bool fMatch;
	while(str_cmp((word = fread_word(fp)), closer))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
		case 'B':
			KEYS("Banner",history->banner,fread_string(fp));
			break;

		case 'M':
			KEYS("Message",history->message,fread_string(fp));
			break;

		case 'S':
			if (!str_cmp(word, "Speaker"))
			{
				free_string(history->speaker_account);
				history->speaker_account = fread_string(fp);

				free_string(history->speaker);
				history->speaker = fread_string(fp);

				fMatch = true;
				break;
			}
			break;

		case 'T':
			if (!str_cmp(word, "Target"))
			{
				free_string(history->target_account);
				history->target_account = fread_string(fp);

				free_string(history->target);
				history->target = fread_string(fp);

				fMatch = true;
				break;
			}
			KEY("Timestamp",history->timestamp,fread_number(fp));
			break;
		}

		if (!fMatch)
		{
			snprintf(buf, sizeof(buf), "load_channel_history(%s): no match for word %s", closer, word);
			bug(buf, 0);
		}
	}

	return history;
}

CHANNEL_REPORT_DATA *load_channel_report(FILE *fp)
{
	CHANNEL_REPORT_DATA *report = new_channel_report();

	char buf[MSL];
	char *word;
	bool fMatch;

	while(str_cmp((word = fread_word(fp)), "#-REPORT"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
		case '#':
			if (!str_cmp(word, "#CONTEXT"))
			{
				CHANNEL_HISTORY_DATA *context = load_channel_history(fp,"#-CONTEXT");
				if (IS_VALID(context))
				{
					list_appendlink(context, report->context);
				}

				fMatch = true;
				break;
			}
			break;

		case 'R':
			KEY("Reason", report->reason, stat_lookup(fread_word(fp),report_reasons,CHANNEL_REPORT_NONE));
			if (!str_cmp(word, "Reporter"))
			{
				free_string(report->reporter_account);
				report->reporter_account = fread_string(fp);

				free_string(report->reporter);
				report->reporter = fread_string(fp);

				fMatch = true;
				break;
			}
			break;

		case 'S':
			if (!str_cmp(word, "Speaker"))
			{
				free_string(report->speaker_account);
				report->speaker_account = fread_string(fp);

				free_string(report->speaker);
				report->speaker = fread_string(fp);

				fMatch = true;
				break;
			}
			KEYS("Summary", report->summary, fread_string(fp));
			break;
		}

		if (!fMatch)
		{
			snprintf(buf, sizeof(buf), "load_channel_report: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return report;
}

CHANNEL_PUNISHMENT_DATA *load_channel_punishment(FILE *fp)
{
	CHANNEL_PUNISHMENT_DATA *punishment = new_channel_punishment();

	char buf[MSL];
	char *word;
	bool fMatch;

	while(str_cmp((word = fread_word(fp)), "#-PUNISHMENT"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
		case 'M':
			if (!str_cmp(word, "Moderator"))
			{
				free_string(punishment->moderator_account);
				punishment->moderator_account = fread_string(fp);

				free_string(punishment->moderator);
				punishment->moderator = fread_string(fp);

				fMatch = true;
				break;
			}
			break;

		case 'R':
			KEY("Reason", punishment->reason, stat_lookup(fread_word(fp),punishment_reasons,PUNISHMENT_NONE));
			break;

		case 'S':
			KEYS("Summary", punishment->summary, fread_string(fp));
			break;

		case 'S':
			if (!str_cmp(word, "Sentence"))
			{
				punishment->start = fread_number(fp);
				punishment->end = fread_number(fp);

				fMatch = true;
				break;
			}
			if (!str_cmp(word, "Speaker"))
			{
				free_string(punishment->speaker_account);
				punishment->speaker_account = fread_string(fp);

				free_string(punishment->speaker);
				punishment->speaker = fread_string(fp);

				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch)
		{
			snprintf(buf, sizeof(buf), "load_channel_punishment: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return punishment;
}

CHANNEL_DATA *load_channel(char *name)
{

	char buf[MSL];
	char *word;
	bool fMatch;


	FILE *fp;
	char path[MIL];
	sprintf(path, "%s%s", CHANNEL_DIR, name);
	if((fp = fopen(path, "r")) == NULL) {
		bug("load_channel: fopen", 0);
		perror(path);
		return NULL;
	} else {
		CHANNEL_DATA *chan = new_channel_data();
		chan->name = name;
		
		while(str_cmp((word = fread_word(fp)), "End"))
		{
			fMatch = false;

			switch(UPPER(word[0]))
			{
			case '#':
				if (!str_cmp(word, "#HISTORY"))
				{
					CHANNEL_HISTORY_DATA *history = load_channel_history(fp,"#-HISTORY");
					if (IS_VALID(history))
					{
						list_appendlink(history, chan->history);
					}

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#REPORT"))
				{
					CHANNEL_REPORT_DATA *report = load_channel_report(fp);
					if (IS_VALID(report))
					{
						list_appendlink(report, chan->report);
					}

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#PUNISHMENT"))
				{
					CHANNEL_PUNISHMENT_DATA *punishment = load_channel_punishment(fp);
					if (IS_VALID(punishment))
					{
						list_appendlink(punishment, chan->punishment);
					}

					fMatch = true;
					break;
				}
				break;

			case 'A':
				if (!str_cmp(word, "Alias"))
				{
					chan->cmd_alias = get_cmd_data(fread_string(fp));
					fMatch = true;
					break;
				}
				break;

			case 'B':
				KEYS("BroadcastFmt", chan->broadcast_format, fread_string(fp));
				break;

			case 'C':
				KEYSCR("CanJoin",chan->can_join,0,PRG_MPROG);
				KEYSCR("CanModerate",chan->can_moderate,0,PRG_MPROG);
				KEYSCR("CanRead",chan->can_read,0,PRG_MPROG);
				KEYSCR("CanRemove",chan->can_remove,0,PRG_MPROG);
				KEYSCR("CanSee",chan->can_see,0,PRG_MPROG);
				KEYSCR("CanSpeak",chan->can_speak,0,PRG_MPROG);
				if (!str_cmp(word, "Colors"))
				{
					free_string(chan->color1);
					chan->color1 = fread_string(fp);
					free_string(chan->color2);
					chan->color2 = fread_string(fp);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "Command"))
				{
					chan->cmd_command = get_cmd_data(fread_string(fp));
					fMatch = true;
					break;
				}
				break;

			case 'D':
				KEYS("Description", chan->description, fread_string(fp));
				break;

			case 'F':
				KEY("Flags", chan->flags, fread_flag(fp));
				break;

			case 'G':
				KEYSCR("GetColors",chan->get_colors,0,PRG_MPROG);
				KEYSCR("GetTitle",chan->get_title,0,PRG_MPROG);
				break;

			case 'H':
				KEY("HistoryLen", chan->history_length, fread_number(fp));
				KEY("HistoryView", chan->history_viewable, fread_number(fp));
				break;

			case 'M':
				KEY("MinPosition", chan->min_position, stat_lookup(fread_word(fp),position_flags,POS_DEAD));
				break;

			case 'O':
				KEYSCR("OnJoin",chan->on_join,0,PRG_MPROG);
				KEYSCR("OnLeave",chan->on_leave,0,PRG_MPROG);
				break;

			case 'P':
				KEY("PunishmentExpire", chan->punishment_expiration, fread_number(fp));
				break;

			case 'R':
				if (!str_cmp(word, "Reserved"))
				{
					char *name = fread_string(fp);

					CHANNEL_DATA **pgchan = get_reserver_channel_ptr(name);

					chan->reserved = pgchan;

					fMatch = true;
					break;
				}
				break;

			case 'S':
				KEYS("SpeakerFmt", chan->speaker_format, fread_string(fp));
				break;

			case 'T':
				KEYS("TargetFmt", chan->target_format, fread_string(fp));
				KEYS("Title", chan->title, fread_string(fp));
				break;

			case 'V':
				KEYS("ViewerFmt", chan->viewer_format, fread_string(fp));
				break;
			}

			if (!fMatch)
			{
				snprintf(buf, sizeof(buf), "load_channel: no match for word %s", word);
				bug(buf, 0);
			}
		}

		return chan;
	}
}

// This must be called *after* areas and commands have been loaded
bool load_channels()
{
	FILE *fp;

	log_string("load_channels: creating channels_list");
	channels_list = list_createx(false, NULL, delete_channel_data);
	if (!IS_VALID(channels_list))
	{
		log_string("channels_list was not created.");
		return false;
	}

	log_string("load_channels: loading " CHANNELS_FILE);
	if ((fp = fopen(CHANNELS_FILE, "r")) == NULL)
	{
		log_string("load_channels: '" CHANNELS_FILE "' file not found.");
		return false;
	}

	char buf[MSL];
	char *word;
	bool fMatch;

	while(str_cmp((word = fread_word(fp)), "End"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
		case 'C':
			if(!str_cmp(word, "Channel"))
			{
				char *name = fread_string(fp);
				CHANNEL_DATA *chan = load_channel(name);
				if (IS_VALID(chan))
					list_appendlink(channels_list, chan);
				
				fMatch = true;
				break;
			}
			break;

		case 'V':
			KEY("Version", channels_version);
			break;
		}

		if (!fMatch)
		{
			snprintf(buf, sizeof(buf), "load_channels: no match for word %s", word);
			bug(buf, 0);
		}
	}
	fclose(fp);

	// Process the channels to update the reserved channels
	ITERATOR it;
	iterator_start(&it, channels_list);
	CHANNEL_DATA *channel;
	while((channel = (CHANNEL_DATA *)iterator_nextdata(&it)))
	{
		// 1) update the reserved channels
		if (channel->reserved != null)
			(*channel->reserved) = channel;

		// 2) Make sure all commands associated with channels are flagged as internal
		if (channel->cmd_command != NULL)
		{
			channel->cmd_command->internal = true;
			channel->cmd_command->function = _do_speak_on_channel;
			free_string(channel->cmd_command->context);
			channel->cmd_command->context = str_dup(channel->name);
		}
		
		if (channel->cmd_alias != NULL)
		{
			channel->cmd_alias->internal = true;
			channel->cmd_alias->function = _do_speak_on_channel;
			free_string(channel->cmd_alias->context);
			channel->cmd_alias->context = str_dup(channel->name);
		}

	}
	iterator_stop(&it);
	
	return true;
}

void save_channel_history(FILE *fp, CHANNEL_HISTORY_DATA *history, char *group)
{
	fprintf(fp, "#%s\n\r", group);
	fprintf(fp, "Speaker %s~ %s~\n\r", history->speaker_account, history->speaker);
	if (!IS_NULLSTR(history->target_account) && !IS_NULLSTR(history->target))
		fprintf(fp, "Target %s~ %s~\n\r", history->target_account, history->target);
	if (!IS_NULLSTR(history->banner))
		fprintf(fp, "Banner %s~\n\r", history->banner);
	fprintf(fp, "Message %s~\n\r", history->message);
	fprintf(fp, "Timestamp %ld\n\r", history->timestamp);
	fprintf(fp, "#-%s\n\r", group);
}

void save_channel_report(FILE *fp, CHANNEL_REPORT_DATA *report)
{
	fprintf(fp, "#REPORT\n\r");
	fprintf(fp, "Reporter %s~ %s~\n\r", report->reporter_account, report->reporter);
	fprintf(fp, "Speaker %s~ %s~\n\r", report->speaker_account, report->speaker);
	fprintf(fp, "Reason %s\n\r", flag_string(report_reasons,report->reason));
	fprintf(fp, "Summary %s~\n\r", report->summary);

	ITERATOR it;
	CHANNEL_HISTORY_DATA *context;
	iterator_start(&it, report->context);
	while((context = (CHANNEL_HISTORY_DATA *)iterator_nextdata(&it)))
	{
		save_channel_history(fp, context, "CONTEXT");
	}
	iterator_stop(&it);

	fprintf(fp, "#-REPORT\n\r");
}

void save_channel_punishment(FILE *fp, CHANNEL_PUNISHMENT_DATA *punishment)
{
	fprintf(fp, "#PUNISHMENT\n\r");
	fprintf(fp, "Moderator %s~ %s~\n\r", punishment->moderator_account, punishment->moderator);
	fprintf(fp, "Speaker %s~ %s~\n\r", punishment->speaker_account, punishment->speaker);
	fprintf(fp, "Reason %s\n\r", flag_string(punishment_reasons,punishment->reason));
	fprintf(fp, "Summary %s~\n\r", punishment->summary);
	fprintf(fp, "Sentence %ld %ld\n\r", punishment->start, punishment->end);
	fprintf(fp, "#-PUNISHMENT\n\r");
}

bool save_channel(CHANNEL_DATA *channel)
{
	ITERATOR it;
	CHANNEL_HISTORY_DATA *history;
	CHANNEL_REPORT_DATA *report;
	CHANNEL_PUNISHMENT_DATA *punishment;

	FILE *fp;
	char path[MIL];
	sprintf(path, "%s%s", CHANNELS_DIR, channel->name)
	if ((fp = fopen(path, "w")) == NULL) {
        bug("save_channel: fopen", 0);
        perror(path);
		return false;
	} else {
		fprintf(fp, "Title %s~\n\r", channel->title);
		fprintf(fp, "Description %s~\n\r", channel->description);
		if (channel->reserved)
			fprintf(fp, "Reserved %s~\n\r", get_reserved_channel_name(channel->reserved));

		if (channel->cmd_command)
			fprintf(fp, "Command %s~\n\r", channel->cmd_command->name);
		
		if (channel->cmd_alias)
			fprintf(fp, "Alias %s~\n\r", channel->cmd_alias->name);

		fprintf(fp, "MinPosition %s\n\r", flag_string(position_flags, channel->min_position));
		fprintf(fp, "Flags %s\n\r", print_flags(channel->flags));
		fprintf(fp, "Colors %s~ %s~\n\r", channel->color1, channel->color2);

		fprintf(fp, "BroadcastFmt %s~\n\r", channel->broadcast_format);
		fprintf(fp, "SpeakerFmt %s~\n\r", channel->speaker_format);
		fprintf(fp, "TargetFmt %s~\n\r", channel->target_format);
		fprintf(fp, "ViewerFmt %s~\n\r", channel->viewer_format);

		fprintf(fp, "CanJoin %s\n\r", widevnum_string_script(channel->can_join, NULL));
		fprintf(fp, "CanSee %s\n\r", widevnum_string_script(channel->can_see, NULL));
		fprintf(fp, "CanRead %s\n\r", widevnum_string_script(channel->can_read, NULL));
		fprintf(fp, "CanSpeak %s\n\r", widevnum_string_script(channel->can_speak, NULL));
		fprintf(fp, "CanModerate %s\n\r", widevnum_string_script(channel->can_moderate, NULL));
		fprintf(fp, "CanRemove %s\n\r", widevnum_string_script(channel->can_remove, NULL));
		fprintf(fp, "OnJoin %s\n\r", widevnum_string_script(channel->on_join, NULL));
		fprintf(fp, "OnLeave %s\n\r", widevnum_string_script(channel->on_leave, NULL));
		fprintf(fp, "GetTitle %s\n\r", widevnum_string_script(channel->get_title, NULL));
		fprintf(fp, "GetColors %s\n\r", widevnum_string_script(channel->get_colors, NULL));

		iterator_start(&it, channel->history);
		while((history = (CHANNEL_HISTORY_DATA *)iterator_nextdata(&it)))
		{
			save_channel_history(fp, history, "HISTORY");
		}
		iterator_stop(&it);

		iterator_start(&it, channel->reports);
		while((report = (CHANNEL_REPORT_DATA *)iterator_nextdata(&it)))
		{
			save_channel_report(fp, report);
		}
		iterator_stop(&it);

		iterator_start(&it, channel->punishments);
		while((punishment = (CHANNEL_PUNISHMENT_DATA *)iterator_nextdata(&it)))
		{
			save_channel_punishment(fp, punishment);
		}
		iterator_stop(&it);

		fprintf(fp, "End\n\r");
		fclose(fp);
		return true;
	}
}

void save_channels()
{
	FILE *fp;

    log_string("save_channels: saving " CHANNELS_FILE);
    if ((fp = fopen(CHANNELS_FILE, "w")) == NULL)
    {
        bug("save_channels: fopen", 0);
        perror(CHANNELS_FILE);
    }
    else
    {
        log_string(formatf("save_channels: Saving %ld channels", list_size(channels_list)));

		fprintf(fp, "Version %ld\n\r", CHANNEL_VERSION);

		ITERATOR it;
		CHANNEL_DATA *channel;

		iterator_start(&it, channels_list);
		while((channel = (CHANNEL_DATA *)iterator_nextdata(&it)))
		{
			if (save_channel(channel))
				fprintf(fp, "Channel %s~\n\r", channel->name);
		}
		iterator_stop(&it);

		fprintf(fp, "End\n\r");
		fclose(fp);
	}
}

PLAYER_CHANNEL_DATA *fread_player_channel_data(const char *name);
PLAYER_CHANNEL_DATA *get_player_channel_data(const char *name)
{
	// 1. See if the player is online
	CHAR_DATA *ch = get_player((char *)name);

	// 2a. Online, return loaded data
	if(IS_VALID(ch) && !IS_NPC(ch))
		return ch->pcdata->channels;

	// 2b. Offline, load data from file
	return fread_player_channel_data(name);
}

char *fread_char_account_name(const char *name);
char *get_player_account_name(const char *name)
{
	// 1. See if the player is online
	CHAR_DATA *ch = get_player((char *)name);

	// 2a. Online, return account name
	if(IS_VALID(ch) && !IS_NPC(ch))
		return ch->pcdata->account_name;

	// 2b. Offline, load account from file
	return fread_char_account_name(name);
}

// Channel System

CHANNEL_ENTRY *get_channel_entry(PLAYER_CHANNEL_DATA *data, CHANNEL_DATA *channel, bool create)
{
	if (!data) return NULL;

	ITERATOR it;
	CHANNEL_ENTRY *entry;
	iterator_start(&it, data->channels);
	while((entry = (CHANNEL_ENTRY *)iterator_nextdata(&it)))
	{
		if (entry->channel == channel)
			break;
	}
	iterator_stop(&it);

	if (!IS_VALID(entry) && create)
	{
		entry = new_channel_entry();
		entry->channel = channel;
		list_appendlink(data->channels, entry);
	}

	return entry;
}

CHANNEL_PUNISHMENT_DATA *get_channel_punishment(CHAR_DATA *ch, CHANNEL_DATA *channel, int type)
{
	if (IS_NPC(ch)) return NULL;

	ITERATOR it;
	CHANNEL_PUNISHMENT_DATA *punishment;

	iterator_start(&it, channel->punishments);
	while((punishment = (CHANNEL_PUNISHMENT_DATA *)iterator_nextdata(&it)))
	{
		if (str_cmp(punishment->speaker_account, ch->pcdata->account_name))
			continue;
		
		if (str_cmp(punishment->speaker, ch->name))
			continue;

		if (punishment->reason != type)
			continue;

		// Active punishment
		if (punishment->end < 0 || punishment->end > current_time)
			break;
	}
	iterator_stop(&it);

	return punishment;
}

bool channel_has_joined(CHAR_DATA *ch, CHANNEL_DATA *channel)
{
	CHANNEL_ENTRY *entry = get_channel_entry(ch->pcdata->channels, channel, false);

	if (!IS_VALID(entry)) return false;

	return IS_SET(entry->flags, CHANNEL_JOINED) && true;
}

bool channel_allows_replay(CHAR_DATA *ch, CHANNEL_DATA *channel)
{
	CHANNEL_ENTRY *entry = get_channel_entry(ch->pcdata->channels, channel, false);

	if (!IS_VALID(entry)) return false;

	return IS_SET(entry->flags, CHANNEL_ALLOW_REPLAY) && true;
}

// Assumes the player has not joined yet.
bool channel_can_join(CHAR_DATA *ch, CHANNEL_DATA *channel)
{
	if (IS_NPC(ch)) return false;

	if (channel->can_join)
	{
		// Allow/Deny
		int ret = execute_script(channel->can_join,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

		if (ret) return false;
	}

	return true;
}

bool channel_can_moderate(CHAR_DATA *ch, CHANNEL_DATA *channel)
{
	if (channel->can_moderate)
	{
		// If player moderated, only players can moderate the channel.
		// Staff has no involvement.
		if (IS_SET(channel->flags, CHANNEL_PLAYER_MODERATED) && IS_IMMORTAL(ch))
			return false;

		// Allow/Deny
		int ret = execute_script(channel->can_moderate,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

		if (ret) return false;
	}
	else if (!IS_IMMORTAL(ch))
		return false;

	return true;
}


bool channel_can_see_player(CHAR_DATA *ch, CHAR_DATA *vch, CHANNEL_DATA *channel)
{
	if (channel->can_see)
	{
		// Allow/Deny
		int ret = execute_script(channel->can_see,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			ch, vch, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

		if (ret) return false;
	}

	return true;
}

char *get_channel_title(CHAR_DATA *ch, CHANNEL_DATA *channel)
{
	free_string(ch->tempstring);
	ch->tempstring = str_dup(channel->title);

	if (channel->get_title)
	{
		execute_script(channel->get_title,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			ch, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
	}

	return str_dup(ch->tempstring);
}

static char *__get_punishment_duration(CHAR_DATA *ch, char *argument, time_t *duration)
{
	char arg[MIL];

	*duration = 0;
	argument = one_argument(argument, arg);
	if (arg[0] == '\0')
	{
		*duration = 86400;
	}
	else if (is_number(arg))
	{
		if (!str_prefix(argument, "days"))
		{
			int days = atoi(arg);
			if (days < 1 || days > 30)
			{
				send_to_char("Invalid number of days.  Please specify a number from 1 to 30.\n\r", ch);
			}
			else
				*duration = days * 86400;
		}
		else if (!str_prefix(argument, "hours"))
		{
			int hours = atoi(arg);
			if (hours < 1 || hours > 720)
			{
				send_to_char("Invalid number of hours.  Please specify a number from 1 to 720 (30 days).\n\r", ch);
			}
			else
				*duration = hours * 3600;
		}
		else
		{
			send_to_char("Please specify either {Gdays{x or {Ghours{x.\n\r", ch);
		}
	}
	else if (!str_prefix(arg, "indefinite"))
	{
		*duration = -1;
	}
	else
	{
		send_to_char("Invalid duration.\n\r", ch);
		send_to_char("Either specify a duration in {Ghours{x or {Gdays{x, or stipulate {Yindefinite{x.\n\r", ch);
	}

	return argument;
}

bool is_church_member(char *name, CHURCH_DATA *church);
static bool __enact_punishment(CHAR_DATA *ch, CHANNEL_DATA *channel, CHANNEL_ENTRY *entry, char *argument, int type)
{
	if (channel_can_moderate(ch, channel))
	{
		if (!entry || !IS_SET(entry->flags, CHANNEL_JOINED))
		{
			send_to_char("You have not a member of that channel.\n\r", ch);
			return true;
		}

		char speaker[MIL];
		argument = one_argument(argument, speaker);

		CHAR_DATA *sch = get_player(speaker);
		// Make sure to get the full name of the speaker if online
		if (IS_VALID(sch))
		{
			strncpy(speaker, sch->name, MIL-1);
		}

		PLAYER_CHANNEL_DATA *pcd = get_player_channel_data(speaker);
		if (!pcd)
		{
			send_to_char("No such player by that name.\n\r", ch);
			return true;
		}

		CHANNEL_ENTRY *ventry = get_channel_entry(pcd, channel, false);

		if (!IS_VALID(ventry))
		{
			send_to_char("{RPreemptive punishments are not allowed.{x\n\r", ch);
			return true;
		}

		// Org Talk channel has special checks to make sure the moderator and the speaker are actually in the same organization
		if (channel == gchan_orgtalk)
		{
			if (!ch->church)
			{
				send_to_char("{RYou are not in a church.{x\n\r", ch);
				return true;
			}

			if (!is_church_member(speaker, ch->church))

		}

		time_t duration;
		argument = __get_punishment_duration(ch, argument, &duration);
		if (!duration)
			return true;

		CHANNEL_PUNISHMENT_DATA *punishment = new_channel_punishment();
		punishment->speaker = str_dup(speaker);
		punishment->speaker_account = str_dup(fread_char_account_name(speaker));
		punishment->moderator = str_dup(ch->name);
		punishment->moderator_account = str_dup(ch->pcdata->account_name);
		punishment->reason = type;
		punishment->start = 0;
		punishment->end = duration;

		ch->desc->pendingChannel = channel;
		ch->desc->pendingPunishment = punishment;

		send_to_char("{WPlease specify a punishment summary:{x\n\r", ch);
		string_append_required(ch, &punishment->summary);
		send_to_char("{YClose with an empty string to cancel punishment.{x\n\r", ch);
		return true;
	}
	
	return false;
}

void do_channel(CHAR_DATA *ch, char *argument, const char *context)
{
	if (IS_NPC(ch)) return;

	// Syntax: channel
	if (argument[0] == '\0')
	{
		// List channels available to you
		// If not joined, must pass the can_join
		BUFFER *buffer = new_buf();

		add_buf(buffer, "Available Channels:\n\r");
		add_buf(buffer,"{Y    [    Name    ] [       Title       ] [ Status ]{x\n\r");
		add_buf(buffer,"{Y==================================================={x\n\r");

		int count = 0;
		ITERATOR it;
		CHANNEL_DATA *channel;
		iterator_start(&it, channels_list);
		while((channel = (CHANNEL_DATA *)iterator_nextdata(&it)))
		{
			CHANNEL_ENTRY *entry = get_channel_entry(ch->pcdata->channels, channel, false);
			bool joined = false;
			if (IS_VALID(entry))
			{
				// Currently on the channel
				if (IS_SET(entry->flags, CHANNEL_JOINED))
					joined = true;
				else if (get_channel_punishment(ch, channel, PUNISHMENT_BANNED) != NULL)
					continue;
			}

			if (!joined && !channel_can_join(ch, channel))
				continue;

			char *title = get_channel_title(ch, channel);
			add_buf(buffer, formatf("{W%-3d %-14.14s %-21.21s  %s{x\n\r", ++count,
				channel->name, title,
				(joined) ? "{g[{GJOINED{g]" : ""));
			free_string(title);
		}
		iterator_stop(&it);
		add_buf(buffer,"{Y==================================================={x\n\r");

		if (!count)
		{
			send_to_char("No channels available.\n\r", ch);
		}
		else if ( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
		{
			send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
		}
		else
		{
			page_to_char(buffer->string, ch);
		}

		free_buf(buffer);
		return;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	CHANNEL_DATA *channel = get_channel_data(arg);
	if (!channel)
	{
		send_to_char("No such channel exists by that name.\n\r", ch);
		return;
	}

	CHANNEL_ENTRY *entry = get_channel_entry(ch->pcdata->channels, channel, false);

	argument = one_argument(argument, arg);
	if (arg[0] != '\0')
	{
		// Syntax: channel <name> ban <player>[ <duration>]		(**)(S+)
		if (!str_prefix(arg, "ban"))
		{
			if (__enact_punishment(ch, channel, entry, argument, PUNISHMENT_BANNED))
				return;
		}

		// Syntax: channel <name> broadcast <message>			(**)
		else if (!str_prefix(arg, "broadcast"))
		{

		}

		// Syntax: channel <name> gag <player>[ <duration>]		(**)(S+)
		else if (!str_prefix(arg, "gag"))
		{
			if (__enact_punishment(ch, channel, entry, argument, PUNISHMENT_NO_REPORT))
				return;
		}

		// Syntax: channel <name> history
		else if (!str_prefix(arg, "history"))
		{

		}

		// Syntax: channel <name> info
		else if (!str_prefix(arg, "info"))
		{
			// If you can't join it, you can't get info about it
		}

		// Syntax: channel <name> join
		else if (!str_prefix(arg, "join"))
		{
			// Check if they already joined
			if (entry && IS_SET(entry->flags, CHANNEL_JOINED))
			{
				send_to_char("You have already joined that channel.\n\r", ch);
				return;
			}

			if (channel->can_join)
			{
				// Allow/Deny
				int ret = execute_script(channel->can_join,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

				if (ret)
				{
					// Default deny message
					if (ret == 1)
					{
						send_to_char("You have already joined that channel.\n\r", ch);
					}

					return;
				}
			}

			// Check if they are banned
			if (entry && (entry->banned < 0 || entry->banned > time(NULL))))
			{
				if (entry->banned < 0)
					send_to_char("You are not allowed to join that channel.\n\r", ch);
				else
					send_to_char(formatf("You are not allowed to join the channel until {W%s{x.\n\r", (char *)ctime((time_t *)&(entry->banned))), ch);
				return;
			}

			// Okay, you are free to join
			if (!entry)
			{
				entry = new_channel_entry(channel);
				list_appendlink(channel, ch->pcdata->channels);
			}

			SET_BIT(entry->flags, CHANNEL_JOINED);
			char *title = get_channel_title(ch, channel);
			send_to_char(formatf("You have joined {W%s{x.\n\r", title), ch);
			free_string(title);

			if (channel->on_join)
			{
				execute_script(channel->on_join,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
			}
		}

		// Syntax: channel <name> leave
		else if (!str_prefix(arg, "leave"))
		{
			// Check if they are a member
			if (!entry || !IS_SET(entry->flags, CHANNEL_JOINED))
			{
				send_to_char("You have not a member of that channel.\n\r", ch);
				return;
			}

			if (channel->on_leave)
			{
				execute_script(channel->on_leave,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					ch, NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
			}

			REMOVE_BIT(entry->flags, CHANNEL_JOINED);
			char *title = get_channel_title(ch, channel);
			send_to_char(formatf("You have left {W%s{x.\n\r", title), ch);
			free_string(title);
			return;
		}

		// Syntax: channel <name> mute <player>[ <duration>]	(**)(S+)
		else if (!str_prefix(arg, "mute"))
		{
			if (__enact_punishment(ch, channel, entry, argument, PUNISHMENT_SILENCED))
				return;
		}

		// Syntax: channel <name> punishments					(**)
		else if (!str_prefix(arg, "punishments"))
		{

		}

		// Syntax: channel <name> remove <player>				(**)
		else if (!str_prefix(arg, "remove"))
		{

		}

		// Syntax: channel <name> report <#> <reason>			(S+)
		else if (!str_prefix(arg, "report"))
		{

		}

		// Syntax: channel <name> reports						(**)
		else if (!str_prefix(arg, "reports"))
		{

		}

		// Syntax: channel <name> unban <player>				(**)
		else if (!str_prefix(arg, "unban"))
		{

		}

		// Syntax: channel <name> ungag <player>				(**)
		else if (!str_prefix(arg, "ungag"))
		{

		}

		// Syntax: channel <name> unmute <player>				(**)
		else if (!str_prefix(arg, "unmute"))
		{

		}

		// Syntax: channel <name> who
		else if (!str_prefix(arg, "who"))
		{
			if (!entry || !IS_SET(entry->flags, CHANNEL_JOINED))
			{
				send_to_char("You must join that channel first.\n\r", ch);
				return;
			}

			// Iterate over player list
			int count = 0;
			BUFFER *buffer = new_buf();

			char *title = get_channel_title(ch, channel);

			add_buf(buffer, formatf("{Y[{W%s{Y Members]{x\n\r", title));
			add_buf(buffer, "{Y=============================={x\n\r");

			for (DESCRIPTOR_DATA *d = descriptor_list; d != NULL; d = d->next)
			{
				CHAR_DATA *vch = (d->original != NULL) ? d->original : d->character;

				if (d->connected != CON_PLAYING || (IS_IMMORTAL(vch) && !can_see_imm(ch, vch))) {
					continue;

				if (channel_can_see_player(ch, vch, channel))
				{
					add_buf(buffer, formatf("{W%-3d{x %s\n\r", ++count, vch->name));
				}
			}

			add_buf(buffer, "{Y=============================={x\n\r");
			add_buf(buffer, formatf("{W%d{x player%s found.\n\r", count, ((count==1)?"":"s")));

			if (!count)
			{
				send_to_char(formatf("No players found on {W%s{x.\n\r", title));
			}
			else if ( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
			free_string(title);
			return;
		}
	}

	bool joined = entry && IS_SET(entry->flags, CHANNEL_JOINED);
	bool mod = joined && channel_can_moderate(ch, channel);

	send_to_char("Syntax: channel <name> info\n\r", ch);

	if (joined)
	{
		send_to_char("        channel <name> history\n\r", ch);
		send_to_char("        channel <name> leave\n\r", ch);
		send_to_char("        channel <name> report <#> <reason>\n\r", ch);
		send_to_char("        channel <name> who\n\r", ch);
		if (mod)
		{
			send_to_char("        channel <name> ban <player>[ <duration>]\n\r", ch);
			send_to_char("        channel <name> broadcast <message>\n\r", ch);
			send_to_char("        channel <name> gag <player>[ <duration>]\n\r", ch);
			send_to_char("        channel <name> mute <player>[ <duration>]\n\r", ch);
			send_to_char("        channel <name> punishments\n\r", ch);
			send_to_char("        channel <name> remove <player>\n\r", ch);
			send_to_char("        channel <name> reports\n\r", ch);
			send_to_char("        channel <name> unban <player>[ <duration>]\n\r", ch);
			send_to_char("        channel <name> ungag <player>[ <duration>]\n\r", ch);
			send_to_char("        channel <name> unmute <player>[ <duration>]\n\r", ch);
		}
	}
	else
	{
		send_to_char("        channel <name> join\n\r", ch);
	}

	// Legend:
	// (S+) = Opens string editor in required mode
	// (**) = Moderator commands
}


// The following must have been established prior to calling this function
// 1) player has joined the channel
// 2) player has passed the can_speak script hook.
// 3) player has not been silenced
void _do_speak_on_channel(CHAR_DATA *ch, char *argument, const char *context)
{
	if (IS_NPC(ch)) return;

	if (IS_NULLSTR(context))
	{
		send_to_char("Channel command was malformed.  Please alert staff.\n\r", ch);
		return;
	}

	CHANNEL_DATA *channel = get_channel_data(context);


}

