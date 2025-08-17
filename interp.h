/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,	   *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *									   *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael	   *
 *  Chastain, Michael Quan, and Mitchell Tse.				   *
 *									   *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc	   *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.						   *
 *									   *
 *  Much time and thought has gone into this software and you are	   *
 *  benefitting.  We hope that you share your changes too.  What goes	   *
 *  around, comes around.						   *
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

/* this is a listing of all the commands and command related data */

/* wrapper function for safe command execution */
void do_function args((CHAR_DATA *ch, DO_FUN *do_fun, char *argument));
void cmd_function args((CHAR_DATA *ch, CMD_FUN *cmd_fun, char *argument, const char *context));

void command_under_construction(CHAR_DATA *ch);

#if 0
/* for command types */
#define ML 	MAX_LEVEL	/* implementor */
#define L1	MAX_LEVEL - 1  	/* creator */
#define L2	MAX_LEVEL - 2	/* supremacy */
#define L3	MAX_LEVEL - 3	/* ascendant */
#define L4 	MAX_LEVEL - 4	/* god */
#define L5	MAX_LEVEL - 5	/* immortal */
#define L6	MAX_LEVEL - 6	/* gimp */
#define IM	LEVEL_IMMORTAL 	/* immortal */
#define HE	LEVEL_HERO	/* hero */
#endif

#define COM_INGORE	1

#define CMDTYPE_NONE            0       // Treated as the catchall / general / miscellaneous group
#define CMDTYPE_MOVE            1
#define CMDTYPE_COMBAT          2
#define CMDTYPE_OBJECT          3
#define CMDTYPE_INFO            4
#define CMDTYPE_COMM            5
#define CMDTYPE_RACIAL          6
#define CMDTYPE_OOC             7
#define CMDTYPE_IMMORTAL        8
#define CMDTYPE_OLC             9
#define CMDTYPE_ADMIN           10
#define CMDTYPE_NEWBIE          11
#define MAX_COMMAND_TYPES       12

/*
 * Structure for a command in the command lookup table.
 */
struct	cmd_type
{
    char * const	name;	   /* command name */
    int16_t     cmd_type;
    CMD_FUN *        cmd_fun;	   /* cmd_function */
    int16_t     position;  /* minimum position required */
    int16_t     rank;     /* min. rank required */
    int16_t     log;       /* log when? */
    bool        show;      /* show? */
    bool        is_ooc;		// Command is purely OOC - certain things won't break when doing these commands
};

/* the command table itself */
extern	const	struct	cmd_type	cmd_table	[];

/*
 * Command functions.
 * Defined in act_*.c (mostly).
 */
