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
#include "traits.h"

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
#define HUNT_WILDS_MAX_RANGE 60

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

void destroy_hash_table(struct hash_header *ht,void (*gman)(void *))
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

      for(i=0;i<ht->klistlen;i++) {
        if(ht->keylist[i]==key)
          break;
      }

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


void room_iterate(ROOM_INDEX_DATA *rb[],void (*func)(int, ROOM_INDEX_DATA *, void *),void *cdata)
{
    register int i;

    for( i = 0; i < WORLD_SIZE; i++ )
    {
    ROOM_INDEX_DATA *temp;

    temp = room_find(rb,i);
    if(temp) (*func)(i,temp,cdata);
    }
}


void hash_iterate(struct hash_header *ht,void (*func)(int, ROOM_INDEX_DATA *, void *),void *cdata)
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


void donothing( void *unused )
{
    (void)unused;
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

    AREA_DATA *start_area = NULL;
    WNUM start_wnum;
    if (resolve_widevnum(in_room_vnum, NULL, &start_wnum))
        start_area = start_wnum.pArea;
    if (!start_area) start_area = get_system_area_fallback();
    startp = get_room_index( start_area, in_room_vnum );

    init_hash_table( &x_room, sizeof(int), 2048 );
    hash_enter( &x_room, in_room_vnum, (void *) - 1 );

    /* initialize queue */
    q_head = (struct room_q *) malloc(sizeof(struct room_q));
    q_tail = q_head;
    q_tail->room_nr = in_room_vnum;
    q_tail->next_q = 0;

    while(q_head)
    {
    AREA_DATA *here_area = NULL;
    WNUM here_wnum;
    if (resolve_widevnum(q_head->room_nr, NULL, &here_wnum))
        here_area = here_wnum.pArea;
    if (!here_area) here_area = get_system_area_fallback();
    herep = get_room_index( here_area, q_head->room_nr );
    /* for each room test all directions */
    if( herep->area == startp->area || !in_zone )
    {
        /* only look in this zone...
           saves cpu time and  makes world safer for players  */
        for( i = 0; i < MAX_DIR; i++ )
        {
        exitp = herep->exit[i];
        if( exit_ok(exitp) && ( thru_doors ? GO_OK_SMARTER : !IS_SET(exitp->exit_info, EX_CLOSED) ) )
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
                void *ancestor = hash_find(&x_room, q_head->room_nr);
                hash_enter( &x_room, tmp_room,
                    (ancestor == (void*)-1) ?
                    (void*)(size_t)(i+1) :
                    ancestor);
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

static int hunt_scope_rank(CHAR_DATA *hunter, CHAR_DATA *candidate)
{
    ROOM_INDEX_DATA *hunter_room;
    ROOM_INDEX_DATA *candidate_room;
    INSTANCE *hunter_instance;
    INSTANCE *candidate_instance;
    AREA_REGION *hunter_region;
    AREA_REGION *candidate_region;

    if (!hunter || !candidate)
        return 100;

    hunter_room = hunter->in_room;
    candidate_room = candidate->in_room;
    if (!hunter_room || !candidate_room)
        return 100;

    if (hunter_room == candidate_room)
        return 0;

    hunter_instance = get_room_instance(hunter_room);
    candidate_instance = get_room_instance(candidate_room);
    if (IS_VALID(hunter_instance) || IS_VALID(candidate_instance))
    {
        if (hunter_instance == candidate_instance)
            return 1;

        return 90;
    }

    if (IN_WILDERNESS(candidate))
        return 80;

    hunter_region = get_room_region(hunter_room);
    candidate_region = get_room_region(candidate_room);
    if (hunter_region && candidate_region && hunter_region == candidate_region)
        return 2;

    if (hunter_room->area && candidate_room->area && hunter_room->area == candidate_room->area)
        return 3;

    return 4;
}

static CHAR_DATA *get_hunt_target(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    ITERATOR it;
    int number;
    int rank;
    int counts[5] = { 0, 0, 0, 0, 0 };
    CHAR_DATA *selected[5] = { NULL, NULL, NULL, NULL, NULL };

    if (!ch || !ch->in_room || !argument || argument[0] == '\0')
        return NULL;

    victim = get_char_room(ch, NULL, argument);
    if (victim)
        return victim;

    number = number_argument(argument, arg);

    iterator_start(&it, loaded_chars);
    while ((victim = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        int rank;

        if (victim->in_room == NULL)
            continue;

        if (!can_see(ch, victim))
            continue;

        if (!is_name(arg, victim->name))
            continue;

        rank = hunt_scope_rank(ch, victim);
        if (rank < 0 || rank > 4)
            continue;

        if (++counts[rank] == number)
            selected[rank] = victim;
    }
    iterator_stop(&it);

    for (rank = 0; rank < 5; rank++)
    {
        if (selected[rank])
            return selected[rank];
    }

    return NULL;
}

static int hunt_wilds_distance(ROOM_INDEX_DATA *from_room, ROOM_INDEX_DATA *to_room)
{
    int dx;
    int dy;
    int adx;
    int ady;

    if (!from_room || !to_room)
        return -1;

    if (!from_room->wilds || !to_room->wilds)
        return -1;

    if (from_room->wilds != to_room->wilds)
        return -1;

    dx = to_room->x - from_room->x;
    dy = to_room->y - from_room->y;
    adx = abs(dx);
    ady = abs(dy);

    return (adx > ady) ? adx : ady;
}

static int hunt_wilds_direction(ROOM_INDEX_DATA *from_room, ROOM_INDEX_DATA *to_room)
{
    int dx;
    int dy;

    if (!from_room || !to_room)
        return -1;

    dx = to_room->x - from_room->x;
    dy = to_room->y - from_room->y;

    if (dx == 0 && dy == 0)
        return -1;

    if (dx > 0 && dy < 0) return DIR_NORTHEAST;
    if (dx < 0 && dy < 0) return DIR_NORTHWEST;
    if (dx > 0 && dy > 0) return DIR_SOUTHEAST;
    if (dx < 0 && dy > 0) return DIR_SOUTHWEST;
    if (dx > 0) return DIR_EAST;
    if (dx < 0) return DIR_WEST;
    if (dy < 0) return DIR_NORTH;
    return DIR_SOUTH;
}

static bool hunt_can_track_between_instances(INSTANCE *hunter_instance, INSTANCE *victim_instance)
{
    if (!IS_VALID(hunter_instance) && !IS_VALID(victim_instance))
        return true;

    if (!IS_VALID(hunter_instance) || !IS_VALID(victim_instance))
        return false;

    if (hunter_instance == victim_instance)
        return true;

    if (IS_VALID(hunter_instance->dungeon) && hunter_instance->dungeon == victim_instance->dungeon)
        return true;

    return false;
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
    && (!ch->race || str_cmp(ch->race->id, "sith"))
    && get_skill(ch,skill_resolve_gsn("hunt")) == 0 )
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

    if (is_dead(ch))
        return;

    if( arg[0] == '\0' )
    {
    send_to_char( "Whom are you trying to hunt?\n\r", ch );
    return;
    }

    if ( !str_cmp( arg2, "auto" ) )
    fAuto = true;

    victim = get_hunt_target(ch, arg);
    if (victim == NULL)
    {
        send_to_char("No-one around by that name.\n\r", ch );
        return;
    }

    if ( !can_hunt( ch, victim ) )
    {
    act("$N has magically covered $S tracks.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    if ( ch->in_room == victim->in_room )
    {
    act( "$N is here!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
    return;
    }

    INSTANCE *ch_instance = get_room_instance(ch->in_room);
    INSTANCE *victim_instance = get_room_instance(victim->in_room);

    if (!hunt_can_track_between_instances(ch_instance, victim_instance))
    {
        send_to_char("You cannot pick up a trail into another instance.\n\r", ch);
        return;
    }

    if (IN_WILDERNESS(ch) || IN_WILDERNESS(victim))
    {
        int wilds_distance;

        if (!IN_WILDERNESS(ch) || !IN_WILDERNESS(victim))
        {
            act("You can only track wilderness targets while both of you are in the wilderness.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        wilds_distance = hunt_wilds_distance(ch->in_room, victim->in_room);
        if (wilds_distance < 0)
        {
            send_to_char("The trail does not cross that wilderness boundary.\n\r", ch);
            return;
        }

        if (wilds_distance > HUNT_WILDS_MAX_RANGE)
        {
            send_to_char("The trail is too faint to follow at this distance.\n\r", ch);
            return;
        }
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
    if (get_skill( victim, skill_resolve_gsn("trackless step") ) > 0
    //&& victim->pcdata->second_sub_class_cleric == CLASS_CLERIC_RANGER
        && ( room_in_sector(victim->in_room, SECT_FIELD)
            || room_in_sector(victim->in_room, SECT_FOREST)
            || room_in_sector(victim->in_room, SECT_HILLS)
            || room_in_sector(victim->in_room, SECT_MOUNTAIN)
            || room_in_sector(victim->in_room, SECT_TUNDRA) ) )
    {
    if ( number_percent() < get_skill( victim, skill_resolve_gsn("trackless step") ) )
    {
        act("$N has covered $S tracks too well for you to follow.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }
    }

    if (race_get_trait_string(ch->race, "scent_track_flavor"))
    act("$n's forked tongue whips out and tastes the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    else
    act( "$n carefully sniffs the air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );


    if (IN_WILDERNESS(ch) && IN_WILDERNESS(victim))
        direction = hunt_wilds_direction(ch->in_room, victim->in_room);
    else
        direction = find_path( ch->in_room->vnum, victim->in_room->vnum,
            ch, -1000, false );

    if( direction == -1 || (IS_NPC(victim) && IS_SET(victim->act[1], ACT2_NO_HUNT)))
    {
    act("You couldn't find a path to $N from here.\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    // Auto-Hunt ?
    if ( fAuto )
    {
    if ( IS_NPC( ch ) )
        return;

    act("You begin hunting $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
    act("$n poises $mself stealthily and sniffs the air.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    ch->hunting = victim;
    return;
    }

    if ( direction < 0 || direction > 9 )
    {
    send_to_char( "Hmm... Something seems to be wrong.\n\r", ch );
    return;
    }

    if (!IS_NPC(ch))
    {
        int16_t sn_hunt = skill_resolve_gsn("hunt");
        int hunt_skill = get_skill(ch, sn_hunt);

        if (number_percent() > (race_get_trait_bool(ch->race, "scent_tracking") ? 100 : hunt_skill))
        {
            send_to_char("You can't find the trail.\n\r", ch);
            return;
        }
    }

    /*
     * Display the results of the search.
     */
    act("$N is $t from here.", ch, victim, NULL, NULL, NULL, dir_name[direction], NULL, TO_CHAR, NULL, NULL );
    check_improve(ch,skill_resolve_gsn("hunt"),true,1);
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
