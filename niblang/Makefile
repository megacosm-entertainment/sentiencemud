OBJDIR = obj
YACCDIR = yacc
C_FLAGS = -Werror -Wfatal-errors -Wno-trigraphs
#C_FLAGS = -Wno-trigraphs

L_FILES = \
	lexer.l \
	method_lexer.l

FLEX_FILES = $(patsubst %.l,$(YACCDIR)/%.c,$(L_FILES))

Y_FILES = \
	parser.y \
	method_parser.y

BISON_FILES = $(patsubst %.y,$(YACCDIR)/%.c,$(Y_FILES))

C_FILES = \
	compile.c \
	dummy.c \
	flags.c \
	funcs.c \
	interpret.c \
	list.c \
	main.c \
	mem.c \
	methods.c \
	scopetree.c \
	script.c \
	tables.c \
	types.c \
	utils.c \
	variables.c

YACC_C_FILES = \
	$(FLEX_FILES) \
	$(BISON_FILES)

O_FILES = $(patsubst $(YACCDIR)/%.c,$(OBJDIR)/$(YACCDIR)/%.o,$(YACC_C_FILES)) $(patsubst %.c,$(OBJDIR)/%.o,$(C_FILES))

EXE = niblang

all: objdir yaccdir $(FLEX_FILES) $(BISON_FILES) $(O_FILES) $(EXE)

objdir:
	@-mkdir -p $(OBJDIR)
	@-mkdir -p $(OBJDIR)/$(YACCDIR)

yaccdir:
	@-mkdir -p $(YACCDIR)

$(YACCDIR)/%.c:  %.l
	@echo Generating Lexer $@
	@flex $<

$(YACCDIR)/%.c:  %.y
	@echo Generating Parser $@
	@bison --debug -Wcounterexamples $<

$(OBJDIR)/%.o:	%.c
	@echo Building $@...
	@gcc -g $(C_FLAGS) -c $< -o $@

$(EXE): $(O_FILES)
	@echo Building $@
	@-rm -f $@	
	@gcc -g -o $@ $^ -lm
	@-chmod 775 $@

-include .depend

clean:
	@-rm -f $(OBJDIR)/*.o
	@-rm -f $(OBJDIR)/$(YACCDIR)/*.o
	@-rm -f $(YACCDIR)/*.*
	@-rm -rf $(OBJDIR)
	@-rm -rf $(YACCDIR)

depend:
	@gcc -E -MM $(C_FILES) -I. > .depend
	@sed -e "/.*:/s//obj\/&/g" <.depend > .depend2
	@gcc -E -MM $(YACC_C_FILES) -I. > .depend
	@sed -e "/.*:/s//obj\/yacc\/&/g" <.depend > .depend3
	@sed -e "/yacc\/\.\.\//s///g" <.depend3 > .depend4
	@sed -e "/#.*/s///g" <.depend2 > .depend
	@sed -e "/#.*/s///g" <.depend4 >> .depend
	@rm -f .depend2
	@rm -f .depend3
	@rm -f .depend4

test:
	@gcc -E -MM $(C_FILES) -I. > .depend
	@sed -e "/.*:/s//obj\/&/g" <.depend > .depend2
	@sed -e "/#.*/s///g" <.depend2 > .depend
	@rm -f .depend2