DECLARE_CMD_FUN( do_addcommand	);
DECLARE_CMD_FUN(	do_advance	);
DECLARE_CMD_FUN(	do_alevel	);
DECLARE_CMD_FUN( do_aload	);
DECLARE_CMD_FUN( do_aedit	);
DECLARE_CMD_FUN( do_affects	);
DECLARE_CMD_FUN( do_affstat	);
DECLARE_CMD_FUN( do_afk		);
DECLARE_CMD_FUN( do_aim		);
DECLARE_CMD_FUN( do_alias	);
DECLARE_CMD_FUN( do_alist	);
DECLARE_CMD_FUN(	do_allow	);
DECLARE_CMD_FUN( do_ambush	);
DECLARE_CMD_FUN( do_announcements);
DECLARE_CMD_FUN( do_arealinks    );
DECLARE_CMD_FUN(	do_areas	);
DECLARE_CMD_FUN( do_areset 	);
DECLARE_CMD_FUN( do_astat	);
DECLARE_CMD_FUN( do_asave	);
DECLARE_CMD_FUN( do_asave_new	);
DECLARE_CMD_FUN( do_asearch	);
DECLARE_CMD_FUN( do_assignhelper	);
DECLARE_CMD_FUN(	do_at		);
DECLARE_CMD_FUN(	do_attach	);
DECLARE_CMD_FUN(	do_auction	);
DECLARE_CMD_FUN( do_autoassist	);
DECLARE_CMD_FUN( do_autoexit	);
DECLARE_CMD_FUN( do_autoeq       );
DECLARE_CMD_FUN( do_autogold	);
DECLARE_CMD_FUN( do_autolist	);
DECLARE_CMD_FUN( do_autoloot	);
DECLARE_CMD_FUN( do_autosac	);
DECLARE_CMD_FUN( do_autosetname  );
DECLARE_CMD_FUN( do_autosplit	);
DECLARE_CMD_FUN( do_autosurvey   );
DECLARE_CMD_FUN( do_autowar	);
DECLARE_CMD_FUN(	do_backstab	);
DECLARE_CMD_FUN(	do_bamfin	);
DECLARE_CMD_FUN(	do_bamfout	);
DECLARE_CMD_FUN(	do_ban		);
DECLARE_CMD_FUN( do_bank		);
DECLARE_CMD_FUN( do_bar		);
DECLARE_CMD_FUN( do_bash		);
DECLARE_CMD_FUN( do_battlespam 	);
DECLARE_CMD_FUN( do_berserk	);
DECLARE_CMD_FUN( do_bind		);
DECLARE_CMD_FUN( do_bite 	);
DECLARE_CMD_FUN( do_blackjack	);
DECLARE_CMD_FUN( do_blow		);
DECLARE_CMD_FUN( do_board	);
DECLARE_CMD_FUN(	do_boat_chase	);
DECLARE_CMD_FUN( do_bomb		);
DECLARE_CMD_FUN( do_boost	);
DECLARE_CMD_FUN( do_botter	);
DECLARE_CMD_FUN(	do_bounty	);
DECLARE_CMD_FUN(	do_brandish	);
DECLARE_CMD_FUN(	do_breathe	);
DECLARE_CMD_FUN( do_brew		);
DECLARE_CMD_FUN( do_brief	);
DECLARE_CMD_FUN( do_build	);
DECLARE_CMD_FUN( do_burgle	);
DECLARE_CMD_FUN(	do_buy		);
DECLARE_CMD_FUN( do_cargo	);
DECLARE_CMD_FUN(	do_cast		);
DECLARE_CMD_FUN( do_catchup	);
DECLARE_CMD_FUN( do_chadd   	);
DECLARE_CMD_FUN(	do_challenge	);
DECLARE_CMD_FUN( do_changes	);
DECLARE_CMD_FUN( do_channels	);
DECLARE_CMD_FUN( do_charge	);
DECLARE_CMD_FUN( do_chat		);
DECLARE_CMD_FUN( do_chbalance	);
DECLARE_CMD_FUN( do_chcreate     );
DECLARE_CMD_FUN( do_chdem	);
DECLARE_CMD_FUN( do_chgohall	);
DECLARE_CMD_FUN( do_chlist	);
DECLARE_CMD_FUN( do_chprom	);
DECLARE_CMD_FUN( do_chrem    	);
DECLARE_CMD_FUN(	do_chtalk	);
DECLARE_CMD_FUN(	do_church	);
DECLARE_CMD_FUN(	do_circle	);
DECLARE_CMD_FUN( do_classset    );
DECLARE_CMD_FUN(	do_clear	);
DECLARE_CMD_FUN( do_clone	);
DECLARE_CMD_FUN(	do_close	);
DECLARE_CMD_FUN( do_colour	);
DECLARE_CMD_FUN( do_colour       );
DECLARE_CMD_FUN( do_combine	);
DECLARE_CMD_FUN(	do_commands	);
DECLARE_CMD_FUN( do_compact	);
DECLARE_CMD_FUN(	do_compare	);
DECLARE_CMD_FUN(	do_compress	);
DECLARE_CMD_FUN( do_concentrate	);
DECLARE_CMD_FUN(	do_consider	);
DECLARE_CMD_FUN( do_consume	);
DECLARE_CMD_FUN(	do_convert	);
DECLARE_CMD_FUN( do_count	);
DECLARE_CMD_FUN(	do_credits	);
DECLARE_CMD_FUN( do_crew		);
DECLARE_CMD_FUN(	do_damage	);
DECLARE_CMD_FUN(	do_dig	);
DECLARE_CMD_FUN( do_danger	);
DECLARE_CMD_FUN( do_delet	);
DECLARE_CMD_FUN( do_delete	);
DECLARE_CMD_FUN(	do_deny		);
DECLARE_CMD_FUN( do_deposit	);
DECLARE_CMD_FUN(	do_description	);
DECLARE_CMD_FUN( do_dice		);
DECLARE_CMD_FUN( do_dirt		);
DECLARE_CMD_FUN(	do_disarm	);
DECLARE_CMD_FUN(	do_disconnect	);
DECLARE_CMD_FUN( do_disembark	);
DECLARE_CMD_FUN( do_dislink	);
DECLARE_CMD_FUN( do_dismount	);
DECLARE_CMD_FUN(	do_donate	);
DECLARE_CMD_FUN(	do_down		);
DECLARE_CMD_FUN(	do_drink	);
DECLARE_CMD_FUN(	do_drop		);
DECLARE_CMD_FUN( do_dump		);
DECLARE_CMD_FUN( do_dungeon  );
DECLARE_CMD_FUN(	do_east		);
DECLARE_CMD_FUN(	do_eat		);
DECLARE_CMD_FUN(	do_echo		);
DECLARE_CMD_FUN( do_email	);
DECLARE_CMD_FUN(	do_emote	);
DECLARE_CMD_FUN( do_enter	);
DECLARE_CMD_FUN( do_envenom	);
DECLARE_CMD_FUN(	do_equipment	);
DECLARE_CMD_FUN( do_evasion	);
DECLARE_CMD_FUN(	do_evict	);
DECLARE_CMD_FUN(	do_examine	);
DECLARE_CMD_FUN(	do_exits	);
DECLARE_CMD_FUN(	do_fade		);
DECLARE_CMD_FUN(	do_feign	);
DECLARE_CMD_FUN( do_fight	);
DECLARE_CMD_FUN(	do_fill		);
DECLARE_CMD_FUN( do_flag		);
DECLARE_CMD_FUN( do_flame	);
DECLARE_CMD_FUN(	do_flee		);
DECLARE_CMD_FUN(	do_follow	);
DECLARE_CMD_FUN(	do_force	);
DECLARE_CMD_FUN( do_formstate    );
DECLARE_CMD_FUN(	do_freeze	);
DECLARE_CMD_FUN(	do_get		);
DECLARE_CMD_FUN(	do_get2	);
DECLARE_CMD_FUN(	do_give		);
DECLARE_CMD_FUN(	do_gohome	);
DECLARE_CMD_FUN(	do_goship	);
DECLARE_CMD_FUN( do_gossip	);
DECLARE_CMD_FUN(	do_goto		);
DECLARE_CMD_FUN(	do_goxy		);
DECLARE_CMD_FUN( do_gq		);
DECLARE_CMD_FUN(	do_group	);
DECLARE_CMD_FUN( do_groups	);
DECLARE_CMD_FUN(	do_gtell	);
DECLARE_CMD_FUN( do_hands	);
DECLARE_CMD_FUN( do_hedit	);
DECLARE_CMD_FUN(	do_help		);
DECLARE_CMD_FUN( do_helper	);
DECLARE_CMD_FUN(	do_hide		);
DECLARE_CMD_FUN(	do_hints	);
DECLARE_CMD_FUN( do_hitch    );
DECLARE_CMD_FUN(	do_holdup	);
DECLARE_CMD_FUN(	do_holylight	);
DECLARE_CMD_FUN(	do_holyaura	);
DECLARE_CMD_FUN(	do_holypersona	);
DECLARE_CMD_FUN(	do_holywarp	);
DECLARE_CMD_FUN(	do_house	);
DECLARE_CMD_FUN(	do_housemove	);
DECLARE_CMD_FUN(	do_hunt		);
DECLARE_CMD_FUN(	do_idea		);
DECLARE_CMD_FUN( do_ifchecks	);
DECLARE_CMD_FUN( do_ignore	);
DECLARE_CMD_FUN( do_imbue	);
DECLARE_CMD_FUN(	do_immortalise	);
DECLARE_CMD_FUN( do_immflag 	);
DECLARE_CMD_FUN(	do_immtalk	);
DECLARE_CMD_FUN( do_imotd	);
DECLARE_CMD_FUN( do_incognito	);
DECLARE_CMD_FUN(	do_infuse	);
DECLARE_CMD_FUN( do_inspect	);
DECLARE_CMD_FUN( do_instance );
DECLARE_CMD_FUN(	do_intimidate	);
DECLARE_CMD_FUN(	do_intone	);
DECLARE_CMD_FUN(	do_inventory	);
DECLARE_CMD_FUN(	do_invis	);
DECLARE_CMD_FUN( do_jawbone	);
DECLARE_CMD_FUN( do_judge	);
DECLARE_CMD_FUN( do_junk		);
DECLARE_CMD_FUN( do_keep		);
DECLARE_CMD_FUN(	do_kick		);
DECLARE_CMD_FUN(	do_kill		);
DECLARE_CMD_FUN( do_knock	);
DECLARE_CMD_FUN(	do_list		);
DECLARE_CMD_FUN( do_load		);
DECLARE_CMD_FUN(	do_lock		);
DECLARE_CMD_FUN(	do_locker	);
DECLARE_CMD_FUN(	do_log		);
DECLARE_CMD_FUN(	do_look		);
DECLARE_CMD_FUN( do_lore		);
DECLARE_CMD_FUN( do_lyc		);
DECLARE_CMD_FUN( do_mail		);
DECLARE_CMD_FUN( do_mapgoto	);
DECLARE_CMD_FUN( do_memory	);
DECLARE_CMD_FUN( do_map		);
DECLARE_CMD_FUN( do_mcopy	);
DECLARE_CMD_FUN( do_medit	);
DECLARE_CMD_FUN(	do_mfind	);
DECLARE_CMD_FUN(	do_mlevel	);
DECLARE_CMD_FUN( do_mlist	);
DECLARE_CMD_FUN(	do_mload	);
DECLARE_CMD_FUN( do_mob		);
DECLARE_CMD_FUN( do_moron	);
DECLARE_CMD_FUN( do_motd		);
DECLARE_CMD_FUN( do_mount	);
DECLARE_CMD_FUN( do_mpdelete	);
DECLARE_CMD_FUN( do_mpcopy	);
DECLARE_CMD_FUN( do_mpdump	);
DECLARE_CMD_FUN( do_mpedit	);
DECLARE_CMD_FUN( do_mpstat	);
DECLARE_CMD_FUN(	do_mset		);
DECLARE_CMD_FUN( do_mshow	);
DECLARE_CMD_FUN(	do_mstat	);
DECLARE_CMD_FUN(	do_multi	);
DECLARE_CMD_FUN( do_music	);
DECLARE_CMD_FUN(	do_mwhere	);
DECLARE_CMD_FUN( do_newlock	);
DECLARE_CMD_FUN( do_news		);
DECLARE_CMD_FUN( do_nochannels	);
DECLARE_CMD_FUN( do_nofollow	);
DECLARE_CMD_FUN( do_noloot	);
DECLARE_CMD_FUN( do_noresurrect  );
DECLARE_CMD_FUN(	do_north	);
DECLARE_CMD_FUN(	do_northeast	);
DECLARE_CMD_FUN(	do_northwest	);
DECLARE_CMD_FUN(	do_noshout	);
DECLARE_CMD_FUN( do_nosummon	);
DECLARE_CMD_FUN(	do_note		);
DECLARE_CMD_FUN(	do_notell	);
DECLARE_CMD_FUN( do_notify	);
DECLARE_CMD_FUN( do_npcstatus	);
DECLARE_CMD_FUN( do_ocopy	);
DECLARE_CMD_FUN( do_oedit	);
DECLARE_CMD_FUN(	do_ofind	);
DECLARE_CMD_FUN( do_olc		);
DECLARE_CMD_FUN(	do_olevel	);
DECLARE_CMD_FUN( do_olist	);
DECLARE_CMD_FUN(	do_oload	);
DECLARE_CMD_FUN( do_ooc   	);
DECLARE_CMD_FUN( do_opcopy	);
DECLARE_CMD_FUN( do_opdelete	);
DECLARE_CMD_FUN( do_opdump 	);
DECLARE_CMD_FUN( do_opedit 	);
DECLARE_CMD_FUN(	do_open		);
DECLARE_CMD_FUN( do_opstat 	);
DECLARE_CMD_FUN(	do_order	);
DECLARE_CMD_FUN(	do_oset		);
DECLARE_CMD_FUN( do_oshow	);
DECLARE_CMD_FUN(	do_ostat	);
DECLARE_CMD_FUN( do_otransfer	);
DECLARE_CMD_FUN( do_owhere	);
DECLARE_CMD_FUN( do_page     );
DECLARE_CMD_FUN(	do_pardon	);
DECLARE_CMD_FUN(	do_password	);
DECLARE_CMD_FUN(	do_peace	);
DECLARE_CMD_FUN( do_pecho	);
DECLARE_CMD_FUN( do_pdelete 	);
DECLARE_CMD_FUN( do_pedit	);
DECLARE_CMD_FUN( do_penalty	);
DECLARE_CMD_FUN( do_plist	);
DECLARE_CMD_FUN( do_permban	);
DECLARE_CMD_FUN( do_pshow	);
DECLARE_CMD_FUN( do_pursuit	);
DECLARE_CMD_FUN(	do_pick		);
DECLARE_CMD_FUN( do_pinquiry     );
DECLARE_CMD_FUN( do_pk		);
DECLARE_CMD_FUN( do_plant	);
DECLARE_CMD_FUN( do_play		);
DECLARE_CMD_FUN( do_pour		);
DECLARE_CMD_FUN(	do_practice	);
DECLARE_CMD_FUN( do_project	);
DECLARE_CMD_FUN( do_prompt	);
DECLARE_CMD_FUN( do_protect	);
DECLARE_CMD_FUN(	do_pull		);
DECLARE_CMD_FUN(	do_purge	);
DECLARE_CMD_FUN(	do_push		);
DECLARE_CMD_FUN(	do_put		);
DECLARE_CMD_FUN( do_qedit	);
DECLARE_CMD_FUN( do_qlist 	);
DECLARE_CMD_FUN(	do_quaff	);
DECLARE_CMD_FUN( do_mission  );
DECLARE_CMD_FUN( do_quiet	);
DECLARE_CMD_FUN(	do_quit		);
DECLARE_CMD_FUN( do_quote	);
DECLARE_CMD_FUN( do_rack         );
DECLARE_CMD_FUN( do_rcopy	);
DECLARE_CMD_FUN( do_read		);
DECLARE_CMD_FUN(	do_reboo	);
DECLARE_CMD_FUN(	do_reboot	);
DECLARE_CMD_FUN(	do_recall	);
DECLARE_CMD_FUN(	do_recho	);
DECLARE_CMD_FUN(	do_recite	);
DECLARE_CMD_FUN( do_reckonin 	);
DECLARE_CMD_FUN( do_reckoning	);
DECLARE_CMD_FUN( do_redit	);
DECLARE_CMD_FUN( do_remcommand	);
DECLARE_CMD_FUN(	do_remove	);
DECLARE_CMD_FUN(	do_renew	);
DECLARE_CMD_FUN(	do_repair	);
DECLARE_CMD_FUN( do_replay	);
DECLARE_CMD_FUN(	do_reply	);
DECLARE_CMD_FUN(	do_report	);
DECLARE_CMD_FUN(	do_rescue	);
DECLARE_CMD_FUN( do_resets	);
DECLARE_CMD_FUN(	do_rest		);
DECLARE_CMD_FUN(	do_restore	);
DECLARE_CMD_FUN( do_restring	);
DECLARE_CMD_FUN( do_resurrect	);
DECLARE_CMD_FUN(	do_return	);
DECLARE_CMD_FUN( do_reverie 	);
DECLARE_CMD_FUN( do_rip      );
DECLARE_CMD_FUN( do_rjunk	);
DECLARE_CMD_FUN( do_rlist	);
DECLARE_CMD_FUN( do_rpcopy	);
DECLARE_CMD_FUN( do_rpdelete	);
DECLARE_CMD_FUN( do_rpdump 	);
DECLARE_CMD_FUN( do_rpedit 	);
DECLARE_CMD_FUN( do_rpstat 	);
DECLARE_CMD_FUN(	do_rset		);
DECLARE_CMD_FUN( do_rshow	);
DECLARE_CMD_FUN(	do_rstat	);
DECLARE_CMD_FUN( do_rules	);
DECLARE_CMD_FUN( do_rwhere	);
DECLARE_CMD_FUN(	do_sacrifice	);
DECLARE_CMD_FUN( do_sadd		);
DECLARE_CMD_FUN(	do_save		);
DECLARE_CMD_FUN(	do_say		);
DECLARE_CMD_FUN(	do_sayto	);
DECLARE_CMD_FUN(	do_scan		);
DECLARE_CMD_FUN(	do_score	);
DECLARE_CMD_FUN( do_scribe	);
DECLARE_CMD_FUN( do_scroll	);
DECLARE_CMD_FUN( do_scry         );
//DECLARE_CMD_FUN( do_scuttle	);
DECLARE_CMD_FUN( do_sdelete	);
DECLARE_CMD_FUN( do_sdemote );
DECLARE_CMD_FUN( do_sduty	);
DECLARE_CMD_FUN( do_seal     );
DECLARE_CMD_FUN(	do_search	);
DECLARE_CMD_FUN(	do_secondary	);
DECLARE_CMD_FUN(	do_sell		);
DECLARE_CMD_FUN( do_set		);
DECLARE_CMD_FUN( do_sflag	);
DECLARE_CMD_FUN( do_shape	);
DECLARE_CMD_FUN( do_shift	);
DECLARE_CMD_FUN( do_ship		);
DECLARE_CMD_FUN( do_ship_list	);
DECLARE_CMD_FUN(	do_shoot	);
DECLARE_CMD_FUN( do_showdamage	);
DECLARE_CMD_FUN(	do_shutdow	);
DECLARE_CMD_FUN(	do_shutdown	);
DECLARE_CMD_FUN( do_sip      );
DECLARE_CMD_FUN( do_sit		);
DECLARE_CMD_FUN( do_skills	);
DECLARE_CMD_FUN( do_skull	);
DECLARE_CMD_FUN(	do_slay		);
DECLARE_CMD_FUN(	do_sleep	);
DECLARE_CMD_FUN(	do_slevel	);
DECLARE_CMD_FUN( do_slist	);
DECLARE_CMD_FUN(	do_slit		);
DECLARE_CMD_FUN(	do_sload	);
DECLARE_CMD_FUN(	do_slookup	);
DECLARE_CMD_FUN( do_smite	);
DECLARE_CMD_FUN( do_smoke	);
DECLARE_CMD_FUN(	do_sneak	);
DECLARE_CMD_FUN( do_socials	);
DECLARE_CMD_FUN( do_sockets	);
DECLARE_CMD_FUN(	do_south	);
DECLARE_CMD_FUN(	do_southeast	);
DECLARE_CMD_FUN(	do_southwest	);
DECLARE_CMD_FUN( do_spell	);
DECLARE_CMD_FUN( do_spells	);
DECLARE_CMD_FUN(	do_split	);
DECLARE_CMD_FUN(	do_sset		);
DECLARE_CMD_FUN( do_songset  );
DECLARE_CMD_FUN( do_spromote );
DECLARE_CMD_FUN( do_ssupervisor  );
DECLARE_CMD_FUN( do_staff	);
DECLARE_CMD_FUN( do_stake	);
DECLARE_CMD_FUN(	do_stand	);
DECLARE_CMD_FUN( do_stat		);
DECLARE_CMD_FUN( do_stats	);
DECLARE_CMD_FUN(	do_startinvasion	);
DECLARE_CMD_FUN(	do_steal	);
DECLARE_CMD_FUN( do_stock	);
DECLARE_CMD_FUN( do_strike	);
DECLARE_CMD_FUN( do_string	);
DECLARE_CMD_FUN( do_survey	);
DECLARE_CMD_FUN(	do_switch	);
DECLARE_CMD_FUN(	do_tedit	);
DECLARE_CMD_FUN(	do_tail_kick	);
DECLARE_CMD_FUN(	do_tell		);
DECLARE_CMD_FUN( do_tells	);
DECLARE_CMD_FUN( do_test		);
DECLARE_CMD_FUN( do_testport	);
DECLARE_CMD_FUN(	do_tfind	);
DECLARE_CMD_FUN(	do_throw	);
DECLARE_CMD_FUN(	do_time		);
DECLARE_CMD_FUN(	do_title	);
DECLARE_CMD_FUN( do_token	);
DECLARE_CMD_FUN( do_toggle	);
DECLARE_CMD_FUN( do_toxins	);
DECLARE_CMD_FUN(	do_tpedit       );
DECLARE_CMD_FUN( do_tpstat 	);
DECLARE_CMD_FUN( do_tpdump 	);
DECLARE_CMD_FUN(	do_tail_kick	);
DECLARE_CMD_FUN(	do_train	);
DECLARE_CMD_FUN( do_trample	);
DECLARE_CMD_FUN( do_trance 	);
DECLARE_CMD_FUN(	do_transfer	);
DECLARE_CMD_FUN( do_tset		);
DECLARE_CMD_FUN( do_tkset	);
DECLARE_CMD_FUN( do_tlist	);
DECLARE_CMD_FUN( do_tshow	);
DECLARE_CMD_FUN( do_tstat	);
DECLARE_CMD_FUN(	do_turn		);
DECLARE_CMD_FUN( do_unalias	);
DECLARE_CMD_FUN( do_unhitch  );
DECLARE_CMD_FUN( do_uninvis	);
DECLARE_CMD_FUN(	do_unlock	);
DECLARE_CMD_FUN( do_ungroup	);
DECLARE_CMD_FUN( do_unread	);
DECLARE_CMD_FUN( do_unrestring   );
DECLARE_CMD_FUN( do_unyoke   );
DECLARE_CMD_FUN(	do_up		);
DECLARE_CMD_FUN(	do_use		);
DECLARE_CMD_FUN(	do_value	);
DECLARE_CMD_FUN( do_showversion	);
DECLARE_CMD_FUN(	do_visible	);
DECLARE_CMD_FUN( do_vislist	);
/* VIZZWILDS */
DECLARE_CMD_FUN( do_vledit	);
DECLARE_CMD_FUN( do_vlinks	);
DECLARE_CMD_FUN( do_regions  );

