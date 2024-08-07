#include "shSRControl.h"
#include <ArduinoJson.h>
#include "extras/c_page.h"
#include "extras/i_page.h"

// ==== имена параметров в запросах ==================
static const String sr_name_str = "name";
static const String sr_command_str = "command";
static const String sr_response_str = "resp";
static const String sr_for_str = "for";
static const String sr_descr_str = "descr";
static const String sr_relays_str = "relays";
static const String sr_module_str = "module";
static const String sr_last_state_str = "last";
static const String sr_save_state_str = "save_state";

// ==== значения параметров в запросах ===============
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
строка запроса от выключателя - имя реле и команда: отозваться, если идет поиск, или переключить состояние реле
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
static const uint16_t CONFIG_SIZE = 2048;
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

#if ARDUINO_USB_CDC_ON_BOOT // Serial используется для USB CDC
static HWCDC *serial = NULL;
#else
static HardwareSerial *serial = NULL;
#endif
static bool logOnState = true;

#if defined(ARDUINO_ARCH_ESP32)
static WebServer *http_server = NULL;
#else
static ESP8266WebServer *http_server = NULL;
#endif
static FS *file_system = NULL;
static WiFiUDP *udp = NULL;
static uint16_t localPort = 0;

static ErrorBuzzer err;

// ===================================================

static IPAddress get_broadcast_address();
static bool get_value_of_argument(String &_res, String _arg, String &_str);
static bool send_udp_packet(const IPAddress &address, const char *buf, size_t bufSize);
static String get_argument(String _res, String _arg);
static String get_json_string_to_send(String _name, String _comm);
static String get_json_string_to_send(String _name,
                                      String _descr,
                                      String _comm,
                                      String _for);

static void print(String _str);
static void println(String _str);

static void switch_local_relay(int8_t index);
static void set_local_relay_state(int8_t index, bool state);
static String get_relay_state(int8_t index);

static void switch_remote_relay(int8_t index);
static void set_remote_relay_state(int8_t index, bool state);
static void set_all_remote_relay_state(bool state);
static void send_command_for_relay(int8_t index, String command);

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
                                String _name,
                                String _descr,
                                int8_t _last = -1);
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

void shRelayControl::init(shRelayData *_relay_array, uint8_t _relay_count)
{
  relayArray = _relay_array;
  relayCount = _relay_count;

  if (&Serial != NULL)
  {
    serial = &Serial;
  }

  for (uint8_t i = 0; i < relayCount; i++)
  {
    digitalWrite(relayArray[i].relayPin, !relayArray[i].relayControlLevel);
    pinMode(relayArray[i].relayPin, OUTPUT);
  }
}

#if ARDUINO_USB_CDC_ON_BOOT // Serial используется для USB CDC
void shRelayControl::setLogOnState(bool _on, HWCDC *_serial)
#else
void shRelayControl::setLogOnState(bool _on, HardwareSerial *_serial)
#endif
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shRelayControl::getLogOnState() { return (logOnState); }

void shRelayControl::startDevice(WiFiUDP *_udp, uint16_t _local_port)
{
  udp = _udp;
  localPort = _local_port;
}

#if defined(ARDUINO_ARCH_ESP32)
void shRelayControl::attachWebInterface(WebServer *_server,
                                        FS *_file_system,
                                        String _config_page)
#else
void shRelayControl::attachWebInterface(ESP8266WebServer *_server,
                                        FS *_file_system,
                                        String _config_page)
#endif
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

  if (http_server)
  { // вызов стартовой страницы модуля реле
    http_server->on("/", HTTP_GET, handleGetRelayIndexPage);
    // вызов страницы настройки модуля реле
    http_server->on(_config_page, HTTP_GET, handleGetRelayConfigPage);
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
      switch_local_relay(i);
    }
  }

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
    receiveUdpPacket(packet_size);
}

