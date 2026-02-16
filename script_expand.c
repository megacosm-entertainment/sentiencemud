
/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <time.h>
#include "strings.h"
#include "merc.h"
#include "scripts.h"
#include "recycle.h"
#include "wilds.h"
#include "tables.h"
#include "class_data.h"
#include "song_data.h"
#include "traits.h"

//#define DEBUG_MODULE
#include "debug.h"


extern	LLIST *loaded_instances;
extern	LLIST *loaded_dungeons;
extern	LLIST *loaded_ships;

#define EXPAND(f)		char * f (SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
#define EXPAND_TYPE(e)		EXPAND( expand_entity_##e )
#define EXPAND_STR(f)	char * f (SCRIPT_VARINFO *info,char *str,BUFFER *buffer)

char *expand_variable(SCRIPT_VARINFO *info, pVARIABLE vars,char *str,pVARIABLE *var);
char *expand_string_expression(SCRIPT_VARINFO *info,char *str,BUFFER *store);



// Check the validity of the script parameters
//	Should the ids mismatch, reset the field
bool check_varinfo(SCRIPT_VARINFO *info)
{
    bool ret = true;
    return ret;
}

void expand_escape2print(char *str, BUFFER *buffer)
{
    char hex[10];
    while(*str) {
        if(ISPRINT(*str)) add_buf_char(buffer, *str);
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
    if(stk->t >= MAX_STACK) return false;
    stk->s[stk->t++] = val;
    return true;
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
    if(script_expression_tostack[op] == STK_MAX) return false;
    return push(stk,script_expression_tostack[op]);
}

static bool process_expession_stack(STACK *stk_op,STACK *stk_opr,int op)
{
    int t;

    if(op == CH_MAX) {
        //printf("invalid operator.\n");
        return false;
    }

    // Iterate through the operators that CAN be popped off the stack
    while((t = script_expression_stack_action[op][top(stk_opr,STK_EMPTY)]) == POP &&
        (t = perform_operation(stk_op,stk_opr)) == DONE);

    if(t == DONE) return true;
    else if(t == PUSH) {
        push_operator(stk_opr,op);
        return true;
    } else if(t == DELETE) {
        --stk_opr->t;
        return true;
    }
//	else if(t == ERROR1) printf("unmatched right parentheses.\n");
//	else if(t == ERROR2) printf("unmatched left parentheses.\n");
//	else if(t == ERROR3) printf("division by zero.\n");
//	else if(t == ERROR4) printf("insufficient operators/operands on stack.\n");

    return false;
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
    bool deref = false;
    pVARIABLE v;

    //if(*str == ESCAPE_VARIABLE && wiznet_variables) {
    //	char msg[MSL];
    //	sprintf(msg,"expand_variable() -> *str IS ESCAPE_VARIABLE\n\r");
    //	wiznet(msg,NULL,NULL,WIZ_SCRIPTS,0,0);
    //}

    if(*str == '*') {
        deref = true;
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

char *expand_number(char *str, int *num)
{
    char arg[MIL], *s = str, *p = arg;
    while(ISDIGIT(*s)) *p++ = *s++;
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
    bool expect = false;	// false = number/open, true = operator/close
    char *p = str;
    pVARIABLE vars = info ? *(info->var) : NULL;

    opnd.t = optr.t = 0;

    if(info)
    while(*str && *str != ESCAPE_END) {
        str = skip_whitespace(str);
        if(ISDIGIT(*str)) {	// Constant
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
            expect = true;
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
            expect = true;
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
            expect = true;
        } else {
//			sprintf(buf,"expand_argument_expression: operator = %c, %2.2X", *str, str[1]&0xFF);
//			wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

            switch(*str) {
            case '+': op = expect ? CH_ADD : CH_MAX; expect=false; break;
            case '-': op = expect ? CH_SUB : CH_NEG; expect=false; break;
            case '*': op = expect ? CH_MUL : CH_MAX; expect=false; break;
            case '/': op = expect ? CH_DIV : CH_MAX; expect=false; break;
            case '%': op = expect ? CH_MOD : CH_MAX; expect=false; break;
            case ':': op = expect ? CH_RAND : CH_MAX; expect=false; break;
            case '!': op = expect ? CH_MAX : CH_NOT; expect=false; break;
            case '(': op = expect ? CH_MAX : CH_OPEN; expect=false; break;
            case ')': op = expect ? CH_CLOSE : CH_MAX; expect=true; break;
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
        add_buf(arg->buffer, game_settings.game_name);
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_GAME_PORT:
        arg->type = ENT_NUMBER;
        arg->d.num = game_settings.telnet_port;
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
    
    case ENTITY_GAME_TIME_HUMAN:
        arg->type = ENT_STRING;
        struct tm *local_time = localtime(&current_time);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%a %b %d %X %Z %Y", local_time);
        arg->d.str = strdup(time_str);
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
        
    case ENTITY_GAME_RESERVED_AREA:
        arg->type = ENT_RESERVED_AREA;
        break;
        
    case ENTITY_GAME_RESERVED_TOKEN:
        arg->type = ENT_RESERVED_TOKEN;
        break;
        
    case ENTITY_GAME_RESERVED_RPROG:
        arg->type = ENT_RESERVED_RPROG;
        break;
        
    case ENTITY_GAME_RESERVED_OPROG:
        arg->type = ENT_RESERVED_OPROG;
        break;
        
    case ENTITY_GAME_RESERVED_MPROG:
        arg->type = ENT_RESERVED_MPROG;
        break;
        
    case ENTITY_GAME_RESERVED_TPROG:
        arg->type = ENT_RESERVED_TPROG;
        break;
    
    case ENTITY_GAME_RESERVED_APROG:
        arg->type = ENT_RESERVED_APROG;
        break;
        
    case ENTITY_GAME_SETTINGS:
        arg->type = ENT_GAME_SETTING;
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
    char time_str[100];
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

    case ENTITY_CHURCH_FOUNDER_LOGIN:
        arg->type = ENT_NUMBER;
        arg->d.num = ( arg->d.church ) ? (arg->d.church->founder_last_login) : 0;
        break;

    case ENTITY_CHURCH_FOUNDER_LOGIN_HUMAN:
        arg->type = ENT_STRING;
        struct tm *founder_time = localtime(&arg->d.church->founder_last_login);
        strftime(time_str, sizeof(time_str), "%a %b %d %X %Z %Y", founder_time);
        arg->d.str = (arg->d.mob) ? str_dup(time_str) : (char *)&str_empty[0];
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

char *expand_entity_boolean(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
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
    char *a;

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

char *expand_entity_mobile(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    CHAR_DATA *self = arg->d.mob;
    char *p;
    char time_str[100];
    switch(*str) {
    case ENTITY_MOB_NAME:
        arg->type = ENT_STRING;
        arg->d.str = arg->d.mob ? (char*)arg->d.mob->name : (char*)SOMEONE;
        break;
    case ENTITY_MOB_SHORT:
        arg->type = ENT_STRING;
        p = arg->d.mob ? (char*)((IS_NPC(arg->d.mob) || arg->d.mob->morphed) ? arg->d.mob->short_descr : capitalize(arg->d.mob->name)) : (char*)"no one";
        clear_buf(arg->buffer);
        add_buf(arg->buffer, p);
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_MOB_LONG:
        arg->type = ENT_STRING;
        arg->d.str = arg->d.mob ? (char*)arg->d.mob->long_descr : (char*)&str_empty[0];
        break;
    case ENTITY_MOB_SEX: // Now returns the body_type name string
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_body_type_name(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_HE: // Subjective pronoun (he, she, they, ze)
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_he_she(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_HIM: // Objective pronoun (him, her, them, zir)
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_him_her(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_HIS: // Possessive adjective (his, her, their, zis)
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_his_her(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_HIS_O: // Possessive pronoun (his, hers, theirs, zirs)
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_his_hers(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_HIMSELF: // Reflexive pronoun (himself, herself, themself, zirself)
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (char *)get_himself_herself(self));
        arg->d.str = buf_string(arg->buffer);
        arg->type = ENT_STRING;
        break;
    case ENTITY_MOB_RACE:
        arg->type = ENT_STRING;
        arg->d.str = (arg->d.mob && arg->d.mob->race) ? (char*)arg->d.mob->race->name : "unknown";
        break;
    case ENTITY_MOB_RACEDATA:
        arg->type = ENT_RACE;
        arg->d.race = self ? self->race : NULL;
        break;
    case ENTITY_MOB_ORIGINALRACE:
        arg->type = ENT_STRING;
        arg->d.str = (arg->d.mob && arg->d.mob->orace) ? (char*)arg->d.mob->orace->name : "none";
        break;
    case ENTITY_MOB_ORIGINALRACEDATA:
        arg->type = ENT_RACE;
        arg->d.race = self ? self->orace : NULL;
        break;
    case ENTITY_MOB_CLASS:
        arg->type = ENT_CLASS;
        arg->d.clazz = (self && !IS_NPC(self) && self->pcdata->current_class)
            ? self->pcdata->current_class->clazz : NULL;
        break;
    case ENTITY_MOB_CLASSLEVEL:
        arg->type = ENT_CLASSLEVEL;
        arg->d.classlevel = (self && !IS_NPC(self))
            ? self->pcdata->current_class : NULL;
        break;
    case ENTITY_MOB_ROOM:
        arg->type = ENT_ROOM;
        arg->d.room = arg->d.mob ? arg->d.mob->in_room : NULL;
        break;
    case ENTITY_MOB_HOUSE:
        arg->type = ENT_ROOM;
        if( arg->d.mob )
        {
            if( IS_NPC(arg->d.mob) )
                arg->d.room = arg->d.mob->home_room;
            else if( arg->d.mob->home > 0 ) {
                AREA_DATA *home_area = find_area_by_vnum(arg->d.mob->home, NULL);
                if (!home_area) home_area = get_system_area_fallback();
                arg->d.room = get_room_index(home_area, arg->d.mob->home);
            }
        }
        else
            arg->d.room = NULL;
        break;
    case ENTITY_MOB_CARRYING:
        if (self && is_llist(self->lcarrying)) {
            arg->type = ENT_PLLIST_OBJ;
            arg->d.blist = self ? self->lcarrying : NULL;
        } else {
            arg->type = ENT_OLLIST_OBJ;
            arg->d.list.ptr.obj = NULL;  // Old style linked list no longer used
            arg->d.list.owner = self;
            arg->d.list.owner_type = ENT_UNKNOWN;
        }
        break;
    case ENTITY_MOB_TOKENS:
        arg->type = ENT_OLLIST_TOK;
        arg->d.list.ptr.tok = self ? &self->tokens : NULL;
        arg->d.list.owner = self;
        arg->d.list.owner_type = ENT_UNKNOWN;
        break;
    case ENTITY_MOB_AFFECTS:
        arg->type = ENT_OLLIST_AFF;
        arg->d.list.ptr.aff = self ? &self->affected : NULL;
        arg->d.list.owner = self;
        arg->d.list.owner_type = ENT_UNKNOWN;
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
    case ENTITY_MOB_EQUIPMENT:
        arg->type = ENT_EQUIPMENT;
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
        arg->d.song = arg->d.mob ? arg->d.mob->song : NULL;
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
        arg->d.conn = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->desc : NULL;	// Works for PCs and switched NPCs
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

    case ENTITY_MOB_NUMGROUPED:
        arg->type = ENT_NUMBER;
        arg->d.num = self ? self->num_grouped : 0;
        break;

    case ENTITY_MOB_DAMAGEDICE:
        arg->type = ENT_DICE;
        arg->d.dice = arg->d.mob ? &arg->d.mob->damage : NULL;;
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

    case ENTITY_MOB_INDEX:
        arg->type = ENT_MOBINDEX;
        arg->d.mobindex = (arg->d.mob && IS_NPC(arg->d.mob)) ? arg->d.mob->pIndexData : NULL;
        break;

    case ENTITY_MOB_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mob ? ((IS_NPC(arg->d.mob)) ? arg->d.mob->level : arg->d.mob->tot_level) : 0;
        break;

    case ENTITY_MOB_LASTLOGOFF:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->pcdata->last_logoff : 0;
        break;
    case ENTITY_MOB_LASTLOGIN:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->pcdata->last_login : 0;
        break;
    case ENTITY_MOB_PLAYED:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? arg->d.mob->played + (int) current_time - arg->d.mob->pcdata->last_login : 0;
        break;
    case ENTITY_MOB_SESSIONTIME:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? (int) current_time - arg->d.mob->pcdata->last_login : 0;
        break;
    case ENTITY_MOB_CREATED:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mob ? ((!IS_NPC(arg->d.mob)) ? arg->d.mob->pcdata->creation_date : arg->d.mob->creation_time) : 0;
        break;
    case ENTITY_MOB_LASTLOGOFF_HUMAN:
        arg->type = ENT_STRING;
        if (arg->d.mob && !IS_NPC(arg->d.mob)) {
            struct tm *logoff_time = localtime(&arg->d.mob->pcdata->last_logoff);
            strftime(time_str, sizeof(time_str), "%a %b %d %X %Z %Y", logoff_time);
            arg->d.str = str_dup(time_str);
        } else {
            arg->d.str = (char *)&str_empty[0];
        }
        break;
    case ENTITY_MOB_LASTLOGIN_HUMAN:
        arg->type = ENT_STRING;
        if (arg->d.mob && !IS_NPC(arg->d.mob)) {
            struct tm *login_time = localtime(&arg->d.mob->pcdata->last_login);
            strftime(time_str, sizeof(time_str), "%a %b %d %X %Z %Y", login_time);
            arg->d.str = str_dup(time_str);
        } else {
            arg->d.str = (char *)&str_empty[0];
        }
        break;
    case ENTITY_MOB_CREATED_HUMAN:
        arg->type = ENT_STRING;
        if (arg->d.mob && !IS_NPC(arg->d.mob)) {
            struct tm *creation_time = localtime(&arg->d.mob->pcdata->creation_date);
            strftime(time_str, sizeof(time_str), "%a %b %d %X %Z %Y", creation_time);
            arg->d.str = str_dup(time_str);
        } else {
            arg->d.str = (char *)&str_empty[0];
        }
        break;
    case ENTITY_MOB_LASTLOGOFF_DELTA:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? current_time - arg->d.mob->pcdata->last_logoff : 0;
        break;
    case ENTITY_MOB_LASTLOGIN_DELTA:
        arg->type = ENT_NUMBER;
        arg->d.num = (arg->d.mob && !IS_NPC(arg->d.mob)) ? current_time - arg->d.mob->pcdata->last_login : 0;
        break;
    case ENTITY_MOB_CREATED_DELTA:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mob ? ((!IS_NPC(arg->d.mob)) ? current_time - arg->d.mob->pcdata->creation_date : current_time - arg->d.mob->creation_time) : 0;
        break;
    case ENTITY_MOB_BODY_TYPE_VALUE:
        arg->d.num = self->body_type;
        arg->type = ENT_NUMBER;
        break;
    case ENTITY_MOB_VERB_PREF_VALUE:
        arg->d.num = self->verb_preference;
        arg->type = ENT_NUMBER;
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
        clear_buf(arg->buffer);
        add_buf(arg->buffer, "unknown");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_MOB_RACEDATA:
        arg->type = ENT_RACE;
        arg->d.race = NULL;
        break;
    case ENTITY_MOB_ORIGINALRACE:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, "none");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_MOB_ORIGINALRACEDATA:
        arg->type = ENT_RACE;
        arg->d.race = NULL;
        break;
    case ENTITY_MOB_CLASS:
        arg->type = ENT_CLASS;
        arg->d.clazz = NULL;
        break;
    case ENTITY_MOB_CLASSLEVEL:
        arg->type = ENT_CLASSLEVEL;
        arg->d.classlevel = NULL;
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
    case ENTITY_MOB_EQUIPMENT:
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
        arg->d.group_owner = NULL;
        break;

    case ENTITY_MOB_NUMGROUPED:
        arg->type = ENT_NUMBER;
        arg->d.num = 0;
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

    case ENTITY_MOB_INDEX:
        arg->type = ENT_MOBINDEX;
        arg->d.mobindex = NULL;
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
        arg->d.list.owner_type = ENT_UNKNOWN;
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
        arg->d.list.owner_type = ENT_UNKNOWN;
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
        arg->d.list.owner_type = ENT_UNKNOWN;
        break;

    case ENTITY_OBJ_INDEX:
        arg->type = ENT_OBJINDEX;
        arg->d.objindex = arg->d.obj ? arg->d.obj->pIndexData : NULL;
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
        arg->d.ship = arg->d.obj ? arg->d.obj->ship : NULL;
        break;

    case ENTITY_OBJ_VARIABLES:
        arg->type = ENT_ILLIST_VARIABLE;
        arg->d.variables = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL;
        break;

    case ENTITY_OBJ_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.obj ? arg->d.obj->level : 0;
        break;

    // Typed data sub-entity accessors
    case ENTITY_OBJ_WEAPON_DATA:
        arg->type = ENT_OBJ_WEAPON;
        arg->d.obj_weapon = self ? WEAPON(self) : NULL;
        break;
    case ENTITY_OBJ_ARMOR_DATA:
        arg->type = ENT_OBJ_ARMOR;
        arg->d.obj_armor = self ? ARMOR(self) : NULL;
        break;
    case ENTITY_OBJ_CONTAINER_DATA:
        arg->type = ENT_OBJ_CONTAINER;
        arg->d.obj_container = self ? CONTAINER(self) : NULL;
        break;
    case ENTITY_OBJ_FLUID_CON_DATA:
        arg->type = ENT_OBJ_FLUID_CON;
        arg->d.obj_fluid_con = self ? FLUID_CON(self) : NULL;
        break;
    case ENTITY_OBJ_FOOD_DATA:
        arg->type = ENT_OBJ_FOOD;
        arg->d.obj_food = self ? FOOD(self) : NULL;
        break;
    case ENTITY_OBJ_FURNITURE_DATA:
        arg->type = ENT_OBJ_FURNITURE;
        arg->d.obj_furniture = self ? FURNITURE(self) : NULL;
        break;
    case ENTITY_OBJ_PORTAL_DATA:
        arg->type = ENT_OBJ_PORTAL;
        arg->d.obj_portal = self ? PORTAL(self) : NULL;
        break;
    case ENTITY_OBJ_LIGHT_DATA:
        arg->type = ENT_OBJ_LIGHT;
        arg->d.obj_light = self ? LIGHT(self) : NULL;
        break;
    case ENTITY_OBJ_MONEY_DATA:
        arg->type = ENT_OBJ_MONEY;
        arg->d.obj_money = self ? MONEY(self) : NULL;
        break;
    case ENTITY_OBJ_WAND_DATA:
        arg->type = ENT_OBJ_WAND;
        arg->d.obj_wand = self ? WAND(self) : NULL;
        break;
    case ENTITY_OBJ_CORPSE_DATA:
        arg->type = ENT_OBJ_CORPSE;
        arg->d.obj_corpse = self ? CORPSE(self) : NULL;
        break;
    case ENTITY_OBJ_INSTRUMENT_DATA:
        arg->type = ENT_OBJ_INSTRUMENT;
        arg->d.obj_instrument = self ? INSTRUMENT(self) : NULL;
        break;
    case ENTITY_OBJ_SEED_DATA:
        arg->type = ENT_OBJ_SEED;
        arg->d.obj_seed = self ? SEED(self) : NULL;
        break;
    case ENTITY_OBJ_CART_DATA:
        arg->type = ENT_OBJ_CART;
        arg->d.obj_cart = self ? CART(self) : NULL;
        break;
    case ENTITY_OBJ_ITEM_SHIP_DATA:
        arg->type = ENT_OBJ_ITEM_SHIP;
        arg->d.obj_item_ship = self ? SHIP_TYPE(self) : NULL;
        break;
    case ENTITY_OBJ_SEXTANT_DATA:
        arg->type = ENT_OBJ_SEXTANT;
        arg->d.obj_sextant = self ? SEXTANT(self) : NULL;
        break;
    case ENTITY_OBJ_WEAPON_CON_DATA:
        arg->type = ENT_OBJ_WEAPON_CON;
        arg->d.obj_weapon_con = self ? WEAPON_CON(self) : NULL;
        break;
    case ENTITY_OBJ_BOOK_DATA:
        arg->type = ENT_OBJ_BOOK;
        arg->d.obj_book = self ? BOOK(self) : NULL;
        break;
    case ENTITY_OBJ_HERB_DATA:
        arg->type = ENT_OBJ_HERB;
        arg->d.obj_herb = self ? HERB(self) : NULL;
        break;
    case ENTITY_OBJ_MIST_DATA:
        arg->type = ENT_OBJ_MIST;
        arg->d.obj_mist = self ? MIST(self) : NULL;
        break;
    case ENTITY_OBJ_TRADE_DATA:
        arg->type = ENT_OBJ_TRADE;
        arg->d.obj_trade = self ? TRADE(self) : NULL;
        break;
    case ENTITY_OBJ_TATTOO_DATA:
        arg->type = ENT_OBJ_TATTOO;
        arg->d.obj_tattoo = self ? TATTOO(self) : NULL;
        break;
    case ENTITY_OBJ_INK_DATA:
        arg->type = ENT_OBJ_INK;
        arg->d.obj_ink = self ? INK(self) : NULL;
        break;
    case ENTITY_OBJ_TELESCOPE_DATA:
        arg->type = ENT_OBJ_TELESCOPE;
        arg->d.obj_telescope = self ? TELESCOPE(self) : NULL;
        break;
    case ENTITY_OBJ_COMPASS_DATA:
        arg->type = ENT_OBJ_COMPASS;
        arg->d.obj_compass = self ? COMPASS(self) : NULL;
        break;
    case ENTITY_OBJ_BODY_PART_DATA:
        arg->type = ENT_OBJ_BODY_PART;
        arg->d.obj_body_part = self ? BODY_PART(self) : NULL;
        break;
    case ENTITY_OBJ_SCROLL_DATA:
        arg->type = ENT_OBJ_SCROLL;
        arg->d.obj_scroll = self ? SCROLL(self) : NULL;
        break;
    case ENTITY_OBJ_TOOL_DATA:
        arg->type = ENT_OBJ_TOOL;
        arg->d.obj_tool = self ? TOOL(self) : NULL;
        break;
    case ENTITY_OBJ_JEWELRY_DATA:
        arg->type = ENT_OBJ_JEWELRY;
        arg->d.obj_jewelry = self ? JEWELRY(self) : NULL;
        break;
    case ENTITY_OBJ_MAP_DATA:
        arg->type = ENT_OBJ_MAP;
        arg->d.obj_map = self ? MAP(self) : NULL;
        break;
    case ENTITY_OBJ_PAGE_DATA:
        arg->type = ENT_OBJ_PAGE;
        arg->d.obj_page = self ? PAGE(self) : NULL;
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
        

    case ENTITY_OBJ_VARIABLES:
        arg->type = ENT_ILLIST_VARIABLE;
        arg->d.variables = NULL;
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
        arg->d.str = arg->d.room ? arg->d.room->name : SOMEWHERE;
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
        arg->d.list.ptr.ed = room ? &room->extra_descr : NULL;
        arg->d.list.owner = room;
        arg->d.list.owner_type = ENT_ROOM;
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
        break;
    case ENTITY_ROOM_SECTION:
        arg->type = ENT_SECTION;
        arg->d.section = (arg->d.room && IS_VALID(arg->d.room->instance_section)) ? arg->d.room->instance_section : NULL;
        break;

    case ENTITY_ROOM_INSTANCE:
        arg->type = ENT_INSTANCE;
        arg->d.section = (arg->d.room && IS_VALID(arg->d.room->instance_section)) ? arg->d.room->instance_section : NULL;
        arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
        break;

    case ENTITY_ROOM_DUNGEON:
        arg->type = ENT_DUNGEON;
        arg->d.section = (arg->d.room && IS_VALID(arg->d.room->instance_section)) ? arg->d.room->instance_section : NULL;
        arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
        arg->d.dungeon = (arg->d.instance && IS_VALID(arg->d.instance->dungeon)) ? arg->d.instance->dungeon : NULL;
        break;

    case ENTITY_ROOM_SHIP:
        arg->type = ENT_SHIP;
        arg->d.section = (arg->d.room && IS_VALID(arg->d.room->instance_section)) ? arg->d.room->instance_section : NULL;
        arg->d.instance = (arg->d.section && IS_VALID(arg->d.section->instance)) ? arg->d.section->instance : NULL;
        arg->d.ship = (arg->d.instance && IS_VALID(arg->d.instance->ship)) ? arg->d.instance->ship : NULL;
        break;

    case ENTITY_ROOM_FLAGS:
        arg->type = ENT_BITMATRIX;
        arg->d.bm.values = room ? room->room_flag : NULL;
        arg->d.bm.bank = room_flagbank;
        break;

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
    if (arg->d.area && arg->d.area->post_office_wnum.vnum > 0) {
        AREA_DATA *post_area = find_area_by_vnum(arg->d.area->post_office_wnum.vnum, NULL);
        if (!post_area) post_area = get_system_area_fallback();
        arg->d.room = get_room_index(post_area, arg->d.area->post_office_wnum.vnum);
    } else {
        arg->d.room = NULL;
    }
    case ENTITY_AREA_POSTOFFICE:
        arg->type = ENT_ROOM;
        arg->d.room = (arg->d.area && arg->d.area->post_office_wnum.vnum > 0) ? get_room_index(arg->d.area, arg->d.area->post_office_wnum.vnum) : NULL;
        break;
    case ENTITY_AREA_LOWERVNUM:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.area ? arg->d.area->min_vnum : 0;
        break;
    case ENTITY_AREA_UPPERVNUM:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.area ? arg->d.area->max_vnum : 0;
        break;
    case ENTITY_AREA_MINLEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.area ? arg->d.area->min_level : 0;
        break;
    case ENTITY_AREA_MAXLEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.area ? arg->d.area->max_level : 0;
        break;
    case ENTITY_AREA_ROOMS:
        arg->type = ENT_PLLIST_ROOM;
        arg->d.blist = arg->d.area ? arg->d.area->room_list : NULL;
        break;
    case ESCAPE_VARIABLE:
        str = expand_escape_variable(info,arg->d.area?arg->d.area->progs->vars:NULL,str+1,arg);
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
    case ENTITY_AREA_MINLEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = 0;
        break;
    case ENTITY_AREA_MAXLEVEL:
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
                for(mob = *arg->d.list.ptr.mob; --count > 0; mob = mob->next_in_room);
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
                for(obj = *arg->d.list.ptr.obj; --count > 0; obj = obj->next_content);
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
                for(token = *arg->d.list.ptr.tok; --count > 0; token = token->next);
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
                for(affect = *arg->d.list.ptr.aff; --count > 0; affect = affect->next);
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
    SKILL_DATA *skill = (arg->d.sn >= 0) ? skill_from_sn(arg->d.sn) : NULL;

    switch(*str) {
    case ENTITY_SKILL_GSN:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->uid : -1;
        break;

    case ENTITY_SKILL_SPELL:
        arg->type = ENT_NUMBER;
        arg->d.num = (skill && skill->isspell) ? 1 : 0;
        break;

    case ENTITY_SKILL_ISSPELL:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = skill ? skill->isspell : false;
        break;

    case ENTITY_SKILL_NAME:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->name) ? skill->name : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_DISPLAY:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->display) ? skill->display : ((skill && skill->name) ? skill->name : ""));
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_DESCRIPTION:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->description) ? skill->description : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_SUMMARY:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->summary) ? skill->summary : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_COMMENTS:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->comments) ? skill->comments : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_UID:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->uid : -1;
        break;

    case ENTITY_SKILL_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = skill ? skill->flags : 0;
        arg->d.bv.table = skill_flags;
        break;

    case ENTITY_SKILL_DIFFICULTY:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->difficulty : 0;
        break;

    case ENTITY_SKILL_BEATS:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->beats : 0;
        break;

    case ENTITY_SKILL_LEVEL_WARRIOR:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->skill_level[CLASS_WARRIOR] : 0;
        break;

    case ENTITY_SKILL_LEVEL_CLERIC:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->skill_level[CLASS_CLERIC] : 0;
        break;

    case ENTITY_SKILL_LEVEL_MAGE:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->skill_level[CLASS_MAGE] : 0;
        break;

    case ENTITY_SKILL_LEVEL_THIEF:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->skill_level[CLASS_THIEF] : 0;
        break;

    case ENTITY_SKILL_DIFFICULTY_WARRIOR:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->rating[CLASS_WARRIOR] : 0;
        break;

    case ENTITY_SKILL_DIFFICULTY_CLERIC:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->rating[CLASS_CLERIC] : 0;
        break;

    case ENTITY_SKILL_DIFFICULTY_MAGE:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->rating[CLASS_MAGE] : 0;
        break;

    case ENTITY_SKILL_DIFFICULTY_THIEF:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->rating[CLASS_THIEF] : 0;
        break;

    case ENTITY_SKILL_TARGET:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->target : 0;
        break;

    case ENTITY_SKILL_POSITION:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->minimum_position : POS_DEAD;
        break;

    case ENTITY_SKILL_NOUN:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->noun_damage) ? skill->noun_damage : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_WEAROFF:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->msg_off) ? skill->msg_off : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_OBJECT:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->msg_obj) ? skill->msg_obj : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_DISPEL:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (skill && skill->msg_disp) ? skill->msg_disp : "");
        arg->d.str = buf_string(arg->buffer);
        break;

    case ENTITY_SKILL_MANA:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->min_mana : 0;
        break;

    case ENTITY_SKILL_RACE:
        arg->type = ENT_RACE;
        arg->d.race = skill ? skill->race : NULL;
        break;

    case ENTITY_SKILL_INK_TYPE1:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[0][0] : 0;
        break;

    case ENTITY_SKILL_INK_TYPE2:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[1][0] : 0;
        break;

    case ENTITY_SKILL_INK_TYPE3:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[2][0] : 0;
        break;

    case ENTITY_SKILL_INK_SIZE1:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[0][1] : 0;
        break;

    case ENTITY_SKILL_INK_SIZE2:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[1][1] : 0;
        break;

    case ENTITY_SKILL_INK_SIZE3:
        arg->type = ENT_NUMBER;
        arg->d.num = skill ? skill->inks[2][1] : 0;
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
    case ENTITY_CONN_CLIENT:
        arg->type = ENT_STRING;
        arg->d.str = (arg->d.conn && (script_security >= MAX_SCRIPT_SECURITY)) ? arg->d.conn->pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString : "Unknown";
        break;
    case ENTITY_CONN_SECURE:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = (arg->d.conn && (script_security >= MAX_SCRIPT_SECURITY) && arg->d.conn->ssl) ? true : false;
        break;
    default: return NULL;
    }

    return str+1;
}

char *expand_entity_affect(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    //printf("expand_entity_affect() called\n\r");
    switch(*str) {
    case ENTITY_AFFECT_NAME:
        arg->type = ENT_STRING;
        if(arg->d.aff) {
            if(arg->d.aff->custom_name)
                arg->d.str = arg->d.aff->custom_name;
            else if (arg->d.aff->type >= 0)
                arg->d.str = skill_table[arg->d.aff->type].name;
            else
                arg->d.str = &str_empty[0];
        } else
            arg->d.str = &str_empty[0];
        //printf("expand_entity_affect(NAME)-> \"%s\"\n\r", arg->d.str);
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
            p = (char *)list_nthdata(arg->d.blist, 0);

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
        arg->d.num = arg->d.group_owner ? list_size(arg->d.group_owner->lgroup) + 1 : 0;

        break;

    default: return NULL;
    }

    return str+1;
}

char *expand_entity_song(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    SONG_DATA *pSong = arg->d.song;

    switch(*str) {
    case ENTITY_SONG_NUMBER:
        arg->type = ENT_NUMBER;
        arg->d.num = pSong ? pSong->uid : -1;
        break;
    case ENTITY_SONG_NAME:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, (pSong ? pSong->name : ""));
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_SONG_UID:
        arg->type = ENT_NUMBER;
        arg->d.num = pSong ? pSong->uid : -1;
        break;
    case ENTITY_SONG_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = pSong ? pSong->flags : 0;
        arg->d.bv.table = song_flags;
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


char *expand_entity_race(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    RACE_DATA *race = arg->d.race;

    switch(*str) {
    case ENTITY_RACE_NAME:
        arg->type = ENT_STRING;
        arg->d.str = race ? race->name : "";
        break;
    case ENTITY_RACE_DESCRIPTION:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, race ? (race->description ? race->description : "") : "");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_RACE_COMMENTS:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, race ? (race->comments ? race->comments : "") : "");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_RACE_UID:
        arg->type = ENT_NUMBER;
        arg->d.num = race ? race->uid : -1;
        break;
    case ENTITY_RACE_ID:
        arg->type = ENT_STRING;
        arg->d.str = race ? (race->id ? race->id : "") : "";
        break;
    case ENTITY_RACE_PLAYABLE:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = race ? race->playable : false;
        break;
    case ENTITY_RACE_STARTING:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = race ? race->starting : false;
        break;
    case ENTITY_RACE_ACT:
        arg->type = ENT_BITMATRIX;
        arg->d.bm.values = race ? race->act : NULL;
        arg->d.bm.bank = race ? act_flagbank : NULL;
        break;
    case ENTITY_RACE_AFFECTS:
        arg->type = ENT_BITMATRIX;
        arg->d.bm.values = race ? race->aff : NULL;
        arg->d.bm.bank = race ? affect_flagbank : NULL;
        break;
    case ENTITY_RACE_OFFENSE:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->off : 0;
        arg->d.bv.table = off_flags;
        break;
    case ENTITY_RACE_IMMUNE:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->imm : 0;
        arg->d.bv.table = imm_flags;
        break;
    case ENTITY_RACE_RESIST:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->res : 0;
        arg->d.bv.table = res_flags;
        break;
    case ENTITY_RACE_VULN:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->vuln : 0;
        arg->d.bv.table = vuln_flags;
        break;
    case ENTITY_RACE_FORM:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->form : 0;
        arg->d.bv.table = form_flags;
        break;
    case ENTITY_RACE_PARTS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = race ? race->parts : 0;
        arg->d.bv.table = part_flags;
        break;
    case ENTITY_RACE_WHO:
        arg->type = ENT_STRING;
        arg->d.str = race ? (race->who_name ? race->who_name : "") : "";
        break;
    case ENTITY_RACE_SIZE_MIN:
        arg->type = ENT_NUMBER;
        arg->d.num = race ? race->min_size : 0;
        break;
    case ENTITY_RACE_SIZE_MAX:
        arg->type = ENT_NUMBER;
        arg->d.num = race ? race->max_size : 0;
        break;
    case ENTITY_RACE_ALIGNMENT:
        arg->type = ENT_NUMBER;
        arg->d.num = race ? race->default_alignment : 0;
        break;

    default: return NULL;
    }

    return str+1;
}


char *expand_entity_class(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    CLASS_DATA *clazz = arg->d.clazz;

    switch(*str) {
    case ENTITY_CLASS_NAME:
        arg->type = ENT_STRING;
        arg->d.str = clazz ? clazz->name : "";
        break;
    case ENTITY_CLASS_DESCRIPTION:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, clazz ? (clazz->description ? clazz->description : "") : "");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_CLASS_COMMENTS:
        arg->type = ENT_STRING;
        clear_buf(arg->buffer);
        add_buf(arg->buffer, clazz ? (clazz->comments ? clazz->comments : "") : "");
        arg->d.str = buf_string(arg->buffer);
        break;
    case ENTITY_CLASS_UID:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->uid : -1;
        break;
    case ENTITY_CLASS_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->type : -1;
        break;
    case ENTITY_CLASS_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = clazz ? clazz->flags : 0;
        arg->d.bv.table = class_flags;
        break;
    case ENTITY_CLASS_PRIMARY_STAT:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->primary_stat : -1;
        break;
    case ENTITY_CLASS_MAX_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->max_level : 0;
        break;
    case ENTITY_CLASS_HP_MIN:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->hp_min : 0;
        break;
    case ENTITY_CLASS_HP_MAX:
        arg->type = ENT_NUMBER;
        arg->d.num = clazz ? clazz->hp_max : 0;
        break;
    case ENTITY_CLASS_GAINS_MANA:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = clazz ? clazz->gains_mana : false;
        break;

    default: return NULL;
    }

    return str+1;
}


