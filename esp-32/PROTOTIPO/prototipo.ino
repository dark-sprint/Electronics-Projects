/*
  ESP32 Unified: Network + Motion Watcher + Notes Web UI
  - Ping rotativo a hosts con notificaciones SMTP (UP/DOWN, arranque)
  - Detección PIR con notificación por correo
  - Servidor web con UI de notas (LittleFS) + dashboard: device info, hosts status, motion log
  - Optimizado para ESP32 con FreeRTOS Tasks
*/

#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <Ping.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// =====================
// CONFIGURACIÓN RED
// =====================
const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";

// =====================
// CONFIGURACIÓN CORREO
// =====================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD"
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
const int pirPin = 2; // GPIO2, equivalente a D2
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
WebServer server(80);

// Motion log circular
String motionLog[MOTION_LOG_MAX];
int motionLogIdx = 0;

// =====================
// HTML (interfaz combinada)
const char* htmlPage = R"rawliteral(
<!-- misma HTML que antes, sin cambios -->
)rawliteral";

// =====================
// UTIL: enviar EMAIL
void sendEmail(const char* subject, const char* message) {
  SMTP_Message mail;
  mail.sender.name = "ESP32 Watcher";
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
  } else Serial.println(String("Correo enviado: ") + subject);

  smtp.closeSession();
}

// =====================
// TASKS + UTIL JSON
const char* TASKS_PATH = "/tasks.json";
const size_t TASKS_DOC_SIZE = 10 * 1024; // buffer para ESP32 más grande

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

// =====================
// HTTP Handlers
void handleRoot() { server.send(200, "text/html", htmlPage); }
void handleInfo() {
  DynamicJsonDocument doc(512);
  doc["ssid"] = ssid;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["chip_id"] = ESP.getEfuseMac();
  doc["flash_size"] = ESP.getFlashChipSize();
  doc["flash_used"] = ESP.getSketchSize();
  doc["flash_free"] = ESP.getFreeSketchSpace();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sdk_version"] = ESP.getSdkVersion();
  doc["uptime_sec"] = millis() / 1000;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}
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
// (handlers de tasks: handleGetTasks, handleAddTask, handleRemoveTask, handleToggleTask) -> iguales al código anterior

// =====================
// LOGICA DE PING + PIR
void logMotion(const String &s) {
  motionLog[motionLogIdx++] = s;
  if (motionLogIdx >= MOTION_LOG_MAX) motionLogIdx = 0;
}

void checkPings() {
  Host &h = hosts[currentHost];
  bool pingResult = Ping.ping(h.ip, 3);

  if (pingResult) { h.okCount++; h.failCount=0; }
  else { h.failCount++; h.okCount=0; }

  if (h.failCount >= 3 && h.isUp) {
    h.isUp=false;
    String subject="❌ "+String(h.name)+" DOWN";
    String message=String(h.name)+" (" ) + h.ip + ") DOWN.";
    sendEmail(subject.c_str(), message.c_str());
    logMotion(message);
  }

  if (h.okCount>=2 && !h.isUp) {
    h.isUp=true;
    String subject="✅ "+String(h.name)+" UP";
    String message=String(h.name)+" (" ) + h.ip + ") UP.";
    sendEmail(subject.c_str(), message.c_str());
    logMotion(message);
  }

  currentHost++;
  if (currentHost>=numHosts) currentHost=0;
}

void checkPIR() {
  bool motion = digitalRead(pirPin) == HIGH;
  if (motion && millis()-lastMotionEmail >= motionDelay) {
    String s = "Movimiento detectado - "+getDateTime();
    sendEmail("🚨 Movimiento detectado", s.c_str());
    logMotion(s);
    lastMotionEmail = millis();
  }
}

// =====================
// TASKS FREERTOS
void pingTask(void* pvParameters){
  for(;;){
    checkPings();
    vTaskDelay(5000/portTICK_PERIOD_MS);
  }
}
void pirTask(void* pvParameters){
  for(;;){
    checkPIR();
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

// =====================
// SETUP + LOOP
void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, HIGH);

  if(!LittleFS.begin()){ Serial.println("LittleFS no montado!"); }

  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi...");
  unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED){ delay(300); Serial.print("."); if(millis()-start>20000) break; }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED) Serial.println("WiFi conectado: "+WiFi.localIP().toString());

  configTime(+2*3600,0,"pool.ntp.org","time.nist.gov");

  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/hosts", handleHosts);
  server.on("/motion", handleMotion);
  server.on("/tasks", handleGetTasks);
  server.on("/add", handleAddTask);
  server.on("/remove", handleRemoveTask);
  server.on("/toggle", handleToggleTask);
  server.begin();

  if(WiFi.status()==WL_CONNECTED && firstBoot){
    sendEmail("🔔 ESP32 Encendido","ESP32 Network+Motion Watcher iniciado.");
    firstBoot=false;
  }

  // Crear tasks FreeRTOS
  xTaskCreatePinnedToCore(pingTask,"PingTask",4096,NULL,1,NULL,0);
  xTaskCreatePinnedToCore(pirTask,"PIRTask",2048,NULL,1,NULL,1);
}

void loop() {
  server.handleClient(); // Loop principal solo maneja webserver
}
