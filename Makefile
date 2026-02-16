CC      = gcc
PROF    = -Wall -O -g -pg -ggdb
OBJDIR	= obj

# Parallel build: use half of available cores by default
# Override with: make JOBS=N or make -jN
NPROC := $(shell nproc 2>/dev/null || echo 4)
JOBS ?= $(shell echo $$(($(NPROC) / 2)))
MAKEFLAGS += -j$(JOBS)

# Dependency directories (submodules in .deps/, others are system packages)
DEPS_DIR = .deps
LIBCOTP_DIR = $(DEPS_DIR)/libcotp
LIBBACKTRACE_DIR = $(DEPS_DIR)/libbacktrace

# Include paths for local dependencies
INCLUDES = -I$(LIBCOTP_DIR)/src -I$(LIBBACKTRACE_DIR)

# Library paths for local dependencies
LIB_PATHS = -L$(LIBCOTP_DIR) -L$(LIBBACKTRACE_DIR)/.libs

# Libraries (system: zlog, jansson, quickmail; local: cotp, backtrace)
LIBS = -lpthread -lz -lm -lrt -lssl -lcrypto -ldl -lcrypt -lquickmail -lcotp -lqrencode -lpng -lhiredis -ljansson -lzlog -lbacktrace -lsodium

GIT_VERSION := "$(shell git describe --dirty --always --tags)"
CUR_BUILD_DATE := "$(shell sh date.sh)"
GIT_URL := "$(shell sh giturl.sh)"

C_FLAGS = $(PROF) -std=c23 -fcommon -DMALLOC_STDLIB -fstack-protector -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC -fabi-version=2 -fno-omit-frame-pointer -DVERSION=\"$(GIT_VERSION)\" -DBUILD_DATE=\"$(CUR_BUILD_DATE)\" -DCOMMIT=\"$(GIT_URL)\" -DMUD_DEBUG -MMD -MP $(INCLUDES)
# -rdynamic exports symbols for stack trace support (backtrace_symbols)
L_FLAGS = $(PROF) -rdynamic $(LIB_PATHS) $(LIBS)

# Build with tests: make BUILD_TESTS=1
ifdef BUILD_TESTS
    C_FLAGS += -DBUILD_TESTS
endif

# Build with coverage: make BUILD_COVERAGE=1
ifdef BUILD_COVERAGE
    C_FLAGS += --coverage -O0
    L_FLAGS += --coverage
    BUILD_TESTS = 1
    C_FLAGS += -DBUILD_TESTS
endif

EXE	= sent