DECLARE_CMD_FUN( do_vnum		);
DECLARE_CMD_FUN(	do_wake		);
DECLARE_CMD_FUN( do_war          );
DECLARE_CMD_FUN( do_warcry	);
DECLARE_CMD_FUN( do_warp		);
DECLARE_CMD_FUN(	do_waypoint	);
DECLARE_CMD_FUN(	do_wear		);
DECLARE_CMD_FUN(	do_weather	);
DECLARE_CMD_FUN(	do_weave	);
/* VIZZWILDS */
DECLARE_CMD_FUN(	do_wedit	);
DECLARE_CMD_FUN(	do_west		);
DECLARE_CMD_FUN(	do_where	);
DECLARE_CMD_FUN( do_whistle	);
DECLARE_CMD_FUN( do_whisper	);
DECLARE_CMD_FUN(	do_who		);
DECLARE_CMD_FUN(	do_who_new	);
DECLARE_CMD_FUN( do_whois	);
DECLARE_CMD_FUN(	do_wimpy	);
DECLARE_CMD_FUN(	do_wizhelp	);
DECLARE_CMD_FUN( do_wizlist	);
DECLARE_CMD_FUN(	do_wizlock	);
DECLARE_CMD_FUN( do_wiznet	);
DECLARE_CMD_FUN( do_wlist	);		// Wilderness List
DECLARE_CMD_FUN( do_worth	);
DECLARE_CMD_FUN( do_write    );
DECLARE_CMD_FUN(	do_wstat	);
DECLARE_CMD_FUN(	do_yell		);
DECLARE_CMD_FUN( do_yoke     );
DECLARE_CMD_FUN(	do_zap		);
DECLARE_CMD_FUN( do_zecho	);
DECLARE_CMD_FUN( do_zot		);

