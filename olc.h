#ifndef __OLC_H__
#define __OLC_H__
/**************************************************************************
 *  File: olc.h                                                            *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*
 * New typedefs.
 */
typedef	bool OLC_FUN		args( ( CHAR_DATA *ch, char *argument ) );

#define DECLARE_OLC_FUN( fun )	OLC_FUN    fun

/*
 * Connected states for editor.
 */
#define ED_NONE		0
#define ED_AREA		1
#define ED_ROOM		2
#define ED_OBJECT	3
#define ED_MOBILE	4
#define ED_MPCODE	5
#define ED_OPCODE       6
#define ED_RPCODE       7
#define ED_SHIP         8
#define ED_HELP		9
#define ED_TPCODE	10
#define ED_TOKEN	11
#define ED_PROJECT	12
#define ED_RSG		13
/* VIZZWILDS */
#define ED_WILDS	14
#define ED_VLINK	15
// Blueprints
#define ED_BPSECT		16
#define ED_BLUEPRINT	17
#define ED_DUNGEON		18

#define ED_APCODE	19
#define ED_IPCODE	20
#define ED_DPCODE	21
#define ED_CMDEDIT  22
#define ED_CHANGESET	23
#define ED_ACCNOTE	24
#define ED_CHARNOTE	28
#define ED_CHLOG 25
#define ED_SOCIAL 26
#define ED_GAMESETTING    27  // Or whatever value is appropriate
#define ED_RACE           29
#define ED_TRAIT          30
#define ED_SKILL          31
#define ED_GROUP          32
#define ED_SONG           33
#define ED_CLASS          34
#define ED_LIQUID         35
#define ED_MATERIAL       36
#define ED_CORPSE        37
#define ED_SECTOR        38
#define ED_REPUTATION    39
#define ED_EVENT         40




#define AEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define HEDIT( fun )            bool fun( CHAR_DATA *ch, char *argument )
#define MEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define OEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define QEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define REDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define SHEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define TEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define PEDIT(fun)		bool fun( CHAR_DATA *ch, char *argument )
/* VIZZWILDS */
#define WEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
// Blueprints
#define BSEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define BPEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define DNGEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define CMDEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define SOCEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define RACEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define TRAITEDIT( fun )      bool fun( CHAR_DATA *ch, char *argument )
#define SKEDIT( fun )         bool fun( CHAR_DATA *ch, char *argument )
#define GREDIT( fun )         bool fun( CHAR_DATA *ch, char *argument )
#define SOEDIT( fun )         bool fun( CHAR_DATA *ch, char *argument )
#define CLSEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define LIQEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define MATEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define CORPSEDIT( fun )      bool fun( CHAR_DATA *ch, char *argument )
#define SECTOREDIT( fun )     bool fun( CHAR_DATA *ch, char *argument )
#define REPEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )
#define EVTEDIT( fun )        bool fun( CHAR_DATA *ch, char *argument )

/*
 * Interpreter Prototypes
 *
 * Framework-migrated editors are dispatched via the editor registry
 * (olc_find_editor_by_type + olc_editor_interp) — their interpreter
 * functions are local to their own .c files.  Only non-framework
 * editors need extern declarations here.
 */
void    hedit   ( CHAR_DATA *ch, char *argument );
void    qedit	( CHAR_DATA *ch, char *argument );
void    gameedit ( CHAR_DATA *ch, char *argument );


/*
 * OLC Constants
 */
#define MAX_MOB	1		/* Default maximum number for resetting mobs */


/*
 * Structure for an OLC editor command.
 *
 * The min_staff_rank field is optional. When 0 (default), the command
 * inherits the editor's own permission check. When set to a STAFF_*
 * value, only characters with that rank or higher may use the command.
 * C99 designated initializers ensure existing tables default to 0.
 */
struct olc_cmd_type
{
    char * const	name;
    OLC_FUN *		olc_fun;
    int			min_staff_rank;     /**< STAFF_* minimum, 0 = inherit editor perm */
};


/*
 * Structure for an OLC editor startup command.
 */
struct	editor_cmd_type
{
    char * const	name;
    DO_FUN *		do_fun;
};


/*
 * Prototypes
 */
bool edit_done( CHAR_DATA *ch );
bool show_commands( CHAR_DATA *ch, char *argument );
bool show_help( CHAR_DATA *ch, char *argument );
bool show_version( CHAR_DATA *ch, char *argument );
int cd_phrase_lookup( int condition, char *phrase );
void add_reset( ROOM_INDEX_DATA *room, RESET_DATA *pReset, int index );

/* Script management helpers (olc_act.c) */
bool edit_script_attached(LLIST **progs, SCRIPT_DATA *script);
bool edit_trigger_exists(LLIST **progs, SCRIPT_DATA *script, int trig_type, const char *phrase);
bool edit_delscript(LLIST **progs, SCRIPT_DATA *script);
bool edit_deltrigger_specific(LLIST **progs, SCRIPT_DATA *script, int trig_type, const char *phrase);


