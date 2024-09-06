#pragma once

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#else
#error "The library is designed for use in the ESP8266 or ESP32 environment."
#endif
#include <WiFiUdp.h>
#include <FS.h>
#include <Ticker.h>
#include "srButtons.h"

// описание свойств реле
struct shRelayData
{
  String relayName;          // имя реле
  uint8_t relayPin;          // пин, к которому подключено реле
  uint8_t relayControlLevel; // управляющий уровень реле (LOW или HIGH)
  bool relayLastState;       // последнее состояние реле
  srButton *relayButton;     // локальная кнопка, управляющая реле (располагается на самом модуле и предназначена для ручного управления реле)
  String relayDescription;   // описание реле
  shRelayData() : relayName(""),
                  relayPin(255),
                  relayControlLevel(HIGH),
                  relayButton(nullptr),
                  relayDescription(""),
                  relayLastState(false) {}
  shRelayData(String relay_name,
              uint8_t relay_pin,
              uint8_t control_level,
              srButton *relay_button = nullptr,
              String relay_description = "") : relayName(relay_name),
                                               relayPin(relay_pin),
                                               relayControlLevel(control_level),
                                               relayButton(relay_button),
                                               relayDescription(relay_description),
                                               relayLastState(false) {}
};

// описание свойств выключателя
struct shSwitchData
{
  String relayName;        // имя ассоциированного с кнопкой удаленного реле
  bool relayFound;         // найдено или нет ассоциированное удаленное реле в сети
  IPAddress relayAddress;  // IP адрес удаленного реле
  srButton *relayButton;   // кнопка, управляющая удаленным реле
  String relayDescription; // описание удаленного реле
  shSwitchData() : relayName(""),
                   relayButton(nullptr),
                   relayDescription(""),
                   relayFound(false),
                   relayAddress(IPAddress(0, 0, 0, 0)) {}
  shSwitchData(String relay_name,
               srButton *relay_button = nullptr) : relayName(relay_name),
                                                   relayButton(relay_button),
                                                   relayDescription(""),
                                                   relayFound(false),
                                                   relayAddress(IPAddress(0, 0, 0, 0)) {}
};
// TODO: подумать над возможностью задания множественных реле, имеющих одно имя, но физически расположенных на разных модулях; т.е. добавить еще одно свойство и при его активации посылать запрос на переключение не по адресу, а широковещательным пакетом

// ==== shRelayControl class ===========================

/**
 * @brief класс модуля WiFi-реле
 *
 */
class shRelayControl
{
private:
  void respondToRelayCheck(int8_t index);
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  /**
   * @brief конструктор
   *
   */
  shRelayControl();

  /**
   * @brief инициализация модуля реле
   *
   * @param _relay_count количество реле
   */
  void init(uint8_t _relay_count);

  /**
   * @brief добавление данных локального реле
   *
   * @param relay_name имя реле
   * @param relay_pin пин, к которому подключено реле
   * @param control_level логический уровень, которым управляется реле (HIGH или LOW)
   * @param relay_button локальная кнопка для управления реле
   * @param relay_description описание реле
   * @return true если реле добавлено успешно, иначе false
   */
  bool addRelay(String relay_name,
                uint8_t relay_pin,
                uint8_t control_level,
                srButton *relay_button = nullptr,
                String relay_description = "");

  /**
   * @brief включение/отключение вывода информации о работе модуля через Serial
   *
   * @param _on
   * @param _serial интерфейс для вывода сообщений
   */
  void setLogOnState(bool _on, Print *_serial = &Serial);

  /**
   * @brief включен или отключен вывод информации о работе модуля через Serial
   *
   * @return true
   * @return false
   */
  bool getLogOnState();

  /**
   * @brief запуск модуля
   *
   * @param _udp ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля
   * @param _local_port порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей
   */
  void startDevice(WiFiUDP *_udp, uint16_t _local_port);

  /**
   * @brief подключение Web-интерфейса
   *
   * @param _server ссылка на экземпляр Web-сервера (WebServer), с которым будет работать модуль
   * @param _file_system ссылка на экземпляр файловой системы модуля для сохранения файла с настройками
   * @param _config_page адрес, по которому будет вызываться Web-страница конфигурации
   */
#if defined(ARDUINO_ARCH_ESP32)
  void attachWebInterface(WebServer *_server, FS *_file_system, String _config_page = "/relay_config");
#else
  void attachWebInterface(ESP8266WebServer *_server, FS *_file_system, String _config_page = "/relay_config");
#endif

