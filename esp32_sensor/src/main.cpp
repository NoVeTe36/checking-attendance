#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <HTTPClient.h>
#include "ecdsa.h"
#include "ecdh-aes.h"
#include "crypto-random-engine.h"

#include "FS.h"
#include "SPIFFS.h"

bool already_setup;

WebServer server(80);

String internal_wifi_ssid;
String internal_wifi_password;

String central_server_ip;

String public_wifi_ssid;
String public_wifi_password;

String sensor_id;
mbedtls_ecdsa_context *server_ecdsa;
String server_pub_key;
String server_valid_until;
String server_cert_signature;
String ca_pub;

std::map<String, mbedtls_ecdh_context> sessions_ecdh;
std::map<String, String> sessions_s_nonce;

// Pin definitions
const int SENSOR_PIN = 22;   // MH Infrared Obstacle Sensor OUT pin
const int RED_LED_PIN = 19;  // Red LED pin
const int BLUE_LED_PIN = 18; // Blue LED pin

// Variables
bool lastSensorState = HIGH;
unsigned long lastDetectionTime = 0;
const unsigned long DEBOUNCE_DELAY = 2000;

// LED timing variables
unsigned long ledStartTime = 0;
bool ledActive = false;

// Sensor detection state
bool motionDetected = false;
unsigned long motionStartTime = 0;
const unsigned long MOTION_WAIT_TIMEOUT = 6000; // 6 seconds

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

