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
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/**
 * @file connection_websocket.c
 * @brief WebSocket connection implementation (RFC 6455)
 *
 * Implements WebSocket protocol for browser-based clients. This file
 * contains both plain WebSocket (not currently used) and WebSocket over
 * TLS (WSS) implementations.
 *
 * WebSocket connections require a two-phase handshake:
 *   1. TLS handshake (for WSS connections)
 *   2. HTTP Upgrade handshake with Sec-WebSocket-Accept key derivation
 *
 * Once connected, data is framed according to RFC 6455:
 *   - Client frames are always masked (XOR with 4-byte key)
 *   - Server frames are never masked
 *   - Text opcode (0x1) used for MUD data
 *   - Close/Ping/Pong control frames handled
 *
 * Key features:
 *   - Base64 encoding for accept key generation (OpenSSL BIO)
 *   - SHA-1 hashing for WebSocket key validation
 *   - Frame parsing with variable-length payload support
 *   - Automatic unmasking of client frames
 *
 * @note Plain WebSocket (ws://) is not supported - only secure WSS (wss://)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "connection.h"
#include "merc.h"

/** @brief External SSL context from tls.c */
extern SSL_CTX *ctx;

/**
 * @name WebSocket Opcodes (RFC 6455)
 * @{
 */
#define WS_OPCODE_CONTINUATION  0x0  /**< Continuation frame */
#define WS_OPCODE_TEXT          0x1  /**< Text data frame */
#define WS_OPCODE_BINARY        0x2  /**< Binary data frame */
#define WS_OPCODE_CLOSE         0x8  /**< Connection close frame */
#define WS_OPCODE_PING          0x9  /**< Ping control frame */
#define WS_OPCODE_PONG          0xA  /**< Pong control frame */
/** @} */

/**
 * @enum ws_handshake_state_t
 * @brief WebSocket handshake progress states
 */
typedef enum {
    WS_HANDSHAKE_READING_REQUEST,  /**< Reading HTTP upgrade request */
    WS_HANDSHAKE_TLS_COMPLETE,     /**< TLS done, awaiting WebSocket upgrade */
    WS_HANDSHAKE_COMPLETE,         /**< Fully connected */
    WS_HANDSHAKE_FAILED            /**< Handshake failed */
} ws_handshake_state_t;

/**
 * @struct ws_state
 * @brief WebSocket protocol state machine
 *
 * Tracks handshake progress and frame parsing state. WebSocket frames
 * may arrive fragmented across multiple reads, so partial frame data
 * is buffered here.
 */
typedef struct ws_state {
    ws_handshake_state_t handshake_state;  /**< Current handshake phase */
    char handshake_buffer[4096];           /**< HTTP upgrade request buffer */
    int handshake_buffer_len;              /**< Bytes in handshake buffer */

    unsigned char frame_buffer[65536];     /**< Incoming frame data buffer */
    int frame_buffer_len;                  /**< Bytes in frame buffer */
    bool frame_fin;                        /**< FIN bit of current frame */
    unsigned char frame_opcode;            /**< Opcode of current frame */
    bool frame_masked;                     /**< True if frame is masked (client frames) */
    unsigned char frame_mask[4];           /**< Masking key */
    uint64_t frame_payload_len;            /**< Payload length from header */
    int frame_bytes_read;                  /**< Payload bytes read so far */
} ws_state_t;

/**
 * @struct connection_websocket
 * @brief WebSocket connection implementation structure
 */
typedef struct connection_websocket {
    connection_t base;       /**< Base connection (must be first) */
    ws_state_t *ws_state;    /**< WebSocket protocol state */
} connection_websocket_t;

/*
 * Forward declarations
 */
static bool ws_read(connection_t *conn, char *buf, int size, int *bytes_read);
static bool ws_write(connection_t *conn, const char *buf, int size, int *bytes_written);
static bool ws_process_handshake(connection_t *conn);
static void ws_close(connection_t *conn);
static void ws_free(connection_t *conn);
static bool ws_is_secure(connection_t *conn);
static const char* ws_get_protocol_name(connection_t *conn);

// WebSocket TLS specific functions
static bool wss_read(connection_t *conn, char *buf, int size, int *bytes_read);
static bool wss_write(connection_t *conn, const char *buf, int size, int *bytes_written);
static bool wss_process_handshake(connection_t *conn);
static void wss_close(connection_t *conn);
static bool wss_is_secure(connection_t *conn);
static const char* wss_get_protocol_name(connection_t *conn);

