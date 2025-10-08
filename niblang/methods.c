#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "../merc.h"
#include "../wilds.h"
#include "niblang.h"

#define YYSTYPE NIBMETHODSTYPE
#define YYLTYPE NIBMETHODLTYPE
#include "yacc/method_parser.h"
#include "yacc/method_lexer.h"

NIB_SCRIPT_STACK_TYPE convert_to_stype(NIB_TYPE *type, bool constant);

// Create function pointers for method names
LLIST *nib_functions = NULL;			// Context-less methods (aka functions)
LLIST *nib_methods_int = NULL;
LLIST *nib_methods_float = NULL;
LLIST *nib_methods_boolean = NULL;
LLIST *nib_methods_char = NULL;
LLIST *nib_methods_string = NULL;
LLIST *nib_methods_map = NULL;
LLIST *nib_methods_widevnum = NULL;
LLIST *nib_methods_time = NULL;
LLIST *nib_methods_list = NULL;
LLIST *nib_methods_array = NULL;
LLIST *nib_methods_flag = NULL;
LLIST *nib_methods_stat = NULL;
LLIST *nib_methods_account = NULL;
LLIST *nib_methods_affect = NULL;
LLIST *nib_methods_area = NULL;
LLIST *nib_methods_channel = NULL;
LLIST *nib_methods_class = NULL;
LLIST *nib_methods_dungeon = NULL;
LLIST *nib_methods_exit = NULL;
LLIST *nib_methods_instance = NULL;
LLIST *nib_methods_liquid = NULL;
LLIST *nib_methods_mail = NULL;
LLIST *nib_methods_material = NULL;
LLIST *nib_methods_mission = NULL;
LLIST *nib_methods_mobile = NULL;
LLIST *nib_methods_note = NULL;
LLIST *nib_methods_object = NULL;
LLIST *nib_methods_org = NULL;
LLIST *nib_methods_quest = NULL;
LLIST *nib_methods_race = NULL;
LLIST *nib_methods_rank = NULL;
LLIST *nib_methods_reputation = NULL;
LLIST *nib_methods_room = NULL;
LLIST *nib_methods_sector = NULL;
LLIST *nib_methods_ship = NULL;
LLIST *nib_methods_skill = NULL;
LLIST *nib_methods_token = NULL;
LLIST *nib_methods_wilds = NULL;
LLIST *nib_methods_world = NULL;

LLIST *nib_fields_int = NULL;
LLIST *nib_fields_float = NULL;
LLIST *nib_fields_boolean = NULL;
LLIST *nib_fields_char = NULL;
LLIST *nib_fields_string = NULL;
LLIST *nib_fields_map = NULL;
LLIST *nib_fields_widevnum = NULL;
LLIST *nib_fields_time = NULL;
LLIST *nib_fields_account = NULL;
LLIST *nib_fields_affect = NULL;
LLIST *nib_fields_area = NULL;
LLIST *nib_fields_channel = NULL;
LLIST *nib_fields_class = NULL;
LLIST *nib_fields_dungeon = NULL;
LLIST *nib_fields_exit = NULL;
LLIST *nib_fields_instance = NULL;
LLIST *nib_fields_liquid = NULL;
LLIST *nib_fields_mail = NULL;
LLIST *nib_fields_material = NULL;
LLIST *nib_fields_mission = NULL;
LLIST *nib_fields_mobile = NULL;
LLIST *nib_fields_note = NULL;
LLIST *nib_fields_object = NULL;
LLIST *nib_fields_org = NULL;
LLIST *nib_fields_quest = NULL;
LLIST *nib_fields_race = NULL;
LLIST *nib_fields_rank = NULL;
LLIST *nib_fields_reputation = NULL;
LLIST *nib_fields_room = NULL;
LLIST *nib_fields_sector = NULL;
LLIST *nib_fields_ship = NULL;
LLIST *nib_fields_skill = NULL;
LLIST *nib_fields_token = NULL;
LLIST *nib_fields_wilds = NULL;
LLIST *nib_fields_world = NULL;
LLIST *nib_fields_list = NULL;
LLIST *nib_fields_array = NULL;
LLIST *nib_fields_flag = NULL;
LLIST *nib_fields_stat = NULL;

static WNUM __static_wnum;
static ACCOUNT_DATA __static_account;
static AFFECT_DATA __static_affect;
static AREA_DATA __static_area;
static CLASS_DATA __static_class;
// static CHANNEL_DATA __static_channel;		// Add when CHANNEL update is done
static DUNGEON __static_dungeon;
static EXIT_DATA __static_exit;
static INSTANCE __static_instance;
static LIQUID __static_liquid;
static MAIL_DATA __static_mail;
static MATERIAL __static_material;
static MISSION_DATA __static_mission;
static CHAR_DATA __static_mobile;
static NOTE_DATA __static_note;
static OBJ_DATA __static_object;
static CHURCH_DATA __static_org;
static RACE_DATA __static_race;
static REPUTATION_INDEX_RANK_DATA __static_rank;
static REPUTATION_DATA __static_reputation;
static ROOM_INDEX_DATA __static_room;
static SECTOR_DATA __static_sector;
static SHIP_DATA __static_ship;
static SKILL_DATA __static_skill;
static TOKEN_DATA __static_token;
static WILDS_DATA __static_wilds;
// static WORLD_DATA __static_world;		// Add when WORLDS are done

#define ADDR(x)				((void *)&(x))
#define GET_OFFSET(v,f)		(size_t)(ADDR((v).f) - ADDR(v))

struct nib_field_offset_type
{
	NIB_TYPE_CLASS type_class;
	NIB_PRIMARY_TYPE primary;		// NT_UNKNOWN for non-primary
	char *field;
	size_t offset;
	size_t size;
	bool lvalue;			// Whether this can be an lvalue
};

#define STRIFY(v)	#v

#define NFO(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), sizeof(s), true }

#define NFOS(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), (s), true }

#define NFOR(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), sizeof(s), false }

#define NFORS(c,p,f,v,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), (s), false }

