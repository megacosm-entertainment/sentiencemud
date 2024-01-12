/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
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
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "db.h"
/* VIZZWILDS - Include wilds.h header */
#include "wilds.h"
#include "scripts.h"
#include "sha256.h"

bool can_see_imm(CHAR_DATA *ch, CHAR_DATA *victim);
void look_through_telescope(CHAR_DATA *ch, OBJ_DATA *telescope, char *argument);
void look_compass(CHAR_DATA *ch, OBJ_DATA *compass);
void look_sextant(CHAR_DATA *ch, OBJ_DATA *sextant);
void look_map(CHAR_DATA *ch, OBJ_DATA *map);
void look_portal(CHAR_DATA *ch, OBJ_DATA *portal);
ROOM_INDEX_DATA *get_portal_destination(CHAR_DATA *ch, OBJ_DATA *portal, bool allow_random);
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

char *const where_name[] = {
    "{Y<used as light>       {x",
    "{B<worn on finger>      {x",
    "{B<worn on finger>      {x",
    "{B<worn around neck>    {x",
    "{B<worn around neck>    {x",
    "{B<worn on torso>       {x",
    "{B<worn on head>        {x",
    "{B<worn on legs>        {x",
    "{B<worn on feet>        {x",
    "{B<worn on hands>       {x",
    "{B<worn on arms>        {x",
    "{B<worn as shield>      {x",
    "{B<worn about body>     {x",
    "{B<worn about waist>    {x",
    "{B<worn around wrist>   {x",
    "{B<worn around wrist>   {x",
    "{R<wielded>             {x",
    "{R<held>                {x",
    "{R<secondary weapon>    {x",
    "{B<worn on ring finger> {x",
    "{B<worn behind back>    {x",
    "{B<worn over shoulder>  {x",
    "{B<worn around ankle>   {x",
    "{B<worn around ankle>   {x",
    "{B<worn on ear>         {x",
    "{B<worn on ear>         {x",
    "{B<worn over eyes>      {x",
    "{B<worn over face>      {x",
    "{c<head>                {x",
    "{c<body>                {x",
    "{c<upper left arm>      {x",
    "{c<upper right arm>     {x",
    "{c<upper left leg>      {x",
    "{c<upper right leg>     {x",
    "{W<lodged in head>      {x",
    "{W<lodged in body>      {x",
    "{W<lodged in arm>       {x",
    "{W<lodged in arm>       {x",
    "{W<lodged in leg>       {x",
    "{W<lodged in leg>       {x",
    "{G<entangled>           {x",
    "{D<concealed from view> {x",
    "{M<floating nearby>     {x",
    "{c<lower left arm>      {x",
    "{c<lower right arm>     {x",
    "{c<lower left leg>      {x",
    "{c<lower right leg>     {x",
    "{c<left shoulder>       {x",
    "{c<right shoulder>      {x",
    "{c<back>                {x",
    "{Y<tabard>              {x",
	"{c<neck>                {x",

};

int wear_params[MAX_WEAR][7] = {
/*	seen,		autoeq		remove		shifted		affects		uneq_death	always_remove */
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		TRUE },  // Light
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Finger
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Finger
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Neck
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Neck
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Torso
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Head
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Legs
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Feet
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Hands
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Arms
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Shield
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Body
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Waist
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Wrist
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Wrist
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Wield
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Held
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Secondary
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Ring Finger
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Back
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Shoulders
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Ankle
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Ankle
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Ear
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Ear
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Eyes
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Face
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Head Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Body Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Arm Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Arm Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Leg Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Leg Tattoo
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Head
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Body
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Arm
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Arm
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Leg
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Lodged in Leg
	{ TRUE,		FALSE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE }, // Entangled
	{ FALSE,	FALSE,		TRUE,		FALSE,		FALSE,		TRUE,		TRUE },  // Concealed
	{ TRUE,		FALSE,		TRUE,		TRUE,		FALSE,		FALSE,		TRUE },  // Floating
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Arm Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Arm Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Leg Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Leg Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Shoulder Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Shoulder Tattoo
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Back Tattoo
	{ TRUE,		TRUE,		TRUE,		FALSE,		TRUE,		TRUE,		FALSE }, // Tabard
	{ TRUE,		FALSE,		TRUE,		FALSE,		TRUE,		FALSE,		FALSE }, // Neck Tattoo
};

int wear_concealed[] = {
	WEAR_NONE,
	WEAR_HANDS,
	WEAR_HANDS,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_HANDS,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_FEET,
	WEAR_FEET,
	WEAR_HEAD,
	WEAR_HEAD,
	WEAR_FACE,
	WEAR_HEAD,
	WEAR_FACE,
	WEAR_BODY,
	WEAR_ARMS,
	WEAR_ARMS,
	WEAR_LEGS,
	WEAR_LEGS,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_ABOUT,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_ARMS,
	WEAR_ARMS,
	WEAR_LEGS,
	WEAR_LEGS,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_NONE,
	WEAR_NONE,

};

/* Determines the VIEWED order of the wear locations */
int wear_view_order[] = {
	WEAR_LIGHT,
	WEAR_FINGER_L,
	WEAR_RING_FINGER,
	WEAR_FINGER_R,
	WEAR_NECK_1,
	WEAR_NECK_2,
	WEAR_TATTOO_NECK,
	WEAR_BODY,
	WEAR_TATTOO_TORSO,
	WEAR_BACK,
	WEAR_TATTOO_BACK,
	WEAR_SHOULDER,
	WEAR_TATTOO_SHOULDER_L,
	WEAR_TATTOO_SHOULDER_R,
	WEAR_HEAD,
	WEAR_TATTOO_HEAD,
	WEAR_FACE,
	WEAR_EYES,
	WEAR_EAR_L,
	WEAR_EAR_R,
	WEAR_ARMS,
	WEAR_TATTOO_UPPER_ARM_L,
	WEAR_TATTOO_LOWER_ARM_L,
	WEAR_TATTOO_UPPER_ARM_R,
	WEAR_TATTOO_LOWER_ARM_R,
	WEAR_WRIST_L,
	WEAR_WRIST_R,
	WEAR_HANDS,
	WEAR_LEGS,
	WEAR_TATTOO_UPPER_LEG_L,
	WEAR_TATTOO_LOWER_LEG_L,
	WEAR_TATTOO_UPPER_LEG_R,
	WEAR_TATTOO_LOWER_LEG_R,
	WEAR_ANKLE_L,
	WEAR_ANKLE_R,
	WEAR_FEET,
	WEAR_ABOUT,
	WEAR_TABARD,
	WEAR_WAIST,
	WEAR_SHIELD,
	WEAR_WIELD,
	WEAR_SECONDARY,
	WEAR_HOLD,
	WEAR_LODGED_HEAD,
	WEAR_LODGED_TORSO,
	WEAR_LODGED_ARM_L,
	WEAR_LODGED_ARM_R,
	WEAR_LODGED_LEG_L,
	WEAR_LODGED_LEG_R,
	WEAR_ENTANGLED,
	WEAR_CONCEALED,
	WEAR_FLOATING,
	WEAR_NONE
};

/* For the player count */
int max_on = 0;

/*
 * Local functions.
 */
char *format_obj_to_char
args((OBJ_DATA * obj, CHAR_DATA * ch, bool fShort));
void show_list_to_char
args((OBJ_DATA * list, CHAR_DATA * ch, bool fShort, bool fShowNothing));
void show_map_and_description args((CHAR_DATA * ch, ROOM_INDEX_DATA *room));
void show_char_to_char_0 args((CHAR_DATA * victim, CHAR_DATA * ch));
void show_char_to_char_1 args((CHAR_DATA * victim, CHAR_DATA * ch, bool examine));
void create_map args((CHAR_DATA * ch, ROOM_INDEX_DATA *start_room, char *map));
void show_char_to_char
args((CHAR_DATA * list, CHAR_DATA * ch, CHAR_DATA * victim));
bool check_blind args((CHAR_DATA * ch));
int show_map(CHAR_DATA * ch, char *buf, char *map, int counter, int line);
char *get_char_where args((CHAR_DATA * ch));
CHURCH_DATA *find_char_church(CHAR_DATA * ch);
int find_char_position_in_church(CHAR_DATA * ch);
void format_page(sh_int n, char *a, CHAR_DATA * wch);
/*
int get_squares_to_show_x(ROOM_INDEX_DATA *pRoom, int bonus_view);
int get_squares_to_show_y(ROOM_INDEX_DATA *pRoom, int bonus_view);
*/
char determine_room_type(ROOM_INDEX_DATA *room);
void convert_map_char(char *buf, char ch);
void show_equipment(CHAR_DATA *ch, CHAR_DATA *victim);


char *format_obj_to_char(OBJ_DATA * obj, CHAR_DATA * ch, bool fShort)
{
    static char buf[MAX_STRING_LENGTH];

    buf[0] = '\0';

    if ((fShort && (obj->short_descr == NULL || obj->short_descr[0] == '\0'))
		|| (obj->description == NULL || obj->description[0] == '\0'))
		return buf;

	// How would you be seeing this?
	if (IS_SET(obj->extra[2], ITEM_EXCLUDE_LIST))
		strcat(buf, "{D(Excluded){x ");
    if (IS_OBJ_STAT(obj, ITEM_INVIS))
		strcat(buf, "{B(Invis){w ");
    if (((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) || IS_AFFECTED(ch, AFF_DETECT_MAGIC)) && IS_OBJ_STAT(obj, ITEM_MAGIC))
		strcat(buf, "{M(Magical){w ");
    if (IS_OBJ_STAT(obj, ITEM_GLOW))
		strcat(buf, "{W(Glowing){w ");
    if (IS_OBJ_STAT(obj, ITEM_HUM))
		strcat(buf, "{C(Humming){w ");
    if (IS_OBJ_STAT(obj, ITEM_BLESS) && ((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) || IS_AFFECTED(ch, AFF_DETECT_GOOD) || (!IS_NPC(ch) && IS_REMORT(ch))))
		strcat(buf, "{y(Gold Aura){w ");
    if (IS_OBJ_STAT(obj, ITEM_EVIL) && ((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) || IS_AFFECTED(ch, AFF_DETECT_EVIL) || (!IS_NPC(ch) && IS_REMORT(ch))))
		strcat(buf, "{R(Red Aura){w ");
    if (IS_SET(obj->extra[1], ITEM_KEPT))
		strcat(buf, "{b({BK{b){x ");
    if (IS_OBJ_STAT(obj, ITEM_PLANTED))
		strcat(buf, "{R(Planted){w ");
    if (IS_SET(obj->extra[1], ITEM_BURIED))
		strcat(buf, "{y(Buried){w ");
    if (fShort)
    {
	if (obj->short_descr != NULL)
	    strcat(buf, obj->short_descr);
	strcat(buf, " ");

	if (IS_WEAPON(obj))
	{
	    if (obj->condition == 0)
		strcat(buf, "{y(Broken){x");
	}
	else
	{
	    strcat(buf, object_damage_table[URANGE
		    (0, 9 - (int) (((float) obj->condition)/10),9)].name);
	}

	/* Show trade class if a commodity */
	if (obj->item_type == ITEM_TRADE_TYPE && obj->value[0] != -1)
	{
	    strcat(buf, "{Y(");
	    strcat(buf, trade_table[ obj->value[0] ].name);
	    strcat(buf, "){x");
	}
    }
    else
    {
	if (obj->description != NULL)
	{
	    if (obj->item_type == ITEM_CART
	    && get_cart_pulled(obj) != NULL)
	    {
		sprintf(buf, "{B%s is here, pulled by %s.{x",
			 capitalize(obj->short_descr),
		 	 get_cart_pulled(obj) == ch ? "you" :
			     pers(get_cart_pulled(obj), ch));
	    }
	    else
	    {
		strcat(buf, "{B");

	        /* If its a ship then describe here that it is moving */
		if (obj != NULL && obj->item_type == ITEM_SHIP)
		{
		    SHIP_DATA *ship;
		    char buf2[MAX_STRING_LENGTH];

		    ship = obj->ship;

		    if( IS_NULLSTR(ship->ship_name) )
		    {
				if (obj->ship->ship_power > SHIP_SPEED_STOPPED)
				{
					if (obj->ship->ship_type == SHIP_AIR_SHIP)
					{
						sprintf(buf2, "{M%s %s flies high above heading %s.{x",
							get_article(ship->index->name, true), ship->index->name, dir_name[ship->steering.compass]);
					}
					else
					{
						sprintf(buf2, "{M%s %s powers through the water sailing %s.{x",
							get_article(ship->index->name, true), ship->index->name, dir_name[ship->steering.compass]);
					}
				}
				else if( ship->scuttle_time > 0 )
				{
					if (IS_NULLSTR(ship->flag))
					{
						sprintf(buf2,
							"{R%s %s burns brightly as flames engulf the vessel!{x",
							get_article(ship->index->name, true), ship->index->name);
					}
					else
					{
						sprintf(buf2,
							"{R%s %s flying the flag, '{x%s{R', burns brightly as flames engulf the vessel!{x",
							get_article(ship->index->name, true), ship->index->name, ship->flag);
					}
				}
				else
				{
					if (obj->ship->ship_type == SHIP_AIR_SHIP)
					{
						if( ship->ship_power == SHIP_SPEED_LANDED )
						{
							if( IS_NULLSTR(ship->flag) )
							{
								sprintf(buf2, "{M%s %s floats gently just above the ground.{x",
									get_article(ship->index->name, true), ship->index->name);
							}
							else
							{
								sprintf(buf2, "{M%s %s, flying the flag '{x%s{M', floats gently just above the ground.{x",
									get_article(ship->index->name, true), ship->index->name, ship->flag);
							}
						}
						else
						{
							if( IS_NULLSTR(ship->flag) )
							{
								sprintf(buf2, "{M%s %s floats gently high in the air.{x",
									get_article(ship->index->name, true), ship->index->name);
							}
							else
							{
								sprintf(buf2, "{M%s %s, flying the flag '{x%s{M', floats gently high in the air.{x",
									get_article(ship->index->name, true), ship->index->name, ship->flag);
							}
						}
					}
					else
					{
						if (IS_NULLSTR(ship->flag))
						{
							sprintf(buf2,
								"{M%s %s gracefully floats here.{x",
								get_article(ship->index->name, true), ship->index->name);
						}
						else
						{
							sprintf(buf2,
								"{M%s %s, flying the flag '{x%s{M', floats here.{x",
								get_article(ship->index->name, true), ship->index->name, ship->flag);
						}
					}
				}
			}
			else
			{
				if (obj->ship->ship_power > SHIP_SPEED_STOPPED)
				{
					if (obj->ship->ship_type == SHIP_AIR_SHIP)
					{
						sprintf(buf2, "{MThe %s '{x%s{M' flies high above heading %s.{x", ship->index->name, ship->ship_name, dir_name[ship->steering.compass]);
					}
					else
					{
						sprintf(buf2, "{MThe %s '{x%s{M', powers through the water sailing %s.{x", ship->index->name, ship->ship_name, dir_name[ship->steering.compass]);
					}
				}
				else if( ship->scuttle_time > 0 )
				{
					if (IS_NULLSTR(ship->flag))
					{
						sprintf(buf2,
							"{RThe %s named '{x%s{R', burns brightly as flames engulf the vessel!{x",
							ship->index->name, ship->ship_name);
					}
					else
					{
						sprintf(buf2,
							"{RThe %s named '{x%s{R', flying the flag, '{x%s{R', burns brightly as flames engulf the vessel!{x",
							ship->index->name, ship->ship_name, ship->flag);
					}
				}
				else
				{
					if (obj->ship->ship_type == SHIP_AIR_SHIP)
					{
						if( ship->ship_power == SHIP_SPEED_LANDED )
						{
							if( IS_NULLSTR(ship->flag) )
							{
								sprintf(buf2, "{MThe %s {x%s{M floats gently just above the ground.{x", ship->index->name, ship->ship_name);
							}
							else
							{
								sprintf(buf2, "{MThe %s {x%s{M, flying the flag '{x%s{M', floats gently just above the ground.{x", ship->index->name, ship->ship_name, ship->flag);
							}
						}
						else
						{
							if( IS_NULLSTR(ship->flag) )
							{
								sprintf(buf2, "{MThe %s {x%s{M floats gently high in the air.{x", ship->index->name, ship->ship_name);
							}
							else
							{
								sprintf(buf2, "{MThe %s {x%s{M, flying the flag '{x%s{M', floats gently high in the air.{x", ship->index->name, ship->ship_name, ship->flag);
							}
						}
					}
					else
					{
						if (IS_NULLSTR(ship->flag))
						{
							sprintf(buf2,
								"{MThe %s named '{x%s{M' gracefully floats here.{x",
								ship->index->name, ship->ship_name);
						}
						else
						{
							sprintf(buf2,
								"{MThe %s named '{x%s{M', flying the flag '{x%s{M', floats here.{x",
								ship->index->name, ship->ship_name, ship->flag);
						}
					}
				}
			}

			strcat(buf, buf2);
		}
		else
		{
		    strcat(buf, obj->description);
		}
	    }
	}
    }

    return buf;
}


/*
 * ROOM VERSION
 * Show a list to a character.
 * Can coalesce duplicated items.
 */
void show_list_to_char(OBJ_DATA *list, CHAR_DATA *ch, bool fShort,
		bool fShowNothing)
{
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    BUFFER *output;
    char **prgpstrShow;
    int *prgnShow;
    char *pstrShow;
    OBJ_DATA *obj;
    int nShow = 0;
    int iShow;
    int count, max;
    bool fCombine;
    OBJ_DATA *mist = NULL, *mobj = NULL;

    if (ch->desc == NULL)
	return;

    if (ch == NULL)
    {
	bug("show_list_to_char: ch was null!", 0);
	return;
    }

    /*
     * Alloc space for output lines.
     */
    output = new_buf();

    count = 0;
    for (obj = list; obj != NULL; obj = obj->next_content)
	count++;
    prgpstrShow = alloc_mem(count * sizeof(char *));
    prgnShow = alloc_mem(count * sizeof(int));
    nShow = 0;
    max = 0;
    mist = NULL;

    /* Figure out if there is a mist-type item in the room, which blocks objects from view. */
    if (list != NULL && list->carried_by == NULL && list->in_room != NULL) {
	for (mobj = list->in_room->contents; mobj != NULL; mobj = mobj->next_content) {
	    if (IS_MIST(mobj) && MIST(mobj)->obscure_objs > max)
		{
			max = MIST(mobj)->obscure_objs;
	    	mist = mobj;
		}
	}
    }

    /*
     * Format the list of objects.
     */
    for (obj = list; obj != NULL; obj = obj->next_content)
    {
	/* You dont see hidden objects in a list.
	   Makes sense since hidden objects can*only* be on the ground. */
	if (IS_SET(obj->extra[0], ITEM_HIDDEN))
	    continue;

	// Similar to hidden items, but you can still target them and they can never be discovered.
	//  Immortals with HOLYLIGHT will be able to see this though (for debugging purposes)
	if (IS_SET(obj->extra[2], ITEM_EXCLUDE_LIST) && !IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[0], PLR_HOLYLIGHT)))
		continue;

	/* Mist type blocks random objs from view. */
	if (mist != NULL && mist != obj && IS_MIST(mist) && MIST(mist)->obscure_objs > number_percent())
	    continue;

	if (obj->wear_loc == WEAR_NONE && can_see_obj(ch, obj)) {
	    pstrShow = format_obj_to_char(obj, ch, fShort);

	    fCombine = FALSE;

	    /*
	     * Look for duplicates, case sensitive. (now it is case sensitive)
	     * Matches tend to be near end so run loop backwards.
	     */
	    for (iShow = nShow - 1; iShow >= 0; iShow--) {
		if (!strcmp(prgpstrShow[iShow], pstrShow)) {
		    prgnShow[iShow]++;
		    fCombine = TRUE;
		    break;
		}
	    }

	    /*
	     * Couldn't combine, or didn't want to.
	     */
	    if (!fCombine)
	    {
		prgpstrShow[nShow] = str_dup(pstrShow);
		prgnShow[nShow] = 1;
		nShow++;
	    }
	}
    }

    /* Do feign. */
    for (victim = ch->in_room->people; victim != NULL; victim = victim->next_in_room)
    {
	if (mist && number_percent() < mist->value[0])
	    continue;

	if ((victim->position == POS_FEIGN) && (victim != ch) && !fShort)
	{
	    sprintf(buf, "     {yThe corpse of %s is lying here.{x\n\r",
		pers(victim, ch));
	    if (!add_buf(output, buf))
	    {
		log_string("act_info, corpse addbuf failed.");
		return;
	    }
	}
    }

    /*
     * Output the formatted list.
     */
    for (iShow = 0; iShow < nShow; iShow++)
    {
	if (prgpstrShow[iShow][0] == '\0') {
	    free_string(prgpstrShow[iShow]);
	    continue;
	}

	if (prgnShow[iShow] != 1) {
	    sprintf(buf, "{Y({G%2d{Y) {x", prgnShow[iShow]);
	    if (!add_buf(output, buf))
	    {
		log_string("act_info, addbuf, combine failed");
		return;
	    }
	} else {
	    if (!add_buf(output, "     "))
	    {
		log_string("act_info, addbuf, combine failed");
		return;
	    }
	}

	add_buf(output, "{x");
	add_buf(output, prgpstrShow[iShow]);
	add_buf(output, "\n\r{x");
	free_string(prgpstrShow[iShow]);
    }

    if (fShowNothing && nShow == 0)
    {
	/* if (IS_NPC(ch) || IS_SET(ch->comm, COMM_COMBINE)) */
	send_to_char("     ", ch);
	send_to_char("Nothing.\n\r", ch);
    }
    page_to_char(buf_string(output), ch);

    /*
     * Clean up.
     */
    free_buf(output);
    free_mem(prgpstrShow, count * sizeof(char *));
    free_mem(prgnShow, count * sizeof(int));
}


void show_char_to_char_0(CHAR_DATA * victim, CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];
    char message[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    buf[0] = '\0';

    if (victim->position == POS_FEIGN)
	return;

    if (RIDDEN(victim) && ch != RIDDEN(victim))
	return;

    if (RIDDEN(victim))
	return;

    if (IS_AFFECTED(victim, AFF_INVISIBLE) || IS_AFFECTED2(victim, AFF2_IMPROVED_INVIS))
		strcat(buf, "{B*{G");
    if (!IS_NPC(victim) && victim->desc == NULL)
		strcat(buf, "{Y[Lost Link]{G ");
    if (IS_SET(victim->comm, COMM_AFK))
		strcat(buf, "{Y[AFK] {G");
    if (victim->invis_level >= LEVEL_HERO)
		strcat(buf, "{B({WW{Ri{Yz{Gi{B) {G");
    if (IS_NPC(victim) && IS_SET(victim->act[1], ACT2_WIZI_MOB))
 		strcat(buf, "{B({WW{Ri{Yz{Gi{GMOB{B) {G");
    if (IS_AFFECTED(victim, AFF_CHARM))
		strcat(buf, "{Y(Charmed) {G");
    if (IS_AFFECTED(victim, AFF_PASS_DOOR))
		strcat(buf, "{C(Translucent) {G");
    if (IS_AFFECTED(victim, AFF_FAERIE_FIRE))
		strcat(buf, "{M(Pink Aura) {G");
    if (IS_EVIL(victim) && (IS_AFFECTED(ch, AFF_DETECT_EVIL) || (!IS_NPC(ch) && IS_REMORT(ch))))
		strcat(buf, "{R(Red Aura) {G");
    if (IS_GOOD(victim) && (IS_AFFECTED(ch, AFF_DETECT_GOOD) || (!IS_NPC(ch) && IS_REMORT(ch))))
		strcat(buf, "{y(Gold Aura) {G");
    if (IS_AFFECTED2(victim,AFF2_DARK_SHROUD))
    	strcat(buf, "{D(Dark Shroud) {G");
    if (victim->pk_timer > 0)
    	strcat(buf, "{r(Blood Aura) {G");

    if (IS_DEAD(victim))
		strcat(buf, "{DThe shadow of ");
    else
		strcat(buf, "{G");

    if (IS_QUESTING(ch) && IS_NPC(victim))
    {
        QUEST_PART_DATA *part;

        for (part = ch->quest->parts; part != NULL; part = part->next)
        {
	    if (part->mob != -1 && !part->complete)
	    {
                if (part->mob == victim->pIndexData->vnum && part->area == victim->pIndexData->area)
                {
   	 	    strcat(buf, "{R[X] {G");
		    break;
		}
	    }
        }
    }

    /* print name */
    if (IS_NPC(victim) ||
		((IS_MORPHED(victim) || IS_SHIFTED(victim)) && !can_see_shift(ch, victim)))
    {
        if (victim->position == POS_STANDING && victim->on == NULL && !MOUNTED(victim))
		{
            sprintf(buf2, "%s\n\r", victim->long_descr);
			buf2[0] = UPPER(buf2[0]);
			strcat(buf, buf2);
		}
        else
		{
			sprintf(buf2, "%s", victim->short_descr);
			buf2[0] = UPPER(buf2[0]);
			strcat(buf, buf2);
		}
    }
    else if (!IS_NPC(victim))
    {
        if (victim->position == POS_STANDING && victim->on == NULL && victim->pcdata->title[0])
		{
			char title[MIL];
			sprintf(title, string_replace_static(victim->pcdata->title, "$n", "{+%s"), victim->name);
			strcat(buf, title);
		}
		else {
			strcat(buf, "{+");
			strcat(buf, victim->name);
		}
    }

    switch (victim->position)
    {
	case POS_DEAD:
	    strcat(buf, " is {RDEAD!!{x");
	    break;
	case POS_MORTAL:
	    strcat(buf, " is mortally wounded.{x");
	    break;
	case POS_INCAP:
	    strcat(buf, " is incapacitated.{x");
	    break;
	case POS_STUNNED:
	    strcat(buf, " is lying here stunned.{x");
	    break;
	case POS_SLEEPING:
	    if (victim->on != NULL)
	    {
			sprintf(message, " is sleeping %s %s.",
				furniture_get_positional(victim->on_compartment->sleeping),
				furniture_get_short_description(victim->on, victim->on_compartment));
			strcat(buf, message);
		}
		else
			strcat(buf, " is sleeping here.");

	    if (IS_NPC(victim))
			strcat(buf, "\n\r");

	    break;
	case POS_RESTING:
	    if (victim->on != NULL)
	    {
			sprintf(message, " is resting %s %s.",
				furniture_get_positional(victim->on_compartment->resting),
				furniture_get_short_description(victim->on, victim->on_compartment));
			strcat(buf, message);
		}
		else
			strcat(buf, " is resting here.");

	    if (IS_NPC(victim))
			strcat(buf, "\n\r");

	    break;
	case POS_SITTING:
	    if (victim->on != NULL)
	    {
			sprintf(message, " is sitting %s %s.",
				furniture_get_positional(victim->on_compartment->sitting),
				furniture_get_short_description(victim->on, victim->on_compartment));
			strcat(buf, message);
		}
		else
			strcat(buf, " is sitting here.");

	    if (IS_NPC(victim))
			strcat(buf, "\n\r");

	    break;
	case POS_STANDING:
	    if (victim->on != NULL)
	    {
			sprintf(message, " is standing %s %s.",
				furniture_get_positional(victim->on_compartment->standing),
				furniture_get_short_description(victim->on, victim->on_compartment));
			strcat(buf, message);
		}
	    else if (MOUNTED(victim))
	    {
			strcat(buf, " {Gis here, riding ");
			strcat(buf, MOUNTED(victim)->short_descr);
			strcat(buf, ".");
	    }
	    else if (PULLING_CART(victim))
	    {
			strcat(buf, " {Gis here, pulling ");
			strcat(buf, PULLING_CART(victim)->short_descr);
			strcat(buf, ".");
	    }
	    else if (!IS_NPC(victim)
		   && ((!IS_MORPHED(victim) && !IS_SHIFTED(victim))
			   || can_see_shift(ch, victim)))
	    {
			strcat(buf, (IS_DEAD(victim)) ? " {Dis here.{x" : " {Gis here.{x");
	    }

	    break;
	case POS_FIGHTING:
	    strcat(buf, " {Gis here, fighting ");
	    if (victim->fighting == NULL)
	    {
			strcat(buf, "thin air??");
	    }
	    else if (victim->fighting == ch)
	    {
			strcat(buf, "YOU!");
	    }
	    else if (victim->in_room == victim->fighting->in_room)
	    {
			strcat(buf, pers(victim->fighting, ch));
			strcat(buf, ".");
	    }
	    else
	    {
			strcat(buf, "someone who left??");
	    }
	    if (IS_NPC(victim))
	    {
			strcat(buf, "\n\r");
	    }
	    break;
    }

	int auras = 0;

    if (!IS_NPC(victim) &&
		((!IS_MORPHED(victim) && !IS_SHIFTED(victim)) ||
		(victim->position != POS_STANDING || MOUNTED(victim) || can_see_shift(ch, victim))))
    {
        if (victim->alignment == 1000)
        {
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "\n\r{W%s is bathed in a holy white aura.{x", pers(victim, ch));
				buf2[4] = UPPER(buf2[4]);
				strcat(buf, buf2);

				auras++;
			}
		}

		if (victim->alignment == -1000)
		{
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "\n\r{R%s is surrounded with the burning fires of hell.{x", pers(victim, ch));
				buf2[4] = UPPER(buf2[4]);
				strcat(buf, buf2);

				auras++;
			}
		}

		if (IS_AFFECTED(victim, AFF_SANCTUARY))
		{
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "\n\r{W%s is surrounded with an aura of sanctuary.{x", pers(victim, ch));
				buf2[4] = UPPER(buf2[4]);
				strcat(buf, buf2);

				auras++;
			}
		}
    }
    else
    {
        if (victim->alignment == 1000)
		{
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "{W%s is bathed in a holy white aura.{x\n\r", pers(victim, ch));
				buf2[2] = UPPER(buf2[2]);
				strcat(buf, buf2);

				auras++;
			}
		}

		if (victim->alignment == -1000)
		{
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "{R%s is surrounded with the burning fires of hell.{x\n\r", pers(victim, ch));
				buf2[2] = UPPER(buf2[2]);
				strcat(buf, buf2);

				auras++;
			}
		}

		if (IS_AFFECTED(victim, AFF_SANCTUARY))
		{
			if (auras < MAX_AURAS_SHOWN)
			{
				sprintf(buf2, "{W%s is surrounded with an aura of sanctuary.{x\n\r", pers(victim, ch));
				buf2[2] = UPPER(buf2[2]);
				strcat(buf, buf2);

				auras++;
			}
		}
    }

	// Aura stuff
	ITERATOR aurait;
	AURA_DATA *aura;
	
	iterator_start(&aurait, victim->auras);
	while((aura = (AURA_DATA *)iterator_nextdata(&aurait)))
	{
		if (str_suffix("\n\r", buf))
			strcat(buf, "\n\r");

		if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
		{
			strcat(buf, "(");
			strcat(buf, aura->name);
			strcat(buf, ") ");
		}

		sprintf(buf2, aura->long_descr, pers(victim, ch));
		strcat(buf, buf2);

		// Reached the aura cap
		if (++auras >= MAX_AURAS_SHOWN) break;
	}
	iterator_stop(&aurait);

    if (!IS_NPC(victim) && ((!IS_MORPHED(victim) && !IS_SHIFTED(victim)) ||
		(victim->position != POS_STANDING || MOUNTED(victim) || can_see_shift(ch, victim))))
	{
    	strcat(buf, "\n\r");
	}

	// Does it have an EOL?
	if (str_suffix("\n\r", buf))
	{
		strcat(buf, "\n\r");
	}

    buf[0] = UPPER(buf[0]);
    send_to_char(buf, ch);
}


