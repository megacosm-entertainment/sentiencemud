#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <time.h>

#include "../merc.h"
#include "niblang.h"
#include "script.h"
#include "tables.h"

#define TOLOWER(ch)		(((ch) >= 'A' && (ch) <= 'Z')?((ch)+' '):(ch))
#define TOUPPER(ch)		(((ch) >= 'a' && (ch) <= 'z')?((ch)-' '):(ch))

// Utils for the standalone

char * const dir_name[] =
{
    "north",
	"east",
	"south",
	"west",
	"up",
	"down",
	"northeast",
	"northwest",
	"southeast",
	"southwest"
};

const int16_t rev_dir[] = {
	DIR_SOUTH,
	DIR_WEST,
	DIR_NORTH,
	DIR_EAST,
	DIR_DOWN,
	DIR_UP,
	DIR_SOUTHWEST,
	DIR_SOUTHEAST,
	DIR_NORTHWEST,
	DIR_NORTHEAST
};


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

int utf8_bytes(utf8char_t ch)
{
	if (ch > 0xFFFFFF) return 4;
	if (ch > 0xFFFF) return 3;
	if (ch > 0xFF) return 2;
	return 1;
}

// Converts the utf8char_t into a string
char *utf8_getbytes(utf8char_t ch)
{
	static char bytes[4][5];
	static int i = 0;

	if (++i > 3) i = 0;
	register char *b = &bytes[i][0];

	if (ch > 0xFFFFFF)
	{
		b[0] = (char)((ch >> 24) & 0xFF);
		b[1] = (char)((ch >> 16) & 0xFF);
		b[2] = (char)((ch >> 8) & 0xFF);
		b[3] = (char)(ch & 0xFF);
		b[4] = 0;
	}
	else if (ch > 0xFFFF)
	{
		b[0] = (char)((ch >> 16) & 0xFF);
		b[1] = (char)((ch >> 8) & 0xFF);
		b[2] = (char)(ch & 0xFF);
		b[3] = 0;
	}
	else if (ch > 0xFF)
	{
		b[0] = (char)((ch >> 8) & 0xFF);
		b[1] = (char)(ch & 0xFF);
		b[2] = 0;
	}
	else
	{
		b[0] = (char)(ch & 0xFF);
		b[1] = 0;
	}

	return b;
}

char *utf8_nextchar(const char *str)
{
	// 2-byte UTF-8
	if (((*str&0xE0) == 0xC0) &&
		((*(str+1) & 0xC0) == 0x80))
		return (char *)str+2;

	// 3-byte UTF-8
	if (((*str&0xF0) == 0xE0) &&
		((*(str+1) & 0xC0) == 0x80) &&
		((*(str+2) & 0xC0) == 0x80))
		return (char *)str+3;

	// 4-byte UTF-8
	if (((*str&0xF8) == 0xF0) &&
		((*(str+1) & 0xC0) == 0x80) &&
		((*(str+2) & 0xC0) == 0x80) &&
		((*(str+3) & 0xC0) == 0x80))
		return (char *)str+4;

	// 1-byte UTF-8
	return (char *)str+1;
}

char *utf8_skip(register const char *str, register size_t len)
{
	for(;*str && len > 0; len--, str = utf8_nextchar(str));
	return (char *)str;
}

utf8char_t utf8_getchar(const char *str)
{
	// 2-byte UTF-8
	if (((*str&0xE0) == 0xC0) &&
		((*(str+1) & 0xC0) == 0x80))
		return ((((utf8char_t)*str) & 0xFF) << 8) | ((utf8char_t)*(str+1) & 0xFF);

	// 3-byte UTF-8
	if (((*str&0xF0) == 0xE0) &&
		((*(str+1) & 0xC0) == 0x80) &&
		((*(str+2) & 0xC0) == 0x80))
		return ((((utf8char_t)*str) & 0xFF) << 16) | (((utf8char_t)*(str+1) & 0xFF) << 8) | ((utf8char_t)*(str+2) & 0xFF);

	// 4-byte UTF-8
	if (((*str&0xF8) == 0xF0) &&
		((*(str+1) & 0xC0) == 0x80) &&
		((*(str+2) & 0xC0) == 0x80) &&
		((*(str+3) & 0xC0) == 0x80))
		return ((((utf8char_t)*str) & 0xFF) << 24) | (((utf8char_t)*(str+1) & 0xFF) << 16) | (((utf8char_t)*(str+2) & 0xFF) << 8) | ((utf8char_t)*(str+3) & 0xFF);

	// 1-byte UTF-8
	return (utf8char_t)*str;
}