#define NFORA(c,p,f,v,a,s) \
	{ NTC_##c, NT_##p, STRIFY(p ## _ ## f), GET_OFFSET(v,f), (a) * sizeof(s), false }

#define NFOEND		{ NTC_VOID, NT_UNKNOWN, NULL, 0 }

static struct nib_field_offset_type __field_offsets[] =
{
	// Account
	NFOR(PRIMARY,ACCOUNT,username,__static_account,char *),
	NFOR(PRIMARY,ACCOUNT,failed_attempts,__static_account,int),
	NFOR(PRIMARY,ACCOUNT,last_failed_attempt,__static_account,time_t),
	NFOR(PRIMARY,ACCOUNT,email,__static_account,char *),
	NFOR(PRIMARY,ACCOUNT,email_verified,__static_account,bool),
	NFOR(PRIMARY,ACCOUNT,character_count,__static_account,int),
	NFOR(PRIMARY,ACCOUNT,character_limit,__static_account,int),
	NFO(PRIMARY,ACCOUNT,acct_flags,__static_account,long),
	NFOR(PRIMARY,ACCOUNT,creation_date,__static_account,time_t),
	NFOR(PRIMARY,ACCOUNT,last_login,__static_account,time_t),
	NFOR(PRIMARY,ACCOUNT,staff_account,__static_account,bool),
	NFOR(PRIMARY,ACCOUNT,lvault,__static_account,LLIST *),
	NFO(PRIMARY,ACCOUNT,vault_rent,__static_account,time_t),

	// Affect
	NFOR(PRIMARY,AFFECT,group,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,where,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,skill,__static_affect,SKILL_DATA *),
	NFO(PRIMARY,AFFECT,token,__static_affect,TOKEN_DATA *),
	NFOR(PRIMARY,AFFECT,level,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,duration,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,location,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,modifier,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,bitvector,__static_affect,long),
	NFOR(PRIMARY,AFFECT,bitvector2,__static_affect,long),
	NFOR(PRIMARY,AFFECT,random,__static_affect,int16_t),
	NFOR(PRIMARY,AFFECT,custom_name,__static_affect,char *),
	NFOR(PRIMARY,AFFECT,slot,__static_affect,int16_t),

	// Area
	NFO(PRIMARY,AREA,uid,__static_area,long),
	NFO(PRIMARY,AREA,name,__static_area,char *),
	NFO(PRIMARY,AREA,description,__static_area,char *),
	NFO(PRIMARY,AREA,area_flags,__static_area,long),
	NFO(PRIMARY,AREA,room_list,__static_area,LLIST *),
	NFO(PRIMARY,AREA,age,__static_area,int16_t),
	NFOR(PRIMARY,AREA,nplayer,__static_area,int16_t),
	NFOR(PRIMARY,AREA,security,__static_area,int),
	NFOR(PRIMARY,AREA,open,__static_area,bool),
	// Default Region in AREA
	NFOR(PRIMARY,AREA,region.uid,__static_area,long),
	NFOR(PRIMARY,AREA,region.name,__static_area,char *),
	NFOR(PRIMARY,AREA,region.description,__static_area,char *),
	NFOR(PRIMARY,AREA,region.area_who,__static_area,int),
	NFOR(PRIMARY,AREA,region.post_office,__static_area,long),
	NFOR(PRIMARY,AREA,region.flags,__static_area,long),
	NFOR(PRIMARY,AREA,region.players,__static_area,LLIST *),
	NFOR(PRIMARY,AREA,region.rooms,__static_area,LLIST *),
	// Method: region.recall
	NFOR(PRIMARY,AREA,region.place_flags,__static_area,long),
	NFOR(PRIMARY,AREA,region.savage_level,__static_area,int),
	NFOR(PRIMARY,AREA,region.land_x,__static_area,int),
	NFOR(PRIMARY,AREA,region.land_y,__static_area,int),
	// Method: region.airship
	NFOR(PRIMARY,AREA,rs_repop,__static_area,int),
	NFO(PRIMARY,AREA,repop,__static_area,int),

	// Channel

	// Class
	NFOR(PRIMARY,CLASS,name,__static_class,char *),
	NFOR(PRIMARY,CLASS,description,__static_class,char *),
	// Method: string display(stat(sex))
	// Method: string who(stat(sex))
	NFOR(PRIMARY,CLASS,uid,__static_class,int),
	NFOR(PRIMARY,CLASS,type,__static_class,int),
	NFOR(PRIMARY,CLASS,flags,__static_class,long),
	NFOR(PRIMARY,CLASS,groups,__static_class,LLIST *),
	NFOR(PRIMARY,CLASS,primary_stat,__static_class,int16_t),
	NFOR(PRIMARY,CLASS,max_level,__static_class,int16_t),

	// Dungeon
	NFOR(PRIMARY,DUNGEON,floors,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,special_rooms,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,special_exits,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,entry_room,__static_dungeon,ROOM_INDEX_DATA *),
	NFOR(PRIMARY,DUNGEON,exit_room,__static_dungeon,ROOM_INDEX_DATA *),
	NFOR(PRIMARY,DUNGEON,flags,__static_dungeon,long),
	NFOR(PRIMARY,DUNGEON,player_owners,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,players,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,mobiles,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,objects,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,bosses,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,rooms,__static_dungeon,LLIST *),
	NFOR(PRIMARY,DUNGEON,age,__static_dungeon,int),
	NFOR(PRIMARY,DUNGEON,idle_timer,__static_dungeon,int),

	// Exit
	NFO(PRIMARY,EXIT,keyword,__static_exit,char *),
	NFO(PRIMARY,EXIT,short_desc,__static_exit,char *),
	NFO(PRIMARY,EXIT,long_desc,__static_exit,char *),
	NFO(PRIMARY,EXIT,u1.to_room,__static_exit,ROOM_INDEX_DATA *),
	NFO(PRIMARY,EXIT,from_room,__static_exit,ROOM_INDEX_DATA *),
	NFO(PRIMARY,EXIT,exit_info,__static_exit,long),
	NFO(PRIMARY,EXIT,door.strength,__static_exit,int16_t),
	NFO(PRIMARY,EXIT,door.material,__static_exit,MATERIAL *),
	NFO(PRIMARY,EXIT,door.lock.pick_chance,__static_exit,int),
	NFO(PRIMARY,EXIT,door.lock.flags,__static_exit,long),
	NFO(PRIMARY,EXIT,door.lock.key_wnum,__static_exit,WNUM),
	NFOR(PRIMARY,EXIT,door.lock.special_keys,__static_exit,LLIST *),

	// Instance

	// Liquid
	NFOR(PRIMARY,LIQUID,uid,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,name,__static_liquid,char *),
	NFOR(PRIMARY,LIQUID,color,__static_liquid,char *),
	NFOR(PRIMARY,LIQUID,flammable,__static_liquid,bool),
	NFOR(PRIMARY,LIQUID,proof,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,full,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,hunger,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,thirst,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,fuel_unit,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,fuel_duration,__static_liquid,int16_t),
	NFOR(PRIMARY,LIQUID,max_mana,__static_liquid,int16_t),

	// Mail
	NFOR(PRIMARY,MAIL,lobjects,__static_mail,LLIST *),
	NFOR(PRIMARY,MAIL,sender,__static_mail,char *),
	NFOR(PRIMARY,MAIL,recipient,__static_mail,char *),
	NFOR(PRIMARY,MAIL,sent_date,__static_mail,time_t),
	NFOR(PRIMARY,MAIL,expire_date,__static_mail,time_t),
	NFOR(PRIMARY,MAIL,deliver_date,__static_mail,time_t),
	NFOR(PRIMARY,MAIL,message,__static_mail,char *),
	NFOR(PRIMARY,MAIL,picked_up,__static_mail,bool),
	NFOR(PRIMARY,MAIL,scripted,__static_mail,bool),
	NFOR(PRIMARY,MAIL,return_service,__static_mail,bool),
	NFOR(PRIMARY,MAIL,timestamp_expiration,__static_mail,bool),

	// Material
	NFOR(PRIMARY,MATERIAL,name,__static_material,char *),
	NFOR(PRIMARY,MATERIAL,material_class,__static_material,int),
	NFOR(PRIMARY,MATERIAL,flags,__static_material,long),
	NFOR(PRIMARY,MATERIAL,flammable,__static_material,int16_t),
	NFOR(PRIMARY,MATERIAL,corrodibility,__static_material,int16_t),
	NFOR(PRIMARY,MATERIAL,fragility,__static_material,int),
	NFOR(PRIMARY,MATERIAL,strength,__static_material,int),
	NFOR(PRIMARY,MATERIAL,value,__static_material,int),
	NFOR(PRIMARY,MATERIAL,corroded,__static_material,MATERIAL *),
	NFOR(PRIMARY,MATERIAL,burned,__static_material,MATERIAL *),

	// Mission

	// Mobile
	NFO(PRIMARY,MOBILE,name,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,short_descr,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,long_descr,__static_mobile,char *),
	NFO(PRIMARY,MOBILE,description,__static_mobile,char *),
	NFOR(PRIMARY,MOBILE,lcarrying,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,lworn,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,llocker,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,lstache,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,laffected,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,ltokens,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,in_room,__static_mobile,ROOM_INDEX_DATA *),
	NFO(PRIMARY,MOBILE,home_room,__static_mobile,ROOM_INDEX_DATA *),
	NFOR(PRIMARY,MOBILE,in_wilds,__static_mobile,WILDS_DATA *),
	NFOR(PRIMARY,MOBILE,at_wilds_x,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,at_wilds_y,__static_mobile,int),
	NFORA(PRIMARY,MOBILE,id,__static_mobile,2,unsigned long),
	NFOR(PRIMARY,MOBILE,num_grouped,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,sex,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,race,__static_mobile,RACE_DATA *),
	NFO(PRIMARY,MOBILE,timer,__static_mobile,int),
	NFO(PRIMARY,MOBILE,wait,__static_mobile,int),
	NFO(PRIMARY,MOBILE,daze,__static_mobile,int),
	NFO(PRIMARY,MOBILE,cast,__static_mobile,int),
	NFO(PRIMARY,MOBILE,panic,__static_mobile,int),
	NFO(PRIMARY,MOBILE,music,__static_mobile,int),
	NFO(PRIMARY,MOBILE,brew,__static_mobile,int),
	NFO(PRIMARY,MOBILE,scribe,__static_mobile,int),
	NFO(PRIMARY,MOBILE,bind,__static_mobile,int),
	NFO(PRIMARY,MOBILE,recite,__static_mobile,int),
	NFO(PRIMARY,MOBILE,resurrect,__static_mobile,int),
	NFO(PRIMARY,MOBILE,ranged,__static_mobile,int),
	NFO(PRIMARY,MOBILE,hide,__static_mobile,int),
	NFO(PRIMARY,MOBILE,paroxysm,__static_mobile,int),
	NFO(PRIMARY,MOBILE,bomb,__static_mobile,int),
	NFO(PRIMARY,MOBILE,bashed,__static_mobile,int),
	NFO(PRIMARY,MOBILE,reverie,__static_mobile,int),
	NFO(PRIMARY,MOBILE,paralyzed,__static_mobile,int),
	NFO(PRIMARY,MOBILE,pk_timer,__static_mobile,int),
	NFO(PRIMARY,MOBILE,no_recall,__static_mobile,int),
	NFO(PRIMARY,MOBILE,inking,__static_mobile,int),
	NFO(PRIMARY,MOBILE,imbuing,__static_mobile,int),
	NFO(PRIMARY,MOBILE,wimpy,__static_mobile,int),
	NFO(PRIMARY,MOBILE,hit,__static_mobile,long),
	NFO(PRIMARY,MOBILE,max_hit,__static_mobile,long),
	NFO(PRIMARY,MOBILE,mana,__static_mobile,long),
	NFO(PRIMARY,MOBILE,max_mana,__static_mobile,long),
	NFO(PRIMARY,MOBILE,move,__static_mobile,long),
	NFO(PRIMARY,MOBILE,max_move,__static_mobile,long),
	NFO(PRIMARY,MOBILE,gold,__static_mobile,long),
	NFO(PRIMARY,MOBILE,silver,__static_mobile,long),
	NFORA(PRIMARY,MOBILE,act,__static_mobile,2,long),
	NFO(PRIMARY,MOBILE,comm,__static_mobile,long),
	NFO(PRIMARY,MOBILE,wiznet,__static_mobile,long),
	NFO(PRIMARY,MOBILE,imm_flags,__static_mobile,long),
	NFO(PRIMARY,MOBILE,res_flags,__static_mobile,long),
	NFO(PRIMARY,MOBILE,vuln_flags,__static_mobile,long),
	NFORA(PRIMARY,MOBILE,affected_by,__static_mobile,2,long),
	NFO(PRIMARY,MOBILE,position,__static_mobile,int),
	NFO(PRIMARY,MOBILE,practice,__static_mobile,int),
	NFO(PRIMARY,MOBILE,train,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,carry_weight,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,carry_number,__static_mobile,int),
	NFO(PRIMARY,MOBILE,alignment,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,hitroll,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,damroll,__static_mobile,int),
	NFO(PRIMARY,MOBILE,xpboost,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,armour[0],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,armour[1],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,armour[2],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,armour[3],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cur_stat[STAT_STR],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cur_stat[STAT_INT],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cur_stat[STAT_WIS],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cur_stat[STAT_DEX],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cur_stat[STAT_CON],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,mod_stat[STAT_STR],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,mod_stat[STAT_INT],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,mod_stat[STAT_WIS],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,mod_stat[STAT_DEX],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,mod_stat[STAT_CON],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,perm_stat[STAT_STR],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,perm_stat[STAT_INT],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,perm_stat[STAT_WIS],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,perm_stat[STAT_DEX],__static_mobile,int),
	NFOR(PRIMARY,MOBILE,perm_stat[STAT_CON],__static_mobile,int),
	NFO(PRIMARY,MOBILE,form,__static_mobile,long),
	NFO(PRIMARY,MOBILE,parts,__static_mobile,long),
	NFO(PRIMARY,MOBILE,lostparts,__static_mobile,long),
	NFO(PRIMARY,MOBILE,size,__static_mobile,int),
	NFO(PRIMARY,MOBILE,material,__static_mobile,MATERIAL *),
	NFO(PRIMARY,MOBILE,damage,__static_mobile,DICE_DATA),
	NFO(PRIMARY,MOBILE,dam_type,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,start_pos,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,default_pos,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,hired_to,__static_mobile,time_t),
	NFOR(PRIMARY,MOBILE,creation_time,__static_mobile,time_t),
	NFOR(PRIMARY,MOBILE,mount,__static_mobile,CHAR_DATA *),
	NFOR(PRIMARY,MOBILE,rider,__static_mobile,CHAR_DATA *),
	NFOR(PRIMARY,MOBILE,riding,__static_mobile,bool),
	NFO(PRIMARY,MOBILE,time_left_death,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,dead,__static_mobile,bool),
	NFOR(PRIMARY,MOBILE,can_release,__static_mobile,bool),
	NFO(PRIMARY,MOBILE,home,__static_mobile,WNUM),
	NFOR(PRIMARY,MOBILE,deaths,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,player_kills,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cpk_kills,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,arena_kills,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,monster_kills,__static_mobile,long),
	NFOR(PRIMARY,MOBILE,wars_won,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,player_deaths,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,cpk_deaths,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,arena_deaths,__static_mobile,int),
	NFOR(PRIMARY,MOBILE,church,__static_mobile,CHURCH_DATA *),
	NFOR(PRIMARY,MOBILE,pending_mission,__static_mobile,MISSION_DATA *),
	NFOR(PRIMARY,MOBILE,missions,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,hunting,__static_mobile,CHAR_DATA *),
	NFOR(PRIMARY,MOBILE,challenged,__static_mobile,CHAR_DATA *),
	NFOR(PRIMARY,MOBILE,challenger,__static_mobile,CHAR_DATA *),
	NFO(PRIMARY,MOBILE,deitypoints,__static_mobile,long),
	NFO(PRIMARY,MOBILE,pneuma,__static_mobile,long),
	NFOR(PRIMARY,MOBILE,pulled_cart,__static_mobile,OBJ_DATA *),
	NFORS(PRIMARY,MOBILE,toxin,__static_mobile,MAX_TOXIN * sizeof(int)),
	NFO(PRIMARY,MOBILE,toxin[TOXIN_PARALYZE],__static_mobile,int),
	NFO(PRIMARY,MOBILE,toxin[TOXIN_WEAKNESS],__static_mobile,int),
	NFO(PRIMARY,MOBILE,toxin[TOXIN_NEURO],__static_mobile,int),
	NFO(PRIMARY,MOBILE,toxin[TOXIN_VENOM],__static_mobile,int),
	NFO(PRIMARY,MOBILE,bitten,__static_mobile,int),
	NFO(PRIMARY,MOBILE,bitten_type,__static_mobile,int),
	NFO(PRIMARY,MOBILE,bitten_level,__static_mobile,int),
	NFO(PRIMARY,MOBILE,repair,__static_mobile,int),
	NFO(PRIMARY,MOBILE,repair_obj,__static_mobile,OBJ_DATA *),
	NFO(PRIMARY,MOBILE,repair_amt,__static_mobile,int),
	NFO(PRIMARY,MOBILE,wildview_bonus_x,__static_mobile,int),
	NFO(PRIMARY,MOBILE,wildview_bonus_y,__static_mobile,int),
	NFORA(PRIMARY,MOBILE,tempstore,__static_mobile,MAX_TEMPSTORE,int),
	NFO(PRIMARY,MOBILE,tempstring,__static_mobile,char *),
	NFOR(PRIMARY,MOBILE,reputations,__static_mobile,LLIST *),
	NFOR(PRIMARY,MOBILE,deathsight_vision,__static_mobile,int),
	
	// Note

	// Object
	NFO(PRIMARY,OBJECT,name,__static_object,char *),
	NFO(PRIMARY,OBJECT,short_descr,__static_object,char *),
	NFO(PRIMARY,OBJECT,description,__static_object,char *),
	NFO(PRIMARY,OBJECT,full_description,__static_object,char *),
	NFOR(PRIMARY,OBJECT,carried_by,__static_object,CHAR_DATA *),
	NFOR(PRIMARY,OBJECT,pulled_by,__static_object,CHAR_DATA *),
	NFO(PRIMARY,OBJECT,num_enchanted,__static_object,int),
	NFOR(PRIMARY,OBJECT,item_type,__static_object,int),
	NFOS(PRIMARY,OBJECT,extra,__static_object,sizeof(long) * 4),
	NFO(PRIMARY,OBJECT,level,__static_object,int),
	NFO(PRIMARY,OBJECT,condition,__static_object,int),

	// Org
	NFOR(PRIMARY,ORG,hall_area,__static_org,AREA_DATA *),
	NFOR(PRIMARY,ORG,vnum_start,__static_org,long),
	NFOR(PRIMARY,ORG,name,__static_org,char *),
	NFOR(PRIMARY,ORG,flag,__static_org,char *),
	NFOR(PRIMARY,ORG,founder,__static_org,char *),
	NFOR(PRIMARY,ORG,owner,__static_org,char *),
	NFOR(PRIMARY,ORG,motd,__static_org,char *),
	NFOR(PRIMARY,ORG,rules,__static_org,char *),
	NFOR(PRIMARY,ORG,info,__static_org,char *),
	NFOR(PRIMARY,ORG,pneuma,__static_org,long),
	NFOR(PRIMARY,ORG,gold,__static_org,long),
	NFOR(PRIMARY,ORG,dp,__static_org,long),
	NFOR(PRIMARY,ORG,max_positions,__static_org,int),
	NFOR(PRIMARY,ORG,size,__static_org,int),
	NFOR(PRIMARY,ORG,alignment,__static_org,int),
	NFOR(PRIMARY,ORG,pk_wins,__static_org,long),
	NFOR(PRIMARY,ORG,pk_losses,__static_org,long),
	NFOR(PRIMARY,ORG,cpk_wins,__static_org,long),
	NFOR(PRIMARY,ORG,cpk_losses,__static_org,long),
	NFOR(PRIMARY,ORG,wars_won,__static_org,long),
	NFOR(PRIMARY,ORG,settings,__static_org,long),
	NFOR(PRIMARY,ORG,lcoffer,__static_org,LLIST *),

	// Quest

	// Race
	NFOR(PRIMARY,RACE,name,__static_race,char *),
	NFOR(PRIMARY,RACE,description,__static_race,char *),
	NFOR(PRIMARY,RACE,comments,__static_race,char *),
	NFOR(PRIMARY,RACE,uid,__static_race,int16_t),
	NFOR(PRIMARY,RACE,playable,__static_race,bool),
	NFOR(PRIMARY,RACE,starting,__static_race,bool),
	NFOR(PRIMARY,RACE,flags,__static_race,long),
	NFORA(PRIMARY,RACE,act,__static_race,2,long),
	NFORA(PRIMARY,RACE,aff,__static_race,2,long),
	NFOR(PRIMARY,RACE,off,__static_race,long),
	NFOR(PRIMARY,RACE,imm,__static_race,long),
	NFOR(PRIMARY,RACE,res,__static_race,long),
	NFOR(PRIMARY,RACE,vuln,__static_race,long),
	NFOR(PRIMARY,RACE,form,__static_race,long),
	NFOR(PRIMARY,RACE,parts,__static_race,long),
	NFOR(PRIMARY,RACE,premort,__static_race,RACE_DATA *),
	NFOR(PRIMARY,RACE,remort,__static_race,bool),
	NFOR(PRIMARY,RACE,who,__static_race,char *),
	NFOR(PRIMARY,RACE,skills,__static_race,LLIST *),
	NFORA(PRIMARY,RACE,stats,__static_race,MAX_STATS,int),
	NFORA(PRIMARY,RACE,max_stats,__static_race,MAX_STATS,int),
	NFORA(PRIMARY,RACE,max_vitals,__static_race,3,int),
	NFOR(PRIMARY,RACE,min_size,__static_race,int),
	NFOR(PRIMARY,RACE,max_size,__static_race,int),
	NFOR(PRIMARY,RACE,default_alignment,__static_race,int),

	// Rank
	NFOR(PRIMARY,RANK,uid,__static_rank,int16_t),
	NFOR(PRIMARY,RANK,ordinal,__static_rank,int16_t),
	NFOR(PRIMARY,RANK,name,__static_rank,char *),
	NFOR(PRIMARY,RANK,description,__static_rank,char *),
	NFOR(PRIMARY,RANK,comments,__static_rank,char *),
	NFOR(PRIMARY,RANK,capacity,__static_rank,long),
	NFOR(PRIMARY,RANK,flags,__static_rank,long),
	NFOR(PRIMARY,RANK,set,__static_rank,long),

	// Reputation
	NFOR(PRIMARY,REPUTATION,flags,__static_reputation,long),
	NFOR(PRIMARY,REPUTATION,reputation,__static_reputation,long),
	NFOR(PRIMARY,REPUTATION,paragon_level,__static_reputation,int),
	NFO(PRIMARY,REPUTATION,token,__static_reputation,TOKEN_DATA *),

	// Room
	NFOR(PRIMARY,ROOM,vnum,__static_room,long),
	NFO(PRIMARY,ROOM,name,__static_room,char *),
	NFO(PRIMARY,ROOM,description,__static_room,char *),
	NFORS(PRIMARY,ROOM,exit,__static_room,sizeof(EXIT_DATA *) * 10),
	NFO(PRIMARY,ROOM,exit[DIR_NORTH],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_NORTHEAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_EAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTHEAST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTH],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_SOUTHWEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_WEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_NORTHWEST],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_UP],__static_room,EXIT_DATA *),
	NFO(PRIMARY,ROOM,exit[DIR_DOWN],__static_room,EXIT_DATA *),
	NFOS(PRIMARY,ROOM,room_flag,__static_room,sizeof(__static_room.room_flag)),
	NFOR(PRIMARY,ROOM,light,__static_room,int),
	NFOR(PRIMARY,ROOM,sector,__static_room,SECTOR_DATA *),
	NFO(PRIMARY,ROOM,sector_flags,__static_room,long),
	NFO(PRIMARY,ROOM,heal_rate,__static_room,int),
	NFO(PRIMARY,ROOM,mana_rate,__static_room,int),
	NFO(PRIMARY,ROOM,move_rate,__static_room,int),
	NFO(PRIMARY,ROOM,savage_level,__static_room,int),
	NFOR(PRIMARY,ROOM,viewwilds,__static_room,WILDS_DATA *),
	NFOR(PRIMARY,ROOM,ship,__static_room,SHIP_DATA *),
	NFOR(PRIMARY,ROOM,lcontents,__static_room,LLIST *),
	NFOR(PRIMARY,ROOM,ltokens,__static_room,LLIST *),
	NFOR(PRIMARY,ROOM,lpeople,__static_room,LLIST *),
	NFOS(PRIMARY,ROOM,tempstore,__static_room,sizeof(__static_room.tempstore)),

	// Sector
	NFOR(PRIMARY,SECTOR,name,__static_sector,char *),
	NFOR(PRIMARY,SECTOR,description,__static_sector,char *),
	NFOR(PRIMARY,SECTOR,sector_class,__static_sector,int16_t),
	NFOR(PRIMARY,SECTOR,flags,__static_sector,long),
	NFOR(PRIMARY,SECTOR,move_cost,__static_sector,int16_t),
	NFOR(PRIMARY,SECTOR,hp_regen,__static_sector,int16_t),
	NFOR(PRIMARY,SECTOR,mana_regen,__static_sector,int16_t),
	NFOR(PRIMARY,SECTOR,move_regen,__static_sector,int16_t),
	NFOR(PRIMARY,SECTOR,soil,__static_sector,int16_t),

	// Ship

	// Skill

	// Token
	NFOR(PRIMARY,TOKEN,name,__static_token,char *),
	NFOR(PRIMARY,TOKEN,description,__static_token,char *),
	NFOR(PRIMARY,TOKEN,type,__static_token,int),
	NFOR(PRIMARY,TOKEN,flags,__static_token,long),
	NFO(PRIMARY,TOKEN,timer,__static_token,int),
	NFO(PRIMARY,TOKEN,player,__static_token,CHAR_DATA *),
	NFO(PRIMARY,TOKEN,object,__static_token,OBJ_DATA *),
	NFO(PRIMARY,TOKEN,room,__static_token,ROOM_INDEX_DATA *),
	NFOS(PRIMARY,TOKEN,value,__static_token,MAX_TOKEN_VALUES * sizeof(long)),
	NFOS(PRIMARY,TOKEN,tempstore,__static_token,MAX_TEMPSTORE * sizeof(long)),


	// Widevnum
	NFO(PRIMARY,WIDEVNUM,pArea,__static_wnum,AREA_DATA *),
	NFO(PRIMARY,WIDEVNUM,vnum,__static_wnum,long),

	// Wilds

	NFOEND
};

