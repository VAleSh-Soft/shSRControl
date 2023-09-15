## shSRControl - (Switch & Relay Control) - библиотека для создания простых модулей WiFi-выключателей и WiFi-реле на базе ESP8266 и ESP32

Библиотека предназначена для огранизации работы и взаимодействия модулей друг с другом в локальной WiFi-сети с помощью udp-пакетов.

- [Основные возможности](#основные-возможности)
- [История версий](#история-версий)
- [Состав библиотеки](#состав-библиотеки)
- [Работа с библиотекой](#работа-с-библиотекой)
- [Остальные методы классов](#остальные-методы-классов)
  - [shSwitchControl](#shswitchcontrol)
  - [shRelayControl](#shrelaycontrol)
- [Web-интерфейс](#web-интерфейс)
  - [Главная страница модуля](#главная-страница-модуля)
  - [Страница настройки](#страница-настройки)
- [Работа с кнопками, библиотека shButton](#работа-с-кнопками-библиотека-shbutton)
- [Зависимости](#зависимости)


### Основные возможности
- Взаимодействие модулей с помощью udp-пакетов, как адресных, так и широковещательных;
- Быстрый поиск реле в сети; модуль WiFi-выключателя может обнаруживать привязанные к нему реле сразу после подключения к сети; 
- Для обнаружения привязанных реле не используется заранее прописанный IP-адрес модулей WiFi-реле; поиск ведется с помощью широковещательных udp-пакетов и позволяет обнаруживать модули реле практически мгновенно;
- в модуле WiFi-выключателя есть возможность подачи звукового сигнала в случае ошибки отправки команды удаленному реле;
- Периодическая проверка доступности реле в сети; периодичность проверки настраивается;
- Наличие Web-интерфейса для настройки модуля;
- При использовании Web-интерфейса настройки сохраняются в файловой системе и автоматически подгружаются при старте модуля;
- Модули реле могут использовать как высокий (HIGH), так и низкий (LOW) управляющий логический уровень; для каждого реле этот уровень настраивается индивидуально;
- Модули WiFi-реле могут иметь локальные кнопки, для управления реле по месту;

### История версий
 Версия 1.0 - 6.09.2023
+ добавлены методы setRelayState(), позволяющие установить конкретное состояние локального или удаленного реле;
+ добавлена возможность вывода сообщений в заданный HardwareSerial, например, Serial1, если он доступен;
+ в модуль выключателя добавлена возможность выдачи звукового сигнала об ошибке отправки команды удаленному реле; в случае, если удаленное реле не ответило на предыдущий запрос, т.е. считается недоступным, выдается два коротких пика; в случае отсутствия WiFI-соединения выдается три коротких пика;
+ в модуль выключателя добавлены методы `setErrorBuzzerState()` и `getErrorBuzzerState()`, позволяющие управлять выдачей звукового сигнала об ошибке;
* доработаны примеры;
* мелкие изменения и исправления;

 Версия 0.9 - 29.08.2023
* передача массива данных реле и их количества перенесена в конструкторы модулей, так логичнее и исключает некоторые проблемы считывания настроек при старте модуля;
* обновлены примеры;
* мелкие изменения и исправления;

 Версия 0.8 - 21.08.2023
+ добавлен Web-интерфейс для управления модулем реле; страница отображает состояние каждого реле и позволяет переключать состояние реле кликом мыши; так же со страницы есть доступ как к настройкам модуля, так и к настройкам WiFi (при условии использования библиотеки [shWiFiConfig](https://github.com/VAleSh-Soft/shWiFiConfig));
+ добавлен Web-интерфейс для управления модулем выключателя; страница отображает кнопки, зарегистрированные в прошивке модуля; кнопки позволяют переключать привязанные к ним удаленные реле, но их состояние не отображают; ссылки на настройки идентичны таковым на странице модуля реле;
 
 Версия 0.7 - 18.08.2023
+ добавлен Web-интерфейс для настройки модуля;
+ добавлены методы для работы с Web-интерфейсом;
* доработаны примеры для использования Web-интерфейса;
+ добавлена документация методов библиотеки;

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
  String relayDescription;
};
```
Здесь:
- `relayName` - имя реле (идентификатор), по которому к нему будет обращаться выключатель; имена реле не должны повторяться в пределах одного модуля; в случае одинаковых имен срабатывать всегда будет то, индекс которого в массиве меньше;
- `relayPin` - пин, к которому подключено реле;
- `shRelayControlLevel` - управляющий уровень реле (LOW или HIGH)
- `relayButton` - локальная кнопка, управляющая реле (располагается на самом модуле и предназначена для ручного управления реле); если локальная кнопка не нужна, свойству `relayButton` присваивается значение **NULL**;
- `relayDescription` - описание реле, которое будет видно в Web-интерфейсе;

```
// описание свойств выключателя
struct shSwitchData
{
  String relayName;
  bool relayFound;
  IPAddress relayAddress;
  shButton *relayButton; 
  String relayDescription;
};
```
Здесь:
- `relayName` - имя ассоциированного с кнопкой удаленного реле; имена реле не должны повторяться в пределах одного модуля; в случае одинаковых имен команда на переключение всегда будет подаваться по адресу реле, индекс которого в массиве меньше;
- `relayFound` - найдено или нет ассоциированное удаленное реле в сети; свойству присваивается **false** после отправки запроса на переключение реле (или его поиска в сети) и присваивается **true** после получения отклика от него;
- `relayAddress` - сохраненный последний IP-адрес модуля реле; по этому адресу выключатель отправляет запрос на переключение реле;
- `relayButton` - кнопка, управляющая удаленным реле с именем `relayName`;
- `relayDescription` - описание ассоциированного удаленного реле;

### Работа с библиотекой

Каждый модуль может иметь несколько реле или кнопок, к каждой кнопке модуля WiFi-выключателя должно быть привязано реле на модуле WiFi-реле. Привязка осуществляется путем задания имени реле. Количество реле/кнопок для одного модуля ограничено лишь числом доступных пинов и доступной памятью модуля ESP и теоретически может достигать 128, однако, при использовании Web-интерфейса сохранение данных гарантировано не более, чем для 8 реле.

В библиотеку данные о реле передаются в виде массивов:

```
shSwitchData relays[];
```
или
```
shRelayData relays[];
```
Примеры заполнения массивов данными можно посмотреть в папке **examples**.

Экземпляр модуля объявляется с помощью конструктора с передачей в него массива реле и их количества:
```
shSwitchControl switch_control(relays, relay_count);
```
или
```
shRelayControl relay_control(relays, relay_count);
```
Здесь:
- `relays` - массив данных реле, локальных (для модуля реле) или удаленных (для модуля выключателя);
- `relay_count` - количество реле/выключателей в модуле;

Если вы планируете использовать Web-интерфейс, то нужно вызвать метод
```
void attachWebInterface(ESP8266WebServer *_server, FS *_file_system, String _config_page = "/relay_config");
```
Здесь:
- `_server` - ссылка на экземпляр Web-сервера (`ESP8266WebServer` для **esp8266** или `WebServer` для **esp32**), с которым будет работать модуль;
- `_file_system` - ссылка на экземпляр файловой системы модуля для сохранения файла с настройками;
- `_config_page` - необязательный параметр, адрес, по которому будет вызываться Web-страница конфигурации; по умолчанию используется адрес **/relay_config**;

В обоих классах методы `attachWebInterface()` идентичны
```
relay_control.attachWebInterface(&HTTP, &FILESYSTEM);
```
```
switch_control.attachWebInterface(&HTTP, &FILESYSTEM);
```
При вызове метода выполняется загрузка сохраненных параметров, поэтому файловая система к моменту вызова метода уже должна быть инициирована. Для модуля реле выполняется восстановление последнего состояния всех подключенных к нему реле, если это задано в настройках.

Перед началом работы необходимо вызвать метод `begin()`:
```
void shRelayControl::begin(WiFiUDP *_udp, 
                           uint16_t _local_port)
```
Здесь:
- `_udp` - ссылка на экземпляр **WiFiUDP**, который будет использоваться для работы модуля;
- `_local_port` - порт для отправки/приема udp-пакетов; должен быть одинаковым для всех связанных модулей;

В обоих классах методы `begin()` идентичны.
```
relay_control.begin(&udp, localPort);
```
```
switch_control.begin(&udp, localPort);
```

Для работы используется метод `tick()`, который должен вызываться как можно чаще, поэтому просто помещается в `loop()`.
```
void loop()
{
  switch_control.tick();// или relay_control.tick();
  HTTP.handleClient(); // если у вас подключен Web-интерфейс
  delay(1);
}
```

### Остальные методы классов
#### shSwitchControl
  
- `void setLogOnState(bool _on, HardwareSerial *_serial = &Serial)` - включение и отключение вывода информации о работе модуля; второй параметр - Serial для вывода сообщений, позволяет задать, например, Serial1, если тот доступен;
- `bool getLogOnState()` - включен или отключен вывод информации о работе модуля через Serial;
- `void setErrorBuzzerState(bool _state, int8_t _pin = -1)` - включить/выключить подачу звукового сигнала об ошибке отправки команды удаленному реле; `_state` - новое состояние опции; `_pin` - пин, к которому подключен буззер; если не указать номер пина, то он будет установлен в значение **-1** (не задан), а опция будет в любом случае отключена;
- `bool getErrorBuzzerState()` - получить состояние опции подачи звукового сигнала об ошибке отправки команды удаленному реле;
- `void setCheckTimer(uint32_t _timer)` - установка интервала проверки доступности связанных реле в сети в милисекундах; по умолчанию установлен интервал в 30 секунд;
- `uint32_t getCheckTimer()` - получение размера интервала проверки доступности связанных реле в сети в милисекундах;
- `void switchRelay(int8_t index)` - переключение удаленного реле; `index` - индекс реле в массиве данных;
- `void switchRelay(String _name)` - переключение удаленного реле; `_name` - имя удаленного реле;
- `void setRelayState(int8_t index, bool state)` - установить состояние удаленного реле; `index` - индекс реле в массиве данных, `state` - новое состояние удаленного реле;
- `void setRelayState(String _name, bool state)` - установить состояние удаленного реле; `_name` - имя удаленного реле, `state` - новое состояние удаленного реле;
- `void findRelays()` - поиск связанных реле в сети;
- `void setModuleDescription(String _descr)` - установка описания модуля; `_descr` - описание;
- `String getModuleDescription()` - получение текущего описания модуля;
- `void setRelayName(int8_t index, String _name)` - установка имени удаленного реле, которым будет управлять кнопка; `index` - индекс реле в массиве данных, `_name` - новое имя реле;
- `String getRelayName(int8_t index)` - получение имени реле; `index` - индекс реле в массиве данных;
- `void setFileName(String _name)` - установка имени файла для сохранения параметров модуля; `_name` - имя файла;
- `String getFileName()` - получение текущего именя файла;
- `bool saveConfige()` - сохранение настроек в файл;
- `bool loadConfig()` - считывание настроек из файла; при старте модуля загрузка сохраненных параметров выполняется автоматически в методе `attachWebInterface()`.

#### shRelayControl

- `void setLogOnState(bool _on, HardwareSerial *_serial = &Serial)` - включение и отключение вывода информации о работе модуля; второй параметр - Serial для вывода сообщений, позволяет задать, например, Serial1, если тот доступен;
- `bool getLogOnState()` - включен или отключен вывод информации о работе модуля через Serial;
- `void switchRelay(int8_t index)` - переключение реле; `index` - индекс реле в массиве данных;
- `void switchRelay(String _name)` - переключение реле; `_name` - имя реле;
- `void setRelayState(int8_t index, bool state)` - установить состояние реле; `index` - индекс реле в массиве данных, `state` - новое состояние реле;
- `void setRelayState(String _name, bool state)` - установить состояние реле; `_name` - имя реле, `state` - новое состояние реле;
- `String getRelayState(int8_t index)` - получение информации о текущем состоянии реле (включено/отключено); `index` - индекс реле в массиве данных;
- `String getRelayState(String _name)` - получение информации о текущем состоянии реле (включено/отключено); `_name` - имя реле;
- `void setModuleDescription(String _descr)` - установка описания модуля; `_descr` - описание;
- `String getModuleDescription()` - получение текущего описания модуля;
- `void setSaveStateOfRelay(bool _state)` - включение/выключение сохранения последнего состояния реле для последующего восстановления при перезапуске модуля; `_state` - значение для установки;
- `bool getSaveStateOfRelay()` - получение текущего состояния опции;
- `void setRelayName(int8_t index, String _name)` - установка имени реле; `index` - индекс реле в массиве данных, `_name` - новое имя реле;
- `String getRelayName(int8_t index)` - получение имени реле; `index` - индекс реле в массиве данных;
- `void setRelayDescription(int8_t index, String _descr)` - установка описания реле; `index` - индекс реле в массиве данных, `_descr_` - описание;
- `String getRelayDescription(int8_t index)` - получение текущего описания реле; `index` - индекс реле в массиве данных;
- `void setFileName(String _name)` - установка имени файла для сохранения параметров модуля; `_name` - имя файла;
- `String getFileName()` - получение текущего именя файла;
- `bool saveConfige()` - сохранение настроек в файл;
- `bool loadConfig()` - считывание настроек из файла; при старте модуля загрузка сохраненных параметров выполняется автоматически в методе `attachWebInterface()`.


### Web-интерфейс

#### Главная страница модуля

Доступ к Web-интерфейсу модуля осуществляется по адресу **/**. На этой странице отображаются:
- описание модуля;
- для модуля реле - переключатели и описание для каждого реле. Переключатели отображают текущее состояние реле и позволяют менять его кликом мыши.
- для модуля выключателя - кнопки и описания удаленных реле, привязанных к кнопкам; кнопки позволяют отправлять команду на переключение состояния удаленных реле кликом мыши;

Так же с этой страницы есть доступ как к настройкам модуля, так и к настройкам WiFi (ссылка на настройки wiFi будет работать только при условии использования библиотеки [shWiFiConfig](https://github.com/VAleSh-Soft/shWiFiConfig)).

#### Страница настройки

Доступ к Web-интерфейсу настройки модуля осуществляется по адресу **/relay_config** как для модуля реле, так и для модуля выключателя. Здесь можно настраивать следующие параметры:

Общие параметры модуля:
- **Описание модуля** - общее описание, например "Выключатель возле рабочего стола"; отображается только на главной странице модуля;
- **Сохранять состояние реле при перезагрузке** - опция доступна только для модуля реле, позволяет восстанавливать последнее состояние каждого реле модуля после включения в сеть, например, после отключения электроэнергии в доме;

Параметры для каждого реле/выключателя модуля:
- **Имя реле** - сетевой идентификатор, по которому к реле будет обращаться выключатель;
- **Описание реле** - текстовое описание, которое видно только в Web-интерфейсе, для удобства настройки; для модуля выключателей это поле только для чтения;

Количество пар **Имя+Описание** на странице зависит от количества реле/выключателей, подключенных к модулю и заданного в прошивке.

Настройки сохраняются в файловой системе модуля в файлах **relay.json** и **switch.json** соответственно.

### Работа с кнопками, библиотека shButton

Для работы с тактовыми кнопками модулей используется библиотека [shButton](https://github.com/VAleSh-Soft/shButton). Опрос кнопок ведется самим модулем в методе `tick()`, отлавливаются события **BTN_DOWN**, но ничто не мешает использовать это и другие события кнопок модуля в своих целях. Однако, здесь есть один нюанс - для корректной и надежной работы с кнопками нельзя допускать множественные опросы кнопки в пределах одного прохода `loop()`. А т.к. метод `getButtonState()` для каждой кнопки вызывается в методе `tick()` модуля, то для работы следует использовать метод `getLastState()` кнопок. Например:
```
void loop()
{
  switch_control.tick(); // здесь кнопки опрашиваются

  // =================================================

  // поэтому остается только получить результат опроса и использовать его
  switch (btn1.getLastState()) 
  {
  case BTN_LONGCLICK:
    Serial.println("btn1 hold");
    break;
  case BTN_DOWN:
    Serial.println("btn1 down");
    break;
  case BTN_DBLCLICK:
    Serial.println("btn1 dblclick");
    break;
  }

  switch (btn2.getLastState())
  {
  case BTN_LONGCLICK:
    Serial.println("btn2 hold");
    break;
  case BTN_DOWN:
    Serial.println("btn2 down");
    break;
  case BTN_DBLCLICK:
    Serial.println("btn2 dblclick");
    break;
  }  
  // =================================================

  HTTP.handleClient();

  delay(1);
}
```

Для подробного описания методов работы с библиотекой `shButton` смотрите ее файл **readme.md**, ссылка на гитхаб приведена ниже.

### Зависимости

Для работы модулей требуются сторонние библиотеки:
- `shButton.h` - https://github.com/VAleSh-Soft/shButton - работа с тактовыми кнопками;
- `ArduinoJson.h` (не ниже версии 6.0) - https://github.com/bblanchon/ArduinoJson - работа с данными в формате JSON;

<hr>

Если возникнут вопросы, пишите на valesh-soft@yandex.ru 
