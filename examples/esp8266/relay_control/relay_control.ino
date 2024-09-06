/**
 * @file relay_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 * @brief WiFi relay module built on esp8266
 * @version 1.2
 * @date 02.06.2024
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
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

srButton btn1(D5);

shRelayControl relay_control;

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // работаем с двумя реле на модуле
  relay_control.init(2);
  
  // заполняем данные локальных реле (локальная кнопка - только для первого реле)
  relay_control.addRelay("relay1", D1, LOW, &btn1);
  relay_control.addRelay("relay2", D2, LOW);

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
