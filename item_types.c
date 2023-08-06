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
		assigned[i] = FALSE;

	assigned[pObjIndex->item_type] = TRUE;
	if (IS_CONTAINER(pObjIndex))	assigned[ITEM_CONTAINER] = TRUE;
	if (IS_FOOD(pObjIndex))			assigned[ITEM_FOOD] = TRUE;
	if (IS_LIGHT(pObjIndex))		assigned[ITEM_LIGHT] = TRUE;
	if (IS_MONEY(pObjIndex))		assigned[ITEM_MONEY] = TRUE;
}


bool obj_index_can_add_item_type(OBJ_INDEX_DATA *pObjIndex, int item_type)
{
	// Only the PRIMARY object matters
	switch(pObjIndex->item_type)
	{
		case ITEM_CONTAINER:
			if (item_type == ITEM_ARMOUR) return TRUE;
			if (item_type == ITEM_CART) return TRUE;
			if (item_type == ITEM_FOUNTAIN) return TRUE;
			if (item_type == ITEM_FURNITURE) return TRUE;
			if (item_type == ITEM_JEWELRY) return TRUE;
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_PORTAL) return TRUE;
			return FALSE;

		case ITEM_JEWELRY:
			if (item_type == ITEM_ARMOUR) return TRUE;
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_LIGHT) return TRUE;
			return FALSE;
		
		case ITEM_CART:
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_FURNITURE) return TRUE;
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_PORTAL) return TRUE;
			return FALSE;

		case ITEM_FURNITURE:
			if (item_type == ITEM_CART) return TRUE;
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_FOUNTAIN) return TRUE;
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_PORTAL) return TRUE;
			return FALSE;

		case ITEM_ARMOUR:
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_JEWELRY) return TRUE;
			if (item_type == ITEM_RANGED_WEAPON) return TRUE;
			if (item_type == ITEM_LIGHT) return TRUE;
			return FALSE;

		case ITEM_WEAPON:
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_STAFF) return TRUE;
			if (item_type == ITEM_WAND) return TRUE;
			return FALSE;

		case ITEM_WAND:
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_STAFF) return TRUE;
			if (item_type == ITEM_WEAPON) return TRUE;
			return FALSE;

		case ITEM_STAFF:
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_WAND) return TRUE;
			if (item_type == ITEM_WEAPON) return TRUE;
			return FALSE;

		case ITEM_RANGED_WEAPON:
			if (item_type == ITEM_ARMOUR) return TRUE;
			if (item_type == ITEM_CONTAINER) return TRUE;
			return FALSE;

		case ITEM_FOUNTAIN:
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_FURNITURE) return TRUE;
			if (item_type == ITEM_PORTAL) return TRUE;
			if (item_type == ITEM_CART) return TRUE;
			return FALSE;

		case ITEM_LIGHT:
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_DRINK_CON) return TRUE;
			if (item_type == ITEM_FOUNTAIN) return TRUE;
			if (item_type == ITEM_FURNITURE) return TRUE;
			if (item_type == ITEM_PORTAL) return TRUE;
			return FALSE;

		case ITEM_PORTAL:
			if (item_type == ITEM_LIGHT) return TRUE;
			if (item_type == ITEM_CONTAINER) return TRUE;
			if (item_type == ITEM_FURNITURE) return TRUE;
			if (item_type == ITEM_FOUNTAIN) return TRUE;
			if (item_type == ITEM_CART) return TRUE;
			return FALSE;
	}

	return FALSE;
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
		if (obj->item_type == ITEM_WEAPON)	// TODO: IS_WEAPON(obj)
		{
			return obj->value[0];		// Weapon type
		}
	}

	return -1;	// No subtype
}

int objindex_get_subtype(OBJ_INDEX_DATA *pObjIndex)
{
	if (pObjIndex)
	{
		if (pObjIndex->item_type == ITEM_WEAPON) // TODO: IS_WEAPON
		{
			return pObjIndex->value[0];
		}
	}

	return -1;
}

