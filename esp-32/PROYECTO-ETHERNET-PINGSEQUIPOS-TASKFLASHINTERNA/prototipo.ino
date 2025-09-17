/*
  ESP32 + W5500 Ethernet Unified: Network + Motion Watcher + Notes Web UI
  - Pings hosts y notifica por SMTP
  - PIR detector con correo
  - Servidor web con notas en LittleFS

  ================= CONEXIONES ESP32 MINI D1 ↔ W5500 =================
  
  Módulo W5500 (USR-ES1)     ESP32 mini D1
  ------------------------------------------------
  SCS (CS/SS)  →  GPIO5   (D8)   [Chip Select]
  SCK          →  GPIO18  (D5)   [SPI Clock]
  MISO         →  GPIO19  (D6)   [SPI MISO]
  MOSI         →  GPIO23  (D7)   [SPI MOSI]
  3.3V         →  3V3            [Alimentación 3.3V]
  GND          →  GND            [Tierra]

  ⚠️ Notas:
  - El módulo W5500 funciona a 3.3V (no usar 5V).
  - El pin CS se puede cambiar en el código con Ethernet.init(CS_PIN).
  - Todos los pines SPI están en el bus SPI por defecto del ESP32.

  =====================================================================
*/

/*
  ESP32 + W5500 Ethernet Unified: Network + Motion Watcher + Notes Web UI
  - Pings hosts y notifica por SMTP
  - PIR detector con correo
  - Servidor web con notas en LittleFS
*/

#include <SPI.h>
#include <Ethernet.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <ESP32Ping.h>
#include <time.h>

// ===== CONFIG ETHERNET =====
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };  
IPAddress ip(192, 168, 1, 200); // IP fija (ajústala según tu red)

// ===== CONFIG SMTP =====
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD"
#define RECIPIENT_EMAIL "DESTINO@gmail.com"

// ===== ARRAY DE HOSTS =====
struct Host {
  const char* ip;
  const char* name;
  bool isUp;
};

Host hosts[] = {
  {"192.168.1.1", "ROUTER", false},
  {"192.168.1.75", "PROXMOX", false},
  {"192.168.1.129", "TV-SALÓN", false},
  {"192.168.1.130", "PC-MILITAR-WIFI", false}
};
const int numHosts = sizeof(hosts) / sizeof(hosts[0]);

// ===== PIR =====
const int pirPin = 4; 
unsigned long lastMotionEmail = 0;
const unsigned long motionDelay = 10000;

// ===== SERVIDOR Y MAIL =====
SMTPSession smtp;
WebServer server(80);

unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000;
int currentHost = 0;
bool firstBoot = true;

// ================= HTML UI =================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head><meta charset="UTF-8"><title>ESP32 Ethernet Notas</title></head>
<body><h3>Servidor Notas vía Ethernet</h3></body>
</html>
)rawliteral";

// ================= FUNCIONES =================
String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "SinHora";
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

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

  if (!smtp.connect(&session)) return;
  MailClient.sendMail(&smtp, &mail);
  smtp.closeSession();
}

void handleRoot() { server.send(200, "text/html", htmlPage); }

void handleInfo() {
  DynamicJsonDocument doc(512);
  doc["ip"] = Ethernet.localIP().toString();
  doc["mac"] = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) +
               ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
  doc["uptime_sec"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  // Inicializar FS
  LittleFS.begin();

  // Inicializar Ethernet
  Ethernet.init(5);  // CS en GPIO5
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP fallo, usando IP estática...");
    Ethernet.begin(mac, ip);
  }
  delay(1000);
  Serial.print("IP asignada: ");
  Serial.println(Ethernet.localIP());

  // Tiempo NTP (W5500 no tiene RTC, pero sí funciona con configTime y DNS)
  configTime(0, 0, "pool.ntp.org");

  // Servidor
  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  if (firstBoot) {
    sendEmail("🔔 ESP32 Ethernet Encendido", "El ESP32 Ethernet Watcher se ha iniciado.");
    firstBoot = false;
  }

  // PING ROTATIVO
  if (currentMillis - lastPingTime >= pingInterval) {
    lastPingTime = currentMillis;
    Host &h = hosts[currentHost];
    bool pingResult = Ping.ping(h.ip, 3);

    static int failCount[20] = {0};
    static int okCount[20] = {0};

    if (pingResult) {
      okCount[currentHost]++;
      failCount[currentHost] = 0;
    } else {
      failCount[currentHost]++;
      okCount[currentHost] = 0;
    }

    if (failCount[currentHost] >= 3 && h.isUp) {
      h.isUp = false;
      sendEmail(("❌ " + String(h.name) + " DOWN").c_str(), (String(h.name) + " está DOWN.").c_str());
    }
    if (okCount[currentHost] >= 2 && !h.isUp) {
      h.isUp = true;
      sendEmail(("✅ " + String(h.name) + " UP").c_str(), (String(h.name) + " está UP.").c_str());
    }

    currentHost++;
    if (currentHost >= numHosts) currentHost = 0;
  }
}
