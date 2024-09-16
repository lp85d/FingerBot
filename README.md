# FingerBot

Проект для управления сервоприводом через интернет на базе ESP8266.

## Описание

FingerBot управляет сервоприводом, позволяя включать и выключать его по запросу с сервера.

## Установка

1. Запустите следующие команды для прошивки устройства:

   ```bash
   C:\Windows\System32>python -m esptool chip_id
   esptool.py v4.7.0
   Found 2 serial ports
   Serial port COM9
   Connecting....
   Detecting chip type... Unsupported detection protocol, switching and trying again...
   Connecting...
   Detecting chip type... ESP8266
   Chip is ESP8266EX
   Features: WiFi
   Crystal is 26MHz
   MAC: 60:01:94:28:64:f4
   Uploading stub...
   Running stub...
   Stub running...
   Chip ID: 0x002864f4
   Hard resetting via RTS pin...

   C:\Windows\System32>python -m esptool --port COM9 write_flash 0x00000 "E:\OSPanel\home\fingerbot.ru\bin\uploads\src.ino.bin"
   esptool.py v4.7.0
   Serial port COM9
   Connecting...
   Detecting chip type... Unsupported detection protocol, switching and trying again...
   Connecting...
   Detecting chip type... ESP8266
   Chip is ESP8266EX
   Features: WiFi
   Crystal is 26MHz
   MAC: 60:01:94:28:64:f4
   Stub is already running. No upload is necessary.
   Configuring flash size...
   Flash will be erased from 0x00000000 to 0x00073fff...
   Compressed 473536 bytes to 341942...
   Wrote 473536 bytes (341942 compressed) at 0x00000000 in 30.4 seconds (effective 124.7 kbit/s)...
   Hash of data verified.

   Leaving...
   Hard resetting via RTS pin...

# Вывод монитора порта PuTTY

```bash
*wm:AutoConnect: FAILED for  60606 ms
*wm:StartAP with SSID:  FingerBot
*wm:AP IP address: 192.168.4.1
*wm:Starting Web Portal
*wm:config portal has timed out
*wm:config portal exiting

ets Jan  8 2013,rst cause:1, boot mode:(3,6)

load 0x4010f000, len 3424, room 16
tail 0
chksum 0x2e
load 0x3fff20b8, len 40, room 8
tail 0
chksum 0x2b
csum 0x2b
v000739c0
~ld
▒▒▒n▒{▒▒o|▒▒d▒;lc▒
▒|r▒l▒o▒
▒g▒
l`▒▒{▒d▒l▒
▒*wm:AutoConnect
*wm:Connecting to SAVED AP: 89041111985
*wm:connectTimeout not set, ESP waitForConnectResult...
*wm:AutoConnect: SUCCESS
*wm:STA IP Address: 192.168.0.152
Получен внешний IP: 185.175.119.204
Ответ от сервера: [{"custom_ip_status":"1"}]
Полученный статус сервера: 1
Ответ от сервера: [{"custom_ip_status":"1"}]
Полученный статус сервера: 1
Ответ от сервера: [{"custom_ip_status":"1"}]
Полученный статус сервера: 1
Ответ от сервера: [{"custom_ip_status":"0"}]
Полученный статус сервера: 0
Ответ от сервера: [{"custom_ip_status":"0"}]
Полученный статус сервера: 0
Ответ от сервера: [{"custom_ip_status":"1"}]
Полученный статус сервера: 1
Ответ от сервера: [{"custom_ip_status":"1"}]
Полученный статус сервера: 1

[![Watch the video](https://img.youtube.com/vi/Yxx3jv5dRPQ/0.jpg)](https://www.youtube.com/watch?v=Yxx3jv5dRPQ)