/*
 * Command Table Externs
 *
 * Framework-migrated editors store their command tables in OLC_EDITOR_DEF
 * structs local to their own .c files — no extern needed.  Only tables
 * referenced directly by olc.c fallback code need externs here.
 */
extern const struct olc_cmd_type	hedit_table[];


/*
 * Editor Commands.
 */
DECLARE_DO_FUN( do_aedit        );
DECLARE_DO_FUN( do_hedit        );
DECLARE_DO_FUN( do_medit        );
DECLARE_DO_FUN( do_mpedit	);
DECLARE_DO_FUN( do_oedit        );
DECLARE_DO_FUN( do_opedit       );
DECLARE_DO_FUN( do_redit        );
DECLARE_DO_FUN( do_rpedit       );
DECLARE_DO_FUN( do_shedit       );
DECLARE_DO_FUN( do_tedit       );
DECLARE_DO_FUN( do_tpedit       );
DECLARE_DO_FUN( do_pedit       );
DECLARE_DO_FUN( do_rsgedit     );
/* VIZZWILDS */
DECLARE_DO_FUN( do_wedit        );
DECLARE_DO_FUN( do_bsedit       );
DECLARE_DO_FUN( do_dngedit       );
DECLARE_DO_FUN( do_cmdedit      );
DECLARE_DO_FUN( do_racedit      );
DECLARE_DO_FUN( do_traitedit    );
DECLARE_DO_FUN( do_skedit       );
DECLARE_DO_FUN( do_gredit       );
DECLARE_DO_FUN( do_soedit       );
DECLARE_DO_FUN( do_clsedit      );
DECLARE_DO_FUN( do_liqedit      );
DECLARE_DO_FUN( do_matedit      );
DECLARE_DO_FUN( do_corpsedit    );
DECLARE_DO_FUN( do_sectoredit   );
DECLARE_DO_FUN( do_repedit      );
DECLARE_DO_FUN( do_evtedit      );


/*
 * Area Editor Prototypes
 */
DECLARE_OLC_FUN( aedit_add_trade	);
DECLARE_OLC_FUN( aedit_age		);
DECLARE_OLC_FUN( aedit_airshipland	);
DECLARE_OLC_FUN( aedit_areawho		);
DECLARE_OLC_FUN( aedit_builder		);
DECLARE_OLC_FUN( aedit_comments     );
DECLARE_OLC_FUN( aedit_create		);
DECLARE_OLC_FUN( aedit_credits		);
DECLARE_OLC_FUN( aedit_desc     );
DECLARE_OLC_FUN( aedit_file		);
DECLARE_OLC_FUN( aedit_flags		);
DECLARE_OLC_FUN( aedit_land_x		);
DECLARE_OLC_FUN( aedit_land_y		);
DECLARE_OLC_FUN( aedit_name		);
DECLARE_OLC_FUN( aedit_notes    );
DECLARE_OLC_FUN( aedit_open		);
DECLARE_OLC_FUN( aedit_placetype	);
DECLARE_OLC_FUN( aedit_regions     );
DECLARE_OLC_FUN( aedit_recall		);
DECLARE_OLC_FUN( aedit_remove_trade	);
DECLARE_OLC_FUN( aedit_repop		);
DECLARE_OLC_FUN( aedit_security		);
DECLARE_OLC_FUN( aedit_set_trade	);
DECLARE_OLC_FUN( aedit_show		);
DECLARE_OLC_FUN( aedit_view_trade	);
DECLARE_OLC_FUN( aedit_vnum		);
DECLARE_OLC_FUN( aedit_x		);
DECLARE_OLC_FUN( aedit_y		);
DECLARE_OLC_FUN( aedit_postoffice	);
DECLARE_OLC_FUN( aedit_varset	);
DECLARE_OLC_FUN( aedit_varclear	);
DECLARE_OLC_FUN( aedit_addaprog		);
DECLARE_OLC_FUN( aedit_delaprog		);
DECLARE_OLC_FUN( aedit_wilds		);
DECLARE_OLC_FUN( aedit_levels       );

/*
 * Room Editor Prototypes
 */
