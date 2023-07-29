/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "scripts.h"
#include "wilds.h"


bool string_argremove_phrase(char *src, char *phrase, char *buf);
bool string_argremove_index(char *src, int argindex, char *buf);

// Prototypes - obviously
pVARIABLE variable_new(void);					// Most likely just INTERNAL
void variable_freedata (pVARIABLE v);
void variable_add(ppVARIABLE list,pVARIABLE var);		// INTERNAL
pVARIABLE variable_create(ppVARIABLE list,char *name, bool index, bool clear);	// INTERNAL

pVARIABLE variable_head = NULL;
pVARIABLE variable_tail = NULL;
pVARIABLE variable_index_head = NULL;
pVARIABLE variable_index_tail = NULL;
pVARIABLE variable_freechain = NULL;

bool variable_validname(char *str)
{
	if(!str || !*str) return FALSE;

	while(*str) {
		if(!isprint(*str) || *str == '~') return FALSE;
		++str;
	}

	return TRUE;
}

pVARIABLE variable_new(void)
{
	pVARIABLE v;

	if (variable_freechain) {
		v = variable_freechain;
		variable_freechain = variable_freechain->next;
	} else
		v = (pVARIABLE)malloc(sizeof(VARIABLE));

	if(v) memset(v,0,sizeof(VARIABLE));

	return v;
}

static void *deepcopy_string(void *str)
{
	return str_dup((char *)str);
}

static void deleter_string(void *str) { free_string((char *)str); }

static void *deepcopy_room(void *src)
{
	LLIST_ROOM_DATA *room = (LLIST_ROOM_DATA *)src;

	LLIST_ROOM_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_ROOM_DATA))) )
	{
		data->room = room->room;
		data->id[0] = room->id[0];
		data->id[1] = room->id[1];
		data->id[2] = room->id[2];
		data->id[3] = room->id[3];
		data->id[4] = room->id[4];
	}

	return data;
}

static void *deepcopy_uid(void *src)
{
	LLIST_UID_DATA *uid = (LLIST_UID_DATA *)src;

	LLIST_UID_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_UID_DATA))) )
	{
		data->ptr = uid->ptr;
		data->id[0] = uid->id[0];
		data->id[1] = uid->id[1];
	}

	return data;
}

static void *deepcopy_exit(void *src)
{
	LLIST_EXIT_DATA *ex = (LLIST_EXIT_DATA *)src;

	LLIST_EXIT_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_EXIT_DATA))) )
	{
		data->room = ex->room;
		data->id[0] = ex->id[0];
		data->id[1] = ex->id[1];
		data->id[2] = ex->id[2];
		data->id[3] = ex->id[3];
		data->door = ex->door;
	}

	return data;
}

static void *deepcopy_skill(void *src)
{
	LLIST_SKILL_DATA *skill = (LLIST_SKILL_DATA *)src;

	LLIST_SKILL_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_SKILL_DATA))) )
	{
		data->mob = skill->mob;
		data->sn = skill->sn;
		data->tok = skill->tok;
		data->mid[0] = skill->mid[0];
		data->mid[1] = skill->mid[1];
		data->tid[0] = skill->tid[0];
		data->tid[1] = skill->tid[1];
	}

	return data;
}

static void *deepcopy_area(void *src)
{
	LLIST_AREA_DATA *area = (LLIST_AREA_DATA *)src;

	LLIST_AREA_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_AREA_DATA))) )
	{
		data->area = area->area;
		data->uid = area->uid;
	}

	return data;
}

static void *deepcopy_area_region(void *src)
{
	LLIST_AREA_REGION_DATA *region = (LLIST_AREA_REGION_DATA *)src;

	LLIST_AREA_REGION_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_AREA_REGION_DATA))) )
	{
		data->aregion = region->aregion;
		data->aid = region->aid;
		data->rid = region->rid;
	}

	return data;
}

static void *deepcopy_wilds(void *src)
{
	LLIST_WILDS_DATA *wilds = (LLIST_WILDS_DATA *)src;

	LLIST_WILDS_DATA *data;

	if( (data = alloc_mem(sizeof(LLIST_WILDS_DATA))) )
	{
		data->wilds = wilds->wilds;
		data->uid = wilds->uid;
	}

	return data;
}


static LISTCOPY_FUNC *__var_blist_copier[] = {
	NULL,
	deepcopy_room,
	deepcopy_uid,
	deepcopy_uid,
	deepcopy_uid,
	deepcopy_exit,
	deepcopy_skill,
	deepcopy_area,
	deepcopy_area_region,
	deepcopy_wilds,
	NULL
};

static void deleter_room(void *data) { free_mem(data, sizeof(LLIST_ROOM_DATA)); }
static void deleter_uid(void *data) { free_mem(data, sizeof(LLIST_UID_DATA)); }
static void deleter_exit(void *data) { free_mem(data, sizeof(LLIST_EXIT_DATA)); }
static void deleter_skill(void *data) { free_mem(data, sizeof(LLIST_SKILL_DATA)); }
static void deleter_area(void *data) { free_mem(data, sizeof(LLIST_AREA_DATA)); }
static void deleter_area_region(void *data) { free_mem(data, sizeof(LLIST_AREA_REGION_DATA)); }
static void deleter_wilds(void *data) { free_mem(data, sizeof(LLIST_WILDS_DATA)); }

static LISTDESTROY_FUNC *__var_blist_deleter[] = {
	NULL,
	deleter_room,
	deleter_uid,
	deleter_uid,
	deleter_uid,
	deleter_exit,
	deleter_skill,
	deleter_area,
	deleter_area_region,
	deleter_wilds,
	NULL
};

/*
static int __var_blist_size[] = {
	0,
	sizeof(LLIST_ROOM_DATA),
	sizeof(LLIST_UID_DATA),
	sizeof(LLIST_UID_DATA),
	sizeof(LLIST_UID_DATA),
	sizeof(LLIST_EXIT_DATA),
	sizeof(LLIST_SKILL_DATA),
	sizeof(LLIST_AREA_DATA),
	sizeof(LLIST_WILDS_DATA),
	0
};
*/

void variable_freedata (pVARIABLE v)
{
	//ITERATOR it;

	variable_clearfield(VAR_VARIABLE, v);

	switch( v->type ) {
	case VAR_STRING:
		if( v->_.s ) free_string(v->_.s);
		break;

	/*
	// This should be handled by the list_destroy itself
	case VAR_PLLIST_STR:
		// Strings needs to be freed, if they are not shared strings
		if( v->_.list ) {
			char *str;
			iterator_start(&it, v->_.list);

			while((str = (char*)iterator_nextdata(&it)))
				free_string(str);

			iterator_stop(&it);
		}
		break;

	// These have special structures for referencing them
	case VAR_BLLIST_ROOM:
	case VAR_BLLIST_MOB:
	case VAR_BLLIST_OBJ:
	case VAR_BLLIST_TOK:
	case VAR_BLLIST_EXIT:
	case VAR_BLLIST_SKILL:
	case VAR_BLLIST_AREA:
	case VAR_BLLIST_WILDS:
		if( v->_.list ) {
			void *ptr;
			iterator_start(&it, v->_.list);

			while((ptr = iterator_nextdata(&it)))
				free_mem(ptr, __var_blist_size[v->type - VAR_BLLIST_FIRST]);

			iterator_stop(&it);
		}
		break;
	*/
	}


	// For all list variables, just remove the reference added when created, this will autopurge the list
	// 20140511 NIB - nope, the use of the purge would not allow for list culling
	if (v->type >= VAR_BLLIST_FIRST && v->type <= VAR_BLLIST_LAST && v->_.list ) {
//		list_remref(v->_.list);
		list_destroy(v->_.list);
		v->_.list = NULL;
	}

	if (v->type >= VAR_PLLIST_FIRST && v->type <= VAR_PLLIST_LAST && v->_.list ) {
//		list_remref(v->_.list);
		list_destroy(v->_.list);
		v->_.list = NULL;
	}
}

void variable_free (pVARIABLE v)
{
	if(!v) return;

	variable_freedata(v);

	free_string(v->name);

	if(v->index) {
		if(v->global_prev) v->global_prev->global_next = v->global_next;
		else variable_index_head = v->global_next;
		if(variable_index_tail == v) variable_index_tail = v->global_prev;
	} else {
		if(v->global_prev) v->global_prev->global_next = v->global_next;
		else variable_head = v->global_next;
		if(variable_tail == v) variable_tail = v->global_prev;
	}
	if(v->global_next) v->global_next->global_prev = v->global_prev;

	v->global_prev = NULL;
	v->global_next = NULL;
	v->type = VAR_UNKNOWN;
	v->next = variable_freechain;
	variable_freechain = v;
}

void variable_freelist(ppVARIABLE list)
{
	pVARIABLE cur,next;

	for(cur = *list;cur;cur = next) {
		next = cur->next;
		variable_free(cur);
	}

	*list = NULL;
}

pVARIABLE variable_get(pVARIABLE list,char *name)
{
	for(;list;list = list->next) {
		if(!str_cmp(list->name,name)) break;
	}
	return list;
}

void variable_add(ppVARIABLE list,pVARIABLE var)
{
	register pVARIABLE cur;

	if(!*list)
		*list = var;
	else if(str_cmp((*list)->name,var->name) > 0) {
		var->next = *list;
		*list = var;
	} else {
		for(cur = *list; cur->next && str_cmp(cur->next->name,var->name) < 0;cur = cur->next);

		var->next = cur->next;
		cur->next = var;
	}
}

/*
static void variable_dump_list(pVARIABLE list, char *prefix)
{
	while(list) {
		log_stringf("%s: %s", prefix, list->name);

		list = list->global_next;
	}
}
*/

pVARIABLE variable_alloc(char *name, bool index)
{
	pVARIABLE var;
	var = variable_new();
	if(var) {
		var->name = str_dup(name);
		var->save = FALSE;
		var->index = index;

		if(index) {
			if(variable_index_tail) variable_index_tail->global_next = var;
			else variable_index_head = var;
			var->global_prev = variable_index_tail;
			variable_index_tail = var;
		} else {
			if(variable_tail) variable_tail->global_next = var;
			else variable_head = var;
			var->global_prev = variable_tail;
			variable_tail = var;
		}
	}

	return var;
}

pVARIABLE variable_create(ppVARIABLE list,char *name, bool index, bool clear)
{
	pVARIABLE var;

	var = variable_get(*list,name);
	if(var) {
		if(clear) variable_freedata(var);
	} else {
		var = variable_alloc(name, index);
		variable_add(list,var);
	}

	return var;
}

#define varset(n,c,t,v,f) \
bool variables_setsave_##n (ppVARIABLE list,char *name,t v, bool save) \
{ \
	pVARIABLE var = variable_create(list,name,FALSE,TRUE); \
 \
	if(!var) return FALSE; \
 \
	var->type = VAR_##c; \
	if( save != TRISTATE ) \
		var->save = save; \
	var->_.f = v; \
 \
	return TRUE; \
} \
 \
bool variables_set_##n (ppVARIABLE list,char *name,t v) \
{ \
	return variables_setsave_##n (list, name, v, TRISTATE); \
} \


varset(boolean,BOOLEAN,bool,boolean,boolean)
varset(integer,INTEGER,int,num,i)
varset(room,ROOM,ROOM_INDEX_DATA*,r,r)
varset(mobile,MOBILE,CHAR_DATA*,m,m)
varset(object,OBJECT,OBJ_DATA*,o,o)
varset(token,TOKEN,TOKEN_DATA*,t,t)
varset(area,AREA,AREA_DATA*,a,a)
varset(area_region,AREA_REGION,AREA_REGION*,ar,ar)
varset(wilds,WILDS,WILDS_DATA*,wilds,wilds)
varset(church,CHURCH,CHURCH_DATA*,church,church)
varset(affect,AFFECT,AFFECT_DATA*,aff,aff)
varset(food_buff,FOOD_BUFF,FOOD_BUFF_DATA*,food_buff,food_buff)
varset(compartment,COMPARTMENT,FURNITURE_COMPARTMENT*,compartment,compartment)
varset(variable,VARIABLE,pVARIABLE,v,variable)
varset(instance_section,SECTION,INSTANCE_SECTION *,section,section)
varset(instance,INSTANCE,INSTANCE *,instance,instance)
varset(dungeon,DUNGEON,DUNGEON *,dungeon,dungeon)
varset(ship,SHIP,SHIP_DATA *,ship,ship)
varset(mobindex,MOBINDEX,MOB_INDEX_DATA *,m,mindex)
varset(objindex,OBJINDEX,OBJ_INDEX_DATA *,o,oindex)
varset(tokenindex,TOKENINDEX,TOKEN_INDEX_DATA *,t,tindex)
varset(blueprint_section,BLUEPRINT_SECTION,BLUEPRINT_SECTION *,bs,bs)
varset(blueprint,BLUEPRINT,BLUEPRINT *,bp,bp)
varset(dungeonindex,DUNGEONINDEX,DUNGEON_INDEX_DATA *,dng,dngindex)
varset(shipindex,SHIPINDEX,SHIP_INDEX_DATA *,ship,shipindex)
varset(mobindex_load,MOBINDEX,WNUM_LOAD,m,wnum_load)
varset(objindex_load,OBJINDEX,WNUM_LOAD,o,wnum_load)
varset(tokenindex_load,TOKENINDEX,WNUM_LOAD,t,wnum_load)
varset(blueprint_section_load,BLUEPRINT_SECTION,WNUM_LOAD,bs,wnum_load)
varset(blueprint_load,BLUEPRINT,WNUM_LOAD,bp,wnum_load)
varset(dungeonindex_load,DUNGEONINDEX,WNUM_LOAD,dng,wnum_load)
varset(shipindex_load,SHIPINDEX,WNUM_LOAD,ship,wnum_load)

bool variables_set_dice (ppVARIABLE list,char *name,DICE_DATA *d)
{
	return variables_setsave_dice (list, name, d, TRISTATE);
}

bool variables_setsave_dice (ppVARIABLE list,char *name,DICE_DATA *d, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_DICE;
	if( save != TRISTATE )
		var->save = save;
	var->_.dice.number = d->number;
	var->_.dice.size = d->size;
	var->_.dice.bonus = d->bonus;
	var->_.dice.last_roll = d->last_roll;

	return TRUE;
}

bool variables_set_door (ppVARIABLE list,char *name, ROOM_INDEX_DATA *room, int door, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_EXIT;
	if( save != TRISTATE )
		var->save = save;
	var->_.door.r = room;
	var->_.door.door = door;

	return TRUE;
}


