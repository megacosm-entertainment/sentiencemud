/***************************************************************************
 *  Sentience MUD                                                          *
 *  OLC Object Editor — Type-specific subcommands.                         *
 *                                                                         *
 *  Each item type that carries type data gets a dedicated oedit command    *
 *  (e.g. "weapon", "portal", "container") so builders can interact with   *
 *  multityped objects by name instead of the legacy v0-v7 indices.        *
 ***************************************************************************/

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../item_types.h"
#include "../../recycle.h"
#include <string.h>

/* Forward declarations for helpers used by type commands */
extern void set_weapon_dice(OBJ_INDEX_DATA *objIndex);
extern void set_armour(OBJ_INDEX_DATA *objIndex);
extern int  get_armour_strength(char *argument);

/* ============================================================================
 *  ARMOR
 * ============================================================================ */
OEDIT(oedit_armor)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_ARMOR(pObj)) {
        send_to_char("Object lacks the armour type. Use '{Waddtype armour{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "pierce")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor pierce <value>\n\r", ch); return false; }
            ARMOR(pObj)->protection[0] = atoi(argument);
            send_to_char("AC pierce set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "bash")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor bash <value>\n\r", ch); return false; }
            ARMOR(pObj)->protection[1] = atoi(argument);
            send_to_char("AC bash set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "slash")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor slash <value>\n\r", ch); return false; }
            ARMOR(pObj)->protection[2] = atoi(argument);
            send_to_char("AC slash set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "exotic")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor exotic <value>\n\r", ch); return false; }
            ARMOR(pObj)->protection[3] = atoi(argument);
            send_to_char("AC exotic set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "strength")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor strength <none|light|medium|strong|heavy>\n\r", ch); return false; }
            ARMOR(pObj)->armor_strength = get_armour_strength(argument);
            set_armour(pObj);
            send_to_char("Armour strength set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: armor type <none|cloth|leather|mail|plate>\n\r", ch); return false; }
            int val = flag_value(armor_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid armor type.\n\r", ch); return false; }
            ARMOR(pObj)->armor_type = val;
            send_to_char("Armour type set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: pierce, bash, slash, exotic, strength, type\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WArmour:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gstrength      {x %s\n\r"
        "  {Gpierce        {x [%d]\n\r"
        "  {Gbash          {x [%d]\n\r"
        "  {Gslash         {x [%d]\n\r"
        "  {Gexotic        {x [%d]\n\r",
        flag_string(armor_types, ARMOR(pObj)->armor_type),
        armour_strength_table[ARMOR(pObj)->armor_strength].name,
        ARMOR(pObj)->protection[0],
        ARMOR(pObj)->protection[1],
        ARMOR(pObj)->protection[2],
        ARMOR(pObj)->protection[3]);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  BODY PART
 * ============================================================================ */
OEDIT(oedit_bodypart)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_BODY_PART(pObj)) {
        send_to_char("Object lacks the body_part type. Use '{Waddtype body_part{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "parts")) {
            if (argument[0] == '\0') { send_to_char("Syntax: bodypart parts <flags>\n\r", ch); return false; }
            long val = flag_value(part_flags, argument);
            if (val != NO_FLAG) BODY_PART(pObj)->parts ^= val;
            send_to_char("Body parts toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "race")) {
            if (argument[0] == '\0') { send_to_char("Syntax: bodypart race <uid>\n\r", ch); return false; }
            BODY_PART(pObj)->race_uid = atoi(argument);
            send_to_char("Race UID set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: parts, race\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WBody Part:{x\n\r"
        "  {Gparts         {x %s\n\r"
        "  {Grace           {x [%d]\n\r",
        flag_string(part_flags, BODY_PART(pObj)->parts),
        BODY_PART(pObj)->race_uid);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  BOOK
 * ============================================================================ */
OEDIT(oedit_book)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_BOOK(pObj)) {
        send_to_char("Object lacks the book type. Use '{Waddtype book{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: book flags <flag>\n\r", ch); return false; }
            int val = flag_value(container_flags, argument);
            if (val != NO_FLAG) TOGGLE_BIT(BOOK(pObj)->flags, val);
            else { send_to_char("Invalid flag.\n\r", ch); return false; }
            send_to_char("Book flags toggled.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: flags\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WBook:{x\n\r"
        "  {Gflags         {x [%s]\n\r",
        flag_string(container_flags, BOOK(pObj)->flags));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CART
 * ============================================================================ */
OEDIT(oedit_cart)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CART(pObj)) {
        send_to_char("Object lacks the cart type. Use '{Waddtype cart{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart capacity <weight>\n\r", ch); return false; }
            CART(pObj)->capacity = atoi(argument);
            send_to_char("Cart capacity set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "delay")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart delay <ticks>\n\r", ch); return false; }
            CART(pObj)->move_delay = atoi(argument);
            send_to_char("Cart move delay set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "strength")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart strength <min_str>\n\r", ch); return false; }
            CART(pObj)->min_strength = atoi(argument);
            send_to_char("Cart min strength set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart items <max>\n\r", ch); return false; }
            CART(pObj)->max_items = atoi(argument);
            send_to_char("Cart max items set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart weightmult <multiplier>\n\r", ch); return false; }
            CART(pObj)->weight_multiplier = atoi(argument);
            send_to_char("Cart weight multiplier set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart flags <flag>\n\r", ch); return false; }
            long val = flag_value(cart_flags, argument);
            if (val != NO_FLAG) CART(pObj)->flags ^= val;
            send_to_char("Cart flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "vanish")) {
            if (argument[0] == '\0') { send_to_char("Syntax: cart vanish <time>\n\r", ch); return false; }
            CART(pObj)->vanish_time = atoi(argument);
            send_to_char("Cart vanish time set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: capacity, delay, strength, items, weightmult, flags, vanish\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCart:{x\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gdelay         {x [%d]\n\r"
        "  {Gstrength      {x [%d]\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d]\n\r"
        "  {Gflags         {x [%s]\n\r"
        "  {Gvanish        {x [%d]\n\r",
        CART(pObj)->capacity,
        CART(pObj)->move_delay,
        CART(pObj)->min_strength,
        CART(pObj)->max_items,
        CART(pObj)->weight_multiplier,
        flag_string(cart_flags, CART(pObj)->flags),
        CART(pObj)->vanish_time);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  COMPASS
 * ============================================================================ */
OEDIT(oedit_compass)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_COMPASS(pObj)) {
        send_to_char("Object lacks the compass type. Use '{Waddtype compass{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "accuracy")) {
            if (argument[0] == '\0') { send_to_char("Syntax: compass accuracy <percent>\n\r", ch); return false; }
            COMPASS(pObj)->accuracy = atoi(argument);
            send_to_char("Compass accuracy set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: accuracy\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCompass:{x\n\r"
        "  {Gaccuracy      {x [%d%%]\n\r",
        COMPASS(pObj)->accuracy);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CONTAINER
 * ============================================================================ */
OEDIT(oedit_container)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CONTAINER(pObj)) {
        send_to_char("Object lacks the container type. Use '{Waddtype container{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container weight <max_kg>\n\r", ch); return false; }
            CONTAINER(pObj)->max_weight = atoi(argument);
            send_to_char("Container max weight set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container flags <flag>\n\r", ch); return false; }
            int val = flag_value(container_flags, argument);
            if (val != NO_FLAG) TOGGLE_BIT(CONTAINER(pObj)->flags, val);
            else { send_to_char("Invalid container flag.\n\r", ch); return false; }
            send_to_char("Container flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container items <max>\n\r", ch); return false; }
            if (atoi(argument) > 225 && ch->tot_level < MAX_LEVEL) {
                send_to_char("Sorry, that value is out of range.\n\r", ch);
                return false;
            }
            CONTAINER(pObj)->max_items = atoi(argument);
            send_to_char("Container max items set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: container weightmult <1-1000>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0 || val > 1000) {
                send_to_char("Weight multiplier must be between 1 and 1000.\n\r", ch);
                return false;
            }
            if (val < 1000 && !has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL) {
                send_to_char("An imp sig is required to set the weight multiplier below 100%.\n\r", ch);
                return false;
            }
            if (has_imp_sig(NULL, pObj))
                use_imp_sig(NULL, pObj);
            CONTAINER(pObj)->weight_multiplier = val;
            send_to_char("Container weight multiplier set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: weight, flags, items, weightmult\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WContainer:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gflags         {x [%s]\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d%%]\n\r",
        CONTAINER(pObj)->max_weight,
        flag_string(container_flags, CONTAINER(pObj)->flags),
        CONTAINER(pObj)->max_items,
        CONTAINER(pObj)->weight_multiplier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  CORPSE
 * ============================================================================ */
OEDIT(oedit_corpse)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_CORPSE(pObj)) {
        send_to_char("Object lacks the corpse type. Use '{Waddtype npccorpse{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse type <corpse_type>\n\r", ch); return false; }
            int val = flag_value(corpse_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid corpse type.\n\r", ch); return false; }
            CORPSE(pObj)->corpse_type = val;
            send_to_char("Corpse type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "resurrection")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse resurrection <percent>\n\r", ch); return false; }
            CORPSE(pObj)->resurrection = atoi(argument);
            send_to_char("Resurrection chance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "animation")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse animation <percent>\n\r", ch); return false; }
            CORPSE(pObj)->animation = atoi(argument);
            send_to_char("Animation chance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "parts")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse parts <body_part_flags>\n\r", ch); return false; }
            long val = flag_value(part_flags, argument);
            if (val == NO_FLAG) { send_to_char("Invalid body part flags.\n\r", ch); return false; }
            CORPSE(pObj)->body_parts = val;
            send_to_char("Body parts set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "mobile")) {
            if (argument[0] == '\0') { send_to_char("Syntax: corpse mobile <vnum|0>\n\r", ch); return false; }
            if (atol(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                MOB_INDEX_DATA *key_mob;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_mob = key_wnum.pArea ? get_mob_index(key_wnum.pArea, key_wnum.vnum) : get_mob_index_global(key_wnum.vnum);
                if (!key_mob) {
                    send_to_char("No such mobile exists.\n\r", ch);
                    return false;
                }
                CORPSE(pObj)->mobile_vnum = key_wnum.vnum;
                CORPSE(pObj)->mobile_area_uid = key_mob->area ? key_mob->area->uid : 0;
            } else {
                CORPSE(pObj)->mobile_vnum = 0;
                CORPSE(pObj)->mobile_area_uid = 0;
            }
            send_to_char("Mobile set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: type, resurrection, animation, parts, mobile\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WCorpse:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gresurrection  {x %d%%\n\r"
        "  {Ganimation     {x %d%%\n\r"
        "  {Gparts         {x %s\n\r"
        "  {Gmobile        {x %s\n\r",
        flag_string(corpse_types, CORPSE(pObj)->corpse_type),
        CORPSE(pObj)->resurrection,
        CORPSE(pObj)->animation,
        flag_string(part_flags, CORPSE(pObj)->body_parts),
        widevnum_string(CORPSE(pObj)->mobile_area_uid > 0
            ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
            CORPSE(pObj)->mobile_vnum, pObj->area));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  DRINK / FLUID CONTAINER
 * ============================================================================ */
OEDIT(oedit_drink)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FLUID_CON(pObj)) {
        send_to_char("Object lacks the fluid container type. Use '{Waddtype drinkcontainer{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink capacity <amount>\n\r", ch); return false; }
            FLUID_CON(pObj)->capacity = atoi(argument);
            send_to_char("Liquid capacity set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "amount")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink amount <current>\n\r", ch); return false; }
            FLUID_CON(pObj)->amount = atoi(argument);
            send_to_char("Liquid amount set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "liquid")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink liquid <liquid_name>\n\r", ch); return false; }
            int liq = liq_lookup(argument);
            FLUID_CON(pObj)->liquid = (liq != -1) ? liq : 0;
            send_to_char("Liquid type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "poison")) {
            FLUID_CON(pObj)->poison = (FLUID_CON(pObj)->poison == 0) ? 1 : 0;
            send_to_char("Poison toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "refill")) {
            if (argument[0] == '\0') { send_to_char("Syntax: drink refill <rate>\n\r", ch); return false; }
            FLUID_CON(pObj)->refill_rate = atoi(argument);
            send_to_char("Refill rate set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: capacity, amount, liquid, poison, refill\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFluid Container:{x\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gamount        {x [%d]\n\r"
        "  {Gliquid        {x %s\n\r"
        "  {Gpoison        {x %s\n\r"
        "  {Grefill        {x [%d]\n\r",
        FLUID_CON(pObj)->capacity,
        FLUID_CON(pObj)->amount,
        liquid_name(FLUID_CON(pObj)->liquid),
        FLUID_CON(pObj)->poison != 0 ? "Yes" : "No",
        FLUID_CON(pObj)->refill_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  FOOD
 * ============================================================================ */
OEDIT(oedit_food)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FOOD(pObj)) {
        send_to_char("Object lacks the food type. Use '{Waddtype food{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "hunger")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food hunger <hours>\n\r", ch); return false; }
            FOOD(pObj)->hunger = atoi(argument);
            send_to_char("Food hunger hours set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "full")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food full <hours>\n\r", ch); return false; }
            FOOD(pObj)->full = atoi(argument);
            send_to_char("Food full hours set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "poison")) {
            FOOD(pObj)->poison = (FOOD(pObj)->poison == 0) ? 1 : 0;
            send_to_char("Poison toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "timer")) {
            if (argument[0] == '\0') { send_to_char("Syntax: food timer <ticks>\n\r", ch); return false; }
            FOOD(pObj)->timer = atoi(argument);
            send_to_char("Food timer set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: hunger, full, poison, timer\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFood:{x\n\r"
        "  {Ghunger        {x [%d]\n\r"
        "  {Gfull          {x [%d]\n\r"
        "  {Gpoison        {x %s\n\r"
        "  {Gtimer         {x [%d]\n\r",
        FOOD(pObj)->hunger,
        FOOD(pObj)->full,
        FOOD(pObj)->poison != 0 ? "Yes" : "No",
        FOOD(pObj)->timer);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  FURNITURE
 * ============================================================================ */
OEDIT(oedit_furniture)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_FURNITURE(pObj)) {
        send_to_char("Object lacks the furniture type. Use '{Waddtype furniture{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "people")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture people <max>\n\r", ch); return false; }
            FURNITURE(pObj)->max_people = atoi(argument);
            send_to_char("Max people set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture weight <max_kg>\n\r", ch); return false; }
            FURNITURE(pObj)->max_weight = atoi(argument);
            send_to_char("Max weight set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture flags <flag>\n\r", ch); return false; }
            long val = flag_value(furniture_flags, argument);
            if (val != NO_FLAG) FURNITURE(pObj)->flags ^= val;
            send_to_char("Furniture flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "heal")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture heal <bonus>\n\r", ch); return false; }
            FURNITURE(pObj)->heal_rate = atoi(argument);
            send_to_char("Heal bonus set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture mana <bonus>\n\r", ch); return false; }
            FURNITURE(pObj)->mana_rate = atoi(argument);
            send_to_char("Mana bonus set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "move")) {
            if (argument[0] == '\0') { send_to_char("Syntax: furniture move <bonus>\n\r", ch); return false; }
            FURNITURE(pObj)->move_rate = atoi(argument);
            send_to_char("Move bonus set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: people, weight, flags, heal, mana, move\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WFurniture:{x\n\r"
        "  {Gpeople        {x [%d]\n\r"
        "  {Gweight        {x [%d]\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Gheal          {x [%d]\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gmove          {x [%d]\n\r",
        FURNITURE(pObj)->max_people,
        FURNITURE(pObj)->max_weight,
        flag_string(furniture_flags, FURNITURE(pObj)->flags),
        FURNITURE(pObj)->heal_rate,
        FURNITURE(pObj)->mana_rate,
        FURNITURE(pObj)->move_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  HERB
 * ============================================================================ */
OEDIT(oedit_herb)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_HERB(pObj)) {
        send_to_char("Object lacks the herb type. Use '{Waddtype herb{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb type <herb_name>\n\r", ch); return false; }
            int i;
            for (i = 0; i < MAX_HERB; i++) {
                if (!str_prefix(argument, herb_table[i].name))
                    break;
            }
            if (i < MAX_HERB) {
                HERB(pObj)->type = i;
                send_to_char("Herb type set.\n\r", ch);
                return true;
            }
            send_to_char("Invalid herb type.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "healing")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb healing <percent>\n\r", ch); return false; }
            HERB(pObj)->healing = atoi(argument);
            send_to_char("Healing rate set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "regen")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb regen <percent>\n\r", ch); return false; }
            HERB(pObj)->regenerative = atoi(argument);
            send_to_char("Regenerative rate set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "refresh")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb refresh <percent>\n\r", ch); return false; }
            HERB(pObj)->refreshing = atoi(argument);
            send_to_char("Refreshing rate set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "immunity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb immunity <flags>\n\r", ch); return false; }
            long val = flag_value(imm_flags, argument);
            if (val != NO_FLAG) { HERB(pObj)->immunity ^= val; send_to_char("Immunity toggled.\n\r", ch); return true; }
            send_to_char("Invalid immunity flag.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "resistance")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb resistance <flags>\n\r", ch); return false; }
            long val = flag_value(res_flags, argument);
            if (val != NO_FLAG) { HERB(pObj)->resistance ^= val; send_to_char("Resistance toggled.\n\r", ch); return true; }
            send_to_char("Invalid resistance flag.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "vulnerability")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb vulnerability <flags>\n\r", ch); return false; }
            long val = flag_value(vuln_flags, argument);
            if (val != NO_FLAG) { HERB(pObj)->vulnerability ^= val; send_to_char("Vulnerability toggled.\n\r", ch); return true; }
            send_to_char("Invalid vulnerability flag.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "spell")) {
            if (argument[0] == '\0') { send_to_char("Syntax: herb spell <spell_name>\n\r", ch); return false; }
            int sn = skill_lookup(argument);
            if (sn > 0 && skill_table[sn].spell_fun != spell_null) {
                HERB(pObj)->spell = sn;
                send_to_char("Spell set.\n\r", ch);
                return true;
            } else if (sn == 0 || !str_cmp(argument, "none")) {
                HERB(pObj)->spell = 0;
                send_to_char("Spell cleared.\n\r", ch);
                return true;
            }
            send_to_char("Invalid spell.\n\r", ch);
            return false;
        }
        send_to_char("Valid fields: type, healing, regen, refresh, immunity, resistance, vulnerability, spell\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WHerb:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Ghealing       {x [%d%%]\n\r"
        "  {Gregen         {x [%d%%]\n\r"
        "  {Grefresh       {x [%d%%]\n\r"
        "  {Gimmunity      {x %s\n\r"
        "  {Gresistance    {x %s\n\r"
        "  {Gvulnerability {x %s\n\r"
        "  {Gspell         {x %s\n\r",
        herb_table[HERB(pObj)->type].name,
        HERB(pObj)->healing,
        HERB(pObj)->regenerative,
        HERB(pObj)->refreshing,
        flag_string(imm_flags, HERB(pObj)->immunity),
        flag_string(res_flags, HERB(pObj)->resistance),
        flag_string(vuln_flags, HERB(pObj)->vulnerability),
        HERB(pObj)->spell > 0 ? skill_table[HERB(pObj)->spell].name : "none");
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  INK
 * ============================================================================ */
