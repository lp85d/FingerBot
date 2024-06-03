#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Servo.h>

ESP8266WebServer server(80);
WiFiClient client;
Servo servo;

const int servoPin = 0;
String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 30000; // Интервал обновления IP (30 секунд)
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 6000; // Интервал проверки статуса сервера (6 секунд)
int currentPosition = 90; // Начальное положение сервопривода (90 градусов для середины диапазона)
String currentStatus = "Unknown";

void saveConfigCallback(WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
    Serial.begin(115200);
    servo.attach(servoPin);
    servo.write(currentPosition);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(180);

    if (!wifiManager.autoConnect("FingerBot")) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }

    server.on("/", handleRoot);
    server.begin();

    Serial.println("Connected to WiFi network.");
    Serial.println("Setup complete.");

    lastUpdateTime = millis();
    lastRequestTime = millis();
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        unsigned long currentMillis = millis();

        if (currentMillis - lastUpdateTime > updateInterval) {
            updateExternalIP();
            lastUpdateTime = currentMillis;
        }

        if (currentMillis - lastRequestTime > requestInterval) {
            checkServerStatus();
            lastRequestTime = currentMillis;
        }
    } else {
        Serial.println("WiFi not connected.");
    }

    server.handleClient();
}

void handleRoot() {
    Serial.println("handleRoot called");

    String message = "<h1>Device Status</h1>";
    message += "<p>Current Position: " + String(currentPosition) + " degrees</p>";
    message += "<p>Current Status: " + currentStatus + "</p>";
    message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";

    server.send(200, "text/html", message);
}

void updateExternalIP() {
    Serial.println("Updating external IP...");
    sendHttpRequest("http://fingerbot.ru/ip/", [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            externalIP = payload;
            Serial.println("External IP updated: " + externalIP);
        } else {
            Serial.printf("Error on HTTP request: %d\n", httpCode);
        }
    });
}

void checkServerStatus() {
    if (externalIP.isEmpty()) return;

    String url = "http://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    sendHttpRequest(url, [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            handleServerResponse(payload);
        } else {
            Serial.printf("Error checking server status. HTTP Code: %d\n", httpCode);
        }
    });
}

void handleServerResponse(const String& response) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.print("Failed to parse response: ");
        Serial.println(error.c_str());
        return;
    }

    int status = doc[0]["custom_ip_status"].as<int>();

    if (status == 1 && currentStatus != "1") {
        Serial.println("Status 1: Moving servo to +150 degrees.");
        currentPosition += 150;
        if (currentPosition > 180) {
            currentPosition = 180; // Ограничиваем до 180 градусов
        }
        servo.write(currentPosition);
        currentStatus = "1";
    } else if (status == 0 && currentStatus != "0") {
        Serial.println("Status 0: Moving servo to -150 degrees.");
        currentPosition -= 150;
        if (currentPosition < 0) {
            currentPosition = 0; // Ограничиваем до 0 градусов
        }
        servo.write(currentPosition);
        currentStatus = "0";
    }
}

void sendHttpRequest(const String& url, std::function<void(int, const String&)> callback) {
    HTTPClient http;
    http.begin(client, url); // Изменено на использование WiFiClient
    http.setTimeout(5000);
    int httpCode = http.GET();
    String payload = httpCode > 0 ? http.getString() : "";

    Serial.printf("HTTP GET request to %s returned code: %d\n", url.c_str(), httpCode);
    if (!payload.isEmpty()) {
        Serial.println("Response payload: " + payload);
    }

    callback(httpCode, payload);
    http.end();
}