bool variables_set_exit (ppVARIABLE list,char *name, EXIT_DATA *ex)
{
	if( !ex || !ex->from_room) return FALSE;

	return variables_set_door( list, name, ex->from_room, ex->orig_door, TRISTATE );
}

bool variables_setsave_exit (ppVARIABLE list,char *name, EXIT_DATA *ex, bool save)
{
	if( !ex || !ex->from_room) return FALSE;

	return variables_set_door( list, name, ex->from_room, ex->orig_door, save);
}


bool variables_set_skill (ppVARIABLE list,char *name,int sn)
{
	return variables_setsave_skill( list, name, sn, TRISTATE );
}

bool variables_setsave_skill (ppVARIABLE list,char *name,int sn, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SKILL;
	if( save != TRISTATE )
		var->save = save;
	var->_.sn =  (sn > 0 && sn < MAX_SKILL) ? sn : 0;

	return TRUE;
}

bool variables_set_skillinfo (ppVARIABLE list,char *name,CHAR_DATA *owner, int sn, TOKEN_DATA *token)
{
	return variables_setsave_skillinfo( list, name, owner, sn, token, TRISTATE );
}

bool variables_setsave_skillinfo (ppVARIABLE list,char *name,CHAR_DATA *owner, int sn, TOKEN_DATA *token, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SKILLINFO;
	if( save != TRISTATE )
		var->save = save;
	var->_.sk.owner = owner;
	var->_.sk.token = token;
	var->_.sk.sn =  (sn > 0 && sn < MAX_SKILL) ? sn : 0;

	return TRUE;
}

bool variables_set_song (ppVARIABLE list,char *name,int sn)
{
	return variables_setsave_song( list, name, sn, TRISTATE );
}

bool variables_setsave_song (ppVARIABLE list,char *name,int sn, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SONG;
	if( save != TRISTATE )
		var->save = save;
	var->_.sn =  (sn >= 0 && sn < MAX_SONGS) ? sn : -1;

	return TRUE;
}

bool variables_set_mobile_id (ppVARIABLE list,char *name,unsigned long a, unsigned long b, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_MOBILE_ID;
	var->save = save;
	var->_.mid.a = a;
	var->_.mid.b = b;

	return TRUE;
}

bool variables_set_object_id (ppVARIABLE list,char *name,unsigned long a, unsigned long b, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_OBJECT_ID;
	var->save = save;
	var->_.oid.a = a;
	var->_.oid.b = b;

	return TRUE;
}

bool variables_set_token_id (ppVARIABLE list,char *name,unsigned long a, unsigned long b, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_TOKEN_ID;
	var->save = save;
	var->_.tid.a = a;
	var->_.tid.b = b;

	return TRUE;
}

bool variables_set_area_id (ppVARIABLE list,char *name, long aid, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_AREA_ID;
	var->save = save;
	var->_.aid = aid;

	return TRUE;
}

bool variables_set_area_region_id(ppVARIABLE list,char *name,long aid,long rid,bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_AREA_REGION_ID;
	var->save = save;
	var->_.arid.aid = aid;
	var->_.arid.rid = rid;

	return TRUE;
}

bool variables_set_wilds_id (ppVARIABLE list,char *name, long wid, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_WILDS_ID;
	var->save = save;
	var->_.wid = wid;

	return TRUE;
}

bool variables_set_church_id (ppVARIABLE list,char *name, long chid, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_CHURCH_ID;
	var->save = save;
	var->_.chid = chid;

	return TRUE;
}

bool variables_set_clone_room (ppVARIABLE list,char *name, ROOM_INDEX_DATA *source,unsigned long a, unsigned long b, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_CLONE_ROOM;
	var->save = save;
	var->_.cr.r = source;
	var->_.cr.a = a;
	var->_.cr.b = b;

	return TRUE;
}

bool variables_set_wilds_room (ppVARIABLE list,char *name, unsigned long w, int x, int y, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_WILDS_ROOM;
	var->save = save;
	var->_.wroom.wuid = w;
	var->_.wroom.x = x;
	var->_.wroom.y = y;

	return TRUE;
}

bool variables_set_clone_door (ppVARIABLE list,char *name, ROOM_INDEX_DATA *source,unsigned long a, unsigned long b, int door, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_CLONE_DOOR;
	var->save = save;
	var->_.cdoor.r = source;
	var->_.cdoor.a = a;
	var->_.cdoor.b = b;
	var->_.cdoor.door = door;

	return TRUE;
}

bool variables_set_wilds_door (ppVARIABLE list,char *name, unsigned long w, int x, int y, int door, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_WILDS_DOOR;
	var->save = save;
	var->_.wdoor.wuid = w;
	var->_.wdoor.x = x;
	var->_.wdoor.y = y;
	var->_.wdoor.door = door;

	return TRUE;
}

bool variables_set_skillinfo_id (ppVARIABLE list,char *name, unsigned long ma, unsigned long mb, unsigned long ta, unsigned long tb, int sn, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SKILLINFO_ID;
	var->save = save;
	var->_.skid.mid[0] = ma;
	var->_.skid.mid[1] = mb;
	var->_.skid.tid[0] = ta;
	var->_.skid.tid[1] = tb;
	var->_.skid.sn = (sn > 0 && sn < MAX_SKILL) ? sn : 0;

	return TRUE;
}

pVARIABLE variables_set_list (ppVARIABLE list, char *name, int type, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(var)
	{

		var->type = type;
		var->save = save;

		if( type > VAR_BLLIST_FIRST && type < VAR_BLLIST_LAST )
			var->_.list = list_createx(FALSE, __var_blist_copier[type - VAR_BLLIST_FIRST], __var_blist_deleter[type - VAR_BLLIST_FIRST]);

		else if( type == VAR_PLLIST_STR )
			var->_.list = list_createx(FALSE, deepcopy_string, deleter_string);

		else
			var->_.list = list_create(FALSE);


		// 20140511 NIB - the use of the purge flag here would not allow for list culling
		//if(var->_.list)
		//	list_addref(var->_.list);
	}

	return var;
}


bool variables_set_connection (ppVARIABLE list, char *name, DESCRIPTOR_DATA *conn)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_CONNECTION;
	var->save = FALSE;					// These will never save
	var->_.conn = conn;

	return TRUE;
}

bool variables_set_list_str (ppVARIABLE list, char *name, char *str, bool save)
{
	char *cpy;
	pVARIABLE var = variable_get(*list, name);

	if( !str ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_PLLIST_STR,save)) )
			return FALSE;
	} else if( var->type != VAR_PLLIST_STR )
		return FALSE;

	cpy = str_dup(str);
	if( !list_appendlink(var->_.list, cpy) )
		free_string(cpy);

	return TRUE;
}


bool variables_append_list_str (ppVARIABLE list, char *name, char *str)
{
	char *cpy;
	pVARIABLE var = variable_get(*list, name);

	if( !str || !var || var->type != VAR_PLLIST_STR) return FALSE;

	cpy = str_dup(str);
	if( !list_appendlink(var->_.list, cpy) )
		free_string(cpy);

	return TRUE;
}

// Used for loading purposes
static bool variables_append_list_uid (ppVARIABLE list, char *name, int type, unsigned long a, unsigned long b)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( (!a && !b)  || !var || var->type != type) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = NULL;
	data->id[0] = a;
	data->id[1] = b;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_set_list_mob (ppVARIABLE list, char *name, CHAR_DATA *mob, bool save)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(mob) ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_MOB,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_MOB )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = mob;
	data->id[0] = mob->id[0];
	data->id[1] = mob->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_append_list_mob (ppVARIABLE list, char *name, CHAR_DATA *mob)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(mob) || !var || var->type != VAR_BLLIST_MOB) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = mob;
	data->id[0] = mob->id[0];
	data->id[1] = mob->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_set_list_obj (ppVARIABLE list, char *name, OBJ_DATA *obj, bool save)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(obj) ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_OBJ,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_OBJ )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = obj;
	data->id[0] = obj->id[0];
	data->id[1] = obj->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_append_list_obj (ppVARIABLE list, char *name, OBJ_DATA *obj)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(obj) || !var || var->type != VAR_BLLIST_OBJ) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = obj;
	data->id[0] = obj->id[0];
	data->id[1] = obj->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_set_list_token (ppVARIABLE list, char *name, TOKEN_DATA *token, bool save)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(token) ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_TOK,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_TOK )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = token;
	data->id[0] = token->id[0];
	data->id[1] = token->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_append_list_token (ppVARIABLE list, char *name, TOKEN_DATA *token)
{
	LLIST_UID_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(token) || !var || var->type != VAR_BLLIST_TOK) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_UID_DATA))) ) return FALSE;

	data->ptr = token;
	data->id[0] = token->id[0];
	data->id[1] = token->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_UID_DATA));

	return TRUE;
}

bool variables_set_list_area (ppVARIABLE list, char *name, AREA_DATA *area, bool save)
{
	LLIST_AREA_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !area ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_AREA,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_AREA )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_DATA))) ) return FALSE;

	data->area = area;
	data->uid = area->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_DATA));

	return TRUE;
}

bool variables_append_list_area (ppVARIABLE list, char *name, AREA_DATA *area)
{
	LLIST_AREA_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !area || !var || var->type != VAR_BLLIST_AREA) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_DATA))) ) return FALSE;

	data->area = area;
	data->uid = area->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_DATA));

	return TRUE;
}

bool variables_set_list_area_region (ppVARIABLE list, char *name, AREA_REGION *aregion, bool save)
{
	LLIST_AREA_REGION_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(aregion) ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_AREA_REGION,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_AREA_REGION )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_REGION_DATA))) ) return FALSE;

	data->aregion = aregion;
	data->aid = aregion->area->uid;
	data->rid = aregion->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_REGION_DATA));

	return TRUE;
}

bool variables_append_list_area_region (ppVARIABLE list, char *name, AREA_REGION *aregion)
{
	LLIST_AREA_REGION_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !IS_VALID(aregion) || !var || var->type != VAR_BLLIST_AREA_REGION) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_REGION_DATA))) ) return FALSE;

	data->aregion = aregion;
	data->aid = aregion->area->uid;
	data->rid = aregion->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_REGION_DATA));

	return TRUE;
}

bool variables_set_list_wilds (ppVARIABLE list, char *name, WILDS_DATA *wilds, bool save)
{
	LLIST_WILDS_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !wilds ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_WILDS,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_WILDS )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_WILDS_DATA))) ) return FALSE;

	data->wilds = wilds;
	data->uid = wilds->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_WILDS_DATA));

	return TRUE;
}

bool variables_append_list_wilds (ppVARIABLE list, char *name, WILDS_DATA *wilds)
{
	LLIST_WILDS_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !wilds || !var || var->type != VAR_BLLIST_WILDS) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_WILDS_DATA))) ) return FALSE;

	data->wilds = wilds;
	data->uid = wilds->uid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_WILDS_DATA));

	return TRUE;
}

bool variables_set_list_room (ppVARIABLE list, char *name, ROOM_INDEX_DATA *room, bool save)
{
	LLIST_ROOM_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !room ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_BLLIST_ROOM,save)) )
			return FALSE;
	} else if( var->type != VAR_BLLIST_ROOM )
		return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_ROOM_DATA))) ) return FALSE;

	data->room = room;
	if( room->source ) {
		data->id[0] = 0;
		data->id[1] = room->source->area->uid;
		data->id[2] = room->source->vnum;
		data->id[3] = room->id[0];
		data->id[4] = room->id[1];
	} else if( room->wilds ) {
		data->id[0] = room->wilds->uid;
		data->id[1] = room->x;
		data->id[2] = room->y;
		data->id[3] = room->z;
		data->id[4] = 0;
	} else {
		data->id[0] = 0;
		data->id[1] = room->area->uid;
		data->id[2] = room->vnum;
		data->id[3] = 0;
		data->id[4] = 0;
	}

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_ROOM_DATA));

	return TRUE;
}

bool variables_append_list_room (ppVARIABLE list, char *name, ROOM_INDEX_DATA *room)
{
	LLIST_ROOM_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !room || !var || var->type != VAR_BLLIST_ROOM) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_ROOM_DATA))) ) return FALSE;

	data->room = room;
	if( room->source ) {
		data->id[0] = 0;
		data->id[1] = room->source->area->uid;
		data->id[2] = room->source->vnum;
		data->id[3] = room->id[0];
		data->id[4] = room->id[1];
	} else if( room->wilds ) {
		data->id[0] = room->wilds->uid;
		data->id[1] = room->x;
		data->id[2] = room->y;
		data->id[3] = room->z;
		data->id[4] = 0;
	} else {
		data->id[0] = 0;
		data->id[1] = room->area->uid;
		data->id[2] = room->vnum;
		data->id[3] = 0;
		data->id[4] = 0;
	}

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_ROOM_DATA));

	return TRUE;
}

bool variables_set_list_connection (ppVARIABLE list, char *name, DESCRIPTOR_DATA *conn, bool save)
{
	pVARIABLE var = variable_get(*list, name);

	if( !conn ) return FALSE;

	if( !var ) {
		if ( !(var = variables_set_list(list,name,VAR_PLLIST_CONN,save)) )
			return FALSE;
	} else if( var->type != VAR_PLLIST_CONN )
		return FALSE;

	return list_appendlink(var->_.list, conn);
}

bool variables_append_list_connection (ppVARIABLE list, char *name, DESCRIPTOR_DATA *conn)
{
	pVARIABLE var = variable_get(*list, name);

	if( !conn || !var || var->type != VAR_PLLIST_CONN) return FALSE;

	return list_appendlink(var->_.list, conn);
}

static bool variables_append_list_area_id(ppVARIABLE list, char *name, long aid)
{
	LLIST_AREA_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !var || var->type != VAR_BLLIST_AREA) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_DATA))) ) return FALSE;

	data->area = NULL;
	data->uid = aid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_DATA));

	return TRUE;
}

static bool variables_append_list_area_region_id(ppVARIABLE list, char *name, long aid, long rid)
{
	LLIST_AREA_REGION_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !var || var->type != VAR_BLLIST_AREA_REGION) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_AREA_REGION_DATA))) ) return FALSE;

	data->aregion = NULL;
	data->aid = aid;
	data->rid = rid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_AREA_REGION_DATA));

	return TRUE;
}

static bool variables_append_list_wilds_id(ppVARIABLE list, char *name, long wid)
{
	LLIST_WILDS_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !var || var->type != VAR_BLLIST_WILDS) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_WILDS_DATA))) ) return FALSE;

	data->wilds = NULL;
	data->uid = wid;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_WILDS_DATA));

	return TRUE;
}


