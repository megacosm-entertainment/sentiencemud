/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"

#if defined( NO_BCOPY )
void bcopy(register char *s1,register char *s2,int len);
#endif

#if defined( NO_BZERO )
void bzero(register char *sp,int len);
#endif

typedef long long hashkey_t;
typedef unsigned long long uhashkey_t;

struct hash_link
{
    hashkey_t	key;
    struct hash_link	*next;
    void		*data;
};

struct hash_header
{
    int			rec_size;
    int			table_size;
    hashkey_t		*keylist;
    int     klistsize, klistlen; /* this is really lame,
							  AMAZINGLY lame */
    struct hash_link	**buckets;
};


#define WORLD_SIZE	30000
#define MAKEKEY(a, v) (hashkey_t)((((hashkey_t)(a))<<32) + (hashkey_t)(v))
#define	HASH_KEY(ht,key)((int)((((uhashkey_t)(key))*17)%(ht)->table_size))

struct hunting_data
{
    char *name;
    struct char_data	**victim;
};

struct room_q
{
    AREA_DATA *area_nr;
    int		room_nr;
    struct room_q	*next_q;
};


#if defined( NO_BCOPY )
void bcopy(register char *s1,register char *s2,int len)
{
    while( len-- ) *(s2++) = *(s1++);
}
#endif

#if defined( NO_BZERO )
void bzero(register char *sp,int len)
{
    while( len-- ) *(sp++) = '\0';
}
#endif

void init_hash_table(struct hash_header	*ht,int rec_size,int table_size)
{
    ht->rec_size	= rec_size;
    ht->table_size= table_size;
    ht->buckets	= (void*)calloc(sizeof(struct hash_link**),table_size);
    ht->keylist	= (void*)malloc(sizeof(hashkey_t)*(ht->klistsize=128));
    ht->klistlen	= 0;
}

void destroy_hash_table(struct hash_header *ht,void (*gman)())
{
    int			i;
    struct hash_link	*scan,*temp;

    for(i=0;i<ht->table_size;i++)
	for(scan=ht->buckets[i];scan;)
	{
	    temp = scan->next;
	    (*gman)(scan->data);
	    free(scan);
	    scan = temp;
	}
    free(ht->buckets);
    free(ht->keylist);
}

void _hash_enter(struct hash_header *ht,hashkey_t key,void *data)
{
    /* precondition: there is no entry for <key> yet */
    int i;
    struct hash_link *temp = (struct hash_link *)malloc(sizeof(struct hash_link));

    temp->key	= key;
    temp->next	= ht->buckets[HASH_KEY(ht,key)];
    temp->data	= data;
    ht->buckets[HASH_KEY(ht,key)] = temp;

    // Resize the keylist
    if(ht->klistlen>=ht->klistsize)
    {
        ht->keylist = (void*)realloc(ht->keylist,sizeof(hashkey_t)*(ht->klistsize*=2));
    }
    // Iterate over keylist to insert the keys in a sorted manner
    // This had i >= 0, but that could have lead to looking outside the allocated memory
    for(i=ht->klistlen; i>0; i--)
    {
        // Insert once we found a place
        if(ht->keylist[i-1]<key)
        {
            ht->keylist[i] = key;
            break;
        }
        // Move element up one to make room
        ht->keylist[i] = ht->keylist[i-1];
    }
    // If it reaches this point, the key belongs at the beginning of the array.
    if (i == 0)
        ht->keylist[0] = key;
    ht->klistlen++;
}

void *hash_find(struct hash_header *ht,hashkey_t key)
{
    struct hash_link *scan;

    scan = ht->buckets[HASH_KEY(ht,key)];

    while(scan && scan->key!=key)
        scan = scan->next;

    return scan ? scan->data : NULL;
}

bool hash_enter(struct hash_header *ht,hashkey_t key,void *data)
{
    void *temp;

    temp = hash_find(ht,key);
    if(temp) return false;

    _hash_enter(ht,key,data);
    return true;
}

void *hash_remove(struct hash_header *ht,hashkey_t key)
{
    // This needs to be double pointer because it needs to update pointers
    struct hash_link **scan;

    scan = ht->buckets+HASH_KEY(ht,key);

    while(*scan && (*scan)->key!=key)
        scan = &(*scan)->next;

    if(*scan)
    {
        int i;
        struct hash_link *temp, *aux;

        temp = (*scan)->data;
        aux	= *scan;
        *scan	= aux->next;
        free(aux);

        for(i=0;i<ht->klistlen;i++)
            if(ht->keylist[i]==key)
                break;

        if(i<ht->klistlen)
        {
            bcopy((char *)ht->keylist+i+1,(char *)ht->keylist+i,
                    (ht->klistlen-i)*sizeof(hashkey_t));
            ht->klistlen--;
        }

        return temp;
    }

    return NULL;
}

