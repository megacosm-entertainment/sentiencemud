%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../../merc.h"
#include "../niblang.h"

#define YYSTYPE NIBMETHODSTYPE
#define YYLTYPE NIBMETHODLTYPE
#include "method_parser.h"

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno;

static char *trim_outer_quotes(const char *src)
{
	int len = strlen(src) - 2;
	char *dest = malloc(len + 1);

	strncpy(dest, src + 1, len);
	dest[len] = 0;

	return dest;
}

%}

%option prefix="nibmethod"
%option outfile="yacc/method_lexer.c" header-file="yacc/method_lexer.h"
%option warn
%option yylineno

%option noyywrap
%option bison-locations
%option bison-bridge

%x ML_COMMENT
%x FIELD_NAME

%%
<INITIAL>"/*"			    { BEGIN(ML_COMMENT); /* ML comments */ }
<ML_COMMENT>"*/"		    { BEGIN(INITIAL); }
<ML_COMMENT>.|\n		    ;	// Ignore stuff inside comment
<ML_COMMENT><<EOF>>		    { /* Complain about unterminated comment */ }
"//"[^\n]*\n     			    ;	// Ignore EOL comments

[0-9]+					{ yylval->number = atol(nibmethodtext); return T_NUMBER; }
"account"				{ return T_ACCOUNT; }
"affect"				{ return T_AFFECT; }
"any"					{ return T_ANY; }
"area"					{ return T_AREA; }
"boolean"				{ return T_BOOLEAN; }
"bool"					{ return T_BOOLEAN; }
"channel"				{ return T_CHANNEL; }
"char"					{ return T_CHAR; }
"class"					{ return T_CLASS; }
"dungeon"				{ return T_DUNGEON; }
"exit"					{ return T_EXIT; }
"field"					{ return T_FIELD; }
"flag"					{ return T_FLAG; }
"float"					{ return T_FLOAT; }
"function"				{ return T_FUNCTION; }
"instance"				{ return T_INSTANCE; }
"int"					{ return T_INT; }
"liquid"				{ return T_LIQUID; }
"list"					{ return T_LIST; }
"mail"					{ return T_MAIL; }
"map"                   { return T_MAP; }
"material"				{ return T_MATERIAL; }
"method"				{ return T_METHOD; }
"mission"				{ return T_MISSION; }
"mobile"				{ return T_MOBILE; }
"object"				{ return T_OBJECT; }
"organization"			{ return T_ORG; }
"quest"					{ return T_QUEST; }
"race"					{ return T_RACE; }
"rank"					{ return T_RANK; }
"readonly"				{ return T_READONLY; }
"reputation"			{ return T_REPUTATION; }
"room"					{ return T_ROOM; }
"ship"					{ return T_SHIP; }
"skill"					{ return T_SKILL; }
"stat"					{ return T_STAT; }
"string"				{ return T_STRING; }
"token"					{ return T_TOKEN; }
"void"					{ return T_VOID; }
"widevnum"				{ return T_WIDEVNUM; }
"wilds"					{ return T_WILDS; }
"world"					{ return T_WORLD; }
"->"					{ return T_ARROW; }
"<"						{ return T_LT; }
">"						{ return T_GT; }
"("						{ return T_OP; }
")"						{ return T_CP; }
","						{ return T_COMMA; }
"..."					{ return T_VARARGS; }
"."						{ BEGIN(FIELD_NAME); return T_DOT; }
";"						{ return T_SEMICOLON; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
<FIELD_NAME>[a-zA-Z_][a-zA-Z0-9_]*  { BEGIN(INITIAL); yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
[ \t\n]+  				{ /* Ignore white space*/ }
.                       { /* complain */ }

%%

