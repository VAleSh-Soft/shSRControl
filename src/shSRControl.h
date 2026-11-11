#pragma once

#include <Arduino.h>
#include <new>
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

#if defined(ARDUINO_ARCH_ESP32)
typedef WebServer shWebServer;
#else
typedef ESP8266WebServer shWebServer;
#endif

#define DEFAULT_UDP_PORT 54321         // номер порта по умолчанию для обмена udp-пакетами
#define DEFAULT_CHECK_INTERVAL 60000ul // интервал опроса удаленных реле модулем выключателя, миллисекунд
#define SIZE_LIST_OF_REMOTE_RELAYS 10  // максимальный размер списка доступных в сети удаленных реле

// описание флагов периодичности выполнения задачи
enum shTaskRepeat : uint8_t
{
  TR_ONCE = 0x00,    // однократное срабатывание; 0b00000000
  TR_MON_DAY = 0x01, // по понедельникам;         0b00000001
  TR_TUE_DAY = 0x02, // по вторникам;             0b00000010
  TR_WED_DAY = 0x04, // по средам;                0b00000100
  TR_THU_DAY = 0x08, // по четвергам;             0b00001000
  TR_FRI_DAY = 0x10, // по пятницам;              0b00010000
  TR_SAT_DAY = 0x20, // по субботам;              0b00100000
  TR_SUN_DAY = 0x40, // по воскресеньям;          0b01000000
  TR_DAILY = 0x80    // ежедневное срабатывание;  0b10000000
};

// описание свойств задач планировщика
struct shSchedulerData
{
  bool task_status;         // статус задачи
  int32_t time_for_task;    // время срабатывания задачи, минут с начала суток
  uint8_t relay_index;      // индекс реле задачи
  bool action_for_relay;    // действие с реле (включить/выключить)
  shTaskRepeat task_repeat; // периодичность выполнения задачи

  shSchedulerData() : task_status(true),
                      time_for_task(0),
                      relay_index(0),
                      action_for_relay(true),
                      task_repeat(TR_ONCE) {}

  shSchedulerData(bool task_status,
                  int32_t time_for_task,
                  uint8_t relay_index,
                  bool action_for_relay,
                  shTaskRepeat task_repeat) : task_status(task_status),
                                              time_for_task(time_for_task),
                                              relay_index(relay_index),
                                              action_for_relay(action_for_relay),
                                              task_repeat(task_repeat) {}
};

// описание типов модулей
enum srModuleType : uint8_t
{
  mtRelay,
  mtSwitch,
  mtTasks,
  mtNtp
};

// описание свойств реле
struct shRelayData
{
  String relayName;          // имя реле
  int8_t relayPin;           // пин, к которому подключено реле
  uint8_t relayControlLevel; // управляющий уровень реле (LOW или HIGH)
  bool relayLastState;       // последнее состояние реле
  String relayDescription;   // описание реле
  srButton *relayButton;     // указатель на локальную кнопку, привязанную к реле

  shRelayData() : relayName(""),
                  relayPin(-1),
                  relayControlLevel(HIGH),
                  relayDescription(""),
                  relayLastState(false),
                  relayButton(NULL) {}

  shRelayData(String relay_name,
              uint8_t relay_pin,
              uint8_t control_level,
              int8_t relay_button_pin = -1,
              String relay_description = "") : relayName(relay_name),
                                               relayPin(relay_pin),
                                               relayControlLevel(control_level),
                                               relayDescription(relay_description),
                                               relayLastState(false),
                                               relayButton((relay_button_pin < 0) ? NULL
                                                                                  : new (std::nothrow) srButton(relay_button_pin)) {}
};

// описание свойств выключателя
struct shSwitchData
{
  String relayName;        // имя ассоциированного с кнопкой удаленного реле
  bool relayFound;         // найдено или нет ассоциированное удаленное реле в сети
  IPAddress relayAddress;  // IP адрес удаленного реле
  String relayDescription; // описание удаленного реле
  srButton *relayButton;   // указатель на локальную кнопку, привязанную к реле

  shSwitchData() : relayName(""),
                   relayDescription(""),
                   relayFound(false),
                   relayAddress(IPAddress(0ul)),
                   relayButton(NULL) {}

