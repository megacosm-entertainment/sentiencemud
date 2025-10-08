/*

Dummy file that will contain method functions to be referenced by the pointer table

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ctype.h>

#include "../merc.h"
#include "niblang.h"
#include "script.h"
#include "interpret.h"
#include "tables.h"

static WNUM __wnum_zero;

GAME_SETTINGS_DATA game_settings;

AREA_DATA plith;

SECTOR_DATA sector_inside;
SECTOR_DATA sector_city;
SECTOR_DATA sector_field;

ROOM_INDEX_DATA beginning;
EXIT_DATA beginning_down;

ROOM_INDEX_DATA pious_street;
EXIT_DATA pious_street_up;

MOB_INDEX_DATA steiner_index;
CHAR_DATA steiner;

MOB_INDEX_DATA ravage_index;
CHAR_DATA ravage;

MOB_INDEX_DATA mayor_index;
CHAR_DATA mayor;

OBJ_INDEX_DATA cloak_index;
OBJ_DATA cloak;

OBJ_INDEX_DATA sword_index;
OBJ_DATA sword;

CHAR_DATA you;
PC_DATA you_pcdata;

CLASS_DATA paladin;

RACE_DATA human;

static LLIST *nib_variables = NULL;

#define CLRMEM(m)		memset(&(m),0,sizeof(m))
#define CLRMEMV(m)		do { memset(&(m),0,sizeof(m)); (m).valid = true; } while(0)
void game_settings_init()
{
	CLRMEM(game_settings);

    /* Basic settings */
    game_settings.game_name = "SentienceMUD";
    game_settings.login_string = "LoginString What is your name?\n\r\n\r";
    game_settings.server_description = "Nibelung 2.0 Test Server";
    game_settings.testport = false;
    game_settings.dev_server = true;
    game_settings.wizlock = true;
    game_settings.new_acct_lock = false;
    game_settings.new_char_lock = false;
    game_settings.wizlock_msg = "Not ready for public access.";
    game_settings.new_acct_lock_msg = "";
    game_settings.new_char_lock_msg = "";
    game_settings.logall = false;
	game_settings.note_boot_errors = false;

    /* Auth */
    game_settings.require_uniq_pass_staff = false;
    game_settings.max_login_attempts = 3;
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
    game_settings.idle_time = 12;
    game_settings.idle_disconnect_time = 30;

    /* Misc Maximums */
    game_settings.max_alias = 80;
    game_settings.max_characters = 0;
    game_settings.max_orgs = 0;
    game_settings.max_logfile_size = 1000000;
	game_settings.org_disable_pk_pneuma_cost = 0;

    /* Email */
    game_settings.enable_email = true;
    game_settings.email_port = 2587;
    game_settings.email_username = "SD6HKJAK234HKJHD8LHJ";
    game_settings.email_host = "email-smtp.us-west-2.amazonaws.com";
    game_settings.email_password = "FMS7zEvwc3cN52IIycrba6FAsd98jlAop1d3HA2fTRW3";
    game_settings.email_from_addr = "notify@sentiencemud.net";
    game_settings.email_from_name = "The Towne Crier";

    /* Missions */
    game_settings.max_mission_allowance = 100;
    game_settings.inc_missions = 6;
    game_settings.max_missions = 25;

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
    game_settings.vault_additional_cost_per_char = 0;
    game_settings.vault_additional_slots_per_char = 0;
    game_settings.vault_additional_weight_per_char = 0;

    /* Coffer Settings */
    game_settings.max_coffer_weight = 0;
    game_settings.max_coffer_items = 0;
    game_settings.coffer_enabled = false;
    game_settings.coffer_rent = false;
    game_settings.coffer_rent_cost = 0;
    game_settings.coffer_rent_currency = "";
    game_settings.coffer_rent_period = 0;

    /* Protocols and Ports*/
    game_settings.enable_telnet = true;
    game_settings.telnet_port = 9999;
    game_settings.enable_tls = false;
    game_settings.tls_port = 9110;
    game_settings.enable_websocket_tls = false;
    game_settings.websocket_tls_port = 0;
    game_settings.enable_web = false;
    game_settings.ssl_cert_path = "/sentience/data/system/certs/sentience.pem";
    game_settings.ssl_key_path = "/sentience/data/system/certs/sentience.key";
    game_settings.enable_insecure_warning = true;
    game_settings.insecure_warning_msg = "{RWARNING: {XAll traffic to and from this port is plaintext.{X";

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
}


