#ifndef ECDH_AES_H
#define ECDH_AES_H

#include "mbedtls/ecdh.h"

void gen_key(mbedtls_ecdh_context &ctx);

void get_public_bytes(mbedtls_ecdh_context &ctx,
                      uint8_t pub_key[],
                      size_t &pub_key_len);

// return 0 if the public bytes is valid and get shared secret successfully
int get_shared_secret(mbedtls_ecdh_context &ctx,
                      uint8_t peer_pub_bytes[],
                      uint8_t shared_secret[]);

void get_shared_key(const uint8_t shared_secret[], uint8_t shared_key[]);

void encrypt(const uint8_t shared_key[],
             const uint8_t plaintext[], size_t plaintext_len,
             uint8_t nonce[], uint8_t ciphertext[], uint8_t tag[]);

void decrypt(const uint8_t shared_key[],
             const uint8_t nonce[],
             const uint8_t ciphertext[],
             const uint8_t tag[],
             uint8_t plaintext[], size_t plaintext_len);

#endif