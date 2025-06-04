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
String pub_key_hex;
String id = "000002";
String token = "123456";

String server_valid_until;
String server_cert_signature;
String ca_pub;


std::map<String, mbedtls_ecdh_context> sessions_ecdh;

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

void esp32_sigup()
{
    gen_signature_key(server_ecdsa_ctx);

    uint8_t pub_key[65];
    get_public_bytes(server_ecdsa_ctx, pub_key);
    pub_key_hex = bytesToHex(pub_key, 65);

    String postData = "{\"id\": \"" + id "\", \"token\": \"" + token + "\", \"pub_key\": \""
    + pub_key_hex + "\"}";

    HTTPClient http;
    http.begin("http://192.168.86.64:5000/sing_cert");                              // Specify the URL
    http.addHeader("Content-Type", "application/json"); // Specify content-type header

    int httpResponseCode = http.POST(postData);
    if (httpResponseCode == 200)
    {
        String response = http.getString(); // Get the response payload
        Serial.println("Response: " + response);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, response);
        if (error)
        {
            Serial.println("Failed to parse JSON response");
            return;
        }
        server_valid_until = doc["valid_until"];
        ca_pub = doc["ca_pub"];
        server_cert_signature = doc["signature"];
    }
    else
    {
        Serial.printf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }
}

void handle_handshake()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String requestBody = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String id = doc["id"];
    String valid_until = doc["valid_until"];
    String pub = doc["pub"];
    String signature = doc["signature"];
    String c_nonce = doc["c_nonce"];
    String session = doc["session"];

    uint8_t pub_bytes[65];
    hexToBytes(pub.c_str(), pub_bytes, 65);
    uint8_t cert_bytes[96];
    cert_bytes(id.c_str(), pub_bytes, valid_until.c_str(), cert_bytes);

    if (verify(cert_bytes, 96, ca_pub, signature.c_str(), signature.length()) != 0)
    {
        server.send(303, "application/json", "{\"status\":\"failed\"}");
        return;
    }
    uint8_t s_nonce[12];
    for (int i = 0; i < 12; i++)
        s_nonce[i] = random(0, 256);

    
    mbedtls_ecdh_context session_ctx;
    gen_key(session_ctx);
    uint8_t session_pub_bytes[65];
    get_public_bytes(session_ctx, session_pub_bytes);
    sessions_ecdh[id] = session_ctx;
    
    uint8_t sesion_bytes[142];
    session_bytes(c_nonce.c_str(), pub_bytes, session_pub_bytes, sesion_bytes);

    uint8_t session_signature[80];
    size_t session_signature_len;;
    sign(server_ecdsa_ct, session_bytes, 142, session_signature, session_signature_len);
    String data = "{\"id\":\"" + id + "\",\"valid_until\":\"" + server_valid_until + "\",\"pub\":\"" + pub_key_hex + "\", \"s_nonce\":\"" + s_nonce + "\",\"session\":\"" + bytesToHex(session_pub_bytes, 65) + "\", \"session_signature\":\"" + bytesToHex(session_signature, session_signature_len) + "\", \"cert_signature\":\"" + server_cert_signature + "\"}";
    server.send(200, "application/json", data);
}


void handle_authenticate()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String requestBody = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String id = doc["id"];
    String signature = doc["signature"];
    String pub = doc["pub"];

    uint8_t pub_bytes[65];
    hexToBytes(pub.c_str(), pub_bytes, 65);

    if (sessions_ecdh.find(id) == sessions_ecdh.end())
    {
        server.send(404, "application/json", "{\"error\":\"Session not found\"}");
        return;
    }

    mbedtls_ecdh_context &session_ctx = sessions_ecdh[id];
    
    uint8_t session_bytes[142];
    
    if (verify(session_bytes, 142, pub_bytes, hexToBytes(signature.c_str(), signature.length()), signature.length()/2) != 0)
    {
        server.send(403, "application/json", "{\"error\":\"Invalid signature\"}");
        return;
    }

    server.send(200, "application/json", "{\"status\":\"succesfull\"}");

    // valid user 

    Serial.println("User authenticated successfully");

}

void setup()
{
    Serial.begin(115200);
    init_crypto_random_engine();

    Wifi.mode(WIFI_AP_STA);

    const char *ssid_sta = "pumkinpatch2";
    const char *password_sta = "Thisisridiculous!";

    WiFi.mode(WIFI_AP_STA);

    // Connect to WiFi network (STA)
    WiFi.begin(ssid_sta, password_sta);
    Serial.println("Connecting to external WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    WiFi.softAP("ESP32", "12345678");
    delay(100);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());

    init_crypto_random_engine();

    esp32_signup();

    generate_server_ecdh_keypair();

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/plain", "Welcome to ESP32 server"); });

    server.on("/handshake", HTTP_POST, handle_handshake);
    server.on("/authenticate", HTTP_POST, handle_authenticate);
    server.begin();
    Serial.println("Server ready.");
}

void loop()
{
    server.handleClient();
}
