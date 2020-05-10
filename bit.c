/***************************************************************************
 *  File: bit.c                                                            *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was written by Jason Dinkel and inspired by Russ Taylor,     *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/
/*
 The code below uses a table lookup system that is based on suggestions
 from Russ Taylor.  There are many routines in handler.c that would benefit
 with the use of tables.  You may consider simplifying your code base by
 implementing a system like below with such functions. -Jason Dinkel
 */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "tables.h"


/*****************************************************************************
 Name:		flag_stat_table
 Purpose:	This table catagorizes the tables following the lookup
 		functions below into stats and flags.  Flags can be toggled
 		but stats can only be assigned.  Update this table when a
 		new set of flags is installed.
 ****************************************************************************/
const struct flag_stat_type flag_stat_table[] =
{
/*  {	structure					stat	}, */
	{	token_flags,				FALSE	},
	{	area_flags,					FALSE	},
	{	sex_flags,					TRUE	},
	{	exit_flags,					FALSE	},
	{	door_resets,				TRUE	},
	{	room_flags,					FALSE	},
	{	sector_flags,				TRUE	},
	{	type_flags,					TRUE	},
	{	extra_flags,				FALSE	},
	{	wear_flags,					FALSE	},
	{	act_flags,					FALSE	},
	{	act2_flags,					FALSE	},
	{	affect_flags,				FALSE	},
	{	affect2_flags,				FALSE	},
	{	apply_flags,				TRUE	},
	{	wear_loc_flags,				TRUE	},
	{	wear_loc_strings,			TRUE	},
	{	wear_loc_names,				TRUE	},
	{	container_flags,			FALSE	},

/* ROM specific flags: */

    {	form_flags,					FALSE	},
    {	part_flags,					FALSE	},
    {	ac_type,					TRUE	},
    {	size_flags,					TRUE	},
    {	position_flags,				TRUE	},
    {	off_flags,					FALSE	},
    {	imm_flags,					FALSE	},
    {	res_flags,					FALSE	},
    {	vuln_flags,					FALSE	},
    {	weapon_class,				TRUE	},
    {	weapon_type2,				FALSE	},
    {	apply_types,				TRUE	},
    {	ranged_weapon_class,		TRUE	},
    {	script_flags,				FALSE	},
    {	catalyst_types,				TRUE	},
    {	affgroup_mobile_flags,		TRUE	},
    {	affgroup_object_flags,		TRUE	},
    {	catalyst_types,				TRUE	},
    {	boolean_types,				TRUE	},
    {	moon_phases,				TRUE	},
    {	spell_target_types,			TRUE	},
    {	area_who_titles,			TRUE	},
    {	area_who_display,			TRUE	},
    {	instrument_types,			TRUE	},
    {	place_flags,				TRUE	},
    {	corpse_types,				TRUE	},
    {	variable_types,				TRUE	},
	{	blueprint_section_types,	TRUE	},
	{	transfer_modes,				TRUE	},
	{	ship_class_types,			TRUE	},
    {	0,							0		}
};


/*****************************************************************************
 Name:		is_stat( table )
 Purpose:	Returns TRUE if the table is a stat table and FALSE if flag.
 Called by:	flag_value and flag_string.
 Note:		This function is local and used only in bit.c.
 ****************************************************************************/
bool is_stat( const struct flag_type *flag_table )
{
    int flag;

    for (flag = 0; flag_stat_table[flag].structure; flag++)
    {
	if ( flag_stat_table[flag].structure == flag_table
	  && flag_stat_table[flag].stat )
	    return TRUE;
    }
    return FALSE;
}


/*****************************************************************************
 Name:		flag_value( table, flag )
 Purpose:	Returns the value of the flags entered.  Multi-flags accepted.
 Called by:	olc.c and olc_act.c.
 ****************************************************************************/
long flag_value( const struct flag_type *flag_table, char *argument)
{
    char word[MAX_INPUT_LENGTH];
    long bit;
    long marked = 0;
    bool found = FALSE;

    if ( flag_table == NULL ) return NO_FLAG;

    if ( is_stat( flag_table ) )
    {
	one_argument( argument, word );

	if ( ( bit = flag_lookup( word, flag_table ) ) != 0 )
	    return bit;
	else
	    return NO_FLAG;
    }

    /*
     * Accept multiple flags.
     */
    for (; ;)
    {
        argument = one_argument( argument, word );

        if ( word[0] == '\0' )
	    break;

        if ( ( bit = flag_lookup( word, flag_table ) ) != 0 )
        {
            SET_BIT( marked, bit );
            found = TRUE;
        }
    }

    if ( found )
	return marked;
    else
	return NO_FLAG;
}


/*****************************************************************************
 Name:		flag_string( table, flags/stat )
 Purpose:	Returns string with name(s) of the flags or stat entered.
 Called by:	act_olc.c, olc.c, and olc_save.c.
 ****************************************************************************/
char *flag_string( const struct flag_type *flag_table, long bits )
{
    static char buf[4][512];
    static int cnt = 0;
    int  flag;

    if ( ++cnt > 3 )
    	cnt = 0;

    buf[cnt][0] = '\0';

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if ( !is_stat( flag_table ) && IS_SET(bits, flag_table[flag].bit) )
	{
	    strcat( buf[cnt], " " );
	    strcat( buf[cnt], flag_table[flag].name );
	}
	else
	if ( flag_table[flag].bit == bits )
	{
	    strcat( buf[cnt], " " );
	    strcat( buf[cnt], flag_table[flag].name );
	    break;
	}
    }
    return (buf[cnt][0] != '\0') ? buf[cnt]+1 : "none";
}


/* Exactly identical to flag_string in every way except that it's delimited
   by commas instead of spaces. */
char *flag_string_commas( const struct flag_type *flag_table, long bits )
{
    static char buf[2][512];
    static int cnt = 0;
    int  flag;

    if ( ++cnt > 1 )
    	cnt = 0;

    buf[cnt][0] = '\0';

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if ( !is_stat( flag_table ) && IS_SET(bits, flag_table[flag].bit) )
	{
	    strcat( buf[cnt], ", " );
	    strcat( buf[cnt], flag_table[flag].name );
	}
	else
	if ( flag_table[flag].bit == bits )
	{
	    strcat( buf[cnt], ", " );
	    strcat( buf[cnt], flag_table[flag].name );
	    break;
	}
    }

    return (buf[cnt][0] != '\0') ? buf[cnt]+1 : "none";
}


