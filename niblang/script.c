#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>
#include <ctype.h>


#include "niblang.h"
#include "script.h"

extern NIB_BUFFER *nib_program_storage;
extern LLIST *nib_global_variables;
extern LLIST *nib_local_variables;
extern LLIST *nib_string_storage;
extern LLIST *nib_comment_storage;

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type);

NIB_SCRIPT *new_nib_script(const char *src, NIB_SCRIPT_CLASS sc)
{
	NIB_SCRIPT *script = calloc(1, sizeof(NIB_SCRIPT));

	if (script)
	{
		script->script_class = sc;
		script->src = strdup(src);

		script->comments = list_copy(nib_comment_storage);

		if (nib_program_storage->len > 0)
		{
			script->code_len = nib_program_storage->len;
			script->code = malloc(nib_program_storage->len);
			if (script->code)
				memcpy(script->code, nib_program_storage->buffer, nib_program_storage->len);
		}

		script->n_globals = list_size(nib_global_variables);
		if (script->n_globals > 0)
		{
			script->globals = calloc(script->n_globals, sizeof(NIB_GLOBAL_VAR));

			if (script->globals)
			{
				ITERATOR it;
				NIB_VARIABLE *var;
				int index = 0;

				iterator_start(&it, nib_global_variables);
				while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
				{
					script->globals[index].type = nib_type_copy(var->type);
					script->globals[index].stype = convert_to_stype(var->type);
					script->globals[index].var = variable_get(var->name);

					++index;
				}
				iterator_stop(&it);
			}
		}

		script->n_locals = list_size(nib_local_variables);
		if (script->n_locals > 0)
		{
			script->locals = calloc(script->n_locals, sizeof(NIB_LOCAL_VAR));

			if (script->locals)
			{
				ITERATOR it;
				NIB_VARIABLE *var;
				int index = 0;

				iterator_start(&it, nib_local_variables);
				while((var = (NIB_VARIABLE *)iterator_nextdata(&it)))
				{
					script->locals[index].name = strdup(var->name);
					script->locals[index].type = nib_type_copy(var->type);
					script->locals[index].stype = convert_to_stype(var->type);
					script->locals[index].constant = var->constant;

					++index;
				}
				iterator_stop(&it);
			}
		}

		script->n_strings = list_size(nib_string_storage);
		if (script->n_strings > 0)
		{
			script->strings = (char **)calloc(script->n_strings, sizeof(char *));

			if (script->strings)
			{
				ITERATOR it;
				char *str;
				int index = 0;

				iterator_start(&it, nib_string_storage);
				while((str = (char *)iterator_nextdata(&it)))
				{
					script->strings[index++] = strdup(str);
				}
				iterator_stop(&it);
			}
		}

	}

	return script;
}

void free_nib_script(NIB_SCRIPT *script)
{
	if (script)
	{
		if (script->src) free(script->src);
		if (script->code) free(script->code);
		list_destroy(script->comments);

		if (script->globals)
		{
			for(int i = script->n_globals; i-- > 0;)
			{
				if (script->globals[i].type) free_nib_type(script->globals[i].type);
			}

			free(script->globals);
		}

		if (script->locals)
		{
			for(int i = script->n_locals; i-- > 0;)
			{
				if (script->locals[i].name) free(script->locals[i].name);
				if (script->locals[i].type) free_nib_type(script->locals[i].type);
			}

			free(script->locals);
		}

		if (script->strings)
		{
			for(int i = script->n_strings; i-- > 0;)
			{
				if (script->strings[i])
					free(script->strings[i]);
			}

			free(script->strings);
		}

		free(script);
	}
}


static const char *opcode_names[] = {
	"--ILLEGAL--",
	"LVALUE_LOCAL",
	"LVALUE_GLOBAL",
	"LVALUE_SELF",
	"LVALUE_FLAG",	// Use for situations like flag.bit = true/false;
	"LVALUE_FIELD",
	"CALL_FUNCTION",
	"CALL_METHOD",
	"LOAD_NUMBER",
	"LOAD_FLOAT",
	"LOAD_CHAR",
	"LOAD_STRING",
	"LOAD_WIDEVNUM",
	"NEW_LIST",
	"CONST0",
	"CONST1",
	"NCONST1",
	"FCONST0",
	"DUP",
	"POP",
	"POPN",
	"RETURN",
	"JUMP",
	"JUMP_ZERO",
	"JUMP_NOT_ZERO",
	"SWITCH",
	"INC",
	"DEC",
	"POST_INC",
	"POST_DEC",
	"PRE_INC",
	"PRE_DEC",
	"ITER_START",
	"ITER_STOP",
	"ITER_NEXT",
	"LAND",
	"LOR",
	"LXOR",
	"LNOT",
	"ASSIGN",
	"VOID_ASSIGN",
	"NEG",
	"ADD",
	"SUBT",
	"MULT",
	"MOD",
	"DIV",
	"EQ",
	"NEQ",
	"LT",
	"LE",
	"GT",
	"GE",
	"BAND",
	"BOR",
	"BXOR",
	"BNOT",
	"LSH",
	"RSH",
	"RSHL",
	"ADD_EQ",
	"VOID_ADD_EQ",
	"SUBT_EQ",
	"MULT_EQ",
	"MOD_EQ",
	"DIV_EQ",
	"BAND_EQ",
	"BOR_EQ",
	"BXOR_EQ",
	"LSH_EQ",
	"RSH_EQ",
	"RSHL_EQ",
	"GET_AREA",
};

