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
 ITEM TYPES

 Handles the various functionality for the individual item type data structures
*/

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "tables.h"

// 

// ITEM_LIGHT				Yes
// ITEM_SCROLL				Yes
// ITEM_WAND				Yes
// ITEM_STAFF				Deleted (to be merged with WEAPON)
// ITEM_WEAPON				No
// ITEM_TREASURE			No
// ITEM_ARMOUR				Yes
// ITEM_POTION				Fluid Container
// ITEM_CLOTHING			No
// ITEM_FURNITURE			Yes
// ITEM_TRASH				No
// ITEM_CONTAINER			Yes
// ITEM_DRINK_CON			Fluid Container
// ITEM_KEY					No
// ITEM_FOOD				Yes
// ITEM_MONEY				Yes
// ITEM_BOAT				No
// ITEM_CORPSE_NPC			No
// ITEM_CORPSE_PC			No
// ITEM_FLUID_CONTAINER		Yes
// ITEM_FOUNTAIN			Fluid Container
// ITEM_PILL				No
// ITEM_PROTECT				To be deleted
// ITEM_MAP					No
// ITEM_PORTAL				Yes
// ITEM_CATALYST			To be deleted
// ITEM_ROOM_KEY			To be deleted
// ITEM_GEM					No
// ITEM_JEWELRY				Yes
// ITEM_JUKEBOX				No
// ITEM_ARTIFACT			No
// ITEM_SHARECERT			To be deleted
// ITEM_ROOM_FLAME			Mist
// ITEM_INSTRUMENT			Yes
// ITEM_SEED				No
// ITEM_CART				No
// ITEM_SHIP				No
// ITEM_ROOM_DARKNESS		To be converted to a room affect
// ITEM_RANGED_WEAPON		No (WEAPON)
// ITEM_SEXTANT				No
// ITEM_WEAPON_CONTAINER	Container
// ITEM_ROOM_ROOMSHIELD		To be converted to a room affect
// ITEM_BOOK				Yes
// ITEM_SMOKE_BOMB			No
// ITEM_STINKING_CLOUD		Mist
// ITEM_HERB				No
// ITEM_SPELL_TRAP			No
// ITEM_WITHERING_CLOUD		Mist
// ITEM_BANK				No
// ITEM_KEYRING				Container
// ITEM_TRADE_TYPE			No
// ITEM_ICE_STORM			Mist
// ITEM_FLOWER				No
// ITEM_EMPTY_VIAL			Fluid Container / Deleted
// ITEM_BLANK_SCROLL		Scroll / Deleted
// ITEM_MIST				Yes
// ITEM_SHRINE				To be deleted
// ITEM_WHISTLE				No
// ITEM_SHOVEL				No
// ITEM_TOOL				No
// ITEM_PIPE				No
// ITEM_TATTOO				Yes
// ITEM_INK					Yes
// ITEM_PART				No
// ITEM_COMMODITY			No
// ITEM_TELESCOPE			No
// ITEM_COMPASS				No
// ITEM_WHETSTONE			No
// ITEM_CHISEL				No
// ITEM_PICK				No
// ITEM_TINDERBOX			No
// ITEM_NEEDLE				No
// ITEM_DRYING_CLOTH		No
// ITEM_BODY_PART			No
// ITEM_PAGE				Yes


/*
Plan:

MIST:
	ITEM_ICE_STORM
	ITEM_MIST
	ITEM_ROOM_FLAME
	ITEM_STINKING_CLOUD
	ITEM_WITHERING_CLOUD

	Flags: icy, fiery, stinking, withering, toxic, acidic, fog
	Obscure Objs: 0-100
	Obscure Mobs: 0-100
	Obscure Room: 0-100 (obscure's view from outside the room)

Give HERBs some stuff for use with PIPEs.

*/

