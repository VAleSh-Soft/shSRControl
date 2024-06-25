#include <Arduino.h>
#include "srButtons.h"


// ---- srButton private -----------------------

bool srButton::getFlag(uint8_t _bit)
{
  bool result = (_bit < 8) ? (((_flags) >> (_bit)) & 0x01) : false;
  return (result);
}

void srButton::setFlag(uint8_t _bit, bool x)
{
  if (_bit < 8)
  {
    (x) ? (_flags) |= (1UL << (_bit)) : (_flags) &= ~(1UL << (_bit));
  }
}

bool srButton::getContactsState()
{
  bool val = digitalRead(_PIN);
  if (getFlag(INPUTTYPE_BIT) == PULL_UP)
  {
    val = !val;
  }
  if (getFlag(BTNTYPE_BIT) == BTN_NC)
  {
    return (!val);
  }

  return (val);
}

void srButton::setBtnUpDown(bool flag, uint32_t thisMls)
{
  setFlag(DEBOUNCE_BIT, false);
  setFlag(FLAG_BIT, flag);

  if (flag)
  {
    if (!getFlag(ONECLICK_BIT))
    { // если это первый клик, запустить таймер двойного клика и поднять флаг одиночного клика
      _btn_state = BTN_DOWN;
      setFlag(ONECLICK_BIT, true);
      dbl_timer = thisMls;
    }
    else if (thisMls - dbl_timer <= _dblclck_timeout)
    {
      _btn_state = BTN_DBLCLICK;
      setFlag(ONECLICK_BIT, false);
    }
  }
  else
  {
    _btn_state = BTN_UP;
  }
}

// ---- srButton public ------------------------

srButton::srButton(uint8_t pin, bool serial_mode)
{
  _PIN = pin;
  setFlag(INPUTTYPE_BIT, PULL_UP);
  pinMode(_PIN, INPUT_PULLUP);
  // (BTN_INPUT_TYPE == PULL_UP) ? pinMode(_PIN, INPUT_PULLUP) : pinMode(_PIN, INPUT);
  setFlag(BTNTYPE_BIT, BTN_NO);
  if (serial_mode)
  {
    _longclick_mode = LCM_CLICKSERIES;
  }
}

uint8_t srButton::getButtonState()
{
  uint32_t thisMls = millis();

  // если поднят флаг подавления дребезга и интервал еще не вышел, больше ничего не делать
  if (_debounce_timeout > 0 &&
      getFlag(DEBOUNCE_BIT) &&
      thisMls - btn_timer < _debounce_timeout)
  {
    return (_btn_state);
  }

  bool isClosed = getContactsState();
  // состояние кнопки не изменилось с прошлого опроса
  if (isClosed == getFlag(FLAG_BIT))
  { // и не поднят флаг подавления дребезга
    if (!getFlag(DEBOUNCE_BIT))
    {
      if (!isClosed)
      { // кнопка находится в отжатом состоянии
        _btn_state = BTN_RELEASED;
        if (thisMls - dbl_timer > _dblclck_timeout)
        { // если период двойного клика закончился, проверить на виртуальный клик и сбросить флаг одиночного клика
          if (getFlag(VIRTUALCLICK_BIT) && getFlag(ONECLICK_BIT))
          {
            _btn_state = BTN_ONECLICK;
          }
          setFlag(ONECLICK_BIT, false);
          setFlag(LONGCLICK_BIT, false);
        }
      }
      else if (thisMls - btn_timer < _longclick_timeout && !getFlag(LONGCLICK_BIT))
      { // кнопка находится в нажатом состоянии, но время удержания еще не вышло, и события удержания еще не было
        _btn_state = BTN_PRESSED;
      }
      else // если кнопка удерживается нажатой дольше времени удержания, то дальше возможны варианты
      {    // если события удержания еще не было, то выдать его
        if (!getFlag(LONGCLICK_BIT))
        {
          if (_longclick_mode == LCM_CLICKSERIES)
          {
            btn_timer = thisMls;
          }
          setFlag(LONGCLICK_BIT, true);
          _btn_state = BTN_LONGCLICK;
        }
        else // если это уже не первое событие удержания, то дальше согласно выбранного режима
        {
          switch (_longclick_mode)
          {
          case LCM_ONLYONCE:
            _btn_state = BTN_PRESSED;
            break;
          case LCM_CLICKSERIES:
            if (thisMls - btn_timer >= _interval_of_serial)
            {
              btn_timer = thisMls;
              _btn_state = BTN_LONGCLICK;
            }
            else
            {
              _btn_state = BTN_PRESSED;
            }
            break;
          default:
            _btn_state = BTN_LONGCLICK;
            break;
          }
        }
        setFlag(ONECLICK_BIT, false);
      }
    }
  }
  // состояние кнопки изменилось с прошлого опроса
  else
  { // если задано подавление дребезга контактов
    if (_debounce_timeout > 0)
    { // если флаг подавления еще не поднят - поднять и больше ничего не делать
      if (!getFlag(DEBOUNCE_BIT))
      {
        btn_timer = thisMls;
        setFlag(DEBOUNCE_BIT, true);
        // и заодно сбросить переменную _btn_state, чтобы не выскакивали множественные события типа BTN_DOWN пока не истечет интервал антидребезга; исключение - состояние удержания кнопки в режиме непрерывного события
        if (!(_btn_state == BTN_LONGCLICK && _longclick_mode == LCM_CONTINUED))
        {
          _btn_state = isButtonClosed();
        }
      } // иначе, если поднят, и интервал вышел - установить состояние кнопки
      else if (thisMls - btn_timer >= _debounce_timeout)
      {
        btn_timer = thisMls;
        setBtnUpDown(isClosed, thisMls);
      }
    }
    else // если подавление вообще не задано, то сразу установить состояние кнопки
    {
      btn_timer = thisMls;
      setBtnUpDown(isClosed, thisMls);
    }
  }

  return (_btn_state);
}

uint8_t srButton::getLastState() { return (_btn_state); }

bool srButton::isButtonClosed() { return (getFlag(FLAG_BIT)); }

bool srButton::isSecondButtonPressed(srButton &_but, uint8_t btn_state)
{
  bool result = false;
  if (getLastState() == btn_state && _but.isButtonClosed())
  {
    result = true;
    resetButtonState();
    _but.resetButtonState();
  }
  return (result);
}

void srButton::resetButtonState()
{
  setFlag(ONECLICK_BIT, false);
  setFlag(LONGCLICK_BIT, false);
  // сброс _btn_state в зависимости от последнего состояния - либо нажата, либо отпущена
  _btn_state = isButtonClosed();
}

void srButton::setLongClickMode(uint8_t longclickmode)
{
  _longclick_mode = longclickmode;
  if (_longclick_mode == LCM_CLICKSERIES && _interval_of_serial == 0)
  {
    _interval_of_serial = 100;
  }
}

void srButton::setTimeoutOfLongClick(uint16_t new_timeout)
{
  _longclick_timeout = new_timeout;
}

// ==== end srButton=================================
