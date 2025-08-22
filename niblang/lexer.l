%{
#include <stdio.h>
#include <stdlib.h>

#include "../niblang.h"

#define YYSTYPE NIBSTYPE
#define YYLTYPE NIBLTYPE
#include "parser.h"

//extern NIBLTYPE yylloc;

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno;

%}

%option prefix="nib"
%option outfile="yacc/lexer.c" header-file="yacc/lexer.h"
%option warn
%option yylineno

%option noyywrap
%option bison-locations
%option bison-bridge

%x ML_COMMENT

%%

<INITIAL>"/*"			    { BEGIN(ML_COMMENT); /* ML comments */ }
<ML_COMMENT>"*/"		    { BEGIN(INITIAL); }
<ML_COMMENT>.|\n		    ;	// Ignore stuff inside comment
<ML_COMMENT><<EOF>>		    { /* Complain about unterminated comment */ }
"//"[^\n]*\n     			    ;	// Ignore EOL comments

[0-9]+\.[0-9]+			{ yylval->float_number = atof(nibtext); return T_FLOAT_NUMBER; }
[0-9]+					{ yylval->number = atoi(nibtext); return T_NUMBER; }
\"(\\.|[^"\\])*\"		{ yylval->literal = strdup(nibtext); return T_STRING_LITERAL; }
"global"                { return T_GLOBAL; }
"map"                   { return T_MAP; }
"if"					{ return T_IF; }
"else"					{ return T_ELSE; }
"switch"				{ return T_SWITCH; }
"case"					{ return T_CASE; }
"default"				{ return T_DEFAULT; }
"while"					{ return T_WHILE; }
"do"					{ return T_DO; }
"for"					{ return T_FOR; }
"break"					{ return T_BREAK; }
"continue"				{ return T_CONTINUE; }
"return"				{ return T_RETURN; }
"int"					{ return T_INT; }
"float"					{ return T_FLOAT; }
"string"				{ return T_STRING; }
"widevnum"				{ return T_WIDEVNUM; }
"boolean"				{ return T_BOOLEAN; }
"flag"					{ return T_FLAG; }
"list"					{ return T_LIST; }
"self"					{ return T_SELF; }
"mobile"				{ return T_MOBILE; }
"object"				{ return T_OBJECT; }
"room"					{ return T_ROOM; }
"token"					{ return T_TOKEN; }
"area"					{ return T_AREA; }
"instance"				{ return T_INSTANCE; }
"dungeon"				{ return T_DUNGEON; }
"ship"					{ return T_SHIP; }
"quest"					{ return T_QUEST; }
"true"                  { return T_TRUE; }
"false"                 { return T_FALSE; }
"null"                  { return T_NULL; }
"sizeof"                { return T_SIZEOF; }
"typeof"                { return T_TYPEOF; }
"{["                    { return T_OPEN_MAP; }
"]}"                    { return T_CLOSE_MAP; }
"<<="					{ return T_LS_ASSIGN; }
">>="					{ return T_RS_ASSIGN; }
"::"                    { return T_COLONS; }
"->"                    { return T_ARROW; }
"**"					{ return T_EXPONENT; }
"--"					{ return T_DECREMENT; }
"++"					{ return T_INCREMENT; }
"<<"					{ return T_LEFT_SHIFT; }
">>"					{ return T_RIGHT_SHIFT; }
"&&"					{ return T_LAND; }
"||"					{ return T_LOR; }
"^^"					{ return T_LXOR; }
"=="					{ return T_EQUAL; }
"!="					{ return T_NOT_EQUAL; }
"<="					{ return T_LT_EQUAL; }
">="					{ return T_GT_EQUAL; }
"<"						{ return T_LT; }
">"						{ return T_GT; }
"="						{ return T_ASSIGN; }
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
".."                    { return T_RANGE; }
"."                     { return T_DOT; }
"["                     { return T_OPEN_BRACKET; }
"]"                     { return T_CLOSE_BRACKET; }
"("						{ return T_OPEN_PAREN; }
")"						{ return T_CLOSE_PAREN; }
"{"						{ return T_OPEN_BRACE; }
"}"						{ return T_CLOSE_BRACE; }
","                     { return T_COMMA; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval->identifier = strdup(nibtext); return T_IDENTIFIER; }
[ \t\n]+  				{ /* Ignore white space*/ }
.                       { /* complain */ }

%%

