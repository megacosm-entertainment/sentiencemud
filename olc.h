
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
#define VLEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
// Blueprints
#define BSEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define BPEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )
#define DNGEDIT( fun )		bool fun( CHAR_DATA *ch, char *argument )

/*
 * Interpreter Prototypes
 */
void    aedit 	( CHAR_DATA *ch, char *argument );
void    hedit   ( CHAR_DATA *ch, char *argument );
void    medit 	( CHAR_DATA *ch, char *argument );
void	mpedit	( CHAR_DATA *ch, char *argument );
void    oedit 	( CHAR_DATA *ch, char *argument );
void    opedit  ( CHAR_DATA *ch, char *argument );
void    qedit	( CHAR_DATA *ch, char *argument );
void    redit 	( CHAR_DATA *ch, char *argument );
void    rpedit  ( CHAR_DATA *ch, char *argument );
void    shedit  ( CHAR_DATA *ch, char *argument );
void	tedit	( CHAR_DATA *ch, char *argument );
void	tpedit	( CHAR_DATA *ch, char *argument );
void	pedit	( CHAR_DATA *ch, char *argument );
/* VIZZWILDS */
void    wedit   ( CHAR_DATA *ch, char *argument );
void    vledit  ( CHAR_DATA *ch, char *argument );
// Blueprints
void    bsedit 	( CHAR_DATA *ch, char *argument );	// Blueprint Sections
void    bpedit 	( CHAR_DATA *ch, char *argument );	// Blueprints
void	dngedit ( CHAR_DATA *ch, char *argument );	// Dungeons

void	apedit	( CHAR_DATA *ch, char *argument );
void	ipedit	( CHAR_DATA *ch, char *argument );
void	dpedit	( CHAR_DATA *ch, char *argument );


/*
 * OLC Constants
 */
#define MAX_MOB	1		/* Default maximum number for resetting mobs */


/*
 * Structure for an OLC editor command.
 */
