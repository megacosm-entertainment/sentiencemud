/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "tables.h"
#include "scripts.h"
#include "recycle.h"
#include "wilds.h"
#include "olc.h"
//#define DEBUG_MODULE
#include "debug.h"


LLIST *trigger_list = NULL;
int top_trigger_type = TRIG__MAX;

int script_security = INIT_SCRIPT_SECURITY;
int script_call_depth = 0;
int script_lastreturn = PRET_EXECUTED;
bool script_remotecall = FALSE;
bool script_destructed = FALSE;
bool wiznet_script = FALSE;
bool script_force_execute = FALSE;	// Executes the script even if disabled
SCRIPT_CB *script_call_stack = NULL;

ROOM_INDEX_DATA room_used_for_wilderness;
ROOM_INDEX_DATA room_pointer_vlink;
ROOM_INDEX_DATA room_pointer_environment;

bool opc_skip_block(SCRIPT_CB *block,int level,bool endblock);
bool is_stat( const struct flag_type *flag_table );
void delete_list_uid_data(void *ptr);

char *	const	dir_name_phrase	[]		=
{
    "0 (north)", "1 (east)", "2 (south)", "3 (west)", "4 (up)", "5 (down)", "6 (northeast)",  "7 (northwest)", "8 (southeast)", "9 (southwest)"
};


void script_clear_mobile(CHAR_DATA *ptr)
{
	register int lp;
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.mob == ptr) {
			stack->info.mob = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		if(stack->info.ch == ptr) stack->info.ch = NULL;
		if(stack->info.vch == ptr) stack->info.vch = NULL;
		if(stack->info.rch == ptr) stack->info.rch = NULL;
		for(lp = 0; lp < stack->loop; lp++) {
			if(stack->loops[lp].d.l.type == ENT_MOBILE) {
				if(stack->loops[lp].d.l.next.m == ptr) {
					if(stack->loops[lp].d.l.cur.m && ptr != stack->loops[lp].d.l.cur.m->next_in_room)
						stack->loops[lp].d.l.next.m = stack->loops[lp].d.l.cur.m->next_in_room;
				}
			}
		}

		stack = stack->next;
	}
}

void script_clear_object(OBJ_DATA *ptr)
{
	register int lp;
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.obj == ptr) {
			stack->info.obj = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		if(stack->info.obj1 == ptr) stack->info.obj1 = NULL;
		if(stack->info.obj2 == ptr) stack->info.obj2 = NULL;
		for(lp = 0; lp < stack->loop; lp++) {
			if(stack->loops[lp].d.l.type == ENT_OBJECT) {
				if(stack->loops[lp].d.l.next.o == ptr) {
					if(stack->loops[lp].d.l.cur.o && ptr != stack->loops[lp].d.l.cur.o->next_content)
						stack->loops[lp].d.l.next.o = stack->loops[lp].d.l.cur.o->next_content;
				}
			}
		}
		stack = stack->next;
	}
}

void script_clear_room(ROOM_INDEX_DATA *ptr)
{
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.room == ptr) {
			stack->info.room = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		stack = stack->next;
	}
}

void script_clear_token(TOKEN_DATA *ptr)
{
	register int lp;
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.token == ptr) {
			stack->info.token = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		for(lp = 0; lp < stack->loop; lp++) {
			if(stack->loops[lp].d.l.type == ENT_TOKEN) {
				if(stack->loops[lp].d.l.next.t == ptr) {
					if(stack->loops[lp].d.l.cur.t && ptr != stack->loops[lp].d.l.cur.t->next)
						stack->loops[lp].d.l.next.t = stack->loops[lp].d.l.cur.t->next;
				}
			}
		}
		stack = stack->next;
	}
}

void script_clear_affect(AFFECT_DATA *ptr)
{
	register int lp;
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		for(lp = 0; lp < stack->loop; lp++) {
			if(stack->loops[lp].d.l.type == ENT_AFFECT) {
				if(stack->loops[lp].d.l.next.aff == ptr) {
					if(stack->loops[lp].d.l.cur.aff && ptr != stack->loops[lp].d.l.cur.aff->next)
						stack->loops[lp].d.l.next.aff = stack->loops[lp].d.l.cur.aff->next;
				}
			}
		}
		stack = stack->next;
	}
}

void script_clear_list(register void *owner)
{
	register int lp;
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		for(lp = 0; lp < stack->loop; lp++) {
			if(stack->loops[lp].d.l.owner == owner) {
				stack->loops[lp].d.l.owner = NULL;
				stack->loops[lp].d.l.owner_type = ENT_UNKNOWN;
				stack->loops[lp].d.l.cur.raw = NULL;
				stack->loops[lp].d.l.next.raw = NULL;
			}
		}
		stack = stack->next;
	}
}

void script_clear_instance(INSTANCE *ptr)
{
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.instance == ptr) {
			stack->info.instance = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		stack = stack->next;
	}
}

void script_clear_dungeon(DUNGEON *ptr)
{
	register SCRIPT_CB *stack = script_call_stack;

	while(stack) {
		if(stack->info.dungeon == ptr) {
			stack->info.dungeon = NULL;
			stack->info.var = NULL;
			stack->info.targ = NULL;
			SET_BIT(stack->flags,SCRIPTEXEC_HALT);
		}
		stack = stack->next;
	}
}


void script_mobile_addref(CHAR_DATA *ch)
{
	if(IS_VALID(ch) && IS_NPC(ch) && ch->progs)
		ch->progs->script_ref++;
}