/*
 * WebSocket virtual function table
 */
static connection_vtable_t ws_vtable = {
    .read = ws_read,
    .write = ws_write,
    .process_handshake = ws_process_handshake,
    .close = ws_close,
    .free = ws_free,
    .is_secure = ws_is_secure,
    .get_protocol_name = ws_get_protocol_name
};

/*
 * WebSocket TLS virtual function table
 */
static connection_vtable_t wss_vtable = {
    .read = wss_read,
    .write = wss_write,
    .process_handshake = wss_process_handshake,
    .close = wss_close,
    .free = ws_free,  // Same as plain WebSocket
    .is_secure = wss_is_secure,
    .get_protocol_name = wss_get_protocol_name
};

/**
 * ws_base64_encode - Base64 encode data for WebSocket accept key
 *
 * Uses OpenSSL BIO to encode binary data (SHA-1 hash) to base64 for
 * the Sec-WebSocket-Accept response header.
 *
 * @param input   Binary data to encode
 * @param length  Length of input data
 * @return        malloc'd base64 string (caller must free), or NULL on error
 */
static char* ws_base64_encode(const unsigned char *input, int length)
{
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;
    char *output;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    output = (char*)malloc(buffer_ptr->length + 1);
    memcpy(output, buffer_ptr->data, buffer_ptr->length);
    output[buffer_ptr->length] = '\0';

    BIO_free_all(bio);
    return output;
}

/**
 * generate_accept_key - Generate Sec-WebSocket-Accept response key
 *
 * Concatenates client key with RFC 6455 magic GUID, computes SHA-1 hash,
 * and base64 encodes the result. This proves to the client that the
 * server understands WebSocket protocol.
 *
 * @param client_key  The Sec-WebSocket-Key from client request
 * @return            malloc'd accept key string (caller must free)
 */
static char* generate_accept_key(const char *client_key)
{
    static const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    unsigned char hash[SHA_DIGEST_LENGTH];

    strlcpy(combined, client_key, sizeof(combined));
    strlcat(combined, magic, sizeof(combined));
    SHA1((unsigned char*)combined, strlen(combined), hash);

    return ws_base64_encode(hash, SHA_DIGEST_LENGTH);
}

/**
 * process_ws_handshake - Complete WebSocket HTTP upgrade handshake
 *
 * Parses the HTTP upgrade request, extracts Sec-WebSocket-Key header,
 * generates the accept key, and sends the 101 Switching Protocols response.
 *
 * @param ws_conn  The WebSocket connection with complete HTTP request in buffer
 * @return         true if handshake successful, false on error
 */
static bool process_ws_handshake(connection_websocket_t *ws_conn)
{
    ws_state_t *state = ws_conn->ws_state;
    char *key_start, *key_end;
    char client_key[256];
    char *accept_key;
    char response[1024];

    // Find Sec-WebSocket-Key header
    key_start = strstr(state->handshake_buffer, "Sec-WebSocket-Key:");
    if (!key_start) {
        log_string("WebSocket handshake: No Sec-WebSocket-Key header");
        return false;
    }

    key_start += 18;  // Skip "Sec-WebSocket-Key:"
    while (*key_start == ' ') key_start++;  // Skip whitespace

    key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        log_string("WebSocket handshake: Malformed Sec-WebSocket-Key");
        return false;
    }

    int key_len = key_end - key_start;
    if (key_len <= 0 || key_len >= sizeof(client_key) || key_len > 64) {
        log_string("WebSocket handshake: Key too long");
        return false;
    }

    strncpy(client_key, key_start, key_len);
    client_key[key_len] = '\0';

    // Generate accept key
    accept_key = generate_accept_key(client_key);
    if (!accept_key) {
        log_string("WebSocket handshake: Failed to generate accept key");
        return false;
    }

    // Send handshake response
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    free(accept_key);

    // Write response directly to socket/SSL (not through WebSocket framing)
    ssize_t nwritten;
    if (ws_conn->base.type == CONN_TYPE_WEBSOCKET_TLS) {
        // Use SSL_write for TLS connections
        SSL *ssl = (SSL*)ws_conn->base.proto_data;
        nwritten = SSL_write(ssl, response, strlen(response));
        if (nwritten <= 0) {
            int ssl_error = SSL_get_error(ssl, nwritten);
            log_stringf("WebSocket handshake: SSL_write() failed, error %d", ssl_error);
            ERR_print_errors_fp(stderr);
            return false;
        }
    } else {
        // Use raw write for plain WebSocket
        nwritten = write(ws_conn->base.fd, response, strlen(response));
        if (nwritten < 0) {
            log_stringf("WebSocket handshake: write() failed: %s", strerror(errno));
            return false;
        }
    }

    log_stringf("WebSocket handshake completed (fd %d)", ws_conn->base.fd);
    state->handshake_state = WS_HANDSHAKE_COMPLETE;
    ws_conn->base.state = CONN_STATE_CONNECTED;
    ws_conn->base.handshake_in_progress = false;

    return true;
}

