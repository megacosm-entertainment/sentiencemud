/***************************************************************************
 *  File: olc_act.c                                                        *
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

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../../strings.h"


MEDIT(medit_show)
{
    MOB_INDEX_DATA *pMob;
    char buf[MAX_STRING_LENGTH];
//	ITERATOR it;
//	PROG_LIST *trigger;
    BUFFER *buffer;

    EDIT_MOB(ch, pMob);

    buffer = new_buf();

    sprintf(buf, "Name:         {C[{x%s{C]{x\n\rArea:         {C[{x%5ld{C]{x %s\n\r",
        pMob->player_name,
        !pMob->area ? -1        : pMob->area->anum,
        !pMob->area ? "No Area" : pMob->area->name);
    add_buf(buffer, buf);
    sprintf(buf, "Loaded:       {C[{x%d{C]{x\n\r", pMob->count);
    add_buf(buffer, buf);

    sprintf(buf, "Sig:          {C[{x%s{C]{x   Creator: {C[{x%s{C]{x\n\r",
        pMob->sig, pMob->creator_sig);
    add_buf(buffer, buf);

    sprintf(buf, "Act:          {C[{x%s{C]{x\n\r",
        bitmatrix_string(act_flagbank, pMob->act));
    add_buf(buffer, buf);
/*
    sprintf(buf, "Act2:         {C[{x%s{C]{x\n\r",
        flag_string(act2_flags, pMob->act2));
    add_buf(buffer, buf);
*/
    sprintf(buf, "Vnum:         {C[{x%6ld{C]{x  Body Type: {C[{x%7d{C]{x  Race: {C[{x%s{C]{x\n\r",
        pMob->vnum,
        pMob->body_type,
        pMob->race ? pMob->race->name : "unknown");
    add_buf(buffer, buf);

    sprintf(buf, "Boss:         {C[%s{C]{x\n\r", (pMob->persist ? "{RYES" : "{gno"));
    add_buf(buffer, buf);

    sprintf(buf, "Persist:      {C[%s{C]{x\n\r", (pMob->persist ? "{WON" : "{Doff"));
    add_buf(buffer, buf);

    if(pMob->attacks < 0) {
        sprintf(buf,
            "Level:        {C[{x%6d{C]{x  Align: {C[{x%6d{C]{x   Owner: {C[{x%s{C]{x\n\r"
            "Hitroll:      {C[{x%6d{C]{x  DamType: {C[{x%s{C]{x\n\r"
            "Movement:     {C[{x%6ld{C]{x  Number of attacks: {C[{Yscripted{C]{x\n\r",
            pMob->level,	pMob->alignment, pMob->owner,
            pMob->hitroll,	attack_table[pMob->dam_type].name,
            pMob->move);
    } else {
        sprintf(buf,
            "Level:        {C[{x%6d{C]{x  Align: {C[{x%6d{C]{x   Owner: {C[{x%s{C]{x\n\r"
            "Hitroll:      {C[{x%6d{C]{x  DamType: {C[{x%s{C]{x\n\r"
            "Movement:     {C[{x%6ld{C]{x  Number of attacks: {C[{x%d{C]{x\n\r",
            pMob->level,	pMob->alignment, pMob->owner,
            pMob->hitroll,	attack_table[pMob->dam_type].name,
            pMob->move, pMob->attacks );
    }
    add_buf(buffer, buf);

    sprintf(buf, "Hit dice:     {C[{x%2dd%-3d+%4d{C]{x ",
        pMob->hit.number,
        pMob->hit.size,
        pMob->hit.bonus);
    add_buf(buffer, buf);

    sprintf(buf, "Damage dice:  {C[{x%2dd%-3d+%4d{C]{x ",
        pMob->damage.number,
        pMob->damage.size,
        pMob->damage.bonus);
    add_buf(buffer, buf);

    sprintf(buf, "Mana dice:    {C[{x%2dd%-3d+%4d{C]{x\n\r",
        pMob->mana.number,
        pMob->mana.size,
        pMob->mana.bonus);
    add_buf(buffer, buf);

    sprintf(buf, "Affected by:  {C[{x%s{C]{x\n\r",
        bitvector_string(2, pMob->affected_by[0], affect_flags, pMob->affected_by[1], affect2_flags));
    add_buf(buffer, buf);
