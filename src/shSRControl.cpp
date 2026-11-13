#if defined(ARDUINO_ARCH_ESP8266)
#include <sntp.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <lwip/apps/sntp.h>
#endif
#include "shSRControl.h"
#include <new>
#include <time.h>
#include <ArduinoJson.h>
#include "extras/c_page.h"
#include "extras/i_page.h"
#include "extras/t_page.h"

#ifdef SWITCH_RELAY_LOG_OFF

#define SR_PRINT(x)
#define SR_PRINTLN(x)

#else

#define SR_PRINT(x)           \
  {                           \
    if (logOnState && serial) \
      serial->print(x);       \
  }
#define SR_PRINTLN(x)         \
  {                           \
    if (logOnState && serial) \
      serial->println(x);     \
  }
#endif

// ==== имена параметров в запросах/ответах ==========
static const String sr_name_str = "name";
static const String sr_command_str = "command";
static const String sr_response_str = "resp";
static const String sr_for_str = "for";
static const String sr_descr_str = "descr";
static const String sr_type_str = "type";
static const String sr_relays_str = "relays";
static const String sr_module_description_str = "module_descr";
static const String sr_module_name_str = "module_name";
static const String sr_udp_port_str = "udp_port";
static const String sr_udp_en_str = "udp_en";
static const String sr_check_interval_str = "check_int";
static const String sr_last_state_str = "last";
static const String sr_ip_addr_str = "addr";
static const String sr_wificonf_str = "wificonf";
static const String sr_tasksconf_str = "tasksconf";
static const String sr_save_state_str = "save_state";
static const String sr_ntp_enabled_str = "ntp_enabled";
static const String sr_ntp_server1_str = "ntp_server1";
static const String sr_ntp_server2_str = "ntp_server2";
static const String sr_ntp_server3_str = "ntp_server3";
static const String sr_unix_str = "unix";
static const String sr_ms_str = "ms";
static const String sr_tz_str = "tz";
static const String sr_tmax_str = "tmax";

static const String sr_t_list_str = "t_list";
static const String sr_r_list_str = "r_list";
static const String sr_state_str = "state";
static const String sr_time_str = "time";
static const String sr_rel_str = "rel";
static const String sr_act_str = "act";
static const String sr_rep_str = "rep";

// ==== значения параметров в запросах/ответах =======
static const String sr_ok_str = "ok";
static const String sr_no_str = "no";
static const String sr_on_str = "on";
static const String sr_off_str = "off";
static const String sr_relay_str = "relay";
static const String sr_switch_str = "switch";
static const String sr_set_on_str = "set_on";
static const String sr_set_off_str = "set_off";
static const String sr_respond_str = "respond";
static const String sr_any_str = "any_relay";
static const String sr_relay_module_str = "r_module";
static const String sr_switch_module_str = "s_module";

/*
строка запроса от выключателя - имя реле и команда: отозваться, если идет поиск, или выполнить команду
{"name":"any_relay","command":"respond"}
{"name":"relay1","command":"switch"}

строка ответа реле - имя реле, описание, на что отвечает и ответ: состояние реле после выполнения команды или "ok" в случае ответа на поиск;
{"name":"relay1","descr":"Розетка у окна","for":"switch","resp":"off"}
{"name":"relay1","descr":"Розетка у окна","for":"respond","resp":"ok"}
*/

static const char TEXT_PLAIN[] PROGMEM = "text/plain";
static const char TEXT_HTML[] PROGMEM = "text/html";
static const char TEXT_JSON[] PROGMEM = "text/json";
static const char APP_JSON[] PROGMEM = "application/json";
static const char RELAY_CONFIG_PAGE[] PROGMEM = "/config_page";
static const char GET_RELAY_CONFIG[] PROGMEM = "/get_config";
static const char GET_RELAY_LIST[] PROGMEM = "/get_relay_list";
static const char SR_SET_CONFIG[] PROGMEM = "/sr_setconfig";
static const char SR_RESTART[] PROGMEM = "/sr_restart";
static const char GET_RELAY_STATE[] PROGMEM = "/relay_getstate";
static const char RELAY_SWITCH[] PROGMEM = "/relay_switch";
static const char REMOTE_RELAY_SWITCH[] PROGMEM = "/remote_switch";
static const char SR_TASKS[] PROGMEM = "/sr_tasks";
static const char TIME_DATA[] PROGMEM = "/time";
static const char NTP_DATA[] PROGMEM = "/ntp";

// константы для работы с JSON
#if defined(ARDUINO_ARCH_ESP8266)
static const uint16_t CONFIG_SIZE = 2048;
static const uint8_t MAX_TASKS_COUNT = 16;
static const uint8_t MAX_RELAY_COUNT = 10;
// static const uint16_t RELAY_LIST_DATA_SIZE = 256;
#else
static const uint16_t CONFIG_SIZE = 4096;
static const uint8_t MAX_TASKS_COUNT = 32;
static const uint8_t MAX_RELAY_COUNT = 20;
// static const uint16_t RELAY_LIST_DATA_SIZE = 512;
#endif
static const uint16_t RELAY_DATA_SIZE = 256;

// ==== общие данные =================================

static srModuleType moduleFlag = mtRelay;

static shRelayData *relayArray = NULL;
static shSwitchData *switchArray = NULL;
static int8_t relayCount = 0;

static shSchedulerData *tasksArray = NULL;
static uint8_t tasksCount = 0;

static String relayFileConfigName = "/relay.json";
static String switchFileConfigName = "/switch.json";
static String tasksFileConfigName = "/tasks.json";
static String ntpFileConfigName = "/ntp.json";

static uint32_t checkInterval = SR_DEFAULT_CHECK_INTERVAL;
static String listOfRemoteRelays[SR_SIZE_LIST_OF_REMOTE_RELAYS];

static bool ntp_enabled = true;
static String ntp_server1 = "ru.pool.ntp.org";
static String ntp_server2 = "pool.ntp.org";
static String ntp_server3 = "time.google.com";
static time_t tz_offset = 10800;

static String module_description = "";
static String module_name = "";
static bool save_state_of_relay = false;

#ifndef SWITCH_RELAY_LOG_OFF
static Print *serial = NULL;
static bool logOnState = true;
#endif

static shWebServer *http_server = NULL;
static FS *file_system = NULL;
static WiFiUDP *udp = NULL;
static uint16_t udpPort = SR_DEFAULT_UDP_PORT;

static shBuzzer bzr;

static String wifi_config_page = "";
static String tasks_config_page = "";

// ===================================================

static IPAddress get_broadcast_address();
static bool get_value_of_argument(String &_res, const String &_arg, String &_str);
static bool send_udp_packet(const IPAddress &address, const char *buf, size_t bufSize);
static bool get_udp_string(String &_str, int _size);
static String get_argument(String &_res, const String &_arg);
static String get_json_string_to_send(const String &_name, const String &_comm);
static String get_json_string_to_send(const String &_name,
                                      const String &_type,
                                      const String &_descr,
                                      const String &_comm,
                                      const String &_for);

static void switch_local_relay(int8_t index);
static void set_local_relay_state(int8_t index, bool state);
static String get_relay_state(int8_t index);

static void switch_remote_relay(int8_t index);
static void set_remote_relay_state(int8_t index, bool state);
static void set_all_remote_relay_state(bool state);
static void send_command_for_relay(int8_t index, const String &command);

static void find_remote_relays();

static void set_udp_port(uint16_t _udp_port);
static uint16_t get_udp_port();
static bool start_udp_interaction(WiFiUDP *_udp, uint16_t _udp_port);

static int8_t addRelayToListOfRemoteRelays(const String &relay_name);
static void clearListOfRemoteRelays();

// ===================================================

static void checkTasksList(srModuleType _mdl);
static int8_t find_task(shSchedulerData &_task);
static bool add_task_to_list(shSchedulerData &_task);
static bool del_task_in_list(uint8_t _idx);
static void set_task_status(uint8_t _idx, bool _state);
static bool set_task_data(uint8_t _idx, shSchedulerData &_task);
static uint8_t get_task_count(bool is_active = false);
static void set_local_time(time_t _now);
static time_t get_local_time();
static void set_time_zone_offset(time_t _tz);
static void set_ntp_data(bool _ntp_enabled,
                         time_t _tz = 180,
                         String _ntp_server1 = "",
                         String _ntp_server2 = "",
                         String _ntp_server3 = "");

// ===================================================
static uint8_t get_button_state(uint8_t _idx, srModuleType _mdl);
static bool is_button_closed(uint8_t _idx, srModuleType _mdl);
static bool is_second_button_pressed(uint8_t _idx,
                                     uint8_t _but_2,
                                     srModuleType _mdl,
                                     uint8_t btn_state = BTN_DOWN);
static void reset_button_state(uint8_t _idx, srModuleType _mdl);
static void set_long_click_mode(uint8_t _idx, uint8_t longclickmode, srModuleType _mdl);
static void set_timeout_of_long_click(uint8_t _idx, uint16_t new_timeout, srModuleType _mdl);
static void set_timeout_of_debounce(uint8_t _idx, uint16_t new_timeout, srModuleType _mdl);

// ===================================================
static void handleGetConfigPage();

static void handleGetIndexPage();

static void handleGetConfig(String _msg);
static void handleGetRelayConfig();
static void handleGetSwitchConfig();
static void handleGetListOfRemoteRelays();

static void setHttpHandleForScheduler();
static void handleGetConfigSchedulerPage();
static void handleReadTasksSetting();
static void handleWriteTasksSetting();
static void handleReadTimeData();
static void handleWriteTimeData();
static void handleReadNtpData();
static void handleWriteNtpData();

static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const int8_t _last = -1);
static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const IPAddress _ip);
static void get_config_json_doc(DynamicJsonDocument &doc, srModuleType _mdl);
static String get_config_json_string(srModuleType _mdl);

static void handleSetConfig();

static void handleRelaySwitch();
static void handleRemoteRelaySwitch();
static void handleGetRelayState();

static void handleRestartModule();

// ===================================================
static bool load_setting(srModuleType _mdl, DynamicJsonDocument &doc);