DECLARE_OLC_FUN( redit_addcdesc		);
DECLARE_OLC_FUN( redit_addrprog		);
DECLARE_OLC_FUN( redit_comments     );
DECLARE_OLC_FUN( redit_coords		);
DECLARE_OLC_FUN( redit_create		);
DECLARE_OLC_FUN( redit_delcdesc		);
DECLARE_OLC_FUN( redit_delrprog		);
DECLARE_OLC_FUN( redit_desc		);
DECLARE_OLC_FUN( redit_dislink		);
DECLARE_OLC_FUN( redit_down		);
DECLARE_OLC_FUN( redit_east		);
DECLARE_OLC_FUN( redit_ed		);
DECLARE_OLC_FUN( redit_editcdesc	);
DECLARE_OLC_FUN( redit_format		);
DECLARE_OLC_FUN( redit_heal		);
DECLARE_OLC_FUN( redit_locale		);
DECLARE_OLC_FUN( redit_mana		);
DECLARE_OLC_FUN( redit_move		);
DECLARE_OLC_FUN( redit_mreset		);
DECLARE_OLC_FUN( redit_name		);
DECLARE_OLC_FUN( redit_north		);
DECLARE_OLC_FUN( redit_northeast	);
DECLARE_OLC_FUN( redit_northwest	);
DECLARE_OLC_FUN( redit_oreset		);
DECLARE_OLC_FUN( redit_owner		);
DECLARE_OLC_FUN( redit_recall       );
DECLARE_OLC_FUN( redit_region       );
DECLARE_OLC_FUN( redit_room		);
//DECLARE_OLC_FUN( redit_room2		);
DECLARE_OLC_FUN( redit_sector		);
DECLARE_OLC_FUN( redit_show		);
DECLARE_OLC_FUN( redit_south		);
DECLARE_OLC_FUN( redit_southeast	);
DECLARE_OLC_FUN( redit_southwest	);
DECLARE_OLC_FUN( redit_up		);
DECLARE_OLC_FUN( redit_west		);
DECLARE_OLC_FUN( redit_varset	);
DECLARE_OLC_FUN( redit_varclear	);
DECLARE_OLC_FUN( redit_persist  );


/*
 * Object Editor Prototypes
 */
DECLARE_OLC_FUN( oedit_addaffect	);
DECLARE_OLC_FUN( oedit_addapply		);
DECLARE_OLC_FUN( oedit_addimmune	);
DECLARE_OLC_FUN( oedit_addoprog		);
DECLARE_OLC_FUN( oedit_addspell		);
DECLARE_OLC_FUN( oedit_addskill		);
DECLARE_OLC_FUN( oedit_addcatalyst	);
DECLARE_OLC_FUN( oedit_addtype		);
DECLARE_OLC_FUN( oedit_affect           );
DECLARE_OLC_FUN( oedit_allowed_fixed	);
DECLARE_OLC_FUN( oedit_armour_strength	);
DECLARE_OLC_FUN( oedit_comments     );
DECLARE_OLC_FUN( oedit_condition        );
DECLARE_OLC_FUN( oedit_cost		);
DECLARE_OLC_FUN( oedit_create		);
DECLARE_OLC_FUN( oedit_delaffect	);
DECLARE_OLC_FUN( oedit_delimmune	);
DECLARE_OLC_FUN( oedit_delcatalyst	);
DECLARE_OLC_FUN( oedit_deloprog		);
DECLARE_OLC_FUN( oedit_delspell		);
DECLARE_OLC_FUN( oedit_desc		);
DECLARE_OLC_FUN( oedit_ed		);
DECLARE_OLC_FUN( oedit_extra            );
//DECLARE_OLC_FUN( oedit_extra2           );
//DECLARE_OLC_FUN( oedit_extra3           );
//DECLARE_OLC_FUN( oedit_extra4           );
DECLARE_OLC_FUN( oedit_fragility	);
DECLARE_OLC_FUN( oedit_level            );
DECLARE_OLC_FUN( oedit_long		);
DECLARE_OLC_FUN( oedit_material		);
DECLARE_OLC_FUN( oedit_name		);
DECLARE_OLC_FUN( oedit_next		);
DECLARE_OLC_FUN( oedit_prev		);
DECLARE_OLC_FUN( oedit_removetype	);
DECLARE_OLC_FUN( oedit_short		);
DECLARE_OLC_FUN( oedit_show		);
DECLARE_OLC_FUN( oedit_sign		);
DECLARE_OLC_FUN( oedit_timer		);
DECLARE_OLC_FUN( oedit_type             );
DECLARE_OLC_FUN( oedit_update		);
DECLARE_OLC_FUN( oedit_value0		);
DECLARE_OLC_FUN( oedit_value1		);
DECLARE_OLC_FUN( oedit_value2		);
DECLARE_OLC_FUN( oedit_value3		);
DECLARE_OLC_FUN( oedit_value4		);
DECLARE_OLC_FUN( oedit_value5		);
DECLARE_OLC_FUN( oedit_value6		);
DECLARE_OLC_FUN( oedit_value7		);
DECLARE_OLC_FUN( oedit_wear             );
DECLARE_OLC_FUN( oedit_weight		);
DECLARE_OLC_FUN( oedit_skeywds			);
DECLARE_OLC_FUN( oedit_varset	);
DECLARE_OLC_FUN( oedit_varclear	);
DECLARE_OLC_FUN( oedit_persist  );
DECLARE_OLC_FUN( oedit_lock		);
DECLARE_OLC_FUN( oedit_waypoints	);