/*
    sprintf(buf, "Affected by2: {C[{x%s{C]{x\n\r",
        flag_string(affect2_flags, pMob->affected_by2));
    add_buf(buffer, buf);
*/
    sprintf(buf, "Armour:        {C[{xpierce: %d  bash: %d  slash: %d  magic: %d{C]{x\n\r",
        pMob->ac[AC_PIERCE], pMob->ac[AC_BASH],
        pMob->ac[AC_SLASH],  pMob->ac[AC_EXOTIC]);
    add_buf(buffer, buf);

    sprintf(buf, "Parts:        {C[{x%s{C]{x\n\r", flag_string(part_flags, pMob->parts));
    add_buf(buffer, buf);

    sprintf(buf, "Imm:          {C[{x%s{C]{x\n\r", flag_string(imm_flags, pMob->imm_flags));
    add_buf(buffer, buf);

    sprintf(buf, "Res:          {C[{x%s{C]{x\n\r", flag_string(res_flags, pMob->res_flags));
    add_buf(buffer, buf);

    sprintf(buf, "Vuln:         {C[{x%s{C]{x\n\r", flag_string(vuln_flags, pMob->vuln_flags));
    add_buf(buffer, buf);

    sprintf(buf, "Off:          {C[{x%s{C]{x\n\r", flag_string(off_flags,  pMob->off_flags));
    add_buf(buffer, buf);

    sprintf(buf, "Size:         {C[{x%s{C]{x\n\r", flag_string(size_flags, pMob->size));
    add_buf(buffer, buf);

    sprintf(buf, "Material:     {C[{x%s{C]{x\n\r", pMob->material);
    add_buf(buffer, buf);

    sprintf(buf, "Start pos:    {C[{x%s{C]{x\n\r", flag_string(position_flags, pMob->start_pos));
    add_buf(buffer, buf);

    sprintf(buf, "Default pos:  {C[{x%s{C]{x\n\r", flag_string(position_flags, pMob->default_pos));
    add_buf(buffer, buf);

    sprintf(buf, "Wealth:       {C[{x%8ld{C]{x\n\r", pMob->wealth);
    add_buf(buffer, buf);

    sprintf(buf, "Script Kwds:  {C[{x%s{C]{x\n\r", pMob->skeywds);
    add_buf(buffer, buf);

    if (pMob->spec_fun) {
        sprintf(buf, "Spec fun:     {C[{x%s{C]{x\n\r",  spec_name(pMob->spec_fun));
        add_buf(buffer, buf);
    }

    sprintf(buf, "Corpse Type:  {C[{x%s{C]{x\n\r", flag_string(corpse_types, pMob->corpse_type));
    add_buf(buffer, buf);

    if (pMob->corpse) {
        OBJ_INDEX_DATA *obj = get_obj_index(pMob->area, pMob->corpse);
        sprintf(buf, "Corpse Obj:   {C[{x%s{C]{x\n\r",  obj->short_descr);
        add_buf(buffer, buf);
    }

    if (pMob->zombie) {
        OBJ_INDEX_DATA *obj = get_obj_index(pMob->area, pMob->zombie);
        sprintf(buf, "Zombie Obj:   {C[{x%s{C]{x\n\r",  obj->short_descr);
        add_buf(buffer, buf);
    }


    sprintf(buf, "Short descr: %s\n\rLong descr:\n\r     %s", pMob->short_descr, pMob->long_descr);
    add_buf(buffer, buf);

    sprintf(buf, "Description:\n\r%s", pMob->description);
    add_buf(buffer, buf);

    sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pMob->comments);
    add_buf(buffer, buf);


    /*
    if (pMob->next_crew_for_sale != NULL)
    {
    MOB_INDEX_DATA *temp_mob;
    sprintf(buf,"Crew for hire:\n\r"
    "-------------------\n\r");
    send_to_char(buf, ch);

    for (temp_mob = pMob->next_crew_for_sale; temp_mob != NULL; temp_mob = temp_mob->next_crew_for_sale)
    {
    sprintf(buf, "%-16s %ld\n\r", temp_mob->short_descr, temp_mob->vnum);
    send_to_char(buf, ch);
    }
    }
    */

    if (pMob->pShop) {
        SHOP_DATA *pShop;
        int iTrade;

        pShop = pMob->pShop;

        sprintf(buf,
            "Shop data for {C[{x%5ld{C]{x:\n\r"
            "  Markup for purchaser: %d%%\n\r"
            "  Markdown for seller:  %d%%\n\r",
            pShop->keeper, pShop->profit_buy, pShop->profit_sell);
        add_buf(buffer, buf);
        sprintf(buf, "  Hours: %d to %d.\n\r", pShop->open_hour, pShop->close_hour);
        add_buf(buffer, buf);

        if( pShop->restock_interval > 0 )
            sprintf(buf, "  Restocking: %d (minutes)\n\r", pShop->restock_interval);
        else
            sprintf(buf, "  Restocking: disabled\n\r");
        add_buf(buffer, buf);

        sprintf(buf, "  Discount Rate: %d%%\n\r", pShop->discount);
        add_buf(buffer, buf);

        sprintf(buf, "  Flags: %s\n\r", flag_string(shop_flags, pShop->flags));
        add_buf(buffer, buf);

        for (iTrade = 0; iTrade < MAX_TRADE; iTrade++) {
            if (pShop->buy_type[iTrade]) {
                if (!iTrade) {
                    add_buf(buffer, "  Number Trades Type\n\r");
                    add_buf(buffer, "  ------ -----------\n\r");
                }
                sprintf(buf, "  {C[{x%4d{C]{x %s\n\r", iTrade, flag_string(type_flags, pShop->buy_type[iTrade]));
                add_buf(buffer, buf);
            }
        }

        if(pShop->shipyard > 0)
        {
            WILDS_DATA *wilds = get_wilds_from_uid(NULL, pShop->shipyard);

            sprintf(buf, "  Shipyard: %s (%ld) at (%d,%d) to (%d,%d)\n\r",
                wilds?wilds->name:"(null)", pShop->shipyard,
                pShop->shipyard_region[0][0], pShop->shipyard_region[0][1],
                pShop->shipyard_region[1][0], pShop->shipyard_region[1][1]);
            add_buf(buffer, buf);

            sprintf(buf, "            %s\n\r", pShop->shipyard_description);
            add_buf(buffer, buf);
        }

        if(pShop->stock != NULL)
        {
            SHOP_STOCK_DATA *pStock;
            int iStock;
            char lvl[MIL];
            char qty[MIL];
            char pricing[MIL];
            char typ[MIL];
            char hours[MIL];
            char item[MIL];
            char disc[MIL];
            int hwidth, lwidth, qwidth, pwidth;

            for(iStock = 1, pStock = pShop->stock;pStock;pStock = pStock->next, iStock++)
            {
                if(iStock == 1)
                {
                    add_buf(buffer, "{G  Stock# Level Quantity Sng Hours    Price(s)    Disc                   Item{x\n\r");
                    add_buf(buffer, "{G  ------ ----- -------- --- ----- -------------- ---- --------------------------------------{x\n\r");
                }

                if( pStock->level > 0 )
                {
                    sprintf(lvl, "{Y%d{x", pStock->level);
                }
                else
                {
                    strcpy(lvl, "{GAuto{x");
                }
                lwidth = get_colour_width(lvl) + 5;

                if( pStock->quantity > 0 )
                {
                    if( pStock->restock_rate > 0 )
                    {
                        sprintf(qty, "{W%d{x / {W%d{x", pStock->quantity, pStock->restock_rate);
                    }
                    else
                    {
                        sprintf(qty, "{W%d{x / {D--{x", pStock->quantity);
                    }
                }
                else
                {
                    strcpy(qty, "   {D--{x   ");
                }
                qwidth = get_colour_width(qty) + 8;

                if( pStock->duration > 0 )
                {
                    sprintf(hours, "{G%d{x", pStock->duration);
                }
                else
                {
                    strcpy(hours, " {D---{x ");
                }
                hwidth = get_colour_width(hours) + 5;

                if( !IS_NULLSTR(pStock->custom_price) )
                {
                    strncpy(pricing, pStock->custom_price, sizeof(pricing)-3);
                    strcat(pricing, "{x");
                    strcpy(disc, " {D--{x ");
                }
                else
                {
                    pricing[0] = '\0';
                    int pj = 0;

                    if( pStock->silver > 0)
                    {
                        long silver = pStock->silver % 100;
                        long gold = pStock->silver / 100;

                        if( gold > 0 )
                        {
                            if( silver > 0 )
                            {
                                pj = sprintf(pricing, "{x%ld{Yg{x%ld{Ws{x", gold, silver);
                            }
                            else
                            {
                                pj = sprintf(pricing, "{x%ld{Yg{x", gold);
                            }
                        }
                        else
                        {
                            pj = sprintf(pricing, "{x%ld{Ws{x", silver);
                        }
                    }

                    if( pStock->qp > 0 )
                    {
                        if( pj > 0 )
                        {
                            pricing[pj++] = ',';
                            pricing[pj++] = ' ';
                        }

                        pj += sprintf(pricing+pj, "{x%ld{Gqp{x", pStock->qp);
                    }

                    if( pStock->dp > 0 )
                    {
                        if( pj > 0 )
                        {
                            pricing[pj++] = ',';
                            pricing[pj++] = ' ';
                        }

                        pj += sprintf(pricing+pj, "{x%ld{Mdp{x", pStock->dp);
                    }

                    if( pStock->pneuma > 0 )
                    {
                        if( pj > 0 )
                        {
                            pricing[pj++] = ',';
                            pricing[pj++] = ' ';
                        }

                        pj += sprintf(pricing+pj, "{x%ld{Cpn{x", pStock->pneuma);
                    }
                    pricing[pj] = '\0';
                    sprintf(disc, "%3d%%", pStock->discount);
                }
                pwidth = get_colour_width(pricing) + 14;

                switch(pStock->type)
                {
                case STOCK_OBJECT:
                    strcpy(typ,"{GOBJECT{x  ");
                    if( pStock->entity.wnum.vnum > 0 ) {

                        OBJ_INDEX_DATA *obj = pStock->entity.wnum.pArea ? 
                            get_obj_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_obj_index(pMob->area, pStock->entity.wnum.vnum);

                        if( !obj ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", obj->short_descr, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", obj->short_descr, pStock->entity.wnum.vnum);
                            }
                        }
                    }
                    else
                        strcpy(item, "-invalid-");

                    break;
                case STOCK_PET:
                    strcpy(typ,"{GPET{x     ");
                    if( pStock->entity.wnum.vnum > 0 ) {

                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);

                        if( !mob ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", mob->short_descr, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", mob->short_descr, pStock->entity.wnum.vnum);
                            }
                        }
                    }
                    else
                        strcpy(item, "-invalid-");
                    break;
                case STOCK_MOUNT:
                    strcpy(typ,"{GMOUNT{x   ");
                    if( pStock->entity.wnum.vnum > 0 ) {

                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);

                        if( !mob ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", mob->short_descr, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", mob->short_descr, pStock->entity.wnum.vnum);
                            }
                        }
                    }
                    else
                        strcpy(item, "-invalid-");
                    break;
                case STOCK_GUARD:
                    strcpy(typ,"{GGUARD{x   ");
                    if( pStock->entity.wnum.vnum > 0 ) {

                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);

                        if( !mob ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", mob->short_descr, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", mob->short_descr, pStock->entity.wnum.vnum);
                            }
                        }
                    }
                    else
                        strcpy(item, "-invalid-");
                    break;

                case STOCK_CREW:
                    strcpy(typ,"{GCREW{x    ");
                    if( pStock->entity.wnum.vnum > 0 ) {

                        MOB_INDEX_DATA *mob = pStock->entity.wnum.pArea ?
                            get_mob_index(pStock->entity.wnum.pArea, pStock->entity.wnum.vnum) :
                            get_mob_index(pMob->area, pStock->entity.wnum.vnum);

                        if( !mob || !mob->pCrew ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", mob->short_descr, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", mob->short_descr, pStock->entity.wnum.vnum);
                            }
                        }
                    }
                    else
                        strcpy(item, "-invalid-");
                    break;

                case STOCK_SHIP:
                    strcpy(typ,"{GSHIP{x    ");
                    if( pStock->entity.wnum.vnum > 0 )
                    {
                        SHIP_INDEX_DATA *ship_index = get_ship_index(pStock->entity.wnum.vnum);

                        if( !ship_index ) {
                            strcpy(item, "-invalid-");
                        }
                        else
                        {
                            if (pStock->entity.wnum.pArea && pStock->entity.wnum.pArea != pMob->area) {
                                sprintf(item, "%s (%ld#%ld)", ship_index->name, pStock->entity.wnum.pArea->uid, pStock->entity.wnum.vnum);
                            } else {
                                sprintf(item, "%s (%ld)", ship_index->name, pStock->entity.wnum.vnum);
                            }
                        }

                    }
                    else
                        strcpy(item, "-invalid-");
                    break;
                case STOCK_CUSTOM:
                    strcpy(typ,"{GCUSTOM{x  ");
                    if(IS_NULLSTR(pStock->custom_keyword))
                    {
                        strcpy(item, "-invalid stock item-");
                    }
                    else
                    {
                        strcpy(item, pStock->custom_keyword);
                    }
                    break;
                }

                sprintf(buf, "  {G[{x%4d{G]{x %-*s %*s  %s  %*s %-*s %s %s%s\n\r", iStock, lwidth, lvl, qwidth, qty, (pStock->singular?"{RY{x":"{GN{x"), hwidth, hours, pwidth, pricing, disc, typ, item);
                add_buf(buffer,buf);

                if( !IS_NULLSTR(pStock->custom_descr) )
                {
                    sprintf(buf, "                                                              - %s\n\r", pStock->custom_descr);
                    add_buf(buffer, buf);
                }
            }
        }
    }

    if ( IS_VALID(pMob->pCrew) )
    {
        add_buf(buffer, "{CShip Crew Data:{x\n\r");
        add_buf(buffer, "{C================================{x\n\r");

        sprintf(buf, "{CMinimum Rank{c:      {WNYI{x\n\r");
        add_buf(buffer, buf);

        sprintf(buf, "{CScouting Rating{c:   {C[{x%d%%{C]{x\n\r", pMob->pCrew->scouting);
        add_buf(buffer, buf);

        sprintf(buf, "{CGunning Rating{c:    {C[{x%d%%{C]{x\n\r", pMob->pCrew->gunning);
        add_buf(buffer, buf);

        sprintf(buf, "{COarring Rating{c:    {C[{x%d%%{C]{x\n\r", pMob->pCrew->oarring);
        add_buf(buffer, buf);

        sprintf(buf, "{CMechanics Rating{c:  {C[{x%d%%{C]{x\n\r", pMob->pCrew->mechanics);
        add_buf(buffer, buf);

        sprintf(buf, "{CNavigation Rating{c: {C[{x%d%%{C]{x\n\r", pMob->pCrew->navigation);
        add_buf(buffer, buf);

        sprintf(buf, "{CLeadership Rating{c: {C[{x%d%%{C]{x\n\r", pMob->pCrew->leadership);
        add_buf(buffer, buf);

        add_buf(buffer, "\n\r");
    }

    if (pMob->pQuestor)
    {
        QUESTOR_DATA *questor = pMob->pQuestor;

        add_buf(buffer, "{YQuestor data:\n\r");

        sprintf(buf, "  {YScroll Vnum: %ld\n\r", questor->scroll);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->keywords))
            sprintf(buf, "  {YKeywords: (empty){x\n\r");
        else
            sprintf(buf, "  {YKeywords: %s{x\n\r", questor->keywords);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->short_descr))
            sprintf(buf, "  {YShort Description: (empty){x\n\r");
        else
            sprintf(buf, "  {YShort Description: %s{x\n\r", questor->short_descr);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->long_descr))
            sprintf(buf, "  {YDescription: (empty){x\n\r");
        else
            sprintf(buf, "  {YDescription: %s{x\n\r", questor->long_descr);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->header))
            sprintf(buf, "  {YHeader: (empty){x\n\r");
        else
            sprintf(buf, "  {YHeader:\n\r%s{x\n\r", questor->header);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->footer))
            sprintf(buf, "  {YFooter: (empty){x\n\r");
        else
            sprintf(buf, "  {YFooter:\n\r%s{x\n\r", questor->footer);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->prefix))
            sprintf(buf, "  {YPrefix: (empty){x\n\r");
        else
            sprintf(buf, "  {YPrefix: %s{x\n\r", questor->prefix);
        add_buf(buffer, buf);

        if(IS_NULLSTR(questor->suffix))
            sprintf(buf, "  {YSuffix: (empty){x\n\r");
        else
            sprintf(buf, "  {YSuffix: %s{x\n\r", questor->suffix);
        add_buf(buffer, buf);

        if( questor->line_width > 0 )
            sprintf(buf, "  {YWidth:  %d{x\n\r", questor->line_width);
        else
            sprintf(buf, "  {YWidth:  disabled{x\n\r");
        add_buf(buffer, buf);
        add_buf(buffer, "\n\r");
    }

    if (pMob->progs)
        olc_show_progs(buffer, pMob->progs, PRG_MPROG, "MobProg Vnum");

    if (pMob->index_vars)
        olc_show_index_vars(buffer, pMob->index_vars);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}

