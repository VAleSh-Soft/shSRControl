#include "shSRControl.h"
#include <ArduinoJson.h>
#include "extras/c_page.h"
#include "extras/i_page.h"

#define SR_PRINT(x)         \
  if (logOnState && serial) \
  serial->print(x)
#define SR_PRINTLN(x)       \
  if (logOnState && serial) \
  serial->println(x)

// ==== имена параметров в запросах/ответах ==========
static const String sr_name_str = "name";
static const String sr_command_str = "command";
static const String sr_response_str = "resp";
static const String sr_for_str = "for";
static const String sr_descr_str = "descr";
static const String sr_relays_str = "relays";
static const String sr_module_str = "module";
static const String sr_last_state_str = "last";
static const String sr_ip_addr_str = "addr";
static const String sr_wificonf_str = "wificonf";
static const String sr_relayconf_str = "relconf";
static const String sr_save_state_str = "save_state";

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
static const char RELAY_GET_CONFIG[] PROGMEM = "/relay_getconfig";
static const char SWITCH_GET_CONFIG[] PROGMEM = "/switch_getconfig";
static const char SR_SET_CONFIG[] PROGMEM = "/sr_setconfig";
static const char RELAY_GET_STATE[] PROGMEM = "/relay_getstate";
static const char RELAY_SWITCH[] PROGMEM = "/relay_switch";
static const char REMOTE_RELAY_SWITCH[] PROGMEM = "/remote_switch";

// константы для работы с JSON
#if defined(ARDUINO_ARCH_ESP8266)
static const uint16_t CONFIG_SIZE = 2048;
#else
static const uint16_t CONFIG_SIZE = 4096;
#endif
static const uint16_t RELAY_DATA_SIZE = 256;

enum ModuleType : uint8_t
{
  mtRelay,
  mtSwitch
};

// ==== общие данные =================================

static shRelayData *relayArray = NULL;
static int8_t relayCount = 0;
static String relayFileConfigName = "/relay.json";

static shSwitchData *switchArray = NULL;
static int8_t switchCount = 0;
static String switchFileConfigName = "/switch.json";

static String module_description = "";
static bool save_state_of_relay = false;

static Print *serial = NULL;
static bool logOnState = true;

static shWebServer *http_server = NULL;
static FS *file_system = NULL;
static WiFiUDP *udp = NULL;
static uint16_t localPort = 0;

static shBuzzer bzr;

static String wifi_config_page = "";
static String relay_config_page = "";

// ===================================================

static IPAddress get_broadcast_address();
static bool get_value_of_argument(String &_res, const String &_arg, String &_str);
static bool send_udp_packet(const IPAddress &address, const char *buf, size_t bufSize);
static String get_argument(String &_res, const String &_arg);
static String get_json_string_to_send(const String &_name, const String &_comm);
static String get_json_string_to_send(const String &_name,
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

// ===================================================
static void handleGetConfigPage(String arg, String page);
static void handleGetRelayConfigPage();
static void handleGetSwitchConfigPage();

static void handleGetRelayIndexPage();
static void handleGetSwitchIndexPage();

static void handleGetConfig(String _msg);
static void handleGetRelayConfig();
static void handleGetSwitchConfig();

static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const int8_t _last = -1);
static void get_relay_data_json(JsonObject &rel,
                                const String &_name,
                                const String &_descr,
                                const IPAddress _ip);
static void get_config_json_doc(DynamicJsonDocument &doc, ModuleType _mdl);
static String get_config_json_string(ModuleType _mdl);

static void handleSetConfig();

static void handleRelaySwitch();
static void handleRemoteRelaySwitch();
static void handleGetRelayState();

// ===================================================
static bool load_setting(ModuleType _mdt, DynamicJsonDocument &doc);

static String get_config_file_name(ModuleType _mdt);
static bool save_config_file(ModuleType _mdt);
static bool save_config_file(ModuleType _mdt, DynamicJsonDocument &doc);
static bool load_config_file(ModuleType _mdt);

// ==== shRelayControl class ===========================

shRelayControl::shRelayControl() {}

void shRelayControl::init(uint8_t _relay_count)
{
  relayArray = new shRelayData[_relay_count];
  if (relayArray)
  {
    relayCount = _relay_count;
  }

  if (&Serial != NULL)
  {
    serial = &Serial;
  }
}

