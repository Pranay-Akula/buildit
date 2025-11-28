/*
 * Protocol definitions for ATM-Bank communication (Idea 1)
 * 
 * Message Format:
 *   Plaintext: [MSG_TYPE (1 byte)][USERNAME (251 bytes)][DATA (variable)][SEQ_NUM (8 bytes)]
 *   
 *   Encrypted: [IV (16 bytes)][AES-256-CBC(plaintext)][HMAC-SHA256(IV + ciphertext) (32 bytes)]
 *
 * All multi-byte integers are in network byte order (big-endian).
 */

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <arpa/inet.h>

/* Message Types */
#define MSG_LOGIN_REQ       0x01
#define MSG_LOGIN_RESP      0x02
#define MSG_BALANCE_REQ     0x03
#define MSG_BALANCE_RESP    0x04
#define MSG_WITHDRAW_REQ    0x05
#define MSG_WITHDRAW_RESP   0x06

/* Protocol Constants */
#define USERNAME_SIZE       251     // Maximum username length + null terminator
#define AUTH_TOKEN_SIZE     32      // HMAC-SHA256 of (card_secret || PIN)
#define PIN_SIZE            4       // PIN is exactly 4 digits

/* Message Header (common to all messages) */
typedef struct {
    uint8_t msg_type;               // One of MSG_* constants
    char username[USERNAME_SIZE];   // Null-padded username
} __attribute__((packed)) msg_header_t;

/* LOGIN_REQ: ATM -> Bank
 * Authenticates user with card secret + PIN
 * DATA field: [auth_token (32 bytes)][PIN (4 bytes)]
 */
typedef struct {
    msg_header_t header;
    uint8_t auth_token[AUTH_TOKEN_SIZE];  // HMAC(card_secret, card_secret || PIN)
    char pin[PIN_SIZE];                   // 4-digit PIN (not null-terminated)
    uint64_t seq_num;                     // Sequence number for replay protection
} __attribute__((packed)) msg_login_req_t;

/* LOGIN_RESP: Bank -> ATM
 * Response to login request
 * DATA field: [success (1 byte)]
 */
typedef struct {
    msg_header_t header;
    uint8_t success;                // 1 = authorized, 0 = not authorized
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_login_resp_t;

/* BALANCE_REQ: ATM -> Bank
 * Query user's current balance
 * DATA field: (empty)
 */
typedef struct {
    msg_header_t header;
    uint64_t seq_num;               // Sequence number for replay protection
} __attribute__((packed)) msg_balance_req_t;

/* BALANCE_RESP: Bank -> ATM
 * Response with user's balance
 * DATA field: [balance (4 bytes)]
 */
typedef struct {
    msg_header_t header;
    int32_t balance;                // Current balance (signed 32-bit)
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_balance_resp_t;

/* WITHDRAW_REQ: ATM -> Bank
 * Request to withdraw amount
 * DATA field: [amount (4 bytes)]
 */
typedef struct {
    msg_header_t header;
    int32_t amount;                 // Amount to withdraw (signed 32-bit)
    uint64_t seq_num;               // Sequence number for replay protection
} __attribute__((packed)) msg_withdraw_req_t;

/* WITHDRAW_RESP: Bank -> ATM
 * Response to withdraw request
 * DATA field: [success (1 byte)][new_balance (4 bytes)]
 */
typedef struct {
    msg_header_t header;
    uint8_t success;                // 1 = success, 0 = insufficient funds
    int32_t new_balance;            // New balance after withdrawal
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_withdraw_resp_t;

/* Maximum sizes for buffers */
#define MAX_PLAINTEXT_SIZE  512     // Maximum plaintext message size
#define IV_SIZE             16      // AES-CBC IV size
#define HMAC_SIZE           32      // HMAC-SHA256 size
#define MAX_ENCRYPTED_SIZE  (IV_SIZE + MAX_PLAINTEXT_SIZE + 16 + HMAC_SIZE) // IV + ciphertext (with padding) + HMAC

/*
 * Helper functions for protocol messages
 */

/* Convert integers to/from network byte order */
/* macOS/BSD provides htonll/ntohll, but Linux may not */
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(htonll)
static inline uint64_t htonll(uint64_t hostlonglong) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)htonl((uint32_t)(hostlonglong & 0xFFFFFFFF)) << 32) | 
           htonl((uint32_t)(hostlonglong >> 32));
    #else
    return hostlonglong;
    #endif
}

static inline uint64_t ntohll(uint64_t netlonglong) {
    return htonll(netlonglong);  // Same operation for conversion
}
#endif

/*
 * Prepare username field (null-pad to USERNAME_SIZE)
 */
static inline void prepare_username(char *dest, const char *src) {
    size_t len = strlen(src);
    if (len >= USERNAME_SIZE) len = USERNAME_SIZE - 1;
    memcpy(dest, src, len);
    memset(dest + len, 0, USERNAME_SIZE - len);
}

#endif /* __PROTOCOL_H__ */