bool script_mobile_remref(CHAR_DATA *ch)
{
	if(IS_VALID(ch) && IS_NPC(ch) && ch->progs) {
		if( ch->progs->script_ref > 0 && !--ch->progs->script_ref ) {
			if( ch->progs->extract_when_done ) {
				// Remove!
				ch->progs->extract_when_done = FALSE;
				extract_char(ch, ch->progs->extract_fPull);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void script_object_addref(OBJ_DATA *obj)
{
	if(IS_VALID(obj) && obj->progs)
		obj->progs->script_ref++;
}

bool script_object_remref(OBJ_DATA *obj)
{
	if(IS_VALID(obj) && obj->progs) {
		if( obj->progs->script_ref > 0 && !--obj->progs->script_ref ) {
			if( obj->progs->extract_when_done ) {
				// Remove!
				obj->progs->extract_when_done = FALSE;
				extract_obj(obj);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void script_room_addref(ROOM_INDEX_DATA *room)
{
	if((room->source || IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) && room->progs)
		room->progs->script_ref++;
}

bool script_room_remref(ROOM_INDEX_DATA *room)
{
	if((room->source || IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) && room->progs) {
		if( room->progs->script_ref > 0 && !--room->progs->script_ref ) {
			if( room->progs->extract_when_done ) {
				// Remove!
				room->progs->extract_when_done = FALSE;
				if(room->source)
					extract_clone_room(room, room->id[0], room->id[1],false);
				else	// Is a WILDS room
					destroy_wilds_vroom(room);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void script_token_addref(TOKEN_DATA *token)
{
	if(IS_VALID(token) && token->progs)
		token->progs->script_ref++;
}

bool script_token_remref(TOKEN_DATA *token)
{
	if(IS_VALID(token) && token->progs) {
		if( token->progs->script_ref > 0 && !--token->progs->script_ref ) {
			if( token->progs->extract_when_done ) {
				// Remove!
				token->progs->extract_when_done = FALSE;
				extract_token(token);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void script_instance_addref(INSTANCE *instance)
{
	if(IS_VALID(instance) && instance->progs)
		instance->progs->script_ref++;
}

bool script_instance_remref(INSTANCE *instance)
{
	if(IS_VALID(instance) && instance->progs) {
		if( instance->progs->script_ref > 0 && !--instance->progs->script_ref ) {
			if( instance->progs->extract_when_done ) {
				// Remove!
				instance->progs->extract_when_done = FALSE;
				extract_instance(instance);
				return TRUE;
			}
		}
	}

	return FALSE;
}

void script_dungeon_addref(DUNGEON *dungeon)
{
	if(IS_VALID(dungeon) && dungeon->progs)
		dungeon->progs->script_ref++;
}

bool script_dungeon_remref(DUNGEON *dungeon)
{
	if(IS_VALID(dungeon) && dungeon->progs) {
		if( dungeon->progs->script_ref > 0 && !--dungeon->progs->script_ref ) {
			if( dungeon->progs->extract_when_done ) {
				// Remove!
				dungeon->progs->extract_when_done = FALSE;
				extract_dungeon(dungeon);
				return TRUE;
			}
		}
	}

	return FALSE;
}

ENT_FIELD *script_entity_fields(int type)
{
	int i;

	for(i=0; entity_type_info[i].type_min < ENT_MAX; i++)
		if( (type >= entity_type_info[i].type_min) && (type <= entity_type_info[i].type_max))
			return entity_type_info[i].fields;

	return NULL;
}

bool script_entity_allow_vars(int type)
{
	int i;

	for(i=0; entity_type_info[i].type_min < ENT_MAX; i++)
		if( (type >= entity_type_info[i].type_min) && (type <= entity_type_info[i].type_max))
			return entity_type_info[i].allow_vars;

	return FALSE;
}

bool script_entity_allow_index(int type)
{
	int i;

	for(i=0; entity_type_info[i].type_min < ENT_MAX; i++)
		if( (type >= entity_type_info[i].type_min) && (type <= entity_type_info[i].type_max))
			return entity_type_info[i].allow_index;

	return FALSE;
}

//void compile_error_show(char *msg);
ENT_FIELD *entity_type_lookup(char *name, ENT_FIELD *list)
{
	int i;
//	char buf[MSL];

	if(!list) return NULL;

	for(i=0;list[i].name;i++) {
//		compile_error_show(buf);
//		sprintf(buf,"entity_type_lookup: '%s' '%s'",list[i].name,name);
		if(!str_cmp(name,list[i].name)) {
//			sprintf(buf,"entity_type_lookup: '%s' found",name);
//			compile_error_show(buf);
			return &list[i];
		}
	}

//	sprintf(buf,"entity_type_lookup: '%s' NOT found",name);
//	compile_error_show(buf);
	return NULL;
}

bool script_expression_push(STACK *stk,int val)
{
	if(stk->t >= MAX_STACK) return FALSE;
	stk->s[stk->t++] = val;
	return TRUE;
}

bool script_expression_push_operator(STACK *stk,int op)
{
	if(script_expression_tostack[op] == STK_MAX) return FALSE;
	return script_expression_push(stk,script_expression_tostack[op]);
}


int get_operator(char *keyword)
{
	register int i;
	for(i = 0; script_operators[i]; i++)
		if(!str_cmp(script_operators[i], keyword))
			return(i);
	return -1;
}

int ifcheck_lookup(char *name, int type)
{
	register int i;

	for(i=0;ifcheck_table[i].name;i++)
		if((ifcheck_table[i].type & type) && !str_cmp(name,ifcheck_table[i].name))
			return i;

	return -1;
}

char *ifcheck_get_value(SCRIPT_VARINFO *info,IFCHECK_DATA *ifc,char *text,long *ret,bool *valid)
{
	int i;
	SCRIPT_PARAM *argv[IFC_MAXPARAMS];
	char *argument;

	*valid = FALSE;

	// Validate parameters
	if(!ifc || !ret) return NULL;

	if(!ifc->func) return NULL;

	// Clear variables
	for(i = 0; i < IFC_MAXPARAMS; i++)
		argv[i] = new_script_param();

	text = skip_whitespace(text);
	argument = text;

	// Stop when there the param list is full, there's no more text or it hits an
	//	operator
	for(i=0;argument && *argument && *argument != ESCAPE_END && *argument != '=' && *argument != '<' &&
		*argument != '>' && *argument != '!' && *argument != '&' && i<IFC_MAXPARAMS;i++) {
//		if(wiznet_script) {
//			sprintf(buf,"*argument = %02.2X (%c)", *argument, isprint(*argument) ? *argument : ' ');
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
		clear_buf(argv[i]->buffer);
		argument = expand_argument(info,argument,argv[i]);
//		if(wiznet_script) {
//			sprintf(buf,"argv[%d].type = %d (%s)", i, argv[i].type, ifcheck_param_type_names[argv[i].type]);
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
	}
//	if(wiznet_script) {
//		sprintf(buf,"args = %d", i);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}
	if(info && (ifc->func)(info,info->mob,info->obj,info->room,info->token,ret,i,argv))
		*valid = TRUE;

//	if(wiznet_script) {
//		sprintf(buf,"ret = %d", *ret);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}
	DBG2EXITVALUE1(PTR,argument);
	for(i = 0; i < IFC_MAXPARAMS; i++)
		free_script_param(argv[i]);

	return argument;
}

int ifcheck_comparison(SCRIPT_VARINFO *info, short param, char *rest, SCRIPT_PARAM *arg)
{
	long lhs, oper, rhs;
	char *text, *p, buf[MIL], buf2[MSL];
	bool valid;
	IFCHECK_DATA *ifc;
	const struct flag_type *stat_table = NULL;
	long def_value = NO_FLAG;

	if(!info) return -1;	// Error

	if(param < -1 || param >= CHK_MAXIFCHECKS)
		 return -1;

	if(param == -1) {
		text = expand_argument(info,rest,arg);
		if(!text) return -1;

		if( arg->type == ENT_BOOLEAN )
			return arg->d.boolean ? 1 : 0;

		if (arg->type == ENT_STAT)
		{
			lhs = arg->d.stat.value;
			stat_table = arg->d.stat.table;
			def_value = arg->d.stat.def_value;
		}
		else
		{
			if( arg->type != ENT_NUMBER )
				return -1;

			lhs = arg->d.num;
		}

	} else {
		ifc = &ifcheck_table[param];

		if(wiznet_script) {
			sprintf(buf2,"Doing ifcheck: %d, '%s'", param, ifc->name);
			wiznet(buf2,NULL,NULL,WIZ_SCRIPTS,0,0);
		}

		text = ifcheck_get_value(info,ifc,rest,&lhs,&valid);

		if(!valid) return FALSE;

		if(!ifc->numeric) return (lhs > 0);
	}

	text = one_argument(text, buf);

	oper = get_operator(buf);
	if (oper < 0) return FALSE;

	p = expand_argument(info,text,arg);
	if(!p || p == text) {
		return -1;
	}

	switch(arg->type) {
	case ENT_NUMBER: rhs = arg->d.num; break;
	case ENT_STRING:
		if(stat_table)
		{
			rhs = script_stat_lookup(arg->d.str,stat_table,def_value);
			break;
		}
		if(is_number(arg->d.str)) {
			rhs = atoi(arg->d.str);
			break;
		}
	default:
		return FALSE;
	}

	switch(oper) {
	case EVAL_EQ:	return (lhs == rhs);
	case EVAL_GE:	return (lhs >= rhs);
	case EVAL_LE:	return (lhs <= rhs);
	case EVAL_NE:	return (lhs != rhs);
	case EVAL_GT:	return (lhs > rhs);
	case EVAL_LT:	return (lhs < rhs);
	case EVAL_MASK:	return (lhs & rhs);
	default:	return FALSE;
	}
}

int boolexp_evaluate(SCRIPT_CB *block, BOOLEXP *be, SCRIPT_PARAM *arg)
{
	int ret;
	if(!block) return -1;	// Error

	switch(be->type) {
	case BOOLEXP_TRUE:
		return ifcheck_comparison(&block->info, be->param, be->rest, arg);

	case BOOLEXP_NOT:
		ret = ifcheck_comparison(&block->info, be->param, be->rest, arg);

		if( ret < 0 ) return -1;

		return ret ? FALSE : TRUE;

	case BOOLEXP_AND:
		ret = boolexp_evaluate(block, be->left, arg);
		if( ret < 0 ) return -1;

		if( ret == FALSE ) return FALSE;	// Short circuit FALSE

		ret = boolexp_evaluate(block, be->right, arg);
		if( ret < 0 ) return -1;

		return ret ? TRUE : FALSE;

	case BOOLEXP_OR:
		ret = boolexp_evaluate(block, be->left, arg);
		if( ret < 0 ) return -1;

		if( ret == TRUE ) return TRUE;		// Short circuit TRUE

		ret = boolexp_evaluate(block, be->right, arg);
		if( ret < 0 ) return -1;

		return ret ? TRUE : FALSE;
	}


	return FALSE;
}

bool opc_skip_to_label(SCRIPT_CB *block,int op,int id,bool dir)
{
	int line, last;
	SCRIPT_CODE *code;
	char buf[MIL];

	code = block->script->code;
	last = block->script->lines;

	if(wiznet_script) {
		sprintf(buf,"Skipping to %s with ID %d.", opcode_names[op], id);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
	}

	if(dir) {	// Forward, after the loop
		for(line = block->line; line < last; line++) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
			if(code[line].opcode == op && code[line].label == id) {
				block->line = line+1;
				DBG2EXITVALUE2(TRUE);
				return TRUE;
			}
		}
	} else {	// Backward
		for(line = block->line; line >= 0; --line) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
			if(code[line].opcode == op && code[line].label == id) {
				block->line = line;
				DBG2EXITVALUE2(TRUE);
				return TRUE;
			}
		}
	}

	DBG2EXITVALUE2(FALSE);
	return FALSE;
}


bool opc_skip_to_level(SCRIPT_CB *block,int op,int level)
{
	int line, last;
	SCRIPT_CODE *code;
	char buf[MIL];

	code = block->script->code;
	last = block->script->lines;

	if(wiznet_script) {
		sprintf(buf,"Skipping to %s with Level %d.", opcode_names[op], level);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
	}

	for(line = block->line; line < last; line++) {
//		if(wiznet_script) {
//			(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
		if(code[line].opcode == op && code[line].level == level) {
			block->line = line+1;
			DBG2EXITVALUE2(TRUE);
			return TRUE;
		}
	}

	DBG2EXITVALUE2(FALSE);
	return FALSE;
}

bool opc_skip_block(SCRIPT_CB *block,int level,bool endblock)
{
	int line, last;
	SCRIPT_CODE *code;

//	if(wiznet_script) {
//		sprintf(buf,"Skipping to %s on level %d.", endblock?"ENDIF":"ELSE/ENDIF", level);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}

	// Looking for an ELSE/ELSEIF or ENDIF
	if(block->state[level] == OP_IF) {
		code = block->script->code;
		last = block->script->lines;

		line = block->line;
		if(code[line].opcode == OP_ELSEIF) ++line;

		for(; line < last; line++) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
			if(((!endblock && (code[line].opcode == OP_ELSE || code[line].opcode == OP_ELSEIF)) ||
				code[line].opcode == OP_ENDIF) && code[line].level == level) {
				block->line = line;
				DBG2EXITVALUE2(TRUE);
				return TRUE;
			}
		}
	} else if(block->state[level] == OP_WHILE)
		return opc_skip_to_label(block,OP_ENDWHILE,block->cur_line->label,TRUE);

	DBG2EXITVALUE2(FALSE);
	return FALSE;
}

void opc_next_line(SCRIPT_CB *block)
{
	block->line++;
}

void script_loop_cleanup(SCRIPT_CB *block, int level)
{
	int i;

	for(i = 0; i < MAX_NESTED_LOOPS; i++)
	{
		if(block->loops[i].valid && block->loops[i].level >= level )
		{
			switch(block->loops[i].d.l.type) {
			case ENT_STRING:
			case ENT_EXIT:
			case ENT_MOBILE:
			case ENT_OBJECT:
			case ENT_TOKEN:
			case ENT_AFFECT:
				break;

			case ENT_PLLIST_STR:
			case ENT_BLLIST_MOB:
			case ENT_BLLIST_OBJ:
			case ENT_BLLIST_TOK:
			case ENT_BLLIST_ROOM:
			case ENT_BLLIST_EXIT:
			case ENT_BLLIST_SKILL:
			case ENT_BLLIST_AREA:
			case ENT_BLLIST_WILDS:
			case ENT_PLLIST_CONN:
			case ENT_PLLIST_MOB:
			case ENT_PLLIST_OBJ:
			case ENT_PLLIST_ROOM:
			case ENT_PLLIST_TOK:
			case ENT_PLLIST_CHURCH:
			case ENT_PLLIST_BOOK_PAGE:
			case ENT_PLLIST_FOOD_BUFF:
			case ENT_PLLIST_COMPARTMENT:
			case ENT_PLLIST_REPUTATION_RANK:
			case ENT_ILLIST_SPELLS:
			case ENT_ILLIST_VARIABLE:
			case ENT_ILLIST_REPUTATION:
			case ENT_ILLIST_REPUTATION_INDEX:
				iterator_stop(&block->loops[i].d.l.list.it);
				break;
			}

			block->loops[i].valid = FALSE;
		}
	}
}

// Function: End script execution
//
// Formats:
// break[ <expression>]
// end[ <expression>]
//
DECL_OPC_FUN(opc_end)
{
	int val;

	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Anything to evaluate?
	if(block->cur_line->rest[0]) {
		SCRIPT_PARAM *arg = new_script_param();
		if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		switch(arg->type) {
		case ENT_STRING:
			if (is_number(arg->d.str))
				val = atoi(arg->d.str);
			else if(!str_prefix(arg->d.str, "denied"))
				val = PRET_DENIED;
			else if(!str_prefix(arg->d.str, "silent"))
				val = PRET_SILENT;
			else if(!str_prefix(arg->d.str, "allowed"))
				val = PRET_ALLOWED;
			else
				val = PRET_EXECUTED;
			break;
		case ENT_NUMBER: val = arg->d.num; break;
		default: val = 0; break;
		}

		DBG3MSG1("val = %d\n", val);
		if(val >= 0) block->ret_val = val;
		free_script_param(arg);
	}

	return FALSE;
}

DECL_OPC_FUN(opc_if)
{
	int ret;

	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	if(block->cur_line->opcode == OP_ELSEIF && block->cond[block->cur_line->level])
		return opc_skip_block(block,block->cur_line->level,TRUE);

	SCRIPT_PARAM *arg = new_script_param();
	ret = boolexp_evaluate(block, (BOOLEXP *)block->cur_line->rest, arg);
	free_script_param(arg);
	if(ret < 0) return FALSE;

	block->state[block->cur_line->level] = OP_IF;
	block->cond[block->cur_line->level] = ret;
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_while)
{
	int ret;

	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	SCRIPT_PARAM *arg = new_script_param();
	ret = boolexp_evaluate(block, (BOOLEXP *)block->cur_line->rest, arg);
	free_script_param(arg);
	if(ret < 0) return FALSE;

	block->state[block->cur_line->level] = OP_WHILE;
	block->cond[block->cur_line->level] = ret;
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_else)
{
	if(block->cond[block->cur_line->level])
		return opc_skip_block(block,block->cur_line->level,TRUE);

	// Since the previous check was false, this block must be true
	block->cond[block->cur_line->level] = TRUE;
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_endif)
{
	// No need to do anything, just keep going.
	// Invalid structures are handled by the preparser
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_command)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Allow only MOBS to do this...
	if(block->type == IFC_M && block->info.mob) {
		BUFFER *buffer = new_buf();
		expand_string(&block->info,block->cur_line->rest,buffer);
		interpret(block->info.mob,buf_string(buffer));
		free_buf(buffer);
	}
	// Ignore the others

	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_gotoline)
{
	int val;

	script_loop_cleanup(block, block->cur_line->level);
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Anything to evaluate?
	if(block->cur_line->rest[0]) {
		SCRIPT_PARAM *arg = new_script_param();
		if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		switch(arg->type) {
		case ENT_STRING: val = atoi(arg->d.str)-1; break;
		case ENT_NUMBER: val = arg->d.num-1; break;
		default: val = -1; break;
		}

		free_script_param(arg);

		if(val >= 0 && val < block->script->lines) {
			block->line = val;
			block->cur_line = &block->script->code[val];
			script_loop_cleanup(block, block->cur_line->level);
			if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = TRUE;
			return TRUE;
		}
	}

	return FALSE;
}

DECL_OPC_FUN(opc_for)
{
	bool skip = FALSE;
	int lp, end, cur, inc;
	char *str1,*str2;

	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	for(lp = block->loop; lp-- > 0;)
		if(block->cur_line->label == block->loops[lp].id)
			break;

	// Initialize loop control
	if(lp < 0) {
		lp = block->loop;

		// Variable Name
		str1 = one_argument(block->cur_line->rest,block->loops[lp].var_name);

		if(!block->loops[lp].var_name[0]) {
			block->ret_val = PRET_BADSYNTAX;
			return FALSE;
		}

		SCRIPT_PARAM *arg = new_script_param();

		if(!(str2 = expand_argument(&block->info,str1,arg))) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		switch(arg->type) {
		case ENT_STRING: cur = atoi(arg->d.str); break;
		case ENT_NUMBER: cur = arg->d.num; break;
		default:
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		if(!(str1 = expand_argument(&block->info,str2,arg))) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		switch(arg->type) {
		case ENT_STRING: end = atoi(arg->d.str); break;
		case ENT_NUMBER: end = arg->d.num; break;
		default:
			block->ret_val = PRET_BADSYNTAX;
			return FALSE;
		}


		if(!(str2 = expand_argument(&block->info,str1,arg))) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		switch(arg->type) {
		case ENT_STRING: inc = atoi(arg->d.str); break;
		case ENT_NUMBER: inc = arg->d.num; break;
		default:
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		free_script_param(arg);

		// No increment?  No looping!
		if(!inc) return opc_skip_to_label(block,OP_ENDFOR,block->cur_line->label,TRUE);

		block->loops[lp].id = block->cur_line->label;
		block->loops[lp].d.f.inc = inc;

		// set the directions correctly
		if((inc > 0) == (cur < end)) {
			block->loops[lp].d.f.cur = cur;
			block->loops[lp].d.f.end = end;
		} else {
			block->loops[lp].d.f.cur = end;
			block->loops[lp].d.f.end = cur;
		}

		// Set the variable
		variables_set_integer(block->info.var,block->loops[lp].var_name,block->loops[lp].d.f.cur);
		block->loop++;
		block->cond[block->cur_line->level] = TRUE;
	} else {
		// Continue loop
		block->loops[lp].d.f.cur += block->loops[lp].d.f.inc;

		// Set the variable
		variables_set_integer(block->info.var,block->loops[lp].var_name,block->loops[lp].d.f.cur);

		if(block->loops[lp].d.f.inc < 0 && (block->loops[lp].d.f.cur < block->loops[lp].d.f.end))
			skip = TRUE;
		else if(block->loops[lp].d.f.inc > 0 && (block->loops[lp].d.f.cur > block->loops[lp].d.f.end))
			skip = TRUE;

		if(skip) {
			block->loop--;
			return opc_skip_to_label(block,OP_ENDFOR,block->loops[lp].id,TRUE);
		}
		block->cond[block->cur_line->level] = TRUE;
	}

	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_endfor)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_FOR,block->cur_line->label,FALSE);
}

DECL_OPC_FUN(opc_exitfor)
{
	script_loop_cleanup(block, block->cur_line->level);
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_ENDFOR,block->cur_line->label,TRUE);
}


DECL_OPC_FUN(opc_list)
{
	bool skip = FALSE;
	int lp, i;
	char *str1,*str2, *str;
	char buf[MSL];
	LLIST *list;
	LLIST_UID_DATA *uid;
	LLIST_ROOM_DATA *lrd;
	LLIST_EXIT_DATA *led;
	LLIST_SKILL_DATA *lsk;
	LLIST_AREA_DATA *lar;
	LLIST_AREA_REGION_DATA *lareg;
	LLIST_WILDS_DATA *lwd;
	DESCRIPTOR_DATA *conn;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	TOKEN_DATA *tok;
	ROOM_INDEX_DATA *here;
	EXIT_DATA *ex;
	CHURCH_DATA *church;
	VARIABLE *variable;
	EXTRA_DESCR_DATA *ed;
	INSTANCE_SECTION *section;
	INSTANCE *instance;
	NAMED_SPECIAL_ROOM *special_room;
	SHIP_DATA *ship;
	AREA_DATA *area;
	AREA_REGION *aregion;
	BOOK_PAGE *book_page;
	FOOD_BUFF_DATA *food_buff;
	FURNITURE_COMPARTMENT *compartment;
	SPELL_DATA *spell;
	REPUTATION_DATA *reputation;
	REPUTATION_INDEX_DATA *repIndex;
	REPUTATION_INDEX_RANK_DATA *rank;

	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	for(lp = block->loop; lp-- > 0;)
		if(block->cur_line->label == block->loops[lp].id)
			break;

	// Initialize loop control
	if(lp < 0) {
		lp = block->loop;

		// Variable Name
		str1 = one_argument(block->cur_line->rest,block->loops[lp].var_name);

		log_stringf("opc_list: initializing for loop variable '%s'", block->loops[lp].var_name);

		if(!block->loops[lp].var_name[0]) {
			block->ret_val = PRET_BADSYNTAX;
			return FALSE;
		}

		SCRIPT_PARAM *arg = new_script_param();

		// Get the LIST
		if(!(str2 = expand_argument(&block->info,str1,arg))) {
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		block->loops[lp].counter = 1;

		switch(arg->type) {
		case ENT_STRING:
			//log_stringf("opc_list: list type ENT_STRING");
			if(IS_NULLSTR(arg->d.str))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			strncpy(block->loops[lp].buf, arg->d.str, MSL-1);
			str = one_argument_norm(block->loops[lp].buf, buf);

			block->loops[lp].d.l.type = ENT_STRING;
			block->loops[lp].d.l.cur.str = NULL;
			block->loops[lp].d.l.next.str = str;
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;


			// Set the variable
			variables_set_string(block->info.var,block->loops[lp].var_name,buf,FALSE);
			break;
		case ENT_EXIT:
			//log_stringf("opc_list: list type ENT_EXIT");
			if(!arg->d.door.r)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			here = arg->d.door.r;
			ex = here->exit[arg->d.door.door];
			block->loops[lp].d.l.type = ENT_EXIT;
			block->loops[lp].d.l.cur.door = arg->d.door.door;
			block->loops[lp].d.l.next.door = MAX_DIR;
			block->loops[lp].d.l.owner = arg->d.door.r;
			block->loops[lp].d.l.owner_type = ENT_EXIT;

			/*
			if(ex) {
				if(here->wilds)
					log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[arg.d.door.door], here->wilds->uid, here->x, here->y);
				else if(here->source)
					log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[arg.d.door.door], here->vnum, here->name, here->id[0], here->id[1]);
				else
					log_stringf("opc_list: %s(%ld,%s)", dir_name[arg.d.door.door], here->vnum, here->name);
			} else
				log_stringf("opc_list: exit(<END>)");
			*/


			if(ex) {
				here = arg->d.door.r;
				for(i=arg->d.door.door + 1; i < MAX_DIR && !here->exit[i]; i++);
				block->loops[lp].d.l.next.door = i;
			}
			// Set the variable
			variables_set_exit(block->info.var,block->loops[lp].var_name,ex);
			break;

		case ENT_OLLIST_MOB:
			//log_stringf("opc_list: list type ENT_MOBILE");
			if(!arg->d.list.ptr.mob || !*arg->d.list.ptr.mob)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_MOBILE;
			block->loops[lp].d.l.cur.m = *arg->d.list.ptr.mob;
			block->loops[lp].d.l.next.m = block->loops[lp].d.l.cur.m->next_in_room;
			block->loops[lp].d.l.owner = arg->d.list.owner;
			block->loops[lp].d.l.owner_type = ENT_MOBILE;

			/*
			if(block->loops[lp].d.l.cur.m) {
				ch = block->loops[lp].d.l.cur.m;
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
			*/

			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.mob);
			break;

		case ENT_OLLIST_OBJ:
			//log_stringf("opc_list: list type ENT_OBJECT");
			if(!arg->d.list.ptr.obj || !*arg->d.list.ptr.obj)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_OBJECT;
			block->loops[lp].d.l.cur.o = *arg->d.list.ptr.obj;
			block->loops[lp].d.l.next.o = block->loops[lp].d.l.cur.o->next_content;
			block->loops[lp].d.l.owner = arg->d.list.owner;
			block->loops[lp].d.l.owner_type = ENT_OBJECT;

			/*
			if(block->loops[lp].d.l.cur.o)
				log_stringf("opc_list: object(%ld,%ld,%ld)", block->loops[lp].d.l.cur.o->pIndexData->vnum, block->loops[lp].d.l.cur.o->id[0], block->loops[lp].d.l.cur.o->id[1]);
			else
				log_stringf("opc_list: object(<END>)");
				*/

			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.obj);
			break;

		case ENT_OLLIST_TOK:
			//log_stringf("opc_list: list type ENT_TOKEN");
			if(!arg->d.list.ptr.tok || !*arg->d.list.ptr.tok)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_TOKEN;
			block->loops[lp].d.l.cur.t = *arg->d.list.ptr.tok;
			block->loops[lp].d.l.next.t = block->loops[lp].d.l.cur.t->next;
			block->loops[lp].d.l.owner = arg->d.list.owner;
			block->loops[lp].d.l.owner_type = ENT_TOKEN;

			/*
			if(block->loops[lp].d.l.cur.t)
				log_stringf("opc_list: token(%ld,%ld,%ld)", block->loops[lp].d.l.cur.t->pIndexData->vnum, block->loops[lp].d.l.cur.t->id[0], block->loops[lp].d.l.cur.t->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			// Set the variable
			variables_set_token(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.tok);
			break;

		case ENT_OLLIST_AFF:
			//log_stringf("opc_list: list type ENT_AFFECT");
			if(!arg->d.list.ptr.aff || !*arg->d.list.ptr.aff)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_AFFECT;
			block->loops[lp].d.l.cur.aff = *arg->d.list.ptr.aff;
			block->loops[lp].d.l.next.aff = block->loops[lp].d.l.cur.aff->next;
			block->loops[lp].d.l.owner = arg->d.list.owner;
			block->loops[lp].d.l.owner_type = ENT_AFFECT;

			/*
			if(block->loops[lp].d.l.cur.aff) {
				if(block->loops[lp].d.l.cur.aff->custom_name)
					log_stringf("opc_list: affect(%s)", block->loops[lp].d.l.cur.aff->custom_name);
				else
					log_stringf("opc_list: affect(%s)", skill_table[block->loops[lp].d.l.cur.aff->type].name);
			} else
				log_stringf("opc_list: affect(<END>)");
				*/

			// Set the variable
			variables_set_affect(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.aff);
			break;

		case ENT_EXTRADESC:
			if(!arg->d.list.ptr.ed || !*arg->d.list.ptr.ed)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_EXTRADESC;
			block->loops[lp].d.l.cur.ed = *arg->d.list.ptr.ed;
			block->loops[lp].d.l.next.ed = block->loops[lp].d.l.cur.ed->next;
			block->loops[lp].d.l.owner = arg->d.list.owner;
			block->loops[lp].d.l.owner_type = arg->d.list.owner_type;

			// Set the variable
			variables_set_string(block->info.var,block->loops[lp].var_name,(*arg->d.list.ptr.ed)->keyword, FALSE);
			break;

		case ENT_PLLIST_STR:
			//log_stringf("opc_list: list type ENT_PLLIST_STR");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_STR;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(str)
				log_stringf("opc_list: string(%s)",str);
			else
				log_stringf("opc_list: string(<END>)");
				*/

			if( !str ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_string(block->info.var,block->loops[lp].var_name,str,FALSE);
			break;

		case ENT_BLLIST_MOB:
			//log_stringf("opc_list: list type ENT_BLLIST_MOB");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_MOB;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			if( !uid ) {
				//log_stringf("opc_list: mobile(<END>)");
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			if( IS_VALID((CHAR_DATA *)uid->ptr) )
				variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)uid->ptr);
			else
				variables_set_mobile_id(block->info.var,block->loops[lp].var_name, uid->id[0], uid->id[1],false);

			/*
			ch = (CHAR_DATA *)(uid->ptr);
			if(!IS_NPC(ch))
				log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
			else
				log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
				*/
			break;

		case ENT_BLLIST_OBJ:
			//log_stringf("opc_list: list type ENT_BLLIST_OBJ");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_OBJ;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			if( !uid ) {
				//log_stringf("opc_list: object(<END>)");
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			if( IS_VALID((OBJ_DATA *)uid->ptr) )
				variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)uid->ptr);
			else
				variables_set_object_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);

			//obj = (OBJ_DATA *)(uid->ptr);
			//log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);

			break;

		case ENT_BLLIST_TOK:
			//log_stringf("opc_list: list type ENT_BLLIST_TOK");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_TOK;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			if( !uid ) {
				//log_stringf("opc_list: token(<END>)");
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			if( IS_VALID((TOKEN_DATA *)uid->ptr) )
				variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)uid->ptr);
			else
				variables_set_token_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);

			//tok = (TOKEN_DATA *)(uid->ptr);
			//log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			break;

		case ENT_BLLIST_ROOM:
			//log_stringf("opc_list: list type ENT_BLLIST_ROOM");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_ROOM;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				lrd = (LLIST_ROOM_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				//log_stringf("opc_list: lrd = %016lX", lrd);
				if( !lrd ) {
					//log_stringf("opc_list: room(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( lrd->room )
					variables_set_room(block->info.var,block->loops[lp].var_name,lrd->room);

			} while( !lrd->room );

			/*
			log_stringf("opc_list: lrd->room = %016lX", lrd->room);
			if(lrd->room->wilds)
				log_stringf("opc_list: room(%ld,%ld,%ld)", lrd->room->wilds->uid, lrd->room->x, lrd->room->y);
			else if(lrd->room->source)
				log_stringf("opc_list: room(%ld,%s,%ld,%ld)", lrd->room->vnum, lrd->room->name, lrd->room->id[0], lrd->room->id[1]);
			else
				log_stringf("opc_list: room(%ld,%s)", lrd->room->vnum, lrd->room->name);
				*/

			break;

		case ENT_BLLIST_EXIT:
			//log_stringf("opc_list: list type ENT_BLLIST_EXIT");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_EXIT;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				led = (LLIST_EXIT_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				if( !led ) {
					//log_stringf("opc_list: exit(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( led->room && led->door >= 0 && led->door < MAX_DIR && led->room->exit[led->door])
					variables_set_door(block->info.var,block->loops[lp].var_name,led->room, led->door, FALSE);

			} while( !led->room || led->door < 0 || led->door >= MAX_DIR || !led->room->exit[led->door] );

			/*
			if(led->room->wilds)
				log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[led->door], led->room->wilds->uid, led->room->x, led->room->y);
			else if(led->room->source)
				log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[led->door], led->room->vnum, led->room->name, led->room->id[0], led->room->id[1]);
			else
				log_stringf("opc_list: %s(%ld,%s)", dir_name[led->door], led->room->vnum, led->room->name);
				*/

			break;

		case ENT_BLLIST_SKILL:
			//log_stringf("opc_list: list type ENT_BLLIST_SKILL");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_SKILL;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				lsk = (LLIST_SKILL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				if( !lsk ) {
					//log_stringf("opc_list: skill(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( IS_VALID(lsk->mob) && IS_VALID(lsk->skill) )
					variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, lsk->mob, lsk->skill, lsk->tok, FALSE);

			} while( !IS_VALID(lsk->mob) || !IS_VALID(lsk->skill) );

			/*
			if(lsk->tok)
				log_stringf("opc_list: skill(%ld,%ld,TOKEN,%s)", lsk->mob->id[0], lsk->mob->id[1], lsk->tok->name);
			else
				log_stringf("opc_list: skill(%ld,%ld,SKILL,%s)", lsk->mob->id[0], lsk->mob->id[1], skill_table[lsk->sn].name);
				*/

			break;

		case ENT_BLLIST_AREA:
			//log_stringf("opc_list: list type ENT_BLLIST_AREA");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_AREA;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				lar = (LLIST_AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				if( !lar ) {
					//log_stringf("opc_list: area(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( lar->area )
					variables_setsave_area(block->info.var,block->loops[lp].var_name, lar->area, FALSE);

			} while( !lar->area );

			//log_stringf("opc_list: area(%ld,%s)", lar->area->uid, lar->area->name);

			break;

		case ENT_BLLIST_AREA_REGION:
			//log_stringf("opc_list: list type ENT_BLLIST_AREA_REGION");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_AREA_REGION;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				lareg = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				if( !lareg ) {
					//log_stringf("opc_list: area(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( lareg->aregion )
					variables_setsave_area_region(block->info.var,block->loops[lp].var_name, lareg->aregion, FALSE);

			} while( !lareg->aregion );

			//log_stringf("opc_list: area region(%ld,%s)", lareg->area->uid, lar->area->name);

			break;

		case ENT_BLLIST_WILDS:
			//log_stringf("opc_list: list type ENT_BLLIST_WILDS");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_BLLIST_WILDS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			do {
				lwd = (LLIST_WILDS_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
				if( !lwd ) {
					//log_stringf("opc_list: wilds(<END>)");
					iterator_stop(&block->loops[lp].d.l.list.it);
					free_script_param(arg);
					return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
				}

				// Set the variable
				if( lwd->wilds )
					variables_setsave_wilds(block->info.var,block->loops[lp].var_name, lwd->wilds, FALSE);

			} while( !lwd->wilds );

			//log_stringf("opc_list: wilds(%ld,%s)", lwd->wilds->uid, lwd->wilds->name);

			break;

		case ENT_PLLIST_CONN:
			//log_stringf("opc_list: list type ENT_PLLIST_CONN");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_CONN;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			conn = (DESCRIPTOR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(conn) {
				if(conn->original)
					log_stringf("opc_list: connection(%s,%d) [SWITCHED]", conn->original->name, conn->original->tot_level);
				else
					log_stringf("opc_list: connection(%s,%d)", conn->character->name, conn->character->tot_level);
			} else
				log_stringf("opc_list: connection(<END>)");*/

			if( !conn ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_connection(block->info.var,block->loops[lp].var_name,conn);
			break;

		case ENT_PLLIST_MOB:
			//log_stringf("opc_list: list type ENT_PLLIST_MOB");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_MOB;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(ch) {
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			if( !ch ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);
			break;

		case ENT_PLLIST_OBJ:
			//log_stringf("opc_list: list type ENT_PLLIST_OBJ");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_OBJ;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(obj)
				log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
			else
				log_stringf("opc_list: object(<END>)");
				*/

			if( !obj ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,obj);
			break;

		case ENT_PLLIST_ROOM:
			//log_stringf("opc_list: list type ENT_PLLIST_ROOM");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_ROOM;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			here = (ROOM_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(here) {
				if(here->wilds)
					log_stringf("opc_list: room(%ld,%ld,%ld)", here->wilds->uid, here->x, here->y);
				else if(here->source)
					log_stringf("opc_list: room(%ld,%s,%ld,%ld)", here->vnum, here->name, here->id[0], here->id[1]);
				else
					log_stringf("opc_list: room(%ld,%s)", here->vnum, here->name);
			} else
				log_stringf("opc_list: room(<END>)");
				*/

			if( !here ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_room(block->info.var,block->loops[lp].var_name,here);
			break;

		case ENT_PLLIST_TOK:
			//log_stringf("opc_list: list type ENT_PLLIST_TOK");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_TOK;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			tok = (TOKEN_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(tok)
				log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			if( !tok ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_token(block->info.var,block->loops[lp].var_name,tok);
			break;


		case ENT_PLLIST_AREA:
			//log_stringf("opc_list: list type ENT_PLLIST_AREA");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_AREA;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			area = (AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(tok)
				log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			if( !area ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_area(block->info.var,block->loops[lp].var_name,area);
			break;


		case ENT_PLLIST_AREA_REGION:
			//log_stringf("opc_list: list type ENT_PLLIST_AREA_REGION");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_AREA_REGION;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			aregion = (AREA_REGION *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(tok)
				log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			if( !aregion ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_area_region(block->info.var,block->loops[lp].var_name,aregion);
			break;

		case ENT_PLLIST_CHURCH:
			//log_stringf("opc_list: list type ENT_PLLIST_CHURCH");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_CHURCH;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			church = (CHURCH_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			if( !church ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_church(block->info.var,block->loops[lp].var_name,church);
			break;


		case ENT_PLLIST_BOOK_PAGE:
			//log_stringf("opc_list: list type ENT_PLLIST_BOOK_PAGE");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_BOOK_PAGE;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			book_page = (BOOK_PAGE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			if( !book_page ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_book_page(block->info.var,block->loops[lp].var_name,book_page);
			break;

		case ENT_PLLIST_FOOD_BUFF:
			//log_stringf("opc_list: list type ENT_PLLIST_FOOD_BUFF");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_FOOD_BUFF;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			food_buff = (FOOD_BUFF_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			if( !food_buff ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_food_buff(block->info.var,block->loops[lp].var_name,food_buff);
			break;

		case ENT_PLLIST_COMPARTMENT:
			//log_stringf("opc_list: list type ENT_PLLIST_COMPARTMENT");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_COMPARTMENT;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			if( !compartment ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_compartment(block->info.var,block->loops[lp].var_name,compartment);
			break;

		case ENT_PLLIST_REPUTATION_RANK:
			//log_stringf("opc_list: list type ENT_PLLIST_REPUTATION_RANK");
			if(!arg->d.blist || !arg->d.blist->valid)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_PLLIST_REPUTATION_RANK;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			if( !IS_VALID(rank) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_reputation_rank(block->info.var,block->loops[lp].var_name,rank);
			break;

		case ENT_ILLIST_MOB_GROUP:
			//log_stringf("opc_list: list type ENT_ILLIST_MOB_GROUP");
			if(!arg->d.group_owner || !arg->d.group_owner->in_room)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			ch = arg->d.group_owner->leader ? arg->d.group_owner->leader : arg->d.group_owner;

			list = list_copy(ch->lgroup);
			if( !list )
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			if( !list_addlink(list, ch) ) {
				list_destroy(list);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_MOB_GROUP;
			block->loops[lp].d.l.list.lp = list;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(ch) {
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			if( !ch ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);

		case ENT_ILLIST_VARIABLE:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			if(!arg->d.variables || !*arg->d.variables)
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_VARIABLE;
			block->loops[lp].d.l.list.lp = variable_copy_tolist(arg->d.variables);
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			variable = (VARIABLE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			if( !variable ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_variable(block->info.var,block->loops[lp].var_name,variable);
			break;

		case ENT_ILLIST_SECTIONS:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_SECTIONS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			section = (INSTANCE_SECTION *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !section ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_instance_section(block->info.var,block->loops[lp].var_name,section);
			break;

		case ENT_ILLIST_INSTANCES:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_INSTANCES;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			instance = (INSTANCE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !instance ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_instance(block->info.var,block->loops[lp].var_name,instance);
			break;

		case ENT_ILLIST_SPECIALROOMS:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_SPECIALROOMS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			special_room = (NAMED_SPECIAL_ROOM *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !special_room ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_room(block->info.var,block->loops[lp].var_name,special_room->room);
			break;

		case ENT_ILLIST_SHIPS:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_SHIPS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			ship = (SHIP_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !ship ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_ship(block->info.var,block->loops[lp].var_name,ship);
			break;


		case ENT_ILLIST_SPELLS:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_SPELLS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			spell = (SPELL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !spell ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_spell(block->info.var,block->loops[lp].var_name,spell);
			break;

		case ENT_ILLIST_SPECIALKEYS:
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_SPECIALKEYS;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !obj ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,obj);
			break;

		case ENT_ILLIST_REPUTATION:
			//log_stringf("opc_list: list type ENT_ILLIST_REPUTATION");
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_REPUTATION;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			reputation = (REPUTATION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !IS_VALID(reputation) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_reputation(block->info.var,block->loops[lp].var_name,reputation);
			break;


		case ENT_ILLIST_REPUTATION_INDEX:
			//log_stringf("opc_list: list type ENT_ILLIST_REPUTATION_INDEX");
			if(!IS_VALID(arg->d.blist))
			{
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			block->loops[lp].d.l.type = ENT_ILLIST_REPUTATION_INDEX;
			block->loops[lp].d.l.list.lp = arg->d.blist;
			iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
			block->loops[lp].d.l.owner = NULL;
			block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

			repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			if( !IS_VALID(repIndex) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				free_script_param(arg);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			// Set the variable
			variables_set_reputation_index(block->info.var,block->loops[lp].var_name,repIndex);
			break;

		default:
			//log_stringf("opc_list: list_type INVALID");
			block->ret_val = PRET_BADSYNTAX;
			free_script_param(arg);
			return FALSE;
		}

		block->loops[lp].id = block->cur_line->label;
		block->loops[lp].valid = TRUE;
		block->loops[lp].level = block->cur_line->level;
		block->loop++;
		block->cond[block->cur_line->level] = TRUE;
		free_script_param(arg);
	} else {
		//log_stringf("opc_list: next loop variable '%s'", block->loops[lp].var_name);
		block->loops[lp].counter++;

		// Continue loop
		switch(block->loops[lp].d.l.type) {
		case ENT_STRING:
			//log_stringf("opc_list: list type ENT_STRING");
			str = block->loops[lp].d.l.next.str;

			if( IS_NULLSTR(str) )
			{
				skip = TRUE;
				break;
			}

			str = one_argument_norm(str,buf);

			variables_set_string(block->info.var,block->loops[lp].var_name,buf,FALSE);

			block->loops[lp].d.l.next.str = str;
			break;


		case ENT_EXIT:
			//log_stringf("opc_list: list type ENT_EXIT");
			i = block->loops[lp].d.l.cur.door = block->loops[lp].d.l.next.door;

			if( i >= MAX_DIR ) {
				skip = TRUE;
				break;
			}

			here = (ROOM_INDEX_DATA *)block->loops[lp].d.l.owner;
			ex = here->exit[i];

			/*
			if(ex) {
				if(here->wilds)
					log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[i], here->wilds->uid, here->x, here->y);
				else if(here->source)
					log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[i], here->vnum, here->name, here->id[0], here->id[1]);
				else
					log_stringf("opc_list: %s(%ld,%s)", dir_name[i], here->vnum, here->name);
			} else
				log_stringf("opc_list: exit(<END>)");
				*/


			// Set the variable
			variables_set_exit(block->info.var,block->loops[lp].var_name,ex);

			if(!ex) {
				skip = TRUE;
				break;
			}

			for(i++; i < MAX_DIR && !here->exit[i]; i++);

			block->loops[lp].d.l.next.door = i;
			break;

		case ENT_MOBILE:
			//log_stringf("opc_list: list type ENT_MOBILE");
			block->loops[lp].d.l.cur.m = block->loops[lp].d.l.next.m;
			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.m);

			/*
			if(block->loops[lp].d.l.cur.m) {
				ch = block->loops[lp].d.l.cur.m;
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			if(!block->loops[lp].d.l.cur.m) {
				skip = TRUE;
				break;
			}

			block->loops[lp].d.l.next.m = block->loops[lp].d.l.cur.m->next_in_room;
			break;

		case ENT_OBJECT:
			//log_stringf("opc_list: list type ENT_OBJECT");
			block->loops[lp].d.l.cur.o = block->loops[lp].d.l.next.o;
			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.o);

			/*
			if(block->loops[lp].d.l.cur.o)
				log_stringf("opc_list: object(%ld,%ld,%ld)", block->loops[lp].d.l.cur.o->pIndexData->vnum, block->loops[lp].d.l.cur.o->id[0], block->loops[lp].d.l.cur.o->id[1]);
			else
				log_stringf("opc_list: object(<END>)");
				*/

			if(!block->loops[lp].d.l.cur.o) {
				skip = TRUE;
				break;
			}

			block->loops[lp].d.l.next.o = block->loops[lp].d.l.cur.o->next_content;
			break;

		case ENT_TOKEN:
			//log_stringf("opc_list: list type ENT_TOKEN");
			block->loops[lp].d.l.cur.t = block->loops[lp].d.l.next.t;
			// Set the variable
			variables_set_token(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.t);

			/*
			if(block->loops[lp].d.l.cur.t)
				log_stringf("opc_list: token(%ld,%ld,%ld)", block->loops[lp].d.l.cur.t->pIndexData->vnum, block->loops[lp].d.l.cur.t->id[0], block->loops[lp].d.l.cur.t->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			if(!block->loops[lp].d.l.cur.t) {
				skip = TRUE;
				break;
			}

			block->loops[lp].d.l.next.t = block->loops[lp].d.l.cur.t->next;
			break;

		case ENT_AFFECT:
			//log_stringf("opc_list: list type ENT_AFFECT");
			block->loops[lp].d.l.cur.aff = block->loops[lp].d.l.next.aff;
			// Set the variable
			variables_set_affect(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.aff);

			/*
			if(block->loops[lp].d.l.cur.aff) {
				if(block->loops[lp].d.l.cur.aff->custom_name)
					log_stringf("opc_list: affect(%s)", block->loops[lp].d.l.cur.aff->custom_name);
				else
					log_stringf("opc_list: affect(%s)", skill_table[block->loops[lp].d.l.cur.aff->type].name);
			} else
				log_stringf("opc_list: affect(<END>)");
				*/

			if(!block->loops[lp].d.l.cur.aff) {
				skip = TRUE;
				break;
			}

			block->loops[lp].d.l.next.aff = block->loops[lp].d.l.cur.aff->next;
			break;

		case ENT_EXTRADESC:
			block->loops[lp].d.l.cur.ed = block->loops[lp].d.l.next.ed;
			ed = block->loops[lp].d.l.cur.ed;
			// Set the variable
			variables_set_string(block->info.var,block->loops[lp].var_name, ed ? ed->keyword : &str_empty[0], FALSE);
			if(!ed) {
				skip = TRUE;
				break;
			}

			block->loops[lp].d.l.next.ed = ed->next;
			break;

		case ENT_PLLIST_STR:
			//log_stringf("opc_list: list type ENT_PLLIST_STR");
			str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			/*
			if(str)
				log_stringf("opc_list: string(%s)",str);
			else
				log_stringf("opc_list: string(<END>)");
				*/

			// Set the variable
			variables_set_string(block->info.var,block->loops[lp].var_name,str?str:&str_empty[0],FALSE);

			if( !str ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
			}

			break;

		case ENT_BLLIST_MOB:
			//log_stringf("opc_list: list type ENT_BLLIST_MOB");
			while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((CHAR_DATA *)uid->ptr) );

			/*
			if(uid) {
				ch = (CHAR_DATA *)(uid->ptr);
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			if( uid )
			{
				if( IS_VALID((CHAR_DATA *)uid->ptr) )
					variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)uid->ptr);
				else
					variables_set_mobile_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
			}
			else
				variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)NULL);

			if( !uid ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_BLLIST_OBJ:
			//log_stringf("opc_list: list type ENT_BLLIST_OBJ");
			while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((OBJ_DATA *)uid->ptr) );
			/*
			if(uid) {
				obj = (OBJ_DATA *)(uid->ptr);
				log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
			} else
				log_stringf("opc_list: object(<END>)");
				*/

			if( uid )
			{
				if( IS_VALID((OBJ_DATA *)uid->ptr) )
					variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)uid->ptr);
				else
					variables_set_object_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
			}
			else
				variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)NULL);

			if( !uid ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_BLLIST_TOK:
			//log_stringf("opc_list: list type ENT_BLLIST_TOK");
			while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((TOKEN_DATA *)uid->ptr) );

			/*
			if(uid) {
				tok = (TOKEN_DATA *)(uid->ptr);
				log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			} else
				log_stringf("opc_list: token(<END>)");
				*/

			if( uid )
			{
				if( IS_VALID((TOKEN_DATA *)uid->ptr) )
					variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)uid->ptr);
				else
					variables_set_token_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
			}
			else
				variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)NULL);

			if( !uid ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_BLLIST_ROOM:
			//log_stringf("opc_list: list type ENT_BLLIST_ROOM");
			while( (lrd = (LLIST_ROOM_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lrd->room );
			/*
			if(lrd) {
				if(lrd->room->wilds)
					log_stringf("opc_list: room(%ld,%ld,%ld)", lrd->room->wilds->uid, lrd->room->x, lrd->room->y);
				else if(lrd->room->source)
					log_stringf("opc_list: room(%ld,%s,%ld,%ld)", lrd->room->vnum, lrd->room->name, lrd->room->id[0], lrd->room->id[1]);
				else
					log_stringf("opc_list: room(%ld,%s)", lrd->room->vnum, lrd->room->name);
			} else
				log_stringf("opc_list: room(<END>)");
				*/

			variables_set_room(block->info.var,block->loops[lp].var_name,lrd?lrd->room:NULL);

			if( !lrd ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;

		case ENT_BLLIST_EXIT:
			//log_stringf("opc_list: list type ENT_BLLIST_EXIT");
			while( (led = (LLIST_EXIT_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) &&
				(!led->room || led->door < 0 || led->door >= MAX_DIR || !led->room->exit[led->door]));
			/*
			if(led) {
				if(led->room->wilds)
					log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[led->door], led->room->wilds->uid, led->room->x, led->room->y);
				else if(led->room->source)
					log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[led->door], led->room->vnum, led->room->name, led->room->id[0], led->room->id[1]);
				else
					log_stringf("opc_list: %s(%ld,%s)", dir_name[led->door], led->room->vnum, led->room->name);
			} else
				log_stringf("opc_list: exit(<END>)");
				*/

			if( !led ) {
				variables_set_door(block->info.var,block->loops[lp].var_name,NULL, DIR_NORTH, FALSE);
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			variables_set_door(block->info.var,block->loops[lp].var_name,led->room, led->door, FALSE);
			break;

		case ENT_BLLIST_SKILL:
			//log_stringf("opc_list: list type ENT_BLLIST_SKILL");
			while( (lsk = (LLIST_SKILL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) &&
				(!IS_VALID(lsk->mob) || !IS_VALID(lsk->skill)) );
			/*
			if(lsk) {
				if(lsk->tok)
					log_stringf("opc_list: skill(%ld,%ld,TOKEN,%s)", lsk->mob->id[0], lsk->mob->id[1], lsk->tok->name);
				else
					log_stringf("opc_list: skill(%ld,%ld,SKILL,%s)", lsk->mob->id[0], lsk->mob->id[1], skill_table[lsk->sn].name);
			} else
				log_stringf("opc_list: skill(<END>)");
				*/

			if( !lsk ) {
				variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, NULL, NULL, NULL, FALSE);
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, lsk->mob, lsk->skill, lsk->tok, FALSE);
			break;

		case ENT_BLLIST_AREA:
			//log_stringf("opc_list: list type ENT_BLLIST_AREA");
			while( (lar = (LLIST_AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lar->area );
			/*
			if(lar) {
				log_stringf("opc_list: area(%ld,%s)", lar->area->uid, lar->area->name);
			} else
				log_stringf("opc_list: area(<END>)");
				*/

			variables_set_area(block->info.var,block->loops[lp].var_name,lar?lar->area:NULL);

			if( !lar ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;

		case ENT_BLLIST_WILDS:
			//log_stringf("opc_list: list type ENT_BLLIST_WILDS");
			while( (lwd = (LLIST_WILDS_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lwd->wilds );
			/*
			if(lwd) {
				log_stringf("opc_list: wilds(%ld,%s)", lwd->wilds->uid, lwd->wilds->name);
			} else
				log_stringf("opc_list: wilds(<END>)");
				*/

			variables_set_wilds(block->info.var,block->loops[lp].var_name,lwd?lwd->wilds:NULL);

			if( !lwd ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;

		case ENT_PLLIST_CONN:
			//log_stringf("opc_list: list type ENT_PLLIST_CONN");
			conn = (DESCRIPTOR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(conn) {
				if(conn->original)
					log_stringf("opc_list: connection(%s,%d) [SWITCHED]", conn->original->name, conn->original->tot_level);
				else
					log_stringf("opc_list: connection(%s,%d)", conn->character->name, conn->character->tot_level);
			} else
				log_stringf("opc_list: connection(<END>)");
				*/

			// Set the variable
			variables_set_connection(block->info.var,block->loops[lp].var_name,conn);

			if( !conn ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_MOB:
			//log_stringf("opc_list: list type ENT_PLLIST_MOB");
			ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(ch) {
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);

			if( !ch ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_OBJ:
			//log_stringf("opc_list: list type ENT_PLLIST_OBJ");
			obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(obj)
				log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
			else
				log_stringf("opc_list: object(<END>)");
				*/

			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,obj);

			if( !obj ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_ROOM:
			//log_stringf("opc_list: list type ENT_PLLIST_ROOM");
			here = (ROOM_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(here) {
				if(here->wilds)
					log_stringf("opc_list: room(%ld,%ld,%ld)", here->wilds->uid, here->x, here->y);
				else if(here->source)
					log_stringf("opc_list: room(%ld,%s,%ld,%ld)", here->vnum, here->name, here->id[0], here->id[1]);
				else
					log_stringf("opc_list: room(%ld,%s)", here->vnum, here->name);
			} else
				log_stringf("opc_list: room(<END>)");
				*/

			// Set the variable
			variables_set_room(block->info.var,block->loops[lp].var_name,here);

			if( !here ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_TOK:
			//log_stringf("opc_list: list type ENT_PLLIST_TOK");
			tok = (TOKEN_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(tok)
				log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
			else
				log_stringf("opc_list: token(<END>)");
				*/

			// Set the variable
			variables_set_token(block->info.var,block->loops[lp].var_name,tok);

			if( !tok ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;

		case ENT_PLLIST_CHURCH:
			//log_stringf("opc_list: list type ENT_PLLIST_CHURCH");
			church = (CHURCH_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

			// Set the variable
			variables_set_church(block->info.var,block->loops[lp].var_name,church);

			if( !church ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_BOOK_PAGE:
			//log_stringf("opc_list: list type ENT_PLLIST_BOOK_PAGE");
			book_page = (BOOK_PAGE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			// Set the variable
			variables_set_book_page(block->info.var,block->loops[lp].var_name,book_page);

			if( !book_page ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_FOOD_BUFF:
			//log_stringf("opc_list: list type ENT_PLLIST_FOOD_BUFF");
			food_buff = (FOOD_BUFF_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			// Set the variable
			variables_set_food_buff(block->info.var,block->loops[lp].var_name,food_buff);

			if( !food_buff ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_COMPARTMENT:
			//log_stringf("opc_list: list type ENT_PLLIST_COMPARTMENT");
			compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			// Set the variable
			variables_set_compartment(block->info.var,block->loops[lp].var_name,compartment);

			if( !compartment ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_PLLIST_REPUTATION_RANK:
			//log_stringf("opc_list: list type ENT_PLLIST_REPUTATION_RANK");
			rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

			// Set the variable
			variables_set_reputation_rank(block->info.var,block->loops[lp].var_name,rank);

			if( !IS_VALID(rank) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;
		case ENT_ILLIST_VARIABLE:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			variable = (VARIABLE *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_variable(block->info.var,block->loops[lp].var_name,variable);

			if( !variable) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_MOB_GROUP:
			//log_stringf("opc_list: list type ENT_ILLIST_MOB_GROUP");
			ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			/*
			if(ch) {
				if(!IS_NPC(ch))
					log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
				else
					log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
			} else
				log_stringf("opc_list: mobile(<END>)");
				*/

			// Set the variable
			variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);

			if( !ch ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_SECTIONS:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			section = (INSTANCE_SECTION *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_instance_section(block->info.var,block->loops[lp].var_name,section);

			if( !section ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_INSTANCES:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			instance = (INSTANCE *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_instance(block->info.var,block->loops[lp].var_name,instance);

			if( !instance ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_SPECIALROOMS:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			special_room = (NAMED_SPECIAL_ROOM *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_room(block->info.var,block->loops[lp].var_name,special_room ? special_room->room : NULL);

			if( !special_room ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_SHIPS:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			ship = (SHIP_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_ship(block->info.var,block->loops[lp].var_name,ship);

			if( !ship ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_SPELLS:
			//log_stringf("opc_list: list type ENT_ILLIST_SPELLS");
			spell = (SPELL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_spell(block->info.var,block->loops[lp].var_name,spell);

			if( !spell ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_SPECIALKEYS:
			//log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
			obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_object(block->info.var,block->loops[lp].var_name,obj);

			if( !obj ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}

			break;

		case ENT_ILLIST_REPUTATION:
			//log_stringf("opc_list: list type ENT_ILLIST_REPUTATION");
			reputation = (REPUTATION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_reputation(block->info.var,block->loops[lp].var_name,reputation);

			if( !IS_VALID(reputation) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;

		case ENT_ILLIST_REPUTATION_INDEX:
			//log_stringf("opc_list: list type ENT_ILLIST_REPUTATION_INDEX");
			repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
			//log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

			// Set the variable
			variables_set_reputation_index(block->info.var,block->loops[lp].var_name,repIndex);

			if( !IS_VALID(repIndex) ) {
				iterator_stop(&block->loops[lp].d.l.list.it);
				skip = TRUE;
				break;
			}
			break;
		}

		if(skip) {
			block->loops[lp].valid = FALSE;
			block->loop--;
			return opc_skip_to_label(block,OP_ENDLIST,block->loops[lp].id,TRUE);
		}

		block->cond[block->cur_line->level] = TRUE;
	}

	opc_next_line(block);
	return TRUE;
}


DECL_OPC_FUN(opc_endlist)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_LIST,block->cur_line->label,FALSE);
}

DECL_OPC_FUN(opc_exitlist)
{
	script_loop_cleanup(block, block->cur_line->level);
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,TRUE);
}

DECL_OPC_FUN(opc_endwhile)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_WHILE,block->cur_line->label,FALSE);
}

DECL_OPC_FUN(opc_exitwhile)
{
	script_loop_cleanup(block, block->cur_line->level);
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	return opc_skip_to_label(block,OP_ENDWHILE,block->cur_line->label,TRUE);
}


DECL_OPC_FUN(opc_switch)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	SCRIPT_PARAM *arg = new_script_param();
	if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
		block->ret_val = PRET_BADSYNTAX;
		free_script_param(arg);
		return FALSE;
	}

	if (arg->type != ENT_NUMBER)
	{
		block->ret_val = PRET_BADSYNTAX;
		free_script_param(arg);
		return FALSE;
	}

	long value = arg->d.num;
	free_script_param(arg);

	if (block->script->switch_table && block->cur_line->param >= 0 && block->cur_line->param < block->script->n_switch_table)
	{
		SCRIPT_SWITCH *sw = &block->script->switch_table[block->cur_line->param];
		SCRIPT_SWITCH_CASE *swc;

		for(swc = sw->cases; swc; swc = swc->next)
		{
			if (value >= swc->a && value <= swc->b)
			{
				if(swc->line >= 0 && swc->line < block->script->lines) {
					block->line = swc->line;
					block->cur_line = &block->script->code[swc->line];
					script_loop_cleanup(block, block->cur_line->level);
					if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = TRUE;
					return TRUE;
				}
				return FALSE;
			}
		}

		// While lines start at 0, the default in a switch statement can never point to line 0, as the switch statement itself (this opcode) must come before it
		if(sw->default_case > 0 && sw->default_case < block->script->lines) {
			block->line = sw->default_case;
			block->cur_line = &block->script->code[sw->default_case];
			script_loop_cleanup(block, block->cur_line->level);
			if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = TRUE;
			return TRUE;
		}

		// Skip to the end of the switch
		return opc_skip_block(block,OP_ENDSWITCH,block->cur_line->level);
	}

	// To get to here means something bad happened
	block->ret_val = PRET_BADSYNTAX;
	return FALSE;
}

DECL_OPC_FUN(opc_endswitch)
{
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_exitswitch)
{
	// Skip to the end of the switch
	return opc_skip_block(block,OP_ENDSWITCH,block->cur_line->level);
}

DECL_OPC_FUN(opc_mob)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_M) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,mob_cmd_table[block->cur_line->param].name);

	if(mob_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted mob command '%s' with nulled security.",mob_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(IS_VALID(block->info.mob)) {
		if( !mob_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*mob_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}


	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_obj)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_O) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,obj_cmd_table[block->cur_line->param].name);

	if(obj_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted obj command '%s' with nulled security.",obj_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(IS_VALID(block->info.obj)) {
		if( !obj_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*obj_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_room)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_R) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,room_cmd_table[block->cur_line->param].name);

	if(room_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted room command '%s' with nulled security.",room_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(block->info.room) {
		if( !room_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*room_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_token)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_T) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,token_cmd_table[block->cur_line->param].name);

	if(token_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted token command '%s' with nulled security.",token_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(IS_VALID(block->info.token)) {
		if( !token_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*token_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}

	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_tokenother)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type == IFC_T) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,tokenother_cmd_table[block->cur_line->param].name);

	if(tokenother_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted tokenother command '%s' with nulled security.",tokenother_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else {
		if( !tokenother_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*tokenother_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}
	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_area)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_A) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,area_cmd_table[block->cur_line->param].name);

	if(area_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted area command '%s' with nulled security.",area_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(block->info.area) {
		if( !area_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*area_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}

	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_instance)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_I) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,instance_cmd_table[block->cur_line->param].name);

	if(instance_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted instance command '%s' with nulled security.",instance_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(IS_VALID(block->info.instance)) {
		if( !instance_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*instance_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}

	opc_next_line(block);
	return TRUE;
}

DECL_OPC_FUN(opc_dungeon)
{
	if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
		return opc_skip_block(block,block->cur_line->level-1,FALSE);

	// Verify
	if(block->type != IFC_D) {
		// Log the error
		return FALSE;
	}

	DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,dungeon_cmd_table[block->cur_line->param].name);

	if(area_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
		char buf[MIL];
		sprintf(buf, "Attempted execution of a restricted dungeon command '%s' with nulled security.",dungeon_cmd_table[block->cur_line->param].name);
		bug(buf, 0);
	} else if(IS_VALID(block->info.dungeon)) {
		if( !dungeon_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
			SCRIPT_PARAM *arg = new_script_param();
			(*dungeon_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
			free_script_param(arg);
			tail_chain();
		}
	}


	opc_next_line(block);
	return TRUE;
}


bool echo_line(SCRIPT_CB *block)
{
	char buf[MSL];
	DBG3MSG4("Executing: Line=%d, Opcode=%d(%s), Level=%d\n", block->line+1,block->cur_line->opcode,opcode_names[block->cur_line->opcode],block->cur_line->level);
	if(wiznet_script) {
		sprintf(buf,"Executing: Line=%d, Opcode=%d(%s), Level=%d", block->line+1,block->cur_line->opcode,opcode_names[block->cur_line->opcode],block->cur_line->level);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
	}
	return TRUE;
}

void script_dump(SCRIPT_DATA *script)
{
#ifdef DEBUG_MODULE
	int i;
	DBG3MSG2("vnum = %d, lines = %d\n", script->vnum, script->lines);
	if(script->code) {
		for(i=0; i < script->lines; i++) {
			DBG3MSG4("Line %d: Opcode=%d(%s), Level=%d\n", i+1,script->code[i].opcode,opcode_names[script->code[i].opcode],script->code[i].level);
		}
	}

#endif
}

void script_dump_wiznet(SCRIPT_DATA *script)
{
	int i;
	char buf[MSL];
	sprintf(buf,"widevnum = %ld#%ld, lines = %d", script->area->uid, script->vnum, script->lines);
	wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
	if(script->code) {
		for(i=0; i < script->lines; i++) {
			sprintf(buf,"Line %d: Opcode=%d(%s), Level=%d", i+1,script->code[i].opcode,opcode_names[script->code[i].opcode],script->code[i].level);
			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
	}
}

int execute_script(SCRIPT_DATA *script,
	CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
	AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
	CHAR_DATA *ch, OBJ_DATA *obj1,OBJ_DATA *obj2,CHAR_DATA *vch,CHAR_DATA *vch2,CHAR_DATA *rch,
	TOKEN_DATA *tok, char *phrase, char *trigger, int trigger_type,
	int number1, int number2, int number3, int number4, int number5)
{
	SCRIPT_CB block;	// Control block
	int saved_call_depth;	// Call depth copied
	int saved_security;	// Security copied
	bool saved_wiznet;
	long level, i;

	//DBG2ENTRY7(NUM,pvnum,PTR,script,PTR,mob,PTR,obj,PTR,room,PTR,token,PTR,ch);

	script_destructed = FALSE;

	if (!script || !script->code) {
//		bug("PROGs: No script to execute for vnum %d.", pvnum);
		return PRET_NOSCRIPT;
	}

	if (IS_VALID(mob) && !IS_NPC(mob) )
	{
//		bug("PROGs: Attempting to run a script with a player actor.", pvnum);
		return PRET_NOSCRIPT;
	}

	if ((mob && obj) || (mob && room) || (mob && token) || (mob && area) || (mob && instance) || (mob && dungeon) ||
		(obj && room) || (obj && token) || (obj && area) || (obj && instance) || (obj && dungeon) ||
		(room && token) || (room && area) || (room && instance) || (room && dungeon) ||
		(token && area) || (token && instance) || (token && dungeon) ||
		(area && instance) || (area && dungeon) ||
		(instance && dungeon)) {
//		bug("PROGs: program_flow received multiple prog types for vnum.", pvnum);
		return PRET_BADTYPE;
	}

	// Silently return.  Disabled scripts should just pretend they don't exist.
	if (!script_force_execute && IS_SET(script->flags,SCRIPT_DISABLED))
		return PRET_NOSCRIPT;

	// System scripts require system level security, only set at specific times!
	if(IS_SET(script->flags, SCRIPT_SYSTEM) && script_security < SYSTEM_SCRIPT_SECURITY)
		return PRET_NOSCRIPT;


	memset(&block,0,sizeof(block));
	block.info.block = &block;
	for(i = 0; i < MAX_NESTED_LOOPS; i++) {
		block.loops[i].valid = FALSE;
		block.loops[i].level = -1;
	}

	saved_wiznet = wiznet_script;
	wiznet_script = (bool)(int)IS_SET(script->flags,SCRIPT_WIZNET);

	if (mob) {
		mob->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_M;
		block.info.progs = mob->progs;
		block.info.location = mob->in_room;
		block.info.var = &mob->progs->vars;
		block.info.targ = &mob->progs->target;

	} else if (obj) {
		obj->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_O;
		block.info.progs = obj->progs;
		block.info.location = obj_room(obj);
		block.info.var = &obj->progs->vars;
		block.info.targ = &obj->progs->target;

	} else if (room) {
		room->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_R;
		block.info.progs = room->progs;
		block.info.location = room;
		block.info.var = &room->progs->vars;
		block.info.targ = &room->progs->target;

	} else if (token) {
		token->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_T;
		block.info.progs = token->progs;
		block.info.location = token_room(token);
		block.info.var = &token->progs->vars;
		block.info.targ = &token->progs->target;

	} else if (area) {
		area->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_A;
		block.info.progs = area->progs;
		block.info.location = NULL;
		block.info.var = &area->progs->vars;
		block.info.targ = &area->progs->target;

	} else if (instance) {
		instance->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_I;
		block.info.progs = instance->progs;
		block.info.location = NULL;
		block.info.var = &instance->progs->vars;
		block.info.targ = &instance->progs->target;

	} else if (dungeon) {
		dungeon->progs->lastreturn = PRET_EXECUTED;
		block.type = IFC_D;
		block.info.progs = dungeon->progs;
		block.info.location = NULL;
		block.info.var = &dungeon->progs->vars;
		block.info.targ = &dungeon->progs->target;

	} else {
		// Log error
		return PRET_BADTYPE;
	}

	if(phrase) strncpy(block.info.phrase,phrase,MSL);
	if(trigger) strncpy(block.info.trigger,trigger,MSL);

	/*
	// TODO: Fix formating for all widevnums
	if(wiznet_script) {
		
		sprintf(buf,"{BScript{C({W%ld{C#{W%ld{C){D: {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){x",
			(long int)(script ? script->area->uid : -1),
			(long int)(script ? script->vnum : -1),
			mob ? HANDLE(mob) : "(mob)",
			mob ? (int)VNUM(mob) : -1,
			obj ? obj->short_descr : "(obj)",
			obj ? (int)VNUM(obj) : -1,
			room ? room->name : "(room)",
			room ? (int)room->vnum : -1,
			token ? token->name : "(token)",
			token ? (int)VNUM(token) : -1,
			ch ? HANDLE(ch) : "(ch)",
			ch ? (int)VNUM(ch) : -1);
		wiznet(buf, NULL, NULL,WIZ_SCRIPTS,0,0);
		script_dump_wiznet(script);
	}
	*/

	// Save script parameters
	block.info.mob = mob;
	block.info.obj = obj;
	block.info.room = room;
	block.info.token = token;
	block.info.area = area;
	block.info.instance = instance;
	block.info.dungeon = dungeon;
	block.info.ch = ch;
	block.info.obj1 = obj1;
	block.info.obj2 = obj2;
	block.info.vch = vch;
	block.info.vch2 = vch2;
	block.info.rch = rch;
	block.info.tok = tok;
	block.info.registers[0] = number1;
	block.info.registers[1] = number2;
	block.info.registers[2] = number3;
	block.info.registers[3] = number4;
	block.info.registers[4] = number5;
	block.info.trigger_type = trigger_type;
	block.script = script;

	saved_security = script_security;

	// Non-system scripts modify the security
	if(!IS_SET(script->flags, SCRIPT_SYSTEM)) {
		if(script_security == INIT_SCRIPT_SECURITY ||
			(IS_SET(script->flags,SCRIPT_SECURED) && (script->security >= script_security)) ||
			script->security < script_security)
			script_security = script->security;
	}

	// Call depth code
	saved_call_depth = script_call_depth;
	if(!script_call_depth) {
		if(!script->depth)
			script_call_depth = MAX_CALL_LEVEL;
		else
			script_call_depth = script->depth;
	}

	// Init stack
	for (level = 0; level < MAX_NESTED_LEVEL; level++) {
		block.state[level] = IN_BLOCK;
		block.cond[level]  = TRUE;
	}
	block.level = 0;
	block.line = 0;
	block.loop = 0;
	block.ret_val = PRET_EXECUTED;
	block.cur_line = &block.script->code[block.line];
	block.next = script_call_stack;
	script_call_stack = &block;

	//DBG3MSG0("Starting script...\n");
	//if(wiznet_script) wiznet("Starting script...",NULL,NULL,WIZ_SCRIPTS,0,0);

	// Run script
	// Until the number of lines of the script has been reached or
	//	An opcode function tells it to quit.
	while(block.line < block.script->lines &&
		block.cur_line->opcode < OP_LASTCODE &&
		echo_line(&block) &&
		(*opcode_table[block.cur_line->opcode])(&block) &&
		!IS_SET(block.flags,SCRIPTEXEC_HALT)) {

		if(block.line < block.script->lines)
			block.cur_line = &block.script->code[block.line];
	}

	if(IS_SET(block.flags,SCRIPTEXEC_HALT)) script_destructed = TRUE;

	//DBG3MSG0("Completed script...\n");
	//if(wiznet_script) wiznet((script_destructed?"Script halted due to entity destruction...":"Completed script..."),NULL,NULL,WIZ_SCRIPTS,0,0);

	wiznet_script = saved_wiznet;

	DBG2EXITVALUE1(NUM,block.ret_val);
	script_security = saved_security;
	script_call_depth = saved_call_depth; // Restore call depth
	script_call_stack = script_call_stack->next; // Back up call stack

	script_loop_cleanup(&block, 0);

	return block.ret_val;
}

/*
 * Get a random PC in the room (for $r parameter)
 */
CHAR_DATA *get_random_char(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token)
{
    CHAR_DATA *vch, *victim = NULL;
    int now = 0, highest = 0;

    if ((mob && obj) || (mob && room) || (obj && room)) {
	bug("get_random_char received multiple prog types",0);
	return NULL;
    }

    if (mob)
	vch = mob->in_room->people;
    else if (obj)
	vch = obj_room(obj)->people;
    else if (token)
	vch = token_room(token)->people;
    else if (!room) {
	    bug("get_random_char: no room, object, or mob!", 0);
	    return NULL;
    } else
	vch = room->people;

    for (; vch; vch = vch->next_in_room) {
        if (mob && mob != vch && !IS_NPC(vch) && can_see(mob, vch) && (now = number_percent()) > highest) {
            victim = vch;
            highest = now;
        } else if ((now = number_percent()) > highest) {
	    victim = vch;
	    highest = now;
	}
    }

    return victim;
}


/*
 * How many other players / mobs are there in the room
 * iFlag: 0: all, 1: players, 2: mobiles 3: mobs w/ same vnum 4: same group
 */
int count_people_room(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, int iFlag)
{
    CHAR_DATA *vch;
    int count;

    if ((mob && obj) || (mob && room) || (obj && room)) {
	bug("count_people_room received multiple prog types",0);
	return 0;
    }

    if (mob && mob->in_room)
	vch = mob->in_room->people;
    else if (obj)
	vch = obj_room(obj)->people;
    else if (token)
	vch = token_room(token)->people;
    else if (room)
        vch = room->people;
    else {
	bug("count_people_room had null room obj and mob.",0);
	return 0;
    }

    for (count = 0; vch; vch = vch->next_in_room) {
	if (mob) {
	    if (mob != vch
	    && (iFlag == 0
	    || (iFlag == 1 && !IS_NPC(vch))
	    || (iFlag == 2 && IS_NPC(vch))
	    || (iFlag == 3 && IS_NPC(mob) && IS_NPC(vch)
	    && mob->pIndexData->vnum == vch->pIndexData->vnum)
	    || (iFlag == 4 && is_same_group(mob, vch)))
	    && can_see(mob, vch))
	  	++count;

	} else if (obj || room) {
	    if (iFlag == 0
	    || (iFlag == 1 && !IS_NPC(vch))
	    || (iFlag == 2 && IS_NPC(vch)))
		++count;
	}
    }

    return (count);
}


/*
 * Get the order of a mob in the room. Useful when several mobs in
 * a room have the same trigger and you want only the first of them
 * to act
 */
//@@@NIB Combined the if statements; once it knew it was a mob,
//		it didn't need to check again.
int get_order(CHAR_DATA *ch, OBJ_DATA *obj)
{
    CHAR_DATA *vch;
    OBJ_DATA *vobj;
    int i;

    if (ch && obj) {
	bug("get_order received multiple prog types",0);
	return 0;
    }

    if (ch) {
	if(!IS_NPC(ch)) return 0;
	vch = ch->in_room->people;

	for (i = 0; vch; vch = vch->next_in_room) {
	    if (vch == ch) return i;

	    if (IS_NPC(vch) && vch->pIndexData->vnum == ch->pIndexData->vnum)
		++i;
	}

    } else {
	if (obj->in_room)
	    vobj = obj->in_room->contents;
	else if (obj->carried_by->in_room->contents)
	    vobj = obj->carried_by->in_room->contents;
	else
	    vobj = NULL;

	for (i = 0; vobj; vobj = vobj->next_content) {
	    if (vobj == obj) return i;

	    if (vobj->pIndexData->vnum == obj->pIndexData->vnum)
		++i;
	}
    }

    return 0;
}

CHAR_DATA *script_get_char_blist(SCRIPT_VARINFO *info, LLIST *blist, CHAR_DATA *viewer, bool player, WNUM wnum, char *name)
{
	int nth = 1, i = 0;
	char buf[MSL];
	CHAR_DATA *ch;
	ITERATOR it;
	LLIST_UID_DATA *luid;

	if(!IS_VALID(blist)) return NULL;

	if( player && wnum.pArea && wnum.vnum > 0 ) return NULL;

	if(name) {
		nth = number_argument(name,buf);

		if(!player && parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
			name = NULL;
		} else {
			wnum = wnum_zero;
			name = buf;
		}
	}

	iterator_start(&it, blist);
	while((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
	{
		if( !luid->ptr ) continue;

		ch = (CHAR_DATA *)luid->ptr;

		if( player && IS_NPC(ch) ) continue;
		if( name && !is_name(name,ch->name) ) continue;
		if( (wnum.pArea && wnum.vnum > 0) && !wnum_match_mob(wnum, ch) ) continue;
		if( viewer && !can_see(viewer,ch) ) continue;

		if( ++i == nth )
			break;
	}
	iterator_stop(&it);

	if(luid && luid->ptr)
		return (CHAR_DATA *)luid->ptr;
	return NULL;
}

CHAR_DATA *script_get_char_list(SCRIPT_VARINFO *info, CHAR_DATA *mobs, CHAR_DATA *viewer, bool player, WNUM wnum, char *name)
{
	int nth = 1, i = 0;
	char buf[MSL];
	CHAR_DATA *ch;
	if(!mobs) return NULL;

	if( player && wnum.pArea && wnum.vnum > 0 ) return NULL;

	if(name) {
		nth = number_argument(name,buf);

		if(!player && parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
			name = NULL;
		} else {
			wnum = wnum_zero;
			name = buf;
		}
	}

	if(player) {
		for(ch = mobs; ch; ch = ch->next_in_room)
			if(!IS_NPC(ch) && is_name(name,ch->name) && (!viewer || can_see(viewer,ch)))
				if( ++i == nth ) return ch;

	} else if(wnum.pArea && wnum.vnum > 0) {
		for(ch = mobs; ch; ch = ch->next_in_room)
			if(IS_NPC(ch) && wnum_match_mob(wnum, ch) && (!viewer || can_see(viewer,ch)))
				if( ++i == nth ) return ch;

	} else if(name) {
		for(ch = mobs; ch; ch = ch->next_in_room)
			if(is_name(name,ch->name) && (!viewer || can_see(viewer,ch)))
				if( ++i == nth ) return ch;

	}
	return NULL;
}


OBJ_DATA *script_get_obj_blist(SCRIPT_VARINFO *info, LLIST *blist, CHAR_DATA *viewer, WNUM wnum, char *name)
{
	int nth = 1, i = 0;
	char buf[MSL];
	OBJ_DATA *obj;
	ITERATOR it;
	LLIST_UID_DATA *luid;

	if(!IS_VALID(blist)) return NULL;

	if(name) {
		nth = number_argument(name,buf);

		if(parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
			name = NULL;
		} else {
			wnum = wnum_zero;
			name = buf;
		}
	}

	iterator_start(&it, blist);
	while((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
	{
		if( !luid->ptr ) continue;

		obj = (OBJ_DATA *)luid->ptr;

		if( name && !is_name(name,obj->name) ) continue;
		if( (wnum.pArea && wnum.vnum > 0) && !wnum_match_obj(wnum, obj) ) continue;
		if( viewer && !can_see_obj(viewer,obj) ) continue;

		if( ++i == nth )
			break;
	}
	iterator_stop(&it);

	if(luid && luid->ptr)
		return (OBJ_DATA *)luid->ptr;
	return NULL;
}


OBJ_DATA *script_get_obj_list(SCRIPT_VARINFO *info, OBJ_DATA *objs, CHAR_DATA *viewer, int worn, WNUM wnum, char *name)
{
	int nth = 1, i = 0;
	char buf[MSL];
	OBJ_DATA *obj;
	if(!objs) return NULL;

	if(name) {
		nth = number_argument(name,buf);

		if(parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
			name = NULL;
		} else {
			wnum = wnum_zero;
			name = buf;
		}
	}

	switch(worn) {
	default:
		if(wnum.pArea && wnum.vnum > 0) {
			for(obj = objs; obj; obj = obj->next_content)
				if(wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		} else if(name) {
			for(obj = objs; obj; obj = obj->next_content)
				if(is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		}
		break;
	case 1:
		if(wnum.pArea && wnum.vnum > 0) {
			for(obj = objs; obj; obj = obj->next_content)
				if(obj->wear_loc != WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		} else if(name) {
			for(obj = objs; obj; obj = obj->next_content)
				if(obj->wear_loc != WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		}
		break;
	case 2:
		if(wnum.pArea && wnum.vnum > 0) {
			for(obj = objs; obj; obj = obj->next_content)
				if(obj->wear_loc == WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		} else if(name) {
			for(obj = objs; obj; obj = obj->next_content)
				if(obj->wear_loc == WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
					if( ++i == nth ) return obj;
		}
		break;
	}
	return NULL;
}


TOKEN_DATA *token_find_match(SCRIPT_VARINFO *info, TOKEN_DATA *tokens,char *argument, SCRIPT_PARAM *arg)
{
	char *rest;
	int i, nth = 1, matches;
	WNUM wnum = wnum_zero;
	int values[MAX_TOKEN_VALUES];
	bool match[MAX_TOKEN_VALUES];
	char buf[MSL];

	if(!(rest = expand_argument(info,argument,arg)))
		return NULL;

	if(arg->type == ENT_WIDEVNUM)
		wnum = arg->d.wnum;
	else if(arg->type == ENT_STRING) {
		nth = number_argument(arg->d.str,buf);
		if(nth < 1 || !parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum))
			return NULL;
	}


	if(!wnum.pArea || wnum.vnum < 1) return NULL;

	for(i=0;*rest && i < MAX_TOKEN_VALUES; i++) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg)))
			return NULL;

		if(arg->type == ENT_NUMBER)
			values[i] = arg->d.num;
		else if(arg->type == ENT_STRING && is_number(arg->d.str))
			values[i] = atoi(arg->d.str);
		else {
			match[i] = FALSE;
			continue;
		}
		match[i] = TRUE;
	}

	for(;i < MAX_TOKEN_VALUES; i++) match[i] = FALSE;

	for(;tokens;tokens = tokens->next) {
		if(wnum_match_token(wnum, tokens)) {
			for(matches = 0, i = 0; i < MAX_TOKEN_VALUES; i++)
				if( !match[i] || tokens->value[i] == values[i] )
					matches++;

			if( matches == MAX_TOKEN_VALUES && !--nth )
				break;
		}
	}

	return tokens;
}

/*
 * Check if ch has a given item or item type
 * vnum: item vnum or -1
 * item_type: item type or -1
 * fWear: TRUE: item must be worn, FALSE: don't care
 */
bool has_item(CHAR_DATA *ch, WNUM wnum, sh_int item_type, bool fWear)
{
    OBJ_DATA *obj;
    for (obj = ch->carrying; obj; obj = obj->next_content)
	if ((!wnum.pArea || wnum.vnum < 1 || wnum_match_obj(wnum, obj))
	&&   (item_type < 0 || obj->pIndexData->item_type == item_type)
	&&   (!fWear || obj->wear_loc != WEAR_NONE))
	    return TRUE;
    return FALSE;
}


/*
 * Check if there's a mob with given vnum in the room
 */
CHAR_DATA *get_mob_wnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, WNUM wnum)
{
    CHAR_DATA *mob;

    if ((ch && obj) || (ch && room) || (obj && room) ||
    	(ch && token) || (obj && token) || (room && token)) {
	bug("get_mob_wnum_room received multiple prog types",0);
	return NULL;
    }

    if (ch)
	mob = ch->in_room->people;
    else if (obj)
	mob = obj_room(obj)->people;
    else if (token)
	mob = token_room(token)->people;
    else mob = room->people;

    for (; mob; mob = mob->next_in_room)
	if (IS_NPC(mob) && wnum_match_mob(wnum, mob))
	    return mob;
    return NULL;
}


/*
 * Check if there's an object with given vnum in the room
 */
OBJ_DATA *get_obj_wnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, WNUM wnum)
{
    OBJ_DATA *vobj;

    if ((ch && obj) || (ch && room) || (obj && room) ||
    	(ch && token) || (obj && token) || (room && token)) {
	bug("get_obj_wnum_room received multiple prog types",0);
	return NULL;
    }

    if (ch)
	vobj = ch->in_room->contents;
    else if (obj)
	vobj = obj_room(obj)->contents;
    else if (token)
	vobj = token_room(token)->contents;
    else
	vobj = room->contents;

    for (; vobj; vobj = vobj->next_content)
	if (wnum_match_obj(wnum, vobj))
	    return vobj;
    return NULL;
}

void get_level_damage(int level, int *num, int *type, bool fRemort, bool fTwo)
{
	*num = (level + 20) / 10;
	*type = (level + 20) / 4;

	if(fTwo) *type = (*type * 7)/5 - 1;
	if(fRemort) {
		*num += 2;
		*type += 2;
	}

	*num = UMAX(1, *num);
	*type = UMAX(8, *type);
}

void do_mob_transfer(CHAR_DATA *ch,ROOM_INDEX_DATA *room,bool quiet, int mode)
{
	if( ch->desc && quiet )
	{
		ch->desc->muted++;
	}

	bool show = !quiet;
	char *phrase = quiet?"silent":NULL;

	ROOM_INDEX_DATA *in_room = ch->in_room;

	DUNGEON *in_dungeon = get_room_dungeon(ch->in_room);
	DUNGEON *to_dungeon = get_room_dungeon(room);

	INSTANCE *in_instance = get_room_instance(ch->in_room);
	INSTANCE *to_instance = get_room_instance(room);

	if (ch->fighting)
		stop_fighting(ch, TRUE);

	if( mode == TRANSFER_MODE_MOVEMENT )
	{
		check_room_shield_source(ch, show);

		if (!IS_DEAD(ch)) {
			check_room_flames(ch, show);
			if ((!IS_NPC(ch) && IS_DEAD(ch)) || (IS_NPC(ch) && ch->hit < 1))
				return;
		}

		/* moving your char negates your ambush */
		if (ch->ambush) {
			if( show )
			{
				send_to_char("You stop your ambush.\n\r", ch);
			}
			free_ambush(ch->ambush);
			ch->ambush = NULL;
		}

		/* Cancels your reciting too. This is incase move_char is called
		   from some other function and doesnt go through interpret(). */
		if (ch->recite > 0) {
			if( show )
			{
				send_to_char("You stop reciting.\n\r", ch);
				act("$n stops reciting.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
			ch->recite = 0;
		}
	}

	char_from_room(ch);
	if(room->wilds)
		char_to_vroom(ch, room->wilds, room->x, room->y);
	else
		char_to_room(ch, room);

	if( mode == TRANSFER_MODE_PORTAL )
	{
		if (show) {
			if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
			{
				WNUM dng_wnum;
				dng_wnum.pArea = in_dungeon->index->area;
				dng_wnum.vnum = in_dungeon->index->vnum;
				OBJ_DATA *portal = get_room_dungeon_portal(room, dng_wnum);

				if( IS_VALID(portal) )
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
					{
						act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);
					}
					else
					{
						act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM);
					}
				}
				else if(MOUNTED(ch))
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
						act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					else

						act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out) )
						act(in_dungeon->index->zone_out, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);
					else
						act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
				}
			}
			else if(!MOUNTED(ch)) {
				if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
					act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else
					act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
			} else {
				if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
					act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else
					act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
		}
	}
	else if( mode == TRANSFER_MODE_MOVEMENT )
	{
		if (show && !IS_AFFECTED(ch, AFF_SNEAK) && ch->invis_level < LEVEL_HERO) {
			if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
			{
				WNUM dng_wnum;
				dng_wnum.pArea = in_dungeon->index->area;
				dng_wnum.vnum = in_dungeon->index->vnum;
				OBJ_DATA *portal = get_room_dungeon_portal(room, dng_wnum);

				if( IS_VALID(portal) )
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
					{
						act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);
					}
					else
					{
						act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM);
					}
				}
				else if(MOUNTED(ch))
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
						act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					else

						act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					if( !IS_NULLSTR(in_dungeon->index->zone_out) )
						act(in_dungeon->index->zone_out, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);
					else
						act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
				}
			}
			else if (in_room->sector_type == SECT_WATER_NOSWIM)
				act("{W$n swims in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			else if (PULLING_CART(ch))
				act("{W$n has arrived, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, NULL, TO_ROOM);
			else if(!MOUNTED(ch)) {
				if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
					act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else
					act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
			} else {
				if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
					act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else
					act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
		}
	}

	move_cart(ch,room,show);

	if(show) do_look(ch, "auto");

	if( mode == TRANSFER_MODE_PORTAL )
	{
		if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
		{
			p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase,0,0,0,0,0);
		}

		if( to_instance != in_instance )
		{
			p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase,0,0,0,0,0);
		}

		p_percent_trigger( ch, NULL, NULL, NULL,NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY , phrase,0,0,0,0,0);

		if ( !IS_NPC( ch ) ) {
			p_greet_trigger( ch, PRG_MPROG ,0,0,0,0,0);
			p_greet_trigger( ch, PRG_OPROG ,0,0,0,0,0);
			p_greet_trigger( ch, PRG_RPROG ,0,0,0,0,0);
		}

	}
	else if( mode == TRANSFER_MODE_MOVEMENT )
	{
		if (!IS_WILDERNESS(room))
			check_traps(ch, show);

		if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
		{
			p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase,0,0,0,0,0);
		}

		if( IS_VALID(to_instance) && to_instance != in_instance )
		{
			p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase,0,0,0,0,0);
		}

		p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase,0,0,0,0,0);

		if (!IS_NPC(ch)) {
			p_greet_trigger(ch, PRG_MPROG,0,0,0,0,0);
			p_greet_trigger(ch, PRG_OPROG,0,0,0,0,0);
			p_greet_trigger(ch, PRG_RPROG,0,0,0,0,0);
		}

		if (!IS_DEAD(ch)) check_rocks(ch, show);
		if (!IS_DEAD(ch)) check_ice(ch, show);
		if (!IS_DEAD(ch)) check_room_flames(ch, show);
		//if (!IS_DEAD(ch)) check_ambush(ch);

		if (MOUNTED(ch) && number_percent() == 1 && get_skill(ch, gsk_riding) > 0)
			check_improve_show(ch, gsk_riding, TRUE, 8, show);

		if (!MOUNTED(ch) && get_skill(ch, gsk_trackless_step) > 0 && number_percent() == 1)
			check_improve_show(ch, gsk_trackless_step, TRUE, 8, show);

		/* Druids regenerate in nature */
		if (get_profession(ch, SUBCLASS_CLERIC) == CLASS_CLERIC_DRUID && is_in_nature(ch)) {
			ch->move += number_range(1,3);
			ch->move = UMIN(ch->move, ch->max_move);
			ch->hit += number_range(1,3);
			ch->hit  = UMIN(ch->hit, ch->max_hit);
		}

		if (!IS_NPC(ch))
			check_quest_rescue_mob(ch, show);
	}

	if( ch->desc && quiet && ch->desc->muted > 0)
	{
		ch->desc->muted--;
	}
}


bool has_trigger(LLIST **bank, int trigger)
{
	int slot;
	PROG_LIST *trig;
	ITERATOR it;

	struct trigger_type *tt = get_trigger_type_bytype(trigger);

	slot = tt ? tt->slot : TRIGSLOT_GENERAL;

	if(bank) {
		iterator_start(&it, bank[slot]);
		while((trig = (PROG_LIST *)iterator_nextdata(&it))) {
			if (is_trigger_type(trig->trig_type,trigger))
				break;
		}
		iterator_stop(&it);

		if(trig)
			return TRUE;
	}

//	DBG2EXITVALUE2(FALSE);
	return FALSE;
}

struct trigger_type *get_trigger_type(char *name, int progs)
{
	ITERATOR it;
	struct trigger_type *tt;
	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		if (!str_cmp(name, tt->name) && IS_SET(tt->progs, progs))
			break;
	}
	iterator_stop(&it);

	if (tt) return tt;

	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		if (tt->alias && is_exact_name(tt->alias, name) && IS_SET(tt->progs, progs))
			break;
	}
	iterator_stop(&it);

	return tt;
}

struct trigger_type *get_trigger_type_bytype(int type)
{
	ITERATOR it;
	struct trigger_type *tt;
	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		if (tt->type == type)
			break;
	}
	iterator_stop(&it);

	return tt;
}

bool is_trigger_type(int tindex, int type)
{
	return tindex == type;
}

bool mp_same_group(CHAR_DATA *ch,CHAR_DATA *vch,CHAR_DATA *to)
{
	return (ch != vch && ch != to && is_same_group(vch,to));
}

bool rop_same_group(CHAR_DATA *ch,CHAR_DATA *vch,CHAR_DATA *to)
{
	// vch is NULL from these
	return (is_same_group(ch,to));
}


ROOM_INDEX_DATA *get_exit_dest(ROOM_INDEX_DATA *room, char *argument)
{
    EXIT_DATA *ex;

    if (!room || !argument[0])
	return NULL;

    if (!str_cmp(argument, "north"))		ex = room->exit[DIR_NORTH];
    else if (!str_cmp(argument, "south"))	ex = room->exit[DIR_SOUTH];
    else if (!str_cmp(argument, "west"))	ex = room->exit[DIR_WEST];
    else if (!str_cmp(argument, "east"))	ex = room->exit[DIR_EAST];
    else if (!str_cmp(argument, "up"))		ex = room->exit[DIR_UP];
    else if (!str_cmp(argument, "down"))	ex = room->exit[DIR_DOWN];
    else if (!str_cmp(argument, "northeast"))	ex = room->exit[DIR_NORTHEAST];
    else if (!str_cmp(argument, "northwest"))	ex = room->exit[DIR_NORTHWEST];
    else if (!str_cmp(argument, "southeast"))	ex = room->exit[DIR_SOUTHEAST];
    else if (!str_cmp(argument, "southwest"))	ex = room->exit[DIR_SOUTHWEST];
    else return NULL;

    return (ex && ex->u1.to_room) ? ex->u1.to_room : NULL;
}


bool script_change_exit(ROOM_INDEX_DATA *pRoom, ROOM_INDEX_DATA *pToRoom, int door)
{
	EXIT_DATA *pExit;

	if (!pToRoom) {
		sh_int rev;

		if (!pRoom->exit[door]) {
			bug("script_change_exit: Couldn't delete exit. %d", pRoom->vnum);
			return FALSE;
		}

		if( IS_SET(pRoom->exit[door]->exit_info, (EX_NOUNLINK|EX_PREVFLOOR|EX_NEXTFLOOR)) )
		{
			bug("script_change_exit: Exit is protected from deletion. %d", pRoom->vnum);
			return FALSE;
		}

		// Remove ToRoom Exit.
		rev = rev_dir[door];
		pToRoom = pRoom->exit[door]->u1.to_room;

		if (pToRoom->exit[rev]) {
			free_exit(pToRoom->exit[rev]);
			pToRoom->exit[rev] = NULL;
		}

		// Remove this exit.
		free_exit(pRoom->exit[door]);
		pRoom->exit[door] = NULL;

		return TRUE;
	}

	// Rules...
	// EITHER -> ENVIRON ... OK
	// STATIC -> CLONE ..... NOT OK
	// CLONE -> STATIC ..... WILL CLONE
	// CLONE -> CLONE ...... OK

	if(pToRoom != &room_pointer_environment) {
		if(room_is_clone(pRoom)) {
			if(!room_is_clone(pToRoom) && !(pToRoom = create_virtual_room(pToRoom,false,false)))
				return false;
		} else if(room_is_clone(pToRoom)) {
			bug("script_change_exit: A link cannot be made from a static room to a clone room.\n\r",0);
			return FALSE;
		}
	}

	if(room_is_clone(pRoom)) {
		if(pToRoom != &room_pointer_environment && !room_is_clone(pToRoom)) {
			// Should this be illegal or should it be made into a cloned room?
			bug("script_change_exit: A link cannot be made between static and clone room.\n\r",0);
			return FALSE;
		}
	} else {
		if(pToRoom != &room_pointer_environment && room_is_clone(pToRoom)) {
			bug("script_change_exit: A link cannot be made between static and clone room.\n\r",0);
			return FALSE;
		}
	}

	if(pToRoom != &room_pointer_environment) {
		if (pToRoom->exit[rev_dir[door]]) {
			bug("script_change_exit: Reverse-side exit to room already exists.", 0);
			return FALSE;
		}
	}

	if (!pRoom->exit[door])
	{
		pRoom->exit[door] = new_exit();
		// This will allow scripts to modify the locks of GENERATED exits
		SET_BIT(pRoom->exit[door]->door.lock.flags, LOCK_CREATED);
		SET_BIT(pRoom->exit[door]->door.rs_lock.flags, LOCK_CREATED);
		// Create one special keys list, they share it.
		pRoom->exit[door]->door.rs_lock.special_keys = list_createx(FALSE, NULL, delete_list_uid_data);
		pRoom->exit[door]->door.lock.special_keys = pRoom->exit[door]->door.rs_lock.special_keys;
	}

	pRoom->exit[door]->u1.to_room = pToRoom;
	pRoom->exit[door]->orig_door = door;
	pRoom->exit[door]->from_room = pRoom;

	if(pToRoom != &room_pointer_environment) {
		door = rev_dir[door];
		pExit = new_exit();
		// This will allow scripts to modify the locks of GENERATED exits
		SET_BIT(pExit->door.lock.flags, LOCK_CREATED);
		SET_BIT(pExit->door.rs_lock.flags, LOCK_CREATED);
		// Create one special keys list, they share it.
		pExit->door.rs_lock.special_keys = list_createx(FALSE, NULL, delete_list_uid_data);
		pExit->door.lock.special_keys = pExit->door.rs_lock.special_keys;
		pExit->u1.to_room = pRoom;
		pExit->orig_door = door;
		pExit->from_room = pToRoom;
		pToRoom->exit[door] = pExit;
	} else {
		// Mark it as an environment
		SET_BIT(pRoom->exit[door]->exit_info, EX_ENVIRONMENT);
	}

	return TRUE;
}


char *trigger_name(int type)
{
	struct trigger_type *tt = get_trigger_type_bytype(type);

	return tt ? tt->name : "INVALID";
}

char *trigger_phrase(int type, char *phrase)
{
	int sn;
	if(type == TRIG_SPELLCAST) {
		sn = atoi(phrase);
		SKILL_DATA *skill = get_skill_data_uid(sn);
		if(!IS_VALID(skill)) return "??";
		return skill->name;
	}

	return phrase;
}

char *trigger_phrase_olcshow(int type, char *phrase, bool is_rprog, bool is_tprog)
{
	int sn;
	if(type == TRIG_SPELLCAST) {
		sn = atoi(phrase);
		SKILL_DATA *skill = get_skill_data_uid(sn);
		if(!IS_VALID(skill)) return "??";
		return skill->name;
	}

	if(	type == TRIG_EXIT ||
		type == TRIG_EXALL ||
		type == TRIG_KNOCK ||
		type == TRIG_KNOCKING ||
		type == TRIG_SHOWEXIT ||
		type == TRIG_LOOK_AT ) {
		sn = atoi(phrase);

		if( sn < 0 || sn >= MAX_DIR) return "nowhere";

		return dir_name[sn];
	}

	// Only care if is_rprog/is_tprog is set
	if((is_rprog || is_tprog) && (type == TRIG_OPEN || type == TRIG_CLOSE)) {
		sn = atoi(phrase);

		if( is_rprog ) {
			if( sn < 0 || sn >= MAX_DIR) return "nowhere";

			return dir_name[sn];
		} else {
			if( sn < 0 || sn >= MAX_DIR) return phrase;

			return dir_name_phrase[sn];
		}
	}

	return phrase;

}

// Common entry point for all the queued commands!
void script_interpret(SCRIPT_VARINFO *info, char *command)
{
	char buf[MSL];

	one_argument(command,buf);

	if(info->mob) {
		if(!str_cmp(buf,"mob")) mob_interpret(info,command);
		else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
		else {
			BUFFER *buffer = new_buf();
			expand_string(info,command,buffer);
			interpret(info->mob,buf_string(buffer));
			free_buf(buffer);
		}
		return;
	}

	if(info->obj) {
		if(!str_cmp(buf,"obj")) obj_interpret(info,command);
		else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
		return;
	}

	if(info->room) {
		if(!str_cmp(buf,"room")) room_interpret(info,command);
		else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
		return;
	}

	if(info->token) {
		if(!str_cmp(buf,"token")) token_interpret(info,command);
		return;
	}

	// Complain
}

typedef bool (*MATCH_STRING)(char *a, char *b);
typedef bool (*MATCH_NUMBER)(int a, int b);
typedef bool (*MATCH_RANGE)(int a, int b, int c);


// MATCH_STRING
static bool __attribute__ ((unused)) match_substr(register char *a, register char *b)
{
	register char *a2;
	register char *b2;

	if(!a || !b) return FALSE;

	if(!*b) return TRUE;

	if(!*a) return FALSE;

	while(*a) {
		for(a2 = a, b2 = b;(*a2 && *b2 && LOWER(*a2) == LOWER(*b2)); ++a2, ++b2);

		if(!*b2) return TRUE;

		a++;
	}

	return FALSE;
}

// MATCH_STRING
static bool __attribute__ ((unused)) match_exact_name(char *a, char *b)
{
	return !str_cmp(a, b);
}

// MATCH_STRING
static bool __attribute__ ((unused)) match_name(char *a, char *b)
{
	return is_name(b, a);
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_random(int a, int b)
{
	return number_range(0, b-1) < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_percent(int a, int b)
{
	return number_percent() < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_equal(int a, int b)
{
	return a == b;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_lt(int a, int b)
{
	return b < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_gt(int a, int b)
{
	return b > a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_not_equal(int a, int b)
{
	return a != b;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_lte(int a, int b)
{
	return b <= a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_gte(int a, int b)
{
	return b >= a;
}

// STRING TRIGGER
int test_string_trigger(char *string, char *wildcard, MATCH_STRING match, int type,
			CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
			CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
			OBJ_DATA *obj1, OBJ_DATA *obj2,
			int register1, int register2, int register3, int register4, int register5)
{
	ITERATOR tit;	// Token iterator
	ITERATOR pit;	// Prog iterator
	PROG_LIST *prg;
	TOKEN_DATA *token;
	unsigned long uid[2];
	int slot;
	int ret_val = PRET_NOSCRIPT, ret;

	if ((mob && obj) || (mob && room) || (obj && room)) {
		bug("test_string_trigger: Multiple program types in trigger %d.", type);
		PRETURN;
	}

	struct trigger_type *tt = get_trigger_type_bytype(type);

	slot = tt ? tt->slot : TRIGSLOT_GENERAL;


	if (mob) {
		script_mobile_addref(mob);

		// Save the UID
		uid[0] = mob->id[0];
		uid[1] = mob->id[1];

		// Check for tokens FIRST
		iterator_start(&tit, mob->ltokens);
		// Loop Level 1
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				// Loop Level 2
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(string, prg->trig_phrase)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);
								script_token_remref(token);
								script_mobile_remref(mob);
								return ret;
							}
						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			// Loop Level 1:
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(string, prg->trig_phrase)) {
						ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&pit);
							script_mobile_remref(mob);
							return ret;
						}

					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);


			if(ret_val == PRET_NOSCRIPT && wildcard != NULL && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1])
			{
				// Check for tokens FIRST
				iterator_start(&tit, mob->ltokens);
				// Loop Level 1
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						// Loop Level 2
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_exact_name(wildcard, prg->trig_phrase)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&tit);
										iterator_stop(&pit);
										script_token_remref(token);
										script_mobile_remref(mob);
										return ret;
									}
								}
							}
						}
						iterator_stop(&pit);
						script_token_remref(token);
						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
					iterator_start(&pit, mob->pIndexData->progs[slot]);
					// Loop Level 1:
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_exact_name(wildcard, prg->trig_phrase)) {
								ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);
									script_mobile_remref(mob);
									return ret;
								}

							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);
				}
			}
		}
		script_mobile_remref(mob);

	} else if (obj && obj_room(obj) ) {
		// Save the UID
		uid[0] = obj->id[0];
		uid[1] = obj->id[1];
		script_object_addref(obj);

		// Check for tokens FIRST
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(string, prg->trig_phrase)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);

								script_token_remref(token);
								script_object_remref(obj);
								return ret;
							}

						}
					}
				}
				iterator_stop(&pit);

				script_token_remref(token);
				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(string, prg->trig_phrase)) {
						ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&pit);
							script_object_remref(obj);
							return ret;
						}

					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			if(ret_val == PRET_NOSCRIPT && wildcard != NULL && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1])
			{
				// Check for tokens FIRST
				iterator_start(&tit, obj->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_exact_name(wildcard, prg->trig_phrase)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&tit);
										iterator_stop(&pit);

										script_token_remref(token);
										script_object_remref(obj);
										return ret;
									}

								}
							}
						}
						iterator_stop(&pit);

						script_token_remref(token);
						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, obj->pIndexData->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_exact_name(wildcard, prg->trig_phrase)) {
								ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);
									script_object_remref(obj);
									return ret;
								}

							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);
				}
			}
		}

		script_object_remref(obj);

	} else if (room) {
		ROOM_INDEX_DATA *source;

		if(room->source) {
			source = room->source;
			uid[0] = room->id[0];
			uid[1] = room->id[1];
		} else {
			source = room;
		}

		script_room_addref(room);

		// Check for tokens FIRST
		iterator_start(&tit, room->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(string, prg->trig_phrase)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);

								script_token_remref(token);
								script_room_remref(room);
								return ret;
							}

						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);

				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, source->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(string, prg->trig_phrase)) {
						ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&pit);
							script_room_remref(room);
							return ret;
						}
					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);


			if(ret_val == PRET_NOSCRIPT && !script_destructed && wildcard != NULL)
			{
				// Check for tokens FIRST
				iterator_start(&tit, room->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_exact_name(wildcard, prg->trig_phrase)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&tit);
										iterator_stop(&pit);

										script_token_remref(token);
										script_room_remref(room);
										return ret;
									}

								}
							}
						}
						iterator_stop(&pit);
						script_token_remref(token);

						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, source->progs->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_exact_name(wildcard, prg->trig_phrase)) {
								ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);
									script_room_remref(room);
									return ret;
								}
							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);

				}
			}

		}
		script_room_remref(room);

	} else
		bug("test_string_trigger: no program type for trigger %d.", type);

	PRETURN;
}


