/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,	   *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *									   *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael	   *
 *  Chastain, Michael Quan, and Mitchell Tse.				   *
 *									   *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc	   *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.						   *
 *									   *
 *  Much time and thought has gone into this software and you are	   *
 *  benefitting.  We hope that you share your changes too.  What goes	   *
 *  around, comes around.						   *
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include "strings.h"
#include "merc.h"
#include "db.h"
#include "math.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "wilds.h"
#include "protocol.h"
#include "olc.h"

long *new_long();
void free_long(long *l);
void delete_long(void *ptr);
WNUM_LOAD *new_list_wnum_load();
void free_list_wnum_load(WNUM_LOAD *wnum);
void delete_list_wnum_load(void *ptr);

void delete_rs_location(void *ptr);
void delete_location(void *ptr);

void dlgedit_buffer_nodes(CHAR_DATA *ch, BUFFER *buffer, DIALOGUE_INDEX_DATA *diag);
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

////////////////////////////////////////
//                                    //
//           Dialogue Trees           //
//                                    //
////////////////////////////////////////

#define DIALOGUE_ENTITY_PLAYER			(0xFF)
#define DIALOGUE_ENTITY_PLAYER_HE		(0xFE)
#define DIALOGUE_ENTITY_PLAYER_HIS		(0xFD)
#define DIALOGUE_ENTITY_PLAYER_HIM		(0xFC)
#define DIALOGUE_ENTITY_PLAYER_HISO		(0xFB)
#define DIALOGUE_ENTITY_VICTIM1			(0xFA)
#define DIALOGUE_ENTITY_VICTIM1_HE		(0xF9)
#define DIALOGUE_ENTITY_VICTIM1_HIS		(0xF8)
#define DIALOGUE_ENTITY_VICTIM1_HIM		(0xF7)
#define DIALOGUE_ENTITY_VICTIM1_HISO	(0xF6)
#define DIALOGUE_ENTITY_VICTIM2			(0xF5)
#define DIALOGUE_ENTITY_VICTIM2_HE		(0xF4)
#define DIALOGUE_ENTITY_VICTIM2_HIS		(0xF3)
#define DIALOGUE_ENTITY_VICTIM2_HIM		(0xF2)
#define DIALOGUE_ENTITY_VICTIM2_HISO	(0xF1)
#define DIALOGUE_ENTITY_OBJECT1			(0xF0)
#define DIALOGUE_ENTITY_OBJECT2			(0xEF)
#define DIALOGUE_ENTITY_ROOM			(0xEE)
#define DIALOGUE_ENTITY_AREA			(0xED)
#define DIALOGUE_ENTITY_0				(0xE0)
#define DIALOGUE_ENTITY_1				(0xE1)
#define DIALOGUE_ENTITY_2				(0xE2)
#define DIALOGUE_ENTITY_3				(0xE3)
#define DIALOGUE_ENTITY_4				(0xE4)
#define DIALOGUE_ENTITY_5				(0xE5)
#define DIALOGUE_ENTITY_6				(0xE6)
#define DIALOGUE_ENTITY_7				(0xE7)
#define DIALOGUE_ENTITY_8				(0xE8)
#define DIALOGUE_ENTITY_9				(0xE9)

struct dialogue_entity_code_type
{
	char *text;
	char code;
};

struct dialogue_entity_code_type dialogue_entity_codes[] =
{
	{ "(player)",			DIALOGUE_ENTITY_PLAYER		},
	{ "(player.he)",		DIALOGUE_ENTITY_PLAYER_HE	},
	{ "(player.his)",		DIALOGUE_ENTITY_PLAYER_HIS	},
	{ "(player.him)",		DIALOGUE_ENTITY_PLAYER_HIM	},
	{ "(player.hiso)",		DIALOGUE_ENTITY_PLAYER_HISO	},
	{ "(victim1)",			DIALOGUE_ENTITY_VICTIM1		},
	{ "(victim1.he)",		DIALOGUE_ENTITY_VICTIM1_HE	},
	{ "(victim1.his)",		DIALOGUE_ENTITY_VICTIM1_HIS	},
	{ "(victim1.him)",		DIALOGUE_ENTITY_VICTIM1_HIM	},
	{ "(victim1.hiso)",		DIALOGUE_ENTITY_VICTIM1_HISO	},
	{ "(victim2)",			DIALOGUE_ENTITY_VICTIM2		},
	{ "(victim2.he)",		DIALOGUE_ENTITY_VICTIM2_HE	},
	{ "(victim2.his)",		DIALOGUE_ENTITY_VICTIM2_HIS	},
	{ "(victim2.him)",		DIALOGUE_ENTITY_VICTIM2_HIM	},
	{ "(victim2.hiso)",		DIALOGUE_ENTITY_VICTIM2_HISO	},
	{ "(object1)",			DIALOGUE_ENTITY_OBJECT1		},
	{ "(object2)",			DIALOGUE_ENTITY_OBJECT2		},
	{ "(room)",				DIALOGUE_ENTITY_ROOM		},
	{ "(area)",				DIALOGUE_ENTITY_AREA		},
	{ "0",					DIALOGUE_ENTITY_0			},
	{ "1",					DIALOGUE_ENTITY_1			},
	{ "2",					DIALOGUE_ENTITY_2			},
	{ "3",					DIALOGUE_ENTITY_3			},
	{ "4",					DIALOGUE_ENTITY_4			},
	{ "5",					DIALOGUE_ENTITY_5			},
	{ "6",					DIALOGUE_ENTITY_6			},
	{ "7",					DIALOGUE_ENTITY_7			},
	{ "8",					DIALOGUE_ENTITY_8			},
	{ "9",					DIALOGUE_ENTITY_9			},
	{ NULL,					0x00						}
};

void init_node_text(NODE_TEXT *nt)
{
	nt->src = &str_empty[0];
	nt->text = &str_empty[0];
}

void free_node_text(NODE_TEXT *nt)
{
	free_string(nt->src);
	free_string(nt->text);
}

DIALOGUE_INDEX_CHOICE *dialogue_index_choice_free;
DIALOGUE_INDEX_CHOICE *new_dialogue_index_choice()
{
	DIALOGUE_INDEX_CHOICE *data;
	if (dialogue_index_choice_free)
	{
		data = dialogue_index_choice_free;
		dialogue_index_choice_free = dialogue_index_choice_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_INDEX_CHOICE));

	memset(data, 0, sizeof(*data));

	init_node_text(&data->text);
	data->hint = &str_empty[0];
	
	return data;
}

void free_dialogue_index_choice(DIALOGUE_INDEX_CHOICE *data)
{
	if (!data) return;

	free_node_text(&data->text);
	free_string(data->hint);

	data->next = dialogue_index_choice_free;
	dialogue_index_choice_free = data;
}

static void delete_dialogue_index_choice(void *ptr)
{
	free_dialogue_index_choice((DIALOGUE_INDEX_CHOICE *)ptr);
}

DIALOGUE_INDEX_BRANCH *dialogue_index_branch_free;
DIALOGUE_INDEX_BRANCH *new_dialogue_index_branch()
{
	DIALOGUE_INDEX_BRANCH *data;
	if (dialogue_index_branch_free)
	{
		data = dialogue_index_branch_free;
		dialogue_index_branch_free = dialogue_index_branch_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_INDEX_BRANCH));

	memset(data, 0, sizeof(*data));

	data->description = &str_empty[0];
	data->variable = &str_empty[0];
	data->value = &str_empty[0];

	return data;
}

void free_dialogue_index_branch(DIALOGUE_INDEX_BRANCH *data)
{
	if (!data) return;

	free_string(data->description);
	free_string(data->variable);
	free_string(data->value);

	data->next = dialogue_index_branch_free;
	dialogue_index_branch_free = data;
}

static void delete_dialogue_index_branch(void *ptr)
{
	free_dialogue_index_branch((DIALOGUE_INDEX_BRANCH *)ptr);
}

// The node must be created with the type in mind as it will affect the options list
DIALOGUE_INDEX_NODE *dialogue_index_node_free;
DIALOGUE_INDEX_NODE *new_dialogue_index_node(int16_t type)
{
	DIALOGUE_INDEX_NODE *data;

	if (dialogue_index_node_free)
	{
		data = dialogue_index_node_free;
		dialogue_index_node_free = dialogue_index_node_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_INDEX_NODE));
	
	memset(data, 0, sizeof(*data));

	init_node_text(&data->text);
	data->type = type;

	if (type == DIALOGUE_TYPE_BRANCH)
		data->options = list_createx(false, NULL, delete_dialogue_index_branch);
	else if (type == DIALOGUE_TYPE_CHOICE)
		data->options = list_createx(false, NULL, delete_dialogue_index_choice);
	else if (type == DIALOGUE_TYPE_SEQUENCE || type == DIALOGUE_TYPE_RANDOM)
		data->options = list_create(false);


	data->variable = &str_empty[0];
	data->value = &str_empty[0];

	VALIDATE(data);
	return data;
}

void free_dialogue_index_node(DIALOGUE_INDEX_NODE *data)
{
	if (!IS_VALID(data)) return;

	free_node_text(&data->text);
	free_string(data->variable);
	free_string(data->value);

	list_destroy(data->options);

	INVALIDATE(data);
	data->next = dialogue_index_node_free;
	dialogue_index_node_free = data;
}

static void delete_dialogue_index_node(void *ptr)
{
	free_dialogue_index_node((DIALOGUE_INDEX_NODE *)ptr);
}

DIALOGUE_INDEX_DATA *dialogue_index_free;
DIALOGUE_INDEX_DATA *new_dialogue_index_data()
{
	DIALOGUE_INDEX_DATA *data;
	if (dialogue_index_free)
	{
		data = dialogue_index_free;
		dialogue_index_free = dialogue_index_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_INDEX_DATA));

	memset(data, 0, sizeof(*data));

	data->name = &str_empty[0];
	data->description = &str_empty[0];
	data->comments = &str_empty[0];

	data->nodes = list_createx(false, NULL, delete_dialogue_index_node);

	data->areas = list_create(false);
	data->mobiles = list_create(false);
	data->objects = list_create(false);
	data->rooms = list_createx(false, NULL, delete_rs_location);
	
	VALIDATE(data);
	return data;
}

void free_dialogue_index_data(DIALOGUE_INDEX_DATA *data)
{
	if (!IS_VALID(data)) return;

	free_string(data->name);
	free_string(data->description);
	free_string(data->comments);

	list_destroy(data->nodes);

	list_destroy(data->areas);
	list_destroy(data->mobiles);
	list_destroy(data->objects);
	list_destroy(data->rooms);

	INVALIDATE(data);
	data->next = dialogue_index_free;
	dialogue_index_free = data;
}


DIALOGUE_BRANCH *dialogue_branch_free;
DIALOGUE_BRANCH *new_dialogue_branch()
{
	DIALOGUE_BRANCH *data;
	if (dialogue_branch_free)
	{
		data = dialogue_branch_free;
		dialogue_branch_free = dialogue_branch_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_BRANCH));
	
	memset(data, 0, sizeof(*data));

	data->description = &str_empty[0];
	data->variable = &str_empty[0];
	data->value = &str_empty[0];
	
	return data;
}

void free_dialogue_branch(DIALOGUE_BRANCH *data)
{
	if (!data) return;

	free_string(data->description);
	free_string(data->variable);
	free_string(data->value);

	data->next = dialogue_branch_free;
	dialogue_branch_free = data;
}

static void delete_dialogue_branch(void *ptr)
{
	free_dialogue_branch((DIALOGUE_BRANCH *)ptr);
}

DIALOGUE_CHOICE *dialogue_choice_free;
DIALOGUE_CHOICE *new_dialogue_choice()
{
	DIALOGUE_CHOICE *data;
	if (dialogue_choice_free)
	{
		data = dialogue_choice_free;
		dialogue_choice_free = dialogue_choice_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_CHOICE));
	
	memset(data, 0, sizeof(*data));

	init_node_text(&data->text);
	data->hint = &str_empty[0];

	return data;
}

void free_dialogue_choice(DIALOGUE_CHOICE *data)
{
	if (!data) return;

	free_node_text(&data->text);
	free_string(data->hint);

	data->next = dialogue_choice_free;
	dialogue_choice_free = data;
}

static void delete_dialogue_choice(void *ptr)
{
	free_dialogue_choice((DIALOGUE_CHOICE *)ptr);
}

DIALOGUE_NODE *dialogue_node_free;
DIALOGUE_NODE *new_dialogue_node(int16_t type)
{
	DIALOGUE_NODE *data;
	if (dialogue_node_free)
	{
		data = dialogue_node_free;
		dialogue_node_free = dialogue_node_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE_NODE));
	
	memset(data, 0, sizeof(*data));

	init_node_text(&data->text);
	data->type = type;

	if (type == DIALOGUE_TYPE_BRANCH)
		data->options = list_createx(false, NULL, delete_dialogue_branch);
	else if (type == DIALOGUE_TYPE_CHOICE)
		data->options = list_createx(false, NULL, delete_dialogue_choice);
	else if (type == DIALOGUE_TYPE_SEQUENCE || type == DIALOGUE_TYPE_RANDOM)
		data->options = list_create(false);

	data->variable = &str_empty[0];
	data->value = &str_empty[0];

	VALIDATE(data);
	return data;
}

void free_dialogue_node(DIALOGUE_NODE *data)
{
	if (!IS_VALID(data)) return;
	
	free_node_text(&data->text);
	free_string(data->variable);
	free_string(data->value);

	list_destroy(data->options);

	INVALIDATE(data);
	data->next = dialogue_node_free;
	dialogue_node_free = data;
}

static void delete_dialogue_node(void *ptr)
{
	free_dialogue_node((DIALOGUE_NODE *)ptr);
}

DIALOGUE *dialogue_free;
DIALOGUE *new_dialogue()
{
	DIALOGUE *data;
	if (dialogue_free)
	{
		data = dialogue_free;
		dialogue_free = dialogue_free->next;
	}
	else
		data = alloc_mem(sizeof(DIALOGUE));

	memset(data, 0, sizeof(*data));

	data->nodes = list_createx(false, NULL, delete_dialogue_node);

	data->areas = list_create(false);
	data->mobiles = list_create(false);
	data->objects = list_create(false);
	data->rooms = list_createx(false, NULL, delete_location);

	VALIDATE(data);
	return data;	
}

void free_dialogue(DIALOGUE *data)
{
	if (!IS_VALID(data)) return;

	variable_freelist(&data->variables);

	list_destroy(data->nodes);

	list_destroy(data->areas);
	list_destroy(data->mobiles);
	list_destroy(data->objects);
	list_destroy(data->rooms);

	INVALIDATE(data);
	data->next = dialogue_free;
	dialogue_free = data;
}




DIALOGUE_INDEX_DATA *get_dialogue_index(AREA_DATA *area, long vnum)
{
	if (!area || vnum < 1) return NULL;

	for(DIALOGUE_INDEX_DATA *index = area->dialogue_index_hash[vnum % MAX_KEY_HASH]; index; index = index->next)
	{
		if (index->vnum == vnum)
			return index;
	}

	return NULL;
}

DIALOGUE_INDEX_DATA *get_dialogue_index_wnum(WNUM wnum)
{
	return get_dialogue_index(wnum.pArea, wnum.vnum);
}

