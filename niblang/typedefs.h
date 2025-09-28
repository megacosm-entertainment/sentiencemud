#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

//typedef enum { false = 0, true = 1 } bool;
typedef long flag_value_t;
typedef unsigned char nib_bytecode_t;
typedef nib_bytecode_t *nib_bytecode_p;
typedef int nib_address_t;

typedef struct nib_script_stack_s NIB_SCRIPT_STACK;
typedef enum nib_script_stack_type_e NIB_SCRIPT_STACK_TYPE;
typedef struct nib_script_stack_lvalue_s NIB_SCRIPT_LVALUE;
typedef struct nib_script_runtime_s NIB_SCRIPT_RUNTIME;
typedef struct nib_local_runtime_var_s NIB_LOCAL_RUNTIME_VAR;
typedef enum nib_switch_type_e SWITCH_TYPE;
typedef enum nib_switch_case_e CASE_TYPE;
typedef struct nib_script_switch_case_s NIB_SWITCH_CASE;
//typedef struct nib_script_argument_s NIB_SCRIPT_ARG;

typedef struct nib_type NIB_TYPE;
typedef struct nib_script_type_s NIB_SCRIPT;
typedef struct nib_script_global_variable_s NIB_GLOBAL_VAR;
typedef struct nib_script_local_variable_s NIB_LOCAL_VAR;

typedef int METHOD_FUNC(NIB_SCRIPT_RUNTIME *nsr, int argc, NIB_SCRIPT_STACK *argv, NIB_SCRIPT_STACK *output);

#endif