static void set_config_file_name(const String &_name, srModuleType _mdl);
static String get_config_file_name(srModuleType _mdl);
static bool save_config_file(srModuleType _mdl);
static bool save_config_file(srModuleType _mdl, DynamicJsonDocument &doc);
static bool load_config_file(srModuleType _mdl);

// оператор сравнения двух задач (если понадобится определять дубликаты)
bool operator==(const shSchedulerData &a, const shSchedulerData &b)
{
  // состояние статуса не учитываем, задача может быть активна или отключена
  // тем не менее она остается той же самой задачей
  return (a.time_for_task == b.time_for_task &&
          a.relay_index == b.relay_index &&
          a.action_for_relay == b.action_for_relay &&
          a.task_repeat == b.task_repeat);
}

// ==== shRelayControl class ===========================

shRelayControl::shRelayControl() { moduleFlag = mtRelay; }

void shRelayControl::init(uint8_t _relay_count)
{
  _relay_count = (_relay_count > MAX_RELAY_COUNT) ? MAX_RELAY_COUNT : _relay_count;

  // защита от утечки памяти при повторном вызове init()
  if (relayArray)
  {
    delete[] relayArray;
  }

  relayArray = new (std::nothrow) shRelayData[_relay_count];
  if (relayArray)
  {
    relayCount = _relay_count;
  }

#ifndef SWITCH_RELAY_LOG_OFF
  if (Serial != NULL)
  {
    serial = &Serial;
  }
#endif
}

bool shRelayControl::addRelay(const String &relay_name,
                              int8_t relay_pin,
                              uint8_t control_level,
                              int8_t relay_button_pin,
                              const String &relay_description)
{
  bool result = false;
  if (relayCount > 0 && relayArray != NULL)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayName == "" && relayArray[i].relayPin < 0)
      {
        relayArray[i] = shRelayData(relay_name,
                                    relay_pin,
                                    control_level,
                                    relay_button_pin,
                                    relay_description);

        pinMode(relay_pin, OUTPUT);
        digitalWrite(relay_pin, !control_level);
        result = true;
        break;
      }
    }
  }
  return (result);
}

void shRelayControl::setUdpPort(uint16_t _udp_port)
{
  set_udp_port(_udp_port);
}

uint16_t shRelayControl::getUdpPort()
{
  return (get_udp_port());
}

#ifndef SWITCH_RELAY_LOG_OFF
void shRelayControl::setLogOnState(bool _on, Print *_serial)
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shRelayControl::getLogOnState() { return (logOnState); }
#endif

void shRelayControl::setButtonBuzzerState(bool _state, int8_t _pin)
{
  bzr.setState(_state, _pin);
}

void shRelayControl::setBtnBeepData(uint16_t _freq, uint32_t _dur)
{
  bzr.setBtnBeepData(_freq, _dur);
}

bool shRelayControl::startUdpInteraction(WiFiUDP *_udp, uint16_t _udp_port)
{
  return start_udp_interaction(_udp, _udp_port);
}

bool shRelayControl::startUdpInteraction(WiFiUDP *_udp)
{
  return start_udp_interaction(_udp, udpPort);
}

void shRelayControl::attachWebInterface(shWebServer *_server,
                                        FS *_file_system,
                                        const String &_tasks_config_page,
                                        const String &_wifi_config_page)
{
  http_server = _server;
  file_system = _file_system;

  if (load_config_file(mtRelay) && save_state_of_relay)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayLastState)
      {
        uint8_t lev = (relayArray[i].relayControlLevel) ? HIGH : LOW;
        digitalWrite(relayArray[i].relayPin, lev);
      }
    }
  }

  wifi_config_page = _wifi_config_page;
  tasks_config_page = _tasks_config_page;

  if (wifi_config_page.length() > 0 && wifi_config_page.indexOf("/") != 0)
  {
    wifi_config_page = "/" + wifi_config_page;
  }
  if (tasks_config_page.length() > 0 && tasks_config_page.indexOf("/") != 0)
  {
    tasks_config_page = "/" + tasks_config_page;
  }

  if (http_server)
  { // вызов стартовой страницы модуля реле
    http_server->on("/", HTTP_GET, handleGetIndexPage);
    // вызов страницы настройки модуля реле
    http_server->on(FPSTR(RELAY_CONFIG_PAGE), HTTP_GET, handleGetConfigPage);
    // запрос текущих настроек
    http_server->on(FPSTR(GET_RELAY_CONFIG), HTTP_GET, handleGetRelayConfig);
    // сохранение настроек
    http_server->on(FPSTR(SR_SET_CONFIG), HTTP_POST, handleSetConfig);
    // переключение реле
    http_server->on(FPSTR(RELAY_SWITCH), HTTP_POST, handleRelaySwitch);
    // запрос текущего состояния всех реле
    http_server->on(FPSTR(GET_RELAY_STATE), HTTP_GET, handleGetRelayState);
    // перезагрузка модуля
    http_server->on(FPSTR(SR_RESTART), HTTP_GET, handleRestartModule);

    // настройка web-интерфейса планировщика задач
    setHttpHandleForScheduler();
  }
}

void shRelayControl::tick()
{
  // опрос кнопок
  if (relayArray != NULL)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayButton != NULL &&
          relayArray[i].relayButton->getButtonState() == BTN_DOWN)
      {
        bzr.btnBeep(); // одиночный пик на каждое нажатие любой кнопки
        switch_local_relay(i);
      }
    }
  }

  // проверка списка планировщика задач
  static unsigned long tasks_timer = 0;
  if (millis() - tasks_timer >= 1000ul)
  {
    tasks_timer = millis();
    checkTasksList(mtRelay);
  }

  // проверка входящих udp-пакетов
  if (udp != NULL)
  {
    int packet_size = udp->parsePacket();
    if (packet_size > 0)
    {
      receiveUdpPacket(packet_size);
    }
  }

  // обработка запросов сервера
  if (http_server)
  {
    http_server->handleClient();
  }
  delay(1);
}

void shRelayControl::respondToRelayCheck(int8_t index)
{
  if (udp == NULL)
  {
    SR_PRINTLN(F("The packet could not be sent; no UDP object was specified."));
    return;
  }

  if ((index >= 0) && (index < relayCount))
  {
    String s = get_json_string_to_send(relayArray[index].relayName,
                                       sr_relay_module_str,
                                       relayArray[index].relayDescription,
                                       sr_ok_str,
                                       sr_respond_str);
    SR_PRINT(relayArray[index].relayName);
    SR_PRINT(F(": request received, response - "));
    SR_PRINTLN(sr_ok_str);
    send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
  }
}

static void set_state(int8_t relay_index, String comm)
{
  if ((relay_index >= 0) && (relay_index < relayCount))
  {
    if (comm == sr_switch_str)
    {
      switch_local_relay(relay_index);
    }
    else if ((comm == sr_set_on_str) || (comm == sr_set_off_str))
    {
      set_local_relay_state(relay_index, (comm == sr_set_on_str));
    }

    String s = get_json_string_to_send(relayArray[relay_index].relayName,
                                       sr_relay_module_str,
                                       relayArray[relay_index].relayDescription,
                                       get_relay_state(relay_index),
                                       comm);
    send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
  }
}

void shRelayControl::receiveUdpPacket(int _size)
{
  String _resp;
  // пакет не прочитан - не отвечаем на него, иначе модуль будет отвечать
  // на мусор и на заведомо неподходящие пакеты
  if (!get_udp_string(_resp, _size))
  {
    return;
  }
  String arg_comm = get_argument(_resp, sr_command_str);
  String arg_name = get_argument(_resp, sr_name_str);

  // если получен запрос на отклик
  if (arg_comm == sr_respond_str)
  {
    if (arg_name == sr_any_str)
    {
      for (uint8_t i = 0; i < relayCount; i++)
      {
        respondToRelayCheck(i);
      }
    }
    else
    {
      respondToRelayCheck(getRelayIndexByName(arg_name));
    }
  }
  // иначе выполнить команду на переключение реле
  else
  {
    if ((arg_comm == sr_switch_str) ||
        (arg_comm == sr_set_on_str) ||
        (arg_comm == sr_set_off_str))
    {
      // всех сразу
      if (arg_name == sr_any_str)
      {
        for (uint8_t i = 0; i < relayCount; i++)
        {
          set_state(i, arg_comm);
        }
      }
      // конкретного реле
      else
      {
        set_state(getRelayIndexByName(get_argument(_resp, sr_name_str)), arg_comm);
      }
    }
    else
    {
      // ответ о неизвестной команде
      String s = get_json_string_to_send(WiFi.localIP().toString(),
                                         sr_relay_module_str,
                                         module_description,
                                         F("unknown command"),
                                         arg_comm);
      send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
    }
  }
}

int8_t shRelayControl::getRelayIndexByName(String _name)
{
  int8_t result = -1;
  if (_name.length() > 0)
  {
    for (int8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayName == _name)
      {
        result = i;
        break;
      }
    }
  }

  return result;
}

void shRelayControl::switchRelay(int8_t index)
{
  switch_local_relay(index);
}

void shRelayControl::switchRelay(String _name)
{
  switch_local_relay(getRelayIndexByName(_name));
}

void shRelayControl::setRelayState(int8_t index, bool state)
{
  set_local_relay_state(index, state);
}

void shRelayControl::setRelayState(String _name, bool state)
{
  set_local_relay_state(getRelayIndexByName(_name), state);
}

void shRelayControl::setStateForAll(bool state)
{
  for (int8_t i = 0; i < relayCount; i++)
  {
    set_local_relay_state(i, state);
  }
}

String shRelayControl::getRelayState(int8_t index)
{
  return (get_relay_state(index));
}

String shRelayControl::getRelayState(String _name)
{
  return (get_relay_state(getRelayIndexByName(_name)));
}

void shRelayControl::setModuleDescription(String &_descr)
{
  module_description = _descr;
}

String shRelayControl::getModuleDescription()
{
  return (module_description);
}

void shRelayControl::setModuleName(const String &_name)
{
  module_name = _name;
}

String shRelayControl::getModuleName()
{
  return (module_name);
}

void shRelayControl::setSaveStateOfRelay(bool _state)
{
  save_state_of_relay = _state;
}

bool shRelayControl::getSaveStateOfRelay()
{
  return (save_state_of_relay);
}