OEDIT(oedit_ink)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_INK(pObj)) {
        send_to_char("Object lacks the ink type. Use '{Waddtype ink{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type1")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type1 <catalyst_type>\n\r", ch); return false; }
            INK(pObj)->types[0] = flag_lookup(argument, catalyst_types);
            send_to_char("Ink type 1 set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "type2")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type2 <catalyst_type>\n\r", ch); return false; }
            INK(pObj)->types[1] = flag_lookup(argument, catalyst_types);
            send_to_char("Ink type 2 set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "type3")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ink type3 <catalyst_type>\n\r", ch); return false; }
            INK(pObj)->types[2] = flag_lookup(argument, catalyst_types);
            send_to_char("Ink type 3 set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: type1, type2, type3\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WInk:{x\n\r"
        "  {Gtype1         {x %s\n\r"
        "  {Gtype2         {x %s\n\r"
        "  {Gtype3         {x %s\n\r",
        flag_string(catalyst_types, INK(pObj)->types[0]),
        flag_string(catalyst_types, INK(pObj)->types[1]),
        flag_string(catalyst_types, INK(pObj)->types[2]));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  INSTRUMENT
 * ============================================================================ */
OEDIT(oedit_instrument)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_INSTRUMENT(pObj)) {
        send_to_char("Object lacks the instrument type. Use '{Waddtype instrument{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument type <instrument_type>\n\r", ch); return false; }
            int val = flag_value(instrument_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid instrument type.\n\r", ch); return false; }
            INSTRUMENT(pObj)->type = val;
            send_to_char("Instrument type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument flags <flag>\n\r", ch); return false; }
            int val = flag_value(instrument_flags, argument);
            if (val == NO_FLAG) { send_to_char("Invalid flag.\n\r", ch); return false; }
            INSTRUMENT(pObj)->flags ^= val;
            send_to_char("Instrument flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "beatsmin")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument beatsmin <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 1 || val > 5000) {
                send_to_char("Min scale factor must be between 1%% and 5000%%.\n\r", ch);
                return false;
            }
            INSTRUMENT(pObj)->beats_min = val;
            send_to_char("Min playtime scale factor set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "beatsmax")) {
            if (argument[0] == '\0') { send_to_char("Syntax: instrument beatsmax <percent>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 1 || val > 5000) {
                send_to_char("Max scale factor must be between 1%% and 5000%%.\n\r", ch);
                return false;
            }
            INSTRUMENT(pObj)->beats_max = val;
            send_to_char("Max playtime scale factor set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: type, flags, beatsmin, beatsmax\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WInstrument:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Gbeatsmin      {x [%d%%]\n\r"
        "  {Gbeatsmax      {x [%d%%]\n\r",
        flag_string(instrument_types, INSTRUMENT(pObj)->type),
        flag_string(instrument_flags, INSTRUMENT(pObj)->flags),
        INSTRUMENT(pObj)->beats_min,
        INSTRUMENT(pObj)->beats_max);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  JEWELRY
 * ============================================================================ */
OEDIT(oedit_jewelry)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_JEWELRY(pObj)) {
        send_to_char("Object lacks the jewelry type. Use '{Waddtype jewelry{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: jewelry mana <max_mana>\n\r", ch); return false; }
            JEWELRY(pObj)->max_mana = atoi(argument);
            send_to_char("Max mana set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: mana\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WJewelry:{x\n\r"
        "  {Gmana          {x [%d]\n\r",
        JEWELRY(pObj)->max_mana);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  LIGHT
 * ============================================================================ */
OEDIT(oedit_light)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_LIGHT(pObj)) {
        send_to_char("Object lacks the light type. Use '{Waddtype light{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "duration")) {
            if (argument[0] == '\0') { send_to_char("Syntax: light duration <hours|-1 for infinite>\n\r", ch); return false; }
            LIGHT(pObj)->duration = atoi(argument);
            send_to_char("Light duration set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: light flags <flag>\n\r", ch); return false; }
            long val = flag_value(light_flags, argument);
            if (val != NO_FLAG) LIGHT(pObj)->flags ^= val;
            send_to_char("Light flags toggled.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: duration, flags\n\r", ch);
        return false;
    }

    if (LIGHT(pObj)->duration == -1) {
        sprintf(buf,
            "{WLight:{x\n\r"
            "  {Gduration      {x Infinite [-1]\n\r"
            "  {Gflags         {x [%s]\n\r",
            flag_string(light_flags, LIGHT(pObj)->flags));
    } else {
        sprintf(buf,
            "{WLight:{x\n\r"
            "  {Gduration      {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r",
            LIGHT(pObj)->duration,
            flag_string(light_flags, LIGHT(pObj)->flags));
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MAP
 * ============================================================================ */
OEDIT(oedit_map)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MAP(pObj)) {
        send_to_char("Object lacks the map type. Use '{Waddtype map{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "wuid")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map wuid <wilderness_uid>\n\r", ch); return false; }
            MAP(pObj)->wuid = atol(argument);
            send_to_char("Map wilderness UID set.\n\r", ch);
            return true;
        }
        if (!str_cmp(field, "x")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map x <coordinate>\n\r", ch); return false; }
            MAP(pObj)->x = atol(argument);
            send_to_char("Map X coordinate set.\n\r", ch);
            return true;
        }
        if (!str_cmp(field, "y")) {
            if (argument[0] == '\0') { send_to_char("Syntax: map y <coordinate>\n\r", ch); return false; }
            MAP(pObj)->y = atol(argument);
            send_to_char("Map Y coordinate set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: wuid, x, y\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMap:{x\n\r"
        "  {Gwuid          {x [%ld]\n\r"
        "  {Gx             {x [%ld]\n\r"
        "  {Gy             {x [%ld]\n\r",
        MAP(pObj)->wuid,
        MAP(pObj)->x,
        MAP(pObj)->y);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MIST
 * ============================================================================ */
OEDIT(oedit_mist)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MIST(pObj)) {
        send_to_char("Object lacks the mist type. Use '{Waddtype mist{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "objects")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist objects <percent>\n\r", ch); return false; }
            MIST(pObj)->obscure_objs = atoi(argument);
            send_to_char("Object obscurity set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "characters")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist characters <percent>\n\r", ch); return false; }
            MIST(pObj)->obscure_mobs = atoi(argument);
            send_to_char("Character obscurity set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "room")) {
            if (argument[0] == '\0') { send_to_char("Syntax: mist room <percent>\n\r", ch); return false; }
            MIST(pObj)->obscure_room = atoi(argument);
            send_to_char("Room obscurity set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: objects, characters, room\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMist:{x\n\r"
        "  {Gobjects       {x [%d%%]\n\r"
        "  {Gcharacters    {x [%d%%]\n\r"
        "  {Groom          {x [%d%%]\n\r",
        MIST(pObj)->obscure_objs,
        MIST(pObj)->obscure_mobs,
        MIST(pObj)->obscure_room);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  MONEY
 * ============================================================================ */
OEDIT(oedit_money)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_MONEY(pObj)) {
        send_to_char("Object lacks the money type. Use '{Waddtype money{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "silver")) {
            if (argument[0] == '\0') { send_to_char("Syntax: money silver <amount>\n\r", ch); return false; }
            MONEY(pObj)->silver = atoi(argument);
            send_to_char("Silver amount set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "gold")) {
            if (argument[0] == '\0') { send_to_char("Syntax: money gold <amount>\n\r", ch); return false; }
            MONEY(pObj)->gold = atoi(argument);
            send_to_char("Gold amount set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: silver, gold\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WMoney:{x\n\r"
        "  {Gsilver        {x [%d]\n\r"
        "  {Ggold          {x [%d]\n\r",
        MONEY(pObj)->silver,
        MONEY(pObj)->gold);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  PAGE
 * ============================================================================ */
OEDIT(oedit_page)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_PAGE(pObj)) {
        send_to_char("Object lacks the page type. Use '{Waddtype part{x' (page).\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "number")) {
            if (argument[0] == '\0') { send_to_char("Syntax: page number <page_no>\n\r", ch); return false; }
            PAGE(pObj)->page_no = atoi(argument);
            send_to_char("Page number set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "title")) {
            if (argument[0] == '\0') { send_to_char("Syntax: page title <text>\n\r", ch); return false; }
            free_string(PAGE(pObj)->title);
            PAGE(pObj)->title = str_dup(argument);
            send_to_char("Page title set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: number, title\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WPage:{x\n\r"
        "  {Gnumber        {x [%d]\n\r"
        "  {Gtitle         {x %s\n\r",
        PAGE(pObj)->page_no,
        PAGE(pObj)->title ? PAGE(pObj)->title : "(none)");
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  PORTAL
 * ============================================================================ */
OEDIT(oedit_portal)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_PORTAL(pObj)) {
        send_to_char("Object lacks the portal type. Use '{Waddtype portal{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal charges <num>\n\r", ch); return false; }
            PORTAL(pObj)->charges = atoi(argument);
            send_to_char("Portal charges set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "exit")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal exit <exit_flags>\n\r", ch); return false; }
            long val = flag_value(portal_exit_flags, argument);
            if (val != NO_FLAG) PORTAL(pObj)->exit ^= val;
            send_to_char("Exit flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal flags <portal_flags>\n\r", ch); return false; }
            int flags = flag_value(portal_flags, argument);
            if (flags != NO_FLAG) {
                PORTAL(pObj)->flags ^= flags;
                if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON))
                    REMOVE_BIT(PORTAL(pObj)->flags, GATE_AREARANDOM);
                if (IS_SET(flags, GATE_DUNGEON) && IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
                    PORTAL(pObj)->params[0] = 0;
                    PORTAL(pObj)->params[1] = 0;
                    PORTAL(pObj)->params[2] = 0;
                    PORTAL(pObj)->params[3] = 0;
                    PORTAL(pObj)->params[4] = 0;
                }
            }
            send_to_char("Portal flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "destination")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal destination <widevnum|-1>\n\r", ch); return false; }
            if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
                if (!get_dungeon_index(atoi(argument))) {
                    send_to_char("There is no such dungeon.\n\r", ch);
                    return false;
                }
                PORTAL(pObj)->params[0] = atol(argument);
                PORTAL(pObj)->params[4] = 0;
                send_to_char("Dungeon vnum set.\n\r", ch);
                return true;
            }

            if (!str_cmp(argument, "-1")) {
                PORTAL(pObj)->params[0] = -1;
                PORTAL(pObj)->params[4] = 0;
                send_to_char("Destination mode set to area random.\n\r", ch);
                return true;
            }

            {
                AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
                WNUM dest_wnum = { NULL, 0 };

                if (!parse_widevnum(argument, context, &dest_wnum) || dest_wnum.pArea == NULL
                    || get_room_index(dest_wnum.pArea, dest_wnum.vnum) == NULL) {
                    send_to_char("No such destination room.\n\r", ch);
                    return false;
                }

                PORTAL(pObj)->params[0] = dest_wnum.vnum;
                PORTAL(pObj)->params[1] = 0;
                PORTAL(pObj)->params[4] = dest_wnum.pArea->uid;
            }

            send_to_char("Destination room set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "param1")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param1 <value> (floor/area/map_uid)\n\r", ch); return false; }
            PORTAL(pObj)->params[1] = atol(argument);
            if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON))
                send_to_char("Dungeon floor set.\n\r", ch);
            else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1)
                send_to_char("Area ID set.\n\r", ch);
            else
                send_to_char("Wilderness map UID set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "param2")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param2 <value> (map_x)\n\r", ch); return false; }
            if (!IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM)
                && PORTAL(pObj)->params[0] <= 0) {
                PORTAL(pObj)->params[2] = atol(argument);
                send_to_char("Wilderness map X set.\n\r", ch);
                return true;
            }
            send_to_char("This field only applies to wilderness portals.\n\r", ch);
            return false;
        }
        if (!str_prefix(field, "param3")) {
            if (argument[0] == '\0') { send_to_char("Syntax: portal param3 <value> (map_y)\n\r", ch); return false; }
            if (!IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM)
                && PORTAL(pObj)->params[0] <= 0) {
                PORTAL(pObj)->params[3] = atol(argument);
                send_to_char("Wilderness map Y set.\n\r", ch);
                return true;
            }
            send_to_char("This field only applies to wilderness portals.\n\r", ch);
            return false;
        }
        send_to_char("Valid fields: charges, exit, flags, destination, param1, param2, param3\n\r", ch);
        return false;
    }

    /* Contextual display based on portal flags */
    if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
        sprintf(buf,
            "{WPortal (Dungeon):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gdestination  {x [%ld] (dungeon)\n\r"
            "  {Gparam1       {x [%ld] (floor)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            PORTAL(pObj)->params[0],
            PORTAL(pObj)->params[1]);
    } else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1) {
        sprintf(buf,
            "{WPortal (Area Random):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gparam1       {x [%ld] (area id)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            PORTAL(pObj)->params[1]);
    } else if (PORTAL(pObj)->params[0] > 0) {
        sprintf(buf,
            "{WPortal (Static):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gdestination  {x [%s]\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            widevnum_string(
                PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL,
                PORTAL(pObj)->params[0], pObj->area));
    } else {
        sprintf(buf,
            "{WPortal (Wilderness):{x\n\r"
            "  {Gcharges      {x [%d]\n\r"
            "  {Gexit         {x %s\n\r"
            "  {Gflags        {x %s\n\r"
            "  {Gparam1       {x [%ld] (map uid)\n\r"
            "  {Gparam2       {x [%ld] (map x)\n\r"
            "  {Gparam3       {x [%ld] (map y)\n\r",
            PORTAL(pObj)->charges,
            flag_string(portal_exit_flags, PORTAL(pObj)->exit),
            flag_string(portal_flags, PORTAL(pObj)->flags),
            PORTAL(pObj)->params[1],
            PORTAL(pObj)->params[2],
            PORTAL(pObj)->params[3]);
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SCROLL
 * ============================================================================ */
OEDIT(oedit_scroll)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SCROLL(pObj)) {
        send_to_char("Object lacks the scroll type. Use '{Waddtype scroll{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: scroll mana <max_mana>\n\r", ch); return false; }
            SCROLL(pObj)->max_mana = atoi(argument);
            send_to_char("Scroll max mana set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: scroll flags <flag>\n\r", ch); return false; }
            long val = flag_value(scroll_flags, argument);
            if (val != NO_FLAG) SCROLL(pObj)->flags ^= val;
            send_to_char("Scroll flags toggled.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: mana, flags\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WScroll:{x\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gflags         {x [%s]\n\r",
        SCROLL(pObj)->max_mana,
        flag_string(scroll_flags, SCROLL(pObj)->flags));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SEED
 * ============================================================================ */