/*
 * A general purpose string trigger. Matches argument to a string trigger
 * phrase.
 */
int p_act_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_string_trigger(argument, "*", match_substr, type, mob, obj, room, ch, victim, victim2, obj1, obj1, register1, register2, register3, register4, register5);
}

// Similar to p_act_trigger, except it does EXACT match
int p_exact_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type,
	int register1, int register2, int register3, int register4, int register5)
{
	/*
	if( type == TRIG_SPELLCAST ) {
		char buf[MIL];

		sprintf(buf, "SPELLCAST: %s", argument);
		wiznet(buf, NULL, NULL, WIZ_SCRIPTS, 0, 0);
	}
	*/
	return test_string_trigger(argument, "*", match_exact_name, type, mob, obj, room, ch, victim, victim2, obj1, obj1, register1, register2, register3, register4, register5);
}

// Similar to p_act_trigger, except it uses is_name
int p_name_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_string_trigger(argument, "*", match_name, type, mob, obj, room, ch, victim, victim2, obj1, obj1, register1, register2, register3, register4, register5);
}




int test_number_trigger(int number, int wildcard, MATCH_NUMBER match, int type,
			CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
			AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
			CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
			OBJ_DATA *obj1, OBJ_DATA *obj2, TOKEN_DATA *tok,
			char *phrase,
			int register1, int register2, int register3, int register4, int register5)
{
	//char buf[MIL];
	ITERATOR tit;	// Token iterator
	ITERATOR pit;	// Prog iterator
	PROG_LIST *prg;
	unsigned long uid[2];
	int slot;
	int ret_val = PRET_NOSCRIPT, ret;

	if ((mob && obj) || (mob && room) || (mob && token) || (mob && area) || (mob && instance) || (mob && dungeon) ||
		(obj && room) || (obj && token) || (obj && area) || (obj && instance) || (obj && dungeon) ||
		(room && token) || (room && area) || (room && instance) || (room && dungeon) ||
		(token && area) || (token && instance) || (token && dungeon) ||
		(area && instance) || (area && dungeon) ||
		(instance && dungeon)) {
		bug("test_number_trigger: Multiple program types in trigger %d.", type);
		PRETURN;
	}

	struct trigger_type *tt = get_trigger_type_bytype(type);

	slot = tt ? tt->slot : TRIGSLOT_GENERAL;


	if (mob) {
		script_mobile_addref(mob);

		// Save the UID
		uid[0] = mob->id[0];
		uid[1] = mob->id[1];

		// Check for tokens FIRST
		iterator_start(&tit, mob->ltokens);
		// Loop Level 1
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {

			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				// Loop Level 2
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(prg->trig_number, number)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			// Loop Level 1:
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
						SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( number != wildcard )
			{
				// Check for tokens FIRST
				iterator_start(&tit, mob->ltokens);
				// Loop Level 1
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {

					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						// Loop Level 2
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_equal(prg->trig_number, wildcard)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
									SETPRET;
								}
							}
						}
						iterator_stop(&pit);
						script_token_remref(token);
					}
				}
				iterator_stop(&tit);

				if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
					iterator_start(&pit, mob->pIndexData->progs[slot]);
					// Loop Level 1:
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								SETPRET;
							}
						}
					}
					iterator_stop(&pit);

				}
			}

		}
		script_mobile_remref(mob);

	} else if (obj && obj_room(obj) ) {
		// Save the UID
		uid[0] = obj->id[0];
		uid[1] = obj->id[1];
		script_object_addref(obj);

		// Check for tokens FIRST
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(prg->trig_number, number)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
						}
					}
				}
				iterator_stop(&pit);

				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(!script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( number != wildcard )
			{
				// Check for tokens FIRST
				iterator_start(&tit, obj->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_equal(prg->trig_number, wildcard)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									SETPRET;
								}
							}
						}
						iterator_stop(&pit);

						script_token_remref(token);
					}
				}
				iterator_stop(&tit);

				if(!script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, obj->pIndexData->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									SETPRET;
							}
						}
					}
					iterator_stop(&pit);
				}
			}

		}

		script_object_remref(obj);

	} else if (room) {
		ROOM_INDEX_DATA *source;

		if(room->source) {
			source = room->source;
			uid[0] = room->id[0];
			uid[1] = room->id[1];
		} else {
			source = room;
		}

		script_room_addref(room);

		// Check for tokens FIRST
		iterator_start(&tit, room->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ((*match)(prg->trig_number, number)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(!script_destructed && source->progs->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, source->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( number != wildcard )
			{
				// Check for tokens FIRST
				iterator_start(&tit, room->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,type)) {
								if (match_equal(prg->trig_number, wildcard)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									SETPRET;
								}
							}
						}
						iterator_stop(&pit);
						script_token_remref(token);
					}
				}
				iterator_stop(&tit);

				if(!script_destructed && source->progs->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, source->progs->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if (match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									SETPRET;
							}
						}
					}
					iterator_stop(&pit);

				}
			}

		}
		script_room_remref(room);
	} else if(token) {
		if( token->pIndexData->progs ) {
			script_token_addref(token);
			script_destructed = FALSE;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			// Loop Level 2
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
			{
				iterator_start(&pit, token->pIndexData->progs[slot]);
				// Loop Level 2
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if (match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								SETPRET;
						}
					}
				}
				iterator_stop(&pit);
			}

			script_token_remref(token);
		}

	} else if(area) {
		if( area->progs->progs ) {
			iterator_start(&pit, area->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, NULL, area, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
			{
				iterator_start(&pit, area->progs->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if (match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, NULL, area, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								SETPRET;
						}
					}
				}
				iterator_stop(&pit);
			}
		}

	} else if(instance) {
		if( instance->blueprint->progs ) {
			script_instance_addref(instance);
			script_destructed = FALSE;
			iterator_start(&pit, instance->blueprint->progs[slot]);
			// Loop Level 2
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, NULL, NULL, instance, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
			{
				iterator_start(&pit, instance->blueprint->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if (match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, NULL, NULL, instance, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								SETPRET;
						}
					}
				}
				iterator_stop(&pit);
			}

			script_instance_remref(instance);
		}

	} else if(dungeon) {
		if( dungeon->index->progs ) {
			script_dungeon_addref(dungeon);
			script_destructed = FALSE;
			iterator_start(&pit, dungeon->index->progs[slot]);
			// Loop Level 2
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, NULL, NULL, NULL, dungeon, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							SETPRET;
					}
				}
			}
			iterator_stop(&pit);

			if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
			{
				iterator_start(&pit, dungeon->index->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if (match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, NULL, NULL, NULL, dungeon, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								SETPRET;
						}
					}
				}
				iterator_stop(&pit);
			}

			script_dungeon_remref(dungeon);
		}

	} else
		bug("test_number_trigger: no program type for trigger %d.", type);

	PRETURN;
}