#if 0
bool composite_matrix[ITEM__MAX][ITEM__MAX] = {
/* ITEM_LIGHT */			{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SCROLL */			{0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_WAND */				{0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_STAFF */			{0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_WEAPON */			{0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_TREASURE */			{0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ARMOUR */			{0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_POTION */			{0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CLOTHING */			{0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_FURNITURE */		{0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_TRASH */			{0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CONTAINER */		{0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_DRINK_CON */		{0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_KEY */				{0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_FOOD */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_MONEY */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_BOAT */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CORPSE_NPC */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CORPSE_PC */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_FOUNTAIN */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_PILL */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_PROTECT */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_MAP */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_PORTAL */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CATALYST */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ROOM_KEY */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_GEM */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_JEWELRY */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_JUKEBOX */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ARTIFACT */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SHARECERT */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ROOM_FLAME */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_INSTRUMENT */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SEED */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_CART */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SHIP */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ROOM_DARKNESS */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_RANGED_WEAPON */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SEXTANT */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_WEAPON_CONTAINER */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ROOM_ROOMSHIELD */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_BOOK */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SMOKE_BOMB */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_STINKING_CLOUD */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_HERB */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SPELL_TRAP */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_WITHERING_CLOUD */	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_BANK */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_KEYRING */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_TRADE_TYPE */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_ICE_STORM */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_FLOWER */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_EMPTY_VIAL */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_BLANK_SCROLL */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_MIST */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SHRINE */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_WHISTLE */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_SHOVEL */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_TOOL */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_PIPE */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_TATTOO */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_INK */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0},
/* ITEM_PART */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0},
/* ITEM_COMMODITY */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0},
/* ITEM_TELESCOPE */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
/* ITEM_COMPASS */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
/* ITEM_WHETSTONE */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},
/* ITEM_CHISEL */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
/* ITEM_PICK */				{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0},
/* ITEM_TINDERBOX */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},
/* ITEM_NEEDLE */			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
/* ITEM_DRYING_CLOTH */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0},
/* ITEM_BODY_PART */		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
};
#endif

void obj_index_assess_item_types(OBJ_INDEX_DATA *pObjIndex, bool assigned[ITEM__MAX])
{
	for(int i = 0; i < ITEM__MAX; i++)
		assigned[i] = false;

	assigned[pObjIndex->item_type] = true;
	if (IS_CONTAINER(pObjIndex))	assigned[ITEM_CONTAINER] = true;
	if (IS_FOOD(pObjIndex))			assigned[ITEM_FOOD] = true;
	if (IS_LIGHT(pObjIndex))		assigned[ITEM_LIGHT] = true;
	if (IS_MONEY(pObjIndex))		assigned[ITEM_MONEY] = true;
}


bool obj_index_can_add_item_type(OBJ_INDEX_DATA *pObjIndex, int item_type)
{
	// Only the PRIMARY object matters
	// If it is not explicitly listed, it doesn't multitype
	switch(pObjIndex->item_type)
	{
		case ITEM_CONTAINER:
			if (item_type == ITEM_ARMOUR) return true;
			if (item_type == ITEM_CART) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_FURNITURE) return true;
			if (item_type == ITEM_JEWELRY) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_PORTAL) return true;
			return false;

		case ITEM_JEWELRY:
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_WAND) return true;
			return false;
		
		case ITEM_CART:
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FURNITURE) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_PORTAL) return true;
			return false;

		case ITEM_FURNITURE:
			if (item_type == ITEM_CART) return true;
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_PORTAL) return true;
			return false;

		case ITEM_ARMOUR:
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_LIGHT) return true;
			return false;

		case ITEM_FLUID_CONTAINER:
			if (item_type == ITEM_ARMOUR) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FURNITURE) return true;
			if (item_type == ITEM_PORTAL) return true;
			if (item_type == ITEM_CART) return true;
			if (item_type == ITEM_WEAPON) return true;
			return false;

		case ITEM_LIGHT:
			if (item_type == ITEM_ARMOUR) return true;
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_FURNITURE) return true;
			if (item_type == ITEM_PORTAL) return true;
			if (item_type == ITEM_WEAPON) return true;
			if (item_type == ITEM_WAND) return true;
			return false;

		case ITEM_PORTAL:
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_CONTAINER) return true;
			if (item_type == ITEM_FURNITURE) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			if (item_type == ITEM_CART) return true;
			return false;

		case ITEM_WAND:
			if (item_type == ITEM_JEWELRY) return true;
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_WEAPON) return true;
			return false;

		case ITEM_WEAPON:
			if (item_type == ITEM_LIGHT) return true;
			if (item_type == ITEM_WAND) return true;
			if (item_type == ITEM_FLUID_CONTAINER) return true;
			return false;
	}

	return false;
}