void dummy_init()
{
	game_settings_init();

	CLRMEMV(paladin);
	CLRMEMV(human);
	CLRMEM(sector_inside);
	CLRMEM(sector_city);
	CLRMEM(sector_field);
	CLRMEM(plith);
	CLRMEM(beginning);
	CLRMEM(pious_street);
	CLRMEM(steiner_index);
	CLRMEMV(steiner);
	CLRMEM(ravage_index);
	CLRMEMV(ravage);
	CLRMEM(mayor_index);
	CLRMEMV(mayor);
	CLRMEM(cloak_index);
	CLRMEMV(cloak);
	CLRMEM(sword_index);
	CLRMEMV(sword);
	CLRMEMV(you);
	CLRMEMV(you_pcdata);

	sector_inside.name = strdup("inside");
	sector_inside.flags = SECTOR_INDOORS | SECTOR_CITY_LIGHTS;
	sector_inside.sector_class = SECTCLASS_CITY;
	sector_inside.move_cost = 1;
	sector_inside.hp_regen = 100;
	sector_inside.mana_regen = 100;
	sector_inside.move_regen = 100;
	sector_inside.soil = -20;

	sector_city.name = strdup("city");
	sector_city.flags = SECTOR_CITY_LIGHTS;
	sector_city.sector_class = SECTCLASS_CITY;
	sector_city.move_cost = 2;
	sector_city.hp_regen = 100;
	sector_city.mana_regen = 100;
	sector_city.move_regen = 100;
	sector_city.soil = -10;

	sector_field.name = strdup("field");
	sector_field.flags = SECTOR_NATURE;
	sector_field.sector_class = SECTCLASS_PLAINS;
	sector_field.move_cost = 2;
	sector_field.hp_regen = 100;
	sector_field.mana_regen = 100;
	sector_field.move_regen = 100;
	sector_field.soil = 5;

	steiner_index.area = &plith;
	steiner_index.vnum = 1L;
	steiner.pIndexData = &steiner_index;
	steiner.valid = true;
	steiner.name = strdup("steiner");
	steiner.short_descr = strdup("Steiner");
	steiner.long_descr = strdup("Steiner watches over the Town of Plith.");
	steiner.description = strdup("Steiner watches over the Town of Plith.");
	steiner.lcarrying = list_create(false);
	steiner.lworn = list_create(false);
	SET_BIT(steiner.act[0], ACT_IS_NPC);
	steiner.race = &human;

	ravage_index.area = &plith;
	ravage_index.vnum = 2L;
	ravage.pIndexData = &ravage_index;
	ravage.valid = true;
	ravage.name = strdup("ravage");
	ravage.short_descr = strdup("Ravage");
	ravage.long_descr = strdup("The sinister Ravage looms over the city.");
	ravage.description = strdup("The sinister Ravage looms over the city.");
	ravage.lcarrying = list_create(false);
	ravage.lworn = list_create(false);
	SET_BIT(ravage.act[0], ACT_IS_NPC);
	ravage.race = &human;

	mayor_index.area = &plith;
	mayor_index.vnum = 3L;
	mayor.pIndexData = &mayor_index;
	mayor.valid = true;
	mayor.name = strdup("mayor plith");
	mayor.short_descr = strdup("the Mayor of Plith");
	mayor.long_descr = strdup("The Mayor governs Plith with a firm hand.");
	mayor.description = strdup("The Mayor governs Plith with a firm hand.");
	mayor.lcarrying = list_create(false);
	mayor.lworn = list_create(false);
	SET_BIT(mayor.act[0], ACT_IS_NPC);
	mayor.race = &human;

	sword_index.area = &plith;
	sword_index.vnum = 1L;
	sword.pIndexData = &sword_index;
	sword.name = strdup("sword justice");
	sword.short_descr = strdup("the Sword of Justice");
	sword.description = strdup("A great sword gleams in the sunlight.");
	sword.full_description = strdup("A great sword brandishing a large S at the crossbar.");
	sword.carried_by = &steiner;
	sword.wear_loc = WEAR_WIELD;
	sword.item_type = ITEM_WEAPON;
	sword.level = 15;
	sword.condition = 76;
	sword.extra[0] = ITEM_BLESS | ITEM_GLOW | ITEM_HUM;
	sword.extra[1] = ITEM_EMITS_LIGHT;
	sword.extra[2] = ITEM_KEEP_EQUIPPED;
	list_appendlink(sword.carried_by->lworn,&sword);

	cloak_index.area = &plith;
	cloak_index.vnum = 2L;
	cloak.pIndexData = &cloak_index;
	cloak.name = strdup("cloak ravage");
	cloak.short_descr = strdup("the Cloak of Ravage");
	cloak.description = strdup("An inky black cloak lies in a heap.");
	cloak.full_description = strdup("An inky black cloak lies in a heap.");
	cloak.carried_by = &ravage;
	cloak.wear_loc = WEAR_BACK;
	cloak.item_type = ITEM_ARMOUR;
	cloak.level = 70;
	cloak.condition = 98;
	list_appendlink(cloak.carried_by->lworn,&cloak);


	beginning.area = &plith;
	beginning.vnum = 1L;
	beginning.name = strdup("The Beginning");
	beginning.description = strdup("The heart of the Town of Plith.");
	beginning.lpeople = list_create(false);
	beginning.rs_sector = &sector_inside;
	beginning.sector = &sector_inside;
	beginning.sector_flags = beginning.rs_sector->flags;
	list_appendlink(beginning.lpeople, &steiner);
	list_appendlink(beginning.lpeople, &ravage);
	list_appendlink(beginning.lpeople, &mayor);

	beginning_down.from_room = &beginning;
	beginning_down.orig_door = DIR_DOWN;
	beginning_down.u1.to_room = &pious_street;
	beginning.exit[DIR_DOWN] = &beginning_down;
	beginning_down.exit_info = EX_ISDOOR | EX_CLOSED;

	pious_street.area = &plith;
	pious_street.vnum = 2L;
	pious_street.name = strdup("Pious Street");
	pious_street.description = strdup("The glorious street of piety.");
	pious_street.lpeople = list_create(false);
	pious_street.rs_sector = &sector_city;
	pious_street.sector = &sector_city;
	pious_street.sector_flags = pious_street.rs_sector->flags;


	pious_street_up.from_room = &pious_street;
	pious_street_up.orig_door = DIR_UP;
	pious_street_up.u1.to_room = &beginning;
	pious_street.exit[DIR_UP] = &pious_street_up;
	pious_street_up.exit_info = EX_ISDOOR | EX_CLOSED;
	
	plith.uid = 1;
	plith.name = strdup("Plith");
	plith.description = strdup("The Town of Plith.");
	plith.area_flags = AREA_NEWBIE;
	plith.room_list = list_create(false);
	plith.room_index_hash[beginning.vnum % MAX_KEY_HASH] = &beginning;
	plith.room_index_hash[pious_street.vnum % MAX_KEY_HASH] = &pious_street;
	list_appendlink(plith.room_list, &beginning);
	list_appendlink(plith.room_list, &pious_street);

	paladin.name = strdup("paladin");
	paladin.description = strdup("A holy knight.");
	paladin.display[SEX_NEUTRAL] = strdup("Paladin(N)");
	paladin.display[SEX_MALE] = strdup("Paladin(M)");
	paladin.display[SEX_FEMALE] = strdup("Paladin(F)");
	paladin.display[SEX_EITHER] = strdup("Paladin(?)");
	paladin.who[SEX_NEUTRAL] = strdup("(N)Paladin");
	paladin.who[SEX_MALE] = strdup("(M)Paladin");
	paladin.who[SEX_FEMALE] = strdup("(F)Paladin");
	paladin.who[SEX_EITHER] = strdup("(?)Paladin");
	paladin.uid = 1;
	paladin.type = CLASS_WARRIOR;
	paladin.flags = CLASS_COMBATIVE;
	// paladin.groups
	paladin.primary_stat = STAT_STR;
	paladin.max_level = MAX_CLASS_LEVEL;

	human.name = strdup("human");
	human.description = strdup("");
	human.comments = strdup("");
	human.uid = 1;
	human.playable = true;
	human.starting = true;
	human.flags = 0;
	human.act[0] = 0;
	human.act[1] = 0;
	human.aff[0] = 0;
	human.aff[1] = 0;
	human.off = 0;
	human.imm = 0;
	human.res = 0;
	human.vuln = 0;
	human.form = 0;
	human.parts = 0;
	human.premort = NULL;
	human.remort = false;
	human.who = strdup("Human");
	human.skills = list_create(false);
	human.stats[STAT_STR] = 13;
	human.max_stats[STAT_STR] = 18;
	human.stats[STAT_DEX] = 13;
	human.max_stats[STAT_DEX] = 18;
	human.stats[STAT_INT] = 13;
	human.max_stats[STAT_INT] = 18;
	human.stats[STAT_WIS] = 13;
	human.max_stats[STAT_WIS] = 18;
	human.stats[STAT_CON] = 13;
	human.max_stats[STAT_CON] = 18;
	human.max_vitals[0] = 3000;
	human.max_vitals[1] = 3000;
	human.max_vitals[2] = 3000;
	human.min_size = SIZE_MEDIUM;
	human.max_size = SIZE_MEDIUM;
	human.default_alignment = 0;


 	nib_register_flag_table("account",acct_flags);
 	nib_register_flag_table("area_region",area_region_flags);
 	nib_register_flag_table("area",area_flags);
 	nib_register_flag_table("areaplace",place_flags);
 	nib_register_flag_table("blueprint_section",blueprint_section_flags);
 	nib_register_flag_table("blueprint",blueprint_flags);
 	nib_register_flag_table("book",book_flags);
 	nib_register_flag_table("cart",cart_flags);
 	nib_register_flag_table("church",church_flags);
 	nib_register_flag_table("class",class_flags);
 	nib_register_flag_table("comm",comm_flags);
 	nib_register_flag_table("compartment",compartment_flags);
 	nib_register_flag_table("container",container_flags);
 	nib_register_flag_table("corpse_object",corpse_object_flags);
 	nib_register_flag_table("dungeon",dungeon_flags);
 	nib_register_flag_table("exit",exit_flags);
 	nib_register_flag_table("fluid_con",fluid_con_flags);
 	nib_register_flag_table("form",form_flags);
 	nib_register_flag_table("furniture_action",furniture_action_flags);
 	nib_register_flag_table("furniture",furniture_flags);
 	nib_register_flag_table("immune",imm_flags);
 	nib_register_flag_table("instance",instance_flags);
 	nib_register_flag_table("instrument",instrument_flags);
 	nib_register_flag_table("light",light_flags);
 	nib_register_flag_table("lock",lock_flags);
 	nib_register_flag_table("material",material_flags);
 	nib_register_flag_table("offense",off_flags);
 	nib_register_flag_table("part",part_flags);
 	nib_register_flag_table("portal_exit",portal_exit_flags);
 	nib_register_flag_table("portal",portal_flags);
 	nib_register_flag_table("practice_entry",practice_entry_flags);
 	nib_register_flag_table("prog_entity",prog_entity_flags);
 	nib_register_flag_table("project",project_flags);
 	nib_register_flag_table("reputation_rank",reputation_rank_flags);
 	nib_register_flag_table("reputation",reputation_flags);
 	nib_register_flag_table("resist",res_flags);
 	nib_register_flag_table("scroll",scroll_flags);
 	nib_register_flag_table("sector",sector_flags);
 	nib_register_flag_table("ship",ship_flags);
 	nib_register_flag_table("shop",shop_flags);
 	nib_register_flag_table("skill_entry",skill_entry_flags);
 	nib_register_flag_table("skill",skill_flags);
 	nib_register_flag_table("song",song_flags);
 	nib_register_flag_table("stock",stock_types);
 	nib_register_flag_table("time_of_day",time_of_day_flags);
 	nib_register_flag_table("token",token_flags);
 	nib_register_flag_table("vuln",vuln_flags);
 	nib_register_flag_table("wear",wear_flags);

 	nib_register_flag_bank("mobile",act_flagbank);
 	nib_register_flag_bank("player",plr_flagbank);
 	nib_register_flag_bank("affect",affect_flagbank);
 	nib_register_flag_bank("object",extra_flagbank);
 	nib_register_flag_bank("room",room_flagbank);

 	nib_register_stat_table("ac",ac_type);
 	nib_register_stat_table("adornment",adornment_types);
 	nib_register_stat_table("affgroup_mobile",affgroup_mobile_flags);
 	nib_register_stat_table("affgroup_object",affgroup_object_flags);
 	nib_register_stat_table("affgroup",affgroup_flags);
 	nib_register_stat_table("ammo",ammo_types);
 	nib_register_stat_table("apply_types",apply_types);
 	nib_register_stat_table("apply",apply_flags);
 	nib_register_stat_table("area_who_display",area_who_display);
 	nib_register_stat_table("area_who_titles",area_who_titles);
 	nib_register_stat_table("armour_protection",armour_protection_types);
 	nib_register_stat_table("armour",armour_types);
 	nib_register_stat_table("blueprint_section",blueprint_section_types);
 	nib_register_stat_table("boolean",boolean_types);
 	nib_register_stat_table("catalyst",catalyst_types);
 	nib_register_stat_table("catalyst_method",catalyst_method_types);
 	nib_register_stat_table("church_sizes",church_sizes);
 	nib_register_stat_table("class",class_types);
 	nib_register_stat_table("corpse",corpse_types);
 	nib_register_stat_table("damage_class",damage_classes);
 	nib_register_stat_table("death_release",death_release_modes);
 	nib_register_stat_table("death",death_types);
 	nib_register_stat_table("food_buff",food_buff_types);
 	nib_register_stat_table("instrument",instrument_types);
 	nib_register_stat_table("itemtypes",type_flags);
 	nib_register_stat_table("material_class",material_classes);
 	nib_register_stat_table("missionary",missionary_types);
 	nib_register_stat_table("moon_phases",moon_phases);
 	nib_register_stat_table("portal_gate",portal_gatetype);
 	nib_register_stat_table("position",position_flags);
 	nib_register_stat_table("ranged_weapon_class",ranged_weapon_class);
 	nib_register_stat_table("room_condition",room_condition_flags);
 	nib_register_stat_table("script_spaces",script_spaces);
 	nib_register_stat_table("sector_class",sector_classes);
 	nib_register_stat_table("sectortypes",sector_types);
 	nib_register_stat_table("sex",sex_flags);
 	nib_register_stat_table("ship_class",ship_class_types);
 	nib_register_stat_table("size",size_flags);
 	nib_register_stat_table("skill_sources",skill_sources);
 	nib_register_stat_table("song_target",song_target_types);
 	nib_register_stat_table("spell_position",spell_position_flags);
 	nib_register_stat_table("spell_target",spell_target_types);
 	nib_register_stat_table("staff_ranks",staff_ranks);
 	nib_register_stat_table("stat",stat_types);
 	nib_register_stat_table("tattoo_loc",tattoo_loc_flags);
	nib_register_stat_table("token",token_types);
	nib_register_stat_table("token_owner",token_owner_types);
 	nib_register_stat_table("tool",tool_types);
 	nib_register_stat_table("transfer_modes",transfer_modes);
 	nib_register_stat_table("vital",vital_types);
 	nib_register_stat_table("weapon_class",weapon_class);
 	nib_register_stat_table("weapon_type2",weapon_type2);
 	nib_register_stat_table("wear_loc",wear_loc_flags);

}

