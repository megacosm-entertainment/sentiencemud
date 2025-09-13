#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "niblang.h"

LLIST *list_create(bool purge)
{
	LLIST *lp = calloc(1,sizeof(LLIST));

	if(lp) {
		lp->next = NULL;
		lp->head = NULL;
		lp->ref = 0;
		lp->size = 0;
		lp->valid = true;
		lp->purge = purge;
		lp->copier = NULL;
		lp->deleter = NULL;
	}

	return lp;
}

LLIST *list_createx(bool purge, LISTCOPY_FUNC copier, LISTDESTROY_FUNC deleter)
{
	LLIST *lp = list_create(purge);

	if( lp ) {
		lp->copier = copier;
		lp->deleter = deleter;
	}

	return lp;
}


LLIST *list_copy(LLIST *src)
{
	if( src == NULL || !src->valid || src->purge ) return NULL;

	LLIST *cpy = list_create(src->purge);

	if( cpy ) {
		register LLIST_LINK *cur, *next;
		bool valid = true;

		cpy->copier = src->copier;
		cpy->deleter = src->deleter;

		for( cur = src->head; cur; cur = next ) {
			next = cur->next;
			if( cur->data ) {
				void *data = cur->data;

				if( cpy->copier )
					data = (*cpy->copier)(data);

				if( !data || !list_appendlink(cpy, data) ) {
					if( data && cpy->copier && cpy->deleter )
						(*cpy->deleter)(data);

					valid = false;
					break;
				}
			}
		}

		if( !valid ) {
			list_destroy(cpy);
			cpy = NULL;
		}
	}

	return cpy;
}

void list_purge(LLIST *lp)
{
	register LLIST_LINK *cur, *next;
	if( lp && !lp->valid && lp->ref < 1) {
		for( cur = lp->head; cur; cur = next ) {
			next = cur->next;

			if( lp->deleter && cur->data )
				(*lp->deleter)(cur->data);

			free(cur);
		}
		lp->head = NULL;
		lp->tail = NULL;
	}
}

void list_destroy(LLIST *lp)
{
	if(lp && lp->valid ) {
		lp->valid = false;
		if( lp->ref < 1 ) {
			// This point is only ever reached if the list has not references at the time this list is destroyed
			// If the list is in-use, the purging/freeing is handled when the references are cleared.
			list_purge(lp);
			free(lp);
		}
	}
}

void list_cull(LLIST *lp)
{
	register LLIST_LINK /* **prev, */ *cur, *next;

	if(lp && lp->ref < 1) {
		// Cull any null data nodes
		for(cur = lp->head;cur;)
		{
			if (!cur->data)
			{
				next = cur->next;
				if (cur->prev)
					cur->prev->next = next;
				else
					lp->head = next;

				if (cur->next)
					cur->next->prev = cur->prev;
				else
					lp->tail = cur->prev;

				free(cur);
				cur = next;
			}
			else
			{
				cur = cur->next;
			}
		}

/*
		for(prev = &lp->head, cur = lp->head; cur;) {
			if(!cur->data) {
				do {
					if( lp->tail == cur )
						lp->tail = lp->tail->prev;
					*prev = cur->next;


					free_mem(cur,sizeof(LLIST_LINK));
					cur = *prev;
				} while(cur && !cur->data);
			} else {
				prev = &cur->next;
				cur = *prev;
			}
		}
*/

		if(!lp->tail && lp->head ) {
			if( lp->head->next ) {
				for(cur = lp->head; cur->next;cur = cur->next )
				{
					cur->next->prev = cur;	// Reset the double linkage
				}
				lp->tail = cur;
			} else
				lp->tail = lp->head;
			lp->head->prev = NULL;
			lp->tail->next = NULL;
		}
	}
}

void list_addref(LLIST *lp)
{
	if(lp) lp->ref++;
}

void list_remref(LLIST *lp)
{
	if(lp) {
		--lp->ref;
		list_cull(lp);

		if(lp->ref < 1 && !lp->valid) {
			list_purge(lp);
			free(lp);
		} else if(lp->ref < 1 && lp->purge) {
			list_destroy(lp);
		}
	}
}

void list_remdata(LLIST *lp, LLIST_LINK *link, bool del)
{
	if( link ) {
		if( del && lp->deleter )
			(*lp->deleter)(link->data);

		link->data = NULL;
		lp->size--;
	}
}