int obj_get_weight(OBJ_DATA *obj)
{
    int weight;

    if (IS_MONEY(obj))
	{
		// TODO: replace with currency data
        return get_weight_coins(MONEY(obj)->silver, MONEY(obj)->gold);
	}

    weight = obj->weight;

    // This is for containers. Calculate weight of contents and factor in weight reduction
	if (IS_CONTAINER(obj))
	{
		int contents = container_get_content_weight(obj, NULL);
		if (contents > 0)
		{
			weight += CONTAINER(obj)->weight_multiplier * contents / 100;
		}
	}

    return weight;
}

int __item_type_get_subtype(OBJ_DATA *obj)
{
	if (IS_VALID(obj))
	{
		if (IS_WEAPON(obj))
		{
			return WEAPON(obj)->weapon_class;
		}
	}

	return -1;	// No subtype
}

int objindex_get_subtype(OBJ_INDEX_DATA *pObjIndex)
{
	if (pObjIndex)
	{
		if (WEAPON(pObjIndex))
		{
			return WEAPON(pObjIndex)->weapon_class;
		}
	}

	return -1;
}

// Open Close Lock Unlock
bool obj_oclu_ambiguous(OBJ_DATA *obj)
{
	int types = 0;

	if (IS_BOOK(obj)) types++;
	if (IS_CONTAINER(obj)) types++;
	if (IS_FLUID_CON(obj)) types++;
	if (IS_FURNITURE(obj))
	{
		// Can target it without a "context" if this is *only* furniture out of this list
		if (FURNITURE(obj)->main_compartment > 0)
			types++;
		else
			types+=list_size(FURNITURE(obj)->compartments);
	}

	if (IS_PORTAL(obj)) types++;

	return types > 1;
}

void obj_oclu_show_parts(CHAR_DATA *ch, OBJ_DATA *obj)
{
	int i = 0;

	if (IS_BOOK(obj))
	{
		if (i > 0) send_to_char(", ", ch);
		send_to_char(BOOK(obj)->short_descr, ch);
		i++;
	}

	if (IS_CONTAINER(obj))
	{
		if (i > 0) send_to_char(", ", ch);
		send_to_char(CONTAINER(obj)->short_descr, ch);
		i++;
	}

	if (IS_FLUID_CON(obj))
	{
		if (i > 0) send_to_char(", ", ch);
		send_to_char(FLUID_CON(obj)->short_descr, ch);
		i++;
	}

	if (IS_FURNITURE(obj))
	{
		ITERATOR it;
		FURNITURE_COMPARTMENT *compartment;
		iterator_start(&it, FURNITURE(obj)->compartments);
		while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
		{
			if (i > 0) send_to_char(", ", ch);
			send_to_char(compartment->short_descr, ch);
			i++;
		}
		iterator_stop(&it);
	}

	if (IS_PORTAL(obj))
	{
		if (i > 0) send_to_char(", ", ch);
		send_to_char(PORTAL(obj)->short_descr, ch);
		i++;
	}


	if (i == 0)
		send_to_char("  none", ch);

	send_to_char("\n\r", ch);
}

