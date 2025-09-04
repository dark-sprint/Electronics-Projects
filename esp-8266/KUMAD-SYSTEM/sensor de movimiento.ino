# Pin HC-SR501 mini	ESP8266 (NodeMCU) 
# VCC	    3.3V
# GND     GND
# OUT     D2 (GPIO4)
#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>

// ===== CONFIGURACIÓN RED =====
const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";

// ===== CONFIGURACIÓN CORREO =====
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587

#define AUTHOR_EMAIL "TU_CORREO@gmail.com"
#define AUTHOR_PASSWORD "TU_APP_PASSWORD" // App Password de Gmail
#define RECIPIENT_EMAIL "DESTINO@gmail.com"

// ===== PIN DEL PIR =====
const int pirPin = D2; // Conectado al OUT del HC-SR501 mini
bool lastState = LOW;  // Estado anterior del sensor

// ===== OBJETO SMTP =====
SMTPSession smtp;

// ===== FUNCIONES =====
void sendEmail(const char* subject, const char* message) {
  SMTP_Message mail;
  mail.sender.name = "ESP8266 MotionWatcher";
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

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  // MAC tipo Acer (opcional)
  uint8_t mac[6] = {0x00, 0x1B, 0x2F, 0xAA, 0xBB, 0xCC}; 
  WiFi.macAddress(mac);

  pinMode(pirPin, INPUT);

  // Conexión Wi-Fi
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
  bool currentState = digitalRead(pirPin);

  // Detecta cambio de estado
  if (currentState == HIGH && lastState == LOW) {
    Serial.println("¡Movimiento detectado!");
    sendEmail("🚨 Movimiento detectado", "Se ha detectado movimiento en la zona monitoreada.");
  }

  lastState = currentState;
  delay(500); // Pequeña pausa para evitar rebotes
}