void show_char_to_char_1(CHAR_DATA * victim, CHAR_DATA * ch, bool examine)
{
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MSL];
    char name[MSL];
    int percent;


    if (can_see(victim, ch) && ch->invis_level < 150)
    {
	if (ch == victim)
	    act("$n looks at $mself.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	else
	{
	    act("$n looks at you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	    act("$n looks at $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	}
    }

    if (victim->description[0] != '\0')
    {
	send_to_char(victim->description, ch);
    }
    else
    {
	act("You see nothing special about $M.", ch, victim, NULL, NULL, NULL, NULL, NULL,
	    TO_CHAR);
    }

    sprintf(name, "%s", victim->name);
    name[0] = UPPER(name[0]);
    if (MOUNTED(victim))
    {
	sprintf(buf, "%s is riding %s.\n\r",
	    name,
	    MOUNTED(victim)->short_descr);
	send_to_char(buf, ch);
    }

    if (RIDDEN(victim))
    {
	sprintf(buf, "%s is being ridden by %s.\n\r", victim->short_descr,
		RIDDEN(victim)->name);
	send_to_char(buf, ch);
    }


    if (victim->max_hit > 0)
	percent = (100 * victim->hit) / victim->max_hit;
    else
	percent = -1;

    if (!IS_DEAD(victim))
	strcpy(buf, "{M");
    else
	strcpy(buf, "{D");
    sprintf(buf2, pers(victim, ch));
    buf2[0] = UPPER(buf2[0]);
    strcat(buf, buf2);

    if (IS_DEAD(victim))
	strcat(buf, " is dead.\n\r");
    else if (percent >= 100)
	strcat(buf, " is in excellent condition.\n\r");
    else if (percent >= 90)
	strcat(buf, " has a few scratches.\n\r");
    else if (percent >= 80)
	    strcat(buf, " has a few scratches and bruises.\n\r");
    else if (percent >= 70)
	strcat(buf, " has some small wounds.\n\r");
    else if (percent >= 60)
	strcat(buf, " has some small wounds and bruises.\n\r");
    else if (percent >= 50)
	strcat(buf, " has some nasty wounds and scratches.\n\r");
    else if (percent >= 40)
	strcat(buf, " looks pretty hurt.\n\r");
    else if (percent >= 30)
	    strcat(buf, " looks very hurt.\n\r");
    else if (percent >= 20)
	strcat(buf, " is in awful condition.\n\r");
    else if (percent >= 10)
	    strcat(buf, " is barely clinging to life.\n\r");
    else
	strcat(buf, " is on the verge of death.\n\r");
    strcat(buf, "{x");

    buf[0] = UPPER(buf[0]);
    send_to_char(buf, ch);

    send_to_char("\n\r", ch);
    show_equipment(ch, victim);

    if (victim != ch
    && !IS_NPC(ch)
    && number_percent() < get_skill(ch, gsk_peek))
    {
	send_to_char("\n\rYou peek at the inventory:\n\r", ch);
	check_improve(ch, gsk_peek, TRUE, 4);
	show_list_to_char(victim->carrying, ch, TRUE, TRUE);
    }

	if( IS_NPC(ch) || !IS_SET(ch->act[1], PLR_NOLORE) || examine )
	{
		if (IS_NPC(victim) && number_percent() < get_skill(ch, gsk_mob_lore))
		{
			if (IS_SET(victim->act[0], ACT_NO_LORE) && ch->tot_level <= victim->tot_level)
				act("\n\r{R$N is too powerful for you to lore.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
			{
				send_to_char("\n\r{YYou recognize the following things about this creature:{x\n\r", ch);
				show_basic_mob_lore(ch, victim);
				check_improve(ch, gsk_mob_lore, TRUE, 7);
			}
		}
    }

    if (IS_NPC(victim) && (!IS_SET(victim->act[0], ACT_NO_LORE) || ch->tot_level > victim->tot_level))
		p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);


	check_mob_factions(ch, victim);
    return;
}

void show_char_to_char(CHAR_DATA *list, CHAR_DATA *ch, CHAR_DATA *victim)
{
    CHAR_DATA *rch;
    OBJ_DATA *mist = NULL;
	OBJ_DATA *obj;

	int max = 0;


    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
    {
		if (IS_MIST(obj) && MIST(obj)->obscure_mobs > max)
		{
			mist = obj;
			max = MIST(obj)->obscure_mobs;
		}
    }

    for (rch = list; rch != NULL; rch = rch->next_in_room)
    {
	/* For the third eye spell, so victim doesn't come up in list */
	if (victim != NULL)
	{
	    if (rch == victim
		|| (RIDDEN(rch) && rch->in_room == RIDDEN(rch)->in_room
		    && RIDDEN(rch) != victim))
		continue;

	    if (!can_see(rch, victim))
		continue;

	    if (mist && IS_MIST(mist) && MIST(mist)->obscure_mobs > number_percent())
			continue;

 	    if (!IS_IMMORTAL(ch)
	    && IS_NPC(victim)
	    && IS_SET(victim->act[1], ACT2_WIZI_MOB))
 	        continue;

            if (IS_AFFECTED(rch, AFF_HIDE)
	    &&  !IS_AFFECTED(ch, AFF_DETECT_HIDDEN))
		continue;

	    if (can_see(victim, rch) || (IS_IMMORTAL(rch) && can_see_imm(ch,rch)))
	    {
		show_char_to_char_0(rch, ch);
		if (MOUNTED(rch)
		    && (rch->in_room == MOUNTED(rch)->in_room))
		    show_char_to_char_0(MOUNTED(rch), ch);
	    }
	    else
   	    if (room_is_dark(ch->in_room)
	    &&  IS_AFFECTED(rch, AFF_INFRARED))
	    {
		send_to_char
		    ("{RYou see glowing red eyes watching YOU!\n\r{x", ch);
	    }
	}
	else
	{
	    if (rch == ch
		|| (RIDDEN(rch) && rch->in_room == RIDDEN(rch)->in_room
		    && RIDDEN(rch) != ch))
		continue;

	    if (get_trust(ch) < rch->invis_level && !can_see_imm(ch,rch))
		continue;

	    if (mist && number_percent() < mist->value[1])
		continue;

            if (IS_AFFECTED(rch, AFF_HIDE)
	    &&  !IS_AFFECTED(ch, AFF_DETECT_HIDDEN))
		continue;

	    if (can_see(ch, rch) || (IS_IMMORTAL(rch) && can_see_imm(ch,rch)))
	    {
		show_char_to_char_0(rch, ch);
		if (MOUNTED(rch)
		    && (rch->in_room == MOUNTED(rch)->in_room))
		    show_char_to_char_0(MOUNTED(rch), ch);

	    }
	    else if (room_is_dark(ch->in_room)
            && IS_AFFECTED(rch, AFF_INFRARED))
	    {
		send_to_char
		    ("{RYou see glowing red eyes watching YOU!\n\r{x", ch);
	    }
	}
    }
}


bool check_blind(CHAR_DATA * ch)
{
    if (!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
	return TRUE;

    if (IS_AFFECTED(ch, AFF_BLIND))
	return FALSE;

    return TRUE;
}


void do_scroll(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[100];
    int lines;

    one_argument(argument, arg);

    if (arg[0] == '\0') {
	if (ch->lines == 0)
	    send_to_char("You do not page long messages.\n\r", ch);
	else {
	    sprintf(buf, "You currently display %d lines per page.\n\r",
		    ch->lines + 2);
	    send_to_char(buf, ch);
	}
	return;
    }

    if (!is_number(arg)) {
	send_to_char("You must provide a number.\n\r", ch);
	return;
    }

    lines = atoi(arg);

    if (lines == 0) {
	send_to_char("Paging disabled.\n\r", ch);
	ch->lines = 0;
	return;
    }

    if (lines < 10 || lines > 100) {
	send_to_char("You must provide a reasonable number.\n\r", ch);
	return;
    }

    sprintf(buf, "Scroll set to %d lines.\n\r", lines);
    send_to_char(buf, ch);
    ch->lines = lines - 2;
}


void do_socials(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int iSocial;
    int col;

    col = 0;
    for (iSocial = 0; social_table[iSocial].name[0] != '\0'; iSocial++)
    {
	sprintf(buf, "%-12s", social_table[iSocial].name);
	send_to_char(buf, ch);
	if (++col % 6 == 0)
	    send_to_char("\n\r", ch);
    }

    if (col % 6 != 0)
	send_to_char("\n\r", ch);
}


void do_motd(CHAR_DATA *ch, char *argument)
{
    HELP_DATA *help;

    if ((help = lookup_help_index(motd, ch->tot_level, topHelpCat)) != NULL)
	send_to_char(help->text, ch);
}


void do_imotd(CHAR_DATA *ch, char *argument)
{
    HELP_DATA *help;

    if ((help = lookup_help_index(imotd, ch->tot_level, topHelpCat)) != NULL)
	send_to_char(help->text, ch);
}


void do_rules(CHAR_DATA *ch, char *argument)
{
    HELP_DATA *help;

    if ((help = lookup_help_index(rules, ch->tot_level, topHelpCat)) != NULL)
	send_to_char(help->text, ch);
}


void do_wizlist(CHAR_DATA *ch, char *argument)
{
	IMMORTAL_DATA *immortal;
	char buf[MSL], duties[MSL/10];
	int years;

	send_to_char("\n\r{b.,-{B-^--,._.,{C[ {WThe Immortals of Sentience {C]{B-.._.,--^-{b-,.{x\n\r", ch);
	send_to_char("{B``{x\n\r", ch);
	for (immortal = immortal_list; immortal != NULL; immortal = immortal->next) {
		years = ((long)current_time - immortal->created)/31556926;

		/* Hack to fix strange bug with duty commas */
/*		if (duties != 0)*/
			snprintf(duties, MSL / 10 - 1, "%s", flag_string_commas(immortal_flags, immortal->duties));
			duties[MSL / 10 - 1] = '\0';

		sprintf(buf, "{B`` {W%11s {B-{x %s{B({Y%d{B){x\n\r",
		immortal->name, immortal->duties == 0 ? "None" : duties + 1, years );

		send_to_char(buf, ch);
	}
	send_to_char("{b``\n\r", ch);
	send_to_char("{b``--{B-^--,._.,-.._.,--^-------------^--,._.,-.._.,--^-{b-,.{x\n\r", ch);
}


void do_prompt(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (argument[0] == '\0') {
	if (IS_SET(ch->comm, COMM_PROMPT)) {
	    send_to_char("You will no longer see prompts.\n\r", ch);
	    REMOVE_BIT(ch->comm, COMM_PROMPT);
	} else {
	    send_to_char("You will now see prompts.\n\r", ch);
	    SET_BIT(ch->comm, COMM_PROMPT);
	}
	return;
    }

    if (!strcmp(argument, "all"))
	strcpy(buf, "{B<%h{Bhp %m{Bm %v{Bmv>{x ");
    else {
	//if (strlen(argument) > 50)
	//    argument[50] = '\0';
	if (strlen_no_colours(argument) > 50)
	{
		send_to_char("That prompt is too long. Must be no more than 50 characters, not counting colour codes.\n\r",ch);
		return;
	}
	strcpy(buf, argument);
	smash_tilde(buf);
	if (str_suffix("%c", buf))
	    strcat(buf, " ");

    }

    free_string(ch->prompt);
    ch->prompt = str_dup(buf);
    sprintf(buf, "Prompt set to %s\n\r", ch->prompt);
    send_to_char(buf, ch);
    return;
}

void do_survey(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship;
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    ship = get_room_ship(ch->in_room);
    if( IS_VALID(ship) )
    {
		if( str_cmp(arg, "auto") )
		{
			if (ship->ship_type != SHIP_AIR_SHIP)
				act("You survey the area around the boat.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				act("You survey the area around the airship.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

		long bonus_view;
		if( IS_SET(ch->in_room->room_flag[0], ROOM_VIEWWILDS) )
			bonus_view = 8;
		else
			bonus_view = 2;

		long x = get_squares_to_show_x(bonus_view * 3) + ch->wildview_bonus_x;
		long y = get_squares_to_show_y(2 * bonus_view / 3) + ch->wildview_bonus_y;

		if( !IS_WILDERNESS(ship->ship->in_room) )
		{
			AREA_DATA *area = ship->ship->in_room->area;
		    act("$p has landed in $T.", ch, NULL, NULL, ship->ship, NULL, NULL, area->name, TO_CHAR);
		    return;
		}

		show_map_to_char_wyx(ship->ship->in_room->wilds, ship->ship->in_room->x, ship->ship->in_room->y, ch, ship->ship->in_room->x, ship->ship->in_room->y, x, y, FALSE);

		// Display sailing vessels in the same room as the ship
		return;
	}

#if 0
/*
    if (ON_SHIP(ch))
    {
	if (str_cmp(arg, "auto"))
	{
	    if (ch->in_room->ship->ship_type != SHIP_AIR_SHIP)
		act("You survey the area around the boat.", ch, NULL, NULL, TO_CHAR);
	    else
		act("You survey the area around the airship.", ch, NULL, NULL, TO_CHAR);
	}
	if (IN_SHIP_NEST(ch))
	    bonus_view = 8;
	else
	    bonus_view = 2;

	x = get_squares_to_show_x(bonus_view);
	y = get_squares_to_show_y(bonus_view);

	orig_ship = ch->in_room->ship;

	 For the airship
	area = orig_ship->ship->in_room->area;
	if (str_cmp(area->name, "Wilderness"))
	{
	    act("$p has landed in $T.", ch, orig_ship->ship, area->name, TO_CHAR);
	    return;
	}

	orig = ch->in_room;
	char_from_room(ch);
	char_to_room(ch, orig->ship->ship->in_room);

	show_map_to_char(ch, ch, bonus_view * 3+ch->wildview_bonus_x, bonus_view * 2/3+ch->wildview_bonus_y, FALSE);

	 Check for sailing ships
	for (ship = ((AREA_DATA *) get_sailing_boat_area())->ship_list;
	     ship != NULL; ship = ship->next)
	{
	    if (orig_ship != ship
		&& (ship->ship->in_room->x < ch->in_room->x + x &&
		    ship->ship->in_room->x > ch->in_room->x - x)
		&& (ship->ship->in_room->y < ch->in_room->y + y &&
		    ship->ship->in_room->y > ch->in_room->y - y)) {

		if (ship->ship->in_room->y == ch->in_room->y &&
		    ship->ship->in_room->x == ch->in_room->x)
		{
			if (ship->scuttle_time <= 0)
			{
				sprintf(buf,
					"{MWe are right next to a '%s' named '%s' flying the flag,'%s'.{x\n\r",
					boat_table[ship->ship_type].name, ship->ship_name, ship->flag);
			}
			else
			{
				sprintf(buf,
					"{RA %s named '%s', flying the flag, '%s', burns brightly as flames engulf the vessel!{x\n\r", plashes about as it bops up and down with the waves.{x\n\r",
					boat_table[ship->ship_type].name, ship->ship_name, ship->flag);
			}
			send_to_char(buf, ch);
			continue;
		}

		if (ship->scuttle_time <= 0)
		{
			sprintf(buf, "{MThe %s '%s', flying the flag '%s' is %s to the ", boat_table[ship->ship_type].name, ship->ship_name, ship->flag, ship->speed != SHIP_SPEED_STOPPED ? "sailing" : "anchored");
			send_to_char(buf, ch);

			if (ship->ship->in_room->y < ch->in_room->y)
				send_to_char("North", ch);
			else if (ship->ship->in_room->y > ch->in_room->y)
				send_to_char("South", ch);

			if (ship->ship->in_room->x > ch->in_room->x)
				send_to_char("East", ch);
			else if (ship->ship->in_room->x < ch->in_room->x)
				send_to_char("West", ch);

			send_to_char(".{x\n\r", ch);
		}
		else
		{
			sprintf(buf, "{RThe %s '%s', flying the flag '%s' burns brightly to the ", boat_table[ship->ship_type].name, ship->ship_name, ship->flag);
			send_to_char(buf, ch);
			if (ship->ship->in_room->y < ch->in_room->y)
				send_to_char("North", ch);
			else if (ship->ship->in_room->y > ch->in_room->y)
				send_to_char("South", ch);

			if (ship->ship->in_room->x > ch->in_room->x)
				send_to_char("East", ch);
			else if (ship->ship->in_room->x < ch->in_room->x)
				send_to_char("West", ch);

			send_to_char(".{x\n\r", ch);
		}
	    }
	}

	char_from_room(ch);
	char_to_room(ch, orig);
	return;
    }
*/
#endif

    if (IN_WILDERNESS(ch))
    {
	int chance;
	chance = get_skill(ch, gsk_survey);
        if (chance == 0)
        {
	    send_to_char("You are unsure of your exact coordinates.\n\r", ch);
	    return;
        }

                if (number_percent() < chance)
                {
                sprintf(buf, "{YFrom the surrounding landmarks you determine you are %ld south, %ld east.{x\n\r", ch->in_room->y, ch->in_room->x);
                send_to_char(buf, ch);
                return;
                }
                else
                {
                sprintf(buf, "{YFrom the surrounding landmarks you determine you are %ld south, %ld east.{x\n\r", ch->in_room->y + number_range(1, 30) - number_range(1,30), ch->in_room->x + number_range(1,30) - number_range(1, 30));
                send_to_char(buf, ch);
                return;
                }

    }

    act("You aren't on a boat.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
}

void show_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool remote, bool silent, bool automatic)
{
	char buf[MAX_STRING_LENGTH];
	EXIT_DATA *pexit;
	int count;
	int linelength;

	if(ch->desc == NULL) return;

	linelength = 0;

	// Check for a MIST with obscure_room
	if (remote)
	{
		OBJ_DATA *mist = NULL;
		int max = 0;

		for(OBJ_DATA *obj = room->contents; obj; obj = obj->next_content)
		{
			if (IS_MIST(obj) && MIST(obj)->obscure_room > max)
			{
				mist = obj;
				max = MIST(obj)->obscure_room;
			}
		}

		if (mist != NULL && max > number_percent())
		{
			send_to_char(mist->description, ch);
			return;
		}
	}


	if (IS_IMMORTAL(ch) && (IS_NPC(ch) || IS_SET(ch->act[0], PLR_HOLYLIGHT))) {
		if (IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) {
			if(room->wilds) {
				sprintf (buf, "\n\r{C [ Area: %ld '%s', Wilds uid: %ld '%s', Vroom (%ld, %ld) ]{x",
					room->area->uid, room->area->name,
					room->wilds->uid, room->wilds->name,
					room->x, room->y);
			} else if(room->source) {
				sprintf (buf, "\n\r{C [ Area: %ld '%s', Clone (%ld#%ld, %ld, %ld) ]{x",
					room->area->uid, room->area->name,
					room->source->area->uid, room->source->vnum,room->id[0],room->id[1]);
			} else {
				sprintf(buf, "{g[Room %ld#%ld]", room->area->uid, room->vnum);
			}
		} else {
			sprintf(buf, "{G[Room %ld#%ld]", room->area->uid, room->vnum);
		}

		if( room->persist )
			strcat(buf, " {WPERSIST{x");

		send_to_char(buf, ch);

		if( IS_VALID(room->instance_section) )
		{
			if (IS_VALID(room->instance_section->instance))
			{
				// in an instance
				INSTANCE *instance = room->instance_section->instance;

				// What kind of instance?
				if (IS_VALID(instance->ship))
				{
					// On a ship
					sprintf(buf, " {c[{CShip {W'{x%s{W' %ld{c:{W%ld %ld{c:{W%ld{c]{x", instance->ship->ship_name, instance->ship->id[0], instance->ship->id[1], instance->uid[0], instance->uid[1]);
				}
				else if (IS_VALID(instance->dungeon))
				{
					// In a dungeon
					sprintf(buf, " {r[{RDungeon {W'%s{W' %ld{r:{W%ld %ld{r:{W%ld{c]{x", instance->dungeon->index->name, instance->dungeon->uid[0], instance->dungeon->uid[1], instance->uid[0], instance->uid[1]);
				}
				// TODO: In a duty
				else if (IS_VALID(instance->object))
				{
					// In an object
					sprintf(buf, " {y[{YObject {W'%s{W' %ld{y:{W%ld %ld{y:{W%ld{y]{x", instance->object->short_descr, instance->object->id[0], instance->object->id[1], instance->uid[0], instance->uid[1]);
				}
				else
				{
					// In a plain instance
					sprintf(buf, " {g[{GInstance {W'%s{W' %ld{g:{W%ld{g]{x", instance->blueprint->name, instance->uid[0], instance->uid[1]);
				}

				send_to_char(buf, ch);
			}
		}

	}

	linelength = strlen(room->name);
	linelength = 50 - linelength;

	if (IS_SET(room->room_flag[0], ROOM_SAFE))
		sprintf(buf, "\n\r {W%s", room->name);
	else if (IS_SET(room->room_flag[0], ROOM_UNDERWATER))
		sprintf(buf, "\n\r {C%s", room->name);
	else
		sprintf(buf, "\n\r {Y%s", room->name);

	send_to_char(buf, ch);

	if (IS_SET(room->room_flag[0], ROOM_PK) && IS_SET(room->room_flag[0], ROOM_CHAOTIC)) {
		sprintf(buf, "  {M[CPK ROOM]");
		send_to_char(buf, ch);
		linelength -= 13;
	} else if (IS_SET(room->room_flag[0], ROOM_CHAOTIC)) {
		sprintf(buf, "  {M[CHAOTIC]");
		send_to_char(buf, ch);
		linelength -= 12;
	} else if (IS_SET(room->room_flag[0], ROOM_PK)) {
		sprintf(buf, "  {R[NPK ROOM]");
		send_to_char(buf, ch);
		linelength -= 12;
	}

	if (IS_SET(room->room_flag[1], ROOM_MULTIPLAY)) {
		sprintf(buf, "  {W[FREE FOR ALL]");
		send_to_char(buf, ch);
		linelength -= 16;
	}

	if (room->ship != NULL) {
		if (room->ship->scuttle_time > 0) {
			sprintf(buf, "  {R[SCUTTLED]");
			send_to_char(buf, ch);
			linelength -= 12;
		}
	}

	if (IS_SET(room->room_flag[0], ROOM_HOUSE_UNSOLD)) {
		sprintf(buf, "  {R[PRIME REAL ESTATE]");
		send_to_char(buf, ch);
		linelength -= 21;
	}

	for (count = 0; count < linelength; count++)
	send_to_char(" ", ch);

	pexit = room->exit[DIR_NORTHWEST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_NORTHWEST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b     ", ch);
			else
				send_to_char("{M#{b     ", ch);
		}
		else
			send_to_char("{YNW{b    ", ch);
	} else
		send_to_char("{b-     ", ch);


	pexit = room->exit[DIR_NORTH];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_NORTH))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b", ch);
			else
				send_to_char("{M#{b", ch);
		}
		else
			send_to_char("{YN{b", ch);
	} else
		send_to_char("{b-", ch);

	pexit = room->exit[DIR_NORTHEAST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_NORTHEAST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("     {W#{x\n\r", ch);
			else
				send_to_char("     {M#{x\n\r", ch);
		}
		else
			send_to_char("    {YNE{x\n\r", ch);
	} else
		send_to_char("     {b-{x\n\r", ch);


	send_to_char ("{B({b-----------------------------------------------{B){b  ", ch);

	pexit = room->exit[DIR_WEST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_WEST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{B<{b-", ch);
			else
				send_to_char("{M#{B<{b-", ch);
		}
		else
			send_to_char("{YW{B<{b-", ch);
	} else
		send_to_char("-{B<{b-", ch);

	pexit = room->exit[DIR_UP];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_UP))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b-{B({WA{B){b-", ch);
			else
				send_to_char("{M#{b-{B({WA{B){b-", ch);
		}
		else
			send_to_char("{YU{b-{B({WA{B){b-", ch);
	} else
		send_to_char("--{B({WA{B){b-", ch);

	pexit = room->exit[DIR_DOWN];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_DOWN))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b-{B>{b", ch);
			else
				send_to_char("{M#{b-{B>{b", ch);
		}
		else
			send_to_char("{YD{b-{B>{b", ch);
	} else
		send_to_char("--{B>{b", ch);

	pexit = room->exit[DIR_EAST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_EAST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{x\n\r", ch);
			else
				send_to_char("{M#{x\n\r", ch);
		}
		else
			send_to_char("{YE{b\n\r", ch);
	} else
		send_to_char("-\n\r", ch);

	for (count = 0; count < 51; count++)
		send_to_char(" ", ch);

	pexit = room->exit[DIR_SOUTHWEST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_SOUTHWEST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b     ", ch);
			else
				send_to_char("{M#{b     ", ch);
		}
		else
			send_to_char("{YSW{b    ", ch);
	} else
		send_to_char("-     ", ch);

	pexit = room->exit[DIR_SOUTH];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_SOUTH))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("{W#{b", ch);
			else
				send_to_char("{M#{b", ch);
		}
		else
			send_to_char("{YS{b", ch);
	} else
		send_to_char("-{b", ch);

	pexit = room->exit[DIR_SOUTHEAST];
	if (pexit != NULL && !IS_SET(pexit->exit_info, EX_WALKTHROUGH) && is_exit_visible(ch, room, DIR_SOUTHEAST))
	{
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			if (IS_SET(pexit->exit_info, EX_TRANSPARENT))
				send_to_char("     {W#{x\n\r", ch);
			else
				send_to_char("     {M#{x\n\r", ch);
		}
		else
			send_to_char("    {YSE{x\n\r", ch);
	} else
		send_to_char("     -{x\n\r", ch);

	if (!automatic || ((!IS_NPC(ch) || IS_SWITCHED(ch)) && !IS_SET(ch->comm, COMM_BRIEF))) {
		if (IS_WILDERNESS(room) || IS_SET(room->room_flag[0], ROOM_VIEWWILDS)) {
			send_to_char("\n\r", ch);
		} else {
			if (room->chat_room != NULL) {
				send_to_char("  {YTopic:{x ", ch);

				sprintf(buf, "%s", room->chat_room->topic);
				send_to_char(buf, ch);

				send_to_char("\n\r\n\r", ch);
			}

			bool add_blank = FALSE;

			if (!remote && get_skill(ch, gsk_sense_danger) > 0 &&
				(!IS_NPC(ch) && ch->pcdata->danger_range > 0)) {
				int dir;
				char buf2[MSL];
				ROOM_INDEX_DATA *to_room;

				buf[0] = '\0';

				for (dir = 0; dir < MAX_DIR; dir++) {
					if (room->exit[dir] != NULL &&
						(to_room = room->exit[dir]->u1.to_room) != NULL) {
						if (IS_SET(to_room->room_flag[0], ROOM_PK) ||
							IS_SET(to_room->room_flag[0], ROOM_CHAOTIC) ||
							is_pk_safe_range(to_room, ch->pcdata->danger_range - 1, rev_dir[dir]) > -1) {
							if (buf[0] == '\0')
								sprintf(buf, "{RYou sense danger to the: %s", dir_name[dir]);
							else {
								sprintf(buf2,", %s", dir_name[dir]);
								strcat(buf, buf2);
							}
						}
					}
				}

				strcat(buf, "{x\n\r");
				send_to_char(buf,ch);

				add_blank = TRUE;

				if (number_percent() == 1)
					check_improve(ch, gsk_sense_danger, TRUE, 1);
			}

			if (!remote)
			{
				if (p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ROOM_HEADER, NULL, 0, 0, 0, 0, 0))
					add_blank = TRUE;
			}

			if (!remote && add_blank)
			{
				send_to_char("\n\r", ch);
			}

#if 1
			if (!IS_SET(ch->comm, COMM_NOMAP) && /*!ON_SHIP(ch) &&*/
				!IS_SET(room->room_flag[0], ROOM_NOMAP) &&
				!IS_SET(room->area->area_flags, AREA_NOMAP))
				show_map_and_description(ch, room);
			else {
#endif
				send_to_char("  ", ch);
				show_room_description(ch, room);
#if 1
			}
#endif
		}
	}

	if (!remote && (!IS_NPC(ch) || IS_SWITCHED(ch)) && IS_SET(ch->act[0], PLR_AUTOEXIT)) {
		send_to_char("\n\r", ch);
		do_exits(ch, "auto");
	}

	if (!remote)
	{
		p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ROOM_FOOTER, NULL, 0, 0, 0, 0, 0);
	}

	/* VIZZWILDS - Check if char is in a wilderness room, and if so display wilds map */
	if (room->wilds && ((automatic && ((!IS_NPC(ch) &&
		IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM) &&
		!IS_SET(ch->comm, COMM_BRIEF)))) ||
		(!automatic && !IS_NPC(ch) &&
		IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)))) {
		int vp_x, vp_y;

		vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
		vp_y = get_squares_to_show_y(ch->wildview_bonus_y);
		show_map_to_char_wyx(room->wilds, room->x, room->y, ch, room->x, room->y, vp_x, vp_y, FALSE);
	}

	if(!IS_NPC(ch) /*&& !IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)*/ &&
		IS_SET(room->room_flag[0], ROOM_VIEWWILDS) &&
		(!automatic || !IS_SET(ch->comm, COMM_BRIEF))) {
		int vp_x, vp_y;
		int x, y;
		WILDS_DATA *viewwilds = NULL;

		if( room->viewwilds )
		{
			viewwilds = room->viewwilds;
			x = room->x;
			y = room->y;
		}
		else if( IS_VALID(room->instance_section) &&
			IS_VALID(room->instance_section->instance) &&
			IS_VALID(room->instance_section->instance->ship) &&
			IS_VALID(room->instance_section->instance->ship->ship) &&
			room->instance_section->instance->ship->ship->in_room )
		{
			ROOM_INDEX_DATA *ship_room = room->instance_section->instance->ship->ship->in_room;
			viewwilds = ship_room->wilds;
			x = ship_room->x;
			y = ship_room->y;
		}

		if( viewwilds )
		{
			vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
			vp_y = get_squares_to_show_y(ch->wildview_bonus_y);
			show_map_to_char_wyx(viewwilds, x, y, ch, x, y, vp_x, vp_y, FALSE);
		}
	}


	/* Check for the reckoning */
	if (reckoning_timer > 0 && pre_reckoning == 0 && !IS_IMMORTAL(ch)) {
		if(remote)
			sprintf(buf, "     {MA heavy thick purple mist obscures the view.{x\n\r");
		else
			sprintf(buf, "     {MA heavy thick purple mist obscures what lies beneath you.{x\n\r");
		send_to_char(buf, ch);
	} else
		show_list_to_char(room->contents, ch, FALSE, FALSE);

	show_char_to_char(room->people, ch, NULL);
	return;
}