bool oclu_get_context(OCLU_CONTEXT *context, OBJ_DATA *obj, char *argument)
{
	if (argument[0] != '\0')
	{
		char arg[MIL];
		int count = number_argument(argument, arg);

		context->is_default = false;

		if (IS_BOOK(obj))
		{
			if (is_name(arg, BOOK(obj)->name) && (count-- == 1))
			{
				context->item_type = ITEM_BOOK;
				context->which = CONTEXT_BOOK;
				context->flags = &(BOOK(obj)->flags);
				context->label = BOOK(obj)->short_descr;
				context->lock = &(BOOK(obj)->lock);
				return true;
			}
		}

		if (IS_CONTAINER(obj))
		{
			if (is_name(arg, CONTAINER(obj)->name) && (count-- == 1))
			{
				context->item_type = ITEM_CONTAINER;
				context->which = CONTEXT_CONTAINER;
				context->flags = &(CONTAINER(obj)->flags);
				context->label = CONTAINER(obj)->short_descr;
				context->lock = &(CONTAINER(obj)->lock);
				return true;
			}
		}

		if (IS_FLUID_CON(obj))
		{
			if (is_name(arg, FLUID_CON(obj)->name) && (count-- == 1))
			{
				context->item_type = ITEM_FLUID_CONTAINER;
				context->which = CONTEXT_FLUID_CON;
				context->flags = &(FLUID_CON(obj)->flags);
				context->label = FLUID_CON(obj)->short_descr;
				context->lock = &(FLUID_CON(obj)->lock);
				return true;
			}
		}

		if (IS_FURNITURE(obj))
		{
			int which = 0;
			ITERATOR it;
			FURNITURE_COMPARTMENT *compartment;
			iterator_start(&it, FURNITURE(obj)->compartments);
			while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
			{
				++which;
				if (is_name(arg, compartment->name) && (count-- == 1))
				{
					context->item_type = ITEM_FURNITURE;
					context->which = which;
					context->flags = &compartment->flags;
					context->label = compartment->short_descr;
					context->lock = &compartment->lock;
					break;
				}

			}
			iterator_stop(&it);

			if (compartment) return true;
		}

		if (IS_PORTAL(obj))
		{
			if (is_name(arg, PORTAL(obj)->name) && (count-- == 1))
			{
				context->item_type = ITEM_PORTAL;
				context->which = CONTEXT_PORTAL;
				context->flags = &(PORTAL(obj)->exit);
				context->label = PORTAL(obj)->short_descr;
				context->lock = &(PORTAL(obj)->lock);
				return true;
			}
		}
	}
	// Not dealing with an ambiguous object
	else if (!obj_oclu_ambiguous(obj))
	{
		context->is_default = true;

		if (IS_BOOK(obj))
		{
			context->item_type = ITEM_BOOK;
			context->which = CONTEXT_BOOK;
			context->flags = &(BOOK(obj)->flags);
			context->label = BOOK(obj)->short_descr;
			context->lock = &(BOOK(obj)->lock);
			return true;
		}

		if (IS_CONTAINER(obj))
		{
			context->item_type = ITEM_CONTAINER;
			context->which = CONTEXT_CONTAINER;
			context->flags = &(CONTAINER(obj)->flags);
			context->label = CONTAINER(obj)->short_descr;
			context->lock = &(CONTAINER(obj)->lock);
			return true;
		}

		if (IS_FLUID_CON(obj))
		{
			context->item_type = ITEM_FLUID_CONTAINER;
			context->which = CONTEXT_FLUID_CON;
			context->flags = &(FLUID_CON(obj)->flags);
			context->label = FLUID_CON(obj)->short_descr;
			context->lock = &(FLUID_CON(obj)->lock);
			return true;
		}

		if (IS_FURNITURE(obj))
		{
			if (FURNITURE(obj)->main_compartment > 0)
			{
				FURNITURE_COMPARTMENT *compartment = (FURNITURE_COMPARTMENT *)list_nthdata(FURNITURE(obj)->compartments, FURNITURE(obj)->main_compartment);
				context->item_type = ITEM_FURNITURE;
				context->which = FURNITURE(obj)->main_compartment;
				context->flags = &compartment->flags;
				context->label = compartment->short_descr;
				context->lock = &compartment->lock;
				return true;
			}
		}

		if (IS_PORTAL(obj))
		{
			context->item_type = ITEM_PORTAL;
			context->which = CONTEXT_PORTAL;
			context->flags = &(PORTAL(obj)->exit);
			context->label = PORTAL(obj)->short_descr;
			context->lock = &(PORTAL(obj)->lock);
			return true;
		}
	}

	return false;
}

bool spell_cmp(SPELL_DATA *a, SPELL_DATA *b)
{
	if (a == NULL) return false;
	
	if (b == NULL) return false;

	if (a->skill != b->skill) return false;

	if (a->level != b->level) return false;

	if (a->repop != b->repop) return false;

	return true;
}

