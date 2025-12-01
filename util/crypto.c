// Crypto functions using OpenSSL

#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <string.h>
#include <stdio.h>

int aes_encrypt(const unsigned char *key,
                const unsigned char *plaintext, size_t plaintext_len,
                unsigned char *ciphertext, size_t *ciphertext_len,
                unsigned char *iv)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int ciphertext_len_int = 0;

    if (RAND_bytes(iv, IV_SIZE) != 1) {
        return -1;
    }

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        return -1;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len_int = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len_int += len;

    *ciphertext_len = ciphertext_len_int;

    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int aes_decrypt(const unsigned char *key,
                const unsigned char *ciphertext, size_t ciphertext_len,
                const unsigned char *iv,
                unsigned char *plaintext, size_t *plaintext_len)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int plaintext_len_int = 0;

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len_int = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len_int += len;

    *plaintext_len = plaintext_len_int;

    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int hmac_sha256(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                unsigned char *hmac_out)
{
    unsigned int hmac_len = 0;

    if (HMAC(EVP_sha256(), key, KEY_SIZE, data, data_len, hmac_out, &hmac_len) == NULL) {
        return -1;
    }

    if (hmac_len != HMAC_SIZE) {
        return -1;
    }

    return 0;
}

int hmac_verify(const unsigned char *key,
                const unsigned char *data, size_t data_len,
                const unsigned char *expected_hmac)
{
    unsigned char computed_hmac[HMAC_SIZE];

    if (hmac_sha256(key, data, data_len, computed_hmac) != 0) {
        return -1;
    }

    if (CRYPTO_memcmp(computed_hmac, expected_hmac, HMAC_SIZE) != 0) {
        return -1;
    }

    return 0;
}

int generate_random_bytes(unsigned char *buffer, size_t length)
{
    if (RAND_bytes(buffer, length) != 1) {
        return -1;
    }
    return 0;
}

int compute_auth_token(const unsigned char *card_secret,
                       const char *pin,
                       unsigned char *auth_token)
{
    unsigned char combined[36];
    memcpy(combined, card_secret, CARD_SECRET_SIZE);
    memcpy(combined + CARD_SECRET_SIZE, pin, 4);

    if (hmac_sha256(card_secret, combined, 36, auth_token) != 0) {
        return -1;
    }

    return 0;
}
