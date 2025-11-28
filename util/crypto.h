/*
 * Crypto utilities for ATM/Bank protocol (Idea 1)
 * 
 * Uses AES-256-CBC for encryption and HMAC-SHA256 for authentication.
 * All messages are encrypted then MAC'd (Encrypt-then-MAC).
 */

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <stddef.h>

#define KEY_SIZE 32        // 256 bits for AES-256
#define IV_SIZE 16         // 128 bits for AES block size
#define HMAC_SIZE 32       // 256 bits for SHA-256
#define CARD_SECRET_SIZE 32 // 256 bits for card secret

/*
 * Encrypt plaintext using AES-256-CBC
 * 
 * Parameters:
 *   key: 32-byte encryption key
 *   plaintext: data to encrypt
 *   plaintext_len: length of plaintext
 *   ciphertext: output buffer (must be large enough: plaintext_len + 16 for padding)
 *   ciphertext_len: output parameter for ciphertext length
 *   iv: 16-byte initialization vector (will be generated randomly)
 * 
 * Returns: 0 on success, -1 on error
 */
int aes_encrypt(const unsigned char *key,
                const unsigned char *plaintext, size_t plaintext_len,
                unsigned char *ciphertext, size_t *ciphertext_len,
                unsigned char *iv);

/*
 * Decrypt ciphertext using AES-256-CBC
 * 
 * Parameters:
 *   key: 32-byte encryption key
 *   ciphertext: data to decrypt
 *   ciphertext_len: length of ciphertext
 *   iv: 16-byte initialization vector (from encryption)
 *   plaintext: output buffer (must be at least ciphertext_len bytes)
 *   plaintext_len: output parameter for plaintext length
 * 
 * Returns: 0 on success, -1 on error
 */
int aes_decrypt(const unsigned char *key,
                const unsigned char *ciphertext, size_t ciphertext_len,
                const unsigned char *iv,
                unsigned char *plaintext, size_t *plaintext_len);

/*
 * Compute HMAC-SHA256 over data
 * 
 * Parameters:
 *   key: 32-byte HMAC key
 *   data: data to authenticate
 *   data_len: length of data
 *   hmac_out: output buffer (must be 32 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int hmac_sha256(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                unsigned char *hmac_out);

/*
 * Verify HMAC-SHA256
 * 
 * Parameters:
 *   key: 32-byte HMAC key
 *   data: data that was authenticated
 *   data_len: length of data
 *   expected_hmac: HMAC to verify (32 bytes)
 * 
 * Returns: 0 if HMAC is valid, -1 if invalid or error
 */
int hmac_verify(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                const unsigned char *expected_hmac);

/*
 * Generate cryptographically secure random bytes
 * 
 * Parameters:
 *   buffer: output buffer
 *   length: number of random bytes to generate
 * 
 * Returns: 0 on success, -1 on error
 */
int generate_random_bytes(unsigned char *buffer, size_t length);

/*
 * Compute HMAC of card_secret concatenated with PIN
 * Used for authentication: only knowing card OR PIN is insufficient
 * 
 * Parameters:
 *   card_secret: 32-byte card secret
 *   pin: 4-character PIN string
 *   auth_token: output buffer (must be 32 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int compute_auth_token(const unsigned char *card_secret,
                       const char *pin,
                       unsigned char *auth_token);

#endif
