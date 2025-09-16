%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "../niblang.h"

#define YYSTYPE NIBSTYPE
#define YYLTYPE NIBLTYPE
#include "parser.h"

//extern NIBLTYPE yylloc;

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno;

static inline bool is_hex_digit(const char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}


static utf8char_t encode_utf8char(int code)
{
	// U0 to U7F
	// 0yyyzzzz -> 0yyyzzzz
	if (code <= 0x7F) return (utf8char_t)code;	// Single byte

	// U80 to U7FF
	// xxxyyyyzzzz -> 110xxxyy 10yyzzzz
	if (code <= 0x7FF)
		return (utf8char_t)(0xC080 | ((code & 0x7C0) << 2) | (code & 0x3F));

	// U800 to UFFFF
	// wwwwxxxxyyyyzzzz -> 1110wwww 10xxxxyy 10yyzzzz
	if (code <= 0xFFFF)
		return (utf8char_t)(0xE08080 | ((code & 0xF000) << 4) | ((code & 0xFC0) << 2) | (code & 0x3F));

	// U10000 to U10FFFF
	// uvvvvwwwwxxxxyyyyzzzz -> 11110uvv 10vvwwww 10xxxxyy 10yyzzzz
	if (code <= 0x10FFFF)
		return (utf8char_t)(0xF0808080 | ((code & 0x1C0000) << 6) | ((code & 0x3F000) << 4) | ((code & 0xFC0) << 2) | (code & 0x3F));

	// Not a valid code
	return -1;
}

static char *put_encode_utf8char(char *dest, int code)
{
	// Code assumed to be [0, 0x10FFFF]

	// U0 to U7F
	// 0yyyzzzz -> 0yyyzzzz
	if (code <= 0x7F)
		*dest++ = (char)(code & 0x7F);

	// U80 to U7FF
	// xxxyyyyzzzz -> 110xxxyy 10yyzzzz
	else if (code <= 0x7FF)
	{
		*dest++ = (char)(0xC0 | ((code & 0x7C0) >> 6));
		*dest++ = (char)(0x80 | (code & 0x3F));
	}

	// U800 to UFFFF
	// wwwwxxxxyyyyzzzz -> 1110wwww 10xxxxyy 10yyzzzz
	else if (code <= 0xFFFF)
	{
		*dest++ = (char)(0xE0 | ((code & 0xF000) >> 12));
		*dest++ = (char)(0x80 | ((code & 0xFC0) >> 6));
		*dest++ = (char)(0x80 | (code & 0x3F));
	}
	
	// U10000 to U10FFFF
	// uvvvvwwwwxxxxyyyyzzzz -> 11110uvv 10vvwwww 10xxxxyy 10yyzzzz
	else
	{
		*dest++ = (char)(0xF0 | ((code & 0x1C0000) >> 18));
		*dest++ = (char)(0x80 | ((code & 0x3F000) >> 12));
		*dest++ = (char)(0x80 | ((code & 0xFC0) >> 6));
		*dest++ = (char)(0x80 | (code & 0x3F));
	}

	return dest;
}


