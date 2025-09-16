/*
  ESP8266 Unified: Network + Motion Watcher + Notes Web UI
  - Ping rotativo a hosts con notificaciones SMTP (UP/DOWN, arranque)
  - Detección PIR con notificación por correo
  - Servidor web con UI de notas (LittleFS) + dashboard: device info, hosts status, motion log
  - No librerías duplicadas; gestión básica de recursos (ArduinoJson para tasks)
  
  Ajustes: completa SSID, passwords, emails y APP password para SMTP.
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <ESP8266Ping.h>
bool firstBoot = true;

//===== DATOS WIFI =====
const char* ssid = "DIGIFIBRA-GGhb";
const char* password = "U75xt69FueYG";

const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";

//=====CONFIGURACIÓN CORREO =====
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "isaacnavajas@gmail.com"
#define AUTHOR_PASSWORD "czxpwidbzrhjqhbt"
#define RECIPIENT_EMAIL "isaacnavajas@gmail.com"

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD"   // App password
#define RECIPIENT_EMAIL "DESTINO@gmail.com"
// ===== ARRAY DE HOSTS =====
struct Host {
  const char* ip;
  const char* name;
  bool isUp;   // Estado actual
};

Host hosts[] = {
  {"192.168.1.1", "ROUTER-DIGI", false},
  {"192.168.1.75", "PROXMOX", false},
  {"192.168.1.129", "TV-SALÓN", false},
  {"192.168.1.130", "PC-MILITAR-WIFI", false},
  {"192.168.1.131", "PORTATIL-AIR", false},
  {"192.168.1.134", "ANDROID", false},
  {"192.168.1.136", "TV-HABITACIÓN", false},
  {"192.168.1.137", "IPHONE", false}
};
const int numHosts = sizeof(hosts) / sizeof(hosts[0]);

// ===== PIN DEL PIR =====
const int pirPin = D2;
unsigned long lastMotionEmail = 0;
const unsigned long motionDelay = 10000;

// ===== OBJETO SMTP =====
SMTPSession smtp;
ESP8266WebServer server(80);

// ===== VARIABLES =====
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000; // Ping cada 5 segundos
int currentHost = 0;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Servidor de notas militar</title>
  <style>
    body { font-family: monospace; background-color:#37451d; padding: 1em; color:white }
    #terminal { white-space: pre-wrap; margin-bottom: 1em; }
    input, button { font-family: Arial; padding: 0.5em; margin-right: 0.5em; }
    ul { list-style-type: none; padding: 0; }
    li { margin: 5px 0; padding: 5px; background: #fff6; border: 1px solid #333; display: flex; align-items: center; display: flex; align-items: center; }
    li button { font-weight: bold; margin-left: auto; color: black; border: none; padding: 5px; cursor:pointer; background-color:transparent; }
    .badge { background-color: black; font-size: 0.7em; padding: 2px 6px; border-radius: 12px;}
  </style>
  <link rel="icon" href="https://raw.githubusercontent.com/aptelliot/Electronics-Projects/main/bandera.png" type="image/png">
</head>
<body>
  <pre id="terminal"></pre>
  <!-- Input de búsqueda -->
  <input type="text" id="searchInput" placeholder="Buscar nota..." style="width:380px" oninput="filterTasks()">
  <br><br>
  <!-- Input de nueva nota -->
  <input type="text" id="taskInput" placeholder="Nueva nota" style="width:300px">
  <button onclick="addTask()">Agregar</button>
  <ul id="taskList" style="width: 60%;"></ul>

  <script>
    function filterTasks() {
      const query = document.getElementById('searchInput').value.toLowerCase();
      const list = document.getElementById('taskList');
      const items = list.getElementsByTagName('li');

      for (let item of items) {
        const text = item.innerText.toLowerCase();
        item.style.display = text.includes(query) ? '' : 'none';
      }
    }

    async function showTerminalInfo() {
      const res = await fetch('/info');
      const data = await res.json();
      const terminal = document.getElementById('terminal');

      // Determinar color según Flash libre
      const flashFreeKB = data.flash_free / 1024;
      const flashColor = flashFreeKB <= 300 ? 'red' : 'yellow';

      const lines = [
        `Conectando a WiFi...`,
        `SSID: ${data.ssid}`,
        `IP Local: ${data.ip}`,
        `RSSI: ${data.rssi} dBm`,
        `Chip ID: ${data.chip_id}`,
        `Tamaño Flash: ${(data.flash_size/1024/1024).toFixed(1)} MB`,
        `Espacio para firmware ocupado: ${(data.flash_used/1024).toFixed(1)} KB`,
        `Espacio disponible para FS: <span style="color:${flashColor}">${flashFreeKB.toFixed(1)} KB</span>`,
        `RAM Libre: ${(data.free_heap/1024).toFixed(1)} KB de 80 KB`,
        `SDK: ${data.sdk_version}`,
        `Tiempo activo: ${(data.uptime_sec/60).toFixed(1)} min`,
        `Inicializando interfaz de tareas...`
      ];

      terminal.innerHTML = lines.join("<br>");
      loadTasks();
    }

    function loadTasks() {
      fetch("/tasks")
        .then(res => res.json())
        .then(tasks => {
          const list = document.getElementById('taskList');
          list.innerHTML = '';
          tasks.forEach((task, i) => {
            const li = document.createElement('li');
            li.innerHTML = `
              <input type="checkbox" ${task.done ? 'checked' : ''} onchange="toggleTask(${i}, this.checked)">
              <div style="margin-left:8px; flex-grow:1;">
                <span class="badge">${task.date || ''}</span>
                <br>
                <span style="text-decoration:${task.done ? 'line-through' : 'none'}">${task.text}</span>
              </div>
              <button onclick="removeTask(${i})">Borrar</button>
            `;
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

    function toggleTask(index, checked) {
      fetch(`/toggle?index=${index}&done=${checked}`).then(loadTasks);
    }

    window.onload = showTerminalInfo;
  </script>
</body>
</html>
)rawliteral";

// ================= FUNCIONES BACKEND =================

// Función para obtener fecha/hora con NTP
String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "SinHora";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

// ===== FUNCIONES =====
void sendEmail(const char* subject, const char* message) {
  SMTP_Message mail;
  mail.sender.name = "ESP8266 Watcher";
  mail.sender.email = AUTHOR_EMAIL;
  mail.subject = subject;
  mail.addRecipient("Admin", RECIPIENT_EMAIL);
  mail.text.content = message;

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  if (!smtp.connect(&session)) {
    Serial.println("Error conectando a SMTP");
    return;
  }

  if (!MailClient.sendMail(&smtp, &mail)) {
    Serial.println("Error enviando correo: " + smtp.errorReason());
  } else {
    Serial.println("Correo enviado correctamente: " + String(subject));
  }

  smtp.closeSession();
}

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

// Agrega tarea (al inicio de la lista)
void handleAddTask() {
  if (!server.hasArg("task")) {
    server.send(400, "text/plain", "Falta 'task'");
    return;
  }

  String newTask = server.arg("task");

  DynamicJsonDocument doc(4096);
  JsonArray arr;

  if (LittleFS.exists("/tasks.json")) {
    File file = LittleFS.open("/tasks.json", "r");
    deserializeJson(doc, file);
    file.close();
    arr = doc.as<JsonArray>();
  } else {
    arr = doc.to<JsonArray>();
  }

  // Crear nuevo array para invertir el orden
  DynamicJsonDocument newDoc(4096);
  JsonArray newArr = newDoc.to<JsonArray>();

  // Insertar la nueva tarea al principio
  JsonObject obj = newArr.createNestedObject();
  obj["text"] = newTask;
  obj["done"] = false;
  obj["date"] = getDateTime();   // Fecha y hora reales

  // Copiar las tareas existentes después
  for (JsonObject oldObj : arr) {
    JsonObject copy = newArr.createNestedObject();
    copy["text"] = oldObj["text"];
    copy["done"] = oldObj["done"];
    copy["date"] = oldObj["date"]; // conservar fechas previas
  }

  // Guardar en el archivo
  File file = LittleFS.open("/tasks.json", "w");
  if (file) {
    serializeJson(newDoc, file);
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

  DynamicJsonDocument doc(4096);
  File file = LittleFS.open("/tasks.json", "r");
  if (!file) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
  deserializeJson(doc, file);
  file.close();

  JsonArray arr = doc.as<JsonArray>();
  if (index >= 0 && index < arr.size()) {
    arr.remove(index);
  }

  file = LittleFS.open("/tasks.json", "w");
  serializeJson(doc, file);
  file.close();

  server.send(200, "text/plain", "OK");
}

// Cambia estado (toggle)
void handleToggleTask() {
  if (!server.hasArg("index") || !server.hasArg("done")) {
    server.send(400, "text/plain", "Falta 'index' o 'done'");
    return;
  }

  int index = server.arg("index").toInt();
  bool done = server.arg("done") == "true";

  DynamicJsonDocument doc(4096);
  File file = LittleFS.open("/tasks.json", "r");
  if (!file) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
  deserializeJson(doc, file);
  file.close();

  JsonArray arr = doc.as<JsonArray>();
  if (index >= 0 && index < arr.size()) {
    arr[index]["done"] = done;
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
  doc["flash_used"] = ESP.getSketchSize();
  doc["flash_free"] = ESP.getFreeSketchSpace();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sdk_version"] = ESP.getSdkVersion();
  doc["uptime_sec"] = millis() / 1000;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Apagado por defecto

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);
  Serial.println(WiFi.localIP());

  // Configurar NTP (cambiar zona horaria según tu país)
  configTime(+2 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  digitalWrite(LED_BUILTIN, LOW);   // Enciende LED mientras conecta
  delay(200);
  digitalWrite(LED_BUILTIN, HIGH);  // Apaga LED un momento
  delay(200);

  LittleFS.begin();

  server.on("/", handleRoot);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/toggle", handleToggleTask);
  server.on("/info", handleInfo);

  digitalWrite(LED_BUILTIN, LOW); // Encendido cuando ya está conectado
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  // --- PRIMER MENSAJE AL ENCENDER ---
  if (firstBoot) {
    sendEmail("🔔 ESP8266 Encendido", "El ESP8266 Network + Motion Watcher se ha iniciado.");
    firstBoot = false;
  }

  // --- PING ROTATIVO DE HOSTS ---
  if (currentMillis - lastPingTime >= pingInterval) {
    lastPingTime = currentMillis;

    Host &h = hosts[currentHost];
    bool pingResult = Ping.ping(h.ip, 3); // 3 intentos por host

    static int failCount[20] = {0}; // soporte hasta 20 hosts
    static int okCount[20] = {0};

    if (pingResult) {
      okCount[currentHost]++;
      failCount[currentHost] = 0;
    } else {
      failCount[currentHost]++;
      okCount[currentHost] = 0;
    }

    // Cambia a DOWN si falla 3 veces seguidas
    if (failCount[currentHost] >= 3 && h.isUp) {
      h.isUp = false;
      String subject = "❌ " + String(h.name) + " DOWN";
      String message = String(h.name) + " (" + h.ip + ") está DOWN.";
      Serial.println(message);
      sendEmail(subject.c_str(), message.c_str());
    }

    // Cambia a UP si responde 2 veces seguidas
    if (okCount[currentHost] >= 2 && !h.isUp) {
      h.isUp = true;
      String subject = "✅ " + String(h.name) + " UP";
      String message = String(h.name) + " (" + h.ip + ") está nuevamente UP.";
      Serial.println(message);
      sendEmail(subject.c_str(), message.c_str());
    }

    // Pasar al siguiente host
    currentHost++;
    if (currentHost >= numHosts) currentHost = 0;
  }
}
