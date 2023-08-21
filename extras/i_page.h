#pragma once

#include <Arduino.h>

static const char index_page[] PROGMEM =
    R"(<!DOCTYPE html>
<html lang='en'>
  <head>
    <meta content='text/html; charset=UTF-8' http-equiv='content-type' />
    <meta name='VIEWPORT' content='width=device-width, initial-scale=1' />
    <style>
      #main {
        max-width: 400px;
        margin-left: auto;
        margin-right: auto;
      }
      .switch-list {
        padding: 25px;
      }
      h3 {
        font-size: 18pt;
      }
      .switch-list > h3,
      .switch-list > h4 {
        text-align: center;
      }
      .cmn-toggle {
        position: absolute;
        margin-left: -9999px;
        visibility: hidden;
      }
      .cmn-toggle + label {
        display: block;
        position: relative;
        cursor: pointer;
        outline: none;
        user-select: none;
        padding: 2px;
        width: 120px;
        height: 60px;
        background-color: #dddddd;
        border-radius: 60px;
        transition: background 0.2s;
      }
      input.cmn-toggle + label:before,
      input.cmn-toggle + label:after {
        display: block;
        position: absolute;
        content: '';
      }
      input.cmn-toggle + label:before {
        top: 2px;
        left: 2px;
        bottom: 2px;
        right: 2px;
        background-color: #fff;
        border-radius: 60px;
        transition: background 0.2s;
      }
      input.cmn-toggle + label:after {
        top: 4px;
        left: 4px;
        bottom: 4px;
        width: 52px;
        background-color: #dddddd;
        border-radius: 52px;
        transition: margin 0.2s, background 0.2s;
      }
      input.cmn-toggle:checked + label {
        background-color: #8ce196;
      }
      input.cmn-toggle:checked + label:after {
        margin-left: 60px;
        background-color: #8ce196;
      }
      .switch {
        display: inline-block;
        height: 65px;
        margin-left: 50px;
        margin-right: auto;
        margin-top: 10px;
        margin-bottom: 15px;
      }
      .setting {
        text-align: right;
        color: black;
        position: fixed;
        display: inline-block;
        top: 95%;
        right: 10px;
        font-size: 12px;
      }
      #label {
        display: none;
      }
    </style>
  </head>
  <body>
    <p id='label'></p>
    <div id='main'>
      <div class='switch-list' id='switch-list'>
        <h3 id='h3'>Модуль WiFi-реле</h3>
        <h4 id='module'>Розетка у окна</h4>
      </div>
      <a href='/relay_config' class='setting'>Настройки модуля</a><br />
      <a href='/wifi_config' class='setting' style='top: 90%'>Настройки WiFi</a>
    </div>
    <script>
      var dest;
      function switchRelay(l_id) {
        let f_data = { relay: l_id }
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/relay_switch', true);
        xhr.setRequestHeader('Content-Type', 'text/json');
        xhr.onreadystatechange = function () {
          try {
            document.getElementById('switch-' + l_id).checked =
              this.responseText = 'on';
          } catch (error) {}
        };
        xhr.send(JSON.stringify(f_data));
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
            document.getElementById('module').innerHTML = doc.module;
            document.getElementById('h3').innerHTML =
              dest == 'relay'
                ? 'Модуль WiFi-реле - управление'
                : 'Модуль WiFi-выключателя - управление';
            var d = document.getElementById('switch-list');
            for (var i = 0; i < doc.relays.length; i++) {
              const d0 = document.createElement('div');
              const sp = document.createElement('span');
              sp.innerHTML = doc.relays[i].descr;
              sp.id = 'descr-' + i.toString();
              d0.append(sp);
              const b = document.createElement('br');
              d0.append(b);
              const d1 = document.createElement('div');
              d1.className = 'switch';
              const inp = document.createElement('input');
              inp.type = 'checkbox';
              inp.id = 'switch-' + i.toString();
              inp.className = 'cmn-toggle';
              inp.checked = doc.relays[i].last;
              d1.append(inp);
              const lab = document.createElement('label');
              lab.id = i.toString();
              lab.addEventListener('click', (event) => {
                switchRelay(event.target.id);
              });
              d1.append(lab);
              d0.append(d1);
              d.append(d0);
            }
          }
        };
        request.send();
      }
      function getRelayState() {
        var request = new XMLHttpRequest();
        request.open('GET', '/relay_getstate', true);
        request.onload = function () {
          if (request.readyState == 4 && request.status == 200) {
            var config_str = request.responseText;
            var doc = JSON.parse(config_str);
            for (var i = 0; i < doc.relays.length; i++) {
              try {
                document.getElementById('switch-' + i.toString()).checked =
                  doc.relays[i].last;
                document.getElementById('descr-' + i.toString()).innerHTML =
                  doc.relays[i].descr;
              } catch (error) {}
            }
          }
        };
        request.send();
      }
      setInterval(getRelayState, 500);
      document.addEventListener('DOMContentLoaded', getConfig);
    </script>
  </body>
</html>
)";