bool nib_field_offset_lookup(NIB_TYPE *context, char *name, size_t *offset, size_t *size, bool *lvalue)
{
	if (!context) return NULL;	// Indicates error or unknown

	NIB_TYPE_CLASS c = context->type_class;
	NIB_PRIMARY_TYPE p = (c == NTC_PRIMARY) ? context->_.primary : NT_UNKNOWN;

	for(int i = 0; __field_offsets[i].field; i++)
	{
		if (__field_offsets[i].type_class == c &&
			__field_offsets[i].primary == p &&
			!str_cmp(__field_offsets[i].field, name))
		{
			*offset = __field_offsets[i].offset;
			*size = __field_offsets[i].size;
			*lvalue = __field_offsets[i].lvalue;
			return true;
		}
	}

	return false;
}

NIB_FIELD *new_nib_field(char *name, NIB_TYPE *type, bool readonly, bool lvalue, size_t offset, METHOD_FUNC *method)
{
	NIB_FIELD *field = calloc(1, sizeof(NIB_FIELD));

	field->name = strdup(name);
	field->type = nib_type_copy(type);
	// fprintf(stderr, "new_nib_field(%s,%s)\n", field->name, nib_get_typename(NULL,type));
	field->stype = convert_to_stype(type, false);
	if (type && type->type_class == NTC_LIST)
		field->stype2 = convert_to_stype(type->_.list.type, false);
	else if (type && type->type_class == NTC_ARRAY)
		field->stype2 = convert_to_stype(type->_.array.type, false);
	else
		field->stype2 = NST_UNKNOWN;
	field->readonly = readonly;
	field->lvalue = lvalue;
	field->offset = offset;
	field->method = method;

	return field;
}