bool shRelayControl::addRelay(const String &relay_name,
                              uint8_t relay_pin,
                              uint8_t control_level,
                              srButton *relay_button,
                              const String &relay_description)
{
  bool result = false;
  if (relayCount > 0)
  {
    for (uint8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayName == "")
      {
        relayArray[i] = shRelayData(relay_name,
                                    relay_pin,
                                    control_level,
                                    relay_button,
                                    relay_description);
        digitalWrite(relay_pin, !control_level);
        pinMode(relay_pin, OUTPUT);
        result = true;
        break;
      }
    }
  }
  return (result);
}

void shRelayControl::setLogOnState(bool _on, Print *_serial)
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shRelayControl::getLogOnState() { return (logOnState); }

void shRelayControl::setButtonBuzzerState(bool _state, int8_t _pin)
{
  bzr.setState(_state, _pin);
}

void shRelayControl::setBtnBeepData(uint16_t _freq, uint32_t _dur)
{
  bzr.setBtnBeepData(_freq, _dur);
}

void shRelayControl::startDevice(WiFiUDP *_udp, uint16_t _local_port)
{
  udp = _udp;
  localPort = _local_port;
}

void shRelayControl::attachWebInterface(shWebServer *_server,
                                        FS *_file_system,
                                        const String &_relay_config_page,
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

  relay_config_page = _relay_config_page;
  wifi_config_page = _wifi_config_page;

  if (relay_config_page.indexOf("/") != 0)
  {
    relay_config_page = "/" + relay_config_page;
  }
  if (wifi_config_page.length() > 0 && wifi_config_page.indexOf("/") != 0)
  {
    wifi_config_page = "/" + wifi_config_page;
  }

  if (http_server)
  { // вызов стартовой страницы модуля реле
    http_server->on("/", HTTP_GET, handleGetRelayIndexPage);
    // вызов страницы настройки модуля реле
    http_server->on(relay_config_page, HTTP_GET, handleGetRelayConfigPage);
    // запрос текущих настроек
    http_server->on(FPSTR(RELAY_GET_CONFIG), HTTP_GET, handleGetRelayConfig);
    // сохранение настроек
    http_server->on(FPSTR(SR_SET_CONFIG), HTTP_POST, handleSetConfig);
    // переключение реле
    http_server->on(FPSTR(RELAY_SWITCH), HTTP_POST, handleRelaySwitch);
    // запрос текущего состояния всех реле
    http_server->on(FPSTR(RELAY_GET_STATE), HTTP_GET, handleGetRelayState);
  }
}

void shRelayControl::tick()
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

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
  {
    receiveUdpPacket(packet_size);
  }

  http_server->handleClient();
  delay(1);
}

void shRelayControl::respondToRelayCheck(int8_t index)
{
  if ((index >= 0) && (index < relayCount))
  {
    String s = get_json_string_to_send(relayArray[index].relayName,
                                       relayArray[index].relayDescription,
                                       sr_ok_str,
                                       sr_respond_str);
    SR_PRINT(relayArray[index].relayName);
    SR_PRINT(F(": request received, response - "));
    SR_PRINTLN(sr_ok_str);
    send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
  }
}

void set_state(int8_t relay_index, String comm)
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
                                       relayArray[relay_index].relayDescription,
                                       get_relay_state(relay_index),
                                       comm);
    send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
  }
}

void shRelayControl::receiveUdpPacket(int _size)
{
  char _str[_size + 1] = {0};

  udp->read(_str, _size);
  udp->flush();

  String _resp = String(_str);
  String comm = get_argument(_resp, sr_command_str);
  String r_name = get_argument(_resp, sr_name_str);
  if (comm == sr_respond_str)
  {
    if (r_name == sr_any_str)
    {
      for (uint8_t i = 0; i < relayCount; i++)
      {
        respondToRelayCheck(i);
      }
    }
    else
    {
      respondToRelayCheck(getRelayIndexByName(r_name));
    }
  }
  else if ((comm == sr_switch_str) ||
           (comm == sr_set_on_str) ||
           (comm == sr_set_off_str))
  {
    if (r_name == sr_any_str)
    {
      for (uint8_t i = 0; i < relayCount; i++)
      {
        set_state(i, comm);
      }
    }
    else
    {
      set_state(getRelayIndexByName(_resp), comm);
    }
  }
  else
  {
    // ответ о неизвестной команде
    String s = get_json_string_to_send(WiFi.localIP().toString(),
                                       module_description,
                                       F("unknown command"),
                                       comm);
    send_udp_packet(udp->remoteIP(), s.c_str(), s.length());
  }
}

