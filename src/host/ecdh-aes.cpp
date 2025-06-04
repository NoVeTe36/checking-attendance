#include "ecdh-aes.h"

#include <Arduino.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/md.h"
#include "mbedtls/gcm.h"

#include "crypto-random-engine.h"

int hkdf_sha256(const uint8_t *salt, size_t salt_len,
                const uint8_t *ikm, size_t ikm_len,
                const uint8_t *info, size_t info_len,
                uint8_t *okm, size_t okm_len)
{
    int ret;
    uint8_t prk[32]; // SHA256 output size
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    // Extract
    ret = mbedtls_md_hmac(md, salt, salt_len, ikm, ikm_len, prk);
    if (ret != 0)
        return ret;

    // Expand
    uint8_t t[32];
    size_t t_len = 0;
    uint8_t counter = 1;
    size_t done = 0;

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    ret = mbedtls_md_setup(&ctx, md, 1);
    if (ret != 0)
        return ret;

    while (done < okm_len)
    {
        mbedtls_md_hmac_starts(&ctx, prk, 32);

        if (t_len > 0)
            mbedtls_md_hmac_update(&ctx, t, t_len);

        mbedtls_md_hmac_update(&ctx, info, info_len);
        mbedtls_md_hmac_update(&ctx, &counter, 1);
        mbedtls_md_hmac_finish(&ctx, t);

        size_t copy_len = ((okm_len - done) > 32) ? 32 : (okm_len - done);
        memcpy(okm + done, t, copy_len);

        done += copy_len;
        t_len = 32;
        counter++;
    }

    mbedtls_md_free(&ctx);
    return 0;
}

void gen_key(mbedtls_ecdh_context &ctx)
{
    mbedtls_ecdh_init(&ctx);

    // Setup ECDH context for curve SECP256R1 (aka NIST P-256)
    int ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        Serial.printf("Failed to load curve: -0x%04X\n", -ret);
        while (1)
            ;
    }

    // Generate private and public keypair
    ret = mbedtls_ecdh_gen_public(&ctx.grp, &ctx.d, &ctx.Q, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        Serial.printf("Failed to generate keypair: -0x%04X\n", -ret);
        while (1)
            ;
    }
}

void get_public_bytes(mbedtls_ecdh_context &ctx, uint8_t pub_key[], size_t &pub_key_len)
{
    int ret = mbedtls_ecp_point_write_binary(&ctx.grp, &ctx.Q,
                                             MBEDTLS_ECP_PF_UNCOMPRESSED,
                                             &pub_key_len, pub_key, 65);
    if (ret != 0)
    {
        Serial.printf("Failed to export public key: -0x%04X\n", -ret);
        while (1)
            ;
    }
}

// return 0 if the public bytes is valid and load public key successfully
int decode_public_bytes(mbedtls_ecdh_context &ctx,
                        uint8_t peer_pub_bytes[],
                        mbedtls_ecp_point &peer_pub)
{
    mbedtls_ecp_point_init(&peer_pub);

    int ret = mbedtls_ecp_point_read_binary(&ctx.grp, &peer_pub, peer_pub_bytes, 65);
    if (ret == MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
    {
        Serial.printf("MBEDTLS_ERR_ECP_BAD_INPUT_DATA\n");
        return -1;
    }
    if (ret == MBEDTLS_ERR_MPI_ALLOC_FAILED)
    {
        Serial.printf("MBEDTLS_ERR_MPI_ALLOC_FAILED\n");
        return -1;
    }
    if (ret == MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE)
    {
        Serial.printf("MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE\n");
        return -1;
    }
    if (ret != 0)
    {
        Serial.printf("Invalid public key\n");
        return -1;
    }
    return 0;
}
// return 0 if the public bytes is valid and get shared secret successfully
int get_shared_secret(mbedtls_ecdh_context &ctx, uint8_t peer_pub_bytes[], uint8_t shared_secret[])
{
    mbedtls_ecp_point peer_pub;
    mbedtls_ecp_point_init(&peer_pub);
    int ret = mbedtls_ecp_point_read_binary(&ctx.grp, &peer_pub, peer_pub_bytes, 65);
    if (ret == MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
    {
        Serial.printf("MBEDTLS_ERR_ECP_BAD_INPUT_DATA\n");
        return -1;
    }
    if (ret == MBEDTLS_ERR_MPI_ALLOC_FAILED)
    {
        Serial.printf("MBEDTLS_ERR_MPI_ALLOC_FAILED\n");
        return -1;
    }
    if (ret == MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE)
    {
        Serial.printf("MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE\n");
        return -1;
    }
    if (ret != 0)
    {
        Serial.printf("Invalid public key\n");
        return -1;
    }

    mbedtls_mpi shared;
    mbedtls_mpi_init(&shared);
    ret = mbedtls_ecdh_compute_shared(&ctx.grp, &shared, &peer_pub, &ctx.d, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        Serial.printf("Failed to compute shared secret\n");
        return -1;
    }
    mbedtls_mpi_write_binary(&shared, shared_secret, 32);
    mbedtls_mpi_free(&shared);
    mbedtls_ecp_point_free(&peer_pub);
    return 0;
}

void get_shared_key(const uint8_t shared_secret[], uint8_t shared_key[])
{
    unsigned char *salt = NULL; // can be NULL or some random bytes
    unsigned char info[] = "handshake data";

    int ret = hkdf_sha256(
        salt, 0,
        shared_secret, 32,
        info, sizeof(info) - 1,
        shared_key, 32);
}

void encrypt(const uint8_t shared_key[], const uint8_t plaintext[], size_t plaintext_len, uint8_t nonce[], uint8_t ciphertext[], uint8_t tag[])
{
    for (int i = 0; i < 12; i++)
        nonce[i] = random(0, 256);
    mbedtls_gcm_context gcm;

    mbedtls_gcm_init(&gcm);

    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, shared_key, 256) != 0)
    {
        Serial.println("Failed to set AES key");
        while (1)
            ;
    }

    if (esp_aes_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                  plaintext_len, nonce, 12,
                                  NULL, 0,
                                  plaintext, ciphertext,
                                  16, tag) != 0)
    {
        Serial.println("AES-GCM encryption failed");
        while (1)
            ;
    }
    mbedtls_gcm_free(&gcm);
}

void decrypt(const uint8_t shared_key[],
             const uint8_t nonce[],
             const uint8_t ciphertext[],
             const uint8_t tag[],
             uint8_t plaintext[], size_t plaintext_len)
{
    // Decrypt to verify
    mbedtls_gcm_context gcm;

    mbedtls_gcm_init(&gcm);

    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, shared_key, 256) != 0)
    {
        Serial.println("Failed to set AES key");
        while (1)
            ;
    }

    if (mbedtls_gcm_auth_decrypt(&gcm, plaintext_len,
                                 nonce, 12,
                                 NULL, 0,
                                 tag, 16,
                                 ciphertext, plaintext) != 0)
    {
        Serial.println("AES-GCM decryption failed");
        while (1)
            ;
    }

    mbedtls_gcm_free(&gcm);
}