static void __cleanup_class(CLASS_DATA *clazz)
{
	if (clazz->name) free(clazz->name);
	if (clazz->description) free(clazz->description);

	for(int i = 0; i < SEX_MAX; i++)
	{
		if (clazz->display[i]) free(clazz->display[i]);
		if (clazz->who[i]) free(clazz->who[i]);
	}

	// list_destroy(clazz->groups);
}

static void __cleanup_race(RACE_DATA *race)
{

}

static void __cleanup_sector(SECTOR_DATA *sector)
{
	if (sector->name) free(sector->name);
}

static void __cleanup_pcdata(PC_DATA *pcdata)
{

}

static void __cleanup_mobile(CHAR_DATA *ch)
{
	if (ch->name) free(ch->name);
	if (ch->short_descr) free(ch->short_descr);
	if (ch->long_descr) free(ch->long_descr);
	if (ch->description) free(ch->description);
	list_destroy(ch->lcarrying);
	list_destroy(ch->lworn);

	if (ch->pcdata) __cleanup_pcdata(ch->pcdata);
}

static void __cleanup_object(OBJ_DATA *obj)
{
	if (obj->name) free(obj->name);
	if (obj->short_descr) free(obj->short_descr);
	if (obj->description) free(obj->description);
	if (obj->full_description) free(obj->full_description);
}

static void __cleanup_exit(EXIT_DATA *ex)
{
	// Nothing yet.
}

static void __cleanup_room(ROOM_INDEX_DATA *room)
{
	if (room->name) free(room->name);
	if (room->description) free(room->description);
	list_destroy(room->lpeople);

	for(int i = 0; i < MAX_DIR; i++)
		if (room->exit[i])
		{
			__cleanup_exit(room->exit[i]);
		}
}

static void __cleanup_area(AREA_DATA *area)
{
	if (area->name) free(area->name);
	if (area->description) free(area->description);
	list_destroy(area->room_list);
}

void dummy_cleanup()
{
	__cleanup_mobile(&steiner);
	__cleanup_mobile(&ravage);
	__cleanup_mobile(&mayor);

	__cleanup_object(&sword);
	__cleanup_object(&cloak);

	__cleanup_room(&beginning);
	__cleanup_room(&pious_street);

	__cleanup_area(&plith);

	__cleanup_sector(&sector_inside);
	__cleanup_sector(&sector_city);

	__cleanup_class(&paladin);
	__cleanup_race(&human);

}

AREA_DATA *find_area(char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	if (!str_cmp(plith.name, name))
		return &plith;

	return NULL;
}

AREA_DATA *get_area_from_uid (long uid)
{
	if (plith.uid == uid) return &plith;
	return NULL;
}

CLASS_DATA *get_class_data(const char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	if (!str_cmp(paladin.name, name)) return &paladin;

	return NULL;
}

LIQUID *liquid_lookup(char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	return NULL;
}

MATERIAL *material_lookup(const char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	return NULL;
}