DECLARE_CMD_FUN( do_mplist	);
DECLARE_CMD_FUN( do_oplist	);
DECLARE_CMD_FUN( do_rplist	);
DECLARE_CMD_FUN( do_tplist	);
DECLARE_CMD_FUN( do_aplist	);
DECLARE_CMD_FUN( do_iplist	);
DECLARE_CMD_FUN( do_dplist	);


DECLARE_CMD_FUN( do_touch	);
DECLARE_CMD_FUN( do_ruboff	);
DECLARE_CMD_FUN( do_ink		);
DECLARE_CMD_FUN( do_affix	);

DECLARE_CMD_FUN( do_takeoff	);
DECLARE_CMD_FUN( do_land		);

DECLARE_CMD_FUN( do_behead	);
DECLARE_CMD_FUN( do_conceal	);
DECLARE_CMD_FUN(	do_rehearse	);

DECLARE_CMD_FUN( do_bpedit	);
DECLARE_CMD_FUN( do_bplist	);
DECLARE_CMD_FUN( do_bpshow	);

DECLARE_CMD_FUN( do_bsedit	);
DECLARE_CMD_FUN( do_bslist	);
DECLARE_CMD_FUN( do_bsshow	);

DECLARE_CMD_FUN( do_dngedit	);
DECLARE_CMD_FUN( do_dnglist	);
DECLARE_CMD_FUN( do_dngshow	);