void shRelayControl::setRelayName(int8_t index, String &_name)
{
  if ((index >= 0) && (index < relayCount))
  {
    relayArray[index].relayName = _name;
  }
}
String shRelayControl::getRelayName(int8_t index)
{
  String result = "";
  if ((index >= 0) && (index < relayCount))
  {
    result = relayArray[index].relayName;
  }

  return (result);
}

void shRelayControl::setRelayDescription(int8_t index, String &_descr)
{
  if ((index >= 0) && (index < relayCount))
  {
    relayArray[index].relayDescription = _descr;
  }
}

String shRelayControl::getRelayDescription(int8_t index)
{
  String result = "";
  if ((index >= 0) && (index < relayCount))
  {
    result = relayArray[index].relayDescription;
  }

  return (result);
}

void shRelayControl::setFileName(const String &_name, srModuleType _mdl)
{
  set_config_file_name(_name, _mdl);
}

String shRelayControl::getFileName(srModuleType _mdl)
{
  return (get_config_file_name(_mdl));
}

bool shRelayControl::saveConfig(srModuleType _mdl)
{
  return (save_config_file(_mdl));
}

bool shRelayControl::loadConfig(srModuleType _mdl)
{
  return (load_config_file(_mdl));
}

void shRelayControl::setTimeToModule(time_t _now)
{
  set_local_time(_now);
}

time_t shRelayControl::getTimeFromModule()
{
  return get_local_time();
}

void shRelayControl::setTimeZoneOffset(time_t _tz)
{
  set_time_zone_offset(_tz);
}

time_t shRelayControl::getTimeZoneOffset()
{
  return tz_offset / 60;
}

void shRelayControl::setNtpData(bool _enabled,
                                time_t _tz,
                                String _ntp_srv1,
                                String _ntp_srv2,
                                String _ntp_srv3)
{
  set_ntp_data(_enabled, _tz, _ntp_srv1, _ntp_srv2, _ntp_srv3);
}

bool shRelayControl::getNtpStatus()
{
  return ntp_enabled;
}

String shRelayControl::getNtpServer1()
{
  return ntp_server1;
}

String shRelayControl::getNtpServer2()
{
  return ntp_server2;
}

String shRelayControl::getNtpServer3()
{
  return ntp_server3;
}

bool shRelayControl::addTaskToList(shSchedulerData &_task)
{
  return add_task_to_list(_task);
}

uint8_t shRelayControl::findTask(shSchedulerData &_task)
{
  return find_task(_task);
}

int8_t shRelayControl::findTask(int32_t _time_for_task,
                                uint8_t _relay_index,
                                bool _action_for_relay,
                                shTaskRepeat _task_repeat)
{
  shSchedulerData task(true,
                       _time_for_task,
                       _relay_index,
                       _action_for_relay,
                       _task_repeat);
  return find_task(task);
}

bool shRelayControl::delTaskInList(uint8_t _idx)
{
  return del_task_in_list(_idx);
}

uint8_t shRelayControl::getTaskCount(bool is_active)
{
  return get_task_count(is_active);
}

void shRelayControl::setTaskStatus(uint8_t _idx, bool _state)
{
  set_task_status(_idx, _state);
}

bool shRelayControl::setTaskData(uint8_t _idx, shSchedulerData &_task)
{
  return set_task_data(_idx, _task);
}

uint8_t shRelayControl::getButtonState(uint8_t _idx)
{
  return get_button_state(_idx, mtRelay);
}

bool shRelayControl::isButtonClosed(uint8_t _idx)
{
  return is_button_closed(_idx, mtRelay);
}

bool shRelayControl::isSecondButtonPressed(uint8_t _idx,
                                           uint8_t _but_2,
                                           uint8_t btn_state)
{
  return is_second_button_pressed(_idx, _but_2, mtRelay, btn_state);
}

void shRelayControl::resetButtonState(uint8_t _idx)
{
  reset_button_state(_idx, mtRelay);
}

void shRelayControl::setButtonLongClickMode(uint8_t _idx, uint8_t longclickmode)
{
  set_long_click_mode(_idx, longclickmode, mtRelay);
}

void shRelayControl::setButtonTimeoutOfLongClick(uint8_t _idx, uint16_t new_timeout)
{
  set_timeout_of_long_click(_idx, new_timeout, mtRelay);
}

void shRelayControl::setButtonTimeoutOfDebounce(uint8_t _idx, uint16_t new_timeout)
{
  set_timeout_of_debounce(_idx, new_timeout, mtRelay);
}

// ==== shSwitchControl class ==========================

shSwitchControl::shSwitchControl() { moduleFlag = mtSwitch; }

void shSwitchControl::init(uint8_t _switch_count)
{
  _switch_count = (_switch_count > MAX_RELAY_COUNT) ? MAX_RELAY_COUNT : _switch_count;

  // защита от утечки памяти при повторном вызове init()
  if (switchArray)
  {
    delete[] switchArray;
  }

  switchArray = new (std::nothrow) shSwitchData[_switch_count];
  if (switchArray)
  {
    relayCount = _switch_count;
  }

#ifndef SWITCH_RELAY_LOG_OFF
  if (Serial != NULL)
  {
    serial = &Serial;
  }
#endif
}

bool shSwitchControl::addRelay(const String &relay_name,
                               int8_t relay_button_pin)
{
  bool result = false;
  if (relayCount > 0 && switchArray != NULL)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (switchArray[i].relayName == "")
      {
        switchArray[i] = shSwitchData(relay_name,
                                      relay_button_pin);

        result = true;
        break;
      }
    }
  }
  return (result);
}

void shSwitchControl::setUdpPort(uint16_t _udp_port)
{
  set_udp_port(_udp_port);
}

uint16_t shSwitchControl::getUdpPort()
{
  return (get_udp_port());
}

#ifndef SWITCH_RELAY_LOG_OFF
void shSwitchControl::setLogOnState(bool _on, Print *_serial)
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shSwitchControl::getLogOnState() { return (logOnState); }
#endif

void shSwitchControl::setErrorBuzzerState(bool _state, int8_t _pin)
{
  bzr.setState(_state, _pin);
}

bool shSwitchControl::getErrorBuzzerState() { return (bzr.getState()); }

void shSwitchControl::setBtnBeepData(uint16_t _freq, uint32_t _dur)
{
  bzr.setBtnBeepData(_freq, _dur);
}

void shSwitchControl::setCheckTimer(uint32_t _timer) { checkInterval = _timer; }

uint32_t shSwitchControl::getCheckTimer() { return (checkInterval); }

bool shSwitchControl::startUdpInteraction(WiFiUDP *_udp, uint16_t _udp_port)
{
  return start_udp_interaction(_udp, _udp_port);
}

bool shSwitchControl::startUdpInteraction(WiFiUDP *_udp)
{
  return start_udp_interaction(_udp, udpPort);
}

void shSwitchControl::attachWebInterface(shWebServer *_server,
                                         FS *_file_system,
                                         const String &_tasks_config_page,
                                         const String &_wifi_config_page)
{
  http_server = _server;
  file_system = _file_system;

  tasks_config_page = _tasks_config_page;
  wifi_config_page = _wifi_config_page;

  if (tasks_config_page.length() > 0 && tasks_config_page.indexOf("/") != 0)
  {
    tasks_config_page = "/" + tasks_config_page;
  }
  if (wifi_config_page.length() > 0 && wifi_config_page.indexOf("/") != 0)
  {
    wifi_config_page = "/" + wifi_config_page;
  }

  load_config_file(mtSwitch);

  if (http_server)
  { // вызов стартовой страницы модуля выключателя
    http_server->on("/", HTTP_GET, handleGetIndexPage);
    // вызов страницы настройки модуля выключателей
    http_server->on(FPSTR(RELAY_CONFIG_PAGE), HTTP_GET, handleGetConfigPage);
    // запрос текущих настроек
    http_server->on(FPSTR(GET_RELAY_CONFIG), HTTP_GET, handleGetSwitchConfig);
    // запрос списка доступных реле
    http_server->on(FPSTR(GET_RELAY_LIST), HTTP_GET, handleGetListOfRemoteRelays);
    // сохранение настроек
    http_server->on(FPSTR(SR_SET_CONFIG), HTTP_POST, handleSetConfig);
    // переключение реле
    http_server->on(FPSTR(REMOTE_RELAY_SWITCH), HTTP_POST, handleRemoteRelaySwitch);
    // перезагрузка модуля
    http_server->on(FPSTR(SR_RESTART), HTTP_GET, handleRestartModule);

    // настройка web-интерфейса планировщика задач
    setHttpHandleForScheduler();
  }
}

void shSwitchControl::tick()
{
  // опрос кнопок
  if (switchArray != NULL)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (switchArray[i].relayButton != NULL &&
          switchArray[i].relayButton->getButtonState() == BTN_DOWN)
      {
        bzr.btnBeep(); // одиночный пик на каждое нажатие любой кнопки
        switch_remote_relay(i);
      }
    }
  }

  // проверка списка планировщика задач
  static unsigned long tasks_timer = 0;
  if (millis() - tasks_timer >= 1000ul)
  {
    tasks_timer = millis();
    checkTasksList(mtSwitch);
  }

  // проверка доступности реле через заданный интервал
  static unsigned long checkTimer = 0;
  if (checkInterval > 0 && millis() - checkTimer >= checkInterval)
  {
    checkTimer = millis();
    find_remote_relays();
  }

  // проверка входящих udp-пакетов
  if (udp != NULL)
  {
    int packet_size = udp->parsePacket();
    if (packet_size > 0)
    {
      receiveUdpPacket(packet_size);
    }
  }

  // обработка запросов сервера
  if (http_server)
  {
    http_server->handleClient();
  }
  delay(1);
}

