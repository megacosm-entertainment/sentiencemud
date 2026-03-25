# Channel.Message Modernization & GMCP Preferences Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize `Sentience.Channel.Message` to use Jansson builders with `_v` versioning, add a `PREF_CAT_GMCP` preference category with three GMCP control preferences, add bidirectional `Sentience.Client.Preferences` GMCP package, and replace hardcoded minimap suppression with preference-driven suppression.

**Architecture:** Three preferences (`gmcp_channels`, `gmcp_suppress_channels`, `gmcp_suppress_minimap`) stored in the existing hierarchical pref system under a new `PREF_CAT_GMCP` category. Connection-type defaults (WebSocket=ON, telnet=OFF) are seeded at login. The channel GMCP path in `channels/channel_gmcp.c` is modernized to use Jansson builders and `sentience_send_package()`. A new `Sentience.Client.Preferences` GMCP package allows the web client to read/set these prefs bidirectionally.

**Tech Stack:** C, Jansson (JSON), existing preference system (`account/preferences.h/c`), GMCP protocol layer (`protocol.c/h`), channel subsystem (`channels/`).

**Spec:** `docs/superpowers/specs/2026-03-25-gmcp-channel-message-design.md`

**Build/test commands:**
```bash
cd /sentience/src && ./build tests          # Build with test support
cd /sentience/src && ./install              # Symlink debug build
cd /sentience && ./sent -test:gmcp          # Run GMCP tests (expect 28+ pass)
cd /sentience && ./sent -test:slink         # Run slink tests (expect 14 pass)
```

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `account/preferences.h` | Modify | Add `PREF_CAT_GMCP` (4), bump `PREF_CAT_MAX` to 5, declare accessor functions |
| `account/preferences.c` | Modify | Implement GMCP accessors, `pref_apply_gmcp_defaults()`, add GMCP section to `do_prefs`, add toggle handling for `gmcp_*` keys |
| `gmcp_sentience.h` | Modify | Add `sentience_channel_message_input_t` struct, declare builder, declare `sentience_send_package()` |
| `gmcp_sentience.c` | Modify | Add Channel.Message builder, make `sentience_send_package()` non-static, add `sentience_send_client_preferences()`, register packages in capabilities |
| `protocol.h` | Modify | Add `GMCP_SENTIENCE_CLIENT_PREFERENCES` to receive enum |
| `protocol.c` | Modify | Add to `GMCPReceiveTable`, add handler in `ParseGMCP` switch |
| `channels/channel_gmcp.c` | Modify | Use Jansson builder + `sentience_send_package()`, add pref filtering |
| `channels/channel_service.c` | Modify | Add inline suppression check in `channel_can_deliver_to_descriptor()` |
| `act_info.c` | Modify | Replace hardcoded GMCP suppression with `pref_gmcp_suppress_minimap()` |
| `nanny.c` | Modify | Call `pref_apply_gmcp_defaults()` at login and character creation |
| `tests/unit/gmcp_sentience_tests.c` | Modify | Add Channel.Message builder tests |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Modify | Add test scenarios |

---

### Task 1: Add PREF_CAT_GMCP and accessor functions

Add the new preference category and the three convenience accessors to the preference system.

**Files:**
- Modify: `account/preferences.h` (lines 37-41 for category defines, ~line 420 for new declarations)
- Modify: `account/preferences.c` (after `pref_check_channel`, ~line 2100)

- [ ] **Step 1: Add PREF_CAT_GMCP constant**

In `account/preferences.h`, change the category defines (lines 37-41):

```c
#define PREF_CAT_TOGGLE       0  /* Toggle/config settings (act/comm flags)  */
#define PREF_CAT_CHANNEL      1  /* Channel on/off and display flags         */
#define PREF_CAT_PROMPT       2  /* Prompt string                            */
#define PREF_CAT_DISPLAY      3  /* Display settings (future: colours, etc.) */
#define PREF_CAT_GMCP         4  /* GMCP delivery and suppression controls   */
#define PREF_CAT_MAX          5  /* Sentinel — keep last                     */
```