DIALOGUE_INDEX_DATA *get_dialogue_index_auid(long auid, long vnum)
{
	return get_dialogue_index(get_area_from_uid(auid), vnum);
}


DIALOGUE_NODE *get_dialogue_node(DIALOGUE *dialogue, long uid)
{
	ITERATOR it;

	DIALOGUE_NODE *node;
	iterator_start(&it, dialogue->nodes);
	while((node = (DIALOGUE_NODE *)iterator_nextdata(&it)))
	{
		if (node->uid == uid)
			break;
	}
	iterator_stop(&it);

	return node;
}

char *compile_dialogue_entity(BUFFER *buffer, char *input)
{
	for(int i = 0; dialogue_entity_codes[i].text; i++)
	{
		if (!str_prefix(dialogue_entity_codes[i].text, input))
		{
			log_stringf("Compile Node Text: '%s' => %02.2X", dialogue_entity_codes[i].text, dialogue_entity_codes[i].code);
			add_buf_char(buffer, dialogue_entity_codes[i].code);
			return input + strlen(dialogue_entity_codes[i].text);
		}
	}

	add_buf_char(buffer, *input);
	return input + 1;
}

char *compile_dialogue_text(char *input)
{
	BUFFER *buffer = new_buf();

	char *start = input;

	while(*input)
	{
		if (*input == '$')
		{
			input = compile_dialogue_entity(buffer, input + 1);
			if (!input)
			{
				free_buf(buffer);
				return str_dup(start);
			}
		}
		else
		{
			add_buf_char(buffer, *input);
			++input;
		}
	}

	char *str = str_dup(buffer->string);
	free_buf(buffer);

	return str;
}

void copy_node_text(NODE_TEXT *d, NODE_TEXT *s)
{
	d->src = str_dup(s->src);
	d->text = str_dup(s->text);
	d->victim1 = s->victim1;
	d->victim2 = s->victim2;
	d->object1 = s->object1;
	d->object2 = s->object2;
	d->room = s->room;
	d->area = s->area;
}

DIALOGUE *clone_dialogue(DIALOGUE_INDEX_DATA *index, CHAR_DATA *ch)
{
	ITERATOR it;

	DIALOGUE *dialogue = new_dialogue();
	dialogue->index = index;

	DIALOGUE_INDEX_NODE *node;
	DIALOGUE_NODE *new_node;

	// First, generate the bare nodes
	iterator_start(&it, index->nodes);
	while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		new_node = new_dialogue_node(node->type);

		new_node->parent = dialogue;
		new_node->index = node;
		new_node->uid = node->uid;

		list_appendlink(dialogue->nodes, new_node);
	}
	iterator_stop(&it);	

	// Finish cloning the data
	iterator_start(&it, dialogue->nodes);
	while((new_node = (DIALOGUE_NODE *)iterator_nextdata(&it)))
	{
		node = new_node->index;

		new_node->parent = dialogue;
		new_node->uid = node->uid;
		copy_node_text(&new_node->text, &node->text);
		new_node->delay = node->delay;
		new_node->speaker = node->speaker;

		if (node->type == DIALOGUE_TYPE_BRANCH)
		{
			ITERATOR oit;
			DIALOGUE_INDEX_BRANCH *branch;
			iterator_start(&oit, node->options);
			while((branch = (DIALOGUE_INDEX_BRANCH *)iterator_nextdata(&oit)))
			{
				DIALOGUE_BRANCH *new_branch = new_dialogue_branch();

				new_branch->description = str_dup(branch->description);
				new_branch->variable = str_dup(branch->variable);
				new_branch->value = str_dup(branch->value);
				if (branch->child)
					new_branch->child = get_dialogue_node(dialogue, branch->child->uid);
				else
					new_branch->child = NULL;

				list_appendlink(new_node->options, new_branch);	
			}
			iterator_stop(&oit);
		}
		else if (node->type == DIALOGUE_TYPE_CHOICE)
		{
			ITERATOR oit;
			DIALOGUE_INDEX_CHOICE *choice;
			iterator_start(&oit, node->options);
			while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&oit)))
			{
				int ret = PRET_ALLOWED;
				if (choice->visible)
				{
					// End 0/allow to be visible
					ret = execute_script(choice->visible, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
				}

				if (ret != PRET_ALLOWED)
				{
					// Not allowed to click on the choice
					if (ret != PRET_DENIED || IS_NULLSTR(choice->hint))
						continue;	// Either it's not the denied value or the choice has no hint defined on it.
				}

				DIALOGUE_CHOICE *new_choice = new_dialogue_choice();

				if (ret == PRET_DENIED)	// Visible but disabled
					new_choice->hint = str_dup(choice->hint);

				copy_node_text(&new_choice->text, &choice->text);
				if (choice->child)
					new_choice->child = get_dialogue_node(dialogue, choice->child->uid);
				else
					new_choice->child = NULL;

				list_appendlink(new_node->options, new_choice);	
			}
			iterator_stop(&oit);
		}
		else if (node->type == DIALOGUE_TYPE_RANDOM || node->type == DIALOGUE_TYPE_SEQUENCE)
		{
			ITERATOR oit;
			DIALOGUE_INDEX_NODE *index_node;
			iterator_start(&oit, node->options);
			while((index_node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
			{
				DIALOGUE_NODE *child_node = get_dialogue_node(dialogue, index_node->uid);
				list_appendlink(node->options, child_node);
			}
			iterator_stop(&oit);
		}

		if (node->for_child)
			new_node->for_child = get_dialogue_node(dialogue, node->for_child->uid);
		else
			new_node->for_child = NULL;

		if (node->child)
			new_node->child = get_dialogue_node(dialogue, node->child->uid);
		else
			new_node->child = NULL;
		
		new_node->script = node->script;
		new_node->variable = str_dup(node->variable);
		new_node->value = str_dup(node->value);
		new_node->destination = node->destination;
		new_node->for_total = node->for_total;
		new_node->for_index = -1;
		new_node->sequence = 1;
	}
	iterator_stop(&it);

	dialogue->areas = list_copy(index->areas);
	dialogue->mobiles = list_copy(index->mobiles);
	dialogue->objects = list_copy(index->objects);
	dialogue->rooms = list_copy(index->rooms);
	for(int i = 0; i < 10; i++)
		dialogue->numbers[i] = index->numbers[i];

	// First node is always the starting node
	dialogue->current_node = (DIALOGUE_NODE *)list_nthdata(dialogue->nodes, 1);

	return dialogue;
}

MOB_INDEX_DATA *get_dialogue_mobile(DIALOGUE *dialogue, int m)
{
	if(m > 0) return (MOB_INDEX_DATA *)list_nthdata(dialogue->mobiles, m);

	return NULL;
}

OBJ_INDEX_DATA *get_dialogue_object(DIALOGUE *dialogue, int o)
{
	if(o > 0) return (OBJ_INDEX_DATA *)list_nthdata(dialogue->objects, o);

	return NULL;
}

ROOM_INDEX_DATA *get_dialogue_room(DIALOGUE *dialogue, int r)
{
	if(r > 0) return (ROOM_INDEX_DATA *)list_nthdata(dialogue->rooms, r);

	return NULL;
}

AREA_DATA *get_dialogue_area(DIALOGUE *dialogue, int a)
{
	if(a > 0) return (AREA_DATA *)list_nthdata(dialogue->areas, a);

	return NULL;
}

long get_dialogue_number(DIALOGUE *dialogue, int n)
{
	if (n >= 0 && n < 10) return dialogue->numbers[n];

	return 0;
}

void expand_node_text(BUFFER *buffer, CHAR_DATA *ch, DIALOGUE *dialogue, NODE_TEXT *nt, char *msg_color)
{
	MOB_INDEX_DATA *v1 = get_dialogue_mobile(dialogue, nt->victim1);
	MOB_INDEX_DATA *v2 = get_dialogue_mobile(dialogue, nt->victim2);
	OBJ_INDEX_DATA *o1 = get_dialogue_object(dialogue, nt->object1);
	OBJ_INDEX_DATA *o2 = get_dialogue_object(dialogue, nt->object2);
	ROOM_INDEX_DATA *r = get_dialogue_room(dialogue, nt->room);
	AREA_DATA *a = get_dialogue_area(dialogue, nt->area);

	char *str = nt->text;
	while (*str)
	{
		switch(((int)*str)&0xFF)
		{
			case DIALOGUE_ENTITY_PLAYER:
				add_buf(buffer, ch->name);
				break;

			case DIALOGUE_ENTITY_PLAYER_HE:
				add_buf(buffer, (char*)he_she[URANGE(0, ch->sex, 2)]);
				break;

			case DIALOGUE_ENTITY_PLAYER_HIS:
				add_buf(buffer, (char*)his_her[URANGE(0, ch->sex, 2)]);
				break;

			case DIALOGUE_ENTITY_PLAYER_HIM:
				add_buf(buffer, (char*)him_her[URANGE(0, ch->sex, 2)]);
				break;

			case DIALOGUE_ENTITY_PLAYER_HISO:
				add_buf(buffer, (char*)his_hers[URANGE(0, ch->sex, 2)]);
				break;

			case DIALOGUE_ENTITY_VICTIM1:
				add_buf(buffer, (v1 ? v1->short_descr : SOMEONE));
				break;

			case DIALOGUE_ENTITY_VICTIM1_HE:
				add_buf(buffer, (char*)(v1 ? he_she[URANGE(0, v1->sex, 2)] : "it"));
				break;

			case DIALOGUE_ENTITY_VICTIM1_HIS:
				add_buf(buffer, (char*)(v1 ? his_her[URANGE(0, v1->sex, 2)] : "its"));
				break;

			case DIALOGUE_ENTITY_VICTIM1_HIM:
				add_buf(buffer, (char*)(v1 ? him_her[URANGE(0, v1->sex, 2)] : "it"));
				break;

			case DIALOGUE_ENTITY_VICTIM1_HISO:
				add_buf(buffer, (char*)(v1 ? his_hers[URANGE(0, v1->sex, 2)] : "its"));
				break;

			case DIALOGUE_ENTITY_VICTIM2:
				add_buf(buffer, (v2 ? v2->short_descr : SOMEONE));
				break;

			case DIALOGUE_ENTITY_VICTIM2_HE:
				add_buf(buffer, (char*)(v2 ? he_she[URANGE(0, v2->sex, 2)] : "it"));
				break;

			case DIALOGUE_ENTITY_VICTIM2_HIS:
				add_buf(buffer, (char*)(v2 ? his_her[URANGE(0, v2->sex, 2)] : "its"));
				break;

			case DIALOGUE_ENTITY_VICTIM2_HIM:
				add_buf(buffer, (char*)(v2 ? him_her[URANGE(0, v2->sex, 2)] : "it"));
				break;

			case DIALOGUE_ENTITY_VICTIM2_HISO:
				add_buf(buffer, (char*)(v2 ? his_hers[URANGE(0, v2->sex, 2)] : "its"));
				break;

			case DIALOGUE_ENTITY_OBJECT1:
				add_buf(buffer, "{W");
				add_buf(buffer, (o1 ? o1->short_descr : SOMETHING));
				if (msg_color) add_buf(buffer, msg_color);
				break;

			case DIALOGUE_ENTITY_OBJECT2:
				add_buf(buffer, "{W");
				add_buf(buffer, (o2 ? o2->short_descr : SOMETHING));
				if (msg_color) add_buf(buffer, msg_color);
				break;

			case DIALOGUE_ENTITY_ROOM:
				add_buf(buffer, "{W");
				add_buf(buffer, (r ? r->name : SOMEWHERE));
				if (msg_color) add_buf(buffer, msg_color);
				break;

			case DIALOGUE_ENTITY_AREA:
				add_buf(buffer, "{W");
				add_buf(buffer, (a ? a->name : SOMEWHERE));
				if (msg_color) add_buf(buffer, msg_color);
				break;

			case DIALOGUE_ENTITY_0:
			case DIALOGUE_ENTITY_1:
			case DIALOGUE_ENTITY_2:
			case DIALOGUE_ENTITY_3:
			case DIALOGUE_ENTITY_4:
			case DIALOGUE_ENTITY_5:
			case DIALOGUE_ENTITY_6:
			case DIALOGUE_ENTITY_7:
			case DIALOGUE_ENTITY_8:
			case DIALOGUE_ENTITY_9:
				add_buf(buffer, formatf("{G%ld", get_dialogue_number(dialogue, (int)(*str - DIALOGUE_ENTITY_0))));
				if (msg_color) add_buf(buffer, msg_color);
				break;

			default:
				add_buf_char(buffer, *str);
				break;
		}

		++str;
	}
}

void show_node_text(CHAR_DATA *ch, DIALOGUE *dialogue, NODE_TEXT *nt, char *command, char *hint)
{
	if (IS_NULLSTR(nt->text)) return;

	BUFFER *buffer = new_buf();

	bool enabled = IS_NULLSTR(hint) && true;

	expand_node_text(buffer, ch, dialogue, nt, NULL);

	if(enabled)
	{
		if (IS_NULLSTR(command) || !isMXP(ch->desc) || !IS_SET(ch->comm, COMM_MXP))
			send_to_char(buffer->string, ch);
		else
			send_to_char(formatf("\t<send href=\"%s\">%s\t</send>", command, buffer->string), ch);
	}
	else
	{
		// Node text is "disabled", so give hint as to why
		send_to_char(buffer->string, ch);
		send_to_char("\n\r", ch);
		send_to_char(string_indent(hint, 8), ch);
	}
	send_to_char("\n\r", ch);
	free_buf(buffer);
}

static void _process_speech_buffer(BUFFER *input, BUFFER *output, MOB_INDEX_DATA *speaker)
{
	char buf[MSL];
	bool break_line = true;
	char *msg = input->string;
	char *second = NULL;

	// Check if this is an exclaimation in the middle
	for(int i = 0; msg[i]; i++)
	{
		if (msg[i] == '!' && msg[i] != ' ')
		{
			break_line = false;
			break;
		}
	}

	if (break_line)
	{
		second = stptok(msg, buf, sizeof(buf), "!");
		second = skip_whitespace(second);

		if (*second != '\0')
		{
			add_buf(output, formatf("{C'%s{C!' exclaims %s{C. '%s{C'{x", buf, speaker->short_descr, second));
			return;
		}
	}

	// Check if this is an inquiry in the middle
	for(int i = 0; msg[i]; i++)
	{
		if (msg[i] == '?' && msg[i] != ' ')
		{
			break_line = false;
			break;
		}
	}

	if (break_line)
	{
		second = stptok(msg, buf, sizeof(buf), "?");
		second = skip_whitespace(second);

		if (*second != '\0')
		{
			add_buf(output, formatf("{C'%s{C?' asks %s{C. '%s{C'{x", buf, speaker->short_descr, second));
			return;
		}
	}

	// Check if this is a statement in the middle
	for(int i = 0; msg[i]; i++)
	{
		if (msg[i] == '.' && msg[i] != ' ')
		{
			break_line = false;
			break;
		}
	}

	if (break_line)
	{
		second = stptok(msg, buf, sizeof(buf), ".");
		second = skip_whitespace(second);

		if (*second != '\0')
		{
			add_buf(output, formatf("{C'%s{C.' says %s{C. '%s{C'{x", buf, speaker->short_descr, second));
			return;
		}
	}

	char last = msg[strlen(msg)-1];
	if (last == '!')
	{
		if (number_percent() < 50)
		{
			add_buf(output, formatf("{C'%s{C' exclaims %s{C.{x", msg, speaker->short_descr));
		}
		else
		{
			add_buf(output, formatf("{C%s{C exclaims, '%s{C'{x", speaker->short_descr, msg));
		}
	}
	else if (last == '?')
	{
		if (number_percent() < 50)
		{
			add_buf(output, formatf("{C'%s{C' asks %s{C.{x", msg, speaker->short_descr));
		}
		else
		{
			add_buf(output, formatf("{C%s{C asks, '%s{C'{x", speaker->short_descr, msg));
		}
	}
	else
	{
		if (number_percent() < 50)
		{
			add_buf(output, formatf("{C'%s{C' says %s{C.{x", msg, speaker->short_descr));
		}
		else
		{
			add_buf(output, formatf("{C%s{C says, '%s{C'{x", speaker->short_descr, msg));
		}
	}
}