// Open Close Lock Unlock
bool obj_oclu_ambiguous(OBJ_DATA *obj)
{
	int types = 0;

	if (IS_CONTAINER(obj)) types++;
	if (IS_FURNITURE(obj))
	{
		// Can target it without a "context" if this is *only* furniture out of this list
		if (FURNITURE(obj)->main_compartment > 0)
			types++;
		else
			types+=list_size(FURNITURE(obj)->compartments);
	}

	if (IS_PORTAL(obj)) types++;
	// if (IS_BOOK(obj)) types++;

	return types > 1;
}

void obj_oclu_show_parts(CHAR_DATA *ch, OBJ_DATA *obj)
{
	int i = 0;

	// IS_BOOK

	if (IS_CONTAINER(obj))
	{
		if (i > 0) send_to_char(", ", ch);
		send_to_char(CONTAINER(obj)->short_descr, ch);
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

		context->is_default = FALSE;

		// IS_BOOK

		if (IS_CONTAINER(obj))
		{
			if (is_name(arg, CONTAINER(obj)->name) && (count-- == 1))
			{
				context->item_type = ITEM_CONTAINER;
				context->which = CONTEXT_CONTAINER;
				context->flags = &(CONTAINER(obj)->flags);
				context->label = CONTAINER(obj)->short_descr;
				context->lock = &(CONTAINER(obj)->lock);
				return TRUE;
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

			if (compartment) return TRUE;
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
				return TRUE;
			}
		}
	}
	// Not dealing with an ambiguous object
	else if (!obj_oclu_ambiguous(obj))
	{
		context->is_default = TRUE;
		// IS_BOOK

		if (IS_CONTAINER(obj))
		{
			context->item_type = ITEM_CONTAINER;
			context->which = CONTEXT_CONTAINER;
			context->flags = &(CONTAINER(obj)->flags);
			context->label = CONTAINER(obj)->short_descr;
			context->lock = &(CONTAINER(obj)->lock);
			return TRUE;
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
				return TRUE;
			}
		}

		if (IS_PORTAL(obj))
		{
			context->item_type = ITEM_PORTAL;
			context->which = CONTEXT_PORTAL;
			context->flags = &(PORTAL(obj)->exit);
			context->label = PORTAL(obj)->short_descr;
			context->lock = &(PORTAL(obj)->lock);
			return TRUE;
		}
	}

	return FALSE;
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
	if (CONTAINER(container)->max_weight < 0) return TRUE;

	int weight = container_get_content_weight(container, obj);

	// No container data
	if (weight < 0) return FALSE;

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
	if (CONTAINER(container)->max_volume < 0) return TRUE;

	int volume = container_get_content_volume(container, obj);

	if (volume < 0) return FALSE;

	return (volume <= CONTAINER(container)->max_volume);
}

// Return true if okay
bool container_check_duplicate(OBJ_DATA *container, OBJ_DATA *obj)
{
	if (!IS_CONTAINER(container)) return FALSE;

	OBJ_DATA *o;

	for(o = container->contains; o; o = o->next)
	{
		// Same index
		if (o->pIndexData == obj->pIndexData)
			return FALSE;
	}

	return TRUE;
}

////////////////////
// FOOD

bool food_has_buffs(OBJ_DATA *obj)
{
	return IS_VALID(obj) && IS_FOOD(obj) && (list_size(obj->_food->buffs) > 0);
}

bool food_apply_buffs(CHAR_DATA *ch, OBJ_DATA *obj, int level, int duration)
{
	if (!IS_VALID(obj)) return FALSE;
	if (!IS_FOOD(obj)) return FALSE;

	if (list_size(obj->_food->buffs) > 0)
	{
		// Remove the old well fed
		affect_strip(ch, gsn_well_fed);

		FOOD_BUFF_DATA *buff;
		ITERATOR it;
		iterator_start(&it, obj->_food->buffs);
		while((buff = (FOOD_BUFF_DATA *)iterator_nextdata(&it)))
		{
			AFFECT_DATA *aff = new_affect();

			aff->group = WELL_FED_GROUP;
			aff->type = gsn_well_fed;

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

	return TRUE;
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
				return TRUE;
		}
	}

	return FALSE;
}