/**
 * connection_websocket_create - Create a plain WebSocket connection
 *
 * Factory function for plain (unencrypted) WebSocket connections.
 * Note: This is currently unused as only WSS is supported.
 *
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate
 * @return      New connection, or NULL on failure
 */
connection_t* connection_websocket_create(int fd, struct descriptor_data *desc)
{
    connection_websocket_t *ws_conn;
    ws_state_t *state;

    // Allocate connection structure
    ws_conn = (connection_websocket_t*)calloc(1, sizeof(connection_websocket_t));
    if (!ws_conn) {
        log_string("connection_websocket_create: Out of memory");
        return NULL;
    }

    // Allocate WebSocket state
    state = (ws_state_t*)calloc(1, sizeof(ws_state_t));
    if (!state) {
        log_string("connection_websocket_create: Out of memory for state");
        free(ws_conn);
        return NULL;
    }

    // Set socket to non-blocking
    if (!connection_set_nonblocking(fd)) {
        free(state);
        free(ws_conn);
        return NULL;
    }

    // Initialize WebSocket state
    state->handshake_state = WS_HANDSHAKE_READING_REQUEST;
    state->handshake_buffer_len = 0;
    state->frame_buffer_len = 0;

    // Initialize base connection
    // Note: Plain WebSocket not supported - this function exists for future use only
    ws_conn->base.vtable = &ws_vtable;
    ws_conn->base.type = CONN_TYPE_WEBSOCKET_TLS;  // Placeholder
    ws_conn->base.state = CONN_STATE_CONNECTING;
    ws_conn->base.fd = fd;
    ws_conn->base.descriptor = desc;
    ws_conn->base.proto_data = state;
    ws_conn->base.last_activity = current_time;
    ws_conn->base.handshake_in_progress = true;
    ws_conn->ws_state = state;

    return (connection_t*)ws_conn;
}

/**
 * ws_unmask_payload - XOR unmask WebSocket client payload
 *
 * Per RFC 6455, all client-to-server frames must be masked with a 4-byte
 * key. This function applies XOR with the rotating mask to recover the
 * original payload data in-place.
 *
 * @param payload  Data buffer to unmask (modified in place)
 * @param len      Length of payload data
 * @param mask     4-byte masking key from frame header
 */
