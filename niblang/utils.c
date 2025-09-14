#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include "niblang.h"

void ltoa(register long num, register char *output)
{
	static char number[100];

	register char *str = &number[99];

	*str = '\0';

	bool sign = false;
	if (num < 0L)
	{
		num = -num;
		sign = true;
	}

	do {
		*(--str) = (num % 10) + '0';
		num /= 10;
	} while(num > 0);

	if (sign)
	{
		*(--str) = '-';
	}

	// Place into output
	do
	{
		*output++ = *str++;
	}
	while(*str);

	*output = 0;
}

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

bool str_prefix(const char *astr, const char *bstr)
{
    if (astr == NULL)
    {
		return true;
    }

    if (bstr == NULL)
    {
		return true;
    }

	// Empty strings should *never* prefix another string
	if (!*astr) return true;

    for (; *astr; astr++, bstr++)
    {
	if (tolower(*astr) != tolower(*bstr))
	    return true;
    }

    return false;
}


bool str_infix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;
    int ichar;
    char c0;

    if ((c0 = tolower(astr[0])) == '\0')
	return true;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);

    for (ichar = 0; ichar <= sstr2 - sstr1; ichar++)
    {
	if (c0 == tolower(bstr[ichar]) && !str_prefix(astr, bstr + ichar))
	    return true;
    }

    return false;
}

bool str_suffix(const char *astr, const char *bstr)
{
    int sstr1;
    int sstr2;

    sstr1 = strlen(astr);
    sstr2 = strlen(bstr);
    if (sstr1 <= sstr2 && !str_cmp(astr, bstr + sstr2 - sstr1))
	return false;
    else
	return true;
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
			number = ((long)rand()<<32) | ((long)rand() << 1) | ((long)rand() & 1) & (power - 1);
		} while (number >= to);
	}
	else
	{
		while ((number = (rand() & (power - 1))) >= to);
	}

	return from + number;
}


unsigned long nib_allocations = 0UL;

struct ledger_s {
	struct ledger_s *prev;
	struct ledger_s *next;
	void *addr;
	size_t count;
	size_t size;
};

static struct ledger_s *ledger_head = NULL;
static struct ledger_s *ledger_tail = NULL;

static void add_ledger(void *addr, size_t count, size_t size)
{
	struct ledger_s *ledger = calloc(1, sizeof(struct ledger_s));

	ledger->addr = addr;
	ledger->count = count;
	ledger->size = size;

	if (ledger_head)
	{
		ledger_tail->next = ledger;
		ledger->prev = ledger_tail;
	}
	else
		ledger_head = ledger;

	ledger_tail = ledger;
	++nib_allocations;
}

static struct ledger_s *find_ledger(void *addr)
{
	struct ledger_s *ledger = ledger_head;

//	printf("find_ledger(%p) -> %p\n", addr, ledger);

	while(ledger)
	{
//		printf(" - %p\n", ledger);
		if (ledger->addr == addr)
			return ledger;

		ledger = ledger->next;
	}

	return NULL;
}

static void remove_ledger(struct ledger_s *ledger)
{
	if (!ledger->prev)
		ledger_head = ledger->next;
	else
		ledger->prev->next = ledger->next;
	if (!ledger->next)
		ledger_tail = ledger->prev;
	else
		ledger->next->prev = ledger->prev;

	free(ledger);
}

static void remove_address(void *addr)
{
	struct ledger_s *ledger = find_ledger(addr);
	if (ledger)
	{
		remove_ledger(ledger);
		--nib_allocations;
	}
}

void nib_ledger_cleanup()
{
	struct ledger_s *next;

	while(ledger_head)
	{
		next = ledger_head->next;
		free(ledger_head);
		ledger_head = next;
	}

	ledger_head = NULL;
	ledger_tail = NULL;
}


char *nib_strdup(const char *str)
{
	if(!str) return NULL;
	char *data = strdup(str);
	add_ledger(data, 0, strlen(data) + 1);
	return data;
}

void *nib_malloc(size_t size)
{
	void *data = malloc(size);
	add_ledger(data, 0, size);
	return data;
}

void *nib_calloc(size_t count, size_t size)
{
	void *data = calloc(count, size);
	add_ledger(data, count, size);
	return data;
}

void nib_free(void *data)
{
	if (data)
	{
		remove_address(data);
		free(data);
	}
}

void hex_dump(void *addr, size_t size)
{
	printf("      ");
	for(int j = 0; j < 16; j++)
	{
		printf(" %2X", j);
	}
	printf(" : ");
	for(int j = 0; j < 16; j++)
	{
		printf("%1.1X", j);
	}
	printf("\n");

	for(size_t i = 0; i < size; i += 16)
	{
		printf("%04X: ", i);
		for(size_t j = 0; j < 16; j++)
		{
			if ((i + j) < size)
			{
				char ch = ((char *)addr)[i + j];

				printf(" %02.2X", (unsigned char)ch);
			}
			else
			{
				printf("   ");
			}
		}

		printf(" : ");

		for(size_t j = 0; j < 16; j++)
		{
			if ((i + j) < size)
			{
				char ch = ((char *)addr)[i + j];

				if (isprint(ch))
					printf("%c", ch);
				else
					printf(".");
			}
			else
			{
				printf(" ");
			}
		}

		printf("\n");
	}
}

void nib_ledger_display()
{
	if (ledger_head)
	{
		struct ledger_s *current = ledger_head;

		printf("Outstanding Memory Ledgers:\n");
		while(current)
		{
			struct ledger_s *next = current->next;

			if (current->count > 0)
			{
				printf("%p (%lu of %lu)\n", current->addr, current->count, current->size);
				hex_dump(current->addr, current->count * current->size);
			}
			else
			{
				printf("%p (%lu)\n", current->addr, current->size);
				hex_dump(current->addr, current->size);
			}

			// Go ahead and free it
			nib_free(current->addr);

			current = next;
		}
	}
}