static bool variables_append_list_room_id (ppVARIABLE list, char *name, unsigned long a, unsigned long b, unsigned long c, unsigned long d, unsigned long e)
{
	LLIST_ROOM_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !var || var->type != VAR_BLLIST_ROOM) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_ROOM_DATA))) ) return FALSE;

	data->room = NULL;
	data->id[0] = a;
	data->id[1] = b;
	data->id[2] = c;
	data->id[3] = d;
	data->id[4] = e;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_ROOM_DATA));

	return TRUE;
}

bool variables_append_list_door (ppVARIABLE list, char *name, ROOM_INDEX_DATA *room, int door)
{
	LLIST_EXIT_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !room || door < 0 || door >= MAX_DIR || !var || var->type != VAR_BLLIST_EXIT) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_EXIT_DATA))) ) return FALSE;

	data->room = room;
	if( room->source ) {
		data->id[0] = 0;
		data->id[1] = room->source->area->uid;
		data->id[2] = room->source->vnum;
		data->id[3] = room->id[0];
		data->id[4] = room->id[1];
	} else if( room->wilds ) {
		data->id[0] = room->wilds->uid;
		data->id[1] = room->x;
		data->id[2] = room->y;
		data->id[3] = room->z;
		data->id[4] = 0;
	} else {
		data->id[0] = 0;
		data->id[1] = room->area->uid;
		data->id[2] = room->vnum;
		data->id[3] = 0;
		data->id[4] = 0;
	}
	data->door = door;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_EXIT_DATA));

	return TRUE;
}

static bool variables_append_list_door_id (ppVARIABLE list, char *name, unsigned long a, unsigned long b, unsigned long c, unsigned long d, unsigned long e, int door)
{
	LLIST_EXIT_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( door < 0 || door >= MAX_DIR || !var || var->type != VAR_BLLIST_EXIT) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_EXIT_DATA))) ) return FALSE;

	data->room = NULL;
	data->id[0] = a;
	data->id[1] = b;
	data->id[2] = c;
	data->id[3] = d;
	data->id[4] = e;
	data->door = door;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_EXIT_DATA));

	return TRUE;
}

bool variables_append_list_exit (ppVARIABLE list, char *name, EXIT_DATA *ex)
{
	if( !ex ) return FALSE;

	return variables_append_list_door(list, name, ex->from_room, ex->orig_door);
}

bool variables_append_list_skill_sn (ppVARIABLE list, char *name, CHAR_DATA *ch, int sn)
{
	LLIST_SKILL_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !ch || sn < 1 || sn >= MAX_SKILL || !var || var->type != VAR_BLLIST_SKILL) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_SKILL_DATA))) ) return FALSE;

	data->mob = ch;
	data->sn = sn;
	data->tok = NULL;
	data->mid[0] = ch->id[0];
	data->mid[1] = ch->id[1];
	data->tid[0] = 0;
	data->tid[1] = 0;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_SKILL_DATA));

	return TRUE;
}

bool variables_append_list_skill_token (ppVARIABLE list, char *name, TOKEN_DATA *tok)
{
	LLIST_SKILL_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !tok || !tok->player || (tok->type != TOKEN_SKILL && tok->type != TOKEN_SPELL) || !var || var->type != VAR_BLLIST_SKILL) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_SKILL_DATA))) ) return FALSE;

	data->mob = tok->player;
	data->sn = 0;
	data->tok = tok;
	data->mid[0] = tok->player->id[0];
	data->mid[1] = tok->player->id[1];
	data->tid[0] = tok->id[0];
	data->tid[1] = tok->id[1];

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_SKILL_DATA));

	return TRUE;
}

static bool variables_append_list_skill_id (ppVARIABLE list, char *name, unsigned long ma, unsigned long mb, unsigned long ta, unsigned long tb, int sn)
{
	LLIST_SKILL_DATA *data;
	pVARIABLE var = variable_get(*list, name);

	if( !var || var->type != VAR_BLLIST_SKILL) return FALSE;

	if( !(data = alloc_mem(sizeof(LLIST_SKILL_DATA))) ) return FALSE;

	data->mob = NULL;
	data->sn = (sn > 0 && sn < MAX_SKILL) ? sn : 0;;
	data->tok = NULL;
	data->mid[0] = ma;
	data->mid[1] = mb;
	data->tid[0] = ta;
	data->tid[1] = tb;

	if( !list_appendlink(var->_.list, data) )
		free_mem(data,sizeof(LLIST_SKILL_DATA));

	return TRUE;
}