  /**
   * @brief обработка событий модуля
   *
   */
  void tick();

  /**
   * @brief переключение состояния реле
   *
   * @param index индекс реле в массиве
   */
  void switchRelay(int8_t index);

  /**
   * @brief переключение состояния реле
   *
   * @param _name сетевое имя реле
   */
  void switchRelay(String _name);

  /**
   * @brief установить состояние реле
   *
   * @param index индекс реле в массиве
   * @param state новое состояние реле; true - включено, иначе выключено;
   */
  void setRelayState(int8_t index, bool state);

  /**
   * @brief установить состояние реле
   *
   * @param _name сетевое имя реле
   * @param state новое состояние реле; true - включено, иначе выключено;
   */
  void setRelayState(String _name, bool state);

  /**
   * @brief получение информации о текущем состоянии реле (включено/отключено)
   *
   * @param index  индекс реле в массиве
   * @return String "on" - включено; "off" - отключено
   */
  String getRelayState(int8_t index);

  /**
   * @brief получение информации о текущем состоянии реле (включено/отключено)
   *
   * @param _name сетевое имя реле
   * @return String  "on" - включено; "off" - отключено
   */
  String getRelayState(String _name);

  /**
   * @brief установка описания модуля
   *
   * @param _descr новое описание
   */
  void setModuleDescription(String _descr);

  /**
   * @brief получение текущего описания модуля
   *
   * @return String
   */
  String getModuleDescription();

  /**
   * @brief сохранение и восстановление последнего состояния реле при перезапуске модуля
   *
   * @param _state новое состояние опции
   */
  void setSaveStateOfRelay(bool _state);

  /**
   * @brief получение текущего состояния опции сохранения и восстановления последнего состояния реле
   *
   * @return true
   * @return false
   */
  bool getSaveStateOfRelay();

  /**
   * @brief установка нового сетевого имени реле
   *
   * @param index индекс реле в массиве
   * @param _name новое сетевое имя
   */
  void setRelayName(int8_t index, String _name);

  /**
   * @brief получение текущего сетевого имени реле
   *
   * @param index индекс реле в массиве
   * @return String
   */
  String getRelayName(int8_t index);

  /**
   * @brief установка нового описания реле
   *
   * @param index индекс реле в массиве
   * @param _descr новое описание
   */
  void setRelayDescription(int8_t index, String _descr);

  /**
   * @brief получение текущего описания реле
   *
   * @param index индекс реле в массиве
   * @return String
   */
  String getRelayDescription(int8_t index);

  /**
   * @brief установка нового имени файла для сохранения настроек
   *
   * @param _name новое имя файла
   */
  void setFileName(String _name);

  /**
   * @brief получение текущего имени файла конфигурации модуля
   *
   * @return String
   */
  String getFileName();

  /**
   * @brief сохранение настроек в файл
   *
   * @return true
   * @return false
   */
  bool saveConfige();

  /**
   * @brief считывание и загрузка сохраненных в файле настроек
   *
   * @return true
   * @return false
   */
  bool loadConfig();
};

// ==== shSwitchControl class ==========================

/**
 * @brief класс модуля WiFi-выключателя
 *
 */
class shSwitchControl
{
private:
  uint32_t checkTimer = 30000;

  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  /**
   * @brief конструктор
   *
   */
  shSwitchControl();

  /**
   * @brief инициализация модуля выключателя
   *
   * @param _switch_count количество удаленных реле
   */
  void init(uint8_t _switch_count);

  /**
   * @brief добавление данных удаленного реле
   *
   * @param relay_name имя реле
   * @param relay_button кнопка, управляющая удаленным реле
   * @return true если данные добавлены успешно, иначе false
   */
  bool addRelay(String relay_name,
                srButton *relay_button = nullptr);

  /**
   * @brief включение/отключение вывода информации о работе модуля через Serial
   *
   * @param _on
   * @param _serial интерфейс для вывода сообщений
   */
  void setLogOnState(bool _on, Print *_serial = &Serial);

  /**
   * @brief включен или отключен вывод информации о работе модуля через Serial
   *
   * @return true
   * @return false
   */
  bool getLogOnState();

  /**
   * @brief включить/выключить подачу звукового сигнала об ошибке отправки команды удаленному реле
   *
   * @param _state новое состояние опции
   * @param _pin пин, к которому подключен буззер
   */
  void setErrorBuzzerState(bool _state, int8_t _pin = -1);