CHURCH_DATA *get_church_by_name(const char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	return NULL;
}

RACE_DATA *get_race_data(const char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	if (!str_cmp(human.name, name)) return &human;

	return NULL;
}

SECTOR_DATA *get_sector_data(char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	if (!str_cmp(sector_inside.name, name)) return &sector_inside;
	if (!str_cmp(sector_city.name, name)) return &sector_city;
	if (!str_cmp(sector_field.name, name)) return &sector_field;

	return NULL;
}

SKILL_DATA *get_skill_data(char *name)
{
	if (!utf8_isstrascii(name)) return NULL;

	return NULL;
}

WILDS_DATA *get_wilds_from_uid(AREA_DATA *area, long uid)
{
	return NULL;
}


//////////////////////////

// SENTIENCE variables
pVARIABLE variable_new(const char *name)
{
	pVARIABLE var = calloc(1, sizeof(VARIABLE));

	if (var)
	{
		var->name = strdup(name);
		var->type = VAR_UNKNOWN;
	}

	return var;
}

void variable_free(pVARIABLE var)
{
	switch(var->type)
	{
		case VAR_STRING:
			if (var->_.str) free(var->_.str);
			break;

		case VAR_LIST:
			list_destroy(var->_.list.list);
			break;

		case VAR_ARRAY:
			if (var->_.array.ptr) free(var->_.array.ptr);
			break;
		
		case VAR_FLAG_BANK:
			if (var->_.flagbank.bits) free(var->_.flagbank.bits);
			break;
	}

	if (var->name) free(var->name);
	free(var);
}

static void __free_variable(void *data)
{
	variable_free((pVARIABLE)data);
}

LLIST *nib_create_var_list()
{
	return list_createx(false, NULL, __free_variable);
}

#define __vp(t) \
	case VAR_##t: return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_##t );

#define __vps(t) \
	case VAR_##t: \
	case VAR_##t##_S: \
		return (type != NULL && type->type_class == NTC_PRIMARY && type->_.primary == NT_##t );

#define __vc(t) \
	case VAR_##t: return (type != NULL && type->type_class == NTC_##t );

#define __vcs(t) \
	case VAR_##t: \
	case VAR_##t##_S: \
		return (type != NULL && type->type_class == NTC_##t );

bool is_valid_variable_type(pVARIABLE var, NIB_TYPE *type)
{
	switch(var->type)
	{
	__vp(BOOLEAN)
	__vp(NUMBER)
	__vp(FLOAT)
	__vp(CHAR)
	__vps(STRING)
	__vp(WIDEVNUM)
	__vp(TIME)
	__vp(DICE)
	__vc(FLAG)
	__vc(FLAG_BANK)
	__vc(STAT)
	__vcs(LIST)
	__vcs(ARRAY)
	__vp(ACCOUNT)
	__vp(AFFECT)
	__vp(AREA)
	// __vp(CHANNEL)
	__vp(CLASS)
	__vp(DUNGEON)
	__vp(EXIT)
	__vp(INSTANCE)
	__vp(LIQUID)
	__vp(MAIL)
	__vp(MATERIAL)
	__vp(MISSION)
	__vp(MOBILE)
	__vp(NOTE)
	__vp(OBJECT)
	__vp(ORG)
	// __vp(QUEST)
	__vp(RACE)
	__vp(RANK)
	__vp(REPUTATION)
	__vp(ROOM)
	__vp(SECTOR)
	__vp(SHIP)
	__vp(SKILL)
	__vp(TOKEN)
	__vp(WILDS)
	// __vp(WORLD)
	}

	return false;
}

pVARIABLE variable_get(const char *name)
{
	ITERATOR it;
	pVARIABLE var;

	iterator_start(&it, nib_variables);
	while((var = (pVARIABLE)iterator_nextdata(&it)))
	{
		if (!str_cmp(var->name, name))
			break;
	}
	iterator_stop(&it);

	return var;
}

#define __var(v,t,f,n) \
pVARIABLE variable_new_##n (const char *name, t value) \
{ \
	pVARIABLE var = variable_new(name); \
 \
	if (var) \
	{ \
		var->type = VAR_##v; \
		var->_.f = value; \
	} \
 \
	return var; \
}

#define __vara(v,t,f,n,a) \
pVARIABLE variable_new_##n (const char *name, t value) \
{ \
	pVARIABLE var = variable_new(name); \
 \
	if (var) \
	{ \
		var->type = VAR_##v; \
		var->_.f = (a); \
	} \
 \
	return var; \
}

__var(NUMBER,long,num,number)
__var(FLOAT,double,flt,float)
__var(BOOLEAN,bool,b,bool)
__var(CHAR,utf8char_t,ch,char)
__var(STRING,char *,str,string_raw)
__vara(STRING,char *,str,string,strdup(value))
__var(STRING_S,char *,str,shared_string)

pVARIABLE variable_new_list_raw(const char *name, LLIST *list, int type, bool constant)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_LIST;
		var->_.list.list = list;
		var->_.list.type = type;
		var->_.list.constant = constant;
	}

	return var;
}

pVARIABLE variable_new_list(const char *name, LLIST *list, int type, bool constant)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_LIST;
		var->_.list.list = list_copy(list);
		var->_.list.type = type;
		var->_.list.constant = constant;
	}

	return var;
}

pVARIABLE variable_new_shared_list(const char *name, LLIST *list, int type, bool constant)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_LIST_S;
		var->_.list.list = list;
		var->_.list.type = type;
		var->_.list.constant = constant;
	}

	return var;
}

pVARIABLE variable_new_flag(const char *name, long number, struct flag_type *table, const char *table_name)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_FLAG;
		var->_.stat.number = number;
		var->_.stat.table = table;
		var->_.stat.table_name = table_name;
	}

	return var;
}


pVARIABLE variable_new_flagbank(const char *name, long *bits, const struct flag_type **bank)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_FLAG_BANK;
		var->_.flagbank.bank = bank;
		for(var->_.flagbank.banks = 0; bank[var->_.flagbank.banks]; var->_.flagbank.banks++);
		var->_.flagbank.bits = calloc(var->_.flagbank.banks, sizeof(long));
		if (!var->_.flagbank.bits)
		{
			variable_free(var);
			return NULL;
		}
	}

	return var;
}

pVARIABLE variable_new_stat(const char *name, long number, struct flag_type *table, const char *table_name)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_STAT;
		var->_.stat.number = number;
		var->_.stat.table = table;
		var->_.stat.table_name = table_name;
	}

	return var;
}

pVARIABLE variable_new_widevnum(const char *name, AREA_DATA *area, long vnum)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_WIDEVNUM;
		var->_.wnum.pArea = area;
		var->_.wnum.vnum = vnum;
	}

	return var;
}

__var(TIME,time_t,timestamp,time)

pVARIABLE variable_new_dice(const char *name, DICE_DATA *dice)
{
	pVARIABLE var = variable_new(name);

	if (var)
	{
		var->type = VAR_DICE;
		if (dice)
			var->_.dice = *dice;
		else
		{
			var->_.dice.number = 0;
			var->_.dice.size = 0;
			var->_.dice.bonus = 0;
			var->_.dice.last_roll = 0L;
		}
	}

	return var;
}

