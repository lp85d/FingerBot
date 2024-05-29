#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#endif

#include <WiFiManager.h>
#include <ArduinoJson.h>

#ifdef ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
WiFiClient client;
#endif

int wpUserLevel = 0;
const int ledPin = 13;

String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 600000; // 10 минут в миллисекундах
unsigned long lastHandleClientTime = 0;
const unsigned long handleClientInterval = 1000; // Интервал обработки клиентов в миллисекундах

void saveConfigCallback(WiFiManager *myWiFiManager) {
    // Ваш код, если необходим
}

void setup() {
    pinMode(ledPin, OUTPUT);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("ArduinoAP")) {
        delay(3000);
        ESP.restart();
        delay(5000);
    }

    server.on("/", handleRoot); // Регистрация обработчика для корневого пути
    server.begin();

    Serial.begin(115200);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (millis() - lastUpdateTime > updateInterval) {
            updateExternalIP();
        }

        if (millis() - lastHandleClientTime > handleClientInterval) {
            server.handleClient();
            lastHandleClientTime = millis();
        }
    }
}

int parseUserLevel(String data) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return 0; // В случае ошибки возвращаем 0
    }

    return doc["user_level"]; // Предполагаем, что уровень пользователя находится в "user_level"
}

void handleRoot() {
    server.send(200, "text/html", "<h1>Выберите Wi-Fi</h1>");
}

void updateExternalIP() {
    HTTPClient http;
    #ifdef ESP32
    http.begin("https://fingerbot.ru/ip/");
    #else
    http.begin(client, "https://fingerbot.ru/ip/");
    #endif
    http.setTimeout(5000); // Таймаут 5 секунд
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            externalIP = http.getString();
            lastUpdateTime = millis();

            getWPUserLevel();
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }
    
    http.end();
}

void getWPUserLevel() {
    String url = "https://fingerbot.ru/wp-json/custom/v1/ip-address?wp_user_level=" + externalIP;
    HTTPClient http;
    #ifdef ESP32
    http.begin(url);
    #else
    http.begin(client, url);
    #endif
    http.setTimeout(5000); // Таймаут 5 секунд
    int httpCode = http.GET();

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            wpUserLevel = parseUserLevel(payload);
            
            if (wpUserLevel == 0) {
                digitalWrite(ledPin, LOW);
            } else if (wpUserLevel == 1) {
                digitalWrite(ledPin, HIGH);
            }
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }

    http.end();
}