void esp32_signup(String server_ip, String id, String token)
{
    mbedtls_ecdsa_context new_server_ecdsa;
    gen_signature_key(new_server_ecdsa);
    uint8_t pub_key[65];
    size_t pub_key_len;
    get_public_bytes(new_server_ecdsa, pub_key, pub_key_len);
    server_pub_key = bytesToHex(pub_key, 65);

    String postData = "{\"id\": \"" + id + "\", \"token\": \"" + token + "\", \"pub_key\": \"" + server_pub_key + "\"}";

    HTTPClient http;
    String url = "http://" + server_ip + "/sign_cert";
    Serial.println("signup at");
    Serial.println(url);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(postData);
    if (httpResponseCode != 200)
    {
        Serial.printf("Error on signing up esp32: %s\n", http.errorToString(httpResponseCode).c_str());
        return;
    }

    String response = http.getString(); // Get the response payload
    Serial.println("Response: " + response);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
        Serial.println("Failed to parse JSON response");
        return;
    }

    server_valid_until = doc["valid_until"].as<const char *>();
    server_cert_signature = doc["signature"].as<const char *>();
    ca_pub = doc["ca_pub"].as<const char *>();

    static uint8_t pem_buf[1600];
    export_private_key(new_server_ecdsa, pem_buf, 1600);
    File file = SPIFFS.open("/server.pem", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.write(pem_buf, strlen((const char *)pem_buf));
    file.close();
    Serial.println("private written");

    file = SPIFFS.open("/server.pub", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(server_pub_key);
    file.close();
    Serial.println("server public written");

    file = SPIFFS.open("/ca.pub", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(ca_pub);
    file.close();
    Serial.println("ca pub written");

    file = SPIFFS.open("/signature", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(server_cert_signature);
    file.close();
    Serial.println("server_cert_signature written");
}

void handle_handshake()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String requestBody = server.arg("plain");
    Serial.println(requestBody);

    DynamicJsonDocument doc(4098);
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
    String other_session_key = doc["session"];
    Serial.println("ok get");

    uint8_t pub_bytes[65];

    Serial.println("nanh pub");
    Serial.println(pub);

    hexToBytes(pub.c_str(), pub_bytes, 65);
    uint8_t cert[90];
    cert_bytes((uint8_t *)id.c_str(), pub_bytes, (uint8_t *)valid_until.c_str(), cert);

    uint8_t ca_pub_bytes[65];
    hexToBytes(ca_pub.c_str(), ca_pub_bytes, 65);

    uint8_t signature_bytes[80];
    hexToBytes(signature.c_str(), signature_bytes, signature.length() / 2);

    if (verify(cert, 90, ca_pub_bytes, signature_bytes, signature.length() / 2) != 0)
    {
        server.send(303, "application/json", "{\"status\":\"failed\"}");
        Serial.println("failed verify");

        return;
    }
    Serial.println("verify cert ok ");

    uint8_t s_nonce[12];
    for (int i = 0; i < 12; i++)
        s_nonce[i] = random(0, 256);

    sessions_s_nonce[id] = bytesToHex(s_nonce, 12);

    mbedtls_ecdh_context session_ctx;
    gen_key(session_ctx);
    uint8_t session_pub_bytes[65];
    size_t session_pub_bytes_len;
    get_public_bytes(session_ctx, session_pub_bytes, session_pub_bytes_len);
    sessions_ecdh[id] = session_ctx;
    Serial.println("gen key, add seesion id ok  ");
    Serial.println(bytesToHex(session_pub_bytes, 65));

    uint8_t sesion_bytes_buf[142];
    uint8_t c_nonce_bytes[12];
    hexToBytes(c_nonce.c_str(), c_nonce_bytes, 12);
    uint8_t other_session_key_bytes[65];
    hexToBytes(other_session_key.c_str(), other_session_key_bytes, 65);
    session_bytes(c_nonce_bytes, other_session_key_bytes, session_pub_bytes, sesion_bytes_buf);

    uint8_t session_signature[80];
    size_t session_signature_len;

    sign(*server_ecdsa, sesion_bytes_buf, 142, session_signature, session_signature_len);

    String data = "{\"id\":\"000002\", \"valid_until\":\"" + server_valid_until + "\",\"pub\":\"" + server_pub_key + "\", \"s_nonce\":\"" + bytesToHex(s_nonce, 12) + "\",\"session\":\"" + bytesToHex(session_pub_bytes, 65) + "\", \"session_signature\":\"" + bytesToHex(session_signature, session_signature_len) + "\", \"cert_signature\":\"" + server_cert_signature + "\"}";
    server.send(200, "application/json", data);
    Serial.println("sign send back  ");
}

void handle_authenticate()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String requestBody = server.arg("plain");
    Serial.println(requestBody);

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
    String other_session = doc["session"];

    Serial.println("authen get ok  ");

    uint8_t pub_bytes[65];
    hexToBytes(pub.c_str(), pub_bytes, 65);

    uint8_t other_session_bytes[65];
    hexToBytes(other_session.c_str(), other_session_bytes, 65);

    if (sessions_ecdh.find(id) == sessions_ecdh.end())
    {
        server.send(404, "application/json", "{\"error\":\"Session not found\"}");
        Serial.print("cannot get session id ");

        return;
    }

    String &session_s_nonce = sessions_s_nonce[id];

    Serial.println("s_nonce");
    Serial.println(session_s_nonce);

    uint8_t s_nonce_bytes[12];
    hexToBytes(session_s_nonce.c_str(), s_nonce_bytes, 12);

    mbedtls_ecdh_context &session_ctx = sessions_ecdh[id];
    uint8_t session_key_pub_bytes[65];
    size_t session_key_pub_bytes_len;
    get_public_bytes(session_ctx, session_key_pub_bytes, session_key_pub_bytes_len);

    Serial.println("server session key");
    Serial.println(bytesToHex(session_key_pub_bytes, 65));

    Serial.println("s_nonce");
    Serial.println(session_s_nonce);

    uint8_t session_message_bytes[142];
    session_bytes(s_nonce_bytes, other_session_bytes, session_key_pub_bytes, session_message_bytes);

    uint8_t signature_bytes[80];
    hexToBytes(signature.c_str(), signature_bytes, signature.length() / 2);

    if (verify(session_message_bytes, 142, pub_bytes, signature_bytes, signature.length() / 2) != 0)
    {
        Serial.print("verify session failed");

        server.send(403, "application/json", "{\"error\":\"Invalid signature\"}");
        return;
    }

    server.send(200, "application/json", "{\"status\":\"succesfull\"}");

    // valid user
    Serial.println("User authenticated successfully");


    HTTPClient http;
    String url = "http://" + central_server_ip + "/checkin";
    Serial.println("signup at");
    Serial.println(url);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    String postData = "{\"cert_id\": \"" + id + "\"}";
    int httpResponseCode = http.POST(postData);
    if (httpResponseCode == 200)
    {
        Serial.printf("told central server ok");
        motionDetected = false;
    } else {
        Serial.printf("central server said fake user");
        ;
    } 

}

void load_config()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed and formatted");
    }
    else
    {
        Serial.println("SPIFFS Mounted successfully");
    }

    String config_path = "/config.json";
    if (!SPIFFS.exists(config_path))
    {
        Serial.println("not setup");
        already_setup = false;
        return;
    }

    Serial.println("already setup");
    already_setup = true;

    File config_file = SPIFFS.open(config_path);
    size_t config_file_size = config_file.size();

    DynamicJsonDocument doc(config_file_size * 2);
    DeserializationError error = deserializeJson(doc, config_file);
    if (error)
    {
        Serial.println("Failed to parse JSON response");
        return;
    }

    internal_wifi_ssid = doc["internal_wifi_ssid"].as<String>();
    internal_wifi_password = doc["internal_wifi_password"].as<String>();

    public_wifi_ssid = doc["public_wifi_ssid"].as<String>();
    public_wifi_password = doc["public_wifi_password"].as<String>();

    sensor_id = doc["sensor_id"].as<String>();
    central_server_ip = doc["central_server_ip"].as<String>();
    server_valid_until = doc["valid_until"].as<String>();

    File server_private_key_file = SPIFFS.open("/server.pem");
    if (!server_private_key_file)
    {
        Serial.println("Failed to open server.pem for reading");
        return;
    }
    String pem_string = server_private_key_file.readString();
    Serial.println(pem_string);
    server_private_key_file.close();
    load_private_key((uint8_t *) pem_string.c_str(), pem_string.length() + 1, &server_ecdsa);

    File server_cert_signature_file = SPIFFS.open("/signature");
    if (!server_cert_signature_file)
    {
        Serial.println("Failed to open signature for reading");
        return;
    }
    server_cert_signature = server_cert_signature_file.readString();

    File server_pub_file = SPIFFS.open("/server.pub");
    if (!server_pub_file)
    {
        Serial.println("Failed to open server.pub for reading");
        return;
    }
    server_pub_key = server_pub_file.readString();

    File ca_pub_file = SPIFFS.open("/ca.pub");
    if (!ca_pub_file)
    {
        Serial.println("Failed to open ca.pub for reading");
        return;
    }
    ca_pub = ca_pub_file.readString();
}