MEDIT(medit_next)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *nextMob = NULL;
    long next_vnum;

    EDIT_MOB(ch, pMob);

    next_vnum = pMob->vnum;

    next_vnum++;
    while (nextMob == NULL
    && next_vnum <= pMob->area->max_vnum)
    {
        nextMob = get_mob_index(pMob->area, next_vnum);
    next_vnum++;
    }

    if (nextMob == NULL)
    {
    send_to_char("No next mob in area.\n\r", ch);
    }
    else
    {
    edit_done(ch);
    ch->desc->pEdit = (void *)nextMob;
    ch->desc->editor = ED_MOBILE;
    }
    return false;
}

MEDIT(medit_persist)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);


    if (!str_cmp(argument,"on")) {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        pMob->persist = true;
        use_imp_sig(pMob, NULL);
        send_to_char("Persistance enabled.\n\r", ch);
    } else if (!str_cmp(argument,"off")) {
        pMob->persist = false;
        send_to_char("Persistance disabled.\n\r", ch);
    } else {
        send_to_char("Usage: persist on/off\n\r", ch);
        return false;
    }

    return true;
}

MEDIT(medit_boss)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (!str_cmp(argument,"on")) {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        pMob->boss = true;
        use_imp_sig(pMob, NULL);
        send_to_char("Boss status enabled.\n\r", ch);
    } else if (!str_cmp(argument,"off")) {
        pMob->boss= false;
        send_to_char("Boss status disabled.\n\r", ch);
    } else {
        send_to_char("Usage: boss on/off\n\r", ch);
        return false;
    }

    return true;
}

MEDIT(medit_prev)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *prevMob = NULL;
    long prev_vnum;

    EDIT_MOB(ch, pMob);

    prev_vnum = pMob->vnum;

    prev_vnum--;
    while (prevMob == NULL
    && prev_vnum >= pMob->area->min_vnum)
    {
    prevMob = get_mob_index(pMob->area, prev_vnum);
    prev_vnum--;
    }

    if (prevMob == NULL)
    {
    send_to_char("No previous mob in area.\n\r", ch);
    }
    else
    {
    edit_done(ch);
    ch->desc->pEdit = (void *)prevMob;
    ch->desc->editor = ED_MOBILE;
    }
    return false;
}

MEDIT(medit_attacks)
{
    MOB_INDEX_DATA *pMob;
    int value;

    EDIT_MOB(ch, pMob);


    if (!str_prefix(argument,"scripted"))
        value = -1;
    else if ((value = atoi(argument)) < 0 || value > 10) {
        send_to_char("Invalid number.\n\r", ch);
        return false;
    }

    pMob->attacks = value;
    send_to_char("Number of attacks set.\n\r", ch);
    return true;
}

MEDIT(medit_owner)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  owner [string]\n\r", ch);
    return false;
    }

    free_string(pMob->owner);
    pMob->owner = str_dup(argument);

    send_to_char("Owner set.\n\r", ch);
    return true;
}

MEDIT(medit_create)
{
    MOB_INDEX_DATA *pMob;
    AREA_DATA *pArea;
    long  value;
    int  iHash;
    long auto_vnum = 0;

    // Auto-vnum: if no argument or argument is 0, find next available vnum in current area
    if (argument[0] == '\0' || !str_cmp(argument, "0"))
    {
    MOB_INDEX_DATA *temp_mob;

    auto_vnum = ch->in_room->area->min_vnum;
    temp_mob = get_mob_index(ch->in_room->area, auto_vnum);
    if (temp_mob != NULL)
    {
        while (temp_mob != NULL)
        {
        temp_mob = get_mob_index(ch->in_room->area, auto_vnum);
        if (temp_mob == NULL) break;
        auto_vnum++;
        }
    }

    if (auto_vnum > ch->in_room->area->max_vnum)
    {
        send_to_char("Sorry, this area has no more space left.\n\r", ch);
        return false;
    }
    
    value = auto_vnum;
    pArea = ch->in_room->area;
    }
    else
    {
    // Parse widevnum format
    WNUM wnum;
    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
    if (!parse_widevnum(argument, context, &wnum)) {
        send_to_char("MEdit: Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }
    
    value = wnum.vnum;
    pArea = wnum.pArea;
    }

    if (!pArea)
    {
    send_to_char("MEdit:  That vnum is not assigned an area.\n\r", ch);
    return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
    send_to_char("MEdit:  Vnum in an area you cannot build in.\n\r", ch);
    return false;
    }

    if (get_mob_index(pArea, value))
    {
    send_to_char("MEdit:  Mobile vnum already exists.\n\r", ch);
    return false;
    }

    pMob			= new_mob_index();
    pMob->vnum			= value;
    pMob->area			= pArea;

    if (value > top_vnum_mob)
    top_vnum_mob = value;

    pMob->act[0]			= ACT_IS_NPC;
    pMob->act[1]			= 0;
    iHash			= value % MAX_KEY_HASH;
    pMob->next			= pArea->mob_index_hash[iHash];
    pArea->mob_index_hash[iHash]	= pMob;
    ch->desc->pEdit		= (void *)pMob;


    // Make sure to set minimum level to 1.
    pMob->level = 1;
    set_mob_hitdice(pMob);
    set_mob_damdice(pMob);
    if (!IS_SET(pMob->act[0], ACT_MOUNT))
    set_mob_movedice(pMob);

    send_to_char("Mobile Created.\n\r", ch);
    SET_BIT(pMob->area->area_flags, AREA_CHANGED);
    free_string(pMob->creator_sig);
    pMob->creator_sig = str_dup(ch->name);
    return true;
}

MEDIT(medit_spec)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  spec [special function]\n\r", ch);
    return false;
    }


    if (!str_cmp(argument, "none"))
    {
        pMob->spec_fun = NULL;

        send_to_char("Spec removed.\n\r", ch);
        return true;
    }

    if (spec_lookup(argument))
    {
    pMob->spec_fun = spec_lookup(argument);
    send_to_char("Spec set.\n\r", ch);
    return true;
    }

    send_to_char("MEdit: No such special function.\n\r", ch);
    return false;
}

MEDIT(medit_damtype)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  damtype [damage message]\n\r", ch);
    send_to_char("For a list of damtypes, type '? weapon'.\n\r", ch);
    return false;
    }

    pMob->dam_type = attack_lookup(argument);
    send_to_char("Damage type set.\n\r", ch);
    return true;
}

MEDIT(medit_align)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  alignment [number]\n\r", ch);
    return false;
    }

    pMob->alignment = atoi(argument);

    send_to_char("Alignment set.\n\r", ch);
    return true;
}