static void ws_unmask_payload(unsigned char *payload, int len, const unsigned char *mask)
{
    for (int i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

/**
 * ws_parse_frame - Parse WebSocket frame from buffer and extract payload
 *
 * Parses the RFC 6455 frame format from the connection's frame buffer:
 *   - 2-byte minimum header (FIN, opcode, mask bit, length)
 *   - Extended length (2 or 8 bytes) if needed
 *   - 4-byte masking key (always present for client frames)
 *   - Payload data
 *
 * Handles control frames:
 *   - CLOSE (0x8): Returns false to signal disconnect
 *   - PING (0x9): Logged (PONG response not yet implemented)
 *   - PONG (0xA): Ignored (keep-alive acknowledgment)
 *
 * Data frames (TEXT/BINARY) are unmasked and copied to output buffer.
 * Processed frame is removed from the internal buffer.
 *
 * @param ws_conn      The WebSocket connection with frame data
 * @param output_buf   Buffer to receive unmasked payload
 * @param output_size  Maximum bytes to copy to output
 * @param output_len   Output: actual bytes copied
 * @return             true if more data needed or success, false on close/error
 */
static bool ws_parse_frame(connection_websocket_t *ws_conn, char *output_buf, int output_size, int *output_len)
{
    ws_state_t *state = ws_conn->ws_state;
    unsigned char *frame = state->frame_buffer;
    int available = state->frame_buffer_len;

    *output_len = 0;

    // Need at least 2 bytes for basic frame header
    if (available < 2)
        return true;  // Need more data

    // Parse frame header
    // bool fin = (frame[0] & 0x80) != 0;  // TODO: Handle fragmented frames
    unsigned char opcode = frame[0] & 0x0F;
    bool masked = (frame[1] & 0x80) != 0;
    uint64_t payload_len = frame[1] & 0x7F;

    int header_len = 2;

    // Extended payload length
    if (payload_len == 126) {
        if (available < 4) return true;  // Need more data
        payload_len = (frame[2] << 8) | frame[3];
        header_len = 4;
    } else if (payload_len == 127) {
        if (available < 10) return true;  // Need more data
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | frame[2 + i];
        }
        header_len = 10;
    }

    // Masking key (always present from client)
    if (masked) {
        if (available < header_len + 4) return true;  // Need more data
        memcpy(state->frame_mask, frame + header_len, 4);
        header_len += 4;
    }

    // Check if we have complete frame
    if (available < header_len + payload_len)
        return true;  // Need more data

    // Process frame based on opcode
    unsigned char *payload = frame + header_len;

    switch (opcode) {
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY:
            // Unmask payload if masked
            if (masked) {
                ws_unmask_payload(payload, payload_len, state->frame_mask);
            }

            // Copy to output buffer
            if (payload_len > output_size) {
                log_string("WebSocket frame too large for output buffer");
                return false;
            }

            memcpy(output_buf, payload, payload_len);
            *output_len = payload_len;
            break;

        case WS_OPCODE_CLOSE:
            log_stringf("WebSocket close frame received (fd %d)", ws_conn->base.fd);
            return false;  // Signal connection close

        case WS_OPCODE_PING:
            // Should send PONG - for now just acknowledge
            log_stringf("WebSocket ping received (fd %d)", ws_conn->base.fd);
            break;

        case WS_OPCODE_PONG:
            // Keep-alive response - just acknowledge
            break;

        default:
            log_stringf("WebSocket unknown opcode 0x%02x (fd %d)", opcode, ws_conn->base.fd);
            break;
    }

    // Remove processed frame from buffer
    int frame_total_len = header_len + payload_len;
    state->frame_buffer_len -= frame_total_len;
    if (state->frame_buffer_len > 0) {
        memmove(state->frame_buffer, state->frame_buffer + frame_total_len, state->frame_buffer_len);
    }

    return true;
}

/**
 * ws_read - Read data from plain WebSocket connection
 *
 * Reads raw socket data into the frame buffer, then attempts to parse
 * a complete WebSocket frame. Returns only complete, unmasked payload
 * data to the caller. Partial frames remain buffered.
 *
 * Blocks until handshake is complete - returns immediately with no data
 * if still in handshake phase.
 *
 * @param conn        The connection to read from
 * @param buf         Buffer to store decoded payload data
 * @param size        Maximum bytes to read
 * @param bytes_read  Output: actual payload bytes extracted
 * @return            true on success/would-block, false on error/disconnect
 *
 * @note Currently unused as only WSS is supported (see wss_read)
 */
static bool ws_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    ssize_t nread;

    *bytes_read = 0;

    // If still in handshake, we shouldn't be reading game data yet
    if (state->handshake_state != WS_HANDSHAKE_COMPLETE) {
        return true;
    }

    // Read raw data from socket into frame buffer
    int buffer_space = sizeof(state->frame_buffer) - state->frame_buffer_len;
    if (buffer_space > 0) {
        nread = read(conn->fd, state->frame_buffer + state->frame_buffer_len, buffer_space);

        if (nread > 0) {
            state->frame_buffer_len += nread;
            conn->last_activity = current_time;
        } else if (nread == 0) {
            // Connection closed
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available yet
            } else {
                // Error
                log_stringf("ws_read: read() failed: %s", strerror(errno));
                return false;
            }
        }
    }

    // Try to parse a frame
    return ws_parse_frame(ws_conn, buf, size, bytes_read);
}

