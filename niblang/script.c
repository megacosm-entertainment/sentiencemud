#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>
#include <ctype.h>


#include "../merc.h"
#include "niblang.h"
#include "script.h"
#include "tables.h"

extern NIB_BUFFER *nib_program_storage;
extern LLIST *nib_global_variables;
extern LLIST *nib_local_variables;
extern LLIST *nib_string_storage;
extern LLIST *nib_comment_storage;
extern LLIST *nib_flag_created_tables;
extern LLIST *nib_stat_created_tables;
extern LLIST *nib_used_tables;
extern LLIST *nib_used_banks;
extern LLIST *nib_switch_blocks;

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

		// The script takes custody of these lists;
		script->flag_tables = nib_flag_created_tables;
		script->stat_tables = nib_stat_created_tables;

		nib_flag_created_tables = NULL;
		nib_stat_created_tables = NULL;

		script->n_tables = list_size(nib_used_tables);
		if (script->n_tables > 0)
		{
			script->tables = (struct flag_type **)calloc(script->n_tables,sizeof(struct flag_type *));

			if (script->tables)
			{
				ITERATOR it;
				struct flag_type *table;
				int index = 0;

				iterator_start(&it,nib_used_tables);
				while((table = (struct flag_type *)iterator_nextdata(&it)))
				{
					script->tables[index++] = table;
				}
				iterator_stop(&it);
			}
		}

		script->n_banks = list_size(nib_used_banks);
		if (script->n_banks > 0)
		{
			script->banks = (struct flag_type ***)calloc(script->n_banks, sizeof(struct flag_type **));
			
			if (script->banks)
			{
				ITERATOR it;
				struct flag_type **bank;
				int index = 0;

				iterator_start(&it, nib_used_banks);
				while((bank = (struct flag_type **)iterator_nextdata(&it)))
				{
					script->banks[index++] = bank;
				}
				iterator_stop(&it);
			}
		}

		script->n_switches = list_size(nib_switch_blocks);
		if (script->n_switches > 0)
		{
			script->switches = calloc(script->n_switches, sizeof(NIB_SWITCH));

			if (script->switches)
			{
				ITERATOR swit;
				int i = 0;
				struct nib_compile_switch_s *s;
				iterator_start(&swit,nib_switch_blocks);
				while((s = (struct nib_compile_switch_s *)iterator_nextdata(&swit)))
				{
					NIB_SWITCH *sw = &script->switches[i++];
					sw->type = s->type;
					sw->default_address = s->has_default ? s->default_address : NIB_INVALID_ADDRESS;
					sw->n_cases = list_size(s->cases);
					if (sw->n_cases > 0)
					{
						sw->cases = calloc(sw->n_cases,sizeof(NIB_SWITCH_CASE));

						if (sw->cases)
						{
							ITERATOR cit;
							int j = 0;
							struct nib_compile_switch_case_s *c;
							iterator_start(&cit,s->cases);
							while((c = (struct nib_compile_switch_case_s *)iterator_nextdata(&cit)))
							{
								NIB_SWITCH_CASE *cs = &sw->cases[j++];

								cs->type = c->type;
								cs->address = c->address;
								switch(sw->type)
								{
								case NSWT_NUMBER:
								case NSWT_STAT:
									cs->a.number = c->a.number;
									cs->b.number = c->b.number;
									break;
								case NSWT_FLOAT:
									cs->a.flt = c->a.flt;
									cs->b.flt = c->b.flt;
									break;
								case NSWT_CHAR:
									cs->a.ch = c->a.ch;
									cs->b.ch = c->b.ch;
									break;
								case NSWT_STRING:
									cs->a.str = nib_add_string_to_storage(c->a.str);
									break;
								}
							}
							iterator_stop(&cit);
						}
					}
				}
				iterator_stop(&swit);
			}
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

		if (script->tables) free(script->tables);
		if (script->banks) free(script->banks);
		list_destroy(script->flag_tables);
		list_destroy(script->stat_tables);

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
	"LVALUE_BIT",	// Use for situations like flag.bit = true/false;
	"LVALUE_BIT_BANK",
	"LVALUE_FIELD",
	"CALL_FUNCTION",
	"CALL_METHOD",
	"LOAD_NUMBER",
	"LOAD_FLOAT",
	"LOAD_CHAR",
	"LOAD_STRING",
	"LOAD_WIDEVNUM",
	"LOAD_FLAG",
	"LOAD_FLAG_TABLE",
	"LOAD_FLAG_BANK",
	"LOAD_STAT",
	"LOAD_GAME_SETTING",
	"LOAD_DICE",
	"NEW_LIST",
	"NEW_ARRAY",
	"NULL",
	"TRUE",
	"FALSE",
	"CONST0",
	"CONST1",
	"NCONST1",
	"FCONST0",
	"DUP",
	"POP",
	"POPN",
	"RETURN",
	"RETURN_BYTE",
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
	"STRINGER_START",
	"STRINGER_STOP",
	"STRINGER_NEXT",
	"INDEXER_START",
	"INDEXER_STOP",
	"INDEXER_NEXT",
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
	"STRPREFIX",
	"STRINFIX",
	"STRSUFFIX",
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
	"GET_CLASS",
	"GET_LIQUID",
	"GET_MATERIAL",
	"GET_ORG",
	"GET_RACE",
	"GET_SECTOR",
	"GET_SKILL",
	"GET_WILDS",
	"PARSE_TIME",
	"PARSE_DICE",
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
		case NST_NUMBER16:	return "int16";
		case NST_NUMBER32:	return "int32";
		case NST_STAT16:	return "stat16";
		case NST_STAT32:	return "stat32";

		case NST_BOOLEAN:	return "boolean";
		case NST_NUMBER:	return "int";
		case NST_FLOAT:		return "float";
		case NST_STRING:	return "string";
		case NST_CHAR:		return "char";
		case NST_MAP:		return "map";
		case NST_WIDEVNUM:	return "widevnum";
		case NST_TIME:		return "time";
		case NST_FLAG:		return "flag";
		case NST_FLAG_BANK:	return "flagbank";
		case NST_FLAG_BANK_S:	return "flagbank_s";
		case NST_STAT:		return "stat";
		case NST_LIST:		return "list";
		case NST_LIST_S:	return "list_s";
		case NST_ARRAY:		return "array";
		case NST_ARRAY_S:	return "array_s";
		case NST_ACCOUNT:	return "account";
		case NST_AFFECT:	return "affect";
		case NST_AREA:		return "area";
		case NST_CHANNEL:	return "channel";
		case NST_CLASS:		return "class";
		case NST_DUNGEON:	return "dungeon";
		case NST_EXIT:		return "exit";
		case NST_INSTANCE:	return "instance";
		case NST_LIQUID:	return "liquid";
		case NST_MAIL:		return "mail";
		case NST_MATERIAL:	return "material";
		case NST_MISSION:	return "mission";
		case NST_MOBILE:	return "mobile";
		case NST_NOTE:		return "note";
		case NST_OBJECT:	return "object";
		case NST_ORG:		return "org";
		case NST_QUEST:		return "quest";
		case NST_RACE:		return "race";
		case NST_RANK:		return "rank";
		case NST_REPUTATION:return "reputation";
		case NST_ROOM:		return "room";
		case NST_SHIP:		return "ship";
		case NST_SKILL:		return "skill";
		case NST_TOKEN:		return "token";
		case NST_WILDS:		return "wilds";
		case NST_WORLD:		return "world";
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
	short index;
	void *pointer;
	struct flag_type *table;
	NIB_SCRIPT_STACK_TYPE type = NST_UNKNOWN;

	printf("Program ML:\n");

	__print_comments(script, addr);
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
		case NI_LOAD_WIDEVNUM:
			type = NST_WIDEVNUM;
			break;
		case NI_LVALUE_SELF:
		case NI_NULL:
		case NI_TRUE:
		case NI_FALSE:
		case NI_CONST0:
		case NI_CONST1:
		case NI_NCONST1:
		case NI_FCONST0:
		case NI_DUP:
		case NI_POP:
		case NI_RETURN:
		case NI_INC:
		case NI_DEC:
		case NI_POST_INC:
		case NI_POST_DEC:
		case NI_PRE_INC:
		case NI_PRE_DEC:
		case NI_LAND:
		case NI_LOR:
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

		case NI_SWITCH:
		{
			memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
			memcpy(&address, &pc[addr+1], sizeof(address)); addr+=sizeof(address);

			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d <addr: %08X>", index, address);

			break;
		}

		case NI_GET_AREA:		type = NST_AREA; break;
		case NI_GET_CLASS:		type = NST_CLASS; break;
		case NI_GET_LIQUID:		type = NST_LIQUID; break;
		case NI_GET_MATERIAL:	type = NST_MATERIAL; break;
		case NI_GET_ORG:		type = NST_ORG; break;
		case NI_GET_RACE:		type = NST_RACE; break;
		case NI_GET_SECTOR:		type = NST_SECTOR; break;
		case NI_GET_SKILL:		type = NST_SKILL; break;
		case NI_GET_WILDS:		type = NST_WILDS; break;
		case NI_PARSE_TIME:		type = NST_TIME; break;
		case NI_PARSE_DICE:		type = NST_DICE; break;

		case NI_RETURN_BYTE:
		{
			nib_bytecode_t ret = pc[addr+1];

			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %02.2X", ret);

			addr++;
			break;
		}

		case NI_NEW_LIST:
		{
			NIB_SCRIPT_STACK_TYPE list_type = (NIB_SCRIPT_STACK_TYPE)pc[addr+1];

			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (new list(%s))", list_type, nst_to_type(list_type));

			addr++;
			break;
		}

		case NI_NEW_ARRAY:
		{
			NIB_SCRIPT_STACK_TYPE array_type = (NIB_SCRIPT_STACK_TYPE)pc[addr+1]; addr++;
			memcpy(&number,&pc[addr+1],sizeof(number)); addr+=sizeof(number);
			
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d %ld (new array(%s[%ld]))", array_type, number, nst_to_type(array_type), number);
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
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(script,var->type), var->name);
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
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s %s)", id, nib_get_typename(script,var->type), var->name);
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
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d (%s .%s)", id, nib_get_typename(script,field->type), field->name);
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
		case NI_LVALUE_BIT:
			{
				flag_value_t bit;
				memcpy(&bit, &pc[addr+1], sizeof(flag_value_t));

				linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X]>", bit);

				addr += sizeof(flag_value_t);
				break;
			}

			case NI_LVALUE_BIT_BANK:
				{
					int bank = (int)pc[addr+1]; addr++;
					flag_value_t bit;
					memcpy(&bit, &pc[addr+1], sizeof(flag_value_t)); addr += sizeof(flag_value_t);

					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d <[%08X]>", bank, bit);
					break;
				}

		case NI_CALL_FUNCTION:
			{
				short id;
				unsigned char args;
				memcpy(&id, &pc[addr+1], sizeof(id));		addr += sizeof(id);
				args = (unsigned char)pc[addr+1];			addr++;

				NIB_METHOD *method = nib_method_get_byid(NST_FUNCTION, id);
				if (method)
				{
					type = method->sresult;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d (%s %s)", id, args, nib_get_typename(script,method->result), method->name);
				}
				else
				{
					type = NST_UNKNOWN;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d --invalid--", id, args);
				}

				break;
			}

		case NI_CALL_METHOD:
			{
				NIB_SCRIPT_STACK_TYPE nst;
				short id;
				unsigned char args;
				nst = (NIB_SCRIPT_STACK_TYPE)pc[addr+1]; addr++;
				memcpy(&id, &pc[addr+1], sizeof(id));		addr += sizeof(id);
				args = (unsigned char)pc[addr+1];			addr++;

				NIB_METHOD *method = nib_method_get_byid(nst, id);
				if (method)
				{
					type = method->sresult;
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %d (%s %s)", id, args, nib_get_typename(script,method->result), method->name);
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
				memcpy(&index, &pc[addr+1], sizeof(index));
				const char *str = nib_get_string(index);

				if (str)
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d \"%s\"", index, str);
				else
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d --invalid--", index);

				addr+=sizeof(index);
				break;
			}

		case NI_LOAD_CHAR:
			{
				utf8char_t ch;
				memcpy(&ch,&pc[addr+1],sizeof(ch)); addr+=sizeof(ch);
				if (utf8_isprint(ch))
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " %s", utf8_getbytes(ch));
				else
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " 0x%X", ch);
			}
			break;

		case NI_LOAD_NUMBER:
			memcpy(&number, &pc[addr+1], sizeof(number));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld", number);

			addr+=sizeof(number);
			break;

		case NI_LOAD_FLAG_TABLE:
			memcpy(&number, &pc[addr+1], sizeof(number)); addr+=sizeof(number);
			memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
			if (index > 0 && index <= script->n_tables)
				table = script->tables[index - 1];
			else
				table = NULL;

			linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X@%s]>", number, nib_get_flag_table_name(script->flag_tables,table));
			break;

		case NI_LOAD_FLAG_BANK:
		{
			// #banks
			// bank index
			// bits1
			// ...
			// bitsN
			int banks = pc[addr+1]; addr++;
			memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
			
			struct flag_type **bank;
			if (index > 0 && index <= script->n_banks)
				bank = script->banks[index - 1];
			else
				bank = NULL;

			if (bank)
			{
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, %s<[[", banks, nib_get_flag_bank_name((const struct flag_type **)bank));
				for(int i = 0; i < banks; i++)
				{
					memcpy(&number, &pc[addr+1], sizeof(long)); addr+=sizeof(long);

					if (i > 0)
						linej += snprintf(line + linej, sizeof(line) - linej - 1, ",%08X", number);
					else
						linej += snprintf(line + linej, sizeof(line) - linej - 1, "%08X", number);
				}
				linej += snprintf(line + linej, sizeof(line) - linej - 1, "]]>");
			}
			else
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %d, ???", banks);


			break;
		}

		case NI_LOAD_FLAG:
			memcpy(&number, &pc[addr+1], sizeof(number));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " <[%08X]>", number);

			addr+=sizeof(number);
			break;

		case NI_LOAD_STAT:
			memcpy(&number, &pc[addr+1], sizeof(number)); addr+=sizeof(number);
			memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
			if (index > 0 && index <= script->n_tables)
				table = script->tables[index - 1];
			else
				table = NULL;
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld@%s", number, nib_get_stat_table_name(script->stat_tables,table));
			break;
		
		case NI_LOAD_GAME_SETTING:
		{
			memcpy(&index, &pc[addr+1], sizeof(index)); addr+=sizeof(index);
			const struct game_setting_type *setting = &game_settings_table[index];
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %s", setting->name);
			break;
		}

		case NI_LOAD_DICE:
		{
			int n,s,b;
			memcpy(&n,&pc[addr+1],sizeof(int)); addr+=sizeof(int);
			memcpy(&s,&pc[addr+1],sizeof(int)); addr+=sizeof(int);
			memcpy(&b,&pc[addr+1],sizeof(int)); addr+=sizeof(int);
			if (b > 0)
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %dd%d+%d", n,s,b);
			else if (b < 0)
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %dd%d%d", n,s,b);
			else
				linej += snprintf(line + linej, sizeof(line) - linej - 1, " %dd%d", n,s);
			break;
		}

		case NI_LOAD_FLOAT:
			memcpy(&floating, &pc[addr+1], sizeof(floating));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " %lf", floating);

			addr+=sizeof(floating);
			break;

		case NI_JUMP:
		case NI_JUMP_ZERO:
		case NI_JUMP_NOT_ZERO:
			memcpy(&address, &pc[addr+1], sizeof(address));
			linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X>", address);

			addr+=sizeof(address);
			break;

		// case NI_INDEXER_START:
		// 	{
		// 		memcpy(&number, &pc[addr+1], sizeof(number)); addr+= sizeof(number);

		// 		linej += snprintf(line + linej, sizeof(line) - linej - 1, " %ld", number);
		// 		break;
		// 	}

		case NI_ITER_NEXT:
		case NI_STRINGER_NEXT:
		case NI_INDEXER_NEXT:
			{
				short id;
				memcpy(&address, &pc[addr+1], sizeof(address));	addr+=sizeof(address);
				memcpy(&id, &pc[addr+1], sizeof(id)); addr+= sizeof(id);

				NIB_VARIABLE *var = nib_get_local_variable_byid(id);

				if (var)
				{
					linej += snprintf(line + linej, sizeof(line) - linej - 1, " <addr: %08X> %d (%s %s)", address, id, nib_get_typename(script,var->type), var->name);
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

void nib_dump_script_tables(NIB_SCRIPT *script)
{
	ITERATOR it;
	struct flag_type_lookup *lookup;

	if (list_size(script->flag_tables) > 0)
	{
		printf("Flag Tables:\n");
		iterator_start(&it, script->flag_tables);
		while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
		{
			printf("- %s\n", lookup->name);
		}
		iterator_stop(&it);
		printf("\n");
	}

	if (list_size(script->stat_tables) > 0)
	{
		printf("Stat Tables:\n");
		iterator_start(&it, script->stat_tables);
		while((lookup = (struct flag_type_lookup *)iterator_nextdata(&it)))
		{
			printf("- %s\n", lookup->name);
		}
		iterator_stop(&it);
		printf("\n");
	}
}

void nib_dump_script_global_variables(NIB_SCRIPT *script)
{
	if (script)
	{
		if (script->n_globals > 0 && script->globals)
		{
			printf("Global Variables:\n");
			for(int i = 0; i < script->n_globals; i++)
			{
				pVARIABLE var = script->globals[i].var;

				const char *type = variable_get_typename(var);

				char left[81];
				int len = snprintf(left,sizeof(left)-1, "%s %s", type, var->name);

				char buf[321];	// Account for 80 UTF-8 characters

				len = 80 - len;
				variable_get_string(var,buf,sizeof(buf)-1,len);

				int vlen = utf8_strlen(buf);

				if (vlen < len)
					printf("%s %*.*s%s\n", left, len - vlen, len - vlen, "", buf);
				else
					printf("%s %s\n", left, buf);
			}
		}
	}
}