/* Type-specific subcommands (oedit_types.c) */
DECLARE_OLC_FUN( oedit_armor		);
DECLARE_OLC_FUN( oedit_bodypart		);
DECLARE_OLC_FUN( oedit_book		);
DECLARE_OLC_FUN( oedit_cart		);
DECLARE_OLC_FUN( oedit_compass		);
DECLARE_OLC_FUN( oedit_container	);
DECLARE_OLC_FUN( oedit_corpse		);
DECLARE_OLC_FUN( oedit_drink		);
DECLARE_OLC_FUN( oedit_food		);
DECLARE_OLC_FUN( oedit_furniture	);
DECLARE_OLC_FUN( oedit_herb		);
DECLARE_OLC_FUN( oedit_ink		);
DECLARE_OLC_FUN( oedit_instrument	);
DECLARE_OLC_FUN( oedit_jewelry		);
DECLARE_OLC_FUN( oedit_light		);
DECLARE_OLC_FUN( oedit_map		);
DECLARE_OLC_FUN( oedit_mist		);
DECLARE_OLC_FUN( oedit_money		);
DECLARE_OLC_FUN( oedit_page		);
DECLARE_OLC_FUN( oedit_portal		);
DECLARE_OLC_FUN( oedit_scroll		);
DECLARE_OLC_FUN( oedit_seed		);
DECLARE_OLC_FUN( oedit_sextant		);
DECLARE_OLC_FUN( oedit_ship		);
DECLARE_OLC_FUN( oedit_tattoo		);
DECLARE_OLC_FUN( oedit_telescope	);
DECLARE_OLC_FUN( oedit_tool		);
DECLARE_OLC_FUN( oedit_trade		);
DECLARE_OLC_FUN( oedit_wand		);
DECLARE_OLC_FUN( oedit_weapon		);
DECLARE_OLC_FUN( oedit_weaponcon	);

/*
 * Mobile Editor Prototypes
 */
DECLARE_OLC_FUN( medit_ac		);
DECLARE_OLC_FUN( medit_act		);
DECLARE_OLC_FUN( medit_act2		);
DECLARE_OLC_FUN( medit_addmprog		);
DECLARE_OLC_FUN( medit_addreputation	);
DECLARE_OLC_FUN( medit_addquest		);
DECLARE_OLC_FUN( medit_affect		);
DECLARE_OLC_FUN( medit_affect2	        );
DECLARE_OLC_FUN( medit_align		);
DECLARE_OLC_FUN( medit_attacks 		);
DECLARE_OLC_FUN( medit_comments     );
DECLARE_OLC_FUN( medit_create		);
DECLARE_OLC_FUN( medit_damdice		);
DECLARE_OLC_FUN( medit_damtype		);
DECLARE_OLC_FUN( medit_delmprog		);
DECLARE_OLC_FUN( medit_delreputation	);
DECLARE_OLC_FUN( medit_delquest		);
DECLARE_OLC_FUN( medit_desc		);
DECLARE_OLC_FUN( medit_form		);
DECLARE_OLC_FUN( medit_gold		);
DECLARE_OLC_FUN( medit_hitdice		);
DECLARE_OLC_FUN( medit_hitroll		);
DECLARE_OLC_FUN( medit_immune 	 	);
DECLARE_OLC_FUN( medit_level		);
DECLARE_OLC_FUN( medit_long		);
DECLARE_OLC_FUN( medit_manadice		);
DECLARE_OLC_FUN( medit_material		);
DECLARE_OLC_FUN( medit_movedice		);
DECLARE_OLC_FUN( medit_name		);
DECLARE_OLC_FUN( medit_next 		);
DECLARE_OLC_FUN( medit_off		);
DECLARE_OLC_FUN( medit_owner		);
DECLARE_OLC_FUN( medit_part		);
DECLARE_OLC_FUN( medit_position		);
DECLARE_OLC_FUN( medit_prev 		);
DECLARE_OLC_FUN( medit_race		);
DECLARE_OLC_FUN( medit_res		);
DECLARE_OLC_FUN( medit_sex		);
DECLARE_OLC_FUN( medit_shop		);
DECLARE_OLC_FUN( medit_short		);
DECLARE_OLC_FUN( medit_show		);
DECLARE_OLC_FUN( medit_sign		);
DECLARE_OLC_FUN( medit_size		);
DECLARE_OLC_FUN( medit_spec		);
DECLARE_OLC_FUN( medit_vuln		);
DECLARE_OLC_FUN( medit_skeywds	);
DECLARE_OLC_FUN( medit_varset	);
DECLARE_OLC_FUN( medit_varclear	);
DECLARE_OLC_FUN( medit_corpsetype	);
DECLARE_OLC_FUN( medit_corpsevnum	);
DECLARE_OLC_FUN( medit_zombievnum	);
DECLARE_OLC_FUN( medit_persist  );
DECLARE_OLC_FUN( medit_questor  );
DECLARE_OLC_FUN( medit_boss		);
DECLARE_OLC_FUN( medit_crew		);

/* Any script editor */
DECLARE_OLC_FUN( scriptedit_show	);
DECLARE_OLC_FUN( scriptedit_code	);
DECLARE_OLC_FUN( scriptedit_depth	);
DECLARE_OLC_FUN( scriptedit_compile	);
DECLARE_OLC_FUN( scriptedit_name	);
DECLARE_OLC_FUN( scriptedit_flags	);
DECLARE_OLC_FUN( scriptedit_security	);
DECLARE_OLC_FUN( scriptedit_comments );

/* Mobprog editor */
DECLARE_OLC_FUN( mpedit_create		);
DECLARE_OLC_FUN( mpedit_list		);

