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

long *new_long();
void free_long(long *l);
void delete_long(void *ptr);
WNUM_LOAD *new_list_wnum_load();
void free_list_wnum_load(WNUM_LOAD *wnum);
void delete_list_wnum_load(void *ptr);

////////////////////////////////////////
//                                    //
//           Dialogue Trees           //
//                                    //
////////////////////////////////////////

#define DIALOGUE_TYPE_TEXT			0		// Displays a message
#define DIALOGUE_TYPE_SET			1		// Sets a variable
#define DIALOGUE_TYPE_CHOICE		2		// Presents a choice to the player
#define DIALOGUE_TYPE_BRANCH		3		// Takes a branch based upon variables set
#define DIALOGUE_TYPE_SCRIPT		4		// Execute mobile script

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

const struct flag_type dialogue_node_types[] =
{
	{ "text",		DIALOGUE_TYPE_TEXT,		true	},
	{ "set",		DIALOGUE_TYPE_SET,		true	},
	{ "choice",		DIALOGUE_TYPE_CHOICE,	true	},
	{ "branch",		DIALOGUE_TYPE_BRANCH,	true	},
	{ "script",		DIALOGUE_TYPE_SCRIPT,	true	},
	{ NULL,			-1,						false	}
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
	data->rooms = list_create(false);
	
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
	data->rooms = list_create(false);

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

		if (node->child)
			new_node->child = get_dialogue_node(dialogue, node->child->uid);
		else
			new_node->child = NULL;
		
		new_node->script = node->script;
		new_node->variable = str_dup(node->variable);
		new_node->value = str_dup(node->value);
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

void show_node_text(CHAR_DATA *ch, DIALOGUE *dialogue, NODE_TEXT *nt, char *command, char *hint)
{
	if (IS_NULLSTR(nt->text)) return;

	BUFFER *buffer = new_buf();

	MOB_INDEX_DATA *v1 = get_dialogue_mobile(dialogue, nt->victim1);
	MOB_INDEX_DATA *v2 = get_dialogue_mobile(dialogue, nt->victim2);
	OBJ_INDEX_DATA *o1 = get_dialogue_object(dialogue, nt->object1);
	OBJ_INDEX_DATA *o2 = get_dialogue_object(dialogue, nt->object2);
	ROOM_INDEX_DATA *r = get_dialogue_room(dialogue, nt->room);
	AREA_DATA *a = get_dialogue_area(dialogue, nt->area);

	bool enabled = IS_NULLSTR(hint) && true;

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
				break;

			case DIALOGUE_ENTITY_OBJECT2:
				add_buf(buffer, "{W");
				add_buf(buffer, (o2 ? o2->short_descr : SOMETHING));
				break;

			case DIALOGUE_ENTITY_ROOM:
				add_buf(buffer, "{W");
				add_buf(buffer, (r ? r->name : SOMEWHERE));
				break;

			case DIALOGUE_ENTITY_AREA:
				add_buf(buffer, "{W");
				add_buf(buffer, (a ? a->name : SOMEWHERE));
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
				break;

			default:
				add_buf_char(buffer, *str);
				break;
		}

		++str;
	}

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

	if (var->type == VAR_INTEGER)
	{
		if (var->_.i == atoi(branch->value))
			return true;
	}
	else if (var->type == VAR_BOOLEAN)
	{
		bool value;
		if (is_number(branch->value))
			value = atoi(branch->value) != 0;
		else if(!str_prefix(branch->value, "true") || !str_prefix(branch->value, "yes") || !str_prefix(branch->value, "on"))
			value = true;
		else if(!str_prefix(branch->value, "false") || !str_prefix(branch->value, "no") || !str_prefix(branch->value, "off"))
			value = false;
		else
			return false;
		
		return var->_.boolean == value;
	}
	else if (var->type == VAR_STRING || var->type == VAR_STRING_S)
	{
		return !str_cmp(var->_.s, branch->value);
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
		
		case DIALOGUE_TYPE_SCRIPT:
			if (node->script)
				execute_script(node->script, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);
			next_node = node->child;
			break;

		case DIALOGUE_TYPE_BRANCH:
			next_node = select_dialogue_branch(ch, dialogue, node);
			break;

		case DIALOGUE_TYPE_CHOICE:
			ch->has_dialogue_choice = true;
			show_dialogue_choices(ch);
			return;
	}

	dialogue->current_node = next_node;
}

bool start_dialogue(CHAR_DATA *ch, DIALOGUE_INDEX_DATA *index, DIALOGUE_CALLBACK cb)
{
	DIALOGUE *dialogue = clone_dialogue(index, ch);

	if (!IS_VALID(dialogue)) return false;

	dialogue->callback = cb;
	ch->dialogue = dialogue;

	// Use this to initialize variables to the dialogue based upon the player
	if (dialogue->index->initialize)
		execute_script(dialogue->index->initialize, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE, 0, 0, 0, 0, 0);

	ch->has_dialogue_choice = false;
	do {
		execute_dialogue_node(ch);
	} while(IS_VALID(ch->dialogue) && dialogue->timer < 1 && !ch->has_dialogue_choice);

	return true;
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

	DIALOGUE_CHOICE *choice = (DIALOGUE_CHOICE *)list_nthdata(ch->dialogue->current_node->options, value);

	// Invalid choice or the choice is disabled
	if(!choice || !IS_NULLSTR(choice->hint))
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

				LLIST *rooms = list_create(false);
				iterator_start(&it, dialogue->rooms);
				while((load = (WNUM_LOAD *)iterator_nextdata(&it)))
				{
					ROOM_INDEX_DATA *r = get_room_index_auid(load->auid, load->vnum);

					if (r)
						list_appendlink(rooms, r);
				}
				iterator_stop(&it);
				list_destroy(dialogue->rooms);
				dialogue->rooms = rooms;

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

					list_appendlink(node->options, branch);
					fMatch = true;
					break;
				}
				else if (node->type == DIALOGUE_TYPE_CHOICE && !str_cmp(word, "#CHOICE"))
				{
					DIALOGUE_INDEX_CHOICE *choice = read_dialogue_index_choice(fp, dialogue, area);

					list_appendlink(node->options, choice);
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
				break;

			case 'S':
				KEY("Script", node->script_load, fread_widevnum(fp, area->uid));
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
					WNUM_LOAD *load = fread_widevnumptr(fp, area->uid);

					if(load)
					{
						if (!list_appendlink(dialogue->rooms, load))
							free_list_wnum_load(load);
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

		case DIALOGUE_TYPE_SCRIPT:
			fprintf(fp, "Script %s\n", widevnum_string_script(node->script, area));
			break;

		case DIALOGUE_TYPE_SET:
			fprintf(fp, "Variable %s~\n", fix_string(node->variable));
			fprintf(fp, "Value %s~\n", fix_string(node->value));
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

	ROOM_INDEX_DATA *room;
	iterator_start(&it, dialogue->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Room %s\n", widevnum_string(room->area, room->vnum, area));
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

void do_dialstart (CHAR_DATA *ch, char *argument)
{
	WNUM wnum;

	if (IS_VALID(ch->dialogue))
	{
		send_to_char("You are already in a dialogue!\n\r", ch);
		return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  dialstart <dialogue widevnum>\n\r", ch);
		return;
	}

	DIALOGUE_INDEX_DATA *index = get_dialogue_index(wnum.pArea, wnum.vnum);
	if (!index)
	{
		send_to_char("No such dialogue with that widevnum.\n\r", ch);
		return;
	}

	start_dialogue(ch, index, __dialogue_report);
}

