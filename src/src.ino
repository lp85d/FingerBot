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
String logMessages; // Строка для хранения логов

void saveConfigCallback(WiFiManager *myWiFiManager) {
    logMessages += "Entered config mode\n";
    logMessages += "IP: " + WiFi.softAPIP().toString() + "\n";
    logMessages += "SSID: " + myWiFiManager->getConfigPortalSSID() + "\n";
}

void setup() {
    Serial.begin(115200);
    servo.attach(servoPin);
    servo.write(currentPosition);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(200);

    if (!wifiManager.autoConnect("FingerBot")) {
        logMessages += "Failed to connect and hit timeout\n";
        delay(3000);
        ESP.restart();
    }

    server.on("/", handleRoot);
    server.begin();

    logMessages += "Connected to WiFi network.\n";
    logMessages += "Setup complete.\n";

    // Вывод IP-адреса в Serial Monitor и в логи
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    logMessages += "IP Address: " + WiFi.localIP().toString() + "\n";

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
        logMessages += "WiFi not connected.\n";
    }

    server.handleClient();
}

void handleRoot() {
    String message = "<h1>Device Status</h1>";
    message += "<p>Current Position: " + String(currentPosition) + " degrees</p>";
    message += "<p>Current Status: " + currentStatus + "</p>";
    message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    message += "<pre>" + logMessages + "</pre>";

    server.send(200, "text/html", message);
}

void updateExternalIP() {
    logMessages += "Updating external IP...\n";
    sendHttpRequest("http://fingerbot.ru/ip/", [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            externalIP = payload;
            logMessages += "External IP updated: " + externalIP + "\n";
        } else {
            logMessages += "Error on HTTP request: " + String(httpCode) + "\n";
        }
    });
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        logMessages += "External IP is empty, skipping status check.\n";
        return;
    }

    String url = "http://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    logMessages += "Checking server status with URL: " + url + "\n";
    sendHttpRequest(url, [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            logMessages += "Server status response received: " + payload + "\n";
            handleServerResponse(payload);
        } else {
            logMessages += "Error checking server status. HTTP Code: " + String(httpCode) + "\n";
        }
    });
}

void handleServerResponse(const String& response) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        logMessages += "Failed to parse response: " + String(error.c_str()) + "\n";
        return;
    }

    int status = doc[0]["custom_ip_status"].as<int>();
    logMessages += "Parsed status: " + String(status) + "\n";

    if (status == 1 && currentStatus != "1") {
        logMessages += "Status 1: Moving servo to 180 degrees.\n"; // Изменено на 180 градусов
        currentPosition = 180;
        logMessages += "Before servo.write(180)\n";
        servo.write(currentPosition);
        logMessages += "After servo.write(180)\n";
        currentStatus = "1";
    } else if (status == 0 && currentStatus != "0") {
        logMessages += "Status 0: Moving servo to 0 degrees.\n";
        currentPosition = 0;
        logMessages += "Before servo.write(0)\n";
        servo.write(currentPosition);
        logMessages += "After servo.write(0)\n";
        currentStatus = "0";
    }
}

void sendHttpRequest(const String& url, std::function<void(int, const String&)> callback) {
    HTTPClient http;
    http.begin(client, url); // Изменено на использование WiFiClient
    http.setTimeout(5000);
    int httpCode = http.GET();
    String payload = httpCode > 0 ? http.getString() : "";

    logMessages += "HTTP GET request to " + url + " returned code: " + String(httpCode) + "\n";
    if (!payload.isEmpty()) {
        logMessages += "Response payload: " + payload + "\n";
    }

    callback(httpCode, payload);
    http.end();
}