/* Objprog editor */
DECLARE_OLC_FUN( opedit_create		);
DECLARE_OLC_FUN( opedit_list		);

/* Roomprog editor */
DECLARE_OLC_FUN( rpedit_create		);
DECLARE_OLC_FUN( rpedit_list		);

/* Tokprog editor */
DECLARE_OLC_FUN( tpedit_list		);
DECLARE_OLC_FUN( tpedit_create		);


/* Ship editor */
DECLARE_OLC_FUN( shedit_create		);
DECLARE_OLC_FUN( shedit_list		);
DECLARE_OLC_FUN( shedit_name		);
DECLARE_OLC_FUN( shedit_show		);
DECLARE_OLC_FUN( shedit_class		);
DECLARE_OLC_FUN( shedit_blueprint	);
DECLARE_OLC_FUN( shedit_desc		);
DECLARE_OLC_FUN( shedit_object		);
DECLARE_OLC_FUN( shedit_hit			);
DECLARE_OLC_FUN( shedit_guns		);
DECLARE_OLC_FUN( shedit_crew		);
DECLARE_OLC_FUN( shedit_move		);
DECLARE_OLC_FUN( shedit_weight		);
DECLARE_OLC_FUN( shedit_capacity	);
DECLARE_OLC_FUN( shedit_flags		);
DECLARE_OLC_FUN( shedit_armor		);
DECLARE_OLC_FUN( shedit_keys		);
DECLARE_OLC_FUN( shedit_turning		);
DECLARE_OLC_FUN( shedit_oars		);

/* Help Editor */
DECLARE_OLC_FUN( hedit_show    		);
DECLARE_OLC_FUN( hedit_make 		);
DECLARE_OLC_FUN( hedit_edit 		);
DECLARE_OLC_FUN( hedit_addcat 		);
DECLARE_OLC_FUN( hedit_opencat 		);
DECLARE_OLC_FUN( hedit_upcat 		);
DECLARE_OLC_FUN( hedit_move 		);
DECLARE_OLC_FUN( hedit_remcat 		);
DECLARE_OLC_FUN( hedit_shiftcat		);
DECLARE_OLC_FUN( hedit_name 		);
DECLARE_OLC_FUN( hedit_description	);
DECLARE_OLC_FUN( hedit_text   		);
DECLARE_OLC_FUN( hedit_level   		);
DECLARE_OLC_FUN( hedit_security		);
DECLARE_OLC_FUN( hedit_keywords   	);
DECLARE_OLC_FUN( hedit_delete 		);
DECLARE_OLC_FUN( hedit_builder		);
DECLARE_OLC_FUN( hedit_addtopic		);
DECLARE_OLC_FUN( hedit_remtopic		);

/* Token Editor */
DECLARE_OLC_FUN( tedit_show		);
DECLARE_OLC_FUN( tedit_comments     );
DECLARE_OLC_FUN( tedit_create		);
DECLARE_OLC_FUN( tedit_name		);
DECLARE_OLC_FUN( tedit_type		);
DECLARE_OLC_FUN( tedit_flags		);
DECLARE_OLC_FUN( tedit_timer		);
DECLARE_OLC_FUN( tedit_ed		);
DECLARE_OLC_FUN( tedit_description	);
DECLARE_OLC_FUN( tedit_value		);
DECLARE_OLC_FUN( tedit_valuename	);
DECLARE_OLC_FUN( tedit_varset	);
DECLARE_OLC_FUN( tedit_varclear	);
DECLARE_OLC_FUN( tedit_addtprog		);
DECLARE_OLC_FUN( tedit_deltprog		);

/* Project editor */
DECLARE_OLC_FUN( pedit_create		);
DECLARE_OLC_FUN( pedit_delete		);
DECLARE_OLC_FUN( pedit_show		);
DECLARE_OLC_FUN( pedit_name		);
DECLARE_OLC_FUN( pedit_area		);
DECLARE_OLC_FUN( pedit_leader		);
DECLARE_OLC_FUN( pedit_summary		);
DECLARE_OLC_FUN( pedit_description	);
DECLARE_OLC_FUN( pedit_security		);
DECLARE_OLC_FUN( pedit_pflag		);
DECLARE_OLC_FUN( pedit_builder		);
DECLARE_OLC_FUN( pedit_completed	);

/* Random String Generator editor */
DECLARE_OLC_FUN( rsgedit_list            );
DECLARE_OLC_FUN( rsgedit_create          );
DECLARE_OLC_FUN( rsgedit_show            );
DECLARE_OLC_FUN( rsgedit_pattern         );
DECLARE_OLC_FUN( rsgedit_class           );
DECLARE_OLC_FUN( rsgedit_generate        );
DECLARE_OLC_FUN( rsgedit_pattern_list    );
DECLARE_OLC_FUN( rsgedit_pattern_create  );
DECLARE_OLC_FUN( rsgedit_pattern_show    );
DECLARE_OLC_FUN( rsgedit_pattern_delete  );
DECLARE_OLC_FUN( rsgedit_pattern_help    );
DECLARE_OLC_FUN( rsgedit_class_list      );
DECLARE_OLC_FUN( rsgedit_class_create    );
DECLARE_OLC_FUN( rsgedit_class_show      );
DECLARE_OLC_FUN( rsgedit_class_delete    );
DECLARE_OLC_FUN( rsgedit_class_add       );
DECLARE_OLC_FUN( rsgedit_class_edit      );
DECLARE_OLC_FUN( rsgedit_class_remove    );
DECLARE_OLC_FUN( rsgedit_class_help      );