C_FILES = \
    account/auth.c \
    account/auth_migrate.c \
    account/auth_sodium.c \
    account/otp.c \
    account/account_notes.c \
    account/penalty.c \
    account/preferences.c \
    account/unlock.c \
    act_class.c \
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
    blueprint.c \
    boat.c \
    bootstrap/bootstrap.c \
    bootstrap/bootstrap_account.c \
    bootstrap/bootstrap_commands.c \
    bootstrap/bootstrap_files.c \
    bootstrap/bootstrap_prompts.c \
    bootstrap/bootstrap_reserved.c \
    chat_rooms.c \
    church.c \
    class_data.c \
    comm.c \
    connection.c \
    connection_tcp.c \
    connection_tls.c \
    connection_websocket.c \
    const.c \
    protocol_layer.c \
    protocol_telnet.c \
    protocol_websocket.c \
    db.c \
    db2.c \
    drunk.c \
    dungeon.c \
    editors/areas/aedit.c \
    editors/blueprints/bpedit.c \
    editors/blueprints/bsedit.c \
    editors/commands/cmdedit.c \
    editors/dungeons/dngedit.c \
    editors/game_settings/gameedit.c \
    editors/help/hedit.c \
    editors/mobiles/medit.c \
    editors/objects/oedit.c \
    editors/projects/pedit.c \
    editors/random_strings/rsgedit.c \
    editors/reserved_vnums/reserved.c \
    editors/rooms/redit.c \
    editors/scripting/olc_mpcode.c \
    editors/ships/shedit.c \
    editors/socials/socialedit.c \
    editors/tokens/tedit.c \
    editors/wilderness/vledit.c \
    editors/wilderness/wedit.c \
    editors/races/racedit.c \
    editors/traits/traitedit.c \
    editors/skills/skedit.c \
    editors/skills/gredit.c \
    editors/skills/soedit.c \
    editors/classes/clsedit.c \
    editors/common.c \
    effects.c \
    events.c \
    fight.c \
    fight2.c \
    gq.c \
    handler.c \
    help.c \
    house.c \
    hunt.c \
    interp.c \
    invasion.c \
    item_types.c \
    item_type_mem.c \
    io/common.c \
    log.c \
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
    nanny.c \
    nanny/nanny_auth.c \
    nanny/nanny_menus.c \
    nanny/nanny_utils.c \
    note.c \
    olc.c \
    olc_act.c \
    olc_act2.c \
    olc_save.c \
    project.c \
    protocol.c \
    mxp_links.c \
    quest.c \
    io/cache/redis_cache.c \
    io/cache/async_cache.c \
    io/json/json_char.c \
    io/json/json_account.c \
    io/json/json_area.c \
    io/json/json_chat.c \
    io/json/json_church.c \
    io/json/json_game_settings.c \
    io/json/json_gq.c \
    io/json/json_instance.c \
    io/json/json_mail.c \
    io/json/json_note.c \
    io/json/json_obj_types.c \
    io/json/json_persist.c \
    io/json/json_race.c \
    io/json/json_reserved.c \
    traits.c \
    io/json/json_ban.c \
    io/json/json_changesets.c \
    io/json/json_commands.c \
    io/json/json_projects.c \
    io/json/json_socials.c \
    io/json/json_staff.c \
    save.c \
    pfile_migrate.c \
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
    secret.c \
    shoot.c \
    skill_data.c \
    skill_group.c \
    skills.c \
    song_data.c \
    special.c \
    staff.c \
    stats.c \
    string.c \
    storage.c \
    tables.c \
    tls.c \
    treasuremap.c \
    update.c \
    weather.c \
    wilds.c

# Add test integration source file only when BUILD_TESTS is enabled
ifdef BUILD_TESTS
    C_FILES += test_integration.c \
               tests/framework/test_framework.c \
               tests/framework/test_loader.c \
               tests/integration/wnum_tests.c \
               tests/integration/reset_tests.c \
               tests/integration/shop_stock_tests.c \
               tests/integration/church_tests.c \
               tests/integration/instance_tests.c \
               tests/integration/chat_rooms_tests.c \
               tests/integration/skill_data_tests.c \
               tests/integration/class_data_tests.c \
               tests/integration/item_type_tests.c \
               tests/integration/lookup_table_tests.c \
               tests/integration/song_data_tests.c \
               tests/integration/skill_group_tests.c \
               tests/integration/trait_system_tests.c
endif

O_FILES = $(patsubst %.c,$(OBJDIR)/%.o,$(C_FILES))

DEP_FILES = $(O_FILES:.o=.d)
all: $(EXE)

OBJ_DIRS_NEEDED := $(sort $(OBJDIR) $(patsubst %/,%,$(dir $(O_FILES))))
objdir:
	@echo "Creating object directories..."
	@mkdir -p $(OBJ_DIRS_NEEDED)
	@-chmod 775 $(OBJ_DIRS_NEEDED)

$(EXE): objdir $(O_FILES)
	@echo "Linking $(EXE)..."
	@rm -f $(EXE)
	@$(CC) -o $(EXE) $(O_FILES) $(L_FLAGS)
	@-chmod 775 $(EXE)
	@echo "Build complete!"

$(OBJDIR)/%.o: %.c
	@echo "Building $<..."
	@mkdir -p $(dir $@)
	@$(CC) -c $(C_FLAGS) $< -o $@

-include $(DEP_FILES)

install: all
	@echo "Installing $(EXE) to ../"
	-cp -f $(EXE) ../
	@echo "Installation complete."

clean:
	@echo "Cleaning up..."
	@-rm -f $(O_FILES) $(DEP_FILES) $(EXE)
	@-rm -rf $(OBJDIR)

.PHONY: all install objdir clean