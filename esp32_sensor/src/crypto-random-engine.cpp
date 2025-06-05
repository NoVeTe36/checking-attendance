#include "crypto-random-engine.h"
#include <Arduino.h>


mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;



void init_crypto_random_engine()
{
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char *pers = "crypto_random_engine";
    
    // Initialize RNG
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *)pers, strlen(pers));
    if (ret != 0)
    {
        Serial.printf("Failed to init random engine: -0x%04X\n", -ret);
        while (1)
            ;
    }
}