#include "shSRControl.h"
#include <ArduinoJson.h>

// ==== имена параметров в запросах ==================
static const String sr_relay_name_str = "relay";
static const String sr_command_str = "command";
static const String sr_response_str = "resp";
static const String sr_for_str = "for";

// ==== значения параметров в запросах
static const String sr_ok_str = "ok";
static const String sr_no_str = "no";
static const String sr_on_str = "on";
static const String sr_off_str = "off";
static const String sr_switch_str = "switch";
static const String sr_respond_str = "respond";
static const String sr_any_str = "any_relay";

/*
строка запроса от выключателя - имя реле и команда: отозваться, если идет поиск, или переключить состояние реле
{"relay":"any_relay","command":"respond"}
{"relay":"relay1","command":"switch"}

строка ответа реле - кто отвечает, на что отвечает и ответ: состояние реле после выполнения команды или "ok" в случае ответа на поиск
{"relay":"relay1","for":"respond","resp":"ok"}
{"relay":"relay1","for":"switch","resp":"off"}
*/

// ==== общие данные =================================

static WiFiUDP *udp = NULL;
static uint16_t localPort = 0;
bool logOn = true;

static bool getValueOfArgument(String &_res, String _arg, String &_str);
static bool sendUdpPacket(const IPAddress &address, const char *buf, uint8_t bufSize);
static String getArgument(String _res, String _arg);
static void print(String _str);
static void println(String _str);

// ==== shRelayControl class ===========================

shRelayControl::shRelayControl() {}

void shRelayControl::setLogOnState(bool _on) { logOn = _on; }

bool shRelayControl::getLogOnState() { return (logOn); }

void shRelayControl::begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, shRelayData *_relay_array)
{
  udp = _udp;
  localPort = _local_port;
  relayCount = _relay_count;
  relayArray = _relay_array;
  for (uint8_t i = 0; i < relayCount; i++)
  {
    digitalWrite(relayArray[i].relayPin, !relayArray[i].shRelayControlLevel);
    pinMode(relayArray[i].relayPin, OUTPUT);
  }
}

void shRelayControl::tick()
{
  for (uint8_t i = 0; i < relayCount; i++)
  {
    if (relayArray[i].relayButton != NULL &&
        relayArray[i].relayButton->getButtonState() == BTN_DOWN)
    {
      switchRelay(i);
    }
  }

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
    receiveUdpPacket(packet_size);
}

String shRelayControl::getJsonStringToSend(String _name, String _comm, String _for)
{
  StaticJsonDocument<256> doc;

  doc[sr_relay_name_str] = _name;
  doc[sr_for_str] = _for;
  doc[sr_response_str] = _comm;

  String _res = "";
  serializeJson(doc, _res);

  return (_res);
}

void shRelayControl::respondToRelayCheck(int8_t index)
{
  if ((index >= 0) && (index <= relayCount))
  {
    String s = getJsonStringToSend(relayArray[index].relayName, sr_ok_str, sr_respond_str);
    print(relayArray[index].relayName);
    print(F(": request received, response - "));
    println(sr_ok_str);
    sendUdpPacket(udp->remoteIP(), s.c_str(), s.length());
  }
}

void shRelayControl::receiveUdpPacket(int _size)
{
  char _str[_size];

  udp->read(_str, _size);
  udp->flush();

  String _resp = String(_str);
  if (getArgument(_resp, sr_command_str) == sr_respond_str)
  {
    String r_name = getArgument(_resp, sr_relay_name_str);
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
  else
  {
    int8_t relay_index = getRelayIndexByName(_resp);
    if ((relay_index >= 0) &&
        (getArgument(_resp, sr_command_str) == sr_switch_str))
    {
      switchRelay(relay_index);

      String s = getJsonStringToSend(relayArray[relay_index].relayName, getRelayState(relay_index), sr_command_str);
      sendUdpPacket(udp->remoteIP(), s.c_str(), s.length());
    }
  }
}

int8_t shRelayControl::getRelayIndexByName(String &_res)
{
  int8_t result = -1;
  String name = getArgument(_res, sr_relay_name_str);
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
  if ((index >= 0) && (index <= relayCount))
  {
    digitalWrite(relayArray[index].relayPin, !digitalRead(relayArray[index].relayPin));
    print(relayArray[index].relayName);
    print(F(": state - "));
    println(getRelayState(index));
  }
}

void shRelayControl::switchRelay(String _name)
{
  switchRelay(getRelayIndexByName(_name));
}

String shRelayControl::getRelayState(int8_t index)
{
  String result = sr_off_str;

  if ((index >= 0) && (index <= relayCount))
  {
    bool state = digitalRead(relayArray[index].relayPin);
    if (!relayArray[index].shRelayControlLevel)
    {
      state = !state;
    }
    result = (state) ? sr_on_str : sr_off_str;
  }
  return (result);
}

String shRelayControl::getRelayState(String _name)
{
  return (getRelayState(getRelayIndexByName(_name)));
}

// ==== shSwitchControl class ==========================

shSwitchControl::shSwitchControl() {}

void shSwitchControl::setLogOnState(bool _on) { logOn = _on; }

bool shSwitchControl::getLogOnState() { return (logOn); }

void shSwitchControl::setCheckTimer(uint32_t _timer) { checkTimer = _timer; }

uint32_t shSwitchControl::getCheckTimer() { return (checkTimer); }

void shSwitchControl::begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, shSwitchData *_relay_array)
{
  udp = _udp;
  localPort = _local_port;
  relayCount = _relay_count;
  relayArray = _relay_array;
  broadcastAddress = (uint32_t)WiFi.localIP() | ~((uint32_t)WiFi.subnetMask());
  // выполнить первичный поиск привязанных реле
  findRelays();
}