MEDIT(medit_level)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    char buf[MSL];

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  level [number]\n\r", ch);
    return false;
    }

    if (atoi(argument) == 0) {
    send_to_char("Sorry, mob levels start at 1.\n\r", ch);
    return false;
    }

    if (atoi(argument) > MAX_MOB_SKILL_LEVEL) {
    sprintf(buf, "Sorry, max mob level is %d.\n\r", MAX_MOB_SKILL_LEVEL);
    return false;
    }

    pMob->level = atoi(argument);

    send_to_char("Level set.\n\r", ch);
    set_mob_hitdice(pMob);
    send_to_char("Hit Dice set.\n\r", ch);
    set_mob_damdice(pMob);
    send_to_char("Damage dice set.\n\r", ch);

    if (!IS_SET(pMob->act[0], ACT_MOUNT)) {
    set_mob_movedice(pMob);
    send_to_char("Movement dice set.\n\r", ch);
    }

    if (IS_SET(pMob->off_flags, OFF_MAGIC))
    {
    set_mob_manadice(pMob);
    send_to_char("Mana dice set.\n\r", ch);
    }

    return true;

}

MEDIT(medit_desc)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    string_append(ch, &pMob->description);
    return true;
    }

    send_to_char("Syntax:  desc    - line edit\n\r", ch);
    return false;
}

MEDIT(medit_comments)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    string_append(ch, &pMob->comments);
    return true;
    }

    send_to_char("Syntax:  desc    - line edit\n\r", ch);
    return false;
}


MEDIT(medit_long)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  long [string]\n\r", ch);
    return false;
    }

    free_string(pMob->long_descr);
    strcat(argument, "{x\n\r");
    pMob->long_descr = str_dup(argument);
    pMob->long_descr[0] = UPPER(pMob->long_descr[0] );

    send_to_char("Long description set.\n\r", ch);
    return true;
}


MEDIT(medit_short)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  short [string]\n\r", ch);
    return false;
    }

    free_string(pMob->short_descr);
    pMob->short_descr = str_dup(argument);

    send_to_char("Short description set.\n\r", ch);
    if (IS_SET(ch->act[0], PLR_AUTOSETNAME))
    {
    free_string(pMob->player_name);
    pMob->player_name = short_to_name(pMob->short_descr);
    send_to_char("Name keywords set.\n\r", ch);
    }
    return true;
}


MEDIT(medit_name)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  name [string]\n\r", ch);
    return false;
    }

    sprintf(name, "%s%c/%s", PLAYER_DIR, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) == NULL)
    {
    free_string(pMob->player_name);
    pMob->player_name = str_dup(argument);

    send_to_char("Name set.\n\r", ch);
    }
    else
    {
    send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
    fclose(fp);
    return false;
    }

    return true;
}


MEDIT(medit_sign)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 154)
    {
    send_to_char("Sorry, only immortals of level 154 and above can do that.\n\r", ch);
    return false;
    }

    free_string(pMob->sig);
    pMob->sig = str_dup(ch->name);

    send_to_char("Mobile signed.\n\r", ch);

    return true;
}

MEDIT(medit_skeywds)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  skwds [string]\n\r", ch);
    return false;
    }

    sprintf(name, "%s%c/%s", PLAYER_DIR, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) == NULL)
    {
    free_string(pMob->skeywds);
    pMob->skeywds = str_dup(argument);

    send_to_char("Script keywords set.\n\r", ch);
    }
    else
    {
    send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
    fclose(fp);
    return false;
    }

    return true;
}


