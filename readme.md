## shSRControl - (Switch & Relay Control) - библиотека для создания простых модулей WiFi-выключателей и WiFi-реле на базе ESP8266 и ESP32

Библиотека предназначена для огранизации работы и взаимодействия модулей друг с другом в локальной WiFi-сети с помощью udp-пакетов.

- [Основные возможности](#основные-возможности)
- [История версий](#история-версий)
- [Состав библиотеки](#состав-библиотеки)
- [Работа с библиотекой](#работа-с-библиотекой)
- [Остальные методы классов](#остальные-методы-классов)
  - [- shSwitchControl](#--shswitchcontrol)
  - [- shRelayControl](#--shrelaycontrol)
- [Зависимости](#зависимости)


### Основные возможности
- Взаимодействие модулей с помощью udp-пакетов, как адресных, так и широковещательных;
- Быстрый поиск реле в сети; модуль WiFi-выключателя может обнаруживать привязанные к нему реле сразу после подключения к сети; 
- Для обнаружения привязанных реле не используется заранее прописанный IP-адрес модулей WiFi-реле; поиск ведется с помощью широковещательных udp-пакетов и позволяет обнаруживать модули реле практически мгновенно;
- Периодическая проверка доступности реле в сети; периодичность проверки настраивается;
- Модули реле могут использовать как высокий (HIGH), так и низкий (LOW) управляющий логический уровень; для каждого реле этот уровень настраивается индивидуально;
- Модули WiFi-реле могут иметь локальные кнопки, для управления реле по месту;

### История версий
 Версия 0.6 - 09.08.2023
+ первая публикация библиотеки;

### Состав библиотеки

Библиотека содержит два класса;
- `shRelayControl` - для создания модулей WiFi-реле ("умные" розетки, "умные" люстры и т.д.);
- `shSwitchControl` - для создания модулей WiFi-выключателей;

А также две структуры, описывающие свойства реле и кнопок;
```
// описание свойств реле
struct shRelayData
{
  String relayName; 
  uint8_t relayPin;
  uint8_t shRelayControlLevel;
  shButton *relayButton; 
};
```
Здесь:
- `relayName` - имя реле, по которому к нему будет обращаться выключатель;
- `relayPin` - пин, к которому подключено реле;
- `shRelayControlLevel` - управляющий уровень реле (LOW или HIGH)
- `relayButton` - локальная кнопка, управляющая реле (располагается на самом модуле и предназначена для ручного управления реле); если локальная кнопка не нужна, свойству `relayButton` присваивается значение **NULL**;

```
// описание свойств выключателя
struct shSwitchData
{
  String relayName;
  bool relayFound;
  IPAddress relayAddress;
  shButton *relayButton; 
};
```
Здесь:
- `relayName` - имя ассоциированного с кнопкой удаленного реле
- `relayFound` - найдено или нет ассоциированное удаленное реле в сети; свойству присваивается **false** после отправки запроса на переключение реле (или его поиска в сети) и присваивается **true** после получения отклика от него;
- `relayAddress` - сохраненный последний IP-адрес модуля реле; по этому адресу выключатель отправляет запрос на переключение реле;
- `relayButton` - кнопка, управляющая удаленным реле с именем `relayName`;

### Работа с библиотекой

Экземпляр модуля объявляется с помощью конструктора без аргументов:
```
shSwitchControl switch_control;
```
или
```
shRelayControl relay_control;
```
Каждый модуль может иметь несколько реле или кнопок, к каждой кнопке модуля WiFi-выключателя должно быть привязано реле на модуле WiFi-реле. Привязка осуществляется путем задания имени реле. Количество реле/кнопок для одного модуля ограничено лишь числом доступных пинов и доступной памятью модуля ESP и теоретически может достигать 128.

В библиотеку данные о реле передаются в виде массивов:

```
shSwitchData relays[];
```
или
```
shRelayData relays[];
```
Примеры заполнения массивов данными можно посмотреть в папке **examples**.

Перед началом работы необходимо вызвать метод `begin()`:
```
void shRelayControl::begin(WiFiUDP *_udp, 
                           uint16_t _local_port,
                           uint8_t _relay_count, 
                           shRelayData *_relay_array)
```
Здесь:
- `_udp` - ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля;
- `_local_port` - порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей;
- `_relay_count` - количество реле/выключателей в модуле;
- `_relay_array` - массив данных реле;

В обоих классах методы `begin()` идентичны.
```
switch_control.begin(&udp, localPort, relays_count, relays);
```
```
relay_control.begin(&udp, localPort, relays_count, relays);
```

Для работы используется метод `tick()`, который должен вызываться как можно чаще, поэтому просто помещается в `loop()`.
```
void loop()
{
  switch_control.tick();// или relay_control.tick();

  delay(1);
}
```

### Остальные методы классов
#### - shSwitchControl
  
- `void findRelays()` - поиск связанных реле в сети;
- `void setLogOnState(bool _on)` - включение и отключение вывода информации о работе модуля через Serial;
- `bool getLogOnState()` - включен или отключен вывод информации о работе модуля через Serial;
- `void setCheckTimer(uint32_t _timer)` - установка интервала проверки доступности связанных реле в сети в милисекундах; по умолчанию установлен интервал в 30 секунд;
- `uint32_t getCheckTimer()` - получение размера интервала проверки доступности связанных реле в сети в милисекундах;
- `void switchRelay(int8_t index)` - включение удаленного реле; `index` - индекс реле в массиве данных;
- `void switchRelay(String _name)` - включение удаленного реле; `_name` - имя удаленного реле;

#### - shRelayControl

- `void setLogOnState(bool _on)` - включение и отключение вывода информации о работе модуля через Serial;
- `bool getLogOnState()` - включен или отключен вывод информации о работе модуля через Serial;
- `void switchRelay(int8_t index)` - включение реле локальной кнопкой; `index` - индекс реле в массиве данных;
- `void switchRelay(String _name)` - включение реле локальной кнопкой; `_name` - имя реле;
- `String getRelayState(int8_t index)` - получение информации о текущем состоянии реле (включено/отключено); `index` - индекс реле в массиве данных;
- `String getRelayState(String _name)` - получение информации о текущем состоянии реле (включено/отключено); `_name` - имя реле;

### Зависимости

Для работы модулей требуются сторонние библиотеки:
- `shButton.h` - https://github.com/VAleSh-Soft/shButton - работа с тактовыми кнопками;
- `ArduinoJson.h` (не ниже версии 6.0) - https://github.com/bblanchon/ArduinoJson - работа с данными в формате JSON;

<hr>

Если возникнут вопросы, пишите на valesh-soft@yandex.ru 