void shSwitchControl::receiveUdpPacket(int _size)
{
  String _resp;
  // пакет не прочитан - не отвечаем на него, иначе модуль будет отвечать
  // на мусор и на заведомо неподходящие пакеты
  if (!get_udp_string(_resp, _size))
  {
    return;
  }

  String arg_for = get_argument(_resp, sr_for_str);
  String arg_name = get_argument(_resp, sr_name_str);
  String arg_command = get_argument(_resp, sr_command_str);

#if defined(ARDUINO_ARCH_ESP8266)
  if (udp->destinationIP() != get_broadcast_address() && arg_command == sr_respond_str)
#else
  if (arg_name == sr_any_str && arg_command == sr_respond_str)
#endif
  // если получен запрос на отклик
  {
    SR_PRINT(module_name);
    SR_PRINT(F(": request received, response - "));
    SR_PRINTLN(sr_ok_str);
    String str = get_json_string_to_send(module_name,
                                         sr_switch_module_str,
                                         module_description,
                                         sr_ok_str,
                                         sr_respond_str);
    send_udp_packet(udp->remoteIP(), str.c_str(), str.length());
    return;
  }
  // иначе обработать ответ реле на посланную команду

  // далее работаем только с пакетами от модулей реле
  if (get_argument(_resp, sr_type_str) == sr_relay_module_str)
  {
    // если имя пустое - ничего не делаем
    if (arg_name.length() == 0)
    {
      SR_PRINT(F("Invalid relay name with IP: "));
      SR_PRINTLN(udp->remoteIP().toString());
      return;
    }

    // добавляем реле в список доступных
    addRelayToListOfRemoteRelays(arg_name);

    int8_t relay_index = getRelayIndexByName(arg_name);
    if (relay_index >= 0)
    {
      switchArray[relay_index].relayFound = true;
      if (arg_for == sr_respond_str)
      {
        switchArray[relay_index].relayDescription = get_argument(_resp, sr_descr_str);
        switchArray[relay_index].relayAddress = udp->remoteIP();
        SR_PRINT(switchArray[relay_index].relayName);
        SR_PRINT(F(" found, IP address: "));
        SR_PRINTLN(switchArray[relay_index].relayAddress);
      }
      else if (arg_for == sr_switch_str ||
               arg_for == sr_set_on_str ||
               arg_for == sr_set_off_str)
      {
        SR_PRINT(switchArray[relay_index].relayName);
        SR_PRINT(F(" response - "));
        SR_PRINTLN(get_argument(_resp, sr_response_str));
      }
    }
    else
    {
      // ответ на случай, если имя ответившего реле модулю неизвестно
      SR_PRINT(F("Module "));
      SR_PRINT(arg_name);
      SR_PRINT(F(", "));
      SR_PRINT(get_argument(_resp, sr_descr_str));
      SR_PRINT(F("; response - "));
      SR_PRINTLN(get_argument(_resp, sr_response_str));
    }
  }
}

int8_t shSwitchControl::getRelayIndexByName(String _name)
{
  int8_t result = -1;
  if (_name.length() > 0)
  {
    for (int8_t i = 0; i < relayCount; i++)
    {
      if (switchArray[i].relayName == _name)
      {
        result = i;
        break;
      }
    }
  }

  return result;
}

void shSwitchControl::switchRelay(int8_t index)
{
  switch_remote_relay(index);
}

void shSwitchControl::switchRelay(String _name)
{
  switch_remote_relay(getRelayIndexByName(_name));
}

void shSwitchControl::setRelayState(int8_t index, bool state)
{
  set_remote_relay_state(index, state);
}

void shSwitchControl::setRelayState(String _name, bool state)
{
  set_remote_relay_state(getRelayIndexByName(_name), state);
}

void shSwitchControl::setStateForAll(bool state, bool _self)
{
  if (_self)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      set_remote_relay_state(i, state);
    }
  }
  else
  {
    set_all_remote_relay_state(state);
  }
}

void shSwitchControl::findRelays()
{
  find_remote_relays();
}

void shSwitchControl::setModuleDescription(const String &_descr)
{
  module_description = _descr;
}

String shSwitchControl::getModuleDescription()
{
  return (module_description);
}

void shSwitchControl::setModuleName(const String &_name)
{
  module_name = _name;
}

String shSwitchControl::getModuleName()
{
  return (module_name);
}

void shSwitchControl::setRelayName(int8_t index, String _name)
{
  if ((index >= 0) && (index < relayCount))
  {
    switchArray[index].relayName = _name;
  }
}
String shSwitchControl::getRelayName(int8_t index)
{
  String result = "";
  if ((index >= 0) && (index < relayCount))
  {
    result = switchArray[index].relayName;
  }

  return (result);
}

void shSwitchControl::setFileName(const String &_name, srModuleType _mdl)
{
  set_config_file_name(_name, _mdl);
}

String shSwitchControl::getFileName(srModuleType _mdl)
{
  return (get_config_file_name(_mdl));
}

bool shSwitchControl::saveConfig(srModuleType _mdl)
{
  return (save_config_file(_mdl));
}

bool shSwitchControl::loadConfig(srModuleType _mdl)
{
  return (load_config_file(_mdl));
}

void shSwitchControl::setTimeToModule(time_t _now)
{
  set_local_time(_now);
}

time_t shSwitchControl::getTimeFromModule()
{
  return get_local_time();
}

void shSwitchControl::setTimeZoneOffset(time_t _tz)
{
  set_time_zone_offset(_tz);
}

time_t shSwitchControl::getTimeZoneOffset()
{
  return tz_offset / 60;
}

void shSwitchControl::setNtpData(bool _enabled,
                                 time_t _tz,
                                 String _ntp_srv1,
                                 String _ntp_srv2,
                                 String _ntp_srv3)

{
  set_ntp_data(_enabled, _tz, _ntp_srv1, _ntp_srv2, _ntp_srv3);
}

bool shSwitchControl::getNtpStatus()
{
  return ntp_enabled;
}

String shSwitchControl::getNtpServer1()
{
  return ntp_server1;
}

String shSwitchControl::getNtpServer2()
{
  return ntp_server2;
}

String shSwitchControl::getNtpServer3()
{
  return ntp_server3;
}

bool shSwitchControl::addTaskToList(shSchedulerData &_task)
{
  return add_task_to_list(_task);
}

uint8_t shSwitchControl::findTask(shSchedulerData &_task)
{
  return find_task(_task);
}

int8_t shSwitchControl::findTask(int32_t _time_for_task,
                                 uint8_t _relay_index,
                                 bool _action_for_relay,
                                 shTaskRepeat _task_repeat)
{
  shSchedulerData task(true,
                       _time_for_task,
                       _relay_index,
                       _action_for_relay,
                       _task_repeat);
  return find_task(task);
}

bool shSwitchControl::delTaskInList(uint8_t _idx)
{
  return del_task_in_list(_idx);
}

uint8_t shSwitchControl::getTaskCount(bool is_active)
{
  return get_task_count(is_active);
}

void shSwitchControl::setTaskStatus(uint8_t _idx, bool _state)
{
  set_task_status(_idx, _state);
}

bool shSwitchControl::setTaskData(uint8_t _idx, shSchedulerData &_task)
{
  return set_task_data(_idx, _task);
}

uint8_t shSwitchControl::getButtonState(uint8_t _idx)
{
  return get_button_state(_idx, mtSwitch);
}

bool shSwitchControl::isButtonClosed(uint8_t _idx)
{
  return is_button_closed(_idx, mtSwitch);
}

bool shSwitchControl::isSecondButtonPressed(uint8_t _idx,
                                            uint8_t _but_2,
                                            uint8_t btn_state)
{
  return is_second_button_pressed(_idx, _but_2, mtSwitch, btn_state);
}

void shSwitchControl::resetButtonState(uint8_t _idx)
{
  reset_button_state(_idx, mtSwitch);
}

void shSwitchControl::setButtonLongClickMode(uint8_t _idx, uint8_t longclickmode)
{
  set_long_click_mode(_idx, longclickmode, mtSwitch);
}

void shSwitchControl::setButtonTimeoutOfLongClick(uint8_t _idx, uint16_t new_timeout)
{
  set_timeout_of_long_click(_idx, new_timeout, mtSwitch);
}

void shSwitchControl::setButtonTimeoutOfDebounce(uint8_t _idx, uint16_t new_timeout)
{
  set_timeout_of_debounce(_idx, new_timeout, mtSwitch);
}

// ===================================================

static IPAddress get_broadcast_address()
{
  IPAddress result(0, 0, 0, 0);
  if (WiFi.isConnected())
  {
    result = (uint32_t)WiFi.localIP() | ~((uint32_t)WiFi.subnetMask());
  }
  else
  {
#if defined(ARDUINO_ARCH_ESP32)
    result = WiFi.softAPBroadcastIP();
#else
    // предполагаем, что маска сети у точки доступа 255.255.255.0
    result = (uint32_t)WiFi.softAPIP() | ~((uint32_t)IPAddress(255, 255, 255, 0));
#endif
  }

  return (result);
}

static bool get_value_of_argument(String &_res, const String &_arg, String &_str)
{
  StaticJsonDocument<RELAY_DATA_SIZE> doc;

  DeserializationError error = deserializeJson(doc, _res);
  bool result = !error;
  if (result)
  {
    result = doc[_arg].as<String>() != "NULL";
  }
  else
  {
    SR_PRINTLN(F("invalid response data"));
    SR_PRINTLN(error.f_str());
  }

  if (result)
  {
    _str = doc[_arg].as<String>();
  }

  return result;
}

static bool send_udp_packet(const IPAddress &address, const char *buf, size_t bufSize)
{
  bool result = (udp != NULL) ? udp->beginPacket(address, udpPort) : false;
  if (result)
  {
    result = (udp->write(reinterpret_cast<const uint8_t *>(buf), bufSize) == bufSize) &&
             (udp->endPacket() == 1);
  }

  if (!result)
  {
    SR_PRINT(F("Error sending UDP packet for IP "));
    SR_PRINT(address);
    SR_PRINT(F(", remote port: "));
    SR_PRINTLN((String)udpPort);
  }

  return (result);
}

static bool get_udp_string(String &_str, int _size)
{
#if defined(ARDUINO_ARCH_ESP8266)
  if (_size > 1024)
  {
    SR_PRINTLN(F("Error: too big size of UDP packet"));
    udp->flush();
    return false;
  }
#endif
  char *res = new (std::nothrow) char[_size + 1];
  if (res == nullptr)
  {
    SR_PRINTLN(F("Error: memory allocation failed for UDP packet"));
#if defined(ARDUINO_ARCH_ESP8266)
    udp->flush();
#else
    udp->clear();
#endif
    return false;
  }

  int read_bytes = udp->read(res, _size);
  res[read_bytes > 0 ? read_bytes : 0] = 0;
#if defined(ARDUINO_ARCH_ESP8266)
  udp->flush();
#else
  udp->clear();
#endif
  _str = String(res);

  delete[] res;
  res = NULL;
  return (read_bytes > 0);
}