void hash_iterate(struct hash_header *ht,void (*func)(),void *cdata)
{
    int i;

    for( i = 0 ; i < ht->klistlen; i++ )
    {
        register hashkey_t key = ht->keylist[i];
        void *temp = hash_find(ht,key);

        (*func)(key,temp,cdata);

        // Check if the function deleted the room, if so, reuse the index
        if(ht->keylist[i]!=key)
            i--;
    }
}


bool exit_ok( EXIT_DATA *pexit )
{
    return ( pexit && pexit->u1.to_room && !IS_SET(pexit->exit_info,EX_NOHUNT) );
}


void donothing( void )
{
    return;
}


int find_path( AREA_DATA *in_area, long in_room_vnum, AREA_DATA *out_area, long out_room_vnum, CHAR_DATA *ch,
	       int depth, int in_zone )
{
    struct room_q *tmp_q, *q_head, *q_tail;
    struct hash_header	x_room;
    AREA_DATA *tmp_area;
    int	i, tmp_room, count=0, thru_doors;
    ROOM_INDEX_DATA *herep;
    ROOM_INDEX_DATA *startp;
    EXIT_DATA *exitp;

    if ( depth <0 )
    {
    	thru_doors = true;
    	depth = -depth;
    }
    else
    	thru_doors = false;

    startp = get_room_index( in_area, in_room_vnum );

    init_hash_table( &x_room, sizeof(int), 2048 );
    hash_enter( &x_room, MAKEKEY(in_area->uid, in_room_vnum), (void *)-1 );

    /* initialize queue */
    q_head = (struct room_q *) malloc(sizeof(struct room_q));
    q_tail = q_head;
    q_tail->area_nr = in_area;
    q_tail->room_nr = in_room_vnum;
    q_tail->next_q = NULL;

    while(q_head)
    {
    	herep = get_room_index( q_head->area_nr, q_head->room_nr );
        /* for each room test all directions */
        if( herep->area == startp->area || !in_zone )
        {
            /* only look in this zone...
            saves cpu time and  makes world safer for players  */
            for( i = 0; i < MAX_DIR; i++ )
            {
                exitp = herep->exit[i];
                if( exit_ok(exitp) && ( thru_doors || !IS_SET(exitp->exit_info, EX_CLOSED) ) )
                {
                    /* next room */

                    // TODO: Does this go into wilderness?

                    tmp_area = herep->exit[i]->u1.to_room->area;
                    tmp_room = herep->exit[i]->u1.to_room->vnum;
                    hashkey_t k = MAKEKEY(tmp_area->uid, tmp_room);
                    void *thp = hash_find(&x_room,k);
                    if( tmp_area != out_area || tmp_room != out_room_vnum )
                    {
                        /* shall we add room to queue ?
                        count determines total breadth and depth */
                        if( !thp &&             /* Not found */
                            ( count < depth )   /* Within depth range of scanning */ )
                        {
                            count++;
                            /* mark room as visted and put on queue */

                            tmp_q = (struct room_q *) malloc(sizeof(struct room_q));
                            tmp_q->area_nr = tmp_area;
                            tmp_q->room_nr = tmp_room;
                            tmp_q->next_q = NULL;
                            q_tail->next_q = tmp_q;
                            q_tail = tmp_q;

                            /* ancestor for first layer is the direction */
                            hashkey_t k = MAKEKEY(q_head->area_nr->uid, q_head->room_nr);
                            void *hp = hash_find(&x_room,k);

                            hash_enter( &x_room, tmp_room, (hp == (void*)-1) ? (void*)(size_t)(i+1) : hp);
                        }
                    }
                    else
                    {
                        /* have reached our goal so free queue */
                        tmp_area = q_head->area_nr;
                        tmp_room = q_head->room_nr;
                        for(;q_head;q_head = tmp_q)
                        {
                            tmp_q = q_head->next_q;
                            free(q_head);
                        }
                        /* return direction if first layer */
                        int dir = (int)(size_t)thp;
                        if (dir == -1)
                        {
                            // This is returned if this is the FIRST point, ie.. the destination is literally adjacent to the starting room exitwise.
                            if (x_room.buckets)
                            {
                                /* junk left over from a previous track */
                                destroy_hash_table(&x_room, donothing);
                            }
                            return i;
                        }
                        else
                        {
                            /* else return the ancestor */
                            if (x_room.buckets)
                            {
                                /* junk left over from a previous track */
                                destroy_hash_table(&x_room, donothing);
                            }
                            return dir-1;
                        }
                    }
                }
            }
        }

        /* free queue head and point to next entry */
        tmp_q = q_head->next_q;
        free(q_head);
        q_head = tmp_q;
    }

    /* couldn't find path */
    if( x_room.buckets )
    {
    	/* junk left over from a previous track */
	    destroy_hash_table( &x_room, donothing );
    }
    return -1;
}


