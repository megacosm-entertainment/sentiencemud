
/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "scripts.h"
#include "recycle.h"
#include "wilds.h"
#include "tables.h"

//#define DEBUG_MODULE
#include "debug.h"

extern RESERVED_WNUM reserved_room_wnums[];
extern RESERVED_WNUM reserved_obj_wnums[];
extern RESERVED_WNUM reserved_mob_wnums[];
extern RESERVED_WNUM reserved_rprog_wnums[];
extern RESERVED_AREA reserved_areas[];

extern	LLIST *loaded_instances;
extern	LLIST *loaded_dungeons;
extern	LLIST *loaded_ships;

RESERVED_WNUM *search_reserved(RESERVED_WNUM *reserved, char *name);
RESERVED_AREA *search_reserved_area(char *name);

char *expand_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,pVARIABLE *var);
char *expand_string_expression(SCRIPT_VARINFO *info,char *str,BUFFER *store);
char *expand_argument_entity(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg);

// Check the validity of the script parameters
//	Should the ids mismatch, reset the field
bool check_varinfo(SCRIPT_VARINFO *info)
{
	bool ret = TRUE;
	return ret;
}

void expand_escape2print(char *str, BUFFER *buffer)
{
	char hex[10];
	while(*str) {
		if(isprint(*str)) add_buf_char(buffer, *str);
		else {
			sprintf(hex,"0x%2.2X", *str);
			add_buf(buffer, hex);
		}
		str++;
		if(*str) add_buf_char(buffer, ' ');
	}
}

char *expand_skip(register char *str)
{
	register int depth = 0;

	while(*str) {
		if(*str == ESCAPE_VARIABLE || *str == ESCAPE_EXPRESSION || *str == ESCAPE_ENTITY)
			++depth;
		else if(*str == ESCAPE_END && !depth--)
			break;
		++str;
	}

	if(depth > 0) return str;

	return str + 1;
}


static bool push(STACK *stk,int val)
{
	if(stk->t >= MAX_STACK) return FALSE;
	stk->s[stk->t++] = val;
	return TRUE;
}

static int perform_operation(STACK *op,STACK *opr)
{
	int op1, op2, optr;

	optr = pop(opr,STK_EMPTY);

	if(optr == STK_MAX || optr == STK_EMPTY || op->t < script_expression_argstack[optr])
		return ERROR4;

	op2 = op1 = 0;
	if(script_expression_argstack[optr] > 1) op2 = pop(op,-1);
	if(script_expression_argstack[optr] > 0) op1 = pop(op,-1);

	switch(optr) {
	case STK_ADD:	op1 += op2; break;
	case STK_SUB:	op1 -= op2; break;
	case STK_MUL:	op1 *= op2; break;
	case STK_DIV:	if(!op2) return ERROR3; op1 /= op2; break;
	case STK_MOD:	if(!op2) return ERROR3; op1 = op1 % op2; break;
	case STK_RAND:	op1 = number_range(op1,op2);	break;
	case STK_NOT:	op1 = !op1; break;
	case STK_NEG:	op1 = -op1; break;
	default:	op1 = 0;
	}

	push(op,op1);
	return DONE;
}

static bool push_operator(STACK *stk,int op)
{
	if(script_expression_tostack[op] == STK_MAX) return FALSE;
	return push(stk,script_expression_tostack[op]);
}

static bool process_expession_stack(STACK *stk_op,STACK *stk_opr,int op)
{
	int t;

	if(op == CH_MAX) {
		//printf("invalid operator.\n");
		return FALSE;
	}

	// Iterate through the operators that CAN be popped off the stack
	while((t = script_expression_stack_action[op][top(stk_opr,STK_EMPTY)]) == POP &&
		(t = perform_operation(stk_op,stk_opr)) == DONE);

	if(t == DONE) return TRUE;
	else if(t == PUSH) {
		push_operator(stk_opr,op);
		return TRUE;
	} else if(t == DELETE) {
		--stk_opr->t;
		return TRUE;
	}
//	else if(t == ERROR1) printf("unmatched right parentheses.\n");
//	else if(t == ERROR2) printf("unmatched left parentheses.\n");
//	else if(t == ERROR3) printf("division by zero.\n");
//	else if(t == ERROR4) printf("insufficient operators/operands on stack.\n");

	return FALSE;
}

// Used by expand_variable to expand internal variable references into string components
// This will allow for things like...
// $<speed<oretype>>
//
// If 'oretype' is 'iron'... it will translate into... speediron
char *expand_variable_recursive(SCRIPT_VARINFO *info, char *str,BUFFER *buffer)
{
//	char esc[MSL];
//	char msg[MSL*2];
	char buf[MSL];
	pVARIABLE var;
	pVARIABLE infovar = info ? *(info->var) : NULL;
/*
	{
		expand_escape2print(str,esc);
		sprintf(msg,"expand_variable_recursive->before = \"%s\"",esc);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}
*/
	BUFFER *name_buffer = new_buf();

	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,name_buffer);
			if(!str) return NULL;
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,name_buffer);
			if(!str) return NULL;
		} else
			add_buf_char(name_buffer, *str++);
	}

/*
	{
		expand_escape2print(str,esc);
		sprintf(msg,"expand_variable_recursive->after = \"%s\"",esc);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}
*/

	var = variable_get(infovar,buf_string(name_buffer));
	if(var) {
		if((var->type == VAR_STRING || var->type == VAR_STRING_S) && var->_.s && *var->_.s) {
			add_buf(buffer,var->_.s);
		} else if(var->type == VAR_INTEGER) {
			sprintf(buf, "%d", var->_.i);
			add_buf(buffer, buf);
		}
/*
	} else if(var) {
		char msg[MSL];
		sprintf(msg,"expand_variable_recursive() -> var '%s' is not a string or is empty\n\r", var->name);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		return NULL;
	} else {
		char msg[MSL];
		sprintf(msg,"expand_variable_recursive() -> no variable named '%s'\n\r", buf);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		return NULL;
*/
	}

	free_buf(name_buffer);

	return str+1;
}

char *expand_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,pVARIABLE *var)
{
	BUFFER *buffer;
	bool deref = FALSE;
	pVARIABLE v;

	//if(*str == ESCAPE_VARIABLE && wiznet_variables) {
	//	char msg[MSL];
	//	sprintf(msg,"expand_variable() -> *str IS ESCAPE_VARIABLE\n\r");
	//	wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	//}

	if(*str == '*') {
		deref = TRUE;
		++str;
	}

	buffer = new_buf();
	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,buffer);
			if(!str) {
				//if(wiznet_variables) {
				//	char msg[MSL];
				//	*p = 0;
				//	sprintf(msg,"expand_variable(\"%s\") -> NULL str RETURN\n\r", buf);
				//	wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
				//}
				free_buf(buffer);
				return NULL;
			}
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,buffer);
			if(!str)
			{
				free_buf(buffer);
				return NULL;
			}
		} else
			add_buf_char(buffer, *str++);
	}

	if(*str != ESCAPE_END) {
		//if(wiznet_variables) {
		//	char msg[MSL];
		//	sprintf(msg,"expand_variable(\"%s\") -> NO ESCAPE\n\r", buf);
		//	wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		//}
		free_buf(buffer);
		return NULL;
	}

	//if(wiznet_variables) {
	//	char msg[MSL];
	//	sprintf(msg,"expand_variable(\"%s\")\n\r", buf);
	//	wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	//}

	v = variable_get(vars,buf_string(buffer));
	if(deref) {
		while(v && v->type == VAR_VARIABLE)
			v = v->_.variable;
	}

	free_buf(buffer);
	*var = v;

	return str+1;
}

char *expand_name(SCRIPT_VARINFO *info,pVARIABLE vars,char *str,BUFFER *buffer)
{
	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,buffer);
			if(!str) return NULL;
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,buffer);
			if(!str) return NULL;
		} else
			add_buf_char(buffer, *str++);
	}

	if(*str != ESCAPE_END) return NULL;

	return str+1;
}

char *expand_number(char *str, long *num)
{
	char arg[MIL], *s = str, *p = arg;
	while(isdigit(*s)) *p++ = *s++;
 	*p = 0;
 	if(arg[0]) {
		str = s;
		*num = atol(arg);
	} else
		*num = -1;
	return str;
}

char *expand_ifcheck(SCRIPT_VARINFO *info,char *str,long *value)
{
	int xifc;
	char *rest;
	IFCHECK_DATA *ifc;
	bool valid;

	DBG2ENTRY3(PTR,info,PTR,str,PTR,value);

	if(str[0] >= ESCAPE_EXTRA && str[1] >= ESCAPE_EXTRA) {
		xifc = (((str[0] - ESCAPE_EXTRA)&0x3F) |
			(((str[1] - ESCAPE_EXTRA)&0x3F)<<6));	// 0 - 4095

		if(xifc >= 0 && xifc < CHK_MAXIFCHECKS) {
			ifc = &ifcheck_table[xifc];

			rest = ifcheck_get_value(info,ifc,str+2,value,&valid);
			if(rest && valid) {
				if( !ifc->numeric )
					*value = *value && 1;	// Make sure T/F is set values  1/0

//					strcpy(buf,"expand_ifcheck 1:");
//					for(xifc = 0; xifc < 20 && rest[xifc]; xifc++)
//						sprintf(buf + 15 + 3*xifc," %2.2X", rest[xifc]&0xFF);
//					wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
				rest = skip_whitespace(rest);
				if(*rest != ESCAPE_END) {
					DBG2EXITVALUE2(INVALID);
					return expand_skip(str);
				}
				++rest;

//					strcpy(buf,"expand_ifcheck 2:");
//					for(xifc = 0; xifc < 20 && rest[xifc]; xifc++)
//						sprintf(buf + 15 + 3*xifc," %2.2X", rest[xifc]&0xFF);
//					wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

				DBG2EXITVALUE1(PTR,rest);
				return rest;
			}
		}
	}

	*value = 0;
	DBG2EXITVALUE2(INVALID);
	return expand_skip(str);
}


char *expand_argument_expression(SCRIPT_VARINFO *info, char *str,int *num)
{
	STACK optr,opnd;
	pVARIABLE var;
	long value,op;
	bool expect = FALSE;	// FALSE = number/open, TRUE = operator/close
	char *p = str;
	pVARIABLE vars = info ? *(info->var) : NULL;

	opnd.t = optr.t = 0;

	if(info)
	while(*str && *str != ESCAPE_END) {
		str = skip_whitespace(str);
		if(isdigit(*str)) {	// Constant
			if(expect) {
				// Generate an error - missing an operator
				break;
			}
			str = expand_number(str,&value);
//			sprintf(buf,"expand_argument_expression: number = %d, %2.2X", value, *str&0xFF);
//			wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
			if(value < 0) {
				// Generate an error - terms in here cannot be negative!
				break;
			}
			if(!push(&opnd,value)) {
				// Generate an error - expression too complex, simplify
				break;
			}
			expect = TRUE;
		} else if(*str == ESCAPE_VARIABLE) {	// Variable.. must resolve to a numerical value
			if(expect) {
				// Generate an error - missing an operator
				break;
			}

			str = expand_variable(info,vars,str+1,&var);
			if(!str) {
				break;
			}
			if (var) {
				if(var->type == VAR_INTEGER)
					value = var->_.i;
				else if(var->type == VAR_STRING || var->type ==  VAR_STRING_S)
					value = is_number(var->_.s) ? atoi(var->_.s) : 0;
				else
					value = 0;
			} else
				value = 0;

//			value = (var && var->type == VAR_INTEGER) ? var->_.i : 0;
//			sprintf(buf,"expand_argument_expression: variable = %d, %2.2X", value, *str&0xFF);
//			wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
			if(!push(&opnd,value)) {
				// Generate an error - expression too complex, simplify
				break;
			}
			expect = TRUE;
		} else if(*str == ESCAPE_ENTITY) {		// $() ENTITY.. must resolve to a numerical value
			if(expect) {
				// Generate an error - missing an operator
				break;
			}

			SCRIPT_PARAM *arg = new_script_param();

			str = expand_argument_entity(info,str+1,arg);
			if(!str) {
				break;
			}

			if (arg->type == ENT_NUMBER)
				value = arg->d.num;
			else if (arg->type == ENT_STRING && is_number(arg->d.str))
				value = atoi(arg->d.str);
			else
				value = 0;

			free_script_param(arg);

			if(!push(&opnd,value)) {
				// Generate an error - expression too complex, simplify
				break;
			}

			expect = TRUE;
		} else if(*str == ESCAPE_EXPRESSION) {
			if(expect) {
				// Generate an error - missing an operator
				break;
			}

			str = expand_ifcheck(info,str+1,&value);
//			sprintf(buf,"expand_argument_expression: expression = %d, %2.2X", value, *str&0xFF);
//			wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
			DBG3MSG1("value = %d\n", value);
			if(!str) {
				// Generate an error - ifcheck had an error, it should report it
				break;
			}
			if(!push(&opnd,value)) {
				// Generate an error - expression too complex, simplify
				break;
			}
			expect = TRUE;
		} else {
//			sprintf(buf,"expand_argument_expression: operator = %c, %2.2X", *str, str[1]&0xFF);
//			wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

			switch(*str) {
			case '+': op = expect ? CH_ADD : CH_MAX; expect=FALSE; break;
			case '-': op = expect ? CH_SUB : CH_NEG; expect=FALSE; break;
			case '*': op = expect ? CH_MUL : CH_MAX; expect=FALSE; break;
			case '/': op = expect ? CH_DIV : CH_MAX; expect=FALSE; break;
			case '%': op = expect ? CH_MOD : CH_MAX; expect=FALSE; break;
			case ':': op = expect ? CH_RAND : CH_MAX; expect=FALSE; break;
			case '!': op = expect ? CH_MAX : CH_NOT; expect=FALSE; break;
			case '(': op = expect ? CH_MAX : CH_OPEN; expect=FALSE; break;
			case ')': op = expect ? CH_CLOSE : CH_MAX; expect=TRUE; break;
			default:  op = CH_MAX; break;
			}

			if(!process_expession_stack(&opnd,&optr,op)) {
				// Generate an error - use what was determined in function
				break;
			}

			++str;
		}
	}

	for(op = 0; op < opnd.t; op++) {
		DBG3MSG2("Operand %d: %d\n", op, opnd.s[op]);
	}

	if(!str || *str != ESCAPE_END || !process_expession_stack(&opnd,&optr,CH_EOS) || opnd.t > 1) {
		*num = 0;
		return expand_skip(str ? str : p);
	} else {
		*num = pop(&opnd,0);
		return str+1;
	}
}


char *expand_argument_variable(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	pVARIABLE var;

	str = expand_variable(info,(info?*(info->var):NULL),str,&var);
	if(!str) return NULL;

	if(!var) {
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
	} else {
		// If it's a variable, completely recurse out of it.
		while(var->type == ENT_VARIABLE)
		{
			var = var->_.variable;

			if(!var) return NULL;
		}

		switch(var->type) {
		default: arg->type = ENT_NUMBER; arg->d.num = 0; break;
		case VAR_INTEGER:	arg->type = ENT_NUMBER; arg->d.num = var->_.i; break;
		case VAR_STRING:
		case VAR_STRING_S:	arg->type = ENT_STRING; arg->d.str = var->_.s; break;
		case VAR_MOBILE:	arg->type = ENT_MOBILE; arg->d.mob = var->_.m; break;
		case VAR_OBJECT:	arg->type = ENT_OBJECT; arg->d.obj = var->_.o; break;
		case VAR_ROOM:		arg->type = ENT_ROOM; arg->d.room = var->_.r; break;
		case VAR_EXIT:
			arg->type = ENT_EXIT;
			arg->d.door.r = var->_.door.r;
			arg->d.door.door = var->_.door.door;
			break;
		case VAR_TOKEN:		arg->type = ENT_TOKEN; arg->d.token = var->_.t; break;
		case VAR_AREA:		arg->type = ENT_AREA; arg->d.area = var->_.a; break;
		case VAR_AREA_REGION:	arg->type = ENT_AREA_REGION; arg->d.aregion = var->_.ar; break;
		case VAR_SKILL:		arg->type = ENT_SKILL; arg->d.skill = var->_.skill; break;
		case VAR_SKILLINFO:
			arg->type = ENT_SKILLINFO;
			arg->d.sk.m = var->_.sk.owner;
			arg->d.sk.t = var->_.sk.token;
			arg->d.sk.skill = var->_.sk.skill;
			if(IS_VALID(var->_.sk.owner)) {
				arg->d.sk.mid[0] = var->_.sk.owner->id[0];
				arg->d.sk.mid[1] = var->_.sk.owner->id[1];
			} else
				arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			if(IS_VALID(var->_.sk.token)) {
				arg->d.sk.tid[0] = var->_.sk.token->id[0];
				arg->d.sk.tid[1] = var->_.sk.token->id[1];
			} else
				arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
			break;
		case VAR_SONG:		arg->type = ENT_SONG; arg->d.song = var->_.song; break;
		case VAR_AFFECT:	arg->type = ENT_AFFECT; arg->d.aff = var->_.aff; break;
		case VAR_BOOK_PAGE:	arg->type = ENT_BOOK_PAGE; arg->d.book_page = var->_.book_page; break;
		case VAR_FOOD_BUFF:	arg->type = ENT_FOOD_BUFF; arg->d.food_buff = var->_.food_buff; break;
		case VAR_COMPARTMENT:	arg->type = ENT_COMPARTMENT; arg->d.compartment = var->_.compartment; break;
		case VAR_SPELL:		arg->type = ENT_SPELL; arg->d.spell = var->_.spell; break;
		case VAR_LOCK_STATE:	arg->type = ENT_LOCK_STATE; arg->d.lockstate = var->_.lockstate; break;
		case VAR_LIQUID:	arg->type = ENT_LIQUID; arg->d.liquid = var->_.liquid; break;

		case VAR_CONNECTION:	arg->type = ENT_CONN; arg->d.conn = var->_.conn; break;

		case VAR_WILDS:		arg->type = ENT_WILDS; arg->d.wilds = var->_.wilds; break;

		case VAR_WILDS_ID:	arg->type = ENT_WILDS_ID; arg->d.wid = var->_.wid; break;

		case VAR_CHURCH:	arg->type = ENT_CHURCH; arg->d.church = var->_.church; break;

		case VAR_CHURCH_ID:	arg->type = ENT_CHURCH_ID; arg->d.chid = var->_.chid; break;

		case VAR_CLONE_ROOM:
			arg->d.cr.r = var->_.cr.r;
			arg->d.cr.a = var->_.cr.a;
			arg->d.cr.b = var->_.cr.b;
			arg->type = ENT_CLONE_ROOM;
			break;

		case VAR_WILDS_ROOM:
			arg->d.wroom.wuid = var->_.wroom.wuid;
			arg->d.wroom.x = var->_.wroom.x;
			arg->d.wroom.y = var->_.wroom.y;
			arg->type = ENT_WILDS_ROOM;
			break;

		case VAR_CLONE_DOOR:
			arg->d.cdoor.r = var->_.cdoor.r;
			arg->d.cdoor.a = var->_.cdoor.a;
			arg->d.cdoor.b = var->_.cdoor.b;
			arg->d.cdoor.door = var->_.cdoor.door;
			arg->type = ENT_CLONE_DOOR;
			break;
		case VAR_WILDS_DOOR:
			arg->d.wdoor.wuid = var->_.wdoor.wuid;
			arg->d.wdoor.x = var->_.wdoor.x;
			arg->d.wdoor.y = var->_.wdoor.y;
			arg->d.wdoor.door = var->_.wdoor.door;
			arg->type = ENT_WILDS_DOOR;
			break;

		case VAR_MOBILE_ID:
			arg->d.uid[0] = var->_.mid.a;
			arg->d.uid[1] = var->_.mid.b;
			arg->type = ENT_MOBILE_ID;
			break;
		case VAR_OBJECT_ID:
			arg->d.uid[0] = var->_.oid.a;
			arg->d.uid[1] = var->_.oid.b;
			arg->type = ENT_OBJECT_ID;
			break;
		case VAR_TOKEN_ID:
			arg->d.uid[0] = var->_.tid.a;
			arg->d.uid[1] = var->_.tid.b;
			arg->type = ENT_TOKEN_ID;
			break;
		case VAR_AREA_ID:
			arg->d.aid = var->_.aid;
			arg->type = ENT_AREA_ID;
			break;
		case VAR_AREA_REGION_ID:
			arg->d.arid.aid = var->_.arid.aid;
			arg->d.arid.rid = var->_.arid.rid;
			arg->type = ENT_AREA_REGION_ID;
			break;

		case VAR_SKILLINFO_ID:
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.skill = var->_.skid.skill;
			arg->d.sk.mid[0] = var->_.skid.mid[0];
			arg->d.sk.mid[1] = var->_.skid.mid[1];
			arg->d.sk.tid[0] = var->_.skid.tid[0];
			arg->d.sk.tid[1] = var->_.skid.tid[1];
			arg->type = ENT_SKILLINFO_ID;
			break;

		case VAR_BLLIST_ROOM:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_ROOM; break;
		case VAR_BLLIST_MOB:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_MOB; break;
		case VAR_BLLIST_OBJ:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_OBJ; break;
		case VAR_BLLIST_TOK:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_TOK; break;
		case VAR_BLLIST_EXIT:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_EXIT; break;
		case VAR_BLLIST_SKILL:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_SKILL; break;
		case VAR_BLLIST_AREA:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_AREA; break;
		case VAR_BLLIST_AREA_REGION:	arg->d.blist = var->_.list;	arg->type = ENT_BLLIST_AREA_REGION; break;
		case VAR_BLLIST_WILDS:	arg->d.blist = var->_.list; arg->type = ENT_BLLIST_WILDS; break;

		case VAR_PLLIST_STR:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_STR; break;
		case VAR_PLLIST_CONN:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_CONN; break;
		case VAR_PLLIST_ROOM:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_ROOM; break;
		case VAR_PLLIST_MOB:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_MOB; break;
		case VAR_PLLIST_OBJ:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_OBJ; break;
		case VAR_PLLIST_TOK:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_TOK; break;
		case VAR_PLLIST_AREA:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_AREA; break;
		case VAR_PLLIST_AREA_REGION:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_AREA_REGION; break;
		case VAR_PLLIST_CHURCH:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_CHURCH; break;
		case VAR_PLLIST_BOOK_PAGE: arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_BOOK_PAGE; break;
		case VAR_PLLIST_FOOD_BUFF: arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_FOOD_BUFF; break;
		case VAR_PLLIST_COMPARTMENT: arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_COMPARTMENT; break;

		}
	}
	return str;

}

void expand_argument_simple_code(SCRIPT_VARINFO *info,unsigned char code,SCRIPT_PARAM *arg)
{
	arg->type = ENT_NUMBER;
	arg->d.num = 0;

	switch(code) {
	case ESCAPE_LI:
		if(info->mob) {
			arg->type = ENT_MOBILE;
			arg->d.mob = info->mob;
		} else if(info->obj) {
			arg->type = ENT_OBJECT;
			arg->d.obj = info->obj;
		} else if(info->room) {
			arg->type = ENT_ROOM;
			arg->d.room = info->room;
		} else if(info->token) {
			arg->type = ENT_TOKEN;
			arg->d.token = info->token;
		}
		break;
	case ESCAPE_LN:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->ch;
		break;
	case ESCAPE_LO:
		arg->type = ENT_OBJECT;
		arg->d.obj = info->obj1;
		break;
	case ESCAPE_LP:
		arg->type = ENT_OBJECT;
		arg->d.obj = info->obj2;
		break;
	case ESCAPE_LQ:
		arg->type = ENT_MOBILE;
		arg->d.mob = (info->targ && (*info->targ)) ? *info->targ : NULL;
		break;
	case ESCAPE_LR:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->rch ? info->rch : get_random_char(info->mob, info->obj, info->room, info->token);
		break;
	case ESCAPE_LT:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->vch;
		break;
	}
}