size_t utf8_strlen(const char *str)
{
	if (!str) return 0;

	size_t len = 0;
	while(*str)
	{
		len++;
		str = utf8_nextchar(str);
	}

	return len;
}

int utf8_str_cmp(const char *astr, const char *bstr)
{
	int ch;
	if (astr == NULL) return -1;

	if (bstr == NULL) return 1;

	for (; *astr || *bstr; astr = utf8_nextchar(astr), bstr = utf8_nextchar(bstr)) {
		utf8char_t ach = utf8_getchar(astr);
		utf8char_t bch = utf8_getchar(bstr);
		if ((ch = (TOLOWER(ach) - TOLOWER(bch))))
			return ch;
	}

	return 0;
}

// UTF8 aware
bool utf8_str_prefix(const char *astr, const char *bstr)
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

    for (; *astr; astr = utf8_nextchar(astr), bstr = utf8_nextchar(bstr))
    {
		utf8char_t ach = utf8_getchar(astr);
		utf8char_t bch = utf8_getchar(bstr);
		if (TOLOWER(ach) != TOLOWER(bch))
			return true;
    }

    return false;
}


// UTF8 aware
bool utf8_str_infix(const char *astr, const char *bstr)
{
    size_t sstr1;
    size_t sstr2;
    int ichar;
    utf8char_t c0;

	c0 = utf8_getchar(astr);
	c0 = TOLOWER(c0);
    if (!c0)
		return true;

    sstr1 = utf8_strlen(astr);
    sstr2 = utf8_strlen(bstr);

    for (ichar = 0; ichar <= (sstr2 - sstr1); ichar++, bstr = utf8_nextchar(bstr))
    {
		utf8char_t bch = utf8_getchar(bstr);
		if ((c0 == TOLOWER(bch)) &&
			!utf8_str_prefix(astr,bstr))
		    return true;
    }

    return false;
}

// UTF8 aware
bool utf8_str_suffix(const char *astr, const char *bstr)
{
    size_t sstr1;
    size_t sstr2;

    sstr1 = utf8_strlen(astr);
    sstr2 = utf8_strlen(bstr);

	if (sstr1 <= sstr2)
	{
		bstr = utf8_skip(bstr, sstr2 - sstr1);

		if (!utf8_str_cmp(astr, bstr))
			return false;
	}

	return true;
}

// Not UTF-8 Aware
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

// Not UTF-8 Aware
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

// Not UTF-8 Aware
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

// Not UTF-8 Aware
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

	if (from == 0 && to <= from)
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
	printf("HEX: %p(%ld)\n", addr, size);
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
				if (current->size == sizeof(NIB_TYPE))
				{
					NIB_TYPE *type = (NIB_TYPE *)(current->addr);

					printf("TYPE?: %s\n", nib_get_typename(NULL,type));
				}

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

static inline bool __check_utf8_bytes(utf8char_t ch)
{
	register unsigned char *bytes = (unsigned char *)(void *)(&ch);

	for(register int i = sizeof(ch); i-- > 0;bytes++)
	{
		if (*bytes == 0xC0) return false;
		if (*bytes == 0xC1) return false;
		if (*bytes >= 0xF5) return false;
	}

	return true;
}

bool utf8_isvalid(utf8char_t ch)
{
	if (ch <= 0x7F) return true;

	if (!__check_utf8_bytes(ch)) return false;	// There was an invalid byte

	if (ch <= 0xFFFF)
		return ((ch & 0xE0C0) == 0xC080);

	if (ch <= 0xFFFFFF)
		return ((ch & 0xF0C0C0) == 0xE08080);

	return ((ch & 0xF8C0C0C0) == 0xF0808080);
}

bool utf8_isprint(utf8char_t ch)
{
	if (ch <= 0x7F) return isprint(ch);

	return utf8_isvalid(ch);	// Everything else is printable?
}

