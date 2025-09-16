/*
  ESP8266 Unified: Network + Motion Watcher + Notes Web UI
  - Ping rotativo a hosts con notificaciones SMTP (UP/DOWN, arranque)
  - Detección PIR con notificación por correo
  - Servidor web con UI de notas (LittleFS) + dashboard: device info, hosts status, motion log
  - No librerías duplicadas; gestión básica de recursos (ArduinoJson para tasks)
  
  Ajustes: completa SSID, passwords, emails y APP password para SMTP.
*/

#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>
#include <ESP8266Ping.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// =====================
// CONFIGURACIÓN RED
// =====================
const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";

// =====================
// CONFIGURACIÓN CORREO (GMail ejemplo)
// =====================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD"   // App password
#define RECIPIENT_EMAIL "DESTINO@gmail.com"

// =====================
// HOSTS A PINGEAR
// =====================
struct Host {
  const char* ip;
  const char* name;
  bool isUp;
  int failCount;
  int okCount;
};

Host hosts[] = {
  {"192.168.1.1", "ROUTER-DIGI", false, 0, 0},
  {"192.168.1.75", "PROXMOX", false, 0, 0},
  {"192.168.1.129", "TV-SALON", false, 0, 0},
  {"192.168.1.130", "PC-MILITAR-WIFI", false, 0, 0},
  {"192.168.1.131", "PORTATIL-AIR", false, 0, 0},
  {"192.168.1.134", "ANDROID", false, 0, 0},
  {"192.168.1.136", "TV-HABITACION", false, 0, 0},
  {"192.168.1.137", "IPHONE", false, 0, 0}
};
const int numHosts = sizeof(hosts) / sizeof(hosts[0]);

// =====================
// PIR
// =====================
const int pirPin = D2;
unsigned long lastMotionEmail = 0;
const unsigned long motionDelay = 10000; // 10s entre emails por movimiento
#define MOTION_LOG_MAX 10

// =====================
// TIMINGS y ESTADO
// =====================
bool firstBoot = true;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000; // cada 5s
int currentHost = 0;

// =====================
// SMTP y WEB
// =====================
SMTPSession smtp;
ESP8266WebServer server(80);

// Motion log (simple circular)
String motionLog[MOTION_LOG_MAX];
int motionLogIdx = 0;