int p_percent_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_number_trigger(0, 0, match_percent, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj1, NULL, phrase, register1, register2, register3, register4, register5);
}

int p_percent2_trigger(AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_number_trigger(0, 0, match_percent, type, NULL, NULL, NULL, NULL, area, instance, dungeon, ch, victim, victim2, obj1, obj1, NULL, phrase, register1, register2, register3, register4, register5);
}


int p_percent_token_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, TOKEN_DATA *tok, int type, char *phrase,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_number_trigger(0, 0, match_percent, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj1, tok, phrase, register1, register2, register3, register4, register5);
}



int p_number_trigger(int number, int wildcard, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
	CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_number_trigger(number, wildcard, match_equal, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj1, NULL, phrase, register1, register2, register3, register4, register5);
}

int p_bribe_trigger(CHAR_DATA *mob, CHAR_DATA *ch, int amount,
	int register1, int register2, int register3, int register4, int register5)
{
	return test_number_trigger(amount, -1, match_gte, TRIG_BRIBE, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
}


int test_number_sight_trigger(int number, int wildcard, MATCH_NUMBER match, int type, int typeall,
			CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
			CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
			OBJ_DATA *obj1, OBJ_DATA *obj2,
			char *phrase,
			int register1, int register2, int register3, int register4, int register5)
{
	ITERATOR tit;	// Token iterator
	ITERATOR pit;	// Prog iterator
	PROG_LIST *prg;
	TOKEN_DATA *token;
	unsigned long uid[2];
	int slot;
	int ret_val = PRET_NOSCRIPT, ret;

	if ((mob && obj) || (mob && room) || (obj && room)) {
		bug("test_number_sight_trigger: Multiple program types in trigger %d.", type);
		PRETURN;
	}

	struct trigger_type *tt = get_trigger_type_bytype(type);
	struct trigger_type *ttall = get_trigger_type_bytype(typeall);

	// They must be in the same slot
	if( !tt || !ttall || tt->slot != ttall->slot )
	{
		bug("test_number_sight_trigger: slot mismatch for sighted trigger %d.", type);
		PRETURN;
	}

	slot = ttall->slot;

	if (mob) {
		script_mobile_addref(mob);

		// Save the UID
		uid[0] = mob->id[0];
		uid[1] = mob->id[1];

		// Check for tokens FIRST
		iterator_start(&tit, mob->ltokens);
		// Loop Level 1
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				// Loop Level 2
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (type != typeall && is_trigger_type(prg->trig_type,type) &&
						(IS_SET(token->flags, TOKEN_SEE_ALL) ||
							(mob->position == mob->pIndexData->default_pos && can_see(mob, enactor))) &&
						(*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&tit);
							iterator_stop(&pit);
							script_token_remref(token);
							script_mobile_remref(mob);
							return ret;
						}

					} else if (is_trigger_type(prg->trig_type,typeall) &&
						(*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&tit);
							iterator_stop(&pit);
							script_token_remref(token);
							script_mobile_remref(mob);
							return ret;
						}

					}
					BREAKPRET;
				}
				iterator_stop(&pit);
				script_token_remref(token);
				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			// Loop Level 1:
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (type != typeall && is_trigger_type(prg->trig_type,type) &&
					mob->position == mob->pIndexData->default_pos &&
					can_see(mob, enactor) &&
					(*match)(prg->trig_number, number)) {
					ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);
						script_mobile_remref(mob);
						return ret;
					}

				} else if (is_trigger_type(prg->trig_type,typeall) &&
					(*match)(prg->trig_number, number)) {
					ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);
						script_mobile_remref(mob);
						return ret;
					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard&& IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) )
			{
				// Check for tokens FIRST
				iterator_start(&tit, mob->ltokens);
				// Loop Level 1
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						// Loop Level 2
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (type != typeall && is_trigger_type(prg->trig_type,type) &&
								(IS_SET(token->flags, TOKEN_SEE_ALL) ||
									(mob->position == mob->pIndexData->default_pos && can_see(mob, enactor))) &&
								match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&tit);
									iterator_stop(&pit);
									script_token_remref(token);
									script_mobile_remref(mob);
									return ret;
								}

							} else if (is_trigger_type(prg->trig_type,typeall) &&
								match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&tit);
									iterator_stop(&pit);
									script_token_remref(token);
									script_mobile_remref(mob);
									return ret;
								}

							}
							BREAKPRET;
						}
						iterator_stop(&pit);
						script_token_remref(token);
						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs)
				{
					iterator_start(&pit, mob->pIndexData->progs[slot]);
					// Loop Level 1:
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (type != typeall && is_trigger_type(prg->trig_type,type) &&
							mob->position == mob->pIndexData->default_pos &&
							can_see(mob, enactor) &&
							match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&pit);
								script_mobile_remref(mob);
								return ret;
							}

						} else if (is_trigger_type(prg->trig_type,typeall) &&
							match_equal(prg->trig_number, wildcard)) {
							ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&pit);
								script_mobile_remref(mob);
								return ret;
							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);
				}

			}
		}
		script_mobile_remref(mob);

	} else if (obj && obj_room(obj) ) {
		// Save the UID
		uid[0] = obj->id[0];
		uid[1] = obj->id[1];
		script_object_addref(obj);

		// Check for tokens FIRST
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,typeall)) {
						if ((*match)(prg->trig_number, number)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);
								script_token_remref(token);
								script_object_remref(obj);
								return ret;
							}

						}
					}
				}
				iterator_stop(&pit);

				script_token_remref(token);
				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,typeall)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&pit);
							script_object_remref(obj);
							return ret;
						}
					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs)
			{
				// Check for tokens FIRST
				iterator_start(&tit, obj->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,typeall)) {
								if (match_equal(prg->trig_number, wildcard)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&tit);
										iterator_stop(&pit);
										script_token_remref(token);
										script_object_remref(obj);
										return ret;
									}

								}
							}
						}
						iterator_stop(&pit);

						script_token_remref(token);
						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, obj->pIndexData->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,typeall)) {
							if (match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);
									script_object_remref(obj);
									return ret;
								}
							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);
				}

			}

		}

		script_object_remref(obj);

	} else if (room) {
		ROOM_INDEX_DATA *source;

		if(room->source) {
			source = room->source;
			uid[0] = room->id[0];
			uid[1] = room->id[1];
		} else {
			source = room;
		}

		script_room_addref(room);

		// Check for tokens FIRST
		iterator_start(&tit, room->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,typeall)) {
						if ((*match)(prg->trig_number, number)) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);
								script_token_remref(token);
								script_room_remref(room);
								return ret;
							}

						}
					}
					BREAKPRET;
				}
				iterator_stop(&pit);
				script_token_remref(token);

				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, source->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,typeall)) {
					if ((*match)(prg->trig_number, number)) {
						ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&pit);

								script_room_remref(room);
								return ret;
							}
					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard)
			{
				// Check for tokens FIRST
				iterator_start(&tit, room->ltokens);
				while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
					if( token->pIndexData->progs ) {
						script_token_addref(token);
						script_destructed = FALSE;
						iterator_start(&pit, token->pIndexData->progs[slot]);
						while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
							if (is_trigger_type(prg->trig_type,typeall)) {
								if (match_equal(prg->trig_number, wildcard)) {
									ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&tit);
										iterator_stop(&pit);
										script_token_remref(token);
										script_room_remref(room);
										return ret;
									}

								}
							}
							BREAKPRET;
						}
						iterator_stop(&pit);
						script_token_remref(token);

						BREAKPRET;
					}
				}
				iterator_stop(&tit);

				if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
					script_destructed = FALSE;
					iterator_start(&pit, source->progs->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,typeall)) {
							if (match_equal(prg->trig_number, wildcard)) {
								ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
									if( ret != PRET_NOSCRIPT) {
										iterator_stop(&pit);

										script_room_remref(room);
										return ret;
									}
							}
						}
						BREAKPRET;
					}
					iterator_stop(&pit);

					if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard)
					{
					}
				}
			}
		}
		script_room_remref(room);

	} else
		bug("test_number_sight_trigger: no program type for trigger %d.", type);

	PRETURN;
}


