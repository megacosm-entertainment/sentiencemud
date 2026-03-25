# Sentience.Client.Layout & Sentience.Auth.QRCode Design Spec

> Date: 2026-03-25
> Status: Draft
> Scope: Two new GMCP packages for the Sentience web client

---

## 1. Sentience.Client.Layout

### Purpose

Bidirectional GMCP package for persisting FlexLayout panel configurations.
Enables the web client to save, load, list, and delete named panel layouts
that survive logout/reconnect.

### Storage

Layouts are stored in account JSON under `"web_client_layouts"` (an object
keyed by layout name). A per-character override is supported: if the character
JSON contains `"web_client_layouts"`, those entries take precedence over account
entries with the same name.

An `"active_layout"` string field (stored alongside `"web_client_layouts"`)
tracks which layout was last loaded, so it can be auto-restored on login.

**Constraints:**
- Maximum 16 KB per layout blob
- Maximum 5 named layouts per scope (account or character)
- Layout names: 1–32 characters, alphanumeric + underscore + hyphen only

> V1 note: All save/delete/load actions operate on **character scope** only.
> Account-level layouts are reserved for future admin/migration tooling.

### Protocol

All messages use the package name `Sentience.Client.Layout`.

#### Client → Server

**Save a layout:**
```json
{"action": "save", "name": "default", "layout": { /* opaque FlexLayout JSON */ }}
```
Server validates: name format, JSON object check, size cap, layout count cap.
Persists to character scope. Echoes `saved` on success or `error` on failure.
Saving also sets the layout as the active layout.

**Load a layout:**
```json
{"action": "load", "name": "combat"}
```
Server looks up the named layout (character → account inheritance), sets it
as the active layout, and sends a `restore` response.

**Delete a layout:**
```json
{"action": "delete", "name": "combat"}
```
Server removes the named layout from character scope. Echoes `deleted` on
success. If the deleted layout was active, `active_layout` is cleared (next
login will get no auto-restore).

**List layouts:**
```json
{"action": "list"}
```
Server returns a list of all available layout names (merged from character
and account scopes) plus which is active.

#### Server → Client

**Restore (on login or load):**
```json
{"action": "restore", "name": "default", "layout": { /* FlexLayout JSON */ }, "_v": 1}
```
Sent automatically on login if an active layout exists. Also sent in response
to a `load` action.

**List response:**
```json
{"action": "list", "layouts": ["default", "combat", "building"], "active": "default", "_v": 1}
```

**Save acknowledgment:**
```json
{"action": "saved", "name": "default", "_v": 1}
```

**Delete acknowledgment:**
```json
{"action": "deleted", "name": "combat", "_v": 1}
```

**Error:**
```json
{"action": "error", "reason": "<code>", "_v": 1}
```

Error codes:
- `"size_limit_exceeded"` — layout blob exceeds 16 KB
- `"max_layouts_reached"` — already at 5 named layouts
- `"invalid_name"` — name fails format validation
- `"not_found"` — requested layout name does not exist
- `"invalid_action"` — unknown action string
- `"invalid_payload"` — layout is not a JSON object

### Server Implementation

**Receive handler:** Add `GMCP_SENTIENCE_CLIENT_LAYOUT` to the `GMCP_RECEIVE`
enum and `GMCPReceiveTable`. Handler in `ParseGMCP` dispatches to
`sentience_handle_client_layout()` in `gmcp_sentience.c`.

**Login send:** In `sentience_gmcp_update()`, on first update (when
`initialized` is false), if the character has an active layout and the
connection is WebSocket, send a `restore` message.

**Persistence:** Layout read/write functions in the character and account
save/load code. Layout blobs are stored as-is (opaque JSON) — the server
does not interpret FlexLayout structure.

**Capabilities:** Register `Sentience.Client.Layout` in the capabilities
package list.

---

## 2. Sentience.Auth.QRCode

### Purpose