bool obj_has_same_spells(LLIST *a, LLIST *b)
{
	ITERATOR ia, ib;
	SPELL_DATA *sa, *sb;

	iterator_start(&ia, a);
	while((sa = (SPELL_DATA *)iterator_nextdata(&ia)))
	{
		iterator_start(&ib, b);
		while((sb = (SPELL_DATA *)iterator_nextdata(&ib)))
		{
			if (spell_cmp(sa, sb))
				break;
		}
		iterator_stop(&ib);

		if (!sb)	// sa was not found in list b
			break;
	}
	iterator_stop(&ia);

	// This spell was not found in the second list
	if (sa) return false;

	iterator_start(&ib, b);
	while((sb = (SPELL_DATA *)iterator_nextdata(&ib)))
	{
		iterator_start(&ia, a);
		while((sa = (SPELL_DATA *)iterator_nextdata(&ia)))
		{
			if (spell_cmp(sa, sb))
				break;
		}
		iterator_stop(&ia);

		if (!sa)	// sb was not found in list a
			break;
	}
	iterator_stop(&ib);

	return !sb;
}


////////////////////
// BOOK

BOOK_PAGE *book_get_page(BOOK_DATA *book, int page_no)
{
	ITERATOR it;
	BOOK_PAGE *page;
	iterator_start(&it, book->pages);
	while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
	{
		if (page->page_no == page_no)
			break;
	}
	iterator_stop(&it);

	return page;
}

bool book_insert_page(BOOK_DATA *book, BOOK_PAGE *new_page)
{
	bool valid = true;

	ITERATOR it;
	BOOK_PAGE *page;
	iterator_start(&it, book->pages);
	while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
	{
		if (page->page_no == new_page->page_no)
		{
			// Page already exists with this page!
			valid = false;
			break;
		}

		if (page->page_no > new_page->page_no)
		{
			iterator_insert_before(&it, new_page);
			break;
		}
	}
	iterator_stop(&it);

	if (valid)
	{
		// The new page is after ALL the existing pages
		if (!page)
			list_appendlink(book->pages, new_page);
	}

	return valid;
}

int book_max_pages(BOOK_DATA *book)
{
	if (list_size(book->pages) < 1) return 0;

	BOOK_PAGE *last_page = (BOOK_PAGE *)list_nthdata(book->pages, -1);

	return IS_VALID(last_page) ? last_page->page_no : 0;
}

////////////////////
// CONTAINER

bool __container_is_listed(LLIST *list, int item_type, int sub_type)
{
	ITERATOR it;
	CONTAINER_FILTER *filter;

	iterator_start(&it, list);
	while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
	{
		if (filter->item_type == item_type && (sub_type < 0 || filter->sub_type < 0 || filter->sub_type == sub_type))
		{
			break;
		}
	}
	iterator_stop(&it);

	return filter != NULL;
}

bool container_is_valid_item_type(OBJ_DATA *container, int item_type, int subtype)
{
	return IS_VALID(container) && IS_CONTAINER(container) &&
			((list_size(CONTAINER(container)->whitelist) < 1) ||
			 __container_is_listed(CONTAINER(container)->whitelist, item_type, subtype)) &&
			((list_size(CONTAINER(container)->blacklist) < 1) ||
			!__container_is_listed(CONTAINER(container)->blacklist, item_type, subtype));
}

// Same as container_is_valid_item_type, except it *MUST* be in the whitelist and either an empty blacklist or NOT in the blacklist
bool container_filters_for_item_type(OBJ_DATA *container, int item_type, int subtype)
{
	return IS_VALID(container) && IS_CONTAINER(container) &&
			(__container_is_listed(CONTAINER(container)->whitelist, item_type, subtype)) &&
			((list_size(CONTAINER(container)->blacklist) < 1) ||
			!__container_is_listed(CONTAINER(container)->blacklist, item_type, subtype));

}

// Is the primary item_type allowed in the container?
bool container_is_valid_item(OBJ_DATA *container, OBJ_DATA *obj)
{
	int subtype = __item_type_get_subtype(obj);
	return IS_VALID(container) && IS_CONTAINER(container) && IS_VALID(obj) &&
			((list_size(CONTAINER(container)->whitelist) < 1) ||
			 __container_is_listed(CONTAINER(container)->whitelist, obj->item_type, subtype)) &&
			((list_size(CONTAINER(container)->blacklist) < 1) ||
			!__container_is_listed(CONTAINER(container)->blacklist, obj->item_type, subtype));
}

