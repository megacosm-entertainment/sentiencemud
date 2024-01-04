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

/*
 * Spell functions.
 * Defined in magic.c.
 */
DECLARE_SPELL_FUN(	spell_acid_blast	);
DECLARE_SPELL_FUN(	spell_acid_breath	);
DECLARE_SPELL_FUN(	spell_afterburn		);
DECLARE_SPELL_FUN(	spell_animate_dead	);
DECLARE_SPELL_FUN(	spell_armour		);
DECLARE_SPELL_FUN(	spell_avatar_shield	);
DECLARE_SPELL_FUN(	spell_bless		);
DECLARE_SPELL_FUN(	spell_blindness		);
DECLARE_SPELL_FUN(	spell_burning_hands	);
DECLARE_SPELL_FUN(	spell_call_familiar	);
DECLARE_SPELL_FUN(	spell_call_lightning	);
DECLARE_SPELL_FUN(      spell_calm		);
DECLARE_SPELL_FUN(      spell_cancellation	);
DECLARE_SPELL_FUN(	spell_cause_critical	);
DECLARE_SPELL_FUN(	spell_cause_light	);
DECLARE_SPELL_FUN(	spell_cause_serious	);
DECLARE_SPELL_FUN(      spell_chain_lightning   );
DECLARE_SPELL_FUN(	spell_change_sex	);
DECLARE_SPELL_FUN(	spell_channel	 	);
DECLARE_SPELL_FUN(	spell_charm_person	);
DECLARE_SPELL_FUN(	spell_chill_touch	);
DECLARE_SPELL_FUN(	spell_cloak_of_guile	);
DECLARE_SPELL_FUN(	spell_colour_spray	);
DECLARE_SPELL_FUN(	spell_continual_light	);
DECLARE_SPELL_FUN(	spell_control_weather	);
DECLARE_SPELL_FUN(	spell_cosmic_blast	);
DECLARE_SPELL_FUN(	spell_counter_spell	);
DECLARE_SPELL_FUN(	spell_create_rose	);
DECLARE_SPELL_FUN(	spell_create_spring	);
DECLARE_SPELL_FUN(	spell_create_water	);
DECLARE_SPELL_FUN(	spell_crucify		);
DECLARE_SPELL_FUN(	spell_cure_blindness	);
DECLARE_SPELL_FUN(	spell_cure_critical	);
DECLARE_SPELL_FUN(      spell_cure_disease	);
DECLARE_SPELL_FUN(	spell_cure_light	);
DECLARE_SPELL_FUN(	spell_cure_poison	);
DECLARE_SPELL_FUN(	spell_cure_serious	);
DECLARE_SPELL_FUN(	spell_curse		);
DECLARE_SPELL_FUN(	spell_dark_sight	);
DECLARE_SPELL_FUN(	spell_deathbarbs	);
DECLARE_SPELL_FUN(	spell_death_grip	);
DECLARE_SPELL_FUN(      spell_demonfire		);
DECLARE_SPELL_FUN(      spell_destruction	);
DECLARE_SPELL_FUN(	spell_detect_evil	);
DECLARE_SPELL_FUN(	spell_detect_good	);
DECLARE_SPELL_FUN(	spell_detect_hidden	);
DECLARE_SPELL_FUN(	spell_detect_invis	);
DECLARE_SPELL_FUN(	spell_detect_magic	);
DECLARE_SPELL_FUN(	spell_dispel_room	);
DECLARE_SPELL_FUN(	spell_discharge		);
DECLARE_SPELL_FUN(	spell_dispel_evil	);
DECLARE_SPELL_FUN(      spell_dispel_good       );
DECLARE_SPELL_FUN(	spell_dispel_magic	);
DECLARE_SPELL_FUN(	spell_eagle_eye 	);
DECLARE_SPELL_FUN(	spell_earthquake	);
DECLARE_SPELL_FUN(	spell_electrical_barrier	);
DECLARE_SPELL_FUN(	spell_enchant_armour	);
DECLARE_SPELL_FUN(	spell_enchant_weapon	);
DECLARE_SPELL_FUN(	spell_energy_drain	);
DECLARE_SPELL_FUN(	spell_energy_field	);
DECLARE_SPELL_FUN(	spell_enervation	);
DECLARE_SPELL_FUN(	spell_ensnare		);
DECLARE_SPELL_FUN( 	spell_entrap		);
DECLARE_SPELL_FUN(	spell_exorcism		);
DECLARE_SPELL_FUN(	spell_faerie_fire	);
DECLARE_SPELL_FUN(	spell_faerie_fog	);
DECLARE_SPELL_FUN(	spell_false_dawn	);
DECLARE_SPELL_FUN(	spell_farsight		);
DECLARE_SPELL_FUN(	spell_fatigue		);
DECLARE_SPELL_FUN(	spell_fireball		);
DECLARE_SPELL_FUN(	spell_fire_breath	);
DECLARE_SPELL_FUN(	spell_fire_cloud	);
DECLARE_SPELL_FUN(	spell_fire_barrier	);
DECLARE_SPELL_FUN(	spell_fireproof		);
DECLARE_SPELL_FUN(	spell_flamestrike	);
DECLARE_SPELL_FUN(	spell_floating_disc	);
DECLARE_SPELL_FUN(	spell_fly		);
DECLARE_SPELL_FUN(      spell_frenzy		);
DECLARE_SPELL_FUN(	spell_frost_breath	);
DECLARE_SPELL_FUN(	spell_frost_barrier	);
DECLARE_SPELL_FUN(	spell_gas_breath	);
DECLARE_SPELL_FUN(	spell_gate		);
DECLARE_SPELL_FUN(	spell_general_purpose	);
DECLARE_SPELL_FUN(	spell_giant_strength	);
DECLARE_SPELL_FUN(	spell_glorious_bolt	);
DECLARE_SPELL_FUN(	spell_hallucination	);
DECLARE_SPELL_FUN(	spell_harm		);
DECLARE_SPELL_FUN(      spell_haste		);
DECLARE_SPELL_FUN(	spell_heal		);
DECLARE_SPELL_FUN(      spell_healing_aura	);
DECLARE_SPELL_FUN(	spell_heat_metal	);
DECLARE_SPELL_FUN(	spell_high_explosive	);
DECLARE_SPELL_FUN(      spell_holy_shield	);
DECLARE_SPELL_FUN(      spell_holy_sword	);
DECLARE_SPELL_FUN(      spell_holy_word		);
DECLARE_SPELL_FUN(	spell_ice_storm		);
DECLARE_SPELL_FUN(	spell_identify		);
DECLARE_SPELL_FUN(	spell_improved_invisibility	);
DECLARE_SPELL_FUN(	spell_inferno		);
DECLARE_SPELL_FUN(	spell_infravision	);
DECLARE_SPELL_FUN(	spell_invis		);
DECLARE_SPELL_FUN(	spell_kill	);
DECLARE_SPELL_FUN(	spell_know_alignment	);
DECLARE_SPELL_FUN(	spell_lightning_bolt	);
DECLARE_SPELL_FUN(	spell_lightning_breath	);
DECLARE_SPELL_FUN(	spell_light_shroud	);
DECLARE_SPELL_FUN(	spell_locate_object	);
DECLARE_SPELL_FUN(	spell_magic_missile	);
DECLARE_SPELL_FUN(      spell_mass_healing	);
DECLARE_SPELL_FUN(	spell_mass_invis	);
DECLARE_SPELL_FUN(	spell_master_weather	);
DECLARE_SPELL_FUN(	spell_maze		);
DECLARE_SPELL_FUN(	spell_momentary_darkness	);
DECLARE_SPELL_FUN(	spell_nexus		);
DECLARE_SPELL_FUN(	spell_paralysis		);
DECLARE_SPELL_FUN(	spell_pass_door		);
DECLARE_SPELL_FUN(      spell_plague		);
DECLARE_SPELL_FUN(	spell_poison		);
DECLARE_SPELL_FUN(	spell_portal		);
DECLARE_SPELL_FUN(	spell_possess		);
DECLARE_SPELL_FUN(	spell_protection_evil	);
DECLARE_SPELL_FUN(	spell_protection_good	);
DECLARE_SPELL_FUN(	spell_raise_dead	);
DECLARE_SPELL_FUN(	spell_ray_of_truth	);
DECLARE_SPELL_FUN(	spell_recharge		);
DECLARE_SPELL_FUN(	spell_reflection	);
DECLARE_SPELL_FUN(	spell_refresh		);
DECLARE_SPELL_FUN(	spell_regeneration	);
DECLARE_SPELL_FUN(	spell_remove_curse	);
DECLARE_SPELL_FUN(	spell_reverie		);
DECLARE_SPELL_FUN(	spell_room_shield	);
DECLARE_SPELL_FUN(	spell_sanctuary		);
DECLARE_SPELL_FUN(	spell_shield		);
DECLARE_SPELL_FUN(	spell_shocking_grasp	);
DECLARE_SPELL_FUN(	spell_silence		);
DECLARE_SPELL_FUN(	spell_sleep		);
DECLARE_SPELL_FUN(	spell_slow		);
DECLARE_SPELL_FUN(	spell_spell_deflection	);
DECLARE_SPELL_FUN(	spell_spell_shield	);
DECLARE_SPELL_FUN(	spell_spell_trap	);
DECLARE_SPELL_FUN(	spell_starflare		);
DECLARE_SPELL_FUN(	spell_stinking_cloud	);
DECLARE_SPELL_FUN(	spell_stone_skin	);
DECLARE_SPELL_FUN(	spell_stone_spikes	);
DECLARE_SPELL_FUN(	spell_summon		);
DECLARE_SPELL_FUN(	spell_teleport		);
DECLARE_SPELL_FUN(	spell_third_eye		);
DECLARE_SPELL_FUN(	spell_toxin_neurotoxin  );
DECLARE_SPELL_FUN(	spell_toxin_paralysis   );
DECLARE_SPELL_FUN(	spell_toxin_venom	);
DECLARE_SPELL_FUN(	spell_toxin_weakness    );
DECLARE_SPELL_FUN(	spell_underwater_breathing	);
DECLARE_SPELL_FUN(	spell_ventriloquate	);
DECLARE_SPELL_FUN(	spell_vision		);
DECLARE_SPELL_FUN(	spell_vocalize		);
DECLARE_SPELL_FUN(	spell_weaken		);
DECLARE_SPELL_FUN(	spell_web		);
DECLARE_SPELL_FUN(	spell_withering_cloud	);
DECLARE_SPELL_FUN(	spell_wind_of_confusion	);
DECLARE_SPELL_FUN(	spell_word_of_recall	);
DECLARE_SPELL_FUN(	spell_toxic_fumes	);	/* NIB : 20070126 */
DECLARE_SPELL_FUN(	spell_cure_toxic	);	/* NIB : 20070127 */
DECLARE_SPELL_FUN(	spell_morphlock		);	/* NIB : 200706?? */
DECLARE_SPELL_FUN(	spell_deathsight	);	/* NIB : 20070711 */

