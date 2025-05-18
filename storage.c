/***************************************************************************
 *                                                                         *
 *    Storage System - Unified storage for characters, accounts, and orgs  *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "merc.h"

// Forward declarations
void storage_character_cmd(CHAR_DATA *ch, char *arguments);
void storage_account_cmd(CHAR_DATA *ch, char *arguments);
void storage_church_cmd(CHAR_DATA *ch, char *arguments);
bool check_storage_access(CHAR_DATA *ch, int storage_type);
void show_storage_help(CHAR_DATA *ch, int storage_type);
bool handle_storage_rent(CHAR_DATA *ch, int storage_type);
bool can_access_church_storage(CHAR_DATA *ch, CHURCH_DATA *church);


/*
 * Main storage command processor
 * Usage: storage <type> <subcommand> [arguments]
 * Types: char/character/locker, account/vault, church/coffer
 */
void do_storage(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg_remainder[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg1);
    
    if (IS_NPC(ch)) {
        send_to_char("NPCs cannot use storage.\n\r", ch);
        return;
    }
    
    if (arg1[0] == '\0') {
        send_to_char("What type of storage do you want to access?\n\r", ch);
        send_to_char("Usage: storage <type> <command>\n\r", ch);
        send_to_char("Types: character/locker, account/vault, church/coffer\n\r", ch);
        return;
    }
    
    // Store remaining arguments
    strcpy(arg_remainder, argument);

    // Handle character storage (locker)
    if (!str_cmp(arg1, "character") || !str_cmp(arg1, "char") || !str_cmp(arg1, "locker")) {
        if (!game_settings.lockers_enabled && !IS_IMMORTAL(ch)) {
            send_to_char("Character storage is currently disabled.\n\r", ch);
            return;
        }
        storage_character_cmd(ch, arg_remainder);
        return;
    }
    
    // Handle account storage (vault)
    if (!str_cmp(arg1, "account") || !str_cmp(arg1, "vault")) {
        if (!game_settings.vault_enabled && !IS_IMMORTAL(ch)) {
            send_to_char("Account storage is currently disabled.\n\r", ch);
            return;
        }
        storage_account_cmd(ch, arg_remainder);
        return;
    }
    
    // Handle church storage (coffer)
    if (!str_cmp(arg1, "church") || !str_cmp(arg1, "coffer")) {
        if (!game_settings.coffer_enabled && !IS_IMMORTAL(ch)) {
            send_to_char("Church storage is currently disabled.\n\r", ch);
            return;
        }
        storage_church_cmd(ch, arg_remainder);
        return;
    }
    
    send_to_char("Invalid storage type. Use: character/locker, account/vault, or church/coffer.\n\r", ch);
}


/*
 * Handle character storage commands
 * Usage: storage character <command> [arguments]
 * Commands: list, put, get, info, rent, upgrade
 */
