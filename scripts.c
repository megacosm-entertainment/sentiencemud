/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "strings.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include "merc.h"
#include "traits.h"
#include "tables.h"
#include "scripts.h"
#include "class_data.h"
#include "skill_data.h"
#include "song_data.h"
#include "event_types.h"
#include "recycle.h"
#include "wilds.h"
#include "skill_group.h"
#include "mxp_links.h"
#include "io/cache/redis_cache.h"
//#define DEBUG_MODULE
#include "debug.h"

void do_emote(CHAR_DATA *ch, char *argument);

extern const char *cmd_operator_table[];

int script_security = INIT_SCRIPT_SECURITY;
int script_call_depth = 0;
int script_lastreturn = PRET_EXECUTED;
bool script_remotecall = false;
bool script_destructed = false;
bool wiznet_script = false;
bool script_force_execute = false;	// Executes the script even if disabled
SCRIPT_CB *script_call_stack = NULL;
static SCRIPT_EXECUTE_CONTEXT script_exec_context = { NULL, NULL };

static void emit_script_staff_event_at(const char *plain_message,
                                       const char *staff_message,
                                       const char *action,
                                       SCRIPT_DATA *script,
                                       const char *file,
                                       long line,
                                       const char *func)
{
    char extra[128];
    log_context_t ctx = {
        .actor_type = "system",
        .actor_name = "script_engine",
        .action = action,
    };

    if (script) {
        snprintf(extra, sizeof(extra), "{\"script_vnum\":%ld,\"script_lines\":%d}",
                 (long)script->vnum, script->lines);
        ctx.extra_json = extra;
    }

    log_event_t ev = {
        .severity = EVENT_SEV_INFO,
        .category = LOG_SCRIPTS,
        .plain_message = plain_message ? plain_message : "script_wiznet_event",
        .staff_message = staff_message,
        .wiznet_flag = WIZ_SCRIPTS,
        .context = &ctx,
        .source_file = file,
        .source_line = line,
        .source_func = func,
    };

    log_emit_event(&ev, NULL);
}

#define emit_script_staff_event(plain_msg, staff_msg, action_str, script_ptr) \
    emit_script_staff_event_at((plain_msg), (staff_msg), (action_str), (script_ptr), \
                               __FILE__, __LINE__, __func__)

#define SCRIPT_LOG_MAX_LEN 16384
#define SCRIPT_RUNTIME_ERROR_MAX 50
#define SCRIPT_RUNTIME_ERROR_TEXT_MAX 512
#define AUDIT_HISTORY_MAX 64

#define AUDIT_ERROR_SCRIPT 1
#define AUDIT_ERROR_RESET  2

typedef struct script_runtime_error_data SCRIPT_RUNTIME_ERROR;
typedef struct audit_occurrence_data AUDIT_OCCURRENCE;

struct audit_occurrence_data {
    time_t when;
    char caller[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char host[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char location[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char message[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
};

struct script_runtime_error_data {
    bool active;
    int id;
    unsigned long long last_touch;
    char signature[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    time_t when;
    time_t first_when;
    long local_count;
    long redis_count;
    int category;
    int script_type;
    long area_uid;
    int script_vnum;
    int line;
    bool has_room;
    bool is_wilds;
    long room_area_uid;
    long room_vnum;
    long wilds_uid;
    long wilds_x;
    long wilds_y;
    int host_kind;
    long host_area_uid;
    long host_vnum;
    char reset_command;
    char type[SCRIPT_RUNTIME_ERROR_TEXT_MAX / 8];
    char caller[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char host[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char location[SCRIPT_RUNTIME_ERROR_TEXT_MAX];
    char message[SCRIPT_RUNTIME_ERROR_TEXT_MAX];

    int history_next;
    int history_count;
    AUDIT_OCCURRENCE history[AUDIT_HISTORY_MAX];
};

static SCRIPT_RUNTIME_ERROR script_runtime_errors[SCRIPT_RUNTIME_ERROR_MAX];
static int script_runtime_error_next_id = 1;
static unsigned long long script_runtime_error_touch = 0ULL;

enum {
    AUDIT_HOST_UNKNOWN = 0,
    AUDIT_HOST_MOB,
    AUDIT_HOST_OBJ,
    AUDIT_HOST_ROOM,
    AUDIT_HOST_TOKEN,
    AUDIT_HOST_RESET,
    AUDIT_HOST_AREA
};

ROOM_INDEX_DATA room_used_for_wilderness;
ROOM_INDEX_DATA room_pointer_vlink;
ROOM_INDEX_DATA room_pointer_environment;

bool opc_skip_block(SCRIPT_CB *block,int level,bool endblock);
bool is_stat( const struct flag_type *flag_table );
static void script_append_runtime_logf(SCRIPT_DATA *script, int line, const char *fmt, ...);

#define SCRIPT_ENTITY_HASH_SIZE 257
#define SCRIPT_ENTITY_LIST_CACHE_MAX 256
#define SCRIPT_IFCHECK_HASH_SIZE 257
#define SCRIPT_LOOKUP_REPORT_INTERVAL 50000UL

typedef struct script_entity_lookup_node SCRIPT_ENTITY_LOOKUP_NODE;
struct script_entity_lookup_node {
    ENT_FIELD *field;
    SCRIPT_ENTITY_LOOKUP_NODE *next;
};

typedef struct script_entity_list_cache SCRIPT_ENTITY_LIST_CACHE;
struct script_entity_list_cache {
    ENT_FIELD *list;
    SCRIPT_ENTITY_LOOKUP_NODE *buckets[SCRIPT_ENTITY_HASH_SIZE];
};

typedef struct script_ifcheck_lookup_node SCRIPT_IFCHECK_LOOKUP_NODE;
struct script_ifcheck_lookup_node {
    int index;
    SCRIPT_IFCHECK_LOOKUP_NODE *next;
};

static SCRIPT_ENTITY_LIST_CACHE script_entity_list_caches[SCRIPT_ENTITY_LIST_CACHE_MAX];
static int script_entity_list_cache_count = 0;
static bool script_entity_list_cache_full_logged = false;
static SCRIPT_IFCHECK_LOOKUP_NODE *script_ifcheck_buckets[SCRIPT_IFCHECK_HASH_SIZE];
static bool script_ifcheck_cache_built = false;
static bool script_ifcheck_cache_failed = false;

typedef struct script_lookup_profile_data SCRIPT_LOOKUP_PROFILE;
struct script_lookup_profile_data {
    unsigned long entity_calls;
    unsigned long entity_hash_hits;
    unsigned long entity_linear_fallbacks;
    unsigned long entity_probe_steps;
    unsigned long entity_hash_calls;
    unsigned long entity_linear_calls;
    unsigned long long entity_hash_time_ns;
    unsigned long long entity_linear_time_ns;

    unsigned long ifcheck_calls;
    unsigned long ifcheck_hash_hits;
    unsigned long ifcheck_linear_fallbacks;
    unsigned long ifcheck_probe_steps;
    unsigned long ifcheck_hash_calls;
    unsigned long ifcheck_linear_calls;
    unsigned long long ifcheck_hash_time_ns;
    unsigned long long ifcheck_linear_time_ns;
};

static SCRIPT_LOOKUP_PROFILE script_lookup_profile = {0};

static const char *script_type_name(int type)
{
    switch (type) {
    case PRG_MPROG: return "mprog";
    case PRG_OPROG: return "oprog";
    case PRG_RPROG: return "rprog";
    case PRG_TPROG: return "tprog";
    case PRG_APROG: return "aprog";
    case PRG_IPROG: return "iprog";
    case PRG_DPROG: return "dprog";
    case PRG_QPROG: return "qprog";
    case PRG_EPROG: return "eprog";
    default: return "unknown";
    }
}

static const char *reset_command_name(char cmd)
{
    switch (cmd) {
    case '*': return "comment";
    case 'M': return "spawn_mobile";
    case 'O': return "place_object";
    case 'P': return "put_in_container";
    case 'G': return "give_to_mobile";
    case 'E': return "equip_to_mobile";
    case 'D': return "set_door_state";
    case 'R': return "randomize_exits";
    case 'S': return "stop";
    default: return "unknown";
    }
}

static unsigned long audit_signature_hash(const char *text)
{
    unsigned long h = 5381;
    const unsigned char *p = (const unsigned char *)text;

    while (p && *p)
        h = ((h << 5) + h) + *p++;

    return h;
}

static SCRIPT_RUNTIME_ERROR *audit_find_entry_by_signature(const char *signature)
{
    int i;

    if (IS_NULLSTR(signature))
        return NULL;

    for (i = 0; i < SCRIPT_RUNTIME_ERROR_MAX; i++) {
        SCRIPT_RUNTIME_ERROR *entry = &script_runtime_errors[i];
        if (entry->active && !str_cmp(entry->signature, signature))
            return entry;
    }

    return NULL;
}

static SCRIPT_RUNTIME_ERROR *audit_find_entry_by_id(int id)
{
    int i;
    for (i = 0; i < SCRIPT_RUNTIME_ERROR_MAX; i++) {
        SCRIPT_RUNTIME_ERROR *entry = &script_runtime_errors[i];
        if (entry->active && entry->id == id)
            return entry;
    }
    return NULL;
}

static SCRIPT_RUNTIME_ERROR *audit_alloc_entry(void)
{
    int i;
    SCRIPT_RUNTIME_ERROR *oldest = NULL;

    for (i = 0; i < SCRIPT_RUNTIME_ERROR_MAX; i++) {
        SCRIPT_RUNTIME_ERROR *entry = &script_runtime_errors[i];
        if (!entry->active)
            return entry;

        if (!oldest || entry->last_touch < oldest->last_touch)
            oldest = entry;
    }

    return oldest;
}

static int audit_entry_compare_touch_desc(const void *a, const void *b)
{
    SCRIPT_RUNTIME_ERROR * const *ea = (SCRIPT_RUNTIME_ERROR * const *)a;
    SCRIPT_RUNTIME_ERROR * const *eb = (SCRIPT_RUNTIME_ERROR * const *)b;

    if ((*ea)->last_touch < (*eb)->last_touch) return 1;
    if ((*ea)->last_touch > (*eb)->last_touch) return -1;
    return 0;
}

static int audit_collect_sorted_entries(SCRIPT_RUNTIME_ERROR **out, int max_out)
{
    int i;
    int count = 0;

    if (!out || max_out <= 0)
        return 0;

    for (i = 0; i < SCRIPT_RUNTIME_ERROR_MAX && count < max_out; i++) {
        SCRIPT_RUNTIME_ERROR *entry = &script_runtime_errors[i];
        if (entry->active)
            out[count++] = entry;
    }

    if (count > 1)
        qsort(out, count, sizeof(SCRIPT_RUNTIME_ERROR *), audit_entry_compare_touch_desc);

    return count;
}

static void audit_entry_add_occurrence(SCRIPT_RUNTIME_ERROR *entry)
{
    AUDIT_OCCURRENCE *occ;

    if (!entry)
        return;

    occ = &entry->history[entry->history_next];
    memset(occ, 0, sizeof(*occ));
    occ->when = entry->when;
    snprintf(occ->caller, sizeof(occ->caller), "%s", entry->caller);
    snprintf(occ->host, sizeof(occ->host), "%s", entry->host);
    snprintf(occ->location, sizeof(occ->location), "%s", entry->location);
    snprintf(occ->message, sizeof(occ->message), "%s", entry->message);

    entry->history_next = (entry->history_next + 1) % AUDIT_HISTORY_MAX;
    if (entry->history_count < AUDIT_HISTORY_MAX)
        entry->history_count++;
}

static void audit_entry_update_redis_count(SCRIPT_RUNTIME_ERROR *entry)
{
    long redis_total;

    if (!entry || IS_NULLSTR(entry->signature))
        return;

    if (redis_audit_error_increment(entry->signature)) {
        redis_total = redis_audit_error_get_count(entry->signature);
        if (redis_total >= 0)
            entry->redis_count = redis_total;
    }
}

static bool audit_entry_matches_current_location(const SCRIPT_RUNTIME_ERROR *entry, ROOM_INDEX_DATA *room)
{
    if (!entry || !room || !entry->has_room)
        return false;

    if (room->wilds) {
        return entry->is_wilds
            && entry->wilds_uid == room->wilds->uid
            && entry->wilds_x == room->x
            && entry->wilds_y == room->y;
    }

    return !entry->is_wilds
        && entry->room_area_uid == (room->area ? room->area->uid : 0)
        && entry->room_vnum == room->vnum;
}

static bool audit_entry_matches_area(const SCRIPT_RUNTIME_ERROR *entry, long area_uid)
{
    if (!entry || area_uid <= 0)
        return false;

    if (entry->area_uid == area_uid)
        return true;

    if (entry->host_area_uid == area_uid)
        return true;

    if (!entry->is_wilds && entry->room_area_uid == area_uid)
        return true;

    return false;
}

static bool audit_is_valid_type_filter(const char *type)
{
    if (IS_NULLSTR(type))
        return false;

    return !str_cmp(type, "all")
        || !str_cmp(type, "scripts")
        || !str_cmp(type, "resets")
        || !str_cmp(type, "spawn_mobile")
        || !str_cmp(type, "place_object")
        || !str_cmp(type, "put_in_container")
        || !str_cmp(type, "give_to_mobile")
        || !str_cmp(type, "equip_to_mobile")
        || !str_cmp(type, "set_door_state")
        || !str_cmp(type, "randomize_exits")
        || !str_cmp(type, "mprog")
        || !str_cmp(type, "oprog")
        || !str_cmp(type, "rprog")
        || !str_cmp(type, "tprog")
        || !str_cmp(type, "aprog")
        || !str_cmp(type, "iprog")
        || !str_cmp(type, "dprog")
        || !str_cmp(type, "qprog")
        || !str_cmp(type, "eprog");
}

static ROOM_INDEX_DATA *script_info_location(SCRIPT_VARINFO *info)
{
    if (!info)
        return NULL;

    if (info->location)
        return info->location;
    if (info->room)
        return info->room;
    if (info->mob)
        return info->mob->in_room;
    if (info->obj)
        return obj_room(info->obj);
    if (info->token)
        return token_room(info->token);

    return NULL;
}

static void script_build_runtime_context(
    SCRIPT_VARINFO *info,
    CHAR_DATA *mob,
    OBJ_DATA *obj,
    ROOM_INDEX_DATA *room,
    TOKEN_DATA *token,
    AREA_DATA *area,
    INSTANCE *instance,
    DUNGEON *dungeon,
    CHAR_DATA *ch,
    CHAR_DATA *vch,
    CHAR_DATA *vch2,
    CHAR_DATA *rch,
    TOKEN_DATA *tok,
    const char *phrase,
    const char *trigger,
    int trigger_type)
{
    if (!info)
        return;

    memset(info, 0, sizeof(*info));

    info->mob = mob;
    info->obj = obj;
    info->room = room;
    info->token = token;
    info->area = area;
    info->instance = instance;
    info->dungeon = dungeon;
    info->ch = ch;
    info->vch = vch;
    info->vch2 = vch2;
    info->rch = rch;
    info->tok = tok;
    info->trigger_type = trigger_type;

    if (!room && ch && ch->in_room)
        info->location = ch->in_room;
    else
        info->location = room;

    if (!IS_NULLSTR(phrase))
        snprintf(info->phrase, sizeof(info->phrase), "%s", phrase);

    if (!IS_NULLSTR(trigger))
        snprintf(info->trigger, sizeof(info->trigger), "%s", trigger);
}

static void script_format_location(SCRIPT_VARINFO *info, char *buf, size_t buf_size)
{
    ROOM_INDEX_DATA *room;

    if (!buf || buf_size == 0)
        return;

    room = script_info_location(info);
    if (!room) {
        snprintf(buf, buf_size, "unknown");
        return;
    }

    if (room->wilds)
        snprintf(buf, buf_size, "wilds:%s (%ld,%ld,%ld)",
            room->wilds->name ? room->wilds->name : "(unknown)",
            room->wilds->uid,
            room->x,
            room->y);
    else
        snprintf(buf, buf_size, "room:%ld#%ld (%s)",
            room->area ? room->area->uid : 0,
            room->vnum,
            room->name ? room->name : "(unnamed)");
}

static void script_capture_location_fields(SCRIPT_VARINFO *info, SCRIPT_RUNTIME_ERROR *entry)
{
    ROOM_INDEX_DATA *room;

    if (!entry)
        return;

    room = script_info_location(info);
    if (!room)
        return;

    entry->has_room = true;
    if (room->wilds) {
        entry->is_wilds = true;
        entry->wilds_uid = room->wilds->uid;
        entry->wilds_x = room->x;
        entry->wilds_y = room->y;
    } else {
        entry->is_wilds = false;
        entry->room_area_uid = room->area ? room->area->uid : 0;
        entry->room_vnum = room->vnum;
    }
}

static void reset_capture_location_fields(ROOM_INDEX_DATA *room, SCRIPT_RUNTIME_ERROR *entry)
{
    if (!room || !entry)
        return;

    entry->has_room = true;
    if (room->wilds) {
        entry->is_wilds = true;
        entry->wilds_uid = room->wilds->uid;
        entry->wilds_x = room->x;
        entry->wilds_y = room->y;
    } else {
        entry->is_wilds = false;
        entry->room_area_uid = room->area ? room->area->uid : 0;
        entry->room_vnum = room->vnum;
    }
}

static void script_format_caller(SCRIPT_VARINFO *info, char *buf, size_t buf_size)
{
    char trigger_preview[256];

    if (!buf || buf_size == 0)
        return;

    trigger_preview[0] = '\0';

    if (!info) {
        snprintf(buf, buf_size, "unknown");
        return;
    }

    if (info->ch) {
        if (IS_NPC(info->ch))
            snprintf(buf, buf_size, "mob:%s (%ld)", HANDLE(info->ch), (long)VNUM(info->ch));
        else
            snprintf(buf, buf_size, "player:%s", HANDLE(info->ch));
        return;
    }

    if (info->vch) {
        if (IS_NPC(info->vch))
            snprintf(buf, buf_size, "victim-mob:%s (%ld)", HANDLE(info->vch), (long)VNUM(info->vch));
        else
            snprintf(buf, buf_size, "victim-player:%s", HANDLE(info->vch));
        return;
    }

    if (info->trigger_type > 0) {
        const char *trigger_type_name = trigger_name(info->trigger_type);
        bool valid_trigger_name = (trigger_type_name && str_cmp(trigger_type_name, "INVALID"));

        if (!IS_NULLSTR(info->trigger)) {
            snprintf(trigger_preview, sizeof(trigger_preview), "%.240s", info->trigger);
            if (valid_trigger_name)
                snprintf(buf, buf_size, "trigger:%d %s (%s)", info->trigger_type, trigger_type_name, trigger_preview);
            else
                snprintf(buf, buf_size, "trigger:%d (%s)", info->trigger_type, trigger_preview);
        } else {
            if (valid_trigger_name)
                snprintf(buf, buf_size, "trigger:%d %s", info->trigger_type, trigger_type_name);
            else
                snprintf(buf, buf_size, "trigger:%d", info->trigger_type);
        }
        return;
    }

    snprintf(buf, buf_size, "unknown");
}

static bool audit_caller_is_trigger(const char *caller)
{
    return caller && !str_prefix("trigger:", caller);
}

static const char *audit_caller_trigger_text(const char *caller)
{
    if (!audit_caller_is_trigger(caller))
        return NULL;

    caller += 8;
    while (*caller && isspace(*caller))
        caller++;

    return caller;
}

static void script_format_host(SCRIPT_VARINFO *info, char *buf, size_t buf_size)
{
    char phrase_preview[256];

    if (!buf || buf_size == 0)
        return;

    phrase_preview[0] = '\0';

    if (!info) {
        snprintf(buf, buf_size, "unknown");
        return;
    }

    if (info->mob) {
        snprintf(buf, buf_size, "mob:%s (%ld)", HANDLE(info->mob), (long)VNUM(info->mob));
        return;
    }

    if (info->obj) {
        snprintf(buf, buf_size, "obj:%s (%ld)",
            info->obj->short_descr ? info->obj->short_descr : "(unnamed)",
            (long)VNUM(info->obj));
        return;
    }

    if (info->room) {
        snprintf(buf, buf_size, "room:%ld#%ld", info->room->area ? info->room->area->uid : 0, info->room->vnum);
        return;
    }

    if (info->token) {
        snprintf(buf, buf_size, "token:%s (%ld)",
            info->token->name ? info->token->name : "(unnamed)",
            (long)VNUM(info->token));
        return;
    }

    if (info->area) {
        snprintf(buf, buf_size, "area:%s (%ld)",
            info->area->name ? info->area->name : "(unnamed)",
            info->area->uid);
        return;
    }

    if (info->instance) {
        snprintf(buf, buf_size, "instance");
        return;
    }

    if (info->dungeon) {
        snprintf(buf, buf_size, "dungeon:%s",
            (info->dungeon->index && info->dungeon->index->name)
                ? info->dungeon->index->name
                : "(unnamed)");
        return;
    }

    if (info->quest) {
        snprintf(buf, buf_size, "quest");
        return;
    }

    if (!IS_NULLSTR(info->phrase)) {
        snprintf(phrase_preview, sizeof(phrase_preview), "%.248s", info->phrase);
        snprintf(buf, buf_size, "phrase:%s", phrase_preview);
        return;
    }

    if (script_info_location(info)) {
        ROOM_INDEX_DATA *loc = script_info_location(info);

        if (loc->wilds)
            snprintf(buf, buf_size, "wilds:%ld (%ld,%ld)",
                loc->wilds->uid,
                loc->x,
                loc->y);
        else
            snprintf(buf, buf_size, "room:%ld#%ld",
                loc->area ? loc->area->uid : 0,
                loc->vnum);
        return;
    }

    snprintf(buf, buf_size, "unknown");
}

static void script_fill_host_ref(SCRIPT_RUNTIME_ERROR *entry, SCRIPT_VARINFO *info)
{
    if (!entry || !info)
        return;

    entry->host_kind = AUDIT_HOST_UNKNOWN;
    entry->host_area_uid = 0;
    entry->host_vnum = 0;

    if (info->mob && info->mob->pIndexData) {
        entry->host_kind = AUDIT_HOST_MOB;
        entry->host_area_uid = info->mob->pIndexData->area ? info->mob->pIndexData->area->uid : 0;
        entry->host_vnum = VNUM(info->mob);
        return;
    }

    if (info->obj && info->obj->pIndexData) {
        entry->host_kind = AUDIT_HOST_OBJ;
        entry->host_area_uid = info->obj->pIndexData->area ? info->obj->pIndexData->area->uid : 0;
        entry->host_vnum = info->obj->pIndexData->vnum;
        return;
    }

    if (info->token && info->token->pIndexData) {
        entry->host_kind = AUDIT_HOST_TOKEN;
        entry->host_area_uid = info->token->pIndexData->area ? info->token->pIndexData->area->uid : 0;
        entry->host_vnum = info->token->pIndexData->vnum;
        return;
    }

    if (info->room) {
        entry->host_kind = AUDIT_HOST_ROOM;
        entry->host_area_uid = info->room->area ? info->room->area->uid : 0;
        entry->host_vnum = info->room->vnum;
        return;
    }

    if (info->area) {
        entry->host_kind = AUDIT_HOST_AREA;
        entry->host_area_uid = info->area->uid;
        return;
    }
}

static void audit_set_signature(SCRIPT_RUNTIME_ERROR *entry)
{
    char sig[SCRIPT_RUNTIME_ERROR_TEXT_MAX];

    if (!entry)
        return;

    snprintf(sig, sizeof(sig), "%d|%s|%ld|%d|%d|%.160s",
        entry->category,
        entry->type,
        entry->area_uid,
        entry->script_vnum,
        entry->line,
        entry->message);
    snprintf(entry->signature, sizeof(entry->signature), "%s", sig);
}

static SCRIPT_RUNTIME_ERROR *audit_record_event(SCRIPT_RUNTIME_ERROR *scratch)
{
    SCRIPT_RUNTIME_ERROR *entry;

    if (!scratch)
        return NULL;

    audit_set_signature(scratch);
    entry = audit_find_entry_by_signature(scratch->signature);

    if (!entry) {
        entry = audit_alloc_entry();
        if (!entry)
            return NULL;

        memset(entry, 0, sizeof(*entry));
        entry->active = true;
        entry->id = script_runtime_error_next_id++;
        entry->first_when = scratch->when;
    }

    entry->when = scratch->when;
    entry->last_touch = ++script_runtime_error_touch;
    entry->local_count++;
    entry->category = scratch->category;
    entry->script_type = scratch->script_type;
    entry->area_uid = scratch->area_uid;
    entry->script_vnum = scratch->script_vnum;
    entry->line = scratch->line;
    entry->has_room = scratch->has_room;
    entry->is_wilds = scratch->is_wilds;
    entry->room_area_uid = scratch->room_area_uid;
    entry->room_vnum = scratch->room_vnum;
    entry->wilds_uid = scratch->wilds_uid;
    entry->wilds_x = scratch->wilds_x;
    entry->wilds_y = scratch->wilds_y;
    entry->host_kind = scratch->host_kind;
    entry->host_area_uid = scratch->host_area_uid;
    entry->host_vnum = scratch->host_vnum;
    entry->reset_command = scratch->reset_command;

    snprintf(entry->type, sizeof(entry->type), "%s", scratch->type);
    snprintf(entry->caller, sizeof(entry->caller), "%s", scratch->caller);
    snprintf(entry->host, sizeof(entry->host), "%s", scratch->host);
    snprintf(entry->location, sizeof(entry->location), "%s", scratch->location);
    snprintf(entry->message, sizeof(entry->message), "%s", scratch->message);
    snprintf(entry->signature, sizeof(entry->signature), "%s", scratch->signature);

    audit_entry_add_occurrence(entry);
    audit_entry_update_redis_count(entry);
    return entry;
}

static void script_runtime_error_push(SCRIPT_DATA *script, int line, const char *message, SCRIPT_VARINFO *info)
{
    SCRIPT_RUNTIME_ERROR scratch;

    if (!script || IS_NULLSTR(message))
        return;

    memset(&scratch, 0, sizeof(scratch));
    scratch.when = current_time;
    scratch.category = AUDIT_ERROR_SCRIPT;
    scratch.script_type = script->type;
    scratch.area_uid = script->area ? script->area->uid : 0;
    scratch.script_vnum = script->vnum;
    scratch.line = line;
    snprintf(scratch.type, sizeof(scratch.type), "%s", script_type_name(script->type));

    script_format_caller(info, scratch.caller, sizeof(scratch.caller));
    script_format_host(info, scratch.host, sizeof(scratch.host));
    script_format_location(info, scratch.location, sizeof(scratch.location));
    script_capture_location_fields(info, &scratch);
    script_fill_host_ref(&scratch, info);
    snprintf(scratch.message, sizeof(scratch.message), "%s", message);

    (void)audit_record_event(&scratch);
}

void audit_log_reset_error(ROOM_INDEX_DATA *room, RESET_DATA *reset, const char *message)
{
    SCRIPT_RUNTIME_ERROR scratch;

    if (IS_NULLSTR(message))
        return;

    memset(&scratch, 0, sizeof(scratch));
    scratch.when = current_time;
    scratch.category = AUDIT_ERROR_RESET;
    scratch.script_type = 0;
    scratch.area_uid = room && room->area ? room->area->uid : 0;
    scratch.script_vnum = room ? room->vnum : 0;
    scratch.line = 0;
    scratch.host_kind = AUDIT_HOST_RESET;
    scratch.reset_command = reset ? reset->command : '?';
    snprintf(scratch.type, sizeof(scratch.type), "%s", reset_command_name(scratch.reset_command));
    snprintf(scratch.caller, sizeof(scratch.caller), "reset_engine");
    snprintf(scratch.host, sizeof(scratch.host), "reset:%s", reset_command_name(scratch.reset_command));

    if (room) {
        if (room->wilds)
            snprintf(scratch.location, sizeof(scratch.location), "wilds:%s (%ld,%ld,%ld)",
                room->wilds->name ? room->wilds->name : "(unknown)",
                room->wilds->uid,
                room->x,
                room->y);
        else
            snprintf(scratch.location, sizeof(scratch.location), "room:%ld#%ld (%s)",
                room->area ? room->area->uid : 0,
                room->vnum,
                room->name ? room->name : "(unnamed)");
    } else {
        snprintf(scratch.location, sizeof(scratch.location), "unknown");
    }

    reset_capture_location_fields(room, &scratch);
    if (room && room->area) {
        scratch.host_area_uid = room->area->uid;
        scratch.host_vnum = room->vnum;
    }

    snprintf(scratch.message, sizeof(scratch.message), "%s", message);
    (void)audit_record_event(&scratch);
}

static void script_log_runtime_error_context(SCRIPT_DATA *script, int line, const char *message, SCRIPT_VARINFO *info)
{
    script_append_runtime_logf(script, line, "%s", message ? message : "(null)");
    script_runtime_error_push(script, line, message ? message : "(null)", info);
}

static void script_set_log_text(char **target, const char *text)
{
    size_t len;
    const char *source;

    if (!target)
        return;

    if (*target) {
        free_string(*target);
        *target = NULL;
    }

    if (!text || !text[0])
        return;

    len = strlen(text);
    source = text;

    if (len > SCRIPT_LOG_MAX_LEN)
        source = text + (len - SCRIPT_LOG_MAX_LEN);

    *target = str_dup(source);
}

static void script_append_runtime_logf(SCRIPT_DATA *script, int line, const char *fmt, ...)
{
    char entry[MSL];
    char combined[SCRIPT_LOG_MAX_LEN + MSL + 8];
    const char *source;
    size_t len;
    va_list args;

    if (!script || IS_NULLSTR(fmt))
        return;

    va_start(args, fmt);
    vsnprintf(entry, sizeof(entry), fmt, args);
    va_end(args);

    if (line > 0)
        snprintf(combined, sizeof(combined), "[%ld#%d line %d] %s\n\r",
            script->area ? script->area->uid : 0,
            script->vnum,
            line,
            entry);
    else
        snprintf(combined, sizeof(combined), "[%ld#%d] %s\n\r",
            script->area ? script->area->uid : 0,
            script->vnum,
            entry);

    if (script->last_runtime_log && script->last_runtime_log[0]) {
        char merged[SCRIPT_LOG_MAX_LEN + MSL + 8];
        snprintf(merged, sizeof(merged), "%s%s", script->last_runtime_log, combined);
        len = strlen(merged);
        source = merged;
        if (len > SCRIPT_LOG_MAX_LEN)
            source = merged + (len - SCRIPT_LOG_MAX_LEN);
        script_set_log_text(&script->last_runtime_log, source);
    } else {
        script_set_log_text(&script->last_runtime_log, combined);
    }

    script->last_runtime_time = current_time;
}

void script_log_runtime_error(SCRIPT_DATA *script, int line, const char *message)
{
    script_log_runtime_error_context(script, line, message, NULL);
}

void do_error(CHAR_DATA *ch, char *argument)
{
    BUFFER *buffer;
    char line_buf[MSL];
    char time_buf[64];
    char area_parse[MSL];
    char trailing_type[MIL];
    char arg_scope[MIL];
    char arg_filter[MIL];
    char arg_type[MIL];
    const char *type_filter_arg = NULL;
    const char *base_command = NULL;
    bool location_only = false;
    bool area_only = false;
    AREA_DATA *filter_area = NULL;
    long filter_area_uid = 0;
    bool filter_any_type = true;
    int filter_category = 0;
    int shown = 0;
    SCRIPT_RUNTIME_ERROR *entries[SCRIPT_RUNTIME_ERROR_MAX];
    int entry_count;
    int i;

    if (!ch)
        return;

    base_command = get_invoked_command_name(ch);
    if (IS_NULLSTR(base_command))
        base_command = "error";

    argument = one_argument(argument, arg_scope);

    if (!str_cmp(arg_scope, "area") || !str_cmp(arg_scope, "zone")) {
        area_only = true;

        while (*argument && isspace(*argument))
            argument++;

        if (IS_NULLSTR(argument)) {
            printf_to_char(ch, "Syntax: %s area <name|uid> [type]\n\r", base_command);
            return;
        }

        arg_filter[0] = '\0';
        arg_type[0] = '\0';
        trailing_type[0] = '\0';

        if (*argument == '\'' || *argument == '"') {
            argument = one_argument(argument, arg_filter);
            argument = one_argument(argument, arg_type);
        } else {
            char *last_space;
            char *tail;

            snprintf(area_parse, sizeof(area_parse), "%s", argument);
            last_space = strrchr(area_parse, ' ');

            if (last_space) {
                snprintf(trailing_type, sizeof(trailing_type), "%s", last_space + 1);

                if (audit_is_valid_type_filter(trailing_type)) {
                    *last_space = '\0';
                    while (*area_parse && isspace(*area_parse))
                        memmove(area_parse, area_parse + 1, strlen(area_parse));

                    tail = area_parse + strlen(area_parse) - 1;
                    while (tail >= area_parse && isspace(*tail)) {
                        *tail = '\0';
                        tail--;
                    }

                    snprintf(arg_filter, sizeof(arg_filter), "%.511s", area_parse);
                    snprintf(arg_type, sizeof(arg_type), "%s", trailing_type);
                } else {
                    snprintf(arg_filter, sizeof(arg_filter), "%.511s", area_parse);
                }
            } else {
                snprintf(arg_filter, sizeof(arg_filter), "%.511s", area_parse);
            }
        }

        if (IS_NULLSTR(arg_filter)) {
            printf_to_char(ch, "Syntax: %s area <name|uid> [type]\n\r", base_command);
            return;
        }

        if (is_number(arg_filter))
            filter_area = get_area_from_uid(atol(arg_filter));
        else
            filter_area = find_area(arg_filter);

        if (!filter_area) {
            send_to_char("No area found with that name/uid.\n\r", ch);
            return;
        }

        filter_area_uid = filter_area->uid;
        type_filter_arg = arg_type;
    } else {
    argument = one_argument(argument, arg_filter);
    argument = one_argument(argument, arg_type);
    }

    if (!str_cmp(arg_scope, "view")) {
        SCRIPT_RUNTIME_ERROR *entry;
        int view_id;
        int h;

        if (!is_number(arg_filter)) {
            printf_to_char(ch, "Syntax: %s view <id>\n\r", base_command);
            return;
        }

        view_id = atoi(arg_filter);
        entry = audit_find_entry_by_id(view_id);
        if (!entry) {
            send_to_char("No error found with that id.\n\r", ch);
            return;
        }

        buffer = new_buf();
        snprintf(line_buf, sizeof(line_buf),
            "Error #%d | %s | local=%ld redis=%ld | sig=%08lx\n\r",
            entry->id,
            entry->type,
            entry->local_count,
            entry->redis_count,
            audit_signature_hash(entry->signature));
        add_buf(buffer, line_buf);
        add_buf(buffer, "Occurrences (newest first):\n\r");

        for (h = 0; h < entry->history_count; h++) {
            int hid = (entry->history_next - 1 - h + AUDIT_HISTORY_MAX) % AUDIT_HISTORY_MAX;
            AUDIT_OCCURRENCE *occ = &entry->history[hid];
            struct tm *tm_info = localtime(&occ->when);
            const char *trigger_text = audit_caller_trigger_text(occ->caller);
            if (!(tm_info && strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", tm_info) > 0))
                snprintf(time_buf, sizeof(time_buf), "%ld", (long)occ->when);

            if (trigger_text) {
                snprintf(line_buf, sizeof(line_buf),
                    "[%2d] %s | trigger: %s | host: %s\n\r"
                    "     location: %s\n\r"
                    "     message: %s\n\r",
                    h + 1,
                    time_buf,
                    trigger_text,
                    occ->host,
                    occ->location,
                    occ->message);
            } else {
                snprintf(line_buf, sizeof(line_buf),
                    "[%2d] %s | caller: %s | host: %s\n\r"
                    "     location: %s\n\r"
                    "     message: %s\n\r",
                    h + 1,
                    time_buf,
                    occ->caller,
                    occ->host,
                    occ->location,
                    occ->message);
            }
            add_buf(buffer, line_buf);
        }

        page_to_char(buffer->string, ch);
        free_buf(buffer);
        return;
    }

    if (IS_NULLSTR(arg_scope)) {
        printf_to_char(ch, "Syntax: %s <location|all> [type]\n\r", base_command);
        printf_to_char(ch, "        %s area <name|uid> [type]\n\r", base_command);
        printf_to_char(ch, "        %s view <id>\n\r", base_command);
        send_to_char("Types: all, scripts, resets, spawn_mobile, place_object, put_in_container, give_to_mobile, equip_to_mobile, set_door_state, randomize_exits, mprog, oprog, rprog, tprog, aprog, iprog, dprog, qprog, eprog\n\r", ch);
        return;
    }

    if (!str_cmp(arg_scope, "location")) {
        location_only = true;
        type_filter_arg = arg_filter;
    } else if (!str_cmp(arg_scope, "all")) {
        location_only = false;
        type_filter_arg = arg_filter;
    } else if (area_only) {
        ;
    } else {
        printf_to_char(ch, "Syntax: %s <location|all> [type]\n\r", base_command);
        printf_to_char(ch, "        %s area <name|uid> [type]\n\r", base_command);
        return;
    }

    if (type_filter_arg && !IS_NULLSTR(type_filter_arg)) {
        filter_any_type = false;

        if (!str_cmp(type_filter_arg, "all"))
            filter_any_type = true;
        else if (!str_cmp(type_filter_arg, "scripts"))
            filter_category = AUDIT_ERROR_SCRIPT;
        else if (!str_cmp(type_filter_arg, "resets"))
            filter_category = AUDIT_ERROR_RESET;
    }

    entry_count = audit_collect_sorted_entries(entries, SCRIPT_RUNTIME_ERROR_MAX);
    if (entry_count == 0) {
        send_to_char("No audit errors recorded.\n\r", ch);
        return;
    }

    buffer = new_buf();
    if (area_only)
        snprintf(line_buf, sizeof(line_buf), "Recent audit errors in area %s (%ld) (newest first):\n\r",
            filter_area->name ? filter_area->name : "(unnamed)",
            filter_area_uid);
    else
        snprintf(line_buf, sizeof(line_buf), "Recent audit errors (newest first):\n\r");
    add_buf(buffer, line_buf);

    for (i = 0; i < entry_count; i++) {
        SCRIPT_RUNTIME_ERROR *entry = entries[i];
        struct tm *tm_info;
        AREA_DATA *host_area;
        AREA_DATA *room_area;
        ROOM_INDEX_DATA *room;
        MOB_INDEX_DATA *mob_index;
        OBJ_INDEX_DATA *obj_index;
        TOKEN_INDEX_DATA *token_index;
        const char *trigger_text;
        char label[MIL];
        char cmd1[MIL], cmd2[MIL];
        mxp_cmd_hint_t items[3];

        if (location_only && !audit_entry_matches_current_location(entry, ch->in_room))
            continue;

        if (area_only && !audit_entry_matches_area(entry, filter_area_uid))
            continue;

        if (!filter_any_type) {
            if (filter_category != 0) {
                if (entry->category != filter_category)
                    continue;
            } else if (str_cmp(entry->type, type_filter_arg)) {
                continue;
            }
        }

        tm_info = localtime(&entry->when);
        if (!(tm_info && strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", tm_info) > 0))
            snprintf(time_buf, sizeof(time_buf), "%ld", (long)entry->when);

        trigger_text = audit_caller_trigger_text(entry->caller);

        snprintf(cmd1, sizeof(cmd1), "%s view %d", base_command, entry->id);
        snprintf(label, sizeof(label), "[#%d]", entry->id);
        mxp_command_link(ch->desc, buffer, cmd1, "View this error entry", label);

        if (trigger_text) {
            snprintf(line_buf, sizeof(line_buf),
            " %s | %s | %s %ld#%d line %d | local=%ld redis=%ld\n\r"
                "     trigger: %s\n\r"
                "     host: ",
                time_buf,
                entry->category == AUDIT_ERROR_RESET ? "reset" : "script",
                entry->type,
                entry->area_uid,
                entry->script_vnum,
                entry->line,
                entry->local_count,
                entry->redis_count,
                trigger_text);
        } else {
            snprintf(line_buf, sizeof(line_buf),
                " %s | %s | %s %ld#%d line %d | local=%ld redis=%ld\n\r"
                "     caller: %s\n\r"
                "     host: ",
                time_buf,
                entry->category == AUDIT_ERROR_RESET ? "reset" : "script",
                entry->type,
                entry->area_uid,
                entry->script_vnum,
                entry->line,
                entry->local_count,
                entry->redis_count,
                entry->caller);
        }
        add_buf(buffer, line_buf);

        host_area = get_area_from_uid(entry->host_area_uid);
        if (entry->host_kind == AUDIT_HOST_ROOM && host_area && entry->host_vnum > 0) {
            room = get_room_index(host_area, entry->host_vnum);
            if (room) {
                snprintf(label, sizeof(label), "room (%ld#%ld, %s)",
                    entry->host_area_uid,
                    entry->host_vnum,
                    room->name ? room->name : "(unnamed)");
                mxp_room_link(ch->desc, buffer, room, label);
            }
            else
                add_buf(buffer, entry->host);
        } else if (entry->host_kind == AUDIT_HOST_MOB && host_area && entry->host_vnum > 0) {
            mob_index = get_mob_index(host_area, entry->host_vnum);
            if (mob_index) {
                snprintf(label, sizeof(label), "mob (%ld#%ld, %s)",
                    entry->host_area_uid,
                    entry->host_vnum,
                    !IS_NULLSTR(mob_index->list_name) ? mob_index->list_name :
                    (!IS_NULLSTR(mob_index->short_descr) ? mob_index->short_descr :
                    (!IS_NULLSTR(mob_index->player_name) ? mob_index->player_name : "(unnamed)")));
                snprintf(cmd1, sizeof(cmd1), "mshow %ld#%ld", entry->host_area_uid, entry->host_vnum);
                snprintf(cmd2, sizeof(cmd2), "medit %ld#%ld", entry->host_area_uid, entry->host_vnum);
                items[0].cmd = cmd1; items[0].hint = "Show mobile";
                items[1].cmd = cmd2; items[1].hint = "Edit mobile";
                mxp_link_multi(ch->desc, buffer, label, items, 2);
            } else add_buf(buffer, entry->host);
        } else if (entry->host_kind == AUDIT_HOST_OBJ && host_area && entry->host_vnum > 0) {
            obj_index = get_obj_index(host_area, entry->host_vnum);
            if (obj_index) {
                snprintf(label, sizeof(label), "object (%ld#%ld, %s)",
                    entry->host_area_uid,
                    entry->host_vnum,
                    !IS_NULLSTR(obj_index->list_name) ? obj_index->list_name :
                    (!IS_NULLSTR(obj_index->short_descr) ? obj_index->short_descr :
                    (!IS_NULLSTR(obj_index->name) ? obj_index->name : "(unnamed)")));
                mxp_obj_vnum_link(ch->desc, buffer, obj_index, label);
            }
            else
                add_buf(buffer, entry->host);
        } else if (entry->host_kind == AUDIT_HOST_TOKEN && host_area && entry->host_vnum > 0) {
            token_index = get_token_index(host_area, entry->host_vnum);
            if (token_index) {
                snprintf(label, sizeof(label), "token (%ld#%ld, %s)",
                    entry->host_area_uid,
                    entry->host_vnum,
                    !IS_NULLSTR(token_index->name) ? token_index->name : "(unnamed)");
                snprintf(cmd1, sizeof(cmd1), "tshow %ld#%ld", entry->host_area_uid, entry->host_vnum);
                snprintf(cmd2, sizeof(cmd2), "tpedit %ld#%ld", entry->host_area_uid, entry->host_vnum);
                items[0].cmd = cmd1; items[0].hint = "Show token";
                items[1].cmd = cmd2; items[1].hint = "Edit token";
                mxp_link_multi(ch->desc, buffer, label, items, 2);
            } else add_buf(buffer, entry->host);
        } else if (entry->host_kind == AUDIT_HOST_RESET) {
            add_buf(buffer, entry->host);
        } else {
            add_buf(buffer, entry->host);
        }
        add_buf(buffer, "\n\r     location: ");

        if (entry->has_room && !entry->is_wilds) {
            room_area = get_area_from_uid(entry->room_area_uid);
            room = room_area ? get_room_index(room_area, entry->room_vnum) : NULL;
            if (room) {
                snprintf(label, sizeof(label), "room (%ld#%ld, %s)",
                    entry->room_area_uid,
                    entry->room_vnum,
                    room->name ? room->name : "(unnamed)");
                mxp_room_link(ch->desc, buffer, room, label);
            }
            else
                add_buf(buffer, entry->location);
        } else if (entry->has_room && entry->is_wilds) {
            snprintf(label, sizeof(label), "wilds (%ld, %ld, %ld)", entry->wilds_uid, entry->wilds_x, entry->wilds_y);
            snprintf(cmd1, sizeof(cmd1), "goxy %ld %ld %ld", entry->wilds_x, entry->wilds_y, entry->wilds_uid);
            mxp_command_link(ch->desc, buffer, cmd1, "Goto wilderness coordinates", label);
        } else {
            add_buf(buffer, entry->location);
        }

        snprintf(line_buf, sizeof(line_buf), "\n\r     message: %s\n\r", entry->message);
        add_buf(buffer, line_buf);
        shown++;
    }

    if (shown == 0)
        add_buf(buffer, "No entries matched the requested filter.\n\r");

    page_to_char(buffer->string, ch);
    free_buf(buffer);
}

void do_scripterrors(CHAR_DATA *ch, char *argument)
{
    (void)argument;
    do_error(ch, "all scripts");
}

static unsigned long long script_profile_now_ns(void)
{
    struct timespec ts;

#if defined(CLOCK_MONOTONIC)
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return ((unsigned long long)ts.tv_sec * 1000000000ULL) + (unsigned long long)ts.tv_nsec;
#endif

#if defined(TIME_UTC)
    if(timespec_get(&ts, TIME_UTC) == TIME_UTC)
        return ((unsigned long long)ts.tv_sec * 1000000000ULL) + (unsigned long long)ts.tv_nsec;
#endif

    return 0ULL;
}

void script_lookup_profile_report(const char *tag)
{
    const char *label = IS_NULLSTR(tag) ? "runtime" : tag;
    double entity_hash_avg_us = 0.0;
    double entity_linear_avg_us = 0.0;
    double ifcheck_hash_avg_us = 0.0;
    double ifcheck_linear_avg_us = 0.0;

    if(script_lookup_profile.entity_hash_calls > 0)
        entity_hash_avg_us = ((double)script_lookup_profile.entity_hash_time_ns /
            (double)script_lookup_profile.entity_hash_calls) / 1000.0;
    if(script_lookup_profile.entity_linear_calls > 0)
        entity_linear_avg_us = ((double)script_lookup_profile.entity_linear_time_ns /
            (double)script_lookup_profile.entity_linear_calls) / 1000.0;
    if(script_lookup_profile.ifcheck_hash_calls > 0)
        ifcheck_hash_avg_us = ((double)script_lookup_profile.ifcheck_hash_time_ns /
            (double)script_lookup_profile.ifcheck_hash_calls) / 1000.0;
    if(script_lookup_profile.ifcheck_linear_calls > 0)
        ifcheck_linear_avg_us = ((double)script_lookup_profile.ifcheck_linear_time_ns /
            (double)script_lookup_profile.ifcheck_linear_calls) / 1000.0;

    pbugf(LOG_SCRIPTS,
        "Lookup profile[%s]: entity calls=%lu hash_hits=%lu fallback=%lu avg_probe=%.2f hash_avg=%.2fus linear_avg=%.2fus | ifcheck calls=%lu hash_hits=%lu fallback=%lu avg_probe=%.2f hash_avg=%.2fus linear_avg=%.2fus",
        label,
        script_lookup_profile.entity_calls,
        script_lookup_profile.entity_hash_hits,
        script_lookup_profile.entity_linear_fallbacks,
        (script_lookup_profile.entity_calls > 0)
            ? ((double)script_lookup_profile.entity_probe_steps / (double)script_lookup_profile.entity_calls)
            : 0.0,
        entity_hash_avg_us,
        entity_linear_avg_us,
        script_lookup_profile.ifcheck_calls,
        script_lookup_profile.ifcheck_hash_hits,
        script_lookup_profile.ifcheck_linear_fallbacks,
        (script_lookup_profile.ifcheck_calls > 0)
            ? ((double)script_lookup_profile.ifcheck_probe_steps / (double)script_lookup_profile.ifcheck_calls)
            : 0.0,
        ifcheck_hash_avg_us,
        ifcheck_linear_avg_us);
}

static unsigned int script_hash_ci(const char *name, unsigned int size)
{
    unsigned int hash = 5381;
    const unsigned char *ptr = (const unsigned char *)name;

    while(*ptr)
        hash = ((hash << 5) + hash) + (unsigned char)tolower(*ptr++);

    return hash % size;
}

static ENT_FIELD *entity_type_lookup_linear(char *name, ENT_FIELD *list)
{
    int i;

    if(!list || !name)
        return NULL;

    for(i = 0; list[i].name; i++)
        if(!str_cmp(name, list[i].name))
            return &list[i];

    return NULL;
}

static SCRIPT_ENTITY_LIST_CACHE *script_get_entity_list_cache(ENT_FIELD *list)
{
    int i;
    int count;

    for(i = 0; i < script_entity_list_cache_count; i++)
        if(script_entity_list_caches[i].list == list)
            return &script_entity_list_caches[i];

    if(script_entity_list_cache_count >= SCRIPT_ENTITY_LIST_CACHE_MAX) {
        if(!script_entity_list_cache_full_logged) {
            pbugf(LOG_SCRIPTS,
                "Entity lookup cache exhausted at %d distinct tables; using linear fallback.",
                SCRIPT_ENTITY_LIST_CACHE_MAX);
            script_entity_list_cache_full_logged = true;
        }
        return NULL;
    }

    SCRIPT_ENTITY_LIST_CACHE *cache = &script_entity_list_caches[script_entity_list_cache_count++];
    memset(cache, 0, sizeof(*cache));
    cache->list = list;

    for(count = 0; list[count].name; count++)
        ;

    for(i = count - 1; i >= 0; i--) {
        SCRIPT_ENTITY_LOOKUP_NODE *node;
        unsigned int bucket;

        node = alloc_mem(sizeof(*node));
        if(!node)
            return NULL;

        node->field = &list[i];
        bucket = script_hash_ci(list[i].name, SCRIPT_ENTITY_HASH_SIZE);
        node->next = cache->buckets[bucket];
        cache->buckets[bucket] = node;
    }

    return cache;
}

static int ifcheck_lookup_linear(char *name, int type)
{
    int i;

    for(i = 0; ifcheck_table[i].name; i++)
        if((ifcheck_table[i].type & type) && !str_cmp(name, ifcheck_table[i].name))
            return i;

    return -1;
}

static bool script_build_ifcheck_cache(void)
{
    int count;
    int i;

    if(script_ifcheck_cache_built)
        return true;
    if(script_ifcheck_cache_failed)
        return false;

    memset(script_ifcheck_buckets, 0, sizeof(script_ifcheck_buckets));

    for(count = 0; ifcheck_table[count].name; count++)
        ;

    for(i = count - 1; i >= 0; i--) {
        SCRIPT_IFCHECK_LOOKUP_NODE *node;
        unsigned int bucket;

        node = alloc_mem(sizeof(*node));
        if(!node) {
            script_ifcheck_cache_failed = true;
            pbugf(LOG_SCRIPTS, "Ifcheck lookup cache allocation failed; using linear fallback.");
            return false;
        }

        node->index = i;
        bucket = script_hash_ci(ifcheck_table[i].name, SCRIPT_IFCHECK_HASH_SIZE);
        node->next = script_ifcheck_buckets[bucket];
        script_ifcheck_buckets[bucket] = node;
    }

    script_ifcheck_cache_built = true;
    return true;
}

char *	const	dir_name_phrase	[]		=
{
    "0 (north)", "1 (east)", "2 (south)", "3 (west)", "4 (up)", "5 (down)", "6 (northeast)",  "7 (northwest)", "8 (southeast)", "9 (southwest)"
};


void script_clear_mobile(CHAR_DATA *ptr)
{
    register int lp;
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.mob == ptr) {
            stack->info.mob = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        if(stack->info.ch == ptr) stack->info.ch = NULL;
        if(stack->info.vch == ptr) stack->info.vch = NULL;
        if(stack->info.rch == ptr) stack->info.rch = NULL;
        for(lp = 0; lp < stack->loop; lp++) {
            if(stack->loops[lp].d.l.type == ENT_MOBILE) {
                if(stack->loops[lp].d.l.next.m == ptr) {
                    if(stack->loops[lp].d.l.cur.m && ptr != stack->loops[lp].d.l.cur.m->next_in_room)
                        stack->loops[lp].d.l.next.m = stack->loops[lp].d.l.cur.m->next_in_room;
                }
            }
        }

        stack = stack->next;
    }
}

void script_clear_object(OBJ_DATA *ptr)
{
    register int lp;
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.obj == ptr) {
            stack->info.obj = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        if(stack->info.obj1 == ptr) stack->info.obj1 = NULL;
        if(stack->info.obj2 == ptr) stack->info.obj2 = NULL;
        for(lp = 0; lp < stack->loop; lp++) {
            if(stack->loops[lp].d.l.type == ENT_OBJECT) {
                if(stack->loops[lp].d.l.next.o == ptr) {
                    if(stack->loops[lp].d.l.cur.o && ptr != stack->loops[lp].d.l.cur.o->next_content)
                        stack->loops[lp].d.l.next.o = stack->loops[lp].d.l.cur.o->next_content;
                }
            }
        }
        stack = stack->next;
    }
}

void script_clear_room(ROOM_INDEX_DATA *ptr)
{
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.room == ptr) {
            stack->info.room = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        stack = stack->next;
    }
}

void script_clear_token(TOKEN_DATA *ptr)
{
    register int lp;
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.token == ptr) {
            stack->info.token = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        for(lp = 0; lp < stack->loop; lp++) {
            if(stack->loops[lp].d.l.type == ENT_TOKEN) {
                if(stack->loops[lp].d.l.next.t == ptr) {
                    if(stack->loops[lp].d.l.cur.t && ptr != stack->loops[lp].d.l.cur.t->next)
                        stack->loops[lp].d.l.next.t = stack->loops[lp].d.l.cur.t->next;
                }
            }
        }
        stack = stack->next;
    }
}

void script_clear_affect(AFFECT_DATA *ptr)
{
    register int lp;
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        for(lp = 0; lp < stack->loop; lp++) {
            if(stack->loops[lp].d.l.type == ENT_AFFECT) {
                if(stack->loops[lp].d.l.next.aff == ptr) {
                    if(stack->loops[lp].d.l.cur.aff && ptr != stack->loops[lp].d.l.cur.aff->next)
                        stack->loops[lp].d.l.next.aff = stack->loops[lp].d.l.cur.aff->next;
                }
            }
        }
        stack = stack->next;
    }
}

void script_clear_list(register void *owner)
{
    register int lp;
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        for(lp = 0; lp < stack->loop; lp++) {
            if(stack->loops[lp].d.l.owner == owner) {
                stack->loops[lp].d.l.owner = NULL;
                stack->loops[lp].d.l.owner_type = ENT_UNKNOWN;
                stack->loops[lp].d.l.cur.raw = NULL;
                stack->loops[lp].d.l.next.raw = NULL;
            }
        }
        stack = stack->next;
    }
}

void script_clear_instance(INSTANCE *ptr)
{
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.instance == ptr) {
            stack->info.instance = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        stack = stack->next;
    }
}

void script_clear_dungeon(DUNGEON *ptr)
{
    register SCRIPT_CB *stack = script_call_stack;

    while(stack) {
        if(stack->info.dungeon == ptr) {
            stack->info.dungeon = NULL;
            stack->info.var = NULL;
            stack->info.targ = NULL;
            SET_BIT(stack->flags,SCRIPTEXEC_HALT);
        }
        stack = stack->next;
    }
}


void script_mobile_addref(CHAR_DATA *ch)
{
    if(IS_VALID(ch) && IS_NPC(ch) && ch->progs)
        ch->progs->script_ref++;
}

bool script_mobile_remref(CHAR_DATA *ch)
{
    if(IS_VALID(ch) && IS_NPC(ch) && ch->progs) {
        if( ch->progs->script_ref > 0 && !--ch->progs->script_ref ) {
            if( ch->progs->extract_when_done ) {
                // Remove!
                ch->progs->extract_when_done = false;
                extract_char(ch, ch->progs->extract_fPull);
                return true;
            }
        }
    }

    return false;
}

void script_object_addref(OBJ_DATA *obj)
{
    if(IS_VALID(obj) && obj->progs)
        obj->progs->script_ref++;
}

bool script_object_remref(OBJ_DATA *obj)
{
    if(IS_VALID(obj) && obj->progs) {
        if( obj->progs->script_ref > 0 && !--obj->progs->script_ref ) {
            if( obj->progs->extract_when_done ) {
                // Remove!
                obj->progs->extract_when_done = false;
                extract_obj(obj);
                return true;
            }
        }
    }

    return false;
}

void script_room_addref(ROOM_INDEX_DATA *room)
{
    if((room->source || IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) && room->progs)
        room->progs->script_ref++;
}

bool script_room_remref(ROOM_INDEX_DATA *room)
{
    if((room->source || IS_SET(room->room_flag[1], ROOM_VIRTUAL_ROOM)) && room->progs) {
        if( room->progs->script_ref > 0 && !--room->progs->script_ref ) {
            if( room->progs->extract_when_done ) {
                // Remove!
                room->progs->extract_when_done = false;
                if(room->source)
                    extract_clone_room(room, room->id[0], room->id[1],false);
                else	// Is a WILDS room
                    destroy_wilds_vroom(room);
                return true;
            }
        }
    }

    return false;
}

void script_token_addref(TOKEN_DATA *token)
{
    if(IS_VALID(token) && token->progs)
        token->progs->script_ref++;
}

bool script_token_remref(TOKEN_DATA *token)
{
    if(IS_VALID(token) && token->progs) {
        if( token->progs->script_ref > 0 && !--token->progs->script_ref ) {
            if( token->progs->extract_when_done ) {
                // Remove!
                token->progs->extract_when_done = false;
                extract_token(token);
                return true;
            }
        }
    }

    return false;
}

void script_instance_addref(INSTANCE *instance)
{
    if(IS_VALID(instance) && instance->progs)
        instance->progs->script_ref++;
}

bool script_instance_remref(INSTANCE *instance)
{
    if(IS_VALID(instance) && instance->progs) {
        if( instance->progs->script_ref > 0 && !--instance->progs->script_ref ) {
            if( instance->progs->extract_when_done ) {
                // Remove!
                instance->progs->extract_when_done = false;
                extract_instance(instance);
                return true;
            }
        }
    }

    return false;
}

void script_dungeon_addref(DUNGEON *dungeon)
{
    if(IS_VALID(dungeon) && dungeon->progs)
        dungeon->progs->script_ref++;
}

bool script_dungeon_remref(DUNGEON *dungeon)
{
    if(IS_VALID(dungeon) && dungeon->progs) {
        if( dungeon->progs->script_ref > 0 && !--dungeon->progs->script_ref ) {
            if( dungeon->progs->extract_when_done ) {
                // Remove!
                dungeon->progs->extract_when_done = false;
                extract_dungeon(dungeon);
                return true;
            }
        }
    }

    return false;
}

ENT_FIELD *script_entity_fields(int type)
{
    int i;

    for(i=0; entity_type_info[i].type_min < ENT_MAX; i++)
        if( (type >= entity_type_info[i].type_min) && (type <= entity_type_info[i].type_max))
            return entity_type_info[i].fields;

    return NULL;
}

bool script_entity_allow_vars(int type)
{
    int i;

    for(i=0; entity_type_info[i].type_min < ENT_MAX; i++)
        if( (type >= entity_type_info[i].type_min) && (type <= entity_type_info[i].type_max))
            return entity_type_info[i].allow_vars;

    return false;
}

const char *script_entity_field_description(const ENT_FIELD *field)
{
    if(!field)
        return NULL;

    return IS_NULLSTR(field->description) ? NULL : field->description;
}

bool script_entity_field_deprecated(const ENT_FIELD *field)
{
    return field ? field->deprecated : false;
}

void script_log_entity_field_pressure_report(int warn_threshold_pct)
{
    int i;
    int threshold = warn_threshold_pct;
    int report_count = 0;
    const int usable_codes = 256 - (int)ESCAPE_EXTRA;

    if(threshold < 0)
        threshold = 0;
    else if(threshold > 100)
        threshold = 100;

    pbugf(LOG_SCRIPTS,
        "Entity field pressure report: threshold=%d%% usable_codes=%d code_range=%u..255",
        threshold,
        usable_codes,
        (unsigned int)ESCAPE_EXTRA);

    for(i = 0; entity_type_info[i].type_min < ENT_MAX; i++) {
        ENT_FIELD *fields = entity_type_info[i].fields;
        const char *first_name_for_code[256] = {0};
        bool warned_code_reuse[256] = {0};
        bool used_code[256] = {0};
        int j;
        int unique_code_count = 0;
        int reuse_count = 0;
        int max_code = -1;
        int total_fields = 0;
        int pressure_pct;

        if(!fields)
            continue;

        for(j = 0; fields[j].name; j++) {
            unsigned int code = (unsigned int)fields[j].code;

            total_fields++;

            if(first_name_for_code[code] == NULL) {
                first_name_for_code[code] = fields[j].name;
            } else if(!warned_code_reuse[code]
                && str_cmp(first_name_for_code[code], fields[j].name)) {
                warned_code_reuse[code] = true;
                reuse_count++;
            }

            if(!used_code[code]) {
                used_code[code] = true;
                unique_code_count++;
            }

            if((int)code > max_code)
                max_code = (int)code;
        }

        pressure_pct = (usable_codes > 0)
            ? (unique_code_count * 100) / usable_codes
            : 0;

        if(threshold > 0 && pressure_pct < threshold)
            continue;

        pbugf(LOG_SCRIPTS,
            "Entity field pressure: table_index=%d type_range=[%d..%d] fields=%d unique_codes=%d/%d (%d%%) max_code=%d reuses=%d",
            i,
            entity_type_info[i].type_min,
            entity_type_info[i].type_max,
            total_fields,
            unique_code_count,
            usable_codes,
            pressure_pct,
            max_code,
            reuse_count);
        report_count++;
    }

    pbugf(LOG_SCRIPTS,
        "Entity field pressure report complete: reported_tables=%d threshold=%d%%",
        report_count,
        threshold);
}

bool script_validate_entity_tables(void)
{
    int errors = 0;
    int warnings = 0;
    int table_count = 0;
    int field_count = 0;
    int highest_pressure_pct = -1;
    int highest_pressure_table_index = -1;
    int highest_pressure_type_min = ENT_UNKNOWN;
    int highest_pressure_type_max = ENT_UNKNOWN;
    int i;

    for(i = 0; entity_type_info[i].type_min < ENT_MAX; i++) {
        ENT_FIELD *fields;
        int j;

        if(entity_type_info[i].type_min > entity_type_info[i].type_max) {
            pbugf(LOG_SCRIPTS,
                "Entity table registry invalid range: min=%d max=%d (index=%d)",
                entity_type_info[i].type_min,
                entity_type_info[i].type_max,
                i);
            errors++;
        }

        if(entity_type_info[i].type_min < ENT_NONE || entity_type_info[i].type_max >= ENT_MAX) {
            pbugf(LOG_SCRIPTS,
                "Entity table registry out-of-range types: min=%d max=%d (index=%d)",
                entity_type_info[i].type_min,
                entity_type_info[i].type_max,
                i);
            errors++;
        }

        for(j = i + 1; entity_type_info[j].type_min < ENT_MAX; j++) {
            if(entity_type_info[i].type_min <= entity_type_info[j].type_max
            && entity_type_info[j].type_min <= entity_type_info[i].type_max) {
                pbugf(LOG_SCRIPTS,
                    "Entity table registry overlap: [%d..%d] with [%d..%d]",
                    entity_type_info[i].type_min,
                    entity_type_info[i].type_max,
                    entity_type_info[j].type_min,
                    entity_type_info[j].type_max);
                errors++;
            }
        }

        fields = entity_type_info[i].fields;
        if(!fields)
            continue;

        table_count++;
        {
            const char *first_name_for_code[256] = {0};
            bool warned_code_reuse[256] = {0};
            bool used_code[256] = {0};
            int unique_code_count = 0;
            int reuse_count = 0;
            int max_code = -1;
            const int usable_codes = 256 - (int)ESCAPE_EXTRA;

        for(j = 0; fields[j].name; j++) {
            int k;
            field_count++;

            if(fields[j].name[0] == '\0') {
                pbugf(LOG_SCRIPTS,
                    "Entity field has empty name in table index=%d field_index=%d",
                    i,
                    j);
                errors++;
            }

            if(fields[j].code < ESCAPE_EXTRA) {
                pbugf(LOG_SCRIPTS,
                    "Entity field '%s' has out-of-band code=%u (valid range %u..%u)",
                    fields[j].name,
                    (unsigned int)fields[j].code,
                    (unsigned int)ESCAPE_EXTRA,
                    255U);
                errors++;
            }

            if(fields[j].type != ENT_UNKNOWN
            && (fields[j].type < ENT_NONE || fields[j].type >= ENT_MAX)) {
                pbugf(LOG_SCRIPTS,
                    "Entity field '%s' has invalid result type=%u",
                    fields[j].name,
                    (unsigned int)fields[j].type);
                errors++;
            }

            if(first_name_for_code[fields[j].code] == NULL) {
                first_name_for_code[fields[j].code] = fields[j].name;
            } else if(!warned_code_reuse[fields[j].code]
                && str_cmp(first_name_for_code[fields[j].code], fields[j].name)) {
                pwarnf(LOG_SCRIPTS,
                    "WARNING: Entity table index=%d reuses field code=%u for '%s' and '%s'",
                    i,
                    (unsigned int)fields[j].code,
                    first_name_for_code[fields[j].code],
                    fields[j].name);
                warned_code_reuse[fields[j].code] = true;
                warnings++;
                reuse_count++;
            }

            if(!used_code[fields[j].code]) {
                used_code[fields[j].code] = true;
                unique_code_count++;
            }
            if((int)fields[j].code > max_code)
                max_code = (int)fields[j].code;

            for(k = j + 1; fields[k].name; k++) {
                if(!str_cmp(fields[j].name, fields[k].name)) {
                    pbugf(LOG_SCRIPTS,
                        "Duplicate entity field name '%s' within table index=%d",
                        fields[j].name,
                        i);
                    errors++;
                }
            }
        }

            if(usable_codes > 0) {
                int pressure_pct = (unique_code_count * 100) / usable_codes;

                if(pressure_pct > highest_pressure_pct) {
                    highest_pressure_pct = pressure_pct;
                    highest_pressure_table_index = i;
                    highest_pressure_type_min = entity_type_info[i].type_min;
                    highest_pressure_type_max = entity_type_info[i].type_max;
                }

            }
        }
    }

    if(errors > 0) {
        pbugf(LOG_SCRIPTS,
            "Entity table validation complete: tables=%d fields=%d errors=%d warnings=%d",
            table_count,
            field_count,
            errors,
            warnings);
    } else if(warnings > 0) {
        pwarnf(LOG_SCRIPTS,
            "Entity table validation complete: tables=%d fields=%d errors=%d warnings=%d",
            table_count,
            field_count,
            errors,
            warnings);
    } else {
        plogf(LOG_SCRIPTS,
            "Entity table validation complete: tables=%d fields=%d errors=%d warnings=%d",
            table_count,
            field_count,
            errors,
            warnings);
    }

    if(highest_pressure_table_index >= 0) {
        plogf(LOG_SCRIPTS,
            "Entity field pressure peak: table_index=%d type_range=[%d..%d] pressure=%d%%",
            highest_pressure_table_index,
            highest_pressure_type_min,
            highest_pressure_type_max,
            highest_pressure_pct);
    }

    script_log_entity_field_pressure_report(75);

    return (errors == 0);
}

//void compile_error_show(char *msg);
ENT_FIELD *entity_type_lookup(char *name, ENT_FIELD *list)
{
    SCRIPT_ENTITY_LIST_CACHE *cache;
    SCRIPT_ENTITY_LOOKUP_NODE *node;
    unsigned int bucket;
    unsigned long long t0;
    unsigned long long t1;

    if(!list || !name)
        return NULL;

    script_lookup_profile.entity_calls++;

    cache = script_get_entity_list_cache(list);
    if(!cache) {
        ENT_FIELD *field;

        t0 = script_profile_now_ns();
        field = entity_type_lookup_linear(name, list);
        t1 = script_profile_now_ns();

        script_lookup_profile.entity_linear_calls++;
        if(t1 >= t0)
            script_lookup_profile.entity_linear_time_ns += (t1 - t0);
        script_lookup_profile.entity_linear_fallbacks++;
        if(script_lookup_profile.entity_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
            script_lookup_profile_report("entity_lookup");
        return field;
    }

    bucket = script_hash_ci(name, SCRIPT_ENTITY_HASH_SIZE);
    t0 = script_profile_now_ns();
    for(node = cache->buckets[bucket]; node; node = node->next) {
        script_lookup_profile.entity_probe_steps++;
        if(!str_cmp(name, node->field->name)) {
            t1 = script_profile_now_ns();
            script_lookup_profile.entity_hash_calls++;
            if(t1 >= t0)
                script_lookup_profile.entity_hash_time_ns += (t1 - t0);
            script_lookup_profile.entity_hash_hits++;
            if(script_lookup_profile.entity_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
                script_lookup_profile_report("entity_lookup");
            return node->field;
        }
    }

    t1 = script_profile_now_ns();
    script_lookup_profile.entity_hash_calls++;
    if(t1 >= t0)
        script_lookup_profile.entity_hash_time_ns += (t1 - t0);

    if(script_lookup_profile.entity_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
        script_lookup_profile_report("entity_lookup");

    return NULL;
}

bool script_expression_push(STACK *stk,int val)
{
    if(stk->t >= MAX_STACK) return false;
    stk->s[stk->t++] = val;
    return true;
}

bool script_expression_push_operator(STACK *stk,int op)
{
    if(script_expression_tostack[op] == STK_MAX) return false;
    return script_expression_push(stk,script_expression_tostack[op]);
}


int get_operator(char *keyword)
{
    register int i;
    for(i = 0; script_operators[i]; i++)
        if(!str_cmp(script_operators[i], keyword))
            return(i);
    return -1;
}

int ifcheck_lookup(char *name, int type)
{
    unsigned int bucket;
    SCRIPT_IFCHECK_LOOKUP_NODE *node;
    unsigned long long t0;
    unsigned long long t1;

    if(!name)
        return -1;

    script_lookup_profile.ifcheck_calls++;

    if(!script_build_ifcheck_cache()) {
        int index;

        t0 = script_profile_now_ns();
        index = ifcheck_lookup_linear(name, type);
        t1 = script_profile_now_ns();

        script_lookup_profile.ifcheck_linear_calls++;
        if(t1 >= t0)
            script_lookup_profile.ifcheck_linear_time_ns += (t1 - t0);
        script_lookup_profile.ifcheck_linear_fallbacks++;
        if(script_lookup_profile.ifcheck_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
            script_lookup_profile_report("ifcheck_lookup");
        return index;
    }

    bucket = script_hash_ci(name, SCRIPT_IFCHECK_HASH_SIZE);
    t0 = script_profile_now_ns();
    for(node = script_ifcheck_buckets[bucket]; node; node = node->next) {
        script_lookup_profile.ifcheck_probe_steps++;
        if((ifcheck_table[node->index].type & type)
        && !str_cmp(name, ifcheck_table[node->index].name)) {
            t1 = script_profile_now_ns();
            script_lookup_profile.ifcheck_hash_calls++;
            if(t1 >= t0)
                script_lookup_profile.ifcheck_hash_time_ns += (t1 - t0);
            script_lookup_profile.ifcheck_hash_hits++;
            if(script_lookup_profile.ifcheck_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
                script_lookup_profile_report("ifcheck_lookup");
            return node->index;
        }
    }

    t1 = script_profile_now_ns();
    script_lookup_profile.ifcheck_hash_calls++;
    if(t1 >= t0)
        script_lookup_profile.ifcheck_hash_time_ns += (t1 - t0);

    if(script_lookup_profile.ifcheck_calls % SCRIPT_LOOKUP_REPORT_INTERVAL == 0)
        script_lookup_profile_report("ifcheck_lookup");

    return -1;
}

char *ifcheck_get_value(SCRIPT_VARINFO *info,IFCHECK_DATA *ifc,char *text,int *ret,bool *valid)
{
    int i;
    SCRIPT_PARAM *argv[IFC_MAXPARAMS];
    char *argument;

    *valid = false;

    // Validate parameters
    if(!ifc || !ret) return NULL;

    if(!ifc->func) return NULL;

    // Clear variables
    for(i = 0; i < IFC_MAXPARAMS; i++)
        argv[i] = new_script_param();

    text = skip_whitespace(text);
    argument = text;

    // Stop when there the param list is full, there's no more text or it hits an
    //	operator
    for(i=0;argument && *argument && *argument != ESCAPE_END && *argument != '=' && *argument != '<' &&
        *argument != '>' && *argument != '!' && *argument != '&' && i<IFC_MAXPARAMS;i++) {
//		if(wiznet_script) {
//			sprintf(buf,"*argument = %02.2X (%c)", *argument, ISPRINT(*argument) ? *argument : ' ');
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
        clear_buf(argv[i]->buffer);
        argument = expand_argument(info,argument,argv[i]);
//		if(wiznet_script) {
//			sprintf(buf,"argv[%d].type = %d (%s)", i, argv[i].type, ifcheck_param_type_names[argv[i].type]);
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
    }
//	if(wiznet_script) {
//		sprintf(buf,"args = %d", i);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}
    if(info && (ifc->func)(info,info->mob,info->obj,info->room,info->token,info->area,ret,i,argv))
        *valid = true;

//	if(wiznet_script) {
//		sprintf(buf,"ret = %d", *ret);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}
    DBG2EXITVALUE1(PTR,argument);
    for(i = 0; i < IFC_MAXPARAMS; i++)
        free_script_param(argv[i]);

    return argument;
}

static bool compare_entity_params(const SCRIPT_PARAM *lhs, const SCRIPT_PARAM *rhs, bool *equal)
{
    if (!lhs || !rhs || !equal)
        return false;

    switch (lhs->type) {
    case ENT_MOBILE:
        if (rhs->type != ENT_MOBILE) return false;
        *equal = (lhs->d.mob == rhs->d.mob);
        return true;
    case ENT_OBJECT:
        if (rhs->type != ENT_OBJECT) return false;
        *equal = (lhs->d.obj == rhs->d.obj);
        return true;
    case ENT_ROOM:
        if (rhs->type != ENT_ROOM) return false;
        *equal = (lhs->d.room == rhs->d.room);
        return true;
    case ENT_TOKEN:
        if (rhs->type != ENT_TOKEN) return false;
        *equal = (lhs->d.token == rhs->d.token);
        return true;
    case ENT_AREA:
        if (rhs->type != ENT_AREA) return false;
        *equal = (lhs->d.area == rhs->d.area);
        return true;
    case ENT_AREA_REGION:
        if (rhs->type != ENT_AREA_REGION) return false;
        *equal = (lhs->d.aregion == rhs->d.aregion);
        return true;
    case ENT_SECTOR:
        if (rhs->type != ENT_SECTOR) return false;
        *equal = (lhs->d.sector == rhs->d.sector);
        return true;
    case ENT_EVENT:
        if (rhs->type != ENT_EVENT) return false;
        *equal = (lhs->d.event.mob == rhs->d.event.mob) &&
                 (lhs->d.event.obj == rhs->d.event.obj) &&
                 (lhs->d.event.uid == rhs->d.event.uid) &&
                 (lhs->d.event.instance_id == rhs->d.event.instance_id);
        return true;
    case ENT_EXIT:
        if (rhs->type != ENT_EXIT) return false;
        *equal = (lhs->d.door.r == rhs->d.door.r && lhs->d.door.door == rhs->d.door.door);
        return true;
    case ENT_WIDEVNUM:
        if (rhs->type != ENT_WIDEVNUM) return false;
        *equal = (lhs->d.wnum.pArea == rhs->d.wnum.pArea && lhs->d.wnum.vnum == rhs->d.wnum.vnum);
        return true;
    case ENT_SKILLGROUP:
        if (rhs->type != ENT_SKILLGROUP) return false;
        *equal = (lhs->d.skill_group == rhs->d.skill_group);
        return true;
    case ENT_REPUTATION:
        if (rhs->type != ENT_REPUTATION) return false;
        *equal = (lhs->d.reputation == rhs->d.reputation);
        return true;
    case ENT_REPUTATION_INDEX:
        if (rhs->type != ENT_REPUTATION_INDEX) return false;
        *equal = (lhs->d.repIndex == rhs->d.repIndex);
        return true;
    case ENT_REPUTATION_RANK:
        if (rhs->type != ENT_REPUTATION_RANK) return false;
        *equal = (lhs->d.repRank == rhs->d.repRank);
        return true;
    default:
        return false;
    }
}

static bool script_param_truthy(const SCRIPT_PARAM *value)
{
    if (!value)
        return false;

    switch (value->type) {
    case ENT_BOOLEAN:
        return value->d.boolean;
    case ENT_NUMBER:
        return value->d.num != 0;
    case ENT_BITVECTOR:
        return value->d.bv.value != 0;
    case ENT_STRING:
        return !IS_NULLSTR(value->d.str);
    case ENT_WIDEVNUM:
        return value->d.wnum.pArea != NULL && value->d.wnum.vnum > 0;
    case ENT_MOBILE:
        return IS_VALID(value->d.mob);
    case ENT_OBJECT:
        return IS_VALID(value->d.obj);
    case ENT_ROOM:
        return value->d.room != NULL;
    case ENT_TOKEN:
        return IS_VALID(value->d.token);
    case ENT_AREA:
        return value->d.area != NULL;
    case ENT_AREA_REGION:
        return value->d.aregion != NULL;
    case ENT_SECTOR:
        return value->d.sector != NULL;
    case ENT_EVENT:
        return value->d.event.uid > 0 || value->d.event.mob != NULL || value->d.event.obj != NULL;
    case ENT_SKILLGROUP:
        return value->d.skill_group != NULL;
    case ENT_REPUTATION:
        return value->d.reputation != NULL;
    case ENT_REPUTATION_INDEX:
        return value->d.repIndex != NULL;
    case ENT_REPUTATION_RANK:
        return value->d.repRank != NULL;
    case ENT_EXIT:
        return value->d.door.r &&
               value->d.door.door >= 0 &&
               value->d.door.door < MAX_DIR &&
               value->d.door.r->exit[value->d.door.door] != NULL;
    default:
        return false;
    }
}

int ifcheck_comparison(SCRIPT_VARINFO *info, short param, char *rest, SCRIPT_PARAM *arg)
{
    int lhs = 0, oper, rhs = 0;
    char *text, *p, buf[MIL], buf2[MSL];
    const char *lhs_string = NULL;
    const char *rhs_string = NULL;
    bool valid;
    IFCHECK_DATA *ifc;
    int max_ifchecks = 0;
    SCRIPT_PARAM lhs_param;
    bool lhs_is_numeric = false;
    bool rhs_is_numeric = false;
    bool lhs_is_string = false;
    bool rhs_is_string = false;
    bool entity_equal = false;
    bool entity_comparable = false;

    memset(&lhs_param, 0, sizeof(lhs_param));

    if(!info) return -1;	// Error

    while (ifcheck_table[max_ifchecks].name)
        ++max_ifchecks;

    if(param < -1 || param >= max_ifchecks)
         return -1;

    if(param == -1) {
        text = expand_argument(info,rest,arg);
        if(!text) return -1;

        lhs_param = *arg;

        if( arg->type == ENT_BOOLEAN )
            return arg->d.boolean ? 1 : 0;

        if( arg->type == ENT_BITVECTOR ) {
            lhs = arg->d.bv.value;
            lhs_is_numeric = true;
        } else if( arg->type == ENT_STRING && is_number(arg->d.str) ) {
            lhs = atoi(arg->d.str);
            lhs_is_numeric = true;
        } else if (arg->type == ENT_STRING) {
            lhs_string = arg->d.str;
            lhs_is_string = true;
        } else {
            if( arg->type == ENT_NUMBER ) {
                lhs = arg->d.num;
                lhs_is_numeric = true;
            }
        }

    } else {
        ifc = &ifcheck_table[param];

        if(wiznet_script) {
            sprintf(buf2,"Doing ifcheck: %d, '%s'", param, ifc->name);
            emit_script_staff_event(buf2, buf2, "ifcheck",
                                    (info && info->block) ? info->block->script : NULL);
        }

        text = ifcheck_get_value(info,ifc,rest,&lhs,&valid);

        if(!valid) return false;

        lhs_is_numeric = true;

        if(!ifc->numeric) return (lhs > 0);
    }

    text = one_argument(text, buf);

    oper = get_operator(buf);
    if (oper < 0) {
        if (param == -1)
            return script_param_truthy(&lhs_param) ? 1 : 0;
        return false;
    }

    p = expand_argument(info,text,arg);
    if(!p || p == text) {
        return -1;
    }

    switch(arg->type) {
    case ENT_NUMBER: rhs = arg->d.num; rhs_is_numeric = true; break;
    case ENT_BOOLEAN: rhs = arg->d.boolean ? 1 : 0; rhs_is_numeric = true; break;
    case ENT_BITVECTOR: rhs = arg->d.bv.value; rhs_is_numeric = true; break;
    case ENT_STRING:
        if(is_number(arg->d.str)) {
            rhs = atoi(arg->d.str);
            rhs_is_numeric = true;
            break;
        }
        rhs_string = arg->d.str;
        rhs_is_string = true;
        break;
    default:
        rhs_is_numeric = false;
        break;
    }

    if (param == -1 && (oper == EVAL_EQ || oper == EVAL_NE)) {
        entity_comparable = compare_entity_params(&lhs_param, arg, &entity_equal);
        if (entity_comparable)
            return (oper == EVAL_EQ) ? entity_equal : !entity_equal;

        if (lhs_is_string && rhs_is_string) {
            bool strings_equal = !str_cmp(lhs_string ? lhs_string : "", rhs_string ? rhs_string : "");
            return (oper == EVAL_EQ) ? strings_equal : !strings_equal;
        }
    }

    if (!lhs_is_numeric || !rhs_is_numeric)
        return false;

    switch(oper) {
    case EVAL_EQ:	return (lhs == rhs);
    case EVAL_GE:	return (lhs >= rhs);
    case EVAL_LE:	return (lhs <= rhs);
    case EVAL_NE:	return (lhs != rhs);
    case EVAL_GT:	return (lhs > rhs);
    case EVAL_LT:	return (lhs < rhs);
    case EVAL_MASK:	return (lhs & rhs);
    default:	return false;
    }
}

int boolexp_evaluate(SCRIPT_CB *block, BOOLEXP *be, SCRIPT_PARAM *arg)
{
    int ret;
    if(!block) return -1;	// Error

    switch(be->type) {
    case BOOLEXP_TRUE:
        return ifcheck_comparison(&block->info, be->param, be->rest, arg);

    case BOOLEXP_NOT:
        ret = ifcheck_comparison(&block->info, be->param, be->rest, arg);

        if( ret < 0 ) return -1;

        return ret ? false : true;

    case BOOLEXP_AND:
        ret = boolexp_evaluate(block, be->left, arg);
        if( ret < 0 ) return -1;

        if( ret == false ) return false;	// Short circuit false

        ret = boolexp_evaluate(block, be->right, arg);
        if( ret < 0 ) return -1;

        return ret ? true : false;

    case BOOLEXP_OR:
        ret = boolexp_evaluate(block, be->left, arg);
        if( ret < 0 ) return -1;

        if( ret == true ) return true;		// Short circuit true

        ret = boolexp_evaluate(block, be->right, arg);
        if( ret < 0 ) return -1;

        return ret ? true : false;
    }


    return false;
}

bool opc_skip_to_label(SCRIPT_CB *block,int op,int id,bool dir)
{
    int line, last;
    SCRIPT_CODE *code;
    char buf[MIL];

    code = block->script->code;
    last = block->script->lines;

    if(wiznet_script) {
        sprintf(buf,"Skipping to %s with ID %d.", opcode_names[op], id);
        emit_script_staff_event(buf, buf, "skip_to_label", block->script);
    }

    if(dir) {	// Forward, after the loop
        for(line = block->line; line < last; line++) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
            if(code[line].opcode == op && code[line].label == id) {
                block->line = line+1;
                DBG2EXITVALUE2(true);
                return true;
            }
        }
    } else {	// Backward
        for(line = block->line; line >= 0; --line) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
            if(code[line].opcode == op && code[line].label == id) {
                block->line = line;
                DBG2EXITVALUE2(true);
                return true;
            }
        }
    }

    DBG2EXITVALUE2(false);
    return false;
}

bool opc_skip_to_level(SCRIPT_CB *block,int op,int level)
{
    int line, last;
    SCRIPT_CODE *code;
    char buf[MIL];

    code = block->script->code;
    last = block->script->lines;

    if(wiznet_script) {
        sprintf(buf,"Skipping to %s with Level %d.", opcode_names[op], level);
        emit_script_staff_event(buf, buf, "skip_to_level", block->script);
    }

    for(line = block->line; line < last; line++) {
//		if(wiznet_script) {
//			(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//		}
        if(code[line].opcode == op && code[line].level == level) {
            block->line = line+1;
            DBG2EXITVALUE2(true);
            return true;
        }
    }

    DBG2EXITVALUE2(false);
    return false;
}

bool opc_skip_block(SCRIPT_CB *block,int level,bool endblock)
{
    int line, last;
    SCRIPT_CODE *code;

//	if(wiznet_script) {
//		sprintf(buf,"Skipping to %s on level %d.", endblock?"ENDIF":"ELSE/ENDIF", level);
//		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//	}

    // Looking for an ELSE/ELSEIF or ENDIF
    if(block->state[level] == OP_IF) {
        code = block->script->code;
        last = block->script->lines;

        line = block->line;
        if(code[line].opcode == OP_ELSEIF) ++line;

        for(; line < last; line++) {
//			if(wiznet_script) {
//				sprintf(buf,"Checking: Line=%d, Opcode=%d(%s), Level=%d", line+1,code[line].opcode,opcode_names[code[line].opcode],code[line].level);
//				wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
//			}
            if(((!endblock && (code[line].opcode == OP_ELSE || code[line].opcode == OP_ELSEIF)) ||
                code[line].opcode == OP_ENDIF) && code[line].level == level) {
                block->line = line;
                DBG2EXITVALUE2(true);
                return true;
            }
        }
    } else if(block->state[level] == OP_WHILE)
        return opc_skip_to_label(block,OP_ENDWHILE,block->cur_line->label,true);

    DBG2EXITVALUE2(false);
    return false;
}

void opc_next_line(SCRIPT_CB *block)
{
    block->line++;
}

void script_loop_cleanup(SCRIPT_CB *block, int level)
{
    int i;

    for(i = 0; i < MAX_NESTED_LOOPS; i++)
    {
        if(block->loops[i].valid && block->loops[i].level >= level )
        {
            switch(block->loops[i].d.l.type) {
            case ENT_STRING:
            case ENT_EXIT:
            case ENT_MOBILE:
            case ENT_OBJECT:
            case ENT_TOKEN:
            case ENT_AFFECT:
                break;

            case ENT_PLLIST_STR:
            case ENT_BLLIST_MOB:
            case ENT_BLLIST_OBJ:
            case ENT_BLLIST_TOK:
            case ENT_BLLIST_ROOM:
            case ENT_BLLIST_EXIT:
            case ENT_BLLIST_SKILL:
            case ENT_BLLIST_AREA:
            case ENT_BLLIST_AREA_REGION:
            case ENT_BLLIST_WILDS:
            case ENT_PLLIST_CONN:
            case ENT_PLLIST_MOB:
            case ENT_PLLIST_OBJ:
            case ENT_PLLIST_ROOM:
            case ENT_PLLIST_TOK:
            case ENT_PLLIST_AREA:
            case ENT_PLLIST_AREA_REGION:
            case ENT_PLLIST_CHURCH:
            case ENT_PLLIST_BOOK_PAGE:
            case ENT_PLLIST_FOOD_BUFF:
            case ENT_PLLIST_REPUTATION_RANK:
            case ENT_ILLIST_VARIABLE:
            case ENT_ILLIST_AURA_STR:
            case ENT_ILLIST_REPUTATION:
            case ENT_ILLIST_REPUTATION_INDEX:
            case ENT_ILLIST_SKILLGROUPS:
                iterator_stop(&block->loops[i].d.l.list.it);
                if (block->loops[i].d.l.type == ENT_ILLIST_AURA_STR)
                    list_destroy(block->loops[i].d.l.list.lp);
                break;

            case ENT_ILLIST_QUEST_STAGES:
            case ENT_ILLIST_QUEST_OBJECTIVES:
                iterator_stop(&block->loops[i].d.l.list.it);
                list_destroy(block->loops[i].d.l.list.lp);
                break;

            case ENT_ILLIST_QUEST:
            case ENT_ILLIST_QUEST_HISTORY:
                iterator_stop(&block->loops[i].d.l.list.it);
                list_destroy(block->loops[i].d.l.list.lp);
                break;

            case ENT_ILLIST_EVENT:
                iterator_stop(&block->loops[i].d.l.list.it);
                list_destroy(block->loops[i].d.l.list.lp);
                break;

            case ENT_ILLIST_MOB_GROUP:
                iterator_stop(&block->loops[i].d.l.list.it);
                list_destroy(block->loops[i].d.l.list.lp);
                break;
            }

            block->loops[i].valid = false;
        }
    }
}

// Function: End script execution
//
// Formats:
// break[ <expression>]
// end[ <expression>]
//
DECL_OPC_FUN(opc_end)
{
    int val;

    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Anything to evaluate?
    if(block->cur_line->rest[0]) {
        SCRIPT_PARAM *arg = new_script_param();
        if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        switch(arg->type) {
        case ENT_STRING: val = atoi(arg->d.str); break;
        case ENT_NUMBER: val = arg->d.num; break;
        default: val = 0; break;
        }

        DBG3MSG1("val = %d\n", val);
        if(val >= 0) block->ret_val = val;
        free_script_param(arg);
    }

    return false;
}

DECL_OPC_FUN(opc_if)
{
    int ret;

    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    if(block->cur_line->opcode == OP_ELSEIF && block->cond[block->cur_line->level])
        return opc_skip_block(block,block->cur_line->level,true);

    SCRIPT_PARAM *arg = new_script_param();
    ret = boolexp_evaluate(block, (BOOLEXP *)block->cur_line->rest, arg);
    free_script_param(arg);
    if(ret < 0) return false;

    block->state[block->cur_line->level] = OP_IF;
    block->cond[block->cur_line->level] = ret;
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_while)
{
    int ret;

    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    SCRIPT_PARAM *arg = new_script_param();
    ret = boolexp_evaluate(block, (BOOLEXP *)block->cur_line->rest, arg);
    free_script_param(arg);
    if(ret < 0) return false;

    block->state[block->cur_line->level] = OP_WHILE;
    block->cond[block->cur_line->level] = ret;
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_else)
{
    if(block->cond[block->cur_line->level])
        return opc_skip_block(block,block->cur_line->level,true);

    // Since the previous check was false, this block must be true
    block->cond[block->cur_line->level] = true;
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_endif)
{
    // No need to do anything, just keep going.
    // Invalid structures are handled by the preparser
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_command)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Allow only MOBS to do this...
    if(block->type == IFC_M && block->info.mob) {
        BUFFER *buffer = new_buf();
        char command[MAX_INPUT_LENGTH];
        char expanded[MAX_STRING_LENGTH];
        char *rest = NULL;
        char *ptr;

        expand_string(&block->info,block->cur_line->rest,buffer);

        strlcpy(expanded, buf_string(buffer), sizeof(expanded));
        ptr = expanded;
        while (ISSPACE(*ptr))
            ptr++;

        if (!IS_NULLSTR(ptr) && !ISALPHA(*ptr) && !ISDIGIT(*ptr)) {
            command[0] = *ptr;
            command[1] = '\0';
            ptr++;
            while (ISSPACE(*ptr))
                ptr++;
            rest = ptr;
        } else {
            rest = one_argument(ptr, command);
        }

        if (!str_cmp(command, "say") || !str_cmp(command, "'")) {
            do_say(block->info.mob, rest);
        } else if (!str_cmp(command, "emote") || !str_cmp(command, ",")) {
            do_emote(block->info.mob, rest);
        } else {
            interpret(block->info.mob, buf_string(buffer));
        }

        free_buf(buffer);
    }
    // Ignore the others

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_gotoline)
{
    int val;

    script_loop_cleanup(block, block->cur_line->level);
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Anything to evaluate?
    if(block->cur_line->rest[0]) {
        SCRIPT_PARAM *arg = new_script_param();
        if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        switch(arg->type) {
        case ENT_STRING: val = atoi(arg->d.str)-1; break;
        case ENT_NUMBER: val = arg->d.num-1; break;
        default: val = -1; break;
        }

        free_script_param(arg);

        if(val >= 0 && val < block->script->lines) {
            block->line = val;
            block->cur_line = &block->script->code[val];
            script_loop_cleanup(block, block->cur_line->level);
            if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = true;
            return true;
        }
    }

    return false;
}

DECL_OPC_FUN(opc_for)
{
    bool skip = false;
    int lp, end, cur, inc;
    char *str1,*str2;

    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    for(lp = block->loop; lp-- > 0;)
        if(block->cur_line->label == block->loops[lp].id)
            break;

    // Initialize loop control
    if(lp < 0) {
        lp = block->loop;

        // Variable Name
        str1 = one_argument(block->cur_line->rest,block->loops[lp].var_name);

        if(!block->loops[lp].var_name[0]) {
            block->ret_val = PRET_BADSYNTAX;
            return false;
        }

        SCRIPT_PARAM *arg = new_script_param();

        if(!(str2 = expand_argument(&block->info,str1,arg))) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        switch(arg->type) {
        case ENT_STRING: cur = atoi(arg->d.str); break;
        case ENT_NUMBER: cur = arg->d.num; break;
        default:
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        if(!(str1 = expand_argument(&block->info,str2,arg))) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        switch(arg->type) {
        case ENT_STRING: end = atoi(arg->d.str); break;
        case ENT_NUMBER: end = arg->d.num; break;
        default:
            block->ret_val = PRET_BADSYNTAX;
            return false;
        }


        if(!(str2 = expand_argument(&block->info,str1,arg))) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        switch(arg->type) {
        case ENT_STRING: inc = atoi(arg->d.str); break;
        case ENT_NUMBER: inc = arg->d.num; break;
        default:
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        free_script_param(arg);

        // No increment?  No looping!
        if(!inc) return opc_skip_to_label(block,OP_ENDFOR,block->cur_line->label,true);

        block->loops[lp].id = block->cur_line->label;
        block->loops[lp].d.f.inc = inc;

        // set the directions correctly
        if((inc > 0) == (cur < end)) {
            block->loops[lp].d.f.cur = cur;
            block->loops[lp].d.f.end = end;
        } else {
            block->loops[lp].d.f.cur = end;
            block->loops[lp].d.f.end = cur;
        }

        // Set the variable
        variables_set_integer(block->info.var,block->loops[lp].var_name,block->loops[lp].d.f.cur);
        block->loop++;
        block->cond[block->cur_line->level] = true;
    } else {
        // Continue loop
        block->loops[lp].d.f.cur += block->loops[lp].d.f.inc;

        // Set the variable
        variables_set_integer(block->info.var,block->loops[lp].var_name,block->loops[lp].d.f.cur);

        if(block->loops[lp].d.f.inc < 0 && (block->loops[lp].d.f.cur < block->loops[lp].d.f.end))
            skip = true;
        else if(block->loops[lp].d.f.inc > 0 && (block->loops[lp].d.f.cur > block->loops[lp].d.f.end))
            skip = true;

        if(skip) {
            block->loop--;
            return opc_skip_to_label(block,OP_ENDFOR,block->loops[lp].id,true);
        }
        block->cond[block->cur_line->level] = true;
    }

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_endfor)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_FOR,block->cur_line->label,false);
}

DECL_OPC_FUN(opc_exitfor)
{
    script_loop_cleanup(block, block->cur_line->level);
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_ENDFOR,block->cur_line->label,true);
}


DECL_OPC_FUN(opc_list)
{
    bool skip = false;
    int lp, i;
    char *str1,*str2, *str;
    char buf[MSL];
    LLIST *list;
    LLIST_UID_DATA *uid;
    LLIST_ROOM_DATA *lrd;
    LLIST_EXIT_DATA *led;
    LLIST_SKILL_DATA *lsk;
    LLIST_AREA_DATA *lar;
    LLIST_AREA_REGION_DATA *lareg;
    LLIST_WILDS_DATA *lwd;
    DESCRIPTOR_DATA *conn;
    CHAR_DATA *ch;
    OBJ_DATA *obj;
    TOKEN_DATA *tok;
    ROOM_INDEX_DATA *here;
    EXIT_DATA *ex;
    AREA_DATA *area;
    AREA_REGION *aregion;
    CHURCH_DATA *church;
    BOOK_PAGE *book_page;
    FOOD_BUFF_DATA *food_buff;
    VARIABLE *variable;
    EXTRA_DESCR_DATA *ed;
    INSTANCE_SECTION *section;
    INSTANCE *instance;
    REPUTATION_DATA *reputation;
    REPUTATION_INDEX_DATA *repIndex;
    REPUTATION_INDEX_RANK_DATA *repRank;
    SKILL_GROUP *skill_group;
    QUEST_STAGE_INDEX_V2_DATA *quest_stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *quest_objective;
    QUEST_DATA *quest_run;
    QUEST_HISTORY_DATA *quest_hist;
    NAMED_SPECIAL_ROOM *special_room;
    SHIP_DATA *ship;
    EVENT_RUNTIME_REF *event_ref;
    EVENT_RUNTIME_REF empty_event_ref = { 0, 0 };

    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    for(lp = block->loop; lp-- > 0;)
        if(block->cur_line->label == block->loops[lp].id)
            break;

    // Initialize loop control
    if(lp < 0) {
        lp = block->loop;

        // Variable Name
        str1 = one_argument(block->cur_line->rest,block->loops[lp].var_name);

        log_stringf("opc_list: initializing for loop variable '%s'", block->loops[lp].var_name);

        if(!block->loops[lp].var_name[0]) {
            block->ret_val = PRET_BADSYNTAX;
            return false;
        }

        SCRIPT_PARAM *arg = new_script_param();

        // Get the LIST
        if(!(str2 = expand_argument(&block->info,str1,arg))) {
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        block->loops[lp].counter = 1;

        switch(arg->type) {
        case ENT_STRING:
            //log_stringf("opc_list: list type ENT_STRING");
            if(IS_NULLSTR(arg->d.str))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            strncpy(block->loops[lp].buf, arg->d.str, MSL-1);
            str = one_argument_norm(block->loops[lp].buf, buf);

            block->loops[lp].d.l.type = ENT_STRING;
            block->loops[lp].d.l.cur.str = NULL;
            block->loops[lp].d.l.next.str = str;
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;


            // Set the variable
            variables_set_string(block->info.var,block->loops[lp].var_name,buf,false);
            break;
        case ENT_EXIT:
            //log_stringf("opc_list: list type ENT_EXIT");
            if(!arg->d.door.r)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            here = arg->d.door.r;
            ex = here->exit[arg->d.door.door];
            block->loops[lp].d.l.type = ENT_EXIT;
            block->loops[lp].d.l.cur.door = arg->d.door.door;
            block->loops[lp].d.l.next.door = MAX_DIR;
            block->loops[lp].d.l.owner = arg->d.door.r;
            block->loops[lp].d.l.owner_type = ENT_EXIT;

            /*
            if(ex) {
                if(here->wilds)
                    log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[arg.d.door.door], here->wilds->uid, here->x, here->y);
                else if(here->source)
                    log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[arg.d.door.door], here->vnum, here->name, here->id[0], here->id[1]);
                else
                    log_stringf("opc_list: %s(%ld,%s)", dir_name[arg.d.door.door], here->vnum, here->name);
            } else
                log_stringf("opc_list: exit(<END>)");
            */


            if(ex) {
                here = arg->d.door.r;
                for(i=arg->d.door.door + 1; i < MAX_DIR && !here->exit[i]; i++);
                block->loops[lp].d.l.next.door = i;
            }
            // Set the variable
            variables_set_exit(block->info.var,block->loops[lp].var_name,ex);
            break;

        case ENT_OLLIST_MOB:
            //log_stringf("opc_list: list type ENT_MOBILE");
            if(!arg->d.list.ptr.mob || !*arg->d.list.ptr.mob)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_MOBILE;
            block->loops[lp].d.l.cur.m = *arg->d.list.ptr.mob;
            block->loops[lp].d.l.next.m = block->loops[lp].d.l.cur.m->next_in_room;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = ENT_MOBILE;

            /*
            if(block->loops[lp].d.l.cur.m) {
                ch = block->loops[lp].d.l.cur.m;
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
            */

            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.mob);
            break;

        case ENT_OLLIST_OBJ:
            //log_stringf("opc_list: list type ENT_OBJECT");
            if(!arg->d.list.ptr.obj || !*arg->d.list.ptr.obj)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_OBJECT;
            block->loops[lp].d.l.cur.o = *arg->d.list.ptr.obj;
            block->loops[lp].d.l.next.o = block->loops[lp].d.l.cur.o->next_content;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = ENT_OBJECT;

            /*
            if(block->loops[lp].d.l.cur.o)
                log_stringf("opc_list: object(%ld,%ld,%ld)", block->loops[lp].d.l.cur.o->pIndexData->vnum, block->loops[lp].d.l.cur.o->id[0], block->loops[lp].d.l.cur.o->id[1]);
            else
                log_stringf("opc_list: object(<END>)");
                */

            // Set the variable
            variables_set_object(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.obj);
            break;

        case ENT_OLLIST_TOK:
            //log_stringf("opc_list: list type ENT_TOKEN");
            if(!arg->d.list.ptr.tok || !*arg->d.list.ptr.tok)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_TOKEN;
            block->loops[lp].d.l.cur.t = *arg->d.list.ptr.tok;
            block->loops[lp].d.l.next.t = block->loops[lp].d.l.cur.t->next;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = ENT_TOKEN;

            /*
            if(block->loops[lp].d.l.cur.t)
                log_stringf("opc_list: token(%ld,%ld,%ld)", block->loops[lp].d.l.cur.t->pIndexData->vnum, block->loops[lp].d.l.cur.t->id[0], block->loops[lp].d.l.cur.t->id[1]);
            else
                log_stringf("opc_list: token(<END>)");
                */

            // Set the variable
            variables_set_token(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.tok);
            break;

        case ENT_OLLIST_AFF:
            //log_stringf("opc_list: list type ENT_AFFECT");
            if(!arg->d.list.ptr.aff || !*arg->d.list.ptr.aff)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_AFFECT;
            block->loops[lp].d.l.cur.aff = *arg->d.list.ptr.aff;
            block->loops[lp].d.l.next.aff = block->loops[lp].d.l.cur.aff->next;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = ENT_AFFECT;

            /*
            if(block->loops[lp].d.l.cur.aff) {
                if(block->loops[lp].d.l.cur.aff->custom_name)
                    log_stringf("opc_list: affect(%s)", block->loops[lp].d.l.cur.aff->custom_name);
                else
                    log_stringf("opc_list: affect(%s)", skill_table[block->loops[lp].d.l.cur.aff->type].name);
            } else
                log_stringf("opc_list: affect(<END>)");
                */

            // Set the variable
            variables_set_affect(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.aff);
            break;

        case ENT_OLLIST_TRAINER_ENTRY:
            if(!arg->d.list.ptr.trainer_entry || !*arg->d.list.ptr.trainer_entry)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_TRAINER_ENTRY;
            block->loops[lp].d.l.cur.trainer_entry = *arg->d.list.ptr.trainer_entry;
            block->loops[lp].d.l.next.trainer_entry = block->loops[lp].d.l.cur.trainer_entry->next;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = ENT_TRAINER;

            variables_set_trainer_entry(block->info.var,block->loops[lp].var_name,*arg->d.list.ptr.trainer_entry);
            break;

        case ENT_EXTRADESC:
            if(!arg->d.list.ptr.ed || !*arg->d.list.ptr.ed)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_EXTRADESC;
            block->loops[lp].d.l.cur.ed = *arg->d.list.ptr.ed;
            block->loops[lp].d.l.next.ed = block->loops[lp].d.l.cur.ed->next;
            block->loops[lp].d.l.owner = arg->d.list.owner;
            block->loops[lp].d.l.owner_type = arg->d.list.owner_type;

            // Set the variable
            variables_set_string(block->info.var,block->loops[lp].var_name,(*arg->d.list.ptr.ed)->keyword, false);
            break;

        case ENT_PLLIST_STR:
            //log_stringf("opc_list: list type ENT_PLLIST_STR");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_STR;

                    case ENT_ILLIST_AURA_STR:
                        if(!arg->d.blist || !arg->d.blist->valid)
                        {
                            free_script_param(arg);
                            return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                        }

                        block->loops[lp].d.l.type = ENT_ILLIST_AURA_STR;
                        block->loops[lp].d.l.list.lp = arg->d.blist;
                        iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
                        block->loops[lp].d.l.owner = NULL;
                        block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

                        str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);

                        if( !str ) {
                            iterator_stop(&block->loops[lp].d.l.list.it);
                            free_script_param(arg);
                            return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                        }

                        variables_set_string(block->info.var,block->loops[lp].var_name,str,false);
                        break;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(str)
                log_stringf("opc_list: string(%s)",str);
            else
                log_stringf("opc_list: string(<END>)");
                */

            if( !str ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_string(block->info.var,block->loops[lp].var_name,str,false);
            break;

        case ENT_BLLIST_MOB:
            //log_stringf("opc_list: list type ENT_BLLIST_MOB");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_MOB;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            if( !uid ) {
                //log_stringf("opc_list: mobile(<END>)");
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            if( IS_VALID((CHAR_DATA *)uid->ptr) )
                variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)uid->ptr);
            else
                variables_set_mobile_id(block->info.var,block->loops[lp].var_name, uid->id[0], uid->id[1],false);

            /*
            ch = (CHAR_DATA *)(uid->ptr);
            if(!IS_NPC(ch))
                log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
            else
                log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
                */
            break;

        case ENT_BLLIST_OBJ:
            //log_stringf("opc_list: list type ENT_BLLIST_OBJ");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_OBJ;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            if( !uid ) {
                //log_stringf("opc_list: object(<END>)");
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            if( IS_VALID((OBJ_DATA *)uid->ptr) )
                variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)uid->ptr);
            else
                variables_set_object_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);

            //obj = (OBJ_DATA *)(uid->ptr);
            //log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);

            break;

        case ENT_BLLIST_TOK:
            //log_stringf("opc_list: list type ENT_BLLIST_TOK");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_TOK;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            if( !uid ) {
                //log_stringf("opc_list: token(<END>)");
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            if( IS_VALID((TOKEN_DATA *)uid->ptr) )
                variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)uid->ptr);
            else
                variables_set_token_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);

            //tok = (TOKEN_DATA *)(uid->ptr);
            //log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
            break;

        case ENT_BLLIST_ROOM:
            //log_stringf("opc_list: list type ENT_BLLIST_ROOM");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_ROOM;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                lrd = (LLIST_ROOM_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                //log_stringf("opc_list: lrd = %016lX", lrd);
                if( !lrd ) {
                    //log_stringf("opc_list: room(<END>)");
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                // Set the variable
                if( lrd->room )
                    variables_set_room(block->info.var,block->loops[lp].var_name,lrd->room);

            } while( !lrd->room );

            /*
            log_stringf("opc_list: lrd->room = %016lX", lrd->room);
            if(lrd->room->wilds)
                log_stringf("opc_list: room(%ld,%ld,%ld)", lrd->room->wilds->uid, lrd->room->x, lrd->room->y);
            else if(lrd->room->source)
                log_stringf("opc_list: room(%ld,%s,%ld,%ld)", lrd->room->vnum, lrd->room->name, lrd->room->id[0], lrd->room->id[1]);
            else
                log_stringf("opc_list: room(%ld,%s)", lrd->room->vnum, lrd->room->name);
                */

            break;

        case ENT_BLLIST_EXIT:
            //log_stringf("opc_list: list type ENT_BLLIST_EXIT");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_EXIT;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                led = (LLIST_EXIT_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                if( !led ) {
                    //log_stringf("opc_list: exit(<END>)");
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                // Set the variable
                if( led->room && led->door >= 0 && led->door < MAX_DIR && led->room->exit[led->door])
                    variables_set_door(block->info.var,block->loops[lp].var_name,led->room, led->door, false);

            } while( !led->room || led->door < 0 || led->door >= MAX_DIR || !led->room->exit[led->door] );

            /*
            if(led->room->wilds)
                log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[led->door], led->room->wilds->uid, led->room->x, led->room->y);
            else if(led->room->source)
                log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[led->door], led->room->vnum, led->room->name, led->room->id[0], led->room->id[1]);
            else
                log_stringf("opc_list: %s(%ld,%s)", dir_name[led->door], led->room->vnum, led->room->name);
                */

            break;

        case ENT_BLLIST_SKILL:
            //log_stringf("opc_list: list type ENT_BLLIST_SKILL");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_SKILL;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                lsk = (LLIST_SKILL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                if( !lsk ) {
                    //log_stringf("opc_list: skill(<END>)");
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                // Set the variable
                if( IS_VALID(lsk->mob) && (IS_VALID(lsk->tok) || ( lsk->sn > 0 && lsk->sn < MAX_SKILL )) )
                    variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, lsk->mob, lsk->sn, lsk->tok, false);

            } while( !IS_VALID(lsk->mob) || (!IS_VALID(lsk->tok) && ( lsk->sn < 1 || lsk->sn >= MAX_SKILL )) );

            /*
            if(lsk->tok)
                log_stringf("opc_list: skill(%ld,%ld,TOKEN,%s)", lsk->mob->id[0], lsk->mob->id[1], lsk->tok->name);
            else
                log_stringf("opc_list: skill(%ld,%ld,SKILL,%s)", lsk->mob->id[0], lsk->mob->id[1], skill_table[lsk->sn].name);
                */

            break;

        case ENT_BLLIST_AREA:
            //log_stringf("opc_list: list type ENT_BLLIST_AREA");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_AREA;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                lar = (LLIST_AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                if( !lar ) {
                    //log_stringf("opc_list: area(<END>)");
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                // Set the variable
                if( lar->area )
                    variables_setsave_area(block->info.var,block->loops[lp].var_name, lar->area, false);

            } while( !lar->area );

            //log_stringf("opc_list: area(%ld,%s)", lar->area->uid, lar->area->name);

            break;

        case ENT_BLLIST_AREA_REGION:
            //log_stringf("opc_list: list type ENT_BLLIST_AREA_REGION");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_AREA_REGION;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                lareg = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                if( !lareg ) {
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                if( lareg->aregion )
                    variables_setsave_area_region(block->info.var,block->loops[lp].var_name, lareg->aregion, false);

            } while( !lareg->aregion );

            break;

        case ENT_BLLIST_WILDS:
            //log_stringf("opc_list: list type ENT_BLLIST_WILDS");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_BLLIST_WILDS;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            do {
                lwd = (LLIST_WILDS_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
                if( !lwd ) {
                    //log_stringf("opc_list: wilds(<END>)");
                    iterator_stop(&block->loops[lp].d.l.list.it);
                    free_script_param(arg);
                    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
                }

                // Set the variable
                if( lwd->wilds )
                    variables_setsave_wilds(block->info.var,block->loops[lp].var_name, lwd->wilds, false);

            } while( !lwd->wilds );

            //log_stringf("opc_list: wilds(%ld,%s)", lwd->wilds->uid, lwd->wilds->name);

            break;

        case ENT_PLLIST_CONN:
            //log_stringf("opc_list: list type ENT_PLLIST_CONN");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_CONN;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            conn = (DESCRIPTOR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(conn) {
                if(conn->original)
                    log_stringf("opc_list: connection(%s,%d) [SWITCHED]", conn->original->name, conn->original->tot_level);
                else
                    log_stringf("opc_list: connection(%s,%d)", conn->character->name, conn->character->tot_level);
            } else
                log_stringf("opc_list: connection(<END>)");*/

            if( !conn ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_connection(block->info.var,block->loops[lp].var_name,conn);
            break;

        case ENT_PLLIST_MOB:
            //log_stringf("opc_list: list type ENT_PLLIST_MOB");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_MOB;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(ch) {
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            if( !ch ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);
            break;

        case ENT_PLLIST_OBJ:
            //log_stringf("opc_list: list type ENT_PLLIST_OBJ");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_OBJ;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(obj)
                log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
            else
                log_stringf("opc_list: object(<END>)");
                */

            if( !obj ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_object(block->info.var,block->loops[lp].var_name,obj);
            break;

        case ENT_PLLIST_ROOM:
            //log_stringf("opc_list: list type ENT_PLLIST_ROOM");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_ROOM;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            here = (ROOM_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(here) {
                if(here->wilds)
                    log_stringf("opc_list: room(%ld,%ld,%ld)", here->wilds->uid, here->x, here->y);
                else if(here->source)
                    log_stringf("opc_list: room(%ld,%s,%ld,%ld)", here->vnum, here->name, here->id[0], here->id[1]);
                else
                    log_stringf("opc_list: room(%ld,%s)", here->vnum, here->name);
            } else
                log_stringf("opc_list: room(<END>)");
                */

            if( !here ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_room(block->info.var,block->loops[lp].var_name,here);
            break;

        case ENT_PLLIST_TOK:
            //log_stringf("opc_list: list type ENT_PLLIST_TOK");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_TOK;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            tok = (TOKEN_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(tok)
                log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
            else
                log_stringf("opc_list: token(<END>)");
                */

            if( !tok ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_token(block->info.var,block->loops[lp].var_name,tok);
            break;

        case ENT_PLLIST_CHURCH:
            //log_stringf("opc_list: list type ENT_PLLIST_CHURCH");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_CHURCH;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            church = (CHURCH_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            //log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

            if( !church ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_church(block->info.var,block->loops[lp].var_name,church);
            break;

        case ENT_PLLIST_BOOK_PAGE:
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_BOOK_PAGE;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            book_page = (BOOK_PAGE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !book_page ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_book_page(block->info.var,block->loops[lp].var_name,book_page);
            break;

        case ENT_PLLIST_FOOD_BUFF:
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_FOOD_BUFF;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            food_buff = (FOOD_BUFF_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !food_buff ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_food_buff(block->info.var,block->loops[lp].var_name,food_buff);
            break;

        case ENT_PLLIST_REPUTATION_RANK:
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_REPUTATION_RANK;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            repRank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !repRank ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_reputation_rank(block->info.var,block->loops[lp].var_name,repRank);
            break;

        case ENT_PLLIST_AREA:
            //log_stringf("opc_list: list type ENT_PLLIST_AREA");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_AREA;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            area = (AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !area ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_area(block->info.var,block->loops[lp].var_name,area);
            break;

        case ENT_PLLIST_AREA_REGION:
            //log_stringf("opc_list: list type ENT_PLLIST_AREA_REGION");
            if(!arg->d.blist || !arg->d.blist->valid)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_PLLIST_AREA_REGION;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,arg->d.blist);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            aregion = (AREA_REGION *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !aregion ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_area_region(block->info.var,block->loops[lp].var_name,aregion);
            break;

        case ENT_ILLIST_MOB_GROUP:
            //log_stringf("opc_list: list type ENT_ILLIST_MOB_GROUP");
            if(!arg->d.group_owner || !arg->d.group_owner->in_room)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            ch = arg->d.group_owner->leader ? arg->d.group_owner->leader : arg->d.group_owner;

            list = list_copy(ch->lgroup);
            if( !list )
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            if( !list_addlink(list, ch) ) {
                list_destroy(list);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_MOB_GROUP;
            block->loops[lp].d.l.list.lp = list;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(ch) {
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            if( !ch ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);
            break;

        case ENT_ILLIST_VARIABLE:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            if(!arg->d.variables || !*arg->d.variables)
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_VARIABLE;
            block->loops[lp].d.l.list.lp = variable_copy_tolist(arg->d.variables);
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            variable = (VARIABLE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            if( !variable ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_variable(block->info.var,block->loops[lp].var_name,variable);
            break;

        case ENT_ILLIST_REPUTATION:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_REPUTATION;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            reputation = (REPUTATION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !reputation ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_reputation(block->info.var,block->loops[lp].var_name,reputation);
            break;

        case ENT_ILLIST_REPUTATION_INDEX:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_REPUTATION_INDEX;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !repIndex ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_reputation_index(block->info.var,block->loops[lp].var_name,repIndex);
            break;

        case ENT_ILLIST_SKILLGROUPS:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_SKILLGROUPS;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            skill_group = (SKILL_GROUP *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !skill_group ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_skill_group(block->info.var,block->loops[lp].var_name,skill_group);
            break;

        case ENT_ILLIST_QUEST_STAGES:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_QUEST_STAGES;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            quest_stage = (QUEST_STAGE_INDEX_V2_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !quest_stage ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_quest_stage(block->info.var,block->loops[lp].var_name,quest_stage);
            break;

        case ENT_ILLIST_QUEST_OBJECTIVES:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_QUEST_OBJECTIVES;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            quest_objective = (QUEST_OBJECTIVE_INDEX_V2_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !quest_objective ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_quest_objective(block->info.var,block->loops[lp].var_name,quest_objective);
            break;

        case ENT_ILLIST_QUEST:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_QUEST;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            quest_run = (QUEST_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !quest_run ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_quest(block->info.var,block->loops[lp].var_name,quest_run);
            break;

        case ENT_ILLIST_QUEST_HISTORY:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_QUEST_HISTORY;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            quest_hist = (QUEST_HISTORY_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !quest_hist ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_quest_history(block->info.var,block->loops[lp].var_name,quest_hist);
            break;

        case ENT_ILLIST_EVENT:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_EVENT;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            event_ref = (EVENT_RUNTIME_REF *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !event_ref ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            variables_set_event(block->info.var,block->loops[lp].var_name,*event_ref);
            break;

        case ENT_ILLIST_SECTIONS:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_SECTIONS;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            section = (INSTANCE_SECTION *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !section ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_instance_section(block->info.var,block->loops[lp].var_name,section);
            break;

        case ENT_ILLIST_INSTANCES:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_INSTANCES;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            instance = (INSTANCE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !instance ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_instance(block->info.var,block->loops[lp].var_name,instance);
            break;

        case ENT_ILLIST_SPECIALROOMS:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_SPECIALROOMS;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            special_room = (NAMED_SPECIAL_ROOM *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !special_room ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_room(block->info.var,block->loops[lp].var_name,special_room->room);
            break;

        case ENT_ILLIST_SHIPS:
            if(!IS_VALID(arg->d.blist))
            {
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            block->loops[lp].d.l.type = ENT_ILLIST_SHIPS;
            block->loops[lp].d.l.list.lp = arg->d.blist;
            iterator_start(&block->loops[lp].d.l.list.it,block->loops[lp].d.l.list.lp);
            block->loops[lp].d.l.owner = NULL;
            block->loops[lp].d.l.owner_type = ENT_UNKNOWN;

            ship = (SHIP_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( !ship ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                free_script_param(arg);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            // Set the variable
            variables_set_ship(block->info.var,block->loops[lp].var_name,ship);
            break;

        default:
            //log_stringf("opc_list: list_type INVALID");
            block->ret_val = PRET_BADSYNTAX;
            free_script_param(arg);
            return false;
        }

        block->loops[lp].id = block->cur_line->label;
        block->loops[lp].valid = true;
        block->loops[lp].level = block->cur_line->level;
        block->loop++;
        block->cond[block->cur_line->level] = true;
        free_script_param(arg);
    } else {
        //log_stringf("opc_list: next loop variable '%s'", block->loops[lp].var_name);
        block->loops[lp].counter++;

        // Continue loop
        switch(block->loops[lp].d.l.type) {
        case ENT_STRING:
            //log_stringf("opc_list: list type ENT_STRING");
            str = block->loops[lp].d.l.next.str;

            if( IS_NULLSTR(str) )
            {
                skip = true;
                break;
            }

            str = one_argument_norm(str,buf);

            variables_set_string(block->info.var,block->loops[lp].var_name,buf,false);

            block->loops[lp].d.l.next.str = str;
            break;


        case ENT_EXIT:
            //log_stringf("opc_list: list type ENT_EXIT");
            i = block->loops[lp].d.l.cur.door = block->loops[lp].d.l.next.door;

            if( i >= MAX_DIR ) {
                skip = true;
                break;
            }

            here = (ROOM_INDEX_DATA *)block->loops[lp].d.l.owner;
            ex = here->exit[i];

            /*
            if(ex) {
                if(here->wilds)
                    log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[i], here->wilds->uid, here->x, here->y);
                else if(here->source)
                    log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[i], here->vnum, here->name, here->id[0], here->id[1]);
                else
                    log_stringf("opc_list: %s(%ld,%s)", dir_name[i], here->vnum, here->name);
            } else
                log_stringf("opc_list: exit(<END>)");
                */


            // Set the variable
            variables_set_exit(block->info.var,block->loops[lp].var_name,ex);

            if(!ex) {
                skip = true;
                break;
            }

            for(i++; i < MAX_DIR && !here->exit[i]; i++);

            block->loops[lp].d.l.next.door = i;
            break;

        case ENT_MOBILE:
            //log_stringf("opc_list: list type ENT_MOBILE");
            block->loops[lp].d.l.cur.m = block->loops[lp].d.l.next.m;
            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.m);

            /*
            if(block->loops[lp].d.l.cur.m) {
                ch = block->loops[lp].d.l.cur.m;
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            if(!block->loops[lp].d.l.cur.m) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.m = block->loops[lp].d.l.cur.m->next_in_room;
            break;

        case ENT_OBJECT:
            //log_stringf("opc_list: list type ENT_OBJECT");
            block->loops[lp].d.l.cur.o = block->loops[lp].d.l.next.o;
            // Set the variable
            variables_set_object(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.o);

            /*
            if(block->loops[lp].d.l.cur.o)
                log_stringf("opc_list: object(%ld,%ld,%ld)", block->loops[lp].d.l.cur.o->pIndexData->vnum, block->loops[lp].d.l.cur.o->id[0], block->loops[lp].d.l.cur.o->id[1]);
            else
                log_stringf("opc_list: object(<END>)");
                */

            if(!block->loops[lp].d.l.cur.o) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.o = block->loops[lp].d.l.cur.o->next_content;
            break;

        case ENT_TOKEN:
            //log_stringf("opc_list: list type ENT_TOKEN");
            block->loops[lp].d.l.cur.t = block->loops[lp].d.l.next.t;
            // Set the variable
            variables_set_token(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.t);

            /*
            if(block->loops[lp].d.l.cur.t)
                log_stringf("opc_list: token(%ld,%ld,%ld)", block->loops[lp].d.l.cur.t->pIndexData->vnum, block->loops[lp].d.l.cur.t->id[0], block->loops[lp].d.l.cur.t->id[1]);
            else
                log_stringf("opc_list: token(<END>)");
                */

            if(!block->loops[lp].d.l.cur.t) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.t = block->loops[lp].d.l.cur.t->next;
            break;

        case ENT_AFFECT:
            //log_stringf("opc_list: list type ENT_AFFECT");
            block->loops[lp].d.l.cur.aff = block->loops[lp].d.l.next.aff;
            // Set the variable
            variables_set_affect(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.aff);

            /*
            if(block->loops[lp].d.l.cur.aff) {
                if(block->loops[lp].d.l.cur.aff->custom_name)
                    log_stringf("opc_list: affect(%s)", block->loops[lp].d.l.cur.aff->custom_name);
                else
                    log_stringf("opc_list: affect(%s)", skill_table[block->loops[lp].d.l.cur.aff->type].name);
            } else
                log_stringf("opc_list: affect(<END>)");
                */

            if(!block->loops[lp].d.l.cur.aff) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.aff = block->loops[lp].d.l.cur.aff->next;
            break;

        case ENT_TRAINER_ENTRY:
            block->loops[lp].d.l.cur.trainer_entry = block->loops[lp].d.l.next.trainer_entry;
            variables_set_trainer_entry(block->info.var,block->loops[lp].var_name,block->loops[lp].d.l.cur.trainer_entry);

            if(!block->loops[lp].d.l.cur.trainer_entry) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.trainer_entry = block->loops[lp].d.l.cur.trainer_entry->next;
            break;

        case ENT_EXTRADESC:
            block->loops[lp].d.l.cur.ed = block->loops[lp].d.l.next.ed;
            ed = block->loops[lp].d.l.cur.ed;
            // Set the variable
            variables_set_string(block->info.var,block->loops[lp].var_name, ed ? ed->keyword : &str_empty[0], false);
            if(!ed) {
                skip = true;
                break;
            }

            block->loops[lp].d.l.next.ed = ed->next;
            break;

        case ENT_PLLIST_STR:
            //log_stringf("opc_list: list type ENT_PLLIST_STR");
            str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            /*
            if(str)
                log_stringf("opc_list: string(%s)",str);
            else
                log_stringf("opc_list: string(<END>)");
                */

            // Set the variable
            variables_set_string(block->info.var,block->loops[lp].var_name,str?str:&str_empty[0],false);

            if( !str ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            break;

        case ENT_ILLIST_AURA_STR:
            str = (char *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            variables_set_string(block->info.var,block->loops[lp].var_name,str?str:&str_empty[0],false);

            if( !str ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
            }

            break;

        case ENT_BLLIST_MOB:
            //log_stringf("opc_list: list type ENT_BLLIST_MOB");
            while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((CHAR_DATA *)uid->ptr) );

            /*
            if(uid) {
                ch = (CHAR_DATA *)(uid->ptr);
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            if( uid )
            {
                if( IS_VALID((CHAR_DATA *)uid->ptr) )
                    variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)uid->ptr);
                else
                    variables_set_mobile_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
            }
            else
                variables_set_mobile(block->info.var,block->loops[lp].var_name,(CHAR_DATA *)NULL);

            if( !uid ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_BLLIST_OBJ:
            //log_stringf("opc_list: list type ENT_BLLIST_OBJ");
            while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((OBJ_DATA *)uid->ptr) );
            /*
            if(uid) {
                obj = (OBJ_DATA *)(uid->ptr);
                log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
            } else
                log_stringf("opc_list: object(<END>)");
                */

            if( uid )
            {
                if( IS_VALID((OBJ_DATA *)uid->ptr) )
                    variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)uid->ptr);
                else
                    variables_set_object_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
            }
            else
                variables_set_object(block->info.var,block->loops[lp].var_name,(OBJ_DATA *)NULL);

            if( !uid ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_BLLIST_TOK:
            //log_stringf("opc_list: list type ENT_BLLIST_TOK");
            while( (uid = (LLIST_UID_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !IS_VALID((TOKEN_DATA *)uid->ptr) );

            /*
            if(uid) {
                tok = (TOKEN_DATA *)(uid->ptr);
                log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
            } else
                log_stringf("opc_list: token(<END>)");
                */

            if( uid )
            {
                if( IS_VALID((TOKEN_DATA *)uid->ptr) )
                    variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)uid->ptr);
                else
                    variables_set_token_id(block->info.var,block->loops[lp].var_name,uid->id[0],uid->id[1],false);
            }
            else
                variables_set_token(block->info.var,block->loops[lp].var_name,(TOKEN_DATA *)NULL);

            if( !uid ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_BLLIST_ROOM:
            //log_stringf("opc_list: list type ENT_BLLIST_ROOM");
            while( (lrd = (LLIST_ROOM_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lrd->room );
            /*
            if(lrd) {
                if(lrd->room->wilds)
                    log_stringf("opc_list: room(%ld,%ld,%ld)", lrd->room->wilds->uid, lrd->room->x, lrd->room->y);
                else if(lrd->room->source)
                    log_stringf("opc_list: room(%ld,%s,%ld,%ld)", lrd->room->vnum, lrd->room->name, lrd->room->id[0], lrd->room->id[1]);
                else
                    log_stringf("opc_list: room(%ld,%s)", lrd->room->vnum, lrd->room->name);
            } else
                log_stringf("opc_list: room(<END>)");
                */

            variables_set_room(block->info.var,block->loops[lp].var_name,lrd?lrd->room:NULL);

            if( !lrd ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }
            break;

        case ENT_BLLIST_EXIT:
            //log_stringf("opc_list: list type ENT_BLLIST_EXIT");
            while( (led = (LLIST_EXIT_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) &&
                (!led->room || led->door < 0 || led->door >= MAX_DIR || !led->room->exit[led->door]));
            /*
            if(led) {
                if(led->room->wilds)
                    log_stringf("opc_list: %s(%ld,%ld,%ld)", dir_name[led->door], led->room->wilds->uid, led->room->x, led->room->y);
                else if(led->room->source)
                    log_stringf("opc_list: %s(%ld,%s,%ld,%ld)", dir_name[led->door], led->room->vnum, led->room->name, led->room->id[0], led->room->id[1]);
                else
                    log_stringf("opc_list: %s(%ld,%s)", dir_name[led->door], led->room->vnum, led->room->name);
            } else
                log_stringf("opc_list: exit(<END>)");
                */

            if( !led ) {
                variables_set_door(block->info.var,block->loops[lp].var_name,NULL, DIR_NORTH, false);
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            variables_set_door(block->info.var,block->loops[lp].var_name,led->room, led->door, false);
            break;

        case ENT_BLLIST_SKILL:
            //log_stringf("opc_list: list type ENT_BLLIST_SKILL");
            while( (lsk = (LLIST_SKILL_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) &&
                (!IS_VALID(lsk->mob) || (!IS_VALID(lsk->tok) && ( lsk->sn < 1 || lsk->sn >= MAX_SKILL ))) );
            /*
            if(lsk) {
                if(lsk->tok)
                    log_stringf("opc_list: skill(%ld,%ld,TOKEN,%s)", lsk->mob->id[0], lsk->mob->id[1], lsk->tok->name);
                else
                    log_stringf("opc_list: skill(%ld,%ld,SKILL,%s)", lsk->mob->id[0], lsk->mob->id[1], skill_table[lsk->sn].name);
            } else
                log_stringf("opc_list: skill(<END>)");
                */

            if( !lsk ) {
                variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, NULL, 0, NULL, false);
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            variables_setsave_skillinfo(block->info.var,block->loops[lp].var_name, lsk->mob, lsk->sn, lsk->tok, false);
            break;

        case ENT_BLLIST_AREA:
            //log_stringf("opc_list: list type ENT_BLLIST_AREA");
            while( (lar = (LLIST_AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lar->area );
            /*
            if(lar) {
                log_stringf("opc_list: area(%ld,%s)", lar->area->uid, lar->area->name);
            } else
                log_stringf("opc_list: area(<END>)");
                */

            variables_set_area(block->info.var,block->loops[lp].var_name,lar?lar->area:NULL);

            if( !lar ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }
            break;

        case ENT_BLLIST_AREA_REGION:
            while( (lareg = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lareg->aregion );

            variables_set_area_region(block->info.var,block->loops[lp].var_name,lareg?lareg->aregion:NULL);

            if( !lareg ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }
            break;

        case ENT_BLLIST_WILDS:
            //log_stringf("opc_list: list type ENT_BLLIST_WILDS");
            while( (lwd = (LLIST_WILDS_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it)) && !lwd->wilds );
            /*
            if(lwd) {
                log_stringf("opc_list: wilds(%ld,%s)", lwd->wilds->uid, lwd->wilds->name);
            } else
                log_stringf("opc_list: wilds(<END>)");
                */

            variables_set_wilds(block->info.var,block->loops[lp].var_name,lwd?lwd->wilds:NULL);

            if( !lwd ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }
            break;

        case ENT_PLLIST_CONN:
            //log_stringf("opc_list: list type ENT_PLLIST_CONN");
            conn = (DESCRIPTOR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(conn) {
                if(conn->original)
                    log_stringf("opc_list: connection(%s,%d) [SWITCHED]", conn->original->name, conn->original->tot_level);
                else
                    log_stringf("opc_list: connection(%s,%d)", conn->character->name, conn->character->tot_level);
            } else
                log_stringf("opc_list: connection(<END>)");
                */

            // Set the variable
            variables_set_connection(block->info.var,block->loops[lp].var_name,conn);

            if( !conn ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_MOB:
            //log_stringf("opc_list: list type ENT_PLLIST_MOB");
            ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(ch) {
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);

            if( !ch ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_OBJ:
            //log_stringf("opc_list: list type ENT_PLLIST_OBJ");
            obj = (OBJ_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(obj)
                log_stringf("opc_list: object(%ld,%ld,%ld)", obj->pIndexData->vnum, obj->id[0], obj->id[1]);
            else
                log_stringf("opc_list: object(<END>)");
                */

            // Set the variable
            variables_set_object(block->info.var,block->loops[lp].var_name,obj);

            if( !obj ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_ROOM:
            //log_stringf("opc_list: list type ENT_PLLIST_ROOM");
            here = (ROOM_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(here) {
                if(here->wilds)
                    log_stringf("opc_list: room(%ld,%ld,%ld)", here->wilds->uid, here->x, here->y);
                else if(here->source)
                    log_stringf("opc_list: room(%ld,%s,%ld,%ld)", here->vnum, here->name, here->id[0], here->id[1]);
                else
                    log_stringf("opc_list: room(%ld,%s)", here->vnum, here->name);
            } else
                log_stringf("opc_list: room(<END>)");
                */

            // Set the variable
            variables_set_room(block->info.var,block->loops[lp].var_name,here);

            if( !here ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_TOK:
            //log_stringf("opc_list: list type ENT_PLLIST_TOK");
            tok = (TOKEN_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(tok)
                log_stringf("opc_list: token(%ld,%ld,%ld)", tok->pIndexData->vnum, tok->id[0], tok->id[1]);
            else
                log_stringf("opc_list: token(<END>)");
                */

            // Set the variable
            variables_set_token(block->info.var,block->loops[lp].var_name,tok);

            if( !tok ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }
            break;

        case ENT_PLLIST_AREA:
            area = (AREA_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_area(block->info.var,block->loops[lp].var_name,area);

            if( !area ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_AREA_REGION:
            aregion = (AREA_REGION *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_area_region(block->info.var,block->loops[lp].var_name,aregion);

            if( !aregion ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_CHURCH:
            //log_stringf("opc_list: list type ENT_PLLIST_CHURCH");
            church = (CHURCH_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: church(%s)", church ? church->name : "<END>");

            // Set the variable
            variables_set_church(block->info.var,block->loops[lp].var_name,church);

            if( !church ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_BOOK_PAGE:
            book_page = (BOOK_PAGE *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_book_page(block->info.var,block->loops[lp].var_name,book_page);

            if( !book_page ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_FOOD_BUFF:
            food_buff = (FOOD_BUFF_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_food_buff(block->info.var,block->loops[lp].var_name,food_buff);

            if( !food_buff ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_PLLIST_REPUTATION_RANK:
            repRank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_reputation_rank(block->info.var,block->loops[lp].var_name,repRank);

            if( !repRank ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;
        case ENT_ILLIST_VARIABLE:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            variable = (VARIABLE *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            // Set the variable
            variables_set_variable(block->info.var,block->loops[lp].var_name,variable);

            if( !variable) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_REPUTATION:
            reputation = (REPUTATION_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_reputation(block->info.var,block->loops[lp].var_name,reputation);

            if( !reputation ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_REPUTATION_INDEX:
            repIndex = (REPUTATION_INDEX_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_reputation_index(block->info.var,block->loops[lp].var_name,repIndex);

            if( !repIndex ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_SKILLGROUPS:
            skill_group = (SKILL_GROUP *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_skill_group(block->info.var,block->loops[lp].var_name,skill_group);

            if( !skill_group ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_QUEST_STAGES:
            quest_stage = (QUEST_STAGE_INDEX_V2_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_quest_stage(block->info.var,block->loops[lp].var_name,quest_stage);

            if( !quest_stage ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_QUEST_OBJECTIVES:
            quest_objective = (QUEST_OBJECTIVE_INDEX_V2_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_quest_objective(block->info.var,block->loops[lp].var_name,quest_objective);

            if( !quest_objective ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_QUEST:
            quest_run = (QUEST_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_quest(block->info.var,block->loops[lp].var_name,quest_run);

            if( !quest_run ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_QUEST_HISTORY:
            quest_hist = (QUEST_HISTORY_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            variables_set_quest_history(block->info.var,block->loops[lp].var_name,quest_hist);

            if( !quest_hist ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_EVENT:
            event_ref = (EVENT_RUNTIME_REF *)iterator_nextdata(&block->loops[lp].d.l.list.it);

            if( event_ref )
                variables_set_event(block->info.var,block->loops[lp].var_name,*event_ref);
            else
                variables_set_event(block->info.var,block->loops[lp].var_name,empty_event_ref);

            if( !event_ref ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_MOB_GROUP:
            //log_stringf("opc_list: list type ENT_ILLIST_MOB_GROUP");
            ch = (CHAR_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            /*
            if(ch) {
                if(!IS_NPC(ch))
                    log_stringf("opc_list: player(%s,%ld,%ld)", ch->name, ch->id[0], ch->id[1]);
                else
                    log_stringf("opc_list: mobile(%ld,%ld,%ld)", ch->pIndexData->vnum, ch->id[0], ch->id[1]);
            } else
                log_stringf("opc_list: mobile(<END>)");
                */

            // Set the variable
            variables_set_mobile(block->info.var,block->loops[lp].var_name,ch);

            if( !ch ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                // This needs to be destroyed
                list_destroy(block->loops[lp].d.l.list.lp);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_SECTIONS:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            section = (INSTANCE_SECTION *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            // Set the variable
            variables_set_instance_section(block->info.var,block->loops[lp].var_name,section);

            if( !section ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_INSTANCES:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            instance = (INSTANCE *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            // Set the variable
            variables_set_instance(block->info.var,block->loops[lp].var_name,instance);

            if( !instance ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_SPECIALROOMS:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            special_room = (NAMED_SPECIAL_ROOM *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            // Set the variable
            variables_set_room(block->info.var,block->loops[lp].var_name,special_room ? special_room->room : NULL);

            if( !special_room ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        case ENT_ILLIST_SHIPS:
            //log_stringf("opc_list: list type ENT_ILLIST_VARIABLE");
            ship = (SHIP_DATA *)iterator_nextdata(&block->loops[lp].d.l.list.it);
            //log_stringf("opc_list: variable(%s)", variable ? variable->name : "<END>");

            // Set the variable
            variables_set_ship(block->info.var,block->loops[lp].var_name,ship);

            if( !ship ) {
                iterator_stop(&block->loops[lp].d.l.list.it);
                skip = true;
                break;
            }

            break;

        }

        if(skip) {
            block->loops[lp].valid = false;
            block->loop--;
            return opc_skip_to_label(block,OP_ENDLIST,block->loops[lp].id,true);
        }

        block->cond[block->cur_line->level] = true;
    }

    opc_next_line(block);
    return true;
}


DECL_OPC_FUN(opc_endlist)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_LIST,block->cur_line->label,false);
}

DECL_OPC_FUN(opc_exitlist)
{
    script_loop_cleanup(block, block->cur_line->level);
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_ENDLIST,block->cur_line->label,true);
}

DECL_OPC_FUN(opc_endwhile)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_WHILE,block->cur_line->label,false);
}

DECL_OPC_FUN(opc_exitwhile)
{
    script_loop_cleanup(block, block->cur_line->level);
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    return opc_skip_to_label(block,OP_ENDWHILE,block->cur_line->label,true);
}

DECL_OPC_FUN(opc_switch)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    SCRIPT_PARAM *arg = new_script_param();
    if(!expand_argument(&block->info,block->cur_line->rest,arg)) {
        block->ret_val = PRET_BADSYNTAX;
        free_script_param(arg);
        return false;
    }

    if (arg->type != ENT_NUMBER)
    {
        block->ret_val = PRET_BADSYNTAX;
        free_script_param(arg);
        return false;
    }

    long value = arg->d.num;
    free_script_param(arg);

    if (block->script->switch_table && block->cur_line->param >= 0 && block->cur_line->param < block->script->n_switch_table)
    {
        SCRIPT_SWITCH *sw = &block->script->switch_table[block->cur_line->param];
        SCRIPT_SWITCH_CASE *swc;

        for(swc = sw->cases; swc; swc = swc->next)
        {
            if (value >= swc->a && value <= swc->b)
            {
                if(swc->line >= 0 && swc->line < block->script->lines) {
                    block->line = swc->line;
                    block->cur_line = &block->script->code[swc->line];
                    script_loop_cleanup(block, block->cur_line->level);
                    if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = true;
                    return true;
                }
                return false;
            }
        }

        // While lines start at 0, the default in a switch statement can never point to line 0, as the switch statement itself (this opcode) must come before it
        if(sw->default_case > 0 && sw->default_case < block->script->lines) {
            block->line = sw->default_case;
            block->cur_line = &block->script->code[sw->default_case];
            script_loop_cleanup(block, block->cur_line->level);
            if(block->cur_line->level > 0) block->cond[block->cur_line->level-1] = true;
            return true;
        }

        // Skip to the end of the switch
        return opc_skip_to_label(block,OP_ENDSWITCH,block->cur_line->level, true);
    }

    // To get to here means something bad happened
    block->ret_val = PRET_BADSYNTAX;
    return false;
}

DECL_OPC_FUN(opc_endswitch)
{
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_exitswitch)
{
    // Skip to the end of the switch
    return opc_skip_to_label(block,OP_ENDSWITCH,block->cur_line->level, true);
}

DECL_OPC_FUN(opc_mob)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_M) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,mob_cmd_table[block->cur_line->param].name);

    if(mob_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted mob command '%s' with nulled security.",mob_cmd_table[block->cur_line->param].name);
    } else if(IS_VALID(block->info.mob)) {
        if( !mob_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*mob_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }


    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_obj)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_O) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,obj_cmd_table[block->cur_line->param].name);

    if(obj_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted obj command '%s' with nulled security.",obj_cmd_table[block->cur_line->param].name);
    } else if(IS_VALID(block->info.obj)) {
        if( !obj_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*obj_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_room)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_R) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,room_cmd_table[block->cur_line->param].name);

    if(room_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted room command '%s' with nulled security.",room_cmd_table[block->cur_line->param].name);
    } else if(block->info.room) {
        if( !room_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*room_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_token)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_T) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,token_cmd_table[block->cur_line->param].name);

    if(token_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted token command '%s' with nulled security.",token_cmd_table[block->cur_line->param].name);
    } else if(IS_VALID(block->info.token)) {
        if( !token_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*token_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_tokenother)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type == IFC_T) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,tokenother_cmd_table[block->cur_line->param].name);

    if(tokenother_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted tokenother command '%s' with nulled security.",tokenother_cmd_table[block->cur_line->param].name);
    } else {
        if( !tokenother_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*tokenother_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }
    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_area)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_A) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,area_cmd_table[block->cur_line->param].name);

    if(area_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted area command '%s' with nulled security.",area_cmd_table[block->cur_line->param].name);
    } else if(block->info.area) {
        if( !area_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*area_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_quest)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    if(block->type != IFC_Q) {
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,area_cmd_table[block->cur_line->param].name);

    if(area_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted quest command '%s' with nulled security.",area_cmd_table[block->cur_line->param].name);
    } else if(block->info.area) {
        if( !area_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*area_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_instance)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_I) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,instance_cmd_table[block->cur_line->param].name);

    if(instance_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted instance command '%s' with nulled security.",instance_cmd_table[block->cur_line->param].name);
    } else if(IS_VALID(block->info.instance)) {
        if( !instance_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*instance_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }

    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_dungeon)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    // Verify
    if(block->type != IFC_D) {
        // Log the error
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,dungeon_cmd_table[block->cur_line->param].name);

    if(area_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted dungeon command '%s' with nulled security.",dungeon_cmd_table[block->cur_line->param].name);
    } else if(IS_VALID(block->info.dungeon)) {
        if( !dungeon_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*dungeon_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }


    opc_next_line(block);
    return true;
}

DECL_OPC_FUN(opc_event)
{
    if(block->cur_line->level > 0 && !block->cond[block->cur_line->level-1])
        return opc_skip_block(block,block->cur_line->level-1,false);

    if(block->type != IFC_E) {
        return false;
    }

    DBG3MSG2("Executing: %d(%s)\n", block->cur_line->param,evt_cmd_table[block->cur_line->param].name);

    if(evt_cmd_table[block->cur_line->param].restricted && script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "Attempted execution of a restricted evt command '%s' with nulled security.",evt_cmd_table[block->cur_line->param].name);
    } else if(block->info.area) {
        if( !evt_cmd_table[block->cur_line->param].required || !IS_NULLSTR(block->cur_line->rest) ) {
            SCRIPT_PARAM *arg = new_script_param();
            (*evt_cmd_table[block->cur_line->param].func) (&block->info,block->cur_line->rest, arg);
            free_script_param(arg);
            tail_chain();
        }
    }

    opc_next_line(block);
    return true;
}


bool echo_line(SCRIPT_CB *block)
{
    char buf[MSL];
    DBG3MSG4("Executing: Line=%d, Opcode=%d(%s), Level=%d\n", block->line+1,block->cur_line->opcode,opcode_names[block->cur_line->opcode],block->cur_line->level);
    if(wiznet_script) {
        sprintf(buf,"Executing: Line=%d, Opcode=%d(%s), Level=%d", block->line+1,block->cur_line->opcode,opcode_names[block->cur_line->opcode],block->cur_line->level);
        emit_script_staff_event(buf, buf, "execute_line", block->script);
    }
    return true;
}

void script_dump(SCRIPT_DATA *script)
{
#ifdef DEBUG_MODULE
    int i;
    DBG3MSG2("vnum = %d, lines = %d\n", script->vnum, script->lines);
    if(script->code) {
        for(i=0; i < script->lines; i++) {
            DBG3MSG4("Line %d: Opcode=%d(%s), Level=%d\n", i+1,script->code[i].opcode,opcode_names[script->code[i].opcode],script->code[i].level);
        }
    }

#endif
}

void script_dump_wiznet(SCRIPT_DATA *script)
{
    int i;
    char buf[MSL];
    sprintf(buf,"vnum = %d, lines = %d", script->vnum, script->lines);
    emit_script_staff_event(buf, buf, "script_dump", script);
    if(script->code) {
        for(i=0; i < script->lines; i++) {
            sprintf(buf,"Line %d: Opcode=%d(%s), Level=%d", i+1,script->code[i].opcode,opcode_names[script->code[i].opcode],script->code[i].level);
            emit_script_staff_event(buf, buf, "script_dump", script);
        }
    }
}

int execute_script(long pvnum, SCRIPT_DATA *script,
    CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
    AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
    CHAR_DATA *ch, OBJ_DATA *obj1,OBJ_DATA *obj2,CHAR_DATA *vch,CHAR_DATA *vch2,CHAR_DATA *rch,
    TOKEN_DATA *tok, char *phrase, char *trigger, int trigger_type,
    int number1, int number2, int number3, int number4, int number5)
{
    char buf[MSL];
    SCRIPT_CB block;	// Control block
    SCRIPT_VARINFO runtime_info;
    int saved_call_depth;	// Call depth copied
    int saved_security;	// Security copied
    bool saved_wiznet;
    long level, i;

    DBG2ENTRY7(NUM,pvnum,PTR,script,PTR,mob,PTR,obj,PTR,room,PTR,token,PTR,ch);

    script_destructed = false;

    script_build_runtime_context(
        &runtime_info,
        mob, obj, room, token,
        area, instance, dungeon,
        ch, vch, vch2, rch, tok,
        phrase, trigger, trigger_type);

    if (!script || !script->code) {
        script_log_runtime_error_context(script, 0,
            formatf("No script bytecode available for execution (vnum %ld).", pvnum), &runtime_info);
        pbugf(LOG_SCRIPTS, "PROGs: No script to execute for vnum %d.", pvnum);
        return PRET_NOSCRIPT;
    }

    if (!script->src || script->src[0] == '\0') {
        script_log_runtime_error_context(script, 0,
            formatf("No script source available for execution (vnum %ld).", pvnum), &runtime_info);
        pbugf(LOG_SCRIPTS, "PROGs: No script source to execute for vnum %d.", pvnum);
        return PRET_NOSCRIPT;
    }

    if (IS_VALID(mob) && !IS_NPC(mob) )
    {
        script_log_runtime_error_context(script, 0,
            formatf("Attempted to run script with a player actor (vnum %ld).", pvnum), &runtime_info);
        pbugf(LOG_SCRIPTS, "PROGs: Attempting to run a script with a player actor for vnum %d.", pvnum);
        return PRET_NOSCRIPT;
    }

    if ((mob && obj) || (mob && room) || (mob && token) || (mob && area) || (mob && instance) || (mob && dungeon) ||
        (obj && room) || (obj && token) || (obj && area) || (obj && instance) || (obj && dungeon) ||
        (room && token) || (room && area) || (room && instance) || (room && dungeon) ||
        (token && area) || (token && instance) || (token && dungeon) ||
        (area && instance) || (area && dungeon) ||
        (instance && dungeon)) {
        script_log_runtime_error_context(script, 0,
            formatf("Script dispatch received multiple conflicting entity contexts (vnum %ld).", pvnum), &runtime_info);
        pbugf(LOG_SCRIPTS, "PROGs: program_flow received multiple prog types for vnum %d.", pvnum);
        return PRET_BADTYPE;
    }

    // Silently return.  Disabled scripts should just pretend they don't exist.
    if (!script_force_execute && IS_SET(script->flags,SCRIPT_DISABLED))
        return PRET_NOSCRIPT;

    // System scripts require system level security, only set at specific times!
    if(IS_SET(script->flags, SCRIPT_SYSTEM) && script_security < SYSTEM_SCRIPT_SECURITY)
        return PRET_NOSCRIPT;


    memset(&block,0,sizeof(block));
    block.info.block = &block;
    for(i = 0; i < MAX_NESTED_LOOPS; i++) {
        block.loops[i].valid = false;
        block.loops[i].level = -1;
    }

    saved_wiznet = wiznet_script;
    wiznet_script = (bool)(int)IS_SET(script->flags,SCRIPT_WIZNET);

    if (mob) {
        mob->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_M;
        block.info.progs = mob->progs;
        block.info.location = mob->in_room;
        block.info.var = &mob->progs->vars;
        block.info.targ = &mob->progs->target;

    } else if (obj) {
        obj->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_O;
        block.info.progs = obj->progs;
        block.info.location = obj_room(obj);
        block.info.var = &obj->progs->vars;
        block.info.targ = &obj->progs->target;

    } else if (room) {
        room->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_R;
        block.info.progs = room->progs;
        block.info.location = room;
        block.info.var = &room->progs->vars;
        block.info.targ = &room->progs->target;

    } else if (token) {
        token->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_T;
        block.info.progs = token->progs;
        block.info.location = token_room(token);
        block.info.var = &token->progs->vars;
        block.info.targ = &token->progs->target;

    } else if (area) {
        area->progs->lastreturn = PRET_EXECUTED;
        if (script && script->type == PRG_QPROG)
            block.type = IFC_Q;
        else if (script && script->type == PRG_EPROG)
            block.type = IFC_E;
        else
            block.type = IFC_A;
        block.info.progs = area->progs;
        block.info.location = NULL;
        block.info.var = &area->progs->vars;
        block.info.targ = &area->progs->target;

    } else if (instance) {
        instance->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_I;
        block.info.progs = instance->progs;
        block.info.location = NULL;
        block.info.var = &instance->progs->vars;
        block.info.targ = &instance->progs->target;

    } else if (dungeon) {
        dungeon->progs->lastreturn = PRET_EXECUTED;
        block.type = IFC_D;
        block.info.progs = dungeon->progs;
        block.info.location = NULL;
        block.info.var = &dungeon->progs->vars;
        block.info.targ = &dungeon->progs->target;

    } else {
        // Log error
        return PRET_BADTYPE;
    }

    if(phrase) strlcpy(block.info.phrase, phrase, sizeof(block.info.phrase));
    if(trigger) strlcpy(block.info.trigger, trigger, sizeof(block.info.trigger));

    if(wiznet_script) {
        sprintf(buf,"{BScript{C({W%d{C){D: {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){D, {B%s{C({W%d{C){x",
            script ? script->vnum : -1,
            mob ? HANDLE(mob) : "(mob)",
            mob ? (int)VNUM(mob) : -1,
            obj ? obj->short_descr : "(obj)",
            obj ? (int)VNUM(obj) : -1,
            room ? room->name : "(room)",
            room ? (int)room->vnum : -1,
            token ? token->name : "(token)",
            token ? (int)VNUM(token) : -1,
            ch ? HANDLE(ch) : "(ch)",
            ch ? (int)VNUM(ch) : -1);
        emit_script_staff_event(buf, buf, "execute_script", script);
        script_dump_wiznet(script);
    }

    // Save script parameters
    block.info.mob = mob;
    block.info.obj = obj;
    block.info.room = room;
    block.info.token = token;
    block.info.area = area;
    block.info.instance = instance;
    block.info.dungeon = dungeon;
    block.info.quest = script_exec_context.quest;
    block.info.ch = ch;
    block.info.obj1 = obj1;
    block.info.obj2 = obj2;
    block.info.vch = vch;
    block.info.vch2 = vch2;
    block.info.rch = rch;
    block.info.tok = tok;
    block.info.registers[0] = number1;
    block.info.registers[1] = number2;
    block.info.registers[2] = number3;
    block.info.registers[3] = number4;
    block.info.registers[4] = number5;
    block.script = script;

    saved_security = script_security;

    // Non-system scripts modify the security
    if(!IS_SET(script->flags, SCRIPT_SYSTEM)) {
        if(script_security == INIT_SCRIPT_SECURITY ||
            (IS_SET(script->flags,SCRIPT_SECURED) && (script->security >= script_security)) ||
            script->security < script_security)
            script_security = script->security;
    }

    // Call depth code
    saved_call_depth = script_call_depth;
    if(!script_call_depth) {
        if(!script->depth)
            script_call_depth = MAX_CALL_LEVEL;
        else
            script_call_depth = script->depth;
    }

    // Init stack
    for (level = 0; level < MAX_NESTED_LEVEL; level++) {
        block.state[level] = IN_BLOCK;
        block.cond[level]  = true;
    }
    block.level = 0;
    block.line = 0;
    block.loop = 0;
    block.ret_val = PRET_EXECUTED;
    block.cur_line = &block.script->code[block.line];
    block.next = script_call_stack;
    script_call_stack = &block;

    DBG3MSG0("Starting script...\n");
    if(wiznet_script) emit_script_staff_event("Starting script...", "Starting script...",
                                              "execute_script_start", script);

    // Run script
    // Until the number of lines of the script has been reached or
    //	An opcode function tells it to quit.
    while(block.line < block.script->lines &&
        block.cur_line->opcode < OP_LASTCODE &&
        echo_line(&block) &&
        (*opcode_table[block.cur_line->opcode])(&block) &&
        !IS_SET(block.flags,SCRIPTEXEC_HALT)) {

        if(block.line < block.script->lines)
            block.cur_line = &block.script->code[block.line];
    }

    if(IS_SET(block.flags,SCRIPTEXEC_HALT)) script_destructed = true;

    DBG3MSG0("Completed script...\n");
    if(wiznet_script) emit_script_staff_event(
        (script_destructed ? "Script halted due to entity destruction..." : "Completed script..."),
        (script_destructed ? "Script halted due to entity destruction..." : "Completed script..."),
        "execute_script_end", script);

    wiznet_script = saved_wiznet;

    DBG2EXITVALUE1(NUM,block.ret_val);
    script_security = saved_security;
    script_call_depth = saved_call_depth; // Restore call depth
    script_call_stack = script_call_stack->next; // Back up call stack

    script_loop_cleanup(&block, 0);

    return block.ret_val;
}

const SCRIPT_EXECUTE_CONTEXT *script_get_execute_context(void)
{
    return &script_exec_context;
}

SCRIPT_EXECUTE_CONTEXT script_set_execute_context(const SCRIPT_EXECUTE_CONTEXT *context)
{
    SCRIPT_EXECUTE_CONTEXT saved_context = script_exec_context;

    if (context)
        script_exec_context = *context;
    else {
        script_exec_context.quest = NULL;
        script_exec_context.event = NULL;
    }

    return saved_context;
}

QUEST_DATA *script_set_execute_quest_context(QUEST_DATA *quest)
{
    SCRIPT_EXECUTE_CONTEXT context = script_exec_context;
    QUEST_DATA *saved_quest = context.quest;

    context.quest = quest;
    script_set_execute_context(&context);

    return saved_quest;
}

int execute_script_quest(long pvnum, SCRIPT_DATA *script, QUEST_DATA *quest,
    CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
    AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
    CHAR_DATA *ch, OBJ_DATA *obj1,OBJ_DATA *obj2,CHAR_DATA *vch,CHAR_DATA *vch2,CHAR_DATA *rch,
    TOKEN_DATA *tok, char *phrase, char *trigger, int trigger_type,
    int number1, int number2, int number3, int number4, int number5)
{
    SCRIPT_EXECUTE_CONTEXT context;
    SCRIPT_EXECUTE_CONTEXT saved_context;
    int ret;

    context = script_exec_context;
    context.quest = quest;
    saved_context = script_set_execute_context(&context);

    ret = execute_script(pvnum, script,
        mob, obj, room, token,
        area, instance, dungeon,
        ch, obj1, obj2, vch, vch2, rch,
        tok, phrase, trigger, trigger_type,
        number1, number2, number3, number4, number5);
    script_set_execute_context(&saved_context);

    return ret;
}

static ROOM_INDEX_DATA *script_room_from_obj_or_token(OBJ_DATA *obj, TOKEN_DATA *token)
{
    if (obj)
        return obj_room(obj);
    if (token)
        return token_room(token);
    return NULL;
}

static ROOM_INDEX_DATA *script_room_from_context(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token)
{
    if (mob)
        return mob->in_room;
    if (obj || token)
        return script_room_from_obj_or_token(obj, token);
    return room;
}

static ROOM_INDEX_DATA *script_room_from_info_context(SCRIPT_VARINFO *info)
{
    if (!info)
        return NULL;
    return script_room_from_context(info->mob, info->obj, info->room, info->token);
}

bool script_get_quest_metrics(const CHAR_DATA *mob, int *points, int *total_completed, bool *active)
{
    bool valid_player = (mob && !IS_NPC(mob) && mob->pcdata);

    if (points)
        *points = valid_player ? mob->questpoints : 0;

    if (total_completed)
        *total_completed = valid_player ? mob->pcdata->quests_completed : 0;

    if (active)
        *active = valid_player ? ON_QUEST(mob) : false;

    return valid_player;
}

bool script_get_mission_metrics(const CHAR_DATA *mob, int *points, int *total_completed, bool *active)
{
    return script_get_quest_metrics(mob, points, total_completed, active);
}

bool script_adjust_quest_points(CHAR_DATA *mob, int delta, int *applied_delta)
{
    int before;
    int after;

    if (applied_delta)
        *applied_delta = 0;

    if (!mob || IS_NPC(mob) || !mob->pcdata)
        return false;

    before = mob->questpoints;
    after = before + delta;

    if (after < 0)
        after = 0;

    mob->questpoints = after;

    if (applied_delta)
        *applied_delta = (after - before);

    return true;
}

bool script_get_class_metrics(const CHAR_DATA *mob, CLASS_DATA **current_class, CLASS_LEVEL **current_level, int *class_count)
{
    bool valid_player = (mob && !IS_NPC(mob) && mob->pcdata);
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    CLASS_LEVEL *level = NULL;

    if (valid_player) {
        level = get_class_level(ch, NULL);
    }

    if (current_class)
        *current_class = valid_player ? get_current_class(ch) : NULL;

    if (current_level)
        *current_level = valid_player ? level : NULL;

    if (class_count)
        *class_count = valid_player ? list_size(ch->pcdata->classes) : 0;

    return valid_player;
}

bool script_get_class_by_name_metrics(const CHAR_DATA *mob, const char *class_name, bool *is_current, int *level, bool *has_class)
{
    bool valid_player = (mob && !IS_NPC(mob) && mob->pcdata);
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    CLASS_DATA *clazz = NULL;
    CLASS_LEVEL *class_level = NULL;

    if (is_current)
        *is_current = false;
    if (level)
        *level = 0;
    if (has_class)
        *has_class = false;

    if (!valid_player || IS_NULLSTR(class_name))
        return false;

    clazz = class_find(class_name);
    if (!clazz)
        return false;

    class_level = get_class_level(ch, clazz);

    if (has_class)
        *has_class = (class_level != NULL);

    if (level)
        *level = class_level ? class_level->level : 0;

    if (is_current)
        *is_current = (get_current_class(ch) == clazz);

    return true;
}

bool script_get_skill_rating_by_sn(const CHAR_DATA *mob, int sn, int *rating)
{
    bool valid_player = (mob && !IS_NPC(mob) && mob->pcdata);
    CHAR_DATA *ch = (CHAR_DATA *)mob;

    if (rating)
        *rating = 0;

    if (!valid_player || sn <= 0)
        return false;

    if (rating)
        *rating = get_skill(ch, sn);

    return true;
}

bool script_get_skill_metrics(const CHAR_DATA *mob, const char *skill_name, int *rating, bool *known)
{
    bool available_now = false;
    bool has_any = false;
    SKILL_DATA *skill;
    int sn;

    if (rating)
        *rating = 0;
    if (known)
        *known = false;

    if (!mob || IS_NULLSTR(skill_name))
        return false;

    if (!script_get_skill_availability(mob, skill_name, &available_now, &has_any))
        return false;

    skill = skill_search(skill_name);
    if (!skill)
        return false;

    sn = skill_sn(skill);
    if (sn < 0 || sn >= MAX_SKILL)
        return false;

    if (rating)
        *rating = available_now ? get_skill((CHAR_DATA *)mob, sn) : 0;

    if (known)
        *known = has_any;

    return true;
}

bool script_get_song_known(const CHAR_DATA *mob, const char *song_name, bool *known)
{
    bool has_any = false;

    if (known)
        *known = false;

    if (!mob || IS_NULLSTR(song_name))
        return false;

    if (!script_get_song_availability(mob, song_name, NULL, &has_any))
        return false;

    if (known)
        *known = has_any;

    return true;
}

bool script_get_skill_availability(const CHAR_DATA *mob, const char *skill_name, bool *available_now, bool *has_any)
{
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    SKILL_DATA *skill;
    SKILL_ENTRY *entry;
    int sn;

    if (available_now)
        *available_now = false;
    if (has_any)
        *has_any = false;

    if (!ch || IS_NULLSTR(skill_name))
        return false;

    skill = skill_search(skill_name);
    if (!skill)
        return false;

    sn = skill_sn(skill);
    if (sn < 0 || sn >= MAX_SKILL)
        return false;

    if (available_now)
        *available_now = (get_skill(ch, sn) > 0);

    if (has_any) {
        if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
            *has_any = (get_skill(ch, sn) > 0);
        } else {
            entry = skill_entry_findsn(ch->sorted_skills, sn);
            *has_any = (entry != NULL) || had_skill(ch, sn) || any_class_grants_skill(ch, sn);
        }
    }

    return true;
}

bool script_get_spell_availability(const CHAR_DATA *mob, const char *spell_name, bool *available_now, bool *has_any)
{
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    SKILL_DATA *skill;
    SKILL_ENTRY *entry;
    int sn;

    if (available_now)
        *available_now = false;
    if (has_any)
        *has_any = false;

    if (!ch || IS_NULLSTR(spell_name))
        return false;

    skill = skill_search(spell_name);
    if (!skill)
        return false;

    sn = skill_sn(skill);
    if (sn < 0 || sn >= MAX_SKILL)
        return false;

    if (!skill_table[sn].spell_fun || skill_table[sn].spell_fun == spell_null)
        return false;

    if (available_now)
        *available_now = (get_skill(ch, sn) > 0);

    if (has_any) {
        if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
            *has_any = (get_skill(ch, sn) > 0);
        } else {
            entry = skill_entry_findsn(ch->sorted_skills, sn);
            *has_any = (entry != NULL) || had_skill(ch, sn) || any_class_grants_skill(ch, sn);
        }
    }

    return true;
}

bool script_get_song_availability(const CHAR_DATA *mob, const char *song_name, bool *available_now, bool *has_any)
{
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    SONG_DATA *song;
    SKILL_ENTRY *entry;

    if (available_now)
        *available_now = false;
    if (has_any)
        *has_any = false;

    if (!ch || IS_NULLSTR(song_name))
        return false;

    song = song_lookup(song_name);
    if (!song)
        return false;

    entry = skill_entry_findsong(ch->sorted_songs, song);

    if (has_any)
        *has_any = (entry != NULL);

    if (available_now)
        *available_now = (entry != NULL) && skill_entry_is_usable_now(ch, entry);

    return true;
}

bool script_get_trait_metrics(const CHAR_DATA *mob, const char *trait_name, bool *available_now, bool *has_any, bool *bool_value, int *int_value, const char **string_value)
{
    CHAR_DATA *ch = (CHAR_DATA *)mob;
    TRAIT_DEF *def;
    ITERATOR it;
    CLASS_LEVEL *cl;

    if (available_now)
        *available_now = false;
    if (has_any)
        *has_any = false;
    if (bool_value)
        *bool_value = false;
    if (int_value)
        *int_value = 0;
    if (string_value)
        *string_value = NULL;

    if (!ch || IS_NULLSTR(trait_name))
        return false;

    def = trait_def_lookup_name(trait_name);
    if (!def)
        return false;

    if (available_now)
        *available_now = ch_has_trait(ch, def->id);

    if (bool_value)
        *bool_value = ch_get_trait_bool(ch, def->id);

    if (int_value)
        *int_value = ch_get_trait_int(ch, def->id);

    if (string_value)
        *string_value = ch_get_trait_string(ch, def->id);

    if (has_any) {
        if (ch->race && ch->race->trait_values && ch->race->trait_values[def->index].set)
            *has_any = true;

        if (!*has_any && ch->orace && ch->orace->trait_values && ch->orace->trait_values[def->index].set)
            *has_any = true;

        if (!*has_any && ch->pcdata && ch->pcdata->trait_values && ch->pcdata->trait_values[def->index].set)
            *has_any = true;

        if (!*has_any && ch->pcdata && ch->pcdata->classes) {
            iterator_start(&it, ch->pcdata->classes);
            while ((cl = (CLASS_LEVEL *)iterator_nextdata(&it))) {
                if (cl->clazz && cl->clazz->trait_values && cl->clazz->trait_values[def->index].set) {
                    *has_any = true;
                    break;
                }
            }
            iterator_stop(&it);
        }
    }

    return true;
}

/*
 * Get a random PC in the room (for $r parameter)
 */
CHAR_DATA *get_random_char(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token)
{
    CHAR_DATA *vch, *victim = NULL;
    ROOM_INDEX_DATA *context_room = NULL;
    int now = 0, highest = 0;

    if ((mob && obj) || (mob && room) || (obj && room)) {
    pbugf(LOG_SCRIPTS, "get_random_char received multiple prog types");
    return NULL;
    }

    context_room = script_room_from_context(mob, obj, room, token);
    if (!context_room) {
        pbugf(LOG_SCRIPTS, "get_random_char: no room, object, or mob!");
        return NULL;
    }
    vch = context_room->people;

    for (; vch; vch = vch->next_in_room) {
        if (mob && mob != vch && !IS_NPC(vch) && can_see(mob, vch) && (now = number_percent()) > highest) {
            victim = vch;
            highest = now;
        } else if ((now = number_percent()) > highest) {
        victim = vch;
        highest = now;
    }
    }

    return victim;
}


/*
 * How many other players / mobs are there in the room
 * iFlag: 0: all, 1: players, 2: mobiles 3: mobs w/ same vnum 4: same group
 */
int count_people_room(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, int iFlag)
{
    CHAR_DATA *vch;
    ROOM_INDEX_DATA *context_room = NULL;
    int count;

    if ((mob && obj) || (mob && room) || (obj && room)) {
    pbugf(LOG_SCRIPTS, "count_people_room received multiple prog types");
    return 0;
    }

    context_room = script_room_from_context(mob, obj, room, token);
    if (!context_room) {
    pbugf(LOG_SCRIPTS, "count_people_room had null room obj and mob.");
    return 0;
    }
    vch = context_room->people;

    for (count = 0; vch; vch = vch->next_in_room) {
    if (mob) {
        if (mob != vch
        && (iFlag == 0
        || (iFlag == 1 && !IS_NPC(vch))
        || (iFlag == 2 && IS_NPC(vch))
        || (iFlag == 3 && IS_NPC(mob) && IS_NPC(vch)
        && mob->pIndexData->vnum == vch->pIndexData->vnum)
        || (iFlag == 4 && is_same_group(mob, vch)))
        && can_see(mob, vch))
          ++count;

    } else if (obj || room) {
        if (iFlag == 0
        || (iFlag == 1 && !IS_NPC(vch))
        || (iFlag == 2 && IS_NPC(vch)))
        ++count;
    }
    }

    return (count);
}


/*
 * Get the order of a mob in the room. Useful when several mobs in
 * a room have the same trigger and you want only the first of them
 * to act
 */
//@@@NIB Combined the if statements; once it knew it was a mob,
//		it didn't need to check again.
int get_order(CHAR_DATA *ch, OBJ_DATA *obj)
{
    CHAR_DATA *vch;
    OBJ_DATA *vobj;
    ROOM_INDEX_DATA *context_room;
    int i;

    if (ch && obj) {
    pbugf(LOG_SCRIPTS, "get_order received multiple prog types");
    return 0;
    }

    if (ch) {
    if(!IS_NPC(ch)) return 0;
    if(!ch->in_room) return 0;
    vch = ch->in_room->people;

    for (i = 0; vch; vch = vch->next_in_room) {
        if (vch == ch) return i;

        if (IS_NPC(vch) && vch->pIndexData->vnum == ch->pIndexData->vnum)
        ++i;
    }

    } else {
    context_room = script_room_from_obj_or_token(obj, NULL);
    if (context_room)
        vobj = context_room->contents;
    else
        vobj = NULL;

    for (i = 0; vobj; vobj = vobj->next_content) {
        if (vobj == obj) return i;

        if (vobj->pIndexData->vnum == obj->pIndexData->vnum)
        ++i;
    }
    }

    return 0;
}

CHAR_DATA *script_get_char_blist(SCRIPT_VARINFO *info, LLIST *blist, CHAR_DATA *viewer, bool player, WNUM wnum, char *name)
{
    int nth = 1, i = 0;
    char buf[MSL];
    CHAR_DATA *ch;
    ITERATOR it;
    LLIST_UID_DATA *luid;

    if(!IS_VALID(blist)) return NULL;

    if( player && wnum.pArea && wnum.vnum > 0 ) return NULL;

    if(name) {
        nth = number_argument(name,buf);

        if(!player && parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
            name = NULL;
        } else {
            wnum = wnum_zero;
            name = buf;
        }
    }

    iterator_start(&it, blist);
    while((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
    {
        if( !luid->ptr ) continue;

        ch = (CHAR_DATA *)luid->ptr;

        if( player && IS_NPC(ch) ) continue;
        if( name && !is_name(name,ch->name) ) continue;
        if( (wnum.pArea && wnum.vnum > 0) && !wnum_match_mob(wnum, ch) ) continue;
        if( viewer && !can_see(viewer,ch) ) continue;

        if( ++i == nth )
            break;
    }
    iterator_stop(&it);

    if(luid && luid->ptr)
        return (CHAR_DATA *)luid->ptr;
    return NULL;
}

CHAR_DATA *script_get_char_list(SCRIPT_VARINFO *info, CHAR_DATA *mobs, CHAR_DATA *viewer, bool player, WNUM wnum, char *name)
{
    int nth = 1, i = 0;
    char buf[MSL];
    CHAR_DATA *ch;
    if(!mobs) return NULL;

    if( player && wnum.pArea && wnum.vnum > 0 ) return NULL;

    if(name) {
        nth = number_argument(name,buf);

        if(!player && parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
            name = NULL;
        } else {
            wnum = wnum_zero;
            name = buf;
        }
    }

    if(player) {
        for(ch = mobs; ch; ch = ch->next_in_room)
            if(!IS_NPC(ch) && is_name(name,ch->name) && (!viewer || can_see(viewer,ch)))
                if( ++i == nth ) return ch;

    } else if(wnum.pArea && wnum.vnum > 0) {
        for(ch = mobs; ch; ch = ch->next_in_room)
            if(IS_NPC(ch) && wnum_match_mob(wnum, ch) && (!viewer || can_see(viewer,ch)))
                if( ++i == nth ) return ch;

    } else if(name) {
        for(ch = mobs; ch; ch = ch->next_in_room)
            if(is_name(name,ch->name) && (!viewer || can_see(viewer,ch)))
                if( ++i == nth ) return ch;

    }
    return NULL;
}


OBJ_DATA *script_get_obj_blist(SCRIPT_VARINFO *info, LLIST *blist, CHAR_DATA *viewer, WNUM wnum, char *name)
{
    int nth = 1, i = 0;
    char buf[MSL];
    OBJ_DATA *obj;
    ITERATOR it;
    LLIST_UID_DATA *luid;

    if(!IS_VALID(blist)) return NULL;

    if(name) {
        nth = number_argument(name,buf);

        if(parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
            name = NULL;
        } else {
            wnum = wnum_zero;
            name = buf;
        }
    }

    iterator_start(&it, blist);
    while((luid = (LLIST_UID_DATA *)iterator_nextdata(&it)))
    {
        if( !luid->ptr ) continue;

        obj = (OBJ_DATA *)luid->ptr;

        if( name && !is_name(name,obj->name) ) continue;
        if( (wnum.pArea && wnum.vnum > 0) && !wnum_match_obj(wnum, obj) ) continue;
        if( viewer && !can_see_obj(viewer,obj) ) continue;

        if( ++i == nth )
            break;
    }
    iterator_stop(&it);

    if(luid && luid->ptr)
        return (OBJ_DATA *)luid->ptr;
    return NULL;
}


OBJ_DATA *script_get_obj_list(SCRIPT_VARINFO *info, void *objs, CHAR_DATA *viewer, int worn, WNUM wnum, char *name)
{
    int nth = 1, i = 0;
    char buf[MSL];
    OBJ_DATA *obj;
    ITERATOR it;
    
    if(!objs) return NULL;

    if(name) {
        nth = number_argument(name,buf);

        if(parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum)) {
            name = NULL;
        } else {
            wnum = wnum_zero;
            name = buf;
        }
    }

    // Check if we're dealing with an LLIST
    if(is_llist(objs)) {
        LLIST *llist = (LLIST *)objs;
        
        iterator_start(&it, llist);
        switch(worn) {
        default:
            if(wnum.pArea && wnum.vnum > 0) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            } else if(name) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            }
            break;
        case 1:
            if(wnum.pArea && wnum.vnum > 0) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(obj->wear_loc != WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            } else if(name) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(obj->wear_loc != WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            }
            break;
        case 2:
            if(wnum.pArea && wnum.vnum > 0) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(obj->wear_loc == WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            } else if(name) {
                while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
                    if(obj->wear_loc == WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                        if( ++i == nth ) {
                            iterator_stop(&it);
                            return obj;
                        }
            }
            break;
        }
        iterator_stop(&it);
        return NULL;
    }
    
    // Traditional linked list case - existing code
    OBJ_DATA *linked_objs = (OBJ_DATA *)objs;
    switch(worn) {
    default:
        if(wnum.pArea && wnum.vnum > 0) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        } else if(name) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        }
        break;
    case 1:
        if(wnum.pArea && wnum.vnum > 0) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(obj->wear_loc != WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        } else if(name) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(obj->wear_loc != WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        }
        break;
    case 2:
        if(wnum.pArea && wnum.vnum > 0) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(obj->wear_loc == WEAR_NONE && wnum_match_obj(wnum, obj) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        } else if(name) {
            for(obj = linked_objs; obj; obj = obj->next_content)
                if(obj->wear_loc == WEAR_NONE && is_name(name,obj->name) && (!viewer || can_see_obj(viewer,obj)))
                    if( ++i == nth ) return obj;
        }
        break;
    }
    return NULL;
}


TOKEN_DATA *token_find_match(SCRIPT_VARINFO *info, TOKEN_DATA *tokens,char *argument, SCRIPT_PARAM *arg)
{
    char *rest;
    int i, nth = 1, matches;
    WNUM wnum = wnum_zero;
    int values[MAX_TOKEN_VALUES];
    bool match[MAX_TOKEN_VALUES];
    char buf[MSL];

    if(!(rest = expand_argument(info,argument,arg)))
        return NULL;

    if(arg->type == ENT_WIDEVNUM)
        wnum = arg->d.wnum;
    else if(arg->type == ENT_NUMBER) {
        char vnum_str[32];
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        if(!parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum))
            return NULL;
    }
    else if(arg->type == ENT_STRING) {
        nth = number_argument(arg->d.str,buf);
        if(nth < 1 || !parse_widevnum(buf, get_area_from_scriptinfo(info), &wnum))
            return NULL;
    }

    if(!wnum.pArea || wnum.vnum < 1) return NULL;

    for(i=0;*rest && i < MAX_TOKEN_VALUES; i++) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg)))
            return NULL;

        if(arg->type == ENT_NUMBER)
            values[i] = arg->d.num;
        else if(arg->type == ENT_STRING && is_number(arg->d.str))
            values[i] = atoi(arg->d.str);
        else {
            match[i] = false;
            continue;
        }
        match[i] = true;
    }

    for(;i < MAX_TOKEN_VALUES; i++) match[i] = false;

    for(;tokens;tokens = tokens->next) {
        if(wnum_match_token(wnum, tokens)) {
            for(matches = 0, i = 0; i < MAX_TOKEN_VALUES; i++)
                if( !match[i] || tokens->value[i] == values[i] )
                    matches++;

            if( matches == MAX_TOKEN_VALUES && !--nth )
                break;
        }
    }

    return tokens;
}

/*
 * Check if ch has a given item or item type
 * vnum: item vnum or -1
 * item_type: item type or -1
 * fWear: true: item must be worn, false: don't care
 */
bool has_item(CHAR_DATA *ch, long vnum, int16_t item_type, bool fWear, AREA_DATA *area)
{
    OBJ_DATA *obj;
    ITERATOR it;
    
    // Check worn items first if looking specifically for worn items
    if (fWear && ch->lworn) {
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if ((vnum < 0 || (obj->pIndexData->vnum == vnum && (!area || obj->pIndexData->area == area))) &&
                (item_type < 0 || obj->pIndexData->item_type == item_type)) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
        return false;  // If we're only looking for worn items and didn't find any
    }
    
    // Check all items (or just inventory items if not checking worn)
    if (ch->lcarrying) {
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
            if ((vnum < 0 || (obj->pIndexData->vnum == vnum && (!area || obj->pIndexData->area == area))) &&
                (item_type < 0 || obj->pIndexData->item_type == item_type) &&
                (!fWear || obj->wear_loc != WEAR_NONE)) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }
    
    return false;
}


/*
 * Check if there's a mob with given vnum in the room
 */
CHAR_DATA *get_mob_vnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, long vnum, AREA_DATA *area)
{
    CHAR_DATA *mob;
    ROOM_INDEX_DATA *context_room = NULL;

    if ((ch && obj) || (ch && room) || (obj && room) ||
        (ch && token) || (obj && token) || (room && token)) {
    pbugf(LOG_SCRIPTS, "get_mob_vnum_room received multiple prog types");
    return NULL;
    }

    context_room = script_room_from_context(ch, obj, room, token);
    if (!context_room)
    return NULL;
    mob = context_room->people;

    for (; mob; mob = mob->next_in_room)
    if (IS_NPC(mob) && mob->pIndexData->vnum == vnum && (!area || mob->pIndexData->area == area))
        return mob;
    return NULL;
}


/*
 * Check if there's an object with given vnum in the room
 */
OBJ_DATA *get_obj_vnum_room(CHAR_DATA *ch, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token, long vnum, AREA_DATA *area)
{
    OBJ_DATA *vobj;
    ROOM_INDEX_DATA *context_room = NULL;

    if ((ch && obj) || (ch && room) || (obj && room) ||
        (ch && token) || (obj && token) || (room && token)) {
    pbugf(LOG_SCRIPTS, "get_obj_vnum_room received multiple prog types");
    return NULL;
    }

    context_room = script_room_from_context(ch, obj, room, token);
    if (!context_room)
    return NULL;
    vobj = context_room->contents;

    for (; vobj; vobj = vobj->next_content)
    if (vobj->pIndexData->vnum == vnum && (!area || vobj->pIndexData->area == area))
        return vobj;
    return NULL;
}

void get_level_damage(int level, int *num, int *type, bool fRemort, bool fTwo)
{
    *num = (level + 20) / 10;
    *type = (level + 20) / 4;

    if(fTwo) *type = (*type * 7)/5 - 1;
    if(fRemort) {
        *num += 2;
        *type += 2;
    }

    *num = UMAX(1, *num);
    *type = UMAX(8, *type);
}

void do_mob_transfer(CHAR_DATA *ch,ROOM_INDEX_DATA *room,bool quiet, int mode)
{
    if( ch->desc && quiet )
    {
        ch->desc->muted++;
    }

    bool show = !quiet;
    char *phrase = quiet?"silent":NULL;

    ROOM_INDEX_DATA *in_room = ch->in_room;

    DUNGEON *in_dungeon = get_room_dungeon(ch->in_room);
    DUNGEON *to_dungeon = get_room_dungeon(room);

    INSTANCE *in_instance = get_room_instance(ch->in_room);
    INSTANCE *to_instance = get_room_instance(room);

    if (ch->fighting)
        stop_fighting(ch, true);

    if( mode == TRANSFER_MODE_MOVEMENT )
    {
        check_room_shield_source(ch, show);

        if (!IS_DEAD(ch)) {
            check_room_flames(ch, show);
            if ((!IS_NPC(ch) && IS_DEAD(ch)) || (IS_NPC(ch) && ch->hit < 1))
                return;
        }

        /* moving your char negates your ambush */
        if (ch->ambush) {
            if( show )
            {
                send_to_char("You stop your ambush.\n\r", ch);
            }
            free_ambush(ch->ambush);
            ch->ambush = NULL;
        }

        /* Cancels your reciting too. This is incase move_char is called
           from some other function and doesnt go through interpret(). */
        if (ch->recite > 0) {
            if( show )
            {
                send_to_char("You stop reciting.\n\r", ch);
                act("$n stops reciting.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
            ch->recite = 0;
        }
    }

    char_from_room(ch);
    if(room->wilds)
        char_to_vroom(ch, room->wilds, room->x, room->y);
    else
        char_to_room(ch, room);

    if( mode == TRANSFER_MODE_PORTAL )
    {
        if (show) {
            if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
            {
                OBJ_DATA *portal = get_room_dungeon_portal(room, in_dungeon->index->vnum);

                if( IS_VALID(portal) )
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
                    {
                        act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    }
                    else
                    {
                        act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                    }
                }
                else if(MOUNTED(ch))
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
                        act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    else

                        act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
                else
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out) )
                        act(in_dungeon->index->zone_out, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    else
                        act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            }
            else if(!MOUNTED(ch)) {
                if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
                    act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                else
                    act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            } else {
                if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
                    act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                else
                    act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
        }
    }
    else if( mode == TRANSFER_MODE_MOVEMENT )
    {
        if (show && !IS_AFFECTED(ch, AFF_SNEAK) && ch->invis_level < LEVEL_HERO) {
            if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
            {
                OBJ_DATA *portal = get_room_dungeon_portal(room, in_dungeon->index->vnum);

                if( IS_VALID(portal) )
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
                    {
                        act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    }
                    else
                    {
                        act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                    }
                }
                else if(MOUNTED(ch))
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
                        act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    else

                        act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
                else
                {
                    if( !IS_NULLSTR(in_dungeon->index->zone_out) )
                        act(in_dungeon->index->zone_out, ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    else
                        act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            }
            else if (room_in_sector(in_room, SECT_WATER_NOSWIM))
                act("{W$n swims in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            else if (PULLING_CART(ch))
                act("{W$n has arrived, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            else if(!MOUNTED(ch)) {
                if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
                    act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                else
                    act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            } else {
                if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
                    act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                else
                    act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
        }
    }

    move_cart(ch,room,show);

    if(show) do_look(ch, "auto");

    if( mode == TRANSFER_MODE_PORTAL )
    {
        if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
        {
            p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase);
        }

        if( to_instance != in_instance )
        {
            p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase);
        }

        p_percent_trigger( ch, NULL, NULL, NULL,NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY , phrase);

        if ( !IS_NPC( ch ) ) {
            p_greet_trigger( ch, PRG_MPROG );
            p_greet_trigger( ch, PRG_OPROG );
            p_greet_trigger( ch, PRG_RPROG );
        }

    }
    else if( mode == TRANSFER_MODE_MOVEMENT )
    {
        if (!IS_WILDERNESS(room))
            check_traps(ch, show);

        if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
        {
            p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase);
        }

        if( IS_VALID(to_instance) && to_instance != in_instance )
        {
            p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase);
        }

        p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY, phrase);

        if (!IS_NPC(ch)) {
            p_greet_trigger(ch, PRG_MPROG);
            p_greet_trigger(ch, PRG_OPROG);
            p_greet_trigger(ch, PRG_RPROG);
        }

        if (!IS_DEAD(ch)) check_rocks(ch, show);
        if (!IS_DEAD(ch)) check_ice(ch, show);
        if (!IS_DEAD(ch)) check_room_flames(ch, show);
        //if (!IS_DEAD(ch)) check_ambush(ch);

        int16_t sn_riding = skill_resolve_gsn("riding");
        if (MOUNTED(ch) && number_percent() == 1 && get_skill(ch, sn_riding) > 0)
            check_improve_show(ch, sn_riding, true, 8, show);

        int16_t sn_trackless = skill_resolve_gsn("trackless step");
        if (!MOUNTED(ch) && get_skill(ch, sn_trackless) > 0 && number_percent() == 1)
            check_improve_show(ch, sn_trackless, true, 8, show);

        /* Nature regen: regenerate in nature */
        if (ch_has_trait(ch, "nature_regen") && is_in_nature(ch)) {
            ch->move += number_range(1,3);
            ch->move = UMIN(ch->move, ch->max_move);
            ch->hit += number_range(1,3);
            ch->hit  = UMIN(ch->hit, ch->max_hit);
        }

        if (!IS_NPC(ch))
            check_quest_rescue_mob(ch, show);
    }

    if( ch->desc && quiet && ch->desc->muted > 0)
    {
        ch->desc->muted--;
    }
}


bool has_trigger(LLIST **bank, int trigger)
{
    int slot;
    PROG_LIST *trig;
    ITERATOR it;

//	DBG2ENTRY2(PTR,bank,NUM,trigger);

//	DBG3MSG3("trigger = %d, name = '%s', slot = %d\n",trigger, trigger_table[trigger].name,trigger_slots[trigger]);

    int trigger_tindex = -1;
    for (int i = 0; i < trigger_table_size; i++) {
        if (trigger_table[i].type == trigger) {
            trigger_tindex = i;
            break;
        }
    }

    if (trigger_tindex < 0)
        return false;

    slot = trigger_table[trigger_tindex].slot;

    if(bank) {
        iterator_start(&it, bank[slot]);
        while((trig = (PROG_LIST *)iterator_nextdata(&it))) {
            if (is_trigger_type(trig->trig_type,trigger))
                break;
        }
        iterator_stop(&it);

        if(trig)
            return true;
    }

//	DBG2EXITVALUE2(false);
    return false;
}

int trigger_index(char *name, int type)
{
    register int i;

    // Check Cannonical names first, so aliases don't override
    for (i = 0; trigger_table[i].name; i++) {
        if (!str_cmp(trigger_table[i].name, name))
            switch (type) {
            case PRG_MPROG: if (trigger_table[i].mob) return i;
            case PRG_OPROG: if (trigger_table[i].obj) return i;
            case PRG_RPROG: if (trigger_table[i].room) return i;
            case PRG_TPROG: if (trigger_table[i].token) return i;
            case PRG_APROG: if (trigger_table[i].area) return i;
            case PRG_IPROG: if (trigger_table[i].instance) return i;
            case PRG_DPROG: if (trigger_table[i].dungeon) return i;
            case PRG_QPROG: if (trigger_table[i].quest) return i;
            case PRG_EPROG: if (trigger_table[i].event) return i;
            }
    }

    // Check Aliases
    for (i = 0; trigger_table[i].name; i++) {
        if (trigger_table[i].alias && is_exact_name(trigger_table[i].alias, name))
            switch (type) {
            case PRG_MPROG: if (trigger_table[i].mob) return i;
            case PRG_OPROG: if (trigger_table[i].obj) return i;
            case PRG_RPROG: if (trigger_table[i].room) return i;
            case PRG_TPROG: if (trigger_table[i].token) return i;
            case PRG_APROG: if (trigger_table[i].area) return i;
            case PRG_IPROG: if (trigger_table[i].instance) return i;
            case PRG_DPROG: if (trigger_table[i].dungeon) return i;
            case PRG_QPROG: if (trigger_table[i].quest) return i;
            case PRG_EPROG: if (trigger_table[i].event) return i;
            }
    }

    return -1;
}

bool is_trigger_type(int tindex, int type)
{
    if(tindex < 0) return false;

//	log_stringf("is_trigger_type: %d, %s, %d", tindex, trigger_table[tindex].name, type);

    return (trigger_table[tindex].type == type);
}

bool script_validate_trigger_table(void)
{
    bool ok = true;
    int canonical_count = 0;

    for (int i = 0; i < trigger_table_size && trigger_table[i].name; i++) {
        canonical_count++;

        if (trigger_table[i].type != i) {
            pbugf(LOG_ERROR,
                  "trigger_table mismatch: index %d ('%s') has type %d (expected %d)",
                  i,
                  trigger_table[i].name,
                  trigger_table[i].type,
                  i);
            ok = false;
        }

        if (trigger_table[i].slot < 0 || trigger_table[i].slot >= TRIGSLOT_MAX) {
            pbugf(LOG_ERROR,
                  "trigger_table mismatch: index %d ('%s') has invalid slot %d",
                  i,
                  trigger_table[i].name,
                  trigger_table[i].slot);
            ok = false;
        }
    }

    if (canonical_count != (TRIG_ZAP + 1)) {
        pbugf(LOG_ERROR,
              "trigger_table mismatch: canonical entry count is %d (expected %d)",
              canonical_count,
              TRIG_ZAP + 1);
        ok = false;
    }

    if (!ok) {
#ifndef NDEBUG
        assert(ok);
#endif
        return false;
    }

    return true;
}

bool mp_same_group(CHAR_DATA *ch,CHAR_DATA *vch,CHAR_DATA *to)
{
    return (ch != vch && ch != to && is_same_group(vch,to));
}

bool rop_same_group(CHAR_DATA *ch,CHAR_DATA *vch,CHAR_DATA *to)
{
    // vch is NULL from these
    return (is_same_group(ch,to));
}


ROOM_INDEX_DATA *get_exit_dest(ROOM_INDEX_DATA *room, char *argument)
{
    EXIT_DATA *ex;

    if (!room || !argument[0])
    return NULL;

    if (!str_cmp(argument, "north"))		ex = room->exit[DIR_NORTH];
    else if (!str_cmp(argument, "south"))	ex = room->exit[DIR_SOUTH];
    else if (!str_cmp(argument, "west"))	ex = room->exit[DIR_WEST];
    else if (!str_cmp(argument, "east"))	ex = room->exit[DIR_EAST];
    else if (!str_cmp(argument, "up"))		ex = room->exit[DIR_UP];
    else if (!str_cmp(argument, "down"))	ex = room->exit[DIR_DOWN];
    else if (!str_cmp(argument, "northeast"))	ex = room->exit[DIR_NORTHEAST];
    else if (!str_cmp(argument, "northwest"))	ex = room->exit[DIR_NORTHWEST];
    else if (!str_cmp(argument, "southeast"))	ex = room->exit[DIR_SOUTHEAST];
    else if (!str_cmp(argument, "southwest"))	ex = room->exit[DIR_SOUTHWEST];
    else return NULL;

    return (ex && ex->u1.to_room) ? ex->u1.to_room : NULL;
}


bool script_change_exit(ROOM_INDEX_DATA *pRoom, ROOM_INDEX_DATA *pToRoom, int door)
{
    EXIT_DATA *pExit;

    if (!pToRoom) {
        int16_t rev;

        if (!pRoom->exit[door]) {
            pbugf(LOG_SCRIPTS, "script_change_exit: Couldn't delete exit. %d", pRoom->vnum);
            return false;
        }

        if( IS_SET(pRoom->exit[door]->exit_info, (EX_NOUNLINK|EX_PREVFLOOR|EX_NEXTFLOOR)) )
        {
            pbugf(LOG_SCRIPTS, "script_change_exit: Exit is protected from deletion. %d", pRoom->vnum);
            return false;
        }

        // Remove ToRoom Exit.
        rev = rev_dir[door];
        pToRoom = pRoom->exit[door]->u1.to_room;

        if (pToRoom->exit[rev]) {
            free_exit(pToRoom->exit[rev]);
            pToRoom->exit[rev] = NULL;
        }

        // Remove this exit.
        free_exit(pRoom->exit[door]);
        pRoom->exit[door] = NULL;

        return true;
    }

    // Rules...
    // EITHER -> ENVIRON ... OK
    // STATIC -> CLONE ..... NOT OK
    // CLONE -> STATIC ..... WILL CLONE
    // CLONE -> CLONE ...... OK

    if(pToRoom != &room_pointer_environment) {
        if(room_is_clone(pRoom)) {
            if(!room_is_clone(pToRoom) && !(pToRoom = create_virtual_room(pToRoom,false,false)))
                return false;
        } else if(room_is_clone(pToRoom)) {
            pbugf(LOG_SCRIPTS, "script_change_exit: A link cannot be made from a static room to a clone room.\n\r",0);
            return false;
        }
    }

    if(room_is_clone(pRoom)) {
        if(pToRoom != &room_pointer_environment && !room_is_clone(pToRoom)) {
            // Should this be illegal or should it be made into a cloned room?
            pbugf(LOG_SCRIPTS, "script_change_exit: A link cannot be made between static and clone room.\n\r",0);
            return false;
        }
    } else {
        if(pToRoom != &room_pointer_environment && room_is_clone(pToRoom)) {
            pbugf(LOG_SCRIPTS, "script_change_exit: A link cannot be made between static and clone room.\n\r",0);
            return false;
        }
    }

    if(pToRoom != &room_pointer_environment) {
        if (pToRoom->exit[rev_dir[door]]) {
            pbugf(LOG_SCRIPTS, "script_change_exit: Reverse-side exit to room already exists.", 0);
            return false;
        }
    }

    if (!pRoom->exit[door]) pRoom->exit[door] = new_exit();

    pRoom->exit[door]->u1.to_room = pToRoom;
    pRoom->exit[door]->orig_door = door;
    pRoom->exit[door]->from_room = pRoom;

    if(pToRoom != &room_pointer_environment) {
        door = rev_dir[door];
        pExit = new_exit();
        pExit->u1.to_room = pRoom;
        pExit->orig_door = door;
        pExit->from_room = pToRoom;
        pToRoom->exit[door] = pExit;
    } else {
        // Mark it as an environment
        SET_BIT(pRoom->exit[door]->exit_info, EX_ENVIRONMENT);
    }

    return true;
}




char *trigger_name(int type)
{
    int tindex;

    for (tindex = 0; tindex < trigger_table_size; tindex++) {
        if (trigger_table[tindex].type == type && trigger_table[tindex].name)
            return trigger_table[tindex].name;
    }

    return "INVALID";
}

char *trigger_phrase(int type, char *phrase)
{
    int sn;
    int tindex;

    for (tindex = 0; tindex < trigger_table_size; tindex++) {
        if (trigger_table[tindex].type == type && trigger_table[tindex].name)
            break;
    }

    if (tindex < trigger_table_size) {
        if(type == TRIG_SPELLCAST) {
            sn = atoi(phrase);
            if(sn < 0) return "reserved";
            return skill_table[sn].name;
        }
    }

    return phrase;
}

char *trigger_phrase_olcshow(int type, char *phrase, bool is_rprog, bool is_tprog)
{
    int sn;
    int tindex;

    for (tindex = 0; tindex < trigger_table_size; tindex++) {
        if (trigger_table[tindex].type == type && trigger_table[tindex].name)
            break;
    }

    if (tindex < trigger_table_size) {
        if(type == TRIG_SPELLCAST) {
            sn = atoi(phrase);
            if(sn < 0) return "reserved";
            return skill_table[sn].name;
        }

        if(	type == TRIG_EXIT ||
            type == TRIG_EXALL ||
            type == TRIG_KNOCK ||
            type == TRIG_KNOCKING) {
            sn = atoi(phrase);

            if( sn < 0 || sn >= MAX_DIR) return "nowhere";

            return dir_name[sn];
        }

        // Only care if is_rprog/is_tprog is set
        if((is_rprog || is_tprog) && (type == TRIG_OPEN || type == TRIG_CLOSE)) {
            sn = atoi(phrase);

            if( is_rprog ) {
                if( sn < 0 || sn >= MAX_DIR) return "nowhere";

                return dir_name[sn];
            } else {
                if( sn < 0 || sn >= MAX_DIR) return phrase;

                return dir_name_phrase[sn];
            }



        }
    }

    return phrase;

}

// Common entry point for all the queued commands!
void script_interpret(SCRIPT_VARINFO *info, char *command)
{
    char buf[MSL];

    one_argument(command,buf);

    if(info->mob) {
        if(!str_cmp(buf,"mob")) mob_interpret(info,command);
        else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
        else {
            BUFFER *buffer = new_buf();
            expand_string(info,command,buffer);
            interpret(info->mob,buf_string(buffer));
            free_buf(buffer);
        }
        return;
    }

    if(info->obj) {
        if(!str_cmp(buf,"obj")) obj_interpret(info,command);
        else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
        return;
    }

    if(info->room) {
        if(!str_cmp(buf,"room")) room_interpret(info,command);
        else if(!str_cmp(buf,"token")) tokenother_interpret(info,command);
        return;
    }

    if(info->token) {
        if(!str_cmp(buf,"token")) token_interpret(info,command);
        return;
    }

    // Complain
}

typedef bool (*MATCH_STRING)(char *a, char *b);
typedef bool (*MATCH_NUMBER)(int a, int b);
typedef bool (*MATCH_RANGE)(int a, int b, int c);


// MATCH_STRING
static bool __attribute__ ((unused)) match_substr(register char *a, register char *b)
{
    register char *a2;
    register char *b2;

    if(!a || !b) return false;

    if(!*b) return true;

    if(!*a) return false;

    while(*a) {
        for(a2 = a, b2 = b;(*a2 && *b2 && LOWER(*a2) == LOWER(*b2)); ++a2, ++b2);

        if(!*b2) return true;

        a++;
    }

    return false;
}

// MATCH_STRING
static bool __attribute__ ((unused)) match_exact_name(char *a, char *b)
{
    return !str_cmp(a, b);
}

// MATCH_STRING
static bool __attribute__ ((unused)) match_name(char *a, char *b)
{
    return is_name(b, a);
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_random(int a, int b)
{
    return number_range(0, b-1) < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_percent(int a, int b)
{
    return number_percent() < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_equal(int a, int b)
{
    return a == b;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_lt(int a, int b)
{
    return b < a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_gt(int a, int b)
{
    return b > a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_not_equal(int a, int b)
{
    return a != b;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_lte(int a, int b)
{
    return b <= a;
}

// MATCH_NUMBER
static bool __attribute__ ((unused)) match_gte(int a, int b)
{
    return b >= a;
}

// STRING TRIGGER
int test_string_trigger(char *string, char *wildcard, MATCH_STRING match, int type,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2)
{
    ITERATOR tit;	// Token iterator
    ITERATOR pit;	// Prog iterator
    PROG_LIST *prg;
    TOKEN_DATA *token;
    unsigned long uid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret;

    if ((mob && obj) || (mob && room) || (obj && room)) {
        pbugf(LOG_SCRIPTS, "test_string_trigger: Multiple program types in trigger %d.", type);
        PRETURN;
    }

    {
        int trigger_tindex = -1;
        for (int i = 0; i < trigger_table_size; i++) {
            if (trigger_table[i].type == type) {
                trigger_tindex = i;
                break;
            }
        }
        if (trigger_tindex < 0) {
            pbugf(LOG_SCRIPTS, "test_string_trigger: unknown trigger type %d.", type);
            PRETURN;
        }
        slot = trigger_table[trigger_tindex].slot;
    }


    if (mob) {
        script_mobile_addref(mob);

        // Save the UID
        uid[0] = mob->id[0];
        uid[1] = mob->id[1];

        // Check for tokens FIRST
        iterator_start(&tit, mob->ltokens);
        // Loop Level 1
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                // Loop Level 2
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(string, prg->trig_phrase)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);
                                script_token_remref(token);
                                script_mobile_remref(mob);
                                return ret;
                            }
                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            // Loop Level 1:
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(string, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&pit);
                            script_mobile_remref(mob);
                            return ret;
                        }

                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);


            if(ret_val == PRET_NOSCRIPT && wildcard != NULL && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1])
            {
                // Check for tokens FIRST
                iterator_start(&tit, mob->ltokens);
                // Loop Level 1
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        // Loop Level 2
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_exact_name(wildcard, prg->trig_phrase)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&tit);
                                        iterator_stop(&pit);
                                        script_token_remref(token);
                                        script_mobile_remref(mob);
                                        return ret;
                                    }
                                }
                            }
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);
                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
                    iterator_start(&pit, mob->pIndexData->progs[slot]);
                    // Loop Level 1:
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_exact_name(wildcard, prg->trig_phrase)) {
                                ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);
                                    script_mobile_remref(mob);
                                    return ret;
                                }

                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);
                }
            }
        }
        script_mobile_remref(mob);

    } else if (obj && obj_room(obj) ) {
        // Save the UID
        uid[0] = obj->id[0];
        uid[1] = obj->id[1];
        script_object_addref(obj);

        // Check for tokens FIRST
        iterator_start(&tit, obj->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(string, prg->trig_phrase)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);

                                script_token_remref(token);
                                script_object_remref(obj);
                                return ret;
                            }

                        }
                    }
                }
                iterator_stop(&pit);

                script_token_remref(token);
                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(string, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&pit);
                            script_object_remref(obj);
                            return ret;
                        }

                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            if(ret_val == PRET_NOSCRIPT && wildcard != NULL && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1])
            {
                // Check for tokens FIRST
                iterator_start(&tit, obj->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_exact_name(wildcard, prg->trig_phrase)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&tit);
                                        iterator_stop(&pit);

                                        script_token_remref(token);
                                        script_object_remref(obj);
                                        return ret;
                                    }

                                }
                            }
                        }
                        iterator_stop(&pit);

                        script_token_remref(token);
                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
                    script_destructed = false;
                    iterator_start(&pit, obj->pIndexData->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_exact_name(wildcard, prg->trig_phrase)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);
                                    script_object_remref(obj);
                                    return ret;
                                }

                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);
                }
            }
        }

        script_object_remref(obj);

    } else if (room) {
        ROOM_INDEX_DATA *source;

        if(room->source) {
            source = room->source;
            uid[0] = room->id[0];
            uid[1] = room->id[1];
        } else {
            source = room;
        }

        script_room_addref(room);

        // Check for tokens FIRST
        iterator_start(&tit, room->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(string, prg->trig_phrase)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);

                                script_token_remref(token);
                                script_room_remref(room);
                                return ret;
                            }

                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);

                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
            script_destructed = false;
            iterator_start(&pit, source->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(string, prg->trig_phrase)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&pit);
                            script_room_remref(room);
                            return ret;
                        }
                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);


            if(ret_val == PRET_NOSCRIPT && !script_destructed && wildcard != NULL)
            {
                // Check for tokens FIRST
                iterator_start(&tit, room->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_exact_name(wildcard, prg->trig_phrase)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&tit);
                                        iterator_stop(&pit);

                                        script_token_remref(token);
                                        script_room_remref(room);
                                        return ret;
                                    }

                                }
                            }
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);

                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
                    script_destructed = false;
                    iterator_start(&pit, source->progs->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_exact_name(wildcard, prg->trig_phrase)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,string,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);
                                    script_room_remref(room);
                                    return ret;
                                }
                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);

                }
            }

        }
        script_room_remref(room);

    } else
        pbugf(LOG_SCRIPTS, "test_string_trigger: no program type for trigger %d.", type);

    PRETURN;
}


/*
 * A general purpose string trigger. Matches argument to a string trigger
 * phrase.
 */
int p_act_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type)
{
    return test_string_trigger(argument, "*", match_substr, type, mob, obj, room, ch, victim, victim2, obj1, obj2);
}

// Similar to p_act_trigger, except it does EXACT match
int p_exact_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type)
{
    if( type == TRIG_SPELLCAST ) {
        char buf[MIL];

        sprintf(buf, "SPELLCAST: %s", argument);
        emit_script_staff_event(buf, buf, "spellcast_trigger", NULL);
    }

    return test_string_trigger(argument, "*", match_exact_name, type, mob, obj, room, ch, victim, victim2, obj1, obj2);
}

// Similar to p_act_trigger, except it uses is_name
int p_name_trigger(char *argument, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type)
{
    return test_string_trigger(argument, "*", match_name, type, mob, obj, room, ch, victim, victim2, obj1, obj2);
}




int test_number_trigger(int number, int wildcard, MATCH_NUMBER match, int type,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
            AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2, TOKEN_DATA *tok,
            char *phrase)
{
    //char buf[MIL];
    ITERATOR tit;	// Token iterator
    ITERATOR pit;	// Prog iterator
    PROG_LIST *prg;
    unsigned long uid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret;

    if ((mob && obj) || (mob && room) || (mob && token) || (mob && area) || (mob && instance) || (mob && dungeon) ||
        (obj && room) || (obj && token) || (obj && area) || (obj && instance) || (obj && dungeon) ||
        (room && token) || (room && area) || (room && instance) || (room && dungeon) ||
        (token && area) || (token && instance) || (token && dungeon) ||
        (area && instance) || (area && dungeon) ||
        (instance && dungeon)) {
        pbugf(LOG_SCRIPTS, "test_number_trigger: Multiple program types in trigger %d.", type);
        PRETURN;
    }

    {
        int trigger_tindex = -1;
        for (int i = 0; i < trigger_table_size; i++) {
            if (trigger_table[i].type == type) {
                trigger_tindex = i;
                break;
            }
        }
        if (trigger_tindex < 0) {
            pbugf(LOG_SCRIPTS, "test_number_trigger: unknown trigger type %d.", type);
            PRETURN;
        }
        slot = trigger_table[trigger_tindex].slot;
    }


    if (mob) {
        script_mobile_addref(mob);

        // Save the UID
        uid[0] = mob->id[0];
        uid[1] = mob->id[1];

        // Check for tokens FIRST
        iterator_start(&tit, mob->ltokens);
        // Loop Level 1
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {

            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                // Loop Level 2
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            // Loop Level 1:
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                        SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( number != wildcard )
            {
                // Check for tokens FIRST
                iterator_start(&tit, mob->ltokens);
                // Loop Level 1
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {

                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        // Loop Level 2
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_equal(prg->trig_number, wildcard)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                    SETPRET;
                                }
                            }
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);
                    }
                }
                iterator_stop(&tit);

                if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
                    iterator_start(&pit, mob->pIndexData->progs[slot]);
                    // Loop Level 1:
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                SETPRET;
                            }
                        }
                    }
                    iterator_stop(&pit);

                }
            }

        }
        script_mobile_remref(mob);

    } else if (obj && obj_room(obj) ) {
        // Save the UID
        uid[0] = obj->id[0];
        uid[1] = obj->id[1];
        script_object_addref(obj);

        // Check for tokens FIRST
        iterator_start(&tit, obj->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);

                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(!script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( number != wildcard )
            {
                // Check for tokens FIRST
                iterator_start(&tit, obj->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_equal(prg->trig_number, wildcard)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    SETPRET;
                                }
                            }
                        }
                        iterator_stop(&pit);

                        script_token_remref(token);
                    }
                }
                iterator_stop(&tit);

                if(!script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
                    script_destructed = false;
                    iterator_start(&pit, obj->pIndexData->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    SETPRET;
                            }
                        }
                    }
                    iterator_stop(&pit);
                }
            }

        }

        script_object_remref(obj);

    } else if (room) {
        ROOM_INDEX_DATA *source;

        if(room->source) {
            source = room->source;
            uid[0] = room->id[0];
            uid[1] = room->id[1];
        } else {
            source = room;
        }

        script_room_addref(room);

        // Check for tokens FIRST
        iterator_start(&tit, room->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(!script_destructed && source->progs && source->progs->progs) {
            script_destructed = false;
            iterator_start(&pit, source->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( number != wildcard )
            {
                // Check for tokens FIRST
                iterator_start(&tit, room->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,type)) {
                                if (match_equal(prg->trig_number, wildcard)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    SETPRET;
                                }
                            }
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);
                    }
                }
                iterator_stop(&tit);

                if(!script_destructed && source->progs && source->progs->progs) {
                    script_destructed = false;
                    iterator_start(&pit, source->progs->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    SETPRET;
                            }
                        }
                    }
                    iterator_stop(&pit);

                }
            }

        }
        script_room_remref(room);
    } else if(token) {
        if( token->pIndexData && token->pIndexData->progs ) {
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            // Loop Level 2
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
            {
                iterator_start(&pit, token->pIndexData->progs[slot]);
                // Loop Level 2
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (match_equal(prg->trig_number, wildcard)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
            }

            script_token_remref(token);
        }

    } else if(area) {
        if( area->progs->progs ) {
            iterator_start(&pit, area->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, area, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
            {
                iterator_start(&pit, area->progs->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (match_equal(prg->trig_number, wildcard)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, area, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
            }
        }

    } else if(instance) {
        if( instance->blueprint->progs ) {
            script_instance_addref(instance);
            script_destructed = false;
            iterator_start(&pit, instance->blueprint->progs[slot]);
            // Loop Level 2
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, NULL, instance, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
            {
                iterator_start(&pit, instance->blueprint->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (match_equal(prg->trig_number, wildcard)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, NULL, instance, NULL, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
            }

            script_instance_remref(instance);
        }

    } else if(dungeon) {
        if( dungeon->index->progs ) {
            script_dungeon_addref(dungeon);
            script_destructed = false;
            iterator_start(&pit, dungeon->index->progs[slot]);
            // Loop Level 2
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, NULL, NULL, dungeon, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            SETPRET;
                    }
                }
            }
            iterator_stop(&pit);

            if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard )
            {
                iterator_start(&pit, dungeon->index->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (match_equal(prg->trig_number, wildcard)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, NULL, NULL, NULL, dungeon, enactor, obj1, obj2, victim, victim2,NULL, tok, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                SETPRET;
                        }
                    }
                }
                iterator_stop(&pit);
            }

            script_dungeon_remref(dungeon);
        }

    } else
        pbugf(LOG_SCRIPTS, "test_number_trigger: no program type for trigger %d.", type);

    PRETURN;
}

int p_percent_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase)
{
    return test_number_trigger(0, 0, match_percent, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj2, NULL, phrase);
}

int p_lifecycle_bank_trigger(LLIST **bank, AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
    QUEST_DATA *quest, CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2,
    OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase)
{
    ITERATOR it;
    PROG_LIST *prg;
    int trigger_tindex = -1;
    int slot;
    int ret_val = PRET_NOSCRIPT;
    const char *safe_phrase;

    if (!bank || type < 0)
        return PRET_NOSCRIPT;

    safe_phrase = IS_NULLSTR(phrase) ? "" : phrase;

    for (int i = 0; i < trigger_table_size; i++)
    {
        if (trigger_table[i].type == type)
        {
            trigger_tindex = i;
            break;
        }
    }

    if (trigger_tindex < 0)
        return PRET_NOSCRIPT;

    slot = trigger_table[trigger_tindex].slot;
    if (slot < 0 || slot >= TRIGSLOT_MAX || !bank[slot])
        return PRET_NOSCRIPT;

    iterator_start(&it, bank[slot]);
    while ((prg = (PROG_LIST *)iterator_nextdata(&it)) != NULL)
    {
        bool phrase_match;
        int ret;
        QUEST_DATA *saved_quest_context;

        if (!prg->script || !is_trigger_type(prg->trig_type, type))
            continue;

        if (prg->numeric)
        {
            if (type == TRIG_RANDOM)
                phrase_match = (number_percent() <= URANGE(0, prg->trig_number, 100));
            else if (!IS_NULLSTR(safe_phrase) && is_number((char *)safe_phrase))
                phrase_match = (prg->trig_number == atoi(safe_phrase));
            else if (prg->trig_number > 0 && prg->trig_number <= 100)
                phrase_match = (number_percent() <= prg->trig_number);
            else
                phrase_match = false;
        }
        else if (IS_NULLSTR(prg->trig_phrase) || !str_cmp(prg->trig_phrase, "*"))
            phrase_match = true;
        else
            phrase_match = !str_cmp(prg->trig_phrase, safe_phrase);

        if (!phrase_match)
            continue;

        saved_quest_context = script_set_execute_quest_context(quest);

        ret = execute_script(prg->vnum, prg->script,
            NULL, NULL, NULL, NULL,
            area, instance, dungeon,
            ch, obj1, obj2, victim, victim2, NULL,
            NULL, (char *)safe_phrase, prg->trig_phrase, type,
            0, 0, 0, 0, 0);

        script_set_execute_quest_context(saved_quest_context);

        if (ret != PRET_NOSCRIPT)
            ret_val = ret;
    }
    iterator_stop(&it);

    return ret_val;
}

int p_percent2_trigger(AREA_DATA *area, INSTANCE *instance, DUNGEON *dungeon,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase)
{
    return test_number_trigger(0, 0, match_percent, type, NULL, NULL, NULL, NULL, area, instance, dungeon, ch, victim, victim2, obj1, obj2, NULL, phrase);
}


int p_percent_token_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, TOKEN_DATA *tok, int type, char *phrase)
{
    return test_number_trigger(0, 0, match_percent, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj2, tok, phrase);
}



int p_number_trigger(int number, int wildcard, CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room, TOKEN_DATA *token,
    CHAR_DATA *ch, CHAR_DATA *victim, CHAR_DATA *victim2, OBJ_DATA *obj1, OBJ_DATA *obj2, int type, char *phrase)
{
    return test_number_trigger(number, wildcard, match_equal, type, mob, obj, room, token, NULL, NULL, NULL, ch, victim, victim2, obj1, obj2, NULL, phrase);
}

int p_bribe_trigger(CHAR_DATA *mob, CHAR_DATA *ch, int amount)
{
    return test_number_trigger(amount, -1, match_gte, TRIG_BRIBE, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL);
}


int test_number_sight_trigger(int number, int wildcard, MATCH_NUMBER match, int type, int typeall,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2,
            char *phrase)
{
    ITERATOR tit;	// Token iterator
    ITERATOR pit;	// Prog iterator
    PROG_LIST *prg;
    TOKEN_DATA *token;
    unsigned long uid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret;

    if ((mob && obj) || (mob && room) || (obj && room)) {
        pbugf(LOG_SCRIPTS, "test_number_sight_trigger: Multiple program types in trigger %d.", type);
        PRETURN;
    }

    {
        int type_tindex = -1;
        int typeall_tindex = -1;
        int i;

        for (i = 0; i < trigger_table_size; i++) {
            if (type_tindex < 0 && trigger_table[i].type == type)
                type_tindex = i;
            if (typeall_tindex < 0 && trigger_table[i].type == typeall)
                typeall_tindex = i;
            if (type_tindex >= 0 && typeall_tindex >= 0)
                break;
        }

        if (type_tindex < 0 || typeall_tindex < 0) {
            pbugf(LOG_SCRIPTS, "test_number_sight_trigger: unknown trigger type(s) %d/%d.", type, typeall);
            PRETURN;
        }

        // They must be in the same slot
        if( trigger_table[type_tindex].slot != trigger_table[typeall_tindex].slot )
        {
            pbugf(LOG_SCRIPTS, "test_number_sight_trigger: slot mismatch for sighted trigger %d.", type);
            PRETURN;
        }

        slot = trigger_table[typeall_tindex].slot;
    }

    if (mob) {
        script_mobile_addref(mob);

        // Save the UID
        uid[0] = mob->id[0];
        uid[1] = mob->id[1];

        // Check for tokens FIRST
        iterator_start(&tit, mob->ltokens);
        // Loop Level 1
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                // Loop Level 2
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (type != typeall && is_trigger_type(prg->trig_type,type) &&
                        (IS_SET(token->flags, TOKEN_SEE_ALL) ||
                            (mob->position == mob->pIndexData->default_pos && can_see(mob, enactor))) &&
                        (*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&tit);
                            iterator_stop(&pit);
                            script_token_remref(token);
                            script_mobile_remref(mob);
                            return ret;
                        }

                    } else if (is_trigger_type(prg->trig_type,typeall) &&
                        (*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&tit);
                            iterator_stop(&pit);
                            script_token_remref(token);
                            script_mobile_remref(mob);
                            return ret;
                        }

                    }
                    BREAKPRET;
                }
                iterator_stop(&pit);
                script_token_remref(token);
                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            // Loop Level 1:
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (type != typeall && is_trigger_type(prg->trig_type,type) &&
                    mob->position == mob->pIndexData->default_pos &&
                    can_see(mob, enactor) &&
                    (*match)(prg->trig_number, number)) {
                    ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);
                        script_mobile_remref(mob);
                        return ret;
                    }

                } else if (is_trigger_type(prg->trig_type,typeall) &&
                    (*match)(prg->trig_number, number)) {
                    ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                    if( ret != PRET_NOSCRIPT) {
                        iterator_stop(&pit);
                        script_mobile_remref(mob);
                        return ret;
                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            if( ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard&& IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) )
            {
                // Check for tokens FIRST
                iterator_start(&tit, mob->ltokens);
                // Loop Level 1
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        // Loop Level 2
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (type != typeall && is_trigger_type(prg->trig_type,type) &&
                                (IS_SET(token->flags, TOKEN_SEE_ALL) ||
                                    (mob->position == mob->pIndexData->default_pos && can_see(mob, enactor))) &&
                                match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&tit);
                                    iterator_stop(&pit);
                                    script_token_remref(token);
                                    script_mobile_remref(mob);
                                    return ret;
                                }

                            } else if (is_trigger_type(prg->trig_type,typeall) &&
                                match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&tit);
                                    iterator_stop(&pit);
                                    script_token_remref(token);
                                    script_mobile_remref(mob);
                                    return ret;
                                }

                            }
                            BREAKPRET;
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);
                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs)
                {
                    iterator_start(&pit, mob->pIndexData->progs[slot]);
                    // Loop Level 1:
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (type != typeall && is_trigger_type(prg->trig_type,type) &&
                            mob->position == mob->pIndexData->default_pos &&
                            can_see(mob, enactor) &&
                            match_equal(prg->trig_number, wildcard)) {
                            ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&pit);
                                script_mobile_remref(mob);
                                return ret;
                            }

                        } else if (is_trigger_type(prg->trig_type,typeall) &&
                            match_equal(prg->trig_number, wildcard)) {
                            ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&pit);
                                script_mobile_remref(mob);
                                return ret;
                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);
                }

            }
        }
        script_mobile_remref(mob);

    } else if (obj && obj_room(obj) ) {
        // Save the UID
        uid[0] = obj->id[0];
        uid[1] = obj->id[1];
        script_object_addref(obj);

        // Check for tokens FIRST
        iterator_start(&tit, obj->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,typeall)) {
                        if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);
                                script_token_remref(token);
                                script_object_remref(obj);
                                return ret;
                            }

                        }
                    }
                }
                iterator_stop(&pit);

                script_token_remref(token);
                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,typeall)) {
                    if ((*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                        if( ret != PRET_NOSCRIPT) {
                            iterator_stop(&pit);
                            script_object_remref(obj);
                            return ret;
                        }
                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs)
            {
                // Check for tokens FIRST
                iterator_start(&tit, obj->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,typeall)) {
                                if (match_equal(prg->trig_number, wildcard)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&tit);
                                        iterator_stop(&pit);
                                        script_token_remref(token);
                                        script_object_remref(obj);
                                        return ret;
                                    }

                                }
                            }
                        }
                        iterator_stop(&pit);

                        script_token_remref(token);
                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
                    script_destructed = false;
                    iterator_start(&pit, obj->pIndexData->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,typeall)) {
                            if (match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);
                                    script_object_remref(obj);
                                    return ret;
                                }
                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);
                }

            }

        }

        script_object_remref(obj);

    } else if (room) {
        ROOM_INDEX_DATA *source;

        if(room->source) {
            source = room->source;
            uid[0] = room->id[0];
            uid[1] = room->id[1];
        } else {
            source = room;
        }

        script_room_addref(room);

        // Check for tokens FIRST
        iterator_start(&tit, room->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,typeall)) {
                        if ((*match)(prg->trig_number, number)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);
                                script_token_remref(token);
                                script_room_remref(room);
                                return ret;
                            }

                        }
                    }
                    BREAKPRET;
                }
                iterator_stop(&pit);
                script_token_remref(token);

                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
            script_destructed = false;
            iterator_start(&pit, source->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,typeall)) {
                    if ((*match)(prg->trig_number, number)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&pit);

                                script_room_remref(room);
                                return ret;
                            }
                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard)
            {
                // Check for tokens FIRST
                iterator_start(&tit, room->ltokens);
                while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                    if( token->pIndexData && token->pIndexData->progs ) {
                        script_token_addref(token);
                        script_destructed = false;
                        iterator_start(&pit, token->pIndexData->progs[slot]);
                        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                            if (is_trigger_type(prg->trig_type,typeall)) {
                                if (match_equal(prg->trig_number, wildcard)) {
                                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&tit);
                                        iterator_stop(&pit);
                                        script_token_remref(token);
                                        script_room_remref(room);
                                        return ret;
                                    }

                                }
                            }
                            BREAKPRET;
                        }
                        iterator_stop(&pit);
                        script_token_remref(token);

                        BREAKPRET;
                    }
                }
                iterator_stop(&tit);

                if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
                    script_destructed = false;
                    iterator_start(&pit, source->progs->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,typeall)) {
                            if (match_equal(prg->trig_number, wildcard)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                    if( ret != PRET_NOSCRIPT) {
                                        iterator_stop(&pit);

                                        script_room_remref(room);
                                        return ret;
                                    }
                            }
                        }
                        BREAKPRET;
                    }
                    iterator_stop(&pit);

                    if(ret_val == PRET_NOSCRIPT && !script_destructed && number != wildcard)
                    {
                    }
                }
            }
        }
        script_room_remref(room);

    } else
        pbugf(LOG_SCRIPTS, "test_number_sight_trigger: no program type for trigger %d.", type);

    PRETURN;
}


int p_location_trigger(CHAR_DATA *ch, ROOM_INDEX_DATA *room, int number, int wildcard, MATCH_NUMBER match, int type, int trig, int trigall)
{
    ITERATOR oit, mit;

    CHAR_DATA *mob;
    OBJ_DATA *obj;
    //TOKEN_DATA *token;
    //PROG_LIST *prg;
    //unsigned long uid[2];
    int ret_val = PRET_NOSCRIPT; // Default for a trigger loop is NO SCRIPT

    // If not in a valid room, there's nothing to check!
    if (!ch || !room)
        PRETURN;

    if (type == PRG_MPROG) {
        iterator_start(&mit, room->lpeople);
        while((ret_val == PRET_NOSCRIPT) && (mob = (CHAR_DATA *)iterator_nextdata(&mit))) {
            ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
                        mob, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL);
        }

        iterator_stop(&mit);

    } else if (type == PRG_OPROG) {
        // Check carrying inventory
        iterator_start(&oit, ch->lcarrying);
        while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
            ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
                        NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL);
        }

        iterator_stop(&oit);
        ISSETPRET;

        // Check room contents
        iterator_start(&oit, room->lcontents);
        while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
            ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
                        NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL);
        }

        iterator_stop(&oit);
        ISSETPRET;

        iterator_start(&mit, room->lpeople);
        while((ret_val == PRET_NOSCRIPT) && (mob = (CHAR_DATA *)iterator_nextdata(&mit))) {
            // Check every other mobile
            if( mob != ch ) {
                iterator_start(&oit, mob->lcarrying);
                while((ret_val == PRET_NOSCRIPT) && (obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
                    ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
                                NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, NULL);
                }
                iterator_stop(&oit);
            }
        }
        iterator_stop(&mit);

    } else if (type == PRG_RPROG) {
        ret_val = test_number_sight_trigger(number, wildcard, match, trig, trigall,
                    NULL, NULL, room, ch, NULL, NULL, NULL, NULL, NULL);
    }

    PRETURN;
}

int p_exit_trigger(CHAR_DATA *ch, int dir, int type)
{
    return p_location_trigger(ch, ch ? ch->in_room : NULL, dir, -1, match_equal, type, TRIG_EXIT, TRIG_EXALL);
}

int p_direction_trigger(CHAR_DATA *ch, ROOM_INDEX_DATA *here, int dir, int type, int trigger)
{
    return p_location_trigger(ch, here, dir, -1, match_equal, type, trigger, trigger);
}


static bool __attribute__ ((unused)) match_target_name(register char *a, register char *b)
{
    char buf[MIL];

    while(*a) {
        a = one_argument(a, buf);
        if( is_name(buf, b) || !str_cmp("all", buf) )
            return true;
    }

    return false;
}


/**
 * trigger_match_vnum - Check if a trigger's numeric phrase matches an entity
 *
 * For widevnum triggers (auid#vnum format): matches both area and vnum.
 * For legacy bare vnum triggers: matches vnum only (any area, backward compat).
 */
static inline bool trigger_match_vnum(PROG_LIST *prg, AREA_DATA *entity_area, int vnum)
{
    if (prg->trig_is_widevnum) {
        return (prg->trig_wnum.pArea == entity_area && prg->trig_wnum.vnum == (long)vnum);
    }
    return match_equal(prg->trig_number, vnum);
}

bool script_vnumname_match_primary(PROG_LIST *prg, AREA_DATA *entity_area, int vnum, const char *name)
{
    if (!prg)
        return false;

    if (prg->numeric)
        return trigger_match_vnum(prg, entity_area, vnum);

    if (!prg->trig_phrase || !name)
        return false;

    return match_target_name(prg->trig_phrase, (char *)name);
}

bool script_vnumname_match_wildcard(PROG_LIST *prg)
{
    if (!prg)
        return false;

    if (prg->numeric)
        return (!prg->trig_is_widevnum && match_equal(prg->trig_number, 0));

    if (!prg->trig_phrase)
        return false;

    return match_exact_name(prg->trig_phrase, "*");
}

int test_vnumname_trigger(char *name, int vnum, AREA_DATA *entity_area, int type,
            CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *enactor, CHAR_DATA *victim, CHAR_DATA *victim2,
            OBJ_DATA *obj1, OBJ_DATA *obj2,
            char *phrase)
{
    ITERATOR tit;	// Token iterator
    ITERATOR pit;	// Prog iterator
    PROG_LIST *prg;
    TOKEN_DATA *token;
    unsigned long uid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret;

    if ((mob && obj) || (mob && room) || (obj && room)) {
        pbugf(LOG_SCRIPTS, "test_vnumname_trigger: Multiple program types in trigger %d.", type);
        PRETURN;
    }

    {
        int trigger_tindex = -1;
        for (int i = 0; i < trigger_table_size; i++) {
            if (trigger_table[i].type == type) {
                trigger_tindex = i;
                break;
            }
        }
        if (trigger_tindex < 0) {
            pbugf(LOG_SCRIPTS, "test_vnumname_trigger: unknown trigger type %d.", type);
            PRETURN;
        }
        slot = trigger_table[trigger_tindex].slot;
    }


    if (mob) {
        script_mobile_addref(mob);

        // Save the UID
        uid[0] = mob->id[0];
        uid[1] = mob->id[1];

        // Check for tokens FIRST
        iterator_start(&tit, mob->ltokens);
        // Loop Level 1
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                // Loop Level 2
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT ) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);

                                script_token_remref(token);
                                script_mobile_remref(mob);
                                return ret;
                            }
                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);
            }
        }
        iterator_stop(&tit);

        if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
            iterator_start(&pit, mob->pIndexData->progs[slot]);
            // Loop Level 1:
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                        ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT ) {
                                iterator_stop(&pit);

                                script_mobile_remref(mob);
                                return ret;
                            }

                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            // RECHECK FOR WILDCARDS

            // Check for tokens FIRST
            iterator_start(&tit, mob->ltokens);
            // Loop Level 1
            while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                if( token->pIndexData && token->pIndexData->progs ) {
                    script_token_addref(token);
                    script_destructed = false;
                    iterator_start(&pit, token->pIndexData->progs[slot]);
                    // Loop Level 2
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (script_vnumname_match_wildcard(prg)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL, phrase, prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT ) {
                                    iterator_stop(&tit);
                                    iterator_stop(&pit);

                                    script_token_remref(token);
                                    script_mobile_remref(mob);
                                    return ret;
                                }
                            }
                        }
                    }
                    iterator_stop(&pit);
                    script_token_remref(token);
                }
            }
            iterator_stop(&tit);

            if(!script_destructed && IS_VALID(mob) && mob->id[0] == uid[0] && mob->id[1] == uid[1] && IS_NPC(mob) && mob->pIndexData->progs) {
                iterator_start(&pit, mob->pIndexData->progs[slot]);
                // Loop Level 1:
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_wildcard(prg)) {
                            ret = execute_script(prg->vnum, prg->script, mob, NULL, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);

                                    script_mobile_remref(mob);
                                    return ret;
                                }

                        }
                    }
                    BREAKPRET;
                }
                iterator_stop(&pit);
            }

        }
        script_mobile_remref(mob);

    } else if (obj && obj_room(obj) ) {
        // Save the UID
        uid[0] = obj->id[0];
        uid[1] = obj->id[1];
        script_object_addref(obj);

        // Check for tokens FIRST
        iterator_start(&tit, obj->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);

                                script_token_remref(token);
                                script_object_remref(obj);
                                return ret;
                            }

                        }
                    }
                }
                iterator_stop(&pit);

                script_token_remref(token);
                BREAKPRET;
            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&pit);

                                script_object_remref(obj);
                                return ret;
                            }

                    }
                }
                BREAKPRET;
            }
            iterator_stop(&pit);

            // RECHECK WILDCARDS

            // Check for tokens FIRST
            iterator_start(&tit, obj->ltokens);
            while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                if( token->pIndexData && token->pIndexData->progs ) {
                    script_token_addref(token);
                    script_destructed = false;
                    iterator_start(&pit, token->pIndexData->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (script_vnumname_match_wildcard(prg)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&tit);
                                    iterator_stop(&pit);

                                    script_token_remref(token);
                                    script_object_remref(obj);
                                    return ret;
                                }

                            }
                        }
                    }
                    iterator_stop(&pit);

                    script_token_remref(token);
                    BREAKPRET;
                }
            }
            iterator_stop(&tit);

            if(ret_val == PRET_NOSCRIPT && !script_destructed && IS_VALID(obj) && obj->id[0] == uid[0] && obj->id[1] == uid[1] && obj->pIndexData->progs) {
                script_destructed = false;
                iterator_start(&pit, obj->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_wildcard(prg)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);

                                    script_object_remref(obj);
                                    return ret;
                                }

                        }
                    }
                    BREAKPRET;
                }
                iterator_stop(&pit);
            }


        }

        script_object_remref(obj);

    } else if (room) {
        ROOM_INDEX_DATA *source;

        if(room->source) {
            source = room->source;
            uid[0] = room->id[0];
            uid[1] = room->id[1];
        } else {
            source = room;
        }

        script_room_addref(room);

        // Check for tokens FIRST
        iterator_start(&tit, room->ltokens);
        while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if( token->pIndexData && token->pIndexData->progs ) {
                script_token_addref(token);
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&tit);
                                iterator_stop(&pit);

                                script_token_remref(token);
                                script_room_remref(room);
                                return ret;
                            }
                        }
                    }
                }
                iterator_stop(&pit);
                script_token_remref(token);

            }
        }
        iterator_stop(&tit);

        if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
            script_destructed = false;
            iterator_start(&pit, source->progs->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,type)) {
                    if (script_vnumname_match_primary(prg, entity_area, vnum, name)) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                            if( ret != PRET_NOSCRIPT) {
                                iterator_stop(&pit);

                                script_room_remref(room);
                                return ret;
                            }

                    }
                }
            }
            iterator_stop(&pit);


            // Check for tokens FIRST
            iterator_start(&tit, room->ltokens);
            while((token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
                if( token->pIndexData && token->pIndexData->progs ) {
                    script_token_addref(token);
                    script_destructed = false;
                    iterator_start(&pit, token->pIndexData->progs[slot]);
                    while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                        if (is_trigger_type(prg->trig_type,type)) {
                            if (script_vnumname_match_wildcard(prg)) {
                                ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT ) {
                                    iterator_stop(&tit);
                                    iterator_stop(&pit);

                                    script_token_remref(token);
                                    script_room_remref(room);
                                    return ret;
                                }
                            }
                        }
                    }
                    iterator_stop(&pit);
                    script_token_remref(token);

                }
            }
            iterator_stop(&tit);

            if(ret_val == PRET_NOSCRIPT && source->progs && source->progs->progs) {
                script_destructed = false;
                iterator_start(&pit, source->progs->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,type)) {
                        if (script_vnumname_match_wildcard(prg)) {
                            ret = execute_script(prg->vnum, prg->script, NULL, NULL, room, NULL, NULL, NULL, NULL, enactor, obj1, obj2, victim, victim2,NULL, NULL,phrase,prg->trig_phrase,type,0,0,0,0,0);
                                if( ret != PRET_NOSCRIPT) {
                                    iterator_stop(&pit);

                                    script_room_remref(room);
                                    return ret;
                                }

                        }
                    }
                }
                iterator_stop(&pit);

            }


        }
        script_room_remref(room);

    } else
        pbugf(LOG_SCRIPTS, "test_vnumname_trigger: no program type for trigger %d.", type);

    PRETURN;
}



int p_give_trigger(CHAR_DATA *mob, OBJ_DATA *obj, ROOM_INDEX_DATA *room,
            CHAR_DATA *ch, OBJ_DATA *dropped, int type)
{
    return test_vnumname_trigger(dropped->name, dropped->pIndexData->vnum,
                    dropped->pIndexData->area, type,
                    mob, obj, room, ch, NULL, NULL, dropped, NULL, NULL);
}


int p_use_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type)
{
    if (obj == NULL) {
        pbugf(LOG_SCRIPTS, "p_use_trigger: received null obj!", 0);
        return PRET_NOSCRIPT;
    }

    if (!obj_room(obj)) return PRET_NOSCRIPT;


    if (type == TRIG_PUSH || type == TRIG_TURN || type == TRIG_PULL || type == TRIG_USE)
        return test_number_trigger(0, 0, match_percent, type, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL);

    return PRET_NOSCRIPT;
}

int p_use_on_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, char *argument)
{
    if (obj == NULL) {
        pbugf(LOG_SCRIPTS, "p_use_on_trigger: received null obj!", 0);
        return PRET_NOSCRIPT;
    }

    if (!obj_room(obj)) return PRET_NOSCRIPT;

    if (type == TRIG_PUSH_ON || type == TRIG_TURN_ON || type == TRIG_PULL_ON)
        return test_string_trigger(argument, "*", match_substr, type, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL);

    return PRET_NOSCRIPT;
}

int p_use_with_trigger(CHAR_DATA *ch, OBJ_DATA *obj, int type, OBJ_DATA *obj1, OBJ_DATA *obj2, CHAR_DATA *victim, CHAR_DATA *victim2)
{
    if (obj == NULL) {
        pbugf(LOG_SCRIPTS, "p_use_with_trigger: received null obj!", 0);
        return PRET_NOSCRIPT;
    }

    if (!obj_room(obj)) return PRET_NOSCRIPT;

    if (type == TRIG_USEWITH)
        return test_number_trigger(0, 0, match_percent, type, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, victim, victim2, obj1, obj2, NULL, NULL);

    return PRET_NOSCRIPT;
}


int p_greet_trigger(CHAR_DATA *ch, int type)
{
    return p_location_trigger(ch, ch ? ch->in_room : NULL, 0, 0, match_percent, type, TRIG_GREET, TRIG_GRALL);
}


int p_hprct_trigger(CHAR_DATA *mob, CHAR_DATA *ch) // @@@NIB
{
    int hit = (100 * mob->hit / mob->max_hit);

    return test_number_trigger(hit, hit, match_lt, TRIG_HPCNT, mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL);
}

int p_emote_trigger(CHAR_DATA *ch, char *emote)
{
    int ret_val = PRET_NOSCRIPT, ret;
    // Check tokens on player

    ret_val = test_string_trigger(emote, "*", match_exact_name, TRIG_EMOTE, ch, NULL, NULL, ch, NULL, NULL, NULL, NULL);


    if( ret_val == PRET_NOSCRIPT && ch->in_room != NULL) {
        CHAR_DATA *mob, *mob_next;

        // Check all mobs in the room
        for (mob = ch->in_room->people; mob != NULL; mob = mob_next) {
            mob_next = mob->next_in_room;

            if (!IS_NPC(mob) || mob->position == mob->pIndexData->default_pos)
            {
                ret = test_string_trigger(emote, "*", match_exact_name, TRIG_EMOTE, mob, NULL, NULL, ch, NULL, NULL, NULL, NULL);

                if( ret != PRET_NOSCRIPT ) {
                    ret_val = ret;
                    break;
                }
            }
        }
    }

    PRETURN;
}

int p_emoteat_trigger(CHAR_DATA *mob, CHAR_DATA *ch, char *emote)
{
    int trig = (mob != ch) ? TRIG_EMOTEAT : TRIG_EMOTESELF;
    return test_string_trigger(emote, "*", match_exact_name, trig, mob, NULL, NULL, ch, NULL, NULL, NULL, NULL);
}

int script_login(CHAR_DATA *ch) // @@@NIB
{
    ITERATOR tit, oit, pit;
    TOKEN_DATA *token;
    OBJ_DATA *obj;
    PROG_LIST *prg;
    SCRIPT_DATA *script;
    unsigned long uid[2];
    //unsigned long ouid[2];
    int slot;
    int ret_val = PRET_NOSCRIPT, ret; // @@@NIB Default for a trigger loop is NO SCRIPT

    variable_dynamic_fix_mobile(ch);

    // Run the SYSTEM LOGIN ROOM SCRIPT
    WNUM wnum;
    if (resolve_widevnum(RPROG_VNUM_PLAYER_INIT, NULL, &wnum))
        script = get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG);
    else
        script = NULL;

    if(script) {
        script_force_execute = true;
        script_security = SYSTEM_SCRIPT_SECURITY;
        execute_script(RPROG_VNUM_PLAYER_INIT, script, NULL, NULL, get_room_index_global(1), NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL,TRIG_LOGIN,0,0,0,0,0);
        script_security = INIT_SCRIPT_SECURITY;
        script_force_execute = false;
    }

    // Run the TRIG_LOGIN
    {
        int trigger_tindex = -1;
        for (int i = 0; i < trigger_table_size; i++) {
            if (trigger_table[i].type == TRIG_LOGIN) {
                trigger_tindex = i;
                break;
            }
        }
        if (trigger_tindex < 0)
            return PRET_NOSCRIPT;
        slot = trigger_table[trigger_tindex].slot;
    }

    // Save the UID
    uid[0] = ch->id[0];
    uid[1] = ch->id[1];

    // Check for tokens FIRST
    iterator_start(&tit, ch->ltokens);
    while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
        if(token->pIndexData && token->pIndexData->progs) {
            script_token_addref(token);
            script_destructed = false;
            iterator_start(&pit, token->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
                    ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL,TRIG_LOGIN,0,0,0,0,0);
                    SETPRET;
                }
            }
            script_token_remref(token);
            iterator_stop(&pit);
        }
    }
    iterator_stop(&tit);

    // Check objects SECOND
    iterator_start(&oit, ch->lcarrying);
    while(( obj = (OBJ_DATA *)iterator_nextdata(&oit))) {
        iterator_start(&tit, obj->ltokens);
        while(( token = (TOKEN_DATA *)iterator_nextdata(&tit))) {
            if(token->pIndexData && token->pIndexData->progs) {
                script_destructed = false;
                iterator_start(&pit, token->pIndexData->progs[slot]);
                while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                    if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
                        ret = execute_script(prg->vnum, prg->script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, NULL,NULL, NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0);
                        SETPRET;
                    }
                }
                iterator_stop(&pit);
            }
        }
        iterator_stop(&tit);

        if(obj->pIndexData->progs) {
            script_destructed = false;
            iterator_start(&pit, obj->pIndexData->progs[slot]);
            while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
                if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number) {
                    ret = execute_script(prg->vnum, prg->script, NULL, obj, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0);
                    SETPRET;
                }
            }
            iterator_stop(&pit);
        }
    }
    iterator_stop(&oit);

    if(ret_val == PRET_NOSCRIPT && IS_VALID(ch) && ch->id[0] == uid[0] && ch->id[0] == uid[1] && IS_NPC(ch) && ch->pIndexData->progs) {
        script_destructed = false;
        iterator_start(&pit, ch->pIndexData->progs[slot]);
        while((prg = (PROG_LIST *)iterator_nextdata(&pit)) && !script_destructed) {
            if (is_trigger_type(prg->trig_type,TRIG_LOGIN) && number_percent() < prg->trig_number &&
                ((ret = execute_script(prg->vnum, prg->script, ch, NULL, NULL, NULL, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL,NULL,NULL,NULL, NULL, TRIG_LOGIN,0,0,0,0,0)) != PRET_NOSCRIPT)) {
                ret_val = ret;
                break;
            }
        }
        iterator_stop(&pit);
    }

    PRETURN; // @@@NIB
}

void do_ifchecks( CHAR_DATA *ch, char *argument)
{
    char buf[MIL]/*, *pbuf*/;
    BUFFER *buffer;
    int i,j;

    if(!ch->lines) {
        send_to_char("Viewing this with paging off is not permitted due to the number of ifchecks.\n\r",ch);
        return;
    }

    buffer = new_buf();
    if(!buffer) {
        send_to_char("WTF?! Couldn't create the buffer!\n\r",ch);
        return;
    }

    add_buf(buffer,"{WIf-Checks:{x\n\r");
    add_buf(buffer,"{D========================================================================={x\n\r");
    add_buf(buffer,"{WNum  {D| {WName                {D| {W              Types               {D| {WValue {D|{x\n\r");
    add_buf(buffer,"{D-------------------------------------------------------------------------{x\n\r");

    for(i=0,j=0;ifcheck_table[i].name;i++) if(!*argument || is_name(argument,ifcheck_table[i].name)) {
        sprintf(buf,"{W%4d{D)  {Y%-20.20s %s %s %s %s %s %s %s   %s{x\n\r",++j,
            ifcheck_table[i].name,
            ((ifcheck_table[i].type & IFC_M) ? "{Gmob" : "   "),
            ((ifcheck_table[i].type & IFC_O) ? "{Gobj" : "   "),
            ((ifcheck_table[i].type & IFC_R) ? "{Groom" : "    "),
            ((ifcheck_table[i].type & IFC_T) ? "{Gtoken" : "     "),
            ((ifcheck_table[i].type & IFC_A) ? "{Garea" : "    "),
            ((ifcheck_table[i].type & IFC_I) ? "{Ginst" : "    "),
            ((ifcheck_table[i].type & IFC_D) ? "{Gdung" : "    "),
            (ifcheck_table[i].numeric ? "{B NUM " : "{R T/F "));
        add_buf(buffer,buf);
    }
    add_buf(buffer,"{D========================================================================={x\n\r");

//	pbuf = buf_string(buffer);
//	sprintf(buf,"pbuf = '%.15s{x', %d\n\r", pbuf, strlen(pbuf));

    if(j > 0)
        page_to_char(buf_string(buffer), ch);
//		send_to_char(buf,ch);
    else
        send_to_char("No ifchecks match that name pattern.\n\r",ch);
    free_buf(buffer);
    return;
}


char *get_script_prompt_string(CHAR_DATA *ch, char *key)
{
    STRING_VECTOR *v;
    if(IS_NPC(ch) || !ch->pcdata->script_prompts) return "";

    v = string_vector_find(ch->pcdata->script_prompts,key);
    return v ? v->string : "";
}


// Returns true if the spell got through.
// Used for token scripts
bool script_spell_deflection(CHAR_DATA *ch, CHAR_DATA *victim, TOKEN_DATA *token, SCRIPT_DATA *script, int mana)
{
    CHAR_DATA *rch = NULL;
    AFFECT_DATA *af;
    int attempts;
    int lev;
    int type;

    if (!IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION))
        return true;

    act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    // Find spell deflection
    for (af = victim->affected; af; af = af->next) {
        if (af->type == skill_lookup("spell deflection"))
        break;
    }

    if (!af) return true;

    lev = (af->level * 3)/4;
    lev = URANGE(15, lev, 90);

    if (number_percent() > lev) {
        if (ch) {
            if (ch == victim)
                send_to_char("Your spell gets through your protective crimson aura!\n\r", ch);
            else {
                act("Your spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n's spell gets through your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                act("$n's spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
            }
        }

        return true;
    }

    type = token->pIndexData->value[TOKVAL_SPELL_TARGET];
    /* it bounces to a random person */
    if (type != TAR_IGNORE)
        for (attempts = 0; attempts < 6; attempts++) {
            rch = get_random_char(NULL, NULL, victim->in_room, NULL);
            if ((ch && rch == ch) || rch == victim ||
                ((type == TAR_CHAR_OFFENSIVE || type == TAR_OBJ_CHAR_OFF) && ch && is_safe(ch, rch, false))) {
                rch = NULL;
                continue;
            }
        }

    // Loses potency with time
    af->level -= 10;
    if (af->level <= 0) {
        send_to_char("{MThe crimson aura around you vanishes.{x\n\r", victim);
        act("{MThe crimson aura around $n vanishes.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        affect_remove(victim, af);
        return true;
    }

    if (rch) {
        if (ch) {
            act("{YYour spell bounces off onto $N!{x",  ch, rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("{Y$n's spell bounces off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
            act("{Y$n's spell bounces off onto $N!{x",  ch, rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        }

        token->value[3] = ch ? ch->tot_level : af->level;

        execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch ? ch : rch, NULL, NULL, rch, NULL,NULL, NULL,"deflection",NULL,TRIG_NONE,0,0,0,0,0);
    } else if (ch) {
        act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{Y$n's spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }

    return false;
}


void token_skill_improve( CHAR_DATA *ch, TOKEN_DATA *token, bool success, int multiplier )
{
    int chance, per;
    char buf[100];
    int rating, max_rating, diff;

    if (IS_NPC(ch))
        return;

    if (IS_SOCIAL(ch))
        return;

    rating = token->value[TOKVAL_SPELL_RATING];
    max_rating = token->pIndexData->value[TOKVAL_SPELL_RATING] * 100;
    diff = token->pIndexData->value[TOKVAL_SPELL_DIFFICULTY];
    if (diff < 1) diff = 1;

    if(!max_rating) max_rating = 100;

    if(rating < 1 || rating >= max_rating)
        return;

    // check to see if the character has a chance to learn
    chance      = 10 * int_app[get_curr_stat(ch, STAT_INT)].learn;
    multiplier  = UMAX(multiplier,1);
    chance     /= (multiplier * diff * 4);
    chance     += ch->level;

    if (number_range(1,1000) > chance)
        return;

    per = 100 * rating / max_rating;

    // now that the character has a CHANCE to learn, see if they really have
    if (success) {
        chance = URANGE(2, 100 - per, 25);
        if (number_percent() < chance) {
            sprintf(buf,"{WYou have become better at %s!{x\n\r", token->name);
            send_to_char(buf,ch);
            token->value[TOKVAL_SPELL_RATING]++;
            gain_exp_typed(ch, NULL, 2 * diff, XP_TYPE_EXPLORATION, true);
        }
    } else {
        chance = URANGE(5, per/2, 30);
        if (number_percent() < chance) {
            sprintf(buf, "{WYou learn from your mistakes, and your %s skill improves.{x\n\r", token->name);
            send_to_char(buf, ch);
            token->value[TOKVAL_SPELL_RATING] += number_range(1,3);
            if(token->value[TOKVAL_SPELL_RATING] >= max_rating)
                token->value[TOKVAL_SPELL_RATING] = max_rating;
            gain_exp_typed(ch, NULL, 2 * diff, XP_TYPE_EXPLORATION, true);
        }
    }
}

SCRIPT_VARINFO *script_get_prior(SCRIPT_VARINFO *info)
{
    return ((info && info->block && info->block->next) ? &(info->block->next->info) : NULL);
}

bool interrupt_script( CHAR_DATA *ch, bool silent )
{
    bool ret = false;

    if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_INTERRUPT, silent?"silent":NULL))
        ret = true;

    if( ch->script_wait > 0) {
        script_end_failure(ch, !silent);
        ret = true;
    }

    return ret;
}

static bool script_varseton_handle_basic_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, char *argument, char *rest, SCRIPT_PARAM *arg)
{
    if (!str_cmp(type_name, "appendline"))
    {
        BUFFER *buffer = new_buf();
        expand_string(info, argument, buffer);
        add_buf(buffer, "\n\r");

        variables_append_string(vars, name, buf_string(buffer));
        free_buf(buffer);
        return true;
    }

    if (!str_cmp(type_name, "bool") || !str_cmp(type_name, "boolean")) {
        switch(arg->type) {
        case ENT_BOOLEAN: variables_set_boolean(vars, name, arg->d.boolean); break;
        case ENT_NUMBER: variables_set_boolean(vars, name, (arg->d.num != 0)); break;
        case ENT_STRING:
            if (is_number(arg->d.str))
                variables_set_boolean(vars, name, (atoi(arg->d.str) != 0));
            else if (!str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "on"))
                variables_set_boolean(vars, name, true);
            else if (!str_cmp(arg->d.str, "false") || !str_cmp(arg->d.str, "no") || !str_cmp(arg->d.str, "off"))
                variables_set_boolean(vars, name, false);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "integer") || !str_cmp(type_name, "number") || !str_cmp(type_name, "num")) {
        switch(arg->type) {
        case ENT_NUMBER: variables_set_integer(vars, name, arg->d.num); break;
        case ENT_STRING:
            if (is_number(arg->d.str))
                variables_set_integer(vars, name, atoi(arg->d.str));
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "rsg") || !str_cmp(type_name, "generator")) {
        BUFFER *spec_buf = new_buf();
        char generated[MSL];

        expand_string(info, argument, spec_buf);
        if (!rsg_generate_spec(buf_string(spec_buf), generated, sizeof(generated))) {
            free_buf(spec_buf);
            return true;
        }

        variables_set_string(vars, name, generated, false);
        free_buf(spec_buf);
        return true;
    }

    if (!str_cmp(type_name, "dec") || !str_cmp(type_name, "decrement")) {
        pVARIABLE var = variable_get(*vars, name);
        if (!var) return true;
        if (var->type != VAR_INTEGER) return true;

        switch(arg->type) {
        case ENT_NUMBER: var->_.i -= arg->d.num; break;
        case ENT_STRING:
            if (is_number(arg->d.str))
                var->_.i -= atoi(arg->d.str);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "inc") || !str_cmp(type_name, "increment")) {
        pVARIABLE var = variable_get(*vars, name);
        if (!var) return true;
        if (var->type != VAR_INTEGER) return true;

        switch(arg->type) {
        case ENT_NUMBER: var->_.i += arg->d.num; break;
        case ENT_STRING:
            if (is_number(arg->d.str))
                var->_.i += atoi(arg->d.str);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "string") || !str_cmp(type_name, "str")) {
        char tmp[MSL], token[MIL], *p;
        int i;

        switch(arg->type) {
        case ENT_NUMBER: sprintf(tmp, "%d", arg->d.num); break;
        case ENT_STRING: strcpy(tmp, arg->d.str); break;
        default: return true;
        }

        if (!(rest = expand_argument(info, rest, arg)))
            return true;

        switch(arg->type) {
        case ENT_NONE:
            variables_set_string(vars, name, tmp, false);
            break;
        case ENT_NUMBER:
            p = tmp;
            for (i = 0; i < arg->d.num && p && *p; i++)
                p = one_argument(p, token);
            if (arg->d.num > 0 && i == arg->d.num)
                variables_set_string(vars, name, token, false);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "append")) {
        char tmp[MSL], token[MIL], *p;
        int i;

        switch(arg->type) {
        case ENT_NUMBER: sprintf(tmp, "%d", arg->d.num); break;
        case ENT_STRING: strcpy(tmp, arg->d.str); break;
        default: return true;
        }

        if (!(rest = expand_argument(info, rest, arg)))
            return true;

        switch(arg->type) {
        case ENT_NONE:
            variables_append_string(vars, name, tmp);
            break;
        case ENT_NUMBER:
            p = tmp;
            for (i = 0; i < arg->d.num && p && *p; i++)
                p = one_argument(p, token);

            if (arg->d.num > 0 && (i == arg->d.num))
                variables_append_string(vars, name, token);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "EXPAND")) {
        int length;
        char *comp_str;
        BUFFER *buffer;

        if (arg->type != ENT_STRING)
            return true;

        comp_str = compile_string(arg->d.str, IFC_ANY, &length, false);
        if (!comp_str)
            return true;

        buffer = new_buf();
        expand_string(info, comp_str, buffer);
        variables_set_string(vars, name, buf_string(buffer), false);

        free_buf(buffer);
        free_string(comp_str);
        return true;
    }

    if (!str_cmp(type_name, "ARGREMOVE")) {
        switch(arg->type) {
        case ENT_NUMBER:
            variables_argremove_string_index(vars, name, arg->d.num);
            break;
        case ENT_STRING:
            variables_argremove_string_phrase(vars, name, arg->d.str);
            break;
        }
        return true;
    }

    if (!str_cmp(type_name, "strformat")) {
        variables_format_string(vars, name);
        return true;
    }

    if (!str_cmp(type_name, "strformatp") || !str_cmp(type_name, "paraformat")) {
        variables_format_paragraph(vars, name);
        return true;
    }

    if (!str_cmp(type_name, "strreplace")) {
        pVARIABLE var = variable_get(*vars, name);
        char o[MSL];
        char n[MSL];
        char *rep;

        if (!var) return true;
        if (var->type != VAR_STRING && var->type != VAR_STRING_S) return true;
        if (arg->type != ENT_STRING) return true;

        strcpy(o, arg->d.str);

        if (!(rest = expand_argument(info, rest, arg)))
            return true;

        if (arg->type != ENT_STRING)
            return true;
        strcpy(n, arg->d.str);

        rep = string_replace_static(var->_.s, o, n);
        if (rep == NULL)
            return true;

        variables_set_string(vars, name, rep, false);
        return true;
    }

    return false;
}

static void script_varseton_handle_mobile_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    ROOM_INDEX_DATA *here, char *rest, SCRIPT_PARAM *arg)
{
    CHAR_DATA *vch = NULL;
    CHAR_DATA *mobs = NULL;
    CHAR_DATA *viewer = NULL;
    char *str = NULL;
    WNUM wnum = wnum_zero;

    if (arg->type == ENT_BLLIST_MOB)
    {
        LLIST *blist = arg->d.blist;
        BUFFER *buffer = NULL;

        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type == ENT_NUMBER)
        {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
            str = NULL;
        }
        else if (arg->type == ENT_STRING)
        {
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                str = NULL;
            else
                str = arg->d.str;
        }
        else
            return;

        viewer = NULL;
        if (*rest)
        {
            if (str)
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if (!(rest = expand_argument(info, rest, arg)))
            {
                if (buffer)
                    free_buf(buffer);
                return;
            }

            if (arg->type == ENT_MOBILE)
                viewer = arg->d.mob;
        }

        vch = script_get_char_blist(info, blist, viewer, false, wnum, str);
        variables_set_mobile(vars, name, vch);
        if (buffer)
            free_buf(buffer);
        return;
    }

    switch(arg->type) {
    case ENT_WIDEVNUM:
        here = get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
        mobs = here ? here->people : NULL;
        break;
    case ENT_NUMBER: {
        WNUM room_wnum = { NULL, 0 };
        char vnum_str[32];
        AREA_DATA *context_area = here ? here->area : NULL;
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        if (parse_widevnum(vnum_str, context_area, &room_wnum) && room_wnum.pArea) {
            here = get_room_index(room_wnum.pArea, room_wnum.vnum);
            mobs = here ? here->people : NULL;
        }
        break;
    }
    case ENT_STRING:
        if (is_number(arg->d.str))
        {
            WNUM room_wnum = { NULL, 0 };
            AREA_DATA *context_area = here ? here->area : NULL;
            if (parse_widevnum(arg->d.str, context_area, &room_wnum) && room_wnum.pArea) {
                here = get_room_index(room_wnum.pArea, room_wnum.vnum);
                mobs = here ? here->people : NULL;
            }
        }
        else if(!str_cmp(arg->d.str, "name")||!str_cmp(arg->d.str, "world"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;
            vch = get_char_world(NULL,arg->d.str);
        }
        else if(!str_cmp(arg->d.str, "here"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;
            vch = get_char_room(NULL,here,arg->d.str);
        }
        else if(!str_cmp(arg->d.str, "vnum"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;

            MOB_INDEX_DATA *mob_index = get_mob_index_from_info(info, arg->d.num);
            if(!mob_index) return;

            vch = get_char_world_index(NULL, mob_index);
        }
        break;
    case ENT_MOBILE:
        vch = arg->d.mob;
        break;
    case ENT_OLLIST_MOB:
        mobs = arg->d.list.ptr.mob ? *arg->d.list.ptr.mob : NULL;
        break;
    case ENT_ROOM:
        mobs = arg->d.room ? arg->d.room->people : NULL;
        break;
    default: return;
    }

    if (mobs) {
        if (!(rest = expand_argument(info, rest, arg)))
            return;

        BUFFER *buffer = NULL;
        wnum = wnum_zero;
        if (arg->type == ENT_NUMBER) {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        }
        else if (arg->type == ENT_STRING) {
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                str = NULL;
            else
                str = arg->d.str;
        } else
            return;

        viewer = NULL;
        if (*rest)
        {
            if (str)
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if (!(rest = expand_argument(info, rest, arg)))
            {
                if (buffer)
                    free_buf(buffer);
                return;
            }

            if (arg->type == ENT_MOBILE)
                viewer = arg->d.mob;
        }

        vch = script_get_char_list(info, mobs, viewer, false, wnum, str);
        if (buffer)
            free_buf(buffer);
    }
    variables_set_mobile(vars, name, vch);
}

static void script_varseton_handle_player_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    char *rest, SCRIPT_PARAM *arg)
{
    CHAR_DATA *vch = NULL;
    CHAR_DATA *mobs = NULL;
    CHAR_DATA *viewer = NULL;
    char *str = NULL;

    if (arg->type == ENT_BLLIST_MOB)
    {
        LLIST *blist = arg->d.blist;
        BUFFER *buffer = NULL;

        if (!(rest = expand_argument(info, rest, arg)))
            return;

        if (arg->type != ENT_STRING)
            return;

        str = arg->d.str;
        viewer = NULL;
        if (*rest)
        {
            buffer = arg->buffer;
            arg->buffer = new_buf();

            if (!(rest = expand_argument(info, rest, arg)))
            {
                if (buffer)
                    free_buf(buffer);
                return;
            }

            if (arg->type == ENT_MOBILE)
                viewer = arg->d.mob;
        }

        vch = script_get_char_blist(info, blist, viewer, true, wnum_zero, str);
        variables_set_mobile(vars, name, vch);
        return;
    }

    switch(arg->type) {
    case ENT_STRING:
        vch = get_player(arg->d.str);
        break;
    case ENT_MOBILE:
        vch = arg->d.mob && !IS_NPC(arg->d.mob) ? arg->d.mob : NULL;
        break;
    case ENT_OLLIST_MOB:
        mobs = arg->d.list.ptr.mob ? *arg->d.list.ptr.mob : NULL;
        if(mobs) {
            if(!expand_argument(info,rest,arg))
                return;
            if(arg->type == ENT_STRING && !is_number(arg->d.str))
                str = arg->d.str;
            else
                mobs = NULL;
        }
        break;
    default: return;
    }

    if(mobs)
    {
        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        vch = script_get_char_list(info, mobs, viewer, true, wnum_zero, str);

        if( buffer )
            free_buf(buffer);
    }
    variables_set_mobile(vars,name,vch);
}

static void script_varseton_handle_object_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    ROOM_INDEX_DATA *here, char *rest, SCRIPT_PARAM *arg)
{
    OBJ_DATA *obj = NULL;
    OBJ_DATA *objs = NULL;
    CHAR_DATA *viewer = NULL;
    LLIST *objs_list = NULL;
    char *str = NULL;
    WNUM wnum = wnum_zero;

    if( arg->type == ENT_BLLIST_OBJ)
    {
        LLIST *blist = arg->d.blist;
        wnum = wnum_zero;
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if( arg->type == ENT_NUMBER )
        {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
            str = NULL;
        }
        else if( arg->type == ENT_STRING )
        {
            if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
            {
                str = NULL;
            }
            else
            {
                str = arg->d.str;
            }
        }
        else
            return;


        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        obj = script_get_obj_blist(info, blist, viewer, wnum, str);
        variables_set_object(vars,name,obj);

        if( buffer )
            free_buf(buffer);
        return;
    }


    switch(arg->type) {
    case ENT_WIDEVNUM:
        here = get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
        objs = here ? here->contents : NULL;
        break;
    case ENT_NUMBER: {
        WNUM room_wnum = { NULL, 0 };
        char vnum_str[32];
        AREA_DATA *context_area = here ? here->area : NULL;
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        if (parse_widevnum(vnum_str, context_area, &room_wnum) && room_wnum.pArea) {
            here = get_room_index(room_wnum.pArea, room_wnum.vnum);
            objs = here ? here->contents : NULL;
        }
        break;
    }
    case ENT_STRING:
        if(is_number(arg->d.str))
        {
            WNUM room_wnum = { NULL, 0 };
            AREA_DATA *context_area = here ? here->area : NULL;
            if (parse_widevnum(arg->d.str, context_area, &room_wnum) && room_wnum.pArea) {
                here = get_room_index(room_wnum.pArea, room_wnum.vnum);
                objs = here ? here->contents : NULL;
            }
        }
        else if(!str_cmp(arg->d.str, "here"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;

            obj = get_obj_here(NULL,here,arg->d.str);
        }
        else if(!str_cmp(arg->d.str, "name")||!str_cmp(arg->d.str, "world"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;

            obj = get_obj_world(NULL, arg->d.str);
        }
        else if(!str_cmp(arg->d.str, "vnum"))
        {
            if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;

            OBJ_INDEX_DATA *obj_index = get_obj_index_from_info(info, arg->d.num);

            obj = get_obj_world_index(NULL, obj_index, false);
        }
        break;
    case ENT_OBJECT:
        obj = arg->d.obj;
        break;
    case ENT_MOBILE:
        if (arg->d.mob && arg->d.mob->lcarrying) {
            objs_list = arg->d.mob->lcarrying;
        } else {
            objs = NULL;
        }
        break;
    case ENT_OLLIST_OBJ:
        if (arg->d.list.ptr.obj && is_llist(*arg->d.list.ptr.obj)) {
            objs_list = (LLIST*)(*arg->d.list.ptr.obj);
        } else {
            objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
        }
        break;
    case ENT_ROOM:
        objs = arg->d.room ? arg->d.room->contents : NULL;
        break;
    default: return;
    }

    if(objs_list) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        wnum = wnum_zero;
        if(arg->type == ENT_NUMBER) {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        }
        else if(arg->type == ENT_STRING) {
            if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                str = NULL;
            else
                str = arg->d.str;
        } else
            return;

        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        obj = script_get_obj_list(info, objs_list, viewer, 0, wnum, str);

        if( buffer )
            free_buf(buffer);
    } else if(objs) {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        wnum = wnum_zero;
        if(arg->type == ENT_NUMBER) {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        }
        else if(arg->type == ENT_STRING) {
            if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                str = NULL;
            else
                str = arg->d.str;
        } else
            return;

        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        obj = script_get_obj_list(info, objs, viewer, 0, wnum, str);

        if( buffer )
            free_buf(buffer);
    }
    variables_set_object(vars,name,obj);
}

static void script_varseton_handle_inventory_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    char *rest, SCRIPT_PARAM *arg, bool worn_only)
{
    OBJ_DATA *obj = NULL;
    OBJ_DATA *objs = NULL;
    CHAR_DATA *viewer = NULL;
    LLIST *inventory_list = NULL;
    char *str = NULL;
    WNUM wnum = wnum_zero;

    switch(arg->type) {
    case ENT_NUMBER:
    {
        char vnum_str[32];
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        if (info->mob) {
            inventory_list = worn_only ? info->mob->lworn : info->mob->lcarrying;
            if (worn_only && !inventory_list)
                objs = info->mob->carrying;
        }
        break;
    }
    case ENT_STRING:
        if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
            str = NULL;
        else
            str = arg->d.str;
        if (info->mob) {
            inventory_list = worn_only ? info->mob->lworn : info->mob->lcarrying;
            if (worn_only && !inventory_list)
                objs = info->mob->carrying;
        }
        break;
    case ENT_MOBILE:
        if (arg->d.mob) {
            inventory_list = worn_only ? arg->d.mob->lworn : arg->d.mob->lcarrying;
            if (worn_only && !inventory_list)
                objs = arg->d.mob->carrying;
        }
        break;
    case ENT_OLLIST_OBJ:
        if (arg->d.list.ptr.obj && is_llist(*arg->d.list.ptr.obj)) {
            inventory_list = (LLIST *)(*arg->d.list.ptr.obj);
        } else {
            objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
        }
        break;
    default:
        return;
    }

    if(inventory_list || objs)
    {
        if(arg->type != ENT_NUMBER && arg->type != ENT_STRING)
        {
            if(!(rest = expand_argument(info,rest,arg)))
                return;
            if(arg->type == ENT_NUMBER) {
                char vnum_str[32];
                snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
                parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
            }
            else if(arg->type == ENT_STRING) {
                if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                    str = NULL;
                else
                    str = arg->d.str;
            } else
                return;
        }

        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        if (inventory_list) {
            obj = script_get_obj_list(info, inventory_list, viewer, worn_only ? 1 : 2, wnum, str);
        } else {
            obj = script_get_obj_list(info, objs, viewer, worn_only ? 1 : 2, wnum, str);
        }

        if( buffer )
            free_buf(buffer);
    }
    variables_set_object(vars,name,obj);
}

static void script_varseton_handle_content_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    char *rest, SCRIPT_PARAM *arg)
{
    OBJ_DATA *obj = NULL;
    OBJ_DATA *objs = NULL;
    OBJ_DATA *container = NULL;
    CHAR_DATA *viewer = NULL;
    LLIST *objs_list = NULL;
    char *str = NULL;
    WNUM wnum = wnum_zero;

    switch(arg->type) {
    case ENT_OLLIST_OBJ:
        if (arg->d.list.ptr.obj && is_llist(*arg->d.list.ptr.obj)) {
            objs_list = (LLIST *)(*arg->d.list.ptr.obj);
        } else {
            objs = arg->d.list.ptr.obj ? *arg->d.list.ptr.obj : NULL;
        }
        break;
    case ENT_OBJECT:
        container = arg->d.obj;
        if (container && is_llist(container->contains)) {
            objs_list = (LLIST *)(container->contains);
        } else {
            objs = container ? container->contains : NULL;
        }
        break;
    default:
        return;
    }

    if(objs_list || objs)
    {
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        if(arg->type == ENT_NUMBER) {
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        }
        else if(arg->type == ENT_STRING) {
            if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
                str = NULL;
            else
                str = arg->d.str;
        } else {
            return;
        }

        BUFFER *buffer = NULL;
        viewer = NULL;
        if( *rest )
        {
            if( str )
            {
                buffer = arg->buffer;
                arg->buffer = new_buf();
            }

            if(!(rest = expand_argument(info,rest,arg)))
            {
                if( buffer )
                    free_buf(buffer);
                return;
            }

            if( arg->type == ENT_MOBILE )
                viewer = arg->d.mob;
        }

        if (objs_list) {
            obj = script_get_obj_list(info, objs_list, viewer, 0, wnum, str);
        } else {
            obj = script_get_obj_list(info, objs, viewer, 0, wnum, str);
        }

        if( buffer )
            free_buf(buffer);
    }

    variables_set_object(vars,name,obj);
}

static bool script_varseton_handle_entity_lookup_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, char *rest, SCRIPT_PARAM *arg)
{
    TOKEN_DATA *token = NULL;
    TOKEN_DATA *tokens = NULL;
    CHAR_DATA *vch = NULL;
    OBJ_DATA *obj = NULL;
    unsigned long id1;

    if(!str_cmp(type_name, "token")) {
        switch(arg->type) {
        case ENT_MOBILE:   tokens = arg->d.mob ? arg->d.mob->tokens : NULL; break;
        case ENT_OBJECT:   tokens = arg->d.obj ? arg->d.obj->tokens : NULL; break;
        case ENT_ROOM:     tokens = arg->d.room ? arg->d.room->tokens : NULL; break;
        case ENT_TOKEN:    token = arg->d.token; break;
        case ENT_OLLIST_TOK: tokens = arg->d.list.ptr.tok ? *arg->d.list.ptr.tok : NULL; break;
        default: return true;
        }

        if(tokens) token = token_find_match(info,tokens, rest, arg);
        variables_set_token(vars,name,token);
        return true;
    }

    if(!str_cmp(type_name, "DICE")) {
        if(arg->type == ENT_DICE && arg->d.dice)
            variables_set_dice(vars,name,arg->d.dice);
        return true;
    }

    if(!str_cmp(type_name, "variable") || !str_cmp(type_name, "var")) {
        pVARIABLE their_vars;
        pVARIABLE their_var;

        switch(arg->type) {
        case ENT_MOBILE:   their_vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? arg->d.mob->progs->vars : NULL; break;
        case ENT_OBJECT:   their_vars = (arg->d.obj && arg->d.obj->progs) ? arg->d.obj->progs->vars : NULL; break;
        case ENT_ROOM:     their_vars = (arg->d.room && arg->d.room->progs) ? arg->d.room->progs->vars : NULL; break;
        case ENT_TOKEN:    their_vars = (arg->d.token && arg->d.token->progs) ? arg->d.token->progs->vars : NULL; break;
        default: return true;
        }

        if(!their_vars)
            return true;

        if(!expand_argument(info,rest,arg))
            return true;

        if(arg->type != ENT_STRING)
            return true;

        their_var = variable_get(their_vars, arg->d.str);
        variables_set_variable(vars,name,their_var);
        return true;
    }

    if(!str_cmp(type_name, "idmobile")) {
        if(arg->type != ENT_NUMBER)
            return true;
        id1 = arg->d.num;
        if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
            return true;
        vch = idfind_mobile(id1,arg->d.num);
        variables_set_mobile(vars,name,vch);
        return true;
    }

    if(!str_cmp(type_name, "idobject")) {
        if(arg->type != ENT_NUMBER)
            return true;
        id1 = arg->d.num;
        if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
            return true;
        obj = idfind_object(id1,arg->d.num);
        variables_set_object(vars,name,obj);
        return true;
    }

    if(!str_cmp(type_name, "idplayer")) {
        if(arg->type != ENT_NUMBER)
            return true;
        id1 = arg->d.num;
        if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
            return true;
        vch = idfind_player(id1,arg->d.num);
        variables_set_mobile(vars,name,vch);
        return true;
    }

    return false;
}

static bool script_varseton_handle_skill_resource_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, ROOM_INDEX_DATA *here, char *rest, SCRIPT_PARAM *arg)
{
    CHAR_DATA *vch = NULL;

    if(!str_cmp(type_name,"croom")) {
        AREA_DATA *area = NULL;
        unsigned long id1 = 0;

        if(arg->type != ENT_NUMBER)
            return true;

        area = find_area_by_vnum(arg->d.num, NULL);
        if (!area)
            area = get_system_area_fallback();
        here = get_room_index(area, arg->d.num);

        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
            return true;
        id1 = arg->d.num;

        if(!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
            return true;

        variables_set_room(vars,name,get_clone_room(here,id1,arg->d.num));
        return true;
    }

    if(!str_cmp(type_name,"skill")) {
        if(arg->type == ENT_STRING)
            variables_set_skill(vars,name,skill_lookup(arg->d.str));
        return true;
    }

    if(!str_cmp(type_name,"skillgroup") || !str_cmp(type_name,"skill_group")) {
        if(arg->type == ENT_STRING)
            variables_set_skill_group(vars, name, skill_group_find(arg->d.str));
        else if(arg->type == ENT_SKILLGROUP)
            variables_set_skill_group(vars, name, arg->d.skill_group);
        return true;
    }

    if(!str_cmp(type_name,"skillinfo")) {
        if(arg->type != ENT_MOBILE)
            return true;

        vch = arg->d.mob;
        if(!expand_argument(info,rest,arg))
            return true;

        if( arg->type == ENT_STRING )
            variables_set_skillinfo(vars,name,vch,skill_lookup(arg->d.str), NULL);
        else if( arg->type == ENT_TOKEN )
            variables_set_skillinfo(vars,name,vch, 0, arg->d.token);
        return true;
    }

    if(!str_cmp(type_name,"song")) {
        if(arg->type == ENT_STRING)
            variables_set_song(vars, name, song_lookup(arg->d.str));
        else if(arg->type == ENT_SONG)
            variables_set_song(vars, name, arg->d.song);
        return true;
    }

    if(!str_cmp(type_name,"spell")) {
        if(arg->type == ENT_SPELL)
            variables_set_spell(vars, name, arg->d.spell);
        return true;
    }

    if(!str_cmp(type_name,"liquid")) {
        if(arg->type == ENT_NUMBER) {
            if (arg->d.num >= 0 && arg->d.num < liquid_count())
                variables_set_liquid(vars, name, arg->d.num);
        }
        else if(arg->type == ENT_STRING) {
            int liq = liquid_lookup(arg->d.str);
            if (liq >= 0)
                variables_set_liquid(vars, name, liq);
        }
        else if(arg->type == ENT_LIQUID)
            variables_set_liquid(vars, name, arg->d.liquid);
        return true;
    }

    if(!str_cmp(type_name,"material")) {
        if(arg->type == ENT_NUMBER) {
            if (arg->d.num >= 0 && arg->d.num < material_count())
                variables_set_material(vars, name, arg->d.num);
        }
        else if(arg->type == ENT_STRING) {
            int mat = material_index_lookup(arg->d.str);
            if (mat >= 0)
                variables_set_material(vars, name, mat);
        }
        else if(arg->type == ENT_MATERIAL)
            variables_set_material(vars, name, arg->d.material);
        return true;
    }

    if(!str_cmp(type_name,"lockstate") || !str_cmp(type_name,"lock_state")) {
        if(arg->type == ENT_LOCK_STATE)
            variables_set_lock_state(vars, name, arg->d.lock_state);
        else if(arg->type == ENT_EXIT) {
            if (arg->d.door.r && arg->d.door.door >= 0 && arg->d.door.door < MAX_DIR && arg->d.door.r->exit[arg->d.door.door])
                variables_set_lock_state(vars, name, &arg->d.door.r->exit[arg->d.door.door]->door.lock);
        }
        return true;
    }

    return false;
}

static bool script_varseton_handle_index_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, SCRIPT_PARAM *arg)
{
    if(!str_cmp(type_name,"mobindex") || !str_cmp(type_name,"mob_index")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_mobindex(vars, name, get_mob_index(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_mobindex(vars, name, get_mob_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_mobindex(vars, name, get_mob_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_MOBINDEX:
            variables_set_mobindex(vars, name, arg->d.mobindex);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"objindex") || !str_cmp(type_name,"obj_index")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_objindex(vars, name, get_obj_index(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_objindex(vars, name, get_obj_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_objindex(vars, name, get_obj_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_OBJINDEX:
            variables_set_objindex(vars, name, arg->d.objindex);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"tokenindex") || !str_cmp(type_name,"tokindex") || !str_cmp(type_name,"token_index")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_tokenindex(vars, name, get_token_index(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_tokenindex(vars, name, get_token_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_tokenindex(vars, name, get_token_index(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_TOKEN_INDEX:
            variables_set_tokenindex(vars, name, arg->d.token_index);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"blueprint") || !str_cmp(type_name,"bp")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_blueprint(vars, name, get_blueprint_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_blueprint(vars, name, get_blueprint_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_blueprint(vars, name, get_blueprint_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_BLUEPRINT:
            variables_set_blueprint(vars, name, arg->d.blueprint);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"bpsection") || !str_cmp(type_name,"blueprint_section")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_blueprint_section(vars, name, get_blueprint_section_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_blueprint_section(vars, name, get_blueprint_section_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_blueprint_section(vars, name, get_blueprint_section_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_BLUEPRINT_SECTION:
            variables_set_blueprint_section(vars, name, arg->d.blueprint_section);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"dngindex") || !str_cmp(type_name,"dungeonindex")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_dungeonindex(vars, name, get_dungeon_index_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_dungeonindex(vars, name, get_dungeon_index_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_dungeonindex(vars, name, get_dungeon_index_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_DUNGEONINDEX:
            variables_set_dungeonindex(vars, name, arg->d.dungeon_index);
            break;
        default:
            break;
        }
        return true;
    }

    if(!str_cmp(type_name,"shipindex")) {
        switch(arg->type) {
        case ENT_WIDEVNUM:
            variables_set_shipindex(vars, name, get_ship_index_for_area(arg->d.wnum.pArea, arg->d.wnum.vnum));
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_shipindex(vars, name, get_ship_index_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                variables_set_shipindex(vars, name, get_ship_index_for_area(index_wnum.pArea, index_wnum.vnum));
            break;
        }
        case ENT_SHIPINDEX:
            variables_set_shipindex(vars, name, arg->d.ship_index);
            break;
        default:
            break;
        }
        return true;
    }

    return false;
}

static bool script_varseton_handle_world_entity_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, char *rest, SCRIPT_PARAM *arg)
{
    if(!str_cmp(type_name,"area")) {
        AREA_DATA *area;

        switch(arg->type) {
        case ENT_NUMBER:    area = get_area_from_uid(arg->d.num); break;
        case ENT_STRING:    area = find_area(arg->d.str); break;
        case ENT_ROOM:      area = arg->d.room ? arg->d.room->area : NULL; break;
        case ENT_AREA:      area = arg->d.area; break;
        default:            area = NULL; break;
        }

        if( area )
            variables_set_area(vars,name,area);
        return true;
    }

    if(!str_cmp(type_name,"arearegion") || !str_cmp(type_name,"aregion")) {
        if (arg->type == ENT_AREA) {
            AREA_DATA *area = arg->d.area;
            AREA_REGION *region = NULL;

            if (!expand_argument(info, rest, arg) || arg->type != ENT_NUMBER)
                return true;

            region = get_area_region_by_uid(area, arg->d.num);
            if (region)
                variables_set_area_region(vars, name, region);
        } else if (arg->type == ENT_AREA_REGION) {
            variables_set_area_region(vars, name, arg->d.aregion);
        }
        return true;
    }

    if(!str_cmp(type_name,"wilds")) {
        WILDS_DATA *wilds = NULL;

        switch(arg->type) {
        case ENT_NUMBER:
            wilds = get_wilds_from_uid(NULL, arg->d.num);
            break;
        case ENT_WILDS:
            wilds = arg->d.wilds;
            break;
        case ENT_WILDS_ID:
            wilds = get_wilds_from_uid(NULL, arg->d.wid);
            break;
        default:
            wilds = NULL;
            break;
        }

        if (wilds)
            variables_set_wilds(vars, name, wilds);
        return true;
    }

    if(!str_cmp(type_name,"section") || !str_cmp(type_name,"sect")) {
        if (arg->type == ENT_SECTION)
            variables_set_instance_section(vars, name, arg->d.section);
        return true;
    }

    if(!str_cmp(type_name,"instance") || !str_cmp(type_name,"inst")) {
        if (arg->type == ENT_INSTANCE)
            variables_set_instance(vars, name, arg->d.instance);
        return true;
    }

    if(!str_cmp(type_name,"dungeon") || !str_cmp(type_name,"dung")) {
        if (arg->type == ENT_DUNGEON)
            variables_set_dungeon(vars, name, arg->d.dungeon);
        return true;
    }

    if(!str_cmp(type_name,"ship")) {
        if (arg->type == ENT_SHIP)
            variables_set_ship(vars, name, arg->d.ship);
        return true;
    }

    if(!str_cmp(type_name,"quest")) {
        if (arg->type == ENT_QUEST)
            variables_set_quest(vars, name, arg->d.quest);
        return true;
    }

    if(!str_cmp(type_name,"quest_stage") || !str_cmp(type_name,"qstage")) {
        if (arg->type == ENT_QUEST_STAGE)
            variables_set_quest_stage(vars, name, arg->d.quest_stage);
        return true;
    }

    if(!str_cmp(type_name,"quest_objective") || !str_cmp(type_name,"qobjective")) {
        if (arg->type == ENT_QUEST_OBJECTIVE)
            variables_set_quest_objective(vars, name, arg->d.quest_objective);
        return true;
    }

    if(!str_cmp(type_name,"class")) {
        if (arg->type == ENT_STRING) {
            CLASS_DATA *clazz = class_find(arg->d.str);
            if (clazz)
                variables_set_class(vars, name, clazz);
        } else if (arg->type == ENT_CLASS) {
            variables_set_class(vars, name, arg->d.clazz);
        }
        return true;
    }

    if(!str_cmp(type_name,"classlevel") || !str_cmp(type_name,"class_level")) {
        if (arg->type == ENT_CLASSLEVEL)
            variables_set_classlevel(vars, name, arg->d.classlevel);
        else if (arg->type == ENT_MOBILE) {
            CHAR_DATA *class_mob = arg->d.mob;
            CLASS_DATA *clazz = NULL;
            CLASS_LEVEL *class_level = NULL;

            if (!expand_argument(info, rest, arg))
                return true;

            if (arg->type == ENT_CLASS)
                clazz = arg->d.clazz;
            else if (arg->type == ENT_STRING)
                clazz = class_find(arg->d.str);

            if (!class_mob || !clazz)
                return true;

            class_level = get_class_level(class_mob, clazz);
            if (class_level)
                variables_set_classlevel(vars, name, class_level);
        }
        return true;
    }

    if(!str_cmp(type_name,"book_page") || !str_cmp(type_name,"bookpage")) {
        if (arg->type == ENT_BOOK_PAGE)
            variables_set_book_page(vars, name, arg->d.book_page);
        return true;
    }

    if(!str_cmp(type_name,"food_buff") || !str_cmp(type_name,"foodbuff")) {
        if (arg->type == ENT_FOOD_BUFF)
            variables_set_food_buff(vars, name, arg->d.food_buff);
        return true;
    }

    if(!str_cmp(type_name,"waypoint")) {
        if (arg->type == ENT_WAYPOINT)
            variables_set_waypoint(vars, name, arg->d.waypoint);
        return true;
    }

    if(!str_cmp(type_name,"stock") || !str_cmp(type_name,"shop_stock")) {
        if (arg->type == ENT_SHOP_STOCK)
            variables_set_shop_stock(vars, name, arg->d.stock);
        return true;
    }

    if(!str_cmp(type_name,"trainer")) {
        if (arg->type == ENT_TRAINER)
            variables_set_trainer(vars, name, arg->d.trainer);
        return true;
    }

    if(!str_cmp(type_name,"trainer_entry") || !str_cmp(type_name,"tentry")) {
        if (arg->type == ENT_TRAINER_ENTRY)
            variables_set_trainer_entry(vars, name, arg->d.trainer_entry);
        return true;
    }

    return false;
}

static bool script_varseton_handle_reputation_and_list_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, char *rest, SCRIPT_PARAM *arg)
{
    if(!str_cmp(type_name,"reputation_index") || !str_cmp(type_name,"repindex") || !str_cmp(type_name,"faction")) {
        REPUTATION_INDEX_DATA *repIndex = NULL;

        switch(arg->type) {
        case ENT_REPUTATION_INDEX:
            repIndex = arg->d.repIndex;
            break;
        case ENT_REPUTATION:
            repIndex = arg->d.reputation ? arg->d.reputation->pIndexData : NULL;
            break;
        case ENT_WIDEVNUM:
            repIndex = get_reputation_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
            break;
        case ENT_NUMBER:
        {
            WNUM index_wnum = wnum_zero;
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                repIndex = get_reputation_index(index_wnum.pArea, index_wnum.vnum);
            break;
        }
        case ENT_STRING:
        {
            WNUM index_wnum = wnum_zero;
            if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                repIndex = get_reputation_index(index_wnum.pArea, index_wnum.vnum);
            break;
        }
        default:
            return true;
        }

        if (repIndex)
            variables_set_reputation_index(vars, name, repIndex);
        return true;
    }

    if(!str_cmp(type_name,"reputation")) {
        if (arg->type == ENT_REPUTATION) {
            variables_set_reputation(vars, name, arg->d.reputation);
        } else if (arg->type == ENT_MOBILE) {
            CHAR_DATA *rep_mob = arg->d.mob;
            REPUTATION_INDEX_DATA *repIndex = NULL;
            REPUTATION_DATA *reputation = NULL;

            if (!expand_argument(info, rest, arg))
                return true;

            if (arg->type == ENT_REPUTATION_INDEX)
                repIndex = arg->d.repIndex;
            else if (arg->type == ENT_WIDEVNUM)
                repIndex = get_reputation_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
            else if (arg->type == ENT_NUMBER) {
                WNUM index_wnum = wnum_zero;
                char vnum_str[32];
                snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
                if (parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &index_wnum))
                    repIndex = get_reputation_index(index_wnum.pArea, index_wnum.vnum);
            } else if (arg->type == ENT_STRING) {
                WNUM index_wnum = wnum_zero;
                if (parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &index_wnum))
                    repIndex = get_reputation_index(index_wnum.pArea, index_wnum.vnum);
            }

            if (!rep_mob || !repIndex)
                return true;

            reputation = get_reputation_char(rep_mob, repIndex->area, repIndex->vnum, false, false);
            if (reputation)
                variables_set_reputation(vars, name, reputation);
        }
        return true;
    }

    if(!str_cmp(type_name,"reputation_rank") || !str_cmp(type_name,"reprank")) {
        if (arg->type == ENT_REPUTATION_RANK) {
            variables_set_reputation_rank(vars, name, arg->d.repRank);
        } else if (arg->type == ENT_REPUTATION) {
            REPUTATION_INDEX_RANK_DATA *rank = NULL;
            if (arg->d.reputation && arg->d.reputation->pIndexData)
                rank = get_reputation_rank(arg->d.reputation->pIndexData, arg->d.reputation->current_rank);
            if (rank)
                variables_set_reputation_rank(vars, name, rank);
        }
        return true;
    }

    if(!str_cmp(type_name,"race")) {
        if (arg->type == ENT_STRING) {
            RACE_DATA *race = race_lookup(arg->d.str);
            if (!race)
                race = race_lookup_name(arg->d.str);
            if (race)
                variables_set_race(vars, name, race);
        } else if (arg->type == ENT_RACE) {
            variables_set_race(vars, name, arg->d.race);
        }
        return true;
    }

    if(!str_cmp(type_name,"moblist")) {
        if( arg->type != ENT_STRING )
            return true;

        if( !str_cmp(arg->d.str, "add") ) {
            if(!(rest = expand_argument(info,rest,arg)))
                return true;

            if( arg->type == ENT_MOBILE && IS_VALID(arg->d.mob) )
                variables_set_list_mob(vars,name,arg->d.mob,false);
        } else if( !str_cmp(arg->d.str, "remove") ) {
            if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
                return true;

            pVARIABLE var = variable_get(*vars, name);

            if( !var || var->type != VAR_BLLIST_MOB || !IS_VALID(var->_.list) )
                return true;

            list_remnthlink(var->_.list, arg->d.num, false);
        } else if( !str_cmp(arg->d.str, "clear") ) {
            pVARIABLE var = variable_get(*vars, name);

            if( !var || var->type != VAR_BLLIST_MOB || !IS_VALID(var->_.list) )
                return true;

            list_clear(var->_.list);
        }
        return true;
    }

    if(!str_cmp(type_name,"objlist")) {
        if( arg->type != ENT_STRING )
            return true;

        if( !str_cmp(arg->d.str, "add") ) {
            if(!(rest = expand_argument(info,rest,arg)))
                return true;

            if( arg->type == ENT_OBJECT && IS_VALID(arg->d.obj) )
                variables_set_list_obj(vars,name,arg->d.obj,false);
        } else if( !str_cmp(arg->d.str, "remove") ) {
            if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
                return true;

            pVARIABLE var = variable_get(*vars, name);

            if( !var || var->type != VAR_BLLIST_OBJ || !IS_VALID(var->_.list) )
                return true;

            list_remnthlink(var->_.list, arg->d.num, false);
        } else if( !str_cmp(arg->d.str, "clear") ) {
            pVARIABLE var = variable_get(*vars, name);

            if( !var || var->type != VAR_BLLIST_OBJ || !IS_VALID(var->_.list) )
                return true;

            list_clear(var->_.list);
        }
        return true;
    }

    return false;
}

static bool script_varseton_handle_room_random_ops(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    const char *type_name, char *rest, SCRIPT_PARAM *arg)
{
    if(!str_cmp(type_name,"findpath")) {
        ROOM_INDEX_DATA *start_room = NULL;
        ROOM_INDEX_DATA *end_room = NULL;
        int depth, in_zone, doors;
        int dir;

        switch(arg->type) {
        case ENT_NUMBER:
        {
            AREA_DATA *area = find_area_by_vnum(arg->d.num, NULL);
            if (!area)
                area = get_system_area_fallback();
            start_room = get_room_index(area, arg->d.num);
            break;
        }
        case ENT_ROOM:
            start_room = arg->d.room;
            break;
        case ENT_EXIT:
            start_room = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL;
            break;
        default:
            break;
        }

        if(!start_room || !(rest = expand_argument(info,rest,arg)))
            return true;

        switch(arg->type) {
        case ENT_NUMBER:
        {
            AREA_DATA *area = find_area_by_vnum(arg->d.num, NULL);
            if (!area)
                area = get_system_area_fallback();
            end_room = get_room_index(area, arg->d.num);
            break;
        }
        case ENT_ROOM:
            end_room = arg->d.room;
            break;
        case ENT_EXIT:
            end_room = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL;
            break;
        default:
            break;
        }

        if(!end_room || !(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
            return true;

        depth = arg->d.num;

        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
            return true;

        in_zone = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "local");

        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
            return true;

        doors = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "doors");

        dir = find_path(start_room->vnum, end_room->vnum, NULL, (doors ? -depth : depth), in_zone);
        if( dir < 0 || dir >= MAX_DIR )
            variables_set_exit(vars,name,NULL);
        else
            variables_set_exit(vars,name,start_room->exit[dir]);
        return true;
    }

    if(!str_cmp(type_name,"randmob")) {
        CHAR_DATA *vch = NULL;

        if( arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || IS_NPC(arg->d.mob) )
            return true;

        vch = arg->d.mob;

        if(!(rest = expand_argument(info,rest,arg)))
            return true;

        if( arg->type == ENT_STRING )
        {
            int continent = get_continent(arg->d.str);
            if( continent < 0 )
                continent = ANY_CONTINENT;

            vch = get_random_mob(vch, continent);
            if( vch != NULL )
                variables_set_mobile(vars,name,vch);
        }
        else if( arg->type == ENT_AREA )
        {
            vch = get_random_mob_area(vch, arg->d.area);
            if( vch != NULL )
                variables_set_mobile(vars,name,vch);
        }
        return true;
    }

    if(!str_cmp(type_name,"randroom")) {
        ROOM_INDEX_DATA *loc = NULL;
        CHAR_DATA *vch = NULL;

        if (arg->type == ENT_AREA)
        {
            loc = get_random_room_area(vch, arg->d.area);
            if( loc != NULL)
                variables_set_room(vars,name,loc);
        }
        else if (arg->type == ENT_NULL)
            vch = NULL;
        else
        {
            if( arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || IS_NPC(arg->d.mob) )
                return true;

            vch = arg->d.mob;
        }

        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
            return true;

        if( arg->type == ENT_STRING )
        {
            int continent = get_continent(arg->d.str);
            if( continent < 0 )
                continent = ANY_CONTINENT;

            loc = get_random_room(vch, continent);
            if( loc != NULL )
                variables_set_room(vars,name,loc);
        }
        return true;
    }

    if(!str_cmp(type_name,"dungeonrand")) {
        ROOM_INDEX_DATA *loc = NULL;

        if( arg->type == ENT_DUNGEON )
            loc = dungeon_random_room(NULL, arg->d.dungeon );
        else if( arg->type == ENT_INSTANCE )
        {
            if( IS_VALID(arg->d.instance) && IS_VALID(arg->d.instance->dungeon) )
                loc = dungeon_random_room(NULL, arg->d.instance->dungeon );
        }
        else if( arg->type == ENT_SECTION )
        {
            if( IS_VALID(arg->d.section) && IS_VALID(arg->d.section->instance) && IS_VALID(arg->d.section->instance->dungeon) )
                loc = dungeon_random_room(NULL, arg->d.section->instance->dungeon );
        }

        if( loc != NULL )
            variables_set_room(vars,name,loc);
        return true;
    }

    if(!str_cmp(type_name,"instancerand")) {
        ROOM_INDEX_DATA *loc = NULL;

        if( arg->type == ENT_INSTANCE )
        {
            if( IS_VALID(arg->d.instance) )
                loc = instance_random_room(NULL, arg->d.instance );
        }
        else if( arg->type == ENT_SECTION )
        {
            if( IS_VALID(arg->d.section) && IS_VALID(arg->d.section->instance) )
                loc = instance_random_room(NULL, arg->d.section->instance );
        }

        if( loc != NULL )
            variables_set_room(vars,name,loc);
        return true;
    }

    if(!str_cmp(type_name,"sectionrand")) {
        ROOM_INDEX_DATA *loc = NULL;

        if( arg->type == ENT_SECTION )
        {
            if( IS_VALID(arg->d.section) )
                loc = section_random_room(NULL, arg->d.section );
        }

        if( loc != NULL )
            variables_set_room(vars,name,loc);
        return true;
    }

    if(!str_cmp(type_name,"specialroom")) {
        ROOM_INDEX_DATA *loc = NULL;

        if( arg->type == ENT_DUNGEON )
        {
            DUNGEON *dungeon = arg->d.dungeon;

            if(!(rest = expand_argument(info,rest,arg)))
                return true;

            if(arg->type == ENT_STRING)
                loc = get_dungeon_special_room_byname(dungeon, arg->d.str);
            else if(arg->type == ENT_NUMBER)
                loc = get_dungeon_special_room(dungeon, arg->d.num);

        }
        else if( arg->type == ENT_INSTANCE )
        {
            INSTANCE *instance = arg->d.instance;

            if(!(rest = expand_argument(info,rest,arg)))
                return true;

            if(arg->type == ENT_STRING)
                loc = get_instance_special_room_byname(instance, arg->d.str);
            else if(arg->type == ENT_NUMBER)
                loc = get_instance_special_room(instance, arg->d.num);
        }

        if( loc != NULL )
            variables_set_room(vars,name,loc);
        return true;
    }

    return false;
}

static void script_varseton_handle_room_type(SCRIPT_VARINFO *info, ppVARIABLE vars, char *name,
    ROOM_INDEX_DATA *here, char *rest, SCRIPT_PARAM *arg)
{
    ITERATOR it;
    LLIST *blist = NULL;
    int idx = 0;

    switch(arg->type) {
    case ENT_WIDEVNUM:
        variables_set_room(vars,name,get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum));
        break;
    case ENT_NUMBER:
    {
        WNUM room_wnum = { NULL, 0 };
        char vnum_str[32];
        AREA_DATA *context_area = here ? here->area : NULL;
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        if (parse_widevnum(vnum_str, context_area, &room_wnum) && room_wnum.pArea)
            variables_set_room(vars,name,get_room_index(room_wnum.pArea, room_wnum.vnum));
        break;
    }
    case ENT_STRING:
        if (is_number(arg->d.str)) {
            WNUM room_wnum = { NULL, 0 };
            AREA_DATA *context_area = here ? here->area : NULL;
            if (parse_widevnum(arg->d.str, context_area, &room_wnum) && room_wnum.pArea)
                variables_set_room(vars,name,get_room_index(room_wnum.pArea, room_wnum.vnum));
        }
        break;
    case ENT_ROOM:
        variables_set_room(vars,name,arg->d.room);
        break;
    case ENT_EXIT:
        here = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL;
        variables_set_room(vars,name,here);
        break;
    case ENT_BLLIST_ROOM:
        blist = arg->d.blist;
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        idx = 0;
        switch(arg->type) {
        default: break;
        case ENT_STRING:
            if( !str_cmp(arg->d.str, "first"))
                idx = 0;
            else if( !str_cmp(arg->d.str, "last"))
                idx = list_size(blist)-1;
            else if( !str_cmp(arg->d.str, "random"))
                idx = number_range(0, list_size(blist)-1);
            break;

        case ENT_NUMBER:
            idx = arg->d.num;
            break;
        }

        {
            LLIST_ROOM_DATA *lroom;
            iterator_start_nth(&it, blist, idx);
            while((lroom = (LLIST_ROOM_DATA *)iterator_nextdata(&it)) && !lroom->room);
            iterator_stop(&it);

            if(lroom && lroom->room)
                variables_set_room(vars,name,lroom->room);
        }
        break;
    case ENT_PLLIST_ROOM:
        blist = arg->d.blist;
        if(!(rest = expand_argument(info,rest,arg)))
            return;

        idx = 0;
        switch(arg->type) {
        default: break;
        case ENT_STRING:
            if( !str_cmp(arg->d.str, "first"))
                idx = 0;
            else if( !str_cmp(arg->d.str, "last"))
                idx = list_size(blist)-1;
            else if( !str_cmp(arg->d.str, "random"))
                idx = number_range(0, list_size(blist)-1);
            break;

        case ENT_NUMBER:
            idx = arg->d.num;
            break;
        }

        variables_set_room(vars, name, list_nthdata(blist, idx));
        break;
    }
}


void script_varseton(SCRIPT_VARINFO *info, ppVARIABLE vars, char *argument, SCRIPT_PARAM *arg)
{
    char buf[MIL], name[MIL], *rest;
    ROOM_INDEX_DATA *here = NULL;
    EXIT_DATA *ex = NULL;
    int vnum = 0, i;

    if(!info) return;

    here = script_room_from_info_context(info);

    if(!vars) return;

    // Get name
    if(!(argument = expand_argument(info,argument,arg)))
        return;

    if( arg->type != ENT_STRING ) return;

    strcpy(name, arg->d.str);
    if(!name[0]) return;

    // Get type
    if(!(argument = expand_argument(info,argument,arg)))
        return;

    if( arg->type != ENT_STRING ) return;

    strncpy(buf, arg->d.str, MIL-1);
    if(!buf[0]) return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    if (script_varseton_handle_basic_ops(info, vars, name, buf, argument, rest, arg))
        return;

    // Copies an extra description
    // Format: ED <OBJECT or ROOM> <keyword>
    if(!str_cmp(buf,"ed")) {
        // ed $<object|room> <keyword>
        char *p = NULL;
        EXTRA_DESCR_DATA *desc;
        ROOM_INDEX_DATA *edroom = NULL;

        switch(arg->type) {
        case ENT_OBJECT:
            if( arg->d.obj->extra_descr )
                desc = arg->d.obj->extra_descr;
            else
                desc = arg->d.obj->pIndexData->extra_descr;

            if( !arg->d.obj->carried_by && !arg->d.obj->in_obj && !arg->d.obj->locker && !arg->d.obj->in_mail )
                edroom = get_environment(arg->d.obj->in_room);
            break;
        case ENT_ROOM:
            desc = arg->d.room->extra_descr;
            edroom = get_environment(arg->d.room);
            break;
        default:return;
        }

        BUFFER *buffer = new_buf();
        expand_string(info,rest,buffer);

        EXTRA_DESCR_DATA *ed = get_extra_descr(buf_string(buffer), desc);

        if( ed )
        {
            p = ed->description;

            if( !p && edroom )
            {
                p = edroom->description;
            }
        }

        variables_set_string(info->var,name,(p ? p : ""),false);

        free_buf(buffer);

    // Format: ROOM <VNUM> - room vnum (supports widevnum)
    // Format: ROOM <ROOM> - explicit room
    // Format: ROOM <EXIT> - gets the destination of the exit
    // Format: ROOM <ROOM-LIST> - first room from the list
    // Format: ROOM <ROOM-LIST> <INDEX> - Nth room from the list
    // Format: ROOM <ROOM-LIST> FIRST - first room from the list
    // Format: ROOM <ROOM-LIST> LAST - last room from the list
    // Format: ROOM <ROOM-LIST> RANDOM - random valid room from the list
    } else if(!str_cmp(buf,"room")) {
        script_varseton_handle_room_type(info, vars, name, here, rest, arg);

    // Find the highest room at that coordinate
    // Format: HIGHROOM <MAP UID> <X> <Y> <Z> <GROUND>
    } else if(!str_cmp(buf,"highroom")) {

        // Format: highroom <wilds-uid> <x> <y> <z> <ground>
        WILDS_DATA *wilds;
        int x, y, z;
        bool ground;

        if(arg->type == ENT_NUMBER) {
            wilds = get_wilds_from_uid(NULL,arg->d.num);
            if(wilds) {
                if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
                x = arg->d.num;
                if(x < 0 || x >= wilds->map_size_x) return;

                if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
                y = arg->d.num;
                if(y < 0 || x >= wilds->map_size_y) return;

                if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_NUMBER) return;
                z = arg->d.num;

                if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) return;
                ground = !str_cmp(arg->d.str,"ground") || !str_cmp(arg->d.str,"true");

                variables_set_room(info->var,name,wilds_seek_down(wilds, x, y, z, ground));
            }
        }

    // Format: COORD <wilds_room>
    // Format: COORD <room-in-wilds>
    // Format: COORD <wilds|wilds_id|wilds_uid> <x> <y>
    } else if(!str_cmp(buf,"coord") || !str_cmp(buf,"coords") || !str_cmp(buf,"wilds_room") || !str_cmp(buf,"wcoord")) {
        WILDS_DATA *wilds = NULL;
        int x = 0;
        int y = 0;

        if (arg->type == ENT_WILDS_ROOM) {
            wilds = get_wilds_from_uid(NULL, arg->d.wroom.wuid);
            x = arg->d.wroom.x;
            y = arg->d.wroom.y;
        } else if (arg->type == ENT_ROOM && arg->d.room && arg->d.room->wilds) {
            wilds = arg->d.room->wilds;
            x = arg->d.room->x;
            y = arg->d.room->y;
        } else if (arg->type == ENT_WILDS || arg->type == ENT_WILDS_ID || arg->type == ENT_NUMBER) {
            if (arg->type == ENT_WILDS)
                wilds = arg->d.wilds;
            else if (arg->type == ENT_WILDS_ID)
                wilds = get_wilds_from_uid(NULL, arg->d.wid);
            else
                wilds = get_wilds_from_uid(NULL, arg->d.num);

            if (!wilds)
                return;

            if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
                return;
            x = arg->d.num;

            if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
                return;
            y = arg->d.num;
        } else {
            return;
        }

        if (!wilds)
            return;

        if (x < 0 || x >= wilds->map_size_x || y < 0 || y >= wilds->map_size_y)
            return;

        variables_set_wilds_room(vars, name, wilds->uid, x, y, false);

    // Format: EXIT <STRING> - finds the exit at the given direction in the current room
    // Format: EXIT <ROOM> <STRING> - same as EXIT <STRING> but at the given room
    // Format: EXIT <EXIT> - explicit exit
    } else if(!str_cmp(buf,"exit")) {
        switch(arg->type) {
        case ENT_ROOM:
            here = arg->d.room;
            if(!here || !expand_argument(info,rest,arg))
                return;
            if(arg->type != ENT_STRING) return;
        case ENT_STRING:
            vnum = get_num_dir(arg->d.str);
            if(vnum < 0) {
                if(!str_cmp(arg->d.str,"random"))
                    vnum = number_range(0,MAX_DIR-1);
                else if(!str_cmp(arg->d.str,"exists")) {
                    for(vnum = number_range(0,MAX_DIR-1), i = 0; i < MAX_DIR && !here->exit[vnum]; i++, vnum = (vnum+1)%MAX_DIR);

                    if(!here->exit[vnum]) vnum = -1;
                } else if(!str_cmp(arg->d.str,"open")) {
                    for(vnum = number_range(0,MAX_DIR-1), i = 0; i < MAX_DIR && !here->exit[vnum]; i++, vnum = (vnum+1)%MAX_DIR);

                    if(!here->exit[vnum] || IS_SET(here->exit[vnum]->exit_info,EX_CLOSED)) vnum = -1;
                }

                if(vnum < 0)
                    return;
            }
            ex = here->exit[vnum];
            break;
        case ENT_EXIT:
            ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
            break;
        }
        variables_set_exit(vars,name,ex);

    // Format: MOBILE <ROOM VNUM or ROOM or MOBLIST> <VNUM or NAME>[ <VIEWER>]
    // Format: MOBILE VNUM <VNUM>[ <VIEWER>]
    // Format: MOBILE NAME|WORLD <NAME>[ <VIEWER>]
    // Format: MOBILE HERE <NAME>[ <VIEWER>]
    // Format: MOBILE <MOBILE>
    } else if(!str_cmp(buf,"mobile") || !str_cmp(buf,"mob")) {
        script_varseton_handle_mobile_type(info, vars, name, here, rest, arg);

    // Format: PLAYER <ROOM VNUM or ROOM or MOBLIST> <NAME>[ <VIEWER>]
    // Format: PLAYER <NAME>
    // Format: PLAYER <PLAYER>
    } else if(!str_cmp(buf,"player")) {
        script_varseton_handle_player_type(info, vars, name, rest, arg);

    // Format: OBJECT <ROOM VNUM or ROOM or MOBILE or OBJECT or OBJLIST> <VNUM or NAME>
    // Format: OBJECT HERE <NAME>
    // Format: OBJECT WORLD <NAME>
    // Format: OBJECT VNUM <VNUM>
    // Format: OBJECT <OBJECT>
    } else if(!str_cmp(buf,"object") || !str_cmp(buf,"obj")) {
        script_varseton_handle_object_type(info, vars, name, here, rest, arg);

    // Format: CARRY <VNUM or NAME>
    // Format: CARRY <MOBILE> <VNUM or NAME>
    // Format: CARRY <OBJLIST> <VNUM or NAME>
    } else if(!str_cmp(buf,"carry")) {
        script_varseton_handle_inventory_type(info, vars, name, rest, arg, false);

    // Format: WORN <VNUM or NAME>
    // Format: WORN <MOBILE or OBJLIST> <VNUM or NAME>
    } else if(!str_cmp(buf,"worn")) {
        script_varseton_handle_inventory_type(info, vars, name, rest, arg, true);

    // Format: CONTENT <OBJECT or OBJLIST> <VNUM or NAME>
    } else if(!str_cmp(buf,"content")) {
        script_varseton_handle_content_type(info, vars, name, rest, arg);

    // Format: TOKEN <MOBILE or OBJECT or ROOM or TOKLIST> <Pattern>
    // Format: TOKEN <TOKEN>
    // Format: DICE <DICE>
    // VARIABLE MOBILE NAME
    // VARIABLE OBJECT NAME
    // VARIABLE ROOM NAME
    // VARIABLE TOKEN NAME
    // Format: IDMOBILE <IDa> <IDb>
    // Format: IDOBJECT <IDa> <IDb>
    // Format: IDPLAYER <IDa> <IDb>
    } else if (script_varseton_handle_entity_lookup_ops(info, vars, name, buf, rest, arg)) {
        return;

    // Format: CROOM <ROOM VNUM> <IDa> <IDb>
    // Format: SKILL <NAME>
    // Format: SKILLGROUP <name>
    // Format: SKILLGROUP <skillgroup>
    // Format: SKILLINFO <MOBILE> <NAME or TOKEN>
    // Format: SONG <NAME>
    // Format: SONG <SONG>
    // Format: SPELL <SPELL>
    // Format: LIQUID <NUMBER|STRING|LIQUID>
    // Format: MATERIAL <NUMBER|STRING|MATERIAL>
    // Format: LOCKSTATE <LOCK_STATE|EXIT>
    // Format: LOCK_STATE <LOCK_STATE|EXIT>
    } else if (script_varseton_handle_skill_resource_ops(info, vars, name, buf, here, rest, arg)) {
        return;

    // Format: MOBINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: MOBINDEX <MOBINDEX>
    // Format: OBJINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: OBJINDEX <OBJINDEX>
    // Format: TOKENINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: TOKENINDEX <TOKEN_INDEX>
    // Format: TOKINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: TOKINDEX <TOKEN_INDEX>
    // Format: BLUEPRINT <WIDEVNUM|NUMBER|STRING>
    // Format: BLUEPRINT <BLUEPRINT>
    // Format: BPSECTION <WIDEVNUM|NUMBER|STRING>
    // Format: BPSECTION <BLUEPRINT_SECTION>
    // Format: DNGINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: DNGINDEX <DUNGEONINDEX>
    // Format: SHIPINDEX <WIDEVNUM|NUMBER|STRING>
    // Format: SHIPINDEX <SHIPINDEX>
    } else if (script_varseton_handle_index_ops(info, vars, name, buf, arg)) {
        return;

    // Format: FINDPATH <ROOM> <ROOM> <DEPTH> <IN-ZONE> <DOORS> - returns the EXIT entity
    // RANDMOB <player> <continent>
    // RANDMOB <player> <area>
    // RANDROOM <area|player|null> <continent>
    // DUNGEONRAND $(SECTION|INSTANCE|DUNGEON)
    // INSTANCERAND $(SECTION|INSTANCE)
    // SECTIONRAND $(SECTION)
    // SPECIALROOM $(INSTANCE|DUNGEON) [#.]KEYWORD
    // SPECIALROOM $(INSTANCE|DUNGEON) INDEX
    } else if (script_varseton_handle_room_random_ops(info, vars, name, buf, rest, arg)) {
        return;

    // AREA uid
    // AREA name
    // AREA room
    // AREA area
    // AREAREGION <area> <region uid>
    // AREAREGION <aregion>
    // AREGION <area> <region uid>
    // AREGION <aregion>
    // Format: WILDS <uid>
    // Format: WILDS <wilds>
    // Format: WILDS <wilds_id>
    // Format: SECTION <section>
    // Format: SECT <section>
    // Format: INSTANCE <instance>
    // Format: INST <instance>
    // Format: DUNGEON <dungeon>
    // Format: DUNG <dungeon>
    // Format: SHIP <ship>
    // Format: QUEST <quest>
    // Format: QUEST_STAGE <quest_stage>
    // Format: QSTAGE <quest_stage>
    // Format: QUEST_OBJECTIVE <quest_objective>
    // Format: QOBJECTIVE <quest_objective>
    // Format: CLASS <name>
    // Format: CLASS <class>
    // Format: CLASSLEVEL <classlevel>
    // Format: CLASSLEVEL <mobile> <class>
    // Format: CLASSLEVEL <mobile> <class-name>
    // Format: BOOK_PAGE <book_page>
    // Format: FOOD_BUFF <food_buff>
    // Format: FOOD_BUFF <obj_food_buff>
    // Format: WAYPOINT <waypoint>
    // Format: STOCK <stock>
    // Format: SHOP_STOCK <stock>
    // Format: TRAINER <trainer>
    // Format: TRAINER_ENTRY <trainer_entry>
    // Format: TENTRY <trainer_entry>
    } else if (script_varseton_handle_world_entity_ops(info, vars, name, buf, rest, arg)) {
        return;

    // Format: REPUTATION_INDEX <widevnum|number|string|repindex|reputation>
    // Format: REPINDEX <widevnum|number|string|repindex|reputation>
    // Format: FACTION <widevnum|number|string|repindex|reputation>
    // Format: REPUTATION <reputation>
    // Format: REPUTATION <mobile> <repindex|widevnum|number|string>
    // Format: REPUTATION_RANK <reprank>
    // Format: REPUTATION_RANK <reputation>
    // Format: RACE <name>
    // Format: RACE <race>
    // MOBLIST add <mobile>
    // MOBLIST remove <index>
    // MOBLIST clear
    // OBJLIST add <object>
    // OBJLIST remove <index>
    // OBJLIST clear
    } else if (script_varseton_handle_reputation_and_list_ops(info, vars, name, buf, rest, arg)) {
        return;

    } else
        return;
}

bool valid_spell_token( TOKEN_DATA *token )
{
    ITERATOR pit;
    PROG_LIST *prg = NULL;

    if( IS_VALID(token) && token->pIndexData && token->pIndexData->progs ) {
        iterator_start(&pit, token->pIndexData->progs[TRIGSLOT_SPELL]);
        while((prg = (PROG_LIST *)iterator_nextdata(&pit)))
            if(prg->trig_type == TRIG_SPELL)
                break;
        iterator_stop(&pit);
    }

    return prg && true;
}

bool visit_script_execute(ROOM_INDEX_DATA *room, void *argv[], int argc, int depth, int door)
{
    int ret;
    SCRIPT_DATA *script = (SCRIPT_DATA *)argv[0];

    ret = execute_script(script->vnum, script, NULL, NULL, room, NULL, NULL, NULL, NULL,
        (CHAR_DATA *)argv[1],	// enactor
        (OBJ_DATA *)argv[2],	// object 1
        (OBJ_DATA *)argv[3],	// object 2
        (CHAR_DATA *)argv[4],	// victim 1
        (CHAR_DATA *)argv[5],	// victim 2
        NULL,					// no random
        NULL,					// no token
        (char *)argv[6],		// phrase
        NULL,					// no trigger
        TRIG_NONE,				// trigger type
        depth,					// register 1
        door,					// register 2
        0,						// register 3
        0,						// register 4
        0);						// register 5

    return ret > 0;
}

void script_end_success(CHAR_DATA *ch)
{
    TOKEN_DATA *tok = NULL;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    SCRIPT_DATA *script = NULL;
    VARIABLE **var = NULL;

    if( ch->script_wait_success != NULL) {
        script = ch->script_wait_success;

        if( ch->script_wait_token != NULL ) {
            if( IS_VALID(ch->script_wait_token) &&
                ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
                tok = ch->script_wait_token;
                var = &tok->progs->vars;
            }
        } else if( ch->script_wait_mob != NULL ) {
            if( IS_VALID(ch->script_wait_mob) &&
                ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
                mob = ch->script_wait_mob;
                var = &mob->progs->vars;
            }
        } else if( ch->script_wait_obj != NULL ) {
            if( IS_VALID(ch->script_wait_obj) &&
                ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
                obj = ch->script_wait_obj;
                var = &obj->progs->vars;
            }
        }

    }

    ch->script_wait_success = NULL;
    ch->script_wait_failure = NULL;
    ch->script_wait_pulse = NULL;
    ch->script_wait_mob = NULL;
    ch->script_wait_obj = NULL;
    ch->script_wait_token = NULL;

    if( var != NULL && script != NULL )
        execute_script(script->vnum, script, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_NONE,0,0,0,0,0);

    ch->script_wait = 0;
}

void script_end_failure(CHAR_DATA *ch, bool messages)
{
    TOKEN_DATA *tok = NULL;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    SCRIPT_DATA *script = NULL;
    VARIABLE **var = NULL;

    if( ch->script_wait_failure != NULL) {
        script = ch->script_wait_failure;

        if( ch->script_wait_token != NULL ) {
            if( IS_VALID(ch->script_wait_token) &&
                ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
                tok = ch->script_wait_token;
                var = &tok->progs->vars;
            }
        } else if( ch->script_wait_mob != NULL ) {
            if( IS_VALID(ch->script_wait_mob) &&
                ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
                mob = ch->script_wait_mob;
                var = &mob->progs->vars;
            }
        } else if( ch->script_wait_obj != NULL ) {
            if( IS_VALID(ch->script_wait_obj) &&
                ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
                ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
                obj = ch->script_wait_obj;
                var = &obj->progs->vars;
            }
        }
    }

    ch->script_wait_success = NULL;
    ch->script_wait_failure = NULL;
    ch->script_wait_pulse = NULL;
    ch->script_wait_mob = NULL;
    ch->script_wait_obj = NULL;
    ch->script_wait_token = NULL;

    if( var != NULL && script != NULL )
        execute_script(script->vnum, script, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, (messages?NULL:"silent"), NULL,TRIG_NONE,0,0,0,0,0);

    ch->script_wait = 0;
}


void script_end_pulse(CHAR_DATA *ch)
{
    TOKEN_DATA *tok = NULL;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    VARIABLE **var = NULL;

    if( ch->script_wait_pulse == NULL)
        return;

    //printf_to_char(ch, "script_end_pulse: %ld\n\r", ch->script_wait);

    if( ch->script_wait_token != NULL ) {
        if( IS_VALID(ch->script_wait_token) &&
            ch->script_wait_token->id[0] == ch->script_wait_id[0] &&
            ch->script_wait_token->id[1] == ch->script_wait_id[1]) {
            tok = ch->script_wait_token;
            var = &tok->progs->vars;
        }
    } else if( ch->script_wait_mob != NULL ) {
        if( IS_VALID(ch->script_wait_mob) &&
            ch->script_wait_mob->id[0] == ch->script_wait_id[0] &&
            ch->script_wait_mob->id[1] == ch->script_wait_id[1]) {
            mob = ch->script_wait_mob;
            var = &mob->progs->vars;
        }
    } else if( ch->script_wait_obj != NULL ) {
        if( IS_VALID(ch->script_wait_obj) &&
            ch->script_wait_obj->id[0] == ch->script_wait_id[0] &&
            ch->script_wait_obj->id[1] == ch->script_wait_id[1]) {
            obj = ch->script_wait_obj;
            var = &obj->progs->vars;
        }
    }

    if(!mob && !obj && !tok) return;

    if( var != NULL )
        execute_script(ch->script_wait_pulse->vnum, ch->script_wait_pulse, mob, obj, NULL, tok, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,TRIG_NONE,0,0,0,0,0);
}

int script_flag_lookup (const char *name, const struct flag_type *flag_table)
{
    int flag;

    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
    if (LOWER(name[0]) == LOWER(flag_table[flag].name[0]) &&
        !str_prefix(name,flag_table[flag].name))
        return flag_table[flag].bit;
    }

    return 0;
}

bool script_bitmatrix_lookup(char *argument, const struct flag_type **bank, long *flags)
{
    char word[MIL];
    bool valid = true;

    if (!bank || !flags) return false;

    for(int i = 0; bank[i]; i++)
        flags[i] = 0;

    while(valid)
    {
        argument = one_argument(argument, word);

        if (word[0] == '\0')
            break;

        long value = 0;
        int nth = -1;
        for(int i = 0; bank[i]; i++)
        {
            value = flag_find(word, bank[i]);
            if (value != 0)
            {
                nth = i;
                break;
            }
        }

        if (value != 0)
        {
            SET_BIT(flags[nth], value);
        }
        else
        {
            valid = false;
        }
    }

    return valid;
}


long script_flag_value( const struct flag_type *flag_table, char *argument)
{
    char word[MAX_INPUT_LENGTH];
    long bit;
    long marked = 0;
    int flag;
    bool found = false;

    if ( flag_table == NULL ) return NO_FLAG;

    if ( is_stat( flag_table ) )
    {
        one_argument( argument, word );

        for (flag = 0; flag_table[flag].name != NULL; flag++)
        {
            if (LOWER(word[0]) == LOWER(flag_table[flag].name[0]) &&
                !str_prefix(word,flag_table[flag].name))
                return flag_table[flag].bit;
        }

        return NO_FLAG;
    }

    /*
     * Accept multiple flags.
     */
    for (; ;)
    {
        argument = one_argument( argument, word );

        if ( word[0] == '\0' )
        break;

        if ( ( bit = script_flag_lookup( word, flag_table ) ) != 0 )
        {
            SET_BIT( marked, bit );
            found = true;
        }
    }

    if ( found )
    return marked;
    else
    return NO_FLAG;
}

CHAR_DATA *script_get_char_room(SCRIPT_VARINFO *info, char *name, bool see_all)
{
    ROOM_INDEX_DATA *resolved_room = NULL;

    if( !info ) return NULL;

    if( info->mob ) {
        if( !info->mob->in_room )
            return NULL;
        if( see_all )	// If see_all, bypass ALL vision checks
            return get_char_room(NULL, info->mob->in_room, name);
        else
            return get_char_room(info->mob, NULL, name);
    }
    resolved_room = script_room_from_info_context(info);
    if (!resolved_room)
        return NULL;
    return get_char_room(NULL, resolved_room, name);

    return NULL;
}

OBJ_DATA *script_get_obj_here(SCRIPT_VARINFO *info, char *name)
{
    ROOM_INDEX_DATA *resolved_room = NULL;

    if( !info ) return NULL;

    if( info->mob ) {
        if( !info->mob->in_room )
            return NULL;
        return get_obj_here(info->mob, NULL, name);
    }
    resolved_room = script_room_from_info_context(info);
    if (!resolved_room)
        return NULL;
    return get_obj_here(NULL, resolved_room, name);

    return NULL;
}

static bool script_get_event_source_from_info(SCRIPT_VARINFO *info, long *event_uid, uint32_t *instance_id, int *bracket)
{
    long uid = 0;
    uint32_t instance = 0;
    int source_bracket = 0;

    if (event_uid)
        *event_uid = 0;
    if (instance_id)
        *instance_id = 0;
    if (bracket)
        *bracket = 0;

    if (!info)
        return false;

    if (info->mob && event_get_mobile_spawn_source(info->mob, &uid, &instance) && uid > 0) {
        event_get_mobile_spawn_bracket(info->mob, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->mob && !IS_NPC(info->mob)
        && event_get_character_active_bracket(info->mob, &uid, &instance, &source_bracket)
        && uid > 0) {
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->obj && event_get_object_spawn_source(info->obj, &uid, &instance) && uid > 0) {
        event_get_object_spawn_bracket(info->obj, &source_bracket);
        if (event_uid)
            *event_uid = uid;
        if (instance_id)
            *instance_id = instance;
        if (bracket)
            *bracket = source_bracket;
        return true;
    }

    if (info->token) {
        if (info->token->player && event_get_mobile_spawn_source(info->token->player, &uid, &instance) && uid > 0) {
            event_get_mobile_spawn_bracket(info->token->player, &source_bracket);
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }

        if (info->token->player && !IS_NPC(info->token->player)
            && event_get_character_active_bracket(info->token->player, &uid, &instance, &source_bracket)
            && uid > 0) {
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }

        if (info->token->object && event_get_object_spawn_source(info->token->object, &uid, &instance) && uid > 0) {
            event_get_object_spawn_bracket(info->token->object, &source_bracket);
            if (event_uid)
                *event_uid = uid;
            if (instance_id)
                *instance_id = instance;
            if (bracket)
                *bracket = source_bracket;
            return true;
        }
    }

    return false;
}

// MLOAD $VNUM|$MOBILE $ROOM[ $VARIABLENAME]
CHAR_DATA *script_mload(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg, bool instanced)
{
    char *rest;
    WNUM wnum = { NULL, 0 };
    MOB_INDEX_DATA *pMobIndex = NULL;
    ROOM_INDEX_DATA *room;
    CHAR_DATA *victim;
    long event_uid = 0;
    uint32_t event_instance_id = 0;
    int event_bracket = 0;

    if(!info) return NULL;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return NULL;

    switch(arg->type) {
    case ENT_WIDEVNUM:
        wnum = arg->d.wnum;
        break;
    case ENT_NUMBER:
    {
        char vnum_str[32];
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        break;
    }
    case ENT_STRING:
        if (arg->d.str)
            parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum);
        break;
    case ENT_MOBILE:
        if (arg->d.mob && arg->d.mob->pIndexData) {
            wnum.pArea = arg->d.mob->pIndexData->area;
            wnum.vnum = arg->d.mob->pIndexData->vnum;
        }
        break;
    default:
        break;
    }

    if (!wnum.pArea || wnum.vnum < 1) {
        return NULL;
    }

    pMobIndex = get_mob_index(wnum.pArea, wnum.vnum);
    if (!pMobIndex) {
        return NULL;
    }

    room = NULL;

    char *var_name = rest;

    if( rest && *rest )
    {
        if(!(rest = expand_argument(info,rest,arg)))
            return NULL;

        if( arg->type == ENT_ROOM )
        {
            room = arg->d.room;
            var_name = rest;
        }
        else if( arg->type == ENT_WIDEVNUM )
        {
            room = get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
            var_name = rest;
        }
        else if( arg->type == ENT_NUMBER )
        {
            WNUM room_wnum = { NULL, 0 };
            char vnum_str[32];
            snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
            parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &room_wnum);
            if (room_wnum.pArea)
                room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            var_name = rest;
        }

    }

    if( !room )
        room = script_room_from_info_context(info);

    if( !room )
        return NULL;

    victim = create_mobile(pMobIndex, false);
    if( !IS_VALID(victim) )
        return NULL;

    if (script_get_event_source_from_info(info, &event_uid, &event_instance_id, &event_bracket) && event_uid > 0) {
        event_tag_mobile_spawn(victim, event_uid, event_instance_id);
        event_set_mobile_spawn_bracket(victim, event_bracket);
    }

    if( instanced )
        SET_BIT(victim->act[1], ACT2_INSTANCE_MOB);

    char_to_room(victim, room);
    if(var_name && *var_name) variables_set_mobile(info->var,var_name,victim);
    p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);

    info->progs->lastreturn = 1;

    return victim;
}

// Helper to get current room from script info
static ROOM_INDEX_DATA *get_current_room_from_info(SCRIPT_VARINFO *info)
{
    return script_room_from_info_context(info);
}

// Unified location parser for all script types
// Supports: ENT_WIDEVNUM, ENT_NUMBER (with area context), wilderness coords, 
// named locations, vroom/clone, ENT_MOBILE, ENT_OBJECT, ENT_ROOM, ENT_EXIT, ENT_TOKEN
char *script_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
{
    char *rest, *rest2;
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    AREA_DATA *area;
    ROOM_INDEX_DATA *loc, *current_room;
    WILDS_DATA *pWilds;
    SCRIPT_PARAM *arg = new_script_param();
    EXIT_DATA *ex;
    int x, y;

    *room = NULL;
    current_room = get_current_room_from_info(info);

    if((rest = expand_argument(info,argument,arg))) {
        switch(arg->type) {
        case ENT_NONE: 
            *room = current_room; 
            break;

        case ENT_WIDEVNUM:
            if (arg->d.wnum.pArea)
                *room = get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
            break;

        case ENT_NUMBER:
            x = arg->d.num;
            rest2 = rest;
            if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                y = arg->d.num;
                if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                    if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

                    if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

                    if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
                        !str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
                        break;

                    rest = rest2;
                    room_used_for_wilderness.wilds = pWilds;
                    room_used_for_wilderness.x = x;
                    room_used_for_wilderness.y = y;
                    *room = &room_used_for_wilderness;
                }
            } else {
                // Single number - resolve with area context
                WNUM wnum = { NULL, 0 };
                char vnum_str[32];
                AREA_DATA *context_area = current_room ? current_room->area : NULL;
                snprintf(vnum_str, sizeof(vnum_str), "%d", x);
                parse_widevnum(vnum_str, context_area, &wnum);
                if (wnum.pArea)
                    *room = get_room_index(wnum.pArea, wnum.vnum);
                rest = rest2;
            }
            break;

        case ENT_STRING:
            if(arg->d.str[0] == '@')
                *room = get_exit_dest(current_room, arg->d.str+1);
            else if(!str_cmp(arg->d.str,"here"))
                *room = current_room;
            else if(!str_cmp(arg->d.str,"vroom") || !str_cmp(arg->d.str,"clone")) {
                int vnum,id1, id2;
                if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                    rest = rest2;
                    vnum = arg->d.num;
                    if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                        rest = rest2;
                        id1 = arg->d.num;
                        if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                            rest = rest2;
                            id2 = arg->d.num;
                            *room = get_clone_room(get_room_index_global(vnum),id1,id2);
                        }
                    }
                }
            } else if(!str_cmp(arg->d.str,"wilds")) {
                if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                    x = arg->d.num;
                    if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                        y = arg->d.num;
                        if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                            if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;
                            if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

                            if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
                                !str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
                                break;

                            room_used_for_wilderness.wilds = pWilds;
                            room_used_for_wilderness.x = x;
                            room_used_for_wilderness.y = y;
                            *room = &room_used_for_wilderness;
                        }
                    }
                }
            } else {
                // Named locations: search area names, then mobs, then objects
                loc = NULL;
                for (area = area_first; area; area = area->next) {
                    if (!str_infix(arg->d.str, area->name)) {
                        if(!(loc = get_area_recall_room(area))) {
                            // Find any room in this area by iterating hash buckets
                            for (int iHash = 0; iHash < MAX_KEY_HASH && !loc; iHash++)
                                if ((loc = area->room_index_hash[iHash]) != NULL)
                                    break;
                        }
                        break;
                    }
                }

                if(!loc) {
                    // Search for character by name
                    victim = NULL;
                    if (info->mob) victim = get_char_world(info->mob, arg->d.str);
                    else if (info->obj) victim = get_char_world(NULL, arg->d.str);
                    else if (info->room || info->token) victim = get_char_world(NULL, arg->d.str);
                    
                    if(victim)
                        loc = victim->in_room;
                    else {
                        // Search for object by name
                        obj = NULL;
                        if (info->mob) obj = get_obj_world(info->mob, arg->d.str);
                        else obj = get_obj_world(NULL, arg->d.str);
                        
                        if (obj)
                            loc = obj_room(obj);
                    }
                }
                *room = loc;
            }
            break;

        case ENT_MOBILE:
            *room = arg->d.mob ? arg->d.mob->in_room : NULL; 
            break;
        case ENT_OBJECT:
            *room = arg->d.obj ? obj_room(arg->d.obj) : NULL; 
            break;
        case ENT_ROOM:
            *room = arg->d.room; 
            break;
        case ENT_EXIT:
            ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
            *room = ex ? exit_destination(ex) : NULL; 
            break;
        case ENT_TOKEN:
            *room = token_room(arg->d.token); 
            break;
        }
    }

    free_script_param(arg);
    return rest;
}

AREA_DATA *get_area_from_scriptinfo(SCRIPT_VARINFO *info)
{
    if (!info) return NULL;
    if (!info->block) return NULL;
    if (!info->block->script) return NULL;
    return info->block->script->area;
}

/**
 * get_script_from_info - Look up a script using the calling script's area context
 *
 * Tries the script's own area first, then falls back to global lookup.
 */
SCRIPT_DATA *get_script_from_info(SCRIPT_VARINFO *info, long vnum, int type)
{
    AREA_DATA *area = get_area_from_scriptinfo(info);
    WNUM wnum;
    if (resolve_widevnum(vnum, area, &wnum))
        return get_script_index(wnum.pArea, wnum.vnum, type);
    return NULL;
}

/**
 * get_script_from_arg - Resolve a script reference argument to a script index
 *
 * Supports:
 * - Numeric values (existing behavior via get_script_from_info)
 * - Numeric strings (via parse_widevnum)
 * - Explicit widevnum strings, parsed with script area context
 */
SCRIPT_DATA *get_script_from_arg(SCRIPT_VARINFO *info, SCRIPT_PARAM *arg, int type, long *resolved_vnum)
{
    long vnum = 0;

    if (!info || !arg) {
        return NULL;
    }

    switch (arg->type) {
    case ENT_WIDEVNUM:
        if (!arg->d.wnum.pArea || arg->d.wnum.vnum < 1)
            return NULL;

        if (resolved_vnum) {
            *resolved_vnum = arg->d.wnum.vnum;
        }

        return get_script_index(arg->d.wnum.pArea, arg->d.wnum.vnum, type);

    case ENT_NUMBER:
        vnum = arg->d.num;
        if (resolved_vnum) {
            *resolved_vnum = vnum;
        }
        return (vnum > 0) ? get_script_from_info(info, vnum, type) : NULL;

    case ENT_STRING:
        if (IS_NULLSTR(arg->d.str)) {
            return NULL;
        }

        {
            WNUM wnum = wnum_zero;
            AREA_DATA *context = get_area_from_scriptinfo(info);

            if (parse_widevnum(arg->d.str, context, &wnum) && wnum.vnum > 0) {
                if (resolved_vnum) {
                    *resolved_vnum = wnum.vnum;
                }

                if (wnum.pArea) {
                    return get_script_index(wnum.pArea, wnum.vnum, type);
                }

                return get_script_from_info(info, wnum.vnum, type);
            }
        }
        return NULL;

    default:
        return NULL;
    }
}

/**
 * get_mob_index_from_info - Look up a mob index using the calling script's area context
 */
MOB_INDEX_DATA *get_mob_index_from_info(SCRIPT_VARINFO *info, long vnum)
{
    AREA_DATA *area = get_area_from_scriptinfo(info);
    WNUM wnum;
    if (resolve_widevnum(vnum, area, &wnum))
        return get_mob_index(wnum.pArea, wnum.vnum);
    return NULL;
}

/**
 * get_obj_index_from_info - Look up an obj index using the calling script's area context
 */
OBJ_INDEX_DATA *get_obj_index_from_info(SCRIPT_VARINFO *info, long vnum)
{
    AREA_DATA *area = get_area_from_scriptinfo(info);
    WNUM wnum;
    if (resolve_widevnum(vnum, area, &wnum))
        return get_obj_index(wnum.pArea, wnum.vnum);
    return NULL;
}

/**
 * get_room_index_from_info - Look up a room index using the calling script's area context
 */
ROOM_INDEX_DATA *get_room_index_from_info(SCRIPT_VARINFO *info, long vnum)
{
    AREA_DATA *area = get_area_from_scriptinfo(info);
    WNUM wnum;
    if (resolve_widevnum(vnum, area, &wnum))
        return get_room_index(wnum.pArea, wnum.vnum);
    return NULL;
}

/**
 * get_token_index_from_info - Look up a token index using the calling script's area context
 */
TOKEN_INDEX_DATA *get_token_index_from_info(SCRIPT_VARINFO *info, long vnum)
{
    AREA_DATA *area = get_area_from_scriptinfo(info);
    WNUM wnum;
    if (resolve_widevnum(vnum, area, &wnum))
        return get_token_index(wnum.pArea, wnum.vnum);
    return NULL;
}

// OLOAD $VNUM|$OBJECT $LEVEL[ none|room|wear|$MOBILE[ wear]|$OBJECT|$ROOM[ $VARIABLENAME]]
OBJ_DATA *script_oload(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg, bool instanced)
{
    char buf[MIL], *rest;
    WNUM wnum = { NULL, 0 };
    long level;
    bool fToroom = false, fWear = false;
    OBJ_INDEX_DATA *pObjIndex = NULL;
    OBJ_DATA *obj;
    CHAR_DATA *to_mob = info->mob;
    OBJ_DATA *to_obj = NULL;
    ROOM_INDEX_DATA *here = NULL;
    ROOM_INDEX_DATA *to_room = NULL;
    int event_bracket = 0;
    long event_uid = 0;
    uint32_t event_instance_id = 0;

    if(!info) return NULL;

    info->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg)))
        return NULL;

    here = script_room_from_info_context(info);

    switch(arg->type) {
    case ENT_WIDEVNUM:
        wnum = arg->d.wnum;
        break;
    case ENT_NUMBER:
    {
        // Backward compatibility: convert to string and parse
        char vnum_str[32];
        snprintf(vnum_str, sizeof(vnum_str), "%d", arg->d.num);
        parse_widevnum(vnum_str, get_area_from_scriptinfo(info), &wnum);
        break;
    }
    case ENT_STRING:
        if (arg->d.str)
            parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum);
        break;
    case ENT_OBJECT:
        if (arg->d.obj && arg->d.obj->pIndexData) {
            wnum.pArea = arg->d.obj->pIndexData->area;
            wnum.vnum = arg->d.obj->pIndexData->vnum;
        }
        break;
    default:
        break;
    }

    if (!wnum.pArea || wnum.vnum < 1) {
        pbugf(LOG_SCRIPTS, "script_oload - Bad wnum arg (%ld#%ld)", 
            wnum.pArea ? wnum.pArea->uid : 0, wnum.vnum);
        return NULL;
    }

    pObjIndex = get_obj_index(wnum.pArea, wnum.vnum);
    if (!pObjIndex) {
        pbugf(LOG_SCRIPTS, "script_oload - Bad obj index (%ld#%ld)",
            wnum.pArea->uid, wnum.vnum);
        return NULL;
    }

    if(rest && *rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg)))
            return NULL;

        switch(arg->type) {
        case ENT_NUMBER: level = arg->d.num; break;
        case ENT_STRING: level = arg->d.str ? atoi(arg->d.str) : 0; break;
        case ENT_MOBILE: level = arg->d.mob ? get_staff_rank(arg->d.mob) : 0; break;
        case ENT_OBJECT: level = arg->d.obj ? arg->d.obj->pIndexData->level : 0; break;
        default: level = 0; break;
        }

        if(level <= 0 || level > pObjIndex->level)
            level = pObjIndex->level;

        if(rest && *rest) {
            argument = rest;
            if(!(rest = expand_argument(info,argument,arg)))
                return NULL;

            /*
             * Added 3rd argument
             * omitted - load to mobile's inventory
             * 'none'  - load to mobile's inventory
             * 'room'  - load to room
             * 'wear'  - load to mobile and force wear
             * MOBILE  - load to target mobile
             *         - 'W' automatically wear
             * OBJECT  - load to target object
             * ROOM    - load to target room
             */

            switch(arg->type) {
            case ENT_STRING:
                if(!str_cmp(arg->d.str, "room"))
                    fToroom = true;
                else if(!str_cmp(arg->d.str, "wear"))
                    fWear = true;
                break;

            case ENT_MOBILE:
                to_mob = arg->d.mob;
                if((rest = one_argument(rest,buf))) {
                    if(!str_cmp(buf, "wear"))
                        fWear = true;
                    // use "none" for neither
                }
                break;

            case ENT_OBJECT:
                if( arg->d.obj && IS_SET(pObjIndex->wear_flags, ITEM_TAKE) ) {
                    if(arg->d.obj->item_type == ITEM_CONTAINER ||
                        arg->d.obj->item_type == ITEM_CART)
                        to_obj = arg->d.obj;
                    else if(arg->d.obj->item_type == ITEM_WEAPON_CONTAINER &&
                        pObjIndex->item_type == ITEM_WEAPON &&
                        IS_WEAPON(pObjIndex) && IS_WEAPON_CON(arg->d.obj) &&
                        WEAPON(pObjIndex)->weapon_class == WEAPON_CON(arg->d.obj)->weapon_type)
                        to_obj = arg->d.obj;
                    else
                        return NULL;	// Trying to put the item into a non-container won't work
                }
                break;

            case ENT_ROOM:		to_room = arg->d.room; break;
            }
        }

    } else
        level = pObjIndex->level;

    obj = create_object(pObjIndex, level, true);
    if( !IS_VALID(obj) )
        return NULL;

    if (script_get_event_source_from_info(info, &event_uid, &event_instance_id, &event_bracket) && event_uid > 0) {
        event_tag_object_spawn(obj, event_uid, event_instance_id);
        event_set_object_spawn_bracket(obj, event_bracket);
    }

    if( instanced )
        SET_BIT(obj->extra[2], ITEM_INSTANCE_OBJ);

    if( to_room )
        obj_to_room(obj, to_room);
    else if( to_obj )
        obj_to_obj(obj, to_obj);
    else if( to_mob && (fWear || !fToroom) && CAN_WEAR(obj, ITEM_TAKE) &&
        (to_mob->carry_number < can_carry_n (to_mob)) &&
        (get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
        obj_to_char(obj, to_mob);
        if (fWear)
            wear_obj(to_mob, obj, true);
    }
    else if( here )
        obj_to_room(obj, here);
    else
    {
        // No place to put the object, nuke it

        // This shouldn't be necessary since it was never put anywhere, used anywhere!
        //extract_obj(obj);

        // This is the minimum actions necessary for a phantom object extraction
        list_remlink(loaded_objects, obj, false);
        loaded_obj_hash_remove(obj);
        --obj->pIndexData->count;
        free_obj(obj);
        return NULL;
    }


    if(rest && *rest) variables_set_object(info->var,rest,obj);
    p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);

    info->progs->lastreturn = 1;

    obj->script_created = true;
    obj->created_script_load.vnum = info->block->script->vnum;
    obj->created_script_type = info->block->script->type;

    return obj;
}

void scriptcmd_bug(SCRIPT_VARINFO *info, char *message)
{
    if (info && info->block && info->block->script)
        script_log_runtime_error_context(info->block->script, info->block->line, message ? message : "(null)", info);

    pbugf(LOG_SCRIPTS, "Script:%ld#%d:Line:%d:%s\n\r",
        info->block->script->area->uid, info->block->script->vnum,
        info->block->line,
        message);
}

int cmd_operator_lookup(const char *str)
{
    for(int i = 0; cmd_operator_table[i]; i++)
        if (!str_cmp(str, cmd_operator_table[i]))
            return i;
    
    return OPR_UNKNOWN;
}