DECLARE_SPELL_FUN(	spell_ice_shards	);
DECLARE_SPELL_FUN(	spell_glacial_wave	);
DECLARE_SPELL_FUN(	spell_dark_shroud	);
DECLARE_SPELL_FUN(	spell_stone_touch	);
DECLARE_SPELL_FUN(	spell_earth_walk	);
DECLARE_SPELL_FUN(	spell_flash		);
DECLARE_SPELL_FUN(	spell_shriek		);
DECLARE_SPELL_FUN(	spell_soul_essence	);





// PRESPELL functions
DECLARE_PRESPELL_FUN ( prespell_silence );



// Artificing Functions:

// PREBREW
DECLARE_PREBREW_FUN ( prebrew_silence );

// BREW
DECLARE_BREW_FUN ( brew_silence );

// QUAFF
DECLARE_QUAFF_FUN ( quaff_silence );

// PRESCRIBE
DECLARE_PRESCRIBE_FUN ( prescribe_silence );

// SCRIBE
DECLARE_SCRIBE_FUN ( scribe_silence );

// RECITE
DECLARE_RECITE_FUN ( recite_identify );
DECLARE_RECITE_FUN ( recite_silence );
DECLARE_RECITE_FUN ( recite_word_of_recall );

// PREINK
DECLARE_PREINK_FUN ( preink_silence );

// INK

// TOUCH
DECLARE_TOUCH_FUN ( touch_armour );
DECLARE_TOUCH_FUN ( touch_chain_lightning );
DECLARE_TOUCH_FUN ( touch_fly );
DECLARE_TOUCH_FUN ( touch_haste );
DECLARE_TOUCH_FUN ( touch_silence );

// PREIMBUE
DECLARE_PREIMBUE_FUN( preimbue_silence );

// IMBUE

// ZAP
DECLARE_ZAP_FUN ( zap_chain_lightning );
DECLARE_ZAP_FUN ( zap_silence );

// BRANDISH
DECLARE_BRANDISH_FUN( brandish_armour );
//DECLARE_BRANDISH_FUN( brandish_fly );
//DECLARE_BRANDISH_FUN( brandish_haste );
DECLARE_BRANDISH_FUN( brandish_silence );
//DECLARE_BRANDISH_FUN( brandish_word_of_recall );

// EQUIP
DECLARE_EQUIP_FUN( equip_armour );
DECLARE_EQUIP_FUN( equip_haste );
DECLARE_EQUIP_FUN( equip_invis );
DECLARE_EQUIP_FUN( equip_silence );