/**
 * ws_write - Write data to plain WebSocket connection
 *
 * Wraps payload data in a WebSocket frame and writes to socket:
 *   - Builds frame header with FIN=1, opcode=TEXT (0x1), mask=0
 *   - Encodes payload length (7-bit, 16-bit, or 64-bit)
 *   - Writes header then payload
 *
 * Server frames are never masked per RFC 6455.
 *
 * @param conn           The connection to write to
 * @param buf            Payload data to send
 * @param size           Bytes to send
 * @param bytes_written  Output: payload bytes written (excluding frame header)
 * @return               true on success, false on error
 *
 * @note Partial header writes are fatal - frame framing would be corrupted.
 * @note Currently unused as only WSS is supported (see wss_write)
 */
static bool ws_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    unsigned char frame_header[10];
    int header_len;
    ssize_t nwritten;

    *bytes_written = 0;

    // If still in handshake, can't send game data yet
    if (state->handshake_state != WS_HANDSHAKE_COMPLETE) {
        return true;
    }

    // Build WebSocket frame header
    // Server frames are never masked (mask bit = 0)
    frame_header[0] = 0x80 | WS_OPCODE_TEXT;  // FIN=1, opcode=text

    // Payload length encoding
    if (size <= 125) {
        frame_header[1] = size;
        header_len = 2;
    } else if (size <= 65535) {
        frame_header[1] = 126;
        frame_header[2] = (size >> 8) & 0xFF;
        frame_header[3] = size & 0xFF;
        header_len = 4;
    } else {
        // For very large messages, use 64-bit length
        frame_header[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame_header[9 - i] = (size >> (i * 8)) & 0xFF;
        }
        header_len = 10;
    }

    // Write frame header
    nwritten = write(conn->fd, frame_header, header_len);
    if (nwritten < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block - return 0 bytes written
            return true;
        }
        log_stringf("ws_write: write(header) failed: %s", strerror(errno));
        return false;
    }

    if (nwritten != header_len) {
        // Partial header write - this is problematic
        log_string("ws_write: Partial header write");
        return false;
    }

    // Write payload
    nwritten = write(conn->fd, buf, size);
    if (nwritten < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block - header was sent but not payload (problematic)
            log_string("ws_write: Payload blocked after header sent");
            return false;
        }
        log_stringf("ws_write: write(payload) failed: %s", strerror(errno));
        return false;
    }

    *bytes_written = nwritten;
    conn->last_activity = current_time;

    return true;
}

/**
 * ws_process_handshake - Process plain WebSocket HTTP upgrade handshake
 *
 * Reads HTTP upgrade request from socket and completes the WebSocket
 * handshake when the full request is received (detected by \r\n\r\n).
 *
 * @param conn  The connection with pending handshake
 * @return      true when handshake complete, false if still in progress/error
 *
 * @note Currently unused as only WSS is supported (see wss_process_handshake)
 */
static bool ws_process_handshake(connection_t *conn)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    ssize_t nread;

    if (!conn->handshake_in_progress)
        return true;  // Already complete

    // Read handshake data
    nread = read(conn->fd,
                 state->handshake_buffer + state->handshake_buffer_len,
                 sizeof(state->handshake_buffer) - state->handshake_buffer_len - 1);

    if (nread > 0) {
        state->handshake_buffer_len += nread;
        state->handshake_buffer[state->handshake_buffer_len] = '\0';

        // Check if we have complete HTTP request (ends with \r\n\r\n)
        if (strstr(state->handshake_buffer, "\r\n\r\n")) {
            return process_ws_handshake(ws_conn);
        }

        // Still reading request
        return false;
    } else if (nread == 0) {
        // Connection closed
        return false;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data yet
            return false;
        }
        // Error
        log_stringf("ws_process_handshake: read() failed: %s", strerror(errno));
        return false;
    }
}

/**
 * ws_close - Close plain WebSocket connection
 *
 * Performs connection shutdown:
 *   1. Sets state to CLOSING
 *   2. Calls shutdown() on socket
 *   3. Closes file descriptor
 *   4. Sets state to CLOSED
 *
 * @param conn  The connection to close
 *
 * @note Does not send WebSocket close frame (0x8) - could be improved
 * @note Currently unused as only WSS is supported (see wss_close)
 */
static void ws_close(connection_t *conn)
{
    if (conn->state == CONN_STATE_CLOSED)
        return;

    conn->state = CONN_STATE_CLOSING;

    // Send WebSocket close frame (opcode 0x8)
    // For now, just close the socket
    shutdown(conn->fd, SHUT_RDWR);
    close(conn->fd);

    conn->state = CONN_STATE_CLOSED;
}