void do_hunt( CHAR_DATA *ch, char *argument )
{
//    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    char arg2[MSL];
    CHAR_DATA *victim;
    int direction;
    bool fAuto = false;

    argument = one_argument( argument, arg );
    argument = one_argument( argument, arg2 );

    bool has_hunt = true;

    if (!IS_NPC(ch))
    {
        // TODO: Change the Sith/Naga race checks to a racial trait?
        if (!IS_SITH(ch) || !IS_SET(ch->parts, PART_LONG_TONGUE))
            has_hunt = false;

        if (get_skill(ch, gsk_hunt) > 0)
            has_hunt = true;
    }

    if (!has_hunt)
    {
        send_to_char("Huh?\n\r",ch);
        return;
    }

    if ( ch->hunting != NULL )
    {
        send_to_char("You stop hunting.\n\r", ch );
        ch->hunting = NULL;
        return;
    }

    if (IN_WILDERNESS(ch))
    {
        send_to_char("Not here.\n\r", ch);
        return;
    }

    if (is_dead(ch))
        return;

    if( arg[0] == '\0' )
    {
    	send_to_char( "Whom are you trying to hunt?\n\r", ch );
    	return;
    }

    if ( !str_cmp( arg2, "auto" ) )
    	fAuto = true;

    if ( ( victim = (CHAR_DATA *)get_char_area( ch, arg ) ) == NULL )
    {
    	victim = get_char_world( ch, arg );
        if ( victim == NULL )
        {
            send_to_char("No-one around by that name.\n\r", ch );
            return;
        }
    }

    if ( !can_hunt( ch, victim ) )
    {
    	act("$N has magically covered $S tracks.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    	return;
    }

    if ( ch->in_room == victim->in_room )
    {
    	act( "$N is here!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    	return;
    }

    if ( IN_WILDERNESS( ch ) )
    {
    	act( "You can't track people out in the wilderness.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    	return;
    }

    if ( IN_WILDERNESS( victim ) )
    {
    	act( "You can't track people who are out in the wilderness.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    	return;
    }

   /*
    * Deduct some movement.
    */
    if( IS_NPC(ch) || ch->move > 2 )
	    deduct_move( ch, 3 );
    else
    {
        send_to_char( "You're too exhausted to hunt for anyone!\n\r", ch );
        return;
    }

    // For trackless step skill
    if (get_skill( victim, gsk_trackless_step ) > 0 && is_in_nature(victim->in_room) )
    {
        if ( number_percent() < get_skill( victim, gsk_trackless_step ) )
        {
            act("$N has covered $S tracks too well for you to follow.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
            return;
        }
    }

    if (!IS_SITH(ch) || !IS_SET(ch->parts, PART_LONG_TONGUE))
    	act( "$n scans $s environment.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
    else
	    act("$n's forked tongue whips out and tastes the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    // Max rooms so people can track across areas without megalag
    direction = find_path( ch->in_room->area, ch->in_room->vnum,
        victim->in_room->area, victim->in_room->vnum,
	    ch, -1000, false );

    if( direction == -1 || (IS_NPC(victim) && IS_SET(victim->act[1], ACT2_NO_HUNT)))
    {
    	act("You couldn't find a path to $N from here.\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    	return;
    }

    // Auto-Hunt ?
    if ( fAuto )
    {
    	if ( IS_NPC( ch ) )
    	    return;

        act("You begin hunting $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
        if (!IS_SITH(ch) || !IS_SET(ch->parts, PART_LONG_TONGUE))
            act("$n poises $mself stealthily and scans the area.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
        else
            act("$n poises $mself stealthily and sniffs the air.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
        ch->hunting = victim;
        return;
    }

    if ( direction < 0 || direction >= MAX_DIR )
    {
    	send_to_char( "Hmm... Something seems to be wrong.\n\r", ch );
    	return;
    }

    if (!IS_NPC(ch))
    {
        int skill = (IS_SITH(ch) && IS_SET(ch->parts, PART_LONG_TONGUE)) ? 100 : get_skill(ch, gsk_hunt);
        if (number_percent() > skill)
        {
            send_to_char("You can't find the trail.\n\r", ch);
            return;
        }
    }

    /*
     * Display the results of the search.
     */
    act("$N is $t from here.", ch, victim, NULL, NULL, NULL, dir_name[direction], NULL, TO_CHAR );
    check_improve(ch,gsk_hunt,true,1);
}


CHAR_DATA *get_char_area( CHAR_DATA *ch, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *ach;
    int number,count;
    ITERATOR it;

    if (ch->in_room == NULL)
    	return NULL;

    if ( (ach = get_char_room( ch, NULL, argument )) != NULL )
		return ach;

    number = number_argument( argument, arg );
    count = 0;
    iterator_start(&it, loaded_chars);
    while(( ach = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		if (ach->in_room == NULL ||
			ach->in_room->area != ch->in_room->area ||
			!can_see( ch, ach ) || !is_name( arg, ach->name ))
			continue;
		if (++count == number)
	    	break;
    }
    iterator_stop(&it);

    return ach;
}
