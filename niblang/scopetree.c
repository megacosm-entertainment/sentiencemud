#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "niblang.h"

// Scope tree
static NIB_SCOPE_NODE *__scope_tree = NULL;
static NIB_SCOPE_NODE *__current_scope = NULL;
static int __scope_id = 0;
static int __max_scope_id = 0;

static int __new_scope_id()
{
	return __scope_id++;
}

static NIB_SCOPE_NODE *__new_scope_node(NIB_SCOPE_NODE *parent)
{
	NIB_SCOPE_NODE *node = calloc(1,sizeof(NIB_SCOPE_NODE));

	if (node)
	{
		__max_scope_id = node->scope = __new_scope_id();
		node->parent = parent;
		node->head = NULL;
		node->tail = NULL;
		node->next = NULL;
		if (parent)
		{
			if (parent->head)
				parent->tail->next = node;
			else
				parent->head = node;
			parent->tail = node;
		}
	}

	return node;
}

static void __free_scopetree(NIB_SCOPE_NODE *node)
{
	if (node)
	{
		if (node->head) __free_scopetree(node->head);

		if (node->next) __free_scopetree(node->next);

		free(node);
	}
}

void nib_init_scopetree()
{
	__scope_id = 0;
	__max_scope_id = 0;
	__scope_tree = __new_scope_node(NULL);
	__current_scope = __scope_tree;
}

void nib_cleanup_scopetree()
{
	__free_scopetree(__scope_tree);
}

// Should never be the global scope
int nib_get_scope()
{
	return __current_scope ? __current_scope->scope : NIB_GLOBAL_SCOPE;
}

int nib_get_max_scope()
{
	return __max_scope_id;
}

// It should only return the GLOBAL scope if it failed
int nib_push_scope()
{
	NIB_SCOPE_NODE *node = __new_scope_node(__current_scope);

	if (node)
	{
		__current_scope = node;
		return node->scope;
	}

	return NIB_GLOBAL_SCOPE;
}

// It should only return the GLOBAL scope if it failed
int nib_pop_scope()
{
	if (__current_scope)
	{
		// Move back up the chain
		if (__current_scope->parent)
			__current_scope = __current_scope->parent;

		return __current_scope->scope;
	}

	return NIB_GLOBAL_SCOPE;
}

// Check to see if the target scope is within view of the 
bool nib_in_scope(int scope)
{
	if(scope == NIB_GLOBAL_SCOPE) return true;	// Global is always in view

	NIB_SCOPE_NODE *current = __current_scope;
	while(current && current->scope != scope)
		current = current->parent;

	return current != NULL;
}

static void __dump_scopetree_node(NIB_SCOPE_NODE *node, int indent)
{
	printf("%*.*s[%d]\n", indent, indent, " ", node->scope);

	if (node->head)
		__dump_scopetree_node(node->head, indent + 2);
	
	if (node->next)
		__dump_scopetree_node(node->next, indent);
}

void nib_dump_scopetree()
{
	printf("Scope Tree:\n");
	printf("===========\n");
	__dump_scopetree_node(__scope_tree, 0);
	printf("\n");
}
