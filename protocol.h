// Protocol message definitions
// Encrypted format: IV (16) || ciphertext || HMAC (32)

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <arpa/inet.h>

#define MSG_LOGIN_REQ       0x01
#define MSG_LOGIN_RESP      0x02
#define MSG_BALANCE_REQ     0x03
#define MSG_BALANCE_RESP    0x04
#define MSG_WITHDRAW_REQ    0x05
#define MSG_WITHDRAW_RESP   0x06

// Sizes
#define USERNAME_SIZE       251     // Maximum username length + null terminator
#define AUTH_TOKEN_SIZE     32      // HMAC-SHA256 of (card_secret || PIN)
#define PIN_SIZE            4       // PIN is exactly 4 digits

typedef struct {
    uint8_t msg_type;               // One of MSG_* constants
    char username[USERNAME_SIZE];   // Null-padded username
} __attribute__((packed)) msg_header_t;

// Login request
typedef struct {
    msg_header_t header;
    uint8_t auth_token[AUTH_TOKEN_SIZE];  // HMAC(card_secret, card_secret || PIN)
    char pin[PIN_SIZE];                   // 4-digit PIN (not null-terminated)
    uint64_t seq_num;                     // Sequence number for replay protection
} __attribute__((packed)) msg_login_req_t;

// Login response
typedef struct {
    msg_header_t header;
    uint8_t success;                // 1 = authorized, 0 = not authorized
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_login_resp_t;

// Balance request
typedef struct {
    msg_header_t header;
    uint64_t seq_num;               // Sequence number for replay protection
} __attribute__((packed)) msg_balance_req_t;

// Balance response
typedef struct {
    msg_header_t header;
    int32_t balance;                // Current balance (signed 32-bit)
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_balance_resp_t;

// Withdraw request
typedef struct {
    msg_header_t header;
    int32_t amount;                 // Amount to withdraw (signed 32-bit)
    uint64_t seq_num;               // Sequence number for replay protection
} __attribute__((packed)) msg_withdraw_req_t;

// Withdraw response
typedef struct {
    msg_header_t header;
    uint8_t success;                // 1 = success, 0 = insufficient funds
    int32_t new_balance;            // New balance after withdrawal
    uint64_t seq_num;               // Echo back the sequence number
} __attribute__((packed)) msg_withdraw_resp_t;

#define MAX_PLAINTEXT_SIZE  512
#define IV_SIZE             16
#define HMAC_SIZE           32
#define MAX_ENCRYPTED_SIZE  (IV_SIZE + MAX_PLAINTEXT_SIZE + 16 + HMAC_SIZE)

// macOS has htonll/ntohll but Linux doesn't
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
    return htonll(netlonglong);
}
#endif

// Pad username with nulls
static inline void prepare_username(char *dest, const char *src) {
    size_t len = strlen(src);
    if (len >= USERNAME_SIZE) len = USERNAME_SIZE - 1;
    memcpy(dest, src, len);
    memset(dest + len, 0, USERNAME_SIZE - len);
}

#endif /* __PROTOCOL_H__ */