- [ ] **Step 2: Declare accessor functions**

In `account/preferences.h`, after the `pref_check_channel` declaration (~line 420), add:

```c
/**
 * GMCP preference accessors — convenience wrappers for PREF_CAT_GMCP bools
 *
 * These check the full inheritance chain (char → account → game default)
 * and return the effective boolean value.
 */
bool pref_gmcp_channels(CHAR_DATA *ch);
bool pref_gmcp_suppress_channels(CHAR_DATA *ch);
bool pref_gmcp_suppress_minimap(CHAR_DATA *ch);

/**
 * pref_apply_gmcp_defaults - Seed GMCP prefs based on connection type
 *
 * Called at login. If no GMCP preference exists anywhere in the inheritance
 * chain, sets a character-level override based on the descriptor's connection
 * type: WebSocket = true, telnet/TLS = false.
 *
 * @param ch  Character entering the game (must have ch->desc set)
 */
void pref_apply_gmcp_defaults(CHAR_DATA *ch);
```

- [ ] **Step 3: Implement accessor functions**

In `account/preferences.c`, after the `pref_check_channel` function (~line 2100), add:

```c
/*
 * GMCP preference accessors
 */

static bool pref_gmcp_get(CHAR_DATA *ch, const char *key)
{
    ACCOUNT_DATA *account;

    if (!ch || IS_NPC(ch))
        return false;

    account = ch->desc ? ch->desc->account : NULL;
    return pref_get_bool(account, ch, key, false);
}

bool pref_gmcp_channels(CHAR_DATA *ch)
{
    return pref_gmcp_get(ch, "gmcp_channels");
}

bool pref_gmcp_suppress_channels(CHAR_DATA *ch)
{
    return pref_gmcp_get(ch, "gmcp_suppress_channels");
}

bool pref_gmcp_suppress_minimap(CHAR_DATA *ch)
{
    return pref_gmcp_get(ch, "gmcp_suppress_minimap");
}
```

- [ ] **Step 4: Implement pref_apply_gmcp_defaults()**

In `account/preferences.c`, after the accessor functions:

```c
/**
 * pref_apply_gmcp_defaults - Seed GMCP prefs based on connection type
 */
void pref_apply_gmcp_defaults(CHAR_DATA *ch)
{
    static const char *gmcp_keys[] = {
        "gmcp_channels",
        "gmcp_suppress_channels",
        "gmcp_suppress_minimap",
        NULL
    };
    ACCOUNT_DATA *account;
    bool is_websocket;
    int i;

    if (!ch || IS_NPC(ch) || !ch->pcdata || !ch->desc)
        return;

    account = ch->desc->account;
    is_websocket = (ch->desc->conn
                    && ch->desc->conn->type == CONN_TYPE_WEBSOCKET_TLS);

    for (i = 0; gmcp_keys[i]; i++) {
        /* If a value exists anywhere in the chain, don't override */
        if (ch->pcdata->preferences
            && pref_find(ch->pcdata->preferences, gmcp_keys[i]))
            continue;
        if (account && account->preferences
            && pref_find(account->preferences, gmcp_keys[i]))
            continue;

        /* No value anywhere — seed from connection type */
        pref_set_bool(&ch->pcdata->preferences, PREF_CAT_GMCP,
                      gmcp_keys[i], is_websocket);
    }
}
```

Note: `connection.h` must be included. Check if `account/preferences.c` already includes it; if not, add `#include "../connection.h"` near the top.

- [ ] **Step 5: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean build (no new tests yet — just infrastructure).

- [ ] **Step 6: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(prefs): add PREF_CAT_GMCP with accessor functions and connection-type defaults

- Add PREF_CAT_GMCP (4) to preference categories
- Add pref_gmcp_channels(), pref_gmcp_suppress_channels(), pref_gmcp_suppress_minimap()
- Add pref_apply_gmcp_defaults() to seed prefs from connection type at login
- WebSocket connections default to ON, telnet/TLS default to OFF

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Wire GMCP defaults into login and add to prefs display

Hook `pref_apply_gmcp_defaults()` into the login flow and display GMCP prefs in the `prefs` command output.