int8_t shRelayControl::getRelayIndexByName(String &_res)
{
  int8_t result = -1;
  String name = get_argument(_res, sr_name_str);
  if (name.length() > 0)
  {
    for (int8_t i = 0; i < relayCount; i++)
    {
      if (relayArray[i].relayName == name)
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

void shRelayControl::setFileName(const String &_name)
{
  relayFileConfigName = _name;
}

String shRelayControl::getFileName()
{
  return (relayFileConfigName);
}

bool shRelayControl::saveConfige()
{
  return (save_config_file(mtRelay));
}

bool shRelayControl::loadConfig()
{
  return (load_config_file(mtRelay));
}

// ==== shSwitchControl class ==========================

shSwitchControl::shSwitchControl() {}

void shSwitchControl::init(uint8_t _switch_count)
{
  switchArray = new shSwitchData[_switch_count];
  if (switchArray)
  {
    switchCount = _switch_count;
  }

  if (&Serial != NULL)
  {
    serial = &Serial;
  }
}

bool shSwitchControl::addRelay(const String &relay_name,
                               srButton *relay_button)
{
  bool result = false;
  if (switchCount > 0)
  {
    for (uint8_t i = 0; i < switchCount; i++)
    {
      if (switchArray[i].relayName == "")
      {
        switchArray[i] = shSwitchData(relay_name,
                                      relay_button);
        result = true;
        break;
      }
    }
  }
  return (result);
}

void shSwitchControl::setLogOnState(bool _on, Print *_serial)
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shSwitchControl::getLogOnState() { return (logOnState); }

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

void shSwitchControl::startDevice(WiFiUDP *_udp, uint16_t _local_port)
{
  udp = _udp;
  localPort = _local_port;
  // выполнить первичный поиск привязанных реле
  find_remote_relays();
}

void shSwitchControl::attachWebInterface(shWebServer *_server,
                                         FS *_file_system,
                                         const String &_relay_config_page,
                                         const String &_wifi_config_page)
{
  http_server = _server;
  file_system = _file_system;

  relay_config_page = _relay_config_page;
  wifi_config_page = _wifi_config_page;

  if (relay_config_page.indexOf("/") != 0)
  {
    relay_config_page = "/" + relay_config_page;
  }
  if (wifi_config_page.length() > 0 && wifi_config_page.indexOf("/") != 0)
  {
    wifi_config_page = "/" + wifi_config_page;
  }

  load_config_file(mtSwitch);

  if (http_server)
  { // вызов стартовой страницы модуля выключателя
    http_server->on("/", HTTP_GET, handleGetSwitchIndexPage);
    // вызов страницы настройки модуля выключателей
    http_server->on(_relay_config_page, HTTP_GET, handleGetSwitchConfigPage);
    // запрос текущих настроек
    http_server->on(FPSTR(SWITCH_GET_CONFIG), HTTP_GET, handleGetSwitchConfig);
    // сохранение настроек
    http_server->on(FPSTR(SR_SET_CONFIG), HTTP_POST, handleSetConfig);
    // переключение реле
    http_server->on(FPSTR(REMOTE_RELAY_SWITCH), HTTP_POST, handleRemoteRelaySwitch);
  }
}

void shSwitchControl::tick()
{
  for (uint8_t i = 0; i < switchCount; i++)
  {
    if (switchArray[i].relayButton != NULL &&
        switchArray[i].relayButton->getButtonState() == BTN_DOWN)
    {
      bzr.btnBeep(); // одиночный пик на каждое нажатие любой кнопки
      switch_remote_relay(i);
    }
  }

  // проверка доступности реле через заданный интервал
  if (millis() - checkTimer >= checkInterval)
  {
    checkTimer = millis();
    find_remote_relays();
  }

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
  {
    receiveUdpPacket(packet_size);
  }

  http_server->handleClient();
  delay(1);
}

void shSwitchControl::receiveUdpPacket(int _size)
{
  char _str[_size + 1] = {0};
  udp->read(_str, _size);
  udp->flush();

  String _resp = String(_str);
#if defined(ARDUINO_ARCH_ESP8266)
  if ((udp->destinationIP() != get_broadcast_address()))
  {
#endif
    int8_t relay_index = getRelayIndexByName(_resp);
    if (relay_index >= 0)
    {
      switchArray[relay_index].relayFound = true;
      String arg_for = get_argument(_resp, sr_for_str);
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
      SR_PRINT(get_argument(_resp, sr_name_str));
      SR_PRINT(F(", "));
      SR_PRINT(get_argument(_resp, sr_descr_str));
      SR_PRINT(F(" response - "));
      SR_PRINTLN(get_argument(_resp, sr_response_str));
    }
#if defined(ARDUINO_ARCH_ESP8266)
  }
  else
  {
    SR_PRINT(F("Skiped broadcast packet from "));
    SR_PRINTLN(udp->remoteIP());
  }
#endif
}

int8_t shSwitchControl::getRelayIndexByName(String &_res)
{
  int8_t result = -1;
  String name = get_argument(_res, sr_name_str);
  if (name.length() > 0)
  {
    for (int8_t i = 0; i < switchCount; i++)
    {
      if (switchArray[i].relayName == name)
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
    for (uint8_t i = 0; i < switchCount; i++)
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

void shSwitchControl::setRelayName(int8_t index, String _name)
{
  if ((index >= 0) && (index < switchCount))
  {
    switchArray[index].relayName = _name;
  }
}
String shSwitchControl::getRelayName(int8_t index)
{
  String result = "";
  if ((index >= 0) && (index < switchCount))
  {
    result = switchArray[index].relayName;
  }

  return (result);
}

void shSwitchControl::setFileName(const String &_name)
{
  switchFileConfigName = _name;
}

String shSwitchControl::getFileName()
{
  return (switchFileConfigName);
}

bool shSwitchControl::saveConfige()
{
  return (save_config_file(mtSwitch));
}

bool shSwitchControl::loadConfig()
{
  return (load_config_file(mtSwitch));
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
    result = doc[_arg].as<String>() != "null";
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
  bool result = udp->beginPacket(address, localPort);

  if (result)
  {
    uint8_t *_buf = new uint8_t[bufSize];
    result = (_buf != NULL);
    if (result)
    {
      memcpy(_buf, buf, bufSize);
      udp->write(_buf, bufSize);
      delete[] _buf;

      result = udp->endPacket() == 1;
    }
  }

  if (!result)
  {
    SR_PRINT(F("Error sending UDP packet for IP "));
    SR_PRINT(address);
    SR_PRINT(F(", remote port: "));
    SR_PRINTLN((String)localPort);
  }

  return (result);
}

static String get_argument(String &_res, const String &_arg)
{
  String result = "";
  get_value_of_argument(_res, _arg, result);
  return result;
}

static String get_json_string_to_send(const String &_name,
                                      const String &_descr,
                                      const String &_comm,
                                      const String &_for)
{
  StaticJsonDocument<RELAY_DATA_SIZE> doc;

  doc[sr_name_str] = _name;
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

    SR_PRINT(relayArray[index].relayName);
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
        (index < switchCount) &&
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
  SR_PRINTLN(F("Sending a request to check IP addresses of relays"));
  SR_PRINT(F("Broadcast address: "));
  SR_PRINTLN(broadcastAddress);
  send_udp_packet(broadcastAddress, s.c_str(), s.length());
}

// ==== реакции сервера ==============================
static void handleGetConfigPage(String arg, String page)
{
  String c_lab = F("<p id='label'>");
  page.replace(c_lab, c_lab + arg);
  http_server->send(200, FPSTR(TEXT_HTML), page);
}

static void handleGetRelayConfigPage()
{
  handleGetConfigPage(FPSTR(RELAY_GET_CONFIG), FPSTR(config_page));
}

static void handleGetSwitchConfigPage()
{
  handleGetConfigPage(FPSTR(SWITCH_GET_CONFIG), FPSTR(config_page));
}

static void handleGetRelayIndexPage()
{
  handleGetConfigPage(FPSTR(RELAY_GET_CONFIG), FPSTR(index_page));
}

static void handleGetSwitchIndexPage()
{
  handleGetConfigPage(FPSTR(SWITCH_GET_CONFIG), FPSTR(index_page));
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

static void get_config_json_doc(DynamicJsonDocument &doc, ModuleType _mdl)
{
  doc[sr_module_str] = module_description;
  JsonArray relays = doc.createNestedArray(sr_relays_str);
  doc[sr_wificonf_str] = wifi_config_page;
  doc[sr_relayconf_str] = relay_config_page;

  switch (_mdl)
  {
  case mtRelay:
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
    break;
  case mtSwitch:
    doc[sr_for_str] = sr_switch_str;

    for (int8_t i = 0; i < switchCount; i++)
    {
      JsonObject rel = relays.createNestedObject();
      get_relay_data_json(rel,
                          switchArray[i].relayName,
                          switchArray[i].relayDescription,
                          switchArray[i].relayAddress);
    }
    break;
  }
}

static String get_config_json_string(ModuleType _mdl)
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

static void getStringValue(String &_var, String _val)
{
  _var = (_val != "null") ? _val : "";
}

static void handleSetConfig()
{
  if (http_server->hasArg("plain") == false)
  {
    http_server->send(200, FPSTR(TEXT_PLAIN), F("Body not received"));
    SR_PRINTLN(F("Failed to save configuration data, no data"));
    return;
  }

  String json = http_server->arg("plain");

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
      load_setting(mtSwitch, doc);
      save_config_file(mtSwitch, doc);
    }
    http_server->send(200, FPSTR(TEXT_HTML), F("<META http-equiv='refresh' content='1;URL=/'><p align='center'>Save settings...</p>"));
  }
}

static void handleRelaySwitch()
{
  if (http_server->hasArg("plain"))
  {
    String json = http_server->arg("plain");

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
  if (http_server->hasArg("plain"))
  {
    String json = http_server->arg("plain");

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

static bool load_setting(ModuleType _mdt, DynamicJsonDocument &doc)
{
  getStringValue(module_description, doc[sr_module_str].as<String>());
  int8_t x = doc[sr_relays_str].size();
  switch (_mdt)
  {
  case mtRelay:
    save_state_of_relay = doc[sr_save_state_str].as<bool>();
    for (int8_t i = 0; i < x && i < relayCount; i++)
    {
      getStringValue(relayArray[i].relayName,
                     doc[sr_relays_str][i][sr_name_str].as<String>());
      getStringValue(relayArray[i].relayDescription,
                     doc[sr_relays_str][i][sr_descr_str].as<String>());
      relayArray[i].relayLastState = doc[sr_relays_str][i][sr_last_state_str].as<bool>();
    }
    break;
  case mtSwitch:
    for (int8_t i = 0; i < x && i < switchCount; i++)
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
    break;
  default:
    return (false);
    break;
  }
  return (true);
}

static String get_config_file_name(ModuleType _mdt)
{
  switch (_mdt)
  {
  case mtRelay:
    return (relayFileConfigName);
  case mtSwitch:
    return (switchFileConfigName);
  default:
    return ("");
  }
}

static bool save_config_file(ModuleType _mdt)
{
  DynamicJsonDocument doc(CONFIG_SIZE);
  get_config_json_doc(doc, _mdt);
  return (save_config_file(_mdt, doc));
}

static bool save_config_file(ModuleType _mdt, DynamicJsonDocument &doc)
{
  if (!file_system)
  {
    return (false);
  }

  String fileName = get_config_file_name(_mdt);

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

static bool load_config_file(ModuleType _mdt)
{
  File configFile;
  String fileName = get_config_file_name(_mdt);

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
    save_config_file(_mdt);
    return (result);
  }
  // Проверяем размер файла, будем использовать файл размером меньше 2048 байта
  size_t size = configFile.size();
  if (size > CONFIG_SIZE)
  {
    SR_PRINTLN(F("WiFi configuration file size is too large."));
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
    result = load_setting(_mdt, doc);
  }
  if (result)
  {
    SR_PRINTLN(F("OK"));
  }

  return (result);
}

// ==== shBuzzer class ===============================

void buzzerTick()
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