#pragma once

#include <Arduino.h>

static const char config_page[] PROGMEM =
    R"(<!DOCTYPE html>
<html>
  <head>
    <meta content='text/html; charset=UTF-8' http-equiv='content-type' />
    <meta name='VIEWPORT' content='width=device-width, initial-scale=1.0' />
    <style>
      body {
        margin: 0;
        font-size: 16px;
        font-weight: 400;
        line-height: 1.5;
        color: #212529;
        text-align: left;
        background-color: #fff;
      }
      .tabs {
        font-size: 0;
        max-width: 400px;
        margin-left: auto;
        margin-right: auto;
      }
      .tabs > h3 {
        font-size: 18pt;
        text-align: center;
      }
      #rel_list {
        border: 2px solid #e0e0e0;
        padding: 10px 15px;
        font-size: 16px;
      }
      #rel_list > h4 {
        margin-left: 10px;
      }
      label {
        margin-left: 10px;
      }
      input[type='text'] {
        display: block;
        margin-bottom: 10px;
        margin-left: 10px;
        width: 300px;
      }
      #label {
        display: none;
      }
      .save_btn {
        margin: 20px 25px;
        width: 120px;
      }
      .btn_right {
        margin-left: 20px;
      }
    </style>
  </head>
  <body>
    <p id='label'></p>
    <div class='tabs'>
      <h3 id='h3'></h3>
      <div id='rel_list'>
        <label id='save_last_state' style='margin: 15px 10px; display: block'>
          <input type='checkbox' id='save_state' style='margin-left: 0px' />
          Сохранять состояние реле при перезагрузке</label
        >
        <label>Описание модуля</label>
        <input
          type='text'
          id='module'
          maxlength='64'
          placeholder='Описание модуля'
        />
        <h4 id='h4'></h4>
      </div>
      <input
        type='button'
        value='Назад'
        class='save_btn'
        onclick="window.open('/', '_self', false);"
      />
      <input
        type='button'
        id='btn_save'
        class='save_btn btn_right'
        value='Сохранить'
        onclick='sendData()'
        disabled
      />
    </div>
    <script>
      var dest;
      function sendData() {
        // сделать проверку длины идентификаторов не менее 6
        document.getElementById('btn_save').disabled = 'disabled';
        var values = [];
        var relays = document
          .getElementById('rel_list')
          .getElementsByClassName('relay');
        var descrs = document
          .getElementById('rel_list')
          .getElementsByClassName('descr');
        for (var i = 0, len = relays.length; i < len; ++i) {
          let rel = { name: relays[i].value, descr: descrs[i].value };
          values.push(rel);
        }
        var f_data = {
          for: dest,
          module: document.getElementById('module').value,
          save_state: document.getElementById('save_state').checked,
          relays: values,
        };
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/sr_setconfig', true);
        xhr.setRequestHeader('Content-Type', 'text/json');
        xhr.onreadystatechange = function () {
          document.body.innerHTML = this.responseText;
        };
        xhr.send(JSON.stringify(f_data));
      }
      function btnSaveEnabled() {
        document.getElementById('btn_save').removeAttribute('disabled');
      }
      function getConfig() {
        var c_lab = document.getElementById('label');
        var c_page = c_lab.innerHTML;
        c_lab.innerHTML = '';
        var request = new XMLHttpRequest();
        request.open('GET', c_page, true);
        request.onload = function () {
          if (request.readyState == 4 && request.status == 200) {
            var config_str = request.responseText;
            var doc = JSON.parse(config_str);
            dest = doc.for;
            document.getElementById('module').value = doc.module;
            document.getElementById('h3').innerHTML =
              dest == 'relay'
                ? 'Настройка модуля WiFi-реле'
                : 'Настройка модуля WiFi-выключателя';
            document.getElementById('h4').innerHTML =
              dest == 'relay'
                ? 'Сетевые идентификаторы реле'
                : 'Идентификаторы удаленных реле';
            var d = document.getElementById('rel_list');
            if (dest != 'relay') {
              document.getElementById('save_last_state').style.display = 'none';
            } else {
              doc.save_state == 1
                ? document
                    .getElementById('save_state')
                    .setAttribute('checked', 'checked')
                : document
                    .getElementById('save_state')
                    .removeAttribute('checked');
            }
            for (var i = 0; i < doc.relays.length; i++) {
              const lab = document.createElement('label');
              lab.innerHTML =
                dest == 'relay'
                  ? 'Реле №' + String(i + 1)
                  : 'Кнопка №' + String(i + 1);
              d.append(lab);
              const inp1 = document.createElement('input');
              inp1.type = 'text';
              inp1.maxLength = '16';
              inp1.className = 'relay';
              inp1.placeholder = 'Идентификатор реле';
              inp1.addEventListener('input', (event) => {
                inp1.value = inp1.value.replace(/[^a-zA-Z0-9_/-]/g, '');
                btnSaveEnabled();
              });

              inp1.value = doc.relays[i].name;
              d.append(inp1);
              const inp2 = document.createElement('input');
              inp2.type = 'text';
              inp2.maxLength = '64';
              inp2.className = 'descr';
              inp2.placeholder = 'Описание реле';
              if (dest == 'switch') {
                inp2.disabled = true;
              }
              inp2.addEventListener('input', (event) => btnSaveEnabled());
              inp2.value = doc.relays[i].descr;
              d.append(inp2);
            }
          }
        };
        request.send();
      }
      document
        .getElementById('save_state')
        .addEventListener('input', btnSaveEnabled);
      document
        .getElementById('module')
        .addEventListener('input', btnSaveEnabled);
      document.addEventListener('DOMContentLoaded', getConfig);
    </script>
  </body>
</html>
)";