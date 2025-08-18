#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiServer.h>
#include <FS.h> // SPIFFS

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

ESP8266WebServer server(80);                                          // puerto servidor web
WiFiServer fakeSSH(2222);                                             // Puerto para simular servidor SSH

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  
  Serial.println("Conectado a la red WiFi");

  if (!SPIFFS.begin()) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  // Cambiar la dirección MAC a un OUI de DELL
  uint8_t newMAC[6] = {0x00, 0x1B, 0x21, 0x01, 0x02, 0x03};          // Ejemplo de dirección MAC
  WiFi.softAPmacAddress(newMAC);                                     // Cambia la MAC para el modo AP
  WiFi.macAddress(newMAC);                                           // Cambia la MAC para el modo estación

  Serial.print("Nueva dirección MAC: ");
  Serial.println(WiFi.macAddress());

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/trap", handleTrap);
  server.begin();
  
  fakeSSH.begin(); // Inicia el servidor fake de SSH
}

void logConnection(const String& message) {
  File logFile = SPIFFS.open("/log.txt", "a");
  if (!logFile) {
    Serial.println("Error al abrir el archivo de log");
    return;
  }
  logFile.println(message);
  logFile.close();
}

void handleRoot() {
  String clientIP = server.client().remoteIP().toString();
  String clientMAC = WiFi.BSSIDstr();
  logConnection("Acceso raíz desde IP: " + clientIP + ", MAC: " + clientMAC);
  server.send(200, "text/html", "<h1>Bienvenido a la Honeypot</h1>");
}

void handleLogin() {
  String clientIP = server.client().remoteIP().toString();
  String clientMAC = WiFi.BSSIDstr();
  logConnection("Intento acceso login desde IP: " + clientIP + ", MAC: " + clientMAC);
  server.send(200, "text/html", "<h1>Login</h1><form>...</form>");
}

void handleTrap() {
  String clientIP = server.client().remoteIP().toString();
  String clientMAC = WiFi.BSSIDstr();
  logConnection("Acceso trampa desde IP: " + clientIP + ", MAC: " + clientMAC);
  server.send(200, "text/html", "<h1>Ruta de trampa</h1>");
}

void checkFakeSSH() {
  WiFiClient client = fakeSSH.available();
  if (client) {
    String clientIP = client.remoteIP().toString();
    logConnection("Conexión al fake SSH desde IP: " + clientIP);
    client.println("SSH-2.0-OpenSSH_7.6p1");                           // Banner fake
    delay(50);                                                         // Tiempo para que el banner se envíe
    client.stop();                                                     // Cierra conexión
  }
}

void loop() {
  server.handleClient();
  checkFakeSSH();                                                       // Chequea conexiones al puerto 2222
}