OEDIT(oedit_seed)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SEED(pObj)) {
        send_to_char("Object lacks the seed type. Use '{Waddtype seed{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "time")) {
            if (argument[0] == '\0') { send_to_char("Syntax: seed time <growth_time>\n\r", ch); return false; }
            SEED(pObj)->growth_time = atoi(argument);
            send_to_char("Growth time set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "object")) {
            if (argument[0] == '\0') { send_to_char("Syntax: seed object <vnum|0>\n\r", ch); return false; }
            if (atoi(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                OBJ_INDEX_DATA *key_obj;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_obj = key_wnum.pArea ? get_obj_index(key_wnum.pArea, key_wnum.vnum) : get_obj_index_global(key_wnum.vnum);
                if (!key_obj) {
                    send_to_char("No such object exists.\n\r", ch);
                    return false;
                }
                SEED(pObj)->object_vnum = key_wnum.vnum;
                SEED(pObj)->object_area_uid = key_obj->area ? key_obj->area->uid : 0;
            } else {
                SEED(pObj)->object_vnum = 0;
                SEED(pObj)->object_area_uid = 0;
            }
            send_to_char("Seed object set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: time, object\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WSeed:{x\n\r"
        "  {Gtime          {x [%d]\n\r"
        "  {Gobject        {x %s\n\r",
        SEED(pObj)->growth_time,
        widevnum_string(SEED(pObj)->object_area_uid > 0
            ? get_area_index(SEED(pObj)->object_area_uid) : NULL,
            SEED(pObj)->object_vnum, pObj->area));
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SEXTANT
 * ============================================================================ */
OEDIT(oedit_sextant)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SEXTANT(pObj)) {
        send_to_char("Object lacks the sextant type. Use '{Waddtype sextant{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "accuracy")) {
            if (argument[0] == '\0') { send_to_char("Syntax: sextant accuracy <percent>\n\r", ch); return false; }
            SEXTANT(pObj)->accuracy = atoi(argument);
            send_to_char("Sextant accuracy set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: accuracy\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WSextant:{x\n\r"
        "  {Gaccuracy      {x [%d%%]\n\r",
        SEXTANT(pObj)->accuracy);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SHIP
 * ============================================================================ */
OEDIT(oedit_ship)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_SHIP_TYPE(pObj)) {
        send_to_char("Object lacks the ship type. Use '{Waddtype ship{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship weight <kg>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->weight = atoi(argument);
            send_to_char("Ship weight set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "delay")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship delay <ticks>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->move_delay = atoi(argument);
            send_to_char("Ship move delay set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "mincrew")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship mincrew <count>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->min_crew = atoi(argument);
            send_to_char("Ship min crew set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "capacity")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship capacity <value>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->capacity = atoi(argument);
            send_to_char("Ship capacity set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "maxcrew")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship maxcrew <count>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->max_crew = atoi(argument);
            send_to_char("Ship max crew set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "room")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship room <room_vnum|0>\n\r", ch); return false; }
            if (atol(argument) != 0) {
                WNUM key_wnum = { NULL, 0 };
                ROOM_INDEX_DATA *key_room;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_room = key_wnum.pArea ? get_room_index(key_wnum.pArea, key_wnum.vnum) : get_room_index_global(key_wnum.vnum);
                if (!key_room) {
                    send_to_char("No such room exists.\n\r", ch);
                    return false;
                }
                SHIP_TYPE(pObj)->first_room = key_wnum.vnum;
                SHIP_TYPE(pObj)->first_room_area_uid = key_room->area ? key_room->area->uid : 0;
            } else {
                SHIP_TYPE(pObj)->first_room = 0;
                SHIP_TYPE(pObj)->first_room_area_uid = 0;
            }
            send_to_char("Ship first room set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "hitpoints")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship hitpoints <hp>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->hit_points = atoi(argument);
            send_to_char("Ship hit points set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "guns")) {
            if (argument[0] == '\0') { send_to_char("Syntax: ship guns <max>\n\r", ch); return false; }
            SHIP_TYPE(pObj)->max_guns = atoi(argument);
            send_to_char("Ship max guns set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: weight, delay, mincrew, capacity, maxcrew, room, hitpoints, guns\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WShip:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gdelay         {x [%d]\n\r"
        "  {Gmincrew       {x [%d]\n\r"
        "  {Gcapacity      {x [%d]\n\r"
        "  {Gmaxcrew       {x [%d]\n\r"
        "  {Groom          {x %s\n\r"
        "  {Ghitpoints     {x [%d]\n\r"
        "  {Gguns          {x [%d]\n\r",
        SHIP_TYPE(pObj)->weight,
        SHIP_TYPE(pObj)->move_delay,
        SHIP_TYPE(pObj)->min_crew,
        SHIP_TYPE(pObj)->capacity,
        SHIP_TYPE(pObj)->max_crew,
        widevnum_string(SHIP_TYPE(pObj)->first_room_area_uid > 0
            ? get_area_index(SHIP_TYPE(pObj)->first_room_area_uid) : NULL,
            SHIP_TYPE(pObj)->first_room, pObj->area),
        SHIP_TYPE(pObj)->hit_points,
        SHIP_TYPE(pObj)->max_guns);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TATTOO
 * ============================================================================ */
OEDIT(oedit_tattoo)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TATTOO(pObj)) {
        send_to_char("Object lacks the tattoo type. Use '{Waddtype tattoo{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "touches")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo touches <count>\n\r", ch); return false; }
            TATTOO(pObj)->touches = atoi(argument);
            send_to_char("Tattoo touches set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "fading")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo fading <percent>\n\r", ch); return false; }
            TATTOO(pObj)->fading_chance = atoi(argument);
            send_to_char("Tattoo fading chance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "faderate")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tattoo faderate <rate>\n\r", ch); return false; }
            TATTOO(pObj)->fading_rate = atoi(argument);
            send_to_char("Tattoo fading rate set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: touches, fading, faderate\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTattoo:{x\n\r"
        "  {Gtouches       {x [%d]\n\r"
        "  {Gfading        {x [%d%%]\n\r"
        "  {Gfaderate      {x [%d]\n\r",
        TATTOO(pObj)->touches,
        TATTOO(pObj)->fading_chance,
        TATTOO(pObj)->fading_rate);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TELESCOPE
 * ============================================================================ */
OEDIT(oedit_telescope)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TELESCOPE(pObj)) {
        send_to_char("Object lacks the telescope type. Use '{Waddtype telescope{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "distance")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope distance <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val < 0 || (val > 0 && val < TELESCOPE(pObj)->min_distance) || val > TELESCOPE(pObj)->max_distance) {
                char msg[MSL];
                sprintf(msg, "Distance must be 0 (collapsed), or from %d to %d.\n\r",
                    TELESCOPE(pObj)->min_distance, TELESCOPE(pObj)->max_distance);
                send_to_char(msg, ch);
                return false;
            }
            TELESCOPE(pObj)->distance = val;
            send_to_char("Telescope distance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "mindist")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope mindist <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0) { send_to_char("Minimum distance must be greater than zero.\n\r", ch); return false; }
            if (val > TELESCOPE(pObj)->max_distance) {
                send_to_char("Must be less than or equal to max distance.\n\r", ch);
                return false;
            }
            TELESCOPE(pObj)->min_distance = val;
            send_to_char("Telescope min distance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "maxdist")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope maxdist <val>\n\r", ch); return false; }
            int val = atoi(argument);
            if (val <= 0) { send_to_char("Maximum distance must be greater than zero.\n\r", ch); return false; }
            if (val < TELESCOPE(pObj)->min_distance) {
                send_to_char("Must be greater than or equal to min distance.\n\r", ch);
                return false;
            }
            TELESCOPE(pObj)->max_distance = val;
            send_to_char("Telescope max distance set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "bonus")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope bonus <view_size>\n\r", ch); return false; }
            TELESCOPE(pObj)->bonus_view = atoi(argument);
            send_to_char("Telescope bonus view set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "heading")) {
            if (argument[0] == '\0') { send_to_char("Syntax: telescope heading <dir|-1 for none>\n\r", ch); return false; }
            TELESCOPE(pObj)->heading = atoi(argument);
            send_to_char("Telescope heading set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: distance, mindist, maxdist, bonus, heading\n\r", ch);
        return false;
    }

    if (TELESCOPE(pObj)->heading >= 0) {
        sprintf(buf,
            "{WTelescope:{x\n\r"
            "  {Gdistance      {x [%d]\n\r"
            "  {Gmindist       {x [%d]\n\r"
            "  {Gmaxdist       {x [%d]\n\r"
            "  {Gbonus         {x [%d]\n\r"
            "  {Gheading       {x [%d]\n\r",
            TELESCOPE(pObj)->distance,
            TELESCOPE(pObj)->min_distance,
            TELESCOPE(pObj)->max_distance,
            TELESCOPE(pObj)->bonus_view,
            TELESCOPE(pObj)->heading);
    } else {
        sprintf(buf,
            "{WTelescope:{x\n\r"
            "  {Gdistance      {x [%d]\n\r"
            "  {Gmindist       {x [%d]\n\r"
            "  {Gmaxdist       {x [%d]\n\r"
            "  {Gbonus         {x [%d]\n\r"
            "  {Gheading       {x [none]\n\r",
            TELESCOPE(pObj)->distance,
            TELESCOPE(pObj)->min_distance,
            TELESCOPE(pObj)->max_distance,
            TELESCOPE(pObj)->bonus_view);
    }
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TOOL
 * ============================================================================ */
OEDIT(oedit_tool)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TOOL(pObj)) {
        send_to_char("Object lacks the tool type. Use '{Waddtype whetstone{x' (or chisel, pick, etc).\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tool type <tool_type>\n\r", ch); return false; }
            int val = flag_value(tool_types, argument);
            if (val == NO_FLAG) { send_to_char("Invalid tool type.\n\r", ch); return false; }
            TOOL(pObj)->type = val;
            send_to_char("Tool type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "tier")) {
            if (argument[0] == '\0') { send_to_char("Syntax: tool tier <level>\n\r", ch); return false; }
            TOOL(pObj)->tier = atoi(argument);
            send_to_char("Tool tier set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: type, tier\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTool:{x\n\r"
        "  {Gtype          {x %s\n\r"
        "  {Gtier          {x [%d]\n\r",
        flag_string(tool_types, TOOL(pObj)->type),
        TOOL(pObj)->tier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  TRADE
 * ============================================================================ */
OEDIT(oedit_trade)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_TRADE(pObj)) {
        send_to_char("Object lacks the trade type. Use '{Waddtype trade_type{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "type")) {
            if (argument[0] == '\0') {
                int i = 0;
                send_to_char("Trade types:\n\r", ch);
                while (trade_table[i].trade_type != -1) {
                    send_to_char(trade_table[i].name, ch);
                    send_to_char("\n\r", ch);
                    i++;
                }
                return false;
            }
            int i = get_trade_item(argument);
            if (i == 0 && str_cmp(argument, "none")) {
                send_to_char("Invalid trade type. Use 'trade type' to see list.\n\r", ch);
                return false;
            }
            TRADE(pObj)->trade_type = i;
            send_to_char("Trade type set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: type\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WTrade:{x\n\r"
        "  {Gtype          {x %s\n\r",
        trade_table[TRADE(pObj)->trade_type].name);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WAND (also staff)
 * ============================================================================ */
OEDIT(oedit_wand)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WAND(pObj)) {
        send_to_char("Object lacks the wand type. Use '{Waddtype wand{x' or '{Waddtype staff{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand mana <max_mana>\n\r", ch); return false; }
            WAND(pObj)->max_mana = atoi(argument);
            send_to_char("Max mana set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand charges <current>\n\r", ch); return false; }
            WAND(pObj)->charges = atoi(argument);
            send_to_char("Current charges set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "maxcharges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand maxcharges <max>\n\r", ch); return false; }
            WAND(pObj)->max_charges = atoi(argument);
            send_to_char("Max charges set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "cooldown")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand cooldown <ticks>\n\r", ch); return false; }
            WAND(pObj)->cooldown = atoi(argument);
            send_to_char("Cooldown set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "recharge")) {
            if (argument[0] == '\0') { send_to_char("Syntax: wand recharge <ticks>\n\r", ch); return false; }
            WAND(pObj)->recharge_time = atoi(argument);
            send_to_char("Recharge time set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: mana, charges, maxcharges, cooldown, recharge\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWand/Staff:{x\n\r"
        "  {Gmana          {x [%d]\n\r"
        "  {Gcharges       {x [%d]\n\r"
        "  {Gmaxcharges    {x [%d]\n\r"
        "  {Gcooldown      {x [%d]\n\r"
        "  {Grecharge      {x [%d]\n\r",
        WAND(pObj)->max_mana,
        WAND(pObj)->charges,
        WAND(pObj)->max_charges,
        WAND(pObj)->cooldown,
        WAND(pObj)->recharge_time);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WEAPON
 * ============================================================================ */
OEDIT(oedit_weapon)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WEAPON(pObj)) {
        send_to_char("Object lacks the weapon type. Use '{Waddtype weapon{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "class")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon class <weapon_class>\n\r", ch); return false; }
            int val = flag_value(weapon_class, argument);
            if (val == NO_FLAG) { send_to_char("Invalid weapon class.\n\r", ch); return false; }
            WEAPON(pObj)->weapon_class = val;
            send_to_char("Weapon class set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "dice")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon dice <number> <size> [bonus]\n\r", ch); return false; }
            char snum[MIL], ssize[MIL];
            argument = one_argument(argument, snum);
            argument = one_argument(argument, ssize);
            if (snum[0] == '\0' || ssize[0] == '\0') {
                send_to_char("Syntax: weapon dice <number> <size> [bonus]\n\r", ch);
                return false;
            }
            WEAPON(pObj)->damage.number = atoi(snum);
            WEAPON(pObj)->damage.size = atoi(ssize);
            if (argument[0] != '\0')
                WEAPON(pObj)->damage.bonus = atoi(argument);
            set_weapon_dice(pObj);
            send_to_char("Weapon damage dice set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "damtype")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon damtype <attack_type>\n\r", ch); return false; }
            WEAPON(pObj)->damage_type = attack_lookup(argument);
            send_to_char("Weapon damage type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "flags")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon flags <weapon_flag>\n\r", ch); return false; }
            long val = flag_value(weapon_type2, argument);
            if (val != NO_FLAG) WEAPON(pObj)->flags ^= val;
            send_to_char("Weapon flags toggled.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "range")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon range <distance>\n\r", ch); return false; }
            WEAPON(pObj)->range = atoi(argument);
            send_to_char("Weapon range set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "mana")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon mana <max_mana>\n\r", ch); return false; }
            WEAPON(pObj)->max_mana = atoi(argument);
            send_to_char("Weapon max mana set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "charges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon charges <current>\n\r", ch); return false; }
            WEAPON(pObj)->charges = atoi(argument);
            send_to_char("Weapon charges set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "maxcharges")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon maxcharges <max>\n\r", ch); return false; }
            WEAPON(pObj)->max_charges = atoi(argument);
            send_to_char("Weapon max charges set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "cooldown")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon cooldown <ticks>\n\r", ch); return false; }
            WEAPON(pObj)->cooldown = atoi(argument);
            send_to_char("Weapon cooldown set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "recharge")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weapon recharge <ticks>\n\r", ch); return false; }
            WEAPON(pObj)->recharge_time = atoi(argument);
            send_to_char("Weapon recharge time set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: class, dice, damtype, flags, range, mana, charges, maxcharges, cooldown, recharge\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWeapon:{x\n\r"
        "  {Gclass         {x %s\n\r"
        "  {Gdice          {x %dd%d+%d\n\r"
        "  {Gdamtype       {x %s\n\r"
        "  {Gflags         {x %s\n\r"
        "  {Grange         {x [%d]\n\r",
        flag_string(weapon_class, WEAPON(pObj)->weapon_class),
        WEAPON(pObj)->damage.number, WEAPON(pObj)->damage.size, WEAPON(pObj)->damage.bonus,
        attack_table[WEAPON(pObj)->damage_type].name,
        flag_string(weapon_type2, WEAPON(pObj)->flags),
        WEAPON(pObj)->range);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  WEAPON CONTAINER
 * ============================================================================ */