void free_nib_field(NIB_FIELD *field)
{
	if (field)
	{
		if (field->name) free(field->name);
		free_nib_type(field->type);

		free(field);
	}
}

bool nib_field_valid_context(NIB_TYPE *context)
{
	if (context)
	{
		if (context->type_class == NTC_PRIMARY)
		{
			switch(context->_.primary)
			{
				case NT_WIDEVNUM:	return true;

				case NT_ACCOUNT:	return true;
				case NT_AFFECT:		return true;
				case NT_AREA:		return true;
				case NT_CHANNEL:	return true;
				case NT_CLASS:		return true;
				case NT_DUNGEON:	return true;
				case NT_EXIT:		return true;
				case NT_INSTANCE:	return true;
				case NT_LIQUID:		return true;
				case NT_MAIL:		return true;
				case NT_MATERIAL:	return true;
				case NT_MISSION:	return true;
				case NT_MOBILE:		return true;
				case NT_NOTE:		return true;
				case NT_OBJECT:		return true;
				case NT_ORG:		return true;
				case NT_QUEST:		return true;
				case NT_RACE:		return true;
				case NT_RANK:		return true;
				case NT_REPUTATION:	return true;
				case NT_ROOM:		return true;
				case NT_SECTOR:		return true;
				case NT_SHIP:		return true;
				case NT_SKILL:		return true;
				case NT_TOKEN:		return true;
				case NT_WILDS:		return true;
				case NT_WORLD:		return true;
			}
		}
	}

	return false;
}

