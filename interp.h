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

#define COM_INGORE	1


/*
 * Structure for a command in the command lookup table.
 */
struct	cmd_type
{
    char * const	name;	   /* command name */
    DO_FUN *		do_fun;	   /* do_function */
    sh_int		position;  /* minimum position required */
    sh_int		level;     /* min. level required */
    sh_int		log;       /* log when? */
    sh_int              show;      /* show? */
    bool		is_ooc;		// Command is purely OOC - certain things won't break when doing these commands
};

/* the command table itself */
extern	const	struct	cmd_type	cmd_table	[];

/*
 * Command functions.
 * Defined in act_*.c (mostly).
 */
DECLARE_DO_FUN( do_addcommand	);
DECLARE_DO_FUN(	do_advance	);
DECLARE_DO_FUN(	do_alevel	);
DECLARE_DO_FUN( do_aload	);
DECLARE_DO_FUN( do_aedit	);
DECLARE_DO_FUN( do_affects	);
DECLARE_DO_FUN( do_affstat	);
DECLARE_DO_FUN( do_afk		);
DECLARE_DO_FUN( do_aim		);
DECLARE_DO_FUN( do_alias	);
DECLARE_DO_FUN( do_alist	);
DECLARE_DO_FUN(	do_allow	);
DECLARE_DO_FUN( do_ambush	);
DECLARE_DO_FUN( do_announcements);
DECLARE_DO_FUN( do_arealinks    );
DECLARE_DO_FUN(	do_areas	);
DECLARE_DO_FUN( do_areset 	);
DECLARE_DO_FUN( do_astat	);
DECLARE_DO_FUN( do_asave	);
DECLARE_DO_FUN( do_asave_new	);
DECLARE_DO_FUN( do_asearch	);
DECLARE_DO_FUN( do_assignhelper	);
DECLARE_DO_FUN(	do_at		);
DECLARE_DO_FUN(	do_attach	);
DECLARE_DO_FUN(	do_auction	);
DECLARE_DO_FUN( do_autoassist	);
DECLARE_DO_FUN( do_autoexit	);
DECLARE_DO_FUN( do_autoeq       );
DECLARE_DO_FUN( do_autogold	);
DECLARE_DO_FUN( do_autolist	);
DECLARE_DO_FUN( do_autoloot	);
DECLARE_DO_FUN( do_autosac	);
DECLARE_DO_FUN( do_autosetname  );
DECLARE_DO_FUN( do_autosplit	);
DECLARE_DO_FUN( do_autosurvey   );
DECLARE_DO_FUN( do_autowar	);
DECLARE_DO_FUN(	do_backstab	);
DECLARE_DO_FUN(	do_bamfin	);
DECLARE_DO_FUN(	do_bamfout	);
DECLARE_DO_FUN(	do_ban		);
DECLARE_DO_FUN( do_bank		);
DECLARE_DO_FUN( do_bar		);
DECLARE_DO_FUN( do_bash		);
DECLARE_DO_FUN( do_battlespam 	);
DECLARE_DO_FUN( do_berserk	);
DECLARE_DO_FUN( do_bind		);
DECLARE_DO_FUN( do_bite 	);
DECLARE_DO_FUN( do_blackjack	);
DECLARE_DO_FUN( do_blow		);
DECLARE_DO_FUN( do_board	);
DECLARE_DO_FUN(	do_boat_chase	);
DECLARE_DO_FUN( do_bomb		);
DECLARE_DO_FUN( do_boost	);
DECLARE_DO_FUN( do_botter	);
DECLARE_DO_FUN(	do_bounty	);
DECLARE_DO_FUN(	do_brandish	);
DECLARE_DO_FUN(	do_breathe	);
DECLARE_DO_FUN( do_brew		);
DECLARE_DO_FUN( do_brief	);
DECLARE_DO_FUN( do_build	);
DECLARE_DO_FUN( do_burgle	);
DECLARE_DO_FUN(	do_buy		);
DECLARE_DO_FUN( do_cargo	);
DECLARE_DO_FUN(	do_cast		);
DECLARE_DO_FUN( do_catchup	);
DECLARE_DO_FUN( do_chadd   	);
DECLARE_DO_FUN(	do_challenge	);
DECLARE_DO_FUN( do_changes	);
DECLARE_DO_FUN( do_channels	);
DECLARE_DO_FUN( do_charge	);
DECLARE_DO_FUN( do_chat		);
DECLARE_DO_FUN( do_chbalance	);
DECLARE_DO_FUN( do_chcreate     );
DECLARE_DO_FUN( do_chdem	);
DECLARE_DO_FUN( do_chgohall	);
DECLARE_DO_FUN( do_chlist	);
DECLARE_DO_FUN( do_chprom	);
DECLARE_DO_FUN( do_chrem    	);
DECLARE_DO_FUN(	do_chtalk	);
DECLARE_DO_FUN(	do_church	);
DECLARE_DO_FUN(	do_circle	);
DECLARE_DO_FUN(	do_clear	);
DECLARE_DO_FUN( do_clone	);
DECLARE_DO_FUN(	do_close	);
DECLARE_DO_FUN( do_colour	);
DECLARE_DO_FUN( do_colour       );
DECLARE_DO_FUN( do_combine	);
DECLARE_DO_FUN(	do_commands	);
DECLARE_DO_FUN( do_compact	);
DECLARE_DO_FUN(	do_compare	);
DECLARE_DO_FUN(	do_compress	);
DECLARE_DO_FUN( do_concentrate	);
DECLARE_DO_FUN(	do_consider	);
DECLARE_DO_FUN( do_consume	);
DECLARE_DO_FUN(	do_convert	);
DECLARE_DO_FUN( do_count	);
DECLARE_DO_FUN(	do_credits	);
DECLARE_DO_FUN( do_crew		);
DECLARE_DO_FUN(	do_damage	);
DECLARE_DO_FUN(	do_dig	);
DECLARE_DO_FUN( do_danger	);
DECLARE_DO_FUN( do_delet	);
DECLARE_DO_FUN( do_delete	);
DECLARE_DO_FUN(	do_deny		);
DECLARE_DO_FUN( do_deposit	);
DECLARE_DO_FUN(	do_description	);
DECLARE_DO_FUN( do_dice		);
DECLARE_DO_FUN( do_dirt		);
DECLARE_DO_FUN(	do_disarm	);
DECLARE_DO_FUN(	do_disconnect	);
DECLARE_DO_FUN( do_disembark	);
DECLARE_DO_FUN( do_dislink	);
DECLARE_DO_FUN( do_dismount	);
DECLARE_DO_FUN(	do_donate	);
DECLARE_DO_FUN(	do_down		);
DECLARE_DO_FUN(	do_drink	);
DECLARE_DO_FUN(	do_drop		);
DECLARE_DO_FUN( do_dump		);
DECLARE_DO_FUN( do_dungeon  );
DECLARE_DO_FUN(	do_east		);
DECLARE_DO_FUN(	do_eat		);
DECLARE_DO_FUN(	do_echo		);
DECLARE_DO_FUN( do_email	);
DECLARE_DO_FUN(	do_emote	);
DECLARE_DO_FUN( do_enter	);
DECLARE_DO_FUN( do_envenom	);
DECLARE_DO_FUN(	do_equipment	);
DECLARE_DO_FUN( do_evasion	);
DECLARE_DO_FUN(	do_evict	);
DECLARE_DO_FUN(	do_examine	);
DECLARE_DO_FUN(	do_exits	);
DECLARE_DO_FUN(	do_fade		);
DECLARE_DO_FUN(	do_feign	);
DECLARE_DO_FUN( do_fight	);
DECLARE_DO_FUN(	do_fill		);
DECLARE_DO_FUN( do_flag		);
DECLARE_DO_FUN( do_flame	);
DECLARE_DO_FUN(	do_flee		);
DECLARE_DO_FUN(	do_follow	);
DECLARE_DO_FUN(	do_force	);
DECLARE_DO_FUN( do_formstate    );
DECLARE_DO_FUN(	do_freeze	);
DECLARE_DO_FUN(	do_get		);
DECLARE_DO_FUN(	do_get2	);
DECLARE_DO_FUN(	do_give		);
DECLARE_DO_FUN(	do_gohome	);
DECLARE_DO_FUN(	do_goship	);
DECLARE_DO_FUN( do_gossip	);
DECLARE_DO_FUN(	do_goto		);
DECLARE_DO_FUN(	do_goxy		);
DECLARE_DO_FUN( do_gq		);
DECLARE_DO_FUN(	do_group	);
DECLARE_DO_FUN( do_groups	);
DECLARE_DO_FUN(	do_gtell	);
DECLARE_DO_FUN( do_hands	);
DECLARE_DO_FUN( do_heal		);
DECLARE_DO_FUN( do_hedit	);
DECLARE_DO_FUN(	do_help		);
DECLARE_DO_FUN( do_helper	);
DECLARE_DO_FUN(	do_hide		);
DECLARE_DO_FUN(	do_hints	);
DECLARE_DO_FUN(	do_holdup	);
DECLARE_DO_FUN(	do_holylight	);
DECLARE_DO_FUN(	do_holyaura	);
DECLARE_DO_FUN(	do_holywarp	);
DECLARE_DO_FUN(	do_house	);
DECLARE_DO_FUN(	do_housemove	);
DECLARE_DO_FUN(	do_hunt		);
DECLARE_DO_FUN(	do_idea		);
DECLARE_DO_FUN( do_ifchecks	);
DECLARE_DO_FUN( do_ignore	);
DECLARE_DO_FUN(	do_immortalise	);
DECLARE_DO_FUN( do_immflag 	);
DECLARE_DO_FUN(	do_immtalk	);
DECLARE_DO_FUN( do_imotd	);
DECLARE_DO_FUN( do_incognito	);
DECLARE_DO_FUN(	do_infuse	);
DECLARE_DO_FUN( do_inspect	);
DECLARE_DO_FUN( do_instance );
DECLARE_DO_FUN(	do_intimidate	);
DECLARE_DO_FUN(	do_intone	);
DECLARE_DO_FUN(	do_inventory	);
DECLARE_DO_FUN(	do_invis	);
DECLARE_DO_FUN( do_jawbone	);
DECLARE_DO_FUN( do_judge	);
DECLARE_DO_FUN( do_junk		);
DECLARE_DO_FUN( do_keep		);
DECLARE_DO_FUN(	do_kick		);
DECLARE_DO_FUN(	do_kill		);
DECLARE_DO_FUN( do_knock	);
DECLARE_DO_FUN(	do_list		);
DECLARE_DO_FUN( do_load		);
DECLARE_DO_FUN(	do_lock		);
DECLARE_DO_FUN(	do_locker	);
DECLARE_DO_FUN(	do_log		);
DECLARE_DO_FUN(	do_look		);
DECLARE_DO_FUN( do_lore		);
DECLARE_DO_FUN( do_lyc		);
DECLARE_DO_FUN( do_mail		);
DECLARE_DO_FUN( do_mapgoto	);
DECLARE_DO_FUN( do_memory	);
DECLARE_DO_FUN( do_map		);
DECLARE_DO_FUN( do_mcopy	);
DECLARE_DO_FUN( do_medit	);
DECLARE_DO_FUN(	do_mfind	);
DECLARE_DO_FUN(	do_mlevel	);
DECLARE_DO_FUN( do_mlist	);
DECLARE_DO_FUN(	do_mload	);
DECLARE_DO_FUN( do_mob		);
DECLARE_DO_FUN( do_moron	);
DECLARE_DO_FUN( do_motd		);
DECLARE_DO_FUN( do_mount	);
DECLARE_DO_FUN( do_mpdelete	);
DECLARE_DO_FUN( do_mpcopy	);
DECLARE_DO_FUN( do_mpdump	);
DECLARE_DO_FUN( do_mpedit	);
DECLARE_DO_FUN( do_mpstat	);
DECLARE_DO_FUN(	do_mset		);
DECLARE_DO_FUN( do_mshow	);
DECLARE_DO_FUN(	do_mstat	);
DECLARE_DO_FUN(	do_multi	);
DECLARE_DO_FUN( do_music	);
DECLARE_DO_FUN(	do_mwhere	);
DECLARE_DO_FUN( do_newlock	);
DECLARE_DO_FUN( do_news		);
DECLARE_DO_FUN( do_nochannels	);
DECLARE_DO_FUN( do_nofollow	);
DECLARE_DO_FUN( do_noloot	);
DECLARE_DO_FUN( do_noresurrect  );
DECLARE_DO_FUN(	do_north	);
DECLARE_DO_FUN(	do_northeast	);
DECLARE_DO_FUN(	do_northwest	);
DECLARE_DO_FUN(	do_noshout	);
DECLARE_DO_FUN( do_nosummon	);
DECLARE_DO_FUN(	do_note		);
DECLARE_DO_FUN(	do_notell	);
DECLARE_DO_FUN( do_notify	);
DECLARE_DO_FUN( do_npcstatus	);
DECLARE_DO_FUN( do_ocopy	);
DECLARE_DO_FUN( do_oedit	);
DECLARE_DO_FUN(	do_ofind	);
DECLARE_DO_FUN( do_olc		);
DECLARE_DO_FUN(	do_olevel	);
DECLARE_DO_FUN( do_olist	);
DECLARE_DO_FUN(	do_oload	);
DECLARE_DO_FUN( do_ooc   	);
DECLARE_DO_FUN( do_opcopy	);
DECLARE_DO_FUN( do_opdelete	);
DECLARE_DO_FUN( do_opdump 	);
DECLARE_DO_FUN( do_opedit 	);
DECLARE_DO_FUN(	do_open		);
DECLARE_DO_FUN( do_opstat 	);
DECLARE_DO_FUN(	do_order	);
DECLARE_DO_FUN(	do_oset		);
DECLARE_DO_FUN( do_oshow	);
DECLARE_DO_FUN(	do_ostat	);
DECLARE_DO_FUN( do_otransfer	);
DECLARE_DO_FUN( do_owhere	);
DECLARE_DO_FUN( do_page     );
DECLARE_DO_FUN(	do_pardon	);
DECLARE_DO_FUN(	do_password	);
DECLARE_DO_FUN(	do_peace	);
DECLARE_DO_FUN( do_pecho	);
DECLARE_DO_FUN( do_pdelete 	);
DECLARE_DO_FUN( do_pedit	);
DECLARE_DO_FUN( do_penalty	);
DECLARE_DO_FUN( do_plist	);
DECLARE_DO_FUN( do_permban	);
DECLARE_DO_FUN( do_pshow	);
DECLARE_DO_FUN( do_pursuit	);
DECLARE_DO_FUN(	do_pick		);
DECLARE_DO_FUN( do_pinquiry     );
DECLARE_DO_FUN( do_pk		);
DECLARE_DO_FUN( do_plant	);
DECLARE_DO_FUN( do_play		);
DECLARE_DO_FUN( do_pour		);
DECLARE_DO_FUN(	do_practice	);
DECLARE_DO_FUN( do_project	);
DECLARE_DO_FUN( do_prompt	);
DECLARE_DO_FUN( do_protect	);
DECLARE_DO_FUN(	do_pull		);
DECLARE_DO_FUN(	do_purge	);
DECLARE_DO_FUN(	do_push		);
DECLARE_DO_FUN(	do_put		);
DECLARE_DO_FUN( do_qedit	);
DECLARE_DO_FUN( do_qlist 	);
DECLARE_DO_FUN(	do_quaff	);
DECLARE_DO_FUN( do_quest        );
DECLARE_DO_FUN( do_quiet	);
DECLARE_DO_FUN(	do_quit		);
DECLARE_DO_FUN( do_quote	);
DECLARE_DO_FUN( do_rack         );
DECLARE_DO_FUN( do_rcopy	);
DECLARE_DO_FUN( do_read		);
DECLARE_DO_FUN(	do_reboo	);
DECLARE_DO_FUN(	do_reboot	);
DECLARE_DO_FUN(	do_recall	);
DECLARE_DO_FUN(	do_recho	);
DECLARE_DO_FUN(	do_recite	);
DECLARE_DO_FUN( do_reckonin 	);
DECLARE_DO_FUN( do_reckoning	);
DECLARE_DO_FUN( do_redit	);
DECLARE_DO_FUN( do_remcommand	);
DECLARE_DO_FUN(	do_remove	);
DECLARE_DO_FUN(	do_renew	);
DECLARE_DO_FUN(	do_repair	);
DECLARE_DO_FUN( do_replay	);
DECLARE_DO_FUN(	do_reply	);
DECLARE_DO_FUN(	do_report	);
DECLARE_DO_FUN(	do_rescue	);
DECLARE_DO_FUN( do_resets	);
DECLARE_DO_FUN(	do_rest		);
DECLARE_DO_FUN(	do_restore	);
DECLARE_DO_FUN( do_restring	);
DECLARE_DO_FUN( do_resurrect	);
DECLARE_DO_FUN(	do_return	);
DECLARE_DO_FUN( do_reverie 	);
DECLARE_DO_FUN( do_rip      );
DECLARE_DO_FUN( do_rjunk	);
DECLARE_DO_FUN( do_rlist	);
DECLARE_DO_FUN( do_rpcopy	);
DECLARE_DO_FUN( do_rpdelete	);
DECLARE_DO_FUN( do_rpdump 	);
DECLARE_DO_FUN( do_rpedit 	);
DECLARE_DO_FUN( do_rpstat 	);
DECLARE_DO_FUN(	do_rset		);
DECLARE_DO_FUN( do_rshow	);
DECLARE_DO_FUN(	do_rstat	);
DECLARE_DO_FUN( do_rules	);
DECLARE_DO_FUN( do_rwhere	);
DECLARE_DO_FUN(	do_sacrifice	);
DECLARE_DO_FUN( do_sadd		);
DECLARE_DO_FUN(	do_save		);
DECLARE_DO_FUN(	do_say		);
DECLARE_DO_FUN(	do_sayto	);
DECLARE_DO_FUN(	do_scan		);
DECLARE_DO_FUN(	do_score	);
DECLARE_DO_FUN( do_scribe	);
DECLARE_DO_FUN( do_scroll	);
DECLARE_DO_FUN( do_scry         );
//DECLARE_DO_FUN( do_scuttle	);
DECLARE_DO_FUN( do_sdelete	);
DECLARE_DO_FUN( do_sduty	);
DECLARE_DO_FUN( do_seal     );
DECLARE_DO_FUN(	do_search	);
DECLARE_DO_FUN(	do_secondary	);
DECLARE_DO_FUN(	do_sell		);
DECLARE_DO_FUN( do_set		);
DECLARE_DO_FUN( do_sexual	);	/* What? hahaha*/
DECLARE_DO_FUN( do_sflag	);
DECLARE_DO_FUN( do_shape	);
DECLARE_DO_FUN( do_shift	);
DECLARE_DO_FUN( do_ship		);
DECLARE_DO_FUN( do_ship_list	);
DECLARE_DO_FUN(	do_shoot	);
DECLARE_DO_FUN( do_showdamage	);
DECLARE_DO_FUN(	do_shutdow	);
DECLARE_DO_FUN(	do_shutdown	);
DECLARE_DO_FUN( do_sip      );
DECLARE_DO_FUN( do_sit		);
DECLARE_DO_FUN( do_skills	);
DECLARE_DO_FUN( do_skull	);
DECLARE_DO_FUN(	do_slay		);
DECLARE_DO_FUN(	do_sleep	);
DECLARE_DO_FUN(	do_slevel	);
/*DECLARE_DO_FUN( do_spromote	);
DECLARE_DO_FUN( do_sdemote	);*/
DECLARE_DO_FUN( do_slist	);
DECLARE_DO_FUN(	do_slit		);
DECLARE_DO_FUN(	do_sload	);
DECLARE_DO_FUN(	do_slookup	);
DECLARE_DO_FUN( do_smite	);
DECLARE_DO_FUN( do_smoke	);
DECLARE_DO_FUN(	do_sneak	);
DECLARE_DO_FUN( do_socials	);
DECLARE_DO_FUN( do_sockets	);
DECLARE_DO_FUN(	do_south	);
DECLARE_DO_FUN(	do_southeast	);
DECLARE_DO_FUN(	do_southwest	);
DECLARE_DO_FUN( do_spell	);
DECLARE_DO_FUN( do_spells	);
DECLARE_DO_FUN(	do_split	);
DECLARE_DO_FUN(	do_sset		);
DECLARE_DO_FUN( do_ssupervisor  );
DECLARE_DO_FUN( do_staff	);
DECLARE_DO_FUN( do_stake	);
DECLARE_DO_FUN(	do_stand	);
DECLARE_DO_FUN( do_stat		);
DECLARE_DO_FUN( do_stats	);
DECLARE_DO_FUN(	do_startinvasion	);
DECLARE_DO_FUN(	do_steal	);
DECLARE_DO_FUN( do_stock	);
DECLARE_DO_FUN( do_strike	);
DECLARE_DO_FUN( do_string	);
DECLARE_DO_FUN( do_survey	);
DECLARE_DO_FUN(	do_switch	);
DECLARE_DO_FUN(	do_tedit	);
DECLARE_DO_FUN(	do_tail_kick	);
DECLARE_DO_FUN(	do_tell		);
DECLARE_DO_FUN( do_tells	);
DECLARE_DO_FUN( do_test		);
DECLARE_DO_FUN( do_testport	);
DECLARE_DO_FUN(	do_tfind	);
DECLARE_DO_FUN(	do_throw	);
DECLARE_DO_FUN(	do_time		);
DECLARE_DO_FUN(	do_title	);
DECLARE_DO_FUN( do_token	);
DECLARE_DO_FUN( do_toggle	);
DECLARE_DO_FUN( do_toxins	);
DECLARE_DO_FUN(	do_tpedit       );
DECLARE_DO_FUN( do_tpstat 	);
DECLARE_DO_FUN( do_tpdump 	);
DECLARE_DO_FUN(	do_tail_kick	);
DECLARE_DO_FUN(	do_train	);
DECLARE_DO_FUN( do_trample	);
DECLARE_DO_FUN( do_trance 	);
DECLARE_DO_FUN(	do_transfer	);
DECLARE_DO_FUN( do_tset		);
DECLARE_DO_FUN( do_tkset	);
DECLARE_DO_FUN( do_tlist	);
DECLARE_DO_FUN( do_tshow	);
DECLARE_DO_FUN( do_tstat	);
DECLARE_DO_FUN(	do_turn		);
DECLARE_DO_FUN( do_unalias	);
DECLARE_DO_FUN( do_uninvis	);
DECLARE_DO_FUN(	do_unlock	);
DECLARE_DO_FUN( do_ungroup	);
DECLARE_DO_FUN( do_unread	);
DECLARE_DO_FUN( do_unrestring   );
DECLARE_DO_FUN(	do_up		);
DECLARE_DO_FUN(	do_use		);
DECLARE_DO_FUN(	do_value	);
DECLARE_DO_FUN( do_showversion	);
DECLARE_DO_FUN(	do_visible	);
DECLARE_DO_FUN( do_vislist	);
/* VIZZWILDS */
DECLARE_DO_FUN( do_vledit	);
DECLARE_DO_FUN( do_vlinks	);
DECLARE_DO_FUN( do_regions  );