/**
 * ws_free - Free WebSocket connection resources
 *
 * Ensures connection is closed, frees the WebSocket state structure,
 * then frees the connection structure itself.
 *
 * @param conn  The connection to free
 *
 * @note Shared by both plain WS and WSS connections (wss_vtable.free)
 */
static void ws_free(connection_t *conn)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;

    // Ensure connection is closed
    if (conn->state != CONN_STATE_CLOSED)
        ws_close(conn);

    // Free WebSocket state
    if (ws_conn->ws_state) {
        free(ws_conn->ws_state);
    }

    // Free the structure
    free(ws_conn);
}

/**
 * ws_is_secure - Check if plain WebSocket connection is encrypted
 *
 * Plain WebSocket (ws://) has no encryption.
 *
 * @param conn  The connection to check (unused)
 * @return      Always returns false
 */
static bool ws_is_secure(connection_t *conn)
{
    return false;
}

/**
 * ws_get_protocol_name - Get protocol name for logging
 *
 * @param conn  The connection (unused)
 * @return      "WebSocket"
 */
static const char* ws_get_protocol_name(connection_t *conn)
{
    return "WebSocket";
}

/**
 * connection_websocket_tls_create - Create a WebSocket over TLS connection
 *
 * Factory function for WSS (WebSocket Secure) connections. Allocates
 * connection structure, WebSocket state, and SSL context. Sets up for
 * two-phase handshake:
 *   1. TLS handshake (SSL_accept)
 *   2. WebSocket HTTP upgrade
 *
 * Uses the global SSL context (ctx) from tls.c. Socket is set to
 * non-blocking mode.
 *
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with connection
 * @return      New connection_t pointer, or NULL on failure
 */
connection_t* connection_websocket_tls_create(int fd, struct descriptor_data *desc)
{
    connection_websocket_t *ws_conn;
    ws_state_t *state;
    SSL *ssl;

    // Allocate connection structure
    ws_conn = (connection_websocket_t*)calloc(1, sizeof(connection_websocket_t));
    if (!ws_conn) {
        log_string("connection_websocket_tls_create: Out of memory");
        return NULL;
    }

    // Allocate WebSocket state
    state = (ws_state_t*)calloc(1, sizeof(ws_state_t));
    if (!state) {
        log_string("connection_websocket_tls_create: Out of memory for state");
        free(ws_conn);
        return NULL;
    }

    // Set socket to non-blocking
    if (!connection_set_nonblocking(fd)) {
        free(state);
        free(ws_conn);
        return NULL;
    }

    // Create SSL object
    ssl = SSL_new(ctx);
    if (!ssl) {
        log_string("connection_websocket_tls_create: SSL_new() failed");
        ERR_print_errors_fp(stderr);
        free(state);
        free(ws_conn);
        return NULL;
    }

    // Attach socket to SSL
    if (SSL_set_fd(ssl, fd) != 1) {
        log_string("connection_websocket_tls_create: SSL_set_fd() failed");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        free(state);
        free(ws_conn);
        return NULL;
    }

    // Set SSL to server mode
    SSL_set_accept_state(ssl);

    // Initialize WebSocket state (handshake will happen after TLS completes)
    state->handshake_state = WS_HANDSHAKE_READING_REQUEST;
    state->handshake_buffer_len = 0;
    state->frame_buffer_len = 0;

    // Initialize base connection
    ws_conn->base.vtable = &wss_vtable;  // Use TLS vtable
    ws_conn->base.type = CONN_TYPE_WEBSOCKET_TLS;
    ws_conn->base.state = CONN_STATE_CONNECTING;
    ws_conn->base.fd = fd;
    ws_conn->base.descriptor = desc;
    ws_conn->base.proto_data = ssl;  // Store SSL in proto_data
    ws_conn->base.last_activity = current_time;
    ws_conn->base.handshake_in_progress = true;
    ws_conn->ws_state = state;

    return (connection_t*)ws_conn;
}

/**
 * wss_process_handshake - Process WSS two-phase handshake
 *
 * Handles the two-phase connection setup for WebSocket over TLS:
 *
 * Phase 1 (WS_HANDSHAKE_READING_REQUEST state):
 *   - Calls SSL_accept() for TLS handshake
 *   - Handles SSL_ERROR_WANT_READ/WANT_WRITE for async operation
 *   - On success, transitions to WS_HANDSHAKE_TLS_COMPLETE
 *
 * Phase 2 (WS_HANDSHAKE_TLS_COMPLETE state):
 *   - Reads HTTP upgrade request via SSL_read()
 *   - When complete request received (ends with \r\n\r\n), calls process_ws_handshake()
 *   - On success, connection is fully established
 *
 * @param conn  The connection with pending handshake
 * @return      true when fully connected, false if in progress or error
 */