DECLARE_CMD_FUN( do_apdump 	);
DECLARE_CMD_FUN( do_ipdump 	);
DECLARE_CMD_FUN( do_dpdump 	);

DECLARE_CMD_FUN(	do_apedit	);
DECLARE_CMD_FUN(	do_ipedit	);
DECLARE_CMD_FUN(	do_dpedit	);

DECLARE_CMD_FUN(	do_shedit	);
DECLARE_CMD_FUN(	do_shlist	);
DECLARE_CMD_FUN(	do_shshow	);

DECLARE_CMD_FUN( do_ships	);
DECLARE_CMD_FUN( do_expand	);
DECLARE_CMD_FUN( do_collapse	);

DECLARE_CMD_FUN( do_spawntreasuremap );
DECLARE_CMD_FUN( do_reserved );
DECLARE_CMD_FUN( do_triggers );

DECLARE_CMD_FUN(	do_liqedit	);
DECLARE_CMD_FUN( do_liqlist	);

DECLARE_CMD_FUN( do_skedit );
DECLARE_CMD_FUN( do_sklist );
DECLARE_CMD_FUN( do_skshow );

DECLARE_CMD_FUN( do_sgedit );
DECLARE_CMD_FUN( do_sglist );
DECLARE_CMD_FUN( do_sgshow );

