#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <time.h>

const char* ssid = "SSID_WIFI";
const char* password = "PASSWORD_WIFI";

ESP8266WebServer server(80);

// Configura SMTP
#define SMTP_HOST "smtp.gmail.com"                               // SMTP de gmail
#define SMTP_PORT 465
#define AUTHOR_EMAIL "CORREO_QUE_ENVIA@gmail.com"                // correo con el que envío el mensaje
#define AUTHOR_PASSWORD "APP_PASSWORD_GMAIL"                     // se guarda sin espacios. Se consigue desde security con la doble autentificación activada en app password
#define RECIPIENT_EMAIL "prueba1@gmail.com,prueba2@gmail.com"    // posibilidad de varios correos que reciben los mensajes

SMTPSession smtp;

unsigned long lastSendTime = 0;
const unsigned long interval = 24UL * 60 * 60 * 1000; // 24h

const int targetHour = 4;       // Hora deseada (24h)
const int targetMinute = 44;    // Minuto deseado
bool alreadySentToday = false;  // asegura de que solo se envíe un mensaje una vez al día

void sendFirstPendingTask() {
  if (!LittleFS.exists("/tasks.json")) return;

  File file = LittleFS.open("/tasks.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();

  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject task : arr) {
    if (!task["enviado"].as<bool>()) {
      String mensaje = task["texto"].as<String>();

      SMTP_Message message;
      message.sender.name = "🤖ESP8266[CTp4i]: 😍 De Isaac para Paula";
      message.sender.email = AUTHOR_EMAIL;
      message.subject = "💌 Mensaje del día";
      // recorro todos los correos
      String recipients = String(RECIPIENT_EMAIL);
      int startIndex = 0;
      int endIndex = recipients.indexOf(',');

      while (endIndex != -1) {
        String email = recipients.substring(startIndex, endIndex);
        message.addRecipient("Destinatario", email);
        startIndex = endIndex + 1;
        endIndex = recipients.indexOf(',', startIndex);
      }

      message.addRecipient("Destinatario", recipients.substring(startIndex));
      message.text.content = mensaje;
      message.text.charSet = "utf-8";
      message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

      smtp.debug(Serial);

      ESP_Mail_Session session;
      session.server.host_name = SMTP_HOST;
      session.server.port = SMTP_PORT;
      session.login.email = AUTHOR_EMAIL;
      session.login.password = AUTHOR_PASSWORD;
      session.login.user_domain = "";
      session.time.ntp_server = "pool.ntp.org";

      if (!smtp.connect(&session)) return;
      if (!MailClient.sendMail(&smtp, &message)) return;

      task["enviado"] = true;
      File out = LittleFS.open("/tasks.json", "w");
      serializeJson(doc, out);
      out.close();
      return;
    }
  }
}

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>🤖 CTp4i</title>
  <style>
    body { font-family: monospace; background-color:#ffee00; padding: 1em; }
    #terminal { white-space: pre-wrap; margin-bottom: 1em; }
    input, button { font-family: Arial; padding: 0.5em; margin-right: 0.5em; }
    ul { list-style-type: none; padding: 0; }
    li button { font-weight: bold; margin-right: 10px; color: #ff00f7; border: none; padding: 5px; cursor:pointer; background-color:#ffee0000; }
  </style>
</head>
<body>
  <pre id="terminal"></pre>

  <h3>⏰ Envío Automático:</h3>
  <label>
    <input type="checkbox" id="autoSendToggle" onchange="toggleAutoSend(this.checked)">
    Activar envío automático de mensajes diarios
  </label>
  <br><br>
  <textarea rows="10" id="taskInput" placeholder="Nuevo mensaje de isaaclovepaula@gmail.com" style="width:500px; padding:2px;"></textarea>
  <br><br>
  <button onclick="addTask()" style="width:507px">Programar mensaje</button>

  <ul id="taskList"></ul>
  <br>
  <footer>
  <hr><h3>Backup</h3>
  <a href="/download" download><button>📥 Descargar download.json</button></a>
  <br><br>

  <h3>Restaurar</h3>
  <form id="uploadForm" method="POST" enctype="multipart/form-data">
    <input type="file" name="upload" accept=".json" id="fileInput" onchange="toggleUploadButton()">
    <br>
    <button type="submit" id="uploadButton" disabled>📤 Subir y reemplazar</button>

  </form>
  </footer>

<script>
  async function toggleAutoSend(enabled) {
    await fetch(`/toggleAutoSend?enable=${enabled ? '1' : '0'}`);
  }

  async function loadAutoSendState() {
    const res = await fetch("/getAutoSend");
    const data = await res.json();
    document.getElementById('autoSendToggle').checked = data.enabled;
  }

  window.onload = async () => {
    await showTerminalInfo();
    await loadAutoSendState();
  };

  function toggleUploadButton() {
    const fileInput = document.getElementById('fileInput');
    const uploadButton = document.getElementById('uploadButton');
    uploadButton.disabled = !fileInput.files.length;
  }

  document.getElementById('uploadForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    const formData = new FormData(this);
    const res = await fetch('/upload', {
      method: 'POST',
      body: formData
    });
    const txt = await res.text();
    alert(txt);
    loadTasks();
  });
</script>

<script>
  function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    let result = '';
    if (days > 0) result += `${days}d `;
    if (hours > 0 || days > 0) result += `${hours}h `;
    if (minutes > 0 || hours > 0 || days > 0) result += `${minutes}m `;
    result += `${secs}s`;
    return result.trim();
  }
  
  async function showTerminalInfo() {
    const res = await fetch('/info');
    const data = await res.json();
    const terminal = document.getElementById('terminal');

    const percent = (data.fs_used / data.fs_total) * 100;
    const color = percent > 80 ? '#ff3d00' : percent > 50 ? '#ffc107' : '#4caf50';

    const lines = [
      `Conectando a WiFi...`,
      `SSID: ${data.ssid}`,
      `IP Local: ${data.ip}`,
      `RSSI: ${data.rssi} dBm`,
      `Chip ID: ${data.chip_id}`,
      `Tamaño Flash: ${data.flash_size} bytes`,
      `RAM Libre: ${data.free_heap} bytes`,
      `SDK: ${data.sdk_version}`,
      `Espacio total (FS): ${data.fs_total} bytes`,
      `Espacio usado (FS): ${data.fs_used} bytes`,
      `Espacio libre (FS): ${data.fs_free} bytes`,
      (() => {
        const usedKB = (data.fs_used / 1024).toFixed(1);
        const totalKB = (data.fs_total / 1024).toFixed(1);
        const percentage = (data.fs_used / data.fs_total * 100).toFixed(1);

        const totalBars = 25;
        const filledBars = Math.round((percentage / 100) * totalBars);
        const emptyBars = totalBars - filledBars;

        const bar = `[${'█'.repeat(filledBars)}${'.'.repeat(emptyBars)}] (${percentage}%)`;
        return `Almacenamiento: ${usedKB} KB / ${totalKB} KB\n${bar}`;
      })(),
      `Tiempo activo: ${formatUptime(data.uptime_sec)}`,
      `Inicializando interfaz de tareas...`
    ];

    let i = 0;
    function typeNextLine() {
      if (i >= lines.length) {
        loadTasks();
        return;
      }
      terminal.innerHTML += lines[i] + "<br>";
      i++;
      setTimeout(typeNextLine, 300);
    }

    typeNextLine();
  }


  function escapeHtml(text) {
    return text
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#039;");
  }

  function formatFecha(fechaStr) {
    const fecha = new Date(fechaStr);
    return fecha.toLocaleString("es-ES", { hour12: false });
  }

  function loadTasks() {
    fetch("/tasks").then(res => res.json()).then(tasks => {
      const list = document.getElementById('taskList');
      list.innerHTML = '';
      tasks.forEach((task, i) => {
        const textoSeguro = escapeHtml(task.texto).replace(/\n/g, "<br>") + "<br>";
        const fecha = task.fecha ? formatFecha(task.fecha) : "Sin fecha";
        const enviadoLabel = task.enviado
          ? `<span style="background-color:#10ff00;color:#fff;padding:2px 5px;border-radius:5px;font-weight:bold;">Enviado</span>`
          : "";

        const li = document.createElement('li');
        li.innerHTML = `<button onclick="removeTask(${i})">Borrar</button> 
                <button onclick="forceSend(${i})">Forzar Enviar</button> 
                ${enviadoLabel} 
                <strong>${fecha}</strong><br>
                ${textoSeguro}`;
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

  function forceSend(index) {
    fetch(`/forceSend?index=${index}`).then(loadTasks);
  }
</script>

</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", htmlPage); }

void handleGetTasks() {
  if (!LittleFS.exists("/tasks.json")) { server.send(200, "application/json", "[]"); return; }
  File file = LittleFS.open("/tasks.json", "r");
  String content = file.readString(); file.close();
  server.send(200, "application/json", content);
}

void handleAddTask() {
  if (!server.hasArg("task")) return server.send(400, "text/plain", "Falta 'task'");

  String newTask = server.arg("task");
  DynamicJsonDocument doc(1024);
  JsonArray arr;

  if (LittleFS.exists("/tasks.json")) {
    File file = LittleFS.open("/tasks.json", "r");
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (!error && doc.is<JsonArray>()) arr = doc.as<JsonArray>();
    else { doc.clear(); arr = doc.to<JsonArray>(); }
  } else arr = doc.to<JsonArray>();

  JsonObject obj = arr.createNestedObject();
  obj["texto"] = newTask;
  obj["enviado"] = false;

  // Añadir fecha en formato ISO
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char fecha[25];
  strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S", t);
  obj["fecha"] = fecha;

  File file = LittleFS.open("/tasks.json", "w");
  serializeJson(doc, file);
  file.close();
  server.send(200, "text/plain", "OK");
}

void handleRemoveTask() {
  if (!server.hasArg("index")) return server.send(400, "text/plain", "Falta 'index'");
  int index = server.arg("index").toInt();

  File file = LittleFS.open("/tasks.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();
  JsonArray arr = doc.as<JsonArray>();

  if (index >= 0 && index < arr.size()) arr.remove(index);

  file = LittleFS.open("/tasks.json", "w");
  serializeJson(doc, file);
  file.close();
  server.send(200, "text/plain", "OK");
}

void handleForceSend() {
  if (!server.hasArg("index")) return server.send(400, "text/plain", "Falta 'index'");
  int index = server.arg("index").toInt();

  File file = LittleFS.open("/tasks.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);
  file.close();
  JsonArray arr = doc.as<JsonArray>();

  if (index >= 0 && index < arr.size()) {
    JsonObject task = arr[index];
    if (!task["enviado"].as<bool>()) {
      // Crea un objeto de mensaje SMTP
      SMTP_Message message;
      message.sender.name = "🤖ESP8266[CTp4i]: 😍 De Isaac para Paula";
      message.sender.email = AUTHOR_EMAIL;
      message.subject = "Tarea forzada";
      message.text.content = task["texto"].as<String>();
      message.text.charSet = "utf-8";
      message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

      // Dividir la cadena de correos en una lista
      String recipients = String(RECIPIENT_EMAIL);
      int startIndex = 0;
      int endIndex = recipients.indexOf(',');

      while (endIndex != -1) {
        String email = recipients.substring(startIndex, endIndex);
        message.addRecipient("Destinatario", email);  // Agregar cada destinatario por separado
        startIndex = endIndex + 1;
        endIndex = recipients.indexOf(',', startIndex);
      }

      // Agregar el último destinatario
      message.addRecipient("Destinatario", recipients.substring(startIndex));


      smtp.debug(Serial);

      ESP_Mail_Session session;
      session.server.host_name = SMTP_HOST;
      session.server.port = SMTP_PORT;
      session.login.email = AUTHOR_EMAIL;
      session.login.password = AUTHOR_PASSWORD;
      session.login.user_domain = "";
      session.time.ntp_server = "pool.ntp.org";

      if (smtp.connect(&session)) {
        if (MailClient.sendMail(&smtp, &message)) {
          task["enviado"] = true;
        }
      }
    }
  }

  file = LittleFS.open("/tasks.json", "w");
  serializeJson(doc, file);
  file.close();
  server.send(200, "text/plain", "Forzado OK");
}

void handleDownload() {
  if (!LittleFS.exists("/tasks.json")) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }
  File file = LittleFS.open("/tasks.json", "r");
  server.streamFile(file, "application/json");
  file.close();
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    LittleFS.remove("/tasks.json");
    File f = LittleFS.open("/tasks.json", "w");
    f.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File f = LittleFS.open("/tasks.json", "a");
    f.write(upload.buf, upload.currentSize);
    f.close();
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "Archivo subido correctamente");
  }
}

void handleInfo() {
  FSInfo fs_info;
  LittleFS.info(fs_info);  // Obtiene info del sistema de archivos

  DynamicJsonDocument doc(512);
  doc["ssid"] = ssid;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["chip_id"] = ESP.getChipId();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sdk_version"] = ESP.getSdkVersion();
  doc["uptime_sec"] = millis() / 1000;
  doc["fs_total"] = fs_info.totalBytes;
  doc["fs_used"] = fs_info.usedBytes;
  doc["fs_free"] = fs_info.totalBytes - fs_info.usedBytes;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleToggleAutoSend() {
  bool enable = server.arg("enable") == "1";
  File file = LittleFS.open("/auto_send.txt", "w");
  file.print(enable ? "1" : "0");
  file.close();
  server.send(200, "text/plain", "OK");
}

void handleGetAutoSend() {
  bool enabled = false;
  if (LittleFS.exists("/auto_send.txt")) {
    File file = LittleFS.open("/auto_send.txt", "r");
    char ch = file.read();
    enabled = (ch == '1');
    file.close();
  }
  server.send(200, "application/json", String("{\"enabled\":") + (enabled ? "true" : "false") + "}");
}

bool isAutoSendEnabled() {
  if (LittleFS.exists("/auto_send.txt")) {
    File file = LittleFS.open("/auto_send.txt", "r");
    char ch = file.read();
    file.close();
    return ch == '1';
  }
  return false; // Por defecto desactivado
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);

  LittleFS.begin();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); // Zona horaria de España con horario de verano
  tzset();
  while (time(nullptr) < 1600000000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" OK");

  lastSendTime = millis();

  server.on("/", handleRoot);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/info", handleInfo);
  server.on("/forceSend", handleForceSend);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/upload", HTTP_POST, []() { server.send(200); }, handleUpload);
  server.on("/toggleAutoSend", handleToggleAutoSend);
  server.on("/getAutoSend", handleGetAutoSend);

  server.begin();
}

void loop() {
  server.handleClient();
  
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  int h = t->tm_hour;
  int m = t->tm_min;

  if (!alreadySentToday && h == targetHour && (m >= targetMinute && m < targetMinute + 2)) {
    if (isAutoSendEnabled()) {
      Serial.println("Envío automático activado. Enviando mensaje programado...");
      sendFirstPendingTask();
    } else {
      Serial.println("Envío automático DESACTIVADO. No se envía mensaje.");
    }
    alreadySentToday = true;
  }

  // Resetear bandera al cambiar de día
  if (h == 0 && m == 0) {
      alreadySentToday = false;
      Serial.println("Reseteando el estado de envío para el nuevo día.");
  }
}