static bool wss_process_handshake(connection_t *conn)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    SSL *ssl = (SSL*)conn->proto_data;
    int ret;

    if (!conn->handshake_in_progress)
        return true;  // Already complete

    // Phase 1: TLS handshake
    if (state->handshake_state == WS_HANDSHAKE_READING_REQUEST) {
        // Still doing TLS handshake - not yet reading WebSocket upgrade
        ERR_clear_error();
        ret = SSL_accept(ssl);

        if (ret == 1) {
            // TLS handshake complete! Now ready for WebSocket upgrade
            log_stringf("WebSocket TLS handshake completed (fd %d), awaiting WebSocket upgrade", conn->fd);
            state->handshake_state = WS_HANDSHAKE_TLS_COMPLETE;
            return false;  // Still need WebSocket upgrade
        }

        int ssl_error = SSL_get_error(ssl, ret);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            // Still in progress
            return false;
        }

        // TLS handshake failed
        log_stringf("WebSocket TLS handshake failed: SSL_accept returned %d, error %d", ret, ssl_error);
        ERR_print_errors_fp(stderr);
        conn->state = CONN_STATE_CLOSED;
        return false;
    }

    // Phase 2: WebSocket HTTP upgrade handshake (using SSL_read)
    ssize_t nread = SSL_read(ssl,
                             state->handshake_buffer + state->handshake_buffer_len,
                             sizeof(state->handshake_buffer) - state->handshake_buffer_len - 1);

    if (nread > 0) {
        state->handshake_buffer_len += nread;
        state->handshake_buffer[state->handshake_buffer_len] = '\0';

        // Check if we have complete HTTP request (ends with \r\n\r\n)
        if (strstr(state->handshake_buffer, "\r\n\r\n")) {
            // Process WebSocket upgrade - need to write response via SSL too
            return process_ws_handshake(ws_conn);
        }

        // Still reading request
        return false;
    } else {
        int ssl_error = SSL_get_error(ssl, nread);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            // No data yet
            return false;
        }
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
            // Connection closed
            log_string("WebSocket TLS: Connection closed during upgrade");
            conn->state = CONN_STATE_CLOSED;
            return false;
        }
        // Error
        log_stringf("wss_process_handshake: SSL_read() failed, error %d", ssl_error);
        ERR_print_errors_fp(stderr);
        conn->state = CONN_STATE_CLOSED;
        return false;
    }
}

/**
 * wss_read - Read data from WebSocket TLS connection
 *
 * Reads encrypted frame data via SSL_read() into the frame buffer,
 * then parses WebSocket frames to extract payload data.
 *
 * Returns only complete, unmasked payload data. Partial frames remain
 * buffered. Returns immediately with no data if handshake incomplete.
 *
 * @param conn        The WSS connection to read from
 * @param buf         Buffer to store decoded payload data
 * @param size        Maximum bytes to read
 * @param bytes_read  Output: actual payload bytes extracted
 * @return            true on success/would-block, false on error/disconnect
 */
static bool wss_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    SSL *ssl = (SSL*)conn->proto_data;
    ssize_t nread;

    *bytes_read = 0;

    // If still in handshake, can't read game data yet
    if (state->handshake_state != WS_HANDSHAKE_COMPLETE) {
        return true;
    }

    // Read raw data into frame buffer using SSL
    nread = SSL_read(ssl,
                     state->frame_buffer + state->frame_buffer_len,
                     sizeof(state->frame_buffer) - state->frame_buffer_len);

    if (nread > 0) {
        state->frame_buffer_len += nread;
        conn->last_activity = current_time;

        // Try to parse a frame
        return ws_parse_frame(ws_conn, buf, size, bytes_read);
    } else {
        int ssl_error = SSL_get_error(ssl, nread);
        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            return true;  // No data available, not an error
        }
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
            // Clean shutdown
            return false;
        }
        // Error
        log_stringf("wss_read: SSL_read() failed, error %d", ssl_error);
        return false;
    }
}