const char *utf8_getnchars(const char *str, int len)
{
	static char buf[4][10000];
	static int i = 0;

	if (++i > 3) i = 0;

	char *start = &buf[i][0];
	char *cur = start;

	while(*str && len > 0)
	{
		int bytes;
		// 2-byte UTF-8
		if (((*str&0xE0) == 0xC0) &&
			((*(str+1) & 0xC0) == 0x80))
			bytes = 2;

		// 3-byte UTF-8
		else if (((*str&0xF0) == 0xE0) &&
			((*(str+1) & 0xC0) == 0x80) &&
			((*(str+2) & 0xC0) == 0x80))
			bytes = 3;

		// 4-byte UTF-8
		else if (((*str&0xF8) == 0xF0) &&
			((*(str+1) & 0xC0) == 0x80) &&
			((*(str+2) & 0xC0) == 0x80) &&
			((*(str+3) & 0xC0) == 0x80))
			bytes = 4;
		else
			bytes = 1;

		for(int i = 0; i < bytes;)
			*cur++ = *str++;
		len--;
	}
	*cur = '\0';

	return start;
}

bool utf8_isstrascii(register const char *str)
{
	if (!str) return false;	// Null pointers are not ascii.

	while(*str && ((unsigned char)*str) < 0x80)
		++str;

	return !*str;
}


char *get_affect_name(AFFECT_DATA *paf)
{
	if (!paf) return "";	

	if (paf->custom_name) return paf->custom_name;

	if (paf->token && !IS_SET(paf->token->flags, TOKEN_HIDE_NAME))
		return paf->token->pIndexData->name;

	if (IS_VALID(paf->skill))
	{
		if (!IS_NULLSTR(paf->skill->display)) return paf->skill->display;

		return paf->skill->name;
	}
	else
		return "???";
}

bool affect_equal(AFFECT_DATA *a, AFFECT_DATA *b)
{
	// Custom name affect
	if (a->custom_name)
		return a->custom_name == b->custom_name;	// Custom names are registered
	else if (b->custom_name)
		return false;

	if (a->token)
		if (b->token)
			return a->token->pIndexData == b->token->pIndexData;
		else
			return false;
	else if (b->token)
		return false;

	if (IS_VALID(a->skill) && IS_VALID(b->skill))
		return a->skill == b->skill;

	return false;
}


size_t get_array_element_size_nst(NIB_SCRIPT_STACK_TYPE nst)
{
	size_t size = sizeof(void *);
	switch(nst)
	{
	case NST_NUMBER:	size = sizeof(long); break;
	case NST_NUMBER32:	size = sizeof(int); break;
	case NST_NUMBER16:	size = sizeof(short); break;
	case NST_FLOAT:		size = sizeof(double); break;
	case NST_BOOLEAN:	size = sizeof(bool); break;
	case NST_CHAR:		size = sizeof(utf8char_t); break;
	case NST_WIDEVNUM:	size = sizeof(WNUM); break;
	case NST_TIME:		size = sizeof(time_t); break;
	case NST_DICE:		size = sizeof(DICE_DATA); break;
	// case NST_STAT:		size = sizeof(long) + sizeof(void *); break;
	// case NST_FLAG:		size = sizeof(long) + sizeof(void *); break;
	}

	return size;
}

time_t parse_time_string(char *str)
{
	int year, month, day, hour, minute, second;
	int result = sscanf(str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
	if (result == 6)
	{
		struct tm timeinfo;
		memset(&timeinfo,0,sizeof(timeinfo));

		timeinfo.tm_year = year - 1900; // Years since 1900
		timeinfo.tm_mon = month - 1;   // Months since January (0-11)
		timeinfo.tm_mday = day;
		timeinfo.tm_hour = hour;
		timeinfo.tm_min = minute;
		timeinfo.tm_sec = second;
		timeinfo.tm_isdst = -1;

		return mktime(&timeinfo);
	}
	else
		return (time_t)-1;
}

bool parse_dice(char *str, DICE_DATA *dice)
{
	int n, s, b, res;

	res = sscanf(str, "%dd%d+%d", &n,&s,&b);
	if (res == 3)
	{
		dice->number = n;
		dice->size = s;
		dice->bonus = b;
		return true;
	}

	res = sscanf(str, "%dd%d-%d", &n,&s,&b);
	if (res == 3)
	{
		dice->number = n;
		dice->size = s;
		dice->bonus = -b;
		return true;
	}

	res = sscanf(str, "%dd%d", &n,&s);
	if (res == 2)
	{
		dice->number = n;
		dice->size = s;
		dice->bonus = 0;
		return true;
	}

	return false;
}

int game_setting_lookup(const char *name)
{
	if (!utf8_isstrascii(name)) return -1;

	for(int i = 0; game_settings_table[i].name; i++)
	{
		if (!str_cmp(game_settings_table[i].name, name) && game_settings_table[i].script_access)
			return i;
	}

	return -1;
}

int game_setting_type(int index)
{
	return game_settings_table[index].type;
}
