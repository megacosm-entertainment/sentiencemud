CC      = gcc
PROF    = -Wall -O -g -pg -ggdb -g 
OBJDIR	= obj
VPATH   = .:obj
LIBS = -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl -lcrypt
#LIBS    = -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lpthread -lz -lm -ldl
NOCRYPT = -Dlinux
#C_FLAGS =  $(PROF) $(NOCRYPT) -DOLD_RAND -DMALLOC_STDLIB -I/usr/include/mysql -DBIG_JOINS=1  -fno-strict-aliasing   -g -fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing
C_FLAGS = $(PROF) -DOLD_RAND -DMALLOC_STDLIB -I/usr/include/mysql -fstack-protector  -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC  -fPIC  -fabi-version=2 -fno-omit-frame-pointer -fno-strict-aliasing
L_FLAGS =  $(PROF) $(LIBS)
EXE	= sent
BUILD_NUMBER_FILE = build.txt

VERSION1 = 20061008
PATH1 = ../20061008
VERSION2 = vizzwild
PATH2 = ../vizzwild

DIFF_TXT = diff_$(VERSION1)_$(VERSION2).txt
DIFF_C = $(patsubst $(PATH1)/%.c,%_c.diff,$(wildcard $(PATH1)/*.c)) $(patsubst $(PATH1)/%.h,%_h.diff,$(wildcard $(PATH1)/*.h))

C_FILES = \
	act_comm.c \
	act_enter.c \
	act_info.c \
	act_info2.c \
	act_move.c \
	act_obj.c \
	act_obj2.c \
	act_wiz.c \
	alias.c \
	auction.c \
	autowar.c \
	ban.c \
	bit.c \
	church.c \
	comm.c \
	const.c \
	db.c \
	db2.c \
	drunk.c \
	effects.c \
	events.c \
	fight.c \
	fight2.c \
	gq.c \
	handler.c \
	healer.c \
	help.c \
	house.c \
	html.c \
	hunt.c \
	interp.c \
	invasion.c \
	locker.c \
	lookup.c \
	magic.c \
	magic2.c \
	magic_acid.c \
	magic_air.c \
	magic_astral.c \
	magic_blood.c \
	magic_body.c \
	magic_chaos.c \
	magic_cold.c \
	magic_cosmic.c \
	magic_dark.c \
	magic_death.c \
	magic_earth.c \
	magic_energy.c \
	magic_fire.c \
	magic_holy.c \
	magic_law.c \
	magic_light.c \
	magic_mana.c \
	magic_metal.c \
	magic_mind.c \
	magic_nature.c \
	magic_shock.c \
	magic_soul.c \
	magic_sound.c \
	magic_toxin.c \
	magic_water.c \
	mail.c \
	mccp.c \
	mem.c \
	mount.c \
	music.c \
	note.c \
	olc.c \
	olc_act.c \
	olc_act2.c \
	olc_mpcode.c \
	olc_save.c \
	project.c \
	protocol.c \
	quest.c \
	save.c \
	scan.c \
	script_commands.c \
	script_comp.c \
	script_const.c \
	script_expand.c \
	script_ifc.c \
	script_mpcmds.c \
	script_opcmds.c \
	script_rpcmds.c \
	script_tpcmds.c \
	script_vars.c \
	scripts.c \
	shoot.c \
	skills.c \
	social.c \
	special.c \
	staff.c \
	stats.c \
	string.c \
	tables.c \
	treasuremap.c \
	update.c \
	weather.c \
	wilds.c \

O_FILES = \
	$(OBJDIR)/act_comm.o \
	$(OBJDIR)/act_enter.o \
	$(OBJDIR)/act_info.o \
	$(OBJDIR)/act_info2.o \
	$(OBJDIR)/act_move.o \
	$(OBJDIR)/act_obj.o \
	$(OBJDIR)/act_obj2.o \
	$(OBJDIR)/act_wiz.o \
	$(OBJDIR)/alias.o \
	$(OBJDIR)/auction.o \
	$(OBJDIR)/autowar.o \
	$(OBJDIR)/ban.o \
	$(OBJDIR)/bit.o \
	$(OBJDIR)/church.o \
	$(OBJDIR)/comm.o \
	$(OBJDIR)/const.o \
	$(OBJDIR)/db.o \
	$(OBJDIR)/db2.o \
	$(OBJDIR)/drunk.o \
	$(OBJDIR)/effects.o \
	$(OBJDIR)/events.o \
	$(OBJDIR)/fight.o \
	$(OBJDIR)/fight2.o \
	$(OBJDIR)/gq.o \
	$(OBJDIR)/handler.o \
	$(OBJDIR)/healer.o \
	$(OBJDIR)/help.o \
	$(OBJDIR)/house.o \
	$(OBJDIR)/html.o \
	$(OBJDIR)/hunt.o \
	$(OBJDIR)/interp.o \
	$(OBJDIR)/invasion.o \
	$(OBJDIR)/locker.o \
	$(OBJDIR)/lookup.o \
	$(OBJDIR)/magic.o \
	$(OBJDIR)/magic2.o \
	$(OBJDIR)/magic_acid.o \
	$(OBJDIR)/magic_air.o \
	$(OBJDIR)/magic_astral.o \
	$(OBJDIR)/magic_blood.o \
	$(OBJDIR)/magic_body.o \
	$(OBJDIR)/magic_chaos.o \
	$(OBJDIR)/magic_cold.o \
	$(OBJDIR)/magic_cosmic.o \
	$(OBJDIR)/magic_dark.o \
	$(OBJDIR)/magic_death.o \
	$(OBJDIR)/magic_earth.o \
	$(OBJDIR)/magic_energy.o \
	$(OBJDIR)/magic_fire.o \
	$(OBJDIR)/magic_holy.o \
	$(OBJDIR)/magic_law.o \
	$(OBJDIR)/magic_light.o \
	$(OBJDIR)/magic_mana.o \
	$(OBJDIR)/magic_metal.o \
	$(OBJDIR)/magic_mind.o \
	$(OBJDIR)/magic_nature.o \
	$(OBJDIR)/magic_shock.o \
	$(OBJDIR)/magic_soul.o \
	$(OBJDIR)/magic_sound.o \
	$(OBJDIR)/magic_toxin.o \
	$(OBJDIR)/magic_water.o \
	$(OBJDIR)/mail.o \
	$(OBJDIR)/mccp.o \
	$(OBJDIR)/mem.o \
	$(OBJDIR)/mount.o \
	$(OBJDIR)/music.o \
	$(OBJDIR)/note.o \
	$(OBJDIR)/olc.o \
	$(OBJDIR)/olc_act.o \
	$(OBJDIR)/olc_act2.o \
	$(OBJDIR)/olc_mpcode.o \
	$(OBJDIR)/olc_save.o \
	$(OBJDIR)/project.o \
	$(OBJDIR)/protocol.o \
	$(OBJDIR)/quest.o \
	$(OBJDIR)/save.o \
	$(OBJDIR)/scan.o \
	$(OBJDIR)/script_commands.o \
	$(OBJDIR)/script_comp.o \
	$(OBJDIR)/script_const.o \
	$(OBJDIR)/script_expand.o \
	$(OBJDIR)/script_ifc.o \
	$(OBJDIR)/script_mpcmds.o \
	$(OBJDIR)/script_opcmds.o \
	$(OBJDIR)/script_rpcmds.o \
	$(OBJDIR)/script_tpcmds.o \
	$(OBJDIR)/script_vars.o \
	$(OBJDIR)/scripts.o \
	$(OBJDIR)/sha256.o \
	$(OBJDIR)/shoot.o \
	$(OBJDIR)/skills.o \
	$(OBJDIR)/social.o \
	$(OBJDIR)/special.o \
	$(OBJDIR)/staff.o \
	$(OBJDIR)/stats.o \
	$(OBJDIR)/string.o \
	$(OBJDIR)/tables.o \
	$(OBJDIR)/treasuremap.o \
	$(OBJDIR)/update.o \
	$(OBJDIR)/weather.o \
	$(OBJDIR)/wilds.o \


ifdef IMC

O_FILES := $(OBJDIR)/imc.o $(OBJDIR)/sha256.o $(O_FILES)

C_FLAGS := $(C_FLAGS) -DIMC -DIMCROM

endif


	  
all: objdir $(O_FILES) $(EXE)

install: all
	-cp -f *.c ~/alpha/src/
	-cp -f *.h ~/alpha/src/
	-cp -f $(EXE) ~/alpha/src/


objdir:
	-mkdir obj
	-chmod 775 obj

$(EXE): $(O_FILES) $(BUILD_NUMBER_FILE)
	rm -f $(EXE)
	$(CC) $(BUILD_NUMBER_LFLAGS) -o $(EXE) $(O_FILES) $(L_FLAGS)
	-chmod 775 $(EXE)

$(OBJDIR)/%.o:	%.c
	@echo Building $<...
	@$(CC) -c $(C_FLAGS) $< -o $(OBJDIR)/$(basename $<).o

%_c.diff:	$(PATH1)/%.c
	-diff -wbBdp $< $(patsubst $(PATH1)/%,$(PATH2)/%,$<) >> $(DIFF_TXT)

%_h.diff:	$(PATH1)/%.h
	-diff -wbBdp $< $(patsubst $(PATH1)/%,$(PATH2)/%,$<) >> $(DIFF_TXT)

-include .depend

clean:
	rm -f $(OBJDIR)/*.o
	rm -rf $(OBJDIR)
	rm -f sent

depend:
	$(CC) -E -MM $(C_FILES) $(C_FLAGS) -I. > .depend
	sed -e "/.*:/s//obj\/&/g" <.depend > .depend2
	sed -e "/#.*/s///g" <.depend2 > .depend
	rm -f .depend2

clear_diff:
	-rm -f $(DIFF_TXT)

diff:	clear_diff $(DIFF_C)

-include .depend
include buildnumber.mak
