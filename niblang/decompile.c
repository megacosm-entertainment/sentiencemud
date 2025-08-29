#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdint.h>

#include "niblang.h"

extern NIB_BUFFER *nib_program_storage;

static const char *opcode_names[] = {
	"--ILLEGAL--",
	"LOAD_LOCAL   ",
	"LOAD_GLOBAL  ",
	"LVALUE_LOCAL ",
	"LVALUE_GLOBAL",
	"LVALUE_FLAG  ",	// Use for situations like flag.bit = true/false;
	"RVALUE_FLAG  ",	// Use for reading the value
	"CALL_FUNCTION",
	"LOAD_STRING  ",
	"LOAD_NUMBER  ",
	"LOAD_FLOAT   ",
	"LOAD_WIDEVNUM",
	"CONST0       ",
	"CONST1       ",
	"NCONST1      ",
	"FCONST0      ",
	"DUP          ",
	"POP          ",
	"RETURN       ",
	"JUMP         ",
	"JUMP_ZERO    ",
	"JUMP_NOT_ZERO",
	"SWITCH       ",
	"INC          ",
	"DEC          ",
	"POST_INC     ",
	"POST_DEC     ",
	"PRE_INC      ",
	"PRE_DEC      ",
	"LAND         ",
	"LOR          ",
	"LXOR         ",
	"LNOT         ",
	"ASSIGN       ",
	"VOID_ASSIGN  ",
	"NEG          ",
	"ADD          ",
	"SUBT         ",
	"MULT         ",
	"MOD          ",
	"DIV          ",
	"EQ           ",
	"NEQ          ",
	"LT           ",
	"LE           ",
	"GT           ",
	"GE           ",
	"BAND         ",
	"BOR          ",
	"BXOR         ",
	"BNOT         ",
	"LSH          ",
	"RSH          ",
	"RSHL         ",
	"ADD_EQ       ",
	"SUBT_EQ      ",
	"MULT_EQ      ",
	"MOD_EQ       ",
	"DIV_EQ       ",
	"BAND_EQ      ",
	"BOR_EQ       ",
	"BXOR_EQ      ",
	"LSH_EQ       ",
	"RSH_EQ       ",
	"RSHL_EQ      ",
	"GET_AREA     ",
};

void nib_decompile_code()
{
	nib_bytecode_p pc = nib_program_storage->buffer;
	uintptr_t addr = 0;
	int address;
	long number;
	double floating;
	int string_index;

	printf("Program ML:\n");
	while(addr < nib_program_storage->len)
	{
		if (pc[addr] == NI_ILLEGAL || pc[addr] >= NI__MAX)
		{
			printf("%08X: --ILLEGAL OPCODE--\n", addr);
			return;
		}
		else
		{
			printf("%08X: %-16s", addr, opcode_names[pc[addr]]);
		}

		switch(pc[addr])
		{
		case NI_LOAD_WIDEVNUM:
		case NI_CALL_FUNCTION:
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
		case NI_GET_AREA:
			break;

		case NI_LOAD_LOCAL:
		case NI_LVALUE_LOCAL:
			{
				short id;
				memcpy(&id, &pc[addr+1], sizeof(id));
				NIB_VARIABLE *var = nib_get_local_variable_byid(id);

				if (var)
					printf(" %d (%s %s)", id, nib_get_typename(var->type), var->name);
				else
					printf(" %d --invalid--", id);

				addr += sizeof(id);
				break;
			}

		case NI_LOAD_GLOBAL:
		case NI_LVALUE_GLOBAL:
			{
				short id;
				memcpy(&id, &pc[addr+1], sizeof(id));
				NIB_VARIABLE *var = nib_get_global_variable_byid(id);

				if (var)
					printf(" %d (%s %s)", id, nib_get_typename(var->type), var->name);
				else
					printf(" %d --invalid--", id);

				addr += sizeof(id);
				break;
			}

		// Allows for assigning a flag bit with true/false
		case NI_LVALUE_FLAG:
		// Allows for reading a flag bit as true/false
		case NI_RVALUE_FLAG:
			{
				flag_value_t bit;
				memcpy(&bit, &pc[addr+1], sizeof(flag_value_t));

				printf(" <[%08X]>", bit);

				addr += sizeof(flag_value_t);
				break;
			}

		case NI_LOAD_STRING:
			{
				memcpy(&string_index, &pc[addr+1], sizeof(string_index));
				const char *str = nib_get_string(string_index);

				if (str)
					printf(" %d \"%s\"", string_index, str);
				else
					printf(" %d --invalid--", string_index);

				addr+=sizeof(string_index);
				break;
			}

		case NI_LOAD_NUMBER:
			memcpy(&number, &pc[addr+1], sizeof(number));
			printf(" %ld", number);

			addr+=sizeof(number);
			break;

		case NI_LOAD_FLOAT:
			memcpy(&floating, &pc[addr+1], sizeof(floating));
			printf(" %lf", floating);

			addr+=sizeof(floating);
			break;

		case NI_JUMP:
		case NI_JUMP_ZERO:
		case NI_JUMP_NOT_ZERO:
		case NI_LAND:
		case NI_LOR:
			memcpy(&address, &pc[addr+1], sizeof(address));
			printf(" <addr: %08X>", address);

			addr+=sizeof(address);
			break;
		}

		printf("\n");

		addr++;
	}

	printf("%08X: --End of Code--\n\n", addr);
	
}