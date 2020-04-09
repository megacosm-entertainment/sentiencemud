
/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "scripts.h"
#include "wilds.h"
#include "tables.h"

//#define DEBUG_MODULE
#include "debug.h"

char *expand_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,pVARIABLE *var);
char *expand_string_expression(SCRIPT_VARINFO *info,char *str,char **store);

static bool wiznet_variables = FALSE;

// Check the validity of the script parameters
//	Should the ids mismatch, reset the field
bool check_varinfo(SCRIPT_VARINFO *info)
{
	bool ret = TRUE;
	return ret;
}

void expand_escape2print(char *str, char *store)
{
	while(*str) {
		if(isprint(*str)) *store++ = *str;
		else {
			sprintf(store,"0x%2.2X", *str);
			store+=4;
		}
		str++;
		if(*str) *store++ = ' ';
	}
	*store = 0;
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
char *expand_variable_recursive(SCRIPT_VARINFO *info, char *str,char **store)
{
//	char esc[MSL];
//	char msg[MSL*2];
	char buf[MSL], *p = buf;
	pVARIABLE var;
	pVARIABLE infovar = info ? *(info->var) : NULL;
/*
	{
		expand_escape2print(str,esc);
		sprintf(msg,"expand_variable_recursive->before = \"%s\"",esc);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}
*/

	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,&p);
			if(!str) return NULL;
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,&p);
			if(!str) return NULL;
		} else
			*p++ = *str++;
	}
	*p = 0;

/*
	{
		expand_escape2print(str,esc);
		sprintf(msg,"expand_variable_recursive->after = \"%s\"",esc);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}
*/

	var = variable_get(infovar,buf);
	if(var) {
		if((var->type == VAR_STRING || var->type == VAR_STRING_S) && var->_.s && *var->_.s) {
			strcpy(*store,var->_.s);
			*store += strlen(var->_.s);
		} else if(var->type == VAR_INTEGER) {
			*store += sprintf(*store,"%d",var->_.i);
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

	return str+1;
}

char *expand_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,pVARIABLE *var)
{
	char buf[MIL], *p = buf;
	bool deref = FALSE;
	pVARIABLE v;

	if(*str == ESCAPE_VARIABLE && wiznet_variables) {
		char msg[MSL];
		sprintf(msg,"expand_variable() -> *str IS ESCAPE_VARIABLE\n\r");
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}

	if(*str == '*') {
		deref = TRUE;
		++str;
	}

	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,&p);
			if(!str) {
				if(wiznet_variables) {
					char msg[MSL];
					*p = 0;
					sprintf(msg,"expand_variable(\"%s\") -> NULL str RETURN\n\r", buf);
					wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
				}
				return NULL;
			}
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,&p);
			if(!str) return NULL;
		} else
			*p++ = *str++;
	}
	*p = 0;

	if(*str != ESCAPE_END) {
		if(wiznet_variables) {
			char msg[MSL];
			sprintf(msg,"expand_variable(\"%s\") -> NO ESCAPE\n\r", buf);
			wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
		return NULL;
	}

	if(wiznet_variables) {
		char msg[MSL];
		sprintf(msg,"expand_variable(\"%s\")\n\r", buf);
		wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
	}

	v = variable_get(vars,buf);
	if(deref) {
		while(v && v->type == VAR_VARIABLE)
			v = v->_.variable;
	}


	*var = v;

	return str+1;
}

char *expand_name(SCRIPT_VARINFO *info,pVARIABLE vars,char *str,char *store)
{
	char buf[MSL], *p = buf;

	while(*str && *str != ESCAPE_END) {
		if(*str == ESCAPE_VARIABLE) {
			str = expand_variable_recursive(info,str+1,&p);
			if(!str) return NULL;
		} else if(*str == ESCAPE_EXPRESSION) {
			str = expand_string_expression(info,str+1,&p);
			if(!str) return NULL;
		} else
			*p++ = *str++;
	}
	*p = 0;

	if(*str != ESCAPE_END) return NULL;

	strcpy(store,buf);

	return str+1;
}

char *expand_number(char *str, int *num)
{
	char arg[MIL], *s = str, *p = arg;
	while(isdigit(*s)) *p++ = *s++;
 	*p = 0;
 	if(arg[0]) {
		str = s;
		*num = atoi(arg);
	} else
		*num = -1;
	return str;
}