void shRelayControl::respondToRelayCheck(int8_t index)
{
  if ((index >= 0) && (index < relayCount))
  {
    String s = get_json_string_to_send(relayArray[index].relayName,
                                       relayArray[index].relayDescription,
                                       sr_ok_str,
                                       sr_respond_str);
    print(relayArray[index].relayName);
    print(F(": request received, response - "));
    println(sr_ok_str);
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

void shRelayControl::setModuleDescription(String _descr)
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

void shRelayControl::setRelayName(int8_t index, String _name)
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

void setRelayDescription(int8_t index, String _descr)
{
  if ((index >= 0) && (index < relayCount))
  {
    relayArray[index].relayDescription = _descr;
  }
}

String getRelayDescription(int8_t index)
{
  String result = "";
  if ((index >= 0) && (index < relayCount))
  {
    result = relayArray[index].relayDescription;
  }

  return (result);
}

void shRelayControl::setFileName(String _name)
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

void shSwitchControl::init(shSwitchData *_switch_array, uint8_t _switch_count)
{
  switchCount = _switch_count;
  switchArray = _switch_array;

  if (&Serial != NULL)
  {
    serial = &Serial;
  }
}

#if ARDUINO_USB_CDC_ON_BOOT // Serial используется для USB CDC
void shSwitchControl::setLogOnState(bool _on, HWCDC *_serial)
#else
void shSwitchControl::setLogOnState(bool _on, HardwareSerial *_serial)
#endif
{
  logOnState = _on;
  serial = (logOnState) ? _serial : NULL;
}

bool shSwitchControl::getLogOnState() { return (logOnState); }

void shSwitchControl::setErrorBuzzerState(bool _state, int8_t _pin)
{
  err.setState(_state, _pin);
}

bool shSwitchControl::getErrorBuzzerState() { return (err.getState()); }

void shSwitchControl::setCheckTimer(uint32_t _timer) { checkTimer = _timer; }

uint32_t shSwitchControl::getCheckTimer() { return (checkTimer); }

void shSwitchControl::startDevice(WiFiUDP *_udp, uint16_t _local_port)
{
  udp = _udp;
  localPort = _local_port;
  // выполнить первичный поиск привязанных реле
  find_remote_relays();
}

#if defined(ARDUINO_ARCH_ESP32)
void shSwitchControl::attachWebInterface(WebServer *_server,
                                         FS *_file_system,
                                         String _config_page)
#else
void shSwitchControl::attachWebInterface(ESP8266WebServer *_server,
                                         FS *_file_system,
                                         String _config_page)
#endif
{
  http_server = _server;
  file_system = _file_system;

  load_config_file(mtSwitch);

  if (http_server)
  { // вызов стартовой страницы модуля выключателя
    http_server->on("/", HTTP_GET, handleGetSwitchIndexPage);
    // вызов страницы настройки модуля выключателей
    http_server->on(_config_page, HTTP_GET, handleGetSwitchConfigPage);
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
      switch_remote_relay(i);
    }
  }

  // проверка доступности реле через заданный интервал
  static uint32_t timer = 0;
  if (millis() - timer >= checkTimer)
  {
    timer = millis();
    find_remote_relays();
  }

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
    receiveUdpPacket(packet_size);
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
        print(switchArray[relay_index].relayName);
        print(F(" found, IP address: "));
        println(switchArray[relay_index].relayAddress.toString());
      }
      else if (arg_for == sr_switch_str ||
               arg_for == sr_set_on_str ||
               arg_for == sr_set_off_str)
      {
        print(switchArray[relay_index].relayName);
        print(F(" response - "));
        println(get_argument(_resp, sr_response_str));
      }
    }
    else
    {
      // ответ на случай, если имя ответившего реле модулю неизвестно
      print(F("Module "));
      print(get_argument(_resp, sr_name_str));
      print(F(", "));
      print(get_argument(_resp, sr_descr_str));
      print(F(" response - "));
      println(get_argument(_resp, sr_response_str));
    }
#if defined(ARDUINO_ARCH_ESP8266)
  }
  else
  {
    print(F("Skip broadcast packet "));
    println(udp->remoteIP().toString());
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

void shSwitchControl::setModuleDescription(String _descr)
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

void shSwitchControl::setFileName(String _name)
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
  IPAddress result{0, 0, 0, 0};
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

static bool get_value_of_argument(String &_res, String _arg, String &_str)
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
    println(F("invalid response data"));
    println(error.f_str());
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
    print(F("Error sending UDP packet for IP "));
    print(address.toString());
    print(F(", remote port: "));
    println((String)localPort);
  }

  return (result);
}

static String get_argument(String _res, String _arg)
{
  String result = "";
  get_value_of_argument(_res, _arg, result);
  return result;
}

static String get_json_string_to_send(String _name, String _descr, String _comm, String _for)
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

static String get_json_string_to_send(String _name, String _comm)
{
  StaticJsonDocument<RELAY_DATA_SIZE> doc;

  doc[sr_name_str] = _name;
  doc[sr_command_str] = _comm;

  String _res = "";
  serializeJson(doc, _res);

  return (_res);
}

static void print(String _str)
{
  if (logOnState && serial)
  {
    serial->print(_str);
  }
}

static void println(String _str)
{
  if (logOnState && serial)
  {
    serial->println(_str);
  }
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

    print(relayArray[index].relayName);
    print(F(": state - "));
    println(get_relay_state(index));
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
    print(F("Sending a request to set the state for all remote relays: "));
    println(st);
    print(F("Broadcast address: "));
    println(broadcastAddress.toString());
    send_udp_packet(broadcastAddress, s.c_str(), s.length());
  }
  else
  {
    err.startBuzzer(3);
    println(F("Failed to send command to remote relay, connection lost"));
  }
}