int p_location_trigger(CHAR_DATA *ch, ROOM_INDEX_DATA *room, int number, int wildcard, MATCH_NUMBER match, int type, int trig, int trigall, int register1, int register2, int register3, int register4, int register5)
{
	ITERATOR oit, mit;

	CHAR_DATA *mob;
	OBJ_DATA *obj;
	//TOKEN_DATA *token;
	//PROG_LIST *prg;
	//unsigned long uid[2];
	int ret_val = PRET_NOSCRIPT; // Default for a trigger loop is NO SCRIPT

	// If not in a valid room, there's nothing to check!
	if (!ch || !room)
		PRETURN;

	if (type == PRG_MPROG) {
		iterator_start(&mit, room->lpeople);
		while((ret_val == PRET_NOSCRIPT) && (mob = (CHAR_DATA *)iterator_nextdata(&mit))) {
			ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
						mob, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
		}

		iterator_stop(&mit);

	} else if (type == PRG_OPROG) {
		// Check carrying inventory
		iterator_start(&oit, ch->lcarrying);
		while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
			ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
						NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
		}

		iterator_stop(&oit);
		ISSETPRET;

		// Check room contents
		iterator_start(&oit, room->lcontents);
		while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
			ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
						NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
		}

		iterator_stop(&oit);
		ISSETPRET;

		iterator_start(&mit, room->lpeople);
		while((ret_val == PRET_NOSCRIPT) && (mob = (CHAR_DATA *)iterator_nextdata(&mit))) {
			// Check every other mobile
			if( mob != ch ) {
				iterator_start(&oit, mob->lcarrying);
				while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
					ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
								NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
				}
				iterator_stop(&oit);
			}
		}
		iterator_stop(&mit);

	} else if (type == PRG_RPROG) {
		ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
					NULL, NULL, room, ch, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
	}

	PRETURN;
}

int p_exit_trigger(CHAR_DATA *ch, int dir, int type, int register1, int register2, int register3, int register4, int register5)
{
	return p_location_trigger(ch, ch ? ch->in_room : NULL, dir, -1, match_equal, type, TRIG_EXIT, TRIG_EXALL, register1, register2, register3, register4, register5);
}

int p_direction_trigger(CHAR_DATA *ch, ROOM_INDEX_DATA *here, int dir, int type, int trigger, int register1, int register2, int register3, int register4, int register5)
{
	return p_location_trigger(ch, here, dir, -1, match_equal, type, trigger, trigger, register1, register2, register3, register4, register5);
}


static bool __attribute__ ((unused)) match_target_name(register char *a, register char *b)
{
	char buf[MIL];

	while(*a) {
		a = one_argument(a, buf);
		if( is_name(buf, b) || !str_cmp("all", buf) )
			return TRUE;
	}

	return FALSE;
}


int test_vnumname_trigger(char *name, int vnum, int type,
			CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
			CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
			OBJ_DATA *obj1, OBJ_DATA *obj2,
			char *phrase,
			int register1, int register2, int register3, int register4, int register5)
{
	ITERATOR tit;	// Token iterator
	ITERATOR pit;	// Prog iterator
	PROG_LIST *prg;
	TOKEN_DATA *token;
	unsigned long uid[2];
	int slot;
	int ret_val = PRET_NOSCRIPT, ret;

	if ((mob && obj) || (mob && room) || (obj && room)) {
		bug("test_vnumname_trigger: Multiple program types in trigger %d.", type);
		PRETURN;
	}

	struct trigger_type *tt = get_trigger_type_bytype(type);

	slot = tt ? tt->slot : TRIGSLOT_GENERAL;


	if (mob) {
		script_mobile_addref(mob);

		// Save the UID
		uid[0] = mob->id[0];
		uid[1] = mob->id[1];

		// Check for tokens FIRST
		iterator_start(&tit, mob->ltokens);
		// Loop Level 1
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				// Loop Level 2
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
							(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT ) {
								iterator_stop(&tit);
								iterator_stop(&pit);

								script_token_remref(token);
								script_mobile_remref(mob);
								return ret;
							}
						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			// Loop Level 1:
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
						(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
						ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT ) {
								iterator_stop(&pit);

								script_mobile_remref(mob);
								return ret;
							}

					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			// RECHECK FOR WILDCARDS

			// Check for tokens FIRST
			iterator_start(&tit, mob->ltokens);
			// Loop Level 1
			while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
				if( token->pIndexData->progs ) {
					script_token_addref(token);
					script_destructed = FALSE;
					iterator_start(&pit, token->pIndexData->progs[slot]);
					// Loop Level 2
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
								(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
								ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT ) {
									iterator_stop(&tit);
									iterator_stop(&pit);

									script_token_remref(token);
									script_mobile_remref(mob);
									return ret;
								}
							}
						}
					}
					iterator_stop(&pit);
					script_token_remref(token);
				}
			}
			iterator_stop(&tit);

			if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
				iterator_start(&pit, mob->pIndexData->progs[slot]);
				// Loop Level 1:
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
							(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
							ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);

									script_mobile_remref(mob);
									return ret;
								}

						}
					}
					BREAKPRET;
				}
				iterator_stop(&pit);
			}

		}
		script_mobile_remref(mob);

	} else if (obj && obj_room(obj) ) {
		// Save the UID
		uid[0] = obj->id[0];
		uid[1] = obj->id[1];
		script_object_addref(obj);

		// Check for tokens FIRST
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
							(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);

								script_token_remref(token);
								script_object_remref(obj);
								return ret;
							}

						}
					}
				}
				iterator_stop(&pit);

				script_token_remref(token);
				BREAKPRET;
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
						(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
						ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&pit);

								script_object_remref(obj);
								return ret;
							}

					}
				}
				BREAKPRET;
			}
			iterator_stop(&pit);

			// RECHECK WILDCARDS

			// Check for tokens FIRST
			iterator_start(&tit, obj->ltokens);
			while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
				if( token->pIndexData->progs ) {
					script_token_addref(token);
					script_destructed = FALSE;
					iterator_start(&pit, token->pIndexData->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
								(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
								ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&tit);
									iterator_stop(&pit);

									script_token_remref(token);
									script_object_remref(obj);
									return ret;
								}

							}
						}
					}
					iterator_stop(&pit);

					script_token_remref(token);
					BREAKPRET;
				}
			}
			iterator_stop(&tit);

			if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
				script_destructed = FALSE;
				iterator_start(&pit, obj->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
							(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
							ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);

									script_object_remref(obj);
									return ret;
								}

						}
					}
					BREAKPRET;
				}
				iterator_stop(&pit);
			}


		}

		script_object_remref(obj);

	} else if (room) {
		ROOM_INDEX_DATA *source;

		if(room->source) {
			source = room->source;
			uid[0] = room->id[0];
			uid[1] = room->id[1];
		} else {
			source = room;
		}

		script_room_addref(room);

		// Check for tokens FIRST
		iterator_start(&tit, room->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
							(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
							ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&tit);
								iterator_stop(&pit);

								script_token_remref(token);
								script_room_remref(room);
								return ret;
							}
						}
					}
				}
				iterator_stop(&pit);
				script_token_remref(token);

			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, source->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,type)) {
					if ( (prg->numeric && match_equal(prg->trig_number, vnum)) ||
						(!prg->numeric && match_target_name(prg->trig_phrase, name)) ) {
						ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
							if( ret != PRET_NOSCRIPT) {
								iterator_stop(&pit);

								script_room_remref(room);
								return ret;
							}

					}
				}
			}
			iterator_stop(&pit);


			// Check for tokens FIRST
			iterator_start(&tit, room->ltokens);
			while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
				if( token->pIndexData->progs ) {
					script_token_addref(token);
					script_destructed = FALSE;
					iterator_start(&pit, token->pIndexData->progs[slot]);
					while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
						if (is_trigger_type(prg->trig_type,type)) {
							if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
								(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
								ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT ) {
									iterator_stop(&tit);
									iterator_stop(&pit);

									script_token_remref(token);
									script_room_remref(room);
									return ret;
								}
							}
						}
					}
					iterator_stop(&pit);
					script_token_remref(token);

				}
			}
			iterator_stop(&tit);

			if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
				script_destructed = FALSE;
				iterator_start(&pit, source->progs->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,type)) {
						if ( (prg->numeric && match_equal(prg->trig_number, 0)) ||
							(!prg->numeric && match_exact_name(prg->trig_phrase, "*")) ) {
							ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,register1,register2,register3,register4,register5);
								if( ret != PRET_NOSCRIPT) {
									iterator_stop(&pit);

									script_room_remref(room);
									return ret;
								}

						}
					}
				}
				iterator_stop(&pit);

			}


		}
		script_room_remref(room);

	} else
		bug("test_vnumname_trigger: no program type for trigger %d.", type);

	PRETURN;
}



int p_give_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
			CHAR_DATA *ch, OBJ_DATA *dropped, int type, int register1, int register2, int register3, int register4, int register5)
{
	return test_vnumname_trigger(dropped->name, dropped->pIndexData->vnum, type,
					mob, obj, room, ch, NULL, NULL, dropped, NULL, NULL, register1, register2, register3, register4, register5);
}


int p_use_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, int register1, int register2, int register3, int register4, int register5)
{
	if (obj == NULL) {
		bug("p_use_trigger: received null obj!", 0);
		return PRET_NOSCRIPT;
	}

	if (!obj_room(obj)) return PRET_NOSCRIPT;


	if (type == TRIG_PUSH || type == TRIG_TURN || type == TRIG_PULL || type == TRIG_USE)
		return test_number_trigger(0, 0, match_percent, type, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);

	return PRET_NOSCRIPT;
}

int p_use_on_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, char *argument, int register1, int register2, int register3, int register4, int register5)
{
	if (obj == NULL) {
		bug("p_use_on_trigger: received null obj!", 0);
		return PRET_NOSCRIPT;
	}

	if (!obj_room(obj)) return PRET_NOSCRIPT;

	if (type == TRIG_PUSH_ON || type == TRIG_TURN_ON || type == TRIG_PULL_ON)
		return test_string_trigger(argument, "*", match_substr, type, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);

	return PRET_NOSCRIPT;
}

int p_use_with_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, OBJ_DATA *obj1, OBJ_DATA *obj2, CHAR_DATA *victim, CHAR_DATA *victim2, int register1, int register2, int register3, int register4, int register5)
{
	if (obj == NULL) {
		bug("p_use_with_trigger: received null obj!", 0);
		return PRET_NOSCRIPT;
	}

	if (!obj_room(obj)) return PRET_NOSCRIPT;

	if (type == TRIG_USEWITH)
		return test_number_trigger(0, 0, match_percent, type, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, victim, victim2, obj1, obj2, NULL, NULL, register1, register2, register3, register4, register5);

	return PRET_NOSCRIPT;
}


int p_greet_trigger(CHAR_DATA *ch, int type, int register1, int register2, int register3, int register4, int register5)
{
	return p_location_trigger(ch, ch ? ch->in_room : NULL, 0, 0, match_percent, type, TRIG_GREET, TRIG_GRALL,register1,register2,register3,register4,register5);
}


int p_hprct_trigger(CHAR_DATA *mob, CHAR_DATA *ch, int register1, int register2, int register3, int register4, int register5)
{
	int hit = (100 * mob->hit / mob->max_hit);

	return test_number_trigger(hit, hit, match_lt, TRIG_HPCNT, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
}

int p_emote_trigger(CHAR_DATA *ch, char *emote, int register1, int register2, int register3, int register4, int register5)
{
	int ret_val = PRET_NOSCRIPT, ret;
	// Check tokens on player

	ret_val = test_string_trigger(emote, "*", match_exact_name, TRIG_EMOTE, ch, NULL, NULL, ch, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);


	if( ret_val == PRET_NOSCRIPT && ch->in_room != NULL) {
		CHAR_DATA *mob, *mob_next;

		// Check all mobs in the room
		for (mob = ch->in_room->people; mob != NULL; mob = mob_next) {
			mob_next = mob->next_in_room;

			if (!IS_NPC(mob) || mob->position == mob->pIndexData->default_pos)
			{
				ret = test_string_trigger(emote, "*", match_exact_name, TRIG_EMOTE, mob, NULL, NULL, ch, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);

				if( ret != PRET_NOSCRIPT ) {
					ret_val = ret;
					break;
				}
			}
		}
	}

	PRETURN;
}

int p_emoteat_trigger(CHAR_DATA *mob, CHAR_DATA *ch, char *emote, int register1, int register2, int register3, int register4, int register5)
{
	int trig = (mob != ch) ? TRIG_EMOTEAT : TRIG_EMOTESELF;
	return test_string_trigger(emote, "*", match_exact_name, trig, mob, NULL, NULL, ch, NULL, NULL, NULL, NULL, register1, register2, register3, register4, register5);
}

int script_login(CHAR_DATA *ch) // @@@NIB
{
	ITERATOR tit, oit, pit;
	TOKEN_DATA *token;
	OBJ_DATA *obj;
	PROG_LIST *prg;
	SCRIPT_DATA *script;
	unsigned long uid[2];
	//unsigned long ouid[2];
	int slot;
	int ret_val = PRET_NOSCRIPT, ret; // @@@NIB Default for a trigger loop is NO SCRIPT

	variable_dynamic_fix_mobile(ch);

	// Run the SYSTEM LOGIN ROOM SCRIPT
	script = get_script_index_wnum(rprog_wnum_player_init,PRG_RPROG);
	if(script) {
		script_force_execute = TRUE;
		script_security = SYSTEM_SCRIPT_SECURITY;
		execute_script(script, NULL, NULL, room_index_default, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL,TRIG_LOGIN,0,0,0,0,0);
		script_security = INIT_SCRIPT_SECURITY;
		script_force_execute = FALSE;
	}

	// Run the TRIG_LOGIN
	struct trigger_type *tt = get_trigger_type_bytype(TRIG_LOGIN);

	slot = tt ? tt->slot : TRIGSLOT_GENERAL;

	// Save the UID
	uid[0] = ch->id[0];
	uid[1] = ch->id[1];

	// Check for tokens FIRST
	iterator_start(&tit, ch->ltokens);
	while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
		if(token->pIndexData->progs) {
			script_token_addref(token);
			script_destructed = FALSE;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
					ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL,TRIG_LOGIN,0,0,0,0,0);
					SETPRET;
				}
			}
			script_token_remref(token);
			iterator_stop(&pit);
		}
	}
	iterator_stop(&tit);

	// Check objects SECOND
	iterator_start(&oit, ch->lcarrying);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
		iterator_start(&tit, obj->ltokens);
		while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if(token->pIndexData->progs) {
				script_destructed = FALSE;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL,NULL, NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0);
						SETPRET;
					}
				}
				iterator_stop(&pit);
			}
		}
		iterator_stop(&tit);

		if(obj->pIndexData->progs) {
			script_destructed = FALSE;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
					ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0);
					SETPRET;
				}
			}
			iterator_stop(&pit);
		}
	}
	iterator_stop(&oit);

	if(ret_val == PRET_NOSCRIPT && IS_VALID(ch) && ch->id[0] == uid[0] && ch->id[0] == uid[1] && IS_NPC(ch) && ch->pIndexData->progs) {
		script_destructed = FALSE;
		iterator_start(&pit, ch->pIndexData->progs[slot]);
		while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
			if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number &&
				((ret = execute_script(prg->script, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0)) != PRET_NOSCRIPT)) {
				ret_val = ret;
				break;
			}
		}
		iterator_stop(&pit);
	}

	PRETURN; // @@@NIB
}

