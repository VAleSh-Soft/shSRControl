#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <shButton.h>
#include <shSRControl.h>

// модуль WiFi-выключателя

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321; // локальный порт для прослушивания udp-пакетов

WiFiUDP udp;

const int8_t ledPin = LED_BUILTIN;

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

shSwitchControl switch_control;

void setup()
{
  Serial.begin(115200);
  Serial.println();

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
    switch_control.begin(&udp, localPort, relays_count, relays);
  }
  else
  {
    Serial.println(F("failed, restart"));
    ESP.restart();
  }
}

void loop()
{
  switch_control.tick();

  delay(1);
}