  shSwitchData(String relay_name,
               int8_t relay_button_pin = -1) : relayName(relay_name),
                                               relayDescription(""),
                                               relayFound(false),
                                               relayAddress(IPAddress(0ul)),
                                               relayButton((relay_button_pin < 0) ? NULL
                                                                                  : new (std::nothrow) srButton(relay_button_pin)) {}
};

// =====================================================

#ifdef SH_LOG_OFF
#define SWITCH_RELAY_LOG_OFF
#endif

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
  int8_t getRelayIndexByName(String _name);

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
   * @param relay_name имя локального реле
   * @param relay_pin пин, к которому подключено реле
   * @param control_level логический уровень, которым управляется реле (HIGH или LOW)
   * @param relay_button локальная кнопка для управления реле или nullptr, если локальной кнопки нет
   * @param relay_description описание реле
   * @return true если реле добавлено успешно, иначе false
   */
  bool addRelay(const String &relay_name,
                int8_t relay_pin,
                uint8_t control_level,
                int8_t relay_button_pin = -1,
                const String &relay_description = "");

  /**
   * @brief установка порта для отправки/приема udp-пакетов
   *
   * @param _udp_port порт для отправки/приема udp-пакетов; должен быть
   *                  одинаковым для всех связанных модулей
   */
  void setUdpPort(uint16_t _udp_port);

  /**
   * @brief получение текущего порта для отправки/приема udp-пакетов
   *
   * @return uint16_t
   */
  uint16_t getUdpPort();

