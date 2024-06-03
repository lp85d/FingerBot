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
#include <Servo.h>

#ifdef ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
WiFiClient client;
#endif

Servo servo;

#ifdef ESP32
const int servoPin = 13; // Для ESP32 используем GPIO13
#else
const int servoPin = 0; // Для ESP-01S используем GPIO0
#endif

String externalIP;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 600000; // 10 минут в миллисекундах
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000; // 10 секунд в миллисекундах

void saveConfigCallback(WiFiManager *myWiFiManager) {
    // Ваш код, если необходим
}

void setup() {
    servo.attach(servoPin);
    servo.write(0); // Устанавливаем начальную позицию серво

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);

    if (!wifiManager.autoConnect("FingerBot")) {
        delay(3000);
        ESP.restart();
        delay(5000);
    }

    server.on("/", handleRoot); // Регистрация обработчика для корневого пути
    server.begin();

    Serial.begin(115200);

    lastUpdateTime = millis(); // Инициализация lastUpdateTime текущим временем
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (millis() - lastUpdateTime > updateInterval) {
            updateExternalIP();
        }

        if (millis() - lastRequestTime > requestInterval) {
            checkServerStatus();
            lastRequestTime = millis();
        }
    }

    server.handleClient(); // Должен вызываться всегда для обработки запросов клиентов
}

void handleRoot() {
    if (externalIP.isEmpty()) {
        server.send(200, "text/html", "<h1>External IP is not yet available. Please wait...</h1>");
        return;
    }

    String url = "https://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
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
            server.send(200, "application/json", payload);
        } else {
            server.send(500, "text/html", "Failed to get response from server.");
        }
    } else {
        server.send(500, "text/html", "Error on HTTP request: " + String(httpCode));
    }

    http.end();
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
            Serial.println("External IP updated: " + externalIP);
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }
    
    http.end();
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        return;
    }

    String url = "https://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
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
            handleServerResponse(payload);
        }
    } else {
        Serial.printf("Error on HTTP request: %d\n", httpCode);
    }

    http.end();
}

void handleServerResponse(String payload) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    if (doc.containsKey("code") && doc["code"] == "no_user_found") {
        Serial.println("No user found with the specified IP address. Checking again in 10 minutes.");
        lastUpdateTime = millis(); // Чтобы проверить IP через 10 минут
        return;
    }

    if (doc[0]["custom_ip_status"] == "1") {
        Serial.println("Status 1: Moving servo to 90 degrees clockwise.");
        servo.write(90); // Двигаем сервопривод на 90 градусов по часовой стрелке
    } else if (doc[0]["custom_ip_status"] == "0") {
        Serial.println("Status 0: Moving servo to 0 degrees.");
        servo.write(0); // Двигаем сервопривод обратно в начальное положение
    }
}