DECLARE_CMD_FUN( do_songedit );
DECLARE_CMD_FUN( do_songlist );
DECLARE_CMD_FUN( do_songshow );

DECLARE_CMD_FUN( do_readycheck );
DECLARE_CMD_FUN( do_ignite );
DECLARE_CMD_FUN( do_extinguish );

//DECLARE_CMD_FUN( do_speed	);
//DECLARE_CMD_FUN( do_steer	);
//DECLARE_CMD_FUN( do_navigate );

DECLARE_CMD_FUN( do_immstrike );


DECLARE_CMD_FUN( do_reputations );
DECLARE_CMD_FUN( do_atwar );

DECLARE_CMD_FUN( do_repedit );
DECLARE_CMD_FUN( do_replist );
DECLARE_CMD_FUN( do_repshow );

DECLARE_CMD_FUN( do_repset );


DECLARE_CMD_FUN( do_matedit );
DECLARE_CMD_FUN( do_matlist );
DECLARE_CMD_FUN( do_matshow );


DECLARE_CMD_FUN( do_setclass );
DECLARE_CMD_FUN( do_classes );
DECLARE_CMD_FUN( do_clslist );
DECLARE_CMD_FUN( do_clsedit );
DECLARE_CMD_FUN( do_clsshow );


DECLARE_CMD_FUN( do_racelist );
DECLARE_CMD_FUN( do_raceedit );
DECLARE_CMD_FUN( do_raceshow );