/* VIZZWILDS */
/* Wilds Editor */
DECLARE_OLC_FUN( wedit_create           );
DECLARE_OLC_FUN( wedit_delete           );
DECLARE_OLC_FUN( wedit_show             );
DECLARE_OLC_FUN( wedit_name             );
DECLARE_OLC_FUN( wedit_region           );
DECLARE_OLC_FUN( wedit_placetype        );
DECLARE_OLC_FUN( wedit_terrain          );
DECLARE_OLC_FUN( wedit_vlink            );

/* Blueprint Section Editor */
DECLARE_OLC_FUN( bsedit_list			);
DECLARE_OLC_FUN( bsedit_show			);
DECLARE_OLC_FUN( bsedit_maze			);
DECLARE_OLC_FUN( bsedit_create			);
DECLARE_OLC_FUN( bsedit_name			);
DECLARE_OLC_FUN( bsedit_description		);
DECLARE_OLC_FUN( bsedit_comments		);
DECLARE_OLC_FUN( bsedit_recall			);
DECLARE_OLC_FUN( bsedit_rooms			);
DECLARE_OLC_FUN( bsedit_link			);
DECLARE_OLC_FUN( bsedit_flags			);
DECLARE_OLC_FUN( bsedit_type			);

// Blueprint Editor
DECLARE_OLC_FUN( bpedit_list			);
DECLARE_OLC_FUN( bpedit_show			);
DECLARE_OLC_FUN( bpedit_create			);
DECLARE_OLC_FUN( bpedit_name			);
DECLARE_OLC_FUN( bpedit_description		);
DECLARE_OLC_FUN( bpedit_comments		);
DECLARE_OLC_FUN( bpedit_areawho			);
DECLARE_OLC_FUN( bpedit_mode			);
DECLARE_OLC_FUN( bpedit_section			);
DECLARE_OLC_FUN( bpedit_static			);
DECLARE_OLC_FUN( bpedit_varset	);
DECLARE_OLC_FUN( bpedit_varclear	);
DECLARE_OLC_FUN( bpedit_addiprog		);
DECLARE_OLC_FUN( bpedit_deliprog		);
DECLARE_OLC_FUN( bpedit_repop			);
DECLARE_OLC_FUN( bpedit_flags			);

// Dungeon Editor
DECLARE_OLC_FUN( dngedit_list			);
DECLARE_OLC_FUN( dngedit_show			);
DECLARE_OLC_FUN( dngedit_mingroup		);
DECLARE_OLC_FUN( dngedit_maxgroup		);
DECLARE_OLC_FUN( dngedit_maxplayers		);
DECLARE_OLC_FUN( dngedit_deathrelease	);
DECLARE_OLC_FUN( dngedit_idletimeout	);
DECLARE_OLC_FUN( dngedit_create			);
DECLARE_OLC_FUN( dngedit_name			);
DECLARE_OLC_FUN( dngedit_description	);
DECLARE_OLC_FUN( dngedit_comments		);
DECLARE_OLC_FUN( dngedit_areawho		);
DECLARE_OLC_FUN( dngedit_floors			);
DECLARE_OLC_FUN( dngedit_levels         );
DECLARE_OLC_FUN( dngedit_entry			);
DECLARE_OLC_FUN( dngedit_exit			);
DECLARE_OLC_FUN( dngedit_flags			);
DECLARE_OLC_FUN( dngedit_zoneout		);
DECLARE_OLC_FUN( dngedit_portalout		);
DECLARE_OLC_FUN( dngedit_mountout		);
DECLARE_OLC_FUN( dngedit_special		);
DECLARE_OLC_FUN( dngedit_varset			);
DECLARE_OLC_FUN( dngedit_varclear		);
DECLARE_OLC_FUN( dngedit_adddprog		);
DECLARE_OLC_FUN( dngedit_deldprog		);
DECLARE_OLC_FUN( dngedit_repop			);


DECLARE_OLC_FUN( apedit_list		);
DECLARE_OLC_FUN( apedit_create		);

DECLARE_OLC_FUN( ipedit_list		);
DECLARE_OLC_FUN( ipedit_create		);

DECLARE_OLC_FUN( dpedit_list		);
DECLARE_OLC_FUN( dpedit_create		);