void do_look(CHAR_DATA * ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg1[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];
	EXIT_DATA *pexit;
	ROOM_INDEX_DATA *char_room;
	ROOM_INDEX_DATA *look_room = NULL;
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	EXTRA_DESCR_DATA *pdesc;
	int door;
	int number, count;
	int i;
	bool perform_lore = FALSE;

	if (ch->desc == NULL)
		return;

	if (ch->position < POS_SLEEPING)
	{
		send_to_char("You can't see anything but stars!\n\r", ch);
		return;
	}

	if (ch->position == POS_SLEEPING)
	{
		send_to_char("You can't see anything, you're sleeping!\n\r", ch);
		return;
	}

	if(!check_vision(ch,ch->in_room,true,true))
		return;

	argument = one_argument(argument, arg1);
	number = number_argument(arg1, arg3);
	count = 0;

	// 'look' or 'look auto'
	if (arg1[0] == '\0')
	{
		show_room(ch,ch->in_room,false,false,false);
		return;
	}

	if (!str_cmp(arg1, "auto"))
	{
		show_room(ch,ch->in_room,false,false,true);
		return;
	}

	if(arg1[0] == '.' || arg3[0] == '.')
	{
		send_to_char("You do not see that here.\n\r", ch);
		return;
	}

	// 'look in'
	if (!str_cmp(arg1, "i") || !str_cmp(arg1, "in") || !str_cmp(arg1, "on"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Look in what?\n\r", ch);
			return;
		}

		if ((obj = get_obj_here(ch, NULL, argument)) == NULL)
		{
			send_to_char("You do not see that here.\n\r", ch);
			return;
		}

		if (IS_CONTAINER(obj))
		{
			if (IS_SET(CONTAINER(obj)->flags, CONT_CLOSED) && !IS_SET(CONTAINER(obj)->flags, CONT_TRANSPARENT))
			{
				act("$p is closed.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			}
			else
			{
				act("$p holds:", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				show_list_to_char(obj->contains, ch, TRUE, TRUE);
			}
		}
		else if (IS_FLUID_CON(obj))
		{
			if (IS_SET(FLUID_CON(obj)->flags, CONT_CLOSED) && !IS_SET(FLUID_CON(obj)->flags, CONT_TRANSPARENT))
			{
				act("$p is closed.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			}
			else if (!IS_VALID(FLUID_CON(obj)->liquid))
			{
				send_to_char("It is empty.\n\r", ch);
			}
			else if (FLUID_CON(obj)->capacity > 0 && FLUID_CON(obj)->amount >= 0)
			{
				if (FLUID_CON(obj)->amount >= FLUID_CON(obj)->capacity)
					sprintf(buf, "It's filled with %s %s liquid.\n\r", get_article(FLUID_CON(obj)->liquid->color, false), FLUID_CON(obj)->liquid->color);
				else if (FLUID_CON(obj)->amount >= (3 * FLUID_CON(obj)->capacity / 4))
					sprintf(buf, "It's more than half-filled with %s %s liquid.\n\r", get_article(FLUID_CON(obj)->liquid->color, false), FLUID_CON(obj)->liquid->color);
				else if (FLUID_CON(obj)->amount > (FLUID_CON(obj)->capacity / 4))
					sprintf(buf, "It's about half-filled with %s %s liquid.\n\r", get_article(FLUID_CON(obj)->liquid->color, false), FLUID_CON(obj)->liquid->color);
				else
					sprintf(buf, "It's about less than half-filled with %s %s liquid.\n\r", get_article(FLUID_CON(obj)->liquid->color, false), FLUID_CON(obj)->liquid->color);
				send_to_char(buf, ch);
			}
		}
		else
		{
			switch (obj->item_type)
			{
			default:
				send_to_char("That is not a container.\n\r", ch);
				break;

			case ITEM_CORPSE_NPC:
			case ITEM_CORPSE_PC:
				act("$p holds:", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				show_list_to_char(obj->contains, ch, TRUE, TRUE);
				break;
			}
		}
		return;
	}

	// look <person>[ <worn item>]
	if ((victim = get_char_room(ch, NULL, arg1)) != NULL)
	{
		if(argument[0])
		{
			number = number_argument(argument, arg3);
			count = 0;
			// look at an object in the inventory
			for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
				if (can_see_obj(ch, obj) && obj->wear_loc != WEAR_NONE && wear_params[obj->wear_loc][WEAR_PARAM_SEEN] &&
					is_name(arg3, obj->name) && (++count == number))
				{
					if (ch != victim)
					{
						if(can_see(victim, ch) && ch->invis_level < LEVEL_IMMORTAL)
						{
							act("$n looks at $p on you.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
							act("$n looks at $p on $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
						}
						act("{MYou take a look at {W$p{M on {W$N{M.{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					}
					send_to_char(obj->full_description, ch);
					send_to_char("\n\r", ch);
					return;
				}

			if (count > 0 && count != number)
			{
				if (count == 1)
					sprintf(buf, "You only see one %s here.\n\r", arg3);
				else
					sprintf(buf, "You only see %d of those here.\n\r", count);

				send_to_char(buf, ch);
				return;
			}

			act("You don't see anything like that on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		} else
			show_char_to_char_1(victim, ch, false);
		return;
	}

	// TODO: hack for the crystal ball in Mordrakes tower
	/*
	if (!str_cmp(arg1, "at"))
	{
		for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
		{
			if (obj->pIndexData->vnum == 152533)
			{
				CHAR_DATA *victim;

				send_to_char("{MThe crystal ball sparks and splutters as an image appears.{x\n\r", ch);
				if ((victim = get_char_world(ch, argument)) == NULL ||
					(victim->in_room != NULL && IS_SET(victim->in_room->room_flag[0], ROOM_SAFE)))
				{
					send_to_char("{MThe image blurs and fades into nothing.{x\n\r", ch);
					return;
				}

				act("For a moment in time you see through the eyes of $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("You feel a momentary shiver up your spine as if you were being watched.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n peers into the crystal ball.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				//Updated from show_room_to_char to show_room. -- Tieryo 08/18/2010
				show_room(ch,victim->in_room,true,false,false);
				return;
			}
		}
	}
	*/

	/* look at an object in the inventory */
	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
		perform_lore = FALSE;
		if (can_see_obj(ch, obj))
		{
			/* Can person lore object */
			perform_lore = FALSE;
			if ((IS_NPC(ch) || !IS_SET(ch->act[1], PLR_NOLORE)) &&
				get_skill(ch, gsk_lore) > 0 &&
				number_percent() <= get_skill(ch, gsk_lore) &&
				((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) ||													// Immortal HOLYLIGHT
				!IS_SET(obj->extra[1], ITEM_NO_LORE) || 														// NO_LORE not set
				(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)) ||									// ALL_REMORT and this is a remort
				(IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && IS_REMORT(ch) && ch->tot_level > obj->level) ||		// REMORT_ONLY and this is a remort, check level
				(!IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && ch->tot_level > obj->level)))						// !REMORT_ONLY, check level
				perform_lore = TRUE;

			pdesc = get_extra_descr(arg3, obj->extra_descr);
			if (pdesc != NULL && pdesc->description != NULL)
			{
				if (++count == number)
				{
					send_to_char(pdesc->description, ch);

					if (perform_lore)
					{
						send_to_char ("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level,ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);
					return;
				}
				else
					continue;
			}

			pdesc = get_extra_descr(arg3, obj->pIndexData->extra_descr);
			if (pdesc != NULL && pdesc->description != NULL)
			{
				if (++count == number)
				{
					send_to_char(pdesc->description, ch);
					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level, ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);
					return;
				}
				else
					continue;
			}

			if (is_name(arg3, obj->name))
			{
				if (++count == number)
				{
					send_to_char(obj->full_description, ch);
					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level,ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);

					if( obj->item_type == ITEM_SEXTANT )
					{
						look_sextant(ch, obj);
					}
					else if(obj->item_type == ITEM_MAP)
					{
						look_map(ch, obj);
					}
					else if(obj->item_type == ITEM_TELESCOPE)
					{
						look_through_telescope(ch, obj, argument);
					}
					else if(obj->item_type == ITEM_COMPASS)
					{
						look_compass(ch, obj);
					}
					else if(IS_PORTAL(obj))
					{
						look_portal(ch, obj);
					}
					else if((obj->pIndexData == obj_index_skull || obj->pIndexData == obj_index_gold_skull) &&
						affect_find(obj->affected, gsk_third_eye) != NULL)
					{
						if ((victim = get_char_world(NULL, obj->owner)) != NULL)
						{
							if (victim == ch)
							{
								send_to_char("{RYou can just as easily use your own eyes.{x\n\r", ch);
								return;
							}

							if (!can_see_room(ch,victim->in_room) ||
								IS_SET(victim->in_room->room_flag[0], ROOM_NOVIEW) ||
								IS_SET(victim->in_room->room_flag[0], ROOM_PRIVATE) ||
								IS_SET(victim->in_room->room_flag[0], ROOM_SOLITARY))
							{
								send_to_char("{DAll you see is darkness.{x\n\r", ch);
								return;
							}

							act("{YYou look through the eyes of $N:{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

							//Updated show_room_to_char to show_room. -- Tieryo 08/18/2010
							show_room(ch,victim->in_room,true,false,false);
						}
						else
							act("{DThe soul of {x$T{D has left this world.", ch, NULL, NULL, NULL, NULL, NULL, obj->owner, TO_CHAR);
					}

					return;
				}
			}
		}
	}

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
	{
		if (can_see_obj(ch, obj))
		{
			/* Can person lore object */
			perform_lore = FALSE;
			if ((IS_NPC(ch) || !IS_SET(ch->act[1], PLR_NOLORE)) &&
				get_skill(ch, gsk_lore) > 0 &&
				number_percent() <= get_skill(ch, gsk_lore) &&
				((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) ||													// Immortal HOLYLIGHT
				!IS_SET(obj->extra[1], ITEM_NO_LORE) || 														// NO_LORE not set
				(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)) ||									// ALL_REMORT and this is a remort
				(IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && IS_REMORT(ch) && ch->tot_level > obj->level) ||		// REMORT_ONLY and this is a remort, check level
				(!IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && ch->tot_level > obj->level)))						// !REMORT_ONLY, check level

				perform_lore = TRUE;

			/* Check extra desc first */
			pdesc = get_extra_descr(arg3, obj->extra_descr);
			if (pdesc != NULL)
			{
				if (++count == number)
				{
					if( pdesc->description )
					{
						send_to_char(pdesc->description, ch);
					}
					else
					{
						ROOM_INDEX_DATA *environ = get_environment(ch->in_room);

						if( environ )
						{
							if( IS_WILDERNESS(environ) )
							{
								WILDS_TERRAIN *pTerrain = get_terrain_by_coors(environ->wilds, environ->x, environ->y);

								if (pTerrain != NULL)
								{
									int vp_x, vp_y;

									vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
									vp_y = get_squares_to_show_y(ch->wildview_bonus_y);

									if( !pTerrain->nonroom )
										show_vroom_header_to_char(pTerrain, environ->wilds, environ->x, environ->y, ch);
									show_map_to_char_wyx(environ->wilds, environ->x, environ->y, ch, environ->x, environ->y, vp_x, vp_y, FALSE);

									if (reckoning_timer > 0 && pre_reckoning == 0 && !IS_IMMORTAL(ch))
										send_to_char("     {MA heavy thick purple mist obscures the view.{x\n\r", ch);

								}
								else
								{
									send_to_char("Nothing special there.\n\r", ch);
									return;
								}
							}
							else
							{
								char_room = ch->in_room;
								ch->in_room = environ;
								do_function(ch, &do_look, "auto");
								ch->in_room = char_room;
							}
						}
						else
						{
							send_to_char("Nothing special there.\n\r", ch);
							return;
						}
					}

					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level, ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);
					return;
				}
			}

			pdesc = get_extra_descr(arg3, obj->pIndexData->extra_descr);
			if (pdesc != NULL)
			{
				if (++count == number)
				{
					if( pdesc->description )
					{
						send_to_char(pdesc->description, ch);
					}
					else
					{
						ROOM_INDEX_DATA *environ = get_environment(ch->in_room);

						if( environ )
						{
							if( IS_WILDERNESS(environ) )
							{
								WILDS_TERRAIN *pTerrain = get_terrain_by_coors(environ->wilds, environ->x, environ->y);

								if (pTerrain != NULL)
								{
									int vp_x, vp_y;

									vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
									vp_y = get_squares_to_show_y(ch->wildview_bonus_y);

									if( !pTerrain->nonroom )
										show_vroom_header_to_char(pTerrain, environ->wilds, environ->x, environ->y, ch);
									show_map_to_char_wyx(environ->wilds, environ->x, environ->y, ch, environ->x, environ->y, vp_x, vp_y, FALSE);

									if (reckoning_timer > 0 && pre_reckoning == 0 && !IS_IMMORTAL(ch))
										send_to_char("     {MA heavy thick purple mist obscures the view.{x\n\r", ch);

								}
								else
								{
									send_to_char("Nothing special there.\n\r", ch);
									return;
								}
							}
							else
							{
								char_room = ch->in_room;
								ch->in_room = environ;
								do_function(ch, &do_look, "auto");
								ch->in_room = char_room;
							}
						}
						else
						{
							send_to_char("Nothing special there.\n\r", ch);
							return;
						}
					}

					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level, ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);
					return;
				}
			}

			if (is_name(arg3, obj->name))
				if (++count == number)
				{
					send_to_char(obj->full_description, ch);
					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level, ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);

					if( obj->item_type == ITEM_TELESCOPE )
					{
						look_through_telescope(ch, obj, argument);
					}
					else if(obj->item_type == ITEM_COMPASS)
					{
						look_compass(ch, obj);
					}
					else if(obj->item_type == ITEM_MAP)
					{
						look_map(ch, obj);
					}
					else if(IS_PORTAL(obj))
					{
						look_portal(ch, obj);
					}
					return;
				}
		}
	}

	pdesc = get_extra_descr(arg3, ch->in_room->extra_descr);
	if (pdesc != NULL)
	{
		if (++count == number)
		{
			if( pdesc->description )
			{
				send_to_char(pdesc->description, ch);
			}
			else
			{
				ROOM_INDEX_DATA *environ = get_environment(ch->in_room);

				if( environ )
				{
					if( IS_WILDERNESS(environ) )
					{
						WILDS_TERRAIN *pTerrain = get_terrain_by_coors(environ->wilds, environ->x, environ->y);

						if (pTerrain != NULL)
						{
							int vp_x, vp_y;

							vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
							vp_y = get_squares_to_show_y(ch->wildview_bonus_y);

							if( !pTerrain->nonroom )
								show_vroom_header_to_char(pTerrain, environ->wilds, environ->x, environ->y, ch);
							show_map_to_char_wyx(environ->wilds, environ->x, environ->y, ch, environ->x, environ->y, vp_x, vp_y, FALSE);

							if (reckoning_timer > 0 && pre_reckoning == 0 && !IS_IMMORTAL(ch))
								send_to_char("     {MA heavy thick purple mist obscures the view.{x\n\r", ch);

						}
						else
						{
							send_to_char("Nothing special there.\n\r", ch);
						}
					}
					else
					{
						char_room = ch->in_room;
						ch->in_room = environ;
						do_function(ch, &do_look, "auto");
						ch->in_room = char_room;
					}
				}
				else
				{
					send_to_char("Nothing special there.\n\r", ch);
				}
			}
			return;
		}
	}

	if (count > 0 && count != number)
	{
		if (count == 1)
			sprintf(buf, "You only see one %s here.\n\r", arg3);
		else
			sprintf(buf, "You only see %d of those here.\n\r", count);

		send_to_char(buf, ch);
		return;
	}

	/* look <exit keyword> */
	for (i = 0; i < MAX_DIR; i++)
	{
		pexit = ch->in_room->exit[i];
		if (pexit != NULL &&
			pexit->u1.to_room != NULL &&
			(!IS_SET(pexit->exit_info, EX_HIDDEN) || IS_SET(pexit->exit_info, EX_FOUND)) &&		// Hidden exists should not be included in this search
			pexit->keyword != NULL &&
			pexit->keyword[0] != '\0' &&
			pexit->keyword[0] != ' ' &&
			pexit->keyword[0] != '.' &&
			!str_cmp(pexit->keyword, arg1) &&
			pexit->short_desc != NULL &&
			pexit->short_desc[0] != '\0')
		{
			send_to_char(pexit->short_desc, ch);
			return;
		}
	}

	/* look <direction> */
	if (!str_cmp(arg1, "n") || !str_cmp(arg1, "north"))
		door = DIR_NORTH;
	else if (!str_cmp(arg1, "e") || !str_cmp(arg1, "east"))
		door = DIR_EAST;
	else if (!str_cmp(arg1, "s") || !str_cmp(arg1, "south"))
		door = DIR_SOUTH;
	else if (!str_cmp(arg1, "w") || !str_cmp(arg1, "west"))
		door = DIR_WEST;
	else if (!str_cmp(arg1, "u") || !str_cmp(arg1, "up"))
		door = DIR_UP;
	else if (!str_cmp(arg1, "d") || !str_cmp(arg1, "down"))
		door = DIR_DOWN;
	else if (!str_cmp(arg1, "ne") || !str_cmp(arg1, "northeast"))
		door = DIR_NORTHEAST;
	else if (!str_cmp(arg1, "nw") || !str_cmp(arg1, "northwest"))
		door = DIR_NORTHWEST;
	else if (!str_cmp(arg1, "se") || !str_cmp(arg1, "southeast"))
		door = DIR_SOUTHEAST;
	else if (!str_cmp(arg1, "sw") || !str_cmp(arg1, "southwest"))
		door = DIR_SOUTHWEST;
	else {
		send_to_char("You do not see that here.\n\r", ch);
		return;
	}

	// LOOK_AT check, allows for conditional viewing through an exit.
	if (p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_LOOK_AT,0,0,0,0,0) > 0)
		return;

	// 'look direction'
	if ((pexit = ch->in_room->exit[door]) == NULL || pexit->u1.to_room == NULL)
	{
		// Check if this is a wilds room
		//  skip up and down directions
		if(ch->in_room->wilds && door != DIR_UP && door != DIR_DOWN)
		{
			WILDS_TERRAIN *pTerrain;
			int to_x = get_wilds_vroom_x_by_dir(ch->in_room->wilds, ch->in_room->x, ch->in_room->y, door);
			int to_y = get_wilds_vroom_y_by_dir(ch->in_room->wilds, ch->in_room->x, ch->in_room->y, door);

			look_room = get_wilds_vroom(ch->in_room->wilds, to_x, to_y);
			if( look_room == NULL )
			{
				pTerrain = get_terrain_by_coors(ch->in_room->wilds, to_x, to_y);

				if (pTerrain != NULL)
				{
					int vp_x, vp_y;

					vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
					vp_y = get_squares_to_show_y(ch->wildview_bonus_y);

					if( !pTerrain->nonroom )
						show_vroom_header_to_char(pTerrain, ch->in_room->wilds, to_x, to_y, ch);
					show_map_to_char_wyx(ch->in_room->wilds, to_x, to_y, ch, ch->in_room->x, ch->in_room->y, vp_x, vp_y, FALSE);

					if (reckoning_timer > 0 && pre_reckoning == 0 && !IS_IMMORTAL(ch))
						send_to_char("     {MA heavy thick purple mist obscures the view.{x\n\r", ch);

					return;
				}
			}
		}

		if( look_room == NULL )
		{
			send_to_char("Nothing special there.\n\r", ch);
			return;
		}
	}
	else
	{
		if (IS_SET(pexit->exit_info, EX_HIDDEN) && !IS_SET(pexit->exit_info, EX_FOUND))
		{
			send_to_char("Nothing special there.\n\r", ch);
			return;
		}

		if (IS_SET(pexit->exit_info, EX_CLOSED) && !IS_SET(pexit->exit_info, EX_TRANSPARENT))
		{
			if (pexit->keyword != NULL && pexit->keyword[0] != '\0' && pexit->keyword[0] != ' ')
			{
				act("You can't see past the $d.", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_CHAR);
				return;
			}
			else
			{
				send_to_char("You fail to see further in that direction.\n\r", ch);
				return;
			}
		}

		look_room = pexit->u1.to_room;
	}

	// Changed looking through an exit to use show_room
	show_room(ch, look_room, true, false, true);
}


void do_examine(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    int number, count;
    CHAR_DATA *victim;
    OBJ_DATA *obj;
	bool perform_lore = FALSE;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0')
    {
		send_to_char("Examine what?\n\r", ch);
		return;
    }

	if(!check_vision(ch,ch->in_room,true,true))
		return;

	// look <person>[ <worn item>]
	if ((victim = get_char_room(ch, NULL, arg1)) != NULL)
	{
		if(argument[0])
		{
			number = number_argument(argument, arg3);
			count = 0;
			// look at an object in the inventory
			for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
				if (can_see_obj(ch, obj) && obj->wear_loc != WEAR_NONE && wear_params[obj->wear_loc][WEAR_PARAM_SEEN] &&
					is_name(arg3, obj->name) && (++count == number))
				{
					if (p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_EXAMINE, argument,0,0,0,0,0)) return;

					if (get_skill(ch, gsk_lore) > 0 &&
						number_percent() <= get_skill(ch, gsk_lore) &&
						((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) ||													// Immortal HOLYLIGHT
						!IS_SET(obj->extra[1], ITEM_NO_LORE) || 														// NO_LORE not set
						(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)) ||									// ALL_REMORT and this is a remort
						(IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && IS_REMORT(ch) && ch->tot_level > obj->level) ||		// REMORT_ONLY and this is a remort, check level
						(!IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && ch->tot_level > obj->level)))						// !REMORT_ONLY, check level
						perform_lore = TRUE;

					if (ch != victim)
					{

						if(can_see(victim, ch) && ch->invis_level < LEVEL_IMMORTAL)
						{
							act("$n examines $p on you.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
							act("$n examines $p on $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
						}
						act("{MYou examine {W$p{M on {W$N{M.{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					}
					send_to_char(obj->full_description, ch);
					if (perform_lore)
					{
						send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
						spell_identify(gsk_lore, ch->tot_level,ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
					}
					else
						send_to_char("\n\r", ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
					check_improve(ch, gsk_lore, TRUE, 10);
					return;
				}

			if (count > 0 && count != number)
			{
				if (count == 1)
					sprintf(buf, "You only see one %s here.\n\r", arg3);
				else
					sprintf(buf, "You only see %d of those here.\n\r", count);

				send_to_char(buf, ch);
				return;
			}

			act("You don't see anything like that on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}
		else
		{
			if (p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_EXAMINE, argument,0,0,0,0,0)) return;

			show_char_to_char_1(victim, ch, true);
		}
		return;
	}

    if ((obj = get_obj_here(ch, NULL, arg1)) != NULL)
    {
		if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_EXAMINE,argument,0,0,0,0,0)) return;

		if (get_skill(ch, gsk_lore) > 0 &&
			number_percent() <= get_skill(ch, gsk_lore) &&
			((!IS_NPC(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT)) ||													// Immortal HOLYLIGHT
			!IS_SET(obj->extra[1], ITEM_NO_LORE) || 														// NO_LORE not set
			(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)) ||									// ALL_REMORT and this is a remort
			(IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && IS_REMORT(ch) && ch->tot_level > obj->level) ||		// REMORT_ONLY and this is a remort, check level
			(!IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && ch->tot_level > obj->level)))						// !REMORT_ONLY, check level
			perform_lore = TRUE;

		send_to_char(obj->full_description, ch);
		if (perform_lore)
		{
			send_to_char("\n\r{YFrom your studies you can conclude the following information: {X\n\r", ch);
			spell_identify(gsk_lore, ch->tot_level,ch, (void *) obj, TARGET_OBJ, WEAR_NONE);
		}
		else
			send_to_char("\n\r", ch);

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE_EX, NULL,0,0,0,0,0);
		check_improve(ch, gsk_lore, TRUE, 10);
    }

    return;
}


void do_exits(CHAR_DATA * ch, char *argument)
{
	extern char *const dir_name[];
	char buf[MAX_STRING_LENGTH];
	ROOM_INDEX_DATA *to_room;
	WILDS_DATA *in_wilds = NULL;
	WILDS_DATA *to_wilds = NULL;
	WILDS_TERRAIN *pTerrain;
	EXIT_DATA *pexit;
	bool found;
	bool fAuto;
	int door;
	int to_vroom_x = 0;
	int to_vroom_y = 0;

	fAuto = !str_cmp(argument, "auto");

	if (!check_blind(ch)) {
		send_to_char("{DYou can't see a thing!\n\r{x", ch);
		return;
	}


	if (fAuto)
		sprintf(buf, "[{WExits{x:");
	else if (IS_IMMORTAL(ch)) {
		if (ch->in_wilds)
			sprintf(buf, "{YObvious exits from room at (%ld, %ld):{x\n\r", ch->in_room->x, ch->in_room->y);
		else
			sprintf(buf, "{YObvious exits from room %ld:{x\n\r", ch->in_room->vnum);
	} else
		sprintf(buf, "{YObvious exits:{x\n\r");

	found = FALSE;

	if (ch->in_room->wilds) {
		for(door = 0; door < MAX_DIR; door++) if((pexit = ch->in_room->exit[door])) {
			if (IS_SET(pexit->exit_info, EX_VLINK)) {
				/* This is a vlink to different wilderness location, be it on the same map or not */
				if (pexit->wilds.wilds_uid > 0) {
					to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
					to_vroom_x = pexit->wilds.x;
					to_vroom_y = pexit->wilds.y;

					if (!(pTerrain = get_terrain_by_coors(to_wilds, to_vroom_x, to_vroom_y)))
						continue;

					to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
					if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y)))
						continue;

				/* Otherwise, Exit leads to a static room. */
				} else if (!(to_room = pexit->u1.to_room))
					continue;

			} else {
				/* In wilds and exit leads to another vroom. */
				to_wilds = ch->in_room->wilds;
				to_vroom_x = get_wilds_vroom_x_by_dir(in_wilds, ch->in_room->x, ch->in_room->y, door);
				to_vroom_y = get_wilds_vroom_y_by_dir(in_wilds, ch->in_room->x, ch->in_room->y, door);

				if (!(pTerrain = get_terrain_by_coors(to_wilds, to_vroom_x, to_vroom_y)))
					continue;

				to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
				if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y)))
					continue;
			}

			if(!can_see_room (ch, to_room))
				continue;

			found = TRUE;

			if (fAuto) {
				strcat(buf, " ");
				strcat(buf, dir_name[door]);
			} else {
				sprintf(buf + strlen(buf), "%-5s - %s",
					capitalize(dir_name[door]),
					room_is_dark(to_room) ? "{DToo dark to tell{x" : to_room->name);

				if (IS_IMMORTAL(ch)) {
					if(to_room->wilds)
						sprintf(buf + strlen(buf), " ({Gwilds (%lu, %lu, %lu){x)\n\r", to_room->wilds->uid, to_room->x, to_room->y);
					else
						sprintf(buf + strlen(buf), " ({Groom %ld{x)\n\r", to_room->vnum);
				} else
					sprintf(buf + strlen(buf), "\n\r");
			}
		}
	} else {
		for(door = 0; door < MAX_DIR; door++) if((pexit = ch->in_room->exit[door])) {
			if (IS_SET(pexit->exit_info, EX_VLINK)) {
				/* This is a vlink to different wilderness location, be it on the same map or not */
				if (pexit->wilds.wilds_uid > 0) {
					to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
					to_vroom_x = pexit->wilds.x;
					to_vroom_y = pexit->wilds.y;

					if (!(pTerrain = get_terrain_by_coors(to_wilds, to_vroom_x, to_vroom_y)))
						continue;

					to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
					if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y)))
						continue;

				/* Otherwise, Exit leads to a static room. */
				} else if (!(to_room = pexit->u1.to_room))
					continue;
			} else if (IS_SET(pexit->exit_info, EX_ENVIRONMENT)) {
				if(!IS_SET(ch->in_room->room_flag[1],ROOM_VIRTUAL_ROOM))
					continue;

				found = TRUE;

				if (fAuto) {
					strcat(buf, " ");
					strcat(buf, dir_name[door]);
				} else
					sprintf(buf + strlen(buf), "%-5s - Environment\n\r", capitalize(dir_name[door]));

				continue;
			} else if (!(to_room = pexit->u1.to_room)) continue;

			if(!can_see_room (ch, to_room))
				continue;

			if(!is_exit_visible(ch, ch->in_room, door)) continue;

			found = TRUE;

			if (fAuto) {
				strcat(buf, " ");
				strcat(buf, dir_name[door]);
			} else {
				sprintf(buf + strlen(buf), "%-5s - %s",
					capitalize(dir_name[door]),
					room_is_dark(to_room) ? "{DToo dark to tell{x" : to_room->name);

				if (IS_IMMORTAL(ch)) {
					if(to_room->wilds)
						sprintf(buf + strlen(buf), " ({Gwilds (%lu, %lu, %lu){x)\n\r", to_room->wilds->uid, to_room->x, to_room->y);
					else
						sprintf(buf + strlen(buf), " ({Groom %ld{x)\n\r", to_room->vnum);
				} else
					sprintf(buf + strlen(buf), "\n\r");
			}
		}
	}

	if (!found)
		strcat(buf, fAuto ? " none" : "None.\n\r");

	if (fAuto)
		strcat(buf, "]\n\r");

	send_to_char(buf, ch);
	return;
}

void do_worth(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (IS_SWITCHED(ch)) {
	send_to_char("You cannot do this while morphed.\n\r", ch);
    }

    if (IS_NPC(ch)) {
	sprintf(buf, "You have {Y%ld {xgold and {Y%ld {xsilver.\n\r",
		ch->gold, ch->silver);
	send_to_char(buf, ch);
	sprintf(buf, "You have {Y%ld {xgold in your bank account.\n\r",
		ch->pcdata->bankbalance);
	return;
    }

    sprintf(buf,
	    "You have {Y%ld {xgold and {Y%ld {xsilver.\n\r",
	    ch->gold, ch->silver);

    send_to_char(buf, ch);

    sprintf(buf, "You have {Y%ld{X gold in your bank account.\n\r",
	    ch->pcdata->bankbalance);
    send_to_char(buf, ch);

    return;
}