OEDIT(oedit_weaponcon)
{
    OBJ_INDEX_DATA *pObj;
    char field[MIL];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    if (!IS_WEAPON_CON(pObj)) {
        send_to_char("Object lacks the weapon_container type. Use '{Waddtype weapon_container{x'.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, field);

    if (field[0] != '\0') {
        if (!str_prefix(field, "weight")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weight <max_kg>\n\r", ch); return false; }
            WEAPON_CON(pObj)->max_weight = atoi(argument);
            send_to_char("Max weight set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "weapontype")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weapontype <weapon_class>\n\r", ch); return false; }
            int val = flag_value(weapon_class, argument);
            if (val == NO_FLAG) { send_to_char("Invalid weapon class.\n\r", ch); return false; }
            WEAPON_CON(pObj)->weapon_type = val;
            send_to_char("Weapon type set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "items")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon items <max>\n\r", ch); return false; }
            WEAPON_CON(pObj)->max_items = atoi(argument);
            send_to_char("Max items set.\n\r", ch);
            return true;
        }
        if (!str_prefix(field, "weightmult")) {
            if (argument[0] == '\0') { send_to_char("Syntax: weaponcon weightmult <multiplier>\n\r", ch); return false; }
            WEAPON_CON(pObj)->weight_multiplier = atoi(argument);
            send_to_char("Weight multiplier set.\n\r", ch);
            return true;
        }
        send_to_char("Valid fields: weight, weapontype, items, weightmult\n\r", ch);
        return false;
    }

    sprintf(buf,
        "{WWeapon Container:{x\n\r"
        "  {Gweight        {x [%d kg]\n\r"
        "  {Gweapontype    {x %s\n\r"
        "  {Gitems         {x [%d]\n\r"
        "  {Gweightmult    {x [%d%%]\n\r",
        WEAPON_CON(pObj)->max_weight,
        flag_string(weapon_class, WEAPON_CON(pObj)->weapon_type),
        WEAPON_CON(pObj)->max_items,
        WEAPON_CON(pObj)->weight_multiplier);
    send_to_char(buf, ch);
    return false;
}

