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

struct hash_link
{
    int			key;
    struct hash_link	*next;
    void		*data;
};

struct hash_header
{
    int			rec_size;
    int			table_size;
    int			*keylist, klistsize, klistlen; /* this is really lame,
							  AMAZINGLY lame */
    struct hash_link	**buckets;
};

#define WORLD_SIZE	30000
#define	HASH_KEY(ht,key)((((unsigned int)(key))*17)%(ht)->table_size)

struct hunting_data
{
    char *name;
    struct char_data	**victim;
};

struct room_q
{
    int		room_nr;
    struct room_q	*next_q;
};

struct nodes
{
    int	visited;
    int	ancestor;
};

#define IS_DIR		(get_room_index(q_head->room_nr)->exit[i])
#define GO_OK		(!IS_SET( IS_DIR->exit_info, EX_CLOSED ))
#define GO_OK_SMARTER	1

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
    ht->keylist	= (void*)malloc(sizeof(ht->keylist)*(ht->klistsize=128));
    ht->klistlen	= 0;
}

void init_world(ROOM_INDEX_DATA *room_db[])
{
    /* zero out the world */
    bzero((char *)room_db,sizeof(ROOM_INDEX_DATA *)*WORLD_SIZE);
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

void _hash_enter(struct hash_header *ht,int key,void *data)
{
    /* precondition: there is no entry for <key> yet */
    struct hash_link	*temp;
    int			i;

    temp		= (struct hash_link *)malloc(sizeof(struct hash_link));
  temp->key	= key;
  temp->next	= ht->buckets[HASH_KEY(ht,key)];
  temp->data	= data;
  ht->buckets[HASH_KEY(ht,key)] = temp;
  if(ht->klistlen>=ht->klistsize)
    {
      ht->keylist = (void*)realloc(ht->keylist,sizeof(*ht->keylist)*
				   (ht->klistsize*=2));
    }
  for(i=ht->klistlen;i>=0;i--)
    {
      if(ht->keylist[i-1]<key)
	{
	  ht->keylist[i] = key;
	  break;
	}
      ht->keylist[i] = ht->keylist[i-1];
    }
  ht->klistlen++;
}

ROOM_INDEX_DATA *room_find(ROOM_INDEX_DATA *room_db[],int key)
{
  return((key<WORLD_SIZE&&key>-1)?room_db[key]:0);
}

void *hash_find(struct hash_header *ht,int key)
{
  struct hash_link *scan;

  scan = ht->buckets[HASH_KEY(ht,key)];

  while(scan && scan->key!=key)
    scan = scan->next;

  return scan ? scan->data : NULL;
}

int room_enter(ROOM_INDEX_DATA *rb[],int key,ROOM_INDEX_DATA *rm)
{
  ROOM_INDEX_DATA *temp;

  temp = room_find(rb,key);
  if(temp) return(0);

  rb[key] = rm;
  return(1);
}

int hash_enter(struct hash_header *ht,int key,void *data)
{
    void *temp;

    temp = hash_find(ht,key);
    if(temp) return 0;

    _hash_enter(ht,key,data);
    return 1;
}


ROOM_INDEX_DATA *room_find_or_create(ROOM_INDEX_DATA *rb[],int key)
{
    ROOM_INDEX_DATA *rv;

    rv = room_find(rb,key);
    if(rv) return rv;

    rv = (ROOM_INDEX_DATA *)malloc(sizeof(ROOM_INDEX_DATA));
    rb[key] = rv;

    return rv;
}


void *hash_find_or_create(struct hash_header *ht,int key)
{
    void *rval;

    rval = hash_find(ht, key);
    if(rval) return rval;

    rval = (void*)malloc(ht->rec_size);
    _hash_enter(ht,key,rval);

    return rval;
}


int room_remove(ROOM_INDEX_DATA *rb[],int key)
{
    ROOM_INDEX_DATA *tmp;

    tmp = room_find(rb,key);
    if(tmp)
    {
	rb[key] = 0;
	free(tmp);
    }
    return(0);
}


void *hash_remove(struct hash_header *ht,int key)
{
  struct hash_link **scan;

  scan = ht->buckets+HASH_KEY(ht,key);

  while(*scan && (*scan)->key!=key)
    scan = &(*scan)->next;

  if(*scan)
    {
      int		i;
      struct hash_link	*temp, *aux;

      temp	= (*scan)->data;
      aux	= *scan;
      *scan	= aux->next;
      free(aux);

      for(i=0;i<ht->klistlen;i++)
	if(ht->keylist[i]==key)
	  break;

      if(i<ht->klistlen)
	{
	  bcopy((char *)ht->keylist+i+1,(char *)ht->keylist+i,(ht->klistlen-i)
		*sizeof(*ht->keylist));
	  ht->klistlen--;
	}

      return temp;
    }

  return NULL;
}


void room_iterate(ROOM_INDEX_DATA *rb[],void (*func)(),void *cdata)
{
    register int i;

    for( i = 0; i < WORLD_SIZE; i++ )
    {
	ROOM_INDEX_DATA *temp;

	temp = room_find(rb,i);
	if(temp) (*func)(i,temp,cdata);
    }
}


void hash_iterate(struct hash_header *ht,void (*func)(),void *cdata)
{
    int i;

    for( i = 0 ; i < ht->klistlen; i++ )
    {
	void		*temp;
	register int	key;

	key = ht->keylist[i];
	temp = hash_find(ht,key);
	(*func)(key,temp,cdata);
	if(ht->keylist[i]!=key) /* They must have deleted this room */
	    i--;		      /* Hit this slot again. */
    }
}


int exit_ok( EXIT_DATA *pexit )
{
  ROOM_INDEX_DATA *to_room;

  return ( pexit && (to_room = pexit->u1.to_room ) && !IS_SET(pexit->exit_info,EX_NOHUNT) );
}


void donothing( void )
{
    return;
}


int find_path( long in_room_vnum, long out_room_vnum, CHAR_DATA *ch,
	       int depth, int in_zone )
{
    struct room_q *tmp_q, *q_head, *q_tail;
    struct hash_header	x_room;
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
    {
	thru_doors = false;
    }

    startp = get_room_index( in_room_vnum );

    init_hash_table( &x_room, sizeof(int), 2048 );
    hash_enter( &x_room, in_room_vnum, (void *) - 1 );

    /* initialize queue */
    q_head = (struct room_q *) malloc(sizeof(struct room_q));
    q_tail = q_head;
    q_tail->room_nr = in_room_vnum;
    q_tail->next_q = 0;

    while(q_head)
    {
	herep = get_room_index( q_head->room_nr );
	/* for each room test all directions */
	if( herep->area == startp->area || !in_zone )
	{
	    /* only look in this zone...
	       saves cpu time and  makes world safer for players  */
	    for( i = 0; i < MAX_DIR; i++ )
	    {
		exitp = herep->exit[i];
		if( exit_ok(exitp) && ( thru_doors ? GO_OK_SMARTER : GO_OK ) )
		{
		    /* next room */
		    tmp_room = herep->exit[i]->u1.to_room->vnum;
		    if( tmp_room != out_room_vnum )
		    {
			/* shall we add room to queue ?
			   count determines total breadth and depth */
			if( !hash_find( &x_room, tmp_room )
				&& ( count < depth ) )
			    /* && !IS_SET( RM_FLAGS(tmp_room), DEATH ) ) */
			{
			    count++;
			    /* mark room as visted and put on queue */

			    tmp_q = (struct room_q *)
				malloc(sizeof(struct room_q));
			    tmp_q->room_nr = tmp_room;
			    tmp_q->next_q = 0;
			    q_tail->next_q = tmp_q;
			    q_tail = tmp_q;

			    /* ancestor for first layer is the direction */
			    hash_enter( &x_room, tmp_room,
				    (hash_find(&x_room,q_head->room_nr) == (void*)-1) ?
				    (void*)(size_t)(i+1) :
				    hash_find(&x_room,q_head->room_nr));
			}
		    }
		    else
		    {
			/* have reached our goal so free queue */
			tmp_room = q_head->room_nr;
			for(;q_head;q_head = tmp_q)
			{
			    tmp_q = q_head->next_q;
			    free(q_head);
			}
			/* return direction if first layer */
			if (hash_find(&x_room,tmp_room)==(void *)-1)
			{
			    if (x_room.buckets)
			    {
				/* junk left over from a previous track */
				destroy_hash_table(&x_room, donothing);
			    }
			    return(i);
			}
			else
			{
			    /* else return the ancestor */
			    int i;

			    i = (int)(size_t)hash_find(&x_room,tmp_room);
			    if (x_room.buckets)
			    {
				/* junk left over from a previous track */
				destroy_hash_table(&x_room, donothing);
			    }
			    return( -1+i);
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

    if (!IS_NPC(ch)
    && str_cmp(race_table[ch->race].name, "sith")
    && get_skill(ch,gsn_hunt) == 0 )
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
    if (get_skill( victim, gsn_trackless_step ) > 0
    //&& victim->pcdata->second_sub_class_cleric == CLASS_CLERIC_RANGER
    && ( victim->in_room->sector_type == SECT_FIELD
         || victim->in_room->sector_type == SECT_FOREST
         || victim->in_room->sector_type == SECT_HILLS
         || victim->in_room->sector_type == SECT_MOUNTAIN
         || victim->in_room->sector_type == SECT_TUNDRA ) )
    {
	if ( number_percent() < get_skill( victim, gsn_trackless_step ) )
	{
	    act("$N has covered $S tracks too well for you to follow.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    return;
	}
    }

    if (!IS_SITH(ch))
	act( "$n carefully sniffs the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
    else
	act("$n's forked tongue whips out and tastes the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);


    // Max rooms so people can track across areas without megalag
    direction = find_path( ch->in_room->vnum, victim->in_room->vnum,
	    ch, -1000, false );

    if( direction == -1 || (IS_NPC(victim) && IS_SET(victim->act2, ACT2_NO_HUNT)))
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
	act("$n poises $mself stealthily and sniffs the air.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	ch->hunting = victim;
	return;
    }

    if ( direction < 0 || direction > 9 )
    {
	send_to_char( "Hmm... Something seems to be wrong.\n\r", ch );
	return;
    }

    if (!IS_NPC(ch) && number_percent() > (IS_SITH(ch) ? 100 : ch->pcdata->learned[gsn_hunt]))
    {
	send_to_char("You can't find the trail.\n\r", ch);
	return;
    }

    /*
     * Display the results of the search.
     */
    act("$N is $t from here.", ch, victim, NULL, NULL, NULL, dir_name[direction], NULL, TO_CHAR );
    check_improve(ch,gsn_hunt,true,1);
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
