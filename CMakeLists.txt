#
# ROM CMakeLists.txt
# Initial version, need to update this.
#
cmake_minimum_required (VERSION 3.10)

project(sent)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX /std:c17)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -std=gnu2x -fcommon -DOLD_RAND -DMALLOC_STDLIB -fstack-protector  -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC -fno-omit-frame-pointer)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -std=c2x -fcommon -DOLD_RAND -DMALLOC_STDLIB -fstack-protector  -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC -fno-omit-frame-pointer)
endif()

add_executable(sent act_comm.c act_enter.c act_info.c act_info2.c act_move.c act_obj.c 
    act_obj2.c act_wiz.c alias.c auction.c autowar.c ban.c bit.c blueprint.c boat.c
    church.c comm.c const.c db.c db2.c drunk.c dungeon.c effects.c events.c fight.c 
    fight2.c gq.c handler.c healer.c help.c house.c html.c hunt.c interp.c invasion.c 
    locker.c lookup.c magic.c magic2.c magic_acid.c magic_air.c magic_astral.c
    magic_blood.c magic_body.c magic_chaos.c magic_cold.c magic_cosmic.c magic_dark.c
    magic_death.c magic_earth.c magic_energy.c magic_fire.c magic_holy.c magic_law.c
    magic_light.c magic_mana.c magic_metal.c magic_mind.c magic_nature.c magic_shock.c
    magic_soul.c magic_sound.c magic_toxin.c magic_water.c mail.c mccp.c mem.c 
    mount.c music.c note.c olc.c olc_act.c olc_act2.c olc_mpcode.c olc_save.c project.c
    protocol.c quest.c save.c scan.c script_commands.c script_comp.c script_const.c
    script_expand.c script_ifc.c script_mpcmds.c script_opcmds.c script_rpcmds.c
    script_tpcmds.c script_vars.c scripts.c sha256.c shoot.c skills.c social.c special.c 
    staff.c stats.c string.c tables.c treasuremap.c update.c weather.c wilds.c)

    target_link_libraries (sent pthread z m rt ssl crypto dl crypt)