bool variables_setindex_integer (ppVARIABLE list,char *name,int num, bool saved)
{
	pVARIABLE var = variable_create(list,name,TRUE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_INTEGER;
	var->_.i = num;
	var->save = saved;

	return TRUE;
}

bool variables_setindex_room (ppVARIABLE list,char *name, WNUM_LOAD wnum_load, bool saved)
{
	pVARIABLE var = variable_create(list,name,TRUE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_ROOM;
	if(fBootDb)
		var->_.wnum_load = wnum_load;
	else
		var->_.r = get_room_index_auid(wnum_load.auid, wnum_load.vnum);
	var->save = saved;

	return TRUE;
}


// Only reason this is seperate is the shared handling
bool variables_setindex_string(ppVARIABLE list,char *name,char *str,bool shared, bool saved)
{
	pVARIABLE var = variable_create(list,name,TRUE,TRUE);

	if(!var) return FALSE;

	if(shared) {
		var->type = VAR_STRING_S;
		var->_.s = str;
	} else {
		var->type = VAR_STRING;
		var->_.s = str_dup(str);
	}
	var->save = saved;

	return TRUE;
}

bool variables_setindex_skill (ppVARIABLE list,char *name,int sn, bool saved)
{
	pVARIABLE var = variable_create(list,name,TRUE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SKILL;
	var->_.sn = sn;
	var->save = saved;

	return TRUE;
}

bool variables_setindex_song (ppVARIABLE list,char *name,int sn, bool saved)
{
	pVARIABLE var = variable_create(list,name,TRUE,TRUE);

	if(!var) return FALSE;

	var->type = VAR_SONG;
	var->_.sn = sn;
	var->save = saved;

	return TRUE;
}


// Only reason this is seperate is the shared handling
bool variables_set_string(ppVARIABLE list,char *name,char *str,bool shared)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	if(shared) {
		var->type = VAR_STRING_S;
		var->_.s = str;
	} else {
		var->type = VAR_STRING;
		var->_.s = str_dup(str);
	}
	return TRUE;
}

bool variables_setsave_string(ppVARIABLE list,char *name,char *str,bool shared, bool save)
{
	pVARIABLE var = variable_create(list,name,FALSE,TRUE);

	if(!var) return FALSE;

	if(shared) {
		var->type = VAR_STRING_S;
		var->_.s = str;
	} else {
		var->type = VAR_STRING;
		var->_.s = str_dup(str);
	}

	var->save = save;
	return TRUE;
}

bool variables_append_string(ppVARIABLE list,char *name,char *str)
{
	pVARIABLE var = variable_create(list,name,FALSE,FALSE);
	char *nstr;
	int len;

	if(!var) return FALSE;

	if(var->type == VAR_STRING_S) {
		len = strlen(var->_.s);
		nstr = alloc_mem(len+strlen(str)+1);
		if(!nstr) return FALSE;
		strcpy(nstr,var->_.s);
		strcpy(nstr+len,str);
		var->_.s = nstr;
	} else if(var->type == VAR_STRING) {
		len = strlen(var->_.s);
		nstr = alloc_mem(len+strlen(str)+1);
		if(!nstr) return FALSE;
		strcpy(nstr,var->_.s);
		strcpy(nstr+len,str);
		free_string(var->_.s);
		var->_.s = nstr;
	} else
		var->_.s = str_dup(str);
	var->type = VAR_STRING;
	return TRUE;
}

bool variables_argremove_string_index(ppVARIABLE list,char *name,int argindex)
{
	pVARIABLE var = variable_create(list,name,FALSE,FALSE);
	char *nstr;
	int len;

	if(!var) return FALSE;

	if(var->type == VAR_STRING_S) {
		len = strlen(var->_.s);

		nstr = alloc_mem(len);
		if(!nstr) return FALSE;

		if(!string_argremove_index(var->_.s, argindex, nstr)) {
			free_mem(nstr, len);
			return FALSE;
		}

		var->_.s = nstr;
	} else if(var->type == VAR_STRING) {
		len = strlen(var->_.s);

		nstr = alloc_mem(len);
		if(!nstr) return FALSE;

		if(!string_argremove_index(var->_.s, argindex, nstr)) {
			free_mem(nstr, len);
			return FALSE;
		}

		free_string(var->_.s);
		var->_.s = nstr;
	} else
		var->_.s = str_dup("");
	var->type = VAR_STRING;
	return TRUE;
}

bool variables_argremove_string_phrase(ppVARIABLE list,char *name,char *phrase)
{
	pVARIABLE var = variable_create(list,name,FALSE,FALSE);
	char *nstr;
	int len;

	if(!var) return FALSE;

	if(var->type == VAR_STRING_S) {
		len = strlen(var->_.s);

		nstr = alloc_mem(len);
		if(!nstr) return FALSE;

		if(!string_argremove_phrase(var->_.s, phrase, nstr)) {
			free_mem(nstr, len);
			return FALSE;
		}

		var->_.s = nstr;
	} else if(var->type == VAR_STRING) {
		len = strlen(var->_.s);

		nstr = alloc_mem(len);
		if(!nstr) return FALSE;

		if(!string_argremove_phrase(var->_.s, phrase, nstr)) {
			free_mem(nstr, len);
			return FALSE;
		}

		free_string(var->_.s);
		var->_.s = nstr;
	} else
		var->_.s = str_dup("");
	var->type = VAR_STRING;
	return TRUE;
}

bool variables_format_string(ppVARIABLE list,char *name)
{
	pVARIABLE var = variable_get(*list,name);

	if(!var || (var->type != VAR_STRING_S && var->type != VAR_STRING)) return FALSE;

	var->_.s = format_string(var->_.s);
	var->type = VAR_STRING;
	return TRUE;
}


bool variables_format_paragraph(ppVARIABLE list,char *name)
{
	pVARIABLE var = variable_get(*list,name);

	if(!var || (var->type != VAR_STRING_S && var->type != VAR_STRING)) return FALSE;

	var->_.s = format_paragraph(var->_.s);
	var->type = VAR_STRING;
	return TRUE;
}

bool variable_remove(ppVARIABLE list,char *name)
{
	register pVARIABLE cur,prev;
	int test = 0;

	if(!*list) return FALSE;

	for(prev = NULL,cur = *list;cur && (test = str_cmp(cur->name,name)) < 0; prev = cur, cur = cur->next);

	if(!test) {
		if(prev) prev->next = cur->next;
		else *list = cur->next;

		variable_free(cur);
		return TRUE;
	}

	return FALSE;
}


bool variable_copy(ppVARIABLE list,char *oldname,char *newname)
{
	pVARIABLE oldv, newv;
	//ITERATOR it;

	if(!(oldv = variable_get(*list,oldname))) return FALSE;

	if(!str_cmp(oldname,newname)) return TRUE;	// Copy to itself is.. dumb.

	if(!(newv = variable_create(list,newname,oldv->index,TRUE))) return FALSE;

	newv->type = oldv->type;
	newv->save = oldv->index ? oldv->save : FALSE;

	switch(newv->type) {
	case VAR_UNKNOWN:		break;
	case VAR_BOOLEAN:		newv->_.boolean = oldv->_.boolean; break;
	case VAR_INTEGER:		newv->_.i = oldv->_.i; break;
	case VAR_STRING:		newv->_.s = str_dup(oldv->_.s); break;
	case VAR_STRING_S:		newv->_.s = oldv->_.s; break;
	case VAR_ROOM:			newv->_.r = oldv->_.r; break;
	case VAR_EXIT:			newv->_.door.r = oldv->_.door.r; newv->_.door.door = oldv->_.door.door; break;
	case VAR_MOBILE:		newv->_.m = oldv->_.m; break;
	case VAR_OBJECT:		newv->_.o = oldv->_.o; break;
	case VAR_TOKEN:			newv->_.t = oldv->_.t; break;
	case VAR_AREA:			newv->_.a = oldv->_.a; break;
	case VAR_AREA_REGION:	newv->_.ar = oldv->_.ar; break;
	case VAR_SKILL:			newv->_.sn = oldv->_.sn; break;
	case VAR_SKILLINFO:		newv->_.sk.owner = oldv->_.sk.owner; newv->_.sk.sn = oldv->_.sk.sn; break;
	case VAR_SONG:			newv->_.sn = oldv->_.sn; break;
	case VAR_AFFECT:		newv->_.aff = oldv->_.aff; break;
	case VAR_FOOD_BUFF:		newv->_.food_buff = oldv->_.food_buff; break;
	case VAR_COMPARTMENT:	newv->_.compartment = oldv->_.compartment; break;

	case VAR_CONNECTION:	newv->_.conn = oldv->_.conn; break;
	case VAR_WILDS:			newv->_.wilds = oldv->_.wilds; break;
	case VAR_CHURCH:		newv->_.church = oldv->_.church; break;
	case VAR_VARIABLE:		newv->_.variable = oldv->_.variable; break;

	case VAR_PLLIST_STR:
	case VAR_PLLIST_CONN:
	case VAR_PLLIST_ROOM:
	case VAR_PLLIST_MOB:
	case VAR_PLLIST_OBJ:
	case VAR_PLLIST_TOK:
	case VAR_PLLIST_CHURCH:
	case VAR_PLLIST_FOOD_BUFF:
	case VAR_PLLIST_COMPARTMENT:
	case VAR_PLLIST_VARIABLE:
	case VAR_PLLIST_AREA:
	case VAR_PLLIST_AREA_REGION:
	case VAR_BLLIST_ROOM:
	case VAR_BLLIST_MOB:
	case VAR_BLLIST_OBJ:
	case VAR_BLLIST_TOK:
	case VAR_BLLIST_EXIT:
	case VAR_BLLIST_SKILL:
	case VAR_BLLIST_AREA:
	case VAR_BLLIST_AREA_REGION:
	case VAR_BLLIST_WILDS:
		// All of the lists that require special allocation will be handled auto-magically by list_copy
		newv->_.list = list_copy(oldv->_.list);
		break;

	}

	return TRUE;
}

bool variable_copyto(ppVARIABLE from,ppVARIABLE to,char *oldname,char *newname, bool index)
{
	pVARIABLE oldv, newv;

	if(from == to) return variable_copy(from,oldname,newname);

	if(!(oldv = variable_get(*from,oldname))) return FALSE;

	if(!(newv = variable_create(to,newname,index,TRUE))) return FALSE;

	newv->type = oldv->type;
	newv->save = oldv->index ? oldv->save : FALSE;

	switch(newv->type) {
	case VAR_UNKNOWN:	break;
	case VAR_BOOLEAN:	newv->_.boolean = oldv->_.boolean; break;
	case VAR_INTEGER:	newv->_.i = oldv->_.i; break;
	case VAR_STRING:	newv->_.s = str_dup(oldv->_.s); break;
	case VAR_STRING_S:	newv->_.s = oldv->_.s; break;
	case VAR_ROOM:		newv->_.r = oldv->_.r; break;
	case VAR_EXIT:		newv->_.door.r = oldv->_.door.r; newv->_.door.door = oldv->_.door.door; break;
	case VAR_MOBILE:	newv->_.m = oldv->_.m; break;
	case VAR_OBJECT:	newv->_.o = oldv->_.o; break;
	case VAR_TOKEN:		newv->_.t = oldv->_.t; break;
	case VAR_AREA:		newv->_.a = oldv->_.a; break;
	case VAR_AREA_REGION:	newv->_.ar = oldv->_.ar; break;
	case VAR_SKILL:		newv->_.sn = oldv->_.sn; break;
	case VAR_SKILLINFO:	newv->_.sk.owner = oldv->_.sk.owner; newv->_.sk.sn = oldv->_.sk.sn; break;
	case VAR_SONG:		newv->_.sn = oldv->_.sn; break;
	case VAR_AFFECT:	newv->_.aff = oldv->_.aff; break;
	case VAR_FOOD_BUFF:	newv->_.food_buff = oldv->_.food_buff; break;
	case VAR_COMPARTMENT:	newv->_.compartment = oldv->_.compartment; break;

	case VAR_CONNECTION:	newv->_.conn = oldv->_.conn; break;
	case VAR_WILDS:			newv->_.wilds = oldv->_.wilds; break;
	case VAR_CHURCH:		newv->_.church = oldv->_.church; break;
	case VAR_VARIABLE:		newv->_.variable = oldv->_.variable; break;
	case VAR_DICE:			newv->_.dice = oldv->_.dice; break;

	case VAR_PLLIST_STR:
	case VAR_PLLIST_CONN:
	case VAR_PLLIST_ROOM:
	case VAR_PLLIST_MOB:
	case VAR_PLLIST_OBJ:
	case VAR_PLLIST_TOK:
	case VAR_PLLIST_CHURCH:
	case VAR_PLLIST_FOOD_BUFF:
	case VAR_PLLIST_COMPARTMENT:
	case VAR_PLLIST_VARIABLE:
	case VAR_PLLIST_AREA:
	case VAR_PLLIST_AREA_REGION:
	case VAR_BLLIST_ROOM:
	case VAR_BLLIST_MOB:
	case VAR_BLLIST_OBJ:
	case VAR_BLLIST_TOK:
	case VAR_BLLIST_EXIT:
	case VAR_BLLIST_SKILL:
	case VAR_BLLIST_AREA:
	case VAR_BLLIST_AREA_REGION:
	case VAR_BLLIST_WILDS:
		// All of the lists that require special allocation will be handled auto-magically by list_copy
		newv->_.list = list_copy(oldv->_.list);
		break;

	}

	return TRUE;
}

bool variable_copylist(ppVARIABLE from,ppVARIABLE to,bool index)
{
	pVARIABLE oldv, newv;

	if(from == to) return TRUE;

	for(oldv = *from; oldv; oldv = oldv->next) {
		if(!(newv = variable_create(to,oldv->name,index,TRUE))) continue;

		newv->type = oldv->type;
		newv->save = oldv->index ? oldv->save : FALSE;

		switch(newv->type) {
		case VAR_UNKNOWN:	break;
		case VAR_BOOLEAN:		newv->_.boolean = oldv->_.boolean; break;
		case VAR_INTEGER:	newv->_.i = oldv->_.i; break;
		case VAR_STRING:	newv->_.s = str_dup(oldv->_.s); break;
		case VAR_STRING_S:	newv->_.s = oldv->_.s; break;
		case VAR_ROOM:		newv->_.r = oldv->_.r; break;
		case VAR_EXIT:		newv->_.door.r = oldv->_.door.r; newv->_.door.door = oldv->_.door.door; break;
		case VAR_MOBILE:	newv->_.m = oldv->_.m; break;
		case VAR_OBJECT:	newv->_.o = oldv->_.o; break;
		case VAR_TOKEN:		newv->_.t = oldv->_.t; break;
		case VAR_SKILL:		newv->_.sn = oldv->_.sn; break;
		case VAR_SKILLINFO:	newv->_.sk.owner = oldv->_.sk.owner; newv->_.sk.sn = oldv->_.sk.sn; break;
		case VAR_SONG:		newv->_.sn = oldv->_.sn; break;
		case VAR_AFFECT:	newv->_.aff = oldv->_.aff; break;
		case VAR_FOOD_BUFF:	newv->_.food_buff = oldv->_.food_buff; break;
		case VAR_COMPARTMENT:	newv->_.compartment = oldv->_.compartment; break;

		case VAR_CONNECTION:	newv->_.conn = oldv->_.conn; break;
		case VAR_WILDS:			newv->_.wilds = oldv->_.wilds; break;
		case VAR_CHURCH:		newv->_.church = oldv->_.church; break;
		case VAR_VARIABLE:		newv->_.variable = oldv->_.variable; break;
		case VAR_DICE:			newv->_.dice = oldv->_.dice; break;

		case VAR_PLLIST_STR:
		case VAR_PLLIST_CONN:
		case VAR_PLLIST_ROOM:
		case VAR_PLLIST_MOB:
		case VAR_PLLIST_OBJ:
		case VAR_PLLIST_TOK:
		case VAR_PLLIST_CHURCH:
		case VAR_PLLIST_FOOD_BUFF:
		case VAR_PLLIST_COMPARTMENT:
		case VAR_PLLIST_VARIABLE:
		case VAR_PLLIST_AREA:
		case VAR_PLLIST_AREA_REGION:
		case VAR_BLLIST_ROOM:
		case VAR_BLLIST_MOB:
		case VAR_BLLIST_OBJ:
		case VAR_BLLIST_TOK:
		case VAR_BLLIST_EXIT:
		case VAR_BLLIST_SKILL:
		case VAR_BLLIST_AREA:
		case VAR_BLLIST_AREA_REGION:
		case VAR_BLLIST_WILDS:
			// All of the lists that require special allocation will be handled auto-magically by list_copy
			newv->_.list = list_copy(oldv->_.list);
			break;

		}
	}

	return TRUE;
}

pVARIABLE variable_copyvar(pVARIABLE oldv)
{
	pVARIABLE newv = variable_alloc(oldv->name, oldv->index);

	if(!newv) return NULL;

	newv->type = oldv->type;
	newv->save = oldv->index ? oldv->save : FALSE;

	switch(newv->type) {
	case VAR_UNKNOWN:		break;
	case VAR_BOOLEAN:		newv->_.boolean = oldv->_.boolean; break;
	case VAR_INTEGER:		newv->_.i = oldv->_.i; break;
	case VAR_STRING:		newv->_.s = str_dup(oldv->_.s); break;
	case VAR_STRING_S:		newv->_.s = oldv->_.s; break;
	case VAR_ROOM:			newv->_.r = oldv->_.r; break;
	case VAR_EXIT:			newv->_.door.r = oldv->_.door.r; newv->_.door.door = oldv->_.door.door; break;
	case VAR_MOBILE:		newv->_.m = oldv->_.m; break;
	case VAR_OBJECT:		newv->_.o = oldv->_.o; break;
	case VAR_TOKEN:			newv->_.t = oldv->_.t; break;
	case VAR_AREA:			newv->_.a = oldv->_.a; break;
	case VAR_AREA_REGION:	newv->_.ar = oldv->_.ar; break;
	case VAR_SKILL:			newv->_.sn = oldv->_.sn; break;
	case VAR_SKILLINFO:		newv->_.sk.owner = oldv->_.sk.owner; newv->_.sk.sn = oldv->_.sk.sn; break;
	case VAR_SONG:			newv->_.sn = oldv->_.sn; break;
	case VAR_AFFECT:		newv->_.aff = oldv->_.aff; break;
	case VAR_FOOD_BUFF:		newv->_.food_buff = oldv->_.food_buff; break;
	case VAR_COMPARTMENT:	newv->_.compartment = oldv->_.compartment; break;

	case VAR_CONNECTION:	newv->_.conn = oldv->_.conn; break;
	case VAR_WILDS:			newv->_.wilds = oldv->_.wilds; break;
	case VAR_CHURCH:		newv->_.church = oldv->_.church; break;
	case VAR_VARIABLE:		newv->_.variable = oldv->_.variable; break;
	case VAR_DICE:			newv->_.dice = oldv->_.dice; break;

	case VAR_PLLIST_STR:
	case VAR_PLLIST_CONN:
	case VAR_PLLIST_ROOM:
	case VAR_PLLIST_MOB:
	case VAR_PLLIST_OBJ:
	case VAR_PLLIST_TOK:
	case VAR_PLLIST_CHURCH:
	case VAR_PLLIST_FOOD_BUFF:
	case VAR_PLLIST_COMPARTMENT:
	case VAR_PLLIST_VARIABLE:
	case VAR_PLLIST_AREA:
	case VAR_PLLIST_AREA_REGION:
	case VAR_BLLIST_ROOM:
	case VAR_BLLIST_MOB:
	case VAR_BLLIST_OBJ:
	case VAR_BLLIST_TOK:
	case VAR_BLLIST_EXIT:
	case VAR_BLLIST_SKILL:
	case VAR_BLLIST_AREA:
	case VAR_BLLIST_AREA_REGION:
	case VAR_BLLIST_WILDS:
		// All of the lists that require special allocation will be handled auto-magically by list_copy
		newv->_.list = list_copy(oldv->_.list);
		break;

	}

	return newv;
}

// Deleter for list using variable copies
void list_free_variable(void *data)
{
//	static char lfv_buf[MIL];

	pVARIABLE var = (pVARIABLE)data;

//	sprintf(lfv_buf, "list_free_variable - called for %s", (var ? var->name : "(null-var)"));
//	wiznet(lfv_buf,NULL,NULL,WIZ_TESTING,0,0);

	variable_free(var);
}


LLIST *variable_copy_tolist(ppVARIABLE vars)
{
	LLIST *lst = list_createx(TRUE, NULL, list_free_variable);
	pVARIABLE oldv;

	for(oldv = *vars; oldv; oldv = oldv->next)
		list_appendlink(lst, variable_copyvar(oldv));

	return lst;
}

bool variable_setsave(pVARIABLE vars,char *name,bool state)
{
	pVARIABLE v = variable_get(vars,name);

	if(v) {
		v->save = state;
		return TRUE;
	}

	return FALSE;
}


void variable_clearfield(int type, void *ptr)
{
	register pVARIABLE cur = variable_head;
	//unsigned long uid[2];

	if(!ptr) return;

	while(cur) {
		switch(cur->type)
		{
		case VAR_AFFECT:
			if (cur->_.aff == ptr)
			{
				cur->_.aff = NULL;
			}
			break;
		case VAR_FOOD_BUFF:
			if (cur->_.food_buff == ptr)
			{
				cur->_.food_buff = NULL;
			}
			break;
		case VAR_COMPARTMENT:
			if (cur->_.compartment == ptr)
			{
				cur->_.compartment = NULL;
			}
			break;
		case VAR_SKILLINFO:

			// Special case for MOBILES, clear out any skill reference.
			if(type == VAR_MOBILE && cur->_.sk.owner == ptr) {
				CHAR_DATA *owner = cur->_.sk.owner;
				TOKEN_DATA *token = cur->_.sk.token;

				cur->_.skid.sn = cur->_.sk.sn;
				if(owner) {
					cur->_.skid.mid[0] = owner->id[0];
					cur->_.skid.mid[1] = owner->id[1];
				}
				else
				{
					cur->_.skid.mid[0] = 0;
					cur->_.skid.mid[1] = 0;
				}
				if(token) {
					cur->_.skid.tid[0] = token->id[0];
					cur->_.skid.tid[1] = token->id[1];
				}
				else
				{
					cur->_.skid.tid[0] = 0;
					cur->_.skid.tid[1] = 0;
				}
				cur->type = VAR_SKILLINFO_ID;

			} else if(type == VAR_TOKEN && cur->_.sk.token == ptr) {
				CHAR_DATA *owner = cur->_.sk.owner;
				TOKEN_DATA *token = cur->_.sk.token;

				cur->_.skid.sn = cur->_.sk.sn;
				if(owner) {
					cur->_.skid.mid[0] = owner->id[0];
					cur->_.skid.mid[1] = owner->id[1];
				}
				else
				{
					cur->_.skid.mid[0] = 0;
					cur->_.skid.mid[1] = 0;
				}
				if(token) {
					cur->_.skid.tid[0] = token->id[0];
					cur->_.skid.tid[1] = token->id[1];
				}
				else
				{
					cur->_.skid.tid[0] = 0;
					cur->_.skid.tid[1] = 0;
				}
				cur->type = VAR_SKILLINFO_ID;
			}
			break;

		case VAR_EXIT:
			if(type == VAR_ROOM && cur->_.door.r == ptr) {
				ROOM_INDEX_DATA *room = cur->_.door.r;
				if(room->wilds) {
					cur->_.wdoor.door = cur->_.door.door;
					cur->_.wdoor.wuid = room->wilds->uid;
					cur->_.wdoor.x = room->x;
					cur->_.wdoor.y = room->y;
					cur->type = VAR_WILDS_DOOR;
				} else if(room->source) {
					cur->_.cdoor.door = cur->_.door.door;
					cur->_.cdoor.r = room->source;
					cur->_.cdoor.a = room->id[0];
					cur->_.cdoor.b = room->id[1];
					cur->type = VAR_CLONE_DOOR;
				}
			}
			break;

		case VAR_ROOM:
			if(type == VAR_ROOM && cur->_.r == ptr) {
				ROOM_INDEX_DATA *room = cur->_.r;
				if(room->wilds) {
					cur->_.wroom.wuid = room->wilds->uid;
					cur->_.wroom.x = room->x;
					cur->_.wroom.y = room->y;
					cur->type = VAR_WILDS_ROOM;
				} else if(room->source) {
					cur->_.cr.r = room->source;
					cur->_.cr.a = room->id[0];
					cur->_.cr.b = room->id[1];
					cur->type = VAR_CLONE_ROOM;
				}
			}
			break;

		case VAR_OBJECT:
			if(type == VAR_OBJECT && cur->_.o == ptr) {
				OBJ_DATA *obj = cur->_.o;

				cur->_.oid.a = obj->id[0];
				cur->_.oid.b = obj->id[1];
				cur->type = VAR_OBJECT_ID;
			}
			break;

		case VAR_MOBILE:
			if(type == VAR_MOBILE && cur->_.m == ptr) {
				CHAR_DATA *mob = cur->_.m;

				cur->_.mid.a = mob->id[0];
				cur->_.mid.b = mob->id[1];
				cur->type = VAR_MOBILE_ID;
			}
			break;

		case VAR_TOKEN:
			if(type == VAR_TOKEN && cur->_.t == ptr) {
				TOKEN_DATA *token = cur->_.t;

				cur->_.tid.a = token->id[0];
				cur->_.tid.b = token->id[1];
				cur->type = VAR_TOKEN_ID;
			}
			break;

		case VAR_BLLIST_ROOM:
			if(type == VAR_ROOM && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_ROOM_DATA *lroom;

				iterator_start(&it, cur->_.list);

				while( (lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) ) {
					if( lroom->room && lroom->room == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}

				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_MOB:
			if(type == VAR_MOBILE && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_UID_DATA *luid;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
					if( luid->ptr && luid->ptr == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}
				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_OBJ:
			if(type == VAR_OBJECT && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_UID_DATA *luid;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
					if( luid->ptr && luid->ptr == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}
				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_TOK:
			if(type == VAR_TOKEN && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_UID_DATA *luid;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) ) {
					if( luid->ptr && luid->ptr == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}
				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_AREA:
			if(type == VAR_AREA && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_AREA_DATA *larea;

				iterator_start(&it, cur->_.list);

				while( (larea = (LLIST_AREA_DATA *)iterator_nextdata(&it)) ) {
					if( larea->area && larea->area == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}
				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_AREA_REGION:
			if(type == VAR_AREA_REGION && list_isvalid(cur->_.list)) {
				ITERATOR it;
				LLIST_AREA_REGION_DATA *laregion;

				iterator_start(&it, cur->_.list);

				while( (laregion = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&it)) ) {
					if( laregion->aregion && laregion->aregion == ptr ) {
						iterator_remcurrent(&it);
						break;
					}
				}
				iterator_stop(&it);
			}
			break;

		case VAR_PLLIST_STR:
			if( type == VAR_STRING && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_CONN:
			if( type == VAR_CONNECTION && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_ROOM:
			if( type == VAR_ROOM && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_MOB:
			if( type == VAR_MOBILE && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_OBJ:
			if( type == VAR_OBJECT && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_TOK:
			if( type == VAR_TOKEN && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_CHURCH:
			if( type == VAR_CHURCH && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_FOOD_BUFF:
			if( type == VAR_FOOD_BUFF && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_COMPARTMENT:
			if( type == VAR_COMPARTMENT && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_AREA:
			if( type == VAR_AREA && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_PLLIST_AREA_REGION:
			if( type == VAR_AREA_REGION && ptr && list_isvalid(cur->_.list)) {
				list_remlink(cur->_.list, ptr);
			}
			break;

		case VAR_SECTION:
			if(type == VAR_SECTION && cur->_.section == ptr)
				cur->_.section = NULL;
			break;

		case VAR_INSTANCE:
			if(type == VAR_INSTANCE && cur->_.instance == ptr)
				cur->_.instance = NULL;
			break;

		case VAR_DUNGEON:
			if(type == VAR_DUNGEON && cur->_.dungeon == ptr)
				cur->_.dungeon = NULL;
			break;

		case VAR_SHIP:
			if(type == VAR_SHIP && cur->_.ship == ptr)
				cur->_.ship = NULL;
			break;

		default:
			if(cur->_.raw == ptr)
				cur->_.raw = NULL;
			break;
		}

		cur = cur->global_next;
	}
}

void variable_index_fix(void)
{
	register pVARIABLE cur = variable_index_head;

	while(cur) {
		if(cur->type == VAR_ROOM) {
			if(cur->_.wnum_load.auid > 0 && cur->_.wnum_load.vnum > 0) cur->_.r = get_room_index_auid(cur->_.wnum_load.auid, cur->_.wnum_load.vnum);
		}
		cur = cur->global_next;
	}
}

// Only fix variables that can be resolved, leave everything else alone
void variable_fix(pVARIABLE var)
{
	register ROOM_INDEX_DATA *room;
	ITERATOR it;
	LLIST_UID_DATA *luid;
	LLIST_ROOM_DATA *lroom;
	LLIST_EXIT_DATA *lexit;
	LLIST_SKILL_DATA *lskill;
	LLIST_AREA_DATA *larea;
	LLIST_AREA_REGION_DATA *laregion;
	LLIST_WILDS_DATA *lwilds;

	if(fBootDb && var->type == VAR_ROOM) {	// Fix room variables on boot...
		if(var->_.wnum_load.auid > 0 && var->_.wnum_load.vnum) {
			var->_.r = get_room_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);
		}
	} else if(var->type == VAR_CLONE_ROOM) {	// Dynamic
		if(var->_.cr.r && (room = get_clone_room(var->_.cr.r, var->_.cr.a, var->_.cr.b)) ) {
			var->_.r = room;
			var->type = VAR_ROOM;
		}
	} else if(var->type == VAR_CLONE_DOOR) {	// Dynamic
		if( var->_.cdoor.r && (room = get_clone_room(var->_.cdoor.r,var->_.cdoor.a,var->_.cdoor.b)) ) {
			var->_.door.door = var->_.cdoor.door;
			var->_.door.r = room;
			var->type = VAR_EXIT;
		}
	} else if(var->type == VAR_WILDS_ROOM) {
		WILDS_DATA *wilds = get_wilds_from_uid(NULL, var->_.wroom.wuid);
		ROOM_INDEX_DATA *vroom;
		if( wilds ) {
			vroom = get_wilds_vroom(wilds, var->_.wroom.x, var->_.wroom.y);

			if( !vroom )
				vroom = create_wilds_vroom(wilds, var->_.wroom.x, var->_.wroom.y);

			var->_.r = vroom;
			var->type = VAR_ROOM;
		}
	} else if(var->type == VAR_WILDS_DOOR) {
		WILDS_DATA *wilds = get_wilds_from_uid(NULL, var->_.wdoor.wuid);
		ROOM_INDEX_DATA *vroom;
		if( wilds ) {
			vroom = get_wilds_vroom(wilds, var->_.wdoor.x, var->_.wdoor.y);

			if( !vroom )
				vroom = create_wilds_vroom(wilds, var->_.wdoor.x, var->_.wdoor.y);

			var->_.door.door = var->_.wdoor.door;
			var->_.door.r = vroom;
			var->type = VAR_EXIT;
		}
	} else if(var->type == VAR_MOBILE_ID) {	// Dynamic
		CHAR_DATA *ch = idfind_mobile(var->_.mid.a, var->_.mid.b);
		if( ch ) {
			var->_.m = ch;
			var->type = VAR_MOBILE;
		}
	} else if(var->type == VAR_OBJECT_ID) {	// Dynamic
		OBJ_DATA *o = idfind_object(var->_.oid.a, var->_.oid.b);
		if( o ) {
			var->_.o = o;
			var->type = VAR_OBJECT;
		}
	} else if(var->type == VAR_TOKEN_ID) {
		TOKEN_DATA *t = idfind_token(var->_.tid.a, var->_.tid.b);
		if( t ) {
			var->_.t = t;
			var->type = VAR_TOKEN;
		}
	} else if(var->type == VAR_AREA_ID) {
		AREA_DATA *area = get_area_from_uid(var->_.aid);
		if( area ) {
			var->_.a = area;
			var->type = VAR_AREA;
		}
	} else if(var->type == VAR_AREA_REGION_ID) {
		AREA_DATA *area = get_area_from_uid(var->_.arid.aid);

		if (area) {
			AREA_REGION *region = get_area_region_by_uid(area, var->_.arid.rid);

			var->_.ar = region;
			var->type = VAR_AREA_REGION;
		}
	} else if(var->type == VAR_WILDS_ID) {
		WILDS_DATA *wilds = get_wilds_from_uid(NULL, var->_.wid);
		if( wilds ) {
			var->_.wilds = wilds;
			var->type = VAR_WILDS;
		}
	} else if(var->type == VAR_SKILLINFO_ID && (var->_.skid.mid[0] > 0 || var->_.skid.mid[1] > 0)) {
		CHAR_DATA *ch = idfind_mobile(var->_.skid.mid[0], var->_.skid.mid[1]);
		if( ch ) {
			if(var->_.skid.tid[0] > 0 || var->_.skid.tid[1] > 0) {
				TOKEN_DATA *tok = idfind_token_char(ch, var->_.skid.tid[0], var->_.skid.tid[1]);

				if( tok ) {
					var->_.sk.token = tok;
					var->_.sk.sn = 0;
					var->_.sk.owner = ch;
					var->type = VAR_SKILLINFO;
				} else
					var->_.skid.mid[0] = var->_.skid.mid[1] = 0;
			} else {
				int sn = var->_.skid.sn;

				var->_.sk.token = NULL;
				var->_.sk.sn = sn;
				var->_.sk.owner = ch;
				var->type = VAR_SKILLINFO;
			}
		}
	} else if(var->type == VAR_BLLIST_MOB && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
			if( !luid->ptr )
				luid->ptr = idfind_mobile(luid->id[0], luid->id[1]);

		iterator_stop(&it);

	} else if(var->type == VAR_BLLIST_OBJ && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
			if( !luid->ptr )
				luid->ptr = idfind_object(luid->id[0], luid->id[1]);

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_TOK && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
			if( !luid->ptr )
				luid->ptr = idfind_token(luid->id[0], luid->id[1]);

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_AREA && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (larea = (LLIST_AREA_DATA *)iterator_nextdata(&it)) )
			if( !larea->area )
				larea->area = get_area_from_uid(larea->uid);

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_AREA_REGION && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (laregion = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&it)) )
			if( !laregion->aregion )
			{
				AREA_DATA *area = get_area_from_uid(laregion->aid);
				if (area)
				{
					laregion->aregion = get_area_region_by_uid(area, laregion->rid);
				}
			}

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_WILDS && var->_.list) {
		iterator_start(&it, var->_.list);

		while( (lwilds = (LLIST_WILDS_DATA *)iterator_nextdata(&it)) )
			if( !lwilds->wilds )
				lwilds->wilds = get_wilds_from_uid(NULL, lwilds->uid);

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_ROOM && var->_.list ) {
		iterator_start(&it, var->_.list);

		while( (lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) )
			if( !lroom->room ) {
				if( lroom->id[0] > 0 ) {	// WILDS room
					WILDS_DATA *wilds = get_wilds_from_uid(NULL, lroom->id[0]);
					if( wilds ) {
						lroom->room = get_wilds_vroom( wilds, lroom->id[1], lroom->id[2]);

						if( !lroom->room )
							lroom->room = create_wilds_vroom(wilds, lroom->id[1], lroom->id[2]);
					}

					if( !lroom->room) iterator_remcurrent(&it);

				} else if( lroom->id[3] > 0 || lroom->id[4] > 0) {	// Clone room
					lroom->room = get_room_index_auid(lroom->id[1], lroom->id[2]);

					if( lroom->room )
						lroom->room = get_clone_room((ROOM_INDEX_DATA *)(lroom->room), lroom->id[3], lroom->id[4]);
				} else if( !(lroom->room = get_room_index_auid(lroom->id[1], lroom->id[2])) )
					iterator_remcurrent(&it);
			}

		iterator_stop(&it);
	} else if(var->type == VAR_BLLIST_EXIT && var->_.list ) {
		iterator_start(&it, var->_.list);

		while( (lexit = (LLIST_EXIT_DATA *)iterator_nextdata(&it)) )
			if( !lexit->room ) {
				if( lexit->id[0] > 0 ) {	// WILDS room
					WILDS_DATA *wilds = get_wilds_from_uid(NULL, lexit->id[0]);
					if( wilds ) {
						lexit->room = get_wilds_vroom( wilds, lexit->id[1], lexit->id[2]);

						if( !lexit->room )
							lexit->room = create_wilds_vroom(wilds, lexit->id[1], lexit->id[2]);

					}

					if( !lexit->room) iterator_remcurrent(&it);
				} else if( lexit->id[3] > 0 || lexit->id[4] > 0) {	// Clone room, can wait
					lexit->room = get_room_index_auid(lexit->id[1], lexit->id[2]);

					if( lexit->room )
						lexit->room = get_clone_room((ROOM_INDEX_DATA *)(lexit->room), lexit->id[3], lexit->id[4]);
				} else if( !(lexit->room = get_room_index_auid(lexit->id[1], lexit->id[2])) )
					iterator_remcurrent(&it);

			}
		iterator_stop(&it);

	} else if(var->type == VAR_BLLIST_SKILL && var->_.list ) {
		iterator_start(&it, var->_.list);

		while( (lskill = (LLIST_SKILL_DATA *)iterator_nextdata(&it)) )
			if( !lskill->mob && ((lskill->sn > 0 && lskill->sn < MAX_SKILL) || lskill->tid[0] > 0 || lskill->tid[1] > 0) ) {
				lskill->mob = idfind_mobile(lskill->mid[0],lskill->mid[1]);

				if( lskill->mob && (lskill->tid[0] > 0 || lskill->tid[1] > 0)) {
					lskill->sn = 0;
					lskill->tok = idfind_token_char(lskill->mob, lskill->tid[0], lskill->tid[1]);

					// Can't resolve the token on a found mob, remove "skill" from list
					if( !lskill->tok ) {
						lskill->tid[0] = lskill->tid[1] = 0;
						iterator_remcurrent(&it);
					}
				}
			}

		iterator_stop(&it);

	} else if(fBootDb && var->type == VAR_MOBINDEX ) {
		var->_.mindex = get_mob_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_OBJINDEX ) {
		var->_.oindex = get_obj_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_TOKENINDEX ) {
		var->_.tindex = get_token_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_BLUEPRINT ) {
		var->_.bp = get_blueprint_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_BLUEPRINT_SECTION ) {
		var->_.bs = get_blueprint_section_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_DUNGEONINDEX ) {
		var->_.dngindex = get_dungeon_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	} else if(fBootDb && var->type == VAR_SHIPINDEX ) {
		var->_.shipindex = get_ship_index_auid(var->_.wnum_load.auid, var->_.wnum_load.vnum);

	}
}

void variable_fix_global(void)
{
	register pVARIABLE cur = variable_head;

	while(cur) {

		variable_fix(cur);

		cur = cur->global_next;
	}
}

void variable_fix_list(register pVARIABLE list)
{
	while(list) {
		variable_fix(list);
		list = list->global_next;
	}
}

// The variable_dynamic_fix_* functions deal with things loaded by players logging in.

// Clone rooms AND Clone doors
void variable_dynamic_fix_clone_room (ROOM_INDEX_DATA *clone)
{
	register pVARIABLE cur = variable_head;
	register CHAR_DATA *ch;
	register OBJ_DATA *obj;
	register TOKEN_DATA *token;
	ITERATOR it;

	if( !clone->source ) return;

	while(cur) {
		switch(cur->type) {
		case VAR_CLONE_ROOM:
			if( clone->source == cur->_.cr.r && clone->id[0] == cur->_.cr.a && clone->id[1] == cur->_.cr.b) {
				cur->_.r = clone;
				cur->type = VAR_ROOM;
			}
			break;
		case VAR_CLONE_DOOR:
			if( clone->source == cur->_.cdoor.r && clone->id[0] == cur->_.cdoor.a && clone->id[1] == cur->_.cdoor.b) {
				cur->_.door.door = cur->_.cdoor.door;
				cur->_.door.r = clone;
				cur->type = VAR_EXIT;
			}
			break;
		case VAR_BLLIST_ROOM:
			if( cur->_.list ) {
				LLIST_ROOM_DATA *lroom;

				iterator_start(&it, cur->_.list);

				while( (lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) ) {
					if( !lroom->room ) {
						if( lroom->id[0] > 0 &&
							lroom->id[1] == clone->source->vnum &&
							clone->id[0] == lroom->id[2] &&
							clone->id[1] == lroom->id[3])
							lroom->room = clone;
					}
				}

				iterator_stop(&it);
			}
			break;

		case VAR_BLLIST_EXIT:
			if( cur->_.list ) {
				LLIST_EXIT_DATA *lexit;

				iterator_start(&it, cur->_.list);

				while( (lexit = (LLIST_EXIT_DATA *)iterator_nextdata(&it)) ) {
					if( !lexit->room ) {
						if( lexit->id[0] > 0 &&
							lexit->id[1] == clone->source->vnum &&
							clone->id[0] == lexit->id[2] &&
							clone->id[1] == lexit->id[3])
							lexit->room = clone;
					}
				}

				iterator_stop(&it);
			}
			break;
		}

		cur = cur->global_next;
	}

	for( obj = clone->contents; obj; obj = obj->next_content)
		variable_dynamic_fix_object(obj);

	for(token = clone->tokens; token; token = token->next)
		variable_dynamic_fix_token(token);

	for( ch = clone->people; ch; ch = ch->next_in_room)
		variable_dynamic_fix_mobile(ch);
}

void variable_dynamic_fix_object(OBJ_DATA *obj)
{
	register pVARIABLE cur = variable_head;
	register OBJ_DATA *o;
	register ROOM_INDEX_DATA *clone;
	register TOKEN_DATA *token;
	register LLIST_UID_DATA *luid;

	while(cur) {
		switch(cur->type) {
		case VAR_OBJECT_ID:
			if( obj->id[0] == cur->_.oid.a && obj->id[1] == cur->_.oid.b) {
				cur->_.o = obj;
				cur->type = VAR_OBJECT;
			}
			break;

		case VAR_BLLIST_OBJ:
			if( cur->_.list && cur->_.list->valid ) {

				ITERATOR it;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
					if( !luid->ptr && obj->id[0] == luid->id[0] && obj->id[1] == luid->id[1])
						luid->ptr = obj;

				iterator_stop(&it);
			}
		}

		cur = cur->global_next;
	}

	for(o = obj->contains; o; o = o->next_content)
		variable_dynamic_fix_object(o);

	for(token = obj->tokens; token; token = token->next)
		variable_dynamic_fix_token(token);

	for(clone = obj->clone_rooms; clone; clone = clone->next_clone)
		variable_dynamic_fix_clone_room(clone);
}

void variable_dynamic_fix_token (TOKEN_DATA *token)
{
	register pVARIABLE cur = variable_head;

	register LLIST_UID_DATA *luid;

	while(cur) {
		switch(cur->type) {
		case VAR_TOKEN_ID:
			if( token->id[0] == cur->_.tid.a && token->id[1] == cur->_.tid.b) {
				cur->_.t = token;
				cur->type = VAR_TOKEN;
			}
			break;

		case VAR_BLLIST_TOK:
			if( cur->_.list && cur->_.list->valid ) {

				ITERATOR it;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
					if( !luid->ptr && token->id[0] == luid->id[0] && token->id[1] == luid->id[1])
						luid->ptr = token;

				iterator_stop(&it);
			}
		}

		cur = cur->global_next;
	}



}

void variable_dynamic_fix_mobile (CHAR_DATA *ch)
{
	register pVARIABLE cur = variable_head;
	register OBJ_DATA *o;
	register TOKEN_DATA *token;
	register ROOM_INDEX_DATA *clone;
	register LLIST_UID_DATA *luid;

	while(cur) {
//		log_stringf("variable_dynamic_fix_mobile: %s, %s, %d", ch->name, cur->name, cur->type);
		switch(cur->type) {
		case VAR_MOBILE_ID:
//			log_stringf("variable_dynamic_fix_mobile:VAR_MOBILE_ID[%s]: %08lX - %08lX :: %08lX - %08lX", cur->name, ch->id[0], cur->_.mid.a, ch->id[1], cur->_.mid.b);
			if( ch->id[0] == cur->_.mid.a && ch->id[1] == cur->_.mid.b) {
				cur->_.m = ch;
				cur->type = VAR_MOBILE;
			}
			break;

		case VAR_SKILLINFO_ID:
			if( ch->id[0] == cur->_.skid.mid[0] && ch->id[1] == cur->_.skid.mid[1] ) {
				if(cur->_.skid.tid[0] > 0 || cur->_.skid.tid[1] > 0) {
					TOKEN_DATA *tok = idfind_token_char(ch, cur->_.skid.tid[0], cur->_.skid.tid[1]);

					if( tok ) {
						cur->_.sk.token = tok;
						cur->_.sk.sn = 0;
						cur->_.sk.owner = ch;
						cur->type = VAR_SKILLINFO;
					} else
						cur->_.skid.mid[0] = cur->_.skid.mid[1] = 0;
				} else {
					int sn = cur->_.skid.sn;

					cur->_.sk.token = NULL;
					cur->_.sk.sn = sn;
					cur->_.sk.owner = ch;
					cur->type = VAR_SKILLINFO;
				}
			}
			break;

		case VAR_BLLIST_MOB:
			if( cur->_.list && cur->_.list->valid ) {

				ITERATOR it;

				iterator_start(&it, cur->_.list);

				while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
					if( !luid->ptr && ch->id[0] == luid->id[0] && ch->id[1] == luid->id[1])
						luid->ptr = ch;

				iterator_stop(&it);
			}
		}

		cur = cur->global_next;
	}

	for(o = ch->carrying; o; o = o->next_content)
		variable_dynamic_fix_object(o);

	for(o = ch->locker; o; o = o->next_content)
		variable_dynamic_fix_object(o);

	for(token = ch->tokens; token; token = token->next)
		variable_dynamic_fix_token(token);

	for(clone = ch->clone_rooms; clone; clone = clone->next_clone)
		variable_dynamic_fix_clone_room(clone);
}

void variable_dynamic_fix_church (CHURCH_DATA *church)
{
	register pVARIABLE cur = variable_head;

	while(cur) {
		switch(cur->type) {
		case VAR_CHURCH_ID:
			if( church->uid == cur->_.chid ) {
				cur->_.church = church;
				cur->type = VAR_CHURCH;
			}
			break;
		}
		cur = cur->global_next;
	}

}


void variable_fwrite_uid_list( char *field, char *name, LLIST *list, FILE *fp)
{
	if(list && list->valid) {
		LLIST_UID_DATA *data;
		ITERATOR it;

		fprintf(fp,"%s %s~\n", field, name);
		iterator_start(&it,list);

		while((data = (LLIST_UID_DATA*)iterator_nextdata(&it)))
			fprintf(fp, "UID %ld %ld\n", data->id[0], data->id[1]);

		iterator_stop(&it);
		fprintf(fp,"End\n");
	}

}

void variable_fwrite(pVARIABLE var, FILE *fp)
{
	ITERATOR it;

	switch(var->type) {
	default:
		break;
	case VAR_BOOLEAN:
		fprintf(fp,"VarBool %s~ %d\n", var->name, var->_.boolean ? 1 : 0);
		break;

	case VAR_INTEGER:
		fprintf(fp,"VarInt %s~ %d\n", var->name, var->_.i);
		break;
	case VAR_STRING:
	case VAR_STRING_S:	// They all get changed to shared on load...
		fprintf(fp,"VarStr %s~ %s~\n", var->name, var->_.s ? var->_.s : "");
		break;

	case VAR_ROOM:
		if(var->_.r) {
			if(var->_.r->wilds)
				fprintf(fp,"VarVRoom %s~ %ld %ld %ld\n", var->name, var->_.r->wilds->uid, var->_.r->x, var->_.r->y);
			else if(var->_.r->source)
				fprintf(fp,"VarCRoom %s~ %ld#%ld %ld %ld\n", var->name, var->_.r->source->area->uid, var->_.r->source->vnum, var->_.r->id[0], var->_.r->id[1]);
			else
				fprintf(fp,"VarRoom %s~ %ld#%ld\n", var->name, var->_.r->area->uid, var->_.r->vnum);
		}
		break;

	case VAR_WILDS_ROOM:
		fprintf(fp,"VarVRoom %s~ %d %d %d\n", var->name, (int)var->_.wroom.wuid, (int)var->_.wroom.x, (int)var->_.wroom.y);
		break;

	// Unresolved CLONE ROOMs
	case VAR_CLONE_ROOM:
		if( var->_.cr.r )
			fprintf(fp,"VarCRoom %s~ %ld#%ld %ld %ld\n", var->name, var->_.cr.r->area->uid, var->_.cr.r->vnum, var->_.cr.a,var->_.cr.b);
		break;

	case VAR_EXIT:
	case VAR_DOOR:
		if(var->_.door.r) {
			if(var->_.door.r->wilds)
				fprintf(fp,"VarVExit %s~ %d %d %d %d\n", var->name, (int)var->_.door.r->wilds->uid, (int)var->_.door.r->x, (int)var->_.door.r->y, var->_.door.door);
			else if(var->_.door.r->source)
				fprintf(fp,"VarCExit %s~ %ld#%ld %d %d %d\n", var->name, var->_.door.r->source->area->uid, var->_.door.r->source->vnum, (int)var->_.door.r->id[0], (int)var->_.door.r->id[1], var->_.door.door);
			else
				fprintf(fp,"VarExit %s~ %ld#%ld %d\n", var->name, var->_.door.r->area->uid, var->_.door.r->vnum, var->_.door.door);
		}
		break;

	case VAR_WILDS_DOOR:
		fprintf(fp,"VarVExit %s~ %d %d %d %d\n", var->name, (int)var->_.wdoor.wuid, (int)var->_.wdoor.x, (int)var->_.wdoor.y, var->_.wdoor.door);
		break;


	// Unresolved EXITS in CLONE ROOMs
	case VAR_CLONE_DOOR:
		if( var->_.cdoor.r )
			fprintf(fp,"VarCExit %s~ %ld#%ld %d %d %d\n", var->name, var->_.cdoor.r->area->uid, var->_.cdoor.r->vnum, (int)var->_.cdoor.a,(int)var->_.cdoor.b, var->_.cdoor.door);
		break;

	case VAR_MOBILE:
		if(var->_.m)
			fprintf(fp,"VarMob %s~ %d %d\n", var->name, (int)var->_.m->id[0], (int)var->_.m->id[1]);
		break;
	case VAR_OBJECT:
		if(var->_.o)
			fprintf(fp,"VarObj %s~ %d %d\n", var->name, (int)var->_.o->id[0], (int)var->_.o->id[1]);
		break;
	case VAR_TOKEN:
		if(var->_.t)
			fprintf(fp,"VarTok %s~ %d %d\n", var->name, (int)var->_.t->id[0], (int)var->_.t->id[1]);
		break;
	case VAR_AREA:
		if(var->_.a)
			fprintf(fp,"VarArea %s~ %ld\n", var->name, var->_.a->uid);
		break;
	case VAR_AREA_REGION:
		if(var->_.ar)
			fprintf(fp,"VarAreaRegion %s~ %ld %ld\n", var->name, var->_.ar->area->uid, var->_.ar->uid);
		break;
	case VAR_WILDS:
		if(var->_.wilds)
			fprintf(fp,"VarWilds %s~ %ld\n", var->name, var->_.wilds->uid);
		break;
	case VAR_CHURCH:
		if(var->_.church)
			fprintf(fp,"VarChurch %s~ %ld\n", var->name, var->_.church->uid);
		break;

	// Unresolved UIDs
	case VAR_MOBILE_ID:
		fprintf(fp,"VarMob %s~ %d %d\n", var->name, (int)var->_.mid.a, (int)var->_.mid.b);
		break;
	case VAR_OBJECT_ID:
		fprintf(fp,"VarObj %s~ %d %d\n", var->name, (int)var->_.oid.a, (int)var->_.oid.b);
		break;
	case VAR_TOKEN_ID:
		fprintf(fp,"VarTok %s~ %d %d\n", var->name, (int)var->_.tid.a, (int)var->_.tid.b);
		break;
	case VAR_AREA_ID:
		fprintf(fp,"VarArea %s~ %ld\n", var->name, var->_.aid);
		break;
	case VAR_AREA_REGION_ID:
		fprintf(fp,"VarAreaRegion %s~ %ld %ld\n", var->name, var->_.arid.aid, var->_.arid.rid);
		break;
	case VAR_WILDS_ID:
		fprintf(fp,"VarWilds %s~ %ld\n", var->name, var->_.wid);
		break;
	case VAR_CHURCH_ID:
		fprintf(fp,"VarChurch %s~ %ld\n", var->name, var->_.chid);
		break;

	case VAR_SKILL:
		fprintf(fp,"VarSkill %s~ '%s'\n", var->name, SKILL_NAME(var->_.sn));
		break;

	case VAR_SONG:
		fprintf(fp,"VarSong %s~ '%s'\n", var->name, SONG_NAME(var->_.sn));
		break;

	case VAR_SKILLINFO:
		if(var->_.sk.owner) {
			if( IS_VALID(var->_.sk.token) )
				fprintf(fp,"VarSkInfo %s~ %d %d %d %d ''\n", var->name, (int)var->_.sk.owner->id[0], (int)var->_.sk.owner->id[1], (int)var->_.sk.token->id[0], (int)var->_.sk.token->id[1]);
			else
				fprintf(fp,"VarSkInfo %s~ %d %d 0 0 '%s'\n", var->name, (int)var->_.sk.owner->id[0], (int)var->_.sk.owner->id[1], SKILL_NAME(var->_.sk.sn));
		}
		break;

	case VAR_SKILLINFO_ID:
		fprintf(fp,"VarSkInfo %s~ %d %d %d %d '%s'\n", var->name, (int)var->_.skid.mid[0], (int)var->_.skid.mid[1], (int)var->_.skid.tid[0], (int)var->_.skid.tid[1], SKILL_NAME(var->_.skid.sn));
		break;

	case VAR_PLLIST_STR:
		if(var->_.list && var->_.list->valid) {
			char *str;

			fprintf(fp,"VarListStr %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((str = (char*)iterator_nextdata(&it)))
				fprintf(fp, "String %s~\n", str);

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;

	case VAR_BLLIST_MOB:
		variable_fwrite_uid_list( "VarListMob", var->name, var->_.list, fp);
		break;

	case VAR_BLLIST_OBJ:
		variable_fwrite_uid_list( "VarListObj", var->name, var->_.list, fp);
		break;

	case VAR_BLLIST_TOK:
		variable_fwrite_uid_list( "VarListTok", var->name, var->_.list, fp);
		break;

	case VAR_BLLIST_AREA:
		if(var->_.list && var->_.list->valid) {
			LLIST_AREA_DATA *area;

			fprintf(fp,"VarListArea %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((area = (LLIST_AREA_DATA*)iterator_nextdata(&it))) if( area->area ) {
				fprintf(fp, "Area %ld\n", area->uid);
			}

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;
	case VAR_BLLIST_WILDS:
		if(var->_.list && var->_.list->valid) {
			LLIST_WILDS_DATA *wilds;

			fprintf(fp,"VarListWilds %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((wilds = (LLIST_WILDS_DATA*)iterator_nextdata(&it))) if( wilds->wilds ) {
				fprintf(fp, "Wilds %ld\n", wilds->uid);
			}

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;
	case VAR_BLLIST_ROOM:
		if(var->_.list && var->_.list->valid) {
			LLIST_ROOM_DATA *room;

			fprintf(fp,"VarListRoom %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((room = (LLIST_ROOM_DATA*)iterator_nextdata(&it))) if( room->room ) {
				if(room->room->wilds)
					fprintf(fp, "VRoom %ld %ld %ld %ld\n", room->room->wilds->uid, room->room->x, room->room->y, room->room->z);
				else if(room->room->source)
					fprintf(fp, "CRoom %ld#%ld %ld %ld\n", room->room->source->area->uid, room->room->source->vnum, room->room->id[0], room->room->id[1]);
				else
					fprintf(fp, "Room %ld#%ld\n", room->room->area->uid, room->room->vnum);
			}

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;
	case VAR_BLLIST_EXIT:
		if(var->_.list && var->_.list->valid) {
			LLIST_EXIT_DATA *room;

			fprintf(fp,"VarListExit %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((room = (LLIST_EXIT_DATA*)iterator_nextdata(&it))) if( room->room ) {
				if(room->room->wilds)
					fprintf(fp, "VRoom %ld %ld %ld %ld %d\n", room->room->wilds->uid, room->room->x, room->room->y, room->room->z, room->door);
				else if(room->room->source)
					fprintf(fp, "CRoom %ld#%ld %ld %ld %d\n", room->room->source->area->uid, room->room->source->vnum, room->room->id[0], room->room->id[1], room->door);
				else
					fprintf(fp, "Room %ld#%ld %d\n", room->room->area->uid, room->room->vnum, room->door);
			}

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;

	case VAR_BLLIST_SKILL:
		if(var->_.list && var->_.list->valid) {
			LLIST_SKILL_DATA *skill;

			fprintf(fp,"VarListSkill %s~\n", var->name);
			iterator_start(&it,var->_.list);

			while((skill = (LLIST_SKILL_DATA*)iterator_nextdata(&it))) if( IS_VALID(skill->mob) ) {
				if( IS_VALID(skill->tok) )
					fprintf(fp, "Token %ld %ld %ld %ld\n", skill->mob->id[0], skill->mob->id[1], skill->tok->id[0], skill->tok->id[1]);
				else
					fprintf(fp, "Skill %ld %ld '%s'\n", skill->mob->id[0], skill->mob->id[1], SKILL_NAME(skill->sn));
			}

			iterator_stop(&it);
			fprintf(fp,"End\n");
		}
		break;

	case VAR_DICE:
		fprintf(fp,"VarDice %s~ %d %d %d %ld\n", var->name,
			var->_.dice.number,
			var->_.dice.size,
			var->_.dice.bonus,
			var->_.dice.last_roll);
		break;

	case VAR_MOBINDEX:
		if (var->_.mindex)
			fprintf(fp, "VarMobIndex %ld#%ld\n", var->_.mindex->area->uid, var->_.mindex->vnum);

	case VAR_OBJINDEX:
		if (var->_.oindex)
			fprintf(fp, "VarObjIndex %ld#%ld\n", var->_.oindex->area->uid, var->_.oindex->vnum);

	case VAR_TOKENINDEX:
		if (var->_.tindex)
			fprintf(fp, "VarTokenIndex %ld#%ld\n", var->_.tindex->area->uid, var->_.tindex->vnum);

	case VAR_BLUEPRINT:
		if (var->_.bp)
			fprintf(fp, "VarBlueprint %ld#%ld\n", var->_.bp->area->uid, var->_.bp->vnum);

	case VAR_BLUEPRINT_SECTION:
		if (var->_.bs)
			fprintf(fp, "VarBlueprintSection %ld#%ld\n", var->_.bs->area->uid, var->_.bs->vnum);

	case VAR_DUNGEONINDEX:
		if (var->_.dngindex)
			fprintf(fp, "VarDungeonIndex %ld#%ld\n", var->_.dngindex->area->uid, var->_.dngindex->vnum);

	case VAR_SHIPINDEX:
		if (var->_.shipindex)
			fprintf(fp, "VarShipIndex %ld#%ld\n", var->_.shipindex->area->uid, var->_.shipindex->vnum);
	}
}

bool variable_fread_str_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "String")) {

			if( !variables_append_list_str( vars, name, fread_string(fp)) )
				return FALSE;

		} else
			fread_to_eol(fp);
	}
}

bool variable_fread_uid_list(ppVARIABLE vars, char *name, int type, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "UID")) {

			if( !variables_append_list_uid( vars, name, type, fread_number(fp), fread_number(fp)) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}
}

bool variable_fread_room_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "VRoom")) {

			if( !variables_append_list_room_id( vars, name, fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp), 0) )
				return FALSE;

		} else if (!str_cmp(word, "CRoom")) {

			if( !variables_append_list_room_id( vars, name, 0, fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp)) )
				return FALSE;

		} else if (!str_cmp(word, "Room")) {

			if( !variables_append_list_room_id( vars, name, 0, fread_number(fp), fread_number(fp), 0, 0) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}

}

bool variable_fread_exit_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "VRoom")) {

			if( !variables_append_list_door_id( vars, name, fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp), 0) )
				return FALSE;

		} else if (!str_cmp(word, "CRoom")) {

			if( !variables_append_list_door_id( vars, name, 0, fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp)) )
				return FALSE;

		} else if (!str_cmp(word, "Room")) {

			if( !variables_append_list_door_id( vars, name, 0, fread_number(fp), fread_number(fp), 0, 0, fread_number(fp)) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}

}

bool variable_fread_area_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "Area")) {

			if( !variables_append_list_area_id( vars, name, fread_number(fp)) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}
}


bool variable_fread_area_region_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "AreaRegion")) {
			long a = fread_number(fp);
			long b = fread_number(fp);

			if( !variables_append_list_area_region_id( vars, name, a, b) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}
}

bool variable_fread_wilds_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "Wilds")) {

			if( !variables_append_list_wilds_id( vars, name, fread_number(fp)) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}

}

