/**
 * @file switch_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 * @brief WiFi switch module
 * @version 1.1
 * @date 29.09.2023
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

// модуль WiFi-выключателя

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
const int8_t buzzerPin = 15;

// работаем с двумя кнопками на модуле выключателя
shButton btn1(D1);
shButton btn2(D2);

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

  // настраиваем первую кнопку, чтобы при ее удержании более двух секунд можно было отключить все ассоциированные с модулем удаленные реле
  btn1.setLongClickMode(LCM_ONLYONCE);
  btn1.setTimeoutOfLongClick(2000);

  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    switch_control.attachWebInterface(&HTTP, &FILESYSTEM);
  }
  // подключаем звуковой сигнализатор об ошибке отправки команды удаленному реле
  switch_control.setErrorBuzzerState(true, buzzerPin);

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

  // при удержании первой кнопки более двух секунд выключаем все реле сразу
  if (btn1.getLastState() == BTN_LONGCLICK)
  {
    switch_control.setStateForAll(false);
  }

  HTTP.handleClient();

  delay(1);
}
