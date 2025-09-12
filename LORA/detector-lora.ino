#include <ESP8266WiFi.h>
#include <LoRa.h>

// Configuración de la red Wi-Fi
const char* ssid = "TU_SSID";
const char* password = "TU_PASSWORD";

// Configuración del servidor web
WiFiServer server(80);

// Variables para almacenar datos LoRa
String receivedData = "";

void setup() {
  Serial.begin(115200);
  // Inicializar LoRa
  LoRa.begin(915E6); // Cambia la frecuencia según tu región
  // Inicializar Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado a Wi-Fi");
  server.begin();
}

void loop() {
  // Escuchar por datos LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }
    Serial.println("Datos recibidos: " + receivedData);
  }

  // Manejar solicitudes del servidor web
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          // Enviar respuesta HTML
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<h1>Dispositivos LoRa Detectados</h1>");
          client.println("<p>" + receivedData + "</p>");
          client.println("</html>");
          break;
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    receivedData = ""; // Limpiar datos después de enviarlos
  }
}