#ifndef SWITCH_RELAY_LOG_OFF
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
#endif

  /**
   * @brief включить/выключить озвучивание нажатия локальных кнопок
   *
   * @param _state новое состояние опции
   * @param _pin пин, к которому подключен буззер
   */
  void setButtonBuzzerState(bool _state, int8_t _pin = -1);

  /**
   * @brief настройка звукового сигнала для нажатия локальных кнопок
   *
   * @param _freq частота в Гц
   * @param _dur длительность сигнала в мс
   */
  void setBtnBeepData(uint16_t _freq, uint32_t _dur);

  /**
   * @brief запуск модуля
   *
   * @param _udp ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля
   * @param _udp_port порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей
   *
   * @return true если сервер UDP запущен успешно, иначе false
   */
  bool startUdpInteraction(WiFiUDP *_udp, uint16_t _udp_port);

  /**
   * @brief запуск модуля
   *
   * @param _udp ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля
   *
   * @return true если сервер UDP запущен успешно, иначе false
   */
  bool startUdpInteraction(WiFiUDP *_udp);

  /**
   * @brief подключение Web-интерфейса
   *
   * @param _server ссылка на экземпляр Web-сервера, с которым будет работать модуль
   * @param _file_system ссылка на экземпляр файловой системы модуля для сохранения файла с настройками
   * @param _tasks_config_page адрес, по которому будет вызываться Web-страница настройки WiFi
   * @param _wifi_config_page адрес, по которому будет вызываться Web-страница настройки WiFi
   */
  void attachWebInterface(shWebServer *_server,
                          FS *_file_system,
                          const String &_tasks_config_page = "",
                          const String &_wifi_config_page = "");

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
   * @brief установить состояние всех локальных реле
   *
   * @param state новое состояние реле; true - включено, иначе выключено;
   */
  void setStateForAll(bool state);

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
  void setModuleDescription(String &_descr);

  /**
   * @brief получение текущего описания модуля
   *
   * @return String
   */
  String getModuleDescription();

  /**
   * @brief установка имени модуля
   *
   * @param _name новое имя
   */
  void setModuleName(const String &_name);

  /**
   * @brief получение текущего имени модуля
   *
   * @return String
   */
  String getModuleName();

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
  void setRelayName(int8_t index, String &_name);

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
  void setRelayDescription(int8_t index, String &_descr);

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
   * @param _mdl тип модуля, для которого нужно получить имя файла конфигурации
   *
   */
  void setFileName(const String &_name, srModuleType _mdl = mtRelay);

  /**
   * @brief получение текущего имени файла конфигурации модуля
   *
   * @param _mdl тип модуля, для которого нужно получить имя файла конфигурации
   *
   * @return String
   */
  String getFileName(srModuleType _mdl = mtRelay);

  /**
   * @brief сохранение настроек в файл
   *
   * @param _mdl тип модуля, для которого нужно сохранить настройки
   *
   * @return true
   * @return false
   */
  bool saveConfig(srModuleType _mdl = mtRelay);

  /**
   * @brief считывание и загрузка сохраненных в файле настроек
   *
   * @param _mdl тип модуля, для которого нужно загрузить настройки
   *
   * @return true
   * @return false
   */
  bool loadConfig(srModuleType _mdl = mtRelay);

  /**
   * @brief установить время модуля (без смещения по часовому поясу)
   *
   * @param _now время в формате unix time
   */
  void setTimeToModule(time_t _now);

  /**
   * @brief получить текущее время модуля (без смещения по часовому поясу)
   *
   * @return time_t время в формате unix time
   */
  time_t getTimeFromModule();

  /**
   * @brief установить смещение времени по часовому поясу
   *
   * @param _tz смещение в минутах
   */
  void setTimeZoneOffset(time_t _tz);

  /**
   * @brief получить текущее смещение времени по часовому поясу
   *
   * @return time_t смещение в минутах
   */
  time_t getTimeZoneOffset();

  /**
   * @brief установка данных для работы с NTP-серверами
   *
   * @param _enabled включение/отключение работы с NTP-серверами
   * @param _tz смещение времени по часовому поясу в минутах (по умолчанию - 3 часа, Московское время)
   * @param _ntp_srv1 первый NTP-сервер
   * @param _ntp_srv2 второй NTP-сервер
   * @param _ntp_srv3 третий NTP-сервер
   */
  void setNtpData(bool _enabled,
                  time_t _tz = 180,
                  String _ntp_srv1 = "",
                  String _ntp_srv2 = "",
                  String _ntp_srv3 = "");

  /**
   * @brief получить статус работы с NTP-серверами
   *
   * @return true - включено
   * @return false - выключено
   */
  bool getNtpStatus();

  /**
   * @brief получить первый NTP-сервер
   *
   * @return String
   */
  String getNtpServer1();

  /**
   * @brief получить второй NTP-сервер
   *
   * @return String
   */
  String getNtpServer2();

  /**
   * @brief получить третий NTP-сервер
   *
   * @return String
   */
  String getNtpServer3();

  /**
   * @brief добавление задачи в планировщик
   *
   * @param _task структура, описывающая задачу
   *
   * @return true
   * @return false
   */
  bool addTaskToList(shSchedulerData &_task);

  /**
   * @brief поиск задачи с заданными параметрами
   *
   * @param _task структура, описывающая искомую задачу
   * @return uint8_t
   */
  uint8_t findTask(shSchedulerData &_task);

  /**
   * @brief поиск задачи с заданными параметрами
   *
   * @param _time_for_task время срабатывания задачи, минут с начала суток
   * @param _relay_index индекс реле задачи
   * @param _action_for_relay действие с реле (включить/выключить)
   * @param _task_repeat периодичность выполнения задачи
   *
   * @return int8_t индекс найденной задачи в массиве или -1 в случае, если задача не найдена
   *
   */
  int8_t findTask(int32_t _time_for_task,
                  uint8_t _relay_index,
                  bool _action_for_relay,
                  shTaskRepeat _task_repeat);

  /**
   * @brief удаление задачи из списка планировщика
   *
   * @param _idx индекс задачи в списке планировщика
   * @return true
   * @return false
   */
  bool delTaskInList(uint8_t _idx);

  /**
   * @brief получить количество задач в планировщике
   *
   * @param is_active считать только активные задачи
   * @return uint8_t
   */
  uint8_t getTaskCount(bool is_active = false);

  /**
   * @brief установить статус задачи - активна / выключена
   *
   * @param _idx индекс задачи в списке планировщика
   * @param _state новый статус задачи
   */
  void setTaskStatus(uint8_t _idx, bool _state);

  /**
   * @brief изменить данные существующей задачи
   *
   * @param _idx индекс задачи в списке планировщика
   * @param _task новые данные задачи
   * @return true
   * @return false
   */
  bool setTaskData(uint8_t _idx, shSchedulerData &_task);

  /**
   * @brief получение текущего состояния кнопки
   *
   * @param _idx индекс в массиве реле, к которому привязана кнопка
   * @return uint8_t
   */
  uint8_t getButtonState(uint8_t _idx);

  /**
   * @brief возвращает true, если по результатам последнего вызова метода getButtonState() кнопка нажата
   *
   * @return true
   * @return false
   */
  bool isButtonClosed(uint8_t _idx);

  /**
   * @brief определение одновременного нажатия двух кнопок; возвращает true, если в момент возникновения события btn_state кнопки с индексом _idx кнопка _but уже нажата; если метод вернул true, состояние обеих кнопок сбрасывается
   *
   * @param _but кнопка, состояние которой надо проверить в момент возникновения события btn_state текущей кнопки
   * @param btn_state состояние, которое отслеживается методом; по умолчанию обычный клик
   * @return true
   * @return false
   */
  bool isSecondButtonPressed(uint8_t _idx, uint8_t _but_2, uint8_t btn_state = BTN_DOWN);

  /**
   * @brief принудительный сброс состояния кнопки; может понадобиться, если по каким-то причинам нужно, например, исключить возникновение событий BTN_ONECLICK и BTN_DBLCLICK
   *
   */
  void resetButtonState(uint8_t _idx);

  /**
   * @brief установка режима обработки удержания кнопки нажатой
   *
   * @param longclickmode новый режим обработки; возможные варианты - LCM_CONTINUED, LCM_ONLYONCE или LCM_CLICKSERIES
   */
  void setButtonLongClickMode(uint8_t _idx, uint8_t longclickmode);

  /**
   * @brief установка интервала удержания кнопки (значение по умолчанию 1000 мс)
   *
   * @param new_timeout новый интервал в милисекундах
   */
  void setButtonTimeoutOfLongClick(uint8_t _idx, uint16_t new_timeout);

  /**
   * @brief установка интервала подавления дребезга контактов кнопки (значение по умолчанию 50 мс)
   *
   * @param new_timeout
   */
  void setButtonTimeoutOfDebounce(uint8_t _idx, uint16_t new_timeout);
};

