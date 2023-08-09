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
struct RelayData
{
  String relayName;          // имя реле
  uint8_t relayPin;          // пин, к которому подключено реле
  uint8_t relayControlLevel; // управляющий уровень реле (LOW или HIGH)
  shButton *relayButton;     // локальная кнопка, управляющая реле (располагается на самом модуле и предназначена для ручного управления реле)
};

// описание свойств выключателя
struct SwitchData
{
  String relayName;       // имя ассоциированного с кнопкой реле
  bool relayFound;        // найдено или нет реле в сети
  IPAddress relayAddress; // IP адрес реле
  shButton *relayButton;  // кнопка, управляющая реле
};

class RelayControl
{
private:
  uint8_t relayCount = 0;
  RelayData *relayArray = NULL;

  String getJsonStringToSend(String _name, String _comm, String _for);
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  RelayControl();
  void begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, RelayData *relay_array);
  void tick();
  void switchRelay(int8_t index);
  void switchRelay(String _name);
  String getRelayState(int8_t index);
  String getRelayState(String _name);
};

class SwitchControl
{
private:
  uint8_t relayCount = 0;
  SwitchData *relayArray = NULL;
  IPAddress broadcastAddress;
  uint32_t checkTimer = 30000;

  String getJsonStringToSend(String _name, String _comm);
  void receiveUdpPacket(int _size);
  int8_t getRelayIndexByName(String &_res);

public:
  SwitchControl();
  void setCheckTimer(uint32_t _timer);
  uint32_t getCheckTimer();
  void begin(WiFiUDP *_udp, uint16_t _local_port, uint8_t _relay_count, SwitchData *relay_array);
  void tick();
  void switchRelay(int8_t index);
  void switchRelay(String _name);
  void findRelays();
  // TODO: добавить возможность проверки конкретного реле по имени
};