**Files:**
- Modify: `nanny.c` (lines 2913 and 5472 — after `pref_apply_to_character`)
- Modify: `account/preferences.c` (~line 2420 — `do_prefs` function)

- [ ] **Step 1: Hook into login flow in nanny.c**

In `nanny.c`, after the existing `pref_apply_to_character()` call at line ~2913 (existing character login):

```c
    if (d->account) {
        pref_apply_to_character(d->account, ch,
                                ch->pcdata ? ch->pcdata->preferences : NULL);
    }
    pref_apply_gmcp_defaults(ch);
```

And at line ~5472 (new character creation), after:

```c
    pref_apply_game_defaults(ch);
    if (d->account)
        pref_apply_to_character(d->account, ch, ch->pcdata->preferences);
    pref_apply_gmcp_defaults(ch);
```

Make sure to add `#include "account/preferences.h"` in nanny.c if not already present.

- [ ] **Step 2: Add GMCP section to prefs display**

In `account/preferences.c`, add a `show_gmcp_settings()` function before `do_prefs`:

```c
static void show_gmcp_settings(CHAR_DATA *ch)
{
    static const struct {
        const char *key;
        const char *label;
    } gmcp_prefs[] = {
        { "gmcp_channels",          "GMCP Channel Data"     },
        { "gmcp_suppress_channels", "Suppress Inline Channels" },
        { "gmcp_suppress_minimap",  "Suppress Inline Minimap"  },
        { NULL, NULL }
    };
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;
    int i;

    send_to_char("\n\r{C--- GMCP ---{x\n\r", ch);

    for (i = 0; gmcp_prefs[i].key; i++) {
        bool val = pref_get_bool(acct, ch, gmcp_prefs[i].key, false);
        const char *source = "{D(def)";

        if (ch->pcdata->preferences
            && pref_find(ch->pcdata->preferences, gmcp_prefs[i].key))
            source = "{Y(char)";
        else if (acct && acct->preferences
                 && pref_find(acct->preferences, gmcp_prefs[i].key))
            source = "{C(acct)";

        sprintf(buf, "  %-26s %s%-3s{x  %s{x\n\r",
                gmcp_prefs[i].label,
                val ? "{G" : "{R",
                val ? "ON" : "OFF",
                source);
        send_to_char(buf, ch);
    }
}
```

Then in `do_prefs` (~line 2430), add the call after `show_filter_settings(ch)`:

```c
        show_toggle_settings(ch);
        show_channel_settings(ch);
        show_prompt_settings(ch);
        show_filter_settings(ch);
        show_gmcp_settings(ch);
```

- [ ] **Step 3: Add gmcp_* toggle handling to do_prefs**

In `do_prefs`, before the existing toggle-by-name logic, add handling for `gmcp_*` keys. Find where the function handles toggle arguments (after the subcommand checks like reset, snapshot, channels, filter). Add:

```c
    /* GMCP preference toggle */
    if (!str_prefix("gmcp_", arg)) {
        static const char *valid_gmcp_keys[] = {
            "gmcp_channels", "gmcp_suppress_channels", "gmcp_suppress_minimap", NULL
        };
        int i;
        bool found = false;

        for (i = 0; valid_gmcp_keys[i]; i++) {
            if (!str_cmp(arg, valid_gmcp_keys[i])) {
                found = true;
                break;
            }
        }

        if (!found) {
            send_to_char("Unknown GMCP preference. Valid: gmcp_channels, "
                         "gmcp_suppress_channels, gmcp_suppress_minimap\n\r", ch);
            return;
        }

        ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;
        bool current = pref_get_bool(acct, ch, arg, false);
        pref_set_bool(&ch->pcdata->preferences, PREF_CAT_GMCP, arg, !current);
        save_char_obj(ch);

        sprintf(buf, "%s is now %s{x.\n\r", arg, !current ? "{GON" : "{ROFF");
        send_to_char(buf, ch);
        return;
    }
```

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean build.

- [ ] **Step 5: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(prefs): wire GMCP defaults into login and add to prefs command

