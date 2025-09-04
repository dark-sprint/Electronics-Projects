#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <ESP_Mail_Client.h>

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
#define AUTHOR_PASSWORD "TU_APP_PASSWORD" // App Password de Gmail
#define RECIPIENT_EMAIL "DESTINO@gmail.com"

// =====================
// ARRAY DE HOSTS
// =====================
struct Host {
  const char* ip;
  const char* name;
  bool isUp;
};

// Agrega aquí tus hosts
Host hosts[] = {
  {"192.168.1.1", "Router", true},
  {"192.168.1.100", "Servidor NAS", true}
};

const int numHosts = sizeof(hosts)/sizeof(hosts[0]);

// =====================
// OBJETO SMTP
// =====================
SMTPSession smtp;

// =====================
// FUNCIONES
// =====================
void sendEmail(const char* subject, const char* message) {
  SMTP_Message mail;
  mail.sender.name = "ESP8266 NetworkWatcher";
  mail.sender.email = AUTHOR_EMAIL;
  mail.subject = subject;
  mail.addRecipient("Admin", RECIPIENT_EMAIL);
  mail.text.content = message;

  smtp.debug(0);
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
    Serial.println("Correo enviado correctamente.");
  }

  smtp.closeSession();
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);

  // cambio MAC para que parezca un servidor Acer
  // MAC tipo Acer: 00:1B:2F + 3 bytes aleatorios
  uint8_t mac[6] = {0x00, 0x1B, 0x2F, 0xAA, 0xBB, 0xCC}; 
  WiFi.macAddress(mac);
  
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

// =====================
// LOOP PRINCIPAL
// =====================
void loop() {
  for (int i = 0; i < numHosts; i++) {
    bool pingResult = Ping.ping(hosts[i].ip, 1); // 1 intento
    if (pingResult != hosts[i].isUp) {
      hosts[i].isUp = pingResult;

      char subject[64];
      char message[128];

      if (pingResult) {
        snprintf(subject, sizeof(subject), "✅ %s UP", hosts[i].name);
        snprintf(message, sizeof(message), "%s (%s) está nuevamente UP.", hosts[i].name, hosts[i].ip);
        Serial.println(message);
      } else {
        snprintf(subject, sizeof(subject), "❌ %s DOWN", hosts[i].name);
        snprintf(message, sizeof(message), "%s (%s) está DOWN.", hosts[i].name, hosts[i].ip);
        Serial.println(message);
      }

      // Enviar correo
      sendEmail(subject, message);
    }
  }

  delay(10000); // Espera 10 segundos antes del siguiente ping
}