static LLIST *__get_field_context_nst(NIB_SCRIPT_STACK_TYPE context)
{
	switch(context)
	{
		case NST_NUMBER:	return nib_fields_int;
		case NST_FLOAT:		return nib_fields_float;
		case NST_BOOLEAN:	return nib_fields_boolean;
		case NST_CHAR:		return nib_fields_char;
		case NST_STRING:	return nib_fields_string;
		case NST_MAP:		return nib_fields_map;
		case NST_WIDEVNUM:	return nib_fields_widevnum;
		case NST_ACCOUNT:	return nib_fields_account;
		case NST_AFFECT:	return nib_fields_affect;
		case NST_AREA:		return nib_fields_area;
		case NST_CHANNEL:	return nib_fields_channel;
		case NST_CLASS:		return nib_fields_class;
		case NST_DUNGEON:	return nib_fields_dungeon;
		case NST_EXIT:		return nib_fields_exit;
		case NST_INSTANCE:	return nib_fields_instance;
		case NST_LIQUID:	return nib_fields_liquid;
		case NST_MAIL:		return nib_fields_mail;
		case NST_MATERIAL:	return nib_fields_material;
		case NST_MISSION:	return nib_fields_mission;
		case NST_MOBILE:	return nib_fields_mobile;
		case NST_NOTE:		return nib_fields_note;
		case NST_OBJECT:	return nib_fields_object;
		case NST_ORG:		return nib_fields_org;
		case NST_QUEST:		return nib_fields_quest;
		case NST_RACE:		return nib_fields_race;
		case NST_RANK:		return nib_fields_rank;
		case NST_REPUTATION:return nib_fields_reputation;
		case NST_ROOM:		return nib_fields_room;
		case NST_SECTOR:	return nib_fields_sector;
		case NST_SHIP:		return nib_fields_ship;
		case NST_SKILL:		return nib_fields_skill;
		case NST_TOKEN:		return nib_fields_token;
		case NST_WILDS:		return nib_fields_wilds;
		case NST_WORLD:		return nib_fields_world;
		case NST_FLAG:		return nib_fields_flag;
		case NST_STAT:		return nib_fields_stat;
		case NST_LIST:		return nib_fields_list;
		case NST_LIST_S:	return nib_fields_list;
		case NST_ARRAY:		return nib_fields_array;
		case NST_ARRAY_S:	return nib_fields_array;
	}

	return NULL;
}