/* the following functions return ASCII names for bit vectors */

/*
 * Return ascii name of an affect location.
 */
char *affect_loc_name( int location )
{
	static char buf[4][MIL];
	static int i = -1;

	i = (i+1)&3;

    switch ( location )
    {
	case APPLY_NONE:		return "none";
	case APPLY_STR:			return "strength";
	case APPLY_DEX:			return "dexterity";
	case APPLY_INT:			return "intelligence";
	case APPLY_WIS:			return "wisdom";
	case APPLY_CON:			return "constitution";
	case APPLY_SEX:			return "sex";
	case APPLY_MANA:		return "mana";
	case APPLY_HIT:			return "hp";
	case APPLY_MOVE:		return "moves";
	case APPLY_GOLD:		return "gold";
	case APPLY_AC:			return "armour class";
	case APPLY_HITROLL:		return "hit roll";
	case APPLY_DAMROLL:		return "damage roll";
	case APPLY_SPELL_AFFECT:	return "none";
	default:
		if(location >= APPLY_SKILL && location < APPLY_SKILL_MAX && skill_table[location - APPLY_SKILL].name) {
			sprintf(buf[i], "%s %%rating", skill_table[location - APPLY_SKILL].name);
			return buf[i];
		}
		break;
    }

    bug( "Affect_location_name: unknown location %d.", location );
    return "(unknown)";
}


/*
 * Return ascii name of an affect bit vector.
 */
