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
*	    Russ Taylor (rtaylor@efn.org)				   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#ifndef __TABLES_H__
#define __TABLES_H__

struct npc_ship_type
{
	int npc_ship_type;
        int ship_type;
};

struct exp_table
{
	long exp;
};

struct hint_type
{
	char *hint;
};

struct wepHitDice
{
	int num;
	int type;
};

struct exp_type
{
	char class;
	long exp;
};

struct church_rank_type
{
	char * rank_name;
};

struct church_band_rank_type
{
	char * mrank_name; /* males */
	char * frank_name; /* females */
};

struct church_cult_rank_type
{
	char * mrank_name;
	char * frank_name;
};

struct church_order_rank_type
{
	char * mrank_name;
	char * frank_name;
};

struct church_church_rank_type
{
	char * mrank_name;
	char * frank_name;
};

struct court_rank_type
{
	char * mrank_name;
	char * frank_name;
};

struct talk_type
{
	char * from;
	char * to;
};

struct string_type
{
	char * name;
};

struct flag_type
{
    char *name;
    long bit;
    bool settable;
};

struct church_type
{
    char 	*name;
    char 	*who_name;
    int16_t 	hall;
    bool	independent; /* true for loners */
};

struct damDiceType
{
    int num;
};

struct position_type
{
    char *name;
    char *short_name;
};

struct sex_type
{
    char *name;
};

struct size_type
{
    char *name;
};

struct	bit_type
{
	const	struct	flag_type *	table;
	char *				help;
};



/* game tables */
extern  const   float   sin_table[];
extern  const   struct  npc_ship_type   npc_ship_table[];
extern	const	struct	npc_ship_hotspot_type	npc_ship_hotspot_table[];
extern	const	struct	talk_type	vampire_talk_table[];
extern	const	struct	court_rank_type	court_rank_table[];
extern	const	struct	church_rank_type	church_rank_table[];
extern	const	struct	church_band_rank_type	church_band_rank_table[];
extern	const	struct	church_cult_rank_type	church_cult_rank_table[];
extern	const	struct	church_order_rank_type	church_order_rank_table[];
extern	const	struct	church_church_rank_type	church_church_rank_table[];
extern  const   struct  exp_table	exp_per_level_table[];
extern	const	struct	position_type	position_table[];
extern	const	struct	sex_type	sex_table[];
extern	const	struct	size_type	size_table[];
extern  const   struct  damDiceType     damDiceTypeTable[];
extern  const   struct  wepHitDice      wepHitDiceTable[];
extern  const   struct  hint_type       hintsTable[];
extern  const   struct  string_type     fragile_table[];
extern  const   struct  flag_type armour_strength_table[];
extern  const   struct  string_type     object_damage_table[];

/* flag tables */
extern  const   struct  flag_type       token_flags[];
extern  const   struct  flag_type       area_flags[];
extern	const	struct	flag_type	place_flags[];
extern	const	struct	flag_type	act_flags[];
extern	const	struct	flag_type	act2_flags[];
extern	const	struct	flag_type	plr_flags[];
extern	const	struct	flag_type	plr2_flags[];
extern	const	struct	flag_type	affect_flags[];
extern	const	struct	flag_type	affect2_flags[];
extern	const	struct	flag_type	off_flags[];
extern	const	struct	flag_type	imm_flags[];
extern	const	struct	flag_type	form_flags[];
extern	const	struct	flag_type	part_flags[];
extern	const	struct	flag_type	comm_flags[];
extern	const	struct	flag_type	extra_flags[];
extern	const	struct	flag_type	extra2_flags[];
extern	const	struct	flag_type	extra3_flags[];
extern	const	struct	flag_type	extra4_flags[];
extern  const	struct	flag_type	church_flags[];
extern	const	struct	flag_type	wear_flags[];
extern	const	struct	flag_type	weapon_flags[];
extern	const	struct	flag_type	container_flags[];
extern	const	struct	flag_type	portal_flags[];
extern	const	struct	flag_type	room_flags[];
extern	const	struct	flag_type	room2_flags[];
extern	const	struct	flag_type	exit_flags[];
extern	const	struct	flag_type	lock_flags[];
extern	const	struct	flag_type	portal_exit_flags[];
extern 	const	struct  flag_type	mprog_flags[];
extern	const	struct	flag_type	oprog_flags[];
extern	const	struct	flag_type	rprog_flags[];
extern  const   struct  flag_type	room_condition_flags[];
extern	const	struct	flag_type	sector_flags[];
extern	const	struct	flag_type	door_resets[];
extern	const struct flag_type wear_loc_names[];
extern	const	struct	flag_type	wear_loc_strings[];
extern	const	struct	flag_type	wear_loc_flags[];
extern	const	struct	flag_type	res_flags[];
extern	const	struct	flag_type	imm_flags[];
extern	const	struct	flag_type	vuln_flags[];
extern	const	struct	flag_type	type_flags[];
extern	const	struct	flag_type	apply_flags[];
extern	const	struct	flag_type	apply_flags_full[];
extern	const	struct	flag_type	area_who_titles[];
extern	const	struct	flag_type	area_who_display[];
extern	const	struct	flag_type	sex_flags[];
extern	const	struct	flag_type	furniture_flags[];
extern	const	struct	flag_type	weapon_class[];
extern	const	struct	flag_type	ranged_weapon_class[];
extern	const	struct	flag_type	apply_types[];
extern	const	struct	flag_type	weapon_type2[];
extern	const	struct	flag_type	apply_types[];
extern	const	struct	flag_type	size_flags[];
extern	const	struct	flag_type	position_flags[];
extern	const	struct	flag_type	ac_type[];
extern	const	struct	bit_type	bitvector_type[];
extern  const   struct  exp_type        experience_type[];
extern	const	struct	flag_type	quest_parts[];
extern  const   struct  flag_type	channel_flags[];
extern  const   struct  flag_type       project_flags[];
extern  const   struct  flag_type       immortal_flags[];
extern  const	struct	flag_type	damage_classes[];
extern  const	struct	flag_type	relic_types[];
extern	const	struct	flag_type	script_flags[];
extern	const	struct	flag_type	interrupt_action_types[];
extern	const struct flag_type corpse_types[];
extern	const struct corpse_info corpse_info_table[];
extern	const struct flag_type time_of_day_flags[];
extern	const struct flag_type death_types[];
extern	const struct flag_type tool_types[];
extern	const int dam_to_corpse[DAM_MAX][11];
extern	const struct corpse_blend_type corpse_blending[];
extern  const struct flag_type catalyst_types[];
extern  const struct flag_type catalyst_method_types[];
extern  const struct flag_type boolean_types[];
extern	const struct flag_type tattoo_loc_flags[];
extern	const char *catalyst_descs[];

extern	const struct flag_type affgroup_mobile_flags[];
extern	const struct flag_type affgroup_object_flags[];
extern	const struct flag_type spell_target_types[];
extern	const struct flag_type song_target_types[];
extern  const struct flag_type moon_phases[];
extern	const struct flag_type instrument_types[];
extern	const struct flag_type instrument_flags[];

extern	const struct flag_type corpse_object_flags[];
extern	const struct flag_type variable_types[];
extern	const struct flag_type skill_flags[];

extern	const struct flag_type shop_flags[];

extern	const struct flag_type blueprint_section_flags[];
extern	const struct flag_type blueprint_section_types[];
extern	const struct flag_type instance_flags[];
extern	const struct flag_type dungeon_flags[];

extern	const struct flag_type transfer_modes[];

extern	const struct flag_type ship_class_types[];
extern	const struct flag_type ship_flags[];

#endif