__var(ACCOUNT,ACCOUNT_DATA *,account,account)
__var(AFFECT,AFFECT_DATA *,affect,affect)
__var(AREA,AREA_DATA *,area,area)
// __var(CHANNEL,CHANNEL_DATA *,channel,channel)
__var(CLASS,CLASS_DATA *,clazz,class)
__var(DUNGEON,DUNGEON *,dungeon,dungeon)
__var(EXIT,EXIT_DATA *,ex,exit)
__var(INSTANCE,INSTANCE *,instance,instance)
__var(LIQUID,LIQUID *,liquid,liquid)
__var(MAIL,MAIL_DATA *,mail,mail)
__var(MATERIAL,MATERIAL *,material,material)
__var(MISSION,MISSION_DATA *,mission,mission)
__var(MOBILE,CHAR_DATA *,mobile,mobile)
__var(NOTE,NOTE_DATA *,note,note)
__var(OBJECT,OBJ_DATA *,object,object)
__var(ORG,CHURCH_DATA *,org,org)
// __var(QUEST,QUEST_DATA *,quest,quest)
__var(RACE,RACE_DATA *,race,race)
__var(RANK,REPUTATION_INDEX_RANK_DATA *,rank,rank)
__var(REPUTATION,REPUTATION_DATA *,reputation,reputation)
__var(ROOM,ROOM_INDEX_DATA *,room,room)
__var(SECTOR,SECTOR_DATA *,sector,sector)
__var(SHIP,SHIP_DATA *,ship,ship)
__var(SKILL,SKILL_DATA *,skill,skill)
__var(TOKEN,TOKEN_DATA *,token,token)
__var(WILDS,WILDS_DATA *,wilds,wilds)
// __var(WORLD,WORLD_DATA *,world,world)