static void send_command_for_relay(int8_t index, String command)
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
        println(F("Sending a command to remote relay"));
        print(F("Relay name: "));
        print(switchArray[index].relayName);
        print(F("; IP: "));
        print(switchArray[index].relayAddress.toString());
        print(F("; command: "));
        println(command);
        switchArray[index].relayFound = false;
        send_udp_packet(switchArray[index].relayAddress, s.c_str(), s.length());
      }
      else
      {
        err.startBuzzer(2);
        print(F("Relay "));
        print(switchArray[index].relayName);
        println(F(" not found!"));
        find_remote_relays();
      }
    }
  }
  else
  {
    err.startBuzzer(3);
    println(F("Failed to send command to remote relay, connection lost"));
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
  println(F("Sending a request to check IP addresses of relays"));
  print(F("Broadcast address: "));
  println(broadcastAddress.toString());
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

static void get_relay_data_json(JsonObject &rel, String _name, String _descr, int8_t _last)
{
  if (_last >= 0)
  {
    rel[sr_last_state_str] = _last;
  }
  rel[sr_name_str] = _name;
  rel[sr_descr_str] = _descr;
}

static void get_config_json_doc(DynamicJsonDocument &doc, ModuleType _mdl)
{
  doc[sr_module_str] = module_description;
  JsonArray relays = doc.createNestedArray(sr_relays_str);
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
                          switchArray[i].relayDescription);
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
    println(F("Failed to save configuration data, no data"));
    return;
  }

  String json = http_server->arg("plain");

  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    println(F("Failed to save configuration data, invalid json data"));
    println(error.f_str());
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

  print(F("Save settings to file "));
  println(fileName);

  File configFile;

  // удалить существующий файл, иначе конфигурация будет добавлена ​​к файлу
  file_system->remove(fileName);

  // Открыть файл для записи
  configFile = file_system->open(fileName, "w");

  if (!configFile)
  {
    print(F("Failed to create configuration file: "));
    println(fileName);
    return (false);
  }

  // сериализовать JSON-файл
  bool result = serializeJson(doc, configFile);
  if (result)
  {
    println(F("OK"));
  }
  else
  {
    print(F("Failed to write file "));
    println(fileName);
  }

  configFile.close();
  return (result);
}

static bool load_config_file(ModuleType _mdt)
{
  File configFile;
  String fileName = get_config_file_name(_mdt);

  print(F("Load settings from file "));
  println(fileName);

  // находим и открываем для чтения файл конфигурации
  bool result = file_system &&
                file_system->exists(fileName) &&
                (configFile = file_system->open(fileName, "r"));

  // если файл конфигурации не найден, сохранить настройки по умолчанию
  if (!result)
  {
    println(F("Config file not found, default config used."));
    save_config_file(_mdt);
    return (result);
  }
  // Проверяем размер файла, будем использовать файл размером меньше 1024 байта
  size_t size = configFile.size();
  if (size > CONFIG_SIZE)
  {
    println(F("WiFi configuration file size is too large."));
    configFile.close();
    return (false);
  }

  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  if (error)
  {
    print("Data serialization error: ");
    println(error.f_str());
    println(F("Failed to read config file, default config is used"));
    result = false;
  }
  else
  // Теперь можно получить значения из doc
  {
    result = load_setting(_mdt, doc);
  }
  if (result)
  {
    println(F("OK"));
  }

  return (result);
}

// ==== ErrorBuzzer class ============================

void buzzerTick()
{
  err.bip();
  if (err.decBipCount() == 0)
  {
    err.stopBuzzer();
  }
}

ErrorBuzzer::ErrorBuzzer() {}

void ErrorBuzzer::setState(bool _state, int8_t _pin)
{
  pin = _pin;
  state = (pin >= 0) ? _state : false;
}

bool ErrorBuzzer::getState()
{
  return (state);
}

void ErrorBuzzer::startBuzzer(uint8_t _num)
{
  buzzer.detach();
  if (state && pin >= 0)
  {
    bip_count = _num;
    buzzer.attach_ms(dur * 2, buzzerTick);
    buzzerTick();
  }
}

void ErrorBuzzer::stopBuzzer()
{
  buzzer.detach();
}

void ErrorBuzzer::bip()
{
  if (state && pin >= 0)
  {
    tone(pin, freq, dur);
  }
  else
  {
    stopBuzzer();
  }
}

uint8_t ErrorBuzzer::decBipCount()
{
  return (--bip_count);
}