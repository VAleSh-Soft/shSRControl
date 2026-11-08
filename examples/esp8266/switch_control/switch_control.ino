/**
 * @file switch_control.ino
 * @author Vladimir Shatalov (valesh-soft@yandex.ru)
 *
 * @brief Пример создания модуля WiFi-выключателя на esp8266;
 *
 *        Дополнительно показано использование локальных кнопок модуля
 *        для выполнения пользовательских задач - в данном случае отключение
 *        всех ассоциированных с модулем удаленных реле при удержании
 *        нажатой более двух секунд кнопки, подвязанной к первому реле;
 *
 *        Так же показано добавление задачи в планировщик задач и подключение
 *        Web-интерфейса планировщика
 *
 * @version 1.5
 * @date 08.11.2026
 *
 * @copyright Copyright (c) 2023-2026
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <shSRControl.h>

// модуль WiFi-выключателя

#define FILESYSTEM SPIFFS

const char *const ssid = "**********"; // имя (SSID) вашей Wi-Fi сети
const char *const pass = "**********"; // пароль для подключения к вашей Wi-Fi сети

const uint16_t localPort = 54321;  // локальный порт для прослушивания udp-пакетов
const uint32_t checkTimer = 60000; // интервал проверки доступности реле - 60 сек

ESP8266WebServer HTTP(80);
WiFiUDP udp;

#if FILESYSTEM == LittleFS
#include <LittleFS.h>
#endif

const int8_t ledPin = LED_BUILTIN;
const int8_t buzzerPin = 15;

// работаем с двумя кнопками на модуле выключателя
srButton btn1(D1);
srButton btn2(D2);

shSwitchControl switch_control;

String config_page = "/config_page"; // адрес страницы конфигурации модуля
String tasks_page = "/tasks_page";   // адрес страницы планировщика задач

void setup()
{
  Serial.begin(115200);
  Serial.println();

  // работаем с двумя удаленными реле
  switch_control.init(2);

  // следующие две настройки могут быть заменены сохраненными в файле
  // конфигурации, поэтому устанавливаются здесь - на случай, если это
  // первый запуск, и файла конфигурации еще нету
  switch_control.setUdpPort(localPort);
  switch_control.setCheckTimer(checkTimer);

  // заполняем данные удаленных реле
  switch_control.addRelay("relay1", &btn1);
  switch_control.addRelay("relay2", &btn2);

  // настраиваем первую кнопку, чтобы при ее удержании более двух секунд можно
  // было отключить все ассоциированные с модулем удаленные реле
  switch_control.setButtonTimeoutOfLongClick(0, 2000);

  // подключаем Web-интерфейс
  if (FILESYSTEM.begin())
  {
    switch_control.attachWebInterface(&HTTP, &FILESYSTEM, config_page, tasks_page);
  }
  // подключаем звуковой сигнализатор об ошибке отправки команды удаленному реле
  switch_control.setErrorBuzzerState(true, buzzerPin);

  // устанавливаем WiFi-соединение
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
  if (switch_control.startDevice(&udp))
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
  switch_control.addTaskToList(task);

  // запускаем HTTP-сервер
  HTTP.onNotFound([]()
                  { HTTP.send(404, "text/plan", F("404. File not found.")); });
  HTTP.begin();
}

void loop()
{
  switch_control.tick();

  // при удержании первой кнопки более двух секунд выключаем все реле сразу
  if (switch_control.getButtonState(0) == BTN_LONGCLICK)
  {
    switch_control.setStateForAll(false);
  }
}
