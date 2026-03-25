# Client.Layout & Auth.QRCode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new GMCP packages — `Sentience.Client.Layout` (bidirectional layout persistence) and `Sentience.Auth.QRCode` (server→client QR code delivery during MFA setup).

**Architecture:** Client.Layout uses jsmn parsing for incoming messages and Jansson for outgoing, storing opaque layout JSON blobs in the character save file via `json_char.c`. Auth.QRCode adapts the existing libpng+libqrencode QR generation to write PNG into a memory buffer, then base64-encodes it via OpenSSL BIO and sends as a GMCP data URL. Both packages register in protocol.h enums, protocol.c receive table (Layout only), and the capabilities list.

**Tech Stack:** C, Jansson (JSON building), jsmn (JSON parsing), libpng, libqrencode, OpenSSL (base64)

**Spec:** `docs/superpowers/specs/2026-03-25-client-layout-auth-qrcode-design.md`

**Existing test baseline:** 29 GMCP test entries (some with multi-scenario sub-tests), 14 slink tests pass

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `protocol.h` | Modify ~line 313 | Add `GMCP_SENTIENCE_CLIENT_LAYOUT` to `GMCP_RECEIVE` enum |
| `protocol.c` | Modify ~lines 3372, 3979 | Add to `GMCPReceiveTable`; add `ParseGMCP` handler |
| `gmcp_sentience.h` | Modify ~end | Add layout data structures, builder/handler declarations |
| `gmcp_sentience.c` | Modify ~lines 240, 490 | Add capabilities, builder, handler, login restore |
| `merc.h` | Modify ~line 5975 | Add `web_client_layouts` + `active_layout` to `pc_data` |
| `io/json/json_char.c` | Modify ~lines 1435, 3567 | Serialize/deserialize layout blobs |
| `account/otp.c` | Modify ~lines 476, 532 | Add `encode_qr_code_as_png_base64()`, wire into MFA setup |
| `nanny.c` | Modify ~lines 1379, 5244 | WebSocket check before `display_qr_code()` calls |
| `mem.c` | Modify ~line 1162 (`free_pcdata`) | Free layout data in pcdata cleanup |
| `tests/unit/gmcp_sentience_tests.c` | Modify | Add test scenario handlers |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Modify | Add test case definitions |
| `docs/GMCP_WEB_CLIENT_REFERENCE.md` | Modify | Document both new packages |

---

## Task 1: Layout Data Structures & Character Persistence

**Files:**
- Modify: `merc.h:5972-5976`
- Modify: `gmcp_sentience.h` (end of file)
- Modify: `io/json/json_char.c:1425-1435` (save) and `~3560-3570` (load)
- Modify: `mem.c:1162` (`free_pcdata`)

This task adds the storage mechanism. Layout blobs are opaque `json_t *` objects stored in a simple linked list on pcdata.

- [ ] **Step 1: Define layout storage struct in `gmcp_sentience.h`**

Add before the `#endif`:

```c
/* ----- Client.Layout storage ----- */
#define LAYOUT_NAME_MAX   32
#define LAYOUT_MAX_COUNT  5
#define LAYOUT_MAX_SIZE   (16 * 1024)  /* 16 KB per layout blob */

typedef struct web_client_layout {
    char name[LAYOUT_NAME_MAX + 1];
    json_t *layout;                      /* Opaque FlexLayout model JSON */
    struct web_client_layout *next;
} web_client_layout_t;

/* Layout storage helpers (gmcp_sentience.c) */
web_client_layout_t *layout_find(web_client_layout_t *list, const char *name);
int                  layout_count(web_client_layout_t *list);
void                 layout_free_all(web_client_layout_t **list);
```

- [ ] **Step 2: Add layout fields to `pc_data` in `merc.h`**

After line 5972 (`PREF_ENTRY *preferences;`), before `trait_values`:

```c
    /* Web client layout persistence */
    web_client_layout_t *web_client_layouts;  /* Linked list of named layouts */
    char active_layout[33];                   /* Name of last-used layout */
```

Include `gmcp_sentience.h` forward declaration or use `struct web_client_layout *` — the recommended approach is to forward-declare the struct in `merc.h` since this avoids circular includes:

```c
/* Forward declaration (near other forward decls at top of merc.h) */
typedef struct web_client_layout web_client_layout_t;
```

Then in the `pc_data` struct, use `web_client_layout_t *` directly. The full struct definition remains in `gmcp_sentience.h`.

- [ ] **Step 3: Implement storage helpers in `gmcp_sentience.c`**

Add after the existing builder functions, before `sentience_gmcp_update()`:

```c
/* ----- Client.Layout storage helpers ----- */

web_client_layout_t *layout_find(web_client_layout_t *list, const char *name)
{
    for (web_client_layout_t *l = list; l; l = l->next)
        if (!str_cmp(l->name, name))
            return l;
    return NULL;
}

int layout_count(web_client_layout_t *list)
{
    int n = 0;
    for (web_client_layout_t *l = list; l; l = l->next)
        n++;
    return n;
}

void layout_free_all(web_client_layout_t **list)
{
    web_client_layout_t *l = *list, *next;
    while (l) {
        next = l->next;
        if (l->layout) json_decref(l->layout);
        free(l);
        l = next;
    }
    *list = NULL;
}
```

- [ ] **Step 4: Add layout serialization in `io/json/json_char.c`**

In `char_to_json()` (or the "basic" serializer), after the `preference_overrides` block (~line 1435):

```c
        /* Web client layouts */
        if (ch->pcdata->web_client_layouts) {
            json_t *layouts_obj = json_object();
            web_client_layout_t *l;
            for (l = ch->pcdata->web_client_layouts; l; l = l->next) {
                if (l->layout)
                    json_object_set(layouts_obj, l->name, l->layout); /* borrowed ref */
            }
            json_object_set_new(basic, "web_client_layouts", layouts_obj);
        }
        if (ch->pcdata->active_layout[0])
            json_object_set_new(basic, "active_layout",
                                json_string(ch->pcdata->active_layout));
```

Add deserialization in the load function, near where `preference_overrides` is read (~line 3567):

```c
        /* Web client layouts */
        {
            json_t *layouts_obj = json_object_get(basic, "web_client_layouts");
            if (layouts_obj && json_is_object(layouts_obj)) {
                const char *lname;
                json_t *lval;
                json_object_foreach(layouts_obj, lname, lval) {
                    if (!json_is_object(lval)) continue;
                    if (layout_count(ch->pcdata->web_client_layouts) >= LAYOUT_MAX_COUNT) break;
                    web_client_layout_t *entry = calloc(1, sizeof(*entry));
                    snprintf(entry->name, sizeof(entry->name), "%s", lname);
                    entry->layout = json_incref(lval);
                    entry->next = ch->pcdata->web_client_layouts;
                    ch->pcdata->web_client_layouts = entry;
                }
            }
            json_t *active = json_object_get(basic, "active_layout");
            if (active && json_is_string(active))
                snprintf(ch->pcdata->active_layout, sizeof(ch->pcdata->active_layout),
                         "%s", json_string_value(active));
        }
```

- [ ] **Step 5: Free layouts in character cleanup**

In `mem.c`, find `free_pcdata()` at line 1162. Add before the end of the function:

```c
    if (pcdata->web_client_layouts)
        layout_free_all(&pcdata->web_client_layouts);
```

- [ ] **Step 6: Build and verify no regressions**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

Expected: 31 pass, 0 new failures.

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(gmcp): add Client.Layout data structures and character persistence"
```

---

## Task 2: Client.Layout Protocol Registration & Receive Handler

**Files:**
- Modify: `protocol.h:305-317` (GMCP_RECEIVE enum)
- Modify: `protocol.c:3364-3374` (GMCPReceiveTable), `~3979` (ParseGMCP handler)
- Modify: `gmcp_sentience.c` (handler function)
- Modify: `gmcp_sentience.h` (handler declaration)

- [ ] **Step 1: Add enum value to `protocol.h`**

In the `GMCP_RECEIVE` enum (~line 313), before `GMCP_RECEIVE_MAX`:

```c
   GMCP_SENTIENCE_CLIENT_LAYOUT,
```

So the enum becomes:
```c
   GMCP_SENTIENCE_CLIENT_PREFERENCES,
   GMCP_SENTIENCE_CLIENT_LAYOUT,
   GMCP_RECEIVE_MAX
```

- [ ] **Step 2: Add to GMCPReceiveTable in `protocol.c`**

After the `GMCP_SENTIENCE_CLIENT_PREFERENCES` entry (~line 3372):

```c
   { GMCP_SENTIENCE_CLIENT_LAYOUT,      "Sentience.Client.Layout"           },
```

- [ ] **Step 3: Add helper and response functions in `gmcp_sentience.c`**

These static functions **must appear before** `sentience_handle_client_layout()` in the file (C requires static functions to be defined or forward-declared before use).

```c
static bool layout_name_is_valid(const char *name)
{
    int len;
    if (IS_NULLSTR(name)) return false;
    len = strlen(name);
    if (len < 1 || len > LAYOUT_NAME_MAX) return false;
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if (!isalnum(c) && c != '_' && c != '-') return false;
    }
    return true;
}

