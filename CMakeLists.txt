#
# ROM CMakeLists.txt
#
cmake_minimum_required (VERSION 3.10)

project(sent)

# Build options
option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)
option(ENABLE_LEGACY_PFILE_READ "Enable legacy text pfile reader fallback" ON)

# Always generate compile_commands.json for clang-tidy and IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(WARNING "Coverage is best supported with GCC. Results may vary with other compilers.")
    endif()
    add_compile_options(--coverage -O0)
    add_link_options(--coverage)
endif()

# Set default parallel build jobs to nproc/2
include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 4)
endif()
math(EXPR DEFAULT_JOBS "${NPROC} / 2")
if(DEFAULT_JOBS LESS 1)
    set(DEFAULT_JOBS 1)
endif()
set(CMAKE_BUILD_PARALLEL_LEVEL ${DEFAULT_JOBS} CACHE STRING "Default parallel build jobs")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.deps/libbacktrace)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/channels)

# Required PCRE2 support for channel filtering.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

# libbacktrace for stack trace file:line resolution (prebuilt)
add_library(backtrace STATIC IMPORTED)
set_target_properties(backtrace PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/.deps/libbacktrace/.libs/libbacktrace.a
)

# Get Git version, build date, and commit URL
# Assumes date.sh and giturl.sh are in the same directory as CMakeLists.txt or in PATH
# and are executable.
execute_process(COMMAND git describe --dirty --always --tags OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND sh -c "./date.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE CUR_BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND sh -c "./giturl.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE GIT_URL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

# Fallback if git commands fail (e.g., not in a git repo or scripts not found)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown_version")
endif()
if(NOT CUR_BUILD_DATE)
    set(CUR_BUILD_DATE "unknown_date")
endif()
if(NOT GIT_URL)
    set(GIT_URL "unknown_url")
endif()

set(COMMON_COMPILE_OPTIONS -Wall -fcommon -DMALLOC_STDLIB -fstack-protector -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC -fno-omit-frame-pointer)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX /std:c17)
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(${COMMON_COMPILE_OPTIONS} -std=c23)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og")
elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(${COMMON_COMPILE_OPTIONS} ${CLANG_SPECIFIC_OPTIONS} -std=c23)
endif()

# Using the comprehensive C_FILES list from your GCC Makefile
set(SOURCE_FILES
    account/auth.c
    account/auth_migrate.c
    account/auth_sodium.c
    account/otp.c
    account/account_notes.c
    account/penalty.c
    account/preferences.c
    account/unlock.c
    act_class.c
    act_comm.c
    act_enter.c
    act_info.c
    act_info2.c
    act_move.c
    act_obj.c
    act_obj2.c
    act_wiz.c
    alias.c
    auction.c
    autowar.c
    ban.c
    bit.c
    blueprint.c
    boat.c
    bootstrap/bootstrap.c
    bootstrap/bootstrap_account.c
    bootstrap/bootstrap_commands.c
    bootstrap/bootstrap_files.c
    bootstrap/bootstrap_prompts.c
    bootstrap/bootstrap_reserved.c
    channels/channels_common.c
    channels/channel_policy.c
    channels/channel_registry.c
    channels/channel_service.c
    channels/channel_filter.c
    channels/channel_moderation.c
    channels/channel_review.c
    channels/channel_transport.c
    channels/channel_transport_local.c
    channels/channel_transport_redis.c
    chat_rooms.c
    church.c
    class_data.c
    comm.c
    connection.c
    connection_tcp.c
    connection_tls.c
    connection_websocket.c
    const.c
    protocol_layer.c
    protocol_telnet.c
    protocol_websocket.c
    db.c
    db2.c
    drunk.c
    dungeon.c
    editors/areas/aedit.c
    editors/blueprints/bpedit.c
    editors/blueprints/bsedit.c
    editors/channels/cedit.c
    editors/commands/cmdedit.c
    editors/corpses/corpsedit.c
    editors/dungeons/dngedit.c
    editors/events/evtedit.c
    editors/game_settings/gameedit.c
    editors/help/hedit.c
    editors/liquids/liqedit.c
    editors/materials/matedit.c
    editors/mobiles/medit.c
    editors/objects/oedit.c
    editors/objects/oedit_types.c
    editors/projects/pedit.c
    editors/quests/qedit.c
    editors/reputation/repedit.c
    editors/random_strings/rsgedit.c
    editors/reserved_vnums/reserved.c
    editors/rooms/redit.c
    editors/sectors/sectoredit.c
    editors/scripting/olc_mpcode.c
    editors/ships/shedit.c
    editors/socials/socialedit.c
    editors/tokens/tedit.c
    editors/wilderness/wedit.c
    editors/races/racedit.c
    editors/traits/traitedit.c
    editors/skills/skedit.c
    editors/skills/gredit.c
    editors/skills/soedit.c
    editors/classes/clsedit.c
    editors/common.c
    editors/common/olc_editor.c
    editors/common/olc_commands.c
    editors/common/olc_display.c
    effects.c
    event_types.c
    events.c
    fight.c
    fight2.c
    gq.c
    handler.c
    log.c
    help.c
    house.c
    hunt.c
    interp.c
    invasion.c
    item_types.c
    item_type_mem.c
    io/common.c
    io/json/json_chat.c
    io/json/json_church.c
    io/json/json_reserved.c
    io/json/json_sectors.c
    io/json/json_race.c
    traits.c
    lookup.c
    magic.c
    magic2.c
    magic_acid.c
    magic_air.c
    magic_astral.c
    magic_blood.c
    magic_body.c
    magic_chaos.c
    magic_cold.c
    magic_cosmic.c
    magic_dark.c
    magic_death.c
    magic_earth.c
    magic_energy.c
    magic_fire.c
    magic_holy.c
    magic_law.c
    magic_light.c
    magic_mana.c
    magic_metal.c
    magic_mind.c
    magic_nature.c
    magic_shock.c
    magic_soul.c
    magic_sound.c
    magic_toxin.c
    magic_water.c
    mail.c
    mccp.c
    mem.c
    mount.c
    music.c
    nanny.c
    nanny/nanny_auth.c
    nanny/nanny_menus.c
    nanny/nanny_utils.c
    note.c
    olc.c
    olc_act.c
    olc_act2.c
    olc_save.c
    project.c
    protocol.c
    mxp_links.c
    quest.c
    reputation.c
    requirements.c
    rview.c
    io/cache/redis_cache.c
    io/cache/async_cache.c
    io/json/json_common.c
    io/json/json_char.c
    io/json/json_account.c
    io/json/json_area.c
    io/json/json_game_settings.c
    io/json/json_gq.c
    io/json/json_instance.c
    io/json/json_mail.c
    io/json/json_note.c
    io/json/json_obj_types.c
    io/json/json_persist.c
    io/json/json_projects.c
    io/json/json_ban.c
    io/json/json_changesets.c
    io/json/json_commands.c
    io/json/json_corpse.c
    io/json/json_olc.c
    io/json/json_rsg.c
    io/json/json_socials.c
    io/json/json_staff.c
    save.c
    scan.c
    script_commands.c
    script_comp.c
    script_const.c
    script_expand.c
    script_ifc.c
    script_mpcmds.c
    script_opcmds.c
    script_rpcmds.c
    script_tpcmds.c
    script_vars.c
    scripts.c
    sectors_runtime.c
    secret.c
    sha256.c
    shoot.c
    skill_data.c
    skill_group.c
    skills.c
    song_data.c
    special.c
    staff.c
    stats.c
    string.c
    utils/buffer.c
    utils/tablefmt.c
    storage.c
    tables.c
    tls.c
    treasuremap.c
    update.c
    weather.c
    wilderness_mods.c
    wilderness_state.c
    wilderness_storage.c
    wilderness_vlinks.c
    wilderness_wmap.c
    wilderness_wterr.c
    wilds.c
    wilds_wildgen.c
)

