// Crypto functions for encrypting/authenticating messages

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <stddef.h>

#define KEY_SIZE 32        // 256 bits for AES-256
#define IV_SIZE 16         // 128 bits for AES block size
#define HMAC_SIZE 32       // 256 bits for SHA-256
#define CARD_SECRET_SIZE 32 // 256 bits for card secret

// Encrypt with AES-256-CBC (generates random IV)
int aes_encrypt(const unsigned char *key,
                const unsigned char *plaintext, size_t plaintext_len,
                unsigned char *ciphertext, size_t *ciphertext_len,
                unsigned char *iv);

// Decrypt with AES-256-CBC
int aes_decrypt(const unsigned char *key,
                const unsigned char *ciphertext, size_t ciphertext_len,
                const unsigned char *iv,
                unsigned char *plaintext, size_t *plaintext_len);

// Compute HMAC-SHA256
int hmac_sha256(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                unsigned char *hmac_out);

// Verify HMAC matches
int hmac_verify(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                const unsigned char *expected_hmac);

// Generate random bytes
int generate_random_bytes(unsigned char *buffer, size_t length);

// Compute auth token = HMAC(card_secret, card_secret || PIN)
int compute_auth_token(const unsigned char *card_secret,
                       const char *pin,
                       unsigned char *auth_token);

#endif