static void sentience_send_layout_error(descriptor_t *d, const char *reason)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("error"));
    json_object_set_new(obj, "reason", json_string(reason));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_saved(descriptor_t *d, const char *name)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("saved"));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_deleted(descriptor_t *d, const char *name)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("deleted"));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

void sentience_send_layout_restore(descriptor_t *d, const char *name, json_t *layout)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "action", json_string("restore"));
    json_object_set_new(obj, "name", json_string(name));
    json_object_set(obj, "layout", layout);  /* borrowed ref */
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}

static void sentience_send_layout_list(descriptor_t *d, CHAR_DATA *ch)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    web_client_layout_t *l;

    for (l = ch->pcdata->web_client_layouts; l; l = l->next)
        json_array_append_new(arr, json_string(l->name));

    json_object_set_new(obj, "action", json_string("list"));
    json_object_set_new(obj, "layouts", arr);
    json_object_set_new(obj, "active",
        ch->pcdata->active_layout[0] ? json_string(ch->pcdata->active_layout)
                                      : json_null());
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    sentience_send_package(d, "Sentience.Client.Layout", obj);
}
```

- [ ] **Step 4: Implement handler function in `gmcp_sentience.c`**

Add `sentience_handle_client_layout()` **after** the static helper functions from Step 3:

```c
/*
 * sentience_handle_client_layout — process incoming Client.Layout messages.
 *
 * Actions: save, load, delete, list
 * Uses Jansson json_loads() to parse the full JSON body from ParseGMCP.
 */
void sentience_handle_client_layout(descriptor_t *d, const char *json_str)
{
    json_error_t err;
    json_t *root, *action_val, *name_val, *layout_val;
    const char *action, *name;
    CHAR_DATA *ch;

    if (!d || !(ch = d->character) || IS_NPC(ch) || !ch->pcdata)
        return;

    root = json_loads(json_str, 0, &err);
    if (!root || !json_is_object(root)) {
        sentience_send_layout_error(d, "invalid_payload");
        if (root) json_decref(root);
        return;
    }

    action_val = json_object_get(root, "action");
    if (!action_val || !json_is_string(action_val)) {
        sentience_send_layout_error(d, "invalid_action");
        json_decref(root);
        return;
    }
    action = json_string_value(action_val);

    if (!strcmp(action, "save")) {
        name_val = json_object_get(root, "name");
        layout_val = json_object_get(root, "layout");

        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        if (!layout_val || !json_is_object(layout_val)) {
            sentience_send_layout_error(d, "invalid_payload");
            json_decref(root);
            return;
        }

        /* Validate name: 1-32 chars, alphanumeric + underscore + hyphen */
        if (!layout_name_is_valid(name)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }

        /* Validate size */
        char *dump = json_dumps(layout_val, JSON_COMPACT);
        if (dump && strlen(dump) > LAYOUT_MAX_SIZE) {
            free(dump);
            sentience_send_layout_error(d, "size_limit_exceeded");
            json_decref(root);
            return;
        }
        if (dump) free(dump);

        /* Check if updating existing or adding new */
        web_client_layout_t *existing = layout_find(ch->pcdata->web_client_layouts, name);
        if (!existing && layout_count(ch->pcdata->web_client_layouts) >= LAYOUT_MAX_COUNT) {
            sentience_send_layout_error(d, "max_layouts_reached");
            json_decref(root);
            return;
        }

        if (existing) {
            if (existing->layout) json_decref(existing->layout);
            existing->layout = json_incref(layout_val);
        } else {
            web_client_layout_t *entry = calloc(1, sizeof(*entry));
            snprintf(entry->name, sizeof(entry->name), "%s", name);
            entry->layout = json_incref(layout_val);
            entry->next = ch->pcdata->web_client_layouts;
            ch->pcdata->web_client_layouts = entry;
        }

        /* Set as active */
        snprintf(ch->pcdata->active_layout, sizeof(ch->pcdata->active_layout),
                 "%s", name);

        save_char_obj(ch);
        sentience_send_layout_saved(d, name);
    }
    else if (!strcmp(action, "load")) {
        name_val = json_object_get(root, "name");
        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        web_client_layout_t *entry = layout_find(ch->pcdata->web_client_layouts, name);
        if (!entry) {
            sentience_send_layout_error(d, "not_found");
            json_decref(root);
            return;
        }

        /* Set as active */
        snprintf(ch->pcdata->active_layout, sizeof(ch->pcdata->active_layout),
                 "%s", name);
        save_char_obj(ch);
        sentience_send_layout_restore(d, entry->name, entry->layout);
    }
    else if (!strcmp(action, "delete")) {
        name_val = json_object_get(root, "name");
        if (!name_val || !json_is_string(name_val)) {
            sentience_send_layout_error(d, "invalid_name");
            json_decref(root);
            return;
        }
        name = json_string_value(name_val);

        web_client_layout_t *prev = NULL, *cur = ch->pcdata->web_client_layouts;
        while (cur) {
            if (!str_cmp(cur->name, name))
                break;
            prev = cur;
            cur = cur->next;
        }
        if (!cur) {
            sentience_send_layout_error(d, "not_found");
            json_decref(root);
            return;
        }

        if (prev) prev->next = cur->next;
        else ch->pcdata->web_client_layouts = cur->next;

        if (cur->layout) json_decref(cur->layout);
        free(cur);

        /* Clear active if deleted layout was active */
        if (!str_cmp(ch->pcdata->active_layout, name))
            ch->pcdata->active_layout[0] = '\0';

        save_char_obj(ch);
        sentience_send_layout_deleted(d, name);
    }
    else if (!strcmp(action, "list")) {
        sentience_send_layout_list(d, ch);
    }
    else {
        sentience_send_layout_error(d, "invalid_action");
    }

    json_decref(root);
}
```

- [ ] **Step 5: Wire handler into ParseGMCP in `protocol.c`**

After the `GMCP_SENTIENCE_CLIENT_PREFERENCES` case block (~line 4018), add:

```c
      case GMCP_SENTIENCE_CLIENT_LAYOUT:
      {
         if (!apDescriptor->character || IS_NPC(apDescriptor->character))
             break;
         sentience_handle_client_layout(apDescriptor, string);
      }
      break;