void variable_get_string(pVARIABLE var, char *buf, int buf_len, int char_len)
{
	int len;
	switch(var->type)
	{
	case VAR_NUMBER:	len = snprintf(buf,buf_len,"%ld", var->_.num); break;
	case VAR_FLOAT:		len = snprintf(buf,buf_len,"%lf", var->_.flt); break;
	case VAR_BOOLEAN:	len = snprintf(buf,buf_len,"%s", var->_.b ? "true" : "false"); break;
	case VAR_CHAR:
			if (utf8_isprint(var->_.ch))
				len = snprintf(buf,buf_len,"'%s'", utf8_getbytes(var->_.ch));
			else
				len = snprintf(buf,buf_len,"0x%X", var->_.ch);
			break;

	case VAR_STRING:
	case VAR_STRING_S:
			if (var->_.str)
			{
				if ((utf8_strlen(var->_.str)) > (char_len - 2))
					len = snprintf(buf,buf_len,"\"%s...\"", utf8_getnchars(var->_.str,char_len-5));
				else
					len = snprintf(buf,buf_len,"\"%s\"",var->_.str);
			}
			else
				len = snprintf(buf,buf_len,"null");
			break;

	case VAR_WIDEVNUM:
		len = snprintf(buf,buf_len,"(%s[%ld]#%ld)",
			((var->_.wnum.pArea) ? var->_.wnum.pArea->name : "null"),
			((var->_.wnum.pArea) ? var->_.wnum.pArea->uid : 0L),
			var->_.wnum.vnum);
		break;

	case VAR_FLAG:
		if (var->_.stat.table)
			len = snprintf(buf,buf_len,"%s", nib_get_flag_string(var->_.stat.table,var->_.stat.number));
		else
			len = snprintf(buf,buf_len,"%08X", var->_.stat.number);
		break;

	case VAR_FLAG_BANK:
		if (var->_.flagbank.bank)
			len = snprintf(buf,buf_len,"%s", nib_get_flagbank_string(var->_.flagbank.bank,var->_.flagbank.bits));
		else
		{
			len = snprintf(buf,buf_len,"%08X", var->_.flagbank.bits[0]);
			for(int i = 1; i < var->_.flagbank.banks; i++)
			{
				len += snprintf(buf + len,buf_len - len,",%08X", var->_.flagbank.bits[i]);
			}
		}
		break;

	case VAR_STAT:
		if (var->_.stat.table)
			len = snprintf(buf,buf_len,"%s", nib_get_stat_string(var->_.stat.table,var->_.stat.number));
		else	// Should never happen?
			len = snprintf(buf,buf_len,"%ld", var->_.stat.number);
		break;

	case VAR_LIST:
	case VAR_LIST_S:
		{
			char *type;
			switch(var->_.list.type)
			{
			case VAR_NUMBER:	type = "int"; break;
			case VAR_FLOAT:		type = "float"; break;
			case VAR_BOOLEAN:	type = "boolean"; break;
			case VAR_CHAR:		type = "char"; break;
			case VAR_STRING:	type = "string"; break;
			case VAR_FLAG:		type = "flag"; break;
			case VAR_FLAG_BANK:	type = "flagbank"; break;
			case VAR_STAT:		type = "stat"; break;
			case VAR_WIDEVNUM:	type = "widevnum"; break;
			case VAR_ACCOUNT:	type = "account"; break;
			case VAR_AFFECT:	type = "affect"; break;
			case VAR_AREA:		type = "area"; break;
			case VAR_CHANNEL:	type = "channel"; break;
			case VAR_CLASS:		type = "class"; break;
			case VAR_DUNGEON:	type = "dungeon"; break;
			case VAR_EXIT:		type = "exit"; break;
			case VAR_INSTANCE:	type = "instance"; break;
			case VAR_LIQUID:	type = "liquid"; break;
			case VAR_MAIL:		type = "mail"; break;
			case VAR_MATERIAL:	type = "material"; break;
			case VAR_MISSION:	type = "mission"; break;
			case VAR_MOBILE:	type = "mobile"; break;
			case VAR_NOTE:		type = "note"; break;
			case VAR_OBJECT:	type = "object"; break;
			case VAR_ORG:		type = "org"; break;
			case VAR_QUEST:		type = "quest"; break;
			case VAR_RACE:		type = "race"; break;
			case VAR_RANK:		type = "rank"; break;
			case VAR_REPUTATION:type = "reputation"; break;
			case VAR_ROOM:		type = "room"; break;
			case VAR_SECTOR:	type = "sector"; break;
			case VAR_SHIP:		type = "ship"; break;
			case VAR_SKILL:		type = "skill"; break;
			case VAR_TOKEN:		type = "token"; break;
			case VAR_WILDS:		type = "wilds"; break;
			case VAR_WORLD:		type = "world"; break;
			default:			type = "???"; break;
			}

			len = snprintf(buf,buf_len,"list(%s%s)[%d]",
				(var->_.list.constant ? "constant " : ""),
				type,
				list_size(var->_.list.list));
			break;
		}

	case VAR_ARRAY:
	case VAR_ARRAY_S:
		{
			char *type;
			switch(var->_.array.type)
			{
			case VAR_NUMBER:	type = "int"; break;
			case VAR_FLOAT:		type = "float"; break;
			case VAR_BOOLEAN:	type = "boolean"; break;
			case VAR_CHAR:		type = "char"; break;
			case VAR_STRING:	type = "string"; break;
			case VAR_FLAG:		type = "flag"; break;
			case VAR_FLAG_BANK:	type = "flagbank"; break;
			case VAR_STAT:		type = "stat"; break;
			case VAR_WIDEVNUM:	type = "widevnum"; break;
			case VAR_ACCOUNT:	type = "account"; break;
			case VAR_AFFECT:	type = "affect"; break;
			case VAR_AREA:		type = "area"; break;
			case VAR_CHANNEL:	type = "channel"; break;
			case VAR_CLASS:		type = "class"; break;
			case VAR_DUNGEON:	type = "dungeon"; break;
			case VAR_EXIT:		type = "exit"; break;
			case VAR_INSTANCE:	type = "instance"; break;
			case VAR_LIQUID:	type = "liquid"; break;
			case VAR_MAIL:		type = "mail"; break;
			case VAR_MATERIAL:	type = "material"; break;
			case VAR_MISSION:	type = "mission"; break;
			case VAR_MOBILE:	type = "mobile"; break;
			case VAR_NOTE:		type = "note"; break;
			case VAR_OBJECT:	type = "object"; break;
			case VAR_ORG:		type = "org"; break;
			case VAR_QUEST:		type = "quest"; break;
			case VAR_RACE:		type = "race"; break;
			case VAR_RANK:		type = "rank"; break;
			case VAR_REPUTATION:type = "reputation"; break;
			case VAR_ROOM:		type = "room"; break;
			case VAR_SECTOR:	type = "sector"; break;
			case VAR_SHIP:		type = "ship"; break;
			case VAR_SKILL:		type = "skill"; break;
			case VAR_TOKEN:		type = "token"; break;
			case VAR_WILDS:		type = "wilds"; break;
			case VAR_WORLD:		type = "world"; break;
			default:			type = "???"; break;
			}

			len = snprintf(buf,buf_len,"array(%s%s[%d])", (var->_.array.constant?"constant ":""), type, var->_.array.length);
			break;
		}

	case VAR_ACCOUNT:
		if (var->_.account)
			len = snprintf(buf,buf_len,"%s", var->_.account->username);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_AFFECT:
		if (var->_.affect)
			len = snprintf(buf,buf_len,"%s", get_affect_name(var->_.affect));
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_AREA:
		if (var->_.area)
			len = snprintf(buf,buf_len,"%s(%ld)", var->_.area->name, var->_.area->uid);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	// case VAR_CHANNEL:
	case VAR_CLASS:
		if (var->_.clazz)
			len = snprintf(buf,buf_len,"%s", var->_.clazz->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_DUNGEON:
		if (var->_.dungeon && var->_.dungeon->index)
		{
			len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
				var->_.dungeon->index->name,
				var->_.dungeon->index->area->name,
				var->_.dungeon->index->area->uid,
				var->_.dungeon->index->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_EXIT:
		if (var->_.ex && var->_.ex->orig_door >= 0 && var->_.ex->orig_door < MAX_DIR)
			len = snprintf(buf,buf_len,"%s(%d)", dir_name[var->_.ex->orig_door], var->_.ex->orig_door);
		else
			len = snprintf(buf,buf_len,"null(%d)", var->_.ex->orig_door);
		break;

	case VAR_INSTANCE:
		if (var->_.instance && var->_.instance->blueprint)
		{
			len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
				var->_.instance->blueprint->name,
				var->_.instance->blueprint->area->name,
				var->_.instance->blueprint->area->uid,
				var->_.instance->blueprint->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_LIQUID:
		if (var->_.liquid)
			len = snprintf(buf,buf_len,"%s", var->_.liquid->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_MAIL:
		if (var->_.mail)
			len = snprintf(buf,buf_len,"<mailto:%s>", var->_.mail->recipient);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_MATERIAL:
		if (var->_.material)
			len = snprintf(buf,buf_len,"%s", var->_.material->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_MISSION:
		if (var->_.liquid)
			len = snprintf(buf,buf_len,"%ld", var->_.mission->timer);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_MOBILE:
		if (var->_.mobile)
		{
			if (var->_.mobile->pIndexData)
				len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
					var->_.mobile->short_descr,
					var->_.mobile->pIndexData->area->name,
					var->_.mobile->pIndexData->area->uid,
					var->_.mobile->pIndexData->vnum);
			else
				len = snprintf(buf,buf_len,"%s(player)", var->_.mobile->short_descr);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_NOTE:
		if (var->_.note)
			len = snprintf(buf,buf_len,"<note:%s>", var->_.note->to_list);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_OBJECT:
		if (var->_.object && var->_.object->pIndexData)
		{
			len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
				var->_.object->short_descr,
				var->_.object->pIndexData->area->name,
				var->_.object->pIndexData->area->uid,
				var->_.object->pIndexData->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_ORG:
		if (var->_.org)
			len = snprintf(buf,buf_len,"%s", var->_.org->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;
	// case VAR_QUEST:
	case VAR_RACE:
		if (var->_.race)
			len = snprintf(buf,buf_len,"%s", var->_.race->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_RANK:
		if (var->_.rank)
			len = snprintf(buf,buf_len,"%s", var->_.rank->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_REPUTATION:
		if (var->_.reputation && var->_.reputation->pIndexData)
			len = snprintf(buf,buf_len,"%s", var->_.reputation->pIndexData->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;
	case VAR_ROOM:
		if (var->_.room)
		{
			if (var->_.room->source)
				len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld:%lu:%lu)",
					var->_.room->name,
					var->_.room->source->area->name,
					var->_.room->source->area->uid,
					var->_.room->source->vnum,
					var->_.room->id[0],
					var->_.room->id[1]);
			else
				len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
					var->_.room->name,
					var->_.room->area->name,
					var->_.room->area->uid,
					var->_.room->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_SECTOR:
		if (var->_.sector)
			len = snprintf(buf,buf_len,"%s", var->_.sector->name);
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_SHIP:
		if (var->_.ship && var->_.ship->index)
		{
			len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
				var->_.ship->index->name,
				var->_.ship->index->area->name,
				var->_.ship->index->area->uid,
				var->_.ship->index->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;

	case VAR_TOKEN:
		if (var->_.token && var->_.token->pIndexData)
		{
			len = snprintf(buf,buf_len,"%s(%s[%ld]#%ld)",
				var->_.token->name,
				var->_.token->pIndexData->area->name,
				var->_.token->pIndexData->area->uid,
				var->_.token->pIndexData->vnum);
		}
		else
			len = snprintf(buf,buf_len,"null");
		break;
	default:
		len = snprintf(buf,buf_len,"???");
		break;
	}
	buf[len] = '\0';
}

const char *variable_get_typename(pVARIABLE var)
{
	switch(var->type)
	{
	case VAR_NUMBER:		return "int";
	case VAR_FLOAT:			return "float";
	case VAR_BOOLEAN:		return "boolean";
	case VAR_CHAR:			return "char";
	case VAR_STRING:		return "string";
	case VAR_STRING_S:		return "string_s";
	case VAR_WIDEVNUM:		return "widevnum";
	case VAR_FLAG:
		if (var->_.stat.table)
		{
			static char buf[101];
			int len = snprintf(buf, sizeof(buf)-1, "flag(%s)", var->_.stat.table_name);
			buf[len] = 0;
			return buf;
		}
		else
			return "flag";

	case VAR_FLAG_BANK:
		if (var->_.flagbank.bank)
		{
			static char buf[101];
			int len = snprintf(buf, sizeof(buf)-1, "flagbank(%s)", nib_get_flag_bank_name(var->_.flagbank.bank));
			buf[len] = 0;
			return buf;
		}
		else
			return "flagbank";

	case VAR_STAT:
		if (var->_.stat.table)
		{
			static char buf[101];
			int len = snprintf(buf, sizeof(buf)-1, "stat(%s)", var->_.stat.table_name);
			buf[len] = 0;
			return buf;
		}
		else
			return "stat(???)";

	case VAR_LIST:
		if (var->_.list.constant)
			switch(var->_.list.type)
			{
			case VAR_NUMBER:	return "list(constant int)";
			case VAR_FLOAT:		return "list(constant float)";
			case VAR_BOOLEAN:	return "list(constant boolean)";
			case VAR_CHAR:		return "list(constant char)";
			case VAR_STRING:	return "list(constant string)";
			case VAR_FLAG:		return "list(constant flag)";
			case VAR_FLAG_BANK:	return "list(constant flagbank)";
			case VAR_STAT:		return "list(constant stat)";
			case VAR_WIDEVNUM:	return "list(constant widevnum)";
			case VAR_ACCOUNT:	return "list(constant account)";
			case VAR_AFFECT:	return "list(constant affect)";
			case VAR_AREA:		return "list(constant area)";
			case VAR_CHANNEL:	return "list(constant channel)";
			case VAR_CLASS:		return "list(constant class)";
			case VAR_DUNGEON:	return "list(constant dungeon)";
			case VAR_EXIT:		return "list(constant exit)";
			case VAR_INSTANCE:	return "list(constant instance)";
			case VAR_LIQUID:	return "list(constant liquid)";
			case VAR_MAIL:		return "list(constant mail)";
			case VAR_MATERIAL:	return "list(constant material)";
			case VAR_MISSION:	return "list(constant mission)";
			case VAR_MOBILE:	return "list(constant mobile)";
			case VAR_NOTE:		return "list(constant note)";
			case VAR_OBJECT:	return "list(constant object)";
			case VAR_ORG:		return "list(constant org)";
			case VAR_QUEST:		return "list(constant quest)";
			case VAR_RACE:		return "list(constant race)";
			case VAR_RANK:		return "list(constant rank)";
			case VAR_REPUTATION:return "list(constant reputation)";
			case VAR_ROOM:		return "list(constant room)";
			case VAR_SECTOR:	return "list(constant sector)";
			case VAR_SHIP:		return "list(constant ship)";
			case VAR_SKILL:		return "list(constant skill)";
			case VAR_TOKEN:		return "list(constant token)";
			case VAR_WILDS:		return "list(constant wilds)";
			case VAR_WORLD:		return "list(constant world)";
			default:			return "list(constant ???)";
			}
		else
			switch(var->_.list.type)
			{
			case VAR_NUMBER:	return "list(int)";
			case VAR_FLOAT:		return "list(float)";
			case VAR_BOOLEAN:	return "list(boolean)";
			case VAR_CHAR:		return "list(char)";
			case VAR_STRING:	return "list(string)";
			case VAR_FLAG:		return "list(flag)";
			case VAR_FLAG_BANK:	return "list(flagbank)";
			case VAR_STAT:		return "list(stat)";
			case VAR_WIDEVNUM:	return "list(widevnum)";
			case VAR_ACCOUNT:	return "list(account)";
			case VAR_AFFECT:	return "list(affect)";
			case VAR_AREA:		return "list(area)";
			case VAR_CHANNEL:	return "list(channel)";
			case VAR_CLASS:		return "list(class)";
			case VAR_DUNGEON:	return "list(dungeon)";
			case VAR_EXIT:		return "list(exit)";
			case VAR_INSTANCE:	return "list(instance)";
			case VAR_LIQUID:	return "list(liquid)";
			case VAR_MAIL:		return "list(mail)";
			case VAR_MATERIAL:	return "list(material)";
			case VAR_MISSION:	return "list(mission)";
			case VAR_MOBILE:	return "list(mobile)";
			case VAR_NOTE:		return "list(note)";
			case VAR_OBJECT:	return "list(object)";
			case VAR_ORG:		return "list(org)";
			case VAR_QUEST:		return "list(quest)";
			case VAR_RACE:		return "list(race)";
			case VAR_RANK:		return "list(rank)";
			case VAR_REPUTATION:return "list(reputation)";
			case VAR_ROOM:		return "list(room)";
			case VAR_SECTOR:	return "list(sector)";
			case VAR_SHIP:		return "list(ship)";
			case VAR_SKILL:		return "list(skill)";
			case VAR_TOKEN:		return "list(token)";
			case VAR_WILDS:		return "list(wilds)";
			case VAR_WORLD:		return "list(world)";
			default:			return "list(???)";
			}

	case VAR_LIST_S:
		if (var->_.list.constant)
			switch(var->_.list.type)
			{
			case VAR_NUMBER:	return "list_s(constant int)";
			case VAR_FLOAT:		return "list_s(constant float)";
			case VAR_BOOLEAN:	return "list_s(constant boolean)";
			case VAR_CHAR:		return "list_s(constant char)";
			case VAR_STRING:	return "list_s(constant string)";
			case VAR_FLAG:		return "list_s(constant flag)";
			case VAR_FLAG_BANK:	return "list_s(constant flagbank)";
			case VAR_STAT:		return "list_s(constant stat)";
			case VAR_WIDEVNUM:	return "list_s(constant widevnum)";
			case VAR_ACCOUNT:	return "list_s(constant account)";
			case VAR_AFFECT:	return "list_s(constant affect)";
			case VAR_AREA:		return "list_s(constant area)";
			case VAR_CHANNEL:	return "list_s(constant channel)";
			case VAR_CLASS:		return "list_s(constant class)";
			case VAR_DUNGEON:	return "list_s(constant dungeon)";
			case VAR_EXIT:		return "list_s(constant exit)";
			case VAR_INSTANCE:	return "list_s(constant instance)";
			case VAR_LIQUID:	return "list_s(constant liquid)";
			case VAR_MAIL:		return "list_s(constant mail)";
			case VAR_MATERIAL:	return "list_s(constant material)";
			case VAR_MISSION:	return "list_s(constant mission)";
			case VAR_MOBILE:	return "list_s(constant mobile)";
			case VAR_NOTE:		return "list_s(constant note)";
			case VAR_OBJECT:	return "list_s(constant object)";
			case VAR_ORG:		return "list_s(constant org)";
			case VAR_QUEST:		return "list_s(constant quest)";
			case VAR_RACE:		return "list_s(constant race)";
			case VAR_RANK:		return "list_s(constant rank)";
			case VAR_REPUTATION:return "list_s(constant reputation)";
			case VAR_ROOM:		return "list_s(constant room)";
			case VAR_SECTOR:	return "list_s(constant sector)";
			case VAR_SHIP:		return "list_s(constant ship)";
			case VAR_SKILL:		return "list_s(constant skill)";
			case VAR_TOKEN:		return "list_s(constant token)";
			case VAR_WILDS:		return "list_s(constant wilds)";
			case VAR_WORLD:		return "list_s(constant world)";
			default:			return "list_s(constant ???)";
			}
		else
			switch(var->_.list.type)
			{
			case VAR_NUMBER:	return "list_s(int)";
			case VAR_FLOAT:		return "list_s(float)";
			case VAR_BOOLEAN:	return "list_s(boolean)";
			case VAR_CHAR:		return "list_s(char)";
			case VAR_STRING:	return "list_s(string)";
			case VAR_FLAG:		return "list_s(flag)";
			case VAR_FLAG_BANK:	return "list_s(flagbank)";
			case VAR_STAT:		return "list_s(stat)";
			case VAR_WIDEVNUM:	return "list_s(widevnum)";
			case VAR_ACCOUNT:	return "list_s(account)";
			case VAR_AFFECT:	return "list_s(affect)";
			case VAR_AREA:		return "list_s(area)";
			case VAR_CHANNEL:	return "list_s(channel)";
			case VAR_CLASS:		return "list_s(class)";
			case VAR_DUNGEON:	return "list_s(dungeon)";
			case VAR_EXIT:		return "list_s(exit)";
			case VAR_INSTANCE:	return "list_s(instance)";
			case VAR_LIQUID:	return "list_s(liquid)";
			case VAR_MAIL:		return "list_s(mail)";
			case VAR_MATERIAL:	return "list_s(material)";
			case VAR_MISSION:	return "list_s(mission)";
			case VAR_MOBILE:	return "list_s(mobile)";
			case VAR_NOTE:		return "list_s(note)";
			case VAR_OBJECT:	return "list_s(object)";
			case VAR_ORG:		return "list_s(org)";
			case VAR_QUEST:		return "list_s(quest)";
			case VAR_RACE:		return "list_s(race)";
			case VAR_RANK:		return "list_s(rank)";
			case VAR_REPUTATION:return "list_s(reputation)";
			case VAR_ROOM:		return "list_s(room)";
			case VAR_SECTOR:	return "list_s(sector)";
			case VAR_SHIP:		return "list_s(ship)";
			case VAR_SKILL:		return "list_s(skill)";
			case VAR_TOKEN:		return "list_s(token)";
			case VAR_WILDS:		return "list_s(wilds)";
			case VAR_WORLD:		return "list_s(world)";
			default:			return "list_s(???)";
			}

	case VAR_ARRAY:
		{
			static char buf[101];
			char *type;
			switch(var->_.array.type)
			{
			case VAR_NUMBER:	type = "int"; break;
			case VAR_FLOAT:		type = "float"; break;
			case VAR_BOOLEAN:	type = "boolean"; break;
			case VAR_CHAR:		type = "char"; break;
			case VAR_STRING:	type = "string"; break;
			case VAR_FLAG:		type = "flag"; break;
			case VAR_FLAG_BANK:	type = "flagbank"; break;
			case VAR_STAT:		type = "stat"; break;
			case VAR_WIDEVNUM:	type = "widevnum"; break;
			case VAR_ACCOUNT:	type = "account"; break;
			case VAR_AFFECT:	type = "affect"; break;
			case VAR_AREA:		type = "area"; break;
			case VAR_CHANNEL:	type = "channel"; break;
			case VAR_CLASS:		type = "class"; break;
			case VAR_DUNGEON:	type = "dungeon"; break;
			case VAR_EXIT:		type = "exit"; break;
			case VAR_INSTANCE:	type = "instance"; break;
			case VAR_LIQUID:	type = "liquid"; break;
			case VAR_MAIL:		type = "mail"; break;
			case VAR_MATERIAL:	type = "material"; break;
			case VAR_MISSION:	type = "mission"; break;
			case VAR_MOBILE:	type = "mobile"; break;
			case VAR_NOTE:		type = "note"; break;
			case VAR_OBJECT:	type = "object"; break;
			case VAR_ORG:		type = "org"; break;
			case VAR_QUEST:		type = "quest"; break;
			case VAR_RACE:		type = "race"; break;
			case VAR_RANK:		type = "rank"; break;
			case VAR_REPUTATION:type = "reputation"; break;
			case VAR_ROOM:		type = "room"; break;
			case VAR_SECTOR:	type = "sector"; break;
			case VAR_SHIP:		type = "ship"; break;
			case VAR_SKILL:		type = "skill"; break;
			case VAR_TOKEN:		type = "token"; break;
			case VAR_WILDS:		type = "wilds"; break;
			case VAR_WORLD:		type = "world"; break;
			default:			type = "???"; break;
			}

			int len = snprintf(buf, sizeof(buf)-1, "array(%s%s[%ld])", (var->_.array.constant?"constant ":""), type, var->_.array.length);
			buf[len] = 0;
			return buf;
		}

	case VAR_ARRAY_S:
		{
			static char buf[101];
			char *type;
			switch(var->_.array.type)
			{
			case VAR_NUMBER:	type = "int"; break;
			case VAR_FLOAT:		type = "float"; break;
			case VAR_BOOLEAN:	type = "boolean"; break;
			case VAR_CHAR:		type = "char"; break;
			case VAR_STRING:	type = "string"; break;
			case VAR_FLAG:		type = "flag"; break;
			case VAR_FLAG_BANK:	type = "flagbank"; break;
			case VAR_STAT:		type = "stat"; break;
			case VAR_WIDEVNUM:	type = "widevnum"; break;
			case VAR_ACCOUNT:	type = "account"; break;
			case VAR_AFFECT:	type = "affect"; break;
			case VAR_AREA:		type = "area"; break;
			case VAR_CHANNEL:	type = "channel"; break;
			case VAR_CLASS:		type = "class"; break;
			case VAR_DUNGEON:	type = "dungeon"; break;
			case VAR_EXIT:		type = "exit"; break;
			case VAR_INSTANCE:	type = "instance"; break;
			case VAR_LIQUID:	type = "liquid"; break;
			case VAR_MAIL:		type = "mail"; break;
			case VAR_MATERIAL:	type = "material"; break;
			case VAR_MISSION:	type = "mission"; break;
			case VAR_MOBILE:	type = "mobile"; break;
			case VAR_NOTE:		type = "note"; break;
			case VAR_OBJECT:	type = "object"; break;
			case VAR_ORG:		type = "org"; break;
			case VAR_QUEST:		type = "quest"; break;
			case VAR_RACE:		type = "race"; break;
			case VAR_RANK:		type = "rank"; break;
			case VAR_REPUTATION:type = "reputation"; break;
			case VAR_ROOM:		type = "room"; break;
			case VAR_SECTOR:	type = "sector"; break;
			case VAR_SHIP:		type = "ship"; break;
			case VAR_SKILL:		type = "skill"; break;
			case VAR_TOKEN:		type = "token"; break;
			case VAR_WILDS:		type = "wilds"; break;
			case VAR_WORLD:		type = "world"; break;
			default:			type = "???"; break;
			}

			int len = snprintf(buf, sizeof(buf)-1, "array_s(%s%s[%ld])", (var->_.array.constant?"constant ":""), type, var->_.array.length);
			buf[len] = 0;
			return buf;
		}

	case VAR_ACCOUNT:	return "account";
	case VAR_AFFECT:	return "affect";
	case VAR_AREA:		return "area";
	case VAR_CHANNEL:	return "channel";
	case VAR_CLASS:		return "class";
	case VAR_DUNGEON:	return "dungeon";
	case VAR_EXIT:		return "exit";
	case VAR_INSTANCE:	return "instance";
	case VAR_LIQUID:	return "liquid";
	case VAR_MAIL:		return "mail";
	case VAR_MATERIAL:	return "material";
	case VAR_MISSION:	return "mission";
	case VAR_MOBILE:	return "mobile";
	case VAR_NOTE:		return "note";
	case VAR_OBJECT:	return "object";
	case VAR_ORG:		return "org";
	case VAR_QUEST:		return "quest";
	case VAR_RACE:		return "race";
	case VAR_RANK:		return "rank";
	case VAR_REPUTATION:return "reputation";
	case VAR_ROOM:		return "room";
	case VAR_SECTOR:	return "sector";
	case VAR_SHIP:		return "ship";
	case VAR_SKILL:		return "skill";
	case VAR_TOKEN:		return "token";
	case VAR_WILDS:		return "wilds";
	case VAR_WORLD:		return "world";
	default:
		return "???";
	}	
}

bool variable_init()
{
	nib_variables = nib_create_var_list();
	if (!list_isvalid(nib_variables)) return false;

	pVARIABLE var;

	var = variable_new_area("plith", &plith);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	var = variable_new_number("vnum", 2);
	list_appendlink(nib_variables, var);

	var = variable_new_float("bar", -INFINITY);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	var = variable_new_widevnum("wnum", &plith, 100L);
	var->readonly = true;
	list_appendlink(nib_variables, var);

	var = variable_new_string("name", "");
	list_appendlink(nib_variables, var);

	var = variable_new_number("iterations", 0);
	list_appendlink(nib_variables, var);

	var = variable_new_char("pressed", utf8_getchar("世"));
	list_appendlink(nib_variables, var);

	var = variable_new_object("sword", &sword);
	list_appendlink(nib_variables, var);

	var = variable_new_class("paladin", &paladin);
	list_appendlink(nib_variables, var);

	return true;
}

void variable_cleanup()
{
	list_destroy(nib_variables);
}

