# Protocol Layer Integration Design

## Overview

This document describes how to integrate the new protocol abstraction layer with the existing codebase, including client capability detection, player preference storage, and output formatting.

## Current Architecture

### Existing Systems

1. **protocol.c (KaVir's snippet)**
   - Handles telnet negotiation (IAC, WILL/DO/DONT/WONT)
   - Detects client capabilities (MSDP, GMCP, MXP, MCCP, 256-colors)
   - Stores capabilities in `protocol_t` structure
   - Processes color codes, MXP tags, MSDP variables

2. **Player Preferences**
   - `PLR_COLOUR` - Player wants colors (stored in character file)
   - `COMM_MXP` - Player wants MXP tags (stored in character file)
   - Other COMM flags for channel preferences

3. **Descriptor Structure**
   - `pProtocol` - Points to `protocol_t` with client capabilities
   - `conn` - Points to `connection_t` (TCP/TLS/WebSocket)

## New Architecture

### Protocol Layer Abstraction

```
┌─────────────────────────────────────────────────┐
│            Game Logic Layer                     │
│  (send_to_char, act, page_to_char, etc.)       │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│         Protocol Layer                          │
│  • Check player preferences (PLR_COLOUR, etc.)  │
│  • Check client capabilities (b256Support, etc.)│
│  • Process colors/MXP based on both             │
│  • Format MSDP/GMCP based on connection type    │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓
┌─────────────────────────────────────────────────┐
│         Connection Layer                        │
│  • Telnet (TCP/TLS)                            │
│  • WebSocket (plain/TLS)                       │
│  • Frame encoding/decoding                     │
└─────────────────────────────────────────────────┘
```

### Capability Detection Flow

#### For Telnet Connections (TCP/TLS):

1. Connection established
2. `protocol_telnet_create()` called
   - Creates `protocol_t` via `ProtocolCreate()`
   - Initializes with default capabilities
3. `protocol_negotiate()` called
   - Sends telnet IAC sequences (WILL MSDP, WILL MXP, etc.)
   - Client responds with DO/DONT
4. Client capabilities stored in `protocol_t`:
   - `pProtocol->bMXP` - MXP support
   - `pProtocol->bMCCP` - Compression support
   - `pProtocol->b256Support` - 256-color support (eYES/eNO/eSOMETIMES)
   - `pProtocol->pVariables[eMSDP_CLIENT_ID]` - Client name
   - `pProtocol->pVariables[eMSDP_CLIENT_VERSION]` - Client version

#### For WebSocket Connections:

1. Connection established
2. WebSocket handshake completed
3. `protocol_websocket_create()` called
   - Sets default capabilities (ANSI, 256-color, UTF-8, GMCP)
4. Optional: Send GMCP Core.Hello to get client info
   - Client responds with client name/version
5. Capabilities stored in protocol layer:
   - Assume modern browser: ANSI colors, UTF-8, 256-color
   - GMCP support (JSON-based)
   - No telnet-specific features

### Player Preference Storage

#### Current Preferences (already implemented):

- `PLR_COLOUR` (bit T in ch->act[0]) - Player wants colors
  - ON: Send ANSI color codes
  - OFF: Strip all color codes

- `COMM_MXP` (bit I in ch->comm) - Player wants MXP
  - ON: Send MXP tags (if client supports)
  - OFF: Strip MXP tags

#### Proposed Additional Preferences:

We should add these to the player settings menu:

- `COMM_ANSI_ONLY` - Use basic ANSI only, no 256-color
  - For players on terminals that claim 256-color but render poorly

- `COMM_UTF8` - Use UTF-8 unicode characters
  - For maps, special symbols, etc.
  - Default: auto-detect from client

### Output Processing Logic

When sending text to a player, the system should:

```c
bool should_send_colors(DESCRIPTOR_DATA *d)
{
    // Must have both: player wants colors AND client supports them
    if (!d->character)
        return protocol_has_capability(d->proto, PROTO_CAP_ANSI_COLOR);

    return IS_SET(d->character->act[0], PLR_COLOUR) &&
           protocol_has_capability(d->proto, PROTO_CAP_ANSI_COLOR);
}

bool should_send_256_colors(DESCRIPTOR_DATA *d)
{
    // Must have: colors enabled, client supports 256, player hasn't disabled
    if (!should_send_colors(d))
        return false;

    if (d->character && IS_SET(d->character->comm, COMM_ANSI_ONLY))
        return false;

    return protocol_has_capability(d->proto, PROTO_CAP_XTERM_256);
}

bool should_send_mxp(DESCRIPTOR_DATA *d)
{
    // Must have: client supports MXP AND player hasn't disabled
    if (!protocol_has_capability(d->proto, PROTO_CAP_MXP))
        return false;

    if (!d->character)
        return false;

    return IS_SET(d->character->comm, COMM_MXP);
}
```

## Integration Steps

### Phase 1: Add Protocol Layer to Descriptor (Non-Breaking)

1. Add `protocol_layer_t *proto` to DESCRIPTOR_DATA (already has `conn`)
2. Create protocol layer in `init_descriptor()`:
   ```c
   dnew->proto = protocol_layer_create_for_connection(dnew->conn, dnew);
   ```
3. Keep existing `pProtocol` for now (backward compatibility)
4. Free protocol layer in `close_socket()`:
   ```c
   if (d->proto) {
       protocol_layer_free(d->proto);
       d->proto = NULL;
   }
   ```

### Phase 2: Update Protocol Telnet Layer (Enhancement)

Make protocol_telnet.c actually use the client capabilities from protocol_t:

```c
static const char* telnet_process_output(protocol_layer_t *proto,
                                        const char *output, int *out_len)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Update capabilities in protocol layer based on protocol_t
    if (telnet_proto->pProtocol->b256Support == eYES)
        proto->capabilities |= PROTO_CAP_XTERM_256;
    else
        proto->capabilities &= ~PROTO_CAP_XTERM_256;

    if (telnet_proto->pProtocol->bMXP)
        proto->capabilities |= PROTO_CAP_MXP;
    else
        proto->capabilities &= ~PROTO_CAP_MXP;

    // Use existing ProtocolOutput
    return ProtocolOutput(proto->descriptor, output, out_len);
}
```

### Phase 3: Update Output Functions (Gradual Migration)

Update send_to_char and related functions to use protocol layer:

```c
void send_to_char(const char *txt, CHAR_DATA *ch)
{
    if (ch == NULL || ch->desc == NULL)
        return;

    DESCRIPTOR_DATA *d = ch->desc;

    // Use new protocol layer if available
    if (d->proto) {
        int len = 0;
        const char *processed = protocol_process_output(d->proto, txt, &len);
        write_to_buffer(d, processed, len > 0 ? len : 0);
    } else {
        // Fallback to old system
        const char *processed = ProtocolOutput(d, txt, NULL);
        write_to_buffer(d, processed, 0);
    }
}
```

### Phase 4: Add Player Preference Commands

Add these commands for players:

```
CONFIG COLOUR ON|OFF       - Enable/disable colors
CONFIG XTERM256 ON|OFF     - Enable/disable 256-color (ANSI only if off)
CONFIG MXP ON|OFF          - Enable/disable MXP tags
CONFIG UTF8 ON|OFF         - Enable/disable UTF-8 characters
```

### Phase 5: WebSocket-Specific Features

For WebSocket clients, implement GMCP properly:

1. Send GMCP Core.Hello on connection
2. Receive Core.Supports.Set to know what modules client wants
3. Send GMCP updates for vitals, room info, etc. via JSON
4. No telnet IAC wrapper needed

## Capability Matrix

| Feature | Telnet (TCP/TLS) | WebSocket | Detected How | Player Control |
|---------|-----------------|-----------|--------------|----------------|
| ANSI Colors | Yes | Yes | Auto | PLR_COLOUR |
| 256 Colors | Sometimes | Yes | Negotiation/Auto | COMM_ANSI_ONLY |
| UTF-8 | Sometimes | Yes | Negotiation/Auto | COMM_UTF8 |
| MXP | Sometimes | No | Negotiation | COMM_MXP |
| MCCP | Sometimes | No | Negotiation | Auto |
| MSDP | Sometimes | No | Negotiation | Auto |
| GMCP | Sometimes | Yes | Negotiation/Auto | Auto |

## Backward Compatibility

1. Keep `pProtocol` until migration complete
2. Fall back to old code path if `proto` is NULL
3. Gradually migrate functions to use new layer
4. Remove old code only after all functions migrated

## Benefits

1. **WebSocket clients get full feature parity** - Colors, GMCP, UTF-8
2. **Player control** - Can disable features they don't want
3. **Client capability detection** - Don't send features client can't use
4. **Transport-agnostic game code** - send_to_char doesn't care about connection type
5. **Future-proof** - Easy to add new connection types or protocols