- Call pref_apply_gmcp_defaults() after pref_apply_to_character() at login
- Call pref_apply_gmcp_defaults() after character creation
- Add GMCP section to prefs command display (show_gmcp_settings)
- Add gmcp_* toggle handling in do_prefs command

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Channel.Message builder + unit tests

Add the pure JSON builder for Channel.Message and unit tests, following the established pattern from Phase 1-3 builders.

**Files:**
- Modify: `gmcp_sentience.h` (add input struct + builder declaration)
- Modify: `gmcp_sentience.c` (add builder function, make `sentience_send_package` non-static, register in capabilities)
- Modify: `tests/unit/gmcp_sentience_tests.c` (add test handler)
- Modify: `tests/data/unit/gmcp_sentience_unit_tests.json` (add test scenarios)

- [ ] **Step 1: Add input struct and declarations to header**

In `gmcp_sentience.h`, after the existing input struct declarations:

```c
/* ── Channel.Message ──────────────────────────────────────────────── */

typedef struct {
    const char *channel;      /* Channel ID (e.g. "gossip", "say") */
    const char *sender;       /* Sender name ("" for system messages) */
    const char *text;         /* Message text (color-stripped) */
    long        timestamp;    /* Unix timestamp */
    const char *tell_target;  /* Recipient name (NULL if not directed) */
} sentience_channel_message_input_t;

json_t *sentience_build_channel_message(const sentience_channel_message_input_t *input);
```

Also in `gmcp_sentience.h`, add a declaration for `sentience_send_package`:

```c
void sentience_send_package(descriptor_t *d, const char *package, json_t *json);
```

- [ ] **Step 2: Implement builder in gmcp_sentience.c**

In `gmcp_sentience.c`, after the existing builders (after `sentience_build_room_map`), add:

```c
/* ── Sentience.Channel.Message builder ─────────────────────────── */

json_t *sentience_build_channel_message(const sentience_channel_message_input_t *input)
{
    json_t *obj;

    if (!input || !input->channel || !input->text)
        return NULL;

    obj = json_object();
    json_object_set_new(obj, "_v",        json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "channel",   json_string(input->channel));
    json_object_set_new(obj, "sender",    json_string(input->sender ? input->sender : ""));
    json_object_set_new(obj, "text",      json_string(input->text));
    json_object_set_new(obj, "timestamp", json_integer(input->timestamp));

    if (input->tell_target)
        json_object_set_new(obj, "tell_target", json_string(input->tell_target));

    return obj;
}
```

- [ ] **Step 3: Make sentience_send_package non-static**

In `gmcp_sentience.c`, change the function signature from:

```c
static void sentience_send_package(descriptor_t *d, const char *package, json_t *json)
```

to:

```c
void sentience_send_package(descriptor_t *d, const char *package, json_t *json)
```

- [ ] **Step 4: Register packages in capabilities**

In `gmcp_sentience.c`, in `sentience_build_client_ready_capabilities_json()`, add:

```c
    json_array_append_new(packages, json_string("Sentience.Channel.Message"));
    json_array_append_new(packages, json_string("Sentience.Client.Preferences"));
```

Add these after the existing `Sentience.Room.Map` line and before the `Sentience.Link` line.

- [ ] **Step 5: Add test handler**

In `tests/unit/gmcp_sentience_tests.c`, add a handler for Channel.Message tests. Pattern it after the existing builder test handlers. The test_type prefix should be `gmcp_chan_`:

```c
static bool handle_channel_message_test(json_t *test_obj)
{
    const char *test_id = json_string_value(json_object_get(test_obj, "test_id"));
    json_t *input_json  = json_object_get(test_obj, "input");
    json_t *expected     = json_object_get(test_obj, "expected");

    sentience_channel_message_input_t input = {
        .channel     = json_string_value(json_object_get(input_json, "channel")),
        .sender      = json_string_value(json_object_get(input_json, "sender")),
        .text        = json_string_value(json_object_get(input_json, "text")),
        .timestamp   = (long)json_integer_value(json_object_get(input_json, "timestamp")),
        .tell_target = json_is_null(json_object_get(input_json, "tell_target"))
                     ? NULL
                     : json_string_value(json_object_get(input_json, "tell_target")),
    };

    json_t *result = sentience_build_channel_message(&input);
    TEST_ASSERT_NOT_NULL(result, test_id, "builder returned NULL");

    bool pass = json_equal(result, expected);
    if (!pass) {
        char *r = json_dumps(result, JSON_COMPACT | JSON_SORT_KEYS);
        char *e = json_dumps(expected, JSON_COMPACT | JSON_SORT_KEYS);
        TEST_FAIL(test_id, "JSON mismatch:\n  got:    %s\n  expect: %s", r, e);
        free(r); free(e);
    }

    json_decref(result);
    return pass;
}
```

Register it in the test dispatcher (the `test_type` → handler mapping):

```c
    else if (!str_prefix("gmcp_chan_", test_type))
        return handle_channel_message_test(test_obj);
```

- [ ] **Step 6: Add test scenarios to JSON**

In `tests/data/unit/gmcp_sentience_unit_tests.json`, add three test objects:

```json
    {
      "test_id": "gmcp_chan_broadcast",
      "test_type": "gmcp_chan_broadcast",
      "description": "Channel.Message: broadcast channel (gossip)",
      "input": {
        "channel": "gossip",
        "sender": "Tieryo",
        "text": "Hello everyone!",
        "timestamp": 1711353600,
        "tell_target": null
      },
      "expected": {
        "_v": 1,
        "channel": "gossip",
        "sender": "Tieryo",
        "text": "Hello everyone!",
        "timestamp": 1711353600
      }
    },
    {
      "test_id": "gmcp_chan_directed",
      "test_type": "gmcp_chan_directed",
      "description": "Channel.Message: directed channel (tell) with tell_target",
      "input": {
        "channel": "tell",
        "sender": "Tieryo",
        "text": "Hey there!",
        "timestamp": 1711353600,
        "tell_target": "Zorin"
      },
      "expected": {
        "_v": 1,
        "channel": "tell",
        "sender": "Tieryo",
        "text": "Hey there!",
        "timestamp": 1711353600,
        "tell_target": "Zorin"
      }
    },
    {
      "test_id": "gmcp_chan_system",
      "test_type": "gmcp_chan_system",
      "description": "Channel.Message: system message (empty sender)",
      "input": {
        "channel": "announce",
        "sender": "",
        "text": "Server restart in 5 minutes.",
        "timestamp": 1711353600,
        "tell_target": null
      },
      "expected": {
        "_v": 1,
        "channel": "announce",
        "sender": "",
        "text": "Server restart in 5 minutes.",
        "timestamp": 1711353600
      }
    }
```

Also update the capabilities test's expected packages array to include `"Sentience.Channel.Message"` and `"Sentience.Client.Preferences"`.

- [ ] **Step 7: Build and run tests**

```bash
cd /sentience/src && ./build tests && ./install
cd /sentience && ./sent -test:gmcp
```

Expected: 31+ GMCP tests pass (28 existing + 3 new Channel.Message). The capabilities test should also pass with the updated expected array.

- [ ] **Step 8: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(gmcp): add Sentience.Channel.Message builder with unit tests

- Add sentience_channel_message_input_t struct and builder
- Make sentience_send_package() non-static for use by channel_gmcp.c
- Register Channel.Message and Client.Preferences in capabilities
- Add 3 unit tests: broadcast, directed (tell), system message

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Modernize channel_gmcp.c to use builders and preference filtering

Replace the raw sprintf JSON construction with the Jansson builder and add preference-based filtering.

**Files:**
- Modify: `channels/channel_gmcp.c`

- [ ] **Step 1: Add includes**

At the top of `channels/channel_gmcp.c`, add:

```c
#include <jansson.h>
#include "../gmcp_sentience.h"
#include "../account/preferences.h"
```

- [ ] **Step 2: Rewrite channel_gmcp_broadcast()**

Replace the existing `channel_gmcp_broadcast()` function body. The function signature stays the same. Remove the manual JSON construction (`json_escape_str` calls, `snprintf` of json_body) and replace with:

```c
void channel_gmcp_broadcast(const CHANNEL_DEF_DATA *def, CHAR_DATA *sender,
                            const char *plain_text, time_t timestamp)
{
    DESCRIPTOR_DATA *d;
    const char *nc;
    char stripped[MSL];

    if (!def || !sender || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (!d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
            continue;
        if (!pref_gmcp_channels(d->character))
            continue;
        if (!pref_check_channel(d->character, def->id))
            continue;
        if (!channel_gmcp_recipient_in_scope(def, sender, d->character))
            continue;

        sentience_channel_message_input_t input = {
            .channel     = def->id,
            .sender      = sender->name,
            .text        = stripped,
            .timestamp   = (long)timestamp,
            .tell_target = NULL,
        };

        sentience_send_package(d, "Sentience.Channel.Message",
            sentience_build_channel_message(&input));
    }
}
```

Key changes:
- Added `bGMCPSupport[GMCP_SUPPORT_SENTIENCE]` check (only Sentience clients)
- Added `pref_gmcp_channels()` check (is GMCP channels enabled for this player)
- Added `pref_check_channel()` check (is this specific channel muted)
- Uses `sentience_build_channel_message()` + `sentience_send_package()` instead of sprintf + SendGMCPRaw
- JSON is built per-recipient (required since `sentience_send_package` takes ownership)

- [ ] **Step 3: Rewrite channel_gmcp_send_directed()**

Replace the existing function body:

```c
void channel_gmcp_send_directed(CHAR_DATA *sender, CHAR_DATA *recipient,
                                const char *channel_id, const char *plain_text,
                                time_t timestamp)
{
    DESCRIPTOR_DATA *d;
    const char *nc;
    char stripped[MSL];

    if (!sender || !recipient || !channel_id || !plain_text)
        return;

    nc = nocolour(plain_text);
    strncpy(stripped, nc ? nc : plain_text, sizeof(stripped) - 1);
    stripped[sizeof(stripped) - 1] = '\0';

    for (d = descriptor_list; d != NULL; d = d->next) {
        if (!d->character)
            continue;
        if (d->character != sender && d->character != recipient)
            continue;
        if (!d->pProtocol || !d->pProtocol->bGMCP)
            continue;
        if (!d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
            continue;
        if (!pref_gmcp_channels(d->character))
            continue;

        sentience_channel_message_input_t input = {
            .channel     = channel_id,
            .sender      = sender->name,
            .text        = stripped,
            .timestamp   = (long)timestamp,
            .tell_target = recipient->name,
        };

        sentience_send_package(d, "Sentience.Channel.Message",
            sentience_build_channel_message(&input));
    }
}
```

- [ ] **Step 4: Remove dead code**

The `json_escape_str()` static function at the top of `channel_gmcp.c` is no longer used by either function. Remove it. Verify no other function in the file calls it.

- [ ] **Step 5: Build and test**

```bash
cd /sentience/src && ./build tests && ./install
cd /sentience && ./sent -test:gmcp
```

Expected: 31+ GMCP tests pass. No functional change to test results since the builder is tested independently.

- [ ] **Step 6: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(gmcp): modernize channel_gmcp.c to use Jansson builders and preference filtering

- Replace sprintf JSON with sentience_build_channel_message() + sentience_send_package()
- Add pref_gmcp_channels() check (skip if GMCP channels disabled)
- Add pref_check_channel() check (skip if channel is muted)
- Add bGMCPSupport[GMCP_SUPPORT_SENTIENCE] check (Sentience clients only)
- Remove dead json_escape_str() function

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Add inline channel suppression and preference-driven minimap suppression

Wire the suppression preferences into the channel delivery and minimap display paths.

**Files:**
- Modify: `channels/channel_service.c` (~line 1272, `channel_can_deliver_to_descriptor`)
- Modify: `act_info.c` (lines ~2601 and ~2628, minimap suppression)

- [ ] **Step 1: Add channel inline suppression**

In `channels/channel_service.c`, at the top, add:

```c
#include "../account/preferences.h"
```

In `channel_can_deliver_to_descriptor()` (~line 1272), after the existing filtering checks (quiet, ignore, wizi) but before `return true`, add:

```c
    /* GMCP inline suppression: if the recipient has both gmcp_channels ON
     * and gmcp_suppress_channels ON, skip inline text delivery — the GMCP
     * path handles it. */
    if (pref_gmcp_channels(*out_victim) && pref_gmcp_suppress_channels(*out_victim))
        return false;
```

Important: verify that `*out_victim` is set by this point (it is — the function sets it earlier during the PLAYING/character checks). The `out_victim` parameter is `CHAR_DATA **out_victim`.

- [ ] **Step 2: Replace hardcoded minimap suppression with preference check**

In `act_info.c`, add include if not already present:

```c
#include "account/preferences.h"
```

At line ~2603 (area minimap), replace:

```c
                !(ch->desc && ch->desc->pProtocol &&
                  ch->desc->pProtocol->bGMCP &&
                  ch->desc->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE]))
```

with:

```c
                !pref_gmcp_suppress_minimap(ch))
```

At line ~2631 (wilderness map), replace:

```c
        !(ch->desc && ch->desc->pProtocol &&
          ch->desc->pProtocol->bGMCP &&
          ch->desc->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])) {
```

with:

```c
        !pref_gmcp_suppress_minimap(ch)) {
```

