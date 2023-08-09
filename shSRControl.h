#pragma once

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#error "The library is designed to be used in an ESP8266 or ESP32 environment"
#endif
#include <WiFiUdp.h>
#include <shButton.h>

// описание свойств реле
struct shRelayData
{
  String relayName;          // имя реле
  uint8_t relayPin;          // пин, к которому подключено реле
  uint8_t shRelayControlLevel; // управляющий уровень реле (LOW или HIGH)
  shButton *relayButton;     // локальная кнопка, управляющая реле (располагается на самом модуле и предназначена для ручного управления реле)
};

// описание свойств выключателя
struct shSwitchData
{
  String relayName;       // имя ассоциированного с кнопкой удаленного реле
  bool relayFound;        // найдено или нет ассоциированное удаленное реле в сети
  IPAddress relayAddress; // IP адрес удаленного реле
  shButton *relayButton;  // кнопка, управляющая удаленным реле
};
// TODO: подумать над возможностью задания множественных реле, имеющих одно имя, но физически расположенных на разных модулях; т.е. добавить еще одно свойство и при его активации посылать запрос на переключение не по адресу, а широковещательным пакетом

class shRelayControl
{
private:
  int8_t relayCount = 0;
  shRelayData *relayArray = NULL;

  String getJsonStringToSend(String _name, String _comm, String _for);
  void respondToRelayCheck(int8_t index);
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  shRelayControl();
  void setLogOnState(bool _on);
  bool getLogOnState();
  void begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, shRelayData *relay_array);
  void tick();
  void switchRelay(int8_t index);
  void switchRelay(String _name);
  String getRelayState(int8_t index);
  String getRelayState(String _name);
};

class shSwitchControl
{
private:
  int8_t relayCount = 0;
  shSwitchData *relayArray = NULL;
  IPAddress broadcastAddress;
  uint32_t checkTimer = 30000;

  String getJsonStringToSend(String _name, String _comm);
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  shSwitchControl();
  void setLogOnState(bool _on);
  bool getLogOnState();
  void setCheckTimer(uint32_t _timer);
  uint32_t getCheckTimer();
  void begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, shSwitchData *relay_array);
  void tick();
  void switchRelay(int8_t index);
  void switchRelay(String _name);
  void findRelays();
  // TODO: добавить возможность проверки конкретного реле по имени
};