static String get_argument(String &_res, const String &_arg)
{
  String result = "";
  get_value_of_argument(_res, _arg, result);
  return result;
}

static String get_json_string_to_send(const String &_name,
                                      const String &_type,
                                      const String &_descr,
                                      const String &_comm,
                                      const String &_for)
{
  StaticJsonDocument<RELAY_DATA_SIZE> doc;

  doc[sr_name_str] = _name;
  doc[sr_type_str] = _type;
  doc[sr_descr_str] = _descr;
  doc[sr_for_str] = _for;
  doc[sr_response_str] = _comm;

  String _res = "";
  serializeJson(doc, _res);

  return (_res);
}

static String get_json_string_to_send(const String &_name, const String &_comm)
{
  StaticJsonDocument<RELAY_DATA_SIZE> doc;

  doc[sr_name_str] = _name;
  doc[sr_command_str] = _comm;

  String _res = "";
  serializeJson(doc, _res);

  return (_res);
}

static void switch_local_relay(int8_t index)
{
  if ((index >= 0) && (index < relayCount))
  {
    set_local_relay_state(index, !(get_relay_state(index) == sr_on_str));
  }
}

static void set_local_relay_state(int8_t index, bool state)
{
  if ((index >= 0) && (index < relayCount))
  {
    if (!relayArray[index].relayControlLevel)
    {
      state = !state;
    }
    digitalWrite(relayArray[index].relayPin, state);
    relayArray[index].relayLastState = get_relay_state(index) == sr_on_str;
    if (save_state_of_relay)
    {
      save_config_file(mtRelay);
    }

    if (relayArray[index].relayName != "")
    {
      SR_PRINT(relayArray[index].relayName);
    }
    else
    {
      SR_PRINT(F("no name relay index "));
      SR_PRINT(index);
    }
    SR_PRINT(F(": state - "));
    SR_PRINTLN(get_relay_state(index));
  }
}

static String get_relay_state(int8_t index)
{
  String result = sr_off_str;

  if ((index >= 0) && (index < relayCount))
  {
    bool state = digitalRead(relayArray[index].relayPin);
    if (!relayArray[index].relayControlLevel)
    {
      state = !state;
    }
    result = (state) ? sr_on_str : sr_off_str;
  }
  return (result);
}

static void switch_remote_relay(int8_t index)
{
  send_command_for_relay(index, sr_switch_str);
}

static void set_remote_relay_state(int8_t index, bool state)
{
  String _str = (state) ? sr_set_on_str : sr_set_off_str;
  send_command_for_relay(index, _str);
}

static void set_all_remote_relay_state(bool state)
{
  if (WiFi.isConnected())
  {
    IPAddress broadcastAddress = get_broadcast_address();

    String st = (state) ? sr_set_on_str : sr_set_off_str;
    String s = get_json_string_to_send(sr_any_str, st);
    SR_PRINT(F("Sending a request to set the state for all remote relays: "));
    SR_PRINTLN(st);
    SR_PRINT(F("Broadcast address: "));
    SR_PRINTLN(broadcastAddress);
    send_udp_packet(broadcastAddress, s.c_str(), s.length());
  }
  else
  {
    bzr.startBuzzer(3);
    SR_PRINTLN(F("Failed to send command to remote relay, connection lost"));
  }
}

static void send_command_for_relay(int8_t index, const String &command)
{
  if (WiFi.isConnected())
  {
    if ((index >= 0) &&
        (index < relayCount) &&
        (switchArray[index].relayName != emptyString))
    {
      if (switchArray[index].relayFound)
      {
        String s = get_json_string_to_send(switchArray[index].relayName,
                                           command);
        SR_PRINTLN(F("Sending a command to remote relay"));
        SR_PRINT(F("Relay name: "));
        SR_PRINT(switchArray[index].relayName);
        SR_PRINT(F("; IP: "));
        SR_PRINT(switchArray[index].relayAddress);
        SR_PRINT(F("; command: "));
        SR_PRINTLN(command);
        switchArray[index].relayFound = false;
        send_udp_packet(switchArray[index].relayAddress, s.c_str(), s.length());
      }
      else
      {
        bzr.startBuzzer(2);
        SR_PRINT(F("Relay "));
        SR_PRINT(switchArray[index].relayName);
        SR_PRINTLN(F(" not found!"));
        find_remote_relays();
      }
    }
  }
  else
  {
    bzr.startBuzzer(3);
    SR_PRINTLN(F("Failed to send command to remote relay, connection lost"));
  }
}

static void find_remote_relays()
{
  for (uint8_t i = 0; i < relayCount; i++)
  {
    switchArray[i].relayFound = false;
  }

  IPAddress broadcastAddress = get_broadcast_address();

  String s = get_json_string_to_send(sr_any_str, sr_respond_str);
  SR_PRINTLN(F("Sending a request to verify the IP addresses of remote relays"));
  SR_PRINT(F("Broadcast address: "));
  SR_PRINTLN(broadcastAddress);
  send_udp_packet(broadcastAddress, s.c_str(), s.length());
}

static void set_udp_port(uint16_t _udp_port)
{
  udpPort = _udp_port;
}

static uint16_t get_udp_port()
{
  return udpPort;
}

static bool start_udp_interaction(WiFiUDP *_udp, uint16_t _udp_port)
{
  udp = _udp;
  udpPort = _udp_port;
  SR_PRINT(F("Starting UDP..."));
  if (udp->begin(udpPort))
  {
    SR_PRINTLN(F("OK"));
    SR_PRINT(F("udp port: "));
    SR_PRINTLN(udpPort);
    if (moduleFlag == mtSwitch)
    {
      // для модуля выключателя выполнить первичный поиск удаленных реле
      find_remote_relays();
    }
    return true;
  }
  SR_PRINTLN(F("unsuccessful"));
  return false;
}

static int8_t addRelayToListOfRemoteRelays(const String &relay_name)
{
  if (relay_name.length() > 0)
  {
    for (uint8_t i = 0; i < SR_SIZE_LIST_OF_REMOTE_RELAYS; i++)
    {
      // если строка в списке есть, ничего не делаем
      if (listOfRemoteRelays[i] == relay_name)
        return i;
    }

    // если строка не в списке, добавляем
    for (uint8_t i = 0; i < SR_SIZE_LIST_OF_REMOTE_RELAYS; i++)
    {
      if (listOfRemoteRelays[i].length() == 0)
      {
        listOfRemoteRelays[i] = relay_name;
        return i;
      }
    }
  }

  return -1;
}

static void clearListOfRemoteRelays()
{
  for (uint8_t i = 0; i < SR_SIZE_LIST_OF_REMOTE_RELAYS; i++)
  {
    listOfRemoteRelays[i] = "";
  }
}

// ===================================================

// ==== обработка списка заданий планировщика ========
static void checkTasksList(srModuleType _mdl)
{
  if (tasksArray != NULL && tasksCount > 0)
  {
    tm m_time;
    // время должно быть синхронизировано (getLocalTime возвращает false
    // пока год не больше 2016, т.е. часы не установлены)
    if (getLocalTime(&m_time, 0))
    {
      bool is_changed = false;
      // текущая минута локальных суток (0..1439); вычисляется из unix time
      // со смещением часового пояса; корректный modulo для любого tz_offset
      int64_t local = (int64_t)get_local_time() + tz_offset;
      uint32_t cur_minute = (uint32_t)(((local % 86400 + 86400) % 86400) / 60);
      // день недели берется из номера дня локальных суток, а не из tm_wday,
      // т.к. часы модуля идут в UTC и tm_wday может отличаться от локального;
      // epoch (1 января 1970) - четверг, в нашем кодировании это 3
      uint8_t cur_wday = (uint8_t)(((local / 86400) + 3) % 7);
      // срабатывание происходит при смене минуты, а не по совпадению секунд;
      // иначе пропуск тика tick() из-за блокировок в loop() приводил бы к
      // потере задач, пришедшихся на пропущенную секунду
      static uint32_t last_minute = 0xFFFFFFFF;
      if (last_minute == 0xFFFFFFFF)
      {
        // первый запуск после установки времени - просто запоминаем текущую
        // минуту, чтобы не выполнить все задачи сразу
        last_minute = cur_minute;
        return;
      }
      if (cur_minute == last_minute)
      {
        return;
      }
      last_minute = cur_minute;
      for (uint8_t i = 0; i < tasksCount; i++)
      {
        if ((tasksArray[i].task_status) &&
            (tasksArray[i].time_for_task == (int32_t)cur_minute) &&
            (tasksArray[i].relay_index < relayCount))
        {
          if (tasksArray[i].task_repeat == TR_DAILY ||
              tasksArray[i].task_repeat == TR_ONCE ||
              ((uint8_t)tasksArray[i].task_repeat & (1 << cur_wday)))
          {
            switch (_mdl)
            {
            case mtRelay:
              set_local_relay_state(tasksArray[i].relay_index, tasksArray[i].action_for_relay);
              break;
            case mtSwitch:
              set_remote_relay_state(tasksArray[i].relay_index, tasksArray[i].action_for_relay);
              break;
            default:
              break;
            }

            SR_PRINTLN(F("The task has been executed successfully"));
            SR_PRINT(F("time: "));
            SR_PRINT(tasksArray[i].time_for_task / 60);
            SR_PRINT(F(":"));
            SR_PRINTLN(tasksArray[i].time_for_task % 60);
            SR_PRINT(F("relay: "));
            SR_PRINTLN((_mdl == mtRelay) ? relayArray[tasksArray[i].relay_index].relayName
                                         : switchArray[tasksArray[i].relay_index].relayName);
            SR_PRINT(F("action: "));
            SR_PRINTLN(tasksArray[i].action_for_relay ? "on" : "off");

            if (tasksArray[i].task_repeat == TR_ONCE)
            {
              tasksArray[i].task_status = false;
              is_changed = true;
            }
          }
        }
      }
      if (is_changed)
      {
        save_config_file(mtTasks);
      }
    }
  }
}