void do_ifchecks( CHAR_DATA *ch, char *argument)
{
	char buf[MIL]/*, *pbuf*/;
	BUFFER *buffer;
	int i,j;

	if(!ch->lines) {
		send_to_char("Viewing this with paging off is not permitted due to the number of ifchecks.\n\r",ch);
		return;
	}

	buffer = new_buf();
	if(!buffer) {
		send_to_char("WTF?! Couldn't create the buffer!\n\r",ch);
		return;
	}

	add_buf(buffer,"{WIf-Checks:{x\n\r");
	add_buf(buffer,"{D========================================================================={x\n\r");
	add_buf(buffer,"{WNum  {D| {WName                {D| {W              Types               {D| {WValue {D|{x\n\r");
	add_buf(buffer,"{D-------------------------------------------------------------------------{x\n\r");

	for(i=0,j=0;ifcheck_table[i].name;i++) if(ifcheck_table[i].name[0] != '_' && (!*argument || is_name(argument,ifcheck_table[i].name))) {
		sprintf(buf,"{W%4d{D)  {Y%-20.20s %s %s %s %s %s %s %s   %s{x\n\r",++j,
			ifcheck_table[i].name,
			((ifcheck_table[i].type & IFC_M) ? "{Gmob" : "   "),
			((ifcheck_table[i].type & IFC_O) ? "{Gobj" : "   "),
			((ifcheck_table[i].type & IFC_R) ? "{Groom" : "    "),
			((ifcheck_table[i].type & IFC_T) ? "{Gtoken" : "     "),
			((ifcheck_table[i].type & IFC_A) ? "{Garea" : "    "),
			((ifcheck_table[i].type & IFC_I) ? "{Ginst" : "    "),
			((ifcheck_table[i].type & IFC_D) ? "{Gdung" : "    "),
			(ifcheck_table[i].numeric ? "{B NUM " : "{R T/F "));
		add_buf(buffer,buf);
	}
	add_buf(buffer,"{D========================================================================={x\n\r");

//	pbuf = buf_string(buffer);
//	sprintf(buf,"pbuf = '%.15s{x', %d\n\r", pbuf, strlen(pbuf));

	if(j > 0)
		page_to_char(buf_string(buffer), ch);
//		send_to_char(buf,ch);
	else
		send_to_char("No ifchecks match that name pattern.\n\r",ch);
	free_buf(buffer);
	return;
}


char *get_script_prompt_string(CHAR_DATA *ch, char *key)
{
	STRING_VECTOR *v;
	if(IS_NPC(ch) || !ch->pcdata->script_prompts) return "";

	v = string_vector_find(ch->pcdata->script_prompts,key);
	return v ? v->string : "";
}


#if 0
// Returns TRUE if the spell got through.
// Used for token scripts
bool script_spell_deflection(CHAR_DATA *ch, CHAR_DATA *victim, TOKEN_DATA *token, SCRIPT_DATA *script, int mana)
{
	CHAR_DATA *rch = NULL;
	AFFECT_DATA *af;
	int attempts;
	int lev;
	int type;

	if (!IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION))
		return TRUE;

	act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	// Find spell deflection
	for (af = victim->affected; af; af = af->next) {
		if (af->skill == gsk_spell_deflection)
		break;
	}

	if (!af) return TRUE;

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev) {
		if (ch) {
			if (ch == victim)
				send_to_char("Your spell gets through your protective crimson aura!\n\r", ch);
			else {
				act("Your spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n's spell gets through your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$n's spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		}

		return TRUE;
	}

	type = token->pIndexData->value[TOKVAL_SPELL_TARGET];
	/* it bounces to a random person */
	if (type != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++) {
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch && rch == ch) || rch == victim ||
				((type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF) && ch && is_safe(ch, rch, FALSE))) {
				rch = NULL;
				continue;
			}
		}

	// Loses potency with time
	af->level -= 10;
	if (af->level <= 0) {
		send_to_char("{MThe crimson aura around you vanishes.{x\n\r", victim);
		act("{MThe crimson aura around $n vanishes.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		affect_remove(victim, af);
		return TRUE;
	}

	if (rch) {
		if (ch) {
			act("{YYour spell bounces off onto $N!{x",  ch, rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$n's spell bounces off onto $N!{x",  ch, rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}

		token->value[3] = ch ? ch->tot_level : af->level;

		execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch ? ch : rch, NULL, NULL, rch, NULL,NULL, NULL,"deflection",NULL,TRIG_NONE,0,0,0,0,0);
	} else if (ch) {
		act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n's spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	return FALSE;
}
#endif

#if 0
void token_skill_improve( CHAR_DATA *ch, TOKEN_DATA *token, bool success, int multiplier )
{
	int chance, per;
	char buf[100];
	int rating, max_rating, diff;

	if (IS_NPC(ch))
		return;

	if (IS_SOCIAL(ch))
		return;

	rating = token->value[TOKVAL_SPELL_RATING];
	max_rating = token->pIndexData->value[TOKVAL_SPELL_RATING] * 100;
	diff = token->pIndexData->value[TOKVAL_SPELL_DIFFICULTY];
	if (diff < 1) diff = 1;

	if(!max_rating) max_rating = 100;

	if(rating < 1 || rating >= max_rating)
		return;

	// check to see if the character has a chance to learn
	chance      = 10 * int_app[get_curr_stat(ch, STAT_INT)].learn;
	multiplier  = UMAX(multiplier,1);
	chance     /= (multiplier * diff * 4);
	chance     += ch->level;

	if (number_range(1,1000) > chance)
		return;

	per = 100 * rating / max_rating;

	// now that the character has a CHANCE to learn, see if they really have
	if (success) {
		chance = URANGE(2, 100 - per, 25);
		if (number_percent() < chance) {
			sprintf(buf,"{WYou have become better at %s!{x\n\r", token->name);
			send_to_char(buf,ch);
			token->value[TOKVAL_SPELL_RATING]++;
			gain_exp(ch, 2 * diff);
		}
	} else {
		chance = URANGE(5, per/2, 30);
		if (number_percent() < chance) {
			sprintf(buf, "{WYou learn from your mistakes, and your %s skill improves.{x\n\r", token->name);
			send_to_char(buf, ch);
			token->value[TOKVAL_SPELL_RATING] += number_range(1,3);
			if(token->value[TOKVAL_SPELL_RATING] >= max_rating)
				token->value[TOKVAL_SPELL_RATING] = max_rating;
			gain_exp(ch,2 * diff);
		}
	}
}
#endif

SCRIPT_VARINFO *script_get_prior(SCRIPT_VARINFO *info)
{
	return ((info && info->block && info->block->next) ? &(info->block->next->info) : NULL);
}

bool interrupt_script( CHAR_DATA *ch, bool silent )
{
	bool ret = FALSE;

	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_INTERRUPT, silent?"silent":NULL,0,0,0,0,0))
		ret = TRUE;

	if( ch->script_wait > 0) {
		script_end_failure(ch, !silent);
		ret = TRUE;
	}

	return ret;
}


void script_varseton(SCRIPT_VARINFO *info, ppVARIABLE vars, char *argument, SCRIPT_PARAM *arg)
{
	char buf[MIL], name[MIL], *rest, *str = NULL;
	CHAR_DATA *vch = NULL, *mobs = NULL, *viewer = NULL;
	OBJ_DATA *obj = NULL, *objs = NULL;
	TOKEN_DATA *token = NULL, *tokens = NULL;
	ROOM_INDEX_DATA *here = NULL;
	EXIT_DATA *ex = NULL;
	LLIST *blist;
	ITERATOR it;
	int vnum = 0, i, idx;
	unsigned long id1/*, id2*/;

	if(!info) return;

	if(info->mob) here = info->mob->in_room;
	else if(info->obj) here = obj_room(info->obj);
	else if(info->room) here = info->room;
	else if(info->token) here = token_room(info->token);

//	if(!viewer && !here) return;

	if(!vars) return;

	// Get name
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	if( arg->type != ENT_STRING ) return;

	strcpy(name, arg->d.str);
	if(!name[0]) return;

	// Get type
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	if( arg->type != ENT_STRING ) return;

	strncpy(buf, arg->d.str, MIL-1);
	if(!buf[0]) return;

	// Appends a fully escaped string to the end of a variable along with an EOL.
	// Format: APPENDLINE[ <string>]
	if(!str_cmp(buf,"appendline"))
	{
		// Special handling to allow "varset <name> appendline" to put a line at the end
		BUFFER *buffer = new_buf();
		expand_string(info,argument,buffer);
		add_buf(buffer,"\n\r");

		variables_append_string(vars,name,buf_string(buffer));
		free_buf(buffer);
		return;
	}

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	// Saves a boolean
	// Format: BOOL <boolean>
	// Format: BOOL <number>
	// Format: BOOL <numerical string>
	if(!str_cmp(buf,"bool")) {
		switch(arg->type) {
		case ENT_BOOLEAN: variables_set_boolean(vars,name,arg->d.boolean); break;
		case ENT_NUMBER: variables_set_boolean(vars,name,(arg->d.num != 0)); break;
		case ENT_STRING:
			if(is_number(arg->d.str))
				variables_set_boolean(vars,name,(atoi(arg->d.str) != 0));
			else if(!str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "on"))
				variables_set_boolean(vars,name,TRUE);
			else if(!str_cmp(arg->d.str, "false") || !str_cmp(arg->d.str, "no") || !str_cmp(arg->d.str, "off"))
				variables_set_boolean(vars,name,FALSE);

			break;

		}

	// Saves a number
	// Format: INTEGER <number>
	// Format: INTEGER <numerical string>
	} else if(!str_cmp(buf,"integer") || !str_cmp(buf,"number")) {
		switch(arg->type) {
		case ENT_NUMBER: variables_set_integer(vars,name,arg->d.num); break;
		case ENT_STRING:
			if(is_number(arg->d.str))
				variables_set_integer(vars,name,atoi(arg->d.str));
			break;
		}

	// Decrements the variable, if it's a NUMBER by the specified decrement
	// Format: DEC <step>
	// Format: DECREMENT <step>
	} else if(!str_cmp(buf,"dec") || !str_cmp(buf,"decrement")) {
		pVARIABLE var = variable_get(*vars, name);
		if(!var) return;

		if(var->type != VAR_INTEGER) return;	// Must be a number!

		switch(arg->type) {
		case ENT_NUMBER: var->_.i -= arg->d.num; break;
		case ENT_STRING:
			if(is_number(arg->d.str))
				var->_.i -= atoi(arg->d.str);
			break;
		}

	// Increments the variable, if it's a NUMBER by the specified decrement
	// Format: INC <step>
	// Format: INCREMENT <step>
	} else if(!str_cmp(buf,"inc") || !str_cmp(buf,"increment")) {
		pVARIABLE var = variable_get(*vars, name);
		if(!var) return;

		if(var->type != VAR_INTEGER) return;	// Must be a number!

		switch(arg->type) {
		case ENT_NUMBER: var->_.i += arg->d.num; break;
		case ENT_STRING:
			if(is_number(arg->d.str))
				var->_.i += atoi(arg->d.str);
			break;
		}
	// Format: STRING <string>[ <word index>]
	} else if(!str_cmp(buf,"string")) {
		char tmp[MSL],*p;

		switch(arg->type) {
		case ENT_NUMBER:
			sprintf(tmp,"%d",arg->d.num);
			break;
		case ENT_STRING:
			strcpy(tmp,arg->d.str);
			break;
		default:return;
		}

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		switch(arg->type) {
		case ENT_NONE:
			variables_set_string(vars,name,tmp,FALSE);
			break;

		case ENT_NUMBER:
			p = tmp;
			for(i=0;i<arg->d.num && p && *p;i++)
				p = one_argument(p,buf);
			if(arg->d.num > 0 && i == arg->d.num)
				variables_set_string(vars,name,buf,FALSE);
			break;
		}

	// Format: APPEND <string>[ <word index>]
	} else if(!str_cmp(buf,"append")) {
		char tmp[MSL], *p;

		switch(arg->type) {
		case ENT_NUMBER:	sprintf(tmp,"%d",arg->d.num); break;
		case ENT_STRING:	strcpy(tmp,arg->d.str); 	break;
		default:return;
		}

		if(!(rest = expand_argument(info,rest,arg))) return;

		switch(arg->type) {
		case ENT_NONE:		variables_append_string(vars,name,tmp); break;
		case ENT_NUMBER:
			p = tmp;
			for(i=0;i<arg->d.num && p && *p;i++) p = one_argument(p,buf);

			if(arg->d.num > 0 && (i == arg->d.num)) variables_append_string(vars,name,buf);
			break;
		}

	// Format: EXPAND <string>
	} else if(!str_cmp(buf,"EXPAND")) {

		if( arg->type != ENT_STRING ) return;

		int length;
		char *comp_str = compile_string(arg->d.str,IFC_ANY,&length,FALSE);	// TODO: Check whether the doquotes needs to be TRUE
		if( !comp_str ) return;

		BUFFER *buffer = new_buf();
		expand_string(info, comp_str, buffer);
		variables_set_string(vars,name,buf_string(buffer),FALSE);

		free_buf(buffer);
		free_string(comp_str);

	// Format: ARGREMOVE <word index>
	// Format: ARGREMOVE <word to remove>
	} else if(!str_cmp(buf,"ARGREMOVE")) {
		switch(arg->type) {
		case ENT_NUMBER:

			variables_argremove_string_index(vars,name,arg->d.num);
			break;
		case ENT_STRING:

			variables_argremove_string_phrase(vars,name,arg->d.str);
			break;
		}

	// Format: STRFORMAT
	// TO-DO: Add "STRFORMAT[ <width>]"
	} else if(!str_cmp(buf,"strformat")) {
		variables_format_string(vars,name);

	// Format: STRFORMATP
	// Format: PARAFORMAT
	// TO-DO: Add "STRFORMATP[ <width>]"
	} else if(!str_cmp(buf,"strformatp") || !str_cmp(buf,"paraformat")) {
		variables_format_paragraph(vars,name);

	// Format: STRREPLACE <OLD> <NEW>
	} else if(!str_cmp(buf,"strreplace")) {
		pVARIABLE var = variable_get(*vars, name);
		if(!var) return;

		if(var->type != VAR_STRING && var->type != VAR_STRING_S) return;

		char o[MSL];
		char n[MSL];
		char *rep;

		if( arg->type != ENT_STRING ) return;
		strcpy(o, arg->d.str);

		if(!(rest = expand_argument(info,rest,arg))) return;

		if( arg->type != ENT_STRING ) return;
		strcpy(n, arg->d.str);

		rep = string_replace_static(var->_.s, o, n);
		if( rep == NULL ) return;	// An error, ret COULD be empty after the replace, so IS_NULLSTR is not the right test

		variables_set_string(vars,name,rep,FALSE);

	// Copies an extra description
	// Format: ED <OBJECT or ROOM> <keyword>
	} else if(!str_cmp(buf,"ed")) {
		// ed $<object|room> <keyword>
		char *p = NULL;
		EXTRA_DESCR_DATA *desc;
		ROOM_INDEX_DATA *edroom = NULL;

		switch(arg->type) {
		case ENT_OBJECT:
			if( arg->d.obj->extra_descr )
				desc = arg->d.obj->extra_descr;
			else
				desc = arg->d.obj->pIndexData->extra_descr;

			if( !arg->d.obj->carried_by && !arg->d.obj->in_obj && !arg->d.obj->locker && !arg->d.obj->in_mail )
				edroom = get_environment(arg->d.obj->in_room);
			break;
		case ENT_ROOM:
			desc = arg->d.room->extra_descr;
			edroom = get_environment(arg->d.room);
			break;
		default:return;
		}

		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		EXTRA_DESCR_DATA *ed = get_extra_descr(buf_string(buffer), desc);

		if( ed )
		{
			p = ed->description;

			if( !p && edroom )
			{
				p = edroom->description;
			}
		}

		variables_set_string(info->var,name,(p ? p : ""),FALSE);

		free_buf(buffer);

	// Format: ROOM <WNUM> - room vnum
	// Format: ROOM <ROOM> - explicit room
	// Format: ROOM <EXIT> - gets the destination of the exit
	// Format: ROOM <ROOM-LIST> - first room from the list
	// Format: ROOM <ROOM-LIST> <INDEX> - Nth room from the list
	// Format: ROOM <ROOM-LIST> FIRST - first room from the list
	// Format: ROOM <ROOM-LIST> LAST - last room from the list
	// Format: ROOM <ROOM-LIST> RANDOM - random valid room from the list
	} else if(!str_cmp(buf,"room")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_room(vars,name,get_room_index_wnum(arg->d.wnum));
			break;
		case ENT_ROOM:
			variables_set_room(vars,name,arg->d.room);
			break;
		case ENT_EXIT:
			here = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL;
			variables_set_room(vars,name,here);
			break;
		case ENT_BLLIST_ROOM:
			blist = arg->d.blist;
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			idx = 0;
			switch(arg->type) {
			default: break;
			case ENT_STRING:
				if( !str_cmp(arg->d.str, "first"))
					idx = 0;
				else if( !str_cmp(arg->d.str, "last"))
					idx = list_size(blist)-1;
				else if( !str_cmp(arg->d.str, "random"))
					idx = number_range(0, list_size(blist)-1);
				break;

			case ENT_NUMBER:
				idx = arg->d.num;
				break;
			}

			{
				LLIST_ROOM_DATA *lroom;
				iterator_start_nth(&it, blist, idx);
				while((lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) && !lroom->room);
				iterator_stop(&it);

				if(lroom && lroom->room)
					variables_set_room(vars,name,lroom->room);
			}
			break;
		case ENT_PLLIST_ROOM:
			blist = arg->d.blist;
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			idx = 0;
			switch(arg->type) {
			default: break;
			case ENT_STRING:
				if( !str_cmp(arg->d.str, "first"))
					idx = 0;
				else if( !str_cmp(arg->d.str, "last"))
					idx = list_size(blist)-1;
				else if( !str_cmp(arg->d.str, "random"))
					idx = number_range(0, list_size(blist)-1);
				break;

			case ENT_NUMBER:
				idx = arg->d.num;
				break;
			}

			variables_set_room(vars, name, list_nthdata(blist, idx));
			break;
		}

	// Find the highest room at that coordinate
	// Format: HIGHROOM <MAP UID> <X> <Y> <Z> <GROUND>
	} else if(!str_cmp(buf,"highroom")) {

		// Format: highroom <wilds-uid> <x> <y> <z> <ground>
		WILDS_DATA *wilds;
		int x, y, z;
		bool ground;

		if(arg->type == ENT_NUMBER) {
			wilds = get_wilds_from_uid(NULL,arg->d.num);
			if(wilds) {
				if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
				x = arg->d.num;
				if(x < 0 || x >= wilds->map_size_x) return;

				if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
				y = arg->d.num;
				if(y < 0 || x >= wilds->map_size_y) return;

				if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
				z = arg->d.num;

				if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;
				ground = !str_cmp(arg->d.str,"ground") || !str_cmp(arg->d.str,"true");

				variables_set_room(info->var,name,wilds_seek_down(wilds, x, y, z, ground));
			}
		}

	// Format: EXIT <STRING> - finds the exit at the given direction in the current room
	// Format: EXIT <ROOM> <STRING> - same as EXIT <STRING> but at the given room
	// Format: EXIT <EXIT> - explicit exit
	} else if(!str_cmp(buf,"exit")) {
		switch(arg->type) {
		case ENT_ROOM:
			here = arg->d.room;
			if(!here || !expand_argument(info,rest,arg))
				return;
			if(arg->type != ENT_STRING) return;
		case ENT_STRING:
			vnum = get_num_dir(arg->d.str);
			if(vnum < 0) {
				if(!str_cmp(arg->d.str,"random"))
					vnum = number_range(0,MAX_DIR-1);
				else if(!str_cmp(arg->d.str,"exists")) {
					for(vnum = number_range(0,MAX_DIR-1), i = 0; i < MAX_DIR && !here->exit[vnum]; i++, vnum = (vnum+1)%MAX_DIR);

					if(!here->exit[vnum]) vnum = -1;
				} else if(!str_cmp(arg->d.str,"open")) {
					for(vnum = number_range(0,MAX_DIR-1), i = 0; i < MAX_DIR && !here->exit[vnum]; i++, vnum = (vnum+1)%MAX_DIR);

					if(!here->exit[vnum] || IS_SET(here->exit[vnum]->exit_info,EX_CLOSED)) vnum = -1;
				}

				if(vnum < 0)
					return;
			}
			ex = here->exit[vnum];
			break;
		case ENT_EXIT:
			ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
			break;
		}
		variables_set_exit(vars,name,ex);

	// Format: MOBILE <ROOM WNUM or ROOM or MOBLIST> <WNUM or NAME>[ <VIEWER>]
	// Format: MOBILE WNUM <WNUM>[ <VIEWER>]
	// Format: MOBILE NAME|WORLD <NAME>[ <VIEWER>]
	// Format: MOBILE HERE <NAME>[ <VIEWER>]
	// Format: MOBILE <MOBILE>
	// Format: MOBILE <MOBINDEX>
	} else if(!str_cmp(buf,"mobile")) {
		WNUM wnum = wnum_zero;
		if( arg->type == ENT_BLLIST_MOB )
		{
			LLIST *blist = arg->d.blist;
			BUFFER *buffer = NULL;
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_WIDEVNUM )
			{
				wnum = arg->d.wnum;
				str = NULL;
			}
			else if( arg->type == ENT_STRING )
			{
				if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
				{
					str = NULL;
				}
				else
				{
					str = arg->d.str;
				}
			}
			else
				return;

			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			vch = script_get_char_blist(info, blist, viewer, FALSE, wnum, str);
			variables_set_mobile(vars,name,vch);
			if( buffer )
				free_buf(buffer);
			return;
		}

		switch(arg->type) {
		case ENT_WIDEVNUM:
			here = get_room_index_wnum(arg->d.wnum);
			mobs = here ? here->people : NULL;
			break;
		case ENT_STRING:
			if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			{
				here = get_room_index_wnum(wnum);
				mobs = here ? here->people : NULL;
			}
			else if(!str_cmp(arg->d.str, "name")||!str_cmp(arg->d.str, "world"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) return;
				vch = get_char_world(NULL,arg->d.str);
			}
			else if(!str_cmp(arg->d.str, "here"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) return;
				vch = get_char_room(NULL,here,arg->d.str);
			}
			else if(!str_cmp(arg->d.str, "wnum"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM) return;

				MOB_INDEX_DATA *mob_index = get_mob_index_wnum(arg->d.wnum);
				if(!mob_index) return;

				vch = get_char_world_index(NULL, mob_index);
			}
			break;
		case ENT_MOBINDEX:
			if (!arg->d.mobindex) return;

			vch = get_char_world_index(NULL, arg->d.mobindex);
			break;
		case ENT_MOBILE:
			vch = arg->d.mob;
			break;
		case ENT_OLLIST_MOB:
			mobs = arg->d.list.ptr.mob ? *arg->d.list.ptr.mob : NULL;
			break;
		case ENT_ROOM:
			mobs = arg->d.room ? arg->d.room->people : NULL;
			break;
		default: return;
		}

		if(mobs) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			BUFFER *buffer = NULL;
			if(arg->type == ENT_WIDEVNUM)
				wnum = arg->d.wnum;
			else if(arg->type == ENT_STRING) {
				if(is_number(arg->d.str))
					vnum = atoi(arg->d.str);
				else
				{
					str = arg->d.str;

				}
			} else
				return;

			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			vch = script_get_char_list(info, mobs, viewer, FALSE, wnum, str);
			if( buffer )
				free_buf(buffer);
		}
		variables_set_mobile(vars,name,vch);

	// Format: PLAYER <ROOM WNUM or ROOM or MOBLIST> <NAME>[ <VIEWER>]
	// Format: PLAYER <NAME>
	// Format: PLAYER <PLAYER>
	} else if(!str_cmp(buf,"player")) {
		if( arg->type == ENT_BLLIST_MOB )
		{
			LLIST *blist = arg->d.blist;
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type != ENT_STRING )
				return;

			BUFFER *buffer = NULL;
			str = arg->d.str;
			viewer = NULL;
			if( *rest )
			{
				buffer = arg->buffer;
				arg->buffer = new_buf();

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			vch = script_get_char_blist(info, blist, viewer, TRUE, wnum_zero, str);
			variables_set_mobile(vars,name,vch);
			return;
		}


		switch(arg->type) {
		case ENT_STRING:
			vch = get_player(arg->d.str);
			break;
		case ENT_MOBILE:
			vch = arg->d.mob && !IS_NPC(arg->d.mob) ? arg->d.mob : NULL;
			break;
		case ENT_OLLIST_MOB:
			mobs = arg->d.list.ptr.mob ? *arg->d.list.ptr.mob : NULL;
			if(mobs) {
				if(!expand_argument(info,rest,arg))
					return;
				if(arg->type == ENT_STRING && !is_number(arg->d.str))
					str = arg->d.str;
				else
					mobs = NULL;
			}
			break;
		default: return;
		}

		if(mobs)
		{
			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			vch = script_get_char_list(info, mobs, viewer, TRUE, wnum_zero, str);

			if( buffer )
				free_buf(buffer);
		}
		variables_set_mobile(vars,name,vch);

	// Format: OBJECT <ROOM WNUM or ROOM or MOBILE or OBJECT or OBJLIST> <WNUM or NAME>
	// Format: OBJECT HERE <NAME>
	// Format: OBJECT WORLD <NAME>
	// Format: OBJECT WNUM <WNUM>
	// Format: OBJECT <OBJECT>
	// Format: OBJECT <OBJINDEX>
	} else if(!str_cmp(buf,"object")) {
		WNUM wnum = wnum_zero;
		if( arg->type == ENT_BLLIST_OBJ)
		{
			LLIST *blist = arg->d.blist;
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_WIDEVNUM )
			{
				wnum = arg->d.wnum;
				str = NULL;
			}
			else if( arg->type == ENT_STRING )
			{
				if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
				{
					str = NULL;
				}
				else
				{
					wnum = wnum_zero;
					str = arg->d.str;
				}
			}
			else
				return;


			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			obj = script_get_obj_blist(info, blist, viewer, wnum, str);
			variables_set_object(vars,name,obj);

			if( buffer )
				free_buf(buffer);
			return;
		}


		switch(arg->type) {
		case ENT_WIDEVNUM:
			here = get_room_index_wnum(arg->d.wnum);
			objs = here ? here->contents : NULL;
			break;
		case ENT_STRING:
			if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			{
				here = get_room_index_wnum(wnum);
				objs = here ? here->contents : NULL;
			}
			else if(!str_cmp(arg->d.str, "here"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) return;

				obj = get_obj_here(NULL,here,arg->d.str);
			}
			else if(!str_cmp(arg->d.str, "name")||!str_cmp(arg->d.str, "world"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) return;

				obj = get_obj_world(NULL, arg->d.str);
			}
			else if(!str_cmp(arg->d.str, "wnum"))
			{
				if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM) return;

				OBJ_INDEX_DATA *obj_index = get_obj_index_wnum(arg->d.wnum);

				obj = get_obj_world_index(NULL, obj_index, FALSE);
			}
			break;
		case ENT_OBJINDEX:
			obj = get_obj_world_index(NULL, arg->d.objindex, FALSE);
			break;
		case ENT_OBJECT:
			obj = arg->d.obj;
			break;
		case ENT_MOBILE:
			objs = arg->d.mob ? arg->d.mob->carrying : NULL;
			break;
		case ENT_OLLIST_OBJ:
			objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
			break;
		case ENT_ROOM:
			objs = arg->d.room ? arg->d.room->contents : NULL;
			break;
		default: return;
		}

		if(objs) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;
			if(arg->type == ENT_WIDEVNUM)
				wnum = arg->d.wnum;
			else if(arg->type == ENT_STRING) {
				if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
					str = arg->d.str;
			} else
				return;

			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			obj = script_get_obj_list(info, objs, viewer, 0, wnum, str);

			if( buffer )
				free_buf(buffer);
		}
		variables_set_object(vars,name,obj);

	// Format: CARRY <WNUM or NAME>
	// Format: CARRY <MOBILE> <WNUM or NAME>
	// Format: CARRY <OBJLIST> <WNUM or NAME>
	} else if(!str_cmp(buf,"carry")) {
		WNUM wnum = wnum_zero;
		switch(arg->type) {
		case ENT_WIDEVNUM:
			wnum = arg->d.wnum;
			objs = info->mob ? info->mob->carrying : NULL;
			break;
		case ENT_STRING:
			if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
				str = arg->d.str;
			objs = info->mob ? info->mob->carrying : NULL;
			break;
		case ENT_MOBILE:
			objs = arg->d.mob ? arg->d.mob->carrying : NULL;
			break;
		case ENT_OLLIST_OBJ:
			objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
			break;
		default: return;
		}

		if(objs)
		{
			if(arg->type != ENT_WIDEVNUM && arg->type != ENT_STRING)
			{
				if(!(rest = expand_argument(info,rest,arg)))
					return;
				if(arg->type == ENT_WIDEVNUM)
					wnum = arg->d.wnum;
				else if(arg->type == ENT_STRING) {
					if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
						str = arg->d.str;
				} else
					return;
			}

			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			obj = script_get_obj_list(info, objs, viewer, 2, wnum, str);

			if( buffer )
				free_buf(buffer);
		}
		variables_set_object(vars,name,obj);

	// Format: WORN <WNUM or NAME>
	// Format: WORN <MOBILE or OBJLIST> <WNUM or NAME>
	} else if(!str_cmp(buf,"worn")) {
		WNUM wnum = wnum_zero;
		switch(arg->type) {
		case ENT_WIDEVNUM:
			wnum = arg->d.wnum;
			objs = info->mob ? info->mob->carrying : NULL;
			break;
		case ENT_STRING:
			if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
				str = arg->d.str;
			objs = info->mob ? info->mob->carrying : NULL;
			break;
		case ENT_MOBILE:
			objs = arg->d.mob ? arg->d.mob->carrying : NULL;
			break;
		case ENT_OLLIST_OBJ:
			objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
			break;
		default: return;
		}

		if(objs)
		{
			if(arg->type != ENT_WIDEVNUM && arg->type != ENT_STRING)
			{
				if(!(rest = expand_argument(info,rest,arg)))
					return;
				if(arg->type == ENT_WIDEVNUM)
					wnum = arg->d.wnum;
				else if(arg->type == ENT_STRING) {
					if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
						str = arg->d.str;
				} else
					return;
			}

			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			obj = script_get_obj_list(info, objs, viewer, 1, wnum, str);

			if( buffer )
				free_buf(buffer);
		}
		variables_set_object(vars,name,obj);

	// Format: CONTENT <OBJECT or OBJLIST> <WNUM or NAME>
	} else if(!str_cmp(buf,"content")) {
		WNUM wnum;
		switch(arg->type) {
		case ENT_OLLIST_OBJ:
			objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
			break;
		case ENT_OBJECT:
			objs = arg->d.obj ? arg->d.obj->contains : NULL;
			break;
		default: return;
		}

		if(objs)
		{
			if(!(rest = expand_argument(info,rest,arg)))
				return;
			if(arg->type == ENT_WIDEVNUM)
				wnum = arg->d.wnum;
			else if(arg->type == ENT_STRING) {
				if(!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
					str = arg->d.str;
			} else
				return;

			BUFFER *buffer = NULL;
			viewer = NULL;
			if( *rest )
			{
				if( str )
				{
					buffer = arg->buffer;
					arg->buffer = new_buf();
				}

				if(!(rest = expand_argument(info,rest,arg)))
				{
					if( buffer )
						free_buf(buffer);
					return;
				}

				if( arg->type == ENT_MOBILE )
					viewer = arg->d.mob;
			}

			obj = script_get_obj_list(info, objs, viewer, 0, wnum, str);

			if( buffer )
				free_buf(buffer);
		}

		variables_set_object(vars,name,obj);

	// Format: TOKEN <MOBILE or OBJECT or ROOM or TOKLIST> <Pattern>
	// Format: TOKEN <TOKEN>
	} else if(!str_cmp(buf,"token")) {
		switch(arg->type) {
		case ENT_MOBILE:   tokens = arg->d.mob ? arg->d.mob->tokens : NULL; break;
		case ENT_OBJECT:   tokens = arg->d.obj ? arg->d.obj->tokens : NULL; break;
		case ENT_ROOM:     tokens = arg->d.room ? arg->d.room->tokens : NULL; break;
		case ENT_TOKEN:    token = arg->d.token; break;
		case ENT_OLLIST_TOK: tokens = arg->d.list.ptr.tok ? *arg->d.list.ptr.tok : NULL; break;
		default: return;
		}

		if(tokens) token = token_find_match(info,tokens, rest, arg);
		variables_set_token(vars,name,token);

	// Format: DICE <DICE>
	} else if(!str_cmp(buf,"DICE")) {
		switch(arg->type) {
		case ENT_DICE:   if(arg->d.dice) variables_set_dice(vars,name,arg->d.dice);
		default: return;
		}

	// VARIABLE MOBILE NAME
	// VARIABLE OBJECT NAME
	// VARIABLE ROOM NAME
	// VARIABLE TOKEN NAME
	} else if(!str_cmp(buf,"variable")) {
		pVARIABLE their_vars, their_var;
		switch(arg->type) {
		case ENT_MOBILE:   their_vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? arg->d.mob->progs->vars : NULL; break;
		case ENT_OBJECT:   their_vars = (arg->d.obj && arg->d.obj->progs) ? arg->d.obj->progs->vars : NULL; break;
		case ENT_ROOM:     their_vars = (arg->d.room && arg->d.room->progs) ? arg->d.room->progs->vars : NULL; break;
		case ENT_TOKEN:    their_vars = (arg->d.token && arg->d.token->progs) ? arg->d.token->progs->vars : NULL; break;
		default: return;
		}

		if(!their_vars) return;

		if(!expand_argument(info,rest,arg))
			return;

		if(arg->type != ENT_STRING) return;

		their_var = variable_get(their_vars, arg->d.str);

		variables_set_variable(vars,name,their_var);


	// Format: IDMOBILE <IDa> <IDb>
	} else if(!str_cmp(buf,"idmobile")) {
		switch(arg->type) {
		case ENT_NUMBER:
			id1 = arg->d.num;
			if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
				return;
			vch = idfind_mobile(id1,arg->d.num);
			break;
		default: return;
		}
		variables_set_mobile(vars,name,vch);

	// Format: IDOBJECT <IDa> <IDb>
	} else if(!str_cmp(buf,"idobject")) {
		switch(arg->type) {
		case ENT_NUMBER:
			id1 = arg->d.num;
			if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
				return;
			obj = idfind_object(id1,arg->d.num);
			break;
		default: return;
		}
		variables_set_object(vars,name,obj);

	// Format: IDPLAYER <IDa> <IDb>
	} else if(!str_cmp(buf,"idplayer")) {
		switch(arg->type) {
		case ENT_NUMBER:
			id1 = arg->d.num;
			if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
				return;
			vch = idfind_player(id1,arg->d.num);
			break;
		default: return;
		}
		variables_set_mobile(vars,name,vch);

	// Format: CROOM <ROOM WNUM> <IDa> <IDb>
	} else if(!str_cmp(buf,"croom")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			here = get_room_index_wnum(arg->d.wnum);
			if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
				return;
			id1 = arg->d.num;
			if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
				return;
			variables_set_room(vars,name,get_clone_room(here,id1,arg->d.num));
			break;
		default: return;
		}

	// Format: SKILL <NAME>
	} else if(!str_cmp(buf,"skill")) {
		switch(arg->type) {
		case ENT_STRING:
			variables_set_skill(vars,name,get_skill_data(arg->d.str));
			break;
		default: return;
		}

	// Format: SKILLINFO <MOBILE> <NAME>
	} else if(!str_cmp(buf,"skillinfo")) {
		switch(arg->type) {
		case ENT_MOBILE:
			vch = arg->d.mob;
			if(!expand_argument(info,rest,arg))
				return;

			if( arg->type == ENT_STRING )
			{
				SKILL_DATA *skill = get_skill_data(arg->d.str);
				if (!IS_VALID(skill)) return;

				SKILL_ENTRY *entry = skill_entry_findskill(vch->sorted_skills, skill);
				if (!entry) return;

				variables_set_skillinfo(vars,name,vch,skill, entry->token);
			}
			break;
		default: return;
		}

	// Format: SONG <NAME>
	} else if(!str_cmp(buf,"song")) {
		switch(arg->type) {
		case ENT_STRING:
			variables_set_song(vars,name,get_song_data(arg->d.str));
			break;
		default: return;
		}

	// Format: MOBINDEX <WIDEVNUM>
	//         MOBINDEX <MOBINDEX>
	} else if(!str_cmp(buf,"mobindex")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_mobindex(vars,name,get_mob_index_wnum(arg->d.wnum));
			break;
		case ENT_MOBINDEX:
			variables_set_mobindex(vars,name,arg->d.mobindex);
			break;
		default: return;
		}

	// Format: OBJINDEX <WIDEVNUM>
	//         OBJINDEX <OBJINDEX>
	} else if(!str_cmp(buf,"objindex")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_objindex(vars,name,get_obj_index_wnum(arg->d.wnum));
			break;
		case ENT_OBJINDEX:
			variables_set_objindex(vars,name,arg->d.objindex);
			break;
		default: return;
		}

	// Format: TOKENINDEX <WIDEVNUM>
	//         TOKENINDEX <TOKENINDEX>
	} else if(!str_cmp(buf,"tokenindex")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_tokenindex(vars,name,get_token_index_wnum(arg->d.wnum));
			break;
		case ENT_TOKENINDEX:
			variables_set_tokenindex(vars,name,arg->d.tokindex);
			break;
		default: return;
		}

	// Format: BLUEPRINT <WIDEVNUM>
	//         BLUEPRINT <BLUEPRINT>
	} else if(!str_cmp(buf,"blueprint")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_blueprint(vars,name,get_blueprint_wnum(arg->d.wnum));
			break;
		case ENT_BLUEPRINT:
			variables_set_blueprint(vars,name,arg->d.bp);
			break;
		default: return;
		}


	// Format: BPSECTION <WIDEVNUM>
	//         BPSECTION <BLUEPRINT SECTION>
	} else if(!str_cmp(buf,"bpsection")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_blueprint_section(vars,name,get_blueprint_section_wnum(arg->d.wnum));
			break;
		case ENT_BLUEPRINT_SECTION:
			variables_set_blueprint_section(vars,name,arg->d.bs);
			break;
		default: return;
		}

	// Format: DUNGEONINDEX <WIDEVNUM>
	//         DUNGEONINDEX <DUNGEONINDEX>
	} else if(!str_cmp(buf,"dungeonindex")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_dungeonindex(vars,name,get_dungeon_index_wnum(arg->d.wnum));
			break;
		case ENT_DUNGEONINDEX:
			variables_set_dungeonindex(vars,name,arg->d.dngindex);
			break;
		default: return;
		}

	// Format: SHIPINDEX <WIDEVNUM>
	//         SHIPINDEX <SHIPINDEX>
	} else if(!str_cmp(buf,"shipindex")) {
		switch(arg->type) {
		case ENT_WIDEVNUM:
			variables_set_shipindex(vars,name,get_ship_index_wnum(arg->d.wnum));
			break;
		case ENT_SHIPINDEX:
			variables_set_shipindex(vars,name,arg->d.shipindex);
			break;
		default: return;
		}

	// Format: FINDPATH <ROOM> <ROOM> <DEPTH> <IN-ZONE> <DOORS> - returns the EXIT entity
	} else if(!str_cmp(buf,"findpath")) {
		ROOM_INDEX_DATA *start_room = NULL, *end_room = NULL;
		int depth, in_zone, doors;
		int dir;

		switch(arg->type) {
		case ENT_WIDEVNUM:	start_room = get_room_index_wnum(arg->d.wnum); break;
		case ENT_ROOM:		start_room = arg->d.room; break;
		case ENT_EXIT:		start_room = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		}

		if(!start_room || !(rest = expand_argument(info,rest,arg)))
			return;

		switch(arg->type) {
		case ENT_WIDEVNUM:	end_room = get_room_index_wnum(arg->d.wnum); break;
		case ENT_ROOM:		end_room = arg->d.room; break;
		case ENT_EXIT:		end_room = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		}

		if(!end_room || !(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		depth = arg->d.num;

		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		in_zone = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "local");

		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		doors = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "doors");

		dir = find_path(start_room->area, start_room->vnum, end_room->area, end_room->vnum, NULL, (doors ? -depth : depth), in_zone);
		if( dir < 0 || dir >= MAX_DIR )
			variables_set_exit(vars,name,NULL);
		else
			variables_set_exit(vars,name,start_room->exit[dir]);

	// AREA uid
	// AREA name
	// AREA room
	// AREA area
	} else if(!str_cmp(buf,"area")) {
		AREA_DATA *area;

		switch(arg->type) {
		case ENT_NUMBER:	area = get_area_from_uid(arg->d.num); break;
		case ENT_STRING:	area = find_area(arg->d.str); break;
		case ENT_ROOM:		area = arg->d.room ? arg->d.room->area : NULL; break;
		case ENT_AREA:		area = arg->d.area; break;
		default:			area = NULL; break;
		}

		if( area )
			variables_set_area(vars,name,area);

	// Format: MOBLIST add <mobile>
	// Format: MOBLIST remove <index>|<mobile>
	// Format: MOBLIST clear
	} else if(!str_cmp(buf,"moblist")) {

		if( arg->type != ENT_STRING )
			return;


		// MOBLIST add <mobile>
		if( !str_cmp(arg->d.str, "add") ) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_MOBILE && IS_VALID(arg->d.mob) )
				variables_set_list_mob(vars,name,arg->d.mob,TRISTATE);

		// MOBLIST remove <index>|<mobile>
		} else if( !str_cmp(arg->d.str, "remove") ) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			pVARIABLE var = variable_get(*vars, name);

			if( !var || var->type != VAR_BLLIST_MOB || !IS_VALID(var->_.list) )
				return;

			if (arg->type == ENT_NUMBER)
				list_remnthlink(var->_.list, arg->d.num);
			else if (arg->type == ENT_MOBILE)
				list_remlink(var->_.list, arg->d.mob);

		// MOBLIST clear
		} else if( !str_cmp(arg->d.str, "clear") ) {
			pVARIABLE var = variable_get(*vars, name);

			if( !var || var->type != VAR_BLLIST_MOB || !IS_VALID(var->_.list) )
				return;

			list_clear(var->_.list);
		}

	// OBJLIST add <object>
	// OBJLIST remove <index>|<object>
	// OBJLIST clear
	} else if(!str_cmp(buf,"objlist")) {
		if( arg->type != ENT_STRING )
			return;


		// OBJLIST add <object>
		if( !str_cmp(arg->d.str, "add") ) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_OBJECT && IS_VALID(arg->d.obj) )
				variables_set_list_obj(vars,name,arg->d.obj,TRISTATE);

		// OBJLIST remove <index>|<mobile>
		} else if( !str_cmp(arg->d.str, "remove") ) {
			if(!(rest = expand_argument(info,rest,arg)))
				return;

			pVARIABLE var = variable_get(*vars, name);

			if( !var || var->type != VAR_BLLIST_OBJ || !IS_VALID(var->_.list) )
				return;

			if (arg->type == ENT_NUMBER)
				list_remnthlink(var->_.list, arg->d.num);
			else if (arg->type == ENT_OBJECT)
				list_remlink(var->_.list, arg->d.obj);

		// OBJLIST clear
		} else if( !str_cmp(arg->d.str, "clear") ) {
			pVARIABLE var = variable_get(*vars, name);

			if( !var || var->type != VAR_BLLIST_OBJ || !IS_VALID(var->_.list) )
				return;

			list_clear(var->_.list);
		}

	// RANDMOB <player> <continent>
	// RANDMOB <player> <area>
	} else if(!str_cmp(buf,"randmob")) {
		if( arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || IS_NPC(arg->d.mob) )
			return;

		vch = arg->d.mob;

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_STRING )
		{
			int continent = get_continent(arg->d.str);
			if( continent < 0 )
				continent = ANY_CONTINENT;

			vch = get_random_mob(vch, continent);
			if( vch != NULL )
				variables_set_mobile(vars,name,vch);
		}
		else if( arg->type == ENT_AREA )
		{
			vch = get_random_mob_area(vch, arg->d.area);
			if( vch != NULL )
				variables_set_mobile(vars,name,vch);
		}

	// RANDROOM <player> <continent>
	} else if(!str_cmp(buf,"randroom")) {
		ROOM_INDEX_DATA *loc;

		if( arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || IS_NPC(arg->d.mob) )
			return;

		vch = arg->d.mob;

		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		int continent = get_continent(arg->d.str);
		if( continent < 0 )
			continent = ANY_CONTINENT;

		loc = get_random_room(vch, continent);
		if( loc != NULL )
			variables_set_room(vars,name,loc);

	// DUNGEONRAND $(SECTION|INSTANCE|DUNGEON)
	} else if(!str_cmp(buf,"dungeonrand")) {
		ROOM_INDEX_DATA *loc = NULL;

		if( arg->type == ENT_DUNGEON )
			loc = dungeon_random_room(NULL, arg->d.dungeon );
		else if( arg->type == ENT_INSTANCE )
		{
			if( IS_VALID(arg->d.instance) && IS_VALID(arg->d.instance->dungeon) )
				loc = dungeon_random_room(NULL, arg->d.instance->dungeon );
		}
		else if( arg->type == ENT_SECTION )
		{
			if( IS_VALID(arg->d.section) && IS_VALID(arg->d.section->instance) && IS_VALID(arg->d.section->instance->dungeon) )
				loc = dungeon_random_room(NULL, arg->d.section->instance->dungeon );
		}

		if( loc != NULL )
			variables_set_room(vars,name,loc);

	// INSTANCERAND $(SECTION|INSTANCE)
	} else if(!str_cmp(buf,"instancerand")) {
		ROOM_INDEX_DATA *loc = NULL;

		if( arg->type == ENT_INSTANCE )
		{
			if( IS_VALID(arg->d.instance) )
				loc = instance_random_room(NULL, arg->d.instance );
		}
		else if( arg->type == ENT_SECTION )
		{
			if( IS_VALID(arg->d.section) && IS_VALID(arg->d.section->instance) )
				loc = instance_random_room(NULL, arg->d.section->instance );
		}

		if( loc != NULL )
			variables_set_room(vars,name,loc);

	// SECTIONRAND $(SECTION)
	} else if(!str_cmp(buf,"sectionrand")) {
		ROOM_INDEX_DATA *loc = NULL;

		if( arg->type == ENT_SECTION )
		{
			if( IS_VALID(arg->d.section) )
				loc = section_random_room(NULL, arg->d.section );
		}

		if( loc != NULL )
			variables_set_room(vars,name,loc);

	// SPECIALROOM $(INSTANCE|DUNGEON) [#.]KEYWORD
	// SPECIALROOM $(INSTANCE|DUNGEON) INDEX
	} else if(!str_cmp(buf,"specialroom")) {
		ROOM_INDEX_DATA *loc = NULL;

		if( arg->type == ENT_DUNGEON )
		{
			DUNGEON *dungeon = arg->d.dungeon;

			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if(arg->type == ENT_STRING)
				loc = get_dungeon_special_room_byname(dungeon, arg->d.str);
			else if(arg->type == ENT_NUMBER)
				loc = get_dungeon_special_room(dungeon, arg->d.num);

		}
		else if( arg->type == ENT_INSTANCE )
		{
			INSTANCE *instance = arg->d.instance;

			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if(arg->type == ENT_STRING)
				loc = get_instance_special_room_byname(instance, arg->d.str);
			else if(arg->type == ENT_NUMBER)
				loc = get_instance_special_room(instance, arg->d.num);
		}

		if( loc != NULL )
			variables_set_room(vars,name,loc);



	} else
		return;
}