void setup_wifi()
{
    if (already_setup)
    {
        WiFi.mode(WIFI_AP_STA);
        // Connect to internal WiFi network
        WiFi.begin(internal_wifi_ssid, internal_wifi_password);
        Serial.println("Connecting to ixternal WiFi...");
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }

        // Broadcast public hotspot
        WiFi.softAP(public_wifi_ssid, public_wifi_password);
        delay(100);
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
    }
    else
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Default ESP", "12345678");
        delay(100);
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
    }
}

void handle_setup()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    String requestBody = server.arg("plain");
    Serial.println("Set up with config");
    Serial.println(requestBody);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error)
    {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String internal_wifi_ssid_to_use = doc["internal_wifi_ssid"].as<String>();
    String internal_wifi_password_to_use = doc["internal_wifi_password"].as<String>();
    String central_server_ip_to_use = doc["central_server_ip"].as<String>();
    String public_wifi_ssid_to_use = doc["public_wifi_ssid"].as<String>();
    String public_wifi_password_to_use = doc["public_wifi_password"].as<String>();
    String sensor_id_to_use = doc["sensor_id"].as<String>();
    String setup_token = doc["setup_token"].as<String>();

    WiFi.mode(WIFI_STA);
    // Connect to internal WiFi network
    WiFi.begin(internal_wifi_ssid_to_use, internal_wifi_password_to_use);
    Serial.println("Connecting to internal WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected signingup");

    esp32_signup(central_server_ip_to_use, sensor_id_to_use, setup_token);

    String config_data =
        "{\"internal_wifi_ssid\": \"" + internal_wifi_ssid_to_use +
        "\", \"internal_wifi_password\": \"" + internal_wifi_password_to_use +
        "\", \"central_server_ip\": \"" + central_server_ip_to_use +
        "\", \"public_wifi_ssid\": \"" + public_wifi_ssid_to_use +
        "\", \"public_wifi_password\": \"" + public_wifi_password_to_use +
        "\", \"sensor_id\": \"" + sensor_id_to_use +
        "\", \"valid_until\": \"" + server_valid_until +
        "\"}";

    File config_file = SPIFFS.open("/config.json", FILE_WRITE);
    config_file.print(config_data);
    config_file.close();
    ESP.restart();
}

