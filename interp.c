/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik Strfeldt, Tom Madsen, and Katja Nyboe.    *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "merc.h"
#include "interp.h"
#include "scripts.h"

// Log-all switch
bool				logAll		= false;


/*
 * Command table.
 */
const	struct	cmd_type	cmd_table	[] =
{
	{ "'",					CMDTYPE_COMM,		do_say,					POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ ",",					CMDTYPE_COMM,		do_emote,				POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ ".",					CMDTYPE_COMM,		do_gossip,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, false,	true },
	{ "/",					CMDTYPE_INFO,		do_chat,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ ":",					CMDTYPE_COMM,		do_immtalk,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, false,	true },
	{ ";",					CMDTYPE_COMM,		do_gtell,				POS_DEAD,		STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "?",					CMDTYPE_NONE,		do_help,				POS_DEAD,		STAFF_PLAYER,		LOG_NEVER,	true,	true },
	{ "^",					CMDTYPE_IMMORTAL,	do_uninvis,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	false },
	{ "addcommand",			CMDTYPE_ADMIN,		do_addcommand,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "advance",			CMDTYPE_ADMIN,		do_advance,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "aedit",				CMDTYPE_OLC,		do_aedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "affects",			CMDTYPE_INFO,		do_affects,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "affix",				CMDTYPE_OBJECT,		do_affix,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "afk",				CMDTYPE_OOC,		do_afk,					POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "alevel",				CMDTYPE_ADMIN,		do_alevel,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "alias",				CMDTYPE_INFO,		do_alias,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "alist",				CMDTYPE_OLC,		do_alist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "allow",				CMDTYPE_ADMIN,		do_allow,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "aload",				CMDTYPE_ADMIN,		do_aload,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "ambush",				CMDTYPE_COMBAT,		do_ambush,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "announcements",		CMDTYPE_COMM,		do_announcements,		POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "apdump",				CMDTYPE_OLC,		do_apdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "apedit",				CMDTYPE_OLC,		do_apedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "aplist",				CMDTYPE_OLC,		do_aplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "arealinks",			CMDTYPE_IMMORTAL,	do_arealinks,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "areset",				CMDTYPE_ADMIN,		do_areset,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "asave",				CMDTYPE_OLC,		do_asave_new,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "asearch",			CMDTYPE_OLC,		do_asearch,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "assignhelper",		CMDTYPE_ADMIN,		do_assignhelper,		POS_RESTING,	STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "at",					CMDTYPE_IMMORTAL,	do_at,					POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	false },
	{ "attach",				CMDTYPE_OBJECT,		do_attach,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "auction",		    CMDTYPE_INFO,		do_auction,				POS_SLEEPING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "autolist",			CMDTYPE_INFO,		do_toggle,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "autosetname",		CMDTYPE_IMMORTAL,	do_autosetname,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "autowar",			CMDTYPE_ADMIN,		do_autowar,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "backstab",			CMDTYPE_COMBAT,		do_backstab,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "ban",				CMDTYPE_ADMIN,		do_ban,					POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "bank",				CMDTYPE_INFO,		do_bank,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bar",				CMDTYPE_MOVE,		do_bar,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bash",				CMDTYPE_COMBAT,		do_bash,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "behead",				CMDTYPE_COMBAT,		do_behead,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "berserk",			CMDTYPE_COMBAT,		do_berserk,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bind",				CMDTYPE_OBJECT,		do_bind,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bite",				CMDTYPE_RACIAL,		do_bite,			 	POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "blackjack",			CMDTYPE_COMBAT,		do_blackjack,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "blow",				CMDTYPE_OBJECT,		do_blow,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bomb",				CMDTYPE_OBJECT,		do_bomb,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "boost",				CMDTYPE_ADMIN,		do_boost,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "botter",				CMDTYPE_ADMIN,		do_botter,				POS_RESTING,	STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "bpedit",				CMDTYPE_OLC,		do_bpedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "bplist",				CMDTYPE_OLC,		do_bplist,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "bpshow",				CMDTYPE_OLC,		do_bpshow,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "brandish",			CMDTYPE_OBJECT,		do_brandish,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "breathe",			CMDTYPE_RACIAL,		do_breathe,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "brew",				CMDTYPE_OBJECT,		do_brew,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "bs",					CMDTYPE_COMBAT,		do_backstab,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "bsedit",				CMDTYPE_OLC,		do_bsedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "bslist",				CMDTYPE_OLC,		do_bslist,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "bsshow",				CMDTYPE_OLC,		do_bsshow,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "build",				CMDTYPE_IMMORTAL,	do_build,				POS_RESTING,	STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "buy",				CMDTYPE_OBJECT,		do_buy,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "cast",				CMDTYPE_NONE,		do_cast,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "catchup",			CMDTYPE_INFO,		do_catchup,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "challenge",			CMDTYPE_COMBAT,		do_challenge,			POS_SLEEPING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "changes",			CMDTYPE_INFO,		do_changes,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "channels",			CMDTYPE_NONE,		do_channels,		    POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "charge",				CMDTYPE_COMBAT,		do_charge,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "chat",				CMDTYPE_INFO,		do_chat,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "church",				CMDTYPE_INFO,		do_church,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "circle",				CMDTYPE_COMBAT,		do_circle,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "classes",			CMDTYPE_INFO,		do_classes,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "clear",				CMDTYPE_NONE,		do_clear,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "clone",				CMDTYPE_IMMORTAL,	do_clone,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "close",				CMDTYPE_OBJECT,		do_close,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "clsedit",			CMDTYPE_OLC,		do_clsedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "clslist",			CMDTYPE_OLC,		do_clslist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "clsshow",			CMDTYPE_OLC,		do_clsshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "cmdedit",			CMDTYPE_OLC,		do_cmdedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL,	true,	true },
	{ "cmdlist", 			CMDTYPE_OLC,		do_cmdlist, 			POS_DEAD, 		STAFF_IMPLEMENTOR, 	LOG_NORMAL, true, 	true },
	{ "collapse",			CMDTYPE_OBJECT,		do_collapse,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "color",				CMDTYPE_INFO,		do_colour,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "colour",				CMDTYPE_INFO,		do_colour,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "combine",			CMDTYPE_OBJECT,		do_combine,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "commands",			CMDTYPE_INFO,		do_commands,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "conceal",			CMDTYPE_OBJECT,		do_conceal,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "config",				CMDTYPE_INFO,		do_toggle,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "consider",			CMDTYPE_COMBAT,		do_consider,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "consume",		 	CMDTYPE_RACIAL,		do_consume,			 	POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "convert",			CMDTYPE_INFO,		do_convert,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "corpsedit",			CMDTYPE_OLC,		do_corpsedit,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	false },
	{ "corpselist",			CMDTYPE_OLC,		do_corpselist,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL,	true,	false },
	{ "corpseshow",			CMDTYPE_OLC,		do_corpseshow,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL,	true,	false },
	{ "count",				CMDTYPE_NONE,		do_count,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "credits",			CMDTYPE_INFO,		do_credits,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "ct",					CMDTYPE_COMM,		do_chtalk,				POS_DEAD,		STAFF_PLAYER,		LOG_NEVER,	true,	true },
	{ "danger",				CMDTYPE_INFO,		do_danger,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "delet",				CMDTYPE_NONE,		do_delet,				POS_DEAD,		STAFF_PLAYER,		LOG_ALWAYS,	false,	false },
	{ "delete",				CMDTYPE_NONE,		do_delete,				POS_STANDING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "deny",				CMDTYPE_ADMIN,		do_deny,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "deposit",			CMDTYPE_OBJECT,		do_deposit,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "description",		CMDTYPE_INFO,		do_description,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "dice",				CMDTYPE_INFO,		do_dice,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "dig",				CMDTYPE_OBJECT,		do_dig,					POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "dirt",				CMDTYPE_COMBAT,		do_dirt,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "disarm",				CMDTYPE_COMBAT,		do_disarm,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "disconnect",			CMDTYPE_ADMIN,		do_disconnect,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "disembark",			CMDTYPE_MOVE,		do_disembark,		 	POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "dislink",			CMDTYPE_OLC,		do_dislink,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "dismount",			CMDTYPE_MOVE,		do_dismount,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "dngedit",			CMDTYPE_OLC,		do_dngedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "dnglist",			CMDTYPE_OLC,		do_dnglist,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "dngshow",			CMDTYPE_OLC,		do_dngshow,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, true,	true },
	{ "donate",				CMDTYPE_OBJECT,		do_donate,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "down",				CMDTYPE_MOVE,		do_down,				POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "dpdump",				CMDTYPE_OLC,		do_dpdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "dpedit",				CMDTYPE_OLC,		do_dpedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "dplist",				CMDTYPE_OLC,		do_dplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "drink",				CMDTYPE_OBJECT,		do_drink,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "drop",				CMDTYPE_OBJECT,		do_drop,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "dump",				CMDTYPE_ADMIN,		do_dump,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "dungeon",			CMDTYPE_OLC,		do_dungeon,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "east",				CMDTYPE_NONE,		do_east,				POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "eat",				CMDTYPE_OBJECT,		do_eat,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "edit",				CMDTYPE_OLC,		do_olc,					POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "email",				CMDTYPE_OOC,		do_email,				POS_DEAD,		STAFF_PLAYER,		LOG_ALWAYS,	true,	true },
	{ "emote",				CMDTYPE_COMM,		do_emote,				POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "enter",		 		CMDTYPE_MOVE,		do_enter,			 	POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "envenom",			CMDTYPE_OBJECT,		do_envenom,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "equipment",			CMDTYPE_INFO,		do_equipment,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "evasion",			CMDTYPE_COMBAT,		do_evasion,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "evict",			 	CMDTYPE_ADMIN,		do_evict,				POS_RESTING,	STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "examine",			CMDTYPE_INFO,		do_examine,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "exits",				CMDTYPE_INFO,		do_exits,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "expand",				CMDTYPE_OBJECT,		do_expand,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "extinguish",			CMDTYPE_OBJECT,		do_extinguish,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL,	true,	false },
	{ "fade",				CMDTYPE_MOVE,		do_fade,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "feed",				CMDTYPE_RACIAL,		do_bite,			 	POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "feign",				CMDTYPE_COMBAT,		do_feign,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "fill",				CMDTYPE_OBJECT,		do_fill,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "flag",				CMDTYPE_INFO,		do_flag,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "flame",				CMDTYPE_COMM,		do_flame,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "flee",				CMDTYPE_COMBAT,		do_flee,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "follow",				CMDTYPE_MOVE,		do_follow,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "force",				CMDTYPE_ADMIN,		do_force,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	false },
	{ "formstate",			CMDTYPE_COMBAT,		do_formstate,			POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "freeze",				CMDTYPE_ADMIN,		do_freeze,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "gecho",				CMDTYPE_IMMORTAL,	do_echo,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "get",				CMDTYPE_OBJECT,		do_get,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "give",				CMDTYPE_OBJECT,		do_give,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "go",					CMDTYPE_MOVE,		do_enter,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "gohome",				CMDTYPE_MOVE,		do_gohome,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "gold",				CMDTYPE_INFO,		do_worth,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "gossip",				CMDTYPE_COMM,		do_gossip,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "goto",				CMDTYPE_IMMORTAL,	do_goto,		        POS_DEAD,       STAFF_IMMORTAL,		LOG_ALWAYS,	true,	false },
	{ "goxy",				CMDTYPE_IMMORTAL,	do_goxy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	false },
	{ "gq",					CMDTYPE_IMMORTAL,	do_gq,					POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "group",				CMDTYPE_INFO,		do_group,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "gtell",				CMDTYPE_COMM,		do_gtell,				POS_DEAD,		STAFF_PLAYER,		LOG_NEVER,	true,	true },
	{ "hands",				CMDTYPE_NONE,		do_hands,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "hedit",				CMDTYPE_OLC,		do_hedit,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "help",				CMDTYPE_INFO,		do_help,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "helper",				CMDTYPE_COMM,		do_helper,			 	POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "hide",				CMDTYPE_MOVE,		do_hide,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "hints",				CMDTYPE_COMM,		do_hints,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "hit",				CMDTYPE_COMBAT,		do_kill,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "hitch",				CMDTYPE_MOVE,		do_hitch,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "hold",				CMDTYPE_OBJECT,		do_wear,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "hold",				CMDTYPE_OBJECT,		do_wear,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "holdup",		        CMDTYPE_COMBAT,		do_holdup,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "holyaura",			CMDTYPE_IMMORTAL,	do_holyaura,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "holylight",			CMDTYPE_IMMORTAL,	do_holylight,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "holypersona",		CMDTYPE_IMMORTAL,	do_holypersona,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "holywarp",			CMDTYPE_IMMORTAL,	do_holywarp,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "house",			 	CMDTYPE_INFO,		do_house,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "housemove",			CMDTYPE_ADMIN,		do_housemove,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "hunt",				CMDTYPE_MOVE,		do_hunt,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "ifchecks",			CMDTYPE_OLC,		do_ifchecks,		    POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "ignite",				CMDTYPE_OBJECT,		do_ignite,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL,	true,	false },
	{ "ignore",				CMDTYPE_COMM,		do_ignore,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	true },
	{ "imbue",				CMDTYPE_OBJECT,		do_imbue,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "immflag",			CMDTYPE_IMMORTAL,	do_immflag,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "immortalise",		CMDTYPE_ADMIN,		do_immortalise,			POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	false },
	{ "immstrike",			CMDTYPE_IMMORTAL,	do_immstrike,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "immtalk",			CMDTYPE_COMM,		do_immtalk,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "imotd",				CMDTYPE_INFO,		do_imotd,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "incognito",			CMDTYPE_IMMORTAL,	do_incognito,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	false },
	{ "infuse",				CMDTYPE_OBJECT,		do_infuse,				POS_DEAD,		STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "ink",				CMDTYPE_OBJECT,		do_ink,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "inspect",	 		CMDTYPE_OBJECT,		do_inspect,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "instance",			CMDTYPE_OLC,		do_instance,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "intimidate",			CMDTYPE_COMBAT,		do_intimidate,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "intone",				CMDTYPE_OBJECT,		do_intone,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "inventory",			CMDTYPE_INFO,		do_inventory,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "invis",				CMDTYPE_IMMORTAL,	do_invis,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, false,	false },
	{ "ipdump",				CMDTYPE_OLC,		do_ipdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "ipedit",				CMDTYPE_OLC,		do_ipedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "iplist",				CMDTYPE_OLC,		do_iplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "judge",				CMDTYPE_INFO,		do_judge,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "junk",				CMDTYPE_IMMORTAL,	do_junk,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "keep",				CMDTYPE_OBJECT,		do_keep,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "kick",				CMDTYPE_COMBAT,		do_kick,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "kill",				CMDTYPE_COMBAT,		do_kill,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "knock",				CMDTYPE_MOVE,		do_knock,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "land",				CMDTYPE_MOVE,		do_land,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "lead",				CMDTYPE_MOVE,		do_lead,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "liqedit",			CMDTYPE_OLC,		do_liqedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "liqlist",			CMDTYPE_OLC,		do_liqlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "list",				CMDTYPE_INFO,		do_list,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "load",				CMDTYPE_OLC,		do_load,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "lock",				CMDTYPE_MOVE,		do_lock,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "locker",				CMDTYPE_OOC,		do_locker,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "log",				CMDTYPE_ADMIN,		do_log,					POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS, false,	true },
	{ "look",				CMDTYPE_INFO,		do_look,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "lore",				CMDTYPE_OBJECT,		do_lore,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "mail",				CMDTYPE_OBJECT,		do_mail,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "matedit",			CMDTYPE_OLC,		do_matedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "matlist",			CMDTYPE_OLC,		do_matlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "matshow",			CMDTYPE_OLC,		do_matshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mccp",			 	CMDTYPE_INFO,		do_compress,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "mcopy",				CMDTYPE_OLC,		do_mcopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "medit",				CMDTYPE_OLC,		do_medit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "memory",				CMDTYPE_ADMIN,		do_memory,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "mission",			CMDTYPE_NONE,		do_mission,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "mlevel",				CMDTYPE_OLC,		do_mlevel,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mlist",				CMDTYPE_OLC,		do_mlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mob",				CMDTYPE_NONE,		do_mob,					POS_DEAD,		STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "motd",				CMDTYPE_INFO,		do_motd,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "mount",				CMDTYPE_MOVE,		do_mount,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "mpcopy",				CMDTYPE_OLC,		do_mpcopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "mpdelete",			CMDTYPE_OLC,		do_mpdelete,			POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "mpdump",				CMDTYPE_OLC,		do_mpdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "mpedit",				CMDTYPE_OLC,		do_mpedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mplist",				CMDTYPE_OLC,		do_mplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mpstat",				CMDTYPE_OLC,		do_mpstat,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "mshow",				CMDTYPE_OLC,		do_mshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "murder",				CMDTYPE_COMBAT,		do_kill,				POS_FIGHTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "music",				CMDTYPE_NONE,		do_music,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "mwhere",				CMDTYPE_OLC,		do_mwhere,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "mxptest",			CMDTYPE_ADMIN,		do_mxptest,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	false,	true },
	{ "ne",					CMDTYPE_MOVE,		do_northeast,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "newlock",			CMDTYPE_ADMIN,		do_newlock,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "news",				CMDTYPE_INFO,		do_news,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "nochannels",			CMDTYPE_ADMIN,		do_nochannels,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "north",				CMDTYPE_MOVE,		do_north,				POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "northeast",			CMDTYPE_MOVE,		do_northeast,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "northwest",			CMDTYPE_MOVE,		do_northwest,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "note",				CMDTYPE_COMM,		do_note,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "notell",				CMDTYPE_ADMIN,		do_notell,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "notify",				CMDTYPE_COMM,		do_notify,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "nw",					CMDTYPE_MOVE,		do_northwest,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "ocopy",				CMDTYPE_OLC,		do_ocopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "oedit",				CMDTYPE_OLC,		do_oedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "olevel",				CMDTYPE_OLC,		do_olevel,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "olist",				CMDTYPE_OLC,		do_olist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "ooc",				CMDTYPE_COMM,		do_ooc,					POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "opcopy",				CMDTYPE_OLC,		do_opcopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "opdelete",			CMDTYPE_OLC,		do_opdelete,			POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "opdump",				CMDTYPE_OLC,		do_opdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "opedit",				CMDTYPE_OLC,		do_opedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "open",				CMDTYPE_OBJECT,		do_open,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "oplist",				CMDTYPE_OLC,		do_oplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "opstat",				CMDTYPE_OLC,		do_opstat,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "order",				CMDTYPE_NONE,		do_order,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "oshow",				CMDTYPE_OLC,		do_oshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "otransfer",			CMDTYPE_OLC,		do_otransfer,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "owhere",				CMDTYPE_OLC,		do_owhere,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "page",				CMDTYPE_OBJECT,		do_page,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "password",			CMDTYPE_OOC,		do_password,			POS_DEAD,		STAFF_PLAYER,		LOG_ALWAYS,	true,	true },
	{ "pdelete",			CMDTYPE_OLC,		do_pdelete,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "peace",				CMDTYPE_IMMORTAL,	do_peace,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "pecho",				CMDTYPE_COMM,		do_pecho,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "pedit",				CMDTYPE_OLC,		do_pedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "permban",			CMDTYPE_ADMIN,		do_permban,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "pick",				CMDTYPE_MOVE,		do_pick,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "pinquiry",			CMDTYPE_OLC,		do_pinquiry,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "pk",					CMDTYPE_COMBAT,		do_pk,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "plant",				CMDTYPE_OBJECT,		do_plant,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "play",				CMDTYPE_NONE,		do_play,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "plist",				CMDTYPE_OLC,		do_plist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "poofin",				CMDTYPE_MOVE,		do_bamfin,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "poofout",			CMDTYPE_MOVE,		do_bamfout,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "pour",				CMDTYPE_OBJECT,		do_pour,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "practice",			CMDTYPE_NONE,		do_practice,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "project",			CMDTYPE_OLC,		do_project,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "prompt",				CMDTYPE_INFO,		do_prompt,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "pshow",				CMDTYPE_OLC,		do_pshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "pull",				CMDTYPE_OBJECT,		do_pull,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "purge",				CMDTYPE_IMMORTAL,	do_purge,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "pursuit",			CMDTYPE_COMBAT,		do_pursuit,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "push",				CMDTYPE_OBJECT,		do_push,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "put",				CMDTYPE_OBJECT,		do_put,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "qlist",				CMDTYPE_COMM,		do_qlist,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "quaff",				CMDTYPE_OBJECT,		do_quaff,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "quiet",				CMDTYPE_COMM,		do_quiet,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "quit",				CMDTYPE_OOC,		do_quit,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "quote",				CMDTYPE_COMM,		do_quote,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "raceedit",			CMDTYPE_OLC,		do_raceedit,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "racelist",			CMDTYPE_OLC,		do_racelist,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "raceshow",			CMDTYPE_OLC,		do_raceshow,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "rack",				CMDTYPE_COMBAT,		do_rack,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rcopy",				CMDTYPE_OLC,		do_rcopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "read",				CMDTYPE_INFO,		do_read,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "readycheck",			CMDTYPE_COMBAT,		do_readycheck,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "reboo",				CMDTYPE_ADMIN,		do_reboo,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, false,	true },
	{ "reboot",				CMDTYPE_ADMIN,		do_reboot,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	false,	true },
	{ "recall",				CMDTYPE_MOVE,		do_recall,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "recite",				CMDTYPE_OBJECT,		do_recite,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "reckonin",			CMDTYPE_ADMIN,		do_reckonin,		    POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	false,	true },
	{ "reckoning",			CMDTYPE_ADMIN,		do_reckoning,			POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "redit",				CMDTYPE_OLC,		do_redit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "regions",			CMDTYPE_OLC,		do_regions,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "remcommand",			CMDTYPE_ADMIN,		do_remcommand,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "remove",				CMDTYPE_OBJECT,		do_remove,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "renew",				CMDTYPE_OBJECT,		do_renew,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "repair",				CMDTYPE_OBJECT,		do_repair,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "repedit",			CMDTYPE_OLC,		do_repedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "replay",				CMDTYPE_COMM,		do_replay,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NEVER,	true,	true },
	{ "replist",			CMDTYPE_OLC,		do_replist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "reply",				CMDTYPE_COMM,		do_reply,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "report",				CMDTYPE_INFO,		do_report,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "repshow",			CMDTYPE_OLC,		do_repshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "reputations",		CMDTYPE_INFO,		do_reputations,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rescue",				CMDTYPE_COMBAT,		do_rescue,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "reserved",			CMDTYPE_ADMIN,		do_reserved,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "resets",				CMDTYPE_OLC,		do_resets,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "rest",				CMDTYPE_MOVE,		do_rest,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "restore",			CMDTYPE_IMMORTAL,	do_restore,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "restring",			CMDTYPE_OBJECT,		do_restring,		    POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "resurrect",			CMDTYPE_NONE,		do_resurrect,			POS_SLEEPING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "return",				CMDTYPE_NONE,		do_return,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "reverie",			CMDTYPE_MOVE,		do_reverie,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rip",				CMDTYPE_OBJECT,		do_rip,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rjunk",				CMDTYPE_OLC,		do_rjunk,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "rlist",				CMDTYPE_OLC,		do_rlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "rpcopy",				CMDTYPE_OLC,		do_rpcopy,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "rpdelete",			CMDTYPE_OLC,		do_rpdelete,			POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "rpdump",				CMDTYPE_OLC,		do_rpdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "rpedit",				CMDTYPE_OLC,		do_rpedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "rplist",				CMDTYPE_OLC,		do_rplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "rpstat",				CMDTYPE_OLC,		do_rpstat,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "rshow",		 		CMDTYPE_OLC,		do_rshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "ruboff",				CMDTYPE_OBJECT,		do_ruboff,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rules",				CMDTYPE_INFO,		do_rules,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "rwhere",				CMDTYPE_OLC,		do_rwhere,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "sacrifice",			CMDTYPE_OBJECT,		do_sacrifice,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sadd",				CMDTYPE_ADMIN,		do_sadd,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "save",				CMDTYPE_OOC,		do_save,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "say",				CMDTYPE_COMM,		do_say,					POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "sayto",				CMDTYPE_COMM,		do_sayto,				POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },	// NIB : 20070121
	{ "scan",				CMDTYPE_INFO,		do_scan,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "score",				CMDTYPE_INFO,		do_score,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "scribe",				CMDTYPE_OBJECT,		do_scribe,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "scroll",				CMDTYPE_OOC,		do_scroll,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "scry",				CMDTYPE_INFO,		do_scry,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sdelete",			CMDTYPE_ADMIN,		do_sdelete,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "sdemote",			CMDTYPE_ADMIN,		do_sdemote,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "sduty",				CMDTYPE_ADMIN,		do_sduty,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "se",					CMDTYPE_MOVE,		do_southeast,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "seal",				CMDTYPE_OBJECT,		do_seal,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "search",				CMDTYPE_INFO,		do_search,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "secondary",			CMDTYPE_OBJECT,		do_secondary,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sectoredit",			CMDTYPE_OLC,		do_sectoredit,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "sectorlist",			CMDTYPE_OLC,		do_sectorlist,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "sectorshow",			CMDTYPE_OLC,		do_sectorshow,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "sell",				CMDTYPE_OBJECT,		do_sell,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "set",				CMDTYPE_IMMORTAL,	do_set,					POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "setclass", 			CMDTYPE_INFO,		do_setclass,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "sgedit",				CMDTYPE_OLC,		do_sgedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "sglist",				CMDTYPE_OLC,		do_sglist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "sgshow",				CMDTYPE_OLC,		do_sgshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "shape",  			CMDTYPE_RACIAL,		do_shape,			 	POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "shedit",				CMDTYPE_OLC,		do_shedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "shift",				CMDTYPE_RACIAL,		do_shift,			 	POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "ship",				CMDTYPE_INFO,		do_ship,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL,  true,	false },
	{ "ships",				CMDTYPE_INFO,		do_ships,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "shlist",				CMDTYPE_OLC,		do_shlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "shoot",				CMDTYPE_COMBAT,		do_shoot,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "showdamage",			CMDTYPE_INFO,		do_showdamage,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "shshow",				CMDTYPE_OLC,		do_shshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "shutdow",			CMDTYPE_ADMIN,		do_shutdow,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_NORMAL, false,	true },
	{ "shutdown",			CMDTYPE_ADMIN,		do_shutdown,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	false,	true },
	{ "sip",				CMDTYPE_OBJECT,		do_sip,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sit",				CMDTYPE_MOVE,		do_sit,					POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "skedit",				CMDTYPE_OLC,		do_skedit,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "skills",				CMDTYPE_INFO,		do_skills,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "sklist",				CMDTYPE_OLC,		do_sklist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "skshow",				CMDTYPE_OLC,		do_skshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "skull",				CMDTYPE_OBJECT,		do_skull,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "slay",				CMDTYPE_IMMORTAL,	do_slay,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	false },
	{ "sleep",				CMDTYPE_MOVE,		do_sleep,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "slist",				CMDTYPE_ADMIN,		do_slist,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "slit",				CMDTYPE_COMBAT,		do_slit,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "smite",				CMDTYPE_COMBAT,		do_smite,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sneak",				CMDTYPE_MOVE,		do_sneak,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "socials",			CMDTYPE_COMM,		do_socials,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "sockets",			CMDTYPE_ADMIN,		do_sockets,				POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "songedit",			CMDTYPE_OLC,		do_songedit,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "songlist",			CMDTYPE_OLC,		do_songlist,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "songshow",			CMDTYPE_OLC,		do_songshow,			POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "south",				CMDTYPE_NONE,		do_south,				POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "southeast",			CMDTYPE_MOVE,		do_southeast,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "southwest",			CMDTYPE_MOVE,		do_southwest,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "spawntreasuremap",	CMDTYPE_IMMORTAL,	do_spawntreasuremap,	POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "spells",				CMDTYPE_INFO,		do_spells,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "split",				CMDTYPE_OBJECT,		do_split,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "spromote",			CMDTYPE_ADMIN,		do_spromote,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "ssupervisor",		CMDTYPE_ADMIN,		do_ssupervisor,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "staff",				CMDTYPE_ADMIN,		do_staff,				POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },
	{ "stake",				CMDTYPE_COMBAT,		do_stake,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "stand",				CMDTYPE_MOVE,		do_stand,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "startinvasion",		CMDTYPE_ADMIN,		do_startinvasion,		POS_DEAD,		STAFF_SUPREMACY,	LOG_ALWAYS,	true,	true },
	{ "stat",				CMDTYPE_IMMORTAL,	do_stat,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	false },
	{ "stats",				CMDTYPE_INFO,		do_stats,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "steal",				CMDTYPE_OBJECT,		do_steal,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "string",				CMDTYPE_IMMORTAL,	do_string,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "survey",			 	CMDTYPE_INFO,		do_survey,			 	POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "sw",					CMDTYPE_MOVE,		do_southwest,			POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	false,	false },
	{ "switch",				CMDTYPE_IMMORTAL,	do_switch,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	true },
	{ "tailkick",			CMDTYPE_COMBAT,		do_tail_kick,			POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "take",				CMDTYPE_OBJECT,		do_get,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "takeoff",			CMDTYPE_MOVE,		do_takeoff,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "tedit",				CMDTYPE_OLC,		do_tedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "tell",				CMDTYPE_COMM,		do_tell,				POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "tells",				CMDTYPE_COMM,		do_tells,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "testport",			CMDTYPE_ADMIN,		do_testport,			POS_DEAD,		STAFF_IMPLEMENTOR,	LOG_ALWAYS,	true,	true },	// 20140521 Nibs
	{ "throw",				CMDTYPE_COMBAT,		do_throw,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "time",				CMDTYPE_INFO,		do_time,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "title",				CMDTYPE_INFO,		do_title,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "tlist",				CMDTYPE_OLC,		do_tlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "toggle",				CMDTYPE_INFO,		do_toggle,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "token",				CMDTYPE_IMMORTAL,	do_token,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "touch",				CMDTYPE_OBJECT,		do_touch,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "toxins",				CMDTYPE_RACIAL,		do_toxins,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "tpdump",				CMDTYPE_OLC,		do_tpdump,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "tpedit",				CMDTYPE_OLC,		do_tpedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "tplist",				CMDTYPE_OLC,		do_tplist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "tpstat",				CMDTYPE_OLC,		do_tpstat,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NEVER,	true,	true },
	{ "train",				CMDTYPE_NONE,		do_train,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "trample",			CMDTYPE_COMBAT,		do_trample,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "trance",				CMDTYPE_COMBAT,		do_trance,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "transfer",			CMDTYPE_IMMORTAL,	do_transfer,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	false },
	{ "triggers",			CMDTYPE_OLC,		do_triggers,			POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "tshow",				CMDTYPE_OLC,		do_tshow,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "turn",				CMDTYPE_COMBAT,		do_turn,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unalias",			CMDTYPE_NONE,		do_unalias,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "ungroup",			CMDTYPE_NONE,		do_ungroup,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unhitch",			CMDTYPE_MOVE,		do_unhitch,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, false,	false },
	{ "uninvis",			CMDTYPE_IMMORTAL,	do_uninvis,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	false },
	{ "unlead",				CMDTYPE_MOVE,		do_unlead,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unlock",				CMDTYPE_MOVE,		do_unlock,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unread",				CMDTYPE_COMM,		do_unread,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unrestring",			CMDTYPE_OBJECT,		do_unrestring,			POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "unyoke",				CMDTYPE_MOVE,		do_unyoke,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL,	true,	false },
	{ "up",					CMDTYPE_MOVE,		do_up,					POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "use",				CMDTYPE_OBJECT,		do_use,					POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "value",				CMDTYPE_OBJECT,		do_value,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "version",			CMDTYPE_INFO,		do_showversion,			POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "visible",			CMDTYPE_NONE,		do_visible,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "vislist",			CMDTYPE_COMM,		do_vislist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "vledit",				CMDTYPE_OLC,		do_vledit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "vlinks",				CMDTYPE_OLC,		do_vlinks,				POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	true },
	{ "vnum",				CMDTYPE_OLC,		do_vnum,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "wake",				CMDTYPE_MOVE,		do_wake,				POS_SLEEPING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "war",				CMDTYPE_COMM,		do_war,					POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "warcry",				CMDTYPE_COMBAT,		do_warcry,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "warp",				CMDTYPE_MOVE,		do_warp,				POS_RESTING,	STAFF_PLAYER,		LOG_ALWAYS,	true,	false },
	{ "wear",				CMDTYPE_OBJECT,		do_wear,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "weather",			CMDTYPE_INFO,		do_weather,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "weave",				CMDTYPE_COMBAT,		do_weave,				POS_FIGHTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "wedit",				CMDTYPE_OLC,		do_wedit,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "west",				CMDTYPE_MOVE,		do_west,				POS_STANDING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "where",				CMDTYPE_INFO,		do_where,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "whisper",			CMDTYPE_COMM,		do_whisper,				POS_RESTING,	STAFF_PLAYER,		LOG_NEVER,	true,	false },
	{ "who",				CMDTYPE_INFO,		do_who_new,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "whois",				CMDTYPE_INFO,		do_whois,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "wield",				CMDTYPE_OBJECT,		do_wear,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "wimpy",				CMDTYPE_COMBAT,		do_wimpy,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "wizhelp",			CMDTYPE_INFO,		do_wizhelp,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "wizinvis",			CMDTYPE_IMMORTAL,	do_invis,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "wizlist",			CMDTYPE_INFO,		do_wizlist,				POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, true,	true },
	{ "wizlock",			CMDTYPE_ADMIN,		do_wizlock,				POS_DEAD,		STAFF_CREATOR,		LOG_ALWAYS,	true,	true },
	{ "wiznet",				CMDTYPE_IMMORTAL,	do_wiznet,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "wlist",				CMDTYPE_OLC,		do_wlist,				POS_DEAD,		STAFF_IMMORTAL,		LOG_NORMAL, true,	true },
	{ "write",				CMDTYPE_OBJECT,		do_write,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "yell",				CMDTYPE_COMM,		do_yell,				POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "yoke",				CMDTYPE_MOVE,		do_yoke,				POS_STANDING,	STAFF_PLAYER,		LOG_NORMAL,	true,	false },
	{ "zap",				CMDTYPE_OBJECT,		do_zap,					POS_RESTING,	STAFF_PLAYER,		LOG_NORMAL, true,	false },
	{ "zecho",				CMDTYPE_COMM,		do_zecho,				POS_DEAD,		STAFF_ASCENDANT,	LOG_ALWAYS,	true,	false },
	{ "zot",	 			CMDTYPE_IMMORTAL,	do_zot,					POS_DEAD,		STAFF_IMMORTAL,		LOG_ALWAYS,	true,	false },
    { "",					CMDTYPE_NONE,		NULL,					POS_DEAD,		STAFF_PLAYER,		LOG_NORMAL, false,	false }
};

bool forced_command = false;	// 20070511NIB: Used to prevent forces to do any restricted command

static void __collect_verbs_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
	ITERATOR tit, pit;
	ROOM_INDEX_DATA *source;
	TOKEN_DATA *token;
	PROG_LIST *prg;
	int slot = TRIGSLOT_VERB;

	if(room->source) {
		source = room->source;
	} else {
		source = room;
	}

	script_room_addref(room);

	// Check for tokens FIRST
	iterator_start(&tit, room->ltokens);
	while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
		if( token->pIndexData->progs ) {
			script_token_addref(token);
			script_destructed = false;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
					execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
				}
			}
			iterator_stop(&pit);
			script_token_remref(token);
		}
	}
	iterator_stop(&tit);

	if(source->progs->progs) {
		script_destructed = false;
		iterator_start(&pit, source->progs->progs[slot]);
		while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
			if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
				execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
			}
		}
		iterator_stop(&pit);
	}
	script_room_remref(room);
}

static void __collect_verbs_mob(CHAR_DATA *ch, CHAR_DATA *mob)
{
	ITERATOR tit, pit;
	TOKEN_DATA *token;
	PROG_LIST *prg;
	int slot = TRIGSLOT_VERB;

	if (IS_NPC(mob))
		script_mobile_addref(mob);

	// Check for tokens FIRST
	iterator_start(&tit, mob->ltokens);
	while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
		if(token->pIndexData->progs) {
			script_token_addref(token);
			script_destructed = false;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
					execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
				}
			}
			iterator_stop(&pit);
			script_token_remref(token);
		}
	}
	iterator_stop(&tit);

	if (ch != mob && IS_NPC(mob))
	{
		if(mob->pIndexData->progs) {
			script_destructed = false;
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
					execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
				}
			}
			iterator_stop(&pit);
		}
	}

	if (IS_NPC(mob))
		script_mobile_remref(mob);
}