void do_score(CHAR_DATA * ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH], buf2[MSL];
    char subclass[MIL];
    int i;
    char tbuf[MAX_STRING_LENGTH];
    int pierce_s;
    int bash_s;
    int slash_s;
    int exotic_s;

    if (IS_NPC(ch))
	return;

    if (IS_SWITCHED(ch))
    {
	send_to_char(
        "You can't see your stats while not in your normal form.\n\r", ch);
	return;
    }

    /* LINE 1 *** */
    sprintf(buf, "\n\r{C|");
    for (i = 0; i < 37; i++)
	strcat(buf, "-+");

    strcat(buf, "|\n\r");
    send_to_char(buf, ch);

	if (IS_VALID(ch->pcdata->current_class))
	{
		strcpy(subclass, ch->pcdata->current_class->clazz->display[ch->sex]);
	    subclass[0] = LOWER(subclass[0]);
	}
	else
		strcpy(subclass, "adventurer");

	if (!IS_NPC(ch) && ch->pcdata->title[0])
	{
		sprintf(buf2, string_replace_static(ch->pcdata->title, "$n", "{+%s"), ch->name);
	}
	else
	{
		strcpy(buf2, ch->name);
	}

    /* LINE 2 *** */
    sprintf(buf, "| {G%s {B[{x%s{B] [{x%s{B] [{x%s{B]{x",
	    buf2,
	    ch->sex == SEX_NEUTRAL ? "sexless" : ch->sex == SEX_MALE ? "male" : "female",
		IS_VALID(ch->race) ? ch->race->name : "unknown",
	    IS_NPC(ch) ? "mobile" : (IS_VALID(ch->pcdata->current_class) ? ch->pcdata->current_class->clazz->name : "unknown"));

    for (i = fstr_len(buf); i < 75; i++)
	strcat(buf, " ");

    strcat(buf, "{C|\n\r");
    send_to_char(buf, ch);
    /* sprintf(tbuf, "BUF IS: %d\n\r", strlen(buf));
       send_to_char(tbuf, ch); */

    /* LINE 3 *** */
    sprintf(buf, "{C|");
    for (i = 0; i < 37; i++)
    {
	strcat(buf, "-+");
    }
    strcat(buf, "|\n\r");
    send_to_char(buf, ch);

    /* LINE 4 *** */

    sprintf(buf, "| {BHP:   {x%ld{B/{x%ld", ch->hit, ch->max_hit);

    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BStr: {x%d{B/{x%d",
	    get_curr_stat(ch, STAT_STR), ch->perm_stat[STAT_STR]);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BPracs: {x%d", ch->practice);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    send_to_char("{C|\n\r", ch);

    /* LINE 5 *** */
    sprintf(buf, "| {BMana: {x%ld{B/{x%ld", ch->mana, ch->max_mana);

    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BInt: {x%d{B/{x%d",
	    get_curr_stat(ch, STAT_INT), ch->perm_stat[STAT_INT]);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BTrains: {x%d", ch->train);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    send_to_char("{C|\n\r", ch);
    /* LINE 6 *** */

    sprintf(buf, "| {BMove: {x%ld{B/{x%ld", ch->move, ch->max_move);

    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BWis: {x%d{B/{x%d",
	    get_curr_stat(ch, STAT_WIS), ch->perm_stat[STAT_WIS]);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BLevel: {x%d", ch->level);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);


    send_to_char("{C|\n\r", ch);

    /* LINE 7 *** */
    if(IS_IMMORTAL(ch))
    	sprintf(buf, "| {BAge: {XAgeless{X");
    else
	    sprintf(buf, "| {BAge: {x%d", get_age(ch));
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BDex: {x%d{B/{x%d",
	    get_curr_stat(ch, STAT_DEX), ch->perm_stat[STAT_DEX]);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    if (!IS_NPC(ch) && ch->level < LEVEL_HERO)
    {
	sprintf(buf, "{BExp to Level: {x%ld",
		(exp_per_level(ch, NULL, ch->pcdata->points) - ch->exp));
	for (i = fstr_len(buf); i < 25; i++)
	    strcat(buf, " ");
	send_to_char(buf, ch);
    }
    else
    {
	for (i = 0; i < 25; i++)
	    send_to_char(" ", ch);
    }
    send_to_char("{C|\n\r", ch);

    /* LINE 8 *** */

    if(IS_IMMORTAL(ch))
	sprintf(buf, "| {BHrs: {XForever");
    else
	sprintf(buf, "| {BHrs: {x%d",
	    ((ch->played + (int) (current_time - ch->logon)) / 3600));

    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);


    sprintf(buf, "{BCon: {x%d{B/{x%d",
	    get_curr_stat(ch, STAT_CON), ch->perm_stat[STAT_CON]);
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);

    sprintf(buf, "{BAlign: {x%d", ch->alignment);
    strcat(buf, "{C(");
    if (ch->alignment > 900)
	strcat(buf, "{Wangelic");
    else if (ch->alignment > 700)
	strcat(buf, "{Csaintly");
    else if (ch->alignment > 350)
	strcat(buf, "{Bgood");
    else if (ch->alignment > 100)
	strcat(buf, "{bkind");
    else if (ch->alignment > -100)
	strcat(buf, "{Dneutral");
    else if (ch->alignment > -350)
	strcat(buf, "{ymean");
    else if (ch->alignment > -700)
	strcat(buf, "{Yevil");
    else if (ch->alignment > -900)
	strcat(buf, "{rdemonic");
    else
	strcat(buf, "{Rsatanic{X");

    strcat(buf, "{C)");
    for (i = fstr_len(buf); i < 25; i++)
	strcat(buf, " ");
    strcat(buf, "|\n\r");
    send_to_char(buf, ch);
    sprintf(buf, "{C| {BQuest Points: {w%-9d", ch->questpoints);
    send_to_char(buf, ch);
    sprintf(buf, "{BDeity Points: {w%-11ld", ch->deitypoints);
    send_to_char(buf, ch);
    sprintf(buf, "{BPneuma: {w%-16ld {C|\n\r", ch->pneuma);
    send_to_char(buf, ch);
    sprintf(buf2, "%d{B[{x+%d{B]", GET_HITROLL(ch), ch->hitroll);
    sprintf(buf, "{C| {BHitroll: {w%-20s", buf2);
    send_to_char(buf, ch);
    sprintf(buf2, "%d{B[{x+%d{B]", GET_DAMROLL(ch), ch->damroll);
    sprintf(buf, "{BDamroll: {w%-46s {C|\n\r", buf2);
    send_to_char(buf, ch);

    sprintf(buf, "{C| {BMonsters Killed: {w%-30ld ",
	    ch->monster_kills);
    send_to_char(buf, ch);
    sprintf(buf, "{BBank Balance: {w%-10ld {C|\n\r", ch->pcdata->bankbalance);
    send_to_char(buf, ch);
    sprintf(buf, "{C| {BDeaths: {w%-39d {BQuests Completed:{x %-7ld{C|\n\r",
		    ch->deaths, ch->pcdata->quests_completed);
    send_to_char(buf, ch);
    sprintf(buf, "{C| {BPK Wins       : {w%-31d {BCPK Wins:{x %-14d {C|\n\r",
	    ch->player_kills,
	    ch->cpk_kills);

    send_to_char(buf, ch);
    sprintf(buf, "{C| {BPK Losses     : {w%-31d {BCPK Losses:{x %-12d {C|\n\r",
	    ch->player_deaths,
	    ch->cpk_deaths);
    send_to_char(buf, ch);
    sprintf(buf, "{C| {BArena Battles Won : {w%-53d{C|\n\r",
	    ch->arena_kills);
    send_to_char(buf, ch);
    sprintf(buf, "{C| {BArena Battles Lost: {w%-53d{C|\n\r",
	    ch->arena_deaths);
      /* ch->pcdata->ship_quest_points[CONT_SERALIA]); */
    send_to_char(buf, ch);

    sprintf(buf, "{C| {BWars Won: {w%-62d {C|\n\r",
     ch->wars_won);/* , ch->pcdata->ship_quest_points[CONT_ATHEMIA]); */
    send_to_char(buf, ch);

    /* LINE 9 *** */
    pierce_s = GET_AC(ch, AC_PIERCE);
    bash_s = GET_AC(ch, AC_BASH);
    slash_s = GET_AC(ch, AC_SLASH);
    exotic_s = GET_AC(ch, AC_EXOTIC);

    /* PIERCE */
    sprintf(tbuf, "{x");
    if (pierce_s < -100)
	pierce_s = -100;

    while (pierce_s <= 100)
    {
	if (pierce_s > 80)
	    strcat(tbuf, "{b");
	else if (pierce_s > 50)
	    strcat(tbuf, "{B");
	else if (pierce_s > 10)
	    strcat(tbuf, "{Y");
	else if (pierce_s > -60)
	    strcat(tbuf, "{R");
	else
	    strcat(tbuf, "{W");

	strcat(tbuf, "*{x");
	pierce_s += 10;
    }
    sprintf(buf, "| {BPiercing {C%3d{B: {Y%-39s{x", GET_AC(ch, AC_PIERCE),
	    tbuf);
    for (i = fstr_len(buf); i < 75; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);
    send_to_char("{C|\n\r", ch);


    /* BASH */
    sprintf(tbuf,"{x");
    if (bash_s < -100)
	bash_s = -100;
    while (bash_s <= 100)
    {
	if (bash_s > 80)
	    strcat(tbuf, "{b");
	else if (bash_s > 50)
	    strcat(tbuf, "{B");
	else if (bash_s > 10)
	    strcat(tbuf, "{Y");
	else if (bash_s > -60)
	    strcat(tbuf, "{R");
	else
	    strcat(tbuf, "{W");

	strcat(tbuf, "*{x");
	bash_s += 10;
    }
    sprintf(buf, "| {BBashing  {C%3d{B: {Y%s{x", GET_AC(ch, AC_BASH),
	    tbuf);
    for (i = fstr_len(buf); i < 75; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);
    send_to_char("{C|\n\r", ch);

    /* SLASH */
    sprintf(tbuf,"{x");
    if (slash_s < -100)
	slash_s = -100;
    while (slash_s <= 100)
    {
	if (slash_s > 80)
	    strcat(tbuf, "{b");
	else if (slash_s > 50)
	    strcat(tbuf, "{B");
	else if (slash_s > 10)
	    strcat(tbuf, "{Y");
	else if (slash_s > -60)
	    strcat(tbuf, "{R");
	else
	    strcat(tbuf, "{W");

	strcat(tbuf, "*{x");
	slash_s += 10;
    }
    sprintf(buf, "| {BSlashing {C%3d{B: {Y%s{x", GET_AC(ch, AC_SLASH),
	    tbuf);
    for (i = fstr_len(buf); i < 75; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);
    send_to_char("{C|\n\r", ch);

    /* EXOTIC */
    sprintf(tbuf,"{x");
    if (exotic_s < -100)
	exotic_s = -100;
    while (exotic_s <= 100)
    {
	if (exotic_s > 80)
	    strcat(tbuf, "{b");
	else if (exotic_s > 50)
	    strcat(tbuf, "{B");
	else if (exotic_s > 10)
	    strcat(tbuf, "{Y");
	else if (exotic_s > -60)
	    strcat(tbuf, "{R");
	else
	    strcat(tbuf, "{W");

	strcat(tbuf, "*{x");
	exotic_s += 10;

    }

    sprintf(buf, "| {BExotic   {C%3d{B: {Y%s{x", GET_AC(ch, AC_EXOTIC),
	    tbuf);
    for (i = fstr_len(buf); i < 75; i++)
	strcat(buf, " ");
    send_to_char(buf, ch);
    send_to_char("{C|\n\r", ch);

    /* CLOSING LINE */
    sprintf(buf, "{C|");
    for (i = 0; i < 37; i++)
    {
	strcat(buf, "-+");
    }
    strcat(buf, "|\n\r");
    send_to_char(buf, ch);

    /* Show Subclasses */
    if (!IS_IMMORTAL(ch))
    {
		send_to_char("{xYou are proficient in the following classes:\n\r", ch);
		ITERATOR lit;
		CLASS_LEVEL *cl;
		iterator_start(&lit, ch->pcdata->classes);
		while((cl = (CLASS_LEVEL *)iterator_nextdata(&lit)))
		{
		    sprintf(buf, "{B[{x%s{B]{x", cl->clazz->display[ch->sex]);
		    send_to_char(buf, ch);
		}
		iterator_stop(&lit);

		send_to_char("\n\r", ch);
    }

    /* if (!IS_NPC(ch) && ch->tot_level >= LEVEL_IMMORTAL && ch->pcdata->immortal)
    {
	if (ch->pcdata->immortal->imm_title != NULL)
	    sprintf(buf, "{BYou hold the Immortal title(s) of {x%s{B.{x\n\r", ch->pcdata->imm_title);
	else
	    sprintf(buf, "{BYou have not been assigned an immortal title{x.\n\r");

	send_to_char(buf, ch);
    }*/

    if (ch->church != NULL && ch->church_member != NULL)
    {
	sprintf(buf, "{YYou hold the position of %s in",
			get_chrank(ch->church_member));
	send_to_char(buf, ch);

/*	switch (ch->church->size)
	{
	case CHURCH_SIZE_BAND:
	    send_to_char("Band", ch);
	    break;
	case CHURCH_SIZE_CULT:
	    send_to_char("Cult", ch);
	    break;
	case CHURCH_SIZE_ORDER:
	    send_to_char("Order", ch);
	    break;
	case CHURCH_SIZE_CHURCH:
	    send_to_char("Church", ch);
	    break;
	}
*/

	sprintf(buf, " %s, which is ", ch->church->name);
	send_to_char(buf, ch);

		switch (ch->church->size)
		{
		case CHURCH_SIZE_BAND:
		    send_to_char("a Band.{x\n\r", ch);
		    break;
		case CHURCH_SIZE_CULT:
		    send_to_char("a Cult.{x\n\r", ch);
		    break;
		case CHURCH_SIZE_ORDER:
		    send_to_char("an Order.{x\n\r", ch);
		    break;
		case CHURCH_SIZE_CHURCH:
		    send_to_char("a Church.{x\n\r", ch);
		    break;
	}

    }

    if (IS_SET(ch->act[0], PLR_HELPER))
	send_to_char("You are a helper.\n\r", ch);

    /*
     * Ship ranks
    if (ch->pcdata->rank[ CONT_SERALIA ] > NPC_SHIP_RANK_NONE)
    {
	    sprintf(buf, "{xIn {WSeralia{x you are {Y%s{x holding the rank of {Y%s{x.\n\r",
			    rating_table[ ch->pcdata->reputation[ CONT_SERALIA ] ].name,
          rating_table[get_player_reputation(ch->pcdata->reputation[CONT_SERALIA])].name,
			    rank_table[ ch->pcdata->rank[ CONT_SERALIA ] ].name);
	    send_to_char(buf, ch);
    }
    if (ch->pcdata->rank[ CONT_ATHEMIA ] > NPC_SHIP_RANK_NONE)
    {
	    sprintf(buf, "{xIn {WAthemia{x you are {Y%s{x holding the rank of {Y%s{x.\n\r",
			    rating_table[ ch->pcdata->reputation[ CONT_ATHEMIA ] ].name,
          rating_table[get_player_reputation(ch->pcdata->reputation[CONT_ATHEMIA])].name,
			    rank_table[ ch->pcdata->rank[ CONT_ATHEMIA ] ].name);
	    send_to_char(buf, ch);
    }
    if (ch->pcdata->rank[ CONT_PIRATE ] > NPC_SHIP_RANK_NONE)
    {
	    sprintf(buf, "{xAs a {RPirate{x you are {Y%s{x holding the rank of {Y%s{x.\n\r",
          rating_table[get_player_reputation(ch->pcdata->reputation[CONT_PIRATE])].name,
			    rating_table[ ch->pcdata->reputation[ CONT_PIRATE ] ].name,
			    rank_table[ ch->pcdata->rank[ CONT_PIRATE ] ].name);
	    send_to_char(buf, ch);
    }
     */


    /*
    if (IN_NATURAL_FORM(ch))
    {
        send_to_char("{RYou are currently in your natural form.{x\n\r", ch);
    }
    */

    if (IS_DEAD(ch))
    {
	int hrs;
        int mins;

	hrs = ch->time_left_death/60;
	mins = ch->time_left_death - 60 * hrs;
	/*
        if (IS_DEMON(ch))
        {
	    sprintf(buf, "{BYou will have restored enough energy to project again in {Y%d {Bminutes{x\n\r", ch->time_left / 2 + 1);
        }
        else
        if (IS_ANGEL(ch))
        {
	    sprintf(buf, "{BYou will have restored enough energy to project again in {Y%d {Bminutes{x\n\r", ch->time_left / 2 + 1);
	}
	*/
	send_to_char("{RYou are dead.{x\n\r", ch);

	if (hrs > 1)
	{
	    sprintf(buf,
	    "{BYou will be restored by the gods in {Y%d{B hours and {Y%d{B minute%s{x\n\r",
		hrs,
		mins,
		mins == 1 ? "{x" : "s");
	}
	else if (hrs == 1)
	{
	    sprintf(buf,
	    "{BYou will be restored by the gods in {Y1{B hour {Y%d{B minute%s{x\n\r",
		mins,
		mins == 1 ? "{x" : "s");
	}
	else
	{
	    sprintf(buf,
	    "{BYou will be restored by the gods in {Y%d{B minute%s{x\n\r",
		mins,
		mins == 1 ? "{x" : "s");
	}
	send_to_char(buf, ch);
    }

    if (ch->maze_time_left > 0)
    {
        send_to_char("{DYou are banished.{x\n\r", ch);
	sprintf(buf,
	"{BYou will be restored to the mortal realm in {Y%d {Bminutes{x.\n\r",
	    ch->maze_time_left / 2 + 1);
	send_to_char(buf, ch);
    }

    sprintf(buf,
  	    "{xYou are carrying %d/%d items with weight %ld/%d kg."
	    " (%ld kg in coins){x\n\r",
	    ch->carry_number, can_carry_n(ch),
	    get_carry_weight(ch), can_carry_w(ch), COIN_WEIGHT(ch));
    send_to_char(buf, ch);

    sprintf(buf, "Wimpy set to %d hit points.\n\r", ch->wimpy);
    send_to_char(buf, ch);

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
	send_to_char("You are drunk.\n\r", ch);
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_THIRST] == 0)
	send_to_char("You are thirsty.\n\r", ch);
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_HUNGER] == 0)
	send_to_char("You are hungry.\n\r", ch);

    switch (ch->position)
    {
    case POS_DEAD:
	send_to_char("You are DEAD!!\n\r", ch);
	break;
    case POS_MORTAL:
	send_to_char("You are mortally wounded.\n\r", ch);
	break;
    case POS_INCAP:
	send_to_char("You are incapacitated.\n\r", ch);
	break;
    case POS_STUNNED:
	send_to_char("You are stunned.\n\r", ch);
	break;
    case POS_SLEEPING:
	send_to_char("You are sleeping.\n\r", ch);
	break;
    case POS_RESTING:
	send_to_char("You are resting.\n\r", ch);
	break;
    case POS_SITTING:
	send_to_char("You are sitting.\n\r", ch);
	break;
    case POS_STANDING:
	if (MOUNTED(ch))
	{
	    sprintf(buf, "You are riding on %s.\n\r",
		    MOUNTED(ch)->short_descr);
	    send_to_char(buf, ch);
	}
	else
	{
	    send_to_char("You are standing.\n\r", ch);
	}
	break;
    case POS_FIGHTING:
	send_to_char("You are fighting.\n\r", ch);
	break;
    }

    if (RIDDEN(ch))
    {
	sprintf(buf, "You are ridden by %s.\n\r",
		IS_NPC(RIDDEN(ch)) ? RIDDEN(ch)->short_descr : RIDDEN(ch)->
		name);
	send_to_char(buf, ch);
    }

    if (ch->pcdata->commands != NULL) {
	COMMAND_DATA *cmd;
	int i;
	char buf2[MSL];

	send_to_char("{YYou have been granted the following commands:{x\n\r", ch);

	i = 0;
	for (cmd = ch->pcdata->commands; cmd != NULL; cmd = cmd->next) {
	    i++;
	    sprintf(buf2, "%-15s", cmd->name);
	    if (i % 4 == 0)
		strcat(buf2, "\n\r");

	    send_to_char(buf2, ch);
	}

	if (i % 4 != 0)
	    send_to_char("\n\r", ch);
    }

    if (IS_IMMORTAL(ch))
    {
		send_to_char("Holy: ", ch);
		send_to_char(MXPCreateSend(ch->desc,"holyaura", (IS_SET(ch->act[1], PLR_HOLYAURA) ? "{WAURA{x" : "{DAURA{x")), ch);
		send_to_char(" ", ch);
		send_to_char(MXPCreateSend(ch->desc,"holylight", (IS_SET(ch->act[0], PLR_HOLYLIGHT) ? "{WLIGHT{x" : "{DLIGHT{x")), ch);
		send_to_char(" ", ch);
		send_to_char(MXPCreateSend(ch->desc,"holypersona", (IS_SET(ch->act[1], PLR_HOLYPERSONA) ? "{WPERSONA{x" : "{DPERSONA{x")), ch);
		send_to_char(" ", ch);
		send_to_char(MXPCreateSend(ch->desc,"holywarp", (IS_SET(ch->act[1], PLR_HOLYWARP) ? "{WWARP{x" : "{DWARP{x")), ch);
		send_to_char("\n\r", ch);

		if (ch->invis_level > 0)
		{
			send_to_char(formatf("Invisible: level %d  [ ", ch->invis_level), ch);
			send_to_char(MXPCreateSend(ch->desc,"wizinvis", "off"), ch);

			if (ch->invis_level > 2)
			{
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("wizinvis %d", ch->invis_level - 1), "down"), ch);
			}

			if (ch->invis_level < get_trust(ch))
			{
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("wizinvis %d", ch->invis_level + 1), "up"), ch);
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("wizinvis %d", get_trust(ch)), "max"), ch);
			}

		}
		else
		{
			send_to_char("Invisible: off  [ ", ch);
			send_to_char(MXPCreateSend(ch->desc,"wizinvis", "on"), ch);
		}
		send_to_char(" ]\n\r", ch);

		if (ch->incog_level > 0)
		{
			sprintf(buf, "Incognito: level %d", ch->incog_level);
			send_to_char(buf, ch);
			send_to_char(formatf("  Incognito: level %d  [ ", ch->incog_level), ch);
			send_to_char(MXPCreateSend(ch->desc,"incognito", "off"), ch);

			if (ch->incog_level > 2)
			{
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("incognito %d", ch->incog_level - 1), "down"), ch);
			}

			if (ch->incog_level < get_trust(ch))
			{
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("incognito %d", ch->incog_level + 1), "up"), ch);
				send_to_char(" ", ch);
				send_to_char(MXPCreateSend(ch->desc,formatf("incognito %d", get_trust(ch)), "max"), ch);
			}
		}
		else
		{
			send_to_char("Incognito: off  [ ", ch);
			send_to_char(MXPCreateSend(ch->desc,"incognito", "on"), ch);
		}
		send_to_char(" ]\n\r", ch);
    }
}

bool are_affects_same_type(AFFECT_DATA *a, AFFECT_DATA *b)
{
	if (a->token)
	{
		if (!b->token) return FALSE;

		return a->token == b->token;
	}
	else
	{
		if (b->token) return FALSE;

		return a->skill == b->skill;
	}
}

char *get_affect_name(AFFECT_DATA *paf)
{
	if (!paf) return "";	

	if (paf->custom_name) return paf->custom_name;

	if (paf->token) return paf->token->name;

	if (IS_VALID(paf->skill))
	{
		if (!IS_NULLSTR(paf->skill->display)) return paf->skill->display;

		return paf->skill->name;
	}
	else
		return "???";
}

void do_affects(CHAR_DATA * ch, char *argument)
{
    AFFECT_DATA *paf, *paf_last = NULL;
    int i;
    char buf[MAX_STRING_LENGTH];

    if (IS_AFFECTED(ch, AFF_HIDE))
    {
        send_to_char("{DYou are hidden.{x\n\r", ch);
    }

    if (IS_AFFECTED(ch, AFF_SNEAK))
    {
        send_to_char("{yYou are sneaking.{x\n\r", ch);
    }

    if (ch->imm_flags != 0)
    {
		sprintf(buf, "{BImmune: {G%s\n\r{x", imm_bit_name(ch->imm_flags));
		send_to_char(buf, ch);
    }

    if (ch->res_flags != 0)
    {
		sprintf(buf, "{BResist: {G%s\n\r{x", imm_bit_name(ch->res_flags));
		send_to_char(buf, ch);
    }

    if (ch->vuln_flags != 0)
    {
		sprintf(buf, "{BVulnerable: {G%s\n\r{x", imm_bit_name(ch->vuln_flags));
		send_to_char(buf, ch);
    }

    if (IS_SAGE(ch))
    {
        sprintf(buf, "{BClass Affects: {Gdetect hidden{x\n\r");
        send_to_char(buf,ch);
    }

    if (ch->race->aff[0] != 0 || ch->race->aff[1] != 0)
    {
        sprintf(buf, "{BRacial Affects: {G%s{x\n\r",
			bitmatrix_string(affect_flagbank, ch->race->aff));
        send_to_char(buf,ch);
    }

    if (ch->affected != NULL)
    {
	    bool found;
	send_to_char("{BYou are affected by the following:\n\r{x", ch);

	for(i=0;affgroup_mobile_flags[i].name;i++) {
		found = FALSE;
		paf_last = NULL;
		for (paf = ch->affected; paf != NULL; paf = paf->next) if(paf->group == affgroup_mobile_flags[i].bit && paf->custom_name) {
			if(!found) {
				found = TRUE;
				sprintf(buf, "{YGroup: {W%s{x\n\r", affgroup_mobile_flags[i].name);
				send_to_char(buf, ch);
			}

			if (paf_last != NULL && paf->custom_name == paf_last->custom_name)
				sprintf(buf, "                           ");
			else
				sprintf(buf, "{BSpell: {G%-20s{x", get_affect_name(paf));
			send_to_char(buf, ch);

			sprintf(buf, "{G: {Blevel {W%3d{B, modifies {W%s {Bby {W%d {x", paf->level, affect_loc_name(paf->location), paf->modifier);
			send_to_char(buf, ch);
			if (paf->duration < 0)	/* @@@NIB : 20070126 : Allows for any negative durations */
				sprintf(buf, "{Rpermanently{x");
			else
				sprintf(buf, "{Bfor {W%d {Bhours{x", paf->duration);
			send_to_char(buf, ch);

			send_to_char("\n\r", ch);
			paf_last = paf;
		}

		paf_last = NULL;
		for (paf = ch->affected; paf != NULL; paf = paf->next) if(paf->group == affgroup_mobile_flags[i].bit && !paf->custom_name) {
			if(!found) {
				found = TRUE;
				sprintf(buf, "{YGroup: {W%s{x\n\r", affgroup_mobile_flags[i].name);
				send_to_char(buf, ch);
			}
			if (paf_last != NULL && are_affects_same_type(paf, paf_last))
				sprintf(buf, "                           ");
			else
				sprintf(buf, "{BSpell: {G%-20s{x", get_affect_name(paf));

			send_to_char(buf, ch);

			sprintf(buf, "{G: {Blevel {W%3d{B, modifies {W%s {Bby {W%d {x", paf->level, affect_loc_name(paf->location), paf->modifier);
			send_to_char(buf, ch);
			if (paf->duration < 0)	/* @@@NIB : 20070126 : Allows for any negative durations */
				sprintf(buf, "{Rpermanently{x");
			else
				sprintf(buf, "{Bfor {W%d {Bhours{x", paf->duration);
			send_to_char(buf, ch);

			send_to_char("\n\r", ch);
			paf_last = paf;
		}
	}
    }
    else
	send_to_char("You are not affected by any spells.\n\r", ch);

    return;
}


char *const day_name[] =
{
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
    "Saturday", "Sunday"
};