// ==== shSwitchControl class ==========================

/**
 * @brief класс модуля WiFi-выключателя
 *
 */
class shSwitchControl
{
private:
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String _name);

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
   * @param relay_name имя удаленного реле
   * @param relay_button кнопка, управляющая удаленным реле или nullptr, если управляющей кнопки нет
   * @return true если данные добавлены успешно, иначе false
   */
  bool addRelay(const String &relay_name,
                int8_t relay_button_pin = -1);

  /**
   * @brief установка порта для отправки/приема udp-пакетов
   *
   * @param _udp_port порт для отправки/приема udp-пакетов; должен быть
   *                  одинаковым для всех связанных модулей
   */
  void setUdpPort(uint16_t _udp_port);

  /**
   * @brief получение текущего порта для отправки/приема udp-пакетов
   *
   * @return uint16_t
   */
  uint16_t getUdpPort();

#ifndef SWITCH_RELAY_LOG_OFF
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
#endif

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
   * @brief настройка звукового сигнала для нажатия кнопок
   *
   * @param _freq частота в Гц
   * @param _dur длительность сигнала в мс
   */
  void setBtnBeepData(uint16_t _freq, uint32_t _dur);

  /**
   * @brief установка интервала проверки доступности связанных реле в сети в милисекундах; по умолчанию установлен интервал в 60 секунд
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
   * @param _udp_port порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей
   *
   * @return true если сервер UDP запущен успешно, иначе false
   */
  bool startUdpInteraction(WiFiUDP *_udp, uint16_t _udp_port);

  /**
   * @brief запуск модуля
   *
   * @param _udp ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля
   *
   * @return true если сервер UDP запущен успешно, иначе false
   */
  bool startUdpInteraction(WiFiUDP *_udp);

  /**
   * @brief подключение Web-интерфейса
   *
   * @param _server ссылка на экземпляр Web-сервера, с которым будет работать модуль
   * @param _file_system ссылка на экземпляр файловой системы модуля для сохранения файла с настройками
   * @param _tasks_config_page адрес, по которому будет вызываться Web-страница настройки WiFi
   * @param _wifi_config_page адрес, по которому будет вызываться Web-страница настройки WiFi
   */
  void attachWebInterface(shWebServer *_server,
                          FS *_file_system,
                          const String &_tasks_config_page = "",
                          const String &_wifi_config_page = "");

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
  void setModuleDescription(const String &_descr);

  /**
   * @brief получение текущего описания модуля
   *
   * @return String
   */
  String getModuleDescription();

  /**
   * @brief установка имени модуля
   *
   * @param _name новое имя
   */
  void setModuleName(const String &_name);

  /**
   * @brief получение текущего имени модуля
   *
   * @return String
   */
  String getModuleName();

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
   * @param _mdl тип модуля, для которого нужно установить имя файла конфигурации
   */
  void setFileName(const String &_name, srModuleType _mdl = mtSwitch);

  /**
   * @brief получение текущего имени файла конфигурации модуля
   *
   * @param _mdl тип модуля, для которого нужно получить имя файла конфигурации
   *
   * @return String
   */
  String getFileName(srModuleType _mdl = mtSwitch);

  /**
   * @brief сохранение настроек в файл
   *
   * @param _mdl тип модуля, для которого нужно сохранить настройки
   *
   * @return true
   * @return false
   */
  bool saveConfig(srModuleType _mdl = mtSwitch);

  /**
   * @brief считывание и загрузка сохраненных в файле настроек
   *
   * @param _mdl тип модуля, для которого нужно загрузить настройки
   *
   * @return true
   * @return false
   */
  bool loadConfig(srModuleType _mdl = mtSwitch);

  /**
   * @brief установить время модуля (без смещения по часовому поясу)
   *
   * @param _now время в формате unix time
   */
  void setTimeToModule(time_t _now);

  /**
   * @brief получить текущее время модуля (без смещения по часовому поясу)
   *
   * @return time_t время в формате unix time
   */
  time_t getTimeFromModule();

  /**
   * @brief установить смещение времени по часовому поясу
   *
   * @param _tz смещение в минутах
   */
  void setTimeZoneOffset(time_t _tz);

  /**
   * @brief получить текущее смещение времени по часовому поясу
   *
   * @return time_t смещение в минутах
   */
  time_t getTimeZoneOffset();

  /**
   * @brief установка данных для работы с NTP-серверами
   *
   * @param _enabled включение/отключение работы с NTP-серверами
   * @param _tz смещение времени по часовому поясу в минутах (по умолчанию - 3 часа, Московское время)
   * @param _ntp_srv1 первый NTP-сервер
   * @param _ntp_srv2 второй NTP-сервер
   * @param _ntp_srv3 третий NTP-сервер
   */
  void setNtpData(bool _enabled,
                  time_t _tz = 180,
                  String _ntp_srv1 = "",
                  String _ntp_srv2 = "",
                  String _ntp_srv3 = "");

  /**
   * @brief получить статус работы с NTP-серверами
   *
   * @return true - включено
   * @return false - выключено
   */
  bool getNtpStatus();

  /**
   * @brief получить первый NTP-сервер
   *
   * @return String
   */
  String getNtpServer1();

  /**
   * @brief получить второй NTP-сервер
   *
   * @return String
   */
  String getNtpServer2();

  /**
   * @brief получить третий NTP-сервер
   *
   * @return String
   */
  String getNtpServer3();

  /**
   * @brief добавление задачи в планировщик
   *
   * @param _task структура, описывающая задачу
   *
   * @return true
   * @return false
   */
  bool addTaskToList(shSchedulerData &_task);

  /**
   * @brief поиск задачи с заданными параметрами
   *
   * @param _task структура, описывающая искомую задачу
   * @return uint8_t
   */
  uint8_t findTask(shSchedulerData &_task);

  /**
   * @brief поиск задачи с заданными параметрами
   *
   * @param _time_for_task время срабатывания задачи, минут с начала суток
   * @param _relay_index индекс реле задачи
   * @param _action_for_relay действие с реле (включить/выключить)
   * @param _task_repeat периодичность выполнения задачи
   *
   * @return int8_t индекс найденной задачи в массиве или -1 в случае, если задача не найдена
   *
   */
  int8_t findTask(int32_t _time_for_task,
                  uint8_t _relay_index,
                  bool _action_for_relay,
                  shTaskRepeat _task_repeat);

  /**
   * @brief удаление задачи из списка планировщика
   *
   * @param _idx индекс задачи в списке планировщика
   * @return true
   * @return false
   */
  bool delTaskInList(uint8_t _idx);

  /**
   * @brief получить количество задач в планировщике
   *
   * @param is_active считать только активные задачи
   * @return uint8_t
   */
  uint8_t getTaskCount(bool is_active = false);

  /**
   * @brief установить статус задачи - активна / выключена
   *
   * @param _idx индекс задачи в списке планировщика
   * @param _state новый статус задачи
   */
  void setTaskStatus(uint8_t _idx, bool _state);

  /**
   * @brief изменить данные существующей задачи
   *
   * @param _idx индекс задачи в списке планировщика
   * @param _task новые данные задачи
   * @return true
   * @return false
   */
  bool setTaskData(uint8_t _idx, shSchedulerData &_task);

  /**
   * @brief получение текущего состояния кнопки
   *
   * @param _idx индекс в массиве реле, к которому привязана кнопка
   * @return uint8_t
   */
  uint8_t getButtonState(uint8_t _idx);

  /**
   * @brief возвращает true, если по результатам последнего вызова метода getButtonState() кнопка нажата
   *
   * @return true
   * @return false
   */
  bool isButtonClosed(uint8_t _idx);

  /**
   * @brief определение одновременного нажатия двух кнопок; возвращает true, если в момент возникновения события btn_state кнопки с индексом _idx кнопка _but уже нажата; если метод вернул true, состояние обеих кнопок сбрасывается
   *
   * @param _but кнопка, состояние которой надо проверить в момент возникновения события btn_state текущей кнопки
   * @param btn_state состояние, которое отслеживается методом; по умолчанию обычный клик
   * @return true
   * @return false
   */
  bool isSecondButtonPressed(uint8_t _idx, uint8_t _but_2, uint8_t btn_state = BTN_DOWN);

  /**
   * @brief принудительный сброс состояния кнопки; может понадобиться, если по каким-то причинам нужно, например, исключить возникновение событий BTN_ONECLICK и BTN_DBLCLICK
   *
   */
  void resetButtonState(uint8_t _idx);

  /**
   * @brief установка режима обработки удержания кнопки нажатой
   *
   * @param longclickmode новый режим обработки; возможные варианты - LCM_CONTINUED, LCM_ONLYONCE или LCM_CLICKSERIES
   */
  void setButtonLongClickMode(uint8_t _idx, uint8_t longclickmode);

  /**
   * @brief установка интервала удержания кнопки (значение по умолчанию 1000 мс)
   *
   * @param new_timeout новый интервал в милисекундах
   */
  void setButtonTimeoutOfLongClick(uint8_t _idx, uint16_t new_timeout);

  /**
   * @brief установка интервала подавления дребезга контактов кнопки (значение по умолчанию 50 мс)
   *
   * @param new_timeout
   */
  void setButtonTimeoutOfDebounce(uint8_t _idx, uint16_t new_timeout);
};

// ==== shBuzzer class ===============================

class shBuzzer
{
private:
  Ticker buzzer;
  int8_t buzzer_pin = -1;
  uint8_t beep_count = 0;
  uint16_t error_freq = 500;
  uint32_t error_dur = 50;
  bool buzzer_state = false;
  uint16_t btn_beep_freq = 2500;
  uint32_t btn_beep_dur = 50;

public:
  shBuzzer();

  void setState(bool _state, int8_t _pin = -1);
  bool getState();
  void startBuzzer(uint8_t _num, uint16_t _freq = 500, uint32_t _dur = 50);
  void stopBuzzer();
  void beep();
  void btnBeep();
  void setBtnBeepData(uint16_t _freq, uint32_t _dur);
  uint8_t decBipCount();
};
