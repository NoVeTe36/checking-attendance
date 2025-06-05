#ifndef CRYPTO_RANDOM_ENGINE_H
#define CRYPTO_RANDOM_ENGINE_H

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

extern mbedtls_entropy_context entropy;
extern mbedtls_ctr_drbg_context ctr_drbg;

void init_crypto_random_engine();


#endif