char *const month_name[] =
{
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

const char *moon_phase_desc[][3] = {
	/* 0:daytime, 1:nighttime, 2:phasename */
	{
		NULL,
		"{BThere is no moon in the sky.{x\n\r",
		"New Moon"
	},
	{
		"{BA faint sliver of the moon grows larger with every passing day.{x\n\r",
		"{BA glowing sliver of the moon grows larger with every passing day.{x\n\r",
		"Waxing Crescent"
	},
	{
		"{BThe right half of the moon looms in the sky faintly.{x\n\r",
		"{BThe right half of the moon looms in the sky brightly.{x\n\r",
		"First Quarter"
	},
	{
		"{BA nearly full moon looms in the sky faintly.{x\n\r",
		"{BA nearly full moon glows brightly in the sky.{x\n\r",
		"Waxing Gibbous"
	},
	{
		"{BA round orb, the ghostly moon looms in all its glory.{x\n\r",
		"{BThe bright, shining moon glows with all its glory.{x\n\r",
		"Full Moon"
	},
	{
		"{BThe once full moon looms in the sky faintly.{x\n\r",
		"{BThe once full moon glows brightly in the sky.{x\n\r",
		"Waning Gibbous"
	},
	{
		"{BThe left half of the moon looms in the sky faintly.{x\n\r",
		"{BThe left half of the moon looms in the sky brightly.{x\n\r",
		"Last Quarter"
	},
	{
		"{BA faint sliver of the moon slips away into the lit sky.{x\n\r",
		"{BA glowing sliver of the moon slips away into the dark night.{x\n\r",
		"Waning Crescent"
	}
};


char *moon_face[19] = {
	".----------.",
	".--':;:o;;::.;;:`--.",
	".-'@;:@@@@@@:O:::.:;:.:`-.",
	".'.@@@:@@@@@@@@:::@@@@:::.::`.",
	"/:::':.'@@@@@@@@,;@@@@:@:;;::.:\\",
	"/@@::o::::@@@:@,.::;@@@@::;;O;:;@\\",
	"/@@@@;;::::::;;;:;;::@@@@@@;;:::@@@\\",
	".:@@@@@\\;:`.-./:'.;.:@@@@@@@@;;.::@@ .",
	"|:@@@@);;--`-'::.;-;;;:@@@@@@@::;;;::|",
	"|@:@@@::;;;:;`;;;::;o::@@@@@@:@@@@:::|",
	"|:;);;;@@:::;;::(o);::::@@;;:@@@@@@;:|",
	"`;.::@::():::@@;;::;();;;@@@;:@@@@;;:'",
	"\\;;;::@@::;@@@@;:;;::::.;@@:::.:'o;/",
	"\\;;:@@@@:;@@\\;;.;@:::;::;:;o;::::/",
	"\\:.;@@:;::;_\\:/:@@;.:::;;;.-.';/",
	"`.:::;.:;::()---;;;::;;;`-' .'",
	"`-.:::;./@|;:.|;;o;;::;.-'",
	"`--./:::.;;;::;;.--'",
	"`----------'"
};

char *moon_colours[][19] = {
	{
		"xxxxxxxxxxxx",
		"xxxxWWxDxxxxDxxxxxxx",
		"xWxDxxDDDDDDxDxxxDxxxDxxxx",
		"WxxDDxxDDDDDDDDxxxDDDDxxxDxxWW",
		"xxWWxxDxDDDDDDDDxxDDDDDDxxxxxDxx",
		"xDDxxDxxxxDDDDDDDxxxDDDDxxxxDxxxDx",
		"xDDDDxxxxxxxxxxxxxxxxDDDDDDxxxxxDDDx",
		"xxDDDDDxxxDDDDDxxxxDxDDDDDDDDxxDxxDDxx",
		"xxDDDDxxxDDDDDxxDxxxxxxDDDDDDDxxxxxxxx",
		"xDxDDxxxxxxxxDxxxxxxDxxDDDDDDxDDDDxxxx",
		"xxxxxxxDDxxxxxxxxDxxxxxxDDxxxDDDDDDxxx",
		"xxDxxDxxxxxxxDDxxxxxDDxxxDDDxxDDDDxxxx",
		"xxxxxxDDxxxDDDDxxxxxxxxDxDDxxxDxxDxx",
		"xxxxDDDDxxDDDxxDxxxxxxxxxxxDxxxxxx",
		"xxDxDDxxxxxDDxDxxxxDxxxxxxDDDxxx",
		"xxxxxxDxxxxDDDDDxxxxxxxxDDDxxx",
		"xxxxxxxDDxDxxDxxxDxxxxxDDD",
		"xxxxDxxxDxxxxxxxxxxx",
		"xxxxxxxxxxxx"
	},
	{
		"RRRRRRRRRRRR",
		"RRRRRRRrRRRRrRRRRRRR",
		"RRRrRRrrrrrrRrRRRrRRRrRRRR",
		"RRRrrRRrrrrrrrrRRRrrrrRRRrRRRR",
		"RRRRRRrRrrrrrrrrRRrrrrrrRRRRRrRR",
		"RrrRRrRRRRrrrrrrrRRRrrrrRRRRrRRRrR",
		"RrrrrRRRRRRRRRRRRRRRRrrrrrrRRRRRrrrR",
		"RRrrrrrRRRrrrrrRRRRrRrrrrrrrrRRrRRrrRR",
		"RRrrrrRRRrrrrrRRrRRRRRRrrrrrrrRRRRRRRR",
		"RrRrrRRRRRRRRrRRRRRRrRRrrrrrrRrrrrRRRR",
		"RRRRRRRrrRRRRRRRRrRRRRRRrrRRRrrrrrrRRR",
		"RRrRRrRRRRRRRrrRRRRRrrRRRrrrRRrrrrRRRR",
		"RRRRRRrrRRRrrrrRRRRRRRRrRrrRRRrRRrRR",
		"RRRRrrrrRRrrrRRrRRRRRRRRRRRrRRRRRR",
		"RRrRrrRRRRRrrRrRRRRrRRRRRRrrrRRR",
		"RRRRRRrRRRRrrrrrRRRRRRRRrrrRRR",
		"RRRRRRRrrRrRRrRRRrRRRRRrrr",
		"RRRRrRRRrRRRRRRRRRRR",
		"RRRRRRRRRRRR"
	}
};

char *moon_colours_mxp[19] = {
				 "333333333333",
			 "33335532333323333333",
		  "35323322222232333233323333",
		"533223322222222333222233323355",
	   "33553323222222223322222233333233",
	  "3223323333222222233322223333233323",
	 "322223333333333333333222222333332223",
	"33222223332222233332322222222332332233",
	"33222233322222332333333222222233333333",
	"32322333333332333333233222222322223333",
	"33333332233333333233333322333222222333",
	"33233233333332233333223332223322223333",
	 "333333223332222333333332322333233233",
	  "3333222233222332333333333332333333",
	   "33232233333223233332333333222333",
		"333333233332222233333333222333",
		  "33333332232332333233333222",
			 "33332333233333333333",
				 "333333333333"
};

char *moon_shadow[19] = {
	".----------.",
	".--'            `--.",
	".-'                    `-.",
	".'                          `.",
	"/                              \\",
	"/                                \\",
	"/                                  \\",
	".                                    .",
	"|                                    |",
	"|                                    |",
	"|                                    |",
	"`                                    '",
	"\\                                  /",
	"\\                                /",
	"\\                              /",
	"`.                          .'",
	"`-.                    .-'",
	"`--.            .--'",
	"`----------'"
};

char moon_shadow_colours[] = "Dr";
char moon_shadow_colour_mxp = '1';

char *moon_spacing = "                                        ";

void draw_moon(CHAR_DATA *ch,int colour)
{
	int i,j,k,l,ll,ld;
	int hours;
	double h, c;
	char buf[MIL],lastc;
	bool mxp = false;

	// Check if they can get better coloring of the moon!
	if( ch || ch->desc || ch->desc->pProtocol )
		mxp = ch->desc->pProtocol->bMXP;

	hours = ((((time_info.year*12)+time_info.month)*35+time_info.day)*24+time_info.hour+MOON_OFFSET) % MOON_PERIOD;
	hours = (hours + MOON_PERIOD) % MOON_PERIOD;

	h = (double)hours / MOON_PERIOD;

/*	sprintf(buf,"hours = %d/%d (%.2lf%%)\n\r", hours, MOON_PERIOD,100.0*h);
	send_to_char(buf,ch); */

	c = (1 - cos(6.2831853 * h)) / 2;

	for(i=0;i<19;i++) {
		l = strlen(moon_shadow[i]);
		ll = (int)(l * c + 0.5);	/* Amount of lit moon */
		if(ll > l) ll = l; else if(ll < 0) ll = 0;
		ld = l - ll;

		if(h < 0.5) {
			/* New Moon to Full Moon
			   Left: Shadow/Nothing, Right: Face */

			send_to_char("{x", ch);
			lastc = 'x';
			if(time_info.hour < 6 || time_info.hour > 19) {
				send_to_char(moon_spacing+20+l/2, ch);
				if( mxp )
				{
					lastc = moon_shadow_colour_mxp;
					if( colour )
					{
						j = sprintf(buf,"%c[F%c00]",COLOUR_CHAR,lastc);
					}
					else
					{
						j = sprintf(buf,"%c[F%c%c%c]",COLOUR_CHAR,lastc,lastc,lastc);
					}
				}
				else
				{
					lastc = moon_shadow_colours[colour];
					j = sprintf(buf,"{%c",lastc);
				}

				for(k = 0; k < ld && moon_shadow[i][k] != 0; k++)
				{
					char ms = moon_shadow[i][k];

					if( ms == '{' || ms == COLOUR_CHAR)
						buf[j++] = ms;

					buf[j++] = ms;
				}
				buf[j] = 0;
				send_to_char(buf, ch);
			} else
				send_to_char(moon_spacing+20+ll-l/2, ch);


			for(k=j=0;k<ll;k++) {
				if( mxp )
				{
					if(lastc != moon_colours_mxp[i][k+ld])
					{
						lastc = moon_colours_mxp[i][k+ld];

						if( colour )
						{
							j += sprintf(buf+j,"%c[F%c00]",COLOUR_CHAR,lastc);
						}
						else
						{
							j += sprintf(buf+j,"%c[F%c%c%c]",COLOUR_CHAR,lastc,lastc,lastc);
						}
					}
				}
				else
				{
					if(lastc != moon_colours[colour][i][k+ld]) {
						buf[j++] = '{';
						buf[j++] = lastc = moon_colours[colour][i][k+ld];
					}
				}

				char mf = moon_face[i][k+ld];

				if( mf == '{' || mf == COLOUR_CHAR )
				{
					// Double up escaping codes
					buf[j++] = mf;
				}

				buf[j++] = mf;
			}
			buf[j] = 0;
			send_to_char(buf, ch);

			if(lastc != 'x') send_to_char("{x", ch);
			send_to_char(moon_spacing+20+l/2, ch);

		} else {
			/* Full Moon to New Moon
			   Left: Face, Right: Shadow/Nothing */

			send_to_char("{x", ch);
			lastc = 'x';
			send_to_char(moon_spacing+20+l/2, ch);
			for(k=j=0;k<ll;k++) {
				if( mxp )
				{
					if(lastc != moon_colours_mxp[i][k+ld])
					{
						lastc = moon_colours_mxp[i][k+ld];

						if( colour )
						{
							j += sprintf(buf+j,"%c[F%c00]",COLOUR_CHAR,lastc);
						}
						else
						{
							j += sprintf(buf+j,"%c[F%c%c%c]",COLOUR_CHAR,lastc,lastc,lastc);
						}
					}
				}
				else
				{
					if(lastc != moon_colours[colour][i][k+ld]) {
						buf[j++] = '{';
						buf[j++] = lastc = moon_colours[colour][i][k+ld];
					}
				}

				char mf = moon_face[i][k];

				if( mf == '{' || mf == COLOUR_CHAR )
				{
					// Double up escaping codes
					buf[j++] = mf;
				}

				buf[j++] = mf;
			}
			buf[j] = 0;
			send_to_char(buf, ch);

			if(time_info.hour < 6 || time_info.hour > 19) {
				if( mxp )
				{
					lastc = moon_shadow_colour_mxp;
					if( colour )
					{
						j = sprintf(buf,"%c[F%c00]",COLOUR_CHAR,lastc);
					}
					else
					{
						j = sprintf(buf,"%c[F%c%c%c]",COLOUR_CHAR,lastc,lastc,lastc);
					}
				}
				else
				{
					lastc = moon_shadow_colours[colour];
					j = sprintf(buf,"{%c",lastc);
				}

				for(k = 0; k < ld && moon_shadow[i][k] != 0; k++)
				{
					char ms = moon_shadow[i][k+ll];

					if( ms == '{' || ms == COLOUR_CHAR)
						buf[j++] = ms;

					buf[j++] = ms;
				}
				buf[j] = 0;
				send_to_char(buf, ch);
				send_to_char("{x", ch);
				//send_to_char(moon_spacing+20+l/2, ch);		// This isn't necessary
			} else {
				send_to_char("{x", ch);
				//send_to_char(moon_spacing+20+ll-l/2, ch);		// This isn't necessary
			}
		}

		send_to_char("\n\r", ch);
	}

}


void do_time(CHAR_DATA * ch, char *argument)
{
    extern char str_boot_time[];
    char buf[MAX_STRING_LENGTH];
    char *suf;
    int day, i, mins;
    bool lunar = FALSE;

    day = time_info.day + 1;

    buf[0] = '\0';

	if (IS_OUTSIDE(ch)) {
		if(!str_cmp(argument,"moon")) {
			draw_moon(ch,(reckoning_timer > 0));
			lunar = true;
		}
		suf = (char *)moon_phase_desc[time_info.moon][(time_info.hour < 6 || time_info.hour > 19)];
		if(suf) strcpy(buf,suf);
	}

	if (buf[0] != '\0') send_to_char(buf, ch);

	/* Add a skill for this? */
	if (0) {
		sprintf(buf,"{BLunar Phase: {W%s{x\n\r", moon_phase_desc[time_info.moon][2]);
		send_to_char(buf, ch);
	}

    if (day > 4 && day < 20)
	suf = "th";
    else if (day % 10 == 1)
	suf = "st";
    else if (day % 10 == 2)
	suf = "nd";
    else if (day % 10 == 3)
	suf = "rd";
    else
	suf = "th";

    sprintf(buf,
	    "{BIt is currently {X%d{B o'clock {X%s{B.{x\n\r"
	    "{BDay of {X%s{B, {X%d%s{B of the Month of {X%s{B.{X\n\r",
	    (time_info.hour % 12 == 0) ? 12 : time_info.hour % 12,
	    time_info.hour >= 12 ? "pm" : "am",
	    day_name[day % 7],
	    day,
	    suf,
	    month_name[time_info.month]);
    send_to_char(buf, ch);

    sprintf(buf,
	    "{BThe last reboot was at {X%s{B{X{BThe system time is {X%s{X",
	    str_boot_time, (char *) ctime(&current_time));
    send_to_char(buf, ch);

    /* Show people how must time is left on various boosts. */
    for (i = 0; boost_table[i].name != NULL && !strcmp(boost_table[i].name, "reckoning") == 0; i++) {
	if (boost_table[i].boost != 100) {
	    mins = (boost_table[i].timer - current_time)/60;

	    if (mins == 0)
		sprintf(buf, "{BThere is {xless than a minute{B of {x%s{B boost ({x%+d%%{B) remaining.{x\n\r",
			boost_table[i].name, (boost_table[i].boost - 100));
	    else
		sprintf(buf, "{BThere %s {x%d minute%s{B of {x%s{B boost ({x%+d%%{B) remaining.{x\n\r",
			mins > 1 ? "are" : "is",
			mins,
			mins > 1 ? "s" : "",
			boost_table[i].name, (boost_table[i].boost - 100));
	send_to_char(buf,ch);
	}
	}
	/* Add special timer for showing remaining reckoning info to immortals. -- Areo */
	if(IS_IMMORTAL(ch))
	{
		if (boost_table[BOOST_RECKONING].boost != 100){
		mins = (boost_table[BOOST_RECKONING].timer - current_time)/60;

		if (mins == 0)
		sprintf(buf, "{RThere is {xless than a minute{R left in The Reckoning{x\n\r");
		else
		sprintf (buf, "{RThere %s {x%d minute%s{R of The Reckoning remaining.{x\n\r",
			mins > 1 ? "are" : "is",
			mins,
			mins > 1 ? "s" : "");
	send_to_char(buf,ch);
	}
	}

	if(!IS_NPC(ch) && lunar)
		p_percent_trigger(ch,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_MOON, NULL,0,0,0,0,0);
}


/*    Uncommenting this function, as Whisp's weather system isn't yet functional -- Areo */
void do_weather(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    static char *const sky_look[4] =
    {
	"cloudless",
	"cloudy",
	"rainy",
	"lit by flashes of lightning"
    };

    if (!IS_OUTSIDE(ch))
    {
	send_to_char("You can't see the weather here.\n\r", ch);
	return;
    }

    if (IN_NETHERWORLD(ch)) {
	send_to_char("Thick rolling clouds tumble and turn. Lightning crashes to the ground all around you.\n\r", ch);
	return;
    }

    if (pre_reckoning > 0) {
	switch (pre_reckoning) {
	case 2:
	    send_to_char
		("The sky is calm but the clouds appear a dull purple.\n\r",
		 ch);
	    break;
	case 3:
	    send_to_char
		("Thick rolling clouds tumble and turn, dimming the light around you.\n\r",
		 ch);
	    break;
	case 4:
	    send_to_char
		("A very strong wind blows from all around you.\n\r", ch);
	    break;
	case 5:
	    send_to_char
		("Sheets of cold blue lightning light the sky as the demonic terror grips the world.\n\r",
		 ch);
	    break;
	}
	return;
    } else if (reckoning_timer > 0) {
	send_to_char
	    ("Sheets of cold blue lightning light the sky as a demonic terror grips the world.\n\r",
	     ch);
	return;
    }

    sprintf(buf, "The sky is %s and %s.\n\r",
	    sky_look[weather_info.sky],
	    weather_info.change >= 0
	    ? "a warm southerly breeze blows" :
	    "a cold northern gust blows");

    send_to_char(buf, ch);
    return;
}




/*    Inclusive who-command */
void do_who_new(CHAR_DATA * ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char level[MSL];
    BUFFER *output;
    DESCRIPTOR_DATA *d;
    int iLevelLower;
    int iLevelUpper;

    int nMatch;
    int nMatch2;
    int line_counter = 0;
    int buf_size;
    int racelen,classlen;
    char *area_type;
    CHURCH_DATA *church = NULL;
    CHAR_DATA *wch;
    char classstr[MAX_STRING_LENGTH];
    char racestr[MAX_STRING_LENGTH];

    iLevelLower = 0;
    iLevelUpper = MAX_LEVEL;


    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    if (arg[0] != '\0')
    {
		if (is_number(arg) && is_number(arg2))
		{
			iLevelLower = atoi(arg);
			iLevelUpper = atoi(arg2);
		}

		for (church = church_list; church != NULL; church = church->next)
		{
			if (!str_prefix(arg, church->name))
			break;
		}
    }

    send_to_char("\n\r{b.,-~^~{B-,._.,{C[ {WPlayers in Sentience {C]{B-.._.,-{b~^~-,.{x\n\r", ch);

    nMatch = 0;
    nMatch2 = 0;
    for (d = descriptor_list; d != NULL; d = d->next)
    {
		CHAR_DATA *wch;

		if (d->connected != CON_PLAYING)
	    	continue;

		wch = (d->original != NULL) ? d->original : d->character;

		if (wch)
		{
			if (IS_IMMORTAL(wch) && !can_see_imm(ch, wch))
				continue;
			else
				nMatch2++;
		}
    }

    buf[0] = '\0';
    output = new_buf();
    for (d = descriptor_list; d != NULL; d = d->next)
    {
		wch = (d->original != NULL) ? d->original : d->character;

		if (d->connected != CON_PLAYING || (IS_IMMORTAL(wch) && !can_see_imm(ch, wch))) {
		    continue;
		}


//	if (!can_see(ch, wch)) {
//	    continue;
//	}

		if ((iLevelLower != 0 &&  wch->tot_level >= iLevelLower && wch->tot_level <= iLevelUpper) ||
			arg[0] =='\0' || !str_prefix(arg, wch->name) ||
			((!str_cmp(arg, "immortal") || !str_cmp(arg, "immortals") || !str_cmp(arg, "imm")) && wch->tot_level >= LEVEL_IMMORTAL) ||
			(church != NULL && wch->church == church) ||
			!str_prefix(arg, wch->race->name) ||
			(IS_VALID(wch->pcdata->current_class) && !str_prefix(arg, wch->pcdata->current_class->clazz->display[wch->sex])))
			;
		else
			continue;

		char *clazzptr;
		char clazz_color = 'x';
        if (IS_IMMORTAL(wch) && IS_SET(wch->act[1], PLR_HOLYPERSONA))
		    clazzptr = wch->pcdata->immortal->imm_flag;
		else if (IS_VALID(wch->pcdata->current_class))
		{
	    	clazzptr = wch->pcdata->current_class->clazz->who[wch->sex];

			switch(wch->pcdata->current_class->clazz->type)
			{
			case CLASS_CLERIC:		clazz_color = 'Y'; break;
			case CLASS_CRAFTING:	clazz_color = 'C'; break;
			case CLASS_EXPLORER:	clazz_color = 'M'; break;
			case CLASS_GATHERING:	clazz_color = 'G'; break;
			case CLASS_MAGE:		clazz_color = 'B'; break;
			case CLASS_THIEF:		clazz_color = 'W'; break;
			case CLASS_WARRIOR:		clazz_color = 'R'; break;
			}
		}
		else
			clazzptr = "";

		// This will autocenter the text into a 12 character span
		classlen = strlen_no_colours(clazzptr);
		int start = 0;
		while(classlen < 12)
		{
			if ((classlen & 1) == 0)	// Even
			{
				classstr[start++] = ' ';
			}

			classlen++;
		}
		strcpy(classstr + start, clazzptr);

		classlen = 12 + strlen(classstr) - strlen_no_colours(classstr);

		if (IS_VALID(wch->race))
			strcpy(racestr, wch->race->who);
		else
			strcpy(racestr, "       ");
		racelen = 7 + strlen(racestr) - strlen_no_colours(racestr);

		nMatch++;

		area_type = get_char_where(wch);

		if (IS_IMMORTAL(wch))
		    sprintf(level, "{W  IMM  {x");
		else
		{
			CLASS_LEVEL *current = wch->pcdata->current_class;
			if (IS_VALID(current))
		    	sprintf(level, "%-3d{B:{G%-3d", current->level, wch->tot_level);
			else
		    	sprintf(level, "---{B:{G%-3d", wch->tot_level);
		}

		sprintf(buf,
        	"{B[{G%s{B][{M%s{B][ {Y%-*.*s {%c%-*.*s {C%-6s {B] %s%s%s%s{G%-12s{x",
			level,
			wch->sex == 0 ? "N" : (wch->sex == 1 ? "M" : "F"),
			racelen,racelen,racestr,
			clazz_color,classlen,classlen,classstr,
			area_type,
			IS_DEAD(wch) ? "{D(Dead) {x" : "",
			wch->incog_level > LEVEL_HERO ? "{D(Incog) {x" : "",
			wch->invis_level > LEVEL_HERO ? "{W(Wizi) {x" : "",
			IS_SET(wch->act[0], PLR_BOTTER) ? "{G[BOTTER] {x" : "",
			wch->name);

		free_string(area_type);
		add_buf(output, buf);

		if (wch->church != NULL)
		{
			buf_size = 50 - fstr_len(&buf[0]);

			for (line_counter = 0; line_counter < buf_size; line_counter++)
			add_buf(output, " ");

			add_buf(output, "{Y[{x");
			add_buf(output, wch->church->flag);
			add_buf(output, "{Y]{x");
		}

		if (IS_SET(wch->act[0],PLR_HELPER))
		    add_buf(output, " {W[H]{X");

		if (IS_SET(wch->comm, COMM_AFK))
		    add_buf(output, " {M[AFK]{x");

		if (IS_SET(wch->comm, COMM_QUIET))
			add_buf(output, " {R[Q]{x");

		if (IS_SET(wch->act[0], PLR_PK) || (wch->church != NULL && wch->church->pk == TRUE))
		    add_buf(output, " {R[PK]{x");

		if (IS_SET(wch->act[0], PLR_BUILDING))
	    	add_buf(output, " {r[Building]{x");

		add_buf(output, "\n\r");
    }

	if( nMatch != nMatch2 ) {
		sprintf(buf2, "\n\rPlayers found: %d\n\r", nMatch);
		add_buf(output, buf2);
	}
    sprintf(buf2, "Players online: %d\n\r", nMatch2);
    add_buf(output, buf2);
    page_to_char(buf_string(output), ch);
    free_buf(output);
}

void do_whois(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    BUFFER *output;
    char buf[3*MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;
    bool found = FALSE;
    int i;
    char idle_time[MSL];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("You must provide a name.\n\r", ch);
	return;
    }

    output = new_buf();

    for (d = descriptor_list; d != NULL; d = d->next)
    {
	CHAR_DATA *wch;
	char const *class;
	char racestr[MAX_STRING_LENGTH];

	if (d->connected != CON_PLAYING || !can_see(ch, d->character))
	    continue;

	wch = (d->original != NULL) ? d->original : d->character;

	if (!can_see(ch, wch))
	    continue;

	if (!str_prefix(arg, wch->name))
	{
	    found = TRUE;

	    class = IS_VALID(wch->pcdata->current_class) ? wch->pcdata->current_class->clazz->name : "unknown";

		// TODO: Add HOLY flag that can be used to show this
        if (wch->tot_level >= LEVEL_IMMORTAL)
		    class = wch->pcdata->immortal->imm_flag;

	if(!IS_IMMORTAL(wch)) {
	        while (*class == '{')
		    class += 2;
	}

	while (isspace(*class))
	{
	   class++;
	}

	// TODO: Add HOLYPERSONA that will hide an immortal's race and class
	/* If they are a player or a shaper, then use the PLAYER RACE name... */
	if(!IS_IMMORTAL(wch) || wch->race == gr_shaper)
		strcpy(racestr, wch->race->who);
	else if (wch->sex == SEX_FEMALE)
		strcpy(racestr, "Goddess");
	else
		strcpy(racestr, "God");

	buf[0] = '\0';
	for (i = 0; i < 32; i++)
  	    strcat(buf, "{Y-{y+");
        add_buf(output, buf);

	if (wch->timer > 0)
	{
	    sprintf(idle_time, "%d minutes", wch->timer);
	}
	else
	{
	    sprintf(idle_time, "Active");
	}

        sprintf(buf, "\n\r{x"
	             "Name         : %s%s\n\r{x"
		     "Sex          : %s\n\r{x"
		     "Church       : %s\n\r{x"
		     "Rank         : %s\n\r{x"
               	     "Race         : %s\n\r{x"
		     "Subclass     : %s\n\r{x"
		     "Level        : %d\n\r\n\r"
		     "Player Kills : {R%d{x\n\r"
		     "Arena Kills  : {x%d{x\n\r"
		     "CPK Kills    : {r%d{x\n\r"
	             "Idle         : %s\n\r"
		     "\n\rDescription:\n\r",
		     wch->name,
		     (wch->pcdata->title != NULL) ? wch->pcdata->title : "",
		     wch->sex == SEX_NEUTRAL ? "None" : (wch->sex == SEX_MALE ? "Male" : "Female"),
		     (wch->church != NULL) ? wch->church_name : "None",
		     (wch->church != NULL && wch->church_member != NULL) ? get_chrank(wch->church_member) : "None",
                     racestr,
		     class,
		     wch->tot_level,
		     wch->player_kills,
		     wch->arena_kills,
		     wch->cpk_kills,
		     idle_time
		    );

	add_buf(output, buf);

	if (wch->description != NULL)
	  add_buf(output, wch->description);

	sprintf(buf, "\n\r");
	for (i = 0; i < 32; i++)
  	    strcat(buf, "{Y-{y+");
	strcat(buf, "{x\n\r");
        add_buf(output, buf);

	break;
	}
    }

    if (!found)
    {
	send_to_char("No one of that name is playing.\n\r", ch);
	return;
    }

    page_to_char(buf_string(output), ch);
    free_buf(output);
}


void format_page(sh_int n, char *a, CHAR_DATA * ch)
{
    sh_int counter;

    if (n - fstr_len(a) <= 0)
	return;

    for (counter = 0; counter < n - fstr_len(a); counter++)
	send_to_char(" ", ch);

}


int fstr_len(char *a)
{
    int counter = 0;
    int char_number = 0;
    for (counter = 0; counter < strlen(a); counter++) {
	if (a[counter] == '{') {
	    counter++;
	} else
	    char_number++;
    }
    return char_number;
}


void do_count(CHAR_DATA * ch, char *argument)
{
    int count;
    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];

    count = 0;

    for (d = descriptor_list; d != NULL; d = d->next)
	if (/*d->connected == CON_PLAYING &&*/ can_see(ch, d->character))
	    count++;

    max_on = UMAX(count, max_on);

    if (max_on == count)
	sprintf(buf,
		"There are {Y%d{x characters on, the most so far today.\n\r",
		count);
    else
	sprintf(buf,
		"There are {Y%d{x characters on, the most on today was {Y%d{x.\n\r",
		count, max_on);

    send_to_char(buf, ch);
}


void do_inventory(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    send_to_char("You are carrying:\n\r", ch);
    show_list_to_char(ch->carrying, ch, TRUE, TRUE);
    if (!IS_DEAD(ch))
    {
	sprintf(buf,
  	    "A total of %d/%d items with weight %ld/%d kg.{x "
	    "(%ld kg in coins)\n\r",
	    ch->carry_number,
	    can_carry_n(ch),
	    get_carry_weight(ch),
	    can_carry_w(ch), COIN_WEIGHT(ch));
	send_to_char(buf, ch);
    }
}


void do_equipment(CHAR_DATA * ch, char *argument)
{
    show_equipment(ch, ch);
}


/*    Show victim's worn equipment to ch. */
void show_equipment(CHAR_DATA *ch, CHAR_DATA *victim)
{
	BUFFER *buffer;
	OBJ_DATA *obj;
	char buf[MSL];
	char buf2[MSL];
	int idx, iWear, parent, count = 0;
	OBJ_DATA *eq[MAX_WEAR];

	buffer = new_buf();

	memset(eq,0,sizeof(eq));

	for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
		if(obj->wear_loc != WEAR_NONE)
			eq[obj->wear_loc] = obj;

	for (idx = 0; wear_view_order[idx] != WEAR_NONE; idx++) {
		iWear = wear_view_order[idx];

		/* Is the slot unusable in shifted form? */
		if(!wear_params[iWear][3] && (IS_SHIFTED_SLAYER(victim) || IS_SHIFTED_WEREWOLF(victim)))
			continue;

		/* Can others see the slot? */
		if(ch != victim) {
			if(!wear_params[iWear][0]) continue;

			parent = wear_concealed[iWear];
			while(parent != WEAR_NONE && (!eq[parent] || !CAN_WEAR(eq[parent],ITEM_CONCEALS)))
				parent = wear_concealed[parent];

			if(parent != WEAR_NONE) continue;
		}

		if ((iWear == WEAR_SECONDARY || iWear == WEAR_SHIELD || iWear == WEAR_HOLD)
			&& !eq[iWear] && both_hands_full(victim))
			continue;

		sprintf(buf, "%s ", where_name[iWear]);
		if (eq[iWear]) {
			if (can_see_obj(ch, eq[iWear]))
				sprintf(buf2, "%s\n\r", format_obj_to_char(eq[iWear], ch, TRUE));
			else
				sprintf(buf2, "something.\n\r");
		} else if (wear_params[iWear][1] && IS_SET(victim->act[0], PLR_AUTOEQ) && ch == victim)
			sprintf(buf2, "nothing.\n\r");
		else
			continue;

		++count;

		strcat(buf, buf2);

		add_buf(buffer, buf);
	}


	if(ch == victim && !count)
		send_to_char("You aren't using any equipment.\n\r", ch);
	else
		page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
}

void do_credits(CHAR_DATA * ch, char *argument)
{
    do_function(ch, &do_help, "diku");
}

void do_consider(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    char *msg;
    int diff;

    one_argument(argument, arg);

    if (arg[0] == '\0') {
	send_to_char("Consider killing whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL) {
	send_to_char("They're not here.\n\r", ch);
	return;
    }

    if (is_safe(ch, victim, FALSE))
    {
	send_to_char("Don't even think about it.\n\r", ch);
	return;
    }

    diff = victim->tot_level - ch->tot_level;

    if (diff <= -20)
	msg = "You can kill $N naked and weaponless.";
    else if (diff <= -15)
	msg = "$N is no match for you.";
    else if (diff <= -10)
	msg = "$N looks like an easy kill.";
    else if (diff <= -5)
	msg = "The perfect match!";
    else if (diff <= 1)
	msg = "$N says 'Do you feel lucky, punk?'.";
    else if (diff <= 5)
	msg = "$N laughs at you mercilessly.";
    else
	msg = "Death will thank you for your gift.";

    act(msg, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    return;
}


void set_title(CHAR_DATA * ch, char *title)
{
    if (IS_NPC(ch)) {
		bug("Set_title: NPC.", 0);
		return;
    }

    free_string(ch->pcdata->title);
    ch->pcdata->title = str_dup(title);
}

void do_title(CHAR_DATA * ch, char *argument)
{
	char arg[MIL];
    if (IS_NPC(ch))
		return;

    if (argument[0] == '\0')
	{
		send_to_char("Syntax:  title set <title...>\n\r", ch);
		send_to_char("         title clear\n\r", ch);
		send_to_char("\n\r", ch);
		send_to_char("Note: Title must include {Y$n{x to indicate where your name will go.\n\r", ch);

		send_to_char("Example: {Wtitle set $n of Bacon{x\n\r", ch);

		char buf[MSL];
		sprintf(buf, "Result:  {+%s of Bacon\n\r", ch->name);
		send_to_char(buf, ch);
		return;
    }

	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "set"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Set your title to what?\n\r", ch);
			return;
		}

		if (strlen(argument) > 45)
			argument[45] = '\0';

		smash_tilde(argument);

		if (IS_IMMORTAL(ch))
		{
			if (str_infix("$n", argument))
			{
				send_to_char("Please include a {Y$n{x somewhere in your title for your name.\n\r", ch);
				return;
			}
		}
		else
		{
			char *plain = nocolour(argument);
			bool valid = !str_prefix(plain, "$n");
			free_string(plain);
			if (!valid)
			{
				send_to_char("Your title must begin with {Y$n{x for your name.\n\r", ch);
				return;
			}
		}

		set_title(ch, argument);

		char buf[MSL * 2];
		char title[MSL];
		sprintf(title, string_replace_static(ch->pcdata->title, "$n", "{+%s"), ch->name);
		sprintf(buf, "Title set to '%s{x'.\n\r", title);
		send_to_char(buf, ch);
		return;
	}
	
	if (!str_prefix(arg, "clear"))
	{
		set_title(ch, "");
		send_to_char("Title cleared.\n\r", ch);
		return;
	}

	do_title(ch, "");	
}


void do_description(CHAR_DATA * ch, char *argument)
{
    string_append(ch, &ch->description);
}


void do_report(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];

    sprintf(buf,
	    "{CYou say 'I have %ld/%ld hp %ld/%ld mana %ld/%ld mv %ld xp.'\n\r{x",
	    ch->hit, ch->max_hit,
	    ch->mana, ch->max_mana, ch->move, ch->max_move, ch->exp);

    send_to_char(buf, ch);

    sprintf(buf,
	    "{C$n says 'I have %ld/%ld hp %ld/%ld mana %ld/%ld mv %ld xp.'{x",
	    ch->hit, ch->max_hit, ch->mana, ch->max_mana, ch->move,
	    ch->max_move, ch->exp);

    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}


void do_wimpy(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int wimpy;

    one_argument(argument, arg);

    if (arg[0] == '\0')
	wimpy = ch->max_hit / 5;
    else
	wimpy = atoi(arg);

    if (wimpy < 0) {
	send_to_char("Your courage exceeds your wisdom.\n\r", ch);
	return;
    }

    if (wimpy > ch->max_hit / 2) {
	send_to_char("Such cowardice ill becomes you.\n\r", ch);
	return;
    }

    ch->wimpy = wimpy;
    sprintf(buf, "Wimpy set to %d hit points.\n\r", wimpy);
    send_to_char(buf, ch);
}

void do_password(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *pArg;
    char *pwdnew;
    char *p;
    char cEnd;

    if (IS_NPC(ch))
	return;

    /*
     * Can't use one_argument here because it smashes case.
     * So we just steal all its code.  Bleagh.
     */
    pArg = arg1;
    while (isspace(*argument))
	argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"')
	cEnd = *argument++;

    while (*argument != '\0') {
	if (*argument == cEnd) {
	    argument++;
	    break;
	}
	*pArg++ = *argument++;
    }
    *pArg = '\0';

    pArg = arg2;
    while (isspace(*argument))
	argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"')
	cEnd = *argument++;

    while (*argument != '\0') {
	if (*argument == cEnd) {
	    argument++;
	    break;
	}
	*pArg++ = *argument++;
    }
    *pArg = '\0';

    if (arg1[0] == '\0' || arg2[0] == '\0') {
	send_to_char("Syntax: password <old> <new>.\n\r", ch);
	return;
    }

	if (ch->pcdata->pwd_vers < 1) {
    if (strcmp(crypt(arg1, ch->pcdata->pwd), ch->pcdata->pwd)) {
	if (strcmp(arg1, ch->pcdata->pwd)){
	WAIT_STATE(ch, 40);
	send_to_char("Wrong password.  Wait 10 seconds.\n\r", ch);
	return;
	}
    }
	} else
	{
		if (strcmp(sha256_crypt(arg1), ch->pcdata->pwd)) {
			WAIT_STATE(ch, 40);
			send_to_char("Wrong password. Wait 10 seconds.\n\r", ch);
			return;
		}
	}


    if (strlen(arg2) < 5) {
	send_to_char
	    ("New password must be at least five characters long.\n\r",
	     ch);
	return;
    }

    /*
     * No tilde allowed because of player file format.
     */
    pwdnew = sha256_crypt(arg2);
    for (p = pwdnew; *p != '\0'; p++) {
	if (*p == '~') {
	    send_to_char("New password not acceptable, try again.\n\r",
			 ch);
	    return;
	}
    }

    free_string(ch->pcdata->pwd);
    ch->pcdata->pwd = str_dup(pwdnew);
	if (ch->pcdata->pwd_vers < 1) {
		ch->pcdata->pwd_vers = 1;
	}
    save_char_obj(ch);
    send_to_char("Password changed.\n\r", ch);
}