// =====================
// HTML (interfaz combinada) - minimizada pero funcional
// =====================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>ESP8266 - Watcher & Notes</title>
<style>
  body{font-family:monospace;background:#162; color:#fff;padding:12px}
  .card{background:#0005;padding:10px;margin:8px 0;border-radius:6px}
  #taskList li{background:#fff2;color:#000;padding:6px;margin:6px 0;display:flex;align-items:center}
  input,button{padding:6px;margin:4px}
  .badge{background:#000;padding:2px 6px;border-radius:8px;color:#fff;font-size:0.8em}
  table{width:100%;border-collapse:collapse}
  td,th{padding:6px;border-bottom:1px solid #333}
</style>
</head>
<body>
<h3>ESP8266 - Network & Motion Watcher + Notes</h3>
<div class="card" id="terminal"></div>

<div class="card">
  <strong>Hosts status</strong>
  <table id="hostsTable"><thead><tr><th>Nombre</th><th>IP</th><th>Estado</th></tr></thead><tbody></tbody></table>
</div>

<div class="card">
  <strong>Motion Log</strong>
  <ul id="motionList"></ul>
</div>

<div class="card">
  <strong>Notas / Tareas</strong><br>
  <input id="taskInput" placeholder="Nueva nota" style="width:60%">
  <button onclick="addTask()">Agregar</button>
  <ul id="taskList"></ul>
</div>

<script>
async function loadInfo(){
  const res = await fetch('/info'); const info = await res.json();
  const term = document.getElementById('terminal');
  term.innerHTML = `
    IP: ${info.ip} | SSID: ${info.ssid} | RSSI: ${info.rssi} dBm<br>
    Flash free: ${(info.flash_free/1024).toFixed(1)} KB | Free heap: ${(info.free_heap/1024).toFixed(1)} KB<br>
    Uptime: ${(info.uptime_sec/60).toFixed(1)} min | SDK: ${info.sdk_version}
  `;
  loadHosts();
  loadMotion();
  loadTasks();
}

async function loadHosts(){
  const res = await fetch('/hosts');
  const data = await res.json();
  const tbody = document.querySelector('#hostsTable tbody');
  tbody.innerHTML = '';
  data.forEach(h=>{
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${h.name}</td><td>${h.ip}</td><td>${h.isUp?'<span class="badge">UP</span>':'<span class="badge">DOWN</span>'}</td>`;
    tbody.appendChild(tr);
  });
}

async function loadMotion(){
  const res = await fetch('/motion');
  const arr = await res.json();
  const ul = document.getElementById('motionList');
  ul.innerHTML = '';
  arr.forEach(s=>{
    const li = document.createElement('li');
    li.textContent = s;
    ul.appendChild(li);
  });
}

function loadTasks(){
  fetch('/tasks').then(r=>r.json()).then(tasks=>{
    const list = document.getElementById('taskList');
    list.innerHTML = '';
    tasks.forEach((t,i)=>{
      const li = document.createElement('li');
      li.innerHTML = `<input type="checkbox" ${t.done? 'checked':''} onchange="toggleTask(${i},this.checked)"><div style="margin-left:8px;flex:1"><small>${t.date||''}</small><br>${t.text}</div><button onclick="removeTask(${i})">Borrar</button>`;
      list.appendChild(li);
    });
  });
}

function addTask(){
  const v = document.getElementById('taskInput').value.trim();
  if(!v) return alert('Escribe una tarea');
  fetch('/add?task='+encodeURIComponent(v)).then(()=>{document.getElementById('taskInput').value='';loadTasks();});
}

function removeTask(i){ fetch('/remove?index='+i).then(loadTasks); }
function toggleTask(i,d){ fetch('/toggle?index='+i+'&done='+d).then(loadTasks); }

window.onload = loadInfo;
setInterval(loadInfo, 5000);
</script>
</body>
</html>
)rawliteral";

// =====================
// UTIL: enviar EMAIL (reutilizable)
// =====================
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
    Serial.println("SMTP connect failed");
    return;
  }

  if (!MailClient.sendMail(&smtp, &mail)) {
    Serial.println(String("Error enviando correo: ") + smtp.errorReason());
  } else {
    Serial.println(String("Correo enviado: ") + subject);
  }

  smtp.closeSession();
}

// =====================
// TASKS (LittleFS + ArduinoJson)
// =====================
const char* TASKS_PATH = "/tasks.json";
const size_t TASKS_DOC_SIZE = 6 * 1024; // espacio para tareas

String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "SinHora";
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buf);
}

String readFileString(const char* path) {
  if (!LittleFS.exists(path)) return String("[]");
  File f = LittleFS.open(path, "r");
  if (!f) return String("[]");
  String s = f.readString(); f.close();
  return s;
}

bool saveJsonToFile(const char* path, JsonDocument &doc) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

// ========== HTTP Handlers ==========

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// /info -> dispositivo
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
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// /hosts -> estado actual de hosts
void handleHosts() {
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (int i=0;i<numHosts;i++){
    JsonObject o = arr.createNestedObject();
    o["ip"] = hosts[i].ip;
    o["name"] = hosts[i].name;
    o["isUp"] = hosts[i].isUp;
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// /motion -> array con logs recientes
void handleMotion() {
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (int i=0;i<MOTION_LOG_MAX;i++){
    int idx = (motionLogIdx + i) % MOTION_LOG_MAX;
    if (motionLog[idx].length()) arr.add(motionLog[idx]);
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// /tasks -> devuelve tasks.json
void handleGetTasks() {
  String content = readFileString(TASKS_PATH);
  server.send(200, "application/json", content);
}

// /add?task=texto -> agrega al inicio
void handleAddTask() {
  if (!server.hasArg("task")) { server.send(400, "text/plain", "Falta 'task'"); return; }
  String newTask = server.arg("task");

  DynamicJsonDocument doc(TASKS_DOC_SIZE);
  // cargar existentes si hay
  if (LittleFS.exists(TASKS_PATH)) {
    File f = LittleFS.open(TASKS_PATH, "r");
    if (f) { DeserializationError err = deserializeJson(doc, f); f.close(); if (err) doc.clear(); }
  }
  JsonArray oldArr = doc.as<JsonArray>();

  DynamicJsonDocument newDoc(TASKS_DOC_SIZE);
  JsonArray newArr = newDoc.to<JsonArray>();

  // nueva tarea al principio
  JsonObject o = newArr.createNestedObject();
  o["text"] = newTask;
  o["done"] = false;
  o["date"] = getDateTime();

  // copiar viejas
  for (JsonObject old : oldArr) {
    JsonObject c = newArr.createNestedObject();
    c["text"] = old["text"];
    c["done"] = old["done"];
    c["date"] = old["date"];
  }

  if (saveJsonToFile(TASKS_PATH, newDoc)) server.send(200, "text/plain", "OK");
  else server.send(500, "text/plain", "No se pudo guardar");
}

// /remove?index=N
void handleRemoveTask() {
  if (!server.hasArg("index")) { server.send(400, "text/plain", "Falta 'index'"); return; }
  int idx = server.arg("index").toInt();

  if (!LittleFS.exists(TASKS_PATH)) { server.send(404, "text/plain", "Archivo no encontrado"); return; }
  DynamicJsonDocument doc(TASKS_DOC_SIZE);
  File f = LittleFS.open(TASKS_PATH, "r");
  if (!f) { server.send(500, "text/plain", "Error abrir"); return; }
  deserializeJson(doc, f); f.close();

  JsonArray arr = doc.as<JsonArray>();
  if (idx >= 0 && idx < arr.size()) arr.remove(idx);

  File fout = LittleFS.open(TASKS_PATH, "w");
  if (fout) { serializeJson(doc, fout); fout.close(); server.send(200, "text/plain", "OK"); }
  else server.send(500, "text/plain", "Error guardar");
}

// /toggle?index=N&done=true/false
void handleToggleTask() {
  if (!server.hasArg("index") || !server.hasArg("done")) { server.send(400, "text/plain", "Falta 'index' o 'done'"); return; }
  int idx = server.arg("index").toInt();
  bool done = server.arg("done") == "true";

  if (!LittleFS.exists(TASKS_PATH)) { server.send(404, "text/plain", "Archivo no encontrado"); return; }
  DynamicJsonDocument doc(TASKS_DOC_SIZE);
  File f = LittleFS.open(TASKS_PATH, "r");
  if (!f) { server.send(500, "text/plain", "Error abrir"); return; }
  deserializeJson(doc, f); f.close();

  JsonArray arr = doc.as<JsonArray>();
  if (idx >= 0 && idx < arr.size()) arr[idx]["done"] = done;

  File fout = LittleFS.open(TASKS_PATH, "w");
  if (fout) { serializeJson(doc, fout); fout.close(); server.send(200, "text/plain", "OK"); }
  else server.send(500, "text/plain", "Error guardar");
}

// =====================
// LOGICA DE PING + PIR
// =====================

void logMotion(const String &s) {
  motionLog[motionLogIdx++] = s;
  if (motionLogIdx >= MOTION_LOG_MAX) motionLogIdx = 0;
}

void checkPings(unsigned long currentMillis) {
  if (currentMillis - lastPingTime < pingInterval) return;
  lastPingTime = currentMillis;

  Host &h = hosts[currentHost];
  bool pingResult = Ping.ping(h.ip, 3);

  if (pingResult) {
    h.okCount++;
    h.failCount = 0;
  } else {
    h.failCount++;
    h.okCount = 0;
  }

  // DOWN si falla 3 veces consecutivas
  if (h.failCount >= 3 && h.isUp) {
    h.isUp = false;
    String subject = "❌ " + String(h.name) + " DOWN";
    String message = String(h.name) + " (" ) + h.ip + ") está DOWN.";
    Serial.println(message);
    sendEmail(subject.c_str(), message.c_str());
    logMotion(message); // usar motionLog para eventos (genérico)
  }

  // UP si responde 2 veces seguidas
  if (h.okCount >= 2 && !h.isUp) {
    h.isUp = true;
    String subject = "✅ " + String(h.name) + " UP";
    String message = String(h.name) + " (" ) + h.ip + ") está UP nuevamente.";
    Serial.println(message);
    sendEmail(subject.c_str(), message.c_str());
    logMotion(message);
  }

  currentHost++;
  if (currentHost >= numHosts) currentHost = 0;
}

void checkPIR(unsigned long currentMillis) {
  bool motion = digitalRead(pirPin) == HIGH;
  if (motion && currentMillis - lastMotionEmail >= motionDelay) {
    String s = "Movimiento detectado en PIR - " + getDateTime();
    Serial.println(s);
    sendEmail("🚨 Movimiento detectado", s.c_str());
    logMotion(s);
    lastMotionEmail = currentMillis;
  }
}

// =====================
// SETUP + LOOP
// =====================
void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS no montado!");
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
    if (millis() - start > 20000) break; // timeout 20s (no bloquear indefinidamente)
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi no conectado");
  }

  // NTP
  configTime(+2 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // HTTP endpoints
  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/hosts", handleHosts);
  server.on("/motion", handleMotion);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/toggle", handleToggleTask);
  server.begin();

  digitalWrite(LED_BUILTIN, LOW); // ON

  // primer email de arranque (espera IP)
  if (WiFi.status() == WL_CONNECTED && firstBoot) {
    sendEmail("🔔 ESP8266 Encendido", "El ESP8266 Network + Motion Watcher se ha iniciado.");
    firstBoot = false;
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // HTTP
  server.handleClient();

  // Pings
  checkPings(currentMillis);

  // PIR
  checkPIR(currentMillis);
}