static LLIST *__get_field_context(NIB_TYPE *context)
{
	if (!context) return NULL;

	if (context->type_class == NTC_PRIMARY)
	{
		switch(context->_.primary)
		{
			case NT_NUMBER:		return nib_fields_int;
			case NT_FLOAT:		return nib_fields_float;
			case NT_BOOLEAN:	return nib_fields_boolean;
			case NT_CHAR:		return nib_fields_char;
			case NT_STRING:		return nib_fields_string;
			case NT_MAP:		return nib_fields_map;
			case NT_WIDEVNUM:	return nib_fields_widevnum;

			case NT_ACCOUNT:	return nib_fields_account;
			case NT_AFFECT:		return nib_fields_affect;
			case NT_AREA:		return nib_fields_area;
			case NT_CHANNEL:	return nib_fields_channel;
			case NT_CLASS:		return nib_fields_class;
			case NT_DUNGEON:	return nib_fields_dungeon;
			case NT_EXIT:		return nib_fields_exit;
			case NT_INSTANCE:	return nib_fields_instance;
			case NT_LIQUID:		return nib_fields_liquid;
			case NT_MAIL:		return nib_fields_mail;
			case NT_MATERIAL:	return nib_fields_material;
			case NT_MISSION:	return nib_fields_mission;
			case NT_MOBILE:		return nib_fields_mobile;
			case NT_NOTE:		return nib_fields_note;
			case NT_OBJECT:		return nib_fields_object;
			case NT_ORG:		return nib_fields_org;
			case NT_QUEST:		return nib_fields_quest;
			case NT_RACE:		return nib_fields_race;
			case NT_RANK:		return nib_fields_rank;
			case NT_REPUTATION:	return nib_fields_reputation;
			case NT_ROOM:		return nib_fields_room;
			case NT_SECTOR:		return nib_fields_sector;
			case NT_SHIP:		return nib_fields_ship;
			case NT_SKILL:		return nib_fields_skill;
			case NT_TOKEN:		return nib_fields_token;
			case NT_WILDS:		return nib_fields_wilds;
			case NT_WORLD:		return nib_fields_world;
		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_fields_list;
	else if (context->type_class == NTC_ARRAY)
		return nib_fields_array;
	else if (context->type_class == NTC_FLAG)
		return nib_fields_flag;
	else if (context->type_class == NTC_STAT)
		return nib_fields_stat;

	return NULL;
}

NIB_FIELD *nib_field_get(NIB_TYPE *context, char *name)
{
	// Determine the context
	LLIST *fields = __get_field_context(context);
	if (!fields) return NULL;

	ITERATOR it;
	NIB_FIELD *field;
	iterator_start(&it, fields);
	while((field = (NIB_FIELD *)iterator_nextdata(&it)))
	{
		if (!str_cmp(field->name, name))
			break;
	}
	iterator_stop(&it);

	return field;
}

NIB_FIELD *nib_field_get_byid(NIB_SCRIPT_STACK_TYPE context, int id)
{
	// Determine the context
	LLIST *fields = __get_field_context_nst(context);
	if (!fields) return NULL;

	ITERATOR it;
	NIB_FIELD *field;
	iterator_start(&it, fields);
	while((field = (NIB_FIELD *)iterator_nextdata(&it)))
	{
		if (field->id == id)
			break;
	}
	iterator_stop(&it);

	return field;
}

bool nib_field_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool readonly, bool lvalue, size_t offset, size_t size, METHOD_FUNC *method)
{
	// Assume the field does not exist

	// Determine the context
	LLIST *fields = __get_field_context(context);
	if (!fields) return false;

	// Create field
	NIB_FIELD *field = new_nib_field(name, ret, readonly, lvalue, offset, method);
	if (!field) return false;

	list_appendlink(fields, field);
	field->id = list_size(fields);
	return true;
}

#include "funcs.h"

#define MFEL(f)	{ #f, nib_method_func_##f, true }
#define MFER(f)	{ #f, nib_method_func_##f, false }
#define MFEND	{ NULL, NULL, false }

const struct nib_method_func_type nib_method_funcs[] =
{
	MFER(affect_is_permanent),
	MFER(area_get_room),
	MFER(array_length),
	MFER(class_display),
	MFER(class_who),
	MFER(exit_get_direction),
	MFER(exit_get_door),
	MFER(exit_get_mate),
	MFEL(exit_get_north),
	MFEL(exit_get_northeast),
	MFEL(exit_get_east),
	MFEL(exit_get_southeast),
	MFEL(exit_get_south),
	MFEL(exit_get_southwest),
	MFEL(exit_get_west),
	MFEL(exit_get_northwest),
	MFEL(exit_get_up),
	MFEL(exit_get_down),
	MFER(exit_is_oneway),
	MFER(exit_is_twoway),
	MFER(function_get_time),
	MFER(function_print_msg),
	MFER(function_random_percent),
	MFER(function_reckoning),
	MFER(list_add),
	MFER(list_insert),
	MFER(list_remove),
	MFER(list_size),
	MFER(mobile_get_widevnum),
	MFER(mobile_is_pc),
	MFER(mobile_get_equipment),
	MFER(number_random_value),
	MFER(rank_color),
	MFER(reputation_name),
	MFER(reputation_description),
	MFER(reputation_comments),
	MFER(reputation_widevnum),
	MFER(reputation_rank),
	MFER(reputation_maximum_rank),
	MFER(reputation_ranks),
	MFER(reputation_initial_rank),
	MFER(reputation_initial_reputation),
	MFER(room_get_exits),
	MFER(room_reset),
	MFER(room_set_sector),
	MFER(string_length),
	MFER(time_add),
	MFER(time_add_minutes),
	MFER(time_add_hours),
	MFER(time_add_days),
	MFER(time_add_months),
	MFER(token_owner_type),
	MFER(token_get_index_value),
	MFEND
};

bool nib_method_func_lookup(const char *name, METHOD_FUNC **func, bool *lvalue)
{
	for(int i = 0; nib_method_funcs[i].name; i++)
		if (!str_cmp(nib_method_funcs[i].name, name))
		{
			*func = nib_method_funcs[i].func;
			*lvalue = nib_method_funcs[i].lvalue;
			return true;
		}

	return false;
}


NIB_METHOD *new_nib_method(char *name, NIB_TYPE *ret, bool constant, bool lvalue, int modifiers, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	NIB_METHOD *method = calloc(1, sizeof(NIB_METHOD));

	method->name = strdup(name);
	method->constant = constant;
	method->lvalue = lvalue;
	method->modifiers = modifiers;
	method->result = nib_type_copy(ret);
	method->sresult = convert_to_stype(ret, false);
	if (ret && ret->type_class == NTC_LIST)
		method->sresult2 = convert_to_stype(ret->_.list.type, false);
	else if (ret && ret->type_class == NTC_ARRAY)
		method->sresult2 = convert_to_stype(ret->_.array.type, false);
		// Length is on the actual result type
	else
		method->sresult2 = NST_UNKNOWN;
	method->nparams = list_size(params);
	if (method->nparams > 0)
	{
		method->params = (NIB_TYPE **)calloc(method->nparams, sizeof(NIB_TYPE *));
		ITERATOR it;
		NIB_TYPE *ptype;
		int index = 0;
		iterator_start(&it, params);
		while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
		{
			method->params[index++] = nib_type_copy(ptype);
		}
		iterator_stop(&it);
	}
	// else method->params = NULL; // already null by calloc

	if (method_name) method->method_name = strdup(method_name);
	method->method = method_func;

	return method;
}

void free_nib_method(NIB_METHOD *method)
{
	if (method)
	{
		free(method->name);
		free_nib_type(method->result);

		if (method->params && method->nparams > 0)
		{
			for(int i = method->nparams; i-- > 0;)
				free_nib_type(method->params[i]);

			free(method->params);
		}

		if (method->method_name)
			free(method->method_name);

		free(method);
	}
}

bool nib_method_valid_context(NIB_TYPE *context)
{
	if (context)
	{
		if (context->type_class == NTC_PRIMARY)
		{
			switch(context->_.primary)
			{
				case NT_NUMBER:		return true;
				case NT_FLOAT:		return true;
				case NT_BOOLEAN:	return true;
				case NT_CHAR:		return true;
				case NT_STRING:		return true;
				case NT_MAP:		return true;
				case NT_WIDEVNUM:	return true;
				case NT_TIME:		return true;

				case NT_ACCOUNT:	return true;
				case NT_AFFECT:		return true;
				case NT_AREA:		return true;
				case NT_CHANNEL:	return true;
				case NT_CLASS:		return true;
				case NT_DUNGEON:	return true;
				case NT_EXIT:		return true;
				case NT_INSTANCE:	return true;
				case NT_LIQUID:		return true;
				case NT_MAIL:		return true;
				case NT_MATERIAL:	return true;
				case NT_MISSION:	return true;
				case NT_MOBILE:		return true;
				case NT_NOTE:		return true;
				case NT_OBJECT:		return true;
				case NT_ORG:		return true;
				case NT_QUEST:		return true;
				case NT_RACE:		return true;
				case NT_RANK:		return true;
				case NT_REPUTATION:	return true;
				case NT_ROOM:		return true;
				case NT_SECTOR:		return true;
				case NT_SHIP:		return true;
				case NT_SKILL:		return true;
				case NT_TOKEN:		return true;
				case NT_WILDS:		return true;
				case NT_WORLD:		return true;
			}
		}
		else if (context->type_class == NTC_LIST)
			return true;
		else if (context->type_class == NTC_ARRAY)
			return true;
		else if (context->type_class == NTC_FLAG)
			return true;
		else if (context->type_class == NTC_STAT)
			return true;
	}

	return false;
}

static LLIST *__get_method_context(NIB_TYPE *context)
{
	if (!context) return nib_functions;

	if (context->type_class == NTC_PRIMARY)
	{
		switch(context->_.primary)
		{
			case NT_NUMBER:		return nib_methods_int;
			case NT_FLOAT:		return nib_methods_float;
			case NT_BOOLEAN:	return nib_methods_boolean;
			case NT_CHAR:		return nib_methods_char;
			case NT_STRING:		return nib_methods_string;
			case NT_MAP:		return nib_methods_map;
			case NT_WIDEVNUM:	return nib_methods_widevnum;
			case NT_TIME:		return nib_methods_time;

			case NT_ACCOUNT:	return nib_methods_account;
			case NT_AFFECT:		return nib_methods_affect;
			case NT_AREA:		return nib_methods_area;
			case NT_CHANNEL:	return nib_methods_channel;
			case NT_CLASS:		return nib_methods_class;
			case NT_DUNGEON:	return nib_methods_dungeon;
			case NT_EXIT:		return nib_methods_exit;
			case NT_INSTANCE:	return nib_methods_instance;
			case NT_LIQUID:		return nib_methods_liquid;
			case NT_MAIL:		return nib_methods_mail;
			case NT_MATERIAL:	return nib_methods_material;
			case NT_MISSION:	return nib_methods_mission;
			case NT_MOBILE:		return nib_methods_mobile;
			case NT_NOTE:		return nib_methods_note;
			case NT_OBJECT:		return nib_methods_object;
			case NT_ORG:		return nib_methods_org;
			case NT_QUEST:		return nib_methods_quest;
			case NT_RACE:		return nib_methods_race;
			case NT_RANK:		return nib_methods_rank;
			case NT_REPUTATION:	return nib_methods_reputation;
			case NT_ROOM:		return nib_methods_room;
			case NT_SECTOR:		return nib_methods_sector;
			case NT_SHIP:		return nib_methods_ship;
			case NT_SKILL:		return nib_methods_skill;
			case NT_TOKEN:		return nib_methods_token;
			case NT_WILDS:		return nib_methods_wilds;
			case NT_WORLD:		return nib_methods_world;

		}
	}
	else if (context->type_class == NTC_LIST)
		return nib_methods_list;
	else if (context->type_class == NTC_ARRAY)
		return nib_methods_array;
	else if (context->type_class == NTC_FLAG)
		return nib_methods_flag;
	else if (context->type_class == NTC_STAT)
		return nib_methods_stat;

	return NULL;
}

static bool __method_matches_signature(NIB_METHOD *method, NIB_TYPE *anytype, char *name, LLIST *params)
{
	if (str_cmp(method->name, name)) return false;

	if (method->nparams < 1 && list_size(params) > 0) return false;

	ITERATOR it;
	NIB_TYPE *ptype;
	int index = 0;

	bool valid = true;
	iterator_start(&it, params);
	while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (index >= method->nparams)
		{
			// Ran out of params
			valid = false;
			break;
		}

		NIB_TYPE *atype = method->params[index];

		if (atype->type_class == NTC_VARARGS)
		{
			// Variable argument method; ignore the rest of the arguments
			break;
		}
		else if (atype->type_class == NTC_ANY)
		{
			// If it is ANY but the anytype is NULL, then skip it.
			//  Only really care if it is a list
			if (anytype == NULL)
				continue;

			atype = anytype;	// Substitute the ANY type replacement
		}
		else if (atype->type_class == NTC_MULTI)
		{
			if (!is_nib_type_in_multi(atype,ptype))
			{
				valid = false;
				break;
			}
		}
		else if (!are_nib_types_equal(atype, ptype))
		{
			valid = false;
			break;
		}
		else if (atype->_reference && !ptype->_reference)
		{
			valid = false;
			break;
		}

		++index;
	}
	iterator_stop(&it);

	return valid;
}