int container_get_content_weight(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (!IS_VALID(container)) return -1;
	if (!IS_CONTAINER(container)) return -1;		// Not a valid container

	int weight = 0;
	OBJ_DATA *o;
	for(o = container->contains; o; o = o->next)
	{
		weight += obj_get_weight(o);
	}

	if (IS_VALID(obj))
	{
		// We have an object, let's see what the total would be with this object
		weight += obj_get_weight(obj);
	}

	return weight;
}

bool container_can_fit_weight(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (CONTAINER(container)->max_weight < 0) return true;

	int weight = container_get_content_weight(container, obj);

	// No container data
	if (weight < 0) return false;

	return (weight <= CONTAINER(container)->max_weight);
}

int container_get_content_volume(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (!IS_VALID(container)) return -1;
	if (!IS_CONTAINER(container)) return -1;		// Not a valid container

	int volume = 0;
	OBJ_DATA *o;
	for(o = container->contains; o; o = o->next)
	{
		volume += get_obj_number(o);
	}

	if (IS_VALID(obj))
	{
		volume += get_obj_number(obj);
	}

	return volume;
}

bool container_can_fit_volume(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (CONTAINER(container)->max_volume < 0) return true;

	int volume = container_get_content_volume(container, obj);

	if (volume < 0) return false;

	return (volume <= CONTAINER(container)->max_volume);
}

// Return true if okay
bool container_check_duplicate(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (!IS_CONTAINER(container)) return false;

	OBJ_DATA *o;

	for(o = container->contains; o; o = o->next)
	{
		// Same index
		if (o->pIndexData == obj->pIndexData)
			return false;
	}

	return true;
}

////////////////////
// FLUID
bool fluid_has_same_fluid(FLUID_CONTAINER_DATA *a, FLUID_CONTAINER_DATA *b)
{
	// Assume both have liquids?
	if (a->liquid != b->liquid) return false;

	return obj_has_same_spells(a->spells, b->spells);
}

void fluid_empty_container(FLUID_CONTAINER_DATA *a)
{
	if (!IS_VALID(a)) return;

	if (a->poison > 0 && a->poison < 100)
	{
		if (number_percent() >= a->poison)
			a->poison = 0;
		else
			a->poison--;
	}
		
	a->amount = 0;
	a->liquid = NULL;
	list_clear(a->spells);
}

////////////////////
// FOOD

bool food_has_buffs(OBJ_DATA *obj)
{
	return IS_VALID(obj) && IS_FOOD(obj) && (list_size(obj->_food->buffs) > 0);
}

