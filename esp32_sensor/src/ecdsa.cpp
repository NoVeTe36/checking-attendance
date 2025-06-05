#include "ecdsa.h"

#include <Arduino.h>
#include "mbedtls/sha256.h"
#include "mbedtls/error.h"

#include "crypto-random-engine.h"

void gen_signature_key(mbedtls_ecdsa_context &ctx)
{
    mbedtls_ecdsa_init(&ctx);

    // Load the curve (secp256r1)
    int ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0)
    {
        Serial.printf("mbedtls_ecp_group_load failed: -0x%04x\n", -ret);
        while (1)
            ;
    }

    ret = mbedtls_ecp_gen_keypair(&ctx.grp, &ctx.d, &ctx.Q,
                                  mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        Serial.printf("mbedtls_ecp_gen_keypair failed: -0x%04x\n", -ret);
        while (1)
            ;
    }
}

// return 0 if succesfull
int load_private_key(const uint8_t data[], size_t data_len, mbedtls_ecdsa_context **ctx)
{

    mbedtls_pk_context pk_priv;
    int ret = mbedtls_pk_parse_key(&pk_priv, data,
                                   data_len, NULL, 0);
    if (ret != 0)
    {
        char error_buf[128];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        Serial.printf("Key parse failed: %s\n", error_buf);
        return ret;
    }

    *ctx = mbedtls_pk_ec(pk_priv);
    return 0;
}

// return 0 if successfull
int export_private_key(const mbedtls_ecdsa_context &ctx,
                       uint8_t pem_buf[],
                       size_t pem_buf_size)
{
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    // Setup pk context for EC key type
    if (mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0)
    {
        Serial.println("Failed to setup pk context");
        return -1;
    }

    mbedtls_ecdsa_context *copy_ctx = mbedtls_pk_ec(pk);
    mbedtls_ecdsa_init(copy_ctx);

    int ret = mbedtls_ecp_group_copy(&copy_ctx->grp, &ctx.grp);
    if (ret != 0)
    {
        Serial.printf("cannot copy group %d\n", ret);
        return ret;
    }

    ret = mbedtls_mpi_copy(&copy_ctx->d, &ctx.d);
    if (ret != 0)
    {
        Serial.printf("cannot copy private key %d\n", ret);
        return ret;
    }

    ret = mbedtls_ecp_copy(&copy_ctx->Q, &ctx.Q);
    if (ret != 0)
    {
        Serial.printf("cannot copy public key %d\n", ret);
        return ret;
    }

    // Prepare buffer for PEM output
    ret = mbedtls_pk_write_key_pem(&pk, pem_buf, pem_buf_size);

    mbedtls_pk_free(&pk);

    if (ret != 0)
    {
        Serial.printf("mbedtls_pk_write_key_pem returned %d\n", ret);
        return ret;
    }
    return 0;
}

void get_public_bytes(mbedtls_ecdsa_context &ctx, uint8_t pub_key[], size_t &pub_key_len)
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

void cert_bytes(const uint8_t id[],
                const uint8_t public_bytes[],
                const uint8_t valid_until[],
                uint8_t cert_bytes[])
{
    memcpy(cert_bytes, id, 6);
    memcpy(cert_bytes + 6, public_bytes, 65);
    memcpy(cert_bytes + 6 + 65, valid_until, 19);
}

void session_bytes(const uint8_t nonce[],
                   const uint8_t c_pub_bytes[],
                   const uint8_t s_pub_bytes[],
                   uint8_t session_bytes[])
{
    memcpy(session_bytes, nonce, 12);
    memcpy(session_bytes + 12, c_pub_bytes, 65);
    memcpy(session_bytes + 12 + 65, s_pub_bytes, 65);
}

void sign(mbedtls_ecdsa_context &ctx,
          const uint8_t message[],
          size_t message_len,
          uint8_t signature[],
          size_t &signature_len)
{
    uint8_t hash[32];
    mbedtls_sha256_ret(message, message_len, hash, 0);

    if (mbedtls_ecdsa_write_signature(&ctx, MBEDTLS_MD_SHA256,
                                      hash, sizeof(hash),
                                      signature, &signature_len,
                                      mbedtls_ctr_drbg_random, &ctr_drbg) != 0)
    {
        Serial.println("Signing failed");
        while (1)
            ;
    }
}

// Return 0 if the signature is valid
int verify(const uint8_t message[],
           size_t message_len,
           uint8_t peer_pub_bytes[],
           const uint8_t signature[],
           size_t signature_len)
{
    mbedtls_ecdsa_context ctx;
    mbedtls_ecdsa_init(&ctx);

    if (mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_SECP256R1) != 0)
    {
        Serial.println("Curve load failed");
        while (1)
            ;
    }

    int ret = mbedtls_ecp_point_read_binary(&ctx.grp, &ctx.Q, peer_pub_bytes, 65);
    if (ret != 0)
    {
        Serial.printf("Public key load failed: -0x%04x\n", -ret);
        return -1;
    }

    uint8_t hash[32];
    mbedtls_sha256_ret(message, message_len, hash, 0);

    ret = mbedtls_ecdsa_read_signature(&ctx, hash, sizeof(hash), signature, signature_len);
    mbedtls_ecdsa_free(&ctx);
    return ret;
}