```

Note: The Layout handler uses Jansson (`json_loads`) to parse the full JSON instead of raw jsmn tokens, because it needs to extract and store an opaque `layout` object. The raw `string` parameter from ParseGMCP contains the JSON body after the package name.

- [ ] **Step 6: Declare handler in `gmcp_sentience.h`**

```c
void sentience_handle_client_layout(descriptor_t *d, const char *json_str);
void sentience_send_layout_restore(descriptor_t *d, const char *name, json_t *layout);
```

- [ ] **Step 7: Build and verify**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

Expected: All existing GMCP tests pass, 0 new failures.

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "feat(gmcp): add Client.Layout receive handler with save/load/delete/list"
```

---

## Task 3: Client.Layout Login Restore & Capabilities

**Files:**
- Modify: `gmcp_sentience.c:~240` (capabilities), `~490` (update/init)

- [ ] **Step 1: Register in capabilities**

In `sentience_build_client_ready_capabilities_json()` (~line 243), after the `Sentience.Client.Preferences` line:

```c
    json_array_append_new(packages, json_string("Sentience.Client.Layout"));
```

- [ ] **Step 2: Add login restore in `sentience_gmcp_update()`**

In the `!cache->initialized` block (the first-update section, ~line 490+), after the `sentience_send_client_preferences()` call, add:

```c
        /* Restore active layout for WebSocket clients */
        if (ch->pcdata && ch->pcdata->active_layout[0]
            && d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS) {
            web_client_layout_t *active = layout_find(
                ch->pcdata->web_client_layouts, ch->pcdata->active_layout);
            if (active && active->layout)
                sentience_send_layout_restore(d, active->name, active->layout);
        }
```

- [ ] **Step 3: Build and verify**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

Expected: All existing GMCP tests pass (capabilities test will need updating — see Task 7).

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat(gmcp): add Client.Layout login restore and capabilities registration"
```

---

## Task 4: Auth.QRCode — In-Memory PNG Base64 Encoding

**Files:**
- Modify: `account/otp.c:~344` (new function after existing `save_qr_code_as_png`)
- Modify: `merc.h` (add declaration, near existing `display_qr_code` declaration)

This task adds the core `encode_qr_code_as_png_base64()` function that converts a QRcode to a `data:image/png;base64,...` string in memory using libpng's custom write callback.

- [ ] **Step 1: Add memory write callback and encode function in `account/otp.c`**

After `save_qr_code_as_png()` (~line 424):

```c
/* ----- In-memory PNG encoding for WebSocket GMCP delivery ----- */

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
} png_mem_buffer_t;

static void png_mem_write_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_mem_buffer_t *buf = (png_mem_buffer_t *)png_get_io_ptr(png_ptr);
    size_t needed = buf->size + length;
    if (needed > buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < needed) new_cap = needed;
        unsigned char *tmp = realloc(buf->data, new_cap);
        if (!tmp) {
            png_error(png_ptr, "png_mem_write_callback: realloc failed");
            return;
        }
        buf->data = tmp;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->size, data, length);
    buf->size += length;
}

static void png_mem_flush_callback(png_structp png_ptr)
{
    (void)png_ptr; /* no-op for memory buffers */
}