static void __print_comments(NIB_SCRIPT *script, long address)
{
	ITERATOR it;
	NIB_SCRIPT_COMMENT *comment;

	bool first = true;
	iterator_start(&it, script->comments);
	while((comment = (NIB_SCRIPT_COMMENT *)iterator_nextdata(&it)))
	{
		if (comment->address == address)
		{
			if (first) {
				printf("\n%s\n", comment->comment);
				first = false;
			}
			else
				printf("%s\n", comment->comment);
		}
	}
	iterator_stop(&it);
}

static const char *nst_to_type(NIB_SCRIPT_STACK_TYPE type)
{
	switch(type)
	{
		case NST_BOOLEAN:	return "boolean";
		case NST_NUMBER:	return "number";
		case NST_FLOAT:		return "float";
		case NST_STRING:	return "string";
		case NST_CHAR:		return "char";
		case NST_MAP:		return "map";
		case NST_WIDEVNUM:	return "widevnum";
		case NST_FLAG:		return "flag";
		case NST_STAT:		return "stat";
		case NST_LIST:		return "list";
		case NST_AREA:		return "area";
		case NST_DUNGEON:	return "dungeon";
		case NST_INSTANCE:	return "instance";
		case NST_MOBILE:	return "mobile";
		case NST_OBJECT:	return "object";
		case NST_QUEST:		return "quest";
		case NST_ROOM:		return "room";
		case NST_SHIP:		return "ship";
		case NST_TOKEN:		return "token";
	}

	return "invalid";
}