void show_dialogue_speech(CHAR_DATA *ch, DIALOGUE *dialogue, int speaker, NODE_TEXT *nt)
{
	if (IS_NULLSTR(nt->text)) return;
	if (speaker < 1) return;

	MOB_INDEX_DATA *sp = get_dialogue_mobile(dialogue, speaker);
	if (!sp) return;	// No speaker found

	BUFFER *buffer = new_buf();

	// Get what is actually said
	expand_node_text(buffer, ch, dialogue, nt, "{C");

	BUFFER *speech = new_buf();

	// Generate the speech text using what was spoken coming from the speaker
	_process_speech_buffer(buffer, speech, sp);

	send_to_char(speech->string, ch);
	send_to_char("\n\r", ch);
	free_buf(buffer);
	free_buf(speech);
}

void show_dialogue_choices(CHAR_DATA *ch)
{
	DIALOGUE *dialogue = ch->dialogue;

	// Not in a dialogue
	if (!IS_VALID(dialogue))
	{
		ch->has_dialogue_choice = false;
		return;
	}

	// Not on a CHOICE node
	if (!IS_VALID(dialogue->current_node) || dialogue->current_node->type != DIALOGUE_TYPE_CHOICE)
	{
		ch->has_dialogue_choice = false;
		return;
	}

	DIALOGUE_NODE *node = dialogue->current_node;

	// Show the prompt (if any)
	show_node_text(ch, dialogue, &node->text, NULL, NULL);

	ITERATOR it;
	DIALOGUE_CHOICE *choice;
	int i = 0;
	iterator_start(&it, node->options);
	while((choice = (DIALOGUE_CHOICE *)iterator_nextdata(&it)))
	{
		char buf[MIL];
		++i;
		send_to_char(formatf("{x[{%c%2d{x] ", (IS_NULLSTR(choice->hint) ? 'W' : 'D'), i), ch);
		sprintf(buf, "%d", i);
		show_node_text(ch, dialogue, &choice->text, buf, choice->hint);
	}
	iterator_stop(&it);
}

void dialogue_set_variable(DIALOGUE *dialogue, DIALOGUE_NODE *node)
{
	if (IS_NULLSTR(node->value)) return;

	pVARIABLE var = variable_get(dialogue->variables, node->variable);
	switch(node->value[0])
	{
		case '=':		// Assign (INTEGER or BOOLEAN)
			if (is_number(node->value+1))
				variables_set_integer(&dialogue->variables, node->variable, atoi(node->value+1));
			else if (!str_prefix(node->value+1, "true") || !str_prefix(node->value+1, "yes") || !str_prefix(node->value+1, "on"))
				variables_set_boolean(&dialogue->variables, node->variable, true);
			else if (!str_prefix(node->value+1, "false") || !str_prefix(node->value+1, "no") || !str_prefix(node->value+1, "off"))
				variables_set_boolean(&dialogue->variables, node->variable, false);
			break;
		
		case '+':
			if (var && var->type == VAR_INTEGER)
				var->_.i += atoi(node->value+1);
			else
				variables_set_integer(&dialogue->variables, node->variable, atoi(node->value+1));
			break;
		
		case '-':
			if (var && var->type == VAR_INTEGER)
				var->_.i -= atoi(node->value+1);
			else
				variables_set_integer(&dialogue->variables, node->variable, -atoi(node->value+1));
			break;

		case '*':
			if (var && var->type == VAR_INTEGER)
				var->_.i *= atoi(node->value+1);
			else
				variables_set_integer(&dialogue->variables, node->variable, atoi(node->value+1));
			break;

		case '/':
		{
			// Used to check for division by zero
			int value = atoi(node->value+1);
			if (value == 0)
				variables_set_integer(&dialogue->variables, node->variable, 0);
			else if (var && var->type == VAR_INTEGER)
				var->_.i /= value;
			else
				variables_set_integer(&dialogue->variables, node->variable, 0);
			break;
		}

		case '!':
			if (var && var->type == VAR_BOOLEAN)
				var->_.boolean = !var->_.boolean;
			break;

		case ':':	// String assign
			variables_set_string(&dialogue->variables, node->variable, node->value+1, false);
			break;
	}
}

bool dialogue_branch_compare(DIALOGUE *dialogue, DIALOGUE_BRANCH *branch)
{
	pVARIABLE var = variable_get(dialogue->variables, branch->variable);

	if (!var) return false;

	// Need a comparison operator
	if (branch->value[0] == '\0')
		return false;

	char op = branch->value[0];
	char *vstr = branch->value + 1;

	if (var->type == VAR_INTEGER)
	{
		int value = atoi(vstr);

		switch(op)
		{
			case '=':	return var->_.i == value;
			case '>':	return var->_.i > value;
			case '<':	return var->_.i < value;
			case '!':	return var->_.i != value;
			default:	return false;
		}
	}
	else if (var->type == VAR_BOOLEAN)
	{
		if (op != '=') return false;

		bool value;
		if (is_number(vstr))
			value = atoi(vstr) != 0;
		else if(!str_prefix(vstr, "true") || !str_prefix(vstr, "yes") || !str_prefix(vstr, "on"))
			value = true;
		else if(!str_prefix(vstr, "false") || !str_prefix(vstr, "no") || !str_prefix(vstr, "off"))
			value = false;
		else
			return false;
		
		return var->_.boolean == value;
	}
	else if (var->type == VAR_STRING || var->type == VAR_STRING_S)
	{
		switch(op)
		{
			case '=':	return !str_cmp(var->_.s, vstr);
			case '*':	return !str_prefix(var->_.s, vstr);
			case '^':	return !str_infix(var->_.s, vstr);
			default:	return false;
		}
	}

	return false;
}

DIALOGUE_NODE *select_dialogue_branch(CHAR_DATA *ch, DIALOGUE *dialogue, DIALOGUE_NODE *node)
{
	ITERATOR it;
	DIALOGUE_BRANCH *branch;
	DIALOGUE_NODE *next_node = NULL;

	iterator_start(&it, node->options);
	while((branch = (DIALOGUE_BRANCH *)iterator_nextdata(&it)))
	{
		if (dialogue_branch_compare(dialogue, branch))
		{
			next_node = branch->child;
			break;
		}
	}
	iterator_stop(&it);

	return next_node;
}

static void __teleport_entourage(CHAR_DATA *ch, ROOM_INDEX_DATA *dest)
{
	if (!IS_VALID(ch)) return;
	if (ch->in_room)
	{
		CHAR_DATA *mob_next;
		for(CHAR_DATA *mob = ch->in_room->people; mob; mob = mob_next)
		{
			mob_next = mob->next_in_room;

			// Must be CH's pet or follower
			if (mob == ch->pet ||
				mob->master == ch)
			{
				char_from_room(mob);
				char_to_room(mob, dest);
			}
		}

		char_from_room(ch);
	}

	char_to_room(ch, dest);
}

// Only teleports immediate entourage within the room
void dialogue_teleport(CHAR_DATA *ch, DIALOGUE *dialogue, int r)
{
	if (IS_NPC(ch)) return;	// Does not work for mobs

	LOCATION *loc = (LOCATION *)list_nthdata(dialogue->rooms, r);

	ROOM_INDEX_DATA *room = location_to_room(loc);
	if (!room) return;	// Room doesn't exist

	__teleport_entourage(MOUNTED(ch), room);	// Send the mount and its entourage
	__teleport_entourage(RIDDEN(ch), room);		// Send the rider and its entourage
	__teleport_entourage(ch, room);				// Send the actual viewer and its entourage

	// This isn't 100% perfect...
	//	pets of followers aren't included
	//	followers of followers aren't included
}

void execute_dialogue_node(CHAR_DATA *ch);

void dialogue_select_choice(CHAR_DATA *ch, DIALOGUE *dialogue, int c)
{
	DIALOGUE_CHOICE *choice = (DIALOGUE_CHOICE *)list_nthdata(dialogue->current_node->options, c);

	// Invalid choice
	if(!choice)
	{
		if (IS_NPC(ch))
		{
			// Pick a random choice
			c = number_range(1, list_size(dialogue->current_node->options));
			choice = (DIALOGUE_CHOICE *)list_nthdata(dialogue->current_node->options, c);
		}
		else
		{
			show_dialogue_choices(ch);
			return;
		}
	}

	// The choice is disabled
	// NPCs can select "disabled" choices
	else if (!IS_NPC(ch) && !IS_NULLSTR(choice->hint))
	{
		show_dialogue_choices(ch);
		return;
	}

	ch->dialogue->current_node = choice->child;
	ch->has_dialogue_choice = false;
	do {
		execute_dialogue_node(ch);
	} while(IS_VALID(ch->dialogue) && ch->dialogue->timer < 1 && !ch->has_dialogue_choice);

}

void execute_dialogue_node(CHAR_DATA *ch)
{
	DIALOGUE *dialogue = ch->dialogue;

	if (!IS_VALID(dialogue)) return;

	DIALOGUE_NODE *node = dialogue->current_node;

	if (!IS_VALID(node))
	{
		if (dialogue->index->completed)
			execute_script(dialogue->index->completed, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

		if (dialogue->callback)
			(*(dialogue->callback))(ch, dialogue);

		ch->dialogue = NULL;
		free_dialogue(dialogue);
		return;
	}

	DIALOGUE_NODE *next_node = NULL;
	dialogue->timer = node->delay;

	switch(node->type)
	{
		case DIALOGUE_TYPE_TEXT:
			show_node_text(ch, dialogue, &node->text, NULL, NULL);
			next_node = node->child;
			break;

		case DIALOGUE_TYPE_SET:
			dialogue_set_variable(dialogue, node);
			next_node = node->child;	
			break;

		case DIALOGUE_TYPE_SPEECH:
			show_dialogue_speech(ch, dialogue, node->speaker, &node->text);
			next_node = node->child;	
			break;
		
		case DIALOGUE_TYPE_SCRIPT:
			if (node->script)
				execute_script(node->script, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
			next_node = node->child;
			break;

		case DIALOGUE_TYPE_BRANCH:
			next_node = select_dialogue_branch(ch, dialogue, node);
			break;

		case DIALOGUE_TYPE_CHOICE:
			if (IS_NPC(ch))
			{
				// Return value is the choice number
				int choice = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DIALOGUE_CHOICE, NULL, 0,0,0,0,0);

				if (choice < 1)
					choice = number_range(1, list_size(node->options));

				dialogue_select_choice(ch, dialogue, choice);
			}
			else
			{
				ch->has_dialogue_choice = true;
				show_dialogue_choices(ch);
			}
			return;

		case DIALOGUE_TYPE_TELEPORT:
			dialogue_teleport(ch, dialogue, node->destination);
			return;

		case DIALOGUE_TYPE_FOR:
			{
				next_node = node->for_child;
				int total = (int)get_dialogue_number(dialogue, node->for_total);
				if (++node->for_index > total)
				{
					// End of the FOR loop
					node->for_index = 0;
					next_node = node->child;
				}
			}
			break;

		case DIALOGUE_TYPE_RANDOM:
			next_node = (DIALOGUE_NODE *)list_randomdata(node->options);
			break;

		case DIALOGUE_TYPE_SEQUENCE:
			next_node = (DIALOGUE_NODE *)list_nthdata(node->options, node->sequence);
			if (++node->sequence > list_size(node->options))
				node->sequence = 1;
			break;
	}

	dialogue->current_node = next_node;
}

// The dialogue is cloned and registry populated before this
bool start_dialogue(CHAR_DATA *ch, DIALOGUE *dialogue, DIALOGUE_CALLBACK cb)
{
	if (!IS_VALID(dialogue)) return false;

	dialogue->callback = cb;
	ch->dialogue = dialogue;

	// Use this to initialize variables to the dialogue based upon the viewer
	if (dialogue->index->initialize)
		execute_script(dialogue->index->initialize, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

	ch->has_dialogue_choice = false;
	do {
		execute_dialogue_node(ch);
	} while(IS_VALID(ch->dialogue) && dialogue->timer < 1 && !ch->has_dialogue_choice);

	return true;
}

bool set_dialogue_mobile(DIALOGUE *dialogue, int m, MOB_INDEX_DATA *mob)
{
	return list_setnthdata(dialogue->mobiles, mob, m, false);
}

bool set_dialogue_object(DIALOGUE *dialogue, int o, OBJ_INDEX_DATA *obj)
{
	return list_setnthdata(dialogue->objects, obj, o, false);
}

bool set_dialogue_location(DIALOGUE *dialogue, int r, LOCATION *loc)
{
	LOCATION *l = (LOCATION *)list_nthdata(dialogue->rooms, r);


	if (!l) return false;

	// Copy everything
	*l = *loc;
	return true;
}

bool set_dialogue_room(DIALOGUE *dialogue, int r, ROOM_INDEX_DATA *room)
{
	LOCATION loc;

	location_from_room(&loc, room);

	return set_dialogue_location(dialogue, r, &loc);
}

bool set_dialogue_wilderness(DIALOGUE *dialogue, int r, WILDS_DATA *wilds, long x, long y)
{
	LOCATION loc;
	
	location_clear(&loc);
	loc.wuid = wilds->uid;
	loc.id[0] = x;
	loc.id[1] = y;

	return set_dialogue_location(dialogue, r, &loc);
}

bool set_dialogue_area(DIALOGUE *dialogue, int a, AREA_DATA *area)
{
	return list_setnthdata(dialogue->areas, area, a, false);
}

void handle_dialogue(CHAR_DATA *ch)
{
	// Ignore handling the actual dialogue
	if (ch->has_dialogue_choice) return;

	DIALOGUE *dialogue = ch->dialogue;

	if (!IS_VALID(dialogue)) return;

	if (dialogue->timer > 0)
	{
		--dialogue->timer;
		//send_to_char(formatf("Dialogue timer: %d\n", dialogue->timer), ch);

		while(IS_VALID(ch->dialogue) && dialogue->timer < 1 && !ch->has_dialogue_choice)
		{
			execute_dialogue_node(ch);
		}
	}
}

void handle_dialogue_choice(CHAR_DATA *ch, char *input)
{
	if (!IS_VALID(ch)) return;
	if (!IS_VALID(ch->dialogue)) return;
	if (!IS_VALID(ch->dialogue->current_node) || ch->dialogue->current_node->type != DIALOGUE_TYPE_CHOICE) return;

	if (!is_number(input))
	{
		show_dialogue_choices(ch);
		return;
	}
	int value = atoi(input);

	dialogue_select_choice(ch, ch->dialogue, value);
}



void fix_dialogues()
{
	for(AREA_DATA *area = area_first; area; area = area->next)
		for(int i = 0; i < MAX_KEY_HASH; i++)
			for(DIALOGUE_INDEX_DATA *dialogue = area->dialogue_index_hash[i]; dialogue; dialogue = dialogue->next)
			{
				ITERATOR it;
				LLIST *areas = list_create(false);
				long *puid;
				iterator_start(&it, dialogue->areas);
				while((puid = (long *)iterator_nextdata(&it)))
				{
					AREA_DATA *a = get_area_from_uid(*puid);

					if (a)
						list_appendlink(areas, a);
				}
				iterator_stop(&it);
				list_destroy(dialogue->areas);
				dialogue->areas = areas;

				WNUM_LOAD *load;

				LLIST *mobiles = list_create(false);
				iterator_start(&it, dialogue->mobiles);
				while((load = (WNUM_LOAD *)iterator_nextdata(&it)))
				{
					MOB_INDEX_DATA *m = get_mob_index_auid(load->auid, load->vnum);

					if (m)
						list_appendlink(mobiles, m);
				}
				iterator_stop(&it);
				list_destroy(dialogue->mobiles);
				dialogue->mobiles = mobiles;

				LLIST *objects = list_create(false);
				iterator_start(&it, dialogue->objects);
				while((load = (WNUM_LOAD *)iterator_nextdata(&it)))
				{
					OBJ_INDEX_DATA *o = get_obj_index_auid(load->auid, load->vnum);

					if (o)
						list_appendlink(objects, o);
				}
				iterator_stop(&it);
				list_destroy(dialogue->objects);
				dialogue->objects = objects;

				if (dialogue->initialize_load.auid > 0 && dialogue->initialize_load.vnum > 0)
					dialogue->initialize = get_script_index_auid(dialogue->initialize_load.auid, dialogue->initialize_load.vnum, PRG_MPROG);
				
				if (dialogue->completed_load.auid > 0 && dialogue->completed_load.vnum > 0)
					dialogue->completed = get_script_index_auid(dialogue->completed_load.auid, dialogue->completed_load.vnum, PRG_MPROG);

				ITERATOR nit;
				DIALOGUE_INDEX_NODE *node;
				iterator_start(&nit, dialogue->nodes);
				while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&nit)))
				{
					if (node->type == DIALOGUE_TYPE_CHOICE)
					{
						ITERATOR oit;
						DIALOGUE_INDEX_CHOICE *choice;
						iterator_start(&oit, node->options);
						while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&oit)))
						{
							if (choice->visible_load.auid > 0 && choice->visible_load.vnum > 0)
							{
								choice->visible = get_script_index_auid(choice->visible_load.auid, choice->visible_load.vnum, PRG_MPROG);
							}
						}
						iterator_stop(&oit);
					}

					if (node->script_load.auid > 0 && node->script_load.vnum > 0)
						node->script = get_script_index_auid(node->script_load.auid, node->script_load.vnum, PRG_MPROG);
				}
				iterator_stop(&nit);
			}
}