/*
 * encode_qr_code_as_png_base64 — render QR code to an in-memory PNG,
 * then base64-encode and return as a data URL string.
 *
 * Returns malloc'd string "data:image/png;base64,..." or NULL on failure.
 * Caller must free() the result.
 */
char *encode_qr_code_as_png_base64(QRcode *qrcode, int scale)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_mem_buffer_t membuf = { NULL, 0, 0 };
    png_bytep *row_pointers = NULL;
    int width, y, x, sy, sx;
    char *base64_str = NULL;
    char *result = NULL;
    static const char prefix[] = "data:image/png;base64,";

    if (!qrcode || scale < 1)
        return NULL;

    width = qrcode->width * scale;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        /* libpng error jump */
        for (y = 0; row_pointers && y < width; y++)
            free(row_pointers[y]);
        free(row_pointers);
        free(membuf.data);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return NULL;
    }

    /* Use memory buffer instead of file */
    membuf.capacity = 4096;
    membuf.data = malloc(membuf.capacity);
    if (!membuf.data) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return NULL;
    }

    png_set_write_fn(png_ptr, &membuf, png_mem_write_callback, png_mem_flush_callback);

    png_set_IHDR(png_ptr, info_ptr, width, width, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    /* Allocate and populate row data */
    row_pointers = malloc(sizeof(png_bytep) * width);
    if (!row_pointers) longjmp(png_jmpbuf(png_ptr), 1);

    memset(row_pointers, 0, sizeof(png_bytep) * width);
    for (y = 0; y < width; y++) {
        row_pointers[y] = malloc(width * 3);
        if (!row_pointers[y]) longjmp(png_jmpbuf(png_ptr), 1);
    }

    for (y = 0; y < qrcode->width; y++) {
        for (x = 0; x < qrcode->width; x++) {
            unsigned char val = (qrcode->data[y * qrcode->width + x] & 1) ? 0 : 255;
            for (sy = 0; sy < scale; sy++) {
                for (sx = 0; sx < scale; sx++) {
                    int ry = y * scale + sy;
                    int rx = x * scale + sx;
                    row_pointers[ry][rx * 3]     = val;
                    row_pointers[ry][rx * 3 + 1] = val;
                    row_pointers[ry][rx * 3 + 2] = val;
                }
            }
        }
    }

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    /* Cleanup libpng resources */
    for (y = 0; y < width; y++) free(row_pointers[y]);
    free(row_pointers);
    row_pointers = NULL;
    png_destroy_write_struct(&png_ptr, &info_ptr);

    /* Base64-encode the PNG buffer using OpenSSL BIO */
    {
        BIO *b64, *bio;
        BUF_MEM *bptr;

        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(bio, membuf.data, (int)membuf.size);
        BIO_flush(bio);
        BIO_get_mem_ptr(bio, &bptr);

        /* Build data URL: prefix + base64 + NUL */
        result = malloc(strlen(prefix) + bptr->length + 1);
        if (result) {
            memcpy(result, prefix, strlen(prefix));
            memcpy(result + strlen(prefix), bptr->data, bptr->length);
            result[strlen(prefix) + bptr->length] = '\0';
        }

        BIO_free_all(bio);
    }

    free(membuf.data);
    return result;
}
```

- [ ] **Step 2: Add declaration in `merc.h`**

Near the existing `display_qr_code` declaration (~line 10908):

```c
char *encode_qr_code_as_png_base64(QRcode *qrcode, int scale);
```

Ensure the necessary includes are available. `account/otp.c` should already include `<png.h>`, `<qrencode.h>`, and OpenSSL headers. Add `#include <openssl/bio.h>` and `#include <openssl/evp.h>` and `#include <openssl/buffer.h>` if not already present.

- [ ] **Step 3: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "feat(otp): add in-memory PNG base64 encoding for QR codes"
```

---

## Task 5: Auth.QRCode Builder, Sender & Capabilities

**Files:**
- Modify: `gmcp_sentience.c:~240` (capabilities), new builder/sender functions
- Modify: `gmcp_sentience.h` (declarations)

- [ ] **Step 1: Add builder function in `gmcp_sentience.c`**

```c
json_t *sentience_build_auth_qrcode_json(const char *purpose, const char *image,
                                          const char *uri, long expires_at)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));
    json_object_set_new(obj, "purpose", json_string(purpose ? purpose : "totp_setup"));
    json_object_set_new(obj, "image", json_string(image ? image : ""));
    json_object_set_new(obj, "uri", json_string(uri ? uri : ""));
    json_object_set_new(obj, "expires_at", json_integer(expires_at));

    return obj;
}

