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
const unsigned long updateInterval = 24 * 60 * 60 * 1000; // Интервал обновления IP (24 часа)
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000; // Интервал проверки статуса сервера (10 секунд)
int currentPosition = 90; // Начальное положение сервопривода (90 градусов для середины диапазона)
String currentStatus = "Unknown";

void saveConfigCallback(WiFiManager *myWiFiManager) {}

void setup() {
    Serial.begin(115200);
    servo.attach(servoPin);
    servo.write(currentPosition);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(60); // Уменьшено время тайм-аута для соединения

    if (!wifiManager.autoConnect("FingerBot")) {
        wifiManager.startConfigPortal(); // Попытка подключения к доступной сети
        delay(3000);
        ESP.restart();
    }

    server.on("/", handleRoot);
    server.begin();

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
        if (currentMillis - lastRequestTime > requestInterval) {
            reconnectWiFi();
            lastRequestTime = currentMillis;
        }
    }

    // Проверка на ввод команды из монитора порта
    checkSerialInput();
}

void handleRoot() {
    String message = "<h1>Device Status</h1>";
    message += "<p>Current Position: " + String(currentPosition) + " degrees</p>";
    message += "<p>Current Status: " + currentStatus + "</p>";
    message += "<p>Signal Strength: " + String(WiFi.RSSI()) + " dBm</p>";
    
    if (externalIP.isEmpty()) {
        message += "<p>External IP: Not available</p>";
    } else {
        message += "<p>External IP: " + externalIP + "</p>";
    }

    server.send(200, "text/html", message);
}

void updateExternalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        sendHttpRequest("https://fingerbot.ru/ip/", [&](int httpCode, const String& payload) {
            Serial.print("HTTP Code: "); Serial.println(httpCode);
            Serial.print("Payload: "); Serial.println(payload);
            if (httpCode == HTTP_CODE_OK) {
                externalIP = payload;
                Serial.print("External IP: "); Serial.println(externalIP);
            } else {
                Serial.println("Failed to get External IP");
            }
        });
    }
}

void checkServerStatus() {
    if (externalIP.isEmpty()) {
        return;
    }

    String url = "http://fingerbot.ru/wp-json/custom/v1/ip-address?custom_ip_status=" + externalIP;
    sendHttpRequest(url, [&](int httpCode, const String& payload) {
        if (httpCode == HTTP_CODE_OK) {
            handleServerResponse(payload);
        }
    });
}

void handleServerResponse(const String& response) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.println("Ошибка десериализации JSON");
        return;
    }

    // Если ответ содержит объект, а не массив
    int status = doc["custom_ip_status"].as<int>();

    if (status == 1 && currentStatus != "1") {
        moveServo(180);  // Плавное перемещение на 180 градусов
        currentStatus = "1";
    } else if (status == 0 && currentStatus != "0") {
        moveServo(0);    // Плавное перемещение на 0 градусов
        currentStatus = "0";
    }
}

void sendHttpRequest(const String& url, std::function<void(int, const String&)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi not connected, cannot send HTTP request");
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
    wifiManager.setConfigPortalTimeout(60);
    
    // Проверка, если уже подключено
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Уже подключено к сети Wi-Fi.");
        return;
    }

    Serial.println("Введите 'reset' для сброса настроек Wi-Fi или 'connect' для подключения.");

    unsigned long startAttemptTime = millis();
    while (millis() - startAttemptTime < 30000) { // Таймаут в 30 секунд
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            command.trim(); // Удаляем лишние пробелы
            
            if (command.equalsIgnoreCase("reset")) {
                Serial.println("Сбрасываю настройки Wi-Fi...");
                WiFi.disconnect(true); // Удаляем сохраненные сети
                Serial.println("Настройки Wi-Fi сброшены. Устройство перезагружается...");
                ESP.restart(); // Перезагрузка устройства
            } else if (command.equalsIgnoreCase("connect")) {
                Serial.println("Попытка подключения к Wi-Fi...");
                if (wifiManager.autoConnect("FingerBot")) {
                    Serial.println("Подключение успешно!");
                    break; // Выход из цикла, если подключение успешно
                } else {
                    Serial.println("Не удалось подключиться. Попробуйте снова.");
                }
            } else {
                Serial.println("Неизвестная команда. Попробуйте 'reset' или 'connect'.");
            }
        }
        delay(100); // Небольшая задержка для предотвращения излишней загрузки процессора
    }
}

void checkSerialInput() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        // Обработка команд для перезагрузки или подключения
        if (command.equalsIgnoreCase("reset")) {
            reconnectWiFi();
        } else if (command.equalsIgnoreCase("connect")) {
            reconnectWiFi();
        }
    }
}

// Плавное перемещение сервопривода
void moveServo(int targetPosition) {
    if (currentPosition != targetPosition) {
        int step = currentPosition < targetPosition ? 1 : -1;
        while (currentPosition != targetPosition) {
            currentPosition += step;
            servo.write(currentPosition);
            delay(20); // Небольшая задержка для плавного перемещения
        }
    }
}