char *expand_escape_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,SCRIPT_PARAM *arg)
{
	int type;
	pVARIABLE var, orig;
	str = expand_variable(info,vars,str,&var);
	if(!str) return NULL;

	// Descend down the rabbit hole, fully redirecting down
	orig = var;
//	while(var && var->type == VAR_VARIABLE)
//		var = var->_.variable;

	switch(*str) {
	case ENTITY_VAR_NUM:
		if(!var) arg->d.num = 0;
		else if(var->type == VAR_INTEGER)
			arg->d.num = var->_.i;
		else return NULL;

		arg->type = ENT_NUMBER;
		break;
	case ENTITY_VAR_STR:
		if(!var) arg->d.str = NULL;
		else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
			arg->d.str = var->_.s;
		else return NULL;

		if(!arg->d.str) arg->d.str = &str_empty[0];
		arg->type = ENT_STRING;
		break;

	case ENTITY_VAR_MOB:
		if(var) {
			if(var->type == VAR_MOBILE && var->_.m) {
				arg->d.mob = var->_.m;
				arg->type = ENT_MOBILE;
			} else if(var->type == VAR_MOBILE_ID) {
				arg->d.uid[0] = var->_.mid.a;
				arg->d.uid[1] = var->_.mid.b;
				arg->type = ENT_MOBILE_ID;
			} else
				return NULL;
		} else
			return NULL;

		break;
	case ENTITY_VAR_OBJ:
		if(var) {
			if(var->type == VAR_OBJECT && var->_.o) {
				arg->d.obj = var->_.o;
				arg->type = ENT_OBJECT;
			} else if(var->type == VAR_OBJECT_ID) {
				arg->d.uid[0] = var->_.oid.a;
				arg->d.uid[1] = var->_.oid.b;
				arg->type = ENT_OBJECT_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;
	case ENTITY_VAR_ROOM:
		if(var) {
			if(var->type == VAR_ROOM) {
				arg->d.room = var->_.r;
				arg->type = ENT_ROOM;
			} else if(var->type == VAR_CLONE_ROOM) {
				arg->d.cr.r = var->_.cr.r;
				arg->d.cr.a = var->_.cr.a;
				arg->d.cr.b = var->_.cr.b;
				arg->type = ENT_CLONE_ROOM;
			} else if(var->type == VAR_WILDS_ROOM) {
				arg->d.wroom.wuid = var->_.wroom.wuid;
				arg->d.wroom.x = var->_.wroom.x;
				arg->d.wroom.y = var->_.wroom.y;
				arg->type = ENT_WILDS_ROOM;
			} else
				return NULL;
		} else
			return NULL;
		break;
	case ENTITY_VAR_EXIT:
		if(var) {
			if(var->type == VAR_EXIT && var->_.door.r) {
				arg->d.door.r = var->_.door.r;
				arg->d.door.door = var->_.door.door;
				arg->type = ENT_EXIT;
			} else if(var->type == VAR_CLONE_DOOR) {
				arg->d.cdoor.r = var->_.cdoor.r;
				arg->d.cdoor.a = var->_.cdoor.a;
				arg->d.cdoor.b = var->_.cdoor.b;
				arg->d.cdoor.door = var->_.cdoor.door;
				arg->type = ENT_CLONE_DOOR;
			} else if(var->type == VAR_WILDS_DOOR) {
				arg->d.wdoor.wuid = var->_.wdoor.wuid;
				arg->d.wdoor.x = var->_.wdoor.x;
				arg->d.wdoor.y = var->_.wdoor.y;
				arg->d.wdoor.door = var->_.wdoor.door;
				arg->type = ENT_WILDS_DOOR;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_TOKEN:
		if(var) {
			if(var->type == VAR_TOKEN && var->_.t) {
				arg->d.token = var->_.t;
				arg->type = ENT_TOKEN;
			} else if(var->type == VAR_TOKEN_ID) {
				arg->d.uid[0] = var->_.tid.a;
				arg->d.uid[1] = var->_.tid.b;
				arg->type = ENT_TOKEN_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_AFFECT:
		if(var && var->type == VAR_AFFECT && var->_.aff)
			arg->d.aff = var->_.aff;
		else return NULL;

		arg->type = ENT_AFFECT;
		break;

	case ENTITY_VAR_FOOD_BUFF:
		if(var && var->type == VAR_FOOD_BUFF && var->_.food_buff)
			arg->d.food_buff = var->_.food_buff;
		else return NULL;

		arg->type = ENT_FOOD_BUFF;
		break;

	case ENTITY_VAR_BOOK_PAGE:
		if(var && var->type == VAR_BOOK_PAGE && var->_.book_page)
			arg->d.book_page = var->_.book_page;
		else return NULL;

		arg->type = ENT_BOOK_PAGE;
		break;

	case ENTITY_VAR_COMPARTMENT:
		if(var && var->type == VAR_COMPARTMENT && var->_.compartment)
			arg->d.compartment = var->_.compartment;
		else return NULL;

		arg->type = ENT_COMPARTMENT;
		break;

	case ENTITY_VAR_CONN:
		if(var && var->type == VAR_CONNECTION && var->_.conn)
			arg->d.conn = var->_.conn;
		else return NULL;

		arg->type = ENT_CONN;
		break;

	case ENTITY_VAR_AREA:
		if(var) {
			if(var->type == VAR_AREA && var->_.a) {
				arg->d.area = var->_.a;
				arg->type = ENT_AREA;
			} else if(var->type == VAR_AREA_ID) {
				arg->d.aid = var->_.aid;
				arg->type = ENT_AREA_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;
	
	case ENTITY_VAR_AREA_REGION:
		if (var) {
			if(var->type == VAR_AREA_REGION && IS_VALID(var->_.ar)) {
				arg->d.aregion = var->_.ar;
				arg->type = ENT_AREA_REGION;
			} else if (var->type == VAR_AREA_REGION_ID) {
				arg->d.arid.aid = var->_.arid.aid;
				arg->d.arid.rid = var->_.arid.rid;
				arg->type = ENT_AREA_REGION_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_WILDS:
		if(var) {
			if( var->type == VAR_WILDS && var->_.wilds) {
				arg->d.wilds = var->_.wilds;
				arg->type = ENT_WILDS;
			} else if(var->type == VAR_WILDS_ID) {
				arg->d.wid = var->_.wid;
				arg->type = ENT_WILDS_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_CHURCH:
		if(var) {
			if( var->type == VAR_CHURCH && var->_.wilds) {
				arg->d.church = var->_.church;
				arg->type = ENT_CHURCH;
			} else if(var->type == VAR_CHURCH_ID) {
				arg->d.chid = var->_.chid;
				arg->type = ENT_CHURCH_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_SKILL:
		if(var && var->type == VAR_SKILL)
			arg->d.skill = var->_.skill;
		else return NULL;

		arg->type = ENT_SKILL;
		break;

	case ENTITY_VAR_SONG:
		if(var && var->type == VAR_SONG)
			arg->d.song = var->_.song;
		else return NULL;

		arg->type = ENT_SONG;
		break;

	case ENTITY_VAR_SECTION:
		if(var && var->type == VAR_SECTION && IS_VALID(var->_.section))
			arg->d.section = var->_.section;
		else return NULL;

		arg->type = ENT_SECTION;
		break;

	case ENTITY_VAR_INSTANCE:
		if(var && var->type == VAR_INSTANCE && IS_VALID(var->_.instance))
			arg->d.instance = var->_.instance;
		else return NULL;

		arg->type = ENT_INSTANCE;
		break;

	case ENTITY_VAR_DUNGEON:
		if(var && var->type == VAR_DUNGEON && IS_VALID(var->_.dungeon))
			arg->d.dungeon= var->_.dungeon;
		else return NULL;

		arg->type = ENT_DUNGEON;
		break;

	case ENTITY_VAR_SHIP:
		if(var && var->type == VAR_SHIP && IS_VALID(var->_.ship))
			arg->d.ship = var->_.ship;
		else return NULL;

		arg->type = ENT_SHIP;
		break;

	case ENTITY_VAR_SKILLINFO:
		if(var) {
			if( var->type == VAR_SKILLINFO ) {
				arg->type = ENT_SKILLINFO;
				arg->d.sk.m = var->_.sk.owner;
				arg->d.sk.t = var->_.sk.token;
				arg->d.sk.skill = var->_.sk.skill;
				if(IS_VALID(var->_.sk.owner)) {
					arg->d.sk.mid[0] = var->_.sk.owner->id[0];
					arg->d.sk.mid[1] = var->_.sk.owner->id[1];
				} else
					arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
				if(IS_VALID(var->_.sk.token)) {
					arg->d.sk.tid[0] = var->_.sk.token->id[0];
					arg->d.sk.tid[1] = var->_.sk.token->id[1];
				} else
					arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
			} else if(var->type == VAR_SKILLINFO_ID) {
				arg->d.sk.m = NULL;
				arg->d.sk.t = NULL;
				arg->d.sk.skill = var->_.skid.skill;
				arg->d.sk.mid[0] = var->_.skid.mid[0];
				arg->d.sk.mid[1] = var->_.skid.mid[1];
				arg->d.sk.tid[0] = var->_.skid.tid[0];
				arg->d.sk.tid[1] = var->_.skid.tid[1];
				arg->type = ENT_SKILLINFO_ID;
			} else
				return NULL;
		} else
			return NULL;
		break;

	case ENTITY_VAR_PLLIST_STR:
	case ENTITY_VAR_PLLIST_CONN:
	case ENTITY_VAR_PLLIST_ROOM:
	case ENTITY_VAR_PLLIST_MOB:
	case ENTITY_VAR_PLLIST_OBJ:
	case ENTITY_VAR_PLLIST_TOK:
	case ENTITY_VAR_PLLIST_AREA:
	case ENTITY_VAR_PLLIST_AREA_REGION:
	case ENTITY_VAR_PLLIST_CHURCH:
	case ENTITY_VAR_PLLIST_BOOK_PAGE:
	case ENTITY_VAR_PLLIST_FOOD_BUFF:
	case ENTITY_VAR_PLLIST_COMPARTMENT:
	case ENTITY_VAR_PLLIST_VARIABLE:
		type = (int)*str + VAR_PLLIST_STR - ENTITY_VAR_PLLIST_STR;
		if(var && var->type == type && var->_.list)
			arg->d.blist = var->_.list;
		else return NULL;
		arg->type = type + ENTITY_VAR_PLLIST_STR - VAR_PLLIST_STR;
		break;

	case ENTITY_VAR_BLLIST_ROOM:
	case ENTITY_VAR_BLLIST_MOB:
	case ENTITY_VAR_BLLIST_OBJ:
	case ENTITY_VAR_BLLIST_TOK:
	case ENTITY_VAR_BLLIST_EXIT:
	case ENTITY_VAR_BLLIST_SKILL:
	case ENTITY_VAR_BLLIST_AREA:
	case ENTITY_VAR_BLLIST_AREA_REGION:
	case ENTITY_VAR_BLLIST_WILDS:
		type = (int)*str + VAR_BLLIST_ROOM - ENTITY_VAR_BLLIST_ROOM;
		if(var && var->type == type && var->_.list)
			arg->d.blist = var->_.list;
		else return NULL;
		arg->type = type + ENTITY_VAR_BLLIST_ROOM - VAR_BLLIST_ROOM;
		break;

	case ENTITY_VAR_VARIABLE:
		arg->d.variable = (orig && orig->type == VAR_VARIABLE) ? orig->_.variable : NULL;
		arg->type = ENT_VARIABLE;
		break;

	case ENTITY_VAR_MOBINDEX:
		arg->d.mobindex = (var && var->type == VAR_MOBINDEX) ? var->_.mindex : NULL;
		arg->type = ENT_MOBINDEX;
		break;

	case ENTITY_VAR_OBJINDEX:
		arg->d.objindex = (var && var->type == VAR_OBJINDEX) ? var->_.oindex : NULL;
		arg->type = ENT_OBJINDEX;
		break;

	case ENTITY_VAR_TOKENINDEX:
		arg->d.tokindex = (var && var->type == VAR_TOKENINDEX) ? var->_.tindex : NULL;
		arg->type = ENT_TOKENINDEX;
		break;

	case ENTITY_VAR_BLUEPRINT:
		arg->d.bp = (var && var->type == VAR_BLUEPRINT) ? var->_.bp : NULL;
		arg->type = ENT_BLUEPRINT;
		break;

	case ENTITY_VAR_BLUEPRINT_SECTION:
		arg->d.bs = (var && var->type == VAR_BLUEPRINT_SECTION) ? var->_.bs : NULL;
		arg->type = ENT_BLUEPRINT_SECTION;
		break;

	case ENTITY_VAR_DUNGEONINDEX:
		arg->d.dngindex = (var && var->type == VAR_DUNGEONINDEX) ? var->_.dngindex : NULL;
		arg->type = ENT_DUNGEONINDEX;
		break;

	case ENTITY_VAR_SHIPINDEX:
		arg->d.shipindex = (var && var->type == VAR_SHIPINDEX) ? var->_.shipindex : NULL;
		arg->type = ENT_SHIPINDEX;
		break;

	case ENTITY_VAR_SPELL:
		arg->d.spell = (var && var->type == VAR_SPELL) ? var->_.spell : NULL;
		arg->type = ENT_SPELL;
		break;
	
	case ENTITY_VAR_LIQUID:
		arg->d.liquid = (var && var->type == VAR_LIQUID) ? var->_.liquid : NULL;
		arg->type = ENT_LIQUID;
		break;
	
	case ENTITY_VAR_LOCK_STATE:
		arg->d.lockstate = (var && var->type == VAR_LOCK_STATE) ? var->_.lockstate : NULL;
		arg->type = ENT_LOCK_STATE;
		break;
	}
	return str;
}

char *expand_entity_primary(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_EXPRESSION:
		arg->type = ENT_NUMBER;
		return expand_argument_expression(info,str+1,&arg->d.num);

	case ENTITY_ENACTOR:
		arg->type = ENT_MOBILE;
		arg->d.mob = info ? info->ch : NULL;
		break;
	case ENTITY_OBJ1:
		arg->type = ENT_OBJECT;
		arg->d.obj = info ? info->obj1 : NULL;
		break;
	case ENTITY_OBJ2:
		arg->type = ENT_OBJECT;
		arg->d.obj = info ? info->obj2 : NULL;
		break;
	case ENTITY_VICTIM:
		arg->type = ENT_MOBILE;
		arg->d.mob = info ? info->vch : NULL;
		break;
	case ENTITY_VICTIM2:
		arg->type = ENT_MOBILE;
		arg->d.mob = info ? info->vch2 : NULL;
		break;
	case ENTITY_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = (info && info->targ) ? *info->targ : NULL;
		break;
	case ENTITY_RANDOM:
		arg->type = ENT_MOBILE;
		// Check that the random character is defined, if not.. pick one
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		arg->d.mob = info ? info->rch : NULL;
		break;
	case ENTITY_HERE:
		arg->d.room = NULL;
		if(info) {
			if(info->mob)
				arg->d.room = info->mob->in_room;
			else if(info->obj)
				arg->d.room = obj_room(info->obj);
			else if(info->room)
				arg->d.room = info->room;
			else if(info->token)
				arg->d.room = token_room(info->token);
		}
		arg->type = ENT_ROOM;
		break;
	case ENTITY_SELF:
		if(info->mob) {
			arg->type = ENT_MOBILE;
			arg->d.mob = info->mob;
		} else if(info->obj) {
			arg->type = ENT_OBJECT;
			arg->d.obj = info->obj;
		} else if(info->room) {
			arg->type = ENT_ROOM;
			arg->d.room = info->room;
		} else if(info->token) {
			arg->type = ENT_TOKEN;
			arg->d.token = info->token;
		} else if(info->area) {
			arg->type = ENT_AREA;
			arg->d.area = info->area;
		} else if(info->instance) {
			arg->type = ENT_INSTANCE;
			arg->d.instance = info->instance;
		} else if(info->dungeon) {
			arg->type = ENT_DUNGEON;
			arg->d.dungeon = info->dungeon;
		} else return NULL;
		break;
	case ENTITY_PHRASE:
		arg->type = ENT_STRING;
		arg->d.str = info->phrase;
		break;
	case ENTITY_TRIGGER:
		arg->type = ENT_STRING;
		arg->d.str = info->trigger;
		break;
	case ENTITY_PRIOR:
		arg->type = ENT_PRIOR;
		arg->d.info = script_get_prior(info);
		break;

	case ENTITY_GAME:
		arg->type = ENT_GAME;
		break;

	case ENTITY_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = info->tok;
		break;

	case ENTITY_REGISTER1:
	case ENTITY_REGISTER2:
	case ENTITY_REGISTER3:
	case ENTITY_REGISTER4:
	case ENTITY_REGISTER5:
		arg->type = ENT_NUMBER;
		arg->d.num = info->registers[*str-ENTITY_REGISTER1];
		break;

	case ENTITY_MXP:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf_char(arg->buffer, '\t');
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_TRUE:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = true;
		break;

	case ENTITY_FALSE:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = false;
		break;

	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,(info?*(info->var):NULL),str+1,arg);
		if(!str) return NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_game(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_GAME_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, "Sentience");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_GAME_PORT:
		arg->type = ENT_NUMBER;
		arg->d.num = port;
		break;

	case ENTITY_GAME_PLAYERS:
		arg->type = ENT_PLLIST_CONN;
		arg->d.blist = conn_players;
		break;
	case ENTITY_GAME_IMMORTALS:
		arg->type = ENT_PLLIST_CONN;
		arg->d.blist = conn_immortals;
		break;
	case ENTITY_GAME_ONLINE:
		arg->type = ENT_PLLIST_CONN;
		arg->d.blist = conn_online;
		break;
	case ENTITY_GAME_PERSIST:
		arg->type = ENT_PERSIST;
		break;
	case ENTITY_GAME_AREAS:
		arg->type = ENT_BLLIST_AREA;
		arg->d.blist = loaded_areas;
		break;
	case ENTITY_GAME_WILDS:
		arg->type = ENT_BLLIST_WILDS;
		arg->d.blist = loaded_wilds;
		break;

	case ENTITY_GAME_CHURCHES:
		arg->type = ENT_PLLIST_CHURCH;
		arg->d.blist = list_churches;
		break;

	case ENTITY_GAME_SHIPS:
		arg->type = ENT_ILLIST_SHIPS;
		arg->d.blist = loaded_ships;
		break;

	case ENTITY_GAME_RELIC_POWER:
		arg->type = ENT_OBJECT;
		arg->d.obj = damage_relic;
		break;

	case ENTITY_GAME_RELIC_KNOWLEDGE:
		arg->type = ENT_OBJECT;
		arg->d.obj = xp_relic;
		break;

	case ENTITY_GAME_RELIC_LOSTSOULS:
		arg->type = ENT_OBJECT;
		arg->d.obj = pneuma_relic;
		break;

	case ENTITY_GAME_RELIC_HEALTH:
		arg->type = ENT_OBJECT;
		arg->d.obj = hp_regen_relic;
		break;

	case ENTITY_GAME_RELIC_MAGIC:
		arg->type = ENT_OBJECT;
		arg->d.obj = mana_regen_relic;
		break;

	case ENTITY_GAME_RESERVED_MOBILE:
		arg->type = ENT_RESERVED_MOBILE;
		break;

	case ENTITY_GAME_RESERVED_OBJECT:
		arg->type = ENT_RESERVED_OBJECT;
		break;

	case ENTITY_GAME_RESERVED_ROOM:
		arg->type = ENT_RESERVED_ROOM;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_persist(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_PERSIST_MOBS:
		arg->d.blist = persist_mobs;
		arg->type = ENT_PLLIST_MOB;
		break;
	case ENTITY_PERSIST_OBJS:
		arg->d.blist = persist_objs;
		arg->type = ENT_PLLIST_OBJ;
		break;
	case ENTITY_PERSIST_ROOMS:
		arg->d.blist = persist_rooms;
		arg->type = ENT_PLLIST_ROOM;
		break;
	default: return NULL;
	}
	return str+1;
}

char *expand_entity_wilds(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_WILDS_NAME:
		arg->type = ENT_STRING;
		if(arg->d.wilds) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.wilds->name);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_WILDS_WIDTH:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.wilds) ? arg->d.wilds->map_size_x : 0;
		break;

	case ENTITY_WILDS_HEIGHT:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.wilds) ? arg->d.wilds->map_size_y : 0;
		break;

	case ENTITY_WILDS_VROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = (arg->d.wilds) ? arg->d.wilds->loaded_vrooms : NULL;
		break;

	case ENTITY_WILDS_VLINKS:
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_wilds_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_WILDS_NAME:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_WILDS_WIDTH:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;

	case ENTITY_WILDS_HEIGHT:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;

	case ENTITY_WILDS_VROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;

	case ENTITY_WILDS_VLINKS:
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_church(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_CHURCH_NAME:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->name && arg->d.church->name[0] ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->name);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_SIZE:
		arg->type = ENT_STRING;
		if( arg->d.church ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, get_chsize_from_number(arg->d.church->size));
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];

		break;

	case ENTITY_CHURCH_FLAG:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->flag && arg->d.church->flag[0] ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->flag);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_FOUNDER:
		arg->type = ENT_MOBILE;
		arg->d.mob = ( arg->d.church ) ? get_player(arg->d.church->founder) : NULL;
		break;

	case ENTITY_CHURCH_FOUNDER_NAME:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->founder && arg->d.church->founder[0] ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->founder);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_MOTD:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->motd && arg->d.church->motd[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_MOTD) || script_security >= MAX_SCRIPT_SECURITY)) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->motd);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_RULES:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->rules && arg->d.church->rules[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_RULES) || script_security >= MAX_SCRIPT_SECURITY) ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->rules);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_INFO:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->info && arg->d.church->info[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_INFO) || script_security >= MAX_SCRIPT_SECURITY) ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.church->info);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.church && location_isset(&arg->d.church->recall_point)) ? location_to_room(&arg->d.church->recall_point) : NULL;
		break;
	case ENTITY_CHURCH_TREASURE:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = (arg->d.church) ? arg->d.church->treasure_rooms : NULL;
		break;
	case ENTITY_CHURCH_KEY:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.church && (script_security >= MAX_SCRIPT_SECURITY)) ? arg->d.church->key : 0;
		break;
	case ENTITY_CHURCH_ONLINE:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = (arg->d.church) ? arg->d.church->online_players : NULL;
		break;
	case ENTITY_CHURCH_ROSTER:
		arg->type = ENT_PLLIST_STR;
		arg->d.blist = (arg->d.church) ? arg->d.church->roster : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_church_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_CHURCH_NAME:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_SIZE:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_FLAG:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_FOUNDER:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ENTITY_CHURCH_MOTD:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_RULES:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_INFO:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_CHURCH_TREASURE:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;
	case ENTITY_CHURCH_KEY:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	case ENTITY_CHURCH_ONLINE:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = NULL;
		break;
	case ENTITY_CHURCH_ROSTER:
		arg->type = ENT_PLLIST_STR;
		arg->d.blist = NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_variable(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_VARIABLE_NAME:
		arg->type = ENT_STRING;
		if( arg->d.variable && arg->d.variable->name && arg->d.variable->name[0] ) {
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.variable->name);
			arg->d.str = buf_string(arg->buffer);
		} else
			arg->d.str = &str_empty[0];

		break;
	case ENTITY_VARIABLE_TYPE:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_VARIABLE_SAVE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.variable && arg->d.variable->save) ? 1 : 0;
		break;
	}

	return str+1;
}