void sentience_send_auth_qrcode(descriptor_t *d, const char *image_data_url,
                                 const char *uri, long expires_at)
{
    json_t *obj = sentience_build_auth_qrcode_json("totp_setup", image_data_url,
                                                    uri, expires_at);
    if (obj)
        sentience_send_package(d, "Sentience.Auth.QRCode", obj);
}
```

- [ ] **Step 2: Add declarations in `gmcp_sentience.h`**

```c
json_t *sentience_build_auth_qrcode_json(const char *purpose, const char *image,
                                          const char *uri, long expires_at);
void sentience_send_auth_qrcode(descriptor_t *d, const char *image_data_url,
                                 const char *uri, long expires_at);
```

- [ ] **Step 3: Register Auth.QRCode in capabilities**

In `sentience_build_client_ready_capabilities_json()`, after the `Client.Layout` line:

```c
    json_array_append_new(packages, json_string("Sentience.Auth.QRCode"));
```

- [ ] **Step 4: Build and verify**

```bash
cd /sentience/src && ./build tests
```

Expected: Clean compile. The capabilities test will now expect 15 packages (was 13).

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat(gmcp): add Auth.QRCode builder, sender, and capabilities registration"
```

---

## Task 6: Wire Auth.QRCode into MFA Setup Flow

**Files:**
- Modify: `account/otp.c:~476` (`setup_mfa_for_char`), `~532` (`setup_mfa_for_account`)
- Modify: `nanny.c:~1379`, `~5244` (direct `display_qr_code` calls)

At each call site, check if the descriptor is WebSocket. If yes, generate the PNG base64 and send via GMCP. If no, fall through to the existing ASCII QR display.

- [ ] **Step 1: Modify `setup_mfa_for_char()` in `account/otp.c`**

Replace the `display_qr_code(ch->desc, qr_url);` call (~line 476) with:

```c
        if (ch->desc->conn && ch->desc->conn->type == CONN_TYPE_WEBSOCKET_TLS) {
            QRcode *qr = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (qr) {
                char *data_url = encode_qr_code_as_png_base64(qr, 6);
                if (data_url) {
                    sentience_send_auth_qrcode(ch->desc, data_url, qr_url, 0);
                    free(data_url);
                }
                QRcode_free(qr);
            }
        } else {
            display_qr_code(ch->desc, qr_url);
        }
```

- [ ] **Step 2: Modify `setup_mfa_for_account()` in `account/otp.c`**

Replace the `display_qr_code(d, qr_url);` call (~line 532) with the same pattern:

```c
        if (d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS) {
            QRcode *qr = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (qr) {
                char *data_url = encode_qr_code_as_png_base64(qr, 6);
                if (data_url) {
                    sentience_send_auth_qrcode(d, data_url, qr_url, 0);
                    free(data_url);
                }
                QRcode_free(qr);
            }
        } else {
            display_qr_code(d, qr_url);
        }
```

- [ ] **Step 3: Modify nanny.c call site ~line 1379**

Replace `display_qr_code(d, qr_url);` with the same WebSocket check pattern (same as Step 2 — descriptor is `d`).

- [ ] **Step 4: Modify nanny.c call site ~line 5244**

Replace `display_qr_code(d, qr_url);` with the same WebSocket check pattern (descriptor is `d` from `ch->desc`; confirm the actual variable name at that call site).

- [ ] **Step 5: Add required includes**

Ensure `account/otp.c` has:
```c
#include "connection.h"        /* CONN_TYPE_WEBSOCKET_TLS */
#include "gmcp_sentience.h"    /* sentience_send_auth_qrcode */
```

And `nanny.c` has (if not already present):
```c
#include "gmcp_sentience.h"
```

- [ ] **Step 6: Build and verify**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