void do_bank(CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char temp[255];
    CHAR_DATA *target;
    OBJ_DATA *obj;
    long amount;
    bool item = FALSE;
    ROOM_INDEX_DATA *room;

    if (IS_NPC(ch))
    return;

    room = ch->in_room;

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
	if (obj->item_type == ITEM_BANK) {
	    item = TRUE;
	    break;
	}
    }

    if (!IS_IMMORTAL(ch)
    && !IS_SET(room->room_flag[0], ROOM_BANK)
    && !item)
    {
	send_to_char("You can't do that here.\n\r", ch);
	return;
    }

    if (IS_DEAD(ch)) {
	send_to_char("You can't bank while dead.\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0')
    {
	send_to_char("Usage: \n\r", ch);
	send_to_char("Bank deposit <amount> \n\r", ch);
	send_to_char("Bank withdraw <amount> \n\r", ch);
	send_to_char("Bank balance \n\r", ch);
	send_to_char("Bank wire <amount> <person>\n\r", ch);
	return;
    }

    if (str_cmp(arg1, "BALANCE") == 0)
    {
	if ((arg2[0] != '\0') && (IS_IMMORTAL(ch)))
	{
	    if ((target = get_char_world(ch, arg3)) == NULL || IS_NPC(target = get_char_world(ch, arg3)))
	    {
		send_to_char("They aren't playing.\n\r", ch);
		return;
	    }
	    else
	    {
		    sprintf(temp, "They have {Y%ld{X gold coins in the bank.\n\r",
		    target->pcdata->bankbalance);
		    send_to_char(temp, ch);
		    return;
	    }
	}

	sprintf(temp, "You have {Y%ld{X gold coins in your bank.\n\r",
	ch->pcdata->bankbalance);
	send_to_char(temp, ch);
	return;
    }

    if (str_cmp(arg1, "DEPOSIT") == 0)
    {
	bool fAll = FALSE;

	if (arg2[0] == '\0')
	{
	    send_to_char
		("You need to deposit an amount or 'all'.\n\r", ch);
	    return;
	}

	if (!str_cmp(arg2, "all"))
	{
	    if (ch->gold == 0)
	    {
		send_to_char("You don't have any gold to deposit.\n\r", ch);
		return;
	    }

	    fAll = TRUE;
	}

	if (fAll)
	    amount = ch->gold;
	else
	    amount = atol(arg2);

	if (!fAll)
	{
	    if (amount <= 0)
	    {
		send_to_char("Invalid amount.\n\r", ch);
		return;
	    }

	    if (amount > ch->gold)
	    {
		send_to_char("You don't have that much gold!\n\r", ch);
		return;
	    }
	}

	ch->gold -= amount;
	ch->pcdata->bankbalance += amount;
	sprintf(temp,
		"You transfer {Y%ld{X gold coins to your bank account.\n\r",
		amount);
	send_to_char(temp, ch);
	sprintf(temp, "Your new balance is {Y%ld{X gold coins.\n\r",
		ch->pcdata->bankbalance);
	send_to_char(temp, ch);
	return;
    }

    if (str_cmp(arg1, "WITHDRAW") == 0)
    {
		if (arg2[0] == '\0' || (!is_number(arg2) && str_cmp(arg2, "all")))
		{
			send_to_char("You need to withdraw an amount or 'all'.\n\r",ch);
			return;
		}

		if (str_cmp(arg2, "all"))
			amount = atol(arg2);
		else
			amount = ch->pcdata->bankbalance;

		if (amount == 0)
		{
			send_to_char("Invalid amount.\n\r", ch);
			return;
		}

		// Immortals can withdraw money out of thin air...
		if (!IS_IMMORTAL(ch) && amount > ch->pcdata->bankbalance)
		{
			send_to_char("You don't have that much in the bank.\n\r", ch);
			return;
		}

		if (amount < 0)
		{
			send_to_char("You can't withdraw a negative amount.\n\r", ch);
			return;
		}

		ch->pcdata->bankbalance -= amount;
		if (ch->pcdata->bankbalance < 0)
			ch->pcdata->bankbalance = 0;

		ch->gold += amount;
		sprintf(temp, "You just withdrew {Y%ld{X gold coins from your account.\n\r", amount);
		send_to_char(temp, ch);
		sprintf(temp, "Your new balance is {Y%ld{X gold coins.\n\r", ch->pcdata->bankbalance);
		send_to_char(temp, ch);
		return;
    }

    if (str_cmp(arg1, "WIRE") == 0)
    {
	if(arg2[0] == '\0')
	{
	    send_to_char("Wire how much?\n\r", ch);
	    return;
	}
	if(arg3[0] == '\0')
	{
	    send_to_char("Wire to whom?\n\r", ch);
	    return;
	}

	if ((target = get_char_world(ch, arg3)) == NULL || IS_NPC(target = get_char_world(ch, arg3)))
	{
	    send_to_char("They aren't playing.\n\r", ch);
	    return;
	}

	if(target == ch)
	{
	    send_to_char("What would be the point?\n\r", ch);
	    return;
	}

	amount = atol(arg2);

	if (amount == 0)
	{
	    send_to_char("Invalid amount.\n\r", ch);
	    return;
	}


	if (amount > ch->pcdata->bankbalance)
	{
	    send_to_char("You don't have that much in the bank.\n\r", ch);
	    return;
	}

	if (amount < 0)
	{
	    send_to_char("You can't transfer a negative amount.\n\r", ch);
	    return;
	}

	ch->pcdata->bankbalance -= amount;
	target->pcdata->bankbalance += amount;
	sprintf(temp, "You have transferred {Y%ld{X gold coins to %s's bank account.\n\r", amount, target->name);
	send_to_char(temp, ch);
	sprintf(temp, "%s has transferred {Y%ld{X gold coins to your bank account.\n\r", ch->name, amount);
	send_to_char(temp, target);
	return;
    }

    send_to_char("Usage: \n\r", ch);
    send_to_char("Bank deposit <amount> \n\r", ch);
    send_to_char("Bank withdraw <amount> \n\r", ch);
    send_to_char("Bank balance \n\r", ch);
    send_to_char("Bank wire <amount> <person>\n\r", ch);
}

void do_botter(CHAR_DATA* ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    CHAR_DATA * victim;

    argument = one_argument(argument, arg);

    if (ch->tot_level < MAX_LEVEL)
    {
	send_to_char("Huh?\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char
	    ("Who do you want to mark as a botter?\n\r", ch);
	return;
    }

    victim = get_char_world(ch, arg);
    if (victim == NULL)
    {
	send_to_char
	    ("That player doesn't exist.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("That isn't a player!\n\r", ch);
	return;
    }

    if (IS_SET(victim->act[0], PLR_BOTTER))
	REMOVE_BIT(victim->act[0], PLR_BOTTER);
    else
	SET_BIT(victim->act[0], PLR_BOTTER);

    send_to_char("Flag toggled.\n\r", ch);
}


/*  @@@NIB : 20070126 : Added types */
char *get_char_where(CHAR_DATA *ch)
{
	if( IS_VALID(ch->in_room->instance_section) && IS_VALID(ch->in_room->instance_section->instance) )
	{
		INSTANCE *instance = ch->in_room->instance_section->instance;

		if( IS_VALID(instance->dungeon) )
		{
			// Prioritize the blueprint
			int area_who = instance->blueprint->area_who;

			// If blank, fall back to the dungeon index
			if( area_who == AREA_BLANK )
				area_who = instance->dungeon->index->area_who;

			// If blank, fall back to default
			if( area_who == AREA_BLANK )
				area_who = AREA_DUNGEON;

			return str_dup(flag_string(area_who_display,area_who));
		}
		else if( IS_VALID(instance->object) )
		{
			if( instance->object->item_type == ITEM_SHIP )
			{
				// When the object is a ship, it doesn't matter what the instance is labeled as
				return str_dup(flag_string(area_who_display,AREA_ON_SHIP));
			}
			else
			{
				return str_dup(flag_string(area_who_display,instance->blueprint->area_who));
			}
		}
		else
		{
			return str_dup(flag_string(area_who_display,instance->blueprint->area_who));
		}

	}

	AREA_REGION *region = get_room_region(ch->in_room);

	if (!IS_VALID(region) || region->area_who <= AREA_BLANK || region->area_who >= AREA_WHO_MAX)
		return str_dup("      ");

	return str_dup(flag_string(area_who_display,region->area_who));
}

#if 0
int get_squares_to_show_x(ROOM_INDEX_DATA *pRoom, int bonus_view)
{
    int squares_to_show_x;
    /* int storm_type; */

    squares_to_show_x = 10;

/*   storm_type = get_storm_for_room(pRoom); */

/*   switch (storm_type) {
       case WEATHER_NONE:
            squares_to_show_x = 16 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 3;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 6;
        case WEATHER_RAIN_STORM:
            squares_to_show_x = 14 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 3;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 8;
						break;
        case WEATHER_LIGHTNING_STORM:
            squares_to_show_x = 12 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 3;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 6;
						break;
        case WEATHER_SNOW_STORM:
            squares_to_show_x = 8 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 2;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 4;
						break;
        case WEATHER_HURRICANE:
            squares_to_show_x = 6 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 2;
						break;
        case WEATHER_TORNADO:
            squares_to_show_x = 6 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_x -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_x -= 2;
						break;
    }

    return squares_to_show_x;
}


int get_squares_to_show_y(ROOM_INDEX_DATA *pRoom, int bonus_view)
{
    int squares_to_show_y;
    int storm_type;

    squares_to_show_y = 10;
**
    storm_type = get_storm_for_room(pRoom);

    switch (storm_type) {
        case WEATHER_NONE:*/
            squares_to_show_y = 12 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 2;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 4;
			/*			break;
        case WEATHER_RAIN_STORM:
            squares_to_show_y = 10 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 2;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 5;
						break;
        case WEATHER_LIGHTNING_STORM:
            squares_to_show_y = 8 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 3;
						break;
        case WEATHER_SNOW_STORM:
            squares_to_show_y = 6 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 2;
						break;
        case WEATHER_HURRICANE:
            squares_to_show_y = 4 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 2;
						break;
        case WEATHER_TORNADO:
            squares_to_show_y = 4 + bonus_view;

						if (weather_info.sky == SUN_SET)
								squares_to_show_y -= 1;

						if (weather_info.sky == SUN_DARK)
								squares_to_show_y -= 2;
						break;
    }
*/
    return squares_to_show_y;
}

#endif

/* UNUSED */
char* get_wilderness_map(AREA_DATA *pArea, int lx, int ly, int bonus_view_x, int bonus_view_y)
{
    int x, y;
    long index;
    char j[6];
    char temp[6];
    int squares_to_show_x;
    int squares_to_show_y;
    bool last_char_same;
    char last_char;
    bool found = FALSE;
    char last_colour_char;
    BUFFER *output;
    char *output_string;


    /*
     * Alloc space for output lines.
     */
    output = new_buf();

    index = (long)((long)ly * pArea->map_size_x + (long)lx + pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

    squares_to_show_x = get_squares_to_show_x(bonus_view_x);
    squares_to_show_y = get_squares_to_show_y(bonus_view_y);

    last_char_same = FALSE;
    last_char = ' ';
    last_colour_char = ' ';

	  add_buf(output,"\n\r");

    for (y=ly - squares_to_show_y;
        y < ly + squares_to_show_y; y++) {
        /* sprintf(j, "%d ", ly); */

        for (x =lx - squares_to_show_x;
            x < lx + squares_to_show_x; x++) {
            /* sprintf(j, "%d ", lx); */

            index = y * pArea->map_size_x + x;

            /* sprintf(temp, "%c", map[index]);
               send_to_char(temp, ch); */
            if (x >= 0
                && x < pArea->map_size_x
                && y >= 0
                && y < pArea->map_size_y)
            {
                if (lx == x
                    && ly == y)
                {
                    sprintf(temp, "{YX{x");}
                    else
                    {
			if (!found) {

				sprintf(j, "%c",pArea->map[index]);

				/* sprintf(j, "%d ", lx); */
				if ((time_info.hour >= 7
							&& time_info.hour < 9 && x == 443
							&& y == 201)
						|| (time_info.hour >= 1
							&& time_info.hour < 3 && x == 498
							&& y == 129)
						|| (time_info.hour >= 16
							&& time_info.hour < 18 && x == 1101
							&& y == 231))
				{
					sprintf(temp, "{y@");
					found = TRUE;
				}

				/*                       if ((time_info.hour >= 9
							 && time_info.hour < 11 && x == 279
							 && y == 105)
							 || (time_info.hour >= 14
							 && time_info.hour < 16 && x == 340
							 && y == 23)
							 || (time_info.hour >= 21
							 && time_info.hour < 23 && x == 106
							 && y == 97))
							 sprintf(temp, "{CB"); */
				else
			     if (!str_cmp(pArea->name, "Netherworld")) {
           if (j[0] == 'S') {
             if (x == 68 && y == 164) {
			         sprintf(temp, "{WX");}
             else {
			         sprintf(temp, "{R~");}
           }
/* (7-20-06) Replaced mappings with Nib's - Areo */
			     else
			     if (j[0] == 'X') {
			       sprintf(temp, "{D^");}
			     else
			     if (j[0] == 'C') {
			     sprintf(temp, "{r.");}
			     else
			     if (j[0] == 'E') {
			     sprintf(temp, "{g~");}
			     else
						if (!str_cmp(j, "*")) {
							sprintf(temp, "{w^");}
						else
							if (!str_cmp(j, "P")) {
								sprintf(temp, "{y.");}
							else
								if (!str_cmp(j, "T")) {
									sprintf(temp, "{r#");}
								else
										if (!str_cmp(j, "&")) {
											sprintf(temp, "{D.");}
										else
			     if (!str_cmp(j, "1")) {
			     sprintf(temp, "{YO");}
			     else
			     if (!str_cmp(j, "F")) {
			     sprintf(temp, "{D.");}
			     else
			     if (!str_cmp(j, "H")) {
			     sprintf(temp, "{R(");}
			     else
			     if (!str_cmp(j, "A")) {
			     sprintf(temp, "{WI");}
			     if (!str_cmp(j, "M")) {
			     sprintf(temp, "{MI");}
			     else
			     if (!str_cmp(j, "V")) {
			     sprintf(temp, "{M=");}
			     }
			     else
				 if (!strcmp(j, " ")) {
				 sprintf(temp, "{Y.");}
 				 else
				 if (!strcmp(j, "!")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "#")) {
				 sprintf(temp, "{D:");}
				 else
				 if (!strcmp(j, "$")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "&")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "'")) {
				 sprintf(temp, "{G~");}
				 else
				 if (!strcmp(j, "(")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, ")")) {
				 sprintf(temp, "{g~");}
				 else
				 if (!strcmp(j, "*")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "+")) {
				 sprintf(temp, "{y.");}
				 else
				 if (!strcmp(j, ",")) {
				 sprintf(temp, "{g.");}
				 else
				 if (!strcmp(j, ".")) {
				 sprintf(temp, "{r.");}
				 else
				 if (!strcmp(j, "/")) {
				 sprintf(temp, "{D.");}
				 else
				 if (!strcmp(j, "0")) {
				 sprintf(temp, "{DO");}
				 else
				 if (!strcmp(j, "1")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, "2")) {
				 sprintf(temp, "{GO");}
				 else
				 if (!strcmp(j, "3")) {
				 sprintf(temp, "{BO");}
				 else
				 if (!strcmp(j, "4")) {
				 sprintf(temp, "{RO");}
				 else
				 if (!strcmp(j, "5")) {
				 sprintf(temp, "{WO");}
				 else
				 if (!strcmp(j, "6")) {
				 sprintf(temp, "{xO");}
				 else
				 if (!strcmp(j, "7")) {
				 sprintf(temp, "{CO");}
				 else
				 if (!strcmp(j, "8")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, "9")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, ":")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, ">")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "@")) {
				 sprintf(temp, "{W#");}
				 else
				 if (!strcmp(j, "A")) {
				 sprintf(temp, "{Y.");}
				 else
				 if (!strcmp(j, "B")) {
				 sprintf(temp, "{W*");}
				 else
				 if (!strcmp(j, "C")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "D")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "E")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "F")) {
				 sprintf(temp, "{R#");}
				 else
				 if (!strcmp(j, "G")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "H")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "I")) {
					if (!str_cmp(pArea->name, "Eden")) {
						sprintf(temp, "{WI");
				 }
				 else	 {
						 sprintf(temp, "{C~");
					 }
				 }
				 else
				 if (!strcmp(j, "J")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "K")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "L")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "M")) {
				 if (!str_cmp(pArea->name, "Eden"))
					 sprintf(temp, "{WI");
				 else
				 	 sprintf(temp, "{MI");}
				 else
				 if (!strcmp(j, "N")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "O")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "P")) {
				 sprintf(temp, "{y^");}
				 else
				 if (!strcmp(j, "Q")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "R")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "S")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "T")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "V")) {
				 if (!str_cmp(pArea->name, "Eden"))
        			 sprintf(temp, "{W=");
				 else
					 sprintf(temp, "{M=");}
				 else
				 if (!strcmp(j, "W")) {
        		 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "X")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "Y")) {
				 sprintf(temp, "{G^");}
				 else
				 if (!strcmp(j, "Z")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "[")) {
				 sprintf(temp, "{x.");}
				 else
				 if (!strcmp(j, "^")) {
				 sprintf(temp, "{g^");}
				 else
				 if (!strcmp(j, "_")) {
				 sprintf(temp, "{Y^");}
				 else
				 if (!strcmp(j, "`")) {
				 sprintf(temp, "{W~");}
				 else
				 if (!strcmp(j, "a")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "b")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "c")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "d")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "e")) {
				 sprintf(temp, "{C.");}
				 else
				 if (!strcmp(j, "f")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "g")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "h")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "i")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "j")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "k")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "l")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "m")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "n")) {
				 sprintf(temp, "{g^");}
				 else
				 if (!strcmp(j, "o")) {
				 sprintf(temp, "{D*");}
				 else
				 if (!strcmp(j, "p")) {
				 sprintf(temp, "{y#");}
				 else
				 if (!strcmp(j, "q")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "r")) {
				 sprintf(temp, "{D.");}
				 else
				 if (!strcmp(j, "s")) {
				 sprintf(temp, "{W.");}
				 else
				 if (!strcmp(j, "t")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "u")) {
				 sprintf(temp, "{M^");}
				 else
				 if (!strcmp(j, "v")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "w")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "x")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "y")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "z")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "{")) {
				 sprintf(temp, "{R~");}
				 else
				 if (!strcmp(j, "|")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "}")) {
				 sprintf(temp, "{Wo");}
			     else
			     sprintf(temp, j);}
			     }
			     if (last_char_same
				 && (temp[2] != last_char
				     || temp[1] != last_colour_char)) {
			     last_char_same = FALSE;}

			     if (temp[2] == last_char
				 && temp[1] ==
				 last_colour_char) last_char_same =
			     TRUE; if (last_char_same) {
			     /* send_to_char("{x.", to); */
			     sprintf(temp, "%c", temp[2]);}

	         add_buf(output,temp);
if (last_char_same) {
			     last_char = temp[0];}
			     else
			     {
			     last_char = temp[2];
			     last_colour_char = temp[1];}
			     }
			     else
			     {
			     if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0) {
			     last_char = '.'; last_colour_char = 'x';
	         add_buf(output,"{x.");
           }
			     else
	         add_buf(output," ");
           }
			     }
	         add_buf(output,"\n\r"); }
	         add_buf(output,"{x");

           output_string = buf_string(output);
           free_buf(output);
           return output_string;
}

#if 0
/* VIZZWILDS - Disabled legacy code. Remove once all existing wilds regions converted to new wilds format*/
void show_map_to_char(CHAR_DATA *ch, CHAR_DATA *to, int bonus_view_x, int bonus_view_y)
{
    int x, y;
    long index;
    DESCRIPTOR_DATA * d;
    bool found;
    char j[5];
    char temp[5];
    int squares_to_show_x;
    int squares_to_show_y;
    bool last_char_same;
    char last_char;
    char last_colour_char;
    OBJ_DATA *obj;

    squares_to_show_x = get_squares_to_show_x(ch->in_room, bonus_view_x);
    squares_to_show_y = get_squares_to_show_y(ch->in_room, bonus_view_y);
    last_char_same = FALSE;
    last_char = ' ';
    last_colour_char = ' ';
    send_to_char("\n\r", ch);
    for (y=ch->in_room->y - squares_to_show_y;
        y < ch->in_room->y + squares_to_show_y; y++) {
        /* sprintf(j, "%d ", ch->in_room->y); */

        for (x =ch->in_room->x - squares_to_show_x;
            x < ch->in_room->x + squares_to_show_x; x++) {
            /* sprintf(j, "%d ", ch->in_room->x); */

            index = y * ch->in_room->area->map_size_x + x;

            /* sprintf(temp, "%c", map[index]); */
            /* send_to_char(temp, ch); */
            if (x >= 0
                && x < ch->in_room->area->map_size_x
                && y >= 0
                && y < ch->in_room->area->map_size_y)
            {
                if (ch->in_room->x == x
                    && ch->in_room->y == y)
                {
                    sprintf(temp, "{M@{x");}
                    else
                    {
                        SHIP_DATA * ship; found = FALSE;
                        /* Check for sailing ships */
			for (ship = ((AREA_DATA *)
						get_sailing_boat_area())->
					ship_list; ship != NULL;
					ship = ship->next)
			{
				if (ch->in_room->ship != ship
						&& can_see_obj(ch, ship->ship)
						&& ship->ship->in_room->x == x
						&& ship->ship->in_room->y == y)
				{
					if (!str_cmp(ship->owner_name, ch->name))
						sprintf(temp, "{Y@");
					else
						sprintf(temp, "{y@"); found = TRUE;
				}
				else
					if (can_see_obj(ch, ship->ship) &&
							((ship->last_room[0] != NULL && ship->last_room[0]->x == x && ship->last_room[0]->y == y) ||
							 (ship->last_room[1] != NULL && ship->last_room[1]->x == x && ship->last_room[1]->y == y) ||
							 (ship->last_room[2] != NULL && ship->last_room[2]->x == x && ship->last_room[2]->y == y)))

					{
						sprintf(temp, "{C~"); found = TRUE;
					}
			}
			for (d = descriptor_list; d != NULL;d = d->next)
			{
				if (d->connected == CON_PLAYING
						&& d->character != ch
						&& can_see(ch, d->character)
						&& IN_WILDERNESS(d->character)
						&& d->character->in_room->x == x
						&& d->character->in_room->y == y)
				{
					/*   sprintf(temp, "%s %ld\n\r", d->character->name, d->character->in_room->vnum);
					  gecho(temp); */
					sprintf(temp, "{W@"); found = TRUE;
				}
			}
			if (!found) {
				AREA_DATA *pArea;

				pArea = ch->in_room->area;

				sprintf(j, "%c",pArea->map[index]);
				/* sprintf(j, "%d ", ch->in_room->x); */
				if ((time_info.hour >= 7
							&& time_info.hour < 9 && x == 443
							&& y == 201)
						|| (time_info.hour >= 1
							&& time_info.hour < 3 && x == 498
							&& y == 129)
						|| (time_info.hour >= 16
							&& time_info.hour < 18 && x == 1101
							&& y == 231))
				{
					sprintf(temp, "{y@");
					found = TRUE;
				}

				/*                       if ((time_info.hour >= 9
							 && time_info.hour < 11 && x == 279
							 && y == 105)
							 || (time_info.hour >= 14
							 && time_info.hour < 16 && x == 340
							 && y == 23)
							 || (time_info.hour >= 21
							 && time_info.hour < 23 && x == 106
							 && y == 97))
							 sprintf(temp, "{CB"); */
				else
					if (IN_NETHERWORLD(ch)) {
           if (j[0] == 'S') {
             if (x == 68 && y == 164) {
			         sprintf(temp, "{WX");}
             else {
			         sprintf(temp, "{R~");}
           }
/* (7-20-06) Replaced mappings with Nib's (Areo) */
			     else
			     if (j[0] == 'X') {
			       sprintf(temp, "{D^");}
			     else
			     if (j[0] == 'C') {
			     sprintf(temp, "{r.");}
			     else
			     if (j[0] == 'E') {
			     sprintf(temp, "{g~");}
			     else
						if (!str_cmp(j, "*")) {
							sprintf(temp, "{w^");}
						else
							if (!str_cmp(j, "P")) {
								sprintf(temp, "{y.");}
							else
								if (!str_cmp(j, "T")) {
									sprintf(temp, "{r#");}
								else
										if (!str_cmp(j, "&")) {
											sprintf(temp, "{D.");}
										else
			     if (!str_cmp(j, "1")) {
			     sprintf(temp, "{YO");}
			     else
			     if (!str_cmp(j, "F")) {
			     sprintf(temp, "{D.");}
			     else
			     if (!str_cmp(j, "H")) {
			     sprintf(temp, "{R(");}
			     else
			     if (!str_cmp(j, "A")) {
			     sprintf(temp, "{WI");}
			     if (!str_cmp(j, "M")) {
			     sprintf(temp, "{MI");}
			     else
			     if (!str_cmp(j, "V")) {
			     sprintf(temp, "{M=");}
			     }
			     else
				 if (!strcmp(j, " ")) {
				 sprintf(temp, "{Y.");}
 				 else
				 if (!strcmp(j, "!")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "#")) {
				 sprintf(temp, "{D:");}
				 else
				 if (!strcmp(j, "$")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "&")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "'")) {
				 sprintf(temp, "{G~");}
				 else
				 if (!strcmp(j, "(")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, ")")) {
				 sprintf(temp, "{g~");}
				 else
				 if (!strcmp(j, "*")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "+")) {
				 sprintf(temp, "{y.");}
				 else
				 if (!strcmp(j, ",")) {
				 sprintf(temp, "{g.");}
				 else
				 if (!strcmp(j, ".")) {
				 sprintf(temp, "{r.");}
				 else
				 if (!strcmp(j, "/")) {
				 sprintf(temp, "{D.");}
				 else
				 if (!strcmp(j, "0")) {
				 sprintf(temp, "{DO");}
				 else
				 if (!strcmp(j, "1")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, "2")) {
				 sprintf(temp, "{GO");}
				 else
				 if (!strcmp(j, "3")) {
				 sprintf(temp, "{BO");}
				 else
				 if (!strcmp(j, "4")) {
				 sprintf(temp, "{RO");}
				 else
				 if (!strcmp(j, "5")) {
				 sprintf(temp, "{WO");}
				 else
				 if (!strcmp(j, "6")) {
				 sprintf(temp, "{xO");}
				 else
				 if (!strcmp(j, "7")) {
				 sprintf(temp, "{CO");}
				 else
				 if (!strcmp(j, "8")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, "9")) {
				 sprintf(temp, "{YO");}
				 else
				 if (!strcmp(j, ":")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, ">")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "@")) {
				 sprintf(temp, "{W#");}
				 else
				 if (!strcmp(j, "A")) {
				 sprintf(temp, "{Y.");}
				 else
				 if (!strcmp(j, "B")) {
				 sprintf(temp, "{W*");}
				 else
				 if (!strcmp(j, "C")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "D")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "E")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "F")) {
				 sprintf(temp, "{R#");}
				 else
				 if (!strcmp(j, "G")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "H")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "I")) {
					if (!str_cmp(pArea->name, "Eden")) {
						sprintf(temp, "{WI");
				 }
				 else	 {
						 sprintf(temp, "{C~");
					 }
				 }
				 else
				 if (!strcmp(j, "J")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "K")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "L")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "M")) {
				 if (!str_cmp(pArea->name, "Eden"))
					 sprintf(temp, "{WI");
				 else
				 	 sprintf(temp, "{MI");}
				 else
				 if (!strcmp(j, "N")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "O")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "P")) {
				 sprintf(temp, "{y^");}
				 else
				 if (!strcmp(j, "Q")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "R")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "S")) {
				 sprintf(temp, "{b~");}
				 else
				 if (!strcmp(j, "T")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "V")) {
				 if (!str_cmp(pArea->name, "Eden"))
        			 sprintf(temp, "{W=");
				 else
					 sprintf(temp, "{M=");}
				 else
				 if (!strcmp(j, "W")) {
        		 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "X")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "Y")) {
				 sprintf(temp, "{G^");}
				 else
				 if (!strcmp(j, "Z")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "[")) {
				 sprintf(temp, "{x.");}
				 else
				 if (!strcmp(j, "^")) {
				 sprintf(temp, "{g^");}
				 else
				 if (!strcmp(j, "_")) {
				 sprintf(temp, "{Y^");}
				 else
				 if (!strcmp(j, "`")) {
				 sprintf(temp, "{W~");}
				 else
				 if (!strcmp(j, "a")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "b")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "c")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "d")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "e")) {
				 sprintf(temp, "{C.");}
				 else
				 if (!strcmp(j, "f")) {
				 sprintf(temp, "{g*");}
				 else
				 if (!strcmp(j, "g")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "h")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "i")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "j")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "k")) {
				 sprintf(temp, "{C~");}
				 else
				 if (!strcmp(j, "l")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "m")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "n")) {
				 sprintf(temp, "{g^");}
				 else
				 if (!strcmp(j, "o")) {
				 sprintf(temp, "{D*");}
				 else
				 if (!strcmp(j, "p")) {
				 sprintf(temp, "{y#");}
				 else
				 if (!strcmp(j, "q")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "r")) {
				 sprintf(temp, "{D.");}
				 else
				 if (!strcmp(j, "s")) {
				 sprintf(temp, "{W.");}
				 else
				 if (!strcmp(j, "t")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "u")) {
				 sprintf(temp, "{M^");}
				 else
				 if (!strcmp(j, "v")) {
				 sprintf(temp, "{B~");}
				 else
				 if (!strcmp(j, "w")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "x")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "y")) {
				 sprintf(temp, "{x^");}
				 else
				 if (!strcmp(j, "z")) {
				 sprintf(temp, "{D^");}
				 else
				 if (!strcmp(j, "{")) {
				 sprintf(temp, "{R~");}
				 else
				 if (!strcmp(j, "|")) {
				 sprintf(temp, "{G*");}
				 else
				 if (!strcmp(j, "}")) {
				 sprintf(temp, "{Wo");}
			     else
			     sprintf(temp, j);}
			     }

          /* Check light */

          if ((weather_info.sunlight == SUN_DARK ||
               weather_info.sunlight == SUN_SET) &&
            !IS_IMMORTAL(ch) &&
	          (obj = get_eq_char(ch, WEAR_LIGHT)) != NULL &&
            obj->item_type == ITEM_LIGHT) {
					  int distance = 0;
            int visible_radius = 0;
            int light = (obj->value[2] == -1) ? 100 : obj->value[2];

            if (weather_info.sunlight == SUN_SET) {
	            visible_radius = URANGE(6, light, 10);
            }
            else {
	            visible_radius = URANGE(1, light, 8);
						}

					  distance = (int) sqrt( 					\
									( x - ch->in_room->x ) *	\
									( x - ch->in_room->x ) +	\
									( y - ch->in_room->y ) *	\
									( y - ch->in_room->y ) );

           if (distance == visible_radius) {
             /* Dark grey edge */
             temp[1] = 'D';
           }
           else
           if (distance > visible_radius) {
             temp[2] = ' ';
           }
          }
			     if (last_char_same
				 && (temp[2] != last_char
				     || temp[1] != last_colour_char)) {
			     last_char_same = FALSE;}

			     if (temp[2] == last_char
				 && temp[1] ==
				 last_colour_char) last_char_same =
			     TRUE; if (last_char_same) {
			     /* send_to_char("{x.", to); */
			     sprintf(temp, "%c", temp[2]);}

			     send_to_char(temp, to); if (last_char_same) {
			     last_char = temp[0];}
			     else
			     {
			     last_char = temp[2];
			     last_colour_char = temp[1];}
			     }
			     else
			     {
			     if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0) {
			     last_char = '.'; last_colour_char = 'x';
			     send_to_char("{x.", to);}
			     else
			     send_to_char(" ", to);}
			     }
			     send_to_char("\n\r", to);}
			     send_to_char("{x", to);
}
#endif