DECLARE_OLC_FUN( cmdedit_create ); 
DECLARE_OLC_FUN( cmdedit_show ); 
DECLARE_OLC_FUN( cmdedit_delete );
DECLARE_OLC_FUN( cmdedit_name ); 
DECLARE_OLC_FUN( cmdedit_description );
DECLARE_OLC_FUN( cmdedit_comments ); 
DECLARE_OLC_FUN( cmdedit_type );
DECLARE_OLC_FUN( cmdedit_rank );
DECLARE_OLC_FUN( cmdedit_order );
DECLARE_OLC_FUN( cmdedit_position ); 
DECLARE_OLC_FUN( cmdedit_log );
DECLARE_OLC_FUN( cmdedit_enabled ); 
DECLARE_OLC_FUN( cmdedit_reason );
DECLARE_OLC_FUN( cmdedit_flags );
DECLARE_OLC_FUN( cmdedit_function );
DECLARE_OLC_FUN( cmdedit_help ); 
DECLARE_OLC_FUN( cmdedit_summary );
DECLARE_OLC_FUN( cmdedit_additional );

DECLARE_OLC_FUN(socialedit_show);
DECLARE_OLC_FUN(socialedit_create);
DECLARE_OLC_FUN(socialedit_name);
DECLARE_OLC_FUN(socialedit_char_no_arg);
DECLARE_OLC_FUN(socialedit_others_no_arg);
DECLARE_OLC_FUN(socialedit_char_found);
DECLARE_OLC_FUN(socialedit_others_found);
DECLARE_OLC_FUN(socialedit_vict_found);
DECLARE_OLC_FUN(socialedit_char_not_found);
DECLARE_OLC_FUN(socialedit_char_auto);
DECLARE_OLC_FUN(socialedit_others_auto);
DECLARE_OLC_FUN(socialedit_delete);
DECLARE_OLC_FUN(socialedit_list);
DECLARE_OLC_FUN(socialedit_save);

/*
 * Race Editor Prototypes
 */
DECLARE_OLC_FUN( racedit_show );
DECLARE_OLC_FUN( racedit_name );
DECLARE_OLC_FUN( racedit_summary );
DECLARE_OLC_FUN( racedit_whoname );
DECLARE_OLC_FUN( racedit_description );
DECLARE_OLC_FUN( racedit_comments );
DECLARE_OLC_FUN( racedit_playable );
DECLARE_OLC_FUN( racedit_starting );
DECLARE_OLC_FUN( racedit_pathrace );
DECLARE_OLC_FUN( racedit_alignment );
DECLARE_OLC_FUN( racedit_size );
DECLARE_OLC_FUN( racedit_stats );
DECLARE_OLC_FUN( racedit_maxstats );
DECLARE_OLC_FUN( racedit_maxvitals );
DECLARE_OLC_FUN( racedit_form );
DECLARE_OLC_FUN( racedit_parts );
DECLARE_OLC_FUN( racedit_act );
DECLARE_OLC_FUN( racedit_affects );
DECLARE_OLC_FUN( racedit_offensive );
DECLARE_OLC_FUN( racedit_immunities );
DECLARE_OLC_FUN( racedit_resistances );
DECLARE_OLC_FUN( racedit_vulnerabilities );
DECLARE_OLC_FUN( racedit_skills );
DECLARE_OLC_FUN( racedit_prerequisite );
DECLARE_OLC_FUN( racedit_remortinto );
DECLARE_OLC_FUN( racedit_trait );
DECLARE_OLC_FUN( racedit_save );
DECLARE_OLC_FUN( racedit_list );

/*
 * Trait Editor Prototypes
 */
DECLARE_OLC_FUN( traitedit_show );
DECLARE_OLC_FUN( traitedit_name );
DECLARE_OLC_FUN( traitedit_category );
DECLARE_OLC_FUN( traitedit_description );
DECLARE_OLC_FUN( traitedit_type );
DECLARE_OLC_FUN( traitedit_default );
DECLARE_OLC_FUN( traitedit_create );
DECLARE_OLC_FUN( traitedit_delete );
DECLARE_OLC_FUN( traitedit_save );
DECLARE_OLC_FUN( traitedit_list );

/*
 * Skill Editor Prototypes
 */
DECLARE_OLC_FUN( skedit_show );
DECLARE_OLC_FUN( skedit_list );
DECLARE_OLC_FUN( skedit_name );
DECLARE_OLC_FUN( skedit_display );
DECLARE_OLC_FUN( skedit_summary );
DECLARE_OLC_FUN( skedit_description );
DECLARE_OLC_FUN( skedit_comments );
DECLARE_OLC_FUN( skedit_helpkeyword );
DECLARE_OLC_FUN( skedit_difficulty );
DECLARE_OLC_FUN( skedit_mana );
DECLARE_OLC_FUN( skedit_beats );
DECLARE_OLC_FUN( skedit_target );
DECLARE_OLC_FUN( skedit_position );
DECLARE_OLC_FUN( skedit_damtype );
DECLARE_OLC_FUN( skedit_msgoff );
DECLARE_OLC_FUN( skedit_msgobj );
DECLARE_OLC_FUN( skedit_spellfun );
DECLARE_OLC_FUN( skedit_flags );
DECLARE_OLC_FUN( skedit_save );

/*
 * Group Editor Prototypes
 */
