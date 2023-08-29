/**
 * @file switch_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 * @brief WiFi switch module
 * @version 1.0
 * @date 18.08.2023
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <shButton.h>
#include <shSRControl.h>

// модуль WiFi-выключателя

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

// работаем с двумя кнопками на модуле выключателя
shButton btn1(16);
shButton btn2(18);

const uint8_t relays_count = 2;

shSwitchData relays[relays_count] = {
    (shSwitchData){
        "relay1",
        false,
        IPAddress(192, 168, 4, 1),
        &btn1},
    (shSwitchData){
        "relay2",
        false,
        IPAddress(192, 168, 4, 1),
        &btn2}};

shSwitchControl switch_control(relays, relays_count);

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // не забудьте, форматировать файловую систему нужно только при первом запуске
  if (FORMAT_FILESYSTEM)
  {
    FILESYSTEM.format();
  }
  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    switch_control.attachWebInterface(&HTTP, &FILESYSTEM);
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
    // установить интервал проверки доступности реле - 60 сек
    switch_control.setCheckTimer(60000);
    // запустить контроль модуля выключателей
    switch_control.begin(&udp, localPort);
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
  switch_control.tick();

  HTTP.handleClient();

  delay(1);
}