  /**
   * @brief получить состояние опции подачи звукового сигнала об ошибке отправки команды удаленному реле
   *
   * @return true
   * @return false
   */
  bool getErrorBuzzerState();

  /**
   * @brief установка интервала проверки доступности связанных реле в сети в милисекундах; по умолчанию установлен интервал в 30 секунд
   *
   * @param _timer новое значение в милисекундах
   */
  void setCheckTimer(uint32_t _timer);

  /**
   * @brief получение размера интервала проверки доступности связанных реле в сети в милисекундах;
   *
   * @return uint32_t
   */
  uint32_t getCheckTimer();

  /**
   * @brief запуск модуля
   *
   * @param _udp ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля
   * @param _local_port порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей
   */
  void startDevice(WiFiUDP *_udp, uint16_t _local_port);

  /**
   * @brief подключение Web-интерфейса
   *
   * @param _server ссылка на экземпляр Web-сервера (WebServer), с которым будет работать модуль
   * @param _file_system ссылка на экземпляр файловой системы модуля для сохранения файла с настройками
   * @param _config_page адрес, по которому будет вызываться Web-страница конфигурации
   */
#if defined(ARDUINO_ARCH_ESP32)
  void attachWebInterface(WebServer *_server, FS *_file_system, String _config_page = "/relay_config");
#else
  void attachWebInterface(ESP8266WebServer *_server, FS *_file_system, String _config_page = "/relay_config");
#endif

  /**
   * @brief обработка событий модуля
   *
   */
  void tick();

  /**
   * @brief переключение состояния удаленного реле
   *
   * @param index индекс реле в массиве
   */
  void switchRelay(int8_t index);

  /**
   * @brief переключение состояния удаленного реле
   *
   * @param _name сетевое имя удаленного реле
   */
  void switchRelay(String _name);

  /**
   * @brief установить состояние удаленного реле
   *
   * @param index индекс реле в массиве
   * @param state новое состояние реле; true - включено, иначе выключено;
   */
  void setRelayState(int8_t index, bool state);

  /**
   * @brief установить состояние удаленного реле
   *
   * @param _name сетевое имя удаленного реле
   * @param state новое состояние реле; true - включено, иначе выключено;
   */
  void setRelayState(String _name, bool state);

  /**
   * @brief установить состояние всех удаленных реле
   *
   * @param state новое состояние реле; true - включено, иначе выключено;
   * @param _self если true - команда на изменения состояния посылается только для реле, ассоциированных с выключателем; иначе команда посылается для всех реле, доступных в сети;
   */
  void setStateForAll(bool state, bool _self = true);

  /**
   * @brief поиск связанных реле в сети
   *
   */
  void findRelays();

  /**
   * @brief установка описания модуля
   *
   * @param _descr новое описание
   */
  void setModuleDescription(String _descr);

  /**
   * @brief получение текущего описания модуля
   *
   * @return String
   */
  String getModuleDescription();

  /**
   * @brief установка сетевого имени удаленного реле, которым будет управлять кнопка с индексом index в массиве
   *
   * @param index индекс реле в массиве
   * @param _name новое имя удаленного реле
   */
  void setRelayName(int8_t index, String _name);

  /**
   * @brief получение текущего имени удаленного реле, связанного с кнопкой
   *
   * @param index индекс реле в массиве
   * @return String
   */
  String getRelayName(int8_t index);

  /**
   * @brief установка нового имени файла для сохранения настроек
   *
   * @param _name новое имя файла
   */
  void setFileName(String _name);

  /**
   * @brief получение текущего имени файла конфигурации модуля
   *
   * @return String
   */
  String getFileName();

  /**
   * @brief сохранение настроек в файл
   *
   * @return true
   * @return false
   */
  bool saveConfige();

  /**
   * @brief считывание и загрузка сохраненных в файле настроек
   *
   * @return true
   * @return false
   */
  bool loadConfig();
};

// ==== ErrorBuzzer class ============================

class ErrorBuzzer
{
private:
  Ticker buzzer;
  int8_t pin = -1;
  uint8_t bip_count = 0;
  uint16_t freq = 500;
  uint32_t dur = 50;
  bool state = false;

public:
  ErrorBuzzer();

  void setState(bool _state, int8_t _pin = -1);
  bool getState();
  void startBuzzer(uint8_t _num);
  void stopBuzzer();
  void bip();
  uint8_t decBipCount();
};