bool variable_fread_skill_list(ppVARIABLE vars, char *name, FILE *fp)
{
	char *word;

	for(; ;) {
		word   = feof(fp) ? "End" : fread_word(fp);

		if (!str_cmp(word, "End"))
			return TRUE;

		else if (!str_cmp(word, "Skill")) {

			if( !variables_append_list_skill_id (vars, name, fread_number(fp), fread_number(fp), 0, 0, skill_lookup(fread_word(fp))) )
				return FALSE;

		} else if (!str_cmp(word, "Token")) {

			if( !variables_append_list_skill_id (vars, name, fread_number(fp), fread_number(fp), fread_number(fp), fread_number(fp), 0) )
				return FALSE;

		} else
			fread_to_eol(fp);

	}

}

int variable_fread_type(char *str)
{
	if( !str_cmp( str, "VarBool" ) ) return VAR_BOOLEAN;
	if( !str_cmp( str, "VarInt" ) ) return VAR_INTEGER;
	if( !str_cmp( str, "VarStr" ) ) return VAR_STRING_S;
	if( !str_cmp( str, "VarRoom" ) ) return VAR_ROOM;
	if( !str_cmp( str, "VarCRoom" ) || !str_cmp( str, "VarRoomC" ) ) return VAR_CLONE_ROOM;
	if( !str_cmp( str, "VarVRoom" ) || !str_cmp( str, "VarRoomV" ) ) return VAR_WILDS_ROOM;
	if( !str_cmp( str, "VarExit" ) ) return VAR_DOOR;
	if( !str_cmp( str, "VarCExit" ) || !str_cmp( str, "VarExitC" ) ) return VAR_CLONE_DOOR;
	if( !str_cmp( str, "VarVExit" ) || !str_cmp( str, "VarExitV" ) ) return VAR_WILDS_DOOR;
	if( !str_cmp( str, "VarMob" ) ) return VAR_MOBILE_ID;
	if( !str_cmp( str, "VarObj" ) ) return VAR_OBJECT_ID;
	if( !str_cmp( str, "VarTok" ) ) return VAR_TOKEN_ID;
	if( !str_cmp( str, "VarArea" ) ) return VAR_AREA_ID;
	if( !str_cmp( str, "VarAreaRegion" ) ) return VAR_AREA_REGION_ID;
	if( !str_cmp( str, "VarWilds" ) ) return VAR_WILDS_ID;
	if( !str_cmp( str, "VarChurch" ) ) return VAR_CHURCH_ID;
	if( !str_cmp( str, "VarSkill" ) ) return VAR_SKILL;
	if( !str_cmp( str, "VarSkInfo" ) ) return VAR_SKILLINFO_ID;
	if( !str_cmp( str, "VarListMob" ) ) return VAR_BLLIST_MOB;
	if( !str_cmp( str, "VarListObj" ) ) return VAR_BLLIST_OBJ;
	if( !str_cmp( str, "VarListTok" ) ) return VAR_BLLIST_TOK;
	if( !str_cmp( str, "VarListRoom" ) ) return VAR_BLLIST_ROOM;
	if( !str_cmp( str, "VarListExit" ) ) return VAR_BLLIST_EXIT;
	if( !str_cmp( str, "VarListSkill" ) ) return VAR_BLLIST_SKILL;
	if( !str_cmp( str, "VarListStr" ) ) return VAR_PLLIST_STR;
	if( !str_cmp( str, "VarListArea" ) ) return VAR_BLLIST_AREA;
	if( !str_cmp( str, "VarListAreaRegion" ) ) return VAR_BLLIST_AREA_REGION;
	if( !str_cmp( str, "VarListWilds" ) ) return VAR_BLLIST_WILDS;
	if( !str_cmp( str, "VarDice" ) ) return VAR_DICE;
	if( !str_cmp( str, "VarSong" ) ) return VAR_SONG;
	if( !str_cmp( str, "VarMobIndex" ) ) return VAR_MOBINDEX;
	if( !str_cmp( str, "VarObjIndex" ) ) return VAR_OBJINDEX;
	if( !str_cmp( str, "VarTokenIndex" ) ) return VAR_TOKENINDEX;
	if( !str_cmp( str, "VarBlueprint" ) ) return VAR_BLUEPRINT;
	if( !str_cmp( str, "VarBlueprintSection" ) ) return VAR_BLUEPRINT_SECTION;
	if( !str_cmp( str, "VarDungeonIndex" ) ) return VAR_DUNGEONINDEX;
	if( !str_cmp( str, "VarShipIndex" ) ) return VAR_SHIPINDEX;

	return VAR_UNKNOWN;
}