static int8_t find_task(shSchedulerData &_task)
{
  int8_t result = -1;
  if (tasksArray != NULL && tasksCount > 0)
  {
    for (uint8_t i = 0; i < tasksCount; i++)
    {
      if (tasksArray[i] == _task)
      {
        result = i;
        break;
      }
    }
  }
  return result;
}

static bool add_task_to_list(shSchedulerData &_task)
{
  // если лимит количества задач достигнут, больше ничего не делать
  if (tasksCount >= MAX_TASKS_COUNT)
  {
    SR_PRINTLN(F("Failed to add a task - the task limit has been reached"));
    return false;
  }

  // контроль повторяемости задачи - если выбраны все дни недели, то
  // установить ежедневный повтор
  if (_task.task_repeat == 0x7f || _task.task_repeat == 0xff)
  {
    _task.task_repeat = TR_DAILY;
  }

  // проверить наличие такой задачи
  int8_t idx = find_task(_task);
  if (idx >= 0)
  {
    // если такая задача найдена, устанавливаем статус задачи в соответствие с
    // _task.task_status и считаем, что задача добавлена успешно
    tasksArray[idx].task_status = _task.task_status;
    SR_PRINTLN(F("The task already exists, and its status has been updated accordingly"));
    return save_config_file(mtTasks);
  }

  // если задача не была найдена, то добавляем ее в список
  static DynamicJsonDocument doc(CONFIG_SIZE);
  doc.clear();
  get_config_json_doc(doc, mtTasks);
  JsonObject task = doc[sr_t_list_str].as<JsonArray>().createNestedObject();
  task[sr_state_str] = (uint8_t)_task.task_status;
  task[sr_time_str] = _task.time_for_task;
  task[sr_rel_str] = _task.relay_index;
  task[sr_act_str] = (uint8_t)_task.action_for_relay;
  task[sr_rep_str] = (uint8_t)_task.task_repeat;

  load_setting(mtTasks, doc);
  SR_PRINTLN(F("The task has been added successfully"));

  return save_config_file(mtTasks, doc);
}

static bool del_task_in_list(uint8_t _idx)
{
  if (_idx >= tasksCount)
  {
    SR_PRINTLN(F("Invalid index for task deletion"));
    return false;
  }

  uint8_t x = tasksCount;

  tasksArray[_idx].time_for_task = -1;
  static DynamicJsonDocument doc(CONFIG_SIZE);
  doc.clear();
  get_config_json_doc(doc, mtTasks);
  load_setting(mtTasks, doc);
  save_config_file(mtTasks, doc);
  if (x - tasksCount == 1)
  {
    SR_PRINTLN(F("The task has been successfully deleted"));
    return true;
  }
  else
  {
    SR_PRINTLN(F("Unsuccessful task deletion"));
    return false;
  }
}

static void set_task_status(uint8_t _idx, bool _state)
{
  if (_idx < tasksCount)
  {
    tasksArray[_idx].task_status = _state;
    save_config_file(mtTasks);
  }
}

static bool set_task_data(uint8_t _idx, shSchedulerData &_task)
{
  if (_idx < tasksCount)
  {
    if (find_task(_task) >= 0)
    {
      SR_PRINTLN(F("A task with these parameters already exists; the change has not been implemented"));
      return false;
    }

    tasksArray[_idx] = _task;
    return save_config_file(mtTasks);
  }
  return false;
}

static uint8_t get_task_count(bool is_active)
{
  if (tasksArray == NULL)
  {
    return 0;
  }

  uint8_t result = 0;
  for (uint8_t i = 0; i < tasksCount; i++)
  {
    // задачи с отрицательным временем считаются удаленными
    if (tasksArray[i].time_for_task >= 0)
    {
      // если задано искать только активные задачи, неактивные пропускаем
      if (is_active && !tasksArray[i].task_status)
      {
        continue;
      }
      result++;
    }
  }
  return result;
}

static void set_local_time(time_t _now)
{
  struct timeval tv = {.tv_sec = _now, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

static time_t get_local_time()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  return tv.tv_sec;
}

static void set_time_zone_offset(time_t _tz)
{
  // контроль данных: не меньше -12 часов (-720) и не больше 14 часов (840)
  _tz = (_tz > 840) ? 840 * 60 : ((_tz < -720) ? -720 * 60 : _tz * 60);
  bool is_change = (_tz != tz_offset);
  tz_offset = _tz;
  if (is_change)
  {
    save_config_file(mtNtp);
  }
}

static void set_ntp_data(bool _ntp_enabled,
                         time_t _tz,
                         String _ntp_server1,
                         String _ntp_server2,
                         String _ntp_server3)
{
  ntp_enabled = _ntp_enabled;
  // контроль данных: не меньше -12 часов (-720) и не больше 14 часов (840)
  tz_offset = (_tz > 840) ? 840 * 60 : ((_tz < -720) ? -720 * 60 : _tz * 60);
  if (_ntp_server1.length() > 0)
  {
    ntp_server1 = _ntp_server1;
  }
  if (_ntp_server2.length() > 0)
  {
    ntp_server2 = _ntp_server2;
  }
  if (_ntp_server3.length() > 0)
  {
    ntp_server3 = _ntp_server3;
  }

  if (ntp_enabled)
  {
    configTzTime("UTC", ntp_server1.c_str(), ntp_server2.c_str(), ntp_server3.c_str());
  }
  else
  {
    sntp_stop();
  }

  save_config_file(mtNtp);
}

// ===================================================
static bool is_valid_button(uint8_t _idx, srModuleType _mdl)
{
  bool result = _idx < relayCount;
  if (result)
  {
    switch (_mdl)
    {
    case mtRelay:
      result = relayArray != NULL && relayArray[_idx].relayButton != NULL;
      break;
    case mtSwitch:
      result = switchArray != NULL && switchArray[_idx].relayButton != NULL;
      break;
    default:
      result = false;
      break;
    }
  }
  return result;
}

static uint8_t get_button_state(uint8_t _idx, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return BTN_RELEASED;
  }
  switch (_mdl)
  {
  case mtRelay:
    return (relayArray[_idx].relayButton->getLastState());
  case mtSwitch:
    return (switchArray[_idx].relayButton->getLastState());
  default:
    return BTN_RELEASED;
  }
}

static bool is_button_closed(uint8_t _idx, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return false;
  }
  switch (_mdl)
  {
  case mtRelay:
    return (relayArray[_idx].relayButton->isButtonClosed());
  case mtSwitch:
    return (switchArray[_idx].relayButton->isButtonClosed());
  default:
    return false;
  }
}

static bool is_second_button_pressed(uint8_t _idx,
                                     uint8_t _but_2,
                                     srModuleType _mdl,
                                     uint8_t btn_state)
{
  if (!is_valid_button(_idx, _mdl) || !is_valid_button(_but_2, _mdl))
  {
    return false;
  }
  switch (_mdl)
  {
  case mtRelay:
    return (relayArray[_idx].relayButton->isSecondButtonPressed(relayArray[_but_2].relayButton,
                                                                btn_state));
  case mtSwitch:
    return (switchArray[_idx].relayButton->isSecondButtonPressed(switchArray[_but_2].relayButton,
                                                                 btn_state));
  default:
    return false;
  }
}

static void reset_button_state(uint8_t _idx, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return;
  }
  switch (_mdl)
  {
  case mtRelay:
    relayArray[_idx].relayButton->resetButtonState();
    break;
  case mtSwitch:
    switchArray[_idx].relayButton->resetButtonState();
    break;
  default:
    break;
  }
}

static void set_long_click_mode(uint8_t _idx, uint8_t longclickmode, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return;
  }
  switch (_mdl)
  {
  case mtRelay:
    relayArray[_idx].relayButton->setLongClickMode(longclickmode);
    break;
  case mtSwitch:
    switchArray[_idx].relayButton->setLongClickMode(longclickmode);
    break;
  default:
    break;
  }
}

static void set_timeout_of_long_click(uint8_t _idx, uint16_t new_timeout, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return;
  }
  switch (_mdl)
  {
  case mtRelay:
    relayArray[_idx].relayButton->setTimeoutOfLongClick(new_timeout);
    break;
  case mtSwitch:
    switchArray[_idx].relayButton->setTimeoutOfLongClick(new_timeout);
    break;
  }
}

static void set_timeout_of_debounce(uint8_t _idx, uint16_t new_timeout, srModuleType _mdl)
{
  if (!is_valid_button(_idx, _mdl))
  {
    return;
  }
  switch (_mdl)
  {
  case mtRelay:
    relayArray[_idx].relayButton->setTimeoutOfDebounce(new_timeout);
    break;
  case mtSwitch:
    switchArray[_idx].relayButton->setTimeoutOfDebounce(new_timeout);
    break;
  }
}

// ==== реакции сервера ==============================
static void handleGetConfigPage()
{
  http_server->send_P(200, TEXT_HTML, CONFIG_PAGE);
}

static void handleGetIndexPage()
{
  http_server->send_P(200, TEXT_HTML, INDEX_PAGE);
}

static void handleGetConfig(String _msg)
{
  http_server->send(200, FPSTR(TEXT_JSON), _msg);
}

static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const int8_t _last)
{
  if (_last >= 0)
  {
    rel[sr_last_state_str] = _last;
  }
  rel[sr_name_str] = _name;
  rel[sr_descr_str] = _descr;
}

static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const IPAddress _ip)
{
  rel[sr_name_str] = _name;
  rel[sr_descr_str] = _descr;
  rel[sr_ip_addr_str] = _ip.toString();
}