static char *trim_outer_quotes(const char *src)
{
	int len = 0;
	// Calculate length of string, being escaped characters aware
	for(const char *p = src + 1; *p && *p != '\"'; p++)
	{
		if (*p == '\\')
		{
			++p;
			if (*p == 'X' || *p == 'x')	// Hex codes can be up to 8 digits, must pass as a valid UTF-8 encoding
			{
				++p;
				// Hex code, limit to 8 digits
				utf8char_t value = 0;
				int digits;
				for(digits = 0; is_hex_digit(*p) && digits < 8; digits++, p++)
				{
					if (*p >= '0' && *p <= '9')
						value = (value << 4) + (*p - '0');
					else if (*p >= 'A' && *p <= 'F')
						value = (value << 4) + (*p - 'A' + 10);
					else if (*p >= 'a' && *p <= 'f')
						value = (value << 4) + (*p - 'a' + 10);
				}

				// Either no digits or more than 2 and not even
				// Because I will let you do \x4.. but I will not let you do \x104
				if (digits == 0 || (digits > 2 && (digits & 1)))
					return NULL;	// Error in parsing

				// Not a valid UTF-8 encoded character
				if (!utf8_isvalid(value))
					return NULL;

				len += (digits + 1)/2;	// Every two digits is a byte that gets encoded into the string
			}
			else if (*p == 'U' || *p == 'u')	// UTF-8 codes can be up to 6 digits, must be a valid UTF-8 code (0 to 10FFFF)
			{
				++p;
				// Hex code, limit to 8 digits
				int value = 0;
				int digits;
				for(digits = 0; is_hex_digit(*p) && digits < 6; digits++, p++)
				{
					if (*p >= '0' && *p <= '9')
						value = (value << 4) + (*p - '0');
					else if (*p >= 'A' && *p <= 'F')
						value = (value << 4) + (*p - 'A' + 10);
					else if (*p >= 'a' && *p <= 'f')
						value = (value << 4) + (*p - 'a' + 10);
				}

				// No digits or invalid code
				if (digits == 0 || value > 0x10FFFF)
					return NULL;	// Error in parsing

				utf8char_t ch = encode_utf8char(value);

				// Skip according to how big the code is
				if (ch <= 0xFF)
					len++;
				else if (ch <= 0xFFFF)
					len+=2;
				else if (ch <= 0xFFFFFF)
					len+=3;
				else
					len+=4;
			}
			else if (*p == '0')	// Octal escapes only amount to one byte
			{
				++p;
				int digits;
				for(digits = 0;*p >= '0' && *p < '7' && digits < 3; digits++, p++);

				if (digits == 0)
					return NULL;
				len++;
			}
			else if (!*p)	// Reached the end of the string without escaping something
				return NULL;
			else	// Everything else is a direct escape into a single character, even the ones that do escape
				len++;
		}
		else
			len++;
	}

	char *dest = nib_malloc(len + 1);
	if (!dest) return NULL;

	// Now copy over into copy
	char *d = dest;
	for(const char *p = src + 1; *p && *p != '\"'; p++)	
	{
		if (*p == '\\')
		{
			++p;
			if (*p == 'X' || *p == 'x')
			{
				++p;
				utf8char_t value = 0;
				int digits = 0;
				for(;*p && digits < 8 && is_hex_digit(*p);digits++,p++)
				{
					if (*p >= '0' && *p <= '9')
						value = (value << 4) + (*p - '0');
					else if (*p >= 'A' && *p <= 'F')
						value = (value << 4) + (*p - 'A' + 10);
					else if (*p >= 'a' && *p <= 'f')
						value = (value << 4) + (*p - 'a' + 10);
				}

				if (digits > 0)
				{
					// 1 -> 0 & ~1 -> 0
					// 2 -> 1 & ~1 -> 0
					// 3 -> 2 & ~1 -> 2
					// 4 -> 3 & ~1 -> 2
					// 5 -> 4 & ~1 -> 4
					// 6 -> 5 & ~1 -> 4
					// 7 -> 6 & ~1 -> 6
					// 8 -> 7 & ~1 -> 6

					for(int i = (digits - 1) & ~1;i > 0; i -= 2)
					{
						*d++ = (value >> (i * 4)) & 0xFF;
					}
					*d++ = value & 0xFF;
				}
			}
			else if (*p == 'U' || *p == 'u')
			{
				++p;
				int value = 0;
				int digits = 0;
				for(;*p && digits < 8 && is_hex_digit(*p);digits++,p++)
				{
					if (*p >= '0' && *p <= '9')
						value = (value << 4) + (*p - '0');
					else if (*p >= 'A' && *p <= 'F')
						value = (value << 4) + (*p - 'A' + 10);
					else if (*p >= 'a' && *p <= 'f')
						value = (value << 4) + (*p - 'a' + 10);
				}

				d = put_encode_utf8char(d, value);
			}
			else if (*p == '0')
			{
				++p;
				int value = 0;
				for(int digits = 0;*p >= '0' && *p < '7' && digits < 3; digits++, p++)
				{
					value = (value << 3) + (*p - '0');
				}

				*d++ = (char)(value & 0xFF);
			}
			else
			{
				switch(*p)
				{
				case '0':	*d++ = '\0'; break;
				case 'a':	*d++ = '\a'; break;
				case 'b':	*d++ = '\b'; break;
				case 'e':	*d++ = '\e'; break;
				case 'n':	*d++ = '\n'; break;
				case 'r':	*d++ = '\r'; break;
				case 't':	*d++ = '\t'; break;
				default:	*d++ = *p; break;
				}
			}
		}
		else
			*d++ = *p;
	}
	*d = '\0';

	return dest;
}

