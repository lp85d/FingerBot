#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <WiFiClientSecure.h>

ESP8266WebServer server(80);
WiFiClientSecure client;
Servo servo;

const int servoPin = 0;
String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 24 * 60 * 60 * 1000;
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000;
int currentPosition = 90;
String currentStatus = "Unknown";

void saveConfigCallback(WiFiManager *myWiFiManager) {}

void setup() {
    Serial.begin(115200);
    servo.attach(servoPin);
    servo.write(currentPosition);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(15);

    if (!wifiManager.autoConnect("FingerBot")) {
        ESP.restart();
    }

    server.on("/", handleRoot);
    server.begin();

    client.setInsecure();

    if (WiFi.status() == WL_CONNECTED) {
        updateExternalIP();
    }

    lastUpdateTime = millis();
    lastRequestTime = millis();
}

void loop() {
    unsigned long currentMillis = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
        if (currentMillis - lastUpdateTime > updateInterval) {
            updateExternalIP();
            lastUpdateTime = currentMillis;
        }

        if (currentMillis - lastRequestTime > requestInterval) {
            checkServerStatus();
            lastRequestTime = currentMillis;
        }
        
        server.handleClient();
    } else {
        reconnectWiFi();
    }

    checkSerialInput();
}

void handleRoot() {
    String message = "<h1>Device Status</h1>";
    message += "<p>Current Position: " + String(currentPosition) + " degrees</p>";
    message += "<p>Current Status: " + currentStatus + "</p>"; // Статус устройства (серво)
    message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    
    if (externalIP.isEmpty()) {
        message += "<p>External IP: Not available</p>";
    } else {
        message += "<p>External IP: " + externalIP + "</p>";
    }

    message += "<p>Status server: " + currentStatus + "</p>"; // Добавляем строку для статуса сервера

    server.send(200, "text/html", message);
}

void updateExternalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        String url = "https://fingerbot.ru/ip/";
        sendHttpRequest(url, [&](int httpCode, const String& payload) {
            if (httpCode == HTTP_CODE_OK) {
                externalIP = payload;
                Serial.print("Получен внешний IP: ");
                Serial.println(externalIP);
            } else {
                Serial.println("Не удалось получить внешний IP");
            }
        });
    }
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        return;
    }

    String url = "https://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    sendHttpRequest(url, [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            handleServerResponse(payload);
        } else {
            Serial.println("Ошибка при запросе статуса сервера");
        }
    });
}

void handleServerResponse(const String& response) {
    Serial.print("Ответ от сервера: ");
    Serial.println(response);  // Выводим весь ответ для отладки

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.println("Ошибка десериализации JSON");
        return;
    }

    // Проверяем, является ли полученный JSON массивом
    if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        if (arr.size() > 0) {
            int status = arr[0]["custom_ip_status"].as<int>(); // Извлекаем статус из первого элемента массива

            Serial.print("Полученный статус сервера: ");
            Serial.println(status);

            if (status == 1 && currentStatus != "1") {
                moveServo(180);
                currentStatus = "1";
            } else if (status == 0 && currentStatus != "0") {
                moveServo(0);
                currentStatus = "0";
            }
        } else {
            Serial.println("Массив пуст");
        }
    } else {
        Serial.println("Ожидался массив, но получен другой формат данных");
    }
}

void sendHttpRequest(const String& url, std::function<void(int, const String&)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(5000);
    int httpCode = http.GET();
    String payload = httpCode > 0 ? http.getString() : "";

    callback(httpCode, payload);
    http.end();
}

void reconnectWiFi() {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(15);
    
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if (!wifiManager.autoConnect("FingerBot")) {
        ESP.restart();
    }
}

void checkSerialInput() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.equalsIgnoreCase("reset") || command.equalsIgnoreCase("connect")) {
            reconnectWiFi();
        }
    }
}

void moveServo(int targetPosition) {
    if (currentPosition != targetPosition) {
        int step = currentPosition < targetPosition ? 1 : -1;
        while (currentPosition != targetPosition) {
            currentPosition += step;
            servo.write(currentPosition);
            delay(20);
        }
    }
}