void do_scry(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char buf[MSL];
	CHAR_DATA *victim;
	BUFFER *buffer;
	bool found = FALSE, local;
	int count;
	ITERATOR vit;

	argument = one_argument(argument, arg);

	if (!get_skill(ch, gsk_scry)) {
		send_to_char("You know nothing of this skill.\n\r", ch);
		return;
	}

	if(!arg[0]) {
		send_to_char("Scry whom?\n\r", ch);
		return;
	}

	if (!get_char_world(ch, arg)) {
		act("You sense no $T in the world.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);
		return;
	}

	local = !str_cmp("local",argument);

	if (ch->mana < 50) {
		send_to_char("You can't gather enough energy.\n\r", ch);
		return;
	}

	ch->mana -= 50;

	send_to_char("{MYou concentrate, extending your aura to the rest of the world...{x\n\r", ch);

	check_improve(ch,gsk_scry,TRUE,5);
	buffer = new_buf();
	count = 0;
	int skill = get_skill(ch, gsk_scry);
	iterator_start(&vit, loaded_chars);
	while(( victim = (CHAR_DATA *)iterator_nextdata(&vit)))
	{
		AREA_REGION *region = get_room_region(victim->in_room);

		if (IS_VALID(region) && victim->in_room->area->open && is_name(arg, victim->name) && can_see(ch, victim) &&
			!IS_VALID(victim->in_room->instance_section) &&
			IS_NPC(victim) && number_percent() < skill &&
			(!local || ch->in_room->area == victim->in_room->area) &&
			(	(region->place_flags == PLACE_FIRST_CONTINENT) ||
				(region->place_flags == PLACE_SECOND_CONTINENT) ||
				(region->place_flags == PLACE_THIRD_CONTINENT) ||
				(region->place_flags == PLACE_FOURTH_CONTINENT) ||
				(region->place_flags == PLACE_ISLAND) ||
				(region->place_flags == PLACE_WILDERNESS) ||
				!str_cmp(victim->in_room->area->name, "Wilderness"))) {
			found = TRUE;
			count++;
			sprintf(buf, "One is in %s (%s)\n\r", victim->in_room->name, victim->in_room->area->name);
			if(!add_buf(buffer,buf)) {
				send_to_char("There are too many souls in the world to focus on what you are looking for.\n\r", ch);
				send_to_char("Try narrowing your search.\n\r", ch);
				free_buf(buffer);
				return;
			}
		}
	}
	iterator_stop(&vit);

	if (!found)
		act("You sense no $T in the world.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);
	else
		page_to_char(buf_string(buffer),ch);

	free_buf(buffer);
}

void show_map_and_description(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    char *tmp;
    char buf[MAX_STRING_LENGTH];
    char map[MAX_STRING_LENGTH];
    int counter;
    int linelength;

    int line;

    counter = 0;
    linelength = 0;
    line = 1;

    buf[0] = '\0';

    create_map(ch, room, &map[0]);
    /* send_to_char(map, ch); */
    counter = show_map(ch, &buf[0], &map[0], counter, line++);

    tmp = find_desc_for_room(room,ch);/* ch->in_room->description; */
    while(*tmp != '\0')
    {
	if (*tmp != '\r' && *tmp != '\n')
        {
	    buf[counter++] = *tmp;
	    linelength++;
	    if (linelength > 50 && *tmp == ' ')
            {
		buf[counter++] = '\n';
                buf[counter++] = '\r';

                while(*(tmp+1) == ' ')
		    tmp++;

                if (line <= 7)
                {
	            counter = show_map(ch, &buf[0], &map[0], counter, line++);
                }
		else {
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
			buf[counter++] = ' ';
		}

		linelength = 0;
	    }
        }
	else
        {
	    buf[counter++] = ' ';
        }
	tmp++;
    }

    buf[counter++] = '\n';
    buf[counter++] = '\r';

    while (line <= 7)
    {
	counter = show_map(ch, &buf[0], &map[0], counter, line);

	if (line <= 7) {
	    buf[counter++] = '\n';
            buf[counter++] = '\r';
	}
	line++;
    }

    buf[counter++] = '\0';

    send_to_char(buf, ch);
}

int show_map(CHAR_DATA * ch, char *buf, char *map, int counter, int line)
{
	char buf2[4];

	if (line == 1 || line == 7) {
		memcpy(buf+counter,"{B+{b----------{B+{x",20);
		counter += 20;
	} else {
		int count;
		memcpy(buf+counter,"{b|",3);
		counter += 3;
		for (count = ((line-1) * 10); count < (((line)) * 10); count++) {
			convert_map_char(buf2, map[count]);

			memcpy(buf+counter,buf2,3);
			counter += 3;
		}
		memcpy(buf+counter,"{b|{x ",6);
		counter += 6;
	}

	return counter;
}


void create_map(CHAR_DATA *ch, ROOM_INDEX_DATA *start_room, char *map)
{
    ROOM_INDEX_DATA *room;
    ROOM_INDEX_DATA *last_room;
    ROOM_INDEX_DATA *temp;
    ROOM_INDEX_DATA *temp2;
    int x, y;
    int counter;
    int x2;
    int y2;

    x = 5;
    y = 3;

/* ok - looks like we're clearing the string. Using a memcpy would be way faster. */
    for (counter = 0; counter < 100; counter++)
    {
        *(map + counter) = ' ';
    }

/* Terminating the string with a null char. */
    *(map + 101) = '\0';
/* Hmm - placing the char in the centre? */
    *(map + 10 * y + x) = '@';

/* Before we check the adjacent rooms, keep track of where we came from */
    last_room = start_room;

/* Check north */
    while(last_room->exit[ DIR_NORTH ] != NULL
          && y > 1
          && is_exit_visible(ch, last_room, DIR_NORTH))
    {
        y--;

        *(map + 10 * y + x) = '|';

        if( IS_SET(last_room->exit[ DIR_NORTH ]->exit_info, EX_PREVFLOOR) )
        {
        	*(map + 10 * (y-1) + x) = '<';
        	break;
		}

        if( IS_SET(last_room->exit[ DIR_NORTH ]->exit_info, EX_NEXTFLOOR) )
        {
        	*(map + 10 * (y-1) + x) = '>';
        	break;
		}

	if ((room = last_room->exit[ DIR_NORTH ]->u1.to_room)==NULL)
            break;

      	last_room = room;
        y--;
        *(map + 10 * y + x) = determine_room_type(room);

        /* Look east */
        temp = room;
        x2 = x;
        y2 = y;

        while(temp->exit[ DIR_EAST ] != NULL && x2 < 8
              && is_exit_visible(ch, temp, DIR_EAST))
        {
            *(map + 10 * y2 + x2 + 1) = '-';

			if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * y2 + x2 + 2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * y2 + x2 + 2) = '>';
				break;
			}

            if ((temp = temp->exit[ DIR_EAST ]->u1.to_room)==NULL)
                break;

            *(map + 10 * y2 + x2 + 2) = determine_room_type(temp);
            x2++;
            x2++;

            if (temp->exit[ DIR_SOUTH ] != NULL
                && is_exit_visible(ch, temp, DIR_SOUTH))
            {

                *(map + 10 * (y2+1) + x2) = '|';

				if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_PREVFLOOR) )
				{
					*(map + 10 * (y2+2) + x2) = '<';
					break;
				}

				if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_NEXTFLOOR) )
				{
					*(map + 10 * (y2+2) + x2) = '>';
					break;
				}

                if ((temp2 = temp->exit[ DIR_SOUTH ]->u1.to_room)==NULL)
                    break;

                *(map + 10 * (y2+2) + x2) = determine_room_type(temp2);
            }
        }

        /* Look west */
        temp = room;
        x2 = x;
        y2 = y;

        while(temp->exit[ DIR_WEST ] != NULL
              && x2 > 1
              && is_exit_visible(ch, temp, DIR_WEST))
        {
            *(map + 10 * y2 + x2 - 1) = '-';

			if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * y2 + x2 - 2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * y2 + x2 - 2) = '>';
				break;
			}


            if ((temp = temp->exit[ DIR_WEST ]->u1.to_room)==NULL)
                break;

            *(map + 10 * y2 + x2 - 2) = determine_room_type(temp);
            x2--;
            x2--;

            if (temp->exit[ DIR_SOUTH ] != NULL
                && is_exit_visible(ch, temp, DIR_SOUTH))
            {
                *(map + 10 * (y2+1) + x2) = '|';

				if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_PREVFLOOR) )
				{
					*(map + 10 * (y2+2) + x2) = '<';
					break;
				}

				if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_NEXTFLOOR) )
				{
					*(map + 10 * (y2+2) + x2) = '>';
					break;
				}

                if ((temp2 = temp->exit[ DIR_SOUTH ]->u1.to_room)==NULL)
                    break;

                *(map + 10 * (y2+2) + x2) = determine_room_type(temp2);
            }
        }
    }

    x = 5;
    y = 3;

    /* work out south */
    last_room = start_room;

    while(last_room->exit[ DIR_SOUTH ] != NULL
          && y < 5
          && is_exit_visible(ch, last_room, DIR_SOUTH))
    {
        y++;

        /* Intermediate char */
        *(map + 10 * y + x) = '|';

        if( IS_SET(last_room->exit[ DIR_SOUTH ]->exit_info, EX_PREVFLOOR) )
        {
        	*(map + 10 * (y+1) + x) = '<';
        	break;
		}

        if( IS_SET(last_room->exit[ DIR_SOUTH ]->exit_info, EX_NEXTFLOOR) )
        {
        	*(map + 10 * (y+1) + x) = '>';
        	break;
		}

        if ((room = last_room->exit[ DIR_SOUTH ]->u1.to_room)==NULL)
            break;

        last_room = room;
        y++;
        *(map + 10 * y + x) = determine_room_type(room);

        /* Look east */
        temp = room;
        x2 = x;
        y2 = y;

        while(temp->exit[ DIR_EAST ] != NULL
              && x2 < 8
              && is_exit_visible(ch, temp, DIR_EAST))
        {
            *(map + 10 * y2 + x2 + 1) = '-';

			if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * y2 + x2 + 2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * y2 + x2 + 2) = '>';
				break;
			}

            if ((temp = temp->exit[ DIR_EAST ]->u1.to_room)==NULL)
                break;

            *(map + 10 * y2 + x2 + 2) = determine_room_type(temp);
            x2++;
            x2++;

            if (temp->exit[ DIR_NORTH ] != NULL
                && is_exit_visible(ch, temp, DIR_NORTH))
            {
                *(map + 10 * (y2-1) + x2) = '|';

				if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_NEXTFLOOR) )
				{
					*(map + 10 * (y2-2) + x2) = '>';
					break;
				}
				if( IS_SET(temp->exit[ DIR_NORTH]->exit_info, EX_PREVFLOOR) )
				{
					*(map + 10 * (y2-2) + x2) = '<';
					break;
				}

                if ((temp2 = temp->exit[ DIR_NORTH ]->u1.to_room)==NULL)
                    break;

                *(map + 10 * (y2-2) + x2) = determine_room_type(temp2);
            }
        }

        /* Look west */
        temp = room;
        x2 = x;
        y2 = y;

        while(temp->exit[ DIR_WEST ] != NULL
              && x2 > 1
              && is_exit_visible(ch, temp, DIR_WEST))
        {
            *(map + 10 * y2 + x2 - 1) = '-';

			if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * y2 + x2 - 2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * y2 + x2 - 2) = '>';
				break;
			}

            if ((temp = temp->exit[ DIR_WEST ]->u1.to_room)== NULL)
                break;

            *(map + 10 * y2 + x2 - 2) = determine_room_type(temp);
            x2--;
            x2--;

            if (temp->exit[ DIR_NORTH ] != NULL
                && is_exit_visible(ch, temp, DIR_NORTH))
            {
                *(map + 10 * (y2-1) + x2) = '|';

				if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_NEXTFLOOR) )
				{
					*(map + 10 * (y2-2) + x2) = '>';
					break;
				}
				if( IS_SET(temp->exit[ DIR_NORTH]->exit_info, EX_PREVFLOOR) )
				{
					*(map + 10 * (y2-2) + x2) = '<';
					break;
				}

                if ((temp2 = temp->exit[ DIR_NORTH ]->u1.to_room)==NULL)
                    break;

                *(map + 10 * (y2-2) + x2) = determine_room_type(temp2);
            }
        }
    }

    x = 5;
    y = 3;

    /* Look east */
    temp = start_room;
    x2 = x;
    y2 = y;

    while(temp->exit[ DIR_EAST ] != NULL
          && x2 < 8
          && is_exit_visible(ch, temp, DIR_EAST))
    {
        *(map + 10 * y2 + x2 + 1) = '-';

        if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_PREVFLOOR) )
        {
        	*(map + 10 * y2 + x2 + 2) = '<';
        	break;
		}

        if( IS_SET(temp->exit[ DIR_EAST ]->exit_info, EX_NEXTFLOOR) )
        {
        	*(map + 10 * y2 + x2 + 2) = '>';
        	break;
		}

        if ((temp = temp->exit[ DIR_EAST ]->u1.to_room)==NULL)
            break;

        *(map + 10 * y2 + x2 + 2) = determine_room_type(temp);
        x2++;
        x2++;

        if (temp->exit[ DIR_SOUTH ] != NULL
            && is_exit_visible(ch, temp, DIR_SOUTH))
        {
            *(map + 10 * (y2+1) + x2) = '|';

			if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * (y2+2) + x2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * (y2+2) + x2) = '>';
				break;
			}

            if ((temp2 = temp->exit[ DIR_SOUTH ]->u1.to_room)==NULL)
                break;

            *(map + 10 * (y2+2) + x2) = determine_room_type(temp2);
        }

        if (temp->exit[ DIR_NORTH ] != NULL
            && is_exit_visible(ch, temp, DIR_NORTH))
        {
            *(map + 10 * (y2-1) + x2) = '|';

			if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * (y2-2) + x2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * (y2-2) + x2) = '>';
				break;
			}

            if ((temp2 = temp->exit[ DIR_NORTH ]->u1.to_room)==NULL)
                break;

            *(map + 10 * (y2-2) + x2) = determine_room_type(temp2);
        }
    }

    x = 5;
    y = 3;

    /* Look west */
    temp = start_room;
    x2 = x;
    y2 = y;

    while(temp->exit[ DIR_WEST ] != NULL
          && x2 > 1
          && is_exit_visible(ch, temp, DIR_WEST))
    {
        *(map + 10 * y2 + x2 - 1) = '-';

        if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_PREVFLOOR) )
        {
        	*(map + 10 * y2 + x2 - 2) = '<';
        	break;
		}

        if( IS_SET(temp->exit[ DIR_WEST ]->exit_info, EX_NEXTFLOOR) )
        {
        	*(map + 10 * y2 + x2 - 2) = '>';
        	break;
		}


        if ((temp = temp->exit[ DIR_WEST ]->u1.to_room)==NULL)
            break;

        *(map + 10 * y2 + x2 - 2) = determine_room_type(temp);
        x2--;
        x2--;

        if (temp->exit[ DIR_SOUTH ] != NULL
            && is_exit_visible(ch, temp, DIR_SOUTH))
        {
            *(map + 10 * (y2+1) + x2) = '|';

			if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * (y2+2) + x2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_SOUTH ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * (y2+2) + x2) = '>';
				break;
			}

            if ((temp2 = temp->exit[ DIR_SOUTH ]->u1.to_room)==NULL)
                break;

            *(map + 10 * (y2+2) + x2) = determine_room_type(temp2);
        }

        if (temp->exit[ DIR_NORTH ] != NULL
            && is_exit_visible(ch, temp, DIR_NORTH))
        {
            *(map + 10 * (y2-1) + x2) = '|';

			if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_PREVFLOOR) )
			{
				*(map + 10 * (y2-2) + x2) = '<';
				break;
			}

			if( IS_SET(temp->exit[ DIR_NORTH ]->exit_info, EX_NEXTFLOOR) )
			{
				*(map + 10 * (y2-2) + x2) = '>';
				break;
			}

            if ((temp2 = temp->exit[ DIR_NORTH ]->u1.to_room)==NULL)
                break;

            *(map + 10 * (y2-2) + x2) = determine_room_type(temp2);
        }
    }
}

char determine_room_type(ROOM_INDEX_DATA *room)
{
    CHAR_DATA *ch;
    MOB_INDEX_DATA *pMob;

    if (room == NULL)
	return '@';

    if (IS_SET(room->room_flag[0], ROOM_CHAOTIC))
	return 'K';
    if (room_is_dark(room))
	return 'D';
    if (IS_SET(room->room_flag[0], ROOM_MOUNT_SHOP))
	return 'M';
    if (IS_SET(room->room_flag[0], ROOM_BANK))
	return 'B';
    if (IS_SET(room->room_flag[0], ROOM_PK))
	return 'V';
    if (IS_SET(room->room_flag[0], ROOM_DEATH_TRAP))
	return 'A';
    if (IS_SET(room->room_flag[0], ROOM_LOCKER))
	return 'L';
    if (IS_SET(room->room_flag[1], ROOM_POST_OFFICE))
	return 'P';

    for (ch = room->people; ch != NULL; ch = ch->next_in_room) {
	pMob = ch->pIndexData;
	if (pMob == NULL) {
	    continue;
	}

	if (ch->shop != NULL && !IS_SET(ch->shop->flags,SHOPFLAG_HIDE_SHOP)) {
	    return 'N';
	}
    }

    if (IS_SET(room->room_flag[0], ROOM_SAFE))
	return 'S';

    return 'R';
}


void convert_map_char(char *buf, char ch)
{
    switch(ch) {
	default:
	    *(buf++) = '{';
	    *(buf++) = 'W';
	    *(buf++) = ch;
	    break;
	case 'N':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'O';
	    break;
	case 'M':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'O';
	    break;
	case 'O':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'P';
	    break;
	case 'L':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'L';
	    break;
	case 'A':
	    *(buf++) = '{';
	    *(buf++) = 'M';
	    *(buf++) = 'O';
	    break;
	case 'K':
	    *(buf++) = '{';
	    *(buf++) = 'r';
	    *(buf++) = 'O';
	    break;
	case 'V':
	    *(buf++) = '{';
	    *(buf++) = 'R';
	    *(buf++) = 'O';
	    break;
	case 'B':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'B';
	    break;
	case 'P':
	    *(buf++) = '{';
	    *(buf++) = 'G';
	    *(buf++) = 'O';
	    break;
	case 'D':
	    *(buf++) = '{';
	    *(buf++) = 'D';
	    *(buf++) = 'O';
	    break;
	case 'S':
	    *(buf++) = '{';
	    *(buf++) = 'W';
	    *(buf++) = 'S';
	    break;
	case '-':
	    *(buf++) = '{';
	    *(buf++) = 'B';
	    *(buf++) = '-';
	    break;
	case '|':
	    *(buf++) = '{';
	    *(buf++) = 'B';
	    *(buf++) = '|';
	    break;
	case 'R':
	    *(buf++) = '{';
	    *(buf++) = 'Y';
	    *(buf++) = 'O';
	    break;
	case '@':
	    *(buf++) = '{';
	    *(buf++) = 'M';
	    *(buf++) = '@';
	    break;
	case '>':
	    *(buf++) = '{';
	    *(buf++) = 'Y';
	    *(buf++) = '>';
	    break;
	case '<':
	    *(buf++) = '{';
	    *(buf++) = 'Y';
	    *(buf++) = '<';
	    break;
    }
}

void do_toxins(CHAR_DATA *ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    int i, n;

    if (!IS_SITH(ch)) {
	send_to_char("Huh?\n\r", ch);
	return;
    }

    for (i = 0; i < MAX_TOXIN; i++)
    {
	sprintf(buf2, "{x");
	for (n = 0; n < ch->toxin[i]; n += 10)
	{
	         if (n < 20) strcat(buf2, "{b<");
	    else if (n < 40) strcat(buf2, "{B<");
	    else if (n < 60) strcat(buf2, "{r<");
	    else if (n < 80) strcat(buf2, "{R<");
	    else             strcat(buf2, "{W<");
	}

	sprintf(buf, "%-12s {Y(%3d%%){x: %s{x\n\r",
	    toxin_table[i].name,
	    ch->toxin[i],
	    buf2);
	buf[0] = UPPER(buf[0]);
	send_to_char(buf, ch);
    }
}

#define LOCALE_SAME_ROOM		0		// They are in the same room
#define LOCALE_SAME_REGION		10		// Same region, different room
#define LOCALE_SAME_AREA		20		// Same area, different region
#define LOCALE_SAME_SECTION		30		// Same instance section, different room
#define LOCALE_SAME_INSTANCE	40		// Same instance, different section
#define LOCALE_SAME_DUNGEON		50		// Same dungeon, different instance
#define LOCALE_SAME_CONTINENT	60		// Same continent, different area (or in wilds)
#define LOCALE_SAME_WILDS_REGION	63	// Same wilderness region, different room
#define LOCALE_SAME_WILDS		65		// Same wilderness map, different region
#define LOCALE_SAME_REALM		70		// Same realm, different continent/wilds
#define LOCALE_ANYWHERE			100		// Different realm altogether
#define LOCALE_NOWHERE			-1		// Nowhere to be found
#define LOCALE_IN_DUNGEON		-2		// They are in a dungeon that you are not in
#define LOCALE_IN_INSTANCE		-3		// They are in an instance (not dungeon) that you are not in
#define LOCALE_IN_WILDS			-4		// They are in a wilderness that you are not in
#define LOCALE_IN_CLONE_ROOM	-5		// In an explicit clone room not in an instance

int relative_locale(CHAR_DATA *ch, CHAR_DATA *victim)
{
	if (IS_SET(victim->in_room->room_flag[0], ROOM_NOWHERE))
		return LOCALE_NOWHERE;

	AREA_REGION *region_v = get_room_region(victim->in_room);
	AREA_REGION *region_c = get_room_region(ch->in_room);

	if (IS_VALID(region_v) && region_v->place_flags == PLACE_NOWHERE)
		return LOCALE_NOWHERE;

	if (ch->in_room == victim->in_room)
		return LOCALE_SAME_ROOM;
	
	if (ch->in_room->area == victim->in_room->area)
	{
		INSTANCE_SECTION *section = ch->in_room->instance_section;
		INSTANCE_SECTION *section2 = victim->in_room->instance_section;

		if (IS_VALID(section))
		{
			// Player is in an instance, so check for SECTION, INSTANCE, DUNGEON
			if (IS_VALID(section2))
			{
				if (section == section2)
				{
					// They are in the same instance section, but not in the same room
					return LOCALE_SAME_SECTION;
				}
				else
				{
					INSTANCE *instance = section->instance;
					INSTANCE *instance2 = section2->instance;

					if (instance == instance2)
					{
						return LOCALE_SAME_INSTANCE;
					}
					else
					{
						DUNGEON *dungeon = IS_VALID(instance) ? instance->dungeon : NULL;
						DUNGEON *dungeon2 = IS_VALID(instance2) ? instance2->dungeon : NULL;

						if (IS_VALID(dungeon))
						{
							if (dungeon == dungeon2)
							{
								return LOCALE_SAME_DUNGEON;
							}
							else
							{
								// Not the same dungeon
								return LOCALE_IN_DUNGEON;
							}
						}
						else
						{
							// In a dungeon
							if (IS_VALID(dungeon2))
								return LOCALE_IN_DUNGEON;

							// In an instance that isn't a dungeon
							return LOCALE_IN_INSTANCE;
						}
					}
				}
			}
			else
			{
				// We are in an instance, they are in wilderness
				if (IN_WILDERNESS(victim))
					return LOCALE_IN_WILDS;

				// We are in an instance, but they are not, but in the same area
				return LOCALE_SAME_AREA;
			}
		}
		else
		{
			// We are not in an instance of any kind
			if (IS_VALID(section2))
			{
				// They are in an instance
				if (IS_VALID(section2->instance))
				{

					if(IS_VALID(section2->instance->dungeon))
						return LOCALE_IN_DUNGEON;
				}

				return LOCALE_IN_INSTANCE;
			}

			if (IN_WILDERNESS(ch))
			{
				if (IN_WILDERNESS(victim))
				{
					if (ch->in_wilds == victim->in_wilds)
					{
						WILDS_REGION *region = get_region_by_coors(ch->in_wilds, ch->in_room->x, ch->in_room->y);
						WILDS_REGION *region2 = get_region_by_coors(victim->in_wilds, victim->in_room->x, victim->in_room->y);

						if (IS_VALID(region) && IS_VALID(region2) && region == region2)
						{
							return LOCALE_SAME_WILDS_REGION;
						}

						// Both in the default region
						if (!region && !region2)
						{
							return LOCALE_SAME_WILDS_REGION;
						}

						return LOCALE_SAME_WILDS;
					}
					else
					{
						return LOCALE_IN_WILDS;
					}
				}
			}
			else if (IN_WILDERNESS(victim))
				return LOCALE_IN_WILDS;
			
			if (victim->in_room->source)
				return LOCALE_IN_CLONE_ROOM;

			// Check they are in the same region, but not the default region (that cascades down to the same area)
			if (IS_VALID(region_v) && IS_VALID(region_c) && region_v == region_c && region_c->uid > 0)
				return LOCALE_SAME_REGION;

			return LOCALE_SAME_AREA;
		}
	}
	else
	{
		// Different areas entirely
		INSTANCE_SECTION *section2 = victim->in_room->instance_section;

		if (IS_VALID(section2))
		{
			// They are in an instance
			if (IS_VALID(section2->instance))
			{
				if(IS_VALID(section2->instance->dungeon))
					return LOCALE_IN_DUNGEON;
			}

			return LOCALE_IN_INSTANCE;
		}

		if (IN_WILDERNESS(ch))
		{
			WILDS_REGION *region = get_region_by_coors(ch->in_wilds, ch->in_room->x, ch->in_room->y);
			long place = IS_VALID(region) ? region->area_place_flags : ch->in_wilds->defaultPlaceFlags;

			// If they are in wilderness, it won't be the same one we are in as we are in different areas
			if (IN_WILDERNESS(victim))
			{
				// Compare place flags
				WILDS_REGION *region2 = get_region_by_coors(victim->in_wilds, victim->in_room->x, victim->in_room->y);

				long place2 = IS_VALID(region2) ? region2->area_place_flags : victim->in_wilds->defaultPlaceFlags;

				if (place == place2)
					return LOCALE_SAME_CONTINENT;

				return LOCALE_IN_WILDS;
			}
			else
			{
				if (victim->in_room->source)
					return LOCALE_IN_CLONE_ROOM;

				if (IS_VALID(region_v) && place == region_v->place_flags)
					return LOCALE_SAME_CONTINENT;
			}

			return LOCALE_ANYWHERE;
		}
		else
		{
			if (IN_WILDERNESS(victim))
			{
				// Compare place flags
				WILDS_REGION *region2 = get_region_by_coors(victim->in_wilds, victim->in_room->x, victim->in_room->y);

				long place2 = IS_VALID(region2) ? region2->area_place_flags : victim->in_wilds->defaultPlaceFlags;

				if (IS_VALID(region_c) && place2 == region_c->place_flags)
					return LOCALE_SAME_CONTINENT;

				return LOCALE_IN_WILDS;
			}
			else
			{
				if (victim->in_room->source)
					return LOCALE_IN_CLONE_ROOM;

				if (IS_VALID(region_c) && IS_VALID(region_v) && region_c->place_flags == region_v->place_flags)
				{
					// Same place/continent
					return LOCALE_SAME_CONTINENT;
				}
			}
		}

		// Need a way to tell if they are in the same realm (Overworld, the Abyss, etc)

		return LOCALE_ANYWHERE;
	}
}

void where_locale(char *buf, CHAR_DATA *ch, CHAR_DATA *victim, int locale)
{
	if (ch == victim)
	{
		sprintf(buf, "%-28s ---\n\r", pers(victim, ch));
		return;
	}

	switch(locale)
	{
		case LOCALE_IN_CLONE_ROOM:
			sprintf(buf, "%-28s %s [CLONE_ROOM]\n\r", pers(victim, ch), victim->in_room->name);
			break;

		case LOCALE_SAME_ROOM:
			sprintf(buf, "%-28s {WRight here{x [SAME_ROOM]\n\r", pers(victim, ch));
			break;

		case LOCALE_SAME_REGION:
			sprintf(buf, "%-28s %s [SAME_REGION]\n\r", pers(victim, ch), victim->in_room->name);
			break;
		
		case LOCALE_SAME_AREA:
		{
			AREA_REGION *region = get_room_region(victim->in_room);
			sprintf(buf, "%-28s %s [SAME AREA]\n\r", pers(victim, ch), region->name);
			break;
		}

		case LOCALE_SAME_CONTINENT:
			// Can be in two static rooms on the same continent, two wilderness rooms with the same area place... or one of each
			if (IN_WILDERNESS(ch))
			{
				if (IN_WILDERNESS(victim))
				{
					if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))	// Immortals just see coordinates
						sprintf(buf, "%-28s %s (%ld, %ld) [SAME_CONTINENT]\n\r", pers(victim, ch), victim->in_room->name, victim->in_room->x, victim->in_room->y);
					else
					{
						// Work out direction
						int dx = victim->in_room->x - ch->in_room->x;
						int dy = victim->in_room->y - ch->in_room->y;

						int angle = (int)(180 * atan2(dx, -dy) / 3.14159);
						if (angle < 0) angle += 360;

						extern int bearing_door[];
						int dir = bearing_door[2 * angle / 45];

						sprintf(buf, "%-28s %s (%s) [SAME_CONTINENT]\n\r", pers(victim, ch), victim->in_room->name, dir_name[dir]);
					}
				}
				else
				{
					sprintf(buf, "%-28s %s [SAME_CONTINENT]\n\r", pers(victim, ch), victim->in_room->area->name);
				}
			}
			else
			{
				if (IN_WILDERNESS(victim))
				{
					WILDS_REGION *region2 = get_region_by_coors(victim->in_wilds, victim->in_room->x, victim->in_room->y);
					
					if (IS_VALID(region2))
						sprintf(buf, "%-28s %s [SAME_CONTINENT]\n\r", pers(victim, ch), flag_string(wilderness_regions, region2->region));
					else
						sprintf(buf, "%-28s %s [SAME_CONTINENT]\n\r", pers(victim, ch), flag_string(wilderness_regions, victim->in_wilds->defaultRegion));
				}
				else
				{
					sprintf(buf, "%-28s %s [SAME_CONTINENT]\n\r", pers(victim, ch), victim->in_room->area->name);
				}
			}
			break;
		
		case LOCALE_SAME_DUNGEON:
		{
			INSTANCE_SECTION *section = victim->in_room->instance_section;
			if (IS_VALID(section) && IS_VALID(section->instance) && IS_VALID(section->instance->dungeon))
			{
				sprintf(buf, "%-28s Floor %d - %s [SAME_DUNGEON]\n\r", pers(victim, ch), section->instance->floor, section->instance->blueprint->name);
			}
			else
			{
				sprintf(buf, "%-28s Floor ??? [SAME_DUNGEON]\n\r", pers(victim, ch));
			}
			break;
		}

		case LOCALE_SAME_INSTANCE:
		{
			INSTANCE_SECTION *section = victim->in_room->instance_section;
			if (IS_VALID(section))
			{
				sprintf(buf, "%-28s %s [SAME_INSTANCE]", pers(victim, ch), section->section->name);
			}
			else
			{
				sprintf(buf, "%-28s ??? [SAME_INSTANCE]", pers(victim, ch));
			}
			break;
		}

		case LOCALE_IN_DUNGEON:
		{
			INSTANCE_SECTION *section = victim->in_room->instance_section;
			if (IS_VALID(section) && IS_VALID(section->instance) && IS_VALID(section->instance->dungeon))
			{
				sprintf(buf, "%-28s In Dungeon: %s [IN_DUNGEON]\n\r", pers(victim, ch), section->instance->dungeon->index->name);
			}
			else
			{
				sprintf(buf, "%-28s In Dungeon: ??? [IN_DUNGEON]\n\r", pers(victim, ch));
			}
			break;
		}

		case LOCALE_IN_INSTANCE:
		{
			// In an instance that isn't a dungeon
			INSTANCE_SECTION *section = victim->in_room->instance_section;
			if (IS_VALID(section) && IS_VALID(section->instance))
			{
				if (IS_VALID(section->instance->ship))
					sprintf(buf, "%-28s Aboard Ship: %s{x [IN_INSTANCE]\n\r", pers(victim, ch), section->instance->ship->ship_name);
				// TODO: Duty
				else
					sprintf(buf, "%-28s Instance: %s{x [IN_INSTANCE]\n\r", pers(victim, ch), section->instance->blueprint->name);
			}
			else
			{
				sprintf(buf, "%-28s ??? [IN_INSTANCE]\n\r", pers(victim, ch));
			}
			break;
		}

		case LOCALE_IN_WILDS:
		{
			WILDS_DATA *wilds = victim->in_wilds;
			if (IS_VALID(wilds))
			{
				if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
					sprintf(buf, "%-28s Map: %s (%ld, %ld) [IN_WILDS]\n\r", pers(victim, ch), wilds->name, victim->in_room->x, victim->in_room->y);
				else
					sprintf(buf, "%-28s Map: %s [IN_WILDS]\n\r", pers(victim, ch), wilds->name);
			}
			else
			{
				sprintf(buf, "%-28s Map: ??? [IN_WILDS]\n\r", pers(victim, ch));
			}

			break;
		}

		case LOCALE_SAME_WILDS_REGION:
		{
			// Just give the room
			if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))	// Immortals just see coordinates
				sprintf(buf, "%-28s %s (%ld, %ld) [SAME_WILDS_REGION]\n\r", pers(victim, ch), victim->in_room->name, victim->in_room->x, victim->in_room->y);
			else
			{
				// Work out direction
				int dx = victim->in_room->x - ch->in_room->x;
				int dy = victim->in_room->y - ch->in_room->y;

				int angle = (int)(180 * atan2(dx, -dy) / 3.14159);
				if (angle < 0) angle += 360;

				extern int bearing_door[];
				int dir = bearing_door[2 * angle / 45];

				sprintf(buf, "%-28s %s (%s) [SAME_WILDS_REGION]\n\r", pers(victim, ch), victim->in_room->name, dir_name[dir]);
			}
			break;
		}

		case LOCALE_SAME_WILDS:
		{
			WILDS_DATA *wilds = victim->in_wilds;
			if (IS_VALID(wilds))
			{
				WILDS_REGION *region = get_region_by_coors(wilds, victim->in_room->x, victim->in_room->y);
				if (IS_VALID(region))
				{
					if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
						sprintf(buf, "%-28s Map Region: %s (%ld, %ld) [SAME_WILDS]\n\r", pers(victim, ch), flag_string(wilderness_regions, region->region), victim->in_room->x, victim->in_room->y);
					else
						sprintf(buf, "%-28s Map Region: %s [SAME_WILDS]\n\r", pers(victim, ch), flag_string(wilderness_regions, region->region));
				}
				else // Use the default region of the wilderness
				{
					if (IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT))
						sprintf(buf, "%-28s Map Region: %s (%ld, %ld) [SAME_WILDS]\n\r", pers(victim, ch), flag_string(wilderness_regions, wilds->defaultRegion), victim->in_room->x, victim->in_room->y);
					else
						sprintf(buf, "%-28s Map Region: %s [SAME_WILDS]\n\r", pers(victim, ch), flag_string(wilderness_regions, wilds->defaultRegion));
				}
			}
			break;
		}

		case LOCALE_ANYWHERE:
		{
			AREA_REGION *region = get_room_region(victim->in_room);

			// At this point, they are not in an instance of any kind
			if (region->place_flags != PLACE_NOWHERE)
				sprintf(buf, "%-28s %s [ANYWHERE]\n\r", pers(victim, ch), flag_string(place_flags, region->place_flags));
			else
				sprintf(buf, "%-28s ??? [ANYWHERE]\n\r", pers(victim, ch));
			break;
		}

		case LOCALE_NOWHERE:
			sprintf(buf, "%-28s ??? [NOWHERE]\n\r", pers(victim, ch));
			break;
	}
}