static utf8char_t convert_to_utf8char(char *str, int bytes)
{
	utf8char_t value = 0;
	for(int i = 0; i < bytes; i++)
		value = (value << 8) + (utf8char_t)(unsigned char)str[i];

	return value;
}

static utf8char_t parse_escape_character(char *str)
{
	// Assume '\#' where # is the escape character we want to return
	switch(str[2])
	{
		case '0':	return (utf8char_t)'\0';
		case 'a':	return (utf8char_t)'\a';
		case 'b':	return (utf8char_t)'\b';
		case 'e':	return (utf8char_t)'\e';
		case 'n':	return (utf8char_t)'\n';
		case 'r':	return (utf8char_t)'\r';
		case 't':	return (utf8char_t)'\t';
	}

	return (utf8char_t)str[2];
}

static utf8char_t parse_octal_escape_character(char *str)
{
    // Assume '\[0-7]+'

    utf8char_t ch = 0;

    for(int i = 2; str[i] && str[i] != '\''; i++)
    {
        if (str[i] >= '0' && str[i] <= '7')
            ch = (ch << 3) + (str[i] - '0');
        else
            return -1;
    }

    return ch;
}

static utf8char_t parse_hex_escape_character(char *str)
{
	// Assume '\x[0-9A-Fa-f]+'

	utf8char_t ch = 0;

	int i;
	for(i = 3; i < 11 && str[i] && str[i] != '\''; i++)
	{
		if (str[i] >= '0' && str[i] <= '9')
			ch = (ch << 4) + (str[i] - '0');
		else if (str[i] >= 'A' && str[i] <= 'F')
			ch = (ch << 4) + (str[i] - 'A' + 10);
		else if (str[i] >= 'a' && str[i] <= 'f')
			ch = (ch << 4) + (str[i] - 'a' + 10);
		else
			return -1;
	}

	// Too many digits for a valid code, can only allow up to 8 digits
	if(str[i] != '\'')
		return -1;

	return ch;
}

static utf8char_t parse_utf8_escape_character(char *str)
{
	// Assume '\u[0-9A-Fa-f]+'
	int code = 0;

	int i;
	for(i = 3; i < 9 && str[i] && str[i] != '\''; i++)
	{
		if (str[i] >= '0' && str[i] <= '9')
			code = (code << 4) + (str[i] - '0');
		else if (str[i] >= 'A' && str[i] <= 'F')
			code = (code << 4) + (str[i] - 'A' + 10);
		else if (str[i] >= 'a' && str[i] <= 'f')
			code = (code << 4) + (str[i] - 'a' + 10);
		else
			return (utf8char_t)-1;
	}

	// Too many digits for a valid code, can only allow up to 6 digits
	if(str[i] != '\'')
		return (utf8char_t)-1;

	if (code > 0x10FFFF)
		return (utf8char_t)-1;

	return encode_utf8char(code);
}

%}

%option prefix="nib"
%option outfile="yacc/lexer.c" header-file="yacc/lexer.h"
%option warn
%option yylineno

%option noyywrap
%option bison-locations
%option bison-bridge

%x ML_COMMENT
%x FIELD_NAME
%x FLAG_NAMES

/* UTF-8 continuation byte */
UB [\200-\277]

%%

<INITIAL>"/*"			    { BEGIN(ML_COMMENT); /* ML comments */ }
<ML_COMMENT>"*/"		    { BEGIN(INITIAL); }
<ML_COMMENT>.|\n		    ;	// Ignore stuff inside comment
<ML_COMMENT><<EOF>>		    { /* Complain about unterminated comment */ }
"//"[^\n]*					;	// Ignore EOL comments