bool variable_fread(ppVARIABLE vars, int type, FILE *fp)
{
	char *name;
	unsigned long a, b, c, d;

	name = fread_string(fp);

	switch(type) {
	case VAR_BOOLEAN:
		return variables_setsave_boolean(vars, name, (fread_number(fp) != 0), TRUE);

	case VAR_INTEGER:
		return variables_setsave_integer(vars, name, fread_number(fp), TRUE);

	case VAR_STRING:
	case VAR_STRING_S:	// They all get changed to shared on load...
		return variables_setsave_string(vars, name, fread_string(fp), TRUE, TRUE);

	case VAR_ROOM:
		{
			WNUM_LOAD wnum = fread_widevnum(fp, 0);
			ROOM_INDEX_DATA *room = get_room_index_auid(wnum.auid, wnum.vnum);
			return room && variables_setsave_room(vars, name, room, TRUE);
		}

	case VAR_CLONE_ROOM:
		{
			WNUM_LOAD wnum = fread_widevnum(fp, 0);
			ROOM_INDEX_DATA *room = get_room_index_auid(wnum.auid, wnum.vnum);
			int x = fread_number(fp);
			int y = fread_number(fp);

			// Wait to resolve until AFTER all persistant rooms are loaded
			return room && variables_set_clone_room(vars, name, room, x, y, TRUE);
		}

	case VAR_WILDS_ROOM:
		{
			int wuid = fread_number(fp);
			WILDS_DATA *wilds;
			int x = fread_number(fp);
			int y = fread_number(fp);

			wilds = get_wilds_from_uid(NULL, wuid);
			if( wilds ) {
				ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, x, y);

				// Go ahead and load it
				if( !room )
					room = create_wilds_vroom(wilds,x,y);

				return room && variables_setsave_room(vars, name, room, TRUE);
			} else
				return variables_set_wilds_room(vars,name,wuid, x, y, TRUE);
		}

	case VAR_DOOR:
		{
			WNUM_LOAD wnum = fread_widevnum(fp, 0);
			ROOM_INDEX_DATA *room = get_room_index_auid(wnum.auid, wnum.vnum);

			return room && variables_set_door(vars, name, room, fread_number(fp), TRUE);
		}

	case VAR_CLONE_DOOR:
		{
			WNUM_LOAD wnum = fread_widevnum(fp, 0);
			ROOM_INDEX_DATA *room = get_room_index_auid(wnum.auid, wnum.vnum);

			int x = fread_number(fp);
			int y = fread_number(fp);
			int door = fread_number(fp);

			return room && variables_set_clone_door(vars, name, room, x, y, door, TRUE);
		}

	case VAR_WILDS_DOOR:
		{
			int wuid = fread_number(fp);
			WILDS_DATA *wilds;
			int x = fread_number(fp);
			int y = fread_number(fp);

			wilds = get_wilds_from_uid(NULL, wuid);
			if( wilds ) {
				ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, x, y);

				// Go ahead and load it
				if( !room )
					room = create_wilds_vroom(wilds,x,y);

				return room && variables_set_door(vars, name, room, fread_number(fp), TRUE);
			} else
				return variables_set_wilds_door(vars, name, wuid, x, y, fread_number(fp), TRUE);
		}

	case VAR_MOBILE_ID:
		a = fread_number(fp);
		b = fread_number(fp);
		return variables_set_mobile_id(vars, name, a, b, TRUE);

	case VAR_OBJECT_ID:
		a = fread_number(fp);
		b = fread_number(fp);
		return variables_set_object_id(vars, name, a, b, TRUE);

	case VAR_TOKEN_ID:
		a = fread_number(fp);
		b = fread_number(fp);
		return variables_set_token_id(vars, name, a, b, TRUE);

	case VAR_AREA_ID:
		return variables_set_area_id(vars, name, fread_number(fp), TRUE);

	case VAR_AREA_REGION_ID:
		a = fread_number(fp);
		b = fread_number(fp);
		return variables_set_area_region_id(vars, name, a, b, TRUE);

	case VAR_WILDS_ID:
		return variables_set_wilds_id(vars, name, fread_number(fp), TRUE);

	case VAR_CHURCH_ID:
		return variables_set_church_id(vars, name, fread_number(fp), TRUE);

	case VAR_SKILL:
		return variables_setsave_skill(vars, name, skill_lookup(fread_word(fp)), TRUE);

	case VAR_SKILLINFO_ID:
		a = fread_number(fp);
		b = fread_number(fp);
		c = fread_number(fp);
		d = fread_number(fp);

		return variables_set_skillinfo_id (vars, name, a, b, c, d, skill_lookup(fread_word(fp)) , TRUE);

	case VAR_SONG:
		return variables_setsave_song(vars, name, song_lookup(fread_word(fp)), TRUE);

	case VAR_PLLIST_STR:
		if( variables_set_list(vars, name, VAR_PLLIST_STR, TRUE) )
			return variable_fread_str_list(vars, name, fp);

	case VAR_BLLIST_MOB:
	case VAR_BLLIST_OBJ:
	case VAR_BLLIST_TOK:
		if( variables_set_list(vars, name, type, TRUE) )
			return variable_fread_uid_list(vars, name, type, fp);
		else
			return FALSE;

	case VAR_BLLIST_AREA:
		if( variables_set_list(vars, name, VAR_BLLIST_AREA, TRUE) )
			return variable_fread_area_list(vars, name, fp);
		else
			return FALSE;

	case VAR_BLLIST_AREA_REGION:
		if( variables_set_list(vars, name, VAR_BLLIST_AREA_REGION, TRUE) )
			return variable_fread_area_region_list(vars, name, fp);
		else
			return FALSE;

	case VAR_BLLIST_WILDS:
		if( variables_set_list(vars, name, VAR_BLLIST_WILDS, TRUE) )
			return variable_fread_wilds_list(vars, name, fp);
		else
			return FALSE;

	case VAR_BLLIST_ROOM:
		if( variables_set_list(vars, name, VAR_BLLIST_ROOM, TRUE) )
			return variable_fread_room_list(vars, name, fp);
		else
			return FALSE;

	case VAR_BLLIST_EXIT:
		if( variables_set_list(vars, name, VAR_BLLIST_EXIT, TRUE) )
			return variable_fread_exit_list(vars, name, fp);
		else
			return FALSE;

	case VAR_BLLIST_SKILL:
		if( variables_set_list(vars, name, VAR_BLLIST_SKILL, TRUE) )
			return variable_fread_skill_list(vars, name, fp);
		else
			return FALSE;

	case VAR_DICE:
		{
			DICE_DATA xyz;

			xyz.number = fread_number(fp);
			xyz.size = fread_number(fp);
			xyz.bonus = fread_number(fp);
			xyz.last_roll = fread_number(fp);

			return variables_setsave_dice(vars, name, &xyz, TRUE);

		}

		return FALSE;
	
	case VAR_MOBINDEX:
		return variables_setsave_mobindex_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_OBJINDEX:
		return variables_setsave_objindex_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_TOKENINDEX:
		return variables_setsave_tokenindex_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_BLUEPRINT:
		return variables_setsave_blueprint_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_BLUEPRINT_SECTION:
		return variables_setsave_blueprint_section_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_DUNGEONINDEX:
		return variables_setsave_dungeonindex_load(vars, name, fread_widevnum(fp, 0), TRUE);
	case VAR_SHIPINDEX:
		return variables_setsave_shipindex_load(vars, name, fread_widevnum(fp, 0), TRUE);

	}

	// Ignore rest
	fread_to_eol(fp);
	return TRUE;
}

