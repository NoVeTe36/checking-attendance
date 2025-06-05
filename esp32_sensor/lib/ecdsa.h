#ifndef ECDSA_H
#define ECDSA_H
#include <stdint.h>
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk.h"

void gen_signature_key(mbedtls_ecdsa_context &ctx);

// return 0 if succesfull
int load_private_key(const uint8_t data[], size_t data_len,
                     mbedtls_ecdsa_context **ctx);

// return 0 if successfull
int export_private_key(const mbedtls_ecdsa_context &ctx,
                       uint8_t pem_buf[],
                       size_t pem_buf_size);

void get_public_bytes(mbedtls_ecdsa_context &ctx, uint8_t pub_key[], size_t &pub_key_len);

void sign(mbedtls_ecdsa_context &ctx,
          const uint8_t message[],
          size_t message_len,
          uint8_t signature[],
          size_t &signature_len);

// Return 0 if the signature is valid
int verify(const uint8_t message[],
           size_t message_len,
           uint8_t peer_pub_bytes[],
           const uint8_t signature[],
           size_t signature_len);

void cert_bytes(const uint8_t id[],
                const uint8_t public_bytes[],
                const uint8_t valid_until[],
                uint8_t cert_bytes[]);

void session_bytes(const uint8_t nonce[],
                   const uint8_t c_pub_bytes[],
                   const uint8_t s_pub_bytes[],
                   uint8_t session_bytes[]);

#endif