char *compile_dialogue_text(char *input);

void read_node_text(FILE *fp, NODE_TEXT *nt)
{
	char *word;
	bool fMatch;

	nt->src = fread_string(fp);
	nt->text = compile_dialogue_text(nt->src);

	while (str_cmp((word = fread_word(fp)), "#-TEXT"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
			case 'A':
				KEY("Area", nt->area, fread_number(fp));
				break;

			case 'O':
				KEY("Object1", nt->object1, fread_number(fp));
				KEY("Object2", nt->object2, fread_number(fp));
				break;

			case 'R':
				KEY("Room", nt->room, fread_number(fp));
				break;

			case 'V':
				KEY("Victim1", nt->victim1, fread_number(fp));
				KEY("Victim2", nt->victim2, fread_number(fp));
				break;
		}

		if (!fMatch) {
			bug(formatf("read_node_text: no match for word %.50s", word), 0);
		}
	}
}


DIALOGUE_INDEX_NODE *get_dialogue_index_node(DIALOGUE_INDEX_DATA *dialogue, long uid)
{
	ITERATOR it;
	DIALOGUE_INDEX_NODE *node;

	iterator_start(&it, dialogue->nodes);
	while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		if (node->uid == uid)
			break;
	}
	iterator_stop(&it);

	return node;
}

DIALOGUE_INDEX_BRANCH *read_dialogue_index_branch(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, AREA_DATA *area)
{
	DIALOGUE_INDEX_BRANCH *branch = new_dialogue_index_branch();
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#-BRANCH"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
			case 'C':
				KEY("Child", branch->child, get_dialogue_index_node(dialogue, fread_number(fp)));
				break;

			case 'D':
				KEYS("Description", branch->description, fread_string(fp));
				break;

			case 'V':
				KEYS("Value", branch->value, fread_string(fp));
				KEYS("Variable", branch->variable, fread_string(fp));
				break;

		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index_branch: no match for word %.50s", word), 0);
		}
	}

	return branch;
}

DIALOGUE_INDEX_CHOICE *read_dialogue_index_choice(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, AREA_DATA *area)
{
	DIALOGUE_INDEX_CHOICE *choice = new_dialogue_index_choice();
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#-CHOICE"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
			case '#':
				if (!str_cmp(word, "#TEXT"))
				{
					read_node_text(fp, &choice->text);
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Child", choice->child, get_dialogue_index_node(dialogue, fread_number(fp)));
				break;

			case 'H':
				KEYS("Hint", choice->hint, fread_string(fp));
				break;

			case 'V':
				KEY("Visible", choice->visible_load, fread_widevnum(fp, area->uid));
				break;
		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index_choice: no match for word %.50s", word), 0);
		}
	}

	return choice;
}

bool read_dialogue_index_for(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, DIALOGUE_INDEX_NODE *node)
{
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), "#-FOR"))
	{
		fMatch = false;

		if (!str_cmp(word, "Child"))
		{
			long uid = fread_number(fp);
			DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(dialogue, uid);
			if (!IS_VALID(child))
			{
				bug(formatf("read_dialogue_index_for: no such index node with uid %ld.", uid), 0);
				return false;
			}
			
			node->for_child = child;
			fMatch = true;
		}
		else if (!str_cmp(word, "Count"))
		{
			int slot = fread_number(fp);
			if (slot < 0 || slot > 9)
			{
				bug(formatf("read_dialogue_index_for: invalid slot (%d) for count number.", slot), 0);
				return false;
			}

			node->for_total = slot;
			fMatch = true;
		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index_for: no match for word %.50s", word), 0);
		}
	}

	return true;
}

bool read_dialogue_index_listtype(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, DIALOGUE_INDEX_NODE *node, char *closer)
{
	char *word;
	bool fMatch;

	while (str_cmp((word = fread_word(fp)), closer))
	{
		fMatch = false;

		if (!str_cmp(word, "Node"))
		{
			long uid = fread_number(fp);
			DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(dialogue, uid);

			if (!IS_VALID(child))
			{
				bug(formatf("read_dialogue_index_listtype(%s): no such index node with uid %ld.", closer + 2, uid), 0);
				return false;
			}

			list_appendlink(node->options, child);
			fMatch = true;
		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index_listtype: no match for word %.50s", word), 0);
		}
	}

	return true;
}

bool read_dialogue_index_node(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, AREA_DATA *area)
{
	char *word;
	bool fMatch;

	long uid = fread_number(fp);

	DIALOGUE_INDEX_NODE *node = get_dialogue_index_node(dialogue, uid);
	if (!IS_VALID(node))	// UID not found
		return false;

	while (str_cmp((word = fread_word(fp)), "#-NODE"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
			case '#':
				if (node->type == DIALOGUE_TYPE_BRANCH && !str_cmp(word, "#BRANCH"))
				{
					DIALOGUE_INDEX_BRANCH *branch = read_dialogue_index_branch(fp, dialogue, area);

					if (branch)
						list_appendlink(node->options, branch);
					fMatch = true;
					break;
				}
				else if (node->type == DIALOGUE_TYPE_CHOICE && !str_cmp(word, "#CHOICE"))
				{
					DIALOGUE_INDEX_CHOICE *choice = read_dialogue_index_choice(fp, dialogue, area);

					if (choice)
						list_appendlink(node->options, choice);
					fMatch = true;
					break;
				}
				else if (node->type == DIALOGUE_TYPE_RANDOM && !str_cmp(word, "#RANDOM"))
				{
					read_dialogue_index_listtype(fp, dialogue, node, "#-RANDOM");
					fMatch = true;
					break;
				}
				else if (node->type == DIALOGUE_TYPE_SEQUENCE && !str_cmp(word, "#SEQUENCE"))
				{
					read_dialogue_index_listtype(fp, dialogue, node, "#-SEQUENCE");
					fMatch = true;
					break;
				}
				else if (node->type == DIALOGUE_TYPE_FOR && !str_cmp(word, "#FOR"))
				{
					read_dialogue_index_for(fp, dialogue, node);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#TEXT"))
				{
					read_node_text(fp, &node->text);
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Child", node->child, get_dialogue_index_node(dialogue, fread_number(fp)));
				break;

			case 'D':
				KEY("Delay", node->delay, fread_number(fp));
				KEY("Destination", node->destination, fread_number(fp));
				break;

			case 'S':
				KEY("Script", node->script_load, fread_widevnum(fp, area->uid));
				KEY("Speaker", node->speaker, fread_number(fp));
				break;
			
			case 'V':
				KEYS("Value", node->value, fread_string(fp));
				KEYS("Variable", node->variable, fread_string(fp));
				break;
		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index_node: no match for word %.50s", word), 0);
		}
	}

	return true;
}

DIALOGUE_INDEX_DATA *read_dialogue_index(FILE *fp, AREA_DATA *area)
{
	DIALOGUE_INDEX_DATA *dialogue = new_dialogue_index_data();
	char *word;
	bool fMatch;

	dialogue->vnum = fread_number(fp);

	area->top_dialogue_vnum = UMAX(area->top_dialogue_vnum, dialogue->vnum);
	area->bottom_dialogue_vnum = UMIN(area->bottom_dialogue_vnum, dialogue->vnum);

	dialogue->areas->deleter = delete_long;
	dialogue->mobiles->deleter = delete_list_wnum_load;
	dialogue->objects->deleter = delete_list_wnum_load;
	dialogue->rooms->deleter = delete_list_wnum_load;

	while (str_cmp((word = fread_word(fp)), "#-DIALOGUE"))
	{
		fMatch = false;

		switch(UPPER(word[0]))
		{
			case '#':
				if (!str_cmp(word, "#NODE"))
				{
					read_dialogue_index_node(fp, dialogue, area);
					fMatch = true;
					break;
				}
				break;

			case 'A':
				if (!str_cmp(word, "Area"))
				{
					long uid = fread_number(fp);

					long *puid = new_long();
					if (puid)
					{
						*puid = uid;

						if (!list_appendlink(dialogue->areas, puid))
							free_long(puid);
					}

					fMatch = true;
					break;
				}
				break;
			
			case 'C':
				KEYS("Comments", dialogue->comments, fread_string(fp));
				KEY("Completed", dialogue->completed_load, fread_widevnum(fp, area->uid));
				break;
			
			case 'D':
				KEYS("Description", dialogue->description, fread_string(fp));
				break;
			
			case 'F':
				KEY("Flags", dialogue->flags, fread_flag(fp));
				break;

			case 'I':
				KEY("Initialize", dialogue->initialize_load, fread_widevnum(fp, area->uid));
				break;

			case 'M':
				if (!str_cmp(word, "Mobile"))
				{
					WNUM_LOAD *load = fread_widevnumptr(fp, area->uid);

					if(load)
					{
						if (!list_appendlink(dialogue->mobiles, load))
							free_list_wnum_load(load);
					}

					fMatch = true;
					break;
				}
				break;

			case 'N':
				KEYS("Name", dialogue->name, fread_string(fp));
				if (!str_cmp(word, "Node"))
				{
					long uid = fread_number(fp);
					int16_t type = stat_lookup(fread_string(fp), dialogue_node_types, -1);

					if (type >= 0)
					{
						DIALOGUE_INDEX_NODE *node = new_dialogue_index_node(type);

						node->parent = dialogue;
						node->uid = uid;

						list_appendlink(dialogue->nodes, node);

						if (node->uid > dialogue->top_node_uid)
							dialogue->top_node_uid = node->uid;
					}
					// else Complain

					fMatch = true;
					break;
				}
				break;

			case 'O':
				if (!str_cmp(word, "Object"))
				{
					WNUM_LOAD *load = fread_widevnumptr(fp, area->uid);

					if(load)
					{
						if (!list_appendlink(dialogue->objects, load))
							free_list_wnum_load(load);
					}

					fMatch = true;
					break;
				}
				break;
			
			case 'R':
				if (!str_cmp(word, "Room"))
				{
					RS_LOCATION *loc = new_rs_location();

					if(loc)
					{
						loc->auid = fread_number(fp);
						loc->wuid = fread_number(fp);
						loc->id[0] = fread_number(fp);
						loc->id[1] = fread_number(fp);
						loc->id[2] = fread_number(fp);

						if (!list_appendlink(dialogue->rooms, loc))
							free_rs_location(loc);
					}

					fMatch = true;
					break;
				}
				break;
		}

		if (!fMatch) {
			bug(formatf("read_dialogue_index: no match for word %.50s", word), 0);
		}
	}

	log_stringf("Dialogue %ld loaded.", dialogue->vnum);
	return dialogue;
}

void save_node_text(FILE *fp, NODE_TEXT *nt)
{
	if (IS_NULLSTR(nt->text)) return;

	fprintf(fp, "#TEXT %s~\n", fix_string(nt->src));
	if (nt->victim1 > 0) fprintf(fp, "Victim1 %d\n", nt->victim1);
	if (nt->victim2 > 0) fprintf(fp, "Victim2 %d\n", nt->victim2);
	if (nt->object1 > 0) fprintf(fp, "Object1 %d\n", nt->object1);
	if (nt->object2 > 0) fprintf(fp, "Object2 %d\n", nt->object2);
	if (nt->room > 0) fprintf(fp, "Room %d\n", nt->room);
	if (nt->area > 0) fprintf(fp, "Area %d\n", nt->area);
	fprintf(fp, "#-TEXT\n");
}

void save_dialogue_index_node(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, DIALOGUE_INDEX_NODE *node, AREA_DATA *area)
{
	fprintf(fp, "#NODE %ld\n", node->uid);
	save_node_text(fp, &node->text);
	fprintf(fp, "Delay %d\n", node->delay);

	switch(node->type)
	{
		case DIALOGUE_TYPE_BRANCH:
		{
			ITERATOR oit;
			DIALOGUE_INDEX_BRANCH *branch;

			iterator_start(&oit, node->options);
			while((branch = (DIALOGUE_INDEX_BRANCH *)iterator_nextdata(&oit)))
			{
				fprintf(fp, "#BRANCH\n");
				fprintf(fp, "Description %s~\n", fix_string(branch->description));
				fprintf(fp, "Variable %s~\n", fix_string(branch->variable));
				fprintf(fp, "Value %s~\n", fix_string(branch->value));
				if (branch->child)
					fprintf(fp, "Child %ld\n", branch->child->uid);
				fprintf(fp, "#-BRANCH\n");
			}
			iterator_stop(&oit);
			break;
		}

		case DIALOGUE_TYPE_CHOICE:
		{
			ITERATOR oit;
			DIALOGUE_INDEX_CHOICE *choice;

			iterator_start(&oit, node->options);
			while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&oit)))
			{
				fprintf(fp, "#CHOICE\n");
				if (choice->visible)
					fprintf(fp, "Visible %s\n", widevnum_string_script(choice->visible, area));
				save_node_text(fp, &choice->text);
				if (!IS_NULLSTR(choice->hint))
					fprintf(fp, "Hint %s~\n", fix_string(choice->hint));
				if (choice->child)
					fprintf(fp, "Child %ld\n", choice->child->uid);
				fprintf(fp, "#-CHOICE\n");
			}
			iterator_stop(&oit);
			break;
		}

		case DIALOGUE_TYPE_FOR:
			fprintf(fp, "#FOR\n");
			if (IS_VALID(node->for_child))
				fprintf(fp, "Child %ld\n", node->for_child->uid);
			fprintf(fp, "Count %d\n", node->for_total);
			fprintf(fp, "#-FOR\n");
			break;

		case DIALOGUE_TYPE_RANDOM:
		{
			ITERATOR oit;
			DIALOGUE_INDEX_NODE *child;

			fprintf(fp, "#RANDOM\n");
			iterator_start(&oit, node->options);
			while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
			{
				fprintf(fp, "Node %ld\n", child->uid);
			}
			iterator_stop(&oit);
			fprintf(fp, "#-RANDOM\n");
			
			break;
		}

		case DIALOGUE_TYPE_SEQUENCE:
		{
			ITERATOR oit;
			DIALOGUE_INDEX_NODE *child;

			fprintf(fp, "#SEQUENCE\n");
			iterator_start(&oit, node->options);
			while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
			{
				fprintf(fp, "Node %ld\n", child->uid);
			}
			iterator_stop(&oit);
			fprintf(fp, "#-SEQUENCE\n");
			
			break;
		}

		case DIALOGUE_TYPE_SPEECH:
			fprintf(fp, "Speaker, %d\n", node->speaker);
			break;

		case DIALOGUE_TYPE_SCRIPT:
			fprintf(fp, "Script %s\n", widevnum_string_script(node->script, area));
			break;

		case DIALOGUE_TYPE_SET:
			fprintf(fp, "Variable %s~\n", fix_string(node->variable));
			fprintf(fp, "Value %s~\n", fix_string(node->value));
			break;

		case DIALOGUE_TYPE_TELEPORT:
			fprintf(fp, "Destination %d\n", node->destination);
			break;

	}

	if (node->child)
		fprintf(fp, "Child %ld\n", node->child->uid);

	fprintf(fp, "#-NODE\n");
}