static void __collect_verbs_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
	ITERATOR tit, pit;
	TOKEN_DATA *token;
	PROG_LIST *prg;
	int slot = TRIGSLOT_VERB;

	script_object_addref(obj);

	// Check for tokens FIRST
	iterator_start(&tit, obj->ltokens);
	while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
		if( token->pIndexData->progs ) {
			script_token_addref(token);
			script_destructed = false;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
					execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
				}
			}
			iterator_stop(&pit);
			script_token_remref(token);
		}
	}
	iterator_stop(&tit);

	if(obj->pIndexData->progs) {
		script_destructed = false;
		iterator_start(&pit, obj->pIndexData->progs[slot]);
		while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
			if (is_trigger_type(prg->trig_type,TRIG_SHOWCOMMANDS)) {
				execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL,NULL,TRIG_SHOWCOMMANDS,0,0,0,0,0);
			}
		}
		iterator_stop(&pit);
	}
	script_object_remref(obj);
}

void collect_verbs(CHAR_DATA *ch)
{
	ITERATOR it;
	OBJ_DATA *obj;
	CHAR_DATA *mob;

	// Self
	__collect_verbs_mob(ch, ch);

	// Here
	__collect_verbs_room(ch, ch->in_room);

	// Mobiles
	iterator_start(&it, ch->in_room->lpeople);
	while((mob = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (mob != ch)
			__collect_verbs_mob(ch, mob);
	}
	iterator_stop(&it);

	// Inventory
	iterator_start(&it, ch->lcarrying);
	while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
	{
		__collect_verbs_obj(ch, obj);
	}
	iterator_stop(&it);

	// Room contents
	iterator_start(&it, ch->in_room->lcontents);
	while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
	{
		__collect_verbs_obj(ch, obj);
	}
	iterator_stop(&it);
}

bool check_verbs(CHAR_DATA *ch, char *command, char *argument)
{
	char buf[MIL], *p;
	ITERATOR tit, pit;
	TOKEN_DATA *token;
	OBJ_DATA *obj;
	CHAR_DATA *mob;
	PROG_LIST *prg;
	int slot;
	int ret_val = PRET_NOSCRIPT, ret; // @@@NIB Default for a trigger loop is NO SCRIPT

	log_stringf("check_verbs: ch(%s), command(%s), argument(%s)", ch->name, command, argument);

	slot = TRIGSLOT_VERB;

	// Check for tokens FIRST
	iterator_start(&tit, ch->ltokens);
	while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
		if(token->pIndexData->progs) {
			log_stringf("check_verbs: ch(%s) token(%ld, %s)", ch->name, token->pIndexData->vnum, token->name);
			script_token_addref(token);
			script_destructed = false;
			iterator_start(&pit, token->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				log_stringf("check_verbs: ch(%s) token(%ld, %s) trigger(%s, %s)", ch->name, token->pIndexData->vnum, token->name, trigger_name(prg->trig_type), prg->trig_phrase);
				if (is_trigger_type(prg->trig_type,TRIG_VERBSELF) && !str_prefix(command, prg->trig_phrase)) {
					log_stringf("check_verbs: ch(%s) token(%ld, %s) trigger(%s, %s) executing", ch->name, token->pIndexData->vnum, token->name, trigger_name(prg->trig_type), prg->trig_phrase);
					ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,argument,prg->trig_phrase,TRIG_VERBSELF,0,0,0,0,0);
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);

						script_token_remref(token);
						return ret;
					}

				}
			}
			iterator_stop(&pit);
			script_token_remref(token);
		}
	}
	iterator_stop(&tit);
	if( ret_val != PRET_NOSCRIPT ) return true;

	p = one_argument(argument,buf);