Server → Client GMCP package that delivers a QR code image during MFA TOTP
setup for WebSocket clients. Replaces the ASCII terminal QR art with a
proper PNG image that the web client can display natively.

### Protocol

**Direction:** Server → Client only (no receive handler needed)

**Package name:** `Sentience.Auth.QRCode`

```json
{
  "_v": 1,
  "purpose": "totp_setup",
  "image": "data:image/png;base64,iVBORw0KGgo...",
  "uri": "otpauth://totp/Sentience:Tieryo?secret=JBSWY3DPEHPK3PXP&issuer=Sentience",
  "expires_at": 0
}
```

| Field        | Type   | Description                                          |
|--------------|--------|------------------------------------------------------|
| `_v`         | int    | Package version (1)                                  |
| `purpose`    | string | `"totp_setup"` — extensible for future QR uses       |
| `image`      | string | Data URL ready base64 PNG (`data:image/png;base64,…`)|
| `uri`        | string | `otpauth://` URI for copy/paste or JS QR generation  |
| `expires_at` | int    | Unix timestamp when setup window expires (0 = N/A)   |

### Server Implementation

**New function** `encode_qr_code_as_png_base64()` in `account/otp.c`:
- Takes a `QRcode *` and scale factor
- Uses libpng's `png_set_write_fn()` with a custom write callback that
  appends to a dynamically grown `malloc`'d buffer (no temp files)
- Base64-encodes the PNG buffer (standard table, no line breaks)
- Prepends `"data:image/png;base64,"` prefix
- Returns the complete data URL string (caller frees)

**Integration points** in `account/otp.c`:
- `setup_mfa_for_char()`: after generating QR, check if the descriptor is a
  WebSocket connection (`d->conn && d->conn->type == CONN_TYPE_WEBSOCKET_TLS`)
  — if yes, call `sentience_send_auth_qrcode()` instead of `display_qr_code()`
- `setup_mfa_for_account()`: same WebSocket check
- The nanny.c `display_qr_code()` call sites (lines ~1379, ~5244): same
  check, send GMCP for WebSocket, ASCII for telnet/TLS

**Builder function** `sentience_build_auth_qrcode_json()` in
`gmcp_sentience.c`:
- Takes `purpose`, `image_data_url`, `uri`, `expires_at`
- Returns `json_t *` object

**Send function** `sentience_send_auth_qrcode()` in `gmcp_sentience.c`:
- Takes descriptor, purpose, image data URL, URI, expires_at
- Builds JSON, sends via `sentience_send_package()`, cleans up

**Capabilities:** Register `Sentience.Auth.QRCode` in the capabilities list.
Do NOT add to `GMCPReceiveTable` (server → client only).

### Size Estimate

QR code at scale=6: ~150×150 px → ~67 KB PNG → ~90 KB base64. Well within
a single WebSocket frame. No chunking needed.

### Telnet/TLS Fallback

The existing `display_qr_code()` ASCII art function remains unchanged for
non-WebSocket clients. The conditional is at the call site, not inside
the QR generation.

---

## Shared Implementation Notes

### Build System

Both `CMakeLists.txt` and `Makefile` must be updated if new `.c` files are
added. For this work, no new files are needed — all code goes into existing
files (`gmcp_sentience.c/h`, `protocol.c/h`, `account/otp.c`).

### Testing

- **Client.Layout:** Unit tests for the builder/parser functions (valid save,
  invalid name, size cap exceeded, max layouts). Integration test for the
  receive handler.
- **Auth.QRCode:** Unit test for the builder function. The `encode_qr_code_as_png_base64()`
  function should be tested with a known QR input to verify it produces valid
  base64 PNG output.

### Capabilities Registration

Add both packages to `sentience_build_client_ready_capabilities_json()`:
```
"Sentience.Client.Layout"
"Sentience.Auth.QRCode"
```

### Web Client Documentation

Update `docs/GMCP_WEB_CLIENT_REFERENCE.md` with both new packages after
implementation.
