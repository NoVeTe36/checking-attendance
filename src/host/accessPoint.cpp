#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <map>

#include "ecdsa.h"
#include "ecdh-aes.h"
#include "crypto-random-engine.h"

WebServer server(80);
mbedtls_ecdsa_context server_ecdsa_ctx;
mbedtls_ecdh_context server_ecdh_ctx;

struct CertInfo
{
    String id;
    String ecdsa_public_key_b64;
    uint64_t valid;
    String signature_b64;
    String c_nonce_b64;
    String session_ecdh_public_key_b64;
    String nonce_b64;
    String ciphertext_b64;
    String tag_b64;
};

std::map<String, CertInfo> deviceCerts;

const char *client_pubkey_hex = "048A602EE430C7EB3D5E68FAF4AFC2614DCE42292EAAF764BCEE66CD542EE2B396A283D11BE4E8F4035887942E4F732F6036A0CDCA5E820EB999BCEACA0E7487FD";

void hexToBytes(const char *hex, uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        sscanf(hex + 2 * i, "%2hhx", &bytes[i]);
    }
}

String bytesToHex(const uint8_t *data, size_t len)
{
    String hexStr = "";
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] < 16)
            hexStr += "0";
        hexStr += String(data[i], HEX);
    }
    hexStr.toUpperCase();
    return hexStr;
}

void get_valid_time_string(char *buf, size_t buf_len)
{
    if (buf_len < 20)
        return;
    unsigned long ms = millis();
    snprintf(buf, buf_len, "20230604120045%03lu", ms % 1000);
}