void script_varclearon(SCRIPT_VARINFO *info, VARIABLE **vars, char *argument, SCRIPT_PARAM *arg)
{
	char name[MIL];

	if(!vars) return;

	// Get name
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_STRING) return;

	strcpy(name, arg->d.str);

	if(!name[0]) return;

	variable_remove(vars,name);
}


bool olc_varset(ppVARIABLE index_vars, CHAR_DATA *ch, char *argument)
{
    char name[MIL];
    char type[MIL];
    char yesno[MIL];
    bool saved;

    if (argument[0] == '\0') {
	send_to_char("Syntax:  varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
	return FALSE;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, type);
    argument = one_argument(argument, yesno);

    if(!variable_validname(name)) {
	send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
	return FALSE;
    }

    saved = !str_cmp(yesno,"yes");

    if(!argument[0]) {
	send_to_char("Set what on the variable?\n\r", ch);
	return FALSE;
    }

    if(!str_cmp(type,"room")) {
		WNUM wnum;

		if(!parse_widevnum(argument, ch->in_room->area, &wnum)) {
			send_to_char("Specify a room widevnum.\n\r", ch);
			return FALSE;
		}

		WNUM_LOAD wnum_load;
		wnum_load.auid = wnum.pArea ? wnum.pArea->uid : 0;
		wnum_load.vnum = wnum.vnum;

		variables_setindex_room(index_vars,name,wnum_load,saved);
    } else if(!str_cmp(type,"string"))
		variables_setindex_string(index_vars,name,argument,FALSE,saved);
    else if(!str_cmp(type,"number"))
	{
		if(!is_number(argument)) {
			send_to_char("Specify an integer.\n\r", ch);
			return FALSE;
		}

		variables_setindex_integer(index_vars,name,atoi(argument),saved);
    }
	else if(!str_cmp(type,"skill"))
	{
		int sn = skill_lookup(argument);
		if (sn <= 0)
		{
			send_to_char("No such skill exists.\n\r", ch);
			return FALSE;
		}
		
		variables_setindex_skill(index_vars,name,sn,saved);
	}
	else if(!str_cmp(type,"song"))
	{
		int sn = song_lookup(argument);
		if (sn <= 0)
		{
			send_to_char("No such song exists.\n\r", ch);
			return FALSE;
		}
		
		variables_setindex_song(index_vars,name,sn,saved);
	}
	else
	{
		send_to_char("Invalid type of variable.\n\r", ch);
		return FALSE;
    }
    send_to_char("Variable set.\n\r", ch);
    return TRUE;
}

bool olc_varclear(ppVARIABLE index_vars, CHAR_DATA *ch, char *argument)
{
    if (argument[0] == '\0') {
		send_to_char("Syntax:  varclear <name>\n\r", ch);
		return FALSE;
    }

    if(!variable_validname(argument)) {
		send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
		return FALSE;
    }

    if(!variable_remove(index_vars,argument)) {
		send_to_char("No such variable defined.\n\r", ch);
		return FALSE;
    }

    send_to_char("Variable cleared.\n\r", ch);
    return TRUE;
}

void olc_show_index_vars(BUFFER *buffer, pVARIABLE index_vars)
{
	char buf[MSL];
	if (index_vars)
	{
		pVARIABLE var;
		int cnt;

		for (cnt = 0, var = index_vars; var; var = var->next) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
			add_buf(buffer, buf);

			for (var = index_vars; var; var = var->next) {
				switch(var->type) {
				case VAR_INTEGER:
					sprintf(buf, "{x%-20.20s {GNUMBER     {Y%c   {W%d{x\n\r", var->name,var->save?'Y':'N',var->_.i);
					break;
				case VAR_STRING:
				case VAR_STRING_S:
					sprintf(buf, "{x%-20.20s {GSTRING     {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N',var->_.s?var->_.s:"(empty)");
					break;
				case VAR_ROOM:
					if(var->_.r && var->_.r->vnum > 0)
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%d{R){x\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->vnum);
					else
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r",var->name,var->save?'Y':'N');
					break;
				case VAR_SKILL:
					if(var->_.sn > 0)
						sprintf(buf, "{x%-20.20s {GSKILL      {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N', SKILL_NAME(var->_.sn));
					else
						sprintf(buf, "{x%-20.20s {GSKILL      {Y%c   {W-invalid-{x\n\r", var->name,var->save?'Y':'N');
					break;
				case VAR_SONG:
					if(var->_.sn >= 0)
						sprintf(buf, "{x%-20.20s {GSONG       {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N', SONG_NAME(var->_.sn));
					else
						sprintf(buf, "{x%-20.20s {GSONG       {Y%c   {W-invalid-{x\n\r", var->name,var->save?'Y':'N');
					break;
				default:
					continue;
				}
				add_buf(buffer, buf);
			}
		}
	}
}

void olc_save_index_vars(FILE *fp, pVARIABLE index_vars, AREA_DATA *pRefArea)
{
	if(index_vars) {
		for(pVARIABLE var = index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %s\n", var->name, var->save, widevnum_string(var->_.r->area, var->_.r->vnum, pRefArea));
			else if(var->type == VAR_SKILL && var->_.sn > 0 )
				fprintf(fp, "VarSkill %s~ %d '%s'\n", var->name, var->save, SKILL_NAME(var->_.sn));
			else if(var->type == VAR_SONG && var->_.sn >= 0 )
				fprintf(fp, "VarSong %s~ %d '%s'\n", var->name, var->save, SONG_NAME(var->_.sn));
		}
	}
}

bool olc_load_index_vars(FILE *fp, char *word, ppVARIABLE index_vars, AREA_DATA *pRefArea)
{
	if (!str_cmp(word, "VarInt")) {
		char *name;
		int value;
		bool saved;

		name = fread_string(fp);
		saved = fread_number(fp);
		value = fread_number(fp);

		variables_setindex_integer (index_vars,name,value,saved);
		return TRUE;
	}

	if (!str_cmp(word, "VarStr")) {
		char *name;
		char *str;
		bool saved;

		name = fread_string(fp);
		saved = fread_number(fp);
		str = fread_string(fp);

		variables_setindex_string (index_vars,name,str,FALSE,saved);
		return TRUE;
	}

	if (!str_cmp(word, "VarRoom")) {
		char *name;
		WNUM_LOAD value;
		bool saved;

		name = fread_string(fp);
		saved = fread_number(fp);
		value = fread_widevnum(fp, 0);

		variables_setindex_room (index_vars,name,value,saved);
		return TRUE;
	}

	if (!str_cmp(word, "VarSkill"))
	{
		char *name;
		int sn;
		bool saved;

		name = fread_string(fp);
		saved = fread_number(fp);
		sn = skill_lookup(fread_word(fp));

		if (sn > 0)
			variables_setindex_skill(index_vars,name,sn,saved);
		return TRUE;
	}

	if (!str_cmp(word, "VarSong"))
	{
		char *name;
		int sn;
		bool saved;

		name = fread_string(fp);
		saved = fread_number(fp);
		sn = song_lookup(fread_word(fp));

		if (sn >= 0)
			variables_setindex_song(index_vars,name,sn,saved);
		return TRUE;
	}

	return FALSE;
}

////////////////////////////////////////////////
//
// Script commands:
//
// varset name type value
//
//   types:				CALL
//     integer <number>			variable_set_integer
//     string <string>			variable_set_string (shared:FALSE)
//     room <entity>			variable_set_room
//     room <vnum>			variable_set_room
//     mobile <entity>			variable_set_mobile
//     mobile <location> <vnum>		variable_set_mobile
//     mobile <location> <name>		variable_set_mobile
//     mobile <mob_list> <vnum>		variable_set_mobile
//     mobile <mob_list> <name>		variable_set_mobile
//     player <entity>			variable_set_mobile
//     player <name>			variable_set_mobile
//     object <entity>			variable_set_object
//     object <location> <vnum>		variable_set_object
//     object <location> <name>		variable_set_object
//     object <obj_list> <vnum>		variable_set_object
//     object <obj_list> <name>		variable_set_object
//     carry <mobile> <vnum>		variable_set_object
//     carry <mobile> <name>		variable_set_object
//     content <object> <vnum>		variable_set_object
//     content <object> <name>		variable_set_object
//     token <token_list> <vnum>	variable_set_token
//     token <token_list> <name>	variable_set_token
//
// Note: <entity> refers to $( ) use
//
// varclear name
// varcopy old new
// varsave name on|off
//	Flags the variable for saving depending on the parameter.
//
// Variable names should be restricted to...
//	1) starting with an alpha character (A-Z and a-z)
//	2) followed by zero or more alphanumeric characters
// It would make checking for them much easier
//