bool list_addlink(LLIST *lp, void *data)
{
	LLIST_LINK *link;

	if(lp && lp->valid && (link = calloc(1,sizeof(LLIST_LINK)))) {

		// No need to worry about the linkage and reference
		link->next = lp->head;
		if( !lp->head )
			lp->tail = link;
		else
			lp->head->prev = link;
		lp->head = link;
		link->prev = NULL;

		link->data = data;
		lp->size++;
		return true;
	}
	return false;
}

bool list_appendlink(LLIST *lp, void *data)
{
	LLIST_LINK *link;

	if(lp && lp->valid && (link = calloc(1,sizeof(LLIST_LINK)))) {
//		log_stringf("list_appendlink: Adding data %016X to list %016X.", lp, data);
		// First one?
		if( !lp->head )
			lp->head = link;
		else
			lp->tail->next = link;
		link->prev = lp->tail;
		lp->tail = link;

		link->data = data;
		lp->size++;
		return true;
	}
	return false;
}

// DOES NOT DEEP COPY ELEMENTS
bool list_appendlist(LLIST *lp, LLIST *src)
{
	if( list_isvalid(lp) && list_isvalid(src) )
	{
		ITERATOR it;
		void *data;
		bool good = true;


		iterator_start(&it, src);
		while( (data = iterator_nextdata(&it)) )
		{
			if( !list_appendlink(lp, data) )
			{
				good = false;
				break;
			}
		}
		iterator_stop(&it);

		return good;
	}

	return false;
}

/*
bool list_movelink(LLIST *lp, int from, int to)
{
	LLIST_LINK *old, *link, *new_link;

	if( from < 0 ) from = lp->size + from + 1;
	if( to < 0 ) to = lp->size + to + 1;

	if( !from || !to ) return false;

	if( from == to ) return true;	// "Moved" it! :D :D :D :D

	//if( to > from ) --to;

	if( lp )
	{
		old = NULL;
		for(link = lp->head; link && from > 0; link = link->next)
			if(link->data)
			{
				--from;
				if( !from )
				{
					old = link;
					break;
				}
			}

		if( !old )
			return false;

		for(link = lp->head; link && to > 0; link = link->next )
		{
			if(link != old)
			{
				if( (to == 1) && (!link->data) )
				{
					// This is an empty link, reuse it
					link->data = old->data;
					old->data = NULL;
					return true;
				}

				if( link->data )
				{
					if( !--to )
					{
						new_link = calloc(1,sizeof(LLIST_LINK));
						if( !new_link )
							return false;

						new_link->data = old->data;
						old->data = NULL;

						if( link->prev )
						{
							new_link->next = link;
							link->prev->next = new_link;
							link->prev = new_link;
						}
						else
						{
							new_link->next = lp->head;
							lp->head = new_link;
							new_link->prev = NULL;
						}

						return true;
					}
				}
			}
		}

		if( to > 0 )
		{
			// Needs to append if it's at the end
			link = calloc(1,sizeof(LLIST_LINK));
			if( !link )
				return false;

			if( !lp->head )
				lp->head = link;
			else
				lp->tail->next = link;
			link->prev = lp->tail;
			lp->tail = link;

			link->data = old->data;
			old->data = NULL;
		}
	}

	return false;
}
*/