static LLIST *__get_method_context_nst(NIB_SCRIPT_STACK_TYPE context)
{
	switch(context)
	{
		case NST_FUNCTION:	return nib_functions;
		case NST_NUMBER:	return nib_methods_int;
		case NST_FLOAT:		return nib_methods_float;
		case NST_BOOLEAN:	return nib_methods_boolean;
		case NST_CHAR:		return nib_methods_char;
		case NST_STRING:	return nib_methods_string;
		case NST_STRING_S:	return nib_methods_string;
		case NST_MAP:		return nib_methods_map;
		case NST_WIDEVNUM:	return nib_methods_widevnum;
		case NST_TIME:		return nib_methods_time;
		case NST_ACCOUNT:	return nib_methods_account;
		case NST_AFFECT:	return nib_methods_affect;
		case NST_AREA:		return nib_methods_area;
		case NST_CHANNEL:	return nib_methods_channel;
		case NST_CLASS:		return nib_methods_class;
		case NST_DUNGEON:	return nib_methods_dungeon;
		case NST_EXIT:		return nib_methods_exit;
		case NST_INSTANCE:	return nib_methods_instance;
		case NST_LIQUID:	return nib_methods_liquid;
		case NST_MAIL:		return nib_methods_mail;
		case NST_MATERIAL:	return nib_methods_material;
		case NST_MISSION:	return nib_methods_mission;
		case NST_MOBILE:	return nib_methods_mobile;
		case NST_NOTE:		return nib_methods_note;
		case NST_OBJECT:	return nib_methods_object;
		case NST_ORG:		return nib_methods_org;
		case NST_QUEST:		return nib_methods_quest;
		case NST_RACE:		return nib_methods_race;
		case NST_RANK:		return nib_methods_rank;
		case NST_REPUTATION:return nib_methods_reputation;
		case NST_ROOM:		return nib_methods_room;
		case NST_SECTOR:	return nib_methods_sector;
		case NST_SHIP:		return nib_methods_ship;
		case NST_SKILL:		return nib_methods_skill;
		case NST_TOKEN:		return nib_methods_token;
		case NST_WILDS:		return nib_methods_wilds;
		case NST_WORLD:		return nib_methods_world;
		case NST_FLAG:		return nib_methods_flag;
		case NST_STAT:		return nib_methods_stat;
		case NST_LIST:		return nib_methods_list;
		case NST_LIST_S:	return nib_methods_list;
		case NST_ARRAY:		return nib_methods_array;
		case NST_ARRAY_S:	return nib_methods_array;
	}

	return NULL;
}



NIB_METHOD *nib_method_get_byid(NIB_SCRIPT_STACK_TYPE context, int id)
{
	// Determine the context
	LLIST *methods = __get_method_context_nst(context);
	if (!methods) return NULL;

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (method->id == id)
			break;
	}
	iterator_stop(&it);

	return method;
}

NIB_METHOD *nib_method_get(NIB_TYPE *context, char *name, LLIST *params)
{
	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return NULL;

	NIB_TYPE *subtype = NULL;
	if (context)
	{
		if (context->type_class == NTC_LIST)
			subtype = context->_.list.type;	// Get LIST element type
		else if (context->type_class == NTC_ARRAY)
			subtype = context->_.list.type;	// Get ARRAY element type
	}

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (__method_matches_signature(method, subtype, name, params))
			break;
	}
	iterator_stop(&it);

	return method;
}