char *expand_ifcheck(SCRIPT_VARINFO *info,char *str,int *value)
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
	int value,op;
	bool first = TRUE;
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
		} else if(*str == ESCAPE_VARIABLE) {	// Variable
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
		first = FALSE;
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
		case VAR_SKILL:		arg->type = ENT_SKILL; arg->d.sn = var->_.sn; break;
		case VAR_SKILLINFO:
			arg->type = ENT_SKILLINFO;
			arg->d.sk.m = var->_.sk.owner;
			arg->d.sk.t = var->_.sk.token;
			arg->d.sk.sn = var->_.sk.sn;
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
		case VAR_AFFECT:	arg->type = ENT_AFFECT; arg->d.aff = var->_.aff; break;

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

		case VAR_SKILLINFO_ID:
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.sn = var->_.skid.sn;
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
		case VAR_BLLIST_WILDS:	arg->d.blist = var->_.list; arg->type = ENT_BLLIST_WILDS; break;

		case VAR_PLLIST_STR:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_STR; break;
		case VAR_PLLIST_CONN:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_CONN; break;
		case VAR_PLLIST_ROOM:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_ROOM; break;
		case VAR_PLLIST_MOB:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_MOB; break;
		case VAR_PLLIST_OBJ:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_OBJ; break;
		case VAR_PLLIST_TOK:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_TOK; break;
		case VAR_PLLIST_CHURCH:	arg->d.blist = var->_.list;	arg->type = ENT_PLLIST_CHURCH; break;

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
			arg->d.sn = var->_.sn;
		else return NULL;

		arg->type = ENT_SKILL;
		break;

	case ENTITY_VAR_SKILLINFO:
		if(var) {
			if( var->type == VAR_SKILLINFO ) {
				arg->type = ENT_SKILLINFO;
				arg->d.sk.m = var->_.sk.owner;
				arg->d.sk.t = var->_.sk.token;
				arg->d.sk.sn = var->_.sk.sn;
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
				arg->d.sk.sn = var->_.skid.sn;
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
	case ENTITY_VAR_PLLIST_CHURCH:
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

	}
	return str;
}

char *expand_entity_primary(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
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
		strcpy(arg->buf,"Sentience");
		arg->d.str = arg->buf;
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
			strcpy(arg->buf, arg->d.wilds->name);
			arg->d.str = arg->buf;
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
			strcpy(arg->buf, arg->d.church->name);
			arg->d.str = arg->buf;
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_SIZE:
		arg->type = ENT_STRING;
		if( arg->d.church ) {
			strcpy(arg->buf, get_chsize_from_number(arg->d.church->size));
			arg->d.str = arg->buf;
		} else
			arg->d.str = &str_empty[0];

		break;

	case ENTITY_CHURCH_FLAG:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->flag && arg->d.church->flag[0] ) {
			strcpy(arg->buf, arg->d.church->flag);
			arg->d.str = arg->buf;
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
			strcpy(arg->buf, arg->d.church->founder);
			arg->d.str = arg->buf;
		} else
			arg->d.str = &str_empty[0];
		break;

	case ENTITY_CHURCH_MOTD:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->motd && arg->d.church->motd[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_MOTD) || script_security >= MAX_SCRIPT_SECURITY)) {
			strcpy(arg->buf, arg->d.church->motd);
			arg->d.str = arg->buf;
		} else
			arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_RULES:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->rules && arg->d.church->rules[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_RULES) || script_security >= MAX_SCRIPT_SECURITY) ) {
			strcpy(arg->buf, arg->d.church->rules);
			arg->d.str = arg->buf;
		} else
			arg->d.str = &str_empty[0];
		break;
	case ENTITY_CHURCH_INFO:
		arg->type = ENT_STRING;
		if( arg->d.church && arg->d.church->info && arg->d.church->info[0] && (IS_SET(arg->d.church->settings, CHURCH_PUBLIC_INFO) || script_security >= MAX_SCRIPT_SECURITY) ) {
			strcpy(arg->buf, arg->d.church->info);
			arg->d.str = arg->buf;
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
			strcpy(arg->buf, arg->d.variable->name);
			arg->d.str = arg->buf;
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

char *expand_entity_number(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	switch(*str) {
	case ENTITY_NUM_ABS:
		arg->d.num = abs(arg->d.num);
		break;
	default: return NULL;
	}

	return str+1;
}


char *expand_entity_string(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char *a, *b;

	switch(*str) {
	case ENTITY_STR_LEN:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.str ? strlen(arg->d.str) : 0;
		break;
	case ENTITY_STR_LOWER:
		for(a = arg->buf, b = arg->d.str; *b; a++, b++)
			*a = LOWER(*b);
		*a = '\0';
		arg->d.str = &arg->buf[0];
		break;
	case ENTITY_STR_UPPER:
		for(a = arg->buf, b = arg->d.str; *b; a++, b++)
			*a = UPPER(*b);
		*a = '\0';
		arg->d.str = &arg->buf[0];
		break;
	case ENTITY_STR_CAPITAL:
		for(a = arg->buf, b = arg->d.str; *b; a++, b++)
			*a = LOWER(*b);
		arg->buf[0] = UPPER(arg->buf[0]);
		*a = '\0';
		arg->d.str = &arg->buf[0];
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
		arg->d.str = arg->d.mob ? (char*)arg->d.mob->name : (char*)SOMEONE;
		break;
	case ENTITY_MOB_SHORT:
		arg->type = ENT_STRING;
		p = arg->d.mob ? (char*)((IS_NPC(arg->d.mob) || arg->d.mob->morphed) ? arg->d.mob->short_descr : capitalize(arg->d.mob->name)) : (char*)"no one";
		strcpy(arg->buf,p);
		arg->d.str = arg->buf;
		break;
	case ENTITY_MOB_LONG:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)arg->d.mob->long_descr : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SEX:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)male_female[URANGE(0,arg->d.mob->sex,2)] : (char*)male_female[0];
		break;
	case ENTITY_MOB_HE:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)he_she[URANGE(0,arg->d.mob->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIM:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)him_her[URANGE(0,arg->d.mob->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_HIS:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)his_her[URANGE(0,arg->d.mob->sex,2)] : (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIS_O:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)his_hers[URANGE(0,arg->d.mob->sex,2)] : (char*)SOMEONES;
		break;
	case ENTITY_MOB_HIMSELF:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)himself[URANGE(0,arg->d.mob->sex,2)] : (char*)SOMEONE;
		break;
	case ENTITY_MOB_RACE:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)race_table[arg->d.mob->race].name : "unknown";
		break;
	case ENTITY_MOB_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.mob ? arg->d.mob->in_room : NULL;
		break;
	case ENTITY_MOB_HOUSE:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.mob && arg->d.mob->home > 0 ? get_room_index(arg->d.mob->home) : NULL;
		break;
	case ENTITY_MOB_CARRYING:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = self ? &self->carrying : NULL;
		arg->d.list.owner = self;
		break;
	case ENTITY_MOB_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = self ? &self->tokens : NULL;
		arg->d.list.owner = self;
		break;
	case ENTITY_MOB_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = self ? &self->affected : NULL;
		arg->d.list.owner = self;
		break;
	case ENTITY_MOB_MOUNT:
		arg->d.mob = arg->d.mob ? arg->d.mob->mount : NULL;
		break;
	case ENTITY_MOB_RIDER:
		arg->d.mob = arg->d.mob ? arg->d.mob->rider : NULL;
		break;
	case ENTITY_MOB_MASTER:
		arg->d.mob = arg->d.mob ? arg->d.mob->master : NULL;
		break;
	case ENTITY_MOB_LEADER:
		arg->d.mob = arg->d.mob ? arg->d.mob->leader : NULL;
		break;
	case ENTITY_MOB_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob && arg->d.mob->owner ? arg->d.mob->owner : &str_empty[0];
		break;
	case ENTITY_MOB_OPPONENT:
		arg->d.mob = arg->d.mob ? arg->d.mob->fighting : NULL;
		break;
	case ENTITY_MOB_CART:
		arg->type = ENT_OBJECT;
		arg->d.obj = arg->d.mob ? arg->d.mob->pulled_cart : NULL;
		break;
	case ENTITY_MOB_FURNITURE:
		arg->type = ENT_OBJECT;
		arg->d.obj = arg->d.mob ? arg->d.mob->on : NULL;
		break;
	case ENTITY_MOB_TARGET:
		arg->d.mob = (arg->d.mob && arg->d.mob->progs) ? arg->d.mob->progs->target : NULL;
		break;
	case ENTITY_MOB_HUNTING:
		arg->d.mob = arg->d.mob ? arg->d.mob->hunting : NULL;
		break;
	case ENTITY_MOB_AREA:
		arg->type = ENT_AREA;
		arg->d.area = arg->d.mob && arg->d.mob->in_room ? arg->d.mob->in_room->area : NULL;
		break;
	case ENTITY_MOB_EQ_LIGHT:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LIGHT);
		break;
	case ENTITY_MOB_EQ_FINGER1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_FINGER_L);
		break;
	case ENTITY_MOB_EQ_FINGER2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_FINGER_R);
		break;
	case ENTITY_MOB_EQ_NECK1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_NECK_1);
		break;
	case ENTITY_MOB_EQ_NECK2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_NECK_2);
		break;
	case ENTITY_MOB_EQ_BODY:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_BODY);
		break;
	case ENTITY_MOB_EQ_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_HEAD);
		break;
	case ENTITY_MOB_EQ_LEGS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LEGS);
		break;
	case ENTITY_MOB_EQ_FEET:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_FEET);
		break;
	case ENTITY_MOB_EQ_HANDS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_HANDS);
		break;
	case ENTITY_MOB_EQ_ARMS:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_ARMS);
		break;
	case ENTITY_MOB_EQ_SHIELD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_SHIELD);
		break;
	case ENTITY_MOB_EQ_ABOUT:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_ABOUT);
		break;
	case ENTITY_MOB_EQ_WAIST:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_WAIST);
		break;
	case ENTITY_MOB_EQ_WRIST1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_WRIST_L);
		break;
	case ENTITY_MOB_EQ_WRIST2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_WRIST_R);
		break;
	case ENTITY_MOB_EQ_WIELD1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_WIELD);
		break;
	case ENTITY_MOB_EQ_HOLD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_HOLD);
		break;
	case ENTITY_MOB_EQ_WIELD2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_SECONDARY);
		break;
	case ENTITY_MOB_EQ_RING:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_RING_FINGER);
		break;
	case ENTITY_MOB_EQ_BACK:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_BACK);
		break;
	case ENTITY_MOB_EQ_SHOULDER:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_SHOULDER);
		break;
	case ENTITY_MOB_EQ_ANKLE1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_ANKLE_L);
		break;
	case ENTITY_MOB_EQ_ANKLE2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_ANKLE_R);
		break;
	case ENTITY_MOB_EQ_EAR1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_EAR_L);
		break;
	case ENTITY_MOB_EQ_EAR2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_EAR_R);
		break;
	case ENTITY_MOB_EQ_EYES:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_EYES);
		break;
	case ENTITY_MOB_EQ_FACE:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_FACE);
		break;
	case ENTITY_MOB_EQ_TATTOO_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_HEAD);
		break;
	case ENTITY_MOB_EQ_TATTOO_TORSO:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_TORSO);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_UPPER_ARM_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_ARM2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_UPPER_ARM_R);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_UPPER_LEG_L);
		break;
	case ENTITY_MOB_EQ_TATTOO_UPPER_LEG2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_UPPER_LEG_R);
		break;
        case ENTITY_MOB_EQ_TATTOO_LOWER_ARM1:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_LOWER_ARM_L);
                break;
        case ENTITY_MOB_EQ_TATTOO_LOWER_ARM2:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_LOWER_ARM_R);
                break;
        case ENTITY_MOB_EQ_TATTOO_LOWER_LEG1:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_LOWER_LEG_L);
                break;
        case ENTITY_MOB_EQ_TATTOO_LOWER_LEG2:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_LOWER_LEG_R);
                break;
        case ENTITY_MOB_EQ_TATTOO_SHOULDER1:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_SHOULDER_L);
                break;
        case ENTITY_MOB_EQ_TATTOO_SHOULDER2:
                arg->type = ENT_OBJECT;
                arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_SHOULDER_R);
                break;
	case ENTITY_MOB_EQ_TATTOO_BACK:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_TATTOO_BACK);
	case ENTITY_MOB_EQ_LODGED_HEAD:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_HEAD);
		break;
	case ENTITY_MOB_EQ_LODGED_TORSO:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_TORSO);
		break;
	case ENTITY_MOB_EQ_LODGED_ARM1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_ARM_L);
		break;
	case ENTITY_MOB_EQ_LODGED_ARM2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_ARM_R);
		break;
	case ENTITY_MOB_EQ_LODGED_LEG1:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_LEG_L);
		break;
	case ENTITY_MOB_EQ_LODGED_LEG2:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_LODGED_LEG_R);
		break;
	case ENTITY_MOB_EQ_ENTANGLED:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_ENTANGLED);
		break;
	case ENTITY_MOB_EQ_CONCEALED:
		arg->type = ENT_OBJECT;
		arg->d.obj = get_eq_char(arg->d.mob,WEAR_CONCEALED);
		break;
	case ENTITY_MOB_NEXT:
		arg->type = ENT_MOBILE;
		arg->d.mob = (arg->d.mob && arg->d.mob->in_room)?arg->d.mob->next_in_room:NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,(arg->d.mob && IS_NPC(arg->d.mob))?arg->d.mob->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_MOB_CASTSPELL:
		arg->type = ENT_SKILL;
		arg->d.sn = arg->d.mob ? arg->d.mob->cast_sn: -1;
		break;
	case ENTITY_MOB_CASTTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = arg->d.mob ? arg->d.mob->cast_token: NULL;
		break;
	case ENTITY_MOB_CASTTARGET:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)arg->d.mob->cast_target_name : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_SONG:
		arg->type = ENT_SONG;
		arg->d.song = arg->d.mob ? arg->d.mob->song_num : -1;
		break;
	case ENTITY_MOB_SONGTOKEN:
		arg->type = ENT_TOKEN;
		arg->d.token = arg->d.mob ? arg->d.mob->song_token: NULL;
		break;
	case ENTITY_MOB_SONGTARGET:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob ? (char*)arg->d.mob->music_target_name : (char*)&str_empty[0];
		break;
	case ENTITY_MOB_INSTRUMENT:
		arg->type = ENT_OBJECT;
		arg->d.obj = arg->d.mob ? arg->d.mob->song_instrument : NULL;
		break;
	case ENTITY_MOB_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.mob && location_isset(&arg->d.mob->recall)) ? location_to_room(&arg->d.mob->recall) : NULL;
		break;
	case ENTITY_MOB_CONNECTION:
		arg->type = ENT_CONN;
		arg->d.conn = arg->d.mob ? arg->d.mob->desc : NULL;	// Works for PCs and switched NPCs
		break;
	case ENTITY_MOB_CHURCH:
		arg->type = ENT_CHURCH;
		arg->d.church = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->church : NULL;	// Works for PCs and switched NPCs
		break;
	case ENTITY_MOB_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = arg->d.mob ? arg->d.mob->lclonerooms : NULL;
		break;
	case ENTITY_MOB_WORN:
		arg->type = ENT_PLLIST_OBJ;
		arg->d.blist = arg->d.mob ? arg->d.mob->lworn : NULL;
		break;
	case ENTITY_MOB_CHECKPOINT:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->checkpoint : NULL;
		break;
	case ENTITY_MOB_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (arg->d.mob && arg->d.mob->progs) ? &arg->d.mob->progs->vars : NULL;
		break;
	case ENTITY_MOB_GROUP:
		arg->type = ENT_GROUP;
		arg->d.group_owner = arg->d.mob;
		break;

	case ENTITY_MOB_DAMAGEDICE:
		arg->type = ENT_DICE;
		arg->d.dice = arg->d.mob ? &arg->d.mob->damage : NULL;;
		break;

	case ENTITY_MOB_ACT:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->act : 0;
		arg->d.bv.table = arg->d.mob ? (IS_NPC(arg->d.mob) ? act_flags : plr_flags) : NULL;
		break;

	case ENTITY_MOB_ACT2:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->act2 : 0;
		arg->d.bv.table = arg->d.mob ? (IS_NPC(arg->d.mob) ? act2_flags : plr2_flags) : NULL;
		break;

	case ENTITY_MOB_AFFECT:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->affected_by : 0;
		arg->d.bv.table = arg->d.mob ? affect_flags : NULL;
		break;

	case ENTITY_MOB_AFFECT2:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->affected_by2 : 0;
		arg->d.bv.table = arg->d.mob ? affect2_flags : NULL;
		break;

	case ENTITY_MOB_OFF:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = (arg->d.mob && IS_NPC(arg->d.mob)) ? arg->d.mob->off_flags : 0;
		arg->d.bv.table = (arg->d.mob && IS_NPC(arg->d.mob)) ? off_flags : NULL;
		break;

	case ENTITY_MOB_IMMUNE:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->imm_flags : 0;
		arg->d.bv.table = arg->d.mob ? imm_flags : NULL;
		break;

	case ENTITY_MOB_RESIST:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->res_flags : 0;
		arg->d.bv.table = arg->d.mob ? res_flags : NULL;
		break;

	case ENTITY_MOB_VULN:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.mob ? arg->d.mob->vuln_flags : 0;
		arg->d.bv.table = arg->d.mob ? vuln_flags : NULL;
		break;

	case ENTITY_MOB_TEMPSTRING:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.mob && arg->d.mob->tempstring ? arg->d.mob->tempstring : &str_empty[0];
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
		strcpy(arg->buf,"no one");
		arg->d.str = arg->buf;
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
		arg->type = ENT_STRING;
		arg->d.str = (char*)male_female[0];
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
		strcpy(arg->buf,"unknown");
		arg->d.str = arg->buf;
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
		break;
	case ENTITY_MOB_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		break;
	case ENTITY_MOB_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = NULL;
		arg->d.list.owner = NULL;
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
		arg->d.sn = -1;
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
		arg->d.song = -1;
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
	case ENTITY_MOB_ACT2:
	case ENTITY_MOB_AFFECT:
	case ENTITY_MOB_AFFECT2:
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
		arg->d.str = arg->d.obj ? arg->d.obj->name : SOMETHING;
		break;
	case ENTITY_OBJ_SHORT:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.obj ? arg->d.obj->short_descr : SOMETHING;
		break;
	case ENTITY_OBJ_LONG:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.obj ? arg->d.obj->description : &str_empty[0];
		break;
	case ENTITY_OBJ_FULLDESC:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.obj ? arg->d.obj->full_description : &str_empty[0];
		break;
	case ENTITY_OBJ_CONTAINER:
		arg->d.obj = arg->d.obj ? arg->d.obj->in_obj : NULL;
		break;
	case ENTITY_OBJ_FURNITURE:
		arg->d.obj = arg->d.obj ? arg->d.obj->on : NULL;
		break;
	case ENTITY_OBJ_CONTENTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = self ? &self->contains : NULL;
		arg->d.list.owner = self;
		break;
	case ENTITY_OBJ_OWNER:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.obj && arg->d.obj->owner ? arg->d.obj->owner : &str_empty[0];
		break;
	case ENTITY_OBJ_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.obj ? obj_room(arg->d.obj) : NULL;
		break;
	case ENTITY_OBJ_CARRIER:
		arg->type = ENT_MOBILE;
		arg->d.mob = arg->d.obj ? arg->d.obj->carried_by : NULL;
		break;
	case ENTITY_OBJ_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = self ? &self->tokens : NULL;
		arg->d.list.owner = self;
		break;
	case ENTITY_OBJ_TARGET:
		arg->d.mob = (arg->d.obj && arg->d.obj->progs) ? arg->d.obj->progs->target : NULL;
		break;
	case ENTITY_OBJ_AREA:
		arg->type = ENT_AREA;
		arg->d.room = arg->d.obj ? obj_room(arg->d.obj) : NULL;
		arg->d.area = arg->d.room ? arg->d.room->area : NULL;
		break;
	case ENTITY_OBJ_NEXT:
		arg->type = ENT_OBJECT;
		arg->d.obj = arg->d.obj?arg->d.obj->next_content:NULL;
		break;
	case ENTITY_OBJ_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.ed = arg->d.obj?arg->d.obj->extra_descr:NULL;
		break;
	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,arg->d.obj?arg->d.obj->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_OBJ_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = arg->d.obj ? arg->d.obj->lclonerooms : NULL;
		break;
	case ENTITY_OBJ_AFFECTS:
		arg->type = ENT_OLLIST_AFF;
		arg->d.list.ptr.aff = self ? &self->affected: NULL;
		arg->d.list.owner = self;
		break;

	case ENTITY_OBJ_INDEX:
		arg->type = ENT_OBJINDEX;
		arg->d.objindex = arg->d.obj ? arg->d.obj->pIndexData : NULL;
		break;

	case ENTITY_OBJ_EXTRA:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.obj ? arg->d.obj->extra_flags : 0;
		arg->d.bv.table = extra_flags;
		break;

	case ENTITY_OBJ_EXTRA2:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.obj ? arg->d.obj->extra2_flags : 0;
		arg->d.bv.table = extra2_flags;
		break;

	case ENTITY_OBJ_EXTRA3:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.obj ? arg->d.obj->extra3_flags : 0;
		arg->d.bv.table = extra3_flags;
		break;

	case ENTITY_OBJ_EXTRA4:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.obj ? arg->d.obj->extra4_flags : 0;
		arg->d.bv.table = extra4_flags;
		break;

	case ENTITY_OBJ_WEAR:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = arg->d.obj ? arg->d.obj->wear_flags : 0;
		arg->d.bv.table = wear_flags;
		break;

	case ENTITY_OBJ_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL;
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
		arg->d.ed = NULL;
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
		break;

	case ENTITY_OBJ_INDEX:
		arg->type = ENT_OBJINDEX;
		arg->d.objindex = NULL;
		break;

	case ENTITY_OBJ_EXTRA:
	case ENTITY_OBJ_EXTRA2:
	case ENTITY_OBJ_EXTRA3:
	case ENTITY_OBJ_EXTRA4:
	case ENTITY_OBJ_WEAR:
		arg->type = ENT_BITVECTOR;
		arg->d.bv.value = 0;
		arg->d.bv.table = NULL;
		break;

	case ENTITY_OBJ_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = NULL;
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
		arg->d.str = arg->d.room ? arg->d.room->name : SOMEWHERE;
		break;
	case ENTITY_ROOM_MOBILES:
		arg->type = ENT_OLLIST_MOB;
		arg->d.list.ptr.mob = room ? &room->people : NULL;
		arg->d.list.owner = room;
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = room ? &room->contents : NULL;
		arg->d.list.owner = room;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = room ? &room->tokens : NULL;
		arg->d.list.owner = room;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = arg->d.room ? arg->d.room->area : NULL;
		break;
	case ENTITY_ROOM_TARGET:
		arg->type = ENT_MOBILE;
		arg->d.mob = (arg->d.room && arg->d.room->progs) ? arg->d.room->progs->target : NULL;
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
		arg->d.door.r = arg->d.room;
		arg->d.door.door = *str - ENTITY_ROOM_NORTH + DIR_NORTH;
		break;
	case ENTITY_ROOM_ENVIRON:
		arg->type = ENT_ROOM;
		arg->d.room = arg->d.room ? get_environment(arg->d.room) : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_ROOM:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.room && arg->d.room->environ_type == ENVIRON_ROOM) ? arg->d.room->environ.room : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_MOB:
		arg->type = ENT_MOBILE;
		arg->d.mob = (arg->d.room && arg->d.room->environ_type == ENVIRON_MOBILE) ? arg->d.room->environ.mob : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_OBJ:
		arg->type = ENT_OBJECT;
		arg->d.obj = (arg->d.room && arg->d.room->environ_type == ENVIRON_OBJECT) ? arg->d.room->environ.obj : NULL;
		break;

	case ENTITY_ROOM_ENVIRON_TOKEN:
		arg->type = ENT_OBJECT;
		arg->d.token = (arg->d.room && arg->d.room->environ_type == ENVIRON_TOKEN) ? arg->d.room->environ.token : NULL;
		break;

	case ENTITY_ROOM_EXTRADESC:
		arg->type = ENT_EXTRADESC;
		arg->d.ed = arg->d.room?arg->d.room->extra_descr:NULL;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.room ? arg->d.room->description : "";
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = arg->d.room ? arg->d.room->lclonerooms : NULL;
		break;

	case ESCAPE_VARIABLE:
		str = expand_escape_variable(info,arg->d.room?arg->d.room->progs->vars:NULL,str+1,arg);
		if(!str) return NULL;
		break;
	case ENTITY_ROOM_VARIABLES:
		arg->type = ENT_ILLIST_VARIABLE;
		arg->d.variables = (arg->d.room && arg->d.room->progs) ? &arg->d.room->progs->vars : NULL;
	default: return NULL;
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
				if(ex->exit_info & EX_LOCKED) i++;
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
	switch(*str) {
	case ENTITY_AREA_NAME:
		arg->type = ENT_STRING;
		arg->d.str = arg->d.area ? arg->d.area->name : SOMEWHERE;
		break;
	case ENTITY_AREA_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.area && location_isset(&arg->d.area->recall)) ? location_to_room(&arg->d.area->recall) : NULL;
		break;
	case ENTITY_AREA_POSTOFFICE:
		arg->type = ENT_ROOM;
		arg->d.room = (arg->d.area && arg->d.area->post_office > 0) ? get_room_index(arg->d.area->post_office) : NULL;
		break;
	case ENTITY_AREA_LOWERVNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.area ? arg->d.area->min_vnum : 0;
		break;
	case ENTITY_AREA_UPPERVNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.area ? arg->d.area->max_vnum : 0;
		break;
	case ENTITY_AREA_ROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = arg->d.area ? arg->d.area->room_list : NULL;
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
	case ENTITY_AREA_RECALL:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_AREA_POSTOFFICE:
		arg->type = ENT_ROOM;
		arg->d.room = NULL;
		break;
	case ENTITY_AREA_LOWERVNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	case ENTITY_AREA_UPPERVNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = 0;
		break;
	case ENTITY_AREA_ROOMS:
		arg->type = ENT_BLLIST_ROOM;
		arg->d.blist = NULL;
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
	switch(*str) {
	case ENTITY_SKILL_GSN:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.sn;	// Redundant!
		break;

	case ENTITY_SKILL_SPELL:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL && skill_table[arg->d.sn].spell_fun && skill_table[arg->d.sn].spell_fun != spell_null) ? 1 : 0;
		break;

	case ENTITY_SKILL_NAME:
		arg->type = ENT_STRING;
		strcpy(arg->buf,(arg->d.sn >= 0 && arg->d.sn < MAX_SKILL && skill_table[arg->d.sn].name) ? skill_table[arg->d.sn].name : "");
		arg->d.str = arg->buf;
		break;

	case ENTITY_SKILL_BEATS:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].beats : 0;
		break;

	case ENTITY_SKILL_LEVEL_WARRIOR:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].skill_level[CLASS_WARRIOR] : 0;
		break;

	case ENTITY_SKILL_LEVEL_CLERIC:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].skill_level[CLASS_CLERIC] : 0;
		break;

	case ENTITY_SKILL_LEVEL_MAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].skill_level[CLASS_MAGE] : 0;
		break;

	case ENTITY_SKILL_LEVEL_THIEF:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].skill_level[CLASS_THIEF] : 0;
		break;


	case ENTITY_SKILL_DIFFICULTY_WARRIOR:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].rating[CLASS_WARRIOR] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_CLERIC:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].rating[CLASS_CLERIC] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_MAGE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].rating[CLASS_MAGE] : 0;
		break;

	case ENTITY_SKILL_DIFFICULTY_THIEF:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].rating[CLASS_THIEF] : 0;
		break;

	case ENTITY_SKILL_TARGET:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].target : 0;
		break;

	case ENTITY_SKILL_POSITION:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].minimum_position : POS_DEAD;
		break;

	case ENTITY_SKILL_NOUN:
		arg->type = ENT_STRING;
		strcpy(arg->buf,(arg->d.sn >= 0 && arg->d.sn < MAX_SKILL && skill_table[arg->d.sn].noun_damage) ? skill_table[arg->d.sn].noun_damage: "");
		arg->d.str = arg->buf;
		break;

	case ENTITY_SKILL_WEAROFF:
		arg->type = ENT_STRING;
		strcpy(arg->buf,(arg->d.sn >= 0 && arg->d.sn < MAX_SKILL && skill_table[arg->d.sn].msg_off) ? skill_table[arg->d.sn].msg_off : "");
		arg->d.str = arg->buf;
		break;

	case ENTITY_SKILL_DISPEL:
		arg->type = ENT_STRING;
		strcpy(arg->buf,(arg->d.sn >= 0 && arg->d.sn < MAX_SKILL && skill_table[arg->d.sn].msg_disp) ? skill_table[arg->d.sn].msg_disp : "");
		arg->d.str = arg->buf;
		break;

	case ENTITY_SKILL_MANA:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].min_mana : 0;
		break;

	case ENTITY_SKILL_INK_TYPE1:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[0][0] : 0;
		break;

	case ENTITY_SKILL_INK_TYPE2:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[1][0] : 0;
		break;

	case ENTITY_SKILL_INK_TYPE3:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[2][0] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE1:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[0][1] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE2:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[1][1] : 0;
		break;

	case ENTITY_SKILL_INK_SIZE3:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.sn >= 0 && arg->d.sn < MAX_SKILL) ? skill_table[arg->d.sn].inks[2][1] : 0;
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
		arg->d.sn = arg->d.sk.sn;
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
			if( arg->d.sk.t )
				arg->d.num = token_skill_rating(arg->d.sk.t);
			else
				arg->d.num = get_skill(arg->d.sk.m,arg->d.sk.sn);
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
		arg->d.sn = arg->d.sk.sn;
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
	case ENTITY_CONN_SNOOPER:
		arg->type = ENT_CONN;
		arg->d.conn = (arg->d.conn && (script_security >= MAX_SCRIPT_SECURITY)) ? arg->d.conn->snoop_by : NULL;
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_affect(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	printf("expand_entity_affect() called\n\r");
	switch(*str) {
	case ENTITY_AFFECT_NAME:
		arg->type = ENT_STRING;
		if(arg->d.aff) {
			if(arg->d.aff->custom_name)
				arg->d.str = arg->d.aff->custom_name;
			else
				arg->d.str = skill_table[arg->d.aff->type].name;
		} else
			arg->d.str = &str_empty[0];
		printf("expand_entity_affect(NAME)-> \"%s\"\n\r", arg->d.str);
		break;

	case ENTITY_AFFECT_GROUP:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->group : -1;
		break;

	case ENTITY_AFFECT_SKILL:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->type : 0;
		break;

	case ENTITY_AFFECT_LOCATION:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->location : -1;
		break;

	case ENTITY_AFFECT_MOD:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->modifier : 0;
		break;

	case ENTITY_AFFECT_LEVEL:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->level : 0;
		break;

	case ENTITY_AFFECT_TIMER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.aff ? arg->d.aff->duration : 0;
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
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
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
		arg->d.ed = NULL;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
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
		break;
	case ENTITY_ROOM_OBJECTS:
		arg->type = ENT_OLLIST_OBJ;
		arg->d.list.ptr.obj = NULL;
		arg->d.list.owner = NULL;
		break;
	case ENTITY_ROOM_TOKENS:
		arg->type = ENT_OLLIST_TOK;
		arg->d.list.ptr.tok = NULL;
		arg->d.list.owner = NULL;
		break;
	case ENTITY_ROOM_AREA:
		arg->type = ENT_AREA;
		arg->d.area = NULL;
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
		arg->d.ed = NULL;
		break;

	case ENTITY_ROOM_DESC:
		arg->type = ENT_STRING;
		arg->d.str = &str_empty[0];
		break;

	case ENTITY_ROOM_CLONEROOMS:
		arg->type = ENT_PLLIST_ROOM;
		arg->d.blist = NULL;
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
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		strcpy(arg->buf, p ? p : "");
		arg->d.str = arg->buf;
		arg->type = ENT_STRING;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, 0);

		strcpy(arg->buf, p ? p : "");
		arg->d.str = arg->buf;
		arg->type = ENT_STRING;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			p = (char *)list_nthdata(arg->d.blist, -1);

		strcpy(arg->buf, p ? p : "");
		arg->d.str = arg->buf;
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
			r = (LLIST_ROOM_DATA *)list_nthdata(arg->d.blist, 0);

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
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 0);

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
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 0);

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
			uid = (LLIST_UID_DATA *)list_nthdata(arg->d.blist, 0);

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
			uid = (LLIST_AREA_DATA *)list_nthdata(arg->d.blist, 0);

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

