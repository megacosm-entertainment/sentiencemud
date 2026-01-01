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
 * ROM 2.4 is copyright 1993-1998 Russ Taylor                              *
 * ROM has been brought to you by the ROM consortium                       *
 *   Russ Taylor (rtaylor@hypercube.org)                                   *
 *   Gabrielle Taylor (gtaylor@hypercube.org)                              *
 *   Brian Moore (zump@rom.org)                                            *
 * By using this code, you have agreed to follow the terms of the          *
 * ROM license, in the file Rom24/doc/rom.license                          *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "merc.h"
#include "olc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "olc_save.h"
#include "wilds.h"

extern void persist_save(void);
extern char *token_index_getvaluename(TOKEN_INDEX_DATA *token, int v);
extern void affect_fix_char(CHAR_DATA *ch);
extern bool newlock;
extern bool wizlock;
extern bool is_test_port;
void pstat_variable_list(BUFFER *buffer, pVARIABLE vars);
char *reboot_reason = NULL; // global





int gconfig_read (void)
{
    FILE *fp;
    bool fMatch;
    char *word;
    char buf[MIL];
    extern GLOBAL_DATA gconfig;

    log_string("Loading configuration settings from gconfig.rc...");

    fp = fopen(CONFIG_FILE,"r");
    if (!fp)
    {
        bug("act_wiz.c, gconfig_read(): Unable to open gconfig.rc file for reading.",0);
        return(1); /* Failure*/
    }

    gconfig.next_mob_uid[0] = 1;	gconfig.next_mob_uid[1] = 0;
    gconfig.next_obj_uid[0] = 1;	gconfig.next_obj_uid[1] = 0;
    gconfig.next_token_uid[0] = 1;	gconfig.next_token_uid[1] = 0;
    gconfig.next_vroom_uid[0] = 1;	gconfig.next_vroom_uid[1] = 0;
    gconfig.next_ship_uid[0] = 1;	gconfig.next_ship_uid[1] = 0;

    gconfig.next_church_uid = 1;
    gconfig.db_version = VERSION_DB_000;

	gconfig.email_port = 0;
	gconfig.email_username = "";
	gconfig.email_host = "";
	gconfig.email_password = "";
	gconfig.email_from_addr = "";
	gconfig.email_from_name = "";

	disconnect_timeout = 30;
	limbo_timeout = 12;

    for(;;)
    {
        word = feof (fp) ? "END" : fread_word(fp);
        fMatch = false;

        switch (UPPER(word[0]))
        {
            case '*':
                fMatch = true;
                fread_to_eol (fp);
            break;
			case 'D':
				KEY ("DBVersion", gconfig.db_version, fread_number(fp));
				KEY ("DisconnectTimeout", disconnect_timeout, fread_number(fp));
				break;

           case 'E':
		   		KEY ("EmailUser", gconfig.email_username, fread_string(fp));
				KEY ("EmailPassword", gconfig.email_password, fread_string(fp));
				KEY ("EmailHost", gconfig.email_host, fread_string(fp));
				KEY ("EmailPort", gconfig.email_port, fread_number(fp));
				KEY ("EmailFromAddr", gconfig.email_from_addr, fread_string(fp));
				KEY ("EmailFromName", gconfig.email_from_name, fread_string(fp));
                if (!str_cmp(word, "END"))
                {
					gconfig.next_mob_uid[3] = gconfig.next_mob_uid[1];
					gconfig.next_mob_uid[2] = gconfig.next_mob_uid[0] + UID_INC - (gconfig.next_mob_uid[0] & UID_MASK);
					if(!gconfig.next_mob_uid[2]) gconfig.next_mob_uid[3]++;

					gconfig.next_obj_uid[3] = gconfig.next_obj_uid[1];
					gconfig.next_obj_uid[2] = gconfig.next_obj_uid[0] + UID_INC - (gconfig.next_obj_uid[0] & UID_MASK);
					if(!gconfig.next_obj_uid[2]) gconfig.next_obj_uid[3]++;

					gconfig.next_token_uid[3] = gconfig.next_token_uid[1];
					gconfig.next_token_uid[2] = gconfig.next_token_uid[0] + UID_INC - (gconfig.next_token_uid[0] & UID_MASK);
					if(!gconfig.next_token_uid[2]) gconfig.next_token_uid[3]++;

					gconfig.next_vroom_uid[3] = gconfig.next_vroom_uid[1];
					gconfig.next_vroom_uid[2] = gconfig.next_vroom_uid[0] + UID_INC - (gconfig.next_vroom_uid[0] & UID_MASK);
					if(!gconfig.next_vroom_uid[2]) gconfig.next_vroom_uid[3]++;

					gconfig.next_ship_uid[3] = gconfig.next_ship_uid[1];
					gconfig.next_ship_uid[2] = gconfig.next_ship_uid[0] + UID_INC - (gconfig.next_ship_uid[0] & UID_MASK);
					if(!gconfig.next_ship_uid[2]) gconfig.next_ship_uid[3]++;


					if(!gconfig.next_church_uid) gconfig.next_church_uid++;

					if (disconnect_timeout <= 0)
						disconnect_timeout = 30;

					if (limbo_timeout <= 0)
						limbo_timeout = 12;
					
					if (disconnect_timeout <= limbo_timeout)
						disconnect_timeout = limbo_timeout + 5;
					
					fclose(fp);
					gconfig_write();
					return(0); /* Success*/
				}
	            break;

			case 'L':
				KEY("LimboTimeout", limbo_timeout, fread_number(fp));
				break;

            case 'N':
            	if(!str_cmp(word,"NextMobUID")) {
					gconfig.next_mob_uid[0] = fread_number(fp);
					gconfig.next_mob_uid[1] = fread_number(fp);
					fMatch = true;
					break;
				}
            	if(!str_cmp(word,"NextObjUID")) {
					gconfig.next_obj_uid[0] = fread_number(fp);
					gconfig.next_obj_uid[1] = fread_number(fp);
					fMatch = true;
					break;
				}
            	if(!str_cmp(word,"NextTokenUID")) {
					gconfig.next_token_uid[0] = fread_number(fp);
					gconfig.next_token_uid[1] = fread_number(fp);
					fMatch = true;
					break;
				}
            	if(!str_cmp(word,"NextVRoomUID")) {
					gconfig.next_vroom_uid[0] = fread_number(fp);
					gconfig.next_vroom_uid[1] = fread_number(fp);
					fMatch = true;
					break;
				}
            	if(!str_cmp(word,"NextShipUID")) {
					gconfig.next_ship_uid[0] = fread_number(fp);
					gconfig.next_ship_uid[1] = fread_number(fp);
					fMatch = true;
					break;
				}
                KEY ("NextAreaUID", gconfig.next_area_uid, fread_number(fp));
                KEY ("NextWildsUID", gconfig.next_wilds_uid, fread_number(fp));
                KEY ("NextVlinkUID", gconfig.next_vlink_uid, fread_number(fp));
                KEY ("NextChurchUID", gconfig.next_church_uid, fread_number(fp));

	            break;


        } /* end switch */

        if (!fMatch)
        {
	    sprintf(buf, "act_wiz.c, gconfig_read(): no match for '%s'!", word);
	    bug(buf, 0);
            fread_to_eol(fp);
        }
    } /* end for */
}

int game_settings_read (void)
{
    FILE *fp;
    bool fMatch;
    char *word;
    char buf[MIL];


    log_string("Loading configuration settings from game_settings.dat...");

    fp = fopen(GAME_SETTINGS_FILE,"r");
    if (!fp)
    {
        bug("act_wiz.c, gamesettings_read(): Unable to open game_settings.dat file for reading.",0);
        return(1); /* Failure*/
    }

   /* Basic settings */
    game_settings.game_name = "";
    game_settings.login_string = "";
    game_settings.server_description = "";
    game_settings.testport = false;
    game_settings.dev_server = false;
    game_settings.wizlock = false;
    game_settings.new_acct_lock = false;
    game_settings.new_char_lock = false;
    game_settings.wizlock_msg = "";
    game_settings.new_acct_lock_msg = "";
    game_settings.new_char_lock_msg = "";
    game_settings.logall = false;
	game_settings.note_boot_errors = false;

    /* Auth */
    game_settings.require_uniq_pass_staff = false;
    game_settings.max_login_attempts = 0;
    game_settings.enable_passwd = true;
    game_settings.enable_mfa = true;
    game_settings.require_email_verif = false;

    /* 2FA */
    game_settings.require_2fa_all = false;
    game_settings.require_2fa_staff = false;
    
    /* Multiplaying & Linking */
    game_settings.allow_mp_acct_all = false;
    game_settings.allow_mp_acct_staff = false;
    game_settings.allow_mp_host_all = false;
    game_settings.allow_mp_host_staff = false;
    game_settings.allow_link_all = false;
    game_settings.allow_unlink_all = false;

    /* Game Systems */
    game_settings.alignment_system = false;
    game_settings.restrict_races_align = false;
    game_settings.restrict_classes_align = false;

    /* Timers */
    game_settings.idle_time = 0;
    game_settings.idle_disconnect_time = 0;

    /* Misc Maximums */
    game_settings.max_alias = 0;
    game_settings.max_characters = 0;
    game_settings.max_orgs = 0;
    game_settings.max_logfile_size = 0;
	game_settings.org_disable_pk_pneuma_cost = 0;

    /* Email */
    game_settings.enable_email = false;
    game_settings.email_port = 0;
    game_settings.email_username = "";
    game_settings.email_host = "";
    game_settings.email_password = "";
    game_settings.email_from_addr = "";
    game_settings.email_from_name = "";

    /* Missions */
    game_settings.max_mission_allowance = 0;
    game_settings.inc_missions = 0;
    game_settings.max_missions = 0;

    /* Locker Settings */
    game_settings.lockers_enabled = false;
    game_settings.locker_rent_enabled = false;
    game_settings.max_locker_weight = 0;
    game_settings.max_locker_items = 0;
    game_settings.locker_rent_cost = 0;
    game_settings.locker_rent_time = 0;
    game_settings.locker_rent_time_max = 0;
    game_settings.locker_additional_cost_per_tier = 0;
    game_settings.locker_additional_slots_per_tier = 0;
    game_settings.locker_additional_weight_per_tier = 0;
    game_settings.locker_tier_max = 0;

    /* Vault Settings */
    game_settings.max_vault_weight = 0;
    game_settings.max_vault_items = 0;
    game_settings.vault_enabled = false;
    game_settings.vault_rent = false;
    game_settings.vault_rent_per_char = false;
    game_settings.vault_rent_cost = 0;
    game_settings.vault_rent_time = 0;
	game_settings.vault_rent_time_max = 0;
    game_settings.vault_additional_cost_per_char = 0;
    game_settings.vault_additional_slots_per_char = 0;
    game_settings.vault_additional_weight_per_char = 0;
	game_settings.vault_require_room = false;

    /* Coffer Settings */
    game_settings.max_coffer_weight = 0;
    game_settings.max_coffer_items = 0;
    game_settings.coffer_enabled = false;
    game_settings.coffer_rent = false;
    game_settings.coffer_rent_cost = 0;
    game_settings.coffer_rent_currency = "";
    game_settings.coffer_rent_time = 0;
	game_settings.coffer_rent_time_max = 0;

    /* Protocols and Ports*/
    game_settings.enable_telnet = false;
    game_settings.telnet_port = 0;
    game_settings.enable_tls = false;
    game_settings.tls_port = 0;
    game_settings.enable_websocket_tls = false;
    game_settings.websocket_tls_port = 0;
    game_settings.enable_web = false;
    game_settings.ssl_cert_path = "";
    game_settings.ssl_key_path = "";
    game_settings.enable_insecure_warning = false;
    game_settings.insecure_warning_msg = "";

    /* MSSP */
    game_settings.mssp_players = 0;
    game_settings.mssp_uptime = 0;
    game_settings.mssp_crawl_delay = 0;
    game_settings.mssp_hostname = "";
    game_settings.mssp_port = 0;
    game_settings.mssp_tls_port = 0;
    game_settings.mssp_codebase = "";
    game_settings.mssp_contact = "";
    game_settings.mssp_created = 0;
    game_settings.mssp_ip = "";
    game_settings.mssp_language = "";
    game_settings.mssp_location = "";
    game_settings.mssp_minimum_age = 0;
    game_settings.mssp_website = "";
    game_settings.mssp_family = "";
    game_settings.mssp_genre = "";
    game_settings.mssp_status = "";
    game_settings.mssp_gamesystem = "";
    game_settings.mssp_intermud = "";
    game_settings.mssp_subgenre = "";
    game_settings.mssp_discord_server = "";
    game_settings.mssp_areas = 0;
    game_settings.mssp_helpfiles = 0;
    game_settings.mssp_mobiles = 0;
    game_settings.mssp_objects = 0;
    game_settings.mssp_rooms = 0;
    game_settings.mssp_classes = 0;
    game_settings.mssp_levels = 0;
    game_settings.mssp_races = 0;
    game_settings.mssp_skills = 0;
    game_settings.mssp_dbsize = 0;
    game_settings.mssp_ansi = false;
    game_settings.mssp_gmcp = false;
    game_settings.mssp_mccp = false;
    game_settings.mssp_mcp = false;
    game_settings.mssp_msdp = false;
    game_settings.mssp_msp = false;
    game_settings.mssp_mxp = false;
    game_settings.mssp_pueb = false;
    game_settings.mssp_utf8 = false;
    game_settings.mssp_vt100 = false;
    game_settings.mssp_xterm256 = false;
    game_settings.mssp_xtermtrue = false;
    game_settings.mssp_atcp = false;
    game_settings.mssp_ssl = false;
    game_settings.mssp_pay2play = false;
    game_settings.mssp_pay4perks = false;
    game_settings.mssp_hiring_builders = false;
    game_settings.mssp_hiring_coders = false;
    game_settings.mssp_adult_material = false;
    game_settings.mssp_multiclass = false;
    game_settings.mssp_newbie_friendly = false;
    game_settings.mssp_player_cities = false;
    game_settings.mssp_player_clans = false;
    game_settings.mssp_player_crafting = false;
    game_settings.mssp_player_guilds = false;
    game_settings.mssp_equipment_system = "";
    game_settings.mssp_multiplaying = "";
    game_settings.mssp_playerkilling = false;
    game_settings.mssp_quest_system = false;
    game_settings.mssp_roleplaying = false;
    game_settings.mssp_training_system = false;
    game_settings.mssp_world_originality = false;


    for(;;)
    {
        word = feof (fp) ? "END" : fread_word(fp);
        fMatch = false;

        switch (UPPER(word[0]))
        {
            case '*':
                fMatch = true;
                fread_to_eol (fp);
            break;
			case 'A':
                KEY("AlignmentSystem", game_settings.alignment_system, fread_number(fp));
                KEY("AllowLinkAll", game_settings.allow_link_all, fread_number(fp));
                KEY("AllowMultiplayAcctAll", game_settings.allow_mp_acct_all, fread_number(fp));
                KEY("AllowMultiplayAcctStaff", game_settings.allow_mp_acct_staff, fread_number(fp));
                KEY("AllowMultiplayHostAll", game_settings.allow_mp_host_all, fread_number(fp));
                KEY("AllowMultiplayHostStaff", game_settings.allow_mp_host_staff, fread_number(fp));
                KEY("AllowUnlinkAll", game_settings.allow_unlink_all, fread_number(fp));
                break;

            case 'C':
			    KEY("CharacterDeleteDelay", game_settings.character_delete_delay_days, fread_number(fp));
                KEY("CofferEnabled", game_settings.coffer_enabled, fread_number(fp));
                KEY("CofferRent", game_settings.coffer_rent, fread_number(fp));
                KEY("CofferRentCost", game_settings.coffer_rent_cost, fread_number(fp));
                KEY("CofferRentCurrency", game_settings.coffer_rent_currency, fread_string(fp));
                KEY("CofferRentTime", game_settings.coffer_rent_time, fread_number(fp));
				KEY("CofferRentTimeMax", game_settings.coffer_rent_time_max, fread_number(fp));
                break;

            case 'D':
                KEY("DevServer", game_settings.dev_server, fread_number(fp));
                break;

            case 'E':
                KEY("Email_Enable", game_settings.enable_email, fread_number(fp));
                KEY("EmailUser", game_settings.email_username, fread_string(fp));
                KEY("EmailPassword", game_settings.email_password, fread_string(fp));
                KEY("EmailHost", game_settings.email_host, fread_string(fp));
                KEY("EmailPort", game_settings.email_port, fread_number(fp));
                KEY("EmailFromAddr", game_settings.email_from_addr, fread_string(fp));
                KEY("EmailFromName", game_settings.email_from_name, fread_string(fp));
                KEY("EnablePasswd", game_settings.enable_passwd, fread_number(fp));
                KEY("EnableMFA", game_settings.enable_mfa, fread_number(fp));
                KEY("EnableWeb", game_settings.enable_web, fread_number(fp));
                KEY("EnableWebsocketTls", game_settings.enable_websocket_tls, fread_number(fp));
                if (!str_cmp(word, "END"))
                {
                    if (game_settings.idle_disconnect_time <= 0)
                        game_settings.idle_disconnect_time = 30;
                    
                    if (game_settings.idle_time <= 0)
                        game_settings.idle_time = 12;

                    if (game_settings.idle_disconnect_time <= game_settings.idle_time)
                        game_settings.idle_disconnect_time = game_settings.idle_time + 5;

                    fclose(fp);
                    game_settings_write();
                    return(0); /* Success*/
                }
                break;

            case 'G':
                KEY("GameName", game_settings.game_name, fread_string(fp));
                break;

            case 'I':
                KEY("IdleDisconnectTimeout", game_settings.idle_disconnect_time, fread_number(fp));
                KEY("IdleTimeout", game_settings.idle_time, fread_number(fp));
                KEY("IncMissions", game_settings.inc_missions, fread_number(fp));
                KEY("InsecureWarning_Enable", game_settings.enable_insecure_warning, fread_number(fp));
                KEY("InsecureWarning_Msg", game_settings.insecure_warning_msg, fread_string(fp));
                break;

            case 'L':
                KEY("LockerAdditionalCostPerTier", game_settings.locker_additional_cost_per_tier, fread_number(fp));
                KEY("LockerAdditionalSlotsPerTier", game_settings.locker_additional_slots_per_tier, fread_number(fp));
                KEY("LockerAdditionalWeightPerTier", game_settings.locker_additional_weight_per_tier, fread_number(fp));
                KEY("LockerRentCost", game_settings.locker_rent_cost, fread_number(fp));
                KEY("LockerRentEnabled", game_settings.locker_rent_enabled, fread_number(fp));
                KEY("LockerRentTime", game_settings.locker_rent_time, fread_number(fp));
                KEY("LockerRentTimeMax", game_settings.locker_rent_time_max, fread_number(fp));
                KEY("LockerTierMax", game_settings.locker_tier_max, fread_number(fp));
                KEY("LockersEnabled", game_settings.lockers_enabled, fread_number(fp));
                KEY("LogAllConnections", game_settings.logall, fread_number(fp));
                KEY("LoginString", game_settings.login_string, fread_string(fp));
                break;

            case 'M':
                KEY("MaxAlias", game_settings.max_alias, fread_number(fp));
                KEY("MaxCharacters", game_settings.max_characters, fread_number(fp));
                KEY("MaxCofferItems", game_settings.max_coffer_items, fread_number(fp));
                KEY("MaxCofferWeight", game_settings.max_coffer_weight, fread_number(fp));
                KEY("MaxLockerItems", game_settings.max_locker_items, fread_number(fp));
                KEY("MaxLockerWeight", game_settings.max_locker_weight, fread_number(fp));
                KEY("MaxLogfileSize", game_settings.max_logfile_size, fread_number(fp));
                KEY("MaxLoginAttempts", game_settings.max_login_attempts, fread_number(fp));
                KEY("MaxMissionAllowance", game_settings.max_mission_allowance, fread_number(fp));
                KEY("MaxMissions", game_settings.max_missions, fread_number(fp));
                KEY("MaxOrgs", game_settings.max_orgs, fread_number(fp));
                KEY("MaxVaultItems", game_settings.max_vault_items, fread_number(fp));
                KEY("MaxVaultWeight", game_settings.max_vault_weight, fread_number(fp));
                KEY("MSSP_HOSTNAME",game_settings.mssp_hostname,fread_string(fp));
                KEY("MSSP_CODEBASE",game_settings.mssp_codebase,fread_string(fp));
                KEY("MSSP_CONTACT",game_settings.mssp_contact,fread_string(fp));
                KEY("MSSP_IP",game_settings.mssp_ip,fread_string(fp));
                KEY("MSSP_LANGUAGE",game_settings.mssp_language,fread_string(fp));
                KEY("MSSP_LOCATION",game_settings.mssp_location,fread_string(fp));
                KEY("MSSP_WEBSITE",game_settings.mssp_website,fread_string(fp));
                KEY("MSSP_FAMILY",game_settings.mssp_family,fread_string(fp));
                KEY("MSSP_GENRE",game_settings.mssp_genre,fread_string(fp));
                KEY("MSSP_STATUS",game_settings.mssp_status,fread_string(fp));
                KEY("MSSP_GAMESYSTEM",game_settings.mssp_gamesystem,fread_string(fp));
                KEY("MSSP_INTERMUD",game_settings.mssp_intermud,fread_string(fp));
                KEY("MSSP_SUBGENRE",game_settings.mssp_subgenre,fread_string(fp));
                KEY("MSSP_DISCORD_SERVER",game_settings.mssp_discord_server,fread_string(fp));
                KEY("MSSP_EQUIPMENT_SYSTEM",game_settings.mssp_equipment_system,fread_string(fp));
                KEY("MSSP_MULTIPLAYING",game_settings.mssp_multiplaying,fread_string(fp));
                KEY("MSSP_CRAWL_DELAY",game_settings.mssp_crawl_delay,fread_number(fp));
                KEY("MSSP_PORT",game_settings.mssp_port,fread_number(fp));
                KEY("MSSP_TLS_PORT",game_settings.mssp_tls_port,fread_number(fp));
                KEY("MSSP_CREATED",game_settings.mssp_created,fread_number(fp));
                KEY("MSSP_MINIMUM_AGE",game_settings.mssp_minimum_age,fread_number(fp));
                KEY("MSSP_AREAS",game_settings.mssp_areas,fread_number(fp));
                KEY("MSSP_HELPFILES",game_settings.mssp_helpfiles,fread_number(fp));
                KEY("MSSP_MOBILES",game_settings.mssp_mobiles,fread_number(fp));
                KEY("MSSP_OBJECTS",game_settings.mssp_objects,fread_number(fp));
                KEY("MSSP_ROOMS",game_settings.mssp_rooms,fread_number(fp));
                KEY("MSSP_CLASSES",game_settings.mssp_classes,fread_number(fp));
                KEY("MSSP_LEVELS",game_settings.mssp_levels,fread_number(fp));
                KEY("MSSP_RACES",game_settings.mssp_races,fread_number(fp));
                KEY("MSSP_SKILLS",game_settings.mssp_skills,fread_number(fp));
                KEY("MSSP_DBSIZE",game_settings.mssp_dbsize,fread_number(fp));
                KEY("MSSP_VT100",game_settings.mssp_vt100,fread_number(fp));
                KEY("MSSP_ANSI",game_settings.mssp_ansi,fread_number(fp));
                KEY("MSSP_ATCP",game_settings.mssp_atcp,fread_number(fp));
                KEY("MSSP_GMCP",game_settings.mssp_gmcp,fread_number(fp));
                KEY("MSSP_MCCP",game_settings.mssp_mccp,fread_number(fp));
                KEY("MSSP_MCP",game_settings.mssp_mcp,fread_number(fp));
                KEY("MSSP_MSDP",game_settings.mssp_msdp,fread_number(fp));
                KEY("MSSP_MSP",game_settings.mssp_msp,fread_number(fp));
                KEY("MSSP_MXP",game_settings.mssp_mxp,fread_number(fp));
                KEY("MSSP_PUEB",game_settings.mssp_pueb,fread_number(fp));
                KEY("MSSP_UTF8",game_settings.mssp_utf8,fread_number(fp));
                KEY("MSSP_VT100",game_settings.mssp_vt100,fread_number(fp));
                KEY("MSSP_XTERM256",game_settings.mssp_xterm256,fread_number(fp));
                KEY("MSSP_XTERMTRUE",game_settings.mssp_xtermtrue,fread_number(fp));
                KEY("MSSP_ATCP",game_settings.mssp_atcp,fread_number(fp));
                KEY("MSSP_SSL",game_settings.mssp_ssl,fread_number(fp));
                KEY("MSSP_PAY2PLAY",game_settings.mssp_pay2play,fread_number(fp));
                KEY("MSSP_PAY4PERKS",game_settings.mssp_pay4perks,fread_number(fp));
                KEY("MSSP_HIRING_BUILDERS",game_settings.mssp_hiring_builders,fread_number(fp));
                KEY("MSSP_HIRING_CODERS",game_settings.mssp_hiring_coders,fread_number(fp));
                KEY("MSSP_ADULT_MATERIAL",game_settings.mssp_adult_material,fread_number(fp));
                KEY("MSSP_MULTICLASS",game_settings.mssp_multiclass,fread_number(fp));
                KEY("MSSP_NEWBIE_FRIENDLY",game_settings.mssp_newbie_friendly,fread_number(fp));
                KEY("MSSP_PLAYER_CITIES",game_settings.mssp_player_cities,fread_number(fp));
                KEY("MSSP_PLAYER_CLANS",game_settings.mssp_player_clans,fread_number(fp));
                KEY("MSSP_PLAYER_CRAFTING",game_settings.mssp_player_crafting,fread_number(fp));
                KEY("MSSP_PLAYER_GUILDS",game_settings.mssp_player_guilds,fread_number(fp));
                KEY("MSSP_PLAYERKILLING",game_settings.mssp_playerkilling,fread_number(fp));
                KEY("MSSP_QUEST_SYSTEM",game_settings.mssp_quest_system,fread_number(fp));
                KEY("MSSP_ROLEPLAYING",game_settings.mssp_roleplaying,fread_number(fp));
                KEY("MSSP_TRAINING_SYSTEM",game_settings.mssp_training_system,fread_number(fp));
                KEY("MSSP_WORLD_ORIGINALITY",game_settings.mssp_world_originality,fread_number(fp));


            case 'N':
				KEY("NewAcctLock",game_settings.new_acct_lock,fread_number(fp));
				KEY("NewAcctLockMsg",game_settings.new_acct_lock_msg,fread_string(fp));
				KEY("NewCharLock",game_settings.new_char_lock,fread_number(fp));
				KEY("NewCharLockMsg",game_settings.new_char_lock_msg,fread_string(fp));
				KEY("NoteBootErrs",game_settings.note_boot_errors,fread_number(fp));
	            break;

			case 'O':
				KEY("OrgMaxRanks", game_settings.org_max_ranks, fread_number(fp));
				KEY("OrgPKCost", game_settings.org_disable_pk_pneuma_cost, fread_number(fp));


			case 'R':
                KEY("Require_2FA_All",game_settings.require_2fa_all,fread_number(fp));
                KEY("Require_2FA_Staff",game_settings.require_2fa_staff,fread_number(fp));
                KEY("RequireEmailVerification",game_settings.require_email_verif,fread_number(fp));
                KEY("RequireUniqPassStaff",game_settings.require_uniq_pass_staff,fread_number(fp));
                KEY("RestrictRacesByAlignment",game_settings.restrict_races_align,fread_number(fp));
                KEY("RestrictClassesByAlignment",game_settings.restrict_classes_align,fread_number(fp));

                break;

            case 'S':
                KEY("ServerDescription", game_settings.server_description, fread_string(fp));
                KEY("SSL_Cert_Path",game_settings.ssl_cert_path,fread_string(fp));
                KEY("SSL_Key_Path",game_settings.ssl_key_path,fread_string(fp));
                break;

            case 'T':
                KEY("Telnet_Enable", game_settings.enable_telnet, fread_number(fp));
                KEY("Telnet_Port", game_settings.telnet_port, fread_number(fp));
                KEY("Testport", game_settings.testport, fread_number(fp));
                KEY("Tls_Enable", game_settings.enable_tls, fread_number(fp));
                KEY("Tls_Port", game_settings.tls_port, fread_number(fp));
                break;

            case 'V':
                KEY("VaultAdditionalCostPerChar", game_settings.vault_additional_cost_per_char, fread_number(fp));
                KEY("VaultAdditionalSlotsPerChar", game_settings.vault_additional_slots_per_char, fread_number(fp));
                KEY("VaultAdditionalWeightPerChar", game_settings.vault_additional_weight_per_char, fread_number(fp));
                KEY("VaultEnabled", game_settings.vault_enabled, fread_number(fp));
                KEY("VaultRent", game_settings.vault_rent, fread_number(fp));
                KEY("VaultRentCost", game_settings.vault_rent_cost, fread_number(fp));
                KEY("VaultRentPerChar", game_settings.vault_rent_per_char, fread_number(fp));
                KEY("VaultRentTime", game_settings.vault_rent_time, fread_number(fp));
				KEY("VaultRentTimeMax", game_settings.vault_rent_time_max, fread_number(fp));

				KEY("VaultRequireRoom", game_settings.vault_require_room, fread_number(fp));

                break;

            case 'W':
                KEY("WebsocketTlsPort", game_settings.websocket_tls_port, fread_number(fp));
                KEY("Wizlock_Enable", game_settings.wizlock, fread_number(fp));
                KEY("Wizlock_Msg", game_settings.wizlock_msg, fread_string(fp));
                break;

        } /* end switch */

        if (!fMatch)
        {
	    sprintf(buf, "act_wiz.c, game_settings_read(): no match for '%s'!", word);
	    bug(buf, 0);
            fread_to_eol(fp);
        }
    } /* end for */


}


int gconfig_write(void)
{
    FILE *fp;
    extern GLOBAL_DATA gconfig;

    fp = fopen(CONFIG_FILE,"w");
    if (!fp)
    {
        bug("act_wiz.c, gconfig_write(): Unable to open gconfig.rc file for writing.",0);
        return(1); /* Failure*/
    }

	fprintf(fp, "DBversion %ld\n", (long)VERSION_DB);
    fprintf(fp, "NextMobUID %ld %ld\n", gconfig.next_mob_uid[2], gconfig.next_mob_uid[3]);
    fprintf(fp, "NextObjUID %ld %ld\n", gconfig.next_obj_uid[2], gconfig.next_obj_uid[3]);
    fprintf(fp, "NextTokenUID %ld %ld\n", gconfig.next_token_uid[2], gconfig.next_token_uid[3]);
    fprintf(fp, "NextVRoomUID %ld %ld\n", gconfig.next_vroom_uid[2], gconfig.next_vroom_uid[3]);
    fprintf(fp, "NextShipUID %ld %ld\n", gconfig.next_ship_uid[2], gconfig.next_ship_uid[3]);
    fprintf(fp, "NextAreaUID %ld\n", gconfig.next_area_uid);
    fprintf(fp, "NextWildsUID %ld\n", gconfig.next_wilds_uid);
    fprintf(fp, "NextVlinkUID %ld\n", gconfig.next_vlink_uid);
    fprintf(fp, "NextChurchUID %ld\n", gconfig.next_church_uid);

    fprintf(fp, "END\n");
    fclose(fp);
/*    log_string("act_wiz.c, gconfig_write(): Config written to 'gconfig.rc'.");*/
    return(0); /* Success*/
}

int game_settings_write(void)
{
	FILE *fp;

	fp = fopen(GAME_SETTINGS_FILE,"w");
	if (!fp)
	{
		bug("act_wiz.c, game_settings_write(): Unable to open game_settings.rc file for writing.",0);
		return(1); /* Failure*/
	}

    /* Basic Settings */
    fprintf(fp, "GameName %s~\n",  game_settings.game_name);
    fprintf(fp, "LoginString %s~\n",  game_settings.login_string);
    fprintf(fp, "ServerDescription %s~\n",  game_settings.server_description);
    fprintf(fp, "DevServer %d\n", game_settings.dev_server);

    /* Port Settings */
    fprintf(fp, "Telnet_Enable %d\n",  game_settings.enable_telnet);
    fprintf(fp, "Telnet_Port %d\n",  game_settings.telnet_port);
    fprintf(fp, "Tls_Enable %d\n",  game_settings.enable_tls);
    fprintf(fp, "Tls_Port %d\n",  game_settings.tls_port);
    fprintf(fp, "EnableWebsocketTls %d\n", game_settings.enable_websocket_tls);
    fprintf(fp, "WebsocketTlsPort %d\n", game_settings.websocket_tls_port);
    fprintf(fp, "EnableWeb %d\n", game_settings.enable_web);
    fprintf(fp, "SSL_Cert_Path %s~\n", game_settings.ssl_cert_path);
    fprintf(fp, "SSL_Key_Path %s~\n", game_settings.ssl_key_path);
    fprintf(fp, "Testport %d\n",  game_settings.testport);
    fprintf(fp, "InsecureWarning_Enable %d\n",  game_settings.enable_insecure_warning);
    fprintf(fp, "InsecureWarning_Msg %s~\n",  game_settings.insecure_warning_msg);

    /* Various Locks */
    fprintf(fp, "Wizlock_Enable %d\n",  game_settings.wizlock);
    fprintf(fp, "Wizlock_Msg %s~\n",  game_settings.wizlock_msg);
    fprintf(fp, "NewAcctLock %d\n", game_settings.new_acct_lock);
    fprintf(fp, "NewAcctLockMsg %s~\n", game_settings.new_acct_lock_msg);
    fprintf(fp, "NewCharLock %d\n", game_settings.new_char_lock);
    fprintf(fp, "NewCharLockMsg %s~\n", game_settings.new_char_lock_msg);

    /* Mission Settings */
    fprintf(fp, "IncMissions %d\n",  game_settings.inc_missions);
    fprintf(fp, "MaxMissionAllowance %d\n",  game_settings.max_mission_allowance);
    fprintf(fp, "MaxMissions %d\n",  game_settings.max_missions);

    /* Locker Settings */
    fprintf(fp, "LockersEnabled %d\n", game_settings.lockers_enabled);
    fprintf(fp, "LockerRentEnabled %d\n", game_settings.locker_rent_enabled);
    fprintf(fp, "MaxLockerWeight %d\n", game_settings.max_locker_weight);
    fprintf(fp, "MaxLockerItems %d\n", game_settings.max_locker_items);
    fprintf(fp, "LockerRentCost %d\n", game_settings.locker_rent_cost);
    fprintf(fp, "LockerRentTime %d\n", game_settings.locker_rent_time);
    fprintf(fp, "LockerRentTimeMax %d\n", game_settings.locker_rent_time_max);
    fprintf(fp, "LockerAdditionalCostPerTier %d\n", game_settings.locker_additional_cost_per_tier);
    fprintf(fp, "LockerAdditionalSlotsPerTier %d\n", game_settings.locker_additional_slots_per_tier);
    fprintf(fp, "LockerAdditionalWeightPerTier %d\n", game_settings.locker_additional_weight_per_tier);
    fprintf(fp, "LockerTierMax %d\n", game_settings.locker_tier_max);

    /* Vault Settings */
    fprintf(fp, "MaxVaultWeight %d\n", game_settings.max_vault_weight);
    fprintf(fp, "MaxVaultItems %d\n", game_settings.max_vault_items);
    fprintf(fp, "VaultEnabled %d\n", game_settings.vault_enabled);
    fprintf(fp, "VaultRent %d\n", game_settings.vault_rent);
    fprintf(fp, "VaultRentPerChar %d\n", game_settings.vault_rent_per_char);
    fprintf(fp, "VaultRentCost %d\n", game_settings.vault_rent_cost);
    fprintf(fp, "VaultRentTime %d\n", game_settings.vault_rent_time);
	fprintf(fp, "VaultRentTimeMax %d\n", game_settings.vault_rent_time_max);
	fprintf(fp, "VaultRequireRoom %d\n", game_settings.vault_require_room);
    fprintf(fp, "VaultAdditionalCostPerChar %d\n", game_settings.vault_additional_cost_per_char);
    fprintf(fp, "VaultAdditionalSlotsPerChar %d\n", game_settings.vault_additional_slots_per_char);
    fprintf(fp, "VaultAdditionalWeightPerChar %d\n", game_settings.vault_additional_weight_per_char);

    /* Coffer Settings */
    fprintf(fp, "MaxCofferWeight %d\n", game_settings.max_coffer_weight);
    fprintf(fp, "MaxCofferItems %d\n", game_settings.max_coffer_items);
    fprintf(fp, "CofferEnabled %d\n", game_settings.coffer_enabled);
    fprintf(fp, "CofferRent %d\n", game_settings.coffer_rent);
    fprintf(fp, "CofferRentCost %d\n", game_settings.coffer_rent_cost);
    fprintf(fp, "CofferRentCurrency %s~\n", game_settings.coffer_rent_currency);
    fprintf(fp, "CofferRentTime %d\n", game_settings.coffer_rent_time);
	fprintf(fp, "CofferRentTimeMax %d\n", game_settings.coffer_rent_time_max);

    /* Game System Settings */
    fprintf(fp, "AlignmentSystem %d\n", game_settings.alignment_system);
    fprintf(fp, "RestrictRacesByAlignment %d\n", game_settings.restrict_races_align);
    fprintf(fp, "RestrictClassesByAlignment %d\n", game_settings.restrict_classes_align);

    /* Account & Character Linking */
    fprintf(fp, "AllowLinkAll %d\n", game_settings.allow_link_all);
    fprintf(fp, "AllowUnlinkAll %d\n", game_settings.allow_unlink_all);

    /* Multiplaying Settings */
    fprintf(fp, "AllowMultiplayAcctAll %d\n",  game_settings.allow_mp_acct_all);
    fprintf(fp, "AllowMultiplayAcctStaff %d\n",  game_settings.allow_mp_acct_staff);
    fprintf(fp, "AllowMultiplayHostAll %d\n",  game_settings.allow_mp_host_all);
    fprintf(fp, "AllowMultiplayHostStaff %d\n",  game_settings.allow_mp_host_staff);

    /* Authentication Settings */
    fprintf(fp, "Require_2FA_All %d\n", game_settings.require_2fa_all);
    fprintf(fp, "Require_2FA_Staff %d\n", game_settings.require_2fa_staff);
    fprintf(fp, "RequireUniqPassStaff %d\n", game_settings.require_uniq_pass_staff);
    fprintf(fp, "RequireEmailVerification %d\n", game_settings.require_email_verif);
    fprintf(fp, "EnablePasswd %d\n", game_settings.enable_passwd);
    fprintf(fp, "EnableMFA %d\n", game_settings.enable_mfa);

    /* Timeouts */
    fprintf(fp, "IdleDisconnectTimeout %d\n",  game_settings.idle_disconnect_time);
    fprintf(fp, "IdleTimeout %d\n",  game_settings.idle_time);

    /* Misc Values */
    fprintf(fp, "LogAllConnections %d\n",  game_settings.logall);
    fprintf(fp, "MaxAlias %d\n",  game_settings.max_alias);
    fprintf(fp, "MaxCharacters %d\n",  game_settings.max_characters);
    fprintf(fp, "MaxLogfileSize %d\n",  game_settings.max_logfile_size);
    fprintf(fp, "MaxLoginAttempts %d\n",  game_settings.max_login_attempts);
    fprintf(fp, "MaxOrgs %d\n",  game_settings.max_orgs);
	fprintf(fp, "NoteBootErrs %d\n", game_settings.note_boot_errors);
	fprintf(fp, "OrgMaxRanks %d\n", game_settings.org_max_ranks);
	fprintf(fp, "CharacterDeleteDelay %d\n", game_settings.character_delete_delay_days);
	fprintf(fp, "OrgPKCost %d\n", game_settings.org_disable_pk_pneuma_cost);


    /* Email */
    fprintf(fp, "Email_Enable %d\n",  game_settings.enable_email);
    fprintf(fp, "EmailUser %s~\n",  game_settings.email_username);
    fprintf(fp, "EmailPassword %s~\n",  game_settings.email_password);
    fprintf(fp, "EmailHost %s~\n",  game_settings.email_host);
    fprintf(fp, "EmailPort %d\n",  game_settings.email_port);
    fprintf(fp, "EmailFromAddr %s~\n",  game_settings.email_from_addr);
    fprintf(fp, "EmailFromName %s~\n",  game_settings.email_from_name);

    /* MSSP */
    fprintf(fp, "MSSP_HOSTNAME %s~\n", game_settings.mssp_hostname);
    fprintf(fp, "MSSP_CODEBASE %s~\n", game_settings.mssp_codebase);
    fprintf(fp, "MSSP_CONTACT %s~\n", game_settings.mssp_contact);
    fprintf(fp, "MSSP_IP %s~\n", game_settings.mssp_ip);
    fprintf(fp, "MSSP_LANGUAGE %s~\n", game_settings.mssp_language);
    fprintf(fp, "MSSP_LOCATION %s~\n", game_settings.mssp_location);
    fprintf(fp, "MSSP_WEBSITE %s~\n", game_settings.mssp_website);
    fprintf(fp, "MSSP_FAMILY %s~\n", game_settings.mssp_family);
    fprintf(fp, "MSSP_GENRE %s~\n", game_settings.mssp_genre);
    fprintf(fp, "MSSP_STATUS %s~\n", game_settings.mssp_status);
    fprintf(fp, "MSSP_GAMESYSTEM %s~\n", game_settings.mssp_gamesystem);
    fprintf(fp, "MSSP_INTERMUD %s~\n", game_settings.mssp_intermud);
    fprintf(fp, "MSSP_SUBGENRE %s~\n", game_settings.mssp_subgenre);
    fprintf(fp, "MSSP_DISCORD_SERVER %s~\n", game_settings.mssp_discord_server);
    fprintf(fp, "MSSP_EQUIPMENT_SYSTEM %s~\n", game_settings.mssp_equipment_system);
    fprintf(fp, "MSSP_MULTIPLAYING %s~\n", game_settings.mssp_multiplaying);
    fprintf(fp, "MSSP_CRAWL_DELAY %d\n", game_settings.mssp_crawl_delay);
    fprintf(fp, "MSSP_PORT %d\n", game_settings.mssp_port);
    fprintf(fp, "MSSP_TLS_PORT %d\n", game_settings.mssp_tls_port);
    fprintf(fp, "MSSP_CREATED %d\n", game_settings.mssp_created);
    fprintf(fp, "MSSP_MINIMUM_AGE %d\n", game_settings.mssp_minimum_age);
    fprintf(fp, "MSSP_AREAS %d\n", game_settings.mssp_areas);
    fprintf(fp, "MSSP_HELPFILES %d\n", game_settings.mssp_helpfiles);
    fprintf(fp, "MSSP_MOBILES %d\n", game_settings.mssp_mobiles);
    fprintf(fp, "MSSP_OBJECTS %d\n", game_settings.mssp_objects);
    fprintf(fp, "MSSP_ROOMS %d\n", game_settings.mssp_rooms);
    fprintf(fp, "MSSP_CLASSES %d\n", game_settings.mssp_classes);
    fprintf(fp, "MSSP_LEVELS %d\n", game_settings.mssp_levels);
    fprintf(fp, "MSSP_RACES %d\n", game_settings.mssp_races);
    fprintf(fp, "MSSP_SKILLS %d\n", game_settings.mssp_skills);
    fprintf(fp, "MSSP_DBSIZE %d\n", game_settings.mssp_dbsize);
    fprintf(fp, "MSSP_VT100 %d\n", game_settings.mssp_vt100);
    fprintf(fp, "MSSP_ANSI %d\n", game_settings.mssp_ansi);
    fprintf(fp, "MSSP_ATCP %d\n", game_settings.mssp_atcp);
    fprintf(fp, "MSSP_GMCP %d\n", game_settings.mssp_gmcp);
    fprintf(fp, "MSSP_MCCP %d\n", game_settings.mssp_mccp);
    fprintf(fp, "MSSP_MCP %d\n", game_settings.mssp_mcp);
    fprintf(fp, "MSSP_MSDP %d\n", game_settings.mssp_msdp);
    fprintf(fp, "MSSP_MSP %d\n", game_settings.mssp_msp);
    fprintf(fp, "MSSP_MXP %d\n", game_settings.mssp_mxp);
    fprintf(fp, "MSSP_PUEB %d\n", game_settings.mssp_pueb);
    fprintf(fp, "MSSP_UTF8 %d\n", game_settings.mssp_utf8);
    fprintf(fp, "MSSP_VT100 %d\n", game_settings.mssp_vt100);
    fprintf(fp, "MSSP_XTERM256 %d\n", game_settings.mssp_xterm256);
    fprintf(fp, "MSSP_XTERMTRUE %d\n", game_settings.mssp_xtermtrue);
    fprintf(fp, "MSSP_ATCP %d\n", game_settings.mssp_atcp);
    fprintf(fp, "MSSP_SSL %d\n", game_settings.mssp_ssl);
    fprintf(fp, "MSSP_PAY2PLAY %d\n", game_settings.mssp_pay2play);
    fprintf(fp, "MSSP_PAY4PERKS %d\n", game_settings.mssp_pay4perks);
    fprintf(fp, "MSSP_HIRING_BUILDERS %d\n", game_settings.mssp_hiring_builders);
    fprintf(fp, "MSSP_HIRING_CODERS %d\n", game_settings.mssp_hiring_coders);
    fprintf(fp, "MSSP_ADULT_MATERIAL %d\n", game_settings.mssp_adult_material);
    fprintf(fp, "MSSP_MULTICLASS %d\n", game_settings.mssp_multiclass);
    fprintf(fp, "MSSP_NEWBIE_FRIENDLY %d\n", game_settings.mssp_newbie_friendly);
    fprintf(fp, "MSSP_PLAYER_CITIES %d\n", game_settings.mssp_player_cities);
    fprintf(fp, "MSSP_PLAYER_CLANS %d\n", game_settings.mssp_player_clans);
    fprintf(fp, "MSSP_PLAYER_CRAFTING %d\n", game_settings.mssp_player_crafting);
    fprintf(fp, "MSSP_PLAYER_GUILDS %d\n", game_settings.mssp_player_guilds);
    fprintf(fp, "MSSP_PLAYERKILLING %d\n", game_settings.mssp_playerkilling);
    fprintf(fp, "MSSP_QUEST_SYSTEM %d\n", game_settings.mssp_quest_system);
    fprintf(fp, "MSSP_ROLEPLAYING %d\n", game_settings.mssp_roleplaying);
    fprintf(fp, "MSSP_TRAINING_SYSTEM %d\n", game_settings.mssp_training_system);
    fprintf(fp, "MSSP_WORLD_ORIGINALITY %d\n", game_settings.mssp_world_originality);


	fprintf(fp, "END\n");
	fclose(fp);
    return(0); /* Success*/
}

void do_wiznet(CHAR_DATA *ch, char *argument)
{
    int flag;
    char buf[MAX_STRING_LENGTH];

    if (argument[0] == '\0')
    {
      	if (IS_SET(ch->wiznet,WIZ_ON))
      	{
            send_to_char("Signing off of Wiznet.\n\r",ch);
            REMOVE_BIT(ch->wiznet,WIZ_ON);
      	}
      	else
      	{
            send_to_char("Welcome to Wiznet!\n\r",ch);
            SET_BIT(ch->wiznet,WIZ_ON);
      	}
      	return;
    }

    if (!str_prefix(argument,"on"))
    {
	send_to_char("Welcome to Wiznet!\n\r",ch);
	SET_BIT(ch->wiznet,WIZ_ON);
	return;
    }

    if (!str_prefix(argument,"off"))
    {
	send_to_char("Signing off of Wiznet.\n\r",ch);
	REMOVE_BIT(ch->wiznet,WIZ_ON);
	return;
    }

    /* show wiznet status */
    if (!str_prefix(argument,"status"))
    {
		buf[0] = '\0';

		if (IS_SET(ch->comm, COMM_COMPACT))
		{
			send_to_char("Wiznet status: ", ch);
			for (flag = 0; wiznet_table[flag].name != NULL; flag++)
			{
				if (wiznet_table[flag].rank <= get_staff_rank(ch))
				{
					if (!str_cmp(wiznet_table[flag].name, "on"))
						sprintf(buf, "\t<send href=\"wiznet\" hint=\"Toggle wiznet\">%s%s{X", IS_SET(ch->wiznet, wiznet_table[flag].flag) ? "{G" : "{R", wiznet_table[flag].name);
					else
						sprintf(buf, "\t<send href=\"wiznet %s\" hint=\"Toggle '%s' wiznet channel\">%s%s{X", wiznet_table[flag].name, wiznet_table[flag].name, IS_SET(ch->wiznet, wiznet_table[flag].flag) ? "{G" : "{R", wiznet_table[flag].name);
				}	
				else
					sprintf(buf, "{D%s{X", wiznet_table[flag].name);
				strcat(buf, " ");
				send_to_char(buf, ch);
			}

			send_to_char("\n\r", ch);
		}
		else
		{
			for (flag = 0; wiznet_table[flag].name != NULL; flag++)
			{
				line(ch, 23, "{B", "_");
				send_to_char("{B|    {WWiznet Status{B    |{X\n\r",ch);
				line(ch, 23, "{B", "-");

				for (flag = 0; wiznet_table[flag].name != NULL; flag++)
				{
		    		if (wiznet_table[flag].rank <= get_staff_rank(ch))
					{
						if (!str_cmp(wiznet_table[flag].name, "on"))
							sprintf(buf, "{B| {W%-15s{X \t<send href=\"wiznet\" hint=\"Toggle wiznet\">%s {B|{X\n\r", wiznet_table[flag].name, IS_SET(ch->wiznet, wiznet_table[flag].flag) ? "{GON{x\t</send> " : "{ROFF{x\t</send>");
						else
							sprintf(buf, "{B| {W%-15s{X \t<send href=\"wiznet %s\" hint=\"Toggle '%s' wiznet channel\">%s {B|{X\n\r", wiznet_table[flag].name, wiznet_table[flag].name, wiznet_table[flag].name, IS_SET(ch->wiznet, wiznet_table[flag].flag) ? "{GON{x\t</send> " : "{ROFF{x\t</send>");
					}
					else
						sprintf(buf, "{B| {D%-15s{X, {rOFF{x {B|{X\n\r", wiznet_table[flag].name);
					send_to_char(buf, ch);
				}
				line(ch, 23, "{B", "-");
				send_to_char("\n\r", ch);
			}
		}
		return;
    }
/*
    if (!str_prefix(argument,"show"))
    // list of all wiznet options
    {
	buf[0] = '\0';

	for (flag = 0; wiznet_table[flag].name != NULL; flag++)
	{
	    if (wiznet_table[flag].rank <= get_staff_rank(ch))
	    {
	    	strcat(buf,wiznet_table[flag].name);
	    	strcat(buf," ");
	    }
	}

	strcat(buf,"\n\r");

	send_to_char("Wiznet options available to you are:\n\r",ch);
	send_to_char(buf,ch);
	return;
    }
*/
    flag = wiznet_lookup(argument);

    if (flag == -1)
    {
		send_to_char("No such option.\n\r",ch);
		return;
    }

	if (get_staff_rank(ch) < wiznet_table[flag].rank)
	{
		send_to_char("You are not a high enough rank to use that option.\n\r", ch);
		return;
	}

    if (IS_SET(ch->wiznet,wiznet_table[flag].flag))
    {
	sprintf(buf,"You will no longer see %s on wiznet.\n\r",
	        wiznet_table[flag].name);
	send_to_char(buf,ch);
	REMOVE_BIT(ch->wiznet,wiznet_table[flag].flag);
    	return;
    }
    else
    {
    	sprintf(buf,"You will now see %s on wiznet.\n\r",
		wiznet_table[flag].name);
	send_to_char(buf,ch);
    	SET_BIT(ch->wiznet,wiznet_table[flag].flag);
	return;
    }

}


void wiznet(char *string, CHAR_DATA *ch, OBJ_DATA *obj,
	    long flag, long flag_skip, int min_level)
{
    DESCRIPTOR_DATA *d;
	char wiz_buf[MSL];
	char wiz_channel[MIL];

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        if (d->connected == CON_PLAYING
	&&  IS_IMMORTAL(d->character)
	&&  IS_SET(d->character->wiznet,WIZ_ON)
	&&  (!flag || IS_SET(d->character->wiznet,flag))
	&&  (!flag_skip || !IS_SET(d->character->wiznet,flag_skip))
	&&  get_staff_rank(d->character) >= min_level
	&&  d->character != ch)
        {
	    /* Higher level imms can see lower level imms sign on wizi, but not vice versa. */
            if (ch != NULL && flag == WIZ_LOGINS) {
		if (d->character->tot_level < ch->tot_level
		&&  ch->invis_level >= LEVEL_IMMORTAL)
		    continue;
	    }

		int flag_pos = 0;
		for (int i = 0; wiznet_table[i].name != NULL; i++)
		{
			if (wiznet_table[i].flag == flag)
			{
				flag_pos = i;
				break;
			}
		}
		
	    if (IS_SET(d->character->wiznet,WIZ_PREFIX))
		{
			strcpy(wiz_channel, wiznet_table[flag_pos].name);
			for (int i = 0; wiz_channel[i] != '\0'; i++)
			{
				wiz_channel[i] = toupper(wiz_channel[i]);
			}

			sprintf(wiz_buf, "{B({MWIZ-{W%s{B){G-->{x ", wiz_channel);
		    send_to_char(wiz_buf,d->character);
		}
		
			act_new(string,d->character,ch,NULL,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
		}
    }
}


void do_zot(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Zot whom?\n\r", ch);
	return;
    }

    if (ch->tot_level == MAX_LEVEL && !str_cmp(arg, "room"))
    {
    	for (victim = ch->in_room->people; victim != NULL; victim = victim->next_in_room)
	{
	    if (victim != ch
	    &&   victim->tot_level < ch->tot_level)
	    {
		send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", victim);

		send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", victim);

		act("{Y$n is struck by a bolt of lightning!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

		sprintf(buf, "{Y***ZOT*** {xYou have zotted %s!\n\r",
			IS_NPC(victim) ? victim->short_descr : victim->name);
		send_to_char(buf, ch);
		send_to_char("{ROUCH! That really did hurt!{x\n\r", victim);

		victim->hit = 1;
		victim->mana = 1;
		victim->move = 1;

		sprintf(buf, "%s zotted %s!",
			ch->name,
			IS_NPC(victim) ? victim->short_descr : victim->name);
		wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);

		log_string(buf);
	    }
	}

	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim) && ch->tot_level < MAX_LEVEL)
    {
	send_to_char("Try zotting players instead.\n\r", ch);
	return;
    }

   if (!IS_NPC(victim) && ch->tot_level <= victim->tot_level)
   {
	send_to_char("You may only punish those below you.\n\r", ch);
	return;
   }

    send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", victim);

    send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", victim);

    act("{Y$n is struck by a bolt of lightning!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    sprintf(buf, "{Y***ZOT*** {xYou have zotted %s!\n\r",
		    IS_NPC(victim) ? victim->short_descr : victim->name);
    send_to_char(buf, ch);
    send_to_char("{ROUCH! That really did hurt!{x\n\r", victim);

    victim->hit = 1;
    victim->mana = 1;
    victim->move = 1;

    sprintf(buf, "%s zotted %s!",
        ch->name,
	IS_NPC(victim) ? victim->short_descr : victim->name);
    wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);

    log_string(buf);
}


void do_nochannels(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Nochannel whom?", ch);
        return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch))
    {
        send_to_char("You failed.\n\r", ch);
        return;
    }

    if (IS_SET(victim->comm, COMM_NOCHANNELS))
    {
        REMOVE_BIT(victim->comm, COMM_NOCHANNELS);
        send_to_char("The gods have restored your channel priviliges.\n\r",
		      victim);
        send_to_char("NOCHANNELS removed.\n\r", ch);
	sprintf(buf,"$N restores channels to %s",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }
    else
    {
        SET_BIT(victim->comm, COMM_NOCHANNELS);
        send_to_char("The gods have revoked your channel priviliges.\n\r",
		       victim);
        send_to_char("NOCHANNELS set.\n\r", ch);
	sprintf(buf,"$N revokes %s's channels.",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }
}


void do_bamfin(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (!IS_NPC(ch))
    {
	smash_tilde(argument);

	if (argument[0] == '\0')
	{
	    sprintf(buf,"Your poofin is %s\n\r",ch->pcdata->immortal->bamfin);
	    send_to_char(buf,ch);
	    return;
	}

	if (strstr(argument,ch->name) == NULL)
	{
	    send_to_char("You must include your name.\n\r",ch);
	    return;
	}

	free_string(ch->pcdata->immortal->bamfin);
	ch->pcdata->immortal->bamfin = str_dup(argument);

        sprintf(buf,"Your poofin is now %s\n\r",ch->pcdata->immortal->bamfin);
        send_to_char(buf,ch);
    }
}


void do_bamfout(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (!IS_NPC(ch))
    {
        smash_tilde(argument);

        if (argument[0] == '\0')
        {
            sprintf(buf,"Your poofout is %s\n\r",ch->pcdata->immortal->bamfout);
            send_to_char(buf,ch);
            return;
        }

        if (strstr(argument,ch->name) == NULL)
        {
            send_to_char("You must include your name.\n\r",ch);
            return;
        }

        free_string(ch->pcdata->immortal->bamfout);
        ch->pcdata->immortal->bamfout = str_dup(argument);

        sprintf(buf,"Your poofout is now %s\n\r",ch->pcdata->immortal->bamfout);
        send_to_char(buf,ch);
    }
}


void do_deny(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH],buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Deny whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("Not on NPC's.\n\r", ch);
	return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch))
    {
	send_to_char("You failed.\n\r", ch);
	return;
    }

    SET_BIT(victim->act[0], PLR_DENY);
    send_to_char("You are denied access!\n\r", victim);
    sprintf(buf,"$N denies access to %s",victim->name);
    wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    act("Denied access to $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    save_char_obj(victim);
    stop_fighting(victim,true);
    do_function(victim, &do_quit, NULL);
}


void do_disconnect(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Disconnect whom?\n\r", ch);
	return;
    }

    if (is_number(arg))
    {
	int desc;

	desc = atoi(arg);
    	for (d = descriptor_list; d != NULL; d = d->next)
    	{
            if (d->descriptor == desc)
            {
            	act("Disconnected $N.", ch, d->character, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

				connection_remove(d);
				close_socket(d);
            	return;
            }
	}
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim->desc == NULL)
    {
	act("$N doesn't have a descriptor.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    for (d = descriptor_list; d != NULL; d = d->next)
    {
	if (d == victim->desc)
	{
            act("Disconnected $N.", ch, d->character, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
			connection_remove(d);
	    close_socket(d);
	    return;
	}
    }

    bug("Do_disconnect: desc not found.", 0);
    send_to_char("Descriptor not found!\n\r", ch);
    return;
}


void do_echo(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
	send_to_char("Global echo what?\n\r", ch);
	return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
	if (d->connected == CON_PLAYING)
	{
	    if (get_staff_rank(d->character) >= get_staff_rank(ch))
		send_to_char("global> ",d->character);
	    send_to_char(argument, d->character);
	    send_to_char("\n\r",   d->character);
	}
    }
}


void do_recho(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
	send_to_char("Local echo what?\n\r", ch);

	return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
	if (d->connected == CON_PLAYING
	&&   d->character->in_room == ch->in_room)
	{
            if (get_staff_rank(d->character) >= get_staff_rank(ch))
                send_to_char("local> ",d->character);
	    send_to_char(argument, d->character);
	    send_to_char("\n\r",   d->character);
	}
    }

    return;
}


void do_zecho(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if (argument[0] == '\0')
    {
	send_to_char("Zone echo what?\n\r",ch);
	return;
    }

    for (d = descriptor_list; d; d = d->next)
    {
	if (d->connected == CON_PLAYING
	&&  d->character->in_room != NULL && ch->in_room != NULL
	&&  d->character->in_room->area == ch->in_room->area)
	{
	    if (get_staff_rank(d->character) >= get_staff_rank(ch))
		send_to_char("zone> ",d->character);
	    send_to_char(argument,d->character);
	    send_to_char("\n\r",d->character);
	}
    }
}


void do_pecho(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);

    if (argument[0] == '\0' || arg[0] == '\0')
    {
	send_to_char("Personal echo what?\n\r", ch);
	return;
    }

    if  ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("Target not found.\n\r",ch);
	return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch) && get_staff_rank(ch) != MAX_LEVEL)
        send_to_char("personal> ",victim);

    send_to_char(argument,victim);
    send_to_char("\n\r",victim);
    send_to_char("personal> ",ch);
    send_to_char(argument,ch);
    send_to_char("\n\r",ch);
}



void do_transfer(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    ROOM_INDEX_DATA *location;
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0')
    {
	send_to_char("Transfer whom (and where)?\n\r", ch);
	return;
    }

    if (!str_cmp(arg1, "all"))
    {
	for (d = descriptor_list; d != NULL; d = d->next)
	{
	    if (d->connected == CON_PLAYING
	    &&   d->character != ch
	    &&   d->character->in_room != NULL
	    &&   can_see(ch, d->character)
	    &&   ch->tot_level >= d->character->tot_level)
	    {
		char buf[MAX_STRING_LENGTH];
		sprintf(buf, "%s %s", d->character->name, arg2);
		do_function(ch, &do_transfer, buf);
	    }
	}
	return;
    }

    if (arg2[0] == '\0')
	location = ch->in_room;
    else
    {
	if ((location = find_location(ch, arg2)) == NULL)
	{
	    send_to_char("No such location.\n\r", ch);
	    return;
	}

	if (!is_room_owner(ch,location) && room_is_private(location, ch)
	&&  get_staff_rank(ch) < STAFF_IMPLEMENTOR)
	{
	    send_to_char("That room is private right now.\n\r", ch);
	    return;
	}
    }

    if ((victim = get_char_world(ch, arg1)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim->in_room == NULL)
    {
	send_to_char("They are in limbo.\n\r", ch);
	return;
    }

    if (victim->tot_level > ch->tot_level && !IS_NPC(victim)) {
	send_to_char("You may not transfer those superior to you.\n\r", ch);
	return;
    }

    if (victim->fighting != NULL)
	stop_fighting(victim, true);

    act("$n disappears.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(victim);
	if(location->wilds)
		char_to_vroom(victim, location->wilds, location->x, location->y);
	else
		char_to_room(victim, location);

    if (victim->pet != NULL)
    {
    	char_from_room (victim->pet);
	if(location->wilds)
		char_to_vroom(victim->pet, location->wilds, location->x, location->y);
	else
		char_to_room(victim->pet, location);
    }

    if (ch != victim)
	act("$n has transferred you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    do_function(victim, &do_look, "auto");

	sprintf(buf, "Transferred $N to %s (%ld)",
	victim->in_room->name,
	victim->in_room->vnum);
	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
}


void do_at(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	ROOM_INDEX_DATA *location;
	ROOM_INDEX_DATA *original;
	WILDS_DATA *wilds = NULL;
	OBJ_DATA *on;
	CHAR_DATA *wch;
	int x = 0;
	int y = 0;
	ITERATOR wit;

	argument = one_argument(argument, arg);

	if (ch->tot_level < 150) {
		send_to_char("Huh?\n\r", ch);
		return;
	}

	if (!arg[0] || !argument[0]) {
		send_to_char("At where what?\n\r", ch);
		return;
	}

	if (!(location = find_location(ch, arg))) {
		send_to_char("No such location.\n\r", ch);
		return;
	}

	if (!is_room_owner(ch,location) && room_is_private(location, ch) &&
		get_staff_rank(ch) < STAFF_IMPLEMENTOR) {
		send_to_char("That room is private right now.\n\r", ch);
		return;
	}

	original = ch->in_room;
	if(original->wilds) {
		wilds = original->wilds;
		x = original->x;
		y = original->y;
	}
	on = ch->on;
	char_from_room(ch);
	if(location->wilds)
		char_to_vroom(ch, location->wilds, location->x, location->y);
	else
		char_to_room(ch, location);
	interpret(ch, argument);

	/*
	* See if 'ch' still exists before continuing!
	* Handles 'at XXXX quit' case.
	*/

	iterator_start(&wit, loaded_chars);
	while(( wch = (CHAR_DATA *)iterator_nextdata(&wit)))
	{
		if (wch == ch) {
			char_from_room(ch);
			if(wilds)
				char_to_vroom(ch, wilds, x, y);
			else
				char_to_room(ch, original);
			ch->on = on;
			break;
		}
	}
	iterator_stop(&wit);
}

void do_startinvasion(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char arg4[MIL];
    int max_level = 0;
    long leader_vnum = 0;
    long mob_vnum = 0;
    INVASION_QUEST *quest;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0')
    {
	send_to_char("startinvasion AREA leader_vnum mob_vnum max_level\n\r", ch);
	return;
    }

  pArea = find_area(arg1);
  if (pArea == NULL) {
    send_to_char("Can't find that area.\n\r", ch);
    return;
  }

  leader_vnum = atol(arg2);
  mob_vnum = atol(arg3);
  max_level = atoi(arg4);

  if (get_mob_index(leader_vnum) == NULL || get_mob_index(mob_vnum) == NULL) {
    send_to_char("One or both of your mob vnums are wrong.\n\r", ch);
    return;
  }

  quest = create_invasion_quest(pArea, max_level, leader_vnum, mob_vnum);
  pArea->invasion_quest = quest;
}

void do_mapgoto(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom;
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    int dx, dy;
    long index = 0;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("MapGoto X Y\n\r", ch);
	return;
    }

	dx = atoi(arg1);
	dy = atoi(arg2);

  pArea = find_area("Wilderness");

  index = (long)((long)dy * (long)pArea->map_size_x + dx + pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

        if ((pRoom = get_room_index(index)) == NULL)
	{
	    send_to_char("Couldn't find room.\n\r", ch);
	    return;
	}

  char_from_room(ch);
  char_to_room(ch, pRoom);

    do_function(ch, &do_look, "auto");
}


void do_goto(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    CHAR_DATA *rch;
    int count = 0;

    if (argument[0] == '\0')
    {
	send_to_char("Goto where?\n\r", ch);
	return;
    }

    if ((location = find_location(ch, argument)) == NULL)
    {
	send_to_char("No such location.\n\r", ch);
	return;
    }

    count = 0;
    for (rch = location->people; rch != NULL; rch = rch->next_in_room)
        count++;

    if (!is_room_owner(ch,location) && room_is_private(location, ch)
    &&  (count > 1 || get_staff_rank(ch) < STAFF_IMPLEMENTOR))
    {
	send_to_char("That room is private right now.\n\r", ch);
	return;
    }

    if (ch->fighting != NULL)
	stop_fighting(ch, true);

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
//	if (get_staff_rank(rch) >= ch->invis_level)
//	{
	    if (ch->pcdata != NULL && ch->pcdata->immortal != NULL &&  ch->pcdata->immortal->bamfout[0] != '\0')
		act("$t",ch,rch, NULL, NULL, NULL,ch->pcdata->immortal->bamfout, NULL,TO_VICT, NULL, NULL);
	    else
		act("$n leaves in a swirling mist.",ch,rch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
//	}
    }

    char_from_room(ch);
	if(location->wilds)
		char_to_vroom(ch, location->wilds, location->x, location->y);
	else
		char_to_room(ch, location);

    if (ch->pet != NULL)
    {
    	char_from_room (ch->pet);
	if(location->wilds)
		char_to_vroom(ch->pet, location->wilds, location->x, location->y);
	else
		char_to_room(ch->pet, location);
    }


    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (ch != rch /*&& get_staff_rank(rch) >= ch->invis_level*/)
        {
            if (ch->pcdata != NULL && ch->pcdata->immortal != NULL && ch->pcdata->immortal->bamfin[0] != '\0')
                act("$t",ch,rch, NULL, NULL, NULL,ch->pcdata->immortal->bamfin, NULL,TO_VICT, NULL, NULL);
            else
                act("$n appears in a swirling mist.",ch,rch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
        }
    }

    do_function(ch, &do_look, "auto");
}

void do_goxy (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    CHAR_DATA *rch;
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    int x, y = 0;
    int wuid = 0;

    if (argument[0] == '\0')
    {
        send_to_char ("Goxy: Usage:\n\r", ch);
        send_to_char ("           : goxy <x coor> <y coor>        "
                      "- Standing in a wilds region, goto these coors.\n\r", ch);
        send_to_char ("           : goxy <x coor> <y coor> <wuid> "
                      "- The same, but specifying the target wilds uid.\n\r", ch);
        return;
    }

    if (!ch->in_room || (pArea = ch->in_room->area) == NULL)
    {
        send_to_char ("Goxy: You need to be in an area that has wilds defined.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (!is_number(arg1) || (x=atoi(arg1)) < 0 || !is_number(arg2) || (y=atoi(arg2)) < 0)
    {
        send_to_char("Usage: goxy (x coordinate) (y coordinate).\n\r", ch);
        send_to_char("  e.g.   goxy 100 25\n\r", ch);
        return;
    }

    if (!str_cmp(arg3, ""))
    {
        if (!ch->in_wilds)
        {
            send_to_char("goxy: you're not in a wilds map. Type 'goxy' for usage info.\n\r", ch);
            return;
        }
        else
            pWilds = ch->in_wilds;
    }
    else
    {
        if (!is_number(arg3) || ((wuid=atoi(arg3)) <= 0))
        {
                send_to_char("goxy: You must specify a valid wuid. Type 'goxy' for usage info.\n\r", ch);
                return;
        }
        else
        {
            if ((pWilds = get_wilds_from_uid(NULL, wuid)) == NULL)
            {
                send_to_char("goxy: Could not find that wuid. Type 'goxy' for usage info.\n\r", ch);
                return;
            }
        }

    }

    /* Vizz - Remember, bottom right corner will have coors (map_size_x - 1, map_size_y - 1)
     *        because the map origin is at (0,0), not (1,1)
     */
    if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1))
    {
        send_to_char("goxy: coordinate out of wilds range.\n\r", ch);
        return;
    }

    if (!check_for_bad_room(pWilds, x, y))
    {
        send_to_char("goxy: there isn't a room at that location!\n\r", ch);
        return;
    }

    if (ch->fighting != NULL)
        stop_fighting (ch, true);

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_staff_rank (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL)
            {
                if (IS_IMMORTAL(ch) && ch->pcdata->immortal->bamfout[0] != '\0')
                    act ("$t", ch, rch, NULL, NULL, NULL, ch->pcdata->immortal->bamfout, NULL, TO_VICT, NULL, NULL);
                else
                    act ("$n leaves in a swirling mist.", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);

/* Vizz - For later on... disabled for now.
                if (ch->pcdata->poofout_mspfile[0] != '\0')
                {
                    sprintf(buf, "!!SOUND(%s V=%d L=1 T=staff)",
                            ch->pcdata->poofout_mspfile,
                            ch->pcdata->poofout_mspvolume);
                    act( buf, ch, NULL, rch, TO_VICT_MSP );
                }
*/

            }

        }

    }

    char_from_room (ch);
    char_to_vroom (ch, pWilds, x, y);

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
        if (get_staff_rank (rch) >= ch->invis_level)
        {
            if (ch->pcdata != NULL && IS_IMMORTAL(ch) && ch->pcdata->immortal->bamfin[0] != '\0')
                act ("$t", ch, rch, NULL, NULL, NULL, ch->pcdata->immortal->bamfin, NULL, TO_VICT, NULL, NULL);
            else
                act ("$n appears in a swirling mist.", ch, rch, NULL, NULL, NULL, NULL, NULL,
                     TO_VICT, NULL, NULL);
        }
    }

    do_function (ch, &do_look, "auto");
    return;
}


void do_stat(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *location;
    CHAR_DATA *victim;

    string = one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  stat <name>\n\r",ch);
	send_to_char("  stat area <number>\n\r", ch);
	send_to_char("  stat wilds <wuid>\n\r", ch);
	send_to_char("  stat obj <name>\n\r",ch);
	send_to_char("  stat mob <name>\n\r",ch);
	send_to_char("  stat room <number>\n\r",ch);
	//send_to_char("  stat aff <character or object>\n\r", ch);
	send_to_char("  stat token <mob <name>|obj <name>|room> [count.]<token vnum>\n\r", ch);
	return;
    }

    if (!str_cmp(arg,"room"))
    {
	do_function(ch, &do_rstat, string);
	return;
    }

    if (!str_cmp(arg,"obj") || !str_cmp(arg,"object"))
    {
	do_function(ch, &do_ostat, string);
	return;
    }

    if(!str_cmp(arg,"char")  || !str_cmp(arg,"mob"))
    {
	do_function(ch, &do_mstat, string);
	return;
    }

    if (!str_cmp (arg, "area"))
    {
        do_function (ch, &do_astat, string);
        return;
    }

    if (!str_cmp(arg, "token"))
    {
	do_function(ch, &do_tstat, string);
	return;
    }

    if (!str_cmp (arg, "wilds"))
    {
        do_function (ch, &do_wstat, string);
        return;
    }
    if (!str_cmp(arg,"acct") || !str_cmp(arg,"account"))
    {
	do_function(ch, &do_accstat, string);
	return;
    }

    /* do it the old way */
    obj = get_obj_world(ch,argument);
    if (obj != NULL)
    {
	do_function(ch, &do_ostat, argument);
	return;
    }

    victim = get_char_world(ch,argument);
    if (victim != NULL)
    {
	do_function(ch, &do_mstat, argument);
	return;
    }

    location = find_location(ch,argument);
    if (location != NULL)
    {
	do_function(ch, &do_rstat, argument);
	return;
    }

    send_to_char("Nothing by that name found anywhere.\n\r",ch);
}

void do_astat (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    BUFFER *output;
    char buf[MSL];
    char arg[MIL];
    int anum;

    one_argument(argument, arg);
    if (!str_cmp(arg, ""))
    {
        pArea = ch->in_room->area;
    }
    else
    {
        if (!is_number(arg))
        {
            send_to_char("Syntax:  stat area\n\r", ch);
            send_to_char("         stat area [uid]\n\r", ch);
            return;
        }
        else
        {
            anum = atoi(arg);
            pArea = get_area_from_uid(anum);
        }
    }

    if(!pArea) {
	    send_to_char("No such area exists.\n\r", ch);
	    return;
    }

    output = new_buf();
    add_buf (output, "\n\r{x[ {Wstat area{x ]\n\r\n\r");
    sprintf (buf, "Area    : [{W%3ld{x]  Uid	: [{W%6ld{x]\n\r", pArea->anum, pArea->uid);
    add_buf (output, buf);
    sprintf (buf, "Name    : [{W%s{x]\n\r", pArea->name ? pArea->name : "(not set)");
    add_buf (output, buf);
    sprintf (buf, "Filename: [{W%s{x]\n\r", pArea->file_name);
    add_buf (output, buf);
    sprintf (buf, "Vnums   : [{W%ld{x-{W%ld{x]\n\r", pArea->min_vnum, pArea->max_vnum);
    add_buf (output, buf);
    sprintf (buf, "Recall  : [{W%6ld{x] {W%s{x\n\r", pArea->recall.id[0],
             get_room_index (pArea->recall.id[0])
             ? get_room_index (pArea->recall.id[0])->name : "none");
    add_buf (output, buf);
    sprintf (buf, "Security: [{W%d{x]\n\r", pArea->security);
    add_buf (output, buf);
    sprintf (buf, "\n\r{C*Staff assigned*{x\n\r");
    add_buf (output, buf);
    sprintf (buf, "Builders: [{W%s{x]\n\r", pArea->builders ? pArea->builders : "(none set)");
    add_buf (output, buf);
    sprintf (buf, "Credits : [{W%s{x]\n\r", pArea->credits ? pArea->credits : "(none set)");
    add_buf (output, buf);
    add_buf (output, "\n\r{C*Wilds Sectors*{x\n\r");

    if (pArea->wilds)
    {
        int cnt=0;
        add_buf (output, "   UID       Dimensions     Name\n\r");
        for (pWilds = pArea->wilds;pWilds;pWilds = pWilds->next)
        {
            sprintf (buf, "  ({W%7ld{x) [{W%5d{x x {W%5d{x] '{W%s{x'\n\r",
                     pWilds->uid,
                     pWilds->map_size_x, pWilds->map_size_y,
                     pWilds->name);
            add_buf (output, buf);
            cnt++;
        }
    }
    else
    {
        add_buf (output, "    (None defined)\n\r");
    }

    add_buf (output, "\n\r{C*Current Attributes*{x\n\r");
    sprintf (buf, "Age     : [{W%d{x]\n\r", pArea->age);
    add_buf (output, buf);
    sprintf (buf, "Flags   : [{W%s{x]\n\r",
             flag_string (area_flags, pArea->area_flags));
    add_buf (output, buf);
    sprintf (buf, "Players : {W%d{x\n\r", pArea->nplayer);
    add_buf (output, buf);

    page_to_char(buf_string(output), ch);
    free_buf(output);
    return;
}

void do_accstat(CHAR_DATA *ch, char *argument)
{
     ACCOUNT_DATA *account;
    ACCOUNT_CHARACTER *acd;
    ACCOUNT_CHARACTER *staff_chars[100];
    ACCOUNT_CHARACTER *regular_chars[100];
    char name_buf[50], rank_buf[50], level_buf[50], race_buf[50], class_buf[50];
    int staff_count = 0, regular_count = 0;
    BUFFER *output;
    char buf[MSL];
    char arg[MIL];
    bool loaded;

    one_argument(argument, arg);

    if (IS_NULLSTR(arg)) {
        send_to_char("Syntax: accstat <accountname>\n\r", ch);
        send_to_char("        accstat player:<name>\n\r", ch);
        return;
    }

    // Use the new get_account_by_identifier function
    account = get_account_by_identifier(arg, &loaded);
    if (!account) {
        if (!strncmp(arg, "player:", 7))
            send_to_char("Player not found or has no account.\n\r", ch);
        else
            send_to_char("No such account exists. Try using player:<name> to look up by character name.\n\r", ch);
        return;
    }

    output = new_buf();
    add_buf(output, "\n\r{x[ {WAccount Status{x ]\n\r\n\r");

    sprintf(buf, "Username      : [{W%s{x]\n\r", account->username);
    add_buf(output, buf);

    sprintf(buf, "Email         : [{W%s{x]\n\r", account->email ? account->email : "(none set)");
    add_buf(output, buf);

    sprintf(buf, "Creation Host   : [{W%s{x]\n\r", account->creation_host ? account->creation_host : "(unknown)");
    add_buf(output, buf);

    sprintf(buf, "Last Host       : [{W%s{x]\n\r", account->last_ip ? account->last_ip : "(unknown)");
    add_buf(output, buf);

    sprintf(buf, "MFA Status       : [{W%s{x]\n\r", account->mfa_enabled ? "{GENABLED{x" : (account->mfa_pending ? "{YSETUP IN PROGRESS{x" : "{ROFF{x"));
    add_buf(output, buf);

    // Format creation date and last login
    if (account->creation_date) {
        strftime(buf, sizeof(buf), "Creation Date : [{W%Y-%m-%d %H:%M:%S{x]\n\r", localtime(&account->creation_date));
        add_buf(output, buf);
    } else {
        add_buf(output, "Creation Date : [{W(unknown){x]\n\r");
    }

    if (account->last_login) {
        strftime(buf, sizeof(buf), "Last Login    : [{W%Y-%m-%d %H:%M:%S{x]\n\r", localtime(&account->last_login));
        add_buf(output, buf);
    } else {
        add_buf(output, "Last Login    : [{W(never){x]\n\r");
    }

    sprintf(buf, "Last Host     : [{W%s{x]\n\r", account->last_login_host ? account->last_login_host : "(unknown)");
    add_buf(output, buf);

    sprintf(buf, "Reset State   : [{W%d{x]\n\r", account->reset_state);
    add_buf(output, buf);

    sprintf(buf, "Char Count    : [{W%d{x] / [{W%d{x] (limit)\n\r", account->character_count, account->character_limit > 0 ? account->character_limit : game_settings.max_characters);
    add_buf(output, buf);

    sprintf(buf, "Staff Limit   : [{W%d{x]\n\r", account->staff_limit);
    add_buf(output, buf);

    sprintf(buf, "Staff Account : [{W%s{x]\n\r", account->staff_account ? "Yes" : "No");
    add_buf(output, buf);

    sprintf(buf, "Flags         : [{W%s{x]\n\r", flag_string(acct_flags, account->acct_flags));
    add_buf(output, buf);

	    int note_count = 0;
    ACCOUNT_NOTE_DATA *note;
    for (note = account->staff_notes; note != NULL; note = note->next)
        note_count++;
    
    if (note_count > 0)
        sprintf(buf, "Staff Notes   : [{R%d note%s{x] (Use 'accnote list %s' to view)\n\r", 
                note_count, note_count == 1 ? "" : "s", account->username);
    else
        sprintf(buf, "Staff Notes   : [{GNone{x]\n\r");
    add_buf(output, buf);

// Separate staff and regular characters
ITERATOR it;
iterator_start(&it, account->characters);
while ((acd = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
    if (acd->staff && acd->staff_rank >= STAFF_IMMORTAL)
        staff_chars[staff_count++] = acd;
    else
        regular_chars[regular_count++] = acd;
}
iterator_stop(&it);

// Sort staff characters alphabetically
for (int i = 0; i < staff_count - 1; i++) {
    for (int j = 0; j < staff_count - i - 1; j++) {
        if (strcasecmp(staff_chars[j]->name, staff_chars[j+1]->name) > 0) {
            ACCOUNT_CHARACTER *temp = staff_chars[j];
            staff_chars[j] = staff_chars[j+1];
            staff_chars[j+1] = temp;
        }
    }
}

// Sort regular characters alphabetically
for (int i = 0; i < regular_count - 1; i++) {
    for (int j = 0; j < regular_count - i - 1; j++) {
        if (strcasecmp(regular_chars[j]->name, regular_chars[j+1]->name) > 0) {
            ACCOUNT_CHARACTER *temp = regular_chars[j];
            regular_chars[j] = regular_chars[j+1];
            regular_chars[j+1] = temp;
        }
    }
}

// Display staff characters
if (staff_count > 0) {
    add_buf(output, "{B=={W[ {YSTAFF CHARACTERS {W]{B=={x\n\r");
    sprintf(buf, "{D%-4s %-16s %-15s %-30s %-20s{x\n\r", 
            "Num", "Name", "Rank", "Location", "Last Logoff");
    add_buf(output, buf);
    sprintf(buf, "{D%s{x\n\r", pad_string("", 90, NULL, "-"));
    add_buf(output, buf);

for (int i = 0; i < staff_count; i++) {
    acd = staff_chars[i];
    const char *staff_rank_str = flag_string(staff_ranks, acd->staff_rank);

    CHAR_DATA *vch = get_char_world(NULL, acd->name);

    const char *loc_str;
    char logoff_buf[32];

    if (vch != NULL) {
        // Character is online, use live data
        loc_str = format_location_string(vch->in_room ? vch->in_room : NULL);
        strcpy(logoff_buf, "{GLogged In{x");
    } else {
        // Offline, use stored data
        loc_str = str_dup(acd->last_area);
        if (acd->last_logoff > 0)
            strftime(logoff_buf, sizeof(logoff_buf), "%Y-%m-%d %H:%M", localtime(&acd->last_logoff));
        else
            strcpy(logoff_buf, "(unknown)");
    }

    sprintf(name_buf, "{W%s{x", acd->name);
    sprintf(rank_buf, "{R%s{x", staff_rank_str ? capitalize(staff_rank_str) : "IMM");

    sprintf(buf, "{G[%2d]{x %s%s %s%s {Y%s{x%s {C%s{x\n\r",
            i + 1,
            name_buf,
            pad_string((char *)name_buf, 16, NULL, " "),
            rank_buf,
            pad_string((char *)rank_buf, 15, NULL, " "),
            loc_str,
            pad_string((char *)loc_str, 30, NULL, " "),
            logoff_buf);
    add_buf(output, buf);
}
}

// Display regular characters
if (regular_count > 0) {
    add_buf(output, "\n\r{B=={W[ {YREGULAR CHARACTERS {W]{B=={x\n\r");
    sprintf(buf, "{D%-4s %-16s %-7s %-12s %-12s %-25s %-20s{x\n\r", 
            "Num", "Name", "Level", "Race", "Class", "Location", "Last Logoff");
    add_buf(output, buf);
    sprintf(buf, "{D%s{x\n\r", pad_string("", 110, NULL, "-"));
    add_buf(output, buf);

    for (int i = 0; i < regular_count; i++) {
        acd = regular_chars[i];

		CHAR_DATA *vch = get_char_world(NULL, acd->name);

        const char *loc_str;
        char logoff_buf[32];
        char *race_name, *class_name;
        int level, tot_level;

        if (vch != NULL) {
            // Character is online, use live data
            if (vch->pcdata && vch->pcdata->sub_class_current)
                level = vch->level;
            else
                level = vch->tot_level;
            tot_level = vch->tot_level;
            race_name = race_table[vch->race].name;
            if (vch->pcdata && vch->pcdata->sub_class_current) {
                class_name = str_dup(sub_class_table[ch->pcdata->sub_class_current].name[ch->sex]);
            }
			else
				class_name = "Adventurer";
            loc_str = format_location_string(vch->in_room ? vch->in_room : NULL);
            strcpy(logoff_buf, "{GLogged In{x");
        } else {
            // Offline, use stored data
            level = acd->current_level > 0 ? acd->current_level : acd->tot_level;
            tot_level = acd->tot_level;
            race_name = acd->race_name ? acd->race_name : "Unknown";
            class_name = acd->class_name ? acd->class_name : "Adventurer";
            loc_str = str_dup(acd->last_area);
            if (acd->last_logoff > 0)
                strftime(logoff_buf, sizeof(logoff_buf), "%Y-%m-%d %H:%M", localtime(&acd->last_logoff));
            else
                strcpy(logoff_buf, "(unknown)");
        }

        sprintf(name_buf, "{W%s{x", acd->name);
        sprintf(level_buf, "{G%d(%d){x", level, tot_level);
        sprintf(race_buf, "{W%s{x", capitalize(race_name));
        sprintf(class_buf, "{W%s{x", capitalize(class_name));

        sprintf(buf, "{G[%2d]{x %s%s %s%s %s%s %s%s {Y%s{x%s {C%s{x\n\r",
                i + staff_count + 1,
                name_buf,
                pad_string((char *)name_buf, 16, NULL, " "),
                level_buf,
                pad_string((char *)level_buf, 7, NULL, " "),
                race_buf,
                pad_string((char *)race_buf, 12, NULL, " "),
                class_buf,
                pad_string((char *)class_buf, 12, NULL, " "),
                loc_str,
                pad_string((char *)loc_str, 25, NULL, " "),
                logoff_buf);
        add_buf(output, buf);
    }
	if (loaded && account) {
    // Only remove and free if it was loaded just for this operation
	if (list_haslink(loaded_accounts, account))
	{
    list_remlink(loaded_accounts, account, NULL); // Remove from global list
    free_account(account);
	}
}
}

if (staff_count == 0 && regular_count == 0) {
    add_buf(output, "   {RNo characters found.{x\n\r");
}

	if (loaded) free_account(account);

    page_to_char(buf_string(output), ch);
    free_buf(output);
    return;
}


void do_rstat(CHAR_DATA *ch, char *argument)
{
    BUFFER *output;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location, *clone, *recall;
    OBJ_DATA *obj;
    CHAR_DATA *rch;
    int door;

    one_argument(argument, arg);
    location = (arg[0] == '\0') ? ch->in_room : find_location(ch, arg);
    if (location == NULL)
    {
	send_to_char("No such location.\n\r", ch);
	return;
    }

    if (!is_room_owner(ch,location) && ch->in_room != location
    &&  room_is_private(location, ch))
    {
	send_to_char("That room is private right now.\n\r", ch);
	return;
    }

    output = new_buf();
    add_buf (output, "{x\n\r[ {Wstat room{x ]\n\r");
    add_buf(output, "\n\r{C*Stats*{x\n\r");
    sprintf(buf, "{YName:{x %s\n\r"
                 "{YArea uid:{x %ld '%s'\n\r",
            location->name,
            location->area->uid,
            location->area->name);
    add_buf(output, buf);

    if (location->wilds == NULL)
    {

    sprintf(buf,
	"{BVnum:{x %ld  {BSector:{x %d  {BLight:{x %d  {BHealing:{x %d  {BMana:{x %d\n\r",
	location->vnum,
	location->sector_type,
                location->light,
                location->heal_rate,
                location->mana_rate);
    }
    else
    {
        sprintf(buf, "{YWilds uid:{x %ld '%s'\n\r"
                     "{YCoors:{x (%ld, %ld)  {YSector:{x %d  {YLight:{x %d  {YHealing:{x %d  {YMana:{x %d\n\r",
                location->wilds->uid,
                location->wilds->name,
                location->x,
                location->y,
                location->sector_type,
                location->light,
                location->heal_rate,
                location->mana_rate);
    }

    add_buf(output, buf);

	if (location_isset(&location->recall))
	{
		if(location->recall.wuid) {
			WILDS_DATA *wilds = get_wilds_from_uid(NULL,location->recall.wuid);
			if(wilds)
				sprintf(buf, "{WRecall:      Wilds {X%s {R[{X%lu{R]{X at {R<{X%lu,%lu,%lu{R>{X\n\r", wilds->name, location->recall.wuid,
					location->recall.id[0],location->recall.id[1],location->recall.id[2]);
			else
				sprintf(buf, "{WRecall:      Wilds {X??? {R[{X%lu{R]{X\n\r", location->recall.wuid);
		} else if(location->recall.id[0] > 0 && (recall = get_room_index(location->recall.id[0]))) {
				sprintf(buf, "{WRecall:      Room {R[{X%5ld{R]{X {X%s\n\r", location->recall.id[0], recall->name);
		} else
				sprintf(buf, "{WRecall:      {R[{X%lu{R]{X none\n\r", location->recall.id[0]);
		add_buf(output, buf);
	}

    sprintf(buf,
            "{YRoom flags:{x %s.\n\r{YDescription:{x\n\r%s\n\r",
            bitmatrix_string(room_flagbank, location->room_flag),
            location->description);
    add_buf(output, buf);

    if (location->extra_descr != NULL)
    {
	EXTRA_DESCR_DATA *ed;

	sprintf(buf, "{YExtra description keywords: {x'");

	for (ed = location->extra_descr; ed; ed = ed->next)
	{
	    add_buf(output, ed->keyword);

	    if (ed->next != NULL)
                add_buf(output, " ");
	}

    }

    add_buf(output, "\n\r{C*Exits*{x\n\r");

    for (door = 0; door < MAX_DIR; door++)
    {
		EXIT_DATA *pexit;

		if ((pexit = location->exit[door]) != NULL)
		{
			if( IS_SET(pexit->exit_info, EX_ENVIRONMENT) )
			{
				sprintf(buf,
                        "{x%s {Y[ENVIRONMENT]{x\n\r"
						"    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
						"    {YLock Flags: {x%s\n\r"
						"    {YExit flags: {x%s\n\r"
						"    {YKeyword:{x '%s'  {YDescription: {x%s",
                        dir_name[door],
                        pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
						flag_string(lock_flags, pexit->door.lock.flags),
                        flag_string(exit_flags, pexit->exit_info),
                        pexit->keyword,
                        pexit->short_desc[0] != '\0'
                        ? pexit->short_desc : "(none).\n\r");
			}
			else if( IS_SET(pexit->exit_info, EX_PREVFLOOR) )
			{
				sprintf(buf,
                        "{x%s {Y[PREVIOUS FLOOR]{x\n\r"
						"    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
						"    {YLock Flags: {x%s\n\r"
						"    {YExit flags: {x%s\n\r"
                        "    {YKeyword:{x '%s'  {YDescription: {x%s",
                        dir_name[door],
                        pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
						flag_string(lock_flags, pexit->door.lock.flags),
                        flag_string(exit_flags, pexit->exit_info),
                        pexit->keyword,
                        pexit->short_desc[0] != '\0'
                        ? pexit->short_desc : "(none).\n\r");
			}
			else if( IS_SET(pexit->exit_info, EX_NEXTFLOOR) )
			{
				sprintf(buf,
					"{x%s {Y[NEXT FLOOR]{x\n\r"
					"    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
					"    {YLock Flags: {x%s\n\r"
					"    {YExit flags: {x%s\n\r"
					"    {YKeyword:{x '%s'  {YDescription: {x%s",
					dir_name[door],
					pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
					flag_string(lock_flags, pexit->door.lock.flags),
					flag_string(exit_flags, pexit->exit_info),
					pexit->keyword,
					pexit->short_desc[0] != '\0'
					? pexit->short_desc : "(none).\n\r");
			}
			else if ((location->wilds == NULL && !IS_SET(pexit->exit_info, EX_VLINK)) ||
					(location->wilds != NULL && IS_SET(pexit->exit_info, EX_VLINK)))
            {
				ROOM_INDEX_DATA *dest = pexit->u1.to_room;

				sprintf(buf,
					"{x%s {Yto vnum {x%ld '%s' {Yin Area uid:{x %ld '%s'\n\r"
					"    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
					"    {YLock Flags: {x%s\n\r"
					"    {YExit flags: {x%s\n\r"
					"    {YKeyword:{x '%s'  {YDescription: {x%s",
					dir_name[door],
					(dest ? dest->vnum : -1),
					(dest ? dest->name : "(null)"),
					(dest ? dest->area->uid : -1),
					(dest ? dest->area->name : "(null)"),
					pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
					flag_string(lock_flags, pexit->door.lock.flags),
					flag_string(exit_flags, pexit->exit_info),
					pexit->keyword,
					pexit->short_desc[0] != '\0'
					? pexit->short_desc : "(none).\n\r");
            }
            else
            {
                if (location->wilds == NULL)
                    /* Exit goes to a wilds location from a static one*/
                    sprintf(buf,
                            "{x%s {Yto coors{x(%d, %d) {Yin Wilds uid:{x %ld{Y, Area uid:{x %ld\n\r"
                            "    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
							"    {YLock Flags: {x%s\n\r"
							"    {YExit flags: {x%s\n\r"
                            "    {YKeyword:{x '%s'  {YDescription: {x%s",
                            dir_name[door],
                            pexit->wilds.x,
                            pexit->wilds.y,
                            pexit->wilds.wilds_uid,
                            pexit->wilds.area_uid,
                            pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
							flag_string(lock_flags, pexit->door.lock.flags),
                            flag_string(exit_flags, pexit->exit_info),
                            pexit->keyword,
                            pexit->short_desc[0] != '\0'
                                                 ? pexit->short_desc : "(none).\n\r");
                else
                    /* Exit goes to a wilds location from another wilds one*/
                    sprintf(buf,
                            "{x%s {Yto coors{x (%d, %d)\n\r"
                            "    {YKey: {x%ld  Pick Chance: {x%d%%\n\r"
							"    {YLock Flags: {x%s\n\r"
							"    {YExit flags: {x%s\n\r"
                            "    {YKeyword:{x '%s'  {YDescription: {x%s",
                            dir_name[door],
                            pexit->wilds.x,
                            pexit->wilds.y,
                            pexit->door.lock.key_vnum, pexit->door.lock.pick_chance,
							flag_string(lock_flags, pexit->door.lock.flags),
                            flag_string(exit_flags, pexit->exit_info),
                            pexit->keyword,
                            pexit->short_desc[0] != '\0'
                                                 ? pexit->short_desc : "(none).\n\r");
            }

	    add_buf(output, buf);
	}
    }

    add_buf(output, "\n\r{C*Contents*{x\n\r");
    add_buf(output, "{YCharacters:{x");
    for (rch = location->people; rch; rch = rch->next_in_room)
    {
	if (can_see(ch,rch))
        {
	    add_buf(output, " ");
	    one_argument(rch->name, buf);
	    add_buf(output, buf);
	}
    }

    add_buf(output, ".\n\r{YObjects:{x");

    if (location->contents != NULL)
    {
        for (obj = location->contents; obj; obj = obj->next_content)
        {
            char buf2[MAX_STRING_LENGTH];

            add_buf(output, " ");
            one_argument(obj->name, buf);
            sprintf(buf2,"(%ld)", obj->pIndexData->vnum);
            strcat(buf, buf2);
            add_buf(output, buf);
        }
    }
    else
        add_buf(output, " (none)");

    add_buf(output, ".\n\r");

    if(location->clones) {
	    add_buf(output,"{CClones:{x\n\r");
	    for(clone = location->clones; clone; clone = clone->next) {
		    switch(clone->environ_type) {
		    case ENVIRON_ROOM: sprintf(buf,"{W%lu:%lu{x at Room [%ld:%lu:%lu]", clone->id[0], clone->id[1], clone->environ.room->vnum, clone->environ.room->id[0], clone->environ.room->id[1]); break;
		    case ENVIRON_MOBILE: sprintf(buf,"{W%lu:%lu{x in Mobile '%s' %ld [%lu:%lu]", clone->id[0], clone->id[1], clone->environ.mob->short_descr, VNUM(clone->environ.mob), clone->environ.mob->id[0], clone->environ.mob->id[1]); break;
		    case ENVIRON_OBJECT: sprintf(buf,"{W%lu:%lu{x in Object '%s' %ld [%lu:%lu]", clone->id[0], clone->id[1], clone->environ.obj->short_descr, VNUM(clone->environ.obj), clone->environ.obj->id[0], clone->environ.obj->id[1]); break;
		    case ENVIRON_TOKEN: sprintf(buf,"{W%lu:%lu{x in Token '%s' %ld [%lu:%lu]", clone->id[0], clone->id[1], clone->environ.token->name, VNUM(clone->environ.token), clone->environ.token->id[0], clone->environ.token->id[1]); break;
		    default: sprintf(buf,"{W%lu:%lu{x in ???", clone->id[0], clone->id[1]);
		    }

		    add_buf(output,buf);
		    add_buf(output,"\n\r");
	    }
    }

    page_to_char (buf_string(output), ch);
    free_buf(output);
    return;
}


/* VIZZWILDS */
void do_wstat (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    BUFFER *output;
    char buf[MSL];
    char arg[MIL];
    int wuid = 0;

    one_argument (argument, arg);

    if (!str_cmp(arg, ""))
    {
        if ((pWilds = ch->in_wilds) == NULL)
        {
            send_to_char("Stat wilds: You're not in a wilds region. Use 'stat wilds <wuid> to specify.\n\r", ch);
            return;
        }

        pArea = pWilds->pArea;
    }
    else
    {
        if (!is_number(arg))
        {
            send_to_char("Syntax:  stat wilds\n\r", ch);
            send_to_char("         stat wilds <wuid>\n\r", ch);
            return;
        }

        wuid = atoi(arg);

        if ((pWilds = get_wilds_from_uid(NULL, wuid)) == NULL)
        {
            send_to_char("Stat wilds: Couldn't locate that wuid in any loaded area.\n\r", ch);
            return;
        }

        pArea = pWilds->pArea;
    }


    output = new_buf();
    add_buf (output, "{x\n\r[ {Wstat wilds{x ]\n\r");

    sprintf(buf, "\n\r{C*Stats*{x\n\r");
    add_buf(output, buf);


    sprintf(buf, "Wilds uid: ({W%ld{x), Name: '{W%s{x'\n\r", pWilds->uid, pWilds->name);
    add_buf(output, buf);

    sprintf(buf, "Defined in area {W%ld{x, '{W%s{x'\n\r",
                  pArea->anum, pArea->name);
    add_buf(output, buf);

    sprintf(buf, "Dimensions: {W%d{x x {W%d{x ({W%ld{x vrooms)\n\r",
                  pWilds->map_size_x,
                  pWilds->map_size_y,
                  (long)(pWilds->map_size_x * pWilds->map_size_y));
    add_buf(output, buf);

    sprintf(buf, "Repop age: {W%d{x\n\r", pWilds->repop);
    add_buf(output, buf);

    sprintf(buf, "\n\r{C*Current Info*{x\n\r");
    add_buf(output, buf);

    sprintf(buf, "Players: {W%d{x\n\r", pWilds->nplayer);
    add_buf(output, buf);

    sprintf(buf, "Loaded Vrooms: %d\n\r", list_size(pWilds->loaded_vrooms));
    add_buf(output, buf);

    sprintf(buf, "Loaded Mobiles: %d\n\r", pWilds->loaded_mobs);
    add_buf(output, buf);

    sprintf(buf, "Loaded Objects: %d\n\r", pWilds->loaded_objs);
    add_buf(output, buf);

    sprintf(buf, "Current Age: {W%d{x\n\r", pWilds->age);
    add_buf(output, buf);

    page_to_char (buf_string(output), ch);
    free_buf(output);
    return;
}

void do_ostat(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
	char script_cmd[10];
    AFFECT_DATA *paf;
    OBJ_DATA *obj;
    EVENT_DATA *ev;
    ROOM_INDEX_DATA *room;
	TOKEN_DATA *token;
	BUFFER *output = new_buf();

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Stat what?\n\r", ch);
	return;
    }

	if (is_number(arg))
	{
		argument = one_argument(argument, arg);
		if (argument[0] != '\0' && is_number(arg) && is_number(argument))
		{
			if ((obj = idfind_object(atoi(arg), atoi(argument))) == NULL)
			{
				send_to_char("Object not found.\n\r", ch);
				return;
			}
		}
		else
		{
			send_to_char("Syntax: stat obj <name|IDa IDb>",ch);
			return;
		}	
				
	}
	else if ((obj = get_obj_world(ch, argument)) == NULL)
    {
	send_to_char("Object not found.\n\r", ch);
	return;
    }

	/*
	Short desc: a soldier's broadsword Name(s): soldier broad sword broadsword
	Vnum: 4009 Area: Reza Type: weapon
	Long description: A massive broadsword of steel lies, discarded on the ground here.
	Full description:
 	A massive broadsword of steel lies, discarded on the ground here.
	Wear bits: take wield
	Extra bits: glow bless burnproof
	Number: 1/1 Weight: 7
	Level: 65 Cost: 3200 Condition: 100 Timer: 0 Owner: (null)
	In room: 0 In object: (none) Carried by: rezian soldier In mail: No Wear_loc: 16
	Values: 1 9 22 3 8 0 0 0
	Affects strength     by   1, level  65.
	*/


	//TODO: Rework the MXP here.
	/* Some quick checks to set colour object values that differ from index */

	sprintf(buf, "Basic information about %s\n\r", obj->short_descr);
	add_buf(output, buf);

	// Keywords, ID, VNUM, Area
	sprintf(buf, "{%sKeywords{X: %s{X {BID{X: %ld %ld {BVNUM{X: \t<send href='oshow %ld' hint='Show index data for object'>%ld\t</send> ({W%s (%ld){X)\n\r",
	(!str_cmp(obj->name, obj->pIndexData->name)) ? "B" : "Y", obj->name, obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, obj->pIndexData->area->name, obj->pIndexData->area->uid);

	add_buf(output, buf);

	if (obj->loaded_by != NULL && ch->tot_level >= LEVEL_IMMORTAL)
    {
	sprintf(buf, "{YItem loaded by: {x%s\n\r", obj->loaded_by);
	add_buf(output, buf);
    }
	else if (obj->script_created && ch->tot_level >= LEVEL_IMMORTAL)
	{		
		sprintf(buf, "{YItem created by \t<send \"%sdump %ld|%sedit %ld\" hint=\"Dump code for %s %ld|Edit %s %ld\">%s %ld\t</send>.\n\r", 
		script_type_table[obj->created_script_type].prog_command, obj->created_script_vnum,
		script_type_table[obj->created_script_type].prog_command, obj->created_script_vnum,
		script_type_table[obj->created_script_type].prog_type, obj->created_script_vnum, 
		script_type_table[obj->created_script_type].prog_type, obj->created_script_vnum,
		script_type_table[obj->created_script_type].prog_type, obj->created_script_vnum);
		
		add_buf(output, buf);
	}
	
	char created_time[100];
	strftime(created_time, 100, "%a %b %d %X %Z %Y", localtime(&obj->creation_time));
	sprintf(buf, "{BCreated at:{x %s\n\r", created_time);
	add_buf(output, buf);

	// Level, Cost, Condition, Timer, Weight
	sprintf(buf, "{%sLevel{X: %d{X {%sCost{X: %ld{X {%sCondition{X: %d{X {%sTimer{X: %d{X {%sWeight{X: %d{X\n\r",
	(obj->level == obj->pIndexData->level) ? "B" : "Y", obj->level,
	(obj->cost == obj->pIndexData->cost) ? "B" : "Y", obj->cost,
	(obj->condition == obj->pIndexData->condition) ? "B" : "Y", obj->condition,
	(obj->timer == obj->pIndexData->timer) ? "B" : "Y", obj->timer,
	(obj->weight == obj->pIndexData->weight) ? "B" : "Y", obj->weight);
	add_buf(output, buf);

	// Type, Wear flags, Owner
	sprintf(buf, "{%sType{X: %s{X {%sWear{X: %s {%sOwner{X: %s{X\n\r",
	(obj->item_type == obj->pIndexData->item_type) ? "B": "Y", item_name(obj->item_type), 
	(!str_cmp(wear_bit_name(obj->wear_flags), wear_bit_name(obj->pIndexData->wear_flags))) ? "B" : "Y", wear_bit_name(obj->wear_flags), 
	(obj->owner == NULL) ? "B" : "Y", (obj->owner == NULL) ? "None" : obj->owner);
	add_buf(output, buf);

	// Extra flags
	sprintf(buf, "{%sExtra Flags{X: %s\n\r",
	(!str_cmp(bitmatrix_string(extra_flagbank, obj->extra), bitmatrix_string(extra_flagbank, obj->pIndexData->extra))) ? "B" : "Y", bitmatrix_string(extra_flagbank, obj->extra));
	add_buf(output, buf);


		sprintf(buf, "{W\n\rItem Values:");
		add_buf(output, buf);
		print_live_obj_values(obj, output);
	

	if (obj->affected)
	{
		sprintf(buf, "{W\n\rAffects:{X\n\r");
		add_buf(output, buf);
		for (paf = obj->affected; paf != NULL; paf = paf->next)
    	{
			sprintf(buf, "{BAffects{x %-12s {Bby{x %3d{B, level{x %3d",
	    	affect_loc_name(paf->location), paf->modifier,paf->level);
			add_buf(output, buf);
			if (paf->duration > -1)
	    		sprintf(buf,", %d {Bhours.{x\n\r",paf->duration);
			else
	    		sprintf(buf,"{B.{x\n\r");
			add_buf(output, buf);
    	}
	}

	sprintf(buf, "\n\r{WLocation:{X\n\r");
	add_buf(output, buf);

	if (obj->in_wilds != NULL)
	{
		sprintf(buf,"{BIn wilds{X: \t<send href='goxy %d %d %ld'>'%s' (%ld) (at %d, %d)\t</send>{X\n\r", obj->x, obj->y, obj->in_wilds->uid, obj->in_wilds->name, obj->in_wilds->uid, obj->x, obj->y);
		add_buf(output, buf);
	}
	else if (obj->in_room != NULL)
	{
		sprintf(buf, "{BIn room{X: \t<send href='rshow %ld'>%ld\t</send>{X\n\r", obj->in_room->vnum, obj->in_room->vnum);
		add_buf(output, buf);
	}
	if (obj->in_obj != NULL)
	{
		sprintf(buf, "{BIn object{X: \t<send href='stat obj %ld %ld'>%s (%ld)\t</send>{X\n\r", obj->in_obj->id[0], obj->in_obj->id[1], obj->in_obj->short_descr, obj->in_obj->pIndexData->vnum);
		add_buf(output, buf);
	}
	if (obj->carried_by != NULL)
	{
		if (IS_NPC(obj->carried_by))
			sprintf(buf, "{BCarried by{X: \t<send href='stat mob %ld %ld'>%s (%ld)\t</send>{X\n\r", obj->carried_by->id[0], obj->carried_by->id[1], obj->carried_by->name, obj->carried_by->pIndexData->vnum);
		else
			sprintf(buf, "{BCarried by{X: \t<send href='stat char %s'>%s\t</send>{X\n\r", obj->carried_by->name, obj->carried_by->name);
		add_buf(output, buf);
	}
	if (obj->in_mail != NULL)
	{

		if (obj->in_mail->scripted)
		{
			switch(obj->in_mail->orig_script_type)
			{
				case PRG_MPROG:
					sprintf(script_cmd, "mpdump");
					break;
				case PRG_OPROG:
					sprintf(script_cmd, "opdump");
					break;
				case PRG_RPROG:
					sprintf(script_cmd, "rpdump");
					break;
				case PRG_TPROG:
					sprintf(script_cmd, "tpdump");
					break;
				case PRG_APROG:
					sprintf(script_cmd, "apdump");
					break;
				case PRG_IPROG:
					sprintf(script_cmd, "ipdump");
					break;
				case PRG_DPROG:
					sprintf(script_cmd, "dpdump");
					break;
				default: break;
			}
			sprintf(buf, "{BIn mail{X: \t<send href=\"%s %ld\">Scripted to %s - ({W%s %ld{X)\t</send>{X\n\r", script_cmd, obj->in_mail->originating_script, obj->in_mail->recipient, script_cmd, obj->in_mail->originating_script);
			
		}
		else
			sprintf(buf, "{BIn mail{X: From %s to %s\n\r{X", obj->in_mail->sender, obj->in_mail->recipient);
		add_buf(output, buf);
	}

	if (obj->wear_loc != WEAR_NONE)
	{
		sprintf(buf, "{BWear Location{X: %s\n\r", flag_string(wear_loc_strings,obj->wear_loc));
		add_buf(output, buf);
	}

	if (!obj->in_room && !obj->in_obj && !obj->carried_by && !obj->in_mail && !obj->in_wilds)
	{
		sprintf(buf, "Object is currently {Rnowhere{X.\n\r");
		add_buf(output, buf);
	}

	sprintf(buf, "\n\r{WDescriptions:{X\n\r");
	add_buf(output, buf);

	sprintf(buf, "{%sShort Desc{X: %s{X\n\r{%sLong Desc{X: %s{X\n\r{%sFull Desc{X:\n\r %s{X\n\r",
	(!str_cmp(obj->short_descr, obj->pIndexData->short_descr)) ? "B" : "Y", obj->short_descr, 
	(!str_cmp(obj->description, obj->pIndexData->description)) ? "B" : "Y", obj->description, 
	(!str_cmp(obj->full_description, obj->pIndexData->full_description)) ? "B" : "Y", obj->full_description);
	add_buf(output, buf);

    if (obj->extra_descr != NULL || obj->pIndexData->extra_descr != NULL)
    {
	EXTRA_DESCR_DATA *ed;

	add_buf(output, "{BExtra description keywords: {x");

	for (ed = obj->extra_descr; ed != NULL; ed = ed->next)
	{
	    add_buf(output, ed->keyword);
	    if (ed->next != NULL)
	    	add_buf(output, " ");
	}

	for (ed = obj->pIndexData->extra_descr; ed != NULL; ed = ed->next)
	{
	    add_buf(output, ed->keyword);
	    if (ed->next != NULL)
		add_buf(output, " ");
	}

	add_buf(output, "\n\r");
    }

	if (obj->events)
	{
		sprintf(buf, "\n\r{WEvents:{X\n\r");
		add_buf(output, buf);
    	for (ev = obj->events; ev != NULL; ev = ev->next_event) 
		{
			sprintf(buf, "{M* {BEvent {x%-53.52s {B[{x%7.3f{B seconds{B]{x\n\r", ev->args, (float) ev->delay/2);
			add_buf(output, buf);
		}
    }

	if (obj->clone_rooms)
	{
		sprintf(buf, "\n\r{WClone Rooms:{X\n\r");
		add_buf(output, buf);
    	for (room = obj->clone_rooms; room; room = room->next_clone) {
			sprintf(buf, "{M* {CClone {W%ld {C[{W%lu{C:{W%lu{C]{x\n\r", room->source->vnum, room->id[0], room->id[1]);
			add_buf(output, buf);
		}
    }
	if (obj->tokens)
	{
		sprintf(buf, "\n\r{WTokens:{X\n\r");
		add_buf(output, buf);
		for (token = obj->tokens; token != NULL; token = token->next) {
			sprintf(buf, "{M* {CToken \t<send href=\"stat token %lu %lu|token junk %lu %lu\" hint=\"Stat token %lu %lu on %s|Remove token %lu %lu on %s\">{W%s\t</send>{X (\t<send href=\"tshow %ld|tedit %ld\" hint=\"Show token %ld|Edit token %ld\">{W%ld\t<send>{X - ID: {W%lu %lu{X){x\n\r", 
			token->id[0], token->id[1], token->id[0], token->id[1], token->id[0], token->id[1], obj->short_descr, token->id[0], token->id[1], 
			obj->short_descr, token->pIndexData->name, token->pIndexData->vnum, token->pIndexData->vnum, token->pIndexData->vnum, token->pIndexData->vnum,
			token->pIndexData->vnum, token->id[0], token->id[1]);
			add_buf(output, buf);
		}
	}

	if(obj->progs->vars)
	{
		sprintf(buf, "\n\r{WVariables:{X\n\r");
		add_buf(output, buf);
		pstat_variable_list(output, obj->progs->vars);
	}

/*
	sprintf(buf, "Delay   %-6d [%s]\n\r",
		obj->progs->delay,
		obj->progs->target ? obj->progs->target->name : "No target");

	add_buf(output, buf);

	if (obj->pIndexData->progs)
	for(i = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
		iterator_start(&it, obj->pIndexData->progs[slot]);
		while(( oprg = (PROG_LIST *)iterator_nextdata(&it))) {
			sprintf(buf, "[%2d] Trigger [%-8s] Program [%4ld] Phrase [%s]\n\r",
				++i, trigger_name(oprg->trig_type),
				oprg->vnum,
				trigger_phrase(oprg->trig_type,oprg->trig_phrase));
			add_buf(output, buf);
		}
		iterator_stop(&it);
	}

	if(obj->progs->vars)
		pstat_variable_list(ch, obj->progs->vars);
*/
	if( !ch->lines && strlen(output->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(output->string, ch);
	}

	free_buf(output);
}


void do_mstat(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	AFFECT_DATA *paf;
	CHAR_DATA *victim;
	EVENT_DATA *ev;

	one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Stat whom?\n\r", ch);
		return;
	}

	
	if (is_number(arg))
	{
		argument = one_argument(argument, arg);
		if (argument[0] != '\0' && is_number(arg) && is_number(argument))
		{
			if ((victim = idfind_mobile(atoi(arg), atoi(argument))) == NULL)
			{
				send_to_char("They aren't here.\n\r", ch);
				return;
			}
		}
		else
		{
			send_to_char("Syntax: stat mob <name|IDa IDb>",ch);
			return;
		}	
				
	}
	else  if ((victim = get_char_world(ch, argument)) == NULL)
	{
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	sprintf(buf, "{BName:{x %s\n\r", HANDLE(victim));
	send_to_char(buf, ch);

	if (victim->in_wilds == NULL)
	{
		sprintf(buf, "Area uid:{x %ld '%s'\n\r"
					 "{YIn_room:{x %ld '%s'\n\r",
					 victim->in_room->area->uid,
					 victim->in_room->area->name,
					 victim->in_room->vnum,
					 victim->in_room->name);
	}
	else
	{
		sprintf(buf, "{YArea uid:{x %ld '%s'\n\r"
					 "{YIn_wilds:{x %ld '%s', {Yat{x (%d, %d)\n\r",
					 victim->in_room->area->uid,
					 victim->in_room->area->name,
					 victim->in_wilds->uid,
					 victim->in_wilds->name,
					 victim->at_wilds_x,
					 victim->at_wilds_y);
	}

	sprintf(buf, "{BVnum:{x %ld  {BRace:{x %s  {BBody Type:{x %s  {BRoom:{x %ld\n\r",
				 VNUM(victim),
				 race_table[victim->race].name,
				 body_type_info[victim->body_type].name,
				 victim->in_room == NULL ? 0 : victim->in_room->vnum);
	send_to_char(buf, ch);

	sprintf(buf, "{BStr:{x %d{W({x%d{W){x  {BInt:{x %d{W({x%d{W){x  {BWis:{x %d{W({x%d{W){x  {BDex:{x %d{W({x%d{W){x  {BCon:{x %d{W({x%d{W){x\n\r",
				 victim->perm_stat[STAT_STR],
				 get_curr_stat(victim,STAT_STR),
				 victim->perm_stat[STAT_INT],
				 get_curr_stat(victim,STAT_INT),
				 victim->perm_stat[STAT_WIS],
				 get_curr_stat(victim,STAT_WIS),
				 victim->perm_stat[STAT_DEX],
				 get_curr_stat(victim,STAT_DEX),
				 victim->perm_stat[STAT_CON],
				 get_curr_stat(victim,STAT_CON));
	send_to_char(buf, ch);

	sprintf(buf, "{BHp: {x%ld/%ld  {BMana: {x%ld/%ld  {BMove:{x %ld/%ld  {BPractices:{x %d {BTrains:{x %d\n\r",
				 victim->hit, victim->max_hit,
				 victim->mana, victim->max_mana,
				 victim->move, victim->max_move,
				 IS_NPC(victim) ? 0 : victim->practice,
				 IS_NPC(victim) ? 0 : victim->train);
	send_to_char(buf, ch);

	sprintf(buf, "{BLv:{x %d  {BAlign:{x %d  {BGold:{x %ld  {BSilver:{x %ld  {BExp:{x %ld\n\r",
				 victim->tot_level,
				 victim->alignment,
				 victim->gold, victim->silver, victim->exp);
	send_to_char(buf, ch);

	if (!IS_NPC(ch))
	{
		sprintf(buf, "{BKarma:{x %ld  {BPneuma:{x %ld  {BQuestPoints:{x %d{x\n\r",
					 victim->deitypoints, victim->pneuma, victim->questpoints);
		send_to_char(buf, ch);
	}

	sprintf(buf,"{BArmour:{x pierce: %d  bash: %d  slash: %d  magic: %d\n\r",
				GET_AC(victim,AC_PIERCE), GET_AC(victim,AC_BASH),
				GET_AC(victim,AC_SLASH),  GET_AC(victim,AC_EXOTIC));
	send_to_char(buf,ch);

	sprintf(buf,"{BHitroll:{x %d  {BDamroll:{x %f {W[{x%d{W]{x {BSize:{x %s {BPosition:{x %s {BWimpy:{x %d\n\r",
				GET_HITROLL(victim),
				40*log(GET_DAMROLL(victim)),
				GET_DAMROLL(victim),
				size_table[victim->size].name, position_table[victim->position].name,
				victim->wimpy);
	send_to_char(buf, ch);

	sprintf(buf, "{BIdle:{x %d minutes ", victim->timer);
	send_to_char(buf, ch);

	if (IS_NPC(victim))
	{
		if( victim->damage.bonus > 0 )
			sprintf(buf, "{BDamage:{x %dd%d+%d  {BMessage:{x  %s\n\r",
						 victim->damage.number,victim->damage.size,victim->damage.bonus,
						 attack_table[victim->dam_type].noun);
		else
			sprintf(buf, "{BDamage:{x %dd%d  {BMessage:{x  %s\n\r",
						 victim->damage.number,victim->damage.size,
						 attack_table[victim->dam_type].noun);
		send_to_char(buf,ch);
	}

	if (IS_NPC(victim) && victim->hunting != NULL)
	{
		sprintf(buf, "Hunting victim: %s (%s)\n\r",
					 IS_NPC(victim->hunting) ? victim->hunting->short_descr	: victim->hunting->name,
					 IS_NPC(victim->hunting) ? "MOB" : "PLAYER");
		send_to_char(buf, ch);
	}

	sprintf(buf, "{BFighting:{x %s\n\r",
	victim->fighting ? victim->fighting->name : "(none)");
	send_to_char(buf, ch);

	if (!IS_NPC(victim))
	{
		sprintf(buf, "{BThirst:{x %d  {BHunger:{x %d  {BFull:{x %d  {BDrunk:{x %d\n\r",
					 victim->pcdata->condition[COND_THIRST],
					 victim->pcdata->condition[COND_HUNGER],
					 victim->pcdata->condition[COND_FULL],
					 victim->pcdata->condition[COND_DRUNK]);
		send_to_char(buf, ch);
	}

	sprintf(buf, "{BCarry number:{x %d  {BCarry weight:{x %ld\n\r",
	victim->carry_number, get_carry_weight(victim) / 10);
	send_to_char(buf, ch);

	if (!IS_NPC(victim))
	{
		sprintf(buf, "{BAge:{x %d  {BPlayed:{x %d  {BTimer:{x %d  {BCreated:{x %s{x",
					 get_age(victim),
					 (int) (victim->played + current_time - victim->logon) / 3600,
					 victim->timer,
					 ((char *) ctime((time_t *)&victim->pcdata->creation_date)));
		send_to_char(buf, ch);
	}

	sprintf(buf, "{BAct :{x %s\n\r",bitmatrix_string(IS_NPC(victim)?act_flagbank:plr_flagbank, victim->act));
	send_to_char(buf,ch);
/*
	sprintf(buf, "{BAct2:{x %s\n\r",act_bit_name((IS_NPC(victim) ? 2 : 4), victim->act2));
	send_to_char(buf,ch);
*/
	if (victim->comm)
	{
		sprintf(buf,"{BComm:{x %s\n\r",comm_bit_name(victim->comm));
		send_to_char(buf,ch);
	}

	if (IS_NPC(victim) && victim->off_flags)
	{
		sprintf(buf, "{BOffense:{x %s\n\r",off_bit_name(victim->off_flags));
		send_to_char(buf,ch);
	}

	if (victim->imm_flags)
	{
		sprintf(buf, "{BImmune:{x %s\n\r",imm_bit_name(victim->imm_flags));
		send_to_char(buf,ch);
	}

	if (victim->res_flags)
	{
		sprintf(buf, "{BResist:{x %s\n\r", imm_bit_name(victim->res_flags));
		send_to_char(buf,ch);
	}

	if (victim->vuln_flags)
	{
		sprintf(buf, "{BVulnerable:{x %s\n\r", imm_bit_name(victim->vuln_flags));
		send_to_char(buf,ch);
	}

	if (victim->affected_by[0])
	{
		sprintf(buf, "{BAffected by{x %s\n\r", affect_bit_name(victim->affected_by[0]));
		send_to_char(buf,ch);
	}

	if (victim->affected_by[1])
	{
		sprintf(buf, "{BAffected2 by{x %s\n\r", affect2_bit_name(victim->affected_by[1]));
		send_to_char(buf,ch);
	}

	sprintf(buf, "{BMaster:{x %s  {BLeader:{x %s  {BPet:{x %s  {B%s:{x %s  {BCart:{x %s\n\r",
				 victim->master ? victim->master->name : "(none)",
				 victim->leader ? victim->leader->name : "(none)",
				 victim->pet ? victim->pet->name : "(none)",
				 (victim->rider?"Rider":"Mount"),
				 (victim->rider?victim->rider->name:(victim->mount?victim->mount->name : "(none)")),
				 victim->pulled_cart ? victim->pulled_cart->short_descr : "(none)");
	send_to_char(buf, ch);

	if (victim->hired_to)
	{
		char hired_time[100];
		strftime(hired_time, 100, "%a %b %d %X %Z %Y", localtime(&victim->hired_to));
		sprintf(buf, "{BHired to:{x %s\n\r", hired_time);
		send_to_char(buf, ch);
	}

	if (!IS_NPC(victim))
	{
		sprintf(buf, "{BSecurity:{x %d.\n\r", victim->pcdata->security);
		send_to_char(buf, ch);
	}

	if (IS_NPC(victim))
	{
		sprintf(buf, "{BShort description:{x %s\n\r{BLong description:{x %s",
					 victim->short_descr,
					 victim->long_descr[0] != '\0' ? victim->long_descr : "(none)\n\r");
		send_to_char(buf, ch);
	}

	if (!IS_NPC(victim))
	{
		sprintf(buf, "{BSubclasses: Mage:{x %s {BCleric:{x %s {BThief:{x %s {BWarrior:{x %s\n\r",
					 victim->pcdata->sub_class_mage < 0 ? "none" : sub_class_table[victim->pcdata->sub_class_mage].name[victim->sex],
					 victim->pcdata->sub_class_cleric < 0 ? "none" : sub_class_table[victim->pcdata->sub_class_cleric].name[victim->sex],
					 victim->pcdata->sub_class_thief < 0 ? "none" : sub_class_table[victim->pcdata->sub_class_thief].name[victim->sex],
					 victim->pcdata->sub_class_warrior < 0 ? "none" : sub_class_table[victim->pcdata->sub_class_warrior].name[victim->sex]);
		send_to_char(buf, ch);
		if (IS_REMORT(victim))
		{
			sprintf(buf, "{BRemort Subclasses: Mage:{x %s {BCleric:{x %s {BThief:{x %s {BWarrior:{x %s\n\r",
						 victim->pcdata->second_sub_class_mage < 0 ? "none" : sub_class_table[victim->pcdata->second_sub_class_mage].name[victim->sex],
						 victim->pcdata->second_sub_class_cleric < 0 ? "none" : sub_class_table[victim->pcdata->second_sub_class_cleric].name[victim->sex],
						 victim->pcdata->second_sub_class_thief < 0 ? "none" : sub_class_table[victim->pcdata->second_sub_class_thief].name[victim->sex],
						 victim->pcdata->second_sub_class_warrior < 0 ? "none" : sub_class_table[victim->pcdata->second_sub_class_warrior].name[victim->sex]);
			send_to_char(buf, ch);
		}
	}

	for (paf = victim->affected; paf != NULL; paf = paf->next) if(!paf->custom_name)
	{
		sprintf(buf, "{C* {BLevel {W%3d {Baffect {x%-20.20s{B modifies {x%-12s{B by {x%2d{B for {x%2d{B hours with bits {x%s{B on slot {x%s\n\r",
					 paf->level,
					 skill_table[(int) paf->type].name,
					 affect_loc_name(paf->location),
					 paf->modifier,
					 paf->duration,
					 affects_bit_name(paf->bitvector, paf->bitvector2),
					 flag_string(wear_loc_names, paf->slot));
		send_to_char(buf, ch);
	}

	for (paf = victim->affected; paf != NULL; paf = paf->next) if(paf->custom_name)
	{
		sprintf(buf, "{C* {BLevel {W%3d {Baffect {x%-20.20s{B modifies {x%-12s{B by {x%2d{B for {x%2d{B hours with bits {x%s{B on slot {x%s\n\r",
					 paf->level,
					 paf->custom_name,
					 affect_loc_name(paf->location),
					 paf->modifier,
					 paf->duration,
					 affects_bit_name(paf->bitvector, paf->bitvector2),
					 flag_string(wear_loc_names, paf->slot));
		send_to_char(buf, ch);
	}

	if (!IS_NPC(victim) && victim->pcdata->commands != NULL)
	{
		send_to_char("{BGranted commands:{x\n\r", ch);

		int i = 0;
		for (COMMAND_DATA *cmd = victim->pcdata->commands; cmd != NULL; cmd = cmd->next)
		{
			i++;
			sprintf(buf, "%-15s", cmd->name);
			if (i % 4 == 0)
				strcat(buf, "\n\r");

			send_to_char(buf, ch);
		}
	}

	if( IS_NPC(victim) && IS_VALID(victim->crew) )
	{
		send_to_char("{CCrew Data:{x\n\r", ch);

		sprintf(buf, " {CScouting{c:   {x%d\n\r", victim->crew->scouting);
		send_to_char(buf, ch);

		sprintf(buf, " {CGunning{c:    {x%d\n\r", victim->crew->gunning);
		send_to_char(buf, ch);

		sprintf(buf, " {COarring{c:    {x%d\n\r", victim->crew->oarring);
		send_to_char(buf, ch);

		sprintf(buf, " {CMechanics{c:  {x%d\n\r", victim->crew->mechanics);
		send_to_char(buf, ch);

		sprintf(buf, " {CNavigation{c: {x%d\n\r", victim->crew->navigation);
		send_to_char(buf, ch);

		sprintf(buf, " {CLeadership{c: {x%d\n\r", victim->crew->leadership);
		send_to_char(buf, ch);
	}

	for (ev = victim->events; ev != NULL; ev = ev->next_event)
	{
		sprintf(buf, "{M* {BEvent {x%-53.52s {B[{x%7.3f{B seconds{B]{x\n\r", ev->args, (float) ev->delay/2);
		send_to_char(buf, ch);
	}

/*
	if( !ch->lines && strlen(output->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(output->string, ch);
	}

	free_buf(output);
*/
}


void do_tstat(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], buf[MSL], buf2[MSL], arg2[MSL], arg3[MSL];
    TOKEN_DATA *token = NULL;
	TOKEN_DATA *tokens = NULL;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *object = NULL;
	ROOM_INDEX_DATA *room = NULL;
    int i;
    long vnum = 0, count;
	bool id_lookup = false;

    BUFFER *buffer;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0') {
	send_to_char("Syntax:  stat token <mob name|obj name|room> [token vnum]\n\r", ch);
	return;
    }

	if (is_number(arg))
	{
		if (arg[0] != '\0' && is_number(arg) && is_number(arg2))
		{
			if ((token = idfind_token(atoi(arg), atoi(arg2))) == NULL)
			{
				send_to_char("No such token\n\r", ch);
				return;
			}
			else
			{
				id_lookup = true;
			}
		}
		else
		{
			send_to_char("Syntax:  tpstat <mobile name|object name|room|ida idb> [[<count>.]<token vnum>]",ch);
			return;
		}	
				
	} else if (!str_cmp(arg,"mob")) {
		if ((victim = get_char_world(NULL, arg2)) == NULL) {
			send_to_char("Mobile not found.\n\r", ch);
			return;
		}
		tokens = victim->tokens;
		count = number_argument(argument, arg3);
	} else if(!str_cmp(arg, "obj")) {
		if ((object = get_obj_world(NULL, arg2)) == NULL) {
			send_to_char("Object not found.\n\r", ch);
			return;
		}

		tokens = object->tokens;
		count = number_argument(argument, arg3);
	} else if(!str_cmp(arg, "room")) {
		room = ch->in_room;
		tokens = room->tokens;
		count = number_argument(arg2, arg3);
	} else if (!id_lookup) {
		send_to_char("Syntax:  stat token <mob name|obj name|room> [token vnum]\n\r", ch);
		return;
	}

    if (arg3[0] != '\0' && !id_lookup) {
		vnum = atol(arg3);

		if (get_token_index(vnum) == NULL) {
			send_to_char("That token vnum does not exist.\n\r", ch);
			return;
		}

		if (victim  && !(token= get_token_char(victim, vnum, count))) {
			act("$N doesn't have that token.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
			return;
		}

		if (object && !(token = get_token_obj(object, vnum, count))) {
			act("$p doesn't have that token.", ch, NULL, NULL, object, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
			return;
		}

		if (room && !(token = get_token_room(room, vnum, count))) {
			send_to_char("The room doesn't have that token.", ch);
			return;
		}
    }

    buffer = new_buf();

    sprintf(buf, "{Y%-9s %-20s %-6s ", "Vnum", "Token Name", "Timer");
    add_buf(buffer, buf);

    for (i = 0; i < MAX_TOKEN_VALUES; i++) {
		sprintf(buf2, "Value%d", i);
		sprintf(buf, "%-20s", buf2);
		add_buf(buffer, buf);
    }

    add_buf(buffer, "{x\n\r");

    if (token == NULL) 
	{
		if (tokens == NULL)
	    	add_buf(buffer, "None.\n\r");
		else
		{
	    	for (token = tokens; token != NULL; token = token->next) 
			{
				buf[0] = '\0';
			sprintf(buf2, "{Y[{x%7ld{Y]{x %-20.20s %-6d ",
				token->pIndexData->vnum, token->name, token->timer);

			strcat(buf, buf2);
			for (i = 0; i < MAX_TOKEN_VALUES; i++) {
		    	sprintf(buf2, "{b[{x%-7.7s{b]{x %-9ld ", token_index_getvaluename(token->pIndexData, i), token->value[i]);
		    	strcat(buf, buf2);
			}

			strcat(buf, "\n\r");
			add_buf(buffer, buf);
	    	}
		}
    }
    else
    {
	buf[0] = '\0';
	sprintf(buf2, "{Y[{x%7ld{Y]{x %-20.20s %-6d ",
		token->pIndexData->vnum, token->name, token->timer);

	strcat(buf, buf2);
	for (i = 0; i < MAX_TOKEN_VALUES; i++) {
	    sprintf(buf2, "{b[{x%-7.7s{b]{x %-9ld ", token_index_getvaluename(token->pIndexData, i), token->value[i]);
	    strcat(buf, buf2);
	}

	strcat(buf, "\n\r");

	add_buf(buffer, buf);
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


void do_vnum(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;

    string = one_argument(argument,arg);

    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  vnum obj <name>\n\r",ch);
	send_to_char("  vnum mob <name>\n\r",ch);
	send_to_char("	vnum token <name>\n\r", ch);
	//send_to_char("  vnum skill <skill or spell>\n\r",ch);
	return;
    }

    if (!str_cmp(arg,"obj"))
    {
	do_function(ch, &do_ofind, string);
 	return;
    }

    if (!str_cmp(arg,"mob") || !str_cmp(arg,"char"))
    {
	do_function(ch, &do_mfind, string);
	return;
    }

	if (!str_cmp(arg, "token") || !str_cmp(arg, "tok"))
	{
	do_function(ch, &do_tfind, string);
	return;
	}

    /*
    if (!str_cmp(arg,"skill") || !str_cmp(arg,"spell"))
    {
	do_function (ch, &do_slookup, string);
	return;
    }
    */
    /* do both */
	send_to_char("Mobiles:\n\r",ch);
    do_function(ch, &do_mfind, argument);
	send_to_char("\n\rObjects:\n\r",ch);
    do_function(ch, &do_ofind, argument);
	send_to_char("\n\rTokens:\n\r", ch);
	do_function(ch, &do_tfind, argument);
}


void do_mfind(CHAR_DATA *ch, char *argument)
{
    /* extern long top_mob_index; */
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    MOB_INDEX_DATA *pMobIndex;
    /* long vnum; */
    int nMatch, iHash;
    bool fAll;
    bool found;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Find whom?\n\r", ch);
	return;
    }

    if (strlen(arg) < 2) {
	send_to_char("Your search must be at least 2 characters long.\n\r", ch);
	return;
    }

    fAll	= false; /* !str_cmp(arg, "all"); */
    found	= false;
    nMatch	= 0;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (pMobIndex = mob_index_hash[iHash]; pMobIndex != NULL; pMobIndex = pMobIndex->next) {
			nMatch++;
			if (fAll || is_name(argument, pMobIndex->player_name)) {
				found = true;
				sprintf(buf, "[%5ld] %s\n\r",
					pMobIndex->vnum, pMobIndex->short_descr);
				send_to_char(buf, ch);
			}
		}
	}

    if (!found)
	send_to_char("No mobiles by that name.\n\r", ch);
}


void do_ofind(CHAR_DATA *ch, char *argument)
{
    /* extern long top_obj_index; */
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    /* long vnum; */
    int nMatch, iHash;
    bool fAll;
    bool found;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Find what?\n\r", ch);
	return;
    }

    fAll	= false; /* !str_cmp(arg, "all"); */
    found	= false;
    nMatch	= 0;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (pObjIndex = obj_index_hash[iHash]; pObjIndex != NULL; pObjIndex = pObjIndex->next) {
			nMatch++;
			if (fAll || is_name(argument, pObjIndex->name)) {
				found = true;
				sprintf(buf, "[%5ld] %s\n\r",
					pObjIndex->vnum, pObjIndex->short_descr);
				send_to_char(buf, ch);
			}
		}
	}

    if (!found)
	send_to_char("No objects by that name.\n\r", ch);
}

void do_tfind(CHAR_DATA *ch, char *argument)
{
    /* extern long top_mob_index; */
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    TOKEN_INDEX_DATA *pTokIndex;
    /* long vnum; */
    int nMatch, iHash;
    bool fAll;
    bool found;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Find what?\n\r", ch);
	return;
    }

    if (strlen(arg) < 2) {
	send_to_char("Your search must be at least 2 characters long.\n\r", ch);
	return;
    }

    fAll	= false; /* !str_cmp(arg, "all"); */
    found	= false;
    nMatch	= 0;

	for (iHash = 0; iHash < MAX_KEY_HASH; iHash++) {
		for (pTokIndex = token_index_hash[iHash]; pTokIndex != NULL; pTokIndex = pTokIndex->next) {
			nMatch++;
			if (fAll || is_name(argument, pTokIndex->name)) {
				found = true;
				sprintf(buf, "[%5ld] %s\n\r",
					pTokIndex->vnum, pTokIndex->name);
				send_to_char(buf, ch);
			}
		}
	}
	

    if (!found)
	send_to_char("No tokens by that name.\n\r", ch);
}


void do_rwhere(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];
    BUFFER *buffer;
    ROOM_INDEX_DATA *room;
    bool found;
    int number, max_found;
    int hash;

    found = false;
    number = 0;
    max_found = 200;

    buffer = new_buf();

    if (argument[0] == '\0')
    {
	send_to_char("Find what?\n\r",ch);
	return;
    }

    for (hash = 0; hash < MAX_KEY_HASH; hash++)
    {
	for (room = room_index_hash[hash]; room != NULL; room = room->next)
	{
	    if (can_see_room(ch, room)
	    &&  is_name(argument, room->name))
	    {
		if (number >= max_found)
		    break;

		number++;
		found = true;
		sprintf(buf, "{Y%3d){x %s (vnum {W%ld{x)\n\r", number,
		    room->name, room->vnum);
		buf[0] = UPPER(buf[0]);
		add_buf(buffer,buf);
	    }
	}

	if (number >= max_found)
	    break;
    }

    if (!found)
        send_to_char("Nothing like that in heaven or earth.\n\r", ch);
    else
        page_to_char(buf_string(buffer),ch);

    free_buf(buffer);
}


void do_owhere(CHAR_DATA *ch, char *argument)
{
    char buf[MIL*2];
    BUFFER *buffer;
    OBJ_DATA *obj;
    OBJ_DATA *in_obj;
    bool found;
    int number = 0, max_found;
    ITERATOR it;

    found = false;
    number = 0;
    max_found = 200;

    buffer = new_buf();

    if (argument[0] == '\0')
    {
	send_to_char("Find what?\n\r",ch);
	return;
    }

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (!can_see_obj(ch, obj) || !is_name(argument, obj->name))
            continue;

        found = true;
        number++;

        for (in_obj = obj; in_obj->in_obj != NULL; in_obj = in_obj->in_obj) ;

        if (in_obj->carried_by != NULL && can_see(ch,in_obj->carried_by) && in_obj->carried_by->in_room != NULL) 
		{
			if (IS_NPC(in_obj->carried_by))
			{
				sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" hint=\"Show information for this object||***DANGER***|Purge this object\">{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %ld|oedit %ld\" hint=\"Show index for %s|Edit %s\">%s\t</send> is carried by \t<send href=\"stat mob %ld %ld|mshow %ld|medit %ld\" hint=\"View info for %s|Show index for %s|Edit %s\">%s\t</send> [\t<send href=\"rshow %ld|redit %ld|goto %ld\" hint=\"View room %ld|Edit room %ld|Go to room %ld\">Room %ld\t</send>]\n\r",
				number, obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, obj->short_descr, obj->short_descr,
				obj->short_descr, (obj->in_obj != NULL) ? obj->in_obj->carried_by->id[0] : obj->carried_by->id[0], (obj->in_obj != NULL) ? obj->in_obj->carried_by->id[1] : obj->carried_by->id[1], 
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->pIndexData->vnum : obj->carried_by->pIndexData->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->pIndexData->vnum : obj->carried_by->pIndexData->vnum,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->short_descr : obj->carried_by->short_descr, (obj->in_obj != NULL) ? obj->in_obj->carried_by->short_descr : obj->carried_by->short_descr,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->short_descr : obj->carried_by->short_descr, (obj->in_obj != NULL) ? obj->in_obj->carried_by->short_descr : obj->carried_by->short_descr,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, 
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum);
				add_buf(buffer, buf);
			}
			else
			{
				sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" hint=\"Show information for this object||***DANGER***|Purge this object\">{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %ld|oedit %ld\" hint=\"Show index for %s|Edit %s\">%s\t</send> is carried by \t<send href=\"stat char %s\">%s\t</send> [\t<send href=\"rshow %ld|redit %ld|goto %ld\" hint=\"View room %ld|Edit room %ld|Go to room %ld\">Room %ld\t</send>]\n\r",
				number, obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, 
				obj->short_descr, obj->short_descr, obj->short_descr, (obj->in_obj != NULL) ? obj->in_obj->carried_by->name : obj->carried_by->name, (obj->in_obj != NULL) ? obj->in_obj->carried_by->name : obj->carried_by->name, 
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, 
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum, (obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum,
				(obj->in_obj != NULL) ? obj->in_obj->carried_by->in_room->vnum : obj->carried_by->in_room->vnum);
				add_buf(buffer, buf);
			}
		} 
		else if (in_obj->in_room != NULL && can_see_room(ch,in_obj->in_room)) 
		{
			sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" hint=\"Show information for this object||***DANGER***|Purge this object\">{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %ld|oedit %ld\" hint=\"Show index for %s|Edit %s\">%s\t</send> is in %s [\t<send href=\"rshow %ld|redit %ld|goto %ld\" hint=\"View room %ld|Edit room %ld|Go to room %ld\">Room %ld\t</send>]\n\r",
			number, obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, 
			obj->short_descr, obj->short_descr, obj->short_descr, in_obj->in_room->name, in_obj->in_room->vnum, in_obj->in_room->vnum, 
			in_obj->in_room->vnum, in_obj->in_room->vnum, in_obj->in_room->vnum, in_obj->in_room->vnum, in_obj->in_room->vnum);
			add_buf(buffer, buf);
			
		} 
		else if (in_obj->in_mail != NULL) 
		{
			sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" hint=\"Show information for this object||***DANGER***|Purge this object\">{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %ld|oedit %ld\" hint=\"Show index for %s|Edit %s\">%s\t</send> is in a mail package\n\r",
			number, obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, obj->short_descr, obj->short_descr, obj->short_descr);
			add_buf(buffer, buf);
		} 
		else 
		{
			sprintf(buf, "{Y%3d) {WID{X: [\t<send href=\"stat obj %ld %ld|||purge obj %ld %ld\" hint=\"Show information for this object||***DANGER***|Purge this object\">{W%ld %ld{X\t</send>]{x \t<send href=\"oshow %ld\" hint=\"Show index of obj %ld\">%s\t</send> is somewhere\n\r",
			number, obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->id[0], obj->id[1], obj->pIndexData->vnum, obj->pIndexData->vnum, obj->short_descr);
			add_buf(buffer, buf);

		}
/*
        buf[0] = UPPER(buf[0]);
        add_buf(buffer,buf);
*/
        if (number >= max_found)
            break;
    }

    if (!found)
	{
        send_to_char("Nothing like that in heaven or earth.\n\r", ch);
	}
	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buf_string(buffer), ch);
	}

    free_buf(buffer);
}


void do_mwhere(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    BUFFER *buffer;
    CHAR_DATA *victim;
    bool found;
    int count = 0;
    ITERATOR vit;

    if (argument[0] == '\0') {
		DESCRIPTOR_DATA *d;

		buffer = new_buf();
		for (d = descriptor_list; d != NULL; d = d->next) {
		    if (d->character != NULL && d->connected == CON_PLAYING &&
		    	d->character->in_room != NULL && can_see(ch,d->character) &&
		    	can_see_room(ch,d->character->in_room)) {
                if (d->character->in_wilds == NULL) {
                    /* Victim is in a normal room, so report the vnum.*/
                    victim = d->character;
                    count++;

					if (d->original != NULL)
						sprintf(buf,"{Y%3d) {WID{X: [{W%ld %ld{X]{x %s (in the body of %s) is in %s [%ld]\n\r",
							count, (long)victim->id[0], (long)victim->id[1], d->original->name,victim->short_descr,
							victim->in_room->name,victim->in_room->vnum);
					else
						sprintf(buf,"{Y%3d) {WID{X: [{W%ld %ld{X]{x %s is in %s [%ld]\n\r",
							count, (long)victim->id[0], (long)victim->id[1], victim->name,victim->in_room->name,
							victim->in_room->vnum);
					add_buf(buffer,buf);
			    } else {
                    /* Victim is in a virtual room, so report the location and position.*/
                    victim = d->character;
                    count++;

                    if (d->original != NULL)
                        sprintf(buf,"{Y%3d) {WID{X: [{W%ld %ld{X]{x %s (in the body of %s) is in wilds '%s', %s (%ld, %ld)\n\r",
							count, (long)victim->id[0], (long)victim->id[1], d->original->name,victim->short_descr,
							victim->in_room->name,
							victim->in_wilds->name, victim->in_room->x, victim->in_room->y);
                    else
						sprintf(buf,"{Y%3d) {WID{X: [{W%ld %ld{X]{x %s is in wilds '%s', %s (%ld, %ld)\n\r",
							count, (long)victim->id[0], (long)victim->id[1], victim->name,
							victim->in_wilds->name,
							victim->in_room->name,
							victim->in_room->x, victim->in_room->y);

					add_buf(buffer,buf);
				}
			}
		}

		page_to_char(buf_string(buffer),ch);
		free_buf(buffer);
		return;
    }

    /* all the mobs without a room */
    if (!str_cmp(argument,"nowhere")) {
        buffer = new_buf();
        found=false;
        count=0;

		iterator_start(&vit, loaded_chars);
		while(( victim = (CHAR_DATA *)iterator_nextdata(&vit)))
		{
            if (victim->in_room==NULL) {
                found = true;
                count++;
                sprintf(buf, "{Y%3d) {WID{X: [{W%ld %ld{X]{x [%5ld] %-28s %lx\n\r", count,
					(long)victim->id[0], (long)victim->id[1],
                    IS_NPC(victim) ? victim->pIndexData->vnum : 0,
                    IS_NPC(victim) ? victim->short_descr : victim->name,
                    (long)victim);
                add_buf(buffer,buf);
            }
		}
		iterator_stop(&vit);

        if (found)
            page_to_char(buf_string(buffer),ch);
        else
            send_to_char("No mobs without rooms found.\n\r",ch);
        free_buf(buffer);
        return;
    }

    /* ok - must be a mobname */
    found = false;
    buffer = new_buf();

	iterator_start(&vit, loaded_chars);
	while(( victim = (CHAR_DATA *)iterator_nextdata(&vit)))
	{
		if (victim->in_room != NULL &&
			is_name(argument, victim->name)) {
			found = true;
			count++;
			sprintf(buf, "{Y%3d) {WID{X: [{W%ld %ld{X]{x [%5ld] %-28s [%5ld] %s\n\r", count,
			(long)victim->id[0], (long)victim->id[1],
			IS_NPC(victim) ? victim->pIndexData->vnum : 0,
			IS_NPC(victim) ? victim->short_descr : victim->name,
			victim->in_room->vnum,
			victim->in_room->name);
			add_buf(buffer,buf);
		}
	}
	iterator_stop(&vit);

    if (!found)
		act("You didn't find any $T.", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_CHAR, NULL, NULL);
    else
    	page_to_char(buf_string(buffer),ch);

    free_buf(buffer);
}


void do_reboo(CHAR_DATA *ch, char *argument)
{
    send_to_char("If you want to REBOOT, spell it out.\n\r", ch);
}


void do_reckonin(CHAR_DATA *ch, char *argument)
{
    send_to_char("This command cannot be abbreviated!\n\r", ch);
}


void do_reboot(CHAR_DATA *ch, char *argument)
{
    int mins;
    int down_time;
    char buf[MSL];
    char arg[MSL];
    char arg2[MSL];
//	char reason[MSL];
    struct tm *reboot_time;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
//	argument = one_argument(argument, reason);

    if (reboot_timer > 0)
    {
	reboot_timer = 0;
	down_timer = 0;
	free_string(reboot_by);
	free_string(reboot_reason);
	gecho("{WREBOOT COUNTDOWN DEACTIVATED.{x\n\r");
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Reboot in how many minutes?\n\r", ch);
	return;
    }

    if (arg2[0] == '\0')
    {
	send_to_char("What is the downtime?\n\r", ch);
	return;
    }

    if (!is_number(arg) || !is_number(arg2))
    {
        send_to_char("Invalid argument given.\n\r", ch);
	return;
    }

    mins = atoi(arg);
    if (mins < 1 || mins > 9999)
    {
	send_to_char("Range for reboot time is 1 to 30 minutes.\n\r", ch);
	return;
    }

    down_time = atoi(arg2);
    if (down_time < 1 || down_time > 9999)
    {
	send_to_char("Range for downtime is 1 to 9999 minutes.\n\r", ch);
	return;
    }

    sprintf(buf, "{WSet reboot timer for %d minutes.{x\n\r", mins);
    send_to_char(buf, ch);


// Set the global reboot_reason to the rest of the argument (or empty string)
	if (reboot_reason)
    	free_string(reboot_reason);
	reboot_reason = str_dup(argument[0] != '\0' ? argument : "");

    reboot_time = localtime(&current_time);
    reboot_time->tm_min += mins;

    reboot_timer = mktime(reboot_time);
    down_timer = down_time;
    reboot_by = str_dup(ch->name);
}


void do_shutdow(CHAR_DATA *ch, char *argument)
{
    send_to_char("If you want to SHUTDOWN, spell it out.\n\r", ch);
}


void do_shutdown(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d,*d_next;
    CHAR_DATA *vch, *tch;
    TOKEN_DATA *token;
    ITERATOR cit, tit;
	char shutdown_information[MAX_STRING_LENGTH];
	char shutdown_reason[MAX_INPUT_LENGTH];

	bool reboot = false;
	
	// Did the shutdown happen while the reboot timer was active or reboot was set by the update handler?
	if (reboot_shutdown || reboot_timer > 0)
		reboot = true;

	if (IS_NULLSTR(argument))
		shutdown_reason[0] = '\0';
	else
		sprintf(shutdown_reason, " for: \"%s\"", argument);
	
	sprintf(shutdown_information, "%s by %s%s at %s", reboot ? "Reboot" : "Shutdown", ch->name, shutdown_reason, (char *) ctime(&current_time));

	if (!reboot || down_timer > 1)
		append_file(ch, SHUTDOWN_FILE, shutdown_information);

	append_file(ch, MAINTENANCE_FILE, shutdown_information);

    //strcat(shutdown_information, "\n\r");
    do_function(ch, &do_echo, shutdown_information);

    /* remove any PURGE_REBOOT tokens on any characters */
	iterator_start(&cit, loaded_chars);
	while(( tch = (CHAR_DATA *)iterator_nextdata(&cit)))
	{
		iterator_start(&tit, tch->ltokens);
		while(( token = (TOKEN_DATA *)iterator_nextdata(&tit)))
		{
			if (IS_SET(token->flags, TOKEN_PURGE_REBOOT)) {
				p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

				sprintf(buf, "char update: token %s(%ld) char %s(%ld) was purged because of reboot",
					token->name, token->pIndexData->vnum, HANDLE(tch), IS_NPC(tch) ? tch->pIndexData->vnum : 0);
				log_string(buf);
				token_from_char(token);
				free_token(token);
			}
		}

		iterator_stop(&tit);
	}
	iterator_stop(&cit);

    merc_down = true;
    for (d = descriptor_list; d != NULL; d = d_next) {
		d_next = d->next;
		if( d->connected == CON_PLAYING )
		{
			vch = d->original ? d->original : d->character;
			if (IS_VALID(vch)) {
				/* save their shift */
				if (ch->shifted != SHIFTED_NONE) {
					shift_char(ch, true);
					ch->shifted = IS_VAMPIRE(ch) ? SHIFTED_WEREWOLF : SHIFTED_SLAYER;
				}

				save_char_obj(vch);
			}
		}

		close_socket(d);
    }

    write_churches_new();
    save_helpfiles_new();
    write_mail();
    write_chat_rooms();
    write_gq();
//    write_permanent_objs();
    persist_save();
    save_projects();
    save_instances();
}


void do_snoop(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;
    char buf[MAX_STRING_LENGTH];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Snoop whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim->desc == NULL)
    {
	send_to_char("No descriptor to snoop.\n\r", ch);
	return;
    }

    if (victim == ch)
    {
	send_to_char("Cancelling all snoops.\n\r", ch);
	wiznet("$N stops being such a snoop.",
		ch,NULL,WIZ_SNOOPS,WIZ_SECURE,get_staff_rank(ch));
	for (d = descriptor_list; d != NULL; d = d->next)
	{
	    if (d->snoop_by == ch->desc)
		d->snoop_by = NULL;
	}
	return;
    }

    if (victim->desc->snoop_by != NULL)
    {
	send_to_char("Busy already.\n\r", ch);
	return;
    }

    if (!is_room_owner(ch,victim->in_room) && ch->in_room != victim->in_room
    &&  room_is_private(victim->in_room, ch))
    {
        send_to_char("That character is in a private room.\n\r",ch);
        return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch))
    {
	send_to_char("You failed.\n\r", ch);
	return;
    }

    if (ch->desc != NULL)
    {
	for (d = ch->desc->snoop_by; d != NULL; d = d->snoop_by)
	{
	    if (d->character == victim || d->original == victim)
	    {
		send_to_char("No snoop loops.\n\r", ch);
		return;
	    }
	}
    }

    victim->desc->snoop_by = ch->desc;
    sprintf(buf,"$N starts snooping on %s",
	(IS_NPC(ch) ? victim->short_descr : victim->name));
    wiznet(buf,ch,NULL,WIZ_SNOOPS,WIZ_SECURE,get_staff_rank(ch));
    act("Now snooping $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
}


void do_switch(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Switch into whom?\n\r", ch);
	return;
    }

    if (ch->desc == NULL)
	return;

    if (ch->desc->original != NULL)
    {
	send_to_char("You are already switched.\n\r", ch);
	return;
    }

    if( ch->desc->editor != ED_NONE )
    {
	send_to_char("You are currently in OLC.  Please exit before switching.\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim == ch)
    {
	send_to_char("That would be pointless.\n\r", ch);
	return;
    }

    if (!IS_NPC(victim))
    {
	send_to_char("You can only switch into mobiles.\n\r",ch);
	return;
    }

    if (!is_room_owner(ch,victim->in_room) && ch->in_room != victim->in_room
    &&  room_is_private(victim->in_room, ch))
    {
	send_to_char("That character is in a private room.\n\r",ch);
	return;
    }

    if (victim->desc != NULL)
    {
	send_to_char("Character in use.\n\r", ch);
	return;
    }

    sprintf(buf,"$N switches into %s",victim->short_descr);
    wiznet(buf,ch,NULL,WIZ_SWITCHES,WIZ_SECURE,get_staff_rank(ch));

    ch->desc->character = victim;
    ch->desc->original  = ch;
    victim->desc        = ch->desc;
    ch->desc            = NULL;
    /* change communications to match */
    if (ch->prompt != NULL)
        victim->prompt = str_dup(ch->prompt);
    victim->comm = ch->comm;
    victim->lines = ch->lines;
    act("Switched into $n.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    SET_BIT(victim->act[0], PLR_COLOUR);
}


void do_return(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if (ch->desc == NULL)
    {
        log_string("act_wiz.c, do_return: ch->desc is NULL.");
        return;
    }

    if (ch->desc->original == NULL)
    {
	send_to_char("You aren't switched.\n\r", ch);
	return;
    }

    send_to_char(
    "{RYou return to your original body. Type replay to see any missed tells.\n\r{x",
	ch);

    if (ch->prompt != NULL)
    {
	free_string(ch->prompt);
	ch->prompt = NULL;
    }

    if (IS_IMMORTAL(ch))
    {
        sprintf(buf,"$N returns from %s.",ch->short_descr);
        wiznet(buf,ch->desc->original,0,WIZ_SWITCHES,WIZ_SECURE,get_staff_rank(ch->desc->original));
    }

    REMOVE_BIT(ch->act[0], PLR_COLOUR);

    ch->desc->character       = ch->desc->original;
    ch->desc->original        = NULL;
    ch->desc->character->desc = ch->desc;
    ch->desc                  = NULL;
}


/* for clone, to insure that cloning goes many levels deep */
void recursive_clone(CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *clone)
{
    OBJ_DATA *c_obj, *t_obj;

    for (c_obj = obj->contains; c_obj != NULL; c_obj = c_obj->next_content)
    {
	t_obj = create_object(c_obj->pIndexData,0, true);
	clone_object(c_obj,t_obj);
	obj_to_obj(t_obj,clone);
	recursive_clone(ch,c_obj,t_obj);
    }
}


/* command that is similar to load */
void do_clone(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *rest;
    CHAR_DATA *mob;
    OBJ_DATA *obj;

    rest = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Clone what?\n\r", ch);
        return;
    }

    if (!str_prefix(arg, "object"))
    {
        mob = NULL;
        obj = get_obj_here(ch, NULL, rest);
        if (obj == NULL)
        {
            send_to_char("You don't see that here.\n\r", ch);
            return;
        }
    }
    else if (!str_prefix(arg, "mobile") || !str_prefix(arg, "character"))
    {
        obj = NULL;
        mob = get_char_room(ch, NULL, rest);
        if (mob == NULL)
        {
            send_to_char("You don't see that here.\n\r", ch);
            return;
        }
    }
    else /* find both */
    {
        mob = get_char_room(ch, NULL, argument);
        obj = get_obj_here(ch, NULL, argument);
        if (mob == NULL && obj == NULL)
        {
            send_to_char("You don't see that here.\n\r", ch);
            return;
        }
    }

    /* clone an object */
    if (obj != NULL)
    {
        OBJ_DATA *clone;

        clone = create_object(obj->pIndexData, 0, true);
        clone_object(obj, clone);
        if (obj->carried_by != NULL)
            obj_to_char(clone, ch);
        else
            obj_to_room(clone, ch->in_room);
        recursive_clone(ch, obj, clone);

        act("$n has created $p.", ch, NULL, NULL, clone, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You clone $p.", ch, NULL, NULL, clone, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        wiznet("$N clones $p.", ch, clone, WIZ_LOAD, WIZ_SECURE, get_staff_rank(ch));
        return;
    }
    else if (mob != NULL)
    {
        CHAR_DATA *clone;
        OBJ_DATA *new_obj;
        OBJ_DATA *carried_obj;
        char buf[MAX_STRING_LENGTH];
        ITERATOR it;

        if (!IS_NPC(mob))
        {
            send_to_char("You can only clone mobiles.\n\r", ch);
            return;
        }

        clone = clone_mobile(mob);

        iterator_start(&it, mob->lcarrying);
        while ((carried_obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            new_obj = create_object(carried_obj->pIndexData, 0, true);
            clone_object(carried_obj, new_obj);
            recursive_clone(ch, carried_obj, new_obj);
            obj_to_char(new_obj, clone);
            new_obj->wear_loc = carried_obj->wear_loc;
        }
        iterator_stop(&it);

        char_to_room(clone, ch->in_room);
        act("$n has created $N.", ch, clone, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You clone $N.", ch, clone, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        sprintf(buf, "$N clones %s.", clone->short_descr);
        wiznet(buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_staff_rank(ch));
        return;
    }
}


void do_load(CHAR_DATA *ch, char *argument)
{
   char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument,arg);

    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  load mob <vnum>\n\r",ch);
	send_to_char("  load obj <vnum> <amt>\n\r",ch);
	/*send_to_char("  load ship <vnum> <room vnum>\n\r",ch);*/
	return;
    }

    if (!str_cmp(arg,"mob") || !str_cmp(arg,"char"))
    {
	do_function(ch, &do_mload, argument);
	return;
    }

    if (!str_cmp(arg,"obj"))
    {
	do_function(ch, &do_oload, argument);
	return;
    }

/*    if (!str_cmp(arg, "ship"))
    {
	do_function(ch, &do_sload, argument);
	return;
    }*/

    /* echo syntax */
    do_function(ch, &do_load, "");
}


void do_mload(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    MOB_INDEX_DATA *pMobIndex;
    CHAR_DATA *victim;
    int amt = 1;
    int i;
    long vnum;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
        send_to_char("Syntax: load mob <vnum|$reserved_name> <amt>.\n\r", ch);
        return;
    }

    // Handle reserved names format using $name
    if (arg1[0] == '$') {
        char *reserved_name = arg1 + 1;  // Skip the $ character
        vnum = get_reserved_vnum(reserved_name);
        if (vnum == -1) {
            send_to_char("No reserved mobile with that name found.\n\r", ch);
            return;
        }
        
        // Convert the vnum to a string for further processing
        sprintf(arg1, "%ld", vnum);
    }
    else if (!is_number(arg1)) {
        send_to_char("Syntax: load mob <vnum|$reserved_name> <amt>.\n\r", ch);
        return;
    }

    if (arg2[0] != '\0')
    {
        if (!is_number(arg2))
        {
            send_to_char("Syntax: mload <vnum|$reserved_name> <amt>.\n\r", ch);
            return;
        }

        amt = atoi(arg2);
        if (amt < 1 || amt > 50)
        {
            send_to_char("Range for amount is 1-50.\n\r", ch);
            return;
        }
    }

    vnum = atol(arg1);
    if ((pMobIndex = get_mob_index(vnum)) == NULL)
    {
        send_to_char("No mobile has that vnum.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, pMobIndex->area))
    {
        send_to_char("You aren't a builder in that area - action logged.\n\r", ch);
        sprintf(buf, "do_mload: %s tried to load %s (vnum %ld) in area %s without permissions!",
            ch->name,
            pMobIndex->short_descr,
            pMobIndex->vnum,
            pMobIndex->area->name);
        log_string(buf);
        return;
    }

    if (amt == 1)
    {
        victim = create_mobile(pMobIndex, false);
        
        if (ch->in_wilds == NULL)
            char_to_room(victim, ch->in_room);
        else
            char_to_vroom(victim, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
            
        p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);

        sprintf(buf, "Loaded %s (%ld)",
            pMobIndex->short_descr,
            pMobIndex->vnum);
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n has created $N!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        sprintf(buf,"$N loads %s.", victim->short_descr);
        wiznet(buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_staff_rank(ch));
    }
    else
    {
        for (i = 0; i < amt; i++)
        {
            victim = create_mobile(pMobIndex, false);
            
            if (ch->in_wilds == NULL)
                char_to_room(victim, ch->in_room);
            else
                char_to_vroom(victim, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
                
            p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
        }

        sprintf(buf, "{Y({G%d{Y){x $n has created %s!",
            amt, pMobIndex->short_descr);
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        
        sprintf(buf, "{Y({G%d{Y){x Loaded %s (%ld)",
            amt, pMobIndex->short_descr, pMobIndex->vnum);
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        
        sprintf(buf, "{Y({G%d{Y){x $N loads %s.",
            amt, pMobIndex->short_descr);
        wiznet(buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_staff_rank(ch));
    }
}


void do_oload(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    OBJ_DATA *obj;
    int amt = 1;
    int i;
    long vnum;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
        send_to_char("Syntax: load obj <vnum|$reserved_name> <amt>.\n\r", ch);
        return;
    }

    // Handle reserved names format using $name
    if (arg1[0] == '$') {
        char *reserved_name = arg1 + 1;  // Skip the $ character
        vnum = get_reserved_vnum(reserved_name);
        if (vnum == -1) {
            send_to_char("No reserved object with that name found.\n\r", ch);
            return;
        }
        
        // Convert the vnum to a string for further processing
        sprintf(arg1, "%ld", vnum);
    }
    else if (!is_number(arg1)) {
        send_to_char("Syntax: load obj <vnum|$reserved_name> <amt>.\n\r", ch);
        return;
    }

    if (arg2[0] != '\0')
    {
        if (!is_number(arg2))
        {
            send_to_char("Syntax: oload <vnum|$reserved_name> <amt>.\n\r", ch);
            return;
        }

        amt = atoi(arg2);
        if (amt < 1 || amt > 50)
        {
            send_to_char("Range for amount is 1-50.\n\r",ch);
            return;
        }
    }

    vnum = atol(arg1);
    if ((pObjIndex = get_obj_index(vnum)) == NULL)
    {
        send_to_char("No object has that vnum.\n\r", ch);
        return;
    }

    if (!has_access_area(ch, pObjIndex->area))
    {
        send_to_char("Insufficient security to load object - action logged.\n\r", ch);
        sprintf(buf, "do_oload: %s tried to load %s (vnum %ld) in area %s without permissions!",
            ch->name,
            pObjIndex->short_descr,
            pObjIndex->vnum,
            pObjIndex->area->name);
        log_string(buf);
        return;
    }

    if (amt == 1)
    {
        obj = create_object(pObjIndex, pObjIndex->level, true);
        if (CAN_WEAR(obj, ITEM_TAKE))
            obj_to_char(obj, ch);
        else if (ch->in_room->wilds == NULL)
        {
            plogf("act_wiz.c, do_oload(): Moving object to static room.");
            obj_to_room(obj, ch->in_room);
        }
        else
        {
            plogf("act_wiz.c, do_oload(): Moving object to vroom.");
            obj_to_vroom(obj, ch->in_room->wilds, ch->at_wilds_x, ch->at_wilds_y);
        }

        act("$n has created $p!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        sprintf(buf, "Loaded $p (%ld)", obj->pIndexData->vnum);
        act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        wiznet("$N loads $p.",ch,obj,WIZ_LOAD,WIZ_SECURE,get_staff_rank(ch));

        obj->loaded_by = str_dup(ch->name);

        p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
    }
    else
    {
        for (i = 0; i < amt; i++)
        {
            obj = create_object(pObjIndex, pObjIndex->level, true);
            if (CAN_WEAR(obj, ITEM_TAKE))
                obj_to_char(obj, ch);
            else if (ch->in_room->wilds == NULL)
            {
                obj_to_room(obj, ch->in_room);
            }
            else
            {
                obj_to_vroom(obj, ch->in_room->wilds, ch->at_wilds_x, ch->at_wilds_y);
            }
            
            obj->loaded_by = str_dup(ch->name);

            p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
        }

        sprintf(buf, "{Y({G%d{Y){x $n has created %s!", amt,
            pObjIndex->short_descr);
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        sprintf(buf, "{Y({G%d{Y){x Loaded %s (%ld)",
            amt, pObjIndex->short_descr, pObjIndex->vnum);
        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        sprintf(buf, "{Y({G%d{Y){x $N loads %s.", amt, pObjIndex->short_descr);
        wiznet(buf, ch, NULL, WIZ_LOAD, WIZ_SECURE, get_staff_rank(ch));
    }
}


void do_purge(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
	char arg3[MIL];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
	bool forced = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);


    if (arg[0] == '\0' || ( str_cmp(arg,"room") && arg2[0] == '\0'))
	{
		send_to_char ("Syntax: purge <object|mob> <keyword|ida idb> [force]\n\r", ch);
		send_to_char ("Syntax: purge room [force]\n\r", ch);
		return;
	}
	else
    {
		if (!str_cmp(arg, "mob"))
		{
			victim = NULL;
			if (is_number(arg2))
			{ 
				argument = one_argument(argument, arg3);
				if (is_number(arg3))
				{
					if((victim = idfind_mobile(atoi(arg2), atoi(arg3))) == NULL)
					{
						send_to_char("No mob has that vnum.\n\r", ch);
						return;
					}
					else if (!IS_NPC(victim))
					{
						send_to_char("You can't purge a player character.\n\r", ch);
						return;
					}

					if (argument[0] != '\0' && !str_cmp(argument, "force"))
					{
						if (ch->tot_level != MAX_LEVEL)
						{
							send_to_char("You must be max level to use the 'force' argument.\n\r", ch);
							return;
						}
						else
							forced = true;
					}
				}
			}
			else if ((victim = get_char_room(ch, NULL, arg2)) != NULL)
    		{
				if (!IS_NPC(victim))
				{
	    			send_to_char("You can't purge a player character.\n\r", ch);
	    			return;
				}
				if (argument[0] != '\0' && !str_cmp(argument, "force"))
				{
					if (ch->tot_level != MAX_LEVEL)
					{
						send_to_char("You must be max level to use the 'force' argument.\n\r", ch);
						return;
					}
					else
						forced = true;
				}
    		}

			if (victim != NULL && IS_SET(victim->act[0],ACT_NOPURGE) && !forced)
			{
				act("$N is flagged 'nopurge' - Try again with the 'force' argument.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
				return;
			} else
			{
				act("Extracted $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
				extract_char(victim, true);
				return;
			}

		}
		else if (!str_cmp(arg, "object") || !str_cmp(arg, "obj"))
		{
			obj = NULL;
			if (is_number(arg2))
			{
				argument = one_argument(argument, arg3);
				if (is_number(arg3))
				{
					if ((obj = idfind_object(atoi(arg2), atoi(arg3))) == NULL)
					{
						send_to_char("No object has that ID.\n\r", ch);
						return;
					}

					if (argument[0] != '\0' && !str_cmp(argument, "force"))
					{
						if (ch->tot_level != MAX_LEVEL)
						{
							send_to_char("You must be max level to use the 'force' argument.\n\r", ch);
							return;
						}
						else
							forced = true;
					}
				}
			}
			else if ((obj = get_obj_list(ch, arg2, ch->in_room->contents)) != NULL)
			{
				if (argument[0] != '\0' && !str_cmp(argument, "force"))
				{
					if (ch->tot_level != MAX_LEVEL)
					{
						send_to_char("You must be max level to use the 'force' argument.\n\r", ch);
						return;
					}
					else
						forced = true;
				}
			}
			else 
			{
				send_to_char("Object not found.\n\r", ch);
					return;
			}

			if (obj != NULL && IS_SET(obj->extra[0], ITEM_NOPURGE) && !forced)
			{
				act("$p is flagged 'nopurge' - Try again with the 'force' argument.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
				return;
			} else
			{
				if (obj->item_type == ITEM_CART)
				{
	    			if(obj->pulled_by) 
					{
		    			obj->pulled_by->pulled_cart = NULL;
		    			obj->pulled_by = NULL;
	    			}
				}
				act("Extracted $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
				extract_obj(obj);
				return;
			}
		}
		else if (!str_cmp(arg, "room"))
		{
	
			CHAR_DATA *vnext;
			CHAR_DATA *victim;
			OBJ_DATA  *obj_next;

			if (!str_cmp(arg2, "force"))
			{
				if (ch->tot_level != MAX_LEVEL)
				{
					send_to_char("You must be max level to use the 'force' argument.\n\r", ch);
					return;
				}
				else
					forced = true;
			}

			for (victim = ch->in_room->people; victim != NULL; victim = vnext)
			{
	    		vnext = victim->next_in_room;
				if (IS_SET(victim->act[0],ACT_NOPURGE) && !forced)
				{
					act("$N is flagged 'nopurge' - Try again with the 'force' argument.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
					continue;
				}
	    		if (IS_NPC(victim)
	    		
	    		&& victim != ch /* safety precaution */
	    		&& victim != ch->rider
	    		&& victim != ch->mount) 
				{
					extract_char(victim, true);
	    		}
			}
		
			for (obj = ch->in_room->contents; obj != NULL; obj = obj_next)
			{
	    		obj_next = obj->next_content;
				if (IS_SET(obj->extra[0], ITEM_NOPURGE) && !forced)
				{
					act("$p is flagged 'nopurge' - Try again with the 'force' argument.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
					continue;
				}

			    if (obj->item_type == ITEM_CART) 
				{
		    		if(obj->pulled_by) {
			    		obj->pulled_by->pulled_cart = NULL;
			    		obj->pulled_by = NULL;
		    		}
	    		}
				extract_obj(obj);
			}

			act("$n purges the room!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
			act("Purged $T.", ch, NULL, NULL, NULL, NULL, NULL, ch->in_room->name, TO_CHAR, NULL, NULL);
			return;
    	}
	}
}

/* Adding some new stuff to advance, for new immortals. It'll now display an intro screen to them. Perhaps the intro would be better as a helpfile, along the same lines as do_greeting? -- Areo 2006-08-23 */
void do_advance(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int level;
    int iLevel;
    int olevel;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NPC(ch)) {
        sprintf("do_advance: NPC %s(%ld) tried to advance", ch->pIndexData->short_descr, ch->pIndexData->vnum);
	log_string(buf);
	send_to_char("No.\n\r", ch);
	return;
    }

    if (arg1[0] == '\0' || arg2[0] == '\0' || !is_number(arg2))
    {
	send_to_char("Syntax: advance <char> <level>.\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg1)) == NULL)
    {
	send_to_char("That player is not here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("Not on NPC's.\n\r", ch);
	return;
    }

    if ((level = atoi(arg2)) < 1 || level > MAX_LEVEL)
    {
	sprintf(buf,"Level must be 1 to %d.\n\r", MAX_LEVEL);
	send_to_char(buf, ch);
	return;
    }

    if ((level > MAX_CLASS_LEVEL) && (level < LEVEL_IMMORTAL))
    {
	sprintf(buf,"Cannot advance to multiclass level range.\n\r");
	send_to_char(buf, ch);
	return;
    }

    if (level > get_trust(ch))
    {
	send_to_char("Limited to your trust level.\n\r", ch);
	return;
    }

    if (level == victim->level) return;

    /*
     * Lower level:
     *   Reset to level 1.
     *   Then raise again.
     *   Currently, an imp can lower another imp.
     *   -- Swiftest
     */
    if (level < victim->level)
    {
        int temp_prac;

	if(victim->pcdata->immortal) {
		IMMORTAL_DATA *immortal, *tmp, *last;

		immortal = victim->pcdata->immortal;

		if (victim->tot_level == MAX_LEVEL) {
			send_to_char("You may not delete implementors.\n\r", ch);
			return;
		}

		if(level < LEVEL_IMMORTAL) {
			act("$N has been deleted from the immortal list.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
			/* Remove it from the global list */
			last = NULL;
			for (tmp = immortal_list; tmp != NULL; tmp = tmp->next) {
				if (tmp == immortal)
					break;

				last = tmp;
			}

			if (last != NULL)
				last->next = immortal->next;
			else
				immortal_list = immortal->next;
			free_immortal(immortal);
			victim->pcdata->immortal = NULL;
		}
	}

	send_to_char("Lowering a player's level!\n\r", ch);
	send_to_char("**** OOOOHHHHHHHHHH  NNNNOOOO ****\n\r", victim);
	temp_prac = victim->practice;
	victim->level    = 1;
	victim->tot_level    = 1;
	victim->exp      = 0; /*exp_per_level(victim,victim->pcdata->points);*/
	victim->max_hit  = 10;
	victim->max_mana = 100;
	victim->max_move = 100;
	victim->practice = 0;
	victim->hit      = victim->max_hit;
	victim->mana     = victim->max_mana;
	victim->move     = victim->max_move;
	advance_level(victim, true);
	victim->practice = temp_prac;
    }
    else
    /* Only show this if we're not making them 150's.*/
    if (level != LEVEL_IMMORTAL)
    {
	send_to_char("Raising a player's level!\n\r", ch);
	send_to_char("**** OOOOHHHHHHHHHH  YYYYEEEESSS ****\n\r", victim);
    }

    if (level < LEVEL_IMMORTAL)
    for (iLevel = victim->level ; iLevel < level; iLevel++)
    {
	victim->level += 1;
	victim->tot_level += 1;
	advance_level(victim,true);
    }
    else
    {
	/* Here's the big one. We'll take the victim's old level, and if they were below 149 (and advanced to 150+, we'll show them a nifty intro screen with some basic instructions. After that, we'll set them as wizi to their new level, give them a basic imm_flag, turn holylight on, and let them know what we've done. This should cut down on needed explanations, if only slightly. -- Areo */

	/* SYN -- add IMMORTAL_DATA here!! */

	olevel = victim->tot_level;
	if (olevel < (LEVEL_IMMORTAL - 1) && level >= LEVEL_IMMORTAL)
	{
		IMMORTAL_DATA *immortal = new_immortal();

		immortal->name = str_dup(victim->name);
		immortal->imm_flag = str_dup("{R  Immortal  {x");
		immortal->created = current_time;

		/* start them off as unassigned */
		immortal->next = immortal_list;
		immortal_list = immortal;

		victim->pcdata->immortal = immortal;

		send_to_char("{B================================================================================{x\n\r", victim);
		send_to_char("{B|{C****************************{WWelcome, new Immortal!{C****************************{B|{x\n\r",victim);
		send_to_char("{B================================================================================{x\n\r", victim);
		sprintf(buf,"\n\rWelcome to the Sentience Immortal Staff. Please read {WHELP IMMORTAL RULES{x now. In addition to this, please type wizhelp to see a full list of your available immortal commands. You may use '{Rimmtalk <message>{X' or '{R: <message>{X' to communicate on the immortal channel. {WHELP %d{x will list available helpfiles for your level.\n\r\n\r", level);
		send_to_char(buf,victim);
	victim->invis_level = level;
	do_function(victim, &do_holylight, "");
	do_function(victim, &do_holywarp, "");
	victim->prompt = str_dup("{W[{R%o{W][{g%O{W] Room: {a%R {W({a%r{W) - {X%h{W>{X%c");
	sprintf(buf, "\n\rYou have been set to wizinvis level {W%d{x.\n\r", victim->invis_level);
	send_to_char(buf,victim);
	}
	victim->level = level;
	victim->tot_level = level;
    }


    if ((victim->level > MAX_CLASS_LEVEL) &&
	 (level < LEVEL_IMMORTAL))
    { /* set level to hero */
	victim->level = 31;
    }

	/* Again, only display if the victim is not a newly minted 150.*/
	if (level != LEVEL_IMMORTAL)
	{
    sprintf(buf,"You are now level %d.\n\r",victim->level);
    send_to_char(buf,victim);
	}
    victim->exp   = 0;/*exp_per_level(victim,victim->pcdata->points)*/
		  /** UMAX(1, victim->level);*/
    victim->trust = 0;
    save_char_obj(victim);
}


void do_trust(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    int level;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0' || !is_number(arg2))
    {
	send_to_char("Syntax: trust <char> <level>.\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg1)) == NULL)
    {
	send_to_char("That player is not here.\n\r", ch);
	return;
    }

    if ((level = atoi(arg2)) < 0 || level > MAX_LEVEL)
    {
	sprintf(buf, "Level must be 0 (reset) or 1 to %d.\n\r",MAX_LEVEL);
	send_to_char(buf, ch);
	return;
    }

    if (level > get_trust(ch))
    {
	send_to_char("Limited to your trust.\n\r", ch);
	return;
    }

    victim->trust = level;
}


void do_restore(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *vch;
    DESCRIPTOR_DATA *d;

    one_argument(argument, arg);
    if (arg[0] == '\0' || !str_cmp(arg,"room"))
    {
    /* cure room */

        for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
        {
			restore_char(vch, ch, 100);
        }


        sprintf(buf, "$N restored room %ld.", ch->in_room->vnum);
        wiznet(buf, ch, NULL, WIZ_RESTORE, WIZ_SECURE, get_staff_rank(ch));

        send_to_char("Room restored.\n\r",ch);
        return;
    }

    /* restore all */
    if ((ch->tot_level >= MAX_LEVEL - 2) && !str_cmp(arg,"all"))
    {
        for (d = descriptor_list; d != NULL; d = d->next)
        {

		    victim = d->character;

		    if (victim == NULL || IS_NPC(victim) || IS_IMMORTAL(victim))
				continue;
			restore_char(victim, ch, 100);
        }

		send_to_char("All active players restored.\n\r",ch);
		return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
		send_to_char("They aren't here.\n\r", ch);
		return;
    }

	restore_char(victim, ch, 100);
    act("Restored $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    sprintf(buf, "$N restored %s.", IS_NPC(victim) ? victim->short_descr : victim->name);
    wiznet(buf,ch,NULL,WIZ_RESTORE,WIZ_SECURE,get_staff_rank(ch));


}


void do_freeze(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH],buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Freeze whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("Not on NPC's.\n\r", ch);
	return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch))
    {
	send_to_char("You failed.\n\r", ch);
	return;
    }

    if (IS_SET(victim->act[0], PLR_FREEZE))
    {
	REMOVE_BIT(victim->act[0], PLR_FREEZE);
	send_to_char("You can play again.\n\r", victim);
	send_to_char("FREEZE removed.\n\r", ch);
	sprintf(buf,"$N thaws %s.",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }
    else
    {
	SET_BIT(victim->act[0], PLR_FREEZE);
	send_to_char("You can't do ANYthing!\n\r", victim);
	send_to_char("FREEZE set.\n\r", ch);
	sprintf(buf,"$N puts %s in the deep freeze.",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }

    save_char_obj(victim);
}


void do_log(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    /* This command is strict */
    if (ch->tot_level < MAX_LEVEL)
    {
	send_to_char("Huh?\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r", ch);
	send_to_char("  log <character>\n\r", ch);
	send_to_char("  log all\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "all"))
    {
	if (logAll)
	{
	    logAll = false;
	    send_to_char("Log ALL off.\n\r", ch);
	}
	else
	{
	    logAll = true;
	    send_to_char("Log ALL on.\n\r", ch);
	}
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("Not on NPC's.\n\r", ch);
	return;
    }

    /*
     * No level check, gods can log anyone.
     */
    if (IS_SET(victim->act[0], PLR_LOG))
    {
	REMOVE_BIT(victim->act[0], PLR_LOG);
	send_to_char("LOG removed.\n\r", ch);
    }
    else
    {
	SET_BIT(victim->act[0], PLR_LOG);
	send_to_char("LOG set.\n\r", ch);
    }
}


void do_notell(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH],buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Notell whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (get_staff_rank(victim) >= get_staff_rank(ch))
    {
	send_to_char("You failed.\n\r", ch);
	return;
    }

    if (IS_SET(victim->comm, COMM_NOTELL))
    {
	REMOVE_BIT(victim->comm, COMM_NOTELL);
	send_to_char("You can tell again.\n\r", victim);
	send_to_char("NOTELL removed.\n\r", ch);
	sprintf(buf,"$N restores tells to %s.",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }
    else
    {
	SET_BIT(victim->comm, COMM_NOTELL);
	send_to_char("You can't tell!\n\r", victim);
	send_to_char("NOTELL set.\n\r", ch);
	sprintf(buf,"$N revokes %s's tells.",victim->name);
	wiznet(buf,ch,NULL,WIZ_PENALTIES,WIZ_SECURE,0);
    }
}


void do_peace(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *rch;

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
	if (rch->fighting != NULL)
	    stop_fighting(rch, true);
	if (IS_NPC(rch) && IS_SET(rch->act[0],ACT_AGGRESSIVE))
	    REMOVE_BIT(rch->act[0],ACT_AGGRESSIVE);
    }

    send_to_char("Done.\n\r", ch);
}


void do_wizlock(CHAR_DATA *ch, char *argument)
{

	if (argument[0] == '\0')
	{
    	if (!game_settings.wizlock)
    	{
			wiznet("$N has wizlocked the game.",ch,NULL,0,0,0);
			send_to_char("Game wizlocked.\n\r", ch);
			game_settings.wizlock = true;
    	}
    	else
    	{
			wiznet("$N removes wizlock.",ch,NULL,0,0,0);
			send_to_char("Game un-wizlocked.\n\r", ch);
			game_settings.wizlock = false;
    	}
	}
	else if (!str_cmp(argument, "clear"))
	{
		if (!IS_NULLSTR(game_settings.wizlock_msg))
		{
			free_string(game_settings.wizlock_msg);
			game_settings.wizlock_msg = str_dup("");
		}
	}
	else
	{
		if (!IS_NULLSTR(game_settings.wizlock_msg))
		{
			free_string(game_settings.wizlock_msg);
		}
		game_settings.wizlock_msg = str_dup(argument);
		wiznet("$N sets wizlock message.",ch,NULL,0,0,0);
		send_to_char("Wizlock message set.\n\r", ch);
    }

	game_settings_write();

}


void do_newlock(CHAR_DATA *ch, char *argument)
{
    newlock = !newlock;
	char arg[MIL];

	if (argument[0] == '\0')
	{
    	send_to_char("Syntax: newlock <char|acct> [$message|clear]\n\r", ch);
		return;
	}
	else
	{
		argument = one_argument(argument, arg);
		if (!str_cmp(arg, "char"))
		{
			if (argument[0] == '\0')
			{
				if (!game_settings.new_char_lock)
				{
					wiznet("$N locks out new characters.",ch,NULL,0,0,0);
					send_to_char("New characters have been locked out.\n\r", ch);
					game_settings.new_char_lock = true;
				}
				else
				{
					wiznet("$N allows new characters back in.",ch,NULL,0,0,0);
					send_to_char("New characters are no longer locked out.\n\r", ch);
					game_settings.new_char_lock = false;
				}
			}
			else if (!str_cmp(argument, "clear"))
			{
				if (!IS_NULLSTR(game_settings.new_char_lock_msg))
				{
					free_string(game_settings.new_char_lock_msg);
					game_settings.new_char_lock_msg = str_dup("");
				}
			}
			else
			{
				if (!IS_NULLSTR(game_settings.new_char_lock_msg))
				{
					free_string(game_settings.new_char_lock_msg);
				}
				game_settings.new_char_lock_msg = str_dup(argument);
				wiznet("$N sets new character message.",ch,NULL,0,0,0);
				send_to_char("New character message set.\n\r", ch);
			}
		}
		else if (!str_cmp(arg, "acct"))
		{
			if (argument[0] == '\0')
			{
				if (!game_settings.new_acct_lock)
				{
					wiznet("$N locks out new accounts.",ch,NULL,0,0,0);
					send_to_char("New accounts have been locked out.\n\r", ch);
					game_settings.new_acct_lock = true;
				}
				else
				{
					wiznet("$N allows new accounts back in.",ch,NULL,0,0,0);
					send_to_char("New accounts are no longer locked out.\n\r", ch);
					game_settings.new_acct_lock = false;
				}
			}
			else if (!str_cmp(argument, "clear"))
			{
				if (!IS_NULLSTR(game_settings.new_acct_lock_msg))
				{
					free_string(game_settings.new_acct_lock_msg);
					game_settings.new_acct_lock_msg = str_dup("");
				}
			}
			else
			{
				if (!IS_NULLSTR(game_settings.new_acct_lock_msg))
				{
					free_string(game_settings.new_acct_lock_msg);
				}
				game_settings.new_acct_lock_msg = str_dup(argument);
				wiznet("$N sets new account message.",ch,NULL,0,0,0);
				send_to_char("New account message set.\n\r", ch);
			}
		}
		game_settings_write();
	}

}

void do_testport(CHAR_DATA *ch, char *argument)
{

    if (!game_settings.testport)
    {
		wiznet("$N enables Test Port Mode.",ch,NULL,0,0,0);
		send_to_char("Test Port Mode enabled.\n\r", ch);
    }
    else
    {
		wiznet("$N disables Test Port Mode.",ch,NULL,0,0,0);
		send_to_char("Test Port Mode disabled.\n\r", ch);
    }
    game_settings_write();
}

/*
void do_slookup(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int sn;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Lookup which skill or spell?\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "all"))
    {
	for (sn = 0; sn < MAX_SKILL; sn++)
	{
	    if (skill_table[sn].name == NULL)
		break;
	    sprintf(buf, "Sn: %3d  Slot: %3d  Skill/spell: '%s'\n\r",
		sn, skill_table[sn].slot, skill_table[sn].name);
	    send_to_char(buf, ch);
	}
    }
    else
    {
	if ((sn = skill_lookup(arg)) < 0)
	{
	    send_to_char("No such skill or spell.\n\r", ch);
	    return;
	}

	sprintf(buf, "Sn: %3d  Slot: %3d  Skill/spell: '%s'\n\r",
	    sn, skill_table[sn].slot, skill_table[sn].name);
	send_to_char(buf, ch);
    }
}
*/

void do_set(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MSL];

    argument = one_argument(argument,arg);

    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  set char  <name> <field> <value>\n\r",ch);
	send_to_char("  set obj   <name> <field> <value>\n\r",ch);
	send_to_char("  set room  <room> <field> <value>\n\r",ch);
	send_to_char("  set church <no.> <field> <value>\n\r",ch);
        send_to_char("  set skill <name> <spell or skill> <value>\n\r",ch);
	send_to_char("  set sky   <cloudless|cloudy|rainy|stormy>\n\r", ch);
	send_to_char("  set time  <hour|day|month|year> <#>\n\r", ch);
	send_to_char("  set token <char name> <token vnum> <v#|timer> <op> <value>\n\r", ch);
	send_to_char("  set account <account> <field> <value>\n\r", ch);

	return;
    }

    if (!str_prefix(arg,"mobile") || !str_prefix(arg,"character"))
    {
	do_function(ch, &do_mset, argument);
	return;
    }

    if (!str_prefix(arg,"skill") || !str_prefix(arg,"spell"))
    {
	do_function(ch, &do_sset, argument);
	return;
    }

    if (!str_prefix(arg,"object"))
    {
	do_function(ch, &do_oset, argument);
	return;
    }

    if (!str_prefix(arg,"room"))
    {
	do_function(ch, &do_rset, argument);
	return;
    }

    if (!str_prefix(arg,"church"))
    {
	do_function(ch, &do_chset, argument);
	return;
    }

    if (!str_prefix(arg, "sky"))
    {
	if (argument[0] == '\0')
	{
	    send_to_char("Syntax: set sky <cloudless|cloudy|rainy|stormy>\n\r", ch);
	    return;
	}

	if (!str_cmp(argument, "cloudless"))
	    weather_info.sky = SKY_CLOUDLESS;
	else if (!str_cmp(argument, "cloudy"))
	    weather_info.sky = SKY_CLOUDY;
	else if (!str_cmp(argument, "rainy"))
	    weather_info.sky = SKY_RAINING;
	else if (!str_cmp(argument, "stormy"))
	    weather_info.sky = SKY_LIGHTNING;
	else
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	sprintf(buf, "Set sky condition to %s.\n\r", argument);
	send_to_char(buf, ch);

	return;
    }

    if (!str_prefix(arg, "time"))
    {
	do_function(ch, &do_tset, argument);
	return;
    }

    if (!str_prefix(arg, "token"))
    {
	do_function(ch, &do_tkset, argument);
	return;
    }

	if (!str_prefix(arg, "account") || !str_prefix(arg, "acct"))
	{
		do_function(ch, &do_accset, argument);
		return;
	}

    /* echo syntax */
    do_function(ch, &do_set, "");
}


/* set token <char name> <token vnum> <v#|timer> <operator> <value> */
void do_tkset(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL], arg2b[MSL];
    char arg3[MSL];
    char arg4[MSL];
    char arg5[MSL];
    char buf[MSL];
    CHAR_DATA *victim;
    TOKEN_DATA *token;
    long vnum, value, count;
    int value_num;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0' || arg5[0] == '\0') {
		send_to_char("Syntax:\n\r  set token <char name> <token vnum> <v#|timer> <op> <value>\n\r", ch);
		return;
    }

    if ((victim = get_char_world(NULL, arg)) == NULL) {
		send_to_char("Character not found.\n\r", ch);
		return;
    }

	count = number_argument(arg2,arg2b);
    vnum = atol(arg2b);

    if ((token = get_token_char(victim, vnum, count)) == NULL) {
		send_to_char("Character doesn't have that token vnum.\n\r", ch);
		return;
    }

    if (!str_cmp(arg3, "timer"))
		value_num = -1;
    else if (is_number(arg3))
		value_num = atoi(arg3);
    else {
		send_to_char("Invalid value argument.\n\r", ch );
		return;
    }

    if (value_num < -1 || value_num >= MAX_TOKEN_VALUES) {
		send_to_char("Invalid value number.\n\r", ch);
		return;
    }

    value = atol(arg5);
    /*
    if (value < -2000000000 || value > 2000000000) {
		send_to_char("Value out of range.\n\r", ch);
		return;
    }
    */

    if (value_num == -1)
    {
	switch (arg4[0])
	{
	    case '+':
		token->timer += value;
		break;

	    case '-':
		token->timer -= value;
		break;

	    case '*':
		token->timer *= value;
		break;

	    case '/':
		if (value == 0) {
		    bug("do_tkset: adjust called with operator / and value 0", 0);
		    return;
		}
		token->timer /= value;
		break;

	    case '%':
		if (value == 0) {
		    bug("do_tkset: adjust called with operator %% and value 0", 0);
		    return;
		}
		token->timer %= value;
		break;

	    case '=':
		token->timer = value;
		break;

	    default:
		sprintf(buf, "do_tkset: bad operator %c", arg5[0]);
		bug(buf, 0);
	}

	sprintf(buf, "Adjusted token %s(%ld.%ld) on char %s, timer %c %ld\n\r",
	    token->name, count, token->pIndexData->vnum, HANDLE(victim),
	    arg4[0], value);
	send_to_char(buf, ch);
    }
    else
    {
	switch (arg4[0])
	{
	    case '+':
		token->value[value_num] += value;
		break;

	    case '-':
		token->value[value_num] -= value;
		break;

	    case '*':
		token->value[value_num] *= value;
		break;

	    case '/':
		if (value == 0) {
		    bug("do_tkset: adjust called with operator / and value 0", 0);
		    return;
		}
		token->value[value_num] /= value;
		break;

	    case '%':
		if (value == 0) {
		    bug("do_tkset: adjust called with operator % and value 0", 0);
		    return;
		}
		token->value[value_num] %= value;
		break;

	    case '=':
		token->value[value_num] = value;
		break;

	    default:
		sprintf(buf, "do_tkset: bad operator %c", arg5[0]);
		bug(buf, 0);
	}

	sprintf(buf, "Adjusted token %s(%ld.%ld) on char %s, value %s %c %ld\n\r",
	    token->name, count, token->pIndexData->vnum, HANDLE(victim),
	    token->pIndexData->value_name[value_num], arg4[0], value);
	send_to_char(buf, ch);
    }

    sprintf(buf, "stat token %s %ld", victim->name, vnum);
    interpret(ch, buf);
}

void set_moon_phase(void)
{
	int hours;

	hours = ((((time_info.year*12)+time_info.month)*35+time_info.day)*24+time_info.hour+MOON_OFFSET) % MOON_PERIOD;
	hours = (hours + MOON_PERIOD) % MOON_PERIOD;

	if(hours <= (MOON_CARDINAL_HALF)) time_info.moon = MOON_NEW;
	else if(hours < (MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_CRESCENT;
	else if(hours <= (MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FIRST_QUARTER;
	else if(hours < (2*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WAXING_GIBBOUS;
	else if(hours <= (2*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_FULL;
	else if(hours < (3*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_GIBBOUS;
	else if(hours <= (3*MOON_CARDINAL_STEP + MOON_CARDINAL_HALF)) time_info.moon = MOON_LAST_QUARTER;
	else if(hours < (4*MOON_CARDINAL_STEP - MOON_CARDINAL_HALF)) time_info.moon = MOON_WANING_CRESCENT;
	else time_info.moon = MOON_NEW;
}

void do_accset(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], buf[MSL];
    ACCOUNT_DATA *account;
    bool loaded = false;
    int value;

    argument = one_argument(argument, arg);   // account name
    argument = one_argument(argument, arg2);  // field
    argument = one_argument(argument, arg3);  // value

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0') {
        send_to_char("Syntax:\n\r  set account <account name> <field> <value>\n\r", ch);
        return;
    }

    account = get_account_online_or_offline(arg, &loaded);
    if (!account) {
        send_to_char("Account not found.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "email")) {
        free_string(account->email);
        account->email = str_dup(arg3);
        sprintf(buf, "Set email for account %s to %s.\n\r", account->username, account->email);
        send_to_char(buf, ch);
    }
    else if (!str_prefix(arg2, "charlimit")) {
        if (!is_number(arg3)) {
            send_to_char("Character limit must be a number.\n\r", ch);
            if (loaded) free_account(account);
            return;
        }
        value = atoi(arg3);
        account->character_limit = value;
        sprintf(buf, "Set character limit for account %s to %d.\n\r", account->username, value);
        send_to_char(buf, ch);
    }
    else if (!str_prefix(arg2, "stafflimit")) {
        if (!is_number(arg3)) {
            send_to_char("Staff limit must be a number.\n\r", ch);
            if (loaded) free_account(account);
            return;
        }
        value = atoi(arg3);
        account->staff_limit = value;
        sprintf(buf, "Set staff limit for account %s to %d.\n\r", account->username, value);
        send_to_char(buf, ch);
    }
	else if (!str_prefix(arg2, "flag")) {
    	char flag_buf[MAX_INPUT_LENGTH];
    	char *flag_name;
    	bool found_flag = false;

	    // Make a copy of arg3 to tokenize
    	strncpy(flag_buf, arg3, sizeof(flag_buf));
    	flag_buf[sizeof(flag_buf)-1] = '\0';

	    flag_name = strtok(flag_buf, " ");
    	while (flag_name != NULL) {
        	long flagval;
        	if ((flagval = flag_value(acct_flags, flag_name)) == NO_FLAG) {
				sprintf(buf, "Invalid account flag: %s\n\r", flag_name);
				send_to_char(buf, ch);
        	    show_flag_cmds(ch, acct_flags);
        	    // Don't return, just skip this flag
        	} else {
        	    TOGGLE_BIT(account->acct_flags, flagval);
        	    found_flag = true;
        	}
        	flag_name = strtok(NULL, " ");
    	}

    	if (found_flag)
    	    send_to_char("Account flag(s) toggled.\n\r", ch);
    	else
        	send_to_char("No valid account flags toggled.\n\r", ch);

	    if (loaded) free_account(account);
	    return;
	}
    else {
        send_to_char("Unknown account field. Valid: email, charlimit, stafflimit, flags\n\r", ch);
        if (loaded) free_account(account);
        return;
    }

    save_account(account);
    if (loaded) free_account(account); // Only free if we loaded it from disk
}

void do_tset(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    int value;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0'
    || (str_cmp(arg, "hour") && str_cmp(arg, "day") && str_cmp(arg, "month") && str_cmp(arg, "year")))
    {
	send_to_char("Syntax:\n\r", ch);
	send_to_char("  set time hour <0-23>\n\r", ch);
	send_to_char("  set time day <0-34>\n\r", ch);
	send_to_char("  set time month <0-11>\n\r", ch);
	send_to_char("  set time year <#>\n\r", ch);
	return;
    }

    if (!is_number(arg2))
    {
	send_to_char("Argument must be numeric.\n\r", ch);
	return;
    }

    value = atoi(arg2);
    if (!str_cmp(arg, "hour"))
    {
	if (value < 0 || value > 23)
	{
	    send_to_char("Invalid. Range is 0-23 hours.\n\r", ch);
	    return;
	}

        send_to_char("Time set.\n\r", ch);
        time_info.hour = value;
        set_moon_phase();
	return;
    }

    if (!str_cmp(arg, "day"))
    {
	if (value < 0 || value > 34)
	{
	    send_to_char("Invalid. Range is 0-34 days.\n\r", ch);
	    return;
	}

	send_to_char("Day set.\n\r", ch);
        set_moon_phase();
	time_info.day = value;
	return;
    }

    if (!str_cmp(arg, "month"))
    {
	if (value < 0 || value > 11)
	{
	    send_to_char("Invalid. Range is 0-11 months.\n\r", ch);
	    return;
	}

	send_to_char("Month set.\n\r", ch);
	time_info.month = value;
        set_moon_phase();
	return;
    }

    if (!str_cmp(arg, "year"))
    {
	if (value < 0 || value > 25000)
	{
	    send_to_char("Invalid. Range is 0-25000.\n\r", ch);
	    return;
	}

	send_to_char("Year set.\n\r", ch);
	time_info.year = value;
        set_moon_phase();
	return;
    }
}


void do_sset(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MSL];
    CHAR_DATA *victim;
    int value;
    int sn;
    bool fAll;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  set skill <name> <spell or skill> <value>\n\r", ch);
	send_to_char("  set skill <name> all <value>\n\r",ch);
	send_to_char("   (use the name of the skill, not the number)\n\r",ch);
	return;
    }

    if ((victim = get_char_world(ch, arg1)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("Not on NPC's.\n\r", ch);
	return;
    }

    fAll = !str_cmp(arg2, "all");

    sn   = 0;
    if (!fAll && (sn = skill_lookup(arg2)) < 0)
    {
	send_to_char("No such skill or spell.\n\r", ch);
	return;
    }

    /*
     * Get the value.
     */
    if (!is_number(arg3))
    {
	send_to_char("Value must be numeric.\n\r", ch);
	return;
    }

    value = atoi(arg3);
    if (value < 0 || value > 100)
    {
	send_to_char("Value range is 0 to 100.\n\r", ch);
	return;
    }

    if (fAll)
    {
		for (sn = 0; sn < MAX_SKILL; sn++)
		{
			if (skill_table[sn].name != NULL && str_cmp(skill_table[sn].name, "none")) {
				if( value == 0 ) {
					if( skill_table[sn].spell_fun == spell_null )
						skill_entry_removeskill(victim,sn, NULL);
					else
						skill_entry_removespell(victim,sn, NULL);
				} else if( skill_entry_findsn( victim->sorted_skills, sn) == NULL) {
					if( skill_table[sn].spell_fun == spell_null ) {
						skill_entry_addskill(victim, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
					} else {
						skill_entry_addspell(victim, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
					}
				}
			}
			victim->pcdata->learned[sn]	= value;
		}
    }
    else {
		if( value == 0 ) {
			if( skill_table[sn].spell_fun == spell_null )
				skill_entry_removeskill(victim,sn, NULL);
			else
				skill_entry_removespell(victim,sn, NULL);
		} else if( skill_entry_findsn( victim->sorted_skills, sn) == NULL) {
			if( skill_table[sn].spell_fun == spell_null ) {
				skill_entry_addskill(victim, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
			} else {
				skill_entry_addspell(victim, sn, NULL, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
			}
		}
		victim->pcdata->learned[sn] = value;
	}

    if (!fAll)
	sprintf(buf, "Set %s's %s skill to %d%%\n\r", victim->name, skill_table[sn].name, value);
    else
	sprintf(buf, "Set all of %s's skills to %d%%\n\r", victim->name, value);

    send_to_char(buf, ch);
}


void do_chset(CHAR_DATA *ch, char *argument)
{
    CHURCH_DATA *church;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument_norm(argument, arg );
    argument = one_argument_norm(argument, arg2);
    argument = one_argument_norm(argument, arg3);

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("Set church <no.> <field> <value>\n\r", ch);
	send_to_char("Fields: name founder pneuma dp gold\n\r", ch);
	send_to_char("        max size align recall treasure\n\r", ch);
	send_to_char("        flag key\n\r", ch);
	return;
    }

    if (!is_number(arg))
    {
	send_to_char("That's not even a number.\n\r", ch);
	return;
    }

    if ((church = find_church(atoi(arg))) == NULL)
    {
	send_to_char("No such church found.\n\r", ch);
	return;
    }

    if (!str_cmp(arg2, "name"))
    {
	CHURCH_PLAYER_DATA *member;

	sprintf(buf, "%s is now known as %s.\n\r", church->name,
		arg3);
	send_to_char(buf, ch);

	sprintf(buf, "{Y[%s will now be known as %s!]{x\n\r",
		church->name, arg3);
	msg_church_members(church, buf);

	free_string(church->name);
	church->name = str_dup(arg3);

	/* Any players with this church should be modified*/
	for (member = church->people; member != NULL; member = member->next)
	{
	    if (member->ch != NULL)
	    {
		free_string(ch->church_name);
		ch->church_name = str_dup(capitalize(arg3));
	    }
	}
	return;
    }

    if (!str_cmp(arg2, "flag"))
    {
	sprintf(buf, "%s's flag is now %s.\n\r", church->name,
		arg3);
	send_to_char(buf, ch);

	free_string(church->flag);
	church->flag = str_dup(arg3);
	return;
    }

    if (!str_cmp(arg2, "founder"))
    {
	sprintf(buf, "%s is now the founder of %s.\n\r",
		capitalize(arg3), church->name);
	send_to_char(buf, ch);

	free_string(church->founder);
	church->founder = str_dup(capitalize(arg3));
	return;
    }

    if (!str_cmp(arg2, "dp"))
    {
	if (!is_number(arg3))
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	sprintf(buf, "%s now has %ld dp.\n\r", church->name, atol(arg3));
	send_to_char(buf, ch);

	church->dp = atol(arg3);
	return;
    }

    if (!str_cmp(arg2, "pneuma"))
    {
	if (!is_number(arg3))
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	sprintf(buf, "%s now has %ld pneuma.\n\r", church->name,
		atol(arg3));
	send_to_char(buf, ch);

	church->pneuma = atol(arg3);
	return;
    }

    if (!str_cmp(arg2, "gold"))
    {
	if (!is_number(arg3))
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	sprintf(buf, "%s now has %ld gold.\n\r", church->name,
		atol(arg3));
	send_to_char(buf, ch);

	church->gold = atol(arg3);
	return;
    }

    if (!str_cmp(arg2, "max"))
    {
		if (!is_number(arg3))
		{
			send_to_char("Invalid argument.\n\r", ch);
			return;
		}

		int max_pos = atoi(arg3);
		int min_pos = church_get_min_positions(church->size);

		if( max_pos < min_pos )
		{
			sprintf(buf, "Minimum number of max positions allowed for a church of that size is %d.\n\r", min_pos);
			send_to_char(buf, ch);
			return;
		}

		sprintf(buf, "Set max positions in %s to %d.\n\r", church->name, max_pos);
		send_to_char(buf, ch);

		church->max_positions = max_pos;
		return;
    }

    if (!str_cmp(arg2, "size"))
    {
	if (!is_number(arg3))
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	if (atoi(arg3) < 1 || atoi(arg3) > 4)
	{
	    send_to_char("Invalid argument.\n\r", ch);
	    return;
	}

	church->size = atoi(arg3);
	sprintf(buf, "Set size of %s to %s.\n\r", church->name,
		get_chsize_from_number(church->size));
	send_to_char(buf, ch);

	return;
    }

    if (!str_prefix(arg2, "alignment"))
    {
	if (!str_cmp(arg3, "evil"))
	    church->alignment = CHURCH_EVIL;
	else if (!str_cmp(arg3, "good"))
	    church->alignment = CHURCH_GOOD;
	else if (!str_cmp(arg3, "neutral"))
	    church->alignment = CHURCH_NEUTRAL;
	else
	{
	    send_to_char("Invalid argument. Choose good, evil, or neutral.\n\r", ch);
	    return;
	}

	arg3[0] = UPPER(arg3[0]);

	sprintf(buf, "Set alignment of %s to %s.\n\r", church->name, arg3);
	send_to_char(buf, ch);
	return;
    }

    if (!str_cmp(arg2, "recall"))
    {
	if (get_room_index(atol(arg3)) == NULL)
	{
	    send_to_char("That room doesn't exist.\n\r", ch);
	    return;
	}

	sprintf(buf,
	    "You have set %s's temple recall point to %ld - %s.\n\r",
	    church->name,
	    atol(arg3),
	    get_room_index(atol(arg3))->name);
	send_to_char(buf, ch);
	church->recall_point.id[0] = atol(arg3);
	church->recall_point.id[1] = church->recall_point.id[2] = 0;
	church->recall_point.wuid = 0;
	return;
    }

    if (!str_cmp(arg2, "key"))
    {
	if (get_obj_index(atol(arg3)) == NULL)
	{
	    send_to_char("That object doesn't exist.\n\r", ch);
	    return;
	}

	sprintf(buf,
	    "You have set %s's key to %ld - %s.\n\r",
	    church->name,
	    atol(arg3),
	    get_obj_index(atol(arg3))->short_descr);
	send_to_char(buf, ch);
	church->key = atol(arg3);
	return;
    }

	if(!str_cmp(arg2, "treasure"))
	{
		if(!str_cmp(arg3, "list"))
		{
			CHURCH_TREASURE_ROOM *treasure;
			ITERATOR it;

			int i = 0;
			iterator_start(&it, church->treasure_rooms);
			while( (treasure = (CHURCH_TREASURE_ROOM *)iterator_nextdata(&it)) )
			{
				if( i == 0 ) {
					sprintf(buf, "{YTreasure rooms for {W%s{Y:\n\r", church->name);
					send_to_char(buf, ch);
					send_to_char("{Y========================================================{x\n\r", ch);
				}

				i++;
				sprintf(buf, "%2d [%-8ld] %s\n\r", i, treasure->room->vnum, treasure->room->name);
				send_to_char(buf, ch);
			}
			iterator_stop(&it);

			if( i == 0 )
				send_to_char("There are no treasure rooms assigned.\n\r", ch);

			return;
		}

		if(!str_cmp(arg3, "add"))
		{
			if( argument[0] == '\0' )
			{
				send_to_char("set church <no> treasure add <vnum>\n\r", ch);
				return;
			}

			if(!is_number(argument))
			{
				send_to_char("That's not even a number.\n\r", ch);
				return;
			}

			long vnum = atol(argument);
			ROOM_INDEX_DATA *room = get_room_index(vnum);

			if(!room)
			{
				send_to_char("That's room does not exist.\n\r", ch);
				return;
			}

			if(is_treasure_room(NULL, room))
			{
				send_to_char("That room is already a treasure room.\n\r", ch);
				return;
			}

			if( !church_add_treasure_room(church, room, false) )
			{
				send_to_char("ERROR: could not add room to treasure rooms list.\n\r", ch);
				return;
			}

			send_to_char("Treasure room added.\n\r", ch);
			return;
		}

		if(!str_cmp(arg3, "remove"))
		{
			if( argument[0] == '\0' )
			{
				send_to_char("set church <no> treasure remove <vnum>\n\r", ch);
				return;
			}

			if(!is_number(argument))
			{
				send_to_char("That's not even a number.\n\r", ch);
				return;
			}

			long vnum = atol(argument);
			ROOM_INDEX_DATA *room = get_room_index(vnum);

			if(!room)
			{
				send_to_char("That's room does not exist.\n\r", ch);
				return;
			}

			if(!is_treasure_room(church, room))
			{
				send_to_char("That room is not a treasure room in the church.\n\r", ch);
				return;
			}

			church_remove_treasure_room(church, room);

			send_to_char("Treasure room removed.\n\r", ch);
			return;
		}

		send_to_char("set church <no> treasure list\n\r", ch);
		send_to_char("                         add <vnum>\n\r", ch);
		send_to_char("                         remove <vnum>\n\r", ch);
		return;
	}

/*
    if (!str_cmp(arg2, "treasure"))
    {
	if (get_room_index(atol(arg3)) == NULL)
	{
	    send_to_char("Room number is not valid.\n\r", ch);
	    return;
	}

	church->treasure_room = atoi(arg3);
	sprintf(buf, "%s treasure room set to %ld - %s\n\r",
	    church->name, church->treasure_room,
	    get_room_index(church->treasure_room)->name);
	send_to_char(buf, ch);
	return;
    }*/
}


void do_mset(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[100];
    CHAR_DATA *victim;
    int value;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strcpy(arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  set char <name> <field> <value>\n\r",ch);
	send_to_char("  Field being one of:\n\r",			ch);
	send_to_char("    str int wis dex con sex\n\r",	ch);
	send_to_char("    race gold silver hp mana move prac\n\r",ch);
	send_to_char("    align train thirst hunger drunk\n\r",	ch);
	send_to_char("    security pneuma dp qp title\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg1)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    /*
     * Snarf the value (which need not be numeric).
     */
    value = is_number(arg3) ? atoi(arg3) : -1;

    /*
     * Set something.
     */
    if (!str_cmp(arg2, "str"))
    {
	if (value < 3 || value > get_max_train(victim,STAT_STR))
	{
	    sprintf(buf,
		"Strength range is 3 to %d\n\r.",
		get_max_train(victim,STAT_STR));
	    send_to_char(buf,ch);
	    return;
	}

	set_perm_stat(victim, STAT_STR, value);
	return;
    }

    if (!str_cmp(arg2, "security"))	/* OLC */
    {
		int security = UMAX(9, ch->pcdata->security);
        if (IS_NPC(victim))
        {
            send_to_char("Not on NPC's.\n\r", ch);
            return;
        }



	if (value > security || value < 0) {
	    if (security > 0) {
			sprintf(buf, "Valid security is 0-%d.\n\r", security);
			send_to_char(buf, ch);
	    } else
			send_to_char("Valid security is 0 only.\n\r", ch);
	    return;
	}
	victim->pcdata->security = value;
	return;
    }

    if (!str_cmp(arg2, "qp"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Can't set that on an NPC.\n\r", ch);
	    return;
	}

	if (arg3[0] == '\0')
	{
	    send_to_char("Set how much?\n\r", ch);
	    return;
	}

	if (atoi(arg3) < 0 || atoi(arg3) > 30000)
	{
	    send_to_char("Sorry, that's out of range.\n\r", ch);
	    return;
	}

	victim->questpoints = atoi(arg3);
	return;
    }

    if (!str_cmp(arg2, "pneuma"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Can't set that on an NPC.\n\r", ch);
	    return;
	}

	if (arg3[0] == '\0')
	{
	    send_to_char("Set how much?\n\r", ch);
	    return;
	}

	if (atoi(arg3) < 0 || atoi(arg3) > 30000)
	{
	    send_to_char("Sorry, that's out of range.\n\r", ch);
	    return;
	}

	victim->pneuma = atoi(arg3);
	return;
    }

    if (!str_cmp(arg2, "dp"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Can't set that on an NPC.\n\r", ch);
	    return;
	}

	if (arg3[0] == '\0')
	{
	    send_to_char("Set how much?\n\r", ch);
	    return;
	}

	if (atoi(arg3) < 0 || atoi(arg3) > 4300000)
	{
	    send_to_char("Sorry, that's out of range.\n\r", ch);
	    return;
	}

	victim->deitypoints = atoi(arg3);
	return;
    }

    /* SYN - now redundant
    if (!str_cmp(arg2, "imm_title"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Can't set that on an NPC.\n\r", ch);
	    return;
	}

	if (victim->tot_level < LEVEL_IMMORTAL)
	{
	    send_to_char("Imm title is for imms only!\n\r", ch);
	    return;
	}

	if (arg3[0] == '\0')
	{
	    send_to_char("You must specify a title!\n\r", ch);
	    return;
	}

	if (strlen(arg3) > 45)
	{
	    send_to_char("That title is too long.\n\r", ch);
	    return;
	}

	free_string(victim->pcdata->imm_title);
	victim->pcdata->imm_title = str_dup(arg3);
	return;
    }
    */

    if (!str_cmp(arg2, "title"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Can't set that on an NPC.\n\r", ch);
	    return;
	}

	if (arg3[0] == '\0')
	{
	    send_to_char("You must specify a title!\n\r", ch);
	    return;
	}

	if (strlen(arg3) > 91)
	{
	    send_to_char("That title is too long.\n\r", ch);
	    return;
	}

	free_string(victim->pcdata->title);
	victim->pcdata->title = str_dup(arg3);
	send_to_char("Title set.\n\r", ch);
	return;
    }

    if (!str_cmp(arg2, "int"))
    {
        if (value < 3 || value > get_max_train(victim,STAT_INT))
        {
            sprintf(buf,
		"Intelligence range is 3 to %d.\n\r",
		get_max_train(victim,STAT_INT));
            send_to_char(buf,ch);
            return;
        }

		set_perm_stat(victim, STAT_INT, value);
        return;
    }

    if (!str_cmp(arg2, "wis"))
    {
	if (value < 3 || value > get_max_train(victim,STAT_WIS))
	{
	    sprintf(buf,
		"Wisdom range is 3 to %d.\n\r",get_max_train(victim,STAT_WIS));
	    send_to_char(buf, ch);
	    return;
	}

	set_perm_stat(victim, STAT_WIS, value);
	return;
    }

    if (!str_cmp(arg2, "dex"))
    {
	if (value < 3 || value > get_max_train(victim,STAT_DEX))
	{
	    sprintf(buf,
		"Dexterity range is 3 to %d.\n\r",
		get_max_train(victim,STAT_DEX));
	    send_to_char(buf, ch);
	    return;
	}

	set_perm_stat(victim, STAT_DEX, value);
	return;
    }

    if (!str_cmp(arg2, "con"))
    {
	if (value < 3 || value > get_max_train(victim,STAT_CON))
	{
	    sprintf(buf,
		"Constitution range is 3 to %d.\n\r",
		get_max_train(victim,STAT_CON));
	    send_to_char(buf, ch);
	    return;
	}

	set_perm_stat(victim, STAT_CON, value);
	return;
    }

    if (!str_prefix(arg2, "bodytype"))
    {
        body_type_t new_body_type = BODY_TYPE_NEUTRAL; // Default
        bool type_set = false;

        if (is_number(arg3))
        {
            int num_val = atoi(arg3);
            // BODY_TYPE_OTHER is 3, BODY_TYPE_RANDOM is 4
            int max_allowed_val = IS_NPC(victim) ? BODY_TYPE_RANDOM : BODY_TYPE_OTHER;

            if (num_val >= 0 && num_val <= max_allowed_val)
            {
                new_body_type = (body_type_t)num_val;
                type_set = true;
            }
            else
            {
                sprintf(buf, "Numeric body type for %s must be 0-%d.\n\r",
                        IS_NPC(victim) ? "NPCs" : "PCs", max_allowed_val);
                send_to_char(buf, ch);
                return;
            }
        }
        else // String input
        {
            int flag_val = flag_value(body_types, arg3); // body_type_flags needs to be accessible
            if (flag_val != NO_FLAG)
            {
                if (!IS_NPC(victim) && flag_val == BODY_TYPE_RANDOM)
                {
                    send_to_char("PCs cannot be set to 'random' body type. Choose neutral, male, female, or other.\n\r", ch);
                    return;
                }
                new_body_type = (body_type_t)flag_val;
                type_set = true;
            }
            else
            {
                char type_list_buf[MAX_STRING_LENGTH] = "Invalid body type name. Valid names/numbers are: ";
                for (int i = 0; body_types[i].name != NULL; i++) {
                    if (!IS_NPC(victim) && body_types[i].bit == BODY_TYPE_RANDOM) continue;
                    char temp_buf[50];
                    sprintf(temp_buf, "%s (%ld), ", body_types[i].name, body_types[i].bit);
                    if (strlen(type_list_buf) + strlen(temp_buf) < MAX_STRING_LENGTH - 3) {
                        strcat(type_list_buf, temp_buf);
                    } else {
                        strcat(type_list_buf, "..."); // Indicate list was truncated
                        break;
                    }
                }
                // Remove trailing ", "
                if (strlen(type_list_buf) > 2 && strcmp(&type_list_buf[strlen(type_list_buf)-2], ", ") == 0) {
                    type_list_buf[strlen(type_list_buf)-2] = '\0';
                }
                strcat(type_list_buf, ".\n\r");
                send_to_char(type_list_buf, ch);
                return;
            }
        }

        if (type_set)
        {
            if (IS_NPC(victim))
            {
                victim->body_type = new_body_type;
            }
            else
            {
                if (victim->pcdata == NULL) {
                     send_to_char("Victim has no player data to set body type.\n\r", ch);
                     return;
                }
                victim->body_type = new_body_type;
            }

            // This function should handle setting pronouns on ch-> or ch->pcdata->
            reset_pronouns_to_body_type(victim, new_body_type);

            // Update old sex field for compatibility
            switch (new_body_type)
            {
                case BODY_TYPE_MALE:
                    victim->sex = SEX_MALE; // Assumes SEX_MALE, SEX_FEMALE, SEX_NEUTRAL are defined
                    break;
                case BODY_TYPE_FEMALE:
                    victim->sex = SEX_FEMALE;
                    break;
                default: // BODY_TYPE_NEUTRAL, BODY_TYPE_OTHER, BODY_TYPE_RANDOM
                    victim->sex = SEX_NEUTRAL;
                    break;
            }
            if (!IS_NPC(victim) && victim->pcdata != NULL)
            {
                victim->pcdata->true_sex = victim->sex; // If true_sex is still used
            }

            sprintf(buf, "%s's body type set to %s.\n\rPronouns reset to defaults for this type.\n\r",
                    capitalize(victim->name), body_type_info[new_body_type].name); // body_type_info needs to be accessible
            send_to_char(buf, ch);
            return;
        }
        send_to_char("Failed to set body type due to an unexpected error.\n\r", ch);
        return;
    }

    if (!str_prefix(arg2, "class"))
    {
	int class;

	if (IS_NPC(victim))
	{
	    send_to_char("Mobiles have no class.\n\r",ch);
	    return;
	}

	class = class_lookup(arg3);
	if (class == -1)
	{
	    char buf[MAX_STRING_LENGTH];

        	strcpy(buf, "Possible classes are: ");
        	for (class = 0; class < MAX_CLASS; class++)
        	{
            	    if (class > 0)
                    	strcat(buf, " ");
            	    strcat(buf, class_table[class].name);
        	}
            strcat(buf, ".\n\r");

	    send_to_char(buf,ch);
	    return;
	}

	victim->pcdata->class_current = class;
	return;
    }

    if (!str_prefix(arg2, "level"))
    {
	if (!IS_NPC(victim))
	{
	    send_to_char("Not on PC's.\n\r", ch);
	    return;
	}

	if (value < 0 || value > MAX_LEVEL)
	{
	    sprintf(buf, "Level range is 0 to %d.\n\r", MAX_LEVEL);
	    send_to_char(buf, ch);
	    return;
	}
	victim->level = value;
	return;
    }

    if (!str_prefix(arg2, "gold"))
    {
	victim->gold = value;
	return;
    }

    if (!str_prefix(arg2, "silver"))
    {
	victim->silver = value;
	return;
    }

    if (!str_prefix(arg2, "hp"))
    {
	if (value < -10 || value > 30000)
	{
	    send_to_char("Hp range is -10 to 30,000 hit points.\n\r", ch);
	    return;
	}
	victim->max_hit = value;
        if (!IS_NPC(victim))
            victim->pcdata->perm_hit = value;
	return;
    }

    if (!str_prefix(arg2, "mana"))
    {
	if (value < 0 || value > 30000)
	{
	    send_to_char("Mana range is 0 to 30,000 mana points.\n\r", ch);
	    return;
	}
	victim->max_mana = value;
        if (!IS_NPC(victim))
            victim->pcdata->perm_mana = value;
	return;
    }

    if (!str_prefix(arg2, "move"))
    {
	if (value < 0 || value > 30000)
	{
	    send_to_char("Move range is 0 to 30,000 move points.\n\r", ch);
	    return;
	}
	victim->max_move = value;
        if (!IS_NPC(victim))
            victim->pcdata->perm_move = value;
	return;
    }

    if (!str_prefix(arg2, "practice"))
    {
	if (value < 0 || value > 250)
	{
	    send_to_char("Practice range is 0 to 250 sessions.\n\r", ch);
	    return;
	}
	victim->practice = value;
	return;
    }

    if (!str_prefix(arg2, "train"))
    {
	if (value < 0 || value > 50)
	{
	    send_to_char("Training session range is 0 to 50 sessions.\n\r",ch);
	    return;
	}
	victim->train = value;
	return;
    }

    if (!str_prefix("align", arg2))
    {
	if (value < -1000 || value > 1000)
	{
	    send_to_char("Alignment range is -1000 to 1000.\n\r", ch);
	    return;
	}
	victim->alignment = value;
	return;
    }

    if (!str_prefix(arg2, "thirst"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Not on NPC's.\n\r", ch);
	    return;
	}

	if (value < -1 || value > 100)
	{
	    send_to_char("Thirst range is -1 to 100.\n\r", ch);
	    return;
	}

	victim->pcdata->condition[COND_THIRST] = value;
	return;
    }

    if (!str_prefix(arg2, "drunk"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Not on NPC's.\n\r", ch);
	    return;
	}

	if (value < -1 || value > 100)
	{
	    send_to_char("Drunk range is -1 to 100.\n\r", ch);
	    return;
	}

	victim->pcdata->condition[COND_DRUNK] = value;
	return;
    }

    if (!str_prefix(arg2, "full"))
    {
	if (IS_NPC(victim))
	{
	    send_to_char("Not on NPC's.\n\r", ch);
	    return;
	}

	if (value < -1 || value > 100)
	{
	    send_to_char("Full range is -1 to 100.\n\r", ch);
	    return;
	}

	victim->pcdata->condition[COND_FULL] = value;
	return;
    }

    if (!str_prefix(arg2, "hunger"))
    {
        if (IS_NPC(victim))
        {
            send_to_char("Not on NPC's.\n\r", ch);
            return;
        }

        if (value < -1 || value > 100)
        {
            send_to_char("Full range is -1 to 100.\n\r", ch);
            return;
        }

        victim->pcdata->condition[COND_HUNGER] = value;
        return;
    }

    if (!str_cmp(arg2, "hunt"))
    {
        CHAR_DATA *hunted = 0;

        if (!IS_NPC(victim))
        {
            send_to_char("Not on PC's.\n\r", ch);
            return;
        }

        if (str_cmp(arg3, "."))
          if ((hunted = get_char_area(victim, arg3)) == NULL)
            {
              send_to_char("Mob couldn't locate the victim to hunt.\n\r", ch);
              return;
            }

        victim->hunting = hunted;
        return;
    }

    if (!str_prefix(arg2, "race"))
    {
	int race;

	race = race_lookup(arg3);

	if (race == 0)
	{
	    send_to_char("That is not a valid race.\n\r",ch);
	    return;
	}

	if (!IS_NPC(victim) && !race_table[race].pc_race)
	{
	    send_to_char("That is not a valid player race.\n\r",ch);
	    return;
	}

	victim->race = race;
	victim->affected_by_perm[0] = race_table[victim->race].aff;
	victim->affected_by_perm[1] = race_table[victim->race].aff2;
    victim->imm_flags_perm = race_table[victim->race].imm;
    victim->res_flags_perm = race_table[victim->race].res;
    victim->vuln_flags_perm = race_table[victim->race].vuln;
    affect_fix_char(victim);

    victim->form        = race_table[victim->race].form;
    victim->parts       = race_table[victim->race].parts;
    victim->lostparts	= 0;

	return;
    }

    /* Syn -  unused
    if (!str_prefix(arg2,"group"))
    {
	if (!IS_NPC(victim))
	{
	    send_to_char("Only on NPCs.\n\r",ch);
	    return;
	}
	victim->group = value;
	return;
    }
    */

    /*
     * Generate usage message.
     */
    do_function(ch, &do_mset, "");
    return;
}


void do_string(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
//    char buf[MSL];
    OBJ_DATA *obj;

    smash_tilde(argument);
    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0' || argument[0] == '\0' )
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  string <object> <field> <string>\n\r",ch);
	send_to_char("  fields: name short long\n\r",ch);
	return;
    }

    if ((obj = get_obj_list(ch, arg, ch->lcarrying)) == NULL)
    {
	send_to_char("Nothing like that in your inventory.\n\r", ch);
	return;
    }

    if (!str_prefix(arg2, "name"))
    {
	free_string(obj->name);
	obj->name = str_dup(argument);
	act("Strung $p's name to '$t'.", ch, NULL, NULL, obj, NULL, argument, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    if (!str_prefix(arg2, "short"))
    {
	free_string(obj->short_descr);
	obj->short_descr = str_dup(argument);
	act("Strung $p's short to '$t'.", ch, NULL, NULL, obj, NULL, argument, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    if (!str_prefix(arg2, "long"))
    {
	free_string(obj->description);
	obj->description = str_dup(argument);
	act("Strung $p's long to '$t'.", ch, NULL, NULL, obj, NULL, argument, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    do_function(ch, &do_string, "");
}


void do_oset(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int value;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strcpy(arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  set obj <object> <field> <value>\n\r",ch);
	send_to_char("  Field being one of:\n\r",				ch);
	send_to_char("    value0 value1 value2 value3 value4 (v1-v4)\n\r",	ch);
	send_to_char("    extra wear level weight cost timer\n\r",		ch);
	return;
    }

    if ((obj = get_obj_world(ch, arg1)) == NULL)
    {
	send_to_char("Nothing like that in heaven or earth.\n\r", ch);
	return;
    }

    /*
     * Snarf the value (which need not be numeric).
     */
    value = atoi(arg3);

    /*
     * Set something.
     */
    if (!str_cmp(arg2, "value0") || !str_cmp(arg2, "v0"))
    {
	obj->value[0] = UMIN(50,value);
	return;
    }

    if (!str_cmp(arg2, "value1") || !str_cmp(arg2, "v1"))
    {
	obj->value[1] = value;
	return;
    }

    if (!str_cmp(arg2, "value2") || !str_cmp(arg2, "v2"))
    {
	obj->value[2] = value;
	return;
    }

    if (!str_cmp(arg2, "value3") || !str_cmp(arg2, "v3"))
    {
	obj->value[3] = value;
	return;
    }

    if (!str_cmp(arg2, "value4") || !str_cmp(arg2, "v4"))
    {
	obj->value[4] = value;
	return;
    }

    if (!str_prefix(arg2, "extra"))
    {
	obj->extra[0] = value;
	return;
    }

    if (!str_prefix(arg2, "wear"))
    {
	obj->wear_flags = value;
	return;
    }

    if (!str_prefix(arg2, "level"))
    {
	obj->level = value;
	return;
    }

    if (!str_prefix(arg2, "weight"))
    {
	obj->weight = value;
	return;
    }

    if (!str_prefix(arg2, "cost"))
    {
	obj->cost = value;
	return;
    }

    if (!str_prefix(arg2, "timer"))
    {
	obj->timer = value;
	return;
    }

    /*
     * Generate usage message.
     */
    do_function(ch, &do_oset, "");
    return;
}



void do_rset(CHAR_DATA *ch, char *argument)
{
    char arg1 [MAX_INPUT_LENGTH];
    char arg2 [MAX_INPUT_LENGTH];
    char arg3 [MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *location;
    int value;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strcpy(arg3, argument);

    if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0')
    {
	send_to_char("Syntax:\n\r",ch);
	send_to_char("  set room <location> <field> <value>\n\r",ch);
	send_to_char("  Field being one of:\n\r",			ch);
	send_to_char("    flags sector\n\r",				ch);
	return;
    }

    if ((location = find_location(ch, arg1)) == NULL)
    {
	send_to_char("No such location.\n\r", ch);
	return;
    }

    if (!is_room_owner(ch,location) && ch->in_room != location
    &&  room_is_private(location, ch))
    {
        send_to_char("That room is private right now.\n\r",ch);
        return;
    }

    /*
     * Snarf the value.
     */
    if (!is_number(arg3))
    {
	send_to_char("Value must be numeric.\n\r", ch);
	return;
    }
    value = atoi(arg3);

    /*
     * Set something.
     */
    if (!str_prefix(arg2, "flags"))
    {
	location->room_flag[0]	= value;
	return;
    }

    if (!str_prefix(arg2, "sector"))
    {
	location->sector_type	= value;
	return;
    }

    /*
     * Generate usage message.
     */
    do_function(ch, &do_rset, "");
    return;
}



void do_sockets(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;
    char buf[2 * MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    char arg[250];
    char arg_type[50];
    int count;
    char s[100];
    char idle[20];
    bool found_match = false;
    int search_type = 0; // 0 = name, 1 = host, 2 = account, 3 = state

    count = 0;
    buf[0] = '\0';
    buf2[0] = '\0';

    strcat(buf2, "\n\r{D[{xNum Connected_State Login@ Idl{D]{W Name{x         Account        Host\n\r");
    strcat(buf2, "{D-----------------------------------------------------------------------------------{x\n\r");

    argument = one_argument(argument, arg);

    if (arg[0] != '\0' && argument[0] != '\0') {
        argument = one_argument(argument, arg_type);
        
        if (!str_prefix(arg_type, "host"))
            search_type = 1;
        else if (!str_prefix(arg_type, "account"))
            search_type = 2;
        else if (!str_prefix(arg_type, "state"))
            search_type = 3;
        else {
            // If arg_type isn't a valid search type, treat it as part of the search term
            // and reset arg to contain both parts
            char full_arg[MAX_INPUT_LENGTH];
            sprintf(full_arg, "%s %s", arg, arg_type);
            strcpy(arg, full_arg);
            search_type = 0; // Default to name search
        }
    }

    for (d = descriptor_list; d; d = d->next) {
        // Skip this descriptor if it doesn't match our search criteria
        if (arg[0] != '\0') {
            found_match = false;
            
            switch (search_type) {
                case 0: // Default search by character name
                    if (d->character && can_see(ch, d->character) && 
                        (is_name(arg, d->character->name) ||
                         (d->original && is_name(arg, d->original->name))))
                        found_match = true;
                    break;
                    
                case 1: // Search by host
                    if (d->host && strstr(d->host, arg))
                        found_match = true;
                    break;
                    
                case 2: // Search by account
                    if (d->account && strstr(d->account->username, arg))
                        found_match = true;
                    break;
                    
                case 3: // Search by connection state
                    if (d->connected < CON_MAX && 
                        strstr(con_states[d->connected].name, arg))
                        found_match = true;
                    break;
            }
            
            if (!found_match)
                continue;
        }

        if (d->character != NULL && !can_see(ch, d->character))
            continue;

        count++;

        /* Get connection state from the table */
        const char *state_name = "UNKNOWN";
        if (d->connected >= 0 && d->connected < CON_MAX)
            state_name = con_states[d->connected].name;

        /* Format "login" value... */
        CHAR_DATA *vch = d->original ? d->original : d->character;
        if (vch)
            strftime(s, 100, "%I:%M%p", localtime(&vch->logon));
        else
            strcpy(s, "------");

        /* Format idle time */
        if (vch && vch->timer > 0)
            sprintf(idle, "%-2d", vch->timer);
        else
            sprintf(idle, "  ");

        /* Get character name */
        const char *char_name = "(None!)";
        if (d->original)
            char_name = d->original->name;
        else if (d->character)
            char_name = d->character->name;

        /* Get account name */
        const char *acct_name = "(None)";
        if (d->account)
            acct_name = d->account->username;

        sprintf(buf, "{D[{x%s%3d{X %-15.15s %7s{g %2s{D]{W %-12s{x %-14s %-30.30s\n\r",
            d->ssl ? "{G" : "{X",
            d->descriptor,
            state_name,
            s,
            idle,
            char_name,
            acct_name,
            d->host);

        strcat(buf2, buf);
    }

    if (count == 0) {
        if (arg[0] == '\0')
            send_to_char("No one is connected.\n\r", ch);
        else
            send_to_char("No matching connections found.\n\r", ch);
        return;
    }

    sprintf(buf, "\n\r%d user%s\n\r", count, count == 1 ? "" : "s");
    strcat(buf2, buf);
    strcat(buf2, "{D-----------------------------------------------------------------------------------{x\n\r");
    send_to_char(buf2, ch);
    return;
}

void do_force(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
	send_to_char("Force whom to do what?\n\r", ch);
	return;
    }

    one_argument(argument,arg2);

    if (!str_cmp(arg2,"delete") || !str_prefix(arg2,"mob"))
    {
	send_to_char("That will NOT be done.\n\r",ch);
	return;
    }

    sprintf(buf, "$n forces you to '%s'.", argument);

    if (!str_cmp(arg, "room"))
    {
	CHAR_DATA *victim_next;

	for (victim = ch->in_room->people; victim != NULL; victim = victim_next)
	{
	    victim_next = victim->next_in_room;

	    if (victim != ch && victim->tot_level < ch->tot_level) {
		act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
		interpret(victim, argument);
	    }
	}
	return;
    }

    if (!str_cmp(arg, "all"))
    {
	DESCRIPTOR_DATA *desc;
	DESCRIPTOR_DATA *desc_next;

	if (ch->tot_level < MAX_LEVEL - 2)
	{
	    send_to_char("Not at your level!\n\r",ch);
	    return;
	}

	for (desc = descriptor_list; desc != NULL; desc = desc_next)
	{
	    desc_next = desc->next;

	    if (desc->connected == CON_PLAYING
	    &&  get_staff_rank(desc->character) < get_staff_rank(ch))
	    {
		act(buf, ch, desc->character, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
		interpret(desc->character, argument);
	    }
	}
    }
    else if (!str_cmp(arg, "gods"))
    {
        DESCRIPTOR_DATA *desc,*desc_next;

        if (ch->tot_level < MAX_LEVEL - 1)
	{
            send_to_char("Not at your level!\n\r",ch);
	    return;
        }

        for (desc = descriptor_list; desc != NULL; desc = desc_next)
	{
            desc_next = desc->next;

	    if (desc->connected==CON_PLAYING
	    &&  get_staff_rank(desc->character) < get_staff_rank(ch)
            &&  desc->character->level >= LEVEL_HERO)
	    {
		act(buf, ch, desc->character, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
		interpret(desc->character, argument);
	    }
        }
    }
    else
    {
	CHAR_DATA *victim;

	if ((victim = get_char_world(ch, arg)) == NULL)
	{
	    send_to_char("They aren't here.\n\r", ch);
	    return;
	}

	if (victim == ch)
	{
	    send_to_char("Aye aye, right away!\n\r", ch);
	    return;
	}

    	if (!is_room_owner(ch,victim->in_room)
	&& ch->in_room != victim->in_room
        && room_is_private(victim->in_room, ch))
    	{
            send_to_char("That character is in a private room.\n\r",ch);
            return;
        }

	if (get_staff_rank(victim) >= get_staff_rank(ch)
	&&   ch->pcdata->staff_rank < STAFF_IMPLEMENTOR
	&&   !IS_NPC(victim))
	{
	    send_to_char("Do it yourself!\n\r", ch);
	    return;
	}

	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
	
	char name[MIL];
	strncpy(name, victim->name, MIL-1);
	interpret(victim, argument);
	act("Forced $N to \"$t\".", ch, victim, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    }

    return;
}


void do_invis(CHAR_DATA *ch, char *argument)
{
    int level;
    char arg[MAX_STRING_LENGTH];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    /* take the default path */

      if (ch->invis_level)
      {
	  ch->invis_level = 0;
	  act("$n slowly fades into existence.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
	  send_to_char("You slowly fade back into existence.\n\r", ch);
      }
      else
      {
	  ch->invis_level = get_staff_rank(ch);
	  act("$n slowly fades into thin air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
	  send_to_char("You slowly vanish into thin air.\n\r", ch);
      }
    else
    /* do the level thing */
    {
      level = atoi(arg);
      if (level < 2 || level > get_staff_rank(ch))
      {
	send_to_char("Invis level must be between 2 and your level.\n\r",ch);
        return;
      }
      else
      {
	  ch->reply = NULL;
          ch->invis_level = level;
          act("$n slowly fades into thin air.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
          send_to_char("You slowly vanish into thin air.\n\r", ch);
      }
    }

    return;
}


void do_incognito(CHAR_DATA *ch, char *argument)
{
    int level;
    char arg[MAX_STRING_LENGTH];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    /* take the default path */

      if (ch->incog_level)
      {
          ch->incog_level = 0;
          act("$n is no longer cloaked.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
          send_to_char("You are no longer cloaked.\n\r", ch);
      }
      else
      {
          ch->incog_level = get_staff_rank(ch);
          act("$n cloaks $s presence.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
          send_to_char("You cloak your presence.\n\r", ch);
      }
    else
    /* do the level thing */
    {
      level = atoi(arg);
      if (level < 2 || level > get_staff_rank(ch))
      {
        send_to_char("Incog level must be between 2 and your level.\n\r",ch);
        return;
      }
      else
      {
          ch->reply = NULL;
          ch->incog_level = level;
          act("$n cloaks $s presence.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
          send_to_char("You cloak your presence.\n\r", ch);
      }
    }

    return;
}


void do_holylight(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
	return;

    if (IS_SET(ch->act[0], PLR_HOLYLIGHT))
    {
	REMOVE_BIT(ch->act[0], PLR_HOLYLIGHT);
	send_to_char("Holy light mode off.\n\r", ch);
    }
    else
    {
	SET_BIT(ch->act[0], PLR_HOLYLIGHT);
	send_to_char("Holy light mode on.\n\r", ch);
    }

    return;
}

void do_holywarp(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
		return;

    if (IS_SET(ch->act[1], PLR_HOLYWARP))
    {
		REMOVE_BIT(ch->act[1], PLR_HOLYWARP);
		send_to_char("Holy warp mode off.\n\r", ch);
    }
    else
    {
		SET_BIT(ch->act[1], PLR_HOLYWARP);
		send_to_char("Holy warp mode on.\n\r", ch);
    }

    return;
}

void do_holyaura(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
	return;

    if (IS_SET(ch->act[1], PLR_HOLYAURA))
    {
	REMOVE_BIT(ch->act[1], PLR_HOLYAURA);
	send_to_char("Holy aura mode off.\n\r", ch);
    }
    else
    {
	SET_BIT(ch->act[1], PLR_HOLYAURA);
	send_to_char("Holy aura mode on.\n\r", ch);
    }

    return;
}

void do_olevel(CHAR_DATA *ch, char *argument)
{
	ITERATOR it;
    char buf[MAX_INPUT_LENGTH];
    char min[MAX_INPUT_LENGTH];
    char max[MAX_INPUT_LENGTH];
    char type[MAX_INPUT_LENGTH];
    char wear_loc[MAX_INPUT_LENGTH];
    BUFFER *buffer;
    OBJ_DATA *obj;
    OBJ_DATA *in_obj;
    bool found;
    int number = 0, max_found;
    found = false;
    number = 0;
    max_found = 200;
    buffer = new_buf();

    argument = one_argument(argument, min);
    argument = one_argument(argument, max);
    argument = one_argument(argument, type);
    argument = one_argument(argument, wear_loc);

    if (min[0] == '\0')
    {
        send_to_char("Syntax: olevel <min> <max> <type> <wear_loc>\n\r", ch);
	return;
    }

	iterator_start(&it, loaded_objects);
	while(( obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
//	    if (next_obj != NULL
//	    && obj->pIndexData->vnum == next_obj->pIndexData->vnum)
//		    continue;

	    if (obj->level < atoi(min) || obj->level > atoi(max))
		    continue;

	    if (type[0] != '\0' && flag_value(type_flags, type) != obj->pIndexData->item_type)
		    continue;

	    if (wear_loc[0] != '\0' && !IS_SET(obj->wear_flags, flag_value(wear_flags, wear_loc)))
		    continue;

	    found = true;
	    number++;
	    for (in_obj = obj; in_obj->in_obj != NULL; in_obj = in_obj->in_obj);

	    if (in_obj->carried_by != NULL &&
	    	can_see(ch,in_obj->carried_by) &&
	    	in_obj->carried_by->in_room != NULL)
		    sprintf(buf, "%3d) %s (vnum %ld) is carried by %s [Room %ld]\n\r",
				number,
				obj->short_descr,
				obj->pIndexData->vnum,
				pers(in_obj->carried_by, ch),
				in_obj->carried_by->in_room->vnum);
	    else if (in_obj->in_room != NULL && can_see_room(ch,in_obj->in_room))
		    sprintf(buf, "%3d) %s (vnum %ld) is in %s [Room %ld]\n\r",
				number,
				obj->short_descr,
				obj->pIndexData->vnum,
				in_obj->in_room->name,
				in_obj->in_room->vnum);
	    else
		    sprintf(buf, "%3d) %s (vnum %ld) is somewhere\n\r",
				number,
				obj->short_descr,
				obj->pIndexData->vnum);

	    buf[0] = UPPER(buf[0]);
	    add_buf(buffer,buf);
	    if (number >= max_found)
		    break;
    }
    iterator_stop(&it);

    if (!found)
	    send_to_char("Nothing like that in heaven or earth.\n\r", ch);
    else
	    page_to_char(buf_string(buffer),ch);

    free_buf(buffer);
}


void do_mlevel(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_INPUT_LENGTH];
	BUFFER *buffer;
	CHAR_DATA *victim;
	bool found;
	int count = 0;
	ITERATOR vit;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax: mlevel <level>\n\r",ch);
		return;
	}
	found = false;
	buffer = new_buf();
	iterator_start(&vit, loaded_chars);
	while(( victim = (CHAR_DATA *)iterator_nextdata(&vit)))
	{
		if (victim->in_room != NULL &&
			atoi(argument) == victim->level) {
			found = true;
			count++;
			sprintf(buf, "%3d) [%5ld] %-28s [%5ld] %s\n\r",
					count,
					IS_NPC(victim) ?
					victim->pIndexData->vnum : 0,
					IS_NPC(victim) ?
					victim->short_descr : victim->name,
					victim->in_room->vnum,
					victim->in_room->name);
			add_buf(buffer,buf);
		}
	}
	iterator_stop(&vit);

	if (!found)
		act("You didn't find any mob of level $T.",
				ch, NULL, NULL, NULL, NULL, NULL, argument, TO_CHAR, NULL, NULL);
	else
		page_to_char(buf_string(buffer),ch);

	free_buf(buffer);
	return;
}


void do_reckoning(CHAR_DATA *ch, char *argument)
{
    struct tm *reck_time;

    if( argument[0] == '\0' )
    {
		if( reckoning_timer > 0 )
		{
			send_to_char("There is already a reckoning in progress.\n\r", ch);
		}
		else
		{
			reckoning_intensity = 100;
			reckoning_duration = 30;
			reckoning_cooldown = 0;

			reck_time = (struct tm *) localtime(&current_time);
			reck_time->tm_min += reckoning_duration;
			reckoning_timer = (time_t) mktime(reck_time);
			reckoning_cooldown_timer = 0;
			pre_reckoning = 1;

			send_to_char("{RLet the reckoning begin.{x\n\r", ch);
		}
		return;
	}

	if( !str_prefix(argument, "info") )
	{
		send_to_char("Coming soon.\n\r", ch);
		return;
	}

	send_to_char("Syntax:  reckoning         - Initiates a reckoning\n\r", ch);
	send_to_char("         reckoning info    - Provides information about The Reckoning\n\r", ch);

}


void do_immortalise(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    //OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    //char buf[MAX_STRING_LENGTH];
    //char buf2[MSL];
    int i;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		send_to_char("Immortalise whom?\n\r", ch);
		send_to_char("Syntax: immortalise <person> <subclass>\n\r", ch);
		return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
		send_to_char("They aren't online.\n\r", ch);
		return;
    }

    if (IS_NPC(victim))
    {
		send_to_char("You can't immortalise NPCs.\n\r", ch);
		return;
    }

    if (IS_REMORT(victim))
    {
		send_to_char("That person has already been immortalised.\n\r", ch);
		return;
    }

    if (victim->tot_level < LEVEL_HERO)
    {
		act("$N must be at max level to remort.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
		return;
    }

    if (*argument == '\0')
    {
		show_multiclass_choices(victim, ch);
		return;
    }

    for (i = CLASS_WARRIOR_WARLORD; i < MAX_SUB_CLASS; i++) {
		if (!str_cmp(argument, sub_class_table[i].name[victim->sex]))
		    break;
    }

    if (i == MAX_SUB_CLASS) {
		send_to_char("Not a valid subclass.\n\r", ch);
		return;
    }

    if (!can_choose_subclass(victim, i))
    {
		act("$N cannot choose that subclass.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
		return;
    }

	remort_player(victim, i);

#if 0
    sprintf(argument, "%s", sub_class_table[i].name[0]);

    i = 0;
    victim->race = get_remort_race(victim);
    sprintf(buf2, "%s", pc_race_table[victim->race].name);
    while (buf2[i] != '\0')
    {
	buf2[i] = UPPER(buf2[i]);
	i++;
    }

    if (victim->alignment < 0)
    {
        sprintf(buf, "{RHoly statues cry tears of blood and the sillhouettes "
		      "of winged horrors appear in the sky.{X\n\r{RA new %s has been born!{x\n\r", buf2);

	victim->alignment = -1000;

    send_to_char("Your mortal essence crumbles as you embrace your fate.\n\r", victim);
	send_to_char("You welcome the dark power as it flows through your divine veins.\n\r", victim);
	send_to_char("A dark influence clouds all that you once knew; your lifeless body\n\r", victim);
	send_to_char("lies slouched in front of you as part of you is torn into the Abyss.\n\r", victim);
	send_to_char("You feel complete, and wielding unfathomable power, you know you can\n\r", victim);
	send_to_char("manipulate it to suit your darkest desires.\n\r", victim);
    }
    else if (victim->alignment > 0)
    {
	sprintf(buf, "{WBrilliant white light radiates down from the heavens and thunder rolls through the valleys.\n\r"
	             "{WA new %s has been born!{x\n\r", buf2);

	victim->alignment = 1000;

 	send_to_char("Your mortal essence shines brightly, blinding your eyes.\n\r", victim);
	send_to_char("Images flash before you: sadness, grief, terror and hatred.\n\r", victim);
	send_to_char("Your life is played to you, from the beginning to the present.\n\r", victim);
	send_to_char("Your veins flow with the divine influence as you stand before your\n\r", victim);
	send_to_char("lifeless mortal vessel. It becomes clear to you that you have been\n\r", victim);
	send_to_char("reborn a divine power.\n\r", victim);
    }
    else
    {
	sprintf(buf, "{CThe cosmic energies of the world shift and the clouds speed overhead.{x\n\r"
				 "{CA new %s has been born!{x\n\r", buf2);

	victim->alignment = 0;
    }

    gecho(buf);

    /* take off equipment*/
    for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
    {
		if (obj->wear_loc != WEAR_NONE)
		    unequip_char(victim, obj, false);
    }

    /* take off remaining affects*/
    while (victim->affected)
		affect_remove(victim, victim->affected);

    /* lower their stats significantly*/
    for (i = 0; i < MAX_STATS; i++) {
		int val = victim->perm_stat[i] - number_range(4,6);
		set_perm_stat(victim, i, UMAX(val, 13));
	}

	victim->affected_by_perm[0] = race_table[victim->race].aff;
	victim->affected_by_perm[1] = race_table[victim->race].aff2;
    victim->imm_flags_perm = race_table[victim->race].imm;
    victim->res_flags_perm = race_table[victim->race].res;
    victim->vuln_flags_perm = race_table[victim->race].vuln;

    victim->form        = race_table[victim->race].form;
    victim->parts       = race_table[victim->race].parts;
    victim->lostparts	= 0;	// Restore anything lost

    /* add skills for remort race*/
    for (i = 0; pc_race_table[victim->race].skills[i] != NULL; i++)
	group_add(victim,pc_race_table[victim->race].skills[i],false);

    victim->pcdata->hit_before  = victim->pcdata->perm_hit;
    victim->pcdata->mana_before = victim->pcdata->perm_mana;
    victim->pcdata->move_before = victim->pcdata->perm_move;

    victim->pcdata->perm_hit  = 20;
    victim->pcdata->perm_mana = 20;
    victim->pcdata->perm_move = 20;

    victim->max_hit  = 20;
    victim->max_mana = 20;
    victim->max_move = 20;

    victim->hit  = 20;
    victim->mana = 20;
    victim->move = 20;

    victim->tot_level = 1;
    victim->level = 1;

	// Reset base affects - will reset affected_by, affected_by2, imm_flags, res_flags and vuln_flags
    affect_fix_char(victim);

    char_from_room(victim);
    char_to_room(victim, get_room_index(ROOM_VNUM_SCHOOL));

    /* mages*/
    if (!str_cmp("archmage", argument)
    ||  !str_cmp("geomancer", argument)
    ||  !str_cmp("illusionist", argument))
    {
	victim->pcdata->class_current = CLASS_MAGE;
	victim->pcdata->second_class_mage = CLASS_MAGE;

	if (!str_cmp("archmage", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_MAGE_ARCHMAGE;
	    victim->pcdata->second_sub_class_mage = CLASS_MAGE_ARCHMAGE;
	}

	if (!str_cmp("geomancer", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_MAGE_GEOMANCER;
	    victim->pcdata->second_sub_class_mage = CLASS_MAGE_GEOMANCER;
	}

	if (!str_cmp("illusionist", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_MAGE_ILLUSIONIST;
	    victim->pcdata->second_sub_class_mage = CLASS_MAGE_ILLUSIONIST;
	}
    }

    /* clerics*/
    if (!str_cmp("alchemist", argument)
    ||  !str_cmp("ranger", argument)
    ||  !str_cmp("adept", argument))
    {
	victim->pcdata->class_current = CLASS_CLERIC;
	victim->pcdata->second_class_cleric = CLASS_CLERIC;

	if (!str_cmp("alchemist", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_CLERIC_ALCHEMIST;
	    victim->pcdata->second_sub_class_cleric = CLASS_CLERIC_ALCHEMIST;
	}

	if (!str_cmp("ranger", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_CLERIC_RANGER;
	    victim->pcdata->second_sub_class_cleric = CLASS_CLERIC_RANGER;
	}

	if (!str_cmp("adept", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_CLERIC_ADEPT;
	    victim->pcdata->second_sub_class_cleric = CLASS_CLERIC_ADEPT;
	}
    }

    /* thieves*/
    if (!str_cmp("highwayman", argument)
    ||  !str_cmp("ninja", argument)
    ||  !str_cmp("sage", argument))
    {
	victim->pcdata->class_current = CLASS_THIEF;
	victim->pcdata->second_class_thief = CLASS_THIEF;

	if (!str_cmp("highwayman", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_THIEF_HIGHWAYMAN;
	    victim->pcdata->second_sub_class_thief = CLASS_THIEF_HIGHWAYMAN;
	}

	if (!str_cmp("ninja", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_THIEF_NINJA;
	    victim->pcdata->second_sub_class_thief = CLASS_THIEF_NINJA;
	}

	if (!str_cmp("sage", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_THIEF_SAGE;
	    victim->pcdata->second_sub_class_thief = CLASS_THIEF_SAGE;
	}
    }

    /* warriors*/
    if (!str_cmp("warlord", argument)
    || !str_cmp("destroyer", argument)
    || !str_cmp("crusader", argument))
    {
	victim->pcdata->class_current = CLASS_WARRIOR;
	victim->pcdata->second_class_warrior = CLASS_WARRIOR;

	if (!str_cmp("warlord", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_WARRIOR_WARLORD;
	    victim->pcdata->second_sub_class_warrior = CLASS_WARRIOR_WARLORD;
	}

	if (!str_cmp("destroyer", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_WARRIOR_DESTROYER;
	    victim->pcdata->second_sub_class_warrior = CLASS_WARRIOR_DESTROYER;
	}

	if (!str_cmp("crusader", argument))
	{
	    victim->pcdata->sub_class_current = CLASS_WARRIOR_CRUSADER;
	    victim->pcdata->second_sub_class_warrior = CLASS_WARRIOR_CRUSADER;
	}
    }

    group_add(victim, class_table[victim->pcdata->class_current].base_group, true);
    group_add(victim, sub_class_table[victim->pcdata->sub_class_current].default_group, true);
    victim->exp = 0;

    sprintf(buf2, sub_class_table[victim->pcdata->sub_class_current].name[victim->sex]);
    buf2[0] = UPPER(buf2[0]);
    sprintf(buf, "All congratulate %s, who is now a%s %s!",
        victim->name, (buf2[0] == 'A' || buf2[0] == 'I' || buf2[0] == 'E' || buf2[0] == 'U'
	    || buf2[0] == 'O') ? "n" : "", buf2);
    crier_announce(buf);
    double_xp(victim);
#endif
}


void do_arealinks(CHAR_DATA *ch, char *argument)
{
    /*FILE *fp;*/
    BUFFER *buffer;
    AREA_DATA *parea;
    EXIT_DATA *pexit;
    ROOM_INDEX_DATA *to_room;
    ROOM_INDEX_DATA *from_room;
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    long vnum = 0;
    long iHash;
    int door;
    bool found = false;

    /* To provide a convenient way to translate door numbers to words */
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    /* First, the 'all' option */
    if (!str_cmp(arg1,"all"))
    {
	/*
	 * If a filename was provided, try to open it for writing
	 * If that fails, just spit output to the screen.
	 */ /*
	if (arg2[0] != '\0')
	{
	    fclose(fpReserve);
	    if((fp = fopen(arg2, "w")) == NULL)
	    {
		send_to_char("Error opening file, printing to screen.\n\r",ch);
		fclose(fp);
		fpReserve = fopen(NULL_FILE, "r");
		fp = NULL;
	    }
	}
	else
	    fp = NULL; */

	/* Open a buffer if it's to be output to the screen */
	/*if (!fp)*/
	    buffer = new_buf();

	/* Loop through all the areas */
	for (parea = area_first; parea != NULL; parea = parea->next)
	{
	    /* First things, add area name  and vnums to the buffer */
	    sprintf(buf, "*** %s (%ld to %ld) ***\n\r",
			 parea->name, parea->min_vnum, parea->max_vnum);
	    /*fp ? fprintf(fp, buf) : */add_buf(buffer, buf);

	    /* Now let's start looping through all the rooms. */
	    found = false;
	    for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	    {
		for(from_room = room_index_hash[iHash];
		     from_room != NULL;
		     from_room = from_room->next)
		{
		    /*
		     * If the room isn't in the current area,
		     * then skip it, not interested.
		     */
		    if (from_room->vnum < parea->min_vnum
		    ||   from_room->vnum > parea->max_vnum)
			continue;

		    /* Aha, room is in the area, lets check all directions */
		    for (door = 0; door < 9; door++)
		    {
			/* Does an exit exist in this direction? */
			if((pexit = from_room->exit[door]) != NULL)
			{
			    to_room = pexit->u1.to_room;

			    /*
			     * If the exit links to a different area
			     * then add it to the buffer/file
			     */
			    if(to_room != NULL
			    &&  (to_room->vnum < parea->min_vnum
			    ||   to_room->vnum > parea->max_vnum))
			    {
				found = true;
				sprintf(buf, "    (%ld) links %s to %s (%ld)\n\r",
				    from_room->vnum, dir_name[door],
				    to_room->area->name, to_room->vnum);

				/* Add to either buffer or file */
				/*if(fp == NULL)*/
				    add_buf(buffer, buf);
				/*else*/
				/*    fprintf(fp, buf);*/
			    }
			}
		    }
		}
	    }

	    /* Informative message for areas with no external links */
	    if (!found)
		add_buf(buffer, "    No links to other areas found.\n\r");
	}

	/* Send the buffer to the player */
	/*if (!fp)
	{*/
	    page_to_char(buf_string(buffer), ch);
	    free_buf(buffer);
	/*}*/
	/* Or just clean up file stuff */
	/*else
	{
	    fclose(fp);
	    fpReserve = fopen(NULL_FILE, "r");
	}*/

	return;
    }

    /* No argument, let's grab the char's current area */
    if(arg1[0] == '\0')
    {
	parea = ch->in_room ? ch->in_room->area : NULL;

	/* In case something wierd is going on, bail */
	if (parea == NULL)
	{
	    send_to_char("You aren't in an area right now, funky.\n\r",ch);
	    return;
	}
    }
    /* Room vnum provided, so lets go find the area it belongs to */
    else if(is_number(arg1))
    {
	vnum = atol(arg1);

	/* Hah! No funny vnums! I saw you trying to break it... */
	if (vnum <= 0 || vnum > 2147483647)
	{
	    send_to_char("The vnum must be between 1 and 2147483647.\n\r",ch);
	    return;
	}

	/* Search the areas for the appropriate vnum range */
	for (parea = area_first; parea != NULL; parea = parea->next)
	{
	    if(vnum >= parea->min_vnum && vnum <= parea->max_vnum)
		break;
	}

	/* Whoops, vnum not contained in any area */
	if (parea == NULL)
	{
	    send_to_char("There is no area containing that vnum.\n\r",ch);
	    return;
	}
    }
    /* Non-number argument, must be trying for an area name */
    else
    {
	/* Loop the areas, compare the name to argument */
	for(parea = area_first; parea != NULL; parea = parea->next)
	{
	    if(!str_prefix(arg1, parea->name))
		break;
	}

	/* Sorry chum, you picked a goofy name */
	if (parea == NULL)
	{
	    send_to_char("There is no such area.\n\r",ch);
	    return;
	}
    }

    /* Just like in all, trying to fix up the file if provided */
   /* if (arg2[0] != '\0')
    {
	fclose(fpReserve);
	if((fp = fopen(arg2, "w")) == NULL)
	{
	    send_to_char("Error opening file, printing to screen.\n\r",ch);
	    fclose(fp);
	    fpReserve = fopen(NULL_FILE, "r");
	    fp = NULL;
	}
    }
    else
	fp = NULL;*/

    /* And we loop the rooms */
    for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
	for(from_room = room_index_hash[iHash];
	     from_room != NULL;
	     from_room = from_room->next)
	{
	    /* Gotta make sure the room belongs to the desired area */
	    if (from_room->vnum < parea->min_vnum
	    ||   from_room->vnum > parea->max_vnum)
		continue;

	    /* Room's good, let's check all the directions for exits */
	    for (door = 0; door < 9; door++)
	    {
		if((pexit = from_room->exit[door]) != NULL)
		{
		    to_room = pexit->u1.to_room;

		    /* Found an exit, does it lead to a different area? */
		    if(to_room != NULL
		    &&  (to_room->vnum < parea->min_vnum
		    ||   to_room->vnum > parea->max_vnum))
		    {
			found = true;
			sprintf(buf, "%s (%ld) links %s to %s (%ld)\n\r",
				    parea->name, from_room->vnum, dir_name[door],
				    to_room->area->name, to_room->vnum);

			/* File or buffer output? */
			/*if(fp == NULL)*/
			    send_to_char(buf, ch);
			/*else*/
			/*    fprintf(fp, buf);*/
		    }
		}
	    }
	}
    }

    /* Informative message telling you it's not externally linked */
    if(!found)
    {
	send_to_char("No links to other areas found.\n\r",ch);
	/* Let's just delete the file if no links found */
	/*if (fp)*/
	/*    unlink(arg2);*/
	return;
    }

    /* Close up and clean up file stuff */
    /*if(fp)
    {
	fclose(fp);
	fpReserve = fopen(NULL_FILE, "r");
    }*/

}


void do_sload(CHAR_DATA *ch, char *argument)
{
#if 0
/*
    char arg1[MAX_INPUT_LENGTH] ,arg2[MAX_INPUT_LENGTH];
    ROOM_INDEX_DATA *pRoom;
    NPC_SHIP_INDEX_DATA *pShip;
    NPC_SHIP_DATA *pNpcShip;
    long room_vnum;
    long ship_vnum;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (arg1[0] == '\0' || !is_number(arg1))
    {
	send_to_char("Syntax: load ship <vnum> <room vnum>.\n\r", ch);
	return;
    }

    ship_vnum = atol(arg1);

    if (arg2[0] != '\0')
    {
	if (!is_number(arg2))
        {
	  send_to_char("Syntax: sload <vnum> <room vnum>.\n\r", ch);
	  return;
	}
        room_vnum = atol(arg2);
        if ((pRoom = get_room_index(room_vnum)) == NULL)
	{
	  send_to_char("Could not find room vnum.\n\r",ch);
  	  return;
	}
    }
    else {
      send_to_char("Syntax: sload <vnum> <room vnum>.\n\r", ch);
      return;
    }

    if ((pShip = get_npc_ship_index(ship_vnum)) == NULL)
    {
	send_to_char("No ship has that vnum.\n\r", ch);
	return;
    }

    pNpcShip = create_npc_sailing_boat(ship_vnum);

     If the npc airship then set airship
    if (pShip->npc_type == NPC_SHIP_AIR_SHIP)
    {
	    plith_airship = pNpcShip;
    }

    obj_to_room(pNpcShip->ship->ship, pRoom);

    if (pNpcShip->ship->ship->in_room == NULL)
    {
	    gecho("NULL already");
    }

    send_to_char("Ship created.\n\r", ch);
    return;
*/
#endif
}


/* Strip items of a vnum or name from a ch.*/
void do_junk(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    bool fAll = false;
    bool found = false;
    ITERATOR it;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: junk <person> <obj vnum or name> [all]\n\r",
                ch);
        return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (argument[0] != '\0'
    && !str_cmp(argument, "all"))
        fAll = true;

    if (is_number(arg2)
    && get_obj_index(atol(arg2)) == NULL)
    {
        send_to_char("No such object even exists.\n\r", ch);
        return;
    }

    iterator_start(&it, victim->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if ((is_number(arg2)
             && obj->pIndexData->vnum == atol(arg2))
        || (is_name(arg2, obj->name)))
        {
            act("Extracted $p from $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            // Need to remove from the iterator before extracting
            iterator_remcurrent(&it);
            extract_obj(obj);
            found = true;
            if (!fAll) break;
        }
    }
    iterator_stop(&it);

    if (found)
        send_to_char("Done.\n\r", ch);
    else
        send_to_char("They are carrying no such object.\n\r", ch);

    return;
}


void do_alevel(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    long xp;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Level whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_world(ch, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    xp = exp_per_level(victim, victim->pcdata->points) - victim->exp;
    gain_exp(victim, xp, false);

    return;
}


void do_areset(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    char buf[MSL];

    if (argument[0] == '\0')
    {
	send_to_char("Reset which area?\n\r", ch);
	return;
    }

    area = NULL;
    for (area = area_first; area != NULL; area = area->next)
    {
	if (!str_infix(argument, area->name))
  	    break;
    }

    if (!area)
    {
	send_to_char("Couldn't find that area.\n\r", ch);
	return;
    }

    reset_area(area);
    area->age = 0;
    sprintf(buf, "Reset %s.\n\r", area->name);
    send_to_char(buf, ch);
}


void do_autosetname(CHAR_DATA *ch, char *argument)
{
    if (!IS_SET(ch->act[0], PLR_AUTOSETNAME))
    {
	send_to_char("AUTOSETNAME on. Your name keywords will now be automatically set when building.\n\r", ch);
	SET_BIT(ch->act[0], PLR_AUTOSETNAME);
    }
    else
    {
	send_to_char("AUTOSETNAME off. Your name keywords will no longer be automatically set.\n\r", ch);
	REMOVE_BIT(ch->act[0], PLR_AUTOSETNAME);
    }
}


void do_autowar(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    char arg[MSL];
    char arg2[MSL];
    char arg3[MSL];
    char arg4[MSL];
    char arg5[MSL];
    int i;
    int min_players;
    int min;
    int max;
    int timer;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);

    if (!str_cmp(arg, "stop"))
    {
	if (auto_war == NULL) {
	    send_to_char("No war is going on.\n\r", ch);
	    return;
	}

	sprintf(buf, "{R%s has ended the autowar.{x\n\r", ch->name);
	war_channel(buf);
	free_auto_war(auto_war);
	return;
    }

    if (auto_war_timer > 0)
    {
	send_to_char("Auto-war already in progress.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("autowar <type> <min_players> <min_level> <max_level> <time_before_start>\n\r", ch);
	send_to_char("What type of autowar?\n\r{Y", ch);
	i = 0;
	while(auto_war_table[i].name != NULL)
	{
	    send_to_char(auto_war_table[i].name, ch);
	    send_to_char("\n\r", ch);
	    i++;
	}
	send_to_char("{x\n\r", ch);
    }

    if (arg2[0] == '\0')
    {
	send_to_char("What is the minimum number of players?\n\r", ch);
	return;
    }

    if (arg3[0] == '\0')
    {
	send_to_char("What is the minimum and maximum level?\n\r", ch);
	return;
    }

    if (arg4[0] == '\0')
    {
	send_to_char("What is the maximum level?\n\r", ch);
	return;
    }

    if (arg5[0] == '\0' || !is_number(arg5))
    {
	timer = 2;
    }
    else
    {
	timer = atoi(arg5);
    }

    i = 0;
    while(auto_war_table[i].name != NULL)
    {
	if (!str_prefix(auto_war_table[i].name, arg))
	{
	    break;
	}
	i++;
    }

    if (auto_war_table[i].name == NULL)
    {
	send_to_char("That isn't an auto-war type.\n\r", ch);
	return;
    }

    if (!is_number(arg2) || !is_number(arg3) || !is_number(arg4))
    {
	send_to_char("Invalid level range given.\n\r", ch);
	return;
    }

    min_players = atoi(arg2);
    if (min_players < 2) {
	send_to_char("You need at least two players to fight a war.\n\r", ch);
	return;
    }

    min = atoi(arg3);
    max = atoi(arg4);

    if (min >= max)
    {
	send_to_char("Invalid level range.\n\r", ch);
	return;
    }

    if (auto_war != NULL)
    {
	free_auto_war(auto_war);
    }

    auto_war = new_auto_war(i, min_players, min, max);
    auto_war_timer = timer;

    sprintf(buf, "{RGet Ready! {RA {Y%s{R war is about to begin for levels {Y%d{R to {Y%d{R, in {Y%d{R minutes!{x\n\r",
	    auto_war_table[i].name, min, max, auto_war_timer);
    gecho(buf);
    gecho("Type 'war join' to enter!\n\r");
}


void do_vislist(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[MSL];
    char player_name[MSL];
    bool found_char;
    FILE *fp;
    STRING_DATA *string;
    STRING_DATA *string_prev;
    bool found;
    int i;

    if (IS_NPC(ch))
	return;

    argument = one_argument(argument, arg);
    arg[0] = UPPER(arg[0]);

    if (strlen(arg) > 12)
	arg[12] = '\0';

    /* show vislist */
    if (arg[0] == '\0' || !str_cmp(arg, "show"))
    {
	send_to_char("{YYou are currently visible to:{x\n\r", ch);
	line(ch, 45, NULL, NULL);
	i = 0;
	for (string = ch->pcdata->vis_to_people; string != NULL;
	      string = string->next)
	{
	    sprintf(buf, "{Y%2d):{x %s\n\r", i + 1, string->string);
	    send_to_char(buf, ch);
	    i++;
	}

	if (i == 0)
	    send_to_char("Nobody.\n\r", ch);

	line(ch, 45, NULL, NULL);

	return;
    }

    if (!str_cmp(arg, ch->name))
    {
	send_to_char("That would be pointless.\n\r", ch);
	return;
    }

    /* take everyone off */
    if (!str_cmp(arg, "clear"))
    {
	STRING_DATA *string_next;

	for (string = ch->pcdata->vis_to_people; string != NULL; string = string_next)
	{
	    string_next = string->next;
	    do_function(ch, &do_vislist, string->string);
	}

	send_to_char("Vislist cleared.\n\r", ch);
	return;
    }

    found = false;
    string_prev = NULL;
    for (string = ch->pcdata->vis_to_people; string != NULL;
          string = string->next)
    {
	if (!str_prefix(arg, string->string))
	{
	    found = true;
	    break;
	}

	string_prev = string;
    }

    if (found)
    {
	act("Removed $t from vis list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR, NULL, NULL);
	if (string_prev != NULL)
	    string_prev->next = string->next;
	else
	    ch->pcdata->vis_to_people = string->next;
	free_string_data(string);
	return;
    }
    else
    {
	CHAR_DATA *victim;

	i = 0;
	for (string = ch->pcdata->vis_to_people; string != NULL;
	      string = string->next)
	    i++;

	if (i > 14)
	{
	    send_to_char("Sorry, maximum is 15 people.\n\r", ch);
	    return;
	}

	if ((victim = get_char_world(ch, arg)) != NULL
	&& !IS_NPC(victim))
	{
	    found_char = true;
	    sprintf(arg, "%s", capitalize(victim->name));
	}
	else
	{
	    sprintf(player_name, "%s%c/%s", PLAYER_DIR, tolower(arg[0]), capitalize(arg));
	    if ((fp = fopen(player_name, "r")) == NULL)
	    {
		found_char = false;
	    }
	    else
	    {
		found_char = true;
		fclose (fp);
	    }
	}

	if (!found_char)
	{
	    send_to_char("That player doesn't exist.\n\r", ch);
	    return;
	}

	string = new_string_data();
	string->string = str_dup(arg);
	act("Added $t to your vis list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR, NULL, NULL);
	string->next = ch->pcdata->vis_to_people;
	ch->pcdata->vis_to_people = string;
    }
}


/* dummy command for whatever, used in debugging only */
void do_test(CHAR_DATA *ch, char *argument)
{
}


void do_assignhelper(CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    CHAR_DATA * victim;
    argument = one_argument(argument, arg);
    if (ch->tot_level < MAX_LEVEL - 1)
    {
	send_to_char("Huh?\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char ("Who do you want to make a helper?\n\r", ch);
	return;
    }

    victim = get_char_world(ch, arg);
    if (victim == NULL)
    {
	send_to_char("That player doesn't exist.\n\r", ch);
	return;
    }

    if (IS_NPC(victim))
    {
	send_to_char("That isn't a player!\n\r", ch);
	return;
    }

    if (IS_SET(victim->act[0], PLR_HELPER))
    {
	REMOVE_BIT(victim->act[0], PLR_HELPER);
	SET_BIT(ch->comm,COMM_NOHELPER);
	if (ch == victim)
	    send_to_char("You are no longer a helper.\n\r", ch);
	else
	{
	    act("$N is no longer a helper.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
	    send_to_char("You are no longer a helper.\n\r", victim);
	}
    }
    else
    {
	SET_BIT(victim->act[0], PLR_HELPER);
	REMOVE_BIT(ch->comm,COMM_NOHELPER);
	if (ch == victim)
	    send_to_char("You are now a helper.\n\r", ch);
	else
	{
	    act("$N is now a helper.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
	    send_to_char("You are now a helper.\n\r", victim);
	}
    }
}


void do_otransfer(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    /*char buf[MAX_STRING_LENGTH];*/
    ROOM_INDEX_DATA *location;
    OBJ_DATA *obj;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
	send_to_char("Transfer what (and where)?\n\r", ch);
	return;
    }

    obj = get_obj_world(ch, arg1);

    if (obj == NULL)
    {
	send_to_char("No object.\n\r", ch);
	return;
    }

    if (obj->carried_by != NULL
    || obj->in_room == NULL)
    {
	act("$p isn't on the ground.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    act("Transferred $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    if (arg2[0] == '\0')
    {
	location = ch->in_room;
    }
    else
    {
	if ((location = find_location(ch, arg2)) == NULL)
	{
	    send_to_char("No such location.\n\r", ch);
	    return;
	}
    }

    obj_from_room(obj);
    if(location->wilds)
    	obj_to_vroom(obj, location->wilds, location->x, location->y);
    else
	obj_to_room(obj, location);

    return;
}


/* Go unwizi for one command only*/
void do_uninvis(CHAR_DATA *ch, char *argument)
{
    int lev_wizi;
    int lev_incog;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax: uninvis <command>\n\r", ch);
	return;
    }

    lev_wizi  = ch->invis_level;
    lev_incog = ch->incog_level;

    ch->invis_level = 0;
    ch->incog_level = 0;
    interpret(ch, argument);

    ch->invis_level = lev_wizi;
    ch->incog_level = lev_incog;
}


/* Allows custom granting of commands to people to do away with all those
   clumsy hacks. (Syn 2006-06-17) */
void do_addcommand(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    CHAR_DATA *vch;
    COMMAND_DATA *cmd;
//    int i;
    bool found = false;
	CMD_DATA *command;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
        send_to_char("Syntax: addcommand [person] [command]\n\r", ch);
	return;
    }

    if ((vch = get_char_world(ch, arg)) == NULL) {
        send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(vch)) {
        send_to_char("You can't give NPCs commands.\n\r", ch);
	return;
    }

    if (vch == ch) {
        send_to_char("You can't give yourself commands.\n\r", ch);
	return;
    }
/*
    for (i = 0; cmd_table[i].name[0] != '\0'; i++)
    {
        if (!str_prefix(arg2, cmd_table[i].name)
	&&  cmd_table[i].level <= ch->tot_level)
	{
	    found = true;
	    break;
	}
    }
*/
	ITERATOR it;
	iterator_start(&it, commands_list);
	while(( command = (CMD_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(arg2, command->name)
		&&  command->rank <= get_staff_rank(ch))
		{
			found = true;
			break;
		}
	}

    if (!found) {
        send_to_char("Command not found.\n\r", ch);
	return;
    }

/*
    if (cmd_table[i].level <= vch->tot_level) {
*/
    if (command->rank <= get_staff_rank(vch)) {		
        act("$N can already use that command due to $S level.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
	return;
    }

    cmd = new_command();
/*
    cmd->name = str_dup(cmd_table[i].name);
*/
    cmd->name = str_dup(command->name);	
    cmd->next = vch->pcdata->commands;
    vch->pcdata->commands = cmd;

/*
    sprintf(buf, "Granted command \"%s\" to %s.\n\r", cmd_table[i].name, vch->name);
*/
    sprintf(buf, "Granted command \"%s\" to %s.\n\r", command->name, vch->name);
    send_to_char(buf, ch);
}


void do_remcommand(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char buf[MSL];
    CHAR_DATA *vch;
    COMMAND_DATA *cmd, *cmd_prev = NULL;
    bool found = false;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
        send_to_char("Syntax: remcommand [person] [command]\n\r", ch);
	return;
    }

    if ((vch = get_char_world(ch, arg)) == NULL) {
        send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (IS_NPC(vch)) {
        send_to_char("You can't remove commands from NPCs.\n\r", ch);
	return;
    }

    if (vch == ch) {
        send_to_char("That would be pointless.\n\r", ch);
	return;
    }

    for (cmd = vch->pcdata->commands; cmd != NULL; cmd = cmd->next)
    {
        if (!str_prefix(arg2, cmd->name))
	{
	    found = true;
	    break;
	}

	cmd_prev = cmd;
    }

    if (!found) {
        send_to_char("Command not found.\n\r", ch);
	return;
    }

    if (cmd_prev != NULL)
	cmd_prev->next = cmd->next;
    else
    {
        vch->pcdata->commands = NULL;
	cmd->next = NULL;
    }

    sprintf(buf, "Removed command \"%s\" from %s.\n\r", cmd->name, vch->name);
    send_to_char(buf, ch);

    free_command(cmd);
}

/* Adjusted boost to allow for up to 7 days (10080 minutes) - Tieryo */
void do_boost(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg[MSL];
	char arg2[MSL];
	char arg3[MSL];
	int type;
	int mins;
	int percent;
	struct tm *timer;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);

	if (arg[0] == '\0' || arg2[0] == '\0') {
		send_to_char("Syntax:  boost <field> <#mins (1-10080 or off)> [percent]\n\rFields: experience damage qp pneuma\n\r", ch);
		return;
	}

	for (type = 0; boost_table[type].name != NULL; type++) {
		if (!str_prefix(arg, boost_table[type].name))
			break;
	}
	/* Don't allow imms to set reckoning boost, this is to be done by the game's internal systems only -- Areo*/
	if (boost_table[type].name == NULL || strcmp(boost_table[type].name,  "reckoning") == 0) {
		send_to_char("Invalid boost field.\n\rFields: experience damage qp pneuma\n\r", ch);
		return;
	}

	if ((mins = atoi(arg2)) < 1 || mins > 10080) {
		if (!str_cmp(arg2, "off"))
			mins = 0;
		else {
			send_to_char("Invalid #mins.\n\rMust be 1-10080 minutes, or off.\n\r", ch);
			return;
		}
	}

	if (arg3[0] == '\0')
		percent = 150;
	else if ((percent = atoi(arg3)) < 100 || percent > 200) {
		send_to_char("Invalid boost percent.\n\rPercent must be 100-200%.\n\r", ch);
		return;
	}

	if (mins == 0)
		sprintf(buf, "Turned off %s boost.\n\r", boost_table[type].name);
	else
		sprintf(buf, "Boosted %s to %+d%% for %d minutes.\n\r", boost_table[type].name, percent-100, mins);

	send_to_char(buf, ch);

	if (boost_table[type].timer == 0 || mins == 0)
		timer = localtime(&current_time);
	else
		timer = localtime(&boost_table[type].timer);

	if (mins >= 1) {
		timer->tm_min += mins;
		boost_table[type].timer = mktime(timer);
		boost_table[type].boost = percent;
		sprintf(buf, "{B({WBOOST{B)--> {W%d {Dminutes of %s {Dboost ({W%+d%%{D)!!!{x\n\r",
		mins, boost_table[type].colour_name, (percent - 100));
		gecho(buf);
		return;
	}
	else
		timer->tm_min = 0;

	boost_table[type].timer = mktime(timer);
}


/* Allows imms to fuck with tokens directly. */
void do_token(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char arg3[MSL];
    char arg4[MSL], arg4b[MSL];
    char buf[MSL];
    long vnum, count;
    TOKEN_DATA *token;
    TOKEN_INDEX_DATA *token_index;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0') {
		send_to_char("Syntax:  token <give|junk> char <character> [#.]<vnum>\n\r", ch);
		send_to_char("         token <give|junk> obj <object> [#.]<vnum>\n\r", ch);
		send_to_char("         token <give|junk> room [#.]<vnum>\n\r", ch);
		return;
    }

	if( !str_cmp(arg2, "char") && arg4[0] != '\0')
	{
	    CHAR_DATA *victim;

		if ((victim = get_char_world(NULL, arg3)) == NULL) {
			send_to_char("Character not found.\n\r", ch);
			return;
		}

		count = number_argument(arg4, arg4b);
	    vnum = atol(arg4b);

		if (!str_cmp(arg, "give")) {
			if(ch->tot_level < (MAX_LEVEL - 1) && ch != victim && !IS_NPC(victim)) {
				send_to_char("You may not give tokens to other players.\n\r",ch);
				return;
			}

			if ((token_index = get_token_index(vnum)) == NULL) {
				send_to_char("That token doesn't exist.\n\r", ch);
				return;
			}

			if (is_singular_token(token_index)) {
				if ((token = get_token_char(victim, vnum, 1)) != NULL) {
					send_to_char("Only one copy of this token can be given.\n\r", ch);
					return;
				}
			}

			if (IS_SET(token_index->flags, TOKEN_PERMANENT)) {

				if( ch->pcdata->security < 10 ) {
					if( !is_test_port ) {
						send_to_char("You may not give permanent tokens.\n\r", ch);
						return;
					}
					else
						send_to_char("{WWARNING: Assigning a permanent token.  This is only allowed while in Test Port Mode.{x\n\r", ch);
				}
			}

			TOKEN_DATA *token = give_token(token_index, victim, NULL, NULL);
			sprintf(buf, "Gave token %s(%ld) to character %s\n\r",
				token_index->name, token_index->vnum, HANDLE(victim));
			send_to_char(buf, ch);
			if (IS_SET(token_index->flags, TOKEN_PERMANENT))
			{
				send_to_char("{YWARNING:{R Token is {WPERMANENT{R.  It may only be removed by a system script or pfile editting.{x\n\r", ch);
			}

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_GIVEN, NULL);

		} else if (!str_cmp(arg, "junk")) {
			if(ch->tot_level < (MAX_LEVEL - 1) && ch != victim && !IS_NPC(victim)) {
				send_to_char("You may not take tokens take other people.\n\r",ch);
				return;
			}

			if ((token = get_token_char(victim, vnum, count)) == NULL) {
				send_to_char("Token not found on victim.\n\r", ch);
				return;
			}

			if( token && IS_SET(token->flags, TOKEN_PERMANENT) ) {
				if( ch->pcdata->security < 10 ) {
					if( !is_test_port ) {
						send_to_char("Token is flagged permanent.  Only the server may remove it.\n\r", ch);
						return;
					}
					else
						send_to_char("{WWARNING: Removing a permanent token.  This is only allowed while in Test Port Mode.{x\n\r", ch);
				}
			}

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

			sprintf(buf, "Removed token %s(%ld.%ld) from character %s\n\r",
				token->name, count, token->pIndexData->vnum, HANDLE(victim));
			send_to_char(buf, ch);

			token_from_char(token);
			free_token(token);
		} else
			send_to_char("Syntax:  token <give|junk> char <character> [#.]<vnum>\n\r", ch);

	} else if(!str_cmp(arg2, "obj") && arg4[0] != '\0') {
		OBJ_DATA *obj;

		if( !(obj = get_obj_world(ch, arg3)) ) {
			send_to_char("Object not found.\n\r", ch);
			return;
		}

		count = number_argument(arg4, arg4b);
	    vnum = atol(arg4b);

		if (!str_cmp(arg, "give")) {
			if ((token_index = get_token_index(vnum)) == NULL) {
				send_to_char("That token doesn't exist.\n\r", ch);
				return;
			}

			if (is_singular_token(token_index)) {
				if ((token = get_token_obj(obj, vnum, 1)) != NULL) {
					send_to_char("Only one copy of this token can be given.\n\r", ch);
					return;
				}
			}

			TOKEN_DATA *token = give_token(token_index, NULL, obj, NULL);
			sprintf(buf, "Gave token %s(%ld) to object %s\n\r", token_index->name, token_index->vnum, obj->short_descr);
			send_to_char(buf, ch);

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_GIVEN, NULL);

		} else if (!str_cmp(arg, "junk")) {
			if ((token = get_token_obj(obj, vnum, count)) == NULL) {
				send_to_char("Token not found on object.\n\r", ch);
				return;
			}

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

			sprintf(buf, "Removed token %s(%ld.%ld) from object %s\n\r",
				token->name, count, token->pIndexData->vnum, obj->short_descr);
			send_to_char(buf, ch);

			token_from_obj(token);
			free_token(token);
		} else
			send_to_char("Syntax:  token <give|junk> obj <object> [#.]<vnum>\n\r", ch);
	} else if(!str_cmp(arg2, "room") ) {
		count = number_argument(arg3, arg4b);
	    vnum = atol(arg4b);

		if (!str_cmp(arg, "give")) {
			if ((token_index = get_token_index(vnum)) == NULL) {
				send_to_char("That token doesn't exist.\n\r", ch);
				return;
			}

			if (is_singular_token(token_index)) {
				if ((token = get_token_room(ch->in_room, vnum, 1)) != NULL) {
					send_to_char("Only one copy of this token can be given.\n\r", ch);
					return;
				}
			}

			TOKEN_DATA *token = give_token(token_index, NULL, NULL, ch->in_room);
			if( ch->in_room->wilds && IS_SET(ch->in_room->room_flag[1], ROOM_VIRTUAL_ROOM))
				sprintf(buf, "Gave token %s(%ld) to wilds room %ld @ (%ld, %ld)\n\r", token_index->name, token_index->vnum, ch->in_room->wilds->uid, ch->in_room->x, ch->in_room->y);
			else if( ch->in_room->source )
				sprintf(buf, "Gave token %s(%ld) to clone room %ld ID(%lu:%lu)\n\r", token_index->name, token_index->vnum, ch->in_room->source->vnum, ch->in_room->id[0], ch->in_room->id[1]);
			else
				sprintf(buf, "Gave token %s(%ld) to room %ld\n\r", token_index->name, token_index->vnum, ch->in_room->vnum);
			send_to_char(buf, ch);

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_GIVEN, NULL);

		} else if (!str_cmp(arg, "junk")) {
			if ((token = get_token_room(ch->in_room, vnum, count)) == NULL) {
				send_to_char("Token not found on object.\n\r", ch);
				return;
			}

			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

			if( ch->in_room->wilds && IS_SET(ch->in_room->room_flag[1], ROOM_VIRTUAL_ROOM))
				sprintf(buf, "Removed token %s(%ld.%ld) from wilds room %ld @ (%ld, %ld)\n\r", token->name, count, token->pIndexData->vnum, ch->in_room->wilds->uid, ch->in_room->x, ch->in_room->y);
			else if( ch->in_room->source )
				sprintf(buf, "Removed token %s(%ld.%ld) from clone room %ld ID(%lu:%lu)\n\r", token->name, count, token->pIndexData->vnum, ch->in_room->source->vnum, ch->in_room->id[0], ch->in_room->id[1]);
			else
				sprintf(buf, "Removed token %s(%ld.%ld) from room %ld\n\r", token->name, count, token->pIndexData->vnum, ch->in_room->vnum);
			send_to_char(buf, ch);

			token_from_room(token);
			free_token(token);
		} else
			send_to_char("Syntax:  token <give|junk> room [#.]<vnum>\n\r", ch);

	}
	else
	{
		send_to_char("Syntax:  token <give|junk> char <character> [#.]<vnum>\n\r", ch);
		send_to_char("         token <give|junk> obj <object> [#.]<vnum>\n\r", ch);
		send_to_char("         token <give|junk> room [#.]<vnum>\n\r", ch);
	}

}


/* Load an area from an .are file into memory. The same exact function used
   in boot_db is used here. This allows us to build areas on the testport
   and import them without shutting down the game. Extreme care should be taken
   with this command, since it can be quite a performance drain.
*/
void do_aload(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    FILE *fp;
    AREA_DATA *area;
    LLIST_AREA_DATA *link;

    argument = one_argument(argument, arg);

    /* Check to see if the area is loaded in already. If it is, free it
       from memory and reload it. Make sure to update all object and mob
       pIndexData pointers and room area pointers. */
    for (area = area_first; area != NULL; area = area->next) {
	if (!str_cmp(area->file_name, argument))
	    break;
    }

    /* The simpler case - the area is not a current area. */
    if (area == NULL) {
	if ((fp = fopen(arg, "r")) == NULL) {
	    send_to_char("Area file not found.\n\r", ch);
	    return;
	}

	link = (LLIST_AREA_DATA *)alloc_mem(sizeof(LLIST_AREA_DATA));
	if( list_appendlink(loaded_areas, link) && (area = read_area_new(fp))) {
		area->next = NULL;

		area_last->next = area;
		area_last = area;

		// Add to script usable list
		link->area = area;
		link->uid = area->uid;

		act("Loaded area $T.", ch, NULL, NULL, NULL, NULL, NULL, area->name, TO_CHAR, NULL, NULL);
	} else
		free_mem( link, sizeof(LLIST_AREA_DATA));
	fclose(fp);
    } else {
	/* Syn - will add in replacement of current area when I have time. */
	send_to_char("Area already exists.\n\r", ch);
    }
}

void do_immflag(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch)) {
       bug("NPC tried to change imm flag", 0);
       return;
    }

    if (argument[0] == '\0') {
	send_to_char("Syntax:  immflag [flag]\n\r", ch);
	return;
    }

    if (strlen_no_colours(argument) > 12)
    {
	send_to_char("That flag is too long. Must be no more than 12 characters, not counting colour codes.\n\r", ch);
	return;
    }

    free_string(ch->pcdata->immortal->imm_flag);
    ch->pcdata->immortal->imm_flag = str_dup(argument);
    act("Your immortal flag has been set to $T.", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_CHAR, NULL, NULL);
}

void do_reloadstats(CHAR_DATA *ch, char *argument)
{
	load_statistics();
	stats_load_time = current_time;
}

// send obj values to a buffer
void print_live_obj_values(OBJ_DATA *obj, BUFFER *buffer)
{
    char buf[MAX_STRING_LENGTH];

    add_buf(buffer, "\n\r");
	
    switch(obj->item_type)
    {
	default:	// No values
	    break;
	case ITEM_LIGHT:

            if (obj->value[2] == -1)
		sprintf(buf, "{B[  {Wv2{B]{%s Light:{x  Infinite[-1]\n\r", (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y");
            else
		sprintf(buf, "{B[  {Wv2{B]{%s Light:{x  [%ld]\n\r", (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2]);

	    add_buf(buffer, buf);
	    break;

	case ITEM_WAND:
	case ITEM_STAFF:
            sprintf(buf,
		"{B[  {Wv0{B]{%s Level:{x          [%ld]\n\r"
		"{B[  {Wv1{B]{%s Charges Total:{x  [%ld]\n\r"
		"{B[  {Wv2{B]{%s Charges Left:{x   [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_PORTAL:
		if( IS_SET(obj->value[2], GATE_DUNGEON) )
		{
			// DUNGEON portal
			sprintf(buf,
				"{B[  {Wv0{B]{%s Charges:{x           [%ld]\n\r"
				"{B[  {Wv1{B]{%s Exit Flags:{x        %s\n\r"
				"{B[  {Wv2{B]{%s Portal Flags:{x      %s\n\r"
				"{B[  {Wv3{B]{%s Goes to (dungeon):{x [%ld]\n\r"
				"{B[  {Wv4{B]{%s Key:{x               [%ld] %s\n\r"
				"{B[  {Wv5{B]{%s Goes to (floor):  {x [%ld]\n\r",
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
				(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(portal_exit_flags, obj->value[1]),
				(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(portal_flags, obj->value[2]),
				(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
				(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				(obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", obj->value[5]);
		}
		else if( IS_SET(obj->value[2], GATE_AREARANDOM) || obj->value[3] == -1 )
		{
			// AREARANDOM portal
			sprintf(buf,
				"{B[  {Wv0{B]{%s Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{%s Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{%s Portal Flags:{x   %s\n\r"
				"{B[  {Wv4{B]{%s Key:{x            [%ld] %s\n\r"
				"{B[  {Wv5{B]{%s Goes to (area id):{x [%ld]\n\r",
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
				(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(portal_exit_flags, obj->value[1]),
				(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(portal_flags, obj->value[2]),
				(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[5]);
		}
		else if(obj->value[3] > 0)
		{
			// STATIC portal
			sprintf(buf,
				"{B[  {Wv0{B]{%s Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{%s Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{%s Portal Flags:{x   %s\n\r"
				"{B[  {Wv3{B]{%s Goes to (vnum):{x [%ld]\n\r"
				"{B[  {Wv4{B]{%s Key:{x            [%ld] %s\n\r",
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
				(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(portal_exit_flags, obj->value[1]),
				(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(portal_flags, obj->value[2]),
				(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
				(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none");
		}
		else
		{
			// WILDERNESS portal
			sprintf(buf,
				"{B[  {Wv0{B]{%s Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{%s Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{%s Portal Flags:{x   %s\n\r"
				"{B[  {Wv4{B]{%s Key:{x            [%ld] %s\n\r"
				"{B[  {Wv5{B]{%s Goes to (map):{x  [%ld]\n\r"
				"{B[  {Wv6{B]{%s Goes to (mapx):{x [%ld]\n\r"
				"{B[  {Wv7{B]{%s Goes to (mapy):{x [%ld]\n\r",
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
				(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(portal_exit_flags, obj->value[1]),
				(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(portal_flags, obj->value[2]),
				(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				(obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", obj->value[5], 
				(obj->value[6] == obj->pIndexData->value[6]) ? "B" : "Y", obj->value[6],
				(obj->value[6] == obj->pIndexData->value[7]) ? "B" : "Y", obj->value[7]);
		}
	    add_buf(buffer, buf);
	    break;

	case ITEM_FURNITURE:
	    sprintf(buf,
	        "{B[  {Wv0{B]{%s Max people:{x      [%ld]\n\r"
	        "{B[  {Wv1{B]{%s Max weight:{x      [%ld]\n\r"
	        "{B[  {Wv2{B]{%s Furniture Flags:{x %s\n\r"
	        "{B[  {Wv3{B]{%s Heal bonus:{x      [%ld]\n\r"
	        "{B[  {Wv4{B]{%s Mana bonus:{x      [%ld]\n\r"
			"{B[  {Wv5{B]{%s Move bonus:{x      [%ld]\n\r",
	        (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
	        (obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
	        (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(furniture_flags, obj->value[2]),
	        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
	        (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4],
			(obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_HERB:
	    sprintf(buf,
	    "{B[  {Wv0{B]{%s Type:{x            [%s]\n\r"
		"{B[  {Wv1{B]{%s Healing:{x         [%ld%%]\n\r"
		"{B[  {Wv2{B]{%s Regenerative:{x    [%ld%%]\n\r"
		"{B[  {Wv3{B]{%s Refreshing:{x      [%ld%%]\n\r"
		"{B[  {Wv4{B]{%s Immunity:{x        [%s]\n\r"
		"{B[  {Wv5{B]{%s Resistance:{x      [%s]\n\r"
		"{B[  {Wv6{B]{%s Vulnerability:{x   [%s]\n\r"
		"{B[  {Wv7{B]{%s Spell:{x           [%s]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", herb_table[obj->value[0]].name,
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
		(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
		(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", flag_string(imm_flags, obj->value[4]),
		(obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", flag_string(res_flags, obj->value[5]),
		(obj->value[6] == obj->pIndexData->value[6]) ? "B" : "Y", flag_string(vuln_flags, obj->value[6]),
		(obj->value[7] == obj->pIndexData->value[7]) ? "B" : "Y", skill_table[obj->value[7]].name);

	    add_buf(buffer, buf);
	    break;

	case ITEM_SCROLL:
	case ITEM_PILL:
	    break;

	case ITEM_POTION:
        sprintf(buf,
            	"{B[  {Wv5{B]{%s Charges:{x                [%ld]\n\r",
            	(obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TATTOO:
            sprintf(buf,
            		"{B[  {Wv0{B]{%s Touches:{x                [%ld]\n\r"
            		"{B[  {Wv1{B]{%s Chance of Fading:{x       [%ld]\n\r",
            		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
					(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_INK:
            sprintf(buf, "{B[  {Wv0{B]{%s Type 1:{x                 [%s]\n\r", 
			(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(catalyst_types, obj->value[0]));
	    add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv1{B]{%s Type 2:{x                 [%s]\n\r", 
			(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(catalyst_types, obj->value[1]));
	    add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv2{B]{%s Type 3:{x                 [%s]\n\r", 
			(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", flag_string(catalyst_types, obj->value[2]));
	    add_buf(buffer, buf);
	    break;

	case ITEM_SEXTANT:
            sprintf(buf,
		"{B[  {Wv0{B]{%s Percentage of working:{x  [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_SEED:
            sprintf(buf,
		"{B[  {Wv0{B]{%s Time before growth:{x     [%ld]\n\r"
		"{B[  {Wv1{B]{%s Turns into object vnum:{x [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_ARMOUR:
	    sprintf(buf,
		"{B[  {Wv0{B] {%sAc pierce       {x[%ld]\n\r"
		"{B[  {Wv1{B] {%sAc bash         {x[%ld]\n\r"
		"{B[  {Wv2{B] {%sAc slash        {x[%ld]\n\r"
		"{B[  {Wv3{B] {%sAc exotic       {x[%ld]\n\r"
		"{B[  {Wv4{B] {%sArmour strength  {x%s\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
		(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
		(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", armour_strength_table[obj->value[4]].name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_ARTIFACT:
	    break;

	case ITEM_RANGED_WEAPON:
            sprintf(buf, "{B[  {Wv0{B]{%s Ranged Weapon class:{x   %s\n\r",
		     (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(ranged_weapon_class, obj->value[0]));
	    add_buf(buffer, buf);

	    sprintf(buf, "{B[  {Wv1{B]{%s Number of dice:{x [%ld]\n\r", 
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv2{B]{%s Type of dice:{x   [%ld]\n\r", 
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2]);
	    add_buf(buffer, buf);

	    sprintf(buf, "{B[  {Wv3{B]{%s Projectile Distance:{x [%ld]\n\r", 
		(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_WEAPON:
            sprintf(buf, "{B[  {Wv0{B]{%s Weapon class:{x   %s\n\r",
		     (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(weapon_class, obj->value[0]));
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{%s Number of dice:{x [%ld]\n\r", 
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv2{B]{%s Type of dice:{x   [%ld]\n\r", 
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv3{B]{%s Type:{x           %s\n\r",
		    (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", attack_table[obj->value[3]].name);
	    add_buf(buffer, buf);
 	    sprintf(buf, "{B[  {Wv4{B]{%s Special type:{x   %s\n\r",
		     (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", flag_string(weapon_type2,  obj->value[4]));
	    add_buf(buffer, buf);
	    break;

	case ITEM_SHIP:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{%s Move delay:{x [%ld]\n\r"
		"{B[  {Wv2{B]{%s Min Crew:{x   [%ld]\n\r"
		"{B[  {Wv3{B]{%s Capacity:{x   [%ld]\n\r"
		"{B[  {Wv4{B]{%s Max Crew:{x   [%ld]\n\r"
		"{B[  {Wv5{B]{%s First Room:{x [%ld]\n\r"
		"{B[  {Wv6{B]{%s Hit Points:{x [%ld]\n\r"
		"{B[  {Wv7{B]{%s Max Guns:{x   [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
        (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4],
        (obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", obj->value[5],
        (obj->value[6] == obj->pIndexData->value[6]) ? "B" : "Y", obj->value[6],
        (obj->value[7] == obj->pIndexData->value[7]) ? "B" : "Y", obj->value[7]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CART:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{%s Move delay:{x [%ld]\n\r"
		"{B[  {Wv2{B]{%s Strength:{x   [%ld]\n\r"
		"{B[  {Wv3{B]{%s Capacity:{x    [%ld]\n\r"
		"{B[  {Wv4{B]{%s Weight Mult:{x [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
        (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TRADE_TYPE:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Trade Type:{x     [%s]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", trade_table[ obj->value[0] ].name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CONTAINER:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{%s Flags:{x      [%s]\n\r"
		"{B[  {Wv2{B]{%s Key:{x     %s [%ld]\n\r"
		"{B[  {Wv3{B]{%s Capacity:{x    [%ld]\n\r"
		"{B[  {Wv4{B]{%s Weight Mult:{x [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(container_flags, obj->value[1]),
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", 
                get_obj_index(obj->value[2])
                    ? get_obj_index(obj->value[2])->short_descr
                    : "none",
        obj->value[2],
        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
        (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_WEAPON_CONTAINER:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{%s Weapon Type:{x [%s]\n\r"
		"{B[  {Wv3{B]{%s Capacity:{x   [%ld]\n\r"
		"{B[  {Wv4{B]{%s Weight Mult:{x[%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(weapon_class, obj->value[1]),
        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
        (obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_DRINK_CON:
	    sprintf(buf,
	        "{B[  {Wv0{B]{%s Liquid Total:{x [%ld]\n\r"
	        "{B[  {Wv1{B]{%s Liquid Left:{x  [%ld]\n\r"
	        "{B[  {Wv2{B]{%s Liquid:{x       %s\n\r"
	        "{B[  {Wv3{B]{%s Poisoned:{x     %s\n\r",
	        (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
	        (obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
	        (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", liq_table[obj->value[2]].liq_name,
	        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3] != 0 ? "Yes" : "No");
	    add_buf(buffer, buf);
	    break;

	case ITEM_FOUNTAIN:
	    sprintf(buf,
	        "{B[  {Wv0{B]{%s Liquid Total:{x [%ld]\n\r"
	        "{B[  {Wv1{B]{%s Liquid Left:{x  [%ld]\n\r"
	        "{B[  {Wv2{B]{%s Liquid:{x     %s\n\r",
	        (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
	        (obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
	        (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", liq_table[obj->value[2]].liq_name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_FOOD:
	    sprintf(buf,
		"{B[  {Wv0{B]{%s Food hours:{x [%ld]\n\r"
		"{B[  {Wv1{B]{%s Full hours:{x [%ld]\n\r"
		"{B[  {Wv3{B]{%s Poisoned  :{x  %s\n\r"
		"{B[  {Wv4{B]{%s Timer     :{x [%ld]\n\r",
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
		(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3] != 0 ? "Yes" : "No",
		(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_MONEY:
            sprintf(buf, "{B[  {Wv0{B]{%s Silver:{x [%ld]\n\r", 
			(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{%s Gold:{x   [%ld]\n\r", 
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

        case ITEM_MIST:
	    sprintf(buf, "{B[  {Wv0{B]{%s %%HideObjects:{x    [%ld]\n\r", 
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{%s %%HideCharacters:{x [%ld]\n\r", 
		(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CORPSE_NPC:
	    sprintf(buf,
	        "{B[  {Wv0{B]{%s Type:{x           %s\n\r"
	        "{B[  {Wv1{B]{%s Resurrection:{x   %d%%\n\r"
	        "{B[  {Wv2{B]{%s Animation:{x      %d%%\n\r"
	        "{B[  {Wv3{B]{%s Body Parts:{x     %s\n\r"
	        "{B[  {Wv5{B]{%s Mobile (vnum):{x  %d\n\r",
	        (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(corpse_types,obj->value[0]),
	        (obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", (int)obj->value[1],
			(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", (int)obj->value[2],
	        (obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", flag_string(part_flags, obj->value[3]),
	        (obj->value[5] == obj->pIndexData->value[5]) ? "B" : "Y", (int)obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_INSTRUMENT:
	    sprintf(buf,
	        "{B[  {Wv0{B]{%s Type:{x            %s\n\r"
	        "{B[  {Wv1{B]{%s Flags:{x           %s\n\r"
	        "{B[  {Wv2{B]{%s Min Time Factor:{x %ld%%\n\r"
	        "{B[  {Wv3{B]{%s Max Time Factor:{x %ld%%\n\r",
	        (obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(instrument_types, obj->value[0]),
	        (obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", flag_string(instrument_flags, obj->value[1]),
	        (obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
			(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_BOOK:
	    sprintf(buf,
		"{B[  {Wv1{B]{%s Flags:{x      [%s]\n\r"
		"{B[  {Wv2{B]{%s Key:{x     %s [%ld]\n\r",
		(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", 
		flag_string(container_flags, obj->value[1]),
		(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", 
                get_obj_index(obj->value[2])
                    ? get_obj_index(obj->value[2])->short_descr
                    : "none",
                obj->value[2]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TELESCOPE:
		if( obj->value[4] < 0 )
			sprintf(buf,
				"{B[  {Wv0{B]{%s Current Distance:{x  [%ld]\n\r"
				"{B[  {Wv1{B]{%s Minimum Distance:{x  [%ld]\n\r"
				"{B[  {Wv2{B]{%s Maximum Distance:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{%s Bonusview Size:{x    [%ld]\n\r"
				"{B[  {Wv4{B]{B Current Heading:{x   [none]\n\r",
					(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
					(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
					(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
					(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3]);
		else
			sprintf(buf,
				"{B[  {Wv0{B]{%s Current Distance:{x  [%ld]\n\r"
				"{B[  {Wv1{B]{%s Minimum Distance:{x  [%ld]\n\r"
				"{B[  {Wv2{B]{%s Maximum Distance:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{%s Bonusview Size:{x    [%ld]\n\r"
				"{B[  {Wv4{B]{%s Current Heading:{x   [%ld]\n\r",
					(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
					(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1],
					(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
					(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3],
					(obj->value[4] == obj->pIndexData->value[4]) ? "B" : "Y", obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_COMPASS:
		if( obj->value[1] > 0 )
		{
			WILDS_DATA *pWilds = get_wilds_from_uid(NULL,obj->value[1]);

			sprintf(buf,
				"{B[  {Wv0{B]{%s Accuracy:{x      [%ld]\n\r"
				"{B[  {Wv1{B]{%s Wilderness:{x    [%ld] %s\n\r"
				"{B[  {Wv2{B]{%s X Coordinate:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{%s Y Coordinate:{x  [%ld]\n\r",
					(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0],
					(obj->value[1] == obj->pIndexData->value[1]) ? "B" : "Y", obj->value[1], (pWilds?pWilds->name:"???"),
					(obj->value[2] == obj->pIndexData->value[2]) ? "B" : "Y", obj->value[2],
					(obj->value[3] == obj->pIndexData->value[3]) ? "B" : "Y", obj->value[3]);
		}
		else
		{
			sprintf(buf,
				"{B[  {Wv0{B]{%s Accuracy:{x      [%ld]\n\r"
				"{B[  {Wv1{B]{B Wilderness:{x    [none]\n\r",
					(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", obj->value[0]);
		}
	    add_buf(buffer, buf);
		break;

	case ITEM_BODY_PART:
		sprintf(buf,
				"{B[  {Wv0{B]{%s Body Parts:{x    %s\n\r"
				"{B[  {Wv1{B]{%s Race:{x          %s\n\r",
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", flag_string(part_flags, obj->value[0]),
				(obj->value[0] == obj->pIndexData->value[0]) ? "B" : "Y", race_table[obj->value[1]].name);

		add_buf(buffer, buf);
		break;
    }
}

void do_pwreset(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    char type[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char target[MAX_INPUT_LENGTH];
    char email[MAX_INPUT_LENGTH];
    char reset_msg[MSL], reset_subject[MSL];
    char tmp_reset_code[16]; // Used for reset codes and can be for new passwords
    DESCRIPTOR_DATA d;       // Used for loading offline accounts/chars
    bool is_account = false;
    ACCOUNT_DATA *account = NULL;

    argument = one_argument(argument, type);
    argument = one_argument(argument, target);
    
    if (type[0] == '\0' || target[0] == '\0')
    {
        send_to_char("Syntax: pwreset <local|email> <character|account> [email]\n\r", ch);
        send_to_char("For account resets, prefix the account name with 'account:'\n\r", ch);
        return;
    }

    // Check if this is an account reset
    if (!strncmp(target, "account:", 8))
    {
        is_account = true;
        memmove(target, target + 8, strlen(target) - 7); // Remove "account:" prefix
    }

    if (!str_cmp(type, "local"))
    {
        if (is_account)
        {
            // Handle account reset (UNCHANGED FROM ORIGINAL)
            if (account_exists(target))
            {
                memset(&d, 0, sizeof(d));
                if (!load_account(&d, target))
                {
                    send_to_char("Error loading that account.\n\r", ch);
                    return;
                }
                account = d.account;

                if (account->reset_code[0] != '\0')
                {
                    free_string(account->reset_code);
                    account->reset_code = str_dup("");
                }

                generate_reset_code(tmp_reset_code, 15);
                account->reset_code = str_dup(tmp_reset_code);
                account->reset_state = RESET_PENDING;
                account->reset_time = current_time;

                sprintf(buf, "Password reset code has been set to %s for account %s.\n\r", 
                        account->reset_code, account->username);
                send_to_char(buf, ch);

                save_account(account);
                free_account(account);
                d.account = NULL;
            }
            else
            {
                send_to_char("That account does not exist.\n\r", ch);
                return;
            }
        }
        else // Character local reset
        {
            if ((victim = get_char_world(ch, target)) == NULL) // Character is OFFLINE
            {
                memset(&d, 0, sizeof(d));
                if (!load_char_obj(&d, target))
                {
                    send_to_char("That player does not exist.\n\r", ch);
                    return;
                }
                // d.character is now the loaded offline character
                d.character->desc = NULL; // Standard practice after loading for manipulation

                // Handle unlinked offline character (This logic remains as per prompt)
                if (d.character->pcdata->account_name == NULL || d.character->pcdata->account_name[0] == '\0')
                {
                    char new_plain_password[16]; // For plain text password
                    generate_reset_code(new_plain_password, 10); // Generate a 10-character random password

                    // Assuming set_encrypted_password handles both setting new and replacing old.
                    // It might internally free old d.character->pcdata->pwd if necessary.
                    if (!set_encrypted_password(&d.character->pcdata->pwd,        
                                                &d.character->pcdata->pwd_vers, 
                                                new_plain_password))               
                    {
                        send_to_char("Password reset failed due to an internal error. Please check server logs.\n\r", ch);
                        free_char(d.character); 
                        return;
                    }

                    d.character->pcdata->need_change_pw = 1; // Mark that user must change password on next login

                    sprintf(buf, "Unlinked character %s. New password set to: %s\n\r"
                                 "Admin must provide this password to the user.\n\r",
                            d.character->name, new_plain_password);
                    send_to_char(buf, ch);

                    save_char_obj(d.character);
                    free_char(d.character);
                    return; // Finished handling unlinked offline character
                }
                else // Linked offline character (Original Logic)
                {
                    if (d.character->pcdata->pwd[0] == '\0')
                    {
                        send_to_char("That character has no password set. Cannot create reset code.\n\r", ch);
                        free_char(d.character);
                        return;
                    }
                    
                    if (d.character->pcdata->reset_code[0] != '\0')
                    {
                        free_string(d.character->pcdata->reset_code);
                        d.character->pcdata->reset_code = str_dup("");
                    }

                    generate_reset_code(tmp_reset_code, 15);
                    d.character->pcdata->reset_code = str_dup(tmp_reset_code);
                    d.character->pcdata->reset_state = RESET_PENDING;
                    d.character->pcdata->reset_time = current_time;

                    sprintf(buf, "Password reset code has been set to %s for %s.\n\r", 
                            d.character->pcdata->reset_code, d.character->name);
                    send_to_char(buf, ch);

                    save_char_obj(d.character);
                    free_char(d.character);
                    return; // Finished handling linked offline character
                }
            }
            else // Character is ONLINE (victim != NULL)
            {
                // Per instructions:
                // 1. No password resets for online characters.
                // 2. No online unlinked characters (so this 'victim' is linked).
                send_to_char("That player is currently online. Password resets are not performed on active online characters.\n\r", ch);
                send_to_char("To reset the password for their account, use 'pwreset <local|email> account:<accountname>'.\n\r", ch);
                send_to_char("Character-specific password actions require the character to be offline.\n\r", ch);
                return;
            }
        }
    }
    else if (!str_cmp(type, "email"))
    {
        // Email reset logic (UNCHANGED FROM ORIGINAL for accounts and offline characters)
        // Note: If an admin attempts "pwreset email <unlinked_char_name>", this section will
        // execute. It will likely fail to find an email for the character or an associated
        // account (if unlinked), and guide the admin to use the "local" option for unlinked, which is correct.
        one_argument(argument, email);

        if (is_account)
        {
            // Handle account email reset (UNCHANGED)
            if (account_exists(target))
            {
                memset(&d, 0, sizeof(d));
                if (!load_account(&d, target))
                {
                    send_to_char("Error loading that account.\n\r", ch);
                    return;
                }
                account = d.account;
                
                if (account->reset_code[0] != '\0')
                {
                    free_string(account->reset_code);
                    account->reset_code = str_dup("");
                }

                generate_reset_code(tmp_reset_code, 15);
                account->reset_code = str_dup(tmp_reset_code);
                account->reset_state = RESET_PENDING;
                account->reset_time = current_time;

                sprintf(reset_subject, "Password Reset for Account: %s", account->username);
                sprintf(reset_msg, "Your account password reset code is: %s.\nPlease note that this code will expire after 24 hours.", 
                        account->reset_code);

                if (email[0] != '\0')
                {
                    send_email_async_ex(NULL, account, email, reset_subject, reset_msg, NULL, NULL);
                    sprintf(buf, "Password reset code has been sent to %s for account %s.\n\r", 
                            email, account->username);
                    send_to_char(buf, ch);
                }
                else
                {
                    if (account->email[0] == '\0')
                    {
                        send_to_char("No email address set for this account. You must use the 'local' option instead.\n\r", ch);
                        free_account(account);
                        d.account = NULL;
                        return;
                    }

                    send_email_async_ex(NULL, account, account->email, reset_subject, reset_msg, NULL, NULL);
                    sprintf(buf, "Password reset code has been sent to %s for account %s.\n\r", 
                            account->email, account->username);
                    send_to_char(buf, ch);
                }

                save_account(account);
                free_account(account);
                d.account = NULL;
            }
            else
            {
                send_to_char("That account does not exist.\n\r", ch);
                return;
            }
        }
        else // Character email reset
        {
            // Original character email reset code (UNCHANGED for offline characters)
            if ((player_exists(target))) // player_exists might be a pfile check
            {
                if ((victim = get_char_world(ch, target)) == NULL) // Offline character
                {
                    memset(&d, 0, sizeof(d));
                    if (!load_char_obj(&d, target))
                    {
                        send_to_char("That player does not exist (or error loading).\n\r", ch);
                        return;
                    }
                    // character is loaded
                    CHAR_DATA *loaded_char = d.character; // Use a distinct variable for clarity
                    loaded_char->desc = NULL;
                    
                    if (loaded_char->pcdata->pwd[0] == '\0')
                    {
                        send_to_char("That character has no password set. Cannot create reset code.\n\r", ch);
                        free_char(loaded_char);
                        return;
                    }
                    
                    if (loaded_char->pcdata->reset_code[0] != '\0')
                    {
                        free_string(loaded_char->pcdata->reset_code);
                        loaded_char->pcdata->reset_code = str_dup("");
                    }

                    generate_reset_code(tmp_reset_code, 15);
                    loaded_char->pcdata->reset_code = str_dup(tmp_reset_code);
                    loaded_char->pcdata->reset_state = RESET_PENDING;
                    loaded_char->pcdata->reset_time = current_time;

                    sprintf(reset_subject, "Password Reset for %s", loaded_char->name);
                    sprintf(reset_msg, "Your password reset code is: %s.\nPlease note that this code will expire after 24 hours.", 
                            loaded_char->pcdata->reset_code);

                    if (email[0] != '\0')
                    {
                        send_email_async(loaded_char, email, reset_subject, reset_msg, NULL, NULL);
                        sprintf(buf, "Password reset code has been sent to %s for %s.\n\r", 
                                email, target);
                        send_to_char(buf, ch);
                    }
                    else
                    {
                        if (loaded_char->pcdata->email[0] == '\0')
                        {
                            // Attempt to use account email if character email is not set
                            ACCOUNT_DATA *char_account_email;
                            DESCRIPTOR_DATA account_d_email; // Separate descriptor for this load
                            
                            save_char_obj(loaded_char); // Save changes like reset code first
                            
                            memset(&account_d_email, 0, sizeof(account_d_email));
                            // For unlinked char, account_name is empty/NULL, load_account will fail
                            if (loaded_char->pcdata->account_name == NULL || loaded_char->pcdata->account_name[0] == '\0' ||
                                !load_account(&account_d_email, loaded_char->pcdata->account_name))
                            {
                                send_to_char("No email address set for this player and unable to load an associated account. You must use the 'local' option instead.\n\r", ch);
                                free_char(loaded_char); // Free the char loaded earlier
                                // account_d_email.account would be NULL if load_account failed or wasn't called.
                                return;
                            }
                            
                            char_account_email = account_d_email.account;
                            
                            if (char_account_email->email[0] == '\0')
                            {
                                send_to_char("No email address set for this player or their account. You must use the 'local' option instead.\n\r", ch);
                                free_char(loaded_char);
                                free_account(char_account_email);
                                account_d_email.account = NULL;
                                return;
                            }

                            send_email_async(loaded_char, char_account_email->email, reset_subject, reset_msg, NULL, NULL);
                            sprintf(buf, "Password reset code has been sent to account email %s for character %s.\n\r", 
                                    char_account_email->email, target);
                            send_to_char(buf, ch);
                            
                            free_account(char_account_email);
                            account_d_email.account = NULL;
                        }
                        else
                        {
                            send_email_async(loaded_char, loaded_char->pcdata->email, reset_subject, reset_msg, NULL, NULL);
                            sprintf(buf, "Password reset code has been sent to %s for %s.\n\r", 
                                    loaded_char->pcdata->email, target);
                            send_to_char(buf, ch);
                        }
                    }

                    save_char_obj(loaded_char); // Save again if email was sent successfully
                    free_char(loaded_char);
                    return; 
                }
                else // Online character (victim is valid)
                {
                    // Per instructions:
                    // 1. No password resets for online characters.
                    // 2. No online unlinked characters (so this 'victim' is linked).
                    send_to_char("That player is currently online. Email password resets are for offline characters or accounts.\n\r", ch);
                    send_to_char("To send a password reset email for their account, use:\n\r", ch);
                    send_to_char("  pwreset email account:<accountname> [optional_email_override]\n\r", ch);
                    send_to_char("To send a password reset email for an offline character, use:\n\r", ch);
                    send_to_char("  pwreset email <charactername> [optional_email_override]\n\r", ch);
                    return;
                }
            }
            else
            {
                send_to_char("That player does not exist.\n\r", ch);
                return;
            }
        }
    }
    else
    {
        send_to_char("Syntax: pwreset <local|email> <character|account> [email]\n\r", ch);
        send_to_char("For account resets, prefix the account name with 'account:'\n\r", ch);
    }
}

void do_mfareset(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    char target[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA d;
    bool is_account = false;
    ACCOUNT_DATA *account = NULL;

    argument = one_argument(argument, target);

    if (target[0] == '\0')
    {
        send_to_char("Syntax: mfareset <character|account>\n\r", ch);
        send_to_char("For account resets, prefix the account name with 'account:'\n\r", ch);
        return;
    }

    // Check if this is an account reset
    if (!strncmp(target, "account:", 8))
    {
        is_account = true;
        memmove(target, target + 8, strlen(target) - 7); // Remove "account:" prefix
    }

    if (is_account)
    {
        // Handle account MFA reset
        if (account_exists(target))
        {
            // Create a temporary descriptor for loading the account
            memset(&d, 0, sizeof(d));
            
            // Load account using the proper function signature
            if (!load_account(&d, target))
            {
                send_to_char("Error loading that account.\n\r", ch);
                return;
            }
            
            account = d.account; // Get the loaded account

            // Check if MFA is enabled or has any MFA data set
            if (!account->mfa_enabled && IS_NULLSTR(account->mfa_key))
            {
                sprintf(buf, "Multifactor auth is not enabled for account %s.\n\r", account->username);
                send_to_char(buf, ch);
                free_account(account);
                d.account = NULL;
                return;
            }

            // Reset MFA settings
            account->mfa_enabled = false;
            
            if (!IS_NULLSTR(account->mfa_key))
            {
                free_string(account->mfa_key);
                account->mfa_key = str_dup("");
            }

            sprintf(buf, "Multifactor auth has been disabled for account %s.\n\r", account->username);
            send_to_char(buf, ch);
            save_account(account);

            free_account(account);
            d.account = NULL;
        }
        else
        {
            send_to_char("That account does not exist.\n\r", ch);
            return;
        }
    }
    else
    {
        // Character MFA reset code
        if ((player_exists(target)))
        {
            if ((victim = get_char_world(ch, target)) == NULL)
            {
                if (!load_char_obj(&d, target))
                {
                    send_to_char("That player does not exist.\n\r", ch);
                    return;
                }
                else
                {
                    d.character->desc = NULL;

                    // Check if MFA is enabled or has any MFA data set
                    if (!d.character->pcdata->mfa_enabled && 
                        IS_NULLSTR(d.character->pcdata->mfa_key)) 
                        //d.character->pcdata->qr_code_expiration == 0)
                    {
                        sprintf(buf, "Multifactor auth is not enabled for %s.\n\r", d.character->name);
                        send_to_char(buf, ch);
                        free_char(d.character);
                        return;
                    }

                    // Reset MFA settings
                    d.character->pcdata->mfa_enabled = false;
                    
                    if (!IS_NULLSTR(d.character->pcdata->mfa_key))
                    {
                        free_string(d.character->pcdata->mfa_key);
                        d.character->pcdata->mfa_key = str_dup("");
                    }
                    
                    //d.character->pcdata->qr_code_expiration = 0;

                    sprintf(buf, "Multifactor auth has been disabled for %s.\n\r", d.character->name);
                    send_to_char(buf, ch);
                    save_char_obj(d.character);
                    free_char(d.character);
                }
            }
            else
            {
                // Check if MFA is enabled for online character
                if (!victim->pcdata->mfa_enabled && 
                    IS_NULLSTR(victim->pcdata->mfa_key)) 
                    //victim->pcdata->qr_code_expiration == 0)
                {
                    sprintf(buf, "Multifactor auth is not enabled for %s.\n\r", victim->name);
                    send_to_char(buf, ch);
                    return;
                }

                // Reset MFA for online character
                victim->pcdata->mfa_enabled = false;
                
                if (!IS_NULLSTR(victim->pcdata->mfa_key))
                {
                    free_string(victim->pcdata->mfa_key);
                    victim->pcdata->mfa_key = str_dup("");
                }
                
                //victim->pcdata->qr_code_expiration = 0;

                sprintf(buf, "Multifactor auth has been disabled for %s.\n\r", victim->name);
                send_to_char(buf, ch);
                save_char_obj(victim);
            }
        }
        else
        {
            send_to_char("That player does not exist.\n\r", ch);
            return;
        }
    }
}

void do_lvlaudit(CHAR_DATA *ch, char *argument)
{
	ITERATOR it;
	AREA_DATA *area;
	int count = 0;
	int sum = 0;
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];

	iterator_start(&it, loaded_chars);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax: lvlaudit <area>\n\r", ch);
		return;
	}

	area = find_area_kwd(argument);

	if (area == NULL)
	{
		send_to_char("That area does not exist.\n\r", ch);
		return;
	}

	while ((victim = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
	{
		if (!IS_NPC(victim))
			continue;

		if (victim->in_room->area != area)
			continue;

		if (IS_SET(victim->act[0], ACT_PET) || IS_SET(victim->act[0], ACT_PROTECTED) || IS_SET(victim->act[0], ACT_TRAIN) ||
		IS_SET(victim->act[0], ACT_PRACTICE) || IS_SET(victim->act[0], ACT_IS_HEALER) || IS_SET(victim->act[0], ACT_CREW_SELLER) ||
		IS_SET(victim->act[0], ACT_IS_BANKER) || IS_SET(victim->act[0], ACT_IS_CHANGER) || IS_SET(victim->act[1], ACT2_CHURCHMASTER) ||
		IS_SET(victim->act[1], ACT2_PLANE_TUNNELER) || IS_SET(victim->act[1], ACT2_AIRSHIP_SELLER) || IS_SET(victim->act[1], ACT2_WIZI_MOB) ||
		IS_SET(victim->act[1], ACT2_TRADER) || IS_SET(victim->act[1], ACT2_LOREMASTER) || IS_SET(victim->act[1], ACT2_GQ_MASTER) ||
		IS_SET(victim->act[1], ACT2_SHIP_QUESTMASTER) || IS_SET(victim->act[1], ACT2_PIRATE) || IS_SET(victim->act[1], ACT2_INVASION_LEADER) ||
		IS_SET(victim->act[1], ACT2_INVASION_MOB) || IS_SET(victim->act[1], ACT2_SOUL_DEPOSIT) || IS_SET(victim->act[1], ACT2_INSTANCE_MOB) ||
		IS_SET(victim->act[1], ACT2_HIRED) || IS_SET(victim->act[1], ACT2_RENEWER) || IS_SET(victim->act[1], ACT2_ADVANCED_TRAINER) || IS_SET(victim->in_room->room_flag[0], ROOM_SAFE) ||
		victim->shop != NULL || victim->pIndexData->pQuestor != NULL)
			continue;

		count++;
		sum += victim->level;
		
	}
	iterator_stop(&it);

	sprintf(buf, "Total mobs in %s: %d\n\r", area->name, count);
	send_to_char(buf, ch);
	sprintf(buf, "Average level of available mobs: %d\n\r", sum / count);
	send_to_char(buf, ch);
	return;
}

void do_acctlink(CHAR_DATA *ch, char *argument) {
    char account_name_arg[MAX_INPUT_LENGTH];
    char char_name_arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    ACCOUNT_DATA *target_account = NULL;
    CHAR_DATA *char_to_link = NULL;
    DESCRIPTOR_DATA d_account, d_char; // d_account for potentially loading offline account
    DESCRIPTOR_DATA *d_iter;
    bool is_account_online = false;

    argument = one_argument(argument, account_name_arg);
    argument = one_argument(argument, char_name_arg);

    if (account_name_arg[0] == '\0' || char_name_arg[0] == '\0') {
        send_to_char("Syntax: acctlink <account_name> <character_name>\n\r", ch);
        return;
    }

    // Initialize d_account. It will only be used if the account is loaded from disk.
    memset(&d_account, 0, sizeof(d_account));

    // 1. Find Account: Check if account is online first
    for (d_iter = descriptor_list; d_iter != NULL; d_iter = d_iter->next) {
        if (d_iter->account != NULL &&
            !str_cmp(d_iter->account->username, account_name_arg)) {
            target_account = d_iter->account;
            is_account_online = true;
            // Optional: send_to_char("Notice: Account is currently online. Using live data.\n\r", ch);
            break;
        }
    }

    // If account not found online, try loading from disk
    if (!target_account) {
        if (!account_exists(account_name_arg)) {
            sprintf(buf, "Account '%s' does not exist.\n\r", account_name_arg);
            send_to_char(buf, ch);
            return;
        }
        // Account exists, try to load it into d_account
        if (!load_account(&d_account, account_name_arg)) {
            send_to_char("That account exists but could not be loaded. Check server logs.\n\r", ch);
            // d_account.account is NULL or invalid if load_account failed and cleaned up.
            return;
        }
        target_account = d_account.account;
        is_account_online = false; // Mark that this account was loaded
    }

    // At this point, target_account is valid (either online or loaded into d_account.account)
    // or we have returned.

    // 2. Check if character to be linked is online
    // get_char_world's first argument 'ch' is for visibility checks by the admin.
    if ((char_to_link = get_char_world(ch, char_name_arg)) != NULL) {
        send_to_char("That character is currently online. They must be offline to be linked.\n\r", ch);
        if (!is_account_online && d_account.account) { // If account was loaded from disk
            free_account(d_account.account);
            d_account.account = NULL;
        }
        return;
    }
    char_to_link = NULL; // Reset pointer, will be set by load_char_obj

    // 3. Load Character (must be offline)
    memset(&d_char, 0, sizeof(d_char));
    if (!player_exists(char_name_arg) || !load_char_obj(&d_char, char_name_arg)) {
        sprintf(buf, "Character '%s' does not exist or could not be loaded.\n\r", char_name_arg);
        send_to_char(buf, ch);
        if (!is_account_online && d_account.account) { // If account was loaded
            free_account(d_account.account);
            d_account.account = NULL;
        }
        return;
    }
    char_to_link = d_char.character;
    char_to_link->desc = NULL; // Standard practice for offline char manipulation

    if (IS_NPC(char_to_link)) {
        send_to_char("NPCs cannot be linked to accounts.\n\r", ch);
        free_char(char_to_link); // char_to_link is d_char.character
        d_char.character = NULL;
        if (!is_account_online && d_account.account) {
            free_account(d_account.account);
            d_account.account = NULL;
        }
        return;
    }

    // 4. Check if character is already linked
    if (char_to_link->pcdata->account_name != NULL && char_to_link->pcdata->account_name[0] != '\0') {
        if (!str_cmp(char_to_link->pcdata->account_name, target_account->username)) {
            sprintf(buf, "Character %s is already linked to account %s.\n\r",
                    char_to_link->name, target_account->username);
            send_to_char(buf, ch);
        } else {
            sprintf(buf, "Character %s is already linked to another account (%s).\n\r"
                         "Please unlink it first using 'acctunlink %s'.\n\r",
                    char_to_link->name, char_to_link->pcdata->account_name, char_to_link->name);
            send_to_char(buf, ch);
        }
        free_char(char_to_link);
        d_char.character = NULL;
        if (!is_account_online && d_account.account) {
            free_account(d_account.account);
            d_account.account = NULL;
        }
        return;
    }

    // 5. Link character to account (update character's pfile)
    free_string(char_to_link->pcdata->account_name);
    char_to_link->pcdata->account_name = str_dup(target_account->username);

    // Copy account ID to character. Assuming account_id is char[2] and target_account->id is similar.
    if (target_account->id[0] != '\0') { // Check if account ID is not empty
        char_to_link->pcdata->account_id[0] = target_account->id[0];
        char_to_link->pcdata->account_id[1] = target_account->id[1];
    } else { // If account ID is empty, clear it on the character
        char_to_link->pcdata->account_id[0] = '\0';
        char_to_link->pcdata->account_id[1] = '\0';
    }

    // Clear character-specific password and reset info, as account credentials will be used
    if (char_to_link->pcdata->pwd) { // Ensure pwd is not NULL before freeing
        free_string(char_to_link->pcdata->pwd);
    }
    char_to_link->pcdata->pwd = str_dup(""); // Set to empty string
    char_to_link->pcdata->pwd_vers = 0;    // Or appropriate value for "use account password"

    if (char_to_link->pcdata->reset_code != NULL && char_to_link->pcdata->reset_code[0] != '\0') {
        free_string(char_to_link->pcdata->reset_code);
        char_to_link->pcdata->reset_code = str_dup("");
    }
    char_to_link->pcdata->reset_state = 0;    // e.g., RESET_NONE
    char_to_link->pcdata->need_change_pw = 0; // No longer needs individual password change

    save_char_obj(char_to_link);

    // 6. Add character to account's list
    // account_add_character expects a fully loaded CHAR_DATA, which char_to_link is.
    account_add_character(target_account, char_to_link);

    // Explicitly save the account, whether it was online or loaded from disk.
    // This ensures changes (like the new character link) are persisted immediately.
    save_account(target_account);

    sprintf(buf, "Character %s has been successfully linked to account %s.\n\r"
                 "The character will now use the account's password.\n\r",
            char_to_link->name, target_account->username);
    send_to_char(buf, ch);
    log_stringf("ACCTLINK: Admin %s linked char %s to account %s.",
                ch->name, char_to_link->name, target_account->username);

    // 7. Cleanup
    // char_to_link was loaded into d_char.character's memory
    free_char(char_to_link);
    d_char.character = NULL; // Nullify to prevent d_char from holding a stale pointer
    char_to_link = NULL;     // Nullify working pointer

    // target_account was either from descriptor_list (online) or d_account.account (loaded from disk)
    if (!is_account_online && d_account.account) {
        // If it was loaded into d_account.account, free that memory.
        // target_account would be pointing to d_account.account in this case.
        free_account(d_account.account);
        d_account.account = NULL; // Nullify to prevent d_account from holding a stale pointer
    }
    target_account = NULL; // Nullify working pointer, its content is either managed elsewhere or freed.
}

/*
 * Unlinks a character from its account.
 * Syntax: acctunlink <character_name>
 */
void do_acctunlink(CHAR_DATA *ch, char *argument) {
    char char_name_arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char old_account_name[MAX_INPUT_LENGTH];
    char new_plain_password[16];

    ACCOUNT_DATA *source_account = NULL;
    CHAR_DATA *char_to_unlink = NULL;
    DESCRIPTOR_DATA d_account, d_char;
    DESCRIPTOR_DATA *d_iter;
    bool is_account_online = false;

    one_argument(argument, char_name_arg);

    if (char_name_arg[0] == '\0') {
        send_to_char("Syntax: acctunlink <character_name>\n\r", ch);
        return;
    }

    // 1. Check if character is online
    if (get_char_world(ch, char_name_arg) != NULL) {
        send_to_char("That character is currently online. They must be offline to be unlinked.\n\r", ch);
        return;
    }

    // 2. Load Character (offline)
    memset(&d_char, 0, sizeof(d_char));
    if (!player_exists(char_name_arg) || !load_char_obj(&d_char, char_name_arg)) {
        sprintf(buf, "Character '%s' does not exist or could not be loaded.\n\r", char_name_arg);
        send_to_char(buf, ch);
        return;
    }
    char_to_unlink = d_char.character;
    char_to_unlink->desc = NULL;

    if (IS_NPC(char_to_unlink)) {
        send_to_char("NPCs are not linked to accounts.\n\r", ch);
        free_char(char_to_unlink);
        d_char.character = NULL;
        return;
    }

    // 3. Check if character is actually linked
    if (char_to_unlink->pcdata->account_name == NULL || char_to_unlink->pcdata->account_name[0] == '\0') {
        sprintf(buf, "Character %s is not currently linked to any account.\n\r", char_to_unlink->name);
        send_to_char(buf, ch);
        free_char(char_to_unlink);
        d_char.character = NULL;
        return;
    }
    strncpy(old_account_name, char_to_unlink->pcdata->account_name, MAX_INPUT_LENGTH - 1);
    old_account_name[MAX_INPUT_LENGTH - 1] = '\0';

    // Free char_to_unlink now; account_remove_character will load/save/free its own copy.
    free_char(char_to_unlink);
    d_char.character = NULL;
    char_to_unlink = NULL;

    // 4. Find Account: Check if account is online first
    memset(&d_account, 0, sizeof(d_account));
    is_account_online = false;
    for (d_iter = descriptor_list; d_iter != NULL; d_iter = d_iter->next) {
        if (d_iter->account != NULL &&
            !str_cmp(d_iter->account->username, old_account_name)) {
            source_account = d_iter->account;
            is_account_online = true;
            break;
        }
    }

    // If account not found online, try loading from disk
    if (!source_account) {
        if (!account_exists(old_account_name) || !load_account(&d_account, old_account_name)) {
            sprintf(buf, "The account (%s) character %s was linked to could not be loaded. This may indicate data inconsistency.\n\r"
                         "Attempting to force unlink character pfile data only.\n\r", old_account_name, char_name_arg);
            send_to_char(buf, ch);
            log_stringf("ACCTUNLINK: Failed to load account %s for unlinking char %s.", old_account_name, char_name_arg);
            return;
        }
        source_account = d_account.account;
        is_account_online = false;
    }

    // 5. Remove character from account (this also updates char pfile to clear account info & saves account)
    account_remove_character(source_account, char_name_arg);

    // Only free the account if we loaded it from disk
    if (!is_account_online && d_account.account) {
        free_account(d_account.account);
        d_account.account = NULL;
    }
    source_account = NULL;

    // 6. Set a new temporary password for the now-unlinked character
    memset(&d_char, 0, sizeof(d_char));
    if (!load_char_obj(&d_char, char_name_arg)) {
        sprintf(buf, "Error: Could not reload character %s after unlinking to set new password. Check logs.\n\r", char_name_arg);
        send_to_char(buf, ch);
        log_stringf("ACCTUNLINK: Critical error reloading %s after unlinking from %s.", char_name_arg, old_account_name);
        return;
    }
    char_to_unlink = d_char.character;
    char_to_unlink->desc = NULL;

    generate_reset_code(new_plain_password, 10);

    if (!set_encrypted_password(&char_to_unlink->pcdata->pwd,
                                &char_to_unlink->pcdata->pwd_vers,
                                new_plain_password)) {
        send_to_char("Password setting failed for unlinked character due to an internal error. Please check server logs.\n\r", ch);
        log_stringf("ACCTUNLINK: set_encrypted_password failed for %s.", char_to_unlink->name);
        free_char(char_to_unlink);
        d_char.character = NULL;
        return;
    }
    char_to_unlink->pcdata->need_change_pw = 1;

    save_char_obj(char_to_unlink);

    sprintf(buf, "Character %s has been unlinked from account %s.\n\r"
                 "A new temporary password has been set for %s: %s\n\r"
                 "The player will be required to change this password upon their next login.\n\r",
            char_to_unlink->name, old_account_name, char_to_unlink->name, new_plain_password);
    send_to_char(buf, ch);
    log_stringf("ACCTUNLINK: Admin %s unlinked char %s from account %s. New temp pass: %s",
                ch->name, char_to_unlink->name, old_account_name, new_plain_password);

    // 7. Cleanup
    free_char(char_to_unlink);
    d_char.character = NULL;
}

// Add a command for immortals to view GC stats
void do_gcstats(CHAR_DATA *ch, char *argument)
{
    if (!IS_IMMORTAL(ch)) {
        send_to_char("Huh?\n\r", ch);
        return;
    }
    
    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Garbage Collection Statistics:\n\r");
    sprintf(buf + strlen(buf), "Items waiting: Mobs %d, Objs %d, Rooms %d, Tokens %d\n\r",
            list_size(gc_mobiles), list_size(gc_objects), 
            list_size(gc_rooms), list_size(gc_tokens));
    sprintf(buf + strlen(buf), "Total GC calls: %ld\n\r", gc_calls);
    sprintf(buf + strlen(buf), "Total items processed: %ld\n\r", gc_total_processed);
    sprintf(buf + strlen(buf), "Average items per call: %.2f\n\r", 
            gc_calls > 0 ? (float)gc_total_processed / gc_calls : 0);
    sprintf(buf + strlen(buf), "Max GC time: %ld ms\n\r", gc_max_time);
    
    send_to_char(buf, ch);
}