/**
 * wss_write - Write data to WebSocket TLS connection
 *
 * Wraps payload in a WebSocket frame and writes via SSL_write():
 *   - Builds frame header with FIN=1, opcode=TEXT (0x1), mask=0
 *   - Encodes payload length (7-bit, 16-bit, or 64-bit)
 *   - Writes header then payload via SSL
 *
 * Server frames are never masked per RFC 6455.
 *
 * @param conn           The WSS connection to write to
 * @param buf            Payload data to send
 * @param size           Bytes to send
 * @param bytes_written  Output: payload bytes written (excluding frame header)
 * @return               true on success, false on error
 *
 * @note Partial header writes are fatal - frame framing would be corrupted.
 */
static bool wss_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    connection_websocket_t *ws_conn = (connection_websocket_t*)conn;
    ws_state_t *state = ws_conn->ws_state;
    SSL *ssl = (SSL*)conn->proto_data;
    unsigned char frame_header[10];
    int header_len;
    ssize_t nwritten;

    *bytes_written = 0;

    // If still in handshake, can't send game data yet
    if (state->handshake_state != WS_HANDSHAKE_COMPLETE) {
        return true;
    }

    // Build WebSocket frame header
    // Server frames are never masked (mask bit = 0)
    frame_header[0] = 0x80 | WS_OPCODE_TEXT;  // FIN=1, opcode=text

    // Payload length encoding
    if (size <= 125) {
        frame_header[1] = size;
        header_len = 2;
    } else if (size <= 65535) {
        frame_header[1] = 126;
        frame_header[2] = (size >> 8) & 0xFF;
        frame_header[3] = size & 0xFF;
        header_len = 4;
    } else {
        // For very large messages, use 64-bit length
        frame_header[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame_header[9 - i] = (size >> (i * 8)) & 0xFF;
        }
        header_len = 10;
    }

    // Write frame header using SSL
    nwritten = SSL_write(ssl, frame_header, header_len);
    if (nwritten <= 0) {
        int ssl_error = SSL_get_error(ssl, nwritten);
        if (ssl_error == SSL_ERROR_WANT_WRITE) {
            return true;  // Would block
        }
        log_stringf("wss_write: SSL_write(header) failed, error %d", ssl_error);
        return false;
    }

    if (nwritten != header_len) {
        // Partial header write - this is problematic
        log_string("wss_write: Partial header write");
        return false;
    }

    // Write payload using SSL
    nwritten = SSL_write(ssl, buf, size);
    if (nwritten <= 0) {
        int ssl_error = SSL_get_error(ssl, nwritten);
        if (ssl_error == SSL_ERROR_WANT_WRITE) {
            // Header was sent but not payload (problematic)
            log_string("wss_write: Payload blocked after header sent");
            return false;
        }
        log_stringf("wss_write: SSL_write(payload) failed, error %d", ssl_error);
        return false;
    }

    *bytes_written = nwritten;
    conn->last_activity = current_time;

    return true;
}

/**
 * wss_close - Close WebSocket TLS connection
 *
 * Performs graceful shutdown:
 *   1. Sets state to CLOSING
 *   2. Calls SSL_shutdown() for TLS close notify
 *   3. Frees SSL context
 *   4. Closes socket file descriptor
 *   5. Sets state to CLOSED
 *
 * @param conn  The WSS connection to close
 *
 * @note Does not send WebSocket close frame (0x8) before SSL shutdown
 */
static void wss_close(connection_t *conn)
{
    SSL *ssl = (SSL*)conn->proto_data;

    if (conn->state == CONN_STATE_CLOSED)
        return;

    conn->state = CONN_STATE_CLOSING;

    // Shutdown SSL
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        conn->proto_data = NULL;
    }

    // Close socket
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }

    conn->state = CONN_STATE_CLOSED;
}

/**
 * wss_is_secure - Check if WSS connection is encrypted
 *
 * WebSocket over TLS is always encrypted.
 *
 * @param conn  The connection to check (unused)
 * @return      Always returns true
 */
static bool wss_is_secure(connection_t *conn)
{
    return true;  // WebSocket TLS is always secure
}

/**
 * wss_get_protocol_name - Get protocol name for logging
 *
 * @param conn  The connection (unused)
 * @return      "WebSocket TLS"
 */
static const char* wss_get_protocol_name(connection_t *conn)
{
    return "WebSocket TLS";
}
