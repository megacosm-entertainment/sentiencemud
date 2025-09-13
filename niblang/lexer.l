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

static char *trim_outer_quotes(const char *src)
{
	int len = strlen(src) - 2;
	char *dest = nib_malloc(len + 1);

	strncpy(dest, src + 1, len);
	dest[len] = 0;

	return dest;
}

static char parse_escape_character(char *str)
{
	// Assume '\#' where # is the escape character we want to return
	switch(str[2])
	{
		case 'n':	return '\n';
		case 'r':	return '\r';
		case 't':	return '\t';
		case 'b':	return '\b';
		case '0':	return '\0';
	}

	return str[2];
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

%%

<INITIAL>"/*"			    { BEGIN(ML_COMMENT); /* ML comments */ }
<ML_COMMENT>"*/"		    { BEGIN(INITIAL); }
<ML_COMMENT>.|\n		    ;	// Ignore stuff inside comment
<ML_COMMENT><<EOF>>		    { /* Complain about unterminated comment */ }
"//"[^\n]*					;	// Ignore EOL comments

[0-9]+\.[0-9]+			{ yylval->float_number = atof(nibtext); return T_FLOAT_NUMBER; }
0x[0-9a-fA-F]+			{ yylval->number = strtol(nibtext+2,NULL,16); return T_NUMBER;}
[0-9]+					{ yylval->number = atol(nibtext); return T_NUMBER; }
['].[']					{ yylval->ch = nibtext[1]; return T_CHAR_LITERAL; }
[']\\.[']				{ yylval->ch = parse_escape_character(nibtext); return T_CHAR_LITERAL; }
\"(\\.|[^"\\])*\"		{ yylval->literal = trim_outer_quotes(nibtext); return T_STRING_LITERAL; }
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