void shSwitchControl::tick()
{
  for (uint8_t i = 0; i < relayCount; i++)
  {
    if (relayArray[i].relayButton != NULL &&
        relayArray[i].relayButton->getButtonState() == BTN_DOWN)
    {
      switchRelay(i);
    }
  }

  // проверка доступности реле через заданный интервал
  static uint32_t timer = 0;
  if (millis() - timer >= checkTimer)
  {
    timer = millis();
    findRelays();
  }

  int packet_size = udp->parsePacket();
  if (packet_size > 0)
    receiveUdpPacket(packet_size);
}

String shSwitchControl::getJsonStringToSend(String _name, String _comm)
{
  StaticJsonDocument<256> doc;

  doc[sr_relay_name_str] = _name;
  doc[sr_command_str] = _comm;

  String _res = "";
  serializeJson(doc, _res);

  return (_res);
}

void shSwitchControl::receiveUdpPacket(int _size)
{
  char _str[_size];

  udp->read(_str, _size);
  udp->flush();

  String _resp = String(_str);
  if ((udp->destinationIP() != broadcastAddress))
  {
    int8_t relay_index = getRelayIndexByName(_resp);
    if (relay_index >= 0)
    {
      String arg_for = getArgument(_resp, sr_for_str);
      if (arg_for == sr_respond_str)
      {
        relayArray[relay_index].relayAddress = udp->remoteIP();
        relayArray[relay_index].relayFound = true;
        print(relayArray[relay_index].relayName);
        print(F(" found, IP address: "));
        println(relayArray[relay_index].relayAddress.toString());
      }
      else if (arg_for == sr_command_str)
      {
        relayArray[relay_index].relayFound = true;
        String s = getArgument(_resp, sr_response_str);
        print(relayArray[relay_index].relayName);
        print(F(" response - "));
        println(s);
      }
    }
  }
  else
  {
    print(F("Skip broadcast packet "));
    println(udp->remoteIP().toString());
  }
}

int8_t shSwitchControl::getRelayIndexByName(String &_res)
{
  int8_t result = -1;
  String name = getArgument(_res, sr_relay_name_str);
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

void shSwitchControl::switchRelay(int8_t index)
{
  if ((index >= 0) && (index <= relayCount))
  {
    if (relayArray[index].relayFound)
    {
      String s = getJsonStringToSend(relayArray[index].relayName, sr_switch_str);
      println(F("Sending a request to switch the relay"));
      print(F("Relay name: "));
      print(relayArray[index].relayName);
      print(F("; IP: "));
      println(relayArray[index].relayAddress.toString());
      relayArray[index].relayFound = false;
      sendUdpPacket(relayArray[index].relayAddress, s.c_str(), s.length());
    }
    else
    {
      print(F("Relay "));
      print(relayArray[index].relayName);
      println(F(" not found!"));
      // TODO: подумать над звуковой индикацией ошибки
      findRelays();
    }
  }
}

void shSwitchControl::switchRelay(String _name)
{
  switchRelay(getRelayIndexByName(_name));
}

void shSwitchControl::findRelays()
{
  for (uint8_t i = 0; i < relayCount; i++)
  {
    relayArray[i].relayFound = false;
  }

  String s = getJsonStringToSend(sr_any_str, sr_respond_str);
  println(F("Sending a request to check IP addresses of relays"));
  print(F("Broadcast address: "));
  println(broadcastAddress.toString());
  sendUdpPacket(broadcastAddress, s.c_str(), s.length());
}

// ===================================================

bool getValueOfArgument(String &_res, String _arg, String &_str)
{
  StaticJsonDocument<256> doc;

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

bool sendUdpPacket(const IPAddress &address, const char *buf, uint8_t bufSize)
{
  udp->beginPacket(address, localPort);
  udp->write(buf, bufSize);
  bool result = udp->endPacket() == 1;
  if (!result)
  {
    print(F("Error sending UDP packet for IP "));
    print(address.toString());
    print(F(", remote port: "));
    print((String)localPort);
  }
  return (result);
}

String getArgument(String _res, String _arg)
{
  String result = "";
  getValueOfArgument(_res, _arg, result);
  return result;
}

void print(String _str)
{
  if (logOn)
  {
    print(_str);
  }
}

void println(String _str)
{
  if (logOn)
  {
    println(_str);
  }
}