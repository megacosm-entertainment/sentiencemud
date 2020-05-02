/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"

//#define DEBUG_MODULE
#include "debug.h"

static BUFFER *compile_err_buffer = NULL;
static int compile_current_line = 0;


void compile_error(char *msg)
{
	char buf[MSL];
	if(compile_err_buffer) {
		add_buf(compile_err_buffer,msg);
		add_buf(compile_err_buffer,"\n\r");
	}
	sprintf(buf, "SCRIPT ERROR: %s", msg);
	bug(buf,0);
}

void compile_error_show(char *msg)
{
	char buf[MSL];
	if(compile_err_buffer) {
		add_buf(compile_err_buffer,msg);
		add_buf(compile_err_buffer,"\n\r");
	} else {
		sprintf(buf, "SCRIPT ERROR: %s", msg);
		bug(buf,0);
	}
}

static int check_operation(int *opnds,STACK *opr)
{
	int optr;

	DBG2ENTRY2(PTR,opnds,PTR,opr);

	optr = pop(opr,STK_EMPTY);

	if(optr == STK_MAX || optr == STK_EMPTY || *opnds < script_expression_argstack[optr])
		return ERROR4;

	*opnds = *opnds + 1 - script_expression_argstack[optr];
	return DONE;
}

static int check_expession_stack(int *opnds,STACK *stk_opr,int op)
{
	int t;

	DBG2ENTRY3(PTR,opnds,PTR,stk_opr,NUM,op);

	if(op == CH_MAX) return ERROR0;

	// Iterate through the operators that CAN be popped off the stack
	while((t = script_expression_stack_action[op][top(stk_opr,STK_EMPTY)]) == POP &&
		(t = check_operation(opnds,stk_opr)) == DONE);

	if(t == DONE) return DONE;
	else if(t == PUSH) {
		script_expression_push_operator(stk_opr,op);
	} else if(t == DELETE) --stk_opr->t;
	else if(t >= ERROR0) return t;

	return DONE;
}

char *compile_ifcheck(char *str,int type, char **store)
{
	char *p = *store;
	char buf[MIL], buf2[MSL], *s;
	int ifc;

	DBG2ENTRY3(PTR,str,NUM,type,PTR,store);

	str = skip_whitespace(str);

	if(!isalpha(*str)) {
		sprintf(buf2,"Line %d: Ifchecks must start with an alphabetic character.", compile_current_line);
		compile_error_show(buf2);
		return NULL;
	}

	s = buf;
	while(isalnum(*str)) *s++ = *str++;
	*s = 0;

	ifc = ifcheck_lookup(buf, type);
	if(ifc < 0 || ifc > 4095) {
		sprintf(buf2,"Line %d: Invalid ifcheck '%s'.", compile_current_line, buf);
		compile_error_show(buf2);
		return NULL;
	}
	*p++ = (ifc & 0x3F) + ESCAPE_EXTRA;	// ????______LLLLLL
	*p++ = ((ifc>>6) & 0x3F) + ESCAPE_EXTRA;	// ????HHHHHH______

	str = compile_substring(str,type,&p,TRUE,TRUE,FALSE);
	if(!str) {
		sprintf(buf2,"Line %d: Error processing ifcheck call '%s'.", compile_current_line, buf);
		compile_error_show(buf2);
		return NULL;
	}

	*store = p;
	return str+1;
}