char *affect_bit_name( long vector )
{
    static char buf[1024];

    buf[0] = '\0';
    if ( vector & AFF_BLIND         ) strcat( buf, " blind"         );
    if ( vector & AFF_INVISIBLE     ) strcat( buf, " invisible"     );
    if ( vector & AFF_DETECT_EVIL   ) strcat( buf, " detect_evil"   );
    if ( vector & AFF_DETECT_INVIS  ) strcat( buf, " detect_invis"  );
    if ( vector & AFF_DETECT_MAGIC  ) strcat( buf, " detect_magic"  );
    if ( vector & AFF_DETECT_HIDDEN ) strcat( buf, " detect_hidden" );
    if ( vector & AFF_DETECT_GOOD   ) strcat( buf, " detect_good"   );
    if ( vector & AFF_SANCTUARY     ) strcat( buf, " sanctuary"     );
    if ( vector & AFF_FAERIE_FIRE   ) strcat( buf, " faerie_fire"   );
    if ( vector & AFF_INFRARED      ) strcat( buf, " infrared"      );
    if ( vector & AFF_CURSE         ) strcat( buf, " curse"         );
    if ( vector & AFF_DEATH_GRIP    ) strcat( buf, " death_grip"    );
    if ( vector & AFF_POISON        ) strcat( buf, " poison"        );
    if ( vector & AFF_SNEAK         ) strcat( buf, " sneak"         );
    if ( vector & AFF_HIDE          ) strcat( buf, " hide"          );
    if ( vector & AFF_SLEEP         ) strcat( buf, " sleep"         );
    if ( vector & AFF_CHARM         ) strcat( buf, " charm"         );
    if ( vector & AFF_FLYING        ) strcat( buf, " flying"        );
    if ( vector & AFF_PASS_DOOR     ) strcat( buf, " pass_door"     );
    if ( vector & AFF_HASTE	    ) strcat( buf, " haste"	    );
    if ( vector & AFF_CALM	    ) strcat( buf, " calm"	    );
    if ( vector & AFF_PLAGUE	    ) strcat( buf, " plague" 	    );
    if ( vector & AFF_WEAKEN	    ) strcat( buf, " weaken" 	    );
    if ( vector & AFF_FRENZY        ) strcat( buf, " frenzy"        );
    if ( vector & AFF_BERSERK	    ) strcat( buf, " berserk"	    );
    if ( vector & AFF_SWIM	    ) strcat( buf, " swim" 	    );
    if ( vector & AFF_REGENERATION  ) strcat( buf, " regeneration"  );
    if ( vector & AFF_SLOW          ) strcat( buf, " slow"          );
    if ( vector & AFF_WEB	    ) strcat( buf, " web");
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *affect2_bit_name( long vector )
{
    static char buf[1024];

    buf[0] = '\0';
    if ( vector & AFF2_SILENCE 	)		strcat( buf, " silence"			);
    if ( vector & AFF2_EVASION 	) 		strcat( buf, " evasion"			);
    if ( vector & AFF2_CLOAK_OF_GUILE) 		strcat( buf, " cloak_of_guile"		);
    if ( vector & AFF2_WARCRY ) 		strcat( buf, " warcry"			);
    if ( vector & AFF2_LIGHT_SHROUD ) 		strcat( buf, " light_shroud"		);
    if ( vector & AFF2_HEALING_AURA ) 		strcat( buf, " healing_aura"		);
    if ( vector & AFF2_ENERGY_FIELD )		strcat( buf, " energy_field"		);
    if ( vector & AFF2_SPELL_SHIELD ) 		strcat( buf, " spell_shield"		);
    if ( vector & AFF2_SPELL_DEFLECTION ) 	strcat( buf, " spell_deflection"	);
    if ( vector & AFF2_AVATAR_SHIELD ) 		strcat( buf, " avatar_shield"		);
    if ( vector & AFF2_FATIGUE ) 		strcat( buf, " fatigue"			);
    if ( vector & AFF2_PARALYSIS ) 		strcat( buf, " paralysis"		);
    if ( vector & AFF2_NEUROTOXIN ) 		strcat( buf, " neurotoxin"		);
    if ( vector & AFF2_TOXIN ) 			strcat( buf, " toxin"			);
    if ( vector & AFF2_ELECTRICAL_BARRIER ) 	strcat( buf, " electrical_barrier"	);
    if ( vector & AFF2_FIRE_BARRIER ) 		strcat( buf, " fire_barrier"		);
    if ( vector & AFF2_FROST_BARRIER ) 		strcat( buf, " frost_barrier"		);
    if ( vector & AFF2_IMPROVED_INVIS ) 	strcat( buf, " improved_invis"		);
    if ( vector & AFF2_ENSNARE ) 		strcat( buf, " ensnare"			);
    if ( vector & AFF2_SEE_CLOAK ) 		strcat( buf, " see_cloak"		);
    if ( vector & AFF2_STONE_SKIN) 		strcat( buf, " stone_skin"		);
    if ( vector & AFF2_MORPHLOCK) 		strcat( buf, " morphlock"		);
    if ( vector & AFF2_DEATHSIGHT) 		strcat( buf, " deathsight"		);
    if ( vector & AFF2_IMMOBILE) 		strcat( buf, " immobile"		);
    if ( vector & AFF2_PROTECTED) 		strcat( buf, " protected"		);
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}

char *affects_bit_name( long vector, long vector2 )
{
    static char buf[2048];

    buf[0] = '\0';
    if ( vector2 & AFF2_AVATAR_SHIELD )			strcat( buf, " avatar_shield" );
    if ( vector & AFF_BERSERK )					strcat( buf, " berserk" );
    if ( vector & AFF_BLIND )					strcat( buf, " blind" );
    if ( vector & AFF_CALM )					strcat( buf, " calm" );
    if ( vector & AFF_CHARM )					strcat( buf, " charm" );
    if ( vector2 & AFF2_CLOAK_OF_GUILE ) 		strcat( buf, " cloak_of_guile" );
    if ( vector & AFF_CURSE )					strcat( buf, " curse" );
    if ( vector & AFF_DEATH_GRIP )				strcat( buf, " death_grip" );
    if ( vector2 & AFF2_DEATHSIGHT )			strcat( buf, " deathsight" );
    if ( vector & AFF_DETECT_EVIL )				strcat( buf, " detect_evil" );
    if ( vector & AFF_DETECT_GOOD )				strcat( buf, " detect_good" );
    if ( vector & AFF_DETECT_HIDDEN )			strcat( buf, " detect_hidden" );
    if ( vector & AFF_DETECT_INVIS )			strcat( buf, " detect_invis" );
    if ( vector & AFF_DETECT_MAGIC )			strcat( buf, " detect_magic" );
    if ( vector2 & AFF2_ELECTRICAL_BARRIER )	strcat( buf, " electrical_barrier" );
    if ( vector2 & AFF2_ENERGY_FIELD )			strcat( buf, " energy_field" );
    if ( vector2 & AFF2_ENSNARE )				strcat( buf, " ensnare" );
    if ( vector2 & AFF2_EVASION )				strcat( buf, " evasion" );
    if ( vector & AFF_FAERIE_FIRE )				strcat( buf, " faerie_fire" );
    if ( vector2 & AFF2_FATIGUE )				strcat( buf, " fatigue" );
    if ( vector2 & AFF2_FIRE_BARRIER )			strcat( buf, " fire_barrier" );
    if ( vector & AFF_FLYING )					strcat( buf, " flying" );
    if ( vector & AFF_FRENZY )					strcat( buf, " frenzy" );
    if ( vector2 & AFF2_FROST_BARRIER )			strcat( buf, " frost_barrier" );
    if ( vector & AFF_HASTE )					strcat( buf, " haste" );
    if ( vector2 & AFF2_HEALING_AURA )			strcat( buf, " healing_aura" );
    if ( vector & AFF_HIDE )					strcat( buf, " hide" );
    if ( vector2 & AFF2_IMMOBILE )				strcat( buf, " immobile" );
    if ( vector2 & AFF2_IMPROVED_INVIS )		strcat( buf, " improved_invis" );
    if ( vector & AFF_INFRARED )				strcat( buf, " infrared" );
    if ( vector & AFF_INVISIBLE )				strcat( buf, " invisible" );
    if ( vector2 & AFF2_LIGHT_SHROUD )			strcat( buf, " light_shroud" );
    if ( vector2 & AFF2_MORPHLOCK )				strcat( buf, " morphlock" );
    if ( vector2 & AFF2_NEUROTOXIN )			strcat( buf, " neurotoxin" );
    if ( vector2 & AFF2_PARALYSIS )				strcat( buf, " paralysis" );
    if ( vector & AFF_PASS_DOOR )				strcat( buf, " pass_door" );
    if ( vector & AFF_PLAGUE )					strcat( buf, " plague" );
    if ( vector & AFF_POISON )					strcat( buf, " poison" );
    if ( vector2 & AFF2_PROTECTED )				strcat( buf, " protected" );
    if ( vector & AFF_REGENERATION )			strcat( buf, " regeneration" );
    if ( vector & AFF_SANCTUARY )				strcat( buf, " sanctuary" );
    if ( vector2 & AFF2_SEE_CLOAK )				strcat( buf, " see_cloak" );
    if ( vector2 & AFF2_SILENCE )				strcat( buf, " silence" );
    if ( vector & AFF_SLEEP )					strcat( buf, " sleep" );
    if ( vector & AFF_SLOW )					strcat( buf, " slow" );
    if ( vector & AFF_SNEAK )					strcat( buf, " sneak" );
    if ( vector2 & AFF2_SPELL_DEFLECTION )		strcat( buf, " spell_deflection" );
    if ( vector2 & AFF2_SPELL_SHIELD )			strcat( buf, " spell_shield" );
    if ( vector2 & AFF2_STONE_SKIN )			strcat( buf, " stone_skin" );
    if ( vector & AFF_SWIM )					strcat( buf, " swim" );
    if ( vector2 & AFF2_TOXIN )					strcat( buf, " toxin" );
    if ( vector2 & AFF2_WARCRY )				strcat( buf, " warcry" );
    if ( vector & AFF_WEAKEN )					strcat( buf, " weaken" );
    if ( vector & AFF_WEB )						strcat( buf, " web" );


    return ( buf[0] != '\0' ) ? buf+1 : "none";
}

/*
 * Return ascii name of extra flags vector.
 */
char *extra_bit_name( long extra_flags )
{
    static char buf[512];

    buf[0] = '\0';
    if ( extra_flags & ITEM_GLOW         ) strcat( buf, " glow"         );
    if ( extra_flags & ITEM_HUM          ) strcat( buf, " hum"          );
  /*if ( extra_flags & ITEM_DARK         ) strcat( buf, " dark"         );*/
    if ( extra_flags & ITEM_NOKEYRING    ) strcat( buf, " nokeyring"    );
    if ( extra_flags & ITEM_EVIL         ) strcat( buf, " evil"         );
    if ( extra_flags & ITEM_INVIS        ) strcat( buf, " invis"        );
    if ( extra_flags & ITEM_MAGIC        ) strcat( buf, " magic"        );
    if ( extra_flags & ITEM_NODROP       ) strcat( buf, " nodrop"       );
    if ( extra_flags & ITEM_BLESS        ) strcat( buf, " bless"        );
    if ( extra_flags & ITEM_ANTI_GOOD    ) strcat( buf, " anti-good"    );
    if ( extra_flags & ITEM_ANTI_EVIL    ) strcat( buf, " anti-evil"    );
    if ( extra_flags & ITEM_ANTI_NEUTRAL ) strcat( buf, " anti-neutral" );
    if ( extra_flags & ITEM_NOREMOVE     ) strcat( buf, " noremove"     );
    if ( extra_flags & ITEM_INVENTORY    ) strcat( buf, " inventory"    );
    if ( extra_flags & ITEM_NOPURGE	 ) strcat( buf, " nopurge"	);
    /*if ( extra_flags & ITEM_VIS_DEATH	 ) strcat( buf, " vis_death"	);*/
    if ( extra_flags & ITEM_ROT_DEATH	 ) strcat( buf, " rot_death"	);
    if ( extra_flags & ITEM_NOLOCATE	 ) strcat( buf, " no_locate"	);
    /*if ( extra_flags & ITEM_SELL_EXTRACT ) strcat( buf, " sell_extract" );*/
    if ( extra_flags & ITEM_HIDDEN 	 ) strcat( buf, " hidden" 	);
    if ( extra_flags & ITEM_BURN_PROOF	 ) strcat( buf, " burn_proof"	);
    if ( extra_flags & ITEM_FREEZE_PROOF ) strcat( buf, " freeze_proof"	);
    if ( extra_flags & ITEM_NOUNCURSE	 ) strcat( buf, " no_uncurse"	);
    if ( extra_flags & ITEM_PERMANENT	 ) strcat( buf, " permanent"	);
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *extra2_bit_name( long extra2_flags )
{
    static char buf[512];

    buf[0] = '\0';
    if ( extra2_flags & ITEM_ALL_REMORT   	) strcat( buf, " all_remort"      	);
    if ( extra2_flags & ITEM_LOCKER    		) strcat( buf, " locker"          	);
    if ( extra2_flags & ITEM_TRUESIGHT 		) strcat( buf, " truesight"     	);
    if ( extra2_flags & ITEM_SCARE     		) strcat( buf, " scare"         	);
    if ( extra2_flags & ITEM_SUSTAIN   		) strcat( buf, " life-sustaining"	);
    if ( extra2_flags & ITEM_ENCHANTED 		) strcat( buf, " enchanted" 		);
    if ( extra2_flags & ITEM_EMITS_LIGHT 	) strcat( buf, " emits_light" 		);
    if ( extra2_flags & ITEM_FLOAT_USER 	) strcat( buf, " float_user" 		);
    if ( extra2_flags & ITEM_SEE_HIDDEN 	) strcat( buf, " see_hidden" 		);
    if ( extra2_flags & ITEM_WEED 			) strcat( buf, " weed"			);
    if ( extra2_flags & ITEM_SUPER_STRONG 	) strcat( buf, " super_strong" 		);
    if ( extra2_flags & ITEM_REMORT_ONLY 	) strcat( buf, " remort_only" 		);
    if ( extra2_flags & ITEM_NO_HUNT 		) strcat( buf, " no_hunt"		);
    if ( extra2_flags & ITEM_NO_RESURRECT 	) strcat( buf, " no_resurrect"		);
    if ( extra2_flags & ITEM_NO_DISCHARGE 	) strcat( buf, " no_discharge"		);
    if ( extra2_flags & ITEM_NO_DONATE 		) strcat( buf, " no_donate"		);
    if ( extra2_flags & ITEM_KEPT 			) strcat( buf, " kept"			);
    if ( extra2_flags & ITEM_SINGULAR 		) strcat( buf, " singular"		);
    if ( extra2_flags & ITEM_NO_ENCHANT 	) strcat( buf, " no_enchant"		);
    if ( extra2_flags & ITEM_NO_LOOT 		) strcat( buf, " no_loot"		);
    if ( extra2_flags & ITEM_NO_CONTAINER	) strcat( buf, " no_container"		);
    if ( extra2_flags & ITEM_THIRD_EYE		) strcat( buf, " third_eye"		);
    if ( extra2_flags & ITEM_UNSEEN			) strcat( buf, " unseen"		);
    if ( extra2_flags & ITEM_BURIED			) strcat( buf, " buried"		);
    if ( extra2_flags & ITEM_NOLOCKER		) strcat( buf, " no_locker"		);
    if ( extra2_flags & ITEM_NOAUCTION		) strcat( buf, " no_auction"		);
    if ( extra2_flags & ITEM_KEEP_VALUE		) strcat( buf, " keep_value"		);
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *extra3_bit_name( long extra3_flags )
{
    static char buf[512];

    buf[0] = '\0';
    if ( extra3_flags & ITEM_EXCLUDE_LIST	) strcat( buf, " exclude_list"		);
    if ( extra3_flags & ITEM_NO_TRANSFER	) strcat( buf, " no_transfer"		);
    if ( extra3_flags & ITEM_ALWAYS_LOOT	) strcat( buf, " always_loot"		);
    if ( extra3_flags & ITEM_FORCE_LOOT		) strcat( buf, " force_loot"		);
    if ( extra3_flags & ITEM_CAN_DISPEL		) strcat( buf, " can_dispel"		);
    if ( extra3_flags & ITEM_KEEP_EQUIPPED	) strcat( buf, " keep_equipped"		);
    if ( extra3_flags & ITEM_NO_ANIMATE		) strcat( buf, " no_animate"		);
    if ( extra3_flags & ITEM_RIFT_UPDATE	) strcat( buf, " rift_update"		);

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}

char *extra4_bit_name( long extra4_flags )
{
    static char buf[512];

    buf[0] = '\0';
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}



/* return ascii name of an act vector */
char *act_bit_name( int act_type, long act_flags )
{
    static char buf[512];

    buf[0] = '\0';

    switch(act_type) {
	case 1:		// NPC->act
		strcat(buf," npc");
		if (act_flags & ACT_SENTINEL 	) strcat(buf, " sentinel");
		if (act_flags & ACT_SCAVENGER	) strcat(buf, " scavenger");
		if (act_flags & ACT_AGGRESSIVE	) strcat(buf, " aggressive");
		if (act_flags & ACT_STAY_AREA	) strcat(buf, " stay_area");
		if (act_flags & ACT_PROTECTED	) strcat(buf, " protected");
		if (act_flags & ACT_WIMPY	) strcat(buf, " wimpy");
		if (act_flags & ACT_PET		) strcat(buf, " pet");
		if (act_flags & ACT_MOUNT	) strcat(buf, " mount");
		if (act_flags & ACT_TRAIN	) strcat(buf, " train");
		if (act_flags & ACT_PRACTICE	) strcat(buf, " practice");
		if (act_flags & ACT_UNDEAD	) strcat(buf, " undead");
		if (act_flags & ACT_CLERIC	) strcat(buf, " cleric");
		if (act_flags & ACT_MAGE	) strcat(buf, " mage");
		if (act_flags & ACT_THIEF	) strcat(buf, " thief");
		if (act_flags & ACT_WARRIOR	) strcat(buf, " warrior");
		if (act_flags & ACT_NOPURGE	) strcat(buf, " no_purge");
		if (act_flags & ACT_IS_HEALER	) strcat(buf, " healer");
		if (act_flags & ACT_IS_BANKER   ) strcat(buf, " banker");
		if (act_flags & ACT_IS_RESTRINGER) strcat(buf, " restringer");
		if (act_flags & ACT_IS_CHANGER  ) strcat(buf, " changer");
		if (act_flags & ACT_UPDATE_ALWAYS) strcat(buf," update_always");
		if (act_flags & ACT_QUESTOR) strcat(buf," questor");
		if (act_flags & ACT_STAY_LOCALE) strcat(buf, " stay_locale");
		break;
	case 2:		// NPC->act2
		if (act_flags & ACT2_CHURCHMASTER) strcat(buf, " churchmaster");
	    if (act_flags & ACT2_NOQUEST ) strcat( buf, " noquest" );
	    if (act_flags & ACT2_PLANE_TUNNELER ) strcat( buf, " plane_tunneler" );
 	    if (act_flags & ACT2_NO_HUNT ) strcat( buf, " no_hunt" );
 	    if (act_flags & ACT2_WIZI_MOB ) strcat( buf, " wizi_mob" );
 	    if (act_flags & ACT2_AIRSHIP_SELLER ) strcat( buf, " airship_seller" );
	    if (act_flags & ACT2_LOREMASTER ) strcat( buf, " loremaster" );
	    if (act_flags & ACT2_NO_RESURRECT ) strcat( buf, " no_resurrect");
	    if (act_flags & ACT2_DROP_EQ ) strcat( buf, " drop_eq");
	    if (act_flags & ACT2_GQ_MASTER ) strcat( buf, " gq_master");
	    if (act_flags & ACT2_SHIP_QUESTMASTER ) strcat( buf, " ship_quest_master" );
	    if (act_flags & ACT2_SEE_ALL ) strcat(buf, " see_all");
	    if (act_flags & ACT2_NO_CHASE ) strcat(buf, " no_chase");
	    if (act_flags & ACT2_TAKES_SKULLS ) strcat( buf, " takes_skulls");
	    if (act_flags & ACT2_PIRATE ) strcat( buf, " pirate");
	    if (act_flags & ACT2_SEE_WIZI) strcat( buf, " see_wizi");
	    if (act_flags & ACT2_SOUL_DEPOSIT) strcat( buf, " soul_deposit");
		if (act_flags & ACT2_HIRED) strcat(buf, " hired");
		break;
	case 3:		// PC->act
		strcat(buf," player");
		if (act_flags & PLR_PK		) strcat(buf, " pk");
		if (act_flags & PLR_AUTOEXIT	) strcat(buf, " autoexit");
		if (act_flags & PLR_AUTOLOOT	) strcat(buf, " autoloot");
		if (act_flags & PLR_AUTOSAC		) strcat(buf, " autosac");
		if (act_flags & PLR_AUTOGOLD	) strcat(buf, " autogold");
		if (act_flags & PLR_AUTOSPLIT	) strcat(buf, " autosplit");
		if (act_flags & PLR_HOLYLIGHT	) strcat(buf, " holy_light");
		if (act_flags & PLR_SHOWDAMAGE	) strcat(buf, " show_damage");
		if (act_flags & PLR_AUTOEQ    	) strcat(buf, " autoeq");
		if (act_flags & PLR_NOSUMMON	) strcat(buf, " no_summon");
		if (act_flags & PLR_NOFOLLOW	) strcat(buf, " no_follow");
		if (act_flags & PLR_FREEZE		) strcat(buf, " frozen");
		if (act_flags & PLR_COLOUR		) strcat(buf, " colour");
		if (act_flags & PLR_PURSUIT		) strcat(buf, " pursuit");
		if (act_flags & PLR_BUILDING	) strcat(buf, " building");
		break;
	case 4:		// PC->act2
		if (act_flags & PLR_AUTOSURVEY	) strcat(buf, " autosurvey");
		if (act_flags & PLR_SACRIFICE_ALL	) strcat(buf, " sacrifice_all");
		if (act_flags & PLR_NO_WAKE		) strcat(buf, " no_wake");
		if (act_flags & PLR_HOLYAURA	) strcat(buf, " holy_aura");
		if (act_flags & PLR_MOBILE		) strcat(buf, " mobile");
		if (act_flags & PLR_FAVSKILLS		) strcat(buf, " favskills");
		if (act_flags & PLR_HOLYWARP	) strcat(buf, " holy_warp");
		if (act_flags & PLR_NORECKONING	) strcat(buf, " no_reckoning");
		if (act_flags & PLR_NOLORE		) strcat(buf, " no_lore");
		break;
	}
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *comm_bit_name(int comm_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (comm_flags & COMM_QUIET		) strcat(buf, " quiet");
    if (comm_flags & COMM_NOWIZ		) strcat(buf, " no_wiz");
    if (comm_flags & COMM_NOAUCTION	) strcat(buf, " no_auction");
    if (comm_flags & COMM_NOMUSIC	) strcat(buf, " no_music");
    if (comm_flags & COMM_COMPACT	) strcat(buf, " compact");
    if (comm_flags & COMM_BRIEF		) strcat(buf, " brief");
    if (comm_flags & COMM_PROMPT	) strcat(buf, " prompt");
    if (comm_flags & COMM_NOTELL	) strcat(buf, " no_tell");
    if (comm_flags & COMM_NOCHANNELS	) strcat(buf, " no_channels");
    if (comm_flags & COMM_NOBATTLESPAM	) strcat(buf, " no_battlespam");
    if (comm_flags & COMM_NOMAP		) strcat(buf, " no_map");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *imm_bit_name(int imm_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (imm_flags & IMM_SUMMON		) strcat(buf, " summon");
    if (imm_flags & IMM_CHARM		) strcat(buf, " charm");
    if (imm_flags & IMM_MAGIC		) strcat(buf, " magic");
    if (imm_flags & IMM_WEAPON		) strcat(buf, " weapon");
    if (imm_flags & IMM_BASH		) strcat(buf, " blunt");
    if (imm_flags & IMM_PIERCE		) strcat(buf, " piercing");
    if (imm_flags & IMM_SLASH		) strcat(buf, " slashing");
    if (imm_flags & IMM_FIRE		) strcat(buf, " fire");
    if (imm_flags & IMM_COLD		) strcat(buf, " cold");
    if (imm_flags & IMM_LIGHTNING	) strcat(buf, " lightning");
    if (imm_flags & IMM_ACID		) strcat(buf, " acid");
    if (imm_flags & IMM_POISON		) strcat(buf, " poison");
    if (imm_flags & IMM_NEGATIVE	) strcat(buf, " negative");
    if (imm_flags & IMM_HOLY		) strcat(buf, " holy");
    if (imm_flags & IMM_ENERGY		) strcat(buf, " energy");
    if (imm_flags & IMM_MENTAL		) strcat(buf, " mental");
    if (imm_flags & IMM_DISEASE		) strcat(buf, " disease");
    if (imm_flags & IMM_DROWNING	) strcat(buf, " drowning");
    if (imm_flags & IMM_LIGHT		) strcat(buf, " light");
    if (imm_flags & IMM_IRON		) strcat(buf, " iron");
    if (imm_flags & IMM_WOOD		) strcat(buf, " wood");
    if (imm_flags & IMM_SILVER		) strcat(buf, " silver");
    if (imm_flags & IMM_KILL 		) strcat( buf, " kill" );
    if (imm_flags & IMM_WATER		) strcat( buf, " water" );
    /* @@@NIB : 20070127*/
    if (imm_flags & IMM_AIR		) strcat(buf, " air" );
    if (imm_flags & IMM_EARTH		) strcat(buf, " earth" );
    if (imm_flags & IMM_PLANT		) strcat(buf, " plant" );

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *res_bit_name(int res_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (res_flags & RES_SUMMON		) strcat(buf, " summon");
    if (res_flags & RES_CHARM		) strcat(buf, " charm");
    if (res_flags & RES_MAGIC		) strcat(buf, " magic");
    if (res_flags & RES_WEAPON		) strcat(buf, " weapon");
    if (res_flags & RES_BASH		) strcat(buf, " blunt");
    if (res_flags & RES_PIERCE		) strcat(buf, " piercing");
    if (res_flags & RES_SLASH		) strcat(buf, " slashing");
    if (res_flags & RES_FIRE		) strcat(buf, " fire");
    if (res_flags & RES_COLD		) strcat(buf, " cold");
    if (res_flags & RES_LIGHTNING	) strcat(buf, " lightning");
    if (res_flags & RES_ACID		) strcat(buf, " acid");
    if (res_flags & RES_POISON		) strcat(buf, " poison");
    if (res_flags & RES_NEGATIVE	) strcat(buf, " negative");
    if (res_flags & RES_HOLY		) strcat(buf, " holy");
    if (res_flags & RES_ENERGY		) strcat(buf, " energy");
    if (res_flags & RES_MENTAL		) strcat(buf, " mental");
    if (res_flags & RES_DISEASE		) strcat(buf, " disease");
    if (res_flags & RES_DROWNING	) strcat(buf, " drowning");
    if (res_flags & RES_LIGHT		) strcat(buf, " light");
    if (res_flags & RES_IRON		) strcat(buf, " iron");
    if (res_flags & RES_WOOD		) strcat(buf, " wood");
    if (res_flags & RES_SILVER		) strcat(buf, " silver");
    if (res_flags & RES_KILL 		) strcat( buf, " kill" );
    if (res_flags & RES_WATER		) strcat(buf, " water");
    /* @@@NIB : 20070127*/
    if (res_flags & RES_AIR		) strcat(buf, " air" );
    if (res_flags & RES_EARTH		) strcat(buf, " earth" );
    if (res_flags & RES_PLANT		) strcat(buf, " plant" );

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *vuln_bit_name(int vuln_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (vuln_flags & VULN_SUMMON	) strcat(buf, " summon");
    if (vuln_flags & VULN_CHARM		) strcat(buf, " charm");
    if (vuln_flags & VULN_MAGIC		) strcat(buf, " magic");
    if (vuln_flags & VULN_WEAPON	) strcat(buf, " weapon");
    if (vuln_flags & VULN_BASH		) strcat(buf, " blunt");
    if (vuln_flags & VULN_PIERCE	) strcat(buf, " piercing");
    if (vuln_flags & VULN_SLASH		) strcat(buf, " slashing");
    if (vuln_flags & VULN_FIRE		) strcat(buf, " fire");
    if (vuln_flags & VULN_COLD		) strcat(buf, " cold");
    if (vuln_flags & VULN_LIGHTNING	) strcat(buf, " lightning");
    if (vuln_flags & VULN_ACID		) strcat(buf, " acid");
    if (vuln_flags & VULN_POISON	) strcat(buf, " poison");
    if (vuln_flags & VULN_NEGATIVE	) strcat(buf, " negative");
    if (vuln_flags & VULN_HOLY		) strcat(buf, " holy");
    if (vuln_flags & VULN_ENERGY	) strcat(buf, " energy");
    if (vuln_flags & VULN_MENTAL	) strcat(buf, " mental");
    if (vuln_flags & VULN_DISEASE	) strcat(buf, " disease");
    if (vuln_flags & VULN_DROWNING	) strcat(buf, " drowning");
    if (vuln_flags & VULN_LIGHT		) strcat(buf, " light");
    if (vuln_flags & VULN_IRON		) strcat(buf, " iron");
    if (vuln_flags & VULN_WOOD		) strcat(buf, " wood");
    if (vuln_flags & VULN_SILVER	) strcat(buf, " silver");
    if (vuln_flags & VULN_KILL 		) strcat(buf, " kill" );
    if (vuln_flags & VULN_WATER		) strcat(buf, " water");
    /* @@@NIB : 20070127*/
    if (vuln_flags & VULN_AIR		) strcat(buf, " air" );
    if (vuln_flags & VULN_EARTH		) strcat(buf, " earth" );
    if (vuln_flags & VULN_PLANT		) strcat(buf, " plant" );

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *wear_bit_name(int wear_flags)
{
    static char buf[512];

    buf [0] = '\0';
    if (wear_flags & ITEM_TAKE		) strcat(buf, " take");
    if (wear_flags & ITEM_WEAR_FINGER	) strcat(buf, " finger");
    if (wear_flags & ITEM_WEAR_NECK	) strcat(buf, " neck");
    if (wear_flags & ITEM_WEAR_BODY	) strcat(buf, " torso");
    if (wear_flags & ITEM_WEAR_HEAD	) strcat(buf, " head");
    if (wear_flags & ITEM_WEAR_LEGS	) strcat(buf, " legs");
    if (wear_flags & ITEM_WEAR_FEET	) strcat(buf, " feet");
    if (wear_flags & ITEM_WEAR_HANDS	) strcat(buf, " hands");
    if (wear_flags & ITEM_WEAR_ARMS	) strcat(buf, " arms");
    if (wear_flags & ITEM_WEAR_SHIELD	) strcat(buf, " shield");
    if (wear_flags & ITEM_WEAR_ABOUT	) strcat(buf, " body");
    if (wear_flags & ITEM_WEAR_WAIST	) strcat(buf, " waist");
    if (wear_flags & ITEM_WEAR_WRIST	) strcat(buf, " wrist");
    if (wear_flags & ITEM_WIELD		) strcat(buf, " wield");
    if (wear_flags & ITEM_HOLD		) strcat(buf, " hold");
    if (wear_flags & ITEM_NO_SAC	) strcat(buf, " nosac");
    if (wear_flags & ITEM_WEAR_FLOAT	) strcat(buf, " float");
    if (wear_flags & ITEM_WEAR_FACE	) strcat(buf, " face");
    if (wear_flags & ITEM_WEAR_EYES	) strcat(buf, " eyes");
    if (wear_flags & ITEM_WEAR_EAR	) strcat(buf, " ear");
    if (wear_flags & ITEM_WEAR_ANKLE	) strcat(buf, " ankle");
    if (wear_flags & ITEM_WEAR_TABARD	) strcat(buf, " tabard");
    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *form_bit_name(long form_flags)
{
    static char buf[512];

    buf[0] = '\0';
    if (form_flags & FORM_POISON	) strcat(buf, " poison");
    else if (form_flags & FORM_EDIBLE	) strcat(buf, " edible");
    if (form_flags & FORM_MAGICAL	) strcat(buf, " magical");
    if (form_flags & FORM_INSTANT_DECAY	) strcat(buf, " instant_rot");
    if (form_flags & FORM_OTHER		) strcat(buf, " other");
    if (form_flags & FORM_ANIMAL	) strcat(buf, " animal");
    if (form_flags & FORM_SENTIENT	) strcat(buf, " sentient");
    if (form_flags & FORM_UNDEAD	) strcat(buf, " undead");
    if (form_flags & FORM_CONSTRUCT	) strcat(buf, " construct");
    if (form_flags & FORM_MIST		) strcat(buf, " mist");
    if (form_flags & FORM_INTANGIBLE	) strcat(buf, " intangible");
    if (form_flags & FORM_BIPED		) strcat(buf, " biped");
    if (form_flags & FORM_CENTAUR	) strcat(buf, " centaur");
    if (form_flags & FORM_INSECT	) strcat(buf, " insect");
    if (form_flags & FORM_SPIDER	) strcat(buf, " spider");
    if (form_flags & FORM_CRUSTACEAN	) strcat(buf, " crustacean");
    if (form_flags & FORM_WORM		) strcat(buf, " worm");
    if (form_flags & FORM_BLOB		) strcat(buf, " blob");
    if (form_flags & FORM_MAMMAL	) strcat(buf, " mammal");
    if (form_flags & FORM_BIRD		) strcat(buf, " bird");
    if (form_flags & FORM_REPTILE	) strcat(buf, " reptile");
    if (form_flags & FORM_SNAKE		) strcat(buf, " snake");
    if (form_flags & FORM_DRAGON	) strcat(buf, " dragon");
    if (form_flags & FORM_AMPHIBIAN	) strcat(buf, " amphibian");
    if (form_flags & FORM_FISH		) strcat(buf, " fish");
    if (form_flags & FORM_COLD_BLOOD 	) strcat(buf, " cold_blooded");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *part_bit_name(int part_flags)
{
    static char buf[512];

    buf[0] = '\0';
    if (part_flags & PART_HEAD		) strcat(buf, " head");
    if (part_flags & PART_ARMS		) strcat(buf, " arms");
    if (part_flags & PART_LEGS		) strcat(buf, " legs");
    if (part_flags & PART_HEART		) strcat(buf, " heart");
    if (part_flags & PART_BRAINS	) strcat(buf, " brains");
    if (part_flags & PART_GUTS		) strcat(buf, " guts");
    if (part_flags & PART_HANDS		) strcat(buf, " hands");
    if (part_flags & PART_FEET		) strcat(buf, " feet");
    if (part_flags & PART_FINGERS	) strcat(buf, " fingers");
    if (part_flags & PART_EAR		) strcat(buf, " ears");
    if (part_flags & PART_EYE		) strcat(buf, " eyes");
    if (part_flags & PART_LONG_TONGUE	) strcat(buf, " long_tongue");
    if (part_flags & PART_EYESTALKS	) strcat(buf, " eyestalks");
    if (part_flags & PART_TENTACLES	) strcat(buf, " tentacles");
    if (part_flags & PART_FINS		) strcat(buf, " fins");
    if (part_flags & PART_WINGS		) strcat(buf, " wings");
    if (part_flags & PART_TAIL		) strcat(buf, " tail");
    if (part_flags & PART_CLAWS		) strcat(buf, " claws");
    if (part_flags & PART_FANGS		) strcat(buf, " fangs");
    if (part_flags & PART_HORNS		) strcat(buf, " horns");
    if (part_flags & PART_SCALES	) strcat(buf, " scales");
    if (part_flags & PART_GILLS)		strcat(buf, " gills");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *weapon_bit_name(int weapon_flags)
{
    static char buf[512];

    buf[0] = '\0';
    if (weapon_flags & WEAPON_ACIDIC	) strcat(buf, " acidic");
    if (weapon_flags & WEAPON_ANNEALED	) strcat(buf, " annealed");
    if (weapon_flags & WEAPON_BARBED	) strcat(buf, " barbed");
    if (weapon_flags & WEAPON_BLAZE	) strcat(buf, " blaze");
    if (weapon_flags & WEAPON_CHIPPED	) strcat(buf, " chipped");
    if (weapon_flags & WEAPON_DULL	) strcat(buf, " dull");
    if (weapon_flags & WEAPON_FLAMING	) strcat(buf, " flaming");
    if (weapon_flags & WEAPON_FROST	) strcat(buf, " frost");
    if (weapon_flags & WEAPON_OFFHAND	) strcat(buf, " off-handed");
    if (weapon_flags & WEAPON_ONEHAND	) strcat(buf, " one-handed");
    if (weapon_flags & WEAPON_POISON	) strcat(buf, " poison");
    if (weapon_flags & WEAPON_RESONATE	) strcat(buf, " resonate");
    if (weapon_flags & WEAPON_SHARP	) strcat(buf, " sharp");
    if (weapon_flags & WEAPON_SHOCKING 	) strcat(buf, " shocking");
    if (weapon_flags & WEAPON_SUCKLE	) strcat(buf, " suckle");
    if (weapon_flags & WEAPON_THROWABLE	) strcat(buf, " throwable");
    if (weapon_flags & WEAPON_TWO_HANDS ) strcat(buf, " two-handed");
    if (weapon_flags & WEAPON_VAMPIRIC	) strcat(buf, " vampiric");
    if (weapon_flags & WEAPON_VORPAL	) strcat(buf, " vorpal");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *cont_bit_name( int cont_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (cont_flags & CONT_CLOSEABLE	) strcat(buf, " closable");
    if (cont_flags & CONT_PICKPROOF	) strcat(buf, " pickproof");
    if (cont_flags & CONT_CLOSED	) strcat(buf, " closed");
    if (cont_flags & CONT_LOCKED	) strcat(buf, " locked");
    if (cont_flags & CONT_PUSHOPEN	) strcat(buf, " pushopen");
    if (cont_flags & CONT_SNAPKEY	) strcat(buf, " snapkey");
    if (cont_flags & CONT_CLOSELOCK	) strcat(buf, " closelock");

    return (buf[0] != '\0' ) ? buf+1 : "none";
}


char *off_bit_name(int off_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (off_flags & OFF_AREA_ATTACK	) strcat(buf, " area attack");
    if (off_flags & OFF_BACKSTAB	) strcat(buf, " backstab");
    if (off_flags & OFF_BASH		) strcat(buf, " bash");
    if (off_flags & OFF_BERSERK		) strcat(buf, " berserk");
    if (off_flags & OFF_DISARM		) strcat(buf, " disarm");
    if (off_flags & OFF_DODGE		) strcat(buf, " dodge");
    if (off_flags & OFF_FADE		) strcat(buf, " fade");
    if (off_flags & OFF_KICK		) strcat(buf, " kick");
    if (off_flags & OFF_KICK_DIRT	) strcat(buf, " kick_dirt");
    if (off_flags & OFF_PARRY		) strcat(buf, " parry");
    if (off_flags & OFF_RESCUE		) strcat(buf, " rescue");
    if (off_flags & OFF_TAIL		) strcat(buf, " tail");
    if (off_flags & OFF_TRIP		) strcat(buf, " trip");
    if (off_flags & OFF_CRUSH		) strcat(buf, " crush");
    if (off_flags & ASSIST_ALL		) strcat(buf, " assist_all");
    if (off_flags & ASSIST_NPC		) strcat(buf, " assist_npc");
    if (off_flags & ASSIST_ALIGN	) strcat(buf, " assist_align");
    if (off_flags & ASSIST_RACE		) strcat(buf, " assist_race");
    if (off_flags & ASSIST_PLAYERS	) strcat(buf, " assist_players");
    if (off_flags & ASSIST_GUARD	) strcat(buf, " assist_guard");
    if (off_flags & ASSIST_VNUM		) strcat(buf, " assist_vnum");
    if (off_flags & OFF_MAGIC		) strcat(buf, " magic");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


char *channel_flag_bit_name(int channel_flags)
{
    static char buf[512];

    buf[0] = '\0';

    if (channel_flags & FLAG_CT		) strcat(buf, " ct");
    if (channel_flags & FLAG_FLAMING 	) strcat(buf, " flaming");
    if (channel_flags & FLAG_GOSSIP	) strcat(buf, " gossip");
    if (channel_flags & FLAG_HELPER	) strcat(buf, " helper");
    if (channel_flags & FLAG_MUSIC	) strcat(buf, " music");
    if (channel_flags & FLAG_OOC	) strcat(buf, " ooc");
    if (channel_flags & FLAG_QUOTE	) strcat(buf, " quote");
    if (channel_flags & FLAG_TELLS	) strcat(buf, " tells");
    if (channel_flags & FLAG_YELL	) strcat(buf, " yell");


    return ( buf[0] != '\0' ) ? buf+1 : "none";
}

