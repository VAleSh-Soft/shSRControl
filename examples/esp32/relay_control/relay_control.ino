/**
 * @file relay_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 *
 * @brief Пример создания модуля WiFi-реле (розетка, люстра и т.д.) на esp32;
 *
 *        Дополнительно показано добавление задачи в планировщик задач и
 *        подключение Web-интерфейса планировщика
 *
 * @version 1.5
 * @date 08.11.2026
 *
 * @copyright Copyright (c) 2023-2026
 *
 */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <shSRControl.h>

#define BUTTON_1_PIN 17 // пин для подключения первой кнопки

#define FILESYSTEM FFat
// Вам нужно отформатировать файловую систему только один раз
// после первого запуска нужно изменить эту настройку на false 
// и перепрошить модуль еще раз
#define FORMAT_FILESYSTEM false

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321;  // локальный порт для прослушивания udp-пакетов

WebServer HTTP(80);
WiFiUDP udp;

#if FILESYSTEM == FFat
#include <FFat.h>
#elif FILESYSTEM == SPIFFS
#include <SPIFFS.h>
#endif

const int8_t ledPin = 4;

shRelayControl relay_control;

String config_page = "/config_page"; // адрес страницы конфигурации модуля
String tasks_page = "/tasks_page";   // адрес страницы планировщика задач

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // работаем с двумя реле на модуле
  relay_control.init(2);

  // следующая настройка может быть заменена сохраненной в файле конфигурации,
  // поэтому устанавливается здесь - на случай, если это первый запуск, и файла
  // конфигурации еще нету
  relay_control.setUdpPort(localPort);

  // заполняем данные локальных реле (локальная кнопка - только для первого реле)
  relay_control.addRelay("relay1", 16, LOW, BUTTON_1_PIN);
  relay_control.addRelay("relay2", 18, LOW);

  // не забудьте, форматировать файловую систему нужно только при первом запуске
  if (FORMAT_FILESYSTEM)
  {
    FILESYSTEM.format();
  }
  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    relay_control.attachWebInterface(&HTTP, &FILESYSTEM, config_page, tasks_page);
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

  // добавить в планировщик задачу, которая будет включать реле с индексом 1
  // по понедельникам, средам и пятницам в 21:30
  shSchedulerData task(true, // задача активна
                       21 * 60 + 30,
                       1,
                       true,
                       (shTaskRepeat)(TR_MON_DAY | TR_WED_DAY | TR_FRI_DAY));
  relay_control.addTaskToList(task);

  // запускаем HTTP-сервер
  HTTP.onNotFound([]()
                  { HTTP.send(404, "text/plan", F("404. File not found.")); });
  HTTP.begin();
}

void loop()
{
  relay_control.tick();
}