[0-9]+\.[0-9]+			{ yylval->float_number = atof(nibtext); return T_FLOAT_NUMBER; }
0x[0-9a-fA-F]+			{ yylval->number = strtol(nibtext+2,NULL,16); return T_NUMBER;}
[0-9]+					{ yylval->number = atol(nibtext); return T_NUMBER; }
'.'						{ yylval->ch = convert_to_utf8char(nibtext, 1); return T_CHAR_LITERAL; }
'[\300-\337]{UB}'		{ yylval->ch = convert_to_utf8char(nibtext, 2); return T_CHAR_LITERAL; }
'[\340-\357]{UB}{2}'	{ yylval->ch = convert_to_utf8char(nibtext, 3); return T_CHAR_LITERAL; }
'[\360-\367]{UB}{3}'	{ yylval->ch = convert_to_utf8char(nibtext, 4); return T_CHAR_LITERAL; }
'\\.'					{ yylval->ch = parse_escape_character(nibtext); return T_CHAR_LITERAL; }
'(\\[Uu][0-9a-fA-F]+)'		{
		utf8char_t ch = parse_utf8_escape_character(nibtext);
		if (ch != (utf8char_t)-1)
		{
			yylval->ch = ch;
			return T_CHAR_LITERAL;
		}
		else
			return T_PARSE_ERROR;
	}
'(\\[Xx][0-9a-fA-F]+)'		{
		utf8char_t ch = parse_hex_escape_character(nibtext);
		if (ch != (utf8char_t)-1)
		{
			yylval->ch = ch;
			return T_CHAR_LITERAL;
		}
		else
			return T_PARSE_ERROR;
	}
'(\\0[0-7]+)'		{
		utf8char_t ch = parse_octal_escape_character(nibtext);
		if (ch != (utf8char_t)-1)
		{
			yylval->ch = ch;
			return T_CHAR_LITERAL;
		}
		else
			return T_PARSE_ERROR;
	}
\"(\\[Xx][0-9a-fA-F]+|\\[Uu][0-9a-fA-F]+|\\0[0-7]+|\\.|[^"\\])*\"		{
		char *str = trim_outer_quotes(nibtext);
		if (!str) return T_PARSE_ERROR;
		yylval->literal = str;
		return T_STRING_LITERAL;
	}
