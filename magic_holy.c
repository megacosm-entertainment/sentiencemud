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
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "wilds.h"
#include "skill_data.h"

SPELL_FUNC(spell_avatar_shield)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    AFFECT_DATA af;
    bool perm = false;
    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    if (perm && is_affected(victim, sn)) {
        affect_strip(victim, sn);
    } else if (IS_AFFECTED2(victim, AFF2_AVATAR_SHIELD)) {
        if (victim == ch)
            send_to_char("You are already protected.\n\r",ch);
        else
            act("$N is already protected.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    af.slot = obj_wear_loc;
    af.where = TO_AFFECTS;
    af.group = AFFGROUP_DIVINE;
    af.type = sn;
    af.skill = skill;
    af.level = level;
    af.duration  = perm ? -1 : 35;
    af.modifier = 0;
    af.bitvector = 0;
    af.bitvector2 = AFF2_AVATAR_SHIELD;
    affect_to_char(victim, &af);
    send_to_char("You feel shielded from evil.\n\r", victim);
    if (ch != victim)
        act("$N is shielded from evil.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return true;
}


SPELL_FUNC(spell_bless)
{
    int sn = skill->uid;
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    AFFECT_DATA af;
    bool perm = false;

    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    /* deal with the object case first */
    if (target == TARGET_OBJ) {
        obj = (OBJ_DATA *) vo;
        if (IS_OBJ_STAT(obj,ITEM_BLESS)) {
            act("$p is already blessed.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return false;
        }

        if (IS_OBJ_STAT(obj,ITEM_EVIL)) {
            AFFECT_DATA *paf;

            paf = affect_find(obj->affected,skill_resolve_gsn("curse"));
            if (!saves_dispel(ch, NULL, paf ? paf->level : obj->level)) {
                if (paf) affect_remove_obj(obj,paf);
                act("$p glows a pale blue.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL, NULL, NULL);
                REMOVE_BIT(obj->extra[0],ITEM_EVIL);
                return true;
            } else {
                act("The evil of $p is too powerful for you to overcome.", ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
                return false;
            }
        }

        af.slot = WEAR_NONE;
        af.where = TO_OBJECT;
        af.group = AFFGROUP_ENCHANT;
        af.type	= sn;
    af.skill = skill;
        af.level = level;
        af.duration = 6 + level/4;
        af.location = APPLY_HITROLL;
        af.modifier = 1;
        af.bitvector = ITEM_BLESS;
        af.bitvector2 = 0;
        affect_to_obj(obj,&af);

        act("$p glows with a holy aura.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL, NULL, NULL);

        return true;
    }

    /* character target */
    victim = (CHAR_DATA *) vo;

    if (perm && is_affected(victim, sn)) {
        affect_strip(victim, sn);
    } else if (victim->position == POS_FIGHTING || is_affected(victim, sn)) {
        if (victim == ch)
            send_to_char("You are already blessed.\n\r",ch);
        else
            act("$N already has divine favor.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    af.slot	= obj_wear_loc;
    af.where = TO_AFFECTS;
    af.group = AFFGROUP_DIVINE;
    af.type = sn;
    af.skill = skill;
    af.level = level;
    af.duration = perm ? -1 : (6+level);
    af.location = APPLY_HITROLL;
    af.modifier = level / 8;
    af.bitvector = 0;
    af.bitvector2 = 0;
    affect_to_char(victim, &af);
    send_to_char("You feel righteous.\n\r", victim);
    if (ch != victim) act("You grant $N the favor of your god.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    return true;
}


SPELL_FUNC(spell_dispel_evil)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    int dam;

    if (IS_GOOD(victim)) {
        act("The gods protect $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return false;
    }

    if (IS_NEUTRAL(victim)) {
        act("$N does not seem to be affected.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    if (victim->hit > (ch->tot_level * 4))
        dam = dice(level, 4);
    else
        dam = UMAX(victim->hit, dice(level,4));
    if (saves_spell(level, victim,DAM_HOLY))
        dam /= 2;
    damage(ch, victim, dam, sn, DAM_HOLY ,true);
    return true;
}

SPELL_FUNC(spell_exorcism)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    ROOM_INDEX_DATA *room;
    AREA_DATA *area;
    int chance;
    int lvl, catalyst;

    act("{YYou perform an exorcism on $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{Y$n performs an exorcism on you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    act("{Y$n performs an exorcism on $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);

    lvl = victim->tot_level - ch->tot_level;
    if(IS_REMORT(victim)) lvl += LEVEL_HERO;	// If the victim is remort, it will require MORE catalyst
    if(IS_REMORT(ch)) lvl -= LEVEL_HERO;		// If the caster is remort, it will require LESS catalyst
    lvl = (lvl > 19) ? (lvl / 10) : 1;

    catalyst = use_catalyst(ch,NULL,CATALYST_HOLY,CATALYST_HOLD|CATALYST_ACTIVE,600,lvl,CATALYST_MAXSTRENGTH,true);

    if (victim->alignment > -500) {
        act("$N is unaffected by your exorcism.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$N's exorcism has no effect upon you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("$N is unaffected by $n's exorcism.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        return false;
    } else {
        act("A large phantasmal pit opens up beneath you making a loud slurping noise!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("A large phantasmal pit opens up beneath $n making a loud slurping noise!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }

    if (victim->tot_level <= LEVEL_HERO/5) area = find_area("Maze-Level1");
    else if (victim->tot_level <= LEVEL_HERO/3) area = find_area("Maze-Level2");
    else if (victim->tot_level <= (2*LEVEL_HERO)/3) area = find_area("Maze-Level3");
    else if (victim->tot_level <= (3*LEVEL_HERO)/4) area = find_area("Maze-Level4");
    else area = find_area("Maze-Level5");

    if (!area) {
        pbugf(LOG_ERROR, "No area for exorcism!");
        return false;
    }

    do
        room = get_room_index(area, number_range(area->min_vnum, area->max_vnum));
    while (!room);

    chance = (ch->tot_level - victim->tot_level) + (catalyst / lvl);

    // If victim is more than 20 levels above char, it won't work
    if (chance < 20) {
        act("$N is too powerful for you to banish.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("You resist the power of $n's exorcism.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        return false;
    }

    chance = URANGE(1, 3 * abs(chance), 100);
    if (number_percent() < chance) {
        send_to_char("{RYou are sucked into the phantasmal pit!{x\n\r", victim);
        act("{R$n is sucked into the phantasmal pit!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        char_from_room(victim);
        char_to_room(victim, room);
        do_function(victim, &do_look, "auto");
    } else {
        act("$N resists the power of your exorcism.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        send_to_char("You feel a slight disturbance in the air.\n\r", victim);
    }
    return true;
}

SPELL_FUNC(spell_glorious_bolt)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    int dam;

    act("{YYou call forth a glorious bolt of fury upon $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{Y$n calls forth a glorious bolt of fury upon $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{Y$n calls forth a glorious bolt of fury upon you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);

    if (check_shield_block_projectile(ch, victim, "glorious bolt", NULL))
        return false;

    if (victim->alignment >= 0) {
        act("{C$N appears unaffected by your glorious bolt.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{CYou are unaffected by $n's glorious bolt.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("{C$N appears unaffected by $n's glorious bolt.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        return true;
    }

    dam = 2 * dice(level, 10);
    dam += dam * (victim->alignment / -1000);
    damage(ch, victim, dam, sn, DAM_HOLY ,true);
    return true;
}



SPELL_FUNC(spell_holy_shield)
{
    int sn = skill->uid;
    OBJ_DATA *obj;
    AFFECT_DATA af;
    bool perm = false;
    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    /* deal with the object case first */
    if (target == TARGET_OBJ) {
        obj = (OBJ_DATA *) vo;

        if (!CAN_WEAR(obj, ITEM_WEAR_SHIELD)) {
            act("$p is not affected.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return false;
        }

        if (IS_OBJ_STAT(obj,ITEM_HOLY)) {
            act("$p is already enchanted!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return false;
        }

        if (IS_OBJ_STAT(obj,ITEM_EVIL)) {
            act("The evil of $p is too powerful for you to enchant.", ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return false;
        }

        af.slot = obj_wear_loc;
        af.where = TO_OBJECT;
        af.group = AFFGROUP_ENCHANT;
        af.type	= sn;
    af.skill = skill;
        af.level = level;
        af.duration = perm ? -1 : (6 + level);
        af.location = APPLY_AC;
        af.modifier = -(level/8);
        af.bitvector = ITEM_HOLY;
        af.bitvector2 = 0;
        affect_to_obj(obj,&af);

        act("Fiery red runes glow brightly on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL, NULL, NULL);
        return true;
    }
    return false;
}


SPELL_FUNC(spell_holy_sword)
{
    int sn = skill->uid;
    OBJ_DATA *obj;
    AFFECT_DATA af;
    bool perm = false;
    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    if (target == TARGET_OBJ) {
        obj = (OBJ_DATA *) vo;

        if (obj->item_type != ITEM_WEAPON || WEAPON(obj)->weapon_class != WEAPON_SWORD) {
            act("$p is not affected.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return false;
        }

        if (IS_OBJ_STAT(obj,ITEM_HOLY)) {
            act("$p is already enchanted!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return false;
        }

        if (IS_OBJ_STAT(obj,ITEM_EVIL)) {
            act("The evil of $p is too powerful for you to overcome.", ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return false;
        }

        af.slot = obj_wear_loc;
        af.where = TO_OBJECT;
        af.group = AFFGROUP_ENCHANT;
        af.type	= sn;
    af.skill = skill;
        af.level = level;
        af.duration = perm ? -1 : (6 + level);
        af.location = 0;
        af.modifier = 0;
        af.bitvector = ITEM_HOLY;
        af.bitvector2 = 0;
        affect_to_obj(obj,&af);

        act("Fiery red runes glow brightly on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL, NULL, NULL);
        return true;
    }
    return false;
}


SPELL_FUNC(spell_holy_word)
{
    int sn = skill->uid;
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;
    int dam;
    int bless_num, curse_num, frenzy_num;

    bless_num = skill_lookup("bless");
    curse_num = skill_lookup("curse");
    frenzy_num = skill_lookup("frenzy");

    act("{W$n utters a word of divine power!{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
    send_to_char("You utter a word of divine power.\n\r",ch);

    for (vch = ch->in_room->people; vch; vch = vch_next) {
        vch_next = vch->next_in_room;

        if ((IS_GOOD(ch) && IS_GOOD(vch)) ||
            (IS_EVIL(ch) && IS_EVIL(vch)) ||
            (IS_NEUTRAL(ch) && IS_NEUTRAL(vch))) {
            send_to_char("You feel full of holy power!\n\r",vch);
            spell_frenzy(skill_from_sn(frenzy_num),level,ch,(void *) vch,TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
            spell_bless(skill_from_sn(bless_num),level,ch,(void *) vch,TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
        } else if ((IS_GOOD(ch) && IS_EVIL(vch)) || (IS_EVIL(ch) && IS_GOOD(vch))) {
            if (!is_safe_spell(ch,vch,true)) {
                spell_curse(skill_from_sn(curse_num),level,ch,(void *) vch,TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                send_to_char("{YYou are struck down!{x\n\r",vch);
                dam = dice(level,8);
                damage(ch,vch,dam,sn,DAM_HOLY,true);
            }
        } else if (IS_NEUTRAL(ch)) {
            if (!is_safe_spell(ch,vch,true)) {
                spell_curse(skill_from_sn(curse_num),level/2,ch,(void *) vch,TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
                send_to_char("{YYou are struck down!{x\n\r",vch);
                dam = dice(level,5);
                damage(ch,vch,dam,sn,DAM_MAGIC,true);
            }
        }
    }

    //  send_to_char("You feel drained.\n\r",ch);
    //  ch->move = 0;
    //  ch->hit /= 2;
    return true;
}


SPELL_FUNC(spell_light_shroud)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    AFFECT_DATA af;
    bool perm = false;
    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    if (perm && is_affected(victim, sn)) {
        affect_strip(victim, sn);
    } else if (IS_AFFECTED2(victim, AFF2_LIGHT_SHROUD)) {
        if (victim == ch)
            send_to_char("You are already protected in a shroud.\n\r",ch);
        else
            act("$N is already protected by a shroud.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);

        return false;
    }

    af.slot = obj_wear_loc;
    af.where = TO_AFFECTS;
    af.group = AFFGROUP_DIVINE;
    af.type = sn;
    af.skill = skill;
    af.level = level;
    af.duration = perm ? -1 : (3 + level/5);
    af.location = APPLY_NONE;
    af.modifier = 0;
    af.bitvector = 0;
    af.bitvector2 = AFF2_LIGHT_SHROUD;
    affect_to_char(victim, &af);
    act("{W$n is surrounded by a light shroud.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    send_to_char("{WYou are surrounded by a light shroud.{x\n\r", victim);
    return true;
}


SPELL_FUNC(spell_remove_curse)
{
    int sn = skill->uid;
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    bool found = false;
    ITERATOR it;

    /* do object cases first */
    if (target == TARGET_OBJ) {
        obj = (OBJ_DATA *) vo;

        if (IS_OBJ_STAT(obj,ITEM_NODROP) || IS_OBJ_STAT(obj,ITEM_NOREMOVE) || IS_OBJ_STAT(obj, ITEM_EVIL)) {
            if (!IS_OBJ_STAT(obj,ITEM_NOUNCURSE) && !saves_dispel(ch, NULL, obj->level)) {
                AFFECT_DATA *paf;

                REMOVE_BIT(obj->extra[0],ITEM_NODROP);
                REMOVE_BIT(obj->extra[0],ITEM_NOREMOVE);

                paf = affect_find(obj->affected,skill_resolve_gsn("curse"));
                if (!saves_dispel(ch, NULL, paf ? paf->level : obj->level)) {
                    if (paf) affect_remove_obj(obj,paf);
                    REMOVE_BIT(obj->extra[0],ITEM_EVIL);
                }

                act("$p glows blue.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL, NULL, NULL);
            } else
                act("The curse on $p is beyond your power.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return true;
        } else
            act("There doesn't seem to be a curse on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);

        return false;
    }

    /* characters */
    victim = (CHAR_DATA *) vo;

    if (check_dispel(ch,victim,skill_resolve_gsn("curse"))) {
        send_to_char("You feel better.\n\r",victim);
        act("$n looks more relaxed.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
    }

    // Check items in inventory using lcarrying
    if (victim->lcarrying) {
        iterator_start(&it, victim->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)) && !found) {
            if ((IS_OBJ_STAT(obj,ITEM_NODROP) || IS_OBJ_STAT(obj,ITEM_NOREMOVE)) && !IS_OBJ_STAT(obj,ITEM_NOUNCURSE)) {
                /* attempt to remove curse */
                if (!saves_dispel(ch,NULL,obj->level)) {
                    found = true;
                    REMOVE_BIT(obj->extra[0],ITEM_NODROP);
                    REMOVE_BIT(obj->extra[0],ITEM_NOREMOVE);
                    act("Your $p glows blue.",victim, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
                    act("$n's $p glows blue.",victim, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                }
            }
        }
        iterator_stop(&it);
    }

    // Also check worn equipment
    if (!found && victim->lworn) {
        iterator_start(&it, victim->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)) && !found) {
            if ((IS_OBJ_STAT(obj,ITEM_NODROP) || IS_OBJ_STAT(obj,ITEM_NOREMOVE)) && !IS_OBJ_STAT(obj,ITEM_NOUNCURSE)) {
                /* attempt to remove curse */
                if (!saves_dispel(ch,NULL,obj->level)) {
                    found = true;
                    REMOVE_BIT(obj->extra[0],ITEM_NODROP);
                    REMOVE_BIT(obj->extra[0],ITEM_NOREMOVE);
                    act("Your $p glows blue.",victim, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
                    act("$n's $p glows blue.",victim, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                }
            }
        }
        iterator_stop(&it);
    }

    return true;
}

SPELL_FUNC(spell_sanctuary)
{
    int sn = skill->uid;
    CHAR_DATA *victim = (CHAR_DATA *) vo;
    AFFECT_DATA af;
    int sk;
    bool perm = false;
    memset(&af,0,sizeof(af));

    if (level > MAGIC_WEAR_SPELL) {
        level -= MAGIC_WEAR_SPELL;
        perm = true;
    }

    if (perm && is_affected(victim, sn)) {
        affect_strip(victim, sn);
    } else if (IS_AFFECTED(victim, AFF_SANCTUARY)) {
        if (victim == ch)
            send_to_char("You are already in sanctuary.\n\r",ch);
        else
            act("$N is already in sanctuary.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);
        return false;
    }

    af.slot = obj_wear_loc;
    af.where = TO_AFFECTS;
    af.group = AFFGROUP_DIVINE;
    af.type = sn;
    af.skill = skill;
    af.level = level;

    sk = get_skill(ch,sn);
    sk = sk * sk / 100;	// 75% -> 56%, 85% -> 72%, 95% -> 90%
    // This ranges from 0% to 60% of level in a x^2 progression
    af.duration = perm ? -1 : ((3 * sk * level / 500) + 1);

    af.location  = APPLY_NONE;
    af.modifier  = 0;
    af.bitvector = AFF_SANCTUARY;
    af.bitvector2 = 0;
    affect_to_char(victim, &af);
    act("{W$n is surrounded by a white aura.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    send_to_char("{WYou are surrounded by a white aura.{x\n\r", victim);
    return true;
}