Expected: All existing GMCP tests pass (we haven't updated the capabilities test count yet).

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat(gmcp): wire Auth.QRCode into MFA setup for WebSocket clients"
```

---

## Task 7: Unit Tests (Client.Layout + Auth.QRCode + Capabilities)

**Files:**
- Modify: `tests/unit/gmcp_sentience_tests.c`
- Modify: `tests/data/unit/gmcp_sentience_unit_tests.json`

This task adds tests for:
- Layout name validation (valid names, invalid names, edge cases)
- Layout storage helpers (find, count, free)
- Auth.QRCode builder (valid output structure)
- Updated capabilities expected count (15 packages)

- [ ] **Step 1: Add forward declarations in `gmcp_sentience_tests.c`**

After the existing forward declarations (~line 23):

```c
static test_result_t run_gmcp_layout_name_validation_scenario(json_t *tc);
static test_result_t run_gmcp_layout_storage_scenario(json_t *tc);
static test_result_t run_gmcp_auth_qrcode_scenario(json_t *tc);
```

- [ ] **Step 2: Add layout name validation scenario runner**

Since `layout_name_is_valid()` is static, add a test-visible wrapper in `gmcp_sentience.c`:

```c
#ifdef BUILD_TESTS
bool test_layout_name_is_valid(const char *name)
{
    return layout_name_is_valid(name);
}
#endif
```

And declare in `gmcp_sentience.h`:

```c
#ifdef BUILD_TESTS
bool test_layout_name_is_valid(const char *name);
#endif
```

Then the scenario runner (note: the framework dispatcher iterates `test_cases` and passes each entry as `tc`, so read `name`/`valid` directly from `tc`):

```c
/* --- Layout name validation scenario --- */

static test_result_t run_gmcp_layout_name_validation_scenario(json_t *tc)
{
    const char *name = test_json_get_string(tc, "name");
    bool expected_valid = json_is_true(json_object_get(tc, "valid"));
    bool actual = test_layout_name_is_valid(name);

    if (actual != expected_valid) {
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
            "layout_name_is_valid('%s') = %s, expected %s",
            name ? name : "(null)",
            actual ? "true" : "false",
            expected_valid ? "true" : "false");
        return TEST_FAIL;
    }
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Add layout storage helpers scenario runner**

```c
/* --- Layout storage helpers scenario --- */

static test_result_t run_gmcp_layout_storage_scenario(json_t *tc)
{
    /* Test layout_find, layout_count, layout_free_all */
    web_client_layout_t *list = NULL;

    /* Initially empty */
    TEST_ASSERT_INT_EQ(0, layout_count(list));
    TEST_ASSERT_NULL(layout_find(list, "default"));

    /* Add one entry */
    web_client_layout_t *e1 = calloc(1, sizeof(*e1));
    snprintf(e1->name, sizeof(e1->name), "default");
    e1->layout = json_object();
    json_object_set_new(e1->layout, "test", json_true());
    e1->next = list;
    list = e1;

    TEST_ASSERT_INT_EQ(1, layout_count(list));
    TEST_ASSERT_NOT_NULL(layout_find(list, "default"));
    TEST_ASSERT_NULL(layout_find(list, "other"));

    /* Add second entry */
    web_client_layout_t *e2 = calloc(1, sizeof(*e2));
    snprintf(e2->name, sizeof(e2->name), "compact");
    e2->layout = json_object();
    e2->next = list;
    list = e2;

    TEST_ASSERT_INT_EQ(2, layout_count(list));
    TEST_ASSERT_NOT_NULL(layout_find(list, "compact"));
    TEST_ASSERT_NOT_NULL(layout_find(list, "default"));

    /* Free all */
    layout_free_all(&list);
    TEST_ASSERT_NULL(list);
    TEST_ASSERT_INT_EQ(0, layout_count(list));

    return TEST_SUCCESS;
}
```

- [ ] **Step 4: Add Auth.QRCode builder scenario runner**

```c
static test_result_t run_gmcp_auth_qrcode_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    const char *purpose = test_json_get_string(params, "purpose");
    const char *image = test_json_get_string(params, "image");
    const char *uri = test_json_get_string(params, "uri");
    long expires_at = json_integer_value(json_object_get(params, "expires_at"));

    json_t *result = sentience_build_auth_qrcode_json(purpose, image, uri, expires_at);
    if (!result) return TEST_FAIL;

    test_result_t status = json_equal(result, expected) ? TEST_SUCCESS : TEST_FAIL;
    if (status != TEST_SUCCESS) {
        char *exp_str = json_dumps(expected, JSON_COMPACT | JSON_SORT_KEYS);
        char *got_str = json_dumps(result, JSON_COMPACT | JSON_SORT_KEYS);
        log_message_f(LOG_LEVEL_ERROR, LOG_UNIT_TESTS,
                     "QRCode mismatch. Expected: %s Got: %s", exp_str, got_str);
        free(exp_str);
        free(got_str);
    }

    json_decref(result);
    return status;
}
```

- [ ] **Step 5: Add dispatch cases in `run_gmcp_sentience_test_case()`**

After the `build_channel_message` case (~line 799):

```c
        } else if (strcmp(func_name, "layout_name_validation") == 0) {
            result = run_gmcp_layout_name_validation_scenario(tc);
        } else if (strcmp(func_name, "layout_storage_helpers") == 0) {
            result = run_gmcp_layout_storage_scenario(tc);
        } else if (strcmp(func_name, "build_auth_qrcode") == 0) {
            result = run_gmcp_auth_qrcode_scenario(tc);
```

- [ ] **Step 6: Add test data in `gmcp_sentience_unit_tests.json`**

Append before the closing `]`:

```json
,
{
  "test_type": "gmcp_sentience_",
  "name": "gmcp_layout_name_validation",
  "description": "Validates layout name rules (1-32 chars, alphanumeric + underscore + hyphen)",
  "input": {
    "function": "layout_name_validation",
    "test_cases": [
      {"name": "default", "valid": true},
      {"name": "my-layout", "valid": true},
      {"name": "compact_v2", "valid": true},
      {"name": "A", "valid": true},
      {"name": "a-b_c-D_1-2-3", "valid": true},
      {"name": "", "valid": false},
      {"name": "has space", "valid": false},
      {"name": "has.dot", "valid": false},
      {"name": "has/slash", "valid": false},
      {"name": "abcdefghijklmnopqrstuvwxyz1234567", "valid": false}
    ]
  },
  "expected_result": "pass"
},
{
  "test_type": "gmcp_sentience_",
  "name": "gmcp_layout_storage_helpers",
  "description": "Tests layout_find, layout_count, and layout_free_all",
  "input": {
    "function": "layout_storage_helpers",
    "test_cases": [{}]
  },
  "expected_result": "pass"
},
{
  "test_type": "gmcp_sentience_",
  "name": "gmcp_build_auth_qrcode_totp",
  "description": "Builds Sentience.Auth.QRCode JSON for TOTP setup",
  "input": {
    "function": "build_auth_qrcode",
    "test_cases": [
      {
        "scenario": "totp_setup",
        "params": {
          "purpose": "totp_setup",
          "image": "data:image/png;base64,dGVzdA==",
          "uri": "otpauth://totp/Sentience:Tieryo?secret=ABC&issuer=Sentience",
          "expires_at": 0
        },
        "expected": {
          "_v": 1,
          "purpose": "totp_setup",
          "image": "data:image/png;base64,dGVzdA==",
          "uri": "otpauth://totp/Sentience:Tieryo?secret=ABC&issuer=Sentience",
          "expires_at": 0
        }
      }
    ]
  },
  "expected_result": "pass"
}
```

- [ ] **Step 7: Update capabilities test expected values**

In `gmcp_sentience_unit_tests.json`, find the `gmcp_build_client_ready_capabilities` test entry. Update its `expected.packages` array to include the 2 new packages (total 15):

Add after `"Sentience.Client.Preferences"`:
```json
"Sentience.Client.Layout",
"Sentience.Auth.QRCode",
```

- [ ] **Step 8: Build and run tests**

```bash
cd /sentience/src && ./build tests
cd /sentience && ./sent -test:gmcp
```

Expected: All previous GMCP tests pass + 3 new tests (layout name validation, layout storage helpers, Auth.QRCode builder). The capabilities test should pass with the updated expected array.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "test(gmcp): add Client.Layout and Auth.QRCode unit tests, update capabilities"
```

---

## Task 8: Update Web Client Documentation

**Files:**
- Modify: `docs/GMCP_WEB_CLIENT_REFERENCE.md`

- [ ] **Step 1: Add Client.Layout section**

In the Package Reference section (after `Sentience.Client.Preferences`), add the full `Sentience.Client.Layout` documentation with:
- All action types (save, load, delete, list)
- Client → Server and Server → Client message formats
- Error codes
- Constraints (name rules, size limit, max count)
- Implementation notes (debounce is client-side, explicit save)

- [ ] **Step 2: Add Auth.QRCode section**

After `Sentience.Auth.Resume`, add the `Sentience.Auth.QRCode` section with:
- When it's sent (MFA TOTP setup only)
- JSON structure with all fields
- Client implementation: `<img src={data.image}>` for rendering
- URI field for copy/paste fallback
- Note that it only fires for WebSocket clients

- [ ] **Step 3: Update Package Summary table**

Add both new packages to the appendix summary table.

- [ ] **Step 4: Update negotiation flow diagram**

The capabilities package now lists 15 packages (was 13). Update the comment if needed.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "docs: add Client.Layout and Auth.QRCode to web client reference"
```

---

## Task Summary

| Task | Description | Dependencies |
|------|-------------|--------------|
| 1 | Layout data structures & character persistence | None |
| 2 | Client.Layout protocol registration & receive handler | Task 1 |
| 3 | Client.Layout login restore & capabilities | Task 2 |
| 4 | Auth.QRCode in-memory PNG base64 encoding | None |
| 5 | Auth.QRCode builder, sender & capabilities | Task 4 |
| 6 | Wire Auth.QRCode into MFA setup flow | Task 5 |
| 7 | Unit tests (Layout validation + storage + QRCode builder + capabilities) | Tasks 3, 5 |
| 8 | Update web client documentation | Tasks 3, 6, 7 |

Tasks 1-3 (Layout) and Tasks 4-6 (QRCode) are independent tracks that can proceed in parallel if desired. Task 7 depends on both tracks. Task 8 is final.