You can also remove `#include "protocol.h"` from act_info.c if it was only added for the GMCP suppression check (but verify it isn't used elsewhere in the file first — it likely is).

- [ ] **Step 3: Build and test**

```bash
cd /sentience/src && ./build tests && ./install
cd /sentience && ./sent -test:gmcp
```

Expected: 31+ GMCP tests pass.

- [ ] **Step 4: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(gmcp): add preference-driven inline suppression for channels and minimap

- Add gmcp_suppress_channels check in channel_can_deliver_to_descriptor()
- Replace hardcoded GMCP minimap suppression with pref_gmcp_suppress_minimap()
- Suppression only active when both gmcp_channels and gmcp_suppress_* are ON

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 6: Add Sentience.Client.Preferences GMCP package (bidirectional)

Add the server-to-client send on login and the client-to-server handler for preference updates.

**Files:**
- Modify: `protocol.h` (add receive enum entry)
- Modify: `protocol.c` (add to receive table + handler in ParseGMCP)
- Modify: `gmcp_sentience.c` (add `sentience_send_client_preferences()`)
- Modify: `gmcp_sentience.h` (declare send function)

- [ ] **Step 1: Add receive enum entry in protocol.h**

In `protocol.h`, add to the GMCP_RECEIVE enum (before `GMCP_RECEIVE_MAX`):

```c
   GMCP_EXTERNAL_DISCORD_GET,
   GMCP_SENTIENCE_CLIENT_PREFERENCES,
   GMCP_RECEIVE_MAX
```

- [ ] **Step 2: Add to receive table in protocol.c**

In `protocol.c`, add to `GMCPReceiveTable` (before the MAX sentinel):

```c
   { GMCP_SENTIENCE_CLIENT_PREFERENCES,	"Sentience.Client.Preferences"		},

   { GMCP_RECEIVE_MAX,					"",									}
```

- [ ] **Step 3: Add send function in gmcp_sentience.c**

In `gmcp_sentience.c`, add:

```c
/**
 * sentience_send_client_preferences - Send current GMCP prefs to client
 *
 * Sends Sentience.Client.Preferences with the current effective values.
 * Called on login and after a client preference update.
 */
void sentience_send_client_preferences(descriptor_t *d)
{
    json_t *obj;
    CHAR_DATA *ch;

    if (!d || !d->character)
        return;

    ch = d->character;

    obj = json_object();
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "gmcp_channels",
                        json_boolean(pref_gmcp_channels(ch)));
    json_object_set_new(obj, "gmcp_suppress_channels",
                        json_boolean(pref_gmcp_suppress_channels(ch)));
    json_object_set_new(obj, "gmcp_suppress_minimap",
                        json_boolean(pref_gmcp_suppress_minimap(ch)));

    sentience_send_package(d, "Sentience.Client.Preferences", obj);
}
```

Note: Add `#include "account/preferences.h"` to `gmcp_sentience.c` if not already present.

- [ ] **Step 4: Declare in header**

In `gmcp_sentience.h`:

```c
void sentience_send_client_preferences(descriptor_t *d);
```

- [ ] **Step 5: Send on login**

In `gmcp_sentience.c`, in `sentience_gmcp_update()`, find the `SENTIENCE_DIRTY_INITIAL` one-shot block (where `Client.Ready.State` is sent). After that send, add:

```c
        sentience_send_client_preferences(d);
```

This sends preferences once on initial connection, right after capabilities and state.

- [ ] **Step 6: Add handler in ParseGMCP**

In `protocol.c`, in the `ParseGMCP` function's switch statement, add a case for the new receive entry. This should be added before the `default: break;` or after the last existing case.

```c
      case GMCP_SENTIENCE_CLIENT_PREFERENCES:
      {
         /* Client sends partial updates: {"gmcp_channels": true, ...}
          * We validate each key, apply as character override, echo back full state. */
         static const char *valid_keys[] = {
             "gmcp_channels", "gmcp_suppress_channels", "gmcp_suppress_minimap", NULL
         };
         CHAR_DATA *ch = apDescriptor->character;

         if (!ch || IS_NPC(ch) || !ch->pcdata)
             break;

         if (t[1].type == JSMN_OBJECT) {
             for (i = 2; i < tokens; i += 2) {
                 key = PullJSONString(t[i].start, t[i].end, string);

                 /* Validate key is in whitelist */
                 bool valid = false;
                 for (int k = 0; valid_keys[k]; k++) {
                     if (!strcmp(key, valid_keys[k])) {
                         valid = true;
                         break;
                     }
                 }
                 if (!valid)
                     continue;

                 /* Parse boolean value */
                 if (i + 1 < tokens) {
                     char *val_str = PullJSONString(t[i + 1].start, t[i + 1].end, string);
                     bool val = (!strcmp(val_str, "true") || !strcmp(val_str, "1"));
                     pref_set_bool(&ch->pcdata->preferences, PREF_CAT_GMCP,
                                   key, val);
                 }
             }
             save_char_obj(ch);
             sentience_send_client_preferences(apDescriptor);
         }
      }
      break;
```

Note: `protocol.c` needs to include `account/preferences.h` for `pref_set_bool` and `PREF_CAT_GMCP`. Also needs `gmcp_sentience.h` for `sentience_send_client_preferences` — it already includes `gmcp_sentience.h` (line 30). Check if `account/preferences.h` is included; if not, add it.

- [ ] **Step 7: Build and test**

```bash
cd /sentience/src && ./build tests && ./install
cd /sentience && ./sent -test:gmcp
```

Expected: 31+ GMCP tests pass.

- [ ] **Step 8: Commit**

```bash
cd /sentience/src && git add -A && git commit -m "feat(gmcp): add bidirectional Sentience.Client.Preferences package

- Send preferences on initial GMCP connection (after Client.Ready.State)
- Handle incoming Sentience.Client.Preferences for client-driven pref updates
- Validate keys against whitelist, apply as character overrides, echo back full state
- Register in GMCPReceiveTable for incoming message dispatch

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 7: Final validation and push

Run full test suite, verify everything works, and push.

- [ ] **Step 1: Run GMCP tests**

```bash
cd /sentience && ./sent -test:gmcp
```

Expected: 31+ pass (28 existing + 3 Channel.Message).

- [ ] **Step 2: Run slink tests**

```bash
cd /sentience && ./sent -test:slink
```

Expected: 14 pass.

- [ ] **Step 3: Push**

```bash
cd /sentience/src && git push origin tieryo/skill_cleanup
```

- [ ] **Step 4: Verify push**

```bash
cd /sentience/src && git --no-pager log --oneline origin/tieryo/skill_cleanup..HEAD
```

Expected: 0 commits (all pushed).
