/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/* Event system is based on code courtesy of Kyndig from MudMagic */
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include "merc.h"
#include "recycle.h"
#include "scripts.h"

int call_depth;
EVENT_DATA *next_event = NULL;

EVENT_DATA *create_event(int delay, int type)
{
	EVENT_DATA *ev;
	ev = new_event();

	if(type < EVENT_MINTYPE || type > EVENT_MAXTYPE)
		return NULL;

	ev->event_type = type;
	ev->delay = delay;
	return ev;
}

void add_event(EVENT_DATA *ev)
{
	if( events_tail )
		events_tail->next = ev;
	else
		events = ev;
	events_tail = ev;
}


void wait_function(void *entity, SCRIPT_VARINFO *info, int event_type, int delay, void *function, char *argument)
{
	EVENT_DATA *ev;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;

	ev = create_event(delay, event_type);
	if(!ev) return;

	if(info) {
		ev->info = malloc(sizeof(*info));
		if(!ev->info) {
			free_event(ev);
			return;
		}
		*ev->info = *info;
	}

	switch (event_type) {
	case EVENT_MOBQUEUE:
		ch = entity;
		if( ch->events_tail )
			ch->events_tail->next_event = ev;
		else
			ch->events = ev;
		ch->events_tail = ev;
		break;
	case EVENT_OBJQUEUE:
		obj = entity;
		if( obj->events_tail )
			obj->events_tail->next_event = ev;
		else
			obj->events = ev;
		obj->events_tail = ev;
		break;
	case EVENT_ROOMQUEUE:
		room = entity;
		if( room->events_tail )
			room->events_tail->next_event = ev;
		else
			room->events = ev;
		room->events_tail = ev;
		break;
	case EVENT_TOKENQUEUE:
		token = entity;
		if( token->events_tail )
			token->events_tail->next_event = ev;
		else
			token->events = ev;
		token->events_tail = ev;
		break;
	case EVENT_ECHO:
		room = entity;
		if( room->events_tail )
			room->events_tail->next_event = ev;
		else
			room->events = ev;
		room->events_tail = ev;
		break;
	default:
		break;
	}

	ev->entity = entity;
	ev->function = function;
	ev->args = str_dup(argument);
	ev->depth = script_call_depth;
	ev->security = script_security;

	add_event(ev);
}

// @@@NIB - 20070119
// Remove all events via the next_event chain that still have
//	a delay on them.   This will handle the possibility
//	that this is called because of an event, since that
//	particular even will have a delay <= 0.
//
void wipe_owned_events(EVENT_DATA *ev)
{
	register EVENT_DATA *ev_next;

	for(;ev;ev = ev_next) {
		ev_next = ev->next_event;
		if(ev->delay > 0) {
			if(ev == next_event)
				next_event = ev->next;
			extract_event(ev);
		}
	}

}

void wipe_clearinfo_mobile(CHAR_DATA *mob)
{
	register EVENT_DATA *ev;
	register DESCRIPTOR_DATA *d;

	for(ev = events;ev;ev = ev->next) if(ev->info) {
		if(ev->info->mob == mob) {
			ev->info->mob = NULL;
			ev->info->var = NULL;
			ev->info->targ = NULL;
		}
		if(ev->info->ch == mob) ev->info->ch = NULL;
		if(ev->info->vch == mob) ev->info->vch = NULL;
		if(ev->info->rch == mob) ev->info->rch = NULL;
	}

	for (d = descriptor_list; d; d = d->next) if(d->input) {
		if(d->input_mob == mob) d->input_mob = NULL;
	}

}

void wipe_clearinfo_object(OBJ_DATA *obj)
{
	register EVENT_DATA *ev;
	register DESCRIPTOR_DATA *d;

	for(ev = events;ev;ev = ev->next) if(ev->info) {
		if(ev->info->obj == obj) {
			ev->info->obj = NULL;
			ev->info->var = NULL;
			ev->info->targ = NULL;
		}
		if(ev->info->obj1 == obj) ev->info->obj1 = NULL;
		if(ev->info->obj2 == obj) ev->info->obj2 = NULL;
	}

	for (d = descriptor_list; d; d = d->next) if(d->input) {
		if(d->input_obj == obj) d->input_obj = NULL;
	}
}

void wipe_clearinfo_token(TOKEN_DATA *token)
{
	register EVENT_DATA *ev;
	register DESCRIPTOR_DATA *d;

	for(ev = events;ev;ev = ev->next) if(ev->info) {
		if(ev->info->token == token) {
			ev->info->token = NULL;
			ev->info->var = NULL;
			ev->info->targ = NULL;
		}
	}
	for (d = descriptor_list; d; d = d->next) if(d->input) {
		if(d->input_tok == token) d->input_tok = NULL;
	}
}

void wipe_clearinfo_room(ROOM_INDEX_DATA *room)
{
	register EVENT_DATA *ev;
	register DESCRIPTOR_DATA *d;

	for(ev = events;ev;ev = ev->next) if(ev->info) {
		if(ev->info->room == room) {
			ev->info->room = NULL;
			ev->info->var = NULL;
			ev->info->targ = NULL;
		}
	}
	for (d = descriptor_list; d; d = d->next) if(d->input) {
		if(d->input_room == room) d->input_room = NULL;
	}
}