/* ============================================================================
 *  SHOW ALL TYPE DATA
 * ============================================================================
 *
 * Writes human-readable type data for ALL active types on an object to a
 * BUFFER.  Called by oedit_show so the editor displays every type that is
 * present, not just the primary one.
 */
void oedit_show_type_data(OBJ_INDEX_DATA *pObj, BUFFER *buffer)
{
    char buf[MSL];

    if (IS_ARMOR(pObj)) {
        sprintf(buf,
            "\n\r{WArmour:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gstrength      {x %s\n\r"
            "  {Gpierce        {x [%d]\n\r"
            "  {Gbash          {x [%d]\n\r"
            "  {Gslash         {x [%d]\n\r"
            "  {Gexotic        {x [%d]\n\r",
            flag_string(armor_types, ARMOR(pObj)->armor_type),
            armour_strength_table[ARMOR(pObj)->armor_strength].name,
            ARMOR(pObj)->protection[0],
            ARMOR(pObj)->protection[1],
            ARMOR(pObj)->protection[2],
            ARMOR(pObj)->protection[3]);
        add_buf(buffer, buf);
    }

    if (IS_BODY_PART(pObj)) {
        RACE_DATA *part_race = race_lookup_uid((int16_t)BODY_PART(pObj)->race_uid);
        sprintf(buf,
            "\n\r{WBody Part:{x\n\r"
            "  {Gparts         {x %s\n\r"
            "  {Grace           {x %s\n\r",
            flag_string(part_flags, BODY_PART(pObj)->parts),
            part_race ? part_race->name : "unknown");
        add_buf(buffer, buf);
    }

    if (IS_BOOK(pObj)) {
        sprintf(buf,
            "\n\r{WBook:{x\n\r"
            "  {Gflags         {x [%s]\n\r",
            flag_string(container_flags, BOOK(pObj)->flags));
        add_buf(buffer, buf);
    }

    if (IS_CART(pObj)) {
        sprintf(buf,
            "\n\r{WCart:{x\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gdelay         {x [%d]\n\r"
            "  {Gstrength      {x [%d]\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r"
            "  {Gvanish        {x [%d]\n\r",
            CART(pObj)->capacity,
            CART(pObj)->move_delay,
            CART(pObj)->min_strength,
            CART(pObj)->max_items,
            CART(pObj)->weight_multiplier,
            flag_string(cart_flags, CART(pObj)->flags),
            CART(pObj)->vanish_time);
        add_buf(buffer, buf);
    }

    if (IS_COMPASS(pObj)) {
        sprintf(buf,
            "\n\r{WCompass:{x\n\r"
            "  {Gaccuracy      {x [%d%%]\n\r",
            COMPASS(pObj)->accuracy);
        add_buf(buffer, buf);
    }

    if (IS_CONTAINER(pObj)) {
        sprintf(buf,
            "\n\r{WContainer:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gflags         {x [%s]\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d%%]\n\r",
            CONTAINER(pObj)->max_weight,
            flag_string(container_flags, CONTAINER(pObj)->flags),
            CONTAINER(pObj)->max_items,
            CONTAINER(pObj)->weight_multiplier);
        add_buf(buffer, buf);
    }

    if (IS_CORPSE(pObj)) {
        sprintf(buf,
            "\n\r{WCorpse:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gresurrection  {x %d%%\n\r"
            "  {Ganimation     {x %d%%\n\r"
            "  {Gparts         {x %s\n\r"
            "  {Gmobile        {x [%s]\n\r",
            flag_string(corpse_types, CORPSE(pObj)->corpse_type),
            CORPSE(pObj)->resurrection,
            CORPSE(pObj)->animation,
            flag_string(part_flags, CORPSE(pObj)->body_parts),
            widevnum_string(
                CORPSE(pObj)->mobile_area_uid > 0 ? get_area_index(CORPSE(pObj)->mobile_area_uid) : NULL,
                CORPSE(pObj)->mobile_vnum, pObj->area));
        add_buf(buffer, buf);
    }

    if (IS_FLUID_CON(pObj)) {
        sprintf(buf,
            "\n\r{WFluid Container:{x\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gamount        {x [%d]\n\r"
            "  {Gliquid        {x %s\n\r"
            "  {Gpoison        {x %s\n\r"
            "  {Grefill        {x [%d]\n\r",
            FLUID_CON(pObj)->capacity,
            FLUID_CON(pObj)->amount,
            liquid_name(FLUID_CON(pObj)->liquid),
            FLUID_CON(pObj)->poison != 0 ? "Yes" : "No",
            FLUID_CON(pObj)->refill_rate);
        add_buf(buffer, buf);
    }

    if (IS_FOOD(pObj)) {
        sprintf(buf,
            "\n\r{WFood:{x\n\r"
            "  {Ghunger        {x [%d]\n\r"
            "  {Gfull          {x [%d]\n\r"
            "  {Gpoison        {x %s\n\r"
            "  {Gtimer         {x [%d]\n\r",
            FOOD(pObj)->hunger,
            FOOD(pObj)->full,
            FOOD(pObj)->poison != 0 ? "Yes" : "No",
            FOOD(pObj)->timer);
        add_buf(buffer, buf);
    }

    if (IS_FURNITURE(pObj)) {
        sprintf(buf,
            "\n\r{WFurniture:{x\n\r"
            "  {Gpeople        {x [%d]\n\r"
            "  {Gweight        {x [%d]\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Gheal          {x [%d]\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gmove          {x [%d]\n\r",
            FURNITURE(pObj)->max_people,
            FURNITURE(pObj)->max_weight,
            flag_string(furniture_flags, FURNITURE(pObj)->flags),
            FURNITURE(pObj)->heal_rate,
            FURNITURE(pObj)->mana_rate,
            FURNITURE(pObj)->move_rate);
        add_buf(buffer, buf);
    }

    if (IS_HERB(pObj)) {
        sprintf(buf,
            "\n\r{WHerb:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Ghealing       {x [%d%%]\n\r"
            "  {Gregen         {x [%d%%]\n\r"
            "  {Grefresh       {x [%d%%]\n\r"
            "  {Gimmunity      {x %s\n\r"
            "  {Gresistance    {x %s\n\r"
            "  {Gvulnerability {x %s\n\r"
            "  {Gspell         {x %s\n\r",
            herb_table[HERB(pObj)->type].name,
            HERB(pObj)->healing,
            HERB(pObj)->regenerative,
            HERB(pObj)->refreshing,
            flag_string(imm_flags, HERB(pObj)->immunity),
            flag_string(res_flags, HERB(pObj)->resistance),
            flag_string(vuln_flags, HERB(pObj)->vulnerability),
            HERB(pObj)->spell > 0 ? skill_table[HERB(pObj)->spell].name : "none");
        add_buf(buffer, buf);
    }

    if (IS_INK(pObj)) {
        sprintf(buf,
            "\n\r{WInk:{x\n\r"
            "  {Gtype1         {x %s\n\r"
            "  {Gtype2         {x %s\n\r"
            "  {Gtype3         {x %s\n\r",
            flag_string(catalyst_types, INK(pObj)->types[0]),
            flag_string(catalyst_types, INK(pObj)->types[1]),
            flag_string(catalyst_types, INK(pObj)->types[2]));
        add_buf(buffer, buf);
    }

    if (IS_INSTRUMENT(pObj)) {
        sprintf(buf,
            "\n\r{WInstrument:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Gbeatsmin      {x [%d%%]\n\r"
            "  {Gbeatsmax      {x [%d%%]\n\r",
            flag_string(instrument_types, INSTRUMENT(pObj)->type),
            flag_string(instrument_flags, INSTRUMENT(pObj)->flags),
            INSTRUMENT(pObj)->beats_min,
            INSTRUMENT(pObj)->beats_max);
        add_buf(buffer, buf);
    }

    if (IS_JEWELRY(pObj)) {
        sprintf(buf,
            "\n\r{WJewelry:{x\n\r"
            "  {Gmana          {x [%d]\n\r",
            JEWELRY(pObj)->max_mana);
        add_buf(buffer, buf);
    }

    if (IS_LIGHT(pObj)) {
        if (LIGHT(pObj)->duration == -1) {
            sprintf(buf,
                "\n\r{WLight:{x\n\r"
                "  {Gduration      {x Infinite [-1]\n\r"
                "  {Gflags         {x [%s]\n\r",
                flag_string(light_flags, LIGHT(pObj)->flags));
        } else {
            sprintf(buf,
                "\n\r{WLight:{x\n\r"
                "  {Gduration      {x [%d]\n\r"
                "  {Gflags         {x [%s]\n\r",
                LIGHT(pObj)->duration,
                flag_string(light_flags, LIGHT(pObj)->flags));
        }
        add_buf(buffer, buf);
    }

    if (IS_MAP(pObj)) {
        sprintf(buf,
            "\n\r{WMap:{x\n\r"
            "  {Gwuid          {x [%ld]\n\r"
            "  {Gx             {x [%ld]\n\r"
            "  {Gy             {x [%ld]\n\r",
            MAP(pObj)->wuid,
            MAP(pObj)->x,
            MAP(pObj)->y);
        add_buf(buffer, buf);
    }

    if (IS_MIST(pObj)) {
        sprintf(buf,
            "\n\r{WMist:{x\n\r"
            "  {Gobjects       {x [%d%%]\n\r"
            "  {Gcharacters    {x [%d%%]\n\r"
            "  {Groom          {x [%d%%]\n\r",
            MIST(pObj)->obscure_objs,
            MIST(pObj)->obscure_mobs,
            MIST(pObj)->obscure_room);
        add_buf(buffer, buf);
    }

    if (IS_MONEY(pObj)) {
        sprintf(buf,
            "\n\r{WMoney:{x\n\r"
            "  {Gsilver        {x [%d]\n\r"
            "  {Ggold          {x [%d]\n\r",
            MONEY(pObj)->silver,
            MONEY(pObj)->gold);
        add_buf(buffer, buf);
    }

    if (IS_PAGE(pObj)) {
        sprintf(buf,
            "\n\r{WPage:{x\n\r"
            "  {Gnumber        {x [%d]\n\r"
            "  {Gtitle         {x %s\n\r",
            PAGE(pObj)->page_no,
            PAGE(pObj)->title ? PAGE(pObj)->title : "(none)");
        add_buf(buffer, buf);
    }

    if (IS_PORTAL(pObj)) {
        if (IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON)) {
            sprintf(buf,
                "\n\r{WPortal (Dungeon):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gdestination  {x [%ld] (dungeon)\n\r"
                "  {Gparam1       {x [%ld] (floor)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                PORTAL(pObj)->params[0],
                PORTAL(pObj)->params[1]);
        } else if (IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1) {
            sprintf(buf,
                "\n\r{WPortal (Area Random):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gparam1       {x [%ld] (area id)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                PORTAL(pObj)->params[1]);
        } else if (PORTAL(pObj)->params[0] > 0) {
            sprintf(buf,
                "\n\r{WPortal (Static):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gdestination  {x [%s]\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                widevnum_string(
                    PORTAL(pObj)->params[4] > 0 ? get_area_index(PORTAL(pObj)->params[4]) : NULL,
                    PORTAL(pObj)->params[0], pObj->area));
        } else {
            sprintf(buf,
                "\n\r{WPortal (Wilderness):{x\n\r"
                "  {Gcharges      {x [%d]\n\r"
                "  {Gexit         {x %s\n\r"
                "  {Gflags        {x %s\n\r"
                "  {Gparam1       {x [%ld] (map uid)\n\r"
                "  {Gparam2       {x [%ld] (map x)\n\r"
                "  {Gparam3       {x [%ld] (map y)\n\r",
                PORTAL(pObj)->charges,
                flag_string(portal_exit_flags, PORTAL(pObj)->exit),
                flag_string(portal_flags, PORTAL(pObj)->flags),
                PORTAL(pObj)->params[1],
                PORTAL(pObj)->params[2],
                PORTAL(pObj)->params[3]);
        }
        add_buf(buffer, buf);
    }

    if (IS_SCROLL(pObj)) {
        sprintf(buf,
            "\n\r{WScroll:{x\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gflags         {x [%s]\n\r",
            SCROLL(pObj)->max_mana,
            flag_string(scroll_flags, SCROLL(pObj)->flags));
        add_buf(buffer, buf);
    }

    if (IS_SEED(pObj)) {
        sprintf(buf,
            "\n\r{WSeed:{x\n\r"
            "  {Gtime          {x [%d]\n\r"
            "  {Gobject        {x [%s]\n\r",
            SEED(pObj)->growth_time,
            widevnum_string(
                SEED(pObj)->object_area_uid > 0 ? get_area_index(SEED(pObj)->object_area_uid) : NULL,
                SEED(pObj)->object_vnum, pObj->area));
        add_buf(buffer, buf);
    }

    if (IS_SEXTANT(pObj)) {
        sprintf(buf,
            "\n\r{WSextant:{x\n\r"
            "  {Gaccuracy      {x [%d%%]\n\r",
            SEXTANT(pObj)->accuracy);
        add_buf(buffer, buf);
    }

    if (IS_SHIP_TYPE(pObj)) {
        sprintf(buf,
            "\n\r{WShip:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gdelay         {x [%d]\n\r"
            "  {Gmincrew       {x [%d]\n\r"
            "  {Gcapacity      {x [%d]\n\r"
            "  {Gmaxcrew       {x [%d]\n\r"
            "  {Groom          {x [%s]\n\r"
            "  {Ghitpoints     {x [%d]\n\r"
            "  {Gguns          {x [%d]\n\r",
            SHIP_TYPE(pObj)->weight,
            SHIP_TYPE(pObj)->move_delay,
            SHIP_TYPE(pObj)->min_crew,
            SHIP_TYPE(pObj)->capacity,
            SHIP_TYPE(pObj)->max_crew,
            widevnum_string(
                SHIP_TYPE(pObj)->first_room_area_uid > 0 ? get_area_index(SHIP_TYPE(pObj)->first_room_area_uid) : NULL,
                SHIP_TYPE(pObj)->first_room, pObj->area),
            SHIP_TYPE(pObj)->hit_points,
            SHIP_TYPE(pObj)->max_guns);
        add_buf(buffer, buf);
    }

    if (IS_TATTOO(pObj)) {
        sprintf(buf,
            "\n\r{WTattoo:{x\n\r"
            "  {Gtouches       {x [%d]\n\r"
            "  {Gfading        {x [%d%%]\n\r"
            "  {Gfaderate      {x [%d]\n\r",
            TATTOO(pObj)->touches,
            TATTOO(pObj)->fading_chance,
            TATTOO(pObj)->fading_rate);
        add_buf(buffer, buf);
    }

    if (IS_TELESCOPE(pObj)) {
        if (TELESCOPE(pObj)->heading >= 0) {
            sprintf(buf,
                "\n\r{WTelescope:{x\n\r"
                "  {Gdistance      {x [%d]\n\r"
                "  {Gmindist       {x [%d]\n\r"
                "  {Gmaxdist       {x [%d]\n\r"
                "  {Gbonus         {x [%d]\n\r"
                "  {Gheading       {x [%d]\n\r",
                TELESCOPE(pObj)->distance,
                TELESCOPE(pObj)->min_distance,
                TELESCOPE(pObj)->max_distance,
                TELESCOPE(pObj)->bonus_view,
                TELESCOPE(pObj)->heading);
        } else {
            sprintf(buf,
                "\n\r{WTelescope:{x\n\r"
                "  {Gdistance      {x [%d]\n\r"
                "  {Gmindist       {x [%d]\n\r"
                "  {Gmaxdist       {x [%d]\n\r"
                "  {Gbonus         {x [%d]\n\r"
                "  {Gheading       {x [none]\n\r",
                TELESCOPE(pObj)->distance,
                TELESCOPE(pObj)->min_distance,
                TELESCOPE(pObj)->max_distance,
                TELESCOPE(pObj)->bonus_view);
        }
        add_buf(buffer, buf);
    }

    if (IS_TOOL(pObj)) {
        sprintf(buf,
            "\n\r{WTool:{x\n\r"
            "  {Gtype          {x %s\n\r"
            "  {Gtier          {x [%d]\n\r",
            flag_string(tool_types, TOOL(pObj)->type),
            TOOL(pObj)->tier);
        add_buf(buffer, buf);
    }

    if (IS_TRADE(pObj)) {
        sprintf(buf,
            "\n\r{WTrade:{x\n\r"
            "  {Gtype          {x %s\n\r",
            trade_table[TRADE(pObj)->trade_type].name);
        add_buf(buffer, buf);
    }

    if (IS_WAND(pObj)) {
        sprintf(buf,
            "\n\r{WWand/Staff:{x\n\r"
            "  {Gmana          {x [%d]\n\r"
            "  {Gcharges       {x [%d]\n\r"
            "  {Gmaxcharges    {x [%d]\n\r"
            "  {Gcooldown      {x [%d]\n\r"
            "  {Grecharge      {x [%d]\n\r",
            WAND(pObj)->max_mana,
            WAND(pObj)->charges,
            WAND(pObj)->max_charges,
            WAND(pObj)->cooldown,
            WAND(pObj)->recharge_time);
        add_buf(buffer, buf);
    }

    if (IS_WEAPON(pObj)) {
        sprintf(buf,
            "\n\r{WWeapon:{x\n\r"
            "  {Gclass         {x %s\n\r"
            "  {Gdice          {x %dd%d+%d\n\r"
            "  {Gdamtype       {x %s\n\r"
            "  {Gflags         {x %s\n\r"
            "  {Grange         {x [%d]\n\r",
            flag_string(weapon_class, WEAPON(pObj)->weapon_class),
            WEAPON(pObj)->damage.number, WEAPON(pObj)->damage.size, WEAPON(pObj)->damage.bonus,
            attack_table[WEAPON(pObj)->damage_type].name,
            flag_string(weapon_type2, WEAPON(pObj)->flags),
            WEAPON(pObj)->range);
        add_buf(buffer, buf);
    }

    if (IS_WEAPON_CON(pObj)) {
        sprintf(buf,
            "\n\r{WWeapon Container:{x\n\r"
            "  {Gweight        {x [%d kg]\n\r"
            "  {Gweapontype    {x %s\n\r"
            "  {Gitems         {x [%d]\n\r"
            "  {Gweightmult    {x [%d%%]\n\r",
            WEAPON_CON(pObj)->max_weight,
            flag_string(weapon_class, WEAPON_CON(pObj)->weapon_type),
            WEAPON_CON(pObj)->max_items,
            WEAPON_CON(pObj)->weight_multiplier);
        add_buf(buffer, buf);
    }
}
