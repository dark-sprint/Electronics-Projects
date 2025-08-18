#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* ssid = "SSID_WIFI";
const char* password = "CONTRASEÑA_WIFI";

ESP8266WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>🤖 LTa7i</title>
  <style>
    body { font-family: monospace; background-color:#ffee00; padding: 1em; }
    #terminal { white-space: pre-wrap; margin-bottom: 1em; }
    input, button { font-family: Arial; padding: 0.5em; margin-right: 0.5em; }
    ul { list-style-type: none; padding: 0; }
    /* li { margin: 5px 0; padding: 10px; background: #222; border: 1px solid #0f0; display: flex; align-items: center; } */
    li button { font-weight: bold; margin-right: 10px; color: #ff00f7; border: none; padding: 5px; cursor:pointer; background-color:#ffee0000; }
  </style>
</head>
<body>
  <pre id="terminal"></pre>
  <input type="text" id="taskInput" placeholder="Nueva tarea" style="width:500px">
  <button onclick="addTask()">Agregar</button>
  <ul id="taskList"></ul>

  <script>
    async function showTerminalInfo() {
      const res = await fetch('/info');
      const data = await res.json();
      const terminal = document.getElementById('terminal');

      const lines = [
        `Conectando a WiFi...`,
        `SSID: ${data.ssid}`,
        `IP Local: ${data.ip}`,
        `RSSI: ${data.rssi} dBm`,
        `Chip ID: ${data.chip_id}`,
        `Tamaño Flash: ${data.flash_size} bytes`,
        `RAM Libre: ${data.free_heap} bytes`,
        `SDK: ${data.sdk_version}`,
        `Tiempo activo: ${data.uptime_sec} seg`,
        `Inicializando interfaz de tareas...`
      ];

      let i = 0;
      function typeNextLine() {
        if (i >= lines.length) {
          loadTasks(); // Cargar tareas tras animación
          return;
        }
        terminal.innerHTML += lines[i] + "<br>";
        i++;
        setTimeout(typeNextLine, 500);
      }

      typeNextLine();
    }

    function loadTasks() {
      fetch("/tasks")
        .then(res => res.json())
        .then(tasks => {
          const list = document.getElementById('taskList');
          list.innerHTML = '';
          tasks.forEach((task, i) => {
            const li = document.createElement('li');
            li.innerHTML = `<button onclick="removeTask(${i})">Borrar</button> ${task}`;
            list.appendChild(li);
          });
        });
    }

    function addTask() {
      const input = document.getElementById('taskInput');
      const text = input.value.trim();
      if (text === '') return alert("Escribe una tarea");
      fetch(`/add?task=${encodeURIComponent(text)}`).then(loadTasks);
      input.value = '';
    }

    function removeTask(index) {
      fetch(`/remove?index=${index}`).then(loadTasks);
    }

    window.onload = showTerminalInfo;
  </script>
</body>
</html>
)rawliteral";

// Página principal
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Devuelve tareas
void handleGetTasks() {
  if (!LittleFS.exists("/tasks.json")) {
    server.send(200, "application/json", "[]");
    return;
  }

  File file = LittleFS.open("/tasks.json", "r");
  if (!file) {
    server.send(500, "text/plain", "Error al abrir el archivo");
    return;
  }

  String content = file.readString();
  file.close();
  server.send(200, "application/json", content);
}

// Agrega tarea
void handleAddTask() {
  if (!server.hasArg("task")) {
    server.send(400, "text/plain", "Falta 'task'");
    return;
  }

  String newTask = server.arg("task");

  DynamicJsonDocument doc(1024);
  JsonArray arr;

  if (LittleFS.exists("/tasks.json")) {
    File file = LittleFS.open("/tasks.json", "r");
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (!error && doc.is<JsonArray>()) {
      arr = doc.as<JsonArray>();
    } else {
      doc.clear();
      arr = doc.to<JsonArray>();
    }
  } else {
    arr = doc.to<JsonArray>();
  }

  arr.add(newTask);

  File file = LittleFS.open("/tasks.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(500, "text/plain", "No se pudo guardar");
  }
}

// Borra tarea
void handleRemoveTask() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Falta 'index'");
    return;
  }

  int index = server.arg("index").toInt();

  DynamicJsonDocument doc(1024);
  File file = LittleFS.open("/tasks.json", "r");
  if (!file) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonArray>()) {
    server.send(500, "text/plain", "Error de formato JSON");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (index >= 0 && index < arr.size()) {
    arr.remove(index);
  }

  file = LittleFS.open("/tasks.json", "w");
  serializeJson(doc, file);
  file.close();

  server.send(200, "text/plain", "OK");
}

// Devuelve info del sistema
void handleInfo() {
  DynamicJsonDocument doc(512);
  doc["ssid"] = ssid;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["chip_id"] = ESP.getChipId();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sdk_version"] = ESP.getSdkVersion();
  doc["uptime_sec"] = millis() / 1000;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);
  Serial.println(WiFi.localIP());

  LittleFS.begin();

  server.on("/", handleRoot);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/info", handleInfo);

  server.begin();
}

void loop() {
  server.handleClient();
}