"area"					{ return T_AREA; }
"boolean"				{ return T_BOOLEAN; }
"bool"					{ return T_BOOLEAN; }	// Shorthand for BOOLEAN
"break"					{ return T_BREAK; }
"case"					{ return T_CASE; }
"char"					{ return T_CHAR; }
"constant"				{ return T_CONSTANT; }
"continue"				{ return T_CONTINUE; }
"default"				{ return T_DEFAULT; }
"do"					{ return T_DO; }
"dungeon"				{ return T_DUNGEON; }
"else"					{ return T_ELSE; }
"false"                 { return T_FALSE; }
"flag"					{ return T_FLAG; }
"float"					{ return T_FLOAT; }
"foreach"				{ return T_FOREACH; }
"for"					{ return T_FOR; }
"global"                { return T_GLOBAL; }
"if"					{ return T_IF; }
"infinity"				{ yylval->float_number = INFINITY; return T_FLOAT_NUMBER; }
"instance"				{ return T_INSTANCE; }
"int"					{ return T_INT; }
"list"					{ return T_LIST; }
"map"                   { return T_MAP; }
"mobile"				{ return T_MOBILE; }
"nan"					{ yylval->float_number = nan(""); return T_FLOAT_NUMBER; }
"null"                  { return T_NULL; }
"object"				{ return T_OBJECT; }
"pi"					{ yylval->float_number = M_PI; return T_FLOAT_NUMBER; }
"quest"					{ return T_QUEST; }
"return"				{ return T_RETURN; }
"room"					{ return T_ROOM; }
"self"					{ return T_SELF; }
"ship"					{ return T_SHIP; }
"sizeof"                { return T_SIZEOF; }
"stat"					{ return T_STAT; }
"string"				{ return T_STRING; }
"switch"				{ return T_SWITCH; }
"table"					{ return T_TABLE; }
"token"					{ return T_TOKEN; }
"true"                  { return T_TRUE; }
"typeof"                { return T_TYPEOF; }
"while"					{ return T_WHILE; }
"widevnum"				{ return T_WIDEVNUM; }
"<["					{ BEGIN(FLAG_NAMES); return T_OPEN_FLAG; }
<FLAG_NAMES>"]>"		{ BEGIN(INITIAL); return T_CLOSE_FLAG; }
"{["                    { return T_OPEN_MAP; }
"]}"                    { return T_CLOSE_MAP; }
"({"					{ return T_OPEN_LIST; }
"})"					{ return T_CLOSE_LIST; }
"<<="					{ yylval->assign = NI_LSH_EQ; return T_ASSIGN; }
">>="					{ yylval->assign = NI_RSH_EQ; return T_ASSIGN; }
">>>="					{ yylval->assign = NI_RSHL_EQ; return T_ASSIGN; }
"+="					{ yylval->assign = NI_ADD_EQ; return T_ASSIGN; }
"-="					{ yylval->assign = NI_SUBT_EQ; return T_ASSIGN; }
"*="					{ yylval->assign = NI_MULT_EQ; return T_ASSIGN; }
"/="					{ yylval->assign = NI_DIV_EQ; return T_ASSIGN; }
"%="					{ yylval->assign = NI_MOD_EQ; return T_ASSIGN; }
"|="					{ yylval->assign = NI_BOR_EQ; return T_ASSIGN; }
"^="					{ yylval->assign = NI_BXOR_EQ; return T_ASSIGN; }
"&="					{ yylval->assign = NI_BAND_EQ; return T_ASSIGN; }
"::"                    { return T_COLONS; }
"->"                    { return T_ARROW; }
"**"					{ return T_EXPONENT; }
"--"					{ return T_DECREMENT; }
"++"					{ return T_INCREMENT; }
"<<"					{ return T_LEFT_SHIFT; }
">>>"					{ return T_RIGHTL_SHIFT; }
">>"					{ return T_RIGHT_SHIFT; }
"&&"					{ return T_LAND; }
"||"					{ return T_LOR; }
"^^"					{ return T_LXOR; }
"=="					{ return T_EQUAL; }
"!="					{ return T_NOT_EQUAL; }
"<="					{ return T_LT_EQUAL; }
">="					{ return T_GT_EQUAL; }
"<~"					{ return T_STR_PREFIX; }
"~~"					{ return T_STR_INFIX; }
"~>"					{ return T_STR_SUFFIX; }
"<"						{ return T_LT; }
">"						{ return T_GT; }
"="						{ yylval->assign = NI_ASSIGN; return T_ASSIGN; }
"!"						{ return T_LNOT; }
"&"						{ return T_BAND; }
"|"						{ return T_BOR; }
"^"						{ return T_BXOR; }
"~"						{ return T_BNOT; }
"+"						{ return T_PLUS; }
"-"						{ return T_MINUS; }
"*"						{ return T_STAR; }
"/"						{ return T_DIVIDE; }
"%"                     { return T_MOD; }
"??"					{ return T_COALESCE; }
"?"						{ return T_QMARK; }
":"						{ return T_COLON; }
";"						{ return T_SEMICOLON; }
"#"						{ return T_WIDEVNUM_DELIM; }
"..."					{ return T_VARARGS; }
".."                    { return T_RANGE; }
"."                     { BEGIN(FIELD_NAME); return T_DOT; }
"["                     { return T_OPEN_BRACKET; }
"]"                     { return T_CLOSE_BRACKET; }
"("						{ return T_OPEN_PAREN; }
")"						{ return T_CLOSE_PAREN; }
"{"						{ return T_OPEN_BRACE; }
"}"						{ return T_CLOSE_BRACE; }
","                     { return T_COMMA; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = nib_strdup(nibtext); return T_IDENTIFIER; }
<FIELD_NAME>[a-zA-Z_][a-zA-Z0-9_]*  { BEGIN(INITIAL); yylval->identifier = nib_strdup(nibtext); return T_IDENTIFIER; }
<FLAG_NAMES>"::"		{ return T_COLONS; }
<FLAG_NAMES>":"			{ return T_COLON; }
<FLAG_NAMES>","   		{ return T_COMMA; }
<FLAG_NAMES>[0-9]+		{ yylval->number = atol(nibtext); return T_NUMBER; }
<FLAG_NAMES>[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = nib_strdup(nibtext); return T_IDENTIFIER; }
[ \t\n]+  				{ /* Ignore white space*/ }
.                       { /* complain */ }

%%

