/**
 * @file relay_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 * @brief WiFi relay module
 * @version 1.0
 * @date 18.08.2023
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <shButton.h>
#include <shSRControl.h>

// модуль WiFi-реле (розетка, люстра и т.д.)

#define FILESYSTEM SPIFFS

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321; // локальный порт для прослушивания udp-пакетов

ESP8266WebServer HTTP(80);
WiFiUDP udp;

#if FILESYSTEM == LittleFS
#include <LittleFS.h>
#endif

const int8_t ledPin = LED_BUILTIN;

shButton btn1(D5);

// работаем с двумя реле на модуле реле (локальная кнопка - только для первого реле)
const uint8_t relays_count = 2;

shRelayData relays[relays_count] = {
    (shRelayData){
        "relay1",
        D1,
        LOW,
        &btn1},
    (shRelayData){
        "relay2",
        D2,
        LOW,
        NULL}};

shRelayControl relay_control(relays, relays_count);

void setup()
{
  Serial.begin(115200);
  Serial.println();

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

  Serial.println(F("Starting UDP"));
  if (udp.begin(localPort))
  {
    Serial.println(F("OK"));
    // запустить контроль модуля реле
    relay_control.begin(&udp, localPort);
  }
  else
  {
    Serial.println(F("failed, restart"));
    ESP.restart();
  }
  HTTP.begin();
}

void loop()
{
  relay_control.tick();

  HTTP.handleClient();

  delay(1);
}