char *compile_expression(char *str,int type, char **store)
{
	char buf[MSL];
	STACK optr;
	int op,opnds=0;
	bool expect = FALSE;	// FALSE = number/open, TRUE = operator/close
	char *p = *store, *rest;

	DBG2ENTRY3(PTR,str,NUM,type,PTR,store);

	str = skip_whitespace(str);	// Process to the first non-whitespace

	*p++ = ESCAPE_EXPRESSION;

	optr.t = 0;

	while(*str && *str != ']') {
		str = skip_whitespace(str);
		if(isdigit(*str)) {	// Constant
			if(expect) {
				sprintf(buf,"Line %d: Expecting an operator.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}

			while(isdigit(*str)) *p++ = *str++;
			++opnds;
			expect = TRUE;
		} else if(isalpha(*str)) {	// Variable (simple, alpha-only)
			if(expect) {
				sprintf(buf,"Line %d: Expecting an operator.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			*p++ = ESCAPE_VARIABLE;
			while(isalpha(*str)) *p++ = *str++;
			*p++ = ESCAPE_END;
			++opnds;
			expect = TRUE;
		} else if(*str == '"') {	// Variable (long, any character)
			if(expect) {
				sprintf(buf,"Line %d: Expecting an operator.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			*p++ = ESCAPE_VARIABLE;
			++str;
			while(*str && *str != '"' && isprint(*str)) *p++ = *str++;
			if(*str != '"') {
				sprintf(buf,"Line %d: Missing quote around long variable name in expression.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			++str;
			*p++ = ESCAPE_END;
			++opnds;
			expect = TRUE;
		} else if(*str == '[') {
			if(expect) {
				sprintf(buf,"Line %d: Expecting an operator.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}

			*p++ = ESCAPE_EXPRESSION;
			rest = compile_ifcheck(str+1,type,&p);
			if(!rest) {
				// Error message handled by compile_ifcheck
				return NULL;
			}
			str = rest;
			*p++ = ESCAPE_END;
			++opnds;
			expect = TRUE;
		} else if(*str != ']') {
// -expression
//
//
//
//


			switch(*str) {
			case '+': op = expect ? CH_ADD : CH_MAX; expect=FALSE; break;
			case '-': op = expect ? CH_SUB : CH_NEG; expect=FALSE; break;
			case '*': op = expect ? CH_MUL : CH_MAX; expect=FALSE; break;
			case '%': op = expect ? CH_MOD : CH_MAX; expect=FALSE; break;
			case '/': op = expect ? CH_DIV : CH_MAX; expect=FALSE; break;
			case ':': op = expect ? CH_RAND : CH_MAX; expect=FALSE; break;
			case '!': op = expect ? CH_MAX : CH_NOT; expect=FALSE; break;
			case '(': op = expect ? CH_MAX : CH_OPEN; expect=FALSE; break;
			case ')': op = expect ? CH_CLOSE : CH_MAX; expect=TRUE; break;
			default:  op = CH_MAX; break;
			}

			if((op = check_expession_stack(&opnds,&optr,op)) != DONE) {
				switch(op) {
				case ERROR0:
					sprintf(buf,"Line %d: Invalid operator '%c' encountered.", compile_current_line, *str);
					break;
				case ERROR1:
					sprintf(buf,"Line %d: Unmatched right parenthesis.", compile_current_line);
					break;
				case ERROR2:
					sprintf(buf,"Line %d: Unmatched left parenthesis.", compile_current_line);
					break;
				case ERROR3:	// NOT DONE HERE!  They will "result" in zero with a bug message
					sprintf(buf,"Line %d: Division by zero.", compile_current_line);
					break;
				case ERROR4:
					sprintf(buf,"Line %d: Invalid expression.", compile_current_line);
					break;
				default:
					sprintf(buf,"Line %d: Unknown expression error.", compile_current_line);
					break;
				}
				compile_error_show(buf);
				return NULL;
			}
			*p++ = *str++;
		}
	}

	str = skip_whitespace(str);
	// There should be a terminating ] after all this mess
	if(*str != ']') {
		sprintf(buf,"Line %d: Missing terminating ']'.", compile_current_line);
		compile_error_show(buf);
		return NULL;
	}

	if(!check_expession_stack(&opnds,&optr,CH_EOS)) {
		sprintf(buf,"Line %d: Invalid expression.", compile_current_line);
		compile_error_show(buf);
		return NULL;
	}

	// There must be one result left
	if(opnds != 1) {
		sprintf(buf,"Line %d: Expression doesn't compute to one value.", compile_current_line);
		compile_error_show(buf);
		return NULL;
	}

	*p++ = ESCAPE_END;

	*store = p;
	return str+1;
}

char *compile_variable(char *str, char **store, int type, bool bracket, bool anychar)
{
	char *p = *store;
	*p++ = ESCAPE_VARIABLE;
	while(str && *str && *str != '>') {
		if(isalpha(*str)) *p++ = *str++;
		else if(*str == '<') {
			str = compile_variable(str+1,&p, type,TRUE,TRUE);
			if(!str) return NULL;
		} else if(*str == '[') {
			str = compile_expression(str+1,type,&p);
			if(!str) return NULL;
		} else if(anychar && isprint(*str)) *p++ = *str++;
		else {
			char buf[MIL];
			sprintf(buf,"Line %d: Invalid character in variable name.", compile_current_line);
			compile_error_show(buf);
			return NULL;
		}
	}
	*p++ = ESCAPE_END;
	if(bracket) {
		if(*str != '>') {
			char buf[MIL];
			sprintf(buf,"Line %d: Missing terminating '>'.", compile_current_line);
			compile_error_show(buf);
			return NULL;
		}
		str++;
	}

	*store = p;
	return str;
}

char *compile_entity_field(char *str,char *field, char *suffix)
{
	char buf[MSL];

	str = skip_whitespace(str);

	if(*str == '"') {
		++str;
		while(*str && *str != '"') *field++ = *str++;
		if(!*str) {
			sprintf(buf,"Line %d: Expecting terminating '\"' in $().", compile_current_line);
			compile_error_show(buf);
			return NULL;
		}
		++str;
	} else {
		while(*str && !isspace(*str) && *str != ')' && *str != '.' && *str != ':') *field++ = *str++;
	}
	*field = 0;

	str = skip_whitespace(str);

	// Has a type suffix, used for variables
	if(*str == ':') {
		str = skip_whitespace(str+1);
		while(*str && !isspace(*str) && *str != ')' && *str != '.') *suffix++ = *str++;
	} else if(*str != ')' && *str != '.') {
		sprintf(buf,"Line %d: Invalid character in $().", compile_current_line);
		compile_error_show(buf);
		return NULL;
	}
	*suffix = 0;

	return str;
}

char *compile_entity(char *str,int type, char **store)
{
	char buf[MSL];
	char field[MIL],suffix[MIL]/*, *s*/;
	int ent = ENT_PRIMARY, next_ent;
	char *p = *store;
	const ENT_FIELD *ftype;

	DBG2ENTRY3(PTR,str,NUM,type,PTR,store);

	*p++ = ESCAPE_ENTITY;
	while(*str && *str != ')') {
		str = skip_whitespace(str);

		if(ent == ENT_PRIMARY) {
			if(*str == '.') {
				sprintf(buf,"Line %d: Unexpected '.' in $().", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
		} else {
			if(*str != '.') {
				sprintf(buf,"Line %d: Expecting '.' in $().", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			str = skip_whitespace(str+1);
		}

		str = compile_entity_field(str,field,suffix);
		if(!str) return NULL;

//		sprintf(buf,"Line %d: $() field '%s' ent %d.", compile_current_line, field, ent);
//		compile_error_show(buf);

		if(ent == ENT_EXTRADESC || ent == ENT_HELP) {
			if(suffix[0]) {
				sprintf(buf,"Line %d: type suffix is only allowed for variable fields.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			if(!compile_variable(field,&p,type,FALSE,TRUE))
				return NULL;
			*p++ = ENTITY_VAR_STR;
			next_ent = ENT_STRING;

		} else if(ent == ENT_BITVECTOR) {
			if(suffix[0]) {
				sprintf(buf,"Line %d: type suffix is only allowed for variable fields.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
			if(!compile_variable(field,&p,type,FALSE,TRUE))
				return NULL;
			*p++ = ENTITY_VAR_BOOLEAN;
			next_ent = ENT_BOOLEAN;

		// Is this a variable call?
		} else if(suffix[0]) {
			if(script_entity_allow_vars(ent)) {
				if(!field[0]) {
					sprintf(buf,"Line %d: Missing $() variable name.", compile_current_line);
					compile_error_show(buf);
					return NULL;
				}

				ftype = entity_type_lookup(suffix,entity_types);
				if(!ftype) {
					sprintf(buf,"Line %d: Invalid $() variable typing '%s'.", compile_current_line, suffix);
					compile_error_show(buf);
					return NULL;
				}

				if(!compile_variable(field,&p,type,FALSE,TRUE))
					return NULL;
				*p++ = ftype->code;
				next_ent = ftype->type;
			} else {
				sprintf(buf,"Line %d: type suffix is only allowed for variable fields.", compile_current_line);
				compile_error_show(buf);
				return NULL;
			}
		} else if((ftype = entity_type_lookup(field,script_entity_fields(ent)))) {
			*p++ = ftype->code;
			next_ent = ftype->type;
		} else {
			sprintf(buf,"Line %d: Invalid $() field '%s'.", compile_current_line, field);
			compile_error_show(buf);
			return NULL;
		}

		str = skip_whitespace(str);

		if(next_ent == ENT_UNKNOWN) {
			switch(ent) {
			case ENT_PRIMARY:
				switch(type) {
				case IFC_M: ent = ENT_MOBILE; break;
				case IFC_O: ent = ENT_OBJECT; break;
				case IFC_R: ent = ENT_ROOM; break;
				case IFC_T: ent = ENT_TOKEN; break;
				case IFC_A: ent = ENT_AREA; break;
				case IFC_I: ent = ENT_INSTANCE; break;
				case IFC_D: ent = ENT_DUNGEON; break;
				default:
					sprintf(buf,"Line %d: Invalid primary $() identifier '%s'.", compile_current_line, field);
					compile_error_show(buf);
					return NULL;
				}
				break;
			case ENT_OLLIST_MOB:	ent = ENT_MOBILE; break;
			case ENT_OLLIST_OBJ:	ent = ENT_OBJECT; break;
			case ENT_OLLIST_TOK:	ent = ENT_TOKEN; break;
			case ENT_OLLIST_AFF:	ent = ENT_AFFECT; break;

			case ENT_BLLIST_ROOM:	ent = ENT_ROOM; break;
			case ENT_BLLIST_MOB:	ent = ENT_MOBILE; break;
			case ENT_BLLIST_OBJ:	ent = ENT_OBJECT; break;
			case ENT_BLLIST_TOK:	ent = ENT_TOKEN; break;
			case ENT_BLLIST_EXIT:	ent = ENT_EXIT; break;
			case ENT_BLLIST_SKILL:	ent = ENT_SKILLINFO; break;
			case ENT_BLLIST_AREA:	ent = ENT_AREA; break;
			case ENT_BLLIST_WILDS:	ent = ENT_WILDS; break;

			case ENT_PLLIST_STR:	ent = ENT_STRING; break;
			case ENT_PLLIST_CONN:	ent = ENT_CONN; break;
			case ENT_PLLIST_ROOM:	ent = ENT_ROOM; break;
			case ENT_PLLIST_MOB:	ent = ENT_MOBILE; break;
			case ENT_PLLIST_OBJ:	ent = ENT_OBJECT; break;
			case ENT_PLLIST_TOK:	ent = ENT_TOKEN; break;
			case ENT_PLLIST_CHURCH:	ent = ENT_CHURCH; break;

			case ENT_ILLIST_VARIABLE:	ent = ENT_VARIABLE; break;

			default:
				sprintf(buf,"Line %d: Invalid $() primary '%s'.", compile_current_line, field);
				compile_error_show(buf);
				return NULL;
			}
		} else
			ent = next_ent;
//		sprintf(buf,"Line %d: $() new ent %d.", compile_current_line, ent);
//		compile_error_show(buf);
	}

	if(*str != ')') {
		sprintf(buf,"Line %d: Missing terminating ')'.", compile_current_line);
		compile_error_show(buf);
		return NULL;
	}

	*p++ = ESCAPE_END;
	*store = p;
	return str+1;
}

char *compile_substring(char *str, int type, char **store, bool ifc, bool doquotes, bool recursed)
{
	char buf[MSL];
	char buf2[MSL];
	char *p, *s, ch;
	bool startword = TRUE, inquote = FALSE;

	DBG2ENTRY4(PTR,str,NUM,type,PTR,store,FLG,ifc);

	p = *store;
	while(*str) {
		// Escape code
		if(*str == '$') {
			if(str[1] == '[')
				str = compile_expression(str+2,type,&p);
			else if(str[1] == '(')
				str = compile_entity(str+2,type,&p);
			else if(str[1] == '<') {
				str = compile_variable(str+2,&p,type,TRUE,TRUE);
			} else if(isalpha(str[1])) {
				*p++ = ESCAPE_UA + str[1] - 'A';
				str += 2;
			} else if(str[1] == '$') {
				*p++ = '$';
				str += 2;
			} else {
				sprintf(buf2,"Line %d: Invalid $-escape sequence.", compile_current_line);
				compile_error_show(buf2);
				return NULL;
			}

			if(!str) {
				return NULL;
			}

			startword = FALSE;
		} else if(ifc && *str == ']')
			break;
		else if(doquotes) {
			if(isspace(*str)) {
				*p++ = *str++;
				startword = TRUE;
			} else {
				s = recursed ? p : buf;
				// Taken from one_argument_norm, except it doesn't skip trailing whitespace
				ch = ' ';
				if(!recursed && doquotes && startword && (*str == '\'' || *str == '"')) {
					inquote = TRUE;
					*s++ = (ch = *str++);
				}
				while(*str && *str != ch) {
					if(ifc && *str == ']') break;
					*s++ = *str++;
				}
				if(*str && inquote) {
					if(ifc && *str == ']') {
						sprintf(buf2,"Line %d: Non-terminated quoted string.", compile_current_line);
						compile_error_show(buf2);
						return NULL;
					}
					inquote = FALSE;
					*s++ = *str++;
				}
				if(!*str && inquote) {
					sprintf(buf2,"Line %d: Non-terminated quoted string.", compile_current_line);
					compile_error_show(buf2);
					return NULL;
				}

				if(recursed)
					p = s;
				else {
					*s = 0;
					s = compile_substring(buf, type, &p, ifc, FALSE, TRUE);
					if(!s) {
						sprintf(buf2,"Line %d: Could not recurse substring.", compile_current_line);
						compile_error_show(buf2);
						return NULL;
					}
				}
			}
		} else
			*p++ = *str++;
	}

	*store = p;
	return str;
}

void compile_string_dump(char *str)
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



// Compiles the string
// Looks for various things
// $[ ]		Expression escapes
// $( )		Entity escapes
// $< >		Variable escapes
// $*		Normal $-codes
// 0-9		Numbers
// string	String
//////
char *compile_string(char *str, int type, int *length, bool doquotes)
{
	char buf[MSL*2+1];
	char *result, *p;

	DBG2ENTRY3(PTR,str,NUM,type,PTR,length);

	p = buf;
	str = compile_substring(str,type,&p,FALSE,doquotes,FALSE);
//_D_
	if(!str) {
		*p = 0;
		compile_string_dump(buf);
		return NULL;
	}

	// Trim off excess whitespace
	while(p > buf && isspace(p[-1])) --p;
//_D_

	*p = 0;
	*length = p - buf;

//_D_
	result = alloc_mem(*length + 1);
//_D_
	if(result) memcpy(result,buf,*length + 1);

	DBG2EXITVALUE1(PTR,result);
	return result;
}

bool compile_script(BUFFER *err_buf,SCRIPT_DATA *script, char *source, int type)
{
	BOOLEXP *bool_exp = NULL, *bool_exp_root = NULL;
	char labels[MAX_NAMED_LABELS][MIL];
	SCRIPT_CODE *code;
	char *src, *start, *line, eol;
	char buf[MIL], rbuf[MSL];
	bool comment, neg, doquotes, valid, linevalid, disable, inspect, processrest, incline;
	int state[MAX_NESTED_LEVEL];
	int loops[MAX_NESTED_LOOPS];
	int i, x, y, level, loop, rline, cline, lines, length, errors,named_labels, bool_exp_cline;
	char *type_name;


	DBG2ENTRY4(PTR,err_buf,PTR,script,PTR,source,NUM,type);

	if(type == IFC_M) {
		script->type = PRG_MPROG;
		type_name = "MOB";
	} else if(type == IFC_O) {
		script->type = PRG_OPROG;
		type_name = "OBJ";
	} else if(type == IFC_R) {
		script->type = PRG_RPROG;
		type_name = "ROOM";
	} else if(type == IFC_T) {
		script->type = PRG_TPROG;
		type_name = "TOKEN";
	} else if(type == IFC_A) {
		script->type = PRG_APROG;
		type_name = "AREA";
	} else if(type == IFC_I) {
		script->type = PRG_IPROG;
		type_name = "INSTANCE";
	} else if(type == IFC_D) {
		script->type = PRG_DPROG;
		type_name = "DUNGEON";
	} else {
		script->type = -1;
		type_name = "???";
	}

	DBG3MSG2("Parsing %s(%d)\n", type_name, script->vnum);

	compile_err_buffer = err_buf;

	inspect = (bool)IS_SET(script->flags,SCRIPT_INSPECT);
	disable = FALSE;

	// Clear the inspection flag.  This is only set when the COMPILE command
	//	is issued by a non-IMP.
	script->flags &= ~SCRIPT_INSPECT;

	bool_exp = NULL;
	bool_exp_cline = -1;

	// Count the lines
	i = 0;
	src = source;
	while(*src) {
		comment = FALSE;
		src = start = skip_whitespace(src);

		// if the first non whitespace is '*', comment
		if(*src == '*') comment = TRUE;

		// Skip to EOL/EOS
		while(*src && *src != '\n' && *src != '\r') ++src;

		// If not a comment and there was anything on the line,
		//	there is something to parse
		if(!comment && src != start) i++;

		// Skip over repeated EOL's, including blank lines
		while(*src == '\n' || *src == '\r') ++src;
	}

	// Create the instruction list
	lines = i;
	code = alloc_mem((lines+1)*sizeof(SCRIPT_CODE));
	if(!code) return FALSE;
	memset(code,0,(lines+1)*sizeof(SCRIPT_CODE));

	// Initialize parsing
	for(i=0;i<MAX_NESTED_LEVEL;i++) state[i] = IN_BLOCK;
	for(i=0;i<MAX_NESTED_LOOPS;i++) loops[i] = -1;
	for(i=0;i<MAX_NAMED_LABELS;i++) labels[i][0] = 0;
	i = 0;
	cline = 0;
	line = 0;
	rline = 0;
	level = 0;
	loop = 0;
	named_labels = 0;
	src = source;
	eol = '\0';

	valid = TRUE;
	errors = 0;

	// Start parsing
	while(*src) {
		compile_current_line = ++rline;	// Increase real line number

		comment = FALSE;
		src = start = skip_whitespace(src);
		// if the first non whitespace is '*', comment
		if(*src == '*') comment = TRUE;

		// Skip to EOL/EOS
		while(*src && *src != '\n' && *src != '\r') ++src;
		eol = *src; *src = '\0';

		// If not a comment and there was anything on the line,
		//	there is something to parse
		if(!comment && src != start) {
			line = start;
			line = one_argument(line,buf);

			doquotes = TRUE;
			linevalid = TRUE;
			processrest = TRUE;
			incline = TRUE;

			do {


				if(!str_cmp(buf,"end") || !str_cmp(buf,"break")) {
					code[cline].opcode = OP_END;
					code[cline].level = level;
					state[level] = IN_BLOCK;
				} else if(!str_cmp(buf,"gotoline")) {
					code[cline].opcode = OP_GOTOLINE;
					code[cline].level = level;
					state[level] = IN_BLOCK;
					if(inspect) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'gotoline' requires inspection by an IMP.", rline);
						compile_error_show(rbuf);
						disable = TRUE;
					}
				} else if(!str_cmp(buf,"if")) {
					if(state[level] == BEGIN_BLOCK) {
						sprintf(rbuf,"Line %d: Unexpected 'if'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					neg = FALSE;
					code[cline].level = level;
					code[cline].opcode = OP_IF;

					state[level++] = BEGIN_BLOCK;
					if (level >= MAX_NESTED_LEVEL) {
						sprintf(rbuf,"Line %d: Nested levels too deep.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					if(!str_prefix("not ",line)) {
						neg = !neg;
						line = skip_whitespace(line+3);
					}

					if(*line == '!') {
						neg = !neg;
						++line;
					}

					bool_exp_root = bool_exp = new_boolexp();
					bool_exp->type = neg ? BOOLEXP_NOT : BOOLEXP_TRUE;
					bool_exp_cline = cline;

					if(*line == '$') {
						bool_exp->param = -1;

					} else {
						line = one_argument(line,buf);
						code[cline].param = ifcheck_lookup(buf,type);
						bool_exp->param = ifcheck_lookup(buf,type);
						if(bool_exp->param < 0) {
							sprintf(rbuf,"Line %d: Invalid ifcheck '%s'.", rline, buf);
							compile_error_show(rbuf);
							linevalid = FALSE;
							break;
						}
					}

					processrest = FALSE;
					bool_exp->rest = compile_string(line,type,&length,doquotes);
					if(!bool_exp->rest) {
						sprintf(rbuf,"Line %d: Error parsing string.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else
						bool_exp->length = (short)length;

					code[cline].rest = (char *)bool_exp;

					state[level] = END_BLOCK;
				} else if(!str_cmp(buf,"elseif")) {
					if (!level || state[level-1] != BEGIN_BLOCK) {
						sprintf(rbuf,"Line %d: Unexpected 'elseif'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					neg = FALSE;
					code[cline].level = level-1;
					code[cline].opcode = OP_ELSEIF;

					if(!str_prefix("not ",line)) {
						neg = !neg;
						line = skip_whitespace(line+3);
					}

					if(*line == '!') {
						neg = !neg;
						++line;
					}

					bool_exp_root = bool_exp = new_boolexp();
					bool_exp->type = neg ? BOOLEXP_NOT : BOOLEXP_TRUE;
					bool_exp_cline = cline;

					if(*line == '$') {
						bool_exp->param = -1;

					} else {
						line = one_argument(line,buf);
						code[cline].param = ifcheck_lookup(buf,type);
						bool_exp->param = ifcheck_lookup(buf,type);
						if(bool_exp->param < 0) {
							sprintf(rbuf,"Line %d: Invalid ifcheck '%s'.", rline, buf);
							compile_error_show(rbuf);
							linevalid = FALSE;
							break;
						}
					}

					processrest = FALSE;
					bool_exp->rest = compile_string(line,type,&length,doquotes);
					if(!bool_exp->rest) {
						sprintf(rbuf,"Line %d: Error parsing string.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else
						bool_exp->length = (short)length;

					code[cline].rest = (char *)bool_exp;

					state[level] = END_BLOCK;
				} else if(!str_cmp(buf,"while")) {
					if(state[level] == BEGIN_BLOCK || state[level] == IN_WHILE) {
						sprintf(rbuf,"Line %d: Unexpected 'while'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					neg = FALSE;
					code[cline].level = level;
					code[cline].opcode = OP_WHILE;

					state[level++] = IN_WHILE;
					if (level >= MAX_NESTED_LEVEL) {
						sprintf(rbuf,"Line %d: Nested levels too deep.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					state[level] = IN_BLOCK;

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;
					if(x >= 0) {
						sprintf(rbuf,"Line %d: duplicate named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(named_labels >= MAX_NAMED_LABELS) {
						sprintf(rbuf,"Line %d: too many named labels in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(loop >= MAX_NESTED_LOOPS) {
						sprintf(rbuf,"Line %d: too many nested loops in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					loops[loop++] = named_labels;
					code[cline].label = named_labels;
					strcpy(labels[named_labels++],buf);

					if(!str_prefix("not ",line)) {
						neg = !neg;
						line = skip_whitespace(line+3);
					}

					if(*line == '!') {
						neg = !neg;
						++line;
					}

					bool_exp_root = bool_exp = new_boolexp();
					bool_exp->type = neg ? BOOLEXP_NOT : BOOLEXP_TRUE;
					bool_exp_cline = cline;

					if(*line == '$') {
						bool_exp->param = -1;

					} else {
						line = one_argument(line,buf);
						bool_exp->param = ifcheck_lookup(buf,type);
						if(bool_exp->param < 0) {
							sprintf(rbuf,"Line %d: Invalid ifcheck '%s'.", rline, buf);
							compile_error_show(rbuf);
							linevalid = FALSE;
							break;
						}
					}

					processrest = FALSE;
					bool_exp->rest = compile_string(line,type,&length,doquotes);
					if(!bool_exp->rest) {
						sprintf(rbuf,"Line %d: Error parsing string.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else
						bool_exp->length = (short)length;

					code[cline].rest = (char *)bool_exp;

				} else if(!str_cmp(buf,"or")) {
					BOOLEXP *be;

					if (!level || (state[level-1] != BEGIN_BLOCK && state[level-1] != IN_WHILE)) {
						sprintf(rbuf,"Line %d: 'or' used without 'if' or 'while'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					neg = FALSE;

					if(!str_prefix("not ",line)) {
						neg = !neg;
						line = skip_whitespace(line+3);
					}

					if(*line == '!') {
						neg = !neg;
						++line;
					}

					be = new_boolexp();
					be->type = BOOLEXP_OR;
					code[bool_exp_cline].rest = (char *)be;	// Update the code entry
					be->left = bool_exp_root;
					be->left->parent = be;
					bool_exp_root = be;

					// Add expression for this line
					bool_exp = be->right = new_boolexp();
					bool_exp->parent = be;
					bool_exp->type = neg ? BOOLEXP_NOT : BOOLEXP_TRUE;

					code[cline].level = level-1;

					if(*line == '$') {
						bool_exp->param = -1;
					} else {
						line = one_argument(line,buf);
						bool_exp->param = ifcheck_lookup(buf,type);
						if(bool_exp->param < 0) {
							sprintf(rbuf,"Line %d: Invalid ifcheck '%s'.", rline, buf);
							compile_error_show(rbuf);
							linevalid = FALSE;
							break;
						}
					}


					processrest = FALSE;
					incline = FALSE;
					bool_exp->rest = compile_string(line,type,&length,doquotes);
					if(!bool_exp->rest) {
						sprintf(rbuf,"Line %d: Error parsing string.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else
						bool_exp->length = (short)length;

				} else if(!str_cmp(buf,"and")) {
					BOOLEXP *be;

					if (!level || (state[level-1] != BEGIN_BLOCK && state[level-1] != IN_WHILE)) {
						sprintf(rbuf,"Line %d: 'and' used without 'if' or 'while'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					neg = FALSE;

					if(!str_prefix("not ",line)) {
						neg = !neg;
						line = skip_whitespace(line+3);
					}

					if(*line == '!') {
						neg = !neg;
						++line;
					}

					code[cline].level = level-1;


					be = new_boolexp();
					be->type = BOOLEXP_AND;
					be->left = bool_exp;
					be->right = new_boolexp();

					if( bool_exp->parent ) {
						bool_exp->parent->right = be;
					} else {
						// This is the root node too
						bool_exp_root = be;
						code[bool_exp_cline].rest = (char *)be;
					}

					be->left->parent = be;
					be->right->parent = be;

					bool_exp->parent = be;
					bool_exp = be->right;
					bool_exp->type = neg ? BOOLEXP_NOT : BOOLEXP_TRUE;


					if(*line == '$') {
						bool_exp->param = -1;
					} else {
						line = one_argument(line,buf);
						bool_exp->param = ifcheck_lookup(buf,type);
						if(bool_exp->param < 0) {
							sprintf(rbuf,"Line %d: Invalid ifcheck '%s'.", rline, buf);
							compile_error_show(rbuf);
							linevalid = FALSE;
							break;
						}
					}

					processrest = FALSE;
					incline = FALSE;
					bool_exp->rest = compile_string(line,type,&length,doquotes);
					if(!bool_exp->rest) {
						sprintf(rbuf,"Line %d: Error parsing string.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else
						bool_exp->length = (short)length;

				} else if(!str_cmp(buf,"else")) {
					if (!level || state[level-1] != BEGIN_BLOCK) {
						sprintf(rbuf,"Line %d: Unmatched 'else'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					state[level] = IN_BLOCK;
					code[cline].level = level-1;
					code[cline].opcode = OP_ELSE;
				} else if(!str_cmp(buf,"endif")) {
					if (!level || state[level-1] != BEGIN_BLOCK) {
						sprintf(rbuf,"Line %d: Unmatched 'endif'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					code[cline].level = level-1;
					code[cline].opcode = OP_ENDIF;
					state[level] = IN_BLOCK;
					state[--level] = END_BLOCK;
				} else if(!str_cmp(buf,"for")) {
					code[cline].opcode = OP_FOR;
					code[cline].level = level;

					state[level++] = IN_FOR;
					if (level >= MAX_NESTED_LEVEL) {
						sprintf(rbuf,"Line %d: Nested levels too deep.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					state[level] = IN_BLOCK;

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;
					if(x >= 0) {
						sprintf(rbuf,"Line %d: duplicate named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(named_labels >= MAX_NAMED_LABELS) {
						sprintf(rbuf,"Line %d: too many named labels in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(loop >= MAX_NESTED_LOOPS) {
						sprintf(rbuf,"Line %d: too many nested loops in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					loops[loop++] = named_labels;
					code[cline].label = named_labels;
					strcpy(labels[named_labels++],buf);
				} else if(!str_cmp(buf,"endfor")) {
					if (!level || state[level-1] != IN_FOR) {
						sprintf(rbuf,"Line %d: Unmatched 'endfor'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					if(loops[loop-1] != x) {
						sprintf(rbuf,"Line %d: trying to end a loop inside another loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					loops[--loop] = -1;
					code[cline].level = level-1;
					code[cline].opcode = OP_ENDFOR;
					code[cline].label = x;
					state[level] = IN_BLOCK;
					state[--level] = IN_BLOCK;
				} else if(!str_cmp(buf,"exitfor")) {
					if (!level || !loop) {
						sprintf(rbuf,"Line %d: 'exitfor' used outside of for loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					for(y = loop;y-- > 0;)
						if(loops[y] == x) break;

					if(y < 0) {
						sprintf(rbuf,"Line %d: 'exitfor' used outside of named loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					code[cline].level = level;
					code[cline].opcode = OP_EXITFOR;
					code[cline].label = x;
					state[level] = IN_BLOCK;
				} else if(!str_cmp(buf,"list")) {
					code[cline].opcode = OP_LIST;
					code[cline].level = level;

					state[level++] = IN_LIST;
					if (level >= MAX_NESTED_LEVEL) {
						sprintf(rbuf,"Line %d: Nested levels too deep.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					state[level] = IN_BLOCK;

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;
					if(x >= 0) {
						sprintf(rbuf,"Line %d: duplicate named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(named_labels >= MAX_NAMED_LABELS) {
						sprintf(rbuf,"Line %d: too many named labels in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					if(loop >= MAX_NESTED_LOOPS) {
						sprintf(rbuf,"Line %d: too many nested loops in script.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}
					loops[loop++] = named_labels;
					code[cline].label = named_labels;
					strcpy(labels[named_labels++],buf);
				} else if(!str_cmp(buf,"endlist")) {
					if (!level || state[level-1] != IN_LIST) {
						sprintf(rbuf,"Line %d: Unmatched 'endlist'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					if(loops[loop-1] != x) {
						sprintf(rbuf,"Line %d: trying to end a loop inside another loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					loops[--loop] = -1;
					code[cline].level = level-1;
					code[cline].opcode = OP_ENDLIST;
					code[cline].label = x;
					state[level] = IN_BLOCK;
					state[--level] = IN_BLOCK;
				} else if(!str_cmp(buf,"exitlist")) {
					if (!level || !loop) {
						sprintf(rbuf,"Line %d: 'exitlist' used outside of for loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					for(y = loop;y-- > 0;)
						if(loops[y] == x) break;

					if(y < 0) {
						sprintf(rbuf,"Line %d: 'exitlist' used outside of named loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					code[cline].level = level;
					code[cline].opcode = OP_EXITLIST;
					code[cline].label = x;
					state[level] = IN_BLOCK;
				} else if(!str_cmp(buf,"endwhile")) {
					if (!level || state[level-1] != IN_WHILE) {
						sprintf(rbuf,"Line %d: Unmatched 'endwhile'.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					if(loops[loop-1] != x) {
						sprintf(rbuf,"Line %d: trying to end a loop inside another loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					loops[--loop] = -1;
					code[cline].level = level-1;
					code[cline].opcode = OP_ENDWHILE;
					code[cline].label = x;
					state[level] = IN_BLOCK;
					state[--level] = IN_BLOCK;
				} else if(!str_cmp(buf,"exitwhile")) {
					if (!level || !loop) {
						sprintf(rbuf,"Line %d: 'exitwhile' used outside of for loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					// Get for name
					line = one_argument(line,buf);
					for(x = named_labels; x-- > 0;)
						if(!str_cmp(buf,labels[x])) break;

					if(x < 0) {
						sprintf(rbuf,"Line %d: undefined named label '%s' used.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					for(y = loop;y-- > 0;)
						if(loops[y] == x) break;

					if(y < 0) {
						sprintf(rbuf,"Line %d: 'exitwhile' used outside of named loop.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					code[cline].level = level;
					code[cline].opcode = OP_EXITWHILE;
					code[cline].label = x;
					state[level] = IN_BLOCK;
				} else if(!str_cmp(buf,"mob")) {
					if(type != IFC_M) {
						sprintf(rbuf,"Line %d: Attempting to do a mob command outside an mprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_MOB;
					code[cline].level = level;
					code[cline].param = mpcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid mob command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && mob_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'mob %s' requires inspection by an IMP.", rline, mob_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;
				} else if(!str_cmp(buf,"obj")) {
					if(type != IFC_O) {
						sprintf(rbuf,"Line %d: Attempting to do a obj command outside an oprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_OBJ;
					code[cline].level = level;
					code[cline].param = opcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid obj command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && obj_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'obj %s' requires inspection by an IMP.", rline, obj_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;
				} else if(!str_cmp(buf,"room")) {
					if(type != IFC_R) {
						sprintf(rbuf,"Line %d: Attempting to do a room command outside an rprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_ROOM;
					code[cline].level = level;
					code[cline].param = rpcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid room command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && room_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'room %s' requires inspection by an IMP.", rline, room_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;
				} else if(!str_cmp(buf,"token")) {
					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = (type == IFC_T) ? OP_TOKEN : OP_TOKENOTHER;
					code[cline].level = level;
					code[cline].param = tpcmd_lookup(buf,(type == IFC_T));
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid token command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect) {
						if((type == IFC_T) && token_cmd_table[code[cline].param].restricted) {
							sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'token %s' requires inspection by an IMP.", rline, token_cmd_table[code[cline].param].name);
							compile_error_show(rbuf);
							disable = TRUE;
						} else if((type != IFC_T) && tokenother_cmd_table[code[cline].param].restricted) {
							sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'token %s' requires inspection by an IMP.", rline, tokenother_cmd_table[code[cline].param].name);
							compile_error_show(rbuf);
							disable = TRUE;
						}
					}
					doquotes = FALSE;
				} else if(!str_cmp(buf,"area")) {
					if(type != IFC_A) {
						sprintf(rbuf,"Line %d: Attempting to do an area command outside an aprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_AREA;
					code[cline].level = level;
					code[cline].param = apcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid area command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && area_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'area %s' requires inspection by an IMP.", rline, area_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;

				} else if(!str_cmp(buf,"instance")) {
					if(type != IFC_I) {
						sprintf(rbuf,"Line %d: Attempting to do an instance command outside an iprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_INSTANCE;
					code[cline].level = level;
					code[cline].param = ipcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid instance command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && instance_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'instance %s' requires inspection by an IMP.", rline, instance_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;

				} else if(!str_cmp(buf,"dungeon")) {
					if(type != IFC_D) {
						sprintf(rbuf,"Line %d: Attempting to do a dungeon command outside a dprog.", rline);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					}

					line = one_argument(line,buf);
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_DUNGEON;
					code[cline].level = level;
					code[cline].param = dpcmd_lookup(buf);
					if(code[cline].param < 0) {
						sprintf(rbuf,"Line %d: Invalid dungeon command '%s'.", rline, buf);
						compile_error_show(rbuf);
						linevalid = FALSE;
						break;
					} else if(inspect && dungeon_cmd_table[code[cline].param].restricted) {
						sprintf(rbuf,"Line %d: {RWARNING:{x Use of 'dungeon %s' requires inspection by an IMP.", rline, dungeon_cmd_table[code[cline].param].name);
						compile_error_show(rbuf);
						disable = TRUE;
					}
					doquotes = FALSE;

				} else if(type == IFC_M) {
					state[level] = IN_BLOCK;
					code[cline].opcode = OP_COMMAND;
					code[cline].level = level;
					line = start;
					doquotes = FALSE;
				} else {
					sprintf(rbuf,"Line %d: Can only call interpreter commands in mprogs.", rline);
					compile_error_show(rbuf);
					linevalid = FALSE;
					break;
				}
			} while(0);

			if(linevalid && processrest) {
				code[cline].rest = compile_string(line,type,&length,doquotes);
				if(!code[cline].rest) {
					sprintf(rbuf,"Line %d: Error parsing string.", rline);
					compile_error_show(rbuf);
					linevalid = FALSE;
				} else
					code[cline].length = (short)length;
			}

			if(!linevalid) {
				code[cline].rest = NULL;
				code[cline].length = 0;
				valid = FALSE;
				++errors;
			}

			if(incline)
				++cline;
			++i;
		}
		*src = eol;
		eol = 0;

		// Skip over repeated EOL's, including blank lines
		while(*src == '\n' || *src == '\r') ++src;

	}

	if(eol) *src = eol;

	// Error happened
	if(*src || i < lines || !valid) {
		sprintf(rbuf,"%s(%d) encountered %d error%s.", type_name, script->vnum, errors, ((errors==1)?"":"s"));
		compile_error(rbuf);
		free_script_code(code,lines);
		if(fBootDb) {
			script->code = NULL;
			script->lines = 0;
		}
		return FALSE;
	}

	// Only deal with
	if(inspect) {
		// If no errors have occured, check if the script needs to be disabled.
		if(disable) {
			sprintf(rbuf,"%s(%d) disabled due to restricted commands.", type_name, script->vnum);
			compile_error_show(rbuf);
			script->flags |= SCRIPT_DISABLED;
		} else
			script->flags &= ~SCRIPT_DISABLED;
	}

	if( !cline || (code[cline-1].opcode != OP_END) ) {
		// Even empty scripts have "one" code.
		//	Only BAD scripts have "no" codes
		code[cline].opcode = OP_END;
		code[cline].level = 0;
		code[cline].rest = str_dup("");
		code[cline].length = 0;
	}

	free_script_code(script->code,script->lines);
	free_string(script->src);
	script->code = code;
	script->src = source;
	script->lines = lines+1;
	return TRUE;
}
