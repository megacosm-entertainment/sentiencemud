#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "niblang.h"

static EXPRESSION_NODE *allocateExpressionNode()
{
    EXPRESSION_NODE *n = (EXPRESSION_NODE *)calloc(1,sizeof(EXPRESSION_NODE));

    if (n == NULL)
        return NULL;

	memset(n, 0, sizeof(EXPRESSION_NODE));

    n->op = expoVALUE;
	n->result = exprINTEGER;

    return n;
}

EXPRESSION_NODE *createInteger(int value)
{
	EXPRESSION_NODE *n = allocateExpressionNode();

	if (n) {
		n->op = expoVALUE;
		n->result = exprINTEGER;

		n->value.i = value;
	}

	return n;
}


EXPRESSION_NODE *createFloat(double value)
{
	EXPRESSION_NODE *n = allocateExpressionNode();

	if (n) {
		n->op = expoVALUE;
		n->result = exprFLOAT;

		n->value.d = value;
	}

	return n;
}


EXPRESSION_NODE *createBoolean(bool value)
{
	EXPRESSION_NODE *n = allocateExpressionNode();

	if (n) {
		n->op = expoVALUE;
		n->result = exprBOOLEAN;

		n->value.b = value;
	}

	return n;
}

EXPRESSION_NODE *createOperation(EXPRESSION_OPERATION op, EXPRESSION_RESULT result, EXPRESSION_NODE *left, EXPRESSION_NODE *right)
{
	EXPRESSION_NODE *n = allocateExpressionNode();

	if (n) {
		n->op = op;
		if (result == exprAUTO)
		{
			if (left)
			{
				if (right)
					n->result = (left->result > right->result) ? left->result : right->result;
				else
					n->result = left->result;
			}
			else
				n->result = exprINTEGER;
		}
		else
			n->result = result;

		n->left = left;
		n->right = right;
	}

	return n;
}


void deleteExpression(EXPRESSION_NODE *b)
{
    if (b == NULL)
        return;

    deleteExpression(b->left);
    deleteExpression(b->right);

    free(b);
}