DECLARE_DO_FUN( do_vnum		);
DECLARE_DO_FUN(	do_wake		);
DECLARE_DO_FUN( do_war          );
DECLARE_DO_FUN( do_warcry	);
DECLARE_DO_FUN( do_warp		);
DECLARE_DO_FUN(	do_waypoint	);
DECLARE_DO_FUN(	do_wear		);
DECLARE_DO_FUN(	do_weather	);
DECLARE_DO_FUN(	do_weave	);
/* VIZZWILDS */
DECLARE_DO_FUN(	do_wedit	);
DECLARE_DO_FUN(	do_west		);
DECLARE_DO_FUN(	do_where	);
DECLARE_DO_FUN( do_whistle	);
DECLARE_DO_FUN( do_whisper	);
DECLARE_DO_FUN(	do_who		);
DECLARE_DO_FUN(	do_who_new	);
DECLARE_DO_FUN( do_whois	);
DECLARE_DO_FUN(	do_wimpy	);
DECLARE_DO_FUN(	do_wizhelp	);
DECLARE_DO_FUN( do_wizlist	);
DECLARE_DO_FUN(	do_wizlock	);
DECLARE_DO_FUN( do_wiznet	);
DECLARE_DO_FUN( do_wlist	);		// Wilderness List
DECLARE_DO_FUN( do_worth	);
DECLARE_DO_FUN( do_write    );
DECLARE_DO_FUN(	do_wstat	);
DECLARE_DO_FUN(	do_yell		);
DECLARE_DO_FUN(	do_zap		);
DECLARE_DO_FUN( do_zecho	);
DECLARE_DO_FUN( do_zot		);

