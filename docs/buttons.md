# Работа с кнопками

### Работа с кнопками, класс srButton

Для работы с тактовыми кнопками модулей используется внутренний модуль **srButton.h**, который является адаптированной для использования здесь копией библиотеки [shButton](https://github.com/VAleSh-Soft/shButton). Все методы и возвращаемые значения аналогичны таковым библиотеки **shButton**.

Опрос кнопок ведется самим модулем в методе `tick()`, отлавливаются события **BTN_DOWN**, но ничто не мешает использовать это и другие события кнопок модуля в своих целях. Однако, здесь есть один нюанс - для корректной и надежной работы с кнопками нельзя допускать множественные опросы кнопки в пределах одного прохода `loop()`. А т.к. метод `getButtonState()` для каждой кнопки вызывается в методе `tick()` модуля, то для работы следует использовать метод `getLastState()` кнопок, возвращающий результат последнего опроса. Например:

```
void loop()
{
  switch_control.tick(); // кнопки опрашиваются здесь

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
}
```

Для подробного описания методов работы с библиотекой [shButton](https://github.com/VAleSh-Soft/shButton) смотрите ее файл **readme.md**.