//	if(!str_cmp(buf,"here")) {
		ROOM_INDEX_DATA *room = ch->in_room;
		ROOM_INDEX_DATA *source;
		//bool isclone;

		if(room->source) {
			source = room->source;
			//isclone = true;
			//uid[0] = room->id[0];
			//uid[1] = room->id[1];
		} else {
			source = room;
			//isclone = false;
		}

		script_room_addref(room);

		// Check for tokens FIRST
		iterator_start(&tit, room->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = false;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if ((is_trigger_type(prg->trig_type,TRIG_VERB) || is_trigger_type(prg->trig_type,TRIG_VERBSELF)) && !str_prefix(command, prg->trig_phrase)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&tit);
							iterator_stop(&pit);

							script_token_remref(token);
							script_room_remref(room);
							return ret;
						}

					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && source->progs->progs) {
			script_destructed = false;
			iterator_start(&pit, source->progs->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
					ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
				} else if (is_trigger_type(prg->trig_type,TRIG_VERBSELF) && !str_prefix(command,prg->trig_phrase)) {
					ret = execute_script(prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL,argument,prg->trig_phrase,TRIG_VERBSELF,0,0,0,0,0);
				}
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);

						script_room_remref(room);
						return ret;
					}


			}
			iterator_stop(&pit);

		}
		script_room_remref(room);

		if( ret_val != PRET_NOSCRIPT ) return true;
//	}

	// Get mobile...
	mob = strcmp(buf,"self") ? get_char_room(ch, NULL, buf) : ch;
	if(mob) {
		script_mobile_addref(mob);

		// Check for tokens FIRST
		iterator_start(&tit, mob->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = false;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&tit);
							iterator_stop(&pit);

							script_token_remref(token);
							script_mobile_remref(mob);
							return ret;
						}

					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && IS_NPC(mob) && mob->pIndexData->progs) {
			script_destructed = false;
			iterator_start(&pit, mob->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
					ret = execute_script(prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);

						script_mobile_remref(mob);
						return ret;
					}

				}
			}
			iterator_stop(&pit);
		}
		script_mobile_remref(mob);

		if( ret_val != PRET_NOSCRIPT ) return true;
	}

	// Get obj...
	if ((obj = get_obj_here(ch, NULL, buf))) {
		script_object_addref(obj);

		// Check for tokens FIRST
		iterator_start(&tit, obj->ltokens);
		while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
			if( token->pIndexData->progs ) {
				script_token_addref(token);
				script_destructed = false;
				iterator_start(&pit, token->pIndexData->progs[slot]);
				while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
					if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
						ret = execute_script(prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
						if( ret != PRET_NOSCRIPT) {
							iterator_stop(&tit);
							iterator_stop(&pit);

							script_token_remref(token);
							script_object_remref(obj);
							return ret;
						}

					}
				}
				iterator_stop(&pit);
				script_token_remref(token);
			}
		}
		iterator_stop(&tit);

		if(ret_val == PRET_NOSCRIPT && obj->pIndexData->progs) {
			script_destructed = false;
			iterator_start(&pit, obj->pIndexData->progs[slot]);
			while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
				if (is_trigger_type(prg->trig_type,TRIG_VERB) && !str_prefix(command, prg->trig_phrase)) {
					ret = execute_script(prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,p,prg->trig_phrase,TRIG_VERB,0,0,0,0,0);
					if( ret != PRET_NOSCRIPT) {
						iterator_stop(&pit);

						script_object_remref(obj);
						return ret;
					}

				}
			}
			iterator_stop(&pit);
		}

		script_object_remref(obj);

		if( ret_val != PRET_NOSCRIPT ) return true;
	}

	return false;
}

// The main entry point for executing commands.
// Can be recursively called from 'at', 'order', 'force'.
void interpret( CHAR_DATA *ch, char *argument )
{
    char command[MAX_INPUT_LENGTH];
    char logline[MAX_INPUT_LENGTH];
//    int cmd;
    int rank;
    bool found, allowed;
    char cmd_copy[MAX_INPUT_LENGTH] ;
    char buf[MSL];
    //const struct cmd_type* selected_command = NULL;
	CMD_DATA *selected_command = NULL;
	CMD_DATA *cmd = NULL;

    // Strip leading spaces
    while (ISSPACE(*argument))
	argument++;

    if ( argument[0] == '\0' )
	return;

    // Frozen people can't do anything
    if (!IS_NPC(ch) && IS_SET(ch->act[0], PLR_FREEZE))
    {
	send_to_char( "You're totally frozen!\n\r", ch );
	return;
    }

    // Neither can paralyzed people
    if (ch->paralyzed > 0 && !is_allowed( argument ))
    {
	send_to_char("You are paralyzed and can't move a muscle!\n\r", ch );
	return;
    }

	// Deal with scripted input
	if(ch->desc && ch->desc->input && ch->desc->input_script.pArea && ch->desc->input_script.vnum > 0 && ch->desc->inputString == NULL) {

		int ret;
		SCRIPT_DATA *script = NULL;
		VARIABLE **var = NULL;
		CHAR_DATA *mob = ch->desc->input_mob;
		OBJ_DATA *obj = ch->desc->input_obj;
		ROOM_INDEX_DATA *room = ch->desc->input_room;
		TOKEN_DATA *tok = ch->desc->input_tok;
		char *v = ch->desc->input_var;

		if(ch->desc->input_mob) {
			script = get_script_index_wnum(ch->desc->input_script,PRG_MPROG);
			var = &ch->desc->input_mob->progs->vars;
		} else if(ch->desc->input_obj) {
			script = get_script_index_wnum(ch->desc->input_script,PRG_OPROG);
			var = &ch->desc->input_obj->progs->vars;
		} else if(ch->desc->input_room) {
			script = get_script_index_wnum(ch->desc->input_script,PRG_RPROG);
			var = &ch->desc->input_room->progs->vars;
		} else if(ch->desc->input_tok) {
			script = get_script_index_wnum(ch->desc->input_script,PRG_TPROG);
			var = &ch->desc->input_tok->progs->vars;
		}

		// Clear this incase other scripts chain together
		ch->desc->input = false;
		ch->desc->input_var = NULL;
		ch->desc->input_script.pArea = NULL;
		ch->desc->input_script.vnum = 0;
		ch->desc->input_mob = NULL;
		ch->desc->input_obj = NULL;
		ch->desc->input_room = NULL;
		ch->desc->input_tok = NULL;
		if(ch->desc->input_prompt) free_string(ch->desc->input_prompt);
		ch->desc->input_prompt = NULL;

		if(script) {
//			send_to_char("Executing script...\n\r",ch);
			if(v) {
//				send_to_char("Var:",ch);
//				send_to_char(v,ch);
//				send_to_char("...\n\r",ch);
				variables_set_string(var,v,argument,false);
			}

			ret = execute_script(script, mob, obj, room, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL,NULL,TRIG_NONE,0,0,0,0,0);
			if(ret > 0 && !IS_NPC(ch) && ch->pcdata->quit_on_input)
				do_function(ch, &do_quit, NULL);
		}

		if(v) free_string(v);

		if(script) return;
	}




    strcpy(cmd_copy, argument);

   /*
    * Grab the command word.
    * Special parsing so ' can be a command,
    *   also no spaces needed after punctuation.
    */
    strcpy( logline, argument );
    if ( !isalpha(argument[0]) && !ISDIGIT(argument[0]) )
    {
	command[0] = argument[0];
	command[1] = '\0';
	argument++;
	while ( ISSPACE(*argument) )
	    argument++;
    }
    else
	argument = one_argument( argument, command );

    // Questions which people must answer before they can go on with life!

	// Sealing a book
	if (IS_VALID(ch->seal_book))
	{
		if (IS_BOOK(ch->seal_book) && IS_SET(BOOK(ch->seal_book)->flags, BOOK_WRITABLE))
		{
			if (!str_prefix(command, "yes"))
			{
				REMOVE_BIT(BOOK(ch->seal_book)->flags, BOOK_WRITABLE);
				act("{Y$p shimmers briefly.{x", ch, NULL, NULL, ch->seal_book, NULL, NULL, NULL, TO_ALL);
				act("$p can no longer be modified.", ch, NULL, NULL, ch->seal_book, NULL, NULL, NULL, TO_CHAR);

				p_percent_trigger(NULL, ch->seal_book, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_SEAL, NULL, 0,0,0,0,0);
			}
		}

		ch->seal_book = NULL;
		return;
	}

    // Remove yourself from a church?
    // Disabled pneuma/dp loss for now - Tieryo
    if (ch->remove_question)
    {
	if (!str_prefix(command, "yes"))
	{
	    if (!str_cmp(ch->name, ch->remove_question->name))
	    {
		if (!str_cmp(ch->name, ch->remove_question->church->founder))
		{
		    act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    sprintf(buf, "{Y[%s has quit %s]{x\n\r", ch->remove_question->name, ch->church->name);
		    gecho( buf );
//		    ch->pneuma = 0;
//		    ch->deitypoints = 0;
		    extract_church( ch->church );
		    ch->remove_question = NULL;
		    sprintf( buf, "%s has quit.", ch->name );
		    append_church_log( ch->church, ch->name );
		}
		else
		{
		    act("{Y[You have removed yourself.]{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    sprintf(buf, "{Y[%s has quit %s]{x\n\r", ch->remove_question->name, ch->church->name);
		    gecho( buf );
		    sprintf( buf, "%s has quit.", ch->name );
		    append_church_log( ch->church, ch->name );
//		    ch->pneuma = 0;
//		    ch->deitypoints = 0;
		    remove_member(ch->remove_question);
		    ch->remove_question = NULL;
		}
	    }
	    return;
	}
	else
        if (!str_prefix(command, "no"))
        {
	    ch->remove_question = NULL;
	    return;
	}
	else
	{
	    send_to_char("Please answer yes or no.\n\r", ch);
	    return;
	}
    }

    if (!IS_NPC(ch) && ch->pcdata->inquiry_subject != NULL) {
	if (command[0] != '\0') {
	    sprintf(buf, "%s %s", command, argument);
	    buf[0] = UPPER(buf[0]);
	    ch->pcdata->inquiry_subject->subject = str_dup(buf);
	    send_to_char("Inquiry added. Starting editor...\n\r", ch);
	    string_append(ch, &ch->pcdata->inquiry_subject->text);
	    ch->pcdata->inquiry_subject = NULL;
	    projects_changed = true;
	}
	else
	    send_to_char("{YEnter inquiry subject:{x ", ch);

	return;
    }

    // Toggle church PK?
    if (ch->pk_question)
    {
	char buf[MAX_STRING_LENGTH];

	if (!str_prefix(command, "yes"))
	{
	    sprintf(buf, "{Y[%s is now a PLAYER KILLING church!]{x\n\r",
		    ch->church->name );
	    gecho( buf );
	    ch->pk_question = false;
	    ch->church->pk = true;
	    return;
	}
	else
        if (!str_prefix(command, "no"))
        {
	    ch->pk_question = false;
	    return;
	}
	else
	{
	    send_to_char("Please answer yes or no.\n\r", ch);
	    return;
	}
    }

    // Toggle personal PK?
    if (ch->personal_pk_question)
    {
	char buf[MAX_STRING_LENGTH];

	if (!str_prefix(command, "yes"))
	{
	    if (!IS_SET( ch->act[0], PLR_PK ) )
	    {
		SET_BIT( ch->act[0], PLR_PK );
		send_to_char("You have toggled PK. Good luck!\n\r", ch );
		sprintf( buf, "%s has toggled PK on!", ch->name );
		crier_announce( buf );
		ch->pneuma -= 5000;
	    }
	    else
	    {
		REMOVE_BIT( ch->act[0], PLR_PK );
		send_to_char("You have toggled PK off.\n\r", ch );
		sprintf( buf, "%s is no longer PK.", ch->name );
		crier_announce( buf );
		ch->pneuma -= 5000;
	    }

	    ch->personal_pk_question = false;
	    return;
	}
	else
        if (!str_prefix(command, "no"))
        {
	    ch->personal_pk_question = false;
	    return;
	}
	else
	{
	    if ( IS_SET( ch->act[0], PLR_PK ) )
	    {
		send_to_char("Toggle PK off? (y/n)\n\r", ch );
	    }
	    else
	    {
		send_to_char("Toggle PK on? (y/n)\n\r", ch );
	    }
	    return;
	}
    }

    // Cross-zone gohall?
    if (ch->cross_zone_question)
    {
	char buf[MAX_STRING_LENGTH];

	if (!str_prefix(command, "yes"))
	{
	    long pneuma_cost;
	    long dp_cost;

	    pneuma_cost = 500;
	    dp_cost = 50000;

	    if ( ch->church == NULL )
		return;

	    ch->church->pneuma -= pneuma_cost;
	    ch->church->dp -= dp_cost;
	    sprintf( buf, "{Y[%s has recalled cross-zone, draining %ld pneuma and %ld karma!]{x\n\r", ch->name, pneuma_cost, dp_cost );
	    msg_church_members( ch->church, buf );
	    ch->cross_zone_question = false;

	    act("{R$n disappears, leaving a resounding echo of discord.{X", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    char_from_room(ch);
	    char_to_room(ch, location_to_room(&ch->church->recall_point));
	    act("$n appears in the room.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    do_function(ch, &do_look, "auto");
	    return;
	}
	else
        if (!str_prefix(command, "no"))
        {
	    send_to_char("Cross-zone recall cancelled.\n\r", ch );
	    ch->cross_zone_question = false;
	    return;
	}
	else
	{
	    send_to_char("Recall cross-zone? (yes/no)\n\r", ch );
	    return;
	}
    }

#if 0
	// TODO: Fix remorting.  This might need to go entirely
    // Remorting!
    if (ch->remort_question) {
		int iClass;
		if(!str_cmp(command, "help")) {
			for (iClass = CLASS_WARRIOR_WARLORD; iClass < MAX_SUB_CLASS; iClass++) {
				if (!str_cmp(argument, sub_class_table[iClass].name[ch->sex]))
					break;
			}

			if (iClass == MAX_SUB_CLASS || !can_choose_subclass(ch, iClass)) {
				send_to_char("Not a valid subclass.\n\r", ch);
				show_multiclass_choices(ch, ch);
				return;
			}

			do_function(ch, do_help, sub_class_table[iClass].name[0]);

		} else {
			for (iClass = CLASS_WARRIOR_WARLORD; iClass < MAX_SUB_CLASS; iClass++) {
				if (!str_cmp(command, sub_class_table[iClass].name[ch->sex]))
					break;
			}

			if (iClass == MAX_SUB_CLASS || !can_choose_subclass(ch, iClass)) {
				send_to_char("Not a valid subclass.\n\r", ch);
				show_multiclass_choices(ch, ch);
				return;
			}

			remort_player(ch, iClass);	// MWUHAHAHA
		}
		return;
	}
#endif

    // Convert church to a different alignment?
    if (!IS_NPC(ch) && ch->pcdata->convert_church != -1)
    {
		char buf[MAX_STRING_LENGTH];

		if (!str_prefix(command, "yes"))
		{
			long pneuma_cost;
			long dp_cost;

			pneuma_cost = 10000;
			dp_cost = 2500000;

			if ( ch->church == NULL )
			return;

			ch->church->pneuma -= pneuma_cost;
			ch->church->dp -= dp_cost;
			ch->church->alignment = ch->pcdata->convert_church;

			sprintf( buf, "{Y[%s has converted to the faith of %s!]{x\n\r",
				ch->church->name,
				ch->church->alignment == CHURCH_GOOD ? "the Pious" :
				ch->church->alignment == CHURCH_NEUTRAL ? "Neutrality" : "Malice" );
			gecho( buf );
			ch->pcdata->convert_church = -1;
			return;
		}
		else if (!str_prefix(command, "no"))
        {
		    send_to_char("Church faith conversion cancelled.\n\r", ch );
		    ch->pcdata->convert_church = -1;
		    return;
		}
		else
		{
		    sprintf( buf, "Are you SURE you want to convert to the faith of %s? (y/n)\n\r"
					"{R***WARNING***:{x all members who cannot follow that faith will be removed on their next login!!!\n\r",
					ch->pcdata->convert_church == CHURCH_GOOD ? "the Pious" :
					ch->pcdata->convert_church == CHURCH_NEUTRAL ? "Neutrality" : "Malice" );
		    send_to_char( buf, ch );
		    return;
		}
    }

    // Answer a challenge?
    // Display character names, no more "a Slayer/a Werewolf" displayed to everyone -- Areo
    if (ch->challenged != NULL)
    {
		CHAR_DATA *victim = ch->challenged;
		char buf[MAX_STRING_LENGTH];

		if (!str_prefix(command, "yes"))
		{
			sprintf(buf, "%s has accepted %s's challenge! May the battle begin!",
				ch->name, victim->name);
			crier_announce( buf );

			sprintf(buf, "{M%s has accepted your challenge!{x\n\r{RYou are transported to the arena!\n\r{x", pers( ch, victim ) );

			send_to_char(buf, victim);

			sprintf(buf, "{MYou have accepted %s's challenge!{x\n\r{RYou are transported to the arena!\n\r{x", pers( victim, ch ) );

			send_to_char(buf, ch);

			if (ch->fighting != NULL)
				stop_fighting(ch, true);

			if (ch->cast > 0)
				stop_casting(ch, true);

			if (ch->script_wait > 0)
				script_end_failure(ch, true);

			interrupt_script(ch,false);


			if (victim->fighting != NULL)
				stop_fighting(victim, true);

			if (victim->cast > 0)
				stop_casting(victim, true);

			if(victim->script_wait > 0)
				script_end_failure(victim, true);

			interrupt_script(victim,false);

			location_from_room(&ch->pcdata->room_before_arena,ch->in_room);
			location_from_room(&victim->pcdata->room_before_arena,victim->in_room);

			char_from_room(ch);
			char_from_room(victim);
			char_to_room(ch, room_index_arena);
			char_to_room(victim, room_index_arena);

			ch->challenged = NULL;
			return;
		}

		if (!str_prefix(command, "no"))
		{
			sprintf( buf, "%s has declined %s's challenge!",
				ch->name, victim->name);
			crier_announce( buf );

			sprintf(buf, "{M%s has declined your challenge!{x\n\r", ch->name);
			send_to_char(buf, victim);

			sprintf(buf, "{MYou have declined %s's challenge!{x\n\r", victim->name);
			send_to_char(buf, ch);
			ch->challenged = NULL;
			return;
		}

		sprintf(buf, "{M%s has challenged you to a fight to the death in the arena!\n\rDo you accept? (Yes/No)\n\r{x", victim->name);
		send_to_char(buf, ch);
		return;
    }

    // Find command in table.
    found = false;
	if (!IS_SWITCHED(ch))
	{
		rank = get_staff_rank(ch);
	}
	else
	{
		rank = get_staff_rank(ch->desc->original);
	}

    ITERATOR it;
    iterator_start(&it, commands_list);
	while(( cmd = (CMD_DATA *)iterator_nextdata(&it)))
	{	
		if ( command[0] == cmd->name[0] && 
		!str_prefix(command, cmd->name) && 
		(!forced_command || cmd->rank == STAFF_PLAYER) && 
		(cmd->rank <= rank || is_granted_command(ch, cmd->name)))
		{
			selected_command = cmd;
			found = true;
			break;
		}
	}
	iterator_stop(&it);

//    for ( cmd = 0; cmd_table[cmd].name[0] != '\0'; cmd++ )
//    {
//		if ( command[0] == cmd_table[cmd].name[0] &&
//			!str_prefix( command, cmd_table[cmd].name ) &&
//			(!forced_command || (cmd_table[cmd].rank == STAFF_PLAYER)) &&  // 20070511NIB - used to prevent script forces from doing imm commands
//			(cmd_table[cmd].rank <= rank || is_granted_command(ch, cmd_table[cmd].name)))
//		{
//			selected_command = &cmd_table[cmd];
//			found = true;
//			break;
//		}
//    }

    allowed = is_allowed(command);

	if (!selected_command->enabled && found)
	{
		sprintf(buf,"%s is currently disabled.\n\r",selected_command->name);
		send_to_char(buf,ch);
		if (!IS_NULLSTR(selected_command->reason))
		{
			sprintf(buf,"Reason: %s\n\r",selected_command->reason);
			send_to_char(buf,ch);
		}
	}

    // Check stuff relevant to interpretation.
    if (IS_AFFECTED(ch, AFF_HIDE) && !(allowed || (found && IS_SET(selected_command->command_flags,CMD_IS_OOC))))
    {
        affect_strip(ch, gsk_hide);
		REMOVE_BIT(ch->affected_by[0], AFF_HIDE);
		act("You step out of the shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		act("$n steps out of the shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
    }

	// TODO: Missing a skill for paralysis
    if (is_affected(ch, get_skill_data("paralysis")) && !allowed)
    {
        send_to_char("You can't move a muscle!\n\r", ch );
        return;
    }

	if (ch->paroxysm > 0 && !allowed)
	{
		if (number_percent() < 20)
		{
			send_to_char("{YYou flail your arms about wildly.{x\n\r", ch);
			act("$n flails $s arms about wildly, unable to control $mself.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else if (number_percent() < 20)
		{
			send_to_char("{YYou cartwheel across the floor.{x\n\r", ch);
			act("{Y$n cartwheels across the floor.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else if (number_percent() < 20)
		{
			send_to_char("{YYou babble nonsensically and foam at the mouth.{x\n\r", ch );
			act("{Y$n babbles nonsensically and foams at the mouth.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else if (number_percent() < 20)
		{
			send_to_char("{YYour fall to the floor and begin to convulse.{x\n\r", ch );
			act("{Y$n collapses to the floor and begins to have seizures.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
		}
		else if (number_percent() < 20)
		{
			send_to_char("{YYou begin to spin around in circles.{x\n\r", ch);
			act("$n spins around dizzifyingly.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			send_to_char("{YYou stare blankly at your feet.{x\n\r", ch);
			act("$n stares blankly, unable to do anything.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}

		return;
	}

    if (ch->cast > 0 && !allowed)
		stop_casting(ch, true);

	if (ch->script_wait > 0 && !allowed)
		script_end_failure(ch, true);

	if(!allowed) interrupt_script(ch,false);

    if (ch->music > 0 && !allowed)
		stop_music(ch, true);

    if (ch->brew > 0 && !allowed)
		return;

	if (ch->imbuing > 0 && !allowed)
		return;

    if (ch->repair > 0 && !allowed)
    {
        act("You stop repairing $p.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
	act("$n stops repairing $p.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_ROOM);
	ch->repair_obj = NULL;
	ch->repair_amt = 0;
	ch->repair = 0;
    }

    if (ch->hide > 0 && !allowed)
    {
        ch->hide = 0;
        send_to_char("You stop looking for a place to hide.\n\r", ch );
    }

    if (ch->bind > 0 && !allowed)
	return;

    if (ch->bomb > 0 && !allowed)
	return;

    if (ch->recite > 0 && !allowed)
    {
	ch->recite = 0;
	free_string( ch->cast_target_name );
	ch->cast_target_name = NULL;
	send_to_char("{WYou stop reciting.{x\n\r", ch );
    }

    if ((ch->reverie > 0 || ch->trance > 0) && !allowed)
    {
    	send_to_char("You can't break your meditation.\n\r",ch );
	return;
    }

    if (ch->scribe > 0 && !allowed)
	return;

    // You can move while shooting, but not much else
    if (ch->ranged > 0 && !allowed)
    {
	if ( str_cmp( command, "north" )
	&&   str_cmp( command, "east" )
	&&   str_cmp( command, "south" )
	&&   str_cmp( command, "west" )
	&&   str_cmp( command, "northwest" )
	&&   str_cmp( command, "northeast" )
	&&   str_cmp( command, "southwest" )
	&&   str_cmp( command, "southeast" )
	&&   str_cmp( command, "up" )
	&&   str_cmp( command, "down" ))
	    stop_ranged( ch, true );
    }

    if (ch->resurrect > 0 && !allowed)
    {
        send_to_char("You stop resurrecting.\n\r", ch );
        ch->resurrect = 0;
    }

    if (ch->fade > 0 && !allowed)
    {
        send_to_char("You fade back into the real world.\n\r", ch );
        ch->fade = 0;
        ch->fade_dir = -1;		//@@@NIB : 20071020
        ch->force_fading = 0;
    }

    // Stop abuse.
	if (selected_command != NULL && found )
	{
    	if (IS_NPC(ch) && selected_command->rank > STAFF_PLAYER)
    	{
			sprintf(buf, "interpret: mob %s (%s) tried immortal command %s",
	    	ch->short_descr, widevnum_string_mobile(ch->pIndexData, NULL), selected_command->name);
			log_string(buf);
			return;
    	}

    	// Log.
    	if ( selected_command->log == LOG_NEVER )
		strcpy( logline, "" );

    	if (((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_LOG)) || logAll || selected_command->log == LOG_ALWAYS))
    	{
			char s[2 * MAX_INPUT_LENGTH];
			char *ps;
			int i;

			ps = s;
			sprintf( log_buf, "Log %s: %s",
	    	IS_NPC(ch) ? ch->short_descr : ch->name, logline );

			// Make sure that was is displayed is what is typed
			for ( i = 0; log_buf[i]; i++ )
			{
	    		*ps++ = log_buf[i];
	    		if ( log_buf[i] == '$' )
					*ps++ = '$';
	    		if ( log_buf[i] == '{' )
				*ps++ = '{';
			}

			*ps = 0;
			wiznet( s, ch, NULL, WIZ_SECURE, 0, get_staff_rank(ch));
			if ( logline[0] != '\0' )
	    		log_string( log_buf );
    	}
	}

    // Command not found... try other places.
    // Modified 2010-08-16 - Changed order. Command -> Custom verbs -> IMC -> Socials -- Tieryo
    if (!found)
    {
    	if (check_verbs(ch,command,argument))
		return;

	#if 0
		if (!IS_NPC(ch) && imc_command_hook(ch, command, argument))
		return;
	#endif

	if (check_social(ch, command, argument))
		return;

	send_to_char( "Huh?\n\r", ch);
	if (IS_NPC(ch))
		sprintf(buf, "NPC \t<send href=\"stat mob %ld %ld|mshow %ld|medit %ld\">%s\t</send> (%ld) tried to use the command '%s' but it didn't exist.", ch->id[0], ch->id[1], ch->pIndexData->vnum, ch->pIndexData->vnum, ch->short_descr, ch->pIndexData->vnum, command);
	else
		sprintf(buf, "%s tried to use the command '%s' but it didn't exist.", ch->name, command);
	log_string(buf);
	wiznet(buf, ch, NULL, WIZ_VERBS, 0, 0);
	return;
    }

    // Command found, let's execute it
    if (ch->position == POS_FEIGN)
    {
	do_function( ch, &do_feign, "");
	if ( !str_cmp( selected_command->name, "feign") )
	    return;
    }

    if (ch->position == POS_HELDUP)
    {
        send_to_char( "{YYou are too fearful to move a muscle!{x\n\r", ch );
        return;
    }

    if (ch->heldup != NULL)
    {
	act( "You lose your concentration and $N escapes!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act( "$n loses $s concentration, freeing you from the holdup!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act( "$n loses $s concentration and $N frees $Mself!", ch, ch->heldup, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
 	stop_holdup(ch);
    }

    // Character not in position for command?
    if ( ch->position < selected_command->position )
    {
	switch( ch->position )
	{
	case POS_DEAD:
	    send_to_char( "Lie still; you are DEAD.\n\r", ch );
	    break;

	case POS_MORTAL:
	case POS_INCAP:
	    send_to_char( "You are far too hurt for that.\n\r", ch );
	    break;

	case POS_STUNNED:
	    send_to_char( "You are too stunned to do that.\n\r", ch );
	    break;

	case POS_SLEEPING:
	    send_to_char( "In your dreams, or what?\n\r", ch );
	    break;

	case POS_RESTING:
	    send_to_char( "You are resting at the moment.\n\r", ch);
	    break;

	case POS_SITTING:
	    send_to_char( "Better stand up first.\n\r",ch);
	    break;

	case POS_FIGHTING:
	    send_to_char( "No way!  You are still fighting!\n\r", ch);
	    break;
	}

	return;
    }

    // Dispatch the command
    (*selected_command->function) ( ch, argument );

    tail_chain();
}


// function to keep argument safe in all commands -- no static strings
void do_function( CHAR_DATA *ch, DO_FUN *do_fun, char *argument )
{
    char *command_string;

    // copy the string
    command_string = argument ? str_dup(argument) : NULL;

    // dispatch the command
    (*do_fun) (ch, command_string);

    // free the string
    if(command_string) free_string(command_string);
}


// Check if a command is a social and execute it if it is.
bool check_social( CHAR_DATA *ch, char *command, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int cmd;
    bool found;

    found  = false;
    for ( cmd = 0; social_table[cmd].name[0] != '\0'; cmd++ )
    {
	if ( command[0] == social_table[cmd].name[0]
	&&   !str_prefix( command, social_table[cmd].name ) )
	{
	    found = true;
	    break;
	}
    }

    if ( !found )
	return false;

    switch ( ch->position )
    {
	case POS_DEAD:
	    send_to_char( "Lie still; you are DEAD.\n\r", ch );
	    return true;

	case POS_INCAP:
	case POS_MORTAL:
	    send_to_char( "You are hurt far too bad for that.\n\r", ch );
	    return true;

	case POS_STUNNED:
	    send_to_char( "You are too stunned to do that.\n\r", ch );
	    return true;

	case POS_SLEEPING:
	    /*
	     * I just know this is the path to a 12" 'if' statement.  :(
	     * But two players asked for it already!  -- Furey
	     */
	    if ( !str_cmp( social_table[cmd].name, "snore" ) )
		break;
	    send_to_char( "In your dreams, or what?\n\r", ch );
	    return true;
    }

    one_argument( argument, arg );
    victim = NULL;
    if ( arg[0] == '\0' ) {
		act( social_table[cmd].others_no_arg, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM    );
		act( social_table[cmd].char_no_arg,   ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR    );
    }
    else if ( ( victim = get_char_room( ch, NULL, arg ) ) == NULL )
    {
		send_to_char( "They aren't here.\n\r", ch );
    }
    else if ( victim == ch )
    {
		act( social_table[cmd].others_auto,   ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM    );
		act( social_table[cmd].char_auto,     ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR    );
    }
    else
    {
		act( social_table[cmd].others_found,  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT );
		act( social_table[cmd].char_found,    ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR    );
		act( social_table[cmd].vict_found,    ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT    );
    }

    // 20140508NIB - Adding EMOTE triggering

    if( victim != NULL )
		p_emoteat_trigger(victim, ch, social_table[cmd].name,0,0,0,0,0);
	else
		p_emote_trigger(ch, social_table[cmd].name,0,0,0,0,0);

    return true;
}


// Return true if an argument is completely numeric.
bool is_number( char *arg )
{
    if ( *arg == '\0' )
        return false;

    if ( *arg == '+' || *arg == '-' )
        arg++;

    for ( ; *arg != '\0'; arg++ )
    {
        if ( !ISDIGIT( *arg ) )
            return false;
    }

    return true;
}

bool is_percent( char *arg )
{
	if ( *arg == '\0' )
	return false;

	for ( ; *arg != '%' && *arg != '\0'; arg++ )
	{
		if ( !ISDIGIT( *arg ) )
			return false;
	}

	if( *arg != '%' )
		return false;

	// Skip the %
	++arg;

	return !*arg;	// Does the string end a null
}


// Given a string like 14.foo, return 14 and 'foo'
int number_argument( char *argument, char *arg )
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
	if ( *pdot == '.' )
	{
	    *pdot = '\0';
	    number = atoi( argument );
	    *pdot = '.';
	    strcpy( arg, pdot+1 );
	    return number;
	}
    }

    strcpy( arg, argument );
    return 1;
}


// Given a string like 14*foo, return 14 and 'foo'
int mult_argument(char *argument, char *arg)
{
    char *pdot;
    int number;

    for ( pdot = argument; *pdot != '\0'; pdot++ )
    {
        if ( *pdot == '*' )
        {
            *pdot = '\0';
			if (is_number(argument))
	            number = atoi( argument );
			else
				number = 0;		// Indicates the part before the * was *not* a number.
            *pdot = '*';
            strcpy( arg, pdot+1 );
            return number;
        }
    }

    strcpy( arg, argument );
    return 1;
}

// Same as one_argument but doesn't lower case the argument
char *one_argument_norm( char *argument, char *arg_first )
{
    char cEnd;

    while ( ISSPACE(*argument) )
	argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
	cEnd = *argument++;

    while ( *argument != '\0' )
    {
	if ( *argument == cEnd )
	{
	    argument++;
	    break;
	}
	*arg_first = *argument;
	arg_first++;
	argument++;
    }
    *arg_first = '\0';

    while ( ISSPACE(*argument) )
	argument++;

    return argument;
}


// Pick off one argument from a string and return the rest. Understands quotes.
char *one_argument( char *argument, char *arg_first )
{
    char cEnd;

    while ( ISSPACE(*argument) )
	argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
	cEnd = *argument++;

    while ( *argument != '\0' )
    {
	if ( *argument == cEnd )
	{
	    argument++;
	    break;
	}
	*arg_first = LOWER(*argument);
	arg_first++;
	argument++;
    }
    *arg_first = '\0';

    while ( ISSPACE(*argument) )
	argument++;

    return argument;
}


/*
 *  * Pick off one argument from a string and return the rest.
 *   * Understands quotes.
 *    */
char *one_caseful_argument (char *argument, char *arg_first)
{
    char cEnd;

    while (ISSPACE (*argument))
        argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"' || *argument == '\'')
        cEnd = *argument++;

    while (*argument != '\0')
    {
        if (*argument == cEnd)
        {
            argument++;
            break;
        }
        *arg_first = *argument;
        arg_first++;
        argument++;
    }
    *arg_first = '\0';

    while (ISSPACE (*argument))
        argument++;

    return argument;
}

static void delete_extra_commands(void *ptr)
{
	free_string((char *)ptr);
}

static int cmd_cmp(void *a, void *b)
{
	return str_cmp((char*)a, (char*)b);
}

// Output a table of commands.
void do_commands( CHAR_DATA *ch, char *argument )
{
    char buf[MAX_STRING_LENGTH], mxp_str[1024];
    int cmd;
    int col;
	int cmdtype = 0;
	CMD_DATA *command;

	if (IS_NPC(ch)) return;

	//ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);

    col = 0;

	if (argument[0] == '\0')
	{
		for (cmdtype = 0; cmdtype < MAX_COMMAND_TYPES; cmdtype++)
		{
			if (cmdtype == CMDTYPE_ADMIN || cmdtype == CMDTYPE_IMMORTAL || cmdtype == CMDTYPE_OLC || cmdtype == CMDTYPE_NEWBIE)
			continue;

			ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);

			sprintf(buf, "\n\r{X===== {W{+%s Commands{X ====={x\n\r", command_types[cmdtype].name);
			send_to_char(buf, ch);
			ITERATOR cit;
			iterator_start(&cit, commands_list);
			col = 0;
			while((command = (CMD_DATA *)iterator_nextdata(&cit)))
			{
				if (command->type != cmdtype)
					continue;

				if (command->rank <= STAFF_PLAYER && !IS_SET(command->command_flags, CMD_HIDE_LISTS))
				{
					if (!list_contains(ch->pcdata->extra_commands, command->name, cmd_cmp))
					{
						if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
							sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
						else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL ) && IS_NULLSTR(command->summary))
							sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
						else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
							sprintf(mxp_str, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
						else
							sprintf(mxp_str, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

						list_appendlink(ch->pcdata->extra_commands, str_dup(mxp_str));
					}

				}
			}
			iterator_stop(&cit);

		if (list_size(ch->pcdata->extra_commands) > 0)
		{
			ITERATOR it;
			char *cmd_str;
			iterator_start(&it, ch->pcdata->extra_commands);
			while((cmd_str = (char *)iterator_nextdata(&it)))
			{
				sprintf( buf, "%s", cmd_str);
				send_to_char( buf, ch );
				if ( ++col % 6 == 0 )
					send_to_char( "\n\r", ch );
			}
			iterator_stop(&it);
		}

		list_destroy(ch->pcdata->extra_commands);
		ch->pcdata->extra_commands = NULL;
		if ( ++col % 6 != 0 )
			send_to_char( "\n\r", ch );
		}

		// Get our extra commands, repeat the process.
		ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);
		collect_verbs(ch);

		sprintf(buf, "\n\r{X===== {WExtra Commands{X ====={x\n\r");
		if (list_size(ch->pcdata->extra_commands) > 0)
		{
			ITERATOR it;
			char *cmd_str;
			iterator_start(&it, ch->pcdata->extra_commands);
			while((cmd_str = (char *)iterator_nextdata(&it)))
			{
				sprintf( buf, "%s", cmd_str);
				send_to_char( buf, ch );
				if ( ++col % 6 == 0 )
					send_to_char( "\n\r", ch );
			}
			iterator_stop(&it);
		}

		list_destroy(ch->pcdata->extra_commands);
		ch->pcdata->extra_commands = NULL;

    	if ( col % 6 != 0 )
		send_to_char( "\n\r", ch );
	}
	else
	{

		if (!str_cmp(argument, "extra"))
		{
					// Get our extra commands, repeat the process.
			ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);
			collect_verbs(ch);

			sprintf(buf, "\n\r{X===== {WExtra Commands{X ====={x\n\r");
			if (list_size(ch->pcdata->extra_commands) > 0)
			{
				ITERATOR it;
				char *cmd_str;
				iterator_start(&it, ch->pcdata->extra_commands);
				while((cmd_str = (char *)iterator_nextdata(&it)))
				{
					sprintf( buf, "%s", cmd_str);
					send_to_char( buf, ch );
					if ( ++col % 6 == 0 )
						send_to_char( "\n\r", ch );
				}
				iterator_stop(&it);
			}

			list_destroy(ch->pcdata->extra_commands);
			ch->pcdata->extra_commands = NULL;
			return;
		}

		if ((cmdtype = flag_value(command_types, argument)) == NO_FLAG)
		{
			send_to_char("Invalid command type.\n\r", ch);
			return;
		}

		cmdtype = flag_value(command_types, argument);
		if (cmdtype == CMDTYPE_ADMIN || cmdtype == CMDTYPE_IMMORTAL || cmdtype == CMDTYPE_OLC || cmdtype == CMDTYPE_NEWBIE)
		{
			send_to_char("Invalid command type.\n\r", ch);
			return;
		}
			
		ch->pcdata->extra_commands = list_createx(false, NULL, delete_extra_commands);

		sprintf(buf, "\n\r{X===== {W{+%s Commands{X ====={x\n\r", command_types[cmdtype].name);
		send_to_char(buf, ch);

		ITERATOR cit;
		iterator_start(&cit, commands_list);
		col = 0;
		while((command = (CMD_DATA *)iterator_nextdata(&cit)))
		{
			if (command->rank == STAFF_PLAYER && !IS_SET(command->command_flags, CMD_HIDE_LISTS) && IS_SET(command->addl_types, flag_value(command_addl_types, argument)))
			{
				if (!list_contains(ch->pcdata->extra_commands, command->name, cmd_cmp))
				{
					if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
						sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
					else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL ) && IS_NULLSTR(command->summary))
						sprintf(mxp_str, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
					else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
						sprintf(mxp_str, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
					else
						sprintf(mxp_str, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

					list_appendlink(ch->pcdata->extra_commands, str_dup(mxp_str));
				}
			}
		}
		if (list_size(ch->pcdata->extra_commands) > 0)
		{
			ITERATOR it;
			char *cmd_str;
			iterator_start(&it, ch->pcdata->extra_commands);
			while((cmd_str = (char *)iterator_nextdata(&it)))
			{
				sprintf( buf, "%s", cmd_str);
				send_to_char( buf, ch );
				if ( ++col % 6 == 0 )
					send_to_char( "\n\r", ch );
			}
			iterator_stop(&it);
		}

		list_destroy(ch->pcdata->extra_commands);
		ch->pcdata->extra_commands = NULL;

    	if ( col % 6 != 0 )
		send_to_char( "\n\r", ch );
	}
}


// Output a table of imm-only commands.
void do_wizhelp( CHAR_DATA *ch, char *argument )
{
    char buf[MAX_STRING_LENGTH];
//    int cmd;
    int col = 0;
	long cmdtype = -1;
	CMD_DATA *command;
	int rank = 0;

	if (argument[0] != '\0')
	{
		cmdtype = flag_value(command_types, argument);
		sprintf(buf, "{B*{G*{B* {XFiltering for {W{+%s{X commands {B*{G*{B*{X\n\r", command_types[cmdtype].name);
		send_to_char(buf,ch);
	}

		if (cmdtype == NO_FLAG)
		{
			send_to_char("Invalid command type.\n\r", ch);
			return;
		}

	
	for ( rank = STAFF_IMMORTAL; rank <= get_staff_rank(ch); rank++ )
	{
		col = 0;
		sprintf(buf, "\n\r{B*{G*{B* {XCommands available to {W{+%s{X {B*{G*{B*{X\n\r", staff_ranks[rank].name);
		send_to_char(buf,ch);

		ITERATOR it;
		iterator_start(&it, commands_list);
		while ((command = (CMD_DATA *)iterator_nextdata(&it)))
		{
			if (cmdtype != -1 && !IS_SET(command->addl_types, flag_value(command_addl_types, argument)))
				continue;

			if (command->rank == rank && command->rank <= get_staff_rank(ch) && !IS_SET(command->command_flags, CMD_HIDE_LISTS) && IS_SET(command->addl_types, flag_value(command_addl_types, argument)))
			{
				if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && !IS_NULLSTR(command->summary))
					sprintf(buf, "\t<send href=\"%s|help #%d\" hint=\"%s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->summary, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
				else if ((command->help_keywords != NULL && str_cmp(command->help_keywords->string, "(null)") && lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) != NULL) && IS_NULLSTR(command->summary))
					sprintf(buf, "\t<send href=\"%s|help #%d\" hint=\"Execute %s|View '%s' helpfile\">{X%s\t</send>%s", command->name, lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat)->index, command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));
				else if ((command->help_keywords == NULL || !str_cmp(command->help_keywords->string, "(null)") || lookup_help_exact(command->help_keywords->string,get_staff_rank(ch),topHelpCat) == NULL) && !IS_NULLSTR(command->summary))
					sprintf(buf, "\t<send href=\"%s\" hint=\"%s\">{X%s\t</send>%s", command->name, command->summary, command->name, pad_string(command->name, 13, NULL, NULL));
				else
					sprintf(buf, "\t<send href=\"%s\" hint=\"Execute %s\">{X%s\t</send>%s", command->name, command->name, command->name, pad_string(command->name, 13, NULL, NULL));

				send_to_char( buf, ch );
				if ( ++col % 6 == 0 )
					send_to_char( "\n\r", ch );
				
			
			}
		}
		iterator_stop(&it);
		send_to_char( "\n\r", ch );
	}

    if ( col % 6 != 0 )
	send_to_char( "\n\r", ch );
}


// Certain informational commands are allowed during paralysis, etc. These are listed here.
bool is_allowed( char *command )
{
    if ( !str_cmp( command, "look")
    || !str_cmp( command, "affects")
    || !str_cmp( command, "whois")
    || !str_cmp( command, "equipment")
    || !str_cmp( command, "inventory")
    || !str_cmp( command, "score") )
        return true;

    return false;
}

// interrupt a chars spell. safe version (no memory leaks)
void stop_music( CHAR_DATA *ch, bool messages )
{

	// Allow for custom messages as well as handling interrupted songs
	if(ch->song_token) {
		if(p_percent_trigger(NULL,NULL,NULL,ch->song_token,ch, NULL, NULL,NULL,NULL,TRIG_TOKEN_INTERRUPT, NULL,(messages?1:0),0,0,0,0))
			messages = false;
	}
    free_string( ch->music_target_name );
    ch->music_target_name = NULL;
    ch->music = 0;
    ch->song = NULL;
    ch->song_token = NULL;
    ch->song_script = NULL;
    ch->song_instrument = NULL;


    if ( messages )
		send_to_char("{YYou stop playing your song.{x\n\r", ch );
}


// interrupt a chars spell. safe version (no memory leaks)
void stop_casting( CHAR_DATA *ch, bool messages )
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	int target;

	if (ch->cast_skill && validate_spell_target(ch, ch->cast_skill->target, ch->cast_target_name, &target, &victim, &obj))
	{
		void *vo = NULL;
		if (target == TARGET_CHAR)
			vo = victim;
		else if (target == TARGET_OBJ)
			vo = obj;

		// Allow for custom messages as well as handling interrupted spells
		if(ch->cast_token) {
			// Return positive to suppress standard messages
			if(p_percent_trigger(NULL,NULL,NULL,ch->cast_token,ch, victim, NULL, obj, NULL, TRIG_TOKEN_INTERRUPT, NULL,0,0,0,0,0) > 0)
				messages = false;
		}
		else if (ch->cast_skill->interrupt_fun && ch->cast_skill->interrupt_fun != spell_null)
		{
			// Return true to supposed standard messages
			if ((*(ch->cast_skill->interrupt_fun))(ch->cast_skill, ch->tot_level, ch, vo, target, WEAR_NONE))
				messages = false;
		}
	}
	free_string(ch->casting_failure_message);
	ch->casting_failure_message = NULL;
    free_string( ch->cast_target_name );
    ch->cast_target_name = NULL;
    ch->cast = 0;
    ch->cast_skill = NULL;
    ch->cast_token = NULL;
    ch->cast_script = NULL;


    if ( messages )
    {
	send_to_char("{WYou stop your casting.{x\n\r", ch);
	if (number_percent() < 10)
	{
	    send_to_char("{YSmall yellow sparks spiral around you then fade away.{x\n\r", ch);
	    act("$n's magic fizzles and dies.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	if (number_percent() < 20)
	{
	    send_to_char("{YYou hear a loud bang as your magic dissipates.{x\n\r", ch);
	    act("{YYou hear a loud bang as $n stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	if (number_percent() < 30)
	{
	    send_to_char("{YA puff of smoke billows out of your ears.{x\n\r", ch);
	    act("{YA puff of smoke billows out of $n's ears as $e stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	if (number_percent() < 40)
	{
	    send_to_char("{YYour skin turns multicoloured then turns back to normal.{x\n\r", ch);
	    act("{Y$n's skin turns multicoloured momentarily as $e stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	if (number_percent() < 50)
	{
	    send_to_char("{YYour magic fizzles and dies.\n\r{x", ch );
	    act("{Y$n's magic fizzles and dies.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	if ( number_percent() < 60 )
	{
	    send_to_char("{YEnergy sizzles as you stop your casting.\n\r{x",
		    ch );
	    act("{YEnergy sizzles as $n stops $s casting.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	}
	else
	if (number_percent() < 70 )
	{
	    send_to_char("{YSparks fly from your fingers as your magic dissipates.{x\n\r", ch );
	    act("{YSparks fly from $n's fingers as $s magic dissipates.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	}
	else
	if (number_percent() < 80 )
	{
	    send_to_char("{YYou eyes flash with white light as you interrupt your spell.{x\n\r", ch );
	    act("{Y$n's eyes flash with white light as $e interrupts $s spell.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	}
	else
	if ( number_percent() < 90 )
	{
	    send_to_char("{YYour hair stands on end for a moment as you stop your spell.{x\n\r", ch );
	    act("{Y$n's hair stands on end for a moment as $e finishes $s spell.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	}
	else
	{
	    send_to_char("{YYour magic dissipates into the air.{x\n\r", ch );
	    act("{Y$n's magic dissipates into the air.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	}
    }
}


// Stop ranged attacks safely.
void stop_ranged( CHAR_DATA *ch, bool messages )
{
    if ( ch == NULL )
    {
		bug( "stop_ranged: null ch", 0 );
		return;
    }

    if ( ch->projectile_weapon != NULL )
    {
		if ( messages )
		{
			act("You put down $p.", ch, NULL, NULL, ch->projectile_weapon, NULL, NULL, NULL, TO_CHAR );
			act("$n puts down $p.", ch, NULL, NULL, ch->projectile_weapon, NULL, NULL, NULL, TO_ROOM );
		}

		ch->ranged = 0;
		ch->projectile_weapon = NULL;
		free_string( ch->projectile_victim );
		ch->projectile_victim = NULL;
		ch->projectile_dir    = -1;
		ch->projectile_range  = 0;
		ch->projectile_mana = 0;

		// Need to destroy created objects
		if (IS_VALID(ch->projectile) && IS_SET(ch->projectile->extra[1], ITEM_CREATED))
		{
			extract_obj(ch->projectile);
		}

		ch->projectile	      = NULL;
    }
}


bool is_granted_command(CHAR_DATA *ch, char *name)
{
    COMMAND_DATA *cmd;

    if (IS_NPC(ch)) {
	bug("is_granted_command: checking an NPC", 0);
	return false;
    }

    for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next)
    {
	if (!str_cmp(cmd->name, name))
	    return true;
    }

    return false;
}

void command_under_construction(CHAR_DATA *ch)
{
	send_to_char("{D*{Y*{D*{Y*{D*{Y[{R UNDER CONSTRUCTION {Y]{D*{Y*{D*{Y*{D*{x\n\r\n\r", ch);
	send_to_char("Command is under construction.  Please be patient until it is ready.\n\r\n\r", ch);
	send_to_char("{D*{Y*{D*{Y*{D*{Y[{R UNDER CONSTRUCTION {Y]{D*{Y*{D*{Y*{D*{x\n\r", ch);
}
