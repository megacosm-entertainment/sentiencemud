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