void save_dialogue_index(FILE *fp, DIALOGUE_INDEX_DATA *dialogue, AREA_DATA *area)
{
	fprintf(fp, "#DIALOGUE %ld\n", dialogue->vnum);

	fprintf(fp, "Name %s~\n", fix_string(dialogue->name));
	fprintf(fp, "Description %s~\n", fix_string(dialogue->description));
	fprintf(fp, "Comments %s~\n", fix_string(dialogue->comments));

	fprintf(fp, "Flags %s\n", print_flags(dialogue->flags));

	if (dialogue->initialize)
		fprintf(fp, "Initialize %s\n", widevnum_string_script(dialogue->initialize, area));

	if (dialogue->completed)
		fprintf(fp, "Completed %s\n", widevnum_string_script(dialogue->completed, area));

	ITERATOR it;
	DIALOGUE_INDEX_NODE *node;

	// First, save the list of nodes with their UIDs
	//   This will make connecting the nodes at loading easier
	iterator_start(&it, dialogue->nodes);
	while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Node %ld %s~\n", node->uid, flag_string(dialogue_node_types, node->type));
	}
	iterator_stop(&it);

	// Second, save the *actual* node definitions
	iterator_start(&it, dialogue->nodes);
	while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		save_dialogue_index_node(fp, dialogue, node, area);
	}
	iterator_stop(&it);

	AREA_DATA *ar;
	iterator_start(&it, dialogue->areas);
	while((ar = (AREA_DATA *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Area %ld\n", ar->uid);
	}
	iterator_stop(&it);

	MOB_INDEX_DATA *mob;
	iterator_start(&it, dialogue->mobiles);
	while((mob = (MOB_INDEX_DATA *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Mobile %s\n", widevnum_string(mob->area, mob->vnum, area));
	}
	iterator_stop(&it);

	OBJ_INDEX_DATA *obj;
	iterator_start(&it, dialogue->objects);
	while((obj = (OBJ_INDEX_DATA *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Object %s\n", widevnum_string(obj->area, obj->vnum, area));
	}
	iterator_stop(&it);

	RS_LOCATION *rs_loc;
	iterator_start(&it, dialogue->rooms);
	while((rs_loc = (RS_LOCATION *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Room %ld %ld %ld %ld %ld\n",
			rs_loc->auid,
			rs_loc->wuid,
			rs_loc->id[0],
			rs_loc->id[1],
			rs_loc->id[2]);
	}
	iterator_stop(&it);

	fprintf(fp, "#-DIALOGUE\n");
}

void save_dialogues(FILE *fp, AREA_DATA *area)
{
	for(int i = 0; i < MAX_KEY_HASH; i++)
	{
		for(DIALOGUE_INDEX_DATA *dialogue = area->dialogue_index_hash[i]; dialogue; dialogue = dialogue->next)
		{
			save_dialogue_index(fp, dialogue, area);
		}
	}
}





void do_diallist(CHAR_DATA *ch, char *argument)
{
	BUFFER *buffer = new_buf();
	AREA_DATA *area = ch->in_room->area;


	add_buf(buffer, "     [          Name          ]\n\r");
	add_buf(buffer, "================================\n\r");

	long count = 0;
	for(long vnum = area->bottom_dialogue_vnum; vnum <= area->top_dialogue_vnum; vnum++)
	{
		DIALOGUE_INDEX_DATA *dialogue = get_dialogue_index(area, vnum);
		if (!dialogue) continue;

		++count;
		add_buf(buffer, formatf("%4d  %s\n\r", vnum, dialogue->name));
	}

	if (count > 0)
	{
		add_buf(buffer, "--------------------------------\n\r");
		add_buf(buffer, formatf("%ld found.\n\r", count));
	}
	else
	{
		clear_buf(buffer);
		add_buf(buffer, "No dialogues to display.\n\r");
	}

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
}

static void __dialogue_report(CHAR_DATA *ch, DIALOGUE *dialogue)
{
	send_to_char("Dialogue done.\n\r", ch);

	// List all variables

	for(pVARIABLE var = dialogue->variables; var; var = var->next)
	{
		switch(var->type)
		{
			case VAR_BOOLEAN:
				send_to_char(formatf("{c[{CBOOLEAN{c] {W%-20.20s %s{x\n\r", var->name, (var->_.boolean ? "{WTrue" : "{DFalse")), ch);
				break;

			case VAR_INTEGER:
				send_to_char(formatf("{c[{CINTEGER{c] {W%-20.20s {G%d{x\n\r", var->name, var->_.i), ch);
				break;

			case VAR_STRING:
			case VAR_STRING_S:
				send_to_char(formatf("{c[{CSTRING{c]  {W%-20.20s {Y%s{x\n\r", var->name, var->_.s), ch);
				break;
		}
	}
}

// TODO: Remove this
void do_dialstart (CHAR_DATA *ch, char *argument)
{
}





////////////////////////
// Dialogue Edit
//

// create - create a new dialogue
// show - show the current dialogue
// node - command for handling nodes
//   add - add a new node for the given type
//   list - list all the nodes
//   delete - delete the given node
//   set - configure a given node
//   
//
// test - test the current dialogue with the given parameters
// name - set the name of the dialogue
// desc - set the description of the dialogue
// comments - set the builders' comments
// flags - toggle dialogue flags
// initialize - set/clear initialization mob script
// completed - set/clear completion mob script


DLGEDIT( dlgedit_create )
{
	AREA_DATA *area = ch->in_room->area;
	DIALOGUE_INDEX_DATA *diag;
	WNUM wnum;
	int iHash;

	if (argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
	{
		long last_vnum = 0;
		long value = area->top_dialogue_vnum + 1;
		for(last_vnum = 1; last_vnum <= area->top_dialogue_vnum; last_vnum++)
		{
			if( !get_dialogue_index(area, last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}

		wnum.pArea = area;
		wnum.vnum = value;
	}

	if( get_dialogue_index(wnum.pArea, wnum.vnum) )
	{
		send_to_char("That dialogue already exists.\n\r", ch);
		return false;
	}

    if (!IS_BUILDER(ch, wnum.pArea))
    {
		send_to_char("DlgEdit:  widevnum in an area you cannot build in.\n\r", ch);
		return false;
    }

	diag = new_dialogue_index_data();
	diag->area = wnum.pArea;
	diag->vnum = wnum.vnum;

	iHash = diag->vnum % MAX_KEY_HASH;
	diag->next = diag->area->dialogue_index_hash[iHash];
	diag->area->dialogue_index_hash[iHash] = diag;
	olc_set_editor(ch, ED_DLGEDIT, diag);

	diag->area->bottom_dialogue_vnum = UMIN(diag->area->bottom_dialogue_vnum, diag->vnum);
	diag->area->top_dialogue_vnum = UMAX(diag->area->top_dialogue_vnum, diag->vnum);

	send_to_char("Dialogue created.\n\r", ch);
	return true;
}

DLGEDIT( dlgedit_show )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	BUFFER *buffer = new_buf();

	// TODO: MAKE THIS TABBED?

	add_buf(buffer, formatf("Dialogue[%s %ld#%ld]: %s\n\r", diag->area->name, diag->area->uid, diag->vnum, diag->name));
	add_buf(buffer, formatf("Description:\n\r  %s\n\r", diag->description));

	if (!IS_NULLSTR(diag->comments))
		add_buf(buffer, formatf("Builders' Comments:\n\r  %s\n\r", diag->comments));

	// TODO: Flags
	
	if (diag->initialize)
		add_buf(buffer, formatf("Initialization Script: %s (%s %ld#%ld) %s\n\r", diag->initialize->name, diag->initialize->area->name, diag->initialize->area->uid, diag->initialize->vnum, olc_show_script_status(diag->initialize, PRG_MPROG)));
	else if (!IS_SET(ch->comm, COMM_BRIEF))
		add_buf(buffer, "Initialization Script: {R-not set-{x\n\r");
	
	if (diag->completed)
		add_buf(buffer, formatf("Completion Script: %s (%s %ld#%ld) %s\n\r", diag->completed->name, diag->completed->area->name, diag->completed->area->uid, diag->completed->vnum, olc_show_script_status(diag->completed, PRG_MPROG)));
	else if (!IS_SET(ch->comm, COMM_BRIEF))
		add_buf(buffer, "Completion Script: {R-not set-{x\n\r");

	if (list_size(diag->nodes) > 0)
		dlgedit_buffer_nodes(ch, buffer, diag);
	else if (!IS_SET(ch->comm, COMM_BRIEF))
		add_buf(buffer, "{DNo Nodes defined.{x\n\r");

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return false;
}

DLGEDIT( dlgedit_name )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(diag->name);
	diag->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return true;
}

DLGEDIT( dlgedit_description )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	if (argument[0] == '\0')
	{
		string_append(ch, &diag->description);
		return true;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return false;
}

DLGEDIT( dlgedit_comments )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	if (argument[0] == '\0')
	{
		string_append(ch, &diag->comments);
		return true;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return false;
}

void dlgedit_buffer_nodes(CHAR_DATA *ch, BUFFER *buffer, DIALOGUE_INDEX_DATA *diag)
{
	int index = 0;
	ITERATOR it;
	DIALOGUE_INDEX_NODE *node;

	iterator_start(&it, diag->nodes);
	while((node = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		add_buf(buffer, formatf("Node #%d:\n\r", ++index));
		add_buf(buffer, formatf("- Uid: %ld\n\r", node->uid));

		switch(node->type)
		{
			case DIALOGUE_TYPE_TEXT:
				add_buf(buffer, "- Type: {GTEXT{x\n\r");
				add_buf(buffer, formatf("  - Text: %s{x\n\r", node->text.src));
				if (node->text.area > 0)
					add_buf(buffer, formatf("  - Area: Slot #{G%d{x\n\r", node->text.area));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Area: {R-not set-{x\n\r");

				if (node->text.room > 0)
					add_buf(buffer, formatf("  - Room: Slot #{G%d{x\n\r", node->text.room));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Room: {R-not set-{x\n\r");

				if (node->text.object1 > 0)
					add_buf(buffer, formatf("  - Object 1: Slot #{G%d{x\n\r", node->text.object1));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Object 1: {R-not set-{x\n\r");

				if (node->text.object2 > 0)
					add_buf(buffer, formatf("  - Object 2: Slot #{G%d{x\n\r", node->text.object2));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Object 2: {R-not set-{x\n\r");

				if (node->text.victim1 > 0)
					add_buf(buffer, formatf("  - Victim 1: Slot #{G%d{x\n\r", node->text.victim1));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Victim 1: {R-not set-{x\n\r");

				if (node->text.victim2 > 0)
					add_buf(buffer, formatf("  - Victim 2: Slot #{G%d{x\n\r", node->text.victim2));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Victim 2: {R-not set-{x\n\r");
				break;

			case DIALOGUE_TYPE_SPEECH:
				add_buf(buffer, "- Type: {GSPEECH{x\n\r");
				add_buf(buffer, formatf("  - Text: %s{x\n\r", node->text.src));
				if (node->text.area > 0)
					add_buf(buffer, formatf("  - Area: Slot #{G%d{x\n\r", node->text.area));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Area: {R-not set-{x\n\r");

				if (node->text.room > 0)
					add_buf(buffer, formatf("  - Room: Slot #{G%d{x\n\r", node->text.room));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Room: {R-not set-{x\n\r");

				if (node->text.object1 > 0)
					add_buf(buffer, formatf("  - Object 1: Slot #{G%d{x\n\r", node->text.object1));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Object 1: {R-not set-{x\n\r");

				if (node->text.object2 > 0)
					add_buf(buffer, formatf("  - Object 2: Slot #{G%d{x\n\r", node->text.object2));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Object 2: {R-not set-{x\n\r");

				if (node->text.victim1 > 0)
					add_buf(buffer, formatf("  - Victim 1: Slot #{G%d{x\n\r", node->text.victim1));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Victim 1: {R-not set-{x\n\r");

				if (node->text.victim2 > 0)
					add_buf(buffer, formatf("  - Victim 2: Slot #{G%d{x\n\r", node->text.victim2));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Victim 2: {R-not set-{x\n\r");

				if (node->speaker > 0)
					add_buf(buffer, formatf("  - Speaker: Slot #{G%d{x\n\r", node->speaker));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Speaker: {R-not set-{x\n\r");
				break;

			case DIALOGUE_TYPE_SET:
				add_buf(buffer, "- Type: {GSET{x\n\r");
				add_buf(buffer, formatf("  - Variable: %s\n\r", node->variable));
				if (!IS_NULLSTR(node->value))
				{
					char op = node->value[0];
					char *vstr = node->value + 1;
					int value = atoi(vstr);
					switch(op)
					{
						case '=':
							add_buf(buffer, formatf("  - Value assigned {G%s{x\n\r", vstr));
							break;
						
						case '+':
							add_buf(buffer, formatf("  - Value incremented by {G%d{x ({W%s{x). [{MINTEGER{x only]\n\r", value, vstr));
							break;
						
						case '-':
							add_buf(buffer, formatf("  - Value decremented by {G%d{x ({W%s{x). [{MINTEGER{x only]\n\r", value, vstr));
							break;
						
						case '*':
							add_buf(buffer, formatf("  - Value multiplied by {G%d{x ({W%s{x). [{MINTEGER{x only]\n\r", value, vstr));
							break;
						
						case '/':
							if (value != 0)
								add_buf(buffer, formatf("  - Value divided by {G%d{x ({W%s{x). [{MINTEGER{x only]\n\r", value, vstr));
							else
								add_buf(buffer, formatf("  - Value divided by {RZERO{x ({W%s{x). [{MINTEGER{x only]\n\r", vstr));
							break;
						
						case '!':
							add_buf(buffer, "  - Value negated. [{MBOOLEAN{x only]\n\r");
							break;

						// TODO: Add bitwise operators?
						
						case ':':
							add_buf(buffer, formatf("  - Value assigned {G%s{x. [{MSTRING{x only]\n\r", vstr));
							break;
					}
				}
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Value: {R-not set-{x\n\r");
				break;

			case DIALOGUE_TYPE_TELEPORT:
				add_buf(buffer, "- Type: {GTELEPORT{x\n\r");
				if (node->destination > 0)
					add_buf(buffer, formatf("  - Destination: Room Slot #%d\n\r", node->destination));
				else if (!IS_SET(ch->comm, COMM_BRIEF))
					add_buf(buffer, "  - Destination: {R-not set-{x\n\r");
				break;

			case DIALOGUE_TYPE_BRANCH:
				add_buf(buffer, "- Type: {GBRANCH{x\n\r");
				if (list_size(node->options) > 0)
				{
					add_buf(buffer, "  - Branches:\n\r");
					int b = 0;
					ITERATOR oit;
					DIALOGUE_INDEX_BRANCH *branch;
					iterator_start(&oit, node->options);
					while((branch = (DIALOGUE_INDEX_BRANCH *)iterator_nextdata(&oit)))
					{
						add_buf(buffer, formatf("   %d) %s{x\n\r", ++b, branch->description));
						add_buf(buffer, formatf("    - Variable: {G%s{x\n\r", branch->variable));
						if (!IS_NULLSTR(branch->value))
						{
							char *vstr = branch->value + 1;
							int value = atoi(vstr);
							switch(branch->value[0])
							{
								case '=':
									add_buf(buffer, formatf("    - Value must be equal to {G%s{x.\n\r", vstr));
									break;

								case '!':
									add_buf(buffer, formatf("    - Value must not be equal to {G%s{x.\n\r", vstr));
									break;

								case '>':
									add_buf(buffer, formatf("    - Value must be greater than {G%d{x ({W%s{x). {W[{MINTEGER{W only]{x\n\r", value, vstr));
									break;

								case '<':
									add_buf(buffer, formatf("    - Value must be less than {G%d{x ({W%s{x). {W[{MINTEGER{W only]{x\n\r", value, vstr));
									break;

								case '*':
									add_buf(buffer, formatf("    - Value must start with {G%s{x. {W[{MSTRING{W only]{x\n\r", vstr));
									break;

								case '^':
									add_buf(buffer, formatf("    - Value must contain {G%s{x. {W[{MSTRING{W only]{x\n\r", vstr));
									break;
							}
						}
						else if (!IS_SET(ch->comm, COMM_BRIEF))
							add_buf(buffer, "    - Value: {R-not set-{x\n\r");

						if (IS_VALID(branch->child))
							add_buf(buffer, formatf("    - Child: Node #%d (%ld)\n\r", list_getindex(diag->nodes, branch->child), branch->child->uid));
						else if (!IS_SET(ch->comm, COMM_BRIEF))
							add_buf(buffer, "    - Child: {R-not set-{x\n\r");
					}
					iterator_stop(&oit);
				}
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "   {D-{x-{Wempty{x-{D-{x\n\r");
				break;

			case DIALOGUE_TYPE_CHOICE:
				add_buf(buffer, "- Type: {GCHOICE{x\n\r");
				if (list_size(node->options) > 0)
				{
					add_buf(buffer, "  - Choices:\n\r");
					int c = 0;
					ITERATOR oit;
					DIALOGUE_INDEX_CHOICE *choice;
					iterator_start(&oit, node->options);
					while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&oit)))
					{
						add_buf(buffer, formatf("   %d) %s{x\n\r", ++c, choice->text.src));
						if (choice->text.area > 0)
							add_buf(buffer, formatf("    - Area: Slot #{G%d{x\n\r", choice->text.area));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Area: {R-not set-{x\n\r");

						if (choice->text.room > 0)
							add_buf(buffer, formatf("    - Room: Slot #{G%d{x\n\r", choice->text.room));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Room: {R-not set-{x\n\r");

						if (choice->text.object1 > 0)
							add_buf(buffer, formatf("    - Object 1: Slot #{G%d{x\n\r", choice->text.object1));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Object 1: {R-not set-{x\n\r");

						if (choice->text.object2 > 0)
							add_buf(buffer, formatf("    - Object 2: Slot #{G%d{x\n\r", choice->text.object2));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Object 2: {R-not set-{x\n\r");

						if (choice->text.victim1 > 0)
							add_buf(buffer, formatf("    - Victim 1: Slot #{G%d{x\n\r", choice->text.victim1));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Victim 1: {R-not set-{x\n\r");

						if (choice->text.victim2 > 0)
							add_buf(buffer, formatf("    - Victim 2: Slot #{G%d{x\n\r", choice->text.victim2));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Victim 2: {R-not set-{x\n\r");

						if (!IS_NULLSTR(choice->hint))
							add_buf(buffer, formatf("    - Hint: %s{x\n\r", choice->hint));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Hint: {R-not set-{x\n\r");

						if (choice->visible)
							add_buf(buffer, formatf("    - Visibility: %s (%s - %ld#%ld) %s\n\r", choice->visible->name, choice->visible->area->name, choice->visible->area->uid, choice->visible->vnum, olc_show_script_status(choice->visible, PRG_MPROG)));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Visibility: {R-not set-{x\n\r");

						if (IS_VALID(choice->child))
							add_buf(buffer, formatf("    - Child: Node #%d (%ld)\n\r", list_getindex(diag->nodes, choice->child), choice->child->uid));
						else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
							add_buf(buffer, "    - Child: {R-not set-{x\n\r");

						add_buf(buffer, "\n\r");
					}
					iterator_stop(&oit);
				}
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "   {D-{x-{Wempty{x-{D-{x\n\r");
				break;

			case DIALOGUE_TYPE_SCRIPT:
				add_buf(buffer, "- Type: {GSCRIPT{x\n\r");
				if (node->script)
					add_buf(buffer, formatf("  - Mob Script: %s (%s - %ld#%ld) %s\n\r", node->script->name, node->script->area->name, node->script->area->uid, node->script->vnum, olc_show_script_status(node->script, PRG_MPROG)));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Mob Script: {R-not set-{x\n\r");
				break;

			case DIALOGUE_TYPE_FOR:
				add_buf(buffer, "- Type: {GFOR{x\n\r");
				if(IS_VALID(node->for_child))
					add_buf(buffer, formatf("  - Child: Node #%d (%ld)\n\r", list_getindex(diag->nodes, node->for_child), node->for_child->uid));
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "  - Child: {R-not set-{x\n\r");
				add_buf(buffer, formatf("  - Count: Number Slot #%d\n\r", node->for_total + 1));	// Slot is stored as 0-9.
				break;

			case DIALOGUE_TYPE_RANDOM:
				add_buf(buffer, "- Type: {GRANDOM{x\n\r");
				if (list_size(node->options) > 0)
				{
					add_buf(buffer, "  - Selection:\n\r");

					ITERATOR oit;
					DIALOGUE_INDEX_NODE *child;
					iterator_start(&oit, node->options);
					while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
					{
						add_buf(buffer, formatf("   - Node #%d (%ld)\n\r", list_getindex(diag->nodes, child), child->uid));
					}
					iterator_stop(&oit);
				}
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "   {D-{x-{Wempty{x-{D-{x\n\r");
				break;

			case DIALOGUE_TYPE_SEQUENCE:
				add_buf(buffer, "- Type: {GSEQUENCE{x\n\r");
				if (list_size(node->options) > 0)
				{
					add_buf(buffer, "  - Sequence:\n\r");
					int s = 0;
					ITERATOR oit;
					DIALOGUE_INDEX_NODE *child;
					iterator_start(&oit, node->options);
					while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
					{
						add_buf(buffer, formatf("   %d) Node #%d (%ld)\n\r", ++s, list_getindex(diag->nodes, child), child->uid));
					}
					iterator_stop(&oit);
				}
				else if (!IS_SET(ch->comm, COMM_BRIEF))	// Hide if BRIEF is turned on
					add_buf(buffer, "   {D-{x-{Wempty{x-{D-{x\n\r");
				break;
		}


	}
	iterator_stop(&it);
}

void dlgedit_unlink_node(DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	// Iterate over every node to unlink node references
	DIALOGUE_INDEX_NODE *n;
	ITERATOR it;

	iterator_start(&it, diag->nodes);
	while((n = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
	{
		// Unlink child/next node
		if (n->child == node)
			n->child = NULL;

		switch(n->type)
		{
		case DIALOGUE_TYPE_TEXT:
		case DIALOGUE_TYPE_SET:
		case DIALOGUE_TYPE_SCRIPT:
		case DIALOGUE_TYPE_SPEECH:
		case DIALOGUE_TYPE_TELEPORT:
			// Nothing to unlink
			break;

		case DIALOGUE_TYPE_CHOICE:
			{
				ITERATOR oit;
				DIALOGUE_INDEX_CHOICE *choice;

				iterator_start(&oit, n->options);
				while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&oit)))
				{
					if (choice->child == node)
						choice->child = NULL;
				}
				iterator_stop(&oit);
				break;
			}

		case DIALOGUE_TYPE_BRANCH:
			{
				ITERATOR oit;
				DIALOGUE_INDEX_BRANCH *branch;

				iterator_start(&oit, n->options);
				while((branch = (DIALOGUE_INDEX_BRANCH *)iterator_nextdata(&oit)))
				{
					if (branch->child == node)
						branch->child = NULL;
				}
				iterator_stop(&oit);

				break;
			}

		case DIALOGUE_TYPE_FOR:
			if (n->for_child == node)
				n->for_child = NULL;
			break;

		case DIALOGUE_TYPE_RANDOM:
		case DIALOGUE_TYPE_SEQUENCE:
			{
				ITERATOR oit;
				DIALOGUE_INDEX_NODE *child;

				iterator_start(&oit, n->options);
				while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&oit)))
				{
					if (child == node)
						iterator_remcurrent(&oit);
				}
				iterator_stop(&oit);

				break;
			}
		}
	}
	iterator_stop(&it);
}

bool dlgedit_node_text(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	int slot;

	argument = one_argument(argument, arg);
	smash_tilde(argument);

	if (!str_prefix(arg, "text"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> text <node text>\n\r", ch);
			return false;
		}

		free_string(node->text.src);
		node->text.src = str_dup(argument);
		send_to_char("Node text changed.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "area"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> area <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> area <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.area = slot;
		send_to_char("Node $(area) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "room"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.room = slot;
		send_to_char("Node $(room) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "obj1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.object1 = slot;
		send_to_char("Node $(object1) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "obj2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.object2 = slot;
		send_to_char("Node $(object2) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "vict1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.victim1 = slot;
		send_to_char("Node $(victim1) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "vict2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.victim2 = slot;
		send_to_char("Node $(victim2) slot changed\n\r", ch);
		return true;
	}

	send_to_char("Syntax:  node <#> text <node text>\n\r", ch);
	send_to_char("         node <#> area <slot #>\n\r", ch);
	send_to_char("         node <#> room <slot #>\n\r", ch);
	send_to_char("         node <#> obj1 <slot #>\n\r", ch);
	send_to_char("         node <#> obj2 <slot #>\n\r", ch);
	send_to_char("         node <#> vict1 <slot #>\n\r", ch);
	send_to_char("         node <#> vict2 <slot #>\n\r", ch);
	return false;
}

bool dlgedit_node_speech(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	int slot;

	argument = one_argument(argument, arg);
	smash_tilde(argument);

	if (!str_prefix(arg, "text"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> text <node text>\n\r", ch);
			return false;
		}

		free_string(node->text.src);
		node->text.src = str_dup(argument);
		free_string(node->text.text);
		node->text.text = compile_dialogue_text(node->text.src);
		send_to_char("Node text changed.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "speaker"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> speaker <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> speaker <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->speaker = slot;
		send_to_char("Node Speaker slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "area"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> area <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> area <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.area = slot;
		send_to_char("Node $(area) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "room"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.room = slot;
		send_to_char("Node $(room) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "obj1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.object1 = slot;
		send_to_char("Node $(object1) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "obj2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.object2 = slot;
		send_to_char("Node $(object2) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "vict1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict1 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.victim1 = slot;
		send_to_char("Node $(victim1) slot changed\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "vict2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict2 <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.victim2 = slot;
		send_to_char("Node $(victim2) slot changed\n\r", ch);
		return true;
	}

	send_to_char("Syntax:  node <#> text <node text>\n\r", ch);
	send_to_char("         node <#> area <slot #>\n\r", ch);
	send_to_char("         node <#> room <slot #>\n\r", ch);
	send_to_char("         node <#> obj1 <slot #>\n\r", ch);
	send_to_char("         node <#> obj2 <slot #>\n\r", ch);
	send_to_char("         node <#> vict1 <slot #>\n\r", ch);
	send_to_char("         node <#> vict2 <slot #>\n\r", ch);
	return false;
}

bool dlgedit_node_set(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	char arg2[MIL];
	char value[MIL * 2];
	char buf[MSL];

	smash_tilde(argument);
	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (arg2[0] != '!' && argument[0] == '\0')
	{
		send_to_char("Syntax:  node <#> <variable> <op>[ <value>]\n\r", ch);
		send_to_char("Please specify a value.\n\r", ch);
		return false;
	}

	if (arg2[0] == '!' && argument[0] != '\0' )
	{
		send_to_char("Syntax:  node <#> <variable> <op>\n\r", ch);
		send_to_char("Please omit a value when using the negation operator.\n\r", ch);
		return false;
	}


	if (!char_in_str("=+-*/!:", arg2[0]))
	{
		send_to_char("Invalid operator.  Select from one of the following:\n\r", ch);
		send_to_char("{Y={x - {GASSIGN{x - Assign value to variable. ({WINTEGER{x and {WBOOLEAN{x)\n\r", ch);
		send_to_char("{Y+{x - {GADD   {x - Adds value to variable. ({WINTEGER{x)\n\r", ch);
		send_to_char("{Y-{x - {GSUBT  {x - Subtracts value from variable. ({WINTEGER{x)\n\r", ch);
		send_to_char("{Y*{x - {GMULT  {x - Multiplies variable by value. ({WINTEGER{x)\n\r", ch);
		send_to_char("{Y/{x - {GDIV   {x - Divides variable by value. ({WINTEGER{x)\n\r", ch);
		send_to_char("{Y!{x - {GNEG   {x - Negates variable. ({WBOOLEAN{x)\n\r", ch);
		send_to_char("{Y:{x - {GSTRING{x - Assign value as string to variable. ({WSTRING{x)\n\r", ch);
		return false;
	}

	value[0] = arg2[0];
	strncpy(value + 1, argument, sizeof(value) - 2);

	free_string(node->variable);
	node->variable = str_dup(arg);

	free_string(node->value);
	node->value = str_dup(value);

	sprintf(buf, "Node variable {G%s {W%c {Y%s{x\n\r", arg, value[0], value + 1);
	send_to_char(buf, ch);
	return true;
}

bool dlgedit_node_script(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	WNUM wnum;

	if (!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
	{
		send_to_char("Syntax:  node <#> <widevnum>\n\r", ch);
		return false;
	}

	SCRIPT_DATA *script = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG);
	if (!script)
	{
		send_to_char("No such mob script with that widevnum.\n\r", ch);
		return false;
	}

	node->script = script;
	send_to_char("Node Script changed.\n\r", ch);
	return true;
}

bool dlgedit_node_teleport(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	int slot;

	argument = one_argument(argument, arg);
	smash_tilde(argument);

	if (!str_prefix(arg, "room"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
			send_to_char("Please provide a positive number or {Ynone{x.\n\r", ch);
			return false;
		}

		node->text.room = slot;
		send_to_char("Node $(room) slot changed\n\r", ch);
		return true;
	}

	send_to_char("Syntax:  node <#> room <slot #>\n\r", ch);
	return false;
}

bool dlgedit_node_choice(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	char buf[MSL];

	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "list"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No choices defined.\n\r", ch);
			return false;
		}

		// Iterate over the options
		ITERATOR it;
		DIALOGUE_INDEX_CHOICE *choice;
		BUFFER *buffer = new_buf();
		int i = 0;

		iterator_start(&it, node->options);
		while((choice = (DIALOGUE_INDEX_CHOICE *)iterator_nextdata(&it)))
		{
			sprintf(buf, "Choice %d:\n\r", ++i);
			add_buf(buffer, buf);

			add_buf(buffer, " - Text: ");
			add_buf(buffer, node->text.src);
			add_buf(buffer, "{x\n\r");

			if (node->text.area > 0)
			{
				sprintf(buf, "   - Area:    {Y#%d{x\n\r", node->text.area);
				add_buf(buffer, buf);
			}

			if (node->text.room > 0)
			{
				sprintf(buf, "   - Room:    {Y#%d{x\n\r", node->text.room);
				add_buf(buffer, buf);
			}

			if (node->text.object1 > 0)
			{
				sprintf(buf, "   - Object1: {Y#%d{x\n\r", node->text.object1);
				add_buf(buffer, buf);
			}

			if (node->text.object2 > 0)
			{
				sprintf(buf, "   - Object2: {Y#%d{x\n\r", node->text.object2);
				add_buf(buffer, buf);
			}

			if (node->text.victim1 > 0)
			{
				sprintf(buf, "   - Victim1: {Y#%d{x\n\r", node->text.victim1);
				add_buf(buffer, buf);
			}

			if (node->text.victim2 > 0)
			{
				sprintf(buf, "   - Victim2: {Y#%d{x\n\r", node->text.victim2);
				add_buf(buffer, buf);
			}

			add_buf(buffer, " - Visibility: ");
			if (choice->visible)
			{
				sprintf(buf, "{Y%s {W({Y%s{W[{Y%ld{W]#{Y%ld{W)", choice->visible->name, choice->visible->area->name, choice->visible->area->uid, choice->visible->vnum);
				add_buf(buffer, buf);
			}
			else
				add_buf(buffer, "{Rnone");
			add_buf(buffer, "{x\n\r");

			add_buf(buffer, " - Hint: ");
			if (IS_NULLSTR(choice->hint))
				add_buf(buffer, "{Rnot set");
			else
			{
				add_buf(buffer, "{G");
				add_buf(buffer, choice->hint);
			}
			add_buf(buffer, "{x\n\r");

			add_buf(buffer, " - Child: ");
			if (IS_VALID(choice->child))
			{
				sprintf(buf, "{GNode #%ld", choice->child->uid);
				add_buf(buffer, buf);
			}
			else
				add_buf(buffer, "{Ynone");
			add_buf(buffer, "{x\n\r");
		}
		iterator_stop(&it);

		if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
		{
			send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
		}
		else
		{
			page_to_char(buffer->string, ch);
		}

		free_buf(buffer);
		return false;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("There are no options for this node.\n\r", ch);
			return false;
		}

		list_clear(node->options);
		send_to_char("Options cleared.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "add"))
	{
		long uid;

		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> add <uid>\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = new_dialogue_index_choice();
		choice->child = child;
		list_appendlink(node->options, choice);

		sprintf(buf, "Option #%d added.\n\r", list_size(node->options));
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "delete"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No options to delete.\n\r", ch);
			return false;
		}

		int index;
		if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> delete <#>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		list_remnthlink(node->options, index, true);
		sprintf(buf, "Option #%d removed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "child"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> child <#> <uid>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> child <#> <uid>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		long uid;

		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> child <#> <uid>\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		choice->child = child;
		sprintf(buf, "Option #%d linked to Node #%ld.\n\r", index, child->uid);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "hint"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> hint <#> <text>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> hint <#> <text>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> hint <#> <text>\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		free_string(choice->hint);
		choice->hint = str_dup(argument);

		sprintf(buf, "Option #%d hint changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "text"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> text <#> <node text>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> text <#> <node text>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> text <#> <node text>\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		free_string(choice->text.src);
		choice->text.src = str_dup(argument);
		free_string(choice->text.text);
		choice->text.text = compile_dialogue_text(choice->text.src);

		sprintf(buf, "Option #%d text changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "area"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> area <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> area <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> area <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.area = slot;
		sprintf(buf, "Option #%d area changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "room"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> room <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> room <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> room <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.room = slot;
		sprintf(buf, "Option #%d room changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "obj1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj1 <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> obj1 <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj1 <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.object1 = slot;
		sprintf(buf, "Option #%d object1 changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "obj2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> obj2 <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> obj2 <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> obj2 <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.object2 = slot;
		sprintf(buf, "Option #%d object2 changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "vict1"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict1 <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> vict1 <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict1 <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.victim1 = slot;
		sprintf(buf, "Option #%d victim1 changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "vict2"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> vict2 <#> <slot #>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);
		smash_tilde(argument);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> vict2 <#> <slot #>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		int slot;
		if (!str_prefix(argument, "none"))
			slot = 0;
		else if (!is_number(argument) || (slot = atoi(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> vict2 <#> <slot #>\n\r", ch);
			send_to_char("Please specify a positive number or {Ynone{x\n\r", ch);
			return false;
		}

		choice->text.victim2 = slot;
		sprintf(buf, "Option #%d victim2 changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "visible"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> visible <#> <widevnum|none>\n\r", ch);
			return false;
		}

		if (list_size(node->options) < 1)
		{
			send_to_char("No options to edit.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> visible <#> <widevnum|none>\n\r", ch);
			sprintf(buf, "Please specify number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_CHOICE *choice = (DIALOGUE_INDEX_CHOICE *)list_nthdata(node->options, index);
		if (!choice)
		{
			send_to_char("Could not find that choice.\n\r", ch);
			return false;
		}

		SCRIPT_DATA *script;
		WNUM wnum;
		if (!str_prefix(argument, "none"))
			script = NULL;
		else if (!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
		{
			send_to_char("Syntax:  node <#> visible <#> <widevnum|none>\n\r", ch);
			return false;
		}
		else
		{
			script = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG);
			if (!script)
			{
				send_to_char("No such mob script with that widevnum.\n\r", ch);
				return false;
			}
		}

		choice->visible = script;
		sprintf(buf, "Option #%d visibility script changed.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	send_to_char("Syntax:  node <#> list\n\r", ch);
	send_to_char("         node <#> clear\n\r", ch);
	send_to_char("         node <#> add <uid>\n\r", ch);
	send_to_char("         node <#> delete <#>\n\r", ch);
	send_to_char("         node <#> child <#> <uid>\n\r", ch);
	send_to_char("         node <#> text <#> <node text>\n\r", ch);
	send_to_char("         node <#> area <#> <slot #>\n\r", ch);
	send_to_char("         node <#> room <#> <slot #>\n\r", ch);
	send_to_char("         node <#> obj1 <#> <slot #>\n\r", ch);
	send_to_char("         node <#> obj2 <#> <slot #>\n\r", ch);
	send_to_char("         node <#> vict1 <#> <slot #>\n\r", ch);
	send_to_char("         node <#> vict2 <#> <slot #>\n\r", ch);
	send_to_char("         node <#> hint <#> <hint>\n\r", ch);
	send_to_char("         node <#> visible <#> <widevnum|none>\n\r", ch);
	return false;
}

bool dlgedit_node_branch(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	char buf[MSL];

	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "list"))
	{
		if(list_size(node->options) < 1)
		{
			send_to_char("Node has no branch options.\n\r", ch);
			return false;
		}

		ITERATOR it;
		DIALOGUE_INDEX_BRANCH *branch;
		int index = 0;

		BUFFER *buffer = new_buf();

		iterator_start(&it, node->options);
		while((branch = (DIALOGUE_INDEX_BRANCH *)iterator_nextdata(&it)))
		{
			sprintf(buf, "Branch: %d\n\r", ++index);
			add_buf(buffer, buf);

			sprintf(buf, " - Description: %s\n\r", branch->description);
			add_buf(buffer, buf);

			if (IS_NULLSTR(branch->variable))
				strcpy(buf, " - Variable: {R-not set-{x\n\r");
			else
				sprintf(buf, " - Variable: {G%s{x\n\r", branch->variable);
			add_buf(buffer, buf);

			if(branch->value[0] != '\0')
			{
				switch(branch->value[0])
				{
					case '=':
						sprintf(buf, " - Value: must be equal to {G%s{x.\n\r", branch->value + 1);
						break;
					case '!':
						sprintf(buf, " - Value: must not be equal to {G%s{x ({WINTEGER{x only).\n\r", branch->value + 1);
						break;
					case '>':
						sprintf(buf, " - Value: must be greater than {G%s{x ({WINTEGER{x only).\n\r", branch->value + 1);
						break;
					case '<':
						sprintf(buf, " - Value: must be less than {G%s{x ({WINTEGER{x only).\n\r", branch->value + 1);
						break;
					case '*':
						sprintf(buf, " - Value: must start with {G%s{x ({WSTRING{x only).\n\r", branch->value + 1);
						break;
					case '^':
						sprintf(buf, " - Value: contain {G%s{x ({WSTRING{x only).\n\r", branch->value + 1);
						break;
					default:
						sprintf(buf, " - Value: {R%s{x is invalid.\n\r", branch->value);
						break;
				}
			}
			else
				sprintf(buf, " - Value: {R-not set-{x\n\r");
			add_buf(buffer, buf);
		}
		iterator_stop(&it);

		if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
		{
			send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
		}
		else
		{
			page_to_char(buffer->string, ch);
		}

		free_buf(buffer);
		return false;
	}

	if (!str_prefix(arg, "clear"))
	{
		if(list_size(node->options) < 1)
		{
			send_to_char("No branches to clear.\n\r", ch);
			return false;
		}

		list_clear(node->options);
		send_to_char("Branches cleared.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "add"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> add <variable> <op> <value> <uid>\n\r", ch);
			return false;
		}

		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];

		argument = one_argument(argument, arg2);
		argument = one_argument(argument, arg3);
		argument = one_argument(argument, arg4);

		char value[MSL];
		if (!char_in_str("=><*^", arg3[0]))
		{
			send_to_char("Invalid operator.  Please use one of the following:\n\r", ch);
			send_to_char("{Y={x - Variable must be equal value.\n\r", ch);
			send_to_char("{Y>{x - Variable must be greater than value.  Integer variables only.\n\r", ch);
			send_to_char("{Y<{x - Variable must be less than value.  Integer variables only.\n\r", ch);
			send_to_char("{Y!{x - Variable must not be equal value.  Integer variables only.\n\r", ch);
			send_to_char("{Y*{x - Variable must start with value.  String variables only.\n\r", ch);
			send_to_char("{Y^{x - Variable must contain value.  String variables only.\n\r", ch);
			return false;	
		}

		if (arg3[0] == '\0')
		{
			send_to_char("Syntax:  node <#> add <variable> <op> <value> <uid>\n\r", ch);
			send_to_char("Please specify a value.\n\r", ch);
			return false;
		}
		value[0] = arg2[0];
		strcpy(value + 1, arg3);

		long uid;
		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> add <variable> <op> <value> <uid>\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *node = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(node))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_BRANCH *branch = new_dialogue_index_branch();
		branch->description = str_dup("");
		branch->variable = str_dup(arg);
		branch->value = str_dup(value);
		branch->child = node;

		list_appendlink(node->options, branch);
		sprintf(buf, "Branch %d added.\n\r", list_size(node->options));
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "desc"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No node has no branches.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}
		
		if (argument[0] != '\0')
		{
			send_to_char("Syntax:  node <#> desc <#> (opens editor)\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_BRANCH *branch = list_nthdata(node->options, index);
		if (!branch)
		{
			send_to_char("Invalid branch data.  Please delete it.\n\r", ch);
			return false;
		}

		string_append(ch, &branch->description);
		return true;
	}


	if (!str_prefix(arg, "delete"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No node has no branches.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		list_remnthlink(node->options, index, true);
		sprintf(buf, "Branch #%d deleted.\n\r", index);
		send_to_char(buf, ch);		
		return true;
	}

	if (!str_prefix(arg, "variable"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No node has no branches.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}
		
		DIALOGUE_INDEX_BRANCH *branch = list_nthdata(node->options, index);
		if (!branch)
		{
			send_to_char("Invalid branch data.  Please delete it.\n\r", ch);
			return false;
		}

		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> variable <#> <name>\n\r", ch);
			return false;
		}

		smash_tilde(argument);
		free_string(branch->variable);
		branch->variable = str_dup(argument);
		sprintf(buf, "Branch #%d variable name changed to {G%s{x.\n\r", index, argument);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "value"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No node has no branches.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_BRANCH *branch = list_nthdata(node->options, index);
		if (!branch)
		{
			send_to_char("Invalid branch data.  Please delete it.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		char value[MSL];
		if (!char_in_str("=><*^", arg[0]))
		{
			send_to_char("Invalid operator.  Please use one of the following:\n\r", ch);
			send_to_char("{Y={x - Variable must be equal value.\n\r", ch);
			send_to_char("{Y>{x - Variable must be greater than value.  Integer variables only.\n\r", ch);
			send_to_char("{Y<{x - Variable must be less than value.  Integer variables only.\n\r", ch);
			send_to_char("{Y!{x - Variable must not be equal value.  Integer variables only.\n\r", ch);
			send_to_char("{Y*{x - Variable must start with value.  String variables only.\n\r", ch);
			send_to_char("{Y^{x - Variable must contain value.  String variables only.\n\r", ch);
			return false;	
		}

		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> value <#> <op> <value>\n\r", ch);
			return false;
		}

		smash_tilde(argument);
		value[0] = arg[0];
		strcpy(value + 1, argument);

		free_string(branch->value);
		branch->value = str_dup(value);
		send_to_char(formatf("Branch #%d value expression changed to {G%s{x.\n\r", index, value), ch);
		return true;
	}

	if (!str_prefix(arg, "next"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("No node has no branches.\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_BRANCH *branch = list_nthdata(node->options, index);
		if (!branch)
		{
			send_to_char("Invalid branch data.  Please delete it.\n\r", ch);
			return false;
		}

		long uid;
		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if(!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		branch->child = child;
		sprintf(buf, "Branch #%d next node changed to Node #%d.\n\r", index, list_getindex(diag->nodes, child));
		send_to_char(buf, ch);
		return true;
	}

	send_to_char("Syntax:  node <#> list\n\r", ch);
	send_to_char("         node <#> clear\n\r", ch);
	send_to_char("         node <#> add <variable> <op> <value> <uid>\n\r", ch);
	send_to_char("         node <#> desc <#> (opens editor)\n\r", ch);
	send_to_char("         node <#> delete <#>\n\r", ch);
	send_to_char("         node <#> variable <#> <name>\n\r", ch);
	send_to_char("         node <#> value <#> <op> <value>\n\r", ch);
	send_to_char("         node <#> next <#> <uid>\n\r", ch);
	return false;
}

bool dlgedit_node_for(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	char buf[MSL];

	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "count"))
	{
		int slot;
		if (!is_number(argument) || (slot = atoi(argument)) < 1 || slot > 10)
		{
			send_to_char("Please specify a number from 1 to 10.\n\r", ch);
			return false;
		}
		node->for_total = slot - 1;		// really should be 0 to 9.
		sprintf(buf, "For loop max count number slot changed to {G%d{x.\n\r", slot);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "child"))
	{
		long uid;
		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		node->for_child = child;
		sprintf(buf, "For loop child node set to Node #%d (%ld).\n\r", list_getindex(diag->nodes, child), uid);
		send_to_char(buf, ch);
		return true;
	}

	send_to_char("Syntax:  node <#> count <number slot #>\n\r", ch);
	send_to_char("         node <#> child <uid>\n\r", ch);
	return false;
}

// SEQUENCE and RANDOM nodes have the same data, just used differently
bool dlgedit_node_listtype(CHAR_DATA *ch, char *argument, DIALOGUE_INDEX_DATA *diag, DIALOGUE_INDEX_NODE *node)
{
	char arg[MIL];
	char buf[MSL];

	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "list"))
	{
		if(list_size(node->options) < 1)
		{
			send_to_char("Node has no options.\n\r", ch);
			return false;
		}

		ITERATOR it;
		DIALOGUE_INDEX_NODE *child;
		int index = 0;

		BUFFER *buffer = new_buf();

		iterator_start(&it, node->options);
		while((child = (DIALOGUE_INDEX_NODE *)iterator_nextdata(&it)))
		{
			sprintf(buf, "%2d) Node #%d (%ld)\n\r", ++index, list_getindex(diag->nodes, child), child->uid);
			add_buf(buffer, buf);
		}
		iterator_stop(&it);

		if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
		{
			send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
		}
		else
		{
			page_to_char(buffer->string, ch);
		}

		free_buf(buffer);
		return false;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("Node has no child nodes.\n\r", ch);
			return false;
		}

		list_clear(node->options);
		send_to_char("Child nodes cleared.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "delete"))
	{
		if (list_size(node->options) < 1)
		{
			send_to_char("Node has no child nodes.\n\r", ch);
			return false;
		}

		int index;
		if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> delete <#>\n\r", ch);
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		list_remnthlink(node->options, index, false);
		sprintf(buf, "Child #%d deleted.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "add"))
	{
		long uid;
		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> add <uid>\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		list_appendlink(node->options, child);
		sprintf(buf, "Child #%d added.\n\r", list_size(node->options));
		send_to_char(buf, ch);
		return true;
	}

	if (!str_prefix(arg, "insert"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  node <#> insert <#> <uid>\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg);

		int index;
		if (!is_number(arg) || (index = atoi(arg)) < 1 || index > list_size(node->options))
		{
			send_to_char("Syntax:  node <#> insert <#> <uid>\n\r", ch);
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(node->options));
			send_to_char(buf, ch);
			return false;
		}

		long uid;
		if (!is_number(argument) || (uid = atol(argument)) < 1)
		{
			send_to_char("Syntax:  node <#> insert <#> <uid>\n\r", ch);
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *child = get_dialogue_index_node(diag, uid);
		if (!IS_VALID(child))
		{
			send_to_char("No such node with that uid.\n\r", ch);
			return false;
		}

		list_insertlink(node->options, child, index);
		sprintf(buf, "Child #%d inserted.\n\r", index);
		send_to_char(buf, ch);
		return true;
	}

	send_to_char("Syntax:  node <#> list\n\r", ch);
	send_to_char("         node <#> clear\n\r", ch);
	send_to_char("         node <#> add <uid>\n\r", ch);
	send_to_char("         node <#> insert <#> <uid>\n\r", ch);
	send_to_char("         node <#> delete <#>\n\r", ch);
	return false;
}

DLGEDIT( dlgedit_node )
{
	DIALOGUE_INDEX_DATA *diag;
	char arg[MIL];
	char buf[MSL];

	EDIT_DIALOGUE(ch, diag);

	if( argument[0] == '\0' )
	{
		// TODO: Convert to a BUFFER as this will be long
		// When handling the type is known, give shortened list of commands
		send_to_char("Syntax:  node list\n\r", ch);
		send_to_char("         node add <type>\n\r", ch);
		send_to_char("         node <#> delay <delay>\n\r", ch);
		send_to_char("         node <#> delete\n\r", ch);
		send_to_char("         node <#> <subcommand>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		if (list_size(diag->nodes) < 1)
		{
			send_to_char("No nodes to list.\n\r", ch);
			return false;
		}

		BUFFER *buffer = new_buf();

		dlgedit_buffer_nodes(ch, buffer, diag);

		if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
		{
			send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
		}
		else
		{
			page_to_char(buffer->string, ch);
		}
		free_buf(buffer);
		return false;
	}

	if (!str_prefix(arg, "add"))
	{
		long value;

		if( (value = flag_value(dialogue_node_types, argument)) != NO_FLAG )
		{
			DIALOGUE_INDEX_NODE *node = new_dialogue_index_node((int16_t)value);

			node->parent = diag;
			node->uid = ++diag->top_node_uid;

			list_appendlink(diag->nodes, node);

			sprintf(buf, "Node %d created.\n\r", list_size(diag->nodes));
			send_to_char(buf, ch);
			return true;
		}

		send_to_char("Invalid node type.  Use '? node_types' for list of valid node types.\n\r", ch);
		show_flag_cmds(ch, dialogue_node_types);
		return false;
	}
	if (is_number(arg))
	{
		if (list_size(diag->nodes) < 1)
		{
			send_to_char("No nodes to configure.\n\r", ch);
			return false;
		}

		long nth;

		if ((nth = atol(arg)) <= 0 || nth > list_size(diag->nodes))
		{
			send_to_char("Syntax:  node <#> ...\n\r", ch);
			sprintf(buf, "Please specify a number from 1 to %d.\n\r", list_size(diag->nodes));
			send_to_char(buf, ch);
			return false;
		}

		DIALOGUE_INDEX_NODE *node = list_nthdata(diag->nodes, nth);
		char arg2[MIL];

		argument = one_argument(argument, arg2);

		if (!str_prefix(arg2, "delay"))
		{
			int delay;
			if (!str_prefix(argument, "none"))
				delay = 0;
			else if (!is_number(argument) || (delay = atoi(argument)) < 1)
			{
				send_to_char("Invalid delay.\n\r", ch);
				send_to_char("Please specify {Ynone{x or a positive number.\n\r", ch);
				return false;
			}
			
			node->delay = delay;
			if (delay > 0)
				sprintf(buf, "Node %ld's delay set to %d.\n\r", nth, delay);
			else
				sprintf(buf, "Node %ld's delay set to {Ynone{x.\n\r", nth);
			send_to_char(buf, ch);
			return true;
		}

		if (!str_prefix(arg2, "delete"))
		{
			dlgedit_unlink_node(diag, node);
			list_remlink(diag->nodes, node, true);

			sprintf(buf, "Node %ld deleted.\n\r", nth);
			send_to_char(buf, ch);
			return true;
		}

		switch(node->type)
		{
			case DIALOGUE_TYPE_TEXT:
				return dlgedit_node_text(ch, argument, diag, node);

			case DIALOGUE_TYPE_SET:
				return dlgedit_node_set(ch, argument, diag, node);

			case DIALOGUE_TYPE_SCRIPT:
				return dlgedit_node_script(ch, argument, diag, node);

			case DIALOGUE_TYPE_SPEECH:
				return dlgedit_node_speech(ch, argument, diag, node);

			case DIALOGUE_TYPE_TELEPORT:
				return dlgedit_node_teleport(ch, argument, diag, node);

			case DIALOGUE_TYPE_CHOICE:
				return dlgedit_node_choice(ch, argument, diag, node);

			case DIALOGUE_TYPE_BRANCH:
				return dlgedit_node_branch(ch, argument, diag, node);

			case DIALOGUE_TYPE_FOR:
				return dlgedit_node_for(ch, argument, diag, node);

			case DIALOGUE_TYPE_RANDOM:
			case DIALOGUE_TYPE_SEQUENCE:
				return dlgedit_node_listtype(ch, argument, diag, node);

			default:
				send_to_char("Invalid node type.  Please delete.\n\r", ch);
				break;
		}

		return false;
	}

	dlgedit_node(ch, "");
	return false;
}

// WIP: Does nothing yet as there are no flags as of yet.
DLGEDIT( dlgedit_flags )
{
	send_to_char("No flags currently\n\r", ch);
	return false;
}

static char *__parse_dialogue_params(CHAR_DATA *ch, DIALOGUE *dialogue, char *argument)
{
	char arg[MIL];

	while(argument[0] != '\0')
	{
		argument = one_argument(argument, arg);

		if (!str_prefix(arg, "area"))
		{
			argument = one_argument(argument, arg);

			AREA_DATA *area = find_area(arg);
			if (area == NULL)
			{
				send_to_char(formatf("No such area with the name {R%s{x.\n\r", arg), ch);
				return NULL;
			}

			list_appendlink(dialogue->areas, area);
		}
		else if (!str_prefix(arg, "room"))
		{
			argument = one_argument(argument, arg);
			WNUM wnum;
			
			if (!parse_widevnum(arg, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
			{
				send_to_char("Invalid widevnum for room.\n\r", ch);
				return NULL;
			}

			ROOM_INDEX_DATA *room = get_room_index(wnum.pArea, wnum.vnum);
			if (!room)
			{
				send_to_char(formatf("No such room at widevnum {R%ld#%ld{x.\n\r", wnum.pArea->uid, wnum.vnum), ch);
				return NULL;
			}

			list_appendlink(dialogue->rooms, room);
		}
		else if (!str_prefix(arg, "object"))
		{
			argument = one_argument(argument, arg);
			WNUM wnum;
			
			if (!parse_widevnum(arg, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
			{
				send_to_char("Invalid widevnum for object.\n\r", ch);
				return NULL;
			}

			OBJ_INDEX_DATA *obj = get_obj_index(wnum.pArea, wnum.vnum);
			if (!obj)
			{
				send_to_char(formatf("No such object at widevnum {R%ld#%ld{x.\n\r", wnum.pArea->uid, wnum.vnum), ch);
				return NULL;
			}

			list_appendlink(dialogue->objects, obj);
		}
		else if (!str_prefix(arg, "mobile"))
		{
			argument = one_argument(argument, arg);
			WNUM wnum;
			
			if (!parse_widevnum(arg, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
			{
				send_to_char("Invalid widevnum for mobile.\n\r", ch);
				return NULL;
			}

			MOB_INDEX_DATA *mob = get_mob_index(wnum.pArea, wnum.vnum);
			if (!mob)
			{
				send_to_char(formatf("No such mobile at widevnum {R%ld#%ld{x.\n\r", wnum.pArea->uid, wnum.vnum), ch);
				return NULL;
			}

			list_appendlink(dialogue->mobiles, mob);
		}
		else if (is_number(arg))
		{
			int slot = atoi(arg);
			if (slot < 1 || slot > 10)
			{
				send_to_char("Number slot must be from 1 to 10.\n\r", ch);
				return NULL;
			}

			argument = one_argument(argument, arg);
			if(!is_number(arg))
			{
				send_to_char(formatf("Please specify a number for slot {G%d{x.\n\r", slot), ch);
				return NULL;
			}

			dialogue->numbers[slot - 1] = atoi(arg);
		}
	}

	return argument;
}

DLGEDIT( dlgedit_test ) 
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	DIALOGUE *dialogue = clone_dialogue(diag, ch);
	if (!IS_VALID(dialogue))
	{
		send_to_char("Failed to clone dialogue.\n\r", ch);
		return false;
	}

	argument = __parse_dialogue_params(ch, dialogue, argument);
	if (argument == NULL)
	{
		return false;
	}

	// Hopefully this will work properly while editting OLC.
	start_dialogue(ch, dialogue, __dialogue_report);

	return false;
}

DLGEDIT( dlgedit_initialize )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	SCRIPT_DATA *script;

	if (!str_prefix(argument, "none"))
	{
		script = NULL;
	}
	else
	{
		WNUM wnum;
		if(!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
		{
			send_to_char("Syntax:  initalize <mob script widevnum|none>\n\r", ch);
			return false;
		}

		script = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG);
		if (!script)
		{
			send_to_char("Syntax:  initalize <mob script widevnum|none>\n\r", ch);
			send_to_char("No such mob script with that widevnum.\n\r", ch);
			return false;
		}
	}

	diag->initialize = script;
	send_to_char("Initialize Script changed.\n\r", ch);
	return true;
}

DLGEDIT( dlgedit_complete )
{
	DIALOGUE_INDEX_DATA *diag;

	EDIT_DIALOGUE(ch, diag);

	SCRIPT_DATA *script;

	if (!str_prefix(argument, "none"))
	{
		script = NULL;
	}
	else
	{
		WNUM wnum;
		if(!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
		{
			send_to_char("Syntax:  complete <mob script widevnum|none>\n\r", ch);
			return false;
		}

		script = get_script_index(wnum.pArea, wnum.vnum, PRG_MPROG);
		if (!script)
		{
			send_to_char("Syntax:  complete <mob script widevnum|none>\n\r", ch);
			send_to_char("No such mob script with that widevnum.\n\r", ch);
			return false;
		}
	}

	diag->completed = script;
	send_to_char("Completion Script changed.\n\r", ch);
	return true;
}