void gen_server_ecdsa_key()
{
    mbedtls_ecdsa_init(&server_ecdsa_ctx);
    mbedtls_ecp_group_load(&server_ecdsa_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    int ret = mbedtls_ecp_gen_keypair(&server_ecdsa_ctx.grp, &server_ecdsa_ctx.d, &server_ecdsa_ctx.Q,
                                      mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        Serial.printf("Failed to generate ECDSA keypair: -0x%04X\n", -ret);
        while (1)
            ;
    }
}

void generate_server_ecdh_keypair()
{
    mbedtls_ecdh_init(&server_ecdh_ctx);
    mbedtls_ecp_group_load(&server_ecdh_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    int ret = mbedtls_ecdh_gen_public(&server_ecdh_ctx.grp, &server_ecdh_ctx.d, &server_ecdh_ctx.Q,
                                      mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        Serial.printf("Failed to generate ECDH keypair: -0x%04X\n", -ret);
        while (1)
            ;
    }
}

void get_server_ecdh_public(uint8_t *pub_key, size_t &pub_key_len)
{
    pub_key_len = 65;
    int ret = mbedtls_ecp_point_write_binary(&server_ecdh_ctx.grp, &server_ecdh_ctx.Q,
                                             MBEDTLS_ECP_PF_UNCOMPRESSED,
                                             &pub_key_len, pub_key, pub_key_len);
    if (ret != 0)
    {
        Serial.printf("Failed to export server ECDH public key: -0x%04X\n", -ret);
        while (1)
            ;
    }
}

void handleInit()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    if (!server.hasArg("plain"))
    {
        server.send(400, "text/plain", "Missing JSON body");
        return;
    }

    String body = server.arg("plain");
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    String id_hex = doc["id"] | "";
    String ecdsa_pubkey_hex = doc["ecdsa_public_key"] | "";
    uint64_t valid = doc["valid"] | 0;
    String signature_hex = doc["signature"] | "";
    String c_nonce_hex = doc["c_nonce"] | "";
    String c_ecdh_pubkey_hex = doc["session_ecdh_public_key"] | "";

    if (id_hex.length() == 0 || ecdsa_pubkey_hex.length() == 0 || signature_hex.length() == 0 ||
        c_nonce_hex.length() == 0 || c_ecdh_pubkey_hex.length() == 0)
    {
        server.send(400, "text/plain", "Missing fields");
        return;
    }

    Serial.printf("Received ID: %s\n", id_hex.c_str());
    Serial.printf("Received ECDSA Public Key: %s\n", ecdsa_pubkey_hex.c_str());
    
    uint8_t id_bytes[6];
    hexToBytes(id_hex.c_str(), id_bytes, 6);

    uint8_t client_pubkey_bytes[65];
    hexToBytes(ecdsa_pubkey_hex.c_str(), client_pubkey_bytes, 65);

    size_t signature_len = signature_hex.length() / 2;
    uint8_t signature_bytes[128];
    hexToBytes(signature_hex.c_str(), signature_bytes, signature_len);

    uint8_t message[71];
    memcpy(message, id_bytes, 6);
    memcpy(message + 6, client_pubkey_bytes, 65);

    uint8_t fixed_client_pubkey[65];
    hexToBytes(client_pubkey_hex, fixed_client_pubkey, 65);
    if (memcmp(client_pubkey_bytes, fixed_client_pubkey, 65) != 0)
    {
        server.send(401, "text/plain", "Client public key mismatch");
        return;
    }

    int ret = verify(message, 71, client_pubkey_bytes, signature_bytes, signature_len);
    if (ret != 0)
    {
        server.send(401, "text/plain", "Invalid signature");
        return;
    }

    const char s_id[6] = {'S', '1', '2', '3', '4', '5'};
    uint8_t server_pubkey_bytes[65];
    size_t pubkey_len = 65;
    get_public_bytes(server_ecdsa_ctx, server_pubkey_bytes, pubkey_len);

    char valid_str[20];
    get_valid_time_string(valid_str, sizeof(valid_str));

    uint8_t s_cert_bytes[6 + 65 + 19];
    cert_bytes((const uint8_t *)s_id, server_pubkey_bytes, (const uint8_t *)valid_str, s_cert_bytes);

    uint8_t s_signature[128];
    size_t s_signature_len;
    sign(server_ecdsa_ctx, s_cert_bytes, sizeof(s_cert_bytes), s_signature, s_signature_len);

    generate_server_ecdh_keypair();
    uint8_t server_ecdh_pubkey[65];
    size_t ecdh_pub_len = 65;
    get_server_ecdh_public(server_ecdh_pubkey, ecdh_pub_len);

    uint8_t c_nonce_bytes[12];
    hexToBytes(c_nonce_hex.c_str(), c_nonce_bytes, 12);

    uint8_t c_ecdh_pubkey_bytes[65];
    hexToBytes(c_ecdh_pubkey_hex.c_str(), c_ecdh_pubkey_bytes, 65);

    uint8_t sig2_msg[12 + 65 + 65];
    memcpy(sig2_msg, c_nonce_bytes, 12);
    memcpy(sig2_msg + 12, c_ecdh_pubkey_bytes, 65);
    memcpy(sig2_msg + 12 + 65, server_ecdh_pubkey, 65);

    uint8_t sig2[128];
    size_t sig2_len;
    sign(server_ecdsa_ctx, sig2_msg, sizeof(sig2_msg), sig2, sig2_len);

    StaticJsonDocument<512> resp;
    resp["s_cert"] = bytesToHex(s_cert_bytes, sizeof(s_cert_bytes));
    resp["session_ecdh_public_key"] = bytesToHex(server_ecdh_pubkey, ecdh_pub_len);
    resp["signature2"] = bytesToHex(sig2, sig2_len);

    String resp_str;
    serializeJson(resp, resp_str);
    server.send(200, "application/json", resp_str);
}

void setup()
{
    Serial.begin(115200);
    init_crypto_random_engine();
    WiFi.softAP("ESP32", "12345678");
    delay(100);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());

    gen_server_ecdsa_key();
    generate_server_ecdh_keypair();

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/plain", "Welcome to ESP32 server"); });

    server.on("/upload", HTTP_POST, handleInit);
    server.begin();
    Serial.println("Server ready.");
}

void loop()
{
    server.handleClient();
}
