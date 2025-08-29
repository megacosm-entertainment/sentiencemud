OBJDIR = obj
YACCDIR = yacc

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
	decompile.c \
	dummy.c \
	flags.c \
	list.c \
	main.c \
	mem.c \
	methods.c \
	scopetree.c \
	types.c \
	utils.c \
	variables.c \
	$(FLEX_FILES) \
	$(BISON_FILES)

O_FILES = $(patsubst $(YACCDIR)/%.c,$(OBJDIR)/$(YACCDIR)/%.o,$(patsubst %.c,$(OBJDIR)/%.o,$(C_FILES)))

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
	@gcc -g -c $< -o $@

$(EXE): $(O_FILES)
	@echo Building $@
	@-rm -f $@	
	@gcc -g -o $@ $^ -lm
	@-chmod 775 $@

clean:
	@-rm -f $(OBJDIR)/*.o
	@-rm -f $(OBJDIR)/$(YACCDIR)/*.o
	@-rm -f $(YACCDIR)/*.*
	@-rm -rf $(OBJDIR)
	@-rm -rf $(YACCDIR)