char *expand_entity_classlevel(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    CLASS_LEVEL *level = arg->d.classlevel;

    switch(*str) {
    case ENTITY_CLASSLEVEL_CLASS:
        arg->type = ENT_CLASS;
        arg->d.clazz = level ? level->clazz : NULL;
        break;
    case ENTITY_CLASSLEVEL_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = level ? level->level : 0;
        break;
    case ENTITY_CLASSLEVEL_XP:
        arg->type = ENT_NUMBER;
        arg->d.num = level ? (int)level->xp : 0;
        break;
    case ENTITY_CLASSLEVEL_TITLE:
        arg->type = ENT_STRING;
        arg->d.str = level ? (level->active_title ? level->active_title : "") : "";
        break;

    default: return NULL;
    }

    return str+1;
}


char *expand_entity_skillentry(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    SKILL_ENTRY *entry = arg->d.entry;

    switch(*str) {
    case ENTITY_SKILLENTRY_SKILL:
        arg->type = ENT_SKILL;
        arg->d.sn = entry ? entry->sn : -1;
        break;
    case ENTITY_SKILLENTRY_SONG:
        arg->type = ENT_SONG;
        arg->d.song = entry ? entry->song : NULL;
        break;
    case ENTITY_SKILLENTRY_RATING:
        arg->type = ENT_NUMBER;
        arg->d.num = entry ? entry->rating : 0;
        break;
    case ENTITY_SKILLENTRY_MOD:
        arg->type = ENT_NUMBER;
        arg->d.num = entry ? entry->mod_rating : 0;
        break;
    case ENTITY_SKILLENTRY_ISSPELL:
        arg->type = ENT_BOOLEAN;
        arg->d.boolean = entry ? entry->isspell : false;
        break;
    case ENTITY_SKILLENTRY_SOURCE:
        arg->type = ENT_NUMBER;
        arg->d.num = entry ? entry->source : 0;
        break;
    case ENTITY_SKILLENTRY_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = entry ? entry->flags : 0;
        arg->d.bv.table = skill_flags;
        break;
    case ENTITY_SKILLENTRY_TOKEN:
        arg->type = ENT_TOKEN;
        arg->d.token = entry ? entry->token : NULL;
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


char *expand_entity_mobindex(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    info = arg->d.info;

    switch(*str) {
    case ENTITY_MOBINDEX_VNUM:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mobindex ? arg->d.mobindex->vnum: 0;
        break;
    case ENTITY_MOBINDEX_LOADED:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mobindex ? arg->d.mobindex->count : 0;
        break;
    case ENTITY_MOBINDEX_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.mobindex ? arg->d.mobindex->level : 0;
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
    case ENTITY_OBJINDEX_LEVEL:
        arg->type = ENT_NUMBER;
        arg->d.num = arg->d.objindex ? arg->d.objindex->level : 0;
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
        arg->d.blist = (section) ? section->rooms : NULL;
        break;
    case ENTITY_SECTION_INSTANCE:
        arg->type = ENT_INSTANCE;
        arg->d.instance = (section && IS_VALID(section->instance)) ? section->instance : NULL;
        break;
    case ENTITY_SECTION_MAP:
        arg->type = ENT_STRING;
        arg->d.str = (section && section->map_text) ? section->map_text : &str_empty[0];
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

    case ENTITY_SHIP_OBJECT:
        arg->type = ENT_OBJECT;
        arg->d.obj = ship->ship;
        break;

    default: return NULL;
    }

    return str+1;
}


///////////////////////////////////////////////////////////////////////////////
// Object typed data sub-entity expand functions
///////////////////////////////////////////////////////////////////////////////

char *expand_entity_obj_weapon(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    WEAPON_DATA *w = arg->d.obj_weapon;

    switch(*str) {
    case ENTITY_OBJ_WEAPON_CLASS:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->weapon_class : 0;
        break;
    case ENTITY_OBJ_WEAPON_DAMAGE_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->damage_type : 0;
        break;
    case ENTITY_OBJ_WEAPON_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = w ? w->flags : 0;
        arg->d.bv.table = weapon_type2;
        break;
    case ENTITY_OBJ_WEAPON_DICE:
        arg->type = ENT_DICE;
        arg->d.dice = w ? &w->damage : NULL;
        break;
    case ENTITY_OBJ_WEAPON_RANGE:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->range : 0;
        break;
    case ENTITY_OBJ_WEAPON_MAX_MANA:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_mana : 0;
        break;
    case ENTITY_OBJ_WEAPON_CHARGES:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->charges : 0;
        break;
    case ENTITY_OBJ_WEAPON_MAX_CHARGES:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_charges : 0;
        break;
    case ENTITY_OBJ_WEAPON_COOLDOWN:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->cooldown : 0;
        break;
    case ENTITY_OBJ_WEAPON_RECHARGE_TIME:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->recharge_time : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_armor(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    ARMOR_DATA *a = arg->d.obj_armor;

    switch(*str) {
    case ENTITY_OBJ_ARMOR_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->armor_type : 0;
        break;
    case ENTITY_OBJ_ARMOR_STRENGTH:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->armor_strength : 0;
        break;
    case ENTITY_OBJ_ARMOR_PROT_PIERCE:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->protection[0] : 0;
        break;
    case ENTITY_OBJ_ARMOR_PROT_BASH:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->protection[1] : 0;
        break;
    case ENTITY_OBJ_ARMOR_PROT_SLASH:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->protection[2] : 0;
        break;
    case ENTITY_OBJ_ARMOR_PROT_EXOTIC:
        arg->type = ENT_NUMBER;
        arg->d.num = a ? a->protection[3] : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_container(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    CONTAINER_DATA *c = arg->d.obj_container;

    switch(*str) {
    case ENTITY_OBJ_CONTAINER_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = c ? c->flags : 0;
        arg->d.bv.table = container_flags;
        break;
    case ENTITY_OBJ_CONTAINER_MAX_WEIGHT:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->max_weight : 0;
        break;
    case ENTITY_OBJ_CONTAINER_WEIGHT_MULT:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->weight_multiplier : 0;
        break;
    case ENTITY_OBJ_CONTAINER_MAX_ITEMS:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->max_items : 0;
        break;
    case ENTITY_OBJ_CONTAINER_MAX_VOLUME:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->max_volume : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_fluid_con(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    FLUID_CONTAINER_DATA *f = arg->d.obj_fluid_con;

    switch(*str) {
    case ENTITY_OBJ_FLUID_CON_CAPACITY:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->capacity : 0;
        break;
    case ENTITY_OBJ_FLUID_CON_AMOUNT:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->amount : 0;
        break;
    case ENTITY_OBJ_FLUID_CON_LIQUID:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->liquid : 0;
        break;
    case ENTITY_OBJ_FLUID_CON_POISON:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->poison : 0;
        break;
    case ENTITY_OBJ_FLUID_CON_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = f ? f->flags : 0;
        arg->d.bv.table = container_flags;
        break;
    case ENTITY_OBJ_FLUID_CON_REFILL_RATE:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->refill_rate : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_food(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    FOOD_DATA *f = arg->d.obj_food;

    switch(*str) {
    case ENTITY_OBJ_FOOD_HUNGER:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->hunger : 0;
        break;
    case ENTITY_OBJ_FOOD_FULL:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->full : 0;
        break;
    case ENTITY_OBJ_FOOD_POISON:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->poison : 0;
        break;
    case ENTITY_OBJ_FOOD_TIMER:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->timer : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_furniture(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    FURNITURE_DATA *f = arg->d.obj_furniture;

    switch(*str) {
    case ENTITY_OBJ_FURNITURE_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = f ? f->flags : 0;
        arg->d.bv.table = furniture_flags;
        break;
    case ENTITY_OBJ_FURNITURE_MAX_PEOPLE:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->max_people : 0;
        break;
    case ENTITY_OBJ_FURNITURE_MAX_WEIGHT:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->max_weight : 0;
        break;
    case ENTITY_OBJ_FURNITURE_HEAL_RATE:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->heal_rate : 0;
        break;
    case ENTITY_OBJ_FURNITURE_MANA_RATE:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->mana_rate : 0;
        break;
    case ENTITY_OBJ_FURNITURE_MOVE_RATE:
        arg->type = ENT_NUMBER;
        arg->d.num = f ? f->move_rate : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_portal(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    PORTAL_DATA *p = arg->d.obj_portal;

    switch(*str) {
    case ENTITY_OBJ_PORTAL_EXIT:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = p ? p->exit : 0;
        arg->d.bv.table = portal_exit_flags;
        break;
    case ENTITY_OBJ_PORTAL_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = p ? p->flags : 0;
        arg->d.bv.table = portal_flags;
        break;
    case ENTITY_OBJ_PORTAL_CHARGES:
        arg->type = ENT_NUMBER;
        arg->d.num = p ? p->charges : 0;
        break;
    case ENTITY_OBJ_PORTAL_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = p ? p->type : 0;
        break;
    case ENTITY_OBJ_PORTAL_DESTINATION:
        arg->type = ENT_NUMBER;
        arg->d.num = p ? p->params[0] : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_light(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    LIGHT_DATA *l = arg->d.obj_light;

    switch(*str) {
    case ENTITY_OBJ_LIGHT_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = l ? l->flags : 0;
        arg->d.bv.table = NULL;
        break;
    case ENTITY_OBJ_LIGHT_DURATION:
        arg->type = ENT_NUMBER;
        arg->d.num = l ? l->duration : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_money(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    MONEY_DATA *m = arg->d.obj_money;

    switch(*str) {
    case ENTITY_OBJ_MONEY_SILVER:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->silver : 0;
        break;
    case ENTITY_OBJ_MONEY_GOLD:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->gold : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_wand(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    WAND_DATA *w = arg->d.obj_wand;

    switch(*str) {
    case ENTITY_OBJ_WAND_MAX_MANA:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_mana : 0;
        break;
    case ENTITY_OBJ_WAND_CHARGES:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->charges : 0;
        break;
    case ENTITY_OBJ_WAND_MAX_CHARGES:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_charges : 0;
        break;
    case ENTITY_OBJ_WAND_COOLDOWN:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->cooldown : 0;
        break;
    case ENTITY_OBJ_WAND_RECHARGE_TIME:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->recharge_time : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_corpse(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    CORPSE_DATA *c = arg->d.obj_corpse;

    switch(*str) {
    case ENTITY_OBJ_CORPSE_CORPSE_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->corpse_type : 0;
        break;
    case ENTITY_OBJ_CORPSE_RESURRECTION:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->resurrection : 0;
        break;
    case ENTITY_OBJ_CORPSE_ANIMATION:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->animation : 0;
        break;
    case ENTITY_OBJ_CORPSE_BODY_PARTS:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->body_parts : 0;
        break;
    case ENTITY_OBJ_CORPSE_MOBILE_VNUM:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->mobile_vnum : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_instrument(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    INSTRUMENT_DATA *i = arg->d.obj_instrument;

    switch(*str) {
    case ENTITY_OBJ_INSTRUMENT_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->type : 0;
        break;
    case ENTITY_OBJ_INSTRUMENT_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = i ? i->flags : 0;
        arg->d.bv.table = NULL;
        break;
    case ENTITY_OBJ_INSTRUMENT_BEATS_MIN:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->beats_min : 0;
        break;
    case ENTITY_OBJ_INSTRUMENT_BEATS_MAX:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->beats_max : 0;
        break;
    case ENTITY_OBJ_INSTRUMENT_MANA_MIN:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->mana_min : 0;
        break;
    case ENTITY_OBJ_INSTRUMENT_MANA_MAX:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->mana_max : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_seed(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    SEED_DATA *s = arg->d.obj_seed;

    switch(*str) {
    case ENTITY_OBJ_SEED_GROWTH_TIME:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->growth_time : 0;
        break;
    case ENTITY_OBJ_SEED_OBJECT_VNUM:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->object_vnum : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_cart(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    CART_DATA *c = arg->d.obj_cart;

    switch(*str) {
    case ENTITY_OBJ_CART_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = c ? c->flags : 0;
        arg->d.bv.table = NULL;
        break;
    case ENTITY_OBJ_CART_MIN_STRENGTH:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->min_strength : 0;
        break;
    case ENTITY_OBJ_CART_MOVE_DELAY:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->move_delay : 0;
        break;
    case ENTITY_OBJ_CART_CAPACITY:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->capacity : 0;
        break;
    case ENTITY_OBJ_CART_MAX_ITEMS:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->max_items : 0;
        break;
    case ENTITY_OBJ_CART_WEIGHT_MULT:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->weight_multiplier : 0;
        break;
    case ENTITY_OBJ_CART_VANISH_TIME:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->vanish_time : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_item_ship(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    ITEM_SHIP_DATA *s = arg->d.obj_item_ship;

    switch(*str) {
    case ENTITY_OBJ_ITEM_SHIP_WEIGHT:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->weight : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_MOVE_DELAY:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->move_delay : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_MIN_CREW:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->min_crew : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_CAPACITY:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->capacity : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_MAX_CREW:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->max_crew : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_FIRST_ROOM:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->first_room : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_HIT_POINTS:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->hit_points : 0;
        break;
    case ENTITY_OBJ_ITEM_SHIP_MAX_GUNS:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->max_guns : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_sextant(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    SEXTANT_DATA *s = arg->d.obj_sextant;

    switch(*str) {
    case ENTITY_OBJ_SEXTANT_ACCURACY:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->accuracy : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_weapon_con(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    WEAPON_CONTAINER_DATA *w = arg->d.obj_weapon_con;

    switch(*str) {
    case ENTITY_OBJ_WEAPON_CON_MAX_WEIGHT:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_weight : 0;
        break;
    case ENTITY_OBJ_WEAPON_CON_WEAPON_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->weapon_type : 0;
        break;
    case ENTITY_OBJ_WEAPON_CON_MAX_ITEMS:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->max_items : 0;
        break;
    case ENTITY_OBJ_WEAPON_CON_WEIGHT_MULT:
        arg->type = ENT_NUMBER;
        arg->d.num = w ? w->weight_multiplier : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_book(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    BOOK_DATA *b = arg->d.obj_book;

    switch(*str) {
    case ENTITY_OBJ_BOOK_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = b ? b->flags : 0;
        arg->d.bv.table = container_flags;
        break;
    case ENTITY_OBJ_BOOK_CURRENT_PAGE:
        arg->type = ENT_NUMBER;
        arg->d.num = b ? b->current_page : 0;
        break;
    case ENTITY_OBJ_BOOK_OPEN_PAGE:
        arg->type = ENT_NUMBER;
        arg->d.num = b ? b->open_page : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_herb(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    HERB_DATA *h = arg->d.obj_herb;

    switch(*str) {
    case ENTITY_OBJ_HERB_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = h ? h->type : 0;
        break;
    case ENTITY_OBJ_HERB_HEALING:
        arg->type = ENT_NUMBER;
        arg->d.num = h ? h->healing : 0;
        break;
    case ENTITY_OBJ_HERB_REGENERATIVE:
        arg->type = ENT_NUMBER;
        arg->d.num = h ? h->regenerative : 0;
        break;
    case ENTITY_OBJ_HERB_REFRESHING:
        arg->type = ENT_NUMBER;
        arg->d.num = h ? h->refreshing : 0;
        break;
    case ENTITY_OBJ_HERB_IMMUNITY:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = h ? h->immunity : 0;
        arg->d.bv.table = imm_flags;
        break;
    case ENTITY_OBJ_HERB_RESISTANCE:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = h ? h->resistance : 0;
        arg->d.bv.table = res_flags;
        break;
    case ENTITY_OBJ_HERB_VULNERABILITY:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = h ? h->vulnerability : 0;
        arg->d.bv.table = vuln_flags;
        break;
    case ENTITY_OBJ_HERB_SPELL:
        arg->type = ENT_NUMBER;
        arg->d.num = h ? h->spell : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_mist(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    MIST_DATA *m = arg->d.obj_mist;

    switch(*str) {
    case ENTITY_OBJ_MIST_OBSCURE_MOBS:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->obscure_mobs : 0;
        break;
    case ENTITY_OBJ_MIST_OBSCURE_OBJS:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->obscure_objs : 0;
        break;
    case ENTITY_OBJ_MIST_OBSCURE_ROOM:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->obscure_room : 0;
        break;
    case ENTITY_OBJ_MIST_ICY:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->icy : 0;
        break;
    case ENTITY_OBJ_MIST_FIERY:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->fiery : 0;
        break;
    case ENTITY_OBJ_MIST_ACIDIC:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->acidic : 0;
        break;
    case ENTITY_OBJ_MIST_STINK:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->stink : 0;
        break;
    case ENTITY_OBJ_MIST_WITHER:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->wither : 0;
        break;
    case ENTITY_OBJ_MIST_TOXIC:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->toxic : 0;
        break;
    case ENTITY_OBJ_MIST_SHOCK:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->shock : 0;
        break;
    case ENTITY_OBJ_MIST_FOG:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->fog : 0;
        break;
    case ENTITY_OBJ_MIST_SLEEP:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->sleep : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_trade(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    TRADE_DATA *t = arg->d.obj_trade;

    switch(*str) {
    case ENTITY_OBJ_TRADE_TRADE_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->trade_type : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_tattoo(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    TATTOO_DATA *t = arg->d.obj_tattoo;

    switch(*str) {
    case ENTITY_OBJ_TATTOO_TOUCHES:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->touches : 0;
        break;
    case ENTITY_OBJ_TATTOO_FADING_CHANCE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->fading_chance : 0;
        break;
    case ENTITY_OBJ_TATTOO_FADING_RATE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->fading_rate : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_ink(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    INK_DATA *i = arg->d.obj_ink;

    switch(*str) {
    case ENTITY_OBJ_INK_TYPE0:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->types[0] : 0;
        break;
    case ENTITY_OBJ_INK_TYPE1:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->types[1] : 0;
        break;
    case ENTITY_OBJ_INK_TYPE2:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->types[2] : 0;
        break;
    case ENTITY_OBJ_INK_AMOUNT0:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->amounts[0] : 0;
        break;
    case ENTITY_OBJ_INK_AMOUNT1:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->amounts[1] : 0;
        break;
    case ENTITY_OBJ_INK_AMOUNT2:
        arg->type = ENT_NUMBER;
        arg->d.num = i ? i->amounts[2] : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_telescope(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    TELESCOPE_DATA *t = arg->d.obj_telescope;

    switch(*str) {
    case ENTITY_OBJ_TELESCOPE_DISTANCE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->distance : 0;
        break;
    case ENTITY_OBJ_TELESCOPE_MIN_DISTANCE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->min_distance : 0;
        break;
    case ENTITY_OBJ_TELESCOPE_MAX_DISTANCE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->max_distance : 0;
        break;
    case ENTITY_OBJ_TELESCOPE_BONUS_VIEW:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->bonus_view : 0;
        break;
    case ENTITY_OBJ_TELESCOPE_HEADING:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->heading : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_compass(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    COMPASS_DATA *c = arg->d.obj_compass;

    switch(*str) {
    case ENTITY_OBJ_COMPASS_ACCURACY:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->accuracy : 0;
        break;
    case ENTITY_OBJ_COMPASS_WUID:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->wuid : 0;
        break;
    case ENTITY_OBJ_COMPASS_X:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->x : 0;
        break;
    case ENTITY_OBJ_COMPASS_Y:
        arg->type = ENT_NUMBER;
        arg->d.num = c ? c->y : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_body_part(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    BODY_PART_DATA *b = arg->d.obj_body_part;

    switch(*str) {
    case ENTITY_OBJ_BODY_PART_PARTS:
        arg->type = ENT_NUMBER;
        arg->d.num = b ? b->parts : 0;
        break;
    case ENTITY_OBJ_BODY_PART_RACE_UID:
        arg->type = ENT_NUMBER;
        arg->d.num = b ? b->race_uid : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_scroll(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    SCROLL_DATA *s = arg->d.obj_scroll;

    switch(*str) {
    case ENTITY_OBJ_SCROLL_MAX_MANA:
        arg->type = ENT_NUMBER;
        arg->d.num = s ? s->max_mana : 0;
        break;
    case ENTITY_OBJ_SCROLL_FLAGS:
        arg->type = ENT_BITVECTOR;
        arg->d.bv.value = s ? s->flags : 0;
        arg->d.bv.table = NULL;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_tool(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    TOOL_DATA *t = arg->d.obj_tool;

    switch(*str) {
    case ENTITY_OBJ_TOOL_TYPE:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->type : 0;
        break;
    case ENTITY_OBJ_TOOL_TIER:
        arg->type = ENT_NUMBER;
        arg->d.num = t ? t->tier : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_jewelry(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    JEWELRY_DATA *j = arg->d.obj_jewelry;

    switch(*str) {
    case ENTITY_OBJ_JEWELRY_MAX_MANA:
        arg->type = ENT_NUMBER;
        arg->d.num = j ? j->max_mana : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_map(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    MAP_DATA *m = arg->d.obj_map;

    switch(*str) {
    case ENTITY_OBJ_MAP_WUID:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->wuid : 0;
        break;
    case ENTITY_OBJ_MAP_X:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->x : 0;
        break;
    case ENTITY_OBJ_MAP_Y:
        arg->type = ENT_NUMBER;
        arg->d.num = m ? m->y : 0;
        break;
    default: return NULL;
    }
    return str+1;
}

char *expand_entity_obj_page(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    PAGE_DATA *p = arg->d.obj_page;

    switch(*str) {
    case ENTITY_OBJ_PAGE_PAGE_NO:
        arg->type = ENT_NUMBER;
        arg->d.num = p ? p->page_no : 0;
        break;
    case ENTITY_OBJ_PAGE_TITLE:
        arg->type = ENT_STRING;
        arg->d.str = (p && p->title) ? p->title : (char*)&str_empty[0];
        break;
    case ENTITY_OBJ_PAGE_TEXT:
        arg->type = ENT_STRING;
        arg->d.str = (p && p->text) ? p->text : (char*)&str_empty[0];
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

char *expand_entity_bitvector(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_BOOLEAN;

        if(arg->d.bv.table)
        {
            BUFFER *buffer = new_buf();
            int bit;
            str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
            if(!str) {
                free_buf(buffer);
                arg->d.boolean = false;
                return NULL;
            }


            bit = flag_lookup(buf_string(buffer), arg->d.bv.table);

            arg->d.boolean = IS_SET(arg->d.bv.value, bit) && true;
            free_buf(buffer);
        }
        else
        {
            arg->d.boolean = false;
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
                arg->d.boolean = false;
                return NULL;
            }

            arg->d.boolean = bitmatrix_isset(buf_string(buffer), arg->d.bm.bank, arg->d.bm.values);
            free_buf(buffer);
        }
        else
        {
            arg->d.boolean = false;
        }
        break;
    default: return NULL;
    }

    return str+1;
}



// Implementation for reserved mobile lookups
EXPAND_TYPE(reserved_mobile)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved mobile vnum
        char *reserved_name = buf_string(buffer);
        // Get the mob index directly
        MOB_INDEX_DATA *mob = get_reserved_mob_index(reserved_name);
        
        if (mob) {
            arg->d.num = mob->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved mobile named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

// Implementation for reserved object lookups
// Fix for expand_entity_reserved_object function

EXPAND_TYPE(reserved_object)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved object vnum
        char *reserved_name = buf_string(buffer);
        
        // Get the object index directly
        OBJ_INDEX_DATA *obj = get_reserved_obj_index(reserved_name);
        
        if (obj) {
            arg->d.num = obj->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved object named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

// Implementation for reserved room lookups
EXPAND_TYPE(reserved_room)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved room vnum
        char *reserved_name = buf_string(buffer);
        
        // Get the room index directly
        ROOM_INDEX_DATA *room = get_reserved_room_index(reserved_name);
        
        if (room) {
            arg->d.num = room->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved room named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

// Implementation for reserved area lookups
EXPAND_TYPE(reserved_area)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved area uid
        char *reserved_name = buf_string(buffer);
        
        // Get the area index directly
        AREA_DATA *area = get_reserved_area_index(reserved_name);
        
        if (area) {
            arg->d.num = area->uid;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved area named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

// Implementation for reserved token lookups
EXPAND_TYPE(reserved_token)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved token vnum
        char *reserved_name = buf_string(buffer);
        
        // Get the token index directly
        TOKEN_INDEX_DATA *token = get_reserved_token_index(reserved_name);
        
        if (token) {
            arg->d.num = token->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved token named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

// Add implementions for all program types
EXPAND_TYPE(reserved_rprog)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved rprog vnum
        char *reserved_name = buf_string(buffer);
        
        // Get the script index directly
        SCRIPT_DATA *script = get_reserved_rprog_index(reserved_name);
        
        if (script) {
            arg->d.num = script->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved rprog named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

EXPAND_TYPE(reserved_oprog)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved oprog vnum
        char *reserved_name = buf_string(buffer);

        // Get the script index directly
        SCRIPT_DATA *script = get_reserved_oprog_index(reserved_name);
        
        if (script) {
            arg->d.num = script->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved oprog named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

EXPAND_TYPE(reserved_mprog)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved mprog vnum
        char *reserved_name = buf_string(buffer);
        
        // Get the script index directly
        SCRIPT_DATA *script = get_reserved_mprog_index(reserved_name);
        
        if (script) {
            arg->d.num = script->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved mprog named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: 
        return NULL;
    }

    return str+1;
}

EXPAND_TYPE(reserved_tprog)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved tprog vnum
        char *reserved_name = buf_string(buffer);
        // Get the script index directly
        SCRIPT_DATA *script = get_reserved_tprog_index(reserved_name);
        
        if (script) {
            arg->d.num = script->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved tprog named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: return NULL;
    }

    return str+1;
}

EXPAND_TYPE(reserved_aprog)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_NUMBER;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info, (info ? *(info->var) : NULL), str+1, buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.num = 0;
            return NULL;
        }
        
        // Get the reserved aprog vnum
        char *reserved_name = buf_string(buffer);

        // Get the script index directly
        SCRIPT_DATA *script = get_reserved_aprog_index(reserved_name);
        
        if (script) {
            arg->d.num = script->vnum;
        } else {
            pbugf(LOG_ERROR, "Could not find reserved aprog named %s", reserved_name);
            arg->d.num = 0;
        }
        
        free_buf(buffer);
        break;
    default: return NULL;
    }

    return str+1;
}

// Implementation for dynamic game settings lookups
EXPAND_TYPE(game_setting)
{
    switch(*str) {
    case ESCAPE_VARIABLE:
        arg->type = ENT_STRING;
        
        BUFFER *buffer = new_buf();
        str = expand_name(info,(info?*(info->var):NULL),str+1,buffer);
        if(!str) {
            free_buf(buffer);
            arg->d.str = &str_empty[0];
            return NULL;
        }
        
        // Get the game setting value, ensuring it's not sensitive
        char *setting_name = buf_string(buffer);
        const struct game_setting_type *setting = get_game_setting(setting_name);
        
        if (setting && !setting->sensitive) {
            clear_buf(arg->buffer);
            
            // Handle different setting types
            switch (setting->type) {
                case SETTING_TYPE_BOOL:
                    add_buf(arg->buffer, *((bool *)setting->ptr) ? "true" : "false");
                    break;
                case SETTING_TYPE_INT:
                    {
                        char num_buf[32];
                        sprintf(num_buf, "%d", *((int *)setting->ptr));
                        add_buf(arg->buffer, num_buf);
                    }
                    break;
                case SETTING_TYPE_FLOAT:
                    {
                        char num_buf[32];
                        sprintf(num_buf, "%.4f", *((float *)setting->ptr));
                        add_buf(arg->buffer, num_buf);
                    }
                    break;
                case SETTING_TYPE_STRING:
                case SETTING_TYPE_EXTSTR:
                    if (*((char **)setting->ptr))
                        add_buf(arg->buffer, *((char **)setting->ptr));
                    break;
                default:
                    add_buf(arg->buffer, "");
            }
            
            arg->d.str = buf_string(arg->buffer);
        } else {
            arg->d.str = &str_empty[0];
        }
        
        free_buf(buffer);
        break;
    default: return NULL;
    }

    return str+1;
}


char *expand_entity_equipment(SCRIPT_VARINFO *info, char *str, SCRIPT_PARAM *arg)
{
    int wearloc = (*str) + WEAR_NONE - ESCAPE_EXTRA;

    if (wearloc > WEAR_NONE && wearloc < MAX_WEAR)
    {
        arg->type = ENT_OBJECT;
        arg->d.obj = IS_VALID(arg->d.mob) ? get_eq_char(arg->d.mob, wearloc) : 0;
        return str+1;
    }

    return NULL;
}
    


char *expand_argument_entity(SCRIPT_VARINFO *info,char *str,SCRIPT_PARAM *arg)
{
    char *next;
    arg->type = ENT_NONE;		// ENT_PRIMARY
    arg->d.num = 0;

    while(str && *str && *str != ESCAPE_END) {
        switch(arg->type) {
        case ENT_PRIMARY:	next = expand_entity_primary(info,str,arg); break;
        case ENT_BOOLEAN:	next = expand_entity_boolean(info,str,arg); break;
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
        case ENT_RACE:		next = expand_entity_race(info,str,arg); break;
        case ENT_CLASS:		next = expand_entity_class(info,str,arg); break;
        case ENT_CLASSLEVEL:	next = expand_entity_classlevel(info,str,arg); break;
        case ENT_SKILLENTRY:	next = expand_entity_skillentry(info,str,arg); break;
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
        case ENT_MOBINDEX:		next = expand_entity_mobindex(info,str,arg); break;
        case ENT_OBJINDEX:		next = expand_entity_objindex(info,str,arg); break;

        case ENT_SECTION:		next = expand_entity_instance_section(info,str,arg); break;
        case ENT_INSTANCE:		next = expand_entity_instance(info,str,arg); break;
        case ENT_DUNGEON:		next = expand_entity_dungeon(info,str,arg); break;

        case ENT_SHIP:			next = expand_entity_ship(info,str,arg); break;

        case ENT_OBJ_WEAPON:		next = expand_entity_obj_weapon(info,str,arg); break;
        case ENT_OBJ_ARMOR:		next = expand_entity_obj_armor(info,str,arg); break;
        case ENT_OBJ_CONTAINER:		next = expand_entity_obj_container(info,str,arg); break;
        case ENT_OBJ_FLUID_CON:		next = expand_entity_obj_fluid_con(info,str,arg); break;
        case ENT_OBJ_FOOD:		next = expand_entity_obj_food(info,str,arg); break;
        case ENT_OBJ_FURNITURE:		next = expand_entity_obj_furniture(info,str,arg); break;
        case ENT_OBJ_PORTAL:		next = expand_entity_obj_portal(info,str,arg); break;
        case ENT_OBJ_LIGHT:		next = expand_entity_obj_light(info,str,arg); break;
        case ENT_OBJ_MONEY:		next = expand_entity_obj_money(info,str,arg); break;
        case ENT_OBJ_WAND:		next = expand_entity_obj_wand(info,str,arg); break;
        case ENT_OBJ_CORPSE:		next = expand_entity_obj_corpse(info,str,arg); break;
        case ENT_OBJ_INSTRUMENT:	next = expand_entity_obj_instrument(info,str,arg); break;
        case ENT_OBJ_SEED:		next = expand_entity_obj_seed(info,str,arg); break;
        case ENT_OBJ_CART:		next = expand_entity_obj_cart(info,str,arg); break;
        case ENT_OBJ_ITEM_SHIP:		next = expand_entity_obj_item_ship(info,str,arg); break;
        case ENT_OBJ_SEXTANT:		next = expand_entity_obj_sextant(info,str,arg); break;
        case ENT_OBJ_WEAPON_CON:	next = expand_entity_obj_weapon_con(info,str,arg); break;
        case ENT_OBJ_BOOK:		next = expand_entity_obj_book(info,str,arg); break;
        case ENT_OBJ_HERB:		next = expand_entity_obj_herb(info,str,arg); break;
        case ENT_OBJ_MIST:		next = expand_entity_obj_mist(info,str,arg); break;
        case ENT_OBJ_TRADE:		next = expand_entity_obj_trade(info,str,arg); break;
        case ENT_OBJ_TATTOO:		next = expand_entity_obj_tattoo(info,str,arg); break;
        case ENT_OBJ_INK:		next = expand_entity_obj_ink(info,str,arg); break;
        case ENT_OBJ_TELESCOPE:		next = expand_entity_obj_telescope(info,str,arg); break;
        case ENT_OBJ_COMPASS:		next = expand_entity_obj_compass(info,str,arg); break;
        case ENT_OBJ_BODY_PART:		next = expand_entity_obj_body_part(info,str,arg); break;
        case ENT_OBJ_SCROLL:		next = expand_entity_obj_scroll(info,str,arg); break;
        case ENT_OBJ_TOOL:		next = expand_entity_obj_tool(info,str,arg); break;
        case ENT_OBJ_JEWELRY:		next = expand_entity_obj_jewelry(info,str,arg); break;
        case ENT_OBJ_MAP:		next = expand_entity_obj_map(info,str,arg); break;
        case ENT_OBJ_PAGE:		next = expand_entity_obj_page(info,str,arg); break;

        case ENT_EQUIPMENT:		next = expand_entity_equipment(info,str,arg); break;

        case ENT_BITVECTOR:		next = expand_entity_bitvector(info,str,arg); break;
        case ENT_BITMATRIX:		next = expand_entity_bitmatrix(info,str,arg); break;
        
        case ENT_RESERVED_MOBILE:
            next = expand_entity_reserved_mobile(info, str, arg);
            break;
            
        case ENT_RESERVED_OBJECT:
            next = expand_entity_reserved_object(info, str, arg);
            break;
            
        case ENT_RESERVED_ROOM:
            next = expand_entity_reserved_room(info, str, arg);
            break;
            
        case ENT_RESERVED_AREA:
            next = expand_entity_reserved_area(info, str, arg);
            break;
            
        case ENT_RESERVED_TOKEN:
            next = expand_entity_reserved_token(info, str, arg);
            break;
            
        case ENT_RESERVED_RPROG:
            next = expand_entity_reserved_rprog(info, str, arg);
            break;
            
        case ENT_RESERVED_OPROG:
            next = expand_entity_reserved_oprog(info, str, arg);
            break;
            
        case ENT_RESERVED_MPROG:
            next = expand_entity_reserved_mprog(info, str, arg);
            break;
            
        case ENT_RESERVED_TPROG:
            next = expand_entity_reserved_tprog(info, str, arg);
            break;
            
        case ENT_RESERVED_APROG:
            next = expand_entity_reserved_aprog(info, str, arg);
            break;
            
        case ENT_GAME_SETTING:
            next = expand_entity_game_setting(info, str, arg);
            break;
            
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

    case ENT_SKILL: {
        SKILL_DATA *sk = (arg->d.sn >= 0) ? skill_from_sn(arg->d.sn) : NULL;
        add_buf(buffer, sk ? sk->name : "none");
        break;
    }

    case ENT_SONG:
        add_buf(buffer, arg->d.song ? arg->d.song->name : "none");
        break;

    case ENT_RACE:
        add_buf(buffer, arg->d.race ? arg->d.race->name : "unknown");
        break;

    case ENT_CLASS:
        add_buf(buffer, arg->d.clazz ? arg->d.clazz->name : "none");
        break;

    case ENT_CLASSLEVEL: {
        CLASS_LEVEL *cl = arg->d.classlevel;
        if (cl && cl->clazz)
            add_buf(buffer, cl->clazz->name);
        else
            add_buf(buffer, "none");
        break;
    }

    case ENT_SKILLENTRY: {
        SKILL_ENTRY *se = arg->d.entry;
        if (se && se->skill_data)
            add_buf(buffer, se->skill_data->name);
        else if (se && se->song)
            add_buf(buffer, se->song->name);
        else
            add_buf(buffer, "none");
        break;
    }
    
    case ENT_BITVECTOR:
        add_buf(buffer, flag_string(arg->d.bv.table, arg->d.bv.value));
        break;

    case ENT_BITMATRIX:
        add_buf(buffer, bitmatrix_string(arg->d.bm.bank, arg->d.bm.values));
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
        else if(info->room) s = (char*)widevnum_string_room(info->room, NULL);
        else if(info->token) s = (char*)widevnum_string_token(info->token->pIndexData, NULL);
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
            add_buf(buffer, var->_.r ? (char*)widevnum_string_room(var->_.r, NULL) : "0");
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
            add_buf(buffer, (var->_.t && var->_.t->pIndexData) ? (char*)widevnum_string_token(var->_.t->pIndexData, NULL) : "0");
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
            DBG2EXITVALUE2(false);
            return false;
        }
    }

//	wiznet("EXPAND_STRING:",NULL,NULL,WIZ_TESTING,0,0);
//	wiznet(start,NULL,NULL,WIZ_TESTING,0,0);
    expand_string_dump(buf_string(buffer));
    DBG2EXITVALUE2(true);
    return true;
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
    char buf[MSL*2+1];

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

    return skip_whitespace(str);
}