bool valid_spell_token( TOKEN_DATA *token )
{
	ITERATOR pit;
	PROG_LIST *prg = NULL;

	if( IS_VALID(token) && token->pIndexData->progs ) {
		iterator_start(&pit, token->pIndexData->progs[TRIGSLOT_SPELL]);
		while((prg = (PROG_LIST *)iterator_nextdata(&pit)))
			if(prg->trig_type == TRIG_SPELL)
				break;
		iterator_stop(&pit);
	}

	return prg && TRUE;
}

bool visit_script_execute(ROOM_INDEX_DATA *room, void *argv[], int argc, int depth, int door)
{
	int ret;
	SCRIPT_DATA *script = (SCRIPT_DATA *)argv[0];

	ret = execute_script(script, NULL, NULL, room, NULL, NULL, NULL, NULL,
		(CHAR_DATA *)argv[1],	// enactor
		(OBJ_DATA *)argv[2],	// object 1
		(OBJ_DATA *)argv[3],	// object 2
		(CHAR_DATA *)argv[4],	// victim 1
		(CHAR_DATA *)argv[5],	// victim 2
		NULL,					// no random
		NULL,					// no token
		(char *)argv[6],		// phrase
		NULL,					// no trigger
		TRIG_NONE,				// trigger type
		depth,					// register 1
		door,					// register 2
		0,						// register 3
		0,						// register 4
		0);						// register 5

	return ret > 0;
}

void script_end_success(CHAR_DATA *ch)
{
	TOKEN_DATA *tok = NULL;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	SCRIPT_DATA *script = NULL;
	VARIABLE **var = NULL;

	if( ch->script_wait_success != NULL) {
		script = ch->script_wait_success;

		if( ch->script_wait_token != NULL ) {
			if( IS_VALID(ch->script_wait_token) &&
				ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
				tok = ch->script_wait_token;
				var = &tok->progs->vars;
			}
		} else if( ch->script_wait_mob != NULL ) {
			if( IS_VALID(ch->script_wait_mob) &&
				ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
				mob = ch->script_wait_mob;
				var = &mob->progs->vars;
			}
		} else if( ch->script_wait_obj != NULL ) {
			if( IS_VALID(ch->script_wait_obj) &&
				ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
				obj = ch->script_wait_obj;
				var = &obj->progs->vars;
			}
		}

	}

	ch->script_wait_success = NULL;
	ch->script_wait_failure = NULL;
	ch->script_wait_pulse = NULL;
	ch->script_wait_mob = NULL;
	ch->script_wait_obj = NULL;
	ch->script_wait_token = NULL;

	if( var != NULL && script != NULL )
		execute_script(script, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE,0,0,0,0,0);

	ch->script_wait = 0;
}

void script_end_failure(CHAR_DATA *ch, bool messages)
{
	TOKEN_DATA *tok = NULL;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	SCRIPT_DATA *script = NULL;
	VARIABLE **var = NULL;

	if( ch->script_wait_failure != NULL) {
		script = ch->script_wait_failure;

		if( ch->script_wait_token != NULL ) {
			if( IS_VALID(ch->script_wait_token) &&
				ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
				tok = ch->script_wait_token;
				var = &tok->progs->vars;
			}
		} else if( ch->script_wait_mob != NULL ) {
			if( IS_VALID(ch->script_wait_mob) &&
				ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
				mob = ch->script_wait_mob;
				var = &mob->progs->vars;
			}
		} else if( ch->script_wait_obj != NULL ) {
			if( IS_VALID(ch->script_wait_obj) &&
				ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
				ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
				obj = ch->script_wait_obj;
				var = &obj->progs->vars;
			}
		}
	}

	ch->script_wait_success = NULL;
	ch->script_wait_failure = NULL;
	ch->script_wait_pulse = NULL;
	ch->script_wait_mob = NULL;
	ch->script_wait_obj = NULL;
	ch->script_wait_token = NULL;

	if( var != NULL && script != NULL )
		execute_script(script, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, (messages?NULL:"silent"), NULL,TRIG_NONE,0,0,0,0,0);

	ch->script_wait = 0;
}


void script_end_pulse(CHAR_DATA *ch)
{
	TOKEN_DATA *tok = NULL;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	VARIABLE **var = NULL;

	if( ch->script_wait_pulse == NULL)
		return;

	//printf_to_char(ch, "script_end_pulse: %ld\n\r", ch->script_wait);

	if( ch->script_wait_token != NULL ) {
		if( IS_VALID(ch->script_wait_token) &&
			ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
			ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
			tok = ch->script_wait_token;
			var = &tok->progs->vars;
		}
	} else if( ch->script_wait_mob != NULL ) {
		if( IS_VALID(ch->script_wait_mob) &&
			ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
			ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
			mob = ch->script_wait_mob;
			var = &mob->progs->vars;
		}
	} else if( ch->script_wait_obj != NULL ) {
		if( IS_VALID(ch->script_wait_obj) &&
			ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
			ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
			obj = ch->script_wait_obj;
			var = &obj->progs->vars;
		}
	}

	if(!mob && !obj && !tok) return;

	if( var != NULL )
		execute_script(ch->script_wait_pulse, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,TRIG_NONE,0,0,0,0,0);
}

int script_flag_lookup (const char *name, const struct flag_type *flag_table)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name))
	    return flag_table[flag].bit;
    }

    return 0;
}

long script_stat_lookup (const char *name, const struct flag_type *flag_table, const long def_value)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
		!str_prefix(name,flag_table[flag].name))
	    return flag_table[flag].bit;
    }

    return def_value;
}

bool script_bitmatrix_lookup(char *argument, const struct flag_type **bank, long *flags)
{
    char word[MIL];
    bool valid = TRUE;

    if (!bank || !flags) return FALSE;

    for(int i = 0; bank[i]; i++)
        flags[i] = 0;

    while(valid)
    {
        argument = one_argument(argument, word);

        if (word[0] == '\0')
            break;

        long value = 0;
        int nth = -1;
        for(int i = 0; bank[i]; i++)
        {
            value = flag_find(word, bank[i]);
            if (value != 0)
            {
                nth = i;
                break;
            }
        }

        if (value != 0)
        {
            SET_BIT(flags[nth], value);
        }
        else
        {
            valid = FALSE;
        }
    }

    return valid;
}

long script_flag_value( const struct flag_type *flag_table, char *argument)
{
    char word[MAX_INPUT_LENGTH];
    long bit;
    long marked = 0;
    int flag;
    bool found = FALSE;

    if ( flag_table == NULL ) return NO_FLAG;

    if ( is_stat( flag_table ) )
    {
		one_argument( argument, word );

	    for (flag = 0; flag_table[flag].name != NULL; flag++)
	    {
			if (LOWER(word[0]) == LOWER(flag_table[flag].name[0]) &&
				!str_prefix(word,flag_table[flag].name))
	    		return flag_table[flag].bit;
    	}

	    return NO_FLAG;
    }

    /*
     * Accept multiple flags.
     */
    for (; ;)
    {
        argument = one_argument( argument, word );

        if ( word[0] == '\0' )
	    break;

        if ( ( bit = script_flag_lookup( word, flag_table ) ) != 0 )
        {
            SET_BIT( marked, bit );
            found = TRUE;
        }
    }

    if ( found )
	return marked;
    else
	return NO_FLAG;
}

CHAR_DATA *script_get_char_room(SCRIPT_VARINFO *info, char *name, bool see_all)
{
	if( !info ) return NULL;

	if( info->mob ) {
		if( see_all )	// If see_all, bypass ALL vision checks
			return get_char_room(NULL, info->mob->in_room, name);
		else
			return get_char_room(info->mob, NULL, name);
	}
	if( info->obj ) return get_char_room(NULL, obj_room(info->obj), name);
	if( info->room ) return get_char_room(NULL, info->room, name);
	if( info->token ) return get_char_room(NULL, token_room(info->token), name);

	return NULL;
}