bool in_same_locale(CHAR_DATA *ch, CHAR_DATA *victim)
{
	// Already in the same room (clone or not)
	if (ch->in_room == victim->in_room) return TRUE;

	// One is in a cloned room, the other is not
	if ((!ch->in_room->source) != (!victim->in_room->source)) return FALSE;

	// Different areas
	if (ch->in_room->area != victim->in_room->area) return FALSE;

	INSTANCE_SECTION *section = ch->in_room->instance_section;
	INSTANCE_SECTION *section2 = victim->in_room->instance_section;

	if (IS_VALID(section))
	{
		if (IS_VALID(section2))
		{
			// Same section already
			if (section == section2) return TRUE;

			INSTANCE *instance = section->instance;
			INSTANCE *instance2 = section2->instance;

			if (IS_VALID(instance))
			{
				if (IS_VALID(instance2))
				{
					// Same instance (either dungeon floor, duty instance, object instance, or ship)
					if (instance == instance2) return TRUE;

					// Same dungeon
					if (IS_VALID(instance->dungeon) && IS_VALID(instance2->dungeon) && instance->dungeon == instance2->dungeon) return TRUE;

					// TODO: check for ship fleets in the future
				}
			}
		}
	}
	else if (!IS_VALID(section2))
	{
		AREA_REGION *region = get_room_region(ch->in_room);
		AREA_REGION *region2 = get_room_region(victim->in_room);

		return IS_VALID(region) && IS_VALID(region2) && region == region2;
	}

	return FALSE;
}

void do_where(CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    DESCRIPTOR_DATA *d;
    bool found;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		sprintf(buf, "{YYou check for players in %s:\n\r{x", ch->in_room->area->name);
		send_to_char(buf, ch);
		found = FALSE;
		for (d = descriptor_list; d; d = d->next)
		{
			if (d->connected == CON_PLAYING
			&& (victim = d->character) != NULL && !IS_NPC(victim)
			&& !IS_SWITCHED(ch)
			&& victim->in_room != NULL
			&& !IS_SET(victim->in_room->room_flag[0], ROOM_NOWHERE)
			&& victim->in_room->area == ch->in_room->area
			&& in_same_locale(ch, victim)
			&& (!IS_MORPHED(victim) || (IS_MORPHED(victim) && can_see_shift(ch, victim)))
			&& (!IS_SHIFTED(victim) || (IS_SHIFTED(victim) && can_see_shift(ch, victim)))
			&& victim->position != POS_FEIGN
			&& can_see(ch, victim))
			{
				found = TRUE;
				sprintf(buf, "%-28s %s\n\r", pers(victim, ch), victim->in_room->name);
				send_to_char(buf, ch);
			}
		}
		if (!found)
		    send_to_char("None\n\r", ch);
	}
	else if (!str_prefix(arg, "group"))
	{
		send_to_char("{CYou check for players in your group:\n\r{x", ch);
		found = FALSE;
		for (d = descriptor_list; d; d = d->next)
		{
			if (d->connected == CON_PLAYING
			&& (victim = d->character) != NULL && !IS_NPC(victim)
			&& !IS_SWITCHED(ch)
			&& victim->in_room != NULL
			&&
			is_same_group(ch, victim))
			{
				int locale = relative_locale(ch, victim);
				found = TRUE;
				where_locale(buf, ch, victim, locale);
				send_to_char(buf, ch);
			}
		}
		if (!found)
		    send_to_char("None\n\r", ch);
	}
}

void do_dice(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    int num;
    int type;
    int result;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Syntax: dice <#dice> <#sides>\n\r", ch);
	return;
    }

    if (!is_number(arg) || !is_number(arg2))
    {
	send_to_char("You must specify numerical arguments.\n\r", ch);
	return;
    }

    num = atoi(arg);
    type = atoi(arg2);
    if (num < 1 || num > 50)
    {
	send_to_char("You can only throw 1-50 dice.\n\r", ch);
	return;
    }

    if (type < 2 || type > 100)
    {
	send_to_char("The dice can only be 2-100 sided.\n\r", ch);
	return;
    }

    result = dice(num, type);
    sprintf(buf, "You rolled a %d.\n\r", result);
    send_to_char(buf, ch);

    sprintf(buf, "$n rolled a %d.", result);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}

int calc_season(void)
{
    int season = 0;

    if (time_info.month <= 1 || time_info.month >= 11)
	season = SEASON_WINTER;
    else if (time_info.month >= 2 && time_info.month <= 4)
	season = SEASON_SPRING;
    else if (time_info.month >= 5 && time_info.month <= 6)
	season = SEASON_SUMMER;
    else if (time_info.month >= 7 && time_info.month <= 10)
	season = SEASON_FALL;

    return season;
}

/*    Figure out which desc to use for a room. */
char *find_desc_for_room(ROOM_INDEX_DATA *room, CHAR_DATA *viewer)
{
	CONDITIONAL_DESCR_DATA *best_cd = NULL;
	CONDITIONAL_DESCR_DATA *cd;
	SCRIPT_DATA *script;

	if ((cd = room->conditional_descr) != NULL) {
		for (cd = room->conditional_descr; cd != NULL; cd = cd->next) {
			switch (cd->condition) {
			case CONDITION_SEASON:
				if (calc_season() == cd->phrase)
					best_cd = cd;
				break;

			case CONDITION_SKY:
				if (weather_info.sky == cd->phrase)
					best_cd = cd;
				break;

			case CONDITION_HOUR:
				if (time_info.hour == cd->phrase)
					best_cd = cd;
				break;

			case CONDITION_SCRIPT:
				script = get_script_index(room->area, cd->phrase,PRG_RPROG);

				if (script && execute_script(script,NULL,NULL,room,NULL,NULL,NULL,NULL,viewer,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,TRIG_NONE,0,0,0,0,0) > 0)
					best_cd = cd;
				break;
			}
		}
	}


	if (best_cd != NULL)
		return best_cd->description;
	else
		return room->description;
}

/*    Show a room's description to a char. */
void show_room_description(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
	send_to_char(find_desc_for_room(room,ch),ch);
}

void show_basic_mob_lore(CHAR_DATA *ch, CHAR_DATA *victim)
{
	char buf[2*MAX_STRING_LENGTH];

	long avg;
	sprintf(buf, "{MImmune to: {x%s\n\r"
			"{MResistant to: {x%s\n\r"
			"{MVulnerable to: {x%s\n\r",
			imm_bit_name(victim->imm_flags),
			res_bit_name(victim->res_flags),
			vuln_bit_name(victim->vuln_flags));
	send_to_char(buf, ch);

	avg = (victim->damage.number * victim->damage.size)
		+ (victim->damage.number);
	avg /= 2;

	avg += victim->damage.bonus;

	sprintf(buf, "{MHealth:{x %s%.0f%%{x {MAverage Damage:{x %ld\n\r",
			victim->hit < victim->max_hit / 2 ? "{R" :
			victim->hit < victim->max_hit/1.5 ? "{G" : "{x",
			(float) victim->hit/victim->max_hit*100,
			/* victim->mana < victim->max_mana / 2 ? "{R" :
				victim->mana < victim->max_mana/1.5 ? "{G" : "{x",
				(float) victim->mana/victim->max_mana*100, */
			avg);
	send_to_char(buf, ch);

	p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LORE, NULL,0,0,0,0,0);
}


void do_expand(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	OBJ_DATA *telescope;

	if( IS_NULLSTR(argument) )
	{
		send_to_char("Expand what?\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( (telescope = get_obj_carry(ch, arg, ch)) == NULL )
	{
		send_to_char("You don't see that.\n\r", ch);
		return;
	}

	int distance;
	if( is_number(argument) )
	{
		distance = atoi(argument);
		if( distance < telescope->value[1] ||
			distance > telescope->value[2] )
		{
			char buf[MSL];
			sprintf(buf, "{xCannot expand $p{x to that distance.  Please pick a value from %d to %d.",
				telescope->value[1], telescope->value[2]);

			act(buf, ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
			return;
		}

	}
	else
		distance = telescope->value[2];

	if( distance > telescope->value[0] )
	{
		telescope->value[0] = distance;
    	act("{xYou expand $p{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
    	act("{x$n expands $p{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_ROOM);
	}
	else if( distance < telescope->value[0] )
	{
		act("{x$p{x is already expanded further.{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
		act("{x$p{x is already expanded that far.{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
	}
}

void do_collapse(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	OBJ_DATA *telescope;

	if( IS_NULLSTR(argument) )
	{
		send_to_char("Expand what?\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( (telescope = get_obj_carry(ch, arg, ch)) == NULL )
	{
		send_to_char("You don't see that.\n\r", ch);
		return;
	}

	int distance;
	if( is_number(argument) )
	{
		distance = atoi(argument);
		if( distance < telescope->value[1] ||
			distance > telescope->value[2] )
		{
			char buf[MSL];
			sprintf(buf, "{xCannot collapse $p{x to that distance.  Please pick a value from %d to %d.",
				telescope->value[1], telescope->value[2]);

			act(buf, ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
			return;
		}

	}
	else
		distance = 0;

	if( distance > telescope->value[0] )
	{
		if( telescope->value[0] > 0 )
			act("{x$p{x is already collapsed further.{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
		else
			act("{xYou cannot collapse $p{x any further.{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
	}
	else if( distance < telescope->value[0] )
	{
		telescope->value[0] = distance;
    	act("{xYou collapse $p{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
    	act("{x$n collapses $p{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_ROOM);
	}
	else
	{
		act("{x$p{x is already collapsed that far.{x.", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
	}
}

void look_sextant(CHAR_DATA *ch, OBJ_DATA *sextant)
{
	if( IS_VALID(sextant) && sextant->item_type == ITEM_SEXTANT &&
		(IN_WILDERNESS(ch) || ON_SHIP(ch)) && ch->in_room != NULL )
	{
		long x, y;
		SHIP_DATA *ship = get_room_ship(ch->in_room);
		long dist = -1;

		int skill;
		SKILL_DATA *gsk = NULL;
		bool success = true;

		if (IS_VALID(ship))
		{
			if( !ship->ship->in_room || !IS_WILDERNESS(ship->ship->in_room) )
			{
				// Doesn't do anything
				return;
			}

			x = ship->ship->in_room->x;
			y = ship->ship->in_room->y;

			skill = get_skill(ch, gsk_navigation);
			if( skill > 0 )
				gsk = gsk_navigation;
			else
			{
				skill = get_skill(ch, gsk_survey);
				if( skill < 1 ) return;

				gsk = gsk_survey;
			}

			success = (number_percent() < skill);
			if (!success || number_percent() >= sextant->value[0])
			{
				x = x + number_range(-30, 30);
				y = y + number_range(-30, 30);

				x = URANGE(0, x, ship->ship->in_room->wilds->map_size_x - 1);
				y = URANGE(0, y, ship->ship->in_room->wilds->map_size_y - 1);
			}

			if( ship->ship_power <= SHIP_SPEED_STOPPED && ship_isowner_player(ship, ch) )
			{
				ship->sextant_x = x;
				ship->sextant_y = y;
			}

			if( ship->seek_point.wilds != NULL )
			{
				long dx = (x - ship->seek_point.x);
				long dy = (y - ship->seek_point.y);

				dist = (long)(sqrt(dx * dx + dy * dy) + 0.5);
			}

		}
		else
		{
			// ch->in_room->wilds is defined
			x = ch->in_room->x;
			y = ch->in_room->y;

			skill = get_skill(ch, gsk_survey);
			if( skill > 0 )
				gsk = gsk_survey;
			else
			{
				skill = get_skill(ch, gsk_navigation);
				if( skill < 1 ) return;

				gsk = gsk_navigation;
			}

			success = (number_percent() < skill);
			if (!success || number_percent() >= sextant->value[0])
			{
				x = x + number_range(-30, 30);
				y = y + number_range(-30, 30);

				x = URANGE(0, x, ch->in_room->wilds->map_size_x - 1);
				y = URANGE(0, y, ch->in_room->wilds->map_size_y - 1);
			}
		}

		char buf[MSL];
		sprintf(buf, "{xThe sextant reads {W%ld{x south, {W%ld{x east.{x\n\r", y, x);
		send_to_char(buf, ch);

		if( dist >= 0 )
		{
			sprintf(buf, "{YYour vessel about %ld miles away.{x\n\r", dist);
			send_to_char(buf, ch);
		}

		if( !IS_NPC(ch) && !ch->pcdata->spam_block_navigation )
		{
			check_improve(ch, gsk, success, 10);
			ch->pcdata->spam_block_navigation = true;
		}
	}

}

void look_through_telescope(CHAR_DATA *ch, OBJ_DATA *telescope, char *argument)
{
	if( !IS_VALID(telescope) || telescope->item_type != ITEM_TELESCOPE )
	{
		return;
	}

	if( telescope->value[0] <= 0 )
	{
		send_to_char("{DThe telescope is fully collapsed.\n\r", ch);
		return;
	}

	int heading;
	bool can_fudge = true;
	if( argument[0] == '\0' )
	{
		if( CAN_WEAR(telescope, ITEM_TAKE) || telescope->value[4] < 0 )
		{
			send_to_char("{RThe telescope is pointing nowhere.\n\r", ch);
			return;
		}

		heading = telescope->value[4];
		can_fudge = false;
	}
	else if( is_number(argument) )
	{
		heading = atoi(argument);

		if( heading < 0 || heading >= 360 )
		{
			return;
		}
	}
	else
	{
		heading = parse_direction(argument);

		switch(heading)
		{
		case DIR_NORTH:				heading = 0; break;
		case DIR_NORTHEAST:			heading = 45; break;
		case DIR_EAST:				heading = 90; break;
		case DIR_SOUTHEAST:			heading = 135; break;
		case DIR_SOUTH:				heading = 180; break;
		case DIR_SOUTHWEST:			heading = 225; break;
		case DIR_WEST:				heading = 270; break;
		case DIR_NORTHWEST:			heading = 315; break;
		default:
			return;
		}
	}

	if( (IN_WILDERNESS(ch) || ON_SHIP(ch)) && ch->in_room != NULL )
	{
		long x, y;
		SHIP_DATA *ship = get_room_ship(ch->in_room);
		WILDS_DATA *wilds = NULL;

		int skill;
		SKILL_DATA *gsk = NULL;
		bool success;

		if (IS_VALID(ship))
		{
			if( ship->ship->in_room && IS_WILDERNESS(ship->ship->in_room) && IS_OUTSIDE(ch) )
			{
				wilds = ship->ship->in_room->wilds;
				x = ship->ship->in_room->x;
				y = ship->ship->in_room->y;
			}
		}
		else
		{
			wilds = ch->in_room->wilds;
			x = ch->in_room->x;
			y = ch->in_room->y;
		}

		if( can_fudge )
		{
			int skill1 = get_skill(ch, gsk_survey);
			int skill2 = get_skill(ch, gsk_navigation);

			if( skill1 >= skill2 )
			{
				skill = skill1;
				gsk = gsk_survey;
			}
			else
			{
				skill = skill2;
				gsk = gsk_navigation;
			}

			int delta = number_percent() - skill;

			if( delta >= 0 )
			{
				success = false;

				++delta;

				heading += number_range(-delta, delta);
				if( heading < 0 ) heading += 360;
				else if( heading >= 360 ) heading -= 360;
			}
		}

		if( wilds )
		{
			// Telescope position
			int tx = x + (int)(telescope->value[0] * sin(3.14159 * heading / 180));
			int ty = y - (int)(telescope->value[0] * cos(3.14159 * heading / 180));

			// Bonusview size
			int bvx = telescope->value[3];
			int bvy = 2 * bvx / 3;

			act("{xPeering through $p{x, you see:{x", ch, NULL, NULL, telescope, NULL, NULL, NULL, TO_CHAR);
			show_map_to_char_wyx(wilds, tx, ty, ch, x, y, bvx, bvy, false);
		}

		if( can_fudge && IS_VALID(gsk) && !IS_NPC(ch) && !ch->pcdata->spam_block_navigation )
		{
			check_improve(ch, gsk, success, 10);
			ch->pcdata->spam_block_navigation = true;
		}
	}

	// Stationary telescopes can save their heading
	if( !CAN_WEAR(telescope, ITEM_TAKE) )
		telescope->value[4] = heading;
}

void look_compass(CHAR_DATA *ch, OBJ_DATA *compass)
{
	char buf[MSL], arg[MIL];
	ROOM_INDEX_DATA *here = ch->in_room;
	SHIP_DATA *ship = get_room_ship(here);
	int heading = -1;
	SKILL_DATA *gsk = NULL;
	int skill;

	if( IS_VALID(ship) )
	{
		// When on a ship, favor navigation over survey
		skill = get_skill(ch, gsk_navigation);

		if( skill > 0 )
			gsk = gsk_navigation;
		else
		{
			skill = get_skill(ch, gsk_survey);

			if( skill < 1 )
				return;

			gsk = gsk_survey;
		}
	}
	else
	{
		// When not on a ship, favor survey over navigation
		skill = get_skill(ch, gsk_survey);

		if( skill > 0 )
			gsk = gsk_survey;
		else
		{
			skill = get_skill(ch, gsk_navigation);

			if( skill < 1 )
				return;

			gsk = gsk_navigation;
		}
	}

	if( skill < 1 )
		return;

	if( compass->value[1] > 0 )
	{
		if( IS_VALID(ship) )
		{
			here = obj_room(ship->ship);
		}

		if( !here || !IS_WILDERNESS(here) )
		{
			return;
		}

		if( here->wilds->uid == compass->value[1] )
		{
			int dx = compass->value[2] - here->x;
			int dy = here->y - compass->value[3];

			if( dx != 0 || dy != 0 )
			{
				heading = (int)(180 * atan2(dx, dy) / 3.14159);
				if( heading < 0 ) heading += 360;
			}
		}
	}
	else if( IS_VALID(ship) )
	{
		// get the heading
		heading = ship->steering.heading;
	}
	else
		return;

	if( heading < 0 )
	{
		act("{xThe needle on $p{x is spinning.{x", ch, NULL, NULL, compass, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( number_percent() >= compass->value[0] )
	{
		heading += number_range(-30, 30);
		if( heading < 0 ) heading += 360;
		else if(heading >= 360) heading -= 360;
	}

	bool success;
	if( number_percent() < skill )
	{
		if( IS_IMMORTAL(ch) /* || TODO: add skill */ )
		{
			switch(heading)
			{
			case 0:		strcpy(arg, "to the north"); break;
			case 45:	strcpy(arg, "to the northeast"); break;
			case 90:	strcpy(arg, "to the east"); break;
			case 135:	strcpy(arg, "to the southeast"); break;
			case 180:	strcpy(arg, "to the south"); break;
			case 225:	strcpy(arg, "to the southwest"); break;
			case 270:	strcpy(arg, "to the west"); break;
			case 315:	strcpy(arg, "to the northwest"); break;
			default:
				sprintf(arg, "to {W%d{x degrees", heading);
				break;
			}
		}
		else
		{
			heading += 23;
			if( heading >= 360 || heading < 45 )
			{
				strcpy(arg, "northward");
			}
			else if( heading < 90 )
			{
				strcpy(arg, "northeastward");
			}
			else if( heading < 135 )
			{
				strcpy(arg, "eastward");
			}
			else if( heading < 180 )
			{
				strcpy(arg, "southeastward");
			}
			else if( heading < 225 )
			{
				strcpy(arg, "southward");
			}
			else if( heading < 270 )
			{
				strcpy(arg, "southwestward");
			}
			else if( heading < 315 )
			{
				strcpy(arg, "westward");
			}
			else if( heading < 360 )
			{
				strcpy(arg, "northwestward");
			}
		}

		sprintf(buf, "{xThe needle on $p{x points %s.", arg);
		act(buf, ch, NULL, NULL, compass, NULL, NULL, NULL, TO_CHAR);
		success = true;
	}
	else
	{
		act("{xYou have trouble reading the needle on $p{x.", ch, NULL, NULL, compass, NULL, NULL, NULL, TO_CHAR);
		success = false;
	}

	if( !IS_NPC(ch) && !ch->pcdata->spam_block_navigation )
	{
		check_improve(ch, gsk, success, 10);
		ch->pcdata->spam_block_navigation = true;
	}
}

void look_map(CHAR_DATA *ch, OBJ_DATA *map)
{
	bool success;
	int skill = get_skill(ch, gsk_navigation);

	if( list_size(map->waypoints) > 0 )
	{
		if( number_percent() < skill )
		{
			int cnt = 0;
			char buf[MSL];
			ITERATOR wit;
			WAYPOINT_DATA *wp;

			iterator_start(&wit, map->waypoints);
			while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
			{
				send_to_char("{WCartographer Waypoints:{x\n\r\n\r", ch);
				send_to_char("{M     [{W     Wilderness     {M] [{W South {M] [{W  East {M] [{W        Name        {M]{x\n\r", ch);
				send_to_char("{M======================================================================={x\n\r", ch);

				iterator_start(&wit, map->waypoints);
				while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
				{
					WILDS_DATA *wilds = get_wilds_from_uid(NULL, wp->w);

					char *wname = wilds ? wilds->name : "{D(null){x";

					int wwidth = get_colour_width(wname) + 20;

					sprintf(buf, "{W%3d{Y)  {x%-*.*s    {W%5d     %5d    {Y%s{x\n\r",
						++cnt,
						wwidth, wwidth, wname,
						wp->y, wp->x, wp->name);

					send_to_char(buf, ch);
				}

				iterator_stop(&wit);

				send_to_char("\n\r", ch);
			}
			iterator_stop(&wit);

			success = true;
		}
		else
		{
			act("{MYou don't understand the numbers written on {x$p{M.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR);

			success = false;
		}

		if( !IS_NPC(ch) && !ch->pcdata->spam_block_navigation )
		{
			check_improve(ch, gsk_navigation, success, 10);
			ch->pcdata->spam_block_navigation = true;
		}
	}
}

void look_portal(CHAR_DATA *ch, OBJ_DATA *portal)
{
	if (IS_NPC(ch)) return;

	if (IS_SET(portal->value[1], EX_CLOSED) && !IS_SET(portal->value[1], EX_TRANSPARENT))
		return;

	ROOM_INDEX_DATA *location = get_portal_destination(ch, portal, FALSE);

	if (location)
	{
		send_to_char("{YPeering through, you see:{x\n\r", ch);

		// Check if it's wilderness
		if (location->wilds && IS_SET(location->room_flag[1], ROOM_VIRTUAL_ROOM)) {
			int vp_x, vp_y;

			vp_x = get_squares_to_show_x(ch->wildview_bonus_x);
			vp_y = get_squares_to_show_y(ch->wildview_bonus_y);
			show_map_to_char_wyx(location->wilds, location->x, location->y, ch, location->x, location->y, vp_x, vp_y, FALSE);
		}
		else
			show_room_description(ch, location);
	}
}

struct class_info_filter
{
	char name[MIL];
	sh_int type;
	sh_int min_level;
	sh_int max_level;


	bool has_name;
	bool has_type;
	bool has_min;
	bool has_max;
};

bool __classes_parse_filters(CHAR_DATA *ch, char *argument, struct class_info_filter *params)
{
	params->has_name = false;
	params->has_type = false;
	params->has_min = false;
	params->has_max = false;

	while(argument[0])
	{
		char arg[MIL];
		argument = one_argument(argument, arg);

		if (!str_prefix(arg, "name"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Missing name in filter.\n\r", ch);
				return false;
			}

			argument = one_argument(argument, params->name);
			params->has_name = true;
		}
		else if (!str_prefix(arg, "type"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Missing type in filter.\n\r", ch);
				return false;
			}

			argument = one_argument(argument, arg);

			int type;
			if ((type = stat_lookup(arg, class_types, NO_FLAG)) == NO_FLAG)
			{
				send_to_char("Invalid class type.  Please select from one of the following:\n\r", ch);
				show_flag_cmds(ch, class_types);
				return false;
			}

			params->type = type;
			params->has_type = true;
		}
		else if (!str_prefix(arg, "min"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Missing minimum level in filter.\n\r", ch);
				return false;
			}

			argument = one_argument(argument, arg);
			int level;
			if (!is_number(arg) || (level = atoi(arg)) < 1 || level > MAX_CLASS_LEVEL)
			{
				send_to_char(formatf("Please provide a level from 1 to %d.\n\r", MAX_CLASS_LEVEL), ch);
				return false;
			}

			params->min_level = level;
			params->has_min = true;
		}
		else if (!str_prefix(arg, "max"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Missing minimum level in filter.\n\r", ch);
				return false;
			}

			argument = one_argument(argument, arg);
			int level;
			if (!is_number(arg) || (level = atoi(arg)) < 1 || level > MAX_CLASS_LEVEL)
			{
				send_to_char(formatf("Please provide a level from 1 to %d.\n\r", MAX_CLASS_LEVEL), ch);
				return false;
			}

			params->max_level = level;
			params->has_max = true;
		}
	}

	if (params->has_min && params->has_max && params->min_level > params->max_level)
	{
		send_to_char("Minimum level cannot be greater than maximum level in filter.\n\r", ch);
		return false;
	}

	return true;
}


void do_classes(CHAR_DATA *ch, char *argument)
{
	if (IS_NPC(ch)) return;

	BUFFER *buffer = new_buf();
	char buf[MSL];
	struct class_info_filter params;

	if (!__classes_parse_filters(ch, argument, &params))
		return;

	int i = 0;
	ITERATOR it;
	CLASS_LEVEL *level;

	add_buf(buffer, formatf(" # [        Name        ] [ Level ] [         Progress         ]\n\r"));
	add_buf(buffer, formatf("=================================================================\n\r"));

	iterator_start(&it, ch->pcdata->classes);
	while((level = (CLASS_LEVEL *)iterator_nextdata(&it)))
	{
		if (params.has_name && str_prefix(params.name, level->clazz->name)) continue;
		if (params.has_type && level->clazz->type != params.type) continue;
		if (params.has_min && level->level < params.min_level) continue;
		if (params.has_max && level->level > params.max_level) continue;

		int maxexp = exp_per_level_table[level->level].exp;
		long percent = (maxexp > 0) ? (100 * level->xp / maxexp) : 0;

		char clazz_color = 'x';
		switch(level->clazz->type)
		{
		case CLASS_CLERIC:		clazz_color = 'Y'; break;
		case CLASS_CRAFTING:	clazz_color = 'C'; break;
		case CLASS_EXPLORER:	clazz_color = 'M'; break;
		case CLASS_GATHERING:	clazz_color = 'G'; break;
		case CLASS_MAGE:		clazz_color = 'B'; break;
		case CLASS_THIEF:		clazz_color = 'W'; break;
		case CLASS_WARRIOR:		clazz_color = 'R'; break;
		}

		char *display = formatf("{%c%s{x", clazz_color, level->clazz->display[ch->sex]);
		int l=20-strlen_no_colours(display);
		l=UMAX(l,0);

		sprintf(buf, formatf("%%2d  %%s%%%ds     %%s     %%s\n\r", l), ++i,
			(ch->pcdata->current_class != level) ?
				MXPCreateSend(ch->desc, formatf("setclass %s", level->clazz->display[ch->sex]), display) :
				display,
			"",
			(level->level < MAX_CLASS_LEVEL) ? formatf("%3d", level->level) : "{WMAX{x",
			(level->level < MAX_CLASS_LEVEL) ? formatf("%ld / %ld (%ld%%)", level->xp, maxexp, percent) : "{D-{x / {D-{x");
		add_buf(buffer, buf);
	}
	iterator_stop(&it);
	add_buf(buffer, formatf("=================================================================\n\r"));

	if (i > 0)
	{
		add_buf(buffer, formatf("%d class%s found.\n\r", i, (i == 1)?"":"es"));
	}
	else
	{
		clear_buf(buffer);
		add_buf(buffer, "No classes found.\n\r");
	}

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
}