char *expand_entity_boolean(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_BOOLEAN_TRUE_FALSE:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.boolean ? "true" : "false";
		break;

	case ENTITY_BOOLEAN_YES_NO:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.boolean ? "yes" : "no";
		break;

	case ENTITY_BOOLEAN_ON_OFF:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.boolean ? "on" : "off";
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_number(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_NUM_ABS:
		arg->d.num = abs(arg->d.num);
		break;

	case ENTITY_NUM_PADLEFT:
		{
			char num[MIL];
			sprintf(num, "%d", arg->d.num);
			int len = strlen(num);
			int padding = *(++str) - ESCAPE_EXTRA;

			clear_buf(arg->buffer);
			if( len < padding )
			{
				char buf[MIL + padding + 1];
				sprintf(buf, "%-*.*s", padding, padding, num);
				add_buf(arg->buffer, buf);
			}
			else
			{
				add_buf(arg->buffer, num);
			}
			arg->d.str = arg->buffer->string;
			arg->type = ENT_STRING;
		}
		break;

	case ENTITY_NUM_PADRIGHT:
		{
			char num[MIL];
			sprintf(num, "%d", arg->d.num);
			int len = strlen(num);
			int padding = *(++str) - ESCAPE_EXTRA;

			clear_buf(arg->buffer);
			if( len < padding )
			{
				char buf[MIL + padding + 1];
				sprintf(buf, "%*.*s", padding, padding, num);
				add_buf(arg->buffer, buf);
			}
			else
			{
				add_buf(arg->buffer, num);
			}
			arg->d.str = arg->buffer->string;
			arg->type = ENT_STRING;
		}
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_string(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
//	char buf[MSL];
	char *a;

//	sprintf(buf,"expand_entity_string: '%s', %d", arg->d.str ? arg->d.str : "(null)", *str);
//	wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);

	switch(*str) {
	case ENTITY_STR_LEN:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.str ? strlen(arg->d.str) : 0;
		break;
	case ENTITY_STR_LOWER:
		if( arg->d.str == buf_string(arg->buffer) )
		{
			// Already in the buffer
			for( a = arg->d.str; *a; a++)
				*a = LOWER(*a);
		}
		else
		{
			clear_buf(arg->buffer);
			for(a = arg->d.str; *a; a++)
				add_buf_char(arg->buffer, LOWER(*a));

			arg->d.str = buf_string(arg->buffer);
		}
		break;
	case ENTITY_STR_UPPER:
		if( arg->d.str == buf_string(arg->buffer) )
		{
			// Already in the buffer
			for( a = arg->d.str; *a; a++)
				*a = UPPER(*a);
		}
		else
		{
			clear_buf(arg->buffer);
			for(a = arg->d.str; *a; a++)
				add_buf_char(arg->buffer, UPPER(*a));

			arg->d.str = buf_string(arg->buffer);
		}
		break;
	case ENTITY_STR_CAPITAL:
//		sprintf(buf,"ENTITY_STR_CAPITAL: before '%s'", arg->d.str);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);

		if (arg->d.str != buf_string(arg->buffer))
		{
			clear_buf(arg->buffer);
			add_buf(arg->buffer, arg->d.str);
			arg->d.str = buf_string(arg->buffer);
		}
		/*
		if( arg->d.str == buf_string(arg->buffer) )
		{
			// Already in the buffer
			for( a = arg->d.str; *a; a++)
				*a = LOWER(*a);
		}
		else
		{
			clear_buf(arg->buffer);
			for(a = arg->d.str; *a; a++)
				add_buf_char(arg->buffer, LOWER(*a));

			arg->d.str = buf_string(arg->buffer);
		}
		*/
		arg->d.str[0] = UPPER(arg->d.str[0]);

//		sprintf(buf,"ENTITY_STR_CAPITAL: after '%s'", arg->d.str);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);
		break;

	case ENTITY_STR_PADLEFT:
		if( arg->d.str )
		{
			int len = strlen(arg->d.str);
			int padding = *(++str) - ESCAPE_EXTRA;

			// Replace string
			if( len < padding )
			{
				char buf[padding + 1];
				sprintf(buf, "%-*.*s", padding, padding, arg->d.str);
				clear_buf(arg->buffer);
				add_buf(arg->buffer, buf);
				arg->d.str = arg->buffer->string;
			}

		}
		else
		{
			int padding = *(++str) - ESCAPE_EXTRA;
			char buf[padding + 1];
			sprintf(buf, "%-*.*s", padding, padding, " ");
			clear_buf(arg->buffer);
			add_buf(arg->buffer, buf);
			arg->d.str = arg->buffer->string;
		}
		break;

	case ENTITY_STR_PADRIGHT:
		if( arg->d.str )
		{
			int len = strlen(arg->d.str);
			int padding = *(++str) - ESCAPE_EXTRA;

			// Replace string
			if( len < padding )
			{
				char buf[padding + 1];
				sprintf(buf, "%*.*s", padding, padding, arg->d.str);
				clear_buf(arg->buffer);
				add_buf(arg->buffer, buf);
				arg->d.str = arg->buffer->string;
			}

		}
		else
		{
			int padding = *(++str) - ESCAPE_EXTRA;
			char buf[padding + 1];
			sprintf(buf, "%*.*s", padding, padding, " ");
			clear_buf(arg->buffer);
			add_buf(arg->buffer, buf);
			arg->d.str = arg->buffer->string;
		}
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_widevnum(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	WNUM wnum = arg->d.wnum;

	switch(*str) {
	case ENTITY_WNUM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = wnum.pArea;
		break;
	case ENTITY_WNUM_VNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = wnum.vnum;
		break;
	case ENTITY_WNUM_MOBINDEX:
		arg->type = ENT_MOBINDEX;
		arg->d.mobindex = get_mob_index_wnum(wnum);
		break;
	case ENTITY_WNUM_OBJINDEX:
		arg->type = ENT_OBJINDEX;
		arg->d.objindex = get_obj_index_wnum(wnum);
		break;
	case ENTITY_WNUM_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = get_room_index_wnum(wnum);
		break;
	case ENTITY_WNUM_TOKENINDEX:
		arg->type = ENT_TOKENINDEX;
		arg->d.tokindex = get_token_index_wnum(wnum);
		break;
	case ENTITY_WNUM_BLUEPRINT:
		arg->type = ENT_BLUEPRINT;
		arg->d.bp = get_blueprint_wnum(wnum);
		break;
	case ENTITY_WNUM_BLUEPRINT_SECTION:
		arg->type = ENT_BLUEPRINT_SECTION;
		arg->d.bs = get_blueprint_section_wnum(wnum);
		break;
	case ENTITY_WNUM_DUNGEONINDEX:
		arg->type = ENT_DUNGEONINDEX;
		arg->d.dngindex = get_dungeon_index_wnum(wnum);
		break;
	case ENTITY_WNUM_SHIPINDEX:
		arg->type = ENT_SHIPINDEX;
		arg->d.shipindex = get_ship_index_wnum(wnum);
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_mobile(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	CHAR_DATA *self = arg->d.mob;
	char *p;
	switch(*str) {
	case ENTITY_MOB_NAME:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)self->name : (char*)SOMEONE;
		break;
	case ENTITY_MOB_SHORT:
		arg->type = ENT_STRING;
		p = self ? (char*)((IS_NPC(self) || self->morphed) ? self->short_descr : capitalize(self->name)) : (char*)"no one";
		clear_buf(arg->buffer);
		add_buf(arg->buffer, p);
		arg->d.str = buf_string(arg->buffer);
		break;
	case ENTITY_MOB_LONG:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)self->long_descr : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SEX:
		arg->type = ENT_STAT;
		arg->d.stat.value = self ? self->sex : SEX_NEUTRAL;
		arg->d.stat.table = sex_table;
		arg->d.stat.def_value = -1;
		break;
	case ENTITY_MOB_HE:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)he_she[URANGE(0,self->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIM:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)him_her[URANGE(0,self->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIS:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)his_her[URANGE(0,self->sex,2)] : (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIS_O:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)his_hers[URANGE(0,self->sex,2)] : (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIMSELF:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)himself[URANGE(0,self->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_RACE:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)race_table[self->race].name : "unknown";
		break;
	case ENTITY_MOB_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = self ? self->in_room : NULL;
		break;
	case ENTITY_MOB_HOUSE:
		arg->type = ENT_ROOM;
		if( self )
		{
			if( IS_NPC(self) )
				arg->d.room = self->home_room;
			else if( self->home.pArea && self->home.vnum > 0 )
				arg->d.room = get_room_index(self->home.pArea, self->home.vnum);
		}
		else
			arg->d.room = NULL;
		break;
	case ENTITY_MOB_CARRYING:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = self ? &self->carrying : NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_MOBILE;
		break;
	case ENTITY_MOB_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = self ? &self->tokens : NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_MOBILE;
		break;
	case ENTITY_MOB_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = self ? &self->affected : NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_MOBILE;
		break;
	case ENTITY_MOB_MOUNT:
		arg->d.mob = self ? self->mount : NULL;
		break;
	case ENTITY_MOB_RIDER:
		arg->d.mob = self ? self->rider : NULL;
		break;
	case ENTITY_MOB_MASTER:
		arg->d.mob = self ? self->master : NULL;
		break;
	case ENTITY_MOB_LEADER:
		arg->d.mob = self ? self->leader : NULL;
		break;
	case ENTITY_MOB_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = self && self->owner ? self->owner : &str_empty[0];
		break;
	case ENTITY_MOB_OPPONENT:
		arg->d.mob = self ? self->fighting : NULL;
		break;
	case ENTITY_MOB_CART:
		arg->type = ENT_OBJECT;
		arg->d.obj = self ? self->pulled_cart : NULL;
		break;
	case ENTITY_MOB_FURNITURE:
		arg->type = ENT_OBJECT;
		arg->d.obj = self ? self->on : NULL;
		break;
	case ENTITY_MOB_TARGET:
		arg->d.mob = (self && self->progs) ? self->progs->target : NULL;
		break;
	case ENTITY_MOB_HUNTING:
		arg->d.mob = self ? self->hunting : NULL;
		break;
	case ENTITY_MOB_AREA:
		arg->type = ENT_AREA;
		arg->d.area = self && self->in_room ? self->in_room->area : NULL;
		break;
	case ENTITY_MOB_EQ_LIGHT:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LIGHT);
		break;
	case ENTITY_MOB_EQ_FINGER1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_FINGER_L);
		break;
	case ENTITY_MOB_EQ_FINGER2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_FINGER_R);
		break;
	case ENTITY_MOB_EQ_NECK1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_NECK_1);
		break;
	case ENTITY_MOB_EQ_NECK2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_NECK_2);
		break;
	case ENTITY_MOB_EQ_BODY:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_BODY);
		break;
	case ENTITY_MOB_EQ_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_HEAD);
		break;
	case ENTITY_MOB_EQ_LEGS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LEGS);
		break;
	case ENTITY_MOB_EQ_FEET:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_FEET);
		break;
	case ENTITY_MOB_EQ_HANDS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_HANDS);
		break;
	case ENTITY_MOB_EQ_ARMS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_ARMS);
		break;
	case ENTITY_MOB_EQ_SHIELD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_SHIELD);
		break;
	case ENTITY_MOB_EQ_ABOUT:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_ABOUT);
		break;
	case ENTITY_MOB_EQ_WAIST:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_WAIST);
		break;
	case ENTITY_MOB_EQ_WRIST1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_WRIST_L);
		break;
	case ENTITY_MOB_EQ_WRIST2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_WRIST_R);
		break;
	case ENTITY_MOB_EQ_WIELD1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_WIELD);
		break;
	case ENTITY_MOB_EQ_HOLD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_HOLD);
		break;
	case ENTITY_MOB_EQ_WIELD2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_SECONDARY);
		break;
	case ENTITY_MOB_EQ_RING:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_RING_FINGER);
		break;
	case ENTITY_MOB_EQ_BACK:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_BACK);
		break;
	case ENTITY_MOB_EQ_SHOULDER:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_SHOULDER);
		break;
	case ENTITY_MOB_EQ_ANKLE1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_ANKLE_L);
		break;
	case ENTITY_MOB_EQ_ANKLE2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_ANKLE_R);
		break;
	case ENTITY_MOB_EQ_EAR1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_EAR_L);
		break;
	case ENTITY_MOB_EQ_EAR2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_EAR_R);
		break;
	case ENTITY_MOB_EQ_EYES:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_EYES);
		break;
	case ENTITY_MOB_EQ_FACE:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_FACE);
		break;
	case ENTITY_MOB_EQ_TATTOO_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_HEAD);
		break;
	case ENTITY_MOB_EQ_TATTOO_TORSO:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_TORSO);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_UPPER_ARM_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_UPPER_ARM_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_UPPER_LEG_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_UPPER_LEG_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_LOWER_ARM1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_LOWER_ARM_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_LOWER_ARM2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_LOWER_ARM_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_LOWER_LEG1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_LOWER_LEG_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_LOWER_LEG2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_LOWER_LEG_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_SHOULDER1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_SHOULDER_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_SHOULDER2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_SHOULDER_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_BACK:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_BACK);
		break;
	case ENTITY_MOB_EQ_TATTOO_NECK:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_TATTOO_NECK);
		break;
	case ENTITY_MOB_EQ_LODGED_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_HEAD);
		break;
	case ENTITY_MOB_EQ_LODGED_TORSO:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_TORSO);
		break;
	case ENTITY_MOB_EQ_LODGED_ARM1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_ARM_L);
		break;
	case ENTITY_MOB_EQ_LODGED_ARM2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_ARM_R);
		break;
	case ENTITY_MOB_EQ_LODGED_LEG1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_LEG_L);
		break;
	case ENTITY_MOB_EQ_LODGED_LEG2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_LODGED_LEG_R);
		break;
	case ENTITY_MOB_EQ_ENTANGLED:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_ENTANGLED);
		break;
	case ENTITY_MOB_EQ_CONCEALED:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(self,WEAR_CONCEALED);
		break;
	case ENTITY_MOB_NEXT:
		arg->type = ENT_MOBILE;
		arg->d.mob = (self && self->in_room)?self->next_in_room:NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,(self && IS_NPC(self))?self->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_MOB_CASTSPELL:
		arg->type = ENT_SKILL;
		arg->d.skill = self ? self->cast_skill: NULL;
		break;
	case ENTITY_MOB_CASTTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = self ? self->cast_token: NULL;
		break;
	case ENTITY_MOB_CASTTARGET:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)self->cast_target_name : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SONG:
		arg->type = ENT_SONG;
		arg->d.song = self ? self->song : NULL;
		break;
	case ENTITY_MOB_SONGTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = self ? self->song_token: NULL;
		break;
	case ENTITY_MOB_SONGTARGET:
		arg->type = ENT_STRING;
		arg->d.str = self ? (char*)self->music_target_name : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_INSTRUMENT:
		arg->type = ENT_OBJECT;
		arg->d.obj = self ? self->song_instrument : NULL;
		break;
	case ENTITY_MOB_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = (self && location_isset(&self->recall)) ? location_to_room(&self->recall) : NULL;
		break;
	case ENTITY_MOB_CONNECTION:
		arg->type = ENT_CONN;
		arg->d.conn = self ? self->desc : NULL;	// Works for PCs and switched NPCs
		break;
	case ENTITY_MOB_CHURCH:
		arg->type = ENT_CHURCH;
		arg->d.church = (self && !IS_NPC(self)) ? self->church : NULL;	// Works for PCs and switched NPCs
		break;
	case ENTITY_MOB_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = self ? self->lclonerooms : NULL;
		break;
	case ENTITY_MOB_WORN:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = self ? self->lworn : NULL;
		break;
	case ENTITY_MOB_CHECKPOINT:
		arg->type = ENT_ROOM;
		arg->d.room = (self && !IS_NPC(self)) ? self->checkpoint : NULL;
		break;
	case ENTITY_MOB_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (self && self->progs) ? &self->progs->vars : NULL;
		break;
	case ENTITY_MOB_GROUP:
		arg->type = ENT_GROUP;
		arg->d.group_owner = self;
		break;

	case ENTITY_MOB_DAMAGEDICE:
		arg->type = ENT_DICE;
		arg->d.dice = self ? &self->damage : NULL;;
		break;

	case ENTITY_MOB_ACT:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = self ? self->act : NULL;
		arg->d.bm.bank = self ? (IS_NPC(self) ? act_flagbank : plr_flagbank) : NULL;
		break;

	case ENTITY_MOB_AFFECT:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = self ? self->affected_by : NULL;
		arg->d.bm.bank = self ? affect_flagbank : NULL;
		break;

	case ENTITY_MOB_OFF:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = (self && IS_NPC(self)) ? self->off_flags : 0;
		arg->d.bv.table = (self && IS_NPC(self)) ? off_flags : NULL;
		break;

	case ENTITY_MOB_IMMUNE:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = self ? self->imm_flags : 0;
		arg->d.bv.table = self ? imm_flags : NULL;
		break;

	case ENTITY_MOB_RESIST:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = self ? self->res_flags : 0;
		arg->d.bv.table = self ? res_flags : NULL;
		break;

	case ENTITY_MOB_VULN:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = self ? self->vuln_flags : 0;
		arg->d.bv.table = self ? vuln_flags : NULL;
		break;

	case ENTITY_MOB_TEMPSTRING:
		arg->type = ENT_STRING;
		arg->d.str = self && self->tempstring ? self->tempstring : &str_empty[0];
		break;
	
	case ENTITY_MOB_PMOUNT:
		arg->type = ENT_WIDEVNUM;
		if (IS_VALID(self) && !IS_NPC(self))
			arg->d.wnum = self->pcdata->personal_mount;
		else
		{
			arg->d.wnum.pArea = NULL;
			arg->d.wnum.vnum = 0;
		}
		break;

	case ENTITY_MOB_INDEX:
		arg->type = ENT_MOBINDEX;
		arg->d.mobindex = (self && IS_NPC(self)) ? self->pIndexData : NULL;
		break;

	case ENTITY_MOB_STACHE:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = self ? self->lstache : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_mobile_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_MOB_NAME:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONE;
		break;
	case ENTITY_MOB_SHORT:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, "no one");
		arg->d.str = buf_string(arg->buffer);
		break;
	case ENTITY_MOB_LONG:
		arg->type = ENT_STRING;
		arg->d.str = (char*)&str_empty[0];
		break;
	case ENTITY_MOB_FULLDESC:
		arg->type = ENT_STRING;
		arg->d.str = (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SEX:
		arg->type = ENT_STAT;
		arg->d.stat.value = SEX_NEUTRAL;
		arg->d.stat.table = sex_table;
		arg->d.stat.def_value = -1;
		break;
	case ENTITY_MOB_HE:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIM:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIS:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIS_O:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIMSELF:
		arg->type = ENT_STRING;
		arg->d.str = (char*)SOMEONE;
		break;
	case ENTITY_MOB_RACE:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, "unknown");
		arg->d.str = buf_string(arg->buffer);
		break;
	case ENTITY_MOB_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_MOB_HOUSE:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_MOB_CARRYING:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_MOB_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_MOB_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_MOB_MOUNT:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_RIDER:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_MASTER:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_LEADER:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_MOB_OPPONENT:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_CART:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;
	case ENTITY_MOB_FURNITURE:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;
	case ENTITY_MOB_TARGET:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_HUNTING:
		arg->d.mob = NULL;
		break;
	case ENTITY_MOB_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
		break;
	case ENTITY_MOB_EQ_LIGHT:
	case ENTITY_MOB_EQ_FINGER1:
	case ENTITY_MOB_EQ_FINGER2:
	case ENTITY_MOB_EQ_NECK1:
	case ENTITY_MOB_EQ_NECK2:
	case ENTITY_MOB_EQ_BODY:
	case ENTITY_MOB_EQ_HEAD:
	case ENTITY_MOB_EQ_LEGS:
	case ENTITY_MOB_EQ_FEET:
	case ENTITY_MOB_EQ_HANDS:
	case ENTITY_MOB_EQ_ARMS:
	case ENTITY_MOB_EQ_SHIELD:
	case ENTITY_MOB_EQ_ABOUT:
	case ENTITY_MOB_EQ_WAIST:
	case ENTITY_MOB_EQ_WRIST1:
	case ENTITY_MOB_EQ_WRIST2:
	case ENTITY_MOB_EQ_WIELD1:
	case ENTITY_MOB_EQ_HOLD:
	case ENTITY_MOB_EQ_WIELD2:
	case ENTITY_MOB_EQ_RING:
	case ENTITY_MOB_EQ_BACK:
	case ENTITY_MOB_EQ_SHOULDER:
	case ENTITY_MOB_EQ_ANKLE1:
	case ENTITY_MOB_EQ_ANKLE2:
	case ENTITY_MOB_EQ_EAR1:
	case ENTITY_MOB_EQ_EAR2:
	case ENTITY_MOB_EQ_EYES:
	case ENTITY_MOB_EQ_FACE:
	case ENTITY_MOB_EQ_TATTOO_HEAD:
	case ENTITY_MOB_EQ_TATTOO_TORSO:
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM1:
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM2:
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG1:
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG2:
	case ENTITY_MOB_EQ_TATTOO_BACK:
	case ENTITY_MOB_EQ_TATTOO_NECK:
        case ENTITY_MOB_EQ_TATTOO_LOWER_ARM1:
        case ENTITY_MOB_EQ_TATTOO_LOWER_ARM2:
        case ENTITY_MOB_EQ_TATTOO_LOWER_LEG1:
        case ENTITY_MOB_EQ_TATTOO_LOWER_LEG2:
        case ENTITY_MOB_EQ_TATTOO_SHOULDER1:
        case ENTITY_MOB_EQ_TATTOO_SHOULDER2:
        case ENTITY_MOB_EQ_LODGED_HEAD:
	case ENTITY_MOB_EQ_LODGED_TORSO:
	case ENTITY_MOB_EQ_LODGED_ARM1:
	case ENTITY_MOB_EQ_LODGED_ARM2:
	case ENTITY_MOB_EQ_LODGED_LEG1:
	case ENTITY_MOB_EQ_LODGED_LEG2:
	case ENTITY_MOB_EQ_ENTANGLED:
	case ENTITY_MOB_EQ_CONCEALED:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;
	case ENTITY_MOB_NEXT:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,NULL,str+1,arg);
		if(!str) return NULL;
		break;

	case ENTITY_MOB_CASTSPELL:
		arg->type = ENT_SKILL;
		arg->d.skill = NULL;
		break;
	case ENTITY_MOB_CASTTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = NULL;
		break;
	case ENTITY_MOB_CASTTARGET:
		arg->type = ENT_STRING;
		arg->d.str = (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SONG:
		arg->type = ENT_SONG;
		arg->d.song = NULL;
		break;
	case ENTITY_MOB_SONGTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = NULL;
		break;
	case ENTITY_MOB_SONGTARGET:
		arg->type = ENT_STRING;
		arg->d.str = (char*)&str_empty[0];
		break;
	case ENTITY_MOB_INSTRUMENT:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;

	case ENTITY_MOB_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_MOB_CONNECTION:
		arg->type = ENT_CONN;
		arg->d.conn = NULL;
		break;
	case ENTITY_MOB_CHURCH:
		arg->type = ENT_CHURCH;
		arg->d.church = NULL;
		break;
	case ENTITY_MOB_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;
	case ENTITY_MOB_WORN:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = NULL;
		break;
	case ENTITY_MOB_CHECKPOINT:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;

	case ENTITY_MOB_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = NULL;

	case ENTITY_MOB_GROUP:
		arg->type = ENT_GROUP;
		arg->d.mob = NULL;
		break;

	case ENTITY_MOB_DAMAGEDICE:
		arg->type = ENT_DICE;
		arg->d.dice = NULL;
		break;

	case ENTITY_MOB_ACT:
	case ENTITY_MOB_AFFECT:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = NULL;
		arg->d.bm.bank = NULL;
		break;

	case ENTITY_MOB_OFF:
	case ENTITY_MOB_IMMUNE:
	case ENTITY_MOB_RESIST:
	case ENTITY_MOB_VULN:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = NULL;
		break;

	case ENTITY_MOB_TEMPSTRING:
		arg->type = ENT_STRING;
		arg->d.str = (char*)&str_empty[0];
		break;
	
	case ENTITY_MOB_PMOUNT:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = NULL;
		arg->d.wnum.vnum = 0;
		break;

	case ENTITY_MOB_INDEX:
		arg->type = ENT_MOBINDEX;
		arg->d.mobindex = NULL;
		break;

	case ENTITY_MOB_STACHE:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *self = arg->d.obj;
	switch(*str) {
	case ENTITY_OBJ_NAME:
		arg->type = ENT_STRING;
		arg->d.str = self ? self->name : SOMETHING;
		break;
	case ENTITY_OBJ_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = self ? self->short_descr : SOMETHING;
		break;
	case ENTITY_OBJ_LONG:
		arg->type = ENT_STRING;
		arg->d.str = self ? self->description : &str_empty[0];
		break;
	case ENTITY_OBJ_FULLDESC:
		arg->type = ENT_STRING;
		arg->d.str = self ? self->full_description : &str_empty[0];
		break;
	case ENTITY_OBJ_CONTAINER:
		self = self ? self->in_obj : NULL;
		break;
	case ENTITY_OBJ_FURNITURE:
		self = self ? self->on : NULL;
		break;
	case ENTITY_OBJ_CONTENTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = self ? &self->contains : NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_OBJ_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = self && self->owner ? self->owner : &str_empty[0];
		break;
	case ENTITY_OBJ_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = self ? obj_room(self) : NULL;
		break;
	case ENTITY_OBJ_CARRIER:
		arg->type = ENT_MOBILE;
		arg->d.mob = self ? self->carried_by : NULL;
		break;
	case ENTITY_OBJ_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = self ? &self->tokens : NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_OBJ_TARGET:
		arg->d.mob = (self && self->progs) ? self->progs->target : NULL;
		break;
	case ENTITY_OBJ_AREA:
		arg->type = ENT_AREA;
		arg->d.room = self ? obj_room(self) : NULL;
		arg->d.area = arg->d.room ? arg->d.room->area : NULL;
		break;
	case ENTITY_OBJ_NEXT:
		arg->type = ENT_OBJECT;
		self = self?self->next_content:NULL;
		break;
	case ENTITY_OBJ_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		if( self )
		{
			if( self->extra_descr )
				arg->d.list.ptr.ed = &self->extra_descr;
			else
				arg->d.list.ptr.ed = &self->pIndexData->extra_descr;
		}
		else
			arg->d.list.ptr.ed = NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_OBJECT;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,self?self->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_OBJ_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = self ? self->lclonerooms : NULL;
		break;
	case ENTITY_OBJ_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = self ? &self->affected: NULL;
		arg->d.list.owner = self;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;

	case ENTITY_OBJ_INDEX:
		arg->type = ENT_OBJINDEX;
		arg->d.objindex = self ? self->pIndexData : NULL;
		break;

	case ENTITY_OBJ_EXTRA:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = self ? self->extra : NULL;
		arg->d.bm.bank = extra_flagbank;
		break;

	case ENTITY_OBJ_WEAR:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = self ? self->wear_flags : 0;
		arg->d.bv.table = wear_flags;
		break;

	case ENTITY_OBJ_SHIP:
		arg->type = ENT_SHIP;
		arg->d.ship = self ? self->ship : NULL;
		break;

	case ENTITY_OBJ_STACHE:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = self ? self->lstache : NULL;
		break;

	case ENTITY_OBJ_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (self && self->progs) ? &self->progs->vars : NULL;
		break;

	case ENTITY_OBJ_ISLOCKERED:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = self && self->locker;
		break;

	case ENTITY_OBJ_ISSTACHED:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = self && self->stached;
		break;

	// Multi-typing
	case ENTITY_OBJ_TYPE_BOOK:
		arg->type = ENT_OBJECT_BOOK;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_CONTAINER:
		arg->type = ENT_OBJECT_CONTAINER;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FLUID_CONTAINER:
		arg->type = ENT_OBJECT_FLUID_CONTAINER;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FOOD:
		arg->type = ENT_OBJECT_FOOD;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FURNITURE:
		arg->type = ENT_OBJECT_FURNITURE;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_INK:
		arg->type = ENT_OBJECT_INK;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_INSTRUMENT:
		arg->type = ENT_OBJECT_INSTRUMENT;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_LIGHT:
		arg->type = ENT_OBJECT_LIGHT;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_MONEY:
		arg->type = ENT_OBJECT_MONEY;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_PAGE:
		arg->type = ENT_BOOK_PAGE;
		arg->d.book_page = IS_VALID(self) ? PAGE(self) : NULL;
		break;

	case ENTITY_OBJ_TYPE_PORTAL:
		arg->type = ENT_OBJECT_PORTAL;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_SCROLL:
		arg->type = ENT_OBJECT_PORTAL;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_TATTOO:
		arg->type = ENT_OBJECT_TATTOO;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_WAND:
		arg->type = ENT_OBJECT_WAND;
		// Uses self
		break;

	// SPELLS?
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_OBJ_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMETHING;
		break;
	case ENTITY_OBJ_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = SOMETHING;
		break;
	case ENTITY_OBJ_LONG:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_OBJ_CONTAINER:
		arg->d.obj = NULL;
		break;
	case ENTITY_OBJ_FURNITURE:
		arg->d.obj = NULL;
		break;
	case ENTITY_OBJ_CONTENTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_OBJ_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_OBJ_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_OBJ_CARRIER:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ENTITY_OBJ_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_OBJ_TARGET:
		arg->d.mob = NULL;
		break;
	case ENTITY_OBJ_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
		break;
	case ENTITY_OBJ_NEXT:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;
	case ENTITY_OBJ_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.list.ptr.ed = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_OBJECT;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_OBJ_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;
	case ENTITY_OBJ_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;

	case ENTITY_OBJ_INDEX:
		arg->type = ENT_OBJINDEX;
		arg->d.objindex = NULL;
		break;

	case ENTITY_OBJ_EXTRA:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = NULL;
		arg->d.bm.bank = NULL;
		break;

	case ENTITY_OBJ_WEAR:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = NULL;
		break;


	case ENTITY_OBJ_SHIP:
		arg->type = ENT_SHIP;
		arg->d.ship = NULL;
		break;

	case ENTITY_OBJ_STACHE:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = NULL;
		break;

	case ENTITY_OBJ_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = NULL;
		break;

	// Multi-typing
	case ENTITY_OBJ_ISLOCKERED:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = false;
		break;

	case ENTITY_OBJ_ISSTACHED:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = false;
		break;

	// Multi-typing
	case ENTITY_OBJ_TYPE_BOOK:
		arg->type = ENT_OBJECT_BOOK;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_CONTAINER:
		arg->type = ENT_OBJECT_CONTAINER;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FLUID_CONTAINER:
		arg->type = ENT_OBJECT_FLUID_CONTAINER;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FOOD:
		arg->type = ENT_OBJECT_FOOD;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_FURNITURE:
		arg->type = ENT_OBJECT_FURNITURE;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_INK:
		arg->type = ENT_OBJECT_INK;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_INSTRUMENT:
		arg->type = ENT_OBJECT_INSTRUMENT;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_LIGHT:
		arg->type = ENT_OBJECT_LIGHT;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_MONEY:
		arg->type = ENT_OBJECT_MONEY;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_PAGE:
		arg->type = ENT_BOOK_PAGE;
		arg->d.book_page = NULL;
		break;

	case ENTITY_OBJ_TYPE_PORTAL:
		arg->type = ENT_OBJECT_PORTAL;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_SCROLL:
		arg->type = ENT_OBJECT_PORTAL;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_TATTOO:
		arg->type = ENT_OBJECT_TATTOO;
		// Uses self
		break;

	case ENTITY_OBJ_TYPE_WAND:
		arg->type = ENT_OBJECT_WAND;
		// Uses self
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	ROOM_INDEX_DATA *room = arg->d.room;
	switch(*str) {
	case ENTITY_ROOM_NAME:
		arg->type = ENT_STRING;
		arg->d.str = room ? room->name : SOMEWHERE;
		break;
	case ENTITY_ROOM_MOBILES:
		arg->type = ENT_OLLIST_MOB;
		arg->d.list.ptr.mob = room ? &room->people : NULL;
		arg->d.list.owner = room;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = room ? &room->contents : NULL;
		arg->d.list.owner = room;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = room ? &room->tokens : NULL;
		arg->d.list.owner = room;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = room ? room->area : NULL;
		break;
	case ENTITY_ROOM_REGION:
		arg->type = ENT_AREA_REGION;
		arg->d.aregion = room ? get_room_region(room) : NULL;
		break;
	case ENTITY_ROOM_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = (room && room->progs) ? room->progs->target : NULL;
		break;
	case ENTITY_ROOM_FLAGS:
		arg->type = ENT_BITMATRIX;
		arg->d.bm.values = room ? room->room_flag : NULL;
		arg->d.bm.bank = room_flagbank;
		break;

	case ENTITY_ROOM_EXITS:
		arg->type = ENT_ARRAY_EXITS;
		// Uses the room
		break;

	case ENTITY_ROOM_NORTH:
	case ENTITY_ROOM_EAST:
	case ENTITY_ROOM_SOUTH:
	case ENTITY_ROOM_WEST:
	case ENTITY_ROOM_UP:
	case ENTITY_ROOM_DOWN:
	case ENTITY_ROOM_NORTHEAST:
	case ENTITY_ROOM_NORTHWEST:
	case ENTITY_ROOM_SOUTHEAST:
	case ENTITY_ROOM_SOUTHWEST:
		arg->type = ENT_EXIT;
		arg->d.door.r = room;
		arg->d.door.door = *str - ENTITY_ROOM_NORTH + DIR_NORTH;
		break;
	case ENTITY_ROOM_ENVIRON:
		arg->type = ENT_ROOM;
		arg->d.room = room ? get_environment(room) : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = (room && room->environ_type == ENVIRON_ROOM) ? room->environ.room : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_MOB:
		arg->type = ENT_MOBILE;
		arg->d.mob = (room && room->environ_type == ENVIRON_MOBILE) ? room->environ.mob : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_OBJ:
		arg->type = ENT_OBJECT;
		arg->d.obj = (room && room->environ_type == ENVIRON_OBJECT) ? room->environ.obj : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_TOKEN:
		arg->type = ENT_OBJECT;
		arg->d.token = (room && room->environ_type == ENVIRON_TOKEN) ? room->environ.token : NULL;
		break;

	case ENTITY_ROOM_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.list.ptr.ed = room ? &room->extra_descr : NULL;
		arg->d.list.owner = room;
		arg->d.list.owner_type = ENT_ROOM;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = room ? room->description : "";
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = room ? room->lclonerooms : NULL;
		break;

	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,room?room->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_ROOM_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (room && room->progs) ? &room->progs->vars : NULL;
		break;
	case ENTITY_ROOM_SECTION:
		arg->type = ENT_SECTION;
		arg->d.section = (room && IS_VALID(room->instance_section)) ? room->instance_section : NULL;
		break;

	case ENTITY_ROOM_INSTANCE:
		arg->type = ENT_INSTANCE;
		arg->d.section = (room && IS_VALID(room->instance_section)) ? room->instance_section : NULL;
		arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
		break;

	case ENTITY_ROOM_DUNGEON:
		arg->type = ENT_DUNGEON;
		arg->d.section = (room && IS_VALID(room->instance_section)) ? room->instance_section : NULL;
		arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
		arg->d.dungeon = (arg->d.instance && IS_VALID(arg->d.instance->dungeon)) ? arg->d.instance->dungeon : NULL;
		break;

	case ENTITY_ROOM_SHIP:
		arg->type = ENT_SHIP;
		arg->d.section = (room && IS_VALID(room->instance_section)) ? room->instance_section : NULL;
		arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
		arg->d.ship = (arg->d.instance && IS_VALID(arg->d.instance->ship)) ? arg->d.instance->ship : NULL;
		break;

	case ENTITY_ROOM_SECTOR:
		arg->type = ENT_STAT;
		arg->d.stat.value = room ? room->sector_type : NO_FLAG;
		arg->d.stat.table = sector_flags;
		arg->d.stat.def_value = SECT_NONE;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_array_exits(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	ROOM_INDEX_DATA *room = arg->d.room;

	switch(*str)
	{
		case ESCAPE_EXPRESSION:
		{
			int door;
			str = expand_argument_expression(info,str+1,&door);
			if (!str) return NULL;

			arg->type = ENT_EXIT;
			arg->d.door.r = room;
			if (door >= 0 && door < MAX_DIR)
			{
				arg->d.door.door = door;
			}
			else
			{
				arg->d.door.door = MAX_DIR;
			}
			return str;
		}

		// Will get a random direction, regardless if it exists.
		case ENTITY_ARRAY_EXITS_RANDOM:
			arg->type = ENT_EXIT;
			arg->d.door.r = room;
			arg->d.door.door = number_range(0, MAX_DIR - 1);
			break;

		// Same as RANDOM, but will only pick the exit if it's valid;
		case ENTITY_ARRAY_EXITS_ANY:
		{
			arg->type = ENT_EXIT;
			arg->d.door.r = room;

			int dir = MAX_DIR;
			if (room)
			{
				int count = 0;
				for(int i = 0; i < MAX_DIR; i++)
					if (room->exit[i])
						count++;

				if (count > 0)
				{
					int door = number_range(1, count);
					
					for(int i = 0; i < MAX_DIR; i++)
					{
						if (room->exit[i] && !--door)
						{
							dir = i;
							break;
						}
					}
				}
			}

			arg->d.door.door = dir;
			break;
		}

		// Same as ANY, but will only pick an open exit
		case ENTITY_ARRAY_EXITS_OPEN:
		{
			arg->type = ENT_EXIT;
			arg->d.door.r = room;

			int dir = MAX_DIR;
			if (room)
			{
				int count = 0;
				for(int i = 0; i < MAX_DIR; i++)
					if (room->exit[i] && !IS_SET(room->exit[i]->exit_info, EX_CLOSED))
						count++;

				if (count > 0)
				{
					int door = number_range(1, count);
					
					for(int i = 0; i < MAX_DIR; i++)
					{
						if (room->exit[i] && !IS_SET(room->exit[i]->exit_info, EX_CLOSED) && !--door)
						{
							dir = i;
							break;
						}
					}
				}
			}

			arg->d.door.door = dir;
			break;
		}

		default:
			return NULL;
	}

	return str+1;
}

char *expand_entity_exit(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	EXIT_DATA *ex = (arg->d.door.r && arg->d.door.door >= 0 && arg->d.door.door < MAX_DIR) ? arg->d.door.r->exit[arg->d.door.door] : NULL;

	switch(*str) {
	case ENTITY_EXIT_NAME:
		arg->type = ENT_STRING;
		arg->d.str = ex ? dir_name[arg->d.door.door] : SOMEWHERE;
		break;
	case ENTITY_EXIT_DOOR:
		arg->type = ENT_NUMBER;
		arg->d.num = ex ? arg->d.door.door : -1;
		break;
	case ENTITY_EXIT_SOURCE:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.door.r;
		break;
	case ENTITY_EXIT_REMOTE:
		arg->type = ENT_ROOM;
		arg->d.room = ex ? exit_destination(ex) : NULL;
		break;
	case ENTITY_EXIT_STATE:
		arg->type = ENT_STRING;
		if(ex) {
			int i;
			if(ex->exit_info & EX_BROKEN)
				i = 1;
			else if(ex->exit_info & EX_CLOSED) {
				i = 2;
				if(ex->door.lock.flags & LOCK_LOCKED) i++;
				if(ex->exit_info & EX_BARRED) i+=2;
			} else i = 0;
			arg->d.str = (char*)exit_states[i];
		} else
			arg->d.str = NULL;
		break;
	case ENTITY_EXIT_MATE:
		arg->type = ENT_EXIT;
		arg->d.door.door = rev_dir[arg->d.door.door];
		if(ex)
			arg->d.door.r = exit_destination(ex);
		else
			arg->d.door.r = NULL;
		break;
	case ENTITY_EXIT_NORTH:
	case ENTITY_EXIT_EAST:
	case ENTITY_EXIT_SOUTH:
	case ENTITY_EXIT_WEST:
	case ENTITY_EXIT_UP:
	case ENTITY_EXIT_DOWN:
	case ENTITY_EXIT_NORTHEAST:
	case ENTITY_EXIT_NORTHWEST:
	case ENTITY_EXIT_SOUTHEAST:
	case ENTITY_EXIT_SOUTHWEST:
		if(ex) {
			arg->type = ENT_EXIT;
			arg->d.door.door = *str - ENTITY_EXIT_NORTH + DIR_NORTH;
			arg->d.door.r = exit_destination(ex);
		}
		break;
	case ENTITY_EXIT_NEXT:
		arg->type = ENT_EXIT;
		if(ex) {
			arg->d.door.door++;
			if( arg->d.door.door >= MAX_DIR )
				arg->d.door.door = DIR_NORTH;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_token(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_TOKEN_NAME:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.token ? arg->d.token->name : &str_empty[0];
		break;
	case ENTITY_TOKEN_INDEX:
		arg->type = ENT_TOKENINDEX;
		arg->d.tokindex = arg->d.token ? arg->d.token->pIndexData : NULL;
		break;
	case ENTITY_TOKEN_OWNER:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.token ? arg->d.token->player : NULL;
		break;
	case ENTITY_TOKEN_OBJECT:
		arg->type = ENT_OBJECT;
		arg->d.obj = arg->d.token ? arg->d.token->object : NULL;
		break;
	case ENTITY_TOKEN_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.token ? arg->d.token->room : NULL;
		break;
	case ENTITY_TOKEN_TIMER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.token ? arg->d.token->timer : 0;
		break;
	case ENTITY_TOKEN_VAL0:
	case ENTITY_TOKEN_VAL1:
	case ENTITY_TOKEN_VAL2:
	case ENTITY_TOKEN_VAL3:
	case ENTITY_TOKEN_VAL4:
	case ENTITY_TOKEN_VAL5:
	case ENTITY_TOKEN_VAL6:
	case ENTITY_TOKEN_VAL7:
	case ENTITY_TOKEN_VAL8:
	case ENTITY_TOKEN_VAL9:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.token ? arg->d.token->value[*str  - ENTITY_TOKEN_VAL0] : 0;
		break;
	case ENTITY_TOKEN_NEXT:
		arg->type = ENT_TOKEN;
		arg->d.token = arg->d.token?arg->d.token->next:NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,arg->d.token?arg->d.token->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_TOKEN_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (arg->d.token && arg->d.token->progs) ? &arg->d.token->progs->vars : NULL;
		break;
	case ENTITY_TOKEN_SKILL:
		arg->type = ENT_SKILLENTRY;
		arg->d.entry = arg->d.token ? arg->d.token->skill : NULL;
		break;
//	case ENTITY_TOKEN_AFFECT:
//		arg->type = ENT_AFFECT;
//		arg->d.aff = arg->d.token ? arg->d.token->affect : NULL;
//		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_token_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_TOKEN_NAME:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;
	case ENTITY_TOKEN_INDEX:
		arg->type = ENT_TOKENINDEX;
		arg->d.tokindex = NULL;
		break;
	case ENTITY_TOKEN_OWNER:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ENTITY_TOKEN_OBJECT:
		arg->type = ENT_OBJECT;
		arg->d.mob = NULL;
		break;
	case ENTITY_TOKEN_ROOM:
		arg->type = ENT_ROOM;
		arg->d.mob = NULL;
		break;
	case ENTITY_TOKEN_TIMER:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	case ENTITY_TOKEN_VAL0:
	case ENTITY_TOKEN_VAL1:
	case ENTITY_TOKEN_VAL2:
	case ENTITY_TOKEN_VAL3:
	case ENTITY_TOKEN_VAL4:
	case ENTITY_TOKEN_VAL5:
	case ENTITY_TOKEN_VAL6:
	case ENTITY_TOKEN_VAL7:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	case ENTITY_TOKEN_NEXT:
		arg->type = ENT_TOKEN;
		arg->d.token = NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_TOKEN_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = NULL;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_area(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	AREA_DATA *area = arg->d.area;
	switch(*str) {
	case ENTITY_AREA_NAME:
		arg->type = ENT_STRING;
		arg->d.str = area ? area->name : SOMEWHERE;
		break;
	case ENTITY_AREA_CREDITS:
		arg->type = ENT_STRING;
		arg->d.str = (area && !IS_NULLSTR(area->credits)) ? area->credits : "";
		break;
	case ENTITY_AREA_DESCRIPTION:
		arg->type = ENT_STRING;
		arg->d.str = (area && !IS_NULLSTR(area->description)) ? area->description : "";
		break;
	case ENTITY_AREA_BUILDERS:
		arg->type = ENT_STRING;
		arg->d.str = (area && !IS_NULLSTR(area->builders)) ? area->builders : "";
		break;
	case ENTITY_AREA_COMMENTS:
		arg->type = ENT_STRING;
		arg->d.str = (area && !IS_NULLSTR(area->comments)) ? area->comments : "";
		break;

	case ENTITY_AREA_UID:
		arg->type = ENT_NUMBER;
		arg->d.num = area ? area->uid : -1;
		break;

	case ENTITY_AREA_SECURITY:
		arg->type = ENT_NUMBER;
		arg->d.num = area ? area->security : -1;
		break;

	case ENTITY_AREA_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = area ? area->area_flags : 0;
		arg->d.bv.table = area_flags;
		break;
	
	case ENTITY_AREA_WILDS:
		arg->type = ENT_WILDS;
		arg->d.wilds = area ? area->wilds : NULL;
		break;

	case ENTITY_AREA_NPLAYER:
		arg->type = ENT_NUMBER;
		arg->d.num = area ? area->nplayer : 0;
		break;
	
	case ENTITY_AREA_AGE:
		arg->type = ENT_NUMBER;
		arg->d.num = area ? area->age : -1;
		break;

	case ENTITY_AREA_ISEMPTY:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = area ? area->empty : true;	// NULL areas are... "empty"
		break;

	case ENTITY_AREA_ISOPEN:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = area ? area->open : false;	// NULL areas are... "closed"
		break;

	case ENTITY_AREA_REGION:
		arg->type = ENT_AREA_REGION;
		arg->d.aregion = area ? &area->region : NULL;
		break;
	case ENTITY_AREA_REGIONS:
		arg->type = ENT_PLLIST_AREA_REGION;
		arg->d.blist = area ? area->regions : NULL;
		break;
	case ENTITY_AREA_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = area ? area->room_list : NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,area?area->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_area_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_AREA_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_AREA_CREDITS:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;
	case ENTITY_AREA_DESCRIPTION:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;
	case ENTITY_AREA_BUILDERS:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;
	case ENTITY_AREA_COMMENTS:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;

	case ENTITY_AREA_UID:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;

	case ENTITY_AREA_SECURITY:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;

	case ENTITY_AREA_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = area_flags;
		break;
	
	case ENTITY_AREA_WILDS:
		arg->type = ENT_WILDS;
		arg->d.wilds = NULL;
		break;

	case ENTITY_AREA_NPLAYER:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	
	case ENTITY_AREA_AGE:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;

	case ENTITY_AREA_ISEMPTY:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = true;	// NULL areas are... "empty"
		break;

	case ENTITY_AREA_ISOPEN:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = false;	// NULL areas are... "closed"
		break;

	case ENTITY_AREA_REGION:
		arg->type = ENT_AREA_REGION;
		arg->d.aregion = NULL;
		break;
	case ENTITY_AREA_REGIONS:
		arg->type = ENT_PLLIST_AREA_REGION;
		arg->d.blist = NULL;
		break;
	case ENTITY_AREA_ROOMS:
		arg->type = ENT_BLLIST_ROOM;
		arg->d.blist = NULL;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_area_region(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	AREA_REGION *region = arg->d.aregion;
	switch(*str) {
	case ENTITY_AREA_REGION_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(region) ? region->name : SOMEWHERE;
		break;
	case ENTITY_AREA_REGION_DESCRIPTION:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(region) ? region->description : "";
		break;
	case ENTITY_AREA_REGION_COMMENTS:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(region) ? region->name : "";
		break;
	case ENTITY_AREA_REGION_AREA:
		arg->type = ENT_AREA;
		arg->d.area = IS_VALID(region) ? region->area : NULL;
		break;
	case ENTITY_AREA_REGION_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = (IS_VALID(region) && location_isset(&region->recall)) ? location_to_room(&region->recall) : NULL;
		break;
	case ENTITY_AREA_REGION_POSTOFFICE:
		arg->type = ENT_ROOM;
		arg->d.room = (IS_VALID(region) && region->post_office > 0) ? get_room_index(region->area, region->post_office) : NULL;
		break;
	case ENTITY_AREA_REGION_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = IS_VALID(region) ? region->rooms : NULL;
		break;
	case ENTITY_AREA_REGION_X:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(region) ? region->x : -1;
		break;
	case ENTITY_AREA_REGION_Y:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(region) ? region->y : -1;
		break;
	case ENTITY_AREA_REGION_LAND_X:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(region) ? region->land_x : -1;
		break;
	case ENTITY_AREA_REGION_LAND_Y:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(region) ? region->land_y : -1;
		break;
	case ENTITY_AREA_REGION_AIRSHIP:
		arg->type = ENT_ROOM;
		arg->d.room = (IS_VALID(region) && region->airship_land_spot > 0) ? get_room_index(region->area, region->airship_land_spot) : NULL;
		break;

	case ENTITY_AREA_REGION_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(region) ? region->flags : 0;
		arg->d.bv.table = area_region_flags;
		break;

	case ENTITY_AREA_REGION_WHO:
		arg->type = ENT_STAT;
		arg->d.stat.value = IS_VALID(region) ? region->area_who : AREA_BLANK;
		arg->d.stat.table = area_who_titles;
		arg->d.stat.def_value = AREA_WHO_MAX;
		break;

	case ENTITY_AREA_REGION_PLACE:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(region) ? region->place_flags: 0;
		arg->d.bv.table = place_flags;
		break;

	case ENTITY_AREA_REGION_SAVAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(region) ? region->savage_level : -1;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_area_region_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_AREA_REGION_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_AREA_REGION_DESCRIPTION:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;
	case ENTITY_AREA_REGION_COMMENTS:
		arg->type = ENT_STRING;
		arg->d.str = "";
		break;
	case ENTITY_AREA_REGION_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
		break;
	case ENTITY_AREA_REGION_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_AREA_REGION_POSTOFFICE:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_AREA_REGION_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;
	case ENTITY_AREA_REGION_X:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;
	case ENTITY_AREA_REGION_Y:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;
	case ENTITY_AREA_REGION_LAND_X:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;
	case ENTITY_AREA_REGION_LAND_Y:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;
	case ENTITY_AREA_REGION_AIRSHIP:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_AREA_REGION_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = area_region_flags;
		break;

	case ENTITY_AREA_REGION_WHO:
		arg->type = ENT_STAT;
		arg->d.stat.value = AREA_BLANK;
		arg->d.stat.table = area_who_titles;
		arg->d.stat.def_value = AREA_WHO_MAX;
		break;

	case ENTITY_AREA_REGION_PLACE:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = place_flags;
		break;

	case ENTITY_AREA_REGION_SAVAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = -1;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_list_mob(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register CHAR_DATA *mob;
	register int count;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.list.ptr.mob)
		{
			arg->type = ENT_MOBILE;
			for(mob = *arg->d.list.ptr.mob;--index > 0 && mob;mob = mob->next_in_room);
			arg->d.mob = mob;
		}

		return str;
	}

	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		count = 0;
		if(arg->d.list.ptr.mob)
			for(mob = *arg->d.list.ptr.mob;mob;mob = mob->next_in_room) count++;
		arg->d.num = count;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.list.ptr.mob) {
			count = 0;
			for(mob = *arg->d.list.ptr.mob;mob;mob = mob->next_in_room) count++;
			if(count > 0) {
				count = number_range(1,count);
				for(mob = *arg->d.list.ptr.mob;count-- > 0;mob = mob->next_in_room);
				arg->d.mob = mob;
			} else
				arg->d.mob = NULL;
		} else
			arg->d.mob = NULL;
		arg->type = ENT_MOBILE;
		break;
	case ENTITY_LIST_FIRST:
		arg->d.mob = arg->d.list.ptr.mob ? *arg->d.list.ptr.mob : NULL;
		arg->type = ENT_MOBILE;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.list.ptr.mob) {
			for(mob = *arg->d.list.ptr.mob;mob && mob->next_in_room;mob = mob->next_in_room);
			arg->d.mob = mob;
		} else
			arg->d.mob = NULL;
		arg->type = ENT_MOBILE;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_list_obj(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register OBJ_DATA *obj;
	register int count;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.list.ptr.obj)
		{
			arg->type = ENT_OBJECT;
			for(obj = *arg->d.list.ptr.obj;--index > 0 && obj;obj = obj->next_content);
			arg->d.obj = obj;
		}

		return str;
	}

	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		count = 0;
		if(arg->d.list.ptr.obj)
			for(obj = *arg->d.list.ptr.obj;obj;obj = obj->next_content) count++;
		arg->d.num = count;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.list.ptr.obj) {
			count = 0;
			for(obj = *arg->d.list.ptr.obj;obj;obj = obj->next_content) count++;
			if(count > 0) {
				count = number_range(1,count);
				for(obj = *arg->d.list.ptr.obj;count-- > 0;obj = obj->next_content);
				arg->d.obj = obj;
			} else
				arg->d.obj = NULL;
		} else
			arg->d.obj = NULL;
		arg->type = ENT_OBJECT;
		break;
	case ENTITY_LIST_FIRST:
		arg->d.obj = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
		arg->type = ENT_OBJECT;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.list.ptr.obj) {
			for(obj = *arg->d.list.ptr.obj;obj && obj->next_content;obj = obj->next_content);
			arg->d.obj = obj;
		} else
			arg->d.obj = NULL;
		arg->type = ENT_OBJECT;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_list_token(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register TOKEN_DATA *token;
	register int count;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.list.ptr.obj)
		{
			arg->type = ENT_TOKEN;
			for(token = *arg->d.list.ptr.tok;--index > 0 && token;token = token->next);
			arg->d.token = token;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		count = 0;
		if(arg->d.list.ptr.tok)
			for(token = *arg->d.list.ptr.tok;token;token = token->next) count++;
		arg->d.num = count;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.list.ptr.tok) {
			count = 0;
			for(token = *arg->d.list.ptr.tok;token;token = token->next) count++;
			if(count > 0) {
				count = number_range(1,count);
				for(token = *arg->d.list.ptr.tok;count-- > 0;token = token->next);
				arg->d.token = token;
			} else
				arg->d.token = NULL;
		} else
			arg->d.token = NULL;
		arg->type = ENT_TOKEN;
		break;
	case ENTITY_LIST_FIRST:
		arg->d.token = arg->d.list.ptr.tok ? *arg->d.list.ptr.tok : NULL;
		arg->type = ENT_TOKEN;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.list.ptr.tok) {
			for(token = *arg->d.list.ptr.tok;token && token->next;token = token->next);
			arg->d.token = token;
		} else
			arg->d.token = NULL;
		arg->type = ENT_TOKEN;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_list_affect(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register AFFECT_DATA *affect;
	register int count;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.list.ptr.obj)
		{
			arg->type = ENT_AFFECT;
			for(affect = *arg->d.list.ptr.aff;--index > 0 && affect;affect = affect->next);
			arg->d.aff = affect;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		count = 0;
		if(arg->d.list.ptr.aff)
			for(affect = *arg->d.list.ptr.aff;affect;affect = affect->next) count++;
		arg->d.num = count;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.list.ptr.aff) {
			count = 0;
			for(affect = *arg->d.list.ptr.aff;affect;affect = affect->next) count++;
			if(count > 0) {
				count = number_range(1,count);
				for(affect = *arg->d.list.ptr.aff;count-- > 0;affect = affect->next);
				arg->d.aff = affect;
			} else
				arg->d.aff = NULL;
		} else
			arg->d.aff = NULL;
		arg->type = ENT_AFFECT;
		break;
	case ENTITY_LIST_FIRST:
		arg->d.aff = arg->d.list.ptr.aff ? *arg->d.list.ptr.aff : NULL;
		arg->type = ENT_AFFECT;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.list.ptr.aff) {
			for(affect = *arg->d.list.ptr.aff;affect && affect->next;affect = affect->next);
			arg->d.aff = affect;
		} else
			arg->d.aff = NULL;
		arg->type = ENT_AFFECT;
		break;
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_skill(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SKILL_DATA *skill = arg->d.skill;

	switch(*str) {
	case ENTITY_SKILL_GSN:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->uid : 0;
		break;

	case ENTITY_SKILL_TOKEN:
		arg->type = ENT_TOKENINDEX;
		arg->d.tokindex = IS_VALID(skill) ? skill->token : NULL;
		break;

	case ENTITY_SKILL_SPELL:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = is_skill_spell(skill);
		break;

	case ENTITY_SKILL_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, IS_VALID(skill) ? skill->name : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_DISPLAY:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (IS_VALID(skill) && !IS_NULLSTR(skill->display)) ? skill->display : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_BEATS:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->beats : 0;
		break;

	case ENTITY_SKILL_LEVEL_WARRIOR:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->skill_level[CLASS_WARRIOR] : 0;
		break;

	case ENTITY_SKILL_LEVEL_CLERIC:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->skill_level[CLASS_CLERIC] : 0;
		break;

	case ENTITY_SKILL_LEVEL_MAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->skill_level[CLASS_MAGE] : 0;
		break;

	case ENTITY_SKILL_LEVEL_THIEF:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->skill_level[CLASS_THIEF] : 0;
		break;


	case ENTITY_SKILL_DIFFICULTY_WARRIOR:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->rating[CLASS_WARRIOR] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_CLERIC:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->rating[CLASS_CLERIC] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_MAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->rating[CLASS_MAGE] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_THIEF:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->rating[CLASS_THIEF] : 0;
		break;

	case ENTITY_SKILL_TARGET:
		arg->type = ENT_STAT;
		arg->d.stat.value = IS_VALID(skill) ? skill->target : TAR_IGNORE;
		arg->d.stat.table = spell_target_types;
		arg->d.stat.def_value = -1;
		break;

	case ENTITY_SKILL_POSITION:
		arg->type = ENT_STAT;
		arg->d.stat.value = IS_VALID(skill) ? skill->minimum_position : POS_DEAD;
		arg->d.stat.table = spell_position_flags;
		arg->d.stat.def_value = -1;
		break;

	case ENTITY_SKILL_NOUN:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (IS_VALID(skill) && skill->noun_damage) ? skill->noun_damage: "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_WEAROFF:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (IS_VALID(skill) && skill->msg_off) ? skill->msg_off : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_OBJECT:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (IS_VALID(skill) && skill->msg_obj) ? skill->msg_obj : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_DISPEL:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (IS_VALID(skill) && skill->msg_disp) ? skill->msg_disp : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SKILL_MANA_CAST:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->cast_mana : 0;
		break;

	case ENTITY_SKILL_MANA_BREW:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->brew_mana : 0;
		break;

	case ENTITY_SKILL_MANA_SCRIBE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->scribe_mana : 0;
		break;

	case ENTITY_SKILL_MANA_IMBUE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->imbue_mana : 0;
		break;

	case ENTITY_SKILL_INK_TYPE1:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[0][0] : 0;
		break;

	case ENTITY_SKILL_INK_TYPE2:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[1][0] : 0;
		break;

	case ENTITY_SKILL_INK_TYPE3:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[2][0] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE1:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[0][1] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE2:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[1][1] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE3:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(skill) ? skill->inks[2][1] : 0;
		break;

	case ENTITY_SKILL_VALUES:
		arg->type = ENT_SKILL_VALUES;
		// Uses arg->d.skill
		break;

	case ENTITY_SKILL_VALUENAMES:
		arg->type = ENT_SKILL_VALUENAMES;
		// Uses arg->d.skill
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_skill_values(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	if (*str == ESCAPE_EXPRESSION)
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		arg->type = ENT_NUMBER;
		if (IS_VALID(arg->d.skill) && index > 1 && index <= MAX_SKILL_VALUES)
			arg->d.num = arg->d.skill->values[index - 1];
		else
			arg->d.num = 0;

		return str;
	}

	return NULL;
}

char *expand_entity_skill_valuenames(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	if (*str == ESCAPE_EXPRESSION)
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		arg->type = ENT_STRING;
		if (IS_VALID(arg->d.skill) && index > 1 && index <= MAX_SKILL_VALUES)
			arg->d.str = arg->d.skill->valuenames[index - 1];
		else
			arg->d.str = "";

		return str;
	}

	return NULL;
}

char *expand_entity_skillentry(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SKILL_ENTRY *entry = arg->d.entry;
	switch(*str) {
	case ENTITY_SKILLENTRY_SKILL:
		arg->type = ENT_SKILL;
		arg->d.skill = entry ? entry->skill : NULL;
		break;

	case ENTITY_SKILLENTRY_SOURCE:
		arg->type = ENT_NUMBER;
		arg->d.num = entry ? entry->source : -1;
		break;

	case ENTITY_SKILLENTRY_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = entry ? entry->flags : 0;
		arg->d.bv.table = entry ? skill_entry_flags : NULL;
		break;

	case ENTITY_SKILLENTRY_ISSPELL:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = entry ? entry->isspell : false;
		break;

	case ENTITY_SKILLENTRY_SONG:
		arg->type = ENT_SONG;
		arg->d.song = entry ? entry->song : NULL;
		break;

	case ENTITY_SKILLENTRY_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = entry ? entry->token : NULL;
		break;

	case ENTITY_SKILLENTRY_RATING:
		arg->type = ENT_NUMBER;
		arg->d.num = entry ? entry->rating : 0;
		break;

	case ENTITY_SKILLENTRY_MOD_RATING:
		arg->type = ENT_NUMBER;
		arg->d.num = entry ? entry->mod_rating : 0;
		break;

	default: return NULL;
	}
	
	return str+1;
}

char *expand_entity_skillinfo(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_SKILLINFO_SKILL:
		arg->type = ENT_SKILL;
		arg->d.skill = arg->d.sk.skill;
		break;

	case ENTITY_SKILLINFO_OWNER:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.sk.m;
		break;

	case ENTITY_SKILLINFO_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = arg->d.sk.t;
		break;

	case ENTITY_SKILLINFO_RATING:
		arg->type = ENT_NUMBER;
		if( arg->d.sk.m ) {
			arg->d.num = get_skill(arg->d.sk.m,arg->d.sk.skill);
		} else
			arg->d.num = 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_skillinfo_id(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_SKILLINFO_SKILL:
		arg->type = ENT_SKILL;
		arg->d.skill = arg->d.sk.skill;
		break;

	case ENTITY_SKILLINFO_OWNER:
		arg->type = ENT_MOBILE_ID;
		arg->d.uid[0] = arg->d.sk.mid[0];
		arg->d.uid[1] = arg->d.sk.mid[1];
		break;

	case ENTITY_SKILLINFO_TOKEN:
		arg->type = ENT_TOKEN_ID;
		arg->d.uid[0] = arg->d.sk.tid[0];
		arg->d.uid[1] = arg->d.sk.tid[1];
		break;

	case ENTITY_SKILLINFO_RATING:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_conn(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_CONN_PLAYER:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.conn ? arg->d.conn->character : NULL;
		break;
	case ENTITY_CONN_ORIGINAL:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.conn ? (arg->d.conn->original ? arg->d.conn->original : arg->d.conn->character) : NULL;
		break;
	case ENTITY_CONN_HOST:
		arg->type = ENT_STRING;
		arg->d.str = (arg->d.conn && (script_security >= MAX_SCRIPT_SECURITY)) ? arg->d.conn->host : &str_empty[0];
		break;
	case ENTITY_CONN_CONNECTION:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.conn && (script_security >= MAX_SCRIPT_SECURITY)) ? arg->d.conn->connected : -1;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_affect(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	AFFECT_DATA *affect = arg->d.aff;
	//printf("expand_entity_affect() called\n\r");
	switch(*str) {
	case ENTITY_AFFECT_NAME:
		arg->type = ENT_STRING;
		if(affect) {
			if(affect->custom_name)
				arg->d.str = affect->custom_name;
			else if(IS_VALID(affect->skill))
			{
				if (IS_NULLSTR(affect->skill->display))
					arg->d.str = affect->skill->name;
				else
					arg->d.str = affect->skill->display;
			}
			else
				arg->d.str = &str_empty[0];
		} else
			arg->d.str = &str_empty[0];
		//printf("expand_entity_affect(NAME)-> \"%s\"\n\r", arg->d.str);
		break;

	case ENTITY_AFFECT_GROUP:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->group : -1;
		break;

	case ENTITY_AFFECT_SKILL:
		arg->type = ENT_SKILL;
		arg->d.skill = affect ? affect->skill : NULL;
		break;

	case ENTITY_AFFECT_CATALYST:
		arg->type = ENT_STAT;
		arg->d.stat.value = affect ? affect->catalyst_type : CATALYST_NONE;
		arg->d.stat.table = catalyst_types;
		arg->d.stat.def_value = NO_FLAG;
		break;

	case ENTITY_AFFECT_WHERE:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->where : -1;
		break;

	case ENTITY_AFFECT_LOCATION:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->location : -1;
		break;

	case ENTITY_AFFECT_MOD:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->modifier : 0;
		break;

	case ENTITY_AFFECT_BITS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = affect ? affect->bitvector : 0;
		arg->d.bv.table = affect ? affect_flags : NULL;
		break;

	case ENTITY_AFFECT_BITS2:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = affect ? affect->bitvector2 : 0;
		arg->d.bv.table = affect ? affect2_flags : NULL;
		break;

	case ENTITY_AFFECT_LEVEL:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->level : 0;
		break;

	case ENTITY_AFFECT_TIMER:
		arg->type = ENT_NUMBER;
		arg->d.num = affect ? affect->duration : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_page(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	BOOK_PAGE *page = IS_VALID(arg->d.obj) && IS_PAGE(arg->d.obj) ? PAGE(arg->d.obj) : NULL;

	//printf("expand_entity_object_page() called\n\r");
	switch(*str) {
	case ENTITY_BOOK_PAGE_NUMBER:
		arg->type = ENT_NUMBER;
		arg->d.num = page ? page->page_no : 0;
		break;

	case ENTITY_BOOK_PAGE_TITLE:
		arg->type = ENT_STRING;
		arg->d.str = page ? page->title : "";
		break;

	case ENTITY_BOOK_PAGE_TEXT:
		arg->type = ENT_STRING;
		arg->d.str = page ? page->text : "";
		break;

	case ENTITY_BOOK_PAGE_BOOK:
		arg->type = ENT_OBJINDEX;
		if (page && page->book.auid > 0 && page->book.vnum > 0)
			arg->d.objindex = get_obj_index_auid(page->book.auid, page->book.vnum);
		else
			arg->d.objindex = NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_book_page(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	//printf("expand_entity_book_page() called\n\r");
	switch(*str) {
	case ENTITY_BOOK_PAGE_NUMBER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.book_page ? arg->d.book_page->page_no : 0;
		break;

	case ENTITY_BOOK_PAGE_TITLE:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.book_page ? arg->d.book_page->title : "";
		break;

	case ENTITY_BOOK_PAGE_TEXT:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.book_page ? arg->d.book_page->text : "";
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_food_buff(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	//printf("expand_entity_food_buff() called\n\r");
	switch(*str) {
	case ENTITY_FOOD_BUFF_WHERE:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.food_buff ? arg->d.food_buff->where : -1;
		break;

	case ENTITY_FOOD_BUFF_LOCATION:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.food_buff ? arg->d.food_buff->location : -1;
		break;

	case ENTITY_FOOD_BUFF_MOD:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.food_buff ? arg->d.food_buff->modifier : 0;
		break;

	case ENTITY_FOOD_BUFF_BITS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.food_buff ? arg->d.food_buff->bitvector : 0;
		arg->d.bv.table = arg->d.food_buff ? affect_flags : NULL;
		break;

	case ENTITY_FOOD_BUFF_BITS2:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.food_buff ? arg->d.food_buff->bitvector2 : 0;
		arg->d.bv.table = arg->d.food_buff ? affect2_flags : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_compartment(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_COMPARTMENT_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(arg->d.compartment) ? arg->d.compartment->name : "";
		break;
	
	case ENTITY_COMPARTMENT_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(arg->d.compartment) ? arg->d.compartment->short_descr : "";
		break;
	
	case ENTITY_COMPARTMENT_DESC:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(arg->d.compartment) ? arg->d.compartment->description : "";
		break;

	case ENTITY_COMPARTMENT_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->flags : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? compartment_flags : NULL;
		break;
	
	case ENTITY_COMPARTMENT_MAX_OCCUPANTS:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.compartment) ? arg->d.compartment->max_occupants : 0;
		break;
	
	case ENTITY_COMPARTMENT_MAX_WEIGHT:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.compartment) ? arg->d.compartment->max_weight : 0;
		break;

	case ENTITY_COMPARTMENT_STANDING:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->standing : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? furniture_flags : NULL;
		break;

	case ENTITY_COMPARTMENT_HANGING:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->hanging : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? furniture_flags : NULL;
		break;

	case ENTITY_COMPARTMENT_SITTING:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->sitting : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? furniture_flags : NULL;
		break;

	case ENTITY_COMPARTMENT_RESTING:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->resting : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? furniture_flags : NULL;
		break;

	case ENTITY_COMPARTMENT_SLEEPING:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(arg->d.compartment) ? arg->d.compartment->sleeping : 0;
		arg->d.bv.table = IS_VALID(arg->d.compartment) ? furniture_flags : NULL;
		break;
	
	case ENTITY_COMPARTMENT_HEALTH:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.compartment) ? arg->d.compartment->health_regen : 0;
		break;
	
	case ENTITY_COMPARTMENT_MANA:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.compartment) ? arg->d.compartment->mana_regen : 0;
		break;
	
	case ENTITY_COMPARTMENT_MOVE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.compartment) ? arg->d.compartment->move_regen : 0;
		break;
	
	case ENTITY_COMPARTMENT_LOCK:
		arg->type = ENT_LOCK_STATE;
		arg->d.lockstate = IS_VALID(arg->d.compartment) ? arg->d.compartment->lock : NULL;
		break;
	
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_liquid(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	LIQUID *liquid = arg->d.liquid;

	switch(*str)
	{
	case ENTITY_LIQUID_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(liquid) ? liquid->name : "";
		break;

	case ENTITY_LIQUID_COLOR:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(liquid) ? liquid->color : "";
		break;

	case ENTITY_LIQUID_UID:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->uid : 0;
		break;

	case ENTITY_LIQUID_USED:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->used : 0;
		break;

	case ENTITY_LIQUID_FLAMMABLE:
		arg->type = ENT_BOOLEAN;
		arg->d.boolean = IS_VALID(liquid) ? liquid->flammable : false;
		break;

	case ENTITY_LIQUID_PROOF:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->proof : 0;
		break;

	case ENTITY_LIQUID_FULL:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->full : 0;
		break;

	case ENTITY_LIQUID_THIRST:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->thirst : 0;
		break;

	case ENTITY_LIQUID_HUNGER:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->hunger : 0;
		break;

	case ENTITY_LIQUID_FUEL_UNIT:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->fuel_unit : 0;
		break;

	case ENTITY_LIQUID_FUEL_DURATION:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(liquid) ? liquid->fuel_duration : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_spelldata(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SPELL_DATA *spell = arg->d.spell;

	switch(*str)
	{
	case ENTITY_SPELLDATA_NAME:
		// TODO: rework after skedit is finished.
		arg->type = ENT_STRING;
		if (spell)
		{
			if (IS_VALID(spell->skill))
				arg->d.str = spell->skill->name;
			else
				arg->d.str = "none";
		}
		else
			arg->d.str = "";
		break;

	case ENTITY_SPELLDATA_SKILL:
		arg->type = ENT_SKILL;
		arg->d.skill = spell ? spell->skill : NULL;
		break;
	
	case ENTITY_SPELLDATA_LEVEL:
		arg->type = ENT_NUMBER;
		arg->d.num = spell ? spell->level : 0;
		break;
	
	case ENTITY_SPELLDATA_CHANCE:
		arg->type = ENT_NUMBER;
		arg->d.num = spell ? spell->repop : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_lockstate(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	LOCK_STATE *lock = arg->d.lockstate;

	switch(*str)
	{
	case ENTITY_LOCKSTATE_KEY:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum = lock ? lock->key_wnum : wnum_zero;
		break;
	
	case ENTITY_LOCKSTATE_PICK:
		arg->type = ENT_NUMBER;
		arg->d.num = lock ? lock->pick_chance : 0;
		break;

	case ENTITY_LOCKSTATE_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = lock ? lock->flags : 0;
		arg->d.bv.table = lock ? lock_flags : NULL;
		break;

	case ENTITY_LOCKSTATE_SPECIALKEYS:
		arg->type = ENT_ILLIST_SPECIALKEYS;
		arg->d.blist = lock ? lock->special_keys : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_clone_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_ROOM_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_ROOM_MOBILES:
		arg->type = ENT_OLLIST_MOB;
		arg->d.list.ptr.mob = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
		break;
	case ENTITY_ROOM_REGION:
		arg->type = ENT_AREA_REGION;
		arg->d.aregion = NULL;
		break;
	case ENTITY_ROOM_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ENTITY_ROOM_NORTH:
	case ENTITY_ROOM_EAST:
	case ENTITY_ROOM_SOUTH:
	case ENTITY_ROOM_WEST:
	case ENTITY_ROOM_UP:
	case ENTITY_ROOM_DOWN:
	case ENTITY_ROOM_NORTHEAST:
	case ENTITY_ROOM_NORTHWEST:
	case ENTITY_ROOM_SOUTHEAST:
	case ENTITY_ROOM_SOUTHWEST:
		arg->type = ENT_EXIT;
		arg->d.door.r = NULL;
		arg->d.door.door = *str - ENTITY_ROOM_NORTH;
		break;
	case ENTITY_ROOM_ENVIRON:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_MOB:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_OBJ:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = NULL;
		break;

	case ENTITY_ROOM_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.list.ptr.ed = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_ROOM;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;

	case ENTITY_ROOM_SECTION:
		arg->type = ENT_SECTION;
		arg->d.section = NULL;
		break;

	case ENTITY_ROOM_INSTANCE:
		arg->type = ENT_INSTANCE;
		arg->d.instance = NULL;
		break;

	case ENTITY_ROOM_DUNGEON:
		arg->type = ENT_DUNGEON;
		arg->d.dungeon = NULL;
		break;

	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,NULL,str+1,arg);
		if(!str) return NULL;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_wilds_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_ROOM_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_ROOM_MOBILES:
		arg->type = ENT_OLLIST_MOB;
		arg->d.list.ptr.mob = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_UNKNOWN;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
		break;
	case ENTITY_ROOM_REGION:
		arg->type = ENT_AREA_REGION;
		arg->d.aregion = NULL;
		break;
	case ENTITY_ROOM_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;
	case ENTITY_ROOM_NORTH:
	case ENTITY_ROOM_EAST:
	case ENTITY_ROOM_SOUTH:
	case ENTITY_ROOM_WEST:
	case ENTITY_ROOM_UP:
	case ENTITY_ROOM_DOWN:
	case ENTITY_ROOM_NORTHEAST:
	case ENTITY_ROOM_NORTHWEST:
	case ENTITY_ROOM_SOUTHEAST:
	case ENTITY_ROOM_SOUTHWEST:
		arg->type = ENT_EXIT;
		arg->d.door.r = NULL;
		arg->d.door.door = *str - ENTITY_ROOM_NORTH;
		break;
	case ENTITY_ROOM_ENVIRON:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_MOB:
		arg->type = ENT_MOBILE;
		arg->d.mob = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_OBJ:
		arg->type = ENT_OBJECT;
		arg->d.obj = NULL;
		break;

	case ENTITY_ROOM_ENVIRON_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = NULL;
		break;

	case ENTITY_ROOM_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.list.ptr.ed = NULL;
		arg->d.list.owner = NULL;
		arg->d.list.owner_type = ENT_ROOM;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
		break;

	case ENTITY_ROOM_SECTION:
		arg->type = ENT_SECTION;
		arg->d.section = NULL;
		break;

	case ENTITY_ROOM_INSTANCE:
		arg->type = ENT_INSTANCE;
		arg->d.instance = NULL;
		break;

	case ENTITY_ROOM_DUNGEON:
		arg->type = ENT_DUNGEON;
		arg->d.dungeon = NULL;
		break;

	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,NULL,str+1,arg);
		if(!str) return NULL;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_clone_door(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_EXIT_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_EXIT_DOOR:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.cdoor.door;
		break;
	case ENTITY_EXIT_SOURCE:
		arg->type = ENT_CLONE_ROOM;
		arg->d.cr.r = arg->d.cdoor.r;
		arg->d.cr.a = arg->d.cdoor.a;
		arg->d.cr.b = arg->d.cdoor.b;
		break;
	case ENTITY_EXIT_REMOTE:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_EXIT_STATE:
		arg->type = ENT_STRING;
		arg->d.str = NULL;
		break;
	case ENTITY_EXIT_MATE:
		arg->type = ENT_EXIT;
		arg->d.door.door = DIR_NORTH;
		arg->d.door.r = NULL;
		break;
	case ENTITY_EXIT_NORTH:
	case ENTITY_EXIT_EAST:
	case ENTITY_EXIT_SOUTH:
	case ENTITY_EXIT_WEST:
	case ENTITY_EXIT_UP:
	case ENTITY_EXIT_DOWN:
	case ENTITY_EXIT_NORTHEAST:
	case ENTITY_EXIT_NORTHWEST:
	case ENTITY_EXIT_SOUTHEAST:
	case ENTITY_EXIT_SOUTHWEST:
		arg->type = ENT_EXIT;
		arg->d.door.door = *str - ENTITY_EXIT_NORTH + DIR_NORTH;
		arg->d.door.r = NULL;
		break;
	case ENTITY_EXIT_NEXT:
		arg->type = ENT_CLONE_DOOR;
		arg->d.cdoor.door++;
		if( arg->d.cdoor.door >= MAX_DIR ) {
			arg->d.cdoor.r = NULL;
			arg->d.cdoor.a = 0;
			arg->d.cdoor.b = 0;
			arg->d.cdoor.door = DIR_NORTH;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_wilds_door(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_EXIT_NAME:
		arg->type = ENT_STRING;
		arg->d.str = SOMEWHERE;
		break;
	case ENTITY_EXIT_DOOR:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.cdoor.door;
		break;
	case ENTITY_EXIT_SOURCE:
		arg->type = ENT_WILDS_ROOM;
		arg->d.wroom.wuid = arg->d.wdoor.wuid;
		arg->d.wroom.x = arg->d.wdoor.x;
		arg->d.wroom.y = arg->d.wdoor.y;
		break;
	case ENTITY_EXIT_REMOTE:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_EXIT_STATE:
		arg->type = ENT_STRING;
		arg->d.str = NULL;
		break;

	// The mate to a "wilds" room flips around
	case ENTITY_EXIT_MATE:
		if( arg->d.wdoor.door == DIR_NORTH )		{ arg->d.wdoor.y--; }
		else if( arg->d.wdoor.door == DIR_EAST )	{ arg->d.wdoor.x++; }
		else if( arg->d.wdoor.door == DIR_SOUTH )	{ arg->d.wdoor.y++; }
		else if( arg->d.wdoor.door == DIR_WEST )	{ arg->d.wdoor.x--; }
		else if( arg->d.wdoor.door == DIR_NORTHWEST )	{ arg->d.wdoor.x--; arg->d.wdoor.y--; }
		else if( arg->d.wdoor.door == DIR_NORTHEAST )	{ arg->d.wdoor.x++; arg->d.wdoor.y--; }
		else if( arg->d.wdoor.door == DIR_SOUTHWEST )	{ arg->d.wdoor.x--; arg->d.wdoor.y++; }
		else if( arg->d.wdoor.door == DIR_SOUTHEAST )	{ arg->d.wdoor.x++; arg->d.wdoor.y++; }
		arg->d.wdoor.door = rev_dir[arg->d.wdoor.door];
		break;

	// Exits in a WILDS room can "move around" even if the wilds map doesn't exist
	case ENTITY_EXIT_NORTH:		arg->d.wdoor.y--; break;
	case ENTITY_EXIT_EAST:		arg->d.wdoor.x++; break;
	case ENTITY_EXIT_SOUTH:		arg->d.wdoor.y++; break;
	case ENTITY_EXIT_WEST:		arg->d.wdoor.x--; break;
	case ENTITY_EXIT_NORTHEAST:	arg->d.wdoor.x++; arg->d.wdoor.y--; break;
	case ENTITY_EXIT_NORTHWEST:	arg->d.wdoor.x--; arg->d.wdoor.y--; break;
	case ENTITY_EXIT_SOUTHEAST:	arg->d.wdoor.x++; arg->d.wdoor.y++; break;
	case ENTITY_EXIT_SOUTHWEST:	arg->d.wdoor.x--; arg->d.wdoor.y++; break;
	case ENTITY_EXIT_UP:
	case ENTITY_EXIT_DOWN:
		arg->type = ENT_EXIT;
		arg->d.door.door = *str - ENTITY_EXIT_NORTH + DIR_NORTH;
		arg->d.door.r = NULL;
		break;
	case ENTITY_EXIT_NEXT:
		arg->type = ENT_WILDS_DOOR;
		arg->d.wdoor.door++;
		if( arg->d.wdoor.door >= MAX_DIR ) {
			arg->d.wdoor.wuid = 0;
			arg->d.wdoor.x = 0;
			arg->d.wdoor.y = 0;
			arg->d.wdoor.door = DIR_NORTH;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_str(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char *p = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, index);

		clear_buf(arg->buffer);
		add_buf(arg->buffer, p ? p : "");
		arg->d.str = buf_string(arg->buffer);
		arg->type = ENT_STRING;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		clear_buf(arg->buffer);
		add_buf(arg->buffer, p ? p : "");
		arg->d.str = buf_string(arg->buffer);
		arg->type = ENT_STRING;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, 1);

		clear_buf(arg->buffer);
		add_buf(arg->buffer, p ? p : "");
		arg->d.str = buf_string(arg->buffer);
		arg->type = ENT_STRING;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, -1);

		clear_buf(arg->buffer);
		add_buf(arg->buffer, p ? p : "");
		arg->d.str = buf_string(arg->buffer);
		arg->type = ENT_STRING;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_blist_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_ROOM_DATA *r = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			r = (LLIST_ROOM_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.room = r ? r->room : NULL;
		arg->type = ENT_ROOM;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			r = (LLIST_ROOM_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.room = r ? r->room : NULL;
		arg->type = ENT_ROOM;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			r = (LLIST_ROOM_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.room = r ? r->room : NULL;
		arg->type = ENT_ROOM;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			r = (LLIST_ROOM_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.room = r ? r->room : NULL;
		arg->type = ENT_ROOM;
		break;
	default: return NULL;
	}


	return str+1;
}

char *expand_entity_blist_mob(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_UID_DATA *uid = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, index);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.mob = (CHAR_DATA *)uid->ptr;
				arg->type = ENT_MOBILE;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_MOBILE_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_MOBILE;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		if( uid ) {
			if( uid->ptr ) {
				arg->d.mob = (CHAR_DATA *)uid->ptr;
				arg->type = ENT_MOBILE;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_MOBILE_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_MOBILE;
		}
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.mob = (CHAR_DATA *)uid->ptr;
				arg->type = ENT_MOBILE;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_MOBILE_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_MOBILE;
		}
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, -1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.mob = (CHAR_DATA *)uid->ptr;
				arg->type = ENT_MOBILE;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_MOBILE_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_MOBILE;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_blist_obj(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_UID_DATA *uid = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, index);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.obj = (OBJ_DATA *)uid->ptr;
				arg->type = ENT_OBJECT;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_OBJECT_ID;
			}
		} else {
			arg->d.obj = NULL;
			arg->type = ENT_OBJECT;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		if( uid ) {
			if( uid->ptr ) {
				arg->d.obj = (OBJ_DATA *)uid->ptr;
				arg->type = ENT_OBJECT;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_OBJECT_ID;
			}
		} else {
			arg->d.obj = NULL;
			arg->type = ENT_OBJECT;
		}
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.obj = (OBJ_DATA *)uid->ptr;
				arg->type = ENT_OBJECT;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_OBJECT_ID;
			}
		} else {
			arg->d.obj = NULL;
			arg->type = ENT_OBJECT;
		}
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, -1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.obj = (OBJ_DATA *)uid->ptr;
				arg->type = ENT_OBJECT;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_OBJECT_ID;
			}
		} else {
			arg->d.obj = NULL;
			arg->type = ENT_OBJECT;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_blist_token(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_UID_DATA *uid = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, index);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.token  = (TOKEN_DATA *)uid->ptr;
				arg->type = ENT_TOKEN;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_TOKEN_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_TOKEN;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		if( uid ) {
			if( uid->ptr ) {
				arg->d.token = (TOKEN_DATA *)uid->ptr;
				arg->type = ENT_TOKEN;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_TOKEN_ID;
			}
		} else {
			arg->d.token = NULL;
			arg->type = ENT_TOKEN;
		}
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.token  = (TOKEN_DATA *)uid->ptr;
				arg->type = ENT_TOKEN;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_TOKEN_ID;
			}
		} else {
			arg->d.mob = NULL;
			arg->type = ENT_TOKEN;
		}
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, -1);

		if( uid ) {
			if( uid->ptr ) {
				arg->d.token = (TOKEN_DATA *)uid->ptr;
				arg->type = ENT_TOKEN;
			} else {
				arg->d.uid[0] = uid->id[0];
				arg->d.uid[1] = uid->id[1];
				arg->type = ENT_TOKEN_ID;
			}
		} else {
			arg->d.token = NULL;
			arg->type = ENT_TOKEN;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_blist_area(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_AREA_DATA *uid = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_AREA_DATA *)list_nthdata(arg->d.blist, index);

		if( uid ) {
			if( uid->area ) {
				arg->d.area = (AREA_DATA *)uid->area;
				arg->type = ENT_AREA;
			} else {
				arg->d.aid = uid->uid;
				arg->type = ENT_AREA_ID;
			}
		} else {
			arg->d.area = NULL;
			arg->type = ENT_AREA;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		return str+1;

	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_AREA_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_AREA_DATA *)list_nthdata(arg->d.blist, 1);

		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_AREA_DATA *)list_nthdata(arg->d.blist, -1);

		break;
	default: return NULL;
	}

	if( uid ) {
		if( uid->area ) {
			arg->d.area = (AREA_DATA *)uid->area;
			arg->type = ENT_AREA;
		} else {
			arg->d.aid = uid->uid;
			arg->type = ENT_AREA_ID;
		}
	} else {
		arg->d.area = NULL;
		arg->type = ENT_AREA;
	}

	return str+1;
}

char *expand_entity_blist_area_region(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_AREA_REGION_DATA *region = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (LLIST_AREA_REGION_DATA *)list_nthdata(arg->d.blist, index);

		if( region ) {
			if( region->aregion ) {
				arg->d.aregion = (AREA_REGION *)region->aregion;
				arg->type = ENT_AREA_REGION;
			} else {
				arg->d.arid.aid = region->aid;
				arg->d.arid.rid = region->rid;
				arg->type = ENT_AREA_REGION_ID;
			}
		} else {
			arg->d.aregion = NULL;
			arg->type = ENT_AREA_REGION;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		return str+1;

	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (LLIST_AREA_REGION_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (LLIST_AREA_REGION_DATA *)list_nthdata(arg->d.blist, 1);

		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (LLIST_AREA_REGION_DATA *)list_nthdata(arg->d.blist, -1);

		break;
	default: return NULL;
	}

	if( region ) {
		if( region->aregion ) {
			arg->d.aregion = (AREA_REGION *)region->aregion;
			arg->type = ENT_AREA_REGION;
		} else {
			arg->d.arid.aid = region->aid;
			arg->d.arid.rid = region->rid;
			arg->type = ENT_AREA_REGION_ID;
		}
	} else {
		arg->d.aregion = NULL;
		arg->type = ENT_AREA_REGION;
	}

	return str+1;
}

char *expand_entity_blist_wilds(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_WILDS_DATA *uid = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_WILDS_DATA *)list_nthdata(arg->d.blist, index);

		if( uid ) {
			if( uid->wilds ) {
				arg->d.wilds = (WILDS_DATA *)uid->wilds;
				arg->type = ENT_WILDS;
			} else {
				arg->d.wid = uid->uid;
				arg->type = ENT_WILDS_ID;
			}
		} else {
			arg->d.wilds = NULL;
			arg->type = ENT_WILDS;
		}

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		return str+1;

	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_WILDS_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_WILDS_DATA *)list_nthdata(arg->d.blist, 1);

		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			uid = (LLIST_WILDS_DATA *)list_nthdata(arg->d.blist, -1);

		break;
	default: return NULL;
	}

	if( uid ) {
		if( uid->wilds ) {
			arg->d.wilds = (WILDS_DATA *)uid->wilds;
			arg->type = ENT_WILDS;
		} else {
			arg->d.wid = uid->uid;
			arg->type = ENT_WILDS_ID;
		}
	} else {
		arg->d.wilds = NULL;
		arg->type = ENT_WILDS;
	}

	return str+1;
}

char *expand_entity_blist_exit(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_EXIT_DATA *e = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			e = (LLIST_EXIT_DATA *)list_nthdata(arg->d.blist, index);

		if( e ) {
			arg->d.door.r = e->room;
			arg->d.door.door = e->door;
		} else {
			arg->d.door.r = NULL;
			arg->d.door.door = 0;
		}
		arg->type = ENT_EXIT;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		return str+1;

	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			e = (LLIST_EXIT_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			e = (LLIST_EXIT_DATA *)list_nthdata(arg->d.blist, 1);

		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			e = (LLIST_EXIT_DATA *)list_nthdata(arg->d.blist, -1);

		break;
	default: return NULL;
	}

	if( e ) {
		arg->d.door.r = e->room;
		arg->d.door.door = e->door;
	} else {
		arg->d.door.r = NULL;
		arg->d.door.door = 0;
	}
	arg->type = ENT_EXIT;

	return str+1;
}

char *expand_entity_blist_skillinfo(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_SKILL_DATA *s = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, index);

		if( s && s->mob && IS_VALID(s->skill) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.skill = s->skill;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.skill = NULL;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		if( s && s->mob && IS_VALID(s->skill) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.skill = s->skill;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.skill = NULL;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, 1);

		if( s && s->mob && IS_VALID(s->skill) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.skill = s->skill;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.skill = NULL;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, -1);

		if( s && s->mob && IS_VALID(s->skill) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.skill = s->skill;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.skill = NULL;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_conn(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register DESCRIPTOR_DATA *conn = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			conn = (DESCRIPTOR_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.conn = conn;
		arg->type = ENT_CONN;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			conn = (DESCRIPTOR_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.conn = conn;
		arg->type = ENT_CONN;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			conn = (DESCRIPTOR_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.conn = conn;
		arg->type = ENT_CONN;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			conn = (DESCRIPTOR_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.conn = conn;
		arg->type = ENT_CONN;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_church(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register CHURCH_DATA *church = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			church = (CHURCH_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.church = church;
		arg->type = ENT_CHURCH;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			church = (CHURCH_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.church = church;
		arg->type = ENT_CHURCH;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			church = (CHURCH_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.church = church;
		arg->type = ENT_CHURCH;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			church = (CHURCH_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.church = church;
		arg->type = ENT_CHURCH;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_book_page(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register BOOK_PAGE *book_page = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			book_page = (BOOK_PAGE *)list_nthdata(arg->d.blist, index);

		arg->d.book_page = book_page;
		arg->type = ENT_BOOK_PAGE;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			book_page = (BOOK_PAGE *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.book_page = book_page;
		arg->type = ENT_BOOK_PAGE;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			book_page = (BOOK_PAGE *)list_nthdata(arg->d.blist, 1);

		arg->d.book_page = book_page;
		arg->type = ENT_BOOK_PAGE;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			book_page = (BOOK_PAGE *)list_nthdata(arg->d.blist, -1);

		arg->d.book_page = book_page;
		arg->type = ENT_BOOK_PAGE;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_food_buff(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register FOOD_BUFF_DATA *food_buff = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			food_buff = (FOOD_BUFF_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.food_buff = food_buff;
		arg->type = ENT_FOOD_BUFF;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			food_buff = (FOOD_BUFF_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.food_buff = food_buff;
		arg->type = ENT_FOOD_BUFF;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			food_buff = (FOOD_BUFF_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.food_buff = food_buff;
		arg->type = ENT_FOOD_BUFF;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			food_buff = (FOOD_BUFF_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.food_buff = food_buff;
		arg->type = ENT_FOOD_BUFF;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_compartment(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register FURNITURE_COMPARTMENT *compartment = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			compartment = (FURNITURE_COMPARTMENT *)list_nthdata(arg->d.blist, index);

		arg->d.compartment = compartment;
		arg->type = ENT_COMPARTMENT;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			compartment = (FURNITURE_COMPARTMENT *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.compartment = compartment;
		arg->type = ENT_COMPARTMENT;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			compartment = (FURNITURE_COMPARTMENT *)list_nthdata(arg->d.blist, 1);

		arg->d.compartment = compartment;
		arg->type = ENT_COMPARTMENT;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			compartment = (FURNITURE_COMPARTMENT *)list_nthdata(arg->d.blist, -1);

		arg->d.compartment = compartment;
		arg->type = ENT_COMPARTMENT;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_mob(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register CHAR_DATA *ch = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			ch = (CHAR_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.mob = ch;
		arg->type = ENT_MOBILE;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			ch = (CHAR_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.mob = ch;
		arg->type = ENT_MOBILE;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			ch = (CHAR_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.mob = ch;
		arg->type = ENT_MOBILE;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			ch = (CHAR_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.mob = ch;
		arg->type = ENT_MOBILE;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_obj(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register OBJ_DATA *obj = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			obj = (OBJ_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.obj = obj;
		arg->type = ENT_OBJECT;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			obj = (OBJ_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.obj = obj;
		arg->type = ENT_OBJECT;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			obj = (OBJ_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.obj = obj;
		arg->type = ENT_OBJECT;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			obj = (OBJ_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.obj = obj;
		arg->type = ENT_OBJECT;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register ROOM_INDEX_DATA *room = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			room = (ROOM_INDEX_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.room = room;
		arg->type = ENT_ROOM;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			room = (ROOM_INDEX_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.room = room;
		arg->type = ENT_ROOM;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			room = (ROOM_INDEX_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.room = room;
		arg->type = ENT_ROOM;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			room = (ROOM_INDEX_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.room = room;
		arg->type = ENT_ROOM;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_token(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register TOKEN_DATA *token = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			token = (TOKEN_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.token = token;
		arg->type = ENT_TOKEN;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			token = (TOKEN_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.token = token;
		arg->type = ENT_TOKEN;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			token = (TOKEN_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.token = token;
		arg->type = ENT_TOKEN;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			token = (TOKEN_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.token = token;
		arg->type = ENT_TOKEN;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_area(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register AREA_DATA *area = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			area = (AREA_DATA *)list_nthdata(arg->d.blist, index);

		arg->d.area = area;
		arg->type = ENT_AREA;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			area = (AREA_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.area = area;
		arg->type = ENT_AREA;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			area = (AREA_DATA *)list_nthdata(arg->d.blist, 1);

		arg->d.area = area;
		arg->type = ENT_AREA;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			area = (AREA_DATA *)list_nthdata(arg->d.blist, -1);

		arg->d.area = area;
		arg->type = ENT_AREA;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_area_region(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register AREA_REGION *region = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (AREA_REGION *)list_nthdata(arg->d.blist, index);

		arg->d.aregion = region;
		arg->type = ENT_AREA_REGION;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (AREA_REGION *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.aregion = region;
		arg->type = ENT_AREA_REGION;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (AREA_REGION *)list_nthdata(arg->d.blist, 1);

		arg->d.aregion = region;
		arg->type = ENT_AREA_REGION;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			region = (AREA_REGION *)list_nthdata(arg->d.blist, -1);

		arg->d.aregion = region;
		arg->type = ENT_AREA_REGION;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_plist_variable(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register VARIABLE *variable = NULL;
	switch(*str) {
	case ESCAPE_EXPRESSION:
	{
		int index;
		str = expand_argument_expression(info,str+1,&index);
		if (!str) return NULL;

		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			variable = (VARIABLE *)list_nthdata(arg->d.blist, index);

		arg->d.variable = variable;
		arg->type = ENT_VARIABLE;

		return str;
	}
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			variable = (VARIABLE *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		arg->d.variable = variable;
		arg->type = ENT_VARIABLE;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			variable = (VARIABLE *)list_nthdata(arg->d.blist, 1);

		arg->d.variable = variable;
		arg->type = ENT_VARIABLE;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			variable = (VARIABLE *)list_nthdata(arg->d.blist, -1);

		arg->d.variable = variable;
		arg->type = ENT_VARIABLE;
		break;
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_group(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register CHAR_DATA *rch;
	register int count;

	switch(*str) {
	case ENTITY_GROUP_OWNER:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.group_owner;
		break;

	case ENTITY_GROUP_LEADER:
		arg->type = ENT_MOBILE;
		arg->d.mob = (arg->d.group_owner ? arg->d.group_owner->leader : arg->d.group_owner);
		break;

	case ENTITY_GROUP_ALLY:
		arg->type = ENT_MOBILE;
		if(arg->d.group_owner && arg->d.group_owner->in_room)
		{
			for(count = 0, rch = arg->d.group_owner->in_room->people; rch; rch = rch->next_in_room)
			{
				if(rch != arg->d.group_owner && arg->d.group_owner->leader == rch->leader)
					++count;
			}

			count = number_range(1, count);
			for(rch = arg->d.group_owner->in_room->people; rch && count > 0; rch = rch->next_in_room)
			{
				if(rch != arg->d.group_owner && arg->d.group_owner->leader == rch->leader) {
					--count;

					if( count < 1)
						break;
				}
			}

			arg->d.mob = rch;
		}
		else
			arg->d.mob = NULL;
		break;

	case ENTITY_GROUP_MEMBER:
		arg->type = ENT_MOBILE;
		if(arg->d.group_owner && arg->d.group_owner->in_room)
		{
			for(count = 0, rch = arg->d.group_owner->in_room->people; rch; rch = rch->next_in_room)
			{
				if(arg->d.group_owner->leader == rch->leader)
					++count;
			}

			count = number_range(1, count);
			for(rch = arg->d.group_owner->in_room->people; rch && count > 0; rch = rch->next_in_room)
			{
				if(arg->d.group_owner->leader == rch->leader) {
					--count;

					if( count < 1)
						break;
				}
			}

			arg->d.mob = rch;
		}
		else
			arg->d.mob = NULL;
		break;

	case ENTITY_GROUP_MEMBERS:
		arg->type = ENT_ILLIST_MOB_GROUP;
		break;

	case ENTITY_GROUP_SIZE:
		arg->type = ENT_NUMBER;

		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_song(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SONG_DATA *song = arg->d.song;

	switch(*str) {
	case ENTITY_SONG_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, (song ? song->name : ""));
		arg->d.str = buf_string(arg->buffer);
		break;
	
	case ENTITY_SONG_UID:
		arg->type = ENT_NUMBER;
		arg->d.num = song ? song->uid : 0;
		break;

	case ENTITY_SONG_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = song ? song->flags : 0;
		arg->d.bv.table = song_flags;
		break;

	case ENTITY_SONG_TOKEN:
		arg->type = ENT_TOKENINDEX;
		arg->d.tokindex = song ? song->token : NULL;
		break;

	case ENTITY_SONG_TARGET:
		arg->type = ENT_STAT;
		arg->d.stat.value = song ? song->target : -1;
		arg->d.stat.table = song_target_types;
		arg->d.stat.def_value = -2;
		break;

	case ENTITY_SONG_BEATS:
		arg->type = ENT_NUMBER;
		arg->d.num = song ? song->beats : -1;
		break;

	case ENTITY_SONG_MANA:
		arg->type = ENT_NUMBER;
		arg->d.num = song ? song->mana : -1;
		break;

	case ENTITY_SONG_LEVEL:
		arg->type = ENT_NUMBER;
		arg->d.num = song ? song->level : -1;
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_prior(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	info = arg->d.info;

	switch(*str) {
	case ENTITY_PRIOR_MOB:
		arg->type = ENT_MOBILE;
		arg->d.mob = info ? info->mob : NULL;
		break;

	case ENTITY_PRIOR_OBJ:
		arg->type = ENT_OBJECT;
		arg->d.obj = info ? info->obj : NULL;
		break;

	case ENTITY_PRIOR_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = info ? info->room : NULL;
		break;

	case ENTITY_PRIOR_TOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = info ? info->token : NULL;
		break;

	case ENTITY_PRIOR_ENACTOR:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->ch;
		break;
	case ENTITY_PRIOR_OBJ1:
		arg->type = ENT_OBJECT;
		arg->d.obj = info->obj1;
		break;
	case ENTITY_PRIOR_OBJ2:
		arg->type = ENT_OBJECT;
		arg->d.obj = info->obj2;
		break;
	case ENTITY_PRIOR_VICTIM:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->vch;
		break;
	case ENTITY_PRIOR_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->targ ? *info->targ : NULL;
		break;
	case ENTITY_PRIOR_RANDOM:
		arg->type = ENT_MOBILE;
		arg->d.mob = info->rch;
		break;
	case ENTITY_PRIOR_HERE:
		if(info->mob)
			arg->d.room = info->mob->in_room;
		else if(info->obj)
			arg->d.room = obj_room(info->obj);
		else if(info->room)
			arg->d.room = info->room;
		else if(info->token)
			arg->d.room = token_room(info->token);
		else return NULL;
		arg->type = ENT_ROOM;
		break;

	case ENTITY_PRIOR_PHRASE:
		arg->type = ENT_STRING;
		arg->d.str = info->phrase;
		break;
	case ENTITY_PRIOR_TRIGGER:
		arg->type = ENT_STRING;
		arg->d.str = info->trigger;
		break;

	case ENTITY_PRIOR_REGISTER1:
	case ENTITY_PRIOR_REGISTER2:
	case ENTITY_PRIOR_REGISTER3:
	case ENTITY_PRIOR_REGISTER4:
	case ENTITY_PRIOR_REGISTER5:
		arg->type = ENT_NUMBER;
		arg->d.num = info->registers[*str-ENTITY_REGISTER1];
		break;

	case ENTITY_PRIOR_PRIOR:
		arg->type = ENT_PRIOR;
		arg->d.info = script_get_prior(arg->d.info);
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_dice(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_DICE_NUMBER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.dice ? arg->d.dice->number : 0;
		break;

	case ENTITY_DICE_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.dice ? arg->d.dice->size : 0;
		break;

	case ENTITY_DICE_BONUS:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.dice ? arg->d.dice->bonus: 0;
		break;

	case ENTITY_DICE_ROLL:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.dice ? dice_roll(arg->d.dice) : 0;
		break;

	case ENTITY_DICE_LAST:
		arg->type = ENT_NUMBER;
		if( arg->d.dice ) {
			if( arg->d.dice->last_roll > 0 )
				arg->d.num = arg->d.dice->last_roll;
			else
				arg->d.num = dice_roll(arg->d.dice);
		}
		else
			arg->d.num = 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_range(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	int min = UMIN(arg->d.range.min, arg->d.range.max);
	int max = UMAX(arg->d.range.min, arg->d.range.max);

	switch(*str) {
	case ENTITY_RANGE_MIN:
		arg->type = ENT_NUMBER;
		arg->d.num = min;
		break;

	case ENTITY_RANGE_MAX:
		arg->type = ENT_NUMBER;
		arg->d.num = max;
		break;

	case ENTITY_RANGE_SPAN:
		arg->type = ENT_NUMBER;
		arg->d.num = max - min;
		break;

	case ENTITY_RANGE_AVERAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = (max + min) / 2;
		break;

	case ENTITY_RANGE_VALUE:
		arg->type = ENT_NUMBER;
		arg->d.num = number_range(min, max);
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_mobindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	info = arg->d.info;
	MOB_INDEX_DATA *mobindex = arg->d.mobindex;

	switch(*str) {
	case ENTITY_MOBINDEX_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = mobindex ? mobindex->area : NULL;
		arg->d.wnum.vnum = mobindex ? mobindex->vnum : 0;
		break;
	case ENTITY_MOBINDEX_LOADED:
		arg->type = ENT_NUMBER;
		arg->d.num = mobindex ? mobindex->count : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_objindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	info = arg->d.info;
	OBJ_INDEX_DATA *objindex = arg->d.objindex;

	switch(*str) {
	case ENTITY_OBJINDEX_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = objindex ? objindex->area : NULL;
		arg->d.wnum.vnum = objindex ? objindex->vnum : 0;
		break;
	case ENTITY_OBJINDEX_LOADED:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->count : 0;
		break;
	case ENTITY_OBJINDEX_INROOMS:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->inrooms : 0;
		break;
	case ENTITY_OBJINDEX_INMAIL:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->inmail : 0;
		break;
	case ENTITY_OBJINDEX_CARRIED:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->carried : 0;
		break;
	case ENTITY_OBJINDEX_LOCKERED:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->lockered : 0;
		break;
	case ENTITY_OBJINDEX_INCONTAINER:
		arg->type = ENT_NUMBER;
		arg->d.num = objindex ? objindex->incontainer : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_tokenindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	info = arg->d.info;
	TOKEN_INDEX_DATA *tokenindex = arg->d.tokindex;

	switch(*str) {
	case ENTITY_TOKENINDEX_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = tokenindex ? tokenindex->area : NULL;
		arg->d.wnum.vnum = tokenindex ? tokenindex->vnum : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_instance_section(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	INSTANCE_SECTION *section = arg->d.section;

	switch(*str) {
	case ENTITY_SECTION_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = IS_VALID(section) ? section->rooms : NULL;
		break;

	case ENTITY_SECTION_INDEX:
		arg->type = ENT_BLUEPRINT_SECTION;
		arg->d.bs = IS_VALID(section) ? section->section : NULL;
		break;
	
	case ENTITY_SECTION_INSTANCE:
		arg->type = ENT_INSTANCE;
		arg->d.instance = (IS_VALID(section) && IS_VALID(section->instance)) ? section->instance : NULL;
		break;
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_blueprint_section(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	BLUEPRINT_SECTION *section = arg->d.bs;

	switch(*str) {
	case ENTITY_BLUEPRINT_SECTION_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = section ? section->area : NULL;
		arg->d.wnum.vnum = section ? section->vnum : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_instance(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	INSTANCE *instance = arg->d.instance;

	switch(*str) {
	case ENTITY_INSTANCE_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, IS_VALID(instance) ? instance->blueprint->name : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_INSTANCE_BLUEPRINT:
		arg->type = ENT_BLUEPRINT;
		arg->d.bp = IS_VALID(instance) ? instance->blueprint : NULL;
		break;

	case ENTITY_INSTANCE_SECTIONS:
		arg->type = ENT_ILLIST_SECTIONS;
		arg->d.blist = IS_VALID(instance) ? instance->sections : NULL;
		break;

	case ENTITY_INSTANCE_OWNERS:
		arg->type = ENT_BLLIST_MOB;
		arg->d.blist = IS_VALID(instance) ? instance->player_owners : NULL;
		break;

	case ENTITY_INSTANCE_OBJECT:
		if( IS_VALID(instance->object) )
		{
			arg->type = ENT_OBJECT;
			arg->d.obj = instance->object;
		}
		else if( instance->object_uid[0] > 0 || instance->object_uid[1] > 0)
		{
			arg->type = ENT_OBJECT_ID;
			arg->d.uid[0] = instance->object_uid[0];
			arg->d.uid[1] = instance->object_uid[1];
		}
		else
		{
			arg->type = ENT_OBJECT;
			arg->d.obj = NULL;
		}
		break;

	case ENTITY_INSTANCE_DUNGEON:
		arg->type = ENT_DUNGEON;
		arg->d.dungeon = IS_VALID(instance->dungeon) ? instance->dungeon : NULL;
		break;

	case ENTITY_INSTANCE_QUEST:
		// TODO: Fix QUEST entity
		return NULL;

	case ENTITY_INSTANCE_SHIP:
		arg->type = ENT_SHIP;
		arg->d.ship = IS_VALID(instance->ship) ? instance->ship : NULL;
		break;

	case ENTITY_INSTANCE_FLOOR:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(instance) ? instance->floor : 0;
		break;

	case ENTITY_INSTANCE_ENTRY:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(instance) ? instance->entrance : NULL;
		break;

	case ENTITY_INSTANCE_EXIT:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(instance) ? instance->exit : NULL;
		break;

	case ENTITY_INSTANCE_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(instance) ? instance->recall : NULL;
		break;

	case ENTITY_INSTANCE_ENVIRON:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(instance) ? instance->environ : NULL;
		break;

	case ENTITY_INSTANCE_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = IS_VALID(instance) ? instance->rooms : NULL;
		break;

	case ENTITY_INSTANCE_PLAYERS:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(instance) ? instance->players : NULL;
		break;

	case ENTITY_INSTANCE_MOBILES:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(instance) ? instance->mobiles : NULL;
		break;

	case ENTITY_INSTANCE_OBJECTS:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = IS_VALID(instance) ? instance->objects : NULL;
		break;

	case ENTITY_INSTANCE_BOSSES:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(instance) ? instance->bosses : NULL;
		break;

	case ENTITY_INSTANCE_SPECIAL_ROOMS:
		arg->type = ENT_ILLIST_SPECIALROOMS;
		arg->d.blist = IS_VALID(instance) ? instance->special_rooms : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_blueprint(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	BLUEPRINT *blueprint = arg->d.bp;

	switch(*str) {
	case ENTITY_BLUEPRINT_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = blueprint ? blueprint->area : NULL;
		arg->d.wnum.vnum = blueprint ? blueprint->vnum : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_dungeon(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	DUNGEON *dungeon = arg->d.dungeon;

	switch(*str) {
	case ENTITY_DUNGEON_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, IS_VALID(dungeon) ? dungeon->index->name : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_DUNGEON_INDEX:
		arg->type = ENT_DUNGEONINDEX;
		arg->d.dngindex = IS_VALID(arg->d.dungeon) ? arg->d.dungeon->index : NULL;
		break;

	case ENTITY_DUNGEON_FLOORS:
		arg->type = ENT_ILLIST_INSTANCES;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->floors : NULL;
		break;

	case ENTITY_DUNGEON_DESC:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, IS_VALID(dungeon) ? dungeon->index->description : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_DUNGEON_OWNERS:
		arg->type = ENT_BLLIST_MOB;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->player_owners : NULL;
		break;

	case ENTITY_DUNGEON_ENTRY:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(dungeon) ? dungeon->entry_room : NULL;
		break;

	case ENTITY_DUNGEON_EXIT:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(dungeon) ? dungeon->exit_room : NULL;
		break;

	case ENTITY_DUNGEON_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->rooms : NULL;
		break;

	case ENTITY_DUNGEON_PLAYERS:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->players : NULL;
		break;

	case ENTITY_DUNGEON_MOBILES:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->mobiles : NULL;
		break;

	case ENTITY_DUNGEON_OBJECTS:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->objects : NULL;
		break;

	case ENTITY_DUNGEON_BOSSES:
		arg->type = ENT_PLLIST_MOB;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->bosses : NULL;
		break;

	case ENTITY_DUNGEON_SPECIAL_ROOMS:
		arg->type = ENT_ILLIST_SPECIALROOMS;
		arg->d.blist = IS_VALID(dungeon) ? dungeon->special_rooms : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_dungeonindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	DUNGEON_INDEX_DATA *dungeon = arg->d.dngindex;

	switch(*str) {
	case ENTITY_DUNGEONINDEX_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = dungeon ? dungeon->area : NULL;
		arg->d.wnum.vnum = dungeon ? dungeon->vnum : 0;
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_ship(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SHIP_DATA *ship = arg->d.ship;

	switch(*str) {
	case ENTITY_SHIP_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, IS_VALID(ship) ? ship->ship_name : "");
		arg->d.str = buf_string(arg->buffer);
		break;

	case ENTITY_SHIP_INDEX:
		arg->type = ENT_SHIPINDEX;
		arg->d.shipindex = IS_VALID(arg->d.ship) ? arg->d.ship->index : NULL;
		break;

	case ENTITY_SHIP_OBJECT:
		arg->type = ENT_OBJECT;
		arg->d.obj = ship->ship;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_shipindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SHIP_INDEX_DATA *ship = arg->d.shipindex;

	switch(*str) {
	case ENTITY_SHIPINDEX_WNUM:
		arg->type = ENT_WIDEVNUM;
		arg->d.wnum.pArea = ship ? ship->area : NULL;
		arg->d.wnum.vnum = ship ? ship->vnum : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_extradesc(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
//	EXTRA_DESCR_DATA *ed;
	switch(*str) {
	case ESCAPE_VARIABLE:
		if(!arg->d.list.owner || !arg->d.list.ptr.ed || !*arg->d.list.ptr.ed || !info->var) return NULL;

/*
		{
			char store[MSL];
			char msg[MSL*2];
			expand_escape2print(str,store);
			sprintf(msg,"expand_entity_extradesc->str = \"%s\"",store);
			wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
*/

		BUFFER *buffer = new_buf();

		str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
		if(!str) {
			free_buf(buffer);
			return NULL;
		}

/*
		{
			char msg[MSL*2];
			sprintf(msg,"expand_entity_extradesc->\"%s\"",buf);
			wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
*/

		arg->type = ENT_STRING;
		int owner_type = arg->d.list.owner_type;
		EXTRA_DESCR_DATA *edesc = get_extra_descr(buf_string(buffer), *arg->d.list.ptr.ed);

		if( edesc != NULL )
		{
			char *desc = edesc->description;

			// Enviroment ED
			if( !desc )
			{
				ROOM_INDEX_DATA *environ = NULL;

				if( owner_type == ENT_ROOM )
				{
					environ = get_environment((ROOM_INDEX_DATA *)arg->d.list.owner);
				}
				else if( owner_type == ENT_OBJECT )
				{
					OBJ_DATA *obj = (OBJ_DATA *)arg->d.list.owner;

					if( obj )
					{
						if( !obj->carried_by && !obj->in_obj && !obj->locker && !obj->in_mail)
						{
							environ = get_environment(obj->in_room);
						}
					}
				}

				if( environ )
				{
					desc = environ->description;
				}
			}

			arg->d.str = desc;
		}
		else
			arg->d.str = NULL;
		if (!arg->d.str) arg->d.str = str_dup("");
		free_buf(buffer);
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_stat(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	long val = arg->d.stat.value;
	const struct flag_type *table = arg->d.stat.table;
	long def_val = arg->d.stat.def_value;
	switch(*str)
	{
	case ESCAPE_VARIABLE:
		arg->type = ENT_BOOLEAN;

		if(table)
		{
			BUFFER *buffer = new_buf();
			long value;
			str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
			if(!str) {
				free_buf(buffer);
				arg->d.boolean = FALSE;
				return NULL;
			}


			value = script_stat_lookup(buf_string(buffer), table, def_val);

			arg->d.boolean = (val == value);
			free_buf(buffer);
		}
		else
		{
			arg->d.boolean = FALSE;
		}
		break;
	case ENTITY_STAT_VALUE:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.stat.value;
		break;
	case ENTITY_STAT_NAME:
		arg->type = ENT_STRING;
		clear_buf(arg->buffer);
		add_buf(arg->buffer, flag_string(table, val));
		arg->d.str = buf_string(arg->buffer);
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_bitvector(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_BOOLEAN;

		if(arg->d.bv.table && arg->d.bv.value)
		{
			BUFFER *buffer = new_buf();
			int bit;
			str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
			if(!str) {
				free_buf(buffer);
				arg->d.boolean = FALSE;
				return NULL;
			}


			bit = script_flag_lookup(buf_string(buffer), arg->d.bv.table);

			arg->d.boolean = IS_SET(arg->d.bv.value, bit) && TRUE;
			free_buf(buffer);
		}
		else
		{
			arg->d.boolean = FALSE;
		}
		break;
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_bitmatrix(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_BOOLEAN;
		if(arg->d.bm.bank && arg->d.bm.values)
		{
			BUFFER *buffer = new_buf();
			str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
			if(!str) {
				free_buf(buffer);
				arg->d.boolean = FALSE;
				return NULL;
			}

			arg->d.boolean = bitmatrix_isset(buf_string(buffer), arg->d.bm.bank, arg->d.bm.values);
			free_buf(buffer);
		}
		else
		{
			arg->d.boolean = FALSE;
		}
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_reserved_mobile(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_MOBINDEX;

		BUFFER *buffer = new_buf();
		str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
		if(!str) {
			free_buf(buffer);
			arg->d.mobindex = NULL;
			return NULL;
		}

		RESERVED_WNUM *reserved = search_reserved(reserved_mob_wnums, buf_string(buffer));
		if (reserved && reserved->data)
			arg->d.mobindex = *((MOB_INDEX_DATA **)reserved->data);
		else
			arg->d.mobindex = NULL;
		free_buf(buffer);
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_reserved_object(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_OBJINDEX;

		BUFFER *buffer = new_buf();
		str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
		if(!str) {
			free_buf(buffer);
			arg->d.objindex = NULL;
			return NULL;
		}

		RESERVED_WNUM *reserved = search_reserved(reserved_obj_wnums, buf_string(buffer));
		if (reserved && reserved->data)
			arg->d.objindex = *((OBJ_INDEX_DATA **)reserved->data);
		else
			arg->d.objindex = NULL;
		free_buf(buffer);
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_reserved_room(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_ROOM;

		BUFFER *buffer = new_buf();
		str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
		if(!str) {
			free_buf(buffer);
			arg->d.room = NULL;
			return NULL;
		}

		RESERVED_WNUM *reserved = search_reserved(reserved_room_wnums, buf_string(buffer));
		if (reserved && reserved->data)
			arg->d.room = *((ROOM_INDEX_DATA **)reserved->data);
		else
			arg->d.room = NULL;
		free_buf(buffer);
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_book(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	BOOK_DATA *book = IS_VALID(arg->d.obj) && IS_BOOK(arg->d.obj) ? BOOK(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_BOOK_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(book) ? book->name : "";
		break;
	
	case ENTITY_OBJ_BOOK_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(book) ? book->short_descr : "";
		break;

	case ENTITY_OBJ_BOOK_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(book) ? book->flags : 0;
		arg->d.bv.table = IS_VALID(book) ? book_flags : NULL;
		break;

	case ENTITY_OBJ_BOOK_CURRENT:
		arg->type = ENT_BOOK_PAGE;
		arg->d.book_page = IS_VALID(book) ? book_get_page(book, book->current_page) : NULL;
		break;

	case ENTITY_OBJ_BOOK_OPENER:
		arg->type = ENT_BOOK_PAGE;
		arg->d.book_page = IS_VALID(book) ? book_get_page(book, book->open_page) : NULL;
		break;
	
	case ENTITY_OBJ_BOOK_PAGES:
		arg->type = ENT_PLLIST_BOOK_PAGE;
		arg->d.blist = IS_VALID(book) ? book->pages : NULL;
		break;

	case ENTITY_OBJ_BOOK_LOCK:
		arg->type = ENT_LOCK_STATE;
		arg->d.lockstate = IS_VALID(book) ? book->lock : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_container(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	CONTAINER_DATA *container = IS_VALID(arg->d.obj) && IS_CONTAINER(arg->d.obj) ? CONTAINER(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_CONTAINER_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(container) ? container->name : "";
		break;
	
	case ENTITY_OBJ_CONTAINER_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(container) ? container->short_descr : "";
		break;

	case ENTITY_OBJ_CONTAINER_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(container) ? container->flags : 0;
		arg->d.bv.table = IS_VALID(container) ? container_flags : NULL;
		break;

	case ENTITY_OBJ_CONTAINER_MAX_WEIGHT:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(container) ? container->max_weight : -1;
		break;

	case ENTITY_OBJ_CONTAINER_WEIGHT_MULTIPLIER:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(container) ? container->weight_multiplier : -1;
		break;
		
	case ENTITY_OBJ_CONTAINER_MAX_VOLUME:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(container) ? container->max_volume : -1;
		break;

	case ENTITY_OBJ_CONTAINER_WHITELIST:
		// Need a way to look for this
		return NULL;

	case ENTITY_OBJ_CONTAINER_BLACKLIST:
		// Need a way to look for this
		return NULL;

	case ENTITY_OBJ_CONTAINER_LOCK:
		arg->type = ENT_LOCK_STATE;
		arg->d.lockstate = IS_VALID(container) ? container->lock : NULL;
		break;
		
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_object_fluid_container(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	FLUID_CONTAINER_DATA *fluid = IS_VALID(arg->d.obj) && IS_FLUID_CON(arg->d.obj) ? FLUID_CON(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_FLUID_CONTAINER_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(fluid) ? fluid->name : "";
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(fluid) ? fluid->short_descr : "";
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(fluid) ? fluid->flags : 0;
		arg->d.bv.table = IS_VALID(fluid) ? fluid_con_flags : NULL;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_LIQUID:
		arg->type = ENT_LIQUID;
		arg->d.liquid = IS_VALID(fluid) ? fluid->liquid : NULL;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_AMOUNT:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(fluid) ? fluid->amount : 0;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_CAPACITY:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(fluid) ? fluid->capacity : 0;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_REFILL_RATE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(fluid) ? fluid->refill_rate : 0;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_POISON:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(fluid) ? fluid->poison : 0;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_POISON_RATE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(fluid) ? fluid->poison_rate : 0;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_SPELLS:
		arg->type = ENT_ILLIST_SPELLS;
		arg->d.blist = IS_VALID(fluid) ? fluid->spells : NULL;
		break;

	case ENTITY_OBJ_FLUID_CONTAINER_LOCK:
		arg->type = ENT_LOCK_STATE;
		arg->d.lockstate = IS_VALID(fluid) ? fluid->lock : NULL;
		break;
		
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_food(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	FOOD_DATA *food = IS_VALID(arg->d.obj) && IS_FOOD(arg->d.obj) ? FOOD(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_FOOD_HUNGER:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(food) ? food->hunger : -1;
		break;
		
	case ENTITY_OBJ_FOOD_FULL:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(food) ? food->full : -1;
		break;
		
	case ENTITY_OBJ_FOOD_POISON:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(food) ? food->poison : -1;
		break;

	case ENTITY_OBJ_FOOD_BUFFS:
		arg->type = ENT_PLLIST_FOOD_BUFF;
		arg->d.blist = IS_VALID(food) ? food->buffs : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_furniture(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	FURNITURE_DATA *furniture = IS_VALID(arg->d.obj) && IS_FURNITURE(arg->d.obj) ? FURNITURE(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_FURNITURE_MAIN_COMPARTMENT:
	{
		if (IS_VALID(furniture) && furniture->main_compartment > 0)
			arg->d.compartment = (FURNITURE_COMPARTMENT *)list_nthdata(furniture->compartments, furniture->main_compartment);
		else
			arg->d.compartment = NULL;
		
		arg->type = ENT_COMPARTMENT;
		break;
	}

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_ink(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_OBJ_INK_TYPES:
		arg->type = ENT_INK_TYPES;
		// Still uses the object
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_ink_types(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *obj = arg->d.obj;

	switch(*str)
	{
		case ESCAPE_EXPRESSION:
		{
			int index;
			str = expand_argument_expression(info,str+1,&index);
			if (!str) return NULL;

			arg->type = ENT_INK_TYPE;
			arg->d.obj_type.obj = obj;
			arg->d.obj_type.type = index;
			return str;
		}

		default:
			return NULL;
	}
}


char *expand_entity_ink_type(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *obj = arg->d.obj_type.obj;
	INK_DATA *ink = IS_VALID(obj) && IS_INK(obj) ? INK(obj) : NULL;
	int type = arg->d.obj_type.type;

	switch(*str)
	{
		case ENTITY_INK_TYPE_CATALYST:
			arg->type = ENT_STAT;
			arg->d.stat.value = IS_VALID(ink) && (type >= 1 && type <= MAX_INK_TYPES) ? ink->types[type] : NO_FLAG;
			arg->d.stat.table = catalyst_types;
			arg->d.stat.def_value = CATALYST_NONE;
			break;

		case ENTITY_INK_TYPE_AMOUNT:
			arg->type = ENT_NUMBER;
			arg->d.num = IS_VALID(ink) && (type >= 1 && type <= MAX_INK_TYPES) ? ink->amounts[type] : -1;
			break;

		default:
			return NULL;
	}

	return str+1;
}

char *expand_entity_object_instrument(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	INSTRUMENT_DATA *instrument = IS_VALID(arg->d.obj) && IS_INSTRUMENT(arg->d.obj) ? INSTRUMENT(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_INSTRUMENT_TYPE:
		arg->type = ENT_STAT;
		arg->d.stat.value = IS_VALID(instrument) ? instrument->type : NO_FLAG;
		arg->d.stat.table = instrument_types;
		arg->d.stat.def_value = INSTRUMENT_NONE;
		break;

	case ENTITY_OBJ_INSTRUMENT_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(instrument) ? instrument->flags : 0;
		arg->d.bv.table = instrument_flags;
		break;

	case ENTITY_OBJ_INSTRUMENT_BEATS:
		arg->type = ENT_RANGE;
		arg->d.range.min = IS_VALID(instrument) ? instrument->beats_min : 0;
		arg->d.range.max = IS_VALID(instrument) ? instrument->beats_max : 0;
		break;

	case ENTITY_OBJ_INSTRUMENT_MANA:
		arg->type = ENT_RANGE;
		arg->d.range.min = IS_VALID(instrument) ? instrument->mana_min : 0;
		arg->d.range.max = IS_VALID(instrument) ? instrument->mana_max : 0;
		break;

	case ENTITY_OBJ_INSTRUMENT_RESERVOIRS:
		arg->type = ENT_INSTRUMENT_RESERVOIRS;
		// Uses the object
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_instrument_reservoirs(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *obj = arg->d.obj;

	switch(*str)
	{
		case ESCAPE_EXPRESSION:
		{
			int index;
			str = expand_argument_expression(info,str+1,&index);
			if (!str) return NULL;

			arg->type = ENT_INSTRUMENT_RESERVOIR;
			arg->d.obj_type.obj = obj;
			arg->d.obj_type.type = index;
			return str;
		}

		default:
			return NULL;
	}
}

char *expand_entity_instrument_reservoir(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *obj = arg->d.obj_type.obj;
	INSTRUMENT_DATA *instrument = IS_VALID(obj) && IS_INSTRUMENT(obj) ? INSTRUMENT(obj) : NULL;
	int type = arg->d.obj_type.type;

	INSTRUMENT_CATALYST *reservoir = IS_VALID(instrument) && (type >= 1 && type <= INSTRUMENT_MAX_CATALYSTS) ? &instrument->reservoirs[type] : NULL;

	switch(*str)
	{
		case ENTITY_INSTRUMENT_RESERVOIR_CATALYST:
			arg->type = ENT_STAT;
			arg->d.stat.value = reservoir ? reservoir->type : NO_FLAG;
			arg->d.stat.table = catalyst_types;
			arg->d.stat.def_value = CATALYST_NONE;
			break;

		case ENTITY_INSTRUMENT_RESERVOIR_AMOUNT:
			arg->type = ENT_NUMBER;
			arg->d.num = reservoir ? reservoir->amount : -1;
			break;

		case ENTITY_INSTRUMENT_RESERVOIR_CAPACITY:
			arg->type = ENT_NUMBER;
			arg->d.num = reservoir ? reservoir->capacity : -1;
			break;

		default:
			return NULL;
	}

	return str+1;
}


char *expand_entity_object_light(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	LIGHT_DATA *light = IS_VALID(arg->d.obj) && IS_LIGHT(arg->d.obj) ? LIGHT(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_LIGHT_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(light) ? light->flags : 0;
		arg->d.bv.table = IS_VALID(light) ? light_flags : NULL;
		break;
	
	case ENTITY_OBJ_LIGHT_DURATION:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(light) ? light->duration : 0;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_money(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	MONEY_DATA *money = IS_VALID(arg->d.obj) && IS_MONEY(arg->d.obj) ? MONEY(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_MONEY_SILVER:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(money) ? money->silver : -1;
		break;

	case ENTITY_OBJ_MONEY_GOLD:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(money) ? money->gold : -1;
		break;

	default: return NULL;
	}

	return str+1;
}

ROOM_INDEX_DATA *get_portal_destination(CHAR_DATA *ch, OBJ_DATA *portal, bool allow_random);

char *expand_entity_object_portal(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	OBJ_DATA *obj = arg->d.obj;
	PORTAL_DATA *portal = IS_VALID(arg->d.obj) && IS_PORTAL(arg->d.obj) ? PORTAL(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_PORTAL_NAME:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(portal) ? portal->name : "";
		break;
	
	case ENTITY_OBJ_PORTAL_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = IS_VALID(portal) ? portal->short_descr : "";
		break;

	case ENTITY_OBJ_PORTAL_EXIT:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(portal) ? portal->flags : 0;
		arg->d.bv.table = IS_VALID(portal) ? portal_exit_flags : NULL;
		break;

	case ENTITY_OBJ_PORTAL_FLAGS:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = IS_VALID(portal) ? portal->flags : 0;
		arg->d.bv.table = IS_VALID(portal) ? portal_flags : NULL;
		break;

	case ENTITY_OBJ_PORTAL_TYPE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->type : -1;
		break;

	// This is dependent upon $(enactor) being valid
	case ENTITY_OBJ_PORTAL_DESTINATION:
		arg->type = ENT_ROOM;
		arg->d.room = IS_VALID(obj) ? get_portal_destination(info->ch, obj, TRUE) : NULL;
		break;

	case ENTITY_OBJ_PORTAL_PARAM0:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->params[0] : -1;
		break;

	case ENTITY_OBJ_PORTAL_PARAM1:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->params[1] : -1;
		break;

	case ENTITY_OBJ_PORTAL_PARAM2:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->params[2] : -1;
		break;

	case ENTITY_OBJ_PORTAL_PARAM3:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->params[3] : -1;
		break;

	case ENTITY_OBJ_PORTAL_PARAM4:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(portal) ? portal->params[4] : -1;
		break;
		
	case ENTITY_OBJ_PORTAL_LOCK:
		arg->type = ENT_LOCK_STATE;
		arg->d.lockstate = IS_VALID(portal) ? portal->lock : NULL;
		break;
		
	case ENTITY_OBJ_PORTAL_SPELLS:
		arg->type = ENT_ILLIST_SPELLS;
		arg->d.blist = IS_VALID(portal) ? portal->spells : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_scroll(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	SCROLL_DATA *scroll = IS_VALID(arg->d.obj) && IS_SCROLL(arg->d.obj) ? SCROLL(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_SCROLL_MAXMANA:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(scroll) ? scroll->max_mana : 0;
		break;

	case ENTITY_OBJ_SCROLL_SPELLS:
		arg->type = ENT_ILLIST_SPELLS;
		arg->d.blist = IS_VALID(scroll) ? scroll->spells : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_tattoo(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	TATTOO_DATA *tattoo = IS_VALID(arg->d.obj) && IS_TATTOO(arg->d.obj) ? TATTOO(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_TATTOO_TOUCHES:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(tattoo) ? tattoo->touches : 0;
		break;

	case ENTITY_OBJ_TATTOO_FADE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(tattoo) ? tattoo->fading_chance : 0;
		break;

	case ENTITY_OBJ_TATTOO_FADING:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(tattoo) ? tattoo->fading_rate : 0;
		break;

	case ENTITY_OBJ_TATTOO_SPELLS:
		arg->type = ENT_ILLIST_SPELLS;
		arg->d.blist = IS_VALID(tattoo) ? tattoo->spells : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_object_wand(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	WAND_DATA *wand = IS_VALID(arg->d.obj) && IS_WAND(arg->d.obj) ? WAND(arg->d.obj) : NULL;

	switch(*str) {
	case ENTITY_OBJ_WAND_CHARGES:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(wand) ? wand->charges : 0;
		break;

	case ENTITY_OBJ_WAND_MAXCHARGES:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(wand) ? wand->max_charges : 0;
		break;

	case ENTITY_OBJ_WAND_COOLDOWN:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(wand) ? wand->cooldown : 0;
		break;

	case ENTITY_OBJ_WAND_RECHARGE:
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(wand) ? wand->recharge_time : 0;
		break;

	case ENTITY_OBJ_WAND_SPELLS:
		arg->type = ENT_ILLIST_SPELLS;
		arg->d.blist = IS_VALID(wand) ? wand->spells : NULL;
		break;

	default: return NULL;
	}

	return str+1;
}

char *expand_entity_catalyst_usage(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	int catalyst = (*str) + CATALYST_NONE - ESCAPE_EXTRA;

	if (catalyst > CATALYST_NONE && catalyst < CATALYST_MAX)
	{
		arg->type = ENT_NUMBER;
		arg->d.num = IS_VALID(arg->d.mob) ?  arg->d.mob->catalyst_usage[catalyst] : 0;
		return str+1;
	}

	return NULL;
}


char *expand_argument_entity(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
//	char buf[MSL];
	char *next;
	arg->type = ENT_NONE;		// ENT_PRIMARY
	arg->d.num = 0;

	while(str && *str && *str != ESCAPE_END) {
//		sprintf(buf,"expand_argument_entity: %d", arg->type);
//		wiznet(buf, NULL, NULL, WIZ_TESTING, 0, 0);

		switch(arg->type) {
		case ENT_PRIMARY:		next = expand_entity_primary(info,str,arg); break;
		case ENT_BOOLEAN:		next = expand_entity_boolean(info,str,arg); break;
		case ENT_NUMBER:		next = expand_entity_number(info,str,arg); break;
		case ENT_STRING:		next = expand_entity_string(info,str,arg); break;
		case ENT_WIDEVNUM:		next = expand_entity_widevnum(info,str,arg); break;
		case ENT_MOBILE:		next = expand_entity_mobile(info,str,arg); break;
		case ENT_OBJECT:		next = expand_entity_object(info,str,arg); break;
		case ENT_ROOM:			next = expand_entity_room(info,str,arg); break;
		case ENT_ARRAY_EXITS:	next = expand_entity_array_exits(info,str,arg); break;
		case ENT_EXIT:			next = expand_entity_exit(info,str,arg); break;
		case ENT_TOKEN:			next = expand_entity_token(info,str,arg); break;
		case ENT_AREA:			next = expand_entity_area(info,str,arg); break;
		case ENT_AREA_REGION:	next = expand_entity_area_region(info,str,arg); break;
		case ENT_OLLIST_MOB:	next = expand_entity_list_mob(info,str,arg); break;
		case ENT_OLLIST_OBJ:	next = expand_entity_list_obj(info,str,arg); break;
		case ENT_OLLIST_TOK:	next = expand_entity_list_token(info,str,arg); break;
		case ENT_OLLIST_AFF:	next = expand_entity_list_affect(info,str,arg); break;
		case ENT_SKILL:		next = expand_entity_skill(info,str,arg); break;
		case ENT_SKILL_VALUES:	next = expand_entity_skill_values(info,str,arg); break;
		case ENT_SKILL_VALUENAMES:	next = expand_entity_skill_valuenames(info,str,arg); break;
		case ENT_SKILLENTRY:	next = expand_entity_skillentry(info,str,arg); break;
		case ENT_SKILLINFO:	next = expand_entity_skillinfo(info,str,arg); break;
		case ENT_CONN:		next = expand_entity_conn(info,str,arg); break;
		case ENT_WILDS:		next = expand_entity_wilds(info,str,arg); break;
		case ENT_CHURCH:	next = expand_entity_church(info,str,arg); break;
		case ENT_EXTRADESC:	next = expand_entity_extradesc(info,str,arg); break;
		case ENT_AFFECT:	next = expand_entity_affect(info,str,arg); break;
		case ENT_BOOK_PAGE:	next = expand_entity_book_page(info,str,arg); break;
		case ENT_FOOD_BUFF:	next = expand_entity_food_buff(info,str,arg); break;
		case ENT_COMPARTMENT:	next = expand_entity_compartment(info,str,arg); break;
		case ENT_LIQUID:		next = expand_entity_liquid(info,str,arg); break;
		case ENT_SPELL:		next = expand_entity_spelldata(info,str,arg); break;
		case ENT_LOCK_STATE:	next = expand_entity_lockstate(info,str,arg); break;
		case ENT_SONG:		next = expand_entity_song(info,str,arg); break;
		case ENT_CLONE_ROOM:	next = expand_entity_clone_room(info,str,arg); break;
		case ENT_WILDS_ROOM:	next = expand_entity_wilds_room(info,str,arg); break;
		case ENT_CLONE_DOOR:	next = expand_entity_clone_door(info,str,arg); break;
		case ENT_WILDS_DOOR:	next = expand_entity_wilds_door(info,str,arg); break;

		case ENT_BLLIST_ROOM:	next = expand_entity_blist_room(info,str,arg); break;
		case ENT_BLLIST_MOB:		next = expand_entity_blist_mob(info,str,arg); break;
		case ENT_BLLIST_OBJ:		next = expand_entity_blist_obj(info,str,arg); break;
		case ENT_BLLIST_TOK:		next = expand_entity_blist_token(info,str,arg); break;
		case ENT_BLLIST_EXIT:	next = expand_entity_blist_exit(info,str,arg); break;
		case ENT_BLLIST_SKILL:	next = expand_entity_blist_skillinfo(info,str,arg); break;
		case ENT_BLLIST_AREA:	next = expand_entity_blist_area(info,str,arg); break;
		case ENT_BLLIST_AREA_REGION:	next = expand_entity_blist_area_region(info,str,arg); break;
		case ENT_BLLIST_WILDS:	next = expand_entity_blist_wilds(info,str,arg); break;

		case ENT_PLLIST_STR:		next = expand_entity_plist_str(info,str,arg); break;
		case ENT_PLLIST_CONN:	next = expand_entity_plist_conn(info,str,arg); break;
		case ENT_PLLIST_ROOM:	next = expand_entity_plist_room(info,str,arg); break;
		case ENT_PLLIST_MOB:		next = expand_entity_plist_mob(info,str,arg); break;
		case ENT_PLLIST_OBJ:		next = expand_entity_plist_obj(info,str,arg); break;
		case ENT_PLLIST_TOK:		next = expand_entity_plist_token(info,str,arg); break;
		case ENT_PLLIST_AREA:		next = expand_entity_plist_area(info,str,arg); break;
		case ENT_PLLIST_AREA_REGION:	next = expand_entity_plist_area_region(info,str,arg); break;
		case ENT_PLLIST_CHURCH:	next = expand_entity_plist_church(info,str,arg); break;
		case ENT_PLLIST_BOOK_PAGE:	next = expand_entity_plist_book_page(info,str,arg); break;
		case ENT_PLLIST_FOOD_BUFF:	next = expand_entity_plist_food_buff(info,str,arg); break;
		case ENT_PLLIST_COMPARTMENT:	next = expand_entity_plist_compartment(info,str,arg); break;

		case ENT_MOBILE_ID:		next = expand_entity_mobile_id(info,str,arg); break;
		case ENT_OBJECT_ID:		next = expand_entity_object_id(info,str,arg); break;
		case ENT_TOKEN_ID:		next = expand_entity_token_id(info,str,arg); break;
		case ENT_AREA_ID:		next = expand_entity_area_id(info,str,arg); break;
		case ENT_AREA_REGION_ID:next = expand_entity_area_region_id(info,str,arg); break;
		case ENT_SKILLINFO_ID:	next = expand_entity_skillinfo_id(info,str,arg); break;
		case ENT_WILDS_ID:		next = expand_entity_wilds_id(info,str,arg); break;
		case ENT_CHURCH_ID:		next = expand_entity_church_id(info,str,arg); break;
		case ENT_GAME:			next = expand_entity_game(info,str,arg); break;
		case ENT_PERSIST:		next = expand_entity_persist(info,str,arg); break;
		case ENT_PRIOR:			next = expand_entity_prior(info,str,arg); break;
		case ENT_VARIABLE:		next = expand_entity_variable(info,str,arg); break;
		case ENT_GROUP:			next = expand_entity_group(info,str,arg); break;
		case ENT_DICE:			next = expand_entity_dice(info,str,arg); break;
		case ENT_RANGE:			next = expand_entity_range(info,str,arg); break;
		case ENT_MOBINDEX:		next = expand_entity_mobindex(info,str,arg); break;
		case ENT_OBJINDEX:		next = expand_entity_objindex(info,str,arg); break;
		case ENT_TOKENINDEX:	next = expand_entity_tokenindex(info,str,arg); break;

		case ENT_SECTION:		next = expand_entity_instance_section(info,str,arg); break;
		case ENT_INSTANCE:		next = expand_entity_instance(info,str,arg); break;
		case ENT_DUNGEON:		next = expand_entity_dungeon(info,str,arg); break;
		case ENT_SHIP:			next = expand_entity_ship(info,str,arg); break;

		case ENT_BLUEPRINT_SECTION:		next = expand_entity_blueprint_section(info,str,arg); break;
		case ENT_BLUEPRINT:				next = expand_entity_blueprint(info,str,arg); break;
		case ENT_DUNGEONINDEX:			next = expand_entity_dungeonindex(info,str,arg); break;
		case ENT_SHIPINDEX:				next = expand_entity_shipindex(info,str,arg); break;

		// Multi-typing
		case ENT_OBJECT_PAGE:			next = expand_entity_object_page(info,str,arg); break;
		case ENT_OBJECT_BOOK:			next = expand_entity_object_book(info,str,arg); break;
		case ENT_OBJECT_CONTAINER:		next = expand_entity_object_container(info,str,arg); break;
		case ENT_OBJECT_FLUID_CONTAINER:	next = expand_entity_object_fluid_container(info,str,arg); break;
		case ENT_OBJECT_FOOD:			next = expand_entity_object_food(info,str,arg); break;
		case ENT_OBJECT_FURNITURE:		next = expand_entity_object_furniture(info,str,arg); break;
		case ENT_OBJECT_INK:			next = expand_entity_object_ink(info,str,arg); break;
		case ENT_OBJECT_INSTRUMENT:		next = expand_entity_object_instrument(info,str,arg); break;
		case ENT_OBJECT_LIGHT:			next = expand_entity_object_light(info,str,arg); break;
		case ENT_OBJECT_MONEY:			next = expand_entity_object_money(info,str,arg); break;
		case ENT_OBJECT_PORTAL:			next = expand_entity_object_portal(info,str,arg); break;
		case ENT_OBJECT_SCROLL:			next = expand_entity_object_scroll(info,str,arg); break;
		case ENT_OBJECT_TATTOO:			next = expand_entity_object_tattoo(info,str,arg); break;
		case ENT_OBJECT_WAND:			next = expand_entity_object_wand(info,str,arg); break;

		case ENT_RESERVED_MOBILE:		next = expand_entity_reserved_mobile(info,str,arg); break;
		case ENT_RESERVED_OBJECT:		next = expand_entity_reserved_object(info,str,arg); break;
		case ENT_RESERVED_ROOM:			next = expand_entity_reserved_room(info,str,arg); break;

		case ENT_INK_TYPES:				next = expand_entity_ink_types(info,str,arg); break;
		case ENT_INK_TYPE:				next = expand_entity_ink_type(info,str,arg); break;
		case ENT_INSTRUMENT_RESERVOIRS:	next = expand_entity_instrument_reservoirs(info,str,arg); break;
		case ENT_INSTRUMENT_RESERVOIR:	next = expand_entity_instrument_reservoir(info,str,arg); break;
		case ENT_CATALYST_USAGE:		next = expand_entity_catalyst_usage(info,str,arg); break;

		case ENT_STAT:			next = expand_entity_stat(info,str,arg); break;
		case ENT_BITVECTOR:		next = expand_entity_bitvector(info,str,arg); break;
		case ENT_BITMATRIX:		next = expand_entity_bitmatrix(info,str,arg); break;
		case ENT_NULL:
			next = str+1;
			arg->type = ENT_NULL;
			arg->d.num = 0;
			break;

		default:				next = NULL; break;
		}

		if(next) str = next;
		else {
			str = expand_skip(str);
			arg->type = ENT_NONE;	// ENT_PRIMARY
			arg->d.num = 0;
		}
	}
	return str && *str ? str+1 : str;
}

char *expand_string_entity(SCRIPT_VARINFO *info,char *str, BUFFER *buffer)
{
	char buf[MIL];
	SCRIPT_PARAM *arg = new_script_param();

	str = expand_argument_entity(info,str,arg);
	if(!str || arg->type == ENT_NONE /* ENT_PRIMARY */) {
		free_script_param(arg);
		return NULL;
	}

	switch(arg->type) {
	default:
		add_buf(buffer, "{D<{x@{W@{x@{D>{x ");
		break;

	case ENT_BOOLEAN:
		add_buf(buffer, arg->d.boolean ? "true" : "false");
		break;
	
	case ENT_WIDEVNUM:
		if (arg->d.wnum.pArea && arg->d.wnum.vnum > 0)
		{
			sprintf(buf, "%ld#%ld", arg->d.wnum.pArea->uid, arg->d.wnum.vnum);
			add_buf(buffer, buf);
		}
		else
			add_buf(buffer, "0#0");
		break;

	case ENT_NUMBER:
		sprintf(buf, "%d", arg->d.num);
		add_buf(buffer, buf);
		break;

	case ENT_STRING:
		add_buf(buffer, arg->d.str ? arg->d.str : "(null)");
		break;

	case ENT_MOBILE:
		if( IS_VALID(arg->d.mob) )
		{
			if( IS_NPC(arg->d.mob) )
				add_buf(buffer, arg->d.mob->short_descr);
			else
				add_buf(buffer, arg->d.mob->name);
		}
		else
		{
			add_buf(buffer, SOMEONE);
		}
		break;

	case ENT_OBJECT:
		add_buf(buffer, arg->d.obj ? arg->d.obj->short_descr : SOMETHING);
		break;

	case ENT_ROOM:
		add_buf(buffer, arg->d.room ? arg->d.room->name : SOMEWHERE);
		break;

	case ENT_EXIT:
		add_buf(buffer, dir_name[arg->d.door.door]);
		break;

	case ENT_TOKEN:
		add_buf(buffer, (arg->d.token && arg->d.token->pIndexData) ? arg->d.token->pIndexData->name : SOMETHING);
		break;

	case ENT_AREA:
		add_buf(buffer, arg->d.area ? arg->d.area->name : SOMEWHERE);
		break;

	case ENT_AREA_REGION:
		add_buf(buffer, IS_VALID(arg->d.aregion) ? arg->d.aregion->name : SOMEWHERE);
		break;

	case ENT_CONN:
		add_buf(buffer, (arg->d.conn && arg->d.conn->character) ? arg->d.conn->character->name : SOMEONE);
		break;

	case ENT_INSTANCE:
		add_buf(buffer, IS_VALID(arg->d.instance) ? arg->d.instance->blueprint->name : SOMEWHERE);
		break;

	case ENT_DUNGEON:
		add_buf(buffer, IS_VALID(arg->d.dungeon) ? arg->d.dungeon->index->name : SOMEWHERE);
		break;

	case ENT_SHIP:
		add_buf(buffer, IS_VALID(arg->d.ship) ? arg->d.ship->ship_name : SOMETHING);
		break;

	case ENT_BITVECTOR:
		add_buf(buffer, flag_string(arg->d.bv.table, arg->d.bv.value));
		break;

	case ENT_BITMATRIX:
		add_buf(buffer, bitmatrix_string(arg->d.bm.bank, arg->d.bm.values));
		break;

	case ENT_STAT:
		add_buf(buffer, flag_string(arg->d.stat.table, arg->d.stat.value));
		break;

	case ENT_BOOK_PAGE:
		add_buf(buffer, IS_VALID(arg->d.book_page) ? arg->d.book_page->title : "");
		break;
	}

	free_script_param(arg);

	return str;
}

void expand_string_simple_code(SCRIPT_VARINFO *info,unsigned char code, BUFFER *buffer)
{
	char buf[MIL], *s = buf;

//	sprintf(buf,"expand_string_simple_code: code = %2.2X", code);
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	buf[0] = '\0';
	switch(code) {
	default:
		s = " {W<{x@{D@{x@{W>{x "; break;
//	case ESCAPE_UA:
//	case ESCAPE_UB:
//	case ESCAPE_UC:
//	case ESCAPE_UD:
	case ESCAPE_UE:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = (char*)he_she[URANGE(0, info->vch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UF:
		if(info->mob) s = (char*)himself[URANGE(0, info->mob->sex, 2)];
		else s = SOMEONE;
		break;
//	case ESCAPE_UG:
//	case ESCAPE_UH:
	case ESCAPE_UI:
		if(info->mob) s = info->mob->short_descr;
		else if(info->obj) s = info->obj->short_descr;
		else if(info->room) s = info->room->name;
		else if(info->token) s = info->token->pIndexData->name;
		break;
	case ESCAPE_UJ:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = (char*)he_she[URANGE(0, info->rch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UK:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = (char*)him_her[URANGE(0, info->rch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UL:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = (char*)his_hers[URANGE(0, info->rch->sex, 2)];
		else
			s = SOMEONES;
		break;
	case ESCAPE_UM:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = (char*)him_her[URANGE(0, info->vch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UN:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = ((IS_NPC(info->ch) || info->ch->morphed) ? info->ch->short_descr : capitalize(info->ch->name));
		else
			s = SOMEONE;
		break;
	case ESCAPE_UO:
		if(info->obj1 && (!info->mob || can_see_obj(info->mob,info->obj1)))
			s = info->obj1->short_descr;
		else
			s = SOMETHING;
		break;
	case ESCAPE_UP:
		if(info->obj2 && (!info->mob || can_see_obj(info->mob,info->obj2)))
			s = info->obj2->short_descr;
		else
			s = SOMETHING;
		break;
	case ESCAPE_UQ:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = ((IS_NPC((*info->targ)) || (*info->targ)->morphed) ? (*info->targ)->short_descr : capitalize((*info->targ)->name));
		else
			s = SOMEONE;
		break;
	case ESCAPE_UR:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = ((IS_NPC(info->rch) || info->rch->morphed) ? info->rch->short_descr : capitalize(info->rch->name));
		else
			s = SOMEONE;
		break;
	case ESCAPE_US:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = (char*)his_her[URANGE(0, info->vch->sex, 2)];
		else
			s = SOMEONES;
		break;
	case ESCAPE_UT:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = ((IS_NPC(info->vch) || info->vch->morphed) ? info->vch->short_descr : capitalize(info->vch->name));
		else
			s = SOMEONE;
		break;
	case ESCAPE_UU:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = (char*)his_hers[URANGE(0, info->vch->sex, 2)];
		else
			s = SOMEONES;
		break;
	case ESCAPE_UV:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = (char*)his_hers[URANGE(0, info->rch->sex, 2)];
		else
			s = SOMEONE;
		break;
//	case ESCAPE_UW:
	case ESCAPE_UX:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = (char*)he_she[URANGE(0, (*info->targ)->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UY:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = (char*)him_her[URANGE(0, (*info->targ)->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_UZ:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = (char*)his_her[URANGE(0, (*info->targ)->sex, 2)];
		else
			s = SOMEONES;
		break;
//	case ESCAPE_LA:
//	case ESCAPE_LB:
//	case ESCAPE_LC:
//	case ESCAPE_LD:
	case ESCAPE_LE:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = (char*)he_she[URANGE(0, info->ch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LF:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = (char*)himself[URANGE(0, info->ch->sex, 2)];
		else
			s = SOMEONE;
		break;
//	case ESCAPE_LG:
//	case ESCAPE_LH:
	case ESCAPE_LI:
		if(info->mob) one_argument(info->mob->name,buf);
		else if(info->obj) one_argument(info->obj->name,buf);
		else if(info->room) sprintf(buf,"%d",(int)info->room->vnum);
		else if(info->token) sprintf(buf,"%d",(int)info->token->pIndexData->vnum);
		break;
	case ESCAPE_LJ:
		if(info->mob) s = (char*)he_she[URANGE(0, info->mob->sex, 2)];
		else s = SOMEONE;
		break;
	case ESCAPE_LK:
		if(info->mob) s = (char*)him_her[URANGE(0, info->mob->sex, 2)];
		else s = SOMEONE;
		break;
	case ESCAPE_LL:
		if(info->mob) s = (char*)his_her[URANGE(0, info->mob->sex, 2)];
		else s = SOMEONES;
		break;
	case ESCAPE_LM:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = (char*)him_her[URANGE(0, info->ch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LN:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			one_argument(info->ch->name,buf);
		else
			s = SOMEONE;
		break;
	case ESCAPE_LO:
		if(info->obj1 && (!info->mob || can_see_obj(info->mob,info->obj1)))
			one_argument(info->obj1->name,buf);
		else
			s = SOMETHING;
		break;
	case ESCAPE_LP:
		if(info->obj2 && (!info->mob || can_see_obj(info->mob,info->obj2)))
			one_argument(info->obj2->name,buf);
		else
			s = SOMETHING;
		break;
	case ESCAPE_LQ:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			one_argument((*info->targ)->name,buf);
		else
			s = SOMEONE;
		break;
	case ESCAPE_LR:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			one_argument(info->rch->name,buf);
		else
			s = SOMEONE;
		break;
	case ESCAPE_LS:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = (char*)his_her[URANGE(0, info->ch->sex, 2)];
		else
			s = SOMEONES;
		break;
	case ESCAPE_LT:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			one_argument(info->vch->name,buf);
		else
			s = SOMEONE;
		break;
	case ESCAPE_LU:
		if(info->ch && (!info->mob || can_see(info->mob,info->ch)))
			s = (char*)his_hers[URANGE(0, info->ch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LV:
		if(info->mob) s = (char*)his_hers[URANGE(0, info->mob->sex, 2)];
		else s = SOMEONES;
		break;
	case ESCAPE_LW:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = (char*)his_hers[URANGE(0, (*info->targ)->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LX:
		if(info->vch && (!info->mob || can_see(info->mob,info->vch)))
			s = (char*)himself[URANGE(0, info->vch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LY:
		if (!info->rch) info->rch = get_random_char(info->mob, info->obj, info->room, info->token);
		if(info->rch && (!info->mob || can_see(info->mob,info->rch)))
			s = (char*)himself[URANGE(0, info->rch->sex, 2)];
		else
			s = SOMEONE;
		break;
	case ESCAPE_LZ:
		if(info->targ && (*info->targ) && (!info->mob || can_see(info->mob,(*info->targ))))
			s = (char*)himself[URANGE(0, (*info->targ)->sex, 2)];
		else
			s = SOMEONE;
		break;
	}

	if(*s) {
		add_buf(buffer, s);
	}
}

char *expand_string_expression(SCRIPT_VARINFO *info,char *str, BUFFER *buffer)
{
	char buf[MIL];
	int num;

	str = expand_argument_expression(info,str,&num);
//	sprintf(buf,"expand_string_expression: str %08X, %2.2X", str, (str ? (*str&0xFF) : 0));
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	if(!str) return NULL;

	sprintf(buf,"%d",num);
	add_buf(buffer,buf);

//	sprintf(buf,"expand_string_expression: %d -> '%s'", num, *store);
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	//DBG3MSG2("num = '%s', str = %02.2X\n", *store, (*str)&0xFF);

	return str;
}

char *expand_string_variable(SCRIPT_VARINFO *info,char *str, BUFFER *buffer)
{
	pVARIABLE var;
	char buf[MIL];

	str = expand_variable(info,(info ? *(info->var) : NULL),str, &var);
	if(!str) return NULL;

	if(!var) {
		add_buf(buffer,"(null-var)");
	} else {
//		while(var->type == VAR_VARIABLE) {
//			var = var->_.variable;
//			if( !var ) {
//				strcpy(*store,"(null-var)");
//				*store += 10;
//				return str;
//			}
//		}

		switch(var->type) {
		case VAR_BOOLEAN:
			if(var->_.boolean) {
				add_buf(buffer,"true");
			} else {
				add_buf(buffer,"false");
			}
			break;
		case VAR_INTEGER:
			sprintf(buf,"%d",var->_.i);
			add_buf(buffer, buf);
			break;
		case VAR_STRING:
		case VAR_STRING_S:
			if(var->_.s && *var->_.s)
				add_buf(buffer,var->_.s);
			break;
		case VAR_ROOM:
			sprintf(buf,"%d",var->_.r ? (int)var->_.r->vnum : 0);
			add_buf(buffer,buf);
			break;
		case VAR_EXIT:
			add_buf(buffer,dir_name[var->_.door.door]);
			break;
		case VAR_MOBILE:
			if(var->_.m) {
				one_argument(var->_.m->name,buf);
				add_buf(buffer,buf);
			}
			break;
		case VAR_OBJECT:
			if(var->_.o) {
				one_argument(var->_.o->name,buf);
				add_buf(buffer,buf);
			}
			break;
		case VAR_TOKEN:
			sprintf(buf,"%d",var->_.t ? (int)var->_.t->pIndexData->vnum : 0);
			add_buf(buffer,buf);
			break;

		case VAR_CONNECTION:
			add_buf(buffer,((var->_.conn && var->_.conn->character) ? var->_.conn->character->name : SOMEONE));
			break;

		default:
			add_buf(buffer,"(null)");
			break;
		}
	}

	return str;
}

void expand_string_dump(char *str)
{
#ifdef DEBUG_MODULE
	char buf[MSL*4+1];
	int i;

	i = 0;
	while(*str)
		i += sprintf(buf+i," %02.2X", (*str++)&0xFF);
	buf[i] = 0;

	printf("str: %s\n", buf);
#endif
}

bool expand_string(SCRIPT_VARINFO *info,char *str,BUFFER *buffer)
{
	str = skip_whitespace(str);
	while(*str) {
		if(*str == ESCAPE_ENTITY)
			str = expand_string_entity(info,str+1,buffer);
		else if(*str == ESCAPE_EXPRESSION)
			str = expand_string_expression(info,str+1,buffer);
		else if(*str == ESCAPE_VARIABLE)
			str = expand_string_variable(info,str+1,buffer);
		else if((unsigned char)*str >= ESCAPE_UA && (unsigned char)*str <= ESCAPE_LZ) {
			expand_string_simple_code(info,*str,buffer);
			++str;
		} else
			add_buf_char(buffer, *str++);

		if(!str) {
			clear_buf(buffer);
			expand_string_dump(buf_string(buffer));
			DBG2EXITVALUE2(FALSE);
			return FALSE;
		}
	}

//	wiznet("EXPAND_STRING:",NULL,NULL,WIZ_TESTING,0,0);
//	wiznet(start,NULL,NULL,WIZ_TESTING,0,0);
	expand_string_dump(buf_string(buffer));
	DBG2EXITVALUE2(TRUE);
	return TRUE;
}

char *one_argument_escape( char *argument, char *arg_first )
{
	int depth;
	char cEnd;

	argument = skip_whitespace(argument);

	cEnd = ' ';
	if ( *argument == '\'' || *argument == '"' )
		cEnd = *argument++;

	while ( *argument && *argument != ESCAPE_END ) {
		if(*argument == ESCAPE_VARIABLE ||
		   *argument == ESCAPE_EXPRESSION ||
		   *argument == ESCAPE_ENTITY) {
			*arg_first++ = *argument++;
			depth = 0;
			while(*argument) {
				if(*argument == ESCAPE_VARIABLE ||
				   *argument == ESCAPE_EXPRESSION ||
				   *argument == ESCAPE_ENTITY)
					++depth;
				else if(*argument == ESCAPE_END && !depth--)
					break;
				*arg_first++ = *argument++;
			}
		}

		if ( *argument == cEnd ) {
			argument++;
			break;
		}
		*arg_first++ = *argument++;
	}
	*arg_first = '\0';

	return skip_whitespace(argument);
}

AREA_DATA *get_area_from_scriptinfo(SCRIPT_VARINFO *info)
{
	if (!info) return NULL;

	if (!info->block) return NULL;

	if (!info->block->script) return NULL;

	return info->block->script->area;
}

char *expand_argument(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char *buf = malloc(strlen(str) + 1);

	str = skip_whitespace(str);
	arg->type = ENT_NONE;		// ENT_PRIMARY
	clear_buf(arg->buffer);

	if(*str == ESCAPE_ENTITY)
		str = expand_argument_entity(info,str+1,arg);
	else if(*str == ESCAPE_EXPRESSION) {
		arg->type = ENT_NUMBER;
		str = expand_argument_expression(info,str+1,&arg->d.num);
	} else if(*str == ESCAPE_VARIABLE) {
		str = expand_argument_variable(info,str+1,arg);
	} else if((unsigned char)*str >= ESCAPE_UA && (unsigned char)*str <= ESCAPE_LZ) {
		expand_argument_simple_code(info,(unsigned char)(*str++),arg);
	} else if(*str) {
		str = one_argument_escape(str,buf);

		if(is_number(buf)) {
			arg->type = ENT_NUMBER;
			arg->d.num = atoi(buf);
		} else if(expand_string(info,buf,arg->buffer)) {
			arg->type = ENT_STRING;
			arg->d.str = buf_string(arg->buffer);

			// If this can be parsed as a widevnum...
			//  AUID#VNUM
			//  NAME#VNUM 
			//
			WNUM wnum;
			if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			{
				arg->type = ENT_WIDEVNUM;
				arg->d.wnum = wnum;
			}
			else if (is_number(arg->d.str))
			{
				int value = atoi(arg->d.str);
				arg->type = ENT_NUMBER;
				arg->d.num = value;
			}
		}
	}

	free(buf);

	return skip_whitespace(str);
}
