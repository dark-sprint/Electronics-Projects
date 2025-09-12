#include <WiFi.h>               // ESP32
#include <ESP_Mail_Client.h>
#include <ESP32Ping.h>          // Para ESP32, en lugar de ESP8266Ping.h

bool firstBoot = true;

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

// ===== ARRAY DE HOSTS =====
struct Host {
  const char* ip;
  const char* name;
  bool isUp;
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
const int pirPin = 2;  // D2 en ESP8266 → GPIO2 en ESP32
unsigned long lastMotionEmail = 0;
const unsigned long motionDelay = 10000;

// ===== OBJETO SMTP =====
SMTPSession smtp;

// ===== VARIABLES =====
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 5000; // Ping cada 5 segundos
int currentHost = 0;

// ===== FUNCIONES =====
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

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

// ===== LOOP PRINCIPAL =====
void loop() {
  unsigned long currentMillis = millis();

  // --- PRIMER MENSAJE AL ENCENDER ---
  if (firstBoot) {
    sendEmail("🔔 ESP32 Encendido", "El ESP32 Network + Motion Watcher se ha iniciado.");
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

  // --- DETECCIÓN DE MOVIMIENTO ---
  /*
  bool motion = digitalRead(pirPin) == HIGH;
  if (motion && currentMillis - lastMotionEmail >= motionDelay) {
    Serial.println("¡Movimiento detectado!");
    sendEmail("🚨 Movimiento detectado", "Se ha detectado movimiento en la zona monitoreada.");
    lastMotionEmail = currentMillis;
  }
  */
}