void storage_character_cmd(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MSL];
    OBJ_DATA *obj;
    bool item_with_locker_flag = false;
    struct tm *rent_time;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    // Check if player has a locker key item
    for (obj = ch->carrying; obj != NULL; obj = obj->next_content) {
        if (IS_SET(obj->extra[1], ITEM_LOCKER)) {
            item_with_locker_flag = true;
            break;
        }
    }

    // Determine if player can access locker here
    if (!IS_SET(ch->in_room->room_flag[0], ROOM_LOCKER) && !item_with_locker_flag) {
        send_to_char("You can't access your character storage here.\n\r", ch);
        return;
    }

    // Staff forgiveness command
    if (IS_IMMORTAL(ch) && IS_STAFF(ch, STAFF_SUPREMACY) && !str_cmp(arg1, "forgive")) {
        CHAR_DATA *player;

        if (arg2[0] == '\0') {
            send_to_char("Forgive whose locker rent?\n\r", ch);
            return;
        }

        player = get_char_world(ch, arg2);
        if (player == NULL || IS_NPC(player)) {
            send_to_char("That player is not online.\n\r", ch);
            return;
        }

        if (player->locker_rent == 0) {
            send_to_char("That player does not have a valid locker.\n\r", ch);
            return;
        }

        if (player->locker_rent > current_time) {
            send_to_char("That player's locker is currently active.\n\r", ch);
            return;
        }

        player->locker_rent = current_time;
        
        // Calculate new rent time (30 days)
        rent_time = (struct tm *)localtime(&player->locker_rent);
        rent_time->tm_mday += game_settings.locker_rent_time;
        player->locker_rent = (time_t)mktime(rent_time);

        act("Locker rent for $N has been forgiven.", ch, player, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
        send_to_char("{WYour character storage rent has been forgiven.{x\n\r", player);
        return;
    }

    // Show help if no command
    if (arg1[0] == '\0') {
        show_storage_help(ch, STORAGE_CHARACTER);
        return;
    }

    // Process rent command
    if (!str_cmp(arg1, "rent")) {
        int cost = game_settings.locker_rent_cost;
        
        // Calculate tiered costs if applicable
        if (ch->locker_tier > 0) {
            cost += (ch->locker_tier * game_settings.locker_additional_cost_per_tier);
        }
        
        if (!game_settings.locker_rent_enabled && !IS_IMMORTAL(ch)) {
            send_to_char("Character storage doesn't require rent payments.\n\r", ch);
            return;
        }

        if (ch->pcdata->bankbalance < cost) {
            sprintf(buf, "You need %d gold in your bank account to rent storage.\n\r", cost);
            send_to_char(buf, ch);
            return;
        }

        ch->pcdata->bankbalance -= cost;

        // Set rent time
        if (ch->locker_rent < current_time) {
            ch->locker_rent = current_time;
        }

        // Calculate new rent expiration
        rent_time = (struct tm *)localtime(&ch->locker_rent);
        rent_time->tm_mday += game_settings.locker_rent_time;
        
        // Cap to max rent time if configured
        time_t max_time = current_time;
        struct tm *max_rent = (struct tm *)localtime(&max_time);
        max_rent->tm_mday += game_settings.locker_rent_time_max;
        time_t max_timestamp = mktime(max_rent);
        
        ch->locker_rent = (time_t)mktime(rent_time);
        
        // Cap to maximum allowed rent time
        if (game_settings.locker_rent_time_max > 0 && 
            ch->locker_rent > max_timestamp) {
            ch->locker_rent = max_timestamp;
        }
        
        send_to_char("Storage rent extended to:\n\r", ch);
        send_to_char((char *)ctime(&ch->locker_rent), ch);
        return;
    }

    // Process upgrade command
    if (!str_cmp(arg1, "upgrade")) {
        int upgrade_cost = game_settings.locker_rent_cost * 5;
        
        if (ch->locker_tier >= game_settings.locker_tier_max) {
            send_to_char("Your character storage is already at maximum capacity.\n\r", ch);
            return;
        }
        
        if (ch->pcdata->bankbalance < upgrade_cost) {
            sprintf(buf, "You need %d gold in your bank account to upgrade storage.\n\r", upgrade_cost);
            send_to_char(buf, ch);
            return;
        }
        
        ch->pcdata->bankbalance -= upgrade_cost;
        ch->locker_tier++;
        
        sprintf(buf, "You've upgraded your character storage to tier %d!\n\r", ch->locker_tier);
        send_to_char(buf, ch);
        sprintf(buf, "New capacity: %d items, %d weight\n\r", 
                game_settings.max_locker_items + (ch->locker_tier * game_settings.locker_additional_slots_per_tier),
                game_settings.max_locker_weight + (ch->locker_tier * game_settings.locker_additional_weight_per_tier));
        send_to_char(buf, ch);
        return;
    }

    // Process info command
    if (!str_cmp(arg1, "info")) {
        if (ch->locker_rent == 0) {
            send_to_char("You have not rented character storage.\n\r", ch);
            return;
        }
        
        int current_weight = 0;
        int item_count = 0;
        OBJ_DATA *obj;
        
        for (obj = ch->locker; obj != NULL; obj = obj->next_content) {
            item_count++;
            current_weight += get_obj_weight(obj);
        }
        
        int max_items = game_settings.max_locker_items + 
                       (ch->locker_tier * game_settings.locker_additional_slots_per_tier);
        int max_weight = game_settings.max_locker_weight + 
                        (ch->locker_tier * game_settings.locker_additional_weight_per_tier);
        
        send_to_char("{WCharacter Storage Information:{x\n\r", ch);
        sprintf(buf, "Tier: %d/%d\n\r", ch->locker_tier, game_settings.locker_tier_max);
        send_to_char(buf, ch);
        sprintf(buf, "Items: %d/%d\n\r", item_count, max_items);
        send_to_char(buf, ch);
        sprintf(buf, "Weight: %d/%d\n\r", current_weight, max_weight);
        send_to_char(buf, ch);
        
        if (game_settings.locker_rent_enabled) {
            send_to_char("Rent paid until:\n\r", ch);
            send_to_char((char *)ctime(&ch->locker_rent), ch);
            
            if (current_time > ch->locker_rent) {
                send_to_char("{RYour storage rent has expired!{x\n\r", ch);
            } else {
                // Calculate days left
                int days_left = (ch->locker_rent - current_time) / 86400;
                sprintf(buf, "Days remaining: %d\n\r", days_left);
                send_to_char(buf, ch);
            }
        } else {
            send_to_char("Storage rental is not required on this realm.\n\r", ch);
        }
        
        return;
    }

    // Check if rent has expired and rent is required
    if (game_settings.locker_rent_enabled && current_time > ch->locker_rent) {
        send_to_char("Your character storage has expired. Please use 'storage character rent' to renew it.\n\r", ch);
        return;
    }

    // Process list command
    if (!str_cmp(arg1, "list")) {
        int item_count = 0;

        for (obj = ch->locker; obj != NULL; obj = obj->next_content)
            item_count++;

        sprintf(buf, "You look in your character storage and see %d items:\n\r", item_count);
        send_to_char(buf, ch);
        show_list_to_char(ch->locker, ch, true, true);

        act("$n looks over the contents of $s storage container.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        return;
    }

    // Commands that require a target item
    if (arg2[0] == '\0') {
        send_to_char("What item do you want to store or retrieve?\n\r", ch);
        return;
    }

    // Process store/put command
    if (!str_cmp(arg1, "store") || !str_cmp(arg1, "put")) {
        if ((obj = get_obj_carry(ch, arg2, ch)) == NULL) {
            send_to_char("You do not have that item.\n\r", ch);
            return;
        }

        // Check storage restrictions
        if ((obj->pIndexData == obj_index_skull || obj->pIndexData == obj_index_gold_skull) && obj->affected != NULL) {
            send_to_char("You can't store that enchanted item in your storage.\n\r", ch);
            return;
        }

        if (obj->timer > 0) {
            send_to_char("You can only store permanent items in your storage.\n\r", ch);
            return;
        }

        if (obj->item_type == ITEM_CONTAINER && obj->contains) {
            send_to_char("You can't put containers in your storage unless they are empty.\n\r", ch);
            return;
        }

        // Check capacity limits
        int current_items = count_char_locker(ch);
        int max_items = game_settings.max_locker_items + 
                       (ch->locker_tier * game_settings.locker_additional_slots_per_tier);
        
        if (current_items >= max_items) {
            send_to_char("Your storage is full!\n\r", ch);
            return;
        }
        
        // Check weight limits
        int current_weight = 0;
        for (OBJ_DATA *locker_obj = ch->locker; locker_obj != NULL; locker_obj = locker_obj->next_content) {
            current_weight += get_obj_weight(locker_obj);
        }
        
        int max_weight = game_settings.max_locker_weight + 
                        (ch->locker_tier * game_settings.locker_additional_weight_per_tier);
        
        if (current_weight + get_obj_weight(obj) > max_weight) {
            send_to_char("Your storage can't hold that much weight!\n\r", ch);
            return;
        }

        // Check other restrictions
        if (IS_SET(obj->extra[1], ITEM_NOLOCKER) || obj_nest_clones(obj) > 0) {
            send_to_char("You can't put that item in your storage.\n\r", ch);
            return;
        }

        act("You place $p in your storage.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n places $p in $s storage.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_char(obj);
        obj_to_locker(obj, ch);
        return;
    }

    // Process get command
    if (!str_cmp(arg1, "get")) {
        if ((obj = get_obj_locker(ch, arg2)) == NULL) {
            send_to_char("That item isn't in your storage.\n\r", ch);
            return;
        }

        act("You get $p from your storage.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n gets $p from $s storage.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_locker(obj);
        obj_to_char(obj, ch);
        return;
    }

    // If we get here, show help
    show_storage_help(ch, STORAGE_CHARACTER);
}


/*
 * Handle account storage commands
 * Usage: storage account <command> [arguments]
 * Commands: list, put, get, info, rent
 */
void storage_account_cmd(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MSL];
    OBJ_DATA *obj;
    struct tm *rent_time;
    ACCOUNT_DATA *account;
    bool loaded = false;
    
    if (ch->pcdata == NULL || ch->pcdata->account_name[0] == '\0') {
        send_to_char("You don't have an account to access vault storage.\n\r", ch);
        return;
    }
    
    // Get the character's account
    account = get_account_by_name(ch->pcdata->account_name);
    if (!account) {
        // Try to load the account
        account = get_account_online_or_offline(ch->pcdata->account_name, &loaded);
        
        if (!account) {
            send_to_char("Your account information couldn't be accessed.\n\r", ch);
            return;
        }
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    // Verify player can access vault here
    if (game_settings.vault_require_room)
    {
        if (!IS_SET(ch->in_room->room_flag[0], ROOM_VAULT)) {
            send_to_char("You need to be at a vault to access account storage.\n\r", ch);
            return;
        }
    }

    // Show help if no command
    if (arg1[0] == '\0') {
        show_storage_help(ch, STORAGE_ACCOUNT);
        if (loaded && account) free_account(account);
        return;
    }

    // Staff forgiveness command
    if (IS_IMMORTAL(ch) && IS_STAFF(ch, STAFF_SUPREMACY) && !str_cmp(arg1, "forgive")) {
        ACCOUNT_DATA *target_account;
        bool target_loaded = false;
        
        if (arg2[0] == '\0') {
            send_to_char("Forgive which account's vault rent?\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        target_account = get_account_by_name(arg2);
        if (!target_account) {
            target_account = get_account_online_or_offline(arg2, &target_loaded);
        }
        
        if (!target_account) {
            send_to_char("That account doesn't exist.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        if (target_account->vault_rent == 0) {
            send_to_char("That account does not have vault storage.\n\r", ch);
            if (target_loaded && target_account) free_account(target_account);
            if (loaded && account) free_account(account);
            return;
        }

        if (target_account->vault_rent > current_time) {
            send_to_char("That account's vault is currently active.\n\r", ch);
            if (target_loaded && target_account) free_account(target_account);
            if (loaded && account) free_account(account);
            return;
        }

        target_account->vault_rent = current_time;
        
        // Calculate new rent time
        rent_time = (struct tm *)localtime(&target_account->vault_rent);
        rent_time->tm_mday += game_settings.vault_rent_time;
        target_account->vault_rent = (time_t)mktime(rent_time);

        sprintf(buf, "Vault rent for account '%s' has been forgiven.\n\r", target_account->username);
        send_to_char(buf, ch);
        
        // Save the account
        save_account(target_account);
        
        // Notify any online players from this account
        DESCRIPTOR_DATA *d;
        for (d = descriptor_list; d != NULL; d = d->next) {
            if (d->character && !IS_NPC(d->character) && 
                d->character->pcdata && 
                !str_cmp(d->character->pcdata->account_name, target_account->username)) {
                send_to_char("{WYour account vault rent has been forgiven.{x\n\r", d->character);
            }
        }
        
        if (target_loaded && target_account) free_account(target_account);
        if (loaded && account) free_account(account);
        return;
    }

    // Process rent command
    if (!str_cmp(arg1, "rent")) {
        if (!game_settings.vault_rent && !IS_IMMORTAL(ch)) {
            send_to_char("Account vault storage doesn't require rent payments.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        int cost = game_settings.vault_rent_cost;
        
        // Calculate cost based on characters if that setting is enabled
        if (game_settings.vault_rent_per_char) {
            cost += (account->character_count * game_settings.vault_additional_cost_per_char);
        }
        
        if (ch->pcdata->bankbalance < cost) {
            sprintf(buf, "You need %d gold in your bank account to rent vault storage.\n\r", cost);
            send_to_char(buf, ch);
            if (loaded && account) free_account(account);
            return;
        }

        ch->pcdata->bankbalance -= cost;

        // Set rent time
        if (account->vault_rent < current_time) {
            account->vault_rent = current_time;
        }

        // Calculate new rent expiration
        rent_time = (struct tm *)localtime(&account->vault_rent);
        rent_time->tm_mday += game_settings.vault_rent_time;
        account->vault_rent = (time_t)mktime(rent_time);
        
        send_to_char("Vault storage rent extended to:\n\r", ch);
        send_to_char((char *)ctime(&account->vault_rent), ch);
        
        // Save the account
        save_account(account);
        
        if (loaded && account) free_account(account);
        return;
    }

    // Process info command
    if (!str_cmp(arg1, "info")) {
        if (account->vault_rent == 0) {
            send_to_char("Your account does not have vault storage.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }
        
        int current_weight = 0;
        int item_count = 0;
        OBJ_DATA *obj;
        
        // Calculate current usage
        for (obj = account->vault_items; obj != NULL; obj = obj->next_content) {
            item_count++;
            current_weight += get_obj_weight(obj);
        }
        
        // Calculate max capacity
        int max_items = game_settings.max_vault_items;
        int max_weight = game_settings.max_vault_weight;
        
        if (game_settings.vault_additional_slots_per_char > 0) {
            max_items += (account->character_count * game_settings.vault_additional_slots_per_char);
        }
        
        if (game_settings.vault_additional_weight_per_char > 0) {
            max_weight += (account->character_count * game_settings.vault_additional_weight_per_char);
        }
        
        send_to_char("{WAccount Vault Information:{x\n\r", ch);
        sprintf(buf, "Account: %s\n\r", account->username);
        send_to_char(buf, ch);
        sprintf(buf, "Characters: %d\n\r", account->character_count);
        send_to_char(buf, ch);
        sprintf(buf, "Items: %d/%d\n\r", item_count, max_items);
        send_to_char(buf, ch);
        sprintf(buf, "Weight: %d/%d\n\r", current_weight, max_weight);
        send_to_char(buf, ch);
        
        if (game_settings.vault_rent) {
            send_to_char("Rent paid until:\n\r", ch);
            send_to_char((char *)ctime(&account->vault_rent), ch);
            
            if (current_time > account->vault_rent) {
                send_to_char("{RYour vault storage rent has expired!{x\n\r", ch);
            } else {
                // Calculate days left
                int days_left = (account->vault_rent - current_time) / 86400;
                sprintf(buf, "Days remaining: %d\n\r", days_left);
                send_to_char(buf, ch);
            }
        } else {
            send_to_char("Vault storage rental is not required on this realm.\n\r", ch);
        }
        
        if (loaded && account) free_account(account);
        return;
    }

    // Check if rent has expired and rent is required
    if (game_settings.vault_rent && current_time > account->vault_rent) {
        send_to_char("Your account vault storage has expired. Please use 'storage account rent' to renew it.\n\r", ch);
        if (loaded && account) free_account(account);
        return;
    }

    // Process list command
    if (!str_cmp(arg1, "list")) {
        int item_count = 0;

        for (obj = account->vault_items; obj != NULL; obj = obj->next_content)
            item_count++;

        sprintf(buf, "You look in your account vault and see %d items:\n\r", item_count);
        send_to_char(buf, ch);
        show_list_to_char(account->vault_items, ch, true, true);

        act("$n looks through $s account vault.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        
        if (loaded && account) free_account(account);
        return;
    }

    // Commands that require a target item
    if (arg2[0] == '\0') {
        send_to_char("What item do you want to store or retrieve?\n\r", ch);
        if (loaded && account) free_account(account);
        return;
    }

    // Process store/put command
    if (!str_cmp(arg1, "store") || !str_cmp(arg1, "put")) {
        if ((obj = get_obj_carry(ch, arg2, ch)) == NULL) {
            send_to_char("You do not have that item.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        // Check storage restrictions
        if ((obj->pIndexData == obj_index_skull || obj->pIndexData == obj_index_gold_skull) && obj->affected != NULL) {
            send_to_char("You can't store that enchanted item in your vault.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        if (obj->timer > 0) {
            send_to_char("You can only store permanent items in your vault.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        if (obj->item_type == ITEM_CONTAINER && obj->contains) {
            send_to_char("You can't put containers in your vault unless they are empty.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        // Check capacity limits
        int current_items = 0;
        int current_weight = 0;
        for (OBJ_DATA *vault_obj = account->vault_items; vault_obj != NULL; vault_obj = vault_obj->next_content) {
            current_items++;
            current_weight += get_obj_weight(vault_obj);
        }
        
        // Calculate max capacity
        int max_items = game_settings.max_vault_items;
        int max_weight = game_settings.max_vault_weight;
        
        if (game_settings.vault_additional_slots_per_char > 0) {
            max_items += (account->character_count * game_settings.vault_additional_slots_per_char);
        }
        
        if (game_settings.vault_additional_weight_per_char > 0) {
            max_weight += (account->character_count * game_settings.vault_additional_weight_per_char);
        }
        
        if (current_items >= max_items) {
            send_to_char("Your vault storage is full!\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }
        
        if (current_weight + get_obj_weight(obj) > max_weight) {
            send_to_char("Your vault storage can't hold that much weight!\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        // Check other restrictions
        if (IS_SET(obj->extra[1], ITEM_NOLOCKER) || obj_nest_clones(obj) > 0) {
            send_to_char("You can't put that item in your vault.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        act("You place $p in your account vault.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n places $p in $s account vault.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_char(obj);
        obj_to_vault(obj, account);
        
        // Save the account
        save_account(account);
        
        if (loaded && account) free_account(account);
        return;
    }

    // Process get command
    if (!str_cmp(arg1, "get")) {
        if ((obj = get_obj_vault(account, arg2)) == NULL) {
            send_to_char("That item isn't in your vault.\n\r", ch);
            if (loaded && account) free_account(account);
            return;
        }

        act("You get $p from your account vault.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n gets $p from $s account vault.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_vault(obj, account);
        obj_to_char(obj, ch);
        
        // Save the account
        save_account(account);
        
        if (loaded && account) free_account(account);
        return;
    }

    // If we get here, show help
    show_storage_help(ch, STORAGE_ACCOUNT);
    if (loaded && account) free_account(account);
}


/*
 * Handle church storage commands
 * Usage: storage church <command> [arguments]
 * Commands: list, put, get, info, rent
 */
void storage_church_cmd(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MSL];
    OBJ_DATA *obj;
    struct tm *rent_time;
    CHURCH_DATA *church;
    
    if (ch->church == NULL) {
        send_to_char("You are not a member of any church.\n\r", ch);
        return;
    }
    
    church = ch->church;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    // Verify player can access coffer here
    if (ch->in_room && is_in_treasure_room(ch->in_room)) {
            send_to_char("You need to be in a church treasure room to access church storage.\n\r", ch);
        return;
    }

    // Check permissions for basic access
    if (!can_access_church_storage(ch, church)) {
        send_to_char("You do not have permission to access the church storage.\n\r", ch);
        return;
    }

    // Show help if no command
    if (arg1[0] == '\0') {
        show_storage_help(ch, STORAGE_CHURCH);
        return;
    }

    // Staff forgiveness command
    if (IS_IMMORTAL(ch) && IS_STAFF(ch, STAFF_SUPREMACY) && !str_cmp(arg1, "forgive")) {
        CHURCH_DATA *target_church;
        
        if (arg2[0] == '\0') {
            send_to_char("Forgive which church's coffer rent?\n\r", ch);
            return;
        }

        target_church = find_church(atoi(arg2));
        if (!target_church) {
            send_to_char("That church doesn't exist.\n\r", ch);
            return;
        }

        if (target_church->coffer_rent == 0) {
            send_to_char("That church does not have coffer storage.\n\r", ch);
            return;
        }

        if (target_church->coffer_rent > current_time) {
            send_to_char("That church's coffer is currently active.\n\r", ch);
            return;
        }

        target_church->coffer_rent = current_time;
        
        // Calculate new rent time
        rent_time = (struct tm *)localtime(&target_church->coffer_rent);
        rent_time->tm_mday += game_settings.coffer_rent_period;
        target_church->coffer_rent = (time_t)mktime(rent_time);

        sprintf(buf, "Coffer rent for church '%s' has been forgiven.\n\r", target_church->name);
        send_to_char(buf, ch);
        
        // Save the church
        save_church(target_church);
        
        // Notify church leaders
        for (DESCRIPTOR_DATA *d = descriptor_list; d != NULL; d = d->next) {
            if (d->character && !IS_NPC(d->character) && 
                d->character->church == target_church &&
                is_church_leader(d->character, d->character->church)) {
                send_to_char("{WYour church coffer rent has been forgiven.{x\n\r", d->character);
            }
        }
        
        return;
    }

// Process rent command
    if (!str_cmp(arg1, "rent")) {
        if (!game_settings.coffer_rent && !IS_IMMORTAL(ch)) {
            send_to_char("Church coffer storage doesn't require rent payments.\n\r", ch);
            return;
        }

        // Can only pay rent if you have finances permission or storage management
        if (!has_church_permission(ch->church_member, CHURCH_PERM_FINANCES) && 
            !has_church_permission(ch->church_member, CHURCH_PERM_STORAGE)) {
            send_to_char("Only church members with finance or storage management permissions can pay coffer rent.\n\r", ch);
            return;
        }
    
    int cost = game_settings.coffer_rent_cost;
    
    // Check if church has enough resources based on currency type
    if (!str_cmp(game_settings.coffer_rent_currency, "pneuma")) {
        // Pneuma payment
        if (church->pneuma < cost) {
            sprintf(buf, "Your church needs %d pneuma in its treasury to pay rent.\n\r", cost);
            send_to_char(buf, ch);
            return;
        }
        
        church->pneuma -= cost;
        sprintf(buf, "%d pneuma has been withdrawn from the church treasury for rent.\n\r", cost);
        send_to_char(buf, ch);
    } 
    else if (!str_cmp(game_settings.coffer_rent_currency, "dp") || 
             !str_cmp(game_settings.coffer_rent_currency, "deitypoints")) {
        // Deity Points payment
        if (church->dp < cost) {
            sprintf(buf, "Your church needs %d deity points in its treasury to pay rent.\n\r", cost);
            send_to_char(buf, ch);
            return;
        }
        
        church->dp -= cost;
        sprintf(buf, "%d deity points have been withdrawn from the church treasury for rent.\n\r", cost);
        send_to_char(buf, ch);
    }
    else if (!str_cmp(game_settings.coffer_rent_currency, "gold")) {
        // Gold payment
        if (church->gold < cost) {
            sprintf(buf, "Your church needs %d gold in its treasury to pay rent.\n\r", cost);
            send_to_char(buf, ch);
            return;
        }
        
        church->gold -= cost;
        sprintf(buf, "%d gold has been withdrawn from the church treasury for rent.\n\r", cost);
        send_to_char(buf, ch);
    } 
    else {
        // Unknown currency
        sprintf(buf, "Unknown rent currency type '%s'. Please report this to an immortal.\n\r", 
                game_settings.coffer_rent_currency);
        send_to_char(buf, ch);
        return;
    }

    // Set rent time
    if (church->coffer_rent < current_time) {
        church->coffer_rent = current_time;
    }

    // Calculate new rent expiration
    rent_time = (struct tm *)localtime(&church->coffer_rent);
    rent_time->tm_mday += game_settings.coffer_rent_period;
    church->coffer_rent = (time_t)mktime(rent_time);
    
    send_to_char("Church coffer storage rent extended to:\n\r", ch);
    send_to_char((char *)ctime(&church->coffer_rent), ch);
    
    // Log the payment in church log
        sprintf(buf, "%s paid %d %s for coffer rent.", 
                ch->name, cost, game_settings.coffer_rent_currency);
        append_church_log(church, buf);
        
        // Save the church
        save_church(church);
        return;
    }

    // Process info command
    if (!str_cmp(arg1, "info")) {
        if (church->coffer_rent == 0 && game_settings.coffer_rent) {
            send_to_char("Your church does not have coffer storage.\n\r", ch);
            return;
        }
        
        int current_weight = 0;
        int item_count = 0;
        OBJ_DATA *obj;
        
        // Calculate current usage
        for (obj = church->coffer; obj != NULL; obj = obj->next_content) {
            item_count++;
            current_weight += get_obj_weight(obj);
        }
        
        int max_items = game_settings.max_coffer_items;
        int max_weight = game_settings.max_coffer_weight;
        
        send_to_char("{WChurch Coffer Information:{x\n\r", ch);
        sprintf(buf, "Church: %s\n\r", church->name);
        send_to_char(buf, ch);
        sprintf(buf, "Items: %d/%d\n\r", item_count, max_items);
        send_to_char(buf, ch);
        sprintf(buf, "Weight: %d/%d\n\r", current_weight, max_weight);
        send_to_char(buf, ch);
        
        if (game_settings.coffer_rent) {
            send_to_char("\n\rRent paid until:\n\r", ch);
            send_to_char((char *)ctime(&church->coffer_rent), ch);
            
            if (current_time > church->coffer_rent) {
                send_to_char("{RYour church coffer storage rent has expired!{x\n\r", ch);
            } else {
                // Calculate days left
                int days_left = (church->coffer_rent - current_time) / 86400;
                sprintf(buf, "Days remaining: %d\n\r", days_left);
                send_to_char(buf, ch);
            }
        } else {
            send_to_char("\n\rChurch coffer storage rental is not required on this realm.\n\r", ch);
        }
        
        return;
    }

    // Check if rent has expired and rent is required
    if (game_settings.coffer_rent && current_time > church->coffer_rent) {
        send_to_char("Your church coffer storage has expired. Please use 'storage church rent' to renew it.\n\r", ch);
        return;
    }

    // Process list command
    if (!str_cmp(arg1, "list")) {
        int item_count = 0;

        for (obj = church->coffer; obj != NULL; obj = obj->next_content)
            item_count++;

        sprintf(buf, "You look in your church coffer and see %d items:\n\r", item_count);
        send_to_char(buf, ch);
        show_list_to_char(church->coffer, ch, true, true);

        act("$n looks through the church coffer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        return;
    }

    // Commands that require a target item
    if (arg2[0] == '\0') {
        send_to_char("What item do you want to store or retrieve?\n\r", ch);
        return;
    }

    // Process store/put command
    if (!str_cmp(arg1, "store") || !str_cmp(arg1, "put")) {
        if (!can_put_to_church_storage(ch, church)) {
            send_to_char("You do not have permission to store items in the church coffer.\n\r", ch);
            return;
        }
        
        if ((obj = get_obj_carry(ch, arg2, ch)) == NULL) {
            send_to_char("You do not have that item.\n\r", ch);
            return;
        }

        if (obj->timer > 0) {
            send_to_char("You can only store permanent items in the church coffer.\n\r", ch);
            return;
        }

        if (obj->item_type == ITEM_CONTAINER && obj->contains) {
            send_to_char("You can't put containers in the church coffer unless they are empty.\n\r", ch);
            return;
        }

        // Check capacity limits
        int current_items = 0;
        int current_weight = 0;
        for (OBJ_DATA *coffer_obj = church->coffer; coffer_obj != NULL; coffer_obj = coffer_obj->next_content) {
            current_items++;
            current_weight += get_obj_weight(coffer_obj);
        }
        
        if (current_items >= game_settings.max_coffer_items) {
            send_to_char("The church coffer is full!\n\r", ch);
            return;
        }
        
        if (current_weight + get_obj_weight(obj) > game_settings.max_coffer_weight) {
            send_to_char("The church coffer can't hold that much weight!\n\r", ch);
            return;
        }

        // Check other restrictions
        if (IS_SET(obj->extra[1], ITEM_NOLOCKER) || obj_nest_clones(obj) > 0) {
            send_to_char("You can't put that item in the church coffer.\n\r", ch);
            return;
        }

        act("You place $p in the church coffer.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n places $p in the church coffer.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_char(obj);
        obj_to_coffer(obj, church);
        
        // Log the action
        sprintf(buf, "%s put %s into the church coffer.", 
                ch->name, obj->short_descr);
        append_church_log(church, buf);
        
        // Save the church data
        save_church(church);
        return;
    }

    if (!str_cmp(arg1, "get")) {
        if (!can_get_from_church_storage(ch, church)) {
            send_to_char("You do not have permission to retrieve items from the church coffer.\n\r", ch);
            return;
        }
        
        if ((obj = get_obj_coffer(church, arg2)) == NULL) {
            send_to_char("That item isn't in the church coffer.\n\r", ch);
            return;
        }

        act("You get $p from the church coffer.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
        act("$n gets $p from the church coffer.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        obj_from_coffer(obj, church);
        obj_to_char(obj, ch);
        
        // Log the action
        sprintf(buf, "%s took %s from the church coffer.", 
                ch->name, obj->short_descr);
        append_church_log(church, buf);
        
        // Save the church data
        save_church(church);
        return;
    }

    // If we get here, show help
    show_storage_help(ch, STORAGE_CHURCH);
}

/*
 * Show help for the specified storage type
 */
void show_storage_help(CHAR_DATA *ch, int storage_type)
{
    switch (storage_type) {
        case STORAGE_CHARACTER:
            send_to_char("Character Storage Commands:\n\r", ch);
            send_to_char("  storage character list          - List contents\n\r", ch);
            send_to_char("  storage character info          - Show storage details\n\r", ch);
            send_to_char("  storage character put <item>    - Store an item\n\r", ch);
            send_to_char("  storage character get <item>    - Retrieve an item\n\r", ch);
            if (game_settings.locker_rent_enabled) {
                send_to_char("  storage character rent          - Pay rent\n\r", ch);
            }
            send_to_char("  storage character upgrade       - Upgrade storage capacity\n\r", ch);
            break;
            
        case STORAGE_ACCOUNT:
            send_to_char("Account Vault Commands:\n\r", ch);
            send_to_char("  storage account list          - List contents\n\r", ch);
            send_to_char("  storage account info          - Show storage details\n\r", ch);
            send_to_char("  storage account put <item>    - Store an item\n\r", ch);
            send_to_char("  storage account get <item>    - Retrieve an item\n\r", ch);
            if (game_settings.vault_rent) {
                send_to_char("  storage account rent          - Pay rent\n\r", ch);
            }
            break;
            
    case STORAGE_CHURCH:
        send_to_char("Church Coffer Commands:\n\r", ch);
        send_to_char("  storage church list             - List contents\n\r", ch);
        send_to_char("  storage church info             - Show storage details\n\r", ch);
        
        // Only show put command if they have permission
        if (ch->church && can_put_to_church_storage(ch, ch->church)) {
            send_to_char("  storage church put <item>       - Store an item\n\r", ch);
        }
        
        // Only show get command if they have permission
        if (ch->church && can_get_from_church_storage(ch, ch->church)) {
            send_to_char("  storage church get <item>       - Retrieve an item\n\r", ch);
        }
        
        // Only show rent command if they have permission and rent is required
        if (game_settings.coffer_rent && ch->church && 
            (has_church_permission(ch->church_member, CHURCH_PERM_FINANCES) || 
             has_church_permission(ch->church_member, CHURCH_PERM_STORAGE))) {
            send_to_char("  storage church rent             - Pay rent\n\r", ch);
        }
            break;
            
        default:
            send_to_char("Invalid storage type.\n\r", ch);
    }
}

/*
 * Check if a player can access church storage
 */
bool can_access_church_storage(CHAR_DATA *ch, CHURCH_DATA *church)
{
    // Immortals can always access
    if (IS_IMMORTAL(ch)) return true;
    
    // Must be a member of the church
    if (ch->church != church) return false;
    
    // Can't access if excommunicated
    if (is_excommunicated(ch)) return false;
    
    // CHURCH_PERM_STORAGE allows full access to storage
    if (has_church_permission(ch->church_member, CHURCH_PERM_STORAGE))
        return true;
    
    // For list and info commands, any storage-related permission is enough
    if (has_church_permission(ch->church_member, CHURCH_PERM_GET_STORAGE) ||
        has_church_permission(ch->church_member, CHURCH_PERM_PUT_STORAGE))
        return true;
    
    return false;
}

/*
 * Object manipulation functions for vault storage
 */
void obj_to_vault(OBJ_DATA *obj, ACCOUNT_DATA *account)
{
    obj->next_content = account->vault_items;
    account->vault_items = obj;
}

OBJ_DATA *get_obj_vault(ACCOUNT_DATA *account, const char *argument)
{
    OBJ_DATA *obj;
    int number;
    int count = 0;
    char arg[MAX_INPUT_LENGTH];
    
    number = number_argument(argument, arg);
    
    for (obj = account->vault_items; obj != NULL; obj = obj->next_content) {
        if (can_see_obj(NULL, obj) && is_name(arg, obj->name)) {
            if (++count == number)
                return obj;
        }
    }
    
    return NULL;
}

void obj_from_vault(OBJ_DATA *obj, ACCOUNT_DATA *account)
{
    OBJ_DATA *prev;
    
    if (account->vault_items == obj) {
        account->vault_items = obj->next_content;
        return;
    }
    
    for (prev = account->vault_items; prev != NULL; prev = prev->next_content) {
        if (prev->next_content == obj) {
            prev->next_content = obj->next_content;
            return;
        }
    }
}

/*
 * Object manipulation functions for church coffer storage
 */
void obj_to_coffer(OBJ_DATA *obj, CHURCH_DATA *church)
{
    obj->next_content = church->coffer;
    church->coffer = obj;
}

OBJ_DATA *get_obj_coffer(CHURCH_DATA *church, const char *argument)
{
    OBJ_DATA *obj;
    int number;
    int count = 0;
    char arg[MAX_INPUT_LENGTH];
    
    number = number_argument(argument, arg);
    
    for (obj = church->coffer; obj != NULL; obj = obj->next_content) {
        if (can_see_obj(NULL, obj) && is_name(arg, obj->name)) {
            if (++count == number)
                return obj;
        }
    }
    
    return NULL;
}

void obj_from_coffer(OBJ_DATA *obj, CHURCH_DATA *church)
{
    OBJ_DATA *prev;
    
    if (church->coffer == obj) {
        church->coffer = obj->next_content;
        return;
    }
    
    for (prev = church->coffer; prev != NULL; prev = prev->next_content) {
        if (prev->next_content == obj) {
            prev->next_content = obj->next_content;
            return;
        }
    }
}

/*
 * Check if a player can retrieve items from church storage
 */
bool can_get_from_church_storage(CHAR_DATA *ch, CHURCH_DATA *church)
{
    if (IS_IMMORTAL(ch)) return true;
    if (ch->church != church) return false;
    if (is_excommunicated(ch)) return false;
    
    // CHURCH_PERM_STORAGE implies all storage permissions
    if (has_church_permission(ch->church_member, CHURCH_PERM_STORAGE))
        return true;
        
    // Specific permission for getting items
    if (has_church_permission(ch->church_member, CHURCH_PERM_GET_STORAGE))
        return true;
        
    return false;
}

/*
 * Check if a player can put items into church storage
 */
bool can_put_to_church_storage(CHAR_DATA *ch, CHURCH_DATA *church)
{
    if (IS_IMMORTAL(ch)) return true;
    if (ch->church != church) return false;
    if (is_excommunicated(ch)) return false;
    
    // CHURCH_PERM_STORAGE implies all storage permissions
    if (has_church_permission(ch->church_member, CHURCH_PERM_STORAGE))
        return true;
        
    // Specific permission for putting items
    if (has_church_permission(ch->church_member, CHURCH_PERM_PUT_STORAGE))
        return true;
        
    return false;
}

void obj_to_storage(OBJ_DATA *obj, CHAR_DATA *ch, int storage_type);
void obj_from_storage(OBJ_DATA *obj);
OBJ_DATA *get_storage_obj(CHAR_DATA *ch, int storage_type, char *argument);