static bool __method_same_signature(NIB_METHOD *method, char *name, LLIST *params)
{
	if (str_cmp(method->name, name)) return false;

	if (list_size(params) != method->nparams) return false;

	ITERATOR it;
	NIB_TYPE *ptype;
	int index = 0;

	iterator_start(&it, params);
	while((ptype = (NIB_TYPE *)iterator_nextdata(&it)))
	{
		if (!are_nib_types_equal(method->params[index], ptype))
			break;

		if (method->params[index]->_reference && !ptype->_reference)
			break;

		++index;
	}
	iterator_stop(&it);

	return (ptype == NULL);
}

bool nib_method_exists(NIB_TYPE *context, char *name, LLIST *params)
{
	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return false;

	ITERATOR it;
	NIB_METHOD *method;
	iterator_start(&it, methods);
	while((method = (NIB_METHOD *)iterator_nextdata(&it)))
	{
		if (__method_same_signature(method, name, params))
			break;
	}
	iterator_stop(&it);

	return (method != NULL);
}

bool nib_method_add(NIB_TYPE *context, char *name, NIB_TYPE *ret, bool constant, bool lvalue, int modifiers, LLIST *params, char *method_name, METHOD_FUNC *method_func)
{
	// Assume the method signature does not exist

	// Determine the context
	LLIST *methods = __get_method_context(context);
	if (!methods) return false;

	// Create methods
	NIB_METHOD *method = new_nib_method(name, ret, constant, lvalue, modifiers, params, method_name, method_func);
	if (!method) return false;

	list_appendlink(methods, method);
	method->id = list_size(methods);
	return true;
}


static bool nib_methods_load()
{
	bool valid = false;

	// Open the methods.dat file
	nibmethodin = fopen("./methods.dat", "r");

	if (nibmethodin != NULL)
	{
		// Parse file
		valid = !nibmethodparse();

		// Close file
		fclose(nibmethodin);
	}

	return valid;
}

static void __free_method(void *data)
{
	free_nib_method((NIB_METHOD *)data);
}

static void __free_field(void *data)
{
	free_nib_field((NIB_FIELD *)data);
}

static inline LLIST *__create_method_list()
{
	return list_createx(false, NULL, __free_method);
}

static inline LLIST *__create_field_list()
{
	return list_createx(false, NULL, __free_field);
}

#define __met(t) \
	nib_methods_##t = __create_method_list(); \
	if (!list_isvalid(nib_methods_##t)) return false;

#define __fld(t) \
	nib_fields_##t = __create_field_list(); \
	if (!list_isvalid(nib_fields_##t)) return false;


bool nib_methods_init()
{
	nib_functions = __create_method_list();
	if(!list_isvalid(nib_functions)) return false;

	__met(int)
	__met(float)
	__met(boolean)
	__met(char)
	__met(string)
	__met(map)
	__met(widevnum)
	__met(time)
	__met(list)
	__met(array)
	__met(flag)
	__met(stat)
	__met(account)
	__met(affect)
	__met(area)
	__met(channel)
	__met(class)
	__met(dungeon)
	__met(exit)
	__met(instance)
	__met(liquid)
	__met(mail)
	__met(material)
	__met(mission)
	__met(mobile)
	__met(note)
	__met(object)
	__met(org)
	__met(quest)
	__met(race)
	__met(rank)
	__met(reputation)
	__met(room)
	__met(sector)
	__met(ship)
	__met(skill)
	__met(token)
	__met(wilds)
	__met(world)

	__fld(int)
	__fld(float)
	__fld(boolean)
	__fld(char)
	__fld(string)
	__fld(map)
	__fld(widevnum)
	__fld(time)
	__fld(list)
	__fld(array)
	__fld(flag)
	__fld(stat)
	__fld(account)
	__fld(affect)
	__fld(area)
	__fld(channel)
	__fld(class)
	__fld(dungeon)
	__fld(exit)
	__fld(instance)
	__fld(liquid)
	__fld(mail)
	__fld(material)
	__fld(mission)
	__fld(mobile)
	__fld(note)
	__fld(object)
	__fld(org)
	__fld(quest)
	__fld(race)
	__fld(rank)
	__fld(reputation)
	__fld(room)
	__fld(sector)
	__fld(ship)
	__fld(skill)
	__fld(token)
	__fld(wilds)
	__fld(world)

	return nib_methods_load();
}

void nib_methods_cleanup()
{
	list_destroy(nib_functions);
	list_destroy(nib_methods_int);
	list_destroy(nib_methods_float);
	list_destroy(nib_methods_boolean);
	list_destroy(nib_methods_char);
	list_destroy(nib_methods_string);
	list_destroy(nib_methods_map);
	list_destroy(nib_methods_widevnum);
	list_destroy(nib_methods_time);
	list_destroy(nib_methods_list);
	list_destroy(nib_methods_array);
	list_destroy(nib_methods_flag);
	list_destroy(nib_methods_stat);
	list_destroy(nib_methods_account);
	list_destroy(nib_methods_affect);
	list_destroy(nib_methods_area);
	list_destroy(nib_methods_channel);
	list_destroy(nib_methods_class);
	list_destroy(nib_methods_dungeon);
	list_destroy(nib_methods_exit);
	list_destroy(nib_methods_instance);
	list_destroy(nib_methods_liquid);
	list_destroy(nib_methods_mail);
	list_destroy(nib_methods_material);
	list_destroy(nib_methods_mission);
	list_destroy(nib_methods_mobile);
	list_destroy(nib_methods_note);
	list_destroy(nib_methods_object);
	list_destroy(nib_methods_org);
	list_destroy(nib_methods_quest);
	list_destroy(nib_methods_race);
	list_destroy(nib_methods_rank);
	list_destroy(nib_methods_reputation);
	list_destroy(nib_methods_room);
	list_destroy(nib_methods_sector);
	list_destroy(nib_methods_ship);
	list_destroy(nib_methods_skill);
	list_destroy(nib_methods_token);
	list_destroy(nib_methods_wilds);
	list_destroy(nib_methods_world);

	list_destroy(nib_fields_int);
	list_destroy(nib_fields_float);
	list_destroy(nib_fields_boolean);
	list_destroy(nib_fields_char);
	list_destroy(nib_fields_string);
	list_destroy(nib_fields_map);
	list_destroy(nib_fields_widevnum);
	list_destroy(nib_fields_time);
	list_destroy(nib_fields_array);
	list_destroy(nib_fields_list);
	list_destroy(nib_fields_flag);
	list_destroy(nib_fields_stat);
	list_destroy(nib_fields_account);
	list_destroy(nib_fields_affect);
	list_destroy(nib_fields_area);
	list_destroy(nib_fields_channel);
	list_destroy(nib_fields_class);
	list_destroy(nib_fields_dungeon);
	list_destroy(nib_fields_exit);
	list_destroy(nib_fields_instance);
	list_destroy(nib_fields_liquid);
	list_destroy(nib_fields_mail);
	list_destroy(nib_fields_material);
	list_destroy(nib_fields_mission);
	list_destroy(nib_fields_mobile);
	list_destroy(nib_fields_note);
	list_destroy(nib_fields_object);
	list_destroy(nib_fields_org);
	list_destroy(nib_fields_quest);
	list_destroy(nib_fields_race);
	list_destroy(nib_fields_rank);
	list_destroy(nib_fields_reputation);
	list_destroy(nib_fields_room);
	list_destroy(nib_fields_sector);
	list_destroy(nib_fields_ship);
	list_destroy(nib_fields_skill);
	list_destroy(nib_fields_token);
	list_destroy(nib_fields_wilds);
	list_destroy(nib_fields_world);
}

void nib_method_get_prototype(NIB_METHOD *method, char *buffer, size_t max_len)
{
	int len = 0;

	len = snprintf(buffer, max_len, "%s", nib_get_typename(NULL,method->result));
	len += snprintf(buffer + len, max_len - len, " %s(", method->name);

	for(int i = 0; i < method->nparams; i++)
	{
		if (i > 0)
			len += snprintf(buffer + len, max_len - len, ",%s", nib_get_typename(NULL,method->params[i]));
		else
			len += snprintf(buffer + len, max_len - len, "%s", nib_get_typename(NULL,method->params[i]));
	}

	len += snprintf(buffer + len, max_len - len, ")");
	buffer[len] = 0;
}