MEDIT(medit_varset)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    return olc_varset(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_varclear)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    return olc_varclear(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_corpsetype)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
        EDIT_MOB(ch, pMob);

        if (!str_cmp(argument, "normal") || !str_cmp(argument, "none")) {
            pMob->corpse_type = RAWKILL_NORMAL;

             send_to_char("Corpse type set.\n\r", ch);
            return true;
        } else if ((value = flag_value(corpse_types, argument)) != NO_FLAG) {
            pMob->corpse_type = value;

             send_to_char("Corpse type set.\n\r", ch);
            return true;
        }
    }

    send_to_char("Syntax: corpsetype [type]\n\r"
          "Type '? corpsetypes' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_corpsevnum)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: corpsevnum [widevnum] (0 to clear)\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "0"))
    {
        send_to_char("Corpse object cleared.\n\r",ch);
        pMob->corpse = 0;
        return true;
    }

    WNUM obj_wnum;
    AREA_DATA *context = strchr(argument, '#') ? pMob->area : NULL;
    if (!parse_widevnum(argument, context, &obj_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if(!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
        send_to_char("Object does not exist.\n\r",ch);
        return false;
    }

    send_to_char("Corpse object vnum set.\n\r",ch);
    pMob->corpse = obj_wnum.vnum;
    return true;
}

MEDIT(medit_zombievnum)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: zombievnum [widevnum] (0 to clear)\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "0"))
    {
        send_to_char("Zombie corpse object cleared.\n\r",ch);
        pMob->zombie = 0;
        return true;
    }

    WNUM obj_wnum;
    AREA_DATA *context = strchr(argument, '#') ? pMob->area : NULL;
    if (!parse_widevnum(argument, context, &obj_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if(!get_obj_index(obj_wnum.pArea, obj_wnum.vnum)) {
        send_to_char("Object does not exist.\n\r",ch);
        return false;
    }

    send_to_char("Zombie corpse object set.\n\r",ch);
    pMob->zombie = obj_wnum.vnum;
    return true;
}

MEDIT(medit_shop)
{
    MOB_INDEX_DATA *pMob;
    char command[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *flag_start;

    argument = one_argument(argument, command);
    flag_start = argument;
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    EDIT_MOB(ch, pMob);

    if (command[0] == '\0')
    {
        send_to_char("Syntax:  shop assign\n\r", ch);
        send_to_char("         shop remove\n\r\n\r", ch);

        send_to_char("         shop discount [0-100] [reset]\n\r", ch);
        send_to_char("         shop flags [flags]\n\r", ch);
        send_to_char("         shop hours [#xopening] [#xclosing]\n\r", ch);
        send_to_char("         shop profit [#xbuying%] [#xselling%]\n\r", ch);
        send_to_char("         shop restock [minutes]\n\r", ch);
        send_to_char("         shop shipyard clear\n\r", ch);
        send_to_char("         shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
        send_to_char("         shop stock add [type] [value]\n\r", ch);
        send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
        send_to_char("         shop stock [#] description [description]\n\r", ch);
        send_to_char("         shop stock [#] duration [#hours|none]\n\r", ch);
        send_to_char("         shop stock [#] level [level]\n\r", ch);
        send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
        send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
        send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
        send_to_char("         shop stock [#] singular\n\r", ch);
        send_to_char("         shop stock [#] remove\n\r", ch);
        send_to_char("         shop type [#x0-4] [item type]\n\r", ch);
        return false;
    }


    if (!str_prefix(command, "hours"))
    {
        if (arg1[0] == '\0' || !is_number(arg1) ||
            argument[0] == '\0' || !is_number(arg2))
        {
            send_to_char("Syntax:  shop hours [#xopening] [#xclosing]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        pMob->pShop->open_hour = atoi(arg1);
        pMob->pShop->close_hour = atoi(arg2);

        send_to_char("Shop hours set.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "restock"))
    {
        if (arg1[0] == '\0' || !is_number(arg1))
        {
            send_to_char("Syntax:  shop restock [minutes]\n\r", ch);
            send_to_char("   Specify at least 10 minutes, or 0 to disable restocking.\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        int interval = atoi(arg1);

        if( interval <= 0 )
        {
            send_to_char("Restocking disabled.\n\r", ch);
            pMob->pShop->restock_interval = 0;
            return true;
        }
        else if( interval < 10 )
        {
            send_to_char("Interval too short.\n\rPlease try at least 10 minutes, or 0 to disable restocking.\n\r", ch);
            return false;
        }
        else
        {
            send_to_char("Restocking changed.\n\r", ch);
            pMob->pShop->restock_interval = interval;
            return true;
        }
    }

    if (!str_prefix(command, "shipyard"))
    {
        char arg3[MIL];
        char arg4[MIL];
        char arg5[MIL];
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);
        argument = one_argument(argument, arg5);

        if( !str_cmp(arg1, "clear") )
        {
            pMob->pShop->shipyard = 0;
            pMob->pShop->shipyard_region[0][0] = 0;
            pMob->pShop->shipyard_region[0][1] = 0;
            pMob->pShop->shipyard_region[1][0] = 0;
            pMob->pShop->shipyard_region[1][1] = 0;

            free_string(pMob->pShop->shipyard_description);
            pMob->pShop->shipyard_description = &str_empty[0];

            send_to_char("Shipyard cleared.\n\r", ch);
            return true;
        }
        if( !is_number(arg1) || !is_number(arg2) || !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || IS_NULLSTR(argument) )
        {
            send_to_char("Syntax:  shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
            send_to_char("         shop shipyard clear\n\r", ch);
            return false;
        }
        long wuid = atol(arg1);
        int x1 = atoi(arg2);
        int y1 = atoi(arg3);
        int x2 = atoi(arg4);
        int y2 = atoi(arg5);

        if( !is_shipyard_valid(wuid, x1, y1, x2, y2) )
        {
            send_to_char("Shipyard not valid.  Please verify wilderness and coordinates.\n\r", ch);
            send_to_char("Make sure Shipyard has safe harbor water tiles next to non-water tiles.\n\r", ch);
            return false;
        }

        pMob->pShop->shipyard = wuid;
        pMob->pShop->shipyard_region[0][0] = x1;
        pMob->pShop->shipyard_region[0][1] = y1;
        pMob->pShop->shipyard_region[1][0] = x2;
        pMob->pShop->shipyard_region[1][1] = y2;

        smash_tilde(argument);
        free_string(pMob->pShop->shipyard_description);
        pMob->pShop->shipyard_description = str_dup(argument);


        send_to_char("Shipyard set.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "discount"))
    {
        if (arg1[0] == '\0' || !is_number(arg1))
        {
            send_to_char("Syntax:  shop discount [0-100] [reset]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        int disc = atoi(arg1);

        if( disc < 0 || disc > 100 )
        {
            send_to_char("Discount must be a percentage (0-100).\n\r", ch);
            return false;
        }

        pMob->pShop->discount = disc;

        if( !str_cmp(arg2, "reset") && pMob->pShop->stock != NULL )
        {
            bool updated = false;
            for(SHOP_STOCK_DATA *stock = pMob->pShop->stock; stock; stock = stock->next)
            {
                if( IS_NULLSTR(stock->custom_keyword) )
                {
                    stock->discount = pMob->pShop->discount;
                    updated = true;
                }
            }

            if( updated )
                send_to_char("Discount changed, and stock updated.\n\r", ch);
            else
                send_to_char("Discount changed.\n\r", ch);
        }
        else
            send_to_char("Discount changed.\n\r", ch);
        return true;
    }


    if (!str_prefix(command, "profit"))
    {
        if (arg1[0] == '\0' || !is_number(arg1) ||
            argument[0] == '\0' || !is_number(arg2))
        {
            send_to_char("Syntax:  shop profit [#xbuying%] [#xselling%]\n\r", ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        pMob->pShop->profit_buy     = atoi(arg1);
        pMob->pShop->profit_sell    = atoi(arg2);

        send_to_char("Shop profit set.\n\r", ch);
        return true;
    }


    if (!str_prefix(command, "type"))
    {
        char buf[MAX_INPUT_LENGTH];
        int value;

        if (arg1[0] == '\0' || !is_number(arg1) || arg2[0] == '\0')
        {
            send_to_char("Syntax:  shop type [#x0-4] [item type]\n\r", ch);
            return false;
        }

        if (atoi(arg1) >= MAX_TRADE)
        {
            sprintf(buf, "MEdit:  May sell %d items max.\n\r", MAX_TRADE);
            send_to_char(buf, ch);
            return false;
        }

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        if ((value = flag_value(type_flags, arg2)) == NO_FLAG)
        {
            send_to_char("MEdit:  That type of item is not known.\n\r", ch);
            return false;
        }

        pMob->pShop->buy_type[atoi(arg1)] = value;

        send_to_char("Shop type set.\n\r", ch);
        return true;
    }

    /* shop assign && shop delete by Phoenix */

    if (!str_prefix(command, "assign"))
    {
        if (pMob->pShop)
        {
            send_to_char("Mob already has a shop assigned to it.\n\r", ch);
            return false;
        }

        pMob->pShop		= new_shop();
        if (!shop_first)
                shop_first	= pMob->pShop;
        if (shop_last)
            shop_last->next	= pMob->pShop;
        shop_last		= pMob->pShop;

        pMob->pShop->keeper	= pMob->vnum;

        send_to_char("New shop assigned to mobile.\n\r", ch);
        return true;
    }

    if (!str_prefix(command, "remove"))
    {
        SHOP_DATA *pShop;

        pShop		= pMob->pShop;
        pMob->pShop	= NULL;

        if (pShop == shop_first)
        {
            if (!pShop->next)
            {
                shop_first = NULL;
                shop_last = NULL;
            }
            else
                shop_first = pShop->next;
        }
        else
        {
            SHOP_DATA *ipShop;

            for (ipShop = shop_first; ipShop; ipShop = ipShop->next)
            {
                if (ipShop->next == pShop)
                {
                    if (!pShop->next)
                    {
                        shop_last = ipShop;
                        shop_last->next = NULL;
                    }
                    else
                        ipShop->next = pShop->next;
                }
            }
        }

        free_shop(pShop);

        send_to_char("Mobile is no longer a shopkeeper.\n\r", ch);
        return true;
    }

    if(!str_prefix(command, "flags"))
    {
        int value;
        if (flag_start[0] != '\0')
        {

            if ((value = flag_value(shop_flags, flag_start)) != NO_FLAG)
            {
                pMob->pShop->flags ^= value;

                send_to_char("Shop flags toggled.\n\r", ch);
                return true;
            }
        }

        send_to_char(	"Syntax: shop flags [flag]\n\r"
                        "Type '? shop' for a list of flags.\n\r", ch);

        return false;
    }

    if(!str_prefix(command, "stock"))
    {
        SHOP_STOCK_DATA *stock;

        if (!pMob->pShop)
        {
            send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
            return false;
        }

        if(arg1[0] == '\0')
        {
            send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
            send_to_char("         shop stock add pet [vnum]\n\r", ch);
            send_to_char("         shop stock add mount [vnum]\n\r", ch);
            send_to_char("         shop stock add guard [vnum]\n\r", ch);
            send_to_char("         shop stock add crew [vnum]\n\r", ch);
            send_to_char("         shop stock add ship [vnum]\n\r", ch);
            send_to_char("         shop stock add custom [keyword]\n\r", ch);
            send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
            send_to_char("         shop stock [#] description [description]\n\r", ch);
            send_to_char("         shop stock [#] duration [#hours|none]\n\r", ch);
            send_to_char("         shop stock [#] level [level]\n\r", ch);
            send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
            send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
            send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
            send_to_char("         shop stock [#] singular\n\r", ch);
            send_to_char("         shop stock [#] remove\n\r", ch);
            return false;
        }

        if(!str_prefix(arg1, "add"))
        {
            if(arg2[0] == '\0' || argument[0] == '\0')
            {
                send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
                send_to_char("         shop stock add pet [vnum]\n\r", ch);
                send_to_char("         shop stock add mount [vnum]\n\r", ch);
                send_to_char("         shop stock add guard [vnum]\n\r", ch);
                send_to_char("         shop stock add crew [vnum]\n\r", ch);
                send_to_char("         shop stock add ship [vnum]\n\r", ch);
                send_to_char("         shop stock add custom [keyword]\n\r", ch);
                return false;
            }

            if(!str_prefix(arg2, "object"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM obj_wnum;
                    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
                    
                    if (!parse_widevnum(argument, context, &obj_wnum)) {
                        send_to_char("Invalid object vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    OBJ_INDEX_DATA *item = obj_wnum.pArea ? 
                        get_obj_index(obj_wnum.pArea, obj_wnum.vnum) :
                        get_obj_index_global(obj_wnum.vnum);
                    
                    if(!item)
                    {
                        send_to_char("Object does not exist.\n\r", ch);
                        return false;
                    }

                    if(item->item_type == ITEM_MONEY)
                    {
                        send_to_char("You cannot sell money.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_OBJECT;
                    stock->entity.wnum = obj_wnum;
                    stock->silver = item->cost;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (OBJECT) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add object [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "pet"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_PET;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 10 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (PET) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add pet [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "mount"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_MOUNT;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 25 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (MOUNT) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add mount [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "guard"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_GUARD;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 50 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (GUARD) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add guard [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "crew"))
            {
                if(argument && argument[0] != '\0')
                {
                    WNUM mob_wnum;
                    AREA_DATA *context = strchr(argument, '#') ? ch->in_room->area : NULL;
                    
                    if (!parse_widevnum(argument, context, &mob_wnum)) {
                        send_to_char("Invalid mob vnum format. Use: vnum, uid#vnum, #vnum, or 'AreaName'#vnum\n\r", ch);
                        return false;
                    }
                    
                    MOB_INDEX_DATA *mob = mob_wnum.pArea ? 
                        get_mob_index(mob_wnum.pArea, mob_wnum.vnum) :
                        get_mob_index_global(mob_wnum.vnum);

                    if(!mob)
                    {
                        send_to_char("Mobile does not exist.\n\r", ch);
                        return false;
                    }

                    if(!mob->pCrew)
                    {
                        send_to_char("Mobile has no Crew definition.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_CREW;
                    stock->entity.wnum = mob_wnum;
                    stock->silver = 50 * mob->level * mob->level;
                    stock->level = mob->level;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (CREW) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add crew [vnum|uid#vnum|#vnum|'AreaName'#vnum]\n\r", ch);
                return false;
            }
            else if(!str_prefix(arg2, "ship"))
            {
                if( !is_shipyard_valid(pMob->pShop->shipyard,
                    pMob->pShop->shipyard_region[0][0],
                    pMob->pShop->shipyard_region[0][1],
                    pMob->pShop->shipyard_region[1][0],
                    pMob->pShop->shipyard_region[1][1]) )
                {
                    send_to_char("Shopkeeper needs to have a valid shipyard defined first before you can add a ship.\n\r", ch);
                    return false;
                }

                if( is_number(argument) )
                {
                    long vnum = atol(argument);
                    SHIP_INDEX_DATA *ship;

                    if( !(ship = get_ship_index(vnum)) )
                    {
                        send_to_char("That ship does not exist.\n\r", ch);
                        return false;
                    }

                    if( !IS_VALID(ship->blueprint) || !ship->ship_object )
                    {
                        send_to_char("Ship is incomplete.  Cannot be sold yet.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_SHIP;
                    stock->entity.wnum.pArea = NULL;
                    stock->entity.wnum.vnum = vnum;
                    stock->silver = 100000;	// Default 1000gold
                    stock->level = 1;
                    stock->discount = pMob->pShop->discount;

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (SHIP) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add ship [vnum]\n\r", ch);
                return false;

            }
            else if(!str_prefix(arg2, "custom"))
            {
                if(!IS_NULLSTR(argument))
                {
                    for(stock = pMob->pShop->stock; stock; stock = stock->next)
                    {
                        if( (stock->type == STOCK_CUSTOM) &&
                            !str_cmp(argument, stock->custom_keyword) )
                        {
                            break;
                        }
                    }

                    if( stock != NULL )
                    {
                        send_to_char("Keyword already used.\n\r", ch);
                        return false;
                    }

                    stock = new_shop_stock();

                    if(!stock)
                    {
                        send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
                        return false;
                    }

                    stock->type = STOCK_CUSTOM;
                    stock->custom_keyword = str_dup(argument);
                    stock->discount = 0;		// They do not handle discounts.
                                                // If you wish to do discounts, that has to be scripted.

                    stock->next = pMob->pShop->stock;
                    pMob->pShop->stock = stock;

                    send_to_char("Stock item (CUSTOM) added.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock add custom [keyword]\n\r", ch);
                return false;
            }

            send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
            send_to_char("         shop stock add pet [vnum]\n\r", ch);
            send_to_char("         shop stock add mount [vnum]\n\r", ch);
            send_to_char("         shop stock add guard [vnum]\n\r", ch);
            send_to_char("         shop stock add custom [keyword]\n\r", ch);
            return false;
        }

        if(is_number(arg1))
        {
            int idx = atoi(arg1);
            stock = get_shop_stock_bypos(pMob->pShop, idx);

            if(!stock)
            {
                send_to_char("Invalid stock number.\n\r", ch);
                return false;
            }


            if(!str_prefix(arg2, "duration"))
            {
                int duration;
                if (!str_prefix(argument, "none"))
                    duration = 0;
                else if (!is_number(argument) || (duration = atoi(argument)) < 1)
                {
                    send_to_char("Please provide a positive number or none.\n\r", ch);
                    return false;
                }

                stock->duration = duration;
                send_to_char("Stock duration changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "price"))
            {
                char arg3[MIL];

                argument = one_argument(argument, arg3);

                if(!str_prefix(arg3, "silver"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Silver price must be a number.\n\r", ch);
                        return false;
                    }

                    int silver = atoi(argument);

                    stock->silver = UMAX(silver, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock silver price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "qp"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Quest point price must be a number.\n\r", ch);
                        return false;
                    }

                    int qp = atoi(argument);

                    stock->qp = UMAX(qp, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock quest point price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "dp"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Deity point price must be a number.\n\r", ch);
                        return false;
                    }

                    int dp = atoi(argument);

                    stock->dp = UMAX(dp, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock deity point price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "pneuma"))
                {
                    if(!is_number(argument))
                    {
                        send_to_char("Pneuma price must be a number.\n\r", ch);
                        return false;
                    }

                    int pneuma = atoi(argument);

                    stock->pneuma = UMAX(pneuma, 0);
                    if( !IS_NULLSTR(stock->custom_price) )
                    {
                        stock->discount = pMob->pShop->discount;
                        free_string(stock->custom_price);
                        stock->custom_price = &str_empty[0];
                    }
                    send_to_char("Stock pneuma price changed.\n\r", ch);
                    return true;
                }

                if(!str_prefix(arg3, "custom"))
                {
                    if(argument[0] == '\0')
                    {
                        send_to_char("Please specify a custom price string.\n\r", ch);
                        send_to_char("Syntax:  shop stock [#] price custom [value]\n\r\n\r", ch);
                        send_to_char("If you wish to clear the custom pricing, select a different pricing type.\n\r", ch);
                        return false;
                    }

                    stock->silver = 0;
                    stock->qp = 0;
                    stock->dp = 0;
                    stock->pneuma = 0;
                    stock->discount = 0;
                    free_string(stock->custom_price);
                    stock->custom_price = str_dup(argument);
                    send_to_char("Stock custom price changed.\n\r", ch);
                    return true;
                }

                send_to_char("Syntax:  shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
                return false;
            }

            if(!str_prefix(arg2, "discount"))
            {
                if( !IS_NULLSTR(stock->custom_price) )
                {
                    send_to_char("Stock items with custom pricing do not receive discounts.\n\r", ch);
                    send_to_char("Those need to be handled in the CUSTOM_PRICE trigger.\n\r", ch);
                    return false;
                }

                if(!is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] discount [0-100]\n\r", ch);
                    return false;
                }

                int disc = atoi(argument);

                if(disc < 0 || disc > 100)
                {
                    send_to_char("Discount must be a percentage (0-100).\n\r", ch);
                    return false;
                }

                stock->discount = disc;
                send_to_char("Stock discount changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "level"))
            {
                if(!is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] level [level]\n\r", ch);
                    return false;
                }

                int lvl = atoi(argument);

                if(lvl < 1)
                {
                    stock->level = 0;
                    send_to_char("Stock level set to automatic.\n\r", ch);
                    return true;
                }

                stock->level = lvl;
                send_to_char("Stock level changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "singular"))
            {
                stock->singular = !stock->singular;
                if(stock->singular)
                    send_to_char("Stock is now singular.\n\r", ch);
                else
                    send_to_char("Stock is no longer singular.\n\r", ch);
                return true;
            }


            if(!str_prefix(arg2, "quantity"))
            {
                if(!str_prefix(argument, "unlimited"))
                {
                    stock->quantity = 0;
                    stock->restock_rate = 0;
                    send_to_char("Stock quantity settings changed.\n\r", ch);
                    return true;
                }

                char arg3[MIL];
                argument = one_argument(argument, arg3);
                if(!is_number(arg3) || !is_number(argument))
                {
                    send_to_char("Syntax:  shop stock [#] quantity [total] [reset rate]\n\r", ch);
                    return false;
                }

                int total = atoi(arg3);
                int rate = atoi(argument);

                if(total < 1)
                {
                    send_to_char("Please specify a positive number for limited quantity.\n\r", ch);
                    return false;
                }

                stock->quantity = total;
                stock->restock_rate = UMAX(rate, 0);		// A rate of zero means it never restock
                send_to_char("Stock quantity settings changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "description"))
            {
                free_string(stock->custom_descr);
                stock->custom_descr = str_dup(argument);

                send_to_char("Stock description changed.\n\r", ch);
                return true;
            }

            if(!str_prefix(arg2, "remove"))
            {
                if( idx < 1 )
                {
                    send_to_char("Please specify a positive number.\n\r", ch);
                    return false;
                }

                SHOP_STOCK_DATA *prev = NULL;
                for(stock = pMob->pShop->stock;stock;prev = stock, stock = stock->next)
                {
                    if(!--idx)
                        break;
                }

                if( !stock )
                {
                    send_to_char("Invalid stock number.\n\r", ch);
                    return false;
                }

                if( prev != NULL )
                {
                    prev->next = stock->next;
                }
                else
                {
                    pMob->pShop->stock = stock->next;
                }

                free_shop_stock(stock);
                send_to_char("Stock item removed.\n\r", ch);
                return true;
            }

            send_to_char("Syntax:  shop stock [#] description [description]\n\r", ch);
            send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
            send_to_char("         shop stock [#] level [level]\n\r", ch);
            send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
            send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
            send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
            send_to_char("         shop stock [#] singular\n\r", ch);
            send_to_char("         shop stock [#] remove\n\r", ch);
            return false;
        }

        medit_shop(ch, "stock");
        return false;
    }

    medit_shop(ch, "");
    return false;
}


MEDIT(medit_sex)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(sex_flags, argument)) != NO_FLAG)
    {
        pMob->sex = value;

        send_to_char("Sex set.\n\r", ch);
        return true;
    }
    else
    if (!str_cmp(argument, "neutral")) // hack
    {
        pMob->sex = SEX_NEUTRAL;
        send_to_char("Sex set.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: sex [sex]\n\r"
          "Type '? sex' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_act)
{
    MOB_INDEX_DATA *pMob;
    //long value;

    if (argument[0] != '\0')
    {
            EDIT_MOB(ch, pMob);

        long bits[2];
        if (bitvector_lookup(argument, 2, bits, act_flags, act2_flags))
        {
            TOGGLE_BIT(pMob->act[0], bits[0]);
            TOGGLE_BIT(pMob->act[1], bits[1]);
            SET_BIT(pMob->act[0], ACT_IS_NPC);	// Force on, all the time

            send_to_char("Act flag toggled.\n\r", ch);
            return true;
    }
    }

    send_to_char("Syntax: act [flag]\n\r"
          "Type '? act' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_affect)
{
    MOB_INDEX_DATA *pMob;
    //int value;

    if (argument[0] != '\0')
    {
        EDIT_MOB(ch, pMob);
        long bits[2];

        if (bitvector_lookup(argument, 2, bits, affect_flags, affect2_flags))
        {
            TOGGLE_BIT(pMob->affected_by[0], bits[0]);
            TOGGLE_BIT(pMob->affected_by[1], bits[1]);

            send_to_char("Affect flag toggled.\n\r", ch);
            return true;
    }
    }

    send_to_char("Syntax: affect [flag]\n\r"
          "Type '? affect' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_ac)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];
    int pierce, bash, slash, exotic;

    do   /* So that I can use break and send the syntax in one place */
    {
    if (argument[0] == '\0')  break;

    EDIT_MOB(ch, pMob);
    argument = one_argument(argument, arg);

    if (!is_number(arg))  break;
    pierce = atoi(arg);
    argument = one_argument(argument, arg);

    if (arg[0] != '\0')
    {
        if (!is_number(arg))  break;
        bash = atoi(arg);
        argument = one_argument(argument, arg);
    }
    else
        bash = pMob->ac[AC_BASH];

    if (arg[0] != '\0')
    {
        if (!is_number(arg))  break;
        slash = atoi(arg);
        argument = one_argument(argument, arg);
    }
    else
        slash = pMob->ac[AC_SLASH];

    if (arg[0] != '\0')
    {
        if (!is_number(arg))  break;
        exotic = atoi(arg);
    }
    else
        exotic = pMob->ac[AC_EXOTIC];

    pMob->ac[AC_PIERCE] = pierce;
    pMob->ac[AC_BASH]   = bash;
    pMob->ac[AC_SLASH]  = slash;
    pMob->ac[AC_EXOTIC] = exotic;

    send_to_char("Ac set.\n\r", ch);
    return true;
    } while (false);    /* Just do it once.. */

    send_to_char("Syntax:  ac [ac-pierce [ac-bash [ac-slash [ac-exotic]]]]\n\r"
          "help MOB_AC  gives a list of reasonable ac-values.\n\r", ch);
    return false;
}


MEDIT(medit_form)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(form_flags, argument)) != NO_FLAG)
    {
        pMob->form ^= value;
        send_to_char("Form toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: form [flags]\n\r"
          "Type '? form' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_part)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(part_flags, argument)) != NO_FLAG)
    {
        pMob->parts ^= value;
        send_to_char("Parts toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: part [flags]\n\r"
          "Type '? part' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_immune)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(imm_flags, argument)) != NO_FLAG)
    {
        pMob->imm_flags ^= value;
        send_to_char("Immunity toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: imm [flags]\n\r"
          "Type '? imm' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_res)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(res_flags, argument)) != NO_FLAG)
    {
        pMob->res_flags ^= value;
        send_to_char("Resistance toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: res [flags]\n\r"
          "Type '? res' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_vuln)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(vuln_flags, argument)) != NO_FLAG)
    {
        pMob->vuln_flags ^= value;
        send_to_char("Vulnerability toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: vuln [flags]\n\r"
          "Type '? vuln' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_material)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char("Syntax:  material [string]\n\r", ch);
    return false;
    }

    free_string(pMob->material);
    pMob->material = str_dup(argument);

    send_to_char("Material set.\n\r", ch);
    return true;
}


MEDIT(medit_off)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(off_flags, argument)) != NO_FLAG)
    {
        pMob->off_flags ^= value;
        send_to_char("Offensive behaviour toggled.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: off [flags]\n\r"
          "Type '? off' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_size)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
    EDIT_MOB(ch, pMob);

    if ((value = flag_value(size_flags, argument)) != NO_FLAG)
    {
        pMob->size = value;
        send_to_char("Size set.\n\r", ch);
        return true;
    }
    }

    send_to_char("Syntax: size [size]\n\r"
          "Type '? size' for a list of sizes.\n\r", ch);
    return false;
}


MEDIT(medit_hitdice)
{
    static char syntax[] = "Syntax:  hitdice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 151)
    {
    send_to_char("You do not have permission to edit hit dice.\n\r", ch);
    return false;
    }

    if (argument[0] == '\0')
    {
    send_to_char(syntax, ch);
    return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
    send_to_char(syntax, ch);
    return false;
    }

    pMob->hit.number = atoi(num  );
    pMob->hit.size   = atoi(type );
    pMob->hit.bonus  = atoi(bonus);

    send_to_char("Hitdice set.\n\r", ch);
    return true;
}


MEDIT(medit_manadice)
{
    static char syntax[] = "Syntax:  manadice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char(syntax, ch);
    return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if (!(is_number(num) && is_number(type) && is_number(bonus)))
    {
    send_to_char(syntax, ch);
    return false;
    }

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
    send_to_char(syntax, ch);
    return false;
    }

    pMob->mana.number = atoi(num  );
    pMob->mana.size   = atoi(type );
    pMob->mana.bonus  = atoi(bonus);

    send_to_char("Manadice set.\n\r", ch);
    return true;
}


MEDIT(medit_damdice)
{
    static char syntax[] = "Syntax:  damdice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
    send_to_char(syntax, ch);
    return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if (!(is_number(num) && is_number(type) && is_number(bonus)))
    {
    send_to_char(syntax, ch);
    return false;
    }

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
    send_to_char(syntax, ch);
    return false;
    }

    pMob->damage.number = atoi(num  );
    pMob->damage.size   = atoi(type );
    pMob->damage.bonus  = atoi(bonus);

    send_to_char("Damdice set.\n\r", ch);
    return true;
}


MEDIT(medit_race)
{
    MOB_INDEX_DATA *pMob;
    RACE_DATA *race;

    if (argument[0] != '\0'
    && (race = race_lookup(argument)) != NULL)
    {
    EDIT_MOB(ch, pMob);

    pMob->race = race;
    pMob->act[0]	  |= race->act[0];
    pMob->act[1]	  |= race->act[1];
    pMob->affected_by[0] |= race->aff[0];
    pMob->off_flags   |= race->off;
    pMob->imm_flags   |= race->imm;
    pMob->res_flags   |= race->res;
    pMob->vuln_flags  |= race->vuln;
    pMob->form        |= race->form;
    pMob->parts       |= race->parts;

    send_to_char("Race set.\n\r", ch);
    return true;
    }

    if (argument[0] == '?')
    {
    char buf[MAX_STRING_LENGTH];
    int count = 0;

    send_to_char("Available races are:", ch);

    for (race = race_list; race != NULL; race = race->next)
    {
        if ((count % 3) == 0)
        send_to_char("\n\r", ch);
        sprintf(buf, " %-15s", race->name);
        send_to_char(buf, ch);
        count++;
    }

    send_to_char("\n\r", ch);
    return false;
    }

    send_to_char("Syntax:  race [race]\n\r"
          "Type 'race ?' for a list of races.\n\r", ch);
    return false;
}


MEDIT(medit_position)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];
    int value;

    argument = one_argument(argument, arg);

    switch (arg[0])
    {
    default:
    break;

    case 'S':
    case 's':
    if (str_prefix(arg, "start"))
        break;

    if ((value = flag_value(position_flags, argument)) == NO_FLAG)
        break;

    EDIT_MOB(ch, pMob);

    pMob->start_pos = value;
    send_to_char("Start position set.\n\r", ch);
    return true;

    case 'D':
    case 'd':
    if (str_prefix(arg, "default"))
        break;

    if ((value = flag_value(position_flags, argument)) == NO_FLAG)
        break;

    EDIT_MOB(ch, pMob);

    pMob->default_pos = value;
    send_to_char("Default position set.\n\r", ch);
    return true;
    }

    send_to_char("Syntax:  position [start/default] [position]\n\r"
          "Type '? position' for a list of positions.\n\r", ch);
    return false;
}


MEDIT(medit_movedice)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  move [number]\n\r", ch);
    return false;
    }

    pMob->move = atoi(argument);

    send_to_char("Movement set.\n\r", ch);
    return true;
}


MEDIT(medit_gold)
{
    MOB_INDEX_DATA *pMob;
    long value;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  wealth [number]\n\r", ch);
    return false;
    }

    value = atol(argument);

    if (value > 1000 && !has_imp_sig(pMob, NULL))
    {
    send_to_char("Sorry, that's too much. Have an IMP sign this mob if you want to set that much gold.\n\r", ch);
    return false;
    }

    pMob->wealth = value;
    use_imp_sig(pMob, NULL);

    send_to_char("Wealth set.\n\r", ch);
    return true;
}


MEDIT(medit_hitroll)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  hitroll [number]\n\r", ch);
    return false;
    }

    pMob->hitroll = atoi(argument);

    send_to_char("Hitroll set.\n\r", ch);
    return true;
}

MEDIT (medit_addmprog)
{
    int tindex, value, slot;
    MOB_INDEX_DATA *pMob;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_MOB(ch, pMob);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (num[0] == '\0' || trigger[0] =='\0' || phrase[0] =='\0')
    {
        send_to_char("Syntax:   addmprog [widevnum] [trigger] [phrase]\n\r",ch);
        return false;
    }

    if ((tindex = trigger_index(trigger, PRG_MPROG)) < 0) {
        send_to_char("Valid flags are:\n\r",ch);
        show_help(ch, "mprog");
        return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

    if(value == TRIG_SPELLCAST) {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "0");
        }
        else
        {
            int sn = skill_lookup(phrase);
            if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
                send_to_char("Invalid spell for trigger.\n\r",ch);
                return false;
            }
            sprintf(phrase,"%d",sn);
        }
    }
    else if( value == TRIG_EXIT || value == TRIG_EXALL )
    {
        if( !str_cmp(phrase, "*") )
        {
            strcpy(phrase, "-1");
        }
        else
        {
            int door = parse_door(phrase);
            if( door < 0 ) {
                send_to_char("Invalid direction for exit/exall trigger.\n\r", ch);
                return false;
            }
            sprintf(phrase,"%d",door);
        }
    }

    WNUM script_wnum;
    AREA_DATA *context = strchr(num, '#') ? pMob->area : NULL;
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    if ((code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_MPROG)) == NULL)
    {
        send_to_char("No such MOBProgram.\n\r",ch);
        return false;
    }

    // Make sure this has a list of progs!
    if(!pMob->progs) pMob->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = script_wnum.vnum;
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
    list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);

    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pMob->progs[slot], list);

    send_to_char("Mprog Added.\n\r",ch);
    return true;
}


MEDIT (medit_delmprog)
{
    MOB_INDEX_DATA *pMob;
    char mprog[MAX_STRING_LENGTH];
    int value;

    EDIT_MOB(ch, pMob);

    one_argument(argument, mprog);
    if (!is_number(mprog) || mprog[0] == '\0')
    {
       send_to_char("Syntax:  delmprog [#mprog]\n\r",ch);
       return false;
    }

    value = atol (mprog);

    if (value < 0)
    {
        send_to_char("Only non-negative mprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(pMob->progs,value)) {
    send_to_char("No such mprog.\n\r",ch);
    return false;
    }

    send_to_char("Mprog removed.\n\r", ch);
    return true;
}


MEDIT(medit_addquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_LIST *quest;
    QUEST_INDEX_DATA *pQuestIndex;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:  addquest [quest vnum]\n\r", ch);
        return false;
    }

    // Quest vnums are global, so use NULL context for global search
    long value = atol(argument);
    if (value <= 0)
    {
        send_to_char("Invalid quest vnum.\n\r", ch);
        return false;
    }

    pQuestIndex = get_quest_index(value);
    if (pQuestIndex == NULL)
    {
        send_to_char("That quest vnum doesn't exist.\n\r", ch);
        return false;
    }

    for (quest = pMob->quests; quest != NULL; quest = quest->next)
    {
        if (quest->vnum == value)
        {
            send_to_char("That would be redundant as you've already added that quest.\n\r", ch);
            return false;
        }
    }

    quest = new_quest_list();
    quest->vnum = pQuestIndex->vnum;

    quest->next = pMob->quests;
    pMob->quests = quest;

    send_to_char("Quest added.\n\r", ch);

    return true;
}


MEDIT(medit_delquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_LIST *quest_list;
    QUEST_LIST *prev_quest_list = NULL;
    int i;
    int counter;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
    send_to_char("Syntax:  delquest [#]\n\r", ch);
    return false;
    }

    i = atoi (argument);
    counter = 0;
    for (quest_list = pMob->quests; quest_list != NULL;
      quest_list = quest_list->next)
    {
    if (i == counter)
        break;

    counter++;
    prev_quest_list = quest_list;
    }

    if (quest_list == NULL)
    {
    send_to_char("Number not found.\n\r", ch);
    return false;
    }

    if (prev_quest_list != NULL)
    prev_quest_list->next = quest_list->next;
    else
    pMob->quests = quest_list->next;

    free_quest_list(quest_list);
    send_to_char("Quest removed.\n\r", ch);
    return true;
}

MEDIT(medit_questor)
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];

    EDIT_MOB(ch, pMob);

    if(IS_NULLSTR(argument))
    {
        send_to_char("QUESTOR ADD                 Adds questor data to mob.\n\r", ch);
        send_to_char("        REMOVE              Removes questor data from mob.\n\r", ch);
        send_to_char("        SCROLL [vnum]       Sets the scroll object to the specified vnum.\n\r", ch);
        send_to_char("        KEYWORDS [string]   Sets keywords of scroll.\n\r", ch);
        send_to_char("        SHORT [string]      Sets short description of scroll.\n\r", ch);
        send_to_char("        LONG [string]       Sets long description of scroll.\n\r", ch);
        send_to_char("        HEADER              Edits scroll header.\n\r", ch);
        send_to_char("        FOOTER              Edits scroll footer.\n\r", ch);
        send_to_char("        PREFIX [string]     Edits line prefix.\n\r", ch);
        send_to_char("        SUFFIX [string]     Edits line suffix.\n\r", ch);
        send_to_char("        WIDTH [width]       Sets line width.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if (!str_prefix(arg,"add"))
    {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
        {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        if( pMob->pQuestor != NULL )
        {
            send_to_char("There is already questor data.\n\r", ch);
            return false;
        }

        pMob->pQuestor = new_questor_data();
        use_imp_sig(pMob, NULL);
        send_to_char("Questor data added.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"remove")) {
        if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
        {
            send_to_char("You can't do this without an IMP's permission.\n\r", ch);
            return false;
        }

        if( pMob->pQuestor == NULL )
        {
            send_to_char("There is any questor data.\n\r", ch);
            return false;
        }

        free_questor_data(pMob->pQuestor);
        pMob->pQuestor = NULL;
        send_to_char("Questor data removed.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"scroll")) {
        if(argument[0] == '\0')
        {
            send_to_char("Syntax: questor scroll [widevnum]\n\r", ch);
            return false;
        }

        WNUM obj_wnum;
        AREA_DATA *context = strchr(argument, '#') ? pMob->area : NULL;
        if (!parse_widevnum(argument, context, &obj_wnum)) {
            send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
            return false;
        }

        if( !get_obj_index(obj_wnum.pArea, obj_wnum.vnum) )
        {
            send_to_char("Object does not exist.\n\r", ch);
            return false;
        }

        pMob->pQuestor->scroll = obj_wnum.vnum;
        send_to_char("Questor scroll object changed.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"keywords")) {
        free_string(pMob->pQuestor->keywords);
        pMob->pQuestor->keywords = str_dup(argument);

        send_to_char("Keywords set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"short")) {
        free_string(pMob->pQuestor->short_descr);
        pMob->pQuestor->short_descr = str_dup(argument);

        send_to_char("Short description set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"long")) {
        free_string(pMob->pQuestor->long_descr);
        pMob->pQuestor->long_descr = str_dup(argument);

        send_to_char("Long description set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"header")) {
        send_to_char("Editting the Questor Header:\n\r", ch);
        send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
        send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
        send_to_char("\n\r", ch);

        string_append(ch, &pMob->pQuestor->header);
        return true;

    } else if (!str_prefix(arg,"footer")) {
        send_to_char("Editting the Questor Footer:\n\r", ch);
        send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
        send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
        send_to_char("\n\r", ch);

        string_append(ch, &pMob->pQuestor->footer);
        return true;

    } else if (!str_prefix(arg,"prefix")) {
        free_string(pMob->pQuestor->prefix);
        pMob->pQuestor->prefix = str_dup(argument);

        send_to_char("Prefix set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"suffix")) {
        free_string(pMob->pQuestor->suffix);
        pMob->pQuestor->suffix = str_dup(argument);

        send_to_char("Prefix set.\n\r", ch);
        return true;

    } else if (!str_prefix(arg,"width")) {
        if(!is_number(argument))
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int width = atoi(argument);
        if( width <= 0 )
        {
            pMob->pQuestor->line_width = 0;
            send_to_char("Line width disabled.\n\r", ch);
            return true;

        }
        else if(width > 160)
        {
            send_to_char("Width is out of range.  Please specify a number from 1 to 160, or 0 to disable width.\n\r", ch);
            return false;
        }

        pMob->pQuestor->line_width = width;
        send_to_char("Line width set.\n\r", ch);
        return true;

    } else {
        medit_questor(ch, "");
        return false;
    }

    return true;

}

MEDIT( medit_crew )
{
    MOB_INDEX_DATA *pMob;
    char arg[MIL];

    EDIT_MOB(ch, pMob);

    if(IS_NULLSTR(argument))
    {
        send_to_char("Syntax:  crew assign\n\r", ch);
        send_to_char("         crew remove\n\r", ch);
        send_to_char("         crew minrank <rank>\n\r", ch);
        send_to_char("         crew scouting <rating>\n\r", ch);
        send_to_char("         crew gunning <rating>\n\r", ch);
        send_to_char("         crew oarring <rating>\n\r", ch);
        send_to_char("         crew mechanics <rating>\n\r", ch);
        send_to_char("         crew navigation <rating>\n\r", ch);
        send_to_char("         crew leadership <rating>\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg);

    if( !str_prefix(arg, "assign") )
    {
        if( IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile already has ship crew data.\n\r", ch);
            return false;
        }

        pMob->pCrew = new_ship_crew_index();
        send_to_char("Ship Crew assigned.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "remove") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile has no ship crew data.\n\r", ch);
            return false;
        }

        free_ship_crew_index(pMob->pCrew);
        pMob->pCrew = NULL;
        send_to_char("Ship Crew removed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "minrank") )
    {
        send_to_char("Not implemented yet.\n\r", ch);
        return false;
    }

    if( !str_prefix(arg, "scouting") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->scouting = value;
        send_to_char("Scouting Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "gunning") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->gunning = value;
        send_to_char("Gunning Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "oarring") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->oarring = value;
        send_to_char("Oarring Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "mechanics") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->mechanics = value;
        send_to_char("Mechanics Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "navigation") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->navigation = value;
        send_to_char("Navigation Rating changed.\n\r", ch);
        return true;
    }

    if( !str_prefix(arg, "leadership") )
    {
        if( !IS_VALID(pMob->pCrew) )
        {
            send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
            return false;
        }

        if( !is_number(argument) )
        {
            send_to_char("That is not a number.\n\r", ch);
            return false;
        }

        int value = atoi(argument);
        if( value < 0 || value > 100 )
        {
            send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
            return false;
        }

        pMob->pCrew->leadership = value;
        send_to_char("Leadership Rating changed.\n\r", ch);
        return true;
    }


    medit_crew(ch, "");
    return false;
}