struct olc_cmd_type
{
    char * const	name;
    OLC_FUN *		olc_fun;
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
bool edit_deltrigger(LLIST **list, int index);


/*
 * Interpreter Table Prototypes
 */
extern const struct olc_cmd_type	aedit_table[];
extern const struct olc_cmd_type	hedit_table[];
extern const struct olc_cmd_type	medit_table[];
extern const struct olc_cmd_type	mpedit_table[];
extern const struct olc_cmd_type	oedit_table[];
extern const struct olc_cmd_type	redit_table[];
extern const struct olc_cmd_type        opedit_table[];
extern const struct olc_cmd_type        rpedit_table[];
extern const struct olc_cmd_type        shedit_table[];
extern const struct olc_cmd_type        tedit_table[];
extern const struct olc_cmd_type        tpedit_table[];
extern const struct olc_cmd_type        pedit_table[];
/* VIZZWILDS */
extern const struct olc_cmd_type        wedit_table[];
extern const struct olc_cmd_type	vledit_table[];
// Blueprints
extern const struct olc_cmd_type	bsedit_table[];
extern const struct olc_cmd_type	bpedit_table[];
extern const struct olc_cmd_type	dngedit_table[];


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
/* VIZZWILDS */
DECLARE_DO_FUN( do_wedit        );
DECLARE_DO_FUN( do_vledit       );
DECLARE_DO_FUN( do_bsedit       );
DECLARE_DO_FUN( do_dngedit       );


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
DECLARE_OLC_FUN( aedit_open		);
DECLARE_OLC_FUN( aedit_placetype	);
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
DECLARE_OLC_FUN( redit_room		);
DECLARE_OLC_FUN( redit_room2		);
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
DECLARE_OLC_FUN( oedit_extra2           );
DECLARE_OLC_FUN( oedit_extra3           );
DECLARE_OLC_FUN( oedit_extra4           );
DECLARE_OLC_FUN( oedit_fragility	);
DECLARE_OLC_FUN( oedit_level            );
DECLARE_OLC_FUN( oedit_long		);
DECLARE_OLC_FUN( oedit_material		);
DECLARE_OLC_FUN( oedit_name		);
DECLARE_OLC_FUN( oedit_next		);
DECLARE_OLC_FUN( oedit_prev		);
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

/*
 * Mobile Editor Prototypes
 */
DECLARE_OLC_FUN( medit_ac		);
DECLARE_OLC_FUN( medit_act		);
DECLARE_OLC_FUN( medit_act2		);
DECLARE_OLC_FUN( medit_addmprog		);
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
DECLARE_OLC_FUN( shedit_addmob		);
DECLARE_OLC_FUN( shedit_addwaypoint	);
DECLARE_OLC_FUN( shedit_captain		);
DECLARE_OLC_FUN( shedit_coord		);
DECLARE_OLC_FUN( shedit_create		);
DECLARE_OLC_FUN( shedit_delmob		);
DECLARE_OLC_FUN( shedit_delwaypoint	);
DECLARE_OLC_FUN( shedit_flag		);
DECLARE_OLC_FUN( shedit_chance		);
DECLARE_OLC_FUN( shedit_initial		);
DECLARE_OLC_FUN( shedit_list		);
DECLARE_OLC_FUN( shedit_name		);
DECLARE_OLC_FUN( shedit_npc		);
DECLARE_OLC_FUN( shedit_npcsub		);
DECLARE_OLC_FUN( shedit_show		);
DECLARE_OLC_FUN( shedit_type		);

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

/* VIZZWILDS */
/* Wilds Editor */
DECLARE_OLC_FUN( wedit_create           );
DECLARE_OLC_FUN( wedit_delete           );
DECLARE_OLC_FUN( wedit_show             );
DECLARE_OLC_FUN( wedit_name             );
DECLARE_OLC_FUN( wedit_terrain          );
DECLARE_OLC_FUN( wedit_vlink            );

/* VLink Editor */
DECLARE_OLC_FUN( vledit_show            );


/* Blueprint Section Editor */
DECLARE_OLC_FUN( bsedit_list			);
DECLARE_OLC_FUN( bsedit_show			);
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

// Dungeon Editor
DECLARE_OLC_FUN( dngedit_list			);
DECLARE_OLC_FUN( dngedit_show			);
DECLARE_OLC_FUN( dngedit_create			);
DECLARE_OLC_FUN( dngedit_name			);
DECLARE_OLC_FUN( dngedit_description	);
DECLARE_OLC_FUN( dngedit_comments		);
DECLARE_OLC_FUN( dngedit_areawho		);
DECLARE_OLC_FUN( dngedit_floors			);
DECLARE_OLC_FUN( dngedit_entry			);
DECLARE_OLC_FUN( dngedit_exit			);
DECLARE_OLC_FUN( dngedit_flags			);
DECLARE_OLC_FUN( dngedit_zoneout		);
DECLARE_OLC_FUN( dngedit_portalout		);
DECLARE_OLC_FUN( dngedit_mountout		);
DECLARE_OLC_FUN( dngedit_special		);
DECLARE_OLC_FUN( dngedit_varset	);
DECLARE_OLC_FUN( dngedit_varclear	);
DECLARE_OLC_FUN( dngedit_adddprog		);
DECLARE_OLC_FUN( dngedit_deldprog		);


DECLARE_OLC_FUN( apedit_list		);
DECLARE_OLC_FUN( apedit_create		);

DECLARE_OLC_FUN( ipedit_list		);
DECLARE_OLC_FUN( ipedit_create		);

DECLARE_OLC_FUN( dpedit_list		);
DECLARE_OLC_FUN( dpedit_create		);

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
#define EDIT_ROOM(ch, room)		do { room = ch->in_room; if(!room || IS_SET(room->room2_flags,ROOM_VIRTUAL_ROOM) || room->source) return FALSE; } while(0)
#define EDIT_ROOM_VOID(ch, room)	do { room = ch->in_room; if(!room || IS_SET(room->room2_flags,ROOM_VIRTUAL_ROOM) || room->source) return; } while(0)
#define EDIT_ROOM_SIMPLE(ch,room)	( room = ch->in_room )
#define EDIT_RPCODE(ch, code)   ( code = (SCRIPT_DATA*)ch->desc->pEdit )
#define EDIT_SHIP(ch, ship)     ( ship = (NPC_SHIP_INDEX_DATA *)ch->desc->pEdit )
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