DECLARE_DO_FUN( do_mplist	);
DECLARE_DO_FUN( do_oplist	);
DECLARE_DO_FUN( do_rplist	);
DECLARE_DO_FUN( do_tplist	);
DECLARE_DO_FUN( do_aplist	);
DECLARE_DO_FUN( do_iplist	);
DECLARE_DO_FUN( do_dplist	);


DECLARE_DO_FUN( do_touch	);
DECLARE_DO_FUN( do_ruboff	);
DECLARE_DO_FUN( do_ink		);
DECLARE_DO_FUN( do_affix	);

DECLARE_DO_FUN( do_takeoff	);
DECLARE_DO_FUN( do_land		);

DECLARE_DO_FUN( do_behead	);
DECLARE_DO_FUN( do_conceal	);
DECLARE_DO_FUN(	do_rehearse	);

DECLARE_DO_FUN( do_bpedit	);
DECLARE_DO_FUN( do_bplist	);
DECLARE_DO_FUN( do_bpshow	);

DECLARE_DO_FUN( do_bsedit	);
DECLARE_DO_FUN( do_bslist	);
DECLARE_DO_FUN( do_bsshow	);

DECLARE_DO_FUN( do_dngedit	);
DECLARE_DO_FUN( do_dnglist	);
DECLARE_DO_FUN( do_dngshow	);