void serve_routes()
{
    if (!already_setup)
    {
        server.on("/setup", HTTP_POST, handle_setup);
    }
    server.on("/handshake", HTTP_POST, handle_handshake);
    server.on("/authenticate", HTTP_POST, handle_authenticate);
    server.begin();
    Serial.println("Server ready.");
}

void setup()
{
    Serial.begin(115200);

    // while (!Serial) {
    //     delay(1000); // wait for serial port to connect. Needed for native USB
    // }

    delay(2000);
    Serial.println("serial started");

    init_crypto_random_engine();

    load_config();

    setup_wifi();

    serve_routes();

    // Initialize pins
    pinMode(SENSOR_PIN, INPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);

    // Turn off both LEDs at startup
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    ledActive = false;
    motionDetected = false;

    Serial.println("=== SENDER ESP32 HTTP SERVER ===");
    Serial.println("Both LEDs should be OFF now!");
}

void loop()
{
    server.handleClient();

    bool currentSensorState = digitalRead(SENSOR_PIN);

    // Detect motion (HIGH to LOW transition)
    if (lastSensorState == HIGH && currentSensorState == LOW)
    {
        // Debounce check
        if (millis() - lastDetectionTime > DEBOUNCE_DELAY)
        {
            Serial.println("🚨 MOTION DETECTED! Waiting for HTTP request...");
            Serial.println("⏰ 6-second window started - send your curl request now!");

            motionDetected = true;
            motionStartTime = millis();
            lastDetectionTime = millis();
        }
    }

    // Update last sensor state
    lastSensorState = currentSensorState;

    // Check if motion detected and timeout reached
    if (motionDetected)
    {
        unsigned long elapsed = millis() - motionStartTime;

        // Show countdown every second
        static unsigned long lastCountdown = 0;
        if (millis() - lastCountdown > 1000)
        {
            int remaining = (MOTION_WAIT_TIMEOUT - elapsed) / 1000;
            if (remaining >= 0)
            {
                Serial.println("⏳ Waiting for request... " + String(remaining) + " seconds remaining");
            }

            lastCountdown = millis();
        }

        // Check timeout
        if (elapsed >= MOTION_WAIT_TIMEOUT)
        {
            Serial.println("❌ TIMEOUT! No HTTP request received within 6 seconds");
            Serial.println("🔴 Showing RED LED for 3 seconds");

            // Show red LED for timeout
            digitalWrite(RED_LED_PIN, HIGH);
            digitalWrite(BLUE_LED_PIN, LOW);
            ledStartTime = millis();
            ledActive = true;

            // Reset motion detection
            motionDetected = false;
        }
    }

    // Handle LED timing (turn off after 3 seconds)
    if (ledActive && (millis() - ledStartTime > 3000))
    {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(BLUE_LED_PIN, LOW);
        ledActive = false;
        Serial.println("LEDs turned OFF");
    }
}