static void get_config_json_doc(DynamicJsonDocument &doc, srModuleType _mdl)
{
  if ((uint8_t)_mdl <= 1)
  {
    // только для mtRelay и mtSwitch
    doc[sr_module_description_str] = module_description;
    doc[sr_module_name_str] = module_name;
    doc[sr_udp_port_str] = udpPort;
    doc[sr_udp_en_str] = (uint8_t)(udp != NULL);
    doc[sr_wificonf_str] = wifi_config_page;
    doc[sr_tasksconf_str] = tasks_config_page;
  }

  switch (_mdl)
  {
  case mtRelay:
  {
    JsonArray relays = doc.createNestedArray(sr_relays_str);
    doc[sr_for_str] = sr_relay_str;
    doc[sr_save_state_str] = (byte)save_state_of_relay;

    for (int8_t i = 0; i < relayCount; i++)
    {
      JsonObject rel = relays.createNestedObject();
      get_relay_data_json(rel,
                          relayArray[i].relayName,
                          relayArray[i].relayDescription,
                          (byte)relayArray[i].relayLastState);
    }
  }
  break;

  case mtSwitch:
  {
    JsonArray relays = doc.createNestedArray(sr_relays_str);
    doc[sr_for_str] = sr_switch_str;
    doc[sr_check_interval_str] = checkInterval / 60000ul;

    for (int8_t i = 0; i < relayCount; i++)
    {
      JsonObject rel = relays.createNestedObject();
      get_relay_data_json(rel,
                          switchArray[i].relayName,
                          switchArray[i].relayDescription,
                          switchArray[i].relayAddress);
    }
  }
  break;

  case mtTasks:
  {
    JsonArray tasks = doc.createNestedArray(sr_t_list_str);
    if (tasksArray != NULL)
    {
      for (uint8_t i = 0; i < tasksCount; i++)
      {
        if (tasksArray[i].time_for_task >= 0)
        {
          JsonObject task = tasks.createNestedObject();
          task[sr_state_str] = (uint8_t)tasksArray[i].task_status;
          task[sr_time_str] = tasksArray[i].time_for_task;
          task[sr_rel_str] = tasksArray[i].relay_index;
          task[sr_act_str] = (uint8_t)tasksArray[i].action_for_relay;
          task[sr_rep_str] = (uint8_t)tasksArray[i].task_repeat;
        }
      }
    }
  }
  break;

  case mtNtp:
  {
    doc[sr_ntp_enabled_str] = (uint8_t)ntp_enabled;
    doc[sr_ntp_server1_str] = ntp_server1;
    doc[sr_ntp_server2_str] = ntp_server2;
    doc[sr_ntp_server3_str] = ntp_server3;
    doc[sr_tz_str] = tz_offset;
  }
  break;

  default:
    break;
  }
}

static String get_config_json_string(srModuleType _mdl)
{
  DynamicJsonDocument doc(CONFIG_SIZE);

  get_config_json_doc(doc, _mdl);

  String _res = "";
  serializeJson(doc, _res);
  return (_res);
}

static void handleGetRelayConfig()
{
  handleGetConfig(get_config_json_string(mtRelay));
}

static void handleGetSwitchConfig()
{
  handleGetConfig(get_config_json_string(mtSwitch));
}

static void handleGetListOfRemoteRelays()
{
  DynamicJsonDocument doc(RELAY_DATA_SIZE);
  JsonArray relays = doc.createNestedArray(sr_relays_str);

  for (uint8_t i = 0; i < SR_SIZE_LIST_OF_REMOTE_RELAYS; i++)
  {
    if (listOfRemoteRelays[i].length() > 0)
    {
      JsonObject rel = relays.createNestedObject();
      rel[sr_relay_str] = listOfRemoteRelays[i];
    }
  }

  String res;
  serializeJson(doc, res);
  handleGetConfig(res);
}

static void setHttpHandleForScheduler()
{
  if (tasks_config_page.length() > 0)
  {
    // вызов страницы настройки планировщика
    http_server->on(tasks_config_page, HTTP_GET, &handleGetConfigSchedulerPage);
    // заполнение списка задач
    http_server->on(FPSTR(SR_TASKS), HTTP_GET, handleReadTasksSetting);
    // сохранение списка задач
    http_server->on(FPSTR(SR_TASKS), HTTP_POST, handleWriteTasksSetting);
    // получение времени модуля
    http_server->on(FPSTR(TIME_DATA), HTTP_GET, handleReadTimeData);
    // сохранение времени модуля
    http_server->on(FPSTR(TIME_DATA), HTTP_POST, handleWriteTimeData);
    // получение NTP настроек
    http_server->on(FPSTR(NTP_DATA), HTTP_GET, handleReadNtpData);
    // сохранение NTP настроек
    http_server->on(FPSTR(NTP_DATA), HTTP_POST, handleWriteNtpData);

    load_config_file(mtTasks);
    load_config_file(mtNtp);
  }
}

static void handleGetConfigSchedulerPage()
{
  http_server->send_P(200, TEXT_HTML, TASK_LIST_PAGE);
}

static void handleReadTasksSetting()
{
  // clear() обязателен: иначе createNestedArray добавит второй t_list
  static DynamicJsonDocument doc(CONFIG_SIZE);
  doc.clear();

  get_config_json_doc(doc, mtTasks);
  doc[sr_tmax_str] = MAX_TASKS_COUNT;

  JsonArray relays = doc.createNestedArray(sr_r_list_str);
  if ((moduleFlag == mtRelay && relayArray != NULL) ||
      (moduleFlag == mtSwitch && switchArray != NULL))
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      relays.add((moduleFlag == mtRelay) ? relayArray[i].relayName
                                         : switchArray[i].relayName);
    }
  }

  String json = "";
  serializeJson(doc, json);
  http_server->send(200, FPSTR(APP_JSON), json);
}

static void handleWriteTasksSetting()
{
  if (!http_server->hasArg(F("plain")))
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("NO"));
    SR_PRINTLN(F("The task list could not be saved - the data was not received"));
    return;
  }

  String json = http_server->arg(F("plain"));

  // проверяем, что пришедший текст действительно валидный JSON
  // с массивом t_list — иначе не сохраняем, чтобы не затереть рабочий список
  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError err = deserializeJson(doc, json);
  if (err || !doc["t_list"].is<JsonArray>())
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("BAD JSON"));
    SR_PRINTLN(F("The task list could not be saved - the JSON data format is incorrect"));
    return;
  }

  load_setting(mtTasks, doc);

  save_config_file(mtTasks, doc);

  http_server->send(200, FPSTR(TEXT_HTML), F("OK"));
}

static void handleReadTimeData()
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  StaticJsonDocument<256> doc;

  doc[sr_unix_str] = (uint32_t)tv.tv_sec;
  doc[sr_ms_str] = (uint32_t)tv.tv_usec / 1000;
  doc[sr_tz_str] = tz_offset;

  String json = "";
  serializeJson(doc, json);

  http_server->send(200, FPSTR(APP_JSON), json);
}

static void handleWriteTimeData()
{
  if (http_server->hasArg(F("plain")) == false)
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("BAD JSON"));
    SR_PRINTLN(F("The time could not be synchronized - the data was not received"));
    return;
  }

  String json = http_server->arg(F("plain"));

  StaticJsonDocument<256> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  if (!error)
  {
    struct timeval tv = {.tv_sec = (time_t)doc[sr_unix_str].as<uint32_t>(),
                         .tv_usec = (suseconds_t)doc[sr_ms_str].as<uint32_t>() * 1000};
    settimeofday(&tv, nullptr);
    int32_t tz = doc[sr_tz_str].as<time_t>();
    bool save_ntp = (tz != tz_offset);
    tz_offset = tz;
    // если изменилось смещение по часовому поясу, пересохранить настройки
    // NTP - смещение сохраняется там
    if (save_ntp)
    {
      save_config_file(mtNtp);
    }

    tm m_time;
    if (!getLocalTime(&m_time, 0))
    {
      http_server->send(400, FPSTR(TEXT_PLAIN), F("BAD JSON"));
      return;
    }
    http_server->send(200, FPSTR(TEXT_PLAIN), F("OK"));
    return;
  }
  else
  {
    SR_PRINTLN(F("The time could not be synchronized - the JSON data format is incorrect"));
  }

  http_server->send(400, FPSTR(TEXT_PLAIN), F("BAD JSON"));
}

static void handleReadNtpData()
{
  DynamicJsonDocument doc(512);

  get_config_json_doc(doc, mtNtp);

  String json = "";
  serializeJson(doc, json);
  http_server->send(200, FPSTR(APP_JSON), json);
}

static void handleWriteNtpData()
{
  if (!http_server->hasArg(F("plain")))
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("NO"));
    SR_PRINTLN(F("Failed to save NTP data - no data was received"));
    return;
  }

  String json = http_server->arg(F("plain"));
  DynamicJsonDocument doc(512);

  DeserializationError err = deserializeJson(doc, json);
  if (err)
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("BAD JSON"));
    SR_PRINTLN(F("Failed to save NTP data - the JSON data format is incorrect"));
    return;
  }
  // читаем новые значения
  load_setting(mtNtp, doc);

  // сохраняем настройки
  save_config_file(mtNtp, doc);

  http_server->send(200, FPSTR(TEXT_HTML), F("OK"));
}

static void getStringValue(String &_var, String _val)
{
  _var = (_val != "NULL") ? _val : "";
}

static void handleSetConfig()
{
  if (http_server->hasArg(F("plain")) == false)
  {
    http_server->send(400, FPSTR(TEXT_PLAIN), F("Body not received"));
    SR_PRINTLN(F("Failed to save configuration data, no data"));
    return;
  }

  String json = http_server->arg(F("plain"));

  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    SR_PRINTLN(F("Failed to save configuration data, invalid json data"));
    SR_PRINTLN(error.f_str());
  }
  else
  {
    if (doc[sr_for_str].as<String>() == sr_relay_str)
    {
      load_setting(mtRelay, doc);
      save_config_file(mtRelay, doc);
    }
    else if (doc[sr_for_str].as<String>() == sr_switch_str)
    {
      if (doc[sr_udp_port_str].as<uint16_t>() != udpPort)
      {
        // если изменился номер порта, очистить список доступных удаленных реле
        // потому что с новым портом реле из списка станут недоступными
        clearListOfRemoteRelays();
      }

      load_setting(mtSwitch, doc);
      save_config_file(mtSwitch, doc);
      find_remote_relays();
    }
    http_server->send(200, FPSTR(TEXT_HTML), F("<META http-equiv='refresh' content='1;URL=/'><p align='center'>Save settings...</p>"));
  }
}

static void handleRelaySwitch()
{
  if (http_server->hasArg(F("plain")))
  {
    String json = http_server->arg(F("plain"));

    int8_t index = get_argument(json, sr_relay_str).toInt();

    switch_local_relay(index);
    http_server->send(200, FPSTR(TEXT_HTML), get_relay_state(index));
  }
  else
  {
    http_server->send(200, FPSTR(TEXT_HTML), sr_off_str);
  }
}