void nib_decompile_code(NIB_SCRIPT *script)
{
	if (!script || !script->code) return;

	nib_bytecode_p pc = script->code;
	uintptr_t addr = 0;
	int address;
	long number;
	char ch;
	double floating;
	int string_index;
	NIB_SCRIPT_STACK_TYPE type = NST_UNKNOWN;

	__print_comments(script, addr);

	printf("Program ML:\n");
	while(addr < script->code_len)
	{
		char line[1000];
		int linej = 0;

		uintptr_t start_addr = addr;

		if (pc[addr] == NI_ILLEGAL || pc[addr] >= NI__MAX)
		{
			printf("%08X: --ILLEGAL OPCODE--\n", addr);
			return;
		}
		else
		{
			linej = snprintf(line, sizeof(line) - 1, "%08X: %-16s", addr, opcode_names[pc[addr]]);
		}

		switch(pc[addr])
		{
		case NI_LVALUE_SELF:
		case NI_LOAD_WIDEVNUM:
		case NI_CONST0:
		case NI_CONST1:
		case NI_NCONST1:
		case NI_FCONST0:
		case NI_DUP:
		case NI_POP:
		case NI_RETURN:
		case NI_SWITCH:
		case NI_INC:
		case NI_DEC:
		case NI_POST_INC:
		case NI_POST_DEC:
		case NI_PRE_INC:
		case NI_PRE_DEC:
		case NI_LXOR:
		case NI_LNOT:
		case NI_ASSIGN:
		case NI_VOID_ASSIGN:
		case NI_NEG:
		case NI_ADD:
		case NI_SUBT:
		case NI_MULT:
		case NI_MOD:
		case NI_DIV:
		case NI_EQ:
		case NI_NEQ:
		case NI_LT:
		case NI_LE:
		case NI_GT:
		case NI_GE:
		case NI_BAND:
		case NI_BOR:
		case NI_BXOR:
		case NI_BNOT:
		case NI_LSH:
		case NI_RSH:
		case NI_RSHL:
		case NI_ADD_EQ:
		case NI_VOID_ADD_EQ:
		case NI_SUBT_EQ:
		case NI_MULT_EQ:
		case NI_MOD_EQ:
		case NI_DIV_EQ:
		case NI_BAND_EQ:
		case NI_BOR_EQ:
		case NI_BXOR_EQ:
		case NI_LSH_EQ:
		case NI_RSH_EQ:
		case NI_RSHL_EQ:
			break;

		case NI_GET_AREA:
			type = NST_AREA;
			break;

		case NI_NEW_LIST:
		{
			NIB_SCRIPT_STACK_TYPE list_type = (NIB_SCRIPT_STACK_TYPE)pc[addr+1];

			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (new list(%s))", list_type, nst_to_type(list_type));

			addr++;
			break;
		}

		case NI_POPN:
			{
				short n;
				memcpy(&n, &pc[addr+1], sizeof(n));
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d", n);

				addr+=sizeof(n);
				break;
			}

		case NI_LVALUE_LOCAL:
			{
				short id;
				memcpy(&id, &pc[addr+1], sizeof(id));
				NIB_VARIABLE *var = nib_get_local_variable_byid(id);

				if (var)
				{
					type = var->stype;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(var->type), var->name);
				}
				else
				{
					type = NST_UNKNOWN;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
				}

				addr += sizeof(id);
				break;
			}

		case NI_LVALUE_GLOBAL:
			{
				short id;
				memcpy(&id, &pc[addr+1], sizeof(id));
				NIB_VARIABLE *var = nib_get_global_variable_byid(id);

				if (var)
				{
					type = var->stype;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(var->type), var->name);
				}
				else
				{
					type = NST_UNKNOWN;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
				}

				addr += sizeof(id);
				break;
			}

		case NI_LVALUE_FIELD:
			{
				short id;
				memcpy(&id, &pc[addr+1], sizeof(id));

				NIB_FIELD *field = nib_field_get_byid(type, id);
				if (field)
				{
					type = field->stype;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s .%s)", id, nib_get_typename(field->type), field->name);
				}
				else
				{
					type = NST_UNKNOWN;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", id);
				}

				addr += sizeof(id);
				break;
			}

		// Allows for access a flag bit
		case NI_LVALUE_FLAG:
			{
				flag_value_t bit;
				memcpy(&bit, &pc[addr+1], sizeof(flag_value_t));

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X]>", bit);

				addr += sizeof(flag_value_t);
				break;
			}

		case NI_CALL_FUNCTION:
			type = NST_FUNCTION;

		case NI_CALL_METHOD:
			{
				short id;
				unsigned char args;
				memcpy(&id, &pc[addr+1], sizeof(id));		addr += sizeof(id);
				args = (unsigned char)pc[addr+1];			addr++;

				NIB_METHOD *method = nib_method_get_byid(type, id);
				if (method)
				{
					type = method->sresult;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d (%s %s)", id, args, nib_get_typename(method->result), method->name);
				}
				else
				{
					type = NST_UNKNOWN;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d --invalid--", id, args);
				}

				break;
			}

		case NI_LOAD_STRING:
			{
				memcpy(&string_index, &pc[addr+1], sizeof(string_index));
				const char *str = nib_get_string(string_index);

				if (str)
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d \"%s\"", string_index, str);
				else
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", string_index);

				addr+=sizeof(string_index);
				break;
			}

		case NI_LOAD_CHAR:
			ch = pc[addr+1];
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %c (%02X)", (isprint(ch) ? ch : '.'), (unsigned char)ch);
			addr++;
			break;

		case NI_LOAD_NUMBER:
			memcpy(&number, &pc[addr+1], sizeof(number));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld", number);

			addr+=sizeof(number);
			break;

		case NI_LOAD_FLOAT:
			memcpy(&floating, &pc[addr+1], sizeof(floating));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %lf", floating);

			addr+=sizeof(floating);
			break;

		case NI_JUMP:
		case NI_JUMP_ZERO:
		case NI_JUMP_NOT_ZERO:
		case NI_LAND:
		case NI_LOR:
			memcpy(&address, &pc[addr+1], sizeof(address));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X>", address);

			addr+=sizeof(address);
			break;

		case NI_ITER_NEXT:
			{
				short id;
				memcpy(&address, &pc[addr+1], sizeof(address));	addr+=sizeof(address);
				memcpy(&id, &pc[addr+1], sizeof(id)); addr+= sizeof(id);

				NIB_VARIABLE *var = nib_get_local_variable_byid(id);

				if (var)
				{
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X> %d (%s %s)", address, id, nib_get_typename(var->type), var->name);
				}
				else
				{
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X> %d --invalid--", address, id);
				}
				break;
			}
		}
		addr++;

		line[linej] = 0;

		char code[1000];
		int codej = 0;

		while(start_addr < addr)
		{
			codej += snprintf(code + codej, sizeof(code) - codej - 1, " %02.2X", (unsigned char)pc[start_addr++]);
		}

		code[codej] = 0;

		printf("%-60.60s ;%s\n", line, code);

		__print_comments(script, addr);
	}

	printf("%08X: --End of Code--\n\n", addr);
}