bool list_movelink(LLIST *lp, int from, int to)
{
    LLIST_LINK *old, *link, *new_link;

    // Adjust negative indices
    if (from < 0) from = lp->size + from + 1;
    if (to < 0) to = lp->size + to + 1;

    // Check for invalid positions
    if (from <= 0 || to <= 0 || from > lp->size || to > lp->size) return false;

    // If from and to are the same, no need to move
    if (from == to) return true;

    if (lp)
    {
        old = NULL;
        // Locate the 'from' node
        for (link = lp->head; link && from > 0; link = link->next)
            if (link->data)
            {
                --from;
                if (!from)
                {
                    old = link;
                    break;
                }
            }

        if (!old) return false;

        // Locate the 'to' position
        for (link = lp->head; link && to > 0; link = link->next)
        {
            if (link != old)
            {
                if ((to == 1) && (!link->data))
                {
                    // This is an empty link, reuse it
                    link->data = old->data;
                    old->data = NULL;
                    return true;
                }

                if (link->data)
                {
                    if (!--to)
                    {
                        new_link = calloc(1,sizeof(LLIST_LINK));
                        if (!new_link) return false;

                        new_link->data = old->data;
                        old->data = NULL;

                        if (link->prev)
                        {
                            new_link->next = link;
                            new_link->prev = link->prev;
                            link->prev->next = new_link;
                            link->prev = new_link;
                        }
                        else
                        {
                            new_link->next = lp->head;
                            lp->head->prev = new_link;
                            lp->head = new_link;
                            new_link->prev = NULL;
                        }

                        // Update list size
                        lp->size++;
                        return true;
                    }
                }
            }
        }

        if (to > 0)
        {
            // Needs to append if it's at the end
            link = calloc(1,sizeof(LLIST_LINK));
            if (!link) return false;

            if (!lp->head)
                lp->head = link;
            else
                lp->tail->next = link;
            link->prev = lp->tail;
            lp->tail = link;

            link->data = old->data;
            old->data = NULL;

            // Update list size
            lp->size++;
        }

        // Update head and tail if necessary
        if (old == lp->head) lp->head = old->next;
        if (old == lp->tail) lp->tail = old->prev;

        // Remove old link from its current position
        if (old->prev) old->prev->next = old->next;
        if (old->next) old->next->prev = old->prev;

        free(old);

        // Update list size
        lp->size--;
    }

    return false;
}

bool list_insertlink(LLIST *lp, void *data, int to)
{
	LLIST_LINK *link, *new_link;

	if( to < 0 ) to = lp->size + to + 1;

	if( !to ) return false;

	if( lp )
	{
		for(link = lp->head; link && to > 0; link = link->next )
		{
			if( (to == 1) && (!link->data) )
			{
				// This is an empty link, reuse it
				link->data = data;
				lp->size++;
				return true;
			}

			if( link->data )
			{
				if( !--to )
				{
					new_link = calloc(1,sizeof(LLIST_LINK));
					if( !new_link )
						return false;

					new_link->data = data;

					if( link->prev )
					{
						new_link->next = link;
						link->prev->next = new_link;
						link->prev = new_link;
						
					}
					else
					{
						new_link->next = lp->head;
						lp->head = link;
						link->prev = NULL;
					}

					lp->size++;
					return true;
				}
			}
		}


		if( to > 0 )
		{
			// Needs to append if it's at the end
			link = calloc(1,sizeof(LLIST_LINK));
			if( !link )
				return false;

			if( !lp->head )
				lp->head = link;
			else
				lp->tail->next = link;
			link->prev = lp->tail;
			lp->tail = link;
			link->data = data;
			lp->size++;
		}

	}

	return false;
}


// Nulls out any data pointer that matches the supplied pointer
// It will NOT cull the list
void list_remlink(LLIST *lp, void *data, bool del)
{
	LLIST_LINK *link, *link_next;

	if(lp && data) {
		for(link = lp->head; link; link = link_next)
		{
			link_next = link->next;
			if(link->data == data) {
				list_remdata(lp, link, del);
			}
		}
	}
}

bool list_haslink(LLIST *list, void *data) {
    LLIST_LINK *link;
    if (!list || !data)
        return false;
    for (link = list->head; link != NULL; link = link->next) {
        if (link->data == data)
            return true;
    }
    return false;
}

// Clears out the entire list
void list_clear(LLIST *lp)
{
	LLIST_LINK *link, *link_next;
	if(lp && lp->valid) {
		for(link = lp->head; link; link = link_next) {
			link_next = link->next;
			list_remdata(lp, link, true);
		}

		lp->size = 0;
		list_cull(lp);
	}
}

// Get the last entry of a list.
void *list_last(LLIST *list)
{
    if (!list || list->size == 0)
        return NULL;
        
    void *data = NULL;
    ITERATOR it;
    
    iterator_start(&it, list);
    while (iterator_hasdata(&it)) {
        data = iterator_nextdata(&it);
    }
    iterator_stop(&it);
    
    return data;
}

