#include <WiFi.h>
#include <WiFiUdp.h>
#include <shButton.h>
#include <shSRControl.h>

// модуль WiFi-реле (розетка, люстра и т.д.)

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321; // локальный порт для прослушивания udp-пакетов

WiFiUDP udp;

const int8_t ledPin = 4;

shButton btn1(D5);

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
    relay_control.begin(&udp, localPort, relays_count, relays);
  }
  else
  {
    Serial.println(F("failed, restart"));
    ESP.restart();
  }
}

void loop()
{
  relay_control.tick();

  delay(1);
}