static void handleRemoteRelaySwitch()
{
  if (http_server->hasArg(F("plain")))
  {
    String json = http_server->arg(F("plain"));

    int8_t index = get_argument(json, sr_relay_str).toInt();

    switch_remote_relay(index);
    http_server->send(200, FPSTR(TEXT_HTML), sr_ok_str);
  }
  else
  {
    http_server->send(200, FPSTR(TEXT_HTML), sr_no_str);
  }
}

static void handleGetRelayState()
{
  String _res = "";

  DynamicJsonDocument doc(CONFIG_SIZE);
  JsonArray relays = doc.createNestedArray(sr_relays_str);
  for (int8_t i = 0; i < relayCount; i++)
  {
    JsonObject rel = relays.createNestedObject();
    bool state = get_relay_state(i) == sr_on_str;
    get_relay_data_json(rel,
                        relayArray[i].relayName,
                        relayArray[i].relayDescription,
                        (byte)state);
  }
  serializeJson(doc, _res);

  http_server->send(200, FPSTR(TEXT_JSON), _res);
}

static void handleRestartModule()
{
  http_server->send(200, FPSTR(TEXT_HTML), F("<META http-equiv='refresh' content='5;URL=/'><p align='center'>Restart module...</p>"));
  delay(2000);
  ESP.restart();
}

static bool load_setting(srModuleType _mdl, DynamicJsonDocument &doc)
{
  if ((uint8_t)_mdl <= 1)
  {
    // только для mtRelay и mtSwitch
    getStringValue(module_description, doc[sr_module_description_str].as<String>());
    getStringValue(module_name, doc[sr_module_name_str].as<String>());
    udpPort = (doc[sr_udp_port_str].isNull()) ? SR_DEFAULT_UDP_PORT
                                              : doc[sr_udp_port_str].as<uint16_t>();

    if (udpPort == 0)
    {
      udpPort = SR_DEFAULT_UDP_PORT;
    }
  }
  int8_t x;
  switch (_mdl)
  {
  case mtRelay:
  {
    x = doc[sr_relays_str].size();
    save_state_of_relay = doc[sr_save_state_str].as<bool>();
    for (int8_t i = 0; i < x && i < relayCount; i++)
    {
      getStringValue(relayArray[i].relayName,
                     doc[sr_relays_str][i][sr_name_str].as<String>());
      getStringValue(relayArray[i].relayDescription,
                     doc[sr_relays_str][i][sr_descr_str].as<String>());
      relayArray[i].relayLastState = doc[sr_relays_str][i][sr_last_state_str].as<bool>();
    }
  }
  break;

  case mtSwitch:
  {
    x = doc[sr_relays_str].size();
    for (int8_t i = 0; i < x && i < relayCount; i++)
    {
      getStringValue(switchArray[i].relayName,
                     doc[sr_relays_str][i][sr_name_str].as<String>());
      if (doc[sr_relays_str][i][sr_name_str].as<String>() == "")
      {
        doc[sr_relays_str][i][sr_descr_str] = "";
      }

      getStringValue(switchArray[i].relayDescription,
                     doc[sr_relays_str][i][sr_descr_str].as<String>());
    }
    checkInterval = (doc[sr_check_interval_str].isNull()) ? SR_DEFAULT_CHECK_INTERVAL
                                                          : doc[sr_check_interval_str].as<uint32_t>() * 60000ul;
  }
  break;

  case mtTasks:
  { // проверяем лимит задач, если нужно, ограничиваем размер
    uint8_t count;
    count = (doc[sr_t_list_str].size() > MAX_TASKS_COUNT)
                ? MAX_TASKS_COUNT
                : doc[sr_t_list_str].size();

    delete[] tasksArray;
    tasksArray = NULL;
    tasksArray = new (std::nothrow) shSchedulerData[count];

    tasksCount = 0;
    if (tasksArray != NULL)
    {
      for (JsonObject t_list_item : doc[sr_t_list_str].as<JsonArray>())
      {
        tasksArray[tasksCount].task_status = t_list_item[sr_state_str].as<bool>();
        tasksArray[tasksCount].time_for_task = t_list_item[sr_time_str].as<uint32_t>();
        tasksArray[tasksCount].relay_index = t_list_item[sr_rel_str].as<uint8_t>();
        if (tasksArray[tasksCount].relay_index >= relayCount)
        {
          tasksArray[tasksCount].relay_index = relayCount - 1;
        }

        tasksArray[tasksCount].action_for_relay = t_list_item[sr_act_str].as<bool>();
        tasksArray[tasksCount].task_repeat = (shTaskRepeat)t_list_item[sr_rep_str].as<uint8_t>();
        if (++tasksCount >= count)
        {
          // загружаем не более разрешенного количества
          return (true);
        }
      }
    }
  }
  break;

  case mtNtp:
  {
    ntp_enabled = doc[sr_ntp_enabled_str].as<bool>();
    ntp_server1 = doc[sr_ntp_server1_str].as<String>();
    ntp_server2 = doc[sr_ntp_server2_str].as<String>();
    ntp_server3 = doc[sr_ntp_server3_str].as<String>();
    tz_offset = doc[sr_tz_str].as<time_t>();

    // применяем новые NTP настройки
    if (ntp_enabled)
    {
      configTzTime("UTC", ntp_server1.c_str(), ntp_server2.c_str(), ntp_server3.c_str());
    }
    else
    {
      sntp_stop();
    }
  }
  break;
  default:
    return (false);
    break;
  }
  return (true);
}

static void set_config_file_name(const String &_name, srModuleType _mdl)
{
  switch (_mdl)
  {
  case mtRelay:
    relayFileConfigName = _name;
    break;
  case mtSwitch:
    switchFileConfigName = _name;
    break;
  case mtTasks:
    tasksFileConfigName = _name;
    break;
  case mtNtp:
    ntpFileConfigName = _name;
    break;
  default:
    break;
  }
}

static String get_config_file_name(srModuleType _mdl)
{
  switch (_mdl)
  {
  case mtRelay:
    return (relayFileConfigName);
  case mtSwitch:
    return (switchFileConfigName);
  case mtTasks:
    return (tasksFileConfigName);
  case mtNtp:
    return (ntpFileConfigName);
  default:
    return ("");
  }
}

static bool save_config_file(srModuleType _mdl)
{
  DynamicJsonDocument doc(CONFIG_SIZE);
  get_config_json_doc(doc, _mdl);
  return (save_config_file(_mdl, doc));
}

static bool save_config_file(srModuleType _mdl, DynamicJsonDocument &doc)
{
  if (!file_system)
  {
    return (false);
  }

  String fileName = get_config_file_name(_mdl);

  SR_PRINT(F("Save settings to file "));
  SR_PRINTLN(fileName);

  File configFile;

  // удалить существующий файл, иначе конфигурация будет добавлена ​​к файлу
  file_system->remove(fileName);

  // Открыть файл для записи
  configFile = file_system->open(fileName, "w");

  if (!configFile)
  {
    SR_PRINT(F("Failed to create configuration file: "));
    SR_PRINTLN(fileName);
    return (false);
  }

  // сериализовать JSON-файл
  bool result = serializeJson(doc, configFile);
  if (result)
  {
    SR_PRINTLN(F("OK"));
  }
  else
  {
    SR_PRINT(F("Failed to write file "));
    SR_PRINTLN(fileName);
  }

  configFile.close();
  return (result);
}

static bool load_config_file(srModuleType _mdl)
{
  File configFile;
  String fileName = get_config_file_name(_mdl);

  SR_PRINT(F("Load settings from file "));
  SR_PRINTLN(fileName);

  // находим и открываем для чтения файл конфигурации
  bool result = file_system &&
                file_system->exists(fileName) &&
                (configFile = file_system->open(fileName, "r"));

  // если файл конфигурации не найден, сохранить настройки по умолчанию
  if (!result)
  {
    SR_PRINTLN(F("Config file not found, default config used."));
    save_config_file(_mdl);
    return (result);
  }
  // Проверяем размер файла, будем использовать файл размером до 2048/4096 байт
  size_t size = configFile.size();
  if (size > CONFIG_SIZE)
  {
    SR_PRINTLN(F("Configuration file size is too large."));
    configFile.close();
    return (false);
  }

  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  if (error)
  {
    SR_PRINT("Data serialization error: ");
    SR_PRINTLN(error.f_str());
    SR_PRINTLN(F("Failed to read config file, default config is used"));
    result = false;
  }
  else
  // Теперь можно получить значения из doc
  {
    result = load_setting(_mdl, doc);
  }
  if (result)
  {
    SR_PRINTLN(F("OK"));
  }

  return (result);
}

// ==== shBuzzer class ===============================

static void buzzerTick()
{
  bzr.beep();
  if (bzr.decBipCount() == 0)
  {
    bzr.stopBuzzer();
  }
}

shBuzzer::shBuzzer() {}

void shBuzzer::setState(bool _state, int8_t _pin)
{
  buzzer_pin = _pin;
  buzzer_state = (buzzer_pin >= 0) ? _state : false;
}

bool shBuzzer::getState()
{
  return (buzzer_state);
}

void shBuzzer::startBuzzer(uint8_t _num, uint16_t _freq, uint32_t _dur)
{
  error_freq = _freq;
  error_dur = _dur;
  buzzer.detach();
  if (buzzer_state && buzzer_pin >= 0)
  {
    beep_count = _num;
    buzzer.attach_ms(error_dur * 2, buzzerTick);
    buzzerTick();
  }
}

void shBuzzer::stopBuzzer()
{
  buzzer.detach();
}

void shBuzzer::beep()
{
  if (buzzer_state && buzzer_pin >= 0)
  {
    tone(buzzer_pin, error_freq, error_dur);
  }
  else
  {
    stopBuzzer();
  }
}

void shBuzzer::btnBeep()
{
  startBuzzer(1, btn_beep_freq, btn_beep_dur);
}

void shBuzzer::setBtnBeepData(uint16_t _freq, uint32_t _dur)
{
  btn_beep_freq = _freq;
  btn_beep_dur = _dur;
}

uint8_t shBuzzer::decBipCount()
{
  return (--beep_count);
}