void *iterator_peek_nextdata(ITERATOR *it)
{
    LLIST_LINK *link;

    if (!it || !it->list || !it->list->valid || !it->current)
        return NULL;

    // If we haven't moved yet, peek at current if valid, else next
    if (!it->moved) {
        link = it->current;
        if (link && link->data)
            link = link->next;
    } else {
        link = it->current ? it->current->next : NULL;
    }

    // Find the next link with data
    while (link && !link->data)
        link = link->next;

    return link ? link->data : NULL;
}

bool iterator_hasdata(ITERATOR *it)
{
    if(it && it->list && it->list->valid && it->current) {
        return (it->current->data != NULL);
    }
    return false;
}

/*
void *list_randomdata(LLIST *lp)
{
	register LLIST_LINK *link = NULL;
	register int nth = 0;

	if(lp && lp->valid) {
		nth = number_range(1, lp->size);
		if( nth < 0 ) nth = lp->size + nth + 1;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	return (link && !nth) ? link->data : NULL;
}
*/


void *list_nthdata(LLIST *lp, register int nth)
{
	register LLIST_LINK *link = NULL;

	if(lp && lp->valid) {
		if( nth < 0 ) nth = lp->size + nth + 1;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	return (link && !nth) ? link->data : NULL;
}

void **list_nthdataptr(LLIST *lp, register int nth)
{
	register LLIST_LINK *link = NULL;

	if(lp && lp->valid) {
		if( nth < 0 ) nth = lp->size + nth + 1;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	return (link && !nth) ? &(link->data) : NULL;
}

void list_remnthlink(LLIST *lp, register int nth, bool del)
{
	register LLIST_LINK *link = NULL;

	if(lp && lp->valid) {
		if( nth < 0 ) nth = lp->size + nth + 1;
		for(link = lp->head; link && nth > 0; link = link->next)
			if(link->data)
			{
				--nth;
				if( !nth ) break;
			}
	}

	if( link && !nth ) {
		list_remdata(lp, link, del);
	}
}

bool list_contains(LLIST *lp, register void *ptr, int (*cmp)(void *a, void *b))
{
	ITERATOR it;
	void *data;

	if(!lp || !lp->valid || !ptr) return false;

	iterator_start(&it, lp);
	if (cmp != NULL)
	{
		while((data = iterator_nextdata(&it)))
		{
			if (!cmp(data, ptr))
				break;
		}
	}
	else
	{
		while((data = iterator_nextdata(&it)) && (data != ptr));
	}

	iterator_stop(&it);

	return data && true;
}

bool list_hasdata(LLIST *lp, register void *ptr)
{
	ITERATOR it;
	void *data;

	if(!lp || !lp->valid || !ptr) return false;

	iterator_start(&it, lp);
	while((data = iterator_nextdata(&it)) && (data != ptr));

	iterator_stop(&it);

	return data && true;
}

int list_size(LLIST *lp)
{
	if(!lp || !lp->valid) return 0;

#if 0
	ITERATOR it;
	int size = 0;

	iterator_start(&it, lp);
	while((iterator_nextdata(&it))) ++size;

	iterator_stop(&it);

	return size;
#else
	return lp->size;
#endif
}

int list_getindex(LLIST *lp, void *ptr)
{
	ITERATOR it;
	void *data;
	int index = 0;

	iterator_start(&it, lp);
	while( (data = iterator_nextdata(&it)) )
	{
		++index;

		if( data == ptr )
			break;
	}
	iterator_stop(&it);

	return data ? index : 0;
}

bool list_isvalid(LLIST *lp)
{
	return lp && lp->valid;
}

// ITERATORs can just be straight variables.  No allocation is needed.
void iterator_start(ITERATOR *it, LLIST *lp)
{
	if(it) {
		if(lp && lp->valid) {
//			log_stringf("iterator_start: list =  %016lX.", lp);
			it->list = lp;
			it->current = lp->head;
			it->moved = false;

			list_addref(lp);
		} else {
			it->list = NULL;
			it->current = NULL;
			it->moved = false;
		}
	}
}

void iterator_start_nth(ITERATOR *it, LLIST *lp, int nth)
{
	register LLIST_LINK *link;

	if(it) {
		if(lp && lp->valid) {
			it->list = lp;

			if( nth < 0 ) nth = lp->size + nth + 1;
			// -1 -> size
			// -2 -> size - 1

			// Skip all dead nodes
			for(link = lp->head; link && !link->data; link = link->next);

			// Skip N-1 nodes
			for(; link && nth > 1; link = link->next)
				if(link->data)
					--nth;

			it->current = link;
			it->moved = false;

			list_addref(lp);
		} else {
			it->list = NULL;
			it->current = NULL;
		}
	}
}

LLIST_LINK *iterator_next(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->next; link && !link->data; link = link->next);
		} else {
			for(link = it->current; link && !link->data; link = link->next);

			it->moved = true;
		}
		it->current = link;

	}

	return link;
}

