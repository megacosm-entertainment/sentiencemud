#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "niblang.h"

// WARNING: NOT UTF8 aware!!!
int str_cmp(const char *astr, const char *bstr)
{
	int ch;
	if (astr == NULL) return -1;

	if (bstr == NULL) return 1;

	for (; *astr || *bstr; astr++, bstr++) {
		if ((ch = (tolower(*astr) - tolower(*bstr))))
			return ch;
	}

	return 0;
}


long number_range(long from, long to)
{
	long power;
	long number;

	if (from == 0 && to == 0)
		return 0;

	if ((to = to - from + 1) <= 1)
		return from;

	for (power = 2L; power < to; power <<= 1L);

	if (to > RAND_MAX)
	{
		do {
			number = ((long)random()<<32) | ((long)rand() << 1) | ((long)rand() & 1);
		} while ((number & (power - 1)) >= to);
	}
	else
	{
		while (((number = random()) & (power - 1)) >= to);
	}

	return from + number;
}