DECLARE_OLC_FUN( gredit_show );
DECLARE_OLC_FUN( gredit_list );
DECLARE_OLC_FUN( gredit_name );
DECLARE_OLC_FUN( gredit_add );
DECLARE_OLC_FUN( gredit_remove );
DECLARE_OLC_FUN( gredit_create );
DECLARE_OLC_FUN( gredit_delete );
DECLARE_OLC_FUN( gredit_save );

/*
 * Song Editor Prototypes
 */
DECLARE_OLC_FUN( soedit_show );
DECLARE_OLC_FUN( soedit_list );
DECLARE_OLC_FUN( soedit_name );
DECLARE_OLC_FUN( soedit_level );
DECLARE_OLC_FUN( soedit_mana );
DECLARE_OLC_FUN( soedit_beats );
DECLARE_OLC_FUN( soedit_target );
DECLARE_OLC_FUN( soedit_spell );
DECLARE_OLC_FUN( soedit_save );

/*
 * MEdit Trainer Sub-editor
 */
DECLARE_OLC_FUN( medit_trainer );

/*
 * Macros
 */
#define TOGGLE_BIT(var, bit)    ((var) ^= (bit))

/* Return pointers to what is being edited. */
#define EDIT_AREA(ch, area)	( area = (AREA_DATA *)ch->desc->pEdit )
#define EDIT_HELP(ch, help)     ( help = (HELP_DATA *)ch->desc->pEdit )
#define EDIT_MOB(ch, mob)	( mob = (MOB_INDEX_DATA *)ch->desc->pEdit )
#define EDIT_MPCODE(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
#define EDIT_OBJ(ch, obj)	( obj = (OBJ_INDEX_DATA *)ch->desc->pEdit )
#define EDIT_OPCODE(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
#define EDIT_QUEST(ch, quest)   ( quest = (QUEST_INDEX_DATA *)ch->desc->pEdit )
#define EDIT_ROOM(ch, room)		do { room = ch->in_room; if(!room || IS_SET(room->room_flag[1],ROOM_VIRTUAL_ROOM) || room->source) return false; } while(0)
#define EDIT_ROOM_VOID(ch, room)	do { room = ch->in_room; if(!room || IS_SET(room->room_flag[1],ROOM_VIRTUAL_ROOM) || room->source) return; } while(0)
#define EDIT_ROOM_SIMPLE(ch,room)	( room = ch->in_room )
#define EDIT_RPCODE(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
#define EDIT_TOKEN(ch, token)	( token = (TOKEN_INDEX_DATA *)ch->desc->pEdit )
#define EDIT_TPCODE(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
#define EDIT_PROJECT(ch, project) ( project = (PROJECT_DATA *)ch->desc->pEdit)
#define EDIT_SCRIPT(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
/* VIZZWILDS */
#define EDIT_WILDS(ch, Wilds)   ( Wilds = (WILDS_DATA *)ch->desc->pEdit )
#define EDIT_VLINK(ch, VLink)   ( VLink = (WILDS_VLINK *)ch->desc->pEdit )

#define EDIT_BPSECT(ch, bs)		( bs = (BLUEPRINT_SECTION *)ch->desc->pEdit )
#define EDIT_BLUEPRINT(ch, bp)	( bp = (BLUEPRINT *)ch->desc->pEdit )
#define EDIT_DUNGEON(ch, dng)	( dng = (DUNGEON_INDEX_DATA *)ch->desc->pEdit )

#define EDIT_SHIP(ch, ship)     ( ship = (SHIP_INDEX_DATA *)ch->desc->pEdit )
#define EDIT_CMD(ch, command)   ( command = (CMD_DATA *)ch->desc->pEdit )
#define EDIT_SOCIAL(ch, social)  (social = (struct social_type *)ch->desc->pEdit)
#define EDIT_RACE(ch, race)      (race = (RACE_DATA *)ch->desc->pEdit)
#define EDIT_TRAIT(ch, def)      (def = (TRAIT_DEF *)ch->desc->pEdit)
#define EDIT_SKILL(ch, skill)    (skill = (SKILL_DATA *)ch->desc->pEdit)
#define EDIT_GROUP(ch, group)    (group = (SKILL_GROUP *)ch->desc->pEdit)
#define EDIT_SONG(ch, song)      (song = (SONG_DATA *)ch->desc->pEdit)
#define EDIT_CLASS(ch, clazz)    (clazz = (CLASS_DATA *)ch->desc->pEdit)


/*
 * Prototypes
 */
void show_liqlist		args ( ( CHAR_DATA *ch ) );
void show_damlist		args ( ( CHAR_DATA *ch ) );
void show_material_list( CHAR_DATA *ch );
char *prog_type_to_name       args ( ( int type ) );
char *token_index_getvaluename args( (TOKEN_INDEX_DATA *token, int v) );

SHOP_STOCK_DATA *get_shop_stock_bypos(SHOP_DATA *shop, int nth);
bool check_range(long lower, long upper);

#define RSGEDIT( fun )           bool fun(CHAR_DATA *ch, char*argument)

#define EDIT_RSG(ch, rsg)   ( rsg = (RANDOM_STRING*)ch->desc->pEdit )
#endif /* !def __OLC_H__ */