DECLARE_CMD_FUN( do_sectorlist );
DECLARE_CMD_FUN( do_sectoredit );
DECLARE_CMD_FUN( do_sectorshow );

DECLARE_CMD_FUN( do_lead );
DECLARE_CMD_FUN( do_unlead );

DECLARE_CMD_FUN( do_corpsedit );
DECLARE_CMD_FUN( do_corpselist );
DECLARE_CMD_FUN( do_corpseshow );

DECLARE_CMD_FUN( do_cmdlist );
DECLARE_CMD_FUN( do_cmdedit  );
DECLARE_CMD_FUN( do_cmdshow  );

DECLARE_CMD_FUN( do_area     );
DECLARE_CMD_FUN( do_areas    );
DECLARE_CMD_FUN( do_reloadstats );
DECLARE_CMD_FUN( do_testemail );
DECLARE_CMD_FUN( do_pwreset );
DECLARE_CMD_FUN( do_lvlaudit );
DECLARE_CMD_FUN( do_keygen );
DECLARE_CMD_FUN( do_mfareset );
DECLARE_CMD_FUN( do_logout );
DECLARE_CMD_FUN (do_accset);
DECLARE_CMD_FUN (do_accstat);
DECLARE_CMD_FUN( do_gameedit );
DECLARE_CMD_FUN( do_accnote);
DECLARE_CMD_FUN ( do_vault );
DECLARE_CMD_FUN( do_coffer );

// Not really a function that will be usable by other commands
DECLARE_CMD_FUN (_do_speak_on_channel);