bool food_apply_buffs(CHAR_DATA *ch, OBJ_DATA *obj, int level, int duration)
{
	if (!IS_VALID(obj)) return false;
	if (!IS_FOOD(obj)) return false;

	if (list_size(obj->_food->buffs) > 0)
	{
		// Remove the old well fed
		affect_strip(ch, &gsk__well_fed);

		FOOD_BUFF_DATA *buff;
		ITERATOR it;
		iterator_start(&it, obj->_food->buffs);
		while((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
		{
			AFFECT_DATA *aff = new_affect();

			aff->group = WELL_FED_GROUP;
			aff->skill = &gsk__well_fed;

			aff->where = buff->where;
			aff->level = buff->level > 0 ? buff->level : level;
			aff->location = buff->location;
			aff->duration = buff->duration > 0 ? buff->duration : duration;
			aff->modifier = buff->modifier;
			aff->bitvector = buff->bitvector;
			aff->bitvector2 = buff->bitvector2;
			aff->slot = WEAR_NONE;

			affect_join(ch, aff);
		}
		iterator_stop(&it);
	}

	return true;
}

////////////////////
// FURNITURE

char *furniture_get_positional(long field)
{
	if (IS_SET(field, FURNITURE_AT))
		return "at";
	else if (IS_SET(field, FURNITURE_ON))
		return "on";
	else if (IS_SET(field, FURNITURE_IN))
		return "in";
	else if (IS_SET(field, FURNITURE_ABOVE))
		return "above";
	else if (IS_SET(field, FURNITURE_UNDER))
		return "under";

	// Default to "on"
	return "on";
}

char *furniture_get_short_description(OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment)
{
	static char buf[4][MSL / 2];
	static int cnt = 0;

	if (++cnt == 4) cnt = 0;

	if (list_size(FURNITURE(obj)->compartments) > 1)
	{
		snprintf(buf[cnt], (MSL / 2 - 1), "%s of %s",
			compartment->short_descr,
			obj->short_descr);
	}
	else
	{
		strncpy(buf[cnt], obj->short_descr, (MSL/2 - 1));
	}

	return buf[cnt];
}

FURNITURE_COMPARTMENT *furniture_get_compartment(OBJ_DATA *obj, int *index, char *name)
{
	if (!IS_FURNITURE(obj)) return NULL;

	ITERATOR it;
	FURNITURE_COMPARTMENT *compartment;

	iterator_start(&it, FURNITURE(obj)->compartments);
	while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
	{
		if (is_name(name, compartment->name) && (!index || (*index)++ == 1))
			break;
	}
	iterator_stop(&it);

	return compartment;
}

FURNITURE_COMPARTMENT *furniture_find_compartment(CHAR_DATA *ch, OBJ_DATA *obj, char *argument, char *verb)
{
	FURNITURE_COMPARTMENT *compartment = NULL;

	// VERB <furniture> <compartment>
	if (argument[0] != '\0')
	{
		char arg2[MIL];
		int count = number_argument(argument, arg2);

		compartment = furniture_get_compartment(obj, &count, arg2);

		if (!IS_VALID(compartment))
			act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	}
	else if (list_size(FURNITURE(obj)->compartments) < 1)
	{
		act("There is no where to $t on $p.", ch, NULL, NULL, obj, NULL, verb, NULL, TO_CHAR);
	}
	else if (FURNITURE(obj)->main_compartment < 1 || list_size(FURNITURE(obj)->compartments) > 1)
	{
		char buf[MSL];
		int cnt = 0;

		act("Please specify where to $t on $p:", ch, NULL, NULL, obj, NULL, verb, NULL, TO_CHAR);
		ITERATOR it;
		iterator_start(&it, FURNITURE(obj)->compartments);
		while((compartment = (FURNITURE_COMPARTMENT *)iterator_nextdata(&it)))
		{
			sprintf(buf, "%-20s", compartment->short_descr);
			send_to_char(buf, ch);

			if (++cnt == 3)
			{
				cnt = 0;
				send_to_char("\n\r", ch);
			}
		}
		iterator_stop(&it);

		if (cnt > 0)
		{
			send_to_char("\n\r", ch);
		}
	}
	else
		compartment = (FURNITURE_COMPARTMENT *)list_nthdata(FURNITURE(obj)->compartments, FURNITURE(obj)->main_compartment);

	return compartment;
}

int furniture_count_users(OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment)
{
	if (!IS_FURNITURE(obj)) return 0;

	if (obj->in_room == NULL) return 0;

	int count = 0;
	if (IS_VALID(compartment))
	{
		for(CHAR_DATA *ch = obj->in_room->people; ch; ch = ch->next_in_room)
		{
			if (ch->on_compartment == compartment)
				count++;
		}
	}
	else
	{
		for(CHAR_DATA *ch = obj->in_room->people; ch; ch = ch->next_in_room)
		{
			if (ch->on == obj)
				count++;
		}
	}

	return count;
}

// The compartment is treated as an interior space
bool compartment_is_inside(FURNITURE_COMPARTMENT *compartment)
{
	return IS_VALID(compartment) && IS_SET(compartment->flags, COMPARTMENT_INSIDE);
}

// The compartment is closed off
//  Later checks for transparency will be done when needed
bool compartment_is_closed(FURNITURE_COMPARTMENT *compartment)
{
	return compartment_is_inside(compartment) && IS_SET(compartment->flags, COMPARTMENT_CLOSED);
}

////////////////////
// LIGHT

bool light_char_has_light(CHAR_DATA *ch)
{
	OBJ_DATA *obj;
	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
		if (obj->wear_loc != WEAR_NONE && IS_LIGHT(obj))
		{
			// Only need one active light source
			if (IS_SET(LIGHT(obj)->flags, LIGHT_IS_ACTIVE))
				return true;
		}
	}

	return false;
}