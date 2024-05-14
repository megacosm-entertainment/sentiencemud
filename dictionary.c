/**************************************************************************r
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
 *  Hash-map dictionary using integer (long) keys                          *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "merc.h"

#define DICT_SIZE		(MAX_KEY_HASH)

#define DICT_KEY(k)		((((k) % DICT_SIZE) + DICT_SIZE) % DICT_SIZE)

static inline long _dict_get_hash_key(long key)
{
	return (key % DICT_SIZE);
}

typedef struct dictionary_type DICTIONARY;
typedef struct dictionary_iterator_type DITERATOR;

// Allows collisions
struct dictionary_type
{
	DICTIONARY *next;
	bool valid;

	LLIST *data[DICT_SIZE];
	int count;

	LISTCOPY_FUNC *copier;
	LISTDESTROY_FUNC *deleter;
};

struct dictionary_iterator_type
{
	DICTIONARY *dict;
	ITERATOR it;
	int hash;
};

DICTIONARY *dictionary_free;
DICTIONARY *new_dictionaryx(LISTCOPY_FUNC *c, LISTDESTROY_FUNC *d)
{
	DICTIONARY *dict;
	if (dictionary_free)
	{
		dict = dictionary_free;
		dictionary_free = dictionary_free->next;
	}
	else
		dict = alloc_mem(sizeof(DICTIONARY));

	memset(dict, 0, sizeof(*dict));

	dict->copier = c;
	dict->deleter = d;

	VALIDATE(dict);
	return dict;
}

DICTIONARY *new_dictionary()
{
	return new_dictionaryx(NULL, NULL);
}

void free_dictionary(DICTIONARY *dict)
{
	if (!IS_VALID(dict)) return;

	for(int i = 0; i < DICT_SIZE; i++)
		list_destroy(dict->data[i]);		// Won't break if it's NULL.

	INVALIDATE(dict);
	dict->next = dictionary_free;
	dictionary_free = dict;
}

static LLIST *__dictionary_list_create(DICTIONARY *dict)
{
	return list_createx(false, dict->copier, dict->deleter);
}

bool dictionary_haskey(DICTIONARY *dict, long key)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	return list_size(dict->data[idx]) > 0;
}

bool dictionary_hasdata(DICTIONARY *dict, void *data)
{
	if (!IS_VALID(dict)) return false;

	for(int i = 0; i < DICT_SIZE; i++)
	{
		if (list_hasdata(dict->data[i], data))
			return true;
	}

	return false;
}

void dictionary_clear(DICTIONARY *dict)
{
	if (!IS_VALID(dict)) return;

	for(int i = 0; i < DICT_SIZE; i++)
	{
		list_clear(dict->data[i]);
	}
	dict->count = 0;
}

bool dictionary_add(DICTIONARY *dict, long key, void *data)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	if (dict->data[idx] != NULL)
	{
		if (list_hasdata(dict->data[idx], data))
			return false;
	}
	else
	{
		dict->data[idx] = __dictionary_list_create(dict);

		if (!dict->data[idx])
			return false;
	}

	if (!list_appendlink(dict->data[idx], data))
		return false;
	
	dict->count++;
	return true;
}

bool dictionary_removekey(DICTIONARY *dict, long key, bool delete)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	int sz = list_size(dict->data[idx]);
	if (sz == 1)
	{
		// Can *only* remove a key if there's only one in the list
		list_clear(dict->data[idx]);
	}

	dict->count--;
	return true;
}

int dictionary_size(DICTIONARY *dict)
{
	return IS_VALID(dict) ? dict->count : 0;
}

void diterator_start(DITERATOR *dit, DICTIONARY *dict)
{
	memset(dit, 0, sizeof(*dit));
	dit->dict = dict;
	dit->hash = -1;
}

void diterator_stop(DITERATOR *dit)
{
	if (dit->hash >= 0 && dit->hash < DICT_SIZE)
		iterator_stop(&dit->it);
}

void *diterator_nextdata(DITERATOR *dit)
{
	if (!IS_VALID(dit->dict)) return NULL;

	// Already done
	if (dit->hash >= DICT_SIZE) return NULL;

	if (dit->hash >= 0)		// Already engaged iterator before
	{
		void *data = iterator_nextdata(&dit->it);
		if (data) return data;	// Still have something in the current hash key

		iterator_stop(&dit->it);
	}

	// Advance to next valid hash
	do {
		++dit->hash;
	} while(dit->hash < DICT_SIZE && list_size(dit->dict->data[dit->hash]) < 1);

	// Exhausted dictionary
	if (dit->hash >= DICT_SIZE)
		return NULL;

	iterator_start(&dit->it, dit->dict->data[dit->hash]);

	return iterator_nextdata(&dit->it);
}




typedef struct unique_dictionary_type UDICTIONARY;
typedef struct unique_dictionary_iterator_type DUITERATOR;

// Does not allow collisions
struct unique_dictionary_type
{
	UDICTIONARY *next;
	bool valid;

	void *data[DICT_SIZE];
	int count;

	LISTCOPY_FUNC *copier;
	LISTDESTROY_FUNC *deleter;
};

struct unique_dictionary_iterator_type
{
	UDICTIONARY *dict;
	int hash;
	bool moved;
};

UDICTIONARY *udictionary_free;
UDICTIONARY *new_udictionaryx(LISTCOPY_FUNC *c, LISTDESTROY_FUNC *d)
{
	UDICTIONARY *dict;
	if (udictionary_free)
	{
		dict = udictionary_free;
		udictionary_free = udictionary_free->next;
	}
	else
		dict = alloc_mem(sizeof(UDICTIONARY));

	memset(dict, 0, sizeof(*dict));

	dict->copier = c;
	dict->deleter = d;

	VALIDATE(dict);
	return dict;
}

UDICTIONARY *new_udictionary()
{
	return new_udictionaryx(NULL, NULL);
}

void free_udictionary(UDICTIONARY *dict)
{
	if (!IS_VALID(dict)) return;

	if (dict->deleter != NULL)
		for(int i = 0; i < DICT_SIZE; i++)
		{
			(*dict->deleter)(dict->data[i]);
		}

	INVALIDATE(dict);
	dict->next = udictionary_free;
	udictionary_free = dict;
}


bool udictionary_haskey(UDICTIONARY *dict, long key)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	return dict->data[idx] != NULL;
}

bool udictionary_hasdata(UDICTIONARY *dict, void *data)
{
	if (!IS_VALID(dict)) return false;

	for(int i = 0; i < DICT_SIZE; i++)
	{
		if (dict->data[i] == data)
			return true;
	}

	return false;
}

void udictionary_clear(UDICTIONARY *dict, bool del)
{
	if (!IS_VALID(dict)) return;

	if (del && dict->deleter)
	{
		for(int i = 0; i < DICT_SIZE; i++)
		{
			(*(dict->deleter))(dict->data[i]);
			dict->data[i] = NULL;
		}
	}
	else
	{
		for(int i = 0; i < DICT_SIZE; i++)
		{
			dict->data[i] = NULL;
		}
	}

	dict->count = 0;
}

bool udictionary_add(UDICTIONARY *dict, long key, void *data)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	if (dict->data[idx] != NULL)
		return false;

	dict->data[idx] = data;
	dict->count++;
	return true;
}

bool udictionary_removekey(UDICTIONARY *dict, long key, bool delete)
{
	if (!IS_VALID(dict)) return false;

	long idx = DICT_KEY(key);

	if (idx < 0 || idx >= DICT_SIZE) return false;

	if (dict->data[idx] == NULL) return false;

	if (delete && dict->deleter)
		(*(dict->deleter))(dict->data[idx]);
	dict->data[idx] = NULL;
	dict->count--;
	return true;
}

int udictionary_size(UDICTIONARY *dict)
{
	return IS_VALID(dict) ? dict->count : 0;
}

void duiterator_start(DUITERATOR *dit, UDICTIONARY *dict)
{
	memset(dit, 0, sizeof(*dit));
	dit->dict = dict;
	dit->hash = -1;
}

void duiterator_stop(DUITERATOR *dit)
{
	dit->hash = DICT_SIZE;
}

void *duiterator_nextdata(DUITERATOR *dit)
{
	if (!IS_VALID(dit->dict)) return NULL;

	// Already done
	if (dit->hash >= DICT_SIZE) return NULL;

	// Advance to next valid hash
	do {
		++dit->hash;
	} while(dit->hash < DICT_SIZE && dit->dict->data[dit->hash] != NULL);

	// Exhausted dictionary
	if (dit->hash >= DICT_SIZE)
		return NULL;

	return dit->dict->data[dit->hash];
}