void *iterator_prevdata(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	//register LLIST_LINK *next = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->prev; link && !link->data; link = link->prev);
		} else {
			for(link = it->current; link && !link->data; link = link->prev);

			it->moved = true;
		}
		it->current = link;
	}

	return link ? link->data : NULL;

}

void *iterator_currentdata(ITERATOR *it)
{
    if(it && it->list && it->list->valid && it->current) {
        return it->current->data;
    }
    return NULL;
}

// Used to get the data ptr for use in scripting
//  Only used when the actual data is a naked pointer, and not a proxy type
void **iterator_nextdataptr(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	//register LLIST_LINK *next = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->next; link && !link->data; link = link->next);
		} else {
			for(link = it->current; link && !link->data; link = link->next);

			it->moved = true;
		}
		it->current = link;
	}

	return link ? &(link->data) : NULL;
}


void *iterator_nextdata(ITERATOR *it)
{
	register LLIST_LINK *link = NULL;
	//register LLIST_LINK *next = NULL;
	if(it && it->list && it->list->valid && it->current) {
		if( it->moved ) {
			for(link = it->current->next; link && !link->data; link = link->next);
		} else {
			for(link = it->current; link && !link->data; link = link->next);

			it->moved = true;
		}
		it->current = link;
	}

	return link ? link->data : NULL;
}

void iterator_setcurrent(ITERATOR *it, void *data)
{
	if (it && it->list && it->list->valid && it->current)
	{
		// Delete the current data using specific deleter if it is defined
		if( it->list->deleter )
			(*it->list->deleter)(it->current->data);
		
		if (!it->current->data)
			it->list->size++;

		it->current->data = data;
	}
}

void iterator_remcurrent(ITERATOR *it)
{
	if(it && it->list && it->current && it->current->data)
	{
		// Delete the data using specific deleter if it is defined
		if( it->list->deleter )
			(*it->list->deleter)(it->current->data);

		it->current->data = NULL;
		it->list->size--;

	}
}

void iterator_reset(ITERATOR *it)
{
	if(it) {
		if(it->list && it->list->valid) {
			it->current = it->list->head;
		} else {
			it->list = NULL;
			it->current = NULL;
		}
	}
}

void iterator_stop(ITERATOR *it)
{
	if(it) {
		if(it->list) list_remref(it->list);

		it->list = NULL;
		it->current = NULL;
	}
}

bool iterator_insert_before(ITERATOR *it, void *data)
{
	if (it && it->list && it->current)
	{
		// This spot is already blank
		if(!it->current->data)
			it->current->data = data;

		// Previous spot is already blank
		else if (it->current->prev && !it->current->prev->data)
			it->current->prev->data = data;
		else
		{
			LLIST_LINK *link = calloc(1,sizeof(LLIST_LINK));
			if(!link) return false;

			link->prev = it->current->prev;

			if (it->current->prev)
				it->current->prev->next = link;
			else
				it->list->head = link;

			link->next = it->current;
			it->current->prev = link;

			link->data = data;
		}

		it->list->size++;
		return true;
	}

	return false;
}

bool iterator_insert_after(ITERATOR *it, void *data)
{
	if (it && it->list && it->current)
	{
		// This spot is already blank
		if(!it->current->data)
			it->current->data = data;

		// Next spot is already blank
		else if (it->current->next && !it->current->next->data)
			it->current->next->data = data;
		else
		{
			LLIST_LINK *link = calloc(1,sizeof(LLIST_LINK));
			if(!link) return false;

			link->next = it->current->next;
			if (it->current->next)
				it->current->next->prev = link;
			else
				it->list->tail = link;

			link->prev = it->current;
			it->current->next = link;

			link->data = data;
		}

		it->list->size++;
		return true;
	}

	return false;
}
