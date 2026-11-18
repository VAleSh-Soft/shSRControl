/**
 * @file relay_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 *
 * @brief Пример создания модуля WiFi-реле (розетка, люстра и т.д.) на esp32;
 *
 *        В примере используется виртуальное реле, т.е. callback-функция,
 *        которая будет вызываться вместо переключения локального реле.
 *
 * @version 1.5
 * @date 18.11.2026
 *
 * @copyright Copyright (c) 2023-2026
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <shSRControl.h>

#define BUTTON_1_PIN D5 // пин для подключения локальной кнопки

#define FILESYSTEM LittleFS

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321; // локальный порт для прослушивания udp-пакетов

ESP8266WebServer HTTP(80);
WiFiUDP udp;

#if FILESYSTEM == LittleFS
#include <LittleFS.h>
#endif

const int8_t ledPin = LED_BUILTIN;

shRelayControl relay_control;

String tasks_page = "/tasks_page"; // адрес страницы планировщика задач

// callback-функция, которая будет вызываться при срабатывании виртуального реле:
// с помощью переключателя в Web-интерфейсе, с помощью клика локальной кнопкой
// или с помощью Wi-Fi-выключателя
// вы можете добавить в эту функцию все, что вам нужно
void relay_callback()
{
  static bool flag = false;
  flag = !flag;
  
  Serial.println();
  Serial.println("The virtual relay has been launched.");
  Serial.print("New state of the virtual relay: ");
  Serial.println((flag) ? "ON" : "OFF");
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // работаем с одним реле
  relay_control.init(1);

  // следующая настройка может быть заменена сохраненной в файле конфигурации,
  // поэтому устанавливается здесь - на случай, если это первый запуск, и файла
  // конфигурации еще нету
  relay_control.setUdpPort(localPort);

  // добавляем виртуальное реле
  relay_control.addRelay("relay1", relay_callback, BUTTON_1_PIN);

  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    relay_control.attachWebInterface(&HTTP, &FILESYSTEM, tasks_page);
  }

  // устанавливаем WiFi-подключение
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

  // запускаем модуль для работы с UDP-пакетами
  // если вы не собираетесь использовать WiFi-выключатели и планируете
  // управлять модулем только через Web-интерфейс, то можете пропустить
  // этот шаг
  if (relay_control.startUdpInteraction(&udp))
  {
    Serial.println(F("Device started"));
  }
  else
  {
    Serial.println(F("failed, restart"));
    ESP.restart();
  }

  // запускаем HTTP-сервер для работы Web-интерфейса
  HTTP.onNotFound([]()
                  { HTTP.send(404, "text/plan", F("404. File not found.")); });
  HTTP.begin();
}

void loop()
{
  relay_control.tick();
}
