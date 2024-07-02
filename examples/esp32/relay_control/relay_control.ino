/**
 * @file relay_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 * @brief WiFi relay module built on esp32
 * @version 1.2
 * @date 02.06.2024
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <shSRControl.h>

// модуль WiFi-реле (розетка, люстра и т.д.)

#define FILESYSTEM SPIFFS
// Вам нужно отформатировать файловую систему только один раз
#define FORMAT_FILESYSTEM false

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321; // локальный порт для прослушивания udp-пакетов

WebServer HTTP(80);
WiFiUDP udp;

#if FILESYSTEM == FFat
#include <FFat.h>
#endif
#if FILESYSTEM == SPIFFS
#include <SPIFFS.h>
#endif

const int8_t ledPin = 4;

srButton btn1(17);

// работаем с двумя реле на модуле реле (локальная кнопка - только для первого реле)
const uint8_t relays_count = 2;

shRelayData relays[relays_count] = {
    (shRelayData){
        "relay1",
        16,
        LOW,
        &btn1},
    (shRelayData){
        "relay2",
        18,
        LOW,
        NULL}};

shRelayControl relay_control;

void setup()
{
  Serial.begin(115200);
  Serial.println();

  relay_control.init(relays, relays_count);

  // не забудьте, форматировать файловую систему нужно только при первом запуске
  if (FORMAT_FILESYSTEM)
  {
    FILESYSTEM.format();
  }
  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    relay_control.attachWebInterface(&HTTP, &FILESYSTEM);
  }

  pinMode(ledPin, OUTPUT);
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(ledPin, !digitalRead(ledPin));
    delay(100);
    Serial.print('.');
  }
  digitalWrite(ledPin, HIGH);
  Serial.println();

  Serial.println(F("WiFi connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  Serial.print("Subnet mask: ");
  Serial.println(WiFi.subnetMask());

  Serial.print(F("Starting UDP..."));
  if (udp.begin(localPort))
  {
    Serial.println(F("OK"));
    // запустить контроль модуля реле
    relay_control.startDevice(&udp, localPort);
  }
  else
  {
    Serial.println(F("failed, restart"));
    ESP.restart();
  }

  HTTP.onNotFound([]()
                  { HTTP.send(404, "text/plan", F("404. File not found.")); });
  HTTP.begin();
}

void loop()
{
  relay_control.tick();

  HTTP.handleClient();

  delay(1);
}