# Add test integration source file only when BUILD_TESTS is enabled
if(BUILD_TESTS)
    list(APPEND SOURCE_FILES 
        test_integration.c
        tests/unit/buffer_function_cases_core.c
        tests/unit/buffer_function_cases_permutations.c
        tests/unit/buffer_function_tests.c
        tests/framework/test_framework.c
        tests/framework/test_loader.c
        tests/framework/test_dispatcher.c
        tests/framework/test_utils.c
        tests/unit/pure_function_tests.c
        tests/integration/wnum_tests.c
        tests/integration/string_editor_tests.c
        tests/integration/reset_tests.c
        tests/integration/shop_stock_tests.c
        tests/integration/church_tests.c
        tests/integration/instance_tests.c
        tests/integration/chat_rooms_tests.c
        tests/integration/skill_data_tests.c
        tests/integration/class_data_tests.c
        tests/integration/item_type_tests.c
        tests/integration/lookup_table_tests.c
        tests/integration/script_engine_tests.c
        tests/integration/channel_pubsub_tests.c
        tests/integration/combat_telemetry_tests.c
        tests/integration/song_data_tests.c
        tests/integration/skill_group_tests.c
        tests/integration/trait_system_tests.c
    )
endif()

add_executable(sent ${SOURCE_FILES})

target_include_directories(sent PRIVATE ${PCRE2_INCLUDE_DIRS})
target_compile_options(sent PRIVATE ${PCRE2_CFLAGS_OTHER})
target_compile_definitions(sent PRIVATE CHANNEL_FILTER_USE_PCRE2)
target_link_libraries(sent PRIVATE ${PCRE2_LIBRARIES})
target_link_directories(sent PRIVATE ${PCRE2_LIBRARY_DIRS})
message(STATUS "Channel filter: PCRE2 enabled (required)")

target_compile_definitions(sent PRIVATE
    VERSION="${GIT_VERSION}"
    BUILD_DATE="${CUR_BUILD_DATE}"
    COMMIT="${GIT_URL}"
    ENABLE_LEGACY_PFILE_READ=$<IF:$<BOOL:${ENABLE_LEGACY_PFILE_READ}>,1,0>
    # MUD_DEBUG enables debug features like stack trace logging
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:MUD_DEBUG>
    # BUILD_TESTS enables integration test framework
    $<$<BOOL:${BUILD_TESTS}>:BUILD_TESTS>
)

# Libraries (system: zlog, jansson, quickmail; local: cotp, backtrace)
target_link_libraries (sent PRIVATE pthread z m rt ssl crypto dl crypt quickmail cotp qrencode png hiredis jansson zlog backtrace sodium)

# Export symbols for stack trace support (backtrace_symbols needs this)
target_link_options(sent PRIVATE -rdynamic)

# Install step (optional, example)
# install(TARGETS sent DESTINATION bin)