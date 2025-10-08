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

#define TYPE(t,r)	yylval->byref = (r); return (t);

%}

%option prefix="nibmethod"
%option outfile="yacc/method_lexer.c" header-file="yacc/method_lexer.h"
%option warn
%option yylineno

%option noyywrap
%option bison-locations
%option bison-bridge

%option stack
%x ML_COMMENT
%x FIELD_NAME
%x INTERNAL_NAME
%x META_DATA

%%
<INITIAL>"/*"			    { yy_push_state(ML_COMMENT); /* ML comments */ }
<ML_COMMENT>"*/"		    { yy_pop_state(); }
<ML_COMMENT>.|\n		    ;	// Ignore stuff inside comment
<ML_COMMENT><<EOF>>		    { /* Complain about unterminated comment */ }
"//"[^\n]*\n     			    ;	// Ignore EOL comments

[0-9]+					{ yylval->number = atol(nibmethodtext); return T_NUMBER; }
<INITIAL,META_DATA>\".*\"				{
		// Clip off the quotes
		yylval->literal = strdup(nibmethodtext+1);
		size_t len = strlen(yylval->literal);
		yylval->literal[len - 1] = '\0';

		return T_STRING_LITERAL;
	}
"account&"				{ TYPE(T_ACCOUNT,true) }
"account"				{ TYPE(T_ACCOUNT,false) }
"affect&"				{ TYPE(T_AFFECT,true) }
"affect"				{ TYPE(T_AFFECT,false) }
"any"					{ return T_ANY; }
"area&"					{ TYPE(T_AREA,true) }
"area"					{ TYPE(T_AREA,false) }
"array"					{ return T_ARRAY; }
"boolean&"				{ TYPE(T_BOOLEAN,true) }
"boolean"				{ TYPE(T_BOOLEAN,false) }
"bool&"					{ TYPE(T_BOOLEAN,true) }
"bool"					{ TYPE(T_BOOLEAN,false) }
"channel&"				{ TYPE(T_CHANNEL,true) }
"channel"				{ TYPE(T_CHANNEL,false) }
"char&"					{ TYPE(T_CHAR,true) }
"char"					{ TYPE(T_CHAR,false) }
"class&"				{ TYPE(T_CLASS,true) }
"class"					{ TYPE(T_CLASS,false) }
"constant"				{ return T_CONSTANT; }
"const"					{ return T_CONSTANT; }
"dice&"					{ TYPE(T_DICE,true) }
"dice"					{ TYPE(T_DICE,false) }
"dungeon&"				{ TYPE(T_DUNGEON,true) }
"dungeon"				{ TYPE(T_DUNGEON,false) }
"exit&"					{ TYPE(T_EXIT,true) }
"exit"					{ TYPE(T_EXIT,false) }
"field"					{ return T_FIELD; }
"flagbank&"				{ TYPE(T_FLAGBANK,true) }
"flagbank"				{ TYPE(T_FLAGBANK,false) }
"flag&"					{ TYPE(T_FLAG,true) }
"flag"					{ TYPE(T_FLAG,false) }
"float&"				{ TYPE(T_FLOAT,true) }
"float"					{ TYPE(T_FLOAT,false) }
"function"				{ return T_FUNCTION; }
"instance&"				{ TYPE(T_INSTANCE,true) }
"instance"				{ TYPE(T_INSTANCE,false) }
"int16&"				{ TYPE(T_INT16,true) }
"int16"					{ TYPE(T_INT16,false) }
"int32&"				{ TYPE(T_INT32,true) }
"int32"					{ TYPE(T_INT32,false) }
"int&"					{ TYPE(T_INT,true) }
"int"					{ TYPE(T_INT,false) }
"liquid&"				{ TYPE(T_LIQUID,true) }
"liquid"				{ TYPE(T_LIQUID,false) }
"list&"					{ TYPE(T_LIST,true) }
"list"					{ TYPE(T_LIST,false) }
"mail&"					{ TYPE(T_MAIL,true) }
"mail"					{ TYPE(T_MAIL,false) }
"map"                   { TYPE(T_MAP,false) }
"material&"				{ TYPE(T_MATERIAL,true) }
"material"				{ TYPE(T_MATERIAL,false) }
"method"				{ return T_METHOD; }
"mission&"				{ TYPE(T_MISSION,true) }
"mission"				{ TYPE(T_MISSION,false) }
"mobile&"				{ TYPE(T_MOBILE,true) }
"mobile"				{ TYPE(T_MOBILE,false) }
"multi"					{ return T_MULTI; }
"mutable"				{ return T_MUTABLE; }
"object&"				{ TYPE(T_OBJECT,true) }
"object"				{ TYPE(T_OBJECT,false) }
"organization&"			{ TYPE(T_ORG,true) }
"organization"			{ TYPE(T_ORG,false) }
"quest&"				{ TYPE(T_QUEST,true) }
"quest"					{ TYPE(T_QUEST,false) }
"race&"					{ TYPE(T_RACE,true) }
"race"					{ TYPE(T_RACE,false) }
"rank&"					{ TYPE(T_RANK,true) }
"rank"					{ TYPE(T_RANK,false) }
"reputation&"			{ TYPE(T_REPUTATION,true) }
"reputation"			{ TYPE(T_REPUTATION,false) }
"room&"					{ TYPE(T_ROOM,true) }
"room"					{ TYPE(T_ROOM,false) }
"sector&"				{ TYPE(T_SECTOR,true) }
"sector"				{ TYPE(T_SECTOR,false) }
"ship&"					{ TYPE(T_SHIP,true) }
"ship"					{ TYPE(T_SHIP,false) }
"skill&"				{ TYPE(T_SKILL,true) }
"skill"					{ TYPE(T_SKILL,false) }
"stat16&"				{ TYPE(T_STAT16,true) }
"stat16"				{ TYPE(T_STAT16,false) }
"stat32&"				{ TYPE(T_STAT32,true) }
"stat32"				{ TYPE(T_STAT32,false) }
"stat&"					{ TYPE(T_STAT,true) }
"stat"					{ TYPE(T_STAT,false) }
"string&"				{ TYPE(T_STRING,true) }
"string"				{ TYPE(T_STRING,false) }
"time&"					{ TYPE(T_TIME,true) }
"time"					{ TYPE(T_TIME,false) }
"token&"				{ TYPE(T_TOKEN,true) }
"token"					{ TYPE(T_TOKEN,false) }
"void"					{ return T_VOID; }
"widevnum&"				{ TYPE(T_WIDEVNUM,true) }
"widevnum"				{ TYPE(T_WIDEVNUM,false) }
"wilds&"				{ TYPE(T_WILDS,true) }
"wilds"					{ TYPE(T_WILDS,false) }
"world&"				{ TYPE(T_WORLD,true) }
"world"					{ TYPE(T_WORLD,false) }
"->"					{ yy_push_state(INTERNAL_NAME); return T_ARROW; }

"<"						{ return T_LT; }
">"						{ return T_GT; }
<INITIAL,FIELD_NAME>"("						{ return T_OP; }
")"						{ return T_CP; }
"["						{ return T_OB; }
"]"						{ return T_CB; }
<INITIAL,META_DATA>","	{ return T_COMMA; }
"|"						{ return T_BAR; }
"..."					{ return T_VARARGS; }
"."						{ yy_push_state(FIELD_NAME); return T_DOT; }
";"						{ return T_SEMICOLON; }
"{"						{ yy_push_state(META_DATA); return T_OMD; }
<META_DATA>"}"			{ yy_pop_state(); return T_CMD;}
<META_DATA>":"			{ return T_COLON; }
<META_DATA>"name"			{ return T_MD_NAME; }
<META_DATA>"description"	{ return T_MD_DESCRIPTION; }
<META_DATA>[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
<FIELD_NAME>[a-zA-Z_][a-zA-Z0-9_]*  { yy_pop_state(); yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
<INTERNAL_NAME>[a-zA-Z_][\.a-zA-Z0-9_\[\]]*  { yy_pop_state(); yylval->identifier = nib_strdup(nibmethodtext); return T_IDENTIFIER; }
<*>[ \t\n]+				{ /* Ignore white space*/ }
.                       { /* complain */ }

%%