OBJ_DATA *script_get_obj_here(SCRIPT_VARINFO *info, char *name)
{

	if( !info ) return NULL;

	if( info->mob ) return get_obj_here(info->mob, NULL, name);
	if( info->obj ) return get_obj_here(NULL, obj_room(info->obj), name);
	if( info->room ) return get_obj_here(NULL, info->room, name);
	if( info->token ) return get_obj_here(NULL, token_room(info->token), name);

	return NULL;
}

// MLOAD $WNUM|$MOBINDEX|$MOBILE $ROOM[ $VARIABLENAME]
CHAR_DATA *script_mload(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg, bool instanced)
{
	char *rest;
	WNUM wnum;
	MOB_INDEX_DATA *pMobIndex = NULL;
	ROOM_INDEX_DATA *room;
	CHAR_DATA *victim;

	if(!info) return NULL;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return NULL;

	switch(arg->type) {
	case ENT_WIDEVNUM:
		pMobIndex = get_mob_index_wnum(arg->d.wnum);
		break;
	case ENT_STRING:
		if (!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			return NULL;
		
		pMobIndex = get_mob_index_wnum(wnum);
		break;
	case ENT_MOBILE:
		if (IS_VALID(arg->d.mob) && IS_NPC(arg->d.mob) )
			pMobIndex = arg->d.mob->pIndexData;
		break;
	case ENT_MOBINDEX:
		pMobIndex = arg->d.mobindex;
		break;
	default: break;
	}

	if (!pMobIndex) {
//		sprintf(buf, "Mpmload: bad mob index (%ld) from mob %ld", vnum, VNUM(info->mob));
//		bug(buf, 0);
		return NULL;
	}

	room = NULL;

	char *var_name = rest;

	if( rest && *rest )
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return NULL;

		if( arg->type == ENT_ROOM )
		{
			room = arg->d.room;
			var_name = rest;
		}
		else if( arg->type == ENT_WIDEVNUM )
		{
			room = get_room_index_wnum(arg->d.wnum);
			var_name = rest;
		}

	}

	if( !room )
	{
		if( info->mob ) room = info->mob->in_room;
		else if( info->obj ) room = obj_room(info->obj);
		else if( info->room ) room = info->room;
		else if( info->token ) room = token_room(info->token);
	}

	if( !room )
		return NULL;

	victim = create_mobile(pMobIndex, FALSE);
	if( !IS_VALID(victim) )
		return NULL;

	if( instanced )
		SET_BIT(victim->act[1], ACT2_INSTANCE_MOB);

	char_to_room(victim, room);
	if(var_name && *var_name) variables_set_mobile(info->var,var_name,victim);
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);

	info->progs->lastreturn = 1;

	return victim;
}

// OLOAD $WNUM|$OBJINDEX|$OBJECT $LEVEL[ none|room|wear|$MOBILE[ wear]|$OBJECT|$ROOM[ $VARIABLENAME]]
OBJ_DATA *script_oload(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg, bool instanced)
{
//	char msg[MSL];
	char buf[MIL], *rest;
	WNUM wnum;
	long level;
	bool fToroom = FALSE, fWear = FALSE;
	OBJ_INDEX_DATA *pObjIndex = NULL;
	OBJ_DATA *obj;
	CHAR_DATA *to_mob = info->mob;
	OBJ_DATA *to_obj = NULL;
	ROOM_INDEX_DATA *here = NULL;
	ROOM_INDEX_DATA *to_room = NULL;

	if(!info) return NULL;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return NULL;

	if( info->mob ) here = info->mob->in_room;
	else if( info->obj ) here = obj_room(info->obj);
	else if( info->room ) here = info->room;
	else if( info->token ) here = token_room(info->token);

	switch(arg->type) {
	case ENT_WIDEVNUM: pObjIndex = get_obj_index_wnum(arg->d.wnum); break;
	case ENT_STRING:
		if (!parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			return NULL;

		pObjIndex = get_obj_index_wnum(wnum);
		break;
	case ENT_OBJECT:
		if (IS_VALID(arg->d.obj))
			pObjIndex = arg->d.obj->pIndexData;
		break;
	case ENT_OBJINDEX:
		pObjIndex = arg->d.objindex;
		break;
	default: return NULL;
	}

	if (!pObjIndex) {
		wiznet("Bad object for script_oload",NULL,NULL,WIZ_TESTING,0,0);

//		bug("*Poload - Bad vnum arg from vnum %d.", VNUM(info->);
		return NULL;
	}

	if(rest && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg)))
			return NULL;

		switch(arg->type) {
		case ENT_NUMBER: level = arg->d.num; break;
		case ENT_STRING: level = arg->d.str ? atoi(arg->d.str) : 0; break;
		case ENT_MOBILE: level = arg->d.mob ? get_trust(arg->d.mob) : 0; break;
		case ENT_OBJECT: level = arg->d.obj ? arg->d.obj->pIndexData->level : 0; break;
		default: level = 0; break;
		}

		if(info->mob)
		{
			if(level <= 0 || level > get_trust(info->mob))
				level = get_trust(info->mob);
		}

		if(rest && *rest) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)))
				return NULL;

			/*
			 * Added 3rd argument
			 * omitted - load to mobile's inventory
			 * 'none'  - load to mobile's inventory
			 * 'room'  - load to room
			 * 'wear'  - load to mobile and force wear
			 * MOBILE  - load to target mobile
			 *         - 'W' automatically wear
			 * OBJECT  - load to target object
			 * ROOM    - load to target room
			 */

			switch(arg->type) {
			case ENT_STRING:
				if(!str_cmp(arg->d.str, "room"))
					fToroom = TRUE;
				else if(!str_cmp(arg->d.str, "wear"))
					fWear = TRUE;
				break;

			case ENT_MOBILE:
				to_mob = arg->d.mob;
				if((rest = one_argument(rest,buf))) {
					if(!str_cmp(buf, "wear"))
						fWear = TRUE;
					// use "none" for neither
				}
				break;

			case ENT_OBJECT:
				if( arg->d.obj && IS_SET(pObjIndex->wear_flags, ITEM_TAKE) ) {
					if(IS_CONTAINER(arg->d.obj))
					{
						int subtype = objindex_get_subtype(pObjIndex);
						if (container_is_valid_item_type(arg->d.obj, pObjIndex->item_type, subtype))
							to_obj = arg->d.obj;
					}
					else if(arg->d.obj->item_type == ITEM_CORPSE_NPC || arg->d.obj->item_type == ITEM_CORPSE_PC)
						to_obj = arg->d.obj;
					else
						return NULL;	// Trying to put the item into a non-container won't work
				}
				break;

			case ENT_ROOM:		to_room = arg->d.room; break;
			}
		}

	} else if(info->mob)
		level = get_trust(info->mob);
	else
		level = 0;

	obj = create_object(pObjIndex, level, TRUE);
	if( !IS_VALID(obj) )
		return NULL;

	if( instanced )
		SET_BIT(obj->extra[2], ITEM_INSTANCE_OBJ);

	if( to_room )
		obj_to_room(obj, to_room);
	else if( to_obj )
		obj_to_obj(obj, to_obj);
	else if( to_mob && (fWear || !fToroom) && CAN_WEAR(obj, ITEM_TAKE) &&
		(to_mob->carry_number < can_carry_n (to_mob)) &&
		(get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
		obj_to_char(obj, to_mob);
		if (fWear)
			wear_obj(to_mob, obj, TRUE);
	}
	else if( here )
		obj_to_room(obj, here);
	else
	{
		// No place to put the object, nuke it

		// This shouldn't be necessary since it was never put anywhere, used anywhere!
		//extract_obj(obj);

		// This is the minimum actions necessary for a phantom object extraction
		list_remlink(loaded_objects, obj);
	    --obj->pIndexData->count;
	    free_obj(obj);
	    return NULL;
	}


	if(rest && *rest) variables_set_object(info->var,rest,obj);
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);

	info->progs->lastreturn = 1;

	return obj;
}




void save_trigger(FILE *fp, struct trigger_type *tt)
{
	fprintf(fp, "#TRIGGER %s~\n", fix_string(tt->name));
	if (!IS_NULLSTR(tt->alias))
		fprintf(fp, "Alias %s\n", tt->alias);
	
	if (tt->type < TRIG__MAX)
	{
		fprintf(fp, "BuiltinType %s~\n", flag_string(builtin_trigger_types, tt->type));
	}

	fprintf(fp, "Scriptable %d\n", (tt->scriptable ? 1 : 0));
	fprintf(fp, "Slot %s~\n", flag_string(trigger_slots, tt->slot));

	if (IS_SET(tt->progs, PRG_MPROG)) fprintf(fp, "Mob\n");
	if (IS_SET(tt->progs, PRG_OPROG)) fprintf(fp, "Obj\n");
	if (IS_SET(tt->progs, PRG_RPROG)) fprintf(fp, "Room\n");
	if (IS_SET(tt->progs, PRG_TPROG)) fprintf(fp, "Token\n");
	if (IS_SET(tt->progs, PRG_APROG)) fprintf(fp, "Area\n");
	if (IS_SET(tt->progs, PRG_IPROG)) fprintf(fp, "Instance\n");
	if (IS_SET(tt->progs, PRG_DPROG)) fprintf(fp, "Dungeon\n");

	fprintf(fp, "#-TRIGGER\n");
}



void save_triggers()
{
	FILE *fp;

	log_string("save_triggers: saving " TRIGGERS_FILE);
	if ((fp = fopen(TRIGGERS_FILE, "w")) == NULL)
	{
		bug("save_triggers: fopen", 0);
		perror(TRIGGERS_FILE);
	}
	else
	{
#if 0
		for(int i = 0; trigger_table[i].name; i++)
		{
			log_stringf("%s => %08X", trigger_table[i].name, trigger_table[i].progs);
			save_trigger(fp, &trigger_table[i]);
		}
#else
		ITERATOR it;
		struct trigger_type *trigger;
		iterator_start(&it, trigger_list);
		while((trigger = (struct trigger_type *)iterator_nextdata(&it)))
		{
			save_trigger(fp, trigger);
		}
		iterator_stop(&it);
#endif

		fprintf(fp, "End\n");
		fclose(fp);
	}
}

struct trigger_type *new_trigger_type()
{
	struct trigger_type *tt = alloc_mem(sizeof(struct trigger_type));
	
	memset(tt, 0, sizeof(*tt));

	tt->name = str_dup("");
	tt->alias = str_dup("");
	tt->type = TRIG__MAX;		// Flag it as being unassigned
	tt->scriptable = true;

	return tt;
}

void free_trigger_type(struct trigger_type *tt)
{
	free_string(tt->name);
	free_string(tt->alias);

	free_mem(tt, sizeof(struct trigger_type));
}

static void delete_trigger_type(void *ptr)
{
	free_trigger_type((struct trigger_type *)ptr);
}

int compare_trigger_types(void *a, void *b)
{
	return str_cmp(((struct trigger_type *)a)->name,((struct trigger_type *)b)->name);
}

struct trigger_type *load_trigger(FILE *fp)
{
	char buf[MSL];
	char *word;
	bool fMatch;
	struct trigger_type *tt = new_trigger_type();

	free_string(tt->name);
	tt->name = fread_string(fp);
	
    while (str_cmp((word = fread_word(fp)), "#-TRIGGER"))
	{
		switch(word[0])
		{
			case 'A':
				KEYS("Alias", tt->alias, fread_string(fp));
				KEYF("Area", tt->progs, PRG_APROG);
				break;
			
			case 'B':
				if (!str_cmp(word, "BuiltinType"))
				{
					int type = stat_lookup(fread_string(fp), builtin_trigger_types, TRIG__MAX);

					if (type != TRIG__MAX)
					{
						tt->type = type;
					}

					fMatch = TRUE;
					break;
				}
				break;

			case 'D':
				KEYF("Dungeon", tt->progs, PRG_DPROG);
				break;

			case 'I':
				KEYF("Instance", tt->progs, PRG_IPROG);
				break;

			case 'M':
				KEYF("Mob", tt->progs, PRG_MPROG);
				break;

			case 'O':
				KEYF("Obj", tt->progs, PRG_OPROG);
				break;

			case 'R':
				KEYF("Room", tt->progs, PRG_RPROG);
				break;

			case 'S':
				KEY("Scriptable", tt->scriptable, fread_number(fp));
				if (!str_cmp(word, "Slot"))
				{
					tt->slot = stat_lookup(fread_string(fp), trigger_slots, NO_FLAG);
					if (tt->slot == NO_FLAG)
						tt->slot = TRIGSLOT_GENERAL;
					fMatch = TRUE;
					break;
				}
				break;

			case 'T':
				KEYF("Token", tt->progs, PRG_TPROG);
				break;
		}

		if (!fMatch) {
			sprintf(buf, "load_trigger: no match for word %s", word);
			bug(buf, 0);
		}
	}

	if (tt->type == TRIG__MAX)
		tt->type = ++top_trigger_type;

	return tt;
}

void insert_trigger_type(struct trigger_type *new_tt)
{
	ITERATOR it;
	struct trigger_type *tt;
	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(new_tt->name, tt->name);
		if(cmp < 0)
		{
			iterator_insert_before(&it, new_tt);
			break;
		}
	}
	iterator_stop(&it);

	if (!tt)
	{
		list_appendlink(trigger_list, new_tt);
	}
}

bool load_triggers()
{
	FILE *fp;
	char buf[MSL];
	char *word;
	bool fMatch;
	top_trigger_type = TRIG__MAX;

	log_string("load_triggers: creating trigger_list");
	trigger_list = list_createx(FALSE, NULL, delete_trigger_type);
	if (!IS_VALID(trigger_list))
	{
		log_string("trigger_list was not created.");
		return FALSE;
	}

#if 1
	log_string("load_triggers: loading " TRIGGERS_FILE);
	if ((fp = fopen(TRIGGERS_FILE, "r")) == NULL)
	{
		bug("load_triggers: fopen", 0);
		perror(TRIGGERS_FILE);
		return FALSE;
	}

	while (str_cmp((word = fread_word(fp)), "End"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#TRIGGER"))
				{
					struct trigger_type *trigger = load_trigger(fp);
					if (trigger)
						insert_trigger_type(trigger);
					else
						log_string("Failed to load a trigger.");
					fMatch = TRUE;
					break;
				}
				break;
		}

		if (!fMatch) {
			sprintf(buf, "load_triggers: no match for word %s", word);
			bug(buf, 0);
		}
	}
	log_string("Triggers loaded.");


#if 0
	ITERATOR it;
	struct trigger_type *tt;

	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		log_stringf("load_triggers: trigger '%s'.", tt->name);
	}
	iterator_stop(&it);
#endif
	log_stringf("load_triggers: total triggers = %d", list_size(trigger_list));
#endif
	return TRUE;
}

bool init_scripting()
{
	return load_triggers();
}

void terminate_scripting()
{
	if (IS_VALID(trigger_list))
		save_triggers();

	list_destroy(trigger_list);
}

const char *get_trigger_type_name(int type)
{
	static int i = 0;
	static char buf[4][MIL];

	char *str = &buf[i][0];
	i = (i + 1) & 3;

	if (type < TRIG__MAX)
	{
		snprintf(str, MIL - 1, "%s", flag_string(builtin_trigger_types, type));
	}
	else
	{
		snprintf(str, MIL - 1, "Custom Trigger #%d", type - TRIG__MAX);
	}
	return str;
}

struct trigger_type *get_trigger_type_byname(char *name)
{
	ITERATOR it;
	struct trigger_type *tt;
	iterator_start(&it, trigger_list);
	while((tt = (struct trigger_type *)iterator_nextdata(&it)))
	{
		if (!str_cmp(tt->name, name))
			break;
	}
	iterator_stop(&it);

	return tt;
}

void do_triggers(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg1[MIL];
	bool authorized = IS_IMPLEMENTOR(ch) && IS_SECURITY(ch, 9);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  triggers list\n\r", ch);
		if (authorized)
		{
			send_to_char("         triggers add <name>[ <slot>[ <space>]]\n\r", ch);
			send_to_char("         triggers install <name> <trigger> <slot> <scriptable> <space>\n\r", ch);
			send_to_char("         triggers scriptable <name> <yes|no>\n\r", ch);
			send_to_char("         triggers space <name> <space>\n\r", ch);
			send_to_char("         triggers remove <name>\n\r", ch);
		}
		return;
	}

	argument = one_argument(argument, arg1);

	if (!str_prefix(arg1, "list"))
	{
		BUFFER *buffer = new_buf();

		add_buf(buffer, "                                                                            {B  {Y  {RR {W  {GA {CI {MD{x\n\r");
		add_buf(buffer, "                                                                            {BM {YO {Ro {WT {Gr {Cn {Mu{x\n\r");
		add_buf(buffer, "                                                                            {Bo {Yb {Ro {Wo {Ge {Cs {Mn{x\n\r");
		add_buf(buffer, "                                                                            {Bb {Yj {Rm {Wk {Ga {Ct {Mg{x\n\r");
		add_buf(buffer, "      [        Name        ] [    Trigger Type    ] [      Slot      ] [S] [    Space    ] [ Count ]\n\r");
		add_buf(buffer, "=================================================================================================\n\r");

		int trigger_no = 1;
		ITERATOR it;
		struct trigger_type *tt;
		iterator_start(&it, trigger_list);
		while((tt = (struct trigger_type *)iterator_nextdata(&it)))
		{
			sprintf(buf, "%-4d   %-20s   %-20s   %-16s   %s   %s %s %s %s %s %s %s{x    %5d\n\r", trigger_no++,
				tt->name,
				get_trigger_type_name(tt->type),
				flag_string(trigger_slots, tt->slot),
				(tt->scriptable ? "{GY" : "{RN"),
				(IS_SET(tt->progs, PRG_MPROG) ? "{B#" : "{b-"),
				(IS_SET(tt->progs, PRG_OPROG) ? "{Y#" : "{y-"),
				(IS_SET(tt->progs, PRG_RPROG) ? "{R#" : "{r-"),
				(IS_SET(tt->progs, PRG_TPROG) ? "{W#" : "{w-"),
				(IS_SET(tt->progs, PRG_APROG) ? "{G#" : "{g-"),
				(IS_SET(tt->progs, PRG_IPROG) ? "{C#" : "{c-"),
				(IS_SET(tt->progs, PRG_DPROG) ? "{M#" : "{m-"),
				tt->usage);

			add_buf(buffer, buf);
		}
		iterator_stop(&it);

		add_buf(buffer, "-------------------------------------------------------------------------------------------------\n\r");

		sprintf(buf, "Total Triggers: %d\n\r", list_size(trigger_list));
		add_buf(buffer, buf);

		if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
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

	if (authorized)
	{
		if (!str_prefix(arg1, "add"))
		{
			char arg2[MIL];	// Name
			char arg3[MIL]; // slot

			argument = one_argument(argument, arg2);
			argument = one_argument(argument, arg3);

			if (IS_NULLSTR(arg2))
			{
				send_to_char("Name must not be empty.\n\r", ch);
				return;
			}

			// Verify the name isn't used
			if (get_trigger_type_byname(arg2))
			{
				send_to_char("Name already in use.  Please pick another name.\n\r", ch);
				return;
			}

			int slot = TRIGSLOT_GENERAL;
			int space = 0;
			if (arg3[0] != '\0')
			{
				if ((slot = stat_lookup(arg3, trigger_slots, NO_FLAG)) == NO_FLAG)
				{
					send_to_char("Invalid slot.\n\r", ch);
					show_help(ch, "trigger slots");
					return;
				}

				if (argument[0] != '\0')
				{
					if ((space = flag_value(script_spaces, argument)) == NO_FLAG)
					{
						send_to_char("Invalid script space.\n\r", ch);
						show_help(ch, "script_spaces");
						return;
					}
				}
			}

			struct trigger_type *tt = new_trigger_type();

			free_string(tt->name);
			tt->name = str_dup(arg2);
			tt->type = ++top_trigger_type;
			tt->slot = slot;
			tt->progs = space;
			
			insert_trigger_type(tt);
			send_to_char("Trigger type added.\n\r", ch);
			save_triggers();
			return;
		}

		if (!str_prefix(arg1, "install"))
		{
			char arg2[MIL];	// Name
			char arg3[MIL];	// built-in trigger
			char arg4[MIL];	// slot
			char arg5[MIL];
			// argument = space

			argument = one_argument(argument, arg2);
			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);
			argument = one_argument(argument, arg5);

			if (IS_NULLSTR(arg2))
			{
				send_to_char("Name must not be empty.\n\r", ch);
				return;
			}

			// Verify the name isn't used
			if (get_trigger_type_byname(arg2))
			{
				send_to_char("Name already in use.  Please pick another name.\n\r", ch);
				return;
			}

			int type;
			if ((type = stat_lookup(arg3, builtin_trigger_types, TRIG_NONE)) == TRIG_NONE)
			{
				send_to_char("Please specify one of the builtin trigger types:\n\r", ch);
				show_help(ch, "trigger_types");
				return;
			}

			if (get_trigger_type_bytype(type))
			{
				send_to_char("That trigger type is already registered.\n\r", ch);
				return;
			}

			int slot;
			if ((slot = stat_lookup(arg4, trigger_slots, NO_FLAG)) == NO_FLAG)
			{
				send_to_char("Please specify one of the trigger slots:\n\r", ch);
				show_help(ch, "trigger_slots");
				return;
			}

			bool scriptable = TRISTATE;
			if (!str_prefix(arg5, "yes"))
				scriptable = TRUE;
			else if (!str_prefix(arg5, "no"))
				scriptable = FALSE;
			else
			{
				send_to_char("Please specify {GYES{x or {RNO{x for scriptability.\n\r",ch);
				return;
			}
			
			int space;
			if ((space = flag_value(script_spaces, argument)) == NO_FLAG)
			{
				send_to_char("Please select from the following script spaces:\n\r", ch);
				show_help(ch, "script_spaces");
				return;
			}

			struct trigger_type *tt = new_trigger_type();

			free_string(tt->name);
			tt->name = str_dup(arg2);
			tt->type = type;
			tt->slot = slot;
			tt->scriptable = scriptable;
			tt->progs = space;
			
			insert_trigger_type(tt);
			send_to_char("Trigger type installed.\n\r", ch);
			save_triggers();
			return;
		}


		if (!str_prefix(arg1, "scriptable"))
		{
			char arg2[MIL];

			argument = one_argument(argument, arg2);

			if (arg2[0] == '\0')
			{
				send_to_char("Syntax:  triggers scriptable <name> <yes|no>\n\r", ch);
				send_to_char("Please specify a name.\n\r", ch);
				return;
			}

			struct trigger_type *tt = get_trigger_type_byname(arg2);
			if (!tt)
			{
				send_to_char("Syntax:  triggers scriptable <name> <yes|no>\n\r", ch);
				send_to_char("No such trigger by that name exists.\n\r", ch);
				return;
			}

			if (tt->type >= TRIG__MAX)
			{
				send_to_char("Custom triggers are inherently scriptable.  That may not be changed.\n\r", ch);
				return;
			}

			bool value = TRISTATE;
			if (!str_prefix(argument, "yes"))
				value = TRUE;
			else if (!str_prefix(argument, "no"))
				value = FALSE;
			else
			{
				send_to_char("Syntax:  triggers scriptable <name> <yes|no>\n\r", ch);
				send_to_char("Please specify {GYES{x or {RNO{x.\n\r", ch);
				return;
			}

			tt->scriptable = value;
			save_triggers();
			send_to_char("Trigger Scriptability changed.\n\r", ch);
			return;
		}

		if (!str_prefix(arg1, "space"))
		{
			char arg2[MIL];

			argument = one_argument(argument, arg2);

			if (arg2[0] == '\0')
			{
				send_to_char("Syntax:  triggers space <name> <space>\n\r", ch);
				send_to_char("Please specify a name.\n\r", ch);
				return;
			}

			struct trigger_type *tt = get_trigger_type_byname(arg2);
			if (!tt)
			{
				send_to_char("Syntax:  triggers space <name> <space>\n\r", ch);
				send_to_char("No such trigger by that name exists.\n\r", ch);
				return;
			}

			// This is potentially super dangerous, so it should be extremely restrictive
			if (tt->type < TRIG__MAX && tt->usage > 0 && ch->pcdata->security < 10)
			{
				send_to_char("Syntax:  triggers space <name> <space>\n\r", ch);
				send_to_char("You may only modify custom triggers.\n\r", ch);
				return;
			}

			int value;
			if ((value = flag_value(script_spaces, argument)) == NO_FLAG)
			{
				send_to_char("Syntax:  triggers space <name> <space>\n\r", ch);
				send_to_char("Invalid script space.\n\r", ch);
				show_help(ch, "script_spaces");
				return;
			}

			TOGGLE_BIT(tt->progs, value);
			save_triggers();
			send_to_char("Trigger Script Space changed.\n\r", ch);
			return;
		}

		if (!str_prefix(arg1, "remove"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  triggers remove <name>\n\r", ch);
				send_to_char("Please specify a name.\n\r", ch);
				return;
			}

			struct trigger_type *tt = get_trigger_type_byname(argument);
			if (!tt)
			{
				send_to_char("Syntax:  triggers remove <name>\n\r", ch);
				send_to_char("No such trigger by that name exists.\n\r", ch);
				return;
			}

			if (tt->type < TRIG__MAX)
			{
				send_to_char("Syntax:  triggers remove <name>\n\r", ch);
				send_to_char("You may only remove unused custom triggers.\n\r", ch);
				return;
			}

			if (tt->usage > 0)
			{
				send_to_char("You may only remove unused custom triggers.\n\r", ch);
				return;
			}

			// This will delete the trigger
			list_remlink(trigger_list, tt);
			save_triggers();
			send_to_char("Custom trigger removed.\n\r", ch);
			return;
		}
	}

	do_triggers(ch, "");
}

void trigger_type_add_use(struct trigger_type *tt)
{
	if (tt)
	{
		++tt->usage;
	}
}

void trigger_type_delete_use(struct trigger_type *tt)
{
	if (tt && tt->usage > 0)
	{
		--tt->usage;
	}
}

// Allows token indices that do percent triggers to fire.
int p_token_index_percent_trigger(TOKEN_INDEX_DATA *tindex, CHAR_DATA *ch, CHAR_DATA *victim1, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int trigger, char *phrase, int ts0, int ts1, int ts2, int ts3, int ts4, int register1, int register2, int register3, int register4, int register5)
{
	log_stringf("%s: tindex = %08X", __FUNCTION__, tindex);

	TOKEN_DATA *token = create_token(tindex);

	log_stringf("%s: token = %08X, %s", __FUNCTION__, token, (token && token->valid) ? "valid" : "invalid");

	if (!IS_VALID(token)) return PRET_NOSCRIPT;

	token->tempstore[0] = ts0;
	token->tempstore[1] = ts1;
	token->tempstore[2] = ts2;
	token->tempstore[3] = ts3;
	token->tempstore[4] = ts4;
	int ret = p_percent_trigger(NULL, NULL, NULL, token, ch, victim1, victim2, obj1, obj2, trigger, phrase,register1,register2,register3,register4,register5);

	// I do not need to extract this thing
	free_token(token);

	return ret;
}

SCRIPT_DATA *get_script_token(TOKEN_INDEX_DATA *token, int trigger, int slot)
{
	ITERATOR it;
	PROG_LIST *prg;
	SCRIPT_DATA *script = NULL;

	if( token->progs )
	{
		iterator_start(&it, token->progs[slot]);
		while(( prg = (PROG_LIST *)iterator_nextdata(&it))) {
			if(is_trigger_type(prg->trig_type,trigger)) {
				script = prg->script;
				break;
			}
		}
		iterator_stop(&it);
	}

	return script;
}

void scriptcmd_bug(SCRIPT_VARINFO *info, char *message)
{
	char buf[2 * MSL];

	sprintf(buf, "Script:%ld#%ld:Line:%d:%s\n\r",
		info->block->script->area->uid, info->block->script->vnum,
		info->block->line,
		message);
	bug(buf, 0);
}


PROG_LIST *find_trigger_data(LLIST **progs, int trigger_type, int count)
{
	if (!progs) return NULL;

	struct trigger_type *tt = get_trigger_type_bytype(trigger_type);

	if (!tt) return NULL;

	PROG_LIST *pr;
	ITERATOR it;
	iterator_start(&it, progs[tt->slot]);
	while((pr = (PROG_LIST *)iterator_nextdata(&it)))
	{
		if (pr->trig_type == trigger_type && --count < 1)
			break;
	}
	iterator_stop(&it);

	return pr;
}