char *expand_entity_blist_wilds(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register LLIST_WILDS_DATA *uid = NULL;
	switch(*str) {
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
			uid = (LLIST_WILDS_DATA *)list_nthdata(arg->d.blist, 0);

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
			e = (LLIST_EXIT_DATA *)list_nthdata(arg->d.blist, 0);

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
	case ENTITY_LIST_SIZE:
		arg->type = ENT_NUMBER;
		arg->d.num = (arg->d.blist && arg->d.blist->valid) ? arg->d.blist->size : 0;
		break;
	case ENTITY_LIST_RANDOM:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, number_range(0,arg->d.blist->size-1));

		if( s && s->mob && (IS_VALID(s->tok) || (s->sn > 0 && s->sn < MAX_SKILL)) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.sn = (s->sn > 0 && s->sn < MAX_SKILL) ? s->sn : 0;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.sn = 0;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;
		break;
	case ENTITY_LIST_FIRST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, 0);

		if( s && s->mob && (IS_VALID(s->tok) || (s->sn > 0 && s->sn < MAX_SKILL)) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.sn = (s->sn > 0 && s->sn < MAX_SKILL) ? s->sn : 0;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.sn = 0;
			arg->d.sk.mid[0] = arg->d.sk.mid[1] = 0;
			arg->d.sk.tid[0] = arg->d.sk.tid[1] = 0;
		}
		arg->type = ENT_SKILLINFO;
		break;
	case ENTITY_LIST_LAST:
		if(arg->d.blist && arg->d.blist->valid && arg->d.blist->size > 0)
			s = (LLIST_SKILL_DATA *)list_nthdata(arg->d.blist, -1);

		if( s && s->mob && (IS_VALID(s->tok) || (s->sn > 0 && s->sn < MAX_SKILL)) ) {
			arg->d.sk.m = s->mob;
			arg->d.sk.t = s->tok;
			arg->d.sk.sn = (s->sn > 0 && s->sn < MAX_SKILL) ? s->sn : 0;
			arg->d.sk.mid[0] = s->mid[0];
			arg->d.sk.mid[1] = s->mid[1];
			arg->d.sk.tid[0] = s->tid[0];
			arg->d.sk.tid[1] = s->tid[1];
		} else {
			arg->d.sk.m = NULL;
			arg->d.sk.t = NULL;
			arg->d.sk.sn = 0;
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
			conn = (DESCRIPTOR_DATA *)list_nthdata(arg->d.blist, 0);

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
			church = (CHURCH_DATA *)list_nthdata(arg->d.blist, 0);

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

char *expand_entity_plist_mob(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register CHAR_DATA *ch = NULL;
	switch(*str) {
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
			ch = (CHAR_DATA *)list_nthdata(arg->d.blist, 0);

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
			obj = (OBJ_DATA *)list_nthdata(arg->d.blist, 0);

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
			room = (ROOM_INDEX_DATA *)list_nthdata(arg->d.blist, 0);

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
			token = (TOKEN_DATA *)list_nthdata(arg->d.blist, 0);

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

char *expand_entity_plist_variable(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	register VARIABLE *variable = NULL;
	switch(*str) {
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
			variable = (VARIABLE *)list_nthdata(arg->d.blist, 0);

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
	const struct music_type* pSong = NULL;

	if( arg->d.song >= 0 && arg->d.song < MAX_SONGS && music_table[arg->d.song].name != NULL )
		pSong = &music_table[arg->d.song];

	switch(*str) {
	case ENTITY_SONG_NUMBER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.song;	// Redundant!
		break;
	case ENTITY_SONG_NAME:
		arg->type = ENT_STRING;
		strcpy(arg->buf,(pSong ? pSong->name : ""));
		arg->d.str = arg->buf;
		break;
	case ENTITY_SONG_SPELL1:
		arg->type = ENT_SKILL;
		arg->d.sn = (pSong && pSong->spell1) ? skill_lookup(pSong->spell1) : -1;
		break;
	case ENTITY_SONG_SPELL2:
		arg->type = ENT_SKILL;
		arg->d.sn = (pSong && pSong->spell2) ? skill_lookup(pSong->spell2) : -1;
		break;
	case ENTITY_SONG_SPELL3:
		arg->type = ENT_SKILL;
		arg->d.sn = (pSong && pSong->spell3) ? skill_lookup(pSong->spell3) : -1;
		break;
	case ENTITY_SONG_TARGET:
		arg->type = ENT_NUMBER;
		arg->d.num = pSong ? pSong->target : -1;
		break;
	case ENTITY_SONG_BEATS:
		arg->type = ENT_NUMBER;
		arg->d.num = pSong ? pSong->beats : -1;
		break;
	case ENTITY_SONG_MANA:
		arg->type = ENT_NUMBER;
		arg->d.num = pSong ? pSong->mana : -1;
		break;
	case ENTITY_SONG_LEVEL:
		arg->type = ENT_NUMBER;
		arg->d.num = pSong ? pSong->level : -1;
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
	info = arg->d.info;

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


char *expand_entity_objindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	info = arg->d.info;

	switch(*str) {
	case ENTITY_OBJINDEX_VNUM:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->vnum: 0;
		break;
	case ENTITY_OBJINDEX_LOADED:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->count : 0;
		break;
	case ENTITY_OBJINDEX_INROOMS:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->inrooms : 0;
		break;
	case ENTITY_OBJINDEX_INMAIL:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->inmail : 0;
		break;
	case ENTITY_OBJINDEX_CARRIED:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->carried : 0;
		break;
	case ENTITY_OBJINDEX_LOCKERED:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->lockered : 0;
		break;
	case ENTITY_OBJINDEX_INCONTAINER:
		arg->type = ENT_NUMBER;
		arg->d.num = arg->d.objindex ? arg->d.objindex->incontainer : 0;
		break;

	default: return NULL;
	}

	return str+1;
}


char *expand_entity_extradesc(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
//	EXTRA_DESCR_DATA *ed;
	char buf[MSL];
	switch(*str) {
	case ESCAPE_VARIABLE:
		if(!arg->d.ed || !info->var) return NULL;

/*
		{
			char store[MSL];
			char msg[MSL*2];
			expand_escape2print(str,store);
			sprintf(msg,"expand_entity_extradesc->str = \"%s\"",store);
			wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
*/

		wiznet_variables = TRUE;
		str = expand_name(info,(info?*(info->var):NULL),str+1,buf);
		wiznet_variables = FALSE;
		if(!str) return NULL;

/*
		{
			char msg[MSL*2];
			sprintf(msg,"expand_entity_extradesc->\"%s\"",buf);
			wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
		}
*/

		arg->type = ENT_STRING;
		arg->d.str = get_extra_descr(buf, arg->d.ed);
		if (!arg->d.str) arg->d.str = str_dup("");
		break;
	default: return NULL;
	}

	return str+1;
}

char *expand_entity_bitvector(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char buf[MSL];
	switch(*str) {
	case ESCAPE_VARIABLE:
		arg->type = ENT_BOOLEAN;

		if(arg->d.bv.table && arg->d.bv.value)
		{
			int bit;
			str = expand_name(info,(info?*(info->var):NULL),str+1,buf);
			if(!str) {
				arg->d.boolean = FALSE;
				return NULL;
			}


			bit = flag_lookup(buf, arg->d.bv.table);

			arg->d.boolean = IS_SET(arg->d.bv.value, bit) && TRUE;
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


char *expand_argument_entity(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char *next;
	arg->type = ENT_NONE;
	arg->d.num = 0;

	while(str && *str && *str != ESCAPE_END) {
		switch(arg->type) {
		case ENT_PRIMARY:	next = expand_entity_primary(info,str,arg); break;
		case ENT_NUMBER:	next = expand_entity_number(info,str,arg); break;
		case ENT_STRING:	next = expand_entity_string(info,str,arg); break;
		case ENT_MOBILE:	next = expand_entity_mobile(info,str,arg); break;
		case ENT_OBJECT:	next = expand_entity_object(info,str,arg); break;
		case ENT_ROOM:		next = expand_entity_room(info,str,arg); break;
		case ENT_EXIT:		next = expand_entity_exit(info,str,arg); break;
		case ENT_TOKEN:		next = expand_entity_token(info,str,arg); break;
		case ENT_AREA:		next = expand_entity_area(info,str,arg); break;
		case ENT_OLLIST_MOB:	next = expand_entity_list_mob(info,str,arg); break;
		case ENT_OLLIST_OBJ:	next = expand_entity_list_obj(info,str,arg); break;
		case ENT_OLLIST_TOK:	next = expand_entity_list_token(info,str,arg); break;
		case ENT_OLLIST_AFF:	next = expand_entity_list_affect(info,str,arg); break;
		case ENT_SKILL:		next = expand_entity_skill(info,str,arg); break;
		case ENT_SKILLINFO:	next = expand_entity_skillinfo(info,str,arg); break;
		case ENT_CONN:		next = expand_entity_conn(info,str,arg); break;
		case ENT_WILDS:		next = expand_entity_wilds(info,str,arg); break;
		case ENT_CHURCH:	next = expand_entity_church(info,str,arg); break;
		case ENT_EXTRADESC:	next = expand_entity_extradesc(info,str,arg); break;
		case ENT_AFFECT:	next = expand_entity_affect(info,str,arg); break;
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
		case ENT_BLLIST_WILDS:	next = expand_entity_blist_wilds(info,str,arg); break;

		case ENT_PLLIST_STR:		next = expand_entity_plist_str(info,str,arg); break;
		case ENT_PLLIST_CONN:	next = expand_entity_plist_conn(info,str,arg); break;
		case ENT_PLLIST_ROOM:	next = expand_entity_plist_room(info,str,arg); break;
		case ENT_PLLIST_MOB:		next = expand_entity_plist_mob(info,str,arg); break;
		case ENT_PLLIST_OBJ:		next = expand_entity_plist_obj(info,str,arg); break;
		case ENT_PLLIST_TOK:		next = expand_entity_plist_token(info,str,arg); break;
		case ENT_PLLIST_CHURCH:	next = expand_entity_plist_church(info,str,arg); break;

		case ENT_MOBILE_ID:		next = expand_entity_mobile_id(info,str,arg); break;
		case ENT_OBJECT_ID:		next = expand_entity_object_id(info,str,arg); break;
		case ENT_TOKEN_ID:		next = expand_entity_token_id(info,str,arg); break;
		case ENT_AREA_ID:		next = expand_entity_area_id(info,str,arg); break;
		case ENT_SKILLINFO_ID:	next = expand_entity_skillinfo_id(info,str,arg); break;
		case ENT_WILDS_ID:		next = expand_entity_wilds_id(info,str,arg); break;
		case ENT_CHURCH_ID:		next = expand_entity_church_id(info,str,arg); break;
		case ENT_GAME:			next = expand_entity_game(info,str,arg); break;
		case ENT_PERSIST:		next = expand_entity_persist(info,str,arg); break;
		case ENT_PRIOR:			next = expand_entity_prior(info,str,arg); break;
		case ENT_VARIABLE:		next = expand_entity_variable(info,str,arg); break;
		case ENT_GROUP:			next = expand_entity_group(info,str,arg); break;
		case ENT_DICE:			next = expand_entity_dice(info,str,arg); break;
		case ENT_OBJINDEX:		next = expand_entity_objindex(info,str,arg); break;
		case ENT_BITVECTOR:		next = expand_entity_bitvector(info,str,arg); break;
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
			arg->type = ENT_NONE;
			arg->d.num = 0;
		}
	}
	return str && *str ? str+1 : str;
}

char *expand_string_entity(SCRIPT_VARINFO *info,char *str,char **store)
{
	SCRIPT_PARAM arg;

	str = expand_argument_entity(info,str,&arg);
	if(!str || arg.type == ENT_NONE) return NULL;

	switch(arg.type) {
	default:		strcpy(*store," {D<{x@{W@{x@{D>{x "); *store += 19;	break;	// Don't display anything
	case ENT_NUMBER:	*store += sprintf(*store,"%d", arg.d.num); break;
	case ENT_STRING:	*store += sprintf(*store,"%s", arg.d.str ? arg.d.str : "(null)"); break;
	case ENT_MOBILE:	*store += sprintf(*store,"%s", arg.d.mob ? (IS_NPC(arg.d.mob) ? arg.d.mob->short_descr : arg.d.mob->name) : SOMEONE); break;
	case ENT_OBJECT:	*store += sprintf(*store,"%s", arg.d.obj ? arg.d.obj->short_descr : SOMETHING); break;
	case ENT_ROOM:		*store += sprintf(*store,"%s", arg.d.room ? arg.d.room->name : SOMEWHERE); break;
	case ENT_EXIT:		*store += sprintf(*store,"%s", dir_name[arg.d.door.door]); break;
	case ENT_TOKEN:		*store += sprintf(*store,"%s", (arg.d.token && arg.d.token->pIndexData) ? arg.d.token->pIndexData->name : SOMETHING); break;
	case ENT_AREA:		*store += sprintf(*store,"%s", arg.d.area ? arg.d.area->name : SOMEWHERE); break;
	case ENT_CONN:		*store += sprintf(*store,"CONN:%s", (arg.d.conn && arg.d.conn->character) ? arg.d.conn->character->name : SOMEONE); break;
	}

	return str;
}

void expand_string_simple_code(SCRIPT_VARINFO *info,unsigned char code,char **store)
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
		strcpy(*store,s);
		*store += strlen(s);
	}
}

char *expand_string_expression(SCRIPT_VARINFO *info,char *str,char **store)
{
	int num, x;

	str = expand_argument_expression(info,str,&num);
//	sprintf(buf,"expand_string_expression: str %08X, %2.2X", str, (str ? (*str&0xFF) : 0));
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	if(!str) return NULL;

	x = sprintf(*store,"%d",num);

//	sprintf(buf,"expand_string_expression: %d -> '%s'", num, *store);
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	DBG3MSG2("num = '%s', str = %02.2X\n", *store, (*str)&0xFF);

	*store += x;

	return str;
}

char *expand_string_variable(SCRIPT_VARINFO *info,char *str,char **store)
{
	pVARIABLE var;
	char buf[MIL];

	str = expand_variable(info,(info ? *(info->var) : NULL),str, &var);
	if(!str) return NULL;

	if(!var) {
		strcpy(*store,"(null-var)");
		*store += 10;
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
				strcpy(*store,"true");
				*store += 4;
			} else {
				strcpy(*store,"false");
				*store += 5;
			}
			break;
		case VAR_INTEGER:
			*store += sprintf(*store,"%d",var->_.i); break;
		case VAR_STRING:
		case VAR_STRING_S:
			if(var->_.s && *var->_.s) *store += sprintf(*store,"%s",var->_.s); break;
		case VAR_ROOM:
			*store += sprintf(*store,"%d",var->_.r ? (int)var->_.r->vnum : 0); break;
		case VAR_EXIT:
			*store += sprintf(*store,"%s",dir_name[var->_.door.door]); break;
		case VAR_MOBILE:
			if(var->_.m) {
				one_argument(var->_.m->name,buf);
				*store += sprintf(*store,"%s",buf);
			}
			break;
		case VAR_OBJECT:
			if(var->_.o) {
				one_argument(var->_.o->name,buf);
				*store += sprintf(*store,"%s",buf);
			}
			break;
		case VAR_TOKEN:
			*store += sprintf(*store,"%d",var->_.t ? (int)var->_.t->pIndexData->vnum : 0); break;
		case VAR_CONNECTION:
			*store += sprintf(*store,"%s",((var->_.conn && var->_.conn->character) ? var->_.conn->character->name : SOMEONE)); break;

		default:
			strcpy(*store,"(null)");
			*store += 6;
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

bool expand_string(SCRIPT_VARINFO *info,char *str,char *store)
{
	char *start = store;
	str = skip_whitespace(str);
	while(*str) {
		if(*str == ESCAPE_ENTITY)
			str = expand_string_entity(info,str+1,&store);
		else if(*str == ESCAPE_EXPRESSION)
			str = expand_string_expression(info,str+1,&store);
		else if(*str == ESCAPE_VARIABLE)
			str = expand_string_variable(info,str+1,&store);
		else if((unsigned char)*str >= ESCAPE_UA && (unsigned char)*str <= ESCAPE_LZ) {
			expand_string_simple_code(info,*str,&store);
			++str;
		} else
			*store++ = *str++;

		if(!str) {
			*store = 0;
			expand_string_dump(start);
			DBG2EXITVALUE2(FALSE);
			return FALSE;
		}
	}

	*store = 0;
//	wiznet("EXPAND_STRING:",NULL,NULL,WIZ_TESTING,0,0);
//	wiznet(start,NULL,NULL,WIZ_TESTING,0,0);
	expand_string_dump(start);
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


char *expand_argument(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
	char buf[MIL];

	str = skip_whitespace(str);
	arg->type = ENT_NONE;

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
		} else if(expand_string(info,buf,arg->buf)) {
			arg->type = ENT_STRING;
			arg->d.str = arg->buf;
		}
	}

	return skip_whitespace(str);
}
