/* 
   ESP8266 Unified: Network + Motion Watcher + Notes Web UI
   ---------------------------------------------------------
   - Ping rotativo a hosts con notificaciones SMTP (UP/DOWN, arranque) 
   - Detección PIR con notificación por correo
   - Servidor web con UI de notas (LittleFS) + dashboard:
       * device info
       * hosts status
       * motion log
   - No librerías duplicadas; gestión básica de recursos (ArduinoJson para tasks)

   Ajustes: completa SSID, passwords, emails y APP password para SMTP.
*/

// ==================== LIBRERÍAS ====================
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <ESP8266Ping.h>

// ==================== VARIABLES GLOBALES ====================
bool firstBoot = true;

// ===== DATOS WIFI =====
const char* ssid     = "TU_WIFI";
const char* password = "TU_PASSWORD";

// ===== CONFIGURACIÓN CORREO =====
#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       587
#define AUTHOR_EMAIL    "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD"
#define RECIPIENT_EMAIL "DESTINO@gmail.com"

// ===== ARRAY DE HOSTS =====
struct Host {
  const char* ip;
  const char* name;
  bool isUp;
};

Host hosts[] = {
  {"192.168.1.1",   "ROUTER-DIGI",       false},
  {"192.168.1.75",  "PROXMOX",           false},
  {"192.168.1.129", "TV-SALÓN",          false},
  {"192.168.1.130", "PC-MILITAR-WIFI",   false},
  {"192.168.1.131", "PORTATIL-AIR",      false},
  {"192.168.1.134", "ANDROID",           false},
  {"192.168.1.136", "TV-HABITACIÓN",     false},
  {"192.168.1.137", "IPHONE",            false}
};
const int numHosts = sizeof(hosts) / sizeof(hosts[0]);

// ===== PIN DEL PIR =====
const int pirPin = D2;
unsigned long lastMotionEmail = 0;
const unsigned long motionDelay = 10000;

// ===== OBJETO SMTP =====
SMTPSession smtp;

// ===== OBJETO SERVIDOR WEB =====
ESP8266WebServer server(80);

// ===== VARIABLES PING =====
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000; // Ping cada 5 segundos
int currentHost = 0;

// ==================== HTML UI ====================
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
    li { margin: 5px 0; padding: 5px; background: #fff6; border: 1px solid #333; display: flex; align-items: center; }
    li button { font-weight: bold; margin-left: auto; color: black; border: none; padding: 5px; cursor:pointer; background-color:transparent; }
    .badge { background-color: black; font-size: 0.7em; padding: 2px 6px; border-radius: 12px;}
  </style>
  <link rel="icon" href="https://raw.githubusercontent.com/aptelliot/Electronics-Projects/main/bandera.png" type="image/png">
</head>
<body>
  <pre id="terminal"></pre>
  <input type="text" id="searchInput" placeholder="Buscar nota..." style="width:380px" oninput="filterTasks()">
  <br><br>
  <input type="text" id="taskInput" placeholder="Nueva nota" style="width:300px">
  <button onclick="addTask()">Agregar</button>
  <ul id="taskList" style="width: 60%;"></ul>
  <script>
    // JS embebido...
  </script>
</body>
</html>
)rawliteral";

// ==================== FUNCIONES BACKEND ====================

// --- Fecha/hora NTP ---
String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "SinHora";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

// --- Envío de correo ---
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

// --- Endpoints servidor ---
void handleRoot();
void handleGetTasks();
void handleAddTask();
void handleRemoveTask();
void handleToggleTask();
void handleInfo();

// ==================== SETUP ====================
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) delay(1000);
  Serial.println(WiFi.localIP());

  // Configurar NTP
  configTime(+2 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  LittleFS.begin();

  // Rutas servidor
  server.on("/", handleRoot);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/toggle", handleToggleTask);
  server.on("/info", handleInfo);

  server.begin();
  digitalWrite(LED_BUILTIN, LOW); // Encendido cuando ya está conectado
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  // --- Notificación de arranque ---
  if (firstBoot) {
    sendEmail("🔔 ESP8266 Encendido", "El ESP8266 Network + Motion Watcher se ha iniciado.");
    firstBoot = false;
  }

  // --- Ping rotativo de hosts ---
  if (currentMillis - lastPingTime >= pingInterval) {
    lastPingTime = currentMillis;
    Host &h = hosts[currentHost];

    bool pingResult = Ping.ping(h.ip, 3);
    static int failCount[20] = {0};
    static int okCount[20]   = {0};

    if (pingResult) {
      okCount[currentHost]++;
      failCount[currentHost] = 0;
    } else {
      failCount[currentHost]++;
      okCount[currentHost] = 0;
    }

    // DOWN
    if (failCount[currentHost] >= 3 && h.isUp) {
      h.isUp = false;
      String subject = "❌ " + String(h.name) + " DOWN";
      String message = String(h.name) + " (" + h.ip + ") está DOWN.";
      Serial.println(message);
      sendEmail(subject.c_str(), message.c_str());
    }

    // UP
    if (okCount[currentHost] >= 2 && !h.isUp) {
      h.isUp = true;
      String subject = "✅ " + String(h.name) + " UP";
      String message = String(h.name) + " (" + h.ip + ") está nuevamente UP.";
      Serial.println(message);
      sendEmail(subject.c_str(), message.c_str());
    }

    // Siguiente host
    currentHost++;
    if (currentHost >= numHosts) currentHost = 0;
  }
}