DECLARE_DO_FUN( do_apdump 	);
DECLARE_DO_FUN( do_ipdump 	);
DECLARE_DO_FUN( do_dpdump 	);

DECLARE_DO_FUN(	do_apedit	);
DECLARE_DO_FUN(	do_ipedit	);
DECLARE_DO_FUN(	do_dpedit	);

DECLARE_DO_FUN(	do_shedit	);
DECLARE_DO_FUN(	do_shlist	);
DECLARE_DO_FUN(	do_shshow	);

DECLARE_DO_FUN( do_ships	);
DECLARE_DO_FUN( do_expand	);
DECLARE_DO_FUN( do_collapse	);

DECLARE_DO_FUN( do_spawntreasuremap );
DECLARE_DO_FUN( do_reserved );
DECLARE_DO_FUN( do_triggers );

DECLARE_DO_FUN(	do_liqedit	);
DECLARE_DO_FUN( do_liqlist	);

DECLARE_DO_FUN( do_skedit );
DECLARE_DO_FUN( do_sklist );
DECLARE_DO_FUN( do_skshow );

DECLARE_DO_FUN( do_sgedit );
DECLARE_DO_FUN( do_sglist );
DECLARE_DO_FUN( do_sgshow );

DECLARE_DO_FUN( do_readycheck );
DECLARE_DO_FUN( do_ignite );
DECLARE_DO_FUN( do_extinguish );

//DECLARE_DO_FUN( do_speed	);
//DECLARE_DO_FUN( do_steer	);
//DECLARE_DO_FUN( do_navigate );

DECLARE